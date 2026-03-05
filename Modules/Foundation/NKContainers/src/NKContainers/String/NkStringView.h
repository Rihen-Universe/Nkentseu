// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\NkStringView.h
// DESCRIPTION: String view (non-owning, read-only) - like std::string_view
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_STRING_NKSTRINGVIEW_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_STRING_NKSTRINGVIEW_H_INCLUDED

#include "NKContainers/NkCompat.h"
#include "NKCore/NkTypes.h"
#include "NKCore/NkExport.h"
#include "NKPlatform/NkCompilerDetect.h"
#include "NKCore/NkInline.h"
#include "NKMemory/NkMemoryFn.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKCore/NkTraits.h"

namespace nkentseu {
    namespace core {
        
        // Forward declaration de NkString
        class NkString;
        
        /**
         * @brief String view (non-owning, read-only)
         * 
         * Vue sur une chaîne existante sans copie.
         * Équivalent std::string_view C++17.
         * 
         * @warning Ne possède PAS les données - la chaîne source
         *          doit rester valide pendant l'utilisation
         */
        class NKENTSEU_CORE_API NkStringView {
            public:
                using ValueType = char;
                using SizeType = usize;
                using Iterator = const char*;
                using ConstIterator = const char*;
                using ReverseIterator = const char*;
                using ConstReverseIterator = const char*;
                
                static constexpr SizeType npos = static_cast<SizeType>(-1);
                
                // ========================================
                // CONSTRUCTORS & DESTRUCTOR
                // ========================================
                
                /**
                 * @brief Constructeur par défaut (empty)
                 */
                NK_CONSTEXPR NkStringView() NK_NOEXCEPT : mData(nullptr), mLength(0) {}
                
                /**
                 * @brief Constructeur depuis C-string
                 */
                NK_CONSTEXPR NkStringView(const char* str) NK_NOEXCEPT 
                    : mData(str), mLength(str ? CalculateLength(str) : 0) {}
                
                /**
                 * @brief Constructeur depuis pointeur + taille
                 */
                NK_CONSTEXPR NkStringView(const char* str, SizeType length) NK_NOEXCEPT 
                    : mData(str), mLength(length) {}
                
                /**
                 * @brief Constructeur depuis NkString
                 */
                NkStringView(const NkString& str) NK_NOEXCEPT;
                
                /**
                 * @brief Constructeur depuis un tableau statique
                 */
                template<SizeType N>
                NK_CONSTEXPR NkStringView(const char (&str)[N]) NK_NOEXCEPT
                    : mData(str), mLength(N - 1) {} // Exclure le null terminator
                
                /**
                 * @brief Copy constructor
                 */
                NK_CONSTEXPR NkStringView(const NkStringView&) NK_NOEXCEPT = default;
                
                /**
                 * @brief Move constructor
                 */
                NK_CONSTEXPR NkStringView(NkStringView&&) NK_NOEXCEPT = default;
                
                /**
                 * @brief Destructor
                 */
                ~NkStringView() = default;
                
                // ========================================
                // ASSIGNMENT OPERATORS
                // ========================================
                
                /**
                 * @brief Copy assignment
                 */
                NkStringView& operator=(const NkStringView&) NK_NOEXCEPT = default;
                
                /**
                 * @brief Move assignment
                 */
                NkStringView& operator=(NkStringView&&) NK_NOEXCEPT = default;
                
                /**
                 * @brief Assign from C-string
                 */
                NkStringView& operator=(const char* str) NK_NOEXCEPT {
                    mData = str;
                    mLength = str ? CalculateLength(str) : 0;
                    return *this;
                }
                
                // ========================================
                // ELEMENT ACCESS
                // ========================================
                
                char operator[](SizeType index) const NK_NOEXCEPT {
                    NK_ASSERT(index < mLength);
                    return mData[index];
                }
                
                char At(SizeType index) const {
                    NK_ASSERT_MSG(index < mLength, "StringView index out of bounds");
                    return mData[index];
                }
                
                char Front() const NK_NOEXCEPT {
                    NK_ASSERT(mLength > 0);
                    return mData[0];
                }
                
                char Back() const NK_NOEXCEPT {
                    NK_ASSERT(mLength > 0);
                    return mData[mLength - 1];
                }
                
                const char* Data() const NK_NOEXCEPT { return mData; }
                
                NK_CONSTEXPR const char* CStr() const NK_NOEXCEPT {
                    // Note: NkStringView n'a pas de null terminator garanti
                    // Cette fonction est pour compatibilité seulement
                    return mData;
                }
                
                NK_CONSTEXPR char* DataMutable() NK_NOEXCEPT {
                    // Cast const away - use with caution!
                    return const_cast<char*>(mData);
                }
                
                // ========================================
                // CAPACITY
                // ========================================
                
                NK_CONSTEXPR SizeType Length() const NK_NOEXCEPT { return mLength; }
                NK_CONSTEXPR SizeType Size() const NK_NOEXCEPT { return mLength; }
                NK_CONSTEXPR usize Count() const NK_NOEXCEPT { return static_cast<usize>(mLength); }
                
                NK_CONSTEXPR bool IsEmpty() const NK_NOEXCEPT { return mLength == 0; }
                NK_CONSTEXPR bool IsNull() const NK_NOEXCEPT { return mData == nullptr; }
                NK_CONSTEXPR bool IsNullOrEmpty() const NK_NOEXCEPT { return mData == nullptr || mLength == 0; }
                
                NK_CONSTEXPR SizeType Capacity() const NK_NOEXCEPT { return mLength; }
                NK_CONSTEXPR SizeType MaxSize() const NK_NOEXCEPT { return npos - 1; }
                
                // ========================================
                // MEMORY MANAGEMENT
                // ========================================
                
                void Clear() NK_NOEXCEPT {
                    mData = nullptr;
                    mLength = 0;
                }
                
                void Reset() NK_NOEXCEPT {
                    mData = nullptr;
                    mLength = 0;
                }
                
                // ========================================
                // MODIFIERS
                // ========================================
                
                void RemovePrefix(SizeType n) NK_NOEXCEPT {
                    NK_ASSERT(n <= mLength);
                    mData += n;
                    mLength -= n;
                }
                
                void RemoveSuffix(SizeType n) NK_NOEXCEPT {
                    NK_ASSERT(n <= mLength);
                    mLength -= n;
                }
                
                NK_CONSTEXPR void ShrinkToFit() NK_NOEXCEPT {
                    // Rien à faire pour StringView
                }
                
                // ========================================
                // OPERATIONS
                // ========================================
                
                NkStringView SubStr(SizeType pos = 0, SizeType count = npos) const {
                    NK_ASSERT(pos <= mLength);
                    SizeType rcount = (count == npos || pos + count > mLength) 
                        ? mLength - pos : count;
                    return NkStringView(mData + pos, rcount);
                }
                
                NkStringView Slice(SizeType start, SizeType end) const {
                    NK_ASSERT(start <= end && end <= mLength);
                    return NkStringView(mData + start, end - start);
                }
                
                NkStringView Left(SizeType count) const NK_NOEXCEPT {
                    return SubStr(0, count);
                }
                
                NkStringView Right(SizeType count) const NK_NOEXCEPT {
                    if (count >= mLength) return *this;
                    return SubStr(mLength - count, count);
                }
                
                NkStringView Mid(SizeType pos, SizeType count = npos) const {
                    return SubStr(pos, count);
                }
                
                NkStringView Trimmed() const NK_NOEXCEPT;
                NkStringView TrimmedLeft() const NK_NOEXCEPT;
                NkStringView TrimmedRight() const NK_NOEXCEPT;
                
                NkStringView TrimmedChars(const char* chars) const NK_NOEXCEPT {
                    SizeType start = 0;
                    SizeType end = mLength;
                    
                    // Trim left
                    while (start < end) {
                        bool found = false;
                        for (SizeType i = 0; chars[i] != '\0'; ++i) {
                            if (mData[start] == chars[i]) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) break;
                        ++start;
                    }
                    
                    // Trim right
                    while (end > start) {
                        bool found = false;
                        for (SizeType i = 0; chars[i] != '\0'; ++i) {
                            if (mData[end - 1] == chars[i]) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) break;
                        --end;
                    }
                    
                    return SubStr(start, end - start);
                }
                
                NkStringView TrimmedLeftChars(const char* chars) const NK_NOEXCEPT {
                    SizeType start = 0;
                    while (start < mLength) {
                        bool found = false;
                        for (SizeType i = 0; chars[i] != '\0'; ++i) {
                            if (mData[start] == chars[i]) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) break;
                        ++start;
                    }
                    return SubStr(start);
                }
                
                NkStringView TrimmedRightChars(const char* chars) const NK_NOEXCEPT {
                    SizeType end = mLength;
                    while (end > 0) {
                        bool found = false;
                        for (SizeType i = 0; chars[i] != '\0'; ++i) {
                            if (mData[end - 1] == chars[i]) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) break;
                        --end;
                    }
                    return SubStr(0, end);
                }
                
                void Swap(NkStringView& other) NK_NOEXCEPT {
                    const char* tmpData = mData;
                    mData = other.mData;
                    other.mData = tmpData;
                    
                    SizeType tmpLen = mLength;
                    mLength = other.mLength;
                    other.mLength = tmpLen;
                }
                
                // ========================================
                // STRING OPERATIONS
                // ========================================
                
                int Compare(NkStringView other) const NK_NOEXCEPT {
                    SizeType minLen = mLength < other.mLength ? mLength : other.mLength;
                    
                    if (minLen > 0) {
                        int result = memory::NkMemCompare(mData, other.mData, minLen);
                        if (result != 0) return result;
                    }
                    
                    if (mLength < other.mLength) return -1;
                    if (mLength > other.mLength) return 1;
                    return 0;
                }
                
                int CompareIgnoreCase(NkStringView other) const NK_NOEXCEPT;
                int CompareNatural(NkStringView other) const NK_NOEXCEPT;
                
                bool Equals(NkStringView other) const NK_NOEXCEPT {
                    return Compare(other) == 0;
                }
                
                bool EqualsIgnoreCase(NkStringView other) const NK_NOEXCEPT {
                    return CompareIgnoreCase(other) == 0;
                }
                
                bool EqualsNatural(NkStringView other) const NK_NOEXCEPT {
                    return CompareNatural(other) == 0;
                }
                
                bool StartsWith(NkStringView prefix) const NK_NOEXCEPT {
                    if (prefix.mLength > mLength) return false;
                    return memory::NkMemCompare(mData, prefix.mData, prefix.mLength) == 0;
                }
                
                bool StartsWith(char c) const NK_NOEXCEPT {
                    return mLength > 0 && mData[0] == c;
                }
                
                bool StartsWithIgnoreCase(NkStringView prefix) const NK_NOEXCEPT;
                
                bool EndsWith(NkStringView suffix) const NK_NOEXCEPT {
                    if (suffix.mLength > mLength) return false;
                    return memory::NkMemCompare(mData + mLength - suffix.mLength, 
                                              suffix.mData, suffix.mLength) == 0;
                }
                
                bool EndsWith(char c) const NK_NOEXCEPT {
                    return mLength > 0 && mData[mLength - 1] == c;
                }
                
                bool EndsWithIgnoreCase(NkStringView suffix) const NK_NOEXCEPT;
                
                bool Contains(NkStringView str) const NK_NOEXCEPT {
                    return Find(str) != npos;
                }
                
                bool Contains(char c) const NK_NOEXCEPT {
                    return Find(c) != npos;
                }
                
                bool ContainsIgnoreCase(NkStringView str) const NK_NOEXCEPT;
                bool ContainsAny(const char* chars) const NK_NOEXCEPT;
                bool ContainsAll(const char* chars) const NK_NOEXCEPT;
                bool ContainsNone(const char* chars) const NK_NOEXCEPT;
                
                SizeType Find(NkStringView str, SizeType pos = 0) const NK_NOEXCEPT;
                SizeType Find(char c, SizeType pos = 0) const NK_NOEXCEPT;
                SizeType FindIgnoreCase(NkStringView str, SizeType pos = 0) const NK_NOEXCEPT;
                
                SizeType RFind(NkStringView str, SizeType pos = npos) const NK_NOEXCEPT;
                SizeType RFind(char c, SizeType pos = npos) const NK_NOEXCEPT;
                SizeType RFindIgnoreCase(NkStringView str, SizeType pos = npos) const NK_NOEXCEPT;
                
                SizeType FindFirstOf(NkStringView chars, SizeType pos = 0) const NK_NOEXCEPT;
                SizeType FindFirstOf(const char* chars, SizeType pos = 0) const NK_NOEXCEPT;
                SizeType FindFirstOf(char c, SizeType pos = 0) const NK_NOEXCEPT;
                
                SizeType FindLastOf(NkStringView chars, SizeType pos = npos) const NK_NOEXCEPT;
                SizeType FindLastOf(const char* chars, SizeType pos = npos) const NK_NOEXCEPT;
                SizeType FindLastOf(char c, SizeType pos = npos) const NK_NOEXCEPT;
                
                SizeType FindFirstNotOf(NkStringView chars, SizeType pos = 0) const NK_NOEXCEPT;
                SizeType FindFirstNotOf(const char* chars, SizeType pos = 0) const NK_NOEXCEPT;
                SizeType FindFirstNotOf(char c, SizeType pos = 0) const NK_NOEXCEPT;
                
                SizeType FindLastNotOf(NkStringView chars, SizeType pos = npos) const NK_NOEXCEPT;
                SizeType FindLastNotOf(const char* chars, SizeType pos = npos) const NK_NOEXCEPT;
                SizeType FindLastNotOf(char c, SizeType pos = npos) const NK_NOEXCEPT;
                
                SizeType Count(NkStringView str) const NK_NOEXCEPT;
                SizeType Count(char c) const NK_NOEXCEPT;
                SizeType CountAny(const char* chars) const NK_NOEXCEPT;
                
                // ========================================
                // CONVERSIONS
                // ========================================
                
                bool ToInt(int& out) const NK_NOEXCEPT;
                bool ToInt32(int32& out) const NK_NOEXCEPT;
                bool ToInt64(int64& out) const NK_NOEXCEPT;
                bool ToUInt(uint32& out) const NK_NOEXCEPT;
                bool ToUInt64(uint64& out) const NK_NOEXCEPT;
                bool ToFloat(float32& out) const NK_NOEXCEPT;
                bool ToDouble(float64& out) const NK_NOEXCEPT;
                bool ToBool(bool& out) const NK_NOEXCEPT;
                
                int ToIntOrDefault(int defaultValue = 0) const NK_NOEXCEPT;
                int32 ToInt32OrDefault(int32 defaultValue = 0) const NK_NOEXCEPT;
                int64 ToInt64OrDefault(int64 defaultValue = 0) const NK_NOEXCEPT;
                uint32 ToUIntOrDefault(uint32 defaultValue = 0) const NK_NOEXCEPT;
                uint64 ToUInt64OrDefault(uint64 defaultValue = 0) const NK_NOEXCEPT;
                float32 ToFloatOrDefault(float32 defaultValue = 0.0f) const NK_NOEXCEPT;
                float64 ToDoubleOrDefault(float64 defaultValue = 0.0) const NK_NOEXCEPT;
                bool ToBoolOrDefault(bool defaultValue = false) const NK_NOEXCEPT;
                
                NkString ToString() const NK_NOEXCEPT;
                NkString ToLower() const NK_NOEXCEPT;
                NkString ToUpper() const NK_NOEXCEPT;
                NkString ToCapitalized() const NK_NOEXCEPT;
                NkString ToTitleCase() const NK_NOEXCEPT;
                
                // ========================================
                // PREDICATES
                // ========================================
                
                bool IsWhitespace() const NK_NOEXCEPT;
                bool IsDigits() const NK_NOEXCEPT;
                bool IsAlpha() const NK_NOEXCEPT;
                bool IsAlphaNumeric() const NK_NOEXCEPT;
                bool IsHexDigits() const NK_NOEXCEPT;
                bool IsLower() const NK_NOEXCEPT;
                bool IsUpper() const NK_NOEXCEPT;
                bool IsPrintable() const NK_NOEXCEPT;
                bool IsNumeric() const NK_NOEXCEPT;
                bool IsInteger() const NK_NOEXCEPT;
                bool IsFloat() const NK_NOEXCEPT;
                bool IsBoolean() const NK_NOEXCEPT;
                bool IsPalindrome() const NK_NOEXCEPT;
                
                // ========================================
                // ITERATORS
                // ========================================
                
                ConstIterator begin() const NK_NOEXCEPT { return mData; }
                ConstIterator end() const NK_NOEXCEPT { return mData + mLength; }
                ConstIterator cbegin() const NK_NOEXCEPT { return mData; }
                ConstIterator cend() const NK_NOEXCEPT { return mData + mLength; }
                
                ReverseIterator rbegin() const NK_NOEXCEPT { return mData + mLength - 1; }
                ReverseIterator rend() const NK_NOEXCEPT { return mData - 1; }
                ConstReverseIterator crbegin() const NK_NOEXCEPT { return mData + mLength - 1; }
                ConstReverseIterator crend() const NK_NOEXCEPT { return mData - 1; }
                
                // ========================================
                // UTILITIES
                // ========================================
                
                SizeType Hash() const NK_NOEXCEPT;
                SizeType HashIgnoreCase() const NK_NOEXCEPT;
                SizeType HashFNV1a() const NK_NOEXCEPT;
                SizeType HashFNV1aIgnoreCase() const NK_NOEXCEPT;
                
                SizeType LevenshteinDistance(NkStringView other) const NK_NOEXCEPT;
                float64 Similarity(NkStringView other) const NK_NOEXCEPT;
                
                NkStringView WithoutPrefix(SizeType count) const NK_NOEXCEPT {
                    return Right(mLength - count);
                }
                
                NkStringView WithoutSuffix(SizeType count) const NK_NOEXCEPT {
                    return Left(mLength - count);
                }
                
                NkStringView WithoutPadding() const NK_NOEXCEPT {
                    return Trimmed();
                }
                
                bool MatchesPattern(NkStringView pattern) const NK_NOEXCEPT;
                bool MatchesRegex(NkStringView regex) const NK_NOEXCEPT;

                SizeType FindLast(NkStringView str, SizeType pos = npos) const NK_NOEXCEPT {
                    return RFind(str, pos);
                }
                
                SizeType FindLast(char c, SizeType pos = npos) const NK_NOEXCEPT {
                    return RFind(c, pos);
                }
                
                // ========================================
                // OPERATORS
                // ========================================
                
                char operator*() const NK_NOEXCEPT {
                    return Front();
                }
                
                // Note: operator+ est retiré car il nécessite un NkString complet
                // NkStringView operator+(NkStringView other) const NK_NOEXCEPT;
                
                NkStringView& operator+=(NkStringView other) NK_NOEXCEPT {
                    // Note: Cette opération n'est pas vraiment possible avec StringView
                    // car StringView ne peut pas modifier sa taille
                    // On retourne *this inchangé
                    return *this;
                }
                
                template<typename T>
                bool operator==(const T& other) const NK_NOEXCEPT {
                    return Compare(NkStringView(other)) == 0;
                }
                
                template<typename T>
                bool operator!=(const T& other) const NK_NOEXCEPT {
                    return Compare(NkStringView(other)) != 0;
                }
                
                template<typename T>
                bool operator<(const T& other) const NK_NOEXCEPT {
                    return Compare(NkStringView(other)) < 0;
                }
                
                template<typename T>
                bool operator<=(const T& other) const NK_NOEXCEPT {
                    return Compare(NkStringView(other)) <= 0;
                }
                
                template<typename T>
                bool operator>(const T& other) const NK_NOEXCEPT {
                    return Compare(NkStringView(other)) > 0;
                }
                
                template<typename T>
                bool operator>=(const T& other) const NK_NOEXCEPT {
                    return Compare(NkStringView(other)) >= 0;
                }
                
            private:
                const char* mData;
                SizeType mLength;
                
                static NK_CONSTEXPR SizeType CalculateLength(const char* str) NK_NOEXCEPT {
                    SizeType len = 0;
                    if (str) {
                        while (str[len] != '\0') {
                            ++len;
                        }
                    }
                    return len;
                }
        };
        
        // ========================================
        // COMPARISON OPERATORS TEMPLATES
        // ========================================
        
        template<typename T1, typename T2>
        NK_INLINE bool operator==(const T1& lhs, const T2& rhs) NK_NOEXCEPT {
            return NkStringView(lhs).Compare(NkStringView(rhs)) == 0;
        }
        
        template<typename T1, typename T2>
        NK_INLINE bool operator!=(const T1& lhs, const T2& rhs) NK_NOEXCEPT {
            return NkStringView(lhs).Compare(NkStringView(rhs)) != 0;
        }
        
        template<typename T1, typename T2>
        NK_INLINE bool operator<(const T1& lhs, const T2& rhs) NK_NOEXCEPT {
            return NkStringView(lhs).Compare(NkStringView(rhs)) < 0;
        }
        
        template<typename T1, typename T2>
        NK_INLINE bool operator<=(const T1& lhs, const T2& rhs) NK_NOEXCEPT {
            return NkStringView(lhs).Compare(NkStringView(rhs)) <= 0;
        }
        
        template<typename T1, typename T2>
        NK_INLINE bool operator>(const T1& lhs, const T2& rhs) NK_NOEXCEPT {
            return NkStringView(lhs).Compare(NkStringView(rhs)) > 0;
        }
        
        template<typename T1, typename T2>
        NK_INLINE bool operator>=(const T1& lhs, const T2& rhs) NK_NOEXCEPT {
            return NkStringView(lhs).Compare(NkStringView(rhs)) >= 0;
        }
        
        // ========================================
        // LITERALS (C++11 user-defined literals)
        // ========================================
        
#if defined(NK_CPP11)
        inline namespace literals {
            inline NkStringView operator""_sv(const char* str, std::size_t len) NK_NOEXCEPT {
                return NkStringView(str, static_cast<usize>(len));
            }
            
            inline NkStringView operator""_nv(const char* str, std::size_t len) NK_NOEXCEPT {
                return NkStringView(str, static_cast<usize>(len));
            }
        }
        #endif
        
    } // namespace core
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_STRING_NKSTRINGVIEW_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-07
// Creation Date: 2026-02-07
// ============================================================
