#include "pch.h"
#include "String.h"
#include <cwchar>

namespace nkentseu {

    // thread_local static char16* conversionBuffer = nullptr;
    // thread_local static char32* conversionBuffer32 = nullptr;
    // thread_local static wchar* conversionBufferW = nullptr;
    // thread_local static char8* conversionBuffer8 = nullptr;

    namespace detail {
        // UTF-8 decoding/encoding
        uint32 Utf8Decode(const char8*& s) {
            uint32 cp = 0;
            uint8 c = *s++;
            
            if((c & 0x80) == 0) {
                return c;
            }
            else if((c & 0xE0) == 0xC0) {
                cp = (c & 0x1F) << 6;
                cp |= (*s++ & 0x3F);
            }
            else if((c & 0xF0) == 0xE0) {
                cp = (c & 0x0F) << 12;
                cp |= (*s++ & 0x3F) << 6;
                cp |= (*s++ & 0x3F);
            }
            else if((c & 0xF8) == 0xF0) {
                cp = (c & 0x07) << 18;
                cp |= (*s++ & 0x3F) << 12;
                cp |= (*s++ & 0x3F) << 6;
                cp |= (*s++ & 0x3F);
            }
            return cp;
        }

        void Utf8Encode(char8*& dest, uint32 cp) {
            if(cp <= 0x7F) {
                *dest++ = static_cast<char8>(cp);
            }
            else if(cp <= 0x7FF) {
                *dest++ = 0xC0 | (cp >> 6);
                *dest++ = 0x80 | (cp & 0x3F);
            }
            else if(cp <= 0xFFFF) {
                *dest++ = 0xE0 | (cp >> 12);
                *dest++ = 0x80 | ((cp >> 6) & 0x3F);
                *dest++ = 0x80 | (cp & 0x3F);
            }
            else {
                *dest++ = 0xF0 | (cp >> 18);
                *dest++ = 0x80 | ((cp >> 12) & 0x3F);
                *dest++ = 0x80 | ((cp >> 6) & 0x3F);
                *dest++ = 0x80 | (cp & 0x3F);
            }
        }

        // UTF-16 helpers
        void Utf16Encode(char16*& dest, uint32 cp) {
            if(cp <= 0xFFFF) {
                *dest++ = static_cast<char16>(cp);
            } else {
                cp -= 0x10000;
                *dest++ = static_cast<char16>(0xD800 | (cp >> 10));
                *dest++ = static_cast<char16>(0xDC00 | (cp & 0x3FF));
            }
        }

        uint32 Utf16Decode(const char16*& s) {
            uint32 cp = *s++;
            if(cp >= 0xD800 && cp <= 0xDBFF) {
                uint32 high = cp - 0xD800;
                uint32 low = *s++ - 0xDC00;
                cp = (high << 10) + low + 0x10000;
            }
            return cp;
        }

        // Conversion générique
        template<typename From, typename To>
        void ConvertBuffer(const From* src, usize len, To* dest) {
            for(usize i = 0; i < len; i++) {
                dest[i] = static_cast<To>(src[i]);
            }
        }

        // Conversion spécifique pour wchar
        void WideConvert(const char* src, usize len, wchar* dest) {
            if(sizeof(wchar) == 2) {
                const char16* src16 = reinterpret_cast<const char16*>(src);
                ConvertBuffer(src16, len, dest);
            } else {
                ConvertBuffer(src, len, dest);
            }
        }
    } // namespace detail

    // Implementations des méthodes de conversion
    StringChar16 String::ToUTF16() {
        const usize bufferSize = m_length * 2;
        char16* buffer = m_allocator.AllocateArray<char16>(bufferSize);
        
        const char8* src = m_data;
        char16* dest = buffer;
        
        while(src < m_data + m_length) {
            uint32 cp = detail::Utf8Decode(src);
            detail::Utf16Encode(dest, cp);
        }
        
        StringChar16 result(buffer, dest - buffer);
        m_allocator.DeallocateArray(buffer);
        return result;
    }

    StringChar32 String::ToUTF32() {
        const usize bufferSize = m_length;
        char32* buffer = m_allocator.AllocateArray<char32>(bufferSize);
        
        const char8* src = m_data;
        char32* dest = buffer;
        
        while(src < m_data + m_length) {
            *dest++ = detail::Utf8Decode(src);
        }
        
        StringChar32 result(buffer, dest - buffer);
        m_allocator.DeallocateArray(buffer);
        return result;
    }

    StringWide String::ToWide() {
        const usize bufferSize = m_length * 2;
        wchar* buffer = m_allocator.AllocateArray<wchar>(bufferSize);
        
        if constexpr (sizeof(wchar) == 2) {
            const auto utf16 = ToUTF16();
            detail::ConvertBuffer(utf16.Data(), utf16.Length(), buffer);
        } else {
            const auto utf32 = ToUTF32();
            detail::ConvertBuffer(utf32.Data(), utf32.Length(), buffer);
        }
        
        StringWide result(buffer, wcslen(buffer));
        m_allocator.DeallocateArray(buffer);
        return result;
    }

    String StringChar16::ToUTF8() {
        const usize bufferSize = m_length * 4;
        char8* buffer = m_allocator.AllocateArray<char8>(bufferSize);
    
        const char16* src = m_data;
        char8* dest = buffer;
    
        while (src < m_data + m_length) {
            uint32 cp = detail::Utf16Decode(src);
            detail::Utf8Encode(dest, cp);
        }
    
        String result(buffer, dest - buffer);
        m_allocator.DeallocateArray(buffer);
        return result;
    }
    
    StringChar32 StringChar16::ToUTF32() {
        const usize bufferSize = m_length * 2;
        char32* buffer = m_allocator.AllocateArray<char32>(bufferSize);
    
        const char16* src = m_data;
        char32* dest = buffer;
    
        while (src < m_data + m_length) {
            *dest++ = detail::Utf16Decode(src);
        }
    
        StringChar32 result(buffer, dest - buffer);
        m_allocator.DeallocateArray(buffer);
        return result;
    }
    
    StringWide StringChar16::ToWide() {
        const usize bufferSize = m_length * 2;
        wchar* buffer = m_allocator.AllocateArray<wchar>(bufferSize);
    
        if constexpr (sizeof(wchar) == 2) {
            detail::ConvertBuffer(m_data, m_length, buffer);
        } else {
            const auto utf32 = ToUTF32();
            detail::ConvertBuffer(utf32.Data(), utf32.Length(), buffer);
        }
    
        StringWide result(buffer, wcslen(buffer));
        m_allocator.DeallocateArray(buffer);
        return result;
    }

    String StringChar32::ToUTF8() {
        const usize bufferSize = m_length * 4;
        char8* buffer = m_allocator.AllocateArray<char8>(bufferSize);
    
        const char32* src = m_data;
        char8* dest = buffer;
    
        for (usize i = 0; i < m_length; ++i) {
            detail::Utf8Encode(dest, src[i]);
        }
    
        String result(buffer, dest - buffer);
        m_allocator.DeallocateArray(buffer);
        return result;
    }
    
    StringChar16 StringChar32::ToUTF16() {
        const usize bufferSize = m_length * 2;
        char16* buffer = m_allocator.AllocateArray<char16>(bufferSize);
    
        const char32* src = m_data;
        char16* dest = buffer;
    
        for (usize i = 0; i < m_length; ++i) {
            detail::Utf16Encode(dest, src[i]);
        }
    
        StringChar16 result(buffer, dest - buffer);
        m_allocator.DeallocateArray(buffer);
        return result;
    }
    
    StringWide StringChar32::ToWide() {
        const usize bufferSize = m_length * 2;
        wchar* buffer = m_allocator.AllocateArray<wchar>(bufferSize);
    
        detail::ConvertBuffer(m_data, m_length, buffer);
        StringWide result(buffer, m_length);
    
        m_allocator.DeallocateArray(buffer);
        return result;
    }

    String StringWide::ToUTF8() {
        if (sizeof(wchar) == 2) {
            return StringChar16(static_cast<const char16*>(static_cast<const void*>(m_data)), m_length).ToUTF8();
        }
        return StringChar32(reinterpret_cast<const char32*>(m_data), m_length).ToUTF8();
    }
    
    StringChar16 StringWide::ToUTF16() {
        if (sizeof(wchar) == 2) {
            return StringChar16(static_cast<const char16*>(static_cast<const void*>(m_data)), m_length);
        }
        return StringChar32(reinterpret_cast<const char32*>(m_data), m_length).ToUTF16();
    }
    
    StringChar32 StringWide::ToUTF32() {
        if (sizeof(wchar) == 4) {
            return StringChar32(reinterpret_cast<const char32*>(m_data), m_length);
        }
        return StringChar16(static_cast<const char16*>(static_cast<const void*>(m_data)), m_length).ToUTF32();
    }
    

    // Opérateur de sortie multi-plateforme
    // std::ostream& StringWide::operator<<(std::ostream& os, const StringWide& str) {
    //     #if defined(NKENTSEU_PLATFORM_WINDOWS)
    //         return os << str.ToUTF8();
    //     #else
    //         std::wstring_convert<std::codecvt_utf8<wchar>> conv;
    //         return os << conv.to_bytes(str.Data());
    //     #endif
    // }

    // // Opérateur d'entrée multi-plateforme
    // std::istream& StringWide::operator>>(std::istream& is, StringWide& str) {
    //     std::string tmp;
    //     is >> tmp;
    //     #if defined(NKENTSEU_PLATFORM_WINDOWS)
    //         str = String(tmp).ToWide();
    //     #else
    //         std::wstring_convert<std::codecvt_utf8<wchar>> conv;
    //         str = StringWide(conv.from_bytes(tmp).c_str());
    //     #endif
    //     return is;
    // }

} // namespace nkentseu