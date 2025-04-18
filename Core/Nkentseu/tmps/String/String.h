#pragma once

#include "Nkentseu/Platform/Export.h"
#include "Nkentseu/Platform/Types.h"
#include "BasicString.h"

namespace nkentseu
{
    
    // =============================================
    // Type Aliases
    // =============================================
    using String = BasicString<char8>;
    using String8 = BasicString<char8_t>;
    using String16 = BasicString<char16>;
    using String32 = BasicString<char32>;
    using WString = BasicString<wchar>;


    std::ostream& operator<<(std::ostream& os, const String& str);

    // =============================================
    // Template Specializations
    // =============================================
    /* Conversion specializations here */

    // Spécialisations externes
    template<>
    template<>
    NKENTSEU_INLINE_API BasicString<char8> BasicString<char16>::ConvertTo<char8>() const {
        BasicString<char8> result;
        UnicodeConvert<char8>(Data(), Length(), result);
        return result;
    }

    template<>
    template<>
    NKENTSEU_INLINE_API BasicString<char32> BasicString<char16>::ConvertTo<char32>() const {
        BasicString<char32> result;
        UnicodeConvert<char32>(Data(), Length(), result);
        return result;
    }

    template<>
    template<>
    NKENTSEU_INLINE_API BasicString<wchar> BasicString<char16>::ConvertTo<wchar>() const {
        static_assert(sizeof(wchar) >= 2, "wchar_t must be at least 16-bit");
        BasicString<wchar> result;
        UnicodeConvert<wchar>(Data(), Length(), result);
        return result;
    }

    template<>
    template<>
    NKENTSEU_INLINE_API BasicString<char8> BasicString<char32>::ConvertTo<char8>() const {
        BasicString<char8> result;
        UnicodeConvert<char8>(Data(), Length(), result);
        return result;
    }

    template<>
    template<>
    NKENTSEU_INLINE_API BasicString<char16> BasicString<char8>::ConvertTo<char16>() const {
        BasicString<char16> result;
        UnicodeConvert<char16>(Data(), Length(), result);
        return result;
    }

    // Spécialisations supplémentaires pour les conversions manquantes

    // Conversion char8 -> char32
    template<>
    template<>
    NKENTSEU_INLINE_API BasicString<char32> BasicString<char8>::ConvertTo<char32>() const {
        BasicString<char32> result;
        UnicodeConvert<char32>(Data(), Length(), result);
        return result;
    }

    // Conversion char8 -> wchar
    template<>
    template<>
    NKENTSEU_INLINE_API BasicString<wchar> BasicString<char8>::ConvertTo<wchar>() const {
        BasicString<wchar> result;
        UnicodeConvert<wchar>(Data(), Length(), result);
        return result;
    }

    // Conversion char32 -> char16
    template<>
    template<>
    NKENTSEU_INLINE_API BasicString<char16> BasicString<char32>::ConvertTo<char16>() const {
        BasicString<char16> result;
        UnicodeConvert<char16>(Data(), Length(), result);
        return result;
    }

    // Conversion char32 -> wchar
    template<>
    template<>
    NKENTSEU_INLINE_API BasicString<wchar> BasicString<char32>::ConvertTo<wchar>() const {
        static_assert(sizeof(wchar) >= 2, "wchar_t must be at least 16-bit");
        BasicString<wchar> result;
        UnicodeConvert<wchar>(Data(), Length(), result);
        return result;
    }

    // Conversion wchar -> char8
    template<>
    template<>
    NKENTSEU_INLINE_API BasicString<char8> BasicString<wchar>::ConvertTo<char8>() const {
        BasicString<char8> result;
        UnicodeConvert<char8>(Data(), Length(), result);
        return result;
    }

    // Conversion wchar -> char16
    template<>
    template<>
    NKENTSEU_INLINE_API BasicString<char16> BasicString<wchar>::ConvertTo<char16>() const {
        static_assert(sizeof(wchar) >= 2, "wchar must be at least 16-bit");
        BasicString<char16> result;
        UnicodeConvert<char16>(Data(), Length(), result);
        return result;
    }

    // Conversion wchar -> char32
    template<>
    template<>
    NKENTSEU_INLINE_API BasicString<char32> BasicString<wchar>::ConvertTo<char32>() const {
        BasicString<char32> result;
        UnicodeConvert<char32>(Data(), Length(), result);
        return result;
    }

    // =============================================
    // Literals
    // =============================================
    namespace literals {
        // Après (ajouter inline)
        NKENTSEU_FORCE_INLINE String operator""_s(const char8* str, size_t len) { 
            return String(str, len); 
        }
        
        NKENTSEU_FORCE_INLINE String16 operator""_s16(const char16* str, size_t len) { 
            return String16(str, len); 
        }
        
        NKENTSEU_FORCE_INLINE String32 operator""_s32(const char32* str, size_t len) { 
            return String32(str, len); 
        }
        
        NKENTSEU_FORCE_INLINE WString operator""_ws(const wchar* str, size_t len) { 
            return WString(str, len); 
        }
    }
} // namespace nkentseu
