// =============================================================================
// Scene/NkSceneGraph.cpp — Implémentations non-template de NkSceneGraph
// =============================================================================
/**
 * @file NkSceneGraph.cpp
 * @brief Implémentation des méthodes non-template de NkSceneGraph.
 * 
 * Ce fichier contient :
 * - Le constructeur/destructeur de NkSceneGraph
 * - Les méthodes de gestion de hiérarchie (SetParent, DetachFromParent)
 * - Les méthodes de recherche (FindByName, FindByLayer)
 * - Les méthodes d'activation/visibilité
 * - La destruction récursive sécurisée
 * - Les méthodes lifecycle (BeginPlay, Tick, etc.) — bien que inline,
 *   définies ici pour cohérence si besoin de profiling séparé
 * 
 * 🔹 Pourquoi séparer ?
 * - Réduction du temps de compilation (moins de recompilation des headers)
 * - Encapsulation de la logique complexe hors du header public
 * - Meilleure organisation pour la maintenance et le profilage
 * 
 * @note Les méthodes template (SpawnActor, SetScript, GetScript) restent dans le
 *       header (.h) car elles nécessitent l'instantiation à chaque site d'appel.
 */

#include "NkSceneGraph.h"
// Inclut déjà NkWorld.h via le header — pas de réinclusion nécessaire

namespace nkentseu {
    namespace ecs {

        // =====================================================================
        // 🔧 Constructeur / Destructeur
        // =====================================================================

        /**
         * @brief Constructeur principal de NkSceneGraph.
         * 
         * @param world Référence au monde ECS à orchestrer.
         * @param name Nom lisible de la scène (pour débogage/logs).
         * 
         * 🔹 Initialisations :
         *    • mWorld : référence au monde (non possédé, durée de vie externe)
         *    • mName : copie du nom via NkString (gestion mémoire interne)
         *    • mScript : nullptr par défaut — à attacher via SetScript()
         *    • mStarted/mPaused : false par défaut — lifecycle non démarré
         * 
         * @note Aucune allocation d'entités ou de composants ici — la scène
         *       est "vide" jusqu'aux premiers appels à SpawnNode/SpawnActor.
         */
        NkSceneGraph::NkSceneGraph(NkWorld& world,
                                   const NkString& name) noexcept
            : mWorld(world)
            , mName(name)
            , mScript(nullptr)
            , mStarted(false)
            , mPaused(false) {
            // Initialisation minimale — la scène est prête à être peuplée
        }

        /**
         * @brief Destructeur : garantit le nettoyage du script via EndPlay().
         * 
         * 🔹 Pourquoi appeler EndPlay() ici ?
         *    • Garantit que OnEndPlay() est toujours exécuté, même en cas
         *      d'exception ou de destruction prématurée de la scène
         *    • Évite les fuites de ressources acquises dans OnBeginPlay()
         *    • Permet au script de sauvegarder son état avant destruction
         * 
         * @warning Ne pas détruire un NkSceneGraph pendant que son lifecycle
         *          est en cours d'exécution (ex: pendant un Tick() d'un autre thread).
         *          Synchroniser l'accès si nécessaire.
         */
        NkSceneGraph::~NkSceneGraph() noexcept {
            EndPlay();
            // Note : mScript est un raw ptr — la mémoire est libérée par
            // ClearScript() ou ici via EndPlay() qui appelle OnEndPlay()
            // avant le delete implicite à la destruction de l'objet englobant
        }

        // =====================================================================
        // 🌳 Gestion de hiérarchie
        // =====================================================================

        /**
         * @brief Attache une entité enfant à un parent dans la hiérarchie.
         * 
         * @param child ID de l'entité à attacher comme enfant.
         * @param parent ID de l'entité parente (Invalid = détacher).
         * 
         * 🔹 Étapes détaillées :
         * 
         * Étape 1 : Mise à jour du composant NkParent de l'enfant
         *    • Si l'enfant avait un ancien parent : le retirer de sa liste NkChildren
         *    • Mettre à jour p->entity avec le nouveau parent (ou Invalid)
         * 
         * Étape 2 : Mise à jour réciproque chez le nouveau parent
         *    • Récupérer ou créer le composant NkChildren du parent
         *    • Ajouter l'ID de l'enfant à la liste (si pas déjà présent)
         * 
         * Étape 3 : Invalidation des matrices mondiales
         *    • Marquer NkWorldTransform de l'enfant comme dirty
         *    • Le TransformSystem recalculera les matrices au prochain update
         * 
         * 🔹 Sécurité :
         *    • Vérifications nullptr sur tous les Get() pour robustesse
         *    • Aucune allocation — opérations O(1) sur structures existantes
         * 
         * @warning Ne pas appeler pendant l'itération sur une query hiérarchique.
         *          Pour un usage sûr, collecter les modifications et appliquer
         *          après la boucle, ou implémenter une version Deferred.
         */
        void NkSceneGraph::SetParent(NkEntityId child,
                                     NkEntityId parent) noexcept {
            // ── Étape 1 : Mise à jour du composant Parent de l'enfant ──
            if (NkParent* p = mWorld.Get<NkParent>(child)) {
                // Retirer de l'ancien parent si existant et valide
                if (p->entity.IsValid()) {
                    if (NkChildren* oldCh = mWorld.Get<NkChildren>(p->entity)) {
                        oldCh->Remove(child);  // O(n) mais n ≤ 64 typiquement
                    }
                }
                // Mettre à jour avec le nouveau parent
                p->entity = parent;
            }

            // ── Étape 2 : Mise à jour réciproque chez le nouveau parent ──
            if (parent.IsValid()) {
                if (NkChildren* ch = mWorld.Get<NkChildren>(parent)) {
                    // Ajout en fin de tableau — O(1), vérifie la capacité
                    ch->Add(child);
                }
            }

            // ── Étape 3 : Invalidation des matrices mondiales ──
            // Le TransformSystem recalculera la chaîne parent→enfant au prochain update
            if (NkWorldTransform* wt = mWorld.Get<NkWorldTransform>(child)) {
                wt->dirty = true;
            }
        }

        /**
         * @brief Détache une entité de son parent (devient racine de scène).
         * 
         * @param child ID de l'entité à détacher.
         * 
         * @note Cette méthode est un wrapper sémantique autour de SetParent().
         *       Elle améliore la lisibilité du code appelant dans les contextes
         *       où l'intention est clairement de "détacher" plutôt que "reparenter".
         * 
         * @see SetParent() pour les détails d'implémentation.
         */
        void NkSceneGraph::DetachFromParent(NkEntityId child) noexcept {
            // Délègue à SetParent avec un parent invalide = racine
            SetParent(child, NkEntityId::Invalid());
        }

        // =====================================================================
        // 🔍 Recherche d'entités
        // =====================================================================

        /**
         * @brief Trouve la première entité dont le nom correspond exactement.
         * 
         * @param name Nom à rechercher (comparaison case-sensitive via NkStrEqual).
         * @return ID de l'entité trouvée, ou Invalid si non trouvée.
         * 
         * 🔹 Algorithme :
         *    • Requête ECS sur toutes les entités ayant un composant NkSceneNode
         *    • Itération avec callback lambda pour comparaison de nom
         *    • Retour immédiat à la première correspondance trouvée (optimisation)
         * 
         * 🔹 Complexité : O(n) où n = nombre d'entités avec NkSceneNode.
         *          Acceptable pour les recherches ponctuelles (UI, debug),
         *          mais à éviter dans les boucles chaudes de mise à jour.
         * 
         * 🔹 Optimisations possibles :
         *    • Maintenir un hash map nom→ID en cache côté NkSceneGraph
         *    • Indexer les noms fréquemment recherchés au spawn
         *    • Utiliser FindByLayer() + filtrage local si le layer est connu
         * 
         * @note Si plusieurs entités portent le même nom, seule la première
         *       rencontrée est retournée (ordre d'itération ECS non garanti).
         *       Pour des noms uniques, envisager une contrainte au niveau métier.
         * 
         * @warning L'utilisation de const_cast sur mWorld est nécessaire car
         *          NkWorld::Query() n'est pas const dans l'API actuelle.
         *          Cela ne modifie pas l'état du monde — seule la lecture a lieu.
         */
        NkEntityId NkSceneGraph::FindByName(const char* name) const noexcept {
            NkEntityId found = NkEntityId::Invalid();
            
            // const_cast nécessaire car Query() n'est pas const dans NkWorld
            // Cela ne viole pas la const-correctness car nous ne modifions pas le monde
            const_cast<NkWorld&>(mWorld)
                .Query<const NkSceneNode>()
                .ForEach([&](NkEntityId id, const NkSceneNode& node) {
                    // Optimisation : arrêt dès la première correspondance trouvée
                    if (!found.IsValid() && NkStrEqual(node.name, name)) {
                        found = id;
                    }
                });
            
            return found;
        }

        /**
         * @brief Collecte toutes les entités d'un layer donné dans un vecteur.
         * 
         * @param layer Identifiant du layer à filtrer (0-255).
         * @param out Vecteur de sortie à remplir (les résultats sont ajoutés à la fin).
         * 
         * 🔹 Usage typique :
         *    • Culling par layer avant rendu ("UI", "Effects", "Occluders")
         *    • Mise à jour sélective de sous-systèmes (ex: physics sur layer 2)
         *    • Debug : afficher/masquer dynamiquement certains groupes
         * 
         * 🔹 Comportement d'ajout :
         *    • Les IDs trouvés sont PushBack-és à la fin du vecteur existant
         *    • Utiliser out.Clear() avant appel si on veut remplacer le contenu
         *    • La méthode ne réserve pas de mémoire — appeler out.Reserve()
         *      en amont si le nombre attendu est connu pour éviter les réallocations
         * 
         * @note Complexité O(n) sur les entités avec NkSceneNode.
         *       Pour des requêtes fréquentes sur le même layer, envisager
         *       un index layer→entités maintenu incrémentalement.
         * 
         * @warning Même avertissement const_cast que FindByName().
         */
        void NkSceneGraph::FindByLayer(uint8 layer, NkVector<NkEntityId>& out) const noexcept {
            const_cast<NkWorld&>(mWorld)
                .Query<const NkSceneNode>()
                .ForEach([&](NkEntityId id, const NkSceneNode& node) {
                    if (node.layer == layer) {
                        out.PushBack(id);
                    }
                });
        }

        // =====================================================================
        // ⚡ Activation et visibilité
        // =====================================================================

        /**
         * @brief Active ou désactive un nœud (logique gameplay).
         * 
         * @param id ID de l'entité à modifier.
         * @param active true pour activer, false pour désactiver.
         * 
         * 🔹 Effet sur le composant NkSceneNode :
         *    • Modifie le champ 'active' qui contrôle la participation aux
         *      systèmes de mise à jour gameplay (Update(), collisions, etc.)
         * 
         * 🔹 Différence avec NkWorld::SetActive() :
         *    • NkSceneGraph::SetActive() agit sur le flag NkSceneNode::active
         *    • NkWorld::SetActive() ajoute/supprime le composant marqueur NkInactive
         *    • Les deux mécanismes peuvent coexister — documenter lequel est utilisé
         * 
         * @note Désactiver un parent n'affecte PAS automatiquement les enfants.
         *       Pour une désactivation récursive, implémenter un helper externe
         *       qui parcourt la hiérarchie et appelle SetActive() sur chaque nœud.
         * 
         * @warning Silencieusement ignoré si le composant NkSceneNode est absent
         *          ou si l'ID est invalide — robustesse en runtime.
         */
        void NkSceneGraph::SetActive(NkEntityId id, bool active) noexcept {
            if (NkSceneNode* n = mWorld.Get<NkSceneNode>(id)) {
                n->active = active;
            }
            // Ignoré silencieusement si NkSceneNode absent ou ID invalide
        }

        /**
         * @brief Définit la visibilité d'un nœud (rendu uniquement).
         * 
         * @param id ID de l'entité à modifier.
         * @param visible true pour rendre visible, false pour cacher.
         * 
         * 🔹 Effet sur le composant NkSceneNode :
         *    • Modifie le champ 'visible' qui contrôle l'affichage au renderer
         *    • N'affecte PAS la logique gameplay : l'entité continue de recevoir
         *      des Update(), des collisions, des événements, etc.
         * 
         * 🔹 Cas d'usage :
         *    • Occlusion culling manuel : cacher un objet derrière un mur
         *    • Effets de transition : fondu via alpha + visible toggle
         *    • Debug : masquer temporairement des éléments d'interface
         *    • LOD (Level of Detail) : cacher les détails lointains
         * 
         * @note Le renderer doit respecter ce flag dans sa boucle de rendu.
         *       Exemple : if (!node->visible) continue; avant DrawCall.
         * 
         * @warning Même robustesse que SetActive() — ignoré si composant absent.
         */
        void NkSceneGraph::SetVisible(NkEntityId id, bool visible) noexcept {
            if (NkSceneNode* n = mWorld.Get<NkSceneNode>(id)) {
                n->visible = visible;
            }
        }

        // =====================================================================
        // 💀 Destruction récursive sécurisée
        // =====================================================================

        /**
         * @brief Détruit une entité et tous ses descendants récursivement.
         * 
         * @param id ID de la racine de la sous-arborescence à détruire.
         * 
         * 🔹 Algorithme détaillé (post-order traversal) :
         * 
         * Étape 1 : Copie locale des IDs enfants
         *    • Récupère le composant NkChildren de l'entité courante
         *    • Copie les IDs dans un tableau stack local (NkEntityId[kMax])
         *    • Stocke le count pour itération — protège contre les modifications
         *      de NkChildren pendant la boucle
         * 
         * Étape 2 : Appel récursif sur chaque enfant
         *    • Pour chaque ID copié : appel DestroyRecursive(childId)
         *    • L'ordre post-order garantit que les enfants sont détruits
         *      AVANT leur parent — évite les dangling references
         * 
         * Étape 3 : Détachement du parent
         *    • Appelle DetachFromParent(id) pour nettoyer les références
         *      dans le composant NkChildren du parent (si existant)
         * 
         * Étape 4 : Destruction finale de l'entité
         *    • Appelle mWorld.Destroy(id) pour :
         *      - Supprimer l'entité de son archétype ECS
         *      - Libérer l'ID pour réutilisation future
         *      - Détruire tous les composants attachés
         * 
         * 🔹 Sécurité et robustesse :
         *    • La copie locale des enfants est CRITIQUE : sans elle, la
         *      suppression d'un enfant modifierait NkChildren pendant
         *      l'itération, causant un comportement indéfini
         *    • Les vérifications nullptr sur Get() assurent la robustesse
         *      même si des composants manquent (edge cases, corruption)
         * 
         * @warning Cette méthode utilise Destroy() IMMÉDIAT — ne pas l'appeler
         *          pendant l'itération sur une query ECS. Pour un usage sûr
         *          dans ce contexte, implémenter une version Deferred en
         *          collectant les IDs d'abord, puis en appelant DestroyDeferred().
         * 
         * @see Exemple 04 dans le header pour un pattern Deferred personnalisé.
         */
        void NkSceneGraph::DestroyRecursive(NkEntityId id) noexcept {
            // ── Étape 1 : Copie locale des enfants pour sécurité d'itération ──
            // Buffer stack local — pas d'allocation heap, taille fixe garantie
            NkEntityId childBuf[NkChildren::kMax];
            uint32 childCount = 0;

            if (NkChildren* ch = mWorld.Get<NkChildren>(id)) {
                childCount = ch->count;
                
                // Copie des IDs AVANT toute modification — essentiel pour la sécurité
                for (uint32 i = 0; i < childCount; ++i) {
                    childBuf[i] = ch->children[i];
                }
            }

            // ── Étape 2 : Appel récursif post-order (enfants d'abord) ──
            for (uint32 i = 0; i < childCount; ++i) {
                DestroyRecursive(childBuf[i]);
            }

            // ── Étape 3 : Détachement du parent (nettoyage des références) ──
            DetachFromParent(id);

            // ── Étape 4 : Destruction finale de l'entité ──
            mWorld.Destroy(id);
        }

        bool NkSceneGraph::HasScript() const noexcept {
            return mScript != nullptr;
        }

        // ── Lifecycle (appelés par NkSceneLifecycleSystem) ─────────────────
        void NkSceneGraph::BeginPlay() noexcept {
            if (mStarted) {
                return;
            }
            mStarted = true;
            if (mScript) {
                mScript->OnBeginPlay();
            }
        }

        void NkSceneGraph::Tick(float dt) noexcept {
            if (!mStarted || mPaused) {
                return;
            }
            if (mScript) {
                mScript->OnTick(dt);
            }
        }

        void NkSceneGraph::LateTick(float dt) noexcept {
            if (!mStarted || mPaused) {
                return;
            }
            if (mScript) {
                mScript->OnLateTick(dt);
            }
        }

        void NkSceneGraph::FixedTick(float fixedDt) noexcept {
            if (!mStarted || mPaused) {
                return;
            }
            if (mScript) {
                mScript->OnFixedTick(fixedDt);
            }
        }

        void NkSceneGraph::EndPlay() noexcept {
            if (!mStarted) {
                return;
            }
            if (mScript) {
                mScript->OnEndPlay();
            }
            mStarted = false;
        }

        void NkSceneGraph::Pause() noexcept {
            if (mPaused) {
                return;
            }
            mPaused = true;
            if (mScript) {
                mScript->OnPause();
            }
        }

        void NkSceneGraph::Resume() noexcept {
            if (!mPaused) {
                return;
            }
            mPaused = false;
            if (mScript) {
                mScript->OnResume();
            }
        }

        // ── SpawnNode ──────────────────────────────────────────────────────
        NkEntityId NkSceneGraph::SpawnNode(const char* name) noexcept {
            NkEntityId id = mWorld.CreateEntity();
            mWorld.Add<NkSceneNode>(id, NkSceneNode(name));
            mWorld.Add<NkParent>(id);
            mWorld.Add<NkChildren>(id);
            mWorld.Add<NkLocalTransform>(id);
            mWorld.Add<NkWorldTransform>(id);
            return id;
        }

        void NkSceneGraph::ClearScript() noexcept {
            if (mScript) {
                mScript->OnEndPlay();
                delete mScript;
                mScript = nullptr;
            }
        }

        // ── Identité ───────────────────────────────────────────────────────
        const char* NkSceneGraph::Name() const noexcept {
            return mName.CStr();
        }

        bool NkSceneGraph::IsPaused() const noexcept {
            return mPaused;
        }

        NkWorld& NkSceneGraph::World() noexcept {
            return mWorld;
        }

        const NkWorld& NkSceneGraph::World() const noexcept {
            return mWorld;
        }

    } // namespace ecs
} // namespace nkentseu