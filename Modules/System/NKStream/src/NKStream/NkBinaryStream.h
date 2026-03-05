#pragma once

#include "NKPlatform/NkPlatformDetect.h"
#include "NKCore/NkTypes.h"
#include "NKCore/NkInline.h"
#include <cstddef>

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

#include "NkStream.h"
#include <cstring>


namespace nkentseu {

    class NKSTREAM_API NkBinaryStream : public NkStream {
        public:
            NkBinaryStream(void* data = nullptr, usize capacity = 0)
                : mBuffer(static_cast<uint8*>(data)),
                  mCapacity(capacity),
                  mSize(0),
                  mPosition(0),
                  mOwned(false) {}

            NkBinaryStream(usize capacity)
                : mBuffer(new uint8[capacity]),
                  mCapacity(capacity),
                  mSize(0),
                  mPosition(0),
                  mOwned(true) {}

            ~NkBinaryStream() override {
                if(mOwned) delete[] mBuffer;
            }

            bool Open(const char*, uint32) override { return true; }
            void Close() override { /* Ne libère pas la mémoire */ }
            bool IsOpen() const override { return true; }

            usize ReadRaw(void* buffer, usize byteCount) override {
                const usize available = mSize - mPosition;
                const usize toRead = (byteCount > available) ? available : byteCount;

                std::memcpy(buffer, mBuffer + mPosition, toRead);
                mPosition += toRead;
                return toRead;
            }

            usize WriteRaw(const void* data, usize byteCount) override {
                EnsureCapacity(mPosition + byteCount);

                std::memcpy(mBuffer + mPosition, data, byteCount);
                mPosition += byteCount;
                if(mPosition > mSize) mSize = mPosition;
                return byteCount;
            }

            bool Seek(usize position) override {
                if(position > mSize) return false;
                mPosition = position;
                return true;
            }

            usize Tell() const override { return mPosition; }
            usize Size() const override { return mSize; }
            bool IsEOF() const override { return mPosition >= mSize; }

            void Resize(usize newSize) {
                EnsureCapacity(newSize);
                mSize = newSize;
            }

            void Reserve(usize newCapacity) {
                if(newCapacity > mCapacity) {
                    Reallocate(newCapacity);
                }
            }

            const uint8* Data() const { return mBuffer; }
            uint8* Data() { return mBuffer; }

        private:
            void EnsureCapacity(usize required) {
                if(required > mCapacity) {
                    usize newCapacity = mCapacity * 2;
                    if(newCapacity < required) newCapacity = required;
                    Reallocate(newCapacity);
                }
            }

            void Reallocate(usize newCapacity) {
                uint8* newBuffer = new uint8[newCapacity];
                std::memcpy(newBuffer, mBuffer, mSize);
                if(mOwned) delete[] mBuffer;
                mBuffer = newBuffer;
                mCapacity = newCapacity;
                mOwned = true;
            }

            uint8* mBuffer;
            usize mCapacity;
            usize mSize;
            usize mPosition;
            bool mOwned;
        };


} // namespace nkentseu
