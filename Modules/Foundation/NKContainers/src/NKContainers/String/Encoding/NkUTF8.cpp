// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\Encoding\NkUTF8.cpp
// DESCRIPTION: UTF-8 implementation
// AUTEUR: Rihen
// DATE: 2026-02-07
// -----------------------------------------------------------------------------

#include "NkUTF8.h"
#include "NkUTF16.h"
#include "NkUTF32.h"

namespace nkentseu {
    
        namespace encoding {
            namespace utf8 {

                usize NkDecodeChar(const Char* str, uint32& outCodepoint) {
                    if (!str) return 0;
                    
                    uint8 byte1 = static_cast<uint8>(str[0]);
                    
                    // 1-byte sequence (0xxxxxxx)
                    if ((byte1 & 0x80) == 0) {
                        outCodepoint = byte1;
                        return 1;
                    }
                    
                    // 2-byte sequence (110xxxxx 10xxxxxx)
                    if ((byte1 & 0xE0) == 0xC0) {
                        if (!NkIsContinuationByte(str[1])) return 0;
                        outCodepoint = ((byte1 & 0x1F) << 6) | (str[1] & 0x3F);
                        return outCodepoint >= 0x80 ? 2 : 0;  // Overlong check
                    }
                    
                    // 3-byte sequence (1110xxxx 10xxxxxx 10xxxxxx)
                    if ((byte1 & 0xF0) == 0xE0) {
                        if (!NkIsContinuationByte(str[1]) || !NkIsContinuationByte(str[2])) return 0;
                        outCodepoint = ((byte1 & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
                        if (outCodepoint < 0x800) return 0;  // Overlong
                        if (outCodepoint >= 0xD800 && outCodepoint <= 0xDFFF) return 0;  // Surrogate
                        return 3;
                    }
                    
                    // 4-byte sequence (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
                    if ((byte1 & 0xF8) == 0xF0) {
                        if (!NkIsContinuationByte(str[1]) || !NkIsContinuationByte(str[2]) || !NkIsContinuationByte(str[3]))
                            return 0;
                        outCodepoint = ((byte1 & 0x07) << 18) | ((str[1] & 0x3F) << 12) |
                                    ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
                        if (outCodepoint < 0x10000 || outCodepoint > 0x10FFFF) return 0;
                        return 4;
                    }
                    
                    return 0;  // Invalid
                }

                usize NkEncodeChar(uint32 codepoint, Char* outBuffer) {
                    if (!outBuffer || !NkIsValidCodepoint(codepoint)) return 0;
                    
                    if (codepoint < 0x80) {
                        outBuffer[0] = static_cast<Char>(codepoint);
                        return 1;
                    }
                    
                    if (codepoint < 0x800) {
                        outBuffer[0] = static_cast<Char>(0xC0 | (codepoint >> 6));
                        outBuffer[1] = static_cast<Char>(0x80 | (codepoint & 0x3F));
                        return 2;
                    }
                    
                    if (codepoint < 0x10000) {
                        outBuffer[0] = static_cast<Char>(0xE0 | (codepoint >> 12));
                        outBuffer[1] = static_cast<Char>(0x80 | ((codepoint >> 6) & 0x3F));
                        outBuffer[2] = static_cast<Char>(0x80 | (codepoint & 0x3F));
                        return 3;
                    }
                    
                    outBuffer[0] = static_cast<Char>(0xF0 | (codepoint >> 18));
                    outBuffer[1] = static_cast<Char>(0x80 | ((codepoint >> 12) & 0x3F));
                    outBuffer[2] = static_cast<Char>(0x80 | ((codepoint >> 6) & 0x3F));
                    outBuffer[3] = static_cast<Char>(0x80 | (codepoint & 0x3F));
                    return 4;
                }

                usize NkCharLength(Char firstByte) {
                    uint8 byte = static_cast<uint8>(firstByte);
                    if ((byte & 0x80) == 0) return 1;
                    if ((byte & 0xE0) == 0xC0) return 2;
                    if ((byte & 0xF0) == 0xE0) return 3;
                    if ((byte & 0xF8) == 0xF0) return 4;
                    return 0;
                }

                usize NkCodepointLength(uint32 codepoint) {
                    if (codepoint < 0x80) return 1;
                    if (codepoint < 0x800) return 2;
                    if (codepoint < 0x10000) return 3;
                    if (codepoint <= 0x10FFFF) return 4;
                    return 0;
                }

                usize NkCountChars(const Char* str, usize byteLength) {
                    if (!str) return 0;
                    
                    usize count = 0;
                    usize pos = 0;
                    
                    while (pos < byteLength) {
                        uint32 codepoint;
                        usize bytes = NkDecodeChar(str + pos, codepoint);
                        if (bytes == 0) break;  // Invalid sequence
                        pos += bytes;
                        ++count;
                    }
                    
                    return count;
                }

                const Char* NkNextChar(const Char* str) {
                    if (!str || *str == '\0') return str;
                    
                    usize len = NkCharLength(*str);
                    return len > 0 ? str + len : str + 1;
                }

                const Char* NkPrevChar(const Char* str, const Char* start) {
                    if (!str || str <= start) return str;
                    
                    const Char* ptr = str - 1;
                    while (ptr > start && NkIsContinuationByte(*ptr)) {
                        --ptr;
                    }
                    return ptr;
                }

                bool NkIsValid(const Char* str, usize length) {
                    if (!str) return false;
                    
                    usize pos = 0;
                    while (pos < length) {
                        uint32 codepoint;
                        usize bytes = NkDecodeChar(str + pos, codepoint);
                        if (bytes == 0) return false;
                        pos += bytes;
                    }
                    return true;
                }

                bool NkIsValidCodepoint(uint32 codepoint) {
                    return codepoint <= 0x10FFFF && !(codepoint >= 0xD800 && codepoint <= 0xDFFF);
                }

                NkConversionResult NkToUTF16(const Char* src, usize srcLen, uint16* dst, usize dstLen,
                                            usize& bytesRead, usize& charsWritten) {
                    if (!src || !dst) return NkConversionResult::NK_SOURCE_ILLEGAL;
                    
                    bytesRead = 0;
                    charsWritten = 0;
                    
                    while (bytesRead < srcLen && charsWritten < dstLen) {
                        uint32 codepoint;
                        usize bytes = NkDecodeChar(src + bytesRead, codepoint);
                        
                        if (bytes == 0) return NkConversionResult::NK_SOURCE_ILLEGAL;
                        
                        usize units = utf16::NkEncodeChar(codepoint, dst + charsWritten);
                        if (units == 0) return NkConversionResult::NK_SOURCE_ILLEGAL;
                        if (charsWritten + units > dstLen) return NkConversionResult::NK_TARGET_EXHAUSTED;
                        
                        bytesRead += bytes;
                        charsWritten += units;
                    }
                    
                    return bytesRead < srcLen ? NkConversionResult::NK_SOURCE_EXHAUSTED : NkConversionResult::NK_SUCCESS;
                }

                NkConversionResult NkToUTF32(const Char* src, usize srcLen, uint32* dst, usize dstLen,
                                            usize& bytesRead, usize& charsWritten) {
                    if (!src || !dst) return NkConversionResult::NK_SOURCE_ILLEGAL;
                    
                    bytesRead = 0;
                    charsWritten = 0;
                    
                    while (bytesRead < srcLen && charsWritten < dstLen) {
                        uint32 codepoint;
                        usize bytes = NkDecodeChar(src + bytesRead, codepoint);
                        
                        if (bytes == 0) return NkConversionResult::NK_SOURCE_ILLEGAL;
                        
                        dst[charsWritten++] = codepoint;
                        bytesRead += bytes;
                    }
                    
                    return bytesRead < srcLen ? NkConversionResult::NK_SOURCE_EXHAUSTED : NkConversionResult::NK_SUCCESS;
                }

                NkConversionResult NkFromUTF16(const uint16* src, usize srcLen, Char* dst, usize dstLen,
                                                usize& charsRead, usize& bytesWritten) {
                    if (!src || !dst) return NkConversionResult::NK_SOURCE_ILLEGAL;
                    
                    charsRead = 0;
                    bytesWritten = 0;
                    
                    while (charsRead < srcLen && bytesWritten < dstLen) {
                        uint32 codepoint;
                        usize units = utf16::NkDecodeChar(src + charsRead, codepoint);
                        
                        if (units == 0) return NkConversionResult::NK_SOURCE_ILLEGAL;
                        
                        Char buffer[4];
                        usize bytes = NkEncodeChar(codepoint, buffer);
                        if (bytes == 0) return NkConversionResult::NK_SOURCE_ILLEGAL;
                        if (bytesWritten + bytes > dstLen) return NkConversionResult::NK_TARGET_EXHAUSTED;
                        
                        for (usize i = 0; i < bytes; ++i) {
                            dst[bytesWritten++] = buffer[i];
                        }
                        charsRead += units;
                    }
                    
                    return charsRead < srcLen ? NkConversionResult::NK_SOURCE_EXHAUSTED : NkConversionResult::NK_SUCCESS;
                }

                NkConversionResult NkFromUTF32(const uint32* src, usize srcLen, Char* dst, usize dstLen,
                                                usize& charsRead, usize& bytesWritten) {
                    if (!src || !dst) return NkConversionResult::NK_SOURCE_ILLEGAL;
                    
                    charsRead = 0;
                    bytesWritten = 0;
                    
                    while (charsRead < srcLen && bytesWritten < dstLen) {
                        Char buffer[4];
                        usize bytes = NkEncodeChar(src[charsRead], buffer);
                        
                        if (bytes == 0) return NkConversionResult::NK_SOURCE_ILLEGAL;
                        if (bytesWritten + bytes > dstLen) return NkConversionResult::NK_TARGET_EXHAUSTED;
                        
                        for (usize i = 0; i < bytes; ++i) {
                            dst[bytesWritten++] = buffer[i];
                        }
                        ++charsRead;
                    }
                    
                    return charsRead < srcLen ? NkConversionResult::NK_SOURCE_EXHAUSTED : NkConversionResult::NK_SUCCESS;
                }

            } // namespace utf8
        } // namespace encoding
    
} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================