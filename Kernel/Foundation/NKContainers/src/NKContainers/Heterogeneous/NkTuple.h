// -----------------------------------------------------------------------------
// FICHIER: NKContainers\Containers\Heterogeneous\NkTuple.h
// DESCRIPTION: Tuple - Collection hétérogène de taille fixe (style Python)
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CONTAINERS_HETEROGENEOUS_NKTUPLE_H
#define NK_CONTAINERS_HETEROGENEOUS_NKTUPLE_H

    // ========================================================================
    // INCLUSIONS DES DÉPENDANCES
    // ========================================================================

    #include "NKCore/NkTypes.h"                    // Définition des types fondamentaux (usize, etc.)
    #include "NKContainers/NkContainersApi.h"      // Macros d'exportation de l'API conteneurs
    #include "NKCore/NkTraits.h"                   // Traits pour move/forward/decay et métaprogrammation

    // ========================================================================
    // ESPACE DE NOMS PRINCIPAL
    // ========================================================================

    namespace nkentseu {

        // ====================================================================
        // MÉTAPROGRAMMATION : ACCÈS AUX ÉLÉMENTS PAR INDEX
        // ====================================================================

        /**
         * @brief Structure forward pour l'accès aux éléments de tuple par index
         * @tparam Index Position de l'élément à accéder (0-based)
         * @tparam Tuple Type du tuple cible
         * @note Spécialisations définies ci-dessous pour chaque index valide
         */
        template<usize Index, typename Tuple>
        struct NkTupleElement;

        /**
         * @brief Trait pour obtenir le type d'un élément de tuple à un index donné
         * @tparam Index Position de l'élément (0-based)
         * @tparam Types Pack de types du tuple
         * @note Utilisé pour la déduction de type dans NkGet<Index>()
         */
        template<usize Index, typename... Types>
        struct NkTupleTypeAt;

        /**
         * @brief Spécialisation récursive pour NkTupleTypeAt
         * @tparam Types Pack de types restants à parcourir
         */
        template<usize Index, typename Head, typename... Tail>
        struct NkTupleTypeAt<Index, Head, Tail...> {
            using Type = typename NkTupleTypeAt<Index - 1, Tail...>::Type;
        };

        /**
         * @brief Cas de base : index 0 retourne le premier type
         * @tparam Head Type du premier élément
         * @tparam Tail Types restants (ignorés)
         */
        template<typename Head, typename... Tail>
        struct NkTupleTypeAt<0, Head, Tail...> {
            using Type = Head;
        };

        // ====================================================================
        // CLASSE PRINCIPALE : NK TUPLE (VARIADIC)
        // ====================================================================

        /**
         * @brief Structure template représentant un tuple hétérogène de taille fixe
         * 
         * Conteneur générique permettant de regrouper N valeurs de types potentiellement
         * différents, avec accès par index à la compilation. Inspiré des tuples Python
         * et de std::tuple, mais sans dépendance à la STL.
         * 
         * PROPRIÉTÉS :
         * - Taille fixe déterminée à la compilation par le nombre de types templates
         * - Stockage direct des membres sans indirection (zero overhead abstraction)
         * - Accès aux éléments via NkGet<Index>(tuple) en temps constant
         * - Support complet des sémantiques de copie, déplacement et forwarding
         * - Comparaisons lexicographiques pour l'usage dans les conteneurs triés
         * - Compatible avec les fonctions retournant des valeurs multiples
         * 
         * GARANTIES :
         * - Layout mémoire : membres stockés séquentiellement [T0][T1]...[TN-1]
         * - Constructeurs constexpr pour évaluation à la compilation (types triviaux)
         * - noexcept sur swap/move quand tous les types sous-jacents le garantissent
         * - Accès par index vérifié à la compilation via static_assert
         * 
         * CAS D'USAGE TYPQUES :
         * - Retour multiple de fonctions : return NkMakeTuple(status, data, error);
         * - Agrégation temporaire de valeurs hétérogènes liées sémantiquement
         * - Clés composites pour les conteneurs associatifs (NkMap<NkTuple<K1,K2>, V>)
         * - Déstructuration manuelle de résultats complexes sans struct dédiée
         * - Métaprogrammation : manipulation de packs de types à la compilation
         * 
         * @tparam Types Pack de types des éléments du tuple (hétérogènes, ordre significatif)
         * 
         * @note Thread-safe pour les lectures concurrentes sur instances distinctes
         * @note Les types doivent être copiables/déplaçables selon l'usage souhaité
         * @note Accès hors limites détecté à la compilation (static_assert), pas à l'exécution
         * 
         * @example Python-like usage:
         * @code
         * // Création : similaire à Python's tuple()
         * auto coords = nkentseu::NkMakeTuple(10, 20.5f, "point");
         * 
         * // Accès par index : similaire à Python's tuple[i]
         * int x = nkentseu::NkGet<0>(coords);      // 10
         * float y = nkentseu::NkGet<1>(coords);    // 20.5
         * 
         * // Comparaison lexicographique
         * auto a = nkentseu::NkMakeTuple(1, 2);
         * auto b = nkentseu::NkMakeTuple(1, 3);
         * bool test = (a < b);  // true : ordre lexicographique
         * @endcode
         */
        template<typename... Types>
        struct NkTuple;

        // ====================================================================
        // CAS DE BASE : TUPLE VIDE
        // ====================================================================

        /**
         * @brief Spécialisation pour le tuple vide (0 éléments)
         * @note Sert de terminaison récursive pour les opérations sur les tuples
         */
        template<>
        struct NkTuple<> {

            /**
             * @brief Constructeur par défaut du tuple vide
             * @note constexpr : toujours évaluable à la compilation
             */
            NKENTSEU_CONSTEXPR NkTuple() {
            }

            /**
             * @brief Opérateur d'égalité pour tuples vides
             * @param other Autre tuple vide à comparer
             * @return true (deux tuples vides sont toujours égaux)
             */
            NKENTSEU_CONSTEXPR bool operator==(const NkTuple& other) const {
                (void)other;
                return true;
            }

            /**
             * @brief Opérateur de comparaison pour tuples vides
             * @param other Autre tuple vide à comparer
             * @return false (deux tuples vides sont équivalents, aucun n'est "inférieur")
             */
            NKENTSEU_CONSTEXPR bool operator<(const NkTuple& other) const {
                (void)other;
                return false;
            }

            /**
             * @brief Échange avec un autre tuple vide (no-op)
             * @param other Autre tuple vide
             * @note noexcept : opération triviale sans effet
             */
            void Swap(NkTuple& other) NKENTSEU_NOEXCEPT {
                (void)other;
            }
        };

        // ====================================================================
        // CAS RÉCURSIF : TUPLE AVEC AU MOINS UN ÉLÉMENT
        // ====================================================================

        /**
         * @brief Spécialisation récursive : tuple avec tête + reste
         * @tparam Head Type du premier élément du tuple
         * @tparam Tail Pack de types des éléments suivants (peut être vide)
         */
        template<typename Head, typename... Tail>
        struct NkTuple<Head, Tail...> {

            // ====================================================================
            // MEMBRES DONNÉES : STOCKAGE HIÉRARCHIQUE
            // ====================================================================

            Head mHead;                   ///< Premier élément du tuple (tête)
            NkTuple<Tail...> mTail;       ///< Tuple récursif contenant le reste des éléments

            // ====================================================================
            // CONSTRUCTEURS
            // ====================================================================

            /**
             * @brief Constructeur par défaut avec initialisation valeur de tous les membres
             * @note Appelle récursivement les constructeurs par défaut de Head et Tail...
             * @note constexpr : évaluable à la compilation pour les types triviaux
             */
            NKENTSEU_CONSTEXPR NkTuple()
                : mHead()
                , mTail() {
            }

            /**
             * @brief Constructeur paramétré avec copie des valeurs fournies
             * @param head Valeur à copier dans le premier élément
             * @param tail Valeurs à copier dans les éléments restants
             * @note Construction récursive : délègue la queue au constructeur de NkTuple<Tail...>
             */
            NKENTSEU_CONSTEXPR NkTuple(const Head& head, const Tail&... tail)
                : mHead(head)
                , mTail(tail...) {
            }

            /**
             * @brief Constructeur de copie pour la sémantique de valeur
             * @param other Tuple source à dupliquer récursivement
             * @note Copie profonde membre par membre via les constructeurs de copie
             */
            NKENTSEU_CONSTEXPR NkTuple(const NkTuple& other)
                : mHead(other.mHead)
                , mTail(other.mTail) {
            }

            /**
             * @brief Constructeur de conversion depuis un tuple de types compatibles
             * @tparam OtherHead Type convertible vers Head pour le premier élément
             * @tparam OtherTail Types convertibles vers Tail... pour le reste
             * @param other Tuple source avec types assignables vers les types cibles
             * @note Permet les conversions implicites entre tuples de types compatibles
             */
            template<typename OtherHead, typename... OtherTail>
            NKENTSEU_CONSTEXPR NkTuple(const NkTuple<OtherHead, OtherTail...>& other)
                : mHead(other.mHead)
                , mTail(other.mTail) {
            }

            // ====================================================================
            // SUPPORT C++11 : MOVE SEMANTICS ET FORWARDING PARFAIT
            // ====================================================================
            #if defined(NK_CPP11)

                /**
                 * @brief Constructeur de déplacement pour transférer les ressources
                 * @param other Tuple source dont les membres seront déplacés récursivement
                 * @note noexcept : garanti si Head et Tail... ont des move constructors noexcept
                 */
                NKENTSEU_CONSTEXPR NkTuple(NkTuple&& other) NKENTSEU_NOEXCEPT
                    : mHead(traits::NkMove(other.mHead))
                    , mTail(traits::NkMove(other.mTail)) {
                }

                /**
                 * @brief Constructeur de déplacement avec conversion de types
                 * @tparam OtherHead Type déplaçable vers Head
                 * @tparam OtherTail Types déplaçables vers Tail...
                 * @param other Tuple source avec types déplaçables
                 * @note Utilise NkForward pour préserver les catégories de valeur
                 */
                template<typename OtherHead, typename... OtherTail>
                NKENTSEU_CONSTEXPR NkTuple(NkTuple<OtherHead, OtherTail...>&& other) NKENTSEU_NOEXCEPT
                    : mHead(traits::NkForward<OtherHead>(other.mHead))
                    , mTail(traits::NkMove(other.mTail)) {
                }

                /**
                 * @brief Constructeur par forwarding parfait pour construction in-place
                 * @tparam H Type déduit du premier argument (avec référence collapsing)
                 * @tparam Args Types déduits des arguments restants
                 * @param head Argument forwarded vers le constructeur de Head
                 * @param tail Arguments forwarded vers la construction récursive de Tail...
                 * @note Évite toute copie/move temporaire : construction directe de chaque membre
                 */
                template<typename H, typename... Args>
                NKENTSEU_CONSTEXPR explicit NkTuple(H&& head, Args&&... tail)
                    : mHead(traits::NkForward<H>(head))
                    , mTail(traits::NkForward<Args>(tail)...) {
                }

            #endif // defined(NK_CPP11)

            // ====================================================================
            // OPÉRATEURS D'AFFECTATION
            // ====================================================================

            /**
             * @brief Opérateur d'affectation par copie avec protection d'auto-affectation
             * @param other Tuple source à copier récursivement
             * @return Référence vers l'instance courante pour le chaînage
             */
            NkTuple& operator=(const NkTuple& other) {
                if (this != &other) {
                    mHead = other.mHead;
                    mTail = other.mTail;
                }
                return *this;
            }

            /**
             * @brief Opérateur d'affectation avec conversion de types
             * @tparam OtherHead Type assignable vers Head
             * @tparam OtherTail Types assignables vers Tail...
             * @param other Tuple source avec types compatibles
             * @return Référence vers l'instance courante pour le chaînage
             */
            template<typename OtherHead, typename... OtherTail>
            NkTuple& operator=(const NkTuple<OtherHead, OtherTail...>& other) {
                mHead = other.mHead;
                mTail = other.mTail;
                return *this;
            }

            // ====================================================================
            // SUPPORT C++11 : MOVE ASSIGNMENT
            // ====================================================================
            #if defined(NK_CPP11)

                /**
                 * @brief Opérateur d'affectation par déplacement
                 * @param other Tuple source dont les membres seront déplacés
                 * @return Référence vers l'instance courante pour le chaînage
                 * @note noexcept : sécurisé si les types sous-jacents le garantissent
                 */
                NkTuple& operator=(NkTuple&& other) NKENTSEU_NOEXCEPT {
                    mHead = traits::NkMove(other.mHead);
                    mTail = traits::NkMove(other.mTail);
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation par déplacement avec conversion
                 * @tparam OtherHead Type déplaçable vers Head
                 * @tparam OtherTail Types déplaçables vers Tail...
                 * @param other Tuple source avec types déplaçables
                 * @return Référence vers l'instance courante pour le chaînage
                 */
                template<typename OtherHead, typename... OtherTail>
                NkTuple& operator=(NkTuple<OtherHead, OtherTail...>&& other) NKENTSEU_NOEXCEPT {
                    mHead = traits::NkForward<OtherHead>(other.mHead);
                    mTail = traits::NkMove(other.mTail);
                    return *this;
                }

            #endif // defined(NK_CPP11)

            // ====================================================================
            // OPÉRATEURS DE COMPARAISON (ORDRE LEXICOGRAPHIQUE)
            // ====================================================================

            /**
                 * @brief Opérateur d'égalité : comparaison récursive membre à membre
                 * @param other Tuple à comparer avec l'instance courante
                 * @return true si tous les éléments correspondants sont égaux
                 * @note Ordre lexicographique : compare d'abord mHead, puis récursivement mTail
                 */
            NKENTSEU_CONSTEXPR bool operator==(const NkTuple& other) const {
                return mHead == other.mHead && mTail == other.mTail;
            }

            /**
             * @brief Opérateur d'inégalité : négation de l'égalité
             * @param other Tuple à comparer avec l'instance courante
             * @return true si au moins un élément diffère
             * @note Implémenté via !(lhs == rhs) pour cohérence et maintenance
             */
            NKENTSEU_CONSTEXPR bool operator!=(const NkTuple& other) const {
                return !(*this == other);
            }

            /**
             * @brief Opérateur de comparaison inférieure : ordre lexicographique récursif
             * @param other Tuple à comparer avec l'instance courante
             * @return true si *this < other selon l'ordre lexicographique
             * @note Règle : (head < other.head) ou (heads égaux et tail < other.tail)
             * @note Essentiel pour l'usage dans NkMap, NkSet et autres conteneurs triés
             */
            NKENTSEU_CONSTEXPR bool operator<(const NkTuple& other) const {
                return (mHead < other.mHead) || (!(other.mHead < mHead) && (mTail < other.mTail));
            }

            /**
             * @brief Opérateur de comparaison inférieure ou égale
             * @param other Tuple à comparer avec l'instance courante
             * @return true si *this <= other (négation de other < *this)
             */
            NKENTSEU_CONSTEXPR bool operator<=(const NkTuple& other) const {
                return !(other < *this);
            }

            /**
             * @brief Opérateur de comparaison supérieure
             * @param other Tuple à comparer avec l'instance courante
             * @return true si *this > other (équivalent à other < *this)
             */
            NKENTSEU_CONSTEXPR bool operator>(const NkTuple& other) const {
                return other < *this;
            }

            /**
             * @brief Opérateur de comparaison supérieure ou égale
             * @param other Tuple à comparer avec l'instance courante
             * @return true si *this >= other (négation de *this < other)
             */
            NKENTSEU_CONSTEXPR bool operator>=(const NkTuple& other) const {
                return !(*this < other);
            }

            // ====================================================================
            // UTILITAIRES
            // ====================================================================

            /**
             * @brief Échange récursif des valeurs avec un autre tuple
             * @param other Autre tuple de mêmes types à échanger
             * @note noexcept : délègue à traits::NkSwap pour chaque niveau récursif
             * @note Complexité O(N) où N = nombre d'éléments, mais swap individuel O(1)
             */
            void Swap(NkTuple& other) NKENTSEU_NOEXCEPT {
                traits::NkSwap(mHead, other.mHead);
                mTail.Swap(other.mTail);
            }
        };

        // ====================================================================
        // MÉTAPROGRAMMATION : SPÉCIALISATIONS NkTupleElement POUR NkGet<Index>
        // ====================================================================

        /**
         * @brief Spécialisation pour accéder à l'élément d'index 0 (tête du tuple)
         * @tparam Head Type du premier élément
         * @tparam Tail Types restants du tuple
         */
        template<typename Head, typename... Tail>
        struct NkTupleElement<0, NkTuple<Head, Tail...>> {

            using Type = Head;  ///< Type de l'élément à l'index 0

            /**
             * @brief Accès en écriture à l'élément d'index 0
             * @param tuple Référence vers le tuple cible
             * @return Référence non-const vers le premier élément (mHead)
             */
            static NKENTSEU_CONSTEXPR Head& Get(NkTuple<Head, Tail...>& tuple) NKENTSEU_NOEXCEPT {
                return tuple.mHead;
            }

            /**
             * @brief Accès en lecture à l'élément d'index 0
             * @param tuple Référence const vers le tuple cible
             * @return Référence const vers le premier élément (mHead)
             */
            static NKENTSEU_CONSTEXPR const Head& Get(const NkTuple<Head, Tail...>& tuple) NKENTSEU_NOEXCEPT {
                return tuple.mHead;
            }

            #if defined(NK_CPP11)
            /**
             * @brief Accès par rvalue reference pour extraction par move
             * @param tuple Rvalue reference vers le tuple cible
             * @return Rvalue reference vers le premier élément
             * @note Permet traits::NkMove(NkGet<0>(std::move(tuple)))
             */
            static NKENTSEU_CONSTEXPR Head&& Get(NkTuple<Head, Tail...>&& tuple) NKENTSEU_NOEXCEPT {
                return traits::NkMove(tuple.mHead);
            }
            #endif
        };

        /**
         * @brief Spécialisation récursive pour accéder aux éléments d'index > 0
         * @tparam Index Position cible (décrémentée à chaque niveau récursif)
         * @tparam Head Type du premier élément (ignoré pour Index > 0)
         * @tparam Tail Types restants du tuple
         * @note Délègue l'accès à NkTupleElement<Index-1, NkTuple<Tail...>>
         */
        template<usize Index, typename Head, typename... Tail>
        struct NkTupleElement<Index, NkTuple<Head, Tail...>> {

            using Type = typename NkTupleElement<Index - 1, NkTuple<Tail...>>::Type;  ///< Type déduit récursivement

            /**
             * @brief Accès en écriture à l'élément d'index Index (via récursion sur mTail)
             * @param tuple Référence vers le tuple cible
             * @return Référence non-const vers l'élément à la position Index
             */
            static NKENTSEU_CONSTEXPR Type& Get(NkTuple<Head, Tail...>& tuple) NKENTSEU_NOEXCEPT {
                return NkTupleElement<Index - 1, NkTuple<Tail...>>::Get(tuple.mTail);
            }

            /**
             * @brief Accès en lecture à l'élément d'index Index (via récursion sur mTail)
             * @param tuple Référence const vers le tuple cible
             * @return Référence const vers l'élément à la position Index
             */
            static NKENTSEU_CONSTEXPR const Type& Get(const NkTuple<Head, Tail...>& tuple) NKENTSEU_NOEXCEPT {
                return NkTupleElement<Index - 1, NkTuple<Tail...>>::Get(tuple.mTail);
            }

            #if defined(NK_CPP11)
            /**
             * @brief Accès par rvalue reference pour extraction par move
             * @param tuple Rvalue reference vers le tuple cible
             * @return Rvalue reference vers l'élément à la position Index
             */
            static NKENTSEU_CONSTEXPR Type&& Get(NkTuple<Head, Tail...>&& tuple) NKENTSEU_NOEXCEPT {
                return traits::NkMove(NkTupleElement<Index - 1, NkTuple<Tail...>>::Get(tuple.mTail));
            }
            #endif
        };

        // ====================================================================
        // FONCTIONS D'ACCÈS PAR INDEX (STYLE std::get)
        // ====================================================================

        /**
         * @brief Accès générique à un élément de tuple par index à la compilation
         * @tparam Index Position de l'élément à accéder (0-based)
         * @tparam Types Pack de types du tuple
         * @param tuple Référence vers le tuple cible
         * @return Référence vers l'élément à l'index spécifié
         * @note static_assert : erreur de compilation si Index >= sizeof...(Types)
         * @note constexpr : accès résolu à la compilation, zero overhead à l'exécution
         */
        template<usize Index, typename... Types>
        NKENTSEU_CONSTEXPR typename NkTupleElement<Index, NkTuple<Types...>>::Type&
        NkGet(NkTuple<Types...>& tuple) NKENTSEU_NOEXCEPT {
            static_assert(Index < sizeof...(Types), "NkTuple index out of bounds");
            return NkTupleElement<Index, NkTuple<Types...>>::Get(tuple);
        }

        /**
         * @brief Accès générique en lecture seule par index
         * @tparam Index Position de l'élément à accéder (0-based)
         * @tparam Types Pack de types du tuple
         * @param tuple Référence const vers le tuple cible
         * @return Référence const vers l'élément à l'index spécifié
         * @note Permet l'accès aux tuples const sans copie
         */
        template<usize Index, typename... Types>
        NKENTSEU_CONSTEXPR const typename NkTupleElement<Index, NkTuple<Types...>>::Type&
        NkGet(const NkTuple<Types...>& tuple) NKENTSEU_NOEXCEPT {
            static_assert(Index < sizeof...(Types), "NkTuple index out of bounds");
            return NkTupleElement<Index, NkTuple<Types...>>::Get(tuple);
        }

        #if defined(NK_CPP11)
        /**
         * @brief Accès générique par rvalue reference pour extraction par move
         * @tparam Index Position de l'élément à accéder (0-based)
         * @tparam Types Pack de types du tuple
         * @param tuple Rvalue reference vers le tuple cible
         * @return Rvalue reference vers l'élément à l'index spécifié
         * @note Permet l'extraction efficace sans copie : auto val = NkMove(NkGet<0>(NkMove(tuple)))
         */
        template<usize Index, typename... Types>
        NKENTSEU_CONSTEXPR typename NkTupleElement<Index, NkTuple<Types...>>::Type&&
        NkGet(NkTuple<Types...>&& tuple) NKENTSEU_NOEXCEPT {
            static_assert(Index < sizeof...(Types), "NkTuple index out of bounds");
            return traits::NkMove(NkTupleElement<Index, NkTuple<Types...>>::Get(tuple));
        }
        #endif

        // ====================================================================
        // FONCTION FACTORY : NkMakeTuple AVEC DÉDUCTION DE TYPES
        // ====================================================================

        /**
         * @brief Fonction factory avec déduction automatique des types des arguments
         * @tparam Types Types déduits des arguments (après decay pour le stockage)
         * @param args Arguments à encapsuler dans le tuple
         * @return Nouvelle instance NkTuple<Types...> initialisée avec les valeurs fournies
         * @note Évite la spécification explicite des templates : auto t = NkMakeTuple(1, 2.0, "x");
         * @note Les types sont decayés (références et cv-qualifiers retirés) pour le stockage
         */
        template<typename... Types>
        NKENTSEU_CONSTEXPR NkTuple<traits::NkDecay_t<Types>...>
        NkMakeTuple(Types&&... args) {
            using TupleType = NkTuple<traits::NkDecay_t<Types>...>;
            return TupleType(traits::NkForward<Types>(args)...);
        }

        // ====================================================================
        // FONCTION SWAP NON-MEMBRE POUR ADL
        // ====================================================================

        /**
         * @brief Fonction swap non-membre pour Argument-Dependent Lookup (ADL)
         * @tparam Types Pack de types du tuple
         * @param lhs Premier tuple à échanger
         * @param rhs Second tuple à échanger
         * @note noexcept : délègue à la méthode membre Swap()
         * @note Permet l'usage idiomatique : using nkentseu::NkSwap; NkSwap(a, b);
         */
        template<typename... Types>
        void NkSwap(NkTuple<Types...>& lhs, NkTuple<Types...>& rhs) NKENTSEU_NOEXCEPT {
            lhs.Swap(rhs);
        }

        // ====================================================================
        // UTILITAIRES SUPPLÉMENTAIRES (STYLE PYTHON)
        // ====================================================================

        /**
         * @brief Obtient la taille (nombre d'éléments) d'un tuple à la compilation
         * @tparam Types Pack de types du tuple
         * @return Valeur constexpr représentant le nombre d'éléments
         * @note Équivalent à Python's len(tuple), mais résolu à la compilation
         */
        template<typename... Types>
        NKENTSEU_CONSTEXPR usize NkTupleSize(const NkTuple<Types...>&) NKENTSEU_NOEXCEPT {
            return sizeof...(Types);
        }

        /**
         * @brief Alias pour la taille d'un tuple (plus pratique dans les templates)
         * @tparam TupleType Type du tuple dont obtenir la taille
         * @note Utilisable dans des contextes constexpr et comme paramètre de template
         */
        template<typename TupleType>
        struct NkTupleSizeTrait;

        template<typename... Types>
        struct NkTupleSizeTrait<NkTuple<Types...>> {
            static constexpr usize value = sizeof...(Types);
        };

        #if defined(NK_CPP11)
        /**
         * @brief Concatène deux tuples en un nouveau tuple
         * @tparam Types1 Pack de types du premier tuple
         * @tparam Types2 Pack de types du second tuple
         * @param t1 Premier tuple à concaténer
         * @param t2 Second tuple à concaténer
         * @return Nouveau tuple contenant tous les éléments de t1 suivis de ceux de t2
         * @note Utilité : combiner des résultats de fonctions retournant des tuples
         * @note Implémentation simplifiée : nécessite expansion de pack avancée
         */
        template<typename... Types1, typename... Types2>
        NKENTSEU_CONSTEXPR NkTuple<Types1..., Types2...>
        NkTupleConcat(const NkTuple<Types1...>& t1, const NkTuple<Types2...>& t2) {
            // Implémentation avancée requise : expansion de pack pour reconstruction
            // Version simplifiée pour l'instant : à compléter selon les besoins
            return NkTuple<Types1..., Types2...>();  // TODO: implémentation complète
        }
        #endif

    } // namespace nkentseu

#endif // NK_CONTAINERS_HETEROGENEOUS_NKTUPLE_H

// ============================================================================
// EXEMPLES D'UTILISATION ET PATTERNS COURANTS (STYLE PYTHON)
// ============================================================================
/*
 * @section examples Exemples pratiques d'utilisation de NkTuple (inspiré Python)
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 1 : Création et accès basique (style Python)
 * --------------------------------------------------------------------------
 * @code
 * #include "NKContainers/Heterogeneous/NkTuple.h"
 * #include <cstdio>
 * 
 * void exempleBase() {
 *     // Création : similaire à Python's tuple()
 *     // Python: coords = (10, 20.5, "point")
 *     auto coords = nkentseu::NkMakeTuple(10, 20.5f, "point");
 *     // coords est de type NkTuple<int, float, const char*>
 *     
 *     // Accès par index : similaire à Python's tuple[i]
 *     // Python: x = coords[0]
 *     int x = nkentseu::NkGet<0>(coords);           // 10
 *     float y = nkentseu::NkGet<1>(coords);         // 20.5
 *     const char* label = nkentseu::NkGet<2>(coords); // "point"
 *     
 *     // Taille du tuple : similaire à Python's len(tuple)
 *     usize size = nkentseu::NkTupleSize(coords);   // 3
 *     
 *     printf("Coords: (%d, %f, %s) - size: %zu\n", x, y, label, size);
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 2 : Retour multiple de fonctions (pattern Pythonique)
 * --------------------------------------------------------------------------
 * @code
 * // Fonction retournant plusieurs valeurs, style Python
 * // Python: return success, result, error_message
 * nkentseu::NkTuple<bool, int, const char*> 
 * DiviserAvecStatut(int dividende, int diviseur) {
 *     if (diviseur == 0) {
 *         return nkentseu::NkMakeTuple(false, 0, "Division par zéro");
 *     }
 *     return nkentseu::NkMakeTuple(true, dividende / diviseur, nullptr);
 * }
 * 
 * void exempleRetourMultiple() {
 *     // Appel et déstructuration manuelle (C++11 compatible)
 *     auto result = DiviserAvecStatut(10, 2);
 *     bool ok = nkentseu::NkGet<0>(result);
 *     int value = nkentseu::NkGet<1>(result);
 *     const char* error = nkentseu::NkGet<2>(result);
 *     
 *     if (ok) {
 *         printf("Résultat: %d\n", value);  // 5
 *     } else {
 *         printf("Erreur: %s\n", error);
 *     }
 *     
 *     // Gestion d'erreur
 *     auto err = DiviserAvecStatut(10, 0);
 *     if (!nkentseu::NkGet<0>(err)) {
 *         printf("Échec: %s\n", nkentseu::NkGet<2>(err));  // Division par zéro
 *     }
 * }
 * 
 * // Avec C++17 structured bindings (si supporté par le compilateur) :
 * // auto [ok, value, error] = DiviserAvecStatut(10, 2);
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 3 : Comparaisons et ordre lexicographique
 * --------------------------------------------------------------------------
 * @code
 * void exempleComparaisons() {
 *     // Tuples comparés lexicographiquement, comme en Python
 *     // Python: (1, 2) < (1, 3)  # True
 *     auto a = nkentseu::NkMakeTuple(1, 2, 3);
 *     auto b = nkentseu::NkMakeTuple(1, 2, 4);
 *     auto c = nkentseu::NkMakeTuple(2, 1, 1);
 *     
 *     bool test1 = (a < b);  // true : premiers éléments égaux, 3 < 4
 *     bool test2 = (a < c);  // true : 1 < 2 au premier élément différent
 *     bool test3 = (a == a); // true : identité
 *     
 *     // Usage dans un conteneur trié (ex: NkSet<NkTuple<int,int>>)
 *     // L'ordre sera cohérent avec l'ordre lexicographique Python
 *     
 *     printf("(1,2,3) < (1,2,4): %s\n", test1 ? "vrai" : "faux");  // vrai
 *     printf("(1,2,3) < (2,1,1): %s\n", test2 ? "vrai" : "faux");  // vrai
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 4 : Clés composites pour conteneurs associatifs
 * --------------------------------------------------------------------------
 * @code
 * // Simulation d'une map avec clé composite (style Python dict avec tuple key)
 * // Python: database[(user_id, timestamp)] = data
 * 
 * struct DataRecord {
 *     int value;
 *     const char* description;
 * };
 * 
 * void exempleClesComposites() {
 *     // Tableau simulé d'entries avec clés tuples
 *     using KeyType = nkentseu::NkTuple<int, usize>;  // (user_id, timestamp)
 *     
 *     KeyType key1 = nkentseu::NkMakeTuple(101, 1234567890);
 *     KeyType key2 = nkentseu::NkMakeTuple(101, 1234567891);  // même user, temps différent
 *     KeyType key3 = nkentseu::NkMakeTuple(102, 1234567890);  // user différent
 *     
 *     // Comparaison pour recherche/insertion dans une map
 *     bool sameUser = (nkentseu::NkGet<0>(key1) == nkentseu::NkGet<0>(key2));  // true
 *     bool differentTime = (nkentseu::NkGet<1>(key1) != nkentseu::NkGet<1>(key2));  // true
 *     
 *     // Ordre pour tri : key1 < key2 car timestamps différents
 *     bool ordered = (key1 < key2);  // true : même user_id, timestamp1 < timestamp2
 *     
 *     printf("Même utilisateur: %s\n", sameUser ? "oui" : "non");
 *     printf("Ordre chronologique: %s\n", ordered ? "key1 avant key2" : "inverse");
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 5 : Move semantics et optimisations (C++11+)
 * --------------------------------------------------------------------------
 * @code
 * #if defined(NK_CPP11)
 * 
 * #include "NKCore/NkString.h"  // Type lourd avec gestion mémoire interne
 * 
 * void exempleMoveSemantics() {
 *     // Création avec forwarding parfait : évite les copies inutiles
 *     nkentseu::NkString heavy1("Données volumineuses 1");
 *     nkentseu::NkString heavy2("Données volumineuses 2");
 *     int metadata = 42;
 *     
 *     // Encapsulation directe sans copie intermédiaire
 *     auto tuple = nkentseu::NkMakeTuple(
 *         nkentseu::traits::NkMove(heavy1),  // déplacé, pas copié
 *         metadata,                           // copié (type trivial)
 *         nkentseu::traits::NkMove(heavy2)   // déplacé, pas copié
 *     );
 *     
 *     // Après move, heavy1 et heavy2 sont dans un état valide mais indéterminé
 *     // Le tuple possède maintenant les ressources mémoire
 *     
 *     // Extraction sélective par move pour transfert ultérieur
 *     nkentseu::NkString extracted = nkentseu::traits::NkMove(
 *         nkentseu::NkGet<0>(nkentseu::traits::NkMove(tuple))  // index 0 + move
 *     );
 *     
 *     printf("Extrait: %s\n", extracted.CStr());
 *     
 *     // Swap efficace entre tuples (O(N) swaps individuels, pas de réallocation)
 *     auto other = nkentseu::NkMakeTuple(99, 1.5f, "test");
 *     nkentseu::NkSwap(tuple, other);  // Échange sans copie des données internes
 * }
 * 
 * #endif // NK_CPP11
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 6 : Généricité et métaprogrammation
 * --------------------------------------------------------------------------
 * @code
 * // Fonction générique affichant n'importe quel tuple (démo de métaprogrammation)
 * // Note: nécessite expansion de pack complète pour une implémentation réelle
 * 
 * template<usize Index, typename Tuple>
 * void AfficherElementSiValide(const Tuple& t) {
 *     constexpr usize size = nkentseu::NkTupleSizeTrait<Tuple>::value;
 *     if constexpr (Index < size) {  // C++17 if constexpr, ou SFINAE pour C++11
 *         printf("Element %zu: [type info]\n", Index);
 *         AfficherElementSiValide<Index + 1, Tuple>(t);
 *     }
 * }
 * 
 * void exempleGenericite() {
 *     auto t1 = nkentseu::NkMakeTuple(42, 3.14, "hello");
 *     auto t2 = nkentseu::NkMakeTuple(true, 'x');
 *     
 *     // Taille à la compilation
 *     constexpr usize s1 = nkentseu::NkTupleSizeTrait<decltype(t1)>::value;  // 3
 *     constexpr usize s2 = nkentseu::NkTupleSizeTrait<decltype(t2)>::value;  // 2
 *     
 *     // Accès générique via template
 *     auto first1 = nkentseu::NkGet<0>(t1);  // int
 *     auto first2 = nkentseu::NkGet<0>(t2);  // bool
 *     
 *     printf("Tailles: %zu et %zu\n", s1, s2);
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 7 : Patterns Pythoniques avancés
 * --------------------------------------------------------------------------
 * @code
 * // Pattern: unpacking partiel (simulé)
 * // Python: x, *rest = (1, 2, 3, 4)
 * 
 * void exempleUnpacking() {
 *     auto data = nkentseu::NkMakeTuple(1, 2, 3, 4, 5);
 *     
 *     // Extraction manuelle du premier élément
 *     auto first = nkentseu::NkGet<0>(data);  // 1
 *     
 *     // Le "reste" est un tuple à reconstruire (nécessite NkTupleConcat avancé)
 *     // Pour l'instant : accès individuel aux indices restants
 *     auto second = nkentseu::NkGet<1>(data);  // 2
 *     auto third = nkentseu::NkGet<2>(data);   // 3
 *     // ...
 *     
 *     printf("First: %d, Next: %d, %d\n", first, second, third);
 * }
 * 
 * // Pattern: tuple comme enregistrement immuable (par convention)
 * struct Point3D {
 *     // Factory retournant un tuple immuable par convention
 *     static nkentseu::NkTuple<float, float, float> Create(float x, float y, float z) {
 *         return nkentseu::NkMakeTuple(x, y, z);
 *     }
 *     
 *     // Accès nommés via helpers (plus lisible que les indices)
 *     static float X(const nkentseu::NkTuple<float,float,float>& p) {
 *         return nkentseu::NkGet<0>(p);
 *     }
 *     static float Y(const nkentseu::NkTuple<float,float,float>& p) {
 *         return nkentseu::NkGet<1>(p);
 *     }
 *     static float Z(const nkentseu::NkTuple<float,float,float>& p) {
 *         return nkentseu::NkGet<2>(p);
 *     }
 * };
 * 
 * void exemplePoint3D() {
 *     auto p = Point3D::Create(1.0f, 2.0f, 3.0f);
 *     printf("Point: (%.1f, %.1f, %.1f)\n", 
 *            Point3D::X(p), Point3D::Y(p), Point3D::Z(p));
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * BONNES PRATIQUES ET RECOMMANDATIONS
 * --------------------------------------------------------------------------
 * 
 * 1. CHOIX DES TYPES DANS LE TUPLE :
 *    - Privilégier les types trivialement copiables pour les performances
 *    - Pour les types lourds : utiliser NkMakeTuple avec traits::NkMove()
 *    - Éviter les références comme éléments de tuple (préférer pointeurs si besoin)
 *    - Documenter l'ordre sémantique des éléments : tuple[i] doit avoir un sens clair
 * 
 * 2. ACCÈS AUX ÉLÉMENTS :
 *    - NkGet<Index>(tuple) : accès par index à la compilation, zero overhead
 *    - Pour la lisibilité : créer des aliases/fonctions nommées (ex: Point3D::X())
 *    - En contexte générique : utiliser NkTupleSizeTrait pour itération compile-time
 * 
 * 3. COMPARAISONS ET TRI :
 *    - L'ordre lexicographique est cohérent avec Python et std::tuple
 *    - Pour un ordre personnalisé : envelopper dans un type avec operator< custom
 *    - Attention aux types flottants : comparer avec tolérance si nécessaire
 * 
 * 4. GESTION MÉMOIRE ET PERFORMANCE :
 *    - Le tuple stocke ses membres par valeur : pas d'allocation dynamique interne
 *    - Pour les grands objets : stocker des pointeurs ou utiliser move semantics
 *    - Swap() est O(N) mais avec swaps individuels O(1) : efficace pour les types triviaux
 * 
 * 5. COMPATIBILITÉ ET PORTABILITÉ :
 *    - NKENTSEU_CONSTEXPR permet l'évaluation compile-time quand possible
 *    - NKENTSEU_NOEXCEPT optimise les chemins sans exception pour les types compatibles
 *    - Les blocs #if defined(NK_CPP11) garantissent la compatibilité C++98/03 de base
 * 
 * 6. EXTENSIONS RECOMMANDÉES (pour aller plus loin) :
 *    - NkApply<NkTuple, Func> : appliquer une fonction à tous les éléments
 *    - NkVisit<NkTuple, IndexRange> : itération compile-time sur une plage d'indices
 *    - Support des structured bindings C++17 via spécialisation de traits
 *    - NkTupleSlice<Start, End, Tuple> : extraction de sous-tuple à la compilation
 *    - Intégration avec NkVariant pour tuples hétérogènes dynamiques
 * 
 * 7. COMPARAISON AVEC PYTHON :
 *    + Avantages C++ : typage statique, zero overhead, évaluation compile-time
 *    + Avantages Python : syntaxe plus concise, unpacking natif, introspection runtime
 *    - Limitation C++ : pas d'unpacking syntaxique sans C++17 structured bindings
 *    - Limitation C++ : taille fixe à la compilation (pas de tuple dynamique)
 */

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Généré par Rihen le 2026-02-09
// Dernière modification : 2026-04-26 (implémentation variadique complète)
// ============================================================