// =============================================================================
// NKContainers/NkIterator.h
// Itérateurs génériques pour le framework NKContainers - sans dépendance STL.
//
// Design :
//  - Réutilisation DIRECTE des macros NKPlatform/NKCore (ZÉRO duplication)
//  - Tags d'itérateurs hiérarchiques (Input → Output → Forward → Bidirectional → RandomAccess)
//  - Templates NkIterator/NkConstIterator avec SFINAE pour opérations conditionnelles
//  - Adaptateur NkReverseIterator pour itération inversée
//  - NkIteratorTraits pour métaprogrammation générique
//  - Fonctions utilitaires : GetBegin/End, NkForeach, NkAdvance, NkDistance, etc.
//  - Compatibilité multiplateforme via NKPlatform
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CONTAINERS_NKITERATOR_H_INCLUDED
#define NKENTSEU_CONTAINERS_NKITERATOR_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKContainers/NkContainersApi.h"   // NKENTSEU_CONTAINERS_API
    #include "NKContainers/NkCompat.h"          // Compatibilité C++ et macros
    #include "NKPlatform/NkPlatformInline.h"    // NKENTSEU_FORCE_INLINE, NKENTSEU_NOEXCEPT

    #include "NKCore/NkTypes.h"                 // nk_size, nk_bool, ptrdiff, etc.
    #include "NKCore/NkTraits.h"                // NkEnableIf_t, NkIsBaseOf_v, etc.

    // -------------------------------------------------------------------------
    // SECTION 2 : ESPACE DE NOMS PRINCIPAL
    // -------------------------------------------------------------------------
    namespace nkentseu {

        // -------------------------------------------------------------------------
        // SECTION 3 : TAGS DE CATÉGORIE D'ITÉRATEURS (Hiérarchie)
        // -------------------------------------------------------------------------
        /**
         * @defgroup IteratorTags Tags de Catégorie d'Itérateurs
         * @brief Marqueurs pour la classification des capacités d'itérateurs
         * @ingroup IteratorUtilities
         *
         * Hiérarchie d'héritage permettant la spécialisation via SFINAE :
         *   NkInputIteratorTag
         *   └─ NkForwardIteratorTag
         *       └─ NkBidirectionalIteratorTag
         *           └─ NkRandomAccessIteratorTag
         *
         * NkOutputIteratorTag est indépendant (écriture seule).
         */
        /** @{ */

        /**
         * @brief Tag pour itérateurs d'entrée (lecture seule, unidirectionnel)
         * @ingroup IteratorTags
         */
        struct NKENTSEU_CONTAINERS_CLASS_EXPORT NkInputIteratorTag {};

        /**
         * @brief Tag pour itérateurs de sortie (écriture seule, unidirectionnel)
         * @ingroup IteratorTags
         */
        struct NKENTSEU_CONTAINERS_CLASS_EXPORT NkOutputIteratorTag {};

        /**
         * @brief Tag pour itérateurs avant (lecture/écriture, multi-pass)
         * @ingroup IteratorTags
         */
        struct NKENTSEU_CONTAINERS_CLASS_EXPORT NkForwardIteratorTag : NkInputIteratorTag {};

        /**
         * @brief Tag pour itérateurs bidirectionnels (avant/arrière)
         * @ingroup IteratorTags
         */
        struct NKENTSEU_CONTAINERS_CLASS_EXPORT NkBidirectionalIteratorTag : NkForwardIteratorTag {};

        /**
         * @brief Tag pour itérateurs à accès aléatoire (arithmétique, indexation)
         * @ingroup IteratorTags
         */
        struct NKENTSEU_CONTAINERS_CLASS_EXPORT NkRandomAccessIteratorTag : NkBidirectionalIteratorTag {};

        /** @} */ // End of IteratorTags

        // -------------------------------------------------------------------------
        // SECTION 4 : NKITERATOR_TRAITS (Métaprogrammation)
        // -------------------------------------------------------------------------
        /**
         * @brief Traits pour l'introspection des types d'itérateurs
         * @tparam Iterator Type de l'itérateur à analyser
         * @ingroup IteratorUtilities
         *
         * Extrait les types associés : ValueType, Pointer, Reference, etc.
         * Fournit des spécialisations pour les pointeurs bruts.
         *
         * @example
         * @code
         * using Value = typename NkIteratorTraits<MyIter>::ValueType;
         * using Diff = typename NkIteratorTraits<MyIter>::DifferenceType;
         * @endcode
         */
        template<typename Iterator>
        struct NKENTSEU_CONTAINERS_CLASS_EXPORT NkIteratorTraits {
            using ValueType = typename Iterator::ValueType;
            using Pointer = typename Iterator::Pointer;
            using Reference = typename Iterator::Reference;
            using DifferenceType = typename Iterator::DifferenceType;
            using IteratorCategory = typename Iterator::IteratorCategory;
        };

        /** @brief Spécialisation pour pointeurs non-const */
        template<typename T>
        struct NkIteratorTraits<T*> {
            using ValueType = T;
            using Pointer = T*;
            using Reference = T&;
            using DifferenceType = nkentseu::ptrdiff;
            using IteratorCategory = NkRandomAccessIteratorTag;
        };

        /** @brief Spécialisation pour pointeurs const */
        template<typename T>
        struct NkIteratorTraits<const T*> {
            using ValueType = T;
            using Pointer = const T*;
            using Reference = const T&;
            using DifferenceType = nkentseu::ptrdiff;
            using IteratorCategory = NkRandomAccessIteratorTag;
        };

        // -------------------------------------------------------------------------
        // SECTION 5 : NKITERATOR (Itérateur mutable générique)
        // -------------------------------------------------------------------------
        /**
         * @brief Itérateur mutable pour conteneurs NKContainers
         * @tparam T Type des éléments pointés
         * @tparam Category Catégorie d'itérateur (défaut: NkRandomAccessIteratorTag)
         * @ingroup IteratorClasses
         *
         * Itérateur wrapper autour d'un pointeur, avec opérations conditionnelles
         * activées via SFINAE selon la catégorie.
         *
         * @note Toutes les opérations sont noexcept et inline pour la performance.
         * @note Les accès ne vérifient pas la validité du pointeur (performance).
         *
         * @example
         * @code
         * int arr[] = {1, 2, 3};
         * NkIterator<int> it(arr);
         * *it = 10;        // Modifie arr[0]
         * ++it;            // Avance à arr[1]
         * auto val = *it;  // val == 2
         * @endcode
         */
        template<typename T, typename Category = NkRandomAccessIteratorTag>
        class NKENTSEU_CONTAINERS_CLASS_EXPORT NkIterator {
            public:
                // Types associés
                using ValueType = T;
                using Pointer = T*;
                using Reference = T&;
                using DifferenceType = nkentseu::ptrdiff;
                using IteratorCategory = Category;

                // -----------------------------------------------------------------
                // Constructeurs
                // -----------------------------------------------------------------
                NKENTSEU_CONTAINERS_API_FORCE_INLINE
                constexpr NkIterator() NKENTSEU_NOEXCEPT : mPtr(nullptr) {}

                NKENTSEU_CONTAINERS_API_FORCE_INLINE
                explicit constexpr NkIterator(Pointer ptr) NKENTSEU_NOEXCEPT : mPtr(ptr) {}

                // -----------------------------------------------------------------
                // Accès aux éléments
                // -----------------------------------------------------------------
                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                Reference operator*() const NKENTSEU_NOEXCEPT { return *mPtr; }

                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                Pointer operator->() const NKENTSEU_NOEXCEPT { return mPtr; }

                // -----------------------------------------------------------------
                // Incrémentation/Décrémentation (toutes catégories)
                // -----------------------------------------------------------------
                NKENTSEU_CONTAINERS_API_FORCE_INLINE
                NkIterator& operator++() NKENTSEU_NOEXCEPT { ++mPtr; return *this; }

                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                NkIterator operator++(int) NKENTSEU_NOEXCEPT {
                    NkIterator tmp = *this; ++mPtr; return tmp;
                }

                // Décrémentation : uniquement pour Bidirectional+
                template<typename C = Category,
                        NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkBidirectionalIteratorTag, C))*>
                NKENTSEU_CONTAINERS_API_FORCE_INLINE
                NkIterator& operator--() NKENTSEU_NOEXCEPT { --mPtr; return *this; }

                template<typename C = Category,
                        NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkBidirectionalIteratorTag, C))*>
                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                NkIterator operator--(int) NKENTSEU_NOEXCEPT {
                    NkIterator tmp = *this; --mPtr; return tmp;
                }

                // -----------------------------------------------------------------
                // Arithmétique : uniquement pour RandomAccess
                // -----------------------------------------------------------------
                template<typename C = Category,
                        NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                NkIterator operator+(DifferenceType offset) const NKENTSEU_NOEXCEPT {
                    return NkIterator(mPtr + offset);
                }

                template<typename C = Category,
                        NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                NkIterator operator-(DifferenceType offset) const NKENTSEU_NOEXCEPT {
                    return NkIterator(mPtr - offset);
                }

                template<typename C = Category,
                        NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
                NKENTSEU_CONTAINERS_API_FORCE_INLINE
                NkIterator& operator+=(DifferenceType offset) NKENTSEU_NOEXCEPT {
                    mPtr += offset; return *this;
                }

                template<typename C = Category,
                        NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
                NKENTSEU_CONTAINERS_API_FORCE_INLINE
                NkIterator& operator-=(DifferenceType offset) NKENTSEU_NOEXCEPT {
                    mPtr -= offset; return *this;
                }

                template<typename C = Category,
                        NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                Reference operator[](DifferenceType offset) const NKENTSEU_NOEXCEPT {
                    return *(mPtr + offset);
                }

                // Distance entre itérateurs
                template<typename C = Category,
                        NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                DifferenceType operator-(const NkIterator& other) const NKENTSEU_NOEXCEPT {
                    return mPtr - other.mPtr;
                }

                // -----------------------------------------------------------------
                // Comparaisons
                // -----------------------------------------------------------------
                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                bool operator==(const NkIterator& other) const NKENTSEU_NOEXCEPT {
                    return mPtr == other.mPtr;
                }

                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                bool operator!=(const NkIterator& other) const NKENTSEU_NOEXCEPT {
                    return mPtr != other.mPtr;
                }

                // Comparaisons avec nullptr
                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                bool operator==(std::nullptr_t) const NKENTSEU_NOEXCEPT {
                    return mPtr == nullptr;
                }

                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                bool operator!=(std::nullptr_t) const NKENTSEU_NOEXCEPT {
                    return mPtr != nullptr;
                }

                // Comparaisons ordonnées : RandomAccess uniquement
                template<typename C = Category,
                        NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                bool operator<(const NkIterator& other) const NKENTSEU_NOEXCEPT {
                    return mPtr < other.mPtr;
                }

                template<typename C = Category,
                        NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                bool operator<=(const NkIterator& other) const NKENTSEU_NOEXCEPT {
                    return mPtr <= other.mPtr;
                }

                template<typename C = Category,
                        NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                bool operator>(const NkIterator& other) const NKENTSEU_NOEXCEPT {
                    return mPtr > other.mPtr;
                }

                template<typename C = Category,
                        NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                bool operator>=(const NkIterator& other) const NKENTSEU_NOEXCEPT {
                    return mPtr >= other.mPtr;
                }

            private:
                Pointer mPtr;
        };

        // -------------------------------------------------------------------------
        // SECTION 6 : NKCONSTITERATOR (Itérateur constant générique)
        // -------------------------------------------------------------------------
        /**
         * @brief Itérateur constant pour conteneurs NKContainers
         * @tparam T Type des éléments pointés
         * @tparam Category Catégorie d'itérateur (défaut: NkRandomAccessIteratorTag)
         * @ingroup IteratorClasses
         *
         * Version const de NkIterator, avec conversion depuis l'itérateur mutable.
         */
        template<typename T, typename Category = NkRandomAccessIteratorTag>
        class NKENTSEU_CONTAINERS_CLASS_EXPORT NkConstIterator {
            public:
                using ValueType = T;
                using Pointer = const T*;
                using Reference = const T&;
                using DifferenceType = nkentseu::ptrdiff;
                using IteratorCategory = Category;

                // Constructeurs
                NKENTSEU_CONTAINERS_API_FORCE_INLINE
                constexpr NkConstIterator() NKENTSEU_NOEXCEPT : mPtr(nullptr) {}

                NKENTSEU_CONTAINERS_API_FORCE_INLINE
                explicit constexpr NkConstIterator(Pointer ptr) NKENTSEU_NOEXCEPT : mPtr(ptr) {}

                // Conversion depuis NkIterator mutable (SFINAE pour non-const T)
                template<typename U = T,
                        NK_ENABLE_IF_T(!NK_IS_CONST_V(U))*>
                NKENTSEU_CONTAINERS_API_FORCE_INLINE
                constexpr NkConstIterator(const NkIterator<U, Category>& other) NKENTSEU_NOEXCEPT
                    : mPtr(other.operator->()) {}

                // Accès
                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                Reference operator*() const NKENTSEU_NOEXCEPT { return *mPtr; }

                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                Pointer operator->() const NKENTSEU_NOEXCEPT { return mPtr; }

                // Incrémentation
                NKENTSEU_CONTAINERS_API_FORCE_INLINE
                NkConstIterator& operator++() NKENTSEU_NOEXCEPT { ++mPtr; return *this; }

                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                NkConstIterator operator++(int) NKENTSEU_NOEXCEPT {
                    NkConstIterator tmp = *this; ++mPtr; return tmp;
                }

                // Décrémentation : Bidirectional+
                template<typename C = Category,
                        NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkBidirectionalIteratorTag, C))*>
                NKENTSEU_CONTAINERS_API_FORCE_INLINE
                NkConstIterator& operator--() NKENTSEU_NOEXCEPT { --mPtr; return *this; }

                template<typename C = Category,
                        NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkBidirectionalIteratorTag, C))*>
                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                NkConstIterator operator--(int) NKENTSEU_NOEXCEPT {
                    NkConstIterator tmp = *this; --mPtr; return tmp;
                }

                // Arithmétique : RandomAccess
                template<typename C = Category,
                        NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                NkConstIterator operator+(DifferenceType offset) const NKENTSEU_NOEXCEPT {
                    return NkConstIterator(mPtr + offset);
                }

                template<typename C = Category,
                        NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                NkConstIterator operator-(DifferenceType offset) const NKENTSEU_NOEXCEPT {
                    return NkConstIterator(mPtr - offset);
                }

                template<typename C = Category,
                        NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
                NKENTSEU_CONTAINERS_API_FORCE_INLINE
                NkConstIterator& operator+=(DifferenceType offset) NKENTSEU_NOEXCEPT {
                    mPtr += offset; return *this;
                }

                template<typename C = Category,
                        NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
                NKENTSEU_CONTAINERS_API_FORCE_INLINE
                NkConstIterator& operator-=(DifferenceType offset) NKENTSEU_NOEXCEPT {
                    mPtr -= offset; return *this;
                }

                template<typename C = Category,
                        NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                Reference operator[](DifferenceType offset) const NKENTSEU_NOEXCEPT {
                    return *(mPtr + offset);
                }

                template<typename C = Category,
                        NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                DifferenceType operator-(const NkConstIterator& other) const NKENTSEU_NOEXCEPT {
                    return mPtr - other.mPtr;
                }

                // Comparaisons
                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                bool operator==(const NkConstIterator& other) const NKENTSEU_NOEXCEPT {
                    return mPtr == other.mPtr;
                }

                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                bool operator!=(const NkConstIterator& other) const NKENTSEU_NOEXCEPT {
                    return mPtr != other.mPtr;
                }

                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                bool operator==(std::nullptr_t) const NKENTSEU_NOEXCEPT {
                    return mPtr == nullptr;
                }

                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                bool operator!=(std::nullptr_t) const NKENTSEU_NOEXCEPT {
                    return mPtr != nullptr;
                }

                // Comparaisons ordonnées
                template<typename C = Category,
                        NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                bool operator<(const NkConstIterator& other) const NKENTSEU_NOEXCEPT {
                    return mPtr < other.mPtr;
                }

                template<typename C = Category,
                        NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                bool operator<=(const NkConstIterator& other) const NKENTSEU_NOEXCEPT {
                    return mPtr <= other.mPtr;
                }

                template<typename C = Category,
                        NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                bool operator>(const NkConstIterator& other) const NKENTSEU_NOEXCEPT {
                    return mPtr > other.mPtr;
                }

                template<typename C = Category,
                        NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                bool operator>=(const NkConstIterator& other) const NKENTSEU_NOEXCEPT {
                    return mPtr >= other.mPtr;
                }

                // Comparaison hétérogène avec NkIterator mutable
                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                bool operator==(const NkIterator<T, Category>& other) const NKENTSEU_NOEXCEPT {
                    return mPtr == other.operator->();
                }

                [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                bool operator!=(const NkIterator<T, Category>& other) const NKENTSEU_NOEXCEPT {
                    return mPtr != other.operator->();
                }

            private:
                Pointer mPtr;
        };

        // -------------------------------------------------------------------------
        // SECTION 7 : SPÉCIALISATIONS NKITERATOR_TRAITS POUR NOS ITÉRATEURS
        // -------------------------------------------------------------------------
        template<typename T, typename Category>
        struct NkIteratorTraits<NkIterator<T, Category>> {
            using ValueType = typename NkIterator<T, Category>::ValueType;
            using Pointer = typename NkIterator<T, Category>::Pointer;
            using Reference = typename NkIterator<T, Category>::Reference;
            using DifferenceType = typename NkIterator<T, Category>::DifferenceType;
            using IteratorCategory = typename NkIterator<T, Category>::IteratorCategory;
        };

        template<typename T, typename Category>
        struct NkIteratorTraits<NkConstIterator<T, Category>> {
            using ValueType = typename NkConstIterator<T, Category>::ValueType;
            using Pointer = typename NkConstIterator<T, Category>::Pointer;
            using Reference = typename NkConstIterator<T, Category>::Reference;
            using DifferenceType = typename NkConstIterator<T, Category>::DifferenceType;
            using IteratorCategory = typename NkConstIterator<T, Category>::IteratorCategory;
        };

        // -------------------------------------------------------------------------
        // SECTION 8 : NKREVERSEITERATOR (Adaptateur d'itération inversée)
        // -------------------------------------------------------------------------
        /**
         * @brief Adaptateur pour itérer en sens inverse
         * @tparam Iterator Type de l'itérateur sous-jacent
         * @ingroup IteratorAdapters
         *
         * Inverse la sémantique d'incrémentation : ++ avance vers le début.
         * Compatible avec tout itérateur supportant --.
         *
         * @example
         * @code
         * NkIterator<int> it(arr + 3);  // Pointe après le dernier élément
         * NkReverseIterator<NkIterator<int>> rit(it);
         * *rit;  // Accède à arr[2]
         * ++rit; // Recule vers arr[1]
         * @endcode
         */
        template<typename Iterator>
        class NKENTSEU_CONTAINERS_CLASS_EXPORT NkReverseIterator {
        public:
            using Traits = NkIteratorTraits<Iterator>;
            using ValueType = typename Traits::ValueType;
            using Pointer = typename Traits::Pointer;
            using Reference = typename Traits::Reference;
            using DifferenceType = typename Traits::DifferenceType;
            using IteratorCategory = typename Traits::IteratorCategory;

            NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr NkReverseIterator() NKENTSEU_NOEXCEPT : mIt() {}

            NKENTSEU_CONTAINERS_API_FORCE_INLINE
            explicit constexpr NkReverseIterator(Iterator it) NKENTSEU_NOEXCEPT : mIt(it) {}

            // Accès : déréférence l'élément PRÉCÉDENT dans l'ordre normal
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            Reference operator*() const NKENTSEU_NOEXCEPT {
                auto tmp = mIt;
                return *--tmp;
            }

            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            Pointer operator->() const NKENTSEU_NOEXCEPT {
                auto tmp = mIt;
                return (--tmp).operator->();
            }

            // Incrémentation = décrémenter l'itérateur sous-jacent
            NKENTSEU_CONTAINERS_API_FORCE_INLINE
            NkReverseIterator& operator++() NKENTSEU_NOEXCEPT { --mIt; return *this; }

            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            NkReverseIterator operator++(int) NKENTSEU_NOEXCEPT {
                NkReverseIterator tmp = *this; --mIt; return tmp;
            }

            // Décrémentation = incrémenter l'itérateur sous-jacent (Bidirectional+)
            template<typename C = IteratorCategory,
                     NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkBidirectionalIteratorTag, C))*>
            NKENTSEU_CONTAINERS_API_FORCE_INLINE
            NkReverseIterator& operator--() NKENTSEU_NOEXCEPT { ++mIt; return *this; }

            template<typename C = IteratorCategory,
                     NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkBidirectionalIteratorTag, C))*>
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            NkReverseIterator operator--(int) NKENTSEU_NOEXCEPT {
                NkReverseIterator tmp = *this; ++mIt; return tmp;
            }

            // Arithmétique inversée (RandomAccess)
            template<typename C = IteratorCategory,
                     NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            NkReverseIterator operator+(DifferenceType offset) const NKENTSEU_NOEXCEPT {
                return NkReverseIterator(mIt - offset);
            }

            template<typename C = IteratorCategory,
                     NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            NkReverseIterator operator-(DifferenceType offset) const NKENTSEU_NOEXCEPT {
                return NkReverseIterator(mIt + offset);
            }

            template<typename C = IteratorCategory,
                     NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
            NKENTSEU_CONTAINERS_API_FORCE_INLINE
            NkReverseIterator& operator+=(DifferenceType offset) NKENTSEU_NOEXCEPT {
                mIt -= offset; return *this;
            }

            template<typename C = IteratorCategory,
                     NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
            NKENTSEU_CONTAINERS_API_FORCE_INLINE
            NkReverseIterator& operator-=(DifferenceType offset) NKENTSEU_NOEXCEPT {
                mIt += offset; return *this;
            }

            template<typename C = IteratorCategory,
                     NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            Reference operator[](DifferenceType offset) const NKENTSEU_NOEXCEPT {
                return *(mIt - (offset + 1));
            }

            // Distance inversée
            template<typename C = IteratorCategory,
                     NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            DifferenceType operator-(const NkReverseIterator& other) const NKENTSEU_NOEXCEPT {
                return other.mIt - mIt;
            }

            // Comparaisons (note: l'ordre est inversé)
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            bool operator==(const NkReverseIterator& other) const NKENTSEU_NOEXCEPT {
                return mIt == other.mIt;
            }

            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            bool operator!=(const NkReverseIterator& other) const NKENTSEU_NOEXCEPT {
                return mIt != other.mIt;
            }

            template<typename C = IteratorCategory,
                     NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            bool operator<(const NkReverseIterator& other) const NKENTSEU_NOEXCEPT {
                return mIt > other.mIt;  // Ordre inversé !
            }

            template<typename C = IteratorCategory,
                     NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            bool operator<=(const NkReverseIterator& other) const NKENTSEU_NOEXCEPT {
                return mIt >= other.mIt;
            }

            template<typename C = IteratorCategory,
                     NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            bool operator>(const NkReverseIterator& other) const NKENTSEU_NOEXCEPT {
                return mIt < other.mIt;
            }

            template<typename C = IteratorCategory,
                     NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkRandomAccessIteratorTag, C))*>
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            bool operator>=(const NkReverseIterator& other) const NKENTSEU_NOEXCEPT {
                return mIt <= other.mIt;
            }

        private:
            Iterator mIt;
        };

        // -------------------------------------------------------------------------
        // SECTION 9 : FONCTIONS UTILITAIRES (GetBegin/End, NkAdvance, etc.)
        // -------------------------------------------------------------------------
        /**
         * @defgroup IteratorUtilities Fonctions Utilitaires pour Itérateurs
         * @brief Helpers génériques pour manipulation d'itérateurs
         */
        /** @{ */

        /**
         * @brief Obtient l'itérateur de début d'un tableau
         * @tparam T Type des éléments
         * @tparam N Taille du tableau
         * @param array Tableau source
         * @return NkIterator<T> pointant sur array[0]
         * @ingroup IteratorUtilities
         */
        template<typename T, nk_size N>
        NKENTSEU_CONTAINERS_API_FORCE_INLINE
        NkIterator<T> GetBegin(T (&array)[N]) NKENTSEU_NOEXCEPT {
            return NkIterator<T>(array);
        }

        /**
         * @brief Obtient l'itérateur de fin d'un tableau
         * @tparam T Type des éléments
         * @tparam N Taille du tableau
         * @param array Tableau source
         * @return NkIterator<T> pointant après array[N-1]
         * @ingroup IteratorUtilities
         */
        template<typename T, nk_size N>
        NKENTSEU_CONTAINERS_API_FORCE_INLINE
        NkIterator<T> GetEnd(T (&array)[N]) NKENTSEU_NOEXCEPT {
            return NkIterator<T>(array + N);
        }

        /**
         * @brief Obtient l'itérateur de début d'un conteneur
         * @tparam Container Type du conteneur (doit avoir Begin())
         * @param container Conteneur source
         * @return Résultat de container.Begin()
         * @ingroup IteratorUtilities
         */
        template<typename Container>
        NKENTSEU_CONTAINERS_API_FORCE_INLINE
        auto GetBegin(Container& container) NKENTSEU_NOEXCEPT
            -> decltype(container.Begin()) {
            return container.Begin();
        }

        /**
         * @brief Obtient l'itérateur de fin d'un conteneur
         * @tparam Container Type du conteneur (doit avoir End())
         * @param container Conteneur source
         * @return Résultat de container.End()
         * @ingroup IteratorUtilities
         */
        template<typename Container>
        NKENTSEU_CONTAINERS_API_FORCE_INLINE
        auto GetEnd(Container& container) NKENTSEU_NOEXCEPT
            -> decltype(container.End()) {
            return container.End();
        }

        /**
         * @brief Version const de GetBegin
         * @ingroup IteratorUtilities
         */
        template<typename Container>
        NKENTSEU_CONTAINERS_API_FORCE_INLINE
        auto GetBegin(const Container& container) NKENTSEU_NOEXCEPT
            -> decltype(container.Begin()) {
            return container.Begin();
        }

        /**
         * @brief Version const de GetEnd
         * @ingroup IteratorUtilities
         */
        template<typename Container>
        NKENTSEU_CONTAINERS_API_FORCE_INLINE
        auto GetEnd(const Container& container) NKENTSEU_NOEXCEPT
            -> decltype(container.End()) {
            return container.End();
        }

        /**
         * @brief Avance un itérateur de n positions
         * @tparam Iterator Type de l'itérateur
         * @param it Itérateur à avancer (modifié in-place)
         * @param n Nombre de positions (peut être négatif pour RandomAccess)
         * @ingroup IteratorUtilities
         */
        template<typename Iterator>
        NKENTSEU_CONTAINERS_API_FORCE_INLINE
        void NkAdvance(Iterator& it,
                       typename NkIteratorTraits<Iterator>::DifferenceType n) NKENTSEU_NOEXCEPT {
            it += n;
        }

        /**
         * @brief Calcule la distance entre deux itérateurs
         * @tparam Iterator Type de l'itérateur
         * @param first Itérateur de début
         * @param last Itérateur de fin
         * @return Nombre d'éléments entre first et last
         * @ingroup IteratorUtilities
         */
        template<typename Iterator>
        NKENTSEU_CONTAINERS_API_FORCE_INLINE
        typename NkIteratorTraits<Iterator>::DifferenceType
        NkDistance(Iterator first, Iterator last) NKENTSEU_NOEXCEPT {
            return last - first;
        }

        /**
         * @brief Retourne un itérateur avancé de n positions (sans modifier l'original)
         * @tparam Iterator Type de l'itérateur
         * @param it Itérateur de base
         * @param n Décalage (défaut: 1)
         * @return Copie de it avancée de n positions
         * @ingroup IteratorUtilities
         */
        template<typename Iterator>
        NKENTSEU_CONTAINERS_API_FORCE_INLINE
        Iterator NkNext(Iterator it,
                        typename NkIteratorTraits<Iterator>::DifferenceType n = 1) NKENTSEU_NOEXCEPT {
            return it + n;
        }

        /**
         * @brief Retourne un itérateur reculé de n positions
         * @tparam Iterator Type de l'itérateur
         * @param it Itérateur de base
         * @param n Décalage (défaut: 1)
         * @return Copie de it reculée de n positions
         * @ingroup IteratorUtilities
         */
        template<typename Iterator>
        NKENTSEU_CONTAINERS_API_FORCE_INLINE
        Iterator NkPrev(Iterator it,
                        typename NkIteratorTraits<Iterator>::DifferenceType n = 1) NKENTSEU_NOEXCEPT {
            return it - n;
        }

        /** @} */ // End of IteratorUtilities

        // -------------------------------------------------------------------------
        // SECTION 10 : FONCTIONS DE PARCOURS (NkForeach, NkWhileach, etc.)
        // -------------------------------------------------------------------------
        /**
         * @defgroup IteratorAlgorithms Algorithmes de Parcours
         * @brief Fonctions d'itération génériques sur plages/conteneurs
         */
        /** @{ */

        /**
         * @brief Applique une opération à chaque élément d'une plage
         * @tparam Iterator Type d'itérateur
         * @tparam Operation Type du callable (doit accepter Reference)
         * @param begin Itérateur de début
         * @param end Itérateur de fin
         * @param op Opération à appliquer
         * @ingroup IteratorAlgorithms
         *
         * @example
         * @code
         * NkForeach(arr, arr + 3, [](int& x) { x *= 2; });
         * @endcode
         */
        template<typename Iterator, typename Operation>
        NKENTSEU_CONTAINERS_API_FORCE_INLINE
        void NkForeach(Iterator begin, Iterator end, Operation op) NKENTSEU_NOEXCEPT {
            for (; begin != end; ++begin) {
                op(*begin);
            }
        }

        /**
         * @brief Applique une opération à chaque élément d'un conteneur
         * @tparam Container Type du conteneur
         * @tparam Operation Type du callable
         * @param container Conteneur à parcourir
         * @param op Opération à appliquer
         * @ingroup IteratorAlgorithms
         */
        template<typename Container, typename Operation>
        NKENTSEU_CONTAINERS_API_FORCE_INLINE
        void NkForeach(const Container& container, Operation op) NKENTSEU_NOEXCEPT {
            NkForeach(GetBegin(container), GetEnd(container), op);
        }

        /**
         * @brief Applique une opération avec index à chaque élément
         * @tparam Iterator Type d'itérateur
         * @tparam Operation Callable acceptant (Reference, nk_size)
         * @param begin Itérateur de début
         * @param end Itérateur de fin
         * @param op Opération avec index
         * @ingroup IteratorAlgorithms
         */
        template<typename Iterator, typename Operation>
        NKENTSEU_CONTAINERS_API_INLINE
        void NkForeachIndexed(Iterator begin, Iterator end, Operation op) {
            nk_size i = 0;
            for (; begin != end; ++begin, ++i) {
                op(*begin, i);
            }
        }

        /**
         * @brief Parcours avec condition d'arrêt personnalisée
         * @tparam Iterator Type d'itérateur
         * @tparam Operation Callable pour traitement
         * @tparam BreakCondition Callable retournant bool pour arrêt
         * @param begin Itérateur de début
         * @param end Itérateur de fin
         * @param op Opération à appliquer
         * @param breakCond Condition d'arrêt (retourne true pour stopper)
         * @ingroup IteratorAlgorithms
         */
        template<typename Iterator, typename Operation, typename BreakCondition>
        NKENTSEU_CONTAINERS_API_INLINE
        void NkForeachWithBreak(Iterator begin, Iterator end,
                                Operation op, BreakCondition breakCond) {
            for (; begin != end; ++begin) {
                op(*begin);
                if (breakCond(*begin)) break;
            }
        }

        /**
         * @brief Parcours tant qu'une condition est vraie
         * @tparam Iterator Type d'itérateur
         * @tparam Condition Callable retournant bool pour continuation
         * @tparam Operation Callable pour traitement
         * @param begin Itérateur de début
         * @param end Itérateur de fin
         * @param cond Condition de continuation
         * @param op Opération à appliquer
         * @ingroup IteratorAlgorithms
         */
        template<typename Iterator, typename Condition, typename Operation>
        NKENTSEU_CONTAINERS_API_FORCE_INLINE
        void NkWhileach(Iterator begin, Iterator end,
                        Condition cond, Operation op) NKENTSEU_NOEXCEPT {
            for (; begin != end && cond(*begin); ++begin) {
                op(*begin);
            }
        }

        /**
         * @brief Version conteneur de NkWhileach
         * @ingroup IteratorAlgorithms
         */
        template<typename Container, typename Condition, typename Operation>
        NKENTSEU_CONTAINERS_API_FORCE_INLINE
        void NkWhileach(const Container& container,
                        Condition cond, Operation op) NKENTSEU_NOEXCEPT {
            NkWhileach(GetBegin(container), GetEnd(container), cond, op);
        }

        /** @} */ // End of IteratorAlgorithms

        // -------------------------------------------------------------------------
        // SECTION 11 : TRAITS DE DÉTECTION D'ITÉRATEUR
        // -------------------------------------------------------------------------
        /**
         * @brief Trait pour détecter si un type est un itérateur NKContainers
         * @tparam T Type à tester
         * @ingroup IteratorTraits
         */
        template<typename T>
        struct NkIsIterator : traits::NkFalseType {};

        template<typename T>
        struct NkIsIterator<T*> : traits::NkTrueType {};

        template<typename T>
        struct NkIsIterator<const T*> : traits::NkTrueType {};

        template<typename T, typename Category>
        struct NkIsIterator<NkIterator<T, Category>> : traits::NkTrueType {};

        template<typename T, typename Category>
        struct NkIsIterator<NkConstIterator<T, Category>> : traits::NkTrueType {};

        /** @brief Variable template constexpr pour NkIsIterator */
        template<typename T>
        inline constexpr nk_bool NkIsIterator_v = NkIsIterator<T>::value;

    } // namespace nkentseu

    // -------------------------------------------------------------------------
    // SECTION 12 : MACROS DE CONFORT POUR BOUCLES (Optionnel)
    // -------------------------------------------------------------------------
    /**
     * @defgroup IteratorMacros Macros de Boucle Confort
     * @brief Macros optionnelles pour syntaxe de boucle simplifiée
     * @note Ces macros sont fournies pour la commodité, l'usage de range-based
     *       for ou des algorithmes NkForeach est recommandé en priorité.
     */
    /** @{ */

    // Comptage d'arguments pour surcharge de macros
    #define NKENTSEU_CONTAINERS_NARG(...) \
        NKENTSEU_CONTAINERS_NARG_(__VA_ARGS__, 8,7,6,5,4,3,2,1,0)
    #define NKENTSEU_CONTAINERS_NARG_(_1,_2,_3,_4,_5,_6,_7,_8,N,...) N
    #define NKENTSEU_CONTAINERS_SELECT(_1,_2,_3,NAME,...) NAME

    /**
     * @brief Macro NK_FOREACH pour boucle intuitive sur conteneurs
     * @def NK_FOREACH
     * @ingroup IteratorMacros
     *
     * Syntaxe : NK_FOREACH(conteneur, variable) ou NK_FOREACH(conteneur, qualif, variable)
     *
     * @example
     * @code
     * NK_FOREACH(myVector, x) {
     *     printf("%d\n", x);
     * }
     * NK_FOREACH(myVector, const auto&, x) {
     *     // x est const
     * }
     * @endcode
     */
    #define NK_FOREACH(...) \
        NKENTSEU_CONTAINERS_SELECT(__VA_ARGS__, \
            NKENTSEU_CONTAINERS_FOREACH_3, \
            NKENTSEU_CONTAINERS_FOREACH_2,)(__VA_ARGS__)

    #define NKENTSEU_CONTAINERS_FOREACH_2(container, value) \
        for (auto it = nkentseu::containers::GetBegin(container), \
                  end = nkentseu::containers::GetEnd(container); \
             it != end; ++it) \
            for (bool nk_foreach_first = true; nk_foreach_first; nk_foreach_first = false) \
                for (auto&& value = *it; nk_foreach_first; nk_foreach_first = false)

    #define NKENTSEU_CONTAINERS_FOREACH_3(container, qualifier, value) \
        for (auto it = nkentseu::containers::GetBegin(container), \
                  end = nkentseu::containers::GetEnd(container); \
             it != end; ++it) \
            for (bool nk_foreach_first = true; nk_foreach_first; nk_foreach_first = false) \
                for (qualifier value = *it; nk_foreach_first; nk_foreach_first = false)

    /**
     * @brief Macro NK_WHILEACH pour boucle conditionnelle
     * @def NK_WHILEACH
     * @ingroup IteratorMacros
     *
     * Syntaxe : NK_WHILEACH(conteneur, variable, condition)
     *
     * @example
     * @code
     * NK_WHILEACH(myVector, x, [](int v){ return v < 10; }) {
     *     printf("%d\n", x);  // Affiche tant que x < 10
     * }
     * @endcode
     */
    #define NK_WHILEACH(...) \
        NKENTSEU_CONTAINERS_SELECT(__VA_ARGS__, \
            NKENTSEU_CONTAINERS_WHILEACH_4, \
            NKENTSEU_CONTAINERS_WHILEACH_3,)(__VA_ARGS__)

    #define NKENTSEU_CONTAINERS_WHILEACH_3(container, value, condition) \
        for (auto it = nkentseu::containers::GetBegin(container), \
                  end = nkentseu::containers::GetEnd(container); \
             it != end && (condition(*it)); ++it) \
            for (bool nk_whileach_first = true; nk_whileach_first; nk_whileach_first = false) \
                for (auto&& value = *it; nk_whileach_first; nk_whileach_first = false)

    #define NKENTSEU_CONTAINERS_WHILEACH_4(container, qualifier, value, condition) \
        for (auto it = nkentseu::containers::GetBegin(container), \
                  end = nkentseu::containers::GetEnd(container); \
             it != end && (condition(*it)); ++it) \
            for (bool nk_whileach_first = true; nk_whileach_first; nk_whileach_first = false) \
                for (qualifier value = *it; nk_whileach_first; nk_whileach_first = false)

    /** @} */ // End of IteratorMacros

    // -------------------------------------------------------------------------
    // SECTION 13 : VALIDATION ET MESSAGES DE DEBUG
    // -------------------------------------------------------------------------
    #ifdef NKENTSEU_CONTAINERS_DEBUG_CONFIG
        #pragma message("NKContainers NkIterator: Iterator utilities loaded")
        #pragma message("  Categories: Input/Output/Forward/Bidirectional/RandomAccess")
        #pragma message("  Classes: NkIterator, NkConstIterator, NkReverseIterator")
        #pragma message("  Utilities: GetBegin/End, NkForeach, NkAdvance, NkDistance")
    #endif

#endif // NKENTSEU_CONTAINERS_NKITERATOR_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION
// =============================================================================
/*
    // Exemple 1 : Itérateur basique sur tableau
    #include <NKContainers/NkIterator.h>
    using namespace nkentseu::containers;

    void ArrayIteration() {
        int arr[] = {10, 20, 30, 40};
        
        NkIterator<int> begin(arr);
        NkIterator<int> end(arr + 4);
        
        // Parcours manuel
        for (auto it = begin; it != end; ++it) {
            printf("%d ", *it);  // Affiche: 10 20 30 40
        }
        
        // Avec NkForeach
        NkForeach(begin, end, [](int& x) { x *= 2; });
        // arr contient maintenant: 20 40 60 80
    }

    // Exemple 2 : Itérateur avec catégorie spécifique
    void ForwardIteratorExample() {
        // Pour une liste chaînée, utiliser ForwardIteratorTag
        using ForwardIter = NkIterator<Node, NkForwardIteratorTag>;
        
        // Seules les opérations ++ et == sont disponibles
        ForwardIter it(head);
        while (it != end) {
            Process(it->data);
            ++it;  // OK
            // --it;  // Erreur de compilation : pas supporté pour Forward
        }
    }

    // Exemple 3 : Itérateur inversé
    void ReverseIteration() {
        int arr[] = {1, 2, 3, 4, 5};
        NkIterator<int> end(arr + 5);
        
        NkReverseIterator<NkIterator<int>> rit(end);
        
        // Parcours à l'envers: 5, 4, 3, 2, 1
        for (int i = 0; i < 5; ++i, ++rit) {
            printf("%d ", *rit);
        }
    }

    // Exemple 4 : Algorithmes utilitaires
    void UtilityFunctions() {
        int arr[] = {1, 2, 3, 4, 5};
        auto begin = GetBegin(arr);
        auto end = GetEnd(arr);
        
        // Distance
        auto dist = NkDistance(begin, end);  // dist == 5
        
        // Advance
        auto mid = begin;
        NkAdvance(mid, 2);  // mid pointe sur arr[2] (valeur 3)
        
        // Next/Prev
        auto next = NkNext(begin, 3);  // begin + 3
        auto prev = NkPrev(end, 1);    // end - 1
    }

    // Exemple 5 : Macro nkforeach (confort)
    void MacroExample() {
        std::vector<int> vec = {1, 2, 3};
        
        // Syntaxe concise
        nkforeach(vec, x) {
            printf("%d\n", x);
        }
        
        // Avec qualificateur
        nkforeach(vec, const int&, x) {
            // x est const, lecture seule
        }
    }

    // Exemple 6 : Itérateur constant et conversion
    void ConstIteratorExample() {
        int arr[] = {1, 2, 3};
        NkIterator<int> mutableIt(arr);
        
        // Conversion implicite vers const
        NkConstIterator<int> constIt = mutableIt;
        
        // *constIt est const int&, pas de modification possible
        // constIt->someMethod();  // Appel de méthode const uniquement
    }
*/

// =============================================================================
// NOTES DE MIGRATION ET COMPATIBILITÉ
// =============================================================================
/*
    Migration depuis code STL :

    STL                           | NKContainers équivalent
    ------------------------------|----------------------------------
    std::iterator_traits<T>      | nkentseu::containers::NkIteratorTraits<T>
    std::input_iterator_tag      | NkInputIteratorTag
    std::forward_iterator        | NkIterator<T, NkForwardIteratorTag>
    std::reverse_iterator<T>     | NkReverseIterator<T>
    std::begin(container)        | nkentseu::containers::GetBegin(container)
    std::distance(a, b)          | NkDistance(a, b)
    std::advance(it, n)          | NkAdvance(it, n)
    std::next(it, n)             | NkNext(it, n)
    for (auto& x : container)    | nkforeach(container, x) { ... }

    Avantages NKContainers :
    - Zéro dépendance STL : portable sur plateformes embarquées
    - Contrôle fin des catégories via SFINAE
    - Intégration avec NKCore/NKPlatform (assertions, logging, export)
    - Performance : tout inline, noexcept, pas d'allocations
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================