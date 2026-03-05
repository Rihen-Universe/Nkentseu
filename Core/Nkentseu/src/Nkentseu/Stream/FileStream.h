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

namespace nkentseu {

    ///////////////////////////////////////////////////////////////////////////////
    // Flux fichier multi-plateforme
    ///////////////////////////////////////////////////////////////////////////////

    class NKENTSEU_API FileStream : public Stream {
    public:
        FileStream() = default;
        ~FileStream() override { Close(); }

        bool Open(const char* path, uint32 mode) override {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                DWORD dwDesiredAccess = 0;
                DWORD dwCreationDisposition = 0;
            
                if(mode & ReadMode) {
                    dwDesiredAccess |= GENERIC_READ;
                    dwCreationDisposition = OPEN_EXISTING;
                }
                if(mode & WriteMode) {
                    dwDesiredAccess |= GENERIC_WRITE;
                    dwCreationDisposition = (mode & AppendMode) ? OPEN_ALWAYS : CREATE_ALWAYS;
                }
            
                m_Handle = CreateFileA(
                    path,
                    dwDesiredAccess,
                    FILE_SHARE_READ,
                    NULL,
                    dwCreationDisposition,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL
                );
            
                if((mode & AppendMode) && IsOpen()) {
                    LARGE_INTEGER li{ .QuadPart = 0 };
                    SetFilePointerEx(m_Handle, li, NULL, FILE_END);
                }
            
            #else // POSIX
                int32 flags = 0;
                mode_t permissions = 0644;

                // Détermination du mode d'accès
                if ((mode & ReadMode) && (mode & WriteMode)) {
                    flags |= O_RDWR;
                } else if (mode & ReadMode) {
                    flags |= O_RDONLY;
                } else if (mode & WriteMode) {
                    flags |= O_WRONLY;
                }

                // Gestion de la création et des modes append/truncate
                if (mode & WriteMode) {
                    flags |= O_CREAT;
                    if (mode & AppendMode) {
                        flags |= O_APPEND;
                    } else {
                        flags |= O_TRUNC;
                    }
                }

                m_Handle = open(path, flags, permissions);
            #endif
            
            return IsOpen();
        }
            

        void Close() override {
            if(IsOpen()) {
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
                CloseHandle(m_Handle);
    #else
                close(m_Handle);
    #endif
                m_Handle = InvalidHandle();
            }
        }

        bool IsOpen() const override {
            return m_Handle != InvalidHandle();
        }

        usize ReadRaw(void* buffer, usize byteCount) override {
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
            DWORD bytesRead = 0;
            ReadFile(m_Handle, buffer, static_cast<DWORD>(byteCount), &bytesRead, NULL);
            return bytesRead;
    #else
            return read(m_Handle, buffer, byteCount);
    #endif
        }

        usize WriteRaw(const void* data, usize byteCount) override {
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
            DWORD bytesWritten = 0;
            WriteFile(m_Handle, data, static_cast<DWORD>(byteCount), &bytesWritten, NULL);
            return bytesWritten;
    #else
            return write(m_Handle, data, byteCount);
    #endif
        }

        bool Seek(usize position) override {
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
            LARGE_INTEGER li{.QuadPart = 0};
            li.QuadPart = position;
            return SetFilePointerEx(m_Handle, li, NULL, FILE_BEGIN);
    #else
            return lseek(m_Handle, position, SEEK_SET) != static_cast<off_t>(-1);
    #endif
        }

        usize Tell() const override {
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
            LARGE_INTEGER li{.QuadPart = 0};
            SetFilePointerEx(m_Handle, li, &li, FILE_CURRENT);
            return li.QuadPart;
    #else
            return lseek(m_Handle, 0, SEEK_CUR);
    #endif
        }

        usize Size() const override {
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
            LARGE_INTEGER size{.QuadPart = 0};
            GetFileSizeEx(m_Handle, &size);
            return size.QuadPart;
    #else
            struct stat st;
            fstat(m_Handle, &st);
            return st.st_size;
    #endif
        }

        bool IsEOF() const override {
            return Tell() >= Size();
        }

        void Flush() {
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
            FlushFileBuffers(m_Handle);
    #else
            fsync(m_Handle);
    #endif
        }

        bool SetEncoding(Encoding encoding) override {
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            // Conversion des BOM pour Windows
            constexpr uint16 UTF16_BOM = 0xFEFF;
            constexpr uint8 UTF8_BOM[] = {0xEF, 0xBB, 0xBF};
    
            switch(encoding) {
                case Encoding::UTF16_LE:
                    if(IsWriteMode()) WriteRaw(&UTF16_BOM, sizeof(UTF16_BOM));
                    m_Encoding = encoding;
                    return true;
                
                case Encoding::UTF8:
                    if(IsWriteMode()) WriteRaw(UTF8_BOM, sizeof(UTF8_BOM));
                    m_Encoding = encoding;
                    return true;
                
                default:
                    m_Encoding = Encoding::SystemDefault;
                    return false;
            }
        #else
            // Linux/Mac utilise généralement UTF-8 par défaut
            m_Encoding = (encoding == Encoding::SystemDefault) ? 
                Encoding::UTF8 : encoding;
            return true;
        #endif
        }
        
        Encoding GetEncoding() const override {
            return m_Encoding;
        }            

    private:
        bool IsWriteMode() const {
            return m_OpenMode & (WriteMode | AppendMode);
        }
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        using HandleType = HANDLE;
        static HANDLE InvalidHandle() { return INVALID_HANDLE_VALUE; }
    #else
        using HandleType = int;
        static int InvalidHandle() { return -1; }
    #endif

        HandleType m_Handle = InvalidHandle();
        Encoding m_Encoding = Encoding::SystemDefault;
    };


} // namespace nkentseu