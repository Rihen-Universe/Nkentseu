#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKVFX/NkWaterMeshBuilder.h — LE PRODUCTEUR D'EAU : d'une caméra à des sommets
// et des indices, et rien d'autre (2026-09-12).
//
// ── CE QU'IL NE SAIT PAS, ET C'EST LA CONDITION DU PARTAGE ──────────────────
// Il ne connaît AUCUN de ses hôtes. Ni ECS, ni Noge, ni application, ni scène.
// Il ne possède aucun tampon, ne crée aucun maillage, n'émet aucun appel de
// dessin, ne touche aucun périphérique. On lui donne une caméra, des paramètres
// et de la mémoire ; il rend des SOMMETS et des INDICES.
//
// C'est ce qui permet aux trois hôtes de partager les mêmes fichiers d'effet en
// les représentant différemment — Noge par son ECS, NkAnimaEditor en direct,
// NKScena le jour où elle existera. Un producteur qui connaîtrait son
// consommateur en aurait bientôt deux, et servirait mal les deux.
//
// ── ⚠️ L'IDIOME QUE L'ADAPTATEUR ECS DEVRA SUIVRE, ET POURQUOI ──────────────
// Noge porte DEUX idiomes d'adaptateur, et la divergence est délibérée :
//   * `NkParticleSystem` prend un `renderer::NkRenderer*` — aucun banc ne peut
//     donc l'exercer sans device, et personne ne l'a jamais fait tourner sans
//     fenêtre ;
//   * `NkFluidVolumeSystem`, plus récent, prend le REGISTRE et non le renderer ;
//     son en-tête dit que ce n'est « pas un détail de style » : en prenant la
//     dépendance la plus ÉTROITE, le pont s'éprouve headless.
// LE TENANT DE L'EAU SUIVRA LE SECOND : il prendra un `NkMeshSystem*` — la chose
// la plus étroite dont il ait besoin — et NON le renderer entier. C'est écrit ICI
// pour que la fusion ne fabrique pas un TROISIÈME idiome faute d'avoir su lequel
// suivre.
//
// Ce fichier-ci, lui, ne prend RIEN : c'est ce qui le rend mesurable sans
// fenêtre, comme la géométrie qu'il assemble.
//
// ── LE REFUS, PLUTÔT QU'UN MAILLAGE À TROUS ─────────────────────────────────
// `NkProjectedGridVertex` REFUSE les sommets dont le rayon rate le plan — un cas
// réel au bord supérieur de l'écran. Or le pavage suppose une grille PLEINE de
// (cols+1)(rows+1) sommets : s'il en manque un, la topologie que le témoin
// vérifie — chaque arête intérieure partagée par exactement deux triangles —
// devient fausse. Poser un sommet de remplacement fabriquerait des triangles
// d'aire nulle EN SILENCE. On refuse donc en bloc, et on DIT combien manquent.
// =============================================================================
#include "NKMath/NkProjectedGrid.h"
#include "NKMath/NkWaterSurface.h"
#include "NKRenderer/Core/NkRendererTypes.h"

namespace nkentseu {
	namespace vfx {

		struct NkWaterMeshParams {
				math::NkProjectedGridParams grid;
				math::NkWaterParams waves;
				float32 time = 0.f;
				// RGBA8 empaqueté, CONSTANT : la couleur donnée à chaque sommet quand
				// `shade` est faux. Le format `Default3D` en réclame une, on la remplit
				// sans prétendre qu'elle signifie quelque chose.
				uint32 color = 0xFFFFFFFFu;

				// ── LA COULEUR PAR SOMMET (2026-09-13) ──────────────────────────────
				// `NkWaterShade` et `NkWaterFoam` vivent dans NKMath depuis le 06/09
				// SANS UN SEUL APPELANT. Les appeler ICI, par sommet, est le seul
				// endroit où la chaîne existe déjà : le producteur tient la hauteur, le
				// jacobien et le plan de repos, et le format de sommet porte déjà une
				// couleur. Aucun nuanceur neuf, aucune texture, aucun Gerstner GPU.
				//
				// ⚠️ DÉFAUT À FAUX, ET C'EST LE SENS D'ERREUR CHOISI. Un appelant écrit
				// avant ce jour — les témoins (p1)/(p2), la sonde ECS, `NkWaterSystem` —
				// reçoit EXACTEMENT le `color` constant d'avant, AU BIT. Un paramètre
				// ajouté sans défaut neutre changerait en silence ce que mesurent des
				// témoins écrits pour autre chose : c'est la faute que le champ
				// `wetness` de `NkDrawCall3D` avait déjà évitée de la même façon.
				bool shade = false;
				math::NkWaterOptics optics;

				// 🔴 CE QUE LE PRODUCTEUR N'A PAS, ET QU'ON NE FABRIQUE PAS EN DOUCE.
				// `NkWaterShade` veut une PROFONDEUR, donc un TERRAIN — et il n'y a
				// aucun terrain ici, ni raison d'en avoir un : `NkWaterSurface.h` dit que
				// le fond est FOURNI (carte de hauteur, `Heightfield` de NKCollision, ou
				// fonction). On pose donc un FOND PLAT, et voici ce que ça rend faux :
				//   * la profondeur ne varie qu'avec la HOULE, jamais avec le relief ;
				//   * il n'y a donc ni haut-fond, ni plage — et la PREMIÈRE des trois
				//     sources de `NkWaterFoam` (l'écume de rivage) ne se lèvera jamais
				//     tant qu'un vrai fond n'est pas branché ;
				//   * le cambrement des vagues à l'approche du rivage — la question
				//     d'origine de Rodolf le 05/09 — reste hors d'atteinte pour la même
				//     raison, et `depthOverride` de `NkWaterEval` reste inutilisé.
				// 3 m n'est pas choisi à l'œil : c'est la profondeur que l'en-tête de
				// `NkWaterSurface.h` prend lui-même en exemple (« à 3 m il reste 36 % du
				// rouge et 98,7 % du bleu »), donc celle où l'effet est déjà chiffré.
				float32 bottomDepth = 3.f;
				// L'albédo du fond, même statut : le producteur ne peut pas le connaître.
				// Sable clair, parce qu'un fond sombre rendrait Beer-Lambert
				// indistinguable de `deepColor` — on ne verrait plus ce qu'on mesure.
				math::NkVec3f bottomColor = {0.45f, 0.40f, 0.30f};
				// La couleur vers laquelle l'écume BLANCHIT. `NkWaterFoam` rend une
				// valeur 0..1 et RIEN d'autre ; en faire une couleur est le travail du
				// consommateur, et c'est exactement ce qu'un nuanceur ferait.
				math::NkVec3f foamColor = {0.92f, 0.95f, 0.97f};
		};

		// Empaquetage de la couleur de sommet pour `NkVertexLayout::Default3D`, dont
		// l'attribut COLOR est `NK_U8x4_NORM` lu sur un `uint32` : sur machine petit-
		// boutienne, l'octet de poids FAIBLE est le ROUGE.
		// ⚠️ Ce n'est pas 0xRRGGBBAA. La forme ci-dessous est celle de
		// `NkMeshSystem.cpp:356` (`PackRGBA`), qui est la source de vérité de ce
		// format ; l'écrire à l'envers donnerait du bleu pour du rouge, en silence.
		NK_FORCE_INLINE uint32 NkWaterPackColor(const math::NkVec3f &c, float32 a = 1.f) noexcept {
			const float32 r = math::NkClamp(c.x, 0.f, 1.f);
			const float32 g = math::NkClamp(c.y, 0.f, 1.f);
			const float32 b = math::NkClamp(c.z, 0.f, 1.f);
			const float32 al = math::NkClamp(a, 0.f, 1.f);
			return ((uint32)(al * 255.f + 0.5f) << 24) | ((uint32)(b * 255.f + 0.5f) << 16) |
				   ((uint32)(g * 255.f + 0.5f) << 8) | (uint32)(r * 255.f + 0.5f);
		}

		// La couleur d'un point de surface : Beer-Lambert sur le fond, PUIS l'écume
		// qui blanchit. Exposée plutôt qu'enfouie dans la boucle pour qu'un
		// instrument puisse refaire le calcul et le comparer à ce qui est PEINT —
		// une sonde qui recopierait la composition finirait par en mesurer une autre.
		NK_FORCE_INLINE math::NkVec3f NkWaterSurfaceColor(const NkWaterMeshParams &p, float32 waveY,
														  float32 jacobianXZ,
														  float32 *foamOut = nullptr) noexcept {
			// `NkWaterEval` rend une hauteur AUTOUR DE ZÉRO ; la surface absolue est
			// `baseY + waveY`, et le fond plat est `baseY - bottomDepth`. La
			// soustraction passe par `NkWaterDepth` et non par une expression écrite
			// ici : une seconde copie divergerait un jour.
			const float32 depth =
				math::NkWaterDepth(p.grid.baseY + waveY, p.grid.baseY - p.bottomDepth);
			const math::NkVec3f eau = math::NkWaterShade(p.optics, p.bottomColor, depth);
			// `NkWaterFoam` prend la hauteur AUTOUR DE ZÉRO : `crestHeight` est une
			// hauteur de crête au-dessus du repos, pas une altitude absolue.
			const float32 f = math::NkWaterFoam(p.optics, depth, waveY, jacobianXZ);
			if (foamOut != nullptr)
				*foamOut = f;
			return eau + (p.foamColor - eau) * f;
		}

		// Combien de sommets et d'indices seront écrits. Calculés depuis la grille
		// SEULE : un appelant dimensionne ses tampons AVANT d'appeler, sans device
		// et sans caméra.
		NK_FORCE_INLINE uint32 NkWaterVertexCount(const math::NkProjectedGridParams &g) noexcept {
			return (g.cols + 1u) * (g.rows + 1u);
		}

		NK_FORCE_INLINE uint32 NkWaterIndexCount(const math::NkProjectedGridParams &g) noexcept {
			return math::NkProjectedGridIndexCount(g.cols, g.rows);
		}

		// Le pavage : simple délégation. La combinatoire vit dans NKMath et n'a
		// aucune raison d'être recopiée ici — une seconde copie divergerait un jour.
		NK_FORCE_INLINE uint32 NkWaterBuildIndices(const math::NkProjectedGridParams &g, uint32 *out,
												   uint32 capacity) noexcept {
			return math::NkProjectedGridIndices(g.cols, g.rows, out, capacity);
		}

		// Remplit les sommets DÉPLACÉS par la houle. Rend le nombre écrit, et ZÉRO
		// en cas de refus : capacité insuffisante, grille invisible, ou au moins un
		// sommet dont le rayon rate le plan.
		//
		// `missing`, s'il est fourni, reçoit le nombre de sommets manquants. C'est
		// ce qui distingue « rien à dessiner » de « la grille a des trous » — deux
		// causes qu'un zéro tout seul confondrait.
		//
		// `jacobianOut`, s'il est fourni, reçoit le JACOBIEN HORIZONTAL de chaque
		// sommet (même capacité que `out`). Il est exposé pour la raison qui a fait
		// exposer `ndcEgare` dans la grille : un instrument doit pouvoir refaire le
		// classement de son côté. La hauteur se relit sur le sommet, le jacobien NON
		// — sans lui, une sonde qui veut juger l'écume devrait RECONSTRUIRE la houle,
		// c'est-à-dire mesurer une deuxième implémentation au lieu de celle-ci.
		uint32 NkWaterBuildVertices(const math::NkMat4f &proj, const math::NkMat4f &view,
									const math::NkVec3f &eye, const NkWaterMeshParams &p,
									renderer::NkVertex3D *out, uint32 capacity,
									uint32 *missing = nullptr,
									float32 *jacobianOut = nullptr) noexcept;

	} // namespace vfx
} // namespace nkentseu
