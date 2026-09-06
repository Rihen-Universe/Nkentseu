#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkProjectedGrid.h — LE MAILLAGE DE L'OCÉAN : une grille uniforme à l'ÉCRAN,
// posée sur le plan d'eau (2026-09-07, nuit). ROADMAP_PRODUITS.md §6.6.
//
// Référence : Claes Johanson, « Real-time water rendering — Introducing the
// projected grid concept », mémoire de master, université de Lund, 2004.
// ⚠️ Ce que je cite est le CONCEPT et la construction de la caméra de portée
// (§2.3 à §2.5) ; je ne cite aucun numéro d'équation, parce que je n'ai pas le
// document sous les yeux cette nuit. Ce qui suit est écrit depuis le principe,
// et chaque décision qui s'en écarte est nommée dans le texte.
//
// ── LE PROBLÈME, ET POURQUOI UNE GRILLE MONDE NE LE RÉSOUT PAS ──────────────
// Un océan est plat et immense. Une grille en espace MONDE dépense la même
// densité de sommets à trois mètres de l'œil et à trois kilomètres : de près
// elle est trop grossière, de loin elle est du gâchis, et il faut choisir ses
// bornes à la main. Une grille en espace ÉCRAN dépense exactement un sommet par
// bloc de pixels, quelle que soit la distance — la densité qu'on paie est celle
// qu'on voit. C'est tout l'apport de Johanson, et c'est ce que le témoin (x3)
// mesure : le rapport entre la plus grande et la plus petite maille à l'écran.
//
// ── COMMENT ─────────────────────────────────────────────────────────────────
// 1. On déprojette les 8 coins du cube NDC : le tronc de vue en monde.
// 2. On coupe ses 12 arêtes par les DEUX plans y = base ± déplacement — la
//    tranche que la houle peut occuper. On garde aussi les coins du tronc qui
//    sont DANS la tranche.
//    ⚠️ Couper par le seul plan de repos ne suffit pas : une vague qui monte
//    entre dans le champ par le haut de l'écran, et la grille bâtie sur le plan
//    de repos ne la couvrirait pas. C'est le défaut classique de cette famille.
// 3. On rabat ces points sur le plan de repos et on cherche leur étendue dans
//    l'espace projectif de la caméra.
// 4. La grille [0,1]² est étalée sur cette étendue, puis chaque sommet est
//    redéprojeté vers le plan de repos. La hauteur vient ensuite de la houle.
//
// ── LE DOMAINE DE CETTE VERSION, ET CE QU'IL Y MANQUE ───────────────────────
// 🔴 CETTE GRILLE EXIGE QUE L'ŒIL SOIT AU-DESSUS DE LA TRANCHE :
//        eye.y > baseY + displacementMax
// En dehors, elle REFUSE (`horsDomaine`) au lieu de rendre une grille plausible.
//
// Ce n'est pas une précaution de principe, c'est une MESURE : œil à 1 m, crête à
// 2 m, sur 1 225 pixels où la surface déplacée se voit, la grille en couvrait
// ZÉRO (témoin (x2), le 2026-09-07). La cause n'est pas une étendue trop
// serrée — c'est que la crête, passée au-dessus de l'œil, se voit AU-DESSUS de
// l'horizon du plan de repos, et qu'un rayon parti au-dessus de cet horizon ne
// rencontre jamais ce plan. Élargir l'étendue n'y change rien : il n'y a aucun
// sommet à poser là.
//
// CE QUI MANQUE A UN NOM : la CAMÉRA DE PORTÉE de Johanson (§2.4-2.5). Au lieu
// de lancer les rayons depuis la caméra de rendu, on les lance depuis une caméra
// dérivée dont l'orientation est CONTRAINTE à regarder le plan — ainsi chaque
// rayon le rencontre, quelle que soit la pose. Son placement est un compromis
// VISUEL que Johanson règle à l'œil ; il n'a pas été écrit ici parce qu'on ne
// règle pas à l'œil ce qu'on ne peut pas regarder.
//
// ⚠️ CE QUE LE DOMAINE COÛTE, dit franchement : pas de caméra au ras de l'eau,
// pas de caméra sous l'eau, pas de vague qui passe au-dessus de l'objectif.
// C'est-à-dire trois des plans les plus spectaculaires d'un océan. La version
// utile aujourd'hui est la vue large depuis un pont ou une falaise.
//
// ── LE CAS DÉGÉNÉRÉ, DIT PLUTÔT QUE MASQUÉ ──────────────────────────────────
// Quand un point retenu est DERRIÈRE la caméra (w <= 0), son image projective
// n'existe pas : l'étendue devient non bornée. On ne l'invente pas et on ne la
// tronque pas en silence — on retombe sur l'étendue PLEINE de l'écran
// ([-1,1] élargi de `edgeBias`), et `repliPleinEcran` le DIT à l'appelant.
// C'est plus large que nécessaire, donc plus cher, et jamais faux : on se trompe
// du côté qui laisse voir.
//
// ⚠️ CE QUE CE FICHIER N'EST PAS. Il ne dessine rien, il ne connaît ni GPU ni
// tampon de sommets : il rend des POSITIONS. Le pavage, les indices et le rendu
// sont au consommateur. C'est la même séparation que NkSplashLaw : la seule
// chose qui puisse être fausse ici est de la géométrie, et de la géométrie se
// mesure sans écran.
// =============================================================================
#include "NKMath/NkFunctions.h"
#include "NKMath/NkMat.h"
#include "NKMath/NkVec.h"

namespace nkentseu {
	namespace math {

		struct NkProjectedGridParams {
				// Résolution de la grille EN ÉCRAN. C'est un budget de sommets, pas une
				// étendue en mètres : il n'y a pas d'étendue en mètres ici.
				uint32 cols = 128u;
				uint32 rows = 128u;
				// Altitude du plan de repos (m).
				float32 baseY = 0.f;
				// Demi-épaisseur de la tranche que la houle peut occuper (m). Doit
				// MAJORER l'amplitude crête-à-creux réelle, sinon une vague peut entrer
				// par le bord de l'écran sans que la grille l'atteigne.
				float32 displacementMax = 2.f;
				// Marge en NDC. Johanson en met une : sans elle, un sommet exactement au
				// bord se retrouve à l'extérieur après déplacement, et l'eau décolle du
				// bord de l'écran d'un pixel — un défaut qu'on ne voit qu'en mouvement.
				float32 edgeBias = 0.05f;
		};

		struct NkProjectedGrid {
				// Faux = la tranche d'eau ne coupe pas le tronc de vue : il n'y a rien à
				// mailler, et ça se DIT (au lieu de rendre une grille vide plausible).
				bool visible = false;
				// Vrai = l'œil n'est PAS au-dessus de la tranche : hors du domaine de
				// cette version. `visible` reste faux. Voir l'en-tête : ce qui manque
				// est la caméra de portée de Johanson, et elle a un nom.
				bool horsDomaine = false;
				// Vrai = l'étendue projective n'était pas bornée (un point retenu était
				// derrière la caméra) et on a pris l'écran entier. Voir l'en-tête.
				bool repliPleinEcran = false;
				// L'étendue retenue, en NDC.
				float32 ndcMinX = -1.f, ndcMaxX = 1.f;
				float32 ndcMinY = -1.f, ndcMaxY = 1.f;
				// Combien de points ont servi à la construire (0 = invisible).
				uint32 pointsRetenus = 0u;
				// L'inverse de la view-projection, gardée pour déprojeter les sommets.
				NkMat4f invViewProj;
		};

		// Déprojette un point NDC vers le monde. Rend faux quand la division
		// projective n'a pas de sens (w ~ 0) — un point à l'infini n'est pas un
		// point, et le taire produirait des NaN qui voyagent.
		NK_FORCE_INLINE bool NkUnprojectNDC(const NkMat4f &invVP, float32 nx, float32 ny, float32 nz,
											NkVec3f &out) noexcept {
			const NkVec4f q = invVP * NkVec4f(nx, ny, nz, 1.f);
			if (NkFabs(q.w) < 1e-9f)
				return false;
			const float32 inv = 1.f / q.w;
			out = NkVec3f{q.x * inv, q.y * inv, q.z * inv};
			return true;
		}

		// Intersection segment [a, b] avec le plan horizontal y = py. Rend faux si le
		// segment ne le traverse pas (les deux extrémités du même côté, ou parallèle).
		NK_FORCE_INLINE bool NkSegmentPlanY(const NkVec3f &a, const NkVec3f &b, float32 py,
											NkVec3f &out) noexcept {
			const float32 da = a.y - py, db = b.y - py;
			if ((da > 0.f && db > 0.f) || (da < 0.f && db < 0.f))
				return false;
			const float32 d = da - db;
			if (NkFabs(d) < 1e-9f)
				return false; // arête dans le plan : ses deux extrémités sont déjà retenues
			const float32 t = da / d;
			if (t < 0.f || t > 1.f)
				return false;
			out = a + (b - a) * t;
			return true;
		}

		// Construit la grille pour une view-projection donnée.
		// `eye` est demande EXPLICITEMENT plutot que deduit de la matrice : la
		// deduire couterait une inversion de plus et, surtout, l'appelant l'a deja.
		// Un parametre qu'on peut donner ne se devine pas.
		inline NkProjectedGrid NkProjectedGridBuild(const NkMat4f &viewProj, const NkVec3f &eye,
													const NkProjectedGridParams &p) noexcept {
			NkProjectedGrid g;
			g.invViewProj = viewProj.Inverse();

			// LE DOMAINE, VERIFIE AVANT TOUT LE RESTE. Hors domaine on REFUSE : la
			// mesure du 07/09 dit qu'on y couvrait 0 pixel sur 1 225, et une grille
			// qui ne couvre rien tout en se declarant visible est pire qu'une
			// absence -- elle fait chercher le defaut ailleurs.
			if (eye.y <= p.baseY + p.displacementMax) {
				g.horsDomaine = true;
				return g;
			}

			// 1. les 8 coins du tronc, en monde.
			NkVec3f coins[8];
			bool coinOk[8];
			uint32 nCoins = 0u;
			for (uint32 k = 0; k < 8u; ++k) {
				const float32 nx = (k & 1u) ? 1.f : -1.f;
				const float32 ny = (k & 2u) ? 1.f : -1.f;
				const float32 nz = (k & 4u) ? 1.f : -1.f;
				coinOk[k] = NkUnprojectNDC(g.invViewProj, nx, ny, nz, coins[k]);
				if (coinOk[k])
					++nCoins;
			}
			if (nCoins < 8u)
				return g; // une projection dégénérée ne rend pas une grille approximative

			// 2. les 12 arêtes, coupées par les deux plans de la tranche.
			static const int kAretes[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
											   {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
			const float32 yHaut = p.baseY + p.displacementMax;
			const float32 yBas = p.baseY - p.displacementMax;

			NkVec3f pts[40];
			uint32 n = 0u;
			for (uint32 e = 0; e < 12u && n + 2u < 40u; ++e) {
				const NkVec3f &a = coins[kAretes[e][0]];
				const NkVec3f &b = coins[kAretes[e][1]];
				NkVec3f h;
				if (NkSegmentPlanY(a, b, yHaut, h))
					pts[n++] = h;
				if (NkSegmentPlanY(a, b, yBas, h))
					pts[n++] = h;
			}
			// les coins DANS la tranche comptent aussi : sous l'eau, ou juste au-dessus,
			// aucune arête ne traverse et pourtant tout l'écran est de l'eau.
			for (uint32 k = 0; k < 8u && n < 40u; ++k) {
				if (coins[k].y <= yHaut && coins[k].y >= yBas)
					pts[n++] = coins[k];
			}
			if (n == 0u)
				return g; // rien : la tranche d'eau ne coupe pas le tronc

			// 3. rabattus sur le plan de repos, puis ramenés en NDC.
			float32 mnx = 1e30f, mxx = -1e30f, mny = 1e30f, mxy = -1e30f;
			bool borne = true;
			for (uint32 i = 0; i < n; ++i) {
				const NkVec4f q = viewProj * NkVec4f(pts[i].x, p.baseY, pts[i].z, 1.f);
				if (q.w <= 1e-6f) {
					borne = false; // derrière la caméra : pas d'image projective
					break;
				}
				const float32 iw = 1.f / q.w;
				const float32 nx = q.x * iw, ny = q.y * iw;
				if (nx < mnx)
					mnx = nx;
				if (nx > mxx)
					mxx = nx;
				if (ny < mny)
					mny = ny;
				if (ny > mxy)
					mxy = ny;
			}

			const float32 b = p.edgeBias;
			if (!borne) {
				g.repliPleinEcran = true;
				mnx = -1.f - b;
				mxx = 1.f + b;
				mny = -1.f - b;
				mxy = 1.f + b;
			} else {
				// La marge de Johanson, puis le rognage à l'écran élargi : au-delà, on
				// mailleraiit ce que personne ne voit.
				mnx = NkMax(mnx - b, -1.f - b);
				mxx = NkMin(mxx + b, 1.f + b);
				mny = NkMax(mny - b, -1.f - b);
				mxy = NkMin(mxy + b, 1.f + b);
			}
			if (mxx <= mnx || mxy <= mny)
				return g; // étendue vide : rien de visible

			g.visible = true;
			g.pointsRetenus = n;
			g.ndcMinX = mnx;
			g.ndcMaxX = mxx;
			g.ndcMinY = mny;
			g.ndcMaxY = mxy;
			return g;
		}

		// Position d'un sommet (i, j) de la grille SUR LE PLAN DE REPOS. La houle
		// ajoute sa hauteur ensuite : c'est le consommateur qui compose, parce que
		// la grille ne connaît pas les vagues et n'a aucune raison de les connaître.
		// Rend faux quand le rayon de ce sommet ne rencontre pas le plan (il regarde
		// au-dessus de l'horizon) — un cas RÉEL au bord supérieur de l'écran, et qui
		// se dit au lieu de rendre un point à mille kilomètres.
		NK_FORCE_INLINE bool NkProjectedGridVertex(const NkProjectedGrid &g, const NkProjectedGridParams &p,
												   uint32 i, uint32 j, NkVec3f &out) noexcept {
			if (!g.visible || p.cols == 0u || p.rows == 0u)
				return false;
			const float32 u = (float32)i / (float32)p.cols;
			const float32 v = (float32)j / (float32)p.rows;
			const float32 nx = g.ndcMinX + (g.ndcMaxX - g.ndcMinX) * u;
			const float32 ny = g.ndcMinY + (g.ndcMaxY - g.ndcMinY) * v;

			// Le rayon de ce pixel : deux déprojections, plan proche et plan lointain.
			NkVec3f a, bb;
			if (!NkUnprojectNDC(g.invViewProj, nx, ny, -1.f, a))
				return false;
			if (!NkUnprojectNDC(g.invViewProj, nx, ny, 1.f, bb))
				return false;
			const NkVec3f d = bb - a;
			if (NkFabs(d.y) < 1e-9f)
				return false; // rayon parallèle au plan
			const float32 t = (p.baseY - a.y) / d.y;
			if (t < 0.f)
				return false; // le plan est DERRIÈRE ce rayon : au-dessus de l'horizon
			out = a + d * t;
			out.y = p.baseY;
			return true;
		}

	} // namespace math
} // namespace nkentseu
