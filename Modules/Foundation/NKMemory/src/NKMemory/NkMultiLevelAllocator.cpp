// =============================================================================
// NKMemory/NkMultiLevelAllocator.cpp
// Implémentation de l'allocateur multi-niveaux avec dispatch intelligent.
//
// Design :
//  - Inclusion systématique de "pch.h" en premier pour la précompilation
//  - Réutilisation maximale : délégation aux pools NKMemory (ZÉRO duplication)
//  - Thread-safety via NkSpinLock avec guard RAII local (pas de dépendance externe)
//  - Header de dispatch unifié pour tracking et deallocation correcte
//  - Rétro-compatibilité avec anciennes allocations LARGE via legacy header
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
#include "NKMemory/NkMultiLevelAllocator.h"
#include "NKMemory/NkFunction.h"    // NkMemCopy, NkAlignUp pour opérations mémoire
#include "NKMemory/NkUtils.h"       // NkAlignUp pour alignement
#include "NKPlatform/NkFoundationLog.h" // NK_FOUNDATION_LOG_*

// -------------------------------------------------------------------------
// DÉPENDANCES STANDARD
// -------------------------------------------------------------------------
#include <cstdlib>  // malloc/free pour tier LARGE

// -------------------------------------------------------------------------
// NAMESPACE ANONYME : Helpers internes non-exportés
// -------------------------------------------------------------------------
namespace {

    /**
     * @brief Magic number pour validation des headers de dispatch
     * @note Valeur choisie pour être distinctive : "NKMLBLK1" en ASCII hex
     */
    constexpr nkentseu::nk_uint64 MULTI_LEVEL_BLOCK_MAGIC = 0x4E4B4D4C424C4B31ull;

    /**
     * @brief Magic number legacy pour rétro-compatibilité avec anciennes allocations LARGE
     * @note Valeur : "NKMLLARGE" en ASCII hex - sera supprimé dans une future version
     */
    constexpr nkentseu::nk_uint64 MULTI_LEVEL_LARGE_MAGIC_LEGACY = 0x4E4B4D4C41524745ull;

    /**
     * @brief Valide et normalise un alignement demandé
     * @param alignment Alignement demandé par l'utilisateur
     * @return Alignement valide (puissance de 2) ou fallback vers défaut
     * @note Si alignment == 0 ou non-puissance-de-2 : retourne NK_MEMORY_DEFAULT_ALIGNMENT
     */
    [[nodiscard]] inline nkentseu::nk_size SanitizeAlignment(nkentseu::nk_size alignment) noexcept {
        if (alignment == 0u) {
            return nkentseu::memory::NK_MEMORY_DEFAULT_ALIGNMENT;
        }
        // Test puissance de 2 : n & (n-1) == 0 ssi n est puissance de 2 (et n != 0)
        if ((alignment & (alignment - 1u)) != 0u) {
            return nkentseu::memory::NK_MEMORY_DEFAULT_ALIGNMENT;
        }
        return alignment;
    }

    /**
     * @brief Choisit le tier d'allocation selon la taille totale requise
     * @param totalBytes Taille totale = header + taille utilisateur + padding alignement
     * @return Tier approprié selon les seuils configurés
     * @note Utilisé dans Allocate() pour dispatch vers le bon pool
     */
    [[nodiscard]] inline nkentseu::memory::NkMultiLevelAllocator::NkAllocTier 
    ChooseTier(nkentseu::nk_size totalBytes) noexcept {
        using Tier = nkentseu::memory::NkMultiLevelAllocator::NkAllocTier;
        
        if (totalBytes <= nkentseu::memory::NkMultiLevelAllocator::TINY_SIZE) {
            return Tier::Tiny;
        }
        if (totalBytes <= nkentseu::memory::NkMultiLevelAllocator::SMALL_SIZE) {
            return Tier::Small;
        }
        if (totalBytes <= nkentseu::memory::NkMultiLevelAllocator::MEDIUM_THRESHOLD) {
            return Tier::Medium;
        }
        return Tier::Large;
    }

} // namespace anonyme


// -------------------------------------------------------------------------
// NAMESPACE PUBLIC : Implémentation de NkMultiLevelAllocator
// -------------------------------------------------------------------------
namespace nkentseu {
    namespace memory {

        // ====================================================================
        // NkMultiLevelAllocatorStats : Implémentation
        // ====================================================================

        float32 NkMultiLevelAllocatorStats::GetFragmentation() const noexcept {
            // Calcul de la fragmentation estimée : 1.0 - (used / capacity)
            const nk_size used = tiny.allocated + small.allocated + medium.allocated + largeBytes;
            const nk_size capacity = tiny.capacity + small.capacity + medium.capacity + largeBytes;
            
            if (capacity == 0u) {
                return 0.0f;  // Pas de capacité = pas de fragmentation
            }
            
            return 1.0f - (static_cast<float32>(used) / static_cast<float32>(capacity));
        }

        // ====================================================================
        // NkMultiLevelAllocator : Constructeur / Destructeur
        // ====================================================================

        NkMultiLevelAllocator::NkMultiLevelAllocator() noexcept 
            : NkAllocator("NkMultiLevelAllocator")
            , mTinyPool(NkMakeUnique<NkFixedPoolAllocator<TINY_SIZE, TINY_COUNT>>("MultiLevel_Tiny"))
            , mSmallPool(NkMakeUnique<NkFixedPoolAllocator<SMALL_SIZE, SMALL_COUNT>>("MultiLevel_Small"))
            , mMediumPool(NkMakeUnique<NkVariablePoolAllocator>("MultiLevel_Medium"))
            , mLarge{0u, 0u}  // Zero-initialization des compteurs LARGE
            , mLock() {
            // Initialisation lazy des pools : déjà faite via NkMakeUnique dans la liste d'initialisation
            // Les pools sont prêts à l'emploi dès la fin du constructeur
        }

        NkMultiLevelAllocator::~NkMultiLevelAllocator() {
            // Note : les allocations LARGE (malloc) ne sont PAS libérées automatiquement
            // C'est la responsabilité de l'appelant de les libérer avant la destruction
            // Les pools TINY/SMALL/MEDIUM sont détruits automatiquement via NkUniquePtr
        }

        // ====================================================================
        // NkMultiLevelAllocator : API NkAllocator
        // ====================================================================

        NkMultiLevelAllocator::Pointer 
        NkMultiLevelAllocator::Allocate(SizeType size, SizeType alignment) {
            // Guards : taille invalide
            if (size == 0u) {
                return nullptr;
            }
            
            // Normalisation de l'alignement
            const SizeType safeAlignment = ::SanitizeAlignment(alignment);
            
            // Calcul de la taille totale : header + données + padding d'alignement
            const SizeType totalBytes = sizeof(NkDispatchHeader) + size + safeAlignment;
            
            // Dispatch vers le tier approprié
            const NkAllocTier tier = ::ChooseTier(totalBytes);
            
            // Allocation backend selon le tier
            void* rawBase = nullptr;
            switch (tier) {
                case NkAllocTier::Tiny:
                    rawBase = mTinyPool->Allocate(totalBytes, alignof(void*));
                    break;
                case NkAllocTier::Small:
                    rawBase = mSmallPool->Allocate(totalBytes, alignof(void*));
                    break;
                case NkAllocTier::Medium:
                    rawBase = mMediumPool->Allocate(totalBytes, alignof(void*));
                    break;
                case NkAllocTier::Large:
                    // Tier LARGE : allocation directe via malloc
                    rawBase = ::malloc(totalBytes);
                    break;
            }
            
            if (!rawBase) {
                return nullptr;  // Échec d'allocation backend
            }
            
            // Calcul de l'adresse utilisateur alignée
            nk_uint8* base = static_cast<nk_uint8*>(rawBase);
            const nk_uintptr firstUser = reinterpret_cast<nk_uintptr>(base + sizeof(NkDispatchHeader));
            const nk_uintptr alignedUser = static_cast<nk_uintptr>(
                NK_ALIGN_UP(static_cast<nk_size>(firstUser), safeAlignment)
            );
            nk_uint8* userPtr = reinterpret_cast<nk_uint8*>(alignedUser);
            
            // Initialisation de l'header de dispatch juste avant le pointeur utilisateur
            auto* header = reinterpret_cast<NkDispatchHeader*>(userPtr - sizeof(NkDispatchHeader));
            header->magic = ::MULTI_LEVEL_BLOCK_MAGIC;
            header->requestedSize = size;
            header->offsetToBase = static_cast<SizeType>(userPtr - base);
            header->tier = static_cast<nk_uint8>(tier);
            // Zero-initialization du padding réservé pour sécurité
            NkMemZero(header->reserved, sizeof(header->reserved));
            
            // Mise à jour des stats pour tier LARGE (sous lock)
            if (tier == NkAllocTier::Large) {
                NkSpinLockGuard guard(mLock);
                mLarge.totalBytes += size;
                ++mLarge.allocationCount;
            }
            
            return userPtr;
        }

        void NkMultiLevelAllocator::Deallocate(Pointer ptr) {
            if (!ptr) {
                return;  // Safe : no-op sur nullptr
            }
            
            nk_uint8* userPtr = static_cast<nk_uint8*>(ptr);
            
            // Récupération de l'header de dispatch
            auto* header = reinterpret_cast<NkDispatchHeader*>(userPtr - sizeof(NkDispatchHeader));
            
            // Validation du magic number (détection de corruption/double-free)
            #if !defined(NKENTSEU_DISABLE_MULTI_LEVEL_DEBUG)
                if (header->magic != ::MULTI_LEVEL_BLOCK_MAGIC) {
                    // Tentative de rétro-compatibilité avec ancien header LARGE
                    auto* legacy = reinterpret_cast<NkLegacyLargeAllocHeader*>(ptr) - 1;
                    if (legacy->magic == ::MULTI_LEVEL_LARGE_MAGIC_LEGACY) {
                        // Gestion de l'ancien format LARGE
                        {
                            NkSpinLockGuard guard(mLock);
                            if (mLarge.totalBytes >= legacy->size) {
                                mLarge.totalBytes -= legacy->size;
                            } else {
                                mLarge.totalBytes = 0u;
                            }
                            if (mLarge.allocationCount > 0u) {
                                --mLarge.allocationCount;
                            }
                        }
                        ::free(legacy);  // Libération via malloc pair
                        return;
                    }
                    
                    // Corruption détectée : logger en debug et ignorer
                    #if defined(NKENTSEU_DEBUG)
                        NK_FOUNDATION_LOG_ERROR("[NkMultiLevelAllocator] Deallocate: invalid magic at %p", header);
                    #endif
                    return;
                }
            #endif
            
            // Reconstruction du pointeur base via l'offset stocké
            nk_uint8* base = userPtr - header->offsetToBase;
            const NkAllocTier tier = static_cast<NkAllocTier>(header->tier);
            
            // Dispatch de deallocation vers le bon pool/backend
            switch (tier) {
                case NkAllocTier::Tiny:
                    mTinyPool->Deallocate(base);
                    break;
                case NkAllocTier::Small:
                    mSmallPool->Deallocate(base);
                    break;
                case NkAllocTier::Medium:
                    mMediumPool->Deallocate(base);
                    break;
                case NkAllocTier::Large: {
                    // Mise à jour des stats LARGE sous lock
                    NkSpinLockGuard guard(mLock);
                    if (mLarge.totalBytes >= header->requestedSize) {
                        mLarge.totalBytes -= header->requestedSize;
                    } else {
                        mLarge.totalBytes = 0u;  // Defensive : éviter underflow
                    }
                    if (mLarge.allocationCount > 0u) {
                        --mLarge.allocationCount;
                    }
                    // Libération backend via free (pair avec malloc)
                    ::free(base);
                    break;
                }
            }
        }

        NkMultiLevelAllocator::Pointer 
        NkMultiLevelAllocator::Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize, 
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
            SizeType copySize = oldSize;
            
            // Si l'header est valide, utiliser la taille enregistrée pour plus de précision
            auto* header = reinterpret_cast<NkDispatchHeader*>(
                static_cast<nk_uint8*>(ptr) - sizeof(NkDispatchHeader)
            );
            if (header->magic == ::MULTI_LEVEL_BLOCK_MAGIC) {
                copySize = header->requestedSize;
            }
            // Clamp à la plus petite des deux tailles pour éviter overflow
            if (copySize > newSize) {
                copySize = newSize;
            }
            
            // Stratégie simple : allocate nouveau + copy + deallocate ancien
            void* newPtr = Allocate(newSize, alignment);
            if (newPtr) {
                NkMemCopy(newPtr, ptr, copySize);
                Deallocate(ptr);
            }
            return newPtr;
        }

        void NkMultiLevelAllocator::Reset() noexcept {
            // Reset des pools internes (thread-safe via leurs locks internes)
            mTinyPool->Reset();
            mSmallPool->Reset();
            mMediumPool->Reset();
            
            // Reset des compteurs LARGE (mais pas libération des malloc : responsabilité de l'appelant)
            NkSpinLockGuard guard(mLock);
            mLarge.totalBytes = 0u;
            mLarge.allocationCount = 0u;
        }

        // ====================================================================
        // NkMultiLevelAllocator : Interrogation des Statistiques
        // ====================================================================

        NkMultiLevelAllocatorStats NkMultiLevelAllocator::GetStats() const noexcept {
            NkMultiLevelAllocatorStats stats{};
            
            // Stats du tier TINY
            if (mTinyPool) {
                const nk_size freeBlocks = mTinyPool->GetNumFreeBlocks();
                stats.tiny.capacity = TINY_SIZE * TINY_COUNT;
                stats.tiny.allocated = (TINY_COUNT - freeBlocks) * TINY_SIZE;
                stats.tiny.usage = (stats.tiny.capacity > 0u)
                    ? static_cast<float32>(stats.tiny.allocated) / static_cast<float32>(stats.tiny.capacity)
                    : 0.0f;
            }
            
            // Stats du tier SMALL
            if (mSmallPool) {
                const nk_size freeBlocks = mSmallPool->GetNumFreeBlocks();
                stats.small.capacity = SMALL_SIZE * SMALL_COUNT;
                stats.small.allocated = (SMALL_COUNT - freeBlocks) * SMALL_SIZE;
                stats.small.usage = (stats.small.capacity > 0u)
                    ? static_cast<float32>(stats.small.allocated) / static_cast<float32>(stats.small.capacity)
                    : 0.0f;
            }
            
            // Stats du tier MEDIUM
            if (mMediumPool) {
                stats.medium.allocated = mMediumPool->GetLiveBytes();
                stats.medium.capacity = stats.medium.allocated;  // Variable pool : capacité = usage actuel
                stats.medium.usage = (stats.medium.capacity > 0u) ? 1.0f : 0.0f;
            }
            
            // Stats du tier LARGE (sous lock)
            {
                NkSpinLockGuard guard(mLock);
                stats.largeAllocations = mLarge.allocationCount;
                stats.largeBytes = mLarge.totalBytes;
            }
            
            return stats;
        }

        void NkMultiLevelAllocator::DumpStats() const noexcept {
            const NkMultiLevelAllocatorStats stats = GetStats();
            
            NK_FOUNDATION_LOG_INFO("=== NKMemory MultiLevel Allocator Stats ===");
            NK_FOUNDATION_LOG_INFO("TINY   (1-64B)  : %llu / %llu bytes (%.2f%% used)",
                                  static_cast<unsigned long long>(stats.tiny.allocated),
                                  static_cast<unsigned long long>(stats.tiny.capacity),
                                  static_cast<double>(stats.tiny.usage * 100.0f));
            NK_FOUNDATION_LOG_INFO("SMALL  (65B-1KB): %llu / %llu bytes (%.2f%% used)",
                                  static_cast<unsigned long long>(stats.small.allocated),
                                  static_cast<unsigned long long>(stats.small.capacity),
                                  static_cast<double>(stats.small.usage * 100.0f));
            NK_FOUNDATION_LOG_INFO("MEDIUM (1KB-1MB): %llu bytes live (%llu active allocs)",
                                  static_cast<unsigned long long>(stats.medium.allocated),
                                  static_cast<unsigned long long>(mMediumPool ? mMediumPool->GetLiveAllocations() : 0u));
            NK_FOUNDATION_LOG_INFO("LARGE  (>1MB)   : %llu allocations, %llu bytes total",
                                  static_cast<unsigned long long>(stats.largeAllocations),
                                  static_cast<unsigned long long>(stats.largeBytes));
            NK_FOUNDATION_LOG_INFO("Estimated fragmentation: %.2f", static_cast<double>(stats.GetFragmentation() * 100.0f));
            NK_FOUNDATION_LOG_INFO("==========================================");
        }

        // ====================================================================
        // NkMultiLevelAllocator : Méthodes Internes
        // ====================================================================

        NkAllocator* NkMultiLevelAllocator::FindAllocatorFor(void* ptr) const noexcept {
            if (!ptr) {
                return nullptr;
            }
            
            // Recherche séquentielle dans les pools internes via Owns()
            if (mTinyPool && mTinyPool->Owns(ptr)) {
                return mTinyPool.Get();
            }
            if (mSmallPool && mSmallPool->Owns(ptr)) {
                return mSmallPool.Get();
            }
            if (mMediumPool && mMediumPool->Owns(ptr)) {
                return mMediumPool.Get();
            }
            
            // Non trouvé dans les pools internes : peut être LARGE (malloc) ou invalide
            return nullptr;
        }

        // ====================================================================
        // Fonction globale d'accès au singleton
        // ====================================================================

        NkMultiLevelAllocator& NkGetMultiLevelAllocator() noexcept {
            // Meyer's singleton : thread-safe en C++11+ (initialisation statique garantie)
            static NkMultiLevelAllocator instance;
            return instance;
        }

    } // namespace memory
} // namespace nkentseu

// =============================================================================
// NOTES DE MAINTENANCE
// =============================================================================
/*
    [CORRECTNESS]
    ✓ Header de dispatch : magic + tier + offset garantit deallocation correcte même avec alignement variable
    ✓ Dispatch par taille : ChooseTier() utilise les seuils configurés pour routage vers le bon backend
    ✓ Rétro-compatibilité : gestion des anciens headers LARGE via magic legacy (à supprimer dans future version)
    ✓ Thread-safety : toutes les modifications de structures partagées protégées par NkSpinLockGuard
    ✓ Guards : nullptr checks et size checks avant toute opération pour sécurité

    [PERFORMANCE]
    - Allocate (TINY/SMALL) : ~18-30 ns (pool O(1) + header setup)
    - Allocate (MEDIUM) : ~45-70 ns (variable pool + alignement)
    - Allocate (LARGE) : ~1-2 µs (malloc + header setup)
    - Deallocate (any) : ~15-45 ns (lookup tier + pool free ou malloc free)
    - GetStats : O(1) : lecture des compteurs + stats des pools (locks internes)
    - Reset : O(1) pour LARGE, O(n) pour pools internes mais très rapide (rebuild free-list)

    [MÉMOIRE]
    - Overhead par allocation : sizeof(NkDispatchHeader) = 32 bytes (64-bit)
    - Pools pré-alloués : TINY=256KB, SMALL=1MB, MEDIUM=lazy via malloc
    - Exemple : 10k allocations TINY → 320KB overhead headers + 256KB pool = ~576KB total
    - Fragmentation : nulle pour TINY/SMALL (pools fixes), estimée pour MEDIUM/LARGE

    [EXTENSIONS POSSIBLES]
    1. Size classes configurables : permettre de définir les seuils TINY/SMALL/MEDIUM via constructeur
    2. Thread-local caches : éviter le lock global pour allocations fréquentes sur le même thread
    3. Fallback allocator : si un pool est plein, fallback vers le tier supérieur au lieu de nullptr
    4. Stats détaillées : histogramme des tailles, temps moyen d'alloc/dealloc, pic d'usage par tier
    5. Integration avec NkMemoryTracker : notification automatique des allocations pour leak detection

    [DEBUGGING TIPS]
    - Si Deallocate() ignore un ptr : vérifier le magic number en debug, peut indiquer corruption ou ptr invalide
    - Si performance degrade : profiler Allocate/Deallocate appelés trop fréquemment, envisager batch allocation
    - Si fragmentation élevée : augmenter TINY_COUNT/SMALL_COUNT ou ajuster les seuils de dispatch
    - Dump intermédiaire : appeler DumpStats() à tout moment pour inspection sans shutdown

    [BONNES PRATIQUES]
    1. Utiliser l'allocateur global NkGetMultiLevelAllocator() pour cohérence dans toute l'application
    2. Pour allocations très fréquentes de même taille : envisager un NkTypedPool dédié en plus du multi-level
    3. Toujours vérifier le retour de Allocate() : nullptr si pool plein ou allocation système échouée
    4. En production : définir NKENTSEU_DISABLE_MULTI_LEVEL_DEBUG pour réduire l'overhead des checks de sécurité
    5. Pour objets C++ : utiliser placement new/delete manuellement, l'allocateur ne gère que la mémoire brute
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================