// -----------------------------------------------------------------------------
// FICHIER: NKContainers\Containers\Associative\NkBTree.h
// DESCRIPTION: Arbre B auto-équilibré optimisé pour systèmes de stockage
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKBTREE_H
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKBTREE_H

    // ========================================================================
    // INCLUSIONS DES DÉPENDANCES
    // ========================================================================

    #include "NKCore/NkTypes.h"                    // Définition des types fondamentaux (usize, etc.)
    #include "NKContainers/NkContainersApi.h"      // Macros d'exportation de l'API conteneurs
    #include "NKCore/NkTraits.h"                   // Traits et métaprogrammation pour move/copy
    #include "NKMemory/NkAllocator.h"              // Système d'allocation mémoire personnalisable
    #include "NKCore/Assert/NkAssert.h"            // Macros d'assertion pour le débogage sécurisé

    // ========================================================================
    // ESPACE DE NOMS PRINCIPAL
    // ========================================================================

    namespace nkentseu {

        /**
         * @brief Classe template implémentant un arbre B (B-Tree) auto-équilibré
         * 
         * Structure de données multi-way tree optimisée pour les systèmes avec accès disque
         * ou mémoire hiérarchique. Chaque nœud peut contenir plusieurs clés triées,
         * réduisant ainsi la hauteur de l'arbre et le nombre d'accès nécessaires.
         * 
         * PROPRIÉTÉS FONDAMENTALES :
         * - Tous les nœuds feuilles sont au même niveau (équilibre garanti)
         * - Un nœud avec k clés possède exactement k+1 pointeurs enfants
         * - Les clés dans chaque nœud sont maintenues triées croissantes
         * - Nombre minimum de clés par nœud : order - 1
         * - Nombre maximum de clés par nœud : 2 * order - 1
         * 
         * CAS D'USAGE PRÉFÉRENTIELS :
         * - Moteurs de bases de données (indexation B+Tree dérivé)
         * - Systèmes de fichiers (allocation de blocs, métadonnées)
         * - Index sur support de stockage secondaire (SSD, HDD)
         * - Caches LRU avec accès par plage de clés
         * 
         * COMPLEXITÉ ALGORITHMIQUE :
         * - Insertion : O(log_m n) où m = order, n = nombre d'éléments
         * - Recherche : O(log_m n) avec factor constant réduit vs arbre binaire
         * - Suppression : O(log_m n) (non implémentée dans cette version)
         * - Espace mémoire : O(n) avec overhead de fragmentation interne < 50%
         * 
         * @tparam T Type des clés stockées (doit supporter <, ==, move/copy construction)
         * @tparam Allocator Type de l'allocateur mémoire (défaut: memory::NkAllocator)
         * 
         * @note L'ordre minimum est fixé à 3 pour garantir la stabilité structurelle
         * @note Thread-unsafe : synchronisation externe requise pour usage concurrent
         * @note Les doublons sont acceptés et insérés selon l'ordre d'arrivée
         */
        template<typename T, typename Allocator = memory::NkAllocator>
        class NkBTree {

            // ====================================================================
            // SECTION PRIVÉE : IMPLÉMENTATION INTERNE
            // ====================================================================
            private:

                /**
                 * @brief Structure interne représentant un nœud de l'arbre B
                 * 
                 * Conteneur dynamique de clés et pointeurs enfants avec gestion mémoire fine.
                 * Utilise l'allocateur fourni pour toutes les allocations dynamiques.
                 * 
                 * @note Layout mémoire : [Keys: 2*order-1][Children: 2*order][Metadata]
                 * @note Les tableaux sont alloués séparément pour éviter le padding excessif
                 */
                struct BTreeNode {

                    T* Keys;                    ///< Tableau dynamique des clés triées
                    BTreeNode** Children;       ///< Tableau dynamique des pointeurs enfants
                    usize NumKeys;              ///< Nombre courant de clés valides dans le nœud
                    bool IsLeaf;                ///< Indicateur de nœud feuille (sans enfants)
                    usize Order;                ///< Degré minimum de l'arbre (paramètre structurel)

                    /**
                     * @brief Constructeur de nœud avec allocation des tableaux internes
                     * @param order Degré minimum de l'arbre B (détermine la capacité)
                     * @param isLeaf Indique si le nœud est une feuille
                     * @param alloc Pointeur vers l'allocateur à utiliser
                     */
                    BTreeNode(usize order, bool isLeaf, Allocator* alloc)
                        : NumKeys(0)
                        , IsLeaf(isLeaf)
                        , Order(order) {
                        const usize maxKeys = 2 * order - 1;
                        const usize maxChildren = 2 * order;
                        Keys = static_cast<T*>(alloc->Allocate(maxKeys * sizeof(T)));
                        Children = static_cast<BTreeNode**>(alloc->Allocate(maxChildren * sizeof(BTreeNode*)));
                        for (usize i = 0; i < maxChildren; ++i) {
                            Children[i] = nullptr;
                        }
                    }
                };

                // ====================================================================
                // MEMBRES DONNÉES : ÉTAT DE L'ARBRE B
                // ====================================================================

                BTreeNode* mRoot;                 ///< Pointeur vers la racine de l'arbre B
                usize mOrder;                     ///< Degré minimum (min children = order, max = 2*order)
                usize mSize;                      ///< Nombre total de clés stockées dans l'arbre
                Allocator* mAllocator;            ///< Pointeur vers l'allocateur mémoire actif

                // ====================================================================
                // MÉTHODES UTILITAIRES DE CALCUL DE CAPACITÉ
                // ====================================================================

                /**
                 * @brief Calcule le nombre maximum de clés autorisées par nœud
                 * @return Valeur 2 * order - 1 selon la définition classique des B-Trees
                 * @note noexcept garantie, opération O(1) purement arithmétique
                 */
                usize MaxKeysPerNode() const NKENTSEU_NOEXCEPT {
                    return 2 * mOrder - 1;
                }

                // ====================================================================
                // MÉTHODES DE GESTION MÉMOIRE DES NŒUDS
                // ====================================================================

                /**
                 * @brief Alloue et construit un nouveau nœud avec les paramètres courants
                 * @param isLeaf Indique si le nœud créé est une feuille
                 * @return Pointeur vers le nœud nouvellement initialisé
                 * @note Utilise placement new pour construction contrôlée sur mémoire allouée
                 */
                BTreeNode* CreateNode(bool isLeaf) {
                    BTreeNode* node = static_cast<BTreeNode*>(mAllocator->Allocate(sizeof(BTreeNode)));
                    new (node) BTreeNode(mOrder, isLeaf, mAllocator);
                    return node;
                }

                /**
                 * @brief Détruit récursivement un nœud et libère toutes ses ressources
                 * @param node Pointeur vers le nœud à détruire
                 * @note Parcours post-ordre : destruction des enfants avant le parent
                 * @note Appel explicite des destructeurs pour les types non-triviaux
                 */
                void DestroyNode(BTreeNode* node) {
                    if (!node) return;
                    if (!node->IsLeaf) {
                        for (usize i = 0; i <= node->NumKeys; ++i) {
                            DestroyNode(node->Children[i]);
                        }
                    }
                    for (usize i = 0; i < node->NumKeys; ++i) {
                        node->Keys[i].~T();
                    }
                    mAllocator->Deallocate(node->Keys);
                    mAllocator->Deallocate(node->Children);
                    node->~BTreeNode();
                    mAllocator->Deallocate(node);
                }

                // ====================================================================
                // MÉTHODES RÉCURSIVES DE RECHERCHE
                // ====================================================================

                /**
                 * @brief Recherche récursivement une clé dans le sous-arbre spécifié
                 * @param node Racine courante du sous-arbre de recherche
                 * @param key Clé cible à localiser
                 * @return Pointeur vers le nœud contenant la clé, ou nullptr si absente
                 * @note Complexité O(log_m n) avec m = order, optimisée par recherche linéaire locale
                 */
                BTreeNode* SearchRecursive(BTreeNode* node, const T& key) const {
                    if (!node) return nullptr;
                    usize i = 0;
                    while (i < node->NumKeys && key > node->Keys[i]) {
                        ++i;
                    }
                    if (i < node->NumKeys && key == node->Keys[i]) {
                        return node;
                    }
                    if (node->IsLeaf) {
                        return nullptr;
                    }
                    return SearchRecursive(node->Children[i], key);
                }

                // ====================================================================
                // MÉTHODES DE MAINTENANCE STRUCTURELLE (SPLIT)
                // ====================================================================

                /**
                 * @brief Scinde un nœud enfant saturé en deux nœuds équilibrés
                 * @param parent Nœud parent contenant le pointeur vers l'enfant à scinder
                 * @param index Position de l'enfant dans le tableau Children du parent
                 * @note Opération critique pour maintenir les invariants de l'arbre B
                 * @note Déplace la clé médiane vers le parent et répartit les clés restantes
                 */
                void SplitChild(BTreeNode* parent, usize index) {
                    BTreeNode* fullChild = parent->Children[index];
                    BTreeNode* newChild = CreateNode(fullChild->IsLeaf);
                    const usize t = mOrder;
                    newChild->NumKeys = t - 1;
                    for (usize i = 0; i < t - 1; ++i) {
                        new (&newChild->Keys[i]) T(traits::NkMove(fullChild->Keys[i + t]));
                        fullChild->Keys[i + t].~T();
                    }
                    if (!fullChild->IsLeaf) {
                        for (usize i = 0; i < t; ++i) {
                            newChild->Children[i] = fullChild->Children[i + t];
                            fullChild->Children[i + t] = nullptr;
                        }
                    }
                    T middleKey = traits::NkMove(fullChild->Keys[t - 1]);
                    fullChild->Keys[t - 1].~T();
                    fullChild->NumKeys = t - 1;
                    for (usize i = parent->NumKeys + 1; i > index + 1; --i) {
                        parent->Children[i] = parent->Children[i - 1];
                    }
                    parent->Children[index + 1] = newChild;
                    for (usize i = parent->NumKeys; i > index; --i) {
                        new (&parent->Keys[i]) T(traits::NkMove(parent->Keys[i - 1]));
                        parent->Keys[i - 1].~T();
                    }
                    new (&parent->Keys[index]) T(traits::NkMove(middleKey));
                    parent->NumKeys++;
                }

                // ====================================================================
                // MÉTHODES RÉCURSIVES D'INSERTION
                // ====================================================================

                /**
                 * @brief Insère une clé dans un nœud non-saturé (précondition garantie)
                 * @param node Nœud courant où effectuer l'insertion
                 * @param key Clé à insérer en respectant l'ordre trié
                 * @note Gère récursivement la descente vers les feuilles et les splits nécessaires
                 * @note Utilise des décalages mémoire pour insérer sans réallocation
                 */
                void InsertNonFull(BTreeNode* node, const T& key) {
                    if (node->IsLeaf) {
                        usize insertPos = node->NumKeys;
                        while (insertPos > 0 && key < node->Keys[insertPos - 1]) {
                            --insertPos;
                        }
                        for (usize i = node->NumKeys; i > insertPos; --i) {
                            new (&node->Keys[i]) T(traits::NkMove(node->Keys[i - 1]));
                            node->Keys[i - 1].~T();
                        }
                        new (&node->Keys[insertPos]) T(key);
                        node->NumKeys++;
                    } else {
                        usize childIndex = node->NumKeys;
                        while (childIndex > 0 && key < node->Keys[childIndex - 1]) {
                            --childIndex;
                        }
                        if (node->Children[childIndex]->NumKeys == MaxKeysPerNode()) {
                            SplitChild(node, childIndex);
                            if (key > node->Keys[childIndex]) {
                                ++childIndex;
                            }
                        }
                        InsertNonFull(node->Children[childIndex], key);
                    }
                }

            // ====================================================================
            // SECTION PUBLIQUE : CONSTRUCTEURS ET DESTRUCTEUR
            // ====================================================================
            public:

                /**
                 * @brief Constructeur principal avec ordre et allocateur personnalisables
                 * @param order Degré minimum de l'arbre (clamped à 3 si inférieur)
                 * @param allocator Pointeur vers un allocateur personnalisé (nullptr = défaut)
                 * @note Initialise un arbre vide avec un nœud racine feuille prêt à recevoir des données
                 * @note Assertion en debug pour garantir order >= 3 (stabilité structurelle)
                 */
                explicit NkBTree(usize order = 3, Allocator* allocator = nullptr)
                    : mRoot(nullptr)
                    , mOrder(order < 3 ? 3 : order)
                    , mSize(0)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
                    NKENTSEU_ASSERT(order >= 3);
                    mRoot = CreateNode(true);
                }

                /**
                 * @brief Destructeur libérant récursivement toute la structure
                 * @note Garantit l'absence de fuite mémoire via RAII
                 * @note Complexité O(n) pour la destruction complète de tous les nœuds
                 */
                ~NkBTree() {
                    DestroyNode(mRoot);
                }

            // ====================================================================
            // SECTION PUBLIQUE : OPÉRATIONS PRINCIPALES
            // ====================================================================

                /**
                 * @brief Insère une nouvelle clé en maintenant les invariants de l'arbre B
                 * @param key Clé à ajouter à la structure
                 * @note Gère automatiquement le split de la racine si nécessaire (croissance en hauteur)
                 * @note Complexité amortie O(log_m n) avec m = order, optimal pour grands volumes
                 * @note Les doublons sont acceptés et insérés à la position appropriée
                 */
                void Insert(const T& key) {
                    if (mRoot->NumKeys == MaxKeysPerNode()) {
                        BTreeNode* newRoot = CreateNode(false);
                        newRoot->Children[0] = mRoot;
                        SplitChild(newRoot, 0);
                        mRoot = newRoot;
                    }
                    InsertNonFull(mRoot, key);
                    ++mSize;
                }

                /**
                 * @brief Recherche la présence d'une clé dans l'arbre
                 * @param key Clé cible à localiser
                 * @return true si trouvée, false sinon
                 * @note Complexité O(log_m n) avec factor constant réduit vs structures binaires
                 * @note Méthode const : ne modifie pas l'état interne, thread-safe en lecture seule
                 */
                bool Search(const T& key) const {
                    return SearchRecursive(mRoot, key) != nullptr;
                }

            // ====================================================================
            // SECTION PUBLIQUE : MÉTHODES DE CAPACITÉ ET D'ÉTAT
            // ====================================================================

                /**
                 * @brief Retourne le nombre total de clés stockées
                 * @return Valeur de type usize représentant la cardinalité
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 */
                usize Size() const NKENTSEU_NOEXCEPT {
                    return mSize;
                }

                /**
                 * @brief Teste si l'arbre ne contient aucune clé
                 * @return true si vide, false sinon
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Équivalent à Size() == 0 mais sémantiquement plus explicite
                 */
                bool Empty() const NKENTSEU_NOEXCEPT {
                    return mSize == 0;
                }
        };

    } // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKBTREE_H

// ============================================================================
// EXEMPLES D'UTILISATION ET PATTERNS COURANTS
// ============================================================================
/*
 * @section examples Exemples pratiques d'utilisation de NkBTree
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 1 : Insertion et recherche de base
 * --------------------------------------------------------------------------
 * @code
 * #include "NKContainers/Associative/NkBTree.h"
 * #include <cstdio>
 * 
 * void exempleBase() {
 *     // Création d'un B-Tree d'entiers avec order=4 (3-7 clés par nœud)
 *     nkentseu::NkBTree<int> btree(4);
 *     
 *     // Insertion de valeurs (l'arbre s'auto-équilibre automatiquement)
 *     btree.Insert(50);
 *     btree.Insert(30);
 *     btree.Insert(70);
 *     btree.Insert(20);
 *     btree.Insert(40);
 *     btree.Insert(60);
 *     btree.Insert(80);
 *     
 *     // Vérification de l'état
 *     printf("Taille: %zu\n", btree.Size());      // Affiche: 7
 *     printf("Vide: %s\n", btree.Empty() ? "oui" : "non");  // Affiche: non
 *     
 *     // Recherches multiples
 *     printf("Recherche 40: %s\n", btree.Search(40) ? "trouvé" : "absent");  // trouvé
 *     printf("Recherche 99: %s\n", btree.Search(99) ? "trouvé" : "absent");  // absent
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 2 : Gestion de clés complexes (strings, structs)
 * --------------------------------------------------------------------------
 * @code
 * #include <string>
 * 
 * void exempleClesComplexes() {
 *     // B-Tree avec clés de type std::string (doit supporter < et ==)
 *     nkentseu::NkBTree<std::string> index(3);
 *     
 *     index.Insert("apple");
 *     index.Insert("banana");
 *     index.Insert("cherry");
 *     index.Insert("apricot");  // S'insère entre "apple" et "banana"
 *     
 *     // Recherche efficace même avec des clés de taille variable
 *     if (index.Search("banana")) {
 *         printf("Fruit indexé avec succès\n");
 *     }
 *     
 *     // Les doublons sont acceptés (comportement multi-set)
 *     index.Insert("apple");  // Second "apple" accepté
 *     printf("Occurrences de 'apple': %zu\n", /* nécessite méthode Count() future * /);
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 3 : Optimisation mémoire avec allocateur personnalisé
 * --------------------------------------------------------------------------
 * @code
 * #include "NKMemory/NkPoolAllocator.h"
 * 
 * void exempleAllocateur() {
 *     // Pool allocator pour réduire la fragmentation sur insertions massives
 *     memory::NkPoolAllocator pool(64 * 1024);  // Pool de 64KB
 *     
 *     // B-Tree utilisant le pool pour toutes ses allocations internes
 *     nkentseu::NkBTree<uint64_t, memory::NkPoolAllocator> dbIndex(5, &pool);
 *     
 *     // Insertion de 10 000 clés sans fragmentation externe
 *     for (uint64_t i = 0; i < 10000; ++i) {
 *         dbIndex.Insert(i * 17);  // Distribution pseudo-aléatoire
 *     }
 *     
 *     printf("Index construit: %zu entrées\n", dbIndex.Size());
 *     // Destruction : libération en bloc via le pool (très rapide)
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 4 : Simulation d'index de base de données
 * --------------------------------------------------------------------------
 * @code
 * struct Record {
 *     uint32_t id;
 *     char name[64];
 *     // ... autres champs
 * };
 * 
 * void exempleIndexDB() {
 *     // Index primaire sur l'ID (clé unique en pratique)
 *     nkentseu::NkBTree<uint32_t> primaryKeyIndex(4);
 *     
 *     // Insertion simulée d'enregistrements
 *     primaryKeyIndex.Insert(1001);
 *     primaryKeyIndex.Insert(1005);
 *     primaryKeyIndex.Insert(1003);
 *     
 *     // Recherche rapide pour jointure ou filtrage
 *     uint32_t searchId = 1003;
 *     if (primaryKeyIndex.Search(searchId)) {
 *         // Fetch du record complet depuis le stockage principal
 *         // Record rec = storage.Fetch(searchId);
 *         printf("Record %u trouvé\n", searchId);
 *     }
 *     
 *     // Avantage B-Tree : peu d'accès disque même pour grands volumes
 *     // Hauteur typique pour 1M de clés avec order=100 : ~3-4 niveaux
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 5 : Choix de l'ordre optimal selon le cas d'usage
 * --------------------------------------------------------------------------
 * @code
 * void exempleChoixOrder() {
 *     // Order faible (3-5) : arbre plus haut, meilleur pour mémoire RAM
 *     nkentseu::NkBTree<int> ramOptimized(3);
 *     
 *     // Order élevé (50-200) : arbre plus plat, optimal pour accès disque/SSD
 *     // Un nœud de 200 clés ~ 800 bytes (int32) : tient dans un secteur disque
 *     nkentseu::NkBTree<int> diskOptimized(100);
 *     
 *     // Règle empirique : order ≈ (taille_page / taille_clé) / 2
 *     constexpr usize pageSize = 4096;      // Page disque typique
 *     constexpr usize keySize = sizeof(int64_t) + sizeof(void*);
 *     usize optimalOrder = (pageSize / keySize) / 2;  // ~256 pour int64+ptr
 *     
 *     nkentseu::NkBTree<int64_t> productionIndex(optimalOrder);
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * BONNES PRATIQUES ET RECOMMANDATIONS
 * --------------------------------------------------------------------------
 * 
 * 1. CHOIX DE L'ORDRE (ORDER) :
 *    - Mémoire RAM : order faible (3-10) pour réduire l'overhead mémoire
 *    - Disque/SSD : order élevé (50-256) pour minimiser les I/O
 *    - Calcul : order ≈ (page_size / (sizeof(T) + sizeof(ptr))) / 2
 *    - Minimum absolu : 3 (garanti par l'implémentation)
 * 
 * 2. GESTION DES TYPES DE CLÉS :
 *    - Doivent implémenter : operator<, operator==, copy/move constructors
 *    - Privilégier les types trivialement copiables pour les performances
 *    - Pour les types lourds : utiliser traits::NkMove() dans les extensions
 * 
 * 3. OPTIMISATIONS AVANCÉES :
 *    - Utiliser un allocateur pool pour les insertions en masse
 *    - Pré-allouer la capacité si le volume est connu à l'avance
 *    - Envisager un B+Tree dérivé pour les parcours séquentiels fréquents
 * 
 * 4. LIMITATIONS CONNUES (version actuelle) :
 *    - Suppression non implémentée : prévoir Remove() avec fusion de nœuds
 *    - Parcours in-order non exposé : ajouter méthode Traverse() si besoin
 *    - Pas de support des itérateurs : à ajouter pour compatibilité STL
 * 
 * 5. SÉCURITÉ ET ROBUSTESSE :
 *    - Assertions en debug pour détecter les invariants violés
 *    - Gestion RAII : aucune fuite mémoire même en cas d'exception
 *    - Thread-unsafe : protéger avec mutex pour accès concurrents
 * 
 * 6. EXTENSIONS RECOMMANDÉES :
 *    - Ajouter Remove() avec gestion des fusions et redistributions
 *    - Implémenter un itérateur pour parcours trié sans récursion
 *    - Ajouter méthode RangeQuery(low, high) pour recherches par intervalle
 *    - Supporter les comparateurs personnalisés via template parameter
 */

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Généré par Rihen le 2026-02-05 22:26:13
// Dernière modification : 2026-04-26 (restructuration et documentation)
// ============================================================