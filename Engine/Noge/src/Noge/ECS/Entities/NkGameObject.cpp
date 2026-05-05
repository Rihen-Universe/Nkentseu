// =============================================================================
// GameObject/NkGameObject.cpp — Implémentations non-template de NkGameObject
// =============================================================================
/**
 * @file NkGameObject.cpp
 * @brief Implémentation des méthodes non-template de NkGameObject.
 * 
 * Ce fichier contient :
 * - Les constructeurs avec initialisation
 * - Les méthodes d'accès à l'identité et à la Transform
 * - La gestion de la hiérarchie (parent/enfants)
 * - Les méthodes d'activation et de destruction
 * 
 * 🔹 Pourquoi séparer ?
 * - Réduction du temps de compilation (moins de recompilation des headers)
 * - Encapsulation de la logique non-template hors du header public
 * - Meilleure organisation pour la maintenance et le débogage
 * 
 * @note Les méthodes template restent dans le header (.h) car elles nécessitent
 *       l'instantiation à chaque site d'appel (contrainte du langage C++).
 */

#include "NkGameObject.h"
#include "NKECS/World/NkWorld.h"  // Pour accès à NkWorld::Get/Add/etc.

namespace nkentseu {
    namespace ecs {

        // =====================================================================
        // 🔧 Constructeurs de NkGameObject
        // =====================================================================

        /**
         * @brief Constructeur liant un handle à une entité ECS existante.
         * 
         * @param id ID de l'entité ECS à encapsuler.
         * @param world Pointeur vers le monde contenant l'entité.
         * 
         * @note Ce constructeur est utilisé en interne par NkWorld.
         *       Les utilisateurs devraient préférer CreateGameObject().
         */
        NkGameObject::NkGameObject(NkEntityId id, NkWorld* world) noexcept : mId(id), mWorld(world) {
            // Initialisation minimale : le handle ne possède pas de donnée,
            // il référence simplement l'état dans le monde ECS.
        }

        /**
         * @brief Constructeur avec initialisation des composants de base.
         * 
         * @param name Nom lisible pour le débogage et l'identification.
         * @param id ID de l'entité ECS cible.
         * @param world Pointeur vers le monde ECS.
         * 
         * 🔹 Composants automatiquement ajoutés :
         *    • NkTransform : pour la position/rotation/scale dans l'espace monde
         *    • NkName : pour un nom lisible (différent de l'ID technique)
         *    • NkTag : pour le marquage/catégorisation (ex: "Enemy", "Interactive")
         * 
         * @note Ces composants sont considérés comme "essentiels" pour tout
         *       GameObject dans la plupart des cas d'usage gameplay.
         */
        NkGameObject::NkGameObject(const NkString& name, NkEntityId id, NkWorld* world) noexcept : mId(id), mWorld(world) {
            // Ajout des composants de base via l'API du monde
            // Ces appels utilisent AddImpl en interne (migration d'archétype si besoin)
            Add<NkTransform>();
            Add<NkName>(name);
            Add<NkTag>();
        }

        // =====================================================================
        // 🆔 Identité & Validité
        // =====================================================================

        /**
         * @brief Vérifie si le handle pointe vers une entité valide et vivante.
         * @return true si mWorld est non-null ET que l'entité est alive dans l'index.
         * 
         * 🔹 Deux conditions pour être "valide" :
         *    1. mWorld != nullptr (le handle a été correctement initialisé)
         *    2. mWorld->IsAlive(mId) == true (l'entité n'a pas été détruite)
         * 
         * @note Cette méthode est très légère (pas d'allocation, pas de syscall).
         *       À appeler systématiquement avant toute opération sur le GameObject.
         */
        bool NkGameObject::IsValid() const noexcept {
            return mWorld && mWorld->IsAlive(mId);
        }

        /**
         * @brief Retourne une référence au monde ECS associé.
         * @return Référence à NkWorld.
         * @warning Asserte en mode debug si mWorld est nullptr.
         * 
         * 🔹 Usage typique :
         *    • Accès à l'API ECS bas-niveau depuis un Behaviour
         *    • Requêtes ou opérations nécessitant le contexte du monde
         * 
         * @note En mode release, l'assert est compilée out — vérifier IsValid()
         *       en amont pour éviter les comportements indéfinis.
         */
        NkWorld& NkGameObject::World() const noexcept {
            NKECS_ASSERT(mWorld);
            return *mWorld;
        }

        /**
         * @brief Retourne le nom lisible de l'objet.
         * @return Pointeur vers une string C (jamais nullptr).
         * 
         * 🔹 Stratégie de fallback en cascade :
         *    1. Si handle invalide → "InvalidGO"
         *    2. Si composant NkName présent → retourne son contenu
         *    3. Sinon → "UnnamedGO"
         * 
         * @note La valeur de retour pointe vers la donnée interne du composant.
         *       Ne pas la stocker au-delà de la durée de vie de l'entité.
         */
        const char* NkGameObject::Name() const noexcept {
            if (!IsValid()) {
                return "InvalidGO";
            }
            if (const auto* n = mWorld->Get<NkName>(mId)) {
                return n->Get();
            }
            return "UnnamedGO";
        }

        /**
         * @brief Définit ou met à jour le nom de l'objet.
         * @param name Nouvelle valeur du nom (string C).
         * 
         * 🔹 Comportement :
         *    • Si NkName existe : mise à jour in-place de la valeur
         *    • Si NkName absent : l'appel est silencieusement ignoré
         *      (le nom de base est normalement défini à la création)
         * 
         * @note Pour ajouter un NkName à posteriori, utiliser Add<NkName>(...)
         *       via l'interface ECS bas-niveau si vraiment nécessaire.
         */
        void NkGameObject::SetName(const char* name) noexcept {
            if (auto* n = mWorld->Get<NkName>(mId)) {
                n->Set(name);
            }
        }

        // =====================================================================
        // 🌀 Transform : accès et modification
        // =====================================================================

        /**
         * @brief Récupère un pointeur vers le composant Transform.
         * @return Pointeur vers NkTransform, ou nullptr si absent/invalide.
         * 
         * 🔹 NkTransform est un singleton : une seule instance par entité.
         *    Cette méthode est donc un wrapper direct vers World().Get<NkTransform>().
         * 
         * @note Retourne nullptr si :
         *    • Le handle est invalide
         *    • L'entité n'a pas de composant NkTransform (rare, sauf création manuelle)
         */
        NkTransform* NkGameObject::Transform() noexcept {
            return mWorld ? mWorld->Get<NkTransform>(mId) : nullptr;
        }

        /**
         * @brief Version const de Transform().
         */
        const NkTransform* NkGameObject::Transform() const noexcept {
            return mWorld ? mWorld->Get<NkTransform>(mId) : nullptr;
        }

        /**
         * @brief Définit la position mondiale de l'objet.
         * @param pos Nouvelle position en coordonnées monde (NkVec3).
         * 
         * 🔹 Effets secondaires :
         *    • Mise à jour directe du champ position dans NkTransform
         *    • Marquage dirty = true pour signaler que les matrices mondes
         *      doivent être recalculées (propagation aux enfants)
         * 
         * @note Le recalcul effectif des matrices est généralement différé
         *       et batché via NkSceneGraph::UpdateTransforms() pour performance.
         */
        void NkGameObject::SetPosition(const NkVec3& pos) noexcept {
            if (auto* t = Transform()) {
                t->position = pos;
                t->dirty = true;
            }
        }

        /**
         * @brief Surcharge pratique pour définir la position composante par composante.
         * @param x Coordonnée X.
         * @param y Coordonnée Y.
         * @param z Coordonnée Z.
         * 
         * @note Appelle SetPosition(NkVec3{x,y,z}) en interne — zero overhead.
         */
        void NkGameObject::SetPosition(float32 x, float32 y, float32 z) noexcept {
            SetPosition({x, y, z});
        }

        /**
         * @brief Définit la rotation de l'objet via un quaternion.
         * @param rot Quaternion représentant la rotation.
         * 
         * 🔹 Pourquoi un quaternion ?
         *    • Évite le gimbal lock des angles d'Euler
         *    • Interpolation sphérique (slerp) fluide pour les animations
         *    • Composition de rotations stable et associative
         * 
         * @note Utiliser NkQuat::FromEuler(pitch, yaw, roll) pour convertir
         *       depuis des angles si nécessaire (attention à l'ordre des axes).
         */
        void NkGameObject::SetRotation(const NkQuat& rot) noexcept {
            if (auto* t = Transform()) {
                t->rotation = rot;
                t->dirty = true;
            }
        }

        /**
         * @brief Définit l'échelle (scale) de l'objet.
         * @param s Vecteur d'échelle (x, y, z).
         * 
         * @warning Les échelles négatives peuvent inverser le winding des faces
         *          et causer des problèmes de culling/backface — à éviter sauf
         *          pour des effets spécifiques (miroirs, etc.).
         */
        void NkGameObject::SetScale(const NkVec3& s) noexcept {
            if (auto* t = Transform()) {
                t->scale = s;
                t->dirty = true;
            }
        }

        // =====================================================================
        // 🌳 Hiérarchie : gestion parent/enfants
        // =====================================================================

        /**
         * @brief Définit le parent de cet objet dans la hiérarchie de scène.
         * @param parent GameObject à définir comme parent.
         * 
         * 🔹 Étapes détaillées :
         *    1. Validation : les deux handles doivent être valides
         *    2. Mise à jour du composant NkParent sur CET objet :
         *       • Si existe : mise à jour du champ entity
         *       • Si absent : ajout via World().Add<NkParent>()
         *    3. Mise à jour réciproque chez le parent :
         *       • Récupère ou crée son composant NkChildren
         *       • Ajoute l'ID de CET objet à la liste si pas déjà présent
         *    4. Marquage dirty de la Transform pour recalcul hiérarchique
         * 
         * 🔹 Propagation des transforms :
         *    • La position/rotation/scale d'un enfant est relative à son parent
         *    • Le recalcul mondial est différé et batché pour performance
         * 
         * @note Silencieusement ignoré si l'un des handles est invalide.
         *       Pas d'assert en release pour robustesse en runtime.
         */
        void NkGameObject::SetParent(const NkGameObject& parent) noexcept {
            if (!IsValid() || !parent.IsValid()) {
                return;
            }

            // Étape 2 : Mise à jour du composant Parent sur cette entité
            if (auto* p = mWorld->Get<NkParent>(mId)) {
                p->entity = parent.mId;
            } else {
                mWorld->Add<NkParent>(mId, NkParent{parent.mId});
            }

            // Étape 3 : Mise à jour réciproque chez le parent (liste Children)
            auto* ch = mWorld->Get<NkChildren>(parent.mId);
            if (!ch) {
                ch = &mWorld->Add<NkChildren>(parent.mId);
            }
            if (!ch->Has(mId)) {
                ch->Add(mId);
            }

            // Étape 4 : Marquage dirty pour propagation hiérarchique
            if (auto* t = Transform()) {
                t->dirty = true;
            }
        }

        /**
         * @brief Récupère le parent de cet objet dans la hiérarchie.
         * @return NkGameObject du parent, ou handle invalide si aucun parent.
         * 
         * 🔹 Deux cas de retour d'un handle invalide :
         *    1. Cet objet n'a pas de composant NkParent (racine de scène)
         *    2. Le composant existe mais pointe vers une entité invalide
         *       (parent détruit mais référence non nettoyée — edge case)
         * 
         * @note Toujours vérifier IsValid() sur le résultat avant utilisation.
         */
        NkGameObject NkGameObject::GetParent() const noexcept {
            if (!IsValid()) {
                return {};
            }
            if (const auto* p = mWorld->Get<NkParent>(mId)) {
                if (p->entity.IsValid()) {
                    return NkGameObject(p->entity, mWorld);
                }
            }
            return {};
        }

        /**
         * @brief Ajoute un enfant à cet objet (inverse de SetParent).
         * @param child GameObject à ajouter comme enfant.
         * 
         * @note Cette méthode est strictement équivalente à :
         *       child.SetParent(*this);
         * 
         *       Elle est fournie pour une API plus intuitive dans les contextes
         *       où l'on "construit" un arbre depuis la racine vers les feuilles.
         */
        void NkGameObject::AddChild(const NkGameObject& child) noexcept {
            if (!IsValid() || !child.IsValid()) {
                return;
            }
            // Délègue à SetParent pour éviter la duplication de logique
            child.SetParent(*this);
        }

        /**
         * @brief Remplit un vecteur avec tous les enfants directs de cet objet.
         * @param out NkVector<NkGameObject> à remplir (ajouté à la fin).
         * 
         * 🔹 Pourquoi un out-param plutôt qu'un retour par valeur ?
         *    • Évite l'allocation implicite de std::vector (interdit ici)
         *    • Permet de réutiliser le même buffer pour plusieurs appels
         *      (réduction des allocations heap en boucle chaude)
         *    • Donne un contrôle explicite de la stratégie d'allocation
         * 
         * 🔹 Comportement d'ajout :
         *    • Les enfants sont PushBack-és à la fin du vecteur existant
         *    • Utiliser out.Clear() avant appel si on veut remplacer le contenu
         *    • La méthode appelle out.Reserve() pour éviter les réallocations
         * 
         * @note Les enfants sont retournés dans l'ordre d'ajout à la liste.
         *       Cet ordre n'est pas garanti stable après suppressions/ajouts.
         */
        void NkGameObject::GetChildren(NkVector<NkGameObject>& out) const noexcept {
            if (!IsValid()) {
                return;
            }
            if (const auto* ch = mWorld->Get<NkChildren>(mId)) {
                // Pré-allocation pour éviter les réallocations multiples
                out.Reserve(out.Size() + ch->count);
                for (uint32 i = 0; i < ch->count; ++i) {
                    out.PushBack(NkGameObject(ch->children[i], mWorld));
                }
            }
        }

        /**
         * @brief Version pratique retournant un nouveau vecteur d'enfants.
         * @return NkVector<NkGameObject> contenant tous les enfants directs.
         * 
         * @warning Cette méthode alloue sur le heap via NkVector.
         *          À éviter dans les boucles de mise à jour fréquentes.
         *          Préférer GetChildren(out) avec un buffer réutilisé.
         * 
         * @note Utile pour :
         *    • Prototypage rapide
         *    • Code hors boucle chaude (UI, debug, initialisation)
         *    • Cas où la simplicité prime sur la performance
         */
        NkVector<NkGameObject> NkGameObject::GetChildrenV() const noexcept {
            NkVector<NkGameObject> result;
            GetChildren(result);
            return result;
        }

        // =====================================================================
        // ⚡ Activation & Cycle de vie
        // =====================================================================

        /**
         * @brief Active ou désactive cet objet dans le monde.
         * @param active true pour activer, false pour désactiver.
         * 
         * 🔹 Mécanisme via composant marqueur NkInactive :
         *    • Activation : World().Remove<NkInactive>(mId)
         *      → L'entité redevient visible pour les queries et systèmes
         *    • Désactivation : World().AddDeferred<NkInactive>(mId)
         *      → L'entité sera filtrée par les queries après FlushDeferred()
         * 
         * 🔹 Pourquoi AddDeferred pour la désactivation ?
         *    • Permet d'appeler SetActive(false) pendant l'itération
         *      sur un Query sans invalider l'itérateur
         *    • La désactivation effective est différée au flush
         * 
         * @note Les Behaviours attachés à un objet inactif ne sont pas mis à jour
         *       par le scheduler (voir NkScheduler::UpdateActiveOnly).
         */
        void NkGameObject::SetActive(bool active) noexcept {
            if (!IsValid()) {
                return;
            }
            if (active) {
                mWorld->Remove<NkInactive>(mId);
            } else {
                mWorld->AddDeferred<NkInactive>(mId);
            }
        }

        /**
         * @brief Vérifie si l'objet est actuellement actif.
         * @return true si valide ET sans composant NkInactive.
         * 
         * @note Cette méthode reflète l'état IMMÉDIAT :
         *    • Si SetActive(false) vient d'être appelé, IsActive() peut encore
         *      retourner true jusqu'au prochain FlushDeferred()
         *    • Pour un état "post-flush", appeler après world.FlushDeferred()
         */
        bool NkGameObject::IsActive() const noexcept {
            return IsValid() && !mWorld->Has<NkInactive>(mId);
        }

        /**
         * @brief Marque cet objet pour destruction différée.
         * 
         * 🔹 Sécurité pendant l'itération :
         *    • Utilise World().DestroyDeferred() en interne
         *    • L'entité reste valide jusqu'au flush, évitant l'invalidation
         *      d'itérateurs dans les queries en cours
         * 
         * 🔹 Effets de la destruction :
         *    • Suppression de l'entité de son archétype
         *    • Libération de l'ID pour réutilisation future
         *    • Destruction de tous les composants attachés
         *    • Nettoyage des références parent/enfants (à gérer côté scène)
         * 
         * @note Après FlushDeferred(), IsValid() retournera false pour ce handle.
         *       Les Behaviours attachés recevront OnDestroy() si implémenté.
         */
        void NkGameObject::DestroyDeferred() noexcept {
            if (IsValid()) {
                mWorld->DestroyDeferred(mId);
            }
        }

        // =====================================================================
        // 🔗 Opérateurs de comparaison
        // =====================================================================

        /**
         * @brief Comparaison d'égalité : même ID ET même monde.
         * @param o Autre NkGameObject à comparer.
         * @return true si les deux handles référencent la même entité ECS.
         * 
         * 🔹 Pourquoi comparer aussi mWorld ?
         *    • Deux mondes ECS distincts peuvent avoir des ID identiques
         *    • La comparaison monde+ID garantit l'unicité globale
         * 
         * @note Deux handles invalides (Id=Invalid, World=nullptr) sont égaux.
         *       C'est un choix de conception pour simplifier les conteneurs.
         */
        bool NkGameObject::operator==(const NkGameObject& o) const noexcept {
            return mId == o.mId && mWorld == o.mWorld;
        }

        /**
         * @brief Comparaison d'inégalité.
         * @note Implémentée via operator== pour éviter la duplication de logique.
         */
        bool NkGameObject::operator!=(const NkGameObject& o) const noexcept {
            return !(*this == o);
        }

    } // namespace ecs
} // namespace nkentseu