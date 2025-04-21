#pragma once

#include "Nkentseu/Platform/Types.h"
#include "Nkentseu/Platform/Export.h"
#include <iostream>
#include <mutex>
#include <string>
#include <iomanip>

#ifdef __linux__
#include <sys/mman.h>
#elif _WIN32
#include <windows.h>
#endif

namespace nkentseu {
    // Configuration constants
    constexpr usize NKENTSEU_MEMORY_ALIGNMENT = 16;
    constexpr usize NKENTSEU_PAGE_SIZE = 16384; // 16KB
    constexpr usize NKENTSEU_SMALL_ALLOC_MAX = 1024;
    constexpr usize NKENTSEU_POOL_COUNT = 16;

    // ANSI color codes
    #define COLOR_RED     "\033[1;31m"
    #define COLOR_YELLOW  "\033[1;33m"
    #define COLOR_GREEN   "\033[1;32m"
    #define COLOR_BLUE    "\033[1;34m"
    #define COLOR_RESET   "\033[0m"

    struct BlockHeader {
        usize size;
        BlockHeader* next;
        BlockHeader* prev;
        bool isFree;
        bool isArray;
        usize elementCount;
        const char* filename;
        const char* function;
        int line;
    };

    class NKENTSEU_API MemoryAllocator {
    private:
        std::mutex mutex_;
        BlockHeader* globalListHead = nullptr;
        BlockHeader* poolFreeLists[NKENTSEU_POOL_COUNT] = { nullptr };

        // Alignement mémoire
        static usize Align(usize size) {
            return (size + NKENTSEU_MEMORY_ALIGNMENT - 1) & ~(NKENTSEU_MEMORY_ALIGNMENT - 1);
        }

        // Allocation système
        void* SystemAlloc(usize size) {
        #ifdef __linux__
            void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, 
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            return ptr == MAP_FAILED ? nullptr : ptr;
        #elif _WIN32
            return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        #endif
        }

        void SystemFree(void* ptr, usize size) {
        #ifdef __linux__
            munmap(ptr, size);
        #elif _WIN32
            VirtualFree(ptr, 0, MEM_RELEASE);
        #endif
        }

        BlockHeader* CreatePool(usize blockSize, usize count) {
            const usize totalSize = Align(sizeof(BlockHeader) + blockSize) * count;
            void* memory = SystemAlloc(totalSize);
            
            BlockHeader* first = nullptr;
            char* current = static_cast<char*>(memory);
            
            for (usize i = 0; i < count; ++i) {
                BlockHeader* header = reinterpret_cast<BlockHeader*>(current);
                header->size = blockSize;
                header->isFree = true;
                header->next = (i == count - 1) ? nullptr : 
                    reinterpret_cast<BlockHeader*>(current + Align(sizeof(BlockHeader) + blockSize));
                
                if (!first) first = header;
                current += Align(sizeof(BlockHeader) + blockSize);
            }
            
            return first;
        }

        void AddToGlobalList(BlockHeader* header) {
            header->prev = nullptr;
            header->next = globalListHead;
            if (globalListHead) globalListHead->prev = header;
            globalListHead = header;
        }

        void RemoveFromGlobalList(BlockHeader* header) {
            if (header->prev) header->prev->next = header->next;
            if (header->next) header->next->prev = header->prev;
            if (header == globalListHead) globalListHead = header->next;
        }

        MemoryAllocator() {
            // Initialisation des pools
            for (usize i = 0; i < NKENTSEU_POOL_COUNT; ++i) {
                usize blockSize = (i + 1) * NKENTSEU_MEMORY_ALIGNMENT;
                poolFreeLists[i] = CreatePool(blockSize, NKENTSEU_PAGE_SIZE / blockSize);
            }
        }

    public:

        ~MemoryAllocator() {
            ReportLeaks();
        }

        template<typename T, typename... Args>
        T* Allocate(const char* file, int line, const char* func, Args&&... args) {
            std::lock_guard<std::mutex> lock(mutex_);
            
            constexpr usize typeSize = sizeof(T);
            BlockHeader* header = nullptr;

            if (typeSize <= NKENTSEU_SMALL_ALLOC_MAX) {
                const usize poolIndex = (typeSize + NKENTSEU_MEMORY_ALIGNMENT - 1) / NKENTSEU_MEMORY_ALIGNMENT - 1;
                
                if (!poolFreeLists[poolIndex]) {
                    poolFreeLists[poolIndex] = CreatePool(
                        (poolIndex + 1) * NKENTSEU_MEMORY_ALIGNMENT,
                        NKENTSEU_PAGE_SIZE / ((poolIndex + 1) * NKENTSEU_MEMORY_ALIGNMENT)
                    );
                }

                header = poolFreeLists[poolIndex];
                poolFreeLists[poolIndex] = header->next;
            } else {
                const usize totalSize = Align(sizeof(BlockHeader) + typeSize);
                header = static_cast<BlockHeader*>(SystemAlloc(totalSize));
                header->size = totalSize;
            }

            header->isFree = false;
            header->filename = file;
            header->line = line;
            header->function = func;
            header->isArray = false;

            AddToGlobalList(header);

            T* obj = new (header + 1) T(std::forward<Args>(args)...);
            return obj;
        }

        template<typename T>
        void Deallocate(T* ptr) {
            if (!ptr) return;

            std::lock_guard<std::mutex> lock(mutex_);
            BlockHeader* header = reinterpret_cast<BlockHeader*>(ptr) - 1;
            
            ptr->~T();

            if (header->size <= NKENTSEU_SMALL_ALLOC_MAX) {
                const usize poolIndex = (header->size + NKENTSEU_MEMORY_ALIGNMENT - 1) / NKENTSEU_MEMORY_ALIGNMENT - 1;
                header->next = poolFreeLists[poolIndex];
                poolFreeLists[poolIndex] = header;
                header->isFree = true;
            } else {
                RemoveFromGlobalList(header);
                SystemFree(header, header->size);
            }
        }

        void ReportLeaks() {
            std::lock_guard<std::mutex> lock(mutex_);
            BlockHeader* current = globalListHead;
            bool leaksFound = false;

            std::cout << COLOR_BLUE "\n[Memory Leak Report]\n" COLOR_RESET;

            while (current) {
                if (!current->isFree) {
                    leaksFound = true;
                    std::cout << COLOR_RED "LEAK: " COLOR_RESET
                              << current->size << " bytes at "
                              << COLOR_YELLOW << current->filename << ":" 
                              << current->line << COLOR_RESET
                              << " (" << current->function << ")\n";
                }
                current = current->next;
            }

            if (!leaksFound) {
                std::cout << COLOR_GREEN "No memory leaks detected!\n" COLOR_RESET;
            }
        }

        MemoryAllocator& GetAllocator() {
            static MemoryAllocator instance;
            return instance;
        }
    };

    // Helper macros
    #define NK_ALLOCATE(T, ...) nkentseu::GetAllocator().Allocate<T>(__FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
    #define NK_DEALLOCATE(ptr) nkentseu::GetAllocator().Deallocate(ptr)
}