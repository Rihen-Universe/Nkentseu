// =============================================================================
// NKCore/Text/NkNumberToText.h
// Conversion nombre -> texte SANS libc : entiers (bases 2/8/10/16) et flottants
// (%f, %e, %g, %a) avec la semantique exacte de printf (drapeaux, largeur,
// precision, arrondi au plus proche / egalite vers le pair).
//
// Pourquoi ce fichier existe :
//  - NkFormat.h (NKContainers) offrait une facade « remplace snprintf » dont le
//    coeur deleguait a snprintf de la libc. La plateforme Bare (console maison,
//    sans OS) n'aura pas de libc : le coeur doit etre a nous.
//  - Il est pose dans NKCore, pas a cote de NkFormat.h, parce que 7 des 81 sites
//    qui appellent snprintf vivent sous NKContainers (NKCore, NKMemory,
//    NKPlatform) : le coeur ne depend que de NkTypes.h et doit etre visible
//    depuis la couche la plus basse possible (NKPlatform reste en dessous).
//
// Algorithme flottant : CONVERSION EXACTE PAR GRANDS ENTIERS.
//  Un double fini vaut exactement m * 2^e (m < 2^53, e dans [-1074, 971]).
//   - e >= 0 : la valeur est l'entier m << e (au plus 1024 bits), converti en
//     decimal par divisions repetees par 10^9 (Steele & White 1990, « Dragon4 »
//     dans sa forme la plus simple, sans recherche du plus court chiffre).
//   - e <  0 : partie entiere = m >> k, partie fractionnaire = (m mod 2^k) / 2^k
//     avec k = -e ; chaque chiffre decimal s'obtient par F = F*10, chiffre =
//     bits de F au-dessus de 2^k, F = F mod 2^k. Le reste se compare EXACTEMENT
//     a 1/2 (bit k-1 puis bits inferieurs) : l'arrondi au plus proche avec
//     egalite vers le pair est donc exact, quelle que soit la precision.
//  Choisi plutot que Ryu (tables de ~10 Ko, et sa variante a precision fixe
//  est un second algorithme) ou Grisu (peut echouer sur ~0,5 % des valeurs et
//  exige un repli exact — c'est-a-dire ceci). La simplicite prouvable prime
//  sur la vitesse : le banc differentiel contre la libc (NKContainers/tests/
//  test_format_libc.cpp) est le temoin.
//
// Zero STL, zero libc : seul NkTypes.h est inclus. Les bits d'un double sont
// lus par copie d'octets (aliasing par unsigned char, defini par le standard).
//
// Non couvert (nomme, pas fait) : long double 80 bits (converti en double),
// positions d'argument « %1$d », caracteres larges.
//
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// DATE : 2026-09-04
// LICENCE : Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================

#pragma once

#ifndef NK_CORE_TEXT_NKNUMBERTOTEXT_H_INCLUDED
#define NK_CORE_TEXT_NKNUMBERTOTEXT_H_INCLUDED

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace numtext {

		// =====================================================================
		// CONSTANTES
		// =====================================================================

		/// Chiffres decimaux maximum de la partie entiere d'un double (2^1024 ~ 1,8e308).
		constexpr uint32 kMaxIntDigits = 310u;
		/// Precision maximale GENEREE. Au-dela, les chiffres sont des zeros exacts
		/// (un double a au plus 1074 chiffres fractionnaires non nuls) : on les
		/// ajoute sans les calculer.
		constexpr uint32 kMaxPrecision = 1100u;
		/// Taille d'un tampon pouvant recevoir n'importe quel corps de flottant.
		constexpr uint32 kBodyMax = kMaxIntDigits + kMaxPrecision + 16u;

		// =====================================================================
		// PUITS D'ECRITURE (semantique snprintf : compte tout, ecrit jusqu'a cap)
		// =====================================================================

		struct NkCharSink {
				char *buf;
				usize cap;
				usize len;

				NkCharSink(char *b, usize c) : buf(b), cap(b ? c : 0u), len(0u) {}

				void Put(char c) {
					if (len < cap)
						buf[len] = c;
					++len;
				}
				void Put(const char *s, usize n) {
					for (usize i = 0u; i < n; ++i)
						Put(s[i]);
				}
				void Fill(char c, usize n) {
					for (usize i = 0u; i < n; ++i)
						Put(c);
				}
				/// Termine toujours par '\0' quand il y a un tampon (meme tronque).
				void Terminate() {
					if (cap > 0u)
						buf[len < cap ? len : cap - 1u] = '\0';
				}
		};

		// =====================================================================
		// SPECIFICATION D'UNE CONVERSION (drapeaux printf)
		// =====================================================================

		struct NkNumSpec {
				char conv = 'd';	   ///< d i u x X o b B f F e E g G a A c s p
				bool left = false;	   ///< '-'
				bool plus = false;	   ///< '+'
				bool space = false;	   ///< ' '
				bool alt = false;	   ///< '#'
				bool zero = false;	   ///< '0'
				int width = -1;		   ///< -1 = non specifiee
				int precision = -1;	   ///< -1 = non specifiee
		};

		// =====================================================================
		// OUTILS
		// =====================================================================

		inline usize NkCStrLen(const char *s) {
			if (!s)
				return 0u;
			usize n = 0u;
			while (s[n])
				++n;
			return n;
		}

		/// Ecrit les chiffres de v en base `base` (2..16), poids fort d'abord,
		/// sans terminateur. `out` doit avoir au moins 64 octets. v == 0 -> "0".
		inline usize NkUIntToDigits(uint64 v, unsigned base, bool upper, char *out) {
			const char *digs = upper ? "0123456789ABCDEF" : "0123456789abcdef";
			char tmp[64];
			usize n = 0u;
			do {
				tmp[n++] = digs[v % base];
				v /= base;
			} while (v != 0u);
			for (usize i = 0u; i < n; ++i)
				out[i] = tmp[n - 1u - i];
			return n;
		}

		// =====================================================================
		// ENTIERS
		// =====================================================================

		/// Emet un entier avec la semantique printf complete.
		/// @param negative Vrai si la valeur signee est negative (mag = |valeur|)
		/// @param mag      Magnitude (ou motif de bits pour u/x/o)
		template <typename Sink> void NkEmitInteger(Sink &out, const NkNumSpec &sp, bool negative, uint64 mag) {
			unsigned base = 10u;
			bool upper = false;
			bool isSigned = false;
			switch (sp.conv) {
				case 'd':
				case 'i':
					isSigned = true;
					break;
				case 'u':
					break;
				case 'x':
					base = 16u;
					break;
				case 'X':
					base = 16u;
					upper = true;
					break;
				case 'o':
					base = 8u;
					break;
				case 'b':
					base = 2u;
					break;
				case 'B':
					base = 2u;
					upper = true;
					break;
				default:
					isSigned = true;
					break;
			}

			char digits[72];
			usize nd = 0u;
			if (!(sp.precision == 0 && mag == 0u)) // « %.0d » de 0 : aucun chiffre
				nd = NkUIntToDigits(mag, base, upper, digits);

			usize zeros = 0u;
			if (sp.precision > 0 && nd < static_cast<usize>(sp.precision))
				zeros = static_cast<usize>(sp.precision) - nd;
			// '#' en octal : le premier chiffre doit etre un 0 (mesure sur la CRT
			// de reference : « %#.0o » de 0 reste vide, on ne force rien sans chiffre)
			if (sp.alt && base == 8u && nd > 0u && digits[0] != '0' && zeros == 0u)
				zeros = 1u;

			const char *prefix = "";
			usize np = 0u;
			if (sp.alt && mag != 0u) {
				if (base == 16u) {
					prefix = upper ? "0X" : "0x";
					np = 2u;
				} else if (base == 2u) {
					prefix = upper ? "0B" : "0b";
					np = 2u;
				}
			}

			char sign = '\0';
			if (isSigned) {
				if (negative)
					sign = '-';
				else if (sp.plus)
					sign = '+';
				else if (sp.space)
					sign = ' ';
			} else if (sp.conv == 'u' && sp.space) {
				// Mesure sur la CRT de reference (MinGW) : le drapeau espace s'applique
				// a %u decimal (pas a x/X/o) et '+' est ignore sur tout non-signe.
				// Le standard dit « indefini » ; le banc l'a rougi sur 9 716 cas.
				sign = ' ';
			}

			const usize body = (sign ? 1u : 0u) + np + zeros + nd;
			const usize pad = (sp.width > 0 && static_cast<usize>(sp.width) > body)
								  ? static_cast<usize>(sp.width) - body
								  : 0u;
			const bool zeroPad = sp.zero && !sp.left && sp.precision < 0;

			if (!sp.left && !zeroPad)
				out.Fill(' ', pad);
			if (sign)
				out.Put(sign);
			out.Put(prefix, np);
			if (zeroPad)
				out.Fill('0', pad);
			out.Fill('0', zeros);
			out.Put(digits, nd);
			if (sp.left)
				out.Fill(' ', pad);
		}

		// =====================================================================
		// TEXTE (%s, %c) : precision = troncature, largeur = espaces
		// =====================================================================

		template <typename Sink> void NkEmitText(Sink &out, const NkNumSpec &sp, const char *s, usize len) {
			if (sp.precision >= 0 && len > static_cast<usize>(sp.precision))
				len = static_cast<usize>(sp.precision);
			const usize pad = (sp.width > 0 && static_cast<usize>(sp.width) > len)
								  ? static_cast<usize>(sp.width) - len
								  : 0u;
			if (!sp.left)
				out.Fill(' ', pad);
			out.Put(s, len);
			if (sp.left)
				out.Fill(' ', pad);
		}

		// =====================================================================
		// GRAND ENTIER NON SIGNE (base 2^32, poids faible d'abord)
		// =====================================================================

		struct NkBigUInt {
				static constexpr uint32 kMaxLimbs = 40u; // 1280 bits > 1024 + 53 + 4
				uint32 limb[kMaxLimbs];
				uint32 n; // membres utilises ; n == 0 <=> zero ; limb[n-1] != 0

				NkBigUInt() : n(0u) {
					for (uint32 i = 0u; i < kMaxLimbs; ++i)
						limb[i] = 0u;
				}

				bool IsZero() const {
					return n == 0u;
				}

				void Normalize() {
					while (n > 0u && limb[n - 1u] == 0u)
						--n;
				}

				void SetU64(uint64 v) {
					for (uint32 i = 0u; i < kMaxLimbs; ++i)
						limb[i] = 0u;
					limb[0] = static_cast<uint32>(v);
					limb[1] = static_cast<uint32>(v >> 32);
					n = 2u;
					Normalize();
				}

				/// *= 2^bits
				void ShiftLeft(unsigned bits) {
					if (IsZero() || bits == 0u)
						return;
					const unsigned words = bits / 32u;
					const unsigned rem = bits % 32u;
					if (rem != 0u) {
						uint32 carry = 0u;
						for (uint32 i = 0u; i < n; ++i) {
							const uint32 v = limb[i];
							limb[i] = (v << rem) | carry;
							carry = v >> (32u - rem);
						}
						if (carry != 0u)
							limb[n++] = carry;
					}
					if (words != 0u) {
						for (uint32 i = n; i-- > 0u;)
							limb[i + words] = limb[i];
						for (uint32 i = 0u; i < words; ++i)
							limb[i] = 0u;
						n += words;
					}
				}

				/// *= m (m petit)
				void MulSmall(uint32 m) {
					uint64 carry = 0u;
					for (uint32 i = 0u; i < n; ++i) {
						const uint64 cur = static_cast<uint64>(limb[i]) * m + carry;
						limb[i] = static_cast<uint32>(cur);
						carry = cur >> 32;
					}
					if (carry != 0u)
						limb[n++] = static_cast<uint32>(carry);
				}

				/// /= d (d petit), rend le reste
				uint32 DivSmall(uint32 d) {
					uint64 rem = 0u;
					for (uint32 i = n; i-- > 0u;) {
						const uint64 cur = (rem << 32) | limb[i];
						limb[i] = static_cast<uint32>(cur / d);
						rem = cur % d;
					}
					Normalize();
					return static_cast<uint32>(rem);
				}

				bool TestBit(unsigned i) const {
					const unsigned li = i >> 5;
					if (li >= n)
						return false;
					return ((limb[li] >> (i & 31u)) & 1u) != 0u;
				}

				/// Un bit a 1 parmi [0, i) ?
				bool AnyBitBelow(unsigned i) const {
					const unsigned li = i >> 5;
					const unsigned b = i & 31u;
					for (unsigned j = 0u; j < li && j < n; ++j)
						if (limb[j] != 0u)
							return true;
					if (li < n && b != 0u && (limb[li] & ((1u << b) - 1u)) != 0u)
						return true;
					return false;
				}

				/// Rend la valeur des bits >= k (supposee < 16) et les efface.
				unsigned ExtractAbove(unsigned k) {
					const unsigned li = k >> 5;
					const unsigned b = k & 31u;
					if (li >= n)
						return 0u;
					uint64 v = static_cast<uint64>(limb[li]) >> b;
					if (b != 0u && li + 1u < n)
						v |= static_cast<uint64>(limb[li + 1u]) << (32u - b);
					const unsigned digit = static_cast<unsigned>(v & 0xFu);
					limb[li] &= (b != 0u) ? ((1u << b) - 1u) : 0u;
					for (uint32 i = li + 1u; i < n; ++i)
						limb[i] = 0u;
					n = li + 1u;
					Normalize();
					return digit;
				}

				/// Ecrit la valeur en decimal (poids fort d'abord), rend le nombre
				/// de chiffres. Detruit la valeur. Zero -> aucun chiffre.
				usize ToDecimal(char *out) {
					uint32 chunks[40];
					uint32 nc = 0u;
					while (!IsZero())
						chunks[nc++] = DivSmall(1000000000u);
					if (nc == 0u)
						return 0u;
					usize len = 0u;
					char tmp[16];
					for (uint32 c = nc; c-- > 0u;) {
						usize dn = NkUIntToDigits(chunks[c], 10u, false, tmp);
						if (c != nc - 1u) // les blocs internes sont sur 9 chiffres
							for (usize z = dn; z < 9u; ++z)
								out[len++] = '0';
						for (usize i = 0u; i < dn; ++i)
							out[len++] = tmp[i];
					}
					return len;
				}
		};

		// =====================================================================
		// DECOMPOSITION D'UN DOUBLE
		// =====================================================================

		struct NkDoubleBits {
				uint64 bits = 0u;
				uint64 mant = 0u; ///< mantisse entiere (bit cache inclus)
				int exp = 0;	  ///< valeur = mant * 2^exp
				bool negative = false;
				bool isInf = false;
				bool isNan = false;
				bool isZero = false;
		};

		inline NkDoubleBits NkDecompose(double d) {
			NkDoubleBits r;
			uint64 u = 0u;
			{
				const unsigned char *src = reinterpret_cast<const unsigned char *>(&d);
				unsigned char *dst = reinterpret_cast<unsigned char *>(&u);
				for (unsigned i = 0u; i < 8u; ++i)
					dst[i] = src[i];
			}
			r.bits = u;
			const unsigned e = static_cast<unsigned>((u >> 52) & 0x7FFu);
			const uint64 f = u & ((static_cast<uint64>(1) << 52) - 1u);
			r.negative = ((u >> 63) & 1u) != 0u;
			if (e == 0x7FFu) {
				if (f == 0u)
					r.isInf = true;
				else {
					r.isNan = true;
					r.negative = false; // le signe d'un NaN n'a pas de sens ; la CRT de reference ne l'imprime pas
				}
			} else if (e == 0u) {
				if (f == 0u)
					r.isZero = true;
				else {
					r.mant = f; // denormalise
					r.exp = -1074;
				}
			} else {
				r.mant = f | (static_cast<uint64>(1) << 52);
				r.exp = static_cast<int>(e) - 1075;
			}
			return r;
		}

		// =====================================================================
		// DEVELOPPEMENT DECIMAL EXACT D'UN DOUBLE FINI POSITIF
		// =====================================================================

		struct NkDecimalExpansion {
				char intDigits[kMaxIntDigits + 8u];
				uint32 intCount = 0u; ///< 0 <=> partie entiere nulle
				NkBigUInt frac;		  ///< partie fractionnaire = frac / 2^k
				unsigned k = 0u;

				void Init(uint64 mant, int exp2) {
					intCount = 0u;
					k = 0u;
					if (mant == 0u) {
						frac.SetU64(0u);
						return;
					}
					if (exp2 >= 0) {
						NkBigUInt big;
						big.SetU64(mant);
						big.ShiftLeft(static_cast<unsigned>(exp2));
						intCount = static_cast<uint32>(big.ToDecimal(intDigits));
						frac.SetU64(0u);
						return;
					}
					k = static_cast<unsigned>(-exp2);
					uint64 ip = 0u;
					uint64 fp = mant;
					if (k < 64u) {
						ip = mant >> k;
						fp = mant & ((static_cast<uint64>(1) << k) - 1u);
					}
					if (ip != 0u)
						intCount = static_cast<uint32>(NkUIntToDigits(ip, 10u, false, intDigits));
					frac.SetU64(fp);
				}

				unsigned NextFracDigit() {
					if (frac.IsZero())
						return 0u;
					frac.MulSmall(10u);
					return frac.ExtractAbove(k);
				}

				bool FracIsZero() const {
					return frac.IsZero();
				}

				/// Reste fractionnaire compare a 1/2 : -1, 0 (egalite exacte), +1
				int FracVsHalf() const {
					if (k == 0u || frac.IsZero())
						return -1;
					if (!frac.TestBit(k - 1u))
						return -1;
					return frac.AnyBitBelow(k - 1u) ? 1 : 0;
				}
		};

		// =====================================================================
		// ARRONDI AU PLUS PROCHE, EGALITE VERS LE PAIR
		// =====================================================================

		/// Arrondit d[0..n) selon le reste r (-1/0/+1). Rend vrai si la retenue
		/// est sortie par la gauche (les chiffres valent alors tous '0' et il faut
		/// prefixer un '1').
		inline bool NkRoundDigits(char *d, usize n, int r) {
			if (n == 0u)
				return false;
			const bool up = (r > 0) || (r == 0 && (((d[n - 1u] - '0') & 1) != 0));
			if (!up)
				return false;
			usize i = n;
			while (i-- > 0u) {
				if (d[i] == '9') {
					d[i] = '0';
				} else {
					++d[i];
					return false;
				}
			}
			return true;
		}

		// =====================================================================
		// CORPS DE FLOTTANTS (sans signe, sans largeur)
		// =====================================================================

		/// %f : `out` doit avoir kBodyMax octets ; rend la longueur.
		inline usize NkFixedBody(const NkDoubleBits &b, int prec, bool alt, char *out) {
			const uint32 effPrec = prec < 0 ? 6u : static_cast<uint32>(prec);
			const uint32 gen = effPrec > kMaxPrecision ? kMaxPrecision : effPrec;

			NkDecimalExpansion ex;
			ex.Init(b.mant, b.exp);

			char d[kMaxIntDigits + kMaxPrecision + 8u];
			usize n = 0u;
			if (ex.intCount == 0u) {
				d[n++] = '0';
			} else {
				for (uint32 i = 0u; i < ex.intCount; ++i)
					d[n++] = ex.intDigits[i];
			}
			usize intN = n;
			for (uint32 i = 0u; i < gen; ++i)
				d[n++] = static_cast<char>('0' + ex.NextFracDigit());

			if (NkRoundDigits(d, n, ex.FracVsHalf())) {
				for (usize i = n; i-- > 0u;)
					d[i + 1u] = d[i];
				d[0] = '1';
				++n;
				++intN;
			}

			usize len = 0u;
			for (usize i = 0u; i < intN; ++i)
				out[len++] = d[i];
			if (effPrec > 0u || alt)
				out[len++] = '.';
			for (usize i = intN; i < n; ++i)
				out[len++] = d[i];
			for (uint32 i = gen; i < effPrec; ++i)
				out[len++] = '0';
			return len;
		}

		/// %e : rend la longueur ; `expOut` recoit l'exposant decimal APRES arrondi.
		inline usize NkExpBody(const NkDoubleBits &b, int prec, bool alt, bool upper, char *out, int *expOut) {
			const uint32 effPrec = prec < 0 ? 6u : static_cast<uint32>(prec);
			const uint32 gen = effPrec > kMaxPrecision ? kMaxPrecision : effPrec;
			const usize need = static_cast<usize>(gen) + 1u;

			char d[kMaxPrecision + 8u];
			usize n = 0u;
			int X = 0;

			if (b.isZero || b.mant == 0u) {
				for (usize i = 0u; i < need; ++i)
					d[n++] = '0';
			} else {
				NkDecimalExpansion ex;
				ex.Init(b.mant, b.exp);
				int r;
				if (ex.intCount > 0u) {
					X = static_cast<int>(ex.intCount) - 1;
					if (ex.intCount >= need) {
						for (usize i = 0u; i < need; ++i)
							d[n++] = ex.intDigits[i];
						if (ex.intCount == need) {
							r = ex.FracVsHalf();
						} else {
							const char c = ex.intDigits[need];
							if (c > '5')
								r = 1;
							else if (c < '5')
								r = -1;
							else {
								r = 0;
								for (uint32 i = static_cast<uint32>(need) + 1u; i < ex.intCount; ++i)
									if (ex.intDigits[i] != '0') {
										r = 1;
										break;
									}
								if (r == 0 && !ex.FracIsZero())
									r = 1;
							}
						}
					} else {
						for (uint32 i = 0u; i < ex.intCount; ++i)
							d[n++] = ex.intDigits[i];
						while (n < need)
							d[n++] = static_cast<char>('0' + ex.NextFracDigit());
						r = ex.FracVsHalf();
					}
				} else {
					int zeros = 0;
					unsigned dg;
					do {
						dg = ex.NextFracDigit();
						++zeros;
					} while (dg == 0u);
					X = -zeros;
					d[n++] = static_cast<char>('0' + dg);
					while (n < need)
						d[n++] = static_cast<char>('0' + ex.NextFracDigit());
					r = ex.FracVsHalf();
				}
				if (NkRoundDigits(d, n, r)) {
					d[0] = '1'; // 9,99 -> 10,0 : les autres chiffres sont deja '0'
					++X;
				}
			}

			usize len = 0u;
			out[len++] = d[0];
			if (effPrec > 0u || alt)
				out[len++] = '.';
			for (usize i = 1u; i < n; ++i)
				out[len++] = d[i];
			for (uint32 i = gen; i < effPrec; ++i)
				out[len++] = '0';
			out[len++] = upper ? 'E' : 'e';
			out[len++] = X < 0 ? '-' : '+';
			{
				char ed[8];
				const usize en = NkUIntToDigits(static_cast<uint64>(X < 0 ? -X : X), 10u, false, ed);
				if (en < 2u)
					out[len++] = '0';
				for (usize i = 0u; i < en; ++i)
					out[len++] = ed[i];
			}
			if (expOut)
				*expOut = X;
			return len;
		}

		/// %g : rend la longueur.
		inline usize NkGeneralBody(const NkDoubleBits &b, int prec, bool alt, bool upper, char *out) {
			int P = prec < 0 ? 6 : (prec == 0 ? 1 : prec);
			int X = 0;
			usize len;
			if (b.isZero || b.mant == 0u) {
				len = NkFixedBody(b, P - 1, alt, out);
			} else {
				len = NkExpBody(b, P - 1, alt, upper, out, &X);
				if (!(X < -4 || X >= P))
					len = NkFixedBody(b, P - 1 - X, alt, out);
			}
			if (!alt) {
				// retire les zeros de fin de la mantisse, puis le point s'il est seul
				usize mEnd = len;
				for (usize i = 0u; i < len; ++i)
					if (out[i] == 'e' || out[i] == 'E') {
						mEnd = i;
						break;
					}
				bool hasDot = false;
				for (usize i = 0u; i < mEnd; ++i)
					if (out[i] == '.') {
						hasDot = true;
						break;
					}
				if (hasDot) {
					usize newEnd = mEnd;
					while (newEnd > 0u && out[newEnd - 1u] == '0')
						--newEnd;
					if (newEnd > 0u && out[newEnd - 1u] == '.')
						--newEnd;
					if (newEnd != mEnd) {
						for (usize i = mEnd; i < len; ++i)
							out[newEnd + (i - mEnd)] = out[i];
						len -= (mEnd - newEnd);
					}
				}
			}
			return len;
		}

		/// %a : hexadecimal flottant (0x1.8p+1). Rend la longueur.
		inline usize NkHexBody(const NkDoubleBits &b, int prec, bool alt, bool upper, char *out) {
			const char *digs = upper ? "0123456789ABCDEF" : "0123456789abcdef";
			usize len = 0u;
			out[len++] = '0';
			out[len++] = upper ? 'X' : 'x';

			unsigned lead;
			uint64 frac; // 52 bits
			int E;
			const unsigned e = static_cast<unsigned>((b.bits >> 52) & 0x7FFu);
			frac = b.bits & ((static_cast<uint64>(1) << 52) - 1u);
			if (b.isZero) {
				lead = 0u;
				E = 0;
			} else if (e == 0u) {
				lead = 0u;
				E = -1022;
			} else {
				lead = 1u;
				E = static_cast<int>(e) - 1023;
			}

			unsigned nibbles = 13u;
			if (prec < 0) {
				while (nibbles > 0u && (frac & 0xFu) == 0u) {
					frac >>= 4;
					--nibbles;
				}
			} else if (prec < 13) {
				const unsigned shift = (13u - static_cast<unsigned>(prec)) * 4u;
				const uint64 rem = frac & ((static_cast<uint64>(1) << shift) - 1u);
				const uint64 half = static_cast<uint64>(1) << (shift - 1u);
				frac >>= shift;
				if (rem > half || (rem == half && ((frac & 1u) != 0u || (prec == 0 && (lead & 1u) != 0u)))) {
					++frac;
					if (prec == 0 || (frac >> (4u * static_cast<unsigned>(prec))) != 0u) {
						frac = 0u;
						++lead;
					}
				}
				nibbles = static_cast<unsigned>(prec);
			}

			out[len++] = digs[lead & 0xFu];
			if (nibbles > 0u || alt)
				out[len++] = '.';
			for (unsigned i = nibbles; i-- > 0u;)
				out[len++] = digs[(i < 13u ? (frac >> (4u * i)) : 0u) & 0xFu];
			if (prec > 13)
				for (int i = 13; i < prec; ++i)
					out[len++] = '0';
			out[len++] = upper ? 'P' : 'p';
			out[len++] = E < 0 ? '-' : '+';
			{
				char ed[8];
				const usize en = NkUIntToDigits(static_cast<uint64>(E < 0 ? -E : E), 10u, false, ed);
				for (usize i = 0u; i < en; ++i)
					out[len++] = ed[i];
			}
			return len;
		}

		/// Corps d'un flottant fini selon la conversion (f F e E g G a A).
		/// Signe et largeur exclus. `out` doit avoir kBodyMax octets.
		inline usize NkFloatBody(const NkDoubleBits &b, char conv, int prec, bool alt, char *out) {
			switch (conv) {
				case 'f':
				case 'F':
					return NkFixedBody(b, prec, alt, out);
				case 'e':
					return NkExpBody(b, prec, alt, false, out, nullptr);
				case 'E':
					return NkExpBody(b, prec, alt, true, out, nullptr);
				case 'G':
					return NkGeneralBody(b, prec, alt, true, out);
				case 'a':
					return NkHexBody(b, prec, alt, false, out);
				case 'A':
					return NkHexBody(b, prec, alt, true, out);
				case 'g':
				default:
					return NkGeneralBody(b, prec, alt, false, out);
			}
		}

		/// Corps d'un flottant QUELCONQUE (inf / nan compris), sans signe.
		inline usize NkFloatBodyAny(const NkDoubleBits &b, char conv, int prec, bool alt, char *out) {
			const bool upper = (conv == 'F' || conv == 'E' || conv == 'G' || conv == 'A');
			if (b.isNan) {
				const char *s = upper ? "NAN" : "nan";
				out[0] = s[0];
				out[1] = s[1];
				out[2] = s[2];
				return 3u;
			}
			if (b.isInf) {
				const char *s = upper ? "INF" : "inf";
				out[0] = s[0];
				out[1] = s[1];
				out[2] = s[2];
				return 3u;
			}
			return NkFloatBody(b, conv, prec, alt, out);
		}

		/// Emet un flottant avec la semantique printf complete.
		template <typename Sink> void NkEmitFloat(Sink &out, const NkNumSpec &sp, double v) {
			const NkDoubleBits b = NkDecompose(v);
			char body[kBodyMax];
			const usize bl = NkFloatBodyAny(b, sp.conv, sp.precision, sp.alt, body);
			const bool special = b.isNan || b.isInf;

			char sign = '\0';
			if (b.negative)
				sign = '-';
			else if (sp.plus)
				sign = '+';
			else if (sp.space)
				sign = ' ';

			const usize bodyLen = (sign ? 1u : 0u) + bl;
			const usize pad = (sp.width > 0 && static_cast<usize>(sp.width) > bodyLen)
								  ? static_cast<usize>(sp.width) - bodyLen
								  : 0u;
			const bool zeroPad = sp.zero && !sp.left && !special;

			if (!sp.left && !zeroPad)
				out.Fill(' ', pad);
			if (sign)
				out.Put(sign);
			if (zeroPad)
				out.Fill('0', pad);
			out.Put(body, bl);
			if (sp.left)
				out.Fill(' ', pad);
		}

		/// Emet un pointeur : « 0x » + hexadecimal minimal (nullptr -> 0x0).
		/// Choix delibere, identique sur toutes les plateformes (la CRT MinGW
		/// imprime 16 chiffres sans prefixe, glibc « 0x… » ou « (nil) »).
		template <typename Sink> void NkEmitPointer(Sink &out, const NkNumSpec &sp, uintptr p) {
			NkNumSpec s = sp;
			s.conv = 'x';
			s.alt = true;
			s.precision = -1;
			if (p == 0u) {
				NkNumSpec t = sp;
				t.conv = 's';
				NkEmitText(out, t, "0x0", 3u);
				return;
			}
			NkEmitInteger(out, s, false, static_cast<uint64>(p));
		}

	} // namespace numtext
} // namespace nkentseu

#endif // NK_CORE_TEXT_NKNUMBERTOTEXT_H_INCLUDED
