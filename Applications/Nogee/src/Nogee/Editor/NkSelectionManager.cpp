#include "NkSelectionManager.h"

namespace nkentseu {
    namespace Unkeny {

        void NkSelectionManager::Select(ecs::NkEntityId id) noexcept {
            mSelected.Clear();
            if (id.IsValid()) mSelected.PushBack(id);
            mPrimary = id;
            Notify();
        }

        void NkSelectionManager::SelectAdd(ecs::NkEntityId id) noexcept {
            if (!id.IsValid() || IsSelected(id)) return;
            mSelected.PushBack(id);
            mPrimary = id;
            Notify();
        }

        void NkSelectionManager::SelectToggle(ecs::NkEntityId id) noexcept {
            if (IsSelected(id)) Deselect(id);
            else                SelectAdd(id);
        }

        void NkSelectionManager::Deselect(ecs::NkEntityId id) noexcept {
            for (nk_isize i = (nk_isize)mSelected.Size() - 1; i >= 0; --i) {
                if (mSelected[i] == id) { mSelected.EraseAt((nk_usize)i); break; }
            }
            if (mPrimary == id)
                mPrimary = mSelected.IsEmpty() ? ecs::NkEntityId::Invalid()
                                               : mSelected[mSelected.Size() - 1];
            Notify();
        }

        void NkSelectionManager::Clear() noexcept {
            mSelected.Clear();
            mPrimary = ecs::NkEntityId::Invalid();
            Notify();
        }

        bool NkSelectionManager::IsSelected(ecs::NkEntityId id) const noexcept {
            for (nk_usize i = 0; i < mSelected.Size(); ++i)
                if (mSelected[i] == id) return true;
            return false;
        }

    } // namespace Unkeny
} // namespace nkentseu
