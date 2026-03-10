// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\NkStringBuilder.cpp
// DESCRIPTION: String builder implementation
// AUTEUR: Rihen
// DATE: 2026-02-07
// -----------------------------------------------------------------------------

#include "NkStringBuilder.h"
#include "NkStringUtils.h"
#include "NkStringHash.h"
#include "NKMemory/NkFunction.h"
#include "NKCore/Assert/NkAssert.h"
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
    

        // ========================================
        // STATIC HELPER FUNCTIONS
        // ========================================
        
        NkStringBuilder::SizeType NkStringBuilder::IntToString(int64 value, char* buffer, SizeType bufferSize) {
            int len = snprintf(buffer, bufferSize, "%lld", static_cast<long long>(value));
            return len >= 0 ? static_cast<SizeType>(len) : 0;
        }
        
        NkStringBuilder::SizeType NkStringBuilder::UIntToString(uint64 value, char* buffer, SizeType bufferSize) {
            int len = snprintf(buffer, bufferSize, "%llu", static_cast<unsigned long long>(value));
            return len >= 0 ? static_cast<SizeType>(len) : 0;
        }
        
        NkStringBuilder::SizeType NkStringBuilder::FloatToString(float64 value, char* buffer, SizeType bufferSize, int precision) {
            char format[16];
            snprintf(format, sizeof(format), "%%.%dlf", precision);
            int len = snprintf(buffer, bufferSize, format, value);
            return len >= 0 ? static_cast<SizeType>(len) : 0;
        }
        
        NkStringBuilder::SizeType NkStringBuilder::HexToString(uint64 value, char* buffer, SizeType bufferSize, bool prefix) {
            if (prefix) {
                int len = snprintf(buffer, bufferSize, "0x%llX", static_cast<unsigned long long>(value));
                return len >= 0 ? static_cast<SizeType>(len) : 0;
            } else {
                int len = snprintf(buffer, bufferSize, "%llX", static_cast<unsigned long long>(value));
                return len >= 0 ? static_cast<SizeType>(len) : 0;
            }
        }
        
        NkStringBuilder::SizeType NkStringBuilder::BinaryToString(uint64 value, char* buffer, SizeType bufferSize, SizeType bits) {
            if (bits > 64) bits = 64;
            if (bits == 0) return 0;
            
            char* ptr = buffer;
            for (SizeType i = bits; i > 0; --i) {
                *ptr++ = (value & (1ULL << (i - 1))) ? '1' : '0';
                if ((i - 1) % 4 == 0 && i > 1) {
                    *ptr++ = ' ';
                }
            }
            *ptr = '\0';
            
            return static_cast<SizeType>(ptr - buffer);
        }
        
        NkStringBuilder::SizeType NkStringBuilder::OctalToString(uint64 value, char* buffer, SizeType bufferSize, bool prefix) {
            if (prefix) {
                int len = snprintf(buffer, bufferSize, "0%llo", static_cast<unsigned long long>(value));
                return len >= 0 ? static_cast<SizeType>(len) : 0;
            } else {
                int len = snprintf(buffer, bufferSize, "%llo", static_cast<unsigned long long>(value));
                return len >= 0 ? static_cast<SizeType>(len) : 0;
            }
        }
        
        // ========================================
        // PRIVATE HELPER FUNCTIONS
        // ========================================
        
        void NkStringBuilder::FreeBuffer() {
            if (mBuffer) {
                mAllocator->Deallocate(mBuffer);
                mBuffer = nullptr;
            }
        }
        
        void NkStringBuilder::AllocateBuffer(SizeType capacity) {
            mBuffer = static_cast<char*>(mAllocator->Allocate((capacity + 1) * sizeof(char)));
            mCapacity = capacity;
            mBuffer[0] = '\0';
            mLength = 0;
        }
        
        void NkStringBuilder::CopyFrom(const NkStringBuilder& other) {
            mAllocator = other.mAllocator;
            mLength = other.mLength;
            mCapacity = other.mCapacity;
            
            if (mCapacity > 0) {
                mBuffer = static_cast<char*>(mAllocator->Allocate((mCapacity + 1) * sizeof(char)));
                if (other.mBuffer && mLength > 0) {
                    memory::NkMemCopy(mBuffer, other.mBuffer, mLength);
                }
                mBuffer[mLength] = '\0';
            } else {
                mBuffer = nullptr;
            }
        }
        
        void NkStringBuilder::MoveFrom(NkStringBuilder&& other) NK_NOEXCEPT {
            mAllocator = other.mAllocator;
            mBuffer = other.mBuffer;
            mLength = other.mLength;
            mCapacity = other.mCapacity;
            
            other.mBuffer = nullptr;
            other.mLength = 0;
            other.mCapacity = 0;
        }
        
        void NkStringBuilder::EnsureCapacity(SizeType needed) {
            if (needed <= mCapacity) return;
            Reallocate(needed);
        }
        
        void NkStringBuilder::GrowIfNeeded(SizeType additionalSize) {
            SizeType needed = mLength + additionalSize;
            if (needed <= mCapacity) return;
            
            // Growth factor: 1.5x
            SizeType newCapacity = mCapacity + (mCapacity / 2);
            if (newCapacity < needed) {
                newCapacity = needed;
            }
            
            Reallocate(newCapacity);
        }
        
        void NkStringBuilder::Reallocate(SizeType newCapacity) {
            if (newCapacity <= mCapacity) return;
            
            char* newBuffer = static_cast<char*>(
                mAllocator->Allocate((newCapacity + 1) * sizeof(char))
            );
            
            if (mBuffer) {
                if (mLength > 0) {
                    memory::NkMemCopy(newBuffer, mBuffer, mLength);
                }
                mAllocator->Deallocate(mBuffer);
            }
            
            mBuffer = newBuffer;
            mCapacity = newCapacity;
            mBuffer[mLength] = '\0';
        }
        
        // ========================================
        // CONSTRUCTORS & DESTRUCTOR
        // ========================================
        
        NkStringBuilder::NkStringBuilder()
            : mAllocator(&memory::NkGetDefaultAllocator())
            , mBuffer(nullptr)
            , mLength(0)
            , mCapacity(0)
        {
            Reserve(DEFAULT_CAPACITY);
        }
        
        NkStringBuilder::NkStringBuilder(SizeType initialCapacity)
            : mAllocator(&memory::NkGetDefaultAllocator())
            , mBuffer(nullptr)
            , mLength(0)
            , mCapacity(0)
        {
            Reserve(initialCapacity);
        }
        
        NkStringBuilder::NkStringBuilder(memory::NkIAllocator& allocator)
            : mAllocator(&allocator)
            , mBuffer(nullptr)
            , mLength(0)
            , mCapacity(0)
        {
            Reserve(DEFAULT_CAPACITY);
        }
        
        NkStringBuilder::NkStringBuilder(SizeType initialCapacity, memory::NkIAllocator& allocator)
            : mAllocator(&allocator)
            , mBuffer(nullptr)
            , mLength(0)
            , mCapacity(0)
        {
            Reserve(initialCapacity);
        }
        
        NkStringBuilder::NkStringBuilder(NkStringView view)
            : mAllocator(&memory::NkGetDefaultAllocator())
            , mBuffer(nullptr)
            , mLength(0)
            , mCapacity(0)
        {
            Append(view);
        }
        
        NkStringBuilder::NkStringBuilder(const char* str)
            : mAllocator(&memory::NkGetDefaultAllocator())
            , mBuffer(nullptr)
            , mLength(0)
            , mCapacity(0)
        {
            if (str) {
                Append(str);
            } else {
                Reserve(DEFAULT_CAPACITY);
            }
        }
        
        NkStringBuilder::NkStringBuilder(const NkString& str)
            : mAllocator(&memory::NkGetDefaultAllocator())
            , mBuffer(nullptr)
            , mLength(0)
            , mCapacity(0)
        {
            Append(str);
        }
        
        NkStringBuilder::NkStringBuilder(const NkStringBuilder& other)
            : mAllocator(&memory::NkGetDefaultAllocator())
            , mBuffer(nullptr)
            , mLength(0)
            , mCapacity(0)
        {
            CopyFrom(other);
        }
        
        NkStringBuilder::NkStringBuilder(NkStringBuilder&& other) NK_NOEXCEPT
            : mAllocator(&memory::NkGetDefaultAllocator())
            , mBuffer(nullptr)
            , mLength(0)
            , mCapacity(0)
        {
            MoveFrom(traits::NkMove(other));
        }
        
        NkStringBuilder::~NkStringBuilder() {
            FreeBuffer();
        }
        
        // ========================================
        // ASSIGNMENT OPERATORS
        // ========================================
        
        NkStringBuilder& NkStringBuilder::operator=(const NkStringBuilder& other) {
            if (this != &other) {
                FreeBuffer();
                CopyFrom(other);
            }
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::operator=(NkStringBuilder&& other) NK_NOEXCEPT {
            if (this != &other) {
                FreeBuffer();
                MoveFrom(traits::NkMove(other));
            }
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::operator=(NkStringView view) {
            Clear();
            Append(view);
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::operator=(const char* str) {
            Clear();
            if (str) {
                Append(str);
            }
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::operator=(const NkString& str) {
            Clear();
            Append(str);
            return *this;
        }
        
        // ========================================
        // APPEND OPERATIONS - STRING
        // ========================================
        
        NkStringBuilder& NkStringBuilder::Append(const char* str) {
            if (!str) return *this;
            return Append(str, string::NkLeng(str));
        }
        
        NkStringBuilder& NkStringBuilder::Append(const char* str, SizeType length) {
            if (!str || length == 0) return *this;
            
            GrowIfNeeded(length);
            memory::NkMemCopy(mBuffer + mLength, str, length);
            mLength += length;
            mBuffer[mLength] = '\0';
            
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::Append(const NkString& str) {
            return Append(str.Data(), str.Length());
        }
        
        NkStringBuilder& NkStringBuilder::Append(NkStringView view) {
            return Append(view.Data(), view.Length());
        }
        
        NkStringBuilder& NkStringBuilder::Append(char ch) {
            GrowIfNeeded(1);
            mBuffer[mLength++] = ch;
            mBuffer[mLength] = '\0';
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::Append(SizeType count, char ch) {
            if (count == 0) return *this;
            
            GrowIfNeeded(count);
            memory::NkMemSet(mBuffer + mLength, ch, count);
            mLength += count;
            mBuffer[mLength] = '\0';
            
            return *this;
        }
        
        // ========================================
        // APPEND OPERATIONS - NUMBERS
        // ========================================
        
        NkStringBuilder& NkStringBuilder::Append(int8 value) {
            return Append(static_cast<int32>(value));
        }
        
        NkStringBuilder& NkStringBuilder::Append(int16 value) {
            return Append(static_cast<int32>(value));
        }
        
        NkStringBuilder& NkStringBuilder::Append(int32 value) {
            char buffer[32];
            SizeType len = IntToString(value, buffer, sizeof(buffer));
            return Append(buffer, len);
        }
        
        NkStringBuilder& NkStringBuilder::Append(int64 value) {
            char buffer[32];
            SizeType len = IntToString(value, buffer, sizeof(buffer));
            return Append(buffer, len);
        }
        
        NkStringBuilder& NkStringBuilder::Append(uint8 value) {
            return Append(static_cast<uint32>(value));
        }
        
        NkStringBuilder& NkStringBuilder::Append(uint16 value) {
            return Append(static_cast<uint32>(value));
        }
        
        NkStringBuilder& NkStringBuilder::Append(uint32 value) {
            char buffer[32];
            SizeType len = UIntToString(value, buffer, sizeof(buffer));
            return Append(buffer, len);
        }
        
        NkStringBuilder& NkStringBuilder::Append(uint64 value) {
            char buffer[32];
            SizeType len = UIntToString(value, buffer, sizeof(buffer));
            return Append(buffer, len);
        }
        
        NkStringBuilder& NkStringBuilder::Append(float32 value) {
            char buffer[64];
            SizeType len = FloatToString(value, buffer, sizeof(buffer));
            return Append(buffer, len);
        }
        
        NkStringBuilder& NkStringBuilder::Append(float64 value) {
            char buffer[64];
            SizeType len = FloatToString(value, buffer, sizeof(buffer));
            return Append(buffer, len);
        }
        
        NkStringBuilder& NkStringBuilder::Append(bool value) {
            return Append(value ? "true" : "false");
        }
        
        // ========================================
        // APPEND HEX/BINARY/OCTAL
        // ========================================
        
        NkStringBuilder& NkStringBuilder::AppendHex(uint32 value, bool prefix) {
            char buffer[32];
            SizeType len = HexToString(value, buffer, sizeof(buffer), prefix);
            return Append(buffer, len);
        }
        
        NkStringBuilder& NkStringBuilder::AppendHex(uint64 value, bool prefix) {
            char buffer[32];
            SizeType len = HexToString(value, buffer, sizeof(buffer), prefix);
            return Append(buffer, len);
        }
        
        NkStringBuilder& NkStringBuilder::AppendBinary(uint32 value, SizeType bits) {
            char buffer[128]; // 32 bits + spaces
            SizeType len = BinaryToString(value, buffer, sizeof(buffer), bits);
            return Append(buffer, len);
        }
        
        NkStringBuilder& NkStringBuilder::AppendBinary(uint64 value, SizeType bits) {
            char buffer[256]; // 64 bits + spaces
            SizeType len = BinaryToString(value, buffer, sizeof(buffer), bits);
            return Append(buffer, len);
        }
        
        NkStringBuilder& NkStringBuilder::AppendOctal(uint32 value, bool prefix) {
            char buffer[32];
            SizeType len = OctalToString(value, buffer, sizeof(buffer), prefix);
            return Append(buffer, len);
        }
        
        NkStringBuilder& NkStringBuilder::AppendOctal(uint64 value, bool prefix) {
            char buffer[32];
            SizeType len = OctalToString(value, buffer, sizeof(buffer), prefix);
            return Append(buffer, len);
        }
        
        // ========================================
        // OPERATOR <<
        // ========================================
        
        NkStringBuilder& NkStringBuilder::operator<<(const char* str) { return Append(str); }
        NkStringBuilder& NkStringBuilder::operator<<(const NkString& str) { return Append(str); }
        NkStringBuilder& NkStringBuilder::operator<<(NkStringView view) { return Append(view); }
        NkStringBuilder& NkStringBuilder::operator<<(char ch) { return Append(ch); }
        NkStringBuilder& NkStringBuilder::operator<<(int8 value) { return Append(value); }
        NkStringBuilder& NkStringBuilder::operator<<(int16 value) { return Append(value); }
        NkStringBuilder& NkStringBuilder::operator<<(int32 value) { return Append(value); }
        NkStringBuilder& NkStringBuilder::operator<<(int64 value) { return Append(value); }
        NkStringBuilder& NkStringBuilder::operator<<(uint8 value) { return Append(value); }
        NkStringBuilder& NkStringBuilder::operator<<(uint16 value) { return Append(value); }
        NkStringBuilder& NkStringBuilder::operator<<(uint32 value) { return Append(value); }
        NkStringBuilder& NkStringBuilder::operator<<(uint64 value) { return Append(value); }
        NkStringBuilder& NkStringBuilder::operator<<(float32 value) { return Append(value); }
        NkStringBuilder& NkStringBuilder::operator<<(float64 value) { return Append(value); }
        NkStringBuilder& NkStringBuilder::operator<<(bool value) { return Append(value); }
        
        // ========================================
        // APPEND LINE
        // ========================================
        
        NkStringBuilder& NkStringBuilder::AppendLine() {
            return Append('\n');
        }
        
        NkStringBuilder& NkStringBuilder::AppendLine(NkStringView str) {
            Append(str);
            return Append('\n');
        }
        
        NkStringBuilder& NkStringBuilder::AppendLine(const char* str) {
            Append(str);
            return Append('\n');
        }
        
        NkStringBuilder& NkStringBuilder::AppendLine(const NkString& str) {
            Append(str);
            return Append('\n');
        }
        
        // ========================================
        // APPEND FORMAT
        // ========================================
        
        NkStringBuilder& NkStringBuilder::AppendFormat(const char* format, ...) {
            if (!format) return *this;
            
            va_list args;
            va_start(args, format);
            AppendVFormat(format, args);
            va_end(args);
            
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::AppendVFormat(const char* format, va_list args) {
            if (!format) return *this;
            
            va_list args_copy;
            va_copy(args_copy, args);
            int size = vsnprintf(nullptr, 0, format, args_copy);
            va_end(args_copy);
            
            if (size < 0) {
                return *this;
            }
            
            GrowIfNeeded(size);
            
            vsnprintf(mBuffer + mLength, size + 1, format, args);
            mLength += size;
            
            return *this;
        }
        
        // ========================================
        // INSERT OPERATIONS
        // ========================================
        
        NkStringBuilder& NkStringBuilder::Insert(SizeType pos, char ch) {
            NK_ASSERT(pos <= mLength);
            
            GrowIfNeeded(1);
            memory::NkMemMove(mBuffer + pos + 1, mBuffer + pos, mLength - pos);
            mBuffer[pos] = ch;
            ++mLength;
            mBuffer[mLength] = '\0';
            
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::Insert(SizeType pos, const char* str) {
            if (!str) return *this;
            return Insert(pos, str, string::NkLeng(str));
        }
        
        NkStringBuilder& NkStringBuilder::Insert(SizeType pos, const char* str, SizeType length) {
            NK_ASSERT(pos <= mLength);
            if (!str || length == 0) return *this;
            
            GrowIfNeeded(length);
            memory::NkMemMove(mBuffer + pos + length, mBuffer + pos, mLength - pos);
            memory::NkMemCopy(mBuffer + pos, str, length);
            mLength += length;
            mBuffer[mLength] = '\0';
            
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::Insert(SizeType pos, NkStringView view) {
            return Insert(pos, view.Data(), view.Length());
        }
        
        NkStringBuilder& NkStringBuilder::Insert(SizeType pos, const NkString& str) {
            return Insert(pos, str.Data(), str.Length());
        }
        
        NkStringBuilder& NkStringBuilder::Insert(SizeType pos, SizeType count, char ch) {
            NK_ASSERT(pos <= mLength);
            if (count == 0) return *this;
            
            GrowIfNeeded(count);
            memory::NkMemMove(mBuffer + pos + count, mBuffer + pos, mLength - pos);
            memory::NkMemSet(mBuffer + pos, ch, count);
            mLength += count;
            mBuffer[mLength] = '\0';
            
            return *this;
        }
        
        // ========================================
        // REPLACE OPERATIONS
        // ========================================
        
        NkStringBuilder& NkStringBuilder::Replace(SizeType pos, SizeType count, const char* str) {
            NK_ASSERT(pos <= mLength);
            if (!str) return Remove(pos, count);
            
            SizeType strLen = string::NkLeng(str);
            SizeType newLength = mLength - count + strLen;
            
            if (strLen > count) {
                GrowIfNeeded(strLen - count);
            }
            
            // Déplacer la partie après la portion à remplacer
            memory::NkMemMove(mBuffer + pos + strLen, mBuffer + pos + count, mLength - pos - count);
            
            // Copier la nouvelle chaîne
            memory::NkMemCopy(mBuffer + pos, str, strLen);
            
            mLength = newLength;
            mBuffer[mLength] = '\0';
            
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::Replace(SizeType pos, SizeType count, NkStringView view) {
            return Replace(pos, count, view.Data());
        }
        
        NkStringBuilder& NkStringBuilder::Replace(char oldChar, char newChar) {
            for (SizeType i = 0; i < mLength; ++i) {
                if (mBuffer[i] == oldChar) {
                    mBuffer[i] = newChar;
                    return *this; // Only replace first occurrence
                }
            }
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::Replace(NkStringView oldStr, NkStringView newStr) {
            SizeType pos = Find(oldStr);
            if (pos != NkStringView::npos) {
                Replace(pos, oldStr.Length(), newStr);
            }
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::ReplaceAll(char oldChar, char newChar) {
            for (SizeType i = 0; i < mLength; ++i) {
                if (mBuffer[i] == oldChar) {
                    mBuffer[i] = newChar;
                }
            }
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::ReplaceAll(NkStringView oldStr, NkStringView newStr) {
            if (oldStr.Empty()) return *this;
            
            SizeType oldLen = oldStr.Length();
            SizeType newLen = newStr.Length();
            SizeType pos = 0;
            
            while ((pos = Find(oldStr, pos)) != NkStringView::npos) {
                Replace(pos, oldLen, newStr);
                pos += newLen;
            }
            
            return *this;
        }
        
        // ========================================
        // REMOVE/ERASE OPERATIONS
        // ========================================
        
        NkStringBuilder& NkStringBuilder::Remove(SizeType pos, SizeType count) {
            NK_ASSERT(pos <= mLength);
            
            if (pos + count > mLength) {
                count = mLength - pos;
            }
            
            memory::NkMemMove(mBuffer + pos, mBuffer + pos + count, mLength - pos - count);
            mLength -= count;
            mBuffer[mLength] = '\0';
            
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::RemoveAt(SizeType pos) {
            return Remove(pos, 1);
        }
        
        NkStringBuilder& NkStringBuilder::RemovePrefix(SizeType count) {
            return Remove(0, count);
        }
        
        NkStringBuilder& NkStringBuilder::RemoveSuffix(SizeType count) {
            if (count > mLength) count = mLength;
            return Remove(mLength - count, count);
        }
        
        NkStringBuilder& NkStringBuilder::RemoveAll(char ch) {
            SizeType writePos = 0;
            for (SizeType readPos = 0; readPos < mLength; ++readPos) {
                if (mBuffer[readPos] != ch) {
                    mBuffer[writePos++] = mBuffer[readPos];
                }
            }
            mLength = writePos;
            mBuffer[mLength] = '\0';
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::RemoveAll(NkStringView str) {
            if (str.Empty()) return *this;
            
            SizeType strLen = str.Length();
            SizeType writePos = 0;
            SizeType readPos = 0;
            
            while (readPos < mLength) {
                if (readPos + strLen <= mLength && 
                    memory::NkMemCompare(mBuffer + readPos, str.Data(), strLen) == 0) {
                    readPos += strLen;
                } else {
                    mBuffer[writePos++] = mBuffer[readPos++];
                }
            }
            
            mLength = writePos;
            mBuffer[mLength] = '\0';
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::RemoveWhitespace() {
            SizeType writePos = 0;
            for (SizeType readPos = 0; readPos < mLength; ++readPos) {
                if (!string::NkIsWhitespace(mBuffer[readPos])) {
                    mBuffer[writePos++] = mBuffer[readPos];
                }
            }
            mLength = writePos;
            mBuffer[mLength] = '\0';
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::Trim() {
            TrimLeft();
            TrimRight();
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::TrimLeft() {
            SizeType pos = 0;
            while (pos < mLength && string::NkIsWhitespace(mBuffer[pos])) {
                ++pos;
            }
            if (pos > 0) {
                RemovePrefix(pos);
            }
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::TrimRight() {
            SizeType pos = mLength;
            while (pos > 0 && string::NkIsWhitespace(mBuffer[pos - 1])) {
                --pos;
            }
            if (pos < mLength) {
                mLength = pos;
                mBuffer[mLength] = '\0';
            }
            return *this;
        }
        
        void NkStringBuilder::Clear() NK_NOEXCEPT {
            mLength = 0;
            if (mBuffer) {
                mBuffer[0] = '\0';
            }
        }
        
        void NkStringBuilder::Reset() NK_NOEXCEPT {
            Clear();
        }
        
        void NkStringBuilder::Release() {
            FreeBuffer();
            mLength = 0;
            mCapacity = 0;
        }
        
        // ========================================
        // CAPACITY & SIZE
        // ========================================
        
        NkStringBuilder::SizeType NkStringBuilder::Length() const NK_NOEXCEPT {
            return mLength;
        }
        
        NkStringBuilder::SizeType NkStringBuilder::Size() const NK_NOEXCEPT {
            return mLength;
        }
        
        usize NkStringBuilder::Count() const NK_NOEXCEPT {
            return static_cast<usize>(mLength);
        }
        
        NkStringBuilder::SizeType NkStringBuilder::Capacity() const NK_NOEXCEPT {
            return mCapacity;
        }
        
        NkStringBuilder::SizeType NkStringBuilder::MaxSize() const NK_NOEXCEPT {
            return MAX_CAPACITY;
        }
        
        bool NkStringBuilder::Empty() const NK_NOEXCEPT {
            return mLength == 0;
        }
        
        bool NkStringBuilder::IsNull() const NK_NOEXCEPT {
            return mBuffer == nullptr;
        }
        
        bool NkStringBuilder::IsNullOrEmpty() const NK_NOEXCEPT {
            return mBuffer == nullptr || mLength == 0;
        }
        
        bool NkStringBuilder::IsFull() const NK_NOEXCEPT {
            return mLength == mCapacity;
        }
        
        void NkStringBuilder::Reserve(SizeType newCapacity) {
            if (newCapacity <= mCapacity) return;
            Reallocate(newCapacity);
        }
        
        void NkStringBuilder::Resize(SizeType newSize, char fillChar) {
            if (newSize > mLength) {
                // Agrandir
                Reserve(newSize);
                memory::NkMemSet(mBuffer + mLength, fillChar, newSize - mLength);
                mLength = newSize;
                mBuffer[mLength] = '\0';
            } else if (newSize < mLength) {
                // Rétrécir
                mLength = newSize;
                mBuffer[mLength] = '\0';
            }
        }
        
        void NkStringBuilder::ShrinkToFit() {
            if (mLength == mCapacity) return;
            
            char* newBuffer = static_cast<char*>(
                mAllocator->Allocate((mLength + 1) * sizeof(char))
            );
            
            if (mBuffer) {
                if (mLength > 0) {
                    memory::NkMemCopy(newBuffer, mBuffer, mLength);
                }
                mAllocator->Deallocate(mBuffer);
            }
            
            mBuffer = newBuffer;
            mCapacity = mLength;
            mBuffer[mLength] = '\0';
        }
        
        // ========================================
        // ELEMENT ACCESS
        // ========================================
        
        char& NkStringBuilder::operator[](SizeType index) {
            NK_ASSERT(index < mLength);
            return mBuffer[index];
        }
        
        const char& NkStringBuilder::operator[](SizeType index) const {
            NK_ASSERT(index < mLength);
            return mBuffer[index];
        }
        
        char& NkStringBuilder::At(SizeType index) {
            NK_ASSERT(index < mLength);
            return mBuffer[index];
        }
        
        const char& NkStringBuilder::At(SizeType index) const {
            NK_ASSERT(index < mLength);
            return mBuffer[index];
        }
        
        char& NkStringBuilder::Front() {
            NK_ASSERT(mLength > 0);
            return mBuffer[0];
        }
        
        const char& NkStringBuilder::Front() const {
            NK_ASSERT(mLength > 0);
            return mBuffer[0];
        }
        
        char& NkStringBuilder::Back() {
            NK_ASSERT(mLength > 0);
            return mBuffer[mLength - 1];
        }
        
        const char& NkStringBuilder::Back() const {
            NK_ASSERT(mLength > 0);
            return mBuffer[mLength - 1];
        }
        
        char* NkStringBuilder::Data() NK_NOEXCEPT {
            return mBuffer;
        }
        
        const char* NkStringBuilder::Data() const NK_NOEXCEPT {
            return mBuffer;
        }
        
        const char* NkStringBuilder::CStr() const NK_NOEXCEPT {
            return mBuffer;
        }
        
        // ========================================
        // SUBSTRING & EXTRACTION
        // ========================================
        
        NkStringView NkStringBuilder::SubStr(SizeType pos, SizeType count) const {
            NK_ASSERT(pos <= mLength);
            SizeType rcount = (count == NkStringView::npos || pos + count > mLength) 
                ? mLength - pos : count;
            return NkStringView(mBuffer + pos, rcount);
        }
        
        NkStringView NkStringBuilder::Slice(SizeType start, SizeType end) const {
            NK_ASSERT(start <= end && end <= mLength);
            return NkStringView(mBuffer + start, end - start);
        }
        
        NkStringView NkStringBuilder::Left(SizeType count) const {
            return SubStr(0, count);
        }
        
        NkStringView NkStringBuilder::Right(SizeType count) const {
            if (count >= mLength) return NkStringView(mBuffer, mLength);
            return SubStr(mLength - count, count);
        }
        
        NkStringView NkStringBuilder::Mid(SizeType pos, SizeType count) const {
            return SubStr(pos, count);
        }
        
        NkString NkStringBuilder::ToString() const {
            return NkString(mBuffer, mLength);
        }
        
        NkString NkStringBuilder::ToString(SizeType pos, SizeType count) const {
            return NkString(SubStr(pos, count));
        }
        
        // ========================================
        // SEARCH & FIND
        // ========================================
        
        NkStringBuilder::SizeType NkStringBuilder::Find(char ch, SizeType pos) const NK_NOEXCEPT {
            if (pos >= mLength) return NkStringView::npos;
            
            for (SizeType i = pos; i < mLength; ++i) {
                if (mBuffer[i] == ch) {
                    return i;
                }
            }
            return NkStringView::npos;
        }
        
        NkStringBuilder::SizeType NkStringBuilder::Find(NkStringView str, SizeType pos) const NK_NOEXCEPT {
            if (str.Empty()) return pos;
            if (pos > mLength) return NkStringView::npos;
            if (str.Length() > mLength - pos) return NkStringView::npos;
            
            for (SizeType i = pos; i <= mLength - str.Length(); ++i) {
                if (memory::NkMemCompare(mBuffer + i, str.Data(), str.Length()) == 0) {
                    return i;
                }
            }
            return NkStringView::npos;
        }
        
        NkStringBuilder::SizeType NkStringBuilder::FindLast(char ch, SizeType pos) const NK_NOEXCEPT {
            if (mLength == 0) return NkStringView::npos;
            
            SizeType searchPos = (pos == NkStringView::npos || pos >= mLength) ? mLength - 1 : pos;
            
            for (SizeType i = searchPos + 1; i > 0; --i) {
                if (mBuffer[i - 1] == ch) {
                    return i - 1;
                }
            }
            return NkStringView::npos;
        }
        
        NkStringBuilder::SizeType NkStringBuilder::FindLast(NkStringView str, SizeType pos) const NK_NOEXCEPT {
            if (str.Empty()) return (pos < mLength) ? pos : mLength;
            if (str.Length() > mLength) return NkStringView::npos;
            
            SizeType searchPos = (pos == NkStringView::npos || pos >= mLength - str.Length()) 
                ? mLength - str.Length() 
                : pos;
            
            for (SizeType i = searchPos + 1; i > 0; --i) {
                SizeType idx = i - 1;
                if (memory::NkMemCompare(mBuffer + idx, str.Data(), str.Length()) == 0) {
                    return idx;
                }
            }
            return NkStringView::npos;
        }
        
        bool NkStringBuilder::Contains(char ch) const NK_NOEXCEPT {
            return Find(ch) != NkStringView::npos;
        }
        
        bool NkStringBuilder::Contains(NkStringView str) const NK_NOEXCEPT {
            return Find(str) != NkStringView::npos;
        }
        
        bool NkStringBuilder::StartsWith(char ch) const NK_NOEXCEPT {
            return mLength > 0 && mBuffer[0] == ch;
        }
        
        bool NkStringBuilder::StartsWith(NkStringView prefix) const NK_NOEXCEPT {
            if (prefix.Length() > mLength) return false;
            return memory::NkMemCompare(mBuffer, prefix.Data(), prefix.Length()) == 0;
        }
        
        bool NkStringBuilder::EndsWith(char ch) const NK_NOEXCEPT {
            return mLength > 0 && mBuffer[mLength - 1] == ch;
        }
        
        bool NkStringBuilder::EndsWith(NkStringView suffix) const NK_NOEXCEPT {
            if (suffix.Length() > mLength) return false;
            return memory::NkMemCompare(mBuffer + mLength - suffix.Length(), suffix.Data(), suffix.Length()) == 0;
        }
        
        // ========================================
        // COMPARISON
        // ========================================
        
        int NkStringBuilder::Compare(NkStringView other) const NK_NOEXCEPT {
            SizeType minLen = mLength < other.Length() ? mLength : other.Length();
            
            if (minLen > 0) {
                int result = memory::NkMemCompare(mBuffer, other.Data(), minLen);
                if (result != 0) return result;
            }
            
            if (mLength < other.Length()) return -1;
            if (mLength > other.Length()) return 1;
            return 0;
        }
        
        int NkStringBuilder::Compare(const NkStringBuilder& other) const NK_NOEXCEPT {
            return Compare(NkStringView(other.mBuffer, other.mLength));
        }
        
        bool NkStringBuilder::Equals(NkStringView other) const NK_NOEXCEPT {
            if (mLength != other.Length()) return false;
            return memory::NkMemCompare(mBuffer, other.Data(), mLength) == 0;
        }
        
        bool NkStringBuilder::Equals(const NkStringBuilder& other) const NK_NOEXCEPT {
            if (mLength != other.mLength) return false;
            return memory::NkMemCompare(mBuffer, other.mBuffer, mLength) == 0;
        }
        
        // ========================================
        // TRANSFORMATIONS
        // ========================================
        
        NkStringBuilder& NkStringBuilder::ToLower() {
            for (SizeType i = 0; i < mLength; ++i) {
                mBuffer[i] = string::NkToLower(mBuffer[i]);
            }
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::ToUpper() {
            for (SizeType i = 0; i < mLength; ++i) {
                mBuffer[i] = string::NkToUpper(mBuffer[i]);
            }
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::Capitalize() {
            if (mLength > 0) {
                mBuffer[0] = string::NkToUpper(mBuffer[0]);
                for (SizeType i = 1; i < mLength; ++i) {
                    mBuffer[i] = string::NkToLower(mBuffer[i]);
                }
            }
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::TitleCase() {
            if (mLength == 0) return *this;
            
            bool newWord = true;
            for (SizeType i = 0; i < mLength; ++i) {
                if (string::NkIsWhitespace(mBuffer[i]) || mBuffer[i] == '-') {
                    newWord = true;
                } else if (newWord) {
                    mBuffer[i] = string::NkToUpper(mBuffer[i]);
                    newWord = false;
                } else {
                    mBuffer[i] = string::NkToLower(mBuffer[i]);
                }
            }
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::Reverse() {
            for (SizeType i = 0; i < mLength / 2; ++i) {
                char temp = mBuffer[i];
                mBuffer[i] = mBuffer[mLength - i - 1];
                mBuffer[mLength - i - 1] = temp;
            }
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::PadLeft(SizeType totalWidth, char paddingChar) {
            if (mLength >= totalWidth) return *this;
            
            SizeType padCount = totalWidth - mLength;
            Insert(0, padCount, paddingChar);
            
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::PadRight(SizeType totalWidth, char paddingChar) {
            if (mLength >= totalWidth) return *this;
            
            SizeType padCount = totalWidth - mLength;
            Append(padCount, paddingChar);
            
            return *this;
        }
        
        NkStringBuilder& NkStringBuilder::PadCenter(SizeType totalWidth, char paddingChar) {
            if (mLength >= totalWidth) return *this;
            
            SizeType padding = totalWidth - mLength;
            SizeType leftPadding = padding / 2;
            SizeType rightPadding = padding - leftPadding;
            
            Insert(0, leftPadding, paddingChar);
            Append(rightPadding, paddingChar);
            
            return *this;
        }
        
        // ========================================
        // ITERATORS
        // ========================================
        
        NkStringBuilder::Iterator NkStringBuilder::begin() NK_NOEXCEPT {
            return mBuffer;
        }
        
        NkStringBuilder::ConstIterator NkStringBuilder::begin() const NK_NOEXCEPT {
            return mBuffer;
        }
        
        NkStringBuilder::ConstIterator NkStringBuilder::cbegin() const NK_NOEXCEPT {
            return mBuffer;
        }
        
        NkStringBuilder::Iterator NkStringBuilder::end() NK_NOEXCEPT {
            return mBuffer + mLength;
        }
        
        NkStringBuilder::ConstIterator NkStringBuilder::end() const NK_NOEXCEPT {
            return mBuffer + mLength;
        }
        
        NkStringBuilder::ConstIterator NkStringBuilder::cend() const NK_NOEXCEPT {
            return mBuffer + mLength;
        }
        
        NkStringBuilder::ReverseIterator NkStringBuilder::rbegin() NK_NOEXCEPT {
            return mBuffer + mLength - 1;
        }
        
        NkStringBuilder::ConstReverseIterator NkStringBuilder::rbegin() const NK_NOEXCEPT {
            return mBuffer + mLength - 1;
        }
        
        NkStringBuilder::ConstReverseIterator NkStringBuilder::crbegin() const NK_NOEXCEPT {
            return mBuffer + mLength - 1;
        }
        
        NkStringBuilder::ReverseIterator NkStringBuilder::rend() NK_NOEXCEPT {
            return mBuffer - 1;
        }
        
        NkStringBuilder::ConstReverseIterator NkStringBuilder::rend() const NK_NOEXCEPT {
            return mBuffer - 1;
        }
        
        NkStringBuilder::ConstReverseIterator NkStringBuilder::crend() const NK_NOEXCEPT {
            return mBuffer - 1;
        }
        
        // ========================================
        // STREAM-LIKE OPERATIONS
        // ========================================
        
        NkStringBuilder& NkStringBuilder::Write(const void* data, SizeType size) {
            return Append(static_cast<const char*>(data), size);
        }
        
        NkStringBuilder& NkStringBuilder::WriteChar(char ch) {
            return Append(ch);
        }
        
        NkStringBuilder& NkStringBuilder::WriteLine(NkStringView str) {
            return AppendLine(str);
        }
        
        // ========================================
        // CONVERSION OPERATORS
        // ========================================
        
        NkStringBuilder::operator NkString() const {
            return ToString();
        }
        
        NkStringBuilder::operator NkStringView() const NK_NOEXCEPT {
            return NkStringView(mBuffer, mLength);
        }
        
        NkStringBuilder::operator const char*() const NK_NOEXCEPT {
            return CStr();
        }
        
        // ========================================
        // UTILITIES
        // ========================================
        
        void NkStringBuilder::Swap(NkStringBuilder& other) NK_NOEXCEPT {
            (traits::NkSwap)(mAllocator, other.mAllocator);
            (traits::NkSwap)(mBuffer, other.mBuffer);
            (traits::NkSwap)(mLength, other.mLength);
            (traits::NkSwap)(mCapacity, other.mCapacity);
        }
        
        NkStringBuilder::SizeType NkStringBuilder::Hash() const NK_NOEXCEPT {
            return string::NkHashFNV1a64(NkStringView(mBuffer, mLength));
        }
        
        // ========================================
        // STATIC METHODS
        // ========================================
        
        NkStringBuilder NkStringBuilder::Join(NkStringView separator, const NkStringView* strings, SizeType count) {
            NkStringBuilder result;
            if (count == 0) return result;
            
            result.Append(strings[0]);
            for (SizeType i = 1; i < count; ++i) {
                result.Append(separator);
                result.Append(strings[i]);
            }
            
            return result;
        }
        
        NkStringBuilder NkStringBuilder::Join(NkStringView separator, const NkStringBuilder* builders, SizeType count) {
            NkStringBuilder result;
            if (count == 0) return result;
            
            result.Append(builders[0]);
            for (SizeType i = 1; i < count; ++i) {
                result.Append(separator);
                result.Append(builders[i]);
            }
            
            return result;
        }
        
    
} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
