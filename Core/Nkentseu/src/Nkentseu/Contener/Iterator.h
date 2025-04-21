#pragma once

// #include <iterator>
#include <cstddef>

namespace nkentseu
{
    template<typename Type>
    class Iterator {
    public:
        // using iterator_category = std::random_access_iterator_tag;
        using value_type        = Type;
        using difference_type   = std::ptrdiff_t;
        using pointer           = Type*;
        using reference         = Type&;

        Iterator() : m_Ptr(nullptr) {}
        explicit Iterator(Type* ptr) : m_Ptr(ptr) {}

        // Opérations de base
        reference operator*() const { return *m_Ptr; }
        pointer operator->() const { return m_Ptr; }
        
        // Incrémentation
        Iterator& operator++() { ++m_Ptr; return *this; }
        Iterator operator++(int) { Iterator tmp(*this); ++m_Ptr; return tmp; }
        
        // Décrémentation
        Iterator& operator--() { --m_Ptr; return *this; }
        Iterator operator--(int) { Iterator tmp(*this); --m_Ptr; return tmp; }
        
        // Arithmétique
        Iterator operator+(difference_type n) const { return Iterator(m_Ptr + n); }
        Iterator operator-(difference_type n) const { return Iterator(m_Ptr - n); }
        difference_type operator-(const Iterator& other) const { return m_Ptr - other.m_Ptr; }
        Iterator& operator+=(difference_type n) { m_Ptr += n; return *this; }
        Iterator& operator-=(difference_type n) { m_Ptr -= n; return *this; }
        
        // Comparaison
        bool operator==(const Iterator& other) const { return m_Ptr == other.m_Ptr; }
        bool operator!=(const Iterator& other) const { return m_Ptr != other.m_Ptr; }
        bool operator<(const Iterator& other) const { return m_Ptr < other.m_Ptr; }
        bool operator>(const Iterator& other) const { return m_Ptr > other.m_Ptr; }
        bool operator<=(const Iterator& other) const { return m_Ptr <= other.m_Ptr; }
        bool operator>=(const Iterator& other) const { return m_Ptr >= other.m_Ptr; }
        
        // Accès aléatoire
        reference operator[](difference_type n) { return m_Ptr[n]; }

    private:
        Type* m_Ptr;
    };

    template<typename Type>
    class ConstIterator {
    public:
        // using iterator_category = std::random_access_iterator_tag;
        using value_type        = const Type;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const Type*;
        using reference         = const Type&;

        ConstIterator() : m_Ptr(nullptr) {}
        explicit ConstIterator(const Type* ptr) : m_Ptr(ptr) {}
        ConstIterator(const Iterator<Type>& other) : m_Ptr(other.operator->()) {} // Conversion implicite

        reference operator*() const { return *m_Ptr; }
        pointer operator->() const { return m_Ptr; }
        
        ConstIterator& operator++() { ++m_Ptr; return *this; }
        ConstIterator operator++(int) { ConstIterator tmp(*this); ++m_Ptr; return tmp; }
        
        ConstIterator& operator--() { --m_Ptr; return *this; }
        ConstIterator operator--(int) { ConstIterator tmp(*this); --m_Ptr; return tmp; }
        
        ConstIterator operator+(difference_type n) const { return ConstIterator(m_Ptr + n); }
        ConstIterator operator-(difference_type n) const { return ConstIterator(m_Ptr - n); }
        difference_type operator-(const ConstIterator& other) const { return m_Ptr - other.m_Ptr; }
        ConstIterator& operator+=(difference_type n) { m_Ptr += n; return *this; }
        ConstIterator& operator-=(difference_type n) { m_Ptr -= n; return *this; }
        
        bool operator==(const ConstIterator& other) const { return m_Ptr == other.m_Ptr; }
        bool operator!=(const ConstIterator& other) const { return m_Ptr != other.m_Ptr; }
        bool operator<(const ConstIterator& other) const { return m_Ptr < other.m_Ptr; }
        bool operator>(const ConstIterator& other) const { return m_Ptr > other.m_Ptr; }
        bool operator<=(const ConstIterator& other) const { return m_Ptr <= other.m_Ptr; }
        bool operator>=(const ConstIterator& other) const { return m_Ptr >= other.m_Ptr; }
        
        reference operator[](difference_type n) { return m_Ptr[n]; }

    private:
        const Type* m_Ptr;
    };

    template<typename Type>
    class ReverseIterator {
    public:
        // using iterator_category = std::random_access_iterator_tag;
        using value_type        = Type;
        using difference_type   = std::ptrdiff_t;
        using pointer           = Type*;
        using reference         = Type&;

        ReverseIterator() : m_Ptr(nullptr) {}
        explicit ReverseIterator(Type* ptr) : m_Ptr(ptr) {}

        reference operator*() const { return *m_Ptr; }
        pointer operator->() const { return m_Ptr; }
        
        ReverseIterator& operator++() { --m_Ptr; return *this; }
        ReverseIterator operator++(int) { ReverseIterator tmp(*this); --m_Ptr; return tmp; }
        
        ReverseIterator& operator--() { ++m_Ptr; return *this; }
        ReverseIterator operator--(int) { ReverseIterator tmp(*this); ++m_Ptr; return tmp; }
        
        ReverseIterator operator+(difference_type n) const { return ReverseIterator(m_Ptr - n); }
        ReverseIterator operator-(difference_type n) const { return ReverseIterator(m_Ptr + n); }
        difference_type operator-(const ReverseIterator& other) const { return other.m_Ptr - m_Ptr; }
        ReverseIterator& operator+=(difference_type n) { m_Ptr -= n; return *this; }
        ReverseIterator& operator-=(difference_type n) { m_Ptr += n; return *this; }
        
        bool operator==(const ReverseIterator& other) const { return m_Ptr == other.m_Ptr; }
        bool operator!=(const ReverseIterator& other) const { return m_Ptr != other.m_Ptr; }
        bool operator<(const ReverseIterator& other) const { return m_Ptr > other.m_Ptr; }
        bool operator>(const ReverseIterator& other) const { return m_Ptr < other.m_Ptr; }
        bool operator<=(const ReverseIterator& other) const { return m_Ptr >= other.m_Ptr; }
        bool operator>=(const ReverseIterator& other) const { return m_Ptr <= other.m_Ptr; }
        
        reference operator[](difference_type n) { return *(m_Ptr - n); }

        // Conversion vers ConstReverseIterator
        // operator ConstReverseIterator() const { return ConstReverseIterator(m_Ptr); }

    private:
        Type* m_Ptr;
    };

    template<typename Type>
    class ConstReverseIterator {
    public:
        // using iterator_category = std::random_access_iterator_tag;
        using value_type        = const Type;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const Type*;
        using reference         = const Type&;

        ConstReverseIterator() : m_Ptr(nullptr) {}
        explicit ConstReverseIterator(const Type* ptr) : m_Ptr(ptr) {}
        ConstReverseIterator(const ReverseIterator<Type>& other) : m_Ptr(other.operator->()) {}

        reference operator*() const { return *m_Ptr; }
        pointer operator->() const { return m_Ptr; }
        
        ConstReverseIterator& operator++() { --m_Ptr; return *this; }
        ConstReverseIterator operator++(int) { ConstReverseIterator tmp(*this); --m_Ptr; return tmp; }
        
        ConstReverseIterator& operator--() { ++m_Ptr; return *this; }
        ConstReverseIterator operator--(int) { ConstReverseIterator tmp(*this); ++m_Ptr; return tmp; }
        
        ConstReverseIterator operator+(difference_type n) const { return ConstReverseIterator(m_Ptr - n); }
        ConstReverseIterator operator-(difference_type n) const { return ConstReverseIterator(m_Ptr + n); }
        difference_type operator-(const ConstReverseIterator& other) const { return other.m_Ptr - m_Ptr; }
        ConstReverseIterator& operator+=(difference_type n) { m_Ptr -= n; return *this; }
        ConstReverseIterator& operator-=(difference_type n) { m_Ptr += n; return *this; }
        
        bool operator==(const ConstReverseIterator& other) const { return m_Ptr == other.m_Ptr; }
        bool operator!=(const ConstReverseIterator& other) const { return m_Ptr != other.m_Ptr; }
        bool operator<(const ConstReverseIterator& other) const { return m_Ptr > other.m_Ptr; }
        bool operator>(const ConstReverseIterator& other) const { return m_Ptr < other.m_Ptr; }
        bool operator<=(const ConstReverseIterator& other) const { return m_Ptr >= other.m_Ptr; }
        bool operator>=(const ConstReverseIterator& other) const { return m_Ptr <= other.m_Ptr; }
        
        reference operator[](difference_type n) { return *(m_Ptr - n); }

    private:
        const Type* m_Ptr;
    };
} // namespace nkentseu
