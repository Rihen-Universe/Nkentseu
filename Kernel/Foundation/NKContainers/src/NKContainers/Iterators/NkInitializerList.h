// =============================================================================
// NKContainers/NkInitializerList.h
// Liste d'initialisation légère, compatible STL, sans dépendance à std::initializer_list
// (sauf pour interopérabilité optionnelle).
//
// Design :
//  - Header-only : toutes les implémentations sont inline
//  - Réutilisation directe des macros NKCore/NKPlatform
//  - Compatible avec NkIterator.h : retourne NkConstIterator pour Begin()/End()
//  - Vue en lecture seule sur des données existantes : ne possède pas les éléments
//  - Interopérabilité STL : constructeur/assignation depuis std::initializer_list<T>
//  - constexpr-friendly : toutes les méthodes sont constexpr pour évaluation compile-time
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CONTAINERS_NKINITIALIZERLIST_H
#define NKENTSEU_CONTAINERS_NKINITIALIZERLIST_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKContainers/NkContainersApi.h"  // NKENTSEU_CONTAINERS_API, etc.
    #include "NKContainers/Iterators/NkIterator.h"  // NkConstIterator, NkReverseIterator
    #include "NKCore/NkTypes.h"                // nk_size (alias de usize)
    #include "NKCore/NkMacros.h"               // NKENTSEU_NODISCARD, etc.

    // Interopérabilité STL optionnelle
    #include <initializer_list>  // Uniquement pour conversion, pas de dépendance runtime

    // -------------------------------------------------------------------------
    // SECTION 2 : CLASSE NkInitializerList
    // -------------------------------------------------------------------------
    /**
     * @defgroup ContainerInitializerList Liste d'Initialisation
     * @brief Vue en lecture seule sur une séquence contiguë d'éléments
     * 
     * Caractéristiques :
     *  - Ne possède PAS les données : référence à un buffer externe
     *  - Lecture seule : pas de méthodes de modification
     *  - Itérateurs : Begin()/End() retournent NkConstIterator
     *  - constexpr : toutes les méthodes sont évaluables à la compilation
     *  - Interop STL : conversion depuis/vers std::initializer_list<T>
     *  - Zéro overhead : deux pointeurs (begin/end) = 16 bytes sur 64-bit
     * 
     * @note Durée de vie : les données référencées doivent vivre plus longtemps que la liste
     * @note Thread-safe : lecture seule, pas de synchronisation nécessaire
     * 
     * @example
     * @code
     * // Création depuis tableau statique
     * int arr[] = {1, 2, 3};
     * nkentseu::containers::NkInitializerList<int> list(arr, 3);
     * 
     * // Création depuis std::initializer_list (interop)
     * auto list2 = nkentseu::containers::NkMakeInitializerList<int>({4, 5, 6});
     * 
     * // Itération
     * for (auto it = list.Begin(); it != list.End(); ++it) {
     *     printf("%d ", *it);  // Affiche : 1 2 3
     * }
     * 
     * // Algorithmes NKContainers
     * nkentseu::containers::NkForeach(list, [](int x) { printf("%d ", x); });
     * @endcode
     */

    namespace nkentseu {
        /**
         * @class NkInitializerList
         * @brief Vue en lecture seule sur une séquence contiguë d'éléments
         * @tparam T Type des éléments (doit être copiable/assignable)
         * @ingroup ContainerInitializerList
         * 
         * Méthodes principales :
         *  - Begin()/End() : itérateurs NkConstIterator pour parcours
         *  - Data()/Size()/Empty() : accès direct aux métadonnées
         *  - Front()/Back()/operator[] : accès aléatoire aux éléments
         *  - Comparaisons : ==, != pour égalité élément par élément
         * 
         * @warning Ne possède pas les données : ne pas retourner une NkInitializerList
         *          pointant vers des données locales d'une fonction
         * 
         * @example Sécurité de durée de vie
         * @code
         * // ❌ DANGEREUX : données locales détruites à la sortie de la fonction
         * NkInitializerList<int> BadExample() {
         *     int local[] = {1, 2, 3};
         *     return NkInitializerList<int>(local, 3);  // dangling pointer !
         * }
         * 
         * // ✅ SÛR : données statiques ou allouées dynamiquement
         * NkInitializerList<int> GoodExample() {
         *     static const int data[] = {1, 2, 3};
         *     return NkInitializerList<int>(data, 3);  // OK : data vit pour tout le programme
         * }
         * @endcode
         */
        template<typename T>
        class NkInitializerList {
        public:
            // =================================================================
            // @name Types associés
            // =================================================================
            
            using ValueType = T;                          ///< Type des éléments
            using SizeType = nkentseu::nk_size;           ///< Type pour la taille
            using Reference = const T&;                   ///< Référence constante
            using ConstReference = const T&;              ///< Alias pour clarté
            using Pointer = const T*;                     ///< Pointeur constant
            using ConstPointer = const T*;                ///< Alias pour clarté
            
            using Iterator = NkConstIterator<T>;          ///< Itérateur constant
            using ConstIterator = NkConstIterator<T>;     ///< Alias pour compatibilité STL
            
            using ReverseIterator = NkReverseIterator<ConstIterator>;      ///< Itérateur inverse
            using ConstReverseIterator = ReverseIterator;                  ///< Alias

            // =================================================================
            // @name Constructeurs
            // =================================================================
            
            /** @brief Constructeur par défaut : liste vide */
            NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr NkInitializerList() noexcept 
                : mBegin(nullptr), mEnd(nullptr) {}

            /**
             * @brief Constructeur depuis plage de pointeurs [first, last)
             * @param first Pointeur vers le premier élément
             * @param last Pointeur vers après le dernier élément
             * @note Ne copie pas les données : référence directe
             */
            NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr NkInitializerList(const T* first, const T* last) noexcept
                : mBegin(first), mEnd(last) {}

            /**
             * @brief Constructeur depuis pointeur + taille
             * @param data Pointeur vers le premier élément
             * @param size Nombre d'éléments
             */
            NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr NkInitializerList(const T* data, SizeType size) noexcept
                : mBegin(data), mEnd(data + size) {}

            /**
             * @brief Constructeur depuis tableau statique
             * @tparam N Taille du tableau (déduite automatiquement)
             * @param arr Tableau statique à référencer
             */
            template<nkentseu::nk_size N>
            NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr NkInitializerList(const T (&arr)[N]) noexcept
                : mBegin(arr), mEnd(arr + N) {}

            /**
             * @brief Constructeur depuis std::initializer_list (interop STL)
             * @param init Liste standard à référencer
             * @note Ne copie pas : référence les données de init
             */
            NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr NkInitializerList(std::initializer_list<T> init) noexcept
                : mBegin(init.begin()), mEnd(init.end()) {}

            // =================================================================
            // @name Itérateurs
            // =================================================================
            
            /** @brief Itérateur constant vers le premier élément */
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr Iterator Begin() const noexcept { return Iterator(mBegin); }

            /** @brief Itérateur constant vers après le dernier élément */
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr Iterator End() const noexcept { return Iterator(mEnd); }

            /** @brief Alias CBegin() pour compatibilité STL */
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr ConstIterator CBegin() const noexcept { return ConstIterator(mBegin); }

            /** @brief Alias CEnd() pour compatibilité STL */
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr ConstIterator CEnd() const noexcept { return ConstIterator(mEnd); }

            /** @brief Itérateur inverse vers le dernier élément */
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr ReverseIterator RBegin() const noexcept {
                return ReverseIterator(End());
            }

            /** @brief Itérateur inverse vers avant le premier élément */
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr ReverseIterator REnd() const noexcept {
                return ReverseIterator(Begin());
            }

            /** @brief Alias CRBegin() pour compatibilité STL */
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr ConstReverseIterator CRBegin() const noexcept { return RBegin(); }

            /** @brief Alias CREnd() pour compatibilité STL */
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr ConstReverseIterator CREnd() const noexcept { return REnd(); }

            // =================================================================
            // @name Accès aux données
            // =================================================================
            
            /** @brief Pointeur vers le premier élément (ou nullptr si vide) */
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr const T* Data() const noexcept { return mBegin; }

            /** @brief Nombre d'éléments dans la liste */
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr SizeType Size() const noexcept {
                return static_cast<SizeType>(mEnd - mBegin);
            }

            /** @brief Teste si la liste est vide */
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr bool Empty() const noexcept { return mBegin == mEnd; }

            /** @brief Référence au premier élément (undefined si Empty()) */
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr Reference Front() const noexcept { return *mBegin; }

            /** @brief Référence au dernier élément (undefined si Empty()) */
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr Reference Back() const noexcept { return *(mEnd - 1); }

            /** @brief Accès par index (sans bounds checking) */
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr Reference operator[](SizeType index) const noexcept {
                return mBegin[index];
            }

            // =================================================================
            // @name Comparaisons
            // =================================================================
            
            /**
             * @brief Égalité élément par élément
             * @param other Autre liste à comparer
             * @return true si même taille et mêmes éléments dans le même ordre
             */
            [[nodiscard]] NKENTSEU_CONTAINERS_API_INLINE
            constexpr bool operator==(const NkInitializerList& other) const noexcept {
                if (Size() != other.Size()) return false;
                for (SizeType i = 0; i < Size(); ++i) {
                    if (mBegin[i] != other.mBegin[i]) return false;
                }
                return true;
            }

            /** @brief Non-égalité : inverse de operator== */
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr bool operator!=(const NkInitializerList& other) const noexcept {
                return !(*this == other);
            }

            // =================================================================
            // @name Assignation (pour interop avec std::initializer_list)
            // =================================================================
            
            /** @brief Assignation depuis une autre NkInitializerList */
            NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr NkInitializerList& operator=(const NkInitializerList& other) noexcept {
                mBegin = other.mBegin;
                mEnd = other.mEnd;
                return *this;
            }

            /** @brief Assignation depuis std::initializer_list (non-const ref) */
            NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr NkInitializerList& operator=(std::initializer_list<T> init) noexcept {
                mBegin = init.begin();
                mEnd = init.end();
                return *this;
            }

            /** @brief Assignation depuis std::initializer_list (const ref) */
            NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr NkInitializerList& operator=(
                const std::initializer_list<T>& init) noexcept {
                mBegin = init.begin();
                mEnd = init.end();
                return *this;
            }

        private:
            const T* mBegin;  ///< Pointeur vers le premier élément
            const T* mEnd;    ///< Pointeur vers après le dernier élément
        };

        // ====================================================================
        // Spécialisation NkIteratorTraits pour NkInitializerList
        // ====================================================================
        
        template<typename T>
        struct NkIteratorTraits<NkInitializerList<T>> {
            using ValueType = T;
            using Pointer = const T*;
            using Reference = const T&;
            using DifferenceType = nkentseu::ptrdiff;
            using IteratorCategory = NkRandomAccessIteratorTag;
        };

        // ====================================================================
        // Helpers de création : NkMakeInitializerList
        // ====================================================================
        
        /**
         * @brief Helper interne pour construction depuis liste d'arguments
         * @note Stocke les éléments dans un tableau interne temporaire
         * @warning À utiliser uniquement pour initialisation locale (pas de retour)
         */
        template<typename T, nkentseu::nk_size N>
        struct NkInitializerListHelper {
            T storage[N];  ///< Buffer interne temporaire
            
            /** @brief Constructeur depuis tableau statique : copie les éléments */
            NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr NkInitializerListHelper(const T (&arr)[N]) noexcept {
                for (nkentseu::nk_size i = 0; i < N; ++i) {
                    storage[i] = arr[i];
                }
            }
            
            /** @brief Conversion vers NkInitializerList<T> */
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr operator NkInitializerList<T>() const noexcept {
                return NkInitializerList<T>(storage, storage + N);
            }
        };

        /** @brief Spécialisation pour liste vide (N=0) */
        template<typename T>
        struct NkInitializerListHelper<T, 0> {
            NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr NkInitializerListHelper() noexcept = default;
            
            template<nkentseu::nk_size N>
            NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr NkInitializerListHelper(const T (&)[N]) noexcept {}
            
            [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
            constexpr operator NkInitializerList<T>() const noexcept {
                return NkInitializerList<T>();
            }
        };

        /**
         * @brief Crée une NkInitializerList depuis une liste d'arguments
         * @tparam T Type des éléments
         * @tparam Args Types des arguments (doivent être convertibles vers T)
         * @param args Arguments à inclure dans la liste
         * @return NkInitializerListHelper<T, N> convertible vers NkInitializerList<T>
         * @note Le helper contient un buffer interne : ne pas retourner la liste résultante
         * 
         * @example
         * @code
         * void ProcessList(NkInitializerList<int> list) {
         *     // ... utilisation ...
         * }
         * 
         * // Appel avec helper temporaire (OK : durée de vie limitée à l'appel)
         * ProcessList(NkMakeInitializerList<int>(1, 2, 3));
         * 
         * // ❌ Ne pas faire :
         * auto bad = NkMakeInitializerList<int>(1, 2, 3);  // buffer temporaire détruit !
         * ProcessList(bad);  // dangling pointer !
         * @endcode
         */
        template<typename T, typename... Args>
        [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
        constexpr NkInitializerListHelper<T, sizeof...(Args)>
        NkMakeInitializerList(Args&&... args) noexcept {
            return NkInitializerListHelper<T, sizeof...(Args)>(
                {static_cast<T>(args)...}
            );
        }

        /** @brief Crée une NkInitializerList depuis un tableau statique */
        template<typename T, nkentseu::nk_size N>
        [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
        constexpr NkInitializerList<T>
        NkMakeInitializerList(const T (&arr)[N]) noexcept {
            return NkInitializerList<T>(arr);
        }

        /** @brief Crée une NkInitializerList depuis pointeur + taille */
        template<typename T>
        [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
        constexpr NkInitializerList<T>
        NkMakeInitializerList(const T* data, nkentseu::nk_size size) noexcept {
            return NkInitializerList<T>(data, size);
        }

        /** @brief Crée une NkInitializerList vide */
        template<typename T>
        [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
        constexpr NkInitializerList<T>
        NkMakeInitializerList() noexcept {
            return NkInitializerList<T>();
        }

        /** @brief Crée une NkInitializerList depuis std::initializer_list */
        template<typename T>
        [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
        constexpr NkInitializerList<T>
        NkMakeInitializerList(std::initializer_list<T> init) noexcept {
            return NkInitializerList<T>(init);
        }

        // ====================================================================
        // Fonctions begin/end libres pour compatibilité range-based for
        // ====================================================================
        
        /** @brief begin() libre pour NkInitializerList */
        template<typename T>
        [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
        constexpr const T* begin(NkInitializerList<T> list) noexcept {
            return list.Begin().operator->();
        }

        /** @brief end() libre pour NkInitializerList */
        template<typename T>
        [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
        constexpr const T* end(NkInitializerList<T> list) noexcept {
            return list.End().operator->();
        }

    } // namespace nkentseu

#endif // NKENTSEU_CONTAINERS_NKINITIALIZERLIST_H

// =============================================================================
// NOTES DE MAINTENANCE
// =============================================================================
/*
    [POURQUOI NE PAS POSSÉDER LES DONNÉES ?]
    
    NkInitializerList est une "vue" (view), pas un conteneur :
    ✓ Zéro allocation : deux pointeurs seulement
    ✓ Zéro copie : référence directe aux données existantes
    ✓ constexpr : évaluable à la compilation pour initialisation statique
    ✓ Interop STL : compatible avec std::initializer_list sans overhead
    
    ⚠️ Responsabilité de l'appelant : garantir que les données vivent assez longtemps
    
    Alternative si besoin de possession : utiliser NkDynamicArray ou NkVector.

    [INTEROPÉRABILITÉ STL]
    
    Inclusion de <initializer_list> uniquement pour :
    - Constructeur NkInitializerList(std::initializer_list<T>)
    - Assignation depuis std::initializer_list<T>
    
    Aucune dépendance runtime : std::initializer_list est lui-même une vue.
    
    Pour désactiver l'interop STL (réduire les includes) :
        #define NKENTSEU_CONTAINERS_DISABLE_STL_INTEROP
        // Puis conditionner les includes/constructeurs dans le code

    [PERFORMANCE]
    
    - Construction : O(1) : juste deux affectations de pointeurs
    - Accès : O(1) : déréférencement de pointeur brut
    - Itération : même performance que boucle sur tableau brut
    - Comparaison == : O(n) dans le pire cas, mais early exit sur première différence

    [CONSTEXPR ET COMPILATION]
    
    Toutes les méthodes sont constexpr pour :
    - Initialisation statique : constexpr NkInitializerList<int> list = {1,2,3};
    - Évaluation compile-time : if constexpr (list.Size() > 0) { /\* ... *\/ }
    - Optimisations agressives du compilateur
    
    Limitations :
    - Les données référencées doivent être constexpr pour évaluation compile-time
    - NkMakeInitializerList avec args crée un buffer local : pas constexpr si args non-constexpr

    [EXTENSIONS POSSIBLES]
    
    1. NkMutableInitializerList : version avec accès en écriture (rarement utile)
    2. NkSpan<T> : vue généralisée avec begin/end + Size() + sous-vues (subspan)
    3. NkStringView : spécialisation pour const char* avec méthodes string-like
    4. Support C++20 ranges : adapter pour std::ranges::range concept

    [DEBUGGING TIPS]
    
    - Si crash à l'accès : vérifier que Data() != nullptr et Size() > 0 avant Front()/Back()
    - Si comparaison == lente : profiler avec de grandes listes ; early exit devrait limiter le coût
    - Si problèmes de durée de vie : utiliser AddressSanitizer pour détecter les use-after-free
    - Pour debugging : ajouter une méthode Dump() en debug-only qui logge les éléments
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================