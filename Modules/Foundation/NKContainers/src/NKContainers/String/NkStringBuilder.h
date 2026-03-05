// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\NkStringBuilder.h
// DESCRIPTION: String builder (efficient concatenation)
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_STRING_NKSTRINGBUILDER_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_STRING_NKSTRINGBUILDER_H_INCLUDED

#include "NKContainers/NkCompat.h"
#include "NkString.h"
#include "NkStringView.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkMemoryFn.h"

namespace nkentseu {
    namespace core {
        
        /**
         * @brief String builder pour concaténations efficaces
         * 
         * Utilise un buffer qui grandit pour éviter les réallocations multiples.
         * Parfait pour construire de grandes chaînes avec beaucoup d'ajouts.
         * 
         * @example
         * StringBuilder sb;
         * sb.Append("Hello");
         * sb.Append(" ");
         * sb.Append("World");
         * String result = sb.ToString();
         */
        class NKENTSEU_CORE_API NkStringBuilder {
        public:
            using SizeType = usize;
            using ValueType = char;
            using Iterator = char*;
            using ConstIterator = const char*;
            using ReverseIterator = char*;
            using ConstReverseIterator = const char*;
            
            static constexpr SizeType DEFAULT_CAPACITY = 256;
            static constexpr SizeType MAX_CAPACITY = (SizeType)-1 - 1;
            
            // ========================================
            // CONSTRUCTORS & DESTRUCTOR
            // ========================================
            
            /**
             * @brief Constructeur par défaut
             */
            NkStringBuilder();
            
            /**
             * @brief Constructeur avec capacité initiale
             */
            explicit NkStringBuilder(SizeType initialCapacity);
            
            /**
             * @brief Constructeur avec allocateur personnalisé
             */
            explicit NkStringBuilder(memory::NkIAllocator& allocator);
            
            /**
             * @brief Constructeur complet
             */
            NkStringBuilder(SizeType initialCapacity, memory::NkIAllocator& allocator);
            
            /**
             * @brief Constructeur depuis string view
             */
            explicit NkStringBuilder(NkStringView view);
            
            /**
             * @brief Constructeur depuis C-string
             */
            explicit NkStringBuilder(const char* str);
            
            /**
             * @brief Constructeur depuis NkString
             */
            explicit NkStringBuilder(const NkString& str);
            
            /**
             * @brief Constructeur de copie
             */
            NkStringBuilder(const NkStringBuilder& other);
            
            /**
             * @brief Constructeur de déplacement
             */
            NkStringBuilder(NkStringBuilder&& other) NK_NOEXCEPT;
            
            /**
             * @brief Destructeur
             */
            ~NkStringBuilder();
            
            // ========================================
            // ASSIGNMENT OPERATORS
            // ========================================
            
            /**
             * @brief Opérateur d'affectation de copie
             */
            NkStringBuilder& operator=(const NkStringBuilder& other);
            
            /**
             * @brief Opérateur d'affectation de déplacement
             */
            NkStringBuilder& operator=(NkStringBuilder&& other) NK_NOEXCEPT;
            
            /**
             * @brief Affectation depuis string view
             */
            NkStringBuilder& operator=(NkStringView view);
            
            /**
             * @brief Affectation depuis C-string
             */
            NkStringBuilder& operator=(const char* str);
            
            /**
             * @brief Affectation depuis NkString
             */
            NkStringBuilder& operator=(const NkString& str);
            
            // ========================================
            // APPEND OPERATIONS
            // ========================================
            
            /**
             * @brief Ajoute une chaîne
             */
            NkStringBuilder& Append(const char* str);
            NkStringBuilder& Append(const char* str, SizeType length);
            NkStringBuilder& Append(const NkString& str);
            NkStringBuilder& Append(NkStringView view);
            NkStringBuilder& Append(char ch);
            NkStringBuilder& Append(SizeType count, char ch);
            
            /**
             * @brief Ajoute un itérateur de caractères
             */
            template<typename InputIterator>
            NkStringBuilder& Append(InputIterator first, InputIterator last);
            
            /**
             * @brief Ajoute depuis un conteneur
             */
            template<typename Container>
            NkStringBuilder& Append(const Container& container);
            
            /**
             * @brief Ajoute un nombre
             */
            NkStringBuilder& Append(int8 value);
            NkStringBuilder& Append(int16 value);
            NkStringBuilder& Append(int32 value);
            NkStringBuilder& Append(int64 value);
            NkStringBuilder& Append(uint8 value);
            NkStringBuilder& Append(uint16 value);
            NkStringBuilder& Append(uint32 value);
            NkStringBuilder& Append(uint64 value);
            NkStringBuilder& Append(float32 value);
            NkStringBuilder& Append(float64 value);
            NkStringBuilder& Append(bool value);
            
            /**
             * @brief Ajoute en hexadécimal
             */
            NkStringBuilder& AppendHex(uint32 value, bool prefix = false);
            NkStringBuilder& AppendHex(uint64 value, bool prefix = false);
            
            /**
             * @brief Ajoute en binaire
             */
            NkStringBuilder& AppendBinary(uint32 value, SizeType bits = 32);
            NkStringBuilder& AppendBinary(uint64 value, SizeType bits = 64);
            
            /**
             * @brief Ajoute en octal
             */
            NkStringBuilder& AppendOctal(uint32 value, bool prefix = false);
            NkStringBuilder& AppendOctal(uint64 value, bool prefix = false);
            
            /**
             * @brief Opérateur <<
             */
            NkStringBuilder& operator<<(const char* str);
            NkStringBuilder& operator<<(const NkString& str);
            NkStringBuilder& operator<<(NkStringView view);
            NkStringBuilder& operator<<(char ch);
            NkStringBuilder& operator<<(int8 value);
            NkStringBuilder& operator<<(int16 value);
            NkStringBuilder& operator<<(int32 value);
            NkStringBuilder& operator<<(int64 value);
            NkStringBuilder& operator<<(uint8 value);
            NkStringBuilder& operator<<(uint16 value);
            NkStringBuilder& operator<<(uint32 value);
            NkStringBuilder& operator<<(uint64 value);
            NkStringBuilder& operator<<(float32 value);
            NkStringBuilder& operator<<(float64 value);
            NkStringBuilder& operator<<(bool value);
            
            /**
             * @brief Ajoute newline
             */
            NkStringBuilder& AppendLine();
            NkStringBuilder& AppendLine(NkStringView str);
            NkStringBuilder& AppendLine(const char* str);
            NkStringBuilder& AppendLine(const NkString& str);
            
            /**
             * @brief Ajoute avec format (simplifié)
             */
            NkStringBuilder& AppendFormat(const char* format, ...);
            
            /**
             * @brief Ajoute avec format variadique
             */
            NkStringBuilder& AppendVFormat(const char* format, va_list args);
            
            // ========================================
            // INSERT OPERATIONS
            // ========================================
            
            NkStringBuilder& Insert(SizeType pos, char ch);
            NkStringBuilder& Insert(SizeType pos, const char* str);
            NkStringBuilder& Insert(SizeType pos, const char* str, SizeType length);
            NkStringBuilder& Insert(SizeType pos, NkStringView view);
            NkStringBuilder& Insert(SizeType pos, const NkString& str);
            NkStringBuilder& Insert(SizeType pos, SizeType count, char ch);
            
            template<typename InputIterator>
            NkStringBuilder& Insert(SizeType pos, InputIterator first, InputIterator last);
            
            // ========================================
            // REPLACE OPERATIONS
            // ========================================
            
            NkStringBuilder& Replace(SizeType pos, SizeType count, const char* str);
            NkStringBuilder& Replace(SizeType pos, SizeType count, NkStringView view);
            NkStringBuilder& Replace(char oldChar, char newChar);
            NkStringBuilder& Replace(NkStringView oldStr, NkStringView newStr);
            NkStringBuilder& ReplaceAll(char oldChar, char newChar);
            NkStringBuilder& ReplaceAll(NkStringView oldStr, NkStringView newStr);
            
            // ========================================
            // REMOVE/ERASE OPERATIONS
            // ========================================
            
            NkStringBuilder& Remove(SizeType pos, SizeType count);
            NkStringBuilder& RemoveAt(SizeType pos);
            NkStringBuilder& RemovePrefix(SizeType count);
            NkStringBuilder& RemoveSuffix(SizeType count);
            NkStringBuilder& RemoveAll(char ch);
            NkStringBuilder& RemoveAll(NkStringView str);
            NkStringBuilder& RemoveWhitespace();
            NkStringBuilder& Trim();
            NkStringBuilder& TrimLeft();
            NkStringBuilder& TrimRight();
            
            /**
             * @brief Vide le builder
             */
            void Clear() NK_NOEXCEPT;
            
            /**
             * @brief Reset (garde capacité)
             */
            void Reset() NK_NOEXCEPT;
            
            /**
             * @brief Libère toute la mémoire
             */
            void Release();
            
            // ========================================
            // CAPACITY & SIZE
            // ========================================
            
            SizeType Length() const NK_NOEXCEPT;
            SizeType Size() const NK_NOEXCEPT;
            usize Count() const NK_NOEXCEPT;
            SizeType Capacity() const NK_NOEXCEPT;
            SizeType MaxSize() const NK_NOEXCEPT;
            
            bool IsEmpty() const NK_NOEXCEPT;
            bool IsNull() const NK_NOEXCEPT;
            bool IsNullOrEmpty() const NK_NOEXCEPT;
            bool IsFull() const NK_NOEXCEPT;
            
            void Reserve(SizeType newCapacity);
            void Resize(SizeType newSize, char fillChar = '\0');
            void ShrinkToFit();
            
            // ========================================
            // ELEMENT ACCESS
            // ========================================
            
            char& operator[](SizeType index);
            const char& operator[](SizeType index) const;
            
            char& At(SizeType index);
            const char& At(SizeType index) const;
            
            char& Front();
            const char& Front() const;
            
            char& Back();
            const char& Back() const;
            
            char* Data() NK_NOEXCEPT;
            const char* Data() const NK_NOEXCEPT;
            const char* CStr() const NK_NOEXCEPT;
            
            // ========================================
            // SUBSTRING & EXTRACTION
            // ========================================
            
            NkStringView SubStr(SizeType pos = 0, SizeType count = NkStringView::npos) const;
            NkStringView Slice(SizeType start, SizeType end) const;
            NkStringView Left(SizeType count) const;
            NkStringView Right(SizeType count) const;
            NkStringView Mid(SizeType pos, SizeType count = NkStringView::npos) const;
            
            NkString ToString() const;
            NkString ToString(SizeType pos, SizeType count) const;
            
            // ========================================
            // SEARCH & FIND
            // ========================================
            
            SizeType Find(char ch, SizeType pos = 0) const NK_NOEXCEPT;
            SizeType Find(NkStringView str, SizeType pos = 0) const NK_NOEXCEPT;
            SizeType FindLast(char ch, SizeType pos = NkStringView::npos) const NK_NOEXCEPT;
            SizeType FindLast(NkStringView str, SizeType pos = NkStringView::npos) const NK_NOEXCEPT;
            
            bool Contains(char ch) const NK_NOEXCEPT;
            bool Contains(NkStringView str) const NK_NOEXCEPT;
            
            bool StartsWith(char ch) const NK_NOEXCEPT;
            bool StartsWith(NkStringView prefix) const NK_NOEXCEPT;
            
            bool EndsWith(char ch) const NK_NOEXCEPT;
            bool EndsWith(NkStringView suffix) const NK_NOEXCEPT;
            
            // ========================================
            // COMPARISON
            // ========================================
            
            int Compare(NkStringView other) const NK_NOEXCEPT;
            int Compare(const NkStringBuilder& other) const NK_NOEXCEPT;
            
            bool Equals(NkStringView other) const NK_NOEXCEPT;
            bool Equals(const NkStringBuilder& other) const NK_NOEXCEPT;
            
            // ========================================
            // TRANSFORMATIONS
            // ========================================
            
            NkStringBuilder& ToLower();
            NkStringBuilder& ToUpper();
            NkStringBuilder& Capitalize();
            NkStringBuilder& TitleCase();
            NkStringBuilder& Reverse();
            NkStringBuilder& PadLeft(SizeType totalWidth, char paddingChar = ' ');
            NkStringBuilder& PadRight(SizeType totalWidth, char paddingChar = ' ');
            NkStringBuilder& PadCenter(SizeType totalWidth, char paddingChar = ' ');
            
            // ========================================
            // ITERATORS
            // ========================================
            
            Iterator begin() NK_NOEXCEPT;
            ConstIterator begin() const NK_NOEXCEPT;
            ConstIterator cbegin() const NK_NOEXCEPT;
            
            Iterator end() NK_NOEXCEPT;
            ConstIterator end() const NK_NOEXCEPT;
            ConstIterator cend() const NK_NOEXCEPT;
            
            ReverseIterator rbegin() NK_NOEXCEPT;
            ConstReverseIterator rbegin() const NK_NOEXCEPT;
            ConstReverseIterator crbegin() const NK_NOEXCEPT;
            
            ReverseIterator rend() NK_NOEXCEPT;
            ConstReverseIterator rend() const NK_NOEXCEPT;
            ConstReverseIterator crend() const NK_NOEXCEPT;
            
            // ========================================
            // STREAM-LIKE OPERATIONS
            // ========================================
            
            NkStringBuilder& Write(const void* data, SizeType size);
            NkStringBuilder& WriteChar(char ch);
            NkStringBuilder& WriteLine(NkStringView str = NkStringView());
            
            // ========================================
            // CONVERSION OPERATORS
            // ========================================
            
            explicit operator NkString() const;
            explicit operator NkStringView() const NK_NOEXCEPT;
            operator const char*() const NK_NOEXCEPT;
            
            // ========================================
            // UTILITIES
            // ========================================
            
            void Swap(NkStringBuilder& other) NK_NOEXCEPT;
            SizeType Hash() const NK_NOEXCEPT;
            
            // ========================================
            // STATIC METHODS
            // ========================================
            
            static NkStringBuilder Join(NkStringView separator, const NkStringView* strings, SizeType count);
            static NkStringBuilder Join(NkStringView separator, const NkStringBuilder* builders, SizeType count);
            
            template<typename Container>
            static NkStringBuilder Join(NkStringView separator, const Container& items);
            
            // Dans la section OUTPUT du header NkStringBuilder.h
            /**
             * @brief Vue sur contenu
             */
            NkStringView View() const NK_NOEXCEPT {
                return NkStringView(mBuffer, mLength);
            }
        private:
            memory::NkIAllocator* mAllocator;
            char* mBuffer;
            SizeType mLength;
            SizeType mCapacity;
            
            void EnsureCapacity(SizeType needed);
            void GrowIfNeeded(SizeType additionalSize);
            void Reallocate(SizeType newCapacity);
            
            // Helper pour conversion nombres
            static SizeType IntToString(int64 value, char* buffer, SizeType bufferSize);
            static SizeType UIntToString(uint64 value, char* buffer, SizeType bufferSize);
            static SizeType FloatToString(float64 value, char* buffer, SizeType bufferSize, int precision = 6);
            static SizeType HexToString(uint64 value, char* buffer, SizeType bufferSize, bool prefix);
            static SizeType BinaryToString(uint64 value, char* buffer, SizeType bufferSize, SizeType bits);
            static SizeType OctalToString(uint64 value, char* buffer, SizeType bufferSize, bool prefix);
            
            // Memory management
            void FreeBuffer();
            void AllocateBuffer(SizeType capacity);
            void CopyFrom(const NkStringBuilder& other);
            void MoveFrom(NkStringBuilder&& other) NK_NOEXCEPT;
        };
        
        // ========================================
        // NON-MEMBER FUNCTIONS
        // ========================================
        
        inline bool operator==(const NkStringBuilder& lhs, const NkStringBuilder& rhs) NK_NOEXCEPT {
            return lhs.Equals(rhs);
        }
        
        inline bool operator!=(const NkStringBuilder& lhs, const NkStringBuilder& rhs) NK_NOEXCEPT {
            return !lhs.Equals(rhs);
        }
        
        inline bool operator==(const NkStringBuilder& lhs, NkStringView rhs) NK_NOEXCEPT {
            return lhs.Equals(rhs);
        }
        
        inline bool operator!=(const NkStringBuilder& lhs, NkStringView rhs) NK_NOEXCEPT {
            return !lhs.Equals(rhs);
        }
        
        inline bool operator==(NkStringView lhs, const NkStringBuilder& rhs) NK_NOEXCEPT {
            return rhs.Equals(lhs);
        }
        
        inline bool operator!=(NkStringView lhs, const NkStringBuilder& rhs) NK_NOEXCEPT {
            return !rhs.Equals(lhs);
        }
        
        inline NkStringBuilder operator+(const NkStringBuilder& lhs, NkStringView rhs) {
            NkStringBuilder result(lhs);
            result.Append(rhs);
            return result;
        }
        
        inline NkStringBuilder operator+(NkStringView lhs, const NkStringBuilder& rhs) {
            NkStringBuilder result(lhs);
            result.Append(rhs);
            return result;
        }
        
        inline NkStringBuilder operator+(const NkStringBuilder& lhs, const NkStringBuilder& rhs) {
            NkStringBuilder result(lhs);
            result.Append(rhs);
            return result;
        }
        
    } // namespace core
} // namespace nkentseu

// ========================================
// TEMPLATE IMPLEMENTATIONS
// ========================================

namespace nkentseu {
    namespace core {
        
        template<typename InputIterator>
        NkStringBuilder& NkStringBuilder::Append(InputIterator first, InputIterator last) {
            SizeType count = 0;
            for (auto it = first; it != last; ++it) {
                ++count;
            }
            
            GrowIfNeeded(count);
            for (auto it = first; it != last; ++it) {
                mBuffer[mLength++] = *it;
            }
            mBuffer[mLength] = '\0';
            
            return *this;
        }
        
        template<typename Container>
        NkStringBuilder& NkStringBuilder::Append(const Container& container) {
            return Append(container.begin(), container.end());
        }
        
        template<typename InputIterator>
        NkStringBuilder& NkStringBuilder::Insert(SizeType pos, InputIterator first, InputIterator last) {
            NK_ASSERT(pos <= mLength);
            
            SizeType count = 0;
            for (auto it = first; it != last; ++it) {
                ++count;
            }
            
            if (count == 0) return *this;
            
            GrowIfNeeded(count);
            memory::NkMemMove(mBuffer + pos + count, mBuffer + pos, mLength - pos);
            
            SizeType i = 0;
            for (auto it = first; it != last; ++it, ++i) {
                mBuffer[pos + i] = *it;
            }
            
            mLength += count;
            mBuffer[mLength] = '\0';
            
            return *this;
        }
        
        template<typename Container>
        NkStringBuilder NkStringBuilder::Join(NkStringView separator, const Container& items) {
            NkStringBuilder result;
            if (items.empty()) return result;
            
            auto it = items.begin();
            result.Append(*it);
            ++it;
            
            for (; it != items.end(); ++it) {
                result.Append(separator);
                result.Append(*it);
            }
            
            return result;
        }
        
    } // namespace core
} // namespace nkentseu

#endif // NK_CORE_NKENTSEU_SRC_NKENTSEU_STRING_NKSTRINGBUILDER_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================