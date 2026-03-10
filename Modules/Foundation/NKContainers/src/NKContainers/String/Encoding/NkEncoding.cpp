// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\Encoding\NkEncoding.cpp
// DESCRIPTION: Encoding base implementation
// AUTEUR: Rihen
// DATE: 2026-02-07
// -----------------------------------------------------------------------------

#include "NkEncoding.h"
#include "NkASCII.h"
#include "NkUTF8.h"
#include "NkUTF16.h"
#include "NkUTF32.h"

namespace nkentseu {
    
        namespace encoding {

            // BOM signatures
            static const uint8 NK_UTF8_BOM[] = {0xEF, 0xBB, 0xBF};
            static const uint8 NK_UTF16LE_BOM[] = {0xFF, 0xFE};
            static const uint8 NK_UTF16BE_BOM[] = {0xFE, 0xFF};
            static const uint8 NK_UTF32LE_BOM[] = {0xFF, 0xFE, 0x00, 0x00};
            static const uint8 NK_UTF32BE_BOM[] = {0x00, 0x00, 0xFE, 0xFF};

            NkEncodingType NkDetectBOM(const void* data, usize size) {
                if (!data || size == 0) return NkEncodingType::NK_UNKNOWN;
                
                const uint8* bytes = static_cast<const uint8*>(data);
                
                // UTF-32 (4 bytes) - check first
                if (size >= 4) {
                    if (bytes[0] == 0xFF && bytes[1] == 0xFE && bytes[2] == 0x00 && bytes[3] == 0x00)
                        return NkEncodingType::NK_UTF32LE;
                    if (bytes[0] == 0x00 && bytes[1] == 0x00 && bytes[2] == 0xFE && bytes[3] == 0xFF)
                        return NkEncodingType::NK_UTF32BE;
                }
                
                // UTF-8 (3 bytes)
                if (size >= 3) {
                    if (bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
                        return NkEncodingType::NK_UTF8;
                }
                
                // UTF-16 (2 bytes)
                if (size >= 2) {
                    if (bytes[0] == 0xFF && bytes[1] == 0xFE)
                        return NkEncodingType::NK_UTF16LE;
                    if (bytes[0] == 0xFE && bytes[1] == 0xFF)
                        return NkEncodingType::NK_UTF16BE;
                }
                
                return NkEncodingType::NK_UNKNOWN;
            }

            Bool NkIsValidASCII(const Char* str, usize length) {
                return ascii::NkIsValid(str, length);
            }

            Bool NkIsValidUTF8(const Char* str, usize length) {
                return utf8::NkIsValid(str, length);
            }

            Bool NkIsValidUTF16(const uint16* str, usize length) {
                return utf16::NkIsValid(str, length);
            }

            Bool NkIsValidUTF32(const uint32* str, usize length) {
                return utf32::NkIsValid(str, length);
            }

        } // namespace encoding
    
} // namespace nkentseu