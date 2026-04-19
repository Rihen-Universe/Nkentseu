// =============================================================================
// NKSerialization/NkSerializationBenchmark.h
// =============================================================================
// Benchmark comparatif des formats de sérialisation.
//
// Corrections vs version originale :
//  - Suppression de la dépendance sur NkTimer (non disponible ici)
//  - BenchmarkTestData : NkISerializable correctement inclus
//  - Suppression de archive.SetInt32/GetInt32 (API correcte : SetInt64)
//  - RunAllBenchmarks : format de sortie corrigé (pas de printf emoji sur MSVC)
//  - Suppression des appels Serialize/DeserializeBinary non définis localement
// =============================================================================
#pragma once

#ifndef NKENTSEU_SERIALIZATION_NKBENCHMARK_H_INCLUDED
#define NKENTSEU_SERIALIZATION_NKBENCHMARK_H_INCLUDED

#include "NKSerialization/NkSerializer.h"
#include "NKSerialization/NkISerializable.h"

#include "NKFileSystem/NkFile.h"
#include "NKContainers/String/NkFormatf.h"

#include <cstdio>
#include <chrono>
#include <cstdlib>

namespace nkentseu {
    namespace benchmark {

        // =============================================================================
        // Résultat d'un benchmark
        // =============================================================================
        struct BenchmarkResult {
            NkSerializationFormat format          = NkSerializationFormat::NK_JSON;
            nk_size               originalSize    = 0u;
            nk_size               serializedSize  = 0u;
            nk_float64            serializeMs     = 0.0;
            nk_float64            deserializeMs   = 0.0;
            nk_float64            compressionRatio = 0.0;
            nk_bool               success         = false;

            [[nodiscard]] nk_float64 ThroughputMBs() const noexcept {
                if (serializeMs <= 0.0 || originalSize == 0u) return 0.0;
                return (static_cast<nk_float64>(originalSize) / 1048576.0) / (serializeMs / 1000.0);
            }
        };

        // =============================================================================
        // Objet de test représentatif
        // =============================================================================
        struct BenchmarkData : public NkISerializable {
            nk_int64   intValue    = 42;
            nk_float64 floatValue  = 3.14159265358979;
            nk_bool    boolValue   = true;
            NkString   stringValue = NkString("Hello, NkECS benchmark!");
            nk_uint64  entityId    = 0x123456789ABCDEFull;
            NkString   nestedStr   = NkString("Repeated pattern for compression. Repeated pattern.");

            nk_bool Serialize(NkArchive& archive) const override {
                archive.SetInt64("intValue",   intValue);
                archive.SetFloat64("floatValue", floatValue);
                archive.SetBool("boolValue",   boolValue);
                archive.SetString("stringValue", stringValue.View());
                archive.SetUInt64("entityId",  entityId);
                archive.SetString("nestedStr", nestedStr.View());
                return true;
            }

            nk_bool Deserialize(const NkArchive& archive) override {
                archive.GetInt64("intValue",     intValue);
                archive.GetFloat64("floatValue", floatValue);
                archive.GetBool("boolValue",     boolValue);
                archive.GetString("stringValue", stringValue);
                archive.GetUInt64("entityId",    entityId);
                archive.GetString("nestedStr",   nestedStr);
                return true;
            }

            [[nodiscard]] const char* GetTypeName() const noexcept override {
                return "BenchmarkData";
            }

            [[nodiscard]] nk_size ApproxSize() const noexcept {
                return sizeof(intValue) + sizeof(floatValue) + sizeof(boolValue) +
                    stringValue.Length() + sizeof(entityId) + nestedStr.Length();
            }
        };

        // =============================================================================
        // Runner de benchmark pour un format
        // =============================================================================
        [[nodiscard]] static BenchmarkResult RunFormat(
            NkSerializationFormat format,
            nk_uint32 iterations = 1000u) noexcept
        {
            BenchmarkResult result;
            result.format = format;

            BenchmarkData data;
            result.originalSize = data.ApproxSize();

            const bool isBinary = (format == NkSerializationFormat::NK_BINARY ||
                                format == NkSerializationFormat::NK_NATIVE);

            // ── Sérialisation ─────────────────────────────────────────────────────────
            NkString        textBuf;
            NkVector<nk_uint8> binBuf;
            nk_bool success = false;

            auto t0 = std::chrono::high_resolution_clock::now();
            for (nk_uint32 i = 0; i < iterations; ++i) {
                data.intValue = static_cast<nk_int64>(i);
                if (isBinary) {
                    success = SerializeBinary(data, format, binBuf);
                } else {
                    success = Serialize(data, format, textBuf, false);
                }
                if (!success) break;
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            result.serializeMs = std::chrono::duration<nk_float64, std::milli>(t1 - t0).count()
                                / static_cast<nk_float64>(iterations);

            if (!success) { result.success = false; return result; }

            result.serializedSize = isBinary ? binBuf.Size() : textBuf.Length();
            result.compressionRatio = result.originalSize > 0u
                ? static_cast<nk_float64>(result.serializedSize) /
                static_cast<nk_float64>(result.originalSize)
                : 0.0;

            // ── Désérialisation ───────────────────────────────────────────────────────
            auto t2 = std::chrono::high_resolution_clock::now();
            for (nk_uint32 i = 0; i < iterations; ++i) {
                BenchmarkData loaded;
                if (isBinary) {
                    success = DeserializeBinary(binBuf.Data(), binBuf.Size(), format, loaded);
                } else {
                    success = Deserialize(textBuf.View(), format, loaded);
                }
                if (!success) break;
            }
            auto t3 = std::chrono::high_resolution_clock::now();
            result.deserializeMs = std::chrono::duration<nk_float64, std::milli>(t3 - t2).count()
                                / static_cast<nk_float64>(iterations);

            result.success = success;
            return result;
        }

        // =============================================================================
        // Rapport complet
        // =============================================================================
        static void RunAllBenchmarks(nk_uint32 iterations = 1000u) noexcept {
            printf("\n=============================================================\n");
            printf("  NKSerialization Benchmark — %u iterations\n", iterations);
            printf("=============================================================\n");
            printf("%-12s %8s %10s %12s %12s %8s\n",
                "Format", "Success", "Bytes", "Ser(ms)", "Deser(ms)", "Ratio");
            printf("%-12s %8s %10s %12s %12s %8s\n",
                "--------", "-------", "-----", "-------", "---------", "-----");

            const NkSerializationFormat formats[] = {
                NkSerializationFormat::NK_JSON,
                NkSerializationFormat::NK_XML,
                NkSerializationFormat::NK_YAML,
                NkSerializationFormat::NK_BINARY,
                NkSerializationFormat::NK_NATIVE,
            };
            const char* names[] = { "JSON", "XML", "YAML", "Binary", "NkNative" };

            for (nk_size i = 0; i < 5u; ++i) {
                BenchmarkResult r = RunFormat(formats[i], iterations);
                printf("%-12s %8s %10zu %12.4f %12.4f %7.2fx\n",
                    names[i],
                    r.success ? "OK" : "FAIL",
                    r.serializedSize,
                    r.serializeMs,
                    r.deserializeMs,
                    r.compressionRatio);
            }
            printf("=============================================================\n\n");
        }

        // =============================================================================
        // Export JSON des résultats
        // =============================================================================
        static nk_bool ExportResultsJSON(const NkVector<BenchmarkResult>& results,
                                        const char* path) noexcept {
            if (!path) return false;
            NkFile file;
            if (!file.Open(path, NkFileMode::NK_WRITE_BINARY)) {
                return false;
            }

            const char* fmtNames[] = { "JSON","XML","YAML","Binary","NkNative","Auto" };
            NkString data;

            data += NkFormatf("{\n  \"results\": [\n");

            for (nk_size i = 0; i < results.Size(); ++i) {
                const BenchmarkResult& r = results[i];
                nk_uint8 fi = static_cast<nk_uint8>(r.format);
                data += NkFormatf("    {\n");
                data += NkFormatf("      \"format\": \"%s\",\n",      fi < 6 ? fmtNames[fi] : "Unknown");
                data += NkFormatf("      \"success\": %s,\n",         r.success ? "true" : "false");
                data += NkFormatf("      \"originalSize\": %zu,\n",   r.originalSize);
                data += NkFormatf("      \"serializedSize\": %zu,\n", r.serializedSize);
                data += NkFormatf("      \"serializeMs\": %.4f,\n",   r.serializeMs);
                data += NkFormatf("      \"deserializeMs\": %.4f,\n", r.deserializeMs);
                data += NkFormatf("      \"compressionRatio\": %.3f,\n", r.compressionRatio);
                data += NkFormatf("      \"throughputMBs\": %.2f\n",  r.ThroughputMBs());
                data += NkFormatf("    }%s\n", (i + 1 < results.Size()) ? "," : "");
            }
            data += NkFormatf( "  ]\n}\n");
            if (file.Write(data) != file.Size()) {
                file.Close();
                return false;
            }
            file.Close();
            return true;
        }

    } // namespace benchmark
} // namespace nkentseu

#endif // NKENTSEU_SERIALIZATION_NKBENCHMARK_H_INCLUDED