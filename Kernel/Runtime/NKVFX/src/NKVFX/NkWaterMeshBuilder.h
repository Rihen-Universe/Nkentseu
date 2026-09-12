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
				// RGBA8 empaqueté, CONSTANT : l'eau ne porte pas de couleur par sommet.
				// Le format `Default3D` en réclame une, on la remplit sans prétendre
				// qu'elle signifie quelque chose.
				uint32 color = 0xFFFFFFFFu;
		};

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
		uint32 NkWaterBuildVertices(const math::NkMat4f &proj, const math::NkMat4f &view,
									const math::NkVec3f &eye, const NkWaterMeshParams &p,
									renderer::NkVertex3D *out, uint32 capacity,
									uint32 *missing = nullptr) noexcept;

	} // namespace vfx
} // namespace nkentseu
