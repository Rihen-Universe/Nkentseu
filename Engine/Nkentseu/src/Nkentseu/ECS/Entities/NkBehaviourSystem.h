#pragma once

#include "NKECS/System/NkSystem.h"
#include "Nkentseu/ECS/Entities/NkGameObject.h"

namespace nkentseu {
namespace ecs {

    // =========================================================================
    // NkBehaviourSystem — exécute le cycle de vie des behaviours
    // =========================================================================
    class NkBehaviourSystem : public NkSystem {
    public:
        NkSystemDesc Describe() const override {
            return NkSystemDesc{}
                .Writes<NkBehaviourHost>()
                .Reads<NkInactive>()           // pour détecter les GameObjects désactivés
                .InGroup(NkSystemGroup::PreUpdate)   // OnStart avant Update
                .Named("NkBehaviourSystem");
        }

        void Execute(NkWorld& world, float dt) override {
            // 1. Phase OnStart (uniquement pour les nouveaux behaviours)
            world.Query<NkBehaviourHost>()
                .Without<NkInactive>()  // ne traite que les GameObjects actifs
                .ForEach([&](NkEntityId, NkBehaviourHost& host) {
                    if (host.hasStarted) return;
                    for (uint32 i = 0; i < host.count; ++i) {
                        auto& beh = host.behaviours[i];
                        if (beh && beh->IsEnabled() && !beh->mStarted) {
                            beh->OnStart();
                            beh->mStarted = true;
                        }
                    }
                    host.hasStarted = true;
                });

            // 2. Phase OnUpdate
            world.Query<NkBehaviourHost>()
                .Without<NkInactive>()
                .ForEach([&](NkEntityId, NkBehaviourHost& host) {
                    for (uint32 i = 0; i < host.count; ++i) {
                        auto& beh = host.behaviours[i];
                        if (beh && beh->IsEnabled() && beh->mStarted)
                            beh->OnUpdate(dt);
                    }
                });
        }
    };

    // =========================================================================
    // NkBehaviourLateSystem — exécute OnLateUpdate après les autres mises à jour
    // =========================================================================
    class NkBehaviourLateSystem : public NkSystem {
    public:
        NkSystemDesc Describe() const override {
            return NkSystemDesc{}
                .Writes<NkBehaviourHost>()
                .InGroup(NkSystemGroup::PostUpdate)
                .Named("NkBehaviourLateSystem");
        }

        void Execute(NkWorld& world, float dt) override {
            world.Query<NkBehaviourHost>()
                .Without<NkInactive>()
                .ForEach([&](NkEntityId, NkBehaviourHost& host) {
                    for (uint32 i = 0; i < host.count; ++i) {
                        auto& beh = host.behaviours[i];
                        if (beh && beh->IsEnabled() && beh->mStarted)
                            beh->OnLateUpdate(dt);
                    }
                });
        }
    };

    // =========================================================================
    // NkBehaviourFixedSystem — exécute OnFixedUpdate (physique)
    // =========================================================================
    class NkBehaviourFixedSystem : public NkSystem {
    public:
        NkSystemDesc Describe() const override {
            return NkSystemDesc{}
                .Writes<NkBehaviourHost>()
                .InGroup(NkSystemGroup::FixedUpdate)
                .Named("NkBehaviourFixedSystem");
        }

        void Execute(NkWorld& world, float fixedDt) override {
            world.Query<NkBehaviourHost>()
                .Without<NkInactive>()
                .ForEach([&](NkEntityId, NkBehaviourHost& host) {
                    for (uint32 i = 0; i < host.count; ++i) {
                        auto& beh = host.behaviours[i];
                        if (beh && beh->IsEnabled() && beh->mStarted)
                            beh->OnFixedUpdate(fixedDt);
                    }
                });
        }
    };

} // namespace ecs
} // namespace nkentseu