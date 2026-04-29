// =============================================================================
// NKSerialization/Binary/NkBinaryWriter.h
// Sérialiseur binaire simple "NKS0" pour archives plates.
//
// Design :
//  - Format binaire compact pour données plates (pas de récursion native)
//  - Objets/tableaux imbriqués sérialisés en texte JSON pour compatibilité
//  - Header versionné avec magic number "NKS0" pour détection rapide
//  - Export contrôlé via NKENTSEU_SERIALIZATION_CLASS_EXPORT
//  - Aucune dépendance STL : tout via NkVector, NkString, types natifs
//
// Structure binaire NKS0 :
//   [Header: 12 bytes]
//     - Magic:4 = 'NKS0' (0x304B534E little-endian)
//     - Version:2 = 1 (format plat)
//     - Reserved:2 = 0 (pour extensions futures)
//     - EntryCount:4 = nombre d'entrées dans l'archive
//   [Entries: variable]
//     Pour chaque entrée :
//       - KeyLen:4 + Key:KeyLen (UTF-8)
//       - Type:1 = NkArchiveValueType (ou STRING pour objets/tableaux)
//       - ValueLen:4 + Value:ValueLen (texte brut ou JSON pour complexes)
//
// Limitations connues :
//  - Format plat uniquement : pas de récursion binaire native
//  - Objets/tableaux → sérialisés en JSON texte (moins compact, plus lisible)
//  - Pour une sérialisation binaire récursive complète : utiliser NkNativeFormat (NKS1)
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_SERIALIZATION_BINARY_NKBINARYWRITER_H
#define NKENTSEU_SERIALIZATION_BINARY_NKBINARYWRITER_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusions des dépendances du module de sérialisation.
    // NkSerializationApi.h fournit les macros d'export requises.

    #include "NKSerialization/NkSerializationApi.h"
    #include "NKSerialization/NkArchive.h"
    #include "NKContainers/Sequential/NkVector.h"
    #include "NKCore/NkTypes.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : DÉCLARATION DU NAMESPACE PRINCIPAL
    // -------------------------------------------------------------------------
    // Tous les symboles du module serialization sont dans le namespace nkentseu.

    namespace nkentseu {


        // =============================================================================
        // CLASSE : NkBinaryWriter
        // DESCRIPTION : Sérialiseur binaire simple pour archives plates NKS0
        // =============================================================================
        /**
         * @class NkBinaryWriter
         * @brief Sérialiseur pour le format binaire plat "NKS0"
         * @ingroup SerializationFormatters
         *
         * Fonctionnalités :
         *  - Écriture binaire compacte pour données plates (clé/valeur simples)
         *  - Gestion automatique de la sérialisation JSON pour objets/tableaux imbriqués
         *  - Header versionné avec magic number pour validation rapide
         *  - Format little-endian pour portabilité multi-plateforme
         *
         * Cas d'usage recommandés :
         *  - Configuration simple sans imbrication complexe
         *  - Échange de données avec systèmes legacy supportant NKS0
         *  - Stockage temporaire où la compacité prime sur la flexibilité
         *
         * Cas d'usage non recommandés :
         *  - Structures hiérarchiques complexes → préférer NkNativeFormat (NKS1)
         *  - Interopérabilité humaine → préférer YAML/JSON/XML
         *  - Données avec intégrité critique → préférer NkNativeFormat avec CRC32
         *
         * Thread-safety :
         *  - Toutes les méthodes sont thread-safe (pas d'état interne mutable)
         *  - Le buffer de sortie NkVector<nk_uint8> est géré par l'appelant
         *
         * @note Ce format est conçu pour la simplicité et la rétro-compatibilité.
         *       Pour de nouvelles implémentations, préférer NkNativeFormat (NKS1).
         *
         * @example Écriture basique d'une archive plate en binaire NKS0
         * @code
         * nkentseu::NkArchive config;
         * config.SetInt32("port", 8080);
         * config.SetString("host", "localhost");
         * config.SetBool("debug", true);
         *
         * nkentseu::NkVector<nk_uint8> buffer;
         * if (nkentseu::NkBinaryWriter::WriteArchive(config, buffer)) {
         *     // buffer contient le format binaire NKS0
         *     // Sauvegarde : File::WriteBinary("config.nks0", buffer.Data(), buffer.Size());
         * }
         * @endcode
         */
        class NKENTSEU_SERIALIZATION_CLASS_EXPORT NkBinaryWriter {


            // -----------------------------------------------------------------
            // SECTION 3 : MEMBRES PUBLICS
            // -----------------------------------------------------------------
        public:


            // -----------------------------------------------------------------
            // MÉTHODE STATIQUE : SÉRIALISATION BINAIRE NKS0
            // -----------------------------------------------------------------
            /**
             * @brief Sérialise un NkArchive en format binaire plat NKS0
             * @param archive Archive source à sérialiser (doit être plate pour résultats optimaux)
             * @param outData Référence vers NkVector<nk_uint8> pour recevoir le payload binaire
             * @return true si la sérialisation a réussi, false en cas d'erreur
             * @ingroup BinaryWriterAPI
             * @note noexcept-friendly : aucune exception levée, erreurs gérées par retour bool
             *
             * Comportement :
             *  - outData est vidé en début d'opération (Clear() appelé)
             *  - Pré-allocation via Reserve() pour éviter les réallocations multiples
             *  - Écriture du header : magic(4) + version(2) + reserved(2) + count(4)
             *  - Pour chaque entrée : keyLen(4) + key + type(1) + valueLen(4) + value
             *  - Les nœuds Object/Array sont convertis en texte JSON via NkJSONWriter
             *
             * Format des valeurs :
             *  - Scalaires : texte canonique stocké tel quel (ex: "42", "true", "3.14")
             *  - Null : valueLen=0, type=NK_VALUE_NULL
             *  - Object : texte JSON du sous-archive (ex: "{\"a\":1,\"b\":2}")
             *  - Array : texte JSON du tableau (ex: "[1,2,3]")
             *
             * @note Performance : O(n × m) où n = nombre d'entrées, m = taille moyenne des valeurs JSON
             * @note Mémoire : pré-allocation estimée à 16 + entries×32 bytes pour réduire les realloc
             *
             * @example
             * @code
             * nkentseu::NkArchive data;
             * data.SetInt32("count", 42);
             * data.SetString("name", "test");
             *
             * nkentseu::NkVector<nk_uint8> output;
             * if (nkentseu::NkBinaryWriter::WriteArchive(data, output)) {
             *     // output contient : [NKS0 header][entry: count=42][entry: name="test"]
             *     printf("Serialized %zu bytes\n", output.Size());
             * }
             * @endcode
             */
            static nk_bool WriteArchive(const NkArchive& archive,
                                        NkVector<nk_uint8>& outData);


        }; // class NkBinaryWriter


    } // namespace nkentseu


#endif // NKENTSEU_SERIALIZATION_BINARY_NKBINARYWRITER_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKBINARYWRITER.H
// =============================================================================
// Ce fichier fournit le sérialiseur binaire plat NKS0 pour NkArchive.
// Il combine écriture binaire compacte et fallback JSON pour compatibilité.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Sérialisation basique d'une configuration plate
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/Binary/NkBinaryWriter.h>

    void SaveSimpleConfig() {
        nkentseu::NkArchive config;
        config.SetInt32("server.port", 8080);
        config.SetString("server.host", "localhost");
        config.SetBool("debug.enabled", true);
        config.SetFloat32("timeout.seconds", 30.5f);

        nkentseu::NkVector<nk_uint8> buffer;
        if (nkentseu::NkBinaryWriter::WriteArchive(config, buffer)) {
            // Sauvegarde dans un fichier (pseudo-code)
            // File::WriteBinary("config.nks0", buffer.Data(), buffer.Size());
            printf("Saved %zu bytes\n", buffer.Size());
        }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Gestion des valeurs imbriquées (fallback JSON)
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/Binary/NkBinaryWriter.h>

    void SaveWithNestedData() {
        nkentseu::NkArchive data;

        // Valeurs scalaires simples → stockées en binaire compact
        data.SetInt32("id", 12345);
        data.SetString("name", "Player1");

        // Objet imbriqué → sérialisé en texte JSON dans le champ value
        nkentseu::NkArchive stats;
        stats.SetInt32("level", 42);
        stats.SetFloat32("health", 100.f);
        data.SetObject("player.stats", stats);
        // Dans le binaire : value = "{\"level\":42,\"health\":100.0}"

        // Tableau → sérialisé en texte JSON
        nkentseu::NkVector<nkentseu::NkArchiveValue> items;
        items.PushBack(nkentseu::NkArchiveValue::FromString("sword"));
        items.PushBack(nkentseu::NkArchiveValue::FromString("shield"));
        data.SetArray("inventory", items);
        // Dans le binaire : value = "[\"sword\",\"shield\"]"

        nkentseu::NkVector<nk_uint8> buffer;
        nkentseu::NkBinaryWriter::WriteArchive(data, buffer);
        // Note : les champs imbriqués occupent plus de place en JSON texte
        // Pour une compacité optimale avec imbrication : utiliser NkNativeFormat
    }
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Écriture itérative avec réutilisation de buffer
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/Binary/NkBinaryWriter.h>

    void BatchExport(const nkentseu::NkVector<nkentseu::NkArchive>& archives,
                     const char* outputDir) {
        // Réutiliser le même buffer pour éviter les allocations répétées
        nkentseu::NkVector<nk_uint8> buffer;

        for (nk_size i = 0; i < archives.Size(); ++i) {
            buffer.Clear();  // Réinitialisation explicite (WriteArchive le fait aussi)

            if (nkentseu::NkBinaryWriter::WriteArchive(archives[i], buffer)) {
                // Construction du nom de fichier
                nkentseu::NkString filename = outputDir;
                filename.Append("/export_");
                filename.Append(nkentseu::NkString::Fmtf("%zu.nks0", i));

                // Sauvegarde (pseudo-code)
                // File::WriteBinary(filename.CStr(), buffer.Data(), buffer.Size());
            }
        }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Intégration avec NkISerializable pour objets polymorphiques
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/Binary/NkBinaryWriter.h>
    #include <NKSerialization/NkISerializable.h>

    bool SaveSerializableBinary(const nkentseu::NkISerializable* obj, const char* filename) {
        if (!obj) {
            return false;
        }

        // Sérialisation via l'interface de base vers NkArchive intermédiaire
        nkentseu::NkArchive data;
        if (!obj->Serialize(data)) {
            return false;
        }

        // Conversion en binaire NKS0
        nkentseu::NkVector<nk_uint8> buffer;
        if (!nkentseu::NkBinaryWriter::WriteArchive(data, buffer)) {
            return false;
        }

        // Sauvegarde dans un fichier (pseudo-code)
        // return File::WriteBinary(filename, buffer.Data(), buffer.Size());
        return true;
    }

    // Usage :
    // PlayerData player;
    // SaveSerializableBinary(&player, "player.nks0");
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Comparaison NKS0 vs NKS1 pour choix de format
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/Binary/NkBinaryWriter.h>
    #include <NKSerialization/Native/NkNativeFormat.h>

    void ChooseFormatExample() {
        nkentseu::NkArchive data;
        data.SetInt32("simple", 42);  // Scalaire simple

        // Pour données plates : NKS0 est suffisant et légèrement plus rapide
        nkentseu::NkVector<nk_uint8> nks0Buf;
        nkentseu::NkBinaryWriter::WriteArchive(data, nks0Buf);

        // Pour données imbriquées : NKS1 (NkNativeFormat) est plus compact
        nkentseu::NkArchive nested;
        nkentseu::NkArchive child;
        child.SetInt32("x", 1);
        nested.SetObject("nested", child);

        nkentseu::NkVector<nk_uint8> nks1Buf;
        nkentseu::native::NkNativeWriter::WriteArchive(nested, nks1Buf);
        // NKS1 stocke l'objet en binaire récursif, pas en JSON texte

        // Règle de décision :
        // - Données plates + compatibilité legacy → NKS0 (NkBinaryWriter)
        // - Données imbriquées + performance → NKS1 (NkNativeFormat)
        // - Intégrité critique → NKS1 avec flag CHECKSUM
    }
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================