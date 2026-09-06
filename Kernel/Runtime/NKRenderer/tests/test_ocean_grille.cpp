// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_ocean_grille.cpp — LES TÉMOINS DE LA GRILLE PROJETÉE (2026-09-07, nuit).
// Johanson 2004, « projected grid ». Aucun GPU, aucune fenêtre : de la géométrie
// se mesure sans écran, et c'est pour ça que la grille a été écrite comme de la
// géométrie pure (NKMath/NkProjectedGrid.h) et non comme un morceau de renderer.
//
// LA QUESTION QUE CES TÉMOINS POSENT, et elle n'est pas « est-ce joli » :
//   *Reste-t-il, quelque part à l'écran, un pixel où l'on VOIT de l'eau et que
//    la grille n'atteint PAS ?* Un tel pixel est un trou dans l'océan.
//
//   (x0) construction nominale : visible, avec ses points.
//   (x1) COUVERTURE DU PLAN DE REPOS : tout pixel où le plan est visible tombe
//        dans l'étendue de la grille. C'est le minimum.
//   (x2) COUVERTURE DE LA TRANCHE : les pixels où la SURFACE DÉPLACÉE (crête et
//        creux) est visible sont couverts eux aussi — 0 découvert sur 1 225.
//   (x2b) 🔴 ET CE QUE `displacementMax` FAIT VRAIMENT. La contre-épreuve à une
//        seule variable a REFUSÉ de rougir : dans le domaine, écraser la tranche
//        ne change RIEN à la couverture. Le témoin dit donc ce fait au lieu de
//        prétendre le contraire. Raison structurelle dans le corps.
//   (x2c) mais le paramètre EST lu : il déplace la frontière du domaine, et ça
//        se mesure des deux côtés. Un paramètre présent et sans aucun effet
//        serait pire qu'absent.
//   (x3) DENSITÉ À L'ÉCRAN : le pas des mailles reprojetées est constant. Et la
//        contre-épreuve donne le chiffre qui justifie tout le concept : une
//        grille MONDE de même budget de sommets, sur la même vue.
//   (x4) POSES DÉGÉNÉRÉES : horizon, plongée verticale, caméra sous l'eau,
//        caméra qui tourne le dos. Aucun NaN, aucune position absurde.
//   (x5) INVISIBLE SE DIT : la grille rend `visible = false` au lieu d'une
//        grille vide plausible.
//
// ⚠️ CE QUE CES TÉMOINS NE PROUVENT PAS : rien n'est dessiné. La grille rend des
// POSITIONS ; le pavage, les indices, le rendu et le niveau de détail ne sont ni
// écrits ni mesurés. C'est écrit ici, dans l'instrument, pas ailleurs.
// =============================================================================
#include "NKMath/NkFunctions.h"
#include "NKMath/NkProjectedGrid.h"
#include "NKMath/NkWaterSurface.h"
#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::math;

namespace {

	int gP = 0, gF = 0;

	void XCHECK(bool ok, const char *quoi) {
		if (ok) {
			++gP;
			std::fprintf(stderr, "  [ok]    %s\n", quoi);
		} else {
			++gF;
			std::fprintf(stderr, "  [ROUGE] %s\n", quoi);
		}
	}

	bool EstFini(float32 v) {
		// Sans <cmath> : un NaN n'est égal à rien, pas même à lui-même, et un
		// infini dépasse toute borne. Deux tests, aucune dépendance.
		return (v == v) && (v < 1e30f) && (v > -1e30f);
	}

	struct Pose {
			NkVec3f oeil;
			NkVec3f cible;
			const char *nom;
	};

	NkMat4f VP(const Pose &pose, float32 aspect = 16.f / 9.f) {
		const NkMat4f proj = NkMat4f::Perspective(NkAngle(60.f), aspect, 0.1f, 2000.f);
		const NkMat4f vue = NkMat4f::LookAt(pose.oeil, pose.cible, NkVec3f{0.f, 1.f, 0.f});
		return proj * vue;
	}

	// Le rayon du pixel (nx, ny) rencontre-t-il le plan y = py DEVANT la caméra ?
	bool PixelVoitLePlan(const NkMat4f &invVP, float32 nx, float32 ny, float32 py, NkVec3f &monde) {
		NkVec3f a, b;
		if (!NkUnprojectNDC(invVP, nx, ny, -1.f, a))
			return false;
		if (!NkUnprojectNDC(invVP, nx, ny, 1.f, b))
			return false;
		const NkVec3f d = b - a;
		if (NkFabs(d.y) < 1e-9f)
			return false;
		const float32 t = (py - a.y) / d.y;
		if (t < 0.f || t > 1.f)
			return false;
		monde = a + d * t;
		return true;
	}

	// Combien de pixels, sur un échantillonnage NDC de N x N, voient le plan y = py
	// SANS être dans l'étendue retenue par la grille ? Zéro = aucun trou.
	uint32 PixelsDecouverts(const NkProjectedGrid &g, const NkMat4f &invVP, float32 py, uint32 N,
							uint32 &vus) {
		uint32 trous = 0;
		vus = 0;
		for (uint32 i = 0; i <= N; ++i) {
			for (uint32 j = 0; j <= N; ++j) {
				const float32 nx = -1.f + 2.f * (float32)i / (float32)N;
				const float32 ny = -1.f + 2.f * (float32)j / (float32)N;
				NkVec3f m;
				if (!PixelVoitLePlan(invVP, nx, ny, py, m))
					continue;
				++vus;
				const float32 eps = 1e-4f;
				if (nx < g.ndcMinX - eps || nx > g.ndcMaxX + eps || ny < g.ndcMinY - eps ||
					ny > g.ndcMaxY + eps)
					++trous;
			}
		}
		return trous;
	}

} // namespace

int NkSondeOceanGrille() {
	gP = 0;
	gF = 0;
	std::fprintf(stderr, "=== LA GRILLE PROJETEE DE L'OCEAN (Johanson 2004) ===\n");

	NkProjectedGridParams p;
	p.cols = 64u;
	p.rows = 64u;
	p.baseY = 0.f;
	p.displacementMax = 2.f;

	// Pose nominale : l'oeil a 8 m, regardant vers l'horizon, legerement plongeant.
	const Pose nominale = {{0.f, 8.f, 0.f}, {0.f, 2.f, -60.f}, "nominale"};
	const NkMat4f vp = VP(nominale);
	NkProjectedGrid g = NkProjectedGridBuild(vp, nominale.oeil, p);

	std::fprintf(stderr, "     (x0) %u points retenus, etendue NDC x[%.3f, %.3f] y[%.3f, %.3f]%s\n",
				 g.pointsRetenus, (double)g.ndcMinX, (double)g.ndcMaxX, (double)g.ndcMinY,
				 (double)g.ndcMaxY, g.repliPleinEcran ? "  (repli plein ecran)" : "");
	XCHECK(g.visible && g.pointsRetenus > 0u, "(x0) pose nominale : la grille se construit et le dit");

	// (x1) COUVERTURE DU PLAN DE REPOS.
	{
		uint32 vus = 0;
		const uint32 trous = PixelsDecouverts(g, g.invViewProj, p.baseY, 48u, vus);
		std::fprintf(stderr, "     (x1) plan de repos : %u pixels d'eau echantillonnes, %u decouverts\n", vus,
					 trous);
		XCHECK(vus > 0u && trous == 0u, "(x1) aucun pixel du plan de repos n'est laisse hors de la grille");
	}

	// (x2) COUVERTURE DE LA TRANCHE.
	//
	// ⚠️ CE TEMOIN A ETE REECRIT PARCE QUE LE PREMIER NE DEPARTAGEAIT RIEN, et le
	// dire vaut plus que le corriger en silence. Sa premiere version comparait
	// deux montages qui differaient par DEUX choses : `displacementMax` ET
	// `edgeBias`. Elle rougissait bien -- mais a cause de la MARGE, pas de la
	// tranche. Mesure qui l'a demasquee : en ecrasant la tranche dans le code
	// lui-meme (mutation M5), le temoin restait VERT. Un critere qui ne peut pas
	// departager est aussi vide qu'un compteur bloque a zero.
	//
	// ET LA POSE COMPTAIT AUTANT QUE LE CRITERE. Depuis un oeil HAUT, les plans
	// y = 0 et y = +2 partagent le meme horizon a l'ecran : la tranche ne change
	// presque rien, et la donnee d'essai ne pouvait pas exprimer l'ecart. Le cas
	// ou la tranche decide est celui ou la CRETE PASSE AU-DESSUS DE L'OEIL --
	// oeil a 1 m, crete a 2 m : la surface deplacee se voit alors DANS LE HAUT de
	// l'ecran, au-dessus de l'horizon du plan de repos, exactement la ou une
	// grille batie sur le seul plan de repos n'a aucun sommet.
	{
		// ⚠️ POSE CHOISIE DANS LE DOMAINE. La premiere version de ce temoin mettait
		// l'oeil a 1 m avec une crete a 2 m : elle a trouve un vrai defaut (0 pixel
		// couvert sur 1 225), qui a fait DECLARER LE DOMAINE de cette version plutot
		// que d'inventer une correction sans image. Ici l'oeil est a 6 m pour une
		// tranche de +/- 2 m : la crete reste sous l'oeil, on est dedans.
		const Pose oeilBas = {{0.f, 6.f, 0.f}, {0.f, 5.f, -40.f}, "oeil a 6 m, tranche +/- 2 m"};
		const NkMat4f vpBas = VP(oeilBas);
		NkProjectedGridParams pb = p;
		pb.edgeBias = 0.f; // LA SEULE VARIABLE sera displacementMax
		pb.displacementMax = 2.f;
		const NkProjectedGrid gb = NkProjectedGridBuild(vpBas, oeilBas.oeil, pb);

		uint32 vusH = 0, vusB = 0;
		const uint32 trousH = PixelsDecouverts(gb, gb.invViewProj, pb.baseY + pb.displacementMax, 48u, vusH);
		const uint32 trousB = PixelsDecouverts(gb, gb.invViewProj, pb.baseY - pb.displacementMax, 48u, vusB);
		std::fprintf(stderr,
					 "     (x2) oeil a 6 m | crete (+2 m) : %u vus, %u decouverts | creux (-2 m) : %u vus, %u decouverts\n",
					 vusH, trousH, vusB, trousB);
		XCHECK(gb.visible && vusH > 0u && trousH == 0u && trousB == 0u,
			   "(x2) la TRANCHE entiere est couverte : une vague qui monte reste dans la grille");

		// (x2b) CE QUE `displacementMax` FAIT VRAIMENT -- mesure, pas suppose.
		//
		// La contre-epreuve a une seule variable a ete ecrite, lancee, et elle a
		// REFUSE de rougir : meme pose, meme marge, `displacementMax` ecrase a 0,
		// et TOUJOURS zero pixel decouvert. Ce n'est pas un temoin faible, c'est un
		// FAIT sur cette version, et il a une raison structurelle :
		//   1. l'etape 3 RABAT les points d'intersection sur le plan de repos avant
		//      de prendre leur etendue -- ce rabattement efface la tranche ;
		//   2. et dans le domaine (oeil AU-DESSUS de la tranche), les plans
		//      y = base et y = base +/- H partagent le meme horizon a l'ecran, donc
		//      l'etendue du plan de repos suffit deja.
		// Hors domaine, la tranche changeait tout (1 225 pixels sur 1 225) -- c'est
		// precisement ce qui a fait DECLARER le domaine.
		//
		// 🔴 CONSEQUENCE, ECRITE PLUTOT QUE TUE : dans cette version,
		// `displacementMax` N'ELARGIT PAS l'etendue. Son seul effet honore est la
		// GARDE DE DOMAINE, et (x2c) le mesure. Le jour ou la camera de portee sera
		// ecrite, ce parametre reprendra son role et cette note tombera.
		{
			NkProjectedGridParams p0 = pb;
			p0.displacementMax = 0.f;
			const NkProjectedGrid g0 = NkProjectedGridBuild(vpBas, oeilBas.oeil, p0);
			uint32 vus0 = 0;
			const uint32 trous0 = PixelsDecouverts(g0, g0.invViewProj, pb.displacementMax, 48u, vus0);
			std::fprintf(stderr,
						 "     (x2b) MEME pose, MEME marge, displacementMax 2 -> 0 : %u vus a la crete, %u decouverts\n",
						 vus0, trous0);
			XCHECK(g0.visible && trous0 == 0u,
				   "(x2b) DANS le domaine, la tranche ne change pas la couverture -- fait mesure, pas suppose");
		}

		// (x2c) ET LE PARAMETRE EST BIEN LU QUELQUE PART. Un parametre present et
		// sans aucun effet serait pire qu'absent : celui-ci deplace la FRONTIERE du
		// domaine, et ca se mesure des deux cotes.
		{
			const Pose oeil3 = {{0.f, 3.f, 0.f}, {0.f, 2.5f, -40.f}, "oeil a 3 m"};
			NkProjectedGridParams pPetit = p, pGrand = p;
			pPetit.displacementMax = 1.f; // 3 > 0 + 1  -> dedans
			pGrand.displacementMax = 5.f; // 3 < 0 + 5  -> dehors
			const NkProjectedGrid gPetit = NkProjectedGridBuild(VP(oeil3), oeil3.oeil, pPetit);
			const NkProjectedGrid gGrand = NkProjectedGridBuild(VP(oeil3), oeil3.oeil, pGrand);
			std::fprintf(stderr,
						 "     (x2c) oeil a 3 m : tranche 1 m -> visible=%d | tranche 5 m -> visible=%d horsDomaine=%d\n",
						 gPetit.visible ? 1 : 0, gGrand.visible ? 1 : 0, gGrand.horsDomaine ? 1 : 0);
			XCHECK(gPetit.visible && !gGrand.visible && gGrand.horsDomaine,
				   "(x2c) displacementMax EST lu : il deplace la frontiere du domaine, dans les deux sens");
		}
	}

	// (x3) DENSITE A L'ECRAN, et le chiffre qui justifie le concept.
	{
		// Le pas ecran des mailles de la grille projetee : par construction il est
		// constant, on le MESURE quand meme -- un « par construction » non mesure
		// est une intention.
		float32 pasMin = 1e30f, pasMax = 0.f;
		bool tousFinis = true;
		for (uint32 j = 0; j <= p.rows; j += 8u) {
			for (uint32 i = 0; i + 8u <= p.cols; i += 8u) {
				NkVec3f a, b;
				if (!NkProjectedGridVertex(g, p, i, j, a) || !NkProjectedGridVertex(g, p, i + 8u, j, b))
					continue;
				const NkVec4f qa = (vp * NkVec4f(a.x, a.y, a.z, 1.f));
				const NkVec4f qb = (vp * NkVec4f(b.x, b.y, b.z, 1.f));
				if (qa.w <= 1e-6f || qb.w <= 1e-6f)
					continue;
				const float32 dx = qb.x / qb.w - qa.x / qa.w;
				const float32 dy = qb.y / qb.w - qa.y / qa.w;
				const float32 d = NkSqrt(dx * dx + dy * dy);
				if (!EstFini(d))
					tousFinis = false;
				if (d < pasMin)
					pasMin = d;
				if (d > pasMax)
					pasMax = d;
			}
		}
		const float32 rapportProjete = (pasMin > 1e-9f) ? pasMax / pasMin : 0.f;

		// LA CONTRE-EPREUVE : une grille MONDE de meme budget, 400 m de cote,
		// centree sous la camera. C'est la solution qu'on remplace, et son rapport
		// de densite ecran est le chiffre qui dit pourquoi.
		float32 mMin = 1e30f, mMax = 0.f;
		for (uint32 j = 0; j <= p.rows; j += 8u) {
			for (uint32 i = 0; i + 8u <= p.cols; i += 8u) {
				const float32 pas = 400.f / (float32)p.cols;
				const NkVec3f a{-200.f + pas * (float32)i, 0.f, -200.f + pas * (float32)j};
				const NkVec3f b{-200.f + pas * (float32)(i + 8u), 0.f, -200.f + pas * (float32)j};
				const NkVec4f qa = (vp * NkVec4f(a.x, a.y, a.z, 1.f));
				const NkVec4f qb = (vp * NkVec4f(b.x, b.y, b.z, 1.f));
				if (qa.w <= 1e-6f || qb.w <= 1e-6f)
					continue;
				const float32 dx = qb.x / qb.w - qa.x / qa.w;
				const float32 dy = qb.y / qb.w - qa.y / qa.w;
				const float32 d = NkSqrt(dx * dx + dy * dy);
				if (d < mMin)
					mMin = d;
				if (d > mMax)
					mMax = d;
			}
		}
		const float32 rapportMonde = (mMin > 1e-9f) ? mMax / mMin : 0.f;
		std::fprintf(stderr,
					 "     (x3) pas ecran : grille PROJETEE min %.5f max %.5f -> rapport %.2f | grille MONDE rapport %.2f\n",
					 (double)pasMin, (double)pasMax, (double)rapportProjete, (double)rapportMonde);
		XCHECK(tousFinis && rapportProjete > 0.f && rapportProjete < 1.5f,
			   "(x3) la grille projetee a un pas ECRAN quasi constant (rapport < 1,5)");
		XCHECK(rapportMonde > 3.f * rapportProjete,
			   "(x3b) CONTRE-EPREUVE : la grille MONDE, meme budget, gaspille -- rapport bien pire");
	}

	// (x4) POSES DEGENEREES : aucun NaN, aucune position absurde.
	{
		const Pose poses[5] = {
			{{0.f, 8.f, 0.f}, {0.f, 8.f, -60.f}, "pile a l'horizon"},
			{{0.f, 30.f, 0.f}, {0.f, 0.f, -0.001f}, "plongee quasi verticale"},
			{{0.f, -3.f, 0.f}, {0.f, 1.f, -20.f}, "camera SOUS l'eau"},
			{{0.f, 8.f, 0.f}, {0.f, 60.f, -10.f}, "vers le ciel"},
			{{0.f, 0.f, 0.f}, {0.f, 0.f, -60.f}, "oeil DANS le plan"},
		};
		uint32 nan = 0, absurdes = 0, construites = 0;
		for (uint32 k = 0; k < 5u; ++k) {
			const NkMat4f v = VP(poses[k]);
			const NkProjectedGrid gg = NkProjectedGridBuild(v, poses[k].oeil, p);
			if (!gg.visible) {
				std::fprintf(stderr, "     (x4) %-24s : invisible (dit)\n", poses[k].nom);
				continue;
			}
			++construites;
			uint32 rendus = 0;
			for (uint32 j = 0; j <= p.rows; j += 4u) {
				for (uint32 i = 0; i <= p.cols; i += 4u) {
					NkVec3f m;
					if (!NkProjectedGridVertex(gg, p, i, j, m))
						continue;
					++rendus;
					if (!EstFini(m.x) || !EstFini(m.y) || !EstFini(m.z))
						++nan;
					else if (NkFabs(m.x) > 1e7f || NkFabs(m.z) > 1e7f)
						++absurdes;
				}
			}
			std::fprintf(stderr, "     (x4) %-24s : %u sommets rendus, etendue x[%.2f, %.2f]%s\n",
						 poses[k].nom, rendus, (double)gg.ndcMinX, (double)gg.ndcMaxX,
						 gg.repliPleinEcran ? " (repli)" : "");
		}
		std::fprintf(stderr, "     (x4) %u poses construites, %u NaN, %u positions > 10 000 km\n", construites,
					 nan, absurdes);
		XCHECK(nan == 0u, "(x4) aucune pose degeneree ne produit de NaN");
		XCHECK(absurdes == 0u, "(x4b) ni de position absurde : un rayon qui rate le plan REFUSE, il n'invente pas");
	}

	// (x5) INVISIBLE SE DIT.
	{
		const Pose enHaut = {{0.f, 500.f, 0.f}, {0.f, 1000.f, -1.f}, "loin au-dessus, vers le ciel"};
		const NkProjectedGrid gg = NkProjectedGridBuild(VP(enHaut), enHaut.oeil, p);
		std::fprintf(stderr, "     (x5) camera loin au-dessus regardant le ciel : visible=%d, points=%u\n",
					 gg.visible ? 1 : 0, gg.pointsRetenus);
		XCHECK(!gg.visible, "(x5) quand l'eau n'est pas dans le champ, la grille le DIT (pas de grille vide)");
	}

	// (x6) UNE INCOHERENCE QUE (x4) A FAIT SORTIR, ET QUE JE NOMME PLUTOT QUE DE
	// LA LISSER. Avec l'oeil EXACTEMENT dans le plan d'eau, la grille rend
	// `visible = true` et pourtant ZERO sommet : tous les rayons sont paralleles
	// au plan et se font refuser un par un. Les deux comportements sont justes
	// separement -- la tranche coupe bien le tronc, et un rayon parallele n'a pas
	// d'intersection -- mais mis bout a bout ils disent deux choses differentes.
	//
	// Ce que `visible` promet est donc PLUS FAIBLE que ce qu'on lit dedans : il
	// dit « la tranche d'eau coupe le tronc de vue », il ne dit PAS « des sommets
	// sortiront ». Je ne change pas le drapeau cette nuit (il faudrait decider si
	// l'oeil dans le plan doit etre repousse d'un epsilon, et ce genre de decision
	// se prend en voyant l'image) ; je BORNE le cas : c'est le SEUL ou ca arrive.
	{
		const Pose ok3[3] = {
			{{0.f, 8.f, 0.f}, {0.f, 2.f, -60.f}, "nominale"},
			{{0.f, 0.5f, 0.f}, {0.f, 0.4f, -60.f}, "oeil a 50 cm au-dessus"},
			{{0.f, -0.5f, 0.f}, {0.f, 0.6f, -60.f}, "oeil a 50 cm dessous"},
		};
		uint32 steriles = 0;
		for (uint32 k = 0; k < 3u; ++k) {
			const NkProjectedGrid gg = NkProjectedGridBuild(VP(ok3[k]), ok3[k].oeil, p);
			if (!gg.visible)
				continue;
			uint32 rendus = 0;
			for (uint32 j = 0; j <= p.rows; j += 4u)
				for (uint32 i = 0; i <= p.cols; i += 4u) {
					NkVec3f m;
					if (NkProjectedGridVertex(gg, p, i, j, m))
						++rendus;
				}
			std::fprintf(stderr, "     (x6) %-24s : %u sommets\n", ok3[k].nom, rendus);
			if (rendus == 0u)
				++steriles;
		}
		XCHECK(steriles == 0u,
			   "(x6) des que l'oeil quitte le plan, meme de 50 cm, la grille produit des sommets");
	}

	// (x7) LE REFUS HORS DOMAINE SE PROUVE. Une capacite qui declare refuser et
	// qu'on ne fait jamais refuser n'a qu'une intention pour garantie. Les trois
	// poses ci-dessous sont EXACTEMENT celles ou la mesure du 07/09 a montre que
	// la grille ne couvrait rien -- elles doivent maintenant etre refusees, et
	// le refus doit se LIRE (`horsDomaine`), pas se deviner d'un `visible` faux
	// qui voudrait dire autre chose ailleurs.
	{
		const Pose hors[3] = {
			{{0.f, 1.f, 0.f}, {0.f, 1.2f, -60.f}, "oeil a 1 m, crete a 2 m"},
			{{0.f, -3.f, 0.f}, {0.f, 1.f, -20.f}, "oeil SOUS l'eau"},
			{{0.f, 2.f, 0.f}, {0.f, 2.f, -60.f}, "oeil pile au niveau de la crete"},
		};
		uint32 refusees = 0, dites = 0;
		for (uint32 k = 0; k < 3u; ++k) {
			const NkProjectedGrid gg = NkProjectedGridBuild(VP(hors[k]), hors[k].oeil, p);
			if (!gg.visible)
				++refusees;
			if (gg.horsDomaine)
				++dites;
			std::fprintf(stderr, "     (x7) %-30s : visible=%d horsDomaine=%d\n", hors[k].nom,
						 gg.visible ? 1 : 0, gg.horsDomaine ? 1 : 0);
		}
		XCHECK(refusees == 3u && dites == 3u,
			   "(x7) les trois poses hors domaine sont REFUSEES, et le refus se lit (horsDomaine)");

		// Et la contre-epreuve du refus : une pose DEDANS ne doit pas etre refusee,
		// sinon la garde interdirait tout et personne ne s'en apercevrait.
		const Pose dedans = {{0.f, 6.f, 0.f}, {0.f, 5.f, -40.f}, "dedans"};
		const NkProjectedGrid gd = NkProjectedGridBuild(VP(dedans), dedans.oeil, p);
		XCHECK(gd.visible && !gd.horsDomaine,
			   "(x7b) CONTRE-EPREUVE : une pose DANS le domaine passe -- la garde n'interdit pas tout");
	}

	std::fprintf(stderr, "=== grille projetee : %d passes, %d echecs ===\n", gP, gF);
	return gF;
}
