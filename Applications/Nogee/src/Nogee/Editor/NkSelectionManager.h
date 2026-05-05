#pragma once
// =============================================================================
// Unkeny/Editor/NkSelectionManager.h
// =============================================================================
// Gère l'entité sélectionnée dans l'éditeur.
// Notifie les panels via EventBus quand la sélection change.
// Support multi-sélection (Ctrl+clic).
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKECS/NkECSDefines.h"
#include "Nkentseu/Core/EventBus.h"

namespace nkentseu {
    namespace Unkeny {

        // Événement émis sur l'EventBus quand la sélection change
        struct NkSelectionChangedEvent {
            ecs::NkEntityId primary;   // entité principale (dernière sélectionnée)
            nk_uint32       count;     // nombre d'entités sélectionnées
        };

        class NkSelectionManager {
        public:
            NkSelectionManager() = default;

            // ── Sélection ─────────────────────────────────────────────────────
            void Select(ecs::NkEntityId id) noexcept;          // sélection simple
            void SelectAdd(ecs::NkEntityId id) noexcept;       // ajoute à la sélection
            void SelectToggle(ecs::NkEntityId id) noexcept;    // toggle (Ctrl+clic)
            void Deselect(ecs::NkEntityId id) noexcept;
            void Clear() noexcept;

            // ── Accès ─────────────────────────────────────────────────────────
            [[nodiscard]] bool             IsSelected(ecs::NkEntityId id) const noexcept;
            [[nodiscard]] ecs::NkEntityId  Primary()   const noexcept { return mPrimary; }
            [[nodiscard]] nk_uint32        Count()     const noexcept {
                return static_cast<nk_uint32>(mSelected.Size());
            }
            [[nodiscard]] bool             HasSelection() const noexcept {
                return !mSelected.IsEmpty();
            }

            // Itération sur la sélection
            const NkVector<ecs::NkEntityId>& All() const noexcept { return mSelected; }

        private:
            void Notify() noexcept {
                EventBus::Dispatch(NkSelectionChangedEvent{mPrimary, Count()});
            }

            NkVector<ecs::NkEntityId> mSelected;
            ecs::NkEntityId           mPrimary = ecs::NkEntityId::Invalid();
        };

    } // namespace Unkeny
} // namespace nkentseu
