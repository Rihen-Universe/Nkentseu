// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\Encoding\NkASCII.h
// DESCRIPTION: ASCII encoding utilities
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_STRING_ENCODING_NKASCII_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_STRING_ENCODING_NKASCII_H_INCLUDED

#include "NKContainers/NkCompat.h"
#include "NkEncoding.h"

namespace nkentseu {
    namespace core {
        namespace encoding {
            namespace ascii {
                
                /**
                 * @brief Vérifie si caractère ASCII (0-127)
                 */
                NKENTSEU_FORCE_INLINE Bool NkIsASCII(Char ch) NK_NOEXCEPT {
                    return static_cast<unsigned char>(ch) < 128;
                }
                
                /**
                 * @brief Vérifie si chaîne est pure ASCII
                 */
                NKENTSEU_CORE_API Bool NkIsValid(const Char* str, usize length) NK_NOEXCEPT;
                
                /**
                 * @brief Prédicats caractères ASCII
                 */
                NKENTSEU_FORCE_INLINE Bool NkIsControl(Char ch) NK_NOEXCEPT {
                    return (ch >= 0 && ch <= 31) || ch == 127;
                }
                
                NKENTSEU_FORCE_INLINE Bool NkIsPrint32able(Char ch) NK_NOEXCEPT {
                    return ch >= 32 && ch <= 126;
                }
                
                NKENTSEU_FORCE_INLINE Bool NkIsWhitespace(Char ch) NK_NOEXCEPT {
                    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || 
                        ch == '\v' || ch == '\f';
                }
                
                NKENTSEU_FORCE_INLINE Bool NkIsDigit(Char ch) NK_NOEXCEPT {
                    return ch >= '0' && ch <= '9';
                }
                
                NKENTSEU_FORCE_INLINE Bool NkIsAlpha(Char ch) NK_NOEXCEPT {
                    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
                }
                
                NKENTSEU_FORCE_INLINE Bool NkIsAlphaNumeric(Char ch) NK_NOEXCEPT {
                    return NkIsAlpha(ch) || NkIsDigit(ch);
                }
                
                NKENTSEU_FORCE_INLINE Bool NkIsUpper(Char ch) NK_NOEXCEPT {
                    return ch >= 'A' && ch <= 'Z';
                }
                
                NKENTSEU_FORCE_INLINE Bool NkIsLower(Char ch) NK_NOEXCEPT {
                    return ch >= 'a' && ch <= 'z';
                }
                
                NKENTSEU_FORCE_INLINE Bool NkIsHexDigit(Char ch) NK_NOEXCEPT {
                    return NkIsDigit(ch) || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f');
                }
                
                NKENTSEU_FORCE_INLINE Bool NkIsPunctuation(Char ch) NK_NOEXCEPT {
                    return (ch >= 33 && ch <= 47) ||   // !"#$%&'()*+,-./
                        (ch >= 58 && ch <= 64) ||   // :;<=>?@
                        (ch >= 91 && ch <= 96) ||   // [\]^_`
                        (ch >= 123 && ch <= 126);   // {|}~
                }
                
                /**
                 * @brief Conversions case
                 */
                NKENTSEU_FORCE_INLINE Char NkToUpper(Char ch) NK_NOEXCEPT {
                    return NkIsLower(ch) ? (ch - 32) : ch;
                }
                
                NKENTSEU_FORCE_INLINE Char NkToLower(Char ch) NK_NOEXCEPT {
                    return NkIsUpper(ch) ? (ch + 32) : ch;
                }
                
                /**
                 * @brief Conversion vers digit
                 */
                NKENTSEU_FORCE_INLINE int32 NkToDigit(Char ch) NK_NOEXCEPT {
                    return NkIsDigit(ch) ? (ch - '0') : -1;
                }
                
                NKENTSEU_FORCE_INLINE int32 NkToHexDigit(Char ch) NK_NOEXCEPT {
                    if (ch >= '0' && ch <= '9') return ch - '0';
                    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
                    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
                    return -1;
                }
                
                /**
                 * @brief Conversion depuis digit
                 */
                NKENTSEU_FORCE_INLINE Char NkFromDigit(int32 digit) NK_NOEXCEPT {
                    return (digit >= 0 && digit <= 9) ? ('0' + digit) : '\0';
                }
                
                NKENTSEU_FORCE_INLINE Char NkFromHexDigit(int32 digit, Bool uppercase = true) NK_NOEXCEPT {
                    if (digit >= 0 && digit <= 9) return '0' + digit;
                    if (digit >= 10 && digit <= 15) {
                        return uppercase ? ('A' + digit - 10) : ('a' + digit - 10);
                    }
                    return '\0';
                }
                
                /**
                 * @brief Comparaison case-insensitive
                 */
                NKENTSEU_CORE_API int32 NkCompareIgnoreCase(const Char* lhs, const Char* rhs, usize length) NK_NOEXCEPT;
                NKENTSEU_CORE_API Bool NkEqualsIgnoreCase(const Char* lhs, const Char* rhs, usize length) NK_NOEXCEPT;
                
                /**
                 * @brief Conversion chaînes
                 */
                NKENTSEU_CORE_API void NkToUpperInPlace(Char* str, usize length) NK_NOEXCEPT;
                NKENTSEU_CORE_API void NkToLowerInPlace(Char* str, usize length) NK_NOEXCEPT;
                
            } // namespace ascii
        } // namespace encoding
    } // namespace core
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_STRING_ENCODING_NKASCII_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-07
// Creation Date: 2026-02-07
// ============================================================