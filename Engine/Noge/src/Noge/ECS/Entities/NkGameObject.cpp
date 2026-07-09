// =============================================================================
// GameObject/NkGameObject.cpp — Implémentations non-template de NkGameObject
// =============================================================================
// Seules les méthodes NON définies inline dans NkGameObject.h vivent ici :
// hiérarchie (SetParent/GetParent/AddChild/GetChildren[V]) + activation/cycle de
// vie (SetActive/IsActive/DestroyDeferred). Tous les accesseurs d'identité et de
// Transform (Id/IsValid/World/Name/SetName/Transform/SetPosition...) sont INLINE
// dans le header (style Unity) — ne pas les redéfinir ici (redefinition ODR).
// =============================================================================

#include "NkGameObject.h"
#include "NKECS/World/NkWorld.h"

namespace nkentseu {
	namespace ecs {

		// =====================================================================
		// Hiérarchie : gestion parent/enfants (via composants NkParent/NkChildren)
		// =====================================================================

		void NkGameObject::SetParent(const NkGameObject &parent) noexcept {
			if (!IsValid() || !parent.IsValid())
				return;

			// NkParent sur cette entité
			if (auto *p = mWorld->Get<NkParent>(mId)) {
				p->entity = parent.mId;
			} else {
				mWorld->Add<NkParent>(mId, NkParent{parent.mId});
			}

			// Réciproque : liste NkChildren du parent
			auto *ch = mWorld->Get<NkChildren>(parent.mId);
			if (!ch) {
				ch = &mWorld->Add<NkChildren>(parent.mId);
			}
			if (!ch->Has(mId)) {
				ch->Add(mId);
			}

			// Marquage dirty pour propagation hiérarchique
			if (auto *t = mWorld->Get<NkTransform>(mId))
				t->MarkDirty();
		}

		NkGameObject NkGameObject::GetParent() const noexcept {
			if (!IsValid())
				return {};
			if (const auto *p = mWorld->Get<NkParent>(mId)) {
				if (p->entity.IsValid()) {
					return NkGameObject(p->entity, mWorld);
				}
			}
			return {};
		}

		void NkGameObject::AddChild(const NkGameObject &child) noexcept {
			if (!IsValid() || !child.IsValid())
				return;
			NkGameObject c = child; // handle léger ; SetParent est non-const
			c.SetParent(*this);		// délègue (évite la duplication)
		}

		void NkGameObject::GetChildren(NkVector<NkGameObject> &out) const noexcept {
			if (!IsValid())
				return;
			if (const auto *ch = mWorld->Get<NkChildren>(mId)) {
				out.Reserve(out.Size() + ch->count);
				for (uint32 i = 0; i < ch->count; ++i) {
					out.PushBack(NkGameObject(ch->children[i], mWorld));
				}
			}
		}

		NkVector<NkGameObject> NkGameObject::GetChildrenV() const noexcept {
			NkVector<NkGameObject> result;
			GetChildren(result);
			return result;
		}

		// =====================================================================
		// Activation & cycle de vie (via marqueur NkInactive + destruction différée)
		// =====================================================================

		void NkGameObject::SetActive(bool active) noexcept {
			if (!IsValid())
				return;
			if (active) {
				mWorld->Remove<NkInactive>(mId);
			} else {
				mWorld->AddDeferred<NkInactive>(mId);
			}
		}

		bool NkGameObject::IsActive() const noexcept {
			return IsValid() && !mWorld->Has<NkInactive>(mId);
		}

		void NkGameObject::DestroyDeferred() noexcept {
			if (IsValid()) {
				mWorld->DestroyDeferred(mId);
			}
		}

	} // namespace ecs
} // namespace nkentseu
