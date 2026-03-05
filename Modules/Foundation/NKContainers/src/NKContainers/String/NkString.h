// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\NkString.h
// DESCRIPTION: Dynamic string class with SSO - like std::string
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_STRING_NKSTRING_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_STRING_NKSTRING_H_INCLUDED

#include "NKContainers/NkCompat.h"
#include "NkStringView.h"
#include "NKMemory/NkAllocator.h"

namespace nkentseu {
    namespace core {
        
        /**
         * @brief Dynamic string avec Small String Optimization (SSO)
         * 
         * - Stocke petites chaînes (<=23 chars) inline (pas d'allocation)
         * - Grandes chaînes allouées dynamiquement
         * - Compatible avec allocateurs custom
         * - Null-terminated (compatible C)
         * 
         * Équivalent std::string mais SANS STL.
         */
        class NKENTSEU_CORE_API NkString {
            public:
                using ValueType = Char;
                using SizeType = usize;
                static constexpr SizeType npos = static_cast<SizeType>(-1);
                
                // SSO threshold (23 chars + null terminator = 24 bytes)
                static constexpr SizeType SSO_SIZE = NK_STRING_SSO_SIZE;
                
                // ========================================
                // CONSTRUCTORS
                // ========================================
                
                /**
                 * @brief Constructeur par défaut (empty)
                 */
                NkString();
                
                /**
                 * @brief Constructeur avec allocateur
                 */
                explicit NkString(memory::NkIAllocator& allocator);
                
                /**
                 * @brief Constructeur depuis C-string
                 */
                NkString(const Char* str);
                NkString(const Char* str, memory::NkIAllocator& allocator);
                
                /**
                 * @brief Constructeur depuis buffer + taille
                 */
                NkString(const Char* str, SizeType length);
                NkString(const Char* str, SizeType length, memory::NkIAllocator& allocator);
                
                /**
                 * @brief Constructeur depuis StringView
                 */
                NkString(NkStringView view);
                NkString(NkStringView view, memory::NkIAllocator& allocator);
                
                /**
                 * @brief Constructeur fill (n fois le même caractère)
                 */
                NkString(SizeType count, Char ch);
                NkString(SizeType count, Char ch, memory::NkIAllocator& allocator);
                
                /**
                 * @brief Copy constructor
                 */
                NkString(const NkString& other);
                NkString(const NkString& other, memory::NkIAllocator& allocator);
                
                /**
                 * @brief Move constructor
                 */
                NkString(NkString&& other) NK_NOEXCEPT;
                
                /**
                 * @brief Destructeur
                 */
                ~NkString();
                
                // ========================================
                // ASSIGNMENT
                // ========================================
                
                NkString& operator=(const NkString& other);
                NkString& operator=(NkString&& other) NK_NOEXCEPT;
                NkString& operator=(const Char* str);
                NkString& operator=(NkStringView view);
                NkString& operator=(Char ch);
                
                // ========================================
                // ELEMENT ACCESS
                // ========================================
                
                Char& operator[](SizeType index);
                const Char& operator[](SizeType index) const;
                
                Char& At(SizeType index);
                const Char& At(SizeType index) const;
                
                Char& Front();
                const Char& Front() const;
                
                Char& Back();
                const Char& Back() const;
                
                /**
                 * @brief Accès données brutes (null-terminated)
                 */
                const Char* Data() const NK_NOEXCEPT;
                Char* Data() NK_NOEXCEPT;
                
                /**
                 * @brief C-string (null-terminated)
                 */
                const Char* CStr() const NK_NOEXCEPT;
                
                // ========================================
                // CAPACITY
                // ========================================
                
                SizeType Length() const NK_NOEXCEPT;
                SizeType Size() const NK_NOEXCEPT;
                SizeType Capacity() const NK_NOEXCEPT;
                Bool IsEmpty() const NK_NOEXCEPT;
                
                /**
                 * @brief Réserve capacité
                 */
                void Reserve(SizeType newCapacity);
                
                /**
                 * @brief Réduit capacité au minimum
                 */
                void ShrinkToFit();
                
                /**
                 * @brief Vide la chaîne
                 */
                void Clear() NK_NOEXCEPT;
                
                // ========================================
                // MODIFIERS
                // ========================================
                
                /**
                 * @brief Ajoute à la fin
                 */
                NkString& Append(const Char* str);
                NkString& Append(const Char* str, SizeType length);
                NkString& Append(const NkString& str);
                NkString& Append(NkStringView view);
                NkString& Append(SizeType count, Char ch);
                NkString& Append(Char ch);
                
                /**
                 * @brief Opérateur +=
                 */
                NkString& operator+=(const Char* str);
                NkString& operator+=(const NkString& str);
                NkString& operator+=(NkStringView view);
                NkString& operator+=(Char ch);
                
                /**
                 * @brief Insert
                 */
                NkString& Insert(SizeType pos, const Char* str);
                NkString& Insert(SizeType pos, const NkString& str);
                NkString& Insert(SizeType pos, NkStringView view);
                NkString& Insert(SizeType pos, SizeType count, Char ch);
                
                /**
                 * @brief Erase
                 */
                NkString& Erase(SizeType pos = 0, SizeType count = npos);
                
                /**
                 * @brief Push/Pop
                 */
                void PushBack(Char ch);
                void PopBack();
                
                /**
                 * @brief Replace
                 */
                NkString& Replace(SizeType pos, SizeType count, const Char* str);
                NkString& Replace(SizeType pos, SizeType count, const NkString& str);
                NkString& Replace(SizeType pos, SizeType count, NkStringView view);
                
                /**
                 * @brief Resize
                 */
                void Resize(SizeType count);
                void Resize(SizeType count, Char ch);
                
                /**
                 * @brief Swap
                 */
                void Swap(NkString& other) NK_NOEXCEPT;
                
                // ========================================
                // STRING OPERATIONS
                // ========================================
                
                /**
                 * @brief SubString
                 */
                NkString SubStr(SizeType pos = 0, SizeType count = npos) const;
                
                /**
                 * @brief Compare
                 */
                int32 Compare(const NkString& other) const NK_NOEXCEPT;
                int32 Compare(NkStringView other) const NK_NOEXCEPT;
                int32 Compare(const Char* str) const NK_NOEXCEPT;
                
                /**
                 * @brief StartsWith/EndsWith
                 */
                Bool StartsWith(NkStringView prefix) const NK_NOEXCEPT;
                Bool StartsWith(Char ch) const NK_NOEXCEPT;
                Bool EndsWith(NkStringView suffix) const NK_NOEXCEPT;
                Bool EndsWith(Char ch) const NK_NOEXCEPT;
                
                /**
                 * @brief Contains
                 */
                Bool Contains(NkStringView str) const NK_NOEXCEPT;
                Bool Contains(Char ch) const NK_NOEXCEPT;
                
                /**
                 * @brief Find
                 */
                SizeType Find(NkStringView str, SizeType pos = 0) const NK_NOEXCEPT;
                SizeType Find(Char ch, SizeType pos = 0) const NK_NOEXCEPT;
                SizeType RFind(NkStringView str, SizeType pos = npos) const NK_NOEXCEPT;
                SizeType RFind(Char ch, SizeType pos = npos) const NK_NOEXCEPT;
                
                // ========================================
                // CONVERSION
                // ========================================
                
                /**
                 * @brief Conversion vers StringView
                 */
                operator NkStringView() const NK_NOEXCEPT;
                NkStringView View() const NK_NOEXCEPT;
                
                // ========================================
                // ITERATORS
                // ========================================
                
                Char* begin() NK_NOEXCEPT;
                const Char* begin() const NK_NOEXCEPT;
                Char* end() NK_NOEXCEPT;
                const Char* end() const NK_NOEXCEPT;
                
                // ========================================
                // ALLOCATOR
                // ========================================
                
                memory::NkIAllocator& GetAllocator() const NK_NOEXCEPT;
                
                // Recherche avancée (utile)
                SizeType FindFirstOf(NkStringView chars, SizeType pos = 0) const NK_NOEXCEPT;
                SizeType FindLastOf(NkStringView chars, SizeType pos = npos) const NK_NOEXCEPT;
                SizeType FindFirstNotOf(NkStringView chars, SizeType pos = 0) const NK_NOEXCEPT;
                SizeType FindLastNotOf(NkStringView chars, SizeType pos = npos) const NK_NOEXCEPT;
                
                // Transformations (peuvent être in-place)
                NkString& ToLower();
                NkString& ToUpper();
                NkString& Trim();
                NkString& TrimLeft();
                NkString& TrimRight();
                
                // Conversion numérique (simple)
                Bool ToInt(int32& out) const NK_NOEXCEPT;
                Bool ToFloat(float32& out) const NK_NOEXCEPT;
                
                // Hash
                uint64 Hash() const NK_NOEXCEPT;
    
                // Conversions vers d'autres types numériques
                Bool ToInt64(int64& out) const NK_NOEXCEPT;
                Bool ToUInt(uint32& out) const NK_NOEXCEPT;
                Bool ToUInt64(uint64& out) const NK_NOEXCEPT;
                Bool ToDouble(float64& out) const NK_NOEXCEPT;
                Bool ToBool(bool& out) const NK_NOEXCEPT;
                
                // Conversions avec valeurs par défaut
                int32 ToInt32(int32 defaultValue = 0) const NK_NOEXCEPT;
                float32 ToFloat32(float32 defaultValue = 0.0f) const NK_NOEXCEPT;
                
                // Test de contenu
                Bool IsDigits() const NK_NOEXCEPT;
                Bool IsAlpha() const NK_NOEXCEPT;
                Bool IsAlphaNumeric() const NK_NOEXCEPT;
                Bool IsWhitespace() const NK_NOEXCEPT;
                Bool IsNumeric() const NK_NOEXCEPT;
                Bool IsInteger() const NK_NOEXCEPT;
                
                // Manipulations avancées
                NkString& Reverse();
                NkString& Capitalize();
                NkString& TitleCase();
                NkString& RemoveChars(NkStringView charsToRemove);
                NkString& RemoveAll(Char ch);
                
                // Formatage
                static NkString Format(const Char* format, ...);
                static NkString VFormat(const Char* format, va_list args);
            private:
                memory::NkIAllocator* mAllocator;
                SizeType mLength;
                SizeType mCapacity;
                
                // SSO storage
                union {
                    Char* mHeapData;           // Heap allocation
                    Char mSSOData[SSO_SIZE + 1];  // Stack storage (SSO)
                };
                
                Bool IsSSO() const NK_NOEXCEPT;
                void SetSSO(Bool sso) NK_NOEXCEPT;
                
                Char* GetData() NK_NOEXCEPT;
                const Char* GetData() const NK_NOEXCEPT;
                
                void AllocateHeap(SizeType capacity);
                void DeallocateHeap();
                void MoveToHeap(SizeType newCapacity);
                void GrowIfNeeded(SizeType additionalSize);
                
                static SizeType CalculateGrowth(SizeType current, SizeType needed);
        };
        
        // ========================================
        // NON-MEMBER FUNCTIONS
        // ========================================
        
        /**
         * @brief Opérateur +
         */
        NkString operator+(const NkString& lhs, const NkString& rhs);
        NkString operator+(const NkString& lhs, const Char* rhs);
        NkString operator+(const Char* lhs, const NkString& rhs);
        NkString operator+(const NkString& lhs, Char rhs);
        NkString operator+(Char lhs, const NkString& rhs);
        
        /**
         * @brief Comparaison
         */
        Bool operator==(const NkString& lhs, const NkString& rhs) NK_NOEXCEPT;
        Bool operator==(const NkString& lhs, const Char* rhs) NK_NOEXCEPT;
        Bool operator!=(const NkString& lhs, const NkString& rhs) NK_NOEXCEPT;
        Bool operator<(const NkString& lhs, const NkString& rhs) NK_NOEXCEPT;
        Bool operator<=(const NkString& lhs, const NkString& rhs) NK_NOEXCEPT;
        Bool operator>(const NkString& lhs, const NkString& rhs) NK_NOEXCEPT;
        Bool operator>=(const NkString& lhs, const NkString& rhs) NK_NOEXCEPT;
        
    } // namespace core
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_STRING_NKSTRING_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-07
// Creation Date: 2026-02-07
// ============================================================