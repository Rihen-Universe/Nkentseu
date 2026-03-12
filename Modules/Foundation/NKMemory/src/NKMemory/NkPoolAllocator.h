// ============================================================
// FILE: NkPoolAllocator.h
// DESCRIPTION: High-performance pool allocator (production-ready)
// AUTHOR: Rihen
// DATE: 2026-03-05
// VERSION: 1.0.0
// ============================================================
// Allocateur pool optimisé pour:
// - Zéro fragmentation
// - O(1) allocation/deallocation
// - Cache-friendly (blocs adjacents)
// - Deterministic performance
// ============================================================

#pragma once

#ifndef NKENTSEU_MEMORY_NKPOOLALLOCATOR_H_INCLUDED
#define NKENTSEU_MEMORY_NKPOOLALLOCATOR_H_INCLUDED

#include "NKMemory/NkAllocator.h"
#include "NKCore/NkTypes.h"
#include "NKCore/NkAtomic.h"
#include "NKMemory/NkExport.h"

#include <new>

#ifndef NKENTSEU_STATIC_ASSERT
#define NKENTSEU_STATIC_ASSERT(condition, message) static_assert((condition), message)
#endif

namespace nkentseu {
namespace memory {

    /**
     * @brief Fixed-size pool allocator (O(1) allocation/deallocation)
     * 
     * Alloue des blocs de taille fixe à partir d'un pool pré-alloué.
     * Idéal pour allocations répétées du même type.
     * 
     * Avantages:
     * - O(1) allocation et deallocation
     * - Zéro fragmentation (blocs adjacents)
     * - Déterministe (pas d'appels OS)
     * - Cache-friendly (pool compact)
     * 
     * Utilisation:
     * - Petit objets fréquents (< numéros de particules, bullets)
     * - Allocations temporaires dans boucles critiques
     * - Message queues (taille fixe)
     * 
     * @example
     * ```cpp
     * // Pool de 1000 objets de 64 bytes
     * NkFixedPoolAllocator<64, 1000> pool;
     * 
     * void* ptr = pool.Allocate();  // O(1)
     * // ...
     * pool.Deallocate(ptr);          // O(1)
     * ```
     */
    template<nk_size BlockSize, nk_size NumBlocks = 256>
    class NKENTSEU_MEMORY_API NkFixedPoolAllocator : public NkAllocator {
    public:
        NKENTSEU_STATIC_ASSERT(BlockSize >= sizeof(nk_size), "BlockSize too small");
        NKENTSEU_STATIC_ASSERT(NumBlocks > 0, "NumBlocks must be > 0");
        
        // ==============================================
        // CONSTRUCTEUR/DESTRUCTEUR
        // ==============================================
        
        explicit NkFixedPoolAllocator(const nk_char* name = "NkFixedPoolAllocator") 
            : NkAllocator(name),
              mBlocks(nullptr),
              mFreeList(nullptr),
              mNumFree(NumBlocks) {
            
            // Pré-allouer le pool entier en une seule allocation OS
            try {
                mBlocks = static_cast<nk_uint8*>(::operator new(BlockSize * NumBlocks));
            } catch (...) {
                mBlocks = nullptr;
            }
            
            if (!mBlocks) {
                return;
            }
            
            // Construire la linked-list de blocs libres
            for (nk_size i = 0; i < NumBlocks; ++i) {
                nk_uint8* block = mBlocks + (i * BlockSize);
                // Stocker le pointeur vers le prochain bloc libre
                nk_uint8** ptrLoc = reinterpret_cast<nk_uint8**>(block);
                *ptrLoc = (i + 1 < NumBlocks) ? mBlocks + ((i + 1) * BlockSize) : nullptr;
            }
            
            mFreeList = mBlocks;
        }
        
        ~NkFixedPoolAllocator() override {
            if (mBlocks) {
                ::operator delete(mBlocks);
                mBlocks = nullptr;
            }
        }
        
        // Non-copiable
        NkFixedPoolAllocator(const NkFixedPoolAllocator&) = delete;
        NkFixedPoolAllocator& operator=(const NkFixedPoolAllocator&) = delete;
        
        // ==============================================
        // ALLOCATOR API
        // ==============================================
        
        Pointer Allocate(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override {
            if (size > BlockSize || !mFreeList) {
                return nullptr;
            }
            
            NkScopedSpinLock guard(mLock);
            
            if (!mFreeList) {
                return nullptr;
            }
            
            // Pop from free list (head)
            nk_uint8* block = mFreeList;
            nk_uint8** ptrLoc = reinterpret_cast<nk_uint8**>(block);
            mFreeList = *ptrLoc;
            mNumFree--;
            
            return static_cast<Pointer>(block);
        }
        
        void Deallocate(Pointer ptr) override {
            if (!ptr) {
                return;
            }
            
            NkScopedSpinLock guard(mLock);
            
            // Push to free list (head)
            nk_uint8* block = static_cast<nk_uint8*>(ptr);
            nk_uint8** ptrLoc = reinterpret_cast<nk_uint8**>(block);
            *ptrLoc = mFreeList;
            mFreeList = block;
            mNumFree++;
        }
        
        void Reset() noexcept override {
            NkScopedSpinLock guard(mLock);
            
            // Rebuild free list
            for (nk_size i = 0; i < NumBlocks; ++i) {
                nk_uint8* block = mBlocks + (i * BlockSize);
                nk_uint8** ptrLoc = reinterpret_cast<nk_uint8**>(block);
                *ptrLoc = (i + 1 < NumBlocks) ? mBlocks + ((i + 1) * BlockSize) : nullptr;
            }
            
            mFreeList = mBlocks;
            mNumFree = NumBlocks;
        }
        
        /**
         * @brief Obtient le nombre de blocs libres
         */
        [[nodiscard]] nk_size GetNumFreeBlocks() const noexcept {
            NkScopedSpinLock guard(mLock);
            return mNumFree;
        }
        
        [[nodiscard]] nk_size GetNumBlocks() const noexcept {
            return NumBlocks;
        }
        
        [[nodiscard]] nk_size GetBlockSize() const noexcept {
            return BlockSize;
        }
        
        [[nodiscard]] bool Owns(Pointer ptr) const noexcept {
            if (!ptr || !mBlocks) {
                return false;
            }
            const nk_uint8* p = static_cast<const nk_uint8*>(ptr);
            return p >= mBlocks && p < (mBlocks + (BlockSize * NumBlocks));
        }
        
        /**
         * @brief Obtient l'utilisation (0.0 = vide, 1.0 = plein)
         */
        [[nodiscard]] float32 GetUsage() const noexcept {
            NkScopedSpinLock guard(mLock);
            return static_cast<float32>(NumBlocks - mNumFree) / static_cast<float32>(NumBlocks);
        }
        
    private:
        nk_uint8* mBlocks;
        nk_uint8* mFreeList;
        nk_size mNumFree;
        mutable NkSpinLock mLock;
    };
    
    // =======================================================
    // VARIABLE-SIZE POOL ALLOCATOR
    // =======================================================
    
    /**
     * @brief Pool allocator pour tailles variées
     * 
     * Maintient plusieurs pools internes pour différentes tailles.
     * Bonne pour allocations hétérogènes.
     */
    class NKENTSEU_MEMORY_API NkVariablePoolAllocator : public NkAllocator {
    public:
        explicit NkVariablePoolAllocator(const nk_char* name = "NkVariablePoolAllocator")
            : NkAllocator(name)
            , mHead(nullptr)
            , mLiveBytes(0u)
            , mLiveAllocations(0u) {}
        
        Pointer Allocate(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
        void Deallocate(Pointer ptr) override;
        void Reset() noexcept override;
        
        [[nodiscard]] nk_size GetLiveBytes() const noexcept {
            NkScopedSpinLock guard(mLock);
            return mLiveBytes;
        }
        
        [[nodiscard]] nk_size GetLiveAllocations() const noexcept {
            NkScopedSpinLock guard(mLock);
            return mLiveAllocations;
        }
        
        [[nodiscard]] bool Owns(Pointer ptr) const noexcept;
        
    private:
        struct Header {
            nk_uint64 magic;
            nk_size requestedSize;
            nk_size offsetToBase;
            Header* prev;
            Header* next;
        };
        
        Header* mHead;
        nk_size mLiveBytes;
        nk_size mLiveAllocations;
        mutable NkSpinLock mLock;
    };

} // namespace memory
} // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKPOOLALLOCATOR_H_INCLUDED
