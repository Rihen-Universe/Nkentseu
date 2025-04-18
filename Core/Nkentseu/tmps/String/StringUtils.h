#pragma once

#include "Nkentseu/Platform/Types.h"
#include "Nkentseu/Platform/Inline.h"
#include "Nkentseu/Platform/Assertion.h"
#include "Nkentseu/Platform/Export.h"

#include <cstdarg>

namespace nkentseu  // Namespace for the nkentseu library
{
    // Macro pour générer les spécialisations de ConversionTraits
    #define DEFINE_CONVERSION_TRAITS(CharT, PREFIX) \
        template<> const CharT ConversionTraits<CharT>::TrueStr[] = PREFIX"true"; \
        template<> const CharT ConversionTraits<CharT>::FalseStr[] = PREFIX"false"; \
        template<> const CharT ConversionTraits<CharT>::NullPtrStr[] = PREFIX"(null)";

    template<typename CharT>
    struct ConversionTraits {
        static const CharT* TrueStr;
        static const CharT* FalseStr;
        static const CharT* NullPtrStr;
    };

    // Génération des spécialisations pour chaque type de caractère
    DEFINE_CONVERSION_TRAITS(charb,)
    DEFINE_CONVERSION_TRAITS(char8, u8)
    DEFINE_CONVERSION_TRAITS(char16, u)
    DEFINE_CONVERSION_TRAITS(char32, U)
    DEFINE_CONVERSION_TRAITS(wchar, L)

    class NKENTSEU_API StringUtils
    {
        private:

            // Buffers statiques pour les conversions
            static constexpr usize INT_BUFFER_SIZE = 32;
            static constexpr usize FLOAT_BUFFER_SIZE = 64;
            static constexpr usize BOOL_BUFFER_SIZE = 8;

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
                    default: return CharT(0);
                }
            }

            template<typename CharT, typename T>
            static CharT* IntToStr(T value, CharT* buffer) {
                bool negative = value < 0;
                usize i = 0;
                
                if (negative) value = -value;
                
                do {
                    buffer[i++] = '0' + (value % 10);
                    value /= 10;
                } while (value > 0);
                
                if (negative) buffer[i++] = '-';
                
                buffer[i] = '\0';
                
                // Inverser la chaîne
                for (usize j = 0; j < i / 2; ++j) {
                    Swap(buffer[j], buffer[i - j - 1]);
                }
                
                return buffer;
            }

            template<typename CharT, typename T>
            static CharT* UIntToStr(T value, CharT* buffer) {
                usize i = 0;
                do {
                    buffer[i++] = '0' + (value % 10);
                    value /= 10;
                } while (value > 0);
                
                buffer[i] = '\0';
                
                for (usize j = 0; j < i / 2; ++j) {
                    Swap(buffer[j], buffer[i - j - 1]);
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
                    MemCpy(buffer, inf, StrLen(inf) + 1);
                    if (value < 0) MemCpy(buffer, "-inf", 5);
                    return;
                }

                // Gestion du signe
                bool negative = value < 0;
                if (negative) value = -value;

                // Séparation partie entière/fractionnelle
                int64 integer = static_cast<int64>(value);
                long double fraction = value - integer;

                // Conversion partie entière
                CharT int_buf[INT_BUFFER_SIZE];
                IntToStr(integer, int_buf);

                // Conversion partie fractionnelle (6 décimales)
                uint64 frac_scaled = static_cast<uint64>(fraction * 1e6L + 0.5L);
                CharT frac_buf[7];
                UIntToStr(frac_scaled, frac_buf);

                // Construction finale
                usize int_len = StrLen(int_buf);
                usize frac_len = StrLen(frac_buf);

                usize pos = 0;
                if (negative) buffer[pos++] = '-';
                MemCpy(buffer + pos, int_buf, int_len);
                pos += int_len;
                buffer[pos++] = '.';
                
                // Padding avec zéros si nécessaire
                for (usize i = frac_len; i < 6; ++i)
                    buffer[pos++] = '0';
                    
                MemCpy(buffer + pos, frac_buf, frac_len);
                pos += frac_len;
                buffer[pos] = '\0';
            }

            template<typename CharT>
            static CharT* StrTokInternal(CharT* str, const CharT* delim, CharT** context) {
                CharT* current = str ? str : *context;
                if (!current) return nullptr;

                // Ignorer les délimiteurs initiaux
                while (*current && StrChr(delim, *current)) 
                    ++current;

                if (!*current) {
                    *context = nullptr;
                    return nullptr;
                }

                CharT* token_start = current;

                // Trouver la fin du token
                while (*current && !StrChr(delim, *current)) 
                    ++current;

                if (*current) {
                    *current = CharT(0);
                    *context = current + 1;
                } else {
                    *context = nullptr;
                }

                return token_start;
            }

            template<typename CharT, typename... Args>
            static usize FormatWithSpecs(CharT* buffer, const CharT* format, const Args&... args) {
                const CharT* args_array[] = { ToString<CharT>(args)... };
                usize written = 0;
                const usize arg_count = sizeof...(args);
                const CharT* format_tmp = format; // Utilisation d'un pointeur const pour éviter la modification du format original

                auto IsClosingDelim = [](CharT c, FmtType fmt_type) {
                    return c == GetClosingDelim<CharT>(fmt_type);
                };

                while (*format_tmp && written < USIZE_MAX) {
                    // Gestion des séquences d'échappement
                    if (*format_tmp == '\\') {
                        format_tmp++;
                        if (!*format_tmp) { // Backslash en fin de chaîne
                            if (written < USIZE_MAX) buffer[written++] = '\\';
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
                            while (*str && written < USIZE_MAX) buffer[written++] = *str++;
                        } else {
                            // Copie littérale si placeholder invalide
                            while (placeholder_start <= format_tmp && written < USIZE_MAX) {
                                buffer[written++] = *placeholder_start++;
                            }
                        }
                    }
                    // Copie normale des caractères
                    else {
                        buffer[written++] = *format_tmp++;
                    }
                }

                buffer[written] = CharT(0);
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

            template<typename CharT>
            static usize CopyString(CharT* dest, const CharT* src) {
                usize len = 0;
                while (*src) {
                    dest[len++] = *src++;
                }
                return len;
            }
        public:
            template<typename CharT, typename T>
            static const CharT* ToString(T value) {
                static_assert(sizeof(CharT) == 0, "No specialization for this type");
            }
            
            // ToString specialisations pour les types de base
            template<typename CharT, typename T> static const CharT* ToString(T value);
            
            // Spécialisations ToString pour les types numériques
            template<typename CharT>
            static const CharT* ToString<int8>(int8 value) {
                static CharT buffer[INT_BUFFER_SIZE];
                return IntToStr(static_cast<int32>(value), buffer);
            }

            template<typename CharT>
            static const CharT* ToString<int16>(int16 value) {
                static CharT buffer[INT_BUFFER_SIZE];
                return IntToStr(static_cast<int32>(value), buffer);
            }

            template<typename CharT>
            static const CharT* ToString<int32>(int32 value) {
                static CharT buffer[INT_BUFFER_SIZE];
                return IntToStr(value, buffer);
            }

            template<typename CharT>
            static const CharT* ToString<int64>(int64 value) {
                static CharT buffer[INT_BUFFER_SIZE];
                return IntToStr(value, buffer);
            }

            template<typename CharT>
            static const CharT* ToString<uint8>(uint8 value) {
                static CharT buffer[INT_BUFFER_SIZE];
                return UIntToStr(value, buffer);
            }

            template<typename CharT>
            static const CharT* ToString<uint16>(uint16 value) {
                static CharT buffer[INT_BUFFER_SIZE];
                return UIntToStr(value, buffer);
            }

            template<typename CharT>
            static const CharT* ToString<uint32>(uint32 value) {
                static CharT buffer[INT_BUFFER_SIZE];
                return UIntToStr(value, buffer);
            }

            template<typename CharT>
            static const CharT* ToString<uint64>(uint64 value) {
                static CharT buffer[INT_BUFFER_SIZE];
                return UIntToStr(value, buffer);
            }

            // template<typename CharT>
            // static const CharT* ToString<float32>(float32 value) {
            //     static CharT buffer[FLOAT_BUFFER_SIZE];
            //     usize len = 0;
            //     int32 integer = static_cast<int32>(value);
            //     float32 fractional = value - integer;
                
            //     // Partie entière
            //     const CharT* int_part = ToString(integer);
            //     while (*int_part) buffer[len++] = *int_part++;
                
            //     // Partie fractionnaire
            //     buffer[len++] = '.';
            //     for (usize i = 0; i < 6; ++i) {
            //         fractional *= 10;
            //         buffer[len++] = '0' + static_cast<int32>(fractional);
            //         fractional -= static_cast<int32>(fractional);
            //     }
                
            //     buffer[len] = '\0';
            //     return buffer;
            // }

            template<typename CharT>
            const charb* ToString<float32>(float32 value) {
                static charb buffer[FLOAT_BUFFER_SIZE];
                FormatFloat(value, buffer); // Utiliser la même méthode que float64/80
                return buffer;
            }

            template<typename CharT>
            static const CharT* ToString<float64>(float64 value) {
                static CharT buffer[FLOAT_BUFFER_SIZE];
                FormatFloat(static_cast<long double>(value), buffer);
                return buffer;
            }

            template<typename CharT>
            static const CharT* ToString<float80>(float80 value) {
                static CharT buffer[FLOAT_BUFFER_SIZE];
                FormatFloat(value, buffer);
                return buffer;
            }

            template<typename CharT>
            static const CharT* ToString<Boolean>(Boolean value) {
                static const CharT* true_str = "true";
                static const CharT* false_str = "false";
                return value ? true_str : false_str;
            }

            // Version template variadic avec gestion de la syntaxe
            template<typename CharT, typename... Args>
            static usize SPrintF(CharT* buffer, const CharT* format, Args... args) {
                return FormatWithSpecs(buffer, format, args...);
            }

            // Version générique pour les pointeurs
            template<typename CharT, typename T>
            static const CharT* ToString(const T* ptr) {
                if (!ptr) return ToString<CharT, int32>(0); // "(null)"
                return ToString<CharT>(*ptr);
            }

            // Pour wchar
            template<>
            const wchar* ToString<wchar, Boolean>(Boolean value) {
                static const wchar* true_str = L"true";
                static const wchar* false_str = L"false";
                return value ? true_str : false_str;
            }

            // VSPrintF implémentation de base
            template<typename CharT>
            static usize VSPrintF(CharT* buffer, const CharT* format, va_list args) {
                usize written = 0;
                const CharT* fmt = format;
                
                while (*fmt) {
                    if (*fmt == '%' || *fmt == '{' || *fmt == '[' || *fmt == '(' || *fmt == '<') {
                        FmtType type = GetFmtType(*fmt);
                        const CharT* start = fmt++;
                        
                        // Extraction de l'index
                        usize index = 0;
                        while (*fmt >= '0' && *fmt <= '9') {
                            index = index * 10 + (*fmt++ - '0');
                        }
                        
                        // Trouver la fin du format
                        CharT closing = GetClosingDelim<CharT>(type);
                        while (*fmt && *fmt != closing) ++fmt;
                        if (!*fmt) break;
                        
                        // Récupération de l'argument
                        const void* arg = va_arg(args, void*);
                        const CharT* str = ToString(static_cast<const decltype(arg)>(arg));
                        
                        // Copie dans le buffer
                        while (*str && written < USIZE_MAX) {
                            buffer[written++] = *str++;
                        }
                        
                        ++fmt;
                    } else {
                        buffer[written++] = *fmt++;
                    }
                }
                
                buffer[written] = CharT(0);
                return written;
            }

            // SPrintF avec gestion de buffer
            template<typename CharT, typename... Args>
            static usize SPrintF(CharT* buffer, const CharT* format, Args... args) {
                va_list vl;
                va_start(vl, args);
                usize result = VSPrintF(buffer, format, vl);
                va_end(vl);
                return result;
            }

            // SnPrintF avec taille maximale
            template<typename CharT>
            static usize SnPrintF(CharT* buffer, usize max_size, const CharT* format, ...) {
                va_list args;
                va_start(args, format);
                usize result = VSnPrintF(buffer, max_size, format, args);
                va_end(args);
                return result;
            }

            template<typename CharT>
            static usize VSnPrintF(CharT* buffer, usize max_size, const CharT* format, va_list args) {
                usize written = 0;
                const CharT* fmt = format;
                
                while (*fmt && written < max_size) {
                    if (*fmt == '%' || *fmt == '{' || *fmt == '[' || *fmt == '(' || *fmt == '<') {
                        FmtType type = GetFmtType(*fmt);
                        const CharT* start = fmt++;
                        
                        // Extraction de l'index
                        usize index = 0;
                        while (*fmt >= '0' && *fmt <= '9') {
                            index = index * 10 + (*fmt++ - '0');
                        }
                        
                        // Trouver la fin du format
                        CharT closing = GetClosingDelim<CharT>(type);
                        while (*fmt && *fmt != closing) ++fmt;
                        if (!*fmt) break;
                        
                        // Récupération de l'argument
                        const void* arg = va_arg(args, void*);
                        const CharT* str = ToString(static_cast<const decltype(arg)>(arg));
                        
                        // Copie sécurisée
                        while (*str && written < max_size - 1) {
                            buffer[written++] = *str++;
                        }
                        
                        ++fmt;
                    } else {
                        buffer[written++] = *fmt++;
                    }
                }
                
                // Terminaison nulle si possible
                if (written < max_size) buffer[written] = CharT(0);
                else if (max_size > 0) buffer[max_size - 1] = CharT(0);
                
                return written;
            }

            // StrLen - Calcule la longueur d'une chaîne (sans le \0)
            template<typename CharT>
            static usize StrLen(const CharT* str) {
                const CharT* ptr = str;
                while (*ptr != CharT(0)) ++ptr;
                return ptr - str;
            }

            // StrCmp - Compare deux chaînes
            template<typename CharT>
            static int32 StrCmp(const CharT* lhs, const CharT* rhs) {
                while (*lhs && (*lhs == *rhs)) { ++lhs; ++rhs; }
                return static_cast<int32>(*lhs) - static_cast<int32>(*rhs);
            }

            // StrNCmp - Compare les n premiers caractères
            template<typename CharT>
            static int32 StrNCmp(const CharT* lhs, const CharT* rhs, usize n) {
                for (usize i = 0; i < n; ++i) {
                    if (lhs[i] != rhs[i]) 
                        return static_cast<int32>(lhs[i]) - static_cast<int32>(rhs[i]);
                    if (!lhs[i]) break;
                }
                return 0;
            }

            // StrCpy - Copie une chaîne
            template<typename CharT>
            static CharT* StrCpy(CharT* dest, const CharT* src) {
                CharT* original = dest;
                while ((*dest++ = *src++) != CharT(0));
                return original;
            }

            // StrNCpy - Copie n caractères maximum
            template<typename CharT>
            static CharT* StrNCpy(CharT* dest, const CharT* src, usize n) {
                usize i = 0;
                for (; i < n && src[i]; ++i) dest[i] = src[i];
                for (; i < n; ++i) dest[i] = CharT(0);
                return dest;
            }

            // StrCat - Concatène deux chaînes
            template<typename CharT>
            static CharT* StrCat(CharT* dest, const CharT* src) {
                CharT* end = dest + StrLen(dest);
                StrCpy(end, src);
                return dest;
            }

            // StrNConcat - Concatène n caractères
            template<typename CharT>
            static CharT* StrNConcat(CharT* dest, const CharT* src, usize n) {
                CharT* end = dest + StrLen(dest);
                usize i = 0;
                while (i < n && src[i]) end[i++] = src[i];
                end[i] = CharT(0);
                return dest;
            }

            // StrChr - Première occurrence d'un caractère
            template<typename CharT>
            static const CharT* StrChr(const CharT* str, CharT ch) {
                while (*str && *str != ch) ++str;
                return *str ? str : nullptr;
            }

            // StrRChr - Dernière occurrence d'un caractère
            template<typename CharT>
            static const CharT* StrRChr(const CharT* str, CharT ch) {
                const CharT* last = nullptr;
                while (*str) {
                    if (*str == ch) last = str;
                    ++str;
                }
                return last;
            }

            // StrStr - Première occurrence d'une sous-chaîne
            template<typename CharT>
            static const CharT* StrStr(const CharT* haystack, const CharT* needle) {
                if (!*needle) return haystack;
                for (const CharT* h = haystack; *h; ++h) {
                    const CharT* h1 = h, *n1 = needle;
                    while (*h1 && *n1 && (*h1 == *n1)) { ++h1; ++n1; }
                    if (!*n1) return h;
                }
                return nullptr;
            }

            // StrPBrk - Premier caractère appartenant à un ensemble
            template<typename CharT>
            static const CharT* StrPBrk(const CharT* str, const CharT* set) {
                for (const CharT* s = str; *s; ++s) 
                    for (const CharT* c = set; *c; ++c) 
                        if (*s == *c) return s;
                return nullptr;
            }

            // StrSpn - Longueur du préfixe valide
            template<typename CharT>
            static usize StrSpn(const CharT* str, const CharT* set) {
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

            // StrCspn - Longueur du préfixe sans caractères int32erdits
            template<typename CharT>
            static usize StrCspn(const CharT* str, const CharT* set) {
                usize len = 0;
                while (str[len]) {
                    for (const CharT* c = set; *c; ++c)
                        if (str[len] == *c) return len;
                    ++len;
                }
                return len;
            }

            // MemCpy - Copie mémoire brute
            template<typename CharT>
            static CharT* MemCpy(CharT* dest, const CharT* src, usize count) {
                for (usize i = 0; i < count; ++i) dest[i] = src[i];
                return dest;
            }

            // MemMove - Copie mémoire sécurisée
            template<typename CharT>
            static CharT* MemMove(CharT* dest, const CharT* src, usize count) {
                if (dest == src) return dest;
                if (dest < src) {
                    for (usize i = 0; i < count; ++i) dest[i] = src[i];
                } else {
                    for (usize i = count; i > 0; --i) dest[i-1] = src[i-1];
                }
                return dest;
            }

            // MemCmp - Comparaison mémoire
            template<typename CharT>
            static int32 MemCmp(const CharT* lhs, const CharT* rhs, usize count) {
                for (usize i = 0; i < count; ++i) {
                    if (lhs[i] != rhs[i])
                        return static_cast<int32>(lhs[i]) - static_cast<int32>(rhs[i]);
                }
                return 0;
            }

            // MemSet - Remplissage mémoire
            template<typename CharT>
            static CharT* MemSet(CharT* dest, CharT value, usize count) {
                for (usize i = 0; i < count; ++i) dest[i] = value;
                return dest;
            }

            // StrNLen - Longueur limitée à max_len
            template<typename CharT>
            static usize StrNLen(const CharT* str, usize max_len) {
                usize len = 0;
                while (len < max_len && str[len] != CharT(0)) ++len;
                return len;
            }

            // StrTok - Découpeur non thread-safe
            template<typename CharT>
            static CharT* StrTok(CharT* str, const CharT* delim) {
                static CharT* context = nullptr;
                return StrTokInternal(str, delim, &context);
            }

            // StrTokR - Version réentrante (thread-safe)
            template<typename CharT>
            static CharT* StrTokR(CharT* str, const CharT* delim, CharT** context) {
                return StrTokInternal(str, delim, context);
            }

            // StrDup - Duplication de chaîne
            template<typename CharT>
            static CharT* StrDup(const CharT* src) {
                if (!src) return nullptr;
                usize len = StrLen(src);
                CharT* dest = new CharT[len + 1];
                MemCpy(dest, src, len);
                dest[len] = CharT(0);
                return dest;
            }

            // StrNDup - Duplication limitée à n caractères
            template<typename CharT>
            static CharT* StrNDup(const CharT* src, usize n) {
                usize len = StrNLen(src, n);
                CharT* dest = new CharT[len + 1];
                MemCpy(dest, src, len);
                dest[len] = CharT(0);
                return dest;
            }

            template<typename CharT>
            static int64 AtoI(const CharT* str) {
                int64 result = 0;
                int sign = 1;
                
                while (IsSpace(*str)) ++str;
                
                if (*str == '-') { sign = -1; ++str; }
                else if (*str == '+') ++str;
                
                while (IsDigit(*str)) {
                    result = result * 10 + (*str - '0');
                    ++str;
                }
                
                return sign * result;
            }

            template<typename CharT>
            static double AtoF(const CharT* str) {
                return StrToD(str, nullptr);
            }

            template<typename CharT>
            static int64 StrToL(const CharT* str, CharT** endptr, int base) {
                // Validation base
                if (base < 2 || base > 36) return 0;
                
                int64 result = 0;
                int sign = 1;
                
                while (IsSpace(*str)) ++str;
                
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
            static double StrToD(const CharT* str, CharT** endptr) {
                double result = 0.0;
                int sign = 1;
                double fraction = 0.1;
                int exponent = 0;
                
                while (IsSpace(*str)) ++str;
                
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
            static void StrReverse(CharT* str) {
                usize len = StrLen(str);
                for (usize i = 0; i < len/2; ++i) {
                    Swap(str[i], str[len - i - 1]);
                }
            }

            template<typename CharT>
            static void StrToLower(CharT* str) {
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
            static void StrTrim(CharT* str) {
                usize start = 0;
                usize end = StrLen(str);
                
                while (IsSpace(str[start])) ++start;
                while (end > start && IsSpace(str[end-1])) --end;
                
                MemMove(str, str + start, end - start);
                str[end - start] = CharT(0);
            }

            template<typename CharT>
            static void StrReplace(CharT* str, CharT oldChar, CharT newChar) {
                for (; *str; ++str) {
                    if (*str == oldChar) *str = newChar;
                }
            }

            template<typename CharT>
            static void StrReplaceSubstr(CharT* str, const CharT* oldSub, const CharT* newSub) {
                usize oldLen = StrLen(oldSub);
                usize newLen = StrLen(newSub);
                CharT* pos;
                
                while (pos = StrStr(str, oldSub)) {
                    MemMove(pos + newLen, pos + oldLen, StrLen(pos + oldLen) + 1);
                    MemCpy(pos, newSub, newLen);
                }
            }

            template<typename CharT>
            static CharT** Split(const CharT* str, CharT delim, usize* count) {
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
                        StrNCpy(result[idx], start, len);
                        result[idx++][len] = CharT(0);
                        start = str + 1;
                    }
                    ++str;
                }
                
                // Dernier token
                usize len = str - start;
                result[idx] = new CharT[len + 1];
                StrNCpy(result[idx], start, len);
                result[idx][len] = CharT(0);
                
                *count = tokens;
                return result;
            }

            template<typename CharT>
            static CharT* Join(const CharT** parts, usize count, CharT delim) {
                usize total_len = 0;
                for (usize i = 0; i < count; ++i) 
                    total_len += StrLen(parts[i]) + 1;
                
                CharT* result = new CharT[total_len];
                CharT* ptr = result;
                
                for (usize i = 0; i < count; ++i) {
                    usize len = StrLen(parts[i]);
                    StrCpy(ptr, parts[i]);
                    ptr += len;
                    if (i != count - 1) *ptr++ = delim;
                }
                
                *ptr = CharT(0);
                return result;
            }

            template<typename CharT>
            static void FreeSplitResult(CharT** result, usize count) {
                for (usize i = 0; i < count; ++i) delete[] result[i];
                delete[] result;
            }

            template<typename CharT>
            static void FreeJoinResult(CharT* result) {
                delete[] result;
            }

            /* CLASSIFICATION DE CARACTÈRES */

            template<typename CharT>
            static bool IsSpace(CharT c) {
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
            static uint64 StrToUL(const CharT* str, CharT** endptr, int base) {
                if (base < 0 || base > 36) {
                    if (endptr) *endptr = const_cast<CharT*>(str);
                    return 0;
                }

                uint64 result = 0;
                const CharT* original = str;

                while (IsSpace(*str)) ++str;

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
            static float32 StrToF(const CharT* str, CharT** endptr) {
                return static_cast<float32>(StrToD(str, endptr));
            }

            template<typename CharT>
            static float80 StrToLD(const CharT* str, CharT** endptr) {
                float80 result = 0.0L;
                int sign = 1;
                float80 fraction = 0.1L;
                int exponent = 0;

                while (IsSpace(*str)) ++str;

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
                return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
            }

            template<typename CharT>
            static bool IsSpace(CharT c) {
                return c == ' ' || (c >= '\t' && c <= '\r');
            }

            template<typename CharT>
            static bool IsUpper(CharT c) {
                return c >= 'A' && c <= 'Z';
            }

            template<typename CharT>
            static bool IsLower(CharT c) {
                return c >= 'a' && c <= 'z';
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
                static_assert(std::is_integral_v<T>, "FastSwap only for integral types");
                if (&a != &b) {
                    a ^= b;
                    b ^= a;
                    a ^= b;
                }
            }
    };
} // namespace nkentseu  // Namespace for the nkentseu library
