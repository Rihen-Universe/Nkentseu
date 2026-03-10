// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\CacheFriendly\NkPool.h
// DESCRIPTION: Object pool - Fast fixed-size allocation
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_CACHEFRIENDLY_NKPOOL_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_CACHEFRIENDLY_NKPOOL_H_INCLUDED

#include "NKContainers/NkCompat.h"
#include "NKCore/NkTypes.h"
#include "NKCore/NkExport.h"
#include "NKCore/NkTraits.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"
#include "NKCore/Assert/NkAssert.h"

namespace nkentseu {
    
        
        /**
         * @brief Object pool - Fast fixed-size memory pool
         * 
         * Pré-alloue un bloc de mémoire et gère des objets de taille fixe.
         * Extrêmement rapide pour allocation/déallocation répétées.
         * 
         * Caractéristiques:
         * - O(1) allocation/déallocation
         * - Pas de fragmentation
         * - Cache-friendly (mémoire contiguë)
         * - Thread-safe possible (avec mutex)
         * 
         * Complexité:
         * - Allocate: O(1)
         * - Deallocate: O(1)
         * - Construction: O(capacity)
         * 
         * @example
         * NkPool<Particle> pool(1000);
         * Particle* p = pool.Allocate();
         * new (p) Particle();
         * // ... use particle
         * p->~Particle();
         * pool.Deallocate(p);
         */
        template<typename T, typename Allocator = memory::NkAllocator>
        class NkPool {
        public:
            using ValueType = T;
            using SizeType = usize;
            
        private:
            // Free list node
            union Node {
                T Object;
                Node* Next;
                
                Node() {}
                ~Node() {}
            };
            
            Node* mBlocks;          // Memory blocks
            Node* mFreeList;        // Head of free list
            SizeType mCapacity;
            SizeType mAllocated;
            Allocator* mAllocator;
            
        public:
            // ========================================
            // CONSTRUCTORS
            // ========================================
            
            /**
             * @brief Construct pool with capacity
             */
            explicit NkPool(SizeType capacity)
                : mBlocks(nullptr)
                , mFreeList(nullptr)
                , mCapacity(capacity)
                , mAllocated(0)
                , mAllocator(&memory::NkGetDefaultAllocator()) {
                NK_ASSERT(capacity > 0);
                
                // Allocate block
                mBlocks = static_cast<Node*>(mAllocator->Allocate(capacity * sizeof(Node)));
                
                // Initialize free list
                for (SizeType i = 0; i < capacity - 1; ++i) {
                    mBlocks[i].Next = &mBlocks[i + 1];
                }
                mBlocks[capacity - 1].Next = nullptr;
                
                mFreeList = mBlocks;
            }
            
            explicit NkPool(SizeType capacity, Allocator* allocator)
                : mBlocks(nullptr)
                , mFreeList(nullptr)
                , mCapacity(capacity)
                , mAllocated(0)
                , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
                NK_ASSERT(capacity > 0);
                
                mBlocks = static_cast<Node*>(mAllocator->Allocate(capacity * sizeof(Node)));
                
                for (SizeType i = 0; i < capacity - 1; ++i) {
                    mBlocks[i].Next = &mBlocks[i + 1];
                }
                mBlocks[capacity - 1].Next = nullptr;
                
                mFreeList = mBlocks;
            }
            
            /**
             * @brief Destructor - does NOT call destructors on objects
             * User must explicitly destroy all objects before pool destruction
             */
            ~NkPool() {
                if (mBlocks) {
                    mAllocator->Deallocate(mBlocks);
                }
            }
            
            // Non-copyable
            NkPool(const NkPool&) = delete;
            NkPool& operator=(const NkPool&) = delete;
            
            // ========================================
            // ALLOCATION
            // ========================================
            
            /**
             * @brief Allocate object from pool
             * Returns nullptr if pool is full
             * Does NOT construct the object - use placement new
             */
            T* Allocate() {
                if (mFreeList == nullptr) {
                    return nullptr;  // Pool exhausted
                }
                
                Node* node = mFreeList;
                mFreeList = mFreeList->Next;
                ++mAllocated;
                
                return &node->Object;
            }
            
            /**
             * @brief Deallocate object back to pool
             * Does NOT destroy the object - call destructor first
             */
            void Deallocate(T* ptr) {
                NK_ASSERT(ptr != nullptr);
                NK_ASSERT(mAllocated > 0);
                NK_ASSERT(ptr >= &mBlocks[0].Object && ptr <= &mBlocks[mCapacity - 1].Object);
                
                Node* node = reinterpret_cast<Node*>(ptr);
                node->Next = mFreeList;
                mFreeList = node;
                --mAllocated;
            }
            
            // ========================================
            // HELPERS (C++11+)
            // ========================================
            
            #if defined(NK_CPP11)
            /**
             * @brief Construct object in pool
             */
            template<typename... Args>
            T* Construct(Args&&... args) {
                T* ptr = Allocate();
                if (ptr) {
                    new (ptr) T(traits::NkForward<Args>(args)...);
                }
                return ptr;
            }
            
            /**
             * @brief Destroy object and return to pool
             */
            void Destroy(T* ptr) {
                if (ptr) {
                    ptr->~T();
                    Deallocate(ptr);
                }
            }
            #else
            /**
             * @brief Construct object with default constructor
             */
            T* Construct() {
                T* ptr = Allocate();
                if (ptr) {
                    new (ptr) T();
                }
                return ptr;
            }
            
            /**
             * @brief Construct object with copy
             */
            T* Construct(const T& value) {
                T* ptr = Allocate();
                if (ptr) {
                    new (ptr) T(value);
                }
                return ptr;
            }
            
            /**
             * @brief Destroy object
             */
            void Destroy(T* ptr) {
                if (ptr) {
                    ptr->~T();
                    Deallocate(ptr);
                }
            }
            #endif
            
            // ========================================
            // CAPACITY
            // ========================================
            
            SizeType Capacity() const NK_NOEXCEPT { return mCapacity; }
            SizeType Allocated() const NK_NOEXCEPT { return mAllocated; }
            SizeType Available() const NK_NOEXCEPT { return mCapacity - mAllocated; }
            bool IsFull() const NK_NOEXCEPT { return mAllocated >= mCapacity; }
            bool Empty() const NK_NOEXCEPT { return mAllocated == 0; }
            
            // ========================================
            // UTILITIES
            // ========================================
            
            /**
             * @brief Check if pointer belongs to this pool
             */
            bool Owns(const T* ptr) const NK_NOEXCEPT {
                return ptr >= &mBlocks[0].Object && ptr <= &mBlocks[mCapacity - 1].Object;
            }
            
            /**
             * @brief Reset pool - deallocate all without destruction
             * WARNING: Does not call destructors!
             */
            void Reset() {
                // Rebuild free list
                for (SizeType i = 0; i < mCapacity - 1; ++i) {
                    mBlocks[i].Next = &mBlocks[i + 1];
                }
                mBlocks[mCapacity - 1].Next = nullptr;
                
                mFreeList = mBlocks;
                mAllocated = 0;
            }
            
            /**
             * @brief Clear pool - destroy all objects
             * WARNING: Assumes all objects were constructed with Construct()
             */
            void Clear() {
                // Can't destroy objects we don't track
                // User must call Destroy() on all objects explicitly
                Reset();
            }
        };
        
    
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_CACHEFRIENDLY_NKPOOL_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// 
// USAGE EXAMPLE:
// 
// struct Particle {
//     float x, y, z;
//     float vx, vy, vz;
// };
// 
// NkPool<Particle> pool(1000);
// 
// // Allocate and construct
// Particle* p = pool.Construct();
// p->x = 1.0f;
// 
// // Or manual
// Particle* p2 = pool.Allocate();
// new (p2) Particle{0, 0, 0, 1, 1, 1};
// 
// // Destroy
// pool.Destroy(p);
// 
// // Or manual
// p2->~Particle();
// pool.Deallocate(p2);
// 
// PERFORMANCE:
// - Allocate: O(1) - just pop from free list
// - Deallocate: O(1) - just push to free list
// - No malloc/free calls after construction
// - No fragmentation
// - Cache-friendly
// 
// THREAD SAFETY:
// - Not thread-safe by default
// - Wrap Allocate/Deallocate with mutex if needed
// - Or use one pool per thread
// 
// ============================================================

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
