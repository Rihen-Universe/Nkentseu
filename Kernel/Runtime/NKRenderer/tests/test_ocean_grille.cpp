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
#include "NKVFX/NkWaterMeshBuilder.h" // le PRODUCTEUR, eprouve sans fenetre
#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::math;

namespace {

	int gP = 0, gF = 0;

	// LE PLANCHER DE COMPARAISONS sous lequel un temoin de stabilite ne conclut
	// PAS. Avec un pas de 4 sur une grille 64x64, une image pleine en offre ~272 ;
	// en dessous de 32 (un peu plus d'un huitieme), la grille a trop change entre
	// les deux images pour qu'une moyenne veuille dire quoi que ce soit. Le cas
	// sort alors INDETERMINE -- ni vert ni rouge. Un vert obtenu sur zero
	// comparaison serait un faux vert, et un faux vert est pire qu'un rouge.
	const uint32 kPlancherComparables = 32u;

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

	// Combien de sommets ont pu etre COMPARES entre deux images. Meme logique de
	// rejet que `GlissementMoyen`, mot pour mot.
	//
	// ⚠️ UN GLISSEMENT DE 0,00 NE VEUT PAS DIRE « parfaitement stable » : il peut
	// vouloir dire « aucun sommet comparable ». Les deux se ressemblent a l'oeil nu
	// et ne disent pas du tout la meme chose -- c'est le meme piege qu'une
	// couverture de 1,00 sur une population vide, deja paye deux fois cette nuit.
	uint32 SommetsComparables(const NkProjectedGrid &ga, const NkProjectedGrid &gb,
							  const NkProjectedGridParams &p, uint32 pas) {
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
				if ((voisin - a).Len() < 1e-6f)
					continue;
				++nb;
			}
		}
		return nb;
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

	// Le rapport entre la PLUS GRANDE et la PLUS PETITE maille A L'ECRAN DU RENDU.
	// 1,00 = densite parfaitement uniforme, et c'est tout l'apport de la grille
	// projetee : depenser un sommet par bloc de pixels, pas par metre carre.
	float32 RapportPasEcran(const NkProjectedGrid &g, const NkProjectedGridParams &p,
							const NkMat4f &vpRendu) {
		float32 mn = 1e30f, mx = 0.f;
		for (uint32 j = 0; j <= p.rows; j += 8u) {
			for (uint32 i = 0; i + 8u <= p.cols; i += 8u) {
				NkVec3f a, b;
				if (!NkProjectedGridVertex(g, p, i, j, a) ||
					!NkProjectedGridVertex(g, p, i + 8u, j, b))
					continue;
				const NkVec4f qa = vpRendu * NkVec4f(a.x, a.y, a.z, 1.f);
				const NkVec4f qb = vpRendu * NkVec4f(b.x, b.y, b.z, 1.f);
				if (qa.w <= 1e-6f || qb.w <= 1e-6f)
					continue;
				const float32 dx = qb.x / qb.w - qa.x / qa.w;
				const float32 dy = qb.y / qb.w - qa.y / qa.w;
				const float32 d = NkSqrt(dx * dx + dy * dy);
				if (d < mn)
					mn = d;
				if (d > mx)
					mx = d;
			}
		}
		return (mn > 1e-9f) ? mx / mn : 0.f;
	}

	// ── LES QUATRE NOMBRES QUI DISENT QU'UN MAILLAGE EST BIEN FORME ──────────
	// Aucun d'eux ne regarde une image : ce sont des comptes sur des indices. Un
	// maillage qui les satisfait n'a pas de fissures.
	struct NkPavageMesure {
			uint32 indices = 0u;
			uint32 aretesUneFois = 0u; // vues une seule fois = le BORD du maillage
			uint32 aretesTrois = 0u;   // vues trois fois ou plus : impossible si sain
			uint32 orientationPos = 0u, orientationNeg = 0u;
			uint32 degeneres = 0u;
			uint32 horsBornes = 0u;
	};

	NkPavageMesure MesurerPavage(const uint32 *idx, uint32 n, uint32 cols, uint32 rows) {
		NkPavageMesure m;
		m.indices = n;
		const uint32 nx = cols + 1u;
		const uint32 sommets = nx * (rows + 1u);
		const uint32 tris = n / 3u;
		for (uint32 t = 0; t < tris; ++t) {
			const uint32 a = idx[t * 3u], b = idx[t * 3u + 1u], c = idx[t * 3u + 2u];
			if (a >= sommets || b >= sommets || c >= sommets) {
				++m.horsBornes;
				continue;
			}
			if (a == b || b == c || a == c) {
				++m.degeneres;
				continue;
			}
			// ⚠️ L'AIRE SIGNEE SE PREND EN ESPACE PARAMETRE (i, j) : elle y est
			// ENTIERE, donc « aire nulle » est exact et ne demande AUCUN epsilon a
			// choisir. Un seuil arbitraire de plus serait un seuil de plus a defendre.
			const int32 ax = (int32)(a % nx), ay = (int32)(a / nx);
			const int32 bx = (int32)(b % nx), by = (int32)(b / nx);
			const int32 cx = (int32)(c % nx), cy = (int32)(c / nx);
			const int32 aire2 = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
			if (aire2 > 0)
				++m.orientationPos;
			else if (aire2 < 0)
				++m.orientationNeg;
			else
				++m.degeneres; // trois sommets alignes
		}
		// Les aretes, comptees SANS conteneur associatif : le depot est zero-STL, et
		// sur 384 aretes une double boucle exacte vaut mieux qu'une table de hachage
		// ecrite a trois heures du matin.
		uint32 ea[1200], eb[1200];
		uint32 ne = 0u;
		for (uint32 t = 0; t < tris && ne + 3u <= 1200u; ++t) {
			const uint32 v[3] = {idx[t * 3u], idx[t * 3u + 1u], idx[t * 3u + 2u]};
			for (uint32 e = 0; e < 3u; ++e) {
				uint32 a = v[e], b = v[(e + 1u) % 3u];
				if (a > b) {
					const uint32 s = a;
					a = b;
					b = s;
				}
				ea[ne] = a;
				eb[ne] = b;
				++ne;
			}
		}
		for (uint32 i = 0; i < ne; ++i) {
			uint32 c = 0u;
			for (uint32 j = 0; j < ne; ++j)
				if (ea[j] == ea[i] && eb[j] == eb[i])
					++c;
			if (c == 1u)
				++m.aretesUneFois;
			else if (c >= 3u)
				++m.aretesTrois;
		}
		return m;
	}

	float32 Croix(const float32 *px, const float32 *py, uint32 a, uint32 b, uint32 c) {
		return (px[b] - px[a]) * (py[c] - py[a]) - (py[b] - py[a]) * (px[c] - px[a]);
	}

	// L'ENVELOPPE CONVEXE du nuage (Andrew, chaine monotone), au plus 40 points.
	// C'est la FORME REELLE de l'eau vue, dont l'etendue rectangulaire n'est qu'un
	// majorant. Rend le nombre de sommets, remplis dans l'ordre du tour.
	uint32 EnveloppeConvexe(const float32 *px, const float32 *py, uint32 n, uint32 *enveloppe) {
		if (n < 3u || n > 40u)
			return 0u;
		uint32 ordre[40];
		for (uint32 i = 0; i < n; ++i)
			ordre[i] = i;
		for (uint32 i = 1u; i < n; ++i) { // tri par insertion sur (x, puis y)
			const uint32 k = ordre[i];
			uint32 j = i;
			while (j > 0u && (px[ordre[j - 1u]] > px[k] ||
							  (px[ordre[j - 1u]] == px[k] && py[ordre[j - 1u]] > py[k]))) {
				ordre[j] = ordre[j - 1u];
				--j;
			}
			ordre[j] = k;
		}
		uint32 H[82];
		uint32 k = 0u;
		for (uint32 t = 0; t < n; ++t) { // chaine basse
			const uint32 i = ordre[t];
			while (k >= 2u && Croix(px, py, H[k - 2u], H[k - 1u], i) <= 1e-12f)
				--k;
			H[k++] = i;
		}
		const uint32 seuil = k + 1u;
		for (uint32 t = n - 1u; t-- > 0u;) { // chaine haute
			const uint32 i = ordre[t];
			while (k >= seuil && Croix(px, py, H[k - 2u], H[k - 1u], i) <= 1e-12f)
				--k;
			H[k++] = i;
		}
		const uint32 m = (k > 0u) ? k - 1u : 0u; // le dernier point reboucle sur le premier
		for (uint32 i = 0; i < m; ++i)
			enveloppe[i] = H[i];
		return m;
	}

	bool DansPolygone(const float32 *px, const float32 *py, const uint32 *idx, uint32 m, float32 x,
					  float32 y) {
		if (m < 3u)
			return false;
		bool dedans = false;
		for (uint32 i = 0, j = m - 1u; i < m; j = i++) {
			const float32 xi = px[idx[i]], yi = py[idx[i]];
			const float32 xj = px[idx[j]], yj = py[idx[j]];
			if (((yi > y) != (yj > y)) && (x < (xj - xi) * (y - yi) / (yj - yi) + xi))
				dedans = !dedans;
		}
		return dedans;
	}

	// Declaration anticipee : `EmpreinteDeplacee` s'appuie sur elle, et elle est
	// definie plus bas avec les autres instruments d'etendue.
	bool SommetSurEtendue(const NkProjectedGrid &g, const NkProjectedGridParams &p, float32 x0,
						  float32 x1, float32 y0, float32 y1, uint32 i, uint32 j, NkVec3f &out);

	// Une houle a UN train, pour le banc de composition.
	NkWaterParams UneHouleTest(float32 lambda, float32 amplitude, float32 raideur, float32 phase) {
		NkWaterParams h;
		h.waveCount = 1u;
		h.waves[0].amplitude = amplitude;
		h.waves[0].wavelength = lambda;
		h.waves[0].direction = NkVec2f{1.f, 0.f};
		h.waves[0].steepness = raideur;
		h.waves[0].phase = phase;
		return h;
	}

	// L'EMPREINTE DEPLACEE : la frontiere de l'etendue, poussee par la houle.
	//
	// ⚠️ SON DOMAINE DE VALIDITE, DIT AVANT L'USAGE. Prendre l'IMAGE DE LA FRONTIERE
	// pour la FRONTIERE DE L'IMAGE n'est licite que si l'application est INJECTIVE,
	// c'est-a-dire tant que le jacobien horizontal reste positif. Des que J <= 0 la
	// surface se replie, le polygone s'auto-intersecte, et un test pair-impair y
	// rendrait n'importe quoi -- un nombre, mais pas une mesure. On REND donc `jMin`
	// pour que l'appelant sache dans quel regime il mesure au lieu de le supposer.
	uint32 EmpreinteDeplacee(const NkProjectedGrid &g, const NkProjectedGridParams &p,
							 const NkWaterParams &houle, float32 t, float32 x0, float32 x1,
							 float32 y0, float32 y1, float32 *px, float32 *pz, uint32 *idx,
							 uint32 capacite, float32 &jMin) {
		jMin = 1e30f;
		uint32 m = 0;
		const uint32 pas = 2u;
		for (uint32 cote = 0; cote < 4u; ++cote) {
			const uint32 fin = (cote == 0u || cote == 2u) ? p.cols : p.rows;
			for (uint32 s = 0; s < fin && m < capacite; s += pas) {
				uint32 i = 0u, j = 0u;
				if (cote == 0u) {
					i = s;
					j = 0u;
				} else if (cote == 1u) {
					i = p.cols;
					j = s;
				} else if (cote == 2u) {
					i = p.cols - s;
					j = p.rows;
				} else {
					i = 0u;
					j = p.rows - s;
				}
				NkVec3f w;
				if (!SommetSurEtendue(g, p, x0, x1, y0, y1, i, j, w))
					continue;
				const NkWaterPoint q = NkWaterEval(houle, w.x, w.z, t);
				if (q.jacobianXZ < jMin)
					jMin = q.jacobianXZ;
				px[m] = q.position.x;
				pz[m] = q.position.z;
				idx[m] = m;
				++m;
			}
		}
		return m;
	}

	// LA COUVERTURE APRES COMPOSITION.
	//
	// On part toujours d'un pixel de RENDU -- c'est lui qui decide de ce qu'on voit
	// -- on trouve le point d'eau qu'il voit, et on demande s'il tombe dans
	// l'empreinte DEPLACEE, et non plus dans l'etendue au repos. C'est toute la
	// difference : la couverture 1,00 acquise est une garantie sur le plan de REPOS,
	// alors que la camera regarde la surface DEPLACEE.
	//
	// `fenetre` restreint l'echantillonnage au centre de l'ecran (1,0 = plein
	// ecran). Comparer plein ecran et centre dit si les trous sont AU BORD ou
	// REPARTIS -- et les deux ne s'expliquent pas pareil. C'est un PROXY, dit comme
	// tel : le bord de l'ecran n'est pas exactement le bord de l'etendue, mais
	// l'etendue est construite pour couvrir l'ecran, donc les deux se correspondent.
	float32 CouvertureComposee(const NkMat4f &renderInvVP, float32 hauteur, uint32 N,
							   const float32 *px, const float32 *pz, const uint32 *idx, uint32 m,
							   float32 fenetre, uint32 &vus) {
		uint32 dedans = 0;
		vus = 0;
		for (uint32 i = 0; i <= N; ++i) {
			for (uint32 j = 0; j <= N; ++j) {
				const float32 nx = fenetre * (-1.f + 2.f * (float32)i / (float32)N);
				const float32 ny = fenetre * (-1.f + 2.f * (float32)j / (float32)N);
				NkVec3f w;
				if (!PixelVoitLePlan(renderInvVP, nx, ny, hauteur, w))
					continue;
				++vus;
				if (DansPolygone(px, pz, idx, m, w.x, w.z))
					++dedans;
			}
		}
		return (vus > 0u) ? (float32)dedans / (float32)vus : 0.f;
	}

	// L'EMPREINTE RE-FORMEE PAR LA PREIMAGE.
	//
	// Au lieu de REMBOURRER l'etendue -- ce qui paie sur toute la surface une
	// correction due sur une bande -- on RE-FORME ses sommets : chaque point de la
	// frontiere W est remplace par D^-1(W), de sorte qu'apres deplacement il
	// retombe exactement sur W. Aucun sommet ajoute, aucune etendue elargie.
	//
	// ⚠️ CE QUE CETTE MESURE PROUVE, ET CE QU'ELLE NE PROUVE PAS. Si Newton
	// converge, alors D(D^-1(W)) = W et la couverture revient a sa valeur de repos
	// MECANIQUEMENT. Le 1,000 qui en sort mesure donc la CONVERGENCE, pas la
	// couverture. Les nombres porteurs sont ceux qu'on rend a cote : le RESIDU, le
	// nombre d'ITERATIONS, et les REFUS.
	uint32 EmpreintePreimage(const NkProjectedGrid &g, const NkProjectedGridParams &p,
							 const NkWaterParams &houle, float32 t, float32 x0, float32 x1,
							 float32 y0, float32 y1, float32 *px, float32 *pz, uint32 *idx,
							 uint32 capacite, float32 &residuMax, uint32 &iterMax, uint32 &refus) {
		residuMax = 0.f;
		iterMax = 0u;
		refus = 0u;
		uint32 m = 0;
		const uint32 pas = 2u;
		for (uint32 cote = 0; cote < 4u; ++cote) {
			const uint32 fin = (cote == 0u || cote == 2u) ? p.cols : p.rows;
			for (uint32 s = 0; s < fin && m < capacite; s += pas) {
				uint32 i = 0u, j = 0u;
				if (cote == 0u) {
					i = s;
					j = 0u;
				} else if (cote == 1u) {
					i = p.cols;
					j = s;
				} else if (cote == 2u) {
					i = p.cols - s;
					j = p.rows;
				} else {
					i = 0u;
					j = p.rows - s;
				}
				NkVec3f w;
				if (!SommetSurEtendue(g, p, x0, x1, y0, y1, i, j, w))
					continue;
				float32 wx = w.x, wz = w.z;
				uint32 it = 0u;
				const bool ok = NkWaterInverseXZ(houle, w.x, w.z, t, wx, wz, it);
				if (it > iterMax)
					iterMax = it;
				if (!ok)
					++refus;
				// On DEPLACE la preimage : elle doit retomber sur la cible.
				const NkWaterPoint q = NkWaterEval(houle, wx, wz, t);
				const float32 ex = q.position.x - w.x, ez = q.position.z - w.z;
				const float32 r = NkSqrt(ex * ex + ez * ez);
				if (r > residuMax)
					residuMax = r;
				px[m] = q.position.x;
				pz[m] = q.position.z;
				idx[m] = m;
				++m;
			}
		}
		return m;
	}

	// LE GACHIS QUE LAISSERAIT UN PAVAGE PARFAIT DE LA FORME.
	//
	// C'est LE nombre qui decide s'il faut ecrire ce pavage. On ne l'extrapole pas
	// a partir du remplissage -- ce serait supposer que tout sommet hors de
	// l'enveloppe est perdu, ce qui est plausible et non mesure. On compte donc
	// seulement les sommets qui tombent DEJA dans l'enveloppe, et on mesure LEUR
	// taux de perte : c'est exactement ce que vaudrait le gachis si l'etendue
	// epousait la forme au lieu de sa boite.
	float32 GachisDansForme(const NkProjectedGrid &g, const NkProjectedGridParams &p,
							const NkMat4f &renderVP, const uint32 *idx, uint32 m, uint32 pas,
							uint32 &dedans) {
		uint32 perdus = 0;
		dedans = 0;
		const float32 hauteurs[3] = {p.baseY, p.baseY + p.displacementMax,
									 p.baseY - p.displacementMax};
		for (uint32 j = 0; j <= p.rows; j += pas) {
			for (uint32 i = 0; i <= p.cols; i += pas) {
				const float32 u = (float32)i / (float32)p.cols;
				const float32 v = (float32)j / (float32)p.rows;
				const float32 nx = g.ndcMinX + (g.ndcMaxX - g.ndcMinX) * u;
				const float32 ny = g.ndcMinY + (g.ndcMaxY - g.ndcMinY) * v;
				if (!DansPolygone(g.ndcX, g.ndcY, idx, m, nx, ny))
					continue;
				NkVec3f w;
				if (!NkProjectedGridVertex(g, p, i, j, w))
					continue;
				++dedans;
				bool vu = false;
				for (uint32 h = 0; h < 3u && !vu; ++h) {
					const NkVec4f q = renderVP * NkVec4f(w.x, hauteurs[h], w.z, 1.f);
					if (q.w <= 1e-6f)
						continue;
					const float32 iw = 1.f / q.w;
					const float32 sx = q.x * iw, sy = q.y * iw;
					if (sx >= -1.f && sx <= 1.f && sy >= -1.f && sy <= 1.f)
						vu = true;
				}
				if (!vu)
					++perdus;
			}
		}
		return (dedans > 0u) ? (float32)perdus / (float32)dedans : 0.f;
	}

	// La position AU SOL que designe un point de l'ecran de portee.
	bool SolDepuisNDC(const NkProjectedGrid &g, const NkProjectedGridParams &p, float32 nx,
					  float32 ny, NkVec3f &out) {
		NkVec3f a, b;
		if (!NkUnprojectNDC(g.invViewProj, nx, ny, -1.f, a))
			return false;
		if (!NkUnprojectNDC(g.invViewProj, nx, ny, 1.f, b))
			return false;
		const NkVec3f d = b - a;
		if (NkFabs(d.y) < 1e-9f)
			return false;
		const float32 t = (p.baseY - a.y) / d.y;
		if (t < 0.f)
			return false;
		out = a + d * t;
		out.y = p.baseY;
		return true;
	}

	// La camera de RENDU voit-elle de l'eau a cette position au sol ? MEME critere
	// que `Gachis`, mot pour mot : trois hauteurs, dans l'ecran, devant. Il DOIT
	// etre le meme, sinon comparer les deux instruments ne voudrait rien dire.
	bool EauVisibleAuSol(const NkMat4f &renderVP, const NkProjectedGridParams &p, float32 x,
						 float32 z) {
		const float32 hauteurs[3] = {p.baseY, p.baseY + p.displacementMax,
									 p.baseY - p.displacementMax};
		for (uint32 h = 0; h < 3u; ++h) {
			const NkVec4f q = renderVP * NkVec4f(x, hauteurs[h], z, 1.f);
			if (q.w <= 1e-6f)
				continue;
			const float32 iw = 1.f / q.w;
			const float32 sx = q.x * iw, sy = q.y * iw;
			if (sx >= -1.f && sx <= 1.f && sy >= -1.f && sy <= 1.f)
				return true;
		}
		return false;
	}

	bool SommetSurEtendue(const NkProjectedGrid &g, const NkProjectedGridParams &p, float32 x0,
						  float32 x1, float32 y0, float32 y1, uint32 i, uint32 j, NkVec3f &out) {
		const float32 u = (float32)i / (float32)p.cols;
		const float32 v = (float32)j / (float32)p.rows;
		return SolDepuisNDC(g, p, x0 + (x1 - x0) * u, y0 + (y1 - y0) * v, out);
	}

	// Le gachis d'une grille etalee sur une ETENDUE DONNEE, et non sur celle que la
	// grille a retenue. Sert a mesurer ce que vaudrait l'etendue si tel point n'y
	// avait pas contribue -- SANS rien changer a l'algorithme.
	float32 GachisSurEtendue(const NkProjectedGrid &g, const NkProjectedGridParams &p,
							 const NkMat4f &renderVP, float32 x0, float32 x1, float32 y0,
							 float32 y1, uint32 pas, uint32 &total) {
		uint32 perdus = 0;
		total = 0;
		for (uint32 j = 0; j <= p.rows; j += pas) {
			for (uint32 i = 0; i <= p.cols; i += pas) {
				NkVec3f m;
				if (!SommetSurEtendue(g, p, x0, x1, y0, y1, i, j, m))
					continue;
				++total;
				if (!EauVisibleAuSol(renderVP, p, m.x, m.z))
					++perdus;
			}
		}
		return (total > 0u) ? (float32)perdus / (float32)total : 0.f;
	}

	float32 RapportPasEcranSurEtendue(const NkProjectedGrid &g, const NkProjectedGridParams &p,
									  const NkMat4f &vpRendu, float32 x0, float32 x1, float32 y0,
									  float32 y1) {
		float32 mn = 1e30f, mx = 0.f;
		for (uint32 j = 0; j <= p.rows; j += 8u) {
			for (uint32 i = 0; i + 8u <= p.cols; i += 8u) {
				NkVec3f a, b;
				if (!SommetSurEtendue(g, p, x0, x1, y0, y1, i, j, a) ||
					!SommetSurEtendue(g, p, x0, x1, y0, y1, i + 8u, j, b))
					continue;
				const NkVec4f qa = vpRendu * NkVec4f(a.x, a.y, a.z, 1.f);
				const NkVec4f qb = vpRendu * NkVec4f(b.x, b.y, b.z, 1.f);
				if (qa.w <= 1e-6f || qb.w <= 1e-6f)
					continue;
				const float32 dx = qb.x / qb.w - qa.x / qa.w;
				const float32 dy = qb.y / qb.w - qa.y / qa.w;
				const float32 d = NkSqrt(dx * dx + dy * dy);
				if (d < mn)
					mn = d;
				if (d > mx)
					mx = d;
			}
		}
		return (mn > 1e-9f) ? mx / mn : 0.f;
	}

	float32 AirePolygone(const float32 *px, const float32 *py, const uint32 *idx, uint32 m) {
		if (m < 3u)
			return 0.f;
		float64 a = 0.0;
		for (uint32 i = 0; i < m; ++i) {
			const uint32 u = idx[i], v = idx[(i + 1u) % m];
			a += (float64)px[u] * (float64)py[v] - (float64)px[v] * (float64)py[u];
		}
		a *= 0.5;
		return (float32)(a < 0.0 ? -a : a);
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

	// La couverture la PIRE parmi les hauteurs reellement visibles depuis la pose.
	// Une hauteur que personne ne voit ne compte pas -- lecon du 07/09, payee deux
	// fois -- et `hauteursVues` dit combien ont pu etre mesurees, pour que
	// l'appelant distingue « 1,00 partout » de « rien n'a pu etre mesure ».
	float32 CouvertureMin(const NkProjectedGrid &g, const NkProjectedGridParams &p,
						  const NkMat4f &invRendu, uint32 &hauteursVues) {
		const float32 hauteurs[3] = {p.baseY, p.baseY + p.displacementMax,
									 p.baseY - p.displacementMax};
		float32 pire = 1.f;
		hauteursVues = 0;
		for (uint32 h = 0; h < 3u; ++h) {
			uint32 vus = 0;
			const uint32 trous = PixelsDecouvertsPortee(g, p, invRendu, hauteurs[h], 48u, vus);
			if (vus == 0u)
				continue;
			++hauteursVues;
			const float32 c = Couverture(vus, trous);
			if (c < pire)
				pire = c;
		}
		return (hauteursVues > 0u) ? pire : 0.f;
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
		uint32 lisses = 0, indetermines = 0;
		for (uint32 k = 0; k < 2u; ++k) {
			const NkProjectedGrid gRepos = BUILD(bancs[k], p);
			const NkProjectedGrid gUn = BUILD(TournerYaw(bancs[k], 1.f), p);
			const NkProjectedGrid gQuart = BUILD(TournerYaw(bancs[k], 0.25f), p);
			const float32 dUn = GlissementMoyen(gRepos, gUn, p, 4u);
			const float32 dQuart = GlissementMoyen(gRepos, gQuart, p, 4u);
			const float32 rapport = (dQuart > 1e-9f) ? dUn / dQuart : 0.f;
			// ⚠️ LE COMPTE DE COMPARAISONS S'IMPRIME TOUJOURS, pas seulement quand le
			// glissement vaut zero. Un compteur qui n'apparait qu'au cas suspect ne
			// protege que le cas qu'on avait deja soupconne.
			const uint32 comparables = SommetsComparables(gRepos, gUn, p, 4u);
			const bool mesurable = comparables >= kPlancherComparables;
			std::fprintf(stderr,
						 "     (x10) %-28s : glissement 1 deg %.4f maille | 0,25 deg %.4f"
						 " -> rapport %.2f (4,00 = continu) | %u sommets compares%s\n",
						 bancs[k].nom, (double)dUn, (double)dQuart, (double)rapport, comparables,
						 mesurable ? "" : "  -> INDETERMINE");
			if (!mesurable)
				++indetermines;
			else if (rapport > 3.f && rapport < 5.f)
				++lisses;
		}
		// LE COMPTE PORTE L'ASSERTION : sous le plancher, le cas n'est ni vert ni
		// rouge, il est INDETERMINE. Un vert obtenu sur zero comparaison est un faux
		// vert -- c'est le piege du 07/09, trois sondes vertes pendant que le defaut
		// etait a l'ecran. Et un temoin dont TOUS les cas seraient indetermines ne
		// prouverait rien : on exige donc au moins un cas mesurable.
		std::fprintf(stderr, "     (x10) %u lisses, %u indetermines sur 2\n", lisses, indetermines);
		XCHECK(indetermines < 2u && lisses == (2u - indetermines),
			   "(x10) le glissement est PROPORTIONNEL a l'angle la ou il est MESURABLE");

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

	// (x12) LA COURBE, AVANT LE CHOIX.
	//
	// On ne cherche pas « la bonne valeur » par tatonnement : on MESURE couverture
	// et gachis sur les DEUX degres de liberte de la camera de portee, sur la pose
	// la plus rasante -- celle qui vaut 0,90. Si les deux mesures ont un domaine
	// commun, la valeur ET sa raison en sortent ensemble. Si elles n'en ont pas,
	// c'est un RESULTAT : la technique a une limite structurelle sur cette pose, et
	// ca se dit au lieu de se noyer dans un compromis.
	{
		const Pose rasante = {{0.f, 1.f, 0.f}, {0.f, 1.2f, -60.f}, "au ras de l'eau"};
		const NkMat4f vpR = VP(rasante);
		const NkMat4f invR = vpR.Inverse();
		const float32 elevations[7] = {0.1f, 0.25f, 0.5f, 1.f, 2.f, 8.f, 32.f};
		const float32 pitches[6] = {0.02f, 0.05f, 0.15f, 0.35f, 0.7f, 1.0f};
		std::fprintf(stderr, "     (x12) COURBE sur la pose rasante (couverture doit rester 1,00)\n");
		uint32 cellules = 0, couvertes = 0;
		float32 planche = 1e30f, pitchPlancher = 0.f, elevPlancher = 0.f;
		for (uint32 a = 0; a < 6u; ++a) {
			for (uint32 e = 0; e < 7u; ++e) {
				NkProjectedGridParams pk = p;
				pk.rangeElevation = elevations[e];
				pk.rangePitchMin = pitches[a];
				const NkProjectedGrid gk = BUILD(rasante, pk);
				uint32 vus = 0;
				const uint32 trous =
					PixelsDecouvertsPortee(gk, pk, invR, pk.baseY + pk.displacementMax, 48u, vus);
				uint32 tot = 0;
				const float32 gach = Gachis(gk, pk, vpR, 2u, tot);
				++cellules;
				if (vus > 0u && Couverture(vus, trous) > 0.999f)
					++couvertes;
				if (gach < planche) {
					planche = gach;
					pitchPlancher = pitches[a];
					elevPlancher = elevations[e];
				}
				std::fprintf(stderr,
							 "     (x12) pitchMin %.2f | elev %5.2f m -> couverture %.2f | gachis %.2f\n",
							 (double)pitches[a], (double)elevations[e],
							 (double)Couverture(vus, trous), (double)gach);
			}
		}

		// 🔴 CE QUE LA COURBE DIT, ET CE N'EST PAS CE QU'ON ATTENDAIT.
		//
		// 1. LA COUVERTURE NE SE DEGRADE JAMAIS -- 1,00 dans les 42 cellules, sur les
		//    DEUX axes. Le compromis de Johanson (« trop bas, on perd la couverture »)
		//    N'EXISTE PAS dans cette version, et la raison est structurelle : l'etendue
		//    n'est plus rognee sur l'ecran de la camera de portee, donc tout point
		//    rabattu est deja sous son horizon des qu'elle est au-dessus de la tranche.
		//    Il n'y a donc AUCUN arbitrage a faire entre ces deux nombres-la.
		// 2. ET LE GACHIS A UN PLANCHER qu'aucun reglage ne franchit. Il decroit avec
		//    les deux axes mais SATURE : a plein aplomb (le maximum possible), 8 m et
		//    32 m d'elevation donnent le meme nombre.
		//
		// CONSEQUENCE : l'objectif de 0,50 n'est PAS atteignable par ces deux
		// parametres. Ce n'est pas un echec de reglage, c'est la forme de l'etendue --
		// un RECTANGLE aligne sur les axes en NDC de portee, alors que l'eau vue par le
		// rendu est un TRAPEZE. Un rectangle circonscrit a un trapeze gaspille la
		// moitie par construction, et c'est exactement la ou le plancher se pose.
		std::fprintf(stderr,
					 "     (x12) PLANCHER du gachis : %.2f (pitchMin %.2f, elev %.2f m) --"
					 " objectif 0,50 NON atteint par ces deux parametres\n",
					 (double)planche, (double)pitchPlancher, (double)elevPlancher);
		XCHECK(cellules == 42u && couvertes == cellules,
			   "(x12) la couverture vaut 1,00 dans TOUTE la nappe : les deux axes ne l'entament jamais");
	}

	// (x13) CE QU'UN REGLAGE DONNE AILLEURS -- le compromis entre les POSES.
	//
	// La courbe (x12) ne parle que de la pose rasante. Prendre son minimum comme
	// defaut serait choisir en silence : le meme reglage agit sur toutes les vues.
	// On mesure donc les trois poses ET l'uniformite du pas ecran pour chaque
	// candidat, et on EXIGE que le compromis soit reel. S'il ne l'etait pas, il y
	// aurait un repas gratuit -- et il faudrait le prendre au lieu de le decrire.
	{
		struct Cand {
				float32 pitch;
				float32 elev;
				const char *nom;
		};
		const Cand cands[4] = {
			{0.05f, 0.5f, "defaut actuel"},
			{0.15f, 2.0f, "intermediaire"},
			{0.35f, 8.0f, "plongeant"},
			{1.00f, 32.0f, "plein aplomb"},
		};
		const Pose troisPoses[3] = {
			{{0.f, 8.f, 0.f}, {0.f, 2.f, -60.f}, "pont 8 m"},
			{{0.f, 1.f, 0.f}, {0.f, 1.2f, -60.f}, "rasante 1 m"},
			{{0.f, -3.f, 0.f}, {0.f, 1.f, -20.f}, "sous l'eau"},
		};
		float32 gachPontDefaut = 0.f, gachPontAplomb = 0.f;
		float32 rapportDefaut = 0.f, rapportAplomb = 0.f;
		for (uint32 c = 0; c < 4u; ++c) {
			NkProjectedGridParams pk = p;
			pk.rangePitchMin = cands[c].pitch;
			pk.rangeElevation = cands[c].elev;
			float32 g3[3] = {0.f, 0.f, 0.f};
			for (uint32 k = 0; k < 3u; ++k) {
				const NkProjectedGrid gk = BUILD(troisPoses[k], pk);
				uint32 tot = 0;
				g3[k] = Gachis(gk, pk, VP(troisPoses[k]), 2u, tot);
			}
			const NkProjectedGrid gPont = BUILD(troisPoses[0], pk);
			const float32 rap = RapportPasEcran(gPont, pk, VP(troisPoses[0]));
			std::fprintf(stderr,
						 "     (x13) %-14s (pitch %.2f, elev %5.2f) : gachis pont %.2f | rasante %.2f"
						 " | sous l'eau %.2f | pas ecran x%.2f\n",
						 cands[c].nom, (double)cands[c].pitch, (double)cands[c].elev, (double)g3[0],
						 (double)g3[1], (double)g3[2], (double)rap);
			if (c == 0u) {
				gachPontDefaut = g3[0];
				rapportDefaut = rap;
			}
			if (c == 3u) {
				gachPontAplomb = g3[0];
				rapportAplomb = rap;
			}
		}
		std::fprintf(stderr,
					 "     (x13) ce que coute le plein aplomb sur la vue de pont : gachis %.2f -> %.2f,"
					 " pas ecran x%.2f -> x%.2f\n",
					 (double)gachPontDefaut, (double)gachPontAplomb, (double)rapportDefaut,
					 (double)rapportAplomb);
		XCHECK(gachPontAplomb > gachPontDefaut || rapportAplomb > rapportDefaut * 1.5f,
			   "(x13) le compromis est REEL : ce qui soulage la vue rasante abime la vue de pont");
	}

	// (x14) LA FORME DE L'EAU VUE, MESUREE AVANT D'ETRE PAVEE.
	//
	// L'etendue est une BOITE en NDC de portee ; la forme reelle est l'enveloppe
	// convexe du nuage dont cette boite est tiree. Le REMPLISSAGE (aire enveloppe /
	// aire boite) dit ce que la boite enferme de vide.
	//
	// ⚠️ ET IL SE VERIFIE TOUT SEUL. Les sommets etant uniformes dans la boite, la
	// fraction qui tombe hors de la forme vaut a peu pres 1 - remplissage. Si ce
	// nombre reproduit le GACHIS mesure par un instrument tout a fait different
	// (x9), alors la forme est bien la cause. S'il ne le reproduit pas, mon
	// explication est fausse et c'est ce chiffre-la qui le dira.
	{
		const Pose troisPoses[3] = {
			{{0.f, 8.f, 0.f}, {0.f, 2.f, -60.f}, "pont 8 m"},
			{{0.f, 1.f, 0.f}, {0.f, 1.2f, -60.f}, "rasante 1 m"},
			{{0.f, -3.f, 0.f}, {0.f, 1.f, -20.f}, "sous l'eau"},
		};
		float32 ecarts[3] = {0.f, 0.f, 0.f};
		float32 gachisForme[3] = {0.f, 0.f, 0.f};
		// ⚠️ EPINGLE SUR L'ETAT D'AVANT LE RETRAIT. Ce temoin a ete ecrit pour
		// mesurer la pathologie du rabattement ; laisser le nouveau defaut y entrer
		// lui ferait mesurer autre chose SANS RIEN CASSER DE VISIBLE -- exactement
		// l'instrument qui se retourne en restant vert, paye une fois cette nuit.
		NkProjectedGridParams pAvant = p;
		pAvant.retraitEgares = false;
		for (uint32 k = 0; k < 3u; ++k) {
			const NkProjectedGrid gk = BUILD(troisPoses[k], pAvant);
			uint32 env[40];
			const uint32 m = EnveloppeConvexe(gk.ndcX, gk.ndcY, gk.ndcCount, env);
			const float32 aireForme = AirePolygone(gk.ndcX, gk.ndcY, env, m);
			const float32 aireBoite = (gk.ndcMaxX - gk.ndcMinX) * (gk.ndcMaxY - gk.ndcMinY);
			const float32 remplissage = (aireBoite > 1e-9f) ? aireForme / aireBoite : 0.f;
			uint32 tot = 0;
			const float32 gach = Gachis(gk, p, VP(troisPoses[k]), 2u, tot);
			const float32 predit = 1.f - remplissage;
			ecarts[k] = NkFabs(predit - gach);
			// LE NOMBRE QUI DECIDE : ce que vaudrait le gachis si l'etendue epousait
			// la forme. Mesure sur les sommets qui y tombent deja, pas extrapole.
			uint32 dedans = 0;
			gachisForme[k] = GachisDansForme(gk, p, VP(troisPoses[k]), env, m, 2u, dedans);
			std::fprintf(stderr,
						 "     (x14) %-12s : enveloppe %u sommets | remplissage %.2f |"
						 " 1-remplissage %.2f contre gachis %.2f (ecart %.2f) | PAVER LA FORME"
						 " laisserait %.2f sur %u sommets\n",
						 troisPoses[k].nom, m, (double)remplissage, (double)predit, (double)gach,
						 (double)ecarts[k], (double)gachisForme[k], dedans);
		}

		// 🔴 CE QUE LE MODELE EXPLIQUE, ET OU IL CASSE.
		//
		// « 1 - remplissage » predit le gachis a 0,03 et 0,06 pres quand l'oeil est
		// HORS de la tranche (pont a 8 m, sous l'eau a -3 m) : l'instrument est bon
		// et le raisonnement tient. Il rate de 0,52 sur la vue rasante, ou l'oeil est
		// DANS la tranche. La boite n'y enferme que 37 % de vide, pas 90 % : LA FORME
		// DE L'ETENDUE N'EST DONC PAS LA CAUSE du 0,90, et mon explication du commit
		// precedent tombe a son tour.
		//
		// On n'assert ici que ce qui est prouve : le modele vaut la ou il vaut. Le
		// gachis non explique reste porte par (x9), qui reste rouge.
		XCHECK(ecarts[0] < 0.10f && ecarts[2] < 0.10f,
			   "(x14) hors de la tranche, le vide de la boite PREDIT le gachis (ecart < 0,10)");
		XCHECK(ecarts[1] > 0.30f,
			   "(x14b) DANS la tranche il ne le predit plus : une TROISIEME cause existe");
	}

	// (x15) LE CAS DEGENERE AVANT LE CAS GENERAL.
	//
	// Un pavage non rectangulaire suppose un POLYGONE : au moins trois sommets et
	// une aire non nulle. Avant d'ecrire ce pavage, on mesure si cette condition
	// tient sur les poses ou la version actuelle refusait proprement -- l'horizon
	// qui coupe le cadre, la plongee verticale, l'oeil dans le plan.
	//
	// Ce que ce temoin exige : TOUTE grille qui se declare visible offre un
	// polygone pavable. S'il rougit, c'est que le pavage devra garder le rectangle
	// dans ces cas-la -- et un refus honnete vaut mieux qu'un pavage faux.
	{
		const Pose degen[6] = {
			{{0.f, 8.f, 0.f}, {0.f, 8.f, -60.f}, "pile a l'horizon"},
			{{0.f, 30.f, 0.f}, {0.f, 0.f, -0.001f}, "plongee verticale"},
			{{0.f, -3.f, 0.f}, {0.f, 1.f, -20.f}, "sous l'eau"},
			{{0.f, 8.f, 0.f}, {0.f, 60.f, -10.f}, "vers le ciel"},
			{{0.f, 0.f, 0.f}, {0.f, 0.f, -60.f}, "oeil DANS le plan"},
			{{0.f, 1.f, 0.f}, {0.f, 1.2f, -60.f}, "rasante 1 m"},
		};
		uint32 visibles = 0, pavables = 0;
		for (uint32 k = 0; k < 6u; ++k) {
			const NkProjectedGrid gk = BUILD(degen[k], p);
			if (!gk.visible) {
				std::fprintf(stderr, "     (x15) %-20s : invisible (dit)\n", degen[k].nom);
				continue;
			}
			++visibles;
			uint32 env[40];
			const uint32 m = EnveloppeConvexe(gk.ndcX, gk.ndcY, gk.ndcCount, env);
			const float32 aire = AirePolygone(gk.ndcX, gk.ndcY, env, m);
			const bool pavable = (m >= 3u) && (aire > 1e-9f);
			if (pavable)
				++pavables;
			std::fprintf(stderr,
						 "     (x15) %-20s : %u points, enveloppe %u sommets, aire %.4f%s%s -> %s\n",
						 degen[k].nom, gk.ndcCount, m, (double)aire,
						 gk.repliPleinEcran ? " [repli plein ecran]" : "",
						 gk.rogneHorizon ? " [rogne horizon]" : "",
						 pavable ? "PAVABLE" : "PAS DE POLYGONE");
		}
		std::fprintf(stderr, "     (x15) %u poses visibles, %u pavables\n", visibles, pavables);
		XCHECK(visibles > 0u && pavables == visibles,
			   "(x15) toute grille qui se declare visible offre un polygone pavable");
	}

	// (x16) LA TROISIEME CAUSE, ISOLEE PAR UNE SEULE VARIABLE.
	//
	// (x14) montre que le modele tient hors de la tranche et casse dedans. Trois
	// poses, c'est un motif, pas une preuve : elles different aussi par l'altitude,
	// la visee et la distance. On change donc UNE chose et rien d'autre -- l'oeil
	// passe de 2,10 m a 1,90 m, de part et d'autre du sommet de la tranche (2,00 m),
	// meme direction de visee, meme tout le reste.
	//
	// Si l'ecart saute en franchissant ce seuil, la cause est le RABATTEMENT : les
	// points d'intersection du tronc de vue et de la tranche sont ecrases sur le
	// plan de repos, et quand l'oeil est DANS la tranche, leur empreinte au sol
	// couvre bien plus large que l'eau reellement visible. Ce n'est alors ni le bord
	// lointain, ni la forme de l'etendue : c'est l'etape 4 elle-meme.
	//
	// 🔴 PREMIERE VERSION DE CE TEMOIN : REFUTEE PAR LUI-MEME, ET JE LA LAISSE DITE.
	// Il opposait 2,10 m et 1,90 m -- 20 cm de part et d'autre du sommet de tranche
	// -- en exigeant que l'ecart saute de 0,20. Mesure : 0,03 puis 0,12. Un facteur
	// quatre, mais pas un saut, et rien qui approche les 0,52 de l'oeil a 1,00 m.
	// FRANCHIR LE SEUIL NE SUFFIT DONC PAS : la perte n'est pas un interrupteur,
	// elle croit a mesure que l'oeil descend vers le plan. Le temoin mesure
	// desormais CETTE forme-la, qui est celle que la mesure a montree.
	{
		const float32 hauteurs[7] = {3.0f, 2.5f, 2.1f, 1.9f, 1.5f, 1.0f, 0.5f};
		float32 gachis[7] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
		float32 pavage[7] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
		// EPINGLE SUR L'ETAT D'AVANT LE RETRAIT : cette nappe decrit la pathologie,
		// pas la grille corrigee.
		NkProjectedGridParams pAvant = p;
		pAvant.retraitEgares = false;
		for (uint32 k = 0; k < 7u; ++k) {
			const Pose po = {{0.f, hauteurs[k], 0.f},
							 {0.f, hauteurs[k] + 0.2f, -60.f},
							 "sonde hauteur"};
			const NkProjectedGrid gk = BUILD(po, pAvant);
			uint32 env[40];
			const uint32 m = EnveloppeConvexe(gk.ndcX, gk.ndcY, gk.ndcCount, env);
			const float32 aireForme = AirePolygone(gk.ndcX, gk.ndcY, env, m);
			const float32 aireBoite = (gk.ndcMaxX - gk.ndcMinX) * (gk.ndcMaxY - gk.ndcMinY);
			const float32 remplissage = (aireBoite > 1e-9f) ? aireForme / aireBoite : 0.f;
			uint32 tot = 0;
			gachis[k] = Gachis(gk, p, VP(po), 2u, tot);
			uint32 dedans = 0;
			pavage[k] = GachisDansForme(gk, p, VP(po), env, m, 2u, dedans);
			std::fprintf(stderr,
						 "     (x16) oeil a %4.2f m %-14s : remplissage %.2f | gachis %.2f |"
						 " ecart %.2f | en pavant la forme %.2f\n",
						 (double)hauteurs[k], (hauteurs[k] > 2.f) ? "(HORS tranche)" : "(DANS tranche)",
						 (double)remplissage, (double)gachis[k],
						 (double)NkFabs((1.f - remplissage) - gachis[k]), (double)pavage[k]);
		}

		// 🔴 ET MA DEUXIEME VERSION DE CE TEMOIN ETAIT FAUSSE AUSSI -- dite, pas
		// effacee. Elle comparait la variation « DANS la tranche » entre 1,50 m et
		// 0,50 m. Or le gachis PASSE PAR UN MAXIMUM a 1,00 m (0,90) et redescend :
		// 1,50 m et 0,50 m valent tous DEUX 0,82, et leur difference s'annule. Deux
		// points symetriques autour d'un sommet ne mesurent pas une variation -- ils
		// mesurent zero, quelle que soit la pente entre eux.
		//
		// CE QUE LA NAPPE MONTRE VRAIMENT :
		//  - le gachis monte de 0,20 a 0,90 puis REDESCEND a 0,82 : non monotone ;
		//  - la rampe COMMENCE AVANT la tranche (0,22 a 2,50 m, 0,35 a 2,10 m, tous
		//    deux dehors) : la frontiere de tranche ne declenche donc PAS le gachis ;
		//  - mais elle SEPARE NETTEMENT le gain du pavage : epouser la forme donne
		//    exactement 0,00 aux trois hauteurs AU-DESSUS de la tranche, et ne
		//    descend jamais sous 0,15 en dessous.
		// C'est ce seuil-la qui existe, et il porte sur une autre grandeur que celle
		// que j'avais testee.
		uint32 nulsDehors = 0, nonNulsDedans = 0;
		for (uint32 k = 0; k < 3u; ++k)
			if (pavage[k] < 0.01f)
				++nulsDehors;
		for (uint32 k = 3u; k < 7u; ++k)
			if (pavage[k] > 0.15f)
				++nonNulsDedans;
		std::fprintf(stderr,
					 "     (x16) pavage de la forme : %u/3 hauteurs HORS tranche a 0,00 |"
					 " %u/4 hauteurs DANS la tranche au-dessus de 0,15\n",
					 nulsDehors, nonNulsDedans);
		XCHECK(nulsDehors == 3u && nonNulsDedans == 4u,
			   "(x16) paver la forme ANNULE le gachis au-dessus de la tranche, et jamais dedans");
	}

	// (x17) LA CONTRIBUTION DU RABATTEMENT, MESUREE ET NON SUPPOSEE.
	//
	// Trois causes ont ete eliminees par la mesure : le bord lointain, le placement
	// de la portee, la forme de l'etendue. Reste l'ECRASEMENT de l'etape 4 : les
	// points d'intersection du tronc de vue et de la tranche sont projetes comme si
	// l'eau etait au sol sous eux -- (x, baseY, z) -- alors qu'ils etaient a
	// (x, y, z) avec y n'importe ou dans la tranche.
	//
	// LA QUESTION : combien de sommets, en vue rasante, sont perdus PARCE QUE leurs
	// points fondateurs ont ete rabattus ? On appelle EGARE un point retenu dont la
	// position AU SOL, apres rabattement, n'est pas de l'eau que la camera de rendu
	// voit. Un tel point etire l'etendue vers un endroit que personne ne regarde.
	//
	// ⚠️ AUCUN CORRECTIF N'EST PROPOSE ICI. On mesure une contribution : ce que
	// vaudrait le gachis si les egares n'avaient pas contribue a l'etendue. Le geste
	// -- retrait, recalibrage ou deplacement -- se decidera apres, en sachant.
	{
		const Pose bancs[2] = {
			{{0.f, 1.f, 0.f}, {0.f, 1.2f, -60.f}, "rasante 1 m"},
			{{0.f, 8.f, 0.f}, {0.f, 2.f, -60.f}, "pont 8 m (controle)"},
		};
		uint32 instrumentsAccordes = 0;
		float32 gachReduits[2] = {-1.f, -1.f};
		// EPINGLE SUR L'ETAT D'AVANT LE RETRAIT : c'est ce temoin qui DEMONTRE la
		// cause (0,90 -> 0,12). Mesure sur la grille deja retraitee, il comparerait
		// une etendue corrigee a elle-meme et ne demontrerait plus rien.
		NkProjectedGridParams pAvant = p;
		pAvant.retraitEgares = false;
		for (uint32 k = 0; k < 2u; ++k) {
			const NkProjectedGrid gk = BUILD(bancs[k], pAvant);
			const NkMat4f renderVP = VP(bancs[k]);

			// 1. VALIDATION DE L'INSTRUMENT AVANT DE LUI FAIRE CONFIANCE. Sur
			// l'etendue que la grille a REELLEMENT retenue, le nouvel instrument doit
			// retrouver le gachis de (x9). Sinon son chiffre sur une etendue reduite
			// ne voudrait rien dire.
			uint32 totRef = 0, totOwn = 0;
			const float32 gachRef = Gachis(gk, p, renderVP, 2u, totRef);
			const float32 gachOwn = GachisSurEtendue(gk, p, renderVP, gk.ndcMinX, gk.ndcMaxX,
													 gk.ndcMinY, gk.ndcMaxY, 2u, totOwn);
			if (NkFabs(gachOwn - gachRef) < 0.01f)
				++instrumentsAccordes;

			// 2. CLASSER LES POINTS RETENUS : egare ou non.
			uint32 egares = 0;
			float32 mnx = 1e30f, mxx = -1e30f, mny = 1e30f, mxy = -1e30f;
			float64 distEgares = 0.0, distGardes = 0.0;
			float64 ecrasEgares = 0.0, ecrasGardes = 0.0;
			for (uint32 i = 0; i < gk.ndcCount; ++i) {
				NkVec3f sol;
				if (!SolDepuisNDC(gk, p, gk.ndcX[i], gk.ndcY[i], sol))
					continue;
				const float32 dx = sol.x - bancs[k].oeil.x, dz = sol.z - bancs[k].oeil.z;
				const float32 dist = NkSqrt(dx * dx + dz * dz);
				const float32 ecras = NkFabs(gk.ptsY[i] - p.baseY);
				const bool vu = EauVisibleAuSol(renderVP, p, sol.x, sol.z);
				if (k == 0u)
					std::fprintf(stderr,
								 "     (x17) rasante | point %2u : hauteur avant rabattement"
								 " %+6.2f m, au sol a %9.1f m -> %s\n",
								 i, (double)gk.ptsY[i], (double)dist, vu ? "vu" : "EGARE");
				if (vu) {
					distGardes += (float64)dist;
					ecrasGardes += (float64)ecras;
					if (gk.ndcX[i] < mnx)
						mnx = gk.ndcX[i];
					if (gk.ndcX[i] > mxx)
						mxx = gk.ndcX[i];
					if (gk.ndcY[i] < mny)
						mny = gk.ndcY[i];
					if (gk.ndcY[i] > mxy)
						mxy = gk.ndcY[i];
				} else {
					++egares;
					distEgares += (float64)dist;
					ecrasEgares += (float64)ecras;
				}
			}
			const uint32 gardes = gk.ndcCount - egares;

			// 3. CE QUE VAUDRAIT L'ETENDUE SANS EUX -- avec la MEME marge relative.
			// ⚠️ Le pas ecran de l'etendue REDUITE ne veut rien dire seul : on mesure
			// aussi celui de l'etendue ACTUELLE, sur la MEME pose. Comparer une
			// etendue hypothetique au x1,00 de la pose nominale serait comparer deux
			// choses differentes.
			const float32 pasActuel = RapportPasEcranSurEtendue(
				gk, p, renderVP, gk.ndcMinX, gk.ndcMaxX, gk.ndcMinY, gk.ndcMaxY);
			float32 gachReduit = -1.f, pasReduit = -1.f;
			if (gardes >= 2u && mxx > mnx && mxy > mny) {
				const float32 ex = (mxx - mnx) * p.edgeBias, ey = (mxy - mny) * p.edgeBias;
				uint32 totR = 0;
				gachReduit = GachisSurEtendue(gk, p, renderVP, mnx - ex, mxx + ex, mny - ey,
											  mxy + ey, 2u, totR);
				pasReduit = RapportPasEcranSurEtendue(gk, p, renderVP, mnx - ex, mxx + ex,
													  mny - ey, mxy + ey);
			}
			gachReduits[k] = gachReduit;
			std::fprintf(stderr,
						 "     (x17) %-20s : %u points dont %u EGARES | distance moyenne egares"
						 " %.0f m contre gardes %.0f m | ecrasement moyen %.2f contre %.2f m\n",
						 bancs[k].nom, gk.ndcCount, egares,
						 (egares > 0u) ? distEgares / (float64)egares : 0.0,
						 (gardes > 0u) ? distGardes / (float64)gardes : 0.0,
						 (egares > 0u) ? ecrasEgares / (float64)egares : 0.0,
						 (gardes > 0u) ? ecrasGardes / (float64)gardes : 0.0);
			std::fprintf(stderr,
						 "     (x17) %-20s : gachis %.2f (controle %.2f) -> SANS les egares"
						 " %.2f | pas ecran actuel x%.2f -> reduit x%.2f\n",
						 bancs[k].nom, (double)gachRef, (double)gachOwn, (double)gachReduit,
						 (double)pasActuel, (double)pasReduit);
		}

		// 🔴 LA CAUSE EST TROUVEE, ET LE MECANISME EST L'INVERSE DE CE QUE J'AVAIS
		// PREDIT.
		//
		// J'attendais que les egares soient les points LOINTAINS -- les intersections
		// de la tranche avec le plan lointain, a ~2 870 m, dont l'empreinte au sol
		// part vers l'horizon. Mesure : distance moyenne des EGARES 1 m, contre
		// 1 915 m pour les gardes. C'est exactement le contraire.
		//
		// Les egares sont les points PRES DE L'OEIL, et quatre d'entre eux sont des
		// COINS DU TRONC DE VUE a 0,1 m : des points qui etaient VISIBLES PAR
		// DEFINITION -- ils sont dans le tronc -- et que le rabattement fait sortir du
		// visible en les posant au sol aux pieds d'une camera qui regarde devant elle.
		// C'est la demonstration la plus nette possible du mecanisme : ce n'est pas
		// leur position qui les perd, c'est l'ecrasement lui-meme.
		//
		// Et l'ecrasement MOYEN ne les distingue pas non plus (1,33 m pour les egares
		// contre 2,00 m pour les gardes) : ce n'est donc pas « de combien on aplatit »
		// qui decide, c'est OU tombe le point une fois aplati.
		//
		// ⚠️ CE QUE JE NE PRETENDS PAS. Le controle « pont 8 m » n'a PAS pu etre
		// calcule : il ne reste que 2 points gardes, la boite reduite est degeneree, et
		// l'instrument rend -1,00 au lieu d'inventer. C'est une limite de la mesure,
		// pas un resultat -- la contribution du rabattement n'est donc etablie QUE sur
		// la vue rasante.
		XCHECK(instrumentsAccordes == 2u,
			   "(x17) le nouvel instrument retrouve le gachis de (x9) sur l'etendue reelle");
		XCHECK(gachReduits[0] >= 0.f && gachReduits[0] < 0.50f,
			   "(x17b) LE RABATTEMENT EXPLIQUE LE GACHIS RASANT : sans les egares, sous 0,50");
	}

	// (x18) LE QUATRIEME CRITERE, MESURE AILLEURS QUE SUR LA POSE NOMINALE.
	//
	// 🔴 UNE DETTE TROUVEE PAR ACCIDENT, ET QUI N'A RIEN A VOIR AVEC CE QU'ON TOUCHE.
	// (x3) mesure le pas ecran depuis un pont : x1,00. J'ai rapporte ce x1,00 comme
	// s'il caracterisait la grille -- il ne caracterise que CETTE pose. En mesurant
	// la meme chose sur la vue rasante pour les besoins de (x17), elle est sortie a
	// x2,50 : la plus grande maille vaut deux fois et demie la plus petite, et aucun
	// temoin ne le disait.
	//
	// Ce n'est pas un prix qu'on vient de payer, c'etait deja la. Mais le critere
	// exige x1,00, donc ce temoin le DIT plutot que de laisser (x3) parler pour
	// toutes les poses, et il ROUGIT.
	{
		const Pose troisPoses[3] = {
			{{0.f, 8.f, 0.f}, {0.f, 2.f, -60.f}, "pont 8 m"},
			{{0.f, 1.f, 0.f}, {0.f, 1.2f, -60.f}, "rasante 1 m"},
			{{0.f, -3.f, 0.f}, {0.f, 1.f, -20.f}, "sous l'eau"},
		};
		float32 pire = 0.f;
		for (uint32 k = 0; k < 3u; ++k) {
			const NkProjectedGrid gk = BUILD(troisPoses[k], p);
			const float32 rap = RapportPasEcran(gk, p, VP(troisPoses[k]));
			if (rap > pire)
				pire = rap;
			std::fprintf(stderr, "     (x18) %-12s : pas ecran x%.2f\n", troisPoses[k].nom,
						 (double)rap);
		}
		std::fprintf(stderr, "     (x18) pire pas ecran sur les trois poses : x%.2f\n",
					 (double)pire);
		XCHECK(pire < 1.5f,
			   "(x18) le pas ecran reste quasi constant sur TOUTES les poses, pas seulement la nominale");
	}

	// (x19) LE RETRAIT, JUGE SUR LES QUATRE CRITERES ET SUR LES TROIS POSES.
	//
	// C'est un RETRAIT, pas une compensation : un point dont la position rabattue
	// n'est pas de l'eau que la camera voit n'a aucune raison d'etre dans l'etendue.
	// Mais un retrait se paie peut-etre ailleurs, et les quatre criteres comptent
	// tous les quatre :
	//   COUVERTURE 1,00  -- c'est LE risque du retrait : retirer un point retrecit
	//                       la boite, et ce qui n'etait couvert que PAR ce point
	//                       peut se decouvrir. Non negociable.
	//   STABILITE  >= 3,9 -- un critere d'appartenance BINAIRE peut faire entrer et
	//                       sortir un point d'une image a l'autre, et le sommet
	//                       correspondant sauterait. C'est le swimming, et c'est le
	//                       risque que je surveille en priorite.
	//   GACHIS     < 0,50
	//   PAS ECRAN  x1,00  -- mesure sur les TROIS poses, plus seulement la nominale.
	{
		const Pose troisPoses[3] = {
			{{0.f, 8.f, 0.f}, {0.f, 2.f, -60.f}, "pont 8 m"},
			{{0.f, 1.f, 0.f}, {0.f, 1.2f, -60.f}, "rasante 1 m"},
			{{0.f, -3.f, 0.f}, {0.f, 1.f, -20.f}, "sous l'eau"},
		};
		uint32 couvertes = 0, gachisOk = 0, pasOk = 0, stables = 0, stablesIndet = 0;
		for (uint32 k = 0; k < 3u; ++k) {
			const NkMat4f vpR = VP(troisPoses[k]);
			const NkMat4f invR = vpR.Inverse();
			NkProjectedGridParams pAvant = p, pApres = p;
			pAvant.retraitEgares = false;
			pApres.retraitEgares = true;

			const NkProjectedGrid gA = BUILD(troisPoses[k], pAvant);
			const NkProjectedGrid gB = BUILD(troisPoses[k], pApres);
			uint32 hA = 0, hB = 0, tA = 0, tB = 0;
			const float32 cA = CouvertureMin(gA, pAvant, invR, hA);
			const float32 cB = CouvertureMin(gB, pApres, invR, hB);
			const float32 gaA = Gachis(gA, pAvant, vpR, 2u, tA);
			const float32 gaB = Gachis(gB, pApres, vpR, 2u, tB);
			const float32 paA = RapportPasEcran(gA, pAvant, vpR);
			const float32 paB = RapportPasEcran(gB, pApres, vpR);

			// LA STABILITE APRES LE RETRAIT, mesuree comme toujours en derivee.
			const NkProjectedGrid s0 = BUILD(troisPoses[k], pApres);
			const NkProjectedGrid s1 = BUILD(TournerYaw(troisPoses[k], 1.f), pApres);
			const NkProjectedGrid sq = BUILD(TournerYaw(troisPoses[k], 0.25f), pApres);
			const float32 dUn = GlissementMoyen(s0, s1, pApres, 4u);
			const float32 dQuart = GlissementMoyen(s0, sq, pApres, 4u);
			const float32 rapport = (dQuart > 1e-9f) ? dUn / dQuart : 0.f;
			// Le compte leve l'ambiguite du zero : 0,00 sur 0 sommet compare n'est pas
			// une stabilite parfaite, c'est une absence de mesure.
			const uint32 comparables = SommetsComparables(s0, s1, pApres, 4u);

			std::fprintf(stderr,
						 "     (x19) %-12s : retrait %s | couverture %.2f -> %.2f (%u hauteurs)"
						 " | gachis %.2f -> %.2f | pas ecran x%.2f -> x%.2f | stabilite %.2f sur"
						 " %u sommets compares\n",
						 troisPoses[k].nom,
						 gB.retraitApplique ? "APPLIQUE " : (gB.retraitAbandonne ? "abandonne" : "inactif  "),
						 (double)cA, (double)cB, hB, (double)gaA, (double)gaB, (double)paA,
						 (double)paB, (double)rapport, comparables);

			if (hB > 0u && cB > 0.999f)
				++couvertes;
			if (gaB < 0.50f)
				++gachisOk;
			if (paB < 1.5f)
				++pasOk;
			// LE COMPTE PORTE L'ASSERTION. Sous le plancher, la pose sort INDETERMINEE
			// au lieu d'etre comptee comme un echec : « rien n'etait mesurable » et
			// « ca a saute » ne sont pas le meme verdict, et les confondre ferait
			// porter au correctif un tort qui n'est peut-etre pas le sien.
			if (comparables < kPlancherComparables)
				++stablesIndet;
			else if (rapport > 3.f && rapport < 5.f)
				++stables;
		}
		std::fprintf(stderr, "     (x19) stabilite : %u mesurables et lisses, %u INDETERMINEES\n",
					 stables, stablesIndet);
		XCHECK(couvertes == 3u, "(x19) COUVERTURE 1,00 sur les trois poses APRES le retrait");
		XCHECK(gachisOk == 3u, "(x19b) GACHIS sous 0,50 sur les trois poses");
		XCHECK(pasOk == 3u, "(x19c) PAS ECRAN quasi constant sur les TROIS poses");
		XCHECK(stablesIndet < 3u && stables == (3u - stablesIndet),
			   "(x19d) STABILITE : la ou c'est MESURABLE, le retrait ne fait pas sauter les sommets");
	}

	// (x20) POURQUOI LA STABILITE CASSE : LE CRITERE D'APPARTENANCE BASCULE.
	//
	// Le retrait repose sur un test BINAIRE -- egare ou non. Si un point change de
	// camp quand la camera tourne d'un degre, il entre ou sort de l'etendue, la
	// boite saute, et TOUS les sommets sautent avec elle. C'est exactement le
	// *swimming*, et (x10) le mesure a 419,79 pour 4,00 attendu.
	//
	// On ne suppose pas ce mecanisme : on compte les egares a plusieurs angles. Si
	// le compte change, la bascule est demontree. S'il ne change pas, la cause du
	// saut est ailleurs et il faudra la chercher -- ce temoin peut donc echouer a
	// prouver ce qu'il avance, et c'est ce qui le rend valable.
	{
		const Pose bancs[3] = {
			{{0.f, 8.f, 0.f}, {0.f, 2.f, -60.f}, "pont 8 m"},
			{{0.f, 1.f, 0.f}, {0.f, 1.2f, -60.f}, "rasante 1 m"},
			{{0.f, -3.f, 0.f}, {0.f, 1.f, -20.f}, "sous l'eau"},
		};
		const float32 angles[4] = {0.f, 0.25f, 0.5f, 1.f};
		uint32 basculesBinaire = 0, basculesContinu = 0;
		for (uint32 k = 0; k < 3u; ++k) {
			uint32 binaires[4] = {0u, 0u, 0u, 0u};
			uint32 continus[4] = {0u, 0u, 0u, 0u};
			for (uint32 a = 0; a < 4u; ++a) {
				const Pose po = TournerYaw(bancs[k], angles[a]);
				const NkProjectedGrid gk = BUILD(po, p);
				const NkMat4f renderVP = VP(po);
				uint32 b = 0, c = 0;
				for (uint32 i = 0; i < gk.ndcCount; ++i) {
					// L'ANCIEN critere, refait ici a l'identique : le point rabattu
					// est-il, OUI ou NON, de l'eau que la camera voit ?
					NkVec3f sol;
					if (SolDepuisNDC(gk, p, gk.ndcX[i], gk.ndcY[i], sol) &&
						!EauVisibleAuSol(renderVP, p, sol.x, sol.z))
						++b;
					// Le NOUVEAU : rabattu a plus de la moitie.
					if (gk.ndcEgare[i])
						++c;
				}
				binaires[a] = b;
				continus[a] = c;
			}
			const bool bB = (binaires[0] != binaires[1]) || (binaires[1] != binaires[2]) ||
							(binaires[2] != binaires[3]);
			const bool bC = (continus[0] != continus[1]) || (continus[1] != continus[2]) ||
							(continus[2] != continus[3]);
			if (bB)
				++basculesBinaire;
			if (bC)
				++basculesContinu;
			std::fprintf(stderr,
						 "     (x20) %-12s : critere BINAIRE a 0/0,25/0,5/1 deg = %u/%u/%u/%u (%s)"
						 " | critere CONTINU = %u/%u/%u/%u (%s)\n",
						 bancs[k].nom, binaires[0], binaires[1], binaires[2], binaires[3],
						 bB ? "BASCULE" : "stable", continus[0], continus[1], continus[2],
						 continus[3], bC ? "BASCULE" : "stable");
		}
		// ⚠️ CE TEMOIN A CHANGE DE SENS DEUX FOIS, ET LES DEUX SONT ECRITES.
		//
		// Version 1, quand le retrait etait BINAIRE : il demontrait la CAUSE du saut
		// -- l'appartenance bascule, la boite saute, les sommets sautent (6/6/4/5 sur
		// le pont). Version 2, apres le passage au continu : j'ai garde l'assertion
		// « ca bascule toujours » en la croyant inoffensive. ELLE EST DEVENUE FAUSSE :
		// 0 pose sur 3. Le nouveau critere (`t > 0,5`) n'est pas l'ancien (`marge <=
		// 0`), et les six « egares » du pont etaient tous MARGINALEMENT dehors --
		// c'est bien pour ca qu'un degre de rotation les faisait changer de camp.
		//
		// Version 3, celle-ci : on mesure LES DEUX CRITERES COTE A COTE sur les memes
		// images. L'ancien doit basculer -- sinon il n'y avait rien a corriger -- et le
		// nouveau doit rester stable. C'est la contre-epreuve a une seule variable du
		// passage au continu, et elle peut echouer des deux cotes.
		std::fprintf(stderr,
					 "     (x20) bascule : %u pose(s) sur 3 avec le critere BINAIRE, %u avec le"
					 " critere CONTINU\n",
					 basculesBinaire, basculesContinu);
		XCHECK(basculesBinaire > 0u && basculesContinu == 0u,
			   "(x20) l'ancien critere BASCULE, le nouveau NON : voila ce que le continu achete");
	}

	// (z1)(z2)(z3) LA COUVERTURE APRES DEPLACEMENT HORIZONTAL.
	//
	// LA FAILLE, DITE AVANT LA MESURE : la couverture 1,00 gagnee plus haut est une
	// garantie sur le PLAN DE REPOS. Ce que la camera voit est la surface DEPLACEE.
	// On a mesure la premiere et SUPPOSE la seconde. Et l'asymetrie est nette : la
	// verticale est gardee par `displacementMax`, l'HORIZONTALE ne l'est par rien --
	// donc c'est aux BORDS de l'etendue que ca doit se rouvrir.
	//
	// HYPOTHESE A REFUTER : la grille ne couvre plus l'eau deplacee sur une bande
	// large comme l'amplitude horizontale, le long des bords. CRITERE ECRIT
	// D'AVANCE : je la tiendrai pour confirmee si, a raideur >= 0,5, la couverture
	// PLEIN ECRAN tombe sous 0,999 PENDANT QUE la couverture CENTRALE reste a 1,000.
	// C'est ce couple, et non la seule chute, qui separe « au bord » de « reparti ».
	//
	// ⚠️ ET LE PIEGE DE COMMENSURABILITE, PARCE QUE JE VIENS D'EN PAYER UN. Si les
	// points de mesure tombent sur des positions commensurables avec la periode de
	// la vague, on mesure toujours la MEME PHASE -- exactement la faute des quatre
	// resolutions multiples de 4 qui echantillonnaient le sinus pile a +/-1. Des
	// deux parades possibles, celle que j'ai prise est le DECALAGE DE PHASE PAR
	// POSE (0 / 1,1 / 2,3 rad), pas des pas premiers avec la longueur d'onde.
	{
		const Pose troisPoses[3] = {
			{{0.f, 8.f, 0.f}, {0.f, 2.f, -60.f}, "pont 8 m"},
			{{0.f, 1.f, 0.f}, {0.f, 1.2f, -60.f}, "rasante 1 m"},
			{{0.f, -3.f, 0.f}, {0.f, 1.f, -20.f}, "sous l'eau"},
		};
		const float32 phases[3] = {0.f, 1.1f, 2.3f};
		const float32 raideurs[5] = {0.f, 0.25f, 0.5f, 0.75f, 1.f};
		float32 px[512], pz[512];
		uint32 idx[512];

		uint32 controleOk = 0, confirmees = 0, auBord = 0;
		for (uint32 k = 0; k < 3u; ++k) {
			const NkMat4f vpR = VP(troisPoses[k]);
			const NkMat4f invR = vpR.Inverse();
			const NkProjectedGrid gk = BUILD(troisPoses[k], p);
			for (uint32 r = 0; r < 5u; ++r) {
				const NkWaterParams houle = UneHouleTest(20.f, 0.6f, raideurs[r], phases[k]);
				float32 jMin = 0.f;
				const uint32 m = EmpreinteDeplacee(gk, p, houle, 0.f, gk.ndcMinX, gk.ndcMaxX,
												   gk.ndcMinY, gk.ndcMaxY, px, pz, idx, 512u, jMin);
				uint32 vusPlein = 0, vusCentre = 0;
				const float32 cPlein =
					CouvertureComposee(invR, p.baseY, 48u, px, pz, idx, m, 1.f, vusPlein);
				const float32 cCentre =
					CouvertureComposee(invR, p.baseY, 48u, px, pz, idx, m, 0.5f, vusCentre);
				std::fprintf(stderr,
							 "     (z2) %-12s raideur %.2f : J min %+.3f | couverture composee"
							 " PLEIN ECRAN %.3f (%u px) | CENTRE %.3f (%u px)\n",
							 troisPoses[k].nom, (double)raideurs[r], (double)jMin, (double)cPlein,
							 vusPlein, (double)cCentre, vusCentre);
				// (z1) LE CONTROLE NEGATIF : a raideur nulle, rien ne bouge, donc la
				// couverture composee DOIT valoir 1,000. Si elle ne le vaut pas, c'est
				// ma mesure qui est fausse -- pas la grille.
				if (r == 0u && cPlein > 0.999f && cCentre > 0.999f)
					++controleOk;
				if (raideurs[r] >= 0.5f && cPlein < 0.999f) {
					++confirmees;
					if (cCentre > 0.999f)
						++auBord;
				}
			}
		}
		std::fprintf(stderr,
					 "     (z1/z2) controle negatif OK sur %u/3 poses | %u cas (raideur >= 0,5)"
					 " perdent de la couverture, dont %u AU BORD (centre intact)\n",
					 controleOk, confirmees, auBord);
		XCHECK(controleOk == 3u,
			   "(z1) CONTROLE NEGATIF : a raideur nulle, la couverture composee vaut 1,000");

		// (z3) LE PRIX DE LA GARDE. Elargir l'etendue rachete la couverture, mais
		// c'est du GACHIS en plus -- et on a mesure que descendre le gachis a 0,58
		// detruirait le 1,00 contre 251,00 qui justifie toute la technique. Une
		// couverture rachetee trop cher n'est pas un gain, donc on donne LES DEUX
		// chiffres avant de proposer quoi que ce soit.
		{
			const Pose banc = troisPoses[1]; // la rasante : c'est elle qui a deja souffert
			const NkMat4f vpR = VP(banc);
			const NkMat4f invR = vpR.Inverse();
			const NkProjectedGrid gk = BUILD(banc, p);
			const NkWaterParams houle = UneHouleTest(20.f, 0.6f, 1.f, phases[1]);
			const float32 marges[4] = {0.f, 0.05f, 0.10f, 0.20f};
			for (uint32 e = 0; e < 4u; ++e) {
				const float32 ex = (gk.ndcMaxX - gk.ndcMinX) * marges[e];
				const float32 ey = (gk.ndcMaxY - gk.ndcMinY) * marges[e];
				const float32 x0 = gk.ndcMinX - ex, x1 = gk.ndcMaxX + ex;
				const float32 y0 = gk.ndcMinY - ey, y1 = gk.ndcMaxY + ey;
				float32 jMin = 0.f;
				const uint32 m =
					EmpreinteDeplacee(gk, p, houle, 0.f, x0, x1, y0, y1, px, pz, idx, 512u, jMin);
				uint32 vus = 0, tot = 0;
				const float32 c = CouvertureComposee(invR, p.baseY, 48u, px, pz, idx, m, 1.f, vus);
				const float32 gach = GachisSurEtendue(gk, p, vpR, x0, x1, y0, y1, 2u, tot);
				std::fprintf(stderr,
							 "     (z3) elargissement %3.0f %% : couverture composee %.3f | gachis"
							 " %.2f (%u sommets)\n",
							 (double)(marges[e] * 100.f), (double)c, (double)gach, tot);
			}
		}
	}

	// (k1)(k2)(k3) LA PREIMAGE : re-former l'etendue au lieu de la rembourrer.
	//
	// LE RAISONNEMENT VIENT DE L'ECHEC PRECEDENT. La grille couvre R au repos et
	// affiche D(R) ; exiger que D(R) contienne V revient a construire R contenant
	// D^-1(V). Elargir R « partout » a ete mesure puis REFUSE : +20 % ne rendait que
	// 0,741 de couverture en faisant passer le gachis de 0,20 a 0,52, au-dela de son
	// seuil. La preimage n'ajoute aucun sommet : elle les RE-FORME.
	//
	// ⚠️ DEUX CHOSES DITES AVANT LES CHIFFRES, pour qu'on ne les lise pas de travers.
	// 1. La couverture de (k2) est QUASI TAUTOLOGIQUE a convergence donnee : si
	//    Newton converge, D(D^-1(W)) = W et la couverture revient mecaniquement a sa
	//    valeur de repos. Ce 1,000 mesure la CONVERGENCE. Les nombres porteurs sont
	//    le residu, les iterations et les refus.
	// 2. Le gachis est INCHANGE PAR CONSTRUCTION, et ce n'est pas un gain mesure :
	//    la preimage bouge les sommets AU REPOS, alors que le gachis porte sur les
	//    positions AFFICHEES, qui restent W. S'il s'ameliorait, c'est que je
	//    mesurerais autre chose.
	//
	// 🔴 ET MON PREMIER CRITERE DE CONVERGENCE ETAIT FAUX D'AVANCE. Je l'avais ecrit
	// en ABSOLU -- residu sous 1e-4 m -- sans mesurer le plancher de bruit de
	// l'arithmetique a ces magnitudes. Mesure : le residu plafonnait a 1,22e-04 et
	// 2,44e-04 m avec les 24 iterations toujours consommees. Or l'etendue s'etale
	// jusqu'a ~2 000 m, ou 1 ULP de float32 vaut EXACTEMENT 2,44e-04 m : les residus
	// etaient a 1 et 2 ULP, c'est-a-dire que Newton avait converge aussi loin que le
	// type le permet et que j'exigeais mieux que ce qu'il peut representer. Le seuil
	// est donc devenu RELATIF a la magnitude -- pas desserre au jugé.
	{
		const Pose troisPoses[3] = {
			{{0.f, 8.f, 0.f}, {0.f, 2.f, -60.f}, "pont 8 m"},
			{{0.f, 1.f, 0.f}, {0.f, 1.2f, -60.f}, "rasante 1 m"},
			{{0.f, -3.f, 0.f}, {0.f, 1.f, -20.f}, "sous l'eau"},
		};
		const float32 phases[3] = {0.f, 1.1f, 2.3f};
		const float32 raideurs[4] = {0.f, 0.5f, 0.75f, 0.9f};
		float32 px[512], pz[512];
		uint32 idx[512];
		uint32 controleOk = 0, restaurees = 0, casMesures = 0, gachisTenu = 0;
		float32 pireResidu = 0.f;
		uint32 pireIter = 0u;

		for (uint32 k = 0; k < 3u; ++k) {
			const NkMat4f vpR = VP(troisPoses[k]);
			const NkMat4f invR = vpR.Inverse();
			const NkProjectedGrid gk = BUILD(troisPoses[k], p);
			uint32 totRef = 0;
			const float32 gachRef = Gachis(gk, p, vpR, 2u, totRef);
			for (uint32 r = 0; r < 4u; ++r) {
				const NkWaterParams houle = UneHouleTest(20.f, 0.6f, raideurs[r], phases[k]);
				float32 residu = 0.f;
				uint32 iter = 0u, refus = 0u;
				const uint32 m =
					EmpreintePreimage(gk, p, houle, 0.f, gk.ndcMinX, gk.ndcMaxX, gk.ndcMinY,
									  gk.ndcMaxY, px, pz, idx, 512u, residu, iter, refus);
				uint32 vus = 0;
				const float32 c = CouvertureComposee(invR, p.baseY, 48u, px, pz, idx, m, 1.f, vus);
				uint32 tot = 0;
				const float32 gach = Gachis(gk, p, vpR, 2u, tot);
				if (residu > pireResidu)
					pireResidu = residu;
				if (iter > pireIter)
					pireIter = iter;
				std::fprintf(stderr,
							 "     (k2) %-12s raideur %.2f : couverture RE-FORMEE %.3f | residu max"
							 " %.2e m | %u iterations max | %u refus | gachis %.2f (ref %.2f)\n",
							 troisPoses[k].nom, (double)raideurs[r], (double)c, (double)residu,
							 iter, refus, (double)gach, (double)gachRef);
				if (r == 0u && c > 0.999f && residu < 1e-6f)
					++controleOk;
				if (raideurs[r] > 0.f) {
					++casMesures;
					if (c > 0.999f && refus == 0u)
						++restaurees;
					if (NkFabs(gach - gachRef) < 0.01f)
						++gachisTenu;
				}
			}
		}
		std::fprintf(stderr,
					 "     (w1/w2) controle negatif %u/3 | %u cas sur %u restaures a 1,000 sans"
					 " refus | gachis tenu sur %u | pire residu %.2e m en %u iterations\n",
					 controleOk, restaurees, casMesures, gachisTenu, (double)pireResidu, pireIter);
		XCHECK(controleOk == 3u,
			   "(k1) CONTROLE NEGATIF : a raideur nulle la preimage est l'identite, residu nul");
		XCHECK(restaurees == casMesures && casMesures > 0u,
			   "(k2) la preimage RESTAURE la couverture composee a 1,000, sans refus");
		XCHECK(gachisTenu == casMesures,
			   "(k2b) et le gachis ne bouge pas : aucun sommet ajoute, l'etendue est re-formee");

		// (k3) LA GARDE DOIT FAIRE FEU, PAS EXISTER. Une capacite qui declare refuser
		// et qu'on ne fait jamais refuser n'a qu'une INTENTION pour garantie -- c'est
		// ce que (x7) avait deja etabli. On pousse donc la raideur AU-DELA de 1, la
		// ou J passe sous 0 : la preimage n'y est plus UNIQUE, et Newton doit REFUSER
		// au lieu d'en rendre une au hasard.
		{
			const Pose banc = troisPoses[1];
			const NkProjectedGrid gk = BUILD(banc, p);
			const float32 dures[3] = {1.0f, 1.3f, 1.8f};
			uint32 avecRefus = 0;
			for (uint32 r = 0; r < 3u; ++r) {
				const NkWaterParams houle = UneHouleTest(20.f, 0.6f, dures[r], phases[1]);
				float32 residu = 0.f;
				uint32 iter = 0u, refus = 0u;
				const uint32 m =
					EmpreintePreimage(gk, p, houle, 0.f, gk.ndcMinX, gk.ndcMaxX, gk.ndcMinY,
									  gk.ndcMaxY, px, pz, idx, 512u, residu, iter, refus);
				std::fprintf(stderr,
							 "     (k3) raideur %.2f (J min ~ %.2f) : %u sommets, %u REFUS, residu"
							 " max %.2e m, %u iterations\n",
							 (double)dures[r], (double)(1.f - dures[r]), m, refus, (double)residu,
							 iter);
				if (refus > 0u)
					++avecRefus;
			}
			XCHECK(avecRefus > 0u,
				   "(k3) au-dela de raideur 1, la preimage REFUSE au lieu de rendre un resultat non converge");
		}
	}

	// (n1)(n2) LE PAVAGE : quatre nombres, aucun jugement.
	//
	// C'etait la seule piece de la chaine minimale que personne n'avait ecrite. Une
	// demonstration est VISUELLE, et ce chantier s'interdit de rien regler a l'oeil :
	// la justesse du pavage se mesure donc SANS REGARDER. Un maillage qui satisfait
	// les quatre nombres ci-dessous n'a PAS DE FISSURES -- et « pas de fissures » est
	// exactement ce qu'un oeil chercherait en premier. Ce que l'oeil garde, c'est le
	// GOUT, et celui-la appartient a Rodolf.
	{
		const uint32 cols = 8u, rows = 8u;
		uint32 tri[512];
		const uint32 n = NkProjectedGridIndices(cols, rows, tri, 512u);

		// ⚠️ LE COMPTE ATTENDU EST CALCULE ICI, depuis nx et ny -- il n'est PAS
		// demande a la fonction qu'on teste. Comparer une fonction a elle-meme ne
		// prouve rien ; on confronte les deux a un troisieme chiffre ecrit a la main.
		const uint32 nx = cols + 1u, ny = rows + 1u;
		const uint32 attendu = 2u * (nx - 1u) * (ny - 1u) * 3u;
		// Et le bord, lui aussi compte a la main : 2 (cols + rows) aretes.
		const uint32 bordAttendu = 2u * (cols + rows);
		const NkPavageMesure m = MesurerPavage(tri, n, cols, rows);

		std::fprintf(stderr,
					 "     (n1) %u x %u cellules : %u indices (attendu %u, fonction %u) | aretes"
					 " vues une fois %u (bord attendu %u), trois fois ou plus %u | orientation"
					 " +%u / -%u | degeneres %u | hors bornes %u\n",
					 cols, rows, n, attendu, NkProjectedGridIndexCount(cols, rows),
					 m.aretesUneFois, bordAttendu, m.aretesTrois, m.orientationPos,
					 m.orientationNeg, m.degeneres, m.horsBornes);

		XCHECK(n == attendu && NkProjectedGridIndexCount(cols, rows) == attendu,
			   "(n1) le compte d'indices vaut EXACTEMENT 2 (nx-1)(ny-1) 3");
		XCHECK(m.aretesTrois == 0u && m.aretesUneFois == bordAttendu,
			   "(n1b) chaque arete INTERIEURE est partagee par exactement DEUX triangles");
		XCHECK(m.orientationNeg == 0u && m.orientationPos == n / 3u,
			   "(n1c) l'enroulement est COHERENT : aucun triangle a contresens");
		XCHECK(m.degeneres == 0u && m.horsBornes == 0u,
			   "(n1d) aucun triangle degenere, aucun indice hors bornes");

		// (n2) LE VOLET NEGATIF. Sans lui, (n1) ne prouve que « ca compile ».
		//
		// ⚠️ ET JE NE PRETENDS PAS QUE LES QUATRE ROUGISSENT. Deplacer UN indice
		// laisse le COMPTE inchange -- il est structurellement aveugle a la topologie.
		// On MESURE donc lequel des quatre attrape la faute, au lieu d'annoncer un
		// « tout rougit » que la mecanique interdit.
		{
			uint32 casse[512];
			for (uint32 i = 0; i < n; ++i)
				casse[i] = tri[i];
			casse[7] = casse[7] + 1u; // un seul indice d'un quad, deplace d'un sommet
			const NkPavageMesure b = MesurerPavage(casse, n, cols, rows);
			const bool compteVoit = (n != attendu);
			const bool aretesVoient = (b.aretesUneFois != bordAttendu) || (b.aretesTrois != 0u);
			const bool enroulVoit = (b.orientationNeg != 0u);
			const bool degenVoit = (b.degeneres != 0u || b.horsBornes != 0u);
			std::fprintf(stderr,
						 "     (n2) UN indice deplace : aretes une fois %u (contre %u), trois fois"
						 " %u | orientation +%u / -%u | degeneres %u -> attrape par :"
						 " compte=%d aretes=%d enroulement=%d degeneres=%d\n",
						 b.aretesUneFois, bordAttendu, b.aretesTrois, b.orientationPos,
						 b.orientationNeg, b.degeneres, compteVoit ? 1 : 0, aretesVoient ? 1 : 0,
						 enroulVoit ? 1 : 0, degenVoit ? 1 : 0);
			XCHECK(aretesVoient || enroulVoit || degenVoit,
				   "(n2) une seule maille cassee est ATTRAPEE par les nombres qui peuvent la voir");
		}

		// (n2b) ET LE REFUS D'ECRIRE UN MAILLAGE TRONQUE. Une capacite insuffisante
		// doit rendre ZERO et ne rien ecrire : un pavage a moitie ecrit serait un
		// maillage a fissures que personne ne verrait venir.
		{
			uint32 petit[8];
			for (uint32 i = 0; i < 8u; ++i)
				petit[i] = 0xFFFFFFFFu;
			const uint32 z = NkProjectedGridIndices(cols, rows, petit, 8u);
			bool intact = true;
			for (uint32 i = 0; i < 8u; ++i)
				if (petit[i] != 0xFFFFFFFFu)
					intact = false;
			std::fprintf(stderr, "     (n2b) capacite 8 pour %u indices : rend %u, tampon intact=%d\n",
						 attendu, z, intact ? 1 : 0);
			XCHECK(z == 0u && intact,
				   "(n2b) capacite insuffisante : le pavage REFUSE et n'ecrit RIEN");
		}
	}

	// (p1)(p2) LE PRODUCTEUR NKVFX, EPROUVE SANS FENETRE.
	//
	// Il ne connait aucun de ses hotes : ni ECS, ni Noge, ni application. On lui
	// donne une camera et de la memoire, il rend des sommets et des indices. C'est
	// ce qui permet de le mesurer ici, sans device et sans fenetre.
	//
	// ⚠️ BASE_Y N'EST PAS ZERO, ET C'EST DELIBERE. `NkWaterEval` rend une hauteur
	// de vague AUTOUR DE ZERO et ignore le plan de repos ; la grille, elle, pose
	// ses sommets SUR ce plan. Si le producteur oubliait de composer les deux,
	// l'eau serait dessinee a l'altitude zero -- et un temoin ecrit avec baseY = 0
	// ne le verrait JAMAIS, les deux valeurs coincidant. C'est la lecon de la
	// donnee d'essai muette, payee deux fois cette nuit.
	{
		const Pose pose = {{0.f, 11.f, 0.f}, {0.f, 5.f, -60.f}, "pont, plan a 3 m"};
		vfx::NkWaterMeshParams wp;
		wp.grid = p;
		wp.grid.cols = 8u;
		wp.grid.rows = 8u;
		wp.grid.baseY = 3.f; // NON NUL : voir ci-dessus
		wp.waves = UneHouleTest(20.f, 0.6f, 0.5f, 0.f);
		wp.time = 0.f;

		// LES COMPTES ATTENDUS SONT CALCULES A LA MAIN, jamais demandes aux
		// fonctions qu'on teste : comparer une fonction a elle-meme ne prouve rien.
		const uint32 sommetsAttendus = 9u * 9u;	   // (cols+1)(rows+1)
		const uint32 indicesAttendus = 8u * 8u * 6u; // cols*rows*6
		const uint32 bordAttendu = 2u * (8u + 8u);

		renderer::NkVertex3D sommets[128];
		uint32 tri[512];
		uint32 manquants = 0u;
		const uint32 nv = vfx::NkWaterBuildVertices(PROJ(), VUE(pose), pose.oeil, wp, sommets,
													128u, &manquants);
		const uint32 ni = vfx::NkWaterBuildIndices(wp.grid, tri, 512u);
		const NkPavageMesure m = MesurerPavage(tri, ni, wp.grid.cols, wp.grid.rows);

		uint32 nan = 0u;
		for (uint32 k = 0; k < nv; ++k)
			if (!EstFini(sommets[k].pos.x) || !EstFini(sommets[k].pos.y) ||
				!EstFini(sommets[k].pos.z) || !EstFini(sommets[k].normal.y))
				++nan;

		std::fprintf(stderr,
					 "     (p1) producteur : %u sommets (attendu %u, manquants %u) | %u indices"
					 " (attendu %u) | aretes une fois %u (bord %u), trois fois %u | NaN %u\n",
					 nv, sommetsAttendus, manquants, ni, indicesAttendus, m.aretesUneFois,
					 bordAttendu, m.aretesTrois, nan);

		XCHECK(nv == sommetsAttendus && manquants == 0u,
			   "(p1) le producteur rend la grille PLEINE, aucun sommet manquant");
		XCHECK(ni == indicesAttendus, "(p1b) et le compte d'indices vaut 2 (nx-1)(ny-1) 3");
		// ⚠️ SEUL L'APPARIEMENT D'ARETES VOIT une maille cassee : mesure en (n2), le
		// compte, l'enroulement et la degenerescence sont AVEUGLES a un indice
		// deplace. Ce temoin s'appuie donc sur celui des quatre nombres qui juge.
		XCHECK(m.aretesUneFois == bordAttendu && m.aretesTrois == 0u,
			   "(p1c) topologie saine -- et c'est l'APPARIEMENT D'ARETES qui le dit, pas les quatre");
		XCHECK(nan == 0u, "(p1d) aucun NaN dans les positions ni les normales");

		// (p2) CONTROLE NEGATIF : amplitude ET raideur nulles -> rien ne bouge, et le
		// producteur doit rendre EXACTEMENT le plan de repos de la grille. Pas « a peu
		// pres » : au bit, comme (y1).
		{
			vfx::NkWaterMeshParams plat = wp;
			plat.waves = UneHouleTest(20.f, 0.f, 0.f, 0.f);
			renderer::NkVertex3D calmes[128];
			uint32 manq2 = 0u;
			const uint32 nv2 = vfx::NkWaterBuildVertices(PROJ(), VUE(pose), pose.oeil, plat,
														 calmes, 128u, &manq2);
			const NkProjectedGrid g =
				NkProjectedGridBuild(PROJ(), VUE(pose), pose.oeil, plat.grid);
			uint32 ecarts = 0u, horsPlan = 0u;
			float32 pireEcart = 0.f;
			for (uint32 j = 0; j <= plat.grid.rows; ++j)
				for (uint32 i = 0; i <= plat.grid.cols; ++i) {
					NkVec3f base;
					if (!NkProjectedGridVertex(g, plat.grid, i, j, base))
						continue;
					const NkVec3f &q = calmes[j * (plat.grid.cols + 1u) + i].pos;
					const float32 d = (q - base).Len();
					if (d != 0.f) {
						++ecarts;
						if (d > pireEcart)
							pireEcart = d;
					}
					if (q.y != plat.grid.baseY)
						++horsPlan;
				}
			std::fprintf(stderr,
						 "     (p2) houle nulle, plan a %.1f m : %u sommets, %u ecarts au plan de"
						 " repos (pire %.3e m), %u hors du plan\n",
						 (double)plat.grid.baseY, nv2, ecarts, (double)pireEcart, horsPlan);
			XCHECK(nv2 == sommetsAttendus && manq2 == 0u && ecarts == 0u && horsPlan == 0u,
				   "(p2) CONTROLE NEGATIF : a houle nulle le producteur rend le plan de repos AU BIT");
		}
	}

	std::fprintf(stderr, "=== grille projetee : %d passes, %d echecs ===\n", gP, gF);
	return gF;
}
