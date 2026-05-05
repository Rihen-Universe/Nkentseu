// =============================================================================
// NKSerialization/XML/NkXMLWriter.cpp
// Implémentation du sérialiseur XML pour NkArchive.
//
// Design :
//  - Fonctions internes dans namespace anonyme pour encapsulation
//  - Écriture récursive avec gestion configurable de l'indentation
//  - Échappement XML-safe des 5 caractères spéciaux standards
//  - Attribution du type sémantique pour préservation round-trip
//  - Aucune allocation dynamique inutile : buffer réutilisé par l'appelant
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "NKSerialization/XML/NkXMLWriter.h"


namespace nkentseu {


    // =============================================================================
    // NAMESPACE ANONYME : HELPERS INTERNES
    // =============================================================================
    // Ces fonctions sont privées au TU pour éviter la pollution de l'espace de noms.
    // Elles implémentent la logique basique de formatage XML.

    namespace {


        // -------------------------------------------------------------------------
        // FONCTION : NkAppendIndent
        // DESCRIPTION : Ajoute des espaces d'indentation conditionnels au buffer
        // -------------------------------------------------------------------------
        /**
         * @brief Ajoute l'indentation XML si le mode pretty est activé
         * @param out Référence vers le buffer NkString de sortie
         * @param pretty true pour activer l'indentation, false pour format compact
         * @param spaces Nombre d'espaces par niveau d'indentation
         * @param level Niveau d'indentation actuel (0 = racine)
         * @note noexcept : garantie de non-levée d'exception
         *
         * Comportement :
         *  - Si pretty=false : retour immédiat (aucune modification)
         *  - Si pretty=true : ajoute '\n' puis level*spaces espaces
         *  - Permet un contrôle fin du formatage sans duplication de code
         *
         * @note Performance : boucle simple O(level), négligeable pour depth < 100
         *
         * @example
         * @code
         * NkString buf;
         * NkAppendIndent(buf, true, 2, 0);   // buf += "\n"
         * NkAppendIndent(buf, true, 2, 2);   // buf += "\n    " (4 espaces)
         * NkAppendIndent(buf, false, 2, 5);  // buf inchangé (mode compact)
         * @endcode
         */
        void NkAppendIndent(NkString& out,
                            nk_bool pretty,
                            nk_int32 spaces,
                            nk_int32 level) noexcept {
            if (!pretty) {
                return;
            }
            out.Append('\n');
            for (nk_int32 i = 0; i < level * spaces; ++i) {
                out.Append(' ');
            }
        }

        // -------------------------------------------------------------------------
        // FONCTION : NkXMLEscape
        // DESCRIPTION : Échappe une chaîne pour conformité XML standard
        // -------------------------------------------------------------------------
        /**
         * @brief Échappe les caractères spéciaux XML dans une chaîne
         * @param input Vue de chaîne source à échapper
         * @return NkString contenant la chaîne échappée
         * @note noexcept : garantie de non-levée d'exception
         *
         * Caractères échappés (conformité XML 1.0) :
         *  - & → &amp;   (ampersand, doit être échappé en premier)
         *  - < → &lt;    (less-than, début de tag)
         *  - > → &gt;    (greater-than, fin de tag optionnelle)
         *  - " → &quot;  (double quote, pour attributs)
         *  - ' → &apos;  (single quote/apostrophe, pour attributs)
         *
         * @note Pré-allocation via Reserve() pour éviter les réallocations multiples.
         * @note Complexité O(n) où n = longueur de la chaîne d'entrée.
         *
         * @example
         * @code
         * NkXMLEscape("Tom & Jerry")      → "Tom &amp; Jerry"
         * NkXMLEscape("<hero>")           → "&lt;hero&gt;"
         * NkXMLEscape("It's \"great\"")   → "It&apos;s &quot;great&quot;"
         * @endcode
         */
        NkString NkXMLEscape(NkStringView input) noexcept {
            NkString out;
            out.Reserve(input.Length() + 8u);
            for (nk_size i = 0; i < input.Length(); ++i) {
                switch (input[i]) {
                    case '&':
                        out.Append("&amp;");
                        break;
                    case '<':
                        out.Append("&lt;");
                        break;
                    case '>':
                        out.Append("&gt;");
                        break;
                    case '"':
                        out.Append("&quot;");
                        break;
                    case '\'':
                        out.Append("&apos;");
                        break;
                    default:
                        out.Append(input[i]);
                        break;
                }
            }
            return out;
        }

        // -------------------------------------------------------------------------
        // FONCTION : NkTypeStr
        // DESCRIPTION : Convertit un NkArchiveValueType en string XML type=
        // -------------------------------------------------------------------------
        /**
         * @brief Convertit un type de valeur en chaîne pour l'attribut XML type=
         * @param t Type de valeur NkArchiveValueType à convertir
         * @return Pointeur vers string statique représentant le type
         * @note noexcept : garantie de non-levée d'exception
         * @note Retourne toujours une string valide (défaut: "string")
         *
         * Correspondances type → string :
         *  - NK_VALUE_NULL    → "null"
         *  - NK_VALUE_BOOL    → "bool"
         *  - NK_VALUE_INT64   → "int64"
         *  - NK_VALUE_UINT64  → "uint64"
         *  - NK_VALUE_FLOAT64 → "float64"
         *  - NK_VALUE_STRING  → "string" (et tout type inconnu)
         *
         * @note Utilisé pour l'attribut type= dans les éléments <entry>.
         *       Permet la reconstruction exacte du type lors du parsing.
         *
         * @example
         * @code
         * NkTypeStr(NK_VALUE_BOOL)    → "bool"
         * NkTypeStr(NK_VALUE_FLOAT64) → "float64"
         * NkTypeStr(NK_VALUE_STRING)  → "string"
         * @endcode
         */
        const char* NkTypeStr(NkArchiveValueType t) noexcept {
            switch (t) {
                case NkArchiveValueType::NK_VALUE_NULL:
                    return "null";
                case NkArchiveValueType::NK_VALUE_BOOL:
                    return "bool";
                case NkArchiveValueType::NK_VALUE_INT64:
                    return "int64";
                case NkArchiveValueType::NK_VALUE_UINT64:
                    return "uint64";
                case NkArchiveValueType::NK_VALUE_FLOAT64:
                    return "float64";
                default:
                    return "string";
            }
        }

        // -------------------------------------------------------------------------
        // DÉCLARATIONS FORWARD : Fonctions récursives mutuellement dépendantes
        // -------------------------------------------------------------------------
        void NkWriteArchiveXML(const NkArchive& archive,
                               NkString& out,
                               nk_bool pretty,
                               nk_int32 spaces,
                               nk_int32 level) noexcept;

        void NkWriteNodeXML(NkStringView key,
                            const NkArchiveNode& node,
                            NkString& out,
                            nk_bool pretty,
                            nk_int32 spaces,
                            nk_int32 level) noexcept;

        // -------------------------------------------------------------------------
        // FONCTION : NkWriteNodeXML
        // DESCRIPTION : Écrit un nœud NkArchiveNode en format XML
        // -------------------------------------------------------------------------
        /**
         * @brief Sérialise un nœud complet avec sa clé en format XML
         * @param key Nom de la clé pour ce nœud (attribut key=)
         * @param node Nœud à sérialiser (scalaire, objet ou tableau)
         * @param out Référence vers le buffer NkString de sortie
         * @param pretty true pour indentation lisible, false pour compact
         * @param spaces Nombre d'espaces par niveau d'indentation
         * @param level Niveau d'indentation actuel
         * @note noexcept : garantie de non-levée d'exception
         *
         * Logique par type de nœud :
         *  - Scalar : <entry key="..." type="...">value</entry>
         *  - Object : <object key="...">[enfants récursifs]</object>
         *  - Array  : <array key="..." count="N">[éléments indexés]</array>
         *
         * Gestion des tableaux :
         *  - Chaque élément reçoit un attribut key= avec son index (0, 1, 2...)
         *  - Les éléments sont écrits avec level+1 pour indentation correcte
         *  - L'attribut count= permet validation rapide côté lecteur
         *
         * @note L'échappement XML est appliqué via NkXMLEscape() sur key et value.
         * @note L'attribut type= est omis pour les valeurs null (contenu vide).
         *
         * @example Sortie pour un nœud objet :
         * @code
         * // Input: key="user", node={object: {name: "Alice", age: 30}}
         * // Output (pretty=true, spaces=2):
         * //   <object key="user">
         * //     <entry key="name" type="string">Alice</entry>
         * //     <entry key="age" type="int64">30</entry>
         * //   </object>
         * @endcode
         */
        void NkWriteNodeXML(NkStringView key,
                            const NkArchiveNode& node,
                            NkString& out,
                            nk_bool pretty,
                            nk_int32 spaces,
                            nk_int32 level) noexcept {
            if (node.IsScalar()) {
                // Scalaire : élément <entry> avec attributs key et type
                NkAppendIndent(out, pretty, spaces, level);
                out.Append("<entry key=\"");
                out.Append(NkXMLEscape(key));
                out.Append("\" type=\"");
                out.Append(NkTypeStr(node.value.type));
                out.Append("\">");
                if (node.value.type != NkArchiveValueType::NK_VALUE_NULL) {
                    out.Append(NkXMLEscape(node.value.text.View()));
                }
                out.Append("</entry>");

            } else if (node.IsObject()) {
                // Objet : élément <object> avec enfants récursifs
                NkAppendIndent(out, pretty, spaces, level);
                out.Append("<object key=\"");
                out.Append(NkXMLEscape(key));
                out.Append("\">");
                NkWriteArchiveXML(*node.object, out, pretty, spaces, level + 1);
                NkAppendIndent(out, pretty, spaces, level);
                out.Append("</object>");

            } else if (node.IsArray()) {
                // Tableau : élément <array> avec éléments indexés
                NkAppendIndent(out, pretty, spaces, level);
                out.Append("<array key=\"");
                out.Append(NkXMLEscape(key));
                out.Append("\" count=\"");
                out.Append(NkString::Fmtf("%zu", node.array.Size()));
                out.Append("\">");
                for (nk_size i = 0; i < node.array.Size(); ++i) {
                    NkString idx = NkString::Fmtf("%zu", i);
                    NkWriteNodeXML(idx.View(), node.array[i], out, pretty, spaces, level + 1);
                }
                NkAppendIndent(out, pretty, spaces, level);
                out.Append("</array>");
            }
        }

        // -------------------------------------------------------------------------
        // FONCTION : NkWriteArchiveXML
        // DESCRIPTION : Écrit un archive complet en XML avec gestion récursive
        // -------------------------------------------------------------------------
        /**
         * @brief Sérialise un NkArchive entier en texte XML
         * @param archive Archive source à sérialiser
         * @param out Référence vers le buffer NkString de sortie
         * @param pretty true pour indentation lisible, false pour compact
         * @param spaces Nombre d'espaces par niveau d'indentation
         * @param level Niveau d'indentation de départ pour ce bloc
         * @note noexcept : garantie de non-levée d'exception
         *
         * Algorithme :
         *  1. Récupère la liste des entrées via Entries()
         *  2. Pour chaque entrée : appelle NkWriteNodeXML avec la clé et le nœud
         *  3. L'ordre d'insertion est préservé (NkVector ordonné)
         *
         * @note Fonction récursive mutuelle avec NkWriteNodeXML.
         *       La profondeur maximale est limitée par la pile d'appels (typiquement <100).
         *
         * @example
         * @code
         * NkArchive arc;
         * arc.SetInt32("a", 1);
         * arc.SetString("b", "test");
         *
         * NkString out;
         * NkWriteArchiveXML(arc, out, true, 2, 0);
         * // out = "\n<entry key=\"a\" type=\"int64\">1</entry>\n<entry key=\"b\" type=\"string\">test</entry>"
         * @endcode
         */
        void NkWriteArchiveXML(const NkArchive& archive,
                               NkString& out,
                               nk_bool pretty,
                               nk_int32 spaces,
                               nk_int32 level) noexcept {
            const auto& entries = archive.Entries();
            for (nk_size i = 0; i < entries.Size(); ++i) {
                NkWriteNodeXML(entries[i].key.View(), entries[i].node, out, pretty, spaces, level);
            }
        }


    } // namespace anonyme


    // =============================================================================
    // NkXMLWriter — IMPLÉMENTATION DES MÉTHODES PUBLIQUES
    // =============================================================================

    // -------------------------------------------------------------------------
    // MÉTHODE : WriteArchive (avec buffer de sortie)
    // DESCRIPTION : Sérialise un archive en XML dans un NkString fourni
    // -------------------------------------------------------------------------
    /**
     * @brief Implémentation de WriteArchive avec buffer de sortie
     * @param archive Archive source à sérialiser
     * @param outXml Référence vers NkString pour recevoir la sortie XML
     * @param pretty true pour indentation lisible, false pour format compact
     * @param indentSpaces Nombre d'espaces par niveau d'indentation
     * @return true si succès (toujours true dans l'implémentation actuelle)
     *
     * Étapes :
     *  1. Vide le buffer de sortie via Clear()
     *  2. Ajoute la racine <archive>
     *  3. Appelle NkWriteArchiveXML avec level=1 pour écrire le contenu
     *  4. Ajoute un saut de ligne final si pretty=true et archive non-vide
     *  5. Ferme avec </archive>
     *  6. Retourne true pour indiquer le succès
     *
     * @note Méthode noexcept-friendly : aucune exception levée.
     * @note Le buffer outXml est réinitialisé : appelant n'a pas besoin de Clear() préalable.
     */
    nk_bool NkXMLWriter::WriteArchive(const NkArchive& archive,
                                      NkString& outXml,
                                      nk_bool pretty,
                                      nk_int32 indentSpaces) {
        outXml.Clear();
        outXml.Append("<archive>");
        NkWriteArchiveXML(archive, outXml, pretty, indentSpaces, 1);
        if (pretty && !archive.Empty()) {
            outXml.Append('\n');
        }
        outXml.Append("</archive>");
        return true;
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : WriteArchive (retour par valeur)
    // DESCRIPTION : Sérialise un archive en XML et retourne le résultat
    // -------------------------------------------------------------------------
    /**
     * @brief Implémentation de WriteArchive avec retour par valeur
     * @param archive Archive source à sérialiser
     * @param pretty true pour indentation lisible, false pour format compact
     * @param indentSpaces Nombre d'espaces par niveau d'indentation
     * @return NkString contenant le texte XML généré
     *
     * Étapes :
     *  1. Crée un NkString local pour le buffer
     *  2. Ajoute la racine <archive>
     *  3. Appelle NkWriteArchiveXML pour remplir le buffer
     *  4. Retourne le NkString par move sémantique (optimisé par le compilateur)
     *
     * @note Préférer la version avec buffer pour les écritures répétées (évite allocations).
     * @note Le retour par valeur est optimisé par RVO/NRVO en C++17+.
     */
    NkString NkXMLWriter::WriteArchive(const NkArchive& archive,
                                       nk_bool pretty,
                                       nk_int32 indentSpaces) {
        NkString out;
        out.Append("<archive>");
        NkWriteArchiveXML(archive, out, pretty, indentSpaces, 1);
        return out;
    }


} // namespace nkentseu


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================