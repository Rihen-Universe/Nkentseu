// -----------------------------------------------------------------------------
// FICHIER: Core\NKContainers\src\NKContainers\String\NkStringUtils.cpp
// DESCRIPTION: Implémentation des utilitaires de manipulation de chaînes
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

// -------------------------------------------------------------------------
// Inclusion de l'en-tête correspondant
// -------------------------------------------------------------------------
#include "NkStringUtils.h"

// -------------------------------------------------------------------------
// Inclusions des modules d'encodage ASCII
// -------------------------------------------------------------------------
#include "Encoding/NkASCII.h"

// -------------------------------------------------------------------------
// Inclusions standard pour les opérations C-style
// -------------------------------------------------------------------------
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <ctime>

// -------------------------------------------------------------------------
// Namespace principal du projet
// -------------------------------------------------------------------------
namespace nkentseu {

    // -------------------------------------------------------------------------
    // Namespace string : utilitaires de chaînes
    // -------------------------------------------------------------------------
    namespace string {

        // =====================================================================
        // NAMESPACE ANONYME : UTILITAIRES INTERNES
        // =====================================================================
        namespace {

            // -------------------------------------------------------------------------
            // Générateur pseudo-aléatoire interne (XORShift64*)
            // -------------------------------------------------------------------------

            /// État interne du générateur aléatoire (initialisé à 0)
            static uint64 gNkStringUtilsRngState = 0u;

            /**
             * @brief Génère le prochain nombre aléatoire 64 bits
             * 
             * @return Valeur uint64 pseudo-aléatoire
             * 
             * @note Algorithme XORShift64* : rapide, bonne distribution statistique.
             *       Auto-ensemencé via time(nullptr) si état initial nul.
             *       Non-cryptographique : usage interne uniquement.
             */
            inline uint64 NkRandomNextU64() {
                if (gNkStringUtilsRngState == 0u) {
                    gNkStringUtilsRngState = static_cast<uint64>(std::time(nullptr));
                    if (gNkStringUtilsRngState == 0u) {
                        gNkStringUtilsRngState = 0x9E3779B97F4A7C15ULL;
                    }
                }
                uint64 x = gNkStringUtilsRngState;
                x ^= x >> 12u;
                x ^= x << 25u;
                x ^= x >> 27u;
                gNkStringUtilsRngState = x;
                return x * 0x2545F4914F6CDD1DULL;
            }

            /**
             * @brief Génère le prochain nombre aléatoire 32 bits
             * 
             * @return Valeur uint32 pseudo-aléatoire (partie haute du 64 bits)
             * 
             * @note Délègue à NkRandomNextU64() et extrait les 32 bits de poids fort.
             */
            inline uint32 NkRandomNextU32() {
                return static_cast<uint32>(NkRandomNextU64() >> 32u);
            }

        } // namespace anonyme

        // =====================================================================
        // SECTION : OPÉRATIONS DE BASE
        // =====================================================================

        usize NkLength(NkStringView str) {
            return str.Length();
        }

        bool NkEmpty(NkStringView str) {
            return str.Empty();
        }

        bool NkIsNotEmpty(NkStringView str) {
            return !str.Empty();
        }

        // =====================================================================
        // SECTION : CONVERSION DE CASSE (ASCII)
        // =====================================================================

        NkString NkToLower(NkStringView str) {
            NkString result(str);
            NkToLowerInPlace(result);
            return result;
        }

        void NkToLowerInPlace(NkString& str) {
            for (usize i = 0; i < str.Length(); ++i) {
                str[i] = encoding::ascii::NkToLower(str[i]);
            }
        }

        NkString NkToUpper(NkStringView str) {
            NkString result(str);
            NkToUpperInPlace(result);
            return result;
        }

        void NkToUpperInPlace(NkString& str) {
            for (usize i = 0; i < str.Length(); ++i) {
                str[i] = encoding::ascii::NkToUpper(str[i]);
            }
        }

        char NkToLower(char ch) {
            if (ch >= 'A' && ch <= 'Z') {
                return ch + ('a' - 'A');
            }
            return ch;
        }

        char NkToUpper(char ch) {
            if (ch >= 'a' && ch <= 'z') {
                return ch - ('a' - 'A');
            }
            return ch;
        }

        NkString NkToggleCase(NkStringView str) {
            NkString result(str);
            NkToggleCaseInPlace(result);
            return result;
        }

        void NkToggleCaseInPlace(NkString& str) {
            for (usize i = 0; i < str.Length(); ++i) {
                if (encoding::ascii::NkIsUpper(str[i])) {
                    str[i] = encoding::ascii::NkToLower(str[i]);
                } else if (encoding::ascii::NkIsLower(str[i])) {
                    str[i] = encoding::ascii::NkToUpper(str[i]);
                }
            }
        }

        NkString NkSwapCase(NkStringView str) {
            return NkToggleCase(str);
        }

        void NkSwapCaseInPlace(NkString& str) {
            NkToggleCaseInPlace(str);
        }

        // =====================================================================
        // SECTION : TRIM (SUPPRESSION D'ESPACES)
        // =====================================================================

        NkStringView NkTrimLeft(NkStringView str) {
            usize start = 0;
            while (start < str.Length() && encoding::ascii::NkIsWhitespace(str[start])) {
                ++start;
            }
            return str.SubStr(start);
        }

        NkStringView NkTrimRight(NkStringView str) {
            usize end = str.Length();
            while (end > 0 && encoding::ascii::NkIsWhitespace(str[end - 1])) {
                --end;
            }
            return str.SubStr(0, end);
        }

        NkStringView NkTrim(NkStringView str) {
            return NkTrimRight(NkTrimLeft(str));
        }

        NkString NkTrimLeftCopy(NkStringView str) {
            return NkString(NkTrimLeft(str));
        }

        NkString NkTrimRightCopy(NkStringView str) {
            return NkString(NkTrimRight(str));
        }

        NkString NkTrimCopy(NkStringView str) {
            return NkString(NkTrim(str));
        }

        void NkTrimLeftInPlace(NkString& str) {
            NkStringView trimmed = NkTrimLeft(str.View());
            if (trimmed.Length() < str.Length()) {
                str.Erase(0, str.Length() - trimmed.Length());
            }
        }

        void NkTrimRightInPlace(NkString& str) {
            NkStringView trimmed = NkTrimRight(str.View());
            if (trimmed.Length() < str.Length()) {
                str.Resize(trimmed.Length());
            }
        }

        void NkTrimInPlace(NkString& str) {
            NkTrimRightInPlace(str);
            NkTrimLeftInPlace(str);
        }

        NkStringView NkTrimLeftChars(NkStringView str, NkStringView chars) {
            usize start = 0;
            while (start < str.Length()) {
                bool found = false;
                for (usize i = 0; i < chars.Length(); ++i) {
                    if (str[start] == chars[i]) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    break;
                }
                ++start;
            }
            return str.SubStr(start);
        }

        NkStringView NkTrimRightChars(NkStringView str, NkStringView chars) {
            usize end = str.Length();
            while (end > 0) {
                bool found = false;
                for (usize i = 0; i < chars.Length(); ++i) {
                    if (str[end - 1] == chars[i]) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    break;
                }
                --end;
            }
            return str.SubStr(0, end);
        }

        NkStringView NkTrimChars(NkStringView str, NkStringView chars) {
            return NkTrimRightChars(NkTrimLeftChars(str, chars), chars);
        }

        // =====================================================================
        // SECTION : JOIN (CONCATÉNATION DE COLLECTIONS)
        // =====================================================================

        NkString NkJoin(
            const NkStringView* strings, 
            usize count, 
            NkStringView separator) {

            if (!strings || count == 0) {
                return NkString();
            }
            usize totalSize = 0;
            for (usize i = 0; i < count; ++i) {
                totalSize += strings[i].Length();
                if (i < count - 1) {
                    totalSize += separator.Length();
                }
            }
            NkString result;
            result.Reserve(totalSize);
            for (usize i = 0; i < count; ++i) {
                result.Append(strings[i]);
                if (i < count - 1) {
                    result.Append(separator);
                }
            }
            return result;
        }

        // =====================================================================
        // SECTION : REMPLACEMENT DE SOUS-CHAÎNES
        // =====================================================================

        NkString NkReplace(NkStringView str, NkStringView from, NkStringView to) {
            if (from.Empty()) {
                return NkString(str);
            }
            auto pos = str.Find(from);
            if (pos == NkStringView::npos) {
                return NkString(str);
            }
            NkString result;
            result.Reserve(str.Length());
            result.Append(str.SubStr(0, pos));
            result.Append(to);
            result.Append(str.SubStr(pos + from.Length()));
            return result;
        }

        NkString NkReplaceAll(NkStringView str, NkStringView from, NkStringView to) {
            if (from.Empty()) {
                return NkString(str);
            }
            NkString result;
            result.Reserve(str.Length());
            usize lastPos = 0;
            usize pos = 0;
            while ((pos = str.Find(from, lastPos)) != NkStringView::npos) {
                result.Append(str.SubStr(lastPos, pos - lastPos));
                result.Append(to);
                lastPos = pos + from.Length();
            }
            result.Append(str.SubStr(lastPos));
            return result;
        }

        void NkReplaceInPlace(NkString& str, NkStringView from, NkStringView to) {
            str = NkReplace(str.View(), from, to);
        }

        void NkReplaceAllInPlace(NkString& str, NkStringView from, NkStringView to) {
            str = NkReplaceAll(str.View(), from, to);
        }

        NkString NkReplaceFirst(NkStringView str, NkStringView from, NkStringView to) {
            return NkReplace(str, from, to);
        }

        NkString NkReplaceLast(NkStringView str, NkStringView from, NkStringView to) {
            if (from.Empty()) {
                return NkString(str);
            }
            auto pos = str.FindLast(from);
            if (pos == NkStringView::npos) {
                return NkString(str);
            }
            NkString result;
            result.Reserve(str.Length());
            result.Append(str.SubStr(0, pos));
            result.Append(to);
            result.Append(str.SubStr(pos + from.Length()));
            return result;
        }

        void NkReplaceFirstInPlace(NkString& str, NkStringView from, NkStringView to) {
            NkReplaceInPlace(str, from, to);
        }

        void NkReplaceLastInPlace(NkString& str, NkStringView from, NkStringView to) {
            str = NkReplaceLast(str.View(), from, to);
        }

        // =====================================================================
        // SECTION : CONVERSIONS NUMÉRIQUES - PARSING
        // =====================================================================

        bool NkParseInt(NkStringView str, int32& out) {
            if (str.Empty()) {
                return false;
            }
            char buffer[64];
            usize len = str.Length() < 63 ? str.Length() : 63;
            memory::NkMemCopy(buffer, str.Data(), len);
            buffer[len] = '\0';
            char* end = nullptr;
            long val = std::strtol(buffer, &end, 10);
            if (end == buffer || *end != '\0') {
                return false;
            }
            out = static_cast<int32>(val);
            return true;
        }

        bool NkParseInt64(NkStringView str, int64& out) {
            if (str.Empty()) {
                return false;
            }
            char buffer[64];
            usize len = str.Length() < 63 ? str.Length() : 63;
            memory::NkMemCopy(buffer, str.Data(), len);
            buffer[len] = '\0';
            char* end = nullptr;
            out = std::strtoll(buffer, &end, 10);
            return end != buffer && *end == '\0';
        }

        bool NkParseUInt(NkStringView str, uint32& out) {
            if (str.Empty()) {
                return false;
            }
            char buffer[64];
            usize len = str.Length() < 63 ? str.Length() : 63;
            memory::NkMemCopy(buffer, str.Data(), len);
            buffer[len] = '\0';
            char* end = nullptr;
            unsigned long val = std::strtoul(buffer, &end, 10);
            if (end == buffer || *end != '\0') {
                return false;
            }
            out = static_cast<uint32>(val);
            return true;
        }

        bool NkParseUInt64(NkStringView str, uint64& out) {
            if (str.Empty()) {
                return false;
            }
            char buffer[64];
            usize len = str.Length() < 63 ? str.Length() : 63;
            memory::NkMemCopy(buffer, str.Data(), len);
            buffer[len] = '\0';
            char* end = nullptr;
            out = std::strtoull(buffer, &end, 10);
            return end != buffer && *end == '\0';
        }

        bool NkParseFloat(NkStringView str, float32& out) {
            if (str.Empty()) {
                return false;
            }
            char buffer[128];
            usize len = str.Length() < 127 ? str.Length() : 127;
            memory::NkMemCopy(buffer, str.Data(), len);
            buffer[len] = '\0';
            char* end = nullptr;
            out = std::strtof(buffer, &end);
            return end != buffer && *end == '\0';
        }

        bool NkParseDouble(NkStringView str, float64& out) {
            if (str.Empty()) {
                return false;
            }
            char buffer[128];
            usize len = str.Length() < 127 ? str.Length() : 127;
            memory::NkMemCopy(buffer, str.Data(), len);
            buffer[len] = '\0';
            char* end = nullptr;
            out = std::strtod(buffer, &end);
            return end != buffer && *end == '\0';
        }

        bool NkParseBool(NkStringView str, bool& out) {
            NkStringView trimmed = NkTrim(str);
            if (trimmed == "true" || 
                trimmed == "True" || 
                trimmed == "TRUE" || 
                trimmed == "1") {
                out = true;
                return true;
            }
            if (trimmed == "false" || 
                trimmed == "False" || 
                trimmed == "FALSE" || 
                trimmed == "0") {
                out = false;
                return true;
            }
            return false;
        }

        bool NkParseHex(NkStringView str, uint32& out) {
            if (str.Empty()) {
                return false;
            }
            NkStringView trimmed = NkTrim(str);
            if (trimmed.StartsWith("0x") || trimmed.StartsWith("0X")) {
                trimmed = trimmed.SubStr(2);
            }
            char buffer[64];
            usize len = trimmed.Length() < 63 ? trimmed.Length() : 63;
            memory::NkMemCopy(buffer, trimmed.Data(), len);
            buffer[len] = '\0';
            char* end = nullptr;
            unsigned long val = std::strtoul(buffer, &end, 16);
            if (end == buffer || *end != '\0') {
                return false;
            }
            out = static_cast<uint32>(val);
            return true;
        }

        bool NkParseHex(NkStringView str, uint64& out) {
            if (str.Empty()) {
                return false;
            }
            NkStringView trimmed = NkTrim(str);
            if (trimmed.StartsWith("0x") || trimmed.StartsWith("0X")) {
                trimmed = trimmed.SubStr(2);
            }
            char buffer[64];
            usize len = trimmed.Length() < 63 ? trimmed.Length() : 63;
            memory::NkMemCopy(buffer, trimmed.Data(), len);
            buffer[len] = '\0';
            char* end = nullptr;
            out = std::strtoull(buffer, &end, 16);
            return end != buffer && *end == '\0';
        }

        // =====================================================================
        // SECTION : CONVERSIONS NUMÉRIQUES - TOSTRING
        // =====================================================================

        NkString NkToString(int32 value) {
            char buffer[32];
            int len = std::snprintf(buffer, sizeof(buffer), "%d", value);
            return NkString(buffer, static_cast<usize>(len));
        }

        NkString NkToString(int64 value) {
            char buffer[32];
            int len = std::snprintf(
                buffer, 
                sizeof(buffer), 
                "%lld", 
                static_cast<long long>(value));
            return NkString(buffer, static_cast<usize>(len));
        }

        NkString NkToString(uint32 value) {
            char buffer[32];
            int len = std::snprintf(buffer, sizeof(buffer), "%u", value);
            return NkString(buffer, static_cast<usize>(len));
        }

        NkString NkToString(uint64 value) {
            char buffer[32];
            int len = std::snprintf(
                buffer, 
                sizeof(buffer), 
                "%llu", 
                static_cast<unsigned long long>(value));
            return NkString(buffer, static_cast<usize>(len));
        }

        NkString NkToString(float32 value, int precision) {
            char buffer[64];
            char format[16];
            std::snprintf(format, sizeof(format), "%%.%df", precision);
            int len = std::snprintf(buffer, sizeof(buffer), format, value);
            return NkString(buffer, static_cast<usize>(len));
        }

        NkString NkToString(float64 value, int precision) {
            char buffer[64];
            char format[16];
            std::snprintf(format, sizeof(format), "%%.%dlf", precision);
            int len = std::snprintf(buffer, sizeof(buffer), format, value);
            return NkString(buffer, static_cast<usize>(len));
        }

        NkString NkToString(bool value) {
            return value ? NkString("true") : NkString("false");
        }

        NkString NkToHex(int32 value, bool prefix) {
            char buffer[32];
            int len = std::snprintf(
                buffer, 
                sizeof(buffer), 
                prefix ? "0x%08X" : "%08X", 
                value);
            return NkString(buffer, static_cast<usize>(len));
        }

        NkString NkToHex(int64 value, bool prefix) {
            char buffer[32];
            int len = std::snprintf(
                buffer, 
                sizeof(buffer), 
                prefix ? "0x%016llX" : "%016llX", 
                static_cast<unsigned long long>(value));
            return NkString(buffer, static_cast<usize>(len));
        }

        NkString NkToHex(uint32 value, bool prefix) {
            char buffer[32];
            int len = std::snprintf(
                buffer, 
                sizeof(buffer), 
                prefix ? "0x%08X" : "%08X", 
                value);
            return NkString(buffer, static_cast<usize>(len));
        }

        NkString NkToHex(uint64 value, bool prefix) {
            char buffer[32];
            int len = std::snprintf(
                buffer, 
                sizeof(buffer), 
                prefix ? "0x%016llX" : "%016llX", 
                static_cast<unsigned long long>(value));
            return NkString(buffer, static_cast<usize>(len));
        }

        NkString NkToStringf(const char* format, ...) {
            if (!format) {
                return NkString();
            }
            va_list args;
            va_start(args, format);
            NkString result = NkVFormatf(format, args);
            va_end(args);
            return result;
        }

        // =====================================================================
        // SECTION : COMPARAISONS (CASE-INSENSITIVE)
        // =====================================================================

        int NkCompareIgnoreCase(NkStringView lhs, NkStringView rhs) {
            usize minLen = lhs.Length() < rhs.Length() ? lhs.Length() : rhs.Length();
            for (usize i = 0; i < minLen; ++i) {
                char l = encoding::ascii::NkToLower(lhs[i]);
                char r = encoding::ascii::NkToLower(rhs[i]);
                if (l < r) {
                    return -1;
                }
                if (l > r) {
                    return 1;
                }
            }
            if (lhs.Length() < rhs.Length()) {
                return -1;
            }
            if (lhs.Length() > rhs.Length()) {
                return 1;
            }
            return 0;
        }

        bool NkEqualsIgnoreCase(NkStringView lhs, NkStringView rhs) {
            return NkCompareIgnoreCase(lhs, rhs) == 0;
        }

        // =====================================================================
        // SECTION : PRÉDICATS DE CARACTÈRES ET DE CHAÎNES
        // =====================================================================

        bool NkIsWhitespace(char ch) {
            return encoding::ascii::NkIsWhitespace(ch);
        }

        bool NkIsDigit(char ch) {
            return encoding::ascii::NkIsDigit(ch);
        }

        bool NkIsAlpha(char ch) {
            return encoding::ascii::NkIsAlpha(ch);
        }

        bool NkIsAlphaNumeric(char ch) {
            return encoding::ascii::NkIsAlphaNumeric(ch);
        }

        bool NkIsLower(char ch) {
            return encoding::ascii::NkIsLower(ch);
        }

        bool NkIsUpper(char ch) {
            return encoding::ascii::NkIsUpper(ch);
        }

        bool NkIsHexDigit(char ch) {
            return encoding::ascii::NkIsHexDigit(ch);
        }

        bool NkIsPrintable(char ch) {
            return encoding::ascii::NkIsPrintable(ch);
        }

        bool NkIsAllWhitespace(NkStringView str) {
            for (usize i = 0; i < str.Length(); ++i) {
                if (!NkIsWhitespace(str[i])) {
                    return false;
                }
            }
            return true;
        }

        bool NkIsAllDigits(NkStringView str) {
            if (str.Empty()) {
                return false;
            }
            for (usize i = 0; i < str.Length(); ++i) {
                if (!NkIsDigit(str[i])) {
                    return false;
                }
            }
            return true;
        }

        bool NkIsAllAlpha(NkStringView str) {
            if (str.Empty()) {
                return false;
            }
            for (usize i = 0; i < str.Length(); ++i) {
                if (!NkIsAlpha(str[i])) {
                    return false;
                }
            }
            return true;
        }

        bool NkIsAllAlphaNumeric(NkStringView str) {
            if (str.Empty()) {
                return false;
            }
            for (usize i = 0; i < str.Length(); ++i) {
                if (!NkIsAlphaNumeric(str[i])) {
                    return false;
                }
            }
            return true;
        }

        bool NkIsAllHexDigits(NkStringView str) {
            if (str.Empty()) {
                return false;
            }
            for (usize i = 0; i < str.Length(); ++i) {
                if (!NkIsHexDigit(str[i])) {
                    return false;
                }
            }
            return true;
        }

        bool NkIsAllPrintable(NkStringView str) {
            if (str.Empty()) {
                return false;
            }
            for (usize i = 0; i < str.Length(); ++i) {
                if (!NkIsPrintable(str[i])) {
                    return false;
                }
            }
            return true;
        }

        bool NkIsNumeric(NkStringView str) {
            if (str.Empty()) {
                return false;
            }
            NkStringView trimmed = NkTrim(str);
            if (trimmed.Empty()) {
                return false;
            }
            bool hasDecimal = false;
            bool hasSign = false;
            usize start = 0;
            if (trimmed[0] == '+' || trimmed[0] == '-') {
                hasSign = true;
                start = 1;
                if (trimmed.Length() <= 1) {
                    return false;
                }
            }
            for (usize i = start; i < trimmed.Length(); ++i) {
                if (trimmed[i] == '.') {
                    if (hasDecimal) {
                        return false;
                    }
                    hasDecimal = true;
                } else if (!encoding::ascii::NkIsDigit(trimmed[i])) {
                    return false;
                }
            }
            return true;
        }

        bool NkIsInteger(NkStringView str) {
            if (str.Empty()) {
                return false;
            }
            NkStringView trimmed = NkTrim(str);
            if (trimmed.Empty()) {
                return false;
            }
            usize start = 0;
            if (trimmed[0] == '+' || trimmed[0] == '-') {
                start = 1;
                if (trimmed.Length() <= 1) {
                    return false;
                }
            }
            for (usize i = start; i < trimmed.Length(); ++i) {
                if (!encoding::ascii::NkIsDigit(trimmed[i])) {
                    return false;
                }
            }
            return true;
        }

        bool NkIsPalindrome(NkStringView str, bool ignoreCase) {
            if (str.Empty()) {
                return true;
            }
            usize i = 0;
            usize j = str.Length() - 1;
            while (i < j) {
                char c1 = str[i];
                char c2 = str[j];
                if (ignoreCase) {
                    c1 = encoding::ascii::NkToLower(c1);
                    c2 = encoding::ascii::NkToLower(c2);
                }
                if (c1 != c2) {
                    return false;
                }
                ++i;
                --j;
            }
            return true;
        }

        bool NkIsEmail(NkStringView str) {
            NkStringView trimmed = NkTrim(str);
            if (trimmed.Empty()) {
                return false;
            }
            usize atPos = trimmed.Find('@');
            if (atPos == NkStringView::npos || 
                atPos == 0 || 
                atPos == trimmed.Length() - 1) {
                return false;
            }
            usize dotPos = trimmed.Find('.', atPos);
            if (dotPos == NkStringView::npos || 
                dotPos == trimmed.Length() - 1) {
                return false;
            }
            if (trimmed.Find(' ') != NkStringView::npos) {
                return false;
            }
            return true;
        }

        bool NkIsIdentifier(NkStringView str) {
            if (str.Empty()) {
                return false;
            }
            if (!encoding::ascii::NkIsAlpha(str[0]) && str[0] != '_') {
                return false;
            }
            for (usize i = 1; i < str.Length(); ++i) {
                if (!encoding::ascii::NkIsAlphaNumeric(str[i]) && str[i] != '_') {
                    return false;
                }
            }
            return true;
        }

        // =====================================================================
        // SECTION : PADDING (REMPLISSAGE)
        // =====================================================================

        NkString NkPadLeft(NkStringView str, usize totalWidth, char paddingChar) {
            if (str.Length() >= totalWidth) {
                return NkString(str);
            }
            NkString result;
            result.Reserve(totalWidth);
            usize padCount = totalWidth - str.Length();
            result.Append(padCount, paddingChar);
            result.Append(str);
            return result;
        }

        NkString NkPadRight(NkStringView str, usize totalWidth, char paddingChar) {
            if (str.Length() >= totalWidth) {
                return NkString(str);
            }
            NkString result(str);
            result.Reserve(totalWidth);
            usize padCount = totalWidth - str.Length();
            result.Append(padCount, paddingChar);
            return result;
        }

        NkString NkPadCenter(NkStringView str, usize totalWidth, char paddingChar) {
            if (str.Length() >= totalWidth) {
                return NkString(str);
            }
            usize padding = totalWidth - str.Length();
            usize leftPadding = padding / 2;
            usize rightPadding = padding - leftPadding;
            NkString result;
            result.Reserve(totalWidth);
            result.Append(leftPadding, paddingChar);
            result.Append(str);
            result.Append(rightPadding, paddingChar);
            return result;
        }

        // =====================================================================
        // SECTION : RÉPÉTITION DE CHAÎNES
        // =====================================================================

        NkString NkRepeat(NkStringView str, usize count) {
            if (count == 0) {
                return NkString();
            }
            NkString result;
            result.Reserve(str.Length() * count);
            for (usize i = 0; i < count; ++i) {
                result.Append(str);
            }
            return result;
        }

        NkString NkRepeat(char ch, usize count) {
            return NkString(count, ch);
        }

        // =====================================================================
        // SECTION : FORMATAGE PRINTF-STYLE
        // =====================================================================

        NkString NkFormatf(const char* format, ...) {
            if (!format) {
                return NkString();
            }
            va_list args;
            va_start(args, format);
            va_list argsCopy;
            va_copy(argsCopy, args);
            int size = std::vsnprintf(nullptr, 0, format, argsCopy);
            va_end(argsCopy);
            if (size < 0) {
                va_end(args);
                return NkString();
            }
            NkString result;
            result.Resize(static_cast<usize>(size));
            std::vsnprintf(result.Data(), static_cast<size_t>(size) + 1, format, args);
            va_end(args);
            return result;
        }

        NkString NkVFormatf(const char* format, va_list args) {
            if (!format) {
                return NkString();
            }
            va_list argsCopy;
            va_copy(argsCopy, args);
            int size = std::vsnprintf(nullptr, 0, format, argsCopy);
            va_end(argsCopy);
            if (size < 0) {
                return NkString();
            }
            NkString result;
            result.Resize(static_cast<usize>(size));
            std::vsnprintf(result.Data(), static_cast<size_t>(size) + 1, format, args);
            return result;
        }

        // =====================================================================
        // SECTION : RECHERCHE ET TESTS DE CONTENU
        // =====================================================================

        bool NkStartsWith(NkStringView str, NkStringView prefix) {
            if (prefix.Length() > str.Length()) {
                return false;
            }
            return memory::NkMemCompare(
                str.Data(), 
                prefix.Data(), 
                prefix.Length()) == 0;
        }

        bool NkStartsWithIgnoreCase(NkStringView str, NkStringView prefix) {
            if (prefix.Length() > str.Length()) {
                return false;
            }
            return NkCompareIgnoreCase(
                str.SubStr(0, prefix.Length()), 
                prefix) == 0;
        }

        bool NkEndsWith(NkStringView str, NkStringView suffix) {
            if (suffix.Length() > str.Length()) {
                return false;
            }
            return memory::NkMemCompare(
                str.Data() + str.Length() - suffix.Length(),
                suffix.Data(),
                suffix.Length()) == 0;
        }

        bool NkEndsWithIgnoreCase(NkStringView str, NkStringView suffix) {
            if (suffix.Length() > str.Length()) {
                return false;
            }
            return NkCompareIgnoreCase(
                str.SubStr(str.Length() - suffix.Length()),
                suffix) == 0;
        }

        bool NkContains(NkStringView str, NkStringView substring) {
            return str.Find(substring) != NkStringView::npos;
        }

        bool NkContainsIgnoreCase(NkStringView str, NkStringView substring) {
            if (substring.Empty()) {
                return true;
            }
            if (str.Length() < substring.Length()) {
                return false;
            }
            for (usize i = 0; i <= str.Length() - substring.Length(); ++i) {
                if (NkEqualsIgnoreCase(
                    str.SubStr(i, substring.Length()), 
                    substring)) {
                    return true;
                }
            }
            return false;
        }

        bool NkContainsAny(NkStringView str, NkStringView characters) {
            for (usize i = 0; i < str.Length(); ++i) {
                for (usize j = 0; j < characters.Length(); ++j) {
                    if (str[i] == characters[j]) {
                        return true;
                    }
                }
            }
            return false;
        }

        bool NkContainsNone(NkStringView str, NkStringView characters) {
            return !NkContainsAny(str, characters);
        }

        bool NkContainsOnly(NkStringView str, NkStringView characters) {
            if (str.Empty()) {
                return true;
            }
            for (usize i = 0; i < str.Length(); ++i) {
                bool found = false;
                for (usize j = 0; j < characters.Length(); ++j) {
                    if (str[i] == characters[j]) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return false;
                }
            }
            return true;
        }

        usize NkFindFirstOf(NkStringView str, NkStringView characters, usize start) {
            for (usize i = start; i < str.Length(); ++i) {
                for (usize j = 0; j < characters.Length(); ++j) {
                    if (str[i] == characters[j]) {
                        return i;
                    }
                }
            }
            return NkStringView::npos;
        }

        usize NkFindLastOf(NkStringView str, NkStringView characters) {
            for (usize i = str.Length(); i > 0; --i) {
                usize idx = i - 1;
                for (usize j = 0; j < characters.Length(); ++j) {
                    if (str[idx] == characters[j]) {
                        return idx;
                    }
                }
            }
            return NkStringView::npos;
        }

        usize NkFindFirstNotOf(NkStringView str, NkStringView characters, usize start) {
            for (usize i = start; i < str.Length(); ++i) {
                bool found = false;
                for (usize j = 0; j < characters.Length(); ++j) {
                    if (str[i] == characters[j]) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return i;
                }
            }
            return NkStringView::npos;
        }

        usize NkFindLastNotOf(NkStringView str, NkStringView characters) {
            for (usize i = str.Length(); i > 0; --i) {
                usize idx = i - 1;
                bool found = false;
                for (usize j = 0; j < characters.Length(); ++j) {
                    if (str[idx] == characters[j]) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return idx;
                }
            }
            return NkStringView::npos;
        }

        usize NkCount(NkStringView str, NkStringView substring) {
            if (substring.Empty() || str.Length() < substring.Length()) {
                return 0;
            }
            usize count = 0;
            usize pos = 0;
            while ((pos = str.Find(substring, pos)) != NkStringView::npos) {
                ++count;
                pos += substring.Length();
            }
            return count;
        }

        usize NkCount(NkStringView str, char character) {
            usize count = 0;
            for (usize i = 0; i < str.Length(); ++i) {
                if (str[i] == character) {
                    ++count;
                }
            }
            return count;
        }

        usize NkFindIgnoreCase(NkStringView str, NkStringView substring, usize start) {
            if (substring.Empty()) {
                return start < str.Length() ? start : NkStringView::npos;
            }
            if (str.Length() < substring.Length()) {
                return NkStringView::npos;
            }
            for (usize i = start; i <= str.Length() - substring.Length(); ++i) {
                if (NkEqualsIgnoreCase(
                    str.SubStr(i, substring.Length()), 
                    substring)) {
                    return i;
                }
            }
            return NkStringView::npos;
        }

        usize NkFindLastIgnoreCase(NkStringView str, NkStringView substring) {
            if (substring.Empty()) {
                return str.Length() > 0 ? str.Length() - 1 : NkStringView::npos;
            }
            if (str.Length() < substring.Length()) {
                return NkStringView::npos;
            }
            for (usize i = str.Length() - substring.Length(); 
                 i != static_cast<usize>(-1); 
                 --i) {
                if (NkEqualsIgnoreCase(
                    str.SubStr(i, substring.Length()), 
                    substring)) {
                    return i;
                }
            }
            return NkStringView::npos;
        }

        // =====================================================================
        // SECTION : EXTRACTION DE SOUS-CHAÎNES
        // =====================================================================

        NkStringView NkSubstringBetween(
            NkStringView str, 
            NkStringView startDelim, 
            NkStringView endDelim) {

            usize startPos = str.Find(startDelim);
            if (startPos == NkStringView::npos) {
                return NkStringView();
            }
            startPos += startDelim.Length();
            usize endPos = str.Find(endDelim, startPos);
            if (endPos == NkStringView::npos) {
                return NkStringView();
            }
            return str.SubStr(startPos, endPos - startPos);
        }

        NkString NkSubstringBetweenCopy(
            NkStringView str, 
            NkStringView startDelim, 
            NkStringView endDelim) {

            return NkString(NkSubstringBetween(str, startDelim, endDelim));
        }

        NkStringView NkSubstringBefore(NkStringView str, NkStringView delimiter) {
            usize pos = str.Find(delimiter);
            if (pos == NkStringView::npos) {
                return str;
            }
            return str.SubStr(0, pos);
        }

        NkStringView NkSubstringAfter(NkStringView str, NkStringView delimiter) {
            usize pos = str.Find(delimiter);
            if (pos == NkStringView::npos) {
                return NkStringView();
            }
            return str.SubStr(pos + delimiter.Length());
        }

        NkStringView NkSubstringBeforeLast(NkStringView str, NkStringView delimiter) {
            usize pos = str.FindLast(delimiter);
            if (pos == NkStringView::npos) {
                return str;
            }
            return str.SubStr(0, pos);
        }

        NkStringView NkSubstringAfterLast(NkStringView str, NkStringView delimiter) {
            usize pos = str.FindLast(delimiter);
            if (pos == NkStringView::npos) {
                return NkStringView();
            }
            return str.SubStr(pos + delimiter.Length());
        }

        char NkFirstChar(NkStringView str) {
            return str.Empty() ? '\0' : str[0];
        }

        char NkLastChar(NkStringView str) {
            return str.Empty() ? '\0' : str[str.Length() - 1];
        }

        NkStringView NkFirstChars(NkStringView str, usize count) {
            if (count >= str.Length()) {
                return str;
            }
            return str.SubStr(0, count);
        }

        NkStringView NkLastChars(NkStringView str, usize count) {
            if (count >= str.Length()) {
                return str;
            }
            return str.SubStr(str.Length() - count);
        }

        NkStringView NkMid(NkStringView str, usize start, usize count) {
            if (start >= str.Length()) {
                return NkStringView();
            }
            if (count == NkStringView::npos || start + count > str.Length()) {
                return str.SubStr(start);
            }
            return str.SubStr(start, count);
        }

        // =====================================================================
        // SECTION : TRANSFORMATIONS DE CHAÎNES
        // =====================================================================

        NkString NkCapitalize(NkStringView str) {
            if (str.Empty()) {
                return NkString();
            }
            NkString result(str);
            NkCapitalizeInPlace(result);
            return result;
        }

        void NkCapitalizeInPlace(NkString& str) {
            if (str.Empty()) {
                return;
            }
            str[0] = encoding::ascii::NkToUpper(str[0]);
            for (usize i = 1; i < str.Length(); ++i) {
                str[i] = encoding::ascii::NkToLower(str[i]);
            }
        }

        NkString NkTitleCase(NkStringView str) {
            if (str.Empty()) {
                return NkString();
            }
            NkString result(str);
            NkTitleCaseInPlace(result);
            return result;
        }

        void NkTitleCaseInPlace(NkString& str) {
            if (str.Empty()) {
                return;
            }
            bool newWord = true;
            for (usize i = 0; i < str.Length(); ++i) {
                if (encoding::ascii::NkIsWhitespace(str[i]) || str[i] == '-') {
                    newWord = true;
                } else if (newWord) {
                    str[i] = encoding::ascii::NkToUpper(str[i]);
                    newWord = false;
                } else {
                    str[i] = encoding::ascii::NkToLower(str[i]);
                }
            }
        }

        NkString NkReverse(NkStringView str) {
            NkString result;
            result.Reserve(str.Length());
            for (usize i = str.Length(); i > 0; --i) {
                result.Append(str[i - 1]);
            }
            return result;
        }

        void NkReverseInPlace(NkString& str) {
            usize len = str.Length();
            for (usize i = 0; i < len / 2; ++i) {
                char temp = str[i];
                str[i] = str[len - i - 1];
                str[len - i - 1] = temp;
            }
        }

        NkString NkRemoveChars(NkStringView str, NkStringView charsToRemove) {
            if (str.Empty() || charsToRemove.Empty()) {
                return NkString(str);
            }
            NkString result;
            result.Reserve(str.Length());
            for (usize i = 0; i < str.Length(); ++i) {
                bool shouldRemove = false;
                for (usize j = 0; j < charsToRemove.Length(); ++j) {
                    if (str[i] == charsToRemove[j]) {
                        shouldRemove = true;
                        break;
                    }
                }
                if (!shouldRemove) {
                    result.Append(str[i]);
                }
            }
            return result;
        }

        void NkRemoveCharsInPlace(NkString& str, NkStringView charsToRemove) {
            if (str.Empty() || charsToRemove.Empty()) {
                return;
            }
            usize writePos = 0;
            for (usize readPos = 0; readPos < str.Length(); ++readPos) {
                bool shouldRemove = false;
                for (usize j = 0; j < charsToRemove.Length(); ++j) {
                    if (str[readPos] == charsToRemove[j]) {
                        shouldRemove = true;
                        break;
                    }
                }
                if (!shouldRemove) {
                    str[writePos++] = str[readPos];
                }
            }
            if (writePos < str.Length()) {
                str.Resize(writePos);
            }
        }

        NkString NkRemoveDuplicates(NkStringView str, char duplicateChar) {
            if (str.Empty()) {
                return NkString();
            }
            NkString result;
            result.Reserve(str.Length());
            char lastChar = '\0';
            for (usize i = 0; i < str.Length(); ++i) {
                if (str[i] != duplicateChar || str[i] != lastChar) {
                    result.Append(str[i]);
                    lastChar = str[i];
                }
            }
            return result;
        }

        void NkRemoveDuplicatesInPlace(NkString& str, char duplicateChar) {
            if (str.Empty()) {
                return;
            }
            usize writePos = 1;
            for (usize readPos = 1; readPos < str.Length(); ++readPos) {
                if (str[readPos] != duplicateChar || str[readPos] != str[writePos - 1]) {
                    str[writePos++] = str[readPos];
                }
            }
            if (writePos < str.Length()) {
                str.Resize(writePos);
            }
        }

        NkString NkRemoveExtraSpaces(NkStringView str) {
            NkString result;
            result.Reserve(str.Length());
            bool lastWasSpace = false;
            for (usize i = 0; i < str.Length(); ++i) {
                if (encoding::ascii::NkIsWhitespace(str[i])) {
                    if (!lastWasSpace && !result.Empty()) {
                        result.Append(' ');
                        lastWasSpace = true;
                    }
                } else {
                    result.Append(str[i]);
                    lastWasSpace = false;
                }
            }
            return NkTrimCopy(result.View());
        }

        void NkRemoveExtraSpacesInPlace(NkString& str) {
            NkString temp = NkRemoveExtraSpaces(str.View());
            str = temp;
        }

        NkString NkInsert(NkStringView str, usize position, NkStringView insertStr) {
            if (position > str.Length()) {
                position = str.Length();
            }
            NkString result;
            result.Reserve(str.Length() + insertStr.Length());
            result.Append(str.SubStr(0, position));
            result.Append(insertStr);
            result.Append(str.SubStr(position));
            return result;
        }

        void NkInsertInPlace(NkString& str, usize position, NkStringView insertStr) {
            if (position > str.Length()) {
                position = str.Length();
            }
            NkString temp;
            temp.Reserve(str.Length() + insertStr.Length());
            temp.Append(str.SubStr(0, position));
            temp.Append(insertStr);
            temp.Append(str.SubStr(position));
            str = temp;
        }

        NkString NkErase(NkStringView str, usize position, usize count) {
            if (position >= str.Length()) {
                return NkString(str);
            }
            if (position + count > str.Length()) {
                count = str.Length() - position;
            }
            NkString result;
            result.Reserve(str.Length() - count);
            result.Append(str.SubStr(0, position));
            result.Append(str.SubStr(position + count));
            return result;
        }

        void NkEraseInPlace(NkString& str, usize position, usize count) {
            if (position >= str.Length()) {
                return;
            }
            if (position + count > str.Length()) {
                count = str.Length() - position;
            }
            str.Erase(position, count);
        }

        NkString NkReplaceChar(NkStringView str, char from, char to) {
            NkString result(str);
            NkReplaceCharInPlace(result, from, to);
            return result;
        }

        void NkReplaceCharInPlace(NkString& str, char from, char to) {
            for (usize i = 0; i < str.Length(); ++i) {
                if (str[i] == from) {
                    str[i] = to;
                    return;
                }
            }
        }

        NkString NkReplaceAllChars(NkStringView str, char from, char to) {
            NkString result(str);
            NkReplaceAllCharsInPlace(result, from, to);
            return result;
        }

        void NkReplaceAllCharsInPlace(NkString& str, char from, char to) {
            for (usize i = 0; i < str.Length(); ++i) {
                if (str[i] == from) {
                    str[i] = to;
                }
            }
        }

        NkString NkRemoveAt(NkStringView str, usize position) {
            if (position >= str.Length()) {
                return NkString(str);
            }
            NkString result;
            result.Reserve(str.Length() - 1);
            result.Append(str.SubStr(0, position));
            result.Append(str.SubStr(position + 1));
            return result;
        }

        void NkRemoveAtInPlace(NkString& str, usize position) {
            if (position >= str.Length()) {
                return;
            }
            str.Erase(position, 1);
        }

        NkString NkInsertChar(NkStringView str, usize position, char ch) {
            if (position > str.Length()) {
                position = str.Length();
            }
            NkString result;
            result.Reserve(str.Length() + 1);
            result.Append(str.SubStr(0, position));
            result.Append(ch);
            result.Append(str.SubStr(position));
            return result;
        }

        void NkInsertCharInPlace(NkString& str, usize position, char ch) {
            if (position > str.Length()) {
                position = str.Length();
            }
            str.Insert(position, 1, ch);
        }

        // =====================================================================
        // SECTION : COMPARAISON NATURELLE (VERSION/TRI)
        // =====================================================================

        int NkCompareNatural(NkStringView lhs, NkStringView rhs) {
            usize i = 0;
            usize j = 0;
            usize len1 = lhs.Length();
            usize len2 = rhs.Length();
            while (i < len1 && j < len2) {
                if (encoding::ascii::NkIsDigit(lhs[i]) && 
                    encoding::ascii::NkIsDigit(rhs[j])) {
                    usize numStart1 = i;
                    usize numStart2 = j;
                    while (i < len1 && encoding::ascii::NkIsDigit(lhs[i])) {
                        ++i;
                    }
                    while (j < len2 && encoding::ascii::NkIsDigit(rhs[j])) {
                        ++j;
                    }
                    NkStringView num1 = lhs.SubStr(numStart1, i - numStart1);
                    NkStringView num2 = rhs.SubStr(numStart2, j - numStart2);
                    while (numStart1 < i && lhs[numStart1] == '0') {
                        ++numStart1;
                    }
                    while (numStart2 < j && rhs[numStart2] == '0') {
                        ++numStart2;
                    }
                    usize lenNum1 = i - numStart1;
                    usize lenNum2 = j - numStart2;
                    if (lenNum1 != lenNum2) {
                        return lenNum1 < lenNum2 ? -1 : 1;
                    }
                    for (usize k = 0; k < lenNum1; ++k) {
                        if (lhs[numStart1 + k] != rhs[numStart2 + k]) {
                            return lhs[numStart1 + k] < rhs[numStart2 + k] ? -1 : 1;
                        }
                    }
                } else {
                    if (lhs[i] != rhs[j]) {
                        return lhs[i] < rhs[j] ? -1 : 1;
                    }
                    ++i;
                    ++j;
                }
            }
            if (i < len1) {
                return 1;
            }
            if (j < len2) {
                return -1;
            }
            return 0;
        }

        int NkCompareNaturalIgnoreCase(NkStringView lhs, NkStringView rhs) {
            usize i = 0;
            usize j = 0;
            usize len1 = lhs.Length();
            usize len2 = rhs.Length();
            while (i < len1 && j < len2) {
                char c1 = encoding::ascii::NkToLower(lhs[i]);
                char c2 = encoding::ascii::NkToLower(rhs[j]);
                if (encoding::ascii::NkIsDigit(lhs[i]) && 
                    encoding::ascii::NkIsDigit(rhs[j])) {
                    usize numStart1 = i;
                    usize numStart2 = j;
                    while (i < len1 && encoding::ascii::NkIsDigit(lhs[i])) {
                        ++i;
                    }
                    while (j < len2 && encoding::ascii::NkIsDigit(rhs[j])) {
                        ++j;
                    }
                    NkStringView num1 = lhs.SubStr(numStart1, i - numStart1);
                    NkStringView num2 = rhs.SubStr(numStart2, j - numStart2);
                    while (numStart1 < i && lhs[numStart1] == '0') {
                        ++numStart1;
                    }
                    while (numStart2 < j && rhs[numStart2] == '0') {
                        ++numStart2;
                    }
                    usize lenNum1 = i - numStart1;
                    usize lenNum2 = j - numStart2;
                    if (lenNum1 != lenNum2) {
                        return lenNum1 < lenNum2 ? -1 : 1;
                    }
                    for (usize k = 0; k < lenNum1; ++k) {
                        if (lhs[numStart1 + k] != rhs[numStart2 + k]) {
                            return lhs[numStart1 + k] < rhs[numStart2 + k] ? -1 : 1;
                        }
                    }
                } else {
                    if (c1 != c2) {
                        return c1 < c2 ? -1 : 1;
                    }
                    ++i;
                    ++j;
                }
            }
            if (i < len1) {
                return 1;
            }
            if (j < len2) {
                return -1;
            }
            return 0;
        }

        // =====================================================================
        // SECTION : ÉCHAPPEMENT / DÉSÉCHAPPEMENT
        // =====================================================================

        NkString NkEscape(
            NkStringView str, 
            NkStringView charsToEscape, 
            char escapeChar) {

            NkString result;
            result.Reserve(str.Length() * 2);
            for (usize i = 0; i < str.Length(); ++i) {
                bool shouldEscape = false;
                for (usize j = 0; j < charsToEscape.Length(); ++j) {
                    if (str[i] == charsToEscape[j]) {
                        shouldEscape = true;
                        break;
                    }
                }
                if (shouldEscape) {
                    result.Append(escapeChar);
                }
                result.Append(str[i]);
            }
            return result;
        }

        NkString NkUnescape(NkStringView str, char escapeChar) {
            NkString result;
            result.Reserve(str.Length());
            for (usize i = 0; i < str.Length(); ++i) {
                if (str[i] == escapeChar && i + 1 < str.Length()) {
                    result.Append(str[++i]);
                } else {
                    result.Append(str[i]);
                }
            }
            return result;
        }

        NkString NkCEscape(NkStringView str) {
            NkString result;
            result.Reserve(str.Length() * 2);
            for (usize i = 0; i < str.Length(); ++i) {
                switch (str[i]) {
                    case '\"':
                        result.Append("\\\"");
                        break;
                    case '\'':
                        result.Append("\\'");
                        break;
                    case '\\':
                        result.Append("\\\\");
                        break;
                    case '\n':
                        result.Append("\\n");
                        break;
                    case '\r':
                        result.Append("\\r");
                        break;
                    case '\t':
                        result.Append("\\t");
                        break;
                    case '\b':
                        result.Append("\\b");
                        break;
                    case '\f':
                        result.Append("\\f");
                        break;
                    default:
                        if (str[i] >= 32 && str[i] <= 126) {
                            result.Append(str[i]);
                        } else {
                            char buffer[5];
                            std::snprintf(
                                buffer, 
                                sizeof(buffer), 
                                "\\%03o", 
                                static_cast<unsigned char>(str[i]));
                            result.Append(buffer);
                        }
                        break;
                }
            }
            return result;
        }

        NkString NkCUnescape(NkStringView str) {
            NkString result;
            result.Reserve(str.Length());
            for (usize i = 0; i < str.Length(); ++i) {
                if (str[i] == '\\' && i + 1 < str.Length()) {
                    ++i;
                    switch (str[i]) {
                        case 'n':
                            result.Append('\n');
                            break;
                        case 'r':
                            result.Append('\r');
                            break;
                        case 't':
                            result.Append('\t');
                            break;
                        case 'b':
                            result.Append('\b');
                            break;
                        case 'f':
                            result.Append('\f');
                            break;
                        case '\"':
                            result.Append('\"');
                            break;
                        case '\'':
                            result.Append('\'');
                            break;
                        case '\\':
                            result.Append('\\');
                            break;
                        case '0': case '1': case '2': case '3': 
                        case '4': case '5': case '6': case '7': {
                            char oct[4] = { str[i], '\0', '\0', '\0' };
                            if (i + 1 < str.Length() && 
                                str[i + 1] >= '0' && 
                                str[i + 1] <= '7') {
                                oct[1] = str[++i];
                                if (i + 1 < str.Length() && 
                                    str[i + 1] >= '0' && 
                                    str[i + 1] <= '7') {
                                    oct[2] = str[++i];
                                }
                            }
                            result.Append(
                                static_cast<char>(std::strtoul(oct, nullptr, 8)));
                            break;
                        }
                        case 'x':
                            if (i + 2 < str.Length()) {
                                char hex[3] = { str[i + 1], str[i + 2], '\0' };
                                i += 2;
                                result.Append(
                                    static_cast<char>(std::strtoul(hex, nullptr, 16)));
                            }
                            break;
                        default:
                            result.Append(str[i]);
                            break;
                    }
                } else {
                    result.Append(str[i]);
                }
            }
            return result;
        }

        NkString NkHTMLEscape(NkStringView str) {
            NkString result;
            result.Reserve(str.Length() * 6);
            for (usize i = 0; i < str.Length(); ++i) {
                switch (str[i]) {
                    case '&':
                        result.Append("&amp;");
                        break;
                    case '<':
                        result.Append("&lt;");
                        break;
                    case '>':
                        result.Append("&gt;");
                        break;
                    case '"':
                        result.Append("&quot;");
                        break;
                    case '\'':
                        result.Append("&#39;");
                        break;
                    default:
                        result.Append(str[i]);
                        break;
                }
            }
            return result;
        }

        NkString NkHTMLUnescape(NkStringView str) {
            NkString result;
            result.Reserve(str.Length());
            for (usize i = 0; i < str.Length(); ++i) {
                if (str[i] == '&') {
                    if (str.SubStr(i, 4) == "&lt;") {
                        result.Append('<');
                        i += 3;
                    } else if (str.SubStr(i, 4) == "&gt;") {
                        result.Append('>');
                        i += 3;
                    } else if (str.SubStr(i, 5) == "&amp;") {
                        result.Append('&');
                        i += 4;
                    } else if (str.SubStr(i, 6) == "&quot;") {
                        result.Append('"');
                        i += 5;
                    } else if (str.SubStr(i, 5) == "&#39;") {
                        result.Append('\'');
                        i += 4;
                    } else {
                        result.Append(str[i]);
                    }
                } else {
                    result.Append(str[i]);
                }
            }
            return result;
        }

        NkString NkURLEncode(NkStringView str) {
            static const char hex[] = "0123456789ABCDEF";
            NkString result;
            result.Reserve(str.Length() * 3);
            for (usize i = 0; i < str.Length(); ++i) {
                char c = str[i];
                if (encoding::ascii::NkIsAlphaNumeric(c) || 
                    c == '-' || 
                    c == '_' || 
                    c == '.' || 
                    c == '~') {
                    result.Append(c);
                } else {
                    result.Append('%');
                    result.Append(hex[(c >> 4) & 0x0F]);
                    result.Append(hex[c & 0x0F]);
                }
            }
            return result;
        }

        NkString NkURLDecode(NkStringView str) {
            NkString result;
            result.Reserve(str.Length());
            for (usize i = 0; i < str.Length(); ++i) {
                if (str[i] == '%' && i + 2 < str.Length()) {
                    char hex[3] = { str[i + 1], str[i + 2], '\0' };
                    result.Append(
                        static_cast<char>(std::strtoul(hex, nullptr, 16)));
                    i += 2;
                } else if (str[i] == '+') {
                    result.Append(' ');
                } else {
                    result.Append(str[i]);
                }
            }
            return result;
        }

        // =====================================================================
        // SECTION : HASHING DE CHAÎNES
        // =====================================================================

        uint64 NkHashFNV1a(NkStringView str) {
            const uint64 offsetBasis = 14695981039346656037ULL;
            const uint64 prime = 1099511628211ULL;
            uint64 hash = offsetBasis;
            for (usize i = 0; i < str.Length(); ++i) {
                hash ^= static_cast<uint64>(static_cast<unsigned char>(str[i]));
                hash *= prime;
            }
            return hash;
        }

        uint64 NkHashFNV1aIgnoreCase(NkStringView str) {
            const uint64 offsetBasis = 14695981039346656037ULL;
            const uint64 prime = 1099511628211ULL;
            uint64 hash = offsetBasis;
            for (usize i = 0; i < str.Length(); ++i) {
                char ch = encoding::ascii::NkToLower(str[i]);
                hash ^= static_cast<uint64>(static_cast<unsigned char>(ch));
                hash *= prime;
            }
            return hash;
        }

        uint64 NkHashDJB2(NkStringView str) {
            uint64 hash = 5381;
            for (usize i = 0; i < str.Length(); ++i) {
                hash = ((hash << 5) + hash) + static_cast<unsigned char>(str[i]);
            }
            return hash;
        }

        uint64 NkHashSDBM(NkStringView str) {
            uint64 hash = 0;
            for (usize i = 0; i < str.Length(); ++i) {
                hash = static_cast<unsigned char>(str[i]) + 
                       (hash << 6) + 
                       (hash << 16) - 
                       hash;
            }
            return hash;
        }

        // =====================================================================
        // SECTION : UTILITAIRES D'ENCODAGE
        // =====================================================================

        NkString NkToAscii(NkStringView str, char replacement) {
            NkString result;
            result.Reserve(str.Length());
            for (usize i = 0; i < str.Length(); ++i) {
                if (static_cast<unsigned char>(str[i]) <= 127) {
                    result.Append(str[i]);
                } else {
                    result.Append(replacement);
                }
            }
            return result;
        }

        bool NkIsValidAscii(NkStringView str) {
            for (usize i = 0; i < str.Length(); ++i) {
                if (static_cast<unsigned char>(str[i]) > 127) {
                    return false;
                }
            }
            return true;
        }

        // =====================================================================
        // SECTION : UTILITAIRES DE CHEMINS DE FICHIERS
        // =====================================================================

        NkStringView NkGetFileName(NkStringView path) {
            usize lastSlash = path.FindLast('/');
            usize lastBackslash = path.FindLast('\\');
            usize lastSeparator = (lastSlash == NkStringView::npos) 
                ? lastBackslash 
                : (lastBackslash == NkStringView::npos) 
                    ? lastSlash 
                    : (lastSlash > lastBackslash) ? lastSlash : lastBackslash;
            if (lastSeparator == NkStringView::npos) {
                return path;
            }
            return path.SubStr(lastSeparator + 1);
        }

        NkStringView NkGetFileNameWithoutExtension(NkStringView path) {
            NkStringView fileName = NkGetFileName(path);
            usize lastDot = fileName.FindLast('.');
            if (lastDot == NkStringView::npos) {
                return fileName;
            }
            return fileName.SubStr(0, lastDot);
        }

        NkStringView NkGetDirectory(NkStringView path) {
            usize lastSlash = path.FindLast('/');
            usize lastBackslash = path.FindLast('\\');
            usize lastSeparator = (lastSlash == NkStringView::npos) 
                ? lastBackslash 
                : (lastBackslash == NkStringView::npos) 
                    ? lastSlash 
                    : (lastSlash > lastBackslash) ? lastSlash : lastBackslash;
            if (lastSeparator == NkStringView::npos) {
                return NkStringView();
            }
            return path.SubStr(0, lastSeparator + 1);
        }

        NkStringView NkGetExtension(NkStringView path) {
            NkStringView fileName = NkGetFileName(path);
            usize lastDot = fileName.FindLast('.');
            if (lastDot == NkStringView::npos) {
                return NkStringView();
            }
            return fileName.SubStr(lastDot);
        }

        NkString NkChangeExtension(NkStringView path, NkStringView newExtension) {
            if (newExtension.Empty() || newExtension[0] != '.') {
                return NkString(path);
            }
            NkStringView dir = NkGetDirectory(path);
            NkStringView nameWithoutExt = NkGetFileNameWithoutExtension(path);
            NkString result;
            result.Reserve(
                dir.Length() + nameWithoutExt.Length() + newExtension.Length());
            result.Append(dir);
            result.Append(nameWithoutExt);
            result.Append(newExtension);
            return result;
        }

        NkString NkCombinePaths(NkStringView path1, NkStringView path2) {
            if (path1.Empty()) {
                return NkString(path2);
            }
            if (path2.Empty()) {
                return NkString(path1);
            }
            NkString result(path1);
            char lastChar = path1[path1.Length() - 1];
            if (lastChar != '/' && lastChar != '\\') {
                result.Append('/');
            }
            usize start = 0;
            while (start < path2.Length() && 
                   (path2[start] == '/' || path2[start] == '\\')) {
                ++start;
            }
            if (start < path2.Length()) {
                result.Append(path2.SubStr(start));
            }
            return result;
        }

        NkString NkNormalizePath(NkStringView path, char separator) {
            NkString result(path);
            for (usize i = 0; i < result.Length(); ++i) {
                if (result[i] == '/' || result[i] == '\\') {
                    result[i] = separator;
                }
            }
            return result;
        }

        bool NkIsAbsolutePath(NkStringView path) {
            if (path.Empty()) {
                return false;
            }
            if (path.Length() >= 2) {
                if ((path[0] >= 'A' && path[0] <= 'Z') || 
                    (path[0] >= 'a' && path[0] <= 'z')) {
                    if (path[1] == ':') {
                        return true;
                    }
                }
                if (path[0] == '\\' && path[1] == '\\') {
                    return true;
                }
            }
            if (path[0] == '/') {
                return true;
            }
            return false;
        }

        // =====================================================================
        // SECTION : TRAITEMENT DE LIGNES ET COMMENTAIRES
        // =====================================================================

        NkString NkRemoveComments(NkStringView str, NkStringView commentMarkers) {
            NkString result;
            result.Reserve(str.Length());
            for (usize i = 0; i < str.Length(); ++i) {
                bool isCommentMarker = false;
                for (usize j = 0; j < commentMarkers.Length(); ++j) {
                    if (str[i] == commentMarkers[j] && 
                        i + 1 < str.Length() && 
                        str[i] == str[i + 1]) {
                        isCommentMarker = true;
                        break;
                    }
                }
                if (isCommentMarker) {
                    while (i < str.Length() && str[i] != '\n') {
                        ++i;
                    }
                    if (i < str.Length()) {
                        result.Append(str[i]);
                    }
                } else {
                    result.Append(str[i]);
                }
            }
            return result;
        }

        NkString NkNormalizeLineEndings(NkStringView str, NkStringView newline) {
            NkString result;
            result.Reserve(str.Length());
            for (usize i = 0; i < str.Length(); ++i) {
                if (str[i] == '\r') {
                    if (i + 1 < str.Length() && str[i + 1] == '\n') {
                        ++i;
                    }
                    result.Append(newline);
                } else if (str[i] == '\n') {
                    result.Append(newline);
                } else {
                    result.Append(str[i]);
                }
            }
            return result;
        }

        usize NkCountLines(NkStringView str) {
            usize count = 0;
            for (usize i = 0; i < str.Length(); ++i) {
                if (str[i] == '\n') {
                    ++count;
                }
            }
            if (!str.Empty() && str[str.Length() - 1] != '\n') {
                ++count;
            }
            return count;
        }

        NkStringView NkGetLine(NkStringView str, usize lineNumber) {
            usize currentLine = 0;
            usize lineStart = 0;
            for (usize i = 0; i < str.Length(); ++i) {
                if (str[i] == '\n') {
                    if (currentLine == lineNumber) {
                        return str.SubStr(lineStart, i - lineStart);
                    }
                    lineStart = i + 1;
                    ++currentLine;
                }
            }
            if (currentLine == lineNumber) {
                return str.SubStr(lineStart);
            }
            return NkStringView();
        }

        // =====================================================================
        // SECTION : MANIPULATIONS DIVERSES DE CHAÎNES
        // =====================================================================

        void NkFill(NkString& str, char ch, usize count) {
            str.Clear();
            str.Append(count, ch);
        }

        NkString NkFillCopy(char ch, usize count) {
            NkString result;
            result.Append(count, ch);
            return result;
        }

        NkString NkClean(NkStringView str) {
            NkString result = NkRemoveExtraSpaces(str);
            NkTrimInPlace(result);
            return result;
        }

        void NkCleanInPlace(NkString& str) {
            NkRemoveExtraSpacesInPlace(str);
            NkTrimInPlace(str);
        }

        NkString NkFillMissing(
            NkStringView str, 
            char fillChar, 
            usize totalLength, 
            bool left) {

            if (str.Length() >= totalLength) {
                return NkString(str);
            }
            NkString result;
            result.Reserve(totalLength);
            usize fillCount = totalLength - str.Length();
            if (left) {
                result.Append(fillCount, fillChar);
                result.Append(str);
            } else {
                result.Append(str);
                result.Append(fillCount, fillChar);
            }
            return result;
        }

        // =====================================================================
        // SECTION : VALIDATION DE FORMATS SPÉCIFIQUES
        // =====================================================================

        bool NkMatchesPattern(NkStringView str, NkStringView pattern) {
            usize strIdx = 0;
            usize patIdx = 0;
            usize strLen = str.Length();
            usize patLen = pattern.Length();
            usize starIdx = NkStringView::npos;
            usize strStarIdx = NkStringView::npos;
            while (strIdx < strLen) {
                if (patIdx < patLen && 
                    (pattern[patIdx] == '?' || str[strIdx] == pattern[patIdx])) {
                    ++strIdx;
                    ++patIdx;
                } else if (patIdx < patLen && pattern[patIdx] == '*') {
                    starIdx = patIdx;
                    strStarIdx = strIdx;
                    ++patIdx;
                } else if (starIdx != NkStringView::npos) {
                    patIdx = starIdx + 1;
                    strIdx = ++strStarIdx;
                } else {
                    return false;
                }
            }
            while (patIdx < patLen && pattern[patIdx] == '*') {
                ++patIdx;
            }
            return patIdx == patLen;
        }

        bool NkIsURL(NkStringView str) {
            NkStringView trimmed = NkTrim(str);
            if (trimmed.Empty()) {
                return false;
            }
            return NkStartsWithIgnoreCase(trimmed, "http://") ||
                   NkStartsWithIgnoreCase(trimmed, "https://") ||
                   NkStartsWithIgnoreCase(trimmed, "ftp://") ||
                   NkStartsWithIgnoreCase(trimmed, "file://");
        }

        // =====================================================================
        // SECTION : UTILITAIRES AVANCÉS
        // =====================================================================

        NkString NkRandomString(usize length, NkStringView charset) {
            if (length == 0 || charset.Empty()) {
                return NkString();
            }
            NkString result;
            result.Reserve(length);
            for (usize i = 0; i < length; ++i) {
                const usize idx = static_cast<usize>(
                    NkRandomNextU64() % charset.Length());
                result.Append(charset[idx]);
            }
            return result;
        }

        NkString NkGenerateUUID() {
            uint32 part1 = NkRandomNextU32();
            uint32 part2 = NkRandomNextU32();
            uint32 part3 = NkRandomNextU32();
            uint32 part4 = NkRandomNextU32();
            char buffer[37];
            std::snprintf(
                buffer, 
                sizeof(buffer),
                "%08X-%04X-%04X-%04X-%08X%04X",
                part1,
                (part2 >> 16) & 0xFFFF,
                part2 & 0xFFFF,
                (part3 >> 16) & 0xFFFF,
                part3 & 0xFFFF, 
                part4);
            return NkString(buffer);
        }

        NkString NkObfuscate(NkStringView str, uint32 seed) {
            NkString result(str);
            for (usize i = 0; i < result.Length(); ++i) {
                result[i] ^= static_cast<char>((seed + i) % 256);
            }
            return result;
        }

        NkString NkDeobfuscate(NkStringView str, uint32 seed) {
            return NkObfuscate(str, seed);
        }

        usize NkLevenshteinDistance(NkStringView str1, NkStringView str2) {
            usize len1 = str1.Length();
            usize len2 = str2.Length();
            if (len1 == 0) {
                return len2;
            }
            if (len2 == 0) {
                return len1;
            }
            usize* distances = new usize[(len1 + 1) * (len2 + 1)];
            for (usize i = 0; i <= len1; ++i) {
                distances[i * (len2 + 1)] = i;
            }
            for (usize j = 0; j <= len2; ++j) {
                distances[j] = j;
            }
            for (usize i = 1; i <= len1; ++i) {
                for (usize j = 1; j <= len2; ++j) {
                    usize cost = (str1[i - 1] == str2[j - 1]) ? 0 : 1;
                    usize deletion = distances[(i - 1) * (len2 + 1) + j] + 1;
                    usize insertion = distances[i * (len2 + 1) + (j - 1)] + 1;
                    usize substitution = distances[(i - 1) * (len2 + 1) + (j - 1)] + cost;
                    distances[i * (len2 + 1) + j] = (deletion < insertion) 
                        ? (deletion < substitution ? deletion : substitution)
                        : (insertion < substitution ? insertion : substitution);
                }
            }
            usize result = distances[len1 * (len2 + 1) + len2];
            delete[] distances;
            return result;
        }

        float64 NkSimilarity(NkStringView str1, NkStringView str2) {
            usize distance = NkLevenshteinDistance(str1, str2);
            usize maxLen = str1.Length() > str2.Length() 
                ? str1.Length() 
                : str2.Length();
            if (maxLen == 0) {
                return 1.0;
            }
            return 1.0 - static_cast<float64>(distance) / static_cast<float64>(maxLen);
        }

        // =====================================================================
        // SECTION : CRÉATION ET MANIPULATION DE STRINGVIEW
        // =====================================================================

        NkStringView NkMakeView(const char* data, usize length) {
            return NkStringView(data, length);
        }

        NkStringView NkMakeView(const char* cstr) {
            return NkStringView(cstr);
        }

        // =====================================================================
        // SECTION : OPÉRATIONS MÉMOIRE SÉCURISÉES
        // =====================================================================

        usize NkSafeCopy(char* dest, usize destSize, NkStringView src) {
            if (!dest || destSize == 0) {
                return 0;
            }
            usize copySize = src.Length() < destSize 
                ? src.Length() 
                : destSize - 1;
            memory::NkMemCopy(dest, src.Data(), copySize);
            dest[copySize] = '\0';
            return copySize;
        }

        usize NkSafeConcat(char* dest, usize destSize, NkStringView src) {
            if (!dest || destSize == 0) {
                return 0;
            }
            usize destLen = 0;
            while (destLen < destSize && dest[destLen] != '\0') {
                ++destLen;
            }
            if (destLen >= destSize) {
                return 0;
            }
            usize copySize = src.Length() < (destSize - destLen - 1) 
                ? src.Length() 
                : (destSize - destLen - 1);
            memory::NkMemCopy(dest + destLen, src.Data(), copySize);
            dest[destLen + copySize] = '\0';
            return copySize;
        }

    } // namespace string

} // namespace nkentseu

// -----------------------------------------------------------------------------
// EXEMPLES D'UTILISATION
// -----------------------------------------------------------------------------
/*
    // =====================================================================
    // Exemple 1 : Parsing de configuration avec split et trim
    // =====================================================================
    {
        bool ParseConfigLine(nkentseu::NkStringView line, ConfigEntry& out) {
            auto cleaned = nkentseu::string::NkTrim(line);
            if (cleaned.Empty() || cleaned.StartsWith('#')) {
                return false;
            }
            auto eqPos = cleaned.Find('=');
            if (eqPos == nkentseu::NkStringView::npos) {
                return false;
            }
            out.key = nkentseu::string::NkTrimCopy(cleaned.Left(eqPos));
            out.value = nkentseu::string::NkTrimCopy(cleaned.SubStr(eqPos + 1));
            return !out.key.Empty() && !out.value.Empty();
        }
    }

    // =====================================================================
    // Exemple 2 : Validation d'entrée utilisateur avec prédicats
    // =====================================================================
    {
        bool IsValidUsername(nkentseu::NkStringView username) {
            return username.Length() >= 3 && 
                   username.Length() <= 20 && 
                   nkentseu::string::NkIsAllAlphaNumeric(username);
        }
        
        bool IsValidPort(nkentseu::NkStringView portStr) {
            uint32 port = 0;
            return nkentseu::string::NkParseUInt(portStr, port) && 
                   port >= 1 && port <= 65535;
        }
    }

    // =====================================================================
    // Exemple 3 : Manipulation de chemins de fichiers
    // =====================================================================
    {
        nkentseu::NkString BuildOutputPath(
            nkentseu::NkStringView sourceFile,
            nkentseu::NkStringView outputDir,
            nkentseu::NkStringView newExtension) {
            
            auto baseName = nkentseu::string::NkGetFileNameWithoutExtension(sourceFile);
            auto path = nkentseu::string::NkCombinePaths(outputDir, baseName);
            return nkentseu::string::NkChangeExtension(path, newExtension);
        }
    }

    // =====================================================================
    // Exemple 4 : Formatage de logs avec hash pour déduplication
    // =====================================================================
    {
        uint64 ComputeLogHash(
            const char* level, 
            uint64 timestamp, 
            nkentseu::NkStringView message) {
            
            auto combined = nkentseu::string::NkFormat(
                "{0}|{1:hex16}|{2}", 
                level, timestamp, message);
            return nkentseu::string::NkHashFNV1a(combined.View());
        }
    }

    // =====================================================================
    // Exemple 5 : Échappement sécurisé pour insertion HTML
    // =====================================================================
    {
        nkentseu::NkString SafeHtmlInsert(nkentseu::NkStringView userInput) {
            return nkentseu::string::NkHTMLEscape(userInput);
        }
        // Usage : SafeHtmlInsert("<script>alert('x')</script>")
        // -> "&lt;script&gt;alert(&#39;x&#39;)&lt;/script&gt;"
    }

    // =====================================================================
    // Exemple 6 : Génération de tokens aléatoires
    // =====================================================================
    {
        nkentseu::NkString GenerateApiToken(usize length = 32) {
            return nkentseu::string::NkRandomString(
                length, 
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
        }
    }

    // =====================================================================
    // Exemple 7 : Comparaison naturelle pour tri de fichiers
    // =====================================================================
    {
        bool NaturalLess(
            nkentseu::NkStringView a, 
            nkentseu::NkStringView b) {
            return nkentseu::string::NkCompareNatural(a, b) < 0;
        }
        // Usage dans std::sort :
        // std::sort(files.begin(), files.end(), NaturalLess);
        // Résultat : ["file1.txt", "file2.txt", "file10.txt"] (ordre naturel)
    }

    // =====================================================================
    // Exemple 8 : Nettoyage et normalisation de texte utilisateur
    // =====================================================================
    {
        nkentseu::NkString NormalizeInput(nkentseu::NkStringView raw) {
            return nkentseu::string::NkClean(raw)
                                .View()
                                .ToLower()
                                .ToString();
        }
        // Usage : NormalizeInput("  Hello   WORLD  ") -> "hello world"
    }

    // =====================================================================
    // Exemple 9 : Opérations mémoire sécurisées pour interop C
    // =====================================================================
    {
        void SafeStringOperation(char* buffer, usize bufferSize) {
            nkentseu::NkStringView source = "Données externes";
            
            // Copie sécurisée : jamais de dépassement de buffer
            usize copied = nkentseu::string::NkSafeCopy(
                buffer, 
                bufferSize, 
                source);
            
            // Concaténation sécurisée
            nkentseu::string::NkSafeConcat(
                buffer, 
                bufferSize, 
                " [validé]");
        }
    }

    // =====================================================================
    // Exemple 10 : Bonnes pratiques et pièges à éviter
    // =====================================================================
    {
        // ✅ BON : Utiliser les vues pour éviter les copies inutiles
        void ProcessText(nkentseu::NkStringView input) {
            if (nkentseu::string::NkStartsWith(input, "CMD:")) {
                auto command = nkentseu::string::NkTrim(input.SubStr(4));
                ExecuteCommand(command);
            }
        }
        
        // ❌ À ÉVITER : Allocation dans les boucles chaudes
        // for (const auto& item : items) {
        //     auto formatted = nkentseu::string::NkFormat("{0}", item);  // Allocation à chaque tour
        //     Send(formatted.CStr());
        // }
        
        // ✅ PRÉFÉRER : Réutilisation de buffer
        nkentseu::NkString buffer;
        buffer.Reserve(128);
        for (const auto& item : items) {
            buffer.Clear();
            buffer.Append(nkentseu::string::NkToString(item));
            Send(buffer.CStr());  // Aucune allocation après la première itération
        }
    }
*/