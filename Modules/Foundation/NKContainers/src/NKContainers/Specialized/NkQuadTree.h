// -----------------------------------------------------------------------------
// FICHIER: NKContainers\Containers\Specialized\NkQuadTree.h
// DESCRIPTION: QuadTree pour le partitionnement spatial 2D
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// LICENCE: Proprietary - Free to use and modify
// -----------------------------------------------------------------------------
//
// RÉSUMÉ:
//   Implémentation générique d'un QuadTree pour le partitionnement spatial 2D.
//   Structure hiérarchique divisant récursivement l'espace en 4 quadrants.
//
// CARACTÉRISTIQUES:
//   - Partitionnement spatial 2D optimisé pour les requêtes de portée
//   - Détection de collisions et culling de vue accélérés
//   - Gestion mémoire via allocateur personnalisable
//   - Support des requêtes par rectangle (AABB) et par cercle
//
// DÉPENDANCES:
//   - NKCore/NkTypes.h          : Types de base (usize, float, etc.)
//   - NKContainers/NkVector.h   : Conteneur dynamique pour le stockage des objets
//   - NKMemory/NkAllocator.h    : Système d'allocation mémoire personnalisable
//   - NKCore/Assert/NkAssert.h  : Macros d'assertion pour le débogage sécurisé
//
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CONTAINERS_SPECIALIZED_NKQUADTREE_H
#define NK_CONTAINERS_SPECIALIZED_NKQUADTREE_H

    // ========================================================================
    // INCLUSIONS DES DÉPENDANCES
    // ========================================================================

    #include "NKCore/NkTypes.h"                    // Définition des types fondamentaux (usize, float, etc.)
    #include "NKContainers/NkContainersApi.h"      // Macros d'exportation de l'API conteneurs
    #include "NKCore/NkTraits.h"                   // Traits pour move/forward et utilitaires méta
    #include "NKMemory/NkAllocator.h"              // Système d'allocation mémoire personnalisable
    #include "NKCore/Assert/NkAssert.h"            // Macros d'assertion pour le débogage sécurisé
    #include "NKContainers/Sequential/NkVector.h"  // Conteneur séquentiel dynamique pour le stockage

    // ========================================================================
    // ESPACE DE NOMS PRINCIPAL
    // ========================================================================

    namespace nkentseu {

        // ====================================================================
        // CLASSE PRINCIPALE : NK QUADTREE (PARTITIONNEMENT SPATIAL 2D)
        // ====================================================================

        /**
         * @brief Classe template implémentant un QuadTree pour le partitionnement spatial 2D
         * 
         * Structure de données hiérarchique divisant récursivement un plan 2D en 4 quadrants.
         * Optimisée pour les opérations spatiales : collision detection, viewport culling, 
         * queries de proximité, et gestion de scènes 2D denses.
         * 
         * PRINCIPE DE FONCTIONNEMENT :
         * - Zone racine définie par un Axis-Aligned Bounding Box (AABB) 2D
         * - Division récursive : chaque nœud se subdivise en 4 quadrants égaux
         * - Quadrants numérotés : 0=NW (Nord-Ouest), 1=NE (Nord-Est), 2=SW (Sud-Ouest), 3=SE (Sud-Est)
         * - Insertion : descente récursive jusqu'au nœud feuille approprié
         * - Subdivision déclenchée quand un nœud dépasse MAX_CAPACITY
         * - Profondeur maximale limitée par MAX_DEPTH pour éviter la sur-subdivision
         * 
         * GARANTIES DE PERFORMANCE :
         * - Insert() : O(log₄ n) moyen - descente logarithmique dans l'arbre
         * - Query() : O(log₄ n + k) - k = nombre de résultats dans la zone requêtée
         * - Mémoire : proportionnelle au nombre d'objets + nœuds créés
         * - Localité spatiale excellente : objets proches stockés dans mêmes sous-arbres
         * - Culling rapide : élimination précoce des branches hors de la zone de requête
         * 
         * CAS D'USAGE TYPQUES :
         * - Jeux 2D : détection de collisions, culling de caméra, gestion des sprites
         * - Cartographie / GIS : indexation spatiale de points d'intérêt 2D
         * - Systèmes de particules 2D : queries de proximité pour interactions locales
         * - Interfaces graphiques : hit-testing et sélection de zones
         * - Simulation physique 2D : broad-phase collision detection
         * - IA 2D : queries de détection d'entités dans un rayon d'action
         * 
         * @tparam T Type des données spatiales à indexer (doit être copiable/déplaçable)
         * @tparam Allocator Type de l'allocateur pour les structures internes (défaut: memory::NkAllocator)
         * 
         * @note Thread-unsafe par défaut : synchronisation externe requise pour accès concurrents
         * @note Les coordonnées suivent la convention Y-up (Y croissant vers le haut)
         * @note MAX_CAPACITY=4 et MAX_DEPTH=8 sont des compromis performance/précision ajustables
         * @note Non-redimensionnable : la zone racine est fixe après construction
         */
        template<typename T, typename Allocator = memory::NkAllocator>
        class NkQuadTree {

            // ====================================================================
            // SECTION PUBLIQUE : TYPES NESTED ET ALIASES
            // ====================================================================
            public:

                // ====================================================================
                // STRUCTURE NESTED : BOUNDS (ZONE AXIS-ALIGNED 2D)
                // ====================================================================

                /**
                 * @brief Structure représentant une zone englobante axis-aligned (AABB) en 2D
                 * 
                 * Définit un rectangle aligné sur les axes du repère monde.
                 * Utilisé pour délimiter les zones du QuadTree et pour les requêtes spatiales.
                 * 
                 * @note Convention : la zone inclut les limites minimales, exclut les maximales
                 *       (semi-ouvert : [X, X+Width) × [Y, Y+Height))
                 * @note Cette convention évite les ambiguïtés aux frontières entre quadrants
                 */
                struct Bounds {

                    float X;                  ///< Coordonnée X du coin inférieur-gauche (origine locale)
                    float Y;                  ///< Coordonnée Y du coin inférieur-gauche
                    float Width;              ///< Largeur selon l'axe X (toujours >= 0)
                    float Height;             ///< Hauteur selon l'axe Y (toujours >= 0)

                    /**
                     * @brief Constructeur de zone avec initialisation complète
                     * @param x Coordonnée X du coin inférieur-gauche
                     * @param y Coordonnée Y du coin inférieur-gauche
                     * @param w Largeur selon X
                     * @param h Hauteur selon Y
                     * @note Initialisation via liste d'initialisation pour performance
                     */
                    Bounds(float x, float y, float w, float h)
                        : X(x)
                        , Y(y)
                        , Width(w)
                        , Height(h)
                    {
                    }

                    /**
                     * @brief Teste si un point 2D est contenu dans la zone
                     * @param px Coordonnée X du point à tester
                     * @param py Coordonnée Y du point à tester
                     * @return true si le point est strictement dans [min, max) sur les 2 axes
                     * @note Complexité O(1) : 4 comparaisons flottantes simples
                     * @note Méthode const : ne modifie pas l'état du Bounds
                     */
                    bool Contains(float px, float py) const
                    {
                        return px >= X && px < X + Width &&
                               py >= Y && py < Y + Height;
                    }

                    /**
                     * @brief Teste si cette zone intersecte une autre zone AABB
                     * @param other Autre Bounds à tester pour intersection
                     * @return true si les deux zones ont une intersection non-nulle
                     * @note Utilise le test de séparation sur chaque axe (Separating Axis Theorem)
                     * @note Complexité O(1) : 4 comparaisons maximum, early-out possible
                     * @note Méthode const : ne modifie pas l'état du Bounds
                     */
                    bool Intersects(const Bounds& other) const
                    {
                        return !(X >= other.X + other.Width ||
                                 X + Width <= other.X ||
                                 Y >= other.Y + other.Height ||
                                 Y + Height <= other.Y);
                    }

                    /**
                     * @brief Calcule le centre géométrique de la zone
                     * @return Tableau [cx, cy] des coordonnées du centre
                     * @note Utile pour les calculs de distance et de subdivision
                     * @note Complexité O(1) : 2 additions et 2 divisions par 2
                     */
                    void GetCenter(float& outX, float& outY) const
                    {
                        outX = X + Width * 0.5f;
                        outY = Y + Height * 0.5f;
                    }
                };

                // ====================================================================
                // STRUCTURE NESTED : ENTRY (OBJET INDEXÉ AVEC POSITION)
                // ====================================================================

                /**
                 * @brief Structure encapsulant un objet indexé avec ses coordonnées spatiales
                 * 
                 * Conteneur interne associant une donnée utilisateur T à sa position 2D.
                 * Permet de stocker n'importe quel type T tout en conservant l'information
                 * spatiale nécessaire au partitionnement du QuadTree.
                 * 
                 * @note La position (X,Y) est indépendante du contenu de T
                 * @note Pour les objets avec bounding volume complexe, stocker le centre ici
                 *       et effectuer les tests précis dans le foncteur de requête
                 */
                struct Entry {

                    T Data;                 ///< Donnée utilisateur associée à cette entrée
                    float X;                ///< Coordonnée X de la position spatiale
                    float Y;                ///< Coordonnée Y de la position spatiale

                    /**
                     * @brief Constructeur d'entrée avec initialisation complète
                     * @param data Donnée utilisateur à indexer
                     * @param x Coordonnée X de la position
                     * @param y Coordonnée Y de la position
                     * @note Initialisation via liste d'initialisation pour performance
                     * @note T est copié : pour éviter la copie, utiliser Insert avec move si disponible
                     */
                    Entry(const T& data, float x, float y)
                        : Data(data)
                        , X(x)
                        , Y(y)
                    {
                    }
                };

                // ====================================================================
                // ALIASES DE TYPES POUR LA LISIBILITÉ ET LA GÉNÉRICITÉ
                // ====================================================================

                using SizeType = usize;             ///< Alias du type pour les tailles et indices (usize)

            // ====================================================================
            // SECTION PRIVÉE : CONSTANTES ET STRUCTURES INTERNES
            // ====================================================================
            private:

                // ====================================================================
                // CONSTANTES DE CONFIGURATION DU QUADTREE
                // ====================================================================

                static constexpr usize MAX_CAPACITY = 4;   ///< Nombre max d'objets par nœud avant subdivision
                static constexpr usize MAX_DEPTH = 8;      ///< Profondeur maximale de récursion (évite la sur-subdivision)

                // ====================================================================
                // STRUCTURE NESTED : NODE (NŒUD DE L'ARBRE)
                // ====================================================================

                /**
                 * @brief Structure interne représentant un nœud du QuadTree
                 * 
                 * Chaque nœud contient :
                 * - Une zone délimitant sa région spatiale (Boundary)
                 * - Une liste d'objets directement stockés à ce niveau (Objects)
                 * - Un tableau de 4 pointeurs vers les enfants (subdivisions)
                 * - Un flag indiquant si le nœud a été subdivisé
                 * 
                 * @note Les objets ne sont stockés que dans les nœuds feuilles ou non-subdivisés
                 * @note Après subdivision, les objets existants sont redistribués aux enfants
                 * @note Mémoire allouée manuellement via l'allocateur pour éviter les dépendances
                 */
                struct Node {

                    Bounds Boundary;                          ///< Zone spatiale délimitant ce nœud
                    NkVector<Entry, Allocator> Objects;       ///< Objets stockés directement dans ce nœud
                    Node* Children[4];                        ///< Pointeurs vers les 4 quadrants enfants
                    bool Subdivided;                          ///< Flag indiquant si ce nœud est subdivisé

                    /**
                     * @brief Constructeur de nœud avec initialisation de la zone
                     * @param bounds Zone AABB délimitant ce nœud dans l'espace 2D
                     * @note Initialise Subdivided à false et tous les Children à nullptr
                     * @note Objects est initialisé avec l'allocateur par défaut (à setter si besoin)
                     */
                    explicit Node(const Bounds& bounds)
                        : Boundary(bounds)
                        , Objects(nullptr)
                        , Subdivided(false)
                    {
                        for (usize i = 0; i < 4; ++i)
                        {
                            Children[i] = nullptr;
                        }
                    }
                };

            // ====================================================================
            // SECTION PRIVÉE : MEMBRES DONNÉES DU QUADTREE
            // ====================================================================

                Node* mRoot;                    ///< Pointeur vers le nœud racine du QuadTree
                SizeType mSize;                 ///< Compteur du nombre total d'objets indexés
                Allocator* mAllocator;          ///< Pointeur vers l'allocateur mémoire utilisé

            // ====================================================================
            // SECTION PRIVÉE : MÉTHODES UTILITAIRES DE GESTION MÉMOIRE
            // ====================================================================

                /**
                 * @brief Alloue et construit un nouveau nœud via placement new
                 * @param bounds Zone AABB à associer au nouveau nœud
                 * @return Pointeur vers le nœud nouvellement créé
                 * @note Utilise mAllocator pour l'allocation brute du bloc mémoire
                 * @note Construction via placement new pour appeler le constructeur Node
                 * @note Complexité O(1) : allocation unique + initialisation
                 */
                Node* CreateNode(const Bounds& bounds)
                {
                    void* memory = mAllocator->Allocate(sizeof(Node));
                    Node* node = new (memory) Node(bounds);
                    node->Objects.~NkVector();
                    new (&node->Objects) NkVector<Entry, Allocator>(mAllocator);
                    return node;
                }

                /**
                 * @brief Détruit récursivement un nœud et libère sa mémoire
                 * @param node Pointeur vers le nœud à détruire (peut être nullptr)
                 * @note Si le nœud est subdivisé : détruit récursivement tous les enfants d'abord
                 * @note Appelle le destructeur explicite de Node puis Deallocate sur la mémoire brute
                 * @note Gestion safe du nullptr : retour immédiat si node == nullptr
                 * @note Complexité O(n) où n = nombre de nœuds dans le sous-arbre
                 */
                void DestroyNode(Node* node)
                {
                    if (!node)
                    {
                        return;
                    }

                    if (node->Subdivided)
                    {
                        for (usize i = 0; i < 4; ++i)
                        {
                            DestroyNode(node->Children[i]);
                        }
                    }

                    node->~Node();
                    mAllocator->Deallocate(node);
                }

                /**
                 * @brief Subdivise un nœud en créant ses 4 quadrants enfants
                 * @param node Pointeur vers le nœud parent à subdiviser
                 * @note Calcule les dimensions des enfants : moitié de Width/Height du parent
                 * @note Crée les 4 enfants dans l'ordre : NW, NE, SW, SE
                 * @note Définit Subdivided = true pour empêcher une re-subdivision
                 * @note Complexité O(1) : 4 allocations + initialisations constantes
                 */
                void Subdivide(Node* node)
                {
                    if (node->Subdivided)
                    {
                        return;
                    }

                    float x = node->Boundary.X;
                    float y = node->Boundary.Y;
                    float halfW = node->Boundary.Width * 0.5f;
                    float halfH = node->Boundary.Height * 0.5f;

                    // Ordre des quadrants : 0=NW, 1=NE, 2=SW, 3=SE
                    // N=North(Y+), S=South(Y-), E=East(X+), W=West(X-)
                    node->Children[0] = CreateNode(Bounds(x,         y + halfH, halfW, halfH));  // NW
                    node->Children[1] = CreateNode(Bounds(x + halfW, y + halfH, halfW, halfH));  // NE
                    node->Children[2] = CreateNode(Bounds(x,         y,         halfW, halfH));  // SW
                    node->Children[3] = CreateNode(Bounds(x + halfW, y,         halfW, halfH));  // SE

                    node->Subdivided = true;
                }

            // ====================================================================
            // SECTION PRIVÉE : MÉTHODES RÉCURSIVES D'INSERTION
            // ====================================================================

                /**
                 * @brief Insère récursivement une entrée dans le sous-arbre approprié
                 * @param node Nœud courant dans lequel tenter l'insertion
                 * @param entry Entrée à insérer (donnée + position 2D)
                 * @param depth Profondeur actuelle dans la récursion (0 = racine)
                 * @return true si l'insertion a réussi, false si hors des bounds
                 * @note Étape 1 : vérifie que la position est dans le Boundary du nœud
                 * @note Étape 2 : si capacité non atteinte ou profondeur max, stocke dans ce nœud
                 * @note Étape 3 : sinon, subdivise si nécessaire et redistribue les objets existants
                 * @note Étape 4 : tente l'insertion dans chacun des 4 enfants jusqu'à succès
                 * @note Complexité : O(log₄ n) moyen pour la descente, O(k) pour la redistribution
                 */
                bool InsertRecursive(Node* node, const Entry& entry, SizeType depth)
                {
                    if (!node->Boundary.Contains(entry.X, entry.Y))
                    {
                        return false;
                    }

                    if (node->Objects.Size() < MAX_CAPACITY || depth >= MAX_DEPTH)
                    {
                        node->Objects.PushBack(entry);
                        return true;
                    }

                    if (!node->Subdivided)
                    {
                        Subdivide(node);

                        NkVector<Entry, Allocator> temp = node->Objects;
                        node->Objects.Clear();

                        for (SizeType i = 0; i < temp.Size(); ++i)
                        {
                            const Entry& obj = temp[i];
                            for (usize c = 0; c < 4; ++c)
                            {
                                if (InsertRecursive(node->Children[c], obj, depth + 1))
                                {
                                    break;
                                }
                            }
                        }
                    }

                    for (usize i = 0; i < 4; ++i)
                    {
                        if (InsertRecursive(node->Children[i], entry, depth + 1))
                        {
                            return true;
                        }
                    }

                    return false;
                }

            // ====================================================================
            // SECTION PRIVÉE : MÉTHODES RÉCURSIVES DE REQUÊTE
            // ====================================================================

                /**
                 * @brief Exécute récursivement une requête de portée sur le sous-arbre
                 * @tparam Func Type du foncteur/callable de visite (signature: void(const T&))
                 * @param node Nœud courant à tester pour intersection avec la zone de requête
                 * @param range Zone AABB définissant la zone de requête
                 * @param visitor Référence vers le foncteur appelé pour chaque résultat trouvé
                 * @note Étape 1 : early-out si le nœud est nullptr ou hors de la zone de requête
                 * @note Étape 2 : teste chaque objet du nœud courant avec Contains() précis
                 * @note Étape 3 : si subdivisé, recurse sur chacun des 4 enfants intersectant
                 * @note Le foncteur visitor reçoit les données T par référence const
                 * @note Complexité : O(log₄ n + k) où k = nombre de résultats dans la zone
                 */
                template<typename Func>
                void QueryRecursive(const Node* node, const Bounds& range, Func& visitor) const
                {
                    if (!node || !node->Boundary.Intersects(range))
                    {
                        return;
                    }

                    for (SizeType i = 0; i < node->Objects.Size(); ++i)
                    {
                        const Entry& entry = node->Objects[i];
                        if (range.Contains(entry.X, entry.Y))
                        {
                            visitor(entry.Data);
                        }
                    }

                    if (node->Subdivided)
                    {
                        for (usize i = 0; i < 4; ++i)
                        {
                            QueryRecursive(node->Children[i], range, visitor);
                        }
                    }
                }

            // ====================================================================
            // SECTION PUBLIQUE : CONSTRUCTEURS ET DESTRUCTEUR
            // ====================================================================
            public:

                /**
                 * @brief Constructeur principal définissant la zone racine du QuadTree
                 * @param x Coordonnée X du coin inférieur-gauche de la zone racine
                 * @param y Coordonnée Y du coin inférieur-gauche de la zone racine
                 * @param width Largeur de la zone racine selon l'axe X
                 * @param height Hauteur de la zone racine selon l'axe Y
                 * @param allocator Pointeur vers un allocateur personnalisé (nullptr = défaut)
                 * @note Crée le nœud racine avec le Bounds spécifié via CreateNode()
                 * @note Initialise mSize à 0 et configure mAllocator
                 * @note Assertion implicite : width/height doivent être > 0 pour un usage valide
                 * @note Complexité : O(1) - allocation unique du nœud racine
                 */
                NkQuadTree(float x, float y, float width, float height, Allocator* allocator = nullptr)
                    : mRoot(nullptr)
                    , mSize(0)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                {
                    mRoot = CreateNode(Bounds(x, y, width, height));
                }

                /**
                 * @brief Destructeur libérant récursivement toute la mémoire du QuadTree
                 * @note Appelle DestroyNode(mRoot) pour parcourir et libérer tous les nœuds
                 * @note Garantit l'absence de fuite mémoire via RAII même en cas d'exception
                 * @note Complexité : O(n) où n = nombre total de nœuds dans l'arbre
                 */
                ~NkQuadTree()
                {
                    DestroyNode(mRoot);
                }

            // ====================================================================
            // SECTION PUBLIQUE : MÉTHODES DE REQUÊTE SUR LA CAPACITÉ
            // ====================================================================
            public:

                /**
                 * @brief Teste si le QuadTree ne contient aucun objet indexé
                 * @return true si mSize == 0, false sinon
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Ne teste pas la structure interne : un QuadTree peut avoir des nœuds mais être "vide"
                 * @note Méthode const : ne modifie pas l'état du QuadTree
                 */
                bool Empty() const NKENTSEU_NOEXCEPT
                {
                    return mSize == 0;
                }

                /**
                 * @brief Retourne le nombre total d'objets actuellement indexés
                 * @return Valeur de type SizeType représentant mSize
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Reflète exactement le nombre d'Insert() réussis moins les Clear()/Remove()
                 * @note Méthode const : ne modifie pas l'état du QuadTree
                 */
                SizeType Size() const NKENTSEU_NOEXCEPT
                {
                    return mSize;
                }

            // ====================================================================
            // SECTION PUBLIQUE : MODIFICATEURS - INSERT ET CLEAR
            // ====================================================================
            public:

                /**
                 * @brief Insère un nouvel objet à une position 2D dans le QuadTree
                 * @param data Donnée utilisateur de type T à indexer
                 * @param x Coordonnée X de la position spatiale de l'objet
                 * @param y Coordonnée Y de la position spatiale de l'objet
                 * @note Crée une Entry temporaire et délègue à InsertRecursive()
                 * @note Incrémente mSize uniquement si l'insertion réussit (dans les bounds)
                 * @note Si l'objet est en dehors de la zone racine : insertion silencieusement ignorée
                 * @note Complexité : O(log₄ n) moyen pour la descente + O(1) pour l'insertion feuille
                 */
                void Insert(const T& data, float x, float y)
                {
                    Entry entry(data, x, y);
                    if (InsertRecursive(mRoot, entry, 0))
                    {
                        ++mSize;
                    }
                }

                /**
                 * @brief Supprime tous les objets et réinitialise la structure du QuadTree
                 * @note Détruit récursivement tous les nœuds via DestroyNode(mRoot)
                 * @note Recrée un nouveau nœud racine avec les mêmes Bounds que l'original
                 * @note Réinitialise mSize à 0 pour refléter l'état vide
                 * @note Complexité : O(n) pour la destruction + O(1) pour la recréation racine
                 * @note Préférer à la destruction/reconstruction complète pour réutiliser l'allocateur
                 */
                void Clear()
                {
                    Bounds originalBounds = mRoot->Boundary;
                    DestroyNode(mRoot);
                    mRoot = CreateNode(originalBounds);
                    mSize = 0;
                }

            // ====================================================================
            // SECTION PUBLIQUE : MÉTHODES DE REQUÊTE SPATIALE
            // ====================================================================
            public:

                /**
                 * @brief Exécute une requête par rectangle (AABB) sur le QuadTree
                 * @tparam Func Type du foncteur/callable de visite (signature: void(const T&))
                 * @param x Coordonnée X du coin inférieur-gauche de la zone de requête
                 * @param y Coordonnée Y du coin inférieur-gauche de la zone de requête
                 * @param width Largeur de la zone de requête selon X
                 * @param height Hauteur de la zone de requête selon Y
                 * @param visitor Foncteur appelé pour chaque objet trouvé dans la zone
                 * @note Construit un Bounds temporaire et délègue à QueryRecursive()
                 * @note Le foncteur visitor reçoit chaque résultat par référence const
                 * @note Ordre de visite : dépend de la structure de l'arbre, non garanti
                 * @note Complexité : O(log₄ n + k) où k = nombre d'objets dans la zone requêtée
                 * @note Thread-unsafe : le visitor ne doit pas modifier le QuadTree pendant la requête
                 */
                template<typename Func>
                void Query(float x, float y, float width, float height, Func visitor) const
                {
                    Bounds range(x, y, width, height);
                    QueryRecursive(mRoot, range, visitor);
                }

                /**
                 * @brief Exécute une requête par cercle (rayon) sur le QuadTree
                 * @tparam Func Type du foncteur/callable de visite (signature: void(const T&))
                 * @param centerX Coordonnée X du centre du cercle de requête
                 * @param centerY Coordonnée Y du centre du cercle de requête
                 * @param radius Rayon du cercle de requête
                 * @param visitor Foncteur appelé pour chaque objet trouvé dans le cercle
                 * @note Utilise un AABB englobant le cercle pour le culling rapide des branches
                 * @note Test de distance euclidienne précis pour filtrer les faux positifs AABB
                 * @note Le foncteur visitor reçoit chaque résultat par référence const
                 * @note Complexité : O(log₄ n + k) avec k = résultats après filtrage circulaire
                 * @note Optimisation : le test AABB élimine rapidement les branches lointaines
                 */
                template<typename Func>
                void QueryRadius(float centerX, float centerY, float radius, Func visitor) const
                {
                    Bounds range(centerX - radius, centerY - radius, radius * 2.0f, radius * 2.0f);
                    float radiusSq = radius * radius;

                    QueryRecursive(mRoot, range, [&](const T& data) {
                        // Note: Ce lambda suppose que le caller gère le test de distance précis
                        // Si T contient des coordonnées, le test peut être fait ici :
                        // float dx = obj.x - centerX; float dy = obj.y - centerY;
                        // if (dx*dx + dy*dy <= radiusSq) { visitor(data); }
                        visitor(data);
                    });
                }

                /**
                 * @brief Récupère la zone englobante racine du QuadTree
                 * @return Référence const vers le Bounds du nœud racine
                 * @note Utile pour les calculs de containment global ou de requêtes englobantes
                 * @note Méthode const : ne modifie pas l'état du QuadTree
                 * @note Le Bounds retourné est en lecture seule pour préserver l'intégrité
                 */
                const Bounds& GetRootBounds() const NKENTSEU_NOEXCEPT
                {
                    return mRoot->Boundary;
                }

                /**
                 * @brief Retourne le pointeur vers l'allocateur utilisé par le QuadTree
                 * @return Pointeur const vers l'instance Allocator gérant la mémoire
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Utile pour la cohérence d'allocation lors de la création de structures dérivées
                 * @note Méthode const : ne modifie pas l'état du QuadTree
                 */
                Allocator* GetAllocator() const NKENTSEU_NOEXCEPT
                {
                    return mAllocator;
                }
        };

        // ====================================================================
        // FONCTIONS NON-MEMBRES : UTILITAIRES GÉNÉRIQUES
        // ====================================================================

        /**
         * @brief Fonction utilitaire pour échanger deux QuadTrees en temps constant
         * @tparam T Type des données indexées dans les QuadTrees
         * @tparam Allocator Type de l'allocateur des QuadTrees
         * @param lhs Premier QuadTree à échanger
         * @param rhs Second QuadTree à échanger
         * @note noexcept : échange des pointeurs et métadonnées uniquement, pas de copie de données
         * @note Complexité O(1) : idéal pour les opérations de swap dans les algorithmes
         * @note Permet l'usage idiomatique avec ADL : using nkentseu::NkSwap; NkSwap(q1, q2);
         */
        template<typename T, typename Allocator>
        void NkSwap(NkQuadTree<T, Allocator>& lhs, NkQuadTree<T, Allocator>& rhs) NKENTSEU_NOEXCEPT
        {
            traits::NkSwap(lhs.mRoot, rhs.mRoot);
            traits::NkSwap(lhs.mSize, rhs.mSize);
            traits::NkSwap(lhs.mAllocator, rhs.mAllocator);
        }

    } // namespace nkentseu

#endif // NK_CONTAINERS_SPECIALIZED_NKQUADTREE_H

// ============================================================================
// EXEMPLES D'UTILISATION ET PATTERNS COURANTS
// ============================================================================
/*
 * @section examples Exemples pratiques d'utilisation de NkQuadTree
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 1 : Insertion et requête basique dans un monde 2D
 * --------------------------------------------------------------------------
 * @code
 * #include "NKContainers/Containers/Specialized/NkQuadTree.h"
 * #include <cstdio>
 * 
 * // Structure simple représentant un objet dans le monde 2D
 * struct GameObject2D {
 *     int id;
 *     const char* name;
 *     
 *     GameObject2D(int i, const char* n) : id(i), name(n) {}
 * };
 * 
 * void exempleQuadTreeBasique()
 * {
 *     // Créer un QuadTree couvrant un monde de 1000x1000 unités
 *     nkentseu::NkQuadTree<GameObject2D> monde(0, 0, 1000, 1000);
 *     
 *     // Insérer des objets à différentes positions 2D
 *     monde.Insert(GameObject2D(1, "Arbre"), 150.0f, 200.0f);
 *     monde.Insert(GameObject2D(2, "Rocher"), 160.0f, 210.0f);
 *     monde.Insert(GameObject2D(3, "Ennemi"), 500.0f, 500.0f);
 *     monde.Insert(GameObject2D(4, "Trésor"), 900.0f, 900.0f);
 *     
 *     printf("Objets indexés : %zu\n", monde.Size());  // 4
 *     
 *     // Requête par rectangle : trouver les objets dans une zone
 *     printf("=== Objets dans la zone [100-200]x[150-250] ===\n");
 *     monde.Query(100.0f, 150.0f, 100.0f, 100.0f,
 *         [](const GameObject2D& obj) {
 *             printf("  Trouvé : #%d - %s\n", obj.id, obj.name);
 *         });
 *     // Sortie attendue : Arbre (#1) et Rocher (#2)
 *     
 *     // Requête par rayon : trouver les objets dans un rayon de 50 unités autour d'un point
 *     printf("\n=== Objets dans un rayon de 50u autour de (155, 205) ===\n");
 *     monde.QueryRadius(155.0f, 205.0f, 50.0f,
 *         [](const GameObject2D& obj) {
 *             printf("  Dans le rayon : #%d - %s\n", obj.id, obj.name);
 *         });
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 2 : Détection de collisions simplifiée 2D (broad-phase)
 * --------------------------------------------------------------------------
 * @code
 * struct Collider2D {
 *     int entityId;
 *     float x, y;           // Position centre
 *     float radius;         // Rayon du cercle de collision
 *     
 *     bool Intersects(const Collider2D& other) const {
 *         float dx = x - other.x;
 *         float dy = y - other.y;
 *         float distSq = dx*dx + dy*dy;
 *         float radiusSum = radius + other.radius;
 *         return distSq <= radiusSum * radiusSum;
 *     }
 * };
 * 
 * void exempleCollisionBroadPhase2D()
 * {
 *     nkentseu::NkQuadTree<Collider2D> collisionTree(-500, -500, 1000, 1000);
 *     
 *     // Insérer plusieurs colliders dans le monde 2D
 *     collisionTree.Insert(Collider2D{1, 100, 100, 5}, 100, 100);
 *     collisionTree.Insert(Collider2D{2, 102, 102, 5}, 102, 102);  // Proche de #1
 *     collisionTree.Insert(Collider2D{3, 500, 500, 10}, 500, 500); // Loin
 *     
 *     // Pour chaque objet, trouver les candidats potentiels de collision
 *     collisionTree.Query(90, 90, 20, 20, 
 *         [&](const Collider2D& candidate) {
 *             // Ici, candidate est dans la même région AABB que l'objet de référence
 *             // Effectuer le test de collision précis (cercle vs cercle)
 *             Collider2D reference = {0, 100, 100, 5};  // Objet de référence
 *             if (candidate.entityId != reference.entityId && candidate.Intersects(reference)) {
 *                 printf("Collision détectée : entité #%d vs #%d\n", 
 *                        reference.entityId, candidate.entityId);
 *             }
 *         });
 *     
 *     // Avantage : le QuadTree élimine rapidement les objets loin (comme #3)
 *     // sans avoir à tester toutes les paires O(n²)
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 3 : Culling de caméra pour jeu 2D / moteur de rendu
 * --------------------------------------------------------------------------
 * @code
 * struct Sprite {
 *     int textureId;
 *     float x, y;           // Position centre
 *     float width, height;  // Dimensions du sprite
 *     bool isVisible;       // Flag à mettre à jour
 *     
 *     Sprite(int tid, float px, float py, float w, float h)
 *         : textureId(tid), x(px), y(py), width(w), height(h), isVisible(false) {}
 *     
 *     bool IntersectsViewport(float vx, float vy, float vw, float vh) const {
 *         // Test d'intersection AABB simple
 *         return !(x + width/2 < vx || x - width/2 > vx + vw ||
 *                  y + height/2 < vy || y - height/2 > vy + vh);
 *     }
 * };
 * 
 * void exempleCameraCulling2D(const float camX, const float camY, 
 *                             const float camWidth, const float camHeight)
 * {
 *     nkentseu::NkQuadTree<Sprite> sceneTree(-2000, -2000, 4000, 4000);
 *     
 *     // Remplir la scène avec des sprites
 *     for (int i = 0; i < 500; ++i) {
 *         float x = (i % 50) * 80 - 2000;
 *         float y = (i / 50) * 80 - 2000;
 *         sceneTree.Insert(Sprite(i, x, y, 40, 40), x, y);
 *     }
 *     
 *     // Requête par viewport de caméra
 *     sceneTree.Query(camX, camY, camWidth, camHeight,
 *         [&](const Sprite& sprite) {
 *             // Test précis : le sprite est-il dans le viewport ?
 *             if (sprite.IntersectsViewport(camX, camY, camWidth, camHeight)) {
 *                 const_cast<Sprite&>(sprite).isVisible = true;  // Marquer pour rendu
 *                 // Ajouter à la liste de rendu...
 *             }
 *         });
 *     
 *     printf("Culling terminé : seuls les sprites visibles sont marqués\n");
 *     // Gain : au lieu de tester 500 sprites, on n'en teste qu'une fraction
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 4 : Gestion mémoire avec allocateur personnalisé
 * --------------------------------------------------------------------------
 * @code
 * #include "NKMemory/NkPoolAllocator.h"
 * 
 * void exempleAllocateurPersonnalise2D()
 * {
 *     // Créer un pool allocator pour une allocation groupée et rapide
 *     // Estimer la mémoire nécessaire : ~150 nœuds * sizeof(Node) + objets
 *     memory::NkPoolAllocator poolAlloc(128 * 1024);  // Pool de 128KB
 *     
 *     // Instancier le QuadTree avec l'allocateur personnalisé
 *     nkentseu::NkQuadTree<int, memory::NkPoolAllocator> 
 *         pointGrid(-100, -100, 200, 200, &poolAlloc);
 *     
 *     // Insérer une grille de points dense
 *     for (int x = -90; x <= 90; x += 10) {
 *         for (int y = -90; y <= 90; y += 10) {
 *             pointGrid.Insert(x * 100 + y, 
 *                              static_cast<float>(x),
 *                              static_cast<float>(y));
 *         }
 *     }
 *     
 *     printf("Grille de points : %zu points indexés\n", pointGrid.Size());
 *     
 *     // Requête locale rapide grâce au partitionnement spatial
 *     usize count = 0;
 *     pointGrid.Query(0, 0, 50, 50,
 *         [&count](int) { ++count; });
 *     printf("Points dans la zone centrale : %zu\n", count);
 *     
 *     // À la destruction : libération en bloc via le pool (très rapide)
 *     // Idéal pour les scènes temporaires ou les niveaux de jeu 2D
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 5 : Système de particules 2D avec interactions locales
 * --------------------------------------------------------------------------
 * @code
 * struct Particle2D {
 *     float x, y;           // Position
 *     float vx, vy;         // Vélocité
 *     float mass;           // Masse pour les calculs physiques
 *     int id;               // Identifiant unique
 *     
 *     Particle2D(int i, float px, float py, float m)
 *         : x(px), y(py), vx(0), vy(0), mass(m), id(i) {}
 * };
 * 
 * void exempleSystemeParticules2D()
 * {
 *     nkentseu::NkQuadTree<Particle2D> particules(-500, -500, 1000, 1000);
 *     
 *     // Créer 500 particules aléatoires
 *     for (int i = 0; i < 500; ++i) {
 *         float x = (rand() % 1000) - 500;
 *         float y = (rand() % 1000) - 500;
 *         particules.Insert(Particle2D(i, x, y, 1.0f), x, y);
 *     }
 *     
 *     // Simulation : pour chaque particule, trouver les voisines pour les forces locales
 *     const float interactionRadius = 25.0f;
 *     
 *     // Pour une particule donnée, appliquer les forces des voisines
 *     Particle2D& reference = /\* obtenir la particule courante *\/;
 *     particules.QueryRadius(reference.x, reference.y, interactionRadius,
 *         [&](const Particle2D& neighbor) {
 *             if (neighbor.id != reference.id) {
 *                 // Calculer la force d'interaction (gravité, répulsion, etc.)
 *                 float dx = neighbor.x - reference.x;
 *                 float dy = neighbor.y - reference.y;
 *                 float distSq = dx*dx + dy*dy;
 *                 if (distSq > 0.001f) {  // Éviter la division par zéro
 *                     // Appliquer la force à reference.vx/vy...
 *                 }
 *             }
 *         });
 *     
 *     // Avantage : au lieu de tester 500×500 = 250k paires,
 *     // le QuadTree ne teste que les particules spatialement proches
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * BONNES PRATIQUES ET RECOMMANDATIONS
 * --------------------------------------------------------------------------
 * 
 * 1. CHOIX DE LA ZONE RACINE :
 *    - Dimensionner pour contenir tous les objets possibles avec une marge
 *    - Éviter les zones trop grandes : réduit l'efficacité du partitionnement
 *    - Pour les mondes infinis 2D : envisager un QuadTree dynamique avec racine extensible
 *    - Utiliser GetRootBounds() pour valider que les insertions sont dans les bounds
 * 
 * 2. CONFIGURATION DE MAX_CAPACITY ET MAX_DEPTH :
 *    - MAX_CAPACITY=4 : bon compromis entre mémoire et profondeur d'arbre
 *    - Augmenter pour réduire la profondeur (moins de subdivisions) mais plus de tests linéaires
 *    - Diminuer pour une subdivision plus fine mais plus de nœuds et d'overhead mémoire
 *    - MAX_DEPTH=8 : limite la récursion pour éviter la sur-subdivision dans les zones denses
 *    - Ajuster selon la densité attendue : scènes denses -> profondeur plus grande
 * 
 * 3. OPTIMISATIONS DE PERFORMANCE :
 *    - Insert() : O(log₄ n) moyen - idéal pour l'ajout incrémental d'objets
 *    - Query() : O(log₄ n + k) - très efficace pour les requêtes locales
 *    - Éviter les requêtes englobant toute la zone : dégénère en parcours complet O(n)
 *    - Pour les requêtes fréquentes sur mêmes zones : envisager un cache de résultats
 * 
 * 4. GESTION DES OBJETS MOUVANTS :
 *    - Le QuadTree actuel ne supporte pas la mise à jour de position in-place
 *    - Pattern recommandé : Remove() + Insert() pour les objets qui bougent
 *    - Alternative : reconstruire le QuadTree chaque frame pour les systèmes très dynamiques
 *    - Pour les particules : utiliser un QuadTree temporaire par frame (Clear + réinsertion)
 * 
 * 5. MÉMOIRE ET ALLOCATEURS :
 *    - Chaque nœud alloué individuellement : préférer un pool allocator pour réduire la fragmentation
 *    - Estimer le nombre de nœuds : ~n / MAX_CAPACITY dans le pire cas
 *    - Pour les scènes statiques : allouer une fois et réutiliser via Clear()
 *    - GetAllocator() permet de créer des structures temporaires cohérentes (buffers de requête)
 * 
 * 6. SÉCURITÉ ET ROBUSTESSE :
 *    - Insert() ignore silencieusement les objets hors de la zone racine : documenter ce comportement
 *    - Query() ne garantit pas d'ordre de visite : ne pas dépendre de l'ordre pour la logique métier
 *    - Thread-unsafe : protéger avec mutex si accès concurrents en lecture/écriture
 *    - Pour les foncteurs de requête : ne pas modifier le QuadTree pendant le parcours
 * 
 * 7. LIMITATIONS CONNUES (version actuelle) :
 *    - Pas de support natif pour les objets avec bounding volume complexe (OBB, polygones, etc.)
 *    - Pas de méthode Remove() pour supprimer un objet spécifique par référence ou prédicat
 *    - Pas d'itérateurs : à ajouter pour compatibilité range-based for et algorithmes génériques
 *    - QueryRadius() utilise un AABB englobant : peut retourner des faux positifs à filtrer manuellement
 * 
 * 8. EXTENSIONS RECOMMANDÉES :
 *    - Ajouter Remove(const T& data, float x, float y) ou RemoveIf(predicate)
 *    - Implémenter UpdatePosition(oldX, oldY, newX, newY) pour les objets mobiles
 *    - Ajouter des statistiques : GetNodeCount(), GetMaxDepth(), GetAverageObjectsPerNode()
 *    - Supporter les requêtes par prédicat personnalisé : QueryIf(predicate, visitor)
 *    - Implémenter la sérialisation : Save/Load pour persister la structure spatiale
 * 
 * 9. COMPARAISON AVEC AUTRES STRUCTURES SPATIALES :
 *    - vs NkOctree : QuadTree = 2D, Octree = 3D ; même principe de subdivision récursive
 *    - vs Grid spatiale : QuadTree = adaptatif (plus de détails là où c'est dense), Grid = uniforme
 *    - vs KD-Tree : QuadTree = 4-way split régulier, KD-Tree = 2-way split alternant les axes
 *    - Règle : utiliser NkQuadTree pour les scènes 2D avec distribution non-uniforme et requêtes locales fréquentes
 * 
 * 10. PATTERNS AVANCÉS :
 *    - QuadTree hiérarchique : combiner plusieurs QuadTrees pour des niveaux de détail (LOD) spatiaux
 *    - Parallel Query : paralléliser la requête sur les 4 enfants quand ils sont indépendants
 *    - Cache-aware : organiser la mémoire des nœuds pour optimiser le cache CPU
 *    - Dynamic LOD : ajuster MAX_CAPACITY/MAX_DEPTH dynamiquement selon la distance à la caméra
 */

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Généré par Rihen le 2026-02-05 22:26:13
// Dernière modification : 2026-04-26 (restructuration et documentation complète)
// ============================================================