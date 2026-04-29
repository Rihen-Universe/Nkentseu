// =============================================================================
// NKSerialization/JSON/NkJSONWriter.h
// Sérialiseur JSON pour NkArchive — écriture de données structurées en format JSON.
//
// Design :
//  - Écriture récursive avec gestion configurable de l'indentation
//  - Support complet des nœuds : scalaires, objets (mappings) et tableaux (arrays)
//  - Échappement JSON-safe des caractères spéciaux (\", \\, \n, etc.)
//  - Export contrôlé via NKENTSEU_SERIALIZATION_CLASS_EXPORT
//  - Options de formatage : pretty-print avec indentation configurable ou compact
//
// Format JSON produit :
//  - Objet racine : { "key1": value1, "key2": value2, ... }
//  - Valeurs scalaires : null, booléens, nombres, strings échappés
//  - Objets imbriqués : { "nested": { "key": "value" } }
//  - Tableaux : [ "item1", "item2", { "obj": "val" } ]
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_SERIALIZATION_JSON_NKJSONWRITER_H
#define NKENTSEU_SERIALIZATION_JSON_NKJSONWRITER_H

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
        // CLASSE : NkJSONWriter
        // DESCRIPTION : Sérialiseur JSON pour NkArchive
        // =============================================================================
        /**
         * @class NkJSONWriter
         * @brief Sérialiseur JSON pour convertir un NkArchive en texte JSON lisible
         * @ingroup SerializationFormatters
         *
         * Fonctionnalités :
         *  - Écriture récursive avec gestion d'indentation configurable
         *  - Support complet des nœuds : scalaires, objets, tableaux
         *  - Échappement JSON-safe des caractères spéciaux via NkJSONEscapeString()
         *  - Options de formatage : pretty-print ou compact (sans espaces)
         *  - Conformité RFC 8259 : JSON valide et interopérable
         *
         * Format de sortie :
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
         * Règles d'échappement JSON :
         *  - " → \" (double quote)
         *  - \ → \\ (backslash)
         *  - / → \/ (slash, optionnel mais recommandé)
         *  - \b → \\b (backspace), \f → \\f (form feed), \n → \\n (newline)
         *  - \r → \\r (carriage return), \t → \\t (tab)
         *  - Caractères de contrôle (<0x20) → \uXXXX (Unicode escape)
         *
         * Thread-safety :
         *  - Toutes les méthodes sont thread-safe (pas d'état interne mutable)
         *  - Le buffer de sortie NkString est géré par l'appelant
         *
         * @note Ce sérialiseur produit du JSON standard lisible par l'homme.
         *       Pour la production, le mode compact (pretty=false) réduit la taille.
         *
         * @example Écriture basique d'un archive en JSON
         * @code
         * nkentseu::NkArchive config;
         * config.SetString("app.name", "MyGame");
         * config.SetInt32("app.version", 1);
         * config.SetBool("debug.enabled", true);
         *
         * // Écriture avec pretty-print par défaut
         * nkentseu::NkString json = nkentseu::NkJSONWriter::WriteArchive(config);
         * printf("%s\n", json.CStr());
         *
         * // Ou écriture compacte dans un buffer existant
         * nkentseu::NkString buffer;
         * if (nkentseu::NkJSONWriter::WriteArchive(config, buffer, false)) {
         *     // buffer contient du JSON sans indentation
         * }
         * @endcode
         */
        class NKENTSEU_SERIALIZATION_CLASS_EXPORT NkJSONWriter {


            // -----------------------------------------------------------------
            // SECTION 3 : MEMBRES PUBLICS
            // -----------------------------------------------------------------
        public:


            // -----------------------------------------------------------------
            // MÉTHODES STATIQUES : SÉRIALISATION JSON
            // -----------------------------------------------------------------
            /**
             * @brief Sérialise un NkArchive en texte JSON dans un buffer fourni
             * @param archive Archive source à sérialiser
             * @param outJson Référence vers NkString pour recevoir la sortie JSON
             * @param pretty true pour indentation lisible, false pour format compact
             * @param indentSpaces Nombre d'espaces par niveau d'indentation (défaut: 2)
             * @return true si la sérialisation a réussi, false en cas d'erreur
             * @ingroup JSONWriterAPI
             *
             * Comportement :
             *  - outJson est vidé en début d'opération (Clear() appelé)
             *  - Si pretty=true : sauts de ligne et indentation selon indentSpaces
             *  - Si pretty=false : JSON compact sur une ligne (plus rapide, plus petit)
             *  - Les clés et strings sont échappés via NkJSONEscapeString()
             *
             * @note Méthode noexcept-friendly : aucune exception levée, erreurs gérées par retour bool
             * @note Performance : O(n) où n = nombre total de caractères en sortie
             *
             * @example
             * @code
             * nkentseu::NkArchive data;
             * data.SetInt32("count", 42);
             *
             * nkentseu::NkString output;
             * if (nkentseu::NkJSONWriter::WriteArchive(data, output, true, 4)) {
             *     // output contient du JSON indenté avec 4 espaces/niveau
             * }
             * @endcode
             */
            static nk_bool WriteArchive(const NkArchive& archive,
                                        NkString& outJson,
                                        nk_bool pretty = true,
                                        nk_int32 indentSpaces = 2);

            /**
             * @brief Sérialise un NkArchive en texte JSON et retourne le résultat
             * @param archive Archive source à sérialiser
             * @param pretty true pour indentation lisible, false pour format compact
             * @param indentSpaces Nombre d'espaces par niveau d'indentation (défaut: 2)
             * @return NkString contenant le texte JSON généré
             * @ingroup JSONWriterAPI
             *
             * Comportement :
             *  - Crée un nouveau NkString pour la sortie
             *  - Pratique pour les usages one-shot ou les retours de fonction
             *  - Délègue à WriteArchive(archive, outJson, pretty, indentSpaces)
             *
             * @note Alloue un NkString interne — éviter dans les boucles serrées
             * @note Préférer WriteArchive(archive, outJson, ...) pour les écritures répétées
             *
             * @example
             * @code
             * nkentseu::NkArchive player;
             * player.SetString("name", "Alice");
             * player.SetInt32("level", 10);
             *
             * // JSON compact pour transmission réseau
             * nkentseu::NkString json = nkentseu::NkJSONWriter::WriteArchive(player, false);
             * SendOverNetwork(json.CStr(), json.Length());
             * @endcode
             */
            static NkString WriteArchive(const NkArchive& archive,
                                         nk_bool pretty = true,
                                         nk_int32 indentSpaces = 2);


        }; // class NkJSONWriter


    } // namespace nkentseu


#endif // NKENTSEU_SERIALIZATION_JSON_NKJSONWRITER_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKJSONWRITER.H
// =============================================================================
// Ce fichier fournit le sérialiseur JSON pour NkArchive.
// Il combine écriture récursive, gestion d'indentation et échappement JSON-safe.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Sérialisation basique d'une configuration
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/JSON/NkJSONWriter.h>

    void SaveGameConfig() {
        nkentseu::NkArchive config;
        config.SetString("game.title", "Adventure Quest");
        config.SetInt32("game.maxPlayers", 4);
        config.SetBool("game.allowMods", true);
        config.SetFloat32("game.difficulty", 0.75f);

        // Écriture avec pretty-print par défaut
        nkentseu::NkString json = nkentseu::NkJSONWriter::WriteArchive(config);

        // Sauvegarde dans un fichier (pseudo-code)
        // File::WriteText("config.json", json.CStr());
    }
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Structure hiérarchique avec objets et tableaux imbriqués
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/JSON/NkJSONWriter.h>

    void SavePlayerProfile() {
        nkentseu::NkArchive player;

        // Champs scalaires directs
        player.SetString("username", "Hero123");
        player.SetInt64("playerId", 987654321);

        // Objet imbriqué "stats"
        nkentseu::NkArchive stats;
        stats.SetInt32("level", 42);
        stats.SetFloat32("experience", 12500.5f);
        stats.SetInt32("kills", 350);
        player.SetObject("stats", stats);

        // Tableau de strings "inventory"
        nkentseu::NkVector<nkentseu::NkArchiveValue> items;
        items.PushBack(nkentseu::NkArchiveValue::FromString("Iron Sword"));
        items.PushBack(nkentseu::NkArchiveValue::FromString("Health Potion"));
        items.PushBack(nkentseu::NkArchiveValue::FromString("Magic Shield"));
        player.SetArray("inventory", items);

        // Sérialisation
        nkentseu::NkString json = nkentseu::NkJSONWriter::WriteArchive(player);
        // Résultat JSON :
        // {
        //   "username": "Hero123",
        //   "playerId": 987654321,
        //   "stats": {
        //     "level": 42,
        //     "experience": 12500.5,
        //     "kills": 350
        //   },
        //   "inventory": ["Iron Sword", "Health Potion", "Magic Shield"]
        // }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Gestion des caractères spéciaux JSON
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/JSON/NkJSONWriter.h>

    void SaveWithSpecialChars() {
        nkentseu::NkArchive data;

        // Valeurs contenant des caractères JSON spéciaux
        data.SetString("quote", "She said: \"Hello!\"");
        data.SetString("path", "C:\\Program Files\\MyGame");
        data.SetString("multiline", "Line1\nLine2\tTabbed");

        nkentseu::NkString json = nkentseu::NkJSONWriter::WriteArchive(data);
        // Sortie JSON (échappement automatique) :
        // {
        //   "quote": "She said: \"Hello!\"",
        //   "path": "C:\\Program Files\\MyGame",
        //   "multiline": "Line1\nLine2\tTabbed"
        // }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Écriture itérative avec réutilisation de buffer
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/JSON/NkJSONWriter.h>

    void BatchExport(const nkentseu::NkVector<nkentseu::NkArchive>& archives,
                     const char* outputDir) {
        // Réutiliser le même buffer pour éviter les allocations répétées
        nkentseu::NkString buffer;

        for (nk_size i = 0; i < archives.Size(); ++i) {
            buffer.Clear();  // Réinitialisation explicite (optionnel)

            if (nkentseu::NkJSONWriter::WriteArchive(archives[i], buffer, true, 2)) {
                // Construction du nom de fichier
                nkentseu::NkString filename = outputDir;
                filename.Append("/export_");
                filename.Append(nkentseu::NkString::Fmtf("%zu.json", i));

                // Sauvegarde (pseudo-code)
                // File::WriteText(filename.CStr(), buffer.CStr());
            }
        }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Mode compact pour transmission réseau ou stockage
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/JSON/NkJSONWriter.h>

    void SendConfigOverNetwork(const nkentseu::NkArchive& config) {
        // Format compact : pas d'indentation, plus petit pour le réseau
        nkentseu::NkString json = nkentseu::NkJSONWriter::WriteArchive(config, false);

        // Envoi via socket (pseudo-code)
        // Socket::Send(json.CStr(), json.Length());

        // Pour debugging : pretty-print local
        #ifdef NKENTSEU_DEBUG
        nkentseu::NkString debugJson = nkentseu::NkJSONWriter::WriteArchive(config, true, 2);
        NK_LOG_DEBUG("Sending config:\n%s", debugJson.CStr());
        #endif
    }
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================