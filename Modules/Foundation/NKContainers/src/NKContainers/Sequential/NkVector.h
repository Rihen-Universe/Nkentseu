// -----------------------------------------------------------------------------
// FICHIER: NKContainers/Sequential/NkVector.h
// DESCRIPTION: Dynamic array container - 100% STL-free
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.1.0
//
// CHANGEMENTS v1.1.0:
//   - Ajout des itérateurs PascalCase Begin()/End()/CBegin()/CEnd() ainsi que
//     leurs variantes inverses RBegin()/REnd()/CRBegin()/CREnd().
//     Ces méthodes sont de simples aliases vers begin()/end() existants.
//     Requis par NkVulkanContext et NkDX12Context qui utilisent Begin().
//   - Ajout de IsEmpty() PascalCase (alias de Empty()).
//   - Erase(ConstIterator) existant est compatible avec Begin()+i sans
//     modification — Begin() et begin() retournent le même pointeur.
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_SEQUENTIAL_NKVECTOR_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_SEQUENTIAL_NKVECTOR_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/NkContainersExport.h"
#include "NKCore/NkTraits.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKContainers/Iterators/NkIterator.h"
#include "NKContainers/Iterators/NkInitializerList.h"
#include "NkVectorError.h"

namespace nkentseu {

    /**
     * @brief Tableau dynamique — conteneur séquentiel principal du framework.
     *
     * Nommage : les méthodes PascalCase (Begin, End, Erase…) sont le standard
     * interne. Les alias minuscules (begin, end) sont maintenus pour la
     * compatibilité avec le range-based for et les algorithmes génériques.
     *
     * Complexité :
     *   - Accès aléatoire : O(1)
     *   - PushBack        : O(1) amorti
     *   - Insert / Erase  : O(n)
     *
     * @example
     *   NkVector<int> v = {1, 2, 3};
     *   v.PushBack(4);
     *   v.Erase(v.Begin() + 1);   // Supprime l'index 1
     *   for (auto& x : v) { ... } // Range-for compatible
     */
    template<typename T, typename Allocator = memory::NkAllocator>
    class NkVector {
        public:

            // ── Types membres ──────────────────────────────────────────────────────

            using ValueType            = T;
            using AllocatorType        = Allocator;
            using SizeType             = usize;
            using DifferenceType       = intptr;
            using Reference            = T&;
            using ConstReference       = const T&;
            using Pointer              = T*;
            using ConstPointer         = const T*;
            using Iterator             = T*;
            using ConstIterator        = const T*;
            using ReverseIterator      = NkReverseIterator<Iterator>;
            using ConstReverseIterator = NkReverseIterator<ConstIterator>;

        private:

            T*         mData;
            SizeType   mSize;
            SizeType   mCapacity;
            Allocator* mAllocator;

            // Facteur de croissance ×2 — moins de réallocations que 1.5×
            SizeType CalculateGrowth(SizeType needed) const {
                SizeType cur = mCapacity;
                SizeType max = MaxSize();
                if (cur > max / 2) { return max; }
                SizeType geometric = cur * 2;
                return (geometric < needed) ? needed : geometric;
            }

            void DestroyRange(T* first, T* last) {
                for (T* it = first; it != last; ++it) {
                    it->~T();
                }
            }

            template<typename... Args>
            void ConstructAt(T* ptr, Args&&... args) {
    #if defined(NK_CPP11)
                new (ptr) T(traits::NkForward<Args>(args)...);
    #else
                new (ptr) T(args...);
    #endif
            }

            void MoveOrCopyRange(T* dest, T* src, SizeType count) {
                if (__is_trivial(T)) {
                    memory::NkCopy(dest, src, count * sizeof(T));
                } else {
                    for (SizeType i = 0; i < count; ++i) {
                        ConstructAt(dest + i, traits::NkMove(src[i]));
                    }
                }
            }

            void Reallocate(SizeType newCapacity) {
                if (newCapacity == 0) {
                    Clear();
                    if (mData) {
                        mAllocator->Deallocate(mData);
                        mData = nullptr;
                    }
                    mCapacity = 0;
                    return;
                }

                T* newData = static_cast<T*>(
                    mAllocator->Allocate(newCapacity * sizeof(T), alignof(T)));

                if (!newData) {
                    NK_VECTOR_THROW_BAD_ALLOC(newCapacity);
                    return;
                }

                if (mData) {
                    MoveOrCopyRange(newData, mData, mSize);
                    DestroyRange(mData, mData + mSize);
                    mAllocator->Deallocate(mData);
                }

                mData     = newData;
                mCapacity = newCapacity;
            }

        public:

            // ── Constructeurs ──────────────────────────────────────────────────────

            NkVector()
                : mData(nullptr)
                , mSize(0)
                , mCapacity(0)
                , mAllocator(&memory::NkGetDefaultAllocator()) {}

            explicit NkVector(Allocator* allocator)
                : mData(nullptr)
                , mSize(0)
                , mCapacity(0)
                , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {}

            explicit NkVector(SizeType count)
                : mData(nullptr)
                , mSize(0)
                , mCapacity(0)
                , mAllocator(&memory::NkGetDefaultAllocator()) {
                Resize(count);
            }

            NkVector(SizeType count, const T& value)
                : mData(nullptr)
                , mSize(0)
                , mCapacity(0)
                , mAllocator(&memory::NkGetDefaultAllocator()) {
                Resize(count, value);
            }

            NkVector(NkInitializerList<T> init)
                : mData(nullptr)
                , mSize(0)
                , mCapacity(0)
                , mAllocator(&memory::NkGetDefaultAllocator()) {
                Reserve(init.Size());
                for (auto it = init.Begin(); it != init.End(); ++it) {
                    PushBack(*it);
                }
            }

            NkVector(std::initializer_list<T> init)
                : mData(nullptr)
                , mSize(0)
                , mCapacity(0)
                , mAllocator(&memory::NkGetDefaultAllocator()) {
                Reserve(init.size());
                for (auto& val : init) { PushBack(val); }
            }

            NkVector(const T& value)
                : mData(nullptr)
                , mSize(0)
                , mCapacity(0)
                , mAllocator(&memory::NkGetDefaultAllocator()) {
                Reserve(1);
                PushBack(value);
            }

            NkVector(const NkVector& other)
                : mData(nullptr)
                , mSize(0)
                , mCapacity(0)
                , mAllocator(other.mAllocator) {
                Reserve(other.mSize);
                for (SizeType i = 0; i < other.mSize; ++i) {
                    PushBack(other.mData[i]);
                }
            }

    #if defined(NK_CPP11)
            NkVector(NkVector&& other) NK_NOEXCEPT
                : mData(other.mData)
                , mSize(other.mSize)
                , mCapacity(other.mCapacity)
                , mAllocator(other.mAllocator) {
                other.mData     = nullptr;
                other.mSize     = 0;
                other.mCapacity = 0;
            }
    #endif

            ~NkVector() {
                Clear();
                if (mData) {
                    mAllocator->Deallocate(mData);
                }
            }

            // ── Affectation ────────────────────────────────────────────────────────

            void Assign(const T& value, SizeType count) {
                Clear();
                Reserve(count);
                for (SizeType i = 0; i < count; ++i) { PushBack(value); }
            }

            void Assign(const T* values, SizeType count) {
                Clear();
                Reserve(count);
                for (SizeType i = 0; i < count; ++i) { PushBack(values[i]); }
            }

            void Assign(NkInitializerList<T> init) {
                Clear();
                Reserve(init.Size());
                for (auto& val : init) { PushBack(val); }
            }

            void Assign(const NkVector& other) {
                if (this != &other) {
                    Clear();
                    Reserve(other.mSize);
                    for (SizeType i = 0; i < other.mSize; ++i) { PushBack(other.mData[i]); }
                }
            }

            NkVector& operator=(const NkVector& other) {
                if (this != &other) {
                    Clear();
                    Reserve(other.mSize);
                    for (SizeType i = 0; i < other.mSize; ++i) { PushBack(other.mData[i]); }
                }
                return *this;
            }

    #if defined(NK_CPP11)
            NkVector& operator=(NkVector&& other) NK_NOEXCEPT {
                if (this != &other) {
                    Clear();
                    if (mData) { mAllocator->Deallocate(mData); }
                    mData      = other.mData;
                    mSize      = other.mSize;
                    mCapacity  = other.mCapacity;
                    mAllocator = other.mAllocator;
                    other.mData     = nullptr;
                    other.mSize     = 0;
                    other.mCapacity = 0;
                }
                return *this;
            }
    #endif

            NkVector& operator=(NkInitializerList<T> init) {
                Clear();
                Reserve(init.Size());
                for (auto& val : init) { PushBack(val); }
                return *this;
            }

            NkVector& operator=(std::initializer_list<T> init) {
                Clear();
                Reserve(init.size());
                for (auto& val : init) { PushBack(val); }
                return *this;
            }

            // ── Accès aux éléments ─────────────────────────────────────────────────

            Reference At(SizeType index) {
                if (index >= mSize) { NK_VECTOR_THROW_OUT_OF_RANGE(index, mSize); }
                return mData[index];
            }

            ConstReference At(SizeType index) const {
                if (index >= mSize) { NK_VECTOR_THROW_OUT_OF_RANGE(index, mSize); }
                return mData[index];
            }

            Reference      operator[](SizeType index)       { NK_ASSERT(index < mSize); return mData[index]; }
            ConstReference operator[](SizeType index) const { NK_ASSERT(index < mSize); return mData[index]; }

            Reference      Front()       { NK_ASSERT(mSize > 0); return mData[0]; }
            ConstReference Front() const { NK_ASSERT(mSize > 0); return mData[0]; }

            Reference      Back()        { NK_ASSERT(mSize > 0); return mData[mSize - 1]; }
            ConstReference Back()  const { NK_ASSERT(mSize > 0); return mData[mSize - 1]; }

            Pointer      Data()       NK_NOEXCEPT { return mData; }
            ConstPointer Data() const NK_NOEXCEPT { return mData; }

            // ── Itérateurs PascalCase ─────────────────────────────────────────────
            //
            // Standard interne du framework. À utiliser dans tout le code applicatif.

            Iterator             Begin()    NK_NOEXCEPT       { return mData; }
            ConstIterator        Begin()    const NK_NOEXCEPT { return mData; }
            ConstIterator        CBegin()   const NK_NOEXCEPT { return mData; }

            Iterator             End()      NK_NOEXCEPT       { return mData + mSize; }
            ConstIterator        End()      const NK_NOEXCEPT { return mData + mSize; }
            ConstIterator        CEnd()     const NK_NOEXCEPT { return mData + mSize; }

            ReverseIterator      RBegin()   NK_NOEXCEPT       { return ReverseIterator(End()); }
            ConstReverseIterator RBegin()   const NK_NOEXCEPT { return ConstReverseIterator(End()); }
            ConstReverseIterator CRBegin()  const NK_NOEXCEPT { return ConstReverseIterator(End()); }

            ReverseIterator      REnd()     NK_NOEXCEPT       { return ReverseIterator(Begin()); }
            ConstReverseIterator REnd()     const NK_NOEXCEPT { return ConstReverseIterator(Begin()); }
            ConstReverseIterator CREnd()    const NK_NOEXCEPT { return ConstReverseIterator(Begin()); }

            // ── Itérateurs STL minuscules (range-based for) ───────────────────────
            //
            // Aliases vers les méthodes PascalCase — logique non dupliquée.

            Iterator             begin()    NK_NOEXCEPT       { return Begin(); }
            ConstIterator        begin()    const NK_NOEXCEPT { return Begin(); }
            ConstIterator        cbegin()   const NK_NOEXCEPT { return CBegin(); }

            Iterator             end()      NK_NOEXCEPT       { return End(); }
            ConstIterator        end()      const NK_NOEXCEPT { return End(); }
            ConstIterator        cend()     const NK_NOEXCEPT { return CEnd(); }

            ReverseIterator      rbegin()   NK_NOEXCEPT       { return RBegin(); }
            ConstReverseIterator rbegin()   const NK_NOEXCEPT { return RBegin(); }
            ConstReverseIterator crbegin()  const NK_NOEXCEPT { return CRBegin(); }

            ReverseIterator      rend()     NK_NOEXCEPT       { return REnd(); }
            ConstReverseIterator rend()     const NK_NOEXCEPT { return REnd(); }
            ConstReverseIterator crend()    const NK_NOEXCEPT { return CREnd(); }

            // ── Capacité ──────────────────────────────────────────────────────────

            bool     IsEmpty()   const NK_NOEXCEPT { return mSize == 0; }  ///< PascalCase
            bool     Empty()     const NK_NOEXCEPT { return mSize == 0; }  ///< Alias
            bool     empty()     const NK_NOEXCEPT { return mSize == 0; }  ///< STL compat

            SizeType Size()      const NK_NOEXCEPT { return mSize; }
            SizeType size()      const NK_NOEXCEPT { return mSize; }

            SizeType Capacity()  const NK_NOEXCEPT { return mCapacity; }
            SizeType capacity()  const NK_NOEXCEPT { return mCapacity; }

            SizeType MaxSize()   const NK_NOEXCEPT {
                return static_cast<SizeType>(-1) / sizeof(T);
            }

            void Reserve(SizeType newCapacity) {
                if (newCapacity > mCapacity) {
                    Reallocate(newCapacity);
                }
            }

            void ShrinkToFit() {
                if (mSize < mCapacity) {
                    Reallocate(mSize);
                }
            }

            // ── Modificateurs ─────────────────────────────────────────────────────

            void Clear() NK_NOEXCEPT {
                if (mData) {
                    DestroyRange(mData, mData + mSize);
                }
                mSize = 0;
            }

            void PushBack(const T& value) {
                if (mSize >= mCapacity) {
                    Reserve(mCapacity == 0 ? 1 : CalculateGrowth(mSize + 1));
                }
                ConstructAt(mData + mSize, value);
                ++mSize;
            }

    #if defined(NK_CPP11)
            void PushBack(T&& value) {
                if (mSize >= mCapacity) {
                    Reserve(mCapacity == 0 ? 1 : CalculateGrowth(mSize + 1));
                }
                ConstructAt(mData + mSize, traits::NkMove(value));
                ++mSize;
            }

            template<typename... Args>
            void EmplaceBack(Args&&... args) {
                if (mSize >= mCapacity) {
                    Reserve(mCapacity == 0 ? 1 : CalculateGrowth(mSize + 1));
                }
                ConstructAt(mData + mSize, traits::NkForward<Args>(args)...);
                ++mSize;
            }
    #endif

            void PopBack() {
                NK_ASSERT(mSize > 0);
                --mSize;
                mData[mSize].~T();
            }

            void Resize(SizeType newSize) {
                if (newSize > mSize) {
                    Reserve(newSize);
                    for (SizeType i = mSize; i < newSize; ++i) { ConstructAt(mData + i); }
                } else if (newSize < mSize) {
                    DestroyRange(mData + newSize, mData + mSize);
                }
                mSize = newSize;
            }

            void Resize(SizeType newSize, const T& value) {
                if (newSize > mSize) {
                    Reserve(newSize);
                    for (SizeType i = mSize; i < newSize; ++i) { ConstructAt(mData + i, value); }
                } else if (newSize < mSize) {
                    DestroyRange(mData + newSize, mData + mSize);
                }
                mSize = newSize;
            }

            void Swap(NkVector& other) NK_NOEXCEPT {
                traits::NkSwap(mData,      other.mData);
                traits::NkSwap(mSize,      other.mSize);
                traits::NkSwap(mCapacity,  other.mCapacity);
                traits::NkSwap(mAllocator, other.mAllocator);
            }

            // ── Insert ────────────────────────────────────────────────────────────

            Iterator Insert(ConstIterator pos, const T& value) {
                SizeType index = static_cast<SizeType>(pos - Begin());
                NK_ASSERT(index <= mSize);

                if (mSize >= mCapacity) {
                    Reserve(CalculateGrowth(mSize + 1));
                }

                for (SizeType i = mSize; i > index; --i) {
                    ConstructAt(mData + i, traits::NkMove(mData[i - 1]));
                    mData[i - 1].~T();
                }

                ConstructAt(mData + index, value);
                ++mSize;
                return Begin() + index;
            }

            // ── Erase ─────────────────────────────────────────────────────────────
            //
            // Les deux surcharges Erase(ConstIterator) et Erase(ConstIterator, ConstIterator)
            // acceptent un résultat de Begin()+i car Begin() retourne Iterator (= T*)
            // qui est implicitement convertible en ConstIterator (= const T*).

            Iterator Erase(ConstIterator pos) {
                SizeType index = static_cast<SizeType>(pos - Begin());
                NK_ASSERT(index < mSize);

                mData[index].~T();

                for (SizeType i = index; i < mSize - 1; ++i) {
                    ConstructAt(mData + i, traits::NkMove(mData[i + 1]));
                    mData[i + 1].~T();
                }

                --mSize;
                return Begin() + index;
            }

            Iterator Erase(ConstIterator first, ConstIterator last) {
                SizeType firstIndex = static_cast<SizeType>(first - Begin());
                SizeType lastIndex  = static_cast<SizeType>(last  - Begin());
                SizeType count      = lastIndex - firstIndex;
                NK_ASSERT(firstIndex <= lastIndex && lastIndex <= mSize);

                DestroyRange(mData + firstIndex, mData + lastIndex);

                for (SizeType i = lastIndex; i < mSize; ++i) {
                    ConstructAt(mData + firstIndex + (i - lastIndex),
                                traits::NkMove(mData[i]));
                    mData[i].~T();
                }

                mSize -= count;
                return Begin() + firstIndex;
            }

    }; // class NkVector


    // ── Fonctions non-membres ──────────────────────────────────────────────────

    template<typename T, typename Allocator>
    void NkSwap(NkVector<T, Allocator>& lhs, NkVector<T, Allocator>& rhs) NK_NOEXCEPT {
        lhs.Swap(rhs);
    }

    template<typename T, typename Allocator>
    bool operator==(const NkVector<T, Allocator>& lhs, const NkVector<T, Allocator>& rhs) {
        if (lhs.Size() != rhs.Size()) { return false; }
        for (typename NkVector<T, Allocator>::SizeType i = 0; i < lhs.Size(); ++i) {
            if (!(lhs[i] == rhs[i])) { return false; }
        }
        return true;
    }

    template<typename T, typename Allocator>
    bool operator!=(const NkVector<T, Allocator>& lhs, const NkVector<T, Allocator>& rhs) {
        return !(lhs == rhs);
    }

} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_SEQUENTIAL_NKVECTOR_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================