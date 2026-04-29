// -----------------------------------------------------------------------------
// FICHIER: NKContainers\Containers\CacheFriendly\NkArray.h
// DESCRIPTION: Tableau de taille fixe - Wrapper zero-cost autour de tableau C
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CONTAINERS_CACHEFRIENDLY_NKARRAY_H
#define NK_CONTAINERS_CACHEFRIENDLY_NKARRAY_H

    // ========================================================================
    // INCLUSIONS DES DÉPENDANCES
    // ========================================================================

    #include "NKCore/NkTypes.h"                    // Définition des types fondamentaux (usize, etc.)
    #include "NKContainers/NkContainersApi.h"      // Macros d'exportation de l'API conteneurs
    #include "NKCore/NkTraits.h"                   // Traits pour swap et utilitaires méta
    #include "NKCore/Assert/NkAssert.h"            // Macros d'assertion pour le débogage sécurisé
    #include "NKContainers/Iterators/NkIterator.h" // Infrastructure pour itérateurs inverses

    // ========================================================================
    // ESPACE DE NOMS PRINCIPAL
    // ========================================================================

    namespace nkentseu {

        // ====================================================================
        // CLASSE PRINCIPALE : NK ARRAY (TABLEAU DE TAILLE FIXE)
        // ====================================================================

        /**
         * @brief Structure template représentant un tableau de taille fixe
         * 
         * Conteneur offrant une interface moderne et sûre autour d'un tableau C natif,
         * avec zéro overhead en performance et une localité mémoire optimale.
         * 
         * PROPRIÉTÉS FONDAMENTALES :
         * - Allocation sur la pile (stack) : pas d'allocation dynamique
         * - Layout mémoire contigu : identique à T[N], excellent pour le cache CPU
         * - Taille fixe connue à la compilation : optimisations agressives possibles
         * - Aggregate initialization : compatible avec { ... } sans constructeur
         * - Interface STL-like : itérateurs, accès bornés, algorithmes génériques
         * 
         * GARANTIES DE PERFORMANCE :
         * - Accès élément : O(1) constant, adressage direct par indice
         * - Itération : O(n) séquentiel, prefetching CPU optimal
         * - Mémoire : sizeof(T) * N exactement, aucun overhead de métadonnées
         * - Construction/destruction : trivial pour les types POD, sinon élément par élément
         * 
         * CAS D'USAGE TYPQUES :
         * - Buffers de taille fixe pour E/S et traitement de données
         * - Tables de lookup et constantes compilées
         * - Structures de données embarquées avec contraintes mémoire strictes
         * - Remplacement de std::array sans dépendance STL
         * - Données temporaires dans des boucles critiques (évite heap fragmentation)
         * 
         * @tparam T Type des éléments stockés (doit être copiable/assignable pour Fill/Swap)
         * @tparam N Nombre d'éléments dans le tableau (constexpr, connu à la compilation)
         * 
         * @note Thread-safe pour les lectures concurrentes sur instances distinctes
         * @note La taille N=0 est supportée via spécialisation (tableau vide)
         * @note Aggregate type : peut être initialisé avec { ... } sans constructeur explicite
         */
        template<typename T, usize N>
        struct NkArray {

            // ====================================================================
            // SECTION PUBLIQUE : ALIASES DE TYPES (CONVENTIONS DUAL)
            // ====================================================================
            public:

                // ====================================================================
                // ALIASES STYLE NKENGINE (PascalCase)
                // ====================================================================

                using ValueType = T;                  ///< Alias du type des éléments stockés
                using SizeType = usize;               ///< Alias du type pour les tailles/comptages
                using Reference = T&;                 ///< Alias de référence non-const vers un élément
                using ConstReference = const T&;      ///< Alias de référence const vers un élément
                using Pointer = T*;                   ///< Alias de pointeur non-const vers un élément
                using ConstPointer = const T*;        ///< Alias de pointeur const vers un élément
                using Iterator = T*;                  ///< Alias d'itérateur mutable (pointeur)
                using ConstIterator = const T*;       ///< Alias d'itérateur const (pointeur const)
                using ReverseIterator = NkReverseIterator<Iterator>;         ///< Itérateur inverse mutable
                using ConstReverseIterator = NkReverseIterator<ConstIterator>; ///< Itérateur inverse const

                // ====================================================================
                // ALIASES STYLE STL (lowercase) POUR COMPATIBILITÉ
                // ====================================================================

                using value_type = T;                 ///< Alias lowercase du type des éléments
                using size_type = usize;              ///< Alias lowercase du type de taille
                using reference = T&;                 ///< Alias lowercase de référence non-const
                using const_reference = const T&;     ///< Alias lowercase de référence const
                using pointer = T*;                   ///< Alias lowercase de pointeur non-const
                using const_pointer = const T*;       ///< Alias lowercase de pointeur const
                using iterator = T*;                  ///< Alias lowercase d'itérateur mutable
                using const_iterator = const T*;      ///< Alias lowercase d'itérateur const

            // ====================================================================
            // SECTION PUBLIQUE : DONNÉES MEMBRES (AGGREGATE INIT)
            // ====================================================================
            public:

                /**
                 * @brief Tableau de données brut, exposé publiquement pour aggregate initialization
                 * @note Allocation contiguë : &mData[i+1] == &mData[i] + 1 garanti
                 * @note Pour N=0 : tableau de taille 1 pour éviter UB, mais inaccessible via API
                 * @note Public pour compatibilité C++ aggregate : NkArray<int,3> a = {1,2,3};
                 */
                T mData[N > 0 ? N : 1];

            // ====================================================================
            // SECTION PUBLIQUE : ACCÈS AUX ÉLÉMENTS (BOUNDED & UNBOUNDED)
            // ====================================================================
            public:

                /**
                 * @brief Accès borné à un élément avec vérification d'index en debug
                 * @param i Index de l'élément à accéder (doit être dans [0, N))
                 * @return Référence non-const vers l'élément à l'index spécifié
                 * @pre i < N (assertion NKENTSEU_ASSERT en mode debug)
                 * @note Complexité O(1) : adressage direct via calcul d'offset
                 * @note Préférer à operator[] quand la sécurité des bornes est critique
                 */
                Reference At(SizeType i) {
                    NKENTSEU_ASSERT(i < N);
                    return mData[i];
                }

                /**
                 * @brief Accès borné à un élément avec vérification d'index en debug (version const)
                 * @param i Index de l'élément à accéder (doit être dans [0, N))
                 * @return Référence const vers l'élément à l'index spécifié
                 * @pre i < N (assertion NKENTSEU_ASSERT en mode debug)
                 * @note Version const pour usage sur NkArray const ou accès en lecture seule
                 */
                ConstReference At(SizeType i) const {
                    NKENTSEU_ASSERT(i < N);
                    return mData[i];
                }

                /**
                 * @brief Accès non-borné à un élément via operator[]
                 * @param i Index de l'élément à accéder
                 * @return Référence non-const vers l'élément à l'index spécifié
                 * @pre i < N (assertion NKENTSEU_ASSERT en mode debug, pas de vérification en release)
                 * @note noexcept : opération purement arithmétique, pas d'exception possible
                 * @note Plus rapide que At() en release : pas de vérification de bornes
                 */
                Reference operator[](SizeType i) NKENTSEU_NOEXCEPT {
                    NKENTSEU_ASSERT(i < N);
                    return mData[i];
                }

                /**
                 * @brief Accès non-borné à un élément via operator[] (version const)
                 * @param i Index de l'élément à accéder
                 * @return Référence const vers l'élément à l'index spécifié
                 * @pre i < N (assertion NKENTSEU_ASSERT en mode debug)
                 * @note noexcept : opération purement arithmétique, pas d'exception possible
                 * @note Version const pour usage sur NkArray const ou accès en lecture seule
                 */
                ConstReference operator[](SizeType i) const NKENTSEU_NOEXCEPT {
                    NKENTSEU_ASSERT(i < N);
                    return mData[i];
                }

                /**
                 * @brief Accès au premier élément du tableau (version mutable)
                 * @return Référence non-const vers mData[0]
                 * @pre N > 0 (accès à mData[0] suppose un tableau non-vide)
                 * @note constexpr : évaluable à la compilation pour les tableaux constexpr
                 * @note noexcept : accès direct par indice, aucune logique complexe
                 */
                NKENTSEU_CONSTEXPR Reference Front() NKENTSEU_NOEXCEPT {
                    return mData[0];
                }

                /**
                 * @brief Accès au premier élément du tableau (version const)
                 * @return Référence const vers mData[0]
                 * @pre N > 0 (accès à mData[0] suppose un tableau non-vide)
                 * @note constexpr : évaluable à la compilation pour les tableaux constexpr
                 * @note noexcept : accès direct par indice, aucune logique complexe
                 */
                NKENTSEU_CONSTEXPR ConstReference Front() const NKENTSEU_NOEXCEPT {
                    return mData[0];
                }

                /**
                 * @brief Accès au dernier élément du tableau (version mutable)
                 * @return Référence non-const vers mData[N-1]
                 * @pre N > 0 (accès à mData[N-1] suppose un tableau non-vide)
                 * @note constexpr : évaluable à la compilation pour les tableaux constexpr
                 * @note noexcept : calcul d'index arithmétique simple
                 */
                NKENTSEU_CONSTEXPR Reference Back() NKENTSEU_NOEXCEPT {
                    return mData[N - 1];
                }

                /**
                 * @brief Accès au dernier élément du tableau (version const)
                 * @return Référence const vers mData[N-1]
                 * @pre N > 0 (accès à mData[N-1] suppose un tableau non-vide)
                 * @note constexpr : évaluable à la compilation pour les tableaux constexpr
                 * @note noexcept : calcul d'index arithmétique simple
                 */
                NKENTSEU_CONSTEXPR ConstReference Back() const NKENTSEU_NOEXCEPT {
                    return mData[N - 1];
                }

                /**
                 * @brief Accès au pointeur brut vers le début du tableau (version mutable)
                 * @return Pointeur non-const vers le premier élément (équivalent à &mData[0])
                 * @note constexpr : évaluable à la compilation
                 * @note noexcept : retourne l'adresse du membre sans calcul
                 * @note Compatible avec les APIs C attendent un T* : func(arr.Data(), arr.Size())
                 */
                NKENTSEU_CONSTEXPR Pointer Data() NKENTSEU_NOEXCEPT {
                    return mData;
                }

                /**
                 * @brief Accès au pointeur brut vers le début du tableau (version const)
                 * @return Pointeur const vers le premier élément (équivalent à &mData[0])
                 * @note constexpr : évaluable à la compilation
                 * @note noexcept : retourne l'adresse du membre sans calcul
                 * @note Compatible avec les APIs C attendent un const T*
                 */
                NKENTSEU_CONSTEXPR ConstPointer Data() const NKENTSEU_NOEXCEPT {
                    return mData;
                }

                // ====================================================================
                // ALIASES LOWERCASE POUR COMPATIBILITÉ STL
                // ====================================================================

                /**
                 * @brief Alias lowercase de Front() pour compatibilité STL
                 * @return Référence non-const vers le premier élément
                 */
                NKENTSEU_CONSTEXPR Reference front() NKENTSEU_NOEXCEPT {
                    return Front();
                }

                /**
                 * @brief Alias lowercase de Front() pour compatibilité STL (const)
                 * @return Référence const vers le premier élément
                 */
                NKENTSEU_CONSTEXPR ConstReference front() const NKENTSEU_NOEXCEPT {
                    return Front();
                }

                /**
                 * @brief Alias lowercase de Back() pour compatibilité STL
                 * @return Référence non-const vers le dernier élément
                 */
                NKENTSEU_CONSTEXPR Reference back() NKENTSEU_NOEXCEPT {
                    return Back();
                }

                /**
                 * @brief Alias lowercase de Back() pour compatibilité STL (const)
                 * @return Référence const vers le dernier élément
                 */
                NKENTSEU_CONSTEXPR ConstReference back() const NKENTSEU_NOEXCEPT {
                    return Back();
                }

                /**
                 * @brief Alias lowercase de Data() pour compatibilité STL
                 * @return Pointeur non-const vers le début du tableau
                 */
                NKENTSEU_CONSTEXPR Pointer data() NKENTSEU_NOEXCEPT {
                    return Data();
                }

                /**
                 * @brief Alias lowercase de Data() pour compatibilité STL (const)
                 * @return Pointeur const vers le début du tableau
                 */
                NKENTSEU_CONSTEXPR ConstPointer data() const NKENTSEU_NOEXCEPT {
                    return Data();
                }

            // ====================================================================
            // SECTION PUBLIQUE : ITÉRATEURS (POINTERS AS ITERATORS)
            // ====================================================================
            public:

                /**
                 * @brief Retourne un itérateur mutable vers le premier élément
                 * @return Iterator (T*) pointant vers mData[0]
                 * @note constexpr : évaluable à la compilation
                 * @note noexcept : retourne l'adresse du premier élément sans calcul
                 * @note Compatible range-based for : for (auto& x : arr) { ... }
                 */
                NKENTSEU_CONSTEXPR Iterator begin() NKENTSEU_NOEXCEPT {
                    return mData;
                }

                /**
                 * @brief Retourne un itérateur const vers le premier élément
                 * @return ConstIterator (const T*) pointant vers mData[0]
                 * @note constexpr : évaluable à la compilation
                 * @note noexcept : retourne l'adresse du premier élément sans calcul
                 */
                NKENTSEU_CONSTEXPR ConstIterator begin() const NKENTSEU_NOEXCEPT {
                    return mData;
                }

                /**
                 * @brief Retourne un itérateur const vers le premier élément (alias cbegin)
                 * @return ConstIterator (const T*) pointant vers mData[0]
                 * @note constexpr : évaluable à la compilation
                 * @note noexcept : retourne l'adresse du premier élément sans calcul
                 * @note Convention STL : cbegin() retourne toujours un const_iterator
                 */
                NKENTSEU_CONSTEXPR ConstIterator cbegin() const NKENTSEU_NOEXCEPT {
                    return mData;
                }

                /**
                 * @brief Retourne un itérateur mutable vers la position "fin" (sentinelle)
                 * @return Iterator (T*) pointant vers mData[N] (un-past-the-end)
                 * @note constexpr : évaluable à la compilation
                 * @note noexcept : calcul d'adresse par arithmétique de pointeurs
                 * @note Condition d'arrêt pour les itérations : it != arr.end()
                 */
                NKENTSEU_CONSTEXPR Iterator end() NKENTSEU_NOEXCEPT {
                    return mData + N;
                }

                /**
                 * @brief Retourne un itérateur const vers la position "fin" (sentinelle)
                 * @return ConstIterator (const T*) pointant vers mData[N]
                 * @note constexpr : évaluable à la compilation
                 * @note noexcept : calcul d'adresse par arithmétique de pointeurs
                 */
                NKENTSEU_CONSTEXPR ConstIterator end() const NKENTSEU_NOEXCEPT {
                    return mData + N;
                }

                /**
                 * @brief Retourne un itérateur const vers la position "fin" (alias cend)
                 * @return ConstIterator (const T*) pointant vers mData[N]
                 * @note constexpr : évaluable à la compilation
                 * @note noexcept : calcul d'adresse par arithmétique de pointeurs
                 * @note Convention STL : cend() retourne toujours un const_iterator
                 */
                NKENTSEU_CONSTEXPR ConstIterator cend() const NKENTSEU_NOEXCEPT {
                    return mData + N;
                }

                /**
                 * @brief Retourne un itérateur inverse mutable vers le dernier élément
                 * @return ReverseIterator initialisé avec end() pour parcours arrière
                 * @note constexpr : évaluable à la compilation
                 * @note noexcept : construction de l'itérateur inverse sans allocation
                 * @note Parcours : rbegin() = dernier élément, rend() = avant le premier
                 */
                NKENTSEU_CONSTEXPR ReverseIterator rbegin() NKENTSEU_NOEXCEPT {
                    return ReverseIterator(end());
                }

                /**
                 * @brief Retourne un itérateur inverse const vers le dernier élément
                 * @return ConstReverseIterator initialisé avec end() pour parcours arrière
                 * @note constexpr : évaluable à la compilation
                 * @note noexcept : construction de l'itérateur inverse sans allocation
                 */
                NKENTSEU_CONSTEXPR ConstReverseIterator rbegin() const NKENTSEU_NOEXCEPT {
                    return ConstReverseIterator(end());
                }

                /**
                 * @brief Retourne un itérateur inverse mutable vers la position "fin inverse"
                 * @return ReverseIterator initialisé avec begin() (sentinelle arrière)
                 * @note constexpr : évaluable à la compilation
                 * @note noexcept : construction de l'itérateur inverse sans allocation
                 * @note Condition d'arrêt pour parcours inverse : rit != arr.rend()
                 */
                NKENTSEU_CONSTEXPR ReverseIterator rend() NKENTSEU_NOEXCEPT {
                    return ReverseIterator(begin());
                }

                /**
                 * @brief Retourne un itérateur inverse const vers la position "fin inverse"
                 * @return ConstReverseIterator initialisé avec begin() (sentinelle arrière)
                 * @note constexpr : évaluable à la compilation
                 * @note noexcept : construction de l'itérateur inverse sans allocation
                 */
                NKENTSEU_CONSTEXPR ConstReverseIterator rend() const NKENTSEU_NOEXCEPT {
                    return ConstReverseIterator(begin());
                }

            // ====================================================================
            // SECTION PUBLIQUE : CAPACITÉ ET MÉTADONNÉES (COMPILE-TIME)
            // ====================================================================
            public:

                /**
                 * @brief Teste si le tableau ne contient aucun élément
                 * @return true si N == 0, false sinon
                 * @note constexpr : évaluable à la compilation, optimisation par le compilateur
                 * @note noexcept : comparaison arithmétique simple, pas d'effet de bord
                 * @note Pour N > 0 : retourne toujours false (tableau de taille fixe non-vide)
                 */
                NKENTSEU_CONSTEXPR bool Empty() const NKENTSEU_NOEXCEPT {
                    return N == 0;
                }

                /**
                 * @brief Retourne le nombre d'éléments dans le tableau
                 * @return Valeur constexpr de type SizeType égale au template parameter N
                 * @note constexpr : connu à la compilation, peut être utilisé dans des expressions constexpr
                 * @note noexcept : retourne une constante, aucun calcul à l'exécution
                 * @note Utile pour les boucles : for (usize i = 0; i < arr.Size(); ++i)
                 */
                NKENTSEU_CONSTEXPR SizeType Size() const NKENTSEU_NOEXCEPT {
                    return N;
                }

                /**
                 * @brief Retourne la taille maximale possible (identique à Size() pour tableau fixe)
                 * @return Valeur constexpr de type SizeType égale à N
                 * @note constexpr : connu à la compilation
                 * @note noexcept : retourne une constante
                 * @note Redondant avec Size() mais présent pour compatibilité interface STL
                 */
                NKENTSEU_CONSTEXPR SizeType MaxSize() const NKENTSEU_NOEXCEPT {
                    return N;
                }

                // ====================================================================
                // ALIASES LOWERCASE POUR COMPATIBILITÉ STL
                // ====================================================================

                /**
                 * @brief Alias lowercase de Empty() pour compatibilité STL
                 * @return true si le tableau est vide (N == 0)
                 */
                NKENTSEU_CONSTEXPR bool empty() const NKENTSEU_NOEXCEPT {
                    return Empty();
                }

                /**
                 * @brief Alias lowercase de Size() pour compatibilité STL
                 * @return Nombre d'éléments dans le tableau (N)
                 */
                NKENTSEU_CONSTEXPR SizeType size() const NKENTSEU_NOEXCEPT {
                    return Size();
                }

                /**
                 * @brief Alias lowercase de MaxSize() pour compatibilité STL
                 * @return Taille maximale (N, identique à size())
                 */
                NKENTSEU_CONSTEXPR SizeType max_size() const NKENTSEU_NOEXCEPT {
                    return MaxSize();
                }

            // ====================================================================
            // SECTION PUBLIQUE : OPÉRATIONS SUR LE CONTENU
            // ====================================================================
            public:

                /**
                 * @brief Remplit tous les éléments du tableau avec une valeur donnée
                 * @param value Référence const vers la valeur à copier dans chaque élément
                 * @note Complexité O(N) : affectation séquentielle de chaque élément
                 * @note Utilise l'opérateur d'affectation de T : T doit être Assignable
                 * @note Pour les types triviaux : peut être optimisé en memset par le compilateur
                 */
                void Fill(const T& value) {
                    for (SizeType i = 0; i < N; ++i) {
                        mData[i] = value;
                    }
                }

                /**
                 * @brief Alias lowercase de Fill() pour compatibilité STL
                 * @param value Référence const vers la valeur à copier dans chaque élément
                 */
                void fill(const T& value) {
                    Fill(value);
                }

                /**
                 * @brief Échange le contenu de ce tableau avec un autre de même type et taille
                 * @param other Référence vers un autre NkArray<T, N> à échanger
                 * @note noexcept : utilise traits::NkSwap() élément par élément
                 * @note Complexité O(N) : N swaps individuels, aucun allocation temporaire
                 * @note Préférer à l'assignation pour les permutations efficaces de grands tableaux
                 */
                void Swap(NkArray& other) NKENTSEU_NOEXCEPT {
                    for (SizeType i = 0; i < N; ++i) {
                        traits::NkSwap(mData[i], other.mData[i]);
                    }
                }

                /**
                 * @brief Alias lowercase de Swap() pour compatibilité STL
                 * @param other Référence vers un autre NkArray<T, N> à échanger
                 */
                void swap(NkArray& other) NKENTSEU_NOEXCEPT {
                    Swap(other);
                }
        };

        // ====================================================================
        // SPÉCIALISATION POUR N = 0 (TABLEAU VIDE)
        // ====================================================================

        /**
         * @brief Spécialisation de NkArray pour le cas N = 0 (tableau vide)
         * 
         * Gère le cas dégénéré d'un tableau de taille zéro sans comportement indéfini.
         * Toutes les opérations sont no-op ou retournent des valeurs constantes.
         * 
         * @note Aucun stockage de données : pas de membre mData alloué
         * @note operator[] retourne une référence vers un dummy static (assert en debug)
         * @note Utile pour la généricité : templates fonctionnant avec N quelconque
         */
        template<typename T>
        struct NkArray<T, 0> {

            // ====================================================================
            // ALIASES DE TYPES MINIMAUX POUR COMPATIBILITÉ
            // ====================================================================
            public:

                using ValueType = T;              ///< Alias du type des éléments (théorique)
                using SizeType = usize;           ///< Alias du type de taille
                using Reference = T&;             ///< Alias de référence (retourne dummy)
                using value_type = T;             ///< Alias lowercase du type
                using size_type = usize;          ///< Alias lowercase du type de taille
                using reference = T&;             ///< Alias lowercase de référence

            // ====================================================================
            // OPÉRATEUR D'ACCÈS (TOUJOURS INVALIDE POUR N=0)
            // ====================================================================
            public:

                /**
                 * @brief Operator[] pour tableau vide : toujours une erreur logique
                 * @param i Index demandé (ignoré, aucun élément n'existe)
                 * @return Référence vers un dummy static (comportement indéfini en release)
                 * @note Assertion en debug : NKENTSEU_ASSERT(false) car accès hors bornes
                 * @note En release : retourne une référence vers un static T pour éviter crash
                 * @note Ne jamais appeler sur NkArray<T, 0> : utiliser Empty() pour vérifier
                 */
                Reference operator[](SizeType) NKENTSEU_NOEXCEPT {
                    NKENTSEU_ASSERT(false);
                    static T dummy;
                    return dummy;
                }

            // ====================================================================
            // MÉTHODES DE CAPACITÉ (CONSTANTES POUR N=0)
            // ====================================================================
            public:

                /**
                 * @brief Teste si le tableau est vide : toujours true pour N=0
                 * @return true (constante à la compilation)
                 * @note constexpr : évaluable à la compilation
                 * @note noexcept : retourne une constante booléenne
                 */
                NKENTSEU_CONSTEXPR bool Empty() const NKENTSEU_NOEXCEPT {
                    return true;
                }

                /**
                 * @brief Retourne la taille : toujours 0 pour cette spécialisation
                 * @return 0 (constante à la compilation)
                 * @note constexpr : évaluable à la compilation
                 * @note noexcept : retourne une constante
                 */
                NKENTSEU_CONSTEXPR SizeType Size() const NKENTSEU_NOEXCEPT {
                    return 0;
                }

                /**
                 * @brief Alias lowercase de Empty() pour compatibilité STL
                 * @return true (toujours vide)
                 */
                NKENTSEU_CONSTEXPR bool empty() const NKENTSEU_NOEXCEPT {
                    return true;
                }

                /**
                 * @brief Alias lowercase de Size() pour compatibilité STL
                 * @return 0 (taille fixe)
                 */
                NKENTSEU_CONSTEXPR SizeType size() const NKENTSEU_NOEXCEPT {
                    return 0;
                }

            // ====================================================================
            // OPÉRATIONS NO-OP POUR TABLEAU VIDE
            // ====================================================================
            public:

                /**
                 * @brief Fill() sur tableau vide : opération no-op
                 * @param value Valeur ignorée (aucun élément à remplir)
                 * @note Complexité O(1) : boucle avec condition N=0 non exécutée
                 */
                void Fill(const T&) {
                }

                /**
                 * @brief Alias lowercase de Fill() pour compatibilité STL
                 * @param value Valeur ignorée
                 */
                void fill(const T&) {
                }

                /**
                 * @brief Swap() sur tableau vide : opération no-op
                 * @param other Autre tableau vide à "échanger" (rien à faire)
                 * @note noexcept : aucune opération réelle
                 */
                void Swap(NkArray&) NKENTSEU_NOEXCEPT {
                }

                /**
                 * @brief Alias lowercase de Swap() pour compatibilité STL
                 * @param other Autre tableau vide
                 */
                void swap(NkArray&) NKENTSEU_NOEXCEPT {
                }
        };

        // ====================================================================
        // FONCTIONS NON-MEMBRES : SWAP ET COMPARAISONS
        // ====================================================================

        /**
         * @brief Fonction swap non-membre pour ADL (Argument-Dependent Lookup)
         * @tparam T Type des éléments du tableau
         * @tparam N Nombre d'éléments dans le tableau
         * @param lhs Premier tableau à échanger
         * @param rhs Second tableau à échanger
         * @note noexcept : délègue à la méthode membre Swap()
         * @note Permet l'usage idiomatique : using nkentseu::NkSwap; NkSwap(a, b);
         */
        template<typename T, usize N>
        void NkSwap(NkArray<T, N>& lhs, NkArray<T, N>& rhs) NKENTSEU_NOEXCEPT {
            lhs.Swap(rhs);
        }

        /**
         * @brief Opérateur d'égalité pour comparer deux tableaux élément par élément
         * @tparam T Type des éléments (doit supporter operator==)
         * @tparam N Nombre d'éléments dans les tableaux
         * @param lhs Premier tableau à comparer
         * @param rhs Second tableau à comparer
         * @return true si tous les éléments correspondants sont égaux, false sinon
         * @note Complexité O(N) : comparaison séquentielle avec short-circuit
         * @note Pour N=0 : retourne true (deux tableaux vides sont égaux)
         */
        template<typename T, usize N>
        bool operator==(const NkArray<T, N>& lhs, const NkArray<T, N>& rhs) {
            for (usize i = 0; i < N; ++i) {
                if (!(lhs[i] == rhs[i])) {
                    return false;
                }
            }
            return true;
        }

        /**
         * @brief Opérateur d'inégalité pour comparer deux tableaux
         * @tparam T Type des éléments
         * @tparam N Nombre d'éléments dans les tableaux
         * @param lhs Premier tableau à comparer
         * @param rhs Second tableau à comparer
         * @return true si les tableaux diffèrent sur au moins un élément
         * @note Implémenté via !(lhs == rhs) pour cohérence et maintenance
         */
        template<typename T, usize N>
        bool operator!=(const NkArray<T, N>& lhs, const NkArray<T, N>& rhs) {
            return !(lhs == rhs);
        }

    } // namespace nkentseu

#endif // NK_CONTAINERS_CACHEFRIENDLY_NKARRAY_H

// ============================================================================
// EXEMPLES D'UTILISATION ET PATTERNS COURANTS
// ============================================================================
/*
 * @section examples Exemples pratiques d'utilisation de NkArray
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 1 : Création et accès de base
 * --------------------------------------------------------------------------
 * @code
 * #include "NKContainers/CacheFriendly/NkArray.h"
 * #include <cstdio>
 * 
 * void exempleBase() {
 *     // Initialisation aggregate (C++11) : compatible { ... }
 *     nkentseu::NkArray<int, 5> arr = {1, 2, 3, 4, 5};
 *     
 *     // Accès via operator[] (rapide, assertion en debug)
 *     printf("arr[2] = %d\n", arr[2]);  // 3
 *     
 *     // Accès borné via At() (plus sûr en production)
 *     if (2 < arr.Size()) {
 *         printf("arr.At(2) = %d\n", arr.At(2));  // 3
 *     }
 *     
 *     // Accès aux extrémités
 *     printf("Front: %d, Back: %d\n", arr.Front(), arr.Back());  // 1, 5
 *     
 *     // Accès au pointeur brut pour interop C
 *     int* ptr = arr.Data();
 *     printf("Via pointeur: %d\n", ptr[0]);  // 1
 *     
 *     // Modification
 *     arr[0] = 10;
 *     printf("Nouveau front: %d\n", arr.Front());  // 10
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 2 : Itération et algorithmes génériques
 * --------------------------------------------------------------------------
 * @code
 * void exempleIteration() {
 *     nkentseu::NkArray<float, 4> values = {1.5f, 2.5f, 3.5f, 4.5f};
 *     
 *     // Range-based for (grâce à begin()/end())
 *     printf("Valeurs: ");
 *     for (float v : values) {
 *         printf("%.1f ", v);
 *     }
 *     // Sortie: 1.5 2.5 3.5 4.5
 *     printf("\n");
 *     
 *     // Itérateur inverse pour parcours arrière
 *     printf("Inverse: ");
 *     for (auto it = values.rbegin(); it != values.rend(); ++it) {
 *         printf("%.1f ", *it);
 *     }
 *     // Sortie: 4.5 3.5 2.5 1.5
 *     printf("\n");
 *     
 *     // Algorithme générique avec itérateurs (style STL)
 *     float sum = 0.0f;
 *     for (auto it = values.begin(); it != values.end(); ++it) {
 *         sum += *it;
 *     }
 *     printf("Somme: %.1f\n", sum);  // 12.0
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 3 : Remplissage et échange
 * --------------------------------------------------------------------------
 * @code
 * void exempleOperations() {
 *     nkentseu::NkArray<int, 10> buffer;
 *     
 *     // Remplissage avec une valeur initiale
 *     buffer.Fill(0);  // Tous les éléments à 0
 *     
 *     // Initialisation sélective
 *     for (usize i = 0; i < buffer.Size(); ++i) {
 *         buffer[i] = static_cast<int>(i * 10);
 *     }
 *     
 *     // Échange avec un autre tableau de même type/taille
 *     nkentseu::NkArray<int, 10> other;
 *     other.Fill(999);
 *     
 *     printf("Avant swap - buffer[0]: %d, other[0]: %d\n", 
 *            buffer[0], other[0]);  // 0, 999
 *     
 *     buffer.Swap(other);  // Échange élément par élément
 *     
 *     printf("Après swap - buffer[0]: %d, other[0]: %d\n", 
 *            buffer[0], other[0]);  // 999, 0
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 4 : Interopérabilité avec APIs C et optimisations
 * --------------------------------------------------------------------------
 * @code
 * // Fonction C attendant un pointeur et une taille
 * void ProcessCStyle(const int* data, usize count) {
 *     for (usize i = 0; i < count; ++i) {
 *         printf("%d ", data[i]);
 *     }
 *     printf("\n");
 * }
 * 
 * void exempleInteropC() {
 *     nkentseu::NkArray<int, 5> arr = {10, 20, 30, 40, 50};
 *     
 *     // Passage direct à une API C : zero-copy, zero-overhead
 *     ProcessCStyle(arr.Data(), arr.Size());
 *     // Sortie: 10 20 30 40 50
 *     
 *     // Modification via le pointeur C reflétée dans l'array C++
 *     int* mutablePtr = arr.Data();
 *     mutablePtr[0] = 999;
 *     printf("arr[0] après modif C: %d\n", arr[0]);  // 999
 * }
 * 
 * // Optimisation cache : parcours séquentiel pour prefetching CPU
 * void exempleCacheFriendly() {
 *     constexpr usize SIZE = 1024;
 *     nkentseu::NkArray<float, SIZE> data;
 *     
 *     // Initialisation
 *     for (usize i = 0; i < SIZE; ++i) {
 *         data[i] = static_cast<float>(i);
 *     }
 *     
 *     // Traitement cache-friendly : accès séquentiel
 *     float sum = 0.0f;
 *     for (float val : data) {  // Parcours contigu, prefetching optimal
 *         sum += val * val;
 *     }
 *     
 *     printf("Somme des carrés: %.2f\n", sum);
 *     // Beaucoup plus rapide qu'un accès aléatoire ou structure de données fragmentée
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 5 : Tableaux constexpr et métaprogrammation
 * --------------------------------------------------------------------------
 * @code
 * // Tableau constexpr pour calculs à la compilation
 * constexpr nkentseu::NkArray<int, 5> makeLookupTable() {
 *     nkentseu::NkArray<int, 5> table = {0, 1, 4, 9, 16};  // Carrés
 *     return table;
 * }
 * 
 * constexpr auto squares = makeLookupTable();
 * 
 * void exempleConstexpr() {
 *     // Accès constexpr si le compilateur le permet
 *     constexpr int val = squares[3];  // 9, évalué à la compilation
 *     printf("Carré de 3: %d\n", val);
 *     
 *     // Utilisation dans des expressions constexpr
 *     static_assert(squares.Size() == 5, "Taille attendue");
 *     static_assert(!squares.Empty(), "Non vide attendu");
 * }
 * 
 * // Template métaprogrammation avec NkArray
 * template<usize N>
 * void ProcessFixedArray(const nkentseu::NkArray<float, N>& arr) {
 *     // N connu à la compilation : optimisations possibles
 *     printf("Traitement de %zu éléments\n", N);
 *     
 *     // Boucle potentiellement unrollée par le compilateur
 *     for (usize i = 0; i < N; ++i) {
 *         // ... traitement de arr[i]
 *     }
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 6 : Cas spécial N=0 (tableau vide)
 * --------------------------------------------------------------------------
 * @code
 * void exempleTableauVide() {
 *     // Tableau de taille zéro : utile pour la généricité
 *     nkentseu::NkArray<int, 0> empty;
 *     
 *     // Vérifications de capacité
 *     printf("Vide: %s, Taille: %zu\n", 
 *            empty.Empty() ? "oui" : "non", empty.Size());  // oui, 0
 *     
 *     // Opérations no-op
 *     empty.Fill(42);  // Rien ne se passe
 *     
 *     nkentseu::NkArray<int, 0> other;
 *     empty.Swap(other);  // Échange de "rien"
 *     
 *     // Itération : boucle non exécutée
 *     for (int val : empty) {
 *         // Jamais exécuté
 *         printf("%d\n", val);
 *     }
 *     
 *     // ATTENTION: operator[] sur tableau vide -> assertion en debug
 *     // if (false) { int x = empty[0]; }  // Ne pas faire !
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * BONNES PRATIQUES ET RECOMMANDATIONS
 * --------------------------------------------------------------------------
 * 
 * 1. CHOIX DE LA TAILLE N :
 *    - Privilégier les tailles petites à moyennes (< 10KB) pour allocation stack
 *    - Pour les grands tableaux : envisager NkVector (heap) si la taille est dynamique
 *    - Utiliser constexpr pour N : permet des optimisations à la compilation
 *    - Documenter la sémantique de la taille fixe dans l'API
 * 
 * 2. ACCÈS ET SÉCURITÉ DES BORNES :
 *    - En debug : operator[] et At() ont des assertions : exploiter pour détecter les bugs
 *    - En release : operator[] est plus rapide (pas de vérification) : utiliser avec précaution
 *    - Pour les APIs publiques : préférer At() avec documentation claire des préconditions
 *    - Toujours vérifier Empty() ou comparer l'index avec Size() avant accès
 * 
 * 3. OPTIMISATIONS DE PERFORMANCE :
 *    - Parcours séquentiel : exploite le prefetching CPU, bien plus rapide qu'accès aléatoire
 *    - Alignement mémoire : NkArray est naturellement aligné sur alignof(T)
 *    - Pour les types triviaux : Fill() peut être optimisé en memset par le compilateur
 *    - constexpr : utiliser pour les tableaux de constantes : évaluation à la compilation
 * 
 * 4. INTEROPÉRABILITÉ :
 *    - Data() retourne un T* : compatible avec les APIs C attendent un tableau brut
 *    - Size() retourne usize : passer explicitement la taille aux fonctions C
 *    - Pattern courant : func(arr.Data(), arr.Size()) pour interop zero-copy
 * 
 * 5. GÉNÉRICITÉ ET TEMPLATES :
 *    - NkArray<T, N> est un type distinct pour chaque N : pas de polymorphisme à l'exécution
 *    - Pour des fonctions génériques sur N : utiliser template<usize N>
 *    - La spécialisation N=0 permet d'éviter des cas spéciaux dans le code générique
 * 
 * 6. LIMITATIONS CONNUES :
 *    - Taille fixe à la compilation : pas de redimensionnement à l'exécution
 *    - Allocation stack : risque de stack overflow pour les très grands N
 *    - Pas de méthodes de recherche (find, contains) : à ajouter si besoin fréquent
 *    - Pas d'opérateurs de comparaison lexicographique (<, <=, >, >=) : à ajouter si besoin
 * 
 * 7. EXTENSIONS RECOMMANDÉES :
 *    - Ajouter NkArray::FillWith(Func) pour initialisation par fonction/générateur
 *    - Implémenter operator<, <=, >, >= pour comparaisons lexicographiques
 *    - Ajouter une méthode Span() retournant une vue légère (si NkSpan existe dans le projet)
 *    - Supporter la construction depuis initializer_list avec vérification de taille
 *    - Ajouter des méthodes algorithmiques : Sum(), Max(), Min() pour types numériques
 * 
 * 8. COMPARAISON AVEC AUTRES STRUCTURES :
 *    - vs T[N] (C array) : NkArray offre interface moderne, itérateurs, sécurité en debug
 *    - vs NkVector<T> : NkArray = stack, taille fixe, zero overhead ; Vector = heap, dynamique
 *    - vs std::array : NkArray est équivalent mais sans dépendance STL, avec conventions NKEngine
 *    - Règle : utiliser NkArray quand la taille est connue à la compilation et fixe
 */

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Généré par Rihen le 2026-02-05 22:26:13
// Dernière modification : 2026-04-26 (restructuration et documentation)
// ============================================================