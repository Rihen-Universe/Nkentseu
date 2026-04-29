// =============================================================================
// NKSerialization/XML/NkXMLWriter.h
// Sérialiseur XML pour NkArchive — écriture de données structurées en format XML.
//
// Design :
//  - Écriture récursive avec gestion configurable de l'indentation
//  - Support des nœuds scalaires, objets (éléments imbriqués) et tableaux (séquences)
//  - Échappement XML-safe des caractères spéciaux (&, <, >, ", ')
//  - Attributs type pour préservation sémantique des valeurs scalaires
//  - Export contrôlé via NKENTSEU_SERIALIZATION_CLASS_EXPORT
//  - Options de formatage : pretty-print avec indentation configurable
//
// Format XML produit :
//  - Racine <archive> contenant tous les éléments
//  - Éléments <entry key="..." type="..."> pour les scalaires
//  - Éléments <object key="..."> pour les objets imbriqués
//  - Éléments <array key="..." count="N"> pour les tableaux
//  - Attribut type : null, bool, int64, uint64, float64, string
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_SERIALIZATION_XML_NKXMLWRITER_H
#define NKENTSEU_SERIALIZATION_XML_NKXMLWRITER_H

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
        // CLASSE : NkXMLWriter
        // DESCRIPTION : Sérialiseur XML pour NkArchive
        // =============================================================================
        /**
         * @class NkXMLWriter
         * @brief Sérialiseur XML pour convertir un NkArchive en texte XML structuré
         * @ingroup SerializationFormatters
         *
         * Fonctionnalités :
         *  - Écriture récursive avec gestion d'indentation configurable
         *  - Support complet des nœuds : scalaires, objets, tableaux
         *  - Échappement XML-safe des caractères spéciaux (& → &amp;, etc.)
         *  - Attribution du type sémantique pour round-trip fidelity
         *  - Options de formatage : pretty-print ou compact
         *
         * Format de sortie :
         *  @code
         *  <archive>
         *    <entry key="player.name" type="string">Hero</entry>
         *    <entry key="player.level" type="int64">42</entry>
         *    <object key="settings">
         *      <entry key="volume" type="float64">0.75</entry>
         *      <entry key="fullscreen" type="bool">true</entry>
         *    </object>
         *    <array key="inventory" count="3">
         *      <entry key="0" type="string">sword</entry>
         *      <entry key="1" type="string">shield</entry>
         *      <entry key="2" type="string">potion</entry>
         *    </array>
         *  </archive>
         *  @endcode
         *
         * Règles d'échappement XML :
         *  - & → &amp;
         *  - < → &lt;
         *  - > → &gt;
         *  - " → &quot;
         *  - ' → &apos;
         *
         * Types supportés via attribut type :
         *  - null    : valeur nulle (contenu vide)
         *  - bool    : true/false (case-sensitive)
         *  - int64   : entier signé 64 bits
         *  - uint64  : entier non-signé 64 bits
         *  - float64 : flottant double précision
         *  - string  : chaîne de caractères (défaut)
         *
         * Thread-safety :
         *  - Toutes les méthodes sont thread-safe (pas d'état interne mutable)
         *  - Le buffer de sortie NkString est géré par l'appelant
         *
         * @note Ce sérialiseur privilégie la lisibilité et la round-trip fidelity.
         *       Pour la production, le mode compact (pretty=false) réduit la taille.
         *
         * @example Écriture basique d'un archive en XML
         * @code
         * nkentseu::NkArchive config;
         * config.SetString("app.name", "MyGame");
         * config.SetInt32("app.version", 1);
         * config.SetBool("debug.enabled", true);
         *
         * // Écriture avec pretty-print par défaut
         * nkentseu::NkString xml = nkentseu::NkXMLWriter::WriteArchive(config);
         * printf("%s\n", xml.CStr());
         *
         * // Ou écriture compacte dans un buffer existant
         * nkentseu::NkString buffer;
         * if (nkentseu::NkXMLWriter::WriteArchive(config, buffer, false)) {
         *     // buffer contient du XML sans indentation
         * }
         * @endcode
         */
        class NKENTSEU_SERIALIZATION_CLASS_EXPORT NkXMLWriter {


            // -----------------------------------------------------------------
            // SECTION 3 : MEMBRES PUBLICS
            // -----------------------------------------------------------------
        public:


            // -----------------------------------------------------------------
            // MÉTHODES STATIQUES : SÉRIALISATION XML
            // -----------------------------------------------------------------
            /**
             * @brief Sérialise un NkArchive en texte XML dans un buffer fourni
             * @param archive Archive source à sérialiser
             * @param outXml Référence vers NkString pour recevoir la sortie XML
             * @param pretty true pour indentation lisible, false pour format compact
             * @param indentSpaces Nombre d'espaces par niveau d'indentation (défaut: 2)
             * @return true si la sérialisation a réussi, false en cas d'erreur
             * @ingroup XMLWriterAPI
             *
             * Comportement :
             *  - outXml est vidé en début d'opération (Clear() appelé)
             *  - La racine <archive>...</archive> est toujours générée
             *  - Si pretty=true : sauts de ligne et indentation selon indentSpaces
             *  - Si pretty=false : XML compact sur une seule ligne (plus rapide)
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
             * if (nkentseu::NkXMLWriter::WriteArchive(data, output, true, 4)) {
             *     // output contient du XML indenté avec 4 espaces/niveau
             * }
             * @endcode
             */
            static nk_bool WriteArchive(const NkArchive& archive,
                                        NkString& outXml,
                                        nk_bool pretty = true,
                                        nk_int32 indentSpaces = 2);

            /**
             * @brief Sérialise un NkArchive en texte XML et retourne le résultat
             * @param archive Archive source à sérialiser
             * @param pretty true pour indentation lisible, false pour format compact
             * @param indentSpaces Nombre d'espaces par niveau d'indentation (défaut: 2)
             * @return NkString contenant le texte XML généré
             * @ingroup XMLWriterAPI
             *
             * Comportement :
             *  - Crée un nouveau NkString pour la sortie
             *  - La racine <archive> est incluse automatiquement
             *  - Pratique pour les usages one-shot ou les retours de fonction
             *
             * @note Alloue un NkString interne — éviter dans les boucles serrées
             * @note Préférer WriteArchive(archive, outXml, ...) pour les écritures répétées
             *
             * @example
             * @code
             * nkentseu::NkArchive player;
             * player.SetString("name", "Alice");
             * player.SetInt32("level", 10);
             *
             * // XML compact pour transmission réseau
             * nkentseu::NkString xml = nkentseu::NkXMLWriter::WriteArchive(player, false);
             * SendOverNetwork(xml.CStr(), xml.Length());
             * @endcode
             */
            static NkString WriteArchive(const NkArchive& archive,
                                         nk_bool pretty = true,
                                         nk_int32 indentSpaces = 2);


        }; // class NkXMLWriter


    } // namespace nkentseu


#endif // NKENTSEU_SERIALIZATION_XML_NKXMLWRITER_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKXMLWRITER.H
// =============================================================================
// Ce fichier fournit le sérialiseur XML pour NkArchive.
// Il combine écriture récursive, gestion d'indentation et échappement XML-safe.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Sérialisation basique d'une configuration
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/XML/NkXMLWriter.h>

    void SaveGameConfig() {
        nkentseu::NkArchive config;
        config.SetString("game.title", "Adventure Quest");
        config.SetInt32("game.maxPlayers", 4);
        config.SetBool("game.allowMods", true);
        config.SetFloat32("game.difficulty", 0.75f);

        // Écriture avec pretty-print par défaut
        nkentseu::NkString xml = nkentseu::NkXMLWriter::WriteArchive(config);

        // Sauvegarde dans un fichier (pseudo-code)
        // File::WriteText("config.xml", xml.CStr());
    }
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Structure hiérarchique avec objets imbriqués
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/XML/NkXMLWriter.h>

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
        nkentseu::NkString xml = nkentseu::NkXMLWriter::WriteArchive(player);
        // Résultat XML :
        // <archive>
        //   <entry key="username" type="string">Hero123</entry>
        //   <entry key="playerId" type="int64">987654321</entry>
        //   <object key="stats">
        //     <entry key="level" type="int64">42</entry>
        //     <entry key="experience" type="float64">12500.5</entry>
        //     <entry key="kills" type="int64">350</entry>
        //   </object>
        //   <array key="inventory" count="3">
        //     <entry key="0" type="string">Iron Sword</entry>
        //     <entry key="1" type="string">Health Potion</entry>
        //     <entry key="2" type="string">Magic Shield</entry>
        //   </array>
        // </archive>
    }
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Gestion des caractères spéciaux XML
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/XML/NkXMLWriter.h>

    void SaveWithSpecialChars() {
        nkentseu::NkArchive data;

        // Valeurs contenant des caractères XML spéciaux
        data.SetString("quote", "Tom & Jerry <heroes>");
        data.SetString("path", "C:\\Program Files\\MyGame");
        data.SetString("comment", "It's a \"great\" day!");

        nkentseu::NkString xml = nkentseu::NkXMLWriter::WriteArchive(data);
        // Sortie XML (échappement automatique) :
        // <archive>
        //   <entry key="quote" type="string">Tom &amp; Jerry &lt;heroes&gt;</entry>
        //   <entry key="path" type="string">C:\Program Files\MyGame</entry>
        //   <entry key="comment" type="string">It&apos;s a &quot;great&quot; day!</entry>
        // </archive>
    }
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Écriture itérative avec réutilisation de buffer
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/XML/NkXMLWriter.h>

    void BatchExport(const nkentseu::NkVector<nkentseu::NkArchive>& archives,
                     const char* outputDir) {
        // Réutiliser le même buffer pour éviter les allocations répétées
        nkentseu::NkString buffer;

        for (nk_size i = 0; i < archives.Size(); ++i) {
            buffer.Clear();  // Réinitialisation explicite (optionnel)

            if (nkentseu::NkXMLWriter::WriteArchive(archives[i], buffer, true, 2)) {
                // Construction du nom de fichier
                nkentseu::NkString filename = outputDir;
                filename.Append("/export_");
                filename.Append(nkentseu::NkString::Fmtf("%zu.xml", i));

                // Sauvegarde (pseudo-code)
                // File::WriteText(filename.CStr(), buffer.CStr());
            }
        }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Mode compact pour transmission réseau
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/XML/NkXMLWriter.h>

    void SendConfigOverNetwork(const nkentseu::NkArchive& config) {
        // Format compact : pas d'indentation, plus petit pour le réseau
        nkentseu::NkString xml = nkentseu::NkXMLWriter::WriteArchive(config, false);

        // Envoi via socket (pseudo-code)
        // Socket::Send(xml.CStr(), xml.Length());

        // Pour debugging : pretty-print local
        #ifdef NKENTSEU_DEBUG
        nkentseu::NkString debugXml = nkentseu::NkXMLWriter::WriteArchive(config, true, 2);
        NK_LOG_DEBUG("Sending config:\n%s", debugXml.CStr());
        #endif
    }
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================