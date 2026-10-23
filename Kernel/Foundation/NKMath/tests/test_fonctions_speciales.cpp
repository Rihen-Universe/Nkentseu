// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// (19/10/2026) Le banc de NkErf et NkErfc (NKMath/NkSpecialFunctions.h) : des valeurs connues, la
// comparaison a la bibliotheque standard sur une grille (la reference, ICI SEULEMENT : regle de Rodolf,
// « dans le banc seulement »), et la vitesse. Contre-epreuve dans le meme banc : l'approximation
// d'Abramowitz et Stegun 7.1.26 (erreur 1,5e-7) doit etre refusee par les memes tolerances.
// -----------------------------------------------------------------------------
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMath/NkSpecialFunctions.h"
#include "NKMath/NkFunctions.h"
#include "NKContainers/String/NkPrint.h"
#include "NKContainers/String/NkString.h"

// (19/10, 15 h 52) La bibliotheque standard n'est plus ici que LA REFERENCE (std::erfc) : la sortie par NkPrint, les
// valeurs absolues et l'exponentielle par NKMath, l'horloge par le compteur du systeme. La garde (garde_std.py) compte
// ces deux appels de reference et leur en-tete : c'est voulu (« dans le banc seulement »).
#include <cmath>
#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

using namespace nkentseu;
using namespace nkentseu::math;

namespace {
	template <typename... Args> void Ecrire(const char *format, Args... args) {
		NkPrint("{0}", NkString::Format(format, args...));
	}

	/// Secondes, horloge monotone du systeme (QueryPerformanceCounter ; clock_gettime ailleurs).
	double Secondes() {
#if defined(_WIN32)
		LARGE_INTEGER f, c;
		QueryPerformanceFrequency(&f);
		QueryPerformanceCounter(&c);
		return (double)c.QuadPart / (double)f.QuadPart;
#else
		timespec t;
		clock_gettime(CLOCK_MONOTONIC, &t);
		return (double)t.tv_sec + 1e-9 * (double)t.tv_nsec;
#endif
	}

	double AbramowitzStegunErfc(double x) {
		const double z = x < 0.0 ? -x : x;
		const double t = 1.0 / (1.0 + 0.3275911 * z);
		const double erf = 1.0 - (((((1.061405429 * t - 1.453152027) * t) + 1.421413741) * t - 0.284496736) * t + 0.254829592) * t * math::NkExp(-z * z);
		return x >= 0.0 ? 1.0 - erf : 1.0 + erf;
	}

	/// L'ecart d'une fonction a std::erfc sur [-6, 27] : absolu sur [-6, 2], relatif au-dela (les petites valeurs).
	void Ecarts(double (*f)(double), double &pireAbsolu, double &pireRelatif) {
		pireAbsolu = 0.0;
		pireRelatif = 0.0;
		for (int i = -6000; i <= 27000; ++i) {
			const double x = (double)i * 0.001;
			const double v = f(x), r = std::erfc(x);
			const double e = math::NkFabs(v - r);
			if (x < 2.0) {
				pireAbsolu = e > pireAbsolu ? e : pireAbsolu;
			} else if (r > 1e-300) {
				// (19/10, apres la premiere mesure) Le relatif sur les doubles NORMAUX seulement : sous 1e-308 (x >
				// 26,5), un double denormalise n'a plus ses 52 bits, aucune precision relative n'y est possible.
				const double rel = e / r;
				pireRelatif = rel > pireRelatif ? rel : pireRelatif;
			}
		}
	}

	double NkErfcDouble(double x) {
		return NkErfc((float64)x);
	}
} // namespace

TEST_CASE(NKMathSpeciales, ErfcValeursConnues) {
	struct Connu {
			double x, erfc;
	};
	// erfc(0) = 1 ; erfc(0,5) ; erfc(1) ; erfc(2) ; erfc(3) ; erfc(5) ; erfc(-1) = 2 - erfc(1).
	const Connu connus[7] = {{0.0, 1.0},
							 {0.5, 0.4795001221869534},
							 {1.0, 0.15729920705028513},
							 {2.0, 0.004677734981047266},
							 {3.0, 2.209049699858544e-05},
							 {5.0, 1.5374597944280349e-12},
							 {-1.0, 1.8427007929497148}};
	int justes = 0;
	for (const Connu &c : connus) {
		const double v = NkErfc((float64)c.x);
		const bool juste = math::NkFabs(v - c.erfc) <= 1e-15 * (c.erfc > 1.0 ? c.erfc : 1.0) || math::NkFabs(v - c.erfc) <= 1e-13 * c.erfc;
		justes += juste ? 1 : 0;
		Ecrire("  erfc(%.1f) = %.17g (attendu %.17g)\n", c.x, v, c.erfc);
	}
	ASSERT_EQUAL(7, justes);
	ASSERT_TRUE(math::NkFabs(NkErf((float64)1.0) - 0.8427007929497149) < 1e-15);
	ASSERT_TRUE(NkErfc((float64)30.0) == 0.0);
}

TEST_CASE(NKMathSpeciales, ErfcContreLaBibliothequeStandard) {
	double absNk = 0.0, relNk = 0.0, absAs = 0.0, relAs = 0.0;
	Ecarts(&NkErfcDouble, absNk, relNk);
	Ecarts(&AbramowitzStegunErfc, absAs, relAs);
	Ecrire("  NkErfc contre std::erfc sur [-6 ; 27], pas 0,001 : ecart absolu %.2e sur [-6 ; 2], relatif %.2e au-dela\n", absNk, relNk);
	Ecrire("  contre-epreuve Abramowitz et Stegun 7.1.26 : absolu %.2e, relatif %.2e (doit depasser 1e-12)\n", absAs, relAs);
	// Criteres ecrits avant la mesure : 1e-15 en absolu, 1e-12 en relatif (sur les queues, la fraction continue
	// porte e^(-x²), que la bibliotheque calcule aussi a quelques ulp pres).
	ASSERT_TRUE(absNk <= 1e-15);
	ASSERT_TRUE(relNk <= 1e-12);
	ASSERT_TRUE(absAs > 1e-12 || relAs > 1e-12);
}

TEST_CASE(NKMathSpeciales, ErfcVitesse) {
	constexpr int kN = 2000000;
	volatile double puitsNk = 0.0, puitsStd = 0.0;
	const double a0 = Secondes();
	for (int i = 0; i < kN; ++i) {
		puitsNk = puitsNk + NkErfc((float64)(-6.0 + 12.0 * (double)i / (double)kN));
	}
	const double a1 = Secondes();
	for (int i = 0; i < kN; ++i) {
		puitsStd = puitsStd + std::erfc(-6.0 + 12.0 * (double)i / (double)kN);
	}
	const double a2 = Secondes();
	const double nsNk = (a1 - a0) * 1e9 / (double)kN;
	const double nsStd = (a2 - a1) * 1e9 / (double)kN;
	Ecrire("  vitesse sur [-6 ; 6] : NkErfc %.1f ns par appel, std::erfc %.1f ns (rapport %.2f)\n", nsNk, nsStd, nsStd > 0.0 ? nsNk / nsStd : 0.0);
	ASSERT_TRUE(math::NkFabs(puitsNk - puitsStd) < 1e-6 * (double)kN);
	ASSERT_TRUE(nsNk > 0.0);
}
