// -----------------------------------------------------------------------------
// FICHIER: NKContainers\Containers\Heterogeneous\NkPair.h
// DESCRIPTION: Conteneur de paire hétérogène - Alternative STL-free
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CONTAINERS_HETEROGENEOUS_NKPAIR_H
#define NK_CONTAINERS_HETEROGENEOUS_NKPAIR_H

    // ========================================================================
    // INCLUSIONS DES DÉPENDANCES
    // ========================================================================

    #include "NKCore/NkTypes.h"                    // Définition des types fondamentaux (usize, etc.)
    #include "NKContainers/NkContainersApi.h"      // Macros d'exportation de l'API conteneurs
    #include "NKCore/NkTraits.h"                   // Traits pour move/forward/decay et utilitaires méta

    // ========================================================================
    // ESPACE DE NOMS PRINCIPAL
    // ========================================================================

    namespace nkentseu {

        // ====================================================================
        // CLASSE PRINCIPALE : NK PAIR
        // ====================================================================

        /**
         * @brief Structure template représentant une paire de valeurs hétérogènes
         * 
         * Conteneur générique permettant de regrouper deux valeurs de types potentiellement
         * différents. Alternative complète à std::pair sans dépendance à la STL.
         * 
         * PROPRIÉTÉS :
         * - Stockage direct des deux membres (pas d'indirection)
         * - Support complet des sémantiques de copie et déplacement
         * - Comparaisons lexicographiques pour l'usage dans les conteneurs triés
         * - Compatibilité avec les conteneurs associatifs (NkMap, NkHashMap, etc.)
         * 
         * GARANTIES :
         * - Layout mémoire : [First: sizeof(T1)][Second: sizeof(T2)] (pas de padding excessif)
         * - Constructeurs constexpr pour évaluation à la compilation quand possible
         * - noexcept sur les opérations de swap et move quand les types sous-jacents le permettent
         * 
         * CAS D'USAGE TYPQUES :
         * - Retour multiple de fonctions : return NkMakePair(key, value);
         * - Stockage clé-valeur dans les conteneurs associatifs
         * - Agrégation temporaire de deux valeurs liées sémantiquement
         * - Itération sur des maps avec accès simultané à clé et valeur
         * 
         * @tparam T1 Type du premier élément de la paire (accessible via .First)
         * @tparam T2 Type du second élément de la paire (accessible via .Second)
         * 
         * @note Les deux types doivent être copiables ou déplaçables selon l'usage
         * @note Thread-safe pour les lectures concurrentes sur instances distinctes
         */
        template<typename T1, typename T2>
        struct NkPair {

            // ====================================================================
            // ALIASES DE TYPES POUR LA MÉTAPROGRAMMATION
            // ====================================================================

            using FirstType = T1;       ///< Alias du type du premier élément
            using SecondType = T2;      ///< Alias du type du second élément

            // ====================================================================
            // MEMBRES DONNÉES : STOCKAGE DES VALEURS
            // ====================================================================

            T1 First;                   ///< Premier élément de la paire (accès public)
            T2 Second;                  ///< Second élément de la paire (accès public)

            // ====================================================================
            // SECTION : CONSTRUCTEURS
            // ====================================================================

            /**
             * @brief Constructeur par défaut avec initialisation valeur des membres
             * @note Appelle les constructeurs par défaut de T1 et T2
             * @note constexpr : évaluable à la compilation pour les types triviaux
             */
            NKENTSEU_CONSTEXPR NkPair()
                : First()
                , Second() {
            }

            /**
             * @brief Constructeur paramétré avec copie des valeurs fournies
             * @param first Valeur à copier dans le premier membre
             * @param second Valeur à copier dans le second membre
             * @note constexpr : permet la construction constexpr pour les types compatibles
             */
            NKENTSEU_CONSTEXPR NkPair(const T1& first, const T2& second)
                : First(first)
                , Second(second) {
            }

            /**
             * @brief Constructeur de copie pour la sémantique de valeur
             * @param other Paire source à dupliquer membre par membre
             * @note constexpr : copie bitwise possible pour les types triviaux
             */
            NKENTSEU_CONSTEXPR NkPair(const NkPair& other)
                : First(other.First)
                , Second(other.Second) {
            }

            /**
             * @brief Constructeur de conversion depuis une paire de types compatibles
             * @tparam U1 Type du premier élément de la paire source
             * @tparam U2 Type du second élément de la paire source
             * @param other Paire source avec types convertibles vers T1 et T2
             * @note Permet les conversions implicites : NkPair<int, double> -> NkPair<float, float>
             */
            template<typename U1, typename U2>
            NKENTSEU_CONSTEXPR NkPair(const NkPair<U1, U2>& other)
                : First(other.First)
                , Second(other.Second) {
            }

            // ====================================================================
            // SUPPORT C++11 : MOVE SEMANTICS ET FORWARDING
            // ====================================================================
            #if defined(NK_CPP11)

                /**
                 * @brief Constructeur de déplacement pour transférer les ressources
                 * @param other Paire source dont les membres seront déplacés
                 * @note noexcept : garantie de non-levée d'exception pour les types compatibles
                 * @note Optimise les transferts pour les types lourds (NkString, NkVector, etc.)
                 */
                NKENTSEU_CONSTEXPR NkPair(NkPair&& other) NKENTSEU_NOEXCEPT
                    : First(traits::NkMove(other.First))
                    , Second(traits::NkMove(other.Second)) {
                }

                /**
                 * @brief Constructeur de déplacement avec conversion de types
                 * @tparam U1 Type convertible vers T1 pour le premier membre
                 * @tparam U2 Type convertible vers T2 pour le second membre
                 * @param other Paire source avec types déplaçables vers T1 et T2
                 * @note Utilise NkForward pour préserver les catégories de valeur
                 */
                template<typename U1, typename U2>
                NKENTSEU_CONSTEXPR NkPair(NkPair<U1, U2>&& other) NKENTSEU_NOEXCEPT
                    : First(traits::NkForward<U1>(other.First))
                    , Second(traits::NkForward<U2>(other.Second)) {
                }

                /**
                 * @brief Constructeur par forwarding parfait pour construction in-place
                 * @tparam U1 Type déduit du premier argument (avec référence collapsing)
                 * @tparam U2 Type déduit du second argument (avec référence collapsing)
                 * @param first Argument forwarded vers le constructeur de T1
                 * @param second Argument forwarded vers le constructeur de T2
                 * @note Évite toute copie/move temporaire : construction directe des membres
                 */
                template<typename U1, typename U2>
                NKENTSEU_CONSTEXPR NkPair(U1&& first, U2&& second)
                    : First(traits::NkForward<U1>(first))
                    , Second(traits::NkForward<U2>(second)) {
                }

            #endif // defined(NK_CPP11)

            // ====================================================================
            // SECTION : OPÉRATEURS D'AFFECTATION
            // ====================================================================

            /**
             * @brief Opérateur d'affectation par copie avec protection d'auto-affectation
             * @param other Paire source à copier
             * @return Référence vers l'instance courante pour le chaînage d'opérations
             * @note Vérification this != &other pour éviter la destruction prématurée
             */
            NkPair& operator=(const NkPair& other) {
                if (this != &other) {
                    First = other.First;
                    Second = other.Second;
                }
                return *this;
            }

            /**
             * @brief Opérateur d'affectation avec conversion de types
             * @tparam U1 Type du premier élément de la paire source
             * @tparam U2 Type du second élément de la paire source
             * @param other Paire source avec types assignables vers T1 et T2
             * @return Référence vers l'instance courante pour le chaînage
             * @note Pas de vérification d'auto-affectation : types différents garantis
             */
            template<typename U1, typename U2>
            NkPair& operator=(const NkPair<U1, U2>& other) {
                First = other.First;
                Second = other.Second;
                return *this;
            }

            // ====================================================================
            // SUPPORT C++11 : MOVE ASSIGNMENT
            // ====================================================================
            #if defined(NK_CPP11)

                /**
                 * @brief Opérateur d'affectation par déplacement
                 * @param other Paire source dont les membres seront déplacés
                 * @return Référence vers l'instance courante pour le chaînage
                 * @note noexcept : sécurisé pour les types avec move noexcept
                 */
                NkPair& operator=(NkPair&& other) NKENTSEU_NOEXCEPT {
                    First = traits::NkMove(other.First);
                    Second = traits::NkMove(other.Second);
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation par déplacement avec conversion
                 * @tparam U1 Type déplaçable vers T1 pour le premier membre
                 * @tparam U2 Type déplaçable vers T2 pour le second membre
                 * @param other Paire source avec types déplaçables
                 * @return Référence vers l'instance courante pour le chaînage
                 */
                template<typename U1, typename U2>
                NkPair& operator=(NkPair<U1, U2>&& other) NKENTSEU_NOEXCEPT {
                    First = traits::NkForward<U1>(other.First);
                    Second = traits::NkForward<U2>(other.Second);
                    return *this;
                }

            #endif // defined(NK_CPP11)

            // ====================================================================
            // SECTION : UTILITAIRES ET OPÉRATIONS
            // ====================================================================

            /**
             * @brief Échange les valeurs de cette paire avec une autre en temps constant
             * @param other Autre paire dont échanger le contenu
             * @note noexcept : délègue à traits::NkSwap pour chaque membre
             * @note Préférer à l'assignation pour les permutations efficaces
             */
            void Swap(NkPair& other) NKENTSEU_NOEXCEPT {
                traits::NkSwap(First, other.First);
                traits::NkSwap(Second, other.Second);
            }
        };

        // ====================================================================
        // OPÉRATEURS DE COMPARAISON (NON-MEMBRES)
        // ====================================================================

        /**
         * @brief Opérateur d'égalité : comparaison membre à membre
         * @tparam T1 Type du premier élément (doit supporter operator==)
         * @tparam T2 Type du second élément (doit supporter operator==)
         * @param lhs Paire de gauche pour la comparaison
         * @param rhs Paire de droite pour la comparaison
         * @return true si First et Second sont égaux respectivement, false sinon
         * @note constexpr : évaluable à la compilation pour les types triviaux
         */
        template<typename T1, typename T2>
        NKENTSEU_CONSTEXPR bool operator==(const NkPair<T1, T2>& lhs, const NkPair<T1, T2>& rhs) {
            return lhs.First == rhs.First && lhs.Second == rhs.Second;
        }

        /**
         * @brief Opérateur d'inégalité : négation de l'égalité
         * @tparam T1 Type du premier élément
         * @tparam T2 Type du second élément
         * @param lhs Paire de gauche pour la comparaison
         * @param rhs Paire de droite pour la comparaison
         * @return true si les paires diffèrent sur au moins un membre
         * @note Implémenté via !(lhs == rhs) pour cohérence et maintenance
         */
        template<typename T1, typename T2>
        NKENTSEU_CONSTEXPR bool operator!=(const NkPair<T1, T2>& lhs, const NkPair<T1, T2>& rhs) {
            return !(lhs == rhs);
        }

        /**
         * @brief Opérateur de comparaison inférieure : ordre lexicographique
         * @tparam T1 Type du premier élément (doit supporter operator<)
         * @tparam T2 Type du second élément (doit supporter operator<)
         * @param lhs Paire de gauche pour la comparaison
         * @param rhs Paire de droite pour la comparaison
         * @return true si lhs < rhs selon l'ordre lexicographique (First, puis Second)
         * @note Règle : lhs < rhs si (lhs.First < rhs.First) ou (First égaux et lhs.Second < rhs.Second)
         * @note Essentiel pour l'usage dans NkMap, NkSet et autres conteneurs triés
         */
        template<typename T1, typename T2>
        NKENTSEU_CONSTEXPR bool operator<(const NkPair<T1, T2>& lhs, const NkPair<T1, T2>& rhs) {
            return lhs.First < rhs.First || (!(rhs.First < lhs.First) && lhs.Second < rhs.Second);
        }

        /**
         * @brief Opérateur de comparaison inférieure ou égale
         * @tparam T1 Type du premier élément
         * @tparam T2 Type du second élément
         * @param lhs Paire de gauche pour la comparaison
         * @param rhs Paire de droite pour la comparaison
         * @return true si lhs <= rhs (négation de rhs < lhs)
         * @note Implémenté via !(rhs < lhs) pour cohérence avec l'ordre strict faible
         */
        template<typename T1, typename T2>
        NKENTSEU_CONSTEXPR bool operator<=(const NkPair<T1, T2>& lhs, const NkPair<T1, T2>& rhs) {
            return !(rhs < lhs);
        }

        /**
         * @brief Opérateur de comparaison supérieure
         * @tparam T1 Type du premier élément
         * @tparam T2 Type du second élément
         * @param lhs Paire de gauche pour la comparaison
         * @param rhs Paire de droite pour la comparaison
         * @return true si lhs > rhs (équivalent à rhs < lhs)
         * @note Implémenté via rhs < lhs pour éviter la duplication de logique
         */
        template<typename T1, typename T2>
        NKENTSEU_CONSTEXPR bool operator>(const NkPair<T1, T2>& lhs, const NkPair<T1, T2>& rhs) {
            return rhs < lhs;
        }

        /**
         * @brief Opérateur de comparaison supérieure ou égale
         * @tparam T1 Type du premier élément
         * @tparam T2 Type du second élément
         * @param lhs Paire de gauche pour la comparaison
         * @param rhs Paire de droite pour la comparaison
         * @return true si lhs >= rhs (négation de lhs < rhs)
         * @note Implémenté via !(lhs < rhs) pour cohérence avec l'ordre strict faible
         */
        template<typename T1, typename T2>
        NKENTSEU_CONSTEXPR bool operator>=(const NkPair<T1, T2>& lhs, const NkPair<T1, T2>& rhs) {
            return !(lhs < rhs);
        }

        // ====================================================================
        // FONCTIONS D'AIDE : CRÉATION ET MANIPULATION
        // ====================================================================

        /**
         * @brief Fonction factory avec déduction automatique des types
         * @tparam T1 Type déduit du premier argument
         * @tparam T2 Type déduit du second argument
         * @param first Valeur pour le premier membre de la paire
         * @param second Valeur pour le second membre de la paire
         * @return Nouvelle instance NkPair<T1, T2> initialisée avec les valeurs fournies
         * @note Évite la spécification explicite des templates : auto p = NkMakePair(1, 2.0);
         */
        template<typename T1, typename T2>
        NKENTSEU_CONSTEXPR NkPair<T1, T2> NkMakePair(const T1& first, const T2& second) {
            return NkPair<T1, T2>(first, second);
        }

        // ====================================================================
        // SUPPORT C++11 : MAKE_PAIR AVEC FORWARDING PARFAIT
        // ====================================================================
        #if defined(NK_CPP11)

            /**
             * @brief Fonction factory avec forwarding parfait et decay des types
             * @tparam T1 Type déduit (avec références) du premier argument
             * @tparam T2 Type déduit (avec références) du second argument
             * @param first Argument forwarded vers la construction du premier membre
             * @param second Argument forwarded vers la construction du second membre
             * @return NkPair avec types decayés (sans références ni cv-qualifiers)
             * @note traits::NkDecay_t retire les références et const/volatile pour le stockage
             * @note Permet NkMakePair(std::move(obj), literal) sans copies inutiles
             */
            template<typename T1, typename T2>
            NKENTSEU_CONSTEXPR NkPair<traits::NkDecay_t<T1>, traits::NkDecay_t<T2>>
            NkMakePair(T1&& first, T2&& second) {
                using FirstType = traits::NkDecay_t<T1>;
                using SecondType = traits::NkDecay_t<T2>;
                return NkPair<FirstType, SecondType>(
                    traits::NkForward<T1>(first),
                    traits::NkForward<T2>(second)
                );
            }

        #endif // defined(NK_CPP11)

        /**
         * @brief Fonction swap non-membre pour ADL (Argument-Dependent Lookup)
         * @tparam T1 Type du premier élément de la paire
         * @tparam T2 Type du second élément de la paire
         * @param lhs Première paire à échanger
         * @param rhs Seconde paire à échanger
         * @note noexcept : délègue à la méthode membre Swap()
         * @note Permet l'usage idiomatique : using std::swap; swap(a, b);
         */
        template<typename T1, typename T2>
        void NkSwap(NkPair<T1, T2>& lhs, NkPair<T1, T2>& rhs) NKENTSEU_NOEXCEPT {
            lhs.Swap(rhs);
        }

        // ====================================================================
        // ACCÈS AUX ÉLÉMENTS PAR NOM (FONCTIONS LIBRES)
        // ====================================================================

        /**
         * @brief Accès au premier élément via fonction libre (version non-const)
         * @tparam T1 Type du premier élément
         * @tparam T2 Type du second élément
         * @param p Référence vers la paire à accéder
         * @return Référence non-const vers le premier membre
         * @note noexcept : accès direct au membre, aucune logique complexe
         */
        template<typename T1, typename T2>
        NKENTSEU_CONSTEXPR T1& NkGetFirst(NkPair<T1, T2>& p) NKENTSEU_NOEXCEPT {
            return p.First;
        }

        /**
         * @brief Accès au premier élément via fonction libre (version const)
         * @tparam T1 Type du premier élément
         * @tparam T2 Type du second élément
         * @param p Référence const vers la paire à accéder
         * @return Référence const vers le premier membre
         * @note Permet l'accès en lecture seule sur les paires const
         */
        template<typename T1, typename T2>
        NKENTSEU_CONSTEXPR const T1& NkGetFirst(const NkPair<T1, T2>& p) NKENTSEU_NOEXCEPT {
            return p.First;
        }

        /**
         * @brief Accès au second élément via fonction libre (version non-const)
         * @tparam T1 Type du premier élément
         * @tparam T2 Type du second élément
         * @param p Référence vers la paire à accéder
         * @return Référence non-const vers le second membre
         * @note noexcept : accès direct au membre, aucune logique complexe
         */
        template<typename T1, typename T2>
        NKENTSEU_CONSTEXPR T2& NkGetSecond(NkPair<T1, T2>& p) NKENTSEU_NOEXCEPT {
            return p.Second;
        }

        /**
         * @brief Accès au second élément via fonction libre (version const)
         * @tparam T1 Type du premier élément
         * @tparam T2 Type du second élément
         * @param p Référence const vers la paire à accéder
         * @return Référence const vers le second membre
         * @note Permet l'accès en lecture seule sur les paires const
         */
        template<typename T1, typename T2>
        NKENTSEU_CONSTEXPR const T2& NkGetSecond(const NkPair<T1, T2>& p) NKENTSEU_NOEXCEPT {
            return p.Second;
        }

        // ====================================================================
        // SUPPORT C++11 : ACCÈS PAR INDEX (STYLE std::get)
        // ====================================================================
        #if defined(NK_CPP11)

            /**
             * @brief Structure de métaprogrammation pour l'accès par index à la compilation
             * @tparam Index Position de l'élément à accéder (0 pour First, 1 pour Second)
             * @tparam T1 Type du premier élément
             * @tparam T2 Type du second élément
             * @note Spécialisations partielles ci-dessous pour Index = 0 et Index = 1
             */
            template<usize Index, typename T1, typename T2>
            struct NkPairElement;

            /**
             * @brief Spécialisation pour l'index 0 : accès au premier élément
             * @tparam T1 Type du premier élément
             * @tparam T2 Type du second élément
             */
            template<typename T1, typename T2>
            struct NkPairElement<0, T1, T2> {

                using Type = T1;  ///< Alias du type de l'élément à l'index 0

                /**
                 * @brief Accès en écriture à l'élément d'index 0
                 * @param p Référence vers la paire cible
                 * @return Référence non-const vers First
                 */
                static NKENTSEU_CONSTEXPR T1& Get(NkPair<T1, T2>& p) NKENTSEU_NOEXCEPT {
                    return p.First;
                }

                /**
                 * @brief Accès en lecture à l'élément d'index 0
                 * @param p Référence const vers la paire cible
                 * @return Référence const vers First
                 */
                static NKENTSEU_CONSTEXPR const T1& Get(const NkPair<T1, T2>& p) NKENTSEU_NOEXCEPT {
                    return p.First;
                }
            };

            /**
             * @brief Spécialisation pour l'index 1 : accès au second élément
             * @tparam T1 Type du premier élément
             * @tparam T2 Type du second élément
             */
            template<typename T1, typename T2>
            struct NkPairElement<1, T1, T2> {

                using Type = T2;  ///< Alias du type de l'élément à l'index 1

                /**
                 * @brief Accès en écriture à l'élément d'index 1
                 * @param p Référence vers la paire cible
                 * @return Référence non-const vers Second
                 */
                static NKENTSEU_CONSTEXPR T2& Get(NkPair<T1, T2>& p) NKENTSEU_NOEXCEPT {
                    return p.Second;
                }

                /**
                 * @brief Accès en lecture à l'élément d'index 1
                 * @param p Référence const vers la paire cible
                 * @return Référence const vers Second
                 */
                static NKENTSEU_CONSTEXPR const T2& Get(const NkPair<T1, T2>& p) NKENTSEU_NOEXCEPT {
                    return p.Second;
                }
            };

            /**
             * @brief Accès générique par index à la compilation (version non-const)
             * @tparam Index Position de l'élément (0 ou 1)
             * @tparam T1 Type du premier élément
             * @tparam T2 Type du second élément
             * @param p Référence vers la paire cible
             * @return Référence vers l'élément à l'index spécifié
             * @note static_assert : erreur de compilation si Index >= 2
             */
            template<usize Index, typename T1, typename T2>
            NKENTSEU_CONSTEXPR typename NkPairElement<Index, T1, T2>::Type&
            NkGet(NkPair<T1, T2>& p) NKENTSEU_NOEXCEPT {
                static_assert(Index < 2, "Index out of bounds for NkPair");
                return NkPairElement<Index, T1, T2>::Get(p);
            }

            /**
             * @brief Accès générique par index à la compilation (version const)
             * @tparam Index Position de l'élément (0 ou 1)
             * @tparam T1 Type du premier élément
             * @tparam T2 Type du second élément
             * @param p Référence const vers la paire cible
             * @return Référence const vers l'élément à l'index spécifié
             * @note static_assert : erreur de compilation si Index >= 2
             */
            template<usize Index, typename T1, typename T2>
            NKENTSEU_CONSTEXPR const typename NkPairElement<Index, T1, T2>::Type&
            NkGet(const NkPair<T1, T2>& p) NKENTSEU_NOEXCEPT {
                static_assert(Index < 2, "Index out of bounds for NkPair");
                return NkPairElement<Index, T1, T2>::Get(p);
            }

            /**
             * @brief Accès générique par index avec rvalue reference (pour move)
             * @tparam Index Position de l'élément (0 ou 1)
             * @tparam T1 Type du premier élément
             * @tparam T2 Type du second élément
             * @param p Rvalue reference vers la paire cible
             * @return Rvalue reference vers l'élément à l'index spécifié
             * @note Permet traits::NkMove(NkGet<0>(std::move(pair))) pour extraction efficace
             */
            template<usize Index, typename T1, typename T2>
            NKENTSEU_CONSTEXPR typename NkPairElement<Index, T1, T2>::Type&&
            NkGet(NkPair<T1, T2>&& p) NKENTSEU_NOEXCEPT {
                static_assert(Index < 2, "Index out of bounds for NkPair");
                return traits::NkMove(NkPairElement<Index, T1, T2>::Get(p));
            }

        #endif // defined(NK_CPP11)

    } // namespace nkentseu

#endif // NK_CONTAINERS_HETEROGENEOUS_NKPAIR_H

// ============================================================================
// EXEMPLES D'UTILISATION ET PATTERNS COURANTS
// ============================================================================
/*
 * @section examples Exemples pratiques d'utilisation de NkPair
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 1 : Création et accès basique
 * --------------------------------------------------------------------------
 * @code
 * #include "NKContainers/Heterogeneous/NkPair.h"
 * #include <cstdio>
 * 
 * void exempleBase() {
 *     // Construction directe avec types explicites
 *     nkentseu::NkPair<int, float> p1(42, 3.14f);
 *     
 *     // Accès aux membres via les champs publics
 *     int key = p1.First;           // 42
 *     float value = p1.Second;      // 3.14f
 *     
 *     // Construction avec NkMakePair (déduction automatique des types)
 *     auto p2 = nkentseu::NkMakePair(100, 2.718);
 *     // p2 est de type NkPair<int, double>
 *     
 *     // Modification des valeurs
 *     p1.First = 99;
 *     p1.Second = 1.414f;
 *     
 *     printf("p1: (%d, %f)\n", p1.First, p1.Second);  // (99, 1.414000)
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 2 : Comparaisons et usage dans les conteneurs triés
 * --------------------------------------------------------------------------
 * @code
 * void exempleComparaisons() {
 *     nkentseu::NkPair<int, int> a(1, 2);
 *     nkentseu::NkPair<int, int> b(1, 3);
 *     nkentseu::NkPair<int, int> c(2, 1);
 *     
 *     // Comparaisons lexicographiques : d'abord First, puis Second
 *     bool test1 = (a < b);  // true : First égaux, 2 < 3
 *     bool test2 = (a < c);  // true : 1 < 2 (First comparé en premier)
 *     bool test3 = (b == a); // false : Seconds différents
 *     
 *     // Usage dans un conteneur trié (ex: NkSet<NkPair<int,int>>)
 *     // L'ordre sera : (1,2) < (1,3) < (2,1) < (2,2) < ...
 *     
 *     printf("a < b: %s\n", test1 ? "vrai" : "faux");  // vrai
 *     printf("a < c: %s\n", test2 ? "vrai" : "faux");  // vrai
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 3 : Retour multiple de fonctions (pattern courant)
 * --------------------------------------------------------------------------
 * @code
 * // Fonction retournant un statut et une valeur
 * nkentseu::NkPair<bool, int> Diviser(int dividende, int diviseur) {
 *     if (diviseur == 0) {
 *         return nkentseu::NkMakePair(false, 0);  // Échec
 *     }
 *     return nkentseu::NkMakePair(true, dividende / diviseur);  // Succès
 * }
 * 
 * void exempleRetourMultiple() {
 *     auto [ok, result] = Diviser(10, 2);  // C++17 structured bindings (si supporté)
 *     // Alternative compatible C++11 :
 *     nkentseu::NkPair<bool, int> res = Diviser(10, 2);
 *     bool ok = res.First;
 *     int result = res.Second;
 *     
 *     if (ok) {
 *         printf("Résultat: %d\n", result);  // 5
 *     }
 *     
 *     // Gestion d'erreur
 *     auto err = Diviser(10, 0);
 *     if (!err.First) {
 *         printf("Erreur: division par zéro\n");
 *     }
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 4 : Itération sur un conteneur associatif (style Map)
 * --------------------------------------------------------------------------
 * @code
 * // Simulation d'itération sur une map avec paires clé-valeur
 * void exempleIteration() {
 *     // Tableau de paires simulant une map
 *     nkentseu::NkPair<const char*, int> entries[] = {
 *         nkentseu::NkMakePair("pommes", 3),
 *         nkentseu::NkMakePair("bananes", 5),
 *         nkentseu::NkMakePair("cerises", 2)
 *     };
 *     
 *     // Parcours avec accès explicite à clé et valeur
 *     for (usize i = 0; i < 3; ++i) {
 *         const char* key = entries[i].First;
 *         int value = entries[i].Second;
 *         printf("%s: %d\n", key, value);
 *     }
 *     
 *     // Alternative avec fonctions d'accès libres
 *     for (usize i = 0; i < 3; ++i) {
 *         printf("%s: %d\n", 
 *                nkentseu::NkGetFirst(entries[i]),
 *                nkentseu::NkGetSecond(entries[i]));
 *     }
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 5 : Move semantics et optimisations C++11
 * --------------------------------------------------------------------------
 * @code
 * #if defined(NK_CPP11)
 * 
 * #include "NKCore/NkString.h"  // Type lourd avec gestion mémoire interne
 * 
 * void exempleMoveSemantics() {
 *     // Construction avec forwarding parfait : évite les copies temporaires
 *     nkentseu::NkString heavy1("Données volumineuses 1");
 *     nkentseu::NkString heavy2("Données volumineuses 2");
 *     
 *     // Emplacement direct dans la paire sans copie intermédiaire
 *     auto pair = nkentseu::NkMakePair(
 *         nkentseu::traits::NkMove(heavy1),  // heavy1 est déplacé, pas copié
 *         nkentseu::traits::NkMove(heavy2)   // heavy2 est déplacé, pas copié
 *     );
 *     
 *     // Après le move, heavy1 et heavy2 sont dans un état valide mais indéterminé
 *     // La paire possède maintenant les ressources mémoire
 *     
 *     // Extraction par move pour transfert ultérieur
 *     nkentseu::NkString extracted = nkentseu::traits::NkMove(
 *         nkentseu::NkGet<1>(nkentseu::traits::NkMove(pair))  // Accès par index + move
 *     );
 *     
 *     printf("Extrait: %s\n", extracted.CStr());
 * }
 * 
 * #endif // NK_CPP11
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 6 : Conversion entre types de paires compatibles
 * --------------------------------------------------------------------------
 * @code
 * void exempleConversion() {
 *     // Paire avec types précis
 *     nkentseu::NkPair<short, float> precise(100, 1.5f);
 *     
 *     // Conversion implicite vers types plus larges
 *     nkentseu::NkPair<int, double> widened = precise;
 *     // widened.First == 100 (short -> int)
 *     // widened.Second == 1.5 (float -> double)
 *     
 *     // Affectation avec conversion
 *     nkentseu::NkPair<long, long double> extended;
 *     extended = precise;  // short->long, float->long double
 *     
 *     // Utile pour les APIs génériques acceptant différents niveaux de précision
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * BONNES PRATIQUES ET RECOMMANDATIONS
 * --------------------------------------------------------------------------
 * 
 * 1. CHOIX DES TYPES T1 ET T2 :
 *    - Privilégier les types trivialement copiables pour les performances
 *    - Pour les types lourds : utiliser NkMakePair avec traits::NkMove()
 *    - Éviter les références comme types de paire (préférer les pointeurs si besoin)
 * 
 * 2. ACCÈS AUX MEMBRES :
 *    - Accès direct via .First / .Second : plus lisible et performant
 *    - Fonctions NkGetFirst/NkGetSecond : utiles pour la généricité (templates)
 *    - NkGet<Index>() : pour le code générique style std::get (C++11+)
 * 
 * 3. COMPARAISONS ET TRI :
 *    - L'ordre lexicographique est cohérent avec std::pair : First prioritaire
 *    - Pour un ordre personnalisé : envelopper dans un type avec operator< custom
 *    - Attention aux types flottants : comparer avec tolérance si nécessaire
 * 
 * 4. GESTION MÉMOIRE :
 *    - La paire stocke ses membres par valeur : pas d'allocation dynamique interne
 *    - Pour les grands objets : stocker des pointeurs ou utiliser move semantics
 *    - Swap() est O(1) pour les types triviaux, O(n) pour les conteneurs internes
 * 
 * 5. COMPATIBILITÉ ET PORTABILITÉ :
 *    - NKENTSEU_CONSTEXPR permet l'évaluation compile-time quand possible
 *    - NKENTSEU_NOEXCEPT optimise les chemins sans exception pour les types compatibles
 *    - Les blocs #if defined(NK_CPP11) garantissent la compatibilité C++98/03
 * 
 * 6. EXTENSIONS RECOMMANDÉES :
 *    - NkTuple pour N > 2 éléments (généralisation de NkPair)
 *    - NkApply/NkVisit pour appliquer une fonction aux deux membres
 *    - Support des structured bindings via spécialisation de traits (C++17)
 */

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Généré par Rihen le 2026-02-09
// Dernière modification : 2026-04-26 (restructuration et documentation)
// ============================================================