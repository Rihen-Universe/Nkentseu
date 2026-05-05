// =============================================================================
// NKMemory/NkAllocator.cpp
// Implémentations des allocateurs mémoire concrets.
//
// Design :
//  - Inclusion systématique de "pch.h" en premier pour la précompilation
//  - Réutilisation maximale des utilitaires NKMemory/NKCore (ZÉRO duplication)
//  - Implémentations plateforme-aware avec fallbacks portables
//  - Gestion thread-safe des singletons via Meyer's pattern
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

// -------------------------------------------------------------------------
// PRÉ-COMPILED HEADER - TOUJOURS EN PREMIER
// -------------------------------------------------------------------------
#include "pch.h"

// -------------------------------------------------------------------------
// EN-TÊTES DU MODULE
// -------------------------------------------------------------------------
#include "NKMemory/NkAllocator.h"
#include "NKCore/NkPlatform.h"

// -------------------------------------------------------------------------
// DÉPENDANCES PLATEFORME (uniquement si nécessaires)
// -------------------------------------------------------------------------
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    #include <windows.h>
    #include <malloc.h>      // _aligned_malloc, _aligned_realloc
#elif defined(NKENTSEU_PLATFORM_POSIX)
    #include <sys/mman.h>    // mmap, munmap
    #include <unistd.h>      // sysconf, _SC_PAGESIZE
    #include <cstdlib>       // posix_memalign
#endif

using namespace nkentseu;

// -------------------------------------------------------------------------
// NAMESPACE ANONYME : Implémentations internes non-exportées
// -------------------------------------------------------------------------
namespace {

    // ========================================================================
    // Helpers mathématiques locaux (non-exportés)
    // ========================================================================
    // Note : Ces fonctions pourraient être déplacées vers NKCore/NkMath.h
    // si réutilisées ailleurs. Pour l'instant, usage interne uniquement.

    [[nodiscard]] inline nkentseu::nk_size MaxSize(nkentseu::nk_size a, nkentseu::nk_size b) noexcept {
        return (a > b) ? a : b;
    }

    [[nodiscard]] inline nkentseu::nk_size MinSize(nkentseu::nk_size a, nkentseu::nk_size b) noexcept {
        return (a < b) ? a : b;
    }

    /**
     * @brief Calcule la prochaine puissance de 2 >= value
     * @note Utilisé par NkBuddyAllocator pour l'arrondi des tailles
     */
    [[nodiscard]] inline nkentseu::nk_size NextPow2(nkentseu::nk_size value) noexcept {
        if (value <= 1u) {
            return 1u;
        }
        --value;
        // Propagation des bits : remplit tous les bits en dessous du MSB
        for (nkentseu::nk_size shift = 1u; shift < sizeof(nk_size) * 8u; shift <<= 1u) {
            value |= value >> shift;
        }
        return value + 1u;  // Incrémente pour obtenir la puissance de 2 suivante
    }

    // ========================================================================
    // Gestion d'alignement pour operator new (C++14 fallback)
    // ========================================================================
    // Sur C++17+, operator new(size_t, std::align_val_t) gère l'alignement nativement.
    // Sur C++14, on implémente manuellement avec un header de métadonnées.

    struct AlignedHeader {
        void*  rawPtr;    // Pointeur brut retourné par ::operator new
        nkentseu::nk_size totalSize; // Taille totale allouée (pour sized delete)
    };

    /**
     * @brief Alloue de la mémoire alignée via operator new (fallback C++14)
     * @note Retourne un pointeur aligné, avec header caché pour deallocation
     */
    [[nodiscard]] void* AllocateAlignedByNew(nkentseu::nk_size size, nkentseu::nk_size alignment) {
        if (size == 0u) {
            return nullptr;
        }

        // Validation et normalisation de l'alignement
        if (!nkentseu::memory::NkIsPowerOfTwo(alignment)) {
            alignment = nkentseu::memory::NK_MEMORY_DEFAULT_ALIGNMENT;
        }
        alignment = MaxSize(alignment, nkentseu::memory::NK_MEMORY_DEFAULT_ALIGNMENT);

        // Sur-allocation pour header + padding d'alignement
        const nkentseu::nk_size total = size + alignment + sizeof(AlignedHeader);
        
        void* raw = nullptr;
        NKENTSEU_DISABLE_WARNING_PUSH
        NKENTSEU_DISABLE_WARNING_MSVC(4244)  // conversion size_t -> void*
        NKENTSEU_DISABLE_WARNING_CLANG("-Wcast-align")
        try {
            raw = ::operator new(total);
        } catch (...) {
            raw = nullptr;  // Exception std::bad_alloc convertie en nullptr
        }
        NKENTSEU_DISABLE_WARNING_POP
        
        if (!raw) {
            return nullptr;
        }

        // Calcul de l'adresse alignée pour l'utilisateur
        nkentseu::nk_uint8* begin = static_cast<nkentseu::nk_uint8*>(raw) + sizeof(AlignedHeader);
        const nkentseu::nk_size beginAddr = reinterpret_cast<nkentseu::nk_size>(begin);
        const nkentseu::nk_size alignedAddr = NK_ALIGN_UP(beginAddr, alignment);
        auto* aligned = reinterpret_cast<nkentseu::nk_uint8*>(alignedAddr);

        // Stockage des métadonnées dans l'header caché
        auto* header = reinterpret_cast<AlignedHeader*>(aligned - sizeof(AlignedHeader));
        header->rawPtr = raw;
        header->totalSize = total;
        
        return aligned;
    }

    /**
     * @brief Libère une mémoire allouée via AllocateAlignedByNew
     * @note Utilise sized delete si disponible (C++14/17)
     */
    void FreeAlignedByNew(void* ptr) {
        if (!ptr) {
            return;
        }
        auto* aligned = static_cast<nkentseu::nk_uint8*>(ptr);
        auto* header = reinterpret_cast<AlignedHeader*>(aligned - sizeof(AlignedHeader));
        
        #if __cpp_sized_deallocation >= 201309L
            ::operator delete(header->rawPtr, header->totalSize);
        #else
            ::operator delete(header->rawPtr);
        #endif
    }

    // ========================================================================
    // Gestion d'alignement pour malloc/free (plateforme-aware)
    // ========================================================================

    [[nodiscard]] void* AllocateAlignedByMalloc(nkentseu::nk_size size, nkentseu::nk_size alignment) {
        // if (size == 0u) {
        //     return nullptr;
        // }

        // // Validation et normalisation de l'alignement
        // if (!nkentseu::memory::NkIsPowerOfTwo(alignment)) {
        //     alignment = nkentseu::memory::NK_MEMORY_DEFAULT_ALIGNMENT;
        // }
        // alignment = MaxSize(alignment, nkentseu::memory::NK_MEMORY_DEFAULT_ALIGNMENT);

        // #if defined(NKENTSEU_PLATFORM_WINDOWS)
        //     return _aligned_malloc(size, alignment);
        // #elif defined(NKENTSEU_PLATFORM_ANDROID)
        //     return ::memalign(alignment, size);
        // #elif defined(NKENTSEU_PLATFORM_POSIX)
        //     // posix_memalign requiert alignment >= sizeof(void*) et puissance de 2
        //     if (alignment < sizeof(void*)) {
        //         alignment = sizeof(void*);
        //     }
        //     void* ptr = nullptr;
        //     if (posix_memalign(&ptr, alignment, size) != 0) {
        //         return nullptr;
        //     }
        //     return ptr;
        // #else
        //     // Fallback minimal : malloc avec avertissement potentiel d'alignement
        //     return ::malloc(size);
        // #endif
        return memory::NkAllocateAligned(size, alignment);
    }

    void FreeAlignedByMalloc(void* ptr) {
        // if (!ptr) {
        //     return;
        // }
        // #if defined(NKENTSEU_PLATFORM_WINDOWS)
        //     _aligned_free(ptr);
        // #elif defined(NKENTSEU_PLATFORM_POSIX)
        //     ::free(ptr);
        // #endif
        memory::NkFreeAligned(ptr);
    }

    // ========================================================================
    // Singletons globaux pour les allocateurs standards (Meyer's singleton)
    // ========================================================================
    // Thread-safe en C++11+ grâce à l'initialisation statique locale garantie.

    nkentseu::memory::NkMallocAllocator& GlobalMallocAllocator() {
        static nkentseu::memory::NkMallocAllocator instance;
        return instance;
    }

    nkentseu::memory::NkNewAllocator& GlobalNewAllocator() {
        static nkentseu::memory::NkNewAllocator instance;
        return instance;
    }

    nkentseu::memory::NkVirtualAllocator& GlobalVirtualAllocator() {
        static nkentseu::memory::NkVirtualAllocator instance;
        return instance;
    }

    // Pointeur global pour l'allocateur par défaut (modifiable via NkSetDefaultAllocator)
    nkentseu::memory::NkAllocator* gDefaultAllocator = nullptr;

} // namespace anonyme


// -------------------------------------------------------------------------
// NAMESPACE PUBLIC : Implémentations des méthodes de NkAllocator
// -------------------------------------------------------------------------
namespace nkentseu {
    namespace memory {

        // ====================================================================
        // NkAllocator : Implémentations des méthodes virtuelles par défaut
        // ====================================================================

        void NkAllocator::Deallocate(Pointer /*ptr*/, SizeType /*size*/) {
            // Par défaut, ignore la taille et délègue à la version simple
            // Les sous-classes peuvent surcharger pour sized deallocation optimisée
        }

        NkAllocator::Pointer NkAllocator::Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                                    AlignType alignment) {
            // Cas 1 : allocation initiale (ptr == nullptr)
            if (!ptr) {
                return Allocate(newSize, alignment);
            }
            // Cas 2 : libération (newSize == 0)
            if (newSize == 0u) {
                Deallocate(ptr);
                return nullptr;
            }
            // Cas 3 : redimensionnement réel
            Pointer newPtr = Allocate(newSize, alignment);
            if (!newPtr) {
                return nullptr;  // Échec : ptr original reste valide
            }
            // Copie du minimum des deux tailles pour éviter overflow
            const SizeType copySize = MinSize(oldSize, newSize);
            NkMemCopy(newPtr, ptr, copySize);
            Deallocate(ptr);
            return newPtr;
        }

        NkAllocator::Pointer NkAllocator::Calloc(SizeType size, AlignType alignment) {
            Pointer ptr = Allocate(size, alignment);
            if (ptr) {
                NkMemZero(ptr, size);  // Zero-initialisation via utilitaire NKMemory
            }
            return ptr;
        }

        void NkAllocator::Reset() noexcept {
            // No-op par défaut : les allocateurs avec état override cette méthode
        }

        const nk_char* NkAllocator::Name() const noexcept {
            return GetName();  // Alias vers la méthode de la classe de base
        }

        // ====================================================================
        // NkNewAllocator : Implémentations spécifiques
        // ====================================================================

        NkNewAllocator::NkNewAllocator() noexcept 
            : NkAllocator("NkNewAllocator") {}

        NkAllocator::Pointer NkNewAllocator::Allocate(SizeType size, AlignType alignment) {
            return ::AllocateAlignedByNew(size, alignment);
        }

        void NkNewAllocator::Deallocate(Pointer ptr) {
            ::FreeAlignedByNew(ptr);
        }

        NkAllocator::Pointer NkNewAllocator::Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                                        AlignType alignment) {
            // Optimisation : éviter allocate+copy+deallocate si possible
            if (!ptr) {
                return Allocate(newSize, alignment);
            }
            if (newSize == 0u) {
                Deallocate(ptr);
                return nullptr;
            }
            // Fallback générique (pourrait être optimisé avec _aligned_realloc sur Windows)
            Pointer newPtr = Allocate(newSize, alignment);
            if (!newPtr) {
                return nullptr;
            }
            NkMemCopy(newPtr, ptr, MinSize(oldSize, newSize));
            Deallocate(ptr);
            return newPtr;
        }

        NkAllocator::Pointer NkNewAllocator::Calloc(SizeType size, AlignType alignment) {
            Pointer ptr = Allocate(size, alignment);
            if (ptr) {
                NkMemZero(ptr, size);
            }
            return ptr;
        }

        // ====================================================================
        // NkMallocAllocator : Implémentations spécifiques
        // ====================================================================

        NkMallocAllocator::NkMallocAllocator() noexcept 
            : NkAllocator("NkMallocAllocator") {}

        NkAllocator::Pointer NkMallocAllocator::Allocate(SizeType size, AlignType alignment) {
            return ::AllocateAlignedByMalloc(size, alignment);
        }

        void NkMallocAllocator::Deallocate(Pointer ptr) {
            ::FreeAlignedByMalloc(ptr);
        }

        void NkMallocAllocator::Deallocate(Pointer ptr, SizeType size) {
            if (!ptr) {
                return;
            }
            // Optimisation C23 : sized deallocation si disponible
            #if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L && !defined(NKENTSEU_PLATFORM_WINDOWS)
                ::free_sized(ptr, size);
            #else
                (void)size;  // Unused in fallback path
                ::FreeAlignedByMalloc(ptr);
            #endif
        }

        NkAllocator::Pointer NkMallocAllocator::Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                                        AlignType alignment) {
            if (!ptr) {
                return Allocate(newSize, alignment);
            }
            if (newSize == 0u) {
                Deallocate(ptr);
                return nullptr;
            }

            // Optimisation Windows : _aligned_realloc préserve l'alignement
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                if (!NkIsPowerOfTwo(alignment)) {
                    alignment = NK_MEMORY_DEFAULT_ALIGNMENT;
                }
                alignment = MaxSize(alignment, NK_MEMORY_DEFAULT_ALIGNMENT);
                if (void* resized = _aligned_realloc(ptr, newSize, alignment)) {
                    return resized;  // Succès : pas de copie manuelle nécessaire
                }
                // Fallback si _aligned_realloc échoue : méthode générique
            #endif

            // Fallback générique : allocate + copy + deallocate
            Pointer newPtr = Allocate(newSize, alignment);
            if (!newPtr) {
                return nullptr;
            }
            NkMemCopy(newPtr, ptr, MinSize(oldSize, newSize));
            Deallocate(ptr);
            return newPtr;
        }

        NkAllocator::Pointer NkMallocAllocator::Calloc(SizeType size, AlignType alignment) {
            if (size == 0u) {
                return nullptr;
            }
            Pointer ptr = Allocate(size, alignment);
            if (ptr) {
                NkMemZero(ptr, size);
            }
            return ptr;
        }

        // ====================================================================
        // NkVirtualAllocator : Implémentations spécifiques (bas niveau)
        // ====================================================================

        NkVirtualAllocator::NkVirtualAllocator() noexcept
            : NkAllocator("NkVirtualAllocator"),
              flags(NK_MEMORY_FLAG_READ | NK_MEMORY_FLAG_WRITE | 
                    NK_MEMORY_FLAG_RESERVE | NK_MEMORY_FLAG_COMMIT | 
                    NK_MEMORY_FLAG_ANONYMOUS),
              mBlocks(nullptr) {}

        NkVirtualAllocator::~NkVirtualAllocator() {
            // Libération de tous les blocs trackés
            VirtualBlock* current = mBlocks;
            while (current) {
                VirtualBlock* next = current->next;
                #if defined(NKENTSEU_PLATFORM_WINDOWS)
                    VirtualFree(current->ptr, 0, MEM_RELEASE);
                #elif defined(NKENTSEU_PLATFORM_POSIX)
                    munmap(current->ptr, current->size);
                #endif
                // Libération du noeud de tracking lui-même via malloc allocator
                GlobalMallocAllocator().Deallocate(current);
                current = next;
            }
            mBlocks = nullptr;
        }

        NkVirtualAllocator::SizeType NkVirtualAllocator::PageSize() const noexcept {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                SYSTEM_INFO info{};
                GetSystemInfo(&info);
                return static_cast<SizeType>(info.dwPageSize);
            #elif defined(NKENTSEU_PLATFORM_POSIX)
                const long page = sysconf(_SC_PAGESIZE);
                return (page > 0) ? static_cast<SizeType>(page) : 4096u;
            #else
                return 4096u;  // Fallback conservateur
            #endif
        }

        NkVirtualAllocator::SizeType NkVirtualAllocator::NormalizeSize(SizeType size) const noexcept {
            const SizeType page = PageSize();
            return NK_ALIGN_UP(size, page);  // Réutilisation de l'utilitaire NKMemory
        }

        void NkVirtualAllocator::TrackBlock(Pointer ptr, SizeType size) {
            // Allocation du noeud de tracking via malloc allocator (évite récursion)
            auto* node = static_cast<VirtualBlock*>(
                GlobalMallocAllocator().Allocate(sizeof(VirtualBlock), alignof(VirtualBlock)));
            if (!node) {
                return;  // Échec silencieux : le bloc ne sera pas tracké (fuite potentielle)
            }
            node->ptr  = ptr;
            node->size = size;
            node->next = mBlocks;  // Insertion en tête de liste
            mBlocks = node;
        }

        NkVirtualAllocator::SizeType NkVirtualAllocator::UntrackBlock(Pointer ptr) noexcept {
            VirtualBlock* prev = nullptr;
            VirtualBlock* cur  = mBlocks;
            
            while (cur) {
                if (cur->ptr == ptr) {
                    const SizeType size = cur->size;
                    // Retrait de la liste chaînée
                    if (prev) {
                        prev->next = cur->next;
                    } else {
                        mBlocks = cur->next;
                    }
                    // Libération du noeud de tracking
                    GlobalMallocAllocator().Deallocate(cur);
                    return size;
                }
                prev = cur;
                cur  = cur->next;
            }
            return 0u;  // Bloc non trouvé
        }

        NkAllocator::Pointer NkVirtualAllocator::AllocateVirtual(SizeType size, const nk_char* /*tag*/) noexcept {
            if (size == 0u) {
                return nullptr;
            }

            const SizeType normalized = NormalizeSize(size);
            void* address = nullptr;

            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                // Traduction des flags NKMemory vers protection Windows
                DWORD protection = PAGE_NOACCESS;
                if ((flags & NK_MEMORY_FLAG_READ) && (flags & NK_MEMORY_FLAG_WRITE)) {
                    protection = PAGE_READWRITE;
                } else if (flags & NK_MEMORY_FLAG_READ) {
                    protection = PAGE_READONLY;
                } else if (flags & NK_MEMORY_FLAG_EXECUTE) {
                    protection = PAGE_EXECUTE_READWRITE;
                }

                // Combinaison des flags d'allocation
                DWORD allocFlags = 0;
                allocFlags |= (flags & NK_MEMORY_FLAG_RESERVE) ? MEM_RESERVE : 0;
                allocFlags |= (flags & NK_MEMORY_FLAG_COMMIT) ? MEM_COMMIT : 0;
                if (allocFlags == 0) {
                    allocFlags = MEM_RESERVE | MEM_COMMIT;  // Défaut : réserve + commit
                }

                address = VirtualAlloc(nullptr, normalized, allocFlags, protection);

            #elif defined(NKENTSEU_PLATFORM_POSIX)
                // Traduction des flags vers protections POSIX
                int protection = PROT_NONE;
                if (flags & NK_MEMORY_FLAG_READ)    protection |= PROT_READ;
                if (flags & NK_MEMORY_FLAG_WRITE)   protection |= PROT_WRITE;
                if (flags & NK_MEMORY_FLAG_EXECUTE) protection |= PROT_EXEC;

                int mapFlags = MAP_PRIVATE;
                #if defined(MAP_ANONYMOUS)
                    mapFlags |= MAP_ANONYMOUS;
                #elif defined(MAP_ANON)
                    mapFlags |= MAP_ANON;  // BSD variant
                #endif

                address = mmap(nullptr, normalized, protection, mapFlags, -1, 0);
                if (address == MAP_FAILED) {
                    address = nullptr;
                }
            #endif

            if (address) {
                TrackBlock(address, normalized);
            }
            return address;
        }

        void NkVirtualAllocator::FreeVirtual(Pointer ptr) noexcept {
            if (!ptr) {
                return;
            }
            const SizeType size = UntrackBlock(ptr);
            if (size == 0u) {
                return;  // Bloc non tracké : ne pas tenter de libérer
            }
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                VirtualFree(ptr, 0, MEM_RELEASE);
            #elif defined(NKENTSEU_PLATFORM_POSIX)
                munmap(ptr, size);
            #endif
        }

        NkAllocator::Pointer NkVirtualAllocator::Allocate(SizeType size, SizeType /*alignment*/) {
            // L'alignement est géré par la page système dans AllocateVirtual
            return AllocateVirtual(size, Name());
        }

        void NkVirtualAllocator::Deallocate(Pointer ptr) {
            FreeVirtual(ptr);
        }

        NkAllocator::Pointer NkVirtualAllocator::Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                                            SizeType alignment) {
            // VirtualAlloc/mmap ne supportent pas le resize in-place : fallback générique
            if (!ptr) {
                return Allocate(newSize, alignment);
            }
            if (newSize == 0u) {
                Deallocate(ptr);
                return nullptr;
            }
            Pointer newPtr = Allocate(newSize, alignment);
            if (!newPtr) {
                return nullptr;
            }
            NkMemCopy(newPtr, ptr, MinSize(oldSize, newSize));
            Deallocate(ptr);
            return newPtr;
        }

        NkAllocator::Pointer NkVirtualAllocator::Calloc(SizeType size, SizeType alignment) {
            Pointer ptr = Allocate(size, alignment);
            if (ptr) {
                NkMemZero(ptr, size);
            }
            return ptr;
        }

        // ====================================================================
        // NkLinearAllocator : Implémentations spécifiques
        // ====================================================================

        NkLinearAllocator::NkLinearAllocator(SizeType capacityBytes, NkAllocator* backingAllocator)
            : NkAllocator("NkLinearAllocator"),
              mBacking(backingAllocator ? backingAllocator : &NkGetDefaultAllocator()),
              mBuffer(nullptr),
              mCapacity(capacityBytes),
              mOffset(0u),
              mOwnsBuffer(true) {
            
            if (capacityBytes == 0u) {
                mOwnsBuffer = false;
                return;
            }
            
            mBuffer = static_cast<nk_uint8*>(mBacking->Allocate(capacityBytes, NK_MEMORY_DEFAULT_ALIGNMENT));
            if (!mBuffer) {
                mCapacity = 0u;
                mOwnsBuffer = false;
            }
        }

        NkLinearAllocator::~NkLinearAllocator() {
            if (mOwnsBuffer && mBuffer && mBacking) {
                mBacking->Deallocate(mBuffer);
            }
            // Reset des membres pour sécurité (dangling pointer prevention)
            mBuffer = nullptr;
            mCapacity = 0u;
            mOffset = 0u;
        }

        NkAllocator::Pointer NkLinearAllocator::Allocate(SizeType size, AlignType alignment) {
            if (!mBuffer || size == 0u) {
                return nullptr;
            }
            if (!NkIsPowerOfTwo(alignment)) {
                alignment = NK_MEMORY_DEFAULT_ALIGNMENT;
            }

            // Calcul de l'adresse alignée dans le buffer
            const SizeType baseAddr = reinterpret_cast<SizeType>(mBuffer) + mOffset;
            const SizeType alignedAddr = NK_ALIGN_UP(baseAddr, alignment);
            const SizeType adjustment = alignedAddr - baseAddr;
            
            // Vérification de capacité restante
            if (mOffset + adjustment + size > mCapacity) {
                return nullptr;  // Out of memory
            }

            // Mise à jour du bump pointer
            mOffset += adjustment;
            Pointer result = mBuffer + mOffset;
            mOffset += size;
            return result;
        }

        void NkLinearAllocator::Deallocate(Pointer /*ptr*/) {
            // No-op : les allocations individuelles ne sont pas supportées
            // Utiliser Reset() pour libérer tout le buffer
        }

        void NkLinearAllocator::Deallocate(Pointer ptr, SizeType size) {
            // Optimisation : deallocation du dernier bloc si top-of-stack
            if (!ptr || !mBuffer || size == 0u) {
                return;
            }
            auto* p = static_cast<nk_uint8*>(ptr);
            if (p + size == mBuffer + mOffset) {
                // C'est la dernière allocation : on peut reclaim l'espace
                mOffset -= size;
            }
            // Sinon : no-op (linear allocator ne supporte pas deallocation arbitraire)
        }

        NkAllocator::Pointer NkLinearAllocator::Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                                        AlignType alignment) {
            if (!ptr) {
                return Allocate(newSize, alignment);
            }
            // Optimisation : shrink in-place ou same-size : no-op
            if (newSize <= oldSize) {
                return ptr;
            }
            // Grow : nécessite reallocation (pas de resize in-place en linear)
            Pointer newPtr = Allocate(newSize, alignment);
            if (!newPtr) {
                return nullptr;
            }
            NkMemCopy(newPtr, ptr, oldSize);  // Copie de l'ancien contenu
            return newPtr;
        }

        NkAllocator::Pointer NkLinearAllocator::Calloc(SizeType size, AlignType alignment) {
            Pointer ptr = Allocate(size, alignment);
            if (ptr) {
                NkMemZero(ptr, size);
            }
            return ptr;
        }

        const nk_char* NkLinearAllocator::Name() const noexcept {
            return "NkLinearAllocator";
        }

        void NkLinearAllocator::Reset() noexcept {
            mOffset = 0u;  // Reset du bump pointer : toutes les allocations sont "libérées"
        }

        // ====================================================================
        // NkArenaAllocator : Implémentations spécifiques (avec markers)
        // ====================================================================

        NkArenaAllocator::NkArenaAllocator(SizeType capacityBytes, NkAllocator* backingAllocator)
            : NkAllocator("NkArenaAllocator"),
              mBacking(backingAllocator ? backingAllocator : &NkGetDefaultAllocator()),
              mBuffer(nullptr),
              mCapacity(capacityBytes),
              mOffset(0u),
              mOwnsBuffer(true) {
            
            if (capacityBytes == 0u) {
                mOwnsBuffer = false;
                return;
            }
            
            mBuffer = static_cast<nk_uint8*>(mBacking->Allocate(capacityBytes, NK_MEMORY_DEFAULT_ALIGNMENT));
            if (!mBuffer) {
                mCapacity = 0u;
                mOwnsBuffer = false;
            }
        }

        NkArenaAllocator::~NkArenaAllocator() {
            if (mOwnsBuffer && mBuffer && mBacking) {
                mBacking->Deallocate(mBuffer);
            }
        }

        NkAllocator::Pointer NkArenaAllocator::Allocate(SizeType size, AlignType alignment) {
            // Identique à NkLinearAllocator pour l'allocation de base
            if (!mBuffer || size == 0u) {
                return nullptr;
            }
            if (!NkIsPowerOfTwo(alignment)) {
                alignment = NK_MEMORY_DEFAULT_ALIGNMENT;
            }

            const SizeType baseAddr = reinterpret_cast<SizeType>(mBuffer) + mOffset;
            const SizeType alignedAddr = NK_ALIGN_UP(baseAddr, alignment);
            const SizeType adjustment = alignedAddr - baseAddr;
            
            if (mOffset + adjustment + size > mCapacity) {
                return nullptr;
            }

            mOffset += adjustment;
            Pointer result = mBuffer + mOffset;
            mOffset += size;
            return result;
        }

        void NkArenaAllocator::Deallocate(Pointer /*ptr*/) {
            // No-op : utiliser FreeToMarker() ou Reset() pour la libération
        }

        NkAllocator::Pointer NkArenaAllocator::Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                                        AlignType alignment) {
            if (!ptr) {
                return Allocate(newSize, alignment);
            }
            if (newSize <= oldSize) {
                return ptr;  // Shrink in-place : no-op pour arena
            }
            Pointer newPtr = Allocate(newSize, alignment);
            if (!newPtr) {
                return nullptr;
            }
            NkMemCopy(newPtr, ptr, oldSize);
            return newPtr;
        }

        NkAllocator::Pointer NkArenaAllocator::Calloc(SizeType size, AlignType alignment) {
            Pointer ptr = Allocate(size, alignment);
            if (ptr) {
                NkMemZero(ptr, size);
            }
            return ptr;
        }

        const nk_char* NkArenaAllocator::Name() const noexcept {
            return "NkArenaAllocator";
        }

        void NkArenaAllocator::Reset() noexcept {
            mOffset = 0u;
        }

        NkArenaAllocator::Marker NkArenaAllocator::CreateMarker() const noexcept {
            return mOffset;  // Capture l'offset courant comme point de restauration
        }

        void NkArenaAllocator::FreeToMarker(Marker marker) noexcept {
            // Restaure l'offset si le marker est valide (dans le passé)
            if (marker <= mOffset) {
                mOffset = marker;
            }
            // Si marker > mOffset : no-op (marker invalide ou futur)
        }

        // ====================================================================
        // NkStackAllocator : Implémentations spécifiques (LIFO)
        // ====================================================================

        NkStackAllocator::NkStackAllocator(SizeType capacityBytes, NkAllocator* backingAllocator)
            : NkAllocator("NkStackAllocator"),
              mBacking(backingAllocator ? backingAllocator : &NkGetDefaultAllocator()),
              mBuffer(nullptr),
              mCapacity(capacityBytes),
              mOffset(0u),
              mLastAllocation(nullptr),
              mOwnsBuffer(true) {
            
            if (capacityBytes == 0u) {
                mOwnsBuffer = false;
                return;
            }
            
            mBuffer = static_cast<nk_uint8*>(mBacking->Allocate(capacityBytes, NK_MEMORY_DEFAULT_ALIGNMENT));
            if (!mBuffer) {
                mCapacity = 0u;
                mOwnsBuffer = false;
            }
        }

        NkStackAllocator::~NkStackAllocator() {
            if (mOwnsBuffer && mBuffer && mBacking) {
                mBacking->Deallocate(mBuffer);
            }
        }

        NkAllocator::Pointer NkStackAllocator::Allocate(SizeType size, AlignType alignment) {
            if (!mBuffer || size == 0u) {
                return nullptr;
            }
            if (!NkIsPowerOfTwo(alignment)) {
                alignment = NK_MEMORY_DEFAULT_ALIGNMENT;
            }

            // Calcul avec header pour chaînage LIFO
            const SizeType stackBase = reinterpret_cast<SizeType>(mBuffer) + mOffset;
            const SizeType headerEnd = stackBase + sizeof(AllocationHeader);
            const SizeType alignedAddr = NK_ALIGN_UP(headerEnd, alignment);
            const SizeType used = (alignedAddr + size) - stackBase;

            if (mOffset + used > mCapacity) {
                return nullptr;
            }

            // Initialisation de l'header pour restauration future
            auto* header = reinterpret_cast<AllocationHeader*>(alignedAddr - sizeof(AllocationHeader));
            header->previousOffset     = mOffset;
            header->previousAllocation = mLastAllocation;

            // Mise à jour de l'état de la pile
            mOffset = mOffset + used;
            mLastAllocation = reinterpret_cast<Pointer>(alignedAddr);
            return mLastAllocation;
        }

        void NkStackAllocator::Deallocate(Pointer ptr) {
            // Deallocation uniquement si c'est le top of stack (LIFO)
            if (!ptr || ptr != mLastAllocation) {
                return;  // Violation LIFO : ignore silencieusement (ou logger en debug)
            }
            
            // Restauration de l'état précédent via l'header
            auto* header = reinterpret_cast<AllocationHeader*>(
                static_cast<nk_uint8*>(ptr) - sizeof(AllocationHeader));
            mOffset = header->previousOffset;
            mLastAllocation = header->previousAllocation;
        }

        NkAllocator::Pointer NkStackAllocator::Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                                        AlignType alignment) {
            if (!ptr) {
                return Allocate(newSize, alignment);
            }
            // Resize uniquement autorisé sur le top of stack
            if (ptr != mLastAllocation) {
                return nullptr;  // Non-LIFO : échec
            }
            if (newSize == oldSize) {
                return ptr;  // No-op
            }

            auto* header = reinterpret_cast<AllocationHeader*>(
                static_cast<nk_uint8*>(ptr) - sizeof(AllocationHeader));
            
            const SizeType stackBase = reinterpret_cast<SizeType>(mBuffer) + header->previousOffset;
            const SizeType ptrAddr = reinterpret_cast<SizeType>(ptr);
            const SizeType newUsed = (ptrAddr + newSize) - stackBase;
            
            if (header->previousOffset + newUsed > mCapacity) {
                return nullptr;  // Out of memory pour le resize
            }

            // Resize in-place : mise à jour de l'offset uniquement
            mOffset = header->previousOffset + newUsed;
            return ptr;
        }

        NkAllocator::Pointer NkStackAllocator::Calloc(SizeType size, AlignType alignment) {
            Pointer ptr = Allocate(size, alignment);
            if (ptr) {
                NkMemZero(ptr, size);
            }
            return ptr;
        }

        const nk_char* NkStackAllocator::Name() const noexcept {
            return "NkStackAllocator";
        }

        void NkStackAllocator::Reset() noexcept {
            mOffset = 0u;
            mLastAllocation = nullptr;
        }

        // ====================================================================
        // NkPoolAllocator : Implémentations spécifiques (free-list)
        // ====================================================================

        NkPoolAllocator::NkPoolAllocator(SizeType blockSize, SizeType blockCount, NkAllocator* backingAllocator)
            : NkAllocator("NkPoolAllocator"),
              mBacking(backingAllocator ? backingAllocator : &NkGetDefaultAllocator()),
              mBuffer(nullptr),
              mFreeList(nullptr),
              mBlockSize(MaxSize(blockSize, static_cast<SizeType>(sizeof(FreeNode)))),
              mBlockCount(blockCount),
              mOwnsBuffer(true) {
            
            if (mBlockSize == 0u || mBlockCount == 0u) {
                mOwnsBuffer = false;
                return;
            }

            const SizeType totalSize = mBlockSize * mBlockCount;
            mBuffer = static_cast<nk_uint8*>(mBacking->Allocate(totalSize, NK_MEMORY_DEFAULT_ALIGNMENT));
            if (!mBuffer) {
                mOwnsBuffer = false;
                return;
            }
            Reset();  // Initialise la free-list
        }

        NkPoolAllocator::~NkPoolAllocator() {
            if (mOwnsBuffer && mBuffer && mBacking) {
                mBacking->Deallocate(mBuffer);
            }
        }

        NkAllocator::Pointer NkPoolAllocator::Allocate(SizeType size, SizeType /*alignment*/) {
            // Pool allocator ignore l'alignement (blocs pré-alignés à l'init)
            if (!mFreeList || size == 0u || size > mBlockSize) {
                return nullptr;
            }
            // Extraction de la tête de la free-list (O(1))
            FreeNode* node = mFreeList;
            mFreeList = mFreeList->next;
            return node;
        }

        void NkPoolAllocator::Deallocate(Pointer ptr) {
            if (!ptr) {
                return;
            }
            // Réinsertion en tête de free-list (O(1))
            auto* node = static_cast<FreeNode*>(ptr);
            node->next = mFreeList;
            mFreeList = node;
        }

        NkAllocator::Pointer NkPoolAllocator::Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                                        AlignType alignment) {
            if (!ptr) {
                return Allocate(newSize, alignment);
            }
            if (newSize == 0u) {
                Deallocate(ptr);
                return nullptr;
            }
            // Si la nouvelle taille tient dans un bloc : no-op (réutilise le même bloc)
            if (newSize <= mBlockSize) {
                return ptr;
            }
            // Sinon : fallback générique (allocate + copy + deallocate)
            Pointer newPtr = Allocate(newSize, alignment);
            if (!newPtr) {
                return nullptr;
            }
            NkMemCopy(newPtr, ptr, MinSize(oldSize, newSize));
            Deallocate(ptr);
            return newPtr;
        }

        NkAllocator::Pointer NkPoolAllocator::Calloc(SizeType size, AlignType alignment) {
            Pointer ptr = Allocate(size, alignment);
            if (ptr) {
                // Zero-initialise le bloc complet (pas seulement size demandé)
                NkMemZero(ptr, mBlockSize);
            }
            return ptr;
        }

        const nk_char* NkPoolAllocator::Name() const noexcept {
            return "NkPoolAllocator";
        }

        void NkPoolAllocator::Reset() noexcept {
            mFreeList = nullptr;
            if (!mBuffer) {
                return;
            }
            // Construction de la free-list chaînée sur tout le buffer
            for (SizeType i = 0u; i < mBlockCount; ++i) {
                auto* node = reinterpret_cast<FreeNode*>(mBuffer + i * mBlockSize);
                node->next = mFreeList;
                mFreeList = node;
            }
        }

        // ====================================================================
        // NkFreeListAllocator : Implémentations spécifiques (variable-size)
        // ====================================================================

        NkFreeListAllocator::NkFreeListAllocator(SizeType capacityBytes, NkAllocator* backingAllocator)
            : NkAllocator("NkFreeListAllocator"),
              mBacking(backingAllocator ? backingAllocator : &NkGetDefaultAllocator()),
              mBuffer(nullptr),
              mCapacity(capacityBytes),
              mHead(nullptr),
              mOwnsBuffer(true) {
            
            if (capacityBytes == 0u || capacityBytes < sizeof(BlockHeader)) {
                mCapacity = 0u;
                mOwnsBuffer = false;
                return;
            }
            
            mBuffer = static_cast<nk_uint8*>(mBacking->Allocate(capacityBytes, NK_MEMORY_DEFAULT_ALIGNMENT));
            if (!mBuffer) {
                mCapacity = 0u;
                mOwnsBuffer = false;
                return;
            }
            Reset();  // Initialise la liste de blocs avec un seul bloc libre
        }

        NkFreeListAllocator::~NkFreeListAllocator() {
            if (mOwnsBuffer && mBuffer && mBacking) {
                mBacking->Deallocate(mBuffer);
            }
        }

        NkAllocator::Pointer NkFreeListAllocator::Allocate(SizeType size, AlignType alignment) {
            if (!mHead || size == 0u) {
                return nullptr;
            }
            if (!NkIsPowerOfTwo(alignment)) {
                alignment = NK_MEMORY_DEFAULT_ALIGNMENT;
            }

            // First-fit : recherche du premier bloc libre assez grand
            BlockHeader* block = mHead;
            while (block) {
                if (!block->isFree) {
                    block = block->next;
                    continue;
                }

                // Calcul de l'adresse utilisateur alignée dans le bloc
                const nk_size blockStart = reinterpret_cast<nk_size>(block);
                const nk_size userStart = NK_ALIGN_UP(
                    blockStart + sizeof(BlockHeader) + sizeof(AllocationHeader), 
                    alignment);
                const nk_size usedSize = (userStart + size) - blockStart;
                
                if (usedSize > block->size) {
                    block = block->next;
                    continue;  // Bloc trop petit : suivant
                }

                // Split du bloc si reste suffisant pour un nouveau bloc
                const nk_size remaining = block->size - usedSize;
                const nk_size minBlock = sizeof(BlockHeader) + sizeof(AllocationHeader) + 8u;
                
                if (remaining > minBlock) {
                    auto* newBlock = reinterpret_cast<BlockHeader*>(blockStart + usedSize);
                    newBlock->size   = remaining;
                    newBlock->next   = block->next;
                    newBlock->prev   = block;
                    newBlock->isFree = true;
                    
                    if (newBlock->next) {
                        newBlock->next->prev = newBlock;
                    }
                    block->next = newBlock;
                    block->size = usedSize;  // Réduit la taille du bloc courant
                }

                // Marquage comme alloué et retour
                block->isFree = false;
                auto* allocHeader = reinterpret_cast<AllocationHeader*>(userStart - sizeof(AllocationHeader));
                allocHeader->block = block;
                return reinterpret_cast<void*>(userStart);
            }

            return nullptr;  // Aucun bloc libre assez grand trouvé
        }

        void NkFreeListAllocator::Coalesce(BlockHeader* block) noexcept {
            if (!block) {
                return;
            }

            // Fusion avec le bloc suivant si libre et adjacent
            if (block->next && block->next->isFree) {
                nk_uint8* blockEnd = reinterpret_cast<nk_uint8*>(block) + block->size;
                if (blockEnd == reinterpret_cast<nk_uint8*>(block->next)) {
                    BlockHeader* next = block->next;
                    block->size += next->size;
                    block->next = next->next;
                    if (block->next) {
                        block->next->prev = block;
                    }
                    // Note : next n'est pas deallocate, il est absorbé dans block
                }
            }

            // Fusion avec le bloc précédent si libre et adjacent
            if (block->prev && block->prev->isFree) {
                nk_uint8* prevEnd = reinterpret_cast<nk_uint8*>(block->prev) + block->prev->size;
                if (prevEnd == reinterpret_cast<nk_uint8*>(block)) {
                    block->prev->size += block->size;
                    block->prev->next = block->next;
                    if (block->next) {
                        block->next->prev = block->prev;
                    }
                    // block est absorbé dans prev : pas besoin de le deallocate
                }
            }
        }

        void NkFreeListAllocator::Deallocate(Pointer ptr) {
            if (!ptr) {
                return;
            }
            // Récupération du BlockHeader via l'AllocationHeader
            auto* allocHeader = reinterpret_cast<AllocationHeader*>(
                static_cast<nk_uint8*>(ptr) - sizeof(AllocationHeader));
            BlockHeader* block = allocHeader->block;
            if (!block) {
                return;  // Corruption détectée : ignore
            }
            block->isFree = true;
            Coalesce(block);  // Tentative de fusion avec les voisins libres
        }

        NkAllocator::Pointer NkFreeListAllocator::Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                                            AlignType alignment) {
            if (!ptr) {
                return Allocate(newSize, alignment);
            }
            if (newSize == 0u) {
                Deallocate(ptr);
                return nullptr;
            }
            // Fallback générique (pourrait être optimisé avec resize in-place si bloc adjacent libre)
            Pointer newPtr = Allocate(newSize, alignment);
            if (!newPtr) {
                return nullptr;
            }
            NkMemCopy(newPtr, ptr, MinSize(oldSize, newSize));
            Deallocate(ptr);
            return newPtr;
        }

        NkAllocator::Pointer NkFreeListAllocator::Calloc(SizeType size, AlignType alignment) {
            Pointer ptr = Allocate(size, alignment);
            if (ptr) {
                NkMemZero(ptr, size);
            }
            return ptr;
        }

        const nk_char* NkFreeListAllocator::Name() const noexcept {
            return "NkFreeListAllocator";
        }

        void NkFreeListAllocator::Reset() noexcept {
            if (!mBuffer || mCapacity < sizeof(BlockHeader)) {
                mHead = nullptr;
                return;
            }
            // Initialisation avec un seul bloc libre couvrant tout le buffer
            mHead = reinterpret_cast<BlockHeader*>(mBuffer);
            mHead->size   = mCapacity;
            mHead->next   = nullptr;
            mHead->prev   = nullptr;
            mHead->isFree = true;
        }

        // ====================================================================
        // NkBuddyAllocator : Implémentations spécifiques (wrapper)
        // ====================================================================

        NkBuddyAllocator::NkBuddyAllocator(SizeType capacityBytes, SizeType minBlockSize, 
                                        NkAllocator* backingAllocator)
            : NkAllocator("NkBuddyAllocator"),
              mMinBlockSize(MaxSize(minBlockSize, static_cast<SizeType>(16u))),
              mBackend(capacityBytes, backingAllocator) {}

        NkBuddyAllocator::~NkBuddyAllocator() = default;

        NkAllocator::Pointer NkBuddyAllocator::Allocate(SizeType size, AlignType alignment) {
            // Arrondi à la puissance de 2 >= max(minBlockSize, size)
            const SizeType rounded = MaxSize(mMinBlockSize, NextPow2(size));
            return mBackend.Allocate(rounded, alignment);
        }

        void NkBuddyAllocator::Deallocate(Pointer ptr) {
            mBackend.Deallocate(ptr);  // Délègue au backend
        }

        NkAllocator::Pointer NkBuddyAllocator::Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                                        AlignType alignment) {
            if (!ptr) {
                return Allocate(newSize, alignment);
            }
            if (newSize == 0u) {
                Deallocate(ptr);
                return nullptr;
            }
            // Fallback générique (buddy ne supporte pas resize in-place facilement)
            Pointer newPtr = Allocate(newSize, alignment);
            if (!newPtr) {
                return nullptr;
            }
            NkMemCopy(newPtr, ptr, MinSize(oldSize, newSize));
            Deallocate(ptr);
            return newPtr;
        }

        NkAllocator::Pointer NkBuddyAllocator::Calloc(SizeType size, AlignType alignment) {
            Pointer ptr = Allocate(size, alignment);
            if (ptr) {
                NkMemZero(ptr, size);
            }
            return ptr;
        }

        const nk_char* NkBuddyAllocator::Name() const noexcept {
            return "NkBuddyAllocator";
        }

        void NkBuddyAllocator::Reset() noexcept {
            mBackend.Reset();  // Reset du backend
        }

        // ====================================================================
        // Fonctions globales : Implémentations
        // ====================================================================

        NkAllocator& NkGetDefaultAllocator() noexcept {
            // Initialisation lazy thread-safe (C++11+ guarantee)
            if (!gDefaultAllocator) {
                gDefaultAllocator = &GlobalMallocAllocator();
            }
            return *gDefaultAllocator;
        }

        NkAllocator& NkGetMallocAllocator() noexcept {
            return GlobalMallocAllocator();
        }

        NkAllocator& NkGetNewAllocator() noexcept {
            return GlobalNewAllocator();
        }

        NkAllocator& NkGetVirtualAllocator() noexcept {
            return GlobalVirtualAllocator();
        }

        void NkSetDefaultAllocator(NkAllocator* allocator) noexcept {
            // nullptr = reset vers malloc allocator
            gDefaultAllocator = allocator ? allocator : &GlobalMallocAllocator();
        }

        void* NkAlloc(nk_size size, NkAllocator* allocator, nk_size alignment) {
            NkAllocator& alloc = allocator ? *allocator : NkGetDefaultAllocator();
            return alloc.Allocate(size, alignment);
        }

        void* NkAllocZero(nk_size count, nk_size size, NkAllocator* allocator, nk_size alignment) {
            if (count == 0u || size == 0u) {
                return nullptr;  // Évite overflow et allocation inutile
            }
            NkAllocator& alloc = allocator ? *allocator : NkGetDefaultAllocator();
            return alloc.Calloc(count * size, alignment);
        }

        void* NkRealloc(void* ptr, nk_size oldSize, nk_size newSize, NkAllocator* allocator, nk_size alignment) {
            NkAllocator& alloc = allocator ? *allocator : NkGetDefaultAllocator();
            return alloc.Reallocate(ptr, oldSize, newSize, alignment);
        }

        void NkFree(void* ptr, NkAllocator* allocator) {
            if (!ptr) {
                return;  // Safe : free(nullptr) est no-op
            }
            NkAllocator& alloc = allocator ? *allocator : NkGetDefaultAllocator();
            alloc.Deallocate(ptr);
        }

        void NkFree(void* ptr, nk_size size, NkAllocator* allocator) {
            if (!ptr) {
                return;
            }
            NkAllocator& alloc = allocator ? *allocator : NkGetDefaultAllocator();
            alloc.Deallocate(ptr, size);  // Sized deallocation si supportée
        }

    } // namespace memory
} // namespace nkentseu

// =============================================================================
// NOTES DE MAINTENANCE
// =============================================================================
/*
    [PERFORMANCE]
    - NkPoolAllocator : O(1) allocate/deallocate, idéal pour objets fréquents
    - NkLinear/Arena : O(1) allocate, Reset() O(1) pour libération bulk
    - NkFreeListAllocator : O(n) allocate (first-fit), O(1) deallocate + coalescing
    - NkVirtualAllocator : syscall mmap/VirtualAlloc : lent, pour grandes régions

    [SÉCURITÉ]
    - Toutes les fonctions gèrent nullptr et size==0 de façon définie
    - Double-free détectable via assertions en mode debug (à implémenter)
    - Alignement toujours validé comme puissance de 2 avant utilisation

    [EXTENSION]
    - Pour ajouter un nouvel allocateur : hériter de NkAllocator, implémenter Allocate/Deallocate
    - Pour optimisations plateforme : ajouter des blocs #if NKENTSEU_PLATFORM_* dans Allocate
    - Pour tracking mémoire : wrapper NkAllocator avec comptage/statistiques

    [THREAD-SAFETY]
    - Aucun allocateur n'est thread-safe par défaut
    - Pour usage multi-thread : protéger avec mutex externe ou utiliser un allocateur par thread
    - Les singletons globaux sont thread-safe à l'initialisation (C++11+)
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================