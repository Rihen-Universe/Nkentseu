// =============================================================================
// NKSerialization/JSON/NkJSONWriter.cpp
// Implémentation du sérialiseur JSON pour NkArchive.
//
// Design :
//  - Fonctions internes dans namespace anonyme pour encapsulation
//  - Écriture récursive avec gestion configurable de l'indentation
//  - Échappement JSON-safe des caractères spéciaux via NkJSONEscapeString()
//  - Aucune allocation dynamique inutile : buffer réutilisé par l'appelant
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/JSON/NkJSONValue.h"


namespace nkentseu {


    // =============================================================================
    // NAMESPACE ANONYME : HELPERS INTERNES
    // =============================================================================
    // Ces fonctions sont privées au TU pour éviter la pollution de l'espace de noms.
    // Elles implémentent la logique basique de formatage JSON.

    namespace {


        // -------------------------------------------------------------------------
        // FONCTION : Indent
        // DESCRIPTION : Ajoute des espaces d'indentation au buffer de sortie
        // -------------------------------------------------------------------------
        /**
         * @brief Ajoute l'indentation JSON pour un niveau donné
         * @param out Référence vers le buffer NkString de sortie
         * @param level Niveau d'indentation (0 = racine, 1 = premier enfant, etc.)
         * @param spaces Nombre d'espaces par niveau d'indentation
         * @note noexcept : garantie de non-levée d'exception
         *
         * Convention d'indentation :
         *  - spaces espaces par niveau (typiquement 2 ou 4)
         *  - Pas de tabulations : évite les problèmes de rendu multi-éditeur
         *
         * @note Performance : boucle simple O(level×spaces), négligeable pour depth < 100
         *
         * @example
         * @code
         * NkString buf;
         * Indent(buf, 0, 2);  // buf inchangé
         * Indent(buf, 2, 2);  // buf += "    " (4 espaces)
         * @endcode
         */
        void Indent(NkString& out, int level, int spaces) noexcept {
            for (int i = 0; i < level * spaces; ++i) {
                out.Append(' ');
            }
        }

        // -------------------------------------------------------------------------
        // DÉCLARATIONS FORWARD : Fonctions récursives mutuellement dépendantes
        // -------------------------------------------------------------------------
        void WriteNodeJSON(const NkArchiveNode& node,
                           NkString& out,
                           bool pretty,
                           int indent,
                           int level) noexcept;

        void WriteArchiveJSON(const NkArchive& archive,
                              NkString& out,
                              bool pretty,
                              int indent,
                              int level) noexcept;

        // -------------------------------------------------------------------------
        // FONCTION : WriteValueJSON
        // DESCRIPTION : Écrit une valeur scalaire NkArchiveValue en format JSON
        // -------------------------------------------------------------------------
        /**
         * @brief Sérialise une valeur scalaire en texte JSON
         * @param v Valeur scalaire à écrire
         * @param out Référence vers le buffer NkString de sortie
         * @note noexcept : garantie de non-levée d'exception
         *
         * Formats de sortie par type :
         *  - NK_VALUE_NULL    : null (mot-clé JSON, minuscules)
         *  - NK_VALUE_BOOL    : true/false (sans quotes, minuscules)
         *  - NK_VALUE_INT64   : 123 (décimal, sans quotes)
         *  - NK_VALUE_UINT64  : 456 (décimal, sans quotes)
         *  - NK_VALUE_FLOAT64 : 3.14 (notation décimale, sans quotes)
         *  - NK_VALUE_STRING  : "text" (double-quoted avec échappement via NkJSONEscapeString)
         *
         * @note Utilise la représentation textuelle canonique stockée dans v.text
         *       pour garantir la cohérence avec le reste du système de sérialisation.
         *
         * @example
         * @code
         * NkString buf;
         * WriteValueJSON(NkArchiveValue::FromBool(true), buf);   // buf += "true"
         * WriteValueJSON(NkArchiveValue::FromString("hi"), buf); // buf += "\"hi\""
         * @endcode
         */
        void WriteValueJSON(const NkArchiveValue& v, NkString& out) noexcept {
            switch (v.type) {
                case NkArchiveValueType::NK_VALUE_NULL:
                    out.Append("null");
                    break;
                case NkArchiveValueType::NK_VALUE_BOOL:
                case NkArchiveValueType::NK_VALUE_INT64:
                case NkArchiveValueType::NK_VALUE_UINT64:
                case NkArchiveValueType::NK_VALUE_FLOAT64:
                    out.Append(v.text);
                    break;
                case NkArchiveValueType::NK_VALUE_STRING:
                default:
                    out.Append('"');
                    out.Append(NkJSONEscapeString(v.text.View()));
                    out.Append('"');
                    break;
            }
        }

        // -------------------------------------------------------------------------
        // FONCTION : WriteArchiveJSON
        // DESCRIPTION : Écrit un archive complet en JSON avec gestion récursive
        // -------------------------------------------------------------------------
        /**
         * @brief Sérialise un NkArchive entier en texte JSON
         * @param archive Archive source à sérialiser
         * @param out Référence vers le buffer NkString de sortie
         * @param pretty true pour indentation lisible, false pour format compact
         * @param indent Nombre d'espaces par niveau d'indentation
         * @param level Niveau d'indentation de départ pour ce bloc
         * @note noexcept : garantie de non-levée d'exception
         *
         * Algorithme :
         *  1. Ouvre l'objet avec '{'
         *  2. Pour chaque entrée :
         *     a. Ajoute saut de ligne + indentation si pretty=true
         *     b. Écrit la clé échappée entre quotes : "key"
         *     c. Ajoute ':' + espace optionnel si pretty=true
         *     d. Appel récursif à WriteNodeJSON pour la valeur
         *     e. Ajoute ',' sauf pour la dernière entrée
         *  3. Ferme l'objet avec '}' + saut de ligne + indentation si pretty=true et non-vide
         *
         * @note Fonction récursive mutuelle avec WriteNodeJSON.
         *       La profondeur maximale est limitée par la pile d'appels (typiquement <100).
         *
         * @example
         * @code
         * NkArchive arc;
         * arc.SetInt32("a", 1);
         * arc.SetString("b", "test");
         *
         * NkString out;
         * WriteArchiveJSON(arc, out, true, 2, 0);
         * // out = "{\n  \"a\": 1,\n  \"b\": \"test\"\n}"
         * @endcode
         */
        void WriteArchiveJSON(const NkArchive& archive,
                              NkString& out,
                              bool pretty,
                              int indent,
                              int level) noexcept {
            const auto& entries = archive.Entries();
            out.Append('{');
            for (nk_size i = 0; i < entries.Size(); ++i) {
                if (pretty) {
                    out.Append('\n');
                    Indent(out, level + 1, indent);
                }
                out.Append('"');
                out.Append(NkJSONEscapeString(entries[i].key.View()));
                out.Append('"');
                out.Append(':');
                if (pretty) {
                    out.Append(' ');
                }
                WriteNodeJSON(entries[i].node, out, pretty, indent, level + 1);
                if (i + 1 < entries.Size()) {
                    out.Append(',');
                }
            }
            if (pretty && !entries.Empty()) {
                out.Append('\n');
                Indent(out, level, indent);
            }
            out.Append('}');
        }

        // -------------------------------------------------------------------------
        // FONCTION : WriteNodeJSON
        // DESCRIPTION : Écrit un nœud NkArchiveNode (scalaire/objet/tableau) en JSON
        // -------------------------------------------------------------------------
        /**
         * @brief Sérialise un nœud complet en format JSON
         * @param node Nœud à sérialiser (scalaire, objet ou tableau)
         * @param out Référence vers le buffer NkString de sortie
         * @param pretty true pour indentation lisible, false pour format compact
         * @param indent Nombre d'espaces par niveau d'indentation
         * @param level Niveau d'indentation actuel
         * @note noexcept : garantie de non-levée d'exception
         *
         * Logique par type de nœud :
         *  - Scalar : appel à WriteValueJSON pour écrire la valeur
         *  - Object : appel récursif à WriteArchiveJSON avec level inchangé
         *  - Array  : écriture de '[' + éléments séparés par ',' + ']'
         *             avec indentation si pretty=true
         *
         * Gestion des tableaux :
         *  - Chaque élément est écrit avec level+1 pour indentation correcte
         *  - Virgule ajoutée entre éléments, pas après le dernier
         *  - Saut de ligne + indentation avant chaque élément si pretty=true
         *
         * @note L'échappement JSON est appliqué via NkJSONEscapeString() sur les strings.
         *
         * @example Sortie pour un nœud tableau :
         * @code
         * // Input: node={array: [1, "two", {obj: true}]}
         * // Output (pretty=true, indent=2):
         * // [
         * //   1,
         * //   "two",
         * //   {
         * //     "obj": true
         * //   }
         * // ]
         * @endcode
         */
        void WriteNodeJSON(const NkArchiveNode& node,
                           NkString& out,
                           bool pretty,
                           int indent,
                           int level) noexcept {
            if (node.IsScalar()) {
                WriteValueJSON(node.value, out);
            } else if (node.IsObject()) {
                WriteArchiveJSON(*node.object, out, pretty, indent, level);
            } else if (node.IsArray()) {
                out.Append('[');
                for (nk_size i = 0; i < node.array.Size(); ++i) {
                    if (pretty) {
                        out.Append('\n');
                        Indent(out, level + 1, indent);
                    }
                    WriteNodeJSON(node.array[i], out, pretty, indent, level + 1);
                    if (i + 1 < node.array.Size()) {
                        out.Append(',');
                    }
                }
                if (pretty && !node.array.Empty()) {
                    out.Append('\n');
                    Indent(out, level, indent);
                }
                out.Append(']');
            }
        }


    } // namespace anonyme


    // =============================================================================
    // NkJSONWriter — IMPLÉMENTATION DES MÉTHODES PUBLIQUES
    // =============================================================================

    // -------------------------------------------------------------------------
    // MÉTHODE : WriteArchive (avec buffer de sortie)
    // DESCRIPTION : Sérialise un archive en JSON dans un NkString fourni
    // -------------------------------------------------------------------------
    /**
     * @brief Implémentation de WriteArchive avec buffer de sortie
     * @param archive Archive source à sérialiser
     * @param outJson Référence vers NkString pour recevoir la sortie JSON
     * @param pretty true pour indentation lisible, false pour format compact
     * @param indentSpaces Nombre d'espaces par niveau d'indentation
     * @return true si succès (toujours true dans l'implémentation actuelle)
     *
     * Étapes :
     *  1. Vide le buffer de sortie via Clear()
     *  2. Appel à WriteArchiveJSON avec level=0 pour écrire le contenu
     *  3. Retourne true pour indiquer le succès
     *
     * @note Méthode noexcept-friendly : aucune exception levée.
     * @note Le buffer outJson est réinitialisé : appelant n'a pas besoin de Clear() préalable.
     */
    nk_bool NkJSONWriter::WriteArchive(const NkArchive& archive,
                                       NkString& outJson,
                                       nk_bool pretty,
                                       nk_int32 indentSpaces) {
        outJson.Clear();
        WriteArchiveJSON(archive, outJson, pretty, static_cast<int>(indentSpaces), 0);
        return true;
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : WriteArchive (retour par valeur)
    // DESCRIPTION : Sérialise un archive en JSON et retourne le résultat
    // -------------------------------------------------------------------------
    /**
     * @brief Implémentation de WriteArchive avec retour par valeur
     * @param archive Archive source à sérialiser
     * @param pretty true pour indentation lisible, false pour format compact
     * @param indentSpaces Nombre d'espaces par niveau d'indentation
     * @return NkString contenant le texte JSON généré
     *
     * Étapes :
     *  1. Crée un NkString local pour le buffer
     *  2. Délègue à WriteArchive(archive, out, pretty, indentSpaces)
     *  3. Retourne le NkString par move sémantique (optimisé par le compilateur)
     *
     * @note Préférer la version avec buffer pour les écritures répétées (évite allocations).
     * @note Le retour par valeur est optimisé par RVO/NRVO en C++17+.
     */
    NkString NkJSONWriter::WriteArchive(const NkArchive& archive,
                                        nk_bool pretty,
                                        nk_int32 indentSpaces) {
        NkString out;
        WriteArchive(archive, out, pretty, indentSpaces);
        return out;
    }


} // namespace nkentseu


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================