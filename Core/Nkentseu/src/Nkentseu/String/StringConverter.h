#pragma once

#include "Nkentseu/Platform/Types.h"
#include "Nkentseu/Platform/Inline.h"
#include "Nkentseu/Platform/Assertion.h"
#include "Nkentseu/Platform/Export.h"

#include <iostream>
#include <vector>

namespace nkentseu
{

    template<typename CharT>
    struct NKENTSEU_TEMPLATE_API StringConverter {
    public:
        template<typename IntegerT>
        static CharT* IntToStr(IntegerT value, CharT* buffer) {
            if(IsSigned_v<IntegerT> == false) {
                std::cout << "Error: IntToStr requires a signed integer type" << std::endl;
                NKENTSEU_DEBUG_BREAK();
            }
            
            bool negative = value < 0;
            usize i = 0;

            using UnsignedT = typename MakeUnsigned<IntegerT>::type;
            UnsignedT abs_value;

            if (negative) {
                if (value == std::numeric_limits<IntegerT>::min()) {
                    abs_value = static_cast<UnsignedT>(std::numeric_limits<IntegerT>::max()) + 1;
                } else {
                    abs_value = static_cast<UnsignedT>(-value);
                }
            } else {
                abs_value = static_cast<UnsignedT>(value);
            }

            do {
                buffer[i++] = '0' + (abs_value % 10);
                abs_value /= 10;
            } while (abs_value > 0);

            if (negative) buffer[i++] = '-';

            buffer[i] = '\0';

            for (usize j = 0; j < i / 2; ++j) {
                auto tmp = buffer[j];
                buffer[j] = buffer[i - j - 1];
                buffer[i - j - 1] = tmp;
            }

            return buffer;
        }

        // Conversion pour entiers non signés
        template<typename IntegerT>
        static CharT* UIntToStr(IntegerT value, CharT* buffer) {
            if(IsUnsigned<IntegerT>::value == false) {
                std::cout << "Error: Type must be unsigned" << std::endl;
                NKENTSEU_DEBUG_BREAK();
            }
            
            usize i = 0;
            do {
                buffer[i++] = '0' + (value % 10);
                value /= 10;
            } while (value > 0);
            
            buffer[i] = '\0';
            
            // Inversion manuelle
            for (usize j = 0; j < i / 2; ++j) {
                auto tmp = buffer[j];
                buffer[j] = buffer[i - j - 1];
                buffer[i - j - 1] = tmp;
            }
            
            return buffer;
        }

        template<typename FloatT, usize N>
        static void FormatFloat(FloatT value, CharT (&buffer)[N], usize precision) {
            // Gestion des valeurs spéciales
            if (value != value) { // NaN
                buffer[0] = 'N'; buffer[1] = 'a'; buffer[2] = 'N'; buffer[3] = '\0';
                return;
            }

            if (value == std::numeric_limits<FloatT>::infinity()) {
                buffer[0] = 'i'; buffer[1] = 'n'; buffer[2] = 'f'; buffer[3] = '\0';
                return;
            }

            if (value == -std::numeric_limits<FloatT>::infinity()) {
                buffer[0] = '-'; buffer[1] = 'i'; buffer[2] = 'n'; buffer[3] = 'f'; buffer[4] = '\0';
                return;
            }

            // Gestion du signe
            bool negative = value < 0;
            if (negative) value = -value;

            // Séparation partie entière/fractionnelle
            int64 integer = static_cast<int64>(value);
            FloatT fraction = value - integer;

            // Conversion partie entière
            CharT int_buf[constants::INT_BUFFER_SIZE];
            IntToStr(integer, int_buf);

            // Calcul manuel de 10^Precision
            constexpr auto pow10 = [](usize exp) -> uint64 {
                FloatT result = 1;
                for (usize i = 0; i < exp; ++i) result *= 10;
                return result;
            };

            // Conversion partie fractionnelle avec contrôle de la précision
            FloatT scaling_factor = pow10(precision);
            uint64 frac_scaled = static_cast<uint64>(fraction * scaling_factor + FloatT(0.5));

            // Buffer pour la partie fractionnelle avec zéros initiaux
            const usize max_frac_buf_size = 32;
            CharT frac_buf[max_frac_buf_size];
            usize buf_size = precision < max_frac_buf_size ? precision : max_frac_buf_size - 1;

            // Remplissage avec des '0' et copie de la valeur convertie
            for (usize i = 0; i < buf_size; ++i) frac_buf[i] = '0';
            frac_buf[buf_size] = '\0';

            CharT temp_buf[32];
            UIntToStr(frac_scaled, temp_buf);
            usize temp_len = 0;
            while (temp_buf[temp_len] != '\0') ++temp_len;

            usize start_pos = buf_size - temp_len;
            for (usize i = 0; i < temp_len && (start_pos + i) < buf_size; ++i) {
                frac_buf[start_pos + i] = temp_buf[i];
            }

            // Suppression des zéros de fin
            usize trimmed_frac_len = buf_size;
            while (trimmed_frac_len > 0 && frac_buf[trimmed_frac_len - 1] == '0') {
                --trimmed_frac_len;
            }

            bool frac_all_zero = (trimmed_frac_len == 0);

            // Construction du résultat final
            auto len = [](const CharT* str) {
                const CharT* ptr = str;
                while (*ptr != CharT(0)) ++ptr;
                return ptr - str;
            };

            usize int_len = len(int_buf);
            usize pos = 0;

            if (negative) buffer[pos++] = '-';
            for (usize i = 0; i < int_len; ++i) buffer[pos++] = int_buf[i];
            
            if (!frac_all_zero) {
                buffer[pos++] = '.';
                for (usize i = 0; i < trimmed_frac_len; ++i) {
                    buffer[pos++] = frac_buf[i];
                }
            }

            buffer[pos] = '\0';
        }

        // Décodeurs
        template<typename SourceCharT>
        static void Decode(const SourceCharT* src, std::vector<uint32>& code_points) {
            if constexpr (IsSame_v<SourceCharT, charb>) {
                while(*src) code_points.push_back(static_cast<uint8>(*src++));
            }
            else if constexpr (IsSame_v<SourceCharT, char8>) {
                DecodeUTF8(src, code_points);
            }
            else if constexpr (IsSame_v<SourceCharT, char16>) {
                DecodeUTF16(src, code_points);
            }
            else if constexpr (IsSame_v<SourceCharT, char32>) {
                while(*src) code_points.push_back(*src++);
            }
            else if constexpr (IsSame_v<SourceCharT, wchar>) {
                if constexpr (sizeof(wchar) == 2)
                    DecodeUTF16(reinterpret_cast<const char16*>(src), code_points);
                else
                    while(*src) code_points.push_back(*src++);
            }
        }

        static void DecodeUTF8(const char8* src, std::vector<uint32>& code_points) {
            while(*src) {
                uint8 c = *src++;
                uint32 cp = c;

                if(c >= 0xF0) { // 4 octets
                    cp = (c & 0x07) << 18;
                    cp |= (*src++ & 0x3F) << 12;
                    cp |= (*src++ & 0x3F) << 6;
                    cp |= (*src++ & 0x3F);
                }
                else if(c >= 0xE0) { // 3 octets
                    cp = (c & 0x0F) << 12;
                    cp |= (*src++ & 0x3F) << 6;
                    cp |= (*src++ & 0x3F);
                }
                else if(c >= 0xC0) { // 2 octets
                    cp = (c & 0x1F) << 6;
                    cp |= (*src++ & 0x3F);
                }

                code_points.push_back(IsValidCodePoint(cp) ? cp : unicode::REPLACEMENT_CHAR);
            }
        }

        static void DecodeUTF16(const char16* src, std::vector<uint32>& code_points) {
            while(*src) {
                uint16 c = *src++;
                if((c & 0xFC00) == 0xD800) { // Surrogate pair
                    uint16 low = *src++;
                    code_points.push_back(((c & 0x3FF) << 10) + (low & 0x3FF) + 0x10000);
                } else {
                    code_points.push_back(c);
                }
            }
        }

        // Encodeurs
        static void EncodeToCharb(const std::vector<uint32>& code_points, charb* dest) {
            for(auto cp : code_points)
                *dest++ = (cp < 0x100) ? static_cast<charb>(cp) : '?';
            *dest = '\0';
        }

        static void EncodeToUTF8(const std::vector<uint32>& code_points, char8* dest) {
            for(auto cp : code_points) {
                if(cp <= 0x7F) {
                    *dest++ = cp;
                } else if(cp <= 0x7FF) {
                    *dest++ = 0xC0 | (cp >> 6);
                    *dest++ = 0x80 | (cp & 0x3F);
                } else if(cp <= 0xFFFF) {
                    *dest++ = 0xE0 | (cp >> 12);
                    *dest++ = 0x80 | ((cp >> 6) & 0x3F);
                    *dest++ = 0x80 | (cp & 0x3F);
                } else {
                    *dest++ = 0xF0 | (cp >> 18);
                    *dest++ = 0x80 | ((cp >> 12) & 0x3F);
                    *dest++ = 0x80 | ((cp >> 6) & 0x3F);
                    *dest++ = 0x80 | (cp & 0x3F);
                }
            }
            *dest = '\0';
        }

        static void EncodeToUTF16(const std::vector<uint32>& code_points, char16* dest) {
            for(auto cp : code_points) {
                if(cp <= 0xFFFF) {
                    *dest++ = cp;
                } else {
                    cp -= 0x10000;
                    *dest++ = 0xD800 | (cp >> 10);
                    *dest++ = 0xDC00 | (cp & 0x3FF);
                }
            }
            *dest = '\0';
        }

        static void EncodeToUTF32(const std::vector<uint32>& code_points, char32* dest) {
            for(auto cp : code_points) {
                *dest++ = IsValidCodePoint(cp) ? cp : unicode::REPLACEMENT_CHAR;
            }
            *dest = '\0';
        }

        #if defined(NKENTSEU_PLATFORM_WINDOWS)
        static void EncodeToWChar(const std::vector<uint32>& code_points, wchar* dest) {
            if constexpr (sizeof(wchar) == 2) {
                EncodeToUTF16(code_points, reinterpret_cast<char16*>(dest));
            } else {
                // Utilisation directe des code points pour wchar 32-bit
                for(auto cp : code_points) {
                    *dest++ = static_cast<wchar>(IsValidCodePoint(cp) ? cp : unicode::REPLACEMENT_CHAR);
                }
                *dest = L'\0';
            }
        }
        #endif
    public:

        template<typename U>
        static typename EnableIf<IsIntegral_v<U> && IsBooleanNatif_v<U>, const CharT*>::type Convert(U value) {
            return value ? ConversionTraits<CharT>::TrueStr : ConversionTraits<CharT>::FalseStr;
        }

        static const CharT* Convert(uint8 value) {
            static thread_local CharT buffer[constants::INT_BUFFER_SIZE];
            return UIntToStr(static_cast<uint32>(value), buffer);
        }

        template<typename U>
        static typename EnableIf<IsIntegral_v<U> && IsSigned_v<U> && !IsBooleanNatif_v<U>, const CharT*>::type Convert(U value) {
            static thread_local CharT buffer[constants::INT_BUFFER_SIZE];
            return IntToStr(value, buffer);
        }

        template<typename U>
        static typename EnableIf<IsIntegral_v<U> && IsUnsigned_v<U> && !IsBooleanNatif_v<U>, const CharT*>::type Convert(U value) {
            static thread_local CharT buffer[constants::INT_BUFFER_SIZE];
            return UIntToStr(value, buffer);
        }

        template<typename U>
        static typename EnableIf<IsFloatingPoint_v<U>, const CharT*>::type Convert(U value, usize precision = 15) {
            static thread_local CharT buffer[constants::FLOAT_BUFFER_SIZE];
            FormatFloat(static_cast<float80>(value), buffer, precision);
            return buffer;
        }

        // Conversions directes spécialisées
        static const charb* ToCharb(const CharT* src) { 
            static thread_local charb buffer[constants::STRING_BUFFER_SIZE];
            std::vector<uint32> code_points;

            // Décodage
            Decode(src, code_points);
            EncodeToCharb(code_points, buffer);

            return buffer; 
        }

        static const char8* ToChar8(const CharT* src) { 
            static thread_local char8 buffer[constants::STRING_BUFFER_SIZE];
            std::vector<uint32> code_points;

            // Décodage
            Decode(src, code_points);
            EncodeToUTF8(code_points, buffer);

            return buffer; 
        }

        static const char16* ToChar16(const CharT* src) { 
            static thread_local char16 buffer[constants::STRING_BUFFER_SIZE];
            std::vector<uint32> code_points;

            // Décodage
            Decode(src, code_points);
            EncodeToUTF16(code_points, buffer);

            return buffer; 
        }

        static const char32* ToChar32(const CharT* src) { 
            static thread_local char32 buffer[constants::STRING_BUFFER_SIZE];
            std::vector<uint32> code_points;

            // Décodage
            Decode(src, code_points);
            EncodeToUTF32(code_points, buffer);

            return buffer; 
        }

        #if defined(NKENTSEU_PLATFORM_WINDOWS)
        static const wchar* ToWchar(const CharT* src) { 
            static thread_local wchar buffer[constants::STRING_BUFFER_SIZE];
            std::vector<uint32> code_points;

            // Décodage
            Decode(src, code_points);
            EncodeToWChar(code_points, buffer);

            return buffer; 
        }
        #endif

        // Ajouter dans la classe StringConverter avant la section publique
        static void EncodeFromCodePoints(const std::vector<uint32>& code_points, CharT* dest) {
            // Dispatch selon le type de caractère cible
            if constexpr (IsSame_v<CharT, charb>) {
                EncodeToCharb(code_points, dest);
            } 
            else if constexpr (IsSame_v<CharT, char8>) {
                EncodeToUTF8(code_points, dest);
            }
            else if constexpr (IsSame_v<CharT, char16>) {
                EncodeToUTF16(code_points, dest);
            }
            else if constexpr (IsSame_v<CharT, char32>) {
                EncodeToUTF32(code_points, dest);
            }
            else if constexpr (IsSame_v<CharT, wchar>) {
                #if defined(NKENTSEU_PLATFORM_WINDOWS)
                EncodeToWChar(code_points, dest);
                #else
                // Pour les systèmes où wchar_t est 32-bit
                EncodeToUTF32(code_points, reinterpret_cast<char32*>(dest));
                #endif
            }
            else {
                static_assert(DependentFalse<CharT>, "Type de caractère non supporté");
            }
        }

        static void EncodeFromCodePoints(const uint32* codePoints, usize length, CharT* dest) {
            EncodeFromCodePoints(std::vector<uint32>(codePoints, codePoints + length), dest);
        }

        // Ajouter dans la section private de la classe
        template<typename T>
        static constexpr bool DependentFalse = false; // Pour static_assert

        static bool DecodeNext(const CharT*& src, const CharT* end, uint32& cp) {
            if constexpr (IsSame_v<CharT, char16>) {
                // Implémentation existante pour UTF-16
                if(src >= end) return false;
                uint16 c = *src++;
                if((c & 0xFC00) == 0xD800) {
                    if(src >= end || (*src & 0xFC00) != 0xDC00) {
                        cp = unicode::REPLACEMENT_CHAR;
                        return false;
                    }
                    uint16 low = *src++;
                    cp = ((c & 0x3FF) << 10) + (low & 0x3FF) + 0x10000;
                    return true;
                }
                cp = c;
                return true;
            }
            else if constexpr (IsSame_v<CharT, charb>) { // UTF-8
                if(src >= end) return false;
                
                uint8 b1 = *src++;
                if(b1 <= 0x7F) { // 1 byte
                    cp = b1;
                    return true;
                }
                
                if((b1 & 0xE0) == 0xC0) { // 2 bytes
                    if(src >= end) return false;
                    uint8 b2 = *src++;
                    if((b2 & 0xC0) != 0x80) return false;
                    cp = ((b1 & 0x1F) << 6) | (b2 & 0x3F);
                    return cp >= 0x80;
                }
                
                if((b1 & 0xF0) == 0xE0) { // 3 bytes
                    if(end - src < 2) return false;
                    uint8 b2 = *src++;
                    uint8 b3 = *src++;
                    if((b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80) return false;
                    cp = ((b1 & 0x0F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
                    return cp >= 0x800;
                }
                
                if((b1 & 0xF8) == 0xF0) { // 4 bytes
                    if(end - src < 3) return false;
                    uint8 b2 = *src++;
                    uint8 b3 = *src++;
                    uint8 b4 = *src++;
                    if((b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80 || (b4 & 0xC0) != 0x80) return false;
                    cp = ((b1 & 0x07) << 18) | ((b2 & 0x3F) << 12) | ((b3 & 0x3F) << 6) | (b4 & 0x3F);
                    return cp >= 0x10000 && cp <= 0x10FFFF;
                }
                
                return false;
            }
            else if constexpr (IsSame_v<CharT, char32>) {
                if(src >= end) return false;
                cp = *src++;
                return IsValidCodePoint(cp);
            }
            else if constexpr (IsSame_v<CharT, wchar>) {
                #if defined(NKENTSEU_PLATFORM_WINDOWS)
                    // Traiter comme UTF-16
                    return DecodeNext(reinterpret_cast<const char16*&>(src), reinterpret_cast<const char16*>(end), cp);
                #else
                    // Traiter comme UTF-32
                    if(src >= end) return false;
                    cp = *src++;
                    return IsValidCodePoint(cp);
                #endif
            }
            else {
                // static_assert(DependentFalse<CharT>, "Type de caractère non supporté");
                return false;
            }
        }

        static bool IsValidCodePoint(uint32 cp) {
            return cp <= 0x10FFFF && 
                  !(cp >= 0xD800 && cp <= 0xDFFF) && 
                  !(cp >= 0xFDD0 && cp <= 0xFDEF) && 
                  (cp & 0xFFFE) != 0xFFFE;
        }
    };

} // namespace nkentseu
