// -----------------------------------------------------------------------------
// FILE: NKMemory/NkGlobalOperators.cpp
// DESCRIPTION: Global operator new/delete overrides with sized deallocation.
//
// DESIGN:
//   - Routes through raw malloc/free to avoid SIOF (Static Initialization
//     Order Fiasco): these operators can be called before any global objects
//     in the project are initialized.
//   - Sized operator delete (C++14) calls free_sized (C23) when available,
//     telling the OS/runtime the exact block size for O(1) freelist lookup.
//   - Compile this file into any executable that wants sized delete support.
//     Do NOT compile it into static libraries (causes ODR violations).
//
// PLATFORMS: All (Win32, Linux, WASM, Android)
// -----------------------------------------------------------------------------

#include <new>
#include <cstdlib>
#include <cstddef>

#if !defined(_WIN32) && !defined(_WIN64) && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#  include <stdlib.h>  // free_sized (C23)
#  define NK_HAS_FREE_SIZED 1
#else
#  define NK_HAS_FREE_SIZED 0
#endif

// ---------------------------------------------------------------------------
// Scalar new / delete
// ---------------------------------------------------------------------------

void* operator new(std::size_t size) {
    if (size == 0u) { size = 1u; }
    void* ptr = std::malloc(size);
    if (!ptr) { throw std::bad_alloc{}; }
    return ptr;
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    if (size == 0u) { size = 1u; }
    return std::malloc(size);
}

void operator delete(void* ptr) noexcept {
    std::free(ptr);
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept {
    std::free(ptr);
}

// C++14 sized scalar delete — compiler calls this when it knows the size.
void operator delete(void* ptr, std::size_t size) noexcept {
#if NK_HAS_FREE_SIZED
    ::free_sized(ptr, size);
#else
    (void)size;
    std::free(ptr);
#endif
}

// ---------------------------------------------------------------------------
// Array new[] / delete[]
// ---------------------------------------------------------------------------

void* operator new[](std::size_t size) {
    if (size == 0u) { size = 1u; }
    void* ptr = std::malloc(size);
    if (!ptr) { throw std::bad_alloc{}; }
    return ptr;
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    if (size == 0u) { size = 1u; }
    return std::malloc(size);
}

void operator delete[](void* ptr) noexcept {
    std::free(ptr);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
    std::free(ptr);
}

// C++14 sized array delete — compiler calls this when it knows the size.
void operator delete[](void* ptr, std::size_t size) noexcept {
#if NK_HAS_FREE_SIZED
    ::free_sized(ptr, size);
#else
    (void)size;
    std::free(ptr);
#endif
}

#undef NK_HAS_FREE_SIZED
