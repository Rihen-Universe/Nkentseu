// =============================================================================
// NKMemory/NkTracker.cpp
// Implémentation du tracker de fuites mémoire O(1) via table de hachage.
//
// Design :
//  - Inclusion systématique de "pch.h" en premier pour la précompilation
//  - Réutilisation des primitives NKCore (ZÉRO duplication de logique)
//  - Thread-safety via NkSpinLock avec acquisition/release explicites
//  - Overhead minimal : allocation directe via ::malloc pour les entrées
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
#include "NKMemory/NkTracker.h"

// -------------------------------------------------------------------------
// DÉPENDANCES OPTIONNELLES (debug/verbose)
// -------------------------------------------------------------------------
#include "NKPlatform/NkFoundationLog.h"    // NK_FOUNDATION_LOG_*

// -------------------------------------------------------------------------
// CONFIGURATION CONDITIONNELLE
// -------------------------------------------------------------------------
// En release avec NKENTSEU_DISABLE_MEMORY_TRACKING, toutes les fonctions
// deviennent des no-ops compilés en inline pour zero overhead.

#if !defined(NKENTSEU_DISABLE_MEMORY_TRACKING)

    // -------------------------------------------------------------------------
    // NAMESPACE PUBLIC : Implémentations de NkMemoryTracker
    // -------------------------------------------------------------------------
    namespace nkentseu {
        namespace memory {

            // ====================================================================
            // Constructeur / Destructeur
            // ====================================================================

            NkMemoryTracker::NkMemoryTracker() noexcept 
                : mLiveAllocations(0)
                , mTotalLiveBytes(0)
                , mPeakBytes(0)
                , mTotalAllocations(0) {
                // Zero-initialisation explicite des buckets (défensive)
                for (nk_size i = 0; i < NK_TRACKER_BUCKET_COUNT; ++i) {
                    mBuckets[i] = nullptr;
                }
            }

            NkMemoryTracker::~NkMemoryTracker() {
                // Cleanup thread-safe : acquiert le lock avant de libérer
                NkSpinLockGuard guard(mLock);
                ClearInternal();
            }

            // ====================================================================
            // API de Tracking
            // ====================================================================

            void NkMemoryTracker::Register(const NkAllocationInfo& info) noexcept {
                if (!info.userPtr || info.size == 0) {
                    return;  // Ignorer les allocations nulles ou vides
                }

                // Acquisition du lock pour section critique thread-safe
                NkSpinLockGuard guard(mLock);

                const nk_size bucket = HashPtr(info.userPtr);
                Entry* existing = FindEntry(info.userPtr);

                if (existing) {
                    // Mise à jour d'une entrée existante (realloc ou re-register)
                    // Déduire l'ancienne taille avant mise à jour
                    if (mTotalLiveBytes >= existing->Info.size) {
                        mTotalLiveBytes -= existing->Info.size;
                    } else {
                        mTotalLiveBytes = 0;  // Defensive : éviter underflow
                    }
                    existing->Info = info;  // Copie des nouvelles métadonnées
                } else {
                    // Nouvelle entrée : allocation via malloc (évite récursion avec NkAllocator)
                    Entry* newEntry = static_cast<Entry*>(::malloc(sizeof(Entry)));
                    if (!newEntry) {
                        // Échec d'allocation du tracker lui-même : échec silencieux
                        // (mieux que crash, mais idéalement logger en debug)
                        #if defined(NKENTSEU_MEMORY_TRACKING_VERBOSE) && defined(NKENTSEU_DEBUG)
                            NK_FOUNDATION_LOG_ERROR("[NKMemory] Failed to allocate tracker entry for %p", info.userPtr);
                        #endif
                        return;
                    }
                    newEntry->Info = info;
                    newEntry->Next = mBuckets[bucket];  // Insertion en tête (O(1))
                    mBuckets[bucket] = newEntry;
                    ++mLiveAllocations;
                }

                // Mise à jour des statistiques globales
                mTotalLiveBytes += info.size;
                ++mTotalAllocations;

                // Mise à jour du peak si nécessaire
                if (mTotalLiveBytes > mPeakBytes) {
                    mPeakBytes = mTotalLiveBytes;
                }

                // Logging verbose en debug (optionnel)
                #if defined(NKENTSEU_MEMORY_TRACKING_VERBOSE) && defined(NKENTSEU_DEBUG)
                    NK_FOUNDATION_LOG_DEBUG("[NKMemory] Registered: %p size=%zu tag=%u at %s:%d",
                                           info.userPtr, info.size, 
                                           static_cast<unsigned>(info.tag),
                                           info.file ? info.file : "<unknown>",
                                           info.line);
                #endif
            }

            void NkMemoryTracker::Unregister(void* ptr) noexcept {
                if (!ptr) {
                    return;  // Safe : no-op sur nullptr
                }

                NkSpinLockGuard guard(mLock);

                const nk_size bucket = HashPtr(ptr);
                Entry* prev = nullptr;
                Entry* current = mBuckets[bucket];

                // Parcours de la liste chaînée dans le bucket
                while (current) {
                    if (current->Info.userPtr == ptr) {
                        // Trouvé : mise à jour des stats et suppression

                        // Déduire la taille de l'allocation libérée
                        if (mTotalLiveBytes >= current->Info.size) {
                            mTotalLiveBytes -= current->Info.size;
                        } else {
                            mTotalLiveBytes = 0;  // Defensive
                        }

                        // Retrait de la liste chaînée
                        if (prev) {
                            prev->Next = current->Next;
                        } else {
                            mBuckets[bucket] = current->Next;  // Tête de liste
                        }

                        // Libération de la mémoire de l'entrée (via malloc/free pair)
                        ::free(current);

                        // Mise à jour du compteur d'allocations live
                        if (mLiveAllocations > 0) {
                            --mLiveAllocations;
                        }

                        #if defined(NKENTSEU_MEMORY_TRACKING_VERBOSE) && defined(NKENTSEU_DEBUG)
                            NK_FOUNDATION_LOG_DEBUG("[NKMemory] Unregistered: %p", ptr);
                        #endif

                        return;  // Succès : sortie immédiate
                    }
                    prev = current;
                    current = current->Next;
                }

                // Non trouvé : allocation non-trackée ou déjà unregister
                // Comportement : no-op silencieux (pas d'erreur)
                #if defined(NKENTSEU_MEMORY_TRACKING_VERBOSE) && defined(NKENTSEU_DEBUG)
                    NK_FOUNDATION_LOG_WARN("[NKMemory] Unregister called for untracked ptr: %p", ptr);
                #endif
            }

            const NkAllocationInfo* NkMemoryTracker::Find(void* ptr) const noexcept {
                if (!ptr) {
                    return nullptr;
                }

                NkSpinLockGuard guard(mLock);
                const Entry* entry = FindEntryConst(ptr);
                return entry ? &entry->Info : nullptr;
            }

            // ====================================================================
            // Reporting et Debugging
            // ====================================================================

            void NkMemoryTracker::DumpLeaks() const noexcept {
                NkSpinLockGuard guard(mLock);

                if (mLiveAllocations == 0) {
                    NK_FOUNDATION_LOG_INFO("[NKMemory] No memory leaks detected. ✓");
                    return;
                }

                NK_FOUNDATION_LOG_INFO("=== NKMemory Leak Report ===");
                NK_FOUNDATION_LOG_INFO("%-12s | %-10s | %-6s | %-30s | %s", 
                                      "Address", "Size", "Tag", "Location", "Name");
                NK_FOUNDATION_LOG_INFO("-------------|------------|--------|--------------------------------|----------------");

                nk_size totalLeaked = 0;

                // Parcours de tous les buckets
                for (nk_size bucket = 0; bucket < NK_TRACKER_BUCKET_COUNT; ++bucket) {
                    const Entry* node = mBuckets[bucket];
                    while (node) {
                        const NkAllocationInfo& info = node->Info;
                        
                        // Formatage de la taille en unités humaines
                        char sizeBuf[32];
                        if (info.size >= (1024 * 1024)) {
                            snprintf(sizeBuf, sizeof(sizeBuf), "%.2f MB", 
                                    static_cast<float>(info.size) / (1024.0f * 1024.0f));
                        } else if (info.size >= 1024) {
                            snprintf(sizeBuf, sizeof(sizeBuf), "%.2f KB", 
                                    static_cast<float>(info.size) / 1024.0f);
                        } else {
                            snprintf(sizeBuf, sizeof(sizeBuf), "%zu B", info.size);
                        }

                        // Formatage de l'emplacement
                        char locationBuf[64];
                        if (info.file && info.function) {
                            snprintf(locationBuf, sizeof(locationBuf), "%s:%d (%s)", 
                                    info.file, info.line, info.function);
                        } else if (info.file) {
                            snprintf(locationBuf, sizeof(locationBuf), "%s:%d", 
                                    info.file, info.line);
                        } else {
                            snprintf(locationBuf, sizeof(locationBuf), "<unknown>");
                        }

                        NK_FOUNDATION_LOG_INFO("%-12p | %-10s | %-6u | %-30s | %s",
                                              info.userPtr,
                                              sizeBuf,
                                              static_cast<unsigned>(info.tag),
                                              locationBuf,
                                              info.name ? info.name : "-");

                        totalLeaked += info.size;
                        node = node->Next;
                    }
                }

                NK_FOUNDATION_LOG_INFO("-------------|------------|--------|--------------------------------|----------------");
                NK_FOUNDATION_LOG_INFO("TOTAL        | %-10zu |        | %-30s | %zu leaks",
                                      totalLeaked, "", mLiveAllocations);
                NK_FOUNDATION_LOG_INFO("Peak memory  : %zu bytes", mPeakBytes);
                NK_FOUNDATION_LOG_INFO("Total allocs : %llu", static_cast<unsigned long long>(mTotalAllocations));
                NK_FOUNDATION_LOG_INFO("==============================");
            }

            // ====================================================================
            // Statistiques
            // ====================================================================

            NkMemoryTracker::Stats NkMemoryTracker::GetStats() const noexcept {
                NkSpinLockGuard guard(mLock);
                return {
                    mTotalLiveBytes,
                    mPeakBytes,
                    mLiveAllocations,
                    mTotalAllocations
                };
            }

            // ====================================================================
            // Gestion du cycle de vie
            // ====================================================================

            void NkMemoryTracker::Clear() noexcept {
                NkSpinLockGuard guard(mLock);
                ClearInternal();
            }

            // ====================================================================
            // Implémentations privées
            // ====================================================================

            nk_size NkMemoryTracker::HashPtr(const void* ptr) noexcept {
                // Mix des bits du pointeur pour distribution uniforme
                // Technique : XOR des bits décalés pour éviter les collisions sur adresses alignées
                const nk_size v = static_cast<nk_size>(reinterpret_cast<nk_uintptr>(ptr));
                
                // Mixing function simple mais efficace pour les pointeurs mémoire
                // (les adresses sont souvent alignées sur 8/16/32 bytes)
                nk_size h = v;
                h ^= (h >> 12u);
                h ^= (h >> 24u);
                h *= 0x9e3779b97f4a7c15ULL;  // Constante de Golden Ratio hash
                h ^= (h >> 16u);
                
                return h % NK_TRACKER_BUCKET_COUNT;
            }

            NkMemoryTracker::Entry* NkMemoryTracker::FindEntry(void* ptr) noexcept {
                Entry* node = mBuckets[HashPtr(ptr)];
                while (node) {
                    if (node->Info.userPtr == ptr) {
                        return node;
                    }
                    node = node->Next;
                }
                return nullptr;
            }

            const NkMemoryTracker::Entry* NkMemoryTracker::FindEntryConst(const void* ptr) const noexcept {
                const Entry* node = mBuckets[HashPtr(ptr)];
                while (node) {
                    if (node->Info.userPtr == ptr) {
                        return node;
                    }
                    node = node->Next;
                }
                return nullptr;
            }

            void NkMemoryTracker::ClearInternal() noexcept {
                // Libération de toutes les entrées dans tous les buckets
                for (nk_size i = 0; i < NK_TRACKER_BUCKET_COUNT; ++i) {
                    Entry* node = mBuckets[i];
                    while (node) {
                        Entry* next = node->Next;
                        ::free(node);  // Pair avec malloc dans Register
                        node = next;
                    }
                    mBuckets[i] = nullptr;
                }

                // Reset des compteurs statistiques
                mLiveAllocations = 0;
                mTotalLiveBytes = 0;
                // Note : mPeakBytes et mTotalAllocations NE sont PAS reset
                // pour préserver l'historique si Clear() est appelé pour dump intermédiaire
            }

            // ====================================================================
            // Fonction globale d'accès au singleton
            // ====================================================================

            NkMemoryTracker& GetGlobalMemoryTracker() noexcept {
                // Meyer's singleton : thread-safe en C++11+ (initialisation statique garantie)
                static NkMemoryTracker instance;
                return instance;
            }

        } // namespace memory
    } // namespace nkentseu

#else // NKENTSEU_DISABLE_MEMORY_TRACKING défini

    // -------------------------------------------------------------------------
    // VERSION NO-OP POUR RELEASE (zero overhead)
    // -------------------------------------------------------------------------
    namespace nkentseu {
        namespace memory {

            // Toutes les méthodes deviennent des no-ops inline ou retour constant
            // Le compilateur optimisera ces appels à néant en release

            NkMemoryTracker::NkMemoryTracker() noexcept 
                : mLiveAllocations(0), mTotalLiveBytes(0), mPeakBytes(0), mTotalAllocations(0) {
                for (nk_size i = 0; i < NK_TRACKER_BUCKET_COUNT; ++i) {
                    mBuckets[i] = nullptr;
                }
            }

            NkMemoryTracker::~NkMemoryTracker() = default;

            void NkMemoryTracker::Register(const NkAllocationInfo&) noexcept {}
            void NkMemoryTracker::Unregister(void*) noexcept {}
            const NkAllocationInfo* NkMemoryTracker::Find(void*) const noexcept { return nullptr; }
            void NkMemoryTracker::DumpLeaks() const noexcept {}
            
            NkMemoryTracker::Stats NkMemoryTracker::GetStats() const noexcept {
                return {0, 0, 0, 0};
            }
            
            void NkMemoryTracker::Clear() noexcept {}

            nk_size NkMemoryTracker::HashPtr(const void*) noexcept { return 0; }
            NkMemoryTracker::Entry* NkMemoryTracker::FindEntry(void*) noexcept { return nullptr; }
            const NkMemoryTracker::Entry* NkMemoryTracker::FindEntryConst(const void*) const noexcept { return nullptr; }
            void NkMemoryTracker::ClearInternal() noexcept {}

            NkMemoryTracker& GetGlobalMemoryTracker() noexcept {
                static NkMemoryTracker instance;
                return instance;
            }

        } // namespace memory
    } // namespace nkentseu

#endif // NKENTSEU_DISABLE_MEMORY_TRACKING

// =============================================================================
// NOTES DE MAINTENANCE
// =============================================================================
/*
    [THREAD-SAFETY]
    ✓ Toutes les méthodes publiques acquièrent NkSpinLock via NkSpinLockGuard
    ✓ NkSpinLock de NKCore est supposé être un spinlock léger, adapté aux sections courtes
    ✓ Les lectures de stats sont cohérentes grâce au lock (pas de tearing sur 64-bit)
    ✓ Find() const acquiert aussi le lock : nécessaire car la structure interne est mutable

    [PERFORMANCE]
    - HashPtr : ~5-10 cycles CPU (mixing simple, pas de multiplication lourde)
    - Register/Unregister : O(1) moyen + overhead du lock (~20-50 cycles selon contention)
    - Find : O(1) moyen + overhead du lock
    - DumpLeaks : O(n + bucket_count) : linéaire en nombre de fuites + 4096 buckets fixes
    - En release avec NKENTSEU_DISABLE_MEMORY_TRACKING : overhead = 0 (fonctions inline no-op)

    [MÉMOIRE]
    - Table de buckets : 4096 * sizeof(Entry*) = 4096 * 8 = 32 KB (sur 64-bit)
    - Par entrée trackée : sizeof(Entry) ≈ sizeof(NkAllocationInfo) + pointer ≈ 80-100 bytes
    - Pour 100k allocations trackées : ~8-10 MB overhead (acceptable pour debug, désactivable en release)

    [EXTENSIONS POSSIBLES]
    1. Filtrage par tag dans DumpLeaks :
       - Ajouter un paramètre optionnel NkMemoryTag filter = NkMemoryTag::NK_MEMORY_DEBUG
       - Skip les entrées dont info.tag != filter (sauf si filter == NK_MEMORY_DEBUG)
    
    2. Export vers formats externes :
       - Ajouter ExportToJson(const char* filepath) pour intégration avec outils externes
       - Format compatible avec Chrome Tracing ou Custom memory viewers
    
    3. Stack trace capture en debug :
       - Avec NKENTSEU_MEMORY_TRACKING_VERBOSE, capturer un callstack simplifié
       - Stocker dans NkAllocationInfo : std::array<void*, 16> callstack (debug only)
    
    4. Allocation pooling pour les Entry :
       - Remplacer ::malloc/::free par un pool d'entrées pré-allouées
       - Réduit la fragmentation du tracker lui-même

    [DEBUGGING TIPS]
    - Si beaucoup de collisions dans un bucket : vérifier la distribution des pointeurs
      (peut indiquer un problème d'alignement ou d'allocation en blocs)
    - Si mLiveAllocations != nombre réel d'entrées : bug dans Register/Unregister pair
    - Utiliser DumpLeaks() en fin de test unitaire pour valider l'absence de fuites
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================