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
// 3. On dérive une CAMÉRA DE PORTÉE : la caméra de rendu, remontée au-dessus de
//    la tranche et contrainte à regarder le plan. Quand le rendu était déjà
//    au-dessus et regardait le plan, elle lui est IDENTIQUE.
// 4. On rabat les points sur le plan de repos et on cherche leur étendue dans
//    l'espace projectif de cette caméra DE PORTÉE — plus dans celui du rendu.
// 5. La grille [0,1]² est étalée sur cette étendue, puis chaque sommet est
//    redéprojeté vers le plan de repos DEPUIS LA CAMÉRA DE PORTÉE. La hauteur
//    vient ensuite de la houle.
//
// ── LE DOMAINE EST TOMBÉ (2026-09-12), ET VOICI CE QU'IL A COÛTÉ ────────────
// La version du 07/09 EXIGEAIT que l'œil soit au-dessus de la tranche
// (`eye.y > baseY + displacementMax`) et REFUSAIT en dehors. Ce n'était pas une
// précaution : œil à 1 m, crête à 2 m, sur 1 225 pixels où la surface déplacée
// se voit, la grille en couvrait ZÉRO. Le prix en plans d'océan était écrit
// franchement — pas de caméra au ras de l'eau, pas de caméra sous l'eau, pas de
// vague au-dessus de l'objectif.
//
// LA CAMÉRA DE PORTÉE EST ÉCRITE, et la raison de ne pas l'écrire ne tenait pas.
// Elle était : « son placement est un compromis VISUEL que Johanson règle à
// l'œil, et on ne règle pas à l'œil ce qu'on ne peut pas regarder. » Or un
// compromis visuel n'est pas un compromis non mesurable. Trois nombres le
// jugent sans jamais regarder, et ce sont eux qui ont conduit chaque décision
// de ce fichier :
//   COUVERTURE — la fraction des pixels voyant de l'eau que la grille atteint.
//     La MÊME mesure qui disait « zéro sur 1 225 » dit aujourd'hui 1,00 sur la
//     même pose. Elle marche dans les deux sens, c'est ce qui la rend valable.
//   GÂCHIS — la fraction des sommets qui ne peuvent rien éclairer, à aucune des
//     hauteurs permises par la houle.
//   STABILITÉ — le *swimming*, mesuré comme une DÉRIVÉE : un déplacement continu
//     est proportionnel à l'angle. Quart de rotation, quart de glissement.
//     Mesuré : rapport 3,99 et 4,00 pour 4,00 attendu.
//
// 🔴 CE QUI N'EST PAS ACQUIS, ET LE CHIFFRE EST LÀ : LE GÂCHIS AUX VUES RASANTES.
// Mesuré 0,17 depuis un pont (œil à 8 m) et 0,24 sous l'eau — mais 0,90 avec
// l'œil à 1 m visant l'horizon, contre un seuil de 0,50 annoncé AVANT la mesure.
// La couverture y est bien de 1,00, et c'est précisément le piège : elle est
// obtenue en maillant beaucoup trop large. Le témoin (x9) reste ROUGE là-dessus,
// volontairement — le seuil n'a pas bougé d'un centième.
//
// DEUX PISTES ONT ÉTÉ ÉLIMINÉES PAR LA MESURE, ET LA SECONDE ÉTAIT LA MIENNE.
// 1. LE BORD LOINTAIN DE L'ÉTENDUE : innocent. Borner au point d'eau le plus
//    lointain que le tronc de vue atteint a divisé l'empreinte par 1,25 MILLION
//    (1,1·10¹³ → 8,9·10⁶ m²) sans presque rien changer au gâchis (0,92 → 0,90).
// 2. LE PLACEMENT DE LA CAMÉRA DE PORTÉE : innocent lui aussi, et j'avais écrit
//    le contraire ici même. La nappe (x12) balaie les DEUX degrés de liberté
//    (42 cellules) sur la pose rasante, et elle dit deux choses :
//      — la COUVERTURE vaut 1,00 dans les 42 cellules. Le compromis de Johanson
//        (« trop basse, elle perd la couverture ») N'EXISTE PAS ici : l'étendue
//        n'étant plus rognée sur l'écran de portée, tout point rabattu est déjà
//        sous son horizon. Les deux axes ne sont pas en tension ;
//      — le GÂCHIS a un PLANCHER qui sature à 0,58, jamais 0,50.
//
// 3. LA FORME DE L'ÉTENDUE : innocente elle aussi — TROISIÈME élimination, et
//    c'est encore une explication écrite ICI qui tombe. J'avais nommé le rectangle
//    circonscrit au trapèze comme « le levier du prochain lot ». Pour le vérifier
//    avant de l'écrire, la grille garde désormais le nuage dont l'étendue est
//    tirée (`ndcX/ndcY`), ce qui permet de comparer la BOÎTE à la FORME réelle —
//    son enveloppe convexe. Deux mesures en sortent :
//      — le vide de la boîte PRÉDIT le gâchis quand l'œil est hors de la tranche :
//        0,14 prédit contre 0,17 mesuré depuis un pont, 0,17 contre 0,24 sous
//        l'eau. L'instrument est bon et le raisonnement tient ;
//      — il ne le prédit plus DU TOUT en vue rasante : 0,37 contre 0,90, écart
//        0,52. La boîte n'y enferme que 37 % de vide, et non 90 %.
//    Et le nombre qui tranche, MESURÉ et non extrapolé — ce que laisserait un
//    pavage PARFAIT de la forme, compté sur les sommets qui y tombent déjà :
//        pont 0,17 → 0,00      sous l'eau 0,24 → 0,03      rasante 0,90 → 0,83
//    Épouser la forme est donc spectaculaire partout SAUF là où l'objectif est
//    fixé. C'est un vrai gain, mais ce n'est PAS la réponse au gâchis rasant, et
//    il ne sera pas écrit sur la foi d'un espoir.
//
// ET LE DÉFAUT (0,05 ; 0,50 m) RESTE, PAR MESURE ET NON PAR INERTIE. Le tableau
// (x13) croise quatre réglages et trois poses : tout ce qui soulage la vue
// rasante alourdit la vue de pont ET l'uniformité du pas écran. Descendre au
// plancher de 0,58 ferait passer le pont de 0,17 à 0,49 et le pas écran de
// ×1,00 à ×5,57 — c'est-à-dire détruire le 1,00 contre 251,00 qui justifie toute
// la technique. On échangerait un témoin rouge contre un pire.
//
// CE QUI RESTE, ET IL EST CERNÉ SANS ÊTRE RÉSOLU. Balayé sur sept hauteurs d'œil
// (3,00 m à 0,50 m, même visée), le gâchis vaut :
//     3,00→0,20   2,50→0,22   2,10→0,35   1,90→0,49   1,50→0,82   1,00→0,90
//     0,50→0,82
// Trois choses s'y lisent, et aucune n'était celle que j'avais supposée :
//  — la rampe COMMENCE AVANT la tranche (0,22 puis 0,35, l'œil étant encore
//    au-dessus) : le sommet de tranche ne déclenche donc pas le gâchis ;
//  — la courbe n'est pas monotone, elle PASSE PAR UN MAXIMUM à 1,00 m puis
//    redescend — ce qui a piégé un de mes propres témoins, qui comparait deux
//    hauteurs symétriques autour de ce sommet et lisait donc zéro ;
//  — mais ce sommet de tranche SÉPARE NETTEMENT le gain du pavage : épouser la
//    forme donne exactement 0,00 aux trois hauteurs au-dessus, et jamais moins de
//    0,15 en dessous.
// L'étape qui sur-estime est le RABATTEMENT des points de la tranche sur le plan
// de repos : plus la vue est rasante, plus l'empreinte au sol de ce qu'on écrase
// déborde l'eau réellement visible. C'est l'étape 4, et c'est là qu'il faudra
// chercher — le rectangle, le bord lointain et le placement ayant été éliminés
// par la mesure, chacun à son tour.
//
// Autrement dit : la caméra de portée est POSÉE et prouvée sur la couverture et
// la stabilité ; le gâchis en vue rasante n'est ni un réglage à trouver, ni une
// forme à changer — les trois ont été éliminés par la mesure. C'est un lot, pas
// un doute, et il porte maintenant sur l'étape 4 elle-même.
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
				// ⚠️ Avec la caméra de portée cette marge est RELATIVE à l'étendue
				// retenue, et non plus absolue en NDC : l'étendue n'est plus bornée par
				// l'écran, donc « 0,05 de NDC » n'y désigne plus une fraction connue.
				float32 edgeBias = 0.05f;

				// ── LA CAMÉRA DE PORTÉE (Johanson §2.4-2.5) ─────────────────────────
				// Faux = chemin d'origine : les sommets sortent de la caméra de rendu,
				// et la grille REFUSE hors du domaine `eye.y > baseY + displacementMax`.
				// Vrai = les sommets sortent d'une caméra DÉRIVÉE, contrainte à regarder
				// le plan, et il n'y a plus de domaine à respecter.
				// Gardé commutable pour que le défaut d'origine reste REPRODUCTIBLE :
				// une correction dont on ne peut plus rejouer le défaut n'est plus
				// prouvable six mois plus tard.
				bool rangeCamera = true;
				// De combien la caméra de portée est remontée AU-DESSUS du sommet de la
				// tranche (m). C'est le seul « réglage » de Johanson ; il ne se règle pas
				// à l'œil ici : son effet est mesuré (couverture, gâchis, stabilité).
				float32 rangeElevation = 0.5f;
				// Sinus de l'inclinaison MINIMALE de la visée sous l'horizontale. Sans
				// ce plancher, une caméra qui vise l'horizon donne un point visé à
				// l'infini — et une caméra de portée qui regarde l'infini ne regarde
				// plus le plan.
				float32 rangePitchMin = 0.05f;
		};

		struct NkProjectedGrid {
				// Faux = la tranche d'eau ne coupe pas le tronc de vue : il n'y a rien à
				// mailler, et ça se DIT (au lieu de rendre une grille vide plausible).
				bool visible = false;
				// Vrai = l'œil n'est PAS au-dessus de la tranche, ET la caméra de portée
				// est désactivée : hors du domaine du chemin d'origine. `visible` reste
				// faux. Avec `rangeCamera`, ce drapeau ne se lève JAMAIS — c'est
				// exactement ce que la caméra de portée achète.
				bool horsDomaine = false;
				// Vrai = la caméra de portée a réellement été posée pour cette image.
				bool porteePosee = false;
				// Vrai = elle a dû être REMONTÉE au-dessus de la tranche, c'est-à-dire
				// que l'œil de rendu, lui, n'était pas dans l'ancien domaine.
				bool porteeRemontee = false;
				// Vrai = l'étendue a été rognée pour rester SOUS l'horizon de la caméra
				// de portée. Au-delà, un sommet n'a pas d'intersection avec le plan : il
				// ne serait pas « imprécis », il n'existerait pas.
				bool rogneHorizon = false;
				// L'altitude d'où la caméra de portée a lancé ses rayons (m).
				float32 porteeY = 0.f;
				// Vrai = l'étendue projective n'était pas bornée (un point retenu était
				// derrière la caméra) et on a pris l'écran entier. Voir l'en-tête.
				bool repliPleinEcran = false;
				// L'étendue retenue, en NDC.
				float32 ndcMinX = -1.f, ndcMaxX = 1.f;
				float32 ndcMinY = -1.f, ndcMaxY = 1.f;
				// Combien de points ont servi à la construire (0 = invisible).
				uint32 pointsRetenus = 0u;
				// L'inverse de la view-projection DONT LES SOMMETS SORTENT, gardée pour
				// les déprojeter.
				NkMat4f invViewProj;
				// ── DANS QUEL ÉCRAN L'ÉTENDUE CI-DESSUS EST-ELLE ÉCRITE ? ───────────
				// `ndcMinX..ndcMaxY` ne sont pas des nombres absolus : ce sont des
				// coordonnées NDC, et un NDC n'existe que relativement à une caméra.
				// Tant que la grille se construisait depuis la caméra de rendu, la
				// question ne se posait pas et la réponse restait implicite — c'est
				// exactement le genre de chose qui devient faux sans prévenir le jour
				// où une deuxième caméra entre dans le calcul. On l'écrit donc :
				// voici la view-projection dans laquelle l'étendue se lit.
				NkMat4f rangeViewProj;
				// ── LE NUAGE DONT L'ÉTENDUE EST TIRÉE, en NDC de portée ─────────────
				// L'étendue ci-dessus est une BOÎTE, donc un MAJORANT de la forme réelle
				// de l'eau vue. Tant qu'on ne garde que la boîte, personne ne peut
				// mesurer ce qu'elle enferme de vide : ça n'est mesurable que si l'on
				// garde aussi ce qu'elle enferme. Ces points sont les positions rabattues
				// sur le plan de repos, projetées dans l'écran de portée — la forme que
				// le pavage devra suivre le jour où il suivra autre chose qu'un
				// rectangle. `ndcCount` vaut 0 quand l'étendue ne dérive d'aucun nuage
				// (repli plein écran), et ce zéro est une information, pas un oubli.
				float32 ndcX[40];
				float32 ndcY[40];
				uint32 ndcCount = 0u;
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

		// Le rayon du pixel (nx, ny) rencontre-t-il le plan y = py DEVANT la caméra
		// dont on donne l'inverse de la view-projection, ET à moins de `distMax` de
		// l'œil (distance horizontale) ?
		//
		// LES DEUX CONDITIONS BORNENT L'ÉTENDUE, et aucune n'est un réglage.
		// 1. RATER LE PLAN : au-dessus de l'horizon, le rayon ne rencontre rien —
		//    un sommet posé là n'est pas imprécis, il n'existe pas.
		// 2. TROP LOIN : près de l'horizon, la déprojection est quasi singulière et
		//    quelques centièmes de NDC valent des milliers de kilomètres au sol.
		//    Mesuré : sans cette borne, l'empreinte atteignait 1,1·10¹³ m² — onze
		//    millions de km² pour un océan qu'on regarde à 2 km — et 92 % des
		//    sommets tombaient hors champ. La borne n'est pas choisie à l'œil : on
		//    ne maille pas plus loin que le point d'eau le plus lointain que le
		//    tronc de vue du RENDU atteint réellement.
		NK_FORCE_INLINE bool NkSommetDansPortee(const NkMat4f &invVP, float32 nx, float32 ny,
												float32 py, const NkVec3f &eye,
												float32 distMax) noexcept {
			NkVec3f a, b;
			if (!NkUnprojectNDC(invVP, nx, ny, -1.f, a))
				return false;
			if (!NkUnprojectNDC(invVP, nx, ny, 1.f, b))
				return false;
			const NkVec3f d = b - a;
			if (NkFabs(d.y) < 1e-9f)
				return false;
			const float32 t = (py - a.y) / d.y;
			if (t < 0.f)
				return false;
			const NkVec3f m = a + d * t;
			const float32 dx = m.x - eye.x, dz = m.z - eye.z;
			return (dx * dx + dz * dz) <= distMax * distMax;
		}

		// Construit la grille pour une caméra de rendu donnée.
		// `eye` est demande EXPLICITEMENT plutot que deduit de la matrice : la
		// deduire couterait une inversion de plus et, surtout, l'appelant l'a deja.
		// Un parametre qu'on peut donner ne se devine pas.
		//
		// ⚠️ LA PROJECTION ET LA VUE SONT DEMANDÉES SÉPARÉMENT, et non leur produit.
		// Ce n'est pas de la cosmétique : la caméra de portée doit réutiliser la
		// PROJECTION du rendu (même ouverture, même rapport d'image, mêmes plans)
		// avec une AUTRE orientation. Un produit déjà fait ne se défait pas, et le
		// re-factoriser à coups d'inversions serait payer pour reconstituer ce que
		// l'appelant tient déjà dans la main.
		inline NkProjectedGrid NkProjectedGridBuild(const NkMat4f &proj, const NkMat4f &view,
													const NkVec3f &eye,
													const NkProjectedGridParams &p) noexcept {
			NkProjectedGrid g;
			const NkMat4f viewProj = proj * view;
			const NkMat4f invRender = viewProj.Inverse();
			// Par défaut — et c'est le cas définitif quand la caméra de portée est
			// désactivée — les sommets sortent de la caméra de RENDU et l'étendue se
			// lit dans son écran. La caméra de portée, si elle est posée, remplacera
			// les DEUX ensemble : elles ne se séparent pas.
			g.invViewProj = invRender;
			g.rangeViewProj = viewProj;

			// LE DOMAINE, ET IL N'EXISTE QUE SANS CAMERA DE PORTEE. Hors domaine le
			// chemin d'origine REFUSE : la mesure du 07/09 dit qu'on y couvrait 0
			// pixel sur 1 225, et une grille qui ne couvre rien tout en se declarant
			// visible est pire qu'une absence -- elle fait chercher le defaut
			// ailleurs. La camera de portee supprime la cause, donc la garde.
			if (!p.rangeCamera && eye.y <= p.baseY + p.displacementMax) {
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
				coinOk[k] = NkUnprojectNDC(invRender, nx, ny, nz, coins[k]);
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

			// JUSQU'OÙ LE RENDU VOIT-IL DE L'EAU ? Les points retenus sont exactement
			// l'intersection du tronc de vue et de la tranche : le plus lointain d'entre
			// eux borne ce qu'il y a à mailler. C'est une borne MESURÉE sur la vue en
			// cours, pas une constante posée à la main, et elle suit l'ouverture, le
			// plan lointain et la pose sans qu'on ait à la régler.
			float32 distMax = 0.f;
			for (uint32 i = 0; i < n; ++i) {
				const float32 dx = pts[i].x - eye.x, dz = pts[i].z - eye.z;
				const float32 d2 = dx * dx + dz * dz;
				if (d2 > distMax)
					distMax = d2;
			}
			distMax = NkSqrt(distMax) * (1.f + p.edgeBias);

			// 3. LA CAMÉRA DE PORTÉE (Johanson §2.4-2.5).
			//
			// Le tronc de vue et la tranche viennent d'être découpés avec la caméra de
			// RENDU : c'est bien elle qui décide de ce qu'on VOIT, et ça ne change pas.
			// Ce qui change est la caméra qui LANCE LES RAYONS. On en dérive une,
			// remontée au-dessus de la tranche et contrainte à regarder le plan : ainsi
			// chacun de ses rayons le rencontre, quelle que soit la pose du rendu — y
			// compris au ras de l'eau, sous l'eau, ou une vague par-dessus l'objectif.
			//
			// ⚠️ ELLE RÉUTILISE LA PROJECTION DU RENDU, pas seulement par économie :
			// quand l'œil de rendu est déjà au-dessus de la tranche et regarde le plan,
			// la construction ci-dessous redonne EXACTEMENT la caméra de rendu. La
			// caméra de portée est alors l'identité, et tout ce que le chemin d'origine
			// mesurait de bon reste vrai au chiffre près. Une correction qui ne se voit
			// pas là où rien n'était cassé est une correction qui ne compense rien.
			NkMat4f rangeVP = viewProj;
			NkMat4f invRange = invRender;
			if (p.rangeCamera) {
				// 3a. remontée au-dessus de la tranche. L'epsilon n'est pas une
				// coquetterie : à altitude nulle au-dessus du plan, le point visé est
				// la caméra elle-même et la visée n'a plus de direction.
				const float32 elev = NkMax(p.rangeElevation, 1e-3f);
				NkVec3f projPos = eye;
				const float32 yMin = p.baseY + p.displacementMax + elev;
				if (projPos.y < yMin) {
					projPos.y = yMin;
					g.porteeRemontee = true;
				}

				// 3b. la direction de visée du RENDU, lue sur son rayon central.
				NkVec3f dir{0.f, -1.f, 0.f};
				NkVec3f a0, b0;
				if (NkUnprojectNDC(invRender, 0.f, 0.f, -1.f, a0) &&
					NkUnprojectNDC(invRender, 0.f, 0.f, 1.f, b0)) {
					const NkVec3f f = (b0 - a0).Normalized();
					if (f.LenSq() > 0.5f)
						dir = f;
				}

				// 3c. LE PLANCHER D'INCLINAISON. Une visée horizontale (ou qui monte)
				// donne un point visé à l'infini : on la rabat sous l'horizontale. C'est
				// la seule entorse à « suivre le rendu », et elle est bornée, donc
				// mesurable — pas réglée à l'œil.
				const float32 pitch = NkMax(p.rangePitchMin, 1e-3f);
				if (!(dir.y < -pitch)) {
					dir.y = -pitch;
					dir = dir.Normalized();
					if (dir.LenSq() < 0.5f)
						dir = NkVec3f{0.f, -1.f, 0.f};
				}

				// 3d. le point visé, SUR le plan de repos.
				const float32 tAim = (p.baseY - projPos.y) / dir.y; // dir.y < 0, projPos.y > baseY
				NkVec3f aim = projPos + dir * tAim;
				aim.y = p.baseY;

				// Visée quasi verticale : `forward × up` s'annule et LookAt rendrait une
				// base dégénérée (Normalize() d'un vecteur nul rend le vecteur nul, en
				// silence). On change de référence plutôt que de produire une matrice
				// muette dont l'inverse partirait en NaN.
				NkVec3f up{0.f, 1.f, 0.f};
				if (NkFabs(dir.y) > 0.9999f)
					up = NkVec3f{0.f, 0.f, -1.f};

				rangeVP = proj * NkMat4f::LookAt(projPos, aim, up);
				invRange = rangeVP.Inverse();
				g.porteePosee = true;
				g.porteeY = projPos.y;
			}
			g.rangeViewProj = rangeVP;
			g.invViewProj = invRange; // LES SOMMETS SORTENT DE LA CAMÉRA DE PORTÉE

			// 4. rabattus sur le plan de repos, puis ramenés dans l'écran DE PORTÉE.
			float32 mnx = 1e30f, mxx = -1e30f, mny = 1e30f, mxy = -1e30f;
			bool borne = true;
			for (uint32 i = 0; i < n; ++i) {
				const NkVec4f q = rangeVP * NkVec4f(pts[i].x, p.baseY, pts[i].z, 1.f);
				if (q.w <= 1e-6f) {
					borne = false; // derrière la caméra : pas d'image projective
					break;
				}
				const float32 iw = 1.f / q.w;
				const float32 nx = q.x * iw, ny = q.y * iw;
				// On GARDE le nuage, et pas seulement ses bornes : c'est lui la forme
				// réelle de l'eau vue, dont la boîte n'est qu'un majorant.
				g.ndcX[i] = nx;
				g.ndcY[i] = ny;
				if (nx < mnx)
					mnx = nx;
				if (nx > mxx)
					mxx = nx;
				if (ny < mny)
					mny = ny;
				if (ny > mxy)
					mxy = ny;
			}
			g.ndcCount = borne ? n : 0u; // le repli plein écran ne dérive d'aucun nuage

			const float32 b = p.edgeBias;
			if (!borne) {
				g.repliPleinEcran = true;
				mnx = -1.f - b;
				mxx = 1.f + b;
				mny = -1.f - b;
				mxy = 1.f + b;
			} else if (p.rangeCamera) {
				// LA MARGE, RELATIVE À L'ÉTENDUE. L'écran de la caméra de portée n'est
				// l'écran de personne : « 0,05 de NDC » n'y désigne plus une fraction
				// connue de l'image. On élargit donc de 5 % de l'étendue retenue.
				const float32 mxyBrut = mxy;
				const float32 ex = (mxx - mnx) * b, ey = (mxy - mny) * b;
				mnx -= ex;
				mxx += ex;
				mny -= ey;
				mxy += ey;

				// ⚠️ ET AUCUN ROGNAGE SUR L'ÉCRAN DE LA CAMÉRA DE PORTÉE. Le chemin
				// d'origine rognait sur [-1, 1] parce que son écran ÉTAIT celui du
				// rendu : au-delà, on maillait ce que personne ne voit. Ici l'écran de
				// portée est un intermédiaire de calcul — y rogner couperait de l'eau
				// que le rendu voit VRAIMENT. L'étendue est déjà serrée sur les points
				// du tronc de rendu : c'est eux, et non un cadre, qui la bornent.
				//
				// LA SEULE BORNE QUI GARDE UN SENS EST L'HORIZON de la caméra de portée,
				// et seulement parce que la marge vient peut-être de la franchir : la
				// recherche part de l'étendue AVANT marge, qui est sûre par construction
				// (elle ne contient que des points réels, donc atteints).
				// ⚠️ LA RECHERCHE PART DE L'ÉTENDUE AVANT MARGE — mais elle ne la suppose
				// plus sûre. Elle l'est pour l'horizon (elle ne contient que des points
				// réels, donc atteints) ; elle ne l'est PAS pour la distance, puisque le
				// haut de la boîte englobante peut déjà viser bien au-delà du point le
				// plus lointain qui l'a produite. On descend donc depuis le bas, qui est
				// sûr des deux côtés.
				if (!NkSommetDansPortee(invRange, mnx, mxy, p.baseY, eye, distMax) ||
					!NkSommetDansPortee(invRange, mxx, mxy, p.baseY, eye, distMax)) {
					float32 lo = mny, hi = mxy;
					if (NkSommetDansPortee(invRange, mnx, mxyBrut, p.baseY, eye, distMax) &&
						NkSommetDansPortee(invRange, mxx, mxyBrut, p.baseY, eye, distMax))
						lo = mxyBrut;
					for (uint32 it = 0; it < 40u; ++it) {
						const float32 mid = (lo + hi) * 0.5f;
						if (NkSommetDansPortee(invRange, mnx, mid, p.baseY, eye, distMax) &&
							NkSommetDansPortee(invRange, mxx, mid, p.baseY, eye, distMax))
							lo = mid;
						else
							hi = mid;
					}
					mxy = lo;
					g.rogneHorizon = true;
				}
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
