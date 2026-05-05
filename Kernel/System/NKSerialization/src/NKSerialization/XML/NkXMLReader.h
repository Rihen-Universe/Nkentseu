// =============================================================================
// NKSerialization/XML/NkXMLReader.h
// Désérialiseur XML pour NkArchive — lecture de données structurées depuis XML.
//
// Design :
//  - Parsing token par token avec gestion récursive des blocs imbriqués
//  - Support des éléments <entry>, <object> et <array> avec attributs
//  - Unescape XML des entités (&amp;, &lt;, &gt;, &quot;, &apos;)
//  - Reconstruction des types via attribut type= pour round-trip fidelity
//  - Export contrôlé via NKENTSEU_SERIALIZATION_CLASS_EXPORT
//  - Message d'erreur optionnel via paramètre NkString* outError
//
// Format XML supporté :
//  - Racine obligatoire : <archive>...</archive>
//  - Scalaires : <entry key="..." type="...">value</entry>
//  - Objets : <object key="...">[enfants]</object>
//  - Tableaux : <array key="..." count="N">[éléments indexés]</array>
//  - Types : null, bool, int64, uint64, float64, string
//
// Limitations connues :
//  - Pas de support des espaces de noms XML (xmlns)
//  - Pas de support des CDATA ou processing instructions
//  - Les attributs autres que key/type/count sont ignorés
//  - Parsing non-validant : tolérant aux erreurs mineures
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_SERIALIZATION_XML_NKXMLREADER_H
#define NKENTSEU_SERIALIZATION_XML_NKXMLREADER_H

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
        // CLASSE : NkXMLReader
        // DESCRIPTION : Désérialiseur XML pour NkArchive
        // =============================================================================
        /**
         * @class NkXMLReader
         * @brief Désérialiseur XML pour convertir du texte XML en NkArchive
         * @ingroup SerializationFormatters
         *
         * Fonctionnalités :
         *  - Parsing token par token avec suivi récursif des blocs
         *  - Reconnaissance des éléments <entry>, <object>, <array>
         *  - Unescape des entités XML (&amp; → &, etc.)
         *  - Reconstruction des types via attribut type=
         *  - Message d'erreur détaillé optionnel via paramètre outError
         *
         * Format d'entrée attendu :
         *  @code
         *  <archive>
         *    <entry key="player.name" type="string">Hero</entry>
         *    <entry key="player.level" type="int64">42</entry>
         *    <object key="settings">
         *      <entry key="volume" type="float64">0.75</entry>
         *    </object>
         *    <array key="items" count="2">
         *      <entry key="0" type="string">sword</entry>
         *      <entry key="1" type="string">shield</entry>
         *    </array>
         *  </archive>
         *  @endcode
         *
         * Règles de parsing :
         *  - Racine <archive> obligatoire : erreur si absente
         *  - Attribut key= requis pour tous les éléments
         *  - Attribut type= optionnel pour <entry> (défaut: string)
         *  - Attribut count= informatif pour <array> (non validé strictement)
         *  - Contenu des <entry> : unescape XML appliqué avant parsing du type
         *
         * Thread-safety :
         *  - Toutes les méthodes sont thread-safe (pas d'état interne mutable)
         *  - L'archive de sortie est fournie par l'appelant
         *
         * @note Ce parser est conçu pour la compatibilité avec NkXMLWriter.
         *       Il n'est pas un parser XML générique — uniquement pour le format produit.
         *
         * @example Lecture basique d'un XML en archive
         * @code
         * const char* xml = R"(
         * <archive>
         *   <entry key="app.name" type="string">MyGame</entry>
         *   <entry key="app.version" type="int64">1</entry>
         * </archive>)";
         *
         * nkentseu::NkArchive config;
         * nkentseu::NkString error;
         * if (nkentseu::NkXMLReader::ReadArchive(xml, config, &error)) {
         *     nkentseu::NkString appName;
         *     config.GetString("app.name", appName);
         *     printf("App: %s\n", appName.CStr());
         * } else {
         *     printf("Parse error: %s\n", error.CStr());
         * }
         * @endcode
         */
        class NKENTSEU_SERIALIZATION_CLASS_EXPORT NkXMLReader {


            // -----------------------------------------------------------------
            // SECTION 3 : MEMBRES PUBLICS
            // -----------------------------------------------------------------
        public:


            // -----------------------------------------------------------------
            // MÉTHODE STATIQUE : DÉSÉRIALISATION XML
            // -----------------------------------------------------------------
            /**
             * @brief Désérialise du texte XML dans un NkArchive fourni
             * @param xml Vue de chaîne contenant le texte XML à parser
             * @param outArchive Référence vers l'archive de destination (vidée avant parsing)
             * @param outError Pointeur optionnel vers NkString pour message d'erreur détaillé
             * @return true si le parsing a réussi, false en cas d'erreur de syntaxe
             * @ingroup XMLReaderAPI
             *
             * Comportement :
             *  - outArchive est vidé en début d'opération (Clear() appelé)
             *  - Recherche du tag racine <archive> — erreur si absent
             *  - Parsing récursif des enfants jusqu'au tag fermant </archive>
             *  - Les éléments inconnus ou mal formés sont ignorés (tolérant)
             *
             * Gestion des erreurs :
             *  - Si outError != nullptr et erreur : message formaté décrivant le problème
             *  - Erreurs courantes : tag racine manquant, attribut key= absent, XML mal formé
             *  - En cas d'erreur partielle : outArchive peut contenir des données partielles
             *
             * @note Méthode noexcept-friendly : aucune exception levée, erreurs gérées par retour bool
             * @note Performance : O(n × d) où n = taille du XML, d = profondeur max d'imbrication
             *
             * @example
             * @code
             * nkentseu::NkString xml = LoadFile("config.xml");
             * nkentseu::NkArchive config;
             * nkentseu::NkString errorMsg;
             *
             * if (!nkentseu::NkXMLReader::ReadArchive(xml.View(), config, &errorMsg)) {
             *     logger.Error("XML parse error: %s\n", errorMsg.CStr());
             *     return false;
             * }
             * // config est maintenant peuplé avec les données du XML
             * @endcode
             */
            static nk_bool ReadArchive(NkStringView xml,
                                       NkArchive& outArchive,
                                       NkString* outError = nullptr);


        }; // class NkXMLReader


    } // namespace nkentseu


#endif // NKENTSEU_SERIALIZATION_XML_NKXMLREADER_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKXMLREADER.H
// =============================================================================
// Ce fichier fournit le désérialiseur XML pour NkArchive.
// Il combine parsing token par token, gestion récursive et validation de syntaxe.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Lecture basique d'un fichier de configuration
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/XML/NkXMLReader.h>

    bool LoadGameConfig(const char* filename, nkentseu::NkArchive& outConfig) {
        // Chargement du fichier en mémoire (pseudo-code)
        nkentseu::NkString xmlContent = File::ReadText(filename);
        if (xmlContent.Empty()) {
            return false;
        }

        // Parsing XML
        nkentseu::NkString error;
        if (!nkentseu::NkXMLReader::ReadArchive(xmlContent.View(), outConfig, &error)) {
            logger.Error("XML parse error in %s: %s\n", filename, error.CStr());
            return false;
        }

        return true;
    }

    // Usage :
    // nkentseu::NkArchive config;
    // if (LoadGameConfig("config.xml", config)) {
    //     // Utiliser config...
    // }
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Parsing avec validation de schéma basique
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/XML/NkXMLReader.h>

    bool LoadAndValidatePlayer(const char* xmlData, nkentseu::NkArchive& outPlayer) {
        nkentseu::NkString error;
        if (!nkentseu::NkXMLReader::ReadArchive(xmlData, outPlayer, &error)) {
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
    #include <NKSerialization/XML/NkXMLReader.h>

    void ParseWithDetailedError(const char* xml) {
        nkentseu::NkArchive result;
        nkentseu::NkString error;

        if (!nkentseu::NkXMLReader::ReadArchive(xml, result, &error)) {
            // Affichage de l'erreur pour debugging
            printf("XML Error: %s\n", error.CStr());

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
    #include <NKSerialization/XML/NkXMLReader.h>

    void LoadComplexStructure() {
        const char* xml = R"(
        <archive>
          <object key="game">
            <entry key="title" type="string">Adventure Quest</entry>
            <entry key="version" type="float64">2.1</entry>
            <array key="players" count="2">
              <entry key="0" type="string">Alice</entry>
              <entry key="1" type="string">Bob</entry>
            </array>
          </object>
          <object key="settings">
            <entry key="difficulty" type="float64">0.75</entry>
            <entry key="allowMods" type="bool">true</entry>
          </object>
        </archive>)";

        nkentseu::NkArchive data;
        if (nkentseu::NkXMLReader::ReadArchive(xml, data)) {
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
    #include <NKSerialization/XML/NkXMLReader.h>
    #include <NKSerialization/NkISerializable.h>

    nkentseu::NkISerializable* LoadSerializableFromXML(const char* xml,
                                                       const char* expectedType) {
        // Parsing XML vers archive intermédiaire
        nkentseu::NkArchive data;
        nkentseu::NkString error;
        if (!nkentseu::NkXMLReader::ReadArchive(xml, data, &error)) {
            logger.Error("XML parse failed: %s\n", error.CStr());
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
    // auto* player = LoadSerializableFromXML(xmlData, "PlayerData");
    // if (player) { /* utiliser... *\/ delete player; }
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================