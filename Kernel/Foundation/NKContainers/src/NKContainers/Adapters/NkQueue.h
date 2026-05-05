// -----------------------------------------------------------------------------
// FICHIER: NKContainers\Containers\Adapters\NkQueue.h
// DESCRIPTION: Adaptateur de file - FIFO (First In First Out)
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CONTAINERS_ADAPTERS_NKQUEUE_H
#define NK_CONTAINERS_ADAPTERS_NKQUEUE_H

    // ========================================================================
    // INCLUSIONS DES DÉPENDANCES
    // ========================================================================

    #include "NKCore/NkTypes.h"                    // Définition des types fondamentaux (usize, etc.)
    #include "NKContainers/NkContainersApi.h"      // Macros d'exportation de l'API conteneurs
    #include "NKCore/NkTraits.h"                   // Traits pour move/forward et utilitaires méta
    #include "NKCore/Assert/NkAssert.h"            // Macros d'assertion pour le débogage sécurisé
    #include "NKContainers/Sequential/NkDeque.h"   // Conteneur séquentiel par défaut (double-ended queue)
    #include "NKContainers/Iterators/NkInitializerList.h" // Support des listes d'initialisation

    // ========================================================================
    // ESPACE DE NOMS PRINCIPAL
    // ========================================================================

    namespace nkentseu {

        // ====================================================================
        // CLASSE PRINCIPALE : NK QUEUE (ADAPTATEUR FIFO)
        // ====================================================================

        /**
         * @brief Classe template implémentant un adaptateur de file FIFO (First In First Out)
         * 
         * Adaptateur de conteneur fournissant une interface de file avec sémantique
         * First In First Out : le premier élément ajouté est le premier à être retiré.
         * 
         * PRINCIPE DE FONCTIONNEMENT :
         * - Wrapper autour d'un conteneur séquentiel (NkDeque par défaut)
         * - Insertions à l'arrière (PushBack), suppressions à l'avant (PopFront)
         * - Accès aux deux extrémités : Front() pour le premier, Back() pour le dernier
         * - Délègue le stockage et la gestion mémoire au conteneur sous-jacent
         * - Aucune itération : accès uniquement aux extrémités de la file
         * 
         * GARANTIES DE PERFORMANCE (avec NkDeque comme conteneur) :
         * - Push() : O(1) constant (ajout à l'arrière du deque)
         * - Pop() : O(1) constant (suppression à l'avant du deque)
         * - Front()/Back() : O(1) constant, accès direct aux extrémités
         * - Size()/Empty() : O(1) constant, métadonnées du conteneur
         * 
         * CAS D'USAGE TYPQUES :
         * - Ordonnancement de tâches et gestion de processus (scheduling)
         * - Buffers circulaires et traitement de flux de données
         * - Algorithmes de parcours en largeur (BFS) sur graphes et arbres
         * - Files d'attente de messages et systèmes de communication inter-processus
         * - Simulation d'événements discrets avec gestion temporelle
         * 
         * @tparam T Type des éléments stockés dans la file (doit être copiable/déplaçable)
         * @tparam Container Type du conteneur sous-jacent (défaut: NkDeque<T>)
         *                   Doit supporter : PushBack(), PopFront(), Front(), Back(), Empty(), Size()
         * 
         * @note Thread-unsafe : synchronisation externe requise pour usage concurrent
         * @note Les opérations modifient les deux extrémités : pas d'accès aléatoire aux éléments intermédiaires
         * @note Le conteneur sous-jacent est accessible via méthode dédiée si besoin (à ajouter)
         */
        template<typename T, typename Container = NkDeque<T>>
        class NkQueue {

            // ====================================================================
            // SECTION PUBLIQUE : ALIASES DE TYPES
            // ====================================================================
            public:

                // ====================================================================
                // ALIASES DE TYPES POUR LA LISIBILITÉ ET LA GÉNÉRICITÉ
                // ====================================================================

                using ValueType = T;                  ///< Alias du type des éléments stockés
                using SizeType = usize;               ///< Alias du type pour les tailles/comptages
                using Reference = T&;                 ///< Alias de référence non-const vers un élément
                using ConstReference = const T&;      ///< Alias de référence const vers un élément
                using ContainerType = Container;      ///< Alias du type du conteneur sous-jacent

            // ====================================================================
            // SECTION PRIVÉE : MEMBRES DONNÉES
            // ====================================================================
            private:

                // ====================================================================
                // MEMBRES DONNÉES : CONTENEUR SOUS-JACENT
                // ====================================================================

                Container mContainer;                 ///< Conteneur séquentiel stockant les éléments de la file

            // ====================================================================
            // SECTION PUBLIQUE : CONSTRUCTEURS ET ASSIGNATION
            // ====================================================================
            public:

                /**
                 * @brief Constructeur par défaut avec conteneur vide
                 * @note Initialise une NkQueue vide prête à recevoir des éléments
                 * @note Complexité O(1) : construction du conteneur sous-jacent uniquement
                 */
                NkQueue() {
                }

                /**
                 * @brief Constructeur explicite depuis un conteneur existant
                 * @param cont Référence const vers le conteneur source à copier
                 * @note Copie le contenu du conteneur dans la queue interne
                 * @note L'ordre FIFO est préservé : premier élément du conteneur = Front()
                 */
                explicit NkQueue(const Container& cont)
                    : mContainer(cont) {
                }

                // ====================================================================
                // SUPPORT C++11 : CONSTRUCTEUR PAR DÉPLACEMENT
                // ====================================================================
                #if defined(NK_CPP11)

                    /**
                     * @brief Constructeur explicite par déplacement depuis un conteneur
                     * @param cont Rvalue reference vers le conteneur source à déplacer
                     * @note noexcept implicite si Container a un move constructor noexcept
                     * @note Transfère efficacement les ressources sans copie des éléments
                     */
                    explicit NkQueue(Container&& cont)
                        : mContainer(traits::NkMove(cont)) {
                    }

                #endif // defined(NK_CPP11)

                /**
                 * @brief Constructeur depuis NkInitializerList
                 * @param init Liste d'éléments pour initialiser la file
                 * @note Insère les éléments dans l'ordre : le premier de la liste est à l'avant (Front)
                 * @note Le dernier élément de la liste se retrouve à l'arrière (Back)
                 */
                NkQueue(NkInitializerList<T> init) {
                    for (auto& val : init) {
                        Push(val);
                    }
                }

                /**
                 * @brief Constructeur depuis std::initializer_list (C++11)
                 * @param init Liste standard d'éléments pour initialiser la file
                 * @note Compatibilité avec la syntaxe C++11 : NkQueue<int> q = {1, 2, 3};
                 * @note Ordre FIFO : Front() = 1, Back() = 3
                 */
                NkQueue(std::initializer_list<T> init) {
                    for (auto& val : init) {
                        Push(val);
                    }
                }

                /**
                 * @brief Opérateur d'affectation depuis NkInitializerList
                 * @param init Nouvelle liste d'éléments pour remplacer le contenu actuel
                 * @return Référence vers l'instance courante pour le chaînage d'opérations
                 * @note Vide la file actuelle avant d'insérer les nouveaux éléments
                 * @note Complexité O(n + m) où n = taille actuelle, m = taille de la liste
                 */
                NkQueue& operator=(NkInitializerList<T> init) {
                    while (!Empty()) {
                        Pop();
                    }
                    for (auto& val : init) {
                        Push(val);
                    }
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation depuis std::initializer_list (C++11)
                 * @param init Nouvelle liste standard d'éléments pour remplacer le contenu
                 * @return Référence vers l'instance courante pour le chaînage d'opérations
                 * @note Syntaxe C++11 : queue = {4, 5, 6};
                 */
                NkQueue& operator=(std::initializer_list<T> init) {
                    while (!Empty()) {
                        Pop();
                    }
                    for (auto& val : init) {
                        Push(val);
                    }
                    return *this;
                }

            // ====================================================================
            // SECTION PUBLIQUE : ACCÈS AUX ÉLÉMENTS DES EXTRÉMITÉS
            // ====================================================================

                /**
                 * @brief Accède à l'élément à l'avant de la file (version mutable)
                 * @return Référence non-const vers l'élément le plus ancien (premier entré)
                 * @pre La file ne doit pas être vide (assertion NKENTSEU_ASSERT en debug)
                 * @note Complexité O(1) : délégation directe à Container::Front()
                 * @note Permet la modification : queue.Front() = newValue;
                 */
                Reference Front() {
                    NKENTSEU_ASSERT(!Empty());
                    return mContainer.Front();
                }

                /**
                 * @brief Accède à l'élément à l'avant de la file (version const)
                 * @return Référence const vers l'élément le plus ancien (premier entré)
                 * @pre La file ne doit pas être vide (assertion NKENTSEU_ASSERT en debug)
                 * @note Version const pour usage sur NkQueue const ou accès en lecture seule
                 * @note Complexité O(1) : délégation directe à Container::Front() const
                 */
                ConstReference Front() const {
                    NKENTSEU_ASSERT(!Empty());
                    return mContainer.Front();
                }

                /**
                 * @brief Accède à l'élément à l'arrière de la file (version mutable)
                 * @return Référence non-const vers l'élément le plus récent (dernier entré)
                 * @pre La file ne doit pas être vide (assertion NKENTSEU_ASSERT en debug)
                 * @note Complexité O(1) : délégation directe à Container::Back()
                 * @note Utile pour inspecter le dernier élément ajouté sans le supprimer
                 */
                Reference Back() {
                    NKENTSEU_ASSERT(!Empty());
                    return mContainer.Back();
                }

                /**
                 * @brief Accède à l'élément à l'arrière de la file (version const)
                 * @return Référence const vers l'élément le plus récent (dernier entré)
                 * @pre La file ne doit pas être vide (assertion NKENTSEU_ASSERT en debug)
                 * @note Version const pour usage sur NkQueue const ou accès en lecture seule
                 * @note Complexité O(1) : délégation directe à Container::Back() const
                 */
                ConstReference Back() const {
                    NKENTSEU_ASSERT(!Empty());
                    return mContainer.Back();
                }

            // ====================================================================
            // SECTION PUBLIQUE : CAPACITÉ ET MÉTADONNÉES
            // ====================================================================

                /**
                 * @brief Teste si la file ne contient aucun élément
                 * @return true si le conteneur sous-jacent est vide, false sinon
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Délègue à Container::Empty() : complexité dépend du conteneur
                 */
                bool Empty() const NKENTSEU_NOEXCEPT {
                    return mContainer.Empty();
                }

                /**
                 * @brief Retourne le nombre d'éléments actuellement dans la file
                 * @return Valeur de type SizeType représentant la longueur de la file
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Délègue à Container::Size() : complexité dépend du conteneur
                 */
                SizeType Size() const NKENTSEU_NOEXCEPT {
                    return mContainer.Size();
                }

            // ====================================================================
            // SECTION PUBLIQUE : MODIFICATEURS - PUSH ET POP
            // ====================================================================

                /**
                 * @brief Ajoute un nouvel élément à l'arrière de la file
                 * @param value Référence const vers l'élément à enfiler
                 * @note Complexité O(1) avec NkDeque (ajout efficace aux deux extrémités)
                 * @note Délègue à Container::PushBack() : l'élément devient le nouveau Back()
                 */
                void Push(const T& value) {
                    mContainer.PushBack(value);
                }

                // ====================================================================
                // SUPPORT C++11 : MOVE SEMANTICS ET EMPLACE
                // ====================================================================
                #if defined(NK_CPP11)

                    /**
                     * @brief Ajoute un élément par déplacement à l'arrière de la file
                     * @param value Rvalue reference vers l'élément à déplacer dans la file
                     * @note Optimise les transferts pour les types lourds (NkString, NkVector, etc.)
                     * @note Nécessite que T soit MoveConstructible
                     */
                    void Push(T&& value) {
                        mContainer.PushBack(traits::NkMove(value));
                    }

                    /**
                     * @brief Construit et ajoute un élément directement à l'arrière de la file
                     * @tparam Args Types des arguments de construction de T
                     * @param args Arguments forwarding vers le constructeur de T
                     * @note Évite toute copie/move temporaire : construction in-place dans le conteneur
                     * @note Particulièrement efficace pour les types avec constructeurs complexes
                     */
                    template<typename... Args>
                    void Emplace(Args&&... args) {
                        mContainer.EmplaceBack(traits::NkForward<Args>(args)...);
                    }

                #endif // defined(NK_CPP11)

                /**
                 * @brief Supprime l'élément à l'avant de la file (le plus ancien)
                 * @pre La file ne doit pas être vide (assertion NKENTSEU_ASSERT en debug)
                 * @note Complexité O(1) : délégation directe à Container::PopFront()
                 * @note L'élément supprimé n'est pas retourné : utiliser Front() avant Pop() si besoin
                 * @note Ne réduit pas nécessairement la capacité du conteneur sous-jacent
                 */
                void Pop() {
                    NKENTSEU_ASSERT(!Empty());
                    mContainer.PopFront();
                }

            // ====================================================================
            // SECTION PUBLIQUE : ÉCHANGE ET UTILITAIRES
            // ====================================================================

                /**
                 * @brief Échange le contenu de cette file avec une autre en temps constant
                 * @param other Autre instance de NkQueue à échanger
                 * @note noexcept : délègue à Container::Swap() pour échange de pointeurs
                 * @note Complexité O(1) : aucune allocation, copie ou destruction d'éléments
                 * @note Préférer à l'assignation pour les permutations efficaces de grandes files
                 */
                void Swap(NkQueue& other) NKENTSEU_NOEXCEPT {
                    mContainer.Swap(other.mContainer);
                }
        };

        // ====================================================================
        // FONCTIONS NON-MEMBRES : SWAP GLOBAL
        // ====================================================================

        /**
         * @brief Fonction swap non-membre pour ADL (Argument-Dependent Lookup)
         * @tparam T Type des éléments de la file
         * @tparam Container Type du conteneur sous-jacent
         * @param lhs Première file à échanger
         * @param rhs Seconde file à échanger
         * @note noexcept : délègue à la méthode membre Swap()
         * @note Permet l'usage idiomatique : using nkentseu::NkSwap; NkSwap(a, b);
         */
        template<typename T, typename Container>
        void NkSwap(NkQueue<T, Container>& lhs, NkQueue<T, Container>& rhs) NKENTSEU_NOEXCEPT {
            lhs.Swap(rhs);
        }

    } // namespace nkentseu

#endif // NK_CONTAINERS_ADAPTERS_NKQUEUE_H

// ============================================================================
// EXEMPLES D'UTILISATION ET PATTERNS COURANTS
// ============================================================================
/*
 * @section examples Exemples pratiques d'utilisation de NkQueue
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 1 : Opérations de base FIFO
 * --------------------------------------------------------------------------
 * @code
 * #include "NKContainers/Adapters/NkQueue.h"
 * #include <cstdio>
 * 
 * void exempleBase() {
 *     // Création d'une file d'entiers avec conteneur par défaut (NkDeque)
 *     nkentseu::NkQueue<int> queue;
 *     
 *     // Enfilement (Push) : FIFO - premier entré, premier sorti
 *     queue.Push(1);  // File: [1] <- avant/arrière
 *     queue.Push(2);  // File: [1, 2] <- arrière
 *     queue.Push(3);  // File: [1, 2, 3] <- arrière
 *     
 *     // Accès aux extrémités sans suppression
 *     int front = queue.Front();  // 1 (premier entré)
 *     int back = queue.Back();    // 3 (dernier entré)
 *     printf("Avant: %d, Arrière: %d\n", front, back);
 *     
 *     // Défilement (Pop) : retire l'élément le plus ancien
 *     queue.Pop();  // File: [2, 3] <- arrière
 *     printf("Nouvel avant: %d\n", queue.Front());  // 2
 *     
 *     // Vérification d'état
 *     printf("Taille: %zu, Vide: %s\n", 
 *            queue.Size(), queue.Empty() ? "oui" : "non");  // Taille: 2, Vide: non
 *     
 *     // Vidage complet de la file (ordre FIFO respecté)
 *     while (!queue.Empty()) {
 *         printf("%d ", queue.Front());
 *         queue.Pop();
 *     }
 *     // Sortie: 2 3 (même ordre que l'insertion)
 *     printf("\n");
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 2 : Initialisation et assignation
 * --------------------------------------------------------------------------
 * @code
 * void exempleInitialisation() {
 *     // Construction depuis initializer_list (C++11)
 *     // Ordre FIFO : premier élément = Front(), dernier = Back()
 *     nkentseu::NkQueue<int> queue1 = {10, 20, 30};
 *     // File: [10, 20, 30] -> Front()=10, Back()=30
 *     
 *     // Construction depuis conteneur existant
 *     nkentseu::NkDeque<int> deque = {1, 2, 3, 4};
 *     nkentseu::NkQueue<int> queue2(deque);
 *     // File: [1, 2, 3, 4] -> Front()=1, Back()=4
 *     
 *     // Déplacement de conteneur (C++11)
 *     #if defined(NK_CPP11)
 *     nkentseu::NkDeque<int> deque2 = {5, 6, 7};
 *     nkentseu::NkQueue<int> queue3(nkentseu::traits::NkMove(deque2));
 *     // deque2 est maintenant vide, queue3 possède [5, 6, 7]
 *     #endif
 *     
 *     // Assignation depuis liste
 *     queue1 = {100, 200};  // Remplace le contenu: [100, 200] -> Front()=100
 *     
 *     // Échange de files
 *     nkentseu::NkQueue<int> queue4 = {999};
 *     nkentseu::NkSwap(queue1, queue4);
 *     // queue1: [999], queue4: [100, 200]
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 3 : Ordonnancement de tâches (task scheduling)
 * --------------------------------------------------------------------------
 * @code
 * struct Tache {
 *     int id;
 *     const char* description;
 *     int priorite;  // Ignoré dans ce modèle FIFO simple
 * };
 * 
 * void exempleScheduling() {
 *     nkentseu::NkQueue<Tache> fileAttente;
 *     
 *     // Arrivée de tâches dans le système
 *     fileAttente.Push({1, "Sauvegarde base de données", 1});
 *     fileAttente.Push({2, "Traitement commande client", 2});
 *     fileAttente.Push({3, "Génération rapport mensuel", 1});
 *     
 *     // Traitement FIFO : premier arrivé, premier servi
 *     printf("Traitement des tâches:\n");
 *     while (!fileAttente.Empty()) {
 *         const Tache& t = fileAttente.Front();
 *         printf("  [%d] %s (prio: %d)\n", t.id, t.description, t.priorite);
 *         fileAttente.Pop();  // Tâche traitée, retirée de la file
 *     }
 *     // Sortie (ordre d'arrivée respecté):
 *     //   [1] Sauvegarde base de données (prio: 1)
 *     //   [2] Traitement commande client (prio: 2)
 *     //   [3] Génération rapport mensuel (prio: 1)
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 4 : Parcours en largeur (BFS) sur un graphe
 * --------------------------------------------------------------------------
 * @code
 * // Graphes simplifiés pour démonstration BFS
 * struct Noeud {
 *     int id;
 *     nkentseu::NkVector<int> voisins;  // IDs des noeuds adjacents
 *     bool visite;
 * };
 * 
 * void BFS(nkentseu::NkVector<Noeud>& graphe, int depart) {
 *     nkentseu::NkQueue<int> file;
 *     
 *     // Initialisation
 *     graphe[depart].visite = true;
 *     file.Push(depart);
 *     
 *     printf("Parcours BFS depuis noeud %d:\n", depart);
 *     
 *     // Boucle principale BFS
 *     while (!file.Empty()) {
 *         int courant = file.Front();
 *         file.Pop();
 *         
 *         printf("  Visite noeud %d\n", courant);
 *         
 *         // Explorer les voisins non visités
 *         for (int voisin : graphe[courant].voisins) {
 *             if (!graphe[voisin].visite) {
 *                 graphe[voisin].visite = true;
 *                 file.Push(voisin);  // Enfiler pour traitement ultérieur
 *             }
 *         }
 *     }
 * }
 * 
 * void exempleBFS() {
 *     // Graphe exemple: 0--1--2
 *     //                 |  |
 *     //                 3--4
 *     nkentseu::NkVector<Noeud> graphe(5);
 *     graphe[0] = {0, {1, 3}, false};
 *     graphe[1] = {1, {0, 2, 4}, false};
 *     graphe[2] = {2, {1, 4}, false};
 *     graphe[3] = {3, {0, 4}, false};
 *     graphe[4] = {4, {1, 2, 3}, false};
 *     
 *     BFS(graphe, 0);
 *     // Sortie possible (ordre dépend de l'ordre des voisins):
 *     //   Visite noeud 0
 *     //   Visite noeud 1
 *     //   Visite noeud 3
 *     //   Visite noeud 2
 *     //   Visite noeud 4
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 5 : Buffer circulaire pour traitement de flux
 * --------------------------------------------------------------------------
 * @code
 * // Simulation de traitement de données en flux continu
 * void exempleBufferFlux() {
 *     nkentseu::NkQueue<int> buffer;
 *     
 *     // Producteur : génère des données
 *     auto produire = [&](int valeur) {
 *         buffer.Push(valeur);
 *         printf("Produit: %d (taille buffer: %zu)\n", valeur, buffer.Size());
 *     };
 *     
 *     // Consommateur : traite les données dans l'ordre d'arrivée
 *     auto consommer = [&]() {
 *         if (!buffer.Empty()) {
 *             int valeur = buffer.Front();
 *             buffer.Pop();
 *             printf("Consommé: %d (taille buffer: %zu)\n", valeur, buffer.Size());
 *             return valeur;
 *         }
 *         return -1;  // Buffer vide
 *     };
 *     
 *     // Simulation entrelacée production/consommation
 *     produire(10);
 *     produire(20);
 *     consommer();  // Traite 10
 *     produire(30);
 *     consommer();  // Traite 20
 *     consommer();  // Traite 30
 *     consommer();  // Buffer vide -> -1
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 6 : Conteneur personnalisé et optimisations C++11
 * --------------------------------------------------------------------------
 * @code
 * // Utilisation d'un conteneur personnalisé (ex: NkVector si PushFront disponible)
 * // Note: NkDeque est recommandé car il supporte PushFront/PopFront efficaces
 * 
 * void exempleConteneurCustom() {
 *     // Queue avec NkDeque explicite (même que par défaut, mais explicite)
 *     nkentseu::NkQueue<int, nkentseu::NkDeque<int>> dequeQueue;
 *     
 *     dequeQueue.Push(1);
 *     dequeQueue.Push(2);
 *     // Mêmes opérations, conteneur spécifié explicitement
 * }
 * 
 * // Optimisations C++11 : move et emplace
 * #if defined(NK_CPP11)
 * 
 * void exempleOptimisations() {
 *     nkentseu::NkQueue<nkentseu::NkString> stringQueue;
 *     
 *     // Push avec move pour éviter la copie de la chaîne
 *     nkentseu::NkString lourd = "Données volumineuses...";
 *     stringQueue.Push(nkentseu::traits::NkMove(lourd));
 *     // 'lourd' est maintenant dans un état valide mais indéterminé
 *     
 *     // Emplace pour construction directe dans le conteneur
 *     stringQueue.Emplace("Formatage: %d", 42);
 *     // Évite la construction temporaire + move : construction in-place
 *     
 *     // Traitement FIFO avec consommation
 *     while (!stringQueue.Empty()) {
 *         printf("%s\n", stringQueue.Front().CStr());
 *         stringQueue.Pop();
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
 * 1. CHOIX DU CONTENEUR SOUS-JACENT :
 *    - NkDeque (défaut) : recommandé, PushBack/PopFront O(1) garanti aux deux extrémités
 *    - NkVector : déconseillé pour Queue car PopFront() est O(n) (décalage des éléments)
 *    - NkList : fonctionnel mais overhead de pointeurs, moins cache-friendly
 *    - Règle : toujours utiliser NkDeque pour NkQueue sauf besoin spécifique
 * 
 * 2. GESTION DES ERREURS ET SÉCURITÉ :
 *    - Toujours vérifier !Empty() avant Front(), Back() ou Pop() en production
 *    - Les assertions NKENTSEU_ASSERT aident au débogage en mode development
 *    - Pour les APIs publiques : préférer TryPop(T& out) retournant bool pour extraction sûre
 * 
 * 3. OPTIMISATIONS DE PERFORMANCE :
 *    - NkDeque gère automatiquement la croissance : pas besoin de reserve() manuel
 *    - Utiliser Emplace() pour éviter les constructions temporaires coûteuses
 *    - Privilégier Push(T&&) avec move pour les types lourds
 *    - Pour les volumes connus : pré-allouer le deque sous-jacent si l'API le permet
 * 
 * 4. MODÈLES D'ACCÈS ET MODIFICATION :
 *    - Front()/Back() retournent des références : modification possible mais attention à la cohérence
 *    - Pop() ne retourne pas l'élément : utiliser Front() avant Pop() si besoin de la valeur
 *    - Pattern courant : T val = queue.Front(); queue.Pop();
 * 
 * 5. SÉCURITÉ THREAD :
 *    - NkQueue est thread-unsafe par conception
 *    - Protéger avec un mutex externe pour accès concurrents multiples
 *    - Pour le pattern producer-consumer : envisager une file thread-safe dédiée avec sémaphores
 * 
 * 6. LIMITATIONS CONNUES (version actuelle) :
 *    - Pas d'accès au conteneur sous-jacent : ajouter GetContainer() si besoin d'itération
 *    - Pas de méthode TryPop() retournant l'élément supprimé : à ajouter pour API plus sûre
 *    - Pas de méthode Clear() explicite : utiliser boucle Pop() ou accéder au conteneur
 *    - Pas de comparaison entre files : operator== à ajouter si besoin
 * 
 * 7. EXTENSIONS RECOMMANDÉES :
 *    - Ajouter GetContainer() const& pour accès en lecture au conteneur interne
 *    - Implémenter TryPop(T& out) : bool pour extraction sûre sans assertion
 *    - Ajouter Clear() : alias pour vider complètement la file via le conteneur
 *    - Supporter les comparaisons lexicographiques : operator==, != pour files
 *    - Ajouter un itérateur en lecture seule (de Front vers Back) pour débogage
 *    - Implémenter une méthode Peek() alias de Front() pour sémantique plus claire
 * 
 * 8. COMPARAISON AVEC AUTRES STRUCTURES :
 *    - vs NkStack (LIFO) : Queue pour FIFO (ordre d'arrivée), Stack pour LIFO (ordre inverse)
 *    - vs NkDeque : Queue est un adaptateur limitant l'interface, Deque offre plus d'opérations
 *    - vs NkVector : Queue garantit sémantique FIFO, Vector permet accès aléatoire mais PopFront() lent
 *    - Règle : utiliser NkQueue quand seule la sémantique FIFO est requise et souhaitée
 * 
 * 9. PATTERNS AVANCÉS :
 *    - Priority Queue : utiliser NkPriorityQueue si l'ordre de traitement dépend d'une priorité
 *    - Double-ended Queue : utiliser NkDeque directement si besoin d'insertion/suppression aux deux bouts
 *    - Circular Buffer : implémenter un buffer à taille fixe avec indices de lecture/écriture modulo
 *    - Blocking Queue : pour la synchronisation thread, ajouter sémaphores/mutex pour attente bloquante
 */

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Généré par Rihen le 2026-02-05 22:26:13
// Dernière modification : 2026-04-26 (restructuration et documentation)
// ============================================================