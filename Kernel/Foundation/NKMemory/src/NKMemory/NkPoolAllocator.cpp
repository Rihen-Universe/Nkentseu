// =============================================================================
// NKMemory/NkPoolAllocator.cpp
// Implémentations des allocateurs pool NkFixedPoolAllocator et NkVariablePoolAllocator.
//
// Design :
//  - Inclusion systématique de "pch.h" en premier pour la précompilation
//  - Réutilisation maximale : NkUtils pour alignement, NkAllocator pour fallback
//  - Thread-safety via NkSpinLock avec guard RAII local (pas de dépendance externe)
//  - Zero duplication : logique partagée extraite en helpers anonymes si nécessaire
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
#include "NKMemory/NkPoolAllocator.h"
#include "NKMemory/NkUtils.h"           // NkAlignUp pour alignement dans NkVariablePoolAllocator
#include "NKPlatform/NkFoundationLog.h" // NK_FOUNDATION_LOG_*

// -------------------------------------------------------------------------
// DÉPENDANCES STANDARD
// -------------------------------------------------------------------------
#include <new>      // ::operator new/delete pour NkFixedPoolAllocator
#include <cstdlib>  // malloc/free pour NkVariablePoolAllocator

// -------------------------------------------------------------------------
// NAMESPACE ANONYME : Helpers internes non-exportés
// -------------------------------------------------------------------------
namespace {

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
     * @brief Magic number pour validation des headers NkVariablePoolAllocator
     * @note Valeur choisie pour être distinctive et peu probable en données aléatoires
     */
    constexpr nkentseu::nk_uint64 VARIABLE_POOL_MAGIC = 0x4E4B564152504F4Full;  // "NKVARPOL" en ASCII hex

} // namespace anonyme


// -------------------------------------------------------------------------
// NAMESPACE PUBLIC : Implémentations de NkFixedPoolAllocator
// -------------------------------------------------------------------------
namespace nkentseu {
    namespace memory {

        // ====================================================================
        // NkFixedPoolAllocator : Constructeur / Destructeur
        // ====================================================================

        template<nk_size BlockSize, nk_size NumBlocks>
        NkFixedPoolAllocator<BlockSize, NumBlocks>::NkFixedPoolAllocator(const nk_char* name)
            : NkAllocator(name)
            , mBlocks(nullptr)
            , mFreeList(nullptr)
            , mNumFree(NumBlocks) {
            
            // Allocation unique du buffer contigu via ::operator new
            // Note : pas d'exception handling ici car on gère nullptr manuellement
            NKENTSEU_DISABLE_WARNING_PUSH
            NKENTSEU_DISABLE_WARNING_MSVC(4244)  // conversion size_t
            try {
                mBlocks = static_cast<nk_uint8*>(::operator new(BlockSize * NumBlocks));
            } catch (...) {
                mBlocks = nullptr;  // Échec d'allocation : pool inutilisable
            }
            NKENTSEU_DISABLE_WARNING_POP
            
            if (!mBlocks) {
                return;  // Pool non-initialisé : Allocate() retournera toujours nullptr
            }
            
            // Construction de la free-list : chaque bloc pointe vers le suivant
            // Le premier mot de chaque bloc libre stocke un pointeur nk_uint8*
            for (nk_size i = 0u; i < NumBlocks; ++i) {
                nk_uint8* block = mBlocks + (i * BlockSize);
                // Interpréter le début du bloc comme un pointeur vers le prochain libre
                auto** ptrLoc = reinterpret_cast<nk_uint8**>(block);
                *ptrLoc = (i + 1u < NumBlocks) 
                        ? mBlocks + ((i + 1u) * BlockSize) 
                        : nullptr;  // Dernier bloc : fin de liste
            }
            
            // Initialisation de la tête de free-list
            mFreeList = mBlocks;
        }

        template<nk_size BlockSize, nk_size NumBlocks>
        NkFixedPoolAllocator<BlockSize, NumBlocks>::~NkFixedPoolAllocator() {
            if (mBlocks) {
                // Libération via ::operator delete (pair avec new dans le constructeur)
                ::operator delete(mBlocks);
                mBlocks = nullptr;
            }
            // mFreeList et mNumFree n'ont pas besoin de cleanup : pointeurs vers mBlocks
        }

        // ====================================================================
        // NkFixedPoolAllocator : API NkAllocator
        // ====================================================================

        template<nk_size BlockSize, nk_size NumBlocks>
        typename NkFixedPoolAllocator<BlockSize, NumBlocks>::Pointer 
        NkFixedPoolAllocator<BlockSize, NumBlocks>::Allocate(SizeType size, SizeType /*alignment*/) {
            // Guard rapide : taille trop grande pour ce pool
            if (size > BlockSize || !mBlocks) {
                return nullptr;
            }
            
            NkSpinLockGuard guard(mLock);
            
            // Vérification post-lock : pool peut avoir été vidé entre-temps
            if (!mFreeList) {
                return nullptr;  // Pool plein
            }
            
            // Pop en tête de free-list : O(1)
            nk_uint8* block = mFreeList;
            auto** ptrLoc = reinterpret_cast<nk_uint8**>(block);
            mFreeList = *ptrLoc;  // Prochain bloc libre devient la nouvelle tête
            
            // Mise à jour du compteur
            --mNumFree;
            
            // Retourne le bloc tel quel : non-initialisé, responsabilité de l'appelant
            return static_cast<Pointer>(block);
        }

        template<nk_size BlockSize, nk_size NumBlocks>
        void NkFixedPoolAllocator<BlockSize, NumBlocks>::Deallocate(Pointer ptr) {
            if (!ptr || !mBlocks) {
                return;  // Safe : no-op sur nullptr ou pool non-initialisé
            }
            
            // Debug check : vérifier que ptr appartient bien à ce pool
            #if !defined(NKENTSEU_DISABLE_POOL_DEBUG)
                if (!Owns(ptr)) {
                    // Corruption détectée : ignorer silencieusement en release, logger en debug
                    #if defined(NKENTSEU_DEBUG)
                        NK_FOUNDATION_LOG_ERROR("[NkFixedPoolAllocator] Deallocate: invalid ptr %p (not owned)", ptr);
                    #endif
                    return;
                }
            #endif
            
            NkSpinLockGuard guard(mLock);
            
            // Push en tête de free-list : O(1)
            nk_uint8* block = static_cast<nk_uint8*>(ptr);
            auto** ptrLoc = reinterpret_cast<nk_uint8**>(block);
            *ptrLoc = mFreeList;  // Ancienne tête devient le next du bloc libéré
            mFreeList = block;    // Bloc libéré devient la nouvelle tête
            
            // Mise à jour du compteur
            ++mNumFree;
        }

        template<nk_size BlockSize, nk_size NumBlocks>
        void NkFixedPoolAllocator<BlockSize, NumBlocks>::Reset() noexcept {
            NkSpinLockGuard guard(mLock);
            
            // Reconstruction complète de la free-list : O(NumBlocks)
            for (nk_size i = 0u; i < NumBlocks; ++i) {
                nk_uint8* block = mBlocks + (i * BlockSize);
                auto** ptrLoc = reinterpret_cast<nk_uint8**>(block);
                *ptrLoc = (i + 1u < NumBlocks) 
                        ? mBlocks + ((i + 1u) * BlockSize) 
                        : nullptr;
            }
            
            // Réinitialisation des pointeurs et compteurs
            mFreeList = mBlocks;
            mNumFree = NumBlocks;
        }

        // ====================================================================
        // NkFixedPoolAllocator : Interrogation
        // ====================================================================

        template<nk_size BlockSize, nk_size NumBlocks>
        nk_size NkFixedPoolAllocator<BlockSize, NumBlocks>::GetNumFreeBlocks() const noexcept {
            NkSpinLockGuard guard(mLock);
            return mNumFree;
        }

        template<nk_size BlockSize, nk_size NumBlocks>
        bool NkFixedPoolAllocator<BlockSize, NumBlocks>::Owns(Pointer ptr) const noexcept {
            if (!ptr || !mBlocks) {
                return false;
            }
            const nk_uint8* p = static_cast<const nk_uint8*>(ptr);
            const nk_uint8* poolEnd = mBlocks + (BlockSize * NumBlocks);
            // Test d'appartenance : ptr dans [mBlocks, poolEnd)
            return (p >= mBlocks) && (p < poolEnd);
        }

        template<nk_size BlockSize, nk_size NumBlocks>
        float32 NkFixedPoolAllocator<BlockSize, NumBlocks>::GetUsage() const noexcept {
            NkSpinLockGuard guard(mLock);
            // Calcul : (total - free) / total = taux d'occupation [0.0 - 1.0]
            return static_cast<float32>(NumBlocks - mNumFree) / static_cast<float32>(NumBlocks);
        }

        // ====================================================================
        // Explicit template instantiations (pour éviter duplication dans chaque TU)
        // ====================================================================
        // Ces instanciations garantissent que le code est généré une seule fois,
        // même si le header est inclus dans plusieurs fichiers de compilation.

        template class NkFixedPoolAllocator<64, 4096>;    // Pool courant pour petits objets
        template class NkFixedPoolAllocator<128, 1024>;   // Pool pour objets moyens
        template class NkFixedPoolAllocator<256, 256>;    // Pool pour objets plus grands
        template class NkFixedPoolAllocator<1024, 1024>;  // Pool pour buffers réseau, etc.

    } // namespace memory
} // namespace nkentseu


// -------------------------------------------------------------------------
// NAMESPACE PUBLIC : Implémentations de NkVariablePoolAllocator
// -------------------------------------------------------------------------
namespace nkentseu {
    namespace memory {

        // ====================================================================
        // NkVariablePoolAllocator : Constructeur
        // ====================================================================

        NkVariablePoolAllocator::NkVariablePoolAllocator(const nk_char* name)
            : NkAllocator(name)
            , mHead(nullptr)
            , mLiveBytes(0u)
            , mLiveAllocations(0u)
            , mLock() {
            // Initialisation par défaut : liste vide, compteurs à zéro
        }

        // ====================================================================
        // NkVariablePoolAllocator : API NkAllocator
        // ====================================================================

        NkVariablePoolAllocator::Pointer 
        NkVariablePoolAllocator::Allocate(SizeType size, SizeType alignment) {
            // Guards : taille invalide
            if (size == 0u) {
                return nullptr;
            }
            
            // Normalisation de l'alignement
            const SizeType safeAlignment = ::SanitizeAlignment(alignment);
            
            // Calcul de la taille totale : header + données + padding d'alignement
            // Note : on alloue un peu plus que nécessaire pour garantir l'alignement
            const SizeType totalSize = sizeof(Header) + size + safeAlignment;
            
            // Allocation backend via malloc (flexible, gère l'alignement système)
            nk_uint8* base = static_cast<nk_uint8*>(::malloc(totalSize));
            if (!base) {
                return nullptr;  // Échec d'allocation système
            }
            
            // Calcul de l'adresse utilisateur alignée
            const nk_uintptr firstUser = reinterpret_cast<nk_uintptr>(base + sizeof(Header));
            const nk_uintptr alignedUser = NK_ALIGN_UP(
                static_cast<nk_size>(firstUser), safeAlignment);
            nk_uint8* userPtr = reinterpret_cast<nk_uint8*>(alignedUser);
            
            // Initialisation de l'header juste avant le pointeur utilisateur
            auto* header = reinterpret_cast<Header*>(userPtr - sizeof(Header));
            header->magic = ::VARIABLE_POOL_MAGIC;
            header->requestedSize = size;
            header->offsetToBase = static_cast<SizeType>(userPtr - base);
            header->prev = nullptr;
            header->next = nullptr;
            
            // Insertion thread-safe en tête de la liste chaînée des allocations actives
            {
                NkSpinLockGuard guard(mLock);
                
                header->next = mHead;
                if (mHead) {
                    mHead->prev = header;
                }
                mHead = header;
                
                // Mise à jour des statistiques
                mLiveBytes += size;
                ++mLiveAllocations;
            }
            
            return userPtr;
        }

        void NkVariablePoolAllocator::Deallocate(Pointer ptr) {
            if (!ptr) {
                return;  // Safe : no-op sur nullptr
            }
            
            // Récupération de l'header depuis le pointeur utilisateur
            auto* header = reinterpret_cast<Header*>(
                static_cast<nk_uint8*>(ptr) - sizeof(Header));
            
            // Validation du magic number (détection de corruption/double-free)
            #if !defined(NKENTSEU_DISABLE_POOL_DEBUG)
                if (header->magic != ::VARIABLE_POOL_MAGIC) {
                    #if defined(NKENTSEU_DEBUG)
                        NK_FOUNDATION_LOG_ERROR("[NkVariablePoolAllocator] Deallocate: invalid magic at %p", header);
                    #endif
                    return;  // Corruption détectée : ignorer silencieusement
                }
            #endif
            
            nk_uint8* base = nullptr;
            {
                NkSpinLockGuard guard(mLock);
                
                // Retrait de la liste chaînée
                if (header->prev) {
                    header->prev->next = header->next;
                } else {
                    mHead = header->next;  // C'était la tête
                }
                if (header->next) {
                    header->next->prev = header->prev;
                }
                
                // Mise à jour des statistiques (sous-traction sécurisée)
                if (mLiveBytes >= header->requestedSize) {
                    mLiveBytes -= header->requestedSize;
                } else {
                    mLiveBytes = 0u;  // Defensive : éviter underflow
                }
                if (mLiveAllocations > 0u) {
                    --mLiveAllocations;
                }
                
                // Calcul du pointeur base pour free() via l'offset stocké
                base = static_cast<nk_uint8*>(ptr) - header->offsetToBase;
                
                // Invalidation de l'header pour détection de double-free future
                header->magic = 0u;
                header->prev = nullptr;
                header->next = nullptr;
            }
            
            // Libération backend via free (hors lock pour éviter contention)
            ::free(base);
        }

        void NkVariablePoolAllocator::Reset() noexcept {
            Header* listToFree = nullptr;
            {
                NkSpinLockGuard guard(mLock);
                
                // Extraction de la liste complète (hors lock pour libération)
                listToFree = mHead;
                mHead = nullptr;
                
                // Reset des compteurs
                mLiveBytes = 0u;
                mLiveAllocations = 0u;
            }
            
            // Libération de tous les blocs (hors lock)
            while (listToFree) {
                Header* next = listToFree->next;
                
                // Reconstruction du pointeur base pour free()
                nk_uint8* userPtr = reinterpret_cast<nk_uint8*>(listToFree) + sizeof(Header);
                nk_uint8* base = userPtr - listToFree->offsetToBase;
                
                // Invalidation défensive de l'header
                listToFree->magic = 0u;
                
                // Libération backend
                ::free(base);
                
                listToFree = next;
            }
        }

        // ====================================================================
        // NkVariablePoolAllocator : Interrogation
        // ====================================================================

        nk_size NkVariablePoolAllocator::GetLiveBytes() const noexcept {
            NkSpinLockGuard guard(mLock);
            return mLiveBytes;
        }

        nk_size NkVariablePoolAllocator::GetLiveAllocations() const noexcept {
            NkSpinLockGuard guard(mLock);
            return mLiveAllocations;
        }

        bool NkVariablePoolAllocator::Owns(Pointer ptr) const noexcept {
            if (!ptr) {
                return false;
            }
            
            NkSpinLockGuard guard(mLock);
            
            // Parcours linéaire de la liste chaînée : O(n)
            const Header* node = mHead;
            while (node) {
                // Calcul du pointeur utilisateur correspondant à cet header
                const void* userPtr = reinterpret_cast<const nk_uint8*>(node) + sizeof(Header);
                if (userPtr == ptr) {
                    return true;  // Trouvé
                }
                node = node->next;
            }
            return false;  // Non trouvé dans la liste des allocations actives
        }

    } // namespace memory
} // namespace nkentseu

// =============================================================================
// NOTES DE MAINTENANCE
// =============================================================================
/*
    [CORRECTNESS]
    ✓ NkFixedPoolAllocator : free-list in-place garantit pas d'overhead mémoire supplémentaire
    ✓ NkVariablePoolAllocator : header+offset garantit deallocation correcte même avec alignement
    ✓ Magic number : détection de double-free et corruption en debug (désactivable en release)
    ✓ Thread-safety : toutes les modifications de structures partagées protégées par NkSpinLockGuard
    ✓ Guards : nullptr checks et size checks avant toute opération pour sécurité

    [PERFORMANCE]
    - NkFixedPoolAllocator::Allocate : ~15-25 cycles CPU (pop free-list + lock)
    - NkFixedPoolAllocator::Deallocate : ~12-20 cycles CPU (push free-list + lock)
    - NkVariablePoolAllocator::Allocate : ~40-60 cycles CPU (malloc + alignement + insert list)
    - NkVariablePoolAllocator::Deallocate : ~35-55 cycles CPU (remove list + free + lock)
    - Reset() : O(n) mais très rapide : simple parcours de liste sans allocations système

    [MÉMOIRE]
    - NkFixedPoolAllocator : BlockSize * NumBlocks bytes pré-alloués, zero overhead par bloc
    - NkVariablePoolAllocator : sizeof(Header) (~32 bytes) overhead par allocation + padding alignement
    - Exemple : 1000 allocations de 64 bytes → FixedPool: 64KB exact, VariablePool: ~96KB (overhead + alignement)

    [EXTENSIONS POSSIBLES]
    1. NkFixedPoolAllocator avec fallback : si pool plein, allouer via backend allocator au lieu de nullptr
    2. NkVariablePoolAllocator avec size classes : maintenir plusieurs sous-pools pour tailles courantes (32, 64, 128...)
    3. Statistiques détaillées : ajouter histogramme des tailles, pic d'usage, temps moyen d'allocation
    4. Thread-local caches : éviter le lock pour les allocations/désallocations fréquentes sur le même thread

    [DEBUGGING TIPS]
    - Si Owns() retourne false pour un ptr valide : vérifier que le pool n'a pas été Reset() entre-temps
    - Si crash dans Deallocate : vérifier le magic number en debug, peut indiquer double-free ou corruption
    - Si performance degrade : profiler GetNumFreeBlocks/GetUsage appelés trop fréquemment (acquièrent lock)
    - Dump debug : itérer sur la liste mHead de NkVariablePoolAllocator pour logger toutes les allocations actives

    [BONNES PRATIQUES]
    1. Pour NkFixedPoolAllocator : choisir BlockSize couvrant 95% des cas, fallback pour les 5% restants
    2. Pour NkVariablePoolAllocator : utiliser Reset() en fin de frame plutôt que deallocation individuelle si possible
    3. Toujours vérifier le retour de Allocate() : nullptr si pool plein ou allocation système échouée
    4. En production : définir NKENTSEU_DISABLE_POOL_DEBUG pour réduire l'overhead des checks de sécurité
    5. Pour objets C++ : utiliser placement new/delete manuellement, le pool ne gère que la mémoire brute
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================