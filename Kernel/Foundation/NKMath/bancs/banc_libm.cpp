// =============================================================================
// NKMath/bancs/banc_libm.cpp
// Le banc de NkLibm CONTRE la libm : écart en ULP sur des millions de valeurs
// tirées au hasard dans chaque plage, valeurs spéciales au bit près, et temps.
//
// Critères (code de sortie 0 seulement si tous tiennent) :
//   - Fabs, Copysign, Trunc, Floor, Ceil, Round, Sqrt : 0 ULP (identiques) ;
//   - Sin, Cos, Exp, Log (float64) : au plus 1 ULP de la libm ;
//   - Tan (float64) : au plus 2 ULP (quotient de deux noyaux) ;
//   - versions float32 : au plus 1 ULP (en float) de sinf, cosf, expf, logf.
// Contre-épreuves : compiler NkLibm.cpp avec -DNK_LIBM_CONTRE_EPREUVE=1 (un terme
// du sinus en moins) ou =2 (un bit de 2/pi faux) : le banc doit échouer.
//
// Ce banc emploie <math.h> et <stdio.h> : la libm est l'objet de la comparaison.
//
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// DATE : 2026-10-22
// =============================================================================
#include "NKMath/NkLibm.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace L = nkentseu::math::libm;

namespace {

	uint64_t gEtat = 0x9E3779B97F4A7C15ull;
	uint64_t Alea() {
		gEtat ^= gEtat << 13;
		gEtat ^= gEtat >> 7;
		gEtat ^= gEtat << 17;
		return gEtat;
	}
	double Unif(double a, double b) {
		return a + (b - a) * ((double)(Alea() >> 11) * 0x1p-53);
	}
	// |x| réparti en logarithme entre a et b, signe au hasard si signe != 0.
	double LogUnif(double a, double b, int signe) {
		const double v = exp(Unif(log(a), log(b)));
		return (signe && (Alea() & 1)) ? -v : v;
	}

	int64_t Ordonne(double x) {
		int64_t i;
		memcpy(&i, &x, 8);
		return i < 0 ? INT64_MIN - i : i;
	}
	uint64_t Ulp(double a, double b) {
		if (a != a && b != b)
			return 0;
		if (a != a || b != b)
			return UINT64_MAX;
		const int64_t d = Ordonne(a) - Ordonne(b);
		return (uint64_t)(d < 0 ? -d : d);
	}
	int32_t Ordonne32(float x) {
		int32_t i;
		memcpy(&i, &x, 4);
		return i < 0 ? INT32_MIN - i : i;
	}
	uint64_t Ulp32(float a, float b) {
		if (a != a && b != b)
			return 0;
		if (a != a || b != b)
			return UINT64_MAX;
		const int64_t d = (int64_t)Ordonne32(a) - (int64_t)Ordonne32(b);
		return (uint64_t)(d < 0 ? -d : d);
	}

	double Secondes() {
		LARGE_INTEGER t, f;
		QueryPerformanceCounter(&t);
		QueryPerformanceFrequency(&f);
		return (double)t.QuadPart / (double)f.QuadPart;
	}

	const int kN = 1000000;
	double gX[kN];
	volatile double gPuits;
	int gFautes = 0;

	typedef double (*F64)(double);
	typedef float (*F32)(float);

	void Cas(const char *nom, const char *plage, double (*nk)(double), double (*ref)(double), uint64_t tolere) {
		uint64_t pire = 0, sup1 = 0, egaux = 0;
		double xPire = 0.0;
		for (int i = 0; i < kN; ++i) {
			const uint64_t u = Ulp(nk(gX[i]), ref(gX[i]));
			if (u == 0)
				++egaux;
			if (u > 1)
				++sup1;
			if (u > pire) {
				pire = u;
				xPire = gX[i];
			}
		}
		// Le temps : meilleur de 5 passes ALTERNÉES (la machine fait autre chose).
		double tn = 1e30, tl = 1e30, s = 0.0;
		for (int passe = 0; passe < 5; ++passe) {
			double t0 = Secondes();
			for (int i = 0; i < kN; ++i)
				s += nk(gX[i]);
			double t1 = Secondes();
			for (int i = 0; i < kN; ++i)
				s += ref(gX[i]);
			double t2 = Secondes();
			if (t1 - t0 < tn)
				tn = t1 - t0;
			if (t2 - t1 < tl)
				tl = t2 - t1;
		}
		gPuits = s;
		const double t0 = 0.0, t1 = tn, t2 = tn + tl;
		const bool ok = pire <= tolere;
		if (!ok)
			++gFautes;
		printf("%-9s %-22s pire %-6llu >1ulp %-7llu identiques %6.2f%%  nk %6.1f ns  libm %6.1f ns  %s",
			   nom, plage, (unsigned long long)pire, (unsigned long long)sup1, 100.0 * (double)egaux / kN,
			   (t1 - t0) * 1e9 / kN, (t2 - t1) * 1e9 / kN, ok ? "OK" : "FAUTE");
		if (pire > 0)
			printf("  (pire en x = %.17g)", xPire);
		printf("\n");
	}

	void Cas32(const char *nom, const char *plage, float (*nk)(float), float (*ref)(float)) {
		uint64_t pire = 0;
		float xPire = 0.0f;
		for (int i = 0; i < kN; ++i) {
			const float x = (float)gX[i];
			const uint64_t u = Ulp32(nk(x), ref(x));
			if (u > pire) {
				pire = u;
				xPire = x;
			}
		}
		double tn = 1e30, tl = 1e30, s = 0.0;
		for (int passe = 0; passe < 5; ++passe) {
			double t0 = Secondes();
			for (int i = 0; i < kN; ++i)
				s += nk((float)gX[i]);
			double t1 = Secondes();
			for (int i = 0; i < kN; ++i)
				s += ref((float)gX[i]);
			double t2 = Secondes();
			if (t1 - t0 < tn)
				tn = t1 - t0;
			if (t2 - t1 < tl)
				tl = t2 - t1;
		}
		gPuits = s;
		const double t0 = 0.0, t1 = tn, t2 = tn + tl;
		const bool ok = pire <= 1;
		if (!ok)
			++gFautes;
		printf("%-9s %-22s pire %-6llu (float)                        nk %6.1f ns  libm %6.1f ns  %s", nom, plage,
			   (unsigned long long)pire, (t1 - t0) * 1e9 / kN, (t2 - t1) * 1e9 / kN, ok ? "OK" : "FAUTE");
		if (pire > 0)
			printf("  (pire en x = %.9g)", (double)xPire);
		printf("\n");
	}

	// Les enveloppes : les mêmes signatures pour NkLibm et la libm.
	double NkSin(double x) { return L::Sin(x); }
	double NkCos(double x) { return L::Cos(x); }
	double NkTan(double x) { return L::Tan(x); }
	double NkExp(double x) { return L::Exp(x); }
	double NkLog(double x) { return L::Log(x); }
	double NkSqrt(double x) { return L::Sqrt(x); }
	double NkFloor(double x) { return L::Floor(x); }
	double NkCeil(double x) { return L::Ceil(x); }
	double NkTrunc(double x) { return L::Trunc(x); }
	double NkRound(double x) { return L::Round(x); }
	double NkFabs(double x) { return L::Fabs(x); }
	double CSin(double x) { return sin(x); }
	double CCos(double x) { return cos(x); }
	double CTan(double x) { return tan(x); }
	double CExp(double x) { return exp(x); }
	double CLog(double x) { return log(x); }
	double CSqrt(double x) { return sqrt(x); }
	double CFloor(double x) { return floor(x); }
	double CCeil(double x) { return ceil(x); }
	double CTrunc(double x) { return trunc(x); }
	double CRound(double x) { return round(x); }
	double CFabs(double x) { return fabs(x); }
	float NkSinf(float x) { return L::Sin(x); }
	float NkCosf(float x) { return L::Cos(x); }
	float NkExpf(float x) { return L::Exp(x); }
	float NkLogf(float x) { return L::Log(x); }
	float NkSqrtf(float x) { return L::Sqrt(x); }
	float CSinf(float x) { return sinf(x); }
	float CCosf(float x) { return cosf(x); }
	float CExpf(float x) { return expf(x); }
	float CLogf(float x) { return logf(x); }
	float CSqrtf(float x) { return sqrtf(x); }

	void Remplir(double a, double b) {
		for (int i = 0; i < kN; ++i)
			gX[i] = Unif(a, b);
	}
	void RemplirLog(double a, double b, int signe) {
		for (int i = 0; i < kN; ++i)
			gX[i] = LogUnif(a, b, signe);
	}

	// Les valeurs spéciales, au bit près.
	void Speciales() {
		const double inf = 1.0 / 0.0 * 1.0, nan = inf - inf;
		const double v[] = {0.0, -0.0, inf, -inf, nan, 1e-310, -1e-310, 5e-324, 1e-20, 0.5, -0.5, 1.0, -1.0,
							2.5, -2.5, 0.49999999999999994, -0.49999999999999994, 4503599627370495.5, 1e22, -1e22,
							1e300, 1.5707963267948966, 3.141592653589793, 6.283185307179586, 7.0e15, 709.78, -745.1, 710.0};
		int fautes = 0;
		for (double x : v) {
			struct {
					const char *nom;
					double a, b;
					uint64_t tol;
			} t[] = {
				{"fabs", L::Fabs(x), fabs(x), 0},	 {"trunc", L::Trunc(x), trunc(x), 0}, {"floor", L::Floor(x), floor(x), 0},
				{"ceil", L::Ceil(x), ceil(x), 0},	 {"round", L::Round(x), round(x), 0}, {"sqrt", L::Sqrt(x), sqrt(x), 0},
				{"sin", L::Sin(x), sin(x), 1},		 {"cos", L::Cos(x), cos(x), 1},		  {"tan", L::Tan(x), tan(x), 2},
				{"exp", L::Exp(x), exp(x), 1},		 {"log", L::Log(x), log(x), 1},
			};
			for (auto &c : t) {
				// Les zéros signés doivent garder leur signe : 0 ULP s'entend au bit près.
				uint64_t ba, bb;
				memcpy(&ba, &c.a, 8);
				memcpy(&bb, &c.b, 8);
				const bool memeBits = ba == bb || (c.a != c.a && c.b != c.b);
				if (Ulp(c.a, c.b) > c.tol || (c.tol == 0 && !memeBits)) {
					printf("  SPECIALE FAUTE %s(%.17g) : nk %.17g, libm %.17g\n", c.nom, x, c.a, c.b);
					++fautes;
				}
			}
		}
		printf("valeurs speciales : %d fautes sur %d\n", fautes, (int)(sizeof(v) / sizeof(v[0])) * 11);
		gFautes += fautes;
	}

	// ── Contre les résultats CORRECTS (reference_libm.py) ───────────────────
	struct Bilan {
			const char *nom;
			uint64_t tol, n = 0, pireNk = 0, pireLibm = 0, exactNk = 0, exactLibm = 0, sup1Nk = 0, sup1Libm = 0;
			double xPire = 0.0;
	};

	double De64(uint64_t u) {
		double d;
		memcpy(&d, &u, 8);
		return d;
	}
	float De32(uint32_t u) {
		float f;
		memcpy(&f, &u, 4);
		return f;
	}

	void Compter(Bilan &b, uint64_t uNk, uint64_t uLibm, double x) {
		++b.n;
		if (uNk == 0)
			++b.exactNk;
		if (uLibm == 0)
			++b.exactLibm;
		if (uNk > 1)
			++b.sup1Nk;
		if (uLibm > 1)
			++b.sup1Libm;
		if (uNk > b.pireNk) {
			b.pireNk = uNk;
			b.xPire = x;
		}
		if (uLibm > b.pireLibm)
			b.pireLibm = uLibm;
	}

	int Reference(const char *chemin) {
		FILE *f = fopen(chemin, "r");
		if (f == nullptr) {
			printf("reference introuvable : %s\n", chemin);
			return 2;
		}
		Bilan b[] = {{"sin", 1}, {"cos", 1}, {"tan", 2}, {"exp", 1}, {"log", 1}, {"sqrt", 0},
					 {"sinf", 1}, {"cosf", 1}, {"expf", 1}, {"logf", 1}, {"sqrtf", 0}};
		char nom[16];
		unsigned long long ux, uy;
		while (fscanf(f, "%15s %llx %llx", nom, &ux, &uy) == 3) {
			for (Bilan &c : b) {
				if (strcmp(nom, c.nom) != 0)
					continue;
				if (nom[strlen(nom) - 1] == 'f') {
					const float x = De32((uint32_t)ux), juste = De32((uint32_t)uy);
					float nk = 0, lm = 0;
					if (c.nom[0] == 's' && c.nom[1] == 'i') {
						nk = L::Sin(x);
						lm = sinf(x);
					} else if (c.nom[0] == 'c') {
						nk = L::Cos(x);
						lm = cosf(x);
					} else if (c.nom[0] == 'e') {
						nk = L::Exp(x);
						lm = expf(x);
					} else if (c.nom[0] == 'l') {
						nk = L::Log(x);
						lm = logf(x);
					} else {
						nk = L::Sqrt(x);
						lm = sqrtf(x);
					}
					Compter(c, Ulp32(nk, juste), Ulp32(lm, juste), (double)x);
				} else {
					const double x = De64(ux), juste = De64(uy);
					double nk = 0, lm = 0;
					switch (c.nom[0] == 's' ? (c.nom[1] == 'i' ? 0 : 5) : (c.nom[0] == 'c' ? 1 : (c.nom[0] == 't' ? 2 : (c.nom[0] == 'e' ? 3 : 4)))) {
						case 0: nk = L::Sin(x); lm = sin(x); break;
						case 1: nk = L::Cos(x); lm = cos(x); break;
						case 2: nk = L::Tan(x); lm = tan(x); break;
						case 3: nk = L::Exp(x); lm = exp(x); break;
						case 4: nk = L::Log(x); lm = log(x); break;
						default: nk = L::Sqrt(x); lm = sqrt(x); break;
					}
					Compter(c, Ulp(nk, juste), Ulp(lm, juste), x);
				}
			}
		}
		fclose(f);
		int fautes = 0;
		printf("ecart au resultat CORRECT (arrondi exact, calcule a 50 chiffres) :\n");
		printf("%-6s %8s | %-28s | %-28s\n", "", "points", "NkLibm : pire, >1 ulp, exacts", "libm : pire, >1 ulp, exacts");
		for (Bilan &c : b) {
			const bool ok = c.n > 0 && c.pireNk <= c.tol;
			if (!ok)
				++fautes;
			printf("%-6s %8llu | %4llu ulp %6llu %7.3f%%   | %4llu ulp %6llu %7.3f%%   %s", c.nom, (unsigned long long)c.n,
				   (unsigned long long)c.pireNk, (unsigned long long)c.sup1Nk, c.n ? 100.0 * (double)c.exactNk / (double)c.n : 0.0,
				   (unsigned long long)c.pireLibm, (unsigned long long)c.sup1Libm,
				   c.n ? 100.0 * (double)c.exactLibm / (double)c.n : 0.0, ok ? "OK" : "FAUTE");
			if (c.pireNk > 0)
				printf("  (pire nk en x = %.17g)", c.xPire);
			printf("\n");
		}
		printf("fautes : %d\n", fautes);
		return fautes == 0 ? 0 : 1;
	}

} // namespace

int main(int argc, char **argv) {
	if (argc >= 3 && strcmp(argv[1], "--reference") == 0)
		return Reference(argv[2]);
	const double pi = 3.141592653589793;
	Speciales();

	Remplir(-pi / 4, pi / 4);
	Cas("sin", "[-pi/4, pi/4]", NkSin, CSin, 1);
	Cas("cos", "[-pi/4, pi/4]", NkCos, CCos, 1);
	Cas("tan", "[-pi/4, pi/4]", NkTan, CTan, 2);
	Remplir(-100.0, 100.0);
	Cas("sin", "[-100, 100]", NkSin, CSin, 1);
	Cas("cos", "[-100, 100]", NkCos, CCos, 1);
	Cas("tan", "[-100, 100]", NkTan, CTan, 2);
	Remplir(-1e6, 1e6);
	Cas("sin", "[-1e6, 1e6]", NkSin, CSin, 1);
	Cas("cos", "[-1e6, 1e6]", NkCos, CCos, 1);
	RemplirLog(2e6, 1e22, 1);
	Cas("sin", "|x| 2e6..1e22", NkSin, CSin, 1);
	Cas("cos", "|x| 2e6..1e22", NkCos, CCos, 1);
	Cas("tan", "|x| 2e6..1e22", NkTan, CTan, 2);
	RemplirLog(1e22, 1e308, 1);
	Cas("sin", "|x| 1e22..1e308", NkSin, CSin, 1);
	Cas("cos", "|x| 1e22..1e308", NkCos, CCos, 1);

	Remplir(-745.0, 709.0);
	Cas("exp", "[-745, 709]", NkExp, CExp, 1);
	Remplir(-1.0, 1.0);
	Cas("exp", "[-1, 1]", NkExp, CExp, 1);
	RemplirLog(1e-307, 1e308, 0);
	Cas("log", "1e-307..1e308", NkLog, CLog, 1);
	Remplir(0.5, 2.0);
	Cas("log", "[0,5 ; 2]", NkLog, CLog, 1);

	RemplirLog(1e-300, 1e300, 0);
	Cas("sqrt", "1e-300..1e300", NkSqrt, CSqrt, 0);
	RemplirLog(1e-3, 1e17, 1);
	Cas("floor", "|x| 1e-3..1e17", NkFloor, CFloor, 0);
	Cas("ceil", "|x| 1e-3..1e17", NkCeil, CCeil, 0);
	Cas("trunc", "|x| 1e-3..1e17", NkTrunc, CTrunc, 0);
	Cas("round", "|x| 1e-3..1e17", NkRound, CRound, 0);
	Cas("fabs", "|x| 1e-3..1e17", NkFabs, CFabs, 0);

	Remplir(-100.0, 100.0);
	Cas32("sinf", "[-100, 100]", NkSinf, CSinf);
	Cas32("cosf", "[-100, 100]", NkCosf, CCosf);
	Remplir(-87.0, 88.0);
	Cas32("expf", "[-87, 88]", NkExpf, CExpf);
	RemplirLog(1e-37, 1e38, 0);
	Cas32("logf", "1e-37..1e38", NkLogf, CLogf);
	Cas32("sqrtf", "1e-37..1e38", NkSqrtf, CSqrtf);

	printf("fautes : %d\n", gFautes);
	return gFautes == 0 ? 0 : 1;
}
