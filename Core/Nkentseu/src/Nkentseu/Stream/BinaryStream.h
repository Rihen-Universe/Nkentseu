#pragma once

#include "Nkentseu/Platform/Types.h"
#include "Nkentseu/Platform/Inline.h"
#include <cstddef>

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

#include "Stream.h"
#include "Nkentseu/String/StringUtils.h"
#include "Nkentseu/Memory/Memory.h"


namespace nkentseu {

    class NKENTSEU_API BinaryStream : public Stream {
        public:
            BinaryStream(void* data = nullptr, usize capacity = 0) 
                : m_Buffer(static_cast<uint8*>(data)), 
                  m_Capacity(capacity),
                  m_Size(0),
                  m_Position(0),
                  m_Owned(false) {}
        
            BinaryStream(usize capacity) 
                : m_Buffer(new uint8[capacity]),
                  m_Capacity(capacity),
                  m_Size(0),
                  m_Position(0),
                  m_Owned(true) {}
        
            ~BinaryStream() override {
                if(m_Owned) delete[] m_Buffer;
            }
        
            bool Open(const char*, uint32) override { return true; }
            void Close() override { /* Ne libère pas la mémoire */ }
            bool IsOpen() const override { return true; }
        
            usize ReadRaw(void* buffer, usize byteCount) override {
                const usize available = m_Size - m_Position;
                const usize toRead = (byteCount > available) ? available : byteCount;
                
                MemorySystem::Copy(buffer, m_Buffer + m_Position, toRead);
                m_Position += toRead;
                return toRead;
            }
        
            usize WriteRaw(const void* data, usize byteCount) override {
                EnsureCapacity(m_Position + byteCount);
                
                MemorySystem::Copy(m_Buffer + m_Position, data, byteCount);
                m_Position += byteCount;
                if(m_Position > m_Size) m_Size = m_Position;
                return byteCount;
            }
        
            bool Seek(usize position) override {
                if(position > m_Size) return false;
                m_Position = position;
                return true;
            }
        
            usize Tell() const override { return m_Position; }
            usize Size() const override { return m_Size; }
            bool IsEOF() const override { return m_Position >= m_Size; }
        
            void Resize(usize newSize) {
                EnsureCapacity(newSize);
                m_Size = newSize;
            }
        
            void Reserve(usize newCapacity) {
                if(newCapacity > m_Capacity) {
                    Reallocate(newCapacity);
                }
            }
        
            const uint8* Data() const { return m_Buffer; }
            uint8* Data() { return m_Buffer; }
        
        private:
            void EnsureCapacity(usize required) {
                if(required > m_Capacity) {
                    usize newCapacity = m_Capacity * 2;
                    if(newCapacity < required) newCapacity = required;
                    Reallocate(newCapacity);
                }
            }
        
            void Reallocate(usize newCapacity) {
                uint8* newBuffer = new uint8[newCapacity];
                MemorySystem::Copy(newBuffer, m_Buffer, m_Size);
                if(m_Owned) delete[] m_Buffer;
                m_Buffer = newBuffer;
                m_Capacity = newCapacity;
                m_Owned = true;
            }
        
            uint8* m_Buffer;
            usize m_Capacity;
            usize m_Size;
            usize m_Position;
            bool m_Owned;
        };
        

} // namespace nkentseu