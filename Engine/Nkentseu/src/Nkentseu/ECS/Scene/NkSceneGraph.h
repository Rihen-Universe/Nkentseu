#pragma once
// =============================================================================
// Scene/NkSceneGraph.h — v3 (SceneScript + lifecycle complet)
// =============================================================================
/**
 * @file NkSceneGraph.h
 * @brief Définition de NkSceneGraph : wrapper de NkWorld avec hiérarchie et lifecycle.
 * 
 * Ce fichier expose l'API publique pour :
 * - Gérer la hiérarchie de scène (parent/enfants, transforms locaux/mondiaux)
 * - Spawner, rechercher et détruire des acteurs avec sémantique "scène"
 * - Attacher un script de scène unique (NkSceneScript) avec lifecycle complet
 * - Contrôler l'activation, la visibilité et la pause de la scène
 * - Coordonner les mises à jour via NkSceneLifecycleSystem
 * 
 * 🔹 ARCHITECTURE CLÉ :
 *    • NkSceneGraph n'est PAS un remplacement de NkWorld — c'est une vue enrichie
 *    • Pour les queries ECS en masse : utiliser NkWorld::Query() directement
 *    • Pour les opérations "scène" (spawn, find, hierarchy) : utiliser NkSceneGraph
 * 
 * 🔹 LIFECYCLE — QUI APPELLE QUOI ?
 *    • BeginPlay() : appelé par NkSceneManager::LoadScene() après chargement
 *    • Tick/LateTick/FixedTick() : appelés automatiquement par NkSceneLifecycleSystem
 *    • EndPlay() : appelé par le destructeur ou NkSceneManager::UnloadScene()
 *    • L'utilisateur NE doit JAMAIS appeler ces méthodes directement
 * 
 * 🔹 SCRIPT DE SCÈNE :
 *    • 0 ou 1 script par scène — jamais plus (pattern Singleton par scène)
 *    • Le script reçoit les callbacks du lifecycle (OnBeginPlay, OnTick, etc.)
 *    • Remplacer un script appelle OnEndPlay() sur l'ancien avant création du nouveau
 * 
 * 🔹 Thread-safety : Non thread-safe. Synchronisation externe requise
 *    pour les accès concurrents au monde sous-jacent.
 * 
 * @author nkentseu
 * @version 3.0
 * @date 2026
 */

#include "NKECS/NkECSDefines.h"
#include "Nkentseu/ECS/Factory/NkGameObjectFactory.h"
#include "NKECS/World/NkWorld.h"
#include "NkSceneScript.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include <utility>
#include <type_traits>

namespace nkentseu {
    namespace ecs {

        // =====================================================================
        // 🧱 Composants built-in de la hiérarchie de scène
        // =====================================================================

        /**
         * @struct NkParent
         * @brief Composant stockant la référence vers l'entité parente.
         * 
         * 🔹 Usage :
         *    • NkEntityId::Invalid() signifie que l'entité est une racine de scène
         *    • Utilisé par NkSceneGraph::SetParent() pour gérer la hiérarchie
         *    • Lu par NkTransformSystem pour calculer les matrices mondiales
         * 
         * @note Composant singleton : une entité ne peut avoir qu'un seul parent.
         *       La modification de ce composant marque automatiquement
         *       NkWorldTransform::dirty = true pour recalcul hiérarchique.
         */
        struct NkParent {
            /// ID de l'entité parente (Invalid = racine de scène)
            NkEntityId entity = NkEntityId::Invalid();
        };
        NK_COMPONENT(NkParent)

        /**
         * @struct NkChildren
         * @brief Composant stockant la liste des entités enfants directs.
         * 
         * 🔹 Caractéristiques :
         *    • Tableau fixe inline de kMax (64) éléments — pas d'allocation heap
         *    • Ajout/suppression en O(1) amorti via swap-with-last
         *    • Recherche linéaire O(n) — acceptable car n ≤ 64 typiquement
         * 
         * 🔹 Pourquoi pas de NkVector ici ?
         *    • Les enfants sont fréquemment itérés dans les boucles chaudes
         *    • Un tableau contigu inline offre une meilleure localité cache
         *    • La limite de 64 enfants est suffisante pour 99% des cas d'usage
         * 
         * @warning Si vous avez besoin de plus de 64 enfants, envisagez de
         *          restructurer votre hiérarchie avec des groupes intermédiaires.
         * 
         * @note L'ordre des enfants n'est pas préservé après suppression
         *       (swap-with-last). Si l'ordre est critique, utiliser une
         *       structure externe ou un composant personnalisé.
         */
        struct NkChildren {
            /// Nombre maximum d'enfants stockables inline
            static constexpr uint32 kMax = 64u;

            /// Tableau inline des IDs d'enfants
            NkEntityId children[kMax] = {};

            /// Nombre actuel d'enfants valides dans le tableau
            uint32 count = 0;

            /**
             * @brief Ajoute un enfant à la liste.
             * @param c ID de l'entité à ajouter comme enfant.
             * @return true si l'ajout a réussi, false si la liste est pleine.
             * 
             * @note Ajout en fin de tableau — O(1).
             *       Aucune vérification de doublon — à gérer en amont si nécessaire.
             */
            bool Add(NkEntityId c) noexcept {
                if (count >= kMax) {
                    return false;
                }
                children[count++] = c;
                return true;
            }

            /**
             * @brief Supprime un enfant de la liste.
             * @param c ID de l'entité à retirer.
             * 
             * 🔹 Algorithme :
             *    • Recherche linéaire de l'ID
             *    • Remplacement par le dernier élément (swap-with-last)
             *    • Décrémentation de count — O(1) après recherche
             * 
             * @note L'ordre des enfants n'est pas préservé après suppression.
             */
            void Remove(NkEntityId c) noexcept {
                for (uint32 i = 0; i < count; ++i) {
                    if (children[i] == c) {
                        children[i] = children[--count];
                        return;
                    }
                }
            }

            /**
             * @brief Vérifie si une entité est dans la liste des enfants.
             * @param c ID de l'entité à rechercher.
             * @return true si l'entité est présente, false sinon.
             * 
             * @note Recherche linéaire O(n) — acceptable pour n ≤ 64.
             */
            bool Has(NkEntityId c) const noexcept {
                for (uint32 i = 0; i < count; ++i) {
                    if (children[i] == c) {
                        return true;
                    }
                }
                return false;
            }
        };
        NK_COMPONENT(NkChildren)

        /**
         * @struct NkSceneNode
         * @brief Métadonnées de haut niveau pour un nœud de scène.
         * 
         * 🔹 Champs :
         *    • name : nom lisible pour débogage et recherche (64 chars max)
         *    • active : flag logique — les entités inactives sont ignorées
         *               par les systèmes de mise à jour gameplay
         *    • visible : flag de rendu — les entités invisibles sont skipées
         *                par le renderer mais restent actives gameplay
         *    • layer : identifiant de couche (0-255) pour le culling/rendu
         * 
         * 🔹 Séparation active/visible :
         *    • active=false → l'entité ne reçoit pas d'Update(), pas de collision
         *    • visible=false → l'entité n'est pas dessinée, mais logique OK
         *    • Permet de désactiver le rendu sans stopper la logique (ex: occlusion)
         * 
         * @note Le nom est copié via NkStrNCpy — tronqué silencieusement à 63 chars.
         *       Pour des noms plus longs, utiliser un composant personnalisé.
         */
        struct NkSceneNode {
            /// Nom lisible du nœud (buffer fixe, null-terminated)
            char name[64] = {};

            /// Flag d'activation logique (gameplay)
            bool active = true;

            /// Flag de visibilité (rendu uniquement)
            bool visible = true;

            /// Identifiant de layer pour culling/grouping (0-255)
            uint8 layer = 0;

            /// Constructeur par défaut
            NkSceneNode() noexcept = default;

            /**
             * @brief Constructeur avec initialisation du nom.
             * @param n String C source pour le nom (copiée, tronquée si >63).
             */
            explicit NkSceneNode(const char* n) noexcept {
                NkStrNCpy(name, n, 63);
            }
        };
        NK_COMPONENT(NkSceneNode)

        /**
         * @struct NkLocalTransform
         * @brief Transformation locale relative au parent (POD, SIMD-friendly).
         * 
         * 🔹 Représentation :
         *    • position[3] : translation en unités monde (x, y, z)
         *    • rotation[4] : quaternion (x, y, z, w) — normalisé attendu
         *    • scale[3] : facteur d'échelle par axe (1,1,1 = identité)
         * 
         * 🔹 Pourquoi des tableaux C et pas des structs NkVec3/NkQuat ?
         *    • Layout mémoire garanti pour interop avec shaders/SIMD
         *    • Pas de padding imprévu — essentiel pour le streaming GPU
         *    • Accès indexé direct pour les boucles vectorisées
         * 
         * @warning Le quaternion doit être normalisé pour des rotations valides.
         *          Utiliser les helpers NkQuat::Normalize() si nécessaire.
         * 
         * @note Taille totale : 40 bytes — tient dans un cache line.
         */
        struct NkLocalTransform {
            /// Position locale (x, y, z)
            float32 position[3] = {0, 0, 0};

            /// Rotation en quaternion (x, y, z, w)
            float32 rotation[4] = {0, 0, 0, 1};

            /// Échelle locale par axe (x, y, z)
            float32 scale[3] = {1, 1, 1};
        };
        NK_COMPONENT(NkLocalTransform)

        /**
         * @struct NkWorldTransform
         * @brief Matrice de transformation mondiale calculée (4x4, row-major).
         * 
         * 🔹 Usage :
         *    • Remplie automatiquement par NkTransformSystem::Update()
         *    • Utilisée par le renderer pour le world-space des meshes
         *    • Ne pas modifier manuellement — recalculée à chaque frame
         * 
         * 🔹 Layout mémoire :
         *    • 16 floats en row-major order (compatible OpenGL/DirectX par défaut)
         *    • Index : [row*4 + col] → m[0]=m00, m[3]=m03, m[12]=m30, etc.
         * 
         * 🔹 Flag dirty :
         *    • true = la matrice doit être recalculée (parent changé, local modifié)
         *    • false = la matrice est à jour — peut être réutilisée telle quelle
         * 
         * @warning Ne pas lire cette matrice avant le premier Update() du système.
         *          Initialisée à l'identité, mais peut être obsolète.
         * 
         * @note Taille : 64 bytes (16*4) + 1 byte bool + padding = 68 bytes.
         */
        struct NkWorldTransform {
            /// Matrice 4x4 row-major pour transformation mondiale
            float32 matrix[16] = {
                1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0,
                0, 0, 0, 1
            };

            /// Flag indiquant si la matrice doit être recalculée
            bool dirty = true;
        };
        NK_COMPONENT(NkWorldTransform)

        // =====================================================================
        // 🌍 NkSceneGraph — Wrapper de NkWorld avec hiérarchie et lifecycle
        // =====================================================================
        /**
         * @class NkSceneGraph
         * @brief Orchestrateur de scène ajoutant hiérarchie, scripting et lifecycle.
         * 
         * 🔹 Responsabilités :
         *    • Gestion de la hiérarchie parent/enfants via NkParent/NkChildren
         *    • Spawning de nœuds/acteurs avec composants de scène pré-configurés
         *    • Recherche d'entités par nom ou par layer
         *    • Contrôle unifié de l'activation/visibilité
         *    • Destruction récursive sécurisée des sous-arborescences
         *    • Gestion d'un script de scène unique avec lifecycle complet
         *    • Intégration avec NkSceneLifecycleSystem pour mises à jour auto
         * 
         * 🔹 Ce que NkSceneGraph NE FAIT PAS :
         *    • Il ne réimplémente PAS l'ECS — tout est délégué à NkWorld
         *    • Il ne gère PAS le rendu — c'est le rôle de renderer::NkRenderScene
         *    • Il ne calcule PAS les matrices — c'est le rôle de NkTransformSystem
         *    • Il ne gère PAS plusieurs scripts — pattern "1 script par scène"
         * 
         * 🔹 Lifecycle — Ordre d'appel typique :
         * @code
         * // Chargement de scène :
         * NkSceneManager::LoadScene() {
         *     // 1. Spawn des acteurs via SceneGraph
         *     // 2. Configuration de la hiérarchie
         *     // 3. APPEL : scene.BeginPlay() ← point d'entrée du script
         * }
         * 
         * // Boucle principale (via NkSceneLifecycleSystem) :
         * void GameLoop::Update(float dt) {
         *     scene.FixedTick(fixedDt);  // Physique, pas affecté par le framerate
         *     scene.Tick(dt);            // Logique gameplay principale
         *     // ... autres systèmes ECS ...
         *     scene.LateTick(dt);        // Post-traitement, caméras, UI
         * }
         * 
         * // Déchargement :
         * NkSceneManager::UnloadScene() {
         *     scene.EndPlay();  // Nettoyage du script
         *     // Destruction des entités...
         * }
         * @endcode
         * 
         * @warning NkSceneGraph est un wrapper léger — sa durée de vie doit être
         *          inférieure ou égale à celle du NkWorld référencé.
         * 
         * @example
         * @code
         * NkWorld world;
         * NkSceneGraph scene(world, "MainLevel");
         * 
         * // Attacher un script de scène personnalisé
         * auto* script = scene.SetScript<MyLevelScript>(/*params=*/);
         * 
         * // Spawner un acteur avec position initiale
         * auto player = scene.SpawnActor<Player>("Hero", 10.f, 0.f, 5.f);
         * 
         * // Construire la hiérarchie
         * auto sword = scene.SpawnNode("Weapon_Slot");
         * scene.SetParent(sword, player.Id());  // sword suit player
         * 
         * // Le lifecycle est géré automatiquement par NkSceneLifecycleSystem
         * // via l'enregistrement dans NkScheduler — pas d'appel manuel requis
         * @endcode
         */
        class NkSceneGraph {
            public:
                /**
                 * @brief Constructeur principal.
                 * @param world Référence au monde ECS à orchestrer.
                 * @param name Nom lisible de la scène (pour débogage/logs).
                 * 
                 * @note Le NkSceneGraph ne possède pas le monde — il le référence.
                 *       Assurez-vous que 'world' reste valide pendant toute la
                 *       durée de vie de cette instance.
                 * 
                 * @warning Le destructeur appelle EndPlay() — ne pas détruire
                 *          une scène pendant que son lifecycle est en cours.
                 */
                explicit NkSceneGraph(NkWorld& world,
                                      const NkString& name = "Scene") noexcept;

                /**
                 * @brief Destructeur : appelle EndPlay() pour nettoyage du script.
                 * @note Garantit que OnEndPlay() est toujours appelé, même en cas
                 *       d'exception ou de destruction prématurée.
                 */
                ~NkSceneGraph() noexcept;

                // Suppression des copies (référence interne non copiable)
                NkSceneGraph(const NkSceneGraph&) = delete;
                NkSceneGraph& operator=(const NkSceneGraph&) = delete;

                // ── Identité et état de la scène ────────────────────────────
                /**
                 * @brief Retourne le nom lisible de la scène.
                 * @return Pointeur vers string C (jamais nullptr).
                 */
                [[nodiscard]] const char* Name() const noexcept;

                /**
                 * @brief Vérifie si la scène est actuellement en pause.
                 * @return true si Pause() a été appelé sans Resume() depuis.
                 * 
                 * @note En pause : Tick/LateTick/FixedTick sont skipés,
                 *       mais BeginPlay/EndPlay peuvent toujours être appelés.
                 */
                [[nodiscard]] bool IsPaused() const noexcept;

                /**
                 * @brief Retourne une référence mutable au monde ECS orchestré.
                 * @return Référence à NkWorld.
                 * 
                 * @note Permet d'accéder à l'API ECS bas-niveau quand nécessaire.
                 *       À utiliser avec précaution pour ne pas contourner
                 *       les invariants maintenus par NkSceneGraph.
                 */
                [[nodiscard]] NkWorld& World() noexcept;

                /**
                 * @brief Version const de World().
                 */
                [[nodiscard]] const NkWorld& World() const noexcept;

                // ── Script de scène (0 ou 1) ────────────────────────────────
                /**
                 * @brief Crée et attache un script de scène de type T.
                 * @tparam T Type du script (doit dériver de NkSceneScript).
                 * @tparam Args Types des arguments pour le constructeur de T.
                 * @param args Arguments forwardés au constructeur de T.
                 * @return Pointeur vers le script nouvellement créé (casté en T*).
                 * 
                 * 🔹 Comportement :
                 *    • Si un script existait déjà : appelle OnEndPlay() sur l'ancien
                 *    • Crée une nouvelle instance de T via new T(args...)
                 *    • Appelle SetContext(&mWorld, this) pour initialisation
                 *    • Retourne un pointeur typé pour configuration immédiate
                 * 
                 * @warning La gestion mémoire est manuelle (new/delete) — le script
                 *          est détruit par ClearScript() ou le destructeur de
                 *          NkSceneGraph. Ne pas delete manuellement le pointeur retourné.
                 * 
                 * @example
                 * @code
                 * class MyLevelScript : public NkSceneScript {
                 * public:
                 *     MyLevelScript(int difficulty) : mDifficulty(difficulty) {}
                 *     void OnBeginPlay() override { SpawnEnemies(mDifficulty); }
                 * private:
                 *     int mDifficulty;
                 * };
                 * 
                 * auto* level = scene.SetScript<MyLevelScript>(/*difficulty=*/3);
                 * @endcode
                 */
                template<typename T, typename... Args>
                T* SetScript(Args&&... args) noexcept;

                /**
                 * @brief Retire le script actuel (appelle OnEndPlay() d'abord).
                 * 
                 * 🔹 Effets :
                 *    • Appelle mScript->OnEndPlay() si un script existe
                 *    • Delete l'instance et met mScript à nullptr
                 *    • Les futurs appels à GetScript() retourneront nullptr
                 * 
                 * @note Utile pour :
                 *    • Changer de script en cours de jeu (rare)
                 *    • Libérer explicitement les ressources du script
                 *    • Tester des scénarios sans script dans les tests unitaires
                 */
                void ClearScript() noexcept;

                /**
                 * @brief Accès typé au script courant.
                 * @tparam T Type attendu du script.
                 * @return Pointeur vers T si le script existe et correspond, nullptr sinon.
                 * 
                 * @note Effectue un static_cast — aucun runtime check de type.
                 *       Si le type ne correspond pas, le comportement est indéfini.
                 *       Pour plus de sécurité, vérifier HasScript() d'abord ou
                 *       utiliser un système de RTTI custom si nécessaire.
                 * 
                 * @example
                 * @code
                 * if (auto* level = scene.GetScript<MyLevelScript>()) {
                 *     level->TriggerEvent("BossSpawn");
                 * }
                 * @endcode
                 */
                template<typename T>
                [[nodiscard]] T* GetScript() noexcept;

                /**
                 * @brief Version const de GetScript().
                 */
                template<typename T>
                [[nodiscard]] const T* GetScript() const noexcept;

                /**
                 * @brief Vérifie si un script est actuellement attaché.
                 * @return true si mScript != nullptr.
                 * 
                 * @note Ne vérifie PAS le type — juste la présence d'un script.
                 *       Utiliser GetScript<T>() pour vérifier le type spécifique.
                 */
                [[nodiscard]] bool HasScript() const noexcept;

                // ── Lifecycle (appelé par NkSceneLifecycleSystem) ───────────
                /**
                 * @brief Point d'entrée du lifecycle — appelé une fois au chargement.
                 * 
                 * 🔹 Quand est-ce appelé ?
                 *    • Par NkSceneManager::LoadScene() après que tous les
                 *      acteurs sont spawnés et la hiérarchie établie
                 *    • Jamais plus d'une fois par instance de scène
                 * 
                 * 🔹 Effets :
                 *    • Met mStarted = true (empêche les appels multiples)
                 *    • Appelle mScript->OnBeginPlay() si un script existe
                 * 
                 * @warning ⚠️ L'utilisateur NE doit PAS appeler cette méthode directement.
                 *          Elle est publique uniquement pour NkSceneLifecycleSystem.
                 *          Un appel manuel peut causer des incohérences de state.
                 */
                void BeginPlay() noexcept;

                /**
                 * @brief Mise à jour principale — appelée chaque frame variable.
                 * @param dt Delta-time en secondes depuis la frame précédente.
                 * 
                 * 🔹 Quand est-ce appelé ?
                 *    • Par NkSceneLifecycleSystem::Update() dans la boucle principale
                 *    • Fréquence variable : dépend du framerate du jeu
                 * 
                 * 🔹 Effets :
                 *    • Skip si !mStarted || mPaused
                 *    • Appelle mScript->OnTick(dt) si un script existe
                 * 
                 * @note Utiliser pour :
                 *    • Logique gameplay dépendante du temps (mouvements, timers)
                 *    • Input handling, AI updates, game state transitions
                 * 
                 * @warning ⚠️ Ne pas appeler directement — laisser le système gérer.
                 */
                void Tick(float dt) noexcept;

                /**
                 * @brief Mise à jour tardive — appelée après Tick(), avant le rendu.
                 * @param dt Delta-time en secondes.
                 * 
                 * 🔹 Quand est-ce appelé ?
                 *    • Par NkSceneLifecycleSystem::LateUpdate() après tous les Tick()
                 * 
                 * 🔹 Effets :
                 *    • Skip si !mStarted || mPaused
                 *    • Appelle mScript->OnLateTick(dt) si un script existe
                 * 
                 * @note Utiliser pour :
                 *    • Mise à jour des caméras (après que les joueurs ont bougé)
                 *    • Calculs d'UI dépendants des positions finales des objets
                 *    • Post-traitement des données de Tick()
                 * 
                 * @warning ⚠️ Ne pas appeler directement.
                 */
                void LateTick(float dt) noexcept;

                /**
                 * @brief Mise à jour fixe — appelée à intervalle régulier pour la physique.
                 * @param fixedDt Delta-time fixe (ex: 1/60s) indépendant du framerate.
                 * 
                 * 🔹 Quand est-ce appelé ?
                 *    • Par NkSceneLifecycleSystem::FixedUpdate() à fréquence constante
                 *    • Peut être appelé 0, 1 ou plusieurs fois par frame selon le lag
                 * 
                 * 🔹 Effets :
                 *    • Skip si !mStarted || mPaused
                 *    • Appelle mScript->OnFixedTick(fixedDt) si un script existe
                 * 
                 * @note Utiliser pour :
                 *    • Physique déterministe (collisions, forces, ragdolls)
                 *    • Logique réseau tick-based pour la synchronisation
                 *    • Tout calcul qui doit être frame-independent
                 * 
                 * @warning ⚠️ Ne pas appeler directement.
                 *          Le paramètre fixedDt est constant — ne pas le multiplier
                 *          par un facteur variable sous peine de casser la physique.
                 */
                void FixedTick(float fixedDt) noexcept;

                /**
                 * @brief Point de sortie du lifecycle — appelé au déchargement.
                 * 
                 * 🔹 Quand est-ce appelé ?
                 *    • Par NkSceneManager::UnloadScene() avant destruction
                 *    • Par le destructeur ~NkSceneGraph() en fallback
                 * 
                 * 🔹 Effets :
                 *    • Skip si !mStarted (déjà nettoyé ou jamais commencé)
                 *    • Appelle mScript->OnEndPlay() si un script existe
                 *    • Met mStarted = false (empêche les appels ultérieurs)
                 * 
                 * @note OnEndPlay() est l'endroit pour :
                 *    • Libérer des ressources acquises dans OnBeginPlay()
                 *    • Sauvegarder l'état de la scène
                 *    • Notifier d'autres systèmes de la fin de la scène
                 * 
                 * @warning ⚠️ Ne pas appeler directement.
                 */
                void EndPlay() noexcept;

                /**
                 * @brief Met la scène en pause — skip Tick/LateTick/FixedTick.
                 * 
                 * 🔹 Effets :
                 *    • Met mPaused = true (empêche les mises à jour)
                 *    • Appelle mScript->OnPause() si un script existe
                 * 
                 * @note La pause n'affecte PAS :
                 *    • Le rendu (la scène reste visible sauf si SetVisible(false))
                 *    • Les événements ECS (queries, events peuvent toujours être émis)
                 *    • BeginPlay/EndPlay (peuvent toujours être appelés)
                 * 
                 * @warning ⚠️ Ne pas appeler directement — utiliser via un système
                 *          de gestion de pause global pour cohérence.
                 */
                void Pause() noexcept;

                /**
                 * @brief Reprend la scène après une pause.
                 * 
                 * 🔹 Effets :
                 *    • Met mPaused = false (réactive les mises à jour)
                 *    • Appelle mScript->OnResume() si un script existe
                 * 
                 * @note OnResume() est l'endroit pour :
                 *    • Reprendre les timers mis en pause
                 *    • Re-synchroniser les états après une pause longue
                 *    • Rejouer des effets audio/visuels interrompus
                 * 
                 * @warning ⚠️ Ne pas appeler directement.
                 */
                void Resume() noexcept;

                // ── Spawn : création de nœuds et acteurs ────────────────────
                /**
                 * @brief Crée un nœud de scène avec composants de base.
                 * @param name Nom lisible du nœud (copié dans NkSceneNode).
                 * @return ID de la nouvelle entité créée.
                 * 
                 * 🔹 Composants automatiquement ajoutés :
                 *    • NkSceneNode : métadonnées (nom, active, visible, layer)
                 *    • NkParent : référence parent (initialisée à Invalid)
                 *    • NkChildren : liste d'enfants (vide au départ)
                 *    • NkLocalTransform : transformation locale (identité)
                 *    • NkWorldTransform : matrice mondiale (identité, dirty=true)
                 * 
                 * @note Pour un GameObject avec logique, préférer SpawnActor<T>().
                 *       SpawnNode() est pour les objets purement hiérarchiques
                 *       (groupes, pivots, objets de scène sans comportement).
                 */
                [[nodiscard]] NkEntityId SpawnNode(const char* name) noexcept;

                /**
                 * @brief Crée un acteur typé (GameObject ou dérivé) avec scène.
                 * @tparam T Type de l'acteur (doit être NkGameObject ou dérivé).
                 * @tparam Args Types des arguments pour le constructeur de T.
                 * @param name Nom lisible de l'acteur.
                 * @param x Position X locale initiale.
                 * @param y Position Y locale initiale.
                 * @param z Position Z locale initiale.
                 * @param args Arguments forwardés au constructeur de T.
                 * @return Instance de T liée à la nouvelle entité.
                 * 
                 * 🔹 Étapes :
                 *    1. Crée un GameObject via NkGameObjectFactory::Create<T>()
                 *    2. Ajoute les composants de scène manquants si nécessaire
                 *    3. Applique la position locale via actor.SetPosition(x,y,z)
                 *    4. Retourne l'instance typée pour configuration immédiate
                 * 
                 * @example
                 * @code
                 * auto enemy = scene.SpawnActor<EnemyAI>(
                 *     "Orc_01",
                 *     15.f, 0.f, 3.f,  // position initiale
                 *     /*difficulty=*/2  // argument pour le constructeur EnemyAI
                 * );
                 * enemy.SetAggression(0.8f);  // Configuration post-spawn
                 * @endcode
                 */
                template<typename T = NkGameObject, typename... Args>
                [[nodiscard]] T SpawnActor(const char* name,
                                           float32 x = 0, float32 y = 0, float32 z = 0,
                                           Args&&... args) noexcept;

                // ── Hiérarchie : gestion parent/enfants ─────────────────────
                /**
                 * @brief Attache une entité enfant à un parent dans la hiérarchie.
                 * @param child ID de l'entité à attacher comme enfant.
                 * @param parent ID de l'entité parente (Invalid = détacher).
                 * 
                 * 🔹 Effets secondaires :
                 *    • Met à jour NkParent de l'enfant
                 *    • Retire l'enfant de l'ancien parent si nécessaire
                 *    • Ajoute l'enfant à la liste NkChildren du nouveau parent
                 *    • Marque NkWorldTransform de l'enfant comme dirty
                 * 
                 * @note La propagation des transforms (recalcul des matrices mondiales)
                 *       est effectuée par NkTransformSystem, pas ici.
                 * 
                 * @warning Ne pas appeler pendant l'itération sur une query hiérarchique.
                 *          Utiliser des opérations différées si nécessaire.
                 */
                void SetParent(NkEntityId child, NkEntityId parent) noexcept;

                /**
                 * @brief Détache une entité de son parent (devient racine).
                 * @param child ID de l'entité à détacher.
                 * 
                 * @note Équivalent à SetParent(child, NkEntityId::Invalid()).
                 *       Fourni pour une API plus expressive.
                 */
                void DetachFromParent(NkEntityId child) noexcept;

                // ── Recherche : trouver des entités dans la scène ───────────
                /**
                 * @brief Trouve la première entité dont le nom correspond exactement.
                 * @param name Nom à rechercher (comparaison case-sensitive).
                 * @return ID de l'entité trouvée, ou Invalid si non trouvée.
                 * 
                 * 🔹 Complexité : O(n) sur toutes les entités avec NkSceneNode.
                 *          À éviter dans les boucles chaudes — préférer un cache.
                 * 
                 * @note Si plusieurs entités ont le même nom, retourne la première
                 *       trouvée (ordre non garanti — dépend de l'itération ECS).
                 *       Pour des noms uniques, envisager une contrainte au niveau métier.
                 * 
                 * @example
                 * @code
                 * NkEntityId door = scene.FindByName("Castle_MainDoor");
                 * if (door.IsValid()) {
                 *     scene.SetActive(door, false);  // Fermer la porte
                 * }
                 * @endcode
                 */
                [[nodiscard]] NkEntityId FindByName(const char* name) const noexcept;

                /**
                 * @brief Collecte toutes les entités d'un layer donné.
                 * @param layer Identifiant du layer à filtrer (0-255).
                 * @param out Vecteur de sortie à remplir (ajouté à la fin).
                 * 
                 * 🔹 Usage typique :
                 *    • Culling par layer avant rendu ("UI", "Effects", "Occluders")
                 *    • Mise à jour sélective de sous-systèmes
                 *    • Debug : afficher uniquement certains layers
                 * 
                 * @note Les résultats sont ajoutés à la fin du vecteur existant.
                 *       Utiliser out.Clear() avant appel si nécessaire.
                 */
                void FindByLayer(uint8 layer, NkVector<NkEntityId>& out) const noexcept;

                // ── Activation / Visibilité ─────────────────────────────────
                /**
                 * @brief Active ou désactive un nœud (logique gameplay).
                 * @param id ID de l'entité à modifier.
                 * @param active true pour activer, false pour désactiver.
                 * 
                 * 🔹 Effet :
                 *    • Modifie le flag NkSceneNode::active
                 *    • Les systèmes devraient vérifier ce flag avant Update()
                 *    • Ne modifie PAS la visibilité renderer (voir SetVisible)
                 * 
                 * @note Désactiver un parent n'affecte pas automatiquement les enfants.
                 *       Pour une désactivation récursive, implémenter un helper.
                 */
                void SetActive(NkEntityId id, bool active) noexcept;

                /**
                 * @brief Définit la visibilité d'un nœud (rendu uniquement).
                 * @param id ID de l'entité à modifier.
                 * @param visible true pour rendre visible, false pour cacher.
                 * 
                 * 🔹 Effet :
                 *    • Modifie le flag NkSceneNode::visible
                 *    • Le renderer devrait skipper les entités avec visible=false
                 *    • La logique gameplay continue de s'exécuter normalement
                 * 
                 * @note Utile pour :
                 *    • Occlusion culling manuel
                 *    • Effets de fondu (alpha + visible toggle)
                 *    • Debug : cacher temporairement des éléments
                 */
                void SetVisible(NkEntityId id, bool visible) noexcept;

                // ── Destruction récursive ───────────────────────────────────
                /**
                 * @brief Détruit une entité et tous ses descendants récursivement.
                 * @param id ID de la racine de la sous-arborescence à détruire.
                 * 
                 * 🔹 Algorithme :
                 *    1. Copie locale des IDs enfants (évite l'invalidation)
                 *    2. Appel récursif sur chaque enfant (post-order traversal)
                 *    3. Détachement du parent (nettoyage des références)
                 *    4. Destruction finale de l'entité via mWorld.Destroy()
                 * 
                 * 🔹 Sécurité :
                 *    • La copie locale des enfants protège contre la modification
                 *      de NkChildren pendant l'itération
                 *    • L'ordre post-order garantit que les enfants sont détruits
                 *      avant leur parent (évite les dangling references)
                 * 
                 * @warning Cette méthode appelle Destroy() immédiat — ne pas l'utiliser
                 *          pendant l'itération sur une query. Pour un usage sûr,
                 *          implémenter une version Deferred en wrapper externe.
                 * 
                 * @example
                 * @code
                 * // Détruire un véhicule et tous ses composants (roues, moteur...)
                 * scene.DestroyRecursive(vehicleId);
                 * // Après cet appel, vehicleId n'est plus valide
                 * @endcode
                 */
                void DestroyRecursive(NkEntityId id) noexcept;

            private:
                /// Référence au monde ECS orchestré (non possédé)
                NkWorld& mWorld;

                /// Nom lisible de la scène (pour logs/debug)
                NkString mName;

                /// Pointeur vers le script de scène (owned, raw ptr)
                /// @note Pas de NkOwned ici car le lifecycle est géré manuellement
                ///       via new/delete dans SetScript/ClearScript/~NkSceneGraph
                NkSceneScript* mScript = nullptr;

                /// Flag indiquant si BeginPlay() a été appelé
                bool mStarted = false;

                /// Flag de pause — skip Tick/LateTick/FixedTick quand true
                bool mPaused = false;
        };

        // =====================================================================
        // 🔗 Alias de compatibilité transitoire
        // =====================================================================
        /**
         * @brief Alias pour migration progressive : NkScene → NkSceneGraph.
         * @deprecated Utiliser directement NkSceneGraph dans le nouveau code.
         * 
         * @note Cet alias sera supprimé après migration complète du codebase.
         *       Nouveaux développements : toujours utiliser NkSceneGraph
         *       pour éviter toute confusion avec d'autres systèmes de scène.
         */
        using NkScene = NkSceneGraph;

        // =====================================================================
        // 📦 Implémentations inline des templates et méthodes simples
        // =====================================================================

        // ── Script de scène (templates) ────────────────────────────────────
        template<typename T, typename... Args>
        T* NkSceneGraph::SetScript(Args&&... args) noexcept {
            static_assert(
                std::is_base_of_v<NkSceneScript, T>,
                "T must derive from NkSceneScript"
            );
            if (mScript) {
                mScript->OnEndPlay();
            }
            mScript = new T(std::forward<Args>(args)...);
            mScript->SetContext(&mWorld, this);
            return static_cast<T*>(mScript);
        }

        template<typename T>
        T* NkSceneGraph::GetScript() noexcept {
            return static_cast<T*>(mScript);
        }

        template<typename T>
        const T* NkSceneGraph::GetScript() const noexcept {
            return static_cast<const T*>(mScript);
        }

        // ── SpawnActor (template) ──────────────────────────────────────────
        template<typename T, typename... Args>
        T NkSceneGraph::SpawnActor(const char* name,
                                   float32 x, float32 y, float32 z,
                                   Args&&... args) noexcept {
            // NkGameObjectFactory::Create() instead of mWorld.CreateGameObject()
            // NkWorld does not know NkGameObject - that is Nkentseu's responsibility
            T actor = NkGameObjectFactory::Create<T>(
                mWorld, name, std::forward<Args>(args)...
            );
            actor.SetPosition(x, y, z);
            return actor;
        }

        // =====================================================================
        // 📚 EXEMPLES D'UTILISATION COMPLETS ET DÉTAILLÉS
        // =====================================================================
        /**
         * @addtogroup NkSceneGraph_Examples
         * @{
         */

        /**
         * @example 01_basic_scene_setup.cpp
         * @brief Configuration basique d'une scène avec script et hiérarchie.
         * 
         * @code
         * #include "NKECS/Scene/NkSceneGraph.h"
         * #include "NKECS/World/NkWorld.h"
         * #include "MyGame/Scripts/LevelScript.h"
         * 
         * using namespace nkentseu::ecs;
         * 
         * void Example_BasicSetup() {
         *     // ── Initialisation du monde et de la scène ─────────────────
         *     NkWorld world;
         *     NkSceneGraph scene(world, "Level_01_Forest");
         * 
         *     // ── Attacher un script de scène personnalisé ───────────────
         *     auto* levelScript = scene.SetScript<ForestLevelScript>(
         *         / *difficulty=* /2,
         *         / *weather=* /WeatherType::Rainy
         *     );
         * 
         *     // ── Spawn d'acteurs avec position initiale ─────────────────
         *     auto player = scene.SpawnActor<Player>(
         *         "Player_Hero",
         *         10.f, 0.f, 5.f  // Position locale (par rapport à la racine)
         *     );
         * 
         *     // ── Construction de la hiérarchie ──────────────────────────
         *     NkEntityId worldRoot = scene.SpawnNode("WorldRoot");
         *     
         *     NkEntityId vehicle = scene.SpawnNode("Vehicle_Car");
         *     scene.SetParent(vehicle, worldRoot);  // Véhicule dans le monde
         * 
         *     NkEntityId wheelFL = scene.SpawnNode("Wheel_FrontLeft");
         *     scene.SetParent(wheelFL, vehicle);    // Roue attachée au véhicule
         * 
         *     // ── Configuration des métadonnées de scène ─────────────────
         *     if (auto* node = world.Get<NkSceneNode>(wheelFL)) {
         *         node->layer = 2;        // Layer "Physics"
         *         node->visible = true;   // Visible au rendu
         *         node->active = true;    // Actif pour la logique
         *     }
         * 
         *     // ── Le lifecycle est géré automatiquement ──────────────────
         *     // NkSceneManager::LoadScene() appellera scene.BeginPlay()
         *     // NkSceneLifecycleSystem appellera Tick/LateTick/FixedTick
         *     // Pas d'appel manuel requis de la part de l'utilisateur
         * }
         * @endcode
         */

        /**
         * @example 02_scene_script_lifecycle.cpp
         * @brief Implémentation d'un script de scène avec callbacks lifecycle.
         * 
         * @code
         * #include "NKECS/Scene/NkSceneGraph.h"
         * #include "NKECS/Scene/NkSceneScript.h"
         * 
         * using namespace nkentseu::ecs;
         * 
         * // Définition d'un script de scène personnalisé
         * class DungeonLevelScript : public NkSceneScript {
         * public:
         *     DungeonLevelScript(int floorIndex, int enemyCount)
         *         : mFloorIndex(floorIndex), mMaxEnemies(enemyCount) {}
         * 
         *     void OnBeginPlay() override {
         *         // Initialisation : spawn des ennemis, configuration de l'ambiance
         *         printf("Début du donjon - Étage %d\n", mFloorIndex);
         *         SpawnEnemies(mMaxEnemies);
         *         SetupAmbientLighting(mFloorIndex);
         *     }
         * 
         *     void OnTick(float dt) override {
         *         // Logique principale : mise à jour des timers, événements
         *         mElapsedTime += dt;
         *         
         *         // Spawn vague d'ennemis toutes les 30 secondes
         *         if (mElapsedTime >= 30.f && !mWaveSpawned) {
         *             SpawnEnemyWave(5);
         *             mWaveSpawned = true;
         *         }
         *     }
         * 
         *     void OnLateTick(float dt) override {
         *         // Post-traitement : mise à jour UI, caméras, effets
         *         UpdateMinimap();
         *         UpdateCameraShake(dt);
         *     }
         * 
         *     void OnFixedTick(float fixedDt) override {
         *         // Physique déterministe : collisions, projectiles
         *         UpdateProjectilePhysics(fixedDt);
         *         CheckTrapCollisions();
         *     }
         * 
         *     void OnPause() override {
         *         // Pause : stopper les timers, mettre en sourdine l'audio
         *         mPausedTime = NkTime::Now();
         *         AudioManager::SetGlobalVolume(0.3f);  // Baisser le volume
         *     }
         * 
         *     void OnResume() override {
         *         // Resume : reprendre les timers, restaurer l'audio
         *         float pauseDuration = NkTime::Now() - mPausedTime;
         *         mElapsedTime += pauseDuration;  // Compenser le temps de pause
         *         AudioManager::SetGlobalVolume(1.0f);
         *     }
         * 
         *     void OnEndPlay() override {
         *         // Nettoyage : sauvegarde, libération de ressources
         *         SavePlayerProgress();
         *         CleanupDynamicAssets();
         *         printf("Fin du donjon - Étage %d\n", mFloorIndex);
         *     }
         * 
         * private:
         *     int mFloorIndex;
         *     int mMaxEnemies;
         *     float mElapsedTime = 0.f;
         *     bool mWaveSpawned = false;
         *     float mPausedTime = 0.f;
         * 
         *     void SpawnEnemies(int count) { / * ... * / }
         *     void SetupAmbientLighting(int floor) { / * ... * / }
         *     void SpawnEnemyWave(int count) { / * ... * / }
         *     void UpdateMinimap() { / * ... * / }
         *     void UpdateCameraShake(float dt) { / * ... * / }
         *     void UpdateProjectilePhysics(float dt) { /\* ... *\/ }
         *     void CheckTrapCollisions() { / * ... * / }
         *     void SavePlayerProgress() { / * ... * / }
         *     void CleanupDynamicAssets() { / * ... * / }
         * };
         * 
         * void Example_ScriptUsage() {
         *     NkWorld world;
         *     NkSceneGraph scene(world, "Dungeon_Floor1");
         * 
         *     // Attacher le script avec paramètres de construction
         *     auto* dungeon = scene.SetScript<DungeonLevelScript>(
         *         / *floorIndex=* /1,
         *         / *enemyCount=* /20
         *     );
         * 
         *     // Le script reçoit automatiquement les callbacks lifecycle
         *     // via NkSceneLifecycleSystem — aucun appel manuel requis
         * 
         *     // Accès au script pour interaction externe (optionnel)
         *     if (auto* script = scene.GetScript<DungeonLevelScript>()) {
         *         // Déclencher un événement depuis l'extérieur du script
         *         // (à utiliser avec précaution pour éviter le couplage fort)
         *     }
         * }
         * @endcode
         */

        /**
         * @example 03_hierarchy_transforms.cpp
         * @brief Gestion des transforms locaux et mondiaux en hiérarchie.
         * 
         * @code
         * #include "NKECS/Scene/NkSceneGraph.h"
         * #include "NKECS/Systems/NkTransformSystem.h"
         * 
         * using namespace nkentseu::ecs;
         * 
         * void Example_HierarchyTransforms() {
         *     NkWorld world;
         *     NkSceneGraph scene(world);
         * 
         *     // ── Création d'une hiérarchie à 3 niveaux ──────────────────
         *     NkEntityId root = scene.SpawnNode("Robot_Root");
         *     NkEntityId arm = scene.SpawnNode("Robot_Arm");
         *     NkEntityId gripper = scene.SpawnNode("Robot_Gripper");
         * 
         *     scene.SetParent(arm, root);
         *     scene.SetParent(gripper, arm);
         * 
         *     // ── Modification des transforms locaux ─────────────────────
         *     if (auto* local = world.Get<NkLocalTransform>(arm)) {
         *         local->position[1] = 2.f;  // Bras décalé de 2m en Y
         *         // Pour rotation quaternion : utiliser NkQuat helpers
         *     }
         * 
         *     // ── Marquage dirty pour recalcul ───────────────────────────
         *     if (auto* wt = world.Get<NkWorldTransform>(arm)) {
         *         wt->dirty = true;  // Force le recalcul de la matrice monde
         *     }
         * 
         *     // ── Mise à jour via le système dédié (à appeler chaque frame)
         *     // NkTransformSystem::Update(world);
         *     // Ce système :
         *     // 1. Parcourt les entités avec NkLocalTransform + NkParent
         *     // 2. Calcule les matrices mondiales par propagation hiérarchique
         *     // 3. Met à jour NkWorldTransform et clear le flag dirty
         * 
         *     // ── Lecture de la matrice mondiale après update ────────────
         *     if (const auto* worldMat = world.Get<NkWorldTransform>(gripper)) {
         *         if (!worldMat->dirty) {
         *             // Matrice à jour — utilisable pour le rendu
         *             const float32* m = worldMat->matrix;
         *             printf("Position mondiale: (%.2f, %.2f, %.2f)\n",
         *                    m[12], m[13], m[14]);  // Translation row-major
         *         }
         *     }
         * 
         *     // ── Bonnes pratiques :
         *     // • Ne jamais modifier NkWorldTransform directement
         *     // • Toujours marquer dirty après modification de NkLocalTransform
         *     // • Appeler NkTransformSystem::Update() avant le rendu
         *     // • Pour des animations complexes, utiliser un AnimationComponent
         *     //   qui met à jour NkLocalTransform et marque dirty automatiquement
         * }
         * @endcode
         */

        /**
         * @example 04_pause_resume_pattern.cpp
         * @brief Gestion de la pause avec préservation de l'état.
         * 
         * @code
         * #include "NKECS/Scene/NkSceneGraph.h"
         * #include "MyGame/Systems/PauseManager.h"
         * 
         * using namespace nkentseu::ecs;
         * 
         * class PauseManager {
         *     NkSceneGraph& mScene;
         *     bool mWasPaused = false;
         * 
         * public:
         *     PauseManager(NkSceneGraph& scene) : mScene(scene) {}
         * 
         *     void TogglePause() {
         *         if (mScene.IsPaused()) {
         *             ResumeGame();
         *         } else {
         *             PauseGame();
         *         }
         *     }
         * 
         * private:
         *     void PauseGame() {
         *         // 1. Mettre la scène en pause (stop Tick/LateTick/FixedTick)
         *         mScene.Pause();
         *         mWasPaused = true;
         * 
         *         // 2. Activer l'UI de pause (layer UI, visible=true)
         *         NkVector<NkEntityId> uiElements;
         *         mScene.FindByLayer(Layers::UI_PAUSE, uiElements);
         *         for (NkEntityId id : uiElements) {
         *             mScene.SetVisible(id, true);
         *         }
         * 
         *         // 3. Mettre en sourdine l'audio gameplay (optionnel)
         *         AudioManager::SetCategoryVolume("Gameplay", 0.2f);
         * 
         *         // 4. Stopper les particules non-essentielles (optimisation)
         *         //    (les particules UI de pause peuvent continuer)
         *     }
         * 
         *     void ResumeGame() {
         *         if (!mWasPaused) return;
         * 
         *         // 1. Reprendre la scène (reactive Tick/LateTick/FixedTick)
         *         mScene.Resume();
         *         mWasPaused = false;
         * 
         *         // 2. Cacher l'UI de pause
         *         NkVector<NkEntityId> uiElements;
         *         mScene.FindByLayer(Layers::UI_PAUSE, uiElements);
         *         for (NkEntityId id : uiElements) {
         *             mScene.SetVisible(id, false);
         *         }
         * 
         *         // 3. Restaurer le volume audio
         *         AudioManager::SetCategoryVolume("Gameplay", 1.0f);
         * 
         *         // 4. Optionnel : effet de transition (fade-in, son de reprise)
         *         UIManager::PlayTransitionEffect("Resume");
         *     }
         * };
         * 
         * void Example_PauseUsage() {
         *     NkWorld world;
         *     NkSceneGraph scene(world);
         *     PauseManager pauseMgr(scene);
         * 
         *     // Dans la boucle d'input :
         *     if (Input::KeyPressed(NkKey::Escape)) {
         *         pauseMgr.TogglePause();
         *     }
         * 
         *     // Le script de scène reçoit automatiquement OnPause/OnResume
         *     // via le lifecycle — pas de code supplémentaire requis
         * }
         * @endcode
         */

        /**
         * @example 05_best_practices_pitfalls.cpp
         * @brief Bonnes pratiques et pièges courants avec NkSceneGraph.
         * 
         * @code
         * // ✅ BONNES PRATIQUES
         * 
         * // 1. Toujours vérifier la validité des IDs retournés par FindByName
         * NkEntityId door = scene.FindByName("MainDoor");
         * if (door.IsValid()) {  // ✅ Vérification essentielle
         *     scene.SetActive(door, false);
         * }
         * 
         * // 2. Réutiliser les vecteurs de collecte pour éviter les allocations
         * NkVector<NkEntityId> layerBuffer;  // Membre de classe, alloué une fois
         * void RenderLayer(uint8 layer) {
         *     layerBuffer.Clear();  // Réutilise la mémoire existante
         *     scene.FindByLayer(layer, layerBuffer);
         *     // ... rendu
         * }
         * 
         * // 3. Marquer dirty après toute modification de transform local
         * if (auto* local = world.Get<NkLocalTransform>(obj)) {
         *     local->position[0] += speed * dt;
         * }
         * if (auto* worldT = world.Get<NkWorldTransform>(obj)) {
         *     worldT->dirty = true;  // ✅ Force le recalcul hiérarchique
         * }
         * 
         * // 4. Utiliser DestroyRecursive pour nettoyer les hiérarchies
         * // plutôt que de détruire manuellement chaque enfant
         * scene.DestroyRecursive(vehicleId);  // ✅ Propre et sécurisé
         * 
         * // 5. Laisser le lifecycle être géré par les systèmes
         * // ❌ NE PAS FAIRE : scene.Tick(dt);  // Appel manuel interdit
         * // ✅ FAIRE : Enregistrer NkSceneLifecycleSystem dans NkScheduler
         * 
         * // 6. Pour les scripts, utiliser OnEndPlay() pour le nettoyage
         * class MyScript : public NkSceneScript {
         *     ResourceHandle mLoadedAsset;
         * public:
         *     void OnBeginPlay() override {
         *         mLoadedAsset = LoadAsset("heavy_resource");  // Acquisition
         *     }
         *     void OnEndPlay() override {
         *         UnloadAsset(mLoadedAsset);  // ✅ Libération garantie
         *     }
         * };
         * 
         * 
         * // ❌ PIÈGES À ÉVITER
         * 
         * // 1. Modifier NkWorldTransform directement (sera écrasé au prochain update)
         * // ❌ world.Get<NkWorldTransform>(id)->matrix[12] = 10.f;  // Inutile !
         * // ✅ Modifier NkLocalTransform et marquer dirty à la place
         * 
         * // 2. Appeler DestroyRecursive pendant une itération de query
         * for (auto [node] : world.Query<NkSceneNode>()) {
         *     if (ShouldDelete(node)) {
         *         scene.DestroyRecursive(node->ownerEntity);  // ❌ Danger !
         *     }
         * }
         * // ✅ Solution : collecter les IDs, détruire après la boucle
         * // ou utiliser une version Deferred custom
         * 
         * // 3. Oublier de détacher avant destruction (fuites de références)
         * // ❌ world.Destroy(childId);  // Le parent garde une référence dangling
         * // ✅ scene.DestroyRecursive(childId);  // Nettoie parent/enfants
         * 
         * // 4. Appeler les méthodes lifecycle manuellement
         * // ❌ scene.BeginPlay(); scene.Tick(dt);  // Interdit !
         * // ✅ Laisser NkSceneManager et NkSceneLifecycleSystem gérer
         * 
         * // 5. Créer plusieurs scripts pour une même scène
         * // ❌ scene.SetScript<ScriptA>(); scene.SetScript<ScriptB>();
         * //    // ScriptA est détruit, seul ScriptB reste — peut causer des bugs
         * // ✅ Utiliser un script principal qui délègue à des sous-systèmes
         * //    class MainSceneScript : public NkSceneScript {
         * //        CombatSystem mCombat;
         * //        QuestSystem mQuests;
         * //        void OnTick(float dt) override {
         * //            mCombat.Update(dt);
         * //            mQuests.Update(dt);
         * //        }
         * //    };
         * @endcode
         */

        /** @} */ // end of NkSceneGraph_Examples group

    } // namespace ecs
} // namespace nkentseu