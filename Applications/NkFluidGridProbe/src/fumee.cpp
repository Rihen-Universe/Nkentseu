// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// fumee.cpp — LA FUMÉE SANS LE FEU.
//
// CE QUE CE FICHIER N'EST PAS : un solveur. Il n'y a pas une ligne de physique
// ici. Tout ce qui suit APPELLE `NkFluidGrid` telle qu'elle est sur `transit`, et
// le rendu de référence `NkFluidGridRaymarch` tel qu'il est. La mesure du 14/09
// qui a décidé de ce fichier tient en une phrase : **il n'y avait rien à
// éteindre**. Les cinq paramètres de la combustion naissent à zéro
// (NkFluidGrid.h:230, 232, 234, 237, 238), `Combust` rend la main avant de
// toucher quoi que ce soit quand `burnRate <= 0` (NkFluidGrid.cpp:1053-1054), et
// l'émission du rendu est fausse par défaut (NkFluidGridRaymarch.h : `emission`).
// Ce palier est donc un RÉGLAGE ET UNE PREUVE, pas un chantier — dit d'avance.
//
// ── LA CONTRADICTION QU'IL A FALLU TRANCHER, et pourquoi c'est ce choix ──────
// La scène de fumée qui existait déjà (rendu.cpp:119, `ConstruirePanache` avec
// `avecFeu = false`) ne brûle pas — carburant identiquement nul — mais elle fait
// monter son panache PAR LA CHALEUR : elle injecte 400 K/s au-dessus de
// l'ambiante (rendu.cpp:154), et la poussée vaut `beta (T - T_amb) - alpha d`
// (NkFluidGrid.cpp:754). Sa température N'EST donc PAS au repos. Or le critère
// (f3) demande qu'elle le soit, et le critère (f2) demande que le panache monte :
// sur cette scène-là, les deux se contredisent.
//
// La scène d'ici lève la contradiction sans une ligne nouvelle : `buoyancyAlpha`
// (NkFluidGrid.h:104) entre dans la poussée en `- alpha d`, donc un alpha NÉGATIF
// est une poussée proportionnelle à la DENSITÉ — une fumée plus légère que l'air
// — qui ne touche JAMAIS la température. (f3) devient alors un critère qu'on peut
// juger AU BIT : la température ne bouge pas du tout, et son négatif — rallumer
// la combustion — la fait bondir de plusieurs centaines de kelvins. Sur une fumée
// déjà à +800 K, ce négatif se serait noyé dans le bruit de sa propre source.
//
// ⚠️ CE QUE CE CHOIX COÛTE, et il est écrit dans la sortie du programme, pas
// seulement ici : `alpha < 0` est un coefficient DÉCLARÉ — « cette fumée est plus
// légère que l'air » — et non la conséquence d'une température. Sa VALEUR, elle,
// n'est pas choisie à l'œil : elle est DÉRIVÉE de la scène chaude de référence,
// pour que les deux panaches subissent la MÊME poussée à l'équilibre.
//
// ── PRÉ-ENREGISTREMENT — écrit AVANT d'avoir lu le moindre chiffre ───────────
//   (f1) LA MASSE SE CONSERVE quand rien ne la produit ni ne la détruit.
//        Montage : la scène d'ici, dissipation COUPÉE, source tirée UNE SEULE
//        fois, parois closes, advection conservative en flux.
//        Attendu : dérive relative < 1,0e-05 — le seuil n'est pas inventé, c'est
//        `kSeuilDerive` du témoin (f1) déjà en place (advection_flux.cpp).
//        GARDE : la masse doit avoir BOUGÉ (variation L1 > 5 % de la masse), sans
//        quoi « elle se conserve » serait vrai d'un champ qui ne transporte rien.
//        NÉGATIF : la même course avec la source ACTIVE à chaque pas -> la masse
//        AUGMENTE, et de ce que la source injecte, mesuré séparément.
//
//   (f2) LE PANACHE MONTE. Mesure : le barycentre de DENSITÉ (et pas celui de
//        température : sur une fumée froide il ne mesurerait rien).
//        Attendu : montée > 10 cellules. Borne volontairement BASSE — on juge un
//        SIGNE et un ordre de grandeur, pas une trajectoire : même une vitesse
//        terminale aussi faible que 0,05 m/s ferait 0,21 m en 4,25 s, soit 10 h.
//        NÉGATIF : `buoyancyEnabled = false` (NkFluidGrid.h:226) -> il ne monte
//        pas (montée < 1 cellule).
//
//   (f3) ÇA NE BRÛLE PAS. Mesure, à CHAQUE pas : l'écart maximal de température à
//        l'ambiante, la chaleur totale, le carburant total.
//        Attendu (f3a) : carburant identiquement NUL, au bit.
//        Attendu (f3b) : l'écart de température reste sous son majorant.
//        ⚠️ LE PREMIER MAJORANT ÉCRIT ÉTAIT LE MAUVAIS, et le dire vaut mieux que
//        le corriger en douce. J'avais écrit N * eps_f32 * T_amb = 9,1e-03 K —
//        l'arrondi. Il ne vaut que pour le SEMI-LAGRANGIEN, qui INTERPOLE et rend
//        un champ uniforme exact. Le schéma en FLUX, lui, transporte la
//        température comme une quantité CONSERVÉE : il ajoute -T dt div(u), donc
//        il dérive du RÉSIDU DE PROJECTION, qui n'est pas nul — la projection
//        s'arrête à une tolérance. Le majorant juste est donc
//        T_amb * dt * N * (|div|h)_max / h, dérivé de la divergence MESURÉE dans
//        la course. Les deux sont publiés côte à côte : sur la mesure du 14/09,
//        l'écart vaut 4,977e-02 K, le majorant par l'arrondi 9,120e-03 K (il
//        aurait crié ROUGE sur un montage correct) et celui par la divergence
//        2,160e-01 K. Un attendu dérivé du mauvais mécanisme est un faux rouge en
//        attente.
//        NÉGATIF : la MÊME scène, combustion rallumée (le réglage chiffré de
//        rendu.cpp:140-147) -> la température monte de plusieurs centaines de K.
//        Le verdict publié est le RAPPORT des deux : il doit valoir >= 1 000.
//
//   (f4) ÇA SE VOIT. Une suite d'images hors-écran, et LE COMPTEUR DE PIXELS
//        PROUVE SON ZÉRO D'ABORD : grille vide -> exactement 0 pixel différent du
//        fond sur les 480 x 360, sur une image dont les rayons COUPENT la boîte
//        (un zéro de caméra qui regarde ailleurs n'est pas un zéro). Ensuite
//        seulement on compte.
//        Attendu : le nombre de pixels de fumée croît, et la rangée BARYCENTRE du
//        panache monte. C'est (f2) reprouvé par un instrument qui ne sait rien de
//        la grille.
//        ⚠️ CE N'EST PAS LA RANGÉE LA PLUS HAUTE QUI JUGE, et c'est la mesure qui
//        l'a imposé : dès la 2e image, le panache touche le bord supérieur du
//        cadre, cette rangée vaut 0 et ne peut plus décroître. « Elle décroît »
//        serait alors vrai par SATURATION. Le nombre d'images saturées est publié.
//        NÉGATIF : le zéro ci-dessus, plus l'image de la course sans poussée, où
//        la rangée barycentre ne doit PAS monter.
//
// LE PRIX est mesuré et ÉCRIT DANS LA SORTIE : millisecondes de simulation et de
// marche de rayon par image, pour cette fumée ET pour le feu de la même grille —
// le feu étant chronométré ici même, en appelant `ConstruirePanache(g, true, N)`,
// et non recopié. Une copie de scène qui dérive est le piège que (n3c) surveille
// déjà dans ce banc ; on ne le tend pas une seconde fois.
// =============================================================================
#include "NKRenderer/Tools/VFX/NkFluidGridRaymarch.h"

#include <cstdio>

// En DERNIER, comme dans NkFluidGrid.cpp : ce fichier porte un namespace `time`
// qui entre en conflit s'il arrive avant les autres.
#include "NKTime/NkChrono.h"

using namespace nkentseu;
using namespace nkentseu::renderer;
using namespace nkentseu::math;

// Partagés avec le reste du banc — un seul compteur de rouges, un seul epsilon.
void ProbeCheck(bool ok, const char *nom, const char *detail);
float32 ProbeAbs(float32 v);
bool EcrirePng(const NkVector<uint8> &rgba, uint32 W, uint32 H, const char *chemin);
void ConstruirePanache(NkFluidGrid &g, bool avecFeu, uint32 pas, float32 epsilon);
float32 EpsilonConfinement();

// =============================================================================
// LES CONSTANTES DE LA SCÈNE — chacune avec sa provenance. Aucune n'est libre.
// =============================================================================
static const float32 kDt = 1.f / 60.f;
static const uint32 kPas = 255;			  // le même nombre de pas que rendu.cpp:230
static const float32 kDebitDensite = 7.f; // densité/s — rendu.cpp:154
static const float32 kDissipation = 0.5f; // 1/s — rendu.cpp:134
static const float32 kTAmbiante = 300.f;  // NkFluidGrid.h:74 (défaut)
static const float32 kGravite = 9.81f;	  // NkFluidGrid.h:75 (défaut)
// Les deux chiffres de la scène CHAUDE de référence, dont on dérive alpha.
static const float32 kAlphaChaud = 0.25f;	// rendu.cpp:136
static const float32 kDeltaTChaud = 400.f;	// K/s — rendu.cpp:154
// Les seuils, tous dérivés ci-dessus, réunis ici pour qu'on les lise d'un coup.
static const float32 kSeuilDeriveMasse = 1.0e-05f; // le seuil de (f1) existant
static const float32 kSeuilBougeL1 = 0.05f;		   // la garde (f1b) existante
static const float32 kMonteeMin = 10.f;			   // en cellules
static const float32 kMonteeNegMax = 1.f;		   // en cellules
static const float32 kEpsilonF32 = 1.192092896e-07f;
static const float32 kSeuilRapportFeu = 1000.f;
static const uint32 kSeuilPixel = 8; // le même seuil d'opacité que rendu.cpp

// La densité d'ÉQUILIBRE dans la source : le débit divisé par la dissipation.
// ⚠️ C'est un MAJORANT, pas la densité réelle : l'advection en emporte aussi.
// On le dit, parce qu'une poussée dérivée d'un majorant est elle-même un
// majorant, et qu'un attendu dérivé d'un majorant doit être lu comme tel.
static float32 DensiteEquilibre() {
	return kDebitDensite / kDissipation;
}

// La poussée de la scène CHAUDE à l'équilibre, avec la loi qu'applique le
// solveur : beta = g / T_amb (Boussinesq, NkFluidGrid.cpp:89-94), et la poussée
// beta (T - T_amb) - alpha d (NkFluidGrid.cpp:754).
static float32 PousseeReference() {
	const float32 beta = kGravite / kTAmbiante;
	return beta * (kDeltaTChaud / kDissipation) - kAlphaChaud * DensiteEquilibre();
}

// L'alpha de la fumée FROIDE : NÉGATIF, et dérivé pour que la poussée à
// l'équilibre soit EXACTEMENT celle de la scène chaude. Les deux panaches sont
// alors comparables — sinon on comparerait deux physiques ET deux réglages.
static float32 AlphaFroid() {
	return -PousseeReference() / DensiteEquilibre();
}

// =============================================================================
// LA SCÈNE. Les cinq paramètres de combustion ne sont PAS écrits : ils restent à
// leur valeur de naissance, zéro. C'est le fond de ce palier — ne rien écrire
// EST le réglage.
// =============================================================================
static NkFluidGridParams SceneFumeeFroide(bool avecDissipation) {
	NkFluidGridParams p;
	p.boundsMin = {-0.25f, 0.f, -0.25f};
	p.boundsMax = {0.25f, 1.6f, 0.25f}; // la boîte HAUTE de rendu.cpp:129
	p.cellSize = 0.02f;					// 25 x 80 x 25
	p.densityDissipation = avecDissipation ? kDissipation : 0.f;
	p.temperatureDissipation = 0.f; // rien à dissiper : T est déjà l'ambiante
	p.buoyancyAlpha = AlphaFroid(); // NÉGATIF — la fumée est plus légère que l'air
	p.pressureTolerance = 1.0e-4f;
	p.vorticityConfinement = EpsilonConfinement(); // UNE seule valeur dans le banc
	// LE schéma qui ferme le bilan de masse : (f1) ne veut rien dire sans lui.
	p.advectFluxConservative = true;
	return p;
}

// La source : de la densité, et RIEN d'autre. Température AJOUTÉE = 0 exactement
// (EmitSphere est additif, NkFluidGrid.cpp:118-120), carburant = 0.
static void EmettreFumee(NkFluidGrid &g) {
	g.EmitSphere({0.f, 0.05f, 0.f}, 0.06f, kDebitDensite * kDt, 0.f, 0.f);
}

// =============================================================================
// UNE COURSE, et tout ce qu'elle relève. Les maxima sont pris À CHAQUE PAS, pas
// à la fin : une température qui monte puis redescend serait invisible autrement.
// =============================================================================
struct CourseFumee {
		float32 masse0 = 0.f, masse1 = 0.f;
		float32 centroidY0 = 0.f, centroidY1 = 0.f;
		bool centroid0Valide = false, centroid1Valide = false;
		float32 ecartTMax = 0.f;   // max sur la course de |T_max - T_ambiante|
		float32 chaleurMax = 0.f;  // max de |somme (T - T_amb) h^3|
		float32 carburantMax = 0.f;
		float32 msSolveur = 0.f;   // somme des Stats().ms — le SOLVEUR seul
		float32 msSolveurMax = 0.f;
		// LE MINIMUM, et ce n'est pas une coquetterie : voir le bloc du PRIX. Sur
		// une machine partagée, la MOYENNE mesure la charge des autres ; le pas le
		// moins gêné est la meilleure approche par le bas du coût vrai.
		float32 msSolveurMin = 1.0e30f;
		float32 msMur = 0.f;	   // le temps RÉEL de la boucle, source comprise
		uint32 nan = 0;
		uint32 speedClamped = 0;
		float32 vitesseMax = 0.f;
		float32 cflMax = 0.f;
		uint32 sousPasMax = 0;
		bool capHit = false;
		// LA DIVERGENCE RÉSIDUELLE, et elle n'est pas là pour décorer : c'est ELLE
		// qui borne la dérive de température du schéma en FLUX (voir (f3)).
		float32 divMax = 0.f; // max de |div|*h moyen sur l'intérieur STRICT (m/s)
		uint32 pas = 0;
};

// `sourceContinue` : la source est tirée à chaque pas (la scène) ou une seule
// fois avant le premier pas (le montage de conservation).
static void Courir(NkFluidGrid &g, const NkFluidGridParams &p, bool sourceContinue, uint32 pas, CourseFumee &r) {
	r = CourseFumee();
	r.pas = pas;
	if (!g.Init(p)) {
		ProbeCheck(false, "(fumee) Init de la grille", "Init a rendu false");
		return;
	}
	EmettreFumee(g);
	r.masse0 = g.TotalMass();
	NkVec3f c0;
	r.centroid0Valide = g.DensityCentroid(c0);
	r.centroidY0 = r.centroid0Valide ? c0.y : 0.f;

	const int64 t0 = ::nkentseu::NkChrono::Now().nanoseconds;
	for (uint32 s = 0; s < pas; ++s) {
		if (sourceContinue && s > 0)
			EmettreFumee(g);
		g.Step(kDt);
		const NkFluidGridStats &st = g.Stats();
		r.msSolveur += st.ms;
		if (st.ms > r.msSolveurMax)
			r.msSolveurMax = st.ms;
		if (st.ms < r.msSolveurMin)
			r.msSolveurMin = st.ms;
		r.nan += st.nanCount;
		r.speedClamped += st.speedClamped;
		if (st.maxSpeed > r.vitesseMax)
			r.vitesseMax = st.maxSpeed;
		if (st.advectCFL > r.cflMax)
			r.cflMax = st.advectCFL;
		if (st.advectSubsteps > r.sousPasMax)
			r.sousPasMax = st.advectSubsteps;
		if (st.advectSubstepCapHit)
			r.capHit = true;
		if (st.divAfterMeanStrict > r.divMax)
			r.divMax = st.divAfterMeanStrict;
		// (f3) — relevé À CHAQUE PAS.
		const float32 ecart = ProbeAbs(st.maxTemperature - p.ambientTemperature);
		if (ecart > r.ecartTMax)
			r.ecartTMax = ecart;
		const float32 chaleur = ProbeAbs(g.TotalHeat());
		if (chaleur > r.chaleurMax)
			r.chaleurMax = chaleur;
		const float32 carb = ProbeAbs(g.TotalFuel());
		if (carb > r.carburantMax)
			r.carburantMax = carb;
	}
	r.msMur = (float32)((::nkentseu::NkChrono::Now().nanoseconds - t0) / 1.0e6);

	r.masse1 = g.TotalMass();
	NkVec3f c1;
	r.centroid1Valide = g.DensityCentroid(c1);
	r.centroidY1 = r.centroid1Valide ? c1.y : 0.f;
}

// =============================================================================
// LE COMPTEUR DE PIXELS — le mien, et il prouve son zéro avant de compter quoi
// que ce soit d'autre. (Une sonde a rendu 40 pixels sans aucune cible le 13/09 ;
// celui-ci est né avec sa contre-épreuve.)
// L'opacité est la distance de Manhattan au fond, 0-765 — la même définition que
// rendu.cpp, pour que les deux instruments soient comparables.
// =============================================================================
static uint32 OpacitePixel(const NkVector<uint8> &rgba, uint32 W, uint32 x, uint32 y, const uint8 bg[3]) {
	const nk_size o = ((nk_size)y * (nk_size)W + (nk_size)x) * 4u;
	const int32 dr = (int32)rgba[o + 0] - (int32)bg[0];
	const int32 dg = (int32)rgba[o + 1] - (int32)bg[1];
	const int32 db = (int32)rgba[o + 2] - (int32)bg[2];
	return (uint32)((dr < 0 ? -dr : dr) + (dg < 0 ? -dg : dg) + (db < 0 ? -db : db));
}

// Rend le nombre de pixels au-dessus du seuil, la rangée la PLUS HAUTE qu'ils
// occupent, et la rangée BARYCENTRE pondérée par l'opacité (y = 0 est le haut de
// l'image : une rangée qui DÉCROÎT est un panache qui MONTE).
// `hautOut` vaut H et `ligneOut` vaut 0 si aucun pixel ne passe le seuil.
//
// ⚠️ POURQUOI DEUX RANGÉES ET PAS UNE. La rangée la plus haute SATURE : dès que le
// panache atteint le bord supérieur de l'image, elle vaut 0 et ne peut plus
// décroître — un critère « elle décroît » deviendrait alors vrai par saturation,
// c'est-à-dire pour la mauvaise raison. Mesuré ici même : à partir de la 2e image
// de la suite, le panache touche le haut du cadre. La rangée BARYCENTRE, elle, ne
// sature pas tant qu'il reste de la fumée en bas, et c'est donc ELLE qui juge.
static uint32 PixelsDeFumee(const NkVector<uint8> &rgba, uint32 W, uint32 H, const uint8 bg[3], uint32 seuil,
							uint32 &hautOut, float32 &ligneOut) {
	uint32 n = 0;
	hautOut = H;
	ligneOut = 0.f;
	float64 poids = 0.0, somme = 0.0;
	for (uint32 y = 0; y < H; ++y)
		for (uint32 x = 0; x < W; ++x) {
			const uint32 o = OpacitePixel(rgba, W, x, y, bg);
			if (o <= seuil)
				continue;
			++n;
			if (y < hautOut)
				hautOut = y;
			poids += (float64)o;
			somme += (float64)o * (float64)y;
		}
	if (poids > 0.0)
		ligneOut = (float32)(somme / poids);
	return n;
}

static NkFluidRaymarchParams CameraDuBanc() {
	NkFluidRaymarchParams rp;
	rp.width = 480;
	rp.height = 360;
	rp.cameraPos = {0.f, 0.38f, 2.10f};
	rp.cameraTarget = {0.f, 0.30f, 0.f};
	rp.fovDegrees = 40.f;
	rp.shadowMaxDistance = 0.35f;
	// `emission` reste FAUSSE : c'est le second interrupteur du feu, celui du
	// rendu (NkFluidGridRaymarch.h). Ne pas l'écrire EST le réglage, ici aussi.
	return rp;
}

// =============================================================================
// LE PALIER
// =============================================================================
void PalierFumee() {
	char buf[640];
	printf("\n=============================================================\n");
	printf("=== LA FUMÉE SANS LE FEU — (f1) masse, (f2) montée, (f3) pas de\n");
	printf("=== combustion, (f4) ça se voit. Chacun avec son NÉGATIF.\n");
	printf("=============================================================\n");

	// ── CE QUI N'EST PAS ÉCRIT, ET QUI EST LE FOND DE CE PALIER ──────────────
	{
		NkFluidGridParams p = SceneFumeeFroide(true);
		snprintf(buf, sizeof(buf),
				 "burnRate %.1f, heatPerFuel %.1f, sootPerFuel %.1f, coolingRate %.1f, fuelDissipation %.1f — "
				 "AUCUN de ces cinq n'est écrit par la scène : ils naissent à zéro (NkFluidGrid.h:230-238) et "
				 "Combust rend la main avant de toucher quoi que ce soit (NkFluidGrid.cpp:1053-1054)",
				 (double)p.burnRate, (double)p.heatPerFuel, (double)p.sootPerFuel, (double)p.coolingRate,
				 (double)p.fuelDissipation);
		ProbeCheck(p.burnRate == 0.f && p.heatPerFuel == 0.f && p.sootPerFuel == 0.f && p.coolingRate == 0.f &&
					   p.fuelDissipation == 0.f,
				   "(f0) la combustion est ÉTEINTE — et elle l'était déjà", buf);

		snprintf(buf, sizeof(buf),
				 "poussée de la scène CHAUDE de référence à l'équilibre : beta (dT) - alpha d = "
				 "(%.4f x %.1f) - (%.2f x %.1f) = %.3f m/s^2 ; densité d'équilibre (MAJORANT : l'advection en "
				 "emporte aussi) %.1f ; d'où alpha FROID = -%.3f / %.1f = %.4f — DÉRIVÉ, jamais posé à l'œil. "
				 "⚠️ ET C'EST UN COEFFICIENT DÉCLARÉ : « cette fumée est plus légère que l'air », pas la "
				 "conséquence d'une température",
				 (double)(kGravite / kTAmbiante), (double)(kDeltaTChaud / kDissipation), (double)kAlphaChaud,
				 (double)DensiteEquilibre(), (double)PousseeReference(), (double)DensiteEquilibre(),
				 (double)PousseeReference(), (double)DensiteEquilibre(), (double)AlphaFroid());
		ProbeCheck(AlphaFroid() < 0.f, "(f0b) la poussée de la fumée froide est DÉRIVÉE de la scène chaude", buf);
	}

	// =========================================================================
	// (f4) D'ABORD LE ZÉRO DU COMPTEUR — avant tout le reste, comme exigé.
	// =========================================================================
	NkFluidRaymarchParams rp = CameraDuBanc();
	const uint8 bg[3] = {(uint8)(rp.background.x * 255.f + 0.5f), (uint8)(rp.background.y * 255.f + 0.5f),
						 (uint8)(rp.background.z * 255.f + 0.5f)};
	{
		NkFluidGrid vide;
		vide.Init(SceneFumeeFroide(true)); // AUCUNE émission : la grille reste nue
		NkVector<uint8> img;
		NkFluidRaymarchStats st;
		NkFluidRaymarchRender(vide, rp, img, st);
		uint32 haut = 0;
		float32 ligne = 0.f;
		const uint32 n = PixelsDeFumee(img, rp.width, rp.height, bg, kSeuilPixel, haut, ligne);
		snprintf(buf, sizeof(buf),
				 "%u pixel(s) au-dessus du seuil %u/765 sur %u ; %u rayons dont %u coupent la boîte — la boîte "
				 "EST traversée, donc ce zéro n'est pas celui d'une caméra qui regarde ailleurs",
				 n, kSeuilPixel, rp.width * rp.height, st.rays, st.raysHit);
		ProbeCheck(n == 0 && st.raysHit > 0, "(f4.0) LE ZÉRO DU COMPTEUR DE PIXELS, prouvé avant tout le reste", buf);
	}

	// =========================================================================
	// (f1) LA MASSE SE CONSERVE — source tirée UNE fois, dissipation coupée.
	// =========================================================================
	{
		NkFluidGridParams p = SceneFumeeFroide(false); // densityDissipation = 0
		NkFluidGrid g;
		// Le champ initial est gardé pour la garde (f1b) : sans lui, « la masse se
		// conserve » serait vrai d'un champ qui ne bouge pas.
		if (!g.Init(p)) {
			ProbeCheck(false, "(f1) Init de la grille", "Init a rendu false");
			return;
		}
		EmettreFumee(g);
		const uint32 total = g.Stats().cellsTotal;
		NkVector<float32> dInit;
		dInit.Resize(total, 0.f);
		{
			const float32 *d0 = g.Density();
			for (uint32 i = 0; i < total; ++i)
				dInit[i] = d0[i];
		}
		const float32 masse0 = g.TotalMass();
		for (uint32 s = 0; s < kPas; ++s)
			g.Step(kDt);
		const float32 masse1 = g.TotalMass();
		const float32 derive = (masse0 > 0.f) ? ProbeAbs(masse1 - masse0) / masse0 : 1.f;

		// GARDE (f1b) : la masse a-t-elle seulement CHANGÉ DE PLACE ?
		float64 l1 = 0.0;
		{
			const float32 *dfin = g.Density();
			for (uint32 k = 1; k <= g.Nz(); ++k)
				for (uint32 j = 1; j <= g.Ny(); ++j)
					for (uint32 i = 1; i <= g.Nx(); ++i) {
						const uint32 id = g.Idx(i, j, k);
						l1 += (float64)ProbeAbs(dfin[id] - dInit[id]);
					}
		}
		const float32 h3 = g.CellSize() * g.CellSize() * g.CellSize();
		const float32 bouge = (masse0 > 0.f) ? (float32)(l1 * (float64)h3) / masse0 : 0.f;
		snprintf(buf, sizeof(buf),
				 "variation L1 du champ = %.2f %% de la masse initiale (exigé > %.0f %%) — sans cette garde, "
				 "(f1) serait vert pour la pire des raisons : un champ qui ne transporte rien",
				 (double)(bouge * 100.f), (double)(kSeuilBougeL1 * 100.f));
		ProbeCheck(bouge > kSeuilBougeL1, "(f1b) GARDE : la masse a réellement été TRANSPORTÉE", buf);

		snprintf(buf, sizeof(buf),
				 "masse %.9f -> %.9f, dérive relative %.3e sur %u pas (seuil %.0e, celui du témoin (f1) déjà en "
				 "place) ; parois closes, dissipation coupée, source tirée UNE seule fois ; masse restée "
				 "collée aux parois %.3e",
				 (double)masse0, (double)masse1, (double)derive, kPas, (double)kSeuilDeriveMasse,
				 (double)g.WallLayerMass());
		ProbeCheck(derive < kSeuilDeriveMasse, "(f1) LA DENSITÉ SE CONSERVE", buf);

		// ── NÉGATIF de (f1) : la source ACTIVE -> la masse augmente de ce qu'elle
		// injecte. Le montant injecté n'est pas déduit : il est MESURÉ, sur une
		// grille neuve où l'on ne tire la source qu'une fois.
		NkFluidGrid mesureSource;
		mesureSource.Init(p);
		EmettreFumee(mesureSource);
		const float32 parTir = mesureSource.TotalMass();

		NkFluidGrid gs;
		gs.Init(p);
		EmettreFumee(gs);
		const float32 masseS0 = gs.TotalMass();
		for (uint32 s = 0; s < kPas; ++s) {
			if (s > 0)
				EmettreFumee(gs);
			gs.Step(kDt);
		}
		const float32 masseS1 = gs.TotalMass();
		const float32 attendu = masseS0 + (float32)(kPas - 1) * parTir;
		const float32 ecart = (attendu > 0.f) ? ProbeAbs(masseS1 - attendu) / attendu : 1.f;
		snprintf(buf, sizeof(buf),
				 "masse %.9f -> %.9f ; la source injecte %.9f par tir et a été tirée %u fois, donc attendu "
				 "%.9f : écart relatif %.3e (seuil %.0e). La masse a bien AUGMENTÉ, et de la quantité exacte "
				 "que la source a mise — ni le schéma ni les parois n'en ont pris",
				 (double)masseS0, (double)masseS1, (double)parTir, kPas, (double)attendu, (double)ecart,
				 (double)kSeuilDeriveMasse);
		ProbeCheck(masseS1 > masse1 && ecart < kSeuilDeriveMasse,
				   "(f1-) NÉGATIF : source active -> la masse AUGMENTE de ce qu'elle injecte", buf);
	}

	// =========================================================================
	// (f2) + (f3) + (f4) — LA SCÈNE, une seule course, et son prix.
	// =========================================================================
	NkFluidGridParams p = SceneFumeeFroide(true);
	NkFluidGrid g;
	CourseFumee r;
	Courir(g, p, true, kPas, r);
	const float32 h = p.cellSize;
	const float32 monteeCellules = (r.centroidY1 - r.centroidY0) / h;

	snprintf(buf, sizeof(buf),
			 "barycentre de DENSITÉ : y = %.4f m -> %.4f m, soit %+.2f cellules en %u pas (%.2f s) — exigé "
			 "> %.0f. Poussée dérivée %.3f m/s^2, vitesse max vue %.2f m/s. (Le barycentre de TEMPÉRATURE, "
			 "lui, ne mesurerait rien ici : la fumée est à l'ambiante)",
			 (double)r.centroidY0, (double)r.centroidY1, (double)monteeCellules, kPas, (double)(kPas * kDt),
			 (double)kMonteeMin, (double)PousseeReference(), (double)r.vitesseMax);
	ProbeCheck(r.centroid0Valide && r.centroid1Valide && monteeCellules > kMonteeMin, "(f2) LE PANACHE MONTE", buf);

	// ── (f3) : rien n'a brûlé. DEUX volets, et le second a un attendu qu'il faut
	// dériver du BON mécanisme — c'est le piège de ce critère.
	//
	// ⚠️ LE MAJORANT PAR L'ARRONDI NE S'APPLIQUE PAS ICI, et le dire est la moitié
	// du critère. Le semi-lagrangien interpole : un champ UNIFORME lui ressort
	// exact, et sa seule dérive possible est l'arrondi, N x eps_f32 x T_amb.
	// Le schéma en FLUX, lui, transporte la température comme une quantité
	// CONSERVÉE : sur une cellule, il ajoute -T dt div(u). Si la vitesse n'est pas
	// à divergence RIGOUREUSEMENT nulle — et elle ne l'est pas, la projection
	// s'arrête à une tolérance — alors la température dérive de ce résidu-là, et
	// de rien d'autre. L'attendu se dérive donc de la divergence MESURÉE dans
	// CETTE course, pas d'un nombre écrit à la main :
	//     dT <= T_amb * dt * N * (|div|*h)_max / h
	// Les deux majorants sont publiés côte à côte pour qu'on voie lequel mord.
	const float32 seuilArrondi = (float32)kPas * kEpsilonF32 * kTAmbiante;
	const float32 seuilDivergence = kTAmbiante * kDt * (float32)kPas * r.divMax / h;
	snprintf(buf, sizeof(buf),
			 "carburant total maximal %.3e sur les %u pas — aucun n'a JAMAIS été injecté, et le solveur ne "
			 "l'advecte même pas tant que burnRate vaut 0 (NkFluidGrid.cpp:1373) ; chaleur totale maximale "
			 "%.3e ; %u NaN, %u vitesses bornées, CFL max %.3f, %u sous-pas%s",
			 (double)r.carburantMax, kPas, (double)r.chaleurMax, r.nan, r.speedClamped, (double)r.cflMax,
			 r.sousPasMax, r.capHit ? " (BORNE ATTEINTE)" : "");
	ProbeCheck(r.carburantMax == 0.f && r.nan == 0, "(f3a) ÇA NE BRÛLE PAS : le carburant reste NUL, au bit", buf);

	snprintf(buf, sizeof(buf),
			 "écart maximal de la température à l'ambiante : %.3e K. Majorant par la DIVERGENCE résiduelle "
			 "(T dt N |div|h / h, avec |div|h max mesuré %.3e m/s) = %.3e K — c'est LUI qui borne un schéma en "
			 "FLUX. Le majorant par l'ARRONDI (%.3e K) ne vaudrait que pour le semi-lagrangien, qui interpole "
			 "et rend un champ uniforme exact : le dire évite de crier rouge sur un montage correct",
			 (double)r.ecartTMax, (double)r.divMax, (double)seuilDivergence, (double)seuilArrondi);
	ProbeCheck(r.ecartTMax <= seuilDivergence, "(f3b) la température ne monte pas : elle DÉRIVE, et dans sa borne",
			   buf);

	// =========================================================================
	// LES NÉGATIFS de (f2) et (f3) — deux courses, une mutation chacune.
	// =========================================================================
	// (f2-) poussée coupée : le barycentre ne doit pas monter.
	NkVector<uint8> imgNeg;
	uint32 hautNeg = 0, pixelsNeg = 0;
	float32 ligneNeg = 0.f;
	{
		NkFluidGridParams q = p;
		q.buoyancyEnabled = false; // NkFluidGrid.h:226 — l'interrupteur de mutation
		NkFluidGrid gn;
		CourseFumee rn;
		Courir(gn, q, true, kPas, rn);
		const float32 monteeNeg = (rn.centroidY1 - rn.centroidY0) / h;
		snprintf(buf, sizeof(buf),
				 "buoyancyEnabled = false : barycentre y = %.4f -> %.4f m, soit %+.2f cellules (exigé < %.0f en "
				 "valeur absolue). La MÊME scène, le MÊME nombre de pas, la MÊME source : seule la poussée est "
				 "coupée — et la montée de %+.2f cellules de (f2) tombe à %+.2f",
				 (double)rn.centroidY0, (double)rn.centroidY1, (double)monteeNeg, (double)kMonteeNegMax,
				 (double)monteeCellules, (double)monteeNeg);
		ProbeCheck(ProbeAbs(monteeNeg) < kMonteeNegMax, "(f2-) NÉGATIF : sans poussée, le panache ne monte pas", buf);
		NkFluidRaymarchStats stn;
		NkFluidRaymarchRender(gn, rp, imgNeg, stn);
		EcrirePng(imgNeg, rp.width, rp.height, "Captures/fumee_sans_poussee_2026-09-14.png");
		pixelsNeg = PixelsDeFumee(imgNeg, rp.width, rp.height, bg, kSeuilPixel, hautNeg, ligneNeg);
	}

	// (f3-) combustion RALLUMÉE : la température doit bondir. Le réglage chiffré
	// est celui de rendu.cpp:140-147, pas un réglage inventé pour l'occasion.
	float32 ecartTFeu = 0.f;
	{
		NkFluidGridParams q = p;
		q.burnRate = 9.f;
		q.heatPerFuel = 900.f;
		q.sootPerFuel = 1.0f;
		q.coolingRate = 2.5f;
		q.temperatureDissipation = 0.f;
		NkFluidGrid gf;
		gf.Init(q);
		// La source du FEU : du carburant, comme rendu.cpp:151.
		for (uint32 s = 0; s < kPas; ++s) {
			gf.EmitSphere({0.f, 0.07f, 0.f}, 0.07f, 0.05f * kDt, 0.f, 5.f * kDt);
			gf.Step(kDt);
			const float32 e = ProbeAbs(gf.Stats().maxTemperature - q.ambientTemperature);
			if (e > ecartTFeu)
				ecartTFeu = e;
		}
		const float32 rapport = (r.ecartTMax > 0.f) ? (ecartTFeu / r.ecartTMax) : 1.0e30f;
		snprintf(buf, sizeof(buf),
				 "combustion rallumée sur la MÊME scène (burnRate %.0f, heatPerFuel %.0f) : la température "
				 "monte de %.1f K au-dessus de l'ambiante, contre %.3e K sans elle — rapport %.3e (exigé "
				 ">= %.0e). C'est CE rapport qui sépare la fumée du feu ; sur une fumée déjà chauffée à "
				 "+800 K, il n'aurait rien séparé du tout",
				 (double)q.burnRate, (double)q.heatPerFuel, (double)ecartTFeu, (double)r.ecartTMax, (double)rapport,
				 (double)kSeuilRapportFeu);
		ProbeCheck(ecartTFeu > 100.f && rapport >= kSeuilRapportFeu,
				   "(f3-) NÉGATIF : combustion rallumée -> la température MONTE", buf);
	}

	// =========================================================================
	// (f4) LA SUITE D'IMAGES — et le second instrument qui reprouve (f2).
	// =========================================================================
	{
		const uint32 kImages = 5;
		uint32 pixels[kImages] = {0, 0, 0, 0, 0};
		uint32 hauts[kImages] = {0, 0, 0, 0, 0};
		float32 lignes[kImages] = {0.f, 0.f, 0.f, 0.f, 0.f};
		float32 msRendu[kImages] = {0.f, 0.f, 0.f, 0.f, 0.f};
		NkFluidGrid gi;
		gi.Init(p);
		NkVector<uint8> img;
		NkFluidRaymarchStats st;
		uint32 faite = 0;
		const char *noms[kImages] = {
			"Captures/fumee_froide_01_2026-09-14.png", "Captures/fumee_froide_02_2026-09-14.png",
			"Captures/fumee_froide_03_2026-09-14.png", "Captures/fumee_froide_04_2026-09-14.png",
			"Captures/fumee_froide_05_2026-09-14.png"};
		for (uint32 s = 0; s < kPas; ++s) {
			gi.EmitSphere({0.f, 0.05f, 0.f}, 0.06f, kDebitDensite * kDt, 0.f, 0.f);
			gi.Step(kDt);
			const uint32 jalon = (kPas / kImages) * (faite + 1u);
			if (faite < kImages && (s + 1u) >= jalon) {
				NkFluidRaymarchRender(gi, rp, img, st);
				EcrirePng(img, rp.width, rp.height, noms[faite]);
				pixels[faite] = PixelsDeFumee(img, rp.width, rp.height, bg, kSeuilPixel, hauts[faite], lignes[faite]);
				msRendu[faite] = st.ms;
				++faite;
			}
		}

		printf("    la suite : ");
		for (uint32 i = 0; i < faite; ++i)
			printf("%u px (haut y=%u, barycentre y=%.1f, %.0f ms)%s", pixels[i], hauts[i], (double)lignes[i],
				   (double)msRendu[i], (i + 1 == faite) ? "\n" : " -> ");

		bool croit = true, monte = true;
		uint32 satures = 0;
		for (uint32 i = 1; i < faite; ++i) {
			if (pixels[i] <= pixels[i - 1])
				croit = false;
			// C'est la rangée BARYCENTRE qui juge : elle ne sature pas.
			if (lignes[i] >= lignes[i - 1])
				monte = false;
		}
		for (uint32 i = 0; i < faite; ++i)
			if (hauts[i] == 0)
				++satures;
		snprintf(buf, sizeof(buf),
				 "%u images 480 x 360 écrites dans Captures/ ; la fumée passe de %u à %u pixels et sa rangée "
				 "BARYCENTRE monte de %.1f à %.1f (y croît vers le bas). ⚠️ La rangée la plus HAUTE, elle, est "
				 "SATURÉE à 0 sur %u des %u images — le panache sort du cadre par le haut, et c'est pourquoi "
				 "elle ne juge pas. Cet instrument ne sait RIEN de la grille : il reprouve (f2) par un autre "
				 "chemin, et son zéro est (f4.0)",
				 faite, faite > 0 ? pixels[0] : 0u, faite > 0 ? pixels[faite - 1] : 0u, faite > 0 ? (double)lignes[0] : 0.0,
				 faite > 0 ? (double)lignes[faite - 1] : 0.0, satures, faite);
		ProbeCheck(faite == kImages && pixels[0] > 0 && croit && monte, "(f4) ÇA SE VOIT, ET ÇA MONTE À L'IMAGE", buf);

		snprintf(buf, sizeof(buf),
				 "sans poussée : %u pixels, rangée barycentre %.1f (haut à la rangée %u), contre %.1f (haut %u) "
				 "avec poussée — le panache coupé reste EN BAS de l'image, et l'écart est de %.1f rangées "
				 "(image : Captures/fumee_sans_poussee_2026-09-14.png)",
				 pixelsNeg, (double)ligneNeg, hautNeg, faite > 0 ? (double)lignes[faite - 1] : 0.0,
				 faite > 0 ? hauts[faite - 1] : 0u, faite > 0 ? (double)(ligneNeg - lignes[faite - 1]) : 0.0);
		ProbeCheck(faite > 0 && ligneNeg > lignes[faite - 1], "(f4-) NÉGATIF : sans poussée, rien ne monte à l'image",
				   buf);
	}

	// =========================================================================
	// LE PRIX — mesuré ici, écrit ICI, dans la sortie du programme.
	// Le feu est chronométré en appelant la scène EXISTANTE (rendu.cpp:119), pas
	// une copie : une copie de scène qui dérive est déjà un piège connu du banc.
	// =========================================================================
	{
		NkFluidGrid gfeu;
		const int64 t0 = ::nkentseu::NkChrono::Now().nanoseconds;
		ConstruirePanache(gfeu, true, kPas, EpsilonConfinement());
		const float32 msFeu = (float32)((::nkentseu::NkChrono::Now().nanoseconds - t0) / 1.0e6);

		// ── LE COÛT PAR PAS DU FEU, pas à pas, pour en tirer le MINIMUM ─────
		// ⚠️ UNE LIGNE EST COPIÉE de rendu.cpp:151 — la source du feu — parce que
		// `ConstruirePanache` ne rend pas la main entre deux pas. Elle est DITE, et
		// elle ne décide de rien : ces pas-ci ne servent qu'au chronomètre, jamais
		// à un critère. Le reste de la scène est celle de rendu.cpp, non recopiée.
		float32 msFeuMin = 1.0e30f, msFeuSomme = 0.f;
		const uint32 kPasChrono = 30;
		for (uint32 s = 0; s < kPasChrono; ++s) {
			gfeu.EmitSphere({0.f, 0.07f, 0.f}, 0.07f, 0.05f * kDt, 0.f, 5.f * kDt);
			gfeu.Step(kDt);
			const float32 ms = gfeu.Stats().ms;
			msFeuSomme += ms;
			if (ms < msFeuMin)
				msFeuMin = ms;
		}

		NkVector<uint8> imgFeu;
		NkFluidRaymarchStats stFeu;
		NkFluidRaymarchParams rf = CameraDuBanc();
		rf.emission = true; // LE feu : le second interrupteur, celui du rendu
		NkFluidRaymarchRender(gfeu, rf, imgFeu, stFeu);

		NkVector<uint8> imgFum;
		NkFluidRaymarchStats stFum;
		NkFluidRaymarchRender(g, rp, imgFum, stFum);

		printf("\n    ───────────────  LE PRIX, PAR IMAGE  ───────────────\n");
		printf("    grille 25 x 80 x 25 = %u cellules intérieures, rendu 480 x 360, CPU seul.\n",
			   g.Stats().cellsInterior);
		printf("    ⚠️ LIRE LA COLONNE « mini » D'ABORD. Cette machine est partagée avec\n");
		printf("       d'autres agents qui compilent : la MOYENNE porte leur charge, pas le\n");
		printf("       coût de ce code. Mesuré le 14/09 : deux courses du MÊME binaire ont\n");
		printf("       rendu 241,9 puis 306,3 ms de moyenne — 27 %% d'écart sans qu'une ligne\n");
		printf("       ne change. Le pas le MOINS gêné est la meilleure approche par le bas.\n");
		printf("                                    mini        moyenne        max\n");
		printf("    FUMÉE FROIDE, simulation :  %8.1f ms  %8.1f ms  %8.1f ms  (%u pas)\n",
			   (double)r.msSolveurMin, (double)(r.msSolveur / (float32)r.pas), (double)r.msSolveurMax, r.pas);
		printf("    FEU,          simulation :  %8.1f ms  %8.1f ms       --      (%u pas chronométrés,\n",
			   (double)msFeuMin, (double)(msFeuSomme / (float32)kPasChrono), kPasChrono);
		printf("                                                                  après %u pas de mise en régime)\n",
			   kPas);
		printf("    RAPPORT feu / fumée sur le MINI : %.2f  (sur la moyenne : %.2f)\n",
			   (double)(msFeuMin / (r.msSolveurMin > 0.f ? r.msSolveurMin : 1.f)),
			   (double)((msFeuSomme / (float32)kPasChrono) / (r.msSolveur / (float32)r.pas)));
		printf("    MARCHE DE RAYON, 480 x 360 : fumée %.0f ms ; feu %.0f ms (émission ALLUMÉE).\n",
			   (double)stFum.ms, (double)stFeu.ms);
		printf("                      échantillons : fumée %llu + %llu d'ombre ; feu %llu + %llu.\n",
			   (unsigned long long)stFum.samples, (unsigned long long)stFum.shadowSamples,
			   (unsigned long long)stFeu.samples, (unsigned long long)stFeu.shadowSamples);
		printf("                      %u rayons dont %u coupent la boîte. UNE seule marche chacun :\n",
			   stFum.rays, stFum.raysHit);
		printf("                      pas de mini, donc ces deux-là portent la charge en entier.\n");
		printf("                      ⚠️ LA RÉSOLUTION VOYAGE AVEC LE CHIFFRE. Le balayage du palier\n");
		printf("                      (2) mesure la MÊME marche à 119,9 ms en 240 x 180, 523,4 ms en\n");
		printf("                      480 x 360 et 2011,3 ms en 960 x 720 : un temps de marche sans sa\n");
		printf("                      résolution ne se compare à rien.\n");
		printf("    Pour mémoire, le mur de la course entière : %.0f ms pour %u pas de fumée\n", (double)r.msMur,
			   r.pas);
		printf("    (soit %.1f ms/image source comprise) et %.0f ms pour %u pas de feu.\n",
			   (double)(r.msMur / (float32)r.pas), (double)msFeu, kPas);
		printf("    ⚠️ Ces chiffres sont MESURÉS ici, sur CETTE machine, dans CETTE course.\n");
		printf("    ⚠️ Le prix de la fumée n'est PAS celui du feu moins la combustion : le feu\n");
		printf("       porte un champ de carburant que la fumée ne fait même pas advecter\n");
		printf("       (NkFluidGrid.cpp:1373 — il est sauté tant que burnRate vaut 0).\n");
		fflush(stdout);
	}
}
