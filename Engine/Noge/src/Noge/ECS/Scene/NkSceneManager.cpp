// =============================================================================
// Scene/NkSceneManager.cpp — Implémentations non-template de NkSceneManager
// =============================================================================
/**
 * @file NkSceneManager.cpp
 * @brief Implémentation des méthodes non-template de NkSceneManager.
 * 
 * Ce fichier contient :
 * - La logique principale de LoadScene() (chargement avec transitions)
 * - Les méthodes de gestion interne (UnloadCurrent, LoadLoadingScene, etc.)
 * - La recherche de factory par nom (FindFactory)
 * 
 * 🔹 Pourquoi séparer ?
 * - Réduction du temps de compilation : la logique complexe de chargement
 *   n'est recompilée que si ce .cpp change, pas à chaque modification du header
 * - Encapsulation : les détails d'implémentation du flux de chargement
 *   sont cachés aux utilisateurs du header
 * - Maintenance : plus facile de profiler/déboguer LoadScene() dans un fichier dédié
 * 
 * @note Les méthodes simples (GetCurrent, Pause, Resume, etc.) restent
 *       dans le header pour performance — leur corps est trivial.
 */

#include "NkSceneManager.h"

// Logger global pour les messages d'erreur/info — à adapter à votre système de logging
extern nkentseu::logger::NkLogger logger;

namespace nkentseu {
    namespace ecs {

        // =====================================================================
        // 🔍 Recherche de factory par nom
        // =====================================================================

        /**
         * @brief Trouve la factory enregistrée pour un nom de scène donné.
         * @param name Nom de la scène à rechercher.
         * @return Pointeur vers la factory si trouvée, nullptr sinon.
         * 
         * 🔹 Algorithme :
         *    • Parcours linéaire de mEntries[0..Size-1]
         *    • Comparaison de NkString via operator== (case-sensitive)
         *    • Retour immédiat à la première correspondance
         * 
         * 🔹 Complexité : O(n) où n = nombre de scènes enregistrées.
         *          Typiquement n < 20 → impact négligeable.
         *          Pour des centaines de scènes, envisager un hash map.
         * 
         * @note Retourne un pointeur vers la factory stockée dans mEntries —
         *       la durée de vie du pointeur est liée à celle du NkSceneManager.
         *       Ne pas le stocker au-delà de l'appel courant.
         */
        NkSceneFactory* NkSceneManager::FindFactory(const NkString& name) noexcept {
            for (nk_usize i = 0; i < mEntries.Size(); ++i) {
                if (mEntries[i].name == name) {
                    return &mEntries[i].factory;
                }
            }
            return nullptr;
        }

        // =====================================================================
        // 🗑️ Déchargement de la scène courante
        // =====================================================================

        /**
         * @brief Décharge proprement la scène actuellement active.
         * 
         * 🔹 Étapes détaillées :
         * 
         * Étape 1 : Notification de fin de scène
         *    • Appelle mCurrentScene->EndPlay()
         *    • Le script de scène peut sauvegarder son état, libérer des ressources
         * 
         * Étape 2 : Flush des opérations différées
         *    • Appelle mWorld.FlushDeferred()
         *    • Garantit que toutes les destructions/AddDeferred/RemoveDeferred
         *      planifiées pendant EndPlay() sont effectivement appliquées
         * 
         * Étape 3 : Libération mémoire
         *    • Delete de mCurrentScene (appelle le destructeur de NkSceneGraph)
         *    • Le destructeur de NkSceneGraph appelle à son tour EndPlay()
         *      en fallback — redondant mais safe
         * 
         * Étape 4 : Reset de l'état interne
         *    • mCurrentScene = nullptr
         *    • mCurrentName = NkString{} (vide)
         * 
         * 🔹 Sécurité :
         *    • Vérifie mCurrentScene != nullptr avant toute opération
         *    • Silencieusement ignoré si aucune scène n'est chargée
         * 
         * @note Cette méthode est privée — appelée uniquement par LoadScene()
         *       et le destructeur ~NkSceneManager().
         */
        void NkSceneManager::UnloadCurrent() noexcept {
            if (mCurrentScene) {
                // ── Étape 1 : Notification de fin de scène ──
                mCurrentScene->EndPlay();

                // ── Étape 2 : Flush des opérations différées ──
                // Essentiel pour appliquer les destructions planifiées dans EndPlay()
                mWorld.FlushDeferred();

                // ── Étape 3 : Libération mémoire ──
                delete mCurrentScene;

                // ── Étape 4 : Reset de l'état interne ──
                mCurrentScene = nullptr;
                mCurrentName = NkString{};
            }
        }

        // =====================================================================
        // 🎬 Gestion de la scène de chargement
        // =====================================================================

        /**
         * @brief Charge et active la scène de chargement temporaire.
         * @param name Nom de la scène de chargement à activer.
         * 
         * 🔹 Préconditions :
         *    • 'name' doit correspondre à une scène enregistrée via Register()
         *    • Aucune scène de chargement ne doit être déjà active
         * 
         * 🔹 Étapes :
         *    1. Recherche de la factory via FindFactory(name)
         *    2. Si trouvée : création de la scène via (*factory)(mWorld)
         *    3. Stockage dans mLoadingScene
         *    4. Appel immédiat de BeginPlay() pour initialisation
         * 
         * @note La scène de chargement est affichée "par-dessus" la scène courante
         *       (qui est en pause). Elle doit donc être légère et ne pas dépendre
         *       d'assets longs à charger.
         * 
         * @warning Si la factory retourne nullptr, la méthode retourne silencieusement.
         *          Le chargement de la scène cible continuera, mais sans écran
         *          de chargement visible — potentiellement une expérience utilisateur
         *          dégradée (écran noir ou gelé).
         */
        void NkSceneManager::LoadLoadingScene(const NkString& name) noexcept {
            NkSceneFactory* f = FindFactory(name);
            if (!f || !(*f)) {
                // Factory non trouvée ou nulle — log en debug si nécessaire
                // logger.Debug("Loading scene '{}' not found or factory null", name.CStr());
                return;
            }

            // Création de la scène de chargement
            mLoadingScene = (*f)(mWorld);
            if (mLoadingScene) {
                // Initialisation immédiate
                mLoadingScene->BeginPlay();
            }
        }

        /**
         * @brief Décharge la scène de chargement temporaire.
         * 
         * 🔹 Étapes :
         *    1. Si mLoadingScene existe :
         *       • Appelle EndPlay() pour nettoyage du script
         *       • FlushDeferred() pour appliquer les destructions
         *       • Delete pour libération mémoire
         *    2. Reset de mLoadingScene à nullptr
         * 
         * @note Appelée après que la scène cible a commencé son BeginPlay(),
         *       pour une transition fluide sans "saut" visuel.
         * 
         * @warning Silencieusement ignoré si mLoadingScene == nullptr —
         *          permet d'appeler UnloadLoadingScene() sans vérification préalable.
         */
        void NkSceneManager::UnloadLoadingScene() noexcept {
            if (mLoadingScene) {
                // Nettoyage du script de chargement
                mLoadingScene->EndPlay();

                // Application des opérations différées
                mWorld.FlushDeferred();

                // Libération mémoire
                delete mLoadingScene;
                mLoadingScene = nullptr;
            }
        }

        // =====================================================================
        // 🚀 Chargement principal d'une scène
        // =====================================================================

        /**
         * @brief Charge une scène enregistrée avec gestion complète des transitions.
         * @param name Nom de la scène à charger.
         * @param transition Configuration de la transition (type, durée, loading).
         * @return true si le chargement a réussi, false en cas d'erreur.
         * 
         * 🔹 Flux détaillé avec gestion d'erreurs :
         * 
         * Étape 0 : Validation de l'entrée
         *    • Recherche de la factory via FindFactory(name)
         *    • Si nullptr ou factory nulle :
         *      - Log d'erreur via logger.Errorf()
         *      - Retour false sans modifier l'état courant
         * 
         * Étape 1 : Déchargement de la scène courante
         *    • Appel de UnloadCurrent() (EndPlay + Flush + delete)
         *    • Garantit un état propre avant création de la nouvelle scène
         * 
         * Étape 2 : Activation de la scène de chargement (si nécessaire)
         *    • Condition : transition.type != Instant ET !transition.loadingScene.IsEmpty()
         *    • Appel de LoadLoadingScene(transition.loadingScene)
         *    • La scène de chargement est affichée pendant le chargement de la cible
         * 
         * Étape 3 : Création de la nouvelle scène
         *    • Appel de la factory : (*factory)(mWorld) → retourne NkSceneGraph*
         *    • Si nullptr :
         *      - Log d'erreur
         *      - Retour false (la scène précédente est déjà déchargée — état dégradé)
         *    • Sinon :
         *      - Stockage dans mCurrentScene et mCurrentName
         * 
         * Étape 4 : Initialisation de la nouvelle scène
         *    • Appel de mCurrentScene->BeginPlay()
         *    • Le script de scène reçoit son point d'entrée
         *    • Toute logique d'initialisation gameplay est exécutée ici
         * 
         * Étape 5 : Désactivation de la scène de chargement
         *    • Appel de UnloadLoadingScene() si elle était active
         *    • Transition visuelle finale vers la scène cible
         * 
         * Étape 6 : Logging de succès
         *    • logger.Infof() pour tracer le chargement réussi
         *    • Retour true pour indiquer le succès à l'appelant
         * 
         * 🔹 Gestion mémoire :
         *    • mCurrentScene et mLoadingScene sont des raw pointers owned
         *    • La destruction est gérée par UnloadCurrent/UnloadLoadingScene
         *    • Aucune fuite mémoire si les factories respectent le contrat (new → return)
         * 
         * @warning Cette méthode est synchrone et bloquante :
         *          • Le thread appelant est suspendu pendant toute la durée
         *          • Pour du chargement asynchrone, utiliser une scène de chargement
         *            avec un système de background loading (NkAsyncLoader)
         * 
         * @note En cas d'erreur après UnloadCurrent() (ex: factory retourne nullptr),
         *       l'application se retrouve sans scène active. C'est un état dégradé
         *       mais contrôlé — l'appelant peut décider de charger une scène de
         *       fallback (ex: "MainMenu") ou de quitter proprement.
         */
        bool NkSceneManager::LoadScene(const NkString& name,
                                       const NkSceneTransition& transition) noexcept {
            // ── Étape 0 : Validation de l'entrée ──
            NkSceneFactory* factory = FindFactory(name);
            if (!factory || !(*factory)) {
                logger.Errorf(
                    "[NkSceneManager] Scène inconnue ou factory nulle : {}\n",
                    name.CStr()
                );
                return false;
            }

            // ── Étape 1 : Déchargement de la scène courante ──
            UnloadCurrent();

            // ── Étape 2 : Scène de chargement (si transition animée) ──
            if (transition.type != NkTransitionType::Instant &&
                !transition.loadingScene.IsEmpty()) {
                LoadLoadingScene(transition.loadingScene);
            }

            // ── Étape 3 : Création de la nouvelle scène ──
            mCurrentScene = (*factory)(mWorld);
            if (!mCurrentScene) {
                logger.Errorf(
                    "[NkSceneManager] Factory a retourné nullptr pour la scène : {}\n",
                    name.CStr()
                );
                // État dégradé : pas de scène active
                // L'appelant peut charger un fallback si nécessaire
                return false;
            }
            mCurrentName = name;

            // ── Étape 4 : Initialisation via BeginPlay ──
            // Le script de scène reçoit son point d'entrée ici
            mCurrentScene->BeginPlay();

            // ── Étape 5 : Fermeture de la scène de chargement ──
            UnloadLoadingScene();

            // ── Étape 6 : Logging de succès ──
            logger.Infof("[NkSceneManager] Scène chargée avec succès : {}\n", name.CStr());
            return true;
        }

        // =====================================================================
        // 🔁 Rechargement de la scène courante
        // =====================================================================

        /**
         * @brief Recharge la scène actuellement active (utile pour restart de niveau).
         * @return true si le rechargement a réussi, false si aucune scène active.
         * 
         * 🔹 Comportement :
         *    • Si mCurrentName est vide : retour false immédiatement
         *    • Sinon : délègue à LoadScene(mCurrentName, NkSceneTransition::Instant())
         * 
         * @note Utilise une transition Instant par défaut pour un restart rapide.
         *       Pour une transition animée, appeler LoadScene() directement avec
         *       le NkSceneTransition souhaité.
         * 
         * @warning Le rechargement décharge d'abord la scène courante —
         *          toute donnée non sauvegardée dans OnEndPlay() sera perdue.
         *          Pour un "soft reset" sans perte de progression, envisager
         *          une méthode ResetState() dans le script de scène plutôt
         *          qu'un rechargement complet.
         */
        bool NkSceneManager::ReloadCurrent() noexcept {
            if (mCurrentName.IsEmpty()) {
                return false;
            }
            // Recharge avec transition instantanée par défaut
            return LoadScene(mCurrentName, NkSceneTransition::Instant());
        }

        // =====================================================================
        // 📦 Implémentations des méthodes simples
        // =====================================================================

        // ── Constructeur / Destructeur ─────────────────────────────────────
        NkSceneManager::NkSceneManager(NkWorld& world) noexcept
            : mWorld(world)
            , mEntries()
            , mCurrentScene(nullptr)
            , mCurrentName()
            , mLoadingScene(nullptr)
            , mLoadingSceneName() {
        }

        NkSceneManager::~NkSceneManager() noexcept {
            UnloadCurrent();
        }

        // ── Enregistrement ─────────────────────────────────────────────────
        void NkSceneManager::Register(const NkString& name,
                                              NkSceneFactory factory) noexcept {
            // Recherche d'une entrée existante pour mise à jour
            for (nk_usize i = 0; i < mEntries.Size(); ++i) {
                if (mEntries[i].name == name) {
                    mEntries[i].factory = static_cast<NkSceneFactory&&>(factory);
                    return;
                }
            }
            // Création d'une nouvelle entrée
            Entry e;
            e.name = name;
            e.factory = static_cast<NkSceneFactory&&>(factory);
            mEntries.PushBack(static_cast<Entry&&>(e));
        }

        void NkSceneManager::SetLoadingScene(const NkString& name) noexcept {
            mLoadingSceneName = name;
        }

        // ── Accès ──────────────────────────────────────────────────────────
        NkSceneGraph* NkSceneManager::GetCurrent() noexcept {
            return mCurrentScene;
        }

        const NkSceneGraph* NkSceneManager::GetCurrent() const noexcept {
            return mCurrentScene;
        }

        const NkString& NkSceneManager::GetCurrentName() const noexcept {
            return mCurrentName;
        }

        bool NkSceneManager::HasScene() const noexcept {
            return mCurrentScene != nullptr;
        }

        // ── Pause / Resume ─────────────────────────────────────────────────
        void NkSceneManager::Pause() noexcept {
            if (mCurrentScene) {
                mCurrentScene->Pause();
            }
        }

        void NkSceneManager::Resume() noexcept {
            if (mCurrentScene) {
                mCurrentScene->Resume();
            }
        }

        // ── Update (transitions animées) ───────────────────────────────────
        void NkSceneManager::Update(nk_float32 dt) noexcept {
            // TODO Phase 5 : implémenter l'animation des fondus
            // Exemple pseudo-code :
            // if (mTransitionActive) {
            //     mTransitionAlpha += dt / mTransitionDuration;
            //     if (mTransitionAlpha >= 1.f) {
            //         FinalizeTransition();
            //     }
            //     renderer.SetFadeAlpha(mTransitionAlpha);
            // }
            (void)dt;  // Supprime warning unused parameter en attendant l'implémentation
        }

    } // namespace ecs
} // namespace nkentseu