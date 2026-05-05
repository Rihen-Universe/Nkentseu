// =============================================================================
// NKSerialization/YAML/NkYAMLReader.h
// Désérialiseur YAML pour NkArchive — lecture de données structurées depuis YAML.
//
// Design :
//  - Parsing ligne par ligne avec gestion de l'indentation
//  - Support des blocs scalaires, objets (mappings) et tableaux (sequences)
//  - Gestion des quotes single/double et échappement YAML
//  - Détection des marqueurs de document (---, ...)
//  - Export contrôlé via NKENTSEU_SERIALIZATION_CLASS_EXPORT
//  - Message d'erreur optionnel via paramètre NkString* outError
//
// Format YAML supporté :
//  - Block mappings : clé: valeur avec indentation
//  - Block sequences : - élément avec indentation
//  - Scalar values : null (~), booléens, nombres, strings quotés
//  - Commentaires : lignes commençant par # (ignorées)
//
// Limitations connues :
//  - Pas de support des flow styles {key: val} ou [item, item]
//  - Pas de support des multi-line strings (| ou >)
//  - Tableaux imbriqués simplifiés (nested arrays → ignorés ou [] )
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_SERIALIZATION_YAML_NKYAMLREADER_H
#define NKENTSEU_SERIALIZATION_YAML_NKYAMLREADER_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusions des dépendances du module de sérialisation.
    // NkSerializationApi.h fournit les macros d'export requises.

    #include "NKSerialization/NkSerializationApi.h"
    #include "NKSerialization/NkArchive.h"
    #include "NKContainers/String/NkString.h"
    #include "NKContainers/String/NkStringView.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : DÉCLARATION DU NAMESPACE PRINCIPAL
    // -------------------------------------------------------------------------
    // Tous les symboles du module serialization sont dans le namespace nkentseu.

    namespace nkentseu {


        // =============================================================================
        // CLASSE : NkYAMLReader
        // DESCRIPTION : Désérialiseur YAML pour NkArchive
        // =============================================================================
        /**
         * @class NkYAMLReader
         * @brief Désérialiseur YAML pour convertir du texte YAML en NkArchive
         * @ingroup SerializationFormatters
         *
         * Fonctionnalités :
         *  - Parsing ligne par ligne avec suivi d'indentation
         *  - Reconnaissance des mappings (clé: valeur) et sequences (- item)
         *  - Parsing des scalaires : null, booléens, nombres, strings quotés
         *  - Ignorance des commentaires (#) et marqueurs de document (---, ...)
         *  - Message d'erreur détaillé optionnel via paramètre outError
         *
         * Format d'entrée attendu :
         *  @code
         *  ---
         *  player:
         *    name: 'Hero'
         *    level: 42
         *    inventory:
         *      - 'sword'
         *      - 'shield'
         *  debug: true
         *  @endcode
         *
         * Règles de parsing :
         *  - Indentation : 2 espaces par niveau (tolérant aux variations)
         *  - Quotes : single ('...') ou double ("...") pour les strings
         *  - Échappement : '' dans single quotes → ' littéral
         *  - Null : ~ ou null (case-insensitive) → NkArchiveValue::Null()
         *  - Booléens : true/false (case-insensitive) → NkArchiveValue::FromBool()
         *  - Nombres : parsing via ToInt64/ToUInt64/ToDouble avec fallback string
         *
         * Thread-safety :
         *  - Toutes les méthodes sont thread-safe (pas d'état interne mutable)
         *  - L'archive de sortie est fournie par l'appelant
         *
         * @note Ce parser est conçu pour la lisibilité, pas pour la performance extrême.
         *       Pour le streaming de gros fichiers, envisager un parser événementiel futur.
         *
         * @example Lecture basique d'un YAML en archive
         * @code
         * const char* yaml = R"(
         * ---
         * app.name: 'MyGame'
         * app.version: 1
         * debug.enabled: true
         * )";
         *
         * nkentseu::NkArchive config;
         * nkentseu::NkString error;
         * if (nkentseu::NkYAMLReader::ReadArchive(yaml, config, &error)) {
         *     nkentseu::NkString appName;
         *     config.GetString("app.name", appName);
         *     printf("App: %s\n", appName.CStr());
         * } else {
         *     printf("Parse error: %s\n", error.CStr());
         * }
         * @endcode
         */
        class NKENTSEU_SERIALIZATION_CLASS_EXPORT NkYAMLReader {


            // -----------------------------------------------------------------
            // SECTION 3 : MEMBRES PUBLICS
            // -----------------------------------------------------------------
        public:


            // -----------------------------------------------------------------
            // MÉTHODE STATIQUE : DÉSÉRIALISATION YAML
            // -----------------------------------------------------------------
            /**
             * @brief Désérialise du texte YAML dans un NkArchive fourni
             * @param yaml Vue de chaîne contenant le texte YAML à parser
             * @param outArchive Référence vers l'archive de destination (vidée avant parsing)
             * @param outError Pointeur optionnel vers NkString pour message d'erreur détaillé
             * @return true si le parsing a réussi, false en cas d'erreur de syntaxe
             * @ingroup YAMLReaderAPI
             *
             * Comportement :
             *  - outArchive est vidé en début d'opération (Clear() appelé)
             *  - Les marqueurs "---" et "..." sont ignorés (début/fin de document)
             *  - Les commentaires (#) et lignes vides sont sautés
             *  - L'indentation détermine la hiérarchie des objets imbriqués
             *
             * Gestion des erreurs :
             *  - Si outError != nullptr et erreur : message formaté décrivant le problème
             *  - Erreurs courantes : indentation incohérente, clé sans valeur, syntaxe invalide
             *  - En cas d'erreur partielle : outArchive peut contenir des données partielles
             *
             * @note Méthode noexcept-friendly : aucune exception levée, erreurs gérées par retour bool
             * @note Performance : O(n × d) où n = nombre de lignes, d = profondeur max d'indentation
             *
             * @example
             * @code
             * nkentseu::NkString yaml = LoadFile("config.yaml");
             * nkentseu::NkArchive config;
             * nkentseu::NkString errorMsg;
             *
             * if (!nkentseu::NkYAMLReader::ReadArchive(yaml.View(), config, &errorMsg)) {
             *     logger.Error("Failed to parse config: %s\n", errorMsg.CStr());
             *     return false;
             * }
             * // config est maintenant peuplé avec les données du YAML
             * @endcode
             */
            static nk_bool ReadArchive(NkStringView yaml,
                                       NkArchive& outArchive,
                                       NkString* outError = nullptr);


        }; // class NkYAMLReader


    } // namespace nkentseu


#endif // NKENTSEU_SERIALIZATION_YAML_NKYAMLREADER_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKYAMLREADER.H
// =============================================================================
// Ce fichier fournit le désérialiseur YAML pour NkArchive.
// Il combine parsing ligne par ligne, gestion d'indentation et validation de syntaxe.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Lecture basique d'un fichier de configuration
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/YAML/NkYAMLReader.h>

    bool LoadGameConfig(const char* filename, nkentseu::NkArchive& outConfig) {
        // Chargement du fichier en mémoire (pseudo-code)
        nkentseu::NkString yamlContent = File::ReadText(filename);
        if (yamlContent.Empty()) {
            return false;
        }

        // Parsing YAML
        nkentseu::NkString error;
        if (!nkentseu::NkYAMLReader::ReadArchive(yamlContent.View(), outConfig, &error)) {
            logger.Error("YAML parse error in %s: %s\n", filename, error.CStr());
            return false;
        }

        return true;
    }

    // Usage :
    // nkentseu::NkArchive config;
    // if (LoadGameConfig("config.yaml", config)) {
    //     // Utiliser config...
    // }
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Parsing avec validation de schéma basique
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/YAML/NkYAMLReader.h>

    bool LoadAndValidatePlayer(const char* yamlData, nkentseu::NkArchive& outPlayer) {
        nkentseu::NkString error;
        if (!nkentseu::NkYAMLReader::ReadArchive(yamlData, outPlayer, &error)) {
            logger.Error("Parse failed: %s\n", error.CStr());
            return false;
        }

        // Validation basique des champs requis
        if (!outPlayer.Has("username") || !outPlayer.Has("playerId")) {
            logger.Error("Missing required fields in player data\n");
            return false;
        }

        // Validation de type optionnelle
        nkentseu::NkString username;
        if (!outPlayer.GetString("username", username) || username.Empty()) {
            logger.Error("username must be a non-empty string\n");
            return false;
        }

        return true;
    }
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Gestion des erreurs détaillées
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/YAML/NkYAMLReader.h>

    void ParseWithDetailedError(const char* yaml) {
        nkentseu::NkArchive result;
        nkentseu::NkString error;

        if (!nkentseu::NkYAMLReader::ReadArchive(yaml, result, &error)) {
            // Affichage de l'erreur pour debugging
            printf("YAML Error: %s\n", error.CStr());

            // En production : logging structuré ou retour à l'utilisateur
            // logger.Error("Config parse error: %s", error.CStr());
        } else {
            printf("Parsed %zu entries successfully\n", result.Size());
        }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Chargement de structures hiérarchiques complexes
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/YAML/NkYAMLReader.h>

    void LoadComplexStructure() {
        const char* yaml = R"(
        ---
        game:
          title: 'Adventure Quest'
          version: 2.1
          players:
            - name: 'Alice'
              class: 'Warrior'
              level: 42
            - name: 'Bob'
              class: 'Mage'
              level: 38
          settings:
            difficulty: 0.75
            allowMods: true
            maxFps: 144
        )";

        nkentseu::NkArchive data;
        if (nkentseu::NkYAMLReader::ReadArchive(yaml, data)) {
            // Accès aux données imbriquées
            nkentseu::NkArchive game;
            if (data.GetObject("game", game)) {
                nkentseu::NkString title;
                game.GetString("title", title);
                printf("Game: %s v", title.CStr());

                nk_float64 version = 0.0;
                game.GetFloat64("version", version);
                printf("%.1f\n", version);
            }
        }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Intégration avec NkISerializable pour chargement polymorphique
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/YAML/NkYAMLReader.h>
    #include <NKSerialization/NkISerializable.h>

    nkentseu::NkISerializable* LoadSerializableFromYAML(const char* yaml,
                                                        const char* expectedType) {
        // Parsing YAML vers archive intermédiaire
        nkentseu::NkArchive data;
        nkentseu::NkString error;
        if (!nkentseu::NkYAMLReader::ReadArchive(yaml, data, &error)) {
            logger.Error("YAML parse failed: %s\n", error.CStr());
            return nullptr;
        }

        // Création dynamique via registry
        auto* obj = nkentseu::NkSerializableRegistry::Global().Create(expectedType);
        if (!obj) {
            logger.Error("Type '%s' not registered\n", expectedType);
            return nullptr;
        }

        // Désérialisation via l'interface de base
        if (!obj->Deserialize(data)) {
            logger.Error("Deserialize failed for type '%s'\n", expectedType);
            delete obj;  // Cleanup en cas d'échec
            return nullptr;
        }

        return obj;  // Ownership transfer to caller
    }

    // Usage :
    // auto* player = LoadSerializableFromYAML(yamlData, "PlayerData");
    // if (player) { /* utiliser... *\/ delete player; }
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================