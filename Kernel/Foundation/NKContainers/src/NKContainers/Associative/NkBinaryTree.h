// -----------------------------------------------------------------------------
// FICHIER: NKContainers\Containers\Associative\NkBinaryTree.h
// DESCRIPTION: Structure d'arbre binaire générique non équilibré
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKBINARYTREE_H
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKBINARYTREE_H

    // ========================================================================
    // INCLUSIONS DES DÉPENDANCES
    // ========================================================================

    #include "NKCore/NkTypes.h"                    // Définition des types fondamentaux (usize, etc.)
    #include "NKContainers/NkContainersApi.h"      // Macros d'exportation de l'API conteneurs
    #include "NKCore/NkTraits.h"                   // Traits et métaprogrammation utilitaires
    #include "NKMemory/NkAllocator.h"              // Système d'allocation mémoire personnalisable
    #include "NKMemory/NkFunction.h"               // Wrapper pour les fonctions et callables
    #include "NKCore/Assert/NkAssert.h"            // Macros d'assertion pour le débogage
    #include "NKContainers/Sequential/NkVector.h"  // Conteneur vectoriel pour le parcours niveau par niveau

    // ========================================================================
    // ESPACE DE NOMS PRINCIPAL
    // ========================================================================

    namespace nkentseu {

        /**
         * @brief Classe template représentant un arbre binaire de recherche générique
         * 
         * Implémentation d'un arbre binaire simple sans mécanisme d'équilibrage automatique.
         * Chaque nœud contient une valeur et deux pointeurs vers ses enfants gauche et droit.
         * 
         * PROPRIÉTÉS :
         * - Structure hiérarchique avec relation parent-enfant
         * - Ordre partiel : valeurs gauches < parent < valeurs droites
         * - Gestion mémoire via allocateur personnalisable
         * - Support des parcours récursifs et itératifs
         * 
         * COMPLEXITÉ ALGORITHMIQUE :
         * - Insertion : O(h) où h = hauteur de l'arbre
         * - Recherche : O(h)
         * - Suppression : O(h) (non implémentée dans cette version)
         * - Cas dégénéré (liste chaînée) : O(n)
         * - Cas équilibré optimal : O(log n)
         * 
         * @tparam T Type des valeurs stockées dans l'arbre (doit supporter l'opérateur <)
         * @tparam Allocator Type de l'allocateur mémoire (défaut: memory::NkAllocator)
         * 
         * @note Les doublons sont ignorés lors de l'insertion pour préserver l'unicité
         * @note Thread-unsafe : synchronisation externe requise pour un usage concurrent
         */
        template<typename T, typename Allocator = memory::NkAllocator>
        class NkBinaryTree {

            // ====================================================================
            // SECTION PUBLIQUE : INTERFACE UTILISATEUR
            // ====================================================================
            public:

                /**
                 * @brief Structure interne représentant un nœud de l'arbre
                 * 
                 * Conteneur élémentaire stockant une valeur et les liens vers les enfants.
                 * Utilise une construction par placement new pour une gestion mémoire fine.
                 */
                struct Node {

                    T Value;          ///< Valeur stockée dans le nœud
                    Node* Left;       ///< Pointeur vers le sous-arbre gauche (valeurs inférieures)
                    Node* Right;      ///< Pointeur vers le sous-arbre droit (valeurs supérieures)

                    /**
                     * @brief Constructeur de nœud avec initialisation des membres
                     * @param value Valeur à encapsuler dans le nœud
                     */
                    Node(const T& value)
                        : Value(value)
                        , Left(nullptr)
                        , Right(nullptr) {
                    }
                };

                // ====================================================================
                // ALIASES DE TYPES POUR LA LISIBILITÉ
                // ====================================================================

                using ValueType = T;        ///< Alias du type de valeur stockée
                using SizeType = usize;     ///< Alias du type utilisé pour les tailles/comptages

            // ====================================================================
            // SECTION PRIVÉE : IMPLÉMENTATION INTERNE
            // ====================================================================
            private:

                // ====================================================================
                // MEMBRES DONNÉES : ÉTAT DE L'ARBRE
                // ====================================================================

                Node* mRoot;                    ///< Pointeur vers la racine de l'arbre (nullptr si vide)
                SizeType mSize;                 ///< Nombre total de nœuds présents dans l'arbre
                Allocator* mAllocator;          ///< Pointeur vers l'allocateur mémoire utilisé

                // ====================================================================
                // MÉTHODES DE GESTION MÉMOIRE DES NŒUDS
                // ====================================================================

                /**
                 * @brief Alloue et construit un nouveau nœud avec la valeur fournie
                 * @param value Valeur à copier dans le nouveau nœud
                 * @return Pointeur vers le nœud nouvellement créé
                 */
                Node* CreateNode(const T& value) {
                    Node* node = static_cast<Node*>(mAllocator->Allocate(sizeof(Node)));
                    new (node) Node(value);
                    return node;
                }

                /**
                 * @brief Détruit et libère la mémoire d'un nœud individuel
                 * @param node Pointeur vers le nœud à détruire
                 */
                void DestroyNode(Node* node) {
                    node->~Node();
                    mAllocator->Deallocate(node);
                }

                /**
                 * @brief Détruit récursivement tout un sous-arbre
                 * @param node Racine du sous-arbre à détruire
                 * @note Utilise un parcours post-ordre pour libérer les enfants avant le parent
                 */
                void DestroyTree(Node* node) {
                    if (!node) return;
                    DestroyTree(node->Left);
                    DestroyTree(node->Right);
                    DestroyNode(node);
                }

                // ====================================================================
                // MÉTHODES RÉCURSIVES D'INSERTION ET DE RECHERCHE
                // ====================================================================

                /**
                 * @brief Insère récursivement une valeur dans le sous-arbre approprié
                 * @param node Racine courante du sous-arbre de traitement
                 * @param value Valeur à insérer
                 * @return Pointeur vers la racine du sous-arbre après insertion
                 * @note Ignore silencieusement les valeurs dupliquées
                 */
                Node* InsertRecursive(Node* node, const T& value) {
                    if (!node) {
                        ++mSize;
                        return CreateNode(value);
                    }
                    if (value < node->Value) {
                        node->Left = InsertRecursive(node->Left, value);
                    } else if (node->Value < value) {
                        node->Right = InsertRecursive(node->Right, value);
                    }
                    return node;
                }

                /**
                 * @brief Recherche récursivement un nœud contenant la valeur cible
                 * @param node Racine courante du sous-arbre de recherche
                 * @param value Valeur recherchée
                 * @return Pointeur vers le nœud trouvé ou nullptr si absent
                 */
                Node* FindRecursive(Node* node, const T& value) const {
                    if (!node) return nullptr;
                    if (value < node->Value) {
                        return FindRecursive(node->Left, value);
                    } else if (node->Value < value) {
                        return FindRecursive(node->Right, value);
                    } else {
                        return node;
                    }
                }

                // ====================================================================
                // MÉTHODES DE PARCOURS RÉCURSIFS (TRAVERSE)
                // ====================================================================

                /**
                 * @brief Parcours en ordre (gauche, racine, droite) - valeurs triées croissantes
                 * @tparam Func Type du callable accepté en paramètre
                 * @param node Racine du sous-arbre à parcourir
                 * @param visitor Fonction appelée pour chaque valeur visitée
                 */
                template<typename Func>
                void InOrderRecursive(Node* node, Func& visitor) const {
                    if (!node) return;
                    InOrderRecursive(node->Left, visitor);
                    visitor(node->Value);
                    InOrderRecursive(node->Right, visitor);
                }

                /**
                 * @brief Parcours pré-ordre (racine, gauche, droite) - utile pour la sérialisation
                 * @tparam Func Type du callable accepté en paramètre
                 * @param node Racine du sous-arbre à parcourir
                 * @param visitor Fonction appelée pour chaque valeur visitée
                 */
                template<typename Func>
                void PreOrderRecursive(Node* node, Func& visitor) const {
                    if (!node) return;
                    visitor(node->Value);
                    PreOrderRecursive(node->Left, visitor);
                    PreOrderRecursive(node->Right, visitor);
                }

                /**
                 * @brief Parcours post-ordre (gauche, droite, racine) - utile pour la libération
                 * @tparam Func Type du callable accepté en paramètre
                 * @param node Racine du sous-arbre à parcourir
                 * @param visitor Fonction appelée pour chaque valeur visitée
                 */
                template<typename Func>
                void PostOrderRecursive(Node* node, Func& visitor) const {
                    if (!node) return;
                    PostOrderRecursive(node->Left, visitor);
                    PostOrderRecursive(node->Right, visitor);
                    visitor(node->Value);
                }

                // ====================================================================
                // MÉTHODES UTILITAIRES RÉCURSIVES
                // ====================================================================

                /**
                 * @brief Calcule récursivement la hauteur d'un sous-arbre
                 * @param node Racine du sous-arbre à analyser
                 * @return Hauteur maximale en nombre de niveaux (0 pour un arbre vide)
                 */
                usize HeightRecursive(Node* node) const {
                    if (!node) return 0;
                    usize leftHeight = HeightRecursive(node->Left);
                    usize rightHeight = HeightRecursive(node->Right);
                    return 1 + (leftHeight > rightHeight ? leftHeight : rightHeight);
                }

                /**
                 * @brief Crée une copie profonde récursive d'un sous-arbre
                 * @param node Racine du sous-arbre à copier
                 * @return Pointeur vers la racine du nouvel arbre copié
                 */
                Node* CopyTree(Node* node) {
                    if (!node) return nullptr;
                    Node* newNode = CreateNode(node->Value);
                    newNode->Left = CopyTree(node->Left);
                    newNode->Right = CopyTree(node->Right);
                    return newNode;
                }

                /**
                 * @brief Vérifie récursivement l'équilibrage de l'arbre (critère AVL)
                 * @param node Racine du sous-arbre à analyser
                 * @return Hauteur du sous-arbre si équilibré, -1 sinon
                 */
                int IsBalancedRecursive(Node* node) const {
                    if (!node) return 0;
                    int leftHeight = IsBalancedRecursive(node->Left);
                    if (leftHeight < 0) return -1;
                    int rightHeight = IsBalancedRecursive(node->Right);
                    if (rightHeight < 0) return -1;
                    int diff = leftHeight - rightHeight;
                    if (diff < -1 || diff > 1) return -1;
                    return 1 + (leftHeight > rightHeight ? leftHeight : rightHeight);
                }

            // ====================================================================
            // SECTION PUBLIQUE : CONSTRUCTEURS ET DESTRUCTEUR
            // ====================================================================
            public:

                /**
                 * @brief Constructeur par défaut avec allocateur optionnel
                 * @param allocator Pointeur vers un allocateur personnalisé (nullptr = défaut)
                 */
                explicit NkBinaryTree(Allocator* allocator = nullptr)
                    : mRoot(nullptr)
                    , mSize(0)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
                }

                /**
                 * @brief Constructeur de copie pour la sémantique de valeur
                 * @param other Arbre source à dupliquer profondément
                 */
                NkBinaryTree(const NkBinaryTree& other)
                    : mRoot(nullptr)
                    , mSize(other.mSize)
                    , mAllocator(other.mAllocator) {
                    mRoot = CopyTree(other.mRoot);
                }

                /**
                 * @brief Destructeur libérant toutes les ressources allouées
                 */
                ~NkBinaryTree() {
                    Clear();
                }

            // ====================================================================
            // SECTION PUBLIQUE : OPÉRATEURS D'AFFECTATION
            // ====================================================================

                /**
                 * @brief Opérateur d'affectation par copie avec gestion d'auto-affectation
                 * @param other Arbre source à copier
                 * @return Référence vers l'instance courante pour le chaînage
                 */
                NkBinaryTree& operator=(const NkBinaryTree& other) {
                    if (this != &other) {
                        Clear();
                        mRoot = CopyTree(other.mRoot);
                        mSize = other.mSize;
                    }
                    return *this;
                }

            // ====================================================================
            // SECTION PUBLIQUE : MÉTHODES DE CAPACITÉ ET D'ÉTAT
            // ====================================================================

                /**
                 * @brief Teste si l'arbre ne contient aucun élément
                 * @return true si l'arbre est vide, false sinon
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 */
                bool Empty() const NKENTSEU_NOEXCEPT {
                    return mSize == 0;
                }

                /**
                 * @brief Retourne le nombre total d'éléments stockés
                 * @return Nombre de nœuds dans l'arbre
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 */
                SizeType Size() const NKENTSEU_NOEXCEPT {
                    return mSize;
                }

                /**
                 * @brief Calcule la hauteur maximale de l'arbre
                 * @return Nombre de niveaux entre la racine et la feuille la plus profonde
                 * @note Complexité O(n) car parcours complet de l'arbre requis
                 */
                usize Height() const {
                    return HeightRecursive(mRoot);
                }

            // ====================================================================
            // SECTION PUBLIQUE : MODIFICATEURS DE L'ARBRE
            // ====================================================================

                /**
                 * @brief Supprime tous les éléments et libère la mémoire associée
                 * @note Complexité O(n) avec parcours post-ordre pour libération sécurisée
                 * @note L'arbre est réinitialisé à l'état vide après l'appel
                 */
                void Clear() {
                    DestroyTree(mRoot);
                    mRoot = nullptr;
                    mSize = 0;
                }

                /**
                 * @brief Insère une nouvelle valeur en respectant l'ordre binaire
                 * @param value Valeur à ajouter à l'arbre
                 * @note Les doublons sont ignorés pour préserver l'unicité des éléments
                 * @note Complexité moyenne O(log n), pire cas O(n) si arbre dégénéré
                 */
                void Insert(const T& value) {
                    mRoot = InsertRecursive(mRoot, value);
                }

            // ====================================================================
            // SECTION PUBLIQUE : MÉTHODES DE RECHERCHE ET D'ACCÈS
            // ====================================================================

                /**
                 * @brief Vérifie la présence d'une valeur dans l'arbre
                 * @param value Valeur à rechercher
                 * @return true si trouvée, false sinon
                 * @note Complexité moyenne O(log n), pire cas O(n)
                 */
                bool Contains(const T& value) const {
                    return FindRecursive(mRoot, value) != nullptr;
                }

                /**
                 * @brief Recherche une valeur et retourne un pointeur vers celle-ci
                 * @param value Valeur à rechercher
                 * @return Pointeur constant vers la valeur trouvée, nullptr si absente
                 * @note Permet d'accéder à la valeur sans copie, utile pour les types lourds
                 */
                const T* Find(const T& value) const {
                    Node* node = FindRecursive(mRoot, value);
                    return node ? &node->Value : nullptr;
                }

                /**
                 * @brief Fournit un accès direct au nœud racine
                 * @return Pointeur vers le nœud racine (nullptr si arbre vide)
                 * @note noexcept garantie, opération O(1)
                 */
                Node* GetRoot() const NKENTSEU_NOEXCEPT {
                    return mRoot;
                }

            // ====================================================================
            // SECTION PUBLIQUE : MÉTHODES DE PARCOURS (TRAVERSE)
            // ====================================================================

                /**
                 * @brief Parcours en ordre croissant avec application d'un visiteur
                 * @tparam Func Type du callable accepté (fonction, lambda, functor)
                 * @param visitor Fonction appliquée à chaque valeur dans l'ordre trié
                 * @note Idéal pour obtenir les éléments triés sans étape de sorting supplémentaire
                 */
                template<typename Func>
                void InOrder(Func visitor) const {
                    InOrderRecursive(mRoot, visitor);
                }

                /**
                 * @brief Parcours pré-ordre (racine en premier) avec visiteur
                 * @tparam Func Type du callable accepté
                 * @param visitor Fonction appliquée à chaque valeur rencontrée
                 * @note Utile pour la sérialisation ou la copie structurelle de l'arbre
                 */
                template<typename Func>
                void PreOrder(Func visitor) const {
                    PreOrderRecursive(mRoot, visitor);
                }

                /**
                 * @brief Parcours post-ordre (racine en dernier) avec visiteur
                 * @tparam Func Type du callable accepté
                 * @param visitor Fonction appliquée à chaque valeur rencontrée
                 * @note Particulièrement adapté aux opérations de libération ou d'agrégation
                 */
                template<typename Func>
                void PostOrder(Func visitor) const {
                    PostOrderRecursive(mRoot, visitor);
                }

                /**
                 * @brief Parcours niveau par niveau (breadth-first) avec visiteur
                 * @tparam Func Type du callable accepté
                 * @param visitor Fonction appliquée à chaque valeur par niveau hiérarchique
                 * @note Implémentation itérative utilisant un vector comme file d'attente
                 * @note Complexité O(n) en temps et espace pour stocker la file temporaire
                 */
                template<typename Func>
                void LevelOrder(Func visitor) const {
                    if (!mRoot) return;
                    NkVector<Node*> queue;
                    queue.PushBack(mRoot);
                    while (!queue.Empty()) {
                        Node* node = queue.Front();
                        queue.Erase(queue.begin());
                        visitor(node->Value);
                        if (node->Left) queue.PushBack(node->Left);
                        if (node->Right) queue.PushBack(node->Right);
                    }
                }

            // ====================================================================
            // SECTION PUBLIQUE : MÉTHODES UTILITAIRES ET ANALYSE
            // ====================================================================

                /**
                 * @brief Retourne la valeur minimale de l'arbre (plus à gauche)
                 * @return Copie de la valeur minimale
                 * @pre L'arbre ne doit pas être vide (assertion en mode debug)
                 * @note Complexité O(h) où h = hauteur, optimal pour un arbre équilibré
                 */
                T Min() const {
                    NKENTSEU_ASSERT(mRoot);
                    Node* node = mRoot;
                    while (node->Left) node = node->Left;
                    return node->Value;
                }

                /**
                 * @brief Retourne la valeur maximale de l'arbre (plus à droite)
                 * @return Copie de la valeur maximale
                 * @pre L'arbre ne doit pas être vide (assertion en mode debug)
                 * @note Complexité O(h) où h = hauteur, optimal pour un arbre équilibré
                 */
                T Max() const {
                    NKENTSEU_ASSERT(mRoot);
                    Node* node = mRoot;
                    while (node->Right) node = node->Right;
                    return node->Value;
                }

                /**
                 * @brief Vérifie si l'arbre respecte le critère d'équilibrage AVL
                 * @return true si pour tout nœud |hauteur_gauche - hauteur_droite| ≤ 1
                 * @note Complexité O(n) car nécessite le calcul des hauteurs de tous les sous-arbres
                 * @note Ne modifie pas l'arbre, méthode const
                 */
                bool IsBalanced() const {
                    return IsBalancedRecursive(mRoot) >= 0;
                }
        };

    } // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKBINARYTREE_H

// ============================================================================
// EXEMPLES D'UTILISATION ET PATTERNS COURANTS
// ============================================================================
/*
 * @section examples Exemples pratiques d'utilisation de NkBinaryTree
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 1 : Insertion et parcours de base
 * --------------------------------------------------------------------------
 * @code
 * #include "NKContainers/Associative/NkBinaryTree.h"
 * #include <cstdio>
 * 
 * void exempleBase() {
 *     // Création d'un arbre d'entiers avec allocateur par défaut
 *     nkentseu::NkBinaryTree<int> tree;
 *     
 *     // Insertion de valeurs (les doublons sont ignorés)
 *     tree.Insert(50);
 *     tree.Insert(30);
 *     tree.Insert(70);
 *     tree.Insert(20);
 *     tree.Insert(40);
 *     tree.Insert(50);  // Ignoré : doublon
 *     
 *     // Affichage de l'état
 *     printf("Taille: %zu\n", tree.Size());      // Affiche: 5
 *     printf("Hauteur: %zu\n", tree.Height());   // Affiche: 3
 *     printf("Minimum: %d\n", tree.Min());       // Affiche: 20
 *     printf("Maximum: %d\n", tree.Max());       // Affiche: 70
 *     
 *     // Parcours en ordre (valeurs triées croissantes)
 *     printf("Parcours InOrder: ");
 *     tree.InOrder([](int val) {
 *         printf("%d ", val);
 *     });
 *     // Résultat: 20 30 40 50 70
 *     printf("\n");
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 2 : Recherche et vérification d'appartenance
 * --------------------------------------------------------------------------
 * @code
 * void exempleRecherche() {
 *     nkentseu::NkBinaryTree<std::string> tree;
 *     
 *     tree.Insert("pomme");
 *     tree.Insert("banane");
 *     tree.Insert("cerise");
 *     
 *     // Vérification rapide de présence
 *     if (tree.Contains("banane")) {
 *         printf("Fruit trouvé!\n");
 *     }
 *     
 *     // Accès à la valeur trouvée (sans copie)
 *     const std::string* result = tree.Find("cerise");
 *     if (result) {
 *         printf("Valeur: %s\n", result->c_str());
 *     }
 *     
 *     // Recherche infructueuse
 *     if (!tree.Contains("orange")) {
 *         printf("Fruit non présent dans l'arbre\n");
 *     }
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 3 : Différents types de parcours
 * --------------------------------------------------------------------------
 * @code
 * void exempleParcours() {
 *     nkentseu::NkBinaryTree<int> tree{10, 5, 15, 3, 7, 12, 20};
 *     
 *     printf("InOrder (trié): ");
 *     tree.InOrder([](int v) { printf("%d ", v); });
 *     // Sortie: 3 5 7 10 12 15 20
 *     
 *     printf("\nPreOrder (racine d'abord): ");
 *     tree.PreOrder([](int v) { printf("%d ", v); });
 *     // Sortie: 10 5 3 7 15 12 20
 *     
 *     printf("\nPostOrder (racine en dernier): ");
 *     tree.PostOrder([](int v) { printf("%d ", v); });
 *     // Sortie: 3 7 5 12 20 15 10
 *     
 *     printf("\nLevelOrder (par niveau): ");
 *     tree.LevelOrder([](int v) { printf("%d ", v); });
 *     // Sortie: 10 5 15 3 7 12 20
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 4 : Gestion mémoire avec allocateur personnalisé
 * --------------------------------------------------------------------------
 * @code
 * #include "NKMemory/NkPoolAllocator.h"
 * 
 * void exempleAllocateur() {
 *     // Création d'un pool d'allocation pour optimiser les performances
 *     memory::NkPoolAllocator poolAllocator(1024);
 *     
 *     // Arbre utilisant l'allocateur personnalisé
 *     nkentseu::NkBinaryTree<float, memory::NkPoolAllocator> tree(&poolAllocator);
 *     
 *     for (int i = 0; i < 100; ++i) {
 *         tree.Insert(static_cast<float>(i) * 1.5f);
 *     }
 *     
 *     // L'arbre libérera automatiquement la mémoire via le pool
 *     // lors de sa destruction (RAII)
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 5 : Copie et sémantique de valeur
 * --------------------------------------------------------------------------
 * @code
 * void exempleCopie() {
 *     nkentseu::NkBinaryTree<int> original;
 *     original.Insert(1);
 *     original.Insert(2);
 *     original.Insert(3);
 *     
 *     // Copie profonde : les deux arbres sont indépendants
 *     nkentseu::NkBinaryTree<int> copie = original;
 *     
 *     copie.Insert(999);  // N'affecte pas l'original
 *     
 *     printf("Original size: %zu\n", original.Size());  // Affiche: 3
 *     printf("Copie size: %zu\n", copie.Size());        // Affiche: 4
 *     
 *     // Affectation avec gestion d'auto-affectation sécurisée
 *     original = original;  // Safe : vérification interne this != &other
 *     original = copie;     // Copie profonde avec libération préalable
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 6 : Analyse structurelle et équilibrage
 * --------------------------------------------------------------------------
 * @code
 * void exempleAnalyse() {
 *     nkentseu::NkBinaryTree<int> tree;
 *     
 *     // Insertion ordonnée -> arbre dégénéré (liste chaînée)
 *     for (int i = 1; i <= 10; ++i) {
 *         tree.Insert(i);
 *     }
 *     
 *     printf("Hauteur (dégénéré): %zu\n", tree.Height());  // Affiche: 10
 *     printf("Équilibré: %s\n", tree.IsBalanced() ? "oui" : "non");  // Affiche: non
 *     
 *     // Réinitialisation et insertion aléatoire pour meilleur équilibrage
 *     tree.Clear();
 *     tree.Insert(5); tree.Insert(3); tree.Insert(7);
 *     tree.Insert(2); tree.Insert(4); tree.Insert(6); tree.Insert(8);
 *     
 *     printf("Hauteur (équilibré): %zu\n", tree.Height());  // Affiche: 3
 *     printf("Équilibré: %s\n", tree.IsBalanced() ? "oui" : "non");  // Affiche: oui
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * BONNES PRATIQUES ET RECOMMANDATIONS
 * --------------------------------------------------------------------------
 * 
 * 1. CHOIX DU TYPE T :
 *    - Doit implémenter l'opérateur < pour le classement
 *    - Privilégier les types copiables ou déplaçables efficacement
 *    - Pour les types lourds, utiliser Find() pour éviter les copies inutiles
 * 
 * 2. GESTION DES PERFORMANCES :
 *    - Éviter les insertions triées séquentielles (risque de dégénérescence)
 *    - Pour de grands volumes, envisager un arbre auto-équilibré (AVL, Rouge-Noir)
 *    - Utiliser un allocateur pool pour réduire la fragmentation mémoire
 * 
 * 3. SÉCURITÉ THREAD :
 *    - La classe n'est pas thread-safe par conception
 *    - Protéger les accès concurrents avec un mutex externe si nécessaire
 *    - Privilégier la copie locale pour les lectures fréquentes
 * 
 * 4. GESTION DES ERREURS :
 *    - Min() et Max() assertent sur arbre vide en mode debug
 *    - Toujours vérifier Empty() avant d'appeler ces méthodes en production
 *    - Les méthodes de recherche retournent nullptr en cas d'échec (pattern safe)
 * 
 * 5. EXTENSIBILITÉ :
 *    - Pour ajouter la suppression, implémenter Remove() avec gestion des 3 cas :
 *      * Nœud feuille : suppression directe
 *      * Un enfant : remplacement par l'enfant
 *      * Deux enfants : remplacement par successeur ou prédécesseur
 */

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Généré par Rihen le 2026-02-05 22:26:13
// Dernière modification : 2026-04-26 (restructuration et documentation)
// ============================================================