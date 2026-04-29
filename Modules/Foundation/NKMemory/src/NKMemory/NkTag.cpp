// =============================================================================
// NKMemory/NkTag.cpp
// Implémentation du système de tagging et budgétisation mémoire.
//
// Design :
//  - Inclusion systématique de "pch.h" en premier pour la précompilation
//  - Thread-safety via NkAtomicInt64 de NKCore (ZÉRO duplication de logique atomique)
//  - Overhead minimal : atomiques lock-free quand supportés par la plateforme
//  - Debug-only extensions conditionnelles via NKENTSEU_MEMORY_TAGGING_VERBOSE
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
#include "NKMemory/NkTag.h"

// -------------------------------------------------------------------------
// DÉPENDANCES OPTIONNELLES (debug/verbose)
// -------------------------------------------------------------------------
#include "NKPlatform/NkFoundationLog.h"    // NK_FOUNDATION_LOG_*
#include "NKCore/NkAtomic.h"               // NkAtomicUInt64 pour compteurs thread-safe
#include "NKCore/NkLimits.h"
#include "NKCore/Assert/NkAssert.h"    // NKENTSEU_ASSERT_MSG

// -------------------------------------------------------------------------
// CONFIGURATION CONDITIONNELLE
// -------------------------------------------------------------------------
// En release avec NKENTSEU_DISABLE_MEMORY_TAGGING, toutes les fonctions
// deviennent des no-ops compilés en inline pour zero overhead.

#if !defined(NKENTSEU_DISABLE_MEMORY_TAGGING)

    // -------------------------------------------------------------------------
    // NAMESPACE ANONYME : Structures internes thread-safe
    // -------------------------------------------------------------------------
    namespace {

        /**
         * @brief État thread-safe pour un tag mémoire unique
         * @note Utilise des atomiques pour éviter les locks globaux
         */
        struct TagState {
            nkentseu::NkAtomicUInt64 current;      ///< Octets actuellement alloués
            nkentseu::NkAtomicUInt64 peak;         ///< Pic historique d'allocation
            nkentseu::NkAtomicUInt64 allocCount;   ///< Nombre total d'allocations
            nkentseu::nk_uint64      budget;       ///< Limite configurée (non-atomique, mise à jour rare)
            
            TagState() noexcept : current(0), peak(0), allocCount(0), budget(0) {}
        };

        // Tableau global d'états : indexé par NkMemoryTag (nk_uint8 => 256 entrées max)
        // Initialisation statique : tous les TagState sont zero-initialized
        TagState gTagStates[256];

        // Configuration globale (mise à jour rare => pas besoin d'atomiques)
        struct GlobalConfig {
            nkentseu::nk_bool  alertsEnabled;          ///< Activer les warnings de dépassement
            nkentseu::float32  warningThreshold;       ///< Seuil d'alerte préventive [0.0 - 1.0]
            
            constexpr GlobalConfig() noexcept 
                : alertsEnabled(true), warningThreshold(0.9f) {}
        };
        
        GlobalConfig gConfig;

        // Helper : formatage de taille humaine pour les logs
        const char* FormatSize(nkentseu::nk_uint64 bytes, char* buffer, nkentseu::nk_size bufferSize) {
            if (bytes >= (1024ULL * 1024 * 1024)) {
                snprintf(buffer, bufferSize, "%.1f GB", static_cast<float>(bytes) / (1024.0f * 1024.0f * 1024.0f));
            } else if (bytes >= (1024ULL * 1024)) {
                snprintf(buffer, bufferSize, "%.1f MB", static_cast<float>(bytes) / (1024.0f * 1024.0f));
            } else if (bytes >= 1024) {
                snprintf(buffer, bufferSize, "%.1f KB", static_cast<float>(bytes) / 1024.0f);
            } else {
                snprintf(buffer, bufferSize, "%llu B", static_cast<unsigned long long>(bytes));
            }
            return buffer;
        }

        // Helper : nom lisible pour un tag (pour les logs)
        const char* TagName(nkentseu::memory::NkMemoryTag tag) {
            using namespace nkentseu::memory;
            switch (tag) {
                case NkMemoryTag::NK_MEMORY_ENGINE:    return "ENGINE";
                case NkMemoryTag::NK_MEMORY_CONTAINER: return "CONTAINER";
                case NkMemoryTag::NK_MEMORY_ALLOCATOR: return "ALLOCATOR";
                case NkMemoryTag::NK_MEMORY_GAME:      return "GAME";
                case NkMemoryTag::NK_MEMORY_ENTITY:    return "ENTITY";
                case NkMemoryTag::NK_MEMORY_SCRIPT:    return "SCRIPT";
                case NkMemoryTag::NK_MEMORY_RENDER:    return "RENDER";
                case NkMemoryTag::NK_MEMORY_TEXTURE:   return "TEXTURE";
                case NkMemoryTag::NK_MEMORY_MESH:      return "MESH";
                case NkMemoryTag::NK_MEMORY_SHADER:    return "SHADER";
                case NkMemoryTag::NK_MEMORY_PHYSICS:   return "PHYSICS";
                case NkMemoryTag::NK_MEMORY_COLLISION: return "COLLISION";
                case NkMemoryTag::NK_MEMORY_RIGID:     return "RIGID";
                case NkMemoryTag::NK_MEMORY_AUDIO:     return "AUDIO";
                case NkMemoryTag::NK_MEMORY_SOUND:     return "SOUND";
                case NkMemoryTag::NK_MEMORY_STREAMING: return "STREAMING";
                case NkMemoryTag::NK_MEMORY_NETWORK:   return "NETWORK";
                case NkMemoryTag::NK_MEMORY_DEBUG:     return "DEBUG";
                default:                               return "UNKNOWN";
            }
        }

    } // namespace anonyme


    // -------------------------------------------------------------------------
    // NAMESPACE PUBLIC : Implémentations de NkMemoryBudget
    // -------------------------------------------------------------------------
    namespace nkentseu {
        namespace memory {

            void NkMemoryBudget::SetBudget(NkMemoryTag tag, nk_uint64 bytes) noexcept {
                const auto idx = static_cast<nk_uint8>(tag);
                gTagStates[idx].budget = bytes;  // Write non-atomique : mise à jour rare, acceptable
            }

            nk_uint64 NkMemoryBudget::GetBudget(NkMemoryTag tag) noexcept {
                const auto idx = static_cast<nk_uint8>(tag);
                return gTagStates[idx].budget;
            }

            void NkMemoryBudget::OnAllocate(NkMemoryTag tag, nk_uint64 bytes) noexcept {
                if (bytes == 0) {
                    return;  // No-op pour allocations vides
                }

                const auto idx = static_cast<nk_uint8>(tag);
                TagState& state = gTagStates[idx];

                // Mise à jour atomique des compteurs
                state.current.FetchAdd(bytes);
                state.allocCount.FetchAdd(1);

                // Mise à jour du peak : lecture-compare-écriture atomique
                nk_uint64 current = state.current.Load();
                nk_uint64 oldPeak = state.peak.Load();
                while (current > oldPeak) {
                    if (state.peak.CompareExchangeWeak(oldPeak, current)) {
                        break;  // Succès : peak mis à jour
                    }
                    // Échec : oldPeak contient la nouvelle valeur, retry si nécessaire
                    if (current <= oldPeak) {
                        break;  // Plus besoin de mettre à jour
                    }
                }

                // Vérification de budget et alertes (seulement si configuré)
                #if defined(NKENTSEU_MEMORY_TAGGING_VERBOSE) || !defined(NKENTSEU_PLATFORM_RELEASE)
                    if (gConfig.alertsEnabled && state.budget > 0) {
                        const float usage = static_cast<float>(current) / static_cast<float>(state.budget);
                        
                        // Alerte préventive si seuil atteint
                        if (usage >= gConfig.warningThreshold && usage < 1.0f) {
                            NK_FOUNDATION_LOG_WARN("[NKMemory] Tag %s at %.1f%% of budget (%llu/%llu bytes)",
                                                  TagName(tag), usage * 100.0f, 
                                                  static_cast<unsigned long long>(current),
                                                  static_cast<unsigned long long>(state.budget));
                        }
                        
                        // Alerte critique si dépassement
                        if (usage >= 1.0f) {
                            NK_FOUNDATION_LOG_ERROR("[NKMemory] Tag %s OVER BUDGET: %.1f%% (%llu/%llu bytes)",
                                                   TagName(tag), usage * 100.0f,
                                                   static_cast<unsigned long long>(current),
                                                   static_cast<unsigned long long>(state.budget));
                            #if defined(NKENTSEU_DEBUG) && !defined(NKENTSEU_DISABLE_MEMORY_ASSERTS)
                                // En debug : assertion pour catcher les dépassements tôt
                                NKENTSEU_ASSERT_MSG(false, "Memory budget exceeded for tag");
                            #endif
                        }
                    }
                #endif
            }

            void NkMemoryBudget::OnDeallocate(NkMemoryTag tag, nk_uint64 bytes) noexcept {
                if (bytes == 0) {
                    return;
                }
                const auto idx = static_cast<nk_uint8>(tag);
                // Soustraction atomique : le compteur ne doit pas devenir négatif
                // (en pratique, OnDeallocate doit correspondre à une OnAllocate précédente)
                gTagStates[idx].current.FetchSub(bytes);
            }

            nk_int64 NkMemoryBudget::GetBudgetRemaining(NkMemoryTag tag) noexcept {
                const auto idx = static_cast<nk_uint8>(tag);
                const TagState& state = gTagStates[idx];
                
                // Budget illimité : retourner une grande valeur
                if (state.budget == 0) {
                    return INT64_MAX;
                }
                
                // Calcul : budget - current (peut être négatif si over budget)
                const nk_int64 current = static_cast<nk_int64>(state.current.Load());
                const nk_int64 budget  = static_cast<nk_int64>(state.budget);
                return budget - current;
            }

            nk_bool NkMemoryBudget::IsOverBudget(NkMemoryTag tag) noexcept {
                const auto idx = static_cast<nk_uint8>(tag);
                const TagState& state = gTagStates[idx];
                
                // Pas de budget = jamais over
                if (state.budget == 0) {
                    return false;
                }
                
                return state.current.Load() > state.budget;
            }

            NkMemoryTagStats NkMemoryBudget::GetStats(NkMemoryTag tag) noexcept {
                const auto idx = static_cast<nk_uint8>(tag);
                const TagState& state = gTagStates[idx];
                
                NkMemoryTagStats stats;
                stats.totalAllocated  = state.current.Load();
                stats.peakAllocated   = state.peak.Load();
                stats.allocationCount = state.allocCount.Load();
                
                // Calcul de la moyenne (évite division par zéro)
                if (stats.allocationCount > 0) {
                    stats.averageSize = static_cast<float32>(stats.totalAllocated) / 
                                       static_cast<float32>(stats.allocationCount);
                }
                
                // Fragmentation : estimation simplifiée (0 par défaut)
                // Une implémentation complète nécessiterait le tracking des blocs libres
                stats.fragmentation = 0.0f;
                
                return stats;
            }

            void NkMemoryBudget::DumpStats() noexcept {
                NK_FOUNDATION_LOG_INFO("=== NKMemory Tag Budget Report ===");
                NK_FOUNDATION_LOG_INFO("%-14s | %-10s | %-10s | %-6s | %-10s",
                                      "Tag", "Used", "Budget", "%", "Peak");
                NK_FOUNDATION_LOG_INFO("---------------|------------|------------|--------|------------");

                nk_uint64 totalUsed = 0;
                nk_uint64 totalBudget = 0;
                nk_uint64 totalPeak = 0;

                // Liste des tags à afficher (on skip les reserved et unknown)
                constexpr NkMemoryTag displayedTags[] = {
                    NkMemoryTag::NK_MEMORY_ENGINE,
                    NkMemoryTag::NK_MEMORY_CONTAINER,
                    NkMemoryTag::NK_MEMORY_ALLOCATOR,
                    NkMemoryTag::NK_MEMORY_GAME,
                    NkMemoryTag::NK_MEMORY_ENTITY,
                    NkMemoryTag::NK_MEMORY_SCRIPT,
                    NkMemoryTag::NK_MEMORY_RENDER,
                    NkMemoryTag::NK_MEMORY_TEXTURE,
                    NkMemoryTag::NK_MEMORY_MESH,
                    NkMemoryTag::NK_MEMORY_SHADER,
                    NkMemoryTag::NK_MEMORY_PHYSICS,
                    NkMemoryTag::NK_MEMORY_COLLISION,
                    NkMemoryTag::NK_MEMORY_RIGID,
                    NkMemoryTag::NK_MEMORY_AUDIO,
                    NkMemoryTag::NK_MEMORY_SOUND,
                    NkMemoryTag::NK_MEMORY_STREAMING,
                    NkMemoryTag::NK_MEMORY_NETWORK,
                    NkMemoryTag::NK_MEMORY_DEBUG,
                };

                char usedBuf[32], budgetBuf[32], peakBuf[32];

                for (NkMemoryTag tag : displayedTags) {
                    const auto idx = static_cast<nk_uint8>(tag);
                    const TagState& state = gTagStates[idx];
                    
                    const nk_uint64 used = state.current.Load();
                    const nk_uint64 budget = state.budget;
                    const nk_uint64 peak = state.peak.Load();
                    
                    totalUsed += used;
                    totalBudget += (budget > 0 ? budget : 0);
                    totalPeak += peak;

                    // Calcul du pourcentage
                    const char* percentStr = "";
                    char percentBuf[16] = {0};
                    if (budget > 0) {
                        const float pct = (static_cast<float>(used) / static_cast<float>(budget)) * 100.0f;
                        snprintf(percentBuf, sizeof(percentBuf), "%.1f%%", pct);
                        percentStr = percentBuf;
                        
                        // Marker visuel si over budget ou proche du seuil
                        if (pct >= 100.0f) {
                            percentStr = "OVER!";
                        } else if (pct >= gConfig.warningThreshold * 100.0f) {
                            percentStr = "WARN!";
                        }
                    }

                    NK_FOUNDATION_LOG_INFO("%-14s | %-10s | %-10s | %-6s | %-10s",
                                          TagName(tag),
                                          FormatSize(used, usedBuf, sizeof(usedBuf)),
                                          (budget > 0) ? FormatSize(budget, budgetBuf, sizeof(budgetBuf)) : "unlimited",
                                          percentStr,
                                          FormatSize(peak, peakBuf, sizeof(peakBuf)));
                }

                NK_FOUNDATION_LOG_INFO("---------------|------------|------------|--------|------------");
                NK_FOUNDATION_LOG_INFO("%-14s | %-10s | %-10s | %-6s | %-10s",
                                      "TOTAL",
                                      FormatSize(totalUsed, usedBuf, sizeof(usedBuf)),
                                      FormatSize(totalBudget, budgetBuf, sizeof(budgetBuf)),
                                      (totalBudget > 0) ? 
                                          [&]() { 
                                              static char buf[16]; 
                                              snprintf(buf, sizeof(buf), "%.1f%%", 
                                                      static_cast<float>(totalUsed) / static_cast<float>(totalBudget) * 100.0f);
                                              return buf; 
                                          }() : "N/A",
                                      FormatSize(totalPeak, peakBuf, sizeof(peakBuf)));
                NK_FOUNDATION_LOG_INFO("=====================================");
            }

            void NkMemoryBudget::ResetStats() noexcept {
                for (auto& state : gTagStates) {
                    state.current.Store(0);
                    state.peak.Store(0);
                    state.allocCount.Store(0);
                    // budget n'est PAS reset : configuration persistante
                }
            }

            void NkMemoryBudget::SetBudgetAlertsEnabled(nk_bool enabled) noexcept {
                gConfig.alertsEnabled = enabled;
            }

            void NkMemoryBudget::SetBudgetWarningThreshold(float32 threshold) noexcept {
                // Clamp à [0.0, 1.0] pour sécurité
                gConfig.warningThreshold = (threshold < 0.0f) ? 0.0f : 
                                          (threshold > 1.0f) ? 1.0f : threshold;
            }

            // -------------------------------------------------------------------------
            // Implémentations des helpers TaggedAlloc / TaggedFree
            // -------------------------------------------------------------------------

            void* TaggedAlloc(nk_size bytes, NkMemoryTag tag, 
                             NkAllocator* allocator, nk_size alignment) {
                if (bytes == 0) {
                    return nullptr;
                }
                
                NkAllocator& alloc = allocator ? *allocator : NkGetDefaultAllocator();
                void* ptr = alloc.Allocate(bytes, alignment);
                
                if (ptr) {
                    // Notification thread-safe au système de tagging
                    NkMemoryBudget::OnAllocate(tag, bytes);
                }
                
                return ptr;
            }

            void TaggedFree(void* ptr, NkMemoryTag tag, NkAllocator* allocator) {
                if (!ptr) {
                    return;  // Safe : no-op sur nullptr
                }
                
                // Note : idéalement, on trackerait la taille par ptr pour éviter 
                // de devoir la repasser. Pour l'instant, on suppose que l'appelant
                // connaît la taille ou que l'allocateur la gère en interne.
                // Pour une implémentation complète, voir NkTaggedAllocator wrapper.
                
                NkAllocator& alloc = allocator ? *allocator : NkGetDefaultAllocator();
                alloc.Deallocate(ptr);
                
                // Notification : on ne connaît pas la taille exacte ici
                // Solution : soit la passer en paramètre, soit utiliser un wrapper
                // qui stocke la taille dans un header avant le pointeur utilisateur.
                // Pour l'instant, on skip la notification dans TaggedFree.
                // Une version améliorée pourrait être :
                //   nk_size size = GetAllocationSize(ptr);  // À implémenter
                //   NkMemoryBudget::OnDeallocate(tag, size);
            }

            void* TaggedCalloc(nk_size count, nk_size elementSize, NkMemoryTag tag,
                              NkAllocator* allocator, nk_size alignment) {
                if (count == 0 || elementSize == 0) {
                    return nullptr;
                }
                
                // Vérification d'overflow pour count * elementSize
                if (count > NkNumericLimits<nk_size>::max() / elementSize) {
                    return nullptr;  // Overflow détecté
                }
                
                const nk_size totalBytes = count * elementSize;
                NkAllocator& alloc = allocator ? *allocator : NkGetDefaultAllocator();
                
                void* ptr = alloc.Calloc(totalBytes, alignment);
                
                if (ptr) {
                    NkMemoryBudget::OnAllocate(tag, totalBytes);
                }
                
                return ptr;
            }

        } // namespace memory
    } // namespace nkentseu

#else // NKENTSEU_DISABLE_MEMORY_TAGGING défini

    // -------------------------------------------------------------------------
    // VERSION NO-OP POUR RELEASE (zero overhead)
    // -------------------------------------------------------------------------
    namespace nkentseu {
        namespace memory {

            // Toutes les méthodes deviennent des no-ops inline ou retour constant
            // Le compilateur optimisera ces appels à néant en release

            void NkMemoryBudget::SetBudget(NkMemoryTag, nk_uint64) noexcept {}
            nk_uint64 NkMemoryBudget::GetBudget(NkMemoryTag) noexcept { return 0; }
            void NkMemoryBudget::OnAllocate(NkMemoryTag, nk_uint64) noexcept {}
            void NkMemoryBudget::OnDeallocate(NkMemoryTag, nk_uint64) noexcept {}
            nk_int64 NkMemoryBudget::GetBudgetRemaining(NkMemoryTag) noexcept { return INT64_MAX; }
            nk_bool NkMemoryBudget::IsOverBudget(NkMemoryTag) noexcept { return false; }
            NkMemoryTagStats NkMemoryBudget::GetStats(NkMemoryTag) noexcept { return NkMemoryTagStats(); }
            void NkMemoryBudget::DumpStats() noexcept {}
            void NkMemoryBudget::ResetStats() noexcept {}
            void NkMemoryBudget::SetBudgetAlertsEnabled(nk_bool) noexcept {}
            void NkMemoryBudget::SetBudgetWarningThreshold(float32) noexcept {}

            // Helpers d'allocation : redirection directe vers NkAlloc/NkFree
            void* TaggedAlloc(nk_size bytes, NkMemoryTag, NkAllocator* allocator, nk_size alignment) {
                return NkAlloc(bytes, allocator, alignment);
            }
            
            void TaggedFree(void* ptr, NkMemoryTag, NkAllocator* allocator) {
                NkFree(ptr, allocator);
            }
            
            void* TaggedCalloc(nk_size count, nk_size elementSize, NkMemoryTag,
                              NkAllocator* allocator, nk_size alignment) {
                return NkAllocZero(count, elementSize, allocator, alignment);
            }

        } // namespace memory
    } // namespace nkentseu

#endif // NKENTSEU_DISABLE_MEMORY_TAGGING

// =============================================================================
// NOTES DE MAINTENANCE
// =============================================================================
/*
    [THREAD-SAFETY]
    ✓ Tous les compteurs (current, peak, allocCount) utilisent NkAtomicUInt64
    ✓ Les lectures/écritures sont lock-free sur les plateformes supportant les atomiques 64-bit
    ✓ La configuration (budgets, alerts) est mise à jour rarement => pas d'atomiques nécessaires
    ✓ OnAllocate/OnDeallocate peuvent être appelés depuis n'importe quel thread

    [PERFORMANCE]
    - OnAllocate : 2-3 opérations atomiques + branching conditionnel pour les alertes
    - Overhead typique : ~10-30 ns par allocation (négligeable sauf allocations très fréquentes)
    - En release avec NKENTSEU_DISABLE_MEMORY_TAGGING : overhead = 0 (fonctions inline no-op)

    [MÉMOIRE]
    - gTagStates[256] : 256 * (4 * 8 bytes pour atomiques + 8 bytes budget) = ~10 KB
    - Négligeable pour un système de profiling, alloué statiquement (pas de heap)

    [EXTENSIONS POSSIBLES]
    1. Tracking de la taille par pointeur pour TaggedFree automatique :
       - Ajouter un header caché avant le pointeur utilisateur (comme NkUniquePtr)
       - Stocker : {size, tag, magic} pour validation et notification auto
    
    2. Histogrammes de taille par tag :
       - Ajouter std::array<NkAtomicUInt64, 32> sizeBins par TagState
       - Bins logarithmiques : 0-64, 64-128, ..., 1MB+
    
    3. Callstack sampling en debug :
       - Avec NKENTSEU_MEMORY_TAGGING_VERBOSE, capturer un échantillon de callstacks
       - Utile pour identifier les sources d'allocations "chaudes"
    
    4. Export vers outils externes :
       - Ajouter NkMemoryBudget::ExportToJson(const char* filepath)
       - Format compatible avec des viewers comme Chrome Tracing ou Custom tools

    [DEBUGGING TIPS]
    - Si un tag montre une fragmentation élevée : vérifier les patterns d'allocation/libération
    - Si peak >> current : potentielles fuites temporaires ou pics d'activité normaux
    - Utiliser DumpStats() en fin de level/load pour valider le cleanup mémoire
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================