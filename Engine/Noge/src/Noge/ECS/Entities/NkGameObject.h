#pragma once
// =============================================================================
// Nkentseu/ECS/Entities/NkGameObject.h — v3 (NkComponentHandle intégré)
// =============================================================================
/**
 * @file NkGameObject.h
 * @brief Handle léger vers une entité ECS avec API de haut niveau (16 bytes).
 *
 * 🔹 CRÉATION : Toujours via NkGameObjectFactory ou NkSceneGraph::SpawnActor.
 *   Jamais directement : le constructeur explicite est réservé aux factories.
 *
 * 🔹 COMPOSANTS INVARIANTS (garantis présents après NkGameObjectFactory::Create) :
 *   NkTransform, NkName, NkTag, NkParent, NkChildren, NkBehaviourHost
 *
 * 🔹 API COMPOSANTS — trois modes selon le besoin :
 *   GetComponent<T>()      → NkComponentHandle<T>   (nullable, style safe)
 *   RequireComponent<T>()  → NkRequiredComponent<T> (assert, pour invariants)
 *   Optional<T>()          → NkOptionalComponent<T> (never-crash, dummy)
 *   AddComponent<T>(args)  → ajoute + retourne NkComponentHandle<T>
 *   AddMultiple<T>(args)   → instances multiples via NkComponentBag
 *   RemoveComponent<T>()   → suppression immédiate
 *   RemoveComponentDeferred<T>() → suppression différée (safe en query)
 *   HasComponent<T>()      → bool
 */

#include "NKECS/NkECSDefines.h"
#include "NKECS/World/NkWorld.h"
#include "Nkentseu/ECS/Components/NkComponentHandle.h"
#include "Nkentseu/ECS/Components/Core/NkTransform.h"
#include "Nkentseu/ECS/Components/Core/NkTag.h"
#include "NkBehaviour.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTraits.h"

namespace nkentseu {
    namespace ecs {

        class NkSceneGraph;
        struct NkBehaviourHost;

        // =====================================================================
        // NkComponentBag<T> — Stockage multi-instances (SBO 8 éléments)
        // =====================================================================
        template<typename T>
        struct NkComponentBag {
            static constexpr uint32 kSBOCapacity = 8u;
            NkComponentBag() noexcept = default;
            ~NkComponentBag() noexcept {
                if (mIsHeap) { delete heapVec; }
                else if constexpr (!traits::NkIsTriviallyDestructible_v<T>) {
                    for (uint32 i = 0; i < mCount; ++i)
                        reinterpret_cast<T*>(inlineBuf)[i].~T();
                }
            }
            NkComponentBag(const NkComponentBag&) = delete;
            NkComponentBag& operator=(const NkComponentBag&) = delete;

            T*              Add(const T& value = T{}) noexcept;
            T*              Get(uint32 idx) noexcept;
            const T*        Get(uint32 idx) const noexcept;
            NkSpan<T>       GetAll() noexcept;
            NkSpan<const T> GetAll() const noexcept;
            [[nodiscard]] uint32 Count() const noexcept { return mCount; }

        private:
            alignas(T) uint8 inlineBuf[sizeof(T) * kSBOCapacity] = {};
            NkVector<T>*     heapVec = nullptr;
            uint32           mCount  = 0;
            bool             mIsHeap = false;
        };

        // =====================================================================
        // NkGameObject — Handle léger (NkEntityId + NkWorld* = 16 bytes)
        // =====================================================================
        class NkGameObject {
            public:
                NkGameObject() noexcept = default;
                explicit NkGameObject(NkEntityId id, NkWorld* world) noexcept
                    : mId(id), mWorld(world) {}

                // ── Identité ──────────────────────────────────────────────
                [[nodiscard]] NkEntityId  Id()      const noexcept { return mId; }
                [[nodiscard]] bool        IsValid()  const noexcept;
                [[nodiscard]] NkWorld&    World()    const noexcept;
                [[nodiscard]] const char* Name()     const noexcept;
                void SetName(const char* name) noexcept;
                explicit operator bool() const noexcept { return IsValid(); }
                bool operator==(const NkGameObject& o) const noexcept {
                    return mId == o.mId && mWorld == o.mWorld;
                }
                bool operator!=(const NkGameObject& o) const noexcept {
                    return !(*this == o);
                }

                // ── Accès rapide aux invariants ────────────────────────────
                [[nodiscard]] NkRequiredComponent<NkTransform> Transform() const noexcept {
                    return {mWorld, mId};
                }
                [[nodiscard]] NkRequiredComponent<NkTag> Tags() const noexcept {
                    return {mWorld, mId};
                }

                // ── Raccourcis Transform (style Unity) ─────────────────────
                void SetPosition(const NkVec3f& p) noexcept {
                    if (auto* t = Resolve<NkTransform>()) t->SetLocalPosition(p);
                }
                void SetPosition(float32 x, float32 y, float32 z) noexcept {
                    if (auto* t = Resolve<NkTransform>()) t->SetLocalPosition(x, y, z);
                }
                void SetRotation(const NkQuatf& q) noexcept {
                    if (auto* t = Resolve<NkTransform>()) t->SetLocalRotation(q);
                }
                void SetScale(const NkVec3f& s) noexcept {
                    if (auto* t = Resolve<NkTransform>()) t->SetLocalScale(s);
                }
                [[nodiscard]] NkVec3f GetPosition() const noexcept {
                    if (const auto* t = ResolveConst<NkTransform>()) return t->localPosition;
                    return {};
                }
                [[nodiscard]] NkVec3f GetWorldPosition() const noexcept {
                    if (const auto* t = ResolveConst<NkTransform>()) return t->GetWorldPosition();
                    return {};
                }

                // ── API Composants — NkComponentHandle ─────────────────────
                /**
                 * @brief Handle résolvable vers T (nullable, pattern safe).
                 * @code
                 * if (auto hp = go.GetComponent<NkHealth>()) hp->value -= 10;
                 * @endcode
                 */
                template<typename T>
                [[nodiscard]] NkComponentHandle<T> GetComponent() const noexcept {
                    return NkComponentHandle<T>(mWorld, mId);
                }

                /**
                 * @brief Handle vers composant garanti (asserte si absent).
                 * Réservé aux composants invariants (NkTransform, NkName...).
                 */
                template<typename T>
                [[nodiscard]] NkRequiredComponent<T> RequireComponent() const noexcept {
                    return NkRequiredComponent<T>(mWorld, mId);
                }

                /**
                 * @brief Handle optionnel : retourne un dummy si absent (never-crash).
                 */
                template<typename T>
                [[nodiscard]] NkOptionalComponent<T> Optional() const noexcept {
                    return NkOptionalComponent<T>(mWorld, mId);
                }

                /**
                 * @brief Ajoute T et retourne NkComponentHandle.
                 */
                template<typename T, typename... Args>
                NkComponentHandle<T> AddComponent(Args&&... args) noexcept {
                    if (!IsValid()) return {};
                    mWorld->Add<T>(mId, T{traits::NkForward<Args>(args)...});
                    return NkComponentHandle<T>(mWorld, mId);
                }

                /**
                 * @brief Ajoute une instance supplémentaire de T (multi-instances).
                 */
                template<typename T, typename... Args>
                T* AddMultiple(Args&&... args) noexcept {
                    if (!IsValid()) return nullptr;
                    auto* bag = mWorld->Get<NkComponentBag<T>>(mId);
                    if (!bag) bag = &mWorld->Add<NkComponentBag<T>>(mId);
                    return bag->Add(T{traits::NkForward<Args>(args)...});
                }

                /**
                 * @brief Toutes les instances de T (via NkComponentBag ou singleton).
                 */
                template<typename T>
                [[nodiscard]] NkSpan<T> GetAllComponents() noexcept {
                    if (!IsValid()) return {};
                    if (auto* bag = mWorld->Get<NkComponentBag<T>>(mId))
                        return bag->GetAll();
                    if (T* c = mWorld->Get<T>(mId)) return {c, 1};
                    return {};
                }

                template<typename T> void RemoveComponent() noexcept {
                    if (IsValid()) mWorld->Remove<T>(mId);
                }
                template<typename T> void RemoveComponentDeferred() noexcept {
                    if (IsValid()) mWorld->RemoveDeferred<T>(mId);
                }
                template<typename T>
                [[nodiscard]] bool HasComponent() const noexcept {
                    return IsValid() && mWorld->Has<T>(mId);
                }

                // ── Compatibilité descendante ──────────────────────────────
                template<typename T, typename... Args>
                T* Add(Args&&... args) noexcept {
                    return AddComponent<T>(traits::NkForward<Args>(args)...).Get();
                }
                template<typename T>
                [[nodiscard]] T* Get() noexcept { return Resolve<T>(); }
                template<typename T>
                [[nodiscard]] const T* Get() const noexcept { return ResolveConst<T>(); }
                template<typename T>
                [[nodiscard]] bool Has() const noexcept { return HasComponent<T>(); }
                template<typename T>
                void Remove() noexcept { RemoveComponent<T>(); }
                template<typename T>
                [[nodiscard]] NkSpan<T> GetAll() noexcept { return GetAllComponents<T>(); }

                // ── Hiérarchie ─────────────────────────────────────────────
                void SetParent(const NkGameObject& parent) noexcept;
                [[nodiscard]] NkGameObject GetParent() const noexcept;
                void AddChild(const NkGameObject& child) noexcept;
                void GetChildren(NkVector<NkGameObject>& out) const noexcept;
                [[nodiscard]] NkVector<NkGameObject> GetChildrenV() const noexcept;

                // ── Behaviours ─────────────────────────────────────────────
                template<typename T, typename... Args>
                T* AddBehaviour(Args&&... args) noexcept;
                template<typename T>
                [[nodiscard]] T* GetBehaviour() const noexcept;

                // ── Activation & Cycle de vie ──────────────────────────────
                void SetActive(bool active) noexcept;
                [[nodiscard]] bool IsActive() const noexcept;
                void DestroyDeferred() noexcept;
                void Destroy() noexcept { DestroyDeferred(); }

            protected:
                NkEntityId mId    = NkEntityId::Invalid();
                NkWorld*   mWorld = nullptr;

            private:
                template<typename T> T* Resolve() noexcept {
                    return (mWorld && mId.IsValid()) ? mWorld->Get<T>(mId) : nullptr;
                }
                template<typename T> const T* ResolveConst() const noexcept {
                    return (mWorld && mId.IsValid()) ? mWorld->Get<T>(mId) : nullptr;
                }

                friend class NkSceneGraph;
        };

        // =====================================================================
        // Implémentations inline NkComponentBag
        // =====================================================================
        template<typename T>
        T* NkComponentBag<T>::Add(const T& value) noexcept {
            if (mIsHeap) {
                heapVec->PushBack(value);
                return &(*heapVec)[heapVec->Size() - 1];
            }
            if (mCount >= kSBOCapacity) {
                heapVec = new NkVector<T>();
                heapVec->Reserve(kSBOCapacity + 8);
                T* buf = reinterpret_cast<T*>(inlineBuf);
                for (uint32 i = 0; i < mCount; ++i)
                    heapVec->PushBack(static_cast<T&&>(buf[i]));
                heapVec->PushBack(value);
                mIsHeap = true;
                return &(*heapVec)[heapVec->Size() - 1];
            }
            T* slot = reinterpret_cast<T*>(inlineBuf) + mCount++;
            new (slot) T(value);
            return slot;
        }
        template<typename T>
        T* NkComponentBag<T>::Get(uint32 idx) noexcept {
            if (idx >= mCount) return nullptr;
            return mIsHeap ? &(*heapVec)[idx] : reinterpret_cast<T*>(inlineBuf) + idx;
        }
        template<typename T>
        const T* NkComponentBag<T>::Get(uint32 idx) const noexcept {
            if (idx >= mCount) return nullptr;
            return mIsHeap ? &(*heapVec)[idx] : reinterpret_cast<const T*>(inlineBuf) + idx;
        }
        template<typename T>
        NkSpan<T> NkComponentBag<T>::GetAll() noexcept {
            return mIsHeap ? NkSpan<T>(heapVec->Data(), heapVec->Size())
                           : NkSpan<T>(reinterpret_cast<T*>(inlineBuf), mCount);
        }
        template<typename T>
        NkSpan<const T> NkComponentBag<T>::GetAll() const noexcept {
            return mIsHeap ? NkSpan<const T>(heapVec->Data(), heapVec->Size())
                           : NkSpan<const T>(reinterpret_cast<const T*>(inlineBuf), mCount);
        }

        // =====================================================================
        // Implémentations inline NkGameObject
        // =====================================================================
        inline bool NkGameObject::IsValid() const noexcept {
            return mWorld && mId.IsValid() && mWorld->IsAlive(mId);
        }
        inline NkWorld& NkGameObject::World() const noexcept {
            NKECS_ASSERT(mWorld);
            return *mWorld;
        }
        inline const char* NkGameObject::Name() const noexcept {
            if (!IsValid()) return "InvalidGO";
            const auto* n = mWorld->Get<NkName>(mId);
            return n ? n->Get() : "UnnamedGO";
        }
        inline void NkGameObject::SetName(const char* name) noexcept {
            if (!IsValid() || !name) return;
            if (auto* n = mWorld->Get<NkName>(mId)) n->Set(name);
            else mWorld->Add<NkName>(mId, NkName(name));
        }
        template<typename T, typename... Args>
        T* NkGameObject::AddBehaviour(Args&&... args) noexcept {
            if (!IsValid()) return nullptr;
            auto* host = mWorld->Get<NkBehaviourHost>(mId);
            if (!host) host = &mWorld->Add<NkBehaviourHost>(mId);
            T* raw = host->template Add<T>(traits::NkForward<Args>(args)...);
            if (raw) raw->SetGameObject(this);
            return raw;
        }
        template<typename T>
        T* NkGameObject::GetBehaviour() const noexcept {
            if (!IsValid()) return nullptr;
            auto* host = mWorld->Get<NkBehaviourHost>(mId);
            return host ? host->template Get<T>() : nullptr;
        }

    } // namespace ecs
} // namespace nkentseu
