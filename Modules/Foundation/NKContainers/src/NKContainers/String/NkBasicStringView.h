// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\NkBasicStringView.h
// DESCRIPTION: Generic string view template (encoding-aware)
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_STRING_NKBASICSTRINGVIEW_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_STRING_NKBASICSTRINGVIEW_H_INCLUDED

#include "NKContainers/NkCompat.h"
#include "NKCore/NkTypes.h"
#include "NKMemory/NkMemoryFn.h"

namespace nkentseu {
    namespace core {
        
        /**
         * @brief Generic string view (non-owning, read-only)
         * 
         * Vue sur chaîne avec support multi-encoding.
         * 
         * @tparam CharT Type de caractère (char, char16_t, char32_t, wchar_t)
         */
        template<typename CharT>
        class BasicStringView {
            public:
                using CharType = CharT;
                using SizeType = usize;
                static constexpr SizeType npos = static_cast<SizeType>(-1);
                
                // ========================================
                // CONSTRUCTORS
                // ========================================
                
                NK_CONSTEXPR BasicStringView() NK_NOEXCEPT
                    : mData(nullptr), mLength(0) {}
                
                NK_CONSTEXPR BasicStringView(const CharT* str) NK_NOEXCEPT
                    : mData(str), mLength(str ? CalculateLength(str) : 0) {}
                
                NK_CONSTEXPR BasicStringView(const CharT* str, SizeType length) NK_NOEXCEPT
                    : mData(str), mLength(length) {}
                
                NK_CONSTEXPR BasicStringView(const BasicStringView&) NK_NOEXCEPT = default;
                BasicStringView& operator=(const BasicStringView&) NK_NOEXCEPT = default;
                
                // ========================================
                // ELEMENT ACCESS
                // ========================================
                
                NK_CONSTEXPR CharT operator[](SizeType index) const NK_NOEXCEPT {
                    return mData[index];
                }
                
                NK_CONSTEXPR CharT At(SizeType index) const {
                    NK_ASSERT(index < mLength);
                    return mData[index];
                }
                
                NK_CONSTEXPR CharT Front() const NK_NOEXCEPT {
                    return mData[0];
                }
                
                NK_CONSTEXPR CharT Back() const NK_NOEXCEPT {
                    return mData[mLength - 1];
                }
                
                NK_CONSTEXPR const CharT* Data() const NK_NOEXCEPT {
                    return mData;
                }
                
                // ========================================
                // CAPACITY
                // ========================================
                
                NK_CONSTEXPR SizeType Length() const NK_NOEXCEPT {
                    return mLength;
                }
                
                NK_CONSTEXPR SizeType Size() const NK_NOEXCEPT {
                    return mLength;
                }
                
                NK_CONSTEXPR bool IsEmpty() const NK_NOEXCEPT {
                    return mLength == 0;
                }
                
                // ========================================
                // MODIFIERS
                // ========================================
                
                NK_CONSTEXPR void RemovePrefix(SizeType n) NK_NOEXCEPT {
                    mData += n;
                    mLength -= n;
                }
                
                NK_CONSTEXPR void RemoveSuffix(SizeType n) NK_NOEXCEPT {
                    mLength -= n;
                }
                
                NK_CONSTEXPR BasicStringView SubStr(SizeType pos = 0, SizeType count = npos) const {
                    SizeType rcount = (count == npos || pos + count > mLength) 
                        ? mLength - pos : count;
                    return BasicStringView(mData + pos, rcount);
                }
                
                void Swap(BasicStringView& other) NK_NOEXCEPT {
                    const CharT* tmpData = mData;
                    mData = other.mData;
                    other.mData = tmpData;
                    
                    SizeType tmpLen = mLength;
                    mLength = other.mLength;
                    other.mLength = tmpLen;
                }
                
                // ========================================
                // OPERATIONS
                // ========================================
                
                int Compare(BasicStringView other) const NK_NOEXCEPT {
                    SizeType minLen = mLength < other.mLength ? mLength : other.mLength;
                    
                    for (SizeType i = 0; i < minLen; ++i) {
                        if (mData[i] < other.mData[i]) return -1;
                        if (mData[i] > other.mData[i]) return 1;
                    }
                    
                    if (mLength < other.mLength) return -1;
                    if (mLength > other.mLength) return 1;
                    return 0;
                }
                
                bool StartsWith(BasicStringView prefix) const NK_NOEXCEPT {
                    if (prefix.mLength > mLength) return false;
                    return SubStr(0, prefix.mLength).Compare(prefix) == 0;
                }
                
                bool StartsWith(CharT ch) const NK_NOEXCEPT {
                    return mLength > 0 && mData[0] == ch;
                }
                
                bool EndsWith(BasicStringView suffix) const NK_NOEXCEPT {
                    if (suffix.mLength > mLength) return false;
                    return SubStr(mLength - suffix.mLength).Compare(suffix) == 0;
                }
                
                bool EndsWith(CharT ch) const NK_NOEXCEPT {
                    return mLength > 0 && mData[mLength - 1] == ch;
                }
                
                bool Contains(BasicStringView str) const NK_NOEXCEPT {
                    return Find(str) != npos;
                }
                
                bool Contains(CharT ch) const NK_NOEXCEPT {
                    return Find(ch) != npos;
                }
                
                SizeType Find(BasicStringView str, SizeType pos = 0) const NK_NOEXCEPT {
                    if (str.mLength == 0) return pos;
                    if (pos > mLength) return npos;
                    if (str.mLength > mLength - pos) return npos;
                    
                    for (SizeType i = pos; i <= mLength - str.mLength; ++i) {
                        bool match = true;
                        for (SizeType j = 0; j < str.mLength; ++j) {
                            if (mData[i + j] != str.mData[j]) {
                                match = false;
                                break;
                            }
                        }
                        if (match) return i;
                    }
                    return npos;
                }
                
                SizeType Find(CharT ch, SizeType pos = 0) const NK_NOEXCEPT {
                    for (SizeType i = pos; i < mLength; ++i) {
                        if (mData[i] == ch) return i;
                    }
                    return npos;
                }
                
                SizeType RFind(CharT ch, SizeType pos = npos) const NK_NOEXCEPT {
                    if (mLength == 0) return npos;
                    SizeType searchPos = (pos >= mLength) ? mLength - 1 : pos;
                    
                    for (SizeType i = searchPos + 1; i > 0; --i) {
                        if (mData[i - 1] == ch) return i - 1;
                    }
                    return npos;
                }
                
                // ========================================
                // ITERATORS
                // ========================================
                
                const CharT* begin() const NK_NOEXCEPT { return mData; }
                const CharT* end() const NK_NOEXCEPT { return mData + mLength; }
                
            private:
                const CharT* mData;
                SizeType mLength;
                
                static NK_CONSTEXPR SizeType CalculateLength(const CharT* str) NK_NOEXCEPT {
                    SizeType len = 0;
                    while (str && str[len] != CharT(0)) {
                        ++len;
                    }
                    return len;
                }
        };
        
        // ========================================
        // TYPE ALIASES
        // ========================================
        
        using StringView8 = BasicStringView<char>;
        using StringView16 = BasicStringView<char16_t>;
        using StringView32 = BasicStringView<char32_t>;
        using WStringView = BasicStringView<wchar_t>;
        
        // using StringView = StringView8;  // UTF-8 par défaut
        
        // ========================================
        // COMPARISON OPERATORS
        // ========================================
        
        template<typename CharT>
        bool operator==(BasicStringView<CharT> lhs, BasicStringView<CharT> rhs) NK_NOEXCEPT {
            return lhs.Compare(rhs) == 0;
        }
        
        template<typename CharT>
        bool operator!=(BasicStringView<CharT> lhs, BasicStringView<CharT> rhs) NK_NOEXCEPT {
            return lhs.Compare(rhs) != 0;
        }
        
        template<typename CharT>
        bool operator<(BasicStringView<CharT> lhs, BasicStringView<CharT> rhs) NK_NOEXCEPT {
            return lhs.Compare(rhs) < 0;
        }
        
    } // namespace core
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_STRING_NKBASICSTRINGVIEW_H_INCLUDED