#pragma once
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <locale>
#include <cwctype>
#include <functional> // Pour std::hash

#include "Nkentseu/Platform/Assertion.h"
#include "Nkentseu/Platform/Types.h"
#include "Nkentseu/Platform/Inline.h"

#include "StringIterator.h"
#include "StringAllocator.h"

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
    // String utilities
    // =============================================
    template<typename CharT>
    usize NKENTSEU_TEMPLATE_API StringLength(const CharT* str) {
        const CharT* ptr = str;
        while(*ptr) ++ptr;
        return ptr - str;
    }

    // =============================================
    // Main String Class
    // =============================================
    template<typename CharT, typename Allocator = BasicStringAllocator<CharT>>
    class NKENTSEU_TEMPLATE_API BasicString {
        public:
        
        // Type aliases
        using Iterator = BasicStringIterator<CharT>;
        using ConstIterator = BasicStringIterator<const CharT>;
        using ReverseIterator = ReverseBasicStringIterator<CharT>;
        using ConstReverseIterator = ReverseBasicStringIterator<const CharT>;
        using SizeType = usize;
        using ValueType = CharT;
        //static constexpr usize npos = static_cast<usize>(-1);
        static constexpr typename BasicString<CharT, Allocator>::SizeType npos = static_cast<SizeType>(-1);
        // SSO implementation
        static constexpr usize SSO_CAPACITY = 15 / sizeof(CharT) + 1;

    private:
        
        union Storage {
            CharT sso[SSO_CAPACITY];
            struct {
                CharT* data;
                usize capacity;
            } heap;
        };

        Storage m_Storage;
        usize m_Size = 0;
        Allocator m_Allocator;

        // Memory management
        void EnsureNullTerminator() noexcept {
            if(Capacity() > m_Size) {
                Data()[m_Size] = CharT();
            }
        }

        static void SafeCopy(CharT* dest, const CharT* src, usize count) noexcept {
            for(usize i = 0; i < count; ++i) {
                dest[i] = src[i];
            }
        }

        void Destroy() noexcept {
            if (!IsSso()) {
                m_Allocator.Deallocate(m_Storage.heap.data, m_Storage.heap.capacity);
            }
        }

        void Grow(usize newCap) {
            const usize newSize = newCap > m_Size * 2 ? newCap : m_Size * 2;
            CharT* newData = m_Allocator.Allocate(newSize);
            
            SafeCopy(newData, Data(), m_Size);
            Destroy();
            
            m_Storage.heap.data = newData;
            m_Storage.heap.capacity = newSize;
        }

        // Unicode conversion internals
        template<typename TargetChar>
        static void UnicodeConvert(const CharT* src, usize len, BasicString<TargetChar>& dest) {
            if constexpr (sizeof(CharT) == sizeof(TargetChar)) {
                dest.Assign(reinterpret_cast<const TargetChar*>(src), len);
            }
            else {
                for(usize i = 0; i < len; ) {
                    char32_t codepoint = ReadCodePoint(src, i, len);
                    AppendCodePoint(dest, codepoint);
                }
            }
        }

        static char32 ReadCodePoint(const CharT* str, usize& index, usize len) {
            if constexpr(sizeof(CharT) == 1) { // UTF-8
                if(index >= len) return 0;
                
                unsigned char c = str[index++];
                if((c & 0x80) == 0) return c;
    
                char32 result = 0;
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

        // ========== Constructors/Destructors ==========
        BasicString() noexcept { m_Storage.sso[0] = CharT(); }
        
        BasicString(const CharT* s, usize count) { Assign(s, count); }
        BasicString(const CharT* s) : BasicString(s, StringLength(s)) {}
        /**
         * Constructeur à partir d'un caractère répété n fois
         * @param count Nombre de répétitions
         * @param ch Caractère à répéter
         */
        explicit BasicString(usize count, CharT ch) {
            if(count == 0) {
                Clear();
                return;
            }

            if(count < SSO_CAPACITY) {
                // Stockage SSO
                for(usize i = 0; i < count; ++i) {
                    m_Storage.sso[i] = ch;
                }
                m_Size = count;
                m_Storage.sso[count] = CharT(); // Null-terminator
            } else {
                // Allocation heap avec espace supplémentaire pour le null-terminator
                const usize totalCapacity = count + 1;
                m_Storage.heap.data = m_Allocator.Allocate(totalCapacity);
                m_Storage.heap.capacity = totalCapacity;
                
                for(usize i = 0; i < count; ++i) {
                    m_Storage.heap.data[i] = ch;
                }
                m_Size = count;
                m_Storage.heap.data[count] = CharT(); // Null-terminator
            }
        }
        
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

        // Ajouter ces constructeurs à BasicString
        // template<typename T>
        // BasicString(const T* s) {
        //     static_assert(sizeof(T) == sizeof(CharT), "Incompatible character type");
        //     m_Size = 0;
        //     while (s[m_Size]) ++m_Size;
        //     Reserve(m_Size);
        //     std::memcpy(Data(), s, m_Size * sizeof(CharT));
        // }

        template<typename T>
        BasicString(const T* s) {
            m_Size = 0;
            while (s[m_Size]) ++m_Size;
            Reserve(m_Size);
            for (usize i = 0; i < m_Size; ++i) {
                Data()[i] = static_cast<CharT>(s[i]);
            }
        }

        // Spécialisation pour char8_t
        template<>
        BasicString(const char8_t* s) {
            m_Size = 0;
            while (s[m_Size]) ++m_Size;
            Reserve(m_Size);
            // std::memcpy(Data(), s, m_Size * sizeof(CharT));
            for (usize i = 0; i < m_Size; ++i) {
                Data()[i] = static_cast<CharT>(s[i]);
            }
        }

        // Dans BasicString.h
        // static BasicString FromWide(const wchar_t* wstr) {
        //     BasicString result;
        //     if (!wstr) return result;
            
        //     usize len = 0;
        //     while (wstr[len]) ++len;
            
        //     // Conversion simple (pour un vrai projet, utiliser ICU ou WinAPI)
        //     result.Reserve(len);
        //     for (usize i = 0; i < len; ++i) {
        //         result.m_Data[i] = static_cast<CharT>(wstr[i]); 
        //     }
        //     result.m_Size = len;
        //     return result;
        // }

        static BasicString FromWide(const wchar_t* wstr) {
            BasicString result;
            if (!wstr) return result;
    
            // Conversion WinAPI
            const int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
            result.Reserve(len);
            WideCharToMultiByte(CP_UTF8, 0, wstr, -1, result.Data(), len, nullptr, nullptr);
            result.m_Size = len - 1; // Exclure le null-terminator
            return result;
        }

        // Constructeur pour wchar_t (spécialisation)
        BasicString(const wchar_t* s) {
            *this = FromWide(s);
        }

        // Dans BasicString
        friend BasicString operator+(const BasicString& lhs, const CharT* rhs) {
            BasicString result(lhs);
            result.Append(rhs);
            return result;
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

        // BasicString& operator=(const BasicString& other) {
        //     if (this != &other) { // Ajouter cette vérification
        //         Clear();
        //         Reserve(other.m_Size);
        //         for (usize i = 0; i < other.m_Size; ++i) {
        //             m_Data[i] = other.m_Data[i];
        //         }
        //         m_Size = other.m_Size;
        //     }
        //     return *this;
        // }

        BasicString& operator=(const CharT* s) {
            Assign(s, StringLength(s));
            return *this;
        }

        // ========== Element Access ==========

        CharT& operator[](size_t index) noexcept {
            return Data()[index];
        }

        CharT operator[](size_t index) const noexcept {
            return Data()[index];
        }

        bool IsSso() const noexcept { return m_Size < SSO_CAPACITY; }

        static bool IsValidUtf8(const char8* str, usize len) {
            usize i = 0;
            while (i < len) {
                int32 bytes = 0;
                uint8 c = str[i];
                // Logique de validation des séquences
                if ((c & 0x80) == 0x00) bytes = 1;
                else if ((c & 0xE0) == 0xC0) bytes = 2;
                else if ((c & 0xF0) == 0xE0) bytes = 3;
                else if ((c & 0xF8) == 0xF0) bytes = 4;
                else return false;
                
                if (i + bytes > len) return false;
                for (int32 j = 1; j < bytes; ++j) {
                    if ((str[i + j] & 0xC0) != 0x80) return false;
                }
                i += bytes;
            }
            return true;
        }

        bool IsValidUtf8() const {
            const unsigned char* bytes = reinterpret_cast<const unsigned char*>(m_Data);
            usize i = 0;
            while (i < m_Size) {
                // Implémentation simplifiée de la validation UTF-8
                if (bytes[i] <= 0x7F) {
                    i += 1;
                } else if ((bytes[i] & 0xE0) == 0xC0) {
                    if (i+1 >= m_Size) return false;
                    if ((bytes[i+1] & 0xC0) != 0x80) return false;
                    i += 2;
                } else if ((bytes[i] & 0xF0) == 0xE0) {
                    if (i+2 >= m_Size) return false;
                    if ((bytes[i+1] & 0xC0) != 0x80) return false;
                    if ((bytes[i+2] & 0xC0) != 0x80) return false;
                    i += 3;
                } else {
                    return false;
                }
            }
            return true;
        }

        // Dans BasicString
        // bool IsValidUtf8() const {
        //     return IsValidUtf8(Data(), m_Size);
        // }

        template<typename F>
        void ForEach(F&& callback) const {
            const CharT* ptr = Data();
            for (usize i = 0; i < m_Size; ++i) {
                callback(ptr[i]);
            }
        }

        template<typename F>
        bool WhileReach(F&& predicate) const {
            const CharT* ptr = Data();
            usize i = 0;
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
        usize Length() const noexcept { return m_Size; }
        usize Capacity() const noexcept { 
            return IsSso() ? SSO_CAPACITY : m_Storage.heap.capacity;
        }

        bool Empty() const noexcept { return m_Size == 0; }

        void Reserve(usize newCap) {
            if (newCap > Capacity()) {
                Grow(newCap);
            }
        }

        void Resize(usize new_size, CharT fill_char = CharT()) {
            const usize old_size = m_Size;
            
            if(new_size < old_size) {
                if constexpr(IsClass<CharT>::value) {
                    for(usize i = new_size; i < old_size; ++i) {
                        Data()[i].~CharT();
                    }
                }
                m_Size = new_size;
            } 
            else if(new_size > old_size) {
                Reserve(new_size);
                for(usize i = old_size; i < new_size; ++i) {
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

            usize tmpSize = m_Size;
            m_Size = other.m_Size;
            other.m_Size = tmpSize;
        }

        void Assign(const CharT* s, usize count) {
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
        int32 CompareLengths(usize otherLength) const noexcept {
            if(m_Size == otherLength) return 0;
            return (m_Size < otherLength) ? -1 : 1;
        }

        void Append(CharT ch) {
            if(m_Size >= Capacity()) Grow(m_Size + 1);
            Data()[m_Size++] = ch;
            Data()[m_Size] = CharT();
        }

        /**
         * Ajoute un caractère à une position spécifique
         * @param ch Caractère à ajouter
         * @param pos Position d'insertion (par défaut à la fin)
         */
        void Append(CharT ch, usize pos) {
            pos = (pos == npos || pos > m_Size) ? m_Size : pos;
            NKENTSEU_ASSERT(pos <= m_Size, "Position d'insertion invalide");

            if(m_Size >= Capacity()) {
                Grow(m_Size + 1);
            }

            // Décalage des caractères
            for(usize i = m_Size; i > pos; --i) {
                Data()[i] = Data()[i - 1];
            }

            Data()[pos] = ch;
            m_Size++;
            EnsureNullTerminator();
        }

        /**
         * Ajoute une chaîne C-style à la fin
         * @param str Chaîne à ajouter (null-terminated)
         */
        void Append(const CharT* str) {
            const usize str_len = StringLength(str);
            Append(str, m_Size, str_len);
        }

        /**
         * Insère une chaîne à position spécifique
         * @param str Chaîne à insérer
         * @param pos Position d'insertion
         * @param count Nombre de caractères à insérer (par défaut jusqu'au null-terminator)
         */
        void Append(const CharT* str, usize pos, usize count) {
            const usize str_len = (count == npos) ? StringLength(str) : count;
            pos = (pos > m_Size) ? m_Size : pos;
            NKENTSEU_ASSERT(pos <= m_Size, "Position d'insertion invalide");

            const usize new_size = m_Size + str_len;
            Reserve(new_size);

            // Décalage des caractères existants
            for(usize i = new_size - 1; i >= pos + str_len; --i) {
                Data()[i] = Data()[i - str_len];
            }

            // Copie de la nouvelle chaîne
            for(usize i = 0; i < str_len; ++i) {
                Data()[pos + i] = str[i];
            }

            m_Size = new_size;
            EnsureNullTerminator();
        }

        /**
         * Ajoute une BasicString à la fin
         * @param str Chaîne à ajouter
         */
        void Append(const BasicString& str) {
            Append(str.Data(), m_Size, str.Length()); 
        }

        /**
         * Insère une BasicString à position spécifique
         * @param str Chaîne à insérer
         * @param pos Position d'insertion (défaut : fin)
         */
        void Append(const BasicString& str, usize pos) {
            Append(str.Data(), pos, str.Length());
        }

        void Insert(usize pos, CharT ch) {
            if (pos > m_Size)
                pos = m_Size;
        
            Reserve(m_Size + 1);
            MoveRight(pos, 1); // décaler à droite
        
            Data()[pos] = ch;
            ++m_Size;
            Data()[m_Size] = CharT(); // null-terminate si besoin
        }
        
        void Insert(usize pos, const CharT* str) {
            if (!str)
                return;
        
            usize len = StringLength(str);
            Insert(pos, str, len);
        }
        
        void Insert(usize pos, const CharT* str, usize len) {
            if (!str || len == 0)
                return;
        
            if (pos > m_Size)
                pos = m_Size;
        
            Reserve(m_Size + len);
            MoveRight(pos, len);
        
            //std::memcpy(Data() + pos, str, len * sizeof(CharT));
            
            for (usize i = 0; i < len ; ++i) {
                m_Data[i] = static_cast<CharT>(str[i]);
            }
            m_Size += len;
            Data()[m_Size] = CharT(); // null-terminate si besoin
        }
        
        void Insert(usize pos, const BasicString& other) {
            Insert(pos, other.Data(), other.m_Size);
        }  

        void MoveRight(usize pos, usize len) {
            if (len == 0 || pos > m_Size) return;
            
            CharT* data = Data();
            for (usize i = m_Size; i > pos; ) {
                --i;
                data[i + len] = data[i];
            }
        }
    
        void MoveLeft(usize pos, usize len) {
            if (len == 0 || pos + len > m_Size) return;
    
            CharT* data = Data();
            usize num_elements = m_Size - (pos + len); // Éléments restants à déplacer
    
            // Décalage manuel depuis le début vers la gauche
            for (usize i = 0; i < num_elements; ++i) {
                data[pos + i] = data[pos + len + i]; // Écrasement des éléments supprimés
            }
        }

        // ========== ACCÈS AU DERNIER CARACTÈRE ==========
    
        /**
         * Accède au dernier caractère (version modifiable)
         * @return Référence au dernier caractère
         */
        CharT& Back() {
            if (m_Size <= 0) throw std::out_of_range("String::Front() called on empty string");
            return Data()[m_Size - 1];
        }

        /**
         * Accède au dernier caractère (version constante)
         * @return Référence constante au dernier caractère
         */
        const CharT& Back() const {
            if (m_Size <= 0) throw std::out_of_range("String::Front() called on empty string");
            return Data()[m_Size - 1];
        }

        /**
         * Accède au dernier caractère (version modifiable)
         * @return Référence au dernier caractère
         */
        CharT& Front() {
             if (m_Size <= 0) throw std::out_of_range("String::Front() called on empty string");
            return Data()[0];
        }

        /**
         * Accède au dernier caractère (version constante)
         * @return Référence constante au dernier caractère
         */
        const CharT& Front() const {
            if (m_Size <= 0) throw std::out_of_range("String::Front() called on empty string");
            return Data()[0];
        }

        // ========== SUPPRESSION DE CARACTÈRES ==========
        
        /**
         * Supprime un caractère à position donnée
         * @param pos Position du caractère à supprimer
         */
        void Erase(usize pos) {
            //NKENTSEU_ASSERT(pos < m_Size, "Position invalide pour Erase()");
            
            // Décalage des caractères suivants
            for(usize i = pos; i < m_Size - 1; ++i) {
                Data()[i] = Data()[i + 1];
            }
            
            --m_Size;
            EnsureNullTerminator();
        }

        /**
         * Supprime une plage de caractères [pos, pos + count)
         * @param pos Position de départ
         * @param count Nombre de caractères à supprimer (par défaut 1)
         */
        void Erase(usize pos, usize count) {
            //NKENTSEU_ASSERT(pos <= m_Size, "Position de départ invalide");
            count = (pos + count > m_Size) ? m_Size - pos : count;
            
            // Décalage des caractères après la plage
            const usize new_size = m_Size - count;
            for(usize i = pos; i < new_size; ++i) {
                Data()[i] = Data()[i + count];
            }
            
            m_Size = new_size;
            EnsureNullTerminator();
        }

        // ========== String Operations ==========
        /**
         * Compare deux chaînes caractère par caractère
         * @param other Chaîne à comparer
         * @return -1, 0 ou 1 selon la comparaison
         */
        int32 Compare(const BasicString& other) const noexcept {
            const usize minLen = std::min(m_Size, other.m_Size);
            
            // Comparaison caractère par caractère
            for(usize i = 0; i < minLen; ++i) {
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
        int32 Compare(usize pos, CharT ch) const {
            //NKENTSEU_ASSERT(pos < m_Size, "Position hors limites");
            if (pos >= m_Size) {
                //logger.Errors("Position hors limites dans CompareAt()");
                std::cerr << "(BasicString::Compare) Position hors limites dans CompareAt()" << std::endl;
                std::cerr << "(BasicString::Compare) Position: " << pos << ", Taille: " << m_Size << std::endl;
                std::cout << "(BasicString::Compare) Position hors limites dans CompareAt()" << std::endl;
                std::cout << "(BasicString::Compare) Position: " << pos << ", Taille: " << m_Size << std::endl;
                return -1; // Out of bounds
            }
            return static_cast<int>(Data()[pos]) - static_cast<int>(ch);
        }

        // ========== COMPARAISON CHAÎNE C-STYLE ==========
        
        /**
         * Compare avec une chaîne C-style
         * @param str Chaîne null-terminated à comparer
         */
        int32 Compare(const CharT* str) const noexcept {
            const CharT* ptr = Data();
            usize i = 0;
            
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
        int32 CompareAt(usize pos, CharT ch) const {
            // NKENTSEU_ASSERT(pos < m_Size, "Position hors limites");
            if (pos >= m_Size) {
                //logger.Errors("Position hors limites dans CompareAt()");
                std::cerr << "(BasicString::CompareAt) Position hors limites dans CompareAt()" << std::endl;
                std::cout << "(BasicString::CompareAt) Position hors limites dans CompareAt()" << std::endl;
                return npos; // Out of bounds
            }
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
        static bool CompareRange(const CharT* str1, const CharT* str2, usize count) noexcept {
            for(usize i = 0; i < count; ++i) {
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
        
        bool StartsWith(const CharT* str) const noexcept {
            if (!str) return false;
            usize i = 0;
        
            while (str[i] != 0) {
                if (i >= m_Size || Data()[i] != str[i])
                    return false;
                ++i;
            }
        
            return true;
        }
        
        bool StartsWith(const BasicString& sh) const noexcept {
            return StartsWith(sh.Data());
        }
        
        bool EndsWith(CharT ch) const noexcept {
            return !Empty() && Data()[m_Size - 1] == ch;
        }
        
        bool EndsWith(const CharT* str) const noexcept {
            if (!str) return false;
        
            usize len = 0;
            while (str[len] != 0) ++len;
        
            if (len > m_Size)
                return false;
        
            for (usize i = 0; i < len; ++i) {
                if (Data()[m_Size - len + i] != str[i])
                    return false;
            }
        
            return true;
        }
        
        bool EndsWith(const BasicString& sh) const noexcept {
            return EndsWith(sh.Data());
        }        

        bool Contains(CharT ch) const noexcept {
            return Find(ch) != npos;
        }

        usize RFind(CharT ch, usize pos = npos) const noexcept {
            pos = (pos > m_Size) ? m_Size - 1 : pos;
            for(usize i = pos; i != npos; --i) {
                if(Data()[i] == ch) return i;
            }
            return npos;
        }

        usize Find(const BasicString& substr, usize pos = 0) const noexcept {
            return Find(substr.Data(), substr.Length(), pos);
        }

        usize Find(const CharT* substr, usize substrLen, usize pos = 0) const noexcept {
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

        usize Find(CharT ch, usize pos = 0) const noexcept {
            for(; pos < Length(); ++pos) {
                if(Data()[pos] == ch) return pos;
            }
            return npos;
        }

        BasicString& Replace(usize pos, usize count, const BasicString& replacement) {
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

        BasicString Substr(usize pos = 0, usize count = npos) const {
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

        explicit operator BasicString<char8>() const { return ConvertTo<char8>(); }
        explicit operator BasicString<char16>() const { return ConvertTo<char16>(); }
        explicit operator BasicString<char32>() const { return ConvertTo<char32>(); }
        explicit operator BasicString<wchar>() const { return ConvertTo<wchar>(); }

        // ========== Operator Overloads ==========
        BasicString& operator+=(const BasicString& other) {
            const usize newSize = m_Size + other.m_Size;
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

        // ========== CONVERSION IMPLICITE ==========
        // operator const CharT*() const noexcept { 
        //     return Data(); 
        // }

        // ========== OPÉRATEURS MEMBRES EXISTANTS ==========
        bool operator==(const BasicString& other) const noexcept { return Compare(other) == 0; }
        bool operator!=(const BasicString& other) const noexcept { return Compare(other) != 0; }
        bool operator<(const BasicString& other) const noexcept { return Compare(other) < 0; }
        bool operator>(const BasicString& other) const noexcept { return Compare(other) > 0; }
        bool operator<=(const BasicString& other) const noexcept { return Compare(other) <= 0; }
        bool operator>=(const BasicString& other) const noexcept { return Compare(other) >= 0; }

        // ========== Stream Operators ==========
        friend std::ostream& operator<<(std::ostream& os, const BasicString& str) {
            if constexpr (std::is_same_v<CharT, char>) {
                return os.write(str.Data(), str.Length());
            } else if constexpr (sizeof(CharT) == sizeof(char)) {
                return os.write(reinterpret_cast<const char*>(str.Data()), str.Length());
            } else {
                // Fallback: affichage caractère par caractère
                for (usize i = 0; i < str.Length(); ++i) {
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
                for (usize i = 0; i < str.Length(); ++i) {
                    os.put(static_cast<wchar_t>(str.Data()[i]));
                }
                return os;
            }
        }   

        // Spécialisation pour les comparaisons avec const CharT*
        bool operator==(const CharT* other) const noexcept {
            return Compare(other) == 0;
        }

        // Opérateur friend 
        friend bool operator==(const CharT* lhs, const BasicString& rhs) {
            return rhs.Compare(lhs) == 0;
        }

        // Conversion vers std::string
        operator std::basic_string<CharT>() const {
            return std::basic_string<CharT>(Data(), Length());
        }
        
        // Conversion depuis std::string
        BasicString(const std::basic_string<CharT>& str) {
            Assign(str.c_str(), str.size());
        }
    };

    // =============================================
    // Opérateur d'entrée pour la lecture de base
    // =============================================
    template<typename CharT, typename Allocator>
    std::basic_istream<CharT>& operator>>(std::basic_istream<CharT>& is, BasicString<CharT, Allocator>& str) {
        std::basic_string<CharT> tmp;
        is >> tmp;
        str.Assign(tmp.c_str(), tmp.size());
        return is;
    }

    // =============================================
    // Version personnalisée de getline
    // =============================================
    template<typename CharT, typename Allocator>
    std::basic_istream<CharT>& getline(std::basic_istream<CharT>& is, 
                                    BasicString<CharT, Allocator>& str, 
                                    CharT delim = '\n') {
        str.Clear();
        CharT ch;
        
        // Ignorer le délimiteur initial
        if(is.peek() == delim) 
            is.ignore();
        
        // Lire jusqu'au délimiteur
        while(is.get(ch) && ch != delim) {
            str.Append(ch);
        }
        
        // Gérer le cas où le dernier caractère est le délimiteur
        if(is.eof() && ch != delim)
            is.clear(is.rdstate() & ~std::ios_base::failbit);
        
        return is;
    }

    // Égalité
    template<typename CharT, typename Allocator>
    bool operator==(const BasicString<CharT, Allocator>& lhs, const CharT* rhs) noexcept {
        return lhs.Compare(rhs) == 0;
    }

    template<typename CharT, typename Allocator>
    bool operator==(const CharT* lhs, const BasicString<CharT, Allocator>& rhs) noexcept {
        return rhs.Compare(lhs) == 0;
    }

    // Inégalité
    template<typename CharT, typename Allocator>
    bool operator!=(const BasicString<CharT, Allocator>& lhs, const CharT* rhs) noexcept {
        return lhs.Compare(rhs) != 0;
    }

    template<typename CharT, typename Allocator>
    bool operator!=(const CharT* lhs, const BasicString<CharT, Allocator>& rhs) noexcept {
        return rhs.Compare(lhs) != 0;
    }

    // Inférieur
    template<typename CharT, typename Allocator>
    bool operator<(const BasicString<CharT, Allocator>& lhs, const CharT* rhs) noexcept {
        return lhs.Compare(rhs) < 0;
    }

    template<typename CharT, typename Allocator>
    bool operator<(const CharT* lhs, const BasicString<CharT, Allocator>& rhs) noexcept {
        return rhs.Compare(lhs) > 0;
    }

    // Supérieur
    template<typename CharT, typename Allocator>
    bool operator>(const BasicString<CharT, Allocator>& lhs, const CharT* rhs) noexcept {
        return lhs.Compare(rhs) > 0;
    }

    template<typename CharT, typename Allocator>
    bool operator>(const CharT* lhs, const BasicString<CharT, Allocator>& rhs) noexcept {
        return rhs.Compare(lhs) < 0;
    }

    // Inférieur ou égal
    template<typename CharT, typename Allocator>
    bool operator<=(const BasicString<CharT, Allocator>& lhs, const CharT* rhs) noexcept {
        return lhs.Compare(rhs) <= 0;
    }

    template<typename CharT, typename Allocator>
    bool operator<=(const CharT* lhs, const BasicString<CharT, Allocator>& rhs) noexcept {
        return rhs.Compare(lhs) >= 0;
    }

    // Supérieur ou égal
    template<typename CharT, typename Allocator>
    bool operator>=(const BasicString<CharT, Allocator>& lhs, const CharT* rhs) noexcept {
        return lhs.Compare(rhs) >= 0;
    }

    template<typename CharT, typename Allocator>
    bool operator>=(const CharT* lhs, const BasicString<CharT, Allocator>& rhs) noexcept {
        return rhs.Compare(lhs) <= 0;
    }

} // namespace nkentseu

namespace std {
    template<typename CharT, typename Allocator>
    struct hash<nkentseu::BasicString<CharT, Allocator>> {
        size_t operator()(const nkentseu::BasicString<CharT, Allocator>& s) const noexcept {
            // Implémentation de hashage simple mais efficace
            size_t hash = 0;
            const size_t prime = 31;
            for(nkentseu::usize i = 0; i < s.Length(); ++i) {
                hash = hash * prime + static_cast<size_t>(s[i]);
            }
            return hash;
        }
    };
}

// namespace std {
//     template<typename CharT, typename Allocator>
//     struct hash<nkentseu::BasicString<CharT, Allocator>> {
//         size_t operator()(const nkentseu::BasicString<CharT, Allocator>& s) const noexcept {
//             size_t hash = 14695981039346656037ULL;
//             for(nkentseu::usize i = 0; i < s.Length(); ++i) {
//                 hash ^= static_cast<size_t>(s[i]);
//                 hash *= 1099511628211ULL;
//             }
//             return hash;
//         }
//     };
// }