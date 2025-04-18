#pragma once

#include "Nkentseu/Platform/Export.h"
#include "Nkentseu/Platform/Types.h"

namespace nkentseu {
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
        BasicStringIterator operator++(int32) { 
            BasicStringIterator tmp = *this; 
            ++m_Ptr; 
            return tmp; 
        }

        BasicStringIterator& operator--() { --m_Ptr; return *this; }
        BasicStringIterator operator--(int32) { 
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
        
        ReverseBasicStringIterator operator++(int32) {
            ReverseBasicStringIterator tmp = *this;
            --m_Current;
            return tmp;
        }

        ReverseBasicStringIterator& operator--() { ++m_Current; return *this; }
        ReverseBasicStringIterator operator--(int32) { 
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
}