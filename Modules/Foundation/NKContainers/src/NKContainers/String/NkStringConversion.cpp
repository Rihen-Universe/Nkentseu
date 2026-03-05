// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\NkStringConversion.cpp
// DESCRIPTION: String encoding conversions
// AUTEUR: Rihen
// DATE: 2026-02-08
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#include "NkBasicString.h"
#include "Encoding/NkUTF8.h"
#include "Encoding/NkUTF16.h"
#include "Encoding/NkUTF32.h"

namespace nkentseu {
    namespace core {

        // ========================================
        // UTF-8 → UTF-16
        // ========================================

        String16 NkToUTF16(const String8& utf8Str) {
            if (utf8Str.IsEmpty()) return String16();
            
            // Estime taille nécessaire (pire cas: tous BMP = 1:1)
            usize maxUnits = utf8Str.Length();
            String16 result;
            result.Reserve(maxUnits);
            
            // Buffer temporaire
            uint16* buffer = new uint16[maxUnits + 1];
            
            usize bytesRead, unitsWritten;
            encoding::NkConversionResult convResult = encoding::utf8::NkToUTF16(
                utf8Str.Data(),
                utf8Str.Length(),
                buffer,
                maxUnits,
                bytesRead,
                unitsWritten
            );
            
            if (convResult == encoding::NkConversionResult::NK_SUCCESS ||
                convResult == encoding::NkConversionResult::NK_SOURCE_EXHAUSTED) {
                buffer[unitsWritten] = 0;
                result.Append(buffer, unitsWritten);
            }
            
            delete[] buffer;
            return result;
        }

        // ========================================
        // UTF-8 → UTF-32
        // ========================================

        String32 NkToUTF32(const String8& utf8Str) {
            if (utf8Str.IsEmpty()) return String32();
            
            // Estime taille (pire cas: tous ASCII = 1:1)
            usize maxChars = utf8Str.Length();
            String32 result;
            result.Reserve(maxChars);
            
            uint32* buffer = new uint32[maxChars + 1];
            
            usize bytesRead, charsWritten;
            encoding::NkConversionResult convResult = encoding::utf8::NkToUTF32(
                utf8Str.Data(),
                utf8Str.Length(),
                buffer,
                maxChars,
                bytesRead,
                charsWritten
            );
            
            if (convResult == encoding::NkConversionResult::NK_SUCCESS ||
                convResult == encoding::NkConversionResult::NK_SOURCE_EXHAUSTED) {
                buffer[charsWritten] = 0;
                result.Append(buffer, charsWritten);
            }
            
            delete[] buffer;
            return result;
        }

        // ========================================
        // UTF-16 → UTF-8
        // ========================================

        String8 NkToUTF8(const String16& utf16Str) {
            if (utf16Str.IsEmpty()) return String8();
            
            // Estime taille (pire cas: tous 4-bytes UTF-8 = 4:1)
            usize maxBytes = utf16Str.Length() * 4;
            String8 result;
            result.Reserve(maxBytes);
            
            Char* buffer = new Char[maxBytes + 1];
            
            usize unitsRead, bytesWritten;
            encoding::NkConversionResult convResult = encoding::utf16::NkToUTF8(
                reinterpret_cast<const uint16*>(utf16Str.Data()),
                utf16Str.Length(),
                buffer,
                maxBytes,
                unitsRead,
                bytesWritten
            );
            
            if (convResult == encoding::NkConversionResult::NK_SUCCESS ||
                convResult == encoding::NkConversionResult::NK_SOURCE_EXHAUSTED) {
                buffer[bytesWritten] = '\0';
                result.Append(buffer, bytesWritten);
            }
            
            delete[] buffer;
            return result;
        }

        // ========================================
        // UTF-32 → UTF-8
        // ========================================

        String8 NkToUTF8(const String32& utf32Str) {
            if (utf32Str.IsEmpty()) return String8();
            
            // Pire cas: 4 bytes par codepoint
            usize maxBytes = utf32Str.Length() * 4;
            String8 result;
            result.Reserve(maxBytes);
            
            Char* buffer = new Char[maxBytes + 1];
            
            usize charsRead, bytesWritten;
            encoding::NkConversionResult convResult = encoding::utf32::NkToUTF8(
                reinterpret_cast<const uint32*>(utf32Str.Data()),
                utf32Str.Length(),
                buffer,
                maxBytes,
                charsRead,
                bytesWritten
            );
            
            if (convResult == encoding::NkConversionResult::NK_SUCCESS ||
                convResult == encoding::NkConversionResult::NK_SOURCE_EXHAUSTED) {
                buffer[bytesWritten] = '\0';
                result.Append(buffer, bytesWritten);
            }
            
            delete[] buffer;
            return result;
        }

        // ========================================
        // WIDE STRING CONVERSIONS
        // ========================================

        #if defined(NK_PLATFORM_WINDOWS)
            // Windows: wchar_t = 2 bytes (UTF-16)
            
            WString NkToWide(const String8& str) {
                return NkBasicString<wchar>(
                    reinterpret_cast<const wchar*>(NkToUTF16(str).Data())
                );
            }
            
            String8 NkFromWide(const WString& wstr) {
                String16 utf16(
                    reinterpret_cast<const char16*>(wstr.Data()),
                    wstr.Length()
                );
                return NkToUTF8(utf16);
            }
            
        #else
            // Unix: wchar_t = 4 bytes (UTF-32)
            
            WString NkToWide(const String8& str) {
                return NkBasicString<wchar>(
                    reinterpret_cast<const wchar*>(NkToUTF32(str).Data())
                );
            }
            
            String8 NkFromWide(const WString& wstr) {
                String32 utf32(
                    reinterpret_cast<const char32*>(wstr.Data()),
                    wstr.Length()
                );
                return NkToUTF8(utf32);
            }
            
        #endif

    } // namespace core
} // namespace nkentseu