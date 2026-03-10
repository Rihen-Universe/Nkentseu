// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\NkBasicString.h
// DESCRIPTION: Generic string template (encoding-aware) - INLINE IMPLEMENTATION
// AUTEUR: Rihen
// DATE: 2026-02-08
// VERSION: 1.0.1 (Fixed with inline impl)
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_STRING_NKBASICSTRING_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_STRING_NKBASICSTRING_H_INCLUDED

#include "NKContainers/NkCompat.h"
#include "NKCore/NkTypes.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"
#include "Encoding/NkEncoding.h"

namespace nkentseu {
    

        // ========================================
        // CHARACTER TRAITS (inline implementation)
        // ========================================

        template<typename CharT>
        struct NkCharTraits {
            using CharType = CharT;
            
            static constexpr CharT Null() { return CharT(0); }
            
            static inline usize Length(const CharT* str) {
                if (!str) return 0;
                usize len = 0;
                while (str[len] != CharT(0)) ++len;
                return len;
            }
            
            static inline int32 Compare(const CharT* s1, const CharT* s2, usize count) {
                for (usize i = 0; i < count; ++i) {
                    if (s1[i] < s2[i]) return -1;
                    if (s1[i] > s2[i]) return 1;
                }
                return 0;
            }
            
            static inline CharT* Copy(CharT* dest, const CharT* src, usize count) {
                for (usize i = 0; i < count; ++i) dest[i] = src[i];
                return dest;
            }
            
            static inline CharT* Move(CharT* dest, const CharT* src, usize count) {
                if (dest < src) {
                    for (usize i = 0; i < count; ++i) dest[i] = src[i];
                } else if (dest > src) {
                    for (usize i = count; i > 0; --i) dest[i-1] = src[i-1];
                }
                return dest;
            }
            
            static inline CharT* Fill(CharT* dest, CharT ch, usize count) {
                for (usize i = 0; i < count; ++i) dest[i] = ch;
                return dest;
            }
            
            static inline const CharT* Find(const CharT* str, usize count, CharT ch) {
                for (usize i = 0; i < count; ++i) {
                    if (str[i] == ch) return str + i;
                }
                return nullptr;
            }
        };

        // ========================================
        // BASIC STRING (template with inline impl)
        // ========================================

        template<typename CharT, typename Traits = NkCharTraits<CharT>>
        class NkBasicString {
            public:
                using CharType = CharT;
                using TraitsType = Traits;
                using SizeType = usize;
                
                static constexpr SizeType npos = static_cast<SizeType>(-1);
                static constexpr SizeType SSO_CAPACITY = 
                    (sizeof(CharT*) + sizeof(SizeType) * 2) / sizeof(CharT) - 1;
                
                // ========================================
                // CONSTRUCTORS (inline)
                // ========================================
                
                NkBasicString() 
                    : mAllocator(&memory::NkGetDefaultAllocator())
                    , mLength(0), mCapacity(SSO_CAPACITY)
                {
                    mSSOData[0] = Traits::Null();
                    SetSSO(true);
                }
                
                explicit NkBasicString(memory::NkIAllocator& allocator)
                    : mAllocator(&allocator)
                    , mLength(0), mCapacity(SSO_CAPACITY)
                {
                    mSSOData[0] = Traits::Null();
                    SetSSO(true);
                }
                
                NkBasicString(const CharT* str) : NkBasicString() {
                    if (str) Append(str);
                }
                
                NkBasicString(const CharT* str, SizeType length) : NkBasicString() {
                    if (str && length > 0) Append(str, length);
                }
                
                NkBasicString(const CharT* str, memory::NkIAllocator& allocator) 
                    : NkBasicString(allocator) {
                    if (str) Append(str);
                }
                
                NkBasicString(SizeType count, CharT ch) : NkBasicString() {
                    if (count > 0) Append(count, ch);
                }
                
                NkBasicString(SizeType count, CharT ch, memory::NkIAllocator& allocator)
                    : NkBasicString(allocator) {
                    if (count > 0) Append(count, ch);
                }
                
                NkBasicString(const NkBasicString& other)
                    : mAllocator(other.mAllocator)
                    , mLength(other.mLength)
                    , mCapacity(other.mCapacity)
                {
                    if (other.IsSSO()) {
                        Traits::Copy(mSSOData, other.mSSOData, mLength + 1);
                        SetSSO(true);
                    } else {
                        AllocateHeap(mLength);
                        Traits::Copy(mHeapData, other.mHeapData, mLength + 1);
                    }
                }
                
                NkBasicString(NkBasicString&& other) NK_NOEXCEPT
                    : mAllocator(other.mAllocator)
                    , mLength(other.mLength)
                    , mCapacity(other.mCapacity)
                {
                    if (other.IsSSO()) {
                        Traits::Copy(mSSOData, other.mSSOData, mLength + 1);
                        SetSSO(true);
                    } else {
                        mHeapData = other.mHeapData;
                        other.mHeapData = nullptr;
                        SetSSO(false);
                    }
                    other.mLength = 0;
                    other.mCapacity = SSO_CAPACITY;
                    other.mSSOData[0] = Traits::Null();
                    other.SetSSO(true);
                }
                
                ~NkBasicString() {
                    if (!IsSSO()) DeallocateHeap();
                }
                
                // ========================================
                // ASSIGNMENT (inline)
                // ========================================
                
                NkBasicString& operator=(const NkBasicString& other) {
                    if (this != &other) {
                        Clear();
                        Append(other);
                    }
                    return *this;
                }
                
                NkBasicString& operator=(NkBasicString&& other) NK_NOEXCEPT {
                    if (this != &other) {
                        if (!IsSSO()) DeallocateHeap();
                        mLength = other.mLength;
                        mCapacity = other.mCapacity;
                        if (other.IsSSO()) {
                            Traits::Copy(mSSOData, other.mSSOData, mLength + 1);
                            SetSSO(true);
                        } else {
                            mHeapData = other.mHeapData;
                            other.mHeapData = nullptr;
                            SetSSO(false);
                        }
                        other.mLength = 0;
                        other.mCapacity = SSO_CAPACITY;
                        other.mSSOData[0] = Traits::Null();
                        other.SetSSO(true);
                    }
                    return *this;
                }
                
                NkBasicString& operator=(const CharT* str) {
                    Clear();
                    if (str) Append(str);
                    return *this;
                }
                
                NkBasicString& operator=(CharT ch) {
                    Clear();
                    Append(ch);
                    return *this;
                }
                
                // ========================================
                // ELEMENT ACCESS (inline)
                // ========================================
                
                CharT& operator[](SizeType index) {
                    NK_ASSERT(index < mLength);
                    return GetData()[index];
                }
                
                const CharT& operator[](SizeType index) const {
                    NK_ASSERT(index < mLength);
                    return GetData()[index];
                }
                
                CharT& At(SizeType index) {
                    NK_ASSERT(index < mLength);
                    return GetData()[index];
                }
                
                const CharT& At(SizeType index) const {
                    NK_ASSERT(index < mLength);
                    return GetData()[index];
                }
                
                CharT& Front() { NK_ASSERT(mLength > 0); return GetData()[0]; }
                const CharT& Front() const { NK_ASSERT(mLength > 0); return GetData()[0]; }
                CharT& Back() { NK_ASSERT(mLength > 0); return GetData()[mLength - 1]; }
                const CharT& Back() const { NK_ASSERT(mLength > 0); return GetData()[mLength - 1]; }
                
                const CharT* Data() const NK_NOEXCEPT { return GetData(); }
                CharT* Data() NK_NOEXCEPT { return GetData(); }
                const CharT* CStr() const NK_NOEXCEPT { return GetData(); }
                
                // ========================================
                // CAPACITY (inline)
                // ========================================
                
                SizeType Length() const NK_NOEXCEPT { return mLength; }
                SizeType Size() const NK_NOEXCEPT { return mLength; }
                SizeType Capacity() const NK_NOEXCEPT { return mCapacity; }
                bool Empty() const NK_NOEXCEPT { return mLength == 0; }
                
                void Reserve(SizeType newCapacity) {
                    if (newCapacity <= mCapacity) return;
                    CharT* newData = static_cast<CharT*>(
                        mAllocator->Allocate((newCapacity + 1) * sizeof(CharT))
                    );
                    Traits::Copy(newData, GetData(), mLength + 1);
                    if (!IsSSO()) mAllocator->Deallocate(mHeapData);
                    mHeapData = newData;
                    mCapacity = newCapacity;
                    SetSSO(false);
                }
                
                void ShrinkToFit() {
                    if (IsSSO() || mLength >= mCapacity) return;
                    if (mLength <= SSO_CAPACITY) {
                        CharT temp[SSO_CAPACITY + 1];
                        Traits::Copy(temp, mHeapData, mLength + 1);
                        DeallocateHeap();
                        Traits::Copy(mSSOData, temp, mLength + 1);
                        mCapacity = SSO_CAPACITY;
                        SetSSO(true);
                    }
                }
                
                void Clear() NK_NOEXCEPT {
                    mLength = 0;
                    GetData()[0] = Traits::Null();
                }
                
                void Resize(SizeType count) { Resize(count, Traits::Null()); }
                
                void Resize(SizeType count, CharT ch) {
                    if (count < mLength) {
                        mLength = count;
                        GetData()[mLength] = Traits::Null();
                    } else if (count > mLength) {
                        GrowIfNeeded(count - mLength);
                        Traits::Fill(GetData() + mLength, ch, count - mLength);
                        mLength = count;
                        GetData()[mLength] = Traits::Null();
                    }
                }
                
                // ========================================
                // MODIFIERS (inline)
                // ========================================
                
                NkBasicString& Append(const CharT* str) {
                    if (!str) return *this;
                    return Append(str, Traits::Length(str));
                }
                
                NkBasicString& Append(const CharT* str, SizeType length) {
                    if (!str || length == 0) return *this;
                    GrowIfNeeded(length);
                    Traits::Copy(GetData() + mLength, str, length);
                    mLength += length;
                    GetData()[mLength] = Traits::Null();
                    return *this;
                }
                
                NkBasicString& Append(const NkBasicString& str) {
                    return Append(str.Data(), str.Length());
                }
                
                NkBasicString& Append(SizeType count, CharT ch) {
                    if (count == 0) return *this;
                    GrowIfNeeded(count);
                    Traits::Fill(GetData() + mLength, ch, count);
                    mLength += count;
                    GetData()[mLength] = Traits::Null();
                    return *this;
                }
                
                NkBasicString& Append(CharT ch) {
                    GrowIfNeeded(1);
                    GetData()[mLength++] = ch;
                    GetData()[mLength] = Traits::Null();
                    return *this;
                }
                
                NkBasicString& operator+=(const CharT* str) { return Append(str); }
                NkBasicString& operator+=(const NkBasicString& str) { return Append(str); }
                NkBasicString& operator+=(CharT ch) { return Append(ch); }
                
                NkBasicString& Insert(SizeType pos, const CharT* str) {
                    if (!str) return *this;
                    SizeType len = Traits::Length(str);
                    if (len == 0) return *this;
                    NK_ASSERT(pos <= mLength);
                    GrowIfNeeded(len);
                    CharT* data = GetData();
                    Traits::Move(data + pos + len, data + pos, mLength - pos);
                    Traits::Copy(data + pos, str, len);
                    mLength += len;
                    data[mLength] = Traits::Null();
                    return *this;
                }
                
                NkBasicString& Insert(SizeType pos, const NkBasicString& str) {
                    return Insert(pos, str.Data());
                }
                
                NkBasicString& Insert(SizeType pos, SizeType count, CharT ch) {
                    NK_ASSERT(pos <= mLength);
                    if (count == 0) return *this;
                    GrowIfNeeded(count);
                    CharT* data = GetData();
                    Traits::Move(data + pos + count, data + pos, mLength - pos);
                    Traits::Fill(data + pos, ch, count);
                    mLength += count;
                    data[mLength] = Traits::Null();
                    return *this;
                }
                
                NkBasicString& Erase(SizeType pos = 0, SizeType count = npos) {
                    NK_ASSERT(pos <= mLength);
                    if (pos == mLength) return *this;
                    if (count == npos || pos + count >= mLength) {
                        count = mLength - pos;
                    }
                    CharT* data = GetData();
                    Traits::Move(data + pos, data + pos + count, mLength - pos - count + 1);
                    mLength -= count;
                    return *this;
                }
                
                void PushBack(CharT ch) { Append(ch); }
                void PopBack() {
                    NK_ASSERT(mLength > 0);
                    --mLength;
                    GetData()[mLength] = Traits::Null();
                }
                
                void Swap(NkBasicString& other) NK_NOEXCEPT {
                    NkBasicString temp(static_cast<NkBasicString&&>(*this));
                    *this = static_cast<NkBasicString&&>(other);
                    other = static_cast<NkBasicString&&>(temp);
                }
                
                // ========================================
                // STRING OPERATIONS (inline)
                // ========================================
                
                NkBasicString SubStr(SizeType pos = 0, SizeType count = npos) const {
                    NK_ASSERT(pos <= mLength);
                    if (count == npos || pos + count > mLength) {
                        count = mLength - pos;
                    }
                    return NkBasicString(GetData() + pos, count);
                }
                
                int Compare(const NkBasicString& other) const NK_NOEXCEPT {
                    SizeType minLen = mLength < other.mLength ? mLength : other.mLength;
                    int result = Traits::Compare(GetData(), other.GetData(), minLen);
                    if (result != 0) return result;
                    if (mLength < other.mLength) return -1;
                    if (mLength > other.mLength) return 1;
                    return 0;
                }
                
                int Compare(const CharT* str) const NK_NOEXCEPT {
                    if (!str) return 1;
                    return Compare(NkBasicString(str));
                }
                
                SizeType Find(const CharT* str, SizeType pos = 0) const NK_NOEXCEPT {
                    if (!str) return npos;
                    SizeType len = Traits::Length(str);
                    if (len == 0) return pos;
                    if (pos > mLength || len > mLength - pos) return npos;
                    const CharT* data = GetData();
                    for (SizeType i = pos; i <= mLength - len; ++i) {
                        if (Traits::Compare(data + i, str, len) == 0) return i;
                    }
                    return npos;
                }
                
                SizeType Find(CharT ch, SizeType pos = 0) const NK_NOEXCEPT {
                    const CharT* found = Traits::Find(GetData() + pos, mLength - pos, ch);
                    return found ? static_cast<SizeType>(found - GetData()) : npos;
                }
                
                SizeType RFind(CharT ch, SizeType pos = npos) const NK_NOEXCEPT {
                    if (mLength == 0) return npos;
                    SizeType searchPos = (pos >= mLength) ? mLength - 1 : pos;
                    const CharT* data = GetData();
                    for (SizeType i = searchPos + 1; i > 0; --i) {
                        if (data[i - 1] == ch) return i - 1;
                    }
                    return npos;
                }
                
                bool StartsWith(const CharT* prefix) const NK_NOEXCEPT {
                    if (!prefix) return false;
                    SizeType len = Traits::Length(prefix);
                    if (len > mLength) return false;
                    return Traits::Compare(GetData(), prefix, len) == 0;
                }
                
                bool EndsWith(const CharT* suffix) const NK_NOEXCEPT {
                    if (!suffix) return false;
                    SizeType len = Traits::Length(suffix);
                    if (len > mLength) return false;
                    return Traits::Compare(GetData() + mLength - len, suffix, len) == 0;
                }
                
                bool Contains(CharT ch) const NK_NOEXCEPT {
                    return Find(ch) != npos;
                }
                
                // ========================================
                // ENCODING-SPECIFIC
                // ========================================
                
                SizeType CodeUnitCount() const NK_NOEXCEPT { return Length(); }
                SizeType CodePointCount() const NK_NOEXCEPT { return mLength; }
                
            private:
                memory::NkIAllocator* mAllocator;
                SizeType mLength;
                SizeType mCapacity;
                
                union {
                    CharT* mHeapData;
                    CharT mSSOData[SSO_CAPACITY + 1];
                };
                
                bool IsSSO() const NK_NOEXCEPT { return mCapacity == SSO_CAPACITY; }
                void SetSSO(bool sso) NK_NOEXCEPT { if (sso) mCapacity = SSO_CAPACITY; }
                
                CharT* GetData() NK_NOEXCEPT { return IsSSO() ? mSSOData : mHeapData; }
                const CharT* GetData() const NK_NOEXCEPT { return IsSSO() ? mSSOData : mHeapData; }
                
                void AllocateHeap(SizeType capacity) {
                    mHeapData = static_cast<CharT*>(
                        mAllocator->Allocate((capacity + 1) * sizeof(CharT))
                    );
                    mCapacity = capacity;
                    SetSSO(false);
                }
                
                void DeallocateHeap() {
                    if (!IsSSO() && mHeapData) {
                        mAllocator->Deallocate(mHeapData);
                        mHeapData = nullptr;
                    }
                }
                
                void GrowIfNeeded(SizeType additionalSize) {
                    SizeType needed = mLength + additionalSize;
                    if (needed <= mCapacity) return;
                    SizeType newCapacity = mCapacity + (mCapacity / 2);
                    if (newCapacity < needed) newCapacity = needed;
                    Reserve(newCapacity);
                }
        };

        // ========================================
        // TYPE ALIASES
        // ========================================

        using String8 = NkBasicString<Char>;
        using String16 = NkBasicString<char16>;
        using String32 = NkBasicString<char32>;
        using WString = NkBasicString<wchar>;
        // using String = String8;  // UTF-8 par défaut

        // ========================================
        // CONVERSION HELPERS
        // ========================================

        NKENTSEU_CORE_API String16 NkToUTF16(const String8& utf8Str);
        NKENTSEU_CORE_API String32 NkToUTF32(const String8& utf8Str);
        NKENTSEU_CORE_API String8 NkToUTF8(const String16& utf16Str);
        NKENTSEU_CORE_API String8 NkToUTF8(const String32& utf32Str);
        NKENTSEU_CORE_API WString NkToWide(const String8& str);
        NKENTSEU_CORE_API String8 NkFromWide(const WString& wstr);

    
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_STRING_NKBASICSTRING_H_INCLUDED // NK_CORE_NKCORE_SRC_NKCORE_STRING_NKBASICSTRING_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-07
// Creation Date: 2026-02-07
// ============================================================