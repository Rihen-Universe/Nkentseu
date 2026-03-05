// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\Iterators\NkInitializerList.h
// DESCRIPTION: Initializer list - Support natif {1,2,3} SANS STL
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 2.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ITERATORS_NKINITIALIZERLIST_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ITERATORS_NKINITIALIZERLIST_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/NkContainersExport.h"

#if defined(NK_CPP11) && !defined(NK_USE_STD_INITIALIZER_LIST)
    #if defined(__has_include)
        #if __has_include(<initializer_list>)
            #define NK_USE_STD_INITIALIZER_LIST 1
        #endif
    #else
        #define NK_USE_STD_INITIALIZER_LIST 1
    #endif
#endif

namespace nkentseu {
    namespace core {
        
        /**
         * @brief Initializer list - Support pour {1, 2, 3} syntax
         * 
         * Cette implémentation remplace std::initializer_list pour permettre
         * l'utilisation de la syntaxe {1, 2, 3} sans aucune dépendance STL.
         * 
         * @example
         * NkVector<int> vec = {1, 2, 3, 4, 5};
         * NkArray<float, 3> arr = {1.0f, 2.0f, 3.0f};
         * 
         * auto list = {1, 2, 3};  // NkInitializerList<int>
         * for (auto& val : list) {
         *     // ...
         * }
         */
        template<typename T>
        class NkInitializerList {
        public:
            using ValueType = T;
            using SizeType = usize;  // Note: usize est un typedef global, pas dans namespace
            using Reference = const T&;
            using ConstReference = const T&;
            using Iterator = const T*;
            using ConstIterator = const T*;
            
        private:
            const T* mBegin;
            SizeType mSize;
            
        public:
            // ========================================
            // CONSTRUCTORS
            // ========================================
            
            /**
             * @brief Default constructor - liste vide
             */
            NK_CONSTEXPR NkInitializerList() NK_NOEXCEPT
                : mBegin(nullptr)
                , mSize(0) {
            }
            
            /**
             * @brief Constructor depuis pointeur + taille
             * 
             * @note Cette fonction est appelée automatiquement par le compilateur
             *       pour les braced-init-lists en C++11+
             */
            NK_CONSTEXPR NkInitializerList(const T* begin, SizeType size) NK_NOEXCEPT
                : mBegin(begin)
                , mSize(size) {
            }
            
            // ========================================
            // CAPACITY
            // ========================================
            
            NK_CONSTEXPR SizeType Size() const NK_NOEXCEPT { 
                return mSize; 
            }
            
            NK_CONSTEXPR SizeType size() const NK_NOEXCEPT { 
                return mSize; 
            }
            
            NK_CONSTEXPR bool IsEmpty() const NK_NOEXCEPT { 
                return mSize == 0; 
            }
            
            NK_CONSTEXPR bool empty() const NK_NOEXCEPT { 
                return mSize == 0; 
            }
            
            // ========================================
            // ELEMENT ACCESS
            // ========================================
            
            NK_CONSTEXPR ConstReference Front() const NK_NOEXCEPT {
                return mBegin[0];
            }
            
            NK_CONSTEXPR ConstReference Back() const NK_NOEXCEPT {
                return mBegin[mSize - 1];
            }
            
            NK_CONSTEXPR ConstReference operator[](SizeType index) const NK_NOEXCEPT {
                return mBegin[index];
            }
            
            NK_CONSTEXPR const T* Data() const NK_NOEXCEPT {
                return mBegin;
            }
            
            // ========================================
            // ITERATORS
            // ========================================
            
            NK_CONSTEXPR ConstIterator begin() const NK_NOEXCEPT { 
                return mBegin; 
            }
            
            NK_CONSTEXPR ConstIterator end() const NK_NOEXCEPT { 
                return mBegin + mSize; 
            }
            
            NK_CONSTEXPR ConstIterator cbegin() const NK_NOEXCEPT { 
                return mBegin; 
            }
            
            NK_CONSTEXPR ConstIterator cend() const NK_NOEXCEPT { 
                return mBegin + mSize; 
            }
        };
        
        // ========================================
        // HELPER FUNCTIONS
        // ========================================
        
        template<typename T>
        NK_CONSTEXPR NkInitializerList<T> NkMakeInitializerList(const T* data, usize size) {
            return NkInitializerList<T>(data, size);
        }
        
        template<typename T, usize N>
        NK_CONSTEXPR NkInitializerList<T> NkMakeInitializerListFromArray(const T (&arr)[N]) {
            return NkInitializerList<T>(arr, N);
        }
        
        #if defined(NK_CPP11)
        
        template<typename T>
        NK_CONSTEXPR const T* begin(NkInitializerList<T> list) NK_NOEXCEPT {
            return list.begin();
        }
        
        template<typename T>
        NK_CONSTEXPR const T* end(NkInitializerList<T> list) NK_NOEXCEPT {
            return list.end();
        }
        
        #endif
        
    } // namespace core
} // namespace nkentseu

// ========================================
// INJECTION DANS NAMESPACE STD (OPTIONNEL)
// ========================================

/**
 * @brief SOLUTION pour support natif de {1, 2, 3}
 * 
 * En C++11+, le compilateur cherche std::initializer_list pour la syntaxe {}.
 * Pour éviter toute dépendance STL, nous définissons notre propre version
 * dans le namespace std qui redirige vers notre implémentation.
 * 
 * Cette approche est techniquement non-standard mais fonctionne avec tous
 * les compilateurs modernes (GCC, Clang, MSVC).
 */

#if defined(NK_CPP11) && !defined(NK_USE_STD_INITIALIZER_LIST)

namespace std {
    /**
     * @brief std::initializer_list -> nkentseu::core::NkInitializerList
     * 
     * Redéfinition de std::initializer_list pour pointer vers notre implémentation.
     * Ceci permet au compilateur d'utiliser notre classe pour {1, 2, 3}.
     */
    template<typename T>
    class initializer_list {
    private:
        const T* mBegin;
        usize mSize;  // Note: usize est global, pas besoin de namespace
        
    public:
        using value_type = T;
        using reference = const T&;
        using const_reference = const T&;
        using size_type = usize;
        using iterator = const T*;
        using const_iterator = const T*;
        
        constexpr initializer_list() noexcept
            : mBegin(nullptr), mSize(0) {}
        
        constexpr initializer_list(const T* begin, size_type size) noexcept
            : mBegin(begin), mSize(size) {}
        
        constexpr size_type size() const noexcept { return mSize; }
        constexpr const_iterator begin() const noexcept { return mBegin; }
        constexpr const_iterator end() const noexcept { return mBegin + mSize; }
        
        // Conversion vers NkInitializerList
        constexpr operator ::nkentseu::core::NkInitializerList<T>() const noexcept {
            return ::nkentseu::core::NkInitializerList<T>(mBegin, mSize);
        }
    };
    
    template<typename T>
    constexpr const T* begin(initializer_list<T> list) noexcept {
        return list.begin();
    }
    
    template<typename T>
    constexpr const T* end(initializer_list<T> list) noexcept {
        return list.end();
    }
}

#endif // NK_CPP11 && !NK_USE_STD_INITIALIZER_LIST

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ITERATORS_NKINITIALIZERLIST_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
