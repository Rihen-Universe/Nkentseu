// =============================================================================
// NKSerialization/YAML/NkYAMLWriter.cpp
// Implémentation du sérialiseur YAML pour NkArchive.
//
// Design :
//  - Fonctions internes dans namespace anonyme pour encapsulation
//  - Écriture récursive avec gestion précise de l'indentation
//  - Échappement YAML-safe des chaînes via single quotes
//  - Détection compile-time des types scalaires pour optimisation
//  - Aucune allocation dynamique inutile : buffer réutilisé par l'appelant
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "NKSerialization/YAML/NkYAMLWriter.h"

// -------------------------------------------------------------------------
// EN-TÊTES STANDARDS POUR OPÉRATIONS SUR LES CARACTÈRES
// -------------------------------------------------------------------------
// Utilisation minimale pour la détection de caractères alphanumériques.

#include <cctype>


namespace nkentseu {


    // =============================================================================
    // NAMESPACE ANONYME : HELPERS INTERNES
    // =============================================================================
    // Ces fonctions sont privées au TU pour éviter la pollution de l'espace de noms.
    // Elles implémentent la logique basique de formatage YAML.

    namespace {


        // -------------------------------------------------------------------------
        // FONCTION : NkYAMLEscapeString
        // DESCRIPTION : Échappe une chaîne pour YAML en utilisant des single quotes
        // -------------------------------------------------------------------------
        /**
         * @brief Échappe une chaîne pour YAML avec single quotes
         * @param s Vue de chaîne source à échapper
         * @return NkString contenant la chaîne échappée entourée de quotes
         * @note noexcept : garantie de non-levée d'exception
         *
         * Règle YAML pour single quotes :
         *  - La chaîne est entourée de '...'
         *  - Chaque ' interne est doublé : ' → ''
         *  - Aucun autre caractère n'a besoin d'échappement dans single quotes
         *
         * @example
         * @code
         * NkYAMLEscapeString("It's fine") → "'It''s fine'"
         * NkYAMLEscapeString("normal")    → "'normal'"
         * @endcode
         */
        NkString NkYAMLEscapeString(NkStringView s) noexcept {
            NkString out;
            out.Append('\'');
            for (nk_size i = 0; i < s.Length(); ++i) {
                if (s[i] == '\'') {
                    out.Append("''");
                } else {
                    out.Append(s[i]);
                }
            }
            out.Append('\'');
            return out;
        }

        // -------------------------------------------------------------------------
        // FONCTION : NkNeedsQuoting
        // DESCRIPTION : Détermine si une clé YAML nécessite des guillemets
        // -------------------------------------------------------------------------
        /**
         * @brief Vérifie si une clé doit être quotée en YAML
         * @param key Vue de chaîne représentant la clé
         * @return true si la clé contient des caractères spéciaux, false sinon
         * @note noexcept : garantie de non-levée d'exception
         *
         * Règles de quotation :
         *  - Clé vide → toujours quotée
         *  - Caractères autorisés sans quotes : [a-zA-Z0-9_-]
         *  - Tout autre caractère (espace, ponctuation, unicode) → nécessite quotes
         *
         * @note Cette fonction est conservative : mieux vaut trop quoter que pas assez.
         *
         * @example
         * @code
         * NkNeedsQuoting("simple_key")      → false
         * NkNeedsQuoting("key with space")  → true
         * NkNeedsQuoting("key:colon")       → true
         * NkNeedsQuoting("")                → true
         * @endcode
         */
        bool NkNeedsQuoting(NkStringView key) noexcept {
            if (key.Empty()) {
                return true;
            }
            for (nk_size i = 0; i < key.Length(); ++i) {
                char c = key[i];
                bool isAlphaNum = (c >= 'a' && c <= 'z')
                    || (c >= 'A' && c <= 'Z')
                    || (c >= '0' && c <= '9');
                bool isAllowedSpecial = (c == '_' || c == '-');
                if (!isAlphaNum && !isAllowedSpecial) {
                    return true;
                }
            }
            return false;
        }

        // -------------------------------------------------------------------------
        // FONCTION : NkAppendIndent
        // DESCRIPTION : Ajoute des espaces d'indentation au buffer de sortie
        // -------------------------------------------------------------------------
        /**
         * @brief Ajoute l'indentation YAML pour un niveau donné
         * @param out Référence vers le buffer NkString de sortie
         * @param level Niveau d'indentation (0 = racine, 1 = premier enfant, etc.)
         * @note noexcept : garantie de non-levée d'exception
         *
         * Convention d'indentation :
         *  - 2 espaces par niveau (standard YAML lisible)
         *  - Pas de tabulations : évite les problèmes de rendu multi-éditeur
         *
         * @note Performance : boucle simple O(level), négligeable pour depth < 100
         *
         * @example
         * @code
         * NkString buf;
         * NkAppendIndent(buf, 0);  // buf = ""
         * NkAppendIndent(buf, 2);  // buf = "    " (4 espaces)
         * @endcode
         */
        void NkAppendIndent(NkString& out, int level) noexcept {
            for (int i = 0; i < level * 2; ++i) {
                out.Append(' ');
            }
        }

        // -------------------------------------------------------------------------
        // DÉCLARATIONS FORWARD : Fonctions récursives mutuellement dépendantes
        // -------------------------------------------------------------------------
        void NkWriteArchiveYAML(const NkArchive& arc, NkString& out, int level) noexcept;
        void NkWriteNodeYAML(NkStringView key, const NkArchiveNode& node, NkString& out, int level) noexcept;

        // -------------------------------------------------------------------------
        // FONCTION : NkWriteValueYAML
        // DESCRIPTION : Écrit une valeur scalaire NkArchiveValue en format YAML
        // -------------------------------------------------------------------------
        /**
         * @brief Sérialise une valeur scalaire en texte YAML
         * @param v Valeur scalaire à écrire
         * @param out Référence vers le buffer NkString de sortie
         * @note noexcept : garantie de non-levée d'exception
         *
         * Formats de sortie par type :
         *  - NK_VALUE_NULL    : ~ (tilde YAML standard)
         *  - NK_VALUE_BOOL    : true/false (sans quotes)
         *  - NK_VALUE_INT64   : 123 (décimal, sans quotes)
         *  - NK_VALUE_UINT64  : 456 (décimal, sans quotes)
         *  - NK_VALUE_FLOAT64 : 3.14 (notation décimale, sans quotes)
         *  - NK_VALUE_STRING  : 'text' (single-quoted avec échappement)
         *
         * @note Utilise la représentation textuelle canonique stockée dans v.text
         *       pour garantir la cohérence avec le reste du système de sérialisation.
         *
         * @example
         * @code
         * NkString buf;
         * NkWriteValueYAML(NkArchiveValue::FromBool(true), buf);   // buf += "true"
         * NkWriteValueYAML(NkArchiveValue::FromString("hi"), buf); // buf += "'hi'"
         * @endcode
         */
        void NkWriteValueYAML(const NkArchiveValue& v, NkString& out) noexcept {
            switch (v.type) {
                case NkArchiveValueType::NK_VALUE_NULL:
                    out.Append('~');
                    break;
                case NkArchiveValueType::NK_VALUE_BOOL:
                case NkArchiveValueType::NK_VALUE_INT64:
                case NkArchiveValueType::NK_VALUE_UINT64:
                case NkArchiveValueType::NK_VALUE_FLOAT64:
                    out.Append(v.text);
                    break;
                case NkArchiveValueType::NK_VALUE_STRING:
                default:
                    out.Append(NkYAMLEscapeString(v.text.View()));
                    break;
            }
        }

        // -------------------------------------------------------------------------
        // FONCTION : NkWriteNodeYAML
        // DESCRIPTION : Écrit un nœud NkArchiveNode (scalaire/objet/tableau) en YAML
        // -------------------------------------------------------------------------
        /**
         * @brief Sérialise un nœud complet avec sa clé en format YAML
         * @param key Nom de la clé pour ce nœud
         * @param node Nœud à sérialiser (scalaire, objet ou tableau)
         * @param out Référence vers le buffer NkString de sortie
         * @param level Niveau d'indentation actuel
         * @note noexcept : garantie de non-levée d'exception
         *
         * Logique par type de nœud :
         *  - Scalar : "key: value\n" sur une ligne
         *  - Object : "key:\n" puis appel récursif NkWriteArchiveYAML avec level+1
         *  - Array  : "key:\n" puis pour chaque élément : "  - value\n"
         *
         * Gestion des tableaux imbriqués :
         *  - Les éléments scalaires sont écrits directement après "- "
         *  - Les éléments objets déclenchent un appel récursif avec level+2
         *  - Les éléments tableaux (nested arrays) sont simplifiés en "[]\n"
         *    (limitation connue : YAML block sequences dans block sequences)
         *
         * @note L'indentation est gérée par NkAppendIndent() avant chaque ligne.
         *
         * @example Sortie pour un nœud objet :
         * @code
         * // Input: key="user", node={object: {name: "Alice", age: 30}}
         * // Output:
         * // user:
         * //   name: 'Alice'
         * //   age: 30
         * @endcode
         */
        void NkWriteNodeYAML(NkStringView key, const NkArchiveNode& node, NkString& out, int level) noexcept {
            NkAppendIndent(out, level);

            // Écriture de la clé avec quotation si nécessaire
            if (NkNeedsQuoting(key)) {
                out.Append(NkYAMLEscapeString(key));
            } else {
                out.Append(key);
            }
            out.Append(':');

            // Traitement selon le type de nœud
            if (node.IsScalar()) {
                // Scalaire : valeur sur la même ligne
                out.Append(' ');
                NkWriteValueYAML(node.value, out);
                out.Append('\n');

            } else if (node.IsObject()) {
                // Objet : saut de ligne puis écriture récursive du sous-archive
                out.Append('\n');
                NkWriteArchiveYAML(*node.object, out, level + 1);

            } else if (node.IsArray()) {
                // Tableau : saut de ligne puis écriture de chaque élément
                out.Append('\n');
                for (nk_size i = 0; i < node.array.Size(); ++i) {
                    NkAppendIndent(out, level + 1);
                    out.Append("- ");
                    if (node.array[i].IsScalar()) {
                        NkWriteValueYAML(node.array[i].value, out);
                        out.Append('\n');
                    } else if (node.array[i].IsObject()) {
                        out.Append('\n');
                        NkWriteArchiveYAML(*node.array[i].object, out, level + 2);
                    } else {
                        // Tableau imbriqué : simplification en [] (limitation YAML block)
                        out.Append("[]\n");
                    }
                }
            }
        }

        // -------------------------------------------------------------------------
        // FONCTION : NkWriteArchiveYAML
        // DESCRIPTION : Écrit un archive complet en YAML avec gestion récursive
        // -------------------------------------------------------------------------
        /**
         * @brief Sérialise un NkArchive entier en texte YAML
         * @param arc Archive source à sérialiser
         * @param out Référence vers le buffer NkString de sortie
         * @param level Niveau d'indentation de départ pour ce bloc
         * @note noexcept : garantie de non-levée d'exception
         *
         * Algorithme :
         *  1. Récupère la liste des entrées via Entries()
         *  2. Pour chaque entrée : appelle NkWriteNodeYAML avec la clé et le nœud
         *  3. L'ordre d'insertion est préservé (NkVector ordonné)
         *
         * @note Fonction récursive mutuelle avec NkWriteNodeYAML.
         *       La profondeur maximale est limitée par la pile d'appels (typiquement <100).
         *
         * @example
         * @code
         * NkArchive arc;
         * arc.SetInt32("a", 1);
         * arc.SetString("b", "test");
         *
         * NkString out;
         * NkWriteArchiveYAML(arc, out, 0);
         * // out = "a: 1\nb: 'test'\n"
         * @endcode
         */
        void NkWriteArchiveYAML(const NkArchive& arc, NkString& out, int level) noexcept {
            const auto& entries = arc.Entries();
            for (nk_size i = 0; i < entries.Size(); ++i) {
                NkWriteNodeYAML(entries[i].key.View(), entries[i].node, out, level);
            }
        }


    } // namespace anonyme


    // =============================================================================
    // NkYAMLWriter — IMPLÉMENTATION DES MÉTHODES PUBLIQUES
    // =============================================================================

    // -------------------------------------------------------------------------
    // MÉTHODE : WriteArchive (avec buffer de sortie)
    // DESCRIPTION : Sérialise un archive en YAML dans un NkString fourni
    // -------------------------------------------------------------------------
    /**
     * @brief Implémentation de WriteArchive avec buffer de sortie
     * @param archive Archive source à sérialiser
     * @param outYaml Référence vers NkString pour recevoir la sortie
     * @return true si succès (toujours true dans l'implémentation actuelle)
     *
     * Étapes :
     *  1. Vide le buffer de sortie via Clear()
     *  2. Ajoute le marqueur de document YAML "---\n"
     *  3. Appelle NkWriteArchiveYAML avec level=0 pour écrire le contenu
     *  4. Retourne true pour indiquer le succès
     *
     * @note Méthode noexcept-friendly : aucune exception levée.
     * @note Le buffer outYaml est réinitialisé : appelant n'a pas besoin de Clear() préalable.
     */
    nk_bool NkYAMLWriter::WriteArchive(const NkArchive& archive, NkString& outYaml) {
        outYaml.Clear();
        outYaml.Append("---\n");
        NkWriteArchiveYAML(archive, outYaml, 0);
        return true;
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : WriteArchive (retour par valeur)
    // DESCRIPTION : Sérialise un archive en YAML et retourne le résultat
    // -------------------------------------------------------------------------
    /**
     * @brief Implémentation de WriteArchive avec retour par valeur
     * @param archive Archive source à sérialiser
     * @return NkString contenant le texte YAML généré
     *
     * Étapes :
     *  1. Crée un NkString local pour le buffer
     *  2. Ajoute le marqueur "---\n"
     *  3. Appelle NkWriteArchiveYAML pour remplir le buffer
     *  4. Retourne le NkString par move sémantique (optimisé par le compilateur)
     *
     * @note Préférer la version avec buffer pour les écritures répétées (évite allocations).
     * @note Le retour par valeur est optimisé par RVO/NRVO en C++17+.
     */
    NkString NkYAMLWriter::WriteArchive(const NkArchive& archive) {
        NkString out;
        out.Append("---\n");
        NkWriteArchiveYAML(archive, out, 0);
        return out;
    }


} // namespace nkentseu


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================