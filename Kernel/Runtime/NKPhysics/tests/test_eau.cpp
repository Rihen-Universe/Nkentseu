// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_eau.cpp — les TÉMOINS de l'eau au-delà du fluide (2026-09-06).
// ROADMAP_PRODUITS.md §6.6 : palier 1 (les éclaboussures) et paliers 2-3 (le
// mouillage). Écrits AVANT de lancer quoi que ce soit, scènes fixées une fois.
// Appelé depuis le main() de test_physics.cpp, mêmes compteurs.
//
// ⚠️ POURQUOI ICI, dans NKPhysics_Tests, alors que NkContactEvent / NkSplashLaw /
// NkWetnessMap vivent dans NKMath : parce que NKPhysics_Tests lie déjà NKMath ET
// NKPhysics, qu'il est DÉJÀ dans la liste `allow` de dutc/dute (Nkentseu.jenga),
// et que le témoin (e) a besoin de NkCloth. Un seul exécutable, aucune cible
// nouvelle à ouvrir. Le témoin qui manque ici est celui du PRODUCTEUR (le
// solveur SPH publie-t-il ?) : il vit dans NKRenderer, et il est dans
// Kernel/Runtime/NKRenderer/tests/test_eau_sph.cpp.
//
// Les chiffres partent sur stderr, comme test_cloth.cpp et pour la même raison
// mesurée : le puits console de NKLogger ne traverse pas un tuyau dans cet arbre.
//
// LES TÉMOINS, et ce que chacun prouve :
//   (a) LA FILE  — bornée, et elle DIT ce qu'elle perd.
//   (b) LA LOI   — seuil, proportionnalité, direction, déterminisme, plafond.
//                  (b3) porte son CONTRÔLE POSITIF : l'instrument sait séparer
//                  une loi linéaire d'une loi en énergie. Sans lui, un rapport
//                  de 2 ne prouverait rien -- il pourrait sortir d'un compteur
//                  qui ne compte pas ce qu'on croit.
//   (c) LA CARTE — dépôt local, séchage exponentiel, MUTATION (séchage coupé),
//                  et deux matières dans le bon ORDRE (pas à la bonne valeur).
//   (d) LA LUMIÈRE — identité à sec (contrôle positif), les constantes de
//                  Lagarde au chiffre près, monotonie, et le RETOUR à la couleur.
//   (e) LE TISSU — masse, absence de dérive, et « pend plus bas », en mm.
//
// ⚠️ CE QUE (d) MESURE EXACTEMENT — la population, dite avant le chiffre : ce
// sont les sorties de `NkApplyWetness`, c'est-à-dire l'albédo et la rugosité que
// le nuanceur RECEVRAIT. Ce n'est PAS une lecture de framebuffer. Le brief
// demande « mesuré en pixels » ; ce banc mesure la valeur qui produit le pixel.
// La lecture de pixels demande un rendu et elle est nommée comme non faite.
// =============================================================================
#include "NKPhysics/NkCloth.h"
#include "NKMath/NkContactEvent.h"
#include "NKMath/NkSplashLaw.h"
#include "NKMath/NkWetnessMap.h"
#include "NKMath/NkFunctions.h"
#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::math;

namespace {

	int *gPass = nullptr;
	int *gFail = nullptr;

	void ECHECK(bool ok, const char *what) {
		if (ok) {
			++(*gPass);
			std::fprintf(stderr, "  [ok]    %s\n", what);
		} else {
			++(*gFail);
			std::fprintf(stderr, "  [ROUGE] %s\n", what);
		}
	}

	// ─────────────────────────────────────────────────────────────────────
	// (a) LA FILE : bornée, et elle dit ce qu'elle perd.
	// ─────────────────────────────────────────────────────────────────────
	void TemoinFile() {
		std::fprintf(stderr, "-- (a) file de contacts bornee --\n");
		NkContactQueue q;
		q.Reserve(4);
		NkContactEvent e;
		e.normalSpeed = 1.f;
		uint32 accepted = 0;
		for (uint32 i = 0; i < 10; ++i) {
			e.index = i;
			if (q.Push(e))
				++accepted;
		}
		std::fprintf(stderr, "     capacite=%u prises=%u perdues=%u proposees=%u\n", q.Capacity(), q.Count(),
					 q.Dropped(), q.Total());
		ECHECK(q.Count() == 4u && accepted == 4u, "(a1) la file borne a sa capacite (4 prises sur 10)");
		ECHECK(q.Dropped() == 6u, "(a2) elle COMPTE ce qu'elle perd (6), elle ne le perd pas en silence");
		ECHECK(q.Total() == 10u, "(a3) elle dit la POPULATION proposee (10) : un chiffre sans elle ne vaut rien");
		ECHECK(q.Overflowed(), "(a4) le debordement est lisible d'un seul appel");
		q.Clear();
		ECHECK(q.Count() == 0u && q.Dropped() == 0u && q.Total() == 0u, "(a5) Clear remet les TROIS compteurs a zero");
	}

	// Un contact synthétique, tombant droit sur un sol horizontal.
	NkContactEvent Impact(float32 vn, uint32 index, float32 t = 0.f) {
		NkContactEvent e;
		e.position = {0.f, 0.f, 0.f};
		e.normal = {0.f, 1.f, 0.f};
		e.velocity = {0.f, -vn, 0.f};
		e.normalSpeed = vn;
		e.time = t;
		e.index = index;
		return e;
	}

	// ─────────────────────────────────────────────────────────────────────
	// (b) LA LOI D'ÉCLABOUSSURE
	// ─────────────────────────────────────────────────────────────────────
	uint32 TotalGouttes(const NkSplashParams &p, float32 vn, uint32 nEvents) {
		uint32 total = 0;
		for (uint32 i = 0; i < nEvents; ++i)
			total += NkSplashDropCount(p, Impact(vn, i, (float32)i * 0.001f));
		return total;
	}

	void TemoinLoi() {
		std::fprintf(stderr, "-- (b) loi d'eclaboussure --\n");
		NkSplashParams p; // seuil 0,5 m/s ; 4 gouttes par m/s au-dessus ; exposant 1
		// (b1) SOUS le seuil : rien. C'est ce qui empeche le fluide POSE sur le fond
		// de pleuvoir des gouttes en permanence.
		ECHECK(NkSplashDropCountReal(p, 0.4f) == 0.f && NkSplashDropCountReal(p, p.speedThreshold) == 0.f,
			   "(b1) aucune goutte au seuil ni en dessous");
		NkSplashDrop buf[64];
		ECHECK(NkSplashEmit(p, Impact(0.4f, 7), buf, 64) == 0u, "(b2) et l'emetteur n'en ecrit aucune non plus");
		ECHECK(NkSplashDropCountReal(p, 1.5f) > 0.f, "(b3) au-dessus du seuil, la loi en demande");

		// (b4) DOUBLER LA VITESSE. Population dite : 2 000 evenements par point, arrondi
		// stochastique deterministe -- la moyenne vaut la loi exactement.
		const uint32 N = 2000u;
		const float32 v1 = p.speedThreshold + 1.f, v2 = p.speedThreshold + 2.f;
		const uint32 t1 = TotalGouttes(p, v1, N), t2 = TotalGouttes(p, v2, N);
		const float32 ratio = t1 ? (float32)t2 / (float32)t1 : 0.f;
		std::fprintf(stderr, "     lineaire (exposant 1) : %u -> %u gouttes sur %u impacts, rapport %.3f\n", t1, t2, N,
					 (double)ratio);
		ECHECK(ratio > 1.9f && ratio < 2.1f,
			   "(b4) LOI DITE : n = gain x (vn - seuil)^1 -> doubler la vitesse double les gouttes");

		// (b5) CONTRÔLE POSITIF DE L'INSTRUMENT. Si le meme montage rendait 2 avec un
		// exposant de 2, c'est que le compteur ne mesure pas la loi. Il doit rendre ~4.
		NkSplashParams pe = p;
		pe.exponent = 2.f;
		pe.maxDropsPerEvent = 4096u; // sinon le plafond ecrase la difference qu'on cherche
		const uint32 e1 = TotalGouttes(pe, v1, N), e2 = TotalGouttes(pe, v2, N);
		const float32 ratioE = e1 ? (float32)e2 / (float32)e1 : 0.f;
		std::fprintf(stderr, "     energie (exposant 2)  : %u -> %u gouttes, rapport %.3f\n", e1, e2, (double)ratioE);
		ECHECK(ratioE > 3.8f && ratioE < 4.2f,
			   "(b5) CONTROLE POSITIF : en energie le meme instrument rend ~4 -- il sait separer les deux lois");

		// (b6) DIRECTION : les gouttes partent DE la surface. Une goutte dont la vitesse
		// entre dans le solide est un defaut visible immediatement.
		NkContactEvent e = Impact(3.f, 42);
		e.velocity = {2.f, -3.f, 0.5f}; // impact oblique : le cas ou une reflexion rasante fait mal
		const uint32 k = NkSplashEmit(p, e, buf, 64);
		bool toutesDehors = true;
		float32 cosMin = 1.f;
		// direction de reference = reflexion redressee vers la normale (NkSplashLaw.h)
		NkVec3f d = e.velocity;
		d.Normalize();
		const float32 dn = d.Dot(e.normal);
		NkVec3f r = d - e.normal * (2.f * dn);
		r.Normalize();
		NkVec3f axis = r + e.normal;
		axis.Normalize();
		for (uint32 i = 0; i < k; ++i) {
			NkVec3f v = buf[i].velocity;
			if (v.Dot(e.normal) <= 0.f)
				toutesDehors = false;
			NkVec3f vn = v;
			vn.Normalize();
			const float32 c = vn.Dot(axis);
			if (c < cosMin)
				cosMin = c;
		}
		std::fprintf(stderr, "     %u gouttes, angle max au cone %.1f deg (demi-angle demande %.1f deg)\n", k,
					 (double)(NkToDegrees(NkAcos(NkClamp(cosMin, -1.f, 1.f)))),
					 (double)NkToDegrees(p.coneHalfAngle));
		ECHECK(k > 0u && toutesDehors, "(b6) toutes les gouttes s'eloignent de la surface (v.n > 0)");
		ECHECK(cosMin >= NkCos(p.coneHalfAngle) - 1e-3f, "(b7) et toutes tiennent dans le cone demande");

		// (b8) DÉTERMINISME : deux passes, meme resultat. Un banc qui ne peut pas etre
		// rejoue ne peut pas etre contredit.
		NkSplashDrop buf2[64];
		const uint32 k2 = NkSplashEmit(p, e, buf2, 64);
		bool identique = (k == k2);
		for (uint32 i = 0; i < k && identique; ++i)
			identique = (buf[i].velocity - buf2[i].velocity).Len() < 1e-6f;
		ECHECK(identique, "(b8) deterministe : le meme contact rend exactement les memes gouttes");

		// (b9) PLAFOND
		NkSplashParams pc = p;
		pc.maxDropsPerEvent = 3u;
		ECHECK(NkSplashDropCount(pc, Impact(100.f, 1)) == 3u, "(b9) le plafond par evenement est respecte");
	}

	// ─────────────────────────────────────────────────────────────────────
	// (c) LA CARTE DE MOUILLAGE : dépôt, séchage, mutation, deux matières.
	// ─────────────────────────────────────────────────────────────────────
	void TemoinCarte() {
		std::fprintf(stderr, "-- (c) carte de mouillage --\n");
		NkWetnessMap m;
		m.Create(256, 256);
		m.material = NkWetSand();
		std::fprintf(stderr, "     carte 256x256 float32 : %u octets (%u Kio) ; en R8 au GPU : %u octets\n", m.Bytes(),
					 m.Bytes() / 1024u, m.Bytes() / 4u);
		ECHECK(m.Bytes() == 256u * 256u * 4u, "(c1) memoire DITE et verifiee : 4 octets par texel");
		ECHECK(m.Max() == 0.f && m.IsDry(), "(c2) une carte neuve est seche");

		m.Splat(0.5f, 0.5f, 0.05f, 1.f);
		const float32 centre = m.Sample(0.5f, 0.5f);
		const float32 loin = m.Sample(0.05f, 0.05f);
		std::fprintf(stderr, "     apres un depot au centre : centre=%.4f, coin=%.4f\n", (double)centre, (double)loin);
		ECHECK(centre > 0.5f, "(c3) le depot mouille la ou le contact a eu lieu");
		ECHECK(loin == 0.f, "(c4) et NULLE PART ailleurs (un depot local reste local)");

		// (c5) SÉCHAGE EXPONENTIEL : apres tau il reste 1/e.
		NkWetnessMap s;
		s.Create(8, 8);
		s.material.dryingTime = 10.f;
		for (uint32 i = 0; i < 64u; ++i)
			s.Data()[i] = 1.f;
		const float32 w0 = s.Sample(0.5f, 0.5f);
		const float32 dt = 1.f / 60.f;
		for (uint32 i = 0; i < 600u; ++i) // 10 s = tau
			s.Dry(dt);
		const float32 wTau = s.Sample(0.5f, 0.5f);
		const float32 attendu = NkExp(-1.f);
		std::fprintf(stderr, "     w0=%.4f -> w(tau)=%.4f (attendu 1/e = %.4f, ecart %.2f %%)\n", (double)w0,
					 (double)wTau, (double)attendu, (double)(100.f * NkFabs(wTau - attendu) / attendu));
		ECHECK(NkFabs(wTau - attendu) < 0.01f * attendu, "(c5) sechage exponentiel : w(tau) = w0 / e a 1 %");
		for (uint32 i = 0; i < 2400u; ++i) // + 40 s = 5 tau au total
			s.Dry(dt);
		ECHECK(s.Sample(0.5f, 0.5f) < 0.01f, "(c6) apres 5 tau il ne reste rien (< 1 %)");

		// (c7) MUTATION : sechage coupe -> le temoin doit ROUGIR. Ici on verifie que
		// la mutation change bien quelque chose ; c'est ce qui rend (c5) capable de rougir.
		NkWetnessMap mu;
		mu.Create(8, 8);
		mu.material.dryingTime = 10.f;
		for (uint32 i = 0; i < 64u; ++i)
			mu.Data()[i] = 1.f;
		for (uint32 i = 0; i < 600u; ++i)
			mu.Dry(dt, false);
		std::fprintf(stderr, "     MUTATION sechage coupe : w apres tau = %.4f (le sain vaut %.4f)\n",
					 (double)mu.Sample(0.5f, 0.5f), (double)wTau);
		ECHECK(mu.Sample(0.5f, 0.5f) > 0.99f,
			   "(c7) MUTATION : sechage desactive -> rien ne seche, donc (c5) SAIT rougir");

		// (c8) DEUX MATIÈRES, ORDRE et non valeur (les tau sont de la direction artistique).
		NkWetnessMap sable, cuir;
		sable.Create(4, 4);
		cuir.Create(4, 4);
		sable.material = NkWetSand();	 // tau 20 s
		cuir.material = NkWetLeather(); // tau 300 s
		for (uint32 i = 0; i < 16u; ++i) {
			sable.Data()[i] = 1.f;
			cuir.Data()[i] = 1.f;
		}
		for (uint32 i = 0; i < 3600u; ++i) { // 60 s
			sable.Dry(dt);
			cuir.Dry(dt);
		}
		const float32 ws = sable.Sample(0.5f, 0.5f), wc = cuir.Sample(0.5f, 0.5f);
		std::fprintf(stderr, "     a 60 s : sable=%.4f (tau %.0f s), cuir=%.4f (tau %.0f s)\n", (double)ws,
					 (double)sable.material.dryingTime, (double)wc, (double)cuir.material.dryingTime);
		ECHECK(ws < wc, "(c8) le sable seche AVANT le cuir (l'ordre est la promesse, pas les secondes)");
		ECHECK(ws < 0.1f && wc > 0.5f, "(c9) et l'ecart est franc a 60 s, pas une difference de bruit");
	}

	// ─────────────────────────────────────────────────────────────────────
	// (d) CE QUE LE MOUILLAGE FAIT À LA LUMIÈRE (formule de Lagarde 3b).
	// ─────────────────────────────────────────────────────────────────────
	void TemoinLumiere() {
		std::fprintf(stderr, "-- (d) l'aspect mouille (Lagarde, Water drop 3b, 2013) --\n");
		const NkVec3f albedo = {0.5f, 0.4f, 0.3f};
		const float32 rough = 0.8f, f0 = 0.04f;
		NkWetMaterial sable = NkWetSand(); // porosite 1

		// (d1) CONTRÔLE POSITIF DE L'INSTRUMENT : a sec, la formule est l'IDENTITE.
		// Une formule qui bouge a w = 0 est fausse avant meme qu'on la mesure.
		NkWetShading sec = NkApplyWetness(albedo, rough, f0, 0.f, sable);
		ECHECK((sec.albedo - albedo).Len() < 1e-6f && NkFabs(sec.roughness - rough) < 1e-6f &&
				   NkFabs(sec.f0 - f0) < 1e-6f,
			   "(d1) CONTROLE POSITIF : a w = 0 la formule ne touche a RIEN");

		// (d2) Les constantes de Lagarde, au chiffre : albedo x0,2 et rugosite x0,6.
		NkWetShading trempe = NkApplyWetness(albedo, rough, f0, 1.f, sable);
		std::fprintf(stderr, "     porosite 1, w = 1 : albedo %.3f -> %.3f (x%.3f), rugosite %.3f -> %.3f (x%.3f)\n",
					 (double)albedo.x, (double)trempe.albedo.x, (double)(trempe.albedo.x / albedo.x), (double)rough,
					 (double)trempe.roughness, (double)(trempe.roughness / rough));
		ECHECK(NkFabs(trempe.albedo.x / albedo.x - 0.2f) < 1e-4f,
			   "(d2) albedo x0,2 a saturation : lerp(1, 0.2, porosite) mot pour mot");
		ECHECK(NkFabs(trempe.roughness / rough - 0.6f) < 1e-4f,
			   "(d3) rugosite x0,6 = 1 - 0,4 x porosite x w (conversion algebrique du gloss de Lagarde)");

		// (d4) MONOTONIE : plus c'est mouille, plus c'est sombre et plus c'est lisse.
		bool monotone = true;
		float32 aPrev = 1e9f, rPrev = 1e9f;
		for (uint32 i = 0; i <= 10u; ++i) {
			const NkWetShading o = NkApplyWetness(albedo, rough, f0, (float32)i * 0.1f, sable);
			if (o.albedo.x > aPrev + 1e-6f || o.roughness > rPrev + 1e-6f)
				monotone = false;
			aPrev = o.albedo.x;
			rPrev = o.roughness;
		}
		ECHECK(monotone, "(d4) albedo et rugosite decroissent quand le mouillage monte (11 points)");

		// (d5) POROSITE 0 (verre poli) : l'eau ne rentre pas, rien ne s'assombrit.
		NkWetMaterial verre;
		verre.porosity = 0.f;
		NkWetShading v = NkApplyWetness(albedo, rough, f0, 1.f, verre);
		ECHECK((v.albedo - albedo).Len() < 1e-6f, "(d5) porosite 0 : trempe mais pas assombri (le mouille n'est pas uniforme)");

		// (d6) IL S'ASSOMBRIT, PUIS IL REVIENT. C'est la demande de Rodolf, bout a bout :
		// une carte mouillee, la couleur mesuree, le sechage, la couleur remesuree.
		NkWetnessMap m;
		m.Create(32, 32);
		m.material = sable;
		m.material.dryingTime = 5.f;
		for (uint32 i = 0; i < 32u * 32u; ++i)
			m.Data()[i] = 1.f;
		const float32 aSec = albedo.x;
		const float32 aMouille = NkApplyWetness(albedo, rough, f0, m.Sample(0.5f, 0.5f), m.material).albedo.x;
		const float32 dt = 1.f / 60.f;
		for (uint32 i = 0; i < 60u * 25u; ++i) // 25 s = 5 tau
			m.Dry(dt);
		const float32 aRevenu = NkApplyWetness(albedo, rough, f0, m.Sample(0.5f, 0.5f), m.material).albedo.x;
		std::fprintf(stderr, "     albedo : sec %.4f -> mouille %.4f (-%.1f %%) -> apres 5 tau %.4f (ecart au sec %.2f %%)\n",
					 (double)aSec, (double)aMouille, (double)(100.f * (1.f - aMouille / aSec)), (double)aRevenu,
					 (double)(100.f * NkFabs(aRevenu - aSec) / aSec));
		ECHECK(aMouille < 0.5f * aSec, "(d6) une surface touchee S'ASSOMBRIT (plus de moitie a saturation)");
		ECHECK(NkFabs(aRevenu - aSec) < 0.01f * aSec, "(d7) et elle RETROUVE sa couleur apres le sechage (1 %)");
	}

	// ─────────────────────────────────────────────────────────────────────
	// (e) LE TISSU MOUILLÉ : plus lourd, et il pend plus bas.
	// ─────────────────────────────────────────────────────────────────────
	// Nappe 1 m x 1 m, 16 x 16, 0,2 kg, rangee du haut epinglee, 3 s a 1/60 s.
	// `alpha` = compliance STRUCTURELLE (m/N). Elle est un ARGUMENT, et c est la
	// lecon de ce temoin : voir le bloc (e3) plus bas.
	float32 CourseNappe(const NkWetnessMap *carte, float32 alpha, float32 &outMasse,
						float32 &outEchelleCompliance) {
		physics::NkCloth c;
		const uint32 n = 16u;
		const float32 d = 1.f / (float32)(n - 1u);
		c.BuildGrid(n, n, NkVec3f{0.f, 1.f, 0.f}, NkVec3f{d, 0.f, 0.f}, NkVec3f{0.f, 0.f, d}, 0.2f);
		// la rangee j = 0 est epinglee : la nappe pend
		for (uint32 i = 0; i < n; ++i)
			c.Pin(c.GridIndex(i, 0u));
		c.params.selfCollision = false;
		c.params.collisions = false;
		c.params.compliance = alpha;
		c.params.shearCompliance = alpha;
		c.wetness = carte;
		for (uint32 s = 0; s < 180u; ++s)
			c.Step(1.f / 60.f);
		outMasse = c.Stats().mass;
		outEchelleCompliance = c.Stats().complianceScale;
		float32 ymin = 1e30f;
		const NkVec3f *P = c.Positions();
		for (uint32 i = 0; i < c.ParticleCount(); ++i)
			if (P[i].y < ymin)
				ymin = P[i].y;
		return ymin;
	}

	void TemoinTissu() {
		std::fprintf(stderr, "-- (e) le tissu mouille --\n");
		NkWetnessMap m;
		m.Create(16, 16);
		m.material = NkWetCloth(); // gain de masse 0,6 ; gain de compliance 3
		for (uint32 i = 0; i < 16u * 16u; ++i)
			m.Data()[i] = 1.f;

		float32 mSec = 0.f, mMou = 0.f, csSec = 0.f, csMou = 0.f;
		// -- LE PIEGE, ET IL A COUTE UN ROUGE AVANT D ETRE COMPRIS ---------------
		// Premiere ecriture de ce temoin : compliance structurelle 0 (le defaut de
		// NkClothParams, « inextensible »). Mesure : la nappe mouillee pendait
		// 0,2 mm PLUS HAUT que la seche. Ce n etait pas un defaut du mouillage --
		// c est la SCENE qui ne pouvait pas repondre a la question. Une nappe
		// inextensible pend a la longueur de son tissu : la masse ne peut pas
		// l allonger, et alpha x 3 vaut encore 0. Le 0,2 mm venait du seul terme qui
		// bougeait, la flexion, qui plie un peu plus la nappe lourde et REMONTE donc
		// son point bas. « Un cas neutre ne teste pas ce qui multiplie zero. »
		// La scene qui repond est un tissu qui S ETIRE. Deuxieme mesure, a alpha =
		// 1e-4 m/N : 0,09 mm d ecart -- du bon signe, mais SOUS le residu du solveur
		// (16 sous-pas x 2 iterations ne convergent pas a ce point, cf. la table de
		// NkCloth.h). Un ecart qu on ne peut pas distinguer du bruit n est pas un
		// temoin. A alpha = 1e-3 m/N (un tricot souple) l ecart est de 2,4 mm, soit
		// 25 fois le seuil : la scene retenue, et les deux chiffres sont dits.
		// Les deux mesures sont gardees, et celle qui ne peut pas repondre est DITE.
		const float32 ySecRigide = CourseNappe(nullptr, 0.f, mSec, csSec);
		const float32 yMouRigide = CourseNappe(&m, 0.f, mMou, csMou);
		std::fprintf(stderr, "     [scene INEXTENSIBLE alpha = 0 : celle qui ne PEUT PAS repondre]\n");
		std::fprintf(stderr, "       point le plus bas : sec %.4f m -> mouille %.4f m (%.2f mm)\n",
					 (double)ySecRigide, (double)yMouRigide, (double)(1000.f * (ySecRigide - yMouRigide)));
		const float32 alpha = 1e-3f;
		const float32 ySec = CourseNappe(nullptr, alpha, mSec, csSec);
		const float32 yMou = CourseNappe(&m, alpha, mMou, csMou);
		std::fprintf(stderr, "     [scene SOUPLE alpha = 1e-3 m/N]\n");
		std::fprintf(stderr, "     masse : sec %.4f kg -> mouille %.4f kg (x%.3f, gain annonce %.2f)\n", (double)mSec,
					 (double)mMou, (double)(mMou / mSec), (double)m.material.saturatedMassGain);
		std::fprintf(stderr, "     point le plus bas : sec %.4f m -> mouille %.4f m (%.2f mm plus bas)\n", (double)ySec,
					 (double)yMou, (double)(1000.f * (ySec - yMou)));
		std::fprintf(stderr, "     echelle de compliance : sec %.3f -> mouille %.3f\n", (double)csSec, (double)csMou);
		ECHECK(NkFabs(mSec - 0.2f) < 1e-3f, "(e1) la nappe seche pese sa masse de construction (0,2 kg)");
		ECHECK(NkFabs(mMou - 0.2f * (1.f + m.material.saturatedMassGain)) < 1e-3f,
			   "(e2) la nappe mouillee pese m_sec x (1 + gain) : la MASSE monte, mesuree");
		ECHECK(yMou < ySec - 1e-4f, "(e3) sur un tissu qui s etire, la mouillee PEND PLUS BAS (> 0,1 mm)");
		ECHECK(NkFabs(ySecRigide - yMouRigide) < 1e-3f,
			   "(e3bis) sur un tissu INEXTENSIBLE elle ne le peut pas : la scene est DITE, pas cachee");
		ECHECK(NkFabs(csMou - (1.f + (m.material.saturatedComplianceGain - 1.f))) < 1e-3f,
			   "(e4) l'echelle de compliance suit la carte (mouillee a 1 partout)");
		ECHECK(NkFabs(csSec - 1.f) < 1e-6f, "(e5) et vaut exactement 1 sans carte (aucun effet de bord a sec)");

		// (e6) PAS DE DÉRIVE. Le piege du lot : recalculer la masse depuis mMass au lieu
		// de mMassDry la multiplierait a chaque pas. 180 pas plus haut n'auraient rien vu
		// si on ne comparait pas la masse a un SEUL pas.
		physics::NkCloth c;
		c.BuildGrid(8u, 8u, NkVec3f{0.f, 1.f, 0.f}, NkVec3f{0.1f, 0.f, 0.f}, NkVec3f{0.f, 0.f, 0.1f}, 0.2f);
		c.params.collisions = false;
		c.wetness = &m;
		c.Step(1.f / 60.f);
		const float32 m1 = c.Stats().mass;
		for (uint32 s = 0; s < 600u; ++s)
			c.Step(1.f / 60.f);
		const float32 m600 = c.Stats().mass;
		std::fprintf(stderr, "     masse au pas 1 : %.5f kg ; au pas 601 : %.5f kg (derive %.3g)\n", (double)m1,
					 (double)m600, (double)NkFabs(m600 - m1));
		ECHECK(NkFabs(m600 - m1) < 1e-5f, "(e6) AUCUNE DERIVE sur 601 pas : la masse se recalcule depuis la masse SECHE");

		// (e7) RETOUR A SEC : la carte retiree, la masse redescend au pas suivant.
		c.wetness = nullptr;
		c.Step(1.f / 60.f);
		std::fprintf(stderr, "     carte retiree : masse = %.5f kg\n", (double)c.Stats().mass);
		ECHECK(NkFabs(c.Stats().mass - 0.2f) < 1e-4f, "(e7) carte retiree -> le tissu retrouve sa masse seche");
	}

} // namespace

int RunEauTests(int &pass, int &fail) {
	gPass = &pass;
	gFail = &fail;
	std::fprintf(stderr, "=== L'EAU AU-DELA DU FLUIDE : temoins §6.6 (eclaboussures + mouillage) ===\n");
	TemoinFile();
	TemoinLoi();
	TemoinCarte();
	TemoinLumiere();
	TemoinTissu();
	return fail;
}
