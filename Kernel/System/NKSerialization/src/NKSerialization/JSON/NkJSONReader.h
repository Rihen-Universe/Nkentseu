// =============================================================================
// NKSerialization/JSON/NkJSONReader.h
// Désérialiseur JSON pour NkArchive — lecture de données structurées depuis JSON.
//
// Design :
//  - Parser récursif avec gestion des objets {} et tableaux [] imbriqués
//  - Parsing des scalaires : null, booléens, nombres, strings échappés
//  - Gestion robuste des whitespace et erreurs de syntaxe JSON
//  - Export contrôlé via NKENTSEU_SERIALIZATION_CLASS_EXPORT
//  - Message d'erreur optionnel via paramètre NkString* outError
//
// Format JSON supporté (RFC 8259 subset) :
//  - Objet racine obligatoire : { "key": value, ... }
//  - Valeurs scalaires : null, true/false, nombres, "strings"
//  - Objets imbriqués : { "nested": { "key": "value" } }
//  - Tableaux : [ "item1", 42, { "obj": true } ]
//  - Échappements : \", \\, \/, \b, \f, \n, \r, \t, \uXXXX
//
// Limitations connues :
//  - Pas de support des nombres en notation scientifique avancée (ex: 1e+10 OK, 1e OK)
//  - \uXXXX émet '?' (support Unicode minimal, extensible)
//  - Pas de validation stricte du format des nombres (tolérant aux erreurs mineures)
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_SERIALIZATION_JSON_NKJSONREADER_H
#define NKENTSEU_SERIALIZATION_JSON_NKJSONREADER_H

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
        // CLASSE : NkJSONReader
        // DESCRIPTION : Désérialiseur JSON pour NkArchive
        // =============================================================================
        /**
         * @class NkJSONReader
         * @brief Désérialiseur JSON pour convertir du texte JSON en NkArchive
         * @ingroup SerializationFormatters
         *
         * Fonctionnalités :
         *  - Parsing récursif avec gestion des objets {} et tableaux [] imbriqués
         *  - Reconnaissance des scalaires : null, booléens, nombres, strings échappés
         *  - Gestion robuste des whitespace (espace, tab, newline, CR)
         *  - Message d'erreur détaillé optionnel via paramètre outError
         *  - Conformité RFC 8259 subset : JSON valide et interopérable
         *
         * Format d'entrée attendu :
         *  @code
         *  {
         *    "player": {
         *      "name": "Hero",
         *      "level": 42,
         *      "inventory": ["sword", "shield", "potion"]
         *    },
         *    "settings": {
         *      "volume": 0.75,
         *      "fullscreen": true
         *    }
         *  }
         *  @endcode
         *
         * Règles de parsing :
         *  - Objet racine obligatoire : erreur si le JSON ne commence pas par '{'
         *  - Clés toujours entre quotes doubles : "key"
         *  - Strings entre quotes doubles avec échappement : "value\"with\"quotes"
         *  - Booléens case-sensitive : true/false (minuscules uniquement)
         *  - Null case-sensitive : null (minuscules uniquement)
         *  - Nombres : entiers signés/non-signés ou flottants en notation décimale
         *
         * Thread-safety :
         *  - Toutes les méthodes sont thread-safe (pas d'état interne mutable)
         *  - L'archive de sortie est fournie par l'appelant
         *
         * @note Ce parser est conçu pour la compatibilité avec NkJSONWriter.
         *       Il n'est pas un parser JSON générique — uniquement pour le format produit.
         *
         * @example Lecture basique d'un JSON en archive
         * @code
         * const char* json = R"({
         *   "app.name": "MyGame",
         *   "app.version": 1,
         *   "debug.enabled": true
         * })";
         *
         * nkentseu::NkArchive config;
         * nkentseu::NkString error;
         * if (nkentseu::NkJSONReader::ReadArchive(json, config, &error)) {
         *     nkentseu::NkString appName;
         *     config.GetString("app.name", appName);
         *     printf("App: %s\n", appName.CStr());
         * } else {
         *     printf("Parse error: %s\n", error.CStr());
         * }
         * @endcode
         */
        class NKENTSEU_SERIALIZATION_CLASS_EXPORT NkJSONReader {


            // -----------------------------------------------------------------
            // SECTION 3 : MEMBRES PUBLICS
            // -----------------------------------------------------------------
        public:


            // -----------------------------------------------------------------
            // MÉTHODE STATIQUE : DÉSÉRIALISATION JSON
            // -----------------------------------------------------------------
            /**
             * @brief Désérialise du texte JSON dans un NkArchive fourni
             * @param json Vue de chaîne contenant le texte JSON à parser
             * @param outArchive Référence vers l'archive de destination (vidée avant parsing)
             * @param outError Pointeur optionnel vers NkString pour message d'erreur détaillé
             * @return true si le parsing a réussi, false en cas d'erreur de syntaxe
             * @ingroup JSONReaderAPI
             *
             * Comportement :
             *  - outArchive est vidé en début d'opération (Clear() appelé)
             *  - Validation du format : doit commencer par '{' (objet racine)
             *  - Parsing récursif des clés/valeurs jusqu'à la fermeture '}'
             *  - Les whitespace sont ignorés entre tokens
             *  - Les erreurs de syntaxe retournent false avec message optionnel
             *
             * Gestion des erreurs :
             *  - Si outError != nullptr et erreur : message formaté avec position
             *  - Erreurs courantes : quote manquante, ':' attendu, ',' manquant, EOF prématuré
             *  - En cas d'erreur partielle : outArchive peut contenir des données partielles
             *
             * @note Méthode noexcept-friendly : aucune exception levée, erreurs gérées par retour bool
             * @note Performance : O(n) où n = taille du texte JSON, avec parsing single-pass
             *
             * @example
             * @code
             * nkentseu::NkString json = LoadFile("config.json");
             * nkentseu::NkArchive config;
             * nkentseu::NkString errorMsg;
             *
             * if (!nkentseu::NkJSONReader::ReadArchive(json.View(), config, &errorMsg)) {
             *     logger.Error("JSON parse error: %s\n", errorMsg.CStr());
             *     return false;
             * }
             * // config est maintenant peuplé avec les données du JSON
             * @endcode
             */
            static nk_bool ReadArchive(NkStringView json,
                                       NkArchive& outArchive,
                                       NkString* outError = nullptr);


        }; // class NkJSONReader


    } // namespace nkentseu


#endif // NKENTSEU_SERIALIZATION_JSON_NKJSONREADER_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKJSONREADER.H
// =============================================================================
// Ce fichier fournit le désérialiseur JSON pour NkArchive.
// Il combine parsing récursif, gestion d'erreurs et validation de syntaxe.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Lecture basique d'un fichier de configuration
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/JSON/NkJSONReader.h>

    bool LoadGameConfig(const char* filename, nkentseu::NkArchive& outConfig) {
        // Chargement du fichier en mémoire (pseudo-code)
        nkentseu::NkString jsonContent = File::ReadText(filename);
        if (jsonContent.Empty()) {
            return false;
        }

        // Parsing JSON
        nkentseu::NkString error;
        if (!nkentseu::NkJSONReader::ReadArchive(jsonContent.View(), outConfig, &error)) {
            logger.Error("JSON parse error in %s: %s\n", filename, error.CStr());
            return false;
        }

        return true;
    }

    // Usage :
    // nkentseu::NkArchive config;
    // if (LoadGameConfig("config.json", config)) {
    //     // Utiliser config...
    // }
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Parsing avec validation de schéma basique
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/JSON/NkJSONReader.h>

    bool LoadAndValidatePlayer(const char* jsonData, nkentseu::NkArchive& outPlayer) {
        nkentseu::NkString error;
        if (!nkentseu::NkJSONReader::ReadArchive(jsonData, outPlayer, &error)) {
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
    #include <NKSerialization/JSON/NkJSONReader.h>

    void ParseWithDetailedError(const char* json) {
        nkentseu::NkArchive result;
        nkentseu::NkString error;

        if (!nkentseu::NkJSONReader::ReadArchive(json, result, &error)) {
            // Affichage de l'erreur pour debugging
            printf("JSON Error: %s\n", error.CStr());

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
    #include <NKSerialization/JSON/NkJSONReader.h>

    void LoadComplexStructure() {
        const char* json = R"({
          "game": {
            "title": "Adventure Quest",
            "version": 2.1,
            "players": ["Alice", "Bob"]
          },
          "settings": {
            "difficulty": 0.75,
            "allowMods": true,
            "maxFps": 144
          }
        })";

        nkentseu::NkArchive data;
        if (nkentseu::NkJSONReader::ReadArchive(json, data)) {
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
    #include <NKSerialization/JSON/NkJSONReader.h>
    #include <NKSerialization/NkISerializable.h>

    nkentseu::NkISerializable* LoadSerializableFromJSON(const char* json,
                                                        const char* expectedType) {
        // Parsing JSON vers archive intermédiaire
        nkentseu::NkArchive data;
        nkentseu::NkString error;
        if (!nkentseu::NkJSONReader::ReadArchive(json, data, &error)) {
            logger.Error("JSON parse failed: %s\n", error.CStr());
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
    // auto* player = LoadSerializableFromJSON(jsonData, "PlayerData");
    // if (player) { /* utiliser... *\/ delete player; }
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================