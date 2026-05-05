// =============================================================================
// FICHIER: NKECS/Scripting/NkScriptSystem.h
// DESCRIPTION: Implémentation des systèmes d'exécution des scripts.
// =============================================================================

#pragma once

#include "../NkECSDefines.h"
#include "NkScriptComponent.h"

namespace nkentseu {
    namespace ecs {

        // ============================================================================
        // NkScriptSystem — Exécute OnUpdate chaque frame
        // ============================================================================
        class NkScriptSystem final : public NkSystem {
            public:
                NkSystemDesc Describe() const override {
                    return NkSystemDesc{}
                        .Writes<NkScriptHost>()
                        .Sequential()
                        .InGroup(NkSystemGroup::Update)
                        .Named("NkScriptSystem");
                }

                void Execute(NkWorld& world, float32 dt) override {
                    world.Query<NkScriptHost>()
                        .Without<NkInactive>()
                        .ForEach([&](NkEntityId id, NkScriptHost& host) {
                            // Phase OnStart (une seule fois)
                            if (host.pendingStart) {
                                for (uint32 i = 0; i < host.count; ++i) {
                                    auto& s = host.scripts[i];
                                    if (s && s->IsEnabled() && !s->HasStarted()) {
                                        s->OnStart(world, id);
                                        s->mStarted = true;
                                    }
                                }
                                host.pendingStart = false;
                                host.started = true;
                            }

                            // Phase OnUpdate
                            for (uint32 i = 0; i < host.count; ++i) {
                                auto& s = host.scripts[i];
                                if (s && s->IsEnabled() && s->HasStarted()) {
                                    s->OnUpdate(world, id, dt);
                                }
                            }
                        });
                }
        };

        // ============================================================================
        // NkScriptLateSystem — Exécute OnLateUpdate après Update
        // ============================================================================
        class NkScriptLateSystem final : public NkSystem {
            public:
                NkSystemDesc Describe() const override {
                    return NkSystemDesc{}
                        .Writes<NkScriptHost>()
                        .Sequential()
                        .InGroup(NkSystemGroup::PostUpdate)
                        .Named("NkScriptLateSystem");
                }

                void Execute(NkWorld& world, float32 dt) override {
                    world.Query<NkScriptHost>()
                        .Without<NkInactive>()
                        .ForEach([&](NkEntityId id, NkScriptHost& host) {
                            for (uint32 i = 0; i < host.count; ++i) {
                                auto& s = host.scripts[i];
                                if (s && s->IsEnabled() && s->HasStarted()) {
                                    s->OnLateUpdate(world, id, dt);
                                }
                            }
                        });
                }
        };

        // ============================================================================
        // NkScriptFixedSystem — Exécute OnFixedUpdate (Physique / Gameplay stable)
        // ============================================================================
        class NkScriptFixedSystem final : public NkSystem {
            public:
                NkSystemDesc Describe() const override {
                    return NkSystemDesc{}
                        .Writes<NkScriptHost>()
                        .Sequential()
                        .InGroup(NkSystemGroup::FixedUpdate)
                        .Named("NkScriptFixedSystem");
                }
 
                void Execute(NkWorld& world, float32 fixedDt) override {
                    world.Query<NkScriptHost>()
                        .Without<NkInactive>()
                        .ForEach([&](NkEntityId id, NkScriptHost& host) {
                            for (uint32 i = 0; i < host.count; ++i) {
                                auto& s = host.scripts[i];
                                if (s && s->IsEnabled() && s->HasStarted()) {
                                    s->OnFixedUpdate(world, id, fixedDt);
                                }
                            }
                        });
                }
        };

    } // namespace ecs
} // namespace nkentseu