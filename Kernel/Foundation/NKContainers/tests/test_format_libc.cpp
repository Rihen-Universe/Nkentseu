// =============================================================================
// NKContainers/tests/test_format_libc.cpp
// BANC DIFFERENTIEL : NkSnprintf (sans libc) contre snprintf (libc), octet
// pour octet. La reference existe sur cette machine : c'est elle le temoin.
//
// CE FICHIER EST LE SEUL DE LA FAMILLE NkFormat A INCLURE <cstdio> : il est la
// reference, pas le sujet.
//
// Portee mesuree (imprimee en fin de banc) :
//  - entiers : tous les entiers de -70 000 a 70 000, plus 100 000 entiers 64 bits
//    aleatoires, dans les six conversions d i u x X o ; chaque valeur passe par
//    une specification complete (drapeaux x largeur x precision) tiree en
//    rotation dans une matrice de 1 800 specifications, et vingt valeurs
//    sentinelles passent par la matrice ENTIERE ;
//  - flottants : 1 000 000 de doubles (motifs de bits aleatoires sur toute la
//    plage + liste de cas limites) a chaque precision 0..17 pour %f, %e, %g,
//    soit 54 conversions par double ; les cas limites passent aussi par une
//    matrice de drapeaux/largeurs ;
//  - %a separement (meme exigence : zero difference) ;
//  - la semantique snprintf : retour = longueur qui aurait ete ecrite,
//    troncature terminee, tampon nul, %n, %*, %.*, hh h l ll j z t ;
//  - les egalites d'arrondi (temoins de la mutation « tronquer au lieu
//    d'arrondir ») : 0,125 -> 0.12 (pair), 0,375 -> 0.38 (pair), 0,5 -> 0,
//    1,5 -> 2, 2,5 -> 2.
// Zero difference = vert ; toute difference est imprimee avec la valeur en hex.
//
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// DATE : 2026-09-04
// LICENCE : Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKCore/Text/NkSnprintf.h"
#include "NKContainers/String/NkFormat.h"

#include <cstdio> // LA REFERENCE (seule inclusion autorisee dans cette famille)

using namespace nkentseu;

#ifndef NK_FORMAT_BENCH_DOUBLES
#define NK_FORMAT_BENCH_DOUBLES 1000000u
#endif
#ifndef NK_FORMAT_BENCH_RANDOM_INTS
#define NK_FORMAT_BENCH_RANDOM_INTS 100000u
#endif

namespace {

	// ── Generateur deterministe (splitmix64, Steele/Lea/Flood 2014) ──────────
	struct Rng {
			uint64 state;
			explicit Rng(uint64 seed) : state(seed) {}
			uint64 Next() {
				uint64 z = (state += 0x9E3779B97F4A7C15ull);
				z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
				z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
				return z ^ (z >> 31);
			}
	};

	double FromBits(uint64 u) {
		double d;
		unsigned char *dst = reinterpret_cast<unsigned char *>(&d);
		const unsigned char *src = reinterpret_cast<const unsigned char *>(&u);
		for (unsigned i = 0u; i < 8u; ++i)
			dst[i] = src[i];
		return d;
	}
	uint64 ToBits(double d) {
		uint64 u;
		unsigned char *dst = reinterpret_cast<unsigned char *>(&u);
		const unsigned char *src = reinterpret_cast<const unsigned char *>(&d);
		for (unsigned i = 0u; i < 8u; ++i)
			dst[i] = src[i];
		return u;
	}

	bool SameBytes(const char *a, const char *b) {
		usize i = 0u;
		for (;; ++i) {
			if (a[i] != b[i])
				return false;
			if (a[i] == '\0')
				return true;
		}
	}

	// ── Compteur de differences, avec impression bornee ───────────────────────
	struct Diff {
			uint64 compared = 0u;
			uint64 differences = 0u;
			uint32 printed = 0u;
			static constexpr uint32 kMaxPrinted = 40u;

			void Report(const char *fmt, uint64 bits, const char *ref, const char *got, int refRet, int gotRet) {
				++differences;
				if (printed < kMaxPrinted) {
					++printed;
					std::printf("  DIFF fmt=\"%s\" valeur=0x%016llx libc=[%s](%d) nk=[%s](%d)\n", fmt,
								static_cast<unsigned long long>(bits), ref, refRet, got, gotRet);
				}
			}
	};

	// Tampons larges : %f de DBL_MAX a precision 17 fait 327 caracteres ; avec
	// une largeur de 40 on reste tres en dessous de 2048.
	constexpr usize kBuf = 2048u;

	void CmpSigned(Diff &d, const char *fmt, long long v) {
		char ref[kBuf], got[kBuf];
		const int rr = std::snprintf(ref, sizeof(ref), fmt, v);
		const int gr = NkSnprintf(got, sizeof(got), fmt, v);
		++d.compared;
		if (rr != gr || !SameBytes(ref, got))
			d.Report(fmt, static_cast<uint64>(v), ref, got, rr, gr);
	}

	void CmpUnsigned(Diff &d, const char *fmt, unsigned long long v) {
		char ref[kBuf], got[kBuf];
		const int rr = std::snprintf(ref, sizeof(ref), fmt, v);
		const int gr = NkSnprintf(got, sizeof(got), fmt, v);
		++d.compared;
		if (rr != gr || !SameBytes(ref, got))
			d.Report(fmt, static_cast<uint64>(v), ref, got, rr, gr);
	}

	void CmpDouble(Diff &d, const char *fmt, double v) {
		char ref[kBuf], got[kBuf];
		const int rr = std::snprintf(ref, sizeof(ref), fmt, v);
		const int gr = NkSnprintf(got, sizeof(got), fmt, v);
		++d.compared;
		if (rr != gr || !SameBytes(ref, got))
			d.Report(fmt, ToBits(v), ref, got, rr, gr);
	}

	// ── Construction d'une specification « %[flags][width][.prec][len]conv » ──
	usize BuildSpec(char *out, const char *flags, int width, int prec, const char *len, char conv) {
		usize n = 0u;
		out[n++] = '%';
		for (const char *f = flags; *f; ++f)
			out[n++] = *f;
		if (width >= 0)
			n += NkSnprintf(out + n, 16u, "%d", width);
		if (prec >= 0) {
			out[n++] = '.';
			n += NkSnprintf(out + n, 16u, "%d", prec);
		}
		for (const char *l = len; *l; ++l)
			out[n++] = *l;
		out[n++] = conv;
		out[n] = '\0';
		return n;
	}

	const char *const kIntFlags[] = {"", "-", "+", " ", "#", "0", "-0", "+0", "#0", "- ", "+#", " #0"};
	constexpr unsigned kIntFlagCount = 12u;
	const int kIntWidths[] = {-1, 1, 5, 12, 25};
	constexpr unsigned kIntWidthCount = 5u;
	const int kIntPrecs[] = {-1, 0, 3, 8, 20};
	constexpr unsigned kIntPrecCount = 5u;
	const char kIntConvs[] = {'d', 'i', 'u', 'x', 'X', 'o'};
	constexpr unsigned kIntConvCount = 6u;
	constexpr unsigned kIntSpecCount = kIntFlagCount * kIntWidthCount * kIntPrecCount * kIntConvCount; // 1 800

	void IntSpecAt(unsigned idx, char *spec) {
		const unsigned conv = idx % kIntConvCount;
		idx /= kIntConvCount;
		const unsigned prec = idx % kIntPrecCount;
		idx /= kIntPrecCount;
		const unsigned width = idx % kIntWidthCount;
		idx /= kIntWidthCount;
		const unsigned flags = idx % kIntFlagCount;
		BuildSpec(spec, kIntFlags[flags], kIntWidths[width], kIntPrecs[prec], "ll", kIntConvs[conv]);
	}

	void CmpIntAllForms(Diff &d, const char *spec, long long v) {
		// la conversion decide du type passe : signee pour d/i, non signee sinon
		usize n = 0u;
		while (spec[n])
			++n;
		const char conv = spec[n - 1u];
		if (conv == 'd' || conv == 'i')
			CmpSigned(d, spec, v);
		else
			CmpUnsigned(d, spec, static_cast<unsigned long long>(v));
	}

	// ── Cas limites flottants (motifs de bits, pour ne dependre d'aucun en-tete) ─
	const uint64 kEdgeBits[] = {
		0x0000000000000000ull, // 0
		0x8000000000000000ull, // -0
		0x0000000000000001ull, // 5e-324, plus petit denormalise
		0x000FFFFFFFFFFFFFull, // plus grand denormalise
		0x0010000000000000ull, // DBL_MIN
		0x7FEFFFFFFFFFFFFFull, // DBL_MAX
		0xFFEFFFFFFFFFFFFFull, // -DBL_MAX
		0x7FF0000000000000ull, // +inf
		0xFFF0000000000000ull, // -inf
		0x7FF8000000000000ull, // nan
		0xFFF8000000000000ull, // -nan
		0x7FF0000000000001ull, // nan signalant
		0x7FF800000000ABCDull, // nan avec charge
		0x3FB999999999999Aull, // 0.1
		0x3FD5555555555555ull, // 1/3
		0x3FE5555555555555ull, // 2/3
		0x430C6BF526340000ull, // 1e15
		0x4341C37937E08000ull, // 1e16
		0x43763457785D8A00ull, // 1e17
		0x3FE0000000000000ull, // 0.5
		0x3FF8000000000000ull, // 1.5
		0x4004000000000000ull, // 2.5
		0x3FC0000000000000ull, // 0.125
		0x3FD8000000000000ull, // 0.375
		0x3FA999999999999Aull, // 0.05
		0x3FC3333333333333ull, // 0.15
		0x3FD0000000000000ull, // 0.25
		0x3FD6666666666666ull, // 0.35
		0x3FEFD70A3D70A3D7ull, // 0.995
		0x4023E66666666666ull, // 9.95
		0x4023EB851EB851ECull, // 9.96
		0x4023FFFFBCBE61EAull, // 9.999999
		0x4023FFFFFBCBE61Full, // 9.9999995
		0x7E37E43C8800759Cull, // 1e300
		0x01A56E1FC2F8F359ull, // 1e-300
		0x419D6F3454000000ull, // 123456789
		0x444B1AE4D6E2EF50ull, // 1e21
		0x447F0FE5D2A9CB00ull, // 1e22 (derniere puissance de dix exacte)
		0x44B52D02C7E14AF6ull, // 1e23
		0x3F50624DD2F1A9FCull, // 0.001
		0x3EE4F8B588E368F1ull, // 1e-5
		0x3F1A36E2EB1C432Dull, // 1e-4
		0x40F86A0000000000ull, // 100000
		0x412E848000000000ull, // 1000000
		0x405EDD2F1A9FBE77ull, // 123.456
		0xC05EDD2F1A9FBE77ull, // -123.456
		0x3FF0000000000001ull, // 1 + epsilon
		0x3FEFFFFFFFFFFFFFull, // 1 - epsilon/2
		0x4011666666666666ull, // 4.35
		0x4005666666666666ull, // 2.675
		0x7FE1CCF385EBC8A0ull, // 1e308
		0x0004000000000000ull, // denormalise « rond »
		0x000FFFFFFFFFFFFEull, // denormalise pair
		0x4340000000000000ull, // 2^53
		0x4340000000000001ull, // 2^53 + 2
		0x43E0000000000000ull, // 2^63
		0x43F0000000000000ull, // 2^64
		0xBFF0000000000000ull, // -1
		0x3FF0000000000000ull, // 1
	};
	constexpr unsigned kEdgeCount = sizeof(kEdgeBits) / sizeof(kEdgeBits[0]);

	const char *const kFltFlags[] = {"", "-", "+", " ", "#", "0", "-0", "+0", "#0", "+#0", "- #"};
	constexpr unsigned kFltFlagCount = 11u;
	const int kFltWidths[] = {-1, 1, 8, 24, 40};
	constexpr unsigned kFltWidthCount = 5u;

} // namespace

// =============================================================================
// ENTIERS
// =============================================================================

TEST_CASE(NkFormatLibc, IntegersAgainstLibc) {
	Diff d;
	char spec[32];

	// 1) tous les entiers de -70 000 a 70 000 : conversion nue + une specification
	//    complete en rotation (les 1 800 sont toutes visitees ~78 fois)
	unsigned rot = 0u;
	for (long long v = -70000; v <= 70000; ++v) {
		CmpSigned(d, "%lld", v);
		CmpSigned(d, "%lli", v);
		CmpUnsigned(d, "%llu", static_cast<unsigned long long>(v));
		CmpUnsigned(d, "%llx", static_cast<unsigned long long>(v));
		CmpUnsigned(d, "%llX", static_cast<unsigned long long>(v));
		CmpUnsigned(d, "%llo", static_cast<unsigned long long>(v));
		IntSpecAt(rot, spec);
		CmpIntAllForms(d, spec, v);
		rot = (rot + 7919u) % kIntSpecCount;
	}

	// 2) 100 000 entiers 64 bits aleatoires
	Rng rng(0x5EEDA11CE5ull);
	for (unsigned i = 0u; i < NK_FORMAT_BENCH_RANDOM_INTS; ++i) {
		const long long v = static_cast<long long>(rng.Next());
		CmpSigned(d, "%lld", v);
		CmpUnsigned(d, "%llu", static_cast<unsigned long long>(v));
		CmpUnsigned(d, "%llx", static_cast<unsigned long long>(v));
		CmpUnsigned(d, "%llo", static_cast<unsigned long long>(v));
		IntSpecAt(rot, spec);
		CmpIntAllForms(d, spec, v);
		rot = (rot + 7919u) % kIntSpecCount;
	}

	// 3) vingt sentinelles x la matrice entiere
	const long long sentinels[] = {0, 1, -1, 7, -7, 255, -255, 256, 65535, -65536, 2147483647LL, -2147483647LL - 1,
								   4294967295LL, 9223372036854775807LL, (-9223372036854775807LL - 1), 1000000000000000000LL,
								   123456789LL, -987654321LL, 4096LL, 1LL << 40};
	for (unsigned s = 0u; s < 20u; ++s)
		for (unsigned i = 0u; i < kIntSpecCount; ++i) {
			IntSpecAt(i, spec);
			CmpIntAllForms(d, spec, sentinels[s]);
		}

	// 4) modificateurs de longueur : la troncature de l'argument est celle de la libc
	{
		char ref[kBuf], got[kBuf];
		int rr, gr;
		rr = std::snprintf(ref, sizeof(ref), "%hhd %hd %d %ld %lld %zu %td %jd %hhu %hu %u %lu %llu", 300, 70000, -5, -6L,
						   -7LL, static_cast<usize>(42), static_cast<isize>(-3), static_cast<long long>(9), 300u, 70000u, 5u,
						   6ul, 7ull);
		gr = NkSnprintf(got, sizeof(got), "%hhd %hd %d %ld %lld %zu %td %jd %hhu %hu %u %lu %llu", 300, 70000, -5, -6L, -7LL,
						static_cast<usize>(42), static_cast<isize>(-3), static_cast<long long>(9), 300u, 70000u, 5u, 6ul,
						7ull);
		++d.compared;
		if (rr != gr || !SameBytes(ref, got))
			d.Report("(longueurs)", 0u, ref, got, rr, gr);
	}

	std::printf("  [entiers] %llu comparaisons, %llu difference(s)\n", static_cast<unsigned long long>(d.compared),
				static_cast<unsigned long long>(d.differences));
	ASSERT_EQUAL(0ull, static_cast<unsigned long long>(d.differences));
}

// =============================================================================
// FLOTTANTS : %f %e %g a chaque precision 0..17
// =============================================================================

TEST_CASE(NkFormatLibc, DoublesAgainstLibc) {
	Diff d;
	char specs[18][3][8]; // [prec][f/e/g]
	for (int p = 0; p <= 17; ++p) {
		BuildSpec(specs[p][0], "", -1, p, "", 'f');
		BuildSpec(specs[p][1], "", -1, p, "", 'e');
		BuildSpec(specs[p][2], "", -1, p, "", 'g');
	}

	auto cmpAll = [&](double v) {
		for (int p = 0; p <= 17; ++p) {
			CmpDouble(d, specs[p][0], v);
			CmpDouble(d, specs[p][1], v);
			CmpDouble(d, specs[p][2], v);
		}
	};

	// 1) cas limites, a chaque precision
	for (unsigned i = 0u; i < kEdgeCount; ++i)
		cmpAll(FromBits(kEdgeBits[i]));

	// 2) aleatoires : 70 % motifs de bits sur toute la plage (exposants extremes,
	//    denormalises, inf, nan compris), 30 % dans la plage « humaine » 2^-60..2^60
	Rng rng(0xF10A7ull);
	const unsigned total = NK_FORMAT_BENCH_DOUBLES;
	for (unsigned i = 0u; i < total; ++i) {
		uint64 bits = rng.Next();
		if (i % 10u >= 7u) {
			const uint64 mant = bits & ((1ull << 52) - 1u);
			const uint64 e = 1023u - 60u + (bits >> 52) % 121u;
			bits = (bits & (1ull << 63)) | (e << 52) | mant;
		}
		cmpAll(FromBits(bits));
	}

	// 3) cas limites x drapeaux x largeurs x {f,e,g,F,E,G}, precisions 0, 2, 6, 17 et « aucune »
	const int precs[] = {-1, 0, 2, 6, 17};
	const char convs[] = {'f', 'e', 'g', 'F', 'E', 'G'};
	char spec[32];
	for (unsigned v = 0u; v < kEdgeCount; ++v)
		for (unsigned fl = 0u; fl < kFltFlagCount; ++fl)
			for (unsigned w = 0u; w < kFltWidthCount; ++w)
				for (unsigned p = 0u; p < 5u; ++p)
					for (unsigned c = 0u; c < 6u; ++c) {
						BuildSpec(spec, kFltFlags[fl], kFltWidths[w], precs[p], "", convs[c]);
						CmpDouble(d, spec, FromBits(kEdgeBits[v]));
					}

	std::printf("  [doubles] %u valeurs (dont %u cas limites), %llu comparaisons, %llu difference(s)\n",
				total + kEdgeCount, kEdgeCount, static_cast<unsigned long long>(d.compared),
				static_cast<unsigned long long>(d.differences));
	ASSERT_EQUAL(0ull, static_cast<unsigned long long>(d.differences));
}

// =============================================================================
// %a (hexadecimal flottant) : meme exigence, compte separe
// =============================================================================

TEST_CASE(NkFormatLibc, HexFloatAgainstLibc) {
	Diff d;
	char spec[32];
	const int precs[] = {-1, 0, 1, 2, 5, 13, 15};
	for (unsigned v = 0u; v < kEdgeCount; ++v)
		for (unsigned p = 0u; p < 7u; ++p) {
			BuildSpec(spec, "", -1, precs[p], "", 'a');
			CmpDouble(d, spec, FromBits(kEdgeBits[v]));
			BuildSpec(spec, "#", 20, precs[p], "", 'A');
			CmpDouble(d, spec, FromBits(kEdgeBits[v]));
		}
	Rng rng(0xA11Cull);
	for (unsigned i = 0u; i < 20000u; ++i) {
		const double v = FromBits(rng.Next());
		for (unsigned p = 0u; p < 7u; ++p) {
			BuildSpec(spec, "", -1, precs[p], "", 'a');
			CmpDouble(d, spec, v);
		}
	}
	std::printf("  [%%a] %llu comparaisons, %llu difference(s)\n", static_cast<unsigned long long>(d.compared),
				static_cast<unsigned long long>(d.differences));
	ASSERT_EQUAL(0ull, static_cast<unsigned long long>(d.differences));
}

// =============================================================================
// SEMANTIQUE snprintf : retour, troncature, tampon nul, %n, *, chaines
// =============================================================================

TEST_CASE(NkFormatLibc, SnprintfSemantics) {
	char got[64];

	// retour = longueur qui aurait ete ecrite ; troncature terminee
	ASSERT_EQUAL(6, NkSnprintf(got, 4u, "%d", 123456));
	ASSERT_TRUE(SameBytes(got, "123"));
	// tampon nul / capacite nulle : longueur seule
	ASSERT_EQUAL(6, NkSnprintf(nullptr, 0u, "%d", 123456));
	got[0] = 'Z';
	ASSERT_EQUAL(6, NkSnprintf(got, 0u, "%d", 123456));
	ASSERT_TRUE(got[0] == 'Z');
	// capacite 1 : seul le terminateur
	ASSERT_EQUAL(3, NkSnprintf(got, 1u, "abc"));
	ASSERT_TRUE(got[0] == '\0');

	// chaines, caracteres, pourcentage, largeur/precision par etoile
	NkSnprintf(got, sizeof(got), "[%5s|%-5s|%.2s|%c|%%|%*d|%-*d|%.*f]", "ab", "ab", "hello", 'x', 6, 42, 4, 7, 2, 3.14159);
	ASSERT_TRUE(SameBytes(got, "[   ab|ab   |he|x|%|    42|7   |3.14]"));
	NkSnprintf(got, sizeof(got), "%s", static_cast<const char *>(nullptr));
	ASSERT_TRUE(SameBytes(got, "(null)"));

	// %n : position courante
	int pos = -1;
	NkSnprintf(got, sizeof(got), "abc%nde", &pos);
	ASSERT_EQUAL(3, pos);

	// pointeur : « 0x » + hexadecimal minimal, nullptr -> 0x0 (choix documente)
	NkSnprintf(got, sizeof(got), "%p", reinterpret_cast<const void *>(static_cast<uintptr>(0x1234)));
	ASSERT_TRUE(SameBytes(got, "0x1234"));
	NkSnprintf(got, sizeof(got), "%p", static_cast<const void *>(nullptr));
	ASSERT_TRUE(SameBytes(got, "0x0"));

	// la meme chose par va_list (NkString::VFormat en depend)
	NkString s = NkString::Format("%s=%d %.1f", "x", 7, 2.25);
	ASSERT_TRUE(SameBytes(s.Data(), "x=7 2.2"));
}

// =============================================================================
// EGALITES D'ARRONDI : temoins de la mutation « tronquer au lieu d'arrondir »
// =============================================================================

TEST_CASE(NkFormatLibc, RoundHalfEvenWitnesses) {
	char got[64];
	NkSnprintf(got, sizeof(got), "%.2f", 0.125);
	ASSERT_TRUE_MSG(SameBytes(got, "0.12"), "0,125 est une egalite exacte : vers le pair -> 0.12");
	NkSnprintf(got, sizeof(got), "%.2f", 0.375);
	ASSERT_TRUE_MSG(SameBytes(got, "0.38"), "0,375 est une egalite exacte : vers le pair -> 0.38 (la troncature donne 0.37)");
	NkSnprintf(got, sizeof(got), "%.0f %.0f %.0f", 0.5, 1.5, 2.5);
	ASSERT_TRUE(SameBytes(got, "0 2 2"));
	NkSnprintf(got, sizeof(got), "%.2f", 2.0 / 3.0);
	ASSERT_TRUE_MSG(SameBytes(got, "0.67"), "2/3 -> 0.67 (la troncature donne 0.66)");
	NkSnprintf(got, sizeof(got), "%.1f %.1f", 0.05, 0.15);
	ASSERT_TRUE_MSG(SameBytes(got, "0.1 0.1"), "0,05 binaire est au-dessus de 1/20 ; 0,15 binaire est en dessous de 3/20");
	NkSnprintf(got, sizeof(got), "%.1e %.1e", 9.95, 9.96);
	ASSERT_TRUE(SameBytes(got, "9.9e+00 1.0e+01"));
	NkSnprintf(got, sizeof(got), "%e %g %g", 5e-324, 1e-5, 1000000.0);
	ASSERT_TRUE(SameBytes(got, "4.940656e-324 1e-05 1e+06"));
}

// =============================================================================
// LES DEUX CHEMINS (template NkPrintf / NkFormat et va_list) PARTAGENT LE COEUR
// =============================================================================

TEST_CASE(NkFormatLibc, TemplatePathSharesCore) {
	char got[128];
	NkSnprintf(got, sizeof(got), "%08.3f|%-12.4e|%+g|%#x|%5d|%.3s", -3.14159, 31415.9, 2.5, 255, 42, "abcdef");
	const NkString t = NkPrintf("%08.3f|%-12.4e|%+g|%#x|%5d|%.3s", -3.14159, 31415.9, 2.5, 255, 42, "abcdef");
	ASSERT_TRUE_MSG(SameBytes(got, t.Data()), t.Data());

	const NkString b = NkFormat("{0:.3f} {1:e} {2:x} {3:,} {4:.17g}", 2.0 / 3.0, 1e300, 48879, 1234567, 0.1);
	ASSERT_TRUE_MSG(SameBytes(b.Data(), "0.667 1.000000e+300 beef 1,234,567 0.10000000000000001"), b.Data());
}
