#pragma once
// =============================================================================
// Nogee/Panels/Model/NkSceneTreeModel.h — MODELE NEUTRE de l'arbre de scene
// =============================================================================
// Aucune bibliotheque d'interface incluse (ni NKUI, ni NKGui, ni NKEditorKit).
// Cf. NkConsoleModel.h pour la raison d'etre de ce dossier.
//
// Contenu : l'etat qu'un arbre de scene garde entre deux images — quels noeuds
// sont deplies, quelle entite est en cours de renommage, laquelle a ouvert le
// menu contextuel. Rien de tout cela ne depend de la facon dont on le dessine.
//
// Le panneau HERITE de ce modele plutot que de le contenir : les 22 references
// existantes du .cpp continuent de compiler sans modification, ce qui rend
// l'extraction sans risque pour le chemin NKUI vivant.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKECS/NkECSDefines.h" // ecs::NkEntityId

namespace nkentseu {
	namespace noge {

		struct NkSceneTreeModel {
				// Entites dépliées (TreeNode expanded)
				static constexpr nk_uint32 kMaxOpen = 256;

				ecs::NkEntityId mContextMenuEntity = ecs::NkEntityId::Invalid();
				ecs::NkEntityId mRenamingEntity = ecs::NkEntityId::Invalid();
				char mRenameBuffer[128] = {};
				bool mScrollToSelected = false;

				ecs::NkEntityId mOpenNodes[kMaxOpen] = {};
				nk_uint32 mOpenCount = 0;

				bool IsOpen(ecs::NkEntityId id) const noexcept {
					for (nk_uint32 i = 0; i < mOpenCount; ++i)
						if (mOpenNodes[i] == id)
							return true;
					return false;
				}

				void SetOpen(ecs::NkEntityId id, bool open) noexcept {
					if (open) {
						if (!IsOpen(id) && mOpenCount < kMaxOpen)
							mOpenNodes[mOpenCount++] = id;
					} else {
						for (nk_uint32 i = 0; i < mOpenCount; ++i) {
							if (mOpenNodes[i] == id) {
								mOpenNodes[i] = mOpenNodes[--mOpenCount];
								return;
							}
						}
					}
				}
		};

	} // namespace noge
} // namespace nkentseu
