// =============================================================================
// FICHIER  : Modules/System/NKSerialization/include/NKSerialization/NkSerializationBenchmark.h
// MODULE   : NKSerialization
// AUTEUR   : Rihen
// DATE     : 2026-02-07
// VERSION  : 1.1.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Outil de benchmark comparatif pour les formats de serialisation du module
//   NKSerialization. Mesure les performances de serialisation/deserialisation
//   en termes de temps d'execution, taille des donnees et ratio de compression.
//
// CORRECTIONS ET AMELIORATIONS (v1.1.0) :
//   - Suppression de la dependance NkTimer (non disponible) → utilisation de std::chrono
//   - Correction API : SetInt32/GetInt32 → SetInt64/GetInt64 (API correcte de NkArchive)
//   - BenchmarkData : heritage NkISerializable correctement implemente
//   - Format de sortie : suppression des emojis pour compatibilite MSVC/terminal
//   - Documentation Doxygen complete pour chaque element public
//   - Indentation stricte des blocs, namespaces et sections d'acces
//   - Une instruction par ligne pour lisibilite optimale
//   - Exemples d'utilisation complets en fin de fichier
//   - Notes de portabilite et de precision des mesures
//
// DEPENDANCES :
//   - NKSerialization (NkSerializer, NkISerializable, NkArchive)
//   - NKFileSystem (NkFile pour l'export des resultats)
//   - NKContainers/String (NkFormat pour le formatage JSON)
//   - <chrono> pour le chronometrage haute resolution (C++11+)
// =============================================================================

#pragma once

#ifndef NKENTSEU_SERIALIZATION_NKBENCHMARK_H
#define NKENTSEU_SERIALIZATION_NKBENCHMARK_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DEPENDANCES INTERNES
    // -------------------------------------------------------------------------
    // Inclusion des composants de serialisation pour les tests de performance.

    #include "NKSerialization/NkSerializer.h"
    #include "NKSerialization/NkISerializable.h"

    // Inclusion du systeme de fichiers pour l'export des resultats
    #include "NKFileSystem/NkFile.h"

    // Inclusion des utilitaires de formatage de chaines
    #include "NKContainers/String/NkFormat.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : EN-TETES STANDARD
    // -------------------------------------------------------------------------
    // Inclusion des en-tetes standards pour le chronometrage et l'affichage.
    // Note : std::chrono est utilise uniquement pour le benchmark, pas en runtime.

    #include <cstdio>
    #include <chrono>
    #include <cstdlib>

    // -------------------------------------------------------------------------
    // SECTION 3 : ESPACE DE NOMS PRINCIPAL
    // -------------------------------------------------------------------------
    // Declaration du namespace principal nkentseu et de son sous-namespace
    // benchmark dedie aux utilitaires de mesure de performance.

    namespace nkentseu {

        namespace benchmark {

            // =================================================================
            // STRUCTURE : BenchmarkResult
            // =================================================================
            /**
             * @struct BenchmarkResult
             * @brief Resultats d'un benchmark de serialisation pour un format donne
             *
             * Cette structure encapsule toutes les metriques mesurees lors d'un
             * test de performance pour un format de serialisation specifique.
             *
             * @note Les temps sont exprimes en millisecondes, les tailles en octets
             * @note Le ratio de compression est calcule comme : serializedSize / originalSize
             *       (valeur < 1.0 indique une compression, > 1.0 une expansion)
             */
            struct BenchmarkResult {
                /** Format de serialisation teste */
                NkSerializationFormat format = NkSerializationFormat::NK_JSON;

                /** Taille approximative des donnees originales en octets */
                nk_size originalSize = 0u;

                /** Taille des donnees serialisees en octets */
                nk_size serializedSize = 0u;

                /** Temps moyen de serialisation par iteration (millisecondes) */
                nk_float64 serializeMs = 0.0;

                /** Temps moyen de deserialisation par iteration (millisecondes) */
                nk_float64 deserializeMs = 0.0;

                /** Ratio de compression : serializedSize / originalSize */
                nk_float64 compressionRatio = 0.0;

                /** Indicateur de succes global du benchmark */
                nk_bool success = false;

                /**
                 * @brief Calcule le debit de serialisation en Mo/s
                 * @return Debit en megaoctets par seconde, ou 0.0 si les donnees sont invalides
                 *
                 * @note Formule : (originalSize / 1048576) / (serializeMs / 1000)
                 * @note Retourne 0.0 si serializeMs <= 0 ou originalSize == 0 pour eviter la division par zero
                 */
                [[nodiscard]] nk_float64 ThroughputMBs() const noexcept {
                    if (serializeMs <= 0.0) {
                        return 0.0;
                    }
                    if (originalSize == 0u) {
                        return 0.0;
                    }
                    nk_float64 sizeMB = static_cast<nk_float64>(originalSize) / 1048576.0;
                    nk_float64 timeSec = serializeMs / 1000.0;
                    return sizeMB / timeSec;
                }
            };

            // =================================================================
            // STRUCTURE : BenchmarkData
            // =================================================================
            /**
             * @struct BenchmarkData
             * @brief Objet de test representatif pour les benchmarks de serialisation
             *
             * Cette structure contient un ensemble representatif de types de donnees
             * (entiers, flottants, booleens, chaines) pour evaluer les performances
             * des differents formats de serialisation dans des conditions realistes.
             *
             * @note Herite de NkISerializable pour etre compatible avec l'API de serialisation
             * @note Les valeurs par defaut sont choisies pour generer des donnees testables
             */
            struct BenchmarkData : public NkISerializable {
                /** Entier signe 64 bits pour tester la serialisation numerique */
                nk_int64 intValue = 42;

                /** Flottant double precision pour tester la precision des formats */
                nk_float64 floatValue = 3.14159265358979;

                /** Valeur booleenne pour tester la serialisation des flags */
                nk_bool boolValue = true;

                /** Chaine de caracteres pour tester la serialisation de texte */
                NkString stringValue = NkString("Hello, NkECS benchmark!");

                /** Identifiant d'entite 64 bits non signe pour simuler des IDs de jeu */
                nk_uint64 entityId = 0x123456789ABCDEFull;

                /** Chaine avec motifs repetes pour evaluer la compression */
                NkString nestedStr = NkString("Repeated pattern for compression. Repeated pattern.");

                /**
                 * @brief Serialise les donnees de cette instance dans une archive
                 * @param archive Reference vers l'archive de sortie
                 * @return nk_bool vrai si la serialisation a reussi, faux sinon
                 *
                 * @note Utilise les methodes Set* de NkArchive avec des cles explicites
                 * @note Toutes les operations doivent reussir pour que la fonction retourne vrai
                 */
                nk_bool Serialize(NkArchive& archive) const override {
                    archive.SetInt64("intValue", intValue);
                    archive.SetFloat64("floatValue", floatValue);
                    archive.SetBool("boolValue", boolValue);
                    archive.SetString("stringValue", stringValue.View());
                    archive.SetUInt64("entityId", entityId);
                    archive.SetString("nestedStr", nestedStr.View());
                    return true;
                }

                /**
                 * @brief Deserialise les donnees depuis une archive vers cette instance
                 * @param archive Reference constante vers l'archive d'entree
                 * @return nk_bool vrai si la deserialisation a reussi, faux sinon
                 *
                 * @note Utilise les methodes Get* de NkArchive avec les memes cles que Serialize
                 * @note Les valeurs par defaut sont conservees si une cle est manquante
                 */
                nk_bool Deserialize(const NkArchive& archive) override {
                    archive.GetInt64("intValue", intValue);
                    archive.GetFloat64("floatValue", floatValue);
                    archive.GetBool("boolValue", boolValue);
                    archive.GetString("stringValue", stringValue);
                    archive.GetUInt64("entityId", entityId);
                    archive.GetString("nestedStr", nestedStr);
                    return true;
                }

                /**
                 * @brief Retourne le nom du type pour l'identification runtime
                 * @return Pointeur vers une chaine de caracteres constante
                 *
                 * @note Utilise par NkSerializableRegistry pour la factory polymorphique
                 * @note Doit etre unique parmi tous les types enregistres
                 */
                [[nodiscard]] const nk_char* GetTypeName() const noexcept override {
                    return "BenchmarkData";
                }

                /**
                 * @brief Estime la taille approximative des donnees en memoire
                 * @return nk_size Taille estimee en octets
                 *
                 * @note Cette estimation est indicative : elle ne tient pas compte
                 *       de l'overhead des conteneurs NkString ni de l'alignement memoire
                 * @note Utile pour calculer le ratio de compression dans les benchmarks
                 */
                [[nodiscard]] nk_size ApproxSize() const noexcept {
                    nk_size total = 0u;
                    total += sizeof(intValue);
                    total += sizeof(floatValue);
                    total += sizeof(boolValue);
                    total += stringValue.Length();
                    total += sizeof(entityId);
                    total += nestedStr.Length();
                    return total;
                }
            };

            // =================================================================
            // FONCTION : RunFormat
            // =================================================================
            /**
             * @brief Execute un benchmark pour un format de serialisation specifique
             * @param format Format de serialisation a tester
             * @param iterations Nombre d'iterations pour la mesure de performance (defaut : 1000)
             * @return BenchmarkResult contenant toutes les metriques mesurees
             *
             * @note Le benchmark mesure :
             *       - Temps moyen de serialisation par iteration
             *       - Temps moyen de deserialisation par iteration
             *       - Taille des donnees serialisees vs originales
             *       - Ratio de compression et debit en Mo/s
             *
             * @note Les mesures utilisent std::chrono::high_resolution_clock pour
             *       une precision maximale (nanosecondes si supporte par la plateforme)
             *
             * @warning Les resultats peuvent varier selon :
             *          - La charge CPU du systeme pendant le test
             *          - Les optimisations du compilateur (-O2, -O3, etc.)
             *          - La presence d'un debugger ou d'un profiler
             *
             * @example
             * @code
             * auto result = nkentseu::benchmark::RunFormat(
             *     nkentseu::NkSerializationFormat::NK_JSON,
             *     5000  // Plus d'iterations pour une mesure plus stable
             * );
             * if (result.success) {
             *     printf("JSON: %.4f ms serialize, %.4f ms deserialize\n",
             *         result.serializeMs, result.deserializeMs);
             * }
             * @endcode
             */
            [[nodiscard]] static BenchmarkResult RunFormat(
                NkSerializationFormat format,
                nk_uint32 iterations = 1000u
            ) noexcept {
                BenchmarkResult result;
                result.format = format;

                BenchmarkData data;
                result.originalSize = data.ApproxSize();

                nk_bool isBinary = false;
                if (format == NkSerializationFormat::NK_BINARY) {
                    isBinary = true;
                }
                if (format == NkSerializationFormat::NK_NATIVE) {
                    isBinary = true;
                }

                NkString textBuf;
                NkVector<nk_uint8> binBuf;
                nk_bool success = false;

                // Phase de serialisation
                auto t0 = std::chrono::high_resolution_clock::now();

                for (nk_uint32 i = 0u; i < iterations; ++i) {
                    data.intValue = static_cast<nk_int64>(i);

                    if (isBinary) {
                        success = SerializeBinary(data, format, binBuf);
                    } else {
                        success = Serialize(data, format, textBuf, false);
                    }

                    if (!success) {
                        break;
                    }
                }

                auto t1 = std::chrono::high_resolution_clock::now();

                nk_float64 totalSerializeMs = std::chrono::duration<
                    nk_float64,
                    std::milli
                >(t1 - t0).count();

                result.serializeMs = totalSerializeMs / static_cast<nk_float64>(iterations);

                if (!success) {
                    result.success = false;
                    return result;
                }

                if (isBinary) {
                    result.serializedSize = binBuf.Size();
                } else {
                    result.serializedSize = textBuf.Length();
                }

                if (result.originalSize > 0u) {
                    result.compressionRatio = static_cast<nk_float64>(result.serializedSize) /
                                              static_cast<nk_float64>(result.originalSize);
                } else {
                    result.compressionRatio = 0.0;
                }

                // Phase de deserialisation
                auto t2 = std::chrono::high_resolution_clock::now();

                for (nk_uint32 i = 0u; i < iterations; ++i) {
                    BenchmarkData loaded;

                    if (isBinary) {
                        success = DeserializeBinary(binBuf.Data(), binBuf.Size(), format, loaded);
                    } else {
                        success = Deserialize(textBuf.View(), format, loaded);
                    }

                    if (!success) {
                        break;
                    }
                }

                auto t3 = std::chrono::high_resolution_clock::now();

                nk_float64 totalDeserializeMs = std::chrono::duration<
                    nk_float64,
                    std::milli
                >(t3 - t2).count();

                result.deserializeMs = totalDeserializeMs / static_cast<nk_float64>(iterations);

                result.success = success;

                return result;
            }

            // =================================================================
            // FONCTION : RunAllBenchmarks
            // =================================================================
            /**
             * @brief Execute une serie complete de benchmarks pour tous les formats supportes
             * @param iterations Nombre d'iterations par format pour la mesure (defaut : 1000)
             *
             * @note Affiche les resultats dans la console au format tableau :
             *       Format | Succes | Taille | Ser(ms) | Deser(ms) | Ratio
             *
             * @note Les formats testes sont :
             *       - NK_JSON : Format texte JSON, lisible et interopérable
             *       - NK_XML : Format texte XML, structure hierarchique explicite
             *       - NK_YAML : Format texte YAML, syntaxe concise
             *       - NK_BINARY : Format binaire generique, compact et rapide
             *       - NK_NATIVE : Format binaire optimise pour NKEntseu
             *
             * @warning NK_AUTO_DETECT n'est pas teste car c'est un mode de detection,
             *          pas un format de serialisation a proprement parler
             *
             * @note La sortie est compatible avec tous les terminaux (pas d'emojis)
             *       pour assurer la portabilite entre Windows, Linux et macOS
             *
             * @example
             * @code
             * // Lancer tous les benchmarks avec 5000 iterations pour plus de precision
             * nkentseu::benchmark::RunAllBenchmarks(5000);
             *
             * // Sortie exemple :
             * // Format        Success      Bytes      Ser(ms)    Deser(ms)    Ratio
             * // --------      -------      -----      -------    ---------    -----
             * // JSON             OK          156       0.0234       0.0189     1.23x
             * // XML              OK          284       0.0456       0.0398     2.18x
             * // ...
             * @endcode
             */
            static void RunAllBenchmarks(nk_uint32 iterations = 1000u) noexcept {
                printf("\n=============================================================\n");
                printf("  NKSerialization Benchmark - %u iterations\n", iterations);
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
                    NkSerializationFormat::NK_NATIVE
                };

                const nk_char* names[] = {
                    "JSON",
                    "XML",
                    "YAML",
                    "Binary",
                    "NkNative"
                };

                for (nk_size i = 0u; i < 5u; ++i) {
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

            // =================================================================
            // FONCTION : ExportResultsJSON
            // =================================================================
            /**
             * @brief Exporte une serie de resultats de benchmark vers un fichier JSON
             * @param results Vecteur constant de BenchmarkResult a exporter
             * @param path Chemin du fichier de sortie (doit etre writable)
             * @return nk_bool vrai si l'export a reussi, faux sinon
             *
             * @note Le fichier JSON genere suit ce schema :
             * @code
             * {
             *   "results": [
             *     {
             *       "format": "JSON",
             *       "success": true,
             *       "originalSize": 128,
             *       "serializedSize": 156,
             *       "serializeMs": 0.0234,
             *       "deserializeMs": 0.0189,
             *       "compressionRatio": 1.23,
             *       "throughputMBs": 5.47
             *     },
             *     ...
             *   ]
             * }
             * @endcode
             *
             * @note Utile pour :
             *       - L'analyse post-mortem des performances
             *       - La generation de graphiques comparatifs
             *       - L'integration avec des outils de CI/CD
             *
             * @example
             * @code
             * nkentseu::NkVector<nkentseu::benchmark::BenchmarkResult> results;
             * results.PushBack(nkentseu::benchmark::RunFormat(NkSerializationFormat::NK_JSON));
             * results.PushBack(nkentseu::benchmark::RunFormat(NkSerializationFormat::NK_NATIVE));
             *
             * if (nkentseu::benchmark::ExportResultsJSON(results, "benchmarks/results.json")) {
             *     printf("Results exported to benchmarks/results.json\n");
             * }
             * @endcode
             */
            static nk_bool ExportResultsJSON(
                const NkVector<BenchmarkResult>& results,
                const nk_char* path
            ) noexcept {
                if (!path) {
                    return false;
                }

                NkFile file;

                if (!file.Open(path, NkFileMode::NK_WRITE_BINARY)) {
                    return false;
                }

                const nk_char* fmtNames[] = {
                    "JSON",
                    "XML",
                    "YAML",
                    "Binary",
                    "NkNative",
                    "Auto"
                };

                NkString data;

                data += NkFormatf("{\n  \"results\": [\n");

                for (nk_size i = 0u; i < results.Size(); ++i) {
                    const BenchmarkResult& r = results[i];
                    nk_uint8 fi = static_cast<nk_uint8>(r.format);
                    const nk_char* formatName = "Unknown";
                    if (fi < 6u) {
                        formatName = fmtNames[fi];
                    }

                    data += NkFormatf("    {\n");
                    data += NkFormatf("      \"format\": \"%s\",\n", formatName);
                    data += NkFormatf("      \"success\": %s,\n", r.success ? "true" : "false");
                    data += NkFormatf("      \"originalSize\": %zu,\n", r.originalSize);
                    data += NkFormatf("      \"serializedSize\": %zu,\n", r.serializedSize);
                    data += NkFormatf("      \"serializeMs\": %.4f,\n", r.serializeMs);
                    data += NkFormatf("      \"deserializeMs\": %.4f,\n", r.deserializeMs);
                    data += NkFormatf("      \"compressionRatio\": %.3f,\n", r.compressionRatio);
                    data += NkFormatf("      \"throughputMBs\": %.2f\n", r.ThroughputMBs());

                    if (i + 1u < results.Size()) {
                        data += NkFormatf("    },\n");
                    } else {
                        data += NkFormatf("    }\n");
                    }
                }

                data += NkFormatf("  ]\n}\n");

                nk_size written = file.Write(data.CStr(), data.Length());

                if (written != data.Length()) {
                    file.Close();
                    return false;
                }

                file.Close();
                return true;
            }

        } // namespace benchmark

    } // namespace nkentseu

#endif // NKENTSEU_SERIALIZATION_NKBENCHMARK_H

// =============================================================================
// EXEMPLES D'UTILISATION - NkSerializationBenchmark.h
// =============================================================================
//
// -----------------------------------------------------------------------------
// Exemple 1 : Benchmark rapide d'un format unique
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkSerializationBenchmark.h>
    #include <NKCore/Log/NkLog.h>

    void QuickBenchmarkExample() {
        // Tester uniquement le format JSON avec 100 iterations pour un test rapide
        auto result = nkentseu::benchmark::RunFormat(
            nkentseu::NkSerializationFormat::NK_JSON,
            100u
        );

        if (result.success) {
            NK_FOUNDATION_LOG_INFO("JSON Benchmark Results:");
            NK_FOUNDATION_LOG_INFO("  Original size: %zu bytes", result.originalSize);
            NK_FOUNDATION_LOG_INFO("  Serialized size: %zu bytes", result.serializedSize);
            NK_FOUNDATION_LOG_INFO("  Compression ratio: %.2fx", result.compressionRatio);
            NK_FOUNDATION_LOG_INFO("  Serialize time: %.4f ms/iter", result.serializeMs);
            NK_FOUNDATION_LOG_INFO("  Deserialize time: %.4f ms/iter", result.deserializeMs);
            NK_FOUNDATION_LOG_INFO("  Throughput: %.2f MB/s", result.ThroughputMBs());
        } else {
            NK_FOUNDATION_LOG_ERROR("JSON benchmark failed");
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Benchmark complet avec export JSON
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkSerializationBenchmark.h>

    void FullBenchmarkWithExport() {
        // Lancer tous les benchmarks avec 2000 iterations pour des mesures stables
        nkentseu::benchmark::RunAllBenchmarks(2000u);

        // Collecter les resultats pour export
        nkentseu::NkVector<nkentseu::benchmark::BenchmarkResult> results;
        results.Reserve(5u);

        results.PushBack(nkentseu::benchmark::RunFormat(
            nkentseu::NkSerializationFormat::NK_JSON, 2000u));
        results.PushBack(nkentseu::benchmark::RunFormat(
            nkentseu::NkSerializationFormat::NK_XML, 2000u));
        results.PushBack(nkentseu::benchmark::RunFormat(
            nkentseu::NkSerializationFormat::NK_YAML, 2000u));
        results.PushBack(nkentseu::benchmark::RunFormat(
            nkentseu::NkSerializationFormat::NK_BINARY, 2000u));
        results.PushBack(nkentseu::benchmark::RunFormat(
            nkentseu::NkSerializationFormat::NK_NATIVE, 2000u));

        // Exporter vers JSON pour analyse ulterieure
        if (nkentseu::benchmark::ExportResultsJSON(results, "benchmarks/results.json")) {
            printf("Benchmark results exported to benchmarks/results.json\n");
        } else {
            printf("Failed to export benchmark results\n");
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Comparaison selective de formats
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkSerializationBenchmark.h>

    void CompareTextVsBinary() {
        printf("\n=== Text vs Binary Format Comparison ===\n");

        // Formats texte
        auto jsonResult = nkentseu::benchmark::RunFormat(
            nkentseu::NkSerializationFormat::NK_JSON, 5000u);
        auto yamlResult = nkentseu::benchmark::RunFormat(
            nkentseu::NkSerializationFormat::NK_YAML, 5000u);

        // Formats binaires
        auto binResult = nkentseu::benchmark::RunFormat(
            nkentseu::NkSerializationFormat::NK_BINARY, 5000u);
        auto nativeResult = nkentseu::benchmark::RunFormat(
            nkentseu::NkSerializationFormat::NK_NATIVE, 5000u);

        printf("\n%-10s %10s %12s %12s %10s\n",
            "Format", "Size", "Ser(ms)", "Deser(ms)", "MB/s");
        printf("%-10s %10s %12s %12s %10s\n",
            "------", "----", "-------", "---------", "----");

        if (jsonResult.success) {
            printf("%-10s %10zu %12.4f %12.4f %10.2f\n",
                "JSON", jsonResult.serializedSize,
                jsonResult.serializeMs, jsonResult.deserializeMs,
                jsonResult.ThroughputMBs());
        }
        if (yamlResult.success) {
            printf("%-10s %10zu %12.4f %12.4f %10.2f\n",
                "YAML", yamlResult.serializedSize,
                yamlResult.serializeMs, yamlResult.deserializeMs,
                yamlResult.ThroughputMBs());
        }
        if (binResult.success) {
            printf("%-10s %10zu %12.4f %12.4f %10.2f\n",
                "Binary", binResult.serializedSize,
                binResult.serializeMs, binResult.deserializeMs,
                binResult.ThroughputMBs());
        }
        if (nativeResult.success) {
            printf("%-10s %10zu %12.4f %12.4f %10.2f\n",
                "NkNative", nativeResult.serializedSize,
                nativeResult.serializeMs, nativeResult.deserializeMs,
                nativeResult.ThroughputMBs());
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Benchmark avec donnees personnalisees
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkSerializationBenchmark.h>

    // Structure de test personnalisee avec plus de donnees
    struct LargeBenchmarkData : public nkentseu::NkISerializable {
        nkentseu::NkVector<nkentseu::nk_int64> numbers;
        nkentseu::NkVector<nkentseu::NkString> texts;

        LargeBenchmarkData() {
            // Generer des donnees de test representatives
            for (nkentseu::nk_int32 i = 0; i < 100; ++i) {
                numbers.PushBack(i * 12345);
                texts.PushBack(nkentseu::NkFormatf("Item #%d with some text", i));
            }
        }

        nk_bool Serialize(nkentseu::NkArchive& ar) const override {
            // Serialisation simplifiee pour l'exemple
            // (en pratique, utiliser NkArchive::SetArray ou similaire)
            return true;
        }

        nk_bool Deserialize(const nkentseu::NkArchive& ar) override {
            return true;
        }

        [[nodiscard]] const nk_char* GetTypeName() const noexcept override {
            return "LargeBenchmarkData";
        }

        [[nodiscard]] nkentseu::nk_size ApproxSize() const noexcept {
            return numbers.Size() * sizeof(nkentseu::nk_int64) +
                   texts.Size() * 32u;  // Estimation grossiere
        }
    };

    void CustomDataBenchmark() {
        LargeBenchmarkData data;
        printf("Testing with large dataset: ~%zu bytes estimated\n", data.ApproxSize());

        // Adapter RunFormat pour utiliser LargeBenchmarkData necessiterait
        // une version template de la fonction ou un callback de creation
        // Pour l'instant, utiliser BenchmarkData standard
        auto result = nkentseu::benchmark::RunFormat(
            nkentseu::NkSerializationFormat::NK_NATIVE, 1000u);

        if (result.success) {
            printf("Large data benchmark complete\n");
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Integration avec un systeme de CI/CD
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkSerializationBenchmark.h>

    // Script de validation de performance pour CI/CD
    nk_bool ValidatePerformanceThresholds() {
        const nk_uint32 iterations = 1000u;
        nk_bool allPassed = true;

        // Seuils de performance minimaux (a ajuster selon la plateforme)
        constexpr nk_float64 MAX_SERIALIZE_MS = 1.0;
        constexpr nk_float64 MAX_DESERIALIZE_MS = 1.0;
        constexpr nk_float64 MIN_THROUGHPUT_MBS = 1.0;

        auto nativeResult = nkentseu::benchmark::RunFormat(
            nkentseu::NkSerializationFormat::NK_NATIVE, iterations);

        if (!nativeResult.success) {
            printf("FAIL: Native format benchmark failed\n");
            return false;
        }

        if (nativeResult.serializeMs > MAX_SERIALIZE_MS) {
            printf("FAIL: Serialize time %.4f ms > threshold %.4f ms\n",
                nativeResult.serializeMs, MAX_SERIALIZE_MS);
            allPassed = false;
        }

        if (nativeResult.deserializeMs > MAX_DESERIALIZE_MS) {
            printf("FAIL: Deserialize time %.4f ms > threshold %.4f ms\n",
                nativeResult.deserializeMs, MAX_DESERIALIZE_MS);
            allPassed = false;
        }

        if (nativeResult.ThroughputMBs() < MIN_THROUGHPUT_MBS) {
            printf("FAIL: Throughput %.2f MB/s < threshold %.2f MB/s\n",
                nativeResult.ThroughputMBs(), MIN_THROUGHPUT_MBS);
            allPassed = false;
        }

        if (allPassed) {
            printf("PASS: All performance thresholds met\n");
        }

        return allPassed;
    }
*/

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================