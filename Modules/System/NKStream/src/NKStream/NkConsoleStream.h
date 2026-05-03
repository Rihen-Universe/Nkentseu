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
    // Flux console avec gestion d'encodage
    ///////////////////////////////////////////////////////////////////////////////

    class NKSTREAM_API NkConsoleStream : public NkStream {
    public:
        enum StreamType { NK_INPUT, NK_OUTPUT, NK_ERROR };

        NkConsoleStream(StreamType type) : mType(type) {
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
            mHandle = GetStdHandle(
                type == NK_INPUT ? STD_INPUT_HANDLE :
                type == NK_INPUT ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE
            );
    #else
            mHandle = type == NK_INPUT ? 0 : type == NK_ERROR ? 2 : 1;
    #endif
        }

        bool Open(const char*, uint32) override {
            // Toujours ouvert pour la console
            return true;
        }

        void Close() override {
            // Ne ferme pas les handles système
        }

        bool IsOpen() const override {
            return true;
        }

        usize ReadRaw(void* buffer, usize byteCount) override {
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
            DWORD read = 0;
            if(mType == NK_INPUT) {
                ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE), buffer,
                            byteCount / sizeof(wchar_t), &read, NULL);
                return read * sizeof(wchar_t);
            }
            return 0;
    #else
            return read(mHandle, buffer, byteCount);
    #endif
        }

        usize WriteRaw(const void* data, usize byteCount) override {
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
            DWORD written = 0;
            const HANDLE hConsole = GetStdHandle(
                mType == NK_ERROR ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE
            );

            if(mEncoding == Encoding::NK_UTF16_LE) {
                WriteConsoleW(hConsole, data, byteCount / sizeof(wchar_t), &written, NULL);
                return written * sizeof(wchar_t);
            }
            else {
                WriteConsoleA(hConsole, data, byteCount, &written, NULL);
                return written;
            }
    #else
            return write(mHandle, data, byteCount);
    #endif
        }

        template<typename CharT>
        usize Read(CharT* buffer, usize count) {
            return ReadRaw(buffer, count * sizeof(CharT)) / sizeof(CharT);
        }

        template<typename CharT>
        usize Write(const CharT* data, usize count) {
            return WriteRaw(data, count * sizeof(CharT)) / sizeof(CharT);
        }

        bool Seek(usize) override { return false; } // Non supporté
        usize Tell() const override { return 0; }
        usize Size() const override { return 0; }

        bool SetEncoding(Encoding encoding) override {
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            switch(encoding) {
                case Encoding::NK_UTF8:
                    SetConsoleOutputCP(CP_UTF8);
                    mEncoding = encoding;
                    return true;

                case Encoding::NK_UTF16_LE:
                    mEncoding = encoding;
                    return true; // Utilise WriteConsoleW automatiquement

                default:
                    SetConsoleOutputCP(CP_ACP); // ANSI par défaut
                    mEncoding = Encoding::NK_SYSTEM_DEFAULT;
                    return false;
            }
        #else
            // Sous Unix, on suppose UTF-8 pour la console
            mEncoding = Encoding::NK_UTF8;
            return (encoding == Encoding::NK_UTF8 || encoding == Encoding::NK_SYSTEM_DEFAULT);
        #endif
        }

        Encoding GetEncoding() const override {
            return mEncoding;
        }

    private:
        Encoding mEncoding = Encoding::NK_SYSTEM_DEFAULT;
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        HANDLE mHandle;
    #else
        int mHandle;
    #endif
        StreamType mType;
    };

} // namespace nkentseu
