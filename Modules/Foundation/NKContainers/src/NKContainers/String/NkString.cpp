// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\NkString.cpp
// DESCRIPTION: Dynamic string implementation (PARTIE 1/2)
// AUTEUR: Rihen
// DATE: 2026-02-07
// -----------------------------------------------------------------------------

#include "NkString.h"
#include "NkStringUtils.h"
#include "NkStringHash.h"
#include "NKMemory/NkMemoryFn.h"

#include <cstdarg>  // Pour va_list, va_start, va_end

namespace nkentseu {
    namespace core {

        // ========================================
        // CONSTRUCTORS
        // ========================================

        NkString::NkString()
            : mAllocator(&memory::NkGetDefaultAllocator())
            , mLength(0)
            , mCapacity(SSO_SIZE)
        {
            mSSOData[0] = '\0';
            SetSSO(true);
        }

        NkString::NkString(memory::NkIAllocator& allocator)
            : mAllocator(&allocator)
            , mLength(0)
            , mCapacity(SSO_SIZE)
        {
            mSSOData[0] = '\0';
            SetSSO(true);
        }

        NkString::NkString(const Char* str)
            : mAllocator(&memory::NkGetDefaultAllocator())
            , mLength(0)
            , mCapacity(SSO_SIZE)
        {
            mSSOData[0] = '\0';
            SetSSO(true);
            
            if (str) {
                SizeType len = string::NkLeng(str);
                if (len > 0) {
                    if (len <= SSO_SIZE) {
                        memory::NkMemCopy(mSSOData, str, len);
                        mSSOData[len] = '\0';
                        mLength = len;
                    } else {
                        MoveToHeap(len);
                        memory::NkMemCopy(mHeapData, str, len);
                        mHeapData[len] = '\0';
                        mLength = len;
                    }
                }
            }
        }

        NkString::NkString(const Char* str, memory::NkIAllocator& allocator)
            : NkString(allocator)
        {
            if (str) {
                Append(str);
            }
        }

        NkString::NkString(const Char* str, SizeType length)
            : mAllocator(&memory::NkGetDefaultAllocator())
            , mLength(0)
            , mCapacity(SSO_SIZE)
        {
            mSSOData[0] = '\0';
            SetSSO(true);
            
            if (str && length > 0) {
                if (length <= SSO_SIZE) {
                    memory::NkMemCopy(mSSOData, str, length);
                    mSSOData[length] = '\0';
                    mLength = length;
                } else {
                    MoveToHeap(length);
                    memory::NkMemCopy(mHeapData, str, length);
                    mHeapData[length] = '\0';
                    mLength = length;
                }
            }
        }

        NkString::NkString(const Char* str, SizeType length, memory::NkIAllocator& allocator)
            : NkString(allocator)
        {
            if (str && length > 0) {
                Append(str, length);
            }
        }

        NkString::NkString(NkStringView view)
            : NkString(view.Data(), view.Length())
        {}

        NkString::NkString(NkStringView view, memory::NkIAllocator& allocator)
            : NkString(view.Data(), view.Length(), allocator)
        {}

        NkString::NkString(SizeType count, Char ch)
            : mAllocator(&memory::NkGetDefaultAllocator())
            , mLength(0)
            , mCapacity(SSO_SIZE)
        {
            mSSOData[0] = '\0';
            SetSSO(true);
            
            if (count > 0) {
                if (count <= SSO_SIZE) {
                    memory::NkMemSet(mSSOData, ch, count);
                    mSSOData[count] = '\0';
                    mLength = count;
                } else {
                    MoveToHeap(count);
                    memory::NkMemSet(mHeapData, ch, count);
                    mHeapData[count] = '\0';
                    mLength = count;
                }
            }
        }

        NkString::NkString(SizeType count, Char ch, memory::NkIAllocator& allocator)
            : NkString(allocator)
        {
            Append(count, ch);
        }

        NkString::NkString(const NkString& other)
            : mAllocator(other.mAllocator)
            , mLength(other.mLength)
            , mCapacity(other.mCapacity)
        {
            if (other.IsSSO()) {
                memory::NkMemCopy(mSSOData, other.mSSOData, mLength + 1);
                SetSSO(true);
            } else {
                AllocateHeap(mLength);
                memory::NkMemCopy(mHeapData, other.mHeapData, mLength + 1);
            }
        }

        NkString::NkString(const NkString& other, memory::NkIAllocator& allocator)
            : mAllocator(&allocator)
            , mLength(0)
            , mCapacity(SSO_SIZE)
        {
            mSSOData[0] = '\0';
            SetSSO(true);
            Append(other);
        }

        NkString::NkString(NkString&& other) NK_NOEXCEPT
            : mAllocator(other.mAllocator)
            , mLength(other.mLength)
            , mCapacity(other.mCapacity)
        {
            if (other.IsSSO()) {
                memory::NkMemCopy(mSSOData, other.mSSOData, mLength + 1);
                SetSSO(true);
            } else {
                mHeapData = other.mHeapData;
                other.mHeapData = nullptr;
                SetSSO(false);
            }
            
            other.mLength = 0;
            other.mCapacity = SSO_SIZE;
            other.mSSOData[0] = '\0';
            other.SetSSO(true);
        }

        NkString::~NkString() {
            if (!IsSSO()) {
                DeallocateHeap();
            }
        }

        // ========================================
        // ASSIGNMENT
        // ========================================

        NkString& NkString::operator=(const NkString& other) {
            if (this != &other) {
                Clear();
                Append(other);
            }
            return *this;
        }

        NkString& NkString::operator=(NkString&& other) NK_NOEXCEPT {
            if (this != &other) {
                if (!IsSSO()) {
                    DeallocateHeap();
                }
                
                mLength = other.mLength;
                mCapacity = other.mCapacity;
                
                if (other.IsSSO()) {
                    memory::NkMemCopy(mSSOData, other.mSSOData, mLength + 1);
                    SetSSO(true);
                } else {
                    mHeapData = other.mHeapData;
                    other.mHeapData = nullptr;
                    SetSSO(false);
                }
                
                other.mLength = 0;
                other.mCapacity = SSO_SIZE;
                other.mSSOData[0] = '\0';
                other.SetSSO(true);
            }
            return *this;
        }

        NkString& NkString::operator=(const Char* str) {
            Clear();
            if (str) {
                Append(str);
            }
            return *this;
        }

        NkString& NkString::operator=(NkStringView view) {
            Clear();
            Append(view);
            return *this;
        }

        NkString& NkString::operator=(Char ch) {
            Clear();
            Append(ch);
            return *this;
        }
        // NkString.cpp PARTIE 2/3 - Element Access, Capacity, Modifiers

        // ========================================
        // ELEMENT ACCESS
        // ========================================

        Char& NkString::operator[](SizeType index) {
            NK_ASSERT(index < mLength);
            return GetData()[index];
        }

        const Char& NkString::operator[](SizeType index) const {
            NK_ASSERT(index < mLength);
            return GetData()[index];
        }

        Char& NkString::At(SizeType index) {
            NK_ASSERT_MSG(index < mLength, "String index out of bounds");
            return GetData()[index];
        }

        const Char& NkString::At(SizeType index) const {
            NK_ASSERT_MSG(index < mLength, "String index out of bounds");
            return GetData()[index];
        }

        Char& NkString::Front() {
            NK_ASSERT(mLength > 0);
            return GetData()[0];
        }

        const Char& NkString::Front() const {
            NK_ASSERT(mLength > 0);
            return GetData()[0];
        }

        Char& NkString::Back() {
            NK_ASSERT(mLength > 0);
            return GetData()[mLength - 1];
        }

        const Char& NkString::Back() const {
            NK_ASSERT(mLength > 0);
            return GetData()[mLength - 1];
        }

        const Char* NkString::Data() const NK_NOEXCEPT {
            return GetData();
        }

        Char* NkString::Data() NK_NOEXCEPT {
            return GetData();
        }

        const Char* NkString::CStr() const NK_NOEXCEPT {
            return GetData();
        }

        // ========================================
        // CAPACITY
        // ========================================

        NkString::SizeType NkString::Length() const NK_NOEXCEPT {
            return mLength;
        }

        NkString::SizeType NkString::Size() const NK_NOEXCEPT {
            return mLength;
        }

        NkString::SizeType NkString::Capacity() const NK_NOEXCEPT {
            return mCapacity;
        }

        Bool NkString::IsEmpty() const NK_NOEXCEPT {
            return mLength == 0;
        }

        void NkString::Reserve(SizeType newCapacity) {
            if (newCapacity <= mCapacity) return;
            
            if (IsSSO()) {
                if (newCapacity <= SSO_SIZE) return;
                MoveToHeap(newCapacity);
            } else {
                Char* newData = static_cast<Char*>(
                    mAllocator->Allocate((newCapacity + 1) * sizeof(Char))
                );
                memory::NkMemCopy(newData, mHeapData, mLength + 1);
                mAllocator->Deallocate(mHeapData);
                mHeapData = newData;
                mCapacity = newCapacity;
            }
        }

        void NkString::ShrinkToFit() {
            if (IsSSO()) return;
            if (mLength <= SSO_SIZE) {
                Char temp[SSO_SIZE + 1];
                memory::NkMemCopy(temp, mHeapData, mLength + 1);
                DeallocateHeap();
                memory::NkMemCopy(mSSOData, temp, mLength + 1);
                mCapacity = SSO_SIZE;
                SetSSO(true);
            } else if (mLength < mCapacity) {
                Char* newData = static_cast<Char*>(
                    mAllocator->Allocate((mLength + 1) * sizeof(Char))
                );
                memory::NkMemCopy(newData, mHeapData, mLength + 1);
                mAllocator->Deallocate(mHeapData);
                mHeapData = newData;
                mCapacity = mLength;
            }
        }

        void NkString::Clear() NK_NOEXCEPT {
            mLength = 0;
            GetData()[0] = '\0';
        }

        // ========================================
        // MODIFIERS - APPEND
        // ========================================

        NkString& NkString::Append(const Char* str) {
            if (!str) return *this;
            return Append(str, string::NkLeng(str));
        }

        NkString& NkString::Append(const Char* str, SizeType length) {
            if (!str || length == 0) return *this;
            
            GrowIfNeeded(length);
            memory::NkMemCopy(GetData() + mLength, str, length);
            mLength += length;
            GetData()[mLength] = '\0';
            
            return *this;
        }

        NkString& NkString::Append(const NkString& str) {
            return Append(str.Data(), str.Length());
        }

        NkString& NkString::Append(NkStringView view) {
            return Append(view.Data(), view.Length());
        }

        NkString& NkString::Append(SizeType count, Char ch) {
            if (count == 0) return *this;
            
            GrowIfNeeded(count);
            memory::NkMemSet(GetData() + mLength, ch, count);
            mLength += count;
            GetData()[mLength] = '\0';
            
            return *this;
        }

        NkString& NkString::Append(Char ch) {
            GrowIfNeeded(1);
            GetData()[mLength++] = ch;
            GetData()[mLength] = '\0';
            return *this;
        }

        NkString& NkString::operator+=(const Char* str) { return Append(str); }
        NkString& NkString::operator+=(const NkString& str) { return Append(str); }
        NkString& NkString::operator+=(NkStringView view) { return Append(view); }
        NkString& NkString::operator+=(Char ch) { return Append(ch); }

        // ========================================
        // MODIFIERS - INSERT
        // ========================================

        NkString& NkString::Insert(SizeType pos, const Char* str) {
            if (!str) return *this;
            return Insert(pos, NkStringView(str));
        }

        NkString& NkString::Insert(SizeType pos, const NkString& str) {
            return Insert(pos, str.View());
        }

        NkString& NkString::Insert(SizeType pos, NkStringView view) {
            NK_ASSERT(pos <= mLength);
            if (view.IsEmpty()) return *this;
            
            GrowIfNeeded(view.Length());
            Char* data = GetData();
            memory::NkMemMove(data + pos + view.Length(), data + pos, mLength - pos);
            memory::NkMemCopy(data + pos, view.Data(), view.Length());
            mLength += view.Length();
            data[mLength] = '\0';
            
            return *this;
        }

        NkString& NkString::Insert(SizeType pos, SizeType count, Char ch) {
            NK_ASSERT(pos <= mLength);
            if (count == 0) return *this;
            
            GrowIfNeeded(count);
            Char* data = GetData();
            memory::NkMemMove(data + pos + count, data + pos, mLength - pos);
            memory::NkMemSet(data + pos, ch, count);
            mLength += count;
            data[mLength] = '\0';
            
            return *this;
        }

        // ========================================
        // MODIFIERS - ERASE
        // ========================================

        NkString& NkString::Erase(SizeType pos, SizeType count) {
            NK_ASSERT(pos <= mLength);
            
            if (pos == mLength) return *this;
            if (count == npos || pos + count >= mLength) {
                count = mLength - pos;
            }
            
            Char* data = GetData();
            memory::NkMemMove(data + pos, data + pos + count, mLength - pos - count + 1);
            mLength -= count;
            
            return *this;
        }

        void NkString::PushBack(Char ch) {
            Append(ch);
        }

        void NkString::PopBack() {
            NK_ASSERT(mLength > 0);
            --mLength;
            GetData()[mLength] = '\0';
        }

        // ========================================
        // MODIFIERS - REPLACE
        // ========================================

        NkString& NkString::Replace(SizeType pos, SizeType count, const Char* str) {
            return Replace(pos, count, NkStringView(str));
        }

        NkString& NkString::Replace(SizeType pos, SizeType count, const NkString& str) {
            return Replace(pos, count, str.View());
        }

        NkString& NkString::Replace(SizeType pos, SizeType count, NkStringView view) {
            NK_ASSERT(pos <= mLength);
            
            if (pos + count > mLength) {
                count = mLength - pos;
            }
            
            if (count == view.Length()) {
                memory::NkMemCopy(GetData() + pos, view.Data(), view.Length());
            } else {
                Erase(pos, count);
                Insert(pos, view);
            }
            
            return *this;
        }

        // ========================================
        // MODIFIERS - RESIZE
        // ========================================

        void NkString::Resize(SizeType count) {
            Resize(count, '\0');
        }

        void NkString::Resize(SizeType count, Char ch) {
            if (count < mLength) {
                mLength = count;
                GetData()[mLength] = '\0';
            } else if (count > mLength) {
                GrowIfNeeded(count - mLength);
                memory::NkMemSet(GetData() + mLength, ch, count - mLength);
                mLength = count;
                GetData()[mLength] = '\0';
            }
        }

        void NkString::Swap(NkString& other) NK_NOEXCEPT {
            // Simple swap of all members
            NkString temp(static_cast<NkString&&>(*this));
            *this = static_cast<NkString&&>(other);
            other = static_cast<NkString&&>(temp);
        }
        // NkString.cpp PARTIE 3/3 - String Operations, Helpers, Non-members

        // ========================================
        // STRING OPERATIONS
        // ========================================

        NkString NkString::SubStr(SizeType pos, SizeType count) const {
            NK_ASSERT(pos <= mLength);
            
            if (count == npos || pos + count > mLength) {
                count = mLength - pos;
            }
            
            return NkString(GetData() + pos, count);
        }

        int32 NkString::Compare(const NkString& other) const NK_NOEXCEPT {
            return Compare(other.View());
        }

        int32 NkString::Compare(NkStringView other) const NK_NOEXCEPT {
            SizeType minLen = mLength < other.Length() ? mLength : other.Length();
            
            int result = memory::NkMemCompare(GetData(), other.Data(), minLen);
            if (result != 0) return result;
            
            if (mLength < other.Length()) return -1;
            if (mLength > other.Length()) return 1;
            return 0;
        }

        int32 NkString::Compare(const Char* str) const NK_NOEXCEPT {
            return Compare(NkStringView(str));
        }

        Bool NkString::StartsWith(NkStringView prefix) const NK_NOEXCEPT {
            if (prefix.Length() > mLength) return false;
            return memory::NkMemCompare(GetData(), prefix.Data(), prefix.Length()) == 0;
        }

        Bool NkString::StartsWith(Char ch) const NK_NOEXCEPT {
            return mLength > 0 && GetData()[0] == ch;
        }

        Bool NkString::EndsWith(NkStringView suffix) const NK_NOEXCEPT {
            if (suffix.Length() > mLength) return false;
            return memory::NkMemCompare(
                GetData() + mLength - suffix.Length(),
                suffix.Data(),
                suffix.Length()
            ) == 0;
        }

        Bool NkString::EndsWith(Char ch) const NK_NOEXCEPT {
            return mLength > 0 && GetData()[mLength - 1] == ch;
        }

        Bool NkString::Contains(NkStringView str) const NK_NOEXCEPT {
            return Find(str) != npos;
        }

        Bool NkString::Contains(Char ch) const NK_NOEXCEPT {
            return Find(ch) != npos;
        }

        NkString::SizeType NkString::Find(NkStringView str, SizeType pos) const NK_NOEXCEPT {
            return View().Find(str, pos);
        }

        NkString::SizeType NkString::Find(Char ch, SizeType pos) const NK_NOEXCEPT {
            return View().Find(ch, pos);
        }

        NkString::SizeType NkString::RFind(NkStringView str, SizeType pos) const NK_NOEXCEPT {
            if (str.IsEmpty()) return mLength;
            if (str.Length() > mLength) return npos;
            
            SizeType searchPos = pos;
            if (searchPos == npos || searchPos > mLength - str.Length()) {
                searchPos = mLength - str.Length();
            }
            
            for (SizeType i = searchPos + 1; i > 0; --i) {
                if (memory::NkMemCompare(GetData() + i - 1, str.Data(), str.Length()) == 0) {
                    return i - 1;
                }
            }
            return npos;
        }

        NkString::SizeType NkString::RFind(Char ch, SizeType pos) const NK_NOEXCEPT {
            return View().RFind(ch, pos);
        }

        // ========================================
        // CONVERSION
        // ========================================

        NkString::operator NkStringView() const NK_NOEXCEPT {
            return View();
        }

        NkStringView NkString::View() const NK_NOEXCEPT {
            return NkStringView(GetData(), mLength);
        }

        // ========================================
        // ITERATORS
        // ========================================

        Char* NkString::begin() NK_NOEXCEPT {
            return GetData();
        }

        const Char* NkString::begin() const NK_NOEXCEPT {
            return GetData();
        }

        Char* NkString::end() NK_NOEXCEPT {
            return GetData() + mLength;
        }

        const Char* NkString::end() const NK_NOEXCEPT {
            return GetData() + mLength;
        }

        // ========================================
        // ALLOCATOR
        // ========================================

        memory::NkIAllocator& NkString::GetAllocator() const NK_NOEXCEPT {
            return *mAllocator;
        }

        // ========================================
        // PRIVATE HELPERS
        // ========================================

        Bool NkString::IsSSO() const NK_NOEXCEPT {
            return mCapacity == SSO_SIZE;
        }

        void NkString::SetSSO(Bool sso) NK_NOEXCEPT {
            if (sso) {
                mCapacity = SSO_SIZE;
            }
        }

        Char* NkString::GetData() NK_NOEXCEPT {
            return IsSSO() ? mSSOData : mHeapData;
        }

        const Char* NkString::GetData() const NK_NOEXCEPT {
            return IsSSO() ? mSSOData : mHeapData;
        }

        void NkString::AllocateHeap(SizeType capacity) {
            mHeapData = static_cast<Char*>(
                mAllocator->Allocate((capacity + 1) * sizeof(Char))
            );
            mCapacity = capacity;
            SetSSO(false);
        }

        void NkString::DeallocateHeap() {
            if (!IsSSO() && mHeapData) {
                mAllocator->Deallocate(mHeapData);
                mHeapData = nullptr;
            }
        }

        void NkString::MoveToHeap(SizeType newCapacity) {
            NK_ASSERT(IsSSO());
            
            Char temp[SSO_SIZE + 1];
            memory::NkMemCopy(temp, mSSOData, mLength + 1);
            
            AllocateHeap(newCapacity);
            memory::NkMemCopy(mHeapData, temp, mLength + 1);
        }

        void NkString::GrowIfNeeded(SizeType additionalSize) {
            SizeType needed = mLength + additionalSize;
            if (needed <= mCapacity) return;
            
            SizeType newCapacity = CalculateGrowth(mCapacity, needed);
            
            if (IsSSO()) {
                MoveToHeap(newCapacity);
            } else {
                Reserve(newCapacity);
            }
        }

        NkString::SizeType NkString::CalculateGrowth(SizeType current, SizeType needed) {
            // Growth factor: 1.5x
            SizeType growth = current + (current / 2);
            return growth > needed ? growth : needed;
        }

        // ========================================
        // ADVANCED SEARCH OPERATIONS
        // ========================================

        NkString::SizeType NkString::FindFirstOf(NkStringView chars, SizeType pos) const NK_NOEXCEPT {
            if (pos >= mLength) return npos;
            
            const Char* data = GetData();
            for (SizeType i = pos; i < mLength; ++i) {
                for (SizeType j = 0; j < chars.Length(); ++j) {
                    if (data[i] == chars[j]) {
                        return i;
                    }
                }
            }
            return npos;
        }

        NkString::SizeType NkString::FindLastOf(NkStringView chars, SizeType pos) const NK_NOEXCEPT {
            if (mLength == 0) return npos;
            
            SizeType searchPos = (pos == npos || pos >= mLength) ? mLength - 1 : pos;
            const Char* data = GetData();
            
            for (SizeType i = searchPos + 1; i > 0; --i) {
                SizeType idx = i - 1;
                for (SizeType j = 0; j < chars.Length(); ++j) {
                    if (data[idx] == chars[j]) {
                        return idx;
                    }
                }
            }
            return npos;
        }

        NkString::SizeType NkString::FindFirstNotOf(NkStringView chars, SizeType pos) const NK_NOEXCEPT {
            if (pos >= mLength) return npos;
            
            const Char* data = GetData();
            for (SizeType i = pos; i < mLength; ++i) {
                bool found = false;
                for (SizeType j = 0; j < chars.Length(); ++j) {
                    if (data[i] == chars[j]) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return i;
                }
            }
            return npos;
        }

        NkString::SizeType NkString::FindLastNotOf(NkStringView chars, SizeType pos) const NK_NOEXCEPT {
            if (mLength == 0) return npos;
            
            SizeType searchPos = (pos == npos || pos >= mLength) ? mLength - 1 : pos;
            const Char* data = GetData();
            
            for (SizeType i = searchPos + 1; i > 0; --i) {
                SizeType idx = i - 1;
                bool found = false;
                for (SizeType j = 0; j < chars.Length(); ++j) {
                    if (data[idx] == chars[j]) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return idx;
                }
            }
            return npos;
        }

        // ========================================
        // IN-PLACE TRANSFORMATIONS
        // ========================================

        NkString& NkString::ToLower() {
            Char* data = GetData();
            for (SizeType i = 0; i < mLength; ++i) {
                data[i] = string::NkToLower(data[i]);
            }
            return *this;
        }

        NkString& NkString::ToUpper() {
            Char* data = GetData();
            for (SizeType i = 0; i < mLength; ++i) {
                data[i] = string::NkToUpper(data[i]);
            }
            return *this;
        }

        NkString& NkString::Trim() {
            TrimRight();
            TrimLeft();
            return *this;
        }

        NkString& NkString::TrimLeft() {
            const Char* data = GetData();
            SizeType start = 0;
            
            while (start < mLength && string::NkIsWhitespace(data[start])) {
                ++start;
            }
            
            if (start > 0) {
                Erase(0, start);
            }
            
            return *this;
        }

        NkString& NkString::TrimRight() {
            if (mLength == 0) return *this;
            
            Char* data = GetData(); // Enlever const
            SizeType end = mLength;
            
            while (end > 0 && string::NkIsWhitespace(data[end - 1])) {
                --end;
            }
            
            if (end < mLength) {
                mLength = end;
                data[mLength] = '\0';
            }
            
            return *this;
        }

        // ========================================
        // NUMERIC CONVERSIONS
        // ========================================

        Bool NkString::ToInt(int32& out) const NK_NOEXCEPT {
            return string::NkParseInt(View(), out);
        }

        Bool NkString::ToFloat(float32& out) const NK_NOEXCEPT {
            return string::NkParseFloat(View(), out);
        }

        // ========================================
        // HASHING
        // ========================================

        uint64 NkString::Hash() const NK_NOEXCEPT {
            return string::NkHashFNV1a64(View());
        }

        Bool NkString::ToInt64(int64& out) const NK_NOEXCEPT {
            return string::NkParseInt64(View(), out);
        }

        Bool NkString::ToUInt(uint32& out) const NK_NOEXCEPT {
            return string::NkParseUInt(View(), out);
        }

        Bool NkString::ToUInt64(uint64& out) const NK_NOEXCEPT {
            return string::NkParseUInt64(View(), out);
        }

        Bool NkString::ToDouble(float64& out) const NK_NOEXCEPT {
            return string::NkParseDouble(View(), out);
        }

        Bool NkString::ToBool(bool& out) const NK_NOEXCEPT {
            return string::NkParseBool(View(), out);
        }

        int32 NkString::ToInt32(int32 defaultValue) const NK_NOEXCEPT {
            int32 result;
            return ToInt(result) ? result : defaultValue;
        }

        float32 NkString::ToFloat32(float32 defaultValue) const NK_NOEXCEPT {
            float32 result;
            return ToFloat(result) ? result : defaultValue;
        }

        Bool NkString::IsDigits() const NK_NOEXCEPT {
            return string::NkIsAllDigits(View());
        }

        Bool NkString::IsAlpha() const NK_NOEXCEPT {
            return string::NkIsAllAlpha(View());
        }

        Bool NkString::IsAlphaNumeric() const NK_NOEXCEPT {
            return string::NkIsAllAlphaNumeric(View());
        }

        Bool NkString::IsWhitespace() const NK_NOEXCEPT {
            return string::NkIsAllWhitespace(View());
        }

        Bool NkString::IsNumeric() const NK_NOEXCEPT {
            return string::NkIsNumeric(View());
        }

        Bool NkString::IsInteger() const NK_NOEXCEPT {
            return string::NkIsInteger(View());
        }

        NkString& NkString::Reverse() {
            Char* data = GetData();
            for (SizeType i = 0; i < mLength / 2; ++i) {
                Char temp = data[i];
                data[i] = data[mLength - i - 1];
                data[mLength - i - 1] = temp;
            }
            return *this;
        }

        NkString& NkString::Capitalize() {
            if (mLength == 0) return *this;
            
            Char* data = GetData();
            data[0] = string::NkToUpper(data[0]);
            for (SizeType i = 1; i < mLength; ++i) {
                data[i] = string::NkToLower(data[i]);
            }
            return *this;
        }

        NkString& NkString::TitleCase() {
            if (mLength == 0) return *this;
            
            Char* data = GetData();
            bool newWord = true;
            
            for (SizeType i = 0; i < mLength; ++i) {
                if (string::NkIsWhitespace(data[i]) || data[i] == '-') {
                    newWord = true;
                } else if (newWord) {
                    data[i] = string::NkToUpper(data[i]);
                    newWord = false;
                } else {
                    data[i] = string::NkToLower(data[i]);
                }
            }
            return *this;
        }

        NkString& NkString::RemoveChars(NkStringView charsToRemove) {
            if (charsToRemove.IsEmpty()) return *this;
            
            Char* data = GetData();
            SizeType writePos = 0;
            
            for (SizeType readPos = 0; readPos < mLength; ++readPos) {
                bool shouldRemove = false;
                for (SizeType j = 0; j < charsToRemove.Length(); ++j) {
                    if (data[readPos] == charsToRemove[j]) {
                        shouldRemove = true;
                        break;
                    }
                }
                
                if (!shouldRemove) {
                    data[writePos++] = data[readPos];
                }
            }
            
            mLength = writePos;
            data[mLength] = '\0';
            return *this;
        }

        NkString& NkString::RemoveAll(Char ch) {
            Char* data = GetData();
            SizeType writePos = 0;
            
            for (SizeType readPos = 0; readPos < mLength; ++readPos) {
                if (data[readPos] != ch) {
                    data[writePos++] = data[readPos];
                }
            }
            
            mLength = writePos;
            data[mLength] = '\0';
            return *this;
        }

        NkString NkString::Format(const Char* format, ...) {
            NkString result;
            if (!format) return result;
            
            va_list args;
            va_start(args, format);
            result = string::NkVFormat(format, args);
            va_end(args);
            
            return result;
        }

        NkString NkString::VFormat(const Char* format, va_list args) {
            return string::NkVFormat(format, args);
        }

        // ========================================
        // NON-MEMBER FUNCTIONS
        // ========================================

        NkString operator+(const NkString& lhs, const NkString& rhs) {
            NkString result;
            result.Reserve(lhs.Length() + rhs.Length());
            result.Append(lhs);
            result.Append(rhs);
            return result;
        }

        NkString operator+(const NkString& lhs, const Char* rhs) {
            NkString result(lhs);
            result.Append(rhs);
            return result;
        }

        NkString operator+(const Char* lhs, const NkString& rhs) {
            NkString result(lhs);
            result.Append(rhs);
            return result;
        }

        NkString operator+(const NkString& lhs, Char rhs) {
            NkString result(lhs);
            result.Append(rhs);
            return result;
        }

        NkString operator+(Char lhs, const NkString& rhs) {
            NkString result(1, lhs);
            result.Append(rhs);
            return result;
        }

        Bool operator==(const NkString& lhs, const NkString& rhs) NK_NOEXCEPT {
            return lhs.Compare(rhs) == 0;
        }

        Bool operator==(const NkString& lhs, const Char* rhs) NK_NOEXCEPT {
            return lhs.Compare(rhs) == 0;
        }

        Bool operator!=(const NkString& lhs, const NkString& rhs) NK_NOEXCEPT {
            return lhs.Compare(rhs) != 0;
        }

        Bool operator<(const NkString& lhs, const NkString& rhs) NK_NOEXCEPT {
            return lhs.Compare(rhs) < 0;
        }

        Bool operator<=(const NkString& lhs, const NkString& rhs) NK_NOEXCEPT {
            return lhs.Compare(rhs) <= 0;
        }

        Bool operator>(const NkString& lhs, const NkString& rhs) NK_NOEXCEPT {
            return lhs.Compare(rhs) > 0;
        }

        Bool operator>=(const NkString& lhs, const NkString& rhs) NK_NOEXCEPT {
            return lhs.Compare(rhs) >= 0;
        }

    } // namespace core
} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================