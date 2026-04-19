#pragma once
// =============================================================================
// Unkeny/Panels/InspectorPanel.h
// =============================================================================
// Affiche et édite les composants de l'entité sélectionnée.
// Utilise NkComponentMetaRegistry (NkReflectComponents.h) pour générer
// automatiquement les widgets selon le type de chaque champ.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKUI/NKUI.h"
#include "NKUI/NkUIWidgets.h"
#include "NKECS/World/NkWorld.h"
#include "NKECS/Systems/NkReflectComponents.h"
#include "../Editor/NkSelectionManager.h"
#include "../Editor/CommandHistory.h"

namespace nkentseu {
    namespace Unkeny {

        class InspectorPanel {
        public:
            InspectorPanel() = default;

            void Render(nkui::NkUIContext& ctx,
                        nkui::NkUIWindowManager& wm,
                        nkui::NkUIDrawList& dl,
                        nkui::NkUIFont& font,
                        nkui::NkUILayoutStack& ls,
                        ecs::NkWorld& world,
                        const NkSelectionManager& sel,
                        CommandHistory* hist,
                        nkui::NkUIRect rect) noexcept;

        private:
            // Rendu d'un composant complet (section pliable)
            void RenderComponent(nkui::NkUIContext& ctx,
                                 nkui::NkUIDrawList& dl,
                                 nkui::NkUIFont& font,
                                 nkui::NkUILayoutStack& ls,
                                 const ecs::NkComponentMeta& meta,
                                 void* compPtr,
                                 ecs::NkEntityId id,
                                 CommandHistory* hist) noexcept;

            // Rendu d'un champ individuel selon son type
            bool RenderField(nkui::NkUIContext& ctx,
                             nkui::NkUIDrawList& dl,
                             nkui::NkUIFont& font,
                             nkui::NkUILayoutStack& ls,
                             const ecs::NkFieldMeta& field,
                             void* compPtr) noexcept;

            // Bouton "Add Component"
            void RenderAddComponentMenu(nkui::NkUIContext& ctx,
                                        nkui::NkUIDrawList& dl,
                                        nkui::NkUIFont& font,
                                        nkui::NkUILayoutStack& ls,
                                        ecs::NkWorld& world,
                                        ecs::NkEntityId id) noexcept;

            // Sections ouvertes (per composant par nom)
            static constexpr nk_uint32 kMaxSections = 32;
            char     mSectionNames[kMaxSections][64] = {};
            bool     mSectionOpen[kMaxSections]      = {};
            nk_uint32 mSectionCount                  = 0;

            bool IsSectionOpen(const char* name) noexcept;
            void SetSectionOpen(const char* name, bool open) noexcept;
        };

    } // namespace Unkeny
} // namespace nkentseu
