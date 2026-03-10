// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\NkStringView.cpp
// DESCRIPTION: String view implementation
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#include "NkStringView.h"
#include "NkString.h"
#include "NkStringUtils.h"
#include "Encoding/NkASCII.h"
#include "NKMemory/NkFunction.h"
#include "NKCore/Assert/NkAssert.h"
#include <cstdlib>
#include <cstring>

namespace nkentseu {
    
        
        // ========================================
        // CONSTRUCTORS
        // ========================================
        
        NkStringView::NkStringView(const NkString& str) NK_NOEXCEPT
            : mData(str.Data())
            , mLength(str.Length()) {
        }
        
        // ========================================
        // STRING OPERATIONS
        // ========================================
        
        int NkStringView::CompareIgnoreCase(NkStringView other) const NK_NOEXCEPT {
            return string::NkCompareIgnoreCase(*this, other);
        }
        
        int NkStringView::CompareNatural(NkStringView other) const NK_NOEXCEPT {
            return string::NkCompareNatural(*this, other);
        }
        
        bool NkStringView::StartsWithIgnoreCase(NkStringView prefix) const NK_NOEXCEPT {
            return string::NkStartsWithIgnoreCase(*this, prefix);
        }
        
        bool NkStringView::EndsWithIgnoreCase(NkStringView suffix) const NK_NOEXCEPT {
            return string::NkEndsWithIgnoreCase(*this, suffix);
        }
        
        bool NkStringView::ContainsIgnoreCase(NkStringView str) const NK_NOEXCEPT {
            return string::NkContainsIgnoreCase(*this, str);
        }
        
        bool NkStringView::ContainsAny(const char* chars) const NK_NOEXCEPT {
            for (SizeType i = 0; i < mLength; ++i) {
                for (SizeType j = 0; chars[j] != '\0'; ++j) {
                    if (mData[i] == chars[j]) {
                        return true;
                    }
                }
            }
            return false;
        }
        
        bool NkStringView::ContainsAll(const char* chars) const NK_NOEXCEPT {
            for (SizeType j = 0; chars[j] != '\0'; ++j) {
                bool found = false;
                for (SizeType i = 0; i < mLength; ++i) {
                    if (mData[i] == chars[j]) {
                        found = true;
                        break;
                    }
                }
                if (!found) return false;
            }
            return true;
        }
        
        bool NkStringView::ContainsNone(const char* chars) const NK_NOEXCEPT {
            return !ContainsAny(chars);
        }
        
        NkStringView::SizeType NkStringView::Find(NkStringView str, SizeType pos) const NK_NOEXCEPT {
            if (str.mLength == 0) return pos;
            if (pos > mLength) return npos;
            if (str.mLength > mLength - pos) return npos;
            
            for (SizeType i = pos; i <= mLength - str.mLength; ++i) {
                if (memory::NkMemCompare(mData + i, str.mData, str.mLength) == 0) {
                    return i;
                }
            }
            return npos;
        }
        
        NkStringView::SizeType NkStringView::Find(char c, SizeType pos) const NK_NOEXCEPT {
            if (pos >= mLength) return npos;
            
            for (SizeType i = pos; i < mLength; ++i) {
                if (mData[i] == c) {
                    return i;
                }
            }
            return npos;
        }
        
        NkStringView::SizeType NkStringView::FindIgnoreCase(NkStringView str, SizeType pos) const NK_NOEXCEPT {
            return string::NkFindIgnoreCase(*this, str, pos);
        }
        
        NkStringView::SizeType NkStringView::RFind(NkStringView str, SizeType pos) const NK_NOEXCEPT {
            if (str.mLength == 0) return (pos < mLength) ? pos : mLength;
            if (str.mLength > mLength) return npos;
            
            SizeType searchPos = (pos == npos || pos >= mLength - str.mLength) 
                ? mLength - str.mLength 
                : pos;
            
            for (SizeType i = searchPos + 1; i > 0; --i) {
                SizeType idx = i - 1;
                if (memory::NkMemCompare(mData + idx, str.mData, str.mLength) == 0) {
                    return idx;
                }
            }
            return npos;
        }
        
        NkStringView::SizeType NkStringView::RFind(char c, SizeType pos) const NK_NOEXCEPT {
            if (mLength == 0) return npos;
            
            SizeType searchPos = (pos == npos || pos >= mLength) ? mLength - 1 : pos;
            
            for (SizeType i = searchPos + 1; i > 0; --i) {
                if (mData[i - 1] == c) {
                    return i - 1;
                }
            }
            return npos;
        }
        
        NkStringView::SizeType NkStringView::RFindIgnoreCase(NkStringView str, SizeType pos) const NK_NOEXCEPT {
            return string::NkFindLastIgnoreCase(*this, str);
        }
        
        NkStringView::SizeType NkStringView::FindFirstOf(NkStringView chars, SizeType pos) const NK_NOEXCEPT {
            for (SizeType i = pos; i < mLength; ++i) {
                for (SizeType j = 0; j < chars.mLength; ++j) {
                    if (mData[i] == chars.mData[j]) {
                        return i;
                    }
                }
            }
            return npos;
        }
        
        NkStringView::SizeType NkStringView::FindFirstOf(const char* chars, SizeType pos) const NK_NOEXCEPT {
            for (SizeType i = pos; i < mLength; ++i) {
                for (SizeType j = 0; chars[j] != '\0'; ++j) {
                    if (mData[i] == chars[j]) {
                        return i;
                    }
                }
            }
            return npos;
        }
        
        NkStringView::SizeType NkStringView::FindFirstOf(char c, SizeType pos) const NK_NOEXCEPT {
            return Find(c, pos);
        }
        
        NkStringView::SizeType NkStringView::FindLastOf(NkStringView chars, SizeType pos) const NK_NOEXCEPT {
            SizeType searchPos = (pos == npos || pos >= mLength) ? mLength - 1 : pos;
            
            for (SizeType i = searchPos + 1; i > 0; --i) {
                SizeType idx = i - 1;
                for (SizeType j = 0; j < chars.mLength; ++j) {
                    if (mData[idx] == chars.mData[j]) {
                        return idx;
                    }
                }
            }
            return npos;
        }
        
        NkStringView::SizeType NkStringView::FindLastOf(const char* chars, SizeType pos) const NK_NOEXCEPT {
            SizeType searchPos = (pos == npos || pos >= mLength) ? mLength - 1 : pos;
            
            for (SizeType i = searchPos + 1; i > 0; --i) {
                SizeType idx = i - 1;
                for (SizeType j = 0; chars[j] != '\0'; ++j) {
                    if (mData[idx] == chars[j]) {
                        return idx;
                    }
                }
            }
            return npos;
        }
        
        NkStringView::SizeType NkStringView::FindLastOf(char c, SizeType pos) const NK_NOEXCEPT {
            return RFind(c, pos);
        }
        
        NkStringView::SizeType NkStringView::FindFirstNotOf(NkStringView chars, SizeType pos) const NK_NOEXCEPT {
            for (SizeType i = pos; i < mLength; ++i) {
                bool found = false;
                for (SizeType j = 0; j < chars.mLength; ++j) {
                    if (mData[i] == chars.mData[j]) {
                        found = true;
                        break;
                    }
                }
                if (!found) return i;
            }
            return npos;
        }
        
        NkStringView::SizeType NkStringView::FindFirstNotOf(const char* chars, SizeType pos) const NK_NOEXCEPT {
            for (SizeType i = pos; i < mLength; ++i) {
                bool found = false;
                for (SizeType j = 0; chars[j] != '\0'; ++j) {
                    if (mData[i] == chars[j]) {
                        found = true;
                        break;
                    }
                }
                if (!found) return i;
            }
            return npos;
        }
        
        NkStringView::SizeType NkStringView::FindFirstNotOf(char c, SizeType pos) const NK_NOEXCEPT {
            for (SizeType i = pos; i < mLength; ++i) {
                if (mData[i] != c) {
                    return i;
                }
            }
            return npos;
        }
        
        NkStringView::SizeType NkStringView::FindLastNotOf(NkStringView chars, SizeType pos) const NK_NOEXCEPT {
            SizeType searchPos = (pos == npos || pos >= mLength) ? mLength - 1 : pos;
            
            for (SizeType i = searchPos + 1; i > 0; --i) {
                SizeType idx = i - 1;
                bool found = false;
                for (SizeType j = 0; j < chars.mLength; ++j) {
                    if (mData[idx] == chars.mData[j]) {
                        found = true;
                        break;
                    }
                }
                if (!found) return idx;
            }
            return npos;
        }
        
        NkStringView::SizeType NkStringView::FindLastNotOf(const char* chars, SizeType pos) const NK_NOEXCEPT {
            SizeType searchPos = (pos == npos || pos >= mLength) ? mLength - 1 : pos;
            
            for (SizeType i = searchPos + 1; i > 0; --i) {
                SizeType idx = i - 1;
                bool found = false;
                for (SizeType j = 0; chars[j] != '\0'; ++j) {
                    if (mData[idx] == chars[j]) {
                        found = true;
                        break;
                    }
                }
                if (!found) return idx;
            }
            return npos;
        }
        
        NkStringView::SizeType NkStringView::FindLastNotOf(char c, SizeType pos) const NK_NOEXCEPT {
            SizeType searchPos = (pos == npos || pos >= mLength) ? mLength - 1 : pos;
            
            for (SizeType i = searchPos + 1; i > 0; --i) {
                if (mData[i - 1] != c) {
                    return i - 1;
                }
            }
            return npos;
        }
        
        NkStringView::SizeType NkStringView::Count(NkStringView str) const NK_NOEXCEPT {
            return string::NkCount(*this, str);
        }
        
        NkStringView::SizeType NkStringView::Count(char c) const NK_NOEXCEPT {
            return string::NkCount(*this, c);
        }
        
        NkStringView::SizeType NkStringView::CountAny(const char* chars) const NK_NOEXCEPT {
            SizeType count = 0;
            for (SizeType i = 0; i < mLength; ++i) {
                for (SizeType j = 0; chars[j] != '\0'; ++j) {
                    if (mData[i] == chars[j]) {
                        ++count;
                        break;
                    }
                }
            }
            return count;
        }
        
        // ========================================
        // CONVERSIONS
        // ========================================
        
        bool NkStringView::ToInt(int& out) const NK_NOEXCEPT {
            return string::NkParseInt(*this, out);
        }
        
        bool NkStringView::ToInt32(int32& out) const NK_NOEXCEPT {
            return string::NkParseInt(*this, out);
        }
        
        bool NkStringView::ToInt64(int64& out) const NK_NOEXCEPT {
            return string::NkParseInt64(*this, out);
        }
        
        bool NkStringView::ToUInt(uint32& out) const NK_NOEXCEPT {
            return string::NkParseUInt(*this, out);
        }
        
        bool NkStringView::ToUInt64(uint64& out) const NK_NOEXCEPT {
            return string::NkParseUInt64(*this, out);
        }
        
        bool NkStringView::ToFloat(float32& out) const NK_NOEXCEPT {
            return string::NkParseFloat(*this, out);
        }
        
        bool NkStringView::ToDouble(float64& out) const NK_NOEXCEPT {
            return string::NkParseDouble(*this, out);
        }
        
        bool NkStringView::ToBool(bool& out) const NK_NOEXCEPT {
            return string::NkParseBool(*this, out);
        }
        
        int NkStringView::ToIntOrDefault(int defaultValue) const NK_NOEXCEPT {
            int result;
            return ToInt(result) ? result : defaultValue;
        }
        
        int32 NkStringView::ToInt32OrDefault(int32 defaultValue) const NK_NOEXCEPT {
            int32 result;
            return ToInt32(result) ? result : defaultValue;
        }
        
        int64 NkStringView::ToInt64OrDefault(int64 defaultValue) const NK_NOEXCEPT {
            int64 result;
            return ToInt64(result) ? result : defaultValue;
        }
        
        uint32 NkStringView::ToUIntOrDefault(uint32 defaultValue) const NK_NOEXCEPT {
            uint32 result;
            return ToUInt(result) ? result : defaultValue;
        }
        
        uint64 NkStringView::ToUInt64OrDefault(uint64 defaultValue) const NK_NOEXCEPT {
            uint64 result;
            return ToUInt64(result) ? result : defaultValue;
        }
        
        float32 NkStringView::ToFloatOrDefault(float32 defaultValue) const NK_NOEXCEPT {
            float32 result;
            return ToFloat(result) ? result : defaultValue;
        }
        
        float64 NkStringView::ToDoubleOrDefault(float64 defaultValue) const NK_NOEXCEPT {
            float64 result;
            return ToDouble(result) ? result : defaultValue;
        }
        
        bool NkStringView::ToBoolOrDefault(bool defaultValue) const NK_NOEXCEPT {
            bool result;
            return ToBool(result) ? result : defaultValue;
        }
        
        NkString NkStringView::ToString() const NK_NOEXCEPT {
            return NkString(mData, mLength);
        }
        
        NkString NkStringView::ToLower() const NK_NOEXCEPT {
            return string::NkToLower(*this);
        }
        
        NkString NkStringView::ToUpper() const NK_NOEXCEPT {
            return string::NkToUpper(*this);
        }
        
        NkString NkStringView::ToCapitalized() const NK_NOEXCEPT {
            return string::NkCapitalize(*this);
        }
        
        NkString NkStringView::ToTitleCase() const NK_NOEXCEPT {
            return string::NkTitleCase(*this);
        }
        
        // ========================================
        // PREDICATES
        // ========================================
        
        bool NkStringView::IsWhitespace() const NK_NOEXCEPT {
            return string::NkIsAllWhitespace(*this);
        }
        
        bool NkStringView::IsDigits() const NK_NOEXCEPT {
            return string::NkIsAllDigits(*this);
        }
        
        bool NkStringView::IsAlpha() const NK_NOEXCEPT {
            return string::NkIsAllAlpha(*this);
        }
        
        bool NkStringView::IsAlphaNumeric() const NK_NOEXCEPT {
            return string::NkIsAllAlphaNumeric(*this);
        }
        
        bool NkStringView::IsHexDigits() const NK_NOEXCEPT {
            return string::NkIsAllHexDigits(*this);
        }
        
        bool NkStringView::IsLower() const NK_NOEXCEPT {
            for (SizeType i = 0; i < mLength; ++i) {
                if (encoding::ascii::NkIsUpper(mData[i])) {
                    return false;
                }
            }
            return true;
        }
        
        bool NkStringView::IsUpper() const NK_NOEXCEPT {
            for (SizeType i = 0; i < mLength; ++i) {
                if (encoding::ascii::NkIsLower(mData[i])) {
                    return false;
                }
            }
            return true;
        }
        
        bool NkStringView::IsPrintable() const NK_NOEXCEPT {
            return string::NkIsAllPrintable(*this);
        }
        
        bool NkStringView::IsNumeric() const NK_NOEXCEPT {
            return string::NkIsNumeric(*this);
        }
        
        bool NkStringView::IsInteger() const NK_NOEXCEPT {
            return string::NkIsInteger(*this);
        }
        
        bool NkStringView::IsFloat() const NK_NOEXCEPT {
            return string::NkIsNumeric(*this) && Contains('.');
        }
        
        bool NkStringView::IsBoolean() const NK_NOEXCEPT {
            NkStringView trimmed = Trimmed();
            return trimmed == "true" || trimmed == "false" ||
                   trimmed == "TRUE" || trimmed == "FALSE" ||
                   trimmed == "True" || trimmed == "False" ||
                   trimmed == "1" || trimmed == "0";
        }
        
        bool NkStringView::IsPalindrome() const NK_NOEXCEPT {
            return string::NkIsPalindrome(*this);
        }
        
        // ========================================
        // UTILITIES
        // ========================================
        
        NkStringView::SizeType NkStringView::Hash() const NK_NOEXCEPT {
            return string::NkHashFNV1a(*this);
        }
        
        NkStringView::SizeType NkStringView::HashIgnoreCase() const NK_NOEXCEPT {
            return string::NkHashFNV1aIgnoreCase(*this);
        }
        
        NkStringView::SizeType NkStringView::HashFNV1a() const NK_NOEXCEPT {
            return string::NkHashFNV1a(*this);
        }
        
        NkStringView::SizeType NkStringView::HashFNV1aIgnoreCase() const NK_NOEXCEPT {
            return string::NkHashFNV1aIgnoreCase(*this);
        }
        
        NkStringView::SizeType NkStringView::LevenshteinDistance(NkStringView other) const NK_NOEXCEPT {
            return string::NkLevenshteinDistance(*this, other);
        }
        
        float64 NkStringView::Similarity(NkStringView other) const NK_NOEXCEPT {
            return string::NkSimilarity(*this, other);
        }
        
        bool NkStringView::MatchesPattern(NkStringView pattern) const NK_NOEXCEPT {
            return string::NkMatchesPattern(*this, pattern);
        }
        
        bool NkStringView::MatchesRegex(NkStringView regex) const NK_NOEXCEPT {
            // Implémentation simple - à étendre avec une vraie regex
            return Find(regex) != npos;
        }
        
        // ========================================
        // TRIM FUNCTIONS
        // ========================================
        
        NkStringView NkStringView::Trimmed() const NK_NOEXCEPT {
            return string::NkTrim(*this);
        }
        
        NkStringView NkStringView::TrimmedLeft() const NK_NOEXCEPT {
            return string::NkTrimLeft(*this);
        }
        
        NkStringView NkStringView::TrimmedRight() const NK_NOEXCEPT {
            return string::NkTrimRight(*this);
        }
        
    
} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-09
// Creation Date: 2026-02-09
// ============================================================
