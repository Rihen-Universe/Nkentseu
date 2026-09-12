#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Noge/ECS/Components/Rendering/NkWaterComponent.h — une surface d'eau posée sur
// une entité (2026-09-12).
//
// Ce composant ne porte AUCUNE logique d'eau : il transporte les paramètres que
// le producteur (NKVFX/NkWaterMeshBuilder) consomme, et les chiffres que
// `NkWaterSystem` y écrit après coup pour qu'on puisse lire ce qui s'est passé.
// Les mêmes fichiers d'effet servent les trois hôtes ; ce composant n'est que la
// représentation ECS de Noge.
//
// L'entité doit AUSSI porter `NkMeshComponent` (la poignée que NkRenderSystem
// dessine) et `NkMaterialComponent`, comme n'importe quel maillage : l'eau n'a
// pas de chemin de rendu à part.
// =============================================================================
#include "NKECS/NkECSDefines.h"
#include "NKECS/Core/NkTypeRegistry.h" // NK_COMPONENT — pas transitif
#include "NKMath/NkProjectedGrid.h"
#include "NKMath/NkWaterSurface.h"

namespace nkentseu {
	namespace ecs {

		struct NkWaterComponent {
				// ── Écrit par l'auteur de la scène ──────────────────────────────────
				// `grid.baseY` est IGNORÉ : le plan de repos est la position monde de
				// l'entité (y). Le reste (cols, rows, displacementMax, caméra de portée,
				// retrait) est honoré tel quel.
				math::NkProjectedGridParams grid;
				math::NkWaterParams waves;
				uint32 color = 0xFFFFFFFFu; // RGBA8, constant : l'eau n'a pas de couleur par sommet
				bool enabled = true;

				// ── Écrit par NkWaterSystem, jamais par l'auteur ─────────────────────
				nk_uint64 meshHandle = 0;	 // le maillage DYNAMIQUE créé pour cette entité
				uint32 createdCols = 0;		 // taille de grille du maillage créé : si elle
				uint32 createdRows = 0;		 // change, le système le recrée
				uint32 lastVertexCount = 0;	 // sommets produits à la dernière image (0 = rien)
				uint32 lastMissing = 0;		 // sommets refusés par la grille à la dernière image
				float32 time = 0.f;			 // temps de houle accumulé par le système
		};
		NK_COMPONENT(NkWaterComponent)

	} // namespace ecs
} // namespace nkentseu
