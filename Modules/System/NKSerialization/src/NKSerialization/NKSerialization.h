// =============================================================================
// FICHIER  : Modules/System/NKSerialization/include/NKSerialization/NKSerialization.h
// MODULE   : NKSerialization
// AUTEUR   : Rihen
// DATE     : 2026-02-07
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Point d'entree unique pour l'ensemble du module NKSerialization.
//   Ce fichier inclut tous les en-tetes publics du module et fournit
//   un acces unifie a toutes les fonctionnalites de serialisation.
//
// USAGE RECOMMANDE :
//   #include <NKSerialization/NKSerialization.h>
//
//   // Toutes les fonctionnalites sont alors disponibles :
//   // - NkArchive, NkISerializable, NkSerializer
//   // - Formats : JSON, XML, YAML, Binary, Native
//   // - Benchmarking et utilitaires de performance
//
// DEPENDANCES :
//   - NKCore (types de base, assertions)
//   - NKFileSystem (NkFile pour les operations fichier)
//   - NKContainers (NkString, NkVector, NkFormat)
//   - NKReflection (optionnel, pour la serialisation reflexive)
// =============================================================================

#pragma once

#ifndef NKENTSEU_SERIALIZATION_NKSERIALIZATION_H
#define NKENTSEU_SERIALIZATION_NKSERIALIZATION_H

    // -------------------------------------------------------------------------
    // SECTION 1 : CONFIGURATION DU MODULE
    // -------------------------------------------------------------------------
    // Inclusion de l'en-tete d'API pour les macros d'export/import si necessaire.
    // Doit etre inclus en premier pour garantir la bonne visibilite des symboles.

    // Note : NKSerialization utilise principalement des templates inline,
    // donc les macros d'export sont moins critiques que pour d'autres modules.
    // Inclure si votre configuration de build le requiert :
    // #include "NKSerialization/NkSerializationApi.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : COMPOSANTS FONDAMENTAUX
    // -------------------------------------------------------------------------
    // Inclusion des composants de base requis pour toute utilisation du module.

    // Archive : Structure centrale pour la representation des donnees serialisees
    #include "NKSerialization/NkArchive.h"

    // Interface : Base pour tous les objets serialisables
    #include "NKSerialization/NkISerializable.h"

    // Versioning : Support de la compatibilite ascendante/descendante des schemas
    #include "NKSerialization/NkSchemaVersioning.h"

    // -------------------------------------------------------------------------
    // SECTION 3 : POINT D'ENTREE UNIFIE
    // -------------------------------------------------------------------------
    // Inclusion de l'API principale de serialisation/deserialisation.

    #include "NKSerialization/NkSerializer.h"

    // -------------------------------------------------------------------------
    // SECTION 4 : FORMATS TEXTE
    // -------------------------------------------------------------------------
    // Inclusion des lecteurs et ecrivains pour les formats de serialisation texte.

    // JSON : JavaScript Object Notation - lisible, interopérable, largement supporte
    #include "NKSerialization/JSON/NkJSONReader.h"
    #include "NKSerialization/JSON/NkJSONWriter.h"
    #include "NKSerialization/JSON/NkJSONValue.h"

    // XML : eXtensible Markup Language - structure hierarchique explicite
    #include "NKSerialization/XML/NkXMLReader.h"
    #include "NKSerialization/XML/NkXMLWriter.h"

    // YAML : YAML Ain't Markup Language - syntaxe concise, sensible a l'indentation
    #include "NKSerialization/YAML/NkYAMLReader.h"
    #include "NKSerialization/YAML/NkYAMLWriter.h"

    // -------------------------------------------------------------------------
    // SECTION 5 : FORMATS BINAIRES
    // -------------------------------------------------------------------------
    // Inclusion des lecteurs et ecrivains pour les formats de serialisation binaire.

    // Binary : Format binaire generique - compact, rapide, non lisible
    #include "NKSerialization/Binary/NkBinaryReader.h"
    #include "NKSerialization/Binary/NkBinaryWriter.h"

    // Native : Format binaire optimise pour l'ecosysteme NKEntseu
    #include "NKSerialization/Native/NkNativeFormat.h"

    // -------------------------------------------------------------------------
    // SECTION 6 : REFLEXION ET SERIALISATION DYNAMIQUE
    // -------------------------------------------------------------------------
    // Inclusion des composants pour la serialisation basee sur la reflexion.

    // Reflect : Integration avec NKReflection pour la serialisation automatique
    #include "NKSerialization/Native/NkReflect.h"

    // -------------------------------------------------------------------------
    // SECTION 7 : UTILITAIRES ET BENCHMARKING
    // -------------------------------------------------------------------------
    // Inclusion optionnelle des utilitaires de mesure de performance.

    // Benchmark : Outils de benchmark comparatif des formats de serialisation
    // Note : Inclure uniquement si necessaire pour eviter la dependance a <chrono>
    // #include "NKSerialization/NkSerializationBenchmark.h"

    // -------------------------------------------------------------------------
    // SECTION 8 : ESPACE DE NOMS DE CONVENANCE
    // -------------------------------------------------------------------------
    // Alias de namespace pour simplifier l'usage du module dans le code client.

    namespace nkentseu {

        /**
         * @namespace serial
         * @brief Alias de convenance pour nkentseu::serialization
         *
         * Permet d'abbrevier les references au namespace de serialisation
         * dans le code client pour plus de concision.
         *
         * @note Cet alias est optionnel : vous pouvez utiliser le namespace
         *       complet nkentseu::serialization si vous preferez l'explicite
         *
         * @example
         * @code
         * using namespace nkentseu::serial;
         * auto archive = NkArchive();
         * archive.SetInt("score", 42);
         * @endcode
         */
        namespace serial = serialization;

    } // namespace nkentseu

#endif // NKENTSEU_SERIALIZATION_NKSERIALIZATION_H

// =============================================================================
// EXEMPLES D'UTILISATION - NKSerialization.h (Point d'entree unique)
// =============================================================================
//
// -----------------------------------------------------------------------------
// Exemple 1 : Inclusion unique et serialisation basique
// -----------------------------------------------------------------------------
/*
    // Dans n'importe quel fichier de votre projet :
    #include <NKSerialization/NKSerialization.h>

    // Toutes les fonctionnalites du module sont maintenant disponibles

    namespace game {

        class Player : public nkentseu::NkISerializable {
        public:
            nkentseu::nk_int32 health = 100;
            nkentseu::nk_float32 experience = 0.0f;

            nk_bool Serialize(nkentseu::NkArchive& ar) override {
                ar.SetInt64("health", health);
                ar.SetFloat64("experience", experience);
                return true;
            }

            nk_bool Deserialize(const nkentseu::NkArchive& ar) override {
                return ar.GetInt64("health", health) &&
                       ar.GetFloat64("experience", experience);
            }
        };

    } // namespace game

    void SavePlayerExample() {
        game::Player player;
        player.health = 85;
        player.experience = 1250.5f;

        // Serialisation en JSON avec formatage lisible
        nkentseu::NkString jsonOutput;
        if (nkentseu::Serialize(
                player,
                nkentseu::NkSerializationFormat::NK_JSON,
                jsonOutput,
                true)) {
            printf("Serialized: %s\n", jsonOutput.CStr());
        }

        // Sauvegarde directe dans un fichier
        if (nkentseu::SaveToFile(player, "saves/player.json")) {
            printf("Player saved successfully\n");
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Chargement avec detection automatique du format
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NKSerialization.h>

    void LoadPlayerExample() {
        game::Player player;  // Doit avoir un constructeur par defaut

        // Detection automatique du format via l'extension du fichier
        // .json → JSON, .xml → XML, .bin → Binary, .nk → Native, etc.
        if (nkentseu::LoadFromFile(
                "saves/player.json",
                player,
                nkentseu::NkSerializationFormat::NK_AUTO_DETECT)) {
            printf("Loaded player: health=%d, experience=%.2f\n",
                player.health, player.experience);
        } else {
            printf("Failed to load player\n");
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Serialisation binaire pour les performances
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NKSerialization.h>

    void BinarySaveExample() {
        game::Player player;
        player.health = 50;
        player.experience = 500.0f;

        // Serialisation en format natif NK (binaire optimise)
        nkentseu::NkVector<nkentseu::nk_uint8> binaryData;
        if (nkentseu::SerializeBinary(
                player,
                nkentseu::NkSerializationFormat::NK_NATIVE,
                binaryData)) {
            printf("Serialized to %zu bytes (binary format)\n", binaryData.Size());

            // Ecriture manuelle si besoin de controle fin sur le fichier
            nkentseu::NkFile file;
            if (file.Open("saves/quick.nk", nkentseu::NkFileMode::NK_WRITE_BINARY)) {
                file.Write(binaryData.Data(), binaryData.Size());
                file.Close();
            }
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Utilisation de l'alias de namespace de convenance
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NKSerialization.h>

    // Optionnel : utiliser l'alias pour plus de concision
    namespace serial = nkentseu::serial;

    void ConciseUsageExample() {
        // Creation et manipulation d'une archive
        serial::NkArchive archive;
        archive.SetInt64("score", 42);
        archive.SetString("player", "Alice");
        archive.SetBool("active", true);

        // Serialisation en YAML
        serial::NkString yamlOutput;
        if (serial::SerializeArchiveText(
                archive,
                serial::NkSerializationFormat::NK_YAML,
                yamlOutput)) {
            printf("YAML output:\n%s\n", yamlOutput.CStr());
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Benchmarking des formats (inclure le header benchmark si besoin)
// -----------------------------------------------------------------------------
/*
    // Note : De-commenter l'inclusion du benchmark dans NKSerialization.h si utilise
    // #include "NKSerialization/NkSerializationBenchmark.h"

    #include <NKSerialization/NKSerialization.h>
    // #include <NKSerialization/NkSerializationBenchmark.h>

    void PerformanceComparisonExample() {
        printf("Running serialization benchmarks...\n");

        // Lancer tous les benchmarks avec 1000 iterations
        // nkentseu::benchmark::RunAllBenchmarks(1000u);

        // Ou tester un format specifique
        /\*
        auto result = nkentseu::benchmark::RunFormat(
            nkentseu::NkSerializationFormat::NK_NATIVE,
            5000u  // Plus d'iterations pour des mesures plus stables
        );

        if (result.success) {
            printf("Native format results:\n");
            printf("  Original size: %zu bytes\n", result.originalSize);
            printf("  Serialized size: %zu bytes\n", result.serializedSize);
            printf("  Compression ratio: %.2fx\n", result.compressionRatio);
            printf("  Serialize time: %.4f ms/iter\n", result.serializeMs);
            printf("  Deserialize time: %.4f ms/iter\n", result.deserializeMs);
            printf("  Throughput: %.2f MB/s\n", result.ThroughputMBs());
        }
        *\/
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Gestion d'erreurs detaillee
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NKSerialization.h>
    #include <NKCore/Log/NkLog.h>

    nk_bool SafeSavePlayer(const game::Player& player, const nk_char* path) {
        nkentseu::NkString error;

        if (!nkentseu::SaveToFile(
                player,
                path,
                nkentseu::NkSerializationFormat::NK_AUTO_DETECT,
                true,   // pretty-print pour les formats texte
                &error  // Capture des messages d'erreur
        )) {
            NK_FOUNDATION_LOG_ERROR("Save failed: %s", error.CStr());
            return false;
        }

        NK_FOUNDATION_LOG_INFO("Player saved to %s", path);
        return true;
    }

    nk_bool SafeLoadPlayer(const nk_char* path, game::Player& outPlayer) {
        nkentseu::NkString error;

        if (!nkentseu::LoadFromFile(
                path,
                outPlayer,
                nkentseu::NkSerializationFormat::NK_AUTO_DETECT,
                &error
        )) {
            NK_FOUNDATION_LOG_ERROR("Load failed: %s", error.CStr());
            return false;
        }

        NK_FOUNDATION_LOG_INFO("Player loaded from %s", path);
        return true;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Factory polymorphique avec serialisation reflexive
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NKSerialization.h>

    // Enregistrement des types pour la factory (dans un fichier .cpp)
    namespace {
        struct PlayerRegistrar {
            PlayerRegistrar() {
                nkentseu::NkSerializableRegistry::Global()
                    .Register<game::Player>("Player");
            }
        };
        static PlayerRegistrar g_playerRegistrar;
    }

    // Chargement polymorphique d'un objet depuis un fichier
    nkentseu::std::unique_ptr<nkentseu::NkISerializable>
    LoadAnyEntity(const nk_char* path) {
        // Lecture de l'archive depuis le fichier
        nkentseu::NkArchive archive;
        if (!nkentseu::LoadFromFile(path, archive,
                nkentseu::NkSerializationFormat::NK_AUTO_DETECT)) {
            return nullptr;
        }

        // Creation polymorphique via le champ "__type__" dans l'archive
        return nkentseu::CreateFromArchive(archive);
    }

    void PolymorphicLoadExample() {
        auto entity = LoadAnyEntity("saves/entity.nk");
        if (entity) {
            // L'objet est du type enregistre sous "__type__" dans le fichier
            // Utilisation via l'interface NkISerializable
            nkentseu::NkArchive debugArchive;
            entity->Serialize(debugArchive);  // Pour inspection ou re-serialisation

            // Ou cast vers le type concret si connu
            game::Player* player = dynamic_cast<game::Player*>(entity.get());
            if (player) {
                printf("Loaded player with health: %d\n", player->health);
            }
        }
    }
*/

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================