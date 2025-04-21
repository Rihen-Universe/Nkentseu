#pragma once

#include "Nkentseu/Platform/Types.h"
#include "Nkentseu/Platform/Inline.h"
#include "Nkentseu/Platform/Assertion.h"
#include "Nkentseu/Platform/Export.h"

#include "StringConverter.h"

#include <cstdarg>
#include <iostream>
#include <unistd.h>  // Pour sbrk

namespace nkentseu  // Namespace for the nkentseu library
{

#define Stru StringUtils  // Directive de raccourci

    class NKENTSEU_API Stru
    {
        private:

            enum class FmtType { CURLY, SQUARE, PAREN, ANGLE };

            template<typename CharT>
            static FmtType GetFmtType(CharT c) {
                switch(c) {
                    case '{': return FmtType::CURLY;
                    case '[': return FmtType::SQUARE;
                    case '(': return FmtType::PAREN;
                    case '<': return FmtType::ANGLE;
                    default:  return FmtType::CURLY;
                }
            }

            template<typename CharT>
            static CharT GetClosingDelim(FmtType type) {
                switch(type) {
                    case FmtType::CURLY:  return '}';
                    case FmtType::SQUARE: return ']';
                    case FmtType::PAREN:  return ')';
                    case FmtType::ANGLE:  return '>';
                    default: return SLTT(CharT);
                }
            }

            template<typename CharT, typename T>
            static CharT* IntToStr(T value, CharT* buffer) {
                bool negative = value < 0;
                usize len = 0;
                
                if (negative) value = -value;
                
                do {
                    buffer[len++] = '0' + (value % 10);
                    value /= 10;
                } while (value > 0);
                
                if (negative) buffer[len++] = '-';
                
                buffer[len] = '\0';
                
                // Inverser la chaîne
                for (usize i = 0; i < len / 2; ++i) {
                    CharT tmp = buffer[len - i - 1];
                    buffer[len - i - 1] = buffer[i];
                    buffer[i] = tmp;
                }
                
                return buffer;
            }

            template<typename CharT, typename T>
            static CharT* UIntToStr(T value, CharT* buffer) {
                usize len = 0;
                do {
                    buffer[len++] = '0' + (value % 10);
                    value /= 10;
                } while (value > 0);
                
                buffer[len] = '\0';
                
                for (usize i = 0; i < len / 2; ++i) {
                    CharT tmp = buffer[len - i - 1];
                    buffer[len - i - 1] = buffer[i];
                    buffer[i] = tmp;
                }
                
                return buffer;
            }

            template<typename CharT, usize N>
            static void FormatFloat(long double value, CharT (&buffer)[N]) {
                // Gestion des valeurs spéciales
                if (value != value) { // NaN
                    buffer[0] = 'N'; buffer[1] = 'a'; buffer[2] = 'N'; buffer[3] = '\0';
                    return;
                }

                if (value > 1e308L || value < -1e308L) { // Infinity simplifié
                    const CharT* inf = "inf";
                    RawMemoryCopy(buffer, inf, ComputeStringLength(inf) + 1);
                    if (value < 0) RawMemoryCopy(buffer, "-inf", 5);
                    return;
                }

                // Gestion du signe
                bool negative = value < 0;
                if (negative) value = -value;

                // Séparation partie entière/fractionnelle
                int64 integer = static_cast<int64>(value);
                long double fraction = value - integer;

                // Conversion partie entière
                CharT int_buf[constants::INT_BUFFER_SIZE];
                IntToStr(integer, int_buf);

                // Conversion partie fractionnelle (6 décimales)
                uint64 frac_scaled = static_cast<uint64>(fraction * 1e6L + 0.5L);
                CharT frac_buf[7];
                UIntToStr(frac_scaled, frac_buf);

                // Construction finale
                usize int_len = ComputeStringLength(int_buf);
                usize frac_len = ComputeStringLength(frac_buf);

                usize pos = 0;
                if (negative) buffer[pos++] = '-';
                RawMemoryCopy(buffer + pos, int_buf, int_len);
                pos += int_len;
                buffer[pos++] = '.';
                
                // Padding avec zéros si nécessaire
                for (usize i = frac_len; i < 6; ++i)
                    buffer[pos++] = '0';
                    
                RawMemoryCopy(buffer + pos, frac_buf, frac_len);
                pos += frac_len;
                buffer[pos] = '\0';
            }

            template<typename CharT>
            static CharT* TokenizeStringInternal(CharT* str, const CharT* delim, CharT** context) {
                CharT* current = str ? str : *context;
                if (!current) return nullptr;

                // Ignorer les délimiteurs initiaux
                while (*current && FindChar(delim, *current)) 
                    ++current;

                if (!*current) {
                    *context = nullptr;
                    return nullptr;
                }

                CharT* token_start = current;

                // Trouver la fin du token
                while (*current && !FindChar(delim, *current)) 
                    ++current;

                if (*current) {
                    *current = SLTT(CharT);
                    *context = current + 1;
                } else {
                    *context = nullptr;
                }

                return token_start;
            }

            template<typename CharT, typename... Args>
            static usize FormatWithSpecs(CharT* buffer, const CharT* format, const Args&... args) {
                // const CharT* args_array[] = { ToString<CharT>(args)... };
                const CharT* args_array[] = { StringConverter<CharT>::Convert(args)... };
                usize written = 0;
                const usize arg_count = sizeof...(args);
                const CharT* format_tmp = format; // Utilisation d'un pointeur const pour éviter la modification du format original

                auto IsClosingDelim = [](CharT c, FmtType fmt_type) {
                    return c == GetClosingDelim<CharT>(fmt_type);
                };

                while (*format_tmp && written < constants::USIZE_MAX) {
                    // Gestion des séquences d'échappement
                    if (*format_tmp == '\\') {
                        format_tmp++;
                        if (!*format_tmp) { // Backslash en fin de chaîne
                            if (written < constants::USIZE_MAX) buffer[written++] = '\\';
                            break;
                        }
                        // Échapper le délimiteur suivant s'il existe
                        buffer[written++] = *format_tmp++;
                    }
                    // Vérification des délimiteurs
                    else if (IsOpeningDelim(*format_tmp)) {
                        const CharT* placeholder_start = format_tmp;
                        FmtType fmt_type = GetFmtType(*format_tmp);
                        CharT closing_delim = GetClosingDelim<CharT>(fmt_type);
                        format_tmp++;

                        // Extraction de l'index
                        usize index = 0;
                        bool valid_index = false;
                        while (*format_tmp >= '0' && *format_tmp <= '9') {
                            index = index * 10 + (*format_tmp - '0');
                            format_tmp++;
                            valid_index = true;
                        }

                        // Validation et fermeture du délimiteur
                        if (valid_index && *format_tmp == closing_delim && index < arg_count) {
                            format_tmp++; // Passer le délimiteur fermant
                            // Copie de l'argument
                            const CharT* str = args_array[index];
                            while (*str && written < constants::USIZE_MAX) buffer[written++] = *str++;
                        } else {
                            // Copie littérale si placeholder invalide
                            while (placeholder_start <= format_tmp && written < constants::USIZE_MAX) {
                                buffer[written++] = *placeholder_start++;
                            }
                        }
                    }
                    // Copie normale des caractères
                    else {
                        buffer[written++] = *format_tmp++;
                    }
                }

                buffer[written] = SLTT(CharT);

                NK_UNUSED IsClosingDelim;
                return written;
            }

            template<typename CharT>
            static bool IsOpeningDelim(CharT c) {
                return c == '{' || c == '[' || c == '(' || c == '<';
            }

            template<typename CharT>
            static usize ParseFormatIndex(const CharT*& format) {
                usize index = 0;
                while (*format >= '0' && *format <= '9') {
                    index = index * 10 + (*format++ - '0');
                }
                return index;
            }

            template<typename CharT>
            static void SkipToClosingDelim(const CharT*& format, CharT closing) {
                while (*format && *format != closing) ++format;
                if (*format) ++format;
            }
        public:

            // Version template variadic avec gestion de la syntaxe
            template<typename CharT, typename... Args>
            static usize Format(CharT* buffer, const CharT* format, Args... args) {
                return FormatWithSpecs(buffer, format, args...);
            }

            // Ajouter ces surcharges spécialisées
            template<typename CharT>
            static usize Format(CharT* buffer, const CharT* format, uint8 value) {
                return Format(buffer, format, static_cast<unsigned int>(value));
            }

            template<typename CharT>
            static usize Format(CharT* buffer, const CharT* format, int8 value) {
                return Format(buffer, format, static_cast<int>(value));
            }

            // ComputeStringLength - Calcule la longueur d'une chaîne (sans le \0)
            template<typename CharT>
            static usize ComputeStringLength(const CharT* str) {
                const CharT* ptr = str;
                while (*ptr != SLTT(CharT)) ++ptr;
                return ptr - str;
            }

            // CompareStrings - Compare deux chaînes
            template<typename CharT>
            static int32 CompareStrings(const CharT* lhs, const CharT* rhs) {
                if (lhs == nullptr || rhs == nullptr) {
                    return (lhs == rhs) ? 0 : (lhs ? -1 : 1);
                }
                usize index = 0;
                while (lhs[index] != SLTT(CharT) && rhs[index] != SLTT(CharT)){
                    if (lhs[index] != rhs[index]) {
                        return static_cast<int32>(lhs[index]) - static_cast<int32>(rhs[index]);
                    }
                    ++index;
                }
                return 0;
            }

            // CompareBoundedStrings - Compare les n premiers caractères
            template<typename CharT>
            static int32 CompareBoundedStrings(const CharT* lhs, const CharT* rhs, usize n) {
                for (usize i = 0; i < n; ++i) {
                    if (lhs[i] != rhs[i]) 
                        return static_cast<int32>(lhs[i]) - static_cast<int32>(rhs[i]);
                    if (!lhs[i]) break;
                }
                return 0;
            }

            // CopyString - Copie une chaîne
            template<typename CharT>
            static CharT* CopyString(CharT* dest, const CharT* src) {
                CharT* original = dest;
                while ((*dest++ = *src++) != SLTT(CharT));
                return original;
            }

            // CopyBoundedString - Copie n caractères maximum

            template<typename CharT>
            static CharT* CopyBoundedString(CharT* dest, const CharT* src, usize n) {
                return CopyBoundedString(dest, 0, src, n); // Appel à la nouvelle version
            }

            template<typename CharT>
            static CharT* CopyBoundedString(CharT* dest, usize destPos, const CharT* src, usize n) {
                if (!dest || !src || n == 0) return dest;

                usize i = 0;
                // Copie des caractères jusqu'à n ou '\0'
                for (; i < n && src[i]; ++i) {
                    dest[destPos + i] = src[i];
                }
                
                // Remplissage du reste avec des nulls
                for (; i < n; ++i) {
                    dest[destPos + i] = SLTT(CharT);
                }
                
                return dest;
            }

            // ConcatenateStrings - Concatène deux chaînes
            template<typename CharT>
            static CharT* ConcatenateStrings(CharT* dest, const CharT* src) {
                CharT* end = dest + ComputeStringLength(dest);
                CopyString(end, src);
                return dest;
            }

            // ConcatenateBoundedStrings - Concatène n caractères
            template<typename CharT>
            static CharT* ConcatenateBoundedStrings(CharT* dest, const CharT* src, usize n) {
                CharT* end = dest + ComputeStringLength(dest);
                usize i = 0;
                while (i < n && src[i]) end[i++] = src[i];
                end[i] = SLTT(CharT); 
                return dest;
            }

            // FindChar - Première occurrence d'un caractère
            template<typename CharT>
            static const CharT* FindChar(const CharT* str, CharT ch) {
                while (*str && *str != ch) ++str;
                return *str ? str : nullptr;
            }

            // FindLastChar - Dernière occurrence d'un caractère
            template<typename CharT>
            static const CharT* FindLastChar(const CharT* str, CharT ch) {
                const CharT* last = nullptr;
                while (*str) {
                    if (*str == ch) last = str;
                    ++str;
                }
                return last;
            }

            // FindSubstring - Première occurrence d'une sous-chaîne
            template<typename CharT>
            static const CharT* FindSubstring(const CharT* haystack, const CharT* needle) {
                if (!*needle) return haystack;
                for (const CharT* h = haystack; *h; ++h) {
                    const CharT* h1 = h, *n1 = needle;
                    while (*h1 && *n1 && (*h1 == *n1)) { ++h1; ++n1; }
                    if (!*n1) return h;
                }
                return nullptr;
            }

            // FindFirstOf - Premier caractère appartenant à un ensemble
            template<typename CharT>
            static const CharT* FindFirstOf(const CharT* str, const CharT* set) {
                for (const CharT* s = str; *s; ++s) 
                    for (const CharT* c = set; *c; ++c) 
                        if (*s == *c) return s;
                return nullptr;
            }

            // SpanInclusive - Longueur du préfixe valide
            template<typename CharT>
            static usize SpanInclusive(const CharT* str, const CharT* set) {
                usize len = 0;
                while (str[len]) {
                    bool found = false;
                    for (const CharT* c = set; *c; ++c)
                        if (str[len] == *c) { found = true; break; }
                    if (!found) break;
                    ++len;
                }
                return len;
            }

            template<typename CharT>
            static usize SpanExclusive(const CharT* str, const CharT* set) {
                usize len = 0;
                while (str[len]) {
                    bool found = false;
                    for (const CharT* c = set; *c; ++c) {
                        if (str[len] == *c) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) { // On s'arrête au premier caractère hors du set
                        break;
                    }
                    ++len;
                }
                return len;
            }

            // RawMemoryCopy - Copie mémoire brute
            template<typename CharT>
            static CharT* RawMemoryCopy(CharT* dest, const CharT* src, usize count) {
                for (usize i = 0; i < count; ++i) dest[i] = src[i];
                return dest;
            }

            // SafeMemoryMove - Copie mémoire sécurisée
            template<typename CharT>
            static CharT* SafeMemoryMove(CharT* dest, const CharT* src, usize count) {
                if (dest == src) return dest;
                if (dest < src) {
                    for (usize i = 0; i < count; ++i) dest[i] = src[i];
                } else {
                    for (usize i = count; i > 0; --i) dest[i-1] = src[i-1];
                }
                return dest;
            }

            // MemoryCompare - Comparaison mémoire
            template<typename CharT>
            static int32 MemoryCompare(const CharT* lhs, const CharT* rhs, usize count) {
                for (usize i = 0; i < count; ++i) {
                    if (lhs[i] != rhs[i])
                        return static_cast<int32>(lhs[i]) - static_cast<int32>(rhs[i]);
                }
                return 0;
            }

            // MemorySet - Remplissage mémoire
            template<typename CharT>
            static CharT* MemorySet(CharT* dest, CharT value, usize count) {
                for (usize i = 0; i < count; ++i) dest[i] = value;
                return dest;
            }

            // BoundedStringLength - Longueur limitée à max_len
            template<typename CharT>
            static usize BoundedStringLength(const CharT* str, usize max_len) {
                usize len = 0;
                while (len < max_len && str[len] != SLTT(CharT)) ++len;
                return len;
            }

            // TokenizeString - Découpeur non thread-safe
            template<typename CharT>
            static CharT* TokenizeString(CharT* str, const CharT* delim) {
                static CharT* context = nullptr;
                return TokenizeStringInternal(str, delim, &context);
            }

            // TokenizeStringSafe - Version réentrante (thread-safe)
            template<typename CharT>
            static CharT* TokenizeStringSafe(CharT* str, const CharT* delim, CharT** context) {
                return TokenizeStringInternal(str, delim, context);
            }

            // DuplicateString - Duplication de chaîne
            template<typename CharT>
            static CharT* DuplicateString(const CharT* src) {
                if (!src) return nullptr;
                usize len = ComputeStringLength(src);
                CharT* dest = new CharT[len + 1];
                RawMemoryCopy(dest, src, len);
                dest[len] = SLTT(CharT);
                return dest;
            }

            // DuplicateBoundedString - Duplication limitée à n caractères
            template<typename CharT>
            static CharT* DuplicateBoundedString(const CharT* src, usize n) {
                usize len = BoundedStringLength(src, n);
                CharT* dest = new CharT[len + 1];
                RawMemoryCopy(dest, src, len);
                dest[len] = SLTT(CharT);
                return dest;
            }

            template<typename CharT>
            static int64 StringToInteger(const CharT* str) {
                int64 result = 0;
                int sign = 1;
                
                while (IsWhitespace(*str)) ++str;
                
                if (*str == '-') { sign = -1; ++str; }
                else if (*str == '+') ++str;
                
                while (IsDigit(*str)) {
                    result = result * 10 + (*str - '0');
                    ++str;
                }
                
                return sign * result;
            }

            template<typename CharT>
            static double StringToFloat(const CharT* str) {
                return StringToDouble<CharT>(str, nullptr);
            }

            template<typename CharT>
            static int64 StringToIntegerBase(const CharT* str, CharT** endptr, int32 base) {
                // Validation base
                if (base < 2 || base > 36) return 0;
                
                int64 result = 0;
                int sign = 1;
                
                while (IsWhitespace(*str)) ++str;
                
                if (*str == '-') { sign = -1; ++str; }
                else if (*str == '+') ++str;
                
                while (true) {
                    int digit = CharToDigit(*str, base);
                    if (digit < 0) break;
                    
                    result = result * base + digit;
                    ++str;
                }
                
                if (endptr) *endptr = const_cast<CharT*>(str);
                return sign * result;
            }

            template<typename CharT>
            static double StringToDouble(const CharT* str, CharT** endptr) {
                double result = 0.0;
                int sign = 1;
                double fraction = 0.1;
                int exponent = 0;
                
                while (IsWhitespace(*str)) ++str;
                
                if (*str == '-') { sign = -1; ++str; }
                else if (*str == '+') ++str;
                
                // Partie entière
                while (IsDigit(*str)) {
                    result = result * 10.0 + (*str - '0');
                    ++str;
                }
                
                // Partie fractionnelle
                if (*str == '.') {
                    ++str;
                    while (IsDigit(*str)) {
                        result += (*str - '0') * fraction;
                        fraction *= 0.1;
                        ++str;
                    }
                }
                
                // Exposant
                if (*str == 'e' || *str == 'E') {
                    ++str;
                    int exp_sign = 1;
                    if (*str == '-') { exp_sign = -1; ++str; }
                    else if (*str == '+') ++str;
                    
                    while (IsDigit(*str)) {
                        exponent = exponent * 10 + (*str - '0');
                        ++str;
                    }
                    exponent *= exp_sign;
                }
                
                if (endptr) *endptr = const_cast<CharT*>(str);
                return sign * result * Pow10(exponent);
            }

            /* MANIPULATIONS DE CHAÎNES */
            template<typename CharT>
            static void ReverseStringInPlace(CharT* str) {
                usize len = ComputeStringLength(str);
                for (usize i = 0; i < len/2; ++i) {
                    CharT tmp = str[len - i - 1];
                    str[len - i - 1] = str[i];
                    str[i] = tmp;
                }
            }

            template<typename CharT>
            static void StringToIntegerBaseower(CharT* str) {
                for (; *str; ++str) {
                    if (*str >= 'A' && *str <= 'Z') *str += 32;
                }
            }

            template<typename CharT>
            static void StrToUpper(CharT* str) {
                for (; *str; ++str) {
                    if (*str >= 'a' && *str <= 'z') *str -= 32;
                }
            }

            template<typename CharT>
            static void TrimWhitespace(CharT* str) {
                usize start = 0;
                usize end = ComputeStringLength(str);
                
                while (IsWhitespace(str[start])) ++start;
                while (end > start && IsWhitespace(str[end-1])) --end;
                
                SafeMemoryMove(str, str + start, end - start);
                str[end - start] = SLTT(CharT);
            }

            template<typename CharT>
            static void ReplaceCharacters(CharT* str, CharT oldChar, CharT newChar) {
                for (; *str; ++str) {
                    if (*str == oldChar) *str = newChar;
                }
            }

            template<typename CharT>
            static void ReplaceSubstring(CharT* str, const CharT* oldSub, const CharT* newSub) {
                if (!str || !oldSub || !newSub) return;
                
                const usize oldLen = ComputeStringLength(oldSub);
                const usize newLen = ComputeStringLength(newSub);
                CharT* current = str;
                
                while (true) {
                    // Trouver la prochaine occurrence avec const
                    const CharT* found = FindSubstring(current, oldSub);
                    if (!found) break;
                    
                    // Convertir en pointeur modifiable via offset
                    const usize pos = found - current;
                    CharT* modifiablePos = current + pos;
                    
                    // Calculer la taille de la queue à déplacer
                    const usize tailSize = ComputeStringLength(modifiablePos + oldLen) + 1; // +1 pour '\0'
                    
                    // Déplacer la partie droite de la chaîne
                    SafeMemoryMove(
                        modifiablePos + newLen,    // Destination
                        modifiablePos + oldLen,    // Source 
                        tailSize * sizeof(CharT)   // Taille en bytes
                    );
                    
                    // Copier la nouvelle sous-chaîne
                    RawMemoryCopy(modifiablePos, newSub, newLen);
                    
                    // Avancer après le remplacement
                    current = modifiablePos + newLen;
                    
                    // Éviter boucle infinie si newSub contient oldSub
                    if (newLen == 0 || (newLen >= oldLen && FindSubstring(newSub, oldSub))) 
                        break;
                }
            }

            template<typename CharT>
            static CharT** SplitString(const CharT* str, CharT delim, usize* count) {
                usize tokens = 1;
                const CharT* tmp = str;
                while (*tmp) if (*tmp++ == delim) ++tokens;
                
                CharT** result = new CharT*[tokens];
                usize idx = 0;
                
                const CharT* start = str;
                while (*str) {
                    if (*str == delim) {
                        usize len = str - start;
                        result[idx] = new CharT[len + 1];
                        CopyBoundedString(result[idx], start, len);
                        result[idx++][len] = SLTT(CharT);
                        start = str + 1;
                    }
                    ++str;
                }
                
                // Dernier token
                usize len = str - start;
                result[idx] = new CharT[len + 1];
                CopyBoundedString(result[idx], start, len);
                result[idx][len] = SLTT(CharT);
                
                *count = tokens;
                return result;
            }

            template<typename CharT>
            static CharT* JoinStrings(const CharT** parts, usize count, CharT delim) {
                usize total_len = 0;
                for (usize i = 0; i < count; ++i) 
                    total_len += ComputeStringLength(parts[i]) + 1;
                
                CharT* result = new CharT[total_len];
                CharT* ptr = result;
                
                for (usize i = 0; i < count; ++i) {
                    usize len = ComputeStringLength(parts[i]);
                    CopyString(ptr, parts[i]);
                    ptr += len;
                    if (i != count - 1) *ptr++ = delim;
                }
                
                *ptr = SLTT(CharT);
                return result;
            }

            template<typename CharT>
            static void FreeSplitStringResult(CharT** result, usize count) {
                for (usize i = 0; i < count; ++i) delete[] result[i];
                delete[] result;
            }

            template<typename CharT>
            static void FreeJoinStringsResult(CharT* result) {
                delete[] result;
            }

            /* CLASSIFICATION DE CARACTÈRES */

            template<typename CharT>
            static bool IsWhitespace(CharT c) {
                return c == ' ' || (c >= '\t' && c <= '\r');
            }

            template<typename CharT>
            static bool IsDigit(CharT c) {
                return c >= '0' && c <= '9';
            }

            template<typename CharT>
            static int32 CharToDigit(CharT c, int base) {
                int value = (c >= '0' && c <= '9') ? c - '0' :
                            (c >= 'A' && c <= 'Z') ? c - 'A' + 10 :
                            (c >= 'a' && c <= 'z') ? c - 'a' + 10 : -1;
                
                return (value >= 0 && value < base) ? value : -1;
            }

            template<typename CharT>
            static uint64 StringToUint64(const CharT* str, CharT** endptr, int base) {
                if (base < 0 || base > 36) {
                    if (endptr) *endptr = const_cast<CharT*>(str);
                    return 0;
                }

                uint64 result = 0;
                const CharT* original = str;

                while (IsWhitespace(*str)) ++str;

                while (true) {
                    int digit = CharToDigit(*str, base);
                    if (digit < 0) break;

                    // Vérification overflow
                    if (result > (ULLONG_MAX - digit) / base) {
                        result = ULLONG_MAX;
                        ++str;
                        break;
                    }

                    result = result * base + digit;
                    ++str;
                }

                if (endptr) *endptr = (str == original) 
                    ? const_cast<CharT*>(original) 
                    : const_cast<CharT*>(str);

                return result;
            }

            template<typename CharT>
            static float32 StringToFloat(const CharT* str, CharT** endptr) {
                return static_cast<float32>(StringToDouble(str, endptr));
            }

            template<typename CharT>
            static float80 StringToIntegerBaseD(const CharT* str, CharT** endptr) {
                float80 result = 0.0L;
                int sign = 1;
                float80 fraction = 0.1L;
                int exponent = 0;

                while (IsWhitespace(*str)) ++str;

                if (*str == '-') { sign = -1; ++str; }
                else if (*str == '+') ++str;

                // Partie entière
                while (IsDigit(*str)) {
                    result = result * 10.0L + (*str - '0');
                    ++str;
                }

                // Partie fractionnelle
                if (*str == '.') {
                    ++str;
                    while (IsDigit(*str)) {
                        result += (*str - '0') * fraction;
                        fraction *= 0.1L;
                        ++str;
                    }
                }

                // Exposant
                if (*str == 'e' || *str == 'E') {
                    ++str;
                    int exp_sign = 1;
                    if (*str == '-') { exp_sign = -1; ++str; }
                    else if (*str == '+') ++str;

                    while (IsDigit(*str)) {
                        exponent = exponent * 10 + (*str - '0');
                        ++str;
                    }
                    exponent *= exp_sign;
                }

                if (endptr) *endptr = const_cast<CharT*>(str);
                return sign * result * Pow10L(exponent);
            }

            static double Pow10(int32 exponent) {
                float64 result = 1.0;
                float64 base = 10.0;
                if (exponent < 0) {
                    base = 0.1;
                    exponent = -exponent;
                }
                while (exponent--) result *= base;
                return result;
            }

            static float80 Pow10L(int32 exponent) {
                float80 result = 1.0L;
                float80 base = 10.0L;
                if (exponent < 0) {
                    base = 0.1L;
                    exponent = -exponent;
                }
                while (exponent-- > 0) result *= base;
                return result;
            }

            template<typename CharT>
            static bool IsAlpha(CharT c) {
                // Vérification des caractères ASCII
                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                    return true;
                }
                
                // Gestion étendue pour les caractères Unicode si le type est suffisamment large
                if constexpr (sizeof(CharT) > 1) {
                    // Lettres accentuées latines (À-Ö Ø-ß à-ö ø-ÿ)
                    if ((c >= 0xC0 && c <= 0xD6) || 
                        (c >= 0xD8 && c <= 0xDE) || 
                        (c >= 0xE0 && c <= 0xF6) || 
                        (c >= 0xF8 && c <= 0xFF)) {
                        return true;
                    }
                    
                    // Alphabet cyrillique (А-Я а-я ёЁ)
                    if ((c >= 0x0410 && c <= 0x042F) ||  // Majuscules
                        (c >= 0x0430 && c <= 0x044F) ||  // Minuscules
                        (c == 0x0401 || c == 0x0451)) {  // Ё et ё
                        return true;
                    }
                    
                    // Alphabet grec (Α-Ω α-ω)
                    if ((c >= 0x0391 && c <= 0x03A9) ||  // Majuscules
                        (c >= 0x03B1 && c <= 0x03C9)) {  // Minuscules
                        return true;
                    }
                }
                
                return false;
            }

            template<typename CharT>
            static bool IsUpper(CharT c) {
                if constexpr (sizeof(CharT) == 1) { // ASCII/Latin-1 (8-bit)
                    return (c >= 'A' && c <= 'Z') || 
                        (c >= 0xC0 && c <= 0xDE && c != 0xD7); // Latin-1 Supplement
                }
                else if constexpr (sizeof(CharT) == 2) { // UTF-16/wchar_t
                    return (c >= u'A' && c <= u'Z') ||
                        (c >= 0x00C0 && c <= 0x00DE && c != 0x00D7) || // Latin-1 Supplement
                        (c >= 0x0100 && (c & 0xFFFE) == 0x0100) || // Latin Extended-A/B
                        (c >= 0x0400 && c <= 0x042F); // Cyrillique
                }
                else { // UTF-32/char32_t (4-byte)
                    return (c >= U'A' && c <= U'Z') ||
                        (c >= 0x00C0 && c <= 0x00DE && c != 0x00D7) || // Latin-1 Supplement
                        (c >= 0x0100 && (c & 0xFFFE) == 0x0100) || // Latin Extended
                        (c >= 0x0391 && c <= 0x03A9); // Grec
                }
            }

            template<typename CharT>
            static bool IsLower(CharT c) {
                if constexpr (sizeof(CharT) == 1) {
                    return (c >= 'a' && c <= 'z') || 
                        (c >= 0xDF && c <= 0xFF && c != 0xF7);
                }
                else if constexpr (sizeof(CharT) == 2) {
                    return (c >= u'a' && c <= u'z') ||
                        (c >= 0x0101 && (c & 0xFFFE) == 0x0101) ||
                        (c >= 0x0430 && c <= 0x044F);
                }
                else {
                    return (c >= U'a' && c <= U'z') ||
                        (c >= U'à' && c <= U'ÿ' && c != U'÷') ||
                        (c >= U'α' && c <= U'ω');
                }
            }

            /* ÉCHANGE DE VALEURS */
            template<typename T>
            static void Swap(T& a, T& b) {
                T temp = a;
                a = b;
                b = temp;
            }

            // Version optimisée pour les types primitifs
            template<typename T>
            static void FastSwap(T& a, T& b) {
                // static_assert(std::is_integral_v<T>, "FastSwap only for integral types");
                if (&a != &b) {
                    a ^= b;
                    b ^= a;
                    a ^= b;
                }
            }


            template<typename CharT>
            static CharT ToLower(CharT c) {
                // Conversion pour ASCII/Latin-1
                if (c >= 'A' && c <= 'Z') {
                    return c + ('a' - 'A');
                }
                // Gestion étendue pour les caractères accentués
                else if constexpr(sizeof(CharT) > 1) {
                    // Latin étendu (À-ß → à-ÿ)
                    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) {
                        return c + 32;
                    }
                    // Cyrillique (А-Я → а-я)
                    else if (c >= 0x0410 && c <= 0x042F) {
                        return c + 32;
                    }
                    // Grec (Α-Ω → α-ω)
                    else if (c >= 0x0391 && c <= 0x03A9) {
                        return c + 32;
                    }
                }
                return c;
            }

            template<typename CharT>
            static CharT ToUpper(CharT c) {
                // Conversion pour ASCII/Latin-1
                if (c >= 'a' && c <= 'z') {
                    return c - ('a' - 'A');
                }
                // Gestion étendue pour les caractères accentués
                else if constexpr(sizeof(CharT) > 1) {
                    // Latin étendu (À-ß à-ÿ)
                    if (c >= 0xE0 && c <= 0xFE && c != 0xF7) {
                        return c - 32;
                    }
                    // Cyrillique (а-я → А-Я)
                    else if (c >= 0x0430 && c <= 0x044F) {
                        return c - 32;
                    }
                    // Grec (α-ω → Α-Ω)
                    else if (c >= 0x03B1 && c <= 0x03C9) {
                        return c - 32;
                    }
                }
                return c;
            }

            template<typename CharT>
            static void ToLower(CharT* str) {
                if (!str) return;

                for (usize i = 0; str[i] != '\0'; ++i) {
                    str[i] = ToLower(str[i]);
                }
            }

            template<typename CharT>
            static void ToUpper(CharT* str) {
                if (!str) return;

                for (usize i = 0; str[i] != '\0'; ++i) {
                    str[i] = ToUpper(str[i]);
                }
            }

            // Version avec buffer externe
            template<typename CharT>
            static void ToLower(const CharT* src, CharT* dest, usize bufferSize) {
                if (!src || !dest || bufferSize == 0) return;

                usize i;
                for (i = 0; src[i] && i < bufferSize - 1; ++i) {
                    dest[i] = ToLower(src[i]);
                }
                dest[i] = '\0';
            }

            template<typename CharT>
            static void ToUpper(const CharT* src, CharT* dest, usize bufferSize) {
                if (!src || !dest || bufferSize == 0) return;

                usize i;
                for (i = 0; src[i] && i < bufferSize - 1; ++i) {
                    dest[i] = ToUpper(src[i]);
                }
                dest[i] = '\0';
            }

            template<typename CharT>
            static bool IsPunctuation(CharT c) {
                // ASCII punctuation (common cases)
                if (c >= 0x21 && c <= 0x2F) return true;
                if (c >= 0x3A && c <= 0x40) return true;
                if (c >= 0x5B && c <= 0x60) return true;
                if (c >= 0x7B && c <= 0x7E) return true;
    
                // Unicode punctuation blocks (simplified)
                if constexpr (sizeof(CharT) > 1) {
                    if (c >= 0x2000 && c <= 0x206F) return true; // General Punctuation
                    if (c >= 0x3000 && c <= 0x303F) return true; // CJK Symbols and Punctuation
                }
                return false;
            }

            template<typename CharT>
            static usize Length(const CharT* str) {
                const CharT* ptr = str;
                while (*ptr != SLTT(CharT)) ++ptr;
                return ptr - str;
            }

            template<typename CharT>
            static int Compare(const CharT* str1, const CharT* str2) {
                if (str1 == nullptr || str2 == nullptr) {
                    return (str1 == str2) ? 0 : (str1 ? -1 : 1);
                }
                usize index = 0;
                while (str1[index] != SLTT(CharT) && str2[index] != SLTT(CharT)){
                    if (str1[index] != str2[index]) {
                        return static_cast<int32>(str1[index]) - static_cast<int32>(str2[index]);
                    }
                    ++index;
                }
                return 0;
            }

            template<typename CharT>
            static CharT* Tokenize(CharT* str, const CharT* delim, CharT** context) {
                CharT* current = str ? str : *context;
                if (!current) return nullptr;

                // Ignorer les délimiteurs initiaux
                while (*current && FindChar(delim, *current)) 
                    ++current;

                if (!*current) {
                    *context = nullptr;
                    return nullptr;
                }

                CharT* token_start = current;

                // Trouver la fin du token
                while (*current && !FindChar(delim, *current)) 
                    ++current;

                if (*current) {
                    *current = SLTT(CharT);
                    *context = current + 1;
                } else {
                    *context = nullptr;
                }

                return token_start;
            }
    };
} // namespace nkentseu  // Namespace for the nkentseu library
