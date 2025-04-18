#include "pch.h"
#include "StringAllocator.h"

// #include "Nkentseu/Platform/Platform.h"
// #include "Nkentseu/Logger/Logger.h"
// #include "Nkentseu/Platform/Assertion.h"

// #include <malloc.h>

// namespace nkentseu {

//     void* StringAllocator::AllocateFromOS(usize size) {
//         void* ptr = AlignedMalloc(ALIGNMENT, size);
//         if(!ptr) {
//             logger.Fatal("Memory allocation failed");
//             NKENTSEU_DEBUG_BREAK();
//         }
//         return ptr;
//     }

//     void* StringAllocator::Allocate(usize size) {
//         std::lock_guard<std::mutex> lock(poolMutex);
        
//         // Recherche dans le pool
//         for(auto& block : memoryPool) {
//             if(!block.used && block.size >= size) {
//                 block.used = true;
//                 return block.ptr;
//             }
//         }

//         // Nouvelle allocation si pool plein
//         if(totalAllocated + size > MAX_POOL_SIZE) {
//             return AllocateFromOS(size);
//         }

//         // Création nouveau bloc
//         void* newPtr = AllocateFromOS(POOL_BLOCK_SIZE);
//         memoryPool.push_back({newPtr, POOL_BLOCK_SIZE, true});
//         totalAllocated += POOL_BLOCK_SIZE;
//         return newPtr;
//     }

//     void StringAllocator::Deallocate(void* ptr) {
//         std::lock_guard<std::mutex> lock(poolMutex);
        
//         for(auto& block : memoryPool) {
//             if(block.ptr == ptr) {
//                 block.used = false;
//                 return;
//             }
//         }
        
//         // Si pas dans le pool, libération directe
//         AlignedFree(ptr);
//     }

// }