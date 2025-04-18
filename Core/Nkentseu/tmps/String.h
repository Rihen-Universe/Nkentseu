#pragma once
#include "StringBase.h"
#include "StringAllocator.h"

#include <iostream>
#include <sstream>
#include <cstring> // Pour memcpy

namespace nkentseu {

    class StringChar16;
    class StringChar32;
    class StringWide;

    class String : public StringBase<char8, StringAllocator> {
    public:
        using Base = StringBase<char8, StringAllocator>;
        using Base::Base;
        using Base::m_data;
        using Base::m_length;
        using Base::m_allocator;

        String() = default;

        // Constructeurs
        String(const char8* str) : Base(str) {}
        String(const char8* str, usize len) : Base(str, len) {}
        //String(const std::string& str) : Base(reinterpret_cast<const char8*>(str.data()), str.size()) {}

        String(const String& other) = default;
        String(String&& other) noexcept = default;

        // Opérateurs d'affectation
        String& operator=(const String& other) = default;
        String& operator=(String&& other) noexcept = default;

        String& operator=(const char8* str) {
            String other(str);
            if (this != &other) {
                String temp(other);
                Swap(temp);
            }
            return *this;
        }

        String& operator=(const std::string& str) {
            String other(str.data(), str.size());
            if (this != &other) {
                String temp(other);
                Swap(temp);
            }
            return *this;
        }

        int Compare(const String& other) const {
            usize minLen = std::min(m_length, other.m_length);
            for (usize i = 0; i < minLen; ++i) {
                if (m_data[i] != other.m_data[i])
                    return static_cast<int>(m_data[i]) - static_cast<int>(other.m_data[i]);
            }
            return static_cast<int>(m_length) - static_cast<int>(other.m_length);
        }        

        // Comparaison
        bool operator==(const String& rhs) const { return StringBase::operator==(rhs); }
        bool operator!=(const String& rhs) const { return !(*this == rhs); }
        bool operator<(const String& rhs)  const { return Compare(rhs) < 0; }
        bool operator<=(const String& rhs) const { return Compare(rhs) <= 0; }
        bool operator>(const String& rhs)  const { return Compare(rhs) > 0; }
        bool operator>=(const String& rhs) const { return Compare(rhs) >= 0; }

        StringChar16 ToUTF16();
        StringChar32 ToUTF32();
        StringWide ToWide();

        String& operator+=(const String& rhs) {
            StringBase::operator+=(rhs);
            return *this;
        }

        // friend std::ostream& operator<<(std::ostream& os, const String& str) {
        //     for (usize i = 0; i < str.Length(); ++i) {
        //         os << static_cast<char>(str[i]); // attention : conversion explicite
        //     }
        //     return os;
        // }

        // // Flux d’entrée
        // friend std::istream& operator>>(std::istream& is, String& str) {
        //     std::basic_string<CharType> temp;
        //     std::getline(is, temp); // lecture jusqu’à fin de ligne
        //     str = String(temp.c_str(), temp.length());
        //     return is;
        // }
    
        friend std::string ToString(const String& str) {
            return std::string(str.m_data, str.m_length);
        }
    };

    class StringChar16 : public StringBase<char16, StringAllocator> {
    public:
        using Base = StringBase<char16, StringAllocator>;
        using Base::Base;
        using Base::m_data;
        using Base::m_length;
        using Base::m_allocator;

        StringChar16() = default;

        explicit StringChar16(const char16* str) : Base(str) {}
        StringChar16(const char16* str, usize len) : Base(str, len) {}
        StringChar16(const std::u16string& str) : Base(str.data(), str.size()) {}

        StringChar16(const StringChar16& other) = default;
        StringChar16(StringChar16&& other) noexcept = default;

        StringChar16& operator=(const StringChar16& other) = default;
        StringChar16& operator=(StringChar16&& other) noexcept = default;

        StringChar16& operator=(const char16* str) {
            StringChar16 other(str);
            if (this != &other) {
                StringChar16 temp(other);
                Swap(temp);
            }
            return *this;
        }

        StringChar16& operator=(const std::u16string& str) {
            StringChar16 other(str.data(), str.size());
            if (this != &other) {
                StringChar16 temp(other);
                Swap(temp);
            }
            return *this;
        }

        bool operator==(const StringChar16& rhs) const { return Compare(rhs) == 0; }
        bool operator!=(const StringChar16& rhs) const { return !(*this == rhs); }
        // etc.
        bool operator<(const StringChar16& rhs)  const { return Compare(rhs) < 0; }
        bool operator<=(const StringChar16& rhs) const { return Compare(rhs) <= 0; }
        bool operator>(const StringChar16& rhs)  const { return Compare(rhs) > 0; }
        bool operator>=(const StringChar16& rhs) const { return Compare(rhs) >= 0; }

        String ToUTF8();
        StringChar32 ToUTF32();
        StringWide ToWide();

        int Compare(const StringChar16& other) const {
            usize minLen = std::min(m_length, other.m_length);
            for (usize i = 0; i < minLen; ++i) {
                if (m_data[i] != other.m_data[i])
                    return static_cast<int>(m_data[i]) - static_cast<int>(other.m_data[i]);
            }
            return static_cast<int>(m_length) - static_cast<int>(other.m_length);
        }        

        StringChar16& operator+=(const StringChar16& rhs) {
            StringBase::operator+=(rhs);
            return *this;
        }

        // Opérateur de sortie avec conversion UTF-8
        // friend std::ostream& operator<<(std::ostream& os, const StringChar16& str) {
        //     return os << str.ToUTF8();
        // }

        // // Opérateur d'entrée avec conversion depuis UTF-8
        // friend std::istream& operator>>(std::istream& is, StringChar16& str) {
        //     String utf8Str;
        //     is >> utf8Str;
        //     str = utf8Str.ToUTF16();
        //     return is;
        // }
    
        friend std::string ToString(const StringChar16& str) {
            return ToString(StringChar16(str).ToUTF8());
        }
    };

    class StringChar32 : public StringBase<char32, StringAllocator> {
    public:
        using Base = StringBase<char32, StringAllocator>;
        using Base::Base;
        using Base::m_data;
        using Base::m_length;
        using Base::m_allocator;

        StringChar32() = default;

        explicit StringChar32(const char32* str) : Base(str) {}
        StringChar32(const char32* str, usize len) : Base(str, len) {}
        StringChar32(const std::u32string& str) : Base(str.data(), str.size()) {}

        StringChar32(const StringChar32& other) = default;
        StringChar32(StringChar32&& other) noexcept = default;

        StringChar32& operator=(const StringChar32& other) = default;
        StringChar32& operator=(StringChar32&& other) noexcept = default;

        StringChar32& operator=(const char32* str) {
            StringChar32 other(str);
            if (this != &other) {
                StringChar32 temp(other);
                Swap(temp);
            }
            return *this;
        }

        StringChar32& operator=(const std::u32string& str) {
            StringChar32 other(str.data(), str.size());
            if (this != &other) {
                StringChar32 temp(other);
                Swap(temp);
            }
            return *this;
        }

        bool operator==(const StringChar32& rhs) const { return Compare(rhs) == 0; }
        // etc.
        bool operator!=(const StringChar32& rhs) const { return !(*this == rhs); }
        bool operator<(const StringChar32& rhs)  const { return Compare(rhs) < 0; }
        bool operator<=(const StringChar32& rhs) const { return Compare(rhs) <= 0; }
        bool operator>(const StringChar32& rhs)  const { return Compare(rhs) > 0; }

        String ToUTF8();
        StringChar16 ToUTF16();
        StringWide ToWide();

        int Compare(const StringChar32& other) const {
            usize minLen = std::min(m_length, other.m_length);
            for (usize i = 0; i < minLen; ++i) {
                if (m_data[i] != other.m_data[i])
                    return static_cast<int>(m_data[i]) - static_cast<int>(other.m_data[i]);
            }
            return static_cast<int>(m_length) - static_cast<int>(other.m_length);
        }        

        // Opérateur de sortie avec conversion UTF-8
        // friend std::ostream& operator<<(std::ostream& os, const StringChar32& str) {
        //     return os << str.ToUTF8();
        // }

        // // Opérateur d'entrée avec conversion depuis UTF-8
        // friend std::istream& operator>>(std::istream& is, StringChar32& str) {
        //     String utf8Str;
        //     is >> utf8Str;
        //     str = utf8Str.ToUTF32();
        //     return is;
        // }
    };

    class StringWide : public StringBase<wchar, StringAllocator> {
    public:
        using Base = StringBase<wchar, StringAllocator>;
        using Base::Base;
        using Base::m_data;
        using Base::m_length;
        using Base::m_allocator;

        StringWide() = default;

        explicit StringWide(const wchar* str) : Base(str) {}
        StringWide(const wchar* str, usize len) : Base(str, len) {}
        StringWide(const std::wstring& str) : Base(str.data(), str.size()) {}

        StringWide(const StringWide& other) = default;
        StringWide(StringWide&& other) noexcept = default;

        StringWide& operator=(const StringWide& other) = default;
        StringWide& operator=(StringWide&& other) noexcept = default;

        StringWide& operator=(const wchar* str) {
            StringWide other(str);
            if (this != &other) {
                StringWide temp(other);
                Swap(temp);
            }
            return *this;
        }

        StringWide& operator=(const std::wstring& str) {
            StringWide other(str.data(), str.size());
            if (this != &other) {
                StringWide temp(other);
                Swap(temp);
            }
            return *this;
        }

        bool operator==(const StringWide& rhs) const { return Compare(rhs) == 0; }
        bool operator!=(const StringWide& rhs) const { return !(*this == rhs); }
        bool operator<(const StringWide& rhs)  const { return Compare(rhs) < 0; }
        bool operator<=(const StringWide& rhs) const { return Compare(rhs) <= 0; }
        bool operator>(const StringWide& rhs)  const { return Compare(rhs) > 0; }
        bool operator>=(const StringWide& rhs) const { return Compare(rhs) >= 0; }
        // etc.


        String ToUTF8();
        StringChar16 ToUTF16();
        StringChar32 ToUTF32();

        int Compare(const StringWide& other) const {
            usize minLen = std::min(m_length, other.m_length);
            for (usize i = 0; i < minLen; ++i) {
                if (m_data[i] != other.m_data[i])
                    return static_cast<int>(m_data[i]) - static_cast<int>(other.m_data[i]);
            }
            return static_cast<int>(m_length) - static_cast<int>(other.m_length);
        }        

        // // Opérateur de sortie multi-plateforme
        // friend std::ostream& operator<<(std::ostream& os, const StringWide& str);

        // // Opérateur d'entrée multi-plateforme
        // friend std::istream& operator>>(std::istream& is, StringWide& str);
    };

    namespace literals {
        NKENTSEU_FORCE_INLINE String operator""_u8(const char* str, usize length) {
            return String(str, length);
        }
        
        NKENTSEU_FORCE_INLINE StringChar16 operator""_u16(const char16_t* str, usize length) {
            return StringChar16(str, length);
        }
        
        NKENTSEU_FORCE_INLINE StringChar32 operator""_u32(const char32_t* str, usize length) {
            return StringChar32(str, length);
        }
        
        NKENTSEU_FORCE_INLINE StringWide operator""_w(const wchar* str, usize length) {
            return StringWide(str, length);
        }
    }

} // namespace nkentseu