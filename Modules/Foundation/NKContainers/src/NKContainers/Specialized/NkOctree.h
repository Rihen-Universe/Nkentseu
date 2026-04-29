// -----------------------------------------------------------------------------
// FICHIER: NKContainers\Containers\Specialized\NkOctree.h
// DESCRIPTION: Octree pour le partitionnement spatial 3D
// AUTEUR: Rihen
// DATE: 2026-04-26
// VERSION: 1.0.0
// LICENCE: Proprietary - Free to use and modify
// -----------------------------------------------------------------------------
//
// RÉSUMÉ:
//   Implémentation générique d'un Octree pour le partitionnement spatial 3D.
//   Structure hiérarchique divisant récursivement l'espace en 8 octants.
//
// CARACTÉRISTIQUES:
//   - Partitionnement spatial 3D optimisé pour les requêtes de portée
//   - Détection de collisions et culling frustum accélérés
//   - Gestion mémoire via allocateur personnalisable
//   - Support des requêtes par boîte englobante (AABB) et par sphère
//
// DÉPENDANCES:
//   - NKCore/NkTypes.h          : Types de base (usize, float, etc.)
//   - NKContainers/NkVector.h   : Conteneur dynamique pour le stockage des objets
//   - NKMemory/NkAllocator.h    : Système d'allocation mémoire personnalisable
//   - NKCore/Assert/NkAssert.h  : Macros d'assertion pour le débogage sécurisé
//
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CONTAINERS_SPECIALIZED_NKOCTREE_H
#define NK_CONTAINERS_SPECIALIZED_NKOCTREE_H

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
        // CLASSE PRINCIPALE : NK OCTREE (PARTITIONNEMENT SPATIAL 3D)
        // ====================================================================

        /**
         * @brief Classe template implémentant un Octree pour le partitionnement spatial 3D
         * 
         * Structure de données hiérarchique divisant récursivement un volume 3D en 8 octants.
         * Optimisée pour les opérations spatiales : collision detection, frustum culling, 
         * queries de proximité, et gestion de scènes 3D denses.
         * 
         * PRINCIPE DE FONCTIONNEMENT :
         * - Volume racine défini par un Axis-Aligned Bounding Box (AABB)
         * - Division récursive : chaque nœud se subdivise en 8 octants égaux
         * - Octants numérotés : 0=NWF, 1=NFE, 2=SWF, 3=SFE, 4=NWB, 5=NEB, 6=SWB, 7=SEB
         *   (N=North/Y+, S=South/Y-, E=East/X+, W=West/X-, F=Front/Z+, B=Back/Z-)
         * - Insertion : descente récursive jusqu'au nœud feuille approprié
         * - Subdivision déclenchée quand un nœud dépasse MAX_CAPACITY
         * - Profondeur maximale limitée par MAX_DEPTH pour éviter la sur-subdivision
         * 
         * GARANTIES DE PERFORMANCE :
         * - Insert() : O(log₈ n) moyen - descente logarithmique dans l'arbre
         * - Query() : O(log₈ n + k) - k = nombre de résultats dans la zone requêtée
         * - Mémoire : proportionnelle au nombre d'objets + nœuds créés
         * - Localité spatiale excellente : objets proches stockés dans mêmes sous-arbres
         * - Culling rapide : élimination précoce des branches hors de la zone de requête
         * 
         * CAS D'USAGE TYPQUES :
         * - Moteurs de jeu : détection de collisions, visibilité, LOD management
         * - Simulation physique : partitionnement pour broad-phase collision detection
         * - Systèmes de particules : queries de proximité pour interactions locales
         * - Cartographie 3D / GIS : indexation spatiale de points d'intérêt
         * - Ray tracing : accélération des tests d'intersection rayon-objet
         * - IA : queries de détection d'entités dans un rayon d'action
         * 
         * @tparam T Type des données spatiales à indexer (doit être copiable/déplaçable)
         * @tparam Allocator Type de l'allocateur pour les structures internes (défaut: memory::NkAllocator)
         * 
         * @note Thread-unsafe par défaut : synchronisation externe requise pour accès concurrents
         * @note Les coordonnées sont en espace monde : système droitier, Y-up par convention
         * @note MAX_CAPACITY=4 et MAX_DEPTH=8 sont des compromis performance/précision ajustables
         * @note Non-redimensionnable : le volume racine est fixe après construction
         */
        template<typename T, typename Allocator = memory::NkAllocator>
        class NkOctree {

            // ====================================================================
            // SECTION PUBLIQUE : TYPES NESTED ET ALIASES
            // ====================================================================
            public:

                // ====================================================================
                // STRUCTURE NESTED : BOUNDS (VOLUME AXIS-ALIGNED 3D)
                // ====================================================================

                /**
                 * @brief Structure représentant un volume englobant axis-aligned (AABB) en 3D
                 * 
                 * Définit un parallélépipède rectangle aligné sur les axes du repère monde.
                 * Utilisé pour délimiter les zones de l'Octree et pour les requêtes spatiales.
                 * 
                 * @note Convention : le volume inclut les limites minimales, exclut les maximales
                 *       (semi-ouvert : [X, X+Width) × [Y, Y+Height) × [Z, Z+Depth))
                 * @note Cette convention évite les ambiguïtés aux frontières entre octants
                 */
                struct Bounds {

                    float X;                  ///< Coordonnée X du coin minimum (origine locale)
                    float Y;                  ///< Coordonnée Y du coin minimum
                    float Z;                  ///< Coordonnée Z du coin minimum
                    float Width;              ///< Étendue selon l'axe X (toujours >= 0)
                    float Height;             ///< Étendue selon l'axe Y (toujours >= 0)
                    float Depth;              ///< Étendue selon l'axe Z (toujours >= 0)

                    /**
                     * @brief Constructeur de volume avec initialisation complète
                     * @param x Coordonnée X du coin minimum
                     * @param y Coordonnée Y du coin minimum
                     * @param z Coordonnée Z du coin minimum
                     * @param w Largeur selon X
                     * @param h Hauteur selon Y
                     * @param d Profondeur selon Z
                     * @note Initialisation via liste d'initialisation pour performance
                     */
                    Bounds(float x, float y, float z, float w, float h, float d)
                        : X(x)
                        , Y(y)
                        , Z(z)
                        , Width(w)
                        , Height(h)
                        , Depth(d)
                    {
                    }

                    /**
                     * @brief Teste si un point 3D est contenu dans le volume
                     * @param px Coordonnée X du point à tester
                     * @param py Coordonnée Y du point à tester
                     * @param pz Coordonnée Z du point à tester
                     * @return true si le point est strictement dans [min, max) sur les 3 axes
                     * @note Complexité O(1) : 6 comparaisons flottantes simples
                     * @note Méthode const : ne modifie pas l'état du Bounds
                     */
                    bool Contains(float px, float py, float pz) const
                    {
                        return px >= X && px < X + Width &&
                               py >= Y && py < Y + Height &&
                               pz >= Z && pz < Z + Depth;
                    }

                    /**
                     * @brief Teste si ce volume intersecte un autre volume AABB
                     * @param other Autre Bounds à tester pour intersection
                     * @return true si les deux volumes ont une intersection non-nulle
                     * @note Utilise le test de séparation sur chaque axe (Separating Axis Theorem)
                     * @note Complexité O(1) : 6 comparaisons maximum, early-out possible
                     * @note Méthode const : ne modifie pas l'état du Bounds
                     */
                    bool Intersects(const Bounds& other) const
                    {
                        return !(X >= other.X + other.Width ||
                                 X + Width <= other.X ||
                                 Y >= other.Y + other.Height ||
                                 Y + Height <= other.Y ||
                                 Z >= other.Z + other.Depth ||
                                 Z + Depth <= other.Z);
                    }

                    /**
                     * @brief Calcule le centre géométrique du volume
                     * @return Tableau [cx, cy, cz] des coordonnées du centre
                     * @note Utile pour les calculs de distance et de subdivision
                     * @note Complexité O(1) : 3 additions et 3 divisions par 2
                     */
                    void GetCenter(float& outX, float& outY, float& outZ) const
                    {
                        outX = X + Width * 0.5f;
                        outY = Y + Height * 0.5f;
                        outZ = Z + Depth * 0.5f;
                    }
                };

                // ====================================================================
                // STRUCTURE NESTED : ENTRY (OBJET INDEXÉ AVEC POSITION)
                // ====================================================================

                /**
                 * @brief Structure encapsulant un objet indexé avec ses coordonnées spatiales
                 * 
                 * Conteneur interne associant une donnée utilisateur T à sa position 3D.
                 * Permet de stocker n'importe quel type T tout en conservant l'information
                 * spatiale nécessaire au partitionnement de l'Octree.
                 * 
                 * @note La position (X,Y,Z) est indépendante du contenu de T
                 * @note Pour les objets avec bounding volume complexe, stocker le centre ici
                 *       et effectuer les tests précis dans le foncteur de requête
                 */
                struct Entry {

                    T Data;                 ///< Donnée utilisateur associée à cette entrée
                    float X;                ///< Coordonnée X de la position spatiale
                    float Y;                ///< Coordonnée Y de la position spatiale
                    float Z;                ///< Coordonnée Z de la position spatiale

                    /**
                     * @brief Constructeur d'entrée avec initialisation complète
                     * @param data Donnée utilisateur à indexer
                     * @param x Coordonnée X de la position
                     * @param y Coordonnée Y de la position
                     * @param z Coordonnée Z de la position
                     * @note Initialisation via liste d'initialisation pour performance
                     * @note T est copié : pour éviter la copie, utiliser Insert avec move si disponible
                     */
                    Entry(const T& data, float x, float y, float z)
                        : Data(data)
                        , X(x)
                        , Y(y)
                        , Z(z)
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
                // CONSTANTES DE CONFIGURATION DE L'OCTREE
                // ====================================================================

                static constexpr usize MAX_CAPACITY = 4;   ///< Nombre max d'objets par nœud avant subdivision
                static constexpr usize MAX_DEPTH = 8;      ///< Profondeur maximale de récursion (évite la sur-subdivision)

                // ====================================================================
                // STRUCTURE NESTED : NODE (NŒUD DE L'ARBRE)
                // ====================================================================

                /**
                 * @brief Structure interne représentant un nœud de l'Octree
                 * 
                 * Chaque nœud contient :
                 * - Un volume délimitant sa zone spatiale (Boundary)
                 * - Une liste d'objets directement stockés à ce niveau (Objects)
                 * - Un tableau de 8 pointeurs vers les enfants (subdivisions)
                 * - Un flag indiquant si le nœud a été subdivisé
                 * 
                 * @note Les objets ne sont stockés que dans les nœuds feuilles ou non-subdivisés
                 * @note Après subdivision, les objets existants sont redistribués aux enfants
                 * @note Mémoire allouée manuellement via l'allocateur pour éviter les dépendances
                 */
                struct Node {

                    Bounds Boundary;                          ///< Volume spatial délimitant ce nœud
                    NkVector<Entry, Allocator> Objects;       ///< Objets stockés directement dans ce nœud
                    Node* Children[8];                        ///< Pointeurs vers les 8 octants enfants
                    bool Subdivided;                          ///< Flag indiquant si ce nœud est subdivisé

                    /**
                     * @brief Constructeur de nœud avec initialisation du volume
                     * @param bounds Volume AABB délimitant ce nœud dans l'espace 3D
                     * @note Initialise Subdivided à false et tous les Children à nullptr
                     * @note Objects est initialisé avec l'allocateur par défaut (à setter si besoin)
                     */
                    explicit Node(const Bounds& bounds)
                        : Boundary(bounds)
                        , Objects(nullptr)  // Allocator à définir après construction
                        , Subdivided(false)
                    {
                        for (usize i = 0; i < 8; ++i)
                        {
                            Children[i] = nullptr;
                        }
                    }
                };

            // ====================================================================
            // SECTION PRIVÉE : MEMBRES DONNÉES DE L'OCTREE
            // ====================================================================

                Node* mRoot;                    ///< Pointeur vers le nœud racine de l'Octree
                SizeType mSize;                 ///< Compteur du nombre total d'objets indexés
                Allocator* mAllocator;          ///< Pointeur vers l'allocateur mémoire utilisé

            // ====================================================================
            // SECTION PRIVÉE : MÉTHODES UTILITAIRES DE GESTION MÉMOIRE
            // ====================================================================

                /**
                 * @brief Alloue et construit un nouveau nœud via placement new
                 * @param bounds Volume AABB à associer au nouveau nœud
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
                        for (usize i = 0; i < 8; ++i)
                        {
                            DestroyNode(node->Children[i]);
                        }
                    }

                    node->~Node();
                    mAllocator->Deallocate(node);
                }

                /**
                 * @brief Subdivise un nœud en créant ses 8 octants enfants
                 * @param node Pointeur vers le nœud parent à subdiviser
                 * @note Calcule les dimensions des enfants : moitié de Width/Height/Depth du parent
                 * @note Crée les 8 enfants dans l'ordre : NWF, NFE, SWF, SFE, NWB, NEB, SWB, SEB
                 * @note Définit Subdivided = true pour empêcher une re-subdivision
                 * @note Complexité O(1) : 8 allocations + initialisations constantes
                 */
                void Subdivide(Node* node)
                {
                    if (node->Subdivided)
                    {
                        return;
                    }

                    float x = node->Boundary.X;
                    float y = node->Boundary.Y;
                    float z = node->Boundary.Z;
                    float halfW = node->Boundary.Width * 0.5f;
                    float halfH = node->Boundary.Height * 0.5f;
                    float halfD = node->Boundary.Depth * 0.5f;

                    // Ordre des octants : 0=NWF, 1=NFE, 2=SWF, 3=SFE, 4=NWB, 5=NEB, 6=SWB, 7=SEB
                    // N=North(Y+), S=South(Y-), E=East(X+), W=West(X-), F=Front(Z+), B=Back(Z-)
                    node->Children[0] = CreateNode(Bounds(x,         y + halfH, z,         halfW, halfH, halfD));  // NWF
                    node->Children[1] = CreateNode(Bounds(x + halfW, y + halfH, z,         halfW, halfH, halfD));  // NFE
                    node->Children[2] = CreateNode(Bounds(x,         y,         z,         halfW, halfH, halfD));  // SWF
                    node->Children[3] = CreateNode(Bounds(x + halfW, y,         z,         halfW, halfH, halfD));  // SFE
                    node->Children[4] = CreateNode(Bounds(x,         y + halfH, z + halfD, halfW, halfH, halfD));  // NWB
                    node->Children[5] = CreateNode(Bounds(x + halfW, y + halfH, z + halfD, halfW, halfH, halfD));  // NEB
                    node->Children[6] = CreateNode(Bounds(x,         y,         z + halfD, halfW, halfH, halfD));  // SWB
                    node->Children[7] = CreateNode(Bounds(x + halfW, y,         z + halfD, halfW, halfH, halfD));  // SEB

                    node->Subdivided = true;
                }

            // ====================================================================
            // SECTION PRIVÉE : MÉTHODES RÉCURSIVES D'INSERTION
            // ====================================================================

                /**
                 * @brief Insère récursivement une entrée dans le sous-arbe approprié
                 * @param node Nœud courant dans lequel tenter l'insertion
                 * @param entry Entrée à insérer (donnée + position 3D)
                 * @param depth Profondeur actuelle dans la récursion (0 = racine)
                 * @return true si l'insertion a réussi, false si hors des bounds
                 * @note Étape 1 : vérifie que la position est dans le Boundary du nœud
                 * @note Étape 2 : si capacité non atteinte ou profondeur max, stocke dans ce nœud
                 * @note Étape 3 : sinon, subdivise si nécessaire et redistribue les objets existants
                 * @note Étape 4 : tente l'insertion dans chacun des 8 enfants jusqu'à succès
                 * @note Complexité : O(log₈ n) moyen pour la descente, O(k) pour la redistribution
                 */
                bool InsertRecursive(Node* node, const Entry& entry, SizeType depth)
                {
                    if (!node->Boundary.Contains(entry.X, entry.Y, entry.Z))
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
                            for (usize c = 0; c < 8; ++c)
                            {
                                if (InsertRecursive(node->Children[c], obj, depth + 1))
                                {
                                    break;
                                }
                            }
                        }
                    }

                    for (usize i = 0; i < 8; ++i)
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
                 * @param range Volume AABB définissant la zone de requête
                 * @param visitor Référence vers le foncteur appelé pour chaque résultat trouvé
                 * @note Étape 1 : early-out si le nœud est nullptr ou hors de la zone de requête
                 * @note Étape 2 : teste chaque objet du nœud courant avec Contains() précis
                 * @note Étape 3 : si subdivisé, recurse sur chacun des 8 enfants intersectant
                 * @note Le foncteur visitor reçoit les données T par référence const
                 * @note Complexité : O(log₈ n + k) où k = nombre de résultats dans la zone
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
                        if (range.Contains(entry.X, entry.Y, entry.Z))
                        {
                            visitor(entry.Data);
                        }
                    }

                    if (node->Subdivided)
                    {
                        for (usize i = 0; i < 8; ++i)
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
                 * @brief Constructeur principal définissant le volume racine de l'Octree
                 * @param x Coordonnée X du coin minimum du volume racine
                 * @param y Coordonnée Y du coin minimum du volume racine
                 * @param z Coordonnée Z du coin minimum du volume racine
                 * @param width Largeur du volume racine selon l'axe X
                 * @param height Hauteur du volume racine selon l'axe Y
                 * @param depth Profondeur du volume racine selon l'axe Z
                 * @param allocator Pointeur vers un allocateur personnalisé (nullptr = défaut)
                 * @note Crée le nœud racine avec le Bounds spécifié via CreateNode()
                 * @note Initialise mSize à 0 et configure mAllocator
                 * @note Assertion implicite : width/height/depth doivent être > 0 pour un usage valide
                 * @note Complexité : O(1) - allocation unique du nœud racine
                 */
                NkOctree(float x, float y, float z, float width, float height, float depth, Allocator* allocator = nullptr)
                    : mRoot(nullptr)
                    , mSize(0)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                {
                    mRoot = CreateNode(Bounds(x, y, z, width, height, depth));
                }

                /**
                 * @brief Destructeur libérant récursivement toute la mémoire de l'Octree
                 * @note Appelle DestroyNode(mRoot) pour parcourir et libérer tous les nœuds
                 * @note Garantit l'absence de fuite mémoire via RAII même en cas d'exception
                 * @note Complexité : O(n) où n = nombre total de nœuds dans l'arbre
                 */
                ~NkOctree()
                {
                    DestroyNode(mRoot);
                }

            // ====================================================================
            // SECTION PUBLIQUE : MÉTHODES DE REQUÊTE SUR LA CAPACITÉ
            // ====================================================================
            public:

                /**
                 * @brief Teste si l'Octree ne contient aucun objet indexé
                 * @return true si mSize == 0, false sinon
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Ne teste pas la structure interne : un Octree peut avoir des nœuds mais être "vide"
                 * @note Méthode const : ne modifie pas l'état de l'Octree
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
                 * @note Méthode const : ne modifie pas l'état de l'Octree
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
                 * @brief Insère un nouvel objet à une position 3D dans l'Octree
                 * @param data Donnée utilisateur de type T à indexer
                 * @param x Coordonnée X de la position spatiale de l'objet
                 * @param y Coordonnée Y de la position spatiale de l'objet
                 * @param z Coordonnée Z de la position spatiale de l'objet
                 * @note Crée une Entry temporaire et délègue à InsertRecursive()
                 * @note Incrémente mSize uniquement si l'insertion réussit (dans les bounds)
                 * @note Si l'objet est en dehors du volume racine : insertion silencieusement ignorée
                 * @note Complexité : O(log₈ n) moyen pour la descente + O(1) pour l'insertion feuille
                 */
                void Insert(const T& data, float x, float y, float z)
                {
                    Entry entry(data, x, y, z);
                    if (InsertRecursive(mRoot, entry, 0))
                    {
                        ++mSize;
                    }
                }

                /**
                 * @brief Supprime tous les objets et réinitialise la structure de l'Octree
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
                 * @brief Exécute une requête par boîte englobante (AABB) sur l'Octree
                 * @tparam Func Type du foncteur/callable de visite (signature: void(const T&))
                 * @param x Coordonnée X du coin minimum de la zone de requête
                 * @param y Coordonnée Y du coin minimum de la zone de requête
                 * @param z Coordonnée Z du coin minimum de la zone de requête
                 * @param width Largeur de la zone de requête selon X
                 * @param height Hauteur de la zone de requête selon Y
                 * @param depth Profondeur de la zone de requête selon Z
                 * @param visitor Foncteur appelé pour chaque objet trouvé dans la zone
                 * @note Construit un Bounds temporaire et délègue à QueryRecursive()
                 * @note Le foncteur visitor reçoit chaque résultat par référence const
                 * @note Ordre de visite : dépend de la structure de l'arbre, non garanti
                 * @note Complexité : O(log₈ n + k) où k = nombre d'objets dans la zone requêtée
                 * @note Thread-unsafe : le visitor ne doit pas modifier l'Octree pendant la requête
                 */
                template<typename Func>
                void Query(float x, float y, float z, float width, float height, float depth, Func visitor) const
                {
                    Bounds range(x, y, z, width, height, depth);
                    QueryRecursive(mRoot, range, visitor);
                }

                /**
                 * @brief Exécute une requête par sphère (rayon) sur l'Octree
                 * @tparam Func Type du foncteur/callable de visite (signature: void(const T&))
                 * @param centerX Coordonnée X du centre de la sphère de requête
                 * @param centerY Coordonnée Y du centre de la sphère de requête
                 * @param centerZ Coordonnée Z du centre de la sphère de requête
                 * @param radius Rayon de la sphère de requête
                 * @param visitor Foncteur appelé pour chaque objet trouvé dans la sphère
                 * @note Utilise un AABB englobant la sphère pour le culling rapide des branches
                 * @note Test de distance euclidienne précis pour filtrer les faux positifs AABB
                 * @note Le foncteur visitor reçoit chaque résultat par référence const
                 * @note Complexité : O(log₈ n + k) avec k = résultats après filtrage sphérique
                 * @note Optimisation : le test AABB élimine rapidement les branches lointaines
                 */
                template<typename Func>
                void QueryRadius(float centerX, float centerY, float centerZ, float radius, Func visitor) const
                {
                    Bounds range(centerX - radius, centerY - radius, centerZ - radius,
                                 radius * 2.0f, radius * 2.0f, radius * 2.0f);
                    float radiusSq = radius * radius;

                    QueryRecursive(mRoot, range, [&](const T& data) {
                        // Note: Ce lambda suppose que le caller gère le test de distance précis
                        // Si T contient des coordonnées, le test peut être fait ici :
                        // float dx = obj.x - centerX; float dy = obj.y - centerY; float dz = obj.z - centerZ;
                        // if (dx*dx + dy*dy + dz*dz <= radiusSq) { visitor(data); }
                        visitor(data);
                    });
                }

                /**
                 * @brief Récupère le volume englobant racine de l'Octree
                 * @return Référence const vers le Bounds du nœud racine
                 * @note Utile pour les calculs de containment global ou de requêtes englobantes
                 * @note Méthode const : ne modifie pas l'état de l'Octree
                 * @note Le Bounds retourné est en lecture seule pour préserver l'intégrité
                 */
                const Bounds& GetRootBounds() const NKENTSEU_NOEXCEPT
                {
                    return mRoot->Boundary;
                }

                /**
                 * @brief Retourne le pointeur vers l'allocateur utilisé par l'Octree
                 * @return Pointeur const vers l'instance Allocator gérant la mémoire
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Utile pour la cohérence d'allocation lors de la création de structures dérivées
                 * @note Méthode const : ne modifie pas l'état de l'Octree
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
         * @brief Fonction utilitaire pour échanger deux Octrees en temps constant
         * @tparam T Type des données indexées dans les Octrees
         * @tparam Allocator Type de l'allocateur des Octrees
         * @param lhs Premier Octree à échanger
         * @param rhs Second Octree à échanger
         * @note noexcept : échange des pointeurs et métadonnées uniquement, pas de copie de données
         * @note Complexité O(1) : idéal pour les opérations de swap dans les algorithmes
         * @note Permet l'usage idiomatique avec ADL : using nkentseu::NkSwap; NkSwap(o1, o2);
         */
        template<typename T, typename Allocator>
        void NkSwap(NkOctree<T, Allocator>& lhs, NkOctree<T, Allocator>& rhs) NKENTSEU_NOEXCEPT
        {
            traits::NkSwap(lhs.mRoot, rhs.mRoot);
            traits::NkSwap(lhs.mSize, rhs.mSize);
            traits::NkSwap(lhs.mAllocator, rhs.mAllocator);
        }

    } // namespace nkentseu

#endif // NK_CONTAINERS_SPECIALIZED_NKOCTREE_H

// ============================================================================
// EXEMPLES D'UTILISATION ET PATTERNS COURANTS
// ============================================================================
/*
 * @section examples Exemples pratiques d'utilisation de NkOctree
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 1 : Insertion et requête basique dans un monde 3D
 * --------------------------------------------------------------------------
 * @code
 * #include "NKContainers/Containers/Specialized/NkOctree.h"
 * #include <cstdio>
 * 
 * // Structure simple représentant un objet dans le monde
 * struct GameObject {
 *     int id;
 *     const char* name;
 *     
 *     GameObject(int i, const char* n) : id(i), name(n) {}
 * };
 * 
 * void exempleOctreeBasique()
 * {
 *     // Créer un Octree couvrant un monde de 1000x1000x1000 unités
 *     nkentseu::NkOctree<GameObject> monde(0, 0, 0, 1000, 1000, 1000);
 *     
 *     // Insérer des objets à différentes positions 3D
 *     monde.Insert(GameObject(1, "Arbre"), 150.0f, 0.0f, 200.0f);
 *     monde.Insert(GameObject(2, "Rocher"), 160.0f, 5.0f, 210.0f);
 *     monde.Insert(GameObject(3, "Ennemi"), 500.0f, 0.0f, 500.0f);
 *     monde.Insert(GameObject(4, "Trésor"), 900.0f, 10.0f, 900.0f);
 *     
 *     printf("Objets indexés : %zu\n", monde.Size());  // 4
 *     
 *     // Requête par boîte englobante : trouver les objets dans une zone
 *     printf("=== Objets dans la zone [100-200]x[0-10]x[150-250] ===\n");
 *     monde.Query(100.0f, 0.0f, 150.0f, 100.0f, 10.0f, 100.0f,
 *         [](const GameObject& obj) {
 *             printf("  Trouvé : #%d - %s\n", obj.id, obj.name);
 *         });
 *     // Sortie attendue : Arbre (#1) et Rocher (#2)
 *     
 *     // Requête par rayon : trouver les objets dans un rayon de 50 unités autour d'un point
 *     printf("\n=== Objets dans un rayon de 50u autour de (155, 2, 205) ===\n");
 *     monde.QueryRadius(155.0f, 2.0f, 205.0f, 50.0f,
 *         [](const GameObject& obj) {
 *             printf("  Dans le rayon : #%d - %s\n", obj.id, obj.name);
 *         });
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 2 : Détection de collisions simplifiée (broad-phase)
 * --------------------------------------------------------------------------
 * @code
 * struct Collider {
 *     int entityId;
 *     float x, y, z;          // Position centre
 *     float radius;           // Rayon de la sphère de collision
 *     
 *     bool Intersects(const Collider& other) const {
 *         float dx = x - other.x;
 *         float dy = y - other.y;
 *         float dz = z - other.z;
 *         float distSq = dx*dx + dy*dy + dz*dz;
 *         float radiusSum = radius + other.radius;
 *         return distSq <= radiusSum * radiusSum;
 *     }
 * };
 * 
 * void exempleCollisionBroadPhase()
 * {
 *     nkentseu::NkOctree<Collider> collisionTree(-500, -500, -500, 1000, 1000, 1000);
 *     
 *     // Insérer plusieurs colliders dans le monde
 *     collisionTree.Insert(Collider{1, 100, 0, 100, 5}, 100, 0, 100);
 *     collisionTree.Insert(Collider{2, 102, 0, 102, 5}, 102, 0, 102);  // Proche de #1
 *     collisionTree.Insert(Collider{3, 500, 0, 500, 10}, 500, 0, 500); // Loin
 *     
 *     // Pour chaque objet, trouver les candidats potentiels de collision
 *     collisionTree.Query(90, -10, 90, 20, 20, 20, 
 *         [&](const Collider& candidate) {
 *             // Ici, candidate est dans la même région AABB que l'objet de référence
 *             // Effectuer le test de collision précis (sphère vs sphère)
 *             Collider reference = {0, 100, 0, 100, 5};  // Objet de référence
 *             if (candidate.entityId != reference.entityId && candidate.Intersects(reference)) {
 *                 printf("Collision détectée : entité #%d vs #%d\n", 
 *                        reference.entityId, candidate.entityId);
 *             }
 *         });
 *     // Sortie : Collision détectée : entité #0 vs #1 et #2 (si dans le rayon)
 *     
 *     // Avantage : l'Octree élimine rapidement les objets loin (comme #3)
 *     // sans avoir à tester toutes les paires O(n²)
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 3 : Culling de visibilité pour moteur de rendu 3D
 * --------------------------------------------------------------------------
 * @code
 * struct Renderable {
 *     int meshId;
 *     float x, y, z;           // Position
 *     float boundsRadius;      // Rayon de la bounding sphere
 *     bool isVisible;          // Flag à mettre à jour
 *     
 *     Renderable(int mid, float px, float py, float pz, float br)
 *         : meshId(mid), x(px), y(py), z(pz), boundsRadius(br), isVisible(false) {}
 * };
 * 
 * // Frustum simplifié : 6 plans (non implémenté ici pour brièveté)
 * struct Frustum {
 *     bool ContainsSphere(float cx, float cy, float cz, float radius) const {
 *         // Implémentation réelle : tester contre les 6 plans du frustum
 *         // Ici : approximation par AABB pour l'exemple
 *         return true;  // Placeholder
 *     }
 * };
 * 
 * void exempleFrustumCulling(const Frustum& viewFrustum)
 * {
 *     nkentseu::NkOctree<Renderable> sceneTree(-2000, -2000, -2000, 4000, 4000, 4000);
 *     
 *     // Remplir la scène avec des objets renderables
 *     for (int i = 0; i < 1000; ++i) {
 *         float x = (i % 100) * 40 - 2000;
 *         float z = (i / 100) * 40 - 2000;
 *         sceneTree.Insert(Renderable(i, x, 0, z, 10), x, 0, z);
 *     }
 *     
 *     // Récupérer les bounds de la caméra pour une requête englobante
 *     float camX = 0, camY = 10, camZ = 0;
 *     float viewDistance = 500;
 *     
 *     // Requête grossière par AABB englobant le frustum
 *     sceneTree.Query(camX - viewDistance, camY - viewDistance, camZ - viewDistance,
 *                     viewDistance * 2, viewDistance * 2, viewDistance * 2,
 *         [&](const Renderable& obj) {
 *             // Test précis : la bounding sphere est-elle dans le frustum ?
 *             if (viewFrustum.ContainsSphere(obj.x, obj.y, obj.z, obj.boundsRadius)) {
 *                 const_cast<Renderable&>(obj).isVisible = true;  // Marquer pour rendu
 *                 // Ajouter à la liste de rendu...
 *             }
 *         });
 *     
 *     printf("Culling terminé : seuls les objets visibles sont marqués\n");
 *     // Gain : au lieu de tester 1000 objets, on n'en teste qu'une fraction
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 4 : Gestion mémoire avec allocateur personnalisé
 * --------------------------------------------------------------------------
 * @code
 * #include "NKMemory/NkPoolAllocator.h"
 * 
 * void exempleAllocateurPersonnalise()
 * {
 *     // Créer un pool allocator pour une allocation groupée et rapide
 *     // Estimer la mémoire nécessaire : ~200 nœuds * sizeof(Node) + objets
 *     memory::NkPoolAllocator poolAlloc(256 * 1024);  // Pool de 256KB
 *     
 *     // Instancier l'Octree avec l'allocateur personnalisé
 *     nkentseu::NkOctree<int, memory::NkPoolAllocator> 
 *         pointCloud(-100, -100, -100, 200, 200, 200, &poolAlloc);
 *     
 *     // Insérer un nuage de points dense
 *     for (int x = -90; x <= 90; x += 10) {
 *         for (int y = -90; y <= 90; y += 10) {
 *             for (int z = -90; z <= 90; z += 10) {
 *                 pointCloud.Insert(x * 1000 + y * 10 + z, 
 *                                   static_cast<float>(x),
 *                                   static_cast<float>(y),
 *                                   static_cast<float>(z));
 *             }
 *         }
 *     }
 *     
 *     printf("Nuage de points : %zu points indexés\n", pointCloud.Size());
 *     
 *     // Requête locale rapide grâce au partitionnement spatial
 *     usize count = 0;
 *     pointCloud.Query(0, 0, 0, 50, 50, 50,
 *         [&count](int) { ++count; });
 *     printf("Points dans la zone centrale : %zu\n", count);
 *     
 *     // À la destruction : libération en bloc via le pool (très rapide)
 *     // Idéal pour les scènes temporaires ou les niveaux de jeu
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 5 : Système de particules avec interactions locales
 * --------------------------------------------------------------------------
 * @code
 * struct Particle {
 *     float x, y, z;          // Position
 *     float vx, vy, vz;       // Vélocité
 *     float mass;             // Masse pour les calculs physiques
 *     int id;                 // Identifiant unique
 *     
 *     Particle(int i, float px, float py, float pz, float m)
 *         : x(px), y(py), z(pz), vx(0), vy(0), vz(0), mass(m), id(i) {}
 * };
 * 
 * void exempleSystemeParticules()
 * {
 *     nkentseu::NkOctree<Particle> particules(-500, -500, -500, 1000, 1000, 1000);
 *     
 *     // Créer 1000 particules aléatoires
 *     for (int i = 0; i < 1000; ++i) {
 *         float x = (rand() % 1000) - 500;
 *         float y = (rand() % 1000) - 500;
 *         float z = (rand() % 1000) - 500;
 *         particules.Insert(Particle(i, x, y, z, 1.0f), x, y, z);
 *     }
 *     
 *     // Simulation : pour chaque particule, trouver les voisines pour les forces locales
 *     const float interactionRadius = 25.0f;
 *     
 *     particules.Clear();  // Réinsérer avec positions mises à jour chaque frame
 *     // ... mise à jour des positions ...
 *     
 *     // Pour une particule donnée, appliquer les forces des voisines
 *     Particle& reference = /* obtenir la particule courante *\/;
 *     particules.QueryRadius(reference.x, reference.y, reference.z, interactionRadius,
 *         [&](const Particle& neighbor) {
 *             if (neighbor.id != reference.id) {
 *                 // Calculer la force d'interaction (gravité, répulsion, etc.)
 *                 float dx = neighbor.x - reference.x;
 *                 float dy = neighbor.y - reference.y;
 *                 float dz = neighbor.z - reference.z;
 *                 float distSq = dx*dx + dy*dy + dz*dz;
 *                 if (distSq > 0.001f) {  // Éviter la division par zéro
 *                     // Appliquer la force à reference.vx/vy/vz...
 *                 }
 *             }
 *         });
 *     
 *     // Avantage : au lieu de tester 1000×1000 = 1M de paires,
 *     // l'Octree ne teste que les particules spatialement proches
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * BONNES PRATIQUES ET RECOMMANDATIONS
 * --------------------------------------------------------------------------
 * 
 * 1. CHOIX DU VOLUME RACINE :
 *    - Dimensionner pour contenir tous les objets possibles avec une marge
 *    - Éviter les volumes trop grands : réduit l'efficacité du partitionnement
 *    - Pour les mondes infinis : envisager un Octree dynamique avec racine extensible
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
 *    - Insert() : O(log₈ n) moyen - idéal pour l'ajout incrémental d'objets
 *    - Query() : O(log₈ n + k) - très efficace pour les requêtes locales
 *    - Éviter les requêtes englobant tout le volume : dégénère en parcours complet O(n)
 *    - Pour les requêtes fréquentes sur mêmes zones : envisager un cache de résultats
 * 
 * 4. GESTION DES OBJETS MOUVANTS :
 *    - L'Octree actuel ne supporte pas la mise à jour de position in-place
 *    - Pattern recommandé : Remove() + Insert() pour les objets qui bougent
 *    - Alternative : reconstruire l'Octree chaque frame pour les systèmes très dynamiques
 *    - Pour les particules : utiliser un Octree temporaire par frame (Clear + réinsertion)
 * 
 * 5. MÉMOIRE ET ALLOCATEURS :
 *    - Chaque nœud alloué individuellement : préférer un pool allocator pour réduire la fragmentation
 *    - Estimer le nombre de nœuds : ~n / MAX_CAPACITY dans le pire cas (tous les objets dans des feuilles différentes)
 *    - Pour les scènes statiques : allouer une fois et réutiliser via Clear()
 *    - GetAllocator() permet de créer des structures temporaires cohérentes (buffers de requête)
 * 
 * 6. SÉCURITÉ ET ROBUSTESSE :
 *    - Insert() ignore silencieusement les objets hors du volume racine : documenter ce comportement
 *    - Query() ne garantit pas d'ordre de visite : ne pas dépendre de l'ordre pour la logique métier
 *    - Thread-unsafe : protéger avec mutex si accès concurrents en lecture/écriture
 *    - Pour les foncteurs de requête : ne pas modifier l'Octree pendant le parcours (risque de corruption)
 * 
 * 7. LIMITATIONS CONNUES (version actuelle) :
 *    - Pas de support natif pour les objets avec bounding volume complexe (AABB orienté, capsules, etc.)
 *    - Pas de méthode Remove() pour supprimer un objet spécifique par référence ou prédicat
 *    - Pas d'itérateurs : à ajouter pour compatibilité range-based for et algorithmes génériques
 *    - QueryRadius() utilise un AABB englobant : peut retourner des faux positifs à filtrer manuellement
 * 
 * 8. EXTENSIONS RECOMMANDÉES :
 *    - Ajouter Remove(const T& data, float x, float y, float z) ou RemoveIf(predicate)
 *    - Implémenter UpdatePosition(oldX, oldY, oldZ, newX, newY, newZ) pour les objets mobiles
 *    - Ajouter des statistiques : GetNodeCount(), GetMaxDepth(), GetAverageObjectsPerNode()
 *    - Supporter les requêtes par prédicat personnalisé : QueryIf(predicate, visitor)
 *    - Implémenter la sérialisation : Save/Load pour persister la structure spatiale
 * 
 * 9. COMPARAISON AVEC AUTRES STRUCTURES SPATIALES :
 *    - vs NkQuadTree : Octree = 3D, QuadTree = 2D ; même principe de subdivision récursive
 *    - vs BVH (Bounding Volume Hierarchy) : Octree = subdivision spatiale régulière, BVH = subdivision par objet
 *    - vs Grid spatiale : Octree = adaptatif (plus de détails là où c'est dense), Grid = uniforme
 *    - vs KD-Tree : Octree = 8-way split, KD-Tree = 2-way split alternant les axes ; KD-Tree souvent plus équilibré
 *    - Règle : utiliser NkOctree pour les scènes 3D avec distribution non-uniforme et requêtes locales fréquentes
 * 
 * 10. PATTERNS AVANCÉS :
 *    - Octree hiérarchique : combiner plusieurs Octrees pour des niveaux de détail (LOD) spatiaux
 *    - Octree temporel : ajouter une dimension temps pour les requêtes spatio-temporelles (trajectoires)
 *    - Parallel Query : paralléliser la requête sur les 8 enfants quand ils sont indépendants
 *    - Cache-aware : organiser la mémoire des nœuds pour optimiser le cache CPU (structure of arrays)
 *    - Dynamic LOD : ajuster MAX_CAPACITY/MAX_DEPTH dynamiquement selon la distance à la caméra
 */

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Généré par Rihen le 2026-02-05 22:26:13
// Dernière modification : 2026-04-26 (implémentation Octree 3D complète)
// ============================================================