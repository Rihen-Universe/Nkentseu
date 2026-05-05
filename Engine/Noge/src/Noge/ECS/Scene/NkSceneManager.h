#pragma once
// =============================================================================
// Scene/NkSceneManager.h — v2 (gestionnaire de scènes avec transitions)
// =============================================================================
/**
 * @file NkSceneManager.h
 * @brief Définition de NkSceneManager : orchestrateur de chargement et transitions de scènes.
 * 
 * Ce fichier expose l'API publique pour :
 * - Enregistrer des scènes via des factories (NkSceneFactory)
 * - Charger/décharger des scènes avec support de transitions (Instant/Fade/Custom)
 * - Gérer une scène de chargement optionnelle pendant les transitions longues
 * - Accéder à la scène courante depuis n'importe où dans l'application
 * - Pause/Resume de la scène active
 * 
 * 🔹 CONCEPTS CLÉS :
 *    • NkSceneHandle : identifiant opaque d'une scène enregistrée (via NkString name)
 *    • NkSceneFactory : fonction qui crée et peuple une NkSceneGraph dans un NkWorld
 *    • NkSceneTransition : décrit le type et la durée d'une transition entre scènes
 * 
 * 🔹 FLUX DE CHARGEMENT D'UNE SCÈNE :
 * @code
 * LoadScene("MainLevel", transition)
 *   1. UnloadCurrent() :
 *      • Appelle mCurrentScene->EndPlay()
 *      • Flush des opérations différées (mWorld.FlushDeferred())
 *      • Delete de l'ancienne scène
 *   2. Si transition != Instant ET loadingScene spécifié :
 *      • LoadLoadingScene(loadingScene) → affiche écran de chargement
 *   3. Création de la nouvelle scène :
 *      • Appel de la factory : (*factory)(mWorld) → retourne NkSceneGraph*
 *      • Stockage dans mCurrentScene + mCurrentName
 *   4. Initialisation :
 *      • mCurrentScene->BeginPlay() → point d'entrée du script de scène
 *   5. Si loadingScene était actif :
 *      • UnloadLoadingScene() → cache l'écran de chargement
 * @endcode
 * 
 * 🔹 THREAD-SAFETY :
 *    • Non thread-safe par défaut — toutes les opérations doivent être appelées
 *      depuis le thread principal (game loop)
 *    • Le NkWorld sous-jacent peut être thread-safe si configuré, mais
 *      NkSceneManager ne gère pas la synchronisation inter-threads
 * 
 * 🔹 GESTION MÉMOIRE :
 *    • NkSceneManager prend ownership des NkSceneGraph* retournés par les factories
 *    • Destruction via delete dans UnloadCurrent()/UnloadLoadingScene()
 *    • Les factories doivent utiliser new (pas de smart pointers pour éviter l'overhead)
 * 
 * @author nkentseu
 * @version 2.0
 * @date 2026
 */

#include "Nkentseu/ECS/Scene/NkSceneGraph.h"
#include "Nkentseu/ECS/Scene/NkSceneScript.h"
#include "NKECS/World/NkWorld.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Functional/NkFunction.h"

namespace nkentseu {
    namespace ecs {

        // =====================================================================
        // 🏭 NkSceneFactory — Signature des factories de scènes
        // =====================================================================
        /**
         * @typedef NkSceneFactory
         * @brief Type de fonction pour la création dynamique de scènes.
         * 
         * 🔹 Signature : NkSceneGraph* factory(NkWorld& world)
         * 
         * 🔹 Contrat :
         *    • La factory reçoit une référence au NkWorld cible
         *    • Elle doit créer une NkSceneGraph (via new) et la peupler
         *    • Elle retourne un pointeur brut — le NkSceneManager en prend ownership
         *    • En cas d'erreur, retourner nullptr (géré par LoadScene avec log d'erreur)
         * 
         * 🔹 Exemple d'implémentation :
         * @code
         * NkSceneGraph* MakeMainMenuScene(NkWorld& world) {
         *     auto* scene = new NkSceneGraph(world, "MainMenu");
         *     
         *     // Spawn des éléments d'UI
         *     scene->SpawnActor<UIButton>("Btn_Start", 100, 200);
         *     scene->SpawnActor<UIButton>("Btn_Options", 100, 250);
         *     
         *     // Attacher un script de scène
         *     scene->SetScript<MainMenuScript>();
         *     
         *     return scene;  // Ownership transféré au NkSceneManager
         * }
         * @endcode
         * 
         * @warning Ne pas utiliser de smart pointers (unique_ptr, shared_ptr)
         *          dans la signature — le NkSceneManager gère manuellement
         *          la durée de vie via new/delete pour éviter l'overhead.
         */
        using NkSceneFactory = NkFunction<NkSceneGraph*(NkWorld&)>;

        // =====================================================================
        // 🔄 NkTransitionType — Types de transitions supportés
        // =====================================================================
        /**
         * @enum NkTransitionType
         * @brief Classification des effets de transition entre scènes.
         * 
         * @note Chaque type définit un comportement visuel et temporel :
         *    • Instant : changement immédiat, aucune animation (debug, tests)
         *    • Fade : fondu vers le noir puis vers la nouvelle scène
         *    • FadeWhite : fondu vers le blanc (utile pour flashs, rêves)
         *    • Custom : callback utilisateur pour animation personnalisée
         * 
         * @warning Le type Custom nécessite une implémentation dans NkSceneManager::Update()
         *          (Phase 5 du TODO) — actuellement non fonctionnel.
         */
        enum class NkTransitionType : nk_uint8 {
            Instant = 0,    ///< Changement immédiat, aucune animation
            Fade,           ///< Fondu progressif vers le noir (standard)
            FadeWhite,      ///< Fondu progressif vers le blanc (effets spéciaux)
            Custom,         ///< Animation personnalisée via callback (à implémenter)
        };

        // =====================================================================
        // 🎬 NkSceneTransition — Descripteur de transition entre scènes
        // =====================================================================
        /**
         * @struct NkSceneTransition
         * @brief Configuration d'une transition : type, durée, scène de chargement.
         * 
         * 🔹 Champs :
         *    • type : effet visuel de la transition (Instant/Fade/Custom)
         *    • duration : durée en secondes pour les transitions animées (Fade)
         *    • loadingScene : nom optionnel d'une scène à afficher pendant le chargement
         * 
         * 🔹 Helpers de construction :
         *    • Instant() : transition immédiate, durée ignorée
         *    • Fade(dur) : fondu noir de durée 'dur' secondes
         *    • WithLoading(name, fadeDur) : fondu + écran de chargement intermédiaire
         * 
         * @note La scène de chargement est utile pour :
         *    • Masquer le temps de streaming des assets lourds
         *    • Afficher une barre de progression ou des conseils gameplay
         *    • Maintenir l'immersion pendant les changements de niveau
         * 
         * @example
         * @code
         * // Transition instantanée (debug, tests rapides)
         * auto t1 = NkSceneTransition::Instant();
         * 
         * // Fondu noir de 1 seconde
         * auto t2 = NkSceneTransition::Fade(1.0f);
         * 
         * // Fondu + écran "Chargement..." pendant 0.5s
         * auto t3 = NkSceneTransition::WithLoading("LoadingScreen", 0.5f);
         * 
         * // Utilisation dans LoadScene :
         * manager.LoadScene("Level_2", t3);
         * @endcode
         */
        struct NkSceneTransition {
            /// Type d'effet de transition
            NkTransitionType type = NkTransitionType::Instant;

            /// Durée de l'animation en secondes (pour Fade/Custom)
            nk_float32 duration = 0.5f;

            /// Nom optionnel de la scène à afficher pendant le chargement
            NkString loadingScene;

            /**
             * @brief Crée une transition instantanée.
             * @return NkSceneTransition configuré pour Instant.
             */
            [[nodiscard]] static NkSceneTransition Instant() noexcept {
                return {NkTransitionType::Instant, 0.f, {}};
            }

            /**
             * @brief Crée une transition par fondu noir.
             * @param dur Durée du fondu en secondes (par défaut 0.5s).
             * @return NkSceneTransition configuré pour Fade.
             */
            [[nodiscard]] static NkSceneTransition Fade(nk_float32 dur = 0.5f) noexcept {
                return {NkTransitionType::Fade, dur, {}};
            }

            /**
             * @brief Crée une transition avec écran de chargement intermédiaire.
             * @param loadingScene Nom de la scène à afficher pendant le chargement.
             * @param fadeDur Durée des fondus d'entrée/sortie (par défaut 0.3s).
             * @return NkSceneTransition configuré pour Fade + loading.
             * 
             * @note Le flux sera :
             *       1. Fondu vers noir (fadeDur)
             *       2. Affichage de loadingScene
             *       3. Chargement de la scène cible en arrière-plan
             *       4. Fondu depuis noir vers la nouvelle scène (fadeDur)
             */
            [[nodiscard]] static NkSceneTransition WithLoading(
                const char* loadingScene, nk_float32 fadeDur = 0.3f) noexcept {
                return {NkTransitionType::Fade, fadeDur, NkString(loadingScene)};
            }
        };

        // =====================================================================
        // 🎭 NkSceneManager — Orchestrateur de scènes et transitions
        // =====================================================================
        /**
         * @class NkSceneManager
         * @brief Gestionnaire centralisé du cycle de vie des scènes.
         * 
         * 🔹 Responsabilités :
         *    • Enregistrement des scènes via Register(name, factory)
         *    • Chargement/déchargement sécurisé avec gestion des transitions
         *    • Support d'une scène de chargement optionnelle pour les transitions longues
         *    • Accès global à la scène courante via GetCurrent()
         *    • Pause/Resume de la scène active
         *    • Mise à jour des transitions animées dans Update(dt)
         * 
         * 🔹 Architecture :
         *    • Stocke les factories dans un NkVector<Entry> — recherche linéaire O(n)
         *      (n typiquement < 20 scènes enregistrées → négligeable)
         *    • Garde un pointeur vers la scène courante (mCurrentScene) et optionnellement
         *      une scène de chargement (mLoadingScene)
         *    • Délègue le lifecycle (BeginPlay/Tick/EndPlay) à NkSceneGraph
         * 
         * 🔹 Intégration dans l'application :
         * @code
         * class Application {
         *     NkWorld mWorld;
         *     NkSceneManager mSceneManager;  // Dépend de mWorld
         *     
         * public:
         *     Application() : mSceneManager(mWorld) {
         *         // Enregistrement des scènes au démarrage
         *         mSceneManager.Register("MainMenu", MakeMainMenuScene);
         *         mSceneManager.Register("Level_1", MakeLevel1Scene);
         *         mSceneManager.SetLoadingScene("LoadingScreen");
         *         
         *         // Chargement initial
         *         mSceneManager.LoadScene("MainMenu", NkSceneTransition::Instant());
         *     }
         *     
         *     void Run() {
         *         while (mRunning) {
         *             float dt = GetDeltaTime();
         *             
         *             // Mise à jour des transitions animées
         *             mSceneManager.Update(dt);
         *             
         *             // ... autres systèmes ECS, rendu, input ...
         *         }
         *     }
         * };
         * @endcode
         * 
         * @warning Le NkSceneManager ne possède PAS le NkWorld — il le référence.
         *          Assurez-vous que le monde reste valide plus longtemps que le manager.
         * 
         * @example
         * @code
         * // Depuis un script de scène pour changer de niveau :
         * void MyLevelScript::OnVictory() {
         *     // Accès au manager via la scène courante
         *     auto* scene = GetScene();  // NkSceneGraph*
         *     auto* manager = scene->GetManager();  // NkSceneManager*
         *     
         *     if (manager) {
         *         // Chargement du niveau suivant avec fondu
         *         manager->LoadScene("Level_2", NkSceneTransition::Fade(1.0f));
         *     }
         * }
         * @endcode
         */
        class NkSceneManager {
            public:
                /**
                 * @brief Constructeur principal.
                 * @param world Référence au monde ECS dans lequel les scènes seront créées.
                 * 
                 * @note Le NkSceneManager ne possède pas le monde — il le référence.
                 *       La durée de vie de 'world' doit englober celle du manager.
                 * 
                 * @warning Aucune scène n'est chargée au construction — appeler
                 *          Register() puis LoadScene() pour initialiser.
                 */
                explicit NkSceneManager(NkWorld& world) noexcept;

                /**
                 * @brief Destructeur : décharge la scène courante si active.
                 * @note Garantit que EndPlay() est appelé avant destruction,
                 *       même en cas d'arrêt prématuré de l'application.
                 */
                ~NkSceneManager() noexcept;

                // Suppression des copies (ownership des scènes non copiable)
                NkSceneManager(const NkSceneManager&) = delete;
                NkSceneManager& operator=(const NkSceneManager&) = delete;

                // ── Enregistrement des scènes ───────────────────────────────
                /**
                 * @brief Enregistre une nouvelle scène dans le manager.
                 * @param name Nom unique de la scène (clé de lookup pour LoadScene).
                 * @param factory Fonction qui crée et peuple la scène dans un NkWorld.
                 * 
                 * 🔹 Comportement :
                 *    • Si 'name' existe déjà : remplace la factory précédente
                 *    • Sinon : ajoute une nouvelle entrée dans mEntries
                 *    • La factory est copiée via NkFunction (capture possible)
                 * 
                 * @note À appeler une fois au démarrage de l'application, avant
                 *       tout chargement de scène. Thread-safe si appelé depuis
                 *       le thread principal uniquement.
                 * 
                 * @example
                 * @code
                 * manager.Register("MainMenu", [](NkWorld& w) {
                 *     auto* scene = new NkSceneGraph(w, "MainMenu");
                 *     scene->SetScript<MainMenuScript>();
                 *     return scene;
                 * });
                 * @endcode
                 */
                void Register(const NkString& name, NkSceneFactory factory) noexcept;

                /**
                 * @brief Définit la scène à afficher pendant les transitions lentes.
                 * @param name Nom de la scène de chargement (doit être enregistrée).
                 * 
                 * @note Cette scène sera automatiquement chargée si :
                 *    • LoadScene() est appelé avec une transition != Instant
                 *    • ET le NkSceneTransition spécifie un loadingScene non-vide
                 * 
                 * @warning La scène de chargement doit être légère et ne pas dépendre
                 *          d'assets longs à charger — son rôle est de masquer
                 *          le temps de chargement, pas de le prolonger.
                 */
                void SetLoadingScene(const NkString& name) noexcept;

                // ── Chargement / Déchargement de scènes ─────────────────────
                /**
                 * @brief Charge une scène enregistrée avec transition optionnelle.
                 * @param name Nom de la scène à charger (doit avoir été Register()-ée).
                 * @param transition Configuration de la transition (par défaut Instant).
                 * @return true si le chargement a réussi, false en cas d'erreur.
                 * 
                 * 🔹 Flux détaillé :
                 *    1. Validation : recherche de la factory par 'name'
                 *    2. UnloadCurrent() :
                 *       • EndPlay() sur l'ancienne scène
                 *       • FlushDeferred() pour appliquer les destructions
                 *       • Delete de l'ancienne scène
                 *    3. Si transition animée + loadingScene spécifié :
                 *       • LoadLoadingScene() → affiche l'écran de chargement
                 *    4. Création de la nouvelle scène :
                 *       • Appel de la factory : (*factory)(mWorld)
                 *       • Stockage dans mCurrentScene/mCurrentName
                 *    5. Initialisation :
                 *       • mCurrentScene->BeginPlay() → entrée du script
                 *    6. Si loadingScene était actif :
                 *       • UnloadLoadingScene() → cache l'écran de chargement
                 * 
                 * @note En cas d'erreur (factory nullptr, scène inconnue) :
                 *    • Log d'erreur via logger.Errorf()
                 *    • Retourne false sans modifier l'état courant
                 *    • La scène précédente reste active (si elle existait)
                 * 
                 * @warning Cette méthode est bloquante — le chargement se fait
                 *          de façon synchrone. Pour du chargement asynchrone
                 *          (streaming d'assets), utiliser une scène de chargement
                 *          avec un système de background loading.
                 * 
                 * @example
                 * @code
                 * // Chargement immédiat (debug)
                 * manager.LoadScene("TestLevel", NkSceneTransition::Instant());
                 * 
                 * // Chargement avec fondu de 1 seconde
                 * manager.LoadScene("Level_2", NkSceneTransition::Fade(1.0f));
                 * 
                 * // Chargement avec écran "Loading..." intermédiaire
                 * manager.LoadScene("Level_3",
                 *     NkSceneTransition::WithLoading("LoadingScreen", 0.5f));
                 * @endcode
                 */
                bool LoadScene(const NkString& name,
                               const NkSceneTransition& transition =
                                   NkSceneTransition::Instant()) noexcept;

                /**
                 * @brief Recharge la scène courante (utile pour restart de niveau).
                 * @return true si le rechargement a réussi, false si aucune scène active.
                 * 
                 * @note Équivalent à LoadScene(mCurrentName) — préserve la transition
                 *       par défaut (Instant). Pour une transition spécifique,
                 *       appeler LoadScene() directement avec le bon NkSceneTransition.
                 * 
                 * @example
                 * @code
                 * // Dans un script de jeu : joueur mort → restart du niveau
                 * void PlayerScript::OnDeath() {
                 *     auto* scene = GetScene();
                 *     if (scene && scene->GetManager()) {
                 *         scene->GetManager()->ReloadCurrent();  // Restart immédiat
                 *     }
                 * }
                 * @endcode
                 */
                bool ReloadCurrent() noexcept;

                // ── Accès à la scène courante ───────────────────────────────
                /**
                 * @brief Retourne un pointeur mutable vers la scène active.
                 * @return NkSceneGraph* ou nullptr si aucune scène chargée.
                 * 
                 * @note Utile pour :
                 *    • Accéder aux méthodes de NkSceneGraph (SpawnActor, FindByName, etc.)
                 *    • Vérifier HasScript()/GetScript<T>() pour interaction avec le script
                 * 
                 * @warning Le pointeur retourné est valide tant que la scène n'est pas
                 *          déchargée. Ne pas le stocker au-delà de la frame courante
                 *          sans vérifier IsValid() ou HasScene().
                 */
                [[nodiscard]] NkSceneGraph* GetCurrent() noexcept;

                /**
                 * @brief Version const de GetCurrent().
                 */
                [[nodiscard]] const NkSceneGraph* GetCurrent() const noexcept;

                /**
                 * @brief Retourne le nom de la scène actuellement chargée.
                 * @return Référence à NkString (vide si aucune scène active).
                 * 
                 * @note Utile pour :
                 *    • Logs/debug : afficher la scène courante dans l'UI
                 *    • Sauvegarde : stocker le nom de la scène dans le savegame
                 *    • Cheats/debug : changer de scène via console de développement
                 */
                [[nodiscard]] const NkString& GetCurrentName() const noexcept;

                /**
                 * @brief Vérifie si une scène est actuellement chargée.
                 * @return true si mCurrentScene != nullptr.
                 * 
                 * @note Plus rapide que GetCurrent() != nullptr car évite le retour de pointeur.
                 *       À utiliser dans les guards early-return pour éviter les nullptr.
                 */
                [[nodiscard]] bool HasScene() const noexcept;

                // ── Contrôle de la scène courante ───────────────────────────
                /**
                 * @brief Met en pause la scène active (si elle existe).
                 * @note Délègue à mCurrentScene->Pause() — skip Tick/LateTick/FixedTick.
                 *       Silencieusement ignoré si aucune scène n'est chargée.
                 */
                void Pause() noexcept;

                /**
                 * @brief Reprend la scène active après une pause.
                 * @note Délègue à mCurrentScene->Resume() — réactive les mises à jour.
                 *       Silencieusement ignoré si aucune scène n'est chargée.
                 */
                void Resume() noexcept;

                // ── Mise à jour des transitions animées ─────────────────────
                /**
                 * @brief Met à jour l'état des transitions animées (à appeler chaque frame).
                 * @param dt Delta-time en secondes depuis la frame précédente.
                 * 
                 * 🔹 Rôle actuel :
                 *    • Placeholder pour l'animation des fondus (Phase 5 TODO)
                 *    • Gestion future des transitions Custom via callbacks
                 * 
                 * @note Cette méthode NE gère PAS Tick/LateTick/FixedTick —
                 *       c'est le rôle de NkSceneLifecycleSystem enregistré dans NkScheduler.
                 * 
                 * @warning À appeler depuis la boucle principale, AVANT le rendu,
                 *          pour que les changements d'alpha/visibilité soient pris
                 *          en compte dans le frame courant.
                 */
                void Update(nk_float32 dt) noexcept;

            private:
                /**
                 * @struct Entry
                 * @brief Entrée de registre : nom + factory d'une scène.
                 */
                struct Entry {
                    /// Nom unique de la scène (clé de lookup)
                    NkString name;
                    /// Factory pour créer la scène dans un NkWorld
                    NkSceneFactory factory;
                };

                /**
                 * @brief Trouve la factory enregistrée pour un nom de scène.
                 * @param name Nom à rechercher.
                 * @return Pointeur vers la factory si trouvée, nullptr sinon.
                 * 
                 * @note Recherche linéaire O(n) dans mEntries — acceptable car
                 *       n typiquement < 20 scènes enregistrées.
                 */
                NkSceneFactory* FindFactory(const NkString& name) noexcept;

                /**
                 * @brief Décharge la scène courante (EndPlay + cleanup).
                 * @note Méthode privée appelée par LoadScene() et le destructeur.
                 *       Garantit que EndPlay() est toujours appelé avant delete.
                 */
                void UnloadCurrent() noexcept;

                /**
                 * @brief Charge la scène de chargement (pour transitions longues).
                 * @param name Nom de la scène de chargement à activer.
                 * @note Appelée uniquement si transition != Instant ET loadingScene spécifié.
                 */
                void LoadLoadingScene(const NkString& name) noexcept;

                /**
                 * @brief Décharge la scène de chargement après utilisation.
                 * @note Appelée après que la scène cible a commencé son BeginPlay().
                 */
                void UnloadLoadingScene() noexcept;

                // ── Membres privés : état du manager ────────────────────────
                /// Référence au monde ECS dans lequel les scènes sont créées
                NkWorld& mWorld;

                /// Registre des scènes disponibles (nom → factory)
                NkVector<Entry> mEntries;

                /// Pointeur vers la scène actuellement active (owned)
                NkSceneGraph* mCurrentScene = nullptr;

                /// Nom de la scène courante (pour ReloadCurrent et logs)
                NkString mCurrentName;

                /// Pointeur vers la scène de chargement temporaire (owned)
                NkSceneGraph* mLoadingScene = nullptr;

                /// Nom de la scène de chargement par défaut (configurable)
                NkString mLoadingSceneName;
        };

        // =====================================================================
        // 📚 EXEMPLES D'UTILISATION COMPLETS ET DÉTAILLÉS
        // =====================================================================
        /**
         * @addtogroup NkSceneManager_Examples
         * @{
         */

        /**
         * @example 01_registration_and_initial_load.cpp
         * @brief Enregistrement des scènes et chargement initial au démarrage.
         * 
         * @code
         * #include "NKECS/Scene/NkSceneManager.h"
         * #include "NKECS/World/NkWorld.h"
         * #include "MyGame/Scenes/MainMenuScene.h"
         * #include "MyGame/Scenes/GameLevelScene.h"
         * #include "MyGame/Scenes/LoadingScene.h"
         * 
         * using namespace nkentseu::ecs;
         * 
         * // Factories de scènes — fonctions libres ou lambdas
         * NkSceneGraph* MakeMainMenuScene(NkWorld& world) {
         *     auto* scene = new NkSceneGraph(world, "MainMenu");
         *     
         *     // Spawn des boutons d'UI
         *     scene->SpawnActor<UIButton>("Btn_Start", 100, 200);
         *     scene->SpawnActor<UIButton>("Btn_Options", 100, 250);
         *     scene->SpawnActor<UIButton>("Btn_Quit", 100, 300);
         *     
         *     // Attacher le script de gestion du menu
         *     scene->SetScript<MainMenuScript>();
         *     
         *     return scene;
         * }
         * 
         * NkSceneGraph* MakeGameLevelScene(NkWorld& world) {
         *     auto* scene = new NkSceneGraph(world, "GameLevel");
         *     
         *     // Spawn du joueur et de l'environnement
         *     auto player = scene->SpawnActor<Player>("Hero", 10.f, 0.f, 5.f);
         *     scene->SpawnActor<Environment>("Forest_Level1");
         *     
         *     // Script de niveau avec logique gameplay
         *     scene->SetScript<GameLevelScript>(/ *difficulty=* /2);
         *     
         *     return scene;
         * }
         * 
         * NkSceneGraph* MakeLoadingScene(NkWorld& world) {
         *     auto* scene = new NkSceneGraph(world, "LoadingScreen");
         *     
         *     // UI minimale : barre de progression + texte
         *     scene->SpawnActor<LoadingBar>("ProgressBar", 200, 300);
         *     scene->SpawnActor<UIText>("LoadingText", 200, 350);
         *     
         *     // Pas de script complexe — juste affichage
         *     return scene;
         * }
         * 
         * void Application::Initialize() {
         *     // Initialisation du monde ECS
         *     NkWorld& world = GetWorld();
         *     
         *     // Création du scene manager
         *     mSceneManager = std::make_unique<NkSceneManager>(world);
         *     
         *     // ── Enregistrement des scènes disponibles ──────────────────
         *     mSceneManager->Register("MainMenu", MakeMainMenuScene);
         *     mSceneManager->Register("GameLevel", MakeGameLevelScene);
         *     mSceneManager->Register("LoadingScreen", MakeLoadingScene);
         *     
         *     // ── Configuration de la scène de chargement par défaut ─────
         *     mSceneManager->SetLoadingScene("LoadingScreen");
         *     
         *     // ── Chargement de la scène initiale ───────────────────────
         *     // Transition instantanée pour un démarrage rapide
         *     bool loaded = mSceneManager->LoadScene(
         *         "MainMenu",
         *         NkSceneTransition::Instant()
         *     );
         *     
         *     if (!loaded) {
         *         logger.Error("Échec du chargement du MainMenu — arrêt de l'application");
         *         RequestQuit();
         *     }
         * }
         * @endcode
         */

        /**
         * @example 02_scene_transitions.cpp
         * @brief Gestion des transitions entre scènes avec effets visuels.
         * 
         * @code
         * #include "NKECS/Scene/NkSceneManager.h"
         * 
         * using namespace nkentseu::ecs;
         * 
         * void Example_Transitions() {
         *     NkSceneManager& mgr = GetSceneManager();
         * 
         *     // ── Transition instantanée (debug, tests rapides) ─────────
         *     // Utile pour :
         *     // • Débogage sans attendre les animations
         *     // • Tests automatisés où la vitesse prime
         *     mgr.LoadScene("TestLevel", NkSceneTransition::Instant());
         * 
         *     // ── Fondu noir standard (expérience joueur normale) ───────
         *     // 1 seconde de fondu vers le noir, puis chargement, puis fondu retour
         *     mgr.LoadScene("Level_2", NkSceneTransition::Fade(1.0f));
         * 
         *     // ── Fondu blanc pour effet spécial (rêve, flash, téléportation)
         *     mgr.LoadScene("DreamSequence", NkSceneTransition::FadeWhite(0.8f));
         * 
         *     // ── Transition avec écran de chargement intermédiaire ─────
         *     // Pour les niveaux lourds qui nécessitent du streaming d'assets :
         *     // 1. Fondu vers noir (0.3s)
         *     // 2. Affichage de "LoadingScreen" avec barre de progression
         *     // 3. Chargement asynchrone de Level_3 en arrière-plan
         *     // 4. Fondu depuis noir vers Level_3 (0.3s)
         *     mgr.LoadScene("Level_3",
         *         NkSceneTransition::WithLoading("LoadingScreen", 0.3f)
         *     );
         * 
         *     // ── Pattern depuis un script de scène (changement de niveau)
         *     class LevelCompleteScript : public NkSceneScript {
         *     public:
         *         void OnVictory() override {
         *             // Accès au manager via la scène courante
         *             auto* scene = GetScene();
         *             if (scene && scene->GetManager()) {
         *                 // Afficher un écran "Victoire!" pendant 2s, puis charger le suivant
         *                 ShowVictoryScreen();
         *                 NkTimer::After(2.0f, [scene]() {
         *                     scene->GetManager()->LoadScene(
         *                         "Level_Next",
         *                         NkSceneTransition::Fade(1.0f)
         *                     );
         *                 });
         *             }
         *         }
         *     };
         * 
         *     // ── Gestion d'erreur : scène inconnue
         *     bool result = mgr.LoadScene("NonExistentLevel");
         *     if (!result) {
         *         // LoadScene retourne false et log une erreur
         *         // La scène précédente reste active — pas de crash
         *         ShowErrorDialog("Niveau non trouvé");
         *     }
         * }
         * @endcode
         */

        /**
         * @example 03_pause_resume_and_reload.cpp
         * @brief Contrôle de la scène courante : pause, reprise, rechargement.
         * 
         * @code
         * #include "NKECS/Scene/NkSceneManager.h"
         * 
         * using namespace nkentseu::ecs;
         * 
         * void Example_PauseAndReload() {
         *     NkSceneManager& mgr = GetSceneManager();
         * 
         *     // ── Pause du jeu (menu pause) ─────────────────────────────
         *     // Dans le handler de touche Escape :
         *     if (Input::KeyPressed(NkKey::Escape)) {
         *         if (!IsPaused()) {
         *             mgr.Pause();  // Délègue à la scène courante
         *             ShowPauseMenu();  // UI overlay
         *         } else {
         *             mgr.Resume();
         *             HidePauseMenu();
         *         }
         *     }
         * 
         *     // ── Rechargement de la scène courante (restart de niveau) ─
         *     // Utile pour :
         *     // • Joueur mort → restart du niveau
         *     // • Debug : tester un niveau depuis le début rapidement
         *     // • Speedrun : reset rapide sans retour au menu principal
         *     void PlayerScript::OnDeath() {
         *         auto* scene = GetScene();
         *         if (scene && scene->GetManager()) {
         *             // Option 1 : restart immédiat
         *             scene->GetManager()->ReloadCurrent();
         *             
         *             // Option 2 : restart avec fondu pour plus de confort visuel
         *             // scene->GetManager()->LoadScene(
         *             //     scene->GetManager()->GetCurrentName(),
         *             //     NkSceneTransition::Fade(0.5f)
         *             // );
         *         }
         *     }
         * 
         *     // ── Vérification de l'état avant opérations ────────────────
         *     void SafeSceneOperation() {
         *         NkSceneManager& mgr = GetSceneManager();
         *         
         *         // Toujours vérifier HasScene() avant d'utiliser GetCurrent()
         *         if (mgr.HasScene()) {
         *             auto* scene = mgr.GetCurrent();
         *             
         *             // Exemple : récupérer le script de scène pour interaction
         *             if (auto* script = scene->GetScript<GameplayScript>()) {
         *                 script->TriggerEvent("CheckpointReached");
         *             }
         *         } else {
         *             // Aucune scène chargée — fallback ou erreur
         *             logger.Warning("Tentative d'opération sur scène inexistante");
         *         }
         *     }
         * 
         *     // ── Accès au nom de la scène courante (logs, sauvegarde) ──
         *     void SaveGame() {
         *         NkSceneManager& mgr = GetSceneManager();
         *         if (mgr.HasScene()) {
         *             const NkString& sceneName = mgr.GetCurrentName();
         *             
         *             SaveData save;
         *             save.currentScene = sceneName;  // Pour reload au chargement
         *             save.playerState = GetPlayerState();
         *             // ... autres données ...
         *             
         *             SaveToFile(save);
         *         }
         *     }
         * }
         * @endcode
         */

        /**
         * @example 04_advanced_patterns.cpp
         * @brief Patterns avancés : chargement asynchrone, transitions custom, gestion d'erreurs.
         * 
         * @code
         * #include "NKECS/Scene/NkSceneManager.h"
         * #include "NKThreading/NkAsyncLoader.h"
         * 
         * using namespace nkentseu::ecs;
         * 
         * // ── Pattern 1 : Chargement asynchrone avec scène de progression
         * class AsyncLoadingScript : public NkSceneScript {
         * public:
         *     AsyncLoadingScript(const NkString& targetScene)
         *         : mTargetScene(targetScene) {}
         * 
         *     void OnBeginPlay() override {
         *         // Démarrer le chargement asynchrone en background
         *         mLoader = std::make_unique<NkAsyncLoader>(
         *             mTargetScene,
         *             [this](bool success) { OnLoadComplete(success); }
         *         );
         *         mLoader->Start();
         *     }
         * 
         *     void OnTick(float dt) override {
         *         // Mettre à jour la barre de progression
         *         if (mLoader && mLoader->IsActive()) {
         *             float progress = mLoader->GetProgress();  // 0.0 à 1.0
         *             UpdateProgressBar(progress);
         *             
         *             // Afficher des conseils aléatoires pendant le chargement
         *             if (mTipTimer <= 0.f) {
         *                 ShowRandomTip();
         *                 mTipTimer = 3.0f;  // Nouveau conseil toutes les 3s
         *             }
         *             mTipTimer -= dt;
         *         }
         *     }
         * 
         * private:
         *     void OnLoadComplete(bool success) {
         *         if (success) {
         *             // Chargement terminé : transition vers la scène cible
         *             auto* scene = GetScene();
         *             if (scene && scene->GetManager()) {
         *                 scene->GetManager()->LoadScene(
         *                     mTargetScene,
         *                     NkSceneTransition::Fade(0.3f)
         *                 );
         *             }
         *         } else {
         *             // Erreur de chargement : retour au menu avec message
         *             ShowErrorDialog("Échec du chargement du niveau");
         *             NkTimer::After(2.0f, []() {
         *                 GetSceneManager().LoadScene("MainMenu");
         *             });
         *         }
         *     }
         * 
         *     NkString mTargetScene;
         *     std::unique_ptr<NkAsyncLoader> mLoader;
         *     float mTipTimer = 0.f;
         * };
         * 
         * // ── Pattern 2 : Transition custom avec callback (à implémenter)
         * // Note : nécessite l'implémentation de NkTransitionType::Custom
         * struct CustomTransition {
         *     NkFunction<void(float alpha)> onAnimate;  // Callback d'animation
         *     NkFunction<void()> onComplete;            // Callback de fin
         * };
         * 
         * void Example_CustomTransition() {
         *     // Pseudo-code pour une transition "porte qui s'ouvre"
         *     CustomTransition doorTransition;
         *     doorTransition.onAnimate = [](float alpha) {
         *         // alpha: 0.0 → 1.0 pendant la transition
         *         // Animer l'ouverture d'une porte 3D en fonction de alpha
         *         AnimateDoor(alpha);
         *         // Assombrir progressivement les bords de l'écran (vignette)
         *         renderer.SetVignetteIntensity(1.f - alpha);
         *     };
         *     doorTransition.onComplete = []() {
         *         // Une fois la transition terminée, charger la scène suivante
         *         GetSceneManager().LoadScene("NextRoom", NkSceneTransition::Instant());
         *     };
         * 
         *     // À intégrer dans NkSceneManager::Update() quand Custom sera supporté
         *     // mgr.LoadScene("NextRoom", NkSceneTransition::Custom(doorTransition));
         * }
         * 
         * // ── Pattern 3 : Gestion robuste des erreurs de chargement
         * bool SafeLoadScene(const NkString& sceneName,
         *                    const NkSceneTransition& transition,
         *                    const NkString& fallbackScene = "MainMenu") {
         *     NkSceneManager& mgr = GetSceneManager();
         *     
         *     // Vérifier que la scène cible est bien enregistrée
         *     // (évite un crash si le nom est mal orthographié)
         *     if (!IsSceneRegistered(sceneName)) {
         *         logger.Error("Scène '{}' non enregistrée — fallback vers '{}'",
         *                      sceneName.CStr(), fallbackScene.CStr());
         *         return mgr.LoadScene(fallbackScene, NkSceneTransition::Instant());
         *     }
         *     
         *     // Tenter le chargement
         *     bool result = mgr.LoadScene(sceneName, transition);
         *     
         *     // En cas d'échec, tenter le fallback
         *     if (!result && !fallbackScene.IsEmpty()) {
         *         logger.Warning("Échec du chargement de '{}' — tentative de fallback",
         *                        sceneName.CStr());
         *         return mgr.LoadScene(fallbackScene, NkSceneTransition::Instant());
         *     }
         *     
         *     return result;
         * }
         * @endcode
         */

        /**
         * @example 05_best_practices_pitfalls.cpp
         * @brief Bonnes pratiques et pièges courants avec NkSceneManager.
         * 
         * @code
         * // ✅ BONNES PRATIQUES
         * 
         * // 1. Enregistrer toutes les scènes au démarrage, avant tout LoadScene
         * void Application::Initialize() {
         *     // ✅ FAIRE : registre complet avant premier chargement
         *     mgr.Register("MainMenu", MakeMainMenu);
         *     mgr.Register("Level_1", MakeLevel1);
         *     mgr.LoadScene("MainMenu", NkSceneTransition::Instant());  // OK
         * }
         * 
         * // 2. Utiliser HasScene() avant d'accéder à GetCurrent()
         * void UpdateUI() {
         *     if (GetSceneManager().HasScene()) {  // ✅ Guard safe
         *         auto* scene = GetSceneManager().GetCurrent();
         *         UpdateSceneUI(scene);
         *     }
         *     // else : UI par défaut ou écran "Aucune scène"
         * }
         * 
         * // 3. Toujours appeler Update(dt) dans la boucle principale
         * void GameLoop::Run() {
         *     while (mRunning) {
         *         float dt = GetDeltaTime();
         *         
         *         // ✅ Mise à jour des transitions animées
         *         mSceneManager.Update(dt);
         *         
         *         // ... autres systèmes ...
         *     }
         * }
         * 
         * // 4. Utiliser des transitions adaptées au contexte
         * // • Instant pour debug/tests
         * // • Fade(0.5f) pour l'expérience joueur normale
         * // • WithLoading pour les niveaux lourds
         * void LoadLevelSmart(const NkString& levelName, bool isHeavy) {
         *     auto transition = isHeavy
         *         ? NkSceneTransition::WithLoading("LoadingScreen", 0.3f)
         *         : NkSceneTransition::Fade(0.5f);
         *     GetSceneManager().LoadScene(levelName, transition);
         * }
         * 
         * // 5. Gérer les erreurs de chargement avec fallback
         * bool LoadWithFallback(const NkString& scene) {
         *     if (!GetSceneManager().LoadScene(scene)) {
         *         logger.Error("Échec chargement '{}' → fallback", scene.CStr());
         *         return GetSceneManager().LoadScene("MainMenu");  // Fallback safe
         *     }
         *     return true;
         * }
         * 
         * 
         * // ❌ PIÈGES À ÉVITER
         * 
         * // 1. Charger une scène non enregistrée (crash ou comportement indéfini)
         * // ❌ mgr.LoadScene("TyopLevel");  // Orthographe incorrecte → nullptr factory
         * // ✅ Toujours vérifier IsSceneRegistered() ou utiliser LoadWithFallback()
         * 
         * // 2. Appeler LoadScene depuis un thread secondaire sans synchronisation
         * // ❌ NkThread([]() { mgr.LoadScene("Level"); });  // Race condition !
         * // ✅ Utiliser un événement ou une queue pour reporter au thread principal :
         * //    EventQueue::Post([sceneName]() {
         * //        GetSceneManager().LoadScene(sceneName);
         * //    });
         * 
         * // 3. Oublier d'appeler Update(dt) → transitions animées bloquées
         * // ❌ while (running) { /* pas d'appel à mgr.Update() */ }
         * // ✅ Toujours appeler mgr.Update(dt) dans la boucle principale
         * 
         * // 4. Stocker un pointeur vers GetCurrent() au-delà de sa validité
         * // ❌ NkSceneGraph* cached = mgr.GetCurrent();
         * //    mgr.LoadScene("Other");  // cached devient dangling !
         * // ✅ Toujours re-résoudre via GetCurrent() ou vérifier HasScene() avant usage
         * 
         * // 5. Utiliser une scène de chargement lourde (défait son objectif)
         * // ❌ LoadingScene avec 100 assets à charger → temps de chargement doublé
         * // ✅ LoadingScene minimaliste : UI simple + barre de progression
         * //    Le chargement réel se fait en background via NkAsyncLoader
         * @endcode
         */

        /** @} */ // end of NkSceneManager_Examples group

    } // namespace ecs
} // namespace nkentseu