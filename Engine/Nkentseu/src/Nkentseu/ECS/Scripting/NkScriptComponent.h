// =============================================================================
// FICHIER: NKECS/Scripting/NkScriptComponent.h
// DESCRIPTION: Système de scripting natif C++ (cycle de vie Unity/MonoBehaviour).
// =============================================================================
#pragma once
#include "../NkECSDefines.h"
#include "../World/NkWorld.h"
#include "../System/NkSystem.h"
#include <memory>
#include <vector>
#include <type_traits>

namespace nkentseu {
    namespace ecs {

        // Forward declarations
        class NkGameObject;
        class NkScriptComponent;

        // ============================================================================
        // NkScriptComponent — Classe de base pour les scripts C++
        // ============================================================================
        // Fournit un cycle de vie complet intégré au Scheduler ECS.
        // Héritez de cette classe pour créer des comportements réutilisables.
        // ============================================================================
        class NkScriptComponent {
            public:
                virtual ~NkScriptComponent() noexcept = default;

                // ── Cycle de vie ────────────────────────────────────────────────────
                virtual void OnAwake(NkWorld& /*world*/, NkEntityId /*self*/) noexcept {}
                virtual void OnStart(NkWorld& /*world*/, NkEntityId /*self*/) noexcept {}
                virtual void OnEnable(NkWorld& /*world*/, NkEntityId /*self*/) noexcept {}
                virtual void OnDisable(NkWorld& /*world*/, NkEntityId /*self*/) noexcept {}
                virtual void OnUpdate(NkWorld& /*world*/, NkEntityId /*self*/, float32 /*dt*/) noexcept {}
                virtual void OnLateUpdate(NkWorld& /*world*/, NkEntityId /*self*/, float32 /*dt*/) noexcept {}
                virtual void OnFixedUpdate(NkWorld& /*world*/, NkEntityId /*self*/, float32 /*fixedDt*/) noexcept {}
                virtual void OnDestroy(NkWorld& /*world*/, NkEntityId /*self*/) noexcept {}

                // ── État ────────────────────────────────────────────────────────────
                void SetEnabled(bool enabled) noexcept { mEnabled = enabled; }
                [[nodiscard]] bool IsEnabled() const noexcept { return mEnabled; }
                [[nodiscard]] bool HasStarted() const noexcept { return mStarted; }

                // Nom du script (pour sérialisation / éditeur)
                [[nodiscard]] virtual const char* GetTypeName() const noexcept = 0;

                // Sérialisation (optionnel, pour hot-reload / sauvegarde)
                virtual void Serialize(char* /*buf*/, uint32 /*bufSize*/) const noexcept {}
                virtual void Deserialize(const char* /*json*/) noexcept {}

            protected:
                bool mEnabled = true;
                bool mStarted = false;

            private:
                friend class NkScriptSystem;
        };

        // ============================================================================
        // NkScriptHost — Composant ECS hébergeant plusieurs scripts par entité
        // ============================================================================
        struct NkScriptHost {
            static constexpr uint32 kMaxScripts = 32u;
            std::shared_ptr<NkScriptComponent> scripts[kMaxScripts] = {};
            uint32 count = 0;
            bool pendingStart = true;
            bool started = false;

            // Attache un script de type T
            template<typename T, typename... Args>
            T* Attach(Args&&... args) noexcept {
                if (count >= kMaxScripts) {
                    return nullptr;
                }
                auto script = std::make_shared<T>(std::forward<Args>(args)...);
                T* raw = script.get();
                scripts[count++] = std::move(script);
                pendingStart = true;
                return raw;
            }

            // Récupère le premier script du type T
            template<typename T>
            [[nodiscard]] T* Get() noexcept {
                for (uint32 i = 0; i < count; ++i) {
                    if (auto* ptr = dynamic_cast<T*>(scripts[i].get())) {
                        return ptr;
                    }
                }
                return nullptr;
            }

            // Vérifie la présence d'un script du type T
            template<typename T>
            [[nodiscard]] bool Has() const noexcept {
                for (uint32 i = 0; i < count; ++i) {
                    if (dynamic_cast<const T*>(scripts[i].get())) {
                        return true;
                    }
                }
                return false;
            }

            // Active/Désactive tous les scripts
            void SetAllEnabled(bool enabled) noexcept {
                for (uint32 i = 0; i < count; ++i) {
                    if (scripts[i]) {
                        scripts[i]->SetEnabled(enabled);
                    }
                }
            }
        };
        NK_COMPONENT(NkScriptHost)

        // ============================================================================
        // NkScriptRegistry — Registre pour instanciation dynamique (éditeur/JSON)
        // ============================================================================
        class NkScriptRegistry {
            public:
                using FactoryFn = std::function<std::shared_ptr<NkScriptComponent>()>;

                [[nodiscard]] static NkScriptRegistry& Global() noexcept {
                    static NkScriptRegistry instance;
                    return instance;
                }

                template<typename T>
                static void Register(const char* name) noexcept {
                    auto& reg = Global();
                    if (reg.mCount >= kMaxEntries) {
                        return;
                    }
                    Entry& e = reg.mEntries[reg.mCount++];
                    std::strncpy(e.name, name, 127);
                    e.factory = [] { return std::make_shared<T>(); };
                }

                [[nodiscard]] std::shared_ptr<NkScriptComponent> Instantiate(const char* name) const noexcept {
                    for (uint32 i = 0; i < mCount; ++i) {
                        if (std::strcmp(mEntries[i].name, name) == 0) {
                            return mEntries[i].factory();
                        }
                    }
                    return nullptr;
                }

                [[nodiscard]] uint32 Count() const noexcept { return mCount; }

            private:
                static constexpr uint32 kMaxEntries = 512u;
                struct Entry {
                    char name[128] = {};
                    FactoryFn factory;
                };
                Entry mEntries[kMaxEntries] = {};
                uint32 mCount = 0;
        };

        // Macro d'auto-enregistrement
        #define NK_REGISTER_SCRIPT(Type)                                              \
        namespace { struct _NkScriptReg_##Type {                                  \
            _NkScriptReg_##Type() noexcept {                                      \
                ::nkentseu::ecs::NkScriptRegistry::Register<Type>(#Type);     \
            }                                                                     \
        } _sNkScriptReg_##Type; }

    } // namespace ecs
} // namespace nkentseu

// =============================================================================
// EXEMPLES D'UTILISATION DE NKSCRIPTCOMPONENT.H
// =============================================================================
/*
// -----------------------------------------------------------------------------
// Exemple 1 : Création d'un script personnalisé
// -----------------------------------------------------------------------------
class PlayerMovement : public nkentseu::ecs::NkScriptComponent {
public:
    void OnStart(nkentseu::ecs::NkWorld& world, nkentseu::ecs::NkEntityId self) noexcept override {
        mSpeed = 5.0f;
        printf("[PlayerMovement] Démarré sur entité %u\n", self.index);
    }

    void OnUpdate(nkentseu::ecs::NkWorld& world, nkentseu::ecs::NkEntityId self, float32 dt) noexcept override {
        auto* t = world.Get<nkentseu::ecs::NkTransform>(self);
        if (t) {
            t->position.x += mSpeed * dt;
            t->dirty = true;
        }
    }

    [[nodiscard]] const char* GetTypeName() const noexcept override { return "PlayerMovement"; }

private:
    float32 mSpeed = 0.f;
};
NK_REGISTER_SCRIPT(PlayerMovement)

// -----------------------------------------------------------------------------
// Exemple 2 : Attacher un script à un GameObject
// -----------------------------------------------------------------------------
void Exemple_AttachScript(nkentseu::ecs::NkWorld& world) {
    auto go = world.CreateGameObject("Player");
    auto* script = go.AddBehaviour<PlayerMovement>();
    if (script) {
        printf("Script attaché : %s\n", script->GetTypeName());
    }
}

// -----------------------------------------------------------------------------
// Exemple 3 : Instanciation dynamique via le Registre
// -----------------------------------------------------------------------------
void Exemple_DynamicInstantiation(nkentseu::ecs::NkWorld& world) {
    auto go = world.CreateGameObject("DynamicEntity");
    auto* host = world.Get<nkentseu::ecs::NkScriptHost>(go.Id());
    if (host) {
        auto script = nkentseu::ecs::NkScriptRegistry::Global().Instantiate("PlayerMovement");
        if (script) {
            host->scripts[host->count++] = script;
            host->pendingStart = true;
        }
    }
}
*/