// -----------------------------------------------------------------------------
// FICHIER: NKContainers\Containers\Adapters\NkStack.h
// DESCRIPTION: Adaptateur de pile - LIFO (Last In First Out)
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CONTAINERS_ADAPTERS_NKSTACK_H
#define NK_CONTAINERS_ADAPTERS_NKSTACK_H

    // ========================================================================
    // INCLUSIONS DES DÉPENDANCES
    // ========================================================================

    #include "NKCore/NkTypes.h"                    // Définition des types fondamentaux (usize, etc.)
    #include "NKContainers/NkContainersApi.h"      // Macros d'exportation de l'API conteneurs
    #include "NKCore/NkTraits.h"                   // Traits pour move/forward et utilitaires méta
    #include "NKCore/Assert/NkAssert.h"            // Macros d'assertion pour le débogage sécurisé
    #include "NKContainers/Sequential/NkVector.h"  // Conteneur séquentiel par défaut pour le stockage
    #include "NKContainers/Iterators/NkInitializerList.h" // Support des listes d'initialisation

    // ========================================================================
    // ESPACE DE NOMS PRINCIPAL
    // ========================================================================

    namespace nkentseu {

        // ====================================================================
        // CLASSE PRINCIPALE : NK STACK (ADAPTATEUR LIFO)
        // ====================================================================

        /**
         * @brief Classe template implémentant un adaptateur de pile LIFO (Last In First Out)
         * 
         * Adaptateur de conteneur fournissant une interface de pile avec sémantique
         * Last In First Out : le dernier élément ajouté est le premier à être retiré.
         * 
         * PRINCIPE DE FONCTIONNEMENT :
         * - Wrapper autour d'un conteneur séquentiel (NkVector par défaut)
         * - Toutes les opérations se font à une seule extrémité (le "sommet" de la pile)
         * - Délègue le stockage et la gestion mémoire au conteneur sous-jacent
         * - Aucune itération : accès uniquement à l'élément du sommet
         * 
         * GARANTIES DE PERFORMANCE (avec NkVector comme conteneur) :
         * - Push() : O(1) amorti (réallocation occasionnelle du vector)
         * - Pop() : O(1) constant
         * - Top() : O(1) constant, accès direct au dernier élément
         * - Size()/Empty() : O(1) constant, métadonnées du conteneur
         * 
         * CAS D'USAGE TYPQUES :
         * - Gestion d'appels récursifs et évaluation d'expressions (RPN)
         * - Algorithmes de backtracking (labyrinthes, puzzles, DFS)
         * - Historique d'actions avec annulation (undo/redo)
         * - Parsing de structures imbriquées (parenthèses, balises XML/HTML)
         * - Simulation de processus avec états empilés
         * 
         * @tparam T Type des éléments stockés dans la pile (doit être copiable/déplaçable)
         * @tparam Container Type du conteneur sous-jacent (défaut: NkVector<T>)
         *                   Doit supporter : PushBack(), PopBack(), Back(), Empty(), Size()
         * 
         * @note Thread-unsafe : synchronisation externe requise pour usage concurrent
         * @note Les opérations modifient uniquement le sommet : pas d'accès aléatoire
         * @note Le conteneur sous-jacent est accessible via GetContainer() si besoin (à ajouter)
         */
        template<typename T, typename Container = NkVector<T>>
        class NkStack {

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

                Container mContainer;                 ///< Conteneur séquentiel stockant les éléments de la pile

            // ====================================================================
            // SECTION PUBLIQUE : CONSTRUCTEURS ET ASSIGNATION
            // ====================================================================
            public:

                /**
                 * @brief Constructeur par défaut avec conteneur vide
                 * @note Initialise un NkStack vide prêt à recevoir des éléments
                 * @note Complexité O(1) : construction du conteneur sous-jacent uniquement
                 */
                NkStack() {
                }

                /**
                 * @brief Constructeur explicite depuis un conteneur existant
                 * @param cont Référence const vers le conteneur source à copier
                 * @note Copie le contenu du conteneur dans le stack interne
                 * @note Le sommet de la pile correspond au dernier élément du conteneur
                 */
                explicit NkStack(const Container& cont)
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
                    explicit NkStack(Container&& cont)
                        : mContainer(traits::NkMove(cont)) {
                    }

                #endif // defined(NK_CPP11)

                /**
                 * @brief Constructeur depuis NkInitializerList
                 * @param init Liste d'éléments pour initialiser la pile
                 * @note Insère les éléments dans l'ordre : le premier de la liste est au fond
                 * @note Le dernier élément de la liste se retrouve au sommet de la pile
                 */
                NkStack(NkInitializerList<T> init) {
                    for (auto& val : init) {
                        Push(val);
                    }
                }

                /**
                 * @brief Constructeur depuis std::initializer_list (C++11)
                 * @param init Liste standard d'éléments pour initialiser la pile
                 * @note Compatibilité avec la syntaxe C++11 : NkStack<int> s = {1, 2, 3};
                 * @note Le dernier élément (3) se retrouve au sommet : Top() retourne 3
                 */
                NkStack(std::initializer_list<T> init) {
                    for (auto& val : init) {
                        Push(val);
                    }
                }

                /**
                 * @brief Opérateur d'affectation depuis NkInitializerList
                 * @param init Nouvelle liste d'éléments pour remplacer le contenu actuel
                 * @return Référence vers l'instance courante pour le chaînage d'opérations
                 * @note Vide la pile actuelle avant d'insérer les nouveaux éléments
                 * @note Complexité O(n + m) où n = taille actuelle, m = taille de la liste
                 */
                NkStack& operator=(NkInitializerList<T> init) {
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
                 * @note Syntaxe C++11 : stack = {4, 5, 6};
                 */
                NkStack& operator=(std::initializer_list<T> init) {
                    while (!Empty()) {
                        Pop();
                    }
                    for (auto& val : init) {
                        Push(val);
                    }
                    return *this;
                }

            // ====================================================================
            // SECTION PUBLIQUE : ACCÈS À L'ÉLÉMENT DU SOMMET
            // ====================================================================

                /**
                 * @brief Accède à l'élément au sommet de la pile (version mutable)
                 * @return Référence non-const vers l'élément du sommet
                 * @pre La pile ne doit pas être vide (assertion NKENTSEU_ASSERT en debug)
                 * @note Complexité O(1) : délégation directe à Container::Back()
                 * @note Permet la modification : stack.Top() = newValue;
                 */
                Reference Top() {
                    NKENTSEU_ASSERT(!Empty());
                    return mContainer.Back();
                }

                /**
                 * @brief Accède à l'élément au sommet de la pile (version const)
                 * @return Référence const vers l'élément du sommet
                 * @pre La pile ne doit pas être vide (assertion NKENTSEU_ASSERT en debug)
                 * @note Version const pour usage sur NkStack const ou accès en lecture seule
                 * @note Complexité O(1) : délégation directe à Container::Back() const
                 */
                ConstReference Top() const {
                    NKENTSEU_ASSERT(!Empty());
                    return mContainer.Back();
                }

            // ====================================================================
            // SECTION PUBLIQUE : CAPACITÉ ET MÉTADONNÉES
            // ====================================================================

                /**
                 * @brief Teste si la pile ne contient aucun élément
                 * @return true si le conteneur sous-jacent est vide, false sinon
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Délègue à Container::Empty() : complexité dépend du conteneur
                 */
                bool Empty() const NKENTSEU_NOEXCEPT {
                    return mContainer.Empty();
                }

                /**
                 * @brief Retourne le nombre d'éléments actuellement dans la pile
                 * @return Valeur de type SizeType représentant la hauteur de la pile
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
                 * @brief Ajoute un nouvel élément au sommet de la pile
                 * @param value Référence const vers l'élément à pousser sur la pile
                 * @note Complexité O(1) amorti avec NkVector (réallocation occasionnelle)
                 * @note Délègue à Container::PushBack() : l'élément devient le nouveau sommet
                 */
                void Push(const T& value) {
                    mContainer.PushBack(value);
                }

                // ====================================================================
                // SUPPORT C++11 : MOVE SEMANTICS ET EMPLACE
                // ====================================================================
                #if defined(NK_CPP11)

                    /**
                     * @brief Ajoute un élément par déplacement au sommet de la pile
                     * @param value Rvalue reference vers l'élément à déplacer sur la pile
                     * @note Optimise les transferts pour les types lourds (NkString, NkVector, etc.)
                     * @note Nécessite que T soit MoveConstructible
                     */
                    void Push(T&& value) {
                        mContainer.PushBack(traits::NkMove(value));
                    }

                    /**
                     * @brief Construit et ajoute un élément directement au sommet de la pile
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
                 * @brief Supprime l'élément au sommet de la pile
                 * @pre La pile ne doit pas être vide (assertion NKENTSEU_ASSERT en debug)
                 * @note Complexité O(1) : délégation directe à Container::PopBack()
                 * @note L'élément supprimé n'est pas retourné : utiliser Top() avant Pop() si besoin
                 * @note Ne réduit pas nécessairement la capacité du conteneur sous-jacent
                 */
                void Pop() {
                    NKENTSEU_ASSERT(!Empty());
                    mContainer.PopBack();
                }

            // ====================================================================
            // SECTION PUBLIQUE : ÉCHANGE ET UTILITAIRES
            // ====================================================================

                /**
                 * @brief Échange le contenu de cette pile avec une autre en temps constant
                 * @param other Autre instance de NkStack à échanger
                 * @note noexcept : délègue à Container::Swap() pour échange de pointeurs
                 * @note Complexité O(1) : aucune allocation, copie ou destruction d'éléments
                 * @note Préférer à l'assignation pour les permutations efficaces de grandes piles
                 */
                void Swap(NkStack& other) NKENTSEU_NOEXCEPT {
                    mContainer.Swap(other.mContainer);
                }
        };

        // ====================================================================
        // FONCTIONS NON-MEMBRES : SWAP GLOBAL ET COMPARAISONS
        // ====================================================================

        /**
         * @brief Fonction swap non-membre pour ADL (Argument-Dependent Lookup)
         * @tparam T Type des éléments de la pile
         * @tparam Container Type du conteneur sous-jacent
         * @param lhs Première pile à échanger
         * @param rhs Seconde pile à échanger
         * @note noexcept : délègue à la méthode membre Swap()
         * @note Permet l'usage idiomatique : using nkentseu::NkSwap; NkSwap(a, b);
         */
        template<typename T, typename Container>
        void NkSwap(NkStack<T, Container>& lhs, NkStack<T, Container>& rhs) NKENTSEU_NOEXCEPT {
            lhs.Swap(rhs);
        }

        /**
         * @brief Opérateur d'égalité pour comparer deux piles
         * @tparam T Type des éléments de la pile
         * @tparam Container Type du conteneur sous-jacent
         * @param lhs Première pile à comparer
         * @param rhs Seconde pile à comparer
         * @return true si les conteneurs sous-jacents sont égaux, false sinon
         * @note Compare les éléments dans l'ordre : du fond vers le sommet de la pile
         * @note Complexité O(n) où n = Size() : comparaison élément par élément
         */
        template<typename T, typename Container>
        bool operator==(const NkStack<T, Container>& lhs, const NkStack<T, Container>& rhs) {
            return lhs.mContainer == rhs.mContainer;
        }

        /**
         * @brief Opérateur d'inégalité pour comparer deux piles
         * @tparam T Type des éléments de la pile
         * @tparam Container Type du conteneur sous-jacent
         * @param lhs Première pile à comparer
         * @param rhs Seconde pile à comparer
         * @return true si les piles diffèrent, false si identiques
         * @note Implémenté via !(lhs == rhs) pour cohérence et maintenance
         */
        template<typename T, typename Container>
        bool operator!=(const NkStack<T, Container>& lhs, const NkStack<T, Container>& rhs) {
            return !(lhs == rhs);
        }

    } // namespace nkentseu

#endif // NK_CONTAINERS_ADAPTERS_NKSTACK_H

// ============================================================================
// EXEMPLES D'UTILISATION ET PATTERNS COURANTS
// ============================================================================
/*
 * @section examples Exemples pratiques d'utilisation de NkStack
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 1 : Opérations de base LIFO
 * --------------------------------------------------------------------------
 * @code
 * #include "NKContainers/Adapters/NkStack.h"
 * #include <cstdio>
 * 
 * void exempleBase() {
 *     // Création d'une pile d'entiers avec conteneur par défaut (NkVector)
 *     nkentseu::NkStack<int> stack;
 *     
 *     // Empilement (Push) : LIFO - dernier entré, premier sorti
 *     stack.Push(1);  // Pile: [1]
 *     stack.Push(2);  // Pile: [1, 2]
 *     stack.Push(3);  // Pile: [1, 2, 3] <- sommet
 *     
 *     // Accès au sommet sans suppression
 *     int top = stack.Top();  // 3
 *     printf("Sommet: %d\n", top);
 *     
 *     // Dépilement (Pop) : retire le sommet
 *     stack.Pop();  // Pile: [1, 2]
 *     printf("Nouveau sommet: %d\n", stack.Top());  // 2
 *     
 *     // Vérification d'état
 *     printf("Taille: %zu, Vide: %s\n", 
 *            stack.Size(), stack.Empty() ? "oui" : "non");  // Taille: 2, Vide: non
 *     
 *     // Vidage complet de la pile
 *     while (!stack.Empty()) {
 *         printf("%d ", stack.Top());
 *         stack.Pop();
 *     }
 *     // Sortie: 2 1 (ordre inverse de l'insertion)
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
 *     // Ordre : premier élément au fond, dernier au sommet
 *     nkentseu::NkStack<int> stack1 = {10, 20, 30};
 *     // Pile: [10, 20, 30] <- sommet (Top() = 30)
 *     
 *     // Construction depuis conteneur existant
 *     nkentseu::NkVector<int> vec = {1, 2, 3, 4};
 *     nkentseu::NkStack<int> stack2(vec);
 *     // Pile: [1, 2, 3, 4] <- sommet (Top() = 4)
 *     
 *     // Déplacement de conteneur (C++11)
 *     #if defined(NK_CPP11)
 *     nkentseu::NkVector<int> vec2 = {5, 6, 7};
 *     nkentseu::NkStack<int> stack3(nkentseu::traits::NkMove(vec2));
 *     // vec2 est maintenant vide, stack3 possède [5, 6, 7]
 *     #endif
 *     
 *     // Assignation depuis liste
 *     stack1 = {100, 200};  // Remplace le contenu: [100, 200] <- sommet
 *     
 *     // Comparaison de piles
 *     nkentseu::NkStack<int> stack4 = {100, 200};
 *     bool same = (stack1 == stack4);  // true : mêmes éléments dans le même ordre
 *     printf("Piles identiques: %s\n", same ? "oui" : "non");
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 3 : Évaluation d'expression en notation polonaise inverse (RPN)
 * --------------------------------------------------------------------------
 * @code
 * // Calculatrice RPN : "3 4 + 2 *" => (3+4)*2 = 14
 * int EvaluerRPN(const char* expression) {
 *     nkentseu::NkStack<int> pile;
 *     
 *     // Parsing simplifié (en production: tokenizer complet)
 *     const char* tokens[] = {"3", "4", "+", "2", "*"};
 *     usize count = 5;
 *     
 *     for (usize i = 0; i < count; ++i) {
 *         const char* token = tokens[i];
 *         
 *         if (token[0] >= '0' && token[0] <= '9') {
 *             // Opérande : empiler la valeur
 *             pile.Push(token[0] - '0');
 *         } else {
 *             // Opérateur : dépiler deux opérandes, appliquer, empiler résultat
 *             int b = pile.Top(); pile.Pop();
 *             int a = pile.Top(); pile.Pop();
 *             
 *             switch (token[0]) {
 *                 case '+': pile.Push(a + b); break;
 *                 case '-': pile.Push(a - b); break;
 *                 case '*': pile.Push(a * b); break;
 *                 case '/': pile.Push(a / b); break;
 *             }
 *         }
 *     }
 *     
 *     return pile.Top();  // Résultat final au sommet
 * }
 * 
 * void exempleRPN() {
 *     int result = EvaluerRPN("3 4 + 2 *");
 *     printf("Résultat RPN: %d\n", result);  // 14
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 4 : Backtracking - Résolution de labyrinthe (simplifié)
 * --------------------------------------------------------------------------
 * @code
 * struct Position {
 *     int x, y;
 * };
 * 
 * // Simulation de backtracking avec pile d'états
 * void exempleBacktracking() {
 *     nkentseu::NkStack<Position> chemin;
 *     
 *     // Point de départ
 *     chemin.Push({0, 0});
 *     
 *     // Exploration simulée
 *     while (!chemin.Empty()) {
 *         Position current = chemin.Top();
 *         printf("Exploration: (%d, %d)\n", current.x, current.y);
 *         
 *         // Condition d'arrêt simulée
 *         if (current.x == 2 && current.y == 2) {
 *             printf("Sortie trouvée!\n");
 *             break;
 *         }
 *         
 *         // Essayer une direction (simplifié)
 *         bool avancé = false;
 *         if (current.x < 2) {
 *             chemin.Push({current.x + 1, current.y});
 *             avancé = true;
 *         }
 *         
 *         // Backtrack si impasse
 *         if (!avancé) {
 *             printf("Impasse, retour arrière\n");
 *             chemin.Pop();
 *         }
 *     }
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 5 : Historique d'actions avec undo/redo (simplifié)
 * --------------------------------------------------------------------------
 * @code
 * class Historique {
 * private:
 *     nkentseu::NkStack<const char*> mUndoStack;  // Actions pour undo
 *     nkentseu::NkStack<const char*> mRedoStack;  // Actions pour redo
 *     
 * public:
 *     void Executer(const char* action) {
 *         mUndoStack.Push(action);
 *         // Vider le redo stack après une nouvelle action
 *         while (!mRedoStack.Empty()) {
 *             mRedoStack.Pop();
 *         }
 *         printf("Action: %s\n", action);
 *     }
 *     
 *     void Undo() {
 *         if (!mUndoStack.Empty()) {
 *             const char* action = mUndoStack.Top();
 *             mUndoStack.Pop();
 *             mRedoStack.Push(action);
 *             printf("Undo: %s\n", action);
 *         }
 *     }
 *     
 *     void Redo() {
 *         if (!mRedoStack.Empty()) {
 *             const char* action = mRedoStack.Top();
 *             mRedoStack.Pop();
 *             mUndoStack.Push(action);
 *             printf("Redo: %s\n", action);
 *         }
 *     }
 * };
 * 
 * void exempleUndoRedo() {
 *     Historique hist;
 *     
 *     hist.Executer("Créer fichier");
 *     hist.Executer("Écrire contenu");
 *     hist.Executer("Sauvegarder");
 *     
 *     hist.Undo();  // Annule "Sauvegarder"
 *     hist.Undo();  // Annule "Écrire contenu"
 *     
 *     hist.Redo();  // Rétablit "Écrire contenu"
 *     hist.Executer("Modifier contenu");  // Nouvelle action vide le redo
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 6 : Conteneur personnalisé et optimisations C++11
 * --------------------------------------------------------------------------
 * @code
 * // Utilisation d'un conteneur personnalisé (ex: NkDeque pour PushBack/PopBack efficaces)
 * #include "NKContainers/Sequential/NkDeque.h"
 * 
 * void exempleConteneurCustom() {
 *     // Stack avec NkDeque au lieu de NkVector par défaut
 *     nkentseu::NkStack<int, nkentseu::NkDeque<int>> dequeStack;
 *     
 *     dequeStack.Push(1);
 *     dequeStack.Push(2);
 *     // Mêmes opérations, conteneur différent en interne
 * }
 * 
 * // Optimisations C++11 : move et emplace
 * #if defined(NK_CPP11)
 * 
 * void exempleOptimisations() {
 *     nkentseu::NkStack<nkentseu::NkString> stringStack;
 *     
 *     // Push avec move pour éviter la copie de la chaîne
 *     nkentseu::NkString lourd = "Données volumineuses...";
 *     stringStack.Push(nkentseu::traits::NkMove(lourd));
 *     // 'lourd' est maintenant dans un état valide mais indéterminé
 *     
 *     // Emplace pour construction directe dans le conteneur
 *     stringStack.Emplace("Formatage: %d", 42);
 *     // Évite la construction temporaire + move : construction in-place
 *     
 *     // Accès et consommation
 *     while (!stringStack.Empty()) {
 *         printf("%s\n", stringStack.Top().CStr());
 *         stringStack.Pop();
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
 *    - NkVector (défaut) : bon compromis, PushBack/PopBack O(1) amorti
 *    - NkDeque : PushBack/PopBack O(1) garanti, meilleure performance pour grandes piles
 *    - NkList : évite les réallocations, mais overhead de pointeurs par élément
 *    - Règle : utiliser NkVector sauf si réallocations fréquentes problématiques
 * 
 * 2. GESTION DES ERREURS ET SÉCURITÉ :
 *    - Toujours vérifier !Empty() avant Top() ou Pop() en production
 *    - Les assertions NKENTSEU_ASSERT aident au débogage en mode development
 *    - Pour les APIs publiques : préférer TryPop(T& out) retournant bool
 * 
 * 3. OPTIMISATIONS DE PERFORMANCE :
 *    - Pré-réserver la capacité du conteneur si le volume est connu :
 *      stack.GetContainer().Reserve(1000);  // À ajouter via méthode d'accès
 *    - Utiliser Emplace() pour éviter les constructions temporaires coûteuses
 *    - Privilégier Push(T&&) avec move pour les types lourds
 * 
 * 4. MODÈLES D'ACCÈS ET MODIFICATION :
 *    - Top() retourne une référence : modification possible mais attention à la cohérence
 *    - Pop() ne retourne pas l'élément : utiliser Top() avant Pop() si besoin de la valeur
 *    - Pattern courant : T val = stack.Top(); stack.Pop();
 * 
 * 5. SÉCURITÉ THREAD :
 *    - NkStack est thread-unsafe par conception
 *    - Protéger avec un mutex externe pour accès concurrents multiples
 *    - Pour le pattern producer-consumer : envisager une pile thread-safe dédiée
 * 
 * 6. LIMITATIONS CONNUES (version actuelle) :
 *    - Pas d'accès au conteneur sous-jacent : ajouter GetContainer() si besoin d'itération
 *    - Pas de méthode TryPop() retournant l'élément supprimé : à ajouter pour API plus sûre
 *    - Pas de swap non-member avec ADL complet : vérifier l'usage avec std::swap
 * 
 * 7. EXTENSIONS RECOMMANDÉES :
 *    - Ajouter GetContainer() const& pour accès en lecture au conteneur interne
 *    - Implémenter TryPop(T& out) : bool pour extraction sûre sans assertion
 *    - Ajouter Clear() : alias pour vider complètement la pile
 *    - Supporter les comparaisons lexicographiques : operator<, <=, >, >=
 *    - Ajouter un itérateur en lecture seule (du sommet vers le fond) pour débogage
 * 
 * 8. COMPARAISON AVEC AUTRES STRUCTURES :
 *    - vs NkVector : Stack limite l'interface à LIFO, plus sûr sémantiquement
 *    - vs NkQueue (FIFO) : Stack pour LIFO, Queue pour FIFO, choisir selon l'algo
 *    - vs NkDeque : Stack est un adaptateur, Deque est un conteneur avec plus d'opérations
 *    - Règle : utiliser NkStack quand seule la sémantique LIFO est requise
 */

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Généré par Rihen le 2026-02-05 22:26:13
// Dernière modification : 2026-04-26 (restructuration et documentation)
// ============================================================