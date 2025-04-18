// StringBase.h
#pragma once
#include "Nkentseu/Memory/Memory.h"
#include "Nkentseu/Platform/Assertion.h"
#include "Nkentseu/Platform/Platform.h"
#include "Nkentseu/Platform/Inline.h"

#include "StringAllocator.h"

#include <algorithm>
#include <utility>
#include <codecvt>
#include <iostream>
#include <functional>
#include <cstring>

namespace nkentseu {

    template<typename CharType, typename Allocator = StringAllocator>
    class NKENTSEU_TEMPLATE_API StringBase {
    protected:
        static constexpr usize SSO_CAPACITY = 16 / sizeof(CharType);
        CharType m_sso[SSO_CAPACITY];
        CharType* m_data = m_sso;
        usize m_length = 0;
        usize m_capacity = SSO_CAPACITY;
        Allocator m_allocator;

        bool IsSSO() const noexcept { return m_data == m_sso; }

        void ReallocateExact(usize newCapacity) {
            newCapacity = memory::AlignUp(newCapacity, static_cast<usize>(16));
            CharType* newData = static_cast<CharType*>(m_allocator.Allocate(newCapacity * sizeof(CharType)));
            
            if (m_data) {
                std::memcpy(newData, m_data, (m_length + 1)  * sizeof(CharType));
                if (!IsSSO()) m_allocator.Deallocate(m_data);
            }
            
            m_data = newData;
            m_capacity = newCapacity;
        }

        void ReserveAligned(usize newCapacity) {
            if (newCapacity <= m_capacity) return;
            
            const float growthFactor = Platform::IsMobile() ? 1.5f : 2.0f;
            newCapacity = static_cast<usize>(m_capacity * growthFactor);
            ReallocateExact(newCapacity);
        }

    public:
        static constexpr usize npos = static_cast<usize>(-1);

        // Itérateurs
        class Iterator {
            CharType* ptr;
        public:
            using iterator_category = std::random_access_iterator_tag;
            using value_type = CharType;
            using difference_type = std::ptrdiff_t;
            using pointer = CharType*;
            using reference = CharType&;

            explicit Iterator(pointer p) : ptr(p) {}

            reference operator*() const { return *ptr; }
            pointer operator->() { return ptr; }
            
            Iterator& operator++() { ++ptr; return *this; }
            Iterator operator++(int) { Iterator tmp = *this; ++ptr; return tmp; }
            
            friend NKENTSEU_FORCE_INLINE bool operator==(const Iterator& a, const Iterator& b) { return a.ptr == b.ptr; }
            friend NKENTSEU_FORCE_INLINE bool operator!=(const Iterator& a, const Iterator& b) { return a.ptr != b.ptr; }
        };

        class ConstIterator {
            const CharType* ptr;
        public:
            using iterator_category = std::random_access_iterator_tag;
            using value_type = const CharType;
            using difference_type = std::ptrdiff_t;
            using pointer = const CharType*;
            using reference = const CharType&;

            explicit ConstIterator(pointer p) : ptr(p) {}

            reference operator*() const { return *ptr; }
            pointer operator->() { return ptr; }
            
            ConstIterator& operator++() { ++ptr; return *this; }
            ConstIterator operator++(int) { ConstIterator tmp = *this; ++ptr; return tmp; }
            
            friend NKENTSEU_FORCE_INLINE bool operator==(const ConstIterator& a, const ConstIterator& b) { return a.ptr == b.ptr; }
            friend NKENTSEU_FORCE_INLINE bool operator!=(const ConstIterator& a, const ConstIterator& b) { return a.ptr != b.ptr; }
        };

        class ReverseIterator {
            CharType* ptr;
        public:
            using iterator_category = std::bidirectional_iterator_tag;
            using value_type = CharType;
            using difference_type = std::ptrdiff_t;
            using pointer = CharType*;
            using reference = CharType&;
    
            explicit ReverseIterator(pointer p) : ptr(p) {}
    
            reference operator*() const { return *ptr; }
            pointer operator->() { return ptr; }
            
            ReverseIterator& operator++() { --ptr; return *this; }
            ReverseIterator operator++(int) { ReverseIterator tmp = *this; --ptr; return tmp; }
            
            friend NKENTSEU_FORCE_INLINE bool operator==(const ReverseIterator& a, const ReverseIterator& b) { return a.ptr == b.ptr; }
            friend NKENTSEU_FORCE_INLINE bool operator!=(const ReverseIterator& a, const ReverseIterator& b) { return a.ptr != b.ptr; }
        };

        class ConstReverseIterator {
            const CharType* ptr;
        public:
            using iterator_category = std::bidirectional_iterator_tag;
            using value_type = const CharType;
            using difference_type = std::ptrdiff_t;
            using pointer = const CharType*;
            using reference = const CharType&;
    
            explicit ConstReverseIterator(pointer p) : ptr(p) {}
    
            reference operator*() const { return *ptr; }
            pointer operator->() { return ptr; }
            
            ConstReverseIterator& operator++() { --ptr; return *this; }
            ConstReverseIterator operator++(int) { ConstReverseIterator tmp = *this; --ptr; return tmp; }
            
            friend NKENTSEU_FORCE_INLINE bool operator==(const ConstReverseIterator& a, const ConstReverseIterator& b) { return a.ptr == b.ptr; }
            friend NKENTSEU_FORCE_INLINE bool operator!=(const ConstReverseIterator& a, const ConstReverseIterator& b) { return a.ptr != b.ptr; }
        };

        // Iterators
        Iterator begin() noexcept { return Iterator(m_data); }
        Iterator end() noexcept { return Iterator(m_data + m_length); }
        ConstIterator Cbegin() const noexcept { return ConstIterator(m_data); }
        ConstIterator Cend() const noexcept { return ConstIterator(m_data + m_length); }
        ReverseIterator Rbegin() noexcept { return ReverseIterator(m_data + m_length - 1); }
        ReverseIterator Rend() noexcept { return ReverseIterator(m_data - 1); }
        ConstReverseIterator Crbegin() const noexcept { return ConstReverseIterator(m_data + m_length - 1); }
        ConstReverseIterator Crend() const noexcept { return ConstReverseIterator(m_data - 1); }

        StringBase() = default;

        explicit StringBase(const CharType* str) {
            if (!str) {
                logger.Fatal("Null pointer in string constructor");
                NKENTSEU_DEBUG_BREAK();
            }
            m_length = std::char_traits<CharType>::length(str);
            ReserveAligned(m_length + 1);
            std::memcpy(m_data, str, m_length * sizeof(CharType));
            m_data[m_length] = '\0';
        }        

        StringBase(const CharType* str, usize length) {
            if (!str) {
                logger.Fatal("Null pointer in string constructor");
                NKENTSEU_DEBUG_BREAK();
            }
            m_length = length;
            ReserveAligned(m_length + 1);
            std::memcpy(m_data, str, length * sizeof(CharType));
            m_data[m_length] = '\0';
        }        

        StringBase(const StringBase& other) {
            m_length = other.m_length;
            if (m_length >= SSO_CAPACITY) {
                ReallocateExact(other.m_capacity);
                std::memcpy(m_data, other.m_data, (m_length + 1) * sizeof(CharType));
            } else {
                std::memcpy(m_sso, other.m_sso, SSO_CAPACITY);
                m_data = m_sso;
            }
            m_capacity = other.m_capacity;
        }        

        StringBase(StringBase&& other) noexcept {
            if (other.IsSSO()) {
                std::memcpy(m_sso, other.m_sso, SSO_CAPACITY);
                m_data = m_sso;
            } else {
                m_data = other.m_data;
            }
            m_length = other.m_length;
            m_capacity = other.m_capacity;
        
            other.m_data = other.m_sso;
            other.m_length = 0;
            other.m_capacity = SSO_CAPACITY;
        }        

        ~StringBase() {
            if(!IsSSO() && m_data) {
                m_allocator.Deallocate(m_data);
            }
        }

        StringBase& operator=(const StringBase& other) {
            if (this != &other) {
                StringBase temp(other);
                Swap(temp);
            }
            return *this;
        }

        StringBase& operator=(StringBase&& other) noexcept {
            if (this != &other) {
                if (!IsSSO() && m_data) {
                    m_allocator.Deallocate(m_data);
                }
        
                m_allocator = std::move(other.m_allocator);
        
                if (other.IsSSO()) {
                    std::memcpy(m_sso, other.m_sso, SSO_CAPACITY);
                    m_data = m_sso;
                } else {
                    m_data = other.m_data;
                }
        
                m_length = other.m_length;
                m_capacity = other.m_capacity;
        
                other.m_data = other.m_sso;
                other.m_length = 0;
                other.m_capacity = SSO_CAPACITY;
            }
            return *this;
        }        

        NKENTSEU_FORCE_INLINE StringBase& operator+=(const StringBase& rhs) {
            return Append(rhs.m_data, rhs.m_length);
        }

        NKENTSEU_FORCE_INLINE StringBase& operator+=(StringBase&& rhs) {
            ReserveAligned(m_length + rhs.m_length);
            std::move(rhs.m_data, rhs.m_data + rhs.m_length, m_data + m_length);
            m_length += rhs.m_length;
            m_data[m_length] = 0;
            return *this;
        }

        NKENTSEU_FORCE_INLINE StringBase operator+(const StringBase& rhs) const {
            StringBase result(*this);
            result += rhs;
            return result;
        }

        NKENTSEU_FORCE_INLINE StringBase operator+(StringBase&& rhs) const {
            StringBase result;
            result.ReserveAligned(m_length + rhs.m_length);
        
            std::memcpy(result.m_data, m_data, m_length);                          // Copier *this
            std::memcpy(result.m_data + m_length, rhs.m_data, rhs.m_length);      // Ajouter rhs
        
            result.m_length = m_length + rhs.m_length;
            result.m_data[result.m_length] = '\0'; // Assurer la terminaison
        
            return result;
        }        

        NKENTSEU_FORCE_INLINE bool operator==(const StringBase& other) const {
            return m_length == other.m_length && 
                   std::equal(m_data, m_data + m_length, other.m_data);
        }

        NKENTSEU_FORCE_INLINE CharType& operator[](usize index) {
            if(index >= m_length) HandleOutOfBounds();
            return m_data[index];
        }

        NKENTSEU_FORCE_INLINE const CharType& operator[](usize index) const { 
            if(index >= m_length) HandleOutOfBounds();
            return m_data[index];
        }

        // template<typename CharType, typename Allocator>
        // std::ostream& operator<<(std::ostream& os, const StringBase<CharType, Allocator>& str) {
        //     return os.write(reinterpret_cast<const char*>(str.Data()), str.Length());
        // }

        // template<typename CharType, typename Allocator>
        // std::ostream& operator>>(std::ostream& os, StringBase<CharType, Allocator>& str) {
        //     std::basic_string<CharType> tmp;
        //     is >> tmp;
        //     str = StringBase(tmp.c_str(), tmp.length());
        //     return is;
        // }

        void Swap(StringBase& other) noexcept {
            if (IsSSO() && other.IsSSO()) {
                std::swap_ranges(m_sso, m_sso + SSO_CAPACITY, other.m_sso);
            }
            else if (IsSSO()) {
                CharType temp[SSO_CAPACITY];
                std::memcpy(temp, m_sso, SSO_CAPACITY);
                std::memcpy(m_sso, other.m_data, SSO_CAPACITY);
                other.m_data = other.m_sso;
                std::memcpy(other.m_sso, temp, SSO_CAPACITY);
                m_data = m_sso;
            }
            else if (other.IsSSO()) {
                CharType temp[SSO_CAPACITY];
                std::memcpy(temp, other.m_sso, SSO_CAPACITY);
                std::memcpy(other.m_sso, m_data, SSO_CAPACITY);
                m_data = m_sso;
                std::memcpy(m_sso, temp, SSO_CAPACITY);
                other.m_data = other.m_sso;
            }
            else {
                std::swap(m_data, other.m_data);
            }
        
            std::swap(m_length, other.m_length);
            std::swap(m_capacity, other.m_capacity);
        }        

        StringBase& Append(const CharType* str, usize length) {
            if (length == 0) return *this;
            ReserveAligned(m_length + length + 1);
            std::memcpy(m_data + m_length, str, length * sizeof(CharType));
            m_length += length;
            m_data[m_length] = '\0';
            return *this;
        }        

        // STL-like interface
        usize Find(CharType ch, usize start = 0) const {
            for(usize i = start; i < m_length; ++i) {
                if(m_data[i] == ch) return i;
            }
            return npos;
        }

        StringBase Substr(usize start, usize length = npos) const {
            length = std::min(length, m_length - start);
            return StringBase(m_data + start, length);
        }

        bool StartsWith(const StringBase& prefix) const {
            return m_length >= prefix.m_length && 
                   std::equal(prefix.m_data, prefix.m_data + prefix.m_length, m_data);
        }

        bool EndsWith(const StringBase& suffix) const {
            return m_length >= suffix.m_length && 
                   std::equal(suffix.m_data, suffix.m_data + suffix.m_length, 
                             m_data + m_length - suffix.m_length);
        }

        size_t Hash() const noexcept {
            return std::hash<std::string_view>{}(
                {reinterpret_cast<const char*>(m_data), m_length * sizeof(CharType)});
        }

        // UTF-8 Support
        class Utf8View {
            const StringBase& str;
        public:
            class Iterator {
                const CharType* ptr;
                uint32_t current;
            public:
                Iterator(const CharType* p) : ptr(p), current(0) { ++*this; }
                
                NKENTSEU_FORCE_INLINE uint32_t operator*() const { return current; }
                
                NKENTSEU_FORCE_INLINE Iterator& operator++() {
                    // UTF-8 decoding logic
                    return *this;
                }
                
                NKENTSEU_FORCE_INLINE bool operator!=(const Iterator& other) const { return ptr != other.ptr; }
            };
            
            Utf8View(const StringBase& s) : str(s) {}
            Iterator begin() const { return Iterator(str.m_data); }
            Iterator end() const { return Iterator(str.m_data + str.m_length); }
        };

        Utf8View AsUtf8() const { return Utf8View(*this); }

        // Accessors
        const CharType* Data() const noexcept { return m_data; }
        usize Length() const noexcept { return m_length; }
        usize Capacity() const noexcept { return m_capacity; }
        bool Empty() const noexcept { return m_length == 0; }

        void Clear() {
            if(!IsSSO() && m_data) {
                m_allocator.Deallocate(m_data);
                m_data = m_sso;
            }
            m_length = 0;
            m_data[0] = 0;
        }

        void ShrinkToFit() {
            if(m_capacity > m_length + 1) {
                ReallocateExact(m_length + 1);
            }
        }

    private:
        void HandleOutOfBounds() const {
            logger.Fatal("String index out of bounds");
            NKENTSEU_DEBUG_BREAK();
        }
    };

} // namespace nkentseu