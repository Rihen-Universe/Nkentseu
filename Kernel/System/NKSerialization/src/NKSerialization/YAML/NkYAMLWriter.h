// =============================================================================
// NKSerialization/YAML/NkYAMLWriter.h
// Sérialiseur YAML pour NkArchive — écriture de données structurées en format YAML.
//
// Design :
//  - Écriture récursive avec gestion correcte de l'indentation
//  - Support des nœuds scalaires, objets (block mapping) et tableaux (block sequence)
//  - Échappement sécurisé des chaînes avec single quotes YAML
//  - Détection automatique des clés nécessitant des guillemets
//  - Export contrôlé via NKENTSEU_SERIALIZATION_CLASS_EXPORT
//  - Aucune allocation dynamique inutile : réutilisation de NkString pour le buffer
//
// Format YAML produit :
//  - Blocs indentés avec 2 espaces par niveau (standard YAML lisible)
//  - Clés simples non quotées, clés spéciales quotées avec single quotes
//  - Valeurs scalaires : null (~), booléens, nombres, strings échappés
//  - Tableaux : syntaxe "- item" avec indentation correcte
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_SERIALIZATION_YAML_NKYAMLWRITER_H
#define NKENTSEU_SERIALIZATION_YAML_NKYAMLWRITER_H

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
        // CLASSE : NkYAMLWriter
        // DESCRIPTION : Sérialiseur YAML pour NkArchive
        // =============================================================================
        /**
         * @class NkYAMLWriter
         * @brief Sérialiseur YAML pour convertir un NkArchive en texte YAML lisible
         * @ingroup SerializationFormatters
         *
         * Fonctionnalités :
         *  - Écriture récursive avec gestion d'indentation (2 espaces/niveau)
         *  - Support complet des nœuds : scalaires, objets, tableaux
         *  - Échappement YAML-safe des chaînes avec single quotes
         *  - Détection automatique des clés nécessitant des guillemets
         *  - Marqueur de document YAML "---" en tête de sortie
         *
         * Format de sortie :
         *  @code
         *  ---
         *  player:
         *    name: 'Hero''s Name'
         *    level: 42
         *    inventory:
         *      - 'sword'
         *      - 'shield'
         *  settings:
         *    volume: 0.75
         *    fullscreen: true
         *  @endcode
         *
         * Règles d'échappement :
         *  - Single quotes : ' → '' (doublage YAML standard)
         *  - Clés avec espaces/caractères spéciaux : automatiquement quotées
         *  - Valeurs null : représentées par ~ (tilde YAML)
         *  - Booléens/nombres : écrits tels quels (sans guillemets)
         *
         * Thread-safety :
         *  - Toutes les méthodes sont thread-safe (pas d'état interne mutable)
         *  - Le buffer de sortie NkString est géré par l'appelant
         *
         * @note Ce sérialiseur produit du YAML lisible par l'homme, pas optimisé pour la taille.
         *       Pour la production, envisager un mode "compact" optionnel en futur.
         *
         * @example Écriture basique d'un archive en YAML
         * @code
         * nkentseu::NkArchive config;
         * config.SetString("app.name", "MyGame");
         * config.SetInt32("app.version", 1);
         * config.SetBool("debug.enabled", true);
         *
         * // Écriture dans une chaîne
         * nkentseu::NkString yaml = nkentseu::NkYAMLWriter::WriteArchive(config);
         * printf("%s\n", yaml.CStr());
         *
         * // Ou écriture dans un buffer existant
         * nkentseu::NkString buffer;
         * if (nkentseu::NkYAMLWriter::WriteArchive(config, buffer)) {
         *     // Sauvegarde dans un fichier...
         * }
         * @endcode
         */
        class NKENTSEU_SERIALIZATION_CLASS_EXPORT NkYAMLWriter {


            // -----------------------------------------------------------------
            // SECTION 3 : MEMBRES PUBLICS
            // -----------------------------------------------------------------
        public:


            // -----------------------------------------------------------------
            // MÉTHODES STATIQUES : SÉRIALISATION YAML
            // -----------------------------------------------------------------
            /**
             * @brief Sérialise un NkArchive en texte YAML dans un buffer fourni
             * @param archive Archive source à sérialiser
             * @param outYaml Référence vers NkString pour recevoir la sortie YAML
             * @return true si la sérialisation a réussi, false en cas d'erreur
             * @ingroup YAMLWriterAPI
             *
             * Comportement :
             *  - outYaml est vidé en début d'opération (Clear() appelé)
             *  - Le marqueur de document YAML "---" est ajouté en tête
             *  - L'indentation commence au niveau 0 (pas d'indentation racine)
             *
             * @note Méthode noexcept-friendly : aucune exception levée, erreurs gérées par retour bool
             * @note Performance : O(n) où n = nombre total de nœuds dans l'archive
             *
             * @example
             * @code
             * nkentseu::NkArchive data;
             * data.SetInt32("count", 42);
             *
             * nkentseu::NkString output;
             * if (nkentseu::NkYAMLWriter::WriteArchive(data, output)) {
             *     // output contient : "---\ncount: 42\n"
             * }
             * @endcode
             */
            static nk_bool WriteArchive(const NkArchive& archive, NkString& outYaml);

            /**
             * @brief Sérialise un NkArchive en texte YAML et retourne le résultat
             * @param archive Archive source à sérialiser
             * @return NkString contenant le texte YAML généré
             * @ingroup YAMLWriterAPI
             *
             * Comportement :
             *  - Crée un nouveau NkString pour la sortie
             *  - Le marqueur "---" est inclus en tête du document
             *  - Pratique pour les usages one-shot ou les retours de fonction
             *
             * @note Alloue un NkString interne — éviter dans les boucles serrées
             * @note Préférer WriteArchive(archive, outYaml) pour les écritures répétées
             *
             * @example
             * @code
             * nkentseu::NkArchive player;
             * player.SetString("name", "Alice");
             * player.SetInt32("level", 10);
             *
             * nkentseu::NkString yaml = nkentseu::NkYAMLWriter::WriteArchive(player);
             * SaveToFile("player.yaml", yaml.CStr());
             * @endcode
             */
            static NkString WriteArchive(const NkArchive& archive);


        }; // class NkYAMLWriter


    } // namespace nkentseu


#endif // NKENTSEU_SERIALIZATION_YAML_NKYAMLWRITER_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKYAMLWRITER.H
// =============================================================================
// Ce fichier fournit le sérialiseur YAML pour NkArchive.
// Il combine écriture récursive, gestion d'indentation et échappement YAML-safe.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Sérialisation basique d'une configuration
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/YAML/NkYAMLWriter.h>

    void SaveGameConfig() {
        nkentseu::NkArchive config;
        config.SetString("game.title", "Adventure Quest");
        config.SetInt32("game.maxPlayers", 4);
        config.SetBool("game.allowMods", true);
        config.SetFloat32("game.difficulty", 0.75f);

        // Écriture dans une chaîne
        nkentseu::NkString yaml = nkentseu::NkYAMLWriter::WriteArchive(config);

        // Sauvegarde dans un fichier (pseudo-code)
        // File::WriteText("config.yaml", yaml.CStr());
    }
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Structure hiérarchique avec objets imbriqués
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/YAML/NkYAMLWriter.h>

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
        nkentseu::NkString yaml = nkentseu::NkYAMLWriter::WriteArchive(player);
        // Résultat YAML :
        // ---
        // username: 'Hero123'
        // playerId: 987654321
        // stats:
        //   level: 42
        //   experience: 12500.5
        //   kills: 350
        // inventory:
        //   - 'Iron Sword'
        //   - 'Health Potion'
        //   - 'Magic Shield'
    }
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Gestion des clés spéciales et échappement
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/YAML/NkYAMLWriter.h>

    void SaveWithSpecialKeys() {
        nkentseu::NkArchive data;

        // Clés avec espaces → automatiquement quotées en sortie
        data.SetString("user first name", "Alice");
        data.SetString("user.last-name", "Wonder");  // tiret : pas de quotes nécessaire

        // Valeurs avec single quotes → échappées en doublant
        data.SetString("quote", "It's a beautiful day!");

        // Valeur null → représentée par ~
        data.SetNull("optionalField");

        nkentseu::NkString yaml = nkentseu::NkYAMLWriter::WriteArchive(data);
        // Sortie YAML :
        // ---
        // 'user first name': 'Alice'
        // user.last-name: 'Wonder'
        // quote: 'It''s a beautiful day!'
        // optionalField: ~
    }
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Écriture itérative avec réutilisation de buffer
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/YAML/NkYAMLWriter.h>

    void BatchExport(const nkentseu::NkVector<nkentseu::NkArchive>& archives,
                     const char* outputDir) {
        // Réutiliser le même buffer pour éviter les allocations répétées
        nkentseu::NkString buffer;

        for (nk_size i = 0; i < archives.Size(); ++i) {
            buffer.Clear();  // Réinitialisation explicite (optionnel, WriteArchive le fait)

            if (nkentseu::NkYAMLWriter::WriteArchive(archives[i], buffer)) {
                // Construction du nom de fichier
                nkentseu::NkString filename = outputDir;
                filename.Append("/export_");
                filename.Append(nkentseu::NkString::Fmtf("%zu.yaml", i));

                // Sauvegarde (pseudo-code)
                // File::WriteText(filename.CStr(), buffer.CStr());
            }
        }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Intégration avec NkISerializable pour types polymorphiques
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/YAML/NkYAMLWriter.h>
    #include <NKSerialization/NkISerializable.h>

    bool SaveSerializable(const nkentseu::NkISerializable* obj, const char* filename) {
        if (!obj) {
            return false;
        }

        // Sérialisation via l'interface de base
        nkentseu::NkArchive data;
        if (!obj->Serialize(data)) {
            return false;
        }

        // Conversion en YAML
        nkentseu::NkString yaml = nkentseu::NkYAMLWriter::WriteArchive(data);

        // Sauvegarde dans un fichier (pseudo-code)
        // return File::WriteText(filename, yaml.CStr());
        return true;  // Simplifié pour l'exemple
    }

    // Usage :
    // PlayerData player;
    // SaveSerializable(&player, "player_save.yaml");
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================