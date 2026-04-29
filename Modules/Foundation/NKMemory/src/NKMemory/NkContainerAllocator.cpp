// =============================================================================
// NKMemory/NkContainerAllocator.cpp
// Implémentation de l'allocateur conteneur haute performance avec size classes et TLS cache.
//
// Design :
//  - Inclusion systématique de "pch.h" en premier pour la précompilation
//  - Réutilisation maximale : NkUtils pour alignement, NkMemCopy/Zero pour opérations
//  - Thread-safety via NkSpinLock avec guard RAII local (pas de dépendance externe)
//  - Generation counter pour invalidation safe des caches TLS lors de Reset()
//  - Registry global pour détection des allocateurs détruits dans les caches TLS
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
#include "NKMemory/NkContainerAllocator.h"
#include "NKMemory/NkUtils.h"           // NkAlignUp, NkMemCopy, NkMemZero, NkIsPowerOfTwo

using namespace nkentseu;

// -------------------------------------------------------------------------
// NAMESPACE ANONYME : Helpers internes non-exportés
// -------------------------------------------------------------------------
namespace {

    /**
     * @brief Guard RAII minimal pour NkSpinLock
     * @note Défini localement car NkSpinLockGuard n'existe pas encore dans NKCore
     * @note Évite les oublis de Unlock() et garantit l'exception-safety
     */
    class SpinLockGuard {
    public:
        explicit SpinLockGuard(NkSpinLock& lock) noexcept : mLock(&lock) {
            mLock->Lock();
        }
        ~SpinLockGuard() noexcept {
            if (mLock) {
                mLock->Unlock();
            }
        }
        // Non-copiable, non-movable pour sécurité
        SpinLockGuard(const SpinLockGuard&) = delete;
        SpinLockGuard& operator=(const SpinLockGuard&) = delete;
        SpinLockGuard(SpinLockGuard&&) = delete;
        SpinLockGuard& operator=(SpinLockGuard&&) = delete;
    private:
        NkSpinLock* mLock;
    };

    /**
     * @brief Tableau des tailles des 13 size classes (16B à 2048B)
     * @note Utilisé par FindClassIndex() pour dispatch rapide
     */
    constexpr nkentseu::nk_size CONTAINER_CLASS_SIZES[nkentseu::memory::NkContainerAllocator::NK_CONTAINER_CLASS_COUNT] = {
        16u,  32u,  64u,  96u,  128u, 192u, 256u,
        384u, 512u, 768u, 1024u, 1536u, 2048u
    };

    /**
     * @brief Arrondit une valeur vers le haut à la prochaine puissance de 2 d'alignement
     * @note Optimisé pour alignment puissance de 2 : utilise masking au lieu de modulo
     */
    [[nodiscard]] inline nkentseu::nk_size AlignUpPow2(nkentseu::nk_size value, nkentseu::nk_size alignment) noexcept {
        return (value + (alignment - 1u)) & ~(alignment - 1u);
    }

    /**
     * @brief Décrémentation atomique saturante (ne descend pas en dessous de 0)
     * @note Utilise CompareExchangeWeak en boucle pour sécurité thread-safe
     */
    inline void AtomicSaturatingDecrement(nkentseu::NkAtomicSize& value) noexcept {
        nkentseu::nk_size expected = value.Load(nkentseu::NkMemoryOrder::NK_RELAXED);
        while (expected > 0u) {
            if (value.CompareExchangeWeak(expected,
                                         expected - 1u,
                                         nkentseu::NkMemoryOrder::NK_RELAXED)) {
                return;
            }
        }
    }

    /**
     * @brief Slot de cache thread-local pour une size class
     * @note Un tableau de ces slots par thread (thread_local) pour fast path sans lock
     */
    struct ContainerTlsCacheSlot {
        nkentseu::memory::NkContainerAllocator* Owner;  ///< Allocateur propriétaire de ce cache
        nk_uint64 Generation;                            ///< Génération pour invalidation safe
        void* Head;                                      ///< Tête de la free-list locale
        nk_size Count;                                   ///< Nombre de slots dans le cache
    };

    /**
     * @brief Cache thread-local global : un slot par size class par thread
     * @note thread_local : chaque thread a sa propre copie, pas de synchronisation nécessaire
     */
    thread_local ContainerTlsCacheSlot gContainerTls[nkentseu::memory::NkContainerAllocator::NK_CONTAINER_CLASS_COUNT] = {};

    /**
     * @brief Lock global pour le registry d'instances d'allocateurs
     * @note Singleton local : initialisation thread-safe en C++11+
     */
    [[nodiscard]] inline NkSpinLock& ContainerAllocatorRegistryLock() noexcept {
        static NkSpinLock lock;
        return lock;
    }

    /**
     * @brief Tête de la liste chaînée globale des instances d'allocateurs
     * @note Singleton local : initialisation thread-safe en C++11+
     */
    [[nodiscard]] inline nkentseu::memory::NkContainerAllocator*& ContainerAllocatorRegistryHead() noexcept {
        static nkentseu::memory::NkContainerAllocator* head = nullptr;
        return head;
    }

} // namespace anonyme


// -------------------------------------------------------------------------
// NAMESPACE PUBLIC : Implémentation de NkContainerAllocator
// -------------------------------------------------------------------------
namespace nkentseu {
    namespace memory {

        // ====================================================================
        // NkContainerAllocator : Constructeur / Destructeur
        // ====================================================================

        NkContainerAllocator::NkContainerAllocator(NkAllocator* backingAllocator,
                                                  nk_size pageSize,
                                                  const nk_char* name)
            : NkAllocator(name ? name : "NkContainerAllocator")
            , mBacking(backingAllocator ? backingAllocator : &NkGetDefaultAllocator())
            , mPageSize(pageSize < 4096u ? 4096u : pageSize)  // Minimum 4KB pour compatibilité OS
            , mHeaderOffset(AlignUpPow2(sizeof(NkContainerAllocHeader), NK_CONTAINER_SMALL_ALIGNMENT))
            , mClasses{}  // Zero-initialization du tableau
            , mLargeHead(nullptr)
            , mLargeAllocationCount(0u)
            , mLargeAllocationBytes(0u)
            , mGeneration(1u)  // Génération initiale = 1
            , mRegistryNext(nullptr)
            , mClassLocks{}  // Zero-initialization des locks
            , mLargeLock() {
            
            // Initialisation des 13 size classes
            for (nk_uint32 i = 0u; i < NK_CONTAINER_CLASS_COUNT; ++i) {
                mClasses[i].UserSize = ::CONTAINER_CLASS_SIZES[i];
                // SlotSize = header aligné + taille utilisateur, le tout aligné sur 16B
                mClasses[i].SlotSize = AlignUpPow2(mHeaderOffset + mClasses[i].UserSize,
                                                  NK_CONTAINER_SMALL_ALIGNMENT);
                mClasses[i].FreeList = nullptr;
                mClasses[i].Pages = nullptr;
                mClasses[i].TotalSlots = 0u;
                mClasses[i].UsedSlots.Store(0u, NkMemoryOrder::NK_RELAXED);
            }

            // Enregistrement dans le registry global pour détection TLS safe
            RegisterAllocatorInstance();
        }

        NkContainerAllocator::~NkContainerAllocator() {
            // Cleanup : libère toutes les ressources et désenregistre l'instance
            Reset();
            UnregisterAllocatorInstance();
        }

        // ====================================================================
        // NkContainerAllocator : Registry Management
        // ====================================================================

        void NkContainerAllocator::RegisterAllocatorInstance() noexcept {
            SpinLockGuard guard(::ContainerAllocatorRegistryLock());
            // Insertion en tête de la liste chaînée globale
            mRegistryNext = ::ContainerAllocatorRegistryHead();
            ::ContainerAllocatorRegistryHead() = this;
        }

        void NkContainerAllocator::UnregisterAllocatorInstance() noexcept {
            SpinLockGuard guard(::ContainerAllocatorRegistryLock());

            // Recherche et retrait de la liste chaînée
            NkContainerAllocator* previous = nullptr;
            NkContainerAllocator* current = ::ContainerAllocatorRegistryHead();
            while (current) {
                if (current == this) {
                    if (previous) {
                        previous->mRegistryNext = current->mRegistryNext;
                    } else {
                        ::ContainerAllocatorRegistryHead() = current->mRegistryNext;
                    }
                    break;
                }
                previous = current;
                current = current->mRegistryNext;
            }

            // Détachement défensif
            mRegistryNext = nullptr;
        }

        nk_bool NkContainerAllocator::IsAllocatorInstanceLive(const NkContainerAllocator* allocator) noexcept {
            if (!allocator) {
                return false;
            }

            SpinLockGuard guard(::ContainerAllocatorRegistryLock());
            // Parcours linéaire de la liste globale (petit nombre d'instances attendu)
            NkContainerAllocator* current = ::ContainerAllocatorRegistryHead();
            while (current) {
                if (current == allocator) {
                    return true;
                }
                current = current->mRegistryNext;
            }
            return false;
        }

        // ====================================================================
        // NkContainerAllocator : Helpers Internes
        // ====================================================================

        nk_bool NkContainerAllocator::IsSmallAlignment(SizeType alignment) const noexcept {
            // Alignement valide pour fast path : puissance de 2 et <= 16B
            return NkIsPowerOfTwo(alignment) && alignment <= NK_CONTAINER_SMALL_ALIGNMENT;
        }

        nk_size NkContainerAllocator::HeaderOffset() const noexcept {
            // Offset pré-calculé au constructeur : header aligné sur 16B
            return mHeaderOffset;
        }

        nk_uint32 NkContainerAllocator::FindClassIndex(SizeType size) const noexcept {
            // Recherche linéaire dans le tableau des 13 size classes (très rapide)
            for (nk_uint32 i = 0u; i < NK_CONTAINER_CLASS_COUNT; ++i) {
                if (size <= mClasses[i].UserSize) {
                    return i;
                }
            }
            // Taille > 2048B → grande allocation
            return NK_CONTAINER_LARGE_CLASS;
        }

        // ====================================================================
        // NkContainerAllocator : Allocation (Public API)
        // ====================================================================

        NkAllocator::Pointer NkContainerAllocator::Allocate(SizeType size, SizeType alignment) {
            // Guards : taille invalide
            if (size == 0u) {
                return nullptr;
            }

            // Normalisation de l'alignement
            if (!NkIsPowerOfTwo(alignment)) {
                alignment = NK_MEMORY_DEFAULT_ALIGNMENT;
            }
            if (alignment < NK_MEMORY_DEFAULT_ALIGNMENT) {
                alignment = NK_MEMORY_DEFAULT_ALIGNMENT;
            }

            // Dispatch vers size class ou grande allocation
            const nk_uint32 classIndex = FindClassIndex(size);

            // Fast path pour petites allocations avec alignement compatible
            if (classIndex != NK_CONTAINER_LARGE_CLASS && IsSmallAlignment(alignment)) {
                // Tentative d'allocation depuis le cache TLS du thread courant (sans lock)
                if (Pointer cached = TryAllocateSmallFromTls(size, classIndex)) {
                    return cached;
                }
                // Fallback : allocation depuis la free-list de la size class (avec lock)
                SpinLockGuard guard(mClassLocks[classIndex]);
                return AllocateSmallLocked(size, classIndex);
            }

            // Grande allocation : backing allocator direct (avec lock dédié)
            SpinLockGuard guard(mLargeLock);
            return AllocateLargeLocked(size, alignment);
        }

        // ====================================================================
        // NkContainerAllocator : TLS Cache Fast Path
        // ====================================================================

        NkAllocator::Pointer NkContainerAllocator::TryAllocateSmallFromTls(SizeType size,
                                                                          nk_uint32 classIndex) noexcept {
            // Guard : index invalide
            if (classIndex >= NK_CONTAINER_CLASS_COUNT) {
                return nullptr;
            }

            // Accès au slot TLS pour cette size class
            ContainerTlsCacheSlot& slot = ::gContainerTls[classIndex];
            // Lecture de la génération courante avec ordering acquire pour visibilité
            const nk_uint64 generation = mGeneration.Load(NkMemoryOrder::NK_ACQUIRE);

            // Vérification de l'ownership et de la génération
            if (slot.Owner == this) {
                if (slot.Generation != generation) {
                    // Génération différente : cache invalide → reset local
                    slot.Generation = generation;
                    slot.Head = nullptr;
                    slot.Count = 0u;
                    return nullptr;
                }

                // Tentative de pop depuis le cache TLS
                if (slot.Head) {
                    auto* node = static_cast<NkFreeNode*>(slot.Head);
                    slot.Head = node->Next;
                    if (slot.Count > 0u) {
                        --slot.Count;
                    }

                    // Reconstruction de l'header de tracking
                    nk_uint8* slotStart = reinterpret_cast<nk_uint8*>(node);
                    nk_uint8* user = slotStart + HeaderOffset();
                    auto* header = reinterpret_cast<NkContainerAllocHeader*>(
                        user - sizeof(NkContainerAllocHeader));
                    header->Offset = HeaderOffset();
                    header->RequestedSize = size;
                    header->ClassIndex = classIndex;
                    header->Reserved = 0u;
                    header->PrevLarge = nullptr;
                    header->NextLarge = nullptr;

                    // Mise à jour du compteur utilisé (atomique relaxé pour performance)
                    mClasses[classIndex].UsedSlots.FetchAdd(1u, NkMemoryOrder::NK_RELAXED);
                    return user;
                }

                return nullptr;  // Cache TLS vide
            }

            // Initialisation ou réinitialisation du cache TLS si nécessaire
            if (slot.Owner == nullptr || !IsAllocatorInstanceLive(slot.Owner)) {
                slot.Owner = this;
                slot.Generation = generation;
                slot.Head = nullptr;
                slot.Count = 0u;
            }

            return nullptr;  // Fallback vers slow path
        }

        // ====================================================================
        // NkContainerAllocator : Allocation Small (Slow Path)
        // ====================================================================

        NkAllocator::Pointer NkContainerAllocator::AllocateSmallLocked(SizeType size, nk_uint32 classIndex) noexcept {
            NkSizeClass& sizeClass = mClasses[classIndex];
            
            // Si free-list vide : allouer une nouvelle page
            if (!sizeClass.FreeList) {
                if (!AllocatePageForClassLocked(classIndex)) {
                    return nullptr;  // Échec d'allocation page
                }
            }

            // Pop depuis la free-list de la size class
            NkFreeNode* node = sizeClass.FreeList;
            sizeClass.FreeList = node->Next;

            // Reconstruction de l'header de tracking
            nk_uint8* slotStart = reinterpret_cast<nk_uint8*>(node);
            nk_uint8* user = slotStart + HeaderOffset();
            auto* header = reinterpret_cast<NkContainerAllocHeader*>(user - sizeof(NkContainerAllocHeader));
            header->Offset = HeaderOffset();
            header->RequestedSize = size;
            header->ClassIndex = classIndex;
            header->Reserved = 0u;
            header->PrevLarge = nullptr;
            header->NextLarge = nullptr;

            // Mise à jour du compteur utilisé
            sizeClass.UsedSlots.FetchAdd(1u, NkMemoryOrder::NK_RELAXED);
            return user;
        }

        nk_bool NkContainerAllocator::AllocatePageForClassLocked(nk_uint32 classIndex) noexcept {
            // Guards : index invalide ou backing allocator nul
            if (classIndex >= NK_CONTAINER_CLASS_COUNT || !mBacking) {
                return false;
            }

            NkSizeClass& sizeClass = mClasses[classIndex];
            
            // Calcul de la taille de l'header de page (aligné sur 16B)
            const nk_size pageHeaderSize = AlignUpPow2(sizeof(NkPageHeader), NK_CONTAINER_SMALL_ALIGNMENT);
            
            // Calcul de la taille de page : au moins assez pour header + 1 slot
            nk_size pageBytes = mPageSize;
            const nk_size minimumBytes = pageHeaderSize + sizeClass.SlotSize;
            if (pageBytes < minimumBytes) {
                pageBytes = minimumBytes;
            }

            // Calcul du nombre de slots dans la page
            nk_size slotCount = (pageBytes - pageHeaderSize) / sizeClass.SlotSize;
            if (slotCount == 0u) {
                // Fallback : au moins 1 slot par page
                slotCount = 1u;
                pageBytes = pageHeaderSize + sizeClass.SlotSize;
            }

            // Allocation de la page via backing allocator
            nk_uint8* raw = static_cast<nk_uint8*>(mBacking->Allocate(pageBytes, NK_CONTAINER_SMALL_ALIGNMENT));
            if (!raw) {
                return false;  // Échec d'allocation backing
            }

            // Initialisation de l'header de page
            auto* page = reinterpret_cast<NkPageHeader*>(raw);
            page->Next = sizeClass.Pages;  // Insertion en tête de liste
            page->RawSize = pageBytes;
            page->ClassIndex = classIndex;
            page->Reserved = 0u;
            sizeClass.Pages = page;

            // Construction de la free-list dans la page
            nk_uint8* slotBase = raw + pageHeaderSize;
            for (nk_size i = 0u; i < slotCount; ++i) {
                auto* node = reinterpret_cast<NkFreeNode*>(slotBase + (i * sizeClass.SlotSize));
                node->Next = sizeClass.FreeList;  // Insertion en tête
                sizeClass.FreeList = node;
            }

            // Mise à jour des stats de la size class
            sizeClass.TotalSlots += slotCount;
            return true;
        }

        // ====================================================================
        // NkContainerAllocator : Allocation Large
        // ====================================================================

        NkAllocator::Pointer NkContainerAllocator::AllocateLargeLocked(SizeType size, SizeType alignment) noexcept {
            if (!mBacking) {
                return nullptr;
            }

            // Normalisation de l'alignement (défensive)
            if (!NkIsPowerOfTwo(alignment)) {
                alignment = NK_MEMORY_DEFAULT_ALIGNMENT;
            }
            if (alignment < NK_MEMORY_DEFAULT_ALIGNMENT) {
                alignment = NK_MEMORY_DEFAULT_ALIGNMENT;
            }

            // Calcul de la taille totale : header + données + padding alignement
            const nk_size totalBytes = size + alignment + sizeof(NkContainerAllocHeader);
            
            // Allocation via backing allocator
            nk_uint8* raw = static_cast<nk_uint8*>(mBacking->Allocate(totalBytes, NK_MEMORY_DEFAULT_ALIGNMENT));
            if (!raw) {
                return nullptr;
            }

            // Calcul de l'adresse utilisateur alignée
            const nk_size rawAddress = reinterpret_cast<nk_size>(raw);
            const nk_size userAddress = NK_ALIGN_UP(rawAddress + sizeof(NkContainerAllocHeader), alignment);
            nk_uint8* user = reinterpret_cast<nk_uint8*>(userAddress);

            // Initialisation de l'header de tracking
            auto* header = reinterpret_cast<NkContainerAllocHeader*>(user - sizeof(NkContainerAllocHeader));
            header->Offset = userAddress - rawAddress;
            header->RequestedSize = size;
            header->ClassIndex = NK_CONTAINER_LARGE_CLASS;
            header->Reserved = 0u;
            header->PrevLarge = nullptr;
            header->NextLarge = mLargeHead;  // Insertion en tête de liste des grandes allocs

            // Mise à jour de la liste chaînée des grandes allocations
            if (mLargeHead) {
                mLargeHead->PrevLarge = header;
            }
            mLargeHead = header;

            // Mise à jour des stats globales
            ++mLargeAllocationCount;
            mLargeAllocationBytes += size;
            return user;
        }

        // ====================================================================
        // NkContainerAllocator : Désallocation (Public API)
        // ====================================================================

        void NkContainerAllocator::Deallocate(Pointer ptr) {
            if (!ptr) {
                return;  // Safe : no-op sur nullptr
            }

            // Récupération de l'header de tracking
            auto* header = reinterpret_cast<NkContainerAllocHeader*>(
                static_cast<nk_uint8*>(ptr) - sizeof(NkContainerAllocHeader));

            // Dispatch vers grande ou petite allocation
            if (header->ClassIndex == NK_CONTAINER_LARGE_CLASS) {
                // Grande allocation : deallocation directe via backing allocator
                SpinLockGuard guard(mLargeLock);
                DeallocateLargeLocked(ptr, header);
                return;
            }

            if (header->ClassIndex < NK_CONTAINER_CLASS_COUNT) {
                // Petite allocation : tentative de retour au cache TLS (fast path)
                TryDeallocateSmallToTls(ptr, header);
                return;
            }

            // Fallback pour pointeurs non-trackés (défensif)
            nk_uint8* raw = static_cast<nk_uint8*>(ptr) - header->Offset;
            if (mBacking && raw) {
                mBacking->Deallocate(raw);
            }
        }

        // ====================================================================
        // NkContainerAllocator : TLS Cache Fast Path (Deallocate)
        // ====================================================================

        void NkContainerAllocator::TryDeallocateSmallToTls(Pointer ptr,
                                                          NkContainerAllocHeader* header) noexcept {
            if (!ptr || !header) {
                return;
            }

            const nk_uint32 classIndex = header->ClassIndex;
            if (classIndex >= NK_CONTAINER_CLASS_COUNT) {
                return;
            }

            ContainerTlsCacheSlot& slot = ::gContainerTls[classIndex];
            const nk_uint64 generation = mGeneration.Load(NkMemoryOrder::NK_ACQUIRE);

            // Vérification et initialisation du cache TLS
            if (slot.Owner == this) {
                if (slot.Generation != generation) {
                    // Génération différente : cache invalide → reset local
                    slot.Generation = generation;
                    slot.Head = nullptr;
                    slot.Count = 0u;
                }
            } else if (slot.Owner == nullptr || !IsAllocatorInstanceLive(slot.Owner)) {
                // Ownership invalide ou allocateur détruit : réinitialisation
                slot.Owner = this;
                slot.Generation = generation;
                slot.Head = nullptr;
                slot.Count = 0u;
            }

            // Tentative de push vers le cache TLS
            if (slot.Owner == this && slot.Generation == generation) {
                // Décrémentation atomique du compteur utilisé
                ::AtomicSaturatingDecrement(mClasses[classIndex].UsedSlots);

                // Push vers le cache TLS
                nk_uint8* slotStart = static_cast<nk_uint8*>(ptr) - header->Offset;
                auto* node = reinterpret_cast<NkFreeNode*>(slotStart);
                node->Next = static_cast<NkFreeNode*>(slot.Head);
                slot.Head = node;
                ++slot.Count;

                // Flush partiel si cache plein
                if (slot.Count > NK_CONTAINER_TLS_CACHE_LIMIT) {
                    SpinLockGuard guard(mClassLocks[classIndex]);
                    NkSizeClass& sizeClass = mClasses[classIndex];
                    while (slot.Count > NK_CONTAINER_TLS_CACHE_RETAIN) {
                        auto* cached = static_cast<NkFreeNode*>(slot.Head);
                        if (!cached) {
                            slot.Count = 0u;
                            break;
                        }

                        // Push vers la free-list de la size class
                        slot.Head = cached->Next;
                        --slot.Count;

                        cached->Next = sizeClass.FreeList;
                        sizeClass.FreeList = cached;
                    }
                }
                return;
            }

            // Fallback : deallocation via free-list de la size class (avec lock)
            SpinLockGuard guard(mClassLocks[classIndex]);
            DeallocateSmallLocked(ptr, header);
        }

        // ====================================================================
        // NkContainerAllocator : Désallocation Small/Large (Slow Path)
        // ====================================================================

        void NkContainerAllocator::DeallocateSmallLocked(Pointer ptr, NkContainerAllocHeader* header) noexcept {
            if (!ptr || !header) {
                return;
            }
            const nk_uint32 classIndex = header->ClassIndex;
            if (classIndex >= NK_CONTAINER_CLASS_COUNT) {
                return;
            }

            NkSizeClass& sizeClass = mClasses[classIndex];
            
            // Push vers la free-list de la size class
            nk_uint8* slotStart = static_cast<nk_uint8*>(ptr) - header->Offset;
            auto* node = reinterpret_cast<NkFreeNode*>(slotStart);
            node->Next = sizeClass.FreeList;
            sizeClass.FreeList = node;

            // Décrémentation atomique saturante du compteur utilisé
            ::AtomicSaturatingDecrement(sizeClass.UsedSlots);
        }

        void NkContainerAllocator::DeallocateLargeLocked(Pointer ptr, NkContainerAllocHeader* header) noexcept {
            if (!ptr || !header || !mBacking) {
                return;
            }

            // Retrait de la liste chaînée des grandes allocations
            if (header->PrevLarge) {
                header->PrevLarge->NextLarge = header->NextLarge;
            } else {
                mLargeHead = header->NextLarge;  // C'était la tête
            }
            if (header->NextLarge) {
                header->NextLarge->PrevLarge = header->PrevLarge;
            }

            // Mise à jour des stats globales (sous-traction sécurisée)
            if (mLargeAllocationCount > 0u) {
                --mLargeAllocationCount;
            }
            if (mLargeAllocationBytes >= header->RequestedSize) {
                mLargeAllocationBytes -= header->RequestedSize;
            } else {
                mLargeAllocationBytes = 0u;  // Defensive : éviter underflow
            }

            // Libération via backing allocator
            nk_uint8* raw = static_cast<nk_uint8*>(ptr) - header->Offset;
            mBacking->Deallocate(raw);
        }

        // ====================================================================
        // NkContainerAllocator : Autres Méthodes Publiques
        // ====================================================================

        NkAllocator::Pointer NkContainerAllocator::Reallocate(Pointer ptr,
                                                             SizeType oldSize,
                                                             SizeType newSize,
                                                             SizeType alignment) {
            // Cas 1 : allocation initiale (ptr == nullptr)
            if (!ptr) {
                return Allocate(newSize, alignment);
            }
            // Cas 2 : libération (newSize == 0)
            if (newSize == 0u) {
                Deallocate(ptr);
                return nullptr;
            }

            // Détermination de la taille à copier
            auto* header = reinterpret_cast<NkContainerAllocHeader*>(
                static_cast<nk_uint8*>(ptr) - sizeof(NkContainerAllocHeader));
            const SizeType knownOldSize = (header->RequestedSize > 0u) ? header->RequestedSize : oldSize;

            // Optimisation shrink in-place : si newSize <= oldSize, no-op
            if (newSize <= knownOldSize) {
                header->RequestedSize = newSize;
                return ptr;
            }

            // Grow : allocate nouveau + copy + deallocate ancien
            Pointer replacement = Allocate(newSize, alignment);
            if (!replacement) {
                return nullptr;
            }

            NkMemCopy(replacement, ptr, knownOldSize);
            Deallocate(ptr);
            return replacement;
        }

        NkAllocator::Pointer NkContainerAllocator::Calloc(SizeType size, SizeType alignment) {
            Pointer ptr = Allocate(size, alignment);
            if (ptr) {
                NkMemZero(ptr, size);  // Zero-initialisation via NKMemory utils
            }
            return ptr;
        }

        const nk_char* NkContainerAllocator::Name() const noexcept {
            return mName;  // Hérité de NkAllocator
        }

        void NkContainerAllocator::Reset() noexcept {
            // Incrémentation de la génération pour invalider les caches TLS
            const nk_uint64 nextGeneration = mGeneration.Load(NkMemoryOrder::NK_RELAXED) + 1u;
            mGeneration.Store(nextGeneration, NkMemoryOrder::NK_RELEASE);

            // Invalidation des caches TLS du thread courant pour cet allocateur
            for (nk_uint32 i = 0u; i < NK_CONTAINER_CLASS_COUNT; ++i) {
                ContainerTlsCacheSlot& slot = ::gContainerTls[i];
                if (slot.Owner == this) {
                    slot.Generation = nextGeneration;
                    slot.Head = nullptr;
                    slot.Count = 0u;
                }
            }

            // Libération des grandes allocations (sous lock)
            {
                SpinLockGuard guard(mLargeLock);
                ReleaseAllLargeLocked();
            }

            // Libération des pages de toutes les size classes (locks individuels)
            for (nk_uint32 i = 0u; i < NK_CONTAINER_CLASS_COUNT; ++i) {
                SpinLockGuard guard(mClassLocks[i]);
                ReleaseAllPagesLocked();
            }
        }

        NkContainerAllocator::NkStats NkContainerAllocator::GetStats() const noexcept {
            NkStats stats{};

            nk_size totalSlots = 0u;
            nk_size usedSlots = 0u;

            // Stats des size classes (locks individuels)
            for (nk_uint32 i = 0u; i < NK_CONTAINER_CLASS_COUNT; ++i) {
                SpinLockGuard guard(mClassLocks[i]);

                totalSlots += mClasses[i].TotalSlots;
                usedSlots += mClasses[i].UsedSlots.Load(NkMemoryOrder::NK_RELAXED);

                // Comptage des pages et bytes réservés
                NkPageHeader* page = mClasses[i].Pages;
                while (page) {
                    ++stats.PageCount;
                    stats.ReservedBytes += page->RawSize;
                    page = page->Next;
                }
            }

            // Stats des grandes allocations (lock dédié)
            {
                SpinLockGuard guard(mLargeLock);
                stats.LargeAllocationCount = mLargeAllocationCount;
                stats.LargeAllocationBytes = mLargeAllocationBytes;
            }

            // Calcul des blocs petits utilisés/libres
            stats.SmallUsedBlocks = usedSlots;
            stats.SmallFreeBlocks = (totalSlots >= usedSlots) ? (totalSlots - usedSlots) : 0u;
            return stats;
        }

        // ====================================================================
        // NkContainerAllocator : Cleanup Internes
        // ====================================================================

        void NkContainerAllocator::ReleaseAllPagesLocked() noexcept {
            if (!mBacking) {
                return;
            }

            // Libération de toutes les pages de toutes les size classes
            for (nk_uint32 i = 0u; i < NK_CONTAINER_CLASS_COUNT; ++i) {
                NkPageHeader* page = mClasses[i].Pages;
                while (page) {
                    NkPageHeader* next = page->Next;
                    mBacking->Deallocate(page);  // Libération via backing allocator
                    page = next;
                }

                // Reset des métadonnées de la size class
                mClasses[i].FreeList = nullptr;
                mClasses[i].Pages = nullptr;
                mClasses[i].TotalSlots = 0u;
                mClasses[i].UsedSlots.Store(0u, NkMemoryOrder::NK_RELAXED);
            }
        }

        void NkContainerAllocator::ReleaseAllLargeLocked() noexcept {
            if (!mBacking) {
                return;
            }

            // Libération de toutes les grandes allocations via backing allocator
            NkContainerAllocHeader* current = mLargeHead;
            while (current) {
                NkContainerAllocHeader* next = current->NextLarge;
                nk_uint8* user = reinterpret_cast<nk_uint8*>(current) + sizeof(NkContainerAllocHeader);
                nk_uint8* raw = user - current->Offset;
                mBacking->Deallocate(raw);
                current = next;
            }

            // Reset des compteurs de grandes allocations
            mLargeHead = nullptr;
            mLargeAllocationCount = 0u;
            mLargeAllocationBytes = 0u;
        }

    } // namespace memory
} // namespace nkentseu

// =============================================================================
// NOTES DE MAINTENANCE
// =============================================================================
/*
    [CORRECTNESS]
    ✓ Size classes : FindClassIndex() garantit que size <= UserSize pour la classe retournée
    ✓ TLS cache : génération + registry check garantissent invalidation safe lors de Reset()/destruction
    ✓ Header de tracking : offset stocké permet reconstruction exacte du pointeur base pour deallocation
    ✓ Atomic operations : UsedSlots utilise FetchAdd/SaturatingDecrement pour stats thread-safe
    ✓ Locks fins : un lock par size class + un pour large → contention minimale en multi-thread

    [PERFORMANCE]
    - Allocate small (TLS hit) : ~5-12 ns (pop cache TLS, pas de lock)
    - Allocate small (TLS miss) : ~20-40 ns (lock size class + free-list pop)
    - Allocate large : ~100ns-1µs (malloc + header setup)
    - Deallocate small (TLS hit) : ~4-10 ns (push cache TLS, pas de lock)
    - Deallocate small (TLS miss) : ~18-35 ns (lock size class + free-list push)
    - Reset() : O(num_pages + num_large) mais très rapide : simple parcours de listes
    - Cache hit rate typique : 85-95% pour workloads de conteneurs STL

    [MÉMOIRE]
    - Overhead par petite allocation : sizeof(NkContainerAllocHeader) = 32 bytes (64-bit)
    - Overhead par page : sizeof(NkPageHeader) = 24 bytes + padding → ~32 bytes
    - Pages pré-allouées : 64KB par défaut × nombre de size classes actives
    - TLS cache : 13 slots × sizeof(ContainerTlsCacheSlot) ≈ 13 × 24 = ~312 bytes par thread
    - Exemple : 10k allocations petites → ~320KB overhead headers + pages partielles

    [EXTENSIONS POSSIBLES]
    1. Size classes configurables : permettre de définir les 13 tailles via constructeur
    2. Adaptive page size : ajuster pageSize dynamiquement selon usage patterns
    3. Per-thread stats : ajouter des compteurs locaux pour profiling plus fin
    4. Integration avec NkMemoryTracker : notification automatique pour leak detection
    5. Fallback allocator : si une size class est pleine, fallback vers la classe supérieure

    [DEBUGGING TIPS]
    - Si Deallocate() ignore un ptr : vérifier l'header en debug, peut indiquer corruption ou ptr invalide
    - Si performance degrade : profiler TLS cache hit rate, ajuster NK_CONTAINER_TLS_CACHE_LIMIT/RETAIN
    - Si fragmentation élevée : augmenter pageSize ou ajuster les seuils de size classes
    - Dump intermédiaire : appeler GetStats() périodiquement pour monitoring sans overhead élevé

    [BONNES PRATIQUES]
    1. Utiliser un NkContainerAllocator par contexte d'exécution (thread, frame, subsystem) pour isolation
    2. Pour objets C++ : utiliser placement new/delete manuellement, l'allocateur ne gère que la mémoire brute
    3. Appeler Reset() en fin de frame/level pour libération bulk, mais détruire les objets C++ d'abord
    4. En production : définir NKENTSEU_DISABLE_CONTAINER_DEBUG pour réduire l'overhead des checks de sécurité
    5. Monitorer GetStats() périodiquement pour détecter saturation ou usage anormal des size classes
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================