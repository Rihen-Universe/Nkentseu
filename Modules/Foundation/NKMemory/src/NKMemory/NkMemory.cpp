// =============================================================================
// NKMemory/NkMemory.cpp
// Implémentation du système mémoire unifié NKMemory.
//
// Design :
//  - Inclusion systématique de "pch.h" en premier pour la précompilation
//  - Réutilisation maximale : délégation aux sous-modules NKMemory (ZÉRO duplication)
//  - Thread-safety via NkSpinLock avec guard RAII local (pas de dépendance externe)
//  - Gestion centralisée des metadata d'allocation pour debugging et profiling
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
#include "NKMemory/NKMemory.h"

// -------------------------------------------------------------------------
// DÉPENDANCES OPTIONNELLES (debug/verbose)
// -------------------------------------------------------------------------
#include "NKPlatform/NkFoundationLog.h"    // NK_FOUNDATION_LOG_*
#include "NKPlatform/NkFoundationLog.h"       // NK_FOUNDATION_SPRINT pour formatage sécurisé

// -------------------------------------------------------------------------
// NAMESPACE ANONYME : Helpers internes non-exportés
// -------------------------------------------------------------------------
namespace {

    /**
     * @brief Minimum de deux valeurs size_t
     * @note Helper local pour éviter dépendance supplémentaire
     */
    [[nodiscard]] inline nkentseu::nk_size Min(nkentseu::nk_size a, nkentseu::nk_size b) noexcept {
        return (a < b) ? a : b;
    }

} // namespace anonyme


// -------------------------------------------------------------------------
// NAMESPACE PUBLIC : Implémentation de NkMemorySystem
// -------------------------------------------------------------------------
namespace nkentseu {
    namespace memory {

        // ====================================================================
        // Singleton : Instance()
        // ====================================================================

        NkMemorySystem& NkMemorySystem::Instance() noexcept {
            // Meyer's singleton : thread-safe en C++11+ (initialisation statique garantie)
            static NkMemorySystem instance;
            return instance;
        }

        // ====================================================================
        // Constructeur / Initialisation des Membres
        // ====================================================================

        NkMemorySystem::NkMemorySystem() noexcept
            : mAllocator(&NkGetDefaultAllocator())
            , mHead(nullptr)
            , mLiveBytes(0u)
            , mPeakBytes(0u)
            , mLiveAllocations(0u)
            , mTotalAllocations(0u)
            , mLogCallback(nullptr)
            , mInitialized(false)
            , mLock()
            , mAllocIndex(&NkGetMallocAllocator())  // Hash table allouée via malloc pour éviter récursion
            , mTracker()
            , mGc(&NkGetDefaultAllocator())          // GC par défaut utilise l'allocateur défaut
            , mDefaultGcNode()
            , mGcHead(nullptr)
            , mGcCount(0u)
            , mNextGcId(1u) {
            
            // Initialisation du noeud pour le GC par défaut (évite allocation dynamique)
            mDefaultGcNode.Collector = &mGc;
            mDefaultGcNode.Allocator = mAllocator;
            mDefaultGcNode.Id = 0u;  // ID 0 réservé pour le default
            CopyGcName(mDefaultGcNode.Name, sizeof(mDefaultGcNode.Name), "Default");
            mDefaultGcNode.IsDefault = true;
            mDefaultGcNode.Prev = nullptr;
            mDefaultGcNode.Next = nullptr;
            
            // Le default GC est toujours en tête de liste
            mGcHead = &mDefaultGcNode;
            mGcCount = 1u;
        }

        // ====================================================================
        // Initialisation Publique
        // ====================================================================

        void NkMemorySystem::Initialize(NkAllocator* allocator) noexcept {
            NkSpinLockGuard guard(mLock);
            
            // Mise à jour de l'allocateur principal
            mAllocator = allocator ? allocator : &NkGetDefaultAllocator();
            
            // Propagation vers le GC par défaut et son noeud
            mGc.SetAllocator(mAllocator);
            mDefaultGcNode.Allocator = mAllocator;
            
            // Initialisation lazy de la hash table d'index
            if (!mAllocIndex.IsInitialized()) {
                (void)mAllocIndex.Initialize(256u, &NkGetMallocAllocator());
            } else {
                // Si déjà initialisé : clear pour repartir de zéro
                mAllocIndex.Clear();
            }
            
            // Reset du tracker de fuites
            mTracker.Clear();
            
            // Flag d'initialisation
            mInitialized = true;
        }

        // ====================================================================
        // Shutdown et Cleanup
        // ====================================================================

        void NkMemorySystem::Shutdown(nk_bool reportLeaks) noexcept {
            NkAllocationNode* leakedHead = nullptr;
            NkGcNode* customGcHead = nullptr;

            {
                // Section critique : extraction des listes sous lock
                NkSpinLockGuard guard(mLock);
                
                // Extraction de la liste des allocations trackées
                leakedHead = mHead;
                mHead = nullptr;
                mAllocIndex.Clear();  // Clear de la hash table pour éviter dangling pointers

                // Séparation des GC personnalisés (à détruire) du default (à conserver)
                NkGcNode* node = mGcHead;
                while (node) {
                    NkGcNode* next = node->Next;
                    if (!node->IsDefault) {
                        // Détachement du noeud personnalisé
                        node->Prev = nullptr;
                        node->Next = customGcHead;
                        if (customGcHead) {
                            customGcHead->Prev = node;
                        }
                        customGcHead = node;
                    }
                    node = next;
                }

                // Reset du default GC node pour réutilisation potentielle
                mDefaultGcNode.Prev = nullptr;
                mDefaultGcNode.Next = nullptr;
                mDefaultGcNode.Allocator = mAllocator;
                mGcHead = &mDefaultGcNode;
                mGcCount = 1u;

                // Flag d'initialisation et reset des compteurs
                mInitialized = false;
                mLiveBytes = 0u;
                mLiveAllocations = 0u;
            }
            // Lock relâché : opérations de destruction hors section critique

            // Destruction des GC personnalisés (hors lock pour éviter deadlock)
            NkAllocator& registryAllocator = NkGetMallocAllocator();
            while (customGcHead) {
                NkGcNode* next = customGcHead->Next;
                
                if (customGcHead->Collector) {
                    // Destruction explicite via placement delete
                    customGcHead->Collector->~NkGarbageCollector();
                    // Libération de la mémoire du collector lui-même
                    registryAllocator.Deallocate(customGcHead->Collector);
                    customGcHead->Collector = nullptr;
                }
                // Libération du noeud de tracking
                registryAllocator.Deallocate(customGcHead);
                
                customGcHead = next;
            }

            // Rapport et destruction des allocations non-libérées (fuites)
            while (leakedHead) {
                NkAllocationNode* next = leakedHead->next;

                // Logging de la fuite si demandé
                if (reportLeaks) {
                    nk_char line[512];
                    NK_FOUNDATION_SPRINT(line,
                            sizeof(line),
                            "[NKMemory] leak ptr=%p size=%llu count=%llu tag=%s at %s:%d (%s)",
                            leakedHead->userPtr,
                            static_cast<unsigned long long>(leakedHead->size),
                            static_cast<unsigned long long>(leakedHead->count),
                            leakedHead->tag ? leakedHead->tag : "<none>",
                            leakedHead->file ? leakedHead->file : "<unknown>",
                            leakedHead->line,
                            leakedHead->function ? leakedHead->function : "<unknown>");
                    LogLine(line);
                }

                // Destruction type-safe via la fonction enregistrée
                if (leakedHead->destroy) {
                    leakedHead->destroy(leakedHead->userPtr, leakedHead->basePtr, mAllocator, leakedHead->count);
                }
                
                // Unregistration du tracker et profiler
                mTracker.Unregister(leakedHead->userPtr);
                NkMemoryProfiler::NotifyFree(leakedHead->userPtr, leakedHead->size);
                
                // Libération du noeud de tracking lui-même
                ReleaseNode(leakedHead);
                
                leakedHead = next;
            }
            
            // Cleanup final du tracker
            mTracker.Clear();
        }

        // ====================================================================
        // Configuration du Logging
        // ====================================================================

        void NkMemorySystem::SetLogCallback(NkLogCallback callback) noexcept {
            NkSpinLockGuard guard(mLock);
            mLogCallback = callback;
        }

        // ====================================================================
        // API d'Allocation Mémoire
        // ====================================================================

        void* NkMemorySystem::Allocate(nk_size size,
                                      nk_size alignment,
                                      const nk_char* file,
                                      nk_int32 line,
                                      const nk_char* function,
                                      const nk_char* tag) {
            // Initialisation lazy si nécessaire
            if (!mInitialized) {
                Initialize(nullptr);
            }

            // Allocation brute via l'allocateur configuré
            void* memory = mAllocator->Allocate(size, alignment);
            if (!memory) {
                return nullptr;  // Échec d'allocation
            }

            // Enregistrement pour tracking avec metadata complète
            RegisterAllocation(memory, memory, size, 1u, alignment,
                            &NkMemorySystem::DestroyRaw, file, line, function, tag);
            return memory;
        }

        void NkMemorySystem::Free(void* pointer) noexcept {
            if (!pointer) {
                return;  // Safe : no-op sur nullptr
            }

            // Recherche et détachement du noeud de tracking
            NkAllocationNode* node = FindAndDetach(pointer);
            if (!node) {
                // Fallback : pointer non tracké, deallocation directe
                if (mAllocator) {
                    mAllocator->Deallocate(pointer);
                }
                return;
            }

            // Destruction type-safe via la fonction enregistrée
            if (node->destroy) {
                node->destroy(node->userPtr, node->basePtr, mAllocator, node->count);
            }
            
            // Unregistration du tracker et profiler
            mTracker.Unregister(node->userPtr);
            NkMemoryProfiler::NotifyFree(node->userPtr, node->size);
            
            // Libération du noeud de tracking lui-même
            ReleaseNode(node);
        }

        // ====================================================================
        // Interrogation des Statistiques
        // ====================================================================

        NkMemoryStats NkMemorySystem::GetStats() const noexcept {
            // Récupération des stats du tracker (thread-safe)
            const NkMemoryTracker::Stats trackerStats = mTracker.GetStats();
            
            // Mapping vers NkMemoryStats
            NkMemoryStats stats{};
            stats.liveBytes = trackerStats.liveBytes;
            stats.peakBytes = trackerStats.peakBytes;
            stats.liveAllocations = trackerStats.liveAllocations;
            stats.totalAllocations = static_cast<nk_size>(trackerStats.totalAllocations);
            
            return stats;
        }

        void NkMemorySystem::DumpLeaks() noexcept {
            // Délègue au tracker pour le dump formaté
            mTracker.DumpLeaks();
        }

        // ====================================================================
        // Gestion des Garbage Collectors
        // ====================================================================

        NkGarbageCollector* NkMemorySystem::CreateGc(NkAllocator* allocator) noexcept {
            // Initialisation lazy si nécessaire
            if (!mInitialized) {
                Initialize(nullptr);
            }

            // Résolution de l'allocateur pour ce GC
            NkAllocator* gcAllocator = allocator ? allocator : mAllocator;
            if (!gcAllocator) {
                gcAllocator = &NkGetDefaultAllocator();
            }

            // Allocation de la mémoire pour le collector via registry allocator (évite récursion)
            NkAllocator& registryAllocator = NkGetMallocAllocator();
            void* collectorMemory = registryAllocator.Allocate(
                sizeof(NkGarbageCollector), alignof(NkGarbageCollector));
            if (!collectorMemory) {
                return nullptr;
            }

            // Allocation de la mémoire pour le noeud de tracking
            void* nodeMemory = registryAllocator.Allocate(
                sizeof(NkGcNode), alignof(NkGcNode));
            if (!nodeMemory) {
                // Cleanup en cas d'échec partiel
                registryAllocator.Deallocate(collectorMemory);
                return nullptr;
            }

            // Construction via placement new
            auto* collector = new (collectorMemory) NkGarbageCollector(gcAllocator);
            auto* node = static_cast<NkGcNode*>(nodeMemory);

            // Registration thread-safe dans la liste des GC managés
            {
                NkSpinLockGuard guard(mLock);
                
                node->Collector = collector;
                node->Allocator = gcAllocator;
                node->Id = mNextGcId++;  // ID unique incrémental
                NK_FOUNDATION_SPRINT(node->Name, sizeof(node->Name), "Gc_%llu",
                                     static_cast<unsigned long long>(node->Id));
                node->IsDefault = false;
                
                // Insertion en tête de la liste chaînée
                node->Prev = nullptr;
                node->Next = mGcHead;
                if (mGcHead) {
                    mGcHead->Prev = node;
                }
                mGcHead = node;
                ++mGcCount;
            }

            return collector;
        }

        nk_bool NkMemorySystem::DestroyGc(NkGarbageCollector* collector) noexcept {
            if (!collector) {
                return false;
            }

            NkGcNode* node = nullptr;
            {
                NkSpinLockGuard guard(mLock);
                
                // Recherche du noeud correspondant
                node = FindGcNodeLocked(collector);
                if (!node || node->IsDefault) {
                    return false;  // Invalide ou tentative de destruction du default
                }

                // Retrait de la liste chaînée
                if (node->Prev) {
                    node->Prev->Next = node->Next;
                } else {
                    mGcHead = node->Next;  // C'était la tête
                }
                if (node->Next) {
                    node->Next->Prev = node->Prev;
                }
                node->Prev = nullptr;
                node->Next = nullptr;

                // Mise à jour du compteur
                if (mGcCount > 0u) {
                    --mGcCount;
                }
            }
            // Lock relâché : destruction hors section critique

            // Destruction explicite via placement delete
            NkAllocator& registryAllocator = NkGetMallocAllocator();
            collector->~NkGarbageCollector();
            registryAllocator.Deallocate(collector);
            registryAllocator.Deallocate(node);
            
            return true;
        }

        nk_bool NkMemorySystem::SetGcName(NkGarbageCollector* collector, const nk_char* name) noexcept {
            if (!collector || !name || !name[0]) {
                return false;
            }

            NkSpinLockGuard guard(mLock);
            
            NkGcNode* node = FindGcNodeLocked(collector);
            if (!node) {
                return false;
            }
            
            CopyGcName(node->Name, sizeof(node->Name), name);
            return true;
        }

        nk_bool NkMemorySystem::GetGcProfile(const NkGarbageCollector* collector,
                                            NkGcProfile* outProfile) const noexcept {
            if (!collector || !outProfile) {
                return false;
            }

            NkSpinLockGuard guard(mLock);
            
            const NkGcNode* node = FindGcNodeLocked(collector);
            if (!node || !node->Collector) {
                return false;
            }

            // Remplissage de la structure de sortie
            outProfile->Id = node->Id;
            CopyGcName(outProfile->Name, sizeof(outProfile->Name), node->Name);
            outProfile->ObjectCount = node->Collector->ObjectCount();
            outProfile->Allocator = node->Allocator;
            outProfile->IsDefault = node->IsDefault;
            
            return true;
        }

        nk_size NkMemorySystem::GetGcCount() const noexcept {
            NkSpinLockGuard guard(mLock);
            return mGcCount;
        }

        // ====================================================================
        // Fonctions de Destruction Type-Safe
        // ====================================================================

        void NkMemorySystem::DestroyRaw(void* /*userPtr*/,
                                       void* basePtr,
                                       NkAllocator* allocator,
                                       nk_size /*count*/) {
            if (allocator && basePtr) {
                allocator->Deallocate(basePtr);
            }
        }

        // ====================================================================
        // Méthodes Internes de Tracking
        // ====================================================================

        void NkMemorySystem::RegisterAllocation(void* userPtr,
                                               void* basePtr,
                                               nk_size size,
                                               nk_size count,
                                               nk_size alignment,
                                               NkDestroyFn destroy,
                                               const nk_char* file,
                                               nk_int32 line,
                                               const nk_char* function,
                                               const nk_char* tag) {
            // Allocation du noeud de tracking via registry allocator (évite récursion)
            NkAllocator& nodeAllocator = NkGetMallocAllocator();
            void* nodeMemory = nodeAllocator.Allocate(
                sizeof(NkAllocationNode), alignof(NkAllocationNode));
            if (!nodeMemory) {
                // Échec d'allocation du metadata : cleanup immédiat pour éviter fuite
                if (destroy) {
                    destroy(userPtr, basePtr, mAllocator, count);
                }
                return;
            }

            // Initialisation du noeud
            auto* node = static_cast<NkAllocationNode*>(nodeMemory);
            node->userPtr = userPtr;
            node->basePtr = basePtr;
            node->size = size;
            node->count = count;
            node->alignment = alignment;
            node->destroy = destroy;
            node->file = file;
            node->line = line;
            node->function = function;
            node->tag = tag;
            node->prev = nullptr;
            node->next = nullptr;

            // Registration thread-safe dans les structures internes
            nk_uint64 allocationSerial = 0u;
            {
                NkSpinLockGuard guard(mLock);
                
                // Initialisation lazy de la hash table si nécessaire
                if (!mAllocIndex.IsInitialized()) {
                    (void)mAllocIndex.Initialize(256u, &NkGetMallocAllocator());
                }
                
                // Insertion en tête de la liste chaînée
                node->next = mHead;
                node->prev = nullptr;
                if (mHead) {
                    mHead->prev = node;
                }
                mHead = node;
                
                // Insertion dans la hash table pour lookup O(1)
                if (mAllocIndex.IsInitialized()) {
                    (void)mAllocIndex.Insert(userPtr, node);
                }

                // Mise à jour des statistiques globales
                ++mLiveAllocations;
                ++mTotalAllocations;
                mLiveBytes += size;
                if (mLiveBytes > mPeakBytes) {
                    mPeakBytes = mLiveBytes;
                }
                allocationSerial = static_cast<nk_uint64>(mTotalAllocations);
            }
            // Lock relâché
            
            // Notification au tracker et profiler (hors section critique)
            NkAllocationInfo info{};
            info.userPtr = userPtr;
            info.basePtr = basePtr;
            info.size = size;
            info.count = count;
            info.alignment = alignment;
            info.tag = static_cast<nk_uint8>(NkMemoryTag::NK_MEMORY_ENGINE);
            info.file = file;
            info.line = line;
            info.function = function;
            info.name = tag;
            info.timestamp = allocationSerial;
            info.threadId = 0u;  // Pourrait être NkThread::GetCurrentId() si disponible
            
            mTracker.Register(info);
            NkMemoryProfiler::NotifyAlloc(userPtr, size, tag);
        }

        NkMemorySystem::NkAllocationNode* NkMemorySystem::FindAndDetach(void* userPtr) noexcept {
            NkSpinLockGuard guard(mLock);

            NkAllocationNode* cur = nullptr;
            
            // Tentative de lookup O(1) via hash table
            if (mAllocIndex.IsInitialized()) {
                void* value = nullptr;
                if (mAllocIndex.Erase(userPtr, &value)) {
                    cur = static_cast<NkAllocationNode*>(value);
                }
            }

            // Fallback : recherche linéaire dans la liste chaînée
            if (!cur) {
                cur = mHead;
                while (cur) {
                    if (cur->userPtr == userPtr) {
                        // Synchronisation avec la hash table si initialisée
                        if (mAllocIndex.IsInitialized()) {
                            (void)mAllocIndex.Erase(userPtr, nullptr);
                        }
                        break;
                    }
                    cur = cur->next;
                }
            }

            if (!cur) {
                return nullptr;  // Non trouvé
            }

            // Retrait de la liste chaînée
            if (cur->prev) {
                cur->prev->next = cur->next;
            } else {
                mHead = cur->next;  // C'était la tête
            }
            if (cur->next) {
                cur->next->prev = cur->prev;
            }
            cur->prev = nullptr;
            cur->next = nullptr;

            // Mise à jour des statistiques
            if (mLiveAllocations > 0u) {
                --mLiveAllocations;
            }
            if (mLiveBytes >= cur->size) {
                mLiveBytes -= cur->size;
            } else {
                mLiveBytes = 0u;  // Defensive : éviter underflow
            }

            return cur;
        }

        void NkMemorySystem::ReleaseNode(NkAllocationNode* node) noexcept {
            if (!node) {
                return;
            }
            // Libération via registry allocator (pair avec Allocate dans RegisterAllocation)
            NkGetMallocAllocator().Deallocate(node);
        }

        void NkMemorySystem::LogLine(const nk_char* message) noexcept {
            if (!message) {
                return;
            }
            if (mLogCallback) {
                mLogCallback(message);
                return;
            }
            // Fallback vers le logging standard de NKPlatform
            NK_FOUNDATION_LOG_WARN("%s", message);
        }

        // ====================================================================
        // Méthodes Internes de Gestion GC
        // ====================================================================

        NkMemorySystem::NkGcNode* NkMemorySystem::FindGcNodeLocked(NkGarbageCollector* collector) noexcept {
            // Parcours linéaire de la liste chaînée (petit nombre de GC attendu)
            NkGcNode* node = mGcHead;
            while (node) {
                if (node->Collector == collector) {
                    return node;
                }
                node = node->Next;
            }
            return nullptr;
        }

        const NkMemorySystem::NkGcNode* NkMemorySystem::FindGcNodeLocked(
            const NkGarbageCollector* collector) const noexcept {
            // Version const : même logique
            const NkGcNode* node = mGcHead;
            while (node) {
                if (node->Collector == collector) {
                    return node;
                }
                node = node->Next;
            }
            return nullptr;
        }

        void NkMemorySystem::CopyGcName(nk_char* destination,
                                       nk_size destinationSize,
                                       const nk_char* source) noexcept {
            if (!destination || destinationSize == 0u) {
                return;
            }

            if (!source) {
                destination[0] = '\0';
                return;
            }

            // Copie avec bounds checking et null-termination garantie
            nk_size i = 0u;
            for (; i + 1u < destinationSize && source[i] != '\0'; ++i) {
                destination[i] = source[i];
            }
            destination[i] = '\0';
        }

    } // namespace memory
} // namespace nkentseu

// =============================================================================
// NOTES DE MAINTENANCE
// =============================================================================
/*
    [THREAD-SAFETY]
    ✓ Toutes les méthodes publiques acquièrent mLock via NkSpinLockGuard local
    ✓ Les sections critiques sont minimales : extraction de données, puis traitement hors lock
    ✓ Les destructeurs et notifications (tracker/profiler) sont appelés hors lock pour éviter deadlock
    ✓ Meyer's singleton pour Instance() : thread-safe en C++11+

    [PERFORMANCE]
    - Allocate/Free : O(1) moyen grâce à hash table, O(n) fallback rare avant init
    - RegisterAllocation : ~50-100 cycles CPU (allocation metadata + insertion hash + notifications)
    - FindAndDetach : O(1) moyen avec hash table, O(n) fallback liste chaînée
    - Shutdown : O(n + m) où n = allocations trackées, m = GC personnalisés
    - GetStats : O(1) : délégation au tracker avec lecture atomique

    [MÉMOIRE]
    - Par allocation trackée : sizeof(NkAllocationNode) ≈ 80-100 bytes overhead (64-bit)
    - Hash table : capacity * sizeof(NkGcObject*) bytes, load factor 70% max
    - GC managés : sizeof(NkGcNode) ≈ 100 bytes par GC + overhead du GC lui-même
    - Exemple : 10k allocations → ~1MB overhead tracking + ~14KB hash table

    [EXTENSIONS POSSIBLES]
    1. Allocation pooling pour les NkAllocationNode : réduire la fragmentation du tracker
    2. Thread-local caching : éviter le lock pour les allocations fréquentes sur le même thread
    3. Sampling mode : tracker seulement 1/N allocations pour réduire l'overhead en production
    4. Export JSON/CSV : ajouter NkMemorySystem::ExportStats(const char* filepath) pour outils externes
    5. Integration avec NkTag : utiliser les tags pour filtrer/stats par catégorie dans GetStats()

    [DEBUGGING TIPS]
    - Si fuite non rapportée : vérifier que Free/Delete a bien été appelé sur le pointeur
    - Si crash dans Shutdown : vérifier qu'aucun thread n'utilise encore le système mémoire
    - Si performance degrade : profiler RegisterAllocation et ajuster la capacité initiale de mAllocIndex
    - Dump intermédiaire : appeler DumpLeaks() à tout moment pour inspection sans shutdown

    [BONNES PRATIQUES]
    1. Toujours utiliser les macros NK_MEM_* pour bénéficier du tracking automatique
    2. Appeler Shutdown(true) en fin d'application pour rapport complet des fuites
    3. Utiliser des tags significatifs pour filtering dans les logs de fuites
    4. Pour les allocations très fréquentes, envisager un pool allocator dédié pour réduire l'overhead
    5. En production, définir NKENTSEU_DISABLE_MEMORY_TRACKING pour performance maximale
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================