#pragma once
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <locale>
#include <cwctype>

#include "Nkentseu/Memory/Memory.h"
#include "Nkentseu/Platform/Assertion.h"
#include "Nkentseu/Platform/Platform.h"
#include "Nkentseu/Platform/Inline.h"

namespace nkentseu {

    // =============================================
    // Meta-programming utilities
    // =============================================
    template<bool B, typename T = void>
    struct EnableIf { static constexpr bool value = B; };

    template<typename T>
    struct EnableIf<true, T> {
        using type = T;
        static constexpr bool value = true;
    };

    template<typename T>
    struct IsClass { static constexpr bool value = __is_class(T); };

    // =============================================
    // Allocator
    // =============================================
    template<typename T>
    class NKENTSEU_TEMPLATE_API BasicStringAllocator {
    public:
        using ValueType = T;
        
        BasicStringAllocator() = default;
        
        template<typename U>
        struct Rebind { using Other = BasicStringAllocator<U>; };

        T* Allocate(size_t n) {
            return static_cast<T*>(::operator new(n * sizeof(T)));
        }

        void Deallocate(T* p, size_t n) noexcept {
            if (!p) return;

            if constexpr (std::is_class_v<T>) {
                for (size_t i = 0; i < n; ++i)
                    p[i].~T();
            }
            ::operator delete(static_cast<void*>(p));
        }

        bool operator==(const BasicStringAllocator&) const { return true; }
        bool operator!=(const BasicStringAllocator&) const { return false; }
    };

    // =============================================
    // String utilities
    // =============================================
    template<typename CharT>
    size_t NKENTSEU_TEMPLATE_API StringLength(const CharT* str) {
        const CharT* ptr = str;
        while(*ptr) ++ptr;
        return ptr - str;
    }

    // =============================================
    // Iterators
    // =============================================
    template<typename CharT>
    class NKENTSEU_TEMPLATE_API BasicStringIterator {
    public:
        using ValueType = CharT;
        using Pointer = CharT*;
        using Reference = CharT&;
        using DifferenceType = ptrdiff_t;
        using difference_type = usize;

        explicit BasicStringIterator(Pointer ptr) : m_Ptr(ptr) {}

        // Iterator operations
        Reference operator*() const { return *m_Ptr; }
        Pointer operator->() { return m_Ptr; }

        BasicStringIterator& operator++() { ++m_Ptr; return *this; }
        BasicStringIterator operator++(int) { 
            BasicStringIterator tmp = *this; 
            ++m_Ptr; 
            return tmp; 
        }

        BasicStringIterator& operator--() { --m_Ptr; return *this; }
        BasicStringIterator operator--(int) { 
            BasicStringIterator tmp = *this; 
            --m_Ptr; 
            return tmp; 
        }

        BasicStringIterator operator+(difference_type n) const { return BasicStringIterator(m_Ptr + n); }
        difference_type operator-(const BasicStringIterator& other) const { return m_Ptr - other.m_Ptr; }

        bool operator==(const BasicStringIterator& other) const { return m_Ptr == other.m_Ptr; }
        bool operator!=(const BasicStringIterator& other) const { return m_Ptr != other.m_Ptr; }

    private:
        Pointer m_Ptr;
    };

    template<typename CharT>
    class NKENTSEU_TEMPLATE_API ReverseBasicStringIterator {
    public:
        using IteratorType = BasicStringIterator<CharT>;
        using ValueType = typename IteratorType::ValueType;
        using Pointer = typename IteratorType::Pointer;
        using Reference = typename IteratorType::Reference;
        using difference_type = usize;

        explicit ReverseBasicStringIterator(IteratorType it) : m_Current(it) {}

        Reference operator*() const { 
            IteratorType tmp = m_Current;
            return *--tmp;
        }

        ReverseBasicStringIterator& operator++() { 
            --m_Current;
            return *this; 
        }
        
        ReverseBasicStringIterator operator++(int) {
            ReverseBasicStringIterator tmp = *this;
            --m_Current;
            return tmp;
        }

        ReverseBasicStringIterator& operator--() { ++m_Current; return *this; }
        ReverseBasicStringIterator operator--(int) { 
            BasicStringIterator tmp = *this; 
            ++m_Current; 
            return tmp; 
        }

        ReverseBasicStringIterator operator+(difference_type n) const { return ReverseBasicStringIterator(m_Current - n); }
        difference_type operator-(const ReverseBasicStringIterator& other) const { return m_Current + other.m_Current; }

        bool operator==(const ReverseBasicStringIterator& other) const { return m_Current == other.m_Current; }
        bool operator!=(const ReverseBasicStringIterator& other) const { return m_Current != other.m_Current; }

    private:
        IteratorType m_Current;
    };

    // =============================================
    // String View
    // =============================================
    template<typename CharT>
    class NKENTSEU_TEMPLATE_API BasicStringView {
    public:
        using ValueType = CharT;
        using ConstPointer = const CharT*;
        
        BasicStringView() noexcept : m_Data(nullptr), m_Size(0) {}
        BasicStringView(const CharT* data, size_t size) noexcept : m_Data(data), m_Size(size) {}
        BasicStringView(const BasicString<CharT>& str) noexcept : m_Data(str.Data()), m_Size(str.Length()) {}

        // Accessors
        ConstPointer Data() const noexcept { return m_Data; }
        size_t Length() const noexcept { return m_Size; }
        bool Empty() const noexcept { return m_Size == 0; }

        CharT operator[](size_t index) const noexcept {
            NKENTSEU_ASSERT(index < m_Size, "Index out of range");
            return m_Data[index];
        }

    private:
        ConstPointer m_Data;
        size_t m_Size;
    };

    // =============================================
    // Main String Class
    // =============================================
    template<typename CharT, typename Allocator = BasicStringAllocator<CharT>>
    class NKENTSEU_TEMPLATE_API BasicString {
    private:
        // SSO implementation
        static constexpr size_t SSO_CAPACITY = 15 / sizeof(CharT) + 1;
        
        union Storage {
            CharT sso[SSO_CAPACITY];
            struct {
                CharT* data;
                size_t capacity;
            } heap;
        };

        Storage m_Storage;
        size_t m_Size = 0;
        Allocator m_Allocator;

        // Memory management
        void EnsureNullTerminator() noexcept {
            if(Capacity() > m_Size) {
                Data()[m_Size] = CharT();
            }
        }

        static void SafeCopy(CharT* dest, const CharT* src, size_t count) noexcept {
            for(size_t i = 0; i < count; ++i) {
                dest[i] = src[i];
            }
        }

        bool IsSso() const noexcept { return m_Size < SSO_CAPACITY; }

        void Destroy() noexcept {
            if (!IsSso()) {
                m_Allocator.Deallocate(m_Storage.heap.data, m_Storage.heap.capacity);
            }
        }

        void Grow(size_t newCap) {
            const size_t newSize = newCap > m_Size * 2 ? newCap : m_Size * 2;
            CharT* newData = m_Allocator.Allocate(newSize);
            
            SafeCopy(newData, Data(), m_Size);
            Destroy();
            
            m_Storage.heap.data = newData;
            m_Storage.heap.capacity = newSize;
        }

        // Unicode conversion internals
        template<typename TargetChar>
        static void UnicodeConvert(const CharT* src, size_t len, BasicString<TargetChar>& dest) {
            if constexpr (sizeof(CharT) == sizeof(TargetChar)) {
                dest.Assign(reinterpret_cast<const TargetChar*>(src), len);
            }
            else {
                for(size_t i = 0; i < len; ) {
                    char32_t codepoint = ReadCodePoint(src, i, len);
                    AppendCodePoint(dest, codepoint);
                }
            }
        }

        static char32_t ReadCodePoint(const CharT* str, size_t& index, size_t len) {
            if constexpr(sizeof(CharT) == 1) { // UTF-8
                if(index >= len) return 0;
                
                unsigned char c = str[index++];
                if((c & 0x80) == 0) return c;
    
                char32_t result = 0;
                int remaining = 0;
                
                if     ((c & 0xE0) == 0xC0) { remaining = 1; result = c & 0x1F; }
                else if((c & 0xF0) == 0xE0) { remaining = 2; result = c & 0x0F; }
                else if((c & 0xF8) == 0xF0) { remaining = 3; result = c & 0x07; }
                
                for(int j = 0; j < remaining && index < len; j++, index++) {
                    result = (result << 6) | (str[index] & 0x3F);
                }
                return result;
            }
            else if constexpr(sizeof(CharT) == 2) { // UTF-16
                if(index >= len) return 0;
                
                char16_t w1 = str[index++];
                if((w1 & 0xF800) == 0xD800 && index < len) {
                    char16_t w2 = str[index];
                    if((w2 & 0xFC00) == 0xDC00) {
                        index++;
                        return 0x10000 + ((w1 & 0x3FF) << 10) | (w2 & 0x3FF);
                    }
                }
                return w1;
            }
            else { // UTF-32
                return index < len ? str[index++] : 0;
            }
        }

        template<typename TargetChar>
        static void AppendCodePoint(BasicString<TargetChar>& dest, char32_t cp) {
            if constexpr(sizeof(TargetChar) == 1) { // UTF-8
                if(cp <= 0x7F) {
                    dest.Append(static_cast<TargetChar>(cp));
                }
                else if(cp <= 0x7FF) {
                    dest.Append(static_cast<TargetChar>(0xC0 | (cp >> 6)));
                    dest.Append(static_cast<TargetChar>(0x80 | (cp & 0x3F)));
                }
                else if(cp <= 0xFFFF) {
                    dest.Append(static_cast<TargetChar>(0xE0 | (cp >> 12)));
                    dest.Append(static_cast<TargetChar>(0x80 | ((cp >> 6) & 0x3F)));
                    dest.Append(static_cast<TargetChar>(0x80 | (cp & 0x3F)));
                }
                else {
                    dest.Append(static_cast<TargetChar>(0xF0 | (cp >> 18)));
                    dest.Append(static_cast<TargetChar>(0x80 | ((cp >> 12) & 0x3F)));
                    dest.Append(static_cast<TargetChar>(0x80 | ((cp >> 6) & 0x3F)));
                    dest.Append(static_cast<TargetChar>(0x80 | (cp & 0x3F)));
                }
            }
            else if constexpr(sizeof(TargetChar) == 2) { // UTF-16
                if(cp > 0xFFFF) {
                    cp -= 0x10000;
                    dest.Append(static_cast<TargetChar>(0xD800 | (cp >> 10)));
                    dest.Append(static_cast<TargetChar>(0xDC00 | (cp & 0x03FF)));
                }
                else {
                    dest.Append(static_cast<TargetChar>(cp));
                }
            }
            else { // UTF-32
                dest.Append(static_cast<TargetChar>(cp));
            }
        }

    public:
        // Type aliases
        using Iterator = BasicStringIterator<CharT>;
        using ConstIterator = BasicStringIterator<const CharT>;
        using ReverseIterator = ReverseBasicStringIterator<CharT>;
        using ConstReverseIterator = ReverseBasicStringIterator<const CharT>;
        static constexpr size_t npos = static_cast<size_t>(-1);

        // ========== Constructors/Destructors ==========
        BasicString() noexcept { m_Storage.sso[0] = CharT(); }
        
        BasicString(const CharT* s, size_t count) { Assign(s, count); }
        BasicString(const CharT* s) : BasicString(s, StringLength(s)) {}
        
        ~BasicString() { Destroy(); }

        BasicString(const BasicString& other) : m_Size(other.m_Size), m_Allocator(other.m_Allocator) 
        {
            if(other.IsSso()) {
                // Copie SSO
                SafeCopy(m_Storage.sso, other.m_Storage.sso, m_Size + 1); // +1 pour le null-terminator
            } 
            else {
                // Allocation heap avec la capacité originale
                m_Storage.heap.data = m_Allocator.Allocate(other.m_Storage.heap.capacity);
                SafeCopy(m_Storage.heap.data, other.m_Storage.heap.data, m_Size);
                m_Storage.heap.capacity = other.m_Storage.heap.capacity;
            }
            EnsureNullTerminator();
        }

        // Move operations
        BasicString(BasicString&& other) noexcept 
            : m_Size(other.m_Size), m_Allocator(std::move(other.m_Allocator)) 
        {
            if (other.IsSso()) {
                SafeCopy(m_Storage.sso, other.m_Storage.sso, m_Size + 1);
            } else {
                m_Storage.heap = other.m_Storage.heap;
                other.m_Storage.heap = {nullptr, 0};
            }
            other.m_Size = 0;
        }

        BasicString& operator=(BasicString&& other) noexcept {
            if (this != &other) {
                Destroy();
                m_Size = other.m_Size;
                m_Allocator = std::move(other.m_Allocator);
                
                if (other.IsSso()) {
                    SafeCopy(m_Storage.sso, other.m_Storage.sso, m_Size + 1);
                } else {
                    m_Storage.heap = other.m_Storage.heap;
                    other.m_Storage.heap = {nullptr, 0};
                }
                other.m_Size = 0;
            }
            return *this;
        }

        // ========== Assignment Operators ==========
        BasicString& operator=(const BasicString& other) {
            if (this != &other) {
                Assign(other.Data(), other.Length());
            }
            return *this;
        }

        BasicString& operator=(const CharT* s) {
            Assign(s, StringLength(s));
            return *this;
        }

        // ========== Element Access ==========
        static bool IsValidUtf8(const char* str, size_t len) {
            size_t i = 0;
            while (i < len) {
                int bytes = 0;
                unsigned char c = str[i];
                // Logique de validation des séquences
                if ((c & 0x80) == 0x00) bytes = 1;
                else if ((c & 0xE0) == 0xC0) bytes = 2;
                else if ((c & 0xF0) == 0xE0) bytes = 3;
                else if ((c & 0xF8) == 0xF0) bytes = 4;
                else return false;
                
                if (i + bytes > len) return false;
                for (int j = 1; j < bytes; ++j) {
                    if ((str[i + j] & 0xC0) != 0x80) return false;
                }
                i += bytes;
            }
            return true;
        }

        template<typename F>
        void ForEach(F&& callback) const {
            const CharT* ptr = Data();
            for (size_t i = 0; i < m_Size; ++i) {
                callback(ptr[i]);
            }
        }

        template<typename F>
        bool WhileReach(F&& predicate) const {
            const CharT* ptr = Data();
            size_t i = 0;
            while (i < m_Size && predicate(ptr[i])) {
                ++i;
            }
            return i == m_Size;
        }

        const CharT* Data() const noexcept { 
            return IsSso() ? m_Storage.sso : m_Storage.heap.data;
        }

        CharT* Data() noexcept { 
            return IsSso() ? m_Storage.sso : m_Storage.heap.data; 
        }

        operator const CharT*() const noexcept { return Data(); }

        // ========== Capacity Operations ==========
        size_t Length() const noexcept { return m_Size; }
        size_t Capacity() const noexcept { 
            return IsSso() ? SSO_CAPACITY : m_Storage.heap.capacity;
        }

        bool Empty() const noexcept { return m_Size == 0; }

        void Reserve(size_t newCap) {
            if (newCap > Capacity()) {
                Grow(newCap);
            }
        }

        void Resize(size_t new_size, CharT fill_char = CharT()) {
            const size_t old_size = m_Size;
            
            if(new_size < old_size) {
                if constexpr(IsClass<CharT>::value) {
                    for(size_t i = new_size; i < old_size; ++i) {
                        Data()[i].~CharT();
                    }
                }
                m_Size = new_size;
            } 
            else if(new_size > old_size) {
                Reserve(new_size);
                for(size_t i = old_size; i < new_size; ++i) {
                    new (Data() + i) CharT(fill_char);
                }
                m_Size = new_size;
            }
            
            EnsureNullTerminator();
        }

        // ========== Modifiers ==========
        template<typename T = CharT>
        typename std::enable_if<sizeof(T) >= 2, bool>::type
        IsValidUnicode() const {
            // Validation basique UTF-16/UTF-32
            const CharT* ptr = Data();
            while(*ptr) {
                if constexpr(sizeof(T) == 2) {
                    if((*ptr & 0xF800) == 0xD800) { // Surrogate pair
                        if((ptr[1] & 0xFC00) != 0xDC00) return false;
                        ++ptr;
                    }
                }
                ++ptr;
            }
            return true;
        }

        void Clear() noexcept {
            m_Storage.sso[0] = CharT();
            m_Size = 0;
        }

        void Swap(BasicString& other) noexcept {
            Storage tmpStorage = m_Storage;
            m_Storage = other.m_Storage;
            other.m_Storage = tmpStorage;

            size_t tmpSize = m_Size;
            m_Size = other.m_Size;
            other.m_Size = tmpSize;
        }

        void Assign(const CharT* s, size_t count) {
            if (count == 0) {
                Clear();
                return;
            }

            if (count > Capacity()) {
                Destroy();
                m_Storage.heap.data = m_Allocator.Allocate(count);
                m_Storage.heap.capacity = count;
            }

            SafeCopy(Data(), s, count);

            //std::memcpy(Data(), s, count * sizeof(CharT));
            m_Size = count;
            Data()[m_Size] = CharT();
        }

        // Helper pour la comparaison des longueurs
        int CompareLengths(size_t otherLength) const noexcept {
            if(m_Size == otherLength) return 0;
            return (m_Size < otherLength) ? -1 : 1;
        }

        void Append(CharT ch) {
            if(m_Size >= Capacity()) Grow(m_Size + 1);
            Data()[m_Size++] = ch;
            Data()[m_Size] = CharT();
        }

        // ========== String Operations ==========
        /**
         * Compare deux chaînes caractère par caractère
         * @param other Chaîne à comparer
         * @return -1, 0 ou 1 selon la comparaison
         */
        int Compare(const BasicString& other) const noexcept {
            const size_t minLen = std::min(m_Size, other.m_Size);
            
            // Comparaison caractère par caractère
            for(size_t i = 0; i < minLen; ++i) {
                if(Data()[i] < other.Data()[i]) return -1;
                else if(Data()[i] > other.Data()[i]) return 1;
            }
            
            // Comparaison des longueurs si début identique
            return CompareLengths(other.m_Size);
        }

        // ========== COMPARAISON CARACTÈRE ==========
    
        /**
         * Compare un caractère à une position spécifique
         * @param pos Position dans la chaîne (doit être < Length())
         * @param ch Caractère à comparer
         * @return Résultat de comparaison (0 = égal, <0 = inférieur, >0 = supérieur)
         */
        int Compare(size_t pos, CharT ch) const {
            NKENTSEU_ASSERT(pos < m_Size, "Position hors limites");
            return static_cast<int>(Data()[pos]) - static_cast<int>(ch);
        }

        // ========== COMPARAISON CHAÎNE C-STYLE ==========
        
        /**
         * Compare avec une chaîne C-style
         * @param str Chaîne null-terminated à comparer
         */
        int Compare(const CharT* str) const noexcept {
            const CharT* ptr = Data();
            size_t i = 0;
            
            // Comparaison jusqu'au premier caractère différent
            while(i < m_Size && *str && ptr[i] == *str) {
                ++i;
                ++str;
            }
            
            // Décision finale
            if(i == m_Size && !*str) return 0; // Égalité
            return (ptr[i] < *str) ? -1 : 1;
        }

        /**
         * Compare un caractère spécifique
         * @param pos Position dans la chaîne
         * @param ch Caractère à comparer
         * @return Résultat de la comparaison (-1, 0, 1)
         */
        int CompareAt(size_t pos, CharT ch) const {
            NKENTSEU_ASSERT(pos < m_Size, "Position hors limites");
            const CharT current = Data()[pos];
            return (current < ch) ? -1 : (current > ch) ? 1 : 0;
        }

        /**
         * Compare deux plages de caractères élément par élément
         * @param str1 Début de la première plage
         * @param str2 Début de la seconde plage
         * @param count Nombre de caractères à comparer
         * @return true si les plages sont identiques, false sinon
         */
        static bool CompareRange(const CharT* str1, const CharT* str2, size_t count) noexcept {
            for(size_t i = 0; i < count; ++i) {
                if(str1[i] != str2[i]) {
                    return false;
                }
            }
            return true;
        }

        // ========== SURCHARGES UTILES ==========
        
        bool StartsWith(CharT ch) const noexcept {
            return !Empty() && Data()[0] == ch;
        }

        bool EndsWith(CharT ch) const noexcept {
            return !Empty() && Data()[m_Size - 1] == ch;
        }

        bool Contains(CharT ch) const noexcept {
            return Find(ch) != npos;
        }

        size_t RFind(CharT ch, size_t pos = npos) const noexcept {
            pos = (pos > m_Size) ? m_Size - 1 : pos;
            for(size_t i = pos; i != npos; --i) {
                if(Data()[i] == ch) return i;
            }
            return npos;
        }

        size_t Find(const BasicString& substr, size_t pos = 0) const noexcept {
            return Find(substr.Data(), substr.Length(), pos);
        }

        size_t Find(const CharT* substr, size_t substrLen, size_t pos = 0) const noexcept {
            if(substrLen == 0 || pos + substrLen > m_Size) 
                return npos;
                
            const CharT* start = Data() + pos;
            const CharT* end = Data() + m_Size - substrLen + 1;
            
            for(const CharT* ptr = start; ptr < end; ++ptr) {
                if(CompareRange(ptr, substr, substrLen)) {
                    return ptr - Data();
                }
            }
            return npos;
        }

        size_t Find(CharT ch, size_t pos = 0) const noexcept {
            for(; pos < Length(); ++pos) {
                if(Data()[pos] == ch) return pos;
            }
            return npos;
        }

        BasicString& Replace(size_t pos, size_t count, const BasicString& replacement) {
            if(pos > Length()) throw std::out_of_range("Invalid position");
            
            count = (pos + count > Length()) ? Length() - pos : count;
            BasicString newStr;
            newStr.Reserve(Length() - count + replacement.Length());
            
            // Partie avant
            newStr.Append(Data(), pos);
            // Remplacement
            newStr.Append(replacement.Data(), replacement.Length());
            // Partie après
            newStr.Append(Data() + pos + count, Length() - pos - count);
            
            Swap(newStr);
            return *this;
        }

        BasicString Substr(size_t pos = 0, size_t count = npos) const {
            if(pos > m_Size) throw std::out_of_range("Invalid substring position");
            
            count = (count == npos || pos + count > m_Size) ? m_Size - pos : count;
            return BasicString(Data() + pos, count);
        }

        bool Contains(const BasicString& substr) const noexcept {
            return Find(substr) != npos;
        }

        // ========== Iterators ==========
        Iterator begin() noexcept { return Iterator(Data()); }
        ConstIterator begin() const noexcept { return ConstIterator(Data()); }
        Iterator end() noexcept { return Iterator(Data() + m_Size); }
        ConstIterator end() const noexcept { return ConstIterator(Data() + m_Size); }

        ReverseIterator rbegin() noexcept { return ReverseIterator(end()); }
        ConstReverseIterator rbegin() const noexcept { return ConstReverseIterator(end()); }
        ReverseIterator rend() noexcept { return ReverseIterator(begin()); }
        ConstReverseIterator rend() const noexcept { return ConstReverseIterator(begin()); }

        // ========== Conversions ==========
        template<typename TargetChar>
        BasicString<TargetChar> ConvertTo() const {
            BasicString<TargetChar> result;
            UnicodeConvert<TargetChar>(Data(), Length(), result);
            return result;
        }

        explicit operator BasicString<char>() const { return ConvertTo<char>(); }
        explicit operator BasicString<wchar_t>() const { return ConvertTo<wchar_t>(); }

        // ========== Operator Overloads ==========
        BasicString& operator+=(const BasicString& other) {
            const size_t newSize = m_Size + other.m_Size;
            Reserve(newSize + 1);
            
            SafeCopy(Data() + m_Size, other.Data(), other.m_Size);
            m_Size = newSize;
            EnsureNullTerminator();
            
            return *this;
        }

        friend BasicString operator+(const BasicString& lhs, const BasicString& rhs) {
            BasicString result(lhs);
            result += rhs;
            return result;
        }

        bool operator==(const BasicString& other) const noexcept { return Compare(other) == 0; }
        bool operator!=(const BasicString& other) const noexcept { return Compare(other) != 0; }
        bool operator<(const BasicString& other) const noexcept { return Compare(other) < 0; }
        bool operator>(const BasicString& other) const noexcept { return Compare(other) > 0; }

        // ========== Stream Operators ==========
        friend std::ostream& operator<<(std::ostream& os, const BasicString& str) {
            if constexpr (std::is_same_v<CharT, char>) {
                return os.write(str.Data(), str.Length());
            } else if constexpr (sizeof(CharT) == sizeof(char)) {
                return os.write(reinterpret_cast<const char*>(str.Data()), str.Length());
            } else {
                // Fallback: affichage caractère par caractère
                for (size_t i = 0; i < str.Length(); ++i) {
                    os.put(static_cast<char>(str.Data()[i]));
                }
                return os;
            }
        }
        
        friend std::wostream& operator<<(std::wostream& os, const BasicString& str) {
            if constexpr (std::is_same_v<CharT, wchar_t>) {
                return os.write(str.Data(), str.Length());
            } else if constexpr (sizeof(CharT) == sizeof(wchar_t)) {
                return os.write(reinterpret_cast<const wchar_t*>(str.Data()), str.Length());
            } else {
                for (size_t i = 0; i < str.Length(); ++i) {
                    os.put(static_cast<wchar_t>(str.Data()[i]));
                }
                return os;
            }
        }   
    };

    // =============================================
    // Type Aliases
    // =============================================
    using String = BasicString<char>;
    using String16 = BasicString<char16_t>;
    using String32 = BasicString<char32_t>;
    using WString = BasicString<wchar_t>;

    // =============================================
    // Template Specializations
    // =============================================
    /* Conversion specializations here */

    // Spécialisations externes
    template<>
    template<>
    NKENTSEU_INLINE_API BasicString<char> BasicString<char16_t>::ConvertTo<char>() const {
        BasicString<char> result;
        UnicodeConvert<char>(Data(), Length(), result);
        return result;
    }

    template<>
    template<>
    NKENTSEU_INLINE_API BasicString<char32_t> BasicString<char16_t>::ConvertTo<char32_t>() const {
        BasicString<char32_t> result;
        UnicodeConvert<char32_t>(Data(), Length(), result);
        return result;
    }

    template<>
    template<>
    NKENTSEU_INLINE_API BasicString<wchar_t> BasicString<char16_t>::ConvertTo<wchar_t>() const {
        static_assert(sizeof(wchar_t) >= 2, "wchar_t must be at least 16-bit");
        BasicString<wchar_t> result;
        UnicodeConvert<wchar_t>(Data(), Length(), result);
        return result;
    }

    template<>
    template<>
    NKENTSEU_INLINE_API BasicString<char> BasicString<char32_t>::ConvertTo<char>() const {
        BasicString<char> result;
        UnicodeConvert<char>(Data(), Length(), result);
        return result;
    }

    template<>
    template<>
    NKENTSEU_INLINE_API BasicString<char16_t> BasicString<char>::ConvertTo<char16_t>() const {
        BasicString<char16_t> result;
        UnicodeConvert<char16_t>(Data(), Length(), result);
        return result;
    }

    // Spécialisations supplémentaires pour les conversions manquantes

    // Conversion char -> char32_t
    template<>
    template<>
    NKENTSEU_INLINE_API BasicString<char32_t> BasicString<char>::ConvertTo<char32_t>() const {
        BasicString<char32_t> result;
        UnicodeConvert<char32_t>(Data(), Length(), result);
        return result;
    }

    // Conversion char -> wchar_t
    template<>
    template<>
    NKENTSEU_INLINE_API BasicString<wchar_t> BasicString<char>::ConvertTo<wchar_t>() const {
        BasicString<wchar_t> result;
        UnicodeConvert<wchar_t>(Data(), Length(), result);
        return result;
    }

    // Conversion char32_t -> char16_t
    template<>
    template<>
    NKENTSEU_INLINE_API BasicString<char16_t> BasicString<char32_t>::ConvertTo<char16_t>() const {
        BasicString<char16_t> result;
        UnicodeConvert<char16_t>(Data(), Length(), result);
        return result;
    }

    // Conversion char32_t -> wchar_t
    template<>
    template<>
    NKENTSEU_INLINE_API BasicString<wchar_t> BasicString<char32_t>::ConvertTo<wchar_t>() const {
        static_assert(sizeof(wchar_t) >= 2, "wchar_t must be at least 16-bit");
        BasicString<wchar_t> result;
        UnicodeConvert<wchar_t>(Data(), Length(), result);
        return result;
    }

    // Conversion wchar_t -> char
    template<>
    template<>
    NKENTSEU_INLINE_API BasicString<char> BasicString<wchar_t>::ConvertTo<char>() const {
        BasicString<char> result;
        UnicodeConvert<char>(Data(), Length(), result);
        return result;
    }

    // Conversion wchar_t -> char16_t
    template<>
    template<>
    NKENTSEU_INLINE_API BasicString<char16_t> BasicString<wchar_t>::ConvertTo<char16_t>() const {
        static_assert(sizeof(wchar_t) >= 2, "wchar_t must be at least 16-bit");
        BasicString<char16_t> result;
        UnicodeConvert<char16_t>(Data(), Length(), result);
        return result;
    }

    // Conversion wchar_t -> char32_t
    template<>
    template<>
    NKENTSEU_INLINE_API BasicString<char32_t> BasicString<wchar_t>::ConvertTo<char32_t>() const {
        BasicString<char32_t> result;
        UnicodeConvert<char32_t>(Data(), Length(), result);
        return result;
    }

    // =============================================
    // Literals
    // =============================================
    namespace literals {
        String operator""_s(const char* str, size_t len) { return String(str, len); }
        String16 operator""_s16(const char16_t* str, size_t len) { return String16(str, len); }
        String32 operator""_s32(const char32_t* str, size_t len) { return String32(str, len); }
        WString operator""_ws(const wchar_t* str, size_t len) { return WString(str, len); }
    }

} // namespace nkentseu