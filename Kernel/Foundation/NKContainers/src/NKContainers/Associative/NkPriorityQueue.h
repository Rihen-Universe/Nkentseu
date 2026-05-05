// -----------------------------------------------------------------------------
// FICHIER: NKContainers\Containers\Associative\NkPriorityQueue.h
// DESCRIPTION: File de priorité - Implémentation par tas binaire (binary heap)
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CONTAINERS_ASSOCIATIVE_NKPRIORITYQUEUE_H
#define NK_CONTAINERS_ASSOCIATIVE_NKPRIORITYQUEUE_H

    // ========================================================================
    // INCLUSIONS DES DÉPENDANCES
    // ========================================================================

    #include "NKCore/NkTypes.h"                    // Définition des types fondamentaux (usize, etc.)
    #include "NKContainers/NkContainersApi.h"      // Macros d'exportation de l'API conteneurs
    #include "NKCore/NkTraits.h"                   // Traits et utilitaires pour swap/move/forward
    #include "NKCore/Assert/NkAssert.h"            // Macros d'assertion pour le débogage sécurisé
    #include "NKContainers/Sequential/NkVector.h"  // Conteneur vectoriel pour le stockage interne du heap

    // ========================================================================
    // ESPACE DE NOMS PRINCIPAL
    // ========================================================================

    namespace nkentseu {

        // ====================================================================
        // COMPARATEUR PAR DÉFAUT : ORDRE CROISSANT (MAX-HEAP)
        // ====================================================================

        /**
         * @brief Foncteur de comparaison par défaut pour NkPriorityQueue
         * 
         * Implémente un ordre strict faible basé sur l'opérateur <.
         * Utilisé pour créer un max-heap par défaut : l'élément "le plus grand"
         * selon cette comparaison a la plus haute priorité et se trouve en tête.
         * 
         * @tparam T Type des éléments comparés (doit supporter operator<)
         * 
         * @note Pour un min-heap, utiliser un comparateur inversé :
         *       [](const T& a, const T& b) { return a > b; }
         */
        template<typename T>
        struct NkPriorityLess {

            /**
             * @brief Opérateur de comparaison pour deux éléments
             * @param lhs Premier opérande (côté gauche)
             * @param rhs Second opérande (côté droit)
             * @return true si lhs < rhs, false sinon
             * @note Méthode const : ne modifie pas l'état du foncteur
             */
            bool operator()(const T& lhs, const T& rhs) const {
                return lhs < rhs;
            }
        };

        // ====================================================================
        // CLASSE PRINCIPALE : NK PRIORITY QUEUE
        // ====================================================================

        /**
         * @brief Classe template implémentant une file de priorité par tas binaire
         * 
         * Structure de données permettant d'accéder en O(1) à l'élément de plus
         * haute priorité, avec insertion et suppression en O(log n).
         * 
         * IMPLÉMENTATION :
         * - Tas binaire complet stocké dans un NkVector (tableau dynamique)
         * - Relation parent-enfant : parent(i) = (i-1)/2, left(i) = 2i+1, right(i) = 2i+2
         * - Invariant de heap : pour tout i, compare(parent(i), child(i)) == false
         * 
         * PROPRIÉTÉS :
         * - Max-heap par défaut (élément maximum en tête)
         * - Comparateur personnalisable via template parameter
         * - Allocateur personnalisable pour gestion mémoire fine
         * - Support C++11 : move semantics, emplace, initializer_list
         * 
         * COMPLEXITÉ ALGORITHMIQUE :
         * - Top() : O(1) - accès direct au premier élément
         * - Push()/Emplace() : O(log n) - insertion avec remontée (heapify-up)
         * - Pop() : O(log n) - suppression avec descente (heapify-down)
         * - Construction depuis liste : O(n log n) naïf, O(n) avec heapify optimal
         * 
         * CAS D'USAGE TYPQUES :
         * - Ordonnancement de tâches par priorité
         * - Algorithmes de graphes (Dijkstra, A*, Prim)
         * - Fusion de flux triés (k-way merge)
         * - Gestion d'événements simulés (discrete event simulation)
         * 
         * @tparam T Type des éléments stockés (doit être copiable/déplaçable)
         * @tparam Allocator Type de l'allocateur mémoire (défaut: memory::NkAllocator)
         * @tparam Compare Foncteur de comparaison (défaut: NkPriorityLess<T> pour max-heap)
         * 
         * @note Thread-unsafe : synchronisation externe requise pour usage concurrent
         * @note Les itérateurs ne sont pas exposés (incompatible avec la sémantique de heap)
         */
        template<typename T,
                 typename Allocator = memory::NkAllocator,
                 typename Compare = NkPriorityLess<T>>
        class NkPriorityQueue {

            // ====================================================================
            // SECTION PUBLIQUE : ALIASES ET TYPES ASSOCIÉS
            // ====================================================================
            public:

                using ValueType = T;              ///< Alias du type des éléments stockés
                using SizeType = usize;           ///< Alias du type pour les tailles/comptages
                using Reference = T&;             ///< Alias de référence non-const
                using ConstReference = const T&;  ///< Alias de référence const

            // ====================================================================
            // SECTION PRIVÉE : MEMBRES ET IMPLÉMENTATION INTERNE
            // ====================================================================
            private:

                // ====================================================================
                // MEMBRES DONNÉES : ÉTAT DE LA FILE DE PRIORITÉ
                // ====================================================================

                Allocator* mAllocator;            ///< Pointeur vers l'allocateur mémoire actif
                NkVector<T, Allocator> mHeap;     ///< Conteneur interne stockant le tas binaire
                Compare mCompare;                 ///< Foncteur de comparaison pour l'ordre de priorité

                // ====================================================================
                // MÉTHODES UTILITAIRES DE NAVIGATION DANS LE TAS
                // ====================================================================

                /**
                 * @brief Calcule l'index du nœud parent dans le tableau du heap
                 * @param i Index du nœud enfant
                 * @return Index du nœud parent : (i - 1) / 2
                 * @note noexcept garantie, opération arithmétique pure O(1)
                 */
                SizeType Parent(SizeType i) const {
                    return (i - 1) / 2;
                }

                /**
                 * @brief Calcule l'index de l'enfant gauche dans le tableau du heap
                 * @param i Index du nœud parent
                 * @return Index de l'enfant gauche : 2 * i + 1
                 * @note noexcept garantie, opération arithmétique pure O(1)
                 */
                SizeType Left(SizeType i) const {
                    return 2 * i + 1;
                }

                /**
                 * @brief Calcule l'index de l'enfant droit dans le tableau du heap
                 * @param i Index du nœud parent
                 * @return Index de l'enfant droit : 2 * i + 2
                 * @note noexcept garantie, opération arithmétique pure O(1)
                 */
                SizeType Right(SizeType i) const {
                    return 2 * i + 2;
                }

                // ====================================================================
                // MÉTHODES DE MAINTENANCE DE L'INVARIANT DE HEAP
                // ====================================================================

                /**
                 * @brief Remonte un élément pour restaurer l'invariant de heap après insertion
                 * @param i Index de l'élément à remonter dans le tableau interne
                 * @note Complexité O(log n) dans le pire cas (hauteur du tas)
                 * @note Utilise le comparateur pour déterminer si un échange est nécessaire
                 */
                void HeapifyUp(SizeType i) {
                    while (i > 0 && mCompare(mHeap[Parent(i)], mHeap[i])) {
                        traits::NkSwap(mHeap[i], mHeap[Parent(i)]);
                        i = Parent(i);
                    }
                }

                /**
                 * @brief Descend un élément pour restaurer l'invariant de heap après suppression
                 * @param i Index de l'élément à descendre dans le tableau interne
                 * @note Complexité O(log n) dans le pire cas (hauteur du tas)
                 * @note Sélectionne le "plus prioritaire" des enfants pour l'échange
                 */
                void HeapifyDown(SizeType i) {
                    SizeType size = mHeap.Size();
                    SizeType largest = i;
                    SizeType left = Left(i);
                    SizeType right = Right(i);
                    if (left < size && mCompare(mHeap[largest], mHeap[left])) {
                        largest = left;
                    }
                    if (right < size && mCompare(mHeap[largest], mHeap[right])) {
                        largest = right;
                    }
                    if (largest != i) {
                        traits::NkSwap(mHeap[i], mHeap[largest]);
                        HeapifyDown(largest);
                    }
                }

            // ====================================================================
            // SECTION PUBLIQUE : CONSTRUCTEURS ET ASSIGNATION
            // ====================================================================
            public:

                /**
                 * @brief Constructeur par défaut avec allocateur global
                 * @note Initialise un heap vide avec l'allocateur par défaut du système
                 * @note Complexité O(1) : aucune allocation initiale
                 */
                NkPriorityQueue()
                    : mAllocator(&memory::NkGetDefaultAllocator())
                    , mHeap(mAllocator)
                    , mCompare() {
                }

                /**
                 * @brief Constructeur avec allocateur personnalisé
                 * @param allocator Pointeur vers l'allocateur à utiliser (nullptr = défaut)
                 * @note Permet un contrôle fin de la stratégie d'allocation mémoire
                 */
                explicit NkPriorityQueue(Allocator* allocator)
                    : mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                    , mHeap(mAllocator)
                    , mCompare() {
                }

                /**
                 * @brief Constructeur depuis NkInitializerList avec allocateur optionnel
                 * @param init Liste d'éléments pour initialiser la file
                 * @param allocator Pointeur vers l'allocateur (nullptr = défaut)
                 * @note Complexité O(n log n) : insertions séquentielles avec heapify-up
                 */
                NkPriorityQueue(NkInitializerList<T> init, Allocator* allocator = nullptr)
                    : mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                    , mHeap(mAllocator)
                    , mCompare() {
                    for (auto& val : init) {
                        Push(val);
                    }
                }

                /**
                 * @brief Constructeur depuis std::initializer_list avec allocateur optionnel
                 * @param init Liste standard d'éléments pour initialiser la file
                 * @param allocator Pointeur vers l'allocateur (nullptr = défaut)
                 * @note Compatibilité avec la syntaxe C++11 { ... }
                 */
                NkPriorityQueue(std::initializer_list<T> init, Allocator* allocator = nullptr)
                    : mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                    , mHeap(mAllocator)
                    , mCompare() {
                    for (auto& val : init) {
                        Push(val);
                    }
                }

                /**
                 * @brief Opérateur d'assignation depuis NkInitializerList
                 * @param init Nouvelle liste d'éléments pour remplacer le contenu
                 * @return Référence vers l'instance courante pour chaînage
                 * @note Efface le contenu actuel avant d'insérer les nouveaux éléments
                 */
                NkPriorityQueue& operator=(NkInitializerList<T> init) {
                    Clear();
                    for (auto& val : init) {
                        Push(val);
                    }
                    return *this;
                }

                /**
                 * @brief Opérateur d'assignation depuis std::initializer_list
                 * @param init Nouvelle liste standard d'éléments
                 * @return Référence vers l'instance courante pour chaînage
                 * @note Syntaxe compatible C++11 : pq = {1, 2, 3};
                 */
                NkPriorityQueue& operator=(std::initializer_list<T> init) {
                    Clear();
                    for (auto& val : init) {
                        Push(val);
                    }
                    return *this;
                }

            // ====================================================================
            // SECTION PUBLIQUE : ACCÈS AUX ÉLÉMENTS
            // ====================================================================

                /**
                 * @brief Accède à l'élément de plus haute priorité (sans le supprimer)
                 * @return Référence const vers l'élément en tête du heap
                 * @pre La file ne doit pas être vide (assertion en mode debug)
                 * @note Complexité O(1) : accès direct à l'indice 0 du tableau interne
                 * @note Méthode const : ne modifie pas l'état de la file
                 */
                ConstReference Top() const {
                    NKENTSEU_ASSERT(!Empty());
                    return mHeap[0];
                }

            // ====================================================================
            // SECTION PUBLIQUE : CAPACITÉ ET MÉTADONNÉES
            // ====================================================================

                /**
                 * @brief Teste si la file de priorité est vide
                 * @return true si aucun élément n'est présent, false sinon
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Équivalent à Size() == 0 mais sémantiquement plus explicite
                 */
                bool Empty() const NKENTSEU_NOEXCEPT {
                    return mHeap.Empty();
                }

                /**
                 * @brief Retourne le nombre d'éléments actuellement dans la file
                 * @return Valeur de type SizeType représentant la cardinalité
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Reflète exactement mHeap.Size() : pas de compteur séparé
                 */
                SizeType Size() const NKENTSEU_NOEXCEPT {
                    return mHeap.Size();
                }

                /**
                 * @brief Fournit un accès à l'allocateur utilisé par l'instance
                 * @return Pointeur const vers l'allocateur mémoire actif
                 * @note noexcept garantie, opération O(1)
                 * @note Utile pour la cohérence d'allocation dans les conteneurs imbriqués
                 */
                Allocator* GetAllocator() const NKENTSEU_NOEXCEPT {
                    return mAllocator;
                }

            // ====================================================================
            // SECTION PUBLIQUE : MODIFICATEURS DE LA FILE
            // ====================================================================

                /**
                 * @brief Insère un nouvel élément en maintenant l'invariant de heap
                 * @param value Élément à ajouter (copié)
                 * @note Complexité amortie O(log n) pour la remontée (heapify-up)
                 * @note Peut déclencher une réallocation du vecteur interne si capacité insuffisante
                 */
                void Push(const T& value) {
                    mHeap.PushBack(value);
                    HeapifyUp(mHeap.Size() - 1);
                }

                // ====================================================================
                // SUPPORT C++11 : MOVE SEMANTICS ET EMPLACE
                // ====================================================================
                #if defined(NK_CPP11)

                    /**
                     * @brief Insère un élément par déplacement (move) pour éviter une copie
                     * @param value Élément à ajouter (déplacé)
                     * @note Nécessite que T soit MoveConstructible
                     * @note Optimisation pour les types lourds ou non-copiables
                     */
                    void Push(T&& value) {
                        mHeap.PushBack(traits::NkMove(value));
                        HeapifyUp(mHeap.Size() - 1);
                    }

                    /**
                     * @brief Construit et insère un élément directement dans le heap
                     * @tparam Args Types des arguments de construction de T
                     * @param args Arguments forwarding vers le constructeur de T
                     * @note Évite toute copie/move temporaire : construction in-place
                     * @note Particulièrement efficace pour les types avec constructeurs complexes
                     */
                    template<typename... Args>
                    void Emplace(Args&&... args) {
                        mHeap.EmplaceBack(traits::NkForward<Args>(args)...);
                        HeapifyUp(mHeap.Size() - 1);
                    }

                #endif // defined(NK_CPP11)

                /**
                 * @brief Supprime l'élément de plus haute priorité (en tête)
                 * @pre La file ne doit pas être vide (assertion en mode debug)
                 * @note Complexité O(log n) pour la descente (heapify-down)
                 * @note Remplace la racine par le dernier élément avant de rééquilibrer
                 */
                void Pop() {
                    NKENTSEU_ASSERT(!Empty());
                    if (mHeap.Size() == 1) {
                        mHeap.PopBack();
                        return;
                    }
                    mHeap[0] = mHeap[mHeap.Size() - 1];
                    mHeap.PopBack();
                    HeapifyDown(0);
                }

                /**
                 * @brief Supprime tous les éléments et libère la mémoire interne
                 * @note Complexité O(n) pour la destruction de tous les éléments
                 * @note La capacité du vecteur interne peut être conservée (dépend de NkVector)
                 * @note Après Clear(), Empty() retourne true et Size() retourne 0
                 */
                void Clear() {
                    mHeap.Clear();
                }

                /**
                 * @brief Échange le contenu de cette file avec une autre en temps constant
                 * @param other Autre instance de NkPriorityQueue à échanger
                 * @note Complexité O(1) : échange des pointeurs et métadonnées uniquement
                 * @note noexcept garantie : aucune opération pouvant lever d'exception
                 * @note Préférer à l'assignation pour les permutations efficaces
                 */
                void Swap(NkPriorityQueue& other) NKENTSEU_NOEXCEPT {
                    mHeap.Swap(other.mHeap);
                    traits::NkSwap(mAllocator, other.mAllocator);
                    traits::NkSwap(mCompare, other.mCompare);
                }
        };

        // ====================================================================
        // FONCTIONS NON-MEMBRES : SWAP GLOBAL ET UTILITAIRES
        // ====================================================================

        /**
         * @brief Fonction swap non-membre pour ADL (Argument-Dependent Lookup)
         * @tparam T Type des éléments de la file
         * @tparam Allocator Type de l'allocateur
         * @tparam Compare Type du comparateur
         * @param lhs Première file à échanger
         * @param rhs Seconde file à échanger
         * @note noexcept garantie : délègue à la méthode membre Swap()
         * @note Permet l'usage idiomatique : using std::swap; swap(a, b);
         */
        template<typename T, typename Allocator, typename Compare>
        void NkSwap(NkPriorityQueue<T, Allocator, Compare>& lhs,
                    NkPriorityQueue<T, Allocator, Compare>& rhs) NKENTSEU_NOEXCEPT {
            lhs.Swap(rhs);
        }

    } // namespace nkentseu

#endif // NK_CONTAINERS_ASSOCIATIVE_NKPRIORITYQUEUE_H

// ============================================================================
// EXEMPLES D'UTILISATION ET PATTERNS COURANTS
// ============================================================================
/*
 * @section examples Exemples pratiques d'utilisation de NkPriorityQueue
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 1 : File de priorité basique (max-heap par défaut)
 * --------------------------------------------------------------------------
 * @code
 * #include "NKContainers/Associative/NkPriorityQueue.h"
 * #include <cstdio>
 * 
 * void exempleMaxHeap() {
 *     // File de priorité d'entiers : le plus grand en tête
 *     nkentseu::NkPriorityQueue<int> pq;
 *     
 *     // Insertion d'éléments dans un ordre arbitraire
 *     pq.Push(3);
 *     pq.Push(1);
 *     pq.Push(4);
 *     pq.Push(1);
 *     pq.Push(5);
 *     pq.Push(9);
 *     
 *     // Extraction dans l'ordre décroissant (plus prioritaire d'abord)
 *     printf("Extraction max-heap: ");
 *     while (!pq.Empty()) {
 *         printf("%d ", pq.Top());  // Accès à l'élément en tête
 *         pq.Pop();                 // Suppression de l'élément en tête
 *     }
 *     // Sortie: 9 5 4 3 1 1
 *     printf("\n");
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 2 : Min-heap avec comparateur personnalisé
 * --------------------------------------------------------------------------
 * @code
 * void exempleMinHeap() {
 *     // Comparateur inversé pour obtenir un min-heap (plus petit en tête)
 *     auto minCompare = [](int a, int b) { return a > b; };
 *     
 *     // Note: nécessite une spécialisation ou wrapper du comparateur
 *     // Alternative: utiliser un type wrapper avec operator< inversé
 *     struct MinInt {
 *         int value;
 *         bool operator<(const MinInt& other) const {
 *             return value > other.value;  // Inversion pour min-heap
 *         }
 *     };
 *     
 *     nkentseu::NkPriorityQueue<MinInt> minPq;
 *     minPq.Push({3});
 *     minPq.Push({1});
 *     minPq.Push({4});
 *     
 *     printf("Extraction min-heap: ");
 *     while (!minPq.Empty()) {
 *         printf("%d ", minPq.Top().value);
 *         minPq.Pop();
 *     }
 *     // Sortie: 1 3 4
 *     printf("\n");
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 3 : Ordonnancement de tâches avec priorité
 * --------------------------------------------------------------------------
 * @code
 * struct Task {
 *     int id;
 *     int priority;  // Plus élevé = plus prioritaire
 *     const char* description;
 *     
 *     // Comparaison basée sur la priorité uniquement
 *     bool operator<(const Task& other) const {
 *         return priority < other.priority;
 *     }
 * };
 * 
 * void exempleTaches() {
 *     nkentseu::NkPriorityQueue<Task> taskQueue;
 *     
 *     // Ajout de tâches avec priorités variées
 *     taskQueue.Push({101, 3, "Sauvegarde base de données"});
 *     taskQueue.Push({102, 7, "Traitement commande urgente"});
 *     taskQueue.Push({103, 1, "Nettoyage logs anciens"});
 *     taskQueue.Push({104, 5, "Génération rapport mensuel"});
 *     
 *     // Traitement par ordre de priorité décroissante
 *     printf("Ordre de traitement:\n");
 *     while (!taskQueue.Empty()) {
 *         const Task& t = taskQueue.Top();
 *         printf("[Prio %d] #%d: %s\n", t.priority, t.id, t.description);
 *         taskQueue.Pop();
 *     }
 *     // Sortie:
 *     // [Prio 7] #102: Traitement commande urgente
 *     // [Prio 5] #104: Génération rapport mensuel
 *     // [Prio 3] #101: Sauvegarde base de données
 *     // [Prio 1] #103: Nettoyage logs anciens
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 4 : Algorithmes de graphes (Dijkstra simplifié)
 * --------------------------------------------------------------------------
 * @code
 * #include <limits>
 * 
 * struct Edge {
 *     int vertex;
 *     int weight;
 *     
 *     // Pour min-heap : on veut le plus petit poids en tête
 *     bool operator<(const Edge& other) const {
 *         return weight > other.weight;  // Inversion pour min-heap
 *     }
 * };
 * 
 * void exempleDijkstra() {
 *     const int INF = std::numeric_limits<int>::max();
 *     nkentseu::NkPriorityQueue<Edge> pq;  // Min-heap via operator< inversé
 *     
 *     // Initialisation : départ depuis le sommet 0
 *     pq.Push({0, 0});
 *     
 *     // Simulation d'étapes de relaxation (simplifiée)
 *     pq.Push({1, 4});  // Arête 0->1 de poids 4
 *     pq.Push({2, 2});  // Arête 0->2 de poids 2
 *     
 *     printf("Traitement des sommets par distance croissante:\n");
 *     while (!pq.Empty()) {
 *         Edge current = pq.Top();
 *         pq.Pop();
 *         printf("Sommet %d, distance: %d\n", current.vertex, current.weight);
 *         
 *         // Ici: relaxer les arêtes sortantes et pousser les nouvelles distances
 *         // if (newDist < dist[neighbor]) { pq.Push({neighbor, newDist}); }
 *     }
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 5 : Fusion de K listes triées (k-way merge)
 * --------------------------------------------------------------------------
 * @code
 * #include "NKContainers/Sequential/NkVector.h"
 * 
 * struct ListElement {
 *     int value;
 *     int listIndex;  // Identifiant de la liste d'origine
 *     
 *     bool operator<(const ListElement& other) const {
 *         return value > other.value;  // Min-heap pour fusion croissante
 *     }
 * };
 * 
 * void exempleFusionListes() {
 *     // K listes triées (simulées)
 *     nkentseu::NkVector<nkentseu::NkVector<int>> lists = {
 *         {1, 4, 7},
 *         {2, 5, 8},
 *         {3, 6, 9}
 *     };
 *     
 *     nkentseu::NkPriorityQueue<ListElement> pq;
 *     nkentseu::NkVector<usize> indices(lists.Size(), 0);  // Index courant par liste
 *     
 *     // Initialisation : premier élément de chaque liste dans le heap
 *     for (usize i = 0; i < lists.Size(); ++i) {
 *         if (!lists[i].Empty()) {
 *             pq.Push({lists[i][0], static_cast<int>(i)});
 *             indices[i] = 1;
 *         }
 *     }
 *     
 *     // Fusion : extraction du minimum et ajout du suivant de la même liste
 *     printf("Fusion K-way: ");
 *     while (!pq.Empty()) {
 *         ListElement minElem = pq.Top();
 *         pq.Pop();
 *         printf("%d ", minElem.value);
 *         
 *         // Ajouter le prochain élément de la liste d'origine si disponible
 *         if (indices[minElem.listIndex] < lists[minElem.listIndex].Size()) {
 *             pq.Push({
 *                 lists[minElem.listIndex][indices[minElem.listIndex]],
 *                 minElem.listIndex
 *             });
 *             indices[minElem.listIndex]++;
 *         }
 *     }
 *     // Sortie: 1 2 3 4 5 6 7 8 9
 *     printf("\n");
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 6 : Optimisation avec emplace et move semantics (C++11)
 * --------------------------------------------------------------------------
 * @code
 * #if defined(NK_CPP11)
 * 
 * struct HeavyObject {
 *     nkentseu::NkVector<char> buffer;
 *     int priority;
 *     
 *     HeavyObject(int prio, usize size) 
 *         : buffer(size, 'x'), priority(prio) {}
 *     
 *     // Comparaison pour max-heap sur la priorité
 *     bool operator<(const HeavyObject& other) const {
 *         return priority < other.priority;
 *     }
 * };
 * 
 * void exempleEmplace() {
 *     nkentseu::NkPriorityQueue<HeavyObject> pq;
 *     
 *     // Emplace évite la construction temporaire + move : construction directe
 *     pq.Emplace(5, 1024);   // priority=5, buffer de 1KB
 *     pq.Emplace(9, 2048);   // priority=9, buffer de 2KB
 *     pq.Emplace(3, 512);    // priority=3, buffer de 512B
 *     
 *     // Extraction avec move pour éviter une copie du buffer lourd
 *     while (!pq.Empty()) {
 *         HeavyObject obj = traits::NkMove(const_cast<HeavyObject&>(pq.Top()));
 *         pq.Pop();
 *         printf("Traitement objet prio %d, taille %zu\n", 
 *                obj.priority, obj.buffer.Size());
 *     }
 * }
 * 
 * #endif // NK_CPP11
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * BONNES PRATIQUES ET RECOMMANDATIONS
 * --------------------------------------------------------------------------
 * 
 * 1. CHOIX DU COMPARATEUR :
 *    - Max-heap par défaut : utiliser NkPriorityLess<T> (operator< normal)
 *    - Min-heap : inverser la logique dans operator< ou fournir un comparateur custom
 *    - Pour les types complexes : définir operator< sémantique, puis inverser si besoin
 * 
 * 2. OPTIMISATIONS DE PERFORMANCE :
 *    - Pré-réserver la capacité avec mHeap.Reserve() si le volume est connu
 *    - Privilégier Emplace() pour éviter les constructions temporaires coûteuses
 *    - Utiliser Push(T&&) pour les objets lourds déjà construits
 * 
 * 3. GESTION MÉMOIRE :
 *    - Utiliser un allocateur pool pour les files de taille fixe ou prévisible
 *    - Attention à la fragmentation avec de nombreuses insertions/suppressions
 *    - Clear() ne réduit pas nécessairement la capacité : appeler ShrinkToFit() si disponible
 * 
 * 4. SÉCURITÉ ET ROBUSTESSE :
 *    - Toujours vérifier !Empty() avant Top() ou Pop() en production
 *    - Les assertions NKENTSEU_ASSERT aident au débogage en mode development
 *    - Thread-unsafe : protéger avec mutex pour accès concurrents multiples
 * 
 * 5. LIMITATIONS CONNUES (version actuelle) :
 *    - Pas de méthode DecreaseKey() : requise pour certaines implémentations de Dijkstra
 *    - Pas d'itérateurs : par conception, incompatible avec l'invariant de heap
 *    - Pas de fusion de deux heaps en O(1) : envisager un Fibonacci heap pour ce cas
 * 
 * 6. EXTENSIONS RECOMMANDÉES :
 *    - Ajouter une méthode BuildHeap(const NkVector<T>&) en O(n) pour construction batch
 *    - Implémenter un itérateur en lecture seule pour inspection (sans garantie d'ordre)
 *    - Ajouter support pour les priorités flottantes avec tolérance epsilon
 */

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Généré par Rihen le 2026-02-05 22:26:13
// Dernière modification : 2026-04-26 (restructuration et documentation)
// ============================================================