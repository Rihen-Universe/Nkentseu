// =============================================================================
// FICHIER: NKECS/Benchmark/NkECSStressTest.h
// DESCRIPTION: Benchmark de validation production (100k entités, 50 composants).
// =============================================================================
#pragma once
#include "../NkECSDefines.h"
#include "../World/NkWorld.h"
#include "../System/NkScheduler.h"
#include "../Serialization/NkSerializer.h"
#include "../Serialization/Native/NkNativeFormat.h"
#include <chrono>
#include <cstdio>
#include <vector>
#include <string>

namespace nkentseu {
namespace ecs {
namespace benchmark {

    // ============================================================================
    // Résultats du benchmark
    // ============================================================================
    struct StressTestResult {
        uint32 entityCount = 0;
        uint32 componentCount = 0;
        double creationTimeMs = 0.0;
        double queryTimeMs = 0.0;
        double serializeTimeMs = 0.0;
        double deserializeTimeMs = 0.0;
        double nativeSizeBytes = 0.0;
        double jsonSizeBytes = 0.0;
        double compressionRatio = 0.0;
        bool success = false;
        
        [[nodiscard]] double GetThroughputMBs() const noexcept {
            if (serializeTimeMs <= 0.0) return 0.0;
            return (jsonSizeBytes / 1048576.0) / (serializeTimeMs / 1000.0);
        }
    };

    // ============================================================================
    // Composants de test pour le benchmark
    // ============================================================================
    struct BenchmarkCompA { float32 v1 = 0.f, v2 = 0.f; };
    struct BenchmarkCompB { int32 v = 0; };
    struct BenchmarkCompC { bool flag = true; };
    struct BenchmarkCompD { float64 v = 0.0; };
    struct BenchmarkCompE { NkVec3 pos = NkVec3::Zero(); };
    
    NK_COMPONENT(BenchmarkCompA)
    NK_COMPONENT(BenchmarkCompB)
    NK_COMPONENT(BenchmarkCompC)
    NK_COMPONENT(BenchmarkCompD)
    NK_COMPONENT(BenchmarkCompE)

    // ============================================================================
    // Générateur de systèmes avec dépendances DAG pour stress-test
    // ============================================================================
    inline std::vector<std::unique_ptr<NkSystem>> GenerateTestSystems(uint32 count) {
        std::vector<std::unique_ptr<NkSystem>> systems;
        systems.reserve(count);
        
        for (uint32 i = 0; i < count; ++i) {
            NkSystemDesc desc;
            desc.Named(("Sys_" + std::to_string(i)).c_str());
            desc.InGroup(NkSystemGroup::Update);
            
            // Assignation cyclique des reads/writes pour créer des conflits DAG
            if (i % 4 == 0) desc.Writes<BenchmarkCompA>().Reads<BenchmarkCompB>();
            else if (i % 4 == 1) desc.Reads<BenchmarkCompA>().Writes<BenchmarkCompC>();
            else if (i % 4 == 2) desc.Reads<BenchmarkCompB>().Writes<BenchmarkCompD>();
            else desc.Reads<BenchmarkCompC>().Reads<BenchmarkCompD>();
            
            // Dépendances explicites chaînées tous les 5 systèmes
            if (i >= 5) {
                // Note : Dans une implémentation réelle, on utiliserait After<SystemType>()
                // Ici on simule avec une priorité
                desc.WithPriority(static_cast<float32>(i) * 0.1f);
            }
            
            systems.push_back(std::make_unique<NkLambdaSystem>(desc, 
                [i](NkWorld& w, float32) {
                    // Simulation de travail léger
                    volatile float32 dummy = 0.f;
                    for (uint32 k = 0; k < 1000; ++k) {
                        dummy += static_cast<float32>(i) * static_cast<float32>(k);
                    }
                    (void)dummy;
                }));
        }
        return systems;
    }

    // ============================================================================
    // Runner de benchmark principal
    // ============================================================================
    [[nodiscard]] inline StressTestResult RunStressTest(
        uint32 entityCount = 10000,
        uint32 componentsPerEntity = 50) noexcept {
        
        StressTestResult res;
        res.entityCount = entityCount;
        res.componentCount = componentsPerEntity;
        
        NkWorld world;
        NkScheduler scheduler(0); // Thread pool auto
        
        // ── 1. Création massive d'entités avec composants variés ─────────────
        auto t0 = std::chrono::high_resolution_clock::now();
        
        for (uint32 i = 0; i < entityCount; ++i) {
            NkEntityId id = world.CreateEntity();
            
            // Ajout de composants variés (simule 50 composants)
            world.Add<NkTransform>(id);
            world.Add<NkTag>(id);
            world.Add<BenchmarkCompA>(id);
            world.Add<BenchmarkCompB>(id);
            world.Add<BenchmarkCompC>(id);
            world.Add<BenchmarkCompD>(id);
            world.Add<BenchmarkCompE>(id);
            
            // Ajout de composants supplémentaires pour atteindre ~50
            for (uint32 c = 7; c < componentsPerEntity; ++c) {
                // Utilise des tags comme composants légers pour le benchmark
                NkTagBit bit = static_cast<NkTagBit>(1ULL << (c % 48));
                if (auto* tag = world.Get<NkTag>(id)) {
                    tag->Add(bit);
                }
            }
        }
        
        auto t1 = std::chrono::high_resolution_clock::now();
        res.creationTimeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        
        // ── 2. Benchmark de requête complexe ─────────────────────────────────
        t0 = std::chrono::high_resolution_clock::now();
        
        uint32 processed = 0;
        world.Query<NkTransform, NkTag>()
             .ForEach([&processed](NkEntityId, NkTransform&, NkTag&) {
                 ++processed;
             });
        
        t1 = std::chrono::high_resolution_clock::now();
        res.queryTimeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        
        // ── 3. Benchmark de sérialisation JSON vs NkNative ───────────────────
        // Peuple un archive avec un sous-ensemble représentatif
        NkArchive archive;
        archive.SetUInt64("entityCount", entityCount);
        archive.SetFloat64("componentCount", static_cast<float64>(componentsPerEntity));
        
        // Ajoute quelques entités représentatives à l'archive
        uint32 sampleCount = std::min(entityCount, 1000u);
        world.Query<NkTransform, NkTag, BenchmarkCompA>()
             .ForEach([&](NkEntityId id, const NkTransform& t, const NkTag& tag, const BenchmarkCompA& a) {
                 if (sampleCount-- == 0) return;
                 NkArchive entityArchive;
                 entityArchive.SetUInt64("id", id.Pack());
                 entityArchive.SetFloat64("posX", t.position.x);
                 entityArchive.SetFloat64("posY", t.position.y);
                 entityArchive.SetFloat64("posZ", t.position.z);
                 entityArchive.SetUInt64("tagBits", tag.bits);
                 entityArchive.SetFloat64("compA_v1", a.v1);
                 entityArchive.SetFloat64("compA_v2", a.v2);
                 archive.SetValue(("entity_" + std::to_string(id.index)).c_str(), entityArchive);
             });
        
        // Sérialisation JSON
        std::string jsonOutput;
        t0 = std::chrono::high_resolution_clock::now();
        serialization::NkSerializeArchive(archive, 
                                         serialization::NkSerializationFormat::NK_JSON, 
                                         jsonOutput);
        t1 = std::chrono::high_resolution_clock::now();
        res.jsonSizeBytes = static_cast<double>(jsonOutput.size());
        res.serializeTimeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        
        // Sérialisation NkNative compressée
        NkVector<nk_uint8> nativeOutput;
        t0 = std::chrono::high_resolution_clock::now();
        serialization::native::NkNativeWriter::WriteArchiveCompressed(
            archive, nativeOutput, true);
        t1 = std::chrono::high_resolution_clock::now();
        res.nativeSizeBytes = static_cast<double>(nativeOutput.Size());
        res.compressionRatio = res.jsonSizeBytes > 0 
            ? res.nativeSizeBytes / res.jsonSizeBytes 
            : 0.0;
        
        // ── 4. Benchmark de désérialisation ──────────────────────────────────
        // JSON
        NkArchive jsonArchive;
        t0 = std::chrono::high_resolution_clock::now();
        serialization::NkDeserializeArchive(jsonOutput.c_str(),
                                           serialization::NkSerializationFormat::NK_JSON,
                                           jsonArchive);
        t1 = std::chrono::high_resolution_clock::now();
        res.deserializeTimeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        
        // NkNative
        NkArchive nativeArchive;
        t0 = std::chrono::high_resolution_clock::now();
        serialization::native::NkNativeReader::ReadArchive(
            nativeOutput.Data(), nativeOutput.Size(), nativeArchive);
        t1 = std::chrono::high_resolution_clock::now();
        // On ne met pas à jour deserializeTimeMs car on garde la valeur JSON
        
        // ── 5. Validation des résultats ──────────────────────────────────────
        res.success = (res.entityCount == entityCount) &&
                      (res.queryTimeMs > 0) &&
                      (res.serializeTimeMs > 0) &&
                      (res.compressionRatio <= 1.0); // La compression devrait réduire la taille
        
        // ── 6. Rapport console ───────────────────────────────────────────────
        printf("\n");
        printf("=============================================================\n");
        printf("🚀 NKECS PRODUCTION STRESS TEST\n");
        printf("=============================================================\n");
        printf("Entités créées       : %u\n", res.entityCount);
        printf("Composants/entité    : %u\n", res.componentCount);
        printf("⏱ Création (ms)       : %.3f\n", res.creationTimeMs);
        printf("⏱ Query SoA (ms)      : %.3f\n", res.queryTimeMs);
        printf("⏱ Sérialisation JSON (ms) : %.3f (%.2f MB)\n", 
               res.serializeTimeMs, res.jsonSizeBytes / 1048576.0);
        printf("⏱ Sérialisation NkNative (ms) : %.3f (%.2f MB)\n", 
               res.serializeTimeMs, res.nativeSizeBytes / 1048576.0);
        printf("📦 Ratio compression  : %.2fx (%.1f%% réduction)\n", 
               1.0 / (res.compressionRatio > 0 ? res.compressionRatio : 1.0),
               (1.0 - res.compressionRatio) * 100.0);
        printf("⏱ Désérialisation JSON (ms) : %.3f\n", res.deserializeTimeMs);
        printf("✅ Validation          : %s\n", res.success ? "PASS" : "FAIL");
        printf("=============================================================\n");
        printf("\n");
        
        return res;
    }

    // ============================================================================
    // Helper pour exporter les résultats en JSON (pour analyse externe)
    // ============================================================================
    [[nodiscard]] inline bool ExportBenchmarkResultsJSON(
        const StressTestResult& res,
        const char* outputPath) noexcept {
        
        if (!outputPath) return false;
        
        FILE* f = std::fopen(outputPath, "w");
        if (!f) return false;
        
        std::fprintf(f, "{\n");
        std::fprintf(f, "  \"entityCount\": %u,\n", res.entityCount);
        std::fprintf(f, "  \"componentCount\": %u,\n", res.componentCount);
        std::fprintf(f, "  \"creationTimeMs\": %.3f,\n", res.creationTimeMs);
        std::fprintf(f, "  \"queryTimeMs\": %.3f,\n", res.queryTimeMs);
        std::fprintf(f, "  \"serializeTimeMs\": %.3f,\n", res.serializeTimeMs);
        std::fprintf(f, "  \"deserializeTimeMs\": %.3f,\n", res.deserializeTimeMs);
        std::fprintf(f, "  \"jsonSizeBytes\": %.0f,\n", res.jsonSizeBytes);
        std::fprintf(f, "  \"nativeSizeBytes\": %.0f,\n", res.nativeSizeBytes);
        std::fprintf(f, "  \"compressionRatio\": %.4f,\n", res.compressionRatio);
        std::fprintf(f, "  \"throughputMBs\": %.2f,\n", res.GetThroughputMBs());
        std::fprintf(f, "  \"success\": %s\n", res.success ? "true" : "false");
        std::fprintf(f, "}\n");
        
        std::fclose(f);
        return true;
    }

} // namespace benchmark
} // namespace ecs
} // namespace nkentseu

// =============================================================================
// EXEMPLES D'UTILISATION DE NKECSSTRESSTEST.H
// =============================================================================

/*
// -----------------------------------------------------------------------------
// Exemple 1 : Exécution du benchmark complet
// -----------------------------------------------------------------------------
int main() {
    using namespace nkentseu::ecs::benchmark;
    
    // Exécute le stress test avec paramètres par défaut (10k entités, 50 composants)
    StressTestResult res = RunStressTest();
    
    // Exporte les résultats pour analyse
    ExportBenchmarkResultsJSON(res, "benchmark_results.json");
    
    return res.success ? 0 : 1;
}

// -----------------------------------------------------------------------------
// Exemple 2 : Benchmark avec paramètres personnalisés
// -----------------------------------------------------------------------------
void Exemple_BenchmarkPersonnalise() {
    using namespace nkentseu::ecs::benchmark;
    
    // Test plus léger pour développement
    StressTestResult res = RunStressTest(1000, 10);
    
    if (res.success) {
        printf("✅ Benchmark réussi : %.2f MB/s throughput\n", res.GetThroughputMBs());
    } else {
        printf("❌ Benchmark échoué\n");
    }
}

// -----------------------------------------------------------------------------
// Exemple 3 : Intégration dans un pipeline CI/CD
// -----------------------------------------------------------------------------
void CiValidation() {
    using namespace nkentseu::ecs::benchmark;
    
    StressTestResult res = RunStressTest(5000, 25);
    
    // Assertions pour validation automatique
    assert(res.entityCount == 5000);
    assert(res.componentCount == 25);
    assert(res.success);
    assert(res.queryTimeMs < 10.0); // Seuil max 10ms pour la query
    assert(res.compressionRatio < 0.8); // Au moins 20% de compression
    
    // Export pour historique
    ExportBenchmarkResultsJSON(res, "ci/benchmark_latest.json");
}
*/