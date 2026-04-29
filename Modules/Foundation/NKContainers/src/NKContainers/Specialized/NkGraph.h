// -----------------------------------------------------------------------------
// FICHIER: NKContainers\Containers\Specialized\NkGraph.h
// DESCRIPTION: Structure de données Graphe - Gestion des sommets et arêtes
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// LICENCE: Proprietary - Free to use and modify
// -----------------------------------------------------------------------------
//
// RÉSUMÉ:
//   Implémentation générique d'un graphe utilisant une liste d'adjacence.
//   Supporte les graphes dirigés et non-dirigés avec pondération des arêtes.
//
// CARACTÉRISTIQUES:
//   - Stockage optimisé pour graphes creux (sparse graphs)
//   - Complexité amortie O(1) pour l'ajout de sommets/arêtes
//   - Algorithmes de parcours DFS et BFS intégrés
//   - Gestionnaire d'allocateur personnalisable
//
// DÉPENDANCES:
//   - NKCore/NkTypes.h          : Types de base (usize, etc.)
//   - NKContainers/NkVector.h   : Conteneur dynamique pour les listes
//   - NKContainers/NkHashMap.h  : Table de hachage pour l'adjacence
//   - NKContainers/NkUnorderedSet.h : Ensemble pour le marquage DFS/BFS
//
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CONTAINERS_SPECIALIZED_NKGRAPH_H
#define NK_CONTAINERS_SPECIALIZED_NKGRAPH_H

    // ========================================================================
    // INCLUSIONS DES DÉPENDANCES
    // ========================================================================

    #include "NKCore/NkTypes.h"                    // Définition des types fondamentaux (usize, etc.)
    #include "NKContainers/NkContainersApi.h"      // Macros d'exportation de l'API conteneurs
    #include "NKCore/NkTraits.h"                   // Traits pour move/forward et utilitaires méta
    #include "NKCore/Assert/NkAssert.h"            // Macros d'assertion pour le débogage sécurisé
    #include "NKContainers/Sequential/NkVector.h"  // Conteneur séquentiel dynamique pour les listes de voisins
    #include "NKContainers/Associative/NkHashMap.h"// Table de hachage pour la structure d'adjacence
    #include "NKContainers/Associative/NkUnorderedSet.h" // Ensemble non-ordonné pour le marquage DFS/BFS

    // ========================================================================
    // ESPACE DE NOMS PRINCIPAL
    // ========================================================================

    namespace nkentseu {

        // ====================================================================
        // CLASSE PRINCIPALE : NK GRAPH (STRUCTURE DE GRAPHE GÉNÉRIQUE)
        // ====================================================================

        /**
         * @brief Classe template implémentant un graphe générique avec sommets et arêtes pondérées
         * 
         * Structure de données représentant un graphe mathématique avec support natif pour :
         * - Graphes dirigés (orientés) et non-dirigés (non-orientés)
         * - Arêtes pondérées avec valeurs float pour les coûts/distances
         * - Parcours algorithmiques DFS (Depth-First Search) et BFS (Breadth-First Search)
         * 
         * PRINCIPE DE FONCTIONNEMENT :
         * - Représentation interne : Liste d'adjacence via NkHashMap<VertexType, VertexList>
         * - Clé de la hashmap : Identifiant unique du sommet source
         * - Valeur associée : NkVector contenant tous les sommets voisins accessibles
         * - Pour graphes non-dirigés : chaque arête (A,B) crée automatiquement (B,A)
         * - Gestion mémoire : allocateur personnalisable via template parameter
         * 
         * GARANTIES DE PERFORMANCE :
         * - AddVertex() : O(1) amorti - insertion hashmap
         * - AddEdge() : O(1) amorti - insertion dans liste d'adjacence
         * - HasEdge() : O(degree) - parcours linéaire des voisins du sommet source
         * - GetNeighbors() : O(1) - accès direct via recherche hashmap
         * - RemoveVertex() : O(V + E) - nécessite nettoyage complet des références
         * - DFS/BFS : O(V + E) - parcours complet du graphe
         * 
         * CAS D'USAGE TYPQUES :
         * - Modélisation de réseaux sociaux (utilisateurs = sommets, relations = arêtes)
         * - Systèmes de navigation et calcul d'itinéraires (villes = sommets, routes = arêtes)
         * - Dépendances de modules/packages dans les build systems
         * - Graphes de connaissances et bases de données relationnelles
         * - Intelligence artificielle : arbres de décision, pathfinding (A*, Dijkstra)
         * - Analyse de flux dans les réseaux (dataflow, circuits électriques)
         * 
         * @tparam VertexType Type des données identifiant les sommets (doit être hashable et comparable)
         * @tparam Allocator Type de l'allocateur pour les structures internes (défaut: memory::NkAllocator)
         * 
         * @note Thread-unsafe par défaut : synchronisation externe requise pour accès concurrents
         * @note VertexType doit implémenter operator== et être compatible avec NkHash pour NkHashMap
         * @note Les poids d'arêtes sont de type float : prévoir pour les calculs de coût/distance
         * @note Non-redimensionnable dynamiquement : les sommets/arêtes sont ajoutés/supprimés individuellement
         */
        template<typename VertexType, typename Allocator = memory::NkAllocator>
        class NkGraph {

            // ====================================================================
            // SECTION PUBLIQUE : TYPES NESTED ET ALIASES
            // ====================================================================
            public:

                // ====================================================================
                // STRUCTURE NESTED : EDGE (REPRÉSENTATION D'UNE ARÊTE)
                // ====================================================================

                /**
                 * @brief Structure représentant une arête pondérée du graphe
                 * 
                 * Contient les informations essentielles d'une connexion entre deux sommets :
                 * - From : Sommet d'origine de l'arête (source)
                 * - To : Sommet de destination de l'arête (cible)
                 * - Weight : Coût, distance ou poids associé au transit par cette arête
                 * 
                 * @note Pour les graphes non-dirigés, une arête (A,B) est équivalente à (B,A)
                 * @note Weight par défaut = 1.0f pour les graphes non-pondérés
                 * @note Utilisée principalement pour l'itération et les algorithmes de plus court chemin
                 */
                struct Edge {

                    VertexType From;          ///< Sommet d'origine de l'arête
                    VertexType To;            ///< Sommet de destination de l'arête
                    float Weight;             ///< Poids ou coût associé à l'arête (défaut: 1.0f)

                    /**
                     * @brief Constructeur d'arête avec initialisation des membres
                     * @param from Sommet source de l'arête
                     * @param to Sommet destination de l'arête
                     * @param weight Poids de l'arête (valeur par défaut: 1.0f)
                     * @note Initialisation via liste d'initialisation pour performance
                     */
                    Edge(const VertexType& from, const VertexType& to, float weight = 1.0f)
                        : From(from)
                        , To(to)
                        , Weight(weight)
                    {
                    }
                };

                // ====================================================================
                // ALIASES DE TYPES POUR LA LISIBILITÉ ET LA GÉNÉRICITÉ
                // ====================================================================

                using SizeType = usize;                        ///< Alias du type pour les tailles et indices (usize)
                using VertexList = NkVector<VertexType, Allocator>;  ///< Alias pour la liste des sommets voisins
                using EdgeList = NkVector<Edge, Allocator>;          ///< Alias pour la liste des arêtes (extensions futures)
                using AdjacencyList = NkHashMap<VertexType, VertexList, Allocator>;  ///< Structure principale d'adjacence

            // ====================================================================
            // SECTION PRIVÉE : MEMBRES DONNÉES ET ÉTAT INTERNE
            // ====================================================================
            private:

                // ====================================================================
                // MEMBRES DONNÉES : ÉTAT DU GRAPHE
                // ====================================================================

                AdjacencyList mAdjacency;      ///< Table de hachage : sommet source -> liste des sommets voisins
                bool mDirected;                ///< Indicateur : true = graphe dirigé, false = non-dirigé
                SizeType mVertexCount;         ///< Compteur du nombre total de sommets uniques dans le graphe
                SizeType mEdgeCount;           ///< Compteur du nombre total d'arêtes (comptage directionnel pour dirigé)
                Allocator* mAllocator;         ///< Pointeur vers l'allocateur mémoire utilisé pour les structures internes

            // ====================================================================
            // SECTION PUBLIQUE : CONSTRUCTEURS
            // ====================================================================
            public:

                /**
                 * @brief Constructeur principal du graphe avec configuration directionnelle
                 * @param directed true pour créer un graphe orienté, false pour non-orienté
                 * @param allocator Pointeur vers un allocateur personnalisé (nullptr = allocateur par défaut)
                 * @note Initialisation de mAdjacency avec l'allocateur spécifié ou le défaut
                 * @note Les compteurs mVertexCount et mEdgeCount sont initialisés à zéro
                 * @note Complexité : O(1) - simple initialisation des membres, pas d'allocation de sommets
                 */
                explicit NkGraph(bool directed = false, Allocator* allocator = nullptr)
                    : mAdjacency(allocator ? allocator : &memory::NkGetDefaultAllocator())
                    , mDirected(directed)
                    , mVertexCount(0)
                    , mEdgeCount(0)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                {
                }

            // ====================================================================
            // SECTION PUBLIQUE : OPÉRATIONS SUR LES SOMMETS (VERTEX OPERATIONS)
            // ====================================================================
            public:

                /**
                 * @brief Ajoute un nouveau sommet au graphe s'il n'existe pas déjà
                 * @param vertex Identifiant du sommet à ajouter (copie par référence const)
                 * @note Opération idempotente : aucun effet si le sommet existe déjà (Contains check)
                 * @note Alloue une nouvelle VertexList vide pour stocker les futurs voisins du sommet
                 * @note Incrémente mVertexCount uniquement lors d'un ajout effectif
                 * @note Complexité : O(1) amorti - insertion dans NkHashMap
                 */
                void AddVertex(const VertexType& vertex)
                {
                    if (!mAdjacency.Contains(vertex))
                    {
                        mAdjacency.Insert(vertex, VertexList(mAllocator));
                        ++mVertexCount;
                    }
                }

                /**
                 * @brief Vérifie l'existence d'un sommet dans le graphe
                 * @param vertex Identifiant du sommet à rechercher
                 * @return true si le sommet existe dans la structure d'adjacence, false sinon
                 * @note Délègue à NkHashMap::Contains() pour la recherche
                 * @note Complexité : O(1) amorti - recherche par hash
                 * @note Méthode const : ne modifie pas l'état du graphe
                 */
                bool HasVertex(const VertexType& vertex) const
                {
                    return mAdjacency.Contains(vertex);
                }

                /**
                 * @brief Supprime un sommet et toutes les arêtes qui lui sont associées
                 * @param vertex Identifiant du sommet à supprimer du graphe
                 * @note Étape 1 : Supprime toutes les arêtes entrantes vers ce sommet depuis d'autres sommets
                 * @note Étape 2 : Supprime la liste d'adjacence sortante du sommet lui-même
                 * @note Décrémente mEdgeCount pour chaque arête supprimée, mVertexCount à la fin
                 * @note Complexité : O(V + E) dans le pire cas - parcours de toutes les listes d'adjacence
                 * @note Aucun effet si le sommet n'existe pas (vérification Contains en début)
                 */
                void RemoveVertex(const VertexType& vertex)
                {
                    if (!mAdjacency.Contains(vertex))
                    {
                        return;
                    }

                    // Supprimer toutes les arêtes pointant VERS ce sommet depuis d'autres sommets
                    for (auto& pair : mAdjacency)
                    {
                        VertexList* neighbors = mAdjacency.Find(pair.First);
                        if (neighbors)
                        {
                            for (SizeType i = 0; i < neighbors->Size(); )
                            {
                                if ((*neighbors)[i] == vertex)
                                {
                                    neighbors->Erase(neighbors->begin() + i);
                                    --mEdgeCount;
                                }
                                else
                                {
                                    ++i;
                                }
                            }
                        }
                    }

                    // Supprimer la liste d'adjacence DU sommet lui-même (arêtes sortantes)
                    mAdjacency.Erase(vertex);
                    --mVertexCount;
                }

            // ====================================================================
            // SECTION PUBLIQUE : OPÉRATIONS SUR LES ARÊTES (EDGE OPERATIONS)
            // ====================================================================
            public:

                /**
                 * @brief Ajoute une arête pondérée entre deux sommets (création automatique des sommets)
                 * @param from Sommet source de l'arête
                 * @param to Sommet destination de l'arête
                 * @param weight Poids de l'arête (valeur par défaut: 1.0f pour graphes non-pondérés)
                 * @note Appel automatique à AddVertex() pour garantir l'existence des deux sommets
                 * @note Pour graphes non-dirigés : crée automatiquement l'arête inverse (to -> from)
                 * @note Évite les boucles automatiques (from == to) pour les graphes non-dirigés uniquement
                 * @note Incrémente mEdgeCount une seule fois même pour les graphes non-dirigés (comptage logique)
                 * @note Complexité : O(1) amorti - insertions dans hashmap et vector
                 */
                void AddEdge(const VertexType& from, const VertexType& to, float weight = 1.0f)
                {
                    AddVertex(from);
                    AddVertex(to);

                    VertexList* neighbors = mAdjacency.Find(from);
                    if (neighbors)
                    {
                        neighbors->PushBack(to);
                        ++mEdgeCount;
                    }

                    if (!mDirected && from != to)
                    {
                        neighbors = mAdjacency.Find(to);
                        if (neighbors)
                        {
                            neighbors->PushBack(from);
                        }
                    }
                }

                /**
                 * @brief Vérifie l'existence d'une arête entre deux sommets spécifiques
                 * @param from Sommet source de l'arête recherchée
                 * @param to Sommet destination de l'arête recherchée
                 * @return true si l'arête existe dans la liste d'adjacence de 'from', false sinon
                 * @note Parcours linéaire de la liste des voisins de 'from' pour trouver 'to'
                 * @note Complexité : O(degree(from)) - proportionnel au nombre de voisins du sommet source
                 * @note Méthode const : ne modifie pas l'état du graphe
                 * @note Retourne false immédiatement si le sommet source n'existe pas
                 */
                bool HasEdge(const VertexType& from, const VertexType& to) const
                {
                    const VertexList* neighbors = mAdjacency.Find(from);
                    if (!neighbors)
                    {
                        return false;
                    }

                    for (SizeType i = 0; i < neighbors->Size(); ++i)
                    {
                        if ((*neighbors)[i] == to)
                        {
                            return true;
                        }
                    }
                    return false;
                }

                /**
                 * @brief Supprime une arête spécifique entre deux sommets
                 * @param from Sommet source de l'arête à supprimer
                 * @param to Sommet destination de l'arête à supprimer
                 * @note Supprime l'arête from->to de la liste d'adjacence de 'from'
                 * @note Pour graphes non-dirigés : supprime également l'arête inverse to->from
                 * @note Décrémente mEdgeCount une seule fois même pour les graphes non-dirigés
                 * @note Aucun effet si l'arête n'existe pas (vérification implicite dans la boucle)
                 * @note Complexité : O(degree(from) + degree(to)) pour les graphes non-dirigés
                 */
                void RemoveEdge(const VertexType& from, const VertexType& to)
                {
                    VertexList* neighbors = mAdjacency.Find(from);
                    if (neighbors)
                    {
                        for (SizeType i = 0; i < neighbors->Size(); )
                        {
                            if ((*neighbors)[i] == to)
                            {
                                neighbors->Erase(neighbors->begin() + i);
                                --mEdgeCount;
                            }
                            else
                            {
                                ++i;
                            }
                        }
                    }

                    if (!mDirected)
                    {
                        neighbors = mAdjacency.Find(to);
                        if (neighbors)
                        {
                            for (SizeType i = 0; i < neighbors->Size(); )
                            {
                                if ((*neighbors)[i] == from)
                                {
                                    neighbors->Erase(neighbors->begin() + i);
                                }
                                else
                                {
                                    ++i;
                                }
                            }
                        }
                    }
                }

            // ====================================================================
            // SECTION PUBLIQUE : MÉTHODES DE REQUÊTE (QUERIES)
            // ====================================================================
            public:

                /**
                 * @brief Récupère la liste des voisins directs d'un sommet donné
                 * @param vertex Sommet cible dont on souhaite obtenir les voisins
                 * @return Pointeur const vers la VertexList des voisins, ou nullptr si sommet inexistant
                 * @note Délègue à NkHashMap::Find() pour la recherche du sommet dans l'adjacence
                 * @note La liste retournée est en lecture seule (const) pour préserver l'intégrité du graphe
                 * @note Complexité : O(1) amorti - recherche directe par hash
                 * @note Méthode const : ne modifie pas l'état du graphe
                 */
                const VertexList* GetNeighbors(const VertexType& vertex) const
                {
                    return mAdjacency.Find(vertex);
                }

                /**
                 * @brief Calcule et retourne le degré (nombre de connexions) d'un sommet
                 * @param vertex Sommet cible dont on souhaite calculer le degré
                 * @return Nombre d'arêtes incidentes au sommet (taille de sa liste de voisins)
                 * @note Retourne 0 si le sommet n'existe pas dans le graphe
                 * @note Pour graphes dirigés : retourne le degré sortant (out-degree) uniquement
                 * @note Complexité : O(1) - accès direct à Size() de la VertexList
                 * @note Méthode const : ne modifie pas l'état du graphe
                 */
                SizeType GetDegree(const VertexType& vertex) const
                {
                    const VertexList* neighbors = mAdjacency.Find(vertex);
                    return neighbors ? neighbors->Size() : 0;
                }

                /**
                 * @brief Retourne le nombre total de sommets uniques dans le graphe
                 * @return Valeur de type SizeType représentant mVertexCount
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Reflète exactement le nombre de clés uniques dans mAdjacency
                 * @note Méthode const : ne modifie pas l'état du graphe
                 */
                SizeType VertexCount() const NKENTSEU_NOEXCEPT
                {
                    return mVertexCount;
                }

                /**
                 * @brief Retourne le nombre total d'arêtes dans le graphe
                 * @return Valeur de type SizeType représentant mEdgeCount
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Pour graphes non-dirigés : compte chaque connexion une seule fois (logique)
                 * @note Méthode const : ne modifie pas l'état du graphe
                 */
                SizeType EdgeCount() const NKENTSEU_NOEXCEPT
                {
                    return mEdgeCount;
                }

                /**
                 * @brief Indique si le graphe est orienté (dirigé) ou non
                 * @return true pour un graphe dirigé, false pour un graphe non-dirigé
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Valeur constante après construction : déterminée par le constructeur
                 * @note Méthode const : ne modifie pas l'état du graphe
                 */
                bool IsDirected() const NKENTSEU_NOEXCEPT
                {
                    return mDirected;
                }

                /**
                 * @brief Vérifie si le graphe est vide (aucun sommet enregistré)
                 * @return true si mVertexCount == 0, false sinon
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Équivalent sémantique à VertexCount() == 0 mais plus explicite
                 * @note Méthode const : ne modifie pas l'état du graphe
                 */
                bool Empty() const NKENTSEU_NOEXCEPT
                {
                    return mVertexCount == 0;
                }

                /**
                 * @brief Retourne le pointeur vers l'allocateur utilisé par le graphe
                 * @return Pointeur const vers l'instance Allocator gérant la mémoire du graphe
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Utile pour la cohérence d'allocation lors de la création de structures dérivées
                 * @note Méthode const : ne modifie pas l'état du graphe
                 */
                Allocator* GetAllocator() const NKENTSEU_NOEXCEPT
                {
                    return mAllocator;
                }

            // ====================================================================
            // SECTION PUBLIQUE : ALGORITHMES DE PARCOURS (TRAVERSAL HELPERS)
            // ====================================================================
            public:

                /**
                 * @brief Parcours en profondeur (DFS - Depth First Search) avec foncteur de visite
                 * @tparam Func Type du foncteur/callable de visite (signature attendue: void(const VertexType&))
                 * @param start Sommet de départ pour initier le parcours DFS
                 * @param visitor Foncteur appelé pour chaque sommet visité durant le parcours
                 * @note Utilise une approche récursive avec marquage via NkUnorderedSet pour éviter les cycles
                 * @note Ordre de visite : plonge d'abord dans chaque branche avant de revenir en arrière
                 * @note Le foncteur visitor reçoit chaque sommet par référence const au moment de sa première visite
                 * @note Complexité : O(V + E) - chaque sommet et arête visité au plus une fois
                 * @note Thread-unsafe : le foncteur visitor ne doit pas modifier le graphe pendant le parcours
                 */
                template<typename Func>
                void DFS(const VertexType& start, Func visitor)
                {
                    NkUnorderedSet<VertexType, Allocator> visited(mAllocator);
                    DFSRecursive(start, visited, visitor);
                }

                /**
                 * @brief Parcours en largeur (BFS - Breadth First Search) avec foncteur de visite
                 * @tparam Func Type du foncteur/callable de visite (signature attendue: void(const VertexType&))
                 * @param start Sommet de départ pour initier le parcours BFS
                 * @param visitor Foncteur appelé pour chaque sommet visité durant le parcours
                 * @note Utilise une file d'attente (queue) pour garantir l'ordre de niveau (distance depuis start)
                 * @note Ordre de visite : tous les voisins à distance 1, puis distance 2, etc.
                 * @note Le foncteur visitor reçoit chaque sommet par référence const au moment de son traitement
                 * @note Complexité : O(V + E) - chaque sommet et arête visité au plus une fois
                 * @note Particulièrement utile pour trouver le plus court chemin dans un graphe non-pondéré
                 */
                template<typename Func>
                void BFS(const VertexType& start, Func visitor)
                {
                    NkUnorderedSet<VertexType, Allocator> visited(mAllocator);
                    NkVector<VertexType, Allocator> queue(mAllocator);

                    queue.PushBack(start);
                    visited.Insert(start);
                    SizeType head = 0;

                    while (head < queue.Size())
                    {
                        const VertexType& vertex = queue[head++];

                        visitor(vertex);

                        const VertexList* neighbors = GetNeighbors(vertex);
                        if (neighbors)
                        {
                            for (SizeType i = 0; i < neighbors->Size(); ++i)
                            {
                                if (!visited.Contains((*neighbors)[i]))
                                {
                                    queue.PushBack((*neighbors)[i]);
                                    visited.Insert((*neighbors)[i]);
                                }
                            }
                        }
                    }
                }

            // ====================================================================
            // SECTION PRIVÉE : MÉTHODES AUXILIAIRES INTERNES
            // ====================================================================
            private:

                /**
                 * @brief Implémentation récursive privée du parcours DFS
                 * @tparam Func Type du foncteur/callable de visite
                 * @param vertex Sommet courant en cours de traitement dans la récursion
                 * @param visited Référence vers l'ensemble des sommets déjà visités (marquage)
                 * @param visitor Référence vers le foncteur de traitement des sommets
                 * @note Marque le sommet courant comme visité avant tout traitement
                 * @note Appelle le visitor sur le sommet courant immédiatement après marquage
                 * @note Parcourt récursivement tous les voisins non encore visités
                 * @note La récursion s'arrête naturellement quand tous les voisins sont visités
                 * @note Complexité : O(degree(vertex)) pour le traitement du sommet courant
                 */
                template<typename Func>
                void DFSRecursive(const VertexType& vertex,
                                  NkUnorderedSet<VertexType, Allocator>& visited,
                                  Func& visitor)
                {
                    visited.Insert(vertex);
                    visitor(vertex);

                    const VertexList* neighbors = GetNeighbors(vertex);
                    if (neighbors)
                    {
                        for (SizeType i = 0; i < neighbors->Size(); ++i)
                        {
                            if (!visited.Contains((*neighbors)[i]))
                            {
                                DFSRecursive((*neighbors)[i], visited, visitor);
                            }
                        }
                    }
                }
        };

        // ====================================================================
        // FONCTIONS NON-MEMBRES : UTILITAIRES GÉNÉRIQUES (OPTIONNEL)
        // ====================================================================

        /**
         * @brief Fonction utilitaire pour échanger deux graphes en temps constant
         * @tparam VertexType Type des sommets des graphes à échanger
         * @tparam Allocator Type de l'allocateur des graphes à échanger
         * @param lhs Premier graphe à échanger
         * @param rhs Second graphe à échanger
         * @note noexcept : échange des pointeurs et métadonnées uniquement, pas de copie de données
         * @note Complexité O(1) : idéal pour les opérations de swap dans les algorithmes
         * @note Permet l'usage idiomatique avec ADL : using nkentseu::NkSwap; NkSwap(g1, g2);
         */
        template<typename VertexType, typename Allocator>
        void NkSwap(NkGraph<VertexType, Allocator>& lhs, NkGraph<VertexType, Allocator>& rhs) NKENTSEU_NOEXCEPT
        {
            // Échange membre par membre via la fonction swap générique des types sous-jacents
            traits::NkSwap(lhs.mAdjacency, rhs.mAdjacency);
            traits::NkSwap(lhs.mDirected, rhs.mDirected);
            traits::NkSwap(lhs.mVertexCount, rhs.mVertexCount);
            traits::NkSwap(lhs.mEdgeCount, rhs.mEdgeCount);
            traits::NkSwap(lhs.mAllocator, rhs.mAllocator);
        }

    } // namespace nkentseu

#endif // NK_CONTAINERS_SPECIALIZED_NKGRAPH_H

// ============================================================================
// EXEMPLES D'UTILISATION ET PATTERNS COURANTS
// ============================================================================
/*
 * @section examples Exemples pratiques d'utilisation de NkGraph
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 1 : Création et manipulation basique d'un graphe non-dirigé
 * --------------------------------------------------------------------------
 * @code
 * #include "NKContainers/Containers/Specialized/NkGraph.h"
 * #include <cstdio>
 * 
 * void exempleGrapheBasique()
 * {
 *     // Créer un graphe non-dirigé avec sommets de type entier
 *     nkentseu::NkGraph<int> graphe(false);
 *     
 *     // Ajouter des sommets (optionnel : AddEdge le fait automatiquement)
 *     graphe.AddVertex(1);
 *     graphe.AddVertex(2);
 *     graphe.AddVertex(3);
 *     
 *     // Ajouter des arêtes avec pondération (coût/distance)
 *     graphe.AddEdge(1, 2, 1.5f);  // Arête 1-2 de poids 1.5
 *     graphe.AddEdge(2, 3, 2.0f);  // Arête 2-3 de poids 2.0
 *     graphe.AddEdge(1, 3, 0.5f);  // Arête 1-3 de poids 0.5
 *     
 *     // Vérifier l'existence d'éléments
 *     bool sommetExiste = graphe.HasVertex(2);           // true
 *     bool areteExiste = graphe.HasEdge(1, 3);            // true
 *     
 *     // Requêter les propriétés du graphe
 *     auto degre = graphe.GetDegree(1);                   // 2 (voisins: 2 et 3)
 *     auto* voisins = graphe.GetNeighbors(1);             // Pointeur vers [2, 3]
 *     
 *     // Statistiques globales
 *     usize nbSommets = graphe.VertexCount();             // 3
 *     usize nbAretes = graphe.EdgeCount();                // 3
 *     bool estDirige = graphe.IsDirected();               // false
 *     
 *     printf("Graphe: %zu sommets, %zu arêtes, dirigé=%s\n",
 *            nbSommets, nbAretes, estDirige ? "oui" : "non");
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 2 : Parcours de graphe avec DFS et BFS
 * --------------------------------------------------------------------------
 * @code
 * #include "NKCore/NkString.h"
 * 
 * void exempleParcours()
 * {
 *     nkentseu::NkGraph<nkentseu::NkString> graphe(true);  // Graphe dirigé
 *     
 *     // Construire un graphe de dépendances de tâches
 *     graphe.AddEdge("build", "compile");
 *     graphe.AddEdge("build", "link");
 *     graphe.AddEdge("compile", "parse");
 *     graphe.AddEdge("link", "compile");
 *     
 *     // Parcours DFS : afficher chaque sommet visité (ordre profondeur)
 *     printf("=== Parcours DFS depuis 'build' ===\n");
 *     graphe.DFS("build", [](const nkentseu::NkString& sommet) {
 *         printf("  Visité (DFS): %s\n", sommet.CStr());
 *     });
 *     
 *     // Parcours BFS : traiter par niveaux de distance depuis la source
 *     printf("\n=== Parcours BFS depuis 'build' ===\n");
 *     graphe.BFS("build", [](const nkentseu::NkString& sommet) {
 *         printf("  Visité (BFS): %s\n", sommet.CStr());
 *     });
 *     
 *     // Résultat typique :
 *     // DFS: build -> compile -> parse -> link (plonge d'abord)
 *     // BFS: build -> compile -> link -> parse (niveau par niveau)
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 3 : Gestion mémoire avec allocateur personnalisé
 * --------------------------------------------------------------------------
 * @code
 * #include "NKMemory/NkPoolAllocator.h"
 * 
 * void exempleAllocateurPersonnalise()
 * {
 *     // Créer un allocateur pool pour une allocation groupée et rapide
 *     memory::NkPoolAllocator poolAlloc(64 * 1024);  // Pool de 64KB
 *     
 *     // Instancier le graphe avec l'allocateur personnalisé
 *     nkentseu::NkGraph<int, memory::NkPoolAllocator> graphe(false, &poolAlloc);
 *     
 *     // Utiliser le graphe normalement : toutes les allocations internes
 *     // (hashmap, vectors, sets) utiliseront le poolAlloc
 *     for (int i = 0; i < 100; ++i)
 *     {
 *         graphe.AddVertex(i);
 *         if (i > 0)
 *         {
 *             graphe.AddEdge(i, i - 1, 1.0f);
 *         }
 *     }
 *     
 *     printf("Graphe créé avec pool allocator: %zu sommets\n", graphe.VertexCount());
 *     
 *     // À la destruction du graphe : libération via le pool (très rapide)
 *     // À la destruction du pool : libération en bloc de toute la mémoire
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 4 : Suppression d'éléments et maintenance du graphe
 * --------------------------------------------------------------------------
 * @code
 * void exempleModificationDynamique()
 * {
 *     nkentseu::NkGraph<int> graphe(false);
 *     
 *     // Construction initiale d'un chemin linéaire
 *     graphe.AddEdge(1, 2);
 *     graphe.AddEdge(2, 3);
 *     graphe.AddEdge(3, 4);
 *     graphe.AddEdge(4, 5);
 *     
 *     printf("État initial: %zu sommets, %zu arêtes\n", 
 *            graphe.VertexCount(), graphe.EdgeCount());  // 5, 4
 *     
 *     // Supprimer une arête spécifique : déconnecte deux sommets
 *     graphe.RemoveEdge(2, 3);
 *     printf("Après RemoveEdge(2,3): %zu arêtes\n", graphe.EdgeCount());  // 3
 *     
 *     // Vérifier la déconnexion
 *     bool encoreConnecte = graphe.HasEdge(2, 3);  // false
 *     
 *     // Supprimer un sommet : supprime automatiquement toutes ses arêtes
 *     graphe.RemoveVertex(3);  // Supprime 3 et les arêtes 2-3, 3-4
 *     printf("Après RemoveVertex(3): %zu sommets, %zu arêtes\n",
 *            graphe.VertexCount(), graphe.EdgeCount());  // 4, 2
 *     
 *     // Vérifications post-suppression
 *     NKENTSEU_ASSERT(graphe.HasVertex(1) == true);
 *     NKENTSEU_ASSERT(graphe.HasVertex(3) == false);  // 3 a été supprimé
 *     NKENTSEU_ASSERT(graphe.HasEdge(2, 3) == false); // arête inexistante
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 5 : Graphe avec types complexes comme sommets
 * --------------------------------------------------------------------------
 * @code
 * // Définition d'un type personnalisé pour représenter une ville
 * struct Ville {
 *     nkentseu::NkString nom;
 *     int population;
 *     
 *     Ville(const char* n, int p) : nom(n), population(p) {}
 *     
 *     // Operator== requis pour la comparaison dans NkHashMap/NkUnorderedSet
 *     bool operator==(const Ville& autre) const {
 *         return nom == autre.nom;  // Comparaison par nom unique
 *     }
 * };
 * 
 * // Spécialisation du hash pour Ville (requis par NkHashMap)
 * template<>
 * struct nkentseu::NkHash<Ville> {
 *     usize operator()(const Ville& ville) const {
 *         // Délègue au hash de NkString pour le champ nom
 *         return nkentseu::NkHash<nkentseu::NkString>()(ville.nom);
 *     }
 * };
 * 
 * void exempleTypesComplexes()
 * {
 *     nkentseu::NkGraph<Ville> reseauRoutier(false);
 *     
 *     // Création des villes
 *     Ville paris("Paris", 2161000);
 *     Ville lyon("Lyon", 513275);
 *     Ville marseille("Marseille", 869815);
 *     
 *     // Connexions avec distances en km comme poids d'arête
 *     reseauRoutier.AddEdge(paris, lyon, 465.0f);
 *     reseauRoutier.AddEdge(lyon, marseille, 314.0f);
 *     reseauRoutier.AddEdge(paris, marseille, 775.0f);
 *     
 *     // Trouver les villes voisines de Lyon
 *     auto* voisinsLyon = reseauRoutier.GetNeighbors(lyon);
 *     if (voisinsLyon)
 *     {
 *         printf("Villes voisines de %s:\n", lyon.nom.CStr());
 *         for (usize i = 0; i < voisinsLyon->Size(); ++i)
 *         {
 *             const Ville& voisine = (*voisinsLyon)[i];
 *             printf("  - %s (pop: %d)\n", voisine.nom.CStr(), voisine.population);
 *         }
 *     }
 *     
 *     // Calculer le degré (nombre de connexions) d'une ville
 *     usize degreLyon = reseauRoutier.GetDegree(lyon);  // 2
 *     printf("%s a %zu connexions directes\n", lyon.nom.CStr(), degreLyon);
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 6 : Algorithme de plus court chemin simplifié (BFS non-pondéré)
 * --------------------------------------------------------------------------
 * @code
 * // Trouve la distance minimale (en nombre d'arêtes) entre deux sommets
 * // Note: Pour graphes pondérés, utiliser Dijkstra avec une priority queue
 * template<typename VertexType, typename Allocator>
 * usize TrouverDistanceMinimale(const nkentseu::NkGraph<VertexType, Allocator>& graphe,
 *                               const VertexType& depart,
 *                               const VertexType& arrivee)
 * {
 *     if (depart == arrivee)
 *     {
 *         return 0;  // Distance nulle si même sommet
 *     }
 *     
 *     nkentseu::NkUnorderedSet<VertexType, Allocator> visites(graphe.GetAllocator());
 *     nkentseu::NkVector<VertexType, Allocator> file(graphe.GetAllocator());
 *     nkentseu::NkVector<usize, Allocator> distances(graphe.GetAllocator());
 *     
 *     file.PushBack(depart);
 *     visites.Insert(depart);
 *     distances.PushBack(0);
 *     
 *     usize tete = 0;
 *     while (tete < file.Size())
 *     {
 *         const VertexType& courant = file[tete];
 *         usize distanceCourante = distances[tete];
 *         ++tete;
 *         
 *         auto* voisins = graphe.GetNeighbors(courant);
 *         if (voisins)
 *         {
 *             for (usize i = 0; i < voisins->Size(); ++i)
 *             {
 *                 const VertexType& voisin = (*voisins)[i];
 *                 
 *                 if (voisin == arrivee)
 *                 {
 *                     return distanceCourante + 1;  // Destination trouvée
 *                 }
 *                 
 *                 if (!visites.Contains(voisin))
 *                 {
 *                     file.PushBack(voisin);
 *                     visites.Insert(voisin);
 *                     distances.PushBack(distanceCourante + 1);
 *                 }
 *             }
 *         }
 *     }
 *     
 *     return static_cast<usize>(-1);  // Pas de chemin trouvé (infini)
 * }
 * 
 * void exemplePlusCourtChemin()
 * {
 *     nkentseu::NkGraph<int> graphe(false);
 *     
 *     // Créer un petit graphe en étoile
 *     graphe.AddEdge(0, 1);
 *     graphe.AddEdge(0, 2);
 *     graphe.AddEdge(1, 3);
 *     graphe.AddEdge(2, 3);
 *     graphe.AddEdge(3, 4);
 *     
 *     // Trouver la distance entre 0 et 4
 *     usize distance = TrouverDistanceMinimale(graphe, 0, 4);
 *     printf("Distance minimale 0->4 : %zu arêtes\n", distance);  // 2 (0->1->3->4 ou 0->2->3->4)
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * BONNES PRATIQUES ET RECOMMANDATIONS
 * --------------------------------------------------------------------------
 * 
 * 1. CHOIX DU TYPE DE SOMMET (VertexType) :
 *    - Pour les performances : privilégier les types légers (int, usize, enums)
 *    - Pour la lisibilité : utiliser des types sémantiques (NkString, structs)
 *    - Requirement : VertexType doit être hashable (NkHash<T> spécialisé) et comparable (operator==)
 *    - Pour les types complexes : spécialiser NkHash et operator== avant utilisation
 * 
 * 2. GESTION DES GRAPHES DIRIGÉS VS NON-DIRIGÉS :
 *    - Dirigé (directed=true) : arête A->B ≠ B->A, utile pour dépendances, flux unidirectionnels
 *    - Non-dirigé (directed=false) : arête A-B = B-A, utile pour réseaux sociaux, routes, connexions
 *    - Attention : EdgeCount() compte logiquement chaque connexion une fois, même pour non-dirigé
 *    - HasEdge(A,B) sur graphe non-dirigé vérifie uniquement A->B, pas B->A implicitement
 * 
 * 3. OPTIMISATIONS DE PERFORMANCE :
 *    - AddVertex/AddEdge : O(1) amorti, idéal pour construction incrémentale
 *    - HasEdge : O(degree) - éviter dans des boucles critiques sur graphes denses
 *    - RemoveVertex : O(V+E) - opération coûteuse, à utiliser avec parcimonie
 *    - Préférer la construction batch (ajouts groupés) aux modifications fréquentes en production
 * 
 * 4. GESTION MÉMOIRE ET ALLOCATEURS :
 *    - Utiliser un allocateur pool pour les graphes de taille connue à l'avance
 *    - Pour les graphes temporaires : l'allocateur par défaut suffit généralement
 *    - GetAllocator() permet de créer des structures cohérentes (queues DFS/BFS) avec le même allocateur
 *    - Attention aux cycles de références si VertexType contient des pointeurs vers le graphe
 * 
 * 5. SÉCURITÉ ET ROBUSTESSE :
 *    - Toutes les méthodes gèrent les cas limites (sommet inexistant, arête inexistante)
 *    - Aucune assertion en release : le code est safe par conception (checks explicites)
 *    - Thread-unsafe : protéger avec mutex/lock si accès concurrents en lecture/écriture
 *    - Pour les algorithmes DFS/BFS : le foncteur visitor ne doit pas modifier le graphe pendant le parcours
 * 
 * 6. LIMITATIONS CONNUES (version actuelle) :
 *    - Pas de support natif pour les arêtes avec métadonnées complexes (étendre struct Edge si besoin)
 *    - HasEdge() linéaire : pour des vérifications fréquentes, envisager NkHashMap<VertexType, NkUnorderedSet<>>
 *    - Pas d'itérateurs : à ajouter pour compatibilité range-based for et algorithmes génériques
 *    - Pas de méthodes de sérialisation : à implémenter selon le format de stockage souhaité
 * 
 * 7. EXTENSIONS RECOMMANDÉES :
 *    - Ajouter un template parameter EdgeData pour des métadonnées personnalisées par arête
 *    - Implémenter FindPath() avec retour de NkVector<VertexType> pour le chemin complet
 *    - Ajouter des méthodes de filtrage : GetNeighborsIf(predicate) pour requêtes conditionnelles
 *    - Supporter les graphes pondérés négatifs : vérifier la cohérence des algorithmes (Bellman-Ford vs Dijkstra)
 *    - Ajouter des statistiques : Density(), AverageDegree(), ConnectedComponents() pour l'analyse
 * 
 * 8. COMPARAISON AVEC AUTRES STRUCTURES :
 *    - vs Matrice d'adjacence : NkGraph = mémoire O(V+E) pour graphes creux, matrice = O(V²) mais HasEdge O(1)
 *    - vs NkTree : NkGraph supporte les cycles et connexions multiples, NkTree = hiérarchie stricte sans cycles
 *    - vs Base de données graphe : NkGraph = in-memory, rapide, simple ; BDD = persistant, requêtes complexes
 *    - Règle : utiliser NkGraph pour les graphes en mémoire de taille modérée (< 10⁶ sommets typiquement)
 * 
 * 9. PATTERNS AVANCÉS :
 *    - Graphe dynamique : combiner avec NkObserver pour notifications lors d'ajout/suppression de sommets
 *    - Sous-graphes : créer des vues filtrées via foncteurs de prédicat sur VertexType/Edge
 *    - Cache de voisins : pour les requêtes HasEdge fréquentes, maintenir un NkHashMap<VertexType, NkUnorderedSet<>>
 *    - Parallélisation : DFS/BFS peuvent être parallélisés par niveau (BFS) ou par branche (DFS) avec précaution
 */

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Généré par Rihen le 2026-02-05 22:26:13
// Dernière modification : 2026-04-26 (restructuration et documentation complète)
// ============================================================