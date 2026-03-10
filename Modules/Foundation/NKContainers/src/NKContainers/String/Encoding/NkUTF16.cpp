// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\Encoding\NkUTF16.cpp
// DESCRIPTION: UTF-16 implementation
// AUTEUR: Rihen
// DATE: 2026-02-07
// -----------------------------------------------------------------------------

#include "NkUTF16.h"
#include "NkUTF8.h"
#include "NkUTF32.h"

namespace nkentseu {
    
        namespace encoding {
            namespace utf16 {

                usize NkDecodeChar(const uint16* str, uint32& outCodepoint) {
                    if (!str) return 0;
                    
                    uint16 unit1 = str[0];
                    
                    // BMP (0x0000 - 0xD7FF, 0xE000 - 0xFFFF)
                    if (!NkIsSurrogate(unit1)) {
                        outCodepoint = unit1;
                        return 1;
                    }
                    
                    // Surrogate pair (0x10000 - 0x10FFFF)
                    if (NkIsHighSurrogate(unit1)) {
                        uint16 unit2 = str[1];
                        if (!NkIsLowSurrogate(unit2)) return 0;  // Invalid
                        
                        outCodepoint = 0x10000 + ((unit1 & 0x3FF) << 10) + (unit2 & 0x3FF);
                        return 2;
                    }
                    
                    return 0;  // Invalid (low surrogate without high)
                }

                usize NkEncodeChar(uint32 codepoint, uint16* outBuffer) {
                    if (!outBuffer) return 0;
                    
                    // BMP
                    if (codepoint < 0x10000) {
                        if (codepoint >= 0xD800 && codepoint <= 0xDFFF) return 0;  // Surrogate range
                        outBuffer[0] = static_cast<uint16>(codepoint);
                        return 1;
                    }
                    
                    // Supplementary planes
                    if (codepoint > 0x10FFFF) return 0;  // Invalid
                    
                    codepoint -= 0x10000;
                    outBuffer[0] = static_cast<uint16>(0xD800 + ((codepoint >> 10) & 0x3FF));
                    outBuffer[1] = static_cast<uint16>(0xDC00 + (codepoint & 0x3FF));
                    return 2;
                }

                usize NkCharLength(uint16 firstUnit) {
                    if (!NkIsSurrogate(firstUnit)) return 1;
                    if (NkIsHighSurrogate(firstUnit)) return 2;
                    return 0;  // Invalid (low surrogate)
                }

                usize NkCodepointLength(uint32 codepoint) {
                    if (codepoint < 0x10000) {
                        if (codepoint >= 0xD800 && codepoint <= 0xDFFF) return 0;
                        return 1;
                    }
                    if (codepoint <= 0x10FFFF) return 2;
                    return 0;
                }

                usize NkCountChars(const uint16* str, usize unitLength) {
                    if (!str) return 0;
                    
                    usize count = 0;
                    usize pos = 0;
                    
                    while (pos < unitLength) {
                        uint32 codepoint;
                        usize units = NkDecodeChar(str + pos, codepoint);
                        if (units == 0) break;
                        pos += units;
                        ++count;
                    }
                    
                    return count;
                }

                const uint16* NkNextChar(const uint16* str) {
                    if (!str) return str;
                    
                    usize len = NkCharLength(*str);
                    return len > 0 ? str + len : str + 1;
                }

                const uint16* NkPrevChar(const uint16* str, const uint16* start) {
                    if (!str || str <= start) return str;
                    
                    const uint16* ptr = str - 1;
                    if (ptr > start && NkIsLowSurrogate(*ptr) && NkIsHighSurrogate(*(ptr - 1))) {
                        --ptr;
                    }
                    return ptr;
                }

                bool NkIsValid(const uint16* str, usize length) {
                    if (!str) return false;
                    
                    usize pos = 0;
                    while (pos < length) {
                        uint32 codepoint;
                        usize units = NkDecodeChar(str + pos, codepoint);
                        if (units == 0) return false;
                        pos += units;
                    }
                    return true;
                }

                NkConversionResult NkToUTF8(const uint16* src, usize srcLen, Char* dst, usize dstLen,
                                            usize& charsRead, usize& bytesWritten) {
                    if (!src || !dst) return NkConversionResult::NK_SOURCE_ILLEGAL;
                    
                    charsRead = 0;
                    bytesWritten = 0;
                    
                    while (charsRead < srcLen && bytesWritten < dstLen) {
                        uint32 codepoint;
                        usize units = NkDecodeChar(src + charsRead, codepoint);
                        
                        if (units == 0) return NkConversionResult::NK_SOURCE_ILLEGAL;
                        
                        Char buffer[4];
                        usize bytes = utf8::NkEncodeChar(codepoint, buffer);
                        if (bytes == 0) return NkConversionResult::NK_SOURCE_ILLEGAL;
                        if (bytesWritten + bytes > dstLen) return NkConversionResult::NK_TARGET_EXHAUSTED;
                        
                        for (usize i = 0; i < bytes; ++i) {
                            dst[bytesWritten++] = buffer[i];
                        }
                        charsRead += units;
                    }
                    
                    return charsRead < srcLen ? NkConversionResult::NK_SOURCE_EXHAUSTED : NkConversionResult::NK_SUCCESS;
                }

                NkConversionResult NkToUTF32(const uint16* src, usize srcLen, uint32* dst, usize dstLen,
                                            usize& charsRead, usize& unitsWritten) {
                    if (!src || !dst) return NkConversionResult::NK_SOURCE_ILLEGAL;
                    
                    charsRead = 0;
                    unitsWritten = 0;
                    
                    while (charsRead < srcLen && unitsWritten < dstLen) {
                        uint32 codepoint;
                        usize units = NkDecodeChar(src + charsRead, codepoint);
                        
                        if (units == 0) return NkConversionResult::NK_SOURCE_ILLEGAL;
                        
                        dst[unitsWritten++] = codepoint;
                        charsRead += units;
                    }
                    
                    return charsRead < srcLen ? NkConversionResult::NK_SOURCE_EXHAUSTED : NkConversionResult::NK_SUCCESS;
                }

            } // namespace utf16
        } // namespace encoding
    
} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================