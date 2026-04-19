#pragma once
// =============================================================================
// Unkeny/Panels/SceneTreePanel.h
// =============================================================================
// Panel qui affiche la hiérarchie ECS de la scène active.
// Utilise NkUI::TreeNode pour afficher les entités parent/enfant.
// Supporte sélection, rename, drag&drop, activation/désactivation.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKUI/NKUI.h"
#include "NKUI/NkUIWidgets.h"
#include "NKECS/World/NkWorld.h"
#include "NKECS/Scene/NkSceneGraph.h"
#include "NKECS/Components/Core/NkCoreComponents.h"
#include "../Editor/NkSelectionManager.h"
#include "../Editor/CommandHistory.h"

namespace nkentseu {
    namespace Unkeny {

        class SceneTreePanel {
        public:
            SceneTreePanel() = default;

            // ── Rendu ─────────────────────────────────────────────────────────
            // Dessine le panel dans la fenêtre NKUI fournie.
            // ctx/wm/dl/font/ls : contexte NKUI actif.
            // world : le monde ECS courant.
            // sel   : gestionnaire de sélection (modifié si l'utilisateur clique).
            // hist  : historique pour les renommages.
            void Render(nkui::NkUIContext& ctx,
                        nkui::NkUIWindowManager& wm,
                        nkui::NkUIDrawList& dl,
                        nkui::NkUIFont& font,
                        nkui::NkUILayoutStack& ls,
                        ecs::NkWorld& world,
                        ecs::NkSceneGraph* scene,
                        NkSelectionManager& sel,
                        CommandHistory* hist,
                        nkui::NkUIRect rect) noexcept;

        private:
            // Rendu récursif d'une entité et de ses enfants
            void RenderEntity(nkui::NkUIContext& ctx,
                              nkui::NkUIDrawList& dl,
                              nkui::NkUIFont& font,
                              nkui::NkUILayoutStack& ls,
                              ecs::NkWorld& world,
                              ecs::NkEntityId id,
                              NkSelectionManager& sel,
                              CommandHistory* hist,
                              nk_uint32 depth) noexcept;

            void RenderContextMenu(nkui::NkUIContext& ctx,
                                   nkui::NkUIDrawList& dl,
                                   nkui::NkUIFont& font,
                                   ecs::NkWorld& world,
                                   ecs::NkEntityId id,
                                   ecs::NkSceneGraph* scene,
                                   NkSelectionManager& sel) noexcept;

            // État interne
            ecs::NkEntityId  mContextMenuEntity = ecs::NkEntityId::Invalid();
            ecs::NkEntityId  mRenamingEntity    = ecs::NkEntityId::Invalid();
            char             mRenameBuffer[128] = {};
            bool             mScrollToSelected  = false;

            // Entités ouvertes (TreeNode expanded)
            static constexpr nk_uint32 kMaxOpen = 256;
            ecs::NkEntityId  mOpenNodes[kMaxOpen] = {};
            nk_uint32        mOpenCount = 0;

            bool IsOpen(ecs::NkEntityId id) const noexcept;
            void SetOpen(ecs::NkEntityId id, bool open) noexcept;
        };

    } // namespace Unkeny
} // namespace nkentseu
