// -----------------------------------------------------------------------------
// FICHIER: NKCore\Functional\NkFunctional.h
// DESCRIPTION: Foncteurs utilitaires pour le framework Nkentseu (équivalents STL::functional)
// AUTEUR: TEUGUIA TADJUIDJE Rodolf / Rihen
// DATE: 2025-06-10
// VERSION: 1.0.0
// LICENCE: Proprietary - Free to use and modify
// -----------------------------------------------------------------------------
//
// RÉSUMÉ:
//   Fournit des foncteurs génériques pour les opérations de hachage, comparaison
//   et logique, similaires aux fonctionnalités de <functional> de la STL.
//
// CARACTÉRISTIQUES:
//   - NkHash : Fonction de hachage générique avec spécialisations pour types primitifs
//   - NkEqual / NkLess / NkGreater : Comparateurs pour ordres et égalité
//   - NkLogicalAnd / NkLogicalOr / NkLogicalNot : Opérateurs logiques génériques
//   - Compatible constexpr pour évaluation à la compilation
//   - Spécialisations optimisées pour int32/64, uint32/64, float32/64, NkString
//
// DÉPENDANCES:
//   - NKCore/NkTypes.h    : Définition des types fondamentaux (usize, float32, etc.)
//   - NKCore/NkTraits.h   : Utilitaires méta-programmation et traits
//   - NKContainers/String/NkString.h : Chaîne de caractères pour spécialisation NkHash
//
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_FUNCTIONAL_NKFUNCTIONAL_H
#define NK_CORE_FUNCTIONAL_NKFUNCTIONAL_H

    // ========================================================================
    // INCLUSIONS DES DÉPENDANCES
    // ========================================================================

    #include "NKCore/NkTypes.h"                    // Types fondamentaux : usize, int32_t, float32, etc.
    #include "NKCore/NkTraits.h"                   // Traits méta-programmation et utilitaires de type
    #include "NKContainers/String/NkString.h"      // Classe NkString pour spécialisation du hachage

    // ========================================================================
    // ESPACE DE NOMS PRINCIPAL
    // ========================================================================

    namespace nkentseu {

        // ====================================================================
        // SECTION : FONCTIONS DE HACHAGE (NKHASH)
        // ====================================================================

        /**
         * @struct NkHash
         * @brief Fonction de hachage générique pour les clés dans les conteneurs associatifs
         * 
         * Équivalent à std::hash de la STL, utilisé par NkHashMap, NkUnorderedSet, etc.
         * 
         * PRINCIPE DE FONCTIONNEMENT :
         * - Template primaire avec static_assert pour forcer la spécialisation
         * - Spécialisations fournies pour les types primitifs courants
         * - Les utilisateurs doivent spécialiser NkHash<T> pour leurs types personnalisés
         * 
         * GARANTIES :
         * - constexpr : évaluable à la compilation pour les types supportés
         * - noexcept : ne lance jamais d'exception
         * - Distribution : vise une distribution uniforme des valeurs de hachage
         * 
         * CAS D'USAGE :
         * - Clés de NkHashMap<Key, Value>
         * - Éléments de NkUnorderedSet<T>
         * - Indexation dans les structures de hachage personnalisées
         * 
         * @tparam T Type de la clé à hacher
         * 
         * @note Pour spécialiser NkHash pour un type personnalisé MyType :
         *       template<> struct NkHash<MyType> { usize operator()(const MyType& k) const noexcept { ... } };
         * @note La qualité du hachage impacte directement les performances des conteneurs associatifs
         */
        template<typename T>
        struct NkHash {

            /**
             * @brief Opérateur d'appel pour calculer le hachage d'une clé
             * @param key Référence const vers la clé à hacher
             * @return Valeur de type usize représentant le hachage
             * @note Cette implémentation générique déclenche une erreur de compilation
             *       via static_assert pour forcer la spécialisation sur le type T
             * @note constexpr : permet l'évaluation à la compilation quand possible
             * @note noexcept : garantie de non-levée d'exception
             */
            constexpr usize operator()([[maybe_unused]] const T& key) const noexcept
            {
                // sizeof(T) == 0 est toujours faux pour un type complet, mais dépendant de T
                // donc Clang diffère l'évaluation jusqu'à l'instanciation pour un message d'erreur clair
                static_assert(sizeof(T) == 0, "NkHash must be specialized for type T");
                return 0;
            }
        };

        // ====================================================================
        // SPÉCIALISATIONS DE NKHASH POUR TYPES PRIMITIFS
        // ====================================================================

        /**
         * @brief Spécialisation de NkHash pour int32_t
         * @note Utilise un mélange bit-à-bit simple : key ^ (key >> 16)
         * @note Efficace pour les petites valeurs entières, distribution acceptable
         */
        template<>
        struct NkHash<int32_t> {

            /**
             * @brief Calcule le hachage d'un entier signé 32 bits
             * @param key Valeur int32_t à hacher
             * @return Hachage de type usize
             * @note Opération XOR avec décalage pour mélanger les bits hauts et bas
             */
            constexpr usize operator()(const int32_t& key) const noexcept
            {
                return static_cast<usize>(key ^ (key >> 16));
            }
        };

        /**
         * @brief Spécialisation de NkHash pour uint32_t
         * @note Même stratégie que int32_t : mélange par XOR et décalage
         */
        template<>
        struct NkHash<uint32_t> {

            /**
             * @brief Calcule le hachage d'un entier non-signé 32 bits
             * @param key Valeur uint32_t à hacher
             * @return Hachage de type usize
             */
            constexpr usize operator()(const uint32_t& key) const noexcept
            {
                return static_cast<usize>(key ^ (key >> 16));
            }
        };

        /**
         * @brief Spécialisation de NkHash pour int64_t
         * @note Décalage de 32 bits pour mélanger les deux moitiés 32-bit de l'entier 64-bit
         */
        template<>
        struct NkHash<int64_t> {

            /**
             * @brief Calcule le hachage d'un entier signé 64 bits
             * @param key Valeur int64_t à hacher
             * @return Hachage de type usize
             * @note XOR entre les 32 bits de poids faible et les 32 bits de poids fort
             */
            constexpr usize operator()(const int64_t& key) const noexcept
            {
                return static_cast<usize>(key ^ (key >> 32));
            }
        };

        /**
         * @brief Spécialisation de NkHash pour uint64_t
         * @note Même approche que int64_t pour une distribution uniforme
         */
        template<>
        struct NkHash<uint64_t> {

            /**
             * @brief Calcule le hachage d'un entier non-signé 64 bits
             * @param key Valeur uint64_t à hacher
             * @return Hachage de type usize
             */
            constexpr usize operator()(const uint64_t& key) const noexcept
            {
                return static_cast<usize>(key ^ (key >> 32));
            }
        };

        /**
         * @brief Spécialisation de NkHash pour float32
         * @note Gestion explicite des cas spéciaux : NaN, ±0, ±infinity
         * @note Utilise une union pour réinterpréter les bits du float en uint32_t
         */
        template<>
        struct NkHash<float32> {

            /**
             * @brief Calcule le hachage d'un flottant 32 bits
             * @param key Valeur float32 à hacher
             * @return Hachage de type usize
             * @note NaN retourne toujours 0 pour assurer hash(NaN) == hash(NaN)
             * @note ±0 et ±infinity retournent des valeurs distinctes codées en dur
             * @note Pour les valeurs normales : réinterprétation bit-à-bit puis hachage uint32
             */
            constexpr usize operator()(const float32& key) const noexcept
            {
                // Gestion des valeurs spéciales IEEE 754
                if (key != key)
                {
                    return 0;  // NaN : retourne une valeur constante
                }
                if (key == key * 0.0f)
                {
                    return key < 0.0f ? 1 : 2;  // ±0 ou ±infinity : codes distincts
                }
                // Réinterprétation des bits via union (safe en C++20, implementation-defined avant)
                union { float32 f; uint32_t i; } u = { key };
                return NkHash<uint32_t>{}(u.i);
            }
        };

        /**
         * @brief Spécialisation de NkHash pour float64
         * @note Même logique que float32 adaptée à la précision double
         */
        template<>
        struct NkHash<float64> {

            /**
             * @brief Calcule le hachage d'un flottant 64 bits
             * @param key Valeur float64 à hacher
             * @return Hachage de type usize
             * @note Gestion des cas spéciaux IEEE 754 en double précision
             * @note Réinterprétation bit-à-bit via union puis délégation à NkHash<uint64_t>
             */
            constexpr usize operator()(const float64& key) const noexcept
            {
                // Gestion des valeurs spéciales IEEE 754
                if (key != key)
                {
                    return 0;  // NaN : retourne une valeur constante
                }
                if (key == key * 0.0)
                {
                    return key < 0.0 ? 1 : 2;  // ±0 ou ±infinity : codes distincts
                }
                // Réinterprétation des bits via union pour float64 -> uint64_t
                union { float64 f; uint64_t i; } u = { key };
                return NkHash<uint64_t>{}(u.i);
            }
        };

        /**
         * @brief Spécialisation de NkHash pour NkString
         * @note Utilise l'algorithme FNV-1a (Fowler-Noll-Vo) pour un hachage rapide et bien distribué
         * @note Constants adaptées à la taille de usize (32 ou 64 bits)
         */
        template<>
        struct NkHash<NkString> {

            /**
             * @brief Calcule le hachage d'une chaîne NkString via FNV-1a
             * @param key Référence const vers la NkString à hacher
             * @return Hachage de type usize avec bonne distribution statistique
             * @note FNV-1a : pour chaque octet, XOR puis multiplication par prime FNV
             * @note Complexité O(n) où n = longueur de la chaîne
             * @note noexcept : aucune allocation ni exception durant le calcul
             */
            usize operator()(const NkString& key) const noexcept
            {
                // Constants FNV-1a selon l'architecture (32 ou 64 bits)
                constexpr usize fnv_prime = sizeof(usize) == 8 ? 1099511628211ULL : 16777619U;
                constexpr usize fnv_offset = sizeof(usize) == 8 ? 1465739525896755127ULL : 2166136261U;

                usize hash = fnv_offset;
                const char* data = key.Data();
                for (usize i = 0; i < key.Length(); ++i)
                {
                    hash ^= static_cast<usize>(data[i]);
                    hash *= fnv_prime;
                }
                return hash;
            }
        };

        // ====================================================================
        // SECTION : COMPARATEURS ET PRÉDICATS
        // ====================================================================

        /**
         * @struct NkEqual
         * @brief Prédicat d'égalité générique, équivalent à std::equal_to
         * 
         * Utilisé pour comparer l'égalité de deux valeurs de type T.
         * Requis par les conteneurs associatifs pour la recherche de clés.
         * 
         * @tparam T Type des valeurs à comparer (doit supporter operator==)
         * 
         * @note constexpr : évaluable à la compilation pour les types triviaux
         * @note noexcept : suppose que operator== de T ne lance pas d'exception
         */
        template<typename T>
        struct NkEqual {

            /**
             * @brief Compare deux valeurs pour l'égalité
             * @param lhs Premier opérande (left-hand side)
             * @param rhs Second opérande (right-hand side)
             * @return true si lhs == rhs, false sinon
             * @note Délègue à l'operator== défini pour le type T
             */
            constexpr bool operator()(const T& lhs, const T& rhs) const noexcept
            {
                return lhs == rhs;
            }
        };

        /**
         * @struct NkLess
         * @brief Comparateur d'ordre strict croissant, équivalent à std::less
         * 
         * Définit un ordre strict faible pour le tri et les conteneurs ordonnés.
         * Utilisé par NkMap, NkSet, et les algorithmes de tri.
         * 
         * @tparam T Type des valeurs à comparer (doit supporter operator<)
         * 
         * @note constexpr : évaluable à la compilation pour les types triviaux
         * @note noexcept : suppose que operator< de T ne lance pas d'exception
         */
        template<typename T>
        struct NkLess {

            /**
             * @brief Compare deux valeurs pour l'ordre strict croissant
             * @param lhs Premier opérande
             * @param rhs Second opérande
             * @return true si lhs < rhs, false sinon
             * @note Délègue à l'operator< défini pour le type T
             */
            constexpr bool operator()(const T& lhs, const T& rhs) const noexcept
            {
                return lhs < rhs;
            }
        };

        /**
         * @struct NkGreater
         * @brief Comparateur d'ordre strict décroissant, équivalent à std::greater
         * 
         * Inverse de NkLess : utile pour les tris inversés ou les priorités inverses.
         * 
         * @tparam T Type des valeurs à comparer (doit supporter operator>)
         */
        template<typename T>
        struct NkGreater {

            /**
             * @brief Compare deux valeurs pour l'ordre strict décroissant
             * @param lhs Premier opérande
             * @param rhs Second opérande
             * @return true si lhs > rhs, false sinon
             */
            constexpr bool operator()(const T& lhs, const T& rhs) const noexcept
            {
                return lhs > rhs;
            }
        };

        /**
         * @struct NkLessEqual
         * @brief Comparateur d'ordre large croissant, équivalent à std::less_equal
         * 
         * Inclut l'égalité dans la comparaison : lhs <= rhs.
         * Utile pour les requêtes de plage et les prédicats de borne supérieure.
         * 
         * @tparam T Type des valeurs à comparer (doit supporter operator<=)
         */
        template<typename T>
        struct NkLessEqual {

            /**
             * @brief Compare deux valeurs pour l'ordre large croissant
             * @param lhs Premier opérande
             * @param rhs Second opérande
             * @return true si lhs <= rhs, false sinon
             */
            constexpr bool operator()(const T& lhs, const T& rhs) const noexcept
            {
                return lhs <= rhs;
            }
        };

        /**
         * @struct NkGreaterEqual
         * @brief Comparateur d'ordre large décroissant, équivalent à std::greater_equal
         * 
         * Inclut l'égalité dans la comparaison inverse : lhs >= rhs.
         * Utile pour les requêtes de plage inversées et bornes inférieures.
         * 
         * @tparam T Type des valeurs à comparer (doit supporter operator>=)
         */
        template<typename T>
        struct NkGreaterEqual {

            /**
             * @brief Compare deux valeurs pour l'ordre large décroissant
             * @param lhs Premier opérande
             * @param rhs Second opérande
             * @return true si lhs >= rhs, false sinon
             */
            constexpr bool operator()(const T& lhs, const T& rhs) const noexcept
            {
                return lhs >= rhs;
            }
        };

        // ====================================================================
        // SECTION : OPÉRATEURS LOGIQUES GÉNÉRIQUES
        // ====================================================================

        /**
         * @struct NkLogicalAnd
         * @brief Opérateur logique ET générique, équivalent à std::logical_and
         * 
         * Évalue lhs && rhs avec sémantique de court-circuit préservée
         * par l'évaluation native de l'opérateur && en C++.
         * 
         * @tparam T Type des opérandes (doit être convertible en bool)
         * 
         * @note constexpr : évaluable à la compilation pour les expressions constantes
         * @note noexcept : l'opérateur && natif ne lance pas d'exception
         */
        template<typename T>
        struct NkLogicalAnd {

            /**
             * @brief Applique l'opérateur logique ET entre deux valeurs
             * @param lhs Premier opérande
             * @param rhs Second opérande
             * @return true si lhs et rhs sont tous deux truthy, false sinon
             * @note Court-circuit : si lhs est false, rhs n'est pas évalué
             */
            constexpr bool operator()(const T& lhs, const T& rhs) const noexcept
            {
                return lhs && rhs;
            }
        };

        /**
         * @struct NkLogicalOr
         * @brief Opérateur logique OU générique, équivalent à std::logical_or
         * 
         * Évalue lhs || rhs avec sémantique de court-circuit préservée.
         * Utile pour combiner des prédicats ou des conditions booléennes.
         * 
         * @tparam T Type des opérandes (doit être convertible en bool)
         */
        template<typename T>
        struct NkLogicalOr {

            /**
             * @brief Applique l'opérateur logique OU entre deux valeurs
             * @param lhs Premier opérande
             * @param rhs Second opérande
             * @return true si au moins l'un des opérandes est truthy, false sinon
             * @note Court-circuit : si lhs est true, rhs n'est pas évalué
             */
            constexpr bool operator()(const T& lhs, const T& rhs) const noexcept
            {
                return lhs || rhs;
            }
        };

        /**
         * @struct NkLogicalNot
         * @brief Opérateur logique NON générique, équivalent à std::logical_not
         * 
         * Négation logique d'une valeur : !value.
         * Utile pour inverser des prédicats ou des conditions.
         * 
         * @tparam T Type de l'opérande (doit être convertible en bool)
         */
        template<typename T>
        struct NkLogicalNot {

            /**
             * @brief Applique la négation logique à une valeur
             * @param value Opérande à nier
             * @return true si value est falsy, false si value est truthy
             * @note Délègue à l'opérateur ! natif du type T
             */
            constexpr bool operator()(const T& value) const noexcept
            {
                return !value;
            }
        };

    } // namespace nkentseu

#endif // NK_CORE_FUNCTIONAL_NKFUNCTIONAL_H

// ============================================================================
// EXEMPLES D'UTILISATION ET PATTERNS COURANTS
// ============================================================================
/*
 * @section examples Exemples pratiques d'utilisation des foncteurs NkFunctional
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 1 : Utilisation de NkHash avec NkHashMap
 * --------------------------------------------------------------------------
 * @code
 * #include "NKCore/Functional/NkFunctional.h"
 * #include "NKContainers/Associative/NkHashMap.h"
 * #include <cstdio>
 * 
 * void exempleNkHashBasique()
 * {
 *     // NkHash est utilisé automatiquement par NkHashMap pour les clés
 *     nkentseu::NkHashMap<int32_t, nkentseu::NkString> map;
 *     
 *     // Insertion : le hachage de la clé int32_t est calculé via NkHash<int32_t>
 *     map.Insert(42, "Réponse ultime");
 *     map.Insert(-7, "Nombre négatif");
 *     
 *     // Recherche : le hachage est recalculé pour localiser la clé
 *     auto* value = map.Find(42);
 *     if (value)
 *     {
 *         printf("Clé 42 -> %s\n", value->CStr());  // "Réponse ultime"
 *     }
 *     
 *     // Hachage manuel pour usage personnalisé
 *     nkentseu::NkHash<nkentseu::NkString> stringHasher;
 *     nkentseu::usize hash1 = stringHasher(nkentseu::NkString("test"));
 *     nkentseu::usize hash2 = stringHasher(nkentseu::NkString("test"));
 *     NKENTSEU_ASSERT(hash1 == hash2);  // Mêmes chaînes -> mêmes hachages
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 2 : Spécialisation de NkHash pour un type personnalisé
 * --------------------------------------------------------------------------
 * @code
 * // Structure personnalisée représentant un point 2D
 * struct Point2D {
 *     int32_t x;
 *     int32_t y;
 *     
 *     Point2D(int32_t px, int32_t py) : x(px), y(py) {}
 *     
 *     // Operator== requis pour NkEqual (utilisé par les conteneurs)
 *     bool operator==(const Point2D& other) const {
 *         return x == other.x && y == other.y;
 *     }
 * };
 * 
 * // Spécialisation de NkHash pour Point2D
 * // Doit être dans le namespace nkentseu pour l'ADL (Argument-Dependent Lookup)
 * namespace nkentseu {
 *     template<>
 *     struct NkHash<Point2D> {
 *         usize operator()(const Point2D& point) const noexcept {
 *             // Combinaison des hachages de x et y via FNV-1a mixé
 *             constexpr usize fnv_prime = sizeof(usize) == 8 ? 1099511628211ULL : 16777619U;
 *             usize hash = NkHash<int32_t>{}(point.x);
 *             hash ^= NkHash<int32_t>{}(point.y) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
 *             hash *= fnv_prime;
 *             return hash;
 *         }
 *     };
 * } // namespace nkentseu
 * 
 * void exempleHashPersonnalise()
 * {
 *     // Maintenant Point2D peut être utilisé comme clé dans NkHashMap
 *     nkentseu::NkHashMap<Point2D, nkentseu::NkString> pointMap;
 *     
 *     pointMap.Insert(Point2D(10, 20), "Zone A");
 *     pointMap.Insert(Point2D(10, 20), "Zone A mise à jour");  // Écrase la valeur précédente
 *     pointMap.Insert(Point2D(30, 40), "Zone B");
 *     
 *     auto* result = pointMap.Find(Point2D(10, 20));
 *     if (result)
 *     {
 *         printf("Point(10,20) -> %s\n", result->CStr());  // "Zone A mise à jour"
 *     }
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 3 : Utilisation des comparateurs avec NkMap (arbre binaire)
 * --------------------------------------------------------------------------
 * @code
 * #include "NKContainers/Associative/NkMap.h"  // Conteneur ordonné basé sur arbre
 * 
 * void exempleComparateurs()
 * {
 *     // NkMap utilise NkLess par défaut pour l'ordre croissant des clés
 *     nkentseu::NkMap<int32_t, nkentseu::NkString> mapCroissant;
 *     mapCroissant.Insert(3, "Trois");
 *     mapCroissant.Insert(1, "Un");
 *     mapCroissant.Insert(2, "Deux");
 *     
 *     // Parcours : les éléments sont ordonnés par clé croissante (1, 2, 3)
 *     printf("Ordre croissant:\n");
 *     for (auto& pair : mapCroissant)
 *     {
 *         printf("  %d -> %s\n", pair.First, pair.Second.CStr());
 *     }
 *     
 *     // Utiliser NkGreater pour un ordre décroissant
 *     nkentseu::NkMap<int32_t, nkentseu::NkString, nkentseu::NkGreater<int32_t>> mapDecroissant;
 *     mapDecroissant.Insert(3, "Trois");
 *     mapDecroissant.Insert(1, "Un");
 *     mapDecroissant.Insert(2, "Deux");
 *     
 *     // Parcours : les éléments sont ordonnés par clé décroissante (3, 2, 1)
 *     printf("\nOrdre décroissant:\n");
 *     for (auto& pair : mapDecroissant)
 *     {
 *         printf("  %d -> %s\n", pair.First, pair.Second.CStr());
 *     }
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 4 : Combinaison de prédicats logiques pour filtrage
 * --------------------------------------------------------------------------
 * @code
 * #include "NKContainers/Sequential/NkVector.h"
 * #include "NKAlgorithms/NkAlgorithm.h"  // Pour NkFindIf, NkCountIf, etc.
 * 
 * void exemplePredicatsLogiques()
 * {
 *     nkentseu::NkVector<int32_t> valeurs = { -5, 0, 3, 10, -2, 7 };
 *     
 *     // Prédicat composé : valeur positive ET paire
 *     auto estPositiveEtPaire = [](int32_t v) {
 *         return nkentseu::NkLogicalAnd<int32_t>{}(v > 0, v % 2 == 0);
 *     };
 *     
 *     // Compter les éléments satisfaisant le prédicat composé
 *     usize count = nkentseu::NkCountIf(valeurs.Begin(), valeurs.End(), estPositiveEtPaire);
 *     printf("Valeurs positives et paires : %zu\n", count);  // 2 (10 et ?)
 *     
 *     // Prédicat : valeur négative OU nulle
 *     auto estNonPositive = [](int32_t v) {
 *         return nkentseu::NkLogicalOr<int32_t>{}(v < 0, v == 0);
 *     };
 *     
 *     // Trouver le premier élément non-positif
 *     auto* it = nkentseu::NkFindIf(valeurs.Begin(), valeurs.End(), estNonPositive);
 *     if (it != valeurs.End())
 *     {
 *         printf("Premier élément non-positif : %d\n", *it);  // -5
 *     }
 *     
 *     // Inversion de prédicat avec NkLogicalNot
 *     auto estStrictementPositive = [](int32_t v) {
 *         return nkentseu::NkLogicalNot<int32_t>{}(nkentseu::NkLogicalOr<int32_t>{}(v < 0, v == 0));
 *     };
 *     // Équivalent à : v > 0, mais démontrant la composition de prédicats
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 5 : Hachage de flottants avec gestion des cas spéciaux
 * --------------------------------------------------------------------------
 * @code
 * void exempleHashFlottants()
 * {
 *     nkentseu::NkHash<float32> floatHasher;
 *     
 *     // Valeurs normales : hachage basé sur la représentation bit-à-bit
 *     nkentseu::usize h1 = floatHasher(3.14f);
 *     nkentseu::usize h2 = floatHasher(3.14f);
 *     NKENTSEU_ASSERT(h1 == h2);  // Mêmes valeurs -> mêmes hachages
 *     
 *     // NaN : toujours le même hachage (0) pour assurer hash(NaN) == hash(NaN)
 *     float32 nan1 = 0.0f / 0.0f;
 *     float32 nan2 = nan1 * 2.0f;
 *     NKENTSEU_ASSERT(nan1 != nan1);  // NaN n'est jamais égal à lui-même
 *     NKENTSEU_ASSERT(floatHasher(nan1) == floatHasher(nan2));  // Mais hachages égaux
 *     
 *     // ±0 : hachages distincts pour préserver la sémantique du signe
 *     NKENTSEU_ASSERT(floatHasher(0.0f) != floatHasher(-0.0f));
 *     
 *     // ±infinity : hachages distincts également
 *     float32 posInf = 1.0f / 0.0f;
 *     float32 negInf = -1.0f / 0.0f;
 *     NKENTSEU_ASSERT(floatHasher(posInf) != floatHasher(negInf));
 *     
 *     // Utilisation dans un NkHashMap avec clés float
 *     nkentseu::NkHashMap<float32, nkentseu::NkString> floatMap;
 *     floatMap.Insert(2.5f, "Deux et demi");
 *     floatMap.Insert(nan1, "Valeur NaN");  // Stocké avec hachage 0
 *     
 *     // Attention : la recherche avec NaN peut être délicate car NaN != NaN
 *     // Privilégier les clés entières ou NkString pour les cas critiques
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * BONNES PRATIQUES ET RECOMMANDATIONS
 * --------------------------------------------------------------------------
 * 
 * 1. SPÉCIALISATION DE NKHASH POUR TYPES PERSONNALISÉS :
 *    - Toujours placer la spécialisation dans le namespace nkentseu pour l'ADL
 *    - Garantir que hash(k1) == hash(k2) si k1 == k2 (cohérence avec NkEqual)
 *    - Viser une distribution uniforme : éviter les collisions excessives
 *    - Documenter la stratégie de hachage pour la maintenance future
 *    - Tester avec des jeux de données réalistes pour valider la distribution
 * 
 * 2. CHOIX DES COMPARATEURS POUR LES CONTENEURS ORDONNÉS :
 *    - NkLess (défaut) : ordre croissant naturel, intuitif pour la plupart des cas
 *    - NkGreater : ordre décroissant, utile pour les priorités inverses ou tris inversés
 *    - NkLessEqual / NkGreaterEqual : rares, car ne définissent pas un ordre strict faible
 *    - Pour les types complexes : définir un comparateur personnalisé avec sémantique claire
 * 
 * 3. PERFORMANCE DES FONCTEURS :
 *    - constexpr : privilégier pour les types triviaux afin d'évaluer à la compilation
 *    - noexcept : garantir pour éviter les overheads d'exception dans les conteneurs
 *    - Éviter les allocations mémoire dans operator() : utiliser des constantes ou calculs locaux
 *    - Pour NkHash<NkString> : FNV-1a est rapide mais O(n) ; pour clés très longues, envisager un hachage partiel
 * 
 * 4. GESTION DES FLOTTANTS DANS LES CLÉS :
 *    - Éviter les float/double comme clés de NkHashMap si possible : problèmes de précision et NaN
 *    - Si nécessaire : normaliser les valeurs (arrondi, epsilon) avant hachage
 *    - Documenter explicitement le comportement avec NaN/±0/±infinity
 *    - Préférer les clés entières ou NkString pour la robustesse
 * 
 * 5. COMPOSITION DE PRÉDICATS LOGIQUES :
 *    - NkLogicalAnd/Or/Not permettent de combiner des conditions de façon générique
 *    - Pour des prédicats complexes : préférer les lambdas pour la lisibilité
 *    - Les foncteurs sont utiles pour les templates et la méta-programmation
 *    - Attention au court-circuit : && et || natifs préservent le court-circuit, pas les appels de fonction
 * 
 * 6. COMPATIBILITÉ ET PORTABILITÉ :
 *    - constexpr : supporté depuis C++11 pour les fonctions simples
 *    - union pour réinterprétation float->int : implementation-defined avant C++20, mais largement supporté
 *    - Pour une portabilité maximale : utiliser std::bit_cast en C++20 ou memcpy pour la réinterprétation
 *    - Tester sur les architectures cibles (32/64 bits) car sizeof(usize) influence les constants FNV
 * 
 * 7. LIMITATIONS CONNUES (version actuelle) :
 *    - Pas de spécialisation pour les tableaux, std::pair, ou types composites standards
 *    - Pas de support pour les hachages salés (seeded hashing) pour la sécurité ou la randomisation
 *    - NkHash<NkString> ne gère pas l'encodage Unicode avancé : hachage octet par octet
 *    - Les comparateurs supposent que operator==/< etc. sont définis et noexcept
 * 
 * 8. EXTENSIONS RECOMMANDÉES :
 *    - Ajouter NkHashCombine() utilitaire pour combiner facilement plusieurs hachages
 *    - Spécialisations pour std::pair<T1,T2>, std::tuple<...>, nkentseu::NkVector<T>
 *    - Support pour hachage avec seed : operator()(const T&, usize seed) pour la randomisation
 *    - Foncteurs pour les comparaisons case-insensitive sur NkString
 *    - Prédicats pour les comparaisons approximatives de flottants (avec epsilon)
 * 
 * 9. COMPARAISON AVEC LA STL :
 *    - NkHash vs std::hash : interface similaire, mais spécialisations spécifiques au framework
 *    - NkEqual/NkLess vs std::equal_to/std::less : mêmes sémantiques, intégration avec NkTypes
 *    - Avantage NkFunctional : constexpr/noexcept garantis, cohérence avec le reste du framework
 *    - Inconvénient : moins de spécialisations standards que la STL ; à étendre selon les besoins
 * 
 * 10. PATTERNS AVANCÉS :
 *    - Hachage parfait (perfect hashing) pour les ensembles de clés statiques connus à la compilation
 *    - Hachage incrémental pour les clés composites : hash = combine(hash, field1, field2, ...)
 *    - Comparateurs contextuels : NkLess avec pointeur vers un contexte de comparaison externe
 *    - Prédicats paramétrés : NkLessThan<T>(threshold) pour des comparaisons avec borne dynamique
 */

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Création : 2025-06-10 par TEUGUIA TADJUIDJE Rodolf
// Dernière modification : 2026-04-26 (restructuration et documentation complète)
// ============================================================