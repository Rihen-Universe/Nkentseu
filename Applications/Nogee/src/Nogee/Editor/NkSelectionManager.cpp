#include "NkSelectionManager.h"

namespace nkentseu {
	namespace noge {

		void NkSelectionManager::Select(ecs::NkEntityId id) noexcept {
			mSelected.Clear();
			if (id.IsValid())
				mSelected.PushBack(id);
			mPrimary = id;
			Notify();
		}

		void NkSelectionManager::SelectAdd(ecs::NkEntityId id) noexcept {
			if (!id.IsValid() || IsSelected(id))
				return;
			mSelected.PushBack(id);
			mPrimary = id;
			Notify();
		}

		void NkSelectionManager::SelectToggle(ecs::NkEntityId id) noexcept {
			if (IsSelected(id))
				Deselect(id);
			else
				SelectAdd(id);
		}

		void NkSelectionManager::Deselect(ecs::NkEntityId id) noexcept {
			for (nk_int64 i = (nk_int64)mSelected.Size() - 1; i >= 0; --i) {
				if (mSelected[(nk_usize)i] == id) {
					mSelected.Erase(mSelected.Begin() + i);
					break;
				}
			}
			if (mPrimary == id)
				mPrimary = mSelected.IsEmpty() ? ecs::NkEntityId::Invalid() : mSelected[mSelected.Size() - 1];
			Notify();
		}

		void NkSelectionManager::Clear() noexcept {
			mSelected.Clear();
			mPrimary = ecs::NkEntityId::Invalid();
			Notify();
		}

		bool NkSelectionManager::IsSelected(ecs::NkEntityId id) const noexcept {
			for (nk_usize i = 0; i < mSelected.Size(); ++i)
				if (mSelected[i] == id)
					return true;
			return false;
		}

	} // namespace noge
} // namespace nkentseu
