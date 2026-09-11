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

	NkMat4f PROJ(float32 aspect = 16.f / 9.f) {
		return NkMat4f::Perspective(NkAngle(60.f), aspect, 0.1f, 2000.f);
	}

	NkMat4f VUE(const Pose &pose) {
		return NkMat4f::LookAt(pose.oeil, pose.cible, NkVec3f{0.f, 1.f, 0.f});
	}

	NkMat4f VP(const Pose &pose, float32 aspect = 16.f / 9.f) { return PROJ(aspect) * VUE(pose); }

	// La grille demande desormais la PROJECTION et la VUE separement au lieu de leur
	// produit : la camera de portee reutilise la projection du rendu SANS son
	// orientation. Ce raccourci garde les temoins lisibles malgre le changement.
	NkProjectedGrid BUILD(const Pose &pose, const NkProjectedGridParams &p,
						  float32 aspect = 16.f / 9.f) {
		return NkProjectedGridBuild(PROJ(aspect), VUE(pose), pose.oeil, p);
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

	// 🔴 ICI SE TROUVAIT `PixelsDecouverts`, ET SA DISPARITION EST UNE MESURE.
	//
	// Elle prenait `g.invViewProj` pour enumerer « les pixels » et comparait leur
	// NDC aux bornes de la grille. Tant que la grille sortait de la camera de
	// rendu, les deux etaient le meme ecran et c'etait juste. Le jour ou la camera
	// de portee est entree dans le calcul, `g.invViewProj` est devenu l'inverse de
	// la camera de PORTEE : la fonction s'est mise a enumerer les pixels d'une
	// camera qui n'affiche rien, et a les comparer a des bornes ecrites dans ce
	// meme ecran. Elle restait donc COHERENTE AVEC ELLE-MEME -- et vide.
	//
	// CE QUE CA A DONNE, MESURE : (x1) et (x2) sont restes VERTS a travers le
	// changement, mais leur nombre de pixels examines a bouge tout seul (1 225 ->
	// 1 274 a la crete). Un temoin vert dont la donnee change sans qu'on ait
	// touche a la donnee ne dit plus rien sur ce qu'il croit garder.
	//
	// La lecon est celle de l'instrument retourne : un temoin ne doit jamais lire
	// SA camera dans l'objet qu'il surveille, parce que l'objet peut en changer.
	// `PixelsDecouvertsPortee` exige donc qu'on lui DONNE la camera de rendu.
	// L'ancienne fonction n'est pas corrigee, elle est retiree : un instrument qui
	// ne peut plus dire la verite ne reste pas dans la boite a outils.

	// LE GACHIS : la fraction des sommets qui ne peuvent RIEN eclairer. Un sommet
	// est utile si, a l'une au moins des trois hauteurs que la houle lui permet
	// (repos, crete, creux), il tombe dans l'ecran du RENDU. Dehors aux trois,
	// aucune vague ne le rendra visible : il est paye et jamais vu.
	float32 Gachis(const NkProjectedGrid &g, const NkProjectedGridParams &p,
				   const NkMat4f &renderVP, uint32 pas, uint32 &total) {
		uint32 perdus = 0;
		total = 0;
		const float32 hauteurs[3] = {p.baseY, p.baseY + p.displacementMax,
									 p.baseY - p.displacementMax};
		for (uint32 j = 0; j <= p.rows; j += pas) {
			for (uint32 i = 0; i <= p.cols; i += pas) {
				NkVec3f m;
				if (!NkProjectedGridVertex(g, p, i, j, m))
					continue;
				++total;
				bool vu = false;
				for (uint32 h = 0; h < 3u && !vu; ++h) {
					const NkVec4f q = renderVP * NkVec4f(m.x, hauteurs[h], m.z, 1.f);
					if (q.w <= 1e-6f)
						continue;
					const float32 iw = 1.f / q.w;
					const float32 nx = q.x * iw, ny = q.y * iw;
					if (nx >= -1.f && nx <= 1.f && ny >= -1.f && ny <= 1.f)
						vu = true;
				}
				if (!vu)
					++perdus;
			}
		}
		return (total > 0u) ? (float32)perdus / (float32)total : 0.f;
	}

	// Fait tourner la camera autour de son oeil, d'un angle donne en degres.
	Pose TournerYaw(const Pose &pose, float32 degres) {
		const NkVec3f d = pose.cible - pose.oeil;
		const NkVec4f q = NkMat4f::RotationY(NkAngle(degres)) * NkVec4f(d.x, d.y, d.z, 0.f);
		Pose out = pose;
		out.cible = pose.oeil + NkVec3f{q.x, q.y, q.z};
		return out;
	}

	// LE *SWIMMING*, MESURE COMME UNE DERIVEE. C'est le vrai defaut visuel de
	// cette famille de techniques : les sommets GLISSENT sur l'eau quand la camera
	// bouge, et si ce glissement SAUTE, la surface frissonne.
	//
	// On ne peut pas juger « ca frissonne » sans ecran -- mais on peut juger la
	// CONTINUITE. Un deplacement continu est proportionnel a l'angle : divisez
	// l'angle par quatre, le deplacement doit etre divise par quatre. Un saut, lui,
	// ne se divise pas. On rapporte le glissement a la taille de la maille pour que
	// le nombre ne depende ni de l'echelle ni de la distance.
	float32 GlissementMoyen(const NkProjectedGrid &ga, const NkProjectedGrid &gb,
							const NkProjectedGridParams &p, uint32 pas) {
		float64 somme = 0.0;
		uint32 nb = 0;
		for (uint32 j = 0; j <= p.rows; j += pas) {
			for (uint32 i = 0; i + pas <= p.cols; i += pas) {
				NkVec3f a, b, voisin;
				if (!NkProjectedGridVertex(ga, p, i, j, a))
					continue;
				if (!NkProjectedGridVertex(gb, p, i, j, b))
					continue;
				if (!NkProjectedGridVertex(ga, p, i + pas, j, voisin))
					continue;
				const float32 maille = (voisin - a).Len();
				if (maille < 1e-6f)
					continue;
				somme += (float64)((b - a).Len() / maille);
				++nb;
			}
		}
		return (nb > 0u) ? (float32)(somme / (float64)nb) : 0.f;
	}

	// L'ETENDUE au sol reellement maillee (m²) : la boite englobante en (x, z) des
	// sommets QUI EXISTENT. C'est la mesure de « l'etendue » dont parle le lot ② --
	// une etendue se compare en metres carres, pas en impressions.
	//
	// ⚠️ LA BOITE PLUTOT QUE LES QUATRE COINS, ET CE N'ETAIT PAS UN DETAIL. La
	// premiere version prenait l'aire du quadrilatere des quatre coins. Sur le
	// chemin d'origine, les coins HAUTS de l'etendue visent au-dessus de l'horizon
	// et n'ont aucune intersection avec le plan : la fonction rendait 0 m². Elle
	// confondait « etendue nulle » et « coin manquant », et faisait rougir (x11b)
	// pour une raison qui n'avait rien a voir avec ce qu'il mesure.
	float32 AireEmpreinte(const NkProjectedGrid &g, const NkProjectedGridParams &p,
						  uint32 &sommets) {
		float32 mnx = 1e30f, mxx = -1e30f, mnz = 1e30f, mxz = -1e30f;
		sommets = 0;
		for (uint32 j = 0; j <= p.rows; j += 2u) {
			for (uint32 i = 0; i <= p.cols; i += 2u) {
				NkVec3f m;
				if (!NkProjectedGridVertex(g, p, i, j, m))
					continue;
				++sommets;
				if (m.x < mnx)
					mnx = m.x;
				if (m.x > mxx)
					mxx = m.x;
				if (m.z < mnz)
					mnz = m.z;
				if (m.z > mxz)
					mxz = m.z;
			}
		}
		return (sommets > 0u) ? (mxx - mnx) * (mxz - mnz) : 0.f;
	}

	// LA MEME QUESTION, MAIS POSEE DANS LE BON ECRAN.
	//
	// `PixelsDecouverts` compare le NDC d'un pixel de RENDU aux bornes de la
	// grille. Ce n'est juste que tant que l'etendue est ecrite dans l'ecran du
	// rendu -- ce qui etait vrai, et implicite. Des qu'une camera de PORTEE entre
	// dans le calcul, les deux NDC ne sont plus le meme espace et la comparaison
	// cesse d'avoir un sens, SANS RIEN CASSER DE VISIBLE : elle continuerait a
	// rendre des nombres plausibles.
	//
	// Ici : on part toujours d'un pixel de RENDU -- c'est bien lui qui decide de ce
	// qu'on VOIT -- on trouve le point d'eau qu'il voit, on le rabat sur le plan de
	// repos exactement comme la grille le fait, et on le projette dans l'ecran de
	// la camera dont l'etendue parle. Un pixel qui voit de l'eau et dont le point
	// tombe hors de l'etendue est un TROU DANS L'OCEAN.
	//
	// Une grille absente (`visible` faux) rend TOUS les pixels decouverts. Ce n'est
	// pas une punition, c'est le sens de la mesure : si la grille refuse de se
	// construire, il n'y a effectivement aucun sommet a l'ecran.
	uint32 PixelsDecouvertsPortee(const NkProjectedGrid &g, const NkProjectedGridParams &p,
								  const NkMat4f &renderInvVP, float32 py, uint32 N, uint32 &vus) {
		uint32 trous = 0;
		vus = 0;
		for (uint32 i = 0; i <= N; ++i) {
			for (uint32 j = 0; j <= N; ++j) {
				const float32 nx = -1.f + 2.f * (float32)i / (float32)N;
				const float32 ny = -1.f + 2.f * (float32)j / (float32)N;
				NkVec3f m;
				if (!PixelVoitLePlan(renderInvVP, nx, ny, py, m))
					continue;
				++vus;
				if (!g.visible) {
					++trous;
					continue;
				}
				const NkVec4f q = g.rangeViewProj * NkVec4f(m.x, p.baseY, m.z, 1.f);
				if (q.w <= 1e-6f) {
					++trous; // derriere la camera de portee : aucun sommet ne tombera la
					continue;
				}
				const float32 iw = 1.f / q.w;
				const float32 gx = q.x * iw, gy = q.y * iw;
				const float32 eps = 1e-4f;
				if (gx < g.ndcMinX - eps || gx > g.ndcMaxX + eps || gy < g.ndcMinY - eps ||
					gy > g.ndcMaxY + eps)
					++trous;
			}
		}
		return trous;
	}

	// La COUVERTURE : la fraction des pixels qui voient de l'eau et que la grille
	// atteint. 1,00 = aucun trou. Sur un echantillon VIDE elle vaut 0 et non 1 :
	// une moyenne sur rien n'est pas un succes, c'est une absence de mesure, et
	// l'appelant doit verifier `vus > 0` separement.
	float32 Couverture(uint32 vus, uint32 trous) {
		return (vus > 0u) ? (float32)(vus - trous) / (float32)vus : 0.f;
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
	NkProjectedGrid g = BUILD(nominale, p);

	std::fprintf(stderr, "     (x0) %u points retenus, etendue NDC x[%.3f, %.3f] y[%.3f, %.3f]%s\n",
				 g.pointsRetenus, (double)g.ndcMinX, (double)g.ndcMaxX, (double)g.ndcMinY,
				 (double)g.ndcMaxY, g.repliPleinEcran ? "  (repli plein ecran)" : "");
	XCHECK(g.visible && g.pointsRetenus > 0u, "(x0) pose nominale : la grille se construit et le dit");

	// (x1) COUVERTURE DU PLAN DE REPOS.
	{
		uint32 vus = 0;
		// La camera de RENDU est donnee explicitement : c'est elle qui decide de ce
		// qu'on voit, et elle ne se lit pas dans la grille.
		const uint32 trous = PixelsDecouvertsPortee(g, p, vp.Inverse(), p.baseY, 48u, vus);
		std::fprintf(stderr,
					 "     (x1) plan de repos : %u pixels d'eau echantillonnes, %u decouverts\n", vus,
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
		NkProjectedGridParams pb = p;
		pb.edgeBias = 0.f; // LA SEULE VARIABLE sera displacementMax
		pb.displacementMax = 2.f;
		const NkProjectedGrid gb = BUILD(oeilBas, pb);

		const NkMat4f invBas = VP(oeilBas).Inverse();
		uint32 vusH = 0, vusB = 0;
		const uint32 trousH =
			PixelsDecouvertsPortee(gb, pb, invBas, pb.baseY + pb.displacementMax, 48u, vusH);
		const uint32 trousB =
			PixelsDecouvertsPortee(gb, pb, invBas, pb.baseY - pb.displacementMax, 48u, vusB);
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
			const NkProjectedGrid g0 = BUILD(oeilBas, p0);
			uint32 vus0 = 0;
			const uint32 trous0 =
				PixelsDecouvertsPortee(g0, p0, invBas, pb.displacementMax, 48u, vus0);
			const float32 couv0 = Couverture(vus0, trous0);
			std::fprintf(stderr,
						 "     (x2b) MEME pose, MEME marge, displacementMax 2 -> 0 : %u vus a la crete,"
						 " %u DECOUVERTS -> couverture %.2f\n",
						 vus0, trous0, (double)couv0);
			XCHECK(g0.visible && trous0 > 0u,
				   "(x2b) displacementMax ELARGIT l'etendue : l'ecraser OUVRE des trous a la crete");
		}

		// (x2c) ET LE PARAMETRE EST BIEN LU QUELQUE PART. Un parametre present et
		// sans aucun effet serait pire qu'absent : celui-ci deplace la FRONTIERE du
		// domaine, et ca se mesure des deux cotes.
		{
			// ⚠️ CE TEMOIN PORTE DESORMAIS SUR LE CHEMIN D'ORIGINE, ET C'EST LE SUJET.
			// Il mesurait la frontiere du domaine ; la camera de portee SUPPRIME cette
			// frontiere. Le garder tel quel l'aurait fait rougir pour la meilleure des
			// raisons. On le pointe donc explicitement sur `rangeCamera = false`, ou
			// la frontiere existe encore, et (x7c) mesure l'autre cote.
			const Pose oeil3 = {{0.f, 3.f, 0.f}, {0.f, 2.5f, -40.f}, "oeil a 3 m"};
			NkProjectedGridParams pPetit = p, pGrand = p;
			pPetit.rangeCamera = false;
			pGrand.rangeCamera = false;
			pPetit.displacementMax = 1.f; // 3 > 0 + 1  -> dedans
			pGrand.displacementMax = 5.f; // 3 < 0 + 5  -> dehors
			const NkProjectedGrid gPetit = BUILD(oeil3, pPetit);
			const NkProjectedGrid gGrand = BUILD(oeil3, pGrand);
			std::fprintf(stderr,
						 "     (x2c) SANS portee, oeil a 3 m : tranche 1 m -> visible=%d | tranche 5 m ->"
						 " visible=%d horsDomaine=%d\n",
						 gPetit.visible ? 1 : 0, gGrand.visible ? 1 : 0, gGrand.horsDomaine ? 1 : 0);
			XCHECK(gPetit.visible && !gGrand.visible && gGrand.horsDomaine,
				   "(x2c) SANS camera de portee, displacementMax deplace la frontiere du domaine");
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
			const NkProjectedGrid gg = BUILD(poses[k], p);
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
		const NkProjectedGrid gg = BUILD(enHaut, p);
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
			const NkProjectedGrid gg = BUILD(ok3[k], p);
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
		// SANS camera de portee, le refus doit tenir : c'est le comportement d'origine
		// et il reste REPRODUCTIBLE, sinon la correction ne serait plus prouvable.
		NkProjectedGridParams pSans = p;
		pSans.rangeCamera = false;
		uint32 refusees = 0, dites = 0;
		for (uint32 k = 0; k < 3u; ++k) {
			const NkProjectedGrid gg = BUILD(hors[k], pSans);
			if (!gg.visible)
				++refusees;
			if (gg.horsDomaine)
				++dites;
			std::fprintf(stderr, "     (x7) SANS portee | %-30s : visible=%d horsDomaine=%d\n",
						 hors[k].nom, gg.visible ? 1 : 0, gg.horsDomaine ? 1 : 0);
		}
		XCHECK(refusees == 3u && dites == 3u,
			   "(x7) SANS portee, les trois poses hors domaine sont REFUSEES, et le refus se lit");

		// Et la contre-epreuve du refus : une pose DEDANS ne doit pas etre refusee,
		// sinon la garde interdirait tout et personne ne s'en apercevrait.
		const Pose dedans = {{0.f, 6.f, 0.f}, {0.f, 5.f, -40.f}, "dedans"};
		const NkProjectedGrid gd = BUILD(dedans, pSans);
		XCHECK(gd.visible && !gd.horsDomaine,
			   "(x7b) CONTRE-EPREUVE : une pose DANS le domaine passe -- la garde n'interdit pas tout");

		// (x7c) ET AVEC LA CAMERA DE PORTEE, LES MEMES TROIS POSES SONT SERVIES.
		// Ce n'est pas « elles ne sont plus refusees » -- une grille peut tres bien
		// se declarer visible et ne rien couvrir, c'est exactement le piege que le
		// domaine avait ete declare pour eviter. On exige donc la COUVERTURE, a la
		// hauteur ou chaque pose fait mal : la crete.
		// ⚠️ ON SONDE LES TROIS HAUTEURS, ET PAS SEULEMENT LA CRETE -- parce que la
		// donnee d'essai peut ne pas pouvoir exprimer la question. « Oeil pile au
		// niveau de la crete » regarde a l'horizontale DANS le plan y = +2 : aucun
		// rayon ne peut rencontrer ce plan, donc ZERO pixel le voit. Mesure sur une
		// population vide, la couverture valait 0,00 et faisait rougir le temoin pour
		// une raison qui ne concerne pas la grille. C'est la lecon du 07/09, une
		// deuxieme fois : un critere juste sur une donnee muette ne prouve rien.
		//
		// On exige donc 1,00 a CHAQUE hauteur reellement visible, et au moins une.
		uint32 servies = 0;
		for (uint32 k = 0; k < 3u; ++k) {
			const NkProjectedGrid gg = BUILD(hors[k], p);
			const NkMat4f invR = VP(hors[k]).Inverse();
			const float32 hauteurs[3] = {p.baseY + p.displacementMax, p.baseY,
										 p.baseY - p.displacementMax};
			uint32 mesurees = 0, bonnes = 0, vusTotal = 0;
			for (uint32 h = 0; h < 3u; ++h) {
				uint32 vus = 0;
				const uint32 trous = PixelsDecouvertsPortee(gg, p, invR, hauteurs[h], 48u, vus);
				if (vus == 0u)
					continue; // cette hauteur n'est pas visible depuis cette pose
				++mesurees;
				vusTotal += vus;
				if (Couverture(vus, trous) > 0.999f)
					++bonnes;
			}
			std::fprintf(stderr,
						 "     (x7c) AVEC portee | %-30s : visible=%d remontee=%d | %u pixels sur"
						 " %u hauteurs visibles, %u a 1,00\n",
						 hors[k].nom, gg.visible ? 1 : 0, gg.porteeRemontee ? 1 : 0, vusTotal,
						 mesurees, bonnes);
			if (gg.visible && mesurees > 0u && bonnes == mesurees)
				++servies;
		}
		XCHECK(servies == 3u,
			   "(x7c) AVEC la camera de portee, les trois poses d'hier sont COUVERTES a 1,00");
	}

	// (x8) LE CAS QUI DOIT ROUGIR AVANT DE VERDIR : OEIL A 1 M, CRETE A 2 M.
	//
	// C'est la pose exacte qui a fait DECLARER le domaine le 07/09 : la crete passe
	// au-dessus de l'oeil, elle se voit donc AU-DESSUS de l'horizon du plan de
	// repos, et la grille batie depuis la camera de rendu n'a aucun sommet a poser
	// la. Mesure d'alors : ZERO pixel couvert sur 1 225.
	//
	// Le meme instrument qui a trouve ce trou doit maintenant prouver qu'il est
	// bouche. C'est la meme mesure, dans les deux sens : elle vaut 0,00 tant que la
	// camera de portee n'est pas posee, et elle doit valoir 1,00 apres.
	{
		const Pose ras = {{0.f, 1.f, 0.f}, {0.f, 1.2f, -60.f}, "oeil a 1 m, crete a 2 m"};
		const NkProjectedGrid gr = BUILD(ras, p);
		const NkMat4f invRendu = VP(ras).Inverse();
		uint32 vus = 0;
		const uint32 trous =
			PixelsDecouvertsPortee(gr, p, invRendu, p.baseY + p.displacementMax, 48u, vus);
		const float32 couv = Couverture(vus, trous);
		std::fprintf(stderr,
					 "     (x8) oeil a 1 m, crete a 2 m : %u pixels voient la crete, %u decouverts"
					 " -> COUVERTURE %.2f (visible=%d horsDomaine=%d)\n",
					 vus, trous, (double)couv, gr.visible ? 1 : 0, gr.horsDomaine ? 1 : 0);
		XCHECK(vus > 0u && couv > 0.999f,
			   "(x8) COUVERTURE 1,00 a la crete, camera au ras de l'eau");
	}

	// (x9) LE GACHIS. La couverture seule se triche : il suffit de mailler la
	// terre entiere pour ne jamais laisser de trou. Le second nombre est donc le
	// prix -- la fraction des sommets qui ne peuvent RIEN eclairer, a aucune des
	// hauteurs que la houle leur permet.
	//
	// LE SEUIL EST ANNONCE AVANT LA MESURE : moins de la moitie. Une grille qui
	// depense plus d'un sommet sur deux hors champ ne merite pas le nom de grille
	// projetee. Si le nombre le depasse, c'est le seuil qui a raison.
	{
		const Pose poses[3] = {
			{{0.f, 8.f, 0.f}, {0.f, 2.f, -60.f}, "nominale, oeil a 8 m"},
			{{0.f, 1.f, 0.f}, {0.f, 1.2f, -60.f}, "au ras de l'eau, oeil a 1 m"},
			{{0.f, -3.f, 0.f}, {0.f, 1.f, -20.f}, "sous l'eau, oeil a -3 m"},
		};
		float32 pire = 0.f, gachisNominal = 0.f;
		for (uint32 k = 0; k < 3u; ++k) {
			const NkProjectedGrid gg = BUILD(poses[k], p);
			uint32 total = 0;
			const float32 gach = Gachis(gg, p, VP(poses[k]), 2u, total);
			if (gach > pire)
				pire = gach;
			if (k == 0u)
				gachisNominal = gach;
			std::fprintf(stderr, "     (x9) %-28s : %u sommets, gachis %.2f\n", poses[k].nom, total,
						 (double)gach);
		}
		XCHECK(pire < 0.50f,
			   "(x9) le gachis reste borne : moins d'un sommet sur deux tombe hors champ");

		// CONTRE-EPREUVE A UNE SEULE VARIABLE : meme pose, meme grille, meme tout --
		// sauf la camera de portee, juchee a 200 m. Elle embrasse alors bien plus
		// large que le rendu, et la grille depense ses sommets ailleurs. Si ce nombre
		// ne bougeait pas, c'est qu'il ne mesurait pas le placement de la portee.
		NkProjectedGridParams pHaut = p;
		pHaut.rangeElevation = 200.f;
		const NkProjectedGrid gh = BUILD(poses[0], pHaut);
		uint32 totalH = 0;
		const float32 gachH = Gachis(gh, pHaut, VP(poses[0]), 2u, totalH);
		// ⚠️ LA COMPARAISON SE FAIT AVEC LA MEME POSE, et c'est tout le sujet. La
		// premiere version comparait ce nombre au PIRE des trois poses : elle
		// opposait la pose nominale juchee a 200 m (0,45) au ras de l'eau pose a 0,5 m
		// (0,92) et concluait que la mutation n'avait rien fait. Elle changeait DEUX
		// choses a la fois -- l'elevation ET la pose -- et ne departageait rien.
		std::fprintf(stderr,
					 "     (x9b) MEME pose nominale, portee juchee a 200 m : %u sommets, gachis"
					 " %.2f (contre %.2f a 0,5 m)\n",
					 totalH, (double)gachH, (double)gachisNominal);
		XCHECK(gachH > gachisNominal,
			   "(x9b) CONTRE-EPREUVE : une portee mal placee AUGMENTE le gachis -- la mesure bouge");
	}

	// (x10) LA STABILITE -- le *swimming*, mesure comme une derivee.
	//
	// C'est le vrai defaut visuel de cette technique, et le seul des trois qu'on
	// serait tente de juger a l'oeil. On ne le juge pas : un deplacement continu
	// est PROPORTIONNEL a l'angle. Divisez la rotation par quatre, le glissement
	// doit etre divise par quatre. Un saut, lui, ne se divise pas.
	{
		const Pose bancs[2] = {
			{{0.f, 8.f, 0.f}, {0.f, 2.f, -60.f}, "nominale, oeil a 8 m"},
			{{0.f, 1.f, 0.f}, {0.f, 1.2f, -60.f}, "au ras de l'eau, oeil a 1 m"},
		};
		uint32 lisses = 0;
		for (uint32 k = 0; k < 2u; ++k) {
			const NkProjectedGrid gRepos = BUILD(bancs[k], p);
			const NkProjectedGrid gUn = BUILD(TournerYaw(bancs[k], 1.f), p);
			const NkProjectedGrid gQuart = BUILD(TournerYaw(bancs[k], 0.25f), p);
			const float32 dUn = GlissementMoyen(gRepos, gUn, p, 4u);
			const float32 dQuart = GlissementMoyen(gRepos, gQuart, p, 4u);
			const float32 rapport = (dQuart > 1e-9f) ? dUn / dQuart : 0.f;
			std::fprintf(stderr,
						 "     (x10) %-28s : glissement 1 deg %.4f maille | 0,25 deg %.4f"
						 " -> rapport %.2f (4,00 = continu)\n",
						 bancs[k].nom, (double)dUn, (double)dQuart, (double)rapport);
			if (dUn > 1e-6f && rapport > 3.f && rapport < 5.f)
				++lisses;
		}
		XCHECK(lisses == 2u,
			   "(x10) le glissement est PROPORTIONNEL a l'angle : aucun saut entre deux images voisines");

		// CONTRE-EPREUVE : si la camera de portee SAUTAIT d'une image a l'autre, ce
		// nombre doit le voir. On lui fournit donc le saut -- meme rotation de 0,25
		// degre, mais l'elevation de portee passe de 0,5 m a 5 m entre les deux
		// images. Le glissement cesse d'etre proportionnel et le rapport quitte la
		// bande. Un temoin de continuite qui ne rougit pas sur une discontinuite
		// fabriquee ne surveille rien.
		//
		// ⚠️ ET LE SAUT DOIT SE PRODUIRE SUR LA POSE OU L'ELEVATION MORD. Premiere
		// version : le saut etait fabrique sur la pose nominale, oeil a 8 m. Or la
		// portee s'y place a max(8 ; 0 + 2 + elevation) -- 0,5 m comme 5 m restent
		// sous 8, donc les DEUX images sortaient de la MEME camera. La mutation ne
		// touchait pas le chemin que le temoin emprunte, et elle est restee VERTE a
		// 4,00. Ici, oeil a 1 m : la portee passe de 2,5 m a 7 m, et le saut existe.
		NkProjectedGridParams pSaut = p;
		pSaut.rangeElevation = 5.f;
		const NkProjectedGrid gA = BUILD(bancs[1], p);
		const NkProjectedGrid gB = BUILD(TournerYaw(bancs[1], 1.f), p);
		const NkProjectedGrid gJ = BUILD(TournerYaw(bancs[1], 0.25f), pSaut);
		const float32 dRef = GlissementMoyen(gA, gB, p, 4u);
		const float32 dJ = GlissementMoyen(gA, gJ, p, 4u);
		const float32 rapportSaut = (dJ > 1e-9f) ? dRef / dJ : 0.f;
		std::fprintf(stderr, "     (x10b) portee qui SAUTE (0,5 -> 5 m) : rapport %.2f\n",
					 (double)rapportSaut);
		XCHECK(rapportSaut <= 3.f || rapportSaut >= 5.f,
			   "(x10b) CONTRE-EPREUVE : un saut de la camera de portee SORT de la bande");
	}

	// (x11) LOT ② : `displacementMax` ELARGIT-IL ENFIN L'ETENDUE, ET DE COMBIEN ?
	//
	// Le 07/09 la reponse mesuree etait NON : le rabattement sur le plan de repos
	// effacait la contribution de la tranche, et le seul effet honore du parametre
	// etait la garde de domaine. Une etendue se compare en METRES CARRES.
	{
		const Pose banc = {{0.f, 6.f, 0.f}, {0.f, 5.f, -40.f}, "oeil a 6 m"};
		const float32 tranches[3] = {0.f, 2.f, 5.f};
		float32 aires[3] = {0.f, 0.f, 0.f};
		for (uint32 k = 0; k < 3u; ++k) {
			NkProjectedGridParams pk = p;
			pk.displacementMax = tranches[k];
			const NkProjectedGrid gk = BUILD(banc, pk);
			uint32 nk = 0;
			aires[k] = AireEmpreinte(gk, pk, nk);
			std::fprintf(stderr, "     (x11) tranche +/- %.1f m -> empreinte %.0f m2 (%u sommets)\n",
						 (double)tranches[k], (double)aires[k], nk);
		}
		const float32 gain = (aires[0] > 1e-3f) ? aires[1] / aires[0] : 0.f;
		std::fprintf(stderr, "     (x11) elargissement tranche 0 -> 2 m : x%.2f\n", (double)gain);
		XCHECK(aires[0] > 0.f && aires[1] > aires[0] && aires[2] > aires[1],
			   "(x11) displacementMax ELARGIT l'etendue, et l'elargissement CROIT avec la tranche");

		// CONTRE-EPREUVE : le fait du 07/09, REJOUE. Sans camera de portee, le meme
		// parametre n'elargit rien du tout -- a l'arrondi pres, la meme empreinte.
		NkProjectedGridParams pS0 = p, pS2 = p;
		pS0.rangeCamera = false;
		pS2.rangeCamera = false;
		pS0.displacementMax = 0.f;
		pS2.displacementMax = 2.f;
		uint32 nS0 = 0, nS2 = 0;
		const float32 aS0 = AireEmpreinte(BUILD(banc, pS0), pS0, nS0);
		const float32 aS2 = AireEmpreinte(BUILD(banc, pS2), pS2, nS2);
		std::fprintf(stderr,
					 "     (x11b) SANS portee : tranche 0 -> %.0f m2 (%u sommets) | tranche 2 ->"
					 " %.0f m2 (%u sommets)\n",
					 (double)aS0, nS0, (double)aS2, nS2);
		XCHECK(aS0 > 0.f && NkFabs(aS2 - aS0) < 1e-3f * aS0,
			   "(x11b) CONTRE-EPREUVE : SANS portee, displacementMax n'elargit RIEN (le fait du 07/09)");
	}

	std::fprintf(stderr, "=== grille projetee : %d passes, %d echecs ===\n", gP, gF);
	return gF;
}
