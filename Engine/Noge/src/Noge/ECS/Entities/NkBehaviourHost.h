#pragma once
// =============================================================================
// NkBehaviourHost.h — composant ECS qui HÉBERGE les NkBehaviour d'une entité.
// =============================================================================
// Rôle : chaque GameObject porteur de scripts possède un NkBehaviourHost (ajouté
// à la volée par NkGameObject::AddBehaviour). L'hôte POSSÈDE les behaviours
// (alloués via NKMemory) et expose l'API attendue par :
//   - NkGameObject  : host->Add<T>(args...) / host->Get<T>()
//   - NkBehaviourSystem (+Late/+Fixed) : accès direct à count / behaviours[] /
//     hasStarted pour piloter OnStart/OnUpdate/OnLateUpdate/OnFixedUpdate.
//
// Cycle de vie mémoire (règle NKMemory : jamais de new/delete brut) :
//   Add<T>   : NkAlloc(sizeof(T)) + NkConstruct<T>(...)   -> NkBehaviour*
//   ~        : OnDestroy() + NkDestroy + NkFree pour chaque behaviour
//
// Sémantique de relocalisation ECS :
//   - MOVE (utilisé par l'archétype pour déplacer un composant) : TRANSFERT de
//     propriété, la source est vidée.
//   - COPY (instanciée par NkTypeRegistry::CopyConstruct) : produit un hôte VIDE.
//     Copier un hôte n'a pas de sens (behaviours polymorphes non clonables) ; on
//     dégrade en hôte vide pour rester compilable et SANS double-free.
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKCore/NkTraits.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"
#include "Noge/ECS/Entities/NkBehaviour.h"

namespace nkentseu {
    namespace ecs {

        struct NkBehaviourHost {
            // Capacité inline (SBO) : 16 scripts par entité — largement suffisant.
            static constexpr uint32 kCapacity = 16u;

            // --- État accédé DIRECTEMENT par NkBehaviourSystem (donc public) ---
            NkBehaviour* behaviours[kCapacity] = {};
            uint32       count                 = 0u;
            bool         hasStarted            = false;

            // -----------------------------------------------------------------
            NkBehaviourHost() noexcept = default;

            ~NkBehaviourHost() noexcept { Clear(); }

            // MOVE : transfert de propriété (utilisé par la relocalisation ECS).
            NkBehaviourHost(NkBehaviourHost&& o) noexcept { MoveFrom(o); }
            NkBehaviourHost& operator=(NkBehaviourHost&& o) noexcept {
                if (this != &o) { Clear(); MoveFrom(o); }
                return *this;
            }

            // COPY : dégradée en hôte VIDE (voir entête). Requise compilable car
            // NkTypeRegistry::CopyConstruct prend l'adresse de la copie.
            NkBehaviourHost(const NkBehaviourHost&) noexcept {}
            NkBehaviourHost& operator=(const NkBehaviourHost&) noexcept {
                Clear();
                return *this;
            }

            // --- API attendue par NkGameObject ------------------------------
            template<typename T, typename... Args>
            T* Add(Args&&... args) noexcept {
                if (count >= kCapacity) return nullptr;
                void* mem = nkentseu::memory::NkAlloc(sizeof(T), nullptr, alignof(T));
                if (!mem) return nullptr;
                T* obj = nkentseu::memory::NkConstruct<T>(
                    static_cast<T*>(mem), nkentseu::traits::NkForward<Args>(args)...);
                behaviours[count++] = obj;   // upcast implicite T* -> NkBehaviour*
                return obj;
            }

            template<typename T>
            T* Get() const noexcept {
                for (uint32 i = 0; i < count; ++i) {
                    if (behaviours[i]) {
                        if (T* p = dynamic_cast<T*>(behaviours[i])) return p;
                    }
                }
                return nullptr;
            }

            void Clear() noexcept {
                for (uint32 i = 0; i < count; ++i) {
                    NkBehaviour* b = behaviours[i];
                    if (b) {
                        b->OnDestroy();
                        nkentseu::memory::NkDestroy(b);   // ~NkBehaviour virtuel -> dérivé
                        nkentseu::memory::NkFree(b);
                        behaviours[i] = nullptr;
                    }
                }
                count      = 0u;
                hasStarted = false;
            }

        private:
            void MoveFrom(NkBehaviourHost& o) noexcept {
                count      = o.count;
                hasStarted = o.hasStarted;
                for (uint32 i = 0; i < count; ++i) {
                    behaviours[i]   = o.behaviours[i];
                    o.behaviours[i] = nullptr;
                }
                o.count      = 0u;
                o.hasStarted = false;
            }
        };

    } // namespace ecs
} // namespace nkentseu
