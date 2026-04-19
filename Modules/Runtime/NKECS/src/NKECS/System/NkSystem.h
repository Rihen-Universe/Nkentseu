// =============================================================================
// System/NkSystem.h
// =============================================================================
// Définition de la classe de base pour les systèmes ECS et de leur descripteur.
//
// Un système est une unité de logique qui s'exécute à chaque frame (ou à
// intervalle fixe). Il déclare explicitement quels composants il lit et écrit,
// ce qui permet au `NkScheduler` de :
//   - Détecter les conflits d'accès concurrents (race conditions).
//   - Construire un graphe de dépendances (DAG) pour l'exécution.
//   - Paralléliser les systèmes indépendants (job system).
//
// Chaque système appartient à un groupe d'exécution (PreUpdate, Update, etc.)
// et peut déclarer des dépendances explicites envers d'autres systèmes.
// =============================================================================

#pragma once

#include "NKECS/NkECSDefines.h"
#include "NKECS/Core/NkTypeRegistry.h"
#include <functional>   // pour std::function
#include <string>       // pour std::string
#include <vector>       // pour std::vector
#include <typeindex>    // pour std::type_index

namespace nkentseu {
    namespace ecs {

        // ---------------------------------------------------------------------
        // Déclaration anticipée de NkWorld
        // ---------------------------------------------------------------------
        class NkWorld;

        // =====================================================================
        // NkSystemGroup
        // =====================================================================
        // Énumération des phases d'exécution dans une frame.
        // L'ordre est fixe : PreUpdate → Update → PostUpdate → FixedUpdate → Render.
        // Le scheduler exécute les systèmes groupe par groupe.
        enum class NkSystemGroup : uint8 {
            PreUpdate   = 0,   // Avant la logique principale (ex: traitement des entrées)
            Update      = 1,   // Logique de jeu principale (déplacement, IA, etc.)
            PostUpdate  = 2,   // Après Update (ex: caméra, préparation du rendu)
            FixedUpdate = 3,   // Physique à taux fixe (indépendant du delta time)
            Render      = 4,   // Préparation des commandes de rendu
            COUNT              // Nombre total de groupes (utilitaire)
        };

        // =====================================================================
        // NkSystemDesc
        // =====================================================================
        // Descripteur déclaratif des dépendances et propriétés d'un système.
        // Utilisé par le scheduler pour l'ordonnancement et la parallélisation.
        struct NkSystemDesc {
            // --- Masques de composants ---
            NkComponentMask readMask;       // Composants lus (accès en lecture seule)
            NkComponentMask writeMask;      // Composants écrits (accès en écriture)
            NkComponentMask excludeMask;    // Composants exclus des requêtes internes (optimisation)

            // --- Ordonnancement ---
            NkSystemGroup   group = NkSystemGroup::Update;   // Groupe d'appartenance
            bool            runInParallel = true;            // Peut s'exécuter en parallèle si pas de conflit
            float32         priority = 0.0f;                 // Priorité au sein du groupe (plus petit = plus tôt)
            const char*     name = "<unnamed>";              // Nom du système (débogage)

            // --- Dépendances explicites ---
            std::vector<std::type_index> afterTypes;         // Types de systèmes à exécuter avant celui-ci

            // -----------------------------------------------------------------
            // Méthodes de configuration (fluent API)
            // -----------------------------------------------------------------

            // Déclare que le système lit le composant T.
            template<typename T>
            NkSystemDesc& Reads() noexcept {
                readMask.Set(NkIdOf<std::remove_const_t<T>>());
                return *this;
            }

            // Déclare que le système écrit le composant T (implique aussi la lecture).
            template<typename T>
            NkSystemDesc& Writes() noexcept {
                writeMask.Set(NkIdOf<std::remove_const_t<T>>());
                readMask.Set(NkIdOf<std::remove_const_t<T>>()); // write implique read
                return *this;
            }

            // Déclare que le système exclut les entités possédant T de ses requêtes.
            template<typename T>
            NkSystemDesc& Excludes() noexcept {
                excludeMask.Set(NkIdOf<std::remove_const_t<T>>());
                return *this;
            }

            // Déclare une dépendance : ce système doit s'exécuter après `OtherSystem`.
            template<typename OtherSystem>
            NkSystemDesc& After() noexcept {
                afterTypes.push_back(std::type_index(typeid(OtherSystem)));
                return *this;
            }

            // Définit le groupe d'exécution.
            NkSystemDesc& InGroup(NkSystemGroup g) noexcept {
                group = g;
                return *this;
            }

            // Définit la priorité (affecte l'ordre au sein du groupe).
            NkSystemDesc& WithPriority(float32 p) noexcept {
                priority = p;
                return *this;
            }

            // Force l'exécution séquentielle (pas de parallélisation).
            NkSystemDesc& Sequential() noexcept {
                runInParallel = false;
                return *this;
            }

            // Donne un nom au système (pour les logs et le profilage).
            NkSystemDesc& Named(const char* n) noexcept {
                name = n;
                return *this;
            }

            // -----------------------------------------------------------------
            // Détection de conflit avec un autre descripteur
            // -----------------------------------------------------------------
            // Deux systèmes sont en conflit si l'un écrit un composant que
            // l'autre lit ou écrit. Ils ne peuvent alors pas s'exécuter en
            // parallèle et doivent être ordonnés.
            [[nodiscard]] bool ConflictsWith(const NkSystemDesc& other) const noexcept {
                for (uint32 w = 0; w < NkComponentMask::kWords; ++w) {
                    // write vs read
                    if (writeMask.words[w] & other.readMask.words[w]) {
                        return true;
                    }
                    // write vs write
                    if (writeMask.words[w] & other.writeMask.words[w]) {
                        return true;
                    }
                    // read vs write
                    if (readMask.words[w] & other.writeMask.words[w]) {
                        return true;
                    }
                }
                return false;
            }
        };

        // =====================================================================
        // NkSystem
        // =====================================================================
        // Classe de base abstraite pour tous les systèmes.
        // Un système doit implémenter `Describe()` et `Execute()`.
        class NkSystem {
          public:
            virtual ~NkSystem() noexcept = default;

            // Retourne le descripteur déclarant les dépendances du système.
            // Appelé une seule fois lors de l'enregistrement dans le scheduler.
            [[nodiscard]] virtual NkSystemDesc Describe() const = 0;

            // Fonction principale appelée à chaque frame par le scheduler.
            // Reçoit une référence au monde et le temps écoulé depuis la dernière frame.
            virtual void Execute(NkWorld& world, float32 dt) = 0;

            // --- Hooks optionnels (surchargeables) ---
            // Appelé lorsque le système est ajouté à un monde.
            virtual void OnCreate(NkWorld& /*world*/) noexcept {}

            // Appelé lorsque le système est retiré du monde.
            virtual void OnDestroy(NkWorld& /*world*/) noexcept {}

            // Appelé lorsque le système est activé.
            virtual void OnEnable() noexcept {}

            // Appelé lorsque le système est désactivé.
            virtual void OnDisable() noexcept {}

            // --- Gestion de l'état actif/inactif ---
            [[nodiscard]] bool IsEnabled() const noexcept {
                return mEnabled;
            }

            void SetEnabled(bool enabled) noexcept {
                mEnabled = enabled;
            }

          protected:
            bool mEnabled = true;   // Le système est-il actif ?
        };

        // ---------------------------------------------------------------------
        // Macro NK_SYSTEM
        // ---------------------------------------------------------------------
        // Optionnelle, peut être placée après la définition d'un système pour
        // améliorer la lisibilité. Aucun effet sur le code.
        #define NK_SYSTEM(Type) /* no-op */

        // =====================================================================
        // NkLambdaSystem
        // =====================================================================
        // Implémentation pratique d'un système à partir d'une lambda.
        // Utile pour le prototypage rapide ou les petits systèmes.
        class NkLambdaSystem final : public NkSystem {
          public:
            // Type de la fonction exécutée.
            using ExecFn = std::function<void(NkWorld&, float32)>;

            // Constructeur prenant un descripteur et une fonction.
            NkLambdaSystem(NkSystemDesc desc, ExecFn fn) noexcept
                : mDesc(std::move(desc))
                , mFn(std::move(fn)) {}

            // Implémentation de l'interface NkSystem.
            NkSystemDesc Describe() const override {
                return mDesc;
            }

            void Execute(NkWorld& world, float32 dt) override {
                if (mFn) {
                    mFn(world, dt);
                }
            }

          private:
            NkSystemDesc mDesc;   // Descripteur du système
            ExecFn       mFn;     // Fonction à exécuter
        };

    } // namespace ecs
} // namespace nkentseu

// =============================================================================
// EXEMPLES D'UTILISATION DE NKSYSTEM.H
// =============================================================================
//
// Les systèmes encapsulent la logique de votre jeu. Voici comment les définir
// et les utiliser avec le scheduler (présenté dans NkScheduler.h).
//
// -----------------------------------------------------------------------------
// Exemple 1 : Définition d'un système simple
// -----------------------------------------------------------------------------
/*
    #include "NkECS/System/NkSystem.h"
    #include "NkECS/World/NkWorld.h"

    // Composants utilisés.
    struct Transform { float x, y, z; };
    struct Velocity  { float vx, vy, vz; };
    NK_COMPONENT(Transform);
    NK_COMPONENT(Velocity);

    // Système de déplacement.
    class MovementSystem : public nkentseu::ecs::NkSystem {
    public:
        nkentseu::ecs::NkSystemDesc Describe() const override {
            return nkentseu::ecs::NkSystemDesc{}
                .Reads<Transform>()           // Lecture de Transform
                .Writes<Velocity>()           // Écriture de Velocity (et lecture implicite)
                .InGroup(nkentseu::ecs::NkSystemGroup::Update)
                .Named("MovementSystem");
        }

        void Execute(nkentseu::ecs::NkWorld& world, float32 dt) override {
            // Itération sur toutes les entités avec Transform et Velocity.
            world.Query<Transform, Velocity>().ForEach(
                [dt](nkentseu::ecs::NkEntityId, Transform& t, const Velocity& v) {
                    t.x += v.vx * dt;
                    t.y += v.vy * dt;
                    t.z += v.vz * dt;
                });
        }
    };
    NK_SYSTEM(MovementSystem); // Optionnel
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Système avec dépendance explicite (After)
// -----------------------------------------------------------------------------
/*
    #include "NkECS/System/NkSystem.h"

    // Système de gestion des entrées (doit s'exécuter avant le mouvement).
    class InputSystem : public nkentseu::ecs::NkSystem {
    public:
        nkentseu::ecs::NkSystemDesc Describe() const override {
            return nkentseu::ecs::NkSystemDesc{}
                .Writes<Velocity>()            // Modifie la vélocité selon l'input
                .InGroup(nkentseu::ecs::NkSystemGroup::PreUpdate)
                .Named("InputSystem");
        }

        void Execute(nkentseu::ecs::NkWorld& world, float32 dt) override {
            // Lit les entrées clavier/souris et met à jour Velocity...
        }
    };

    // MovementSystem déclare qu'il doit s'exécuter après InputSystem.
    class MovementSystem : public nkentseu::ecs::NkSystem {
    public:
        nkentseu::ecs::NkSystemDesc Describe() const override {
            return nkentseu::ecs::NkSystemDesc{}
                .Reads<Transform>()
                .Writes<Velocity>()
                .After<InputSystem>()          // Garantit l'ordre InputSystem → MovementSystem
                .InGroup(nkentseu::ecs::NkSystemGroup::Update)
                .Named("MovementSystem");
        }

        void Execute(nkentseu::ecs::NkWorld& world, float32 dt) override {
            // ...
        }
    };
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Utilisation de NkLambdaSystem pour prototyper
// -----------------------------------------------------------------------------
/*
    #include "NkECS/System/NkSystem.h"
    #include "NkECS/World/NkWorld.h"

    void ExampleLambdaSystem() {
        nkentseu::ecs::NkWorld world;

        // Création d'un système à la volée avec une lambda.
        auto desc = nkentseu::ecs::NkSystemDesc{}
            .Writes<Transform>()
            .InGroup(nkentseu::ecs::NkSystemGroup::Update)
            .Named("LambdaGravity");

        auto gravitySystem = std::make_unique<nkentseu::ecs::NkLambdaSystem>(
            desc,
            [](nkentseu::ecs::NkWorld& w, float32 dt) {
                w.Query<Transform>().ForEach([dt](nkentseu::ecs::NkEntityId, Transform& t) {
                    t.y -= 9.81f * dt; // gravité simple
                });
            });

        // Ajout au scheduler (cf. NkScheduler.h)...
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Système avec exclusion (Without implicite)
// -----------------------------------------------------------------------------
/*
    #include "NkECS/System/NkSystem.h"

    struct Dead {}; // tag
    NK_COMPONENT(Dead);

    class HealthSystem : public nkentseu::ecs::NkSystem {
    public:
        nkentseu::ecs::NkSystemDesc Describe() const override {
            return nkentseu::ecs::NkSystemDesc{}
                .Writes<Health>()
                .Excludes<Dead>()              // Les entités mortes sont ignorées
                .Named("HealthSystem");
        }

        void Execute(nkentseu::ecs::NkWorld& world, float32 dt) override {
            world.Query<Health>()
                .Without<Dead>()               // Double sécurité
                .ForEach([](nkentseu::ecs::NkEntityId id, Health& h) {
                    if (h.value <= 0) {
                        // Marquer comme mort...
                    }
                });
        }
    };
*/

// =============================================================================