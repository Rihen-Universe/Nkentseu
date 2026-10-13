// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKAnima/tests/test_lois_de_temps.cpp — LE BANC DES LOIS DE TEMPS
// (Realtime/NkRtEase.h, 13/10/2026).
//
// D'ou il vient : les gestes fabriques du patient virtuel de PV3DE (un bras leve,
// la toux, le saut) suivaient des lois de temps ecrites a la main dans PV3DE. Elles
// sont passees dans NKAnima.
//
// Chaque critere est MESURE (la valeur est imprimee) ; la mutation, posee dans le
// MEME binaire par NK_RT_MUTATION :
//
//   le passage de 0 a 1 est a secousse minimale          ease_linear   (LT1, LT2)
//
//   NK_RT_MUTATION=ease_linear ./Build/Tests/Debug-Windows/NKAnima_Tests.exe
// =============================================================================
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKAnima/NKAnima.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::anim;
using namespace nkentseu::anim::rt;

namespace {

	struct Mutations {
			Mutations() {
				const char *e = std::getenv("NK_RT_MUTATION");
				NkRtSetMutations((e != nullptr && std::strstr(e, "ease_linear") != nullptr) ? (uint32)NK_RT_MUT_EASE_LINEAR : 0u);
			}
			~Mutations() {
				NkRtSetMutations(0u);
			}
	};

} // namespace

// -----------------------------------------------------------------------------
// LT1 — LA SECOUSSE MINIMALE : 0 en 0, 1 en 1, la moitie au milieu, symetrique,
// jamais decroissante ; et surtout VITESSE ET ACCELERATION NULLES AUX DEUX BOUTS :
// a 1 % du depart elle n'a parcouru que 1e-5 (une rampe droite : 1e-2), et sa
// vitesse moyenne sur ce centieme est mille fois sous celle de la rampe.
//                                                         [mutation : ease_linear]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, LT1_La_secousse_minimale) {
	Mutations _m;
	ASSERT_TRUE_MSG(NkRtMinJerk(0.f) == 0.f && NkRtMinJerk(1.f) == 1.f, "0 en 0, 1 en 1");
	ASSERT_TRUE_MSG(NkRtMinJerk(-3.f) == 0.f && NkRtMinJerk(7.f) == 1.f, "u est borne a [0 ; 1]");
	float32 pireSym = 0.f, pireRecul = 0.f, avant = 0.f;
	for (int32 k = 0; k <= 1000; ++k) {
		const float32 u = (float32)k / 1000.f, f = NkRtMinJerk(u);
		pireSym = std::fmax(pireSym, std::fabs(f + NkRtMinJerk(1.f - u) - 1.f));
		pireRecul = std::fmax(pireRecul, avant - f);
		avant = f;
	}
	const float32 h = 0.01f;
	const float32 debut = NkRtMinJerk(h), fin = 1.f - NkRtMinJerk(1.f - h);
	// L'acceleration au depart : la difference seconde f(2h) - 2 f(h) + f(0), sur h^2.
	const float32 acc = (NkRtMinJerk(2.f * h) - 2.f * NkRtMinJerk(h)) / (h * h);
	std::printf("  [LT1] a 1 %% du depart : %.2e parcouru (rampe droite : 1e-2) ; a 1 %% de l'arrivee : %.2e ; acceleration au depart %.3f ; "
				"symetrie a %.1e pres ; recul %.1e\n",
				(double)debut, (double)fin, (double)acc, (double)pireSym, (double)pireRecul);
	ASSERT_TRUE_MSG(std::fabs(NkRtMinJerk(0.5f) - 0.5f) < 1e-6f, "la moitie au milieu");
	ASSERT_TRUE_MSG(pireSym < 1e-5f, "symetrique : f(u) + f(1 - u) = 1");
	ASSERT_TRUE_MSG(pireRecul < 1e-6f, "jamais decroissante (a l'arrondi pres)");
	ASSERT_TRUE_MSG(debut < 1e-4f && fin < 1e-4f, "la vitesse est nulle aux deux bouts (1e-5 parcouru a 1 %, pas 1e-2)");
	ASSERT_TRUE_MSG(std::fabs(acc) < 1.f, "l'acceleration est nulle au depart (60 h : 0,6 a h = 1 %)");
}

// -----------------------------------------------------------------------------
// LT2 — L'ENVELOPPE ET LA FENETRE. Attaque 0,4 s, tenue 1 s, relache 0,6 s : 0 avant,
// 1 pendant la tenue, 0 apres ; aucun saut (au plus 0,005 entre deux instants a
// 1 ms). Sans relache : 0 apres la tenue. La fenetre de 1 a 3 s, ouverte en 0,2 s
// et refermee en 0,5 s : 0 dehors, 1 au milieu, la moitie a mi-bord.
//                                                         [mutation : ease_linear]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, LT2_L_enveloppe_et_la_fenetre) {
	Mutations _m;
	NkRtEnvelope e;
	e.attack = 0.4f;
	e.hold = 1.f;
	e.release = 0.6f;
	float32 pireSaut = 0.f, avant = 0.f, haut = 0.f;
	for (int32 k = -100; k <= 2300; ++k) {
		const float32 w = NkRtEnvelopeWeight(e, (float32)k * 0.001f);
		pireSaut = std::fmax(pireSaut, std::fabs(w - avant));
		haut = std::fmax(haut, w);
		avant = w;
	}
	std::printf("  [LT2] enveloppe 0,4 / 1 / 0,6 s : le plus grand saut entre deux instants a 1 ms est %.4f ; sommet %.3f\n", (double)pireSaut, (double)haut);
	ASSERT_TRUE_MSG(NkRtEnvelopeWeight(e, -0.1f) == 0.f && NkRtEnvelopeWeight(e, 0.f) == 0.f, "0 avant le debut");
	ASSERT_TRUE_MSG(NkRtEnvelopeWeight(e, 0.9f) == 1.f && haut < 1.00001f, "1 pendant la tenue, jamais plus");
	ASSERT_TRUE_MSG(NkRtEnvelopeWeight(e, 2.1f) == 0.f, "0 apres la relache");
	ASSERT_TRUE_MSG(std::fabs(NkRtEnvelopeWeight(e, 0.2f) - 0.5f) < 1e-5f && std::fabs(NkRtEnvelopeWeight(e, 1.7f) - 0.5f) < 1e-4f,
					"la moitie a mi-attaque et a mi-relache");
	ASSERT_TRUE_MSG(pireSaut < 0.006f, "aucun saut aux raccords (la pente la plus forte de l'attaque fait 0,0047 par ms)");
	NkRtEnvelope tenu = e;
	tenu.release = 0.f;
	ASSERT_TRUE_MSG(NkRtEnvelopeWeight(tenu, 1.3f) == 1.f && NkRtEnvelopeWeight(tenu, 1.5f) == 0.f, "sans relache : 0 apres la tenue");
	// La fenetre.
	ASSERT_TRUE_MSG(NkRtWindow(0.9f, 1.f, 3.f, 0.2f, 0.5f) == 0.f && NkRtWindow(3.f, 1.f, 3.f, 0.2f, 0.5f) == 0.f, "la fenetre : 0 dehors");
	ASSERT_TRUE_MSG(NkRtWindow(2.f, 1.f, 3.f, 0.2f, 0.5f) == 1.f, "la fenetre : 1 au milieu");
	const float32 ouvre = NkRtWindow(1.1f, 1.f, 3.f, 0.2f, 0.5f), ferme = NkRtWindow(2.75f, 1.f, 3.f, 0.2f, 0.5f);
	const float32 bord = NkRtWindow(1.002f, 1.f, 3.f, 0.2f, 0.5f);
	std::printf("  [LT2] fenetre : a mi-ouverture %.4f, a mi-fermeture %.4f ; a 1 %% de l'ouverture %.2e (rampe droite : 1e-2)\n", (double)ouvre,
				(double)ferme, (double)bord);
	ASSERT_TRUE_MSG(std::fabs(ouvre - 0.5f) < 1e-3f && std::fabs(ferme - 0.5f) < 1e-3f, "la fenetre : la moitie a mi-bord, ouverture et fermeture");
	ASSERT_TRUE_MSG(bord < 1e-4f, "la fenetre s'ouvre sans a-coup");
}

// -----------------------------------------------------------------------------
// LT3 — HERMITE ET LE VOL. Hermite tient ses deux valeurs et ses deux pentes (lues
// par difference). Le vol : la hauteur demandee est atteinte au sommet, a v0 / g ;
// le corps revient a 0 a 2 v0 / g ; et son acceleration (difference seconde) est
// -g a 0,1 % pres -- la loi que le banc de PV3DE exige du saut du patient.
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, LT3_Hermite_et_le_vol) {
	Mutations _m;
	const float32 p0 = 0.3f, m0 = -1.2f, p1 = 1.1f, m1 = 0.7f, h = 1e-3f;
	const float32 pente0 = (NkRtHermite(h, p0, m0, p1, m1) - NkRtHermite(0.f, p0, m0, p1, m1)) / h;
	const float32 pente1 = (NkRtHermite(1.f, p0, m0, p1, m1) - NkRtHermite(1.f - h, p0, m0, p1, m1)) / h;
	std::printf("  [LT3] Hermite : pentes lues %.3f et %.3f (demandees %.1f et %.1f)\n", (double)pente0, (double)pente1, (double)m0, (double)m1);
	ASSERT_TRUE_MSG(NkRtHermite(0.f, p0, m0, p1, m1) == p0 && std::fabs(NkRtHermite(1.f, p0, m0, p1, m1) - p1) < 1e-6f, "Hermite tient ses deux valeurs");
	ASSERT_TRUE_MSG(std::fabs(pente0 - m0) < 0.02f && std::fabs(pente1 - m1) < 0.02f, "Hermite tient ses deux pentes");
	// Le vol : 25 cm de haut sous 9,81 m/s2.
	const float32 g = 9.81f, hauteur = 0.25f;
	const float32 v0 = NkRtBallisticSpeed(hauteur, g), duree = NkRtBallisticTime(v0, g);
	const float32 sommet = NkRtBallisticHeight(v0, g, v0 / g);
	const float32 dt = 0.02f;
	float64 pire = 0.;
	for (float32 t = dt; t < duree - dt; t += dt) {
		const float64 a = ((float64)NkRtBallisticHeight(v0, g, t + dt) - 2. * (float64)NkRtBallisticHeight(v0, g, t) + (float64)NkRtBallisticHeight(v0, g, t - dt)) /
						  ((float64)dt * (float64)dt);
		pire = std::fmax(pire, std::fabs(a + (float64)g) / (float64)g);
	}
	std::printf("  [LT3] vol de %.2f m : decollage a %.3f m/s, %.3f s en l'air, sommet %.4f m, retour a %.1e m ; acceleration a %.3f %% de -g\n",
				(double)hauteur, (double)v0, (double)duree, (double)sommet, (double)NkRtBallisticHeight(v0, g, duree), pire * 100.);
	ASSERT_TRUE_MSG(std::fabs(sommet - hauteur) < 1e-5f, "le sommet du vol est la hauteur demandee");
	ASSERT_TRUE_MSG(std::fabs(NkRtBallisticHeight(v0, g, duree)) < 1e-5f, "le corps revient a sa hauteur de depart a 2 v0 / g");
	ASSERT_TRUE_MSG(pire < 0.001, "l'acceleration du vol est -g, a 0,1 % pres");
}

// -----------------------------------------------------------------------------
// LT4 — LA SECOUSSE : 0 avant, 1 au bout de la montee, 0 au bout de la retombee,
// le quart a mi-retombee (elle retombe en carre).
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, LT4_La_secousse) {
	Mutations _m;
	const float32 monte = 0.05f, tombe = 0.20f;
	ASSERT_TRUE_MSG(NkRtPulse(-0.1f, monte, tombe) == 0.f && NkRtPulse(0.f, monte, tombe) == 0.f, "0 avant");
	ASSERT_TRUE_MSG(std::fabs(NkRtPulse(0.025f, monte, tombe) - 0.5f) < 1e-5f, "la moitie a mi-montee (une ligne droite)");
	ASSERT_TRUE_MSG(std::fabs(NkRtPulse(monte, monte, tombe) - 1.f) < 1e-6f, "1 au bout de la montee");
	ASSERT_TRUE_MSG(std::fabs(NkRtPulse(monte + 0.5f * tombe, monte, tombe) - 0.25f) < 1e-5f, "le quart a mi-retombee (en carre)");
	ASSERT_TRUE_MSG(NkRtPulse(monte + tombe + 0.01f, monte, tombe) == 0.f, "0 apres la retombee");
}
