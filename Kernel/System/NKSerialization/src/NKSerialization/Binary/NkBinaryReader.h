// =============================================================================
// NKSerialization/Binary/NkBinaryReader.h
// Désérialiseur binaire simple "NKS0" pour archives plates.
//
// Design :
//  - Parsing binaire little-endian avec validation de header
//  - Reconstruction des types scalaires depuis leur représentation textuelle
//  - Gestion tolérante des bytes de padding en fin de fichier (non-fatal)
//  - Export contrôlé via NKENTSEU_SERIALIZATION_CLASS_EXPORT
//  - Message d'erreur optionnel via paramètre NkString* outError
//
// Format attendu (NKS0) :
//   [Header: 12 bytes]
//     - Magic:4 = 'NKS0' (0x304B534E)
//     - Version:2 = 1
//     - Reserved:2 = 0
//     - EntryCount:4 = nombre d'entrées
//   [Entries: variable]
//     Pour chaque entrée :
//       - KeyLen:4 + Key:KeyLen (UTF-8)
//       - Type:1 = NkArchiveValueType
//       - ValueLen:4 + Value:ValueLen (texte brut)
//
// Reconstruction des valeurs :
//  - Le texte est parsé selon le type pour reconstruire l'union raw
//  - Bool : "true" → raw.b=true, autre → raw.b=false
//  - Int64/UInt64/Float64 : parsing via ToInt64/ToUInt64/ToDouble
//  - Null/String : texte utilisé tel quel
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_SERIALIZATION_BINARY_NKBINARYREADER_H
#define NKENTSEU_SERIALIZATION_BINARY_NKBINARYREADER_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusions des dépendances du module de sérialisation.
    // NkSerializationApi.h fournit les macros d'export requises.

    #include "NKSerialization/NkSerializationApi.h"
    #include "NKSerialization/NkArchive.h"
    #include "NKContainers/String/NkString.h"
    #include "NKCore/NkTypes.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : DÉCLARATION DU NAMESPACE PRINCIPAL
    // -------------------------------------------------------------------------
    // Tous les symboles du module serialization sont dans le namespace nkentseu.

    namespace nkentseu {


        // =============================================================================
        // CLASSE : NkBinaryReader
        // DESCRIPTION : Désérialiseur binaire simple pour archives plates NKS0
        // =============================================================================
        /**
         * @class NkBinaryReader
         * @brief Désérialiseur pour le format binaire plat "NKS0"
         * @ingroup SerializationFormatters
         *
         * Fonctionnalités :
         *  - Parsing binaire little-endian avec bounds-checking à chaque lecture
         *  - Validation du magic number et de la version pour détection de format
         *  - Reconstruction des types scalaires depuis leur représentation textuelle
         *  - Gestion tolérante des bytes de padding en fin de fichier (non-fatal)
         *  - Message d'erreur détaillé optionnel via paramètre outError
         *
         * Reconstruction des valeurs :
         *  - Bool : parsing de "true"/"false" (case-sensitive)
         *  - Int64 : parsing décimal via NkString::ToInt64()
         *  - UInt64 : parsing décimal via NkString::ToUInt64()
         *  - Float64 : parsing via NkString::ToDouble()
         *  - Null/String : texte utilisé tel quel, raw non-modifié
         *
         * Gestion des erreurs :
         *  - Bounds-checking : retour false si dépassement de buffer
         *  - Magic/version invalide : erreur explicite avec code hexadécimal
         *  - Entrée corrompue : message avec index de l'entrée fautive
         *  - Bytes trailing : avertissement non-fatal (certains writers ajoutent du padding)
         *
         * Thread-safety :
         *  - Toutes les méthodes sont thread-safe (pas d'état interne mutable)
         *  - Les buffers d'entrée sont en lecture seule — pas de modification
         *
         * @note Ce lecteur est conçu pour la compatibilité avec NkBinaryWriter (NKS0).
         *       Pour lire le format binaire récursif NKS1, utiliser NkNativeReader.
         *
         * @example Lecture basique d'un binaire NKS0 avec gestion d'erreur
         * @code
         * // Chargement du fichier binaire en mémoire (pseudo-code)
         * nkentseu::NkVector<nk_uint8> fileData = File::ReadBinary("config.nks0");
         *
         * nkentseu::NkArchive result;
         * nkentseu::NkString error;
         * if (nkentseu::NkBinaryReader::ReadArchive(
         *         fileData.Data(), fileData.Size(), result, &error)) {
         *     // result contient les données désérialisées
         *     nk_int32 port = 0;
         *     result.GetInt32("server.port", port);
         *     printf("Port: %d\n", port);
         * } else {
         *     printf("Load failed: %s\n", error.CStr());
         * }
         * @endcode
         */
        class NKENTSEU_SERIALIZATION_CLASS_EXPORT NkBinaryReader {


            // -----------------------------------------------------------------
            // SECTION 3 : MEMBRES PUBLICS
            // -----------------------------------------------------------------
        public:


            // -----------------------------------------------------------------
            // MÉTHODE STATIQUE : DÉSÉRIALISATION BINAIRE NKS0
            // -----------------------------------------------------------------
            /**
             * @brief Désérialise un buffer binaire NKS0 dans un NkArchive
             * @param data Pointeur const vers le buffer binaire source
             * @param size Taille du buffer en bytes
             * @param outArchive Archive de destination pour les données désérialisées
             * @param outError Pointeur optionnel vers NkString pour message d'erreur détaillé
             * @return true si la désérialisation a réussi, false en cas d'erreur
             * @ingroup BinaryReaderAPI
             * @note noexcept-friendly : aucune exception levée, erreurs gérées par retour bool
             *
             * Étapes de désérialisation :
             *  1. Validation minimale : lecture du header (12 bytes minimum)
             *  2. Lecture et validation : magic == NK_BINARY_MAGIC, version == 1
             *  3. Pour chaque entrée (count fois) :
             *     a. Lecture de keyLen(4), type(1), valueLen(4)
             *     b. Bounds-checking : vérification que keyLen+valueLen <= données restantes
             *     c. Construction de la clé NkString et de la valeur NkArchiveValue
             *     d. Reconstruction de l'union raw via ReconstructRaw()
             *     e. Insertion dans outArchive via SetValue()
             *  4. Gestion des bytes trailing : avertissement non-fatal si outError fourni
             *  5. Retour du résultat
             *
             * @note En cas d'erreur partielle : outArchive peut contenir des données partielles.
             *       Toujours vérifier le retour bool avant d'utiliser le résultat.
             * @note Les objets/tableaux stockés en JSON texte ne sont pas re-parsés automatiquement.
             *       L'appelant doit parser manuellement le texte si nécessaire.
             *
             * @example Voir l'exemple dans la documentation de la classe.
             */
            static nk_bool ReadArchive(const nk_uint8* data,
                                       nk_size size,
                                       NkArchive& outArchive,
                                       NkString* outError = nullptr);


        }; // class NkBinaryReader


    } // namespace nkentseu


#endif // NKENTSEU_SERIALIZATION_BINARY_NKBINARYREADER_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKBINARYREADER.H
// =============================================================================
// Ce fichier fournit le désérialiseur binaire plat NKS0 pour NkArchive.
// Il combine parsing binaire robuste, reconstruction de types et gestion d'erreurs.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Lecture basique d'un fichier de configuration
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/Binary/NkBinaryReader.h>

    bool LoadSimpleConfig(const char* filename, nkentseu::NkArchive& outConfig) {
        // Chargement du fichier en mémoire (pseudo-code)
        nkentseu::NkVector<nk_uint8> fileData = File::ReadBinary(filename);
        if (fileData.Empty()) {
            return false;
        }

        // Parsing binaire NKS0
        nkentseu::NkString error;
        if (!nkentseu::NkBinaryReader::ReadArchive(fileData.Data(), fileData.Size(), outConfig, &error)) {
            printf("Binary parse error in %s: %s\n", filename, error.CStr());
            return false;
        }

        return true;
    }

    // Usage :
    // nkentseu::NkArchive config;
    // if (LoadSimpleConfig("config.nks0", config)) {
    //     // Utiliser config...
    // }
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Gestion des erreurs détaillées
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/Binary/NkBinaryReader.h>

    void ParseWithDetailedError(const nk_uint8* data, nk_size size) {
        nkentseu::NkArchive result;
        nkentseu::NkString error;

        if (!nkentseu::NkBinaryReader::ReadArchive(data, size, result, &error)) {
            // Affichage de l'erreur pour debugging
            printf("Binary Error: %s\n", error.CStr());

            // Analyse du type d'erreur pour récupération
            if (error.StartsWith("Invalid binary magic")) {
                printf("  → Wrong file format (expected NKS0)\n");
            } else if (error.StartsWith("Unsupported binary version")) {
                printf("  → File created with incompatible version\n");
            } else if (error.StartsWith("Corrupted entry")) {
                printf("  → Data corruption detected in specific entry\n");
            }
        } else {
            printf("Parsed %zu entries successfully\n", result.Size());
        }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Reconstruction manuelle des objets JSON stockés en texte
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/Binary/NkBinaryReader.h>
    #include <NKSerialization/JSON/NkJSONReader.h>

    void LoadWithNestedReconstruction() {
        nkentseu::NkVector<nk_uint8> fileData = File::ReadBinary("data.nks0");
        nkentseu::NkArchive flat;
        nkentseu::NkBinaryReader::ReadArchive(fileData.Data(), fileData.Size(), flat);

        // Les objets/tableaux sont stockés en texte JSON — reconstruction manuelle si besoin
        nkentseu::NkString statsJson;
        if (flat.GetString("player.stats", statsJson)) {
            // Parser le JSON pour reconstruire l'objet imbriqué
            nkentseu::NkArchive stats;
            nkentseu::NkString parseError;
            if (nkentseu::NkJSONReader::ReadArchive(statsJson.View(), stats, &parseError)) {
                // stats contient maintenant l'objet reconstruit
                nk_int32 level = 0;
                stats.GetInt32("level", level);
                printf("Player level: %d\n", level);
            } else {
                printf("Failed to parse nested JSON: %s\n", parseError.CStr());
            }
        }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Intégration avec NkISerializable pour chargement polymorphique
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/Binary/NkBinaryReader.h>
    #include <NKSerialization/NkISerializable.h>

    nkentseu::NkISerializable* LoadSerializableFromBinary(const nk_uint8* data,
                                                          nk_size size,
                                                          const char* expectedType) {
        // Parsing binaire NKS0 vers archive intermédiaire
        nkentseu::NkArchive flat;
        nkentseu::NkString error;
        if (!nkentseu::NkBinaryReader::ReadArchive(data, size, flat, &error)) {
            printf("Binary parse failed: %s\n", error.CStr());
            return nullptr;
        }

        // Création dynamique via registry
        auto* obj = nkentseu::NkSerializableRegistry::Global().Create(expectedType);
        if (!obj) {
            printf("Type '%s' not registered\n", expectedType);
            return nullptr;
        }

        // Désérialisation via l'interface de base
        if (!obj->Deserialize(flat)) {
            printf("Deserialize failed for type '%s'\n", expectedType);
            delete obj;  // Cleanup en cas d'échec
            return nullptr;
        }

        return obj;  // Ownership transfer to caller
    }

    // Usage :
    // auto* player = LoadSerializableFromBinary(buffer.Data(), buffer.Size(), "PlayerData");
    // if (player) { /* utiliser... *\/ delete player; }
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Gestion tolérante des bytes de padding
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/Binary/NkBinaryReader.h>

    void LoadWithPaddingTolerance() {
        // Certains systèmes ajoutent du padding (alignement secteur, etc.)
        nkentseu::NkVector<nk_uint8> paddedData = File::ReadBinary("padded.nks0");

        nkentseu::NkArchive result;
        nkentseu::NkString warning;  // Utilisé pour les avertissements non-fataux

        // Le lecteur accepte les bytes trailing sans échec
        if (nkentseu::NkBinaryReader::ReadArchive(paddedData.Data(), paddedData.Size(), result, &warning)) {
            if (!warning.Empty()) {
                // warning contient un message sur les bytes trailing (optionnel)
                printf("Note: %s\n", warning.CStr());
            }
            // result contient les données valides malgré le padding
        }
    }
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================