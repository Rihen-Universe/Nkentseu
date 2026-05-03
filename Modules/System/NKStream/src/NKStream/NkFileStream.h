#pragma once

#include "NKPlatform/NkPlatformDetect.h"
#include "NKPlatform/NkPlatformInline.h"
#include "NKCore/NkTypes.h"
#include <cstddef>

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

#include "NKStream/NkStream.h"

namespace nkentseu {

    ///////////////////////////////////////////////////////////////////////////////
    // Flux fichier multi-plateforme
    ///////////////////////////////////////////////////////////////////////////////

    class NKSTREAM_API NkFileStream : public NkStream {
        public:
            NkFileStream() = default;
            ~NkFileStream() override { Close(); }

            bool Open(const char* path, uint32 mode) override {
                #if defined(NKENTSEU_PLATFORM_WINDOWS)
                    DWORD dwDesiredAccess = 0;
                    DWORD dwCreationDisposition = 0;

                    if(mode & NK_READ_MODE) {
                        dwDesiredAccess |= GENERIC_READ;
                        dwCreationDisposition = OPEN_EXISTING;
                    }
                    if(mode & NK_WRITE_MODE) {
                        dwDesiredAccess |= GENERIC_WRITE;
                        dwCreationDisposition = (mode & NK_APPEND_MODE) ? OPEN_ALWAYS : CREATE_ALWAYS;
                    }

                    mHandle = CreateFileA(
                        path,
                        dwDesiredAccess,
                        FILE_SHARE_READ,
                        NULL,
                        dwCreationDisposition,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL
                    );

                    if((mode & NK_APPEND_MODE) && IsOpen()) {
                        LARGE_INTEGER li{ .QuadPart = 0 };
                        SetFilePointerEx(mHandle, li, NULL, FILE_END);
                    }

                #else // POSIX
                    int32 flags = 0;
                    mode_t permissions = 0644;

                    // Détermination du mode d'accès
                    if ((mode & NK_READE_MODE) && (mode & NK_WRITE_MODE)) {
                        flags |= O_RDWR;
                    } else if (mode & NK_READE_MODE) {
                        flags |= O_RDONLY;
                    } else if (mode & NK_WRITE_MODE) {
                        flags |= O_WRONLY;
                    }

                    // Gestion de la création et des modes append/truncate
                    if (mode & NK_WRITE_MODE) {
                        flags |= O_CREAT;
                        if (mode & AppendMode) {
                            flags |= O_APPEND;
                        } else {
                            flags |= O_TRUNC;
                        }
                    }

                    mHandle = open(path, flags, permissions);
                #endif

                return IsOpen();
            }


            void Close() override {
                if(IsOpen()) {
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
                    CloseHandle(mHandle);
        #else
                    close(mHandle);
        #endif
                    mHandle = InvalidHandle();
                }
            }

            bool IsOpen() const override {
                return mHandle != InvalidHandle();
            }

            usize ReadRaw(void* buffer, usize byteCount) override {
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
                DWORD bytesRead = 0;
                ReadFile(mHandle, buffer, static_cast<DWORD>(byteCount), &bytesRead, NULL);
                return bytesRead;
        #else
                return read(mHandle, buffer, byteCount);
        #endif
            }

            usize WriteRaw(const void* data, usize byteCount) override {
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
                DWORD bytesWritten = 0;
                WriteFile(mHandle, data, static_cast<DWORD>(byteCount), &bytesWritten, NULL);
                return bytesWritten;
        #else
                return write(mHandle, data, byteCount);
        #endif
            }

            bool Seek(usize position) override {
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
                LARGE_INTEGER li{.QuadPart = 0};
                li.QuadPart = position;
                return SetFilePointerEx(mHandle, li, NULL, FILE_BEGIN);
        #else
                return lseek(mHandle, position, SEEK_SET) != static_cast<off_t>(-1);
        #endif
            }

            usize Tell() const override {
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
                LARGE_INTEGER li{.QuadPart = 0};
                SetFilePointerEx(mHandle, li, &li, FILE_CURRENT);
                return li.QuadPart;
        #else
                return lseek(mHandle, 0, SEEK_CUR);
        #endif
            }

            usize Size() const override {
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
                LARGE_INTEGER size{.QuadPart = 0};
                GetFileSizeEx(mHandle, &size);
                return size.QuadPart;
        #else
                struct stat st;
                fstat(mHandle, &st);
                return st.st_size;
        #endif
            }

            bool IsEOF() const override {
                return Tell() >= Size();
            }

            void Flush() {
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
                FlushFileBuffers(mHandle);
        #else
                fsync(mHandle);
        #endif
            }

            bool SetEncoding(Encoding encoding) override {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                // Conversion des BOM pour Windows
                constexpr uint16 UTF16_BOM = 0xFEFF;
                constexpr uint8 UTF8_BOM[] = {0xEF, 0xBB, 0xBF};

                switch(encoding) {
                    case Encoding::NK_UTF16_LE:
                        if(IsWriteMode()) WriteRaw(&UTF16_BOM, sizeof(UTF16_BOM));
                        mEncoding = encoding;
                        return true;

                    case Encoding::NK_UTF8:
                        if(IsWriteMode()) WriteRaw(UTF8_BOM, sizeof(UTF8_BOM));
                        mEncoding = encoding;
                        return true;

                    default:
                        mEncoding = Encoding::NK_SYSTEM_DEFAULT;
                        return false;
                }
            #else
                // Linux/Mac utilise généralement UTF-8 par défaut
                mEncoding = (encoding == Encoding::NK_SYSTEM_DEFAULT) ?
                    Encoding::NK_UTF8 : encoding;
                return true;
            #endif
            }

            Encoding GetEncoding() const override {
                return mEncoding;
            }

        private:
            bool IsWriteMode() const {
                return mOpenMode & (NK_WRITE_MODE | NK_APPEND_MODE);
            }
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            using HandleType = HANDLE;
            static HANDLE InvalidHandle() { return INVALID_HANDLE_VALUE; }
        #else
            using HandleType = int;
            static int InvalidHandle() { return -1; }
        #endif

            HandleType mHandle = InvalidHandle();
            Encoding mEncoding = Encoding::NK_SYSTEM_DEFAULT;
    };


} // namespace nkentseu
