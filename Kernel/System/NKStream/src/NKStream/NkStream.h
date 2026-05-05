#pragma once

#include "NKPlatform/NkPlatformDetect.h"
#include "NKPlatform/NkPlatformInline.h"
#include "NKCore/NkTypes.h"
#include "NKStream/NkStreamExport.h"
#include <cstddef>

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

namespace nkentseu {

    enum class Encoding {
        NK_SYSTEM_DEFAULT = 0,
        NK_UTF8,
        NK_UTF16_LE,
        NK_UTF16_BE,
    };

    ///////////////////////////////////////////////////////////////////////////////
    // Interface de base pour les flux
    ///////////////////////////////////////////////////////////////////////////////

    class NKSTREAM_API NkStream {
        public:
            enum OpenMode {
                NK_READ_MODE = 0x01,    // Changé de Read
                NK_WRITE_MODE = 0x02,   // Changé de Write
                NK_APPEND_MODE = 0x04,
                NK_BINARY_MODE = 0x08,
                NK_TEXT_MODE = 0x10,
            };

            virtual ~NkStream() = default;

            virtual bool Open(const char* path, uint32 mode) = 0;
            virtual void Close() = 0;
            virtual bool IsOpen() const = 0;

            template<typename T>
            usize Read(T* buffer, usize count) {
                return ReadRaw(buffer, count * sizeof(T)) / sizeof(T);
            }

            template<typename T>
            usize Write(const T* data, usize count) {
                return WriteRaw(data, count * sizeof(T)) / sizeof(T);
            }

            virtual usize ReadRaw(void* buffer, usize byteCount) = 0;
            virtual usize WriteRaw(const void* data, usize byteCount) = 0;

            virtual bool Seek(usize position) = 0;
            virtual usize Tell() const = 0;
            virtual usize Size() const = 0;
            virtual bool IsEOF() const = 0;

            virtual bool SetEncoding(Encoding){
                return true;
            }

            virtual Encoding GetEncoding() const {
                return Encoding::NK_SYSTEM_DEFAULT;
            }
        protected:
            uint32 mOpenMode = 0;
    };

} // namespace nkentseu
