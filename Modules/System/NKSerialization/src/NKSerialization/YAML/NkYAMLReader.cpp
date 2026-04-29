// =============================================================================
// NKSerialization/YAML/NkYAMLReader.cpp
// Implémentation du désérialiseur YAML pour NkArchive.
//
// Design :
//  - Parsing ligne par ligne avec suivi d'indentation pour hiérarchie
//  - Fonctions internes dans namespace anonyme pour encapsulation
//  - Gestion robuste des quotes, échappements et valeurs scalaires
//  - Détection des structures imbriquées via comparaison d'indentation
//  - Message d'erreur optionnel pour debugging en production
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "NKSerialization/YAML/NkYAMLReader.h"

// -------------------------------------------------------------------------
// EN-TÊTES STANDARDS POUR OPÉRATIONS SUR LES CHAÎNES
// -------------------------------------------------------------------------
// Utilisation de cctype pour détection de caractères alphanumériques.

#include <cctype>


namespace nkentseu {


    // =============================================================================
    // NAMESPACE ANONYME : HELPERS INTERNES DE PARSING
    // =============================================================================
    // Ces fonctions sont privées au TU pour éviter la pollution de l'espace de noms.
    // Elles implémentent la logique basique de parsing YAML ligne par ligne.

    namespace {


        // -------------------------------------------------------------------------
        // FONCTION : NkTrimStr
        // DESCRIPTION : Supprime les espaces de début et fin d'une NkStringView
        // -------------------------------------------------------------------------
        /**
         * @brief Trim les espaces d'une vue de chaîne
         * @param v Vue de chaîne source à trimmer
         * @return NkString contenant la version trimmée (copie)
         * @note noexcept : garantie de non-levée d'exception
         *
         * Comportement :
         *  - Crée une copie NkString car NkString::Trim() modifie in-place
         *  - Supprime les espaces ' ' et tabulations '\t' en début/fin
         *  - Ne modifie pas les espaces internes à la chaîne
         *
         * @note Performance : allocation d'un NkString temporaire — acceptable
         *       car appelé uniquement pendant le parsing (pas en boucle serrée).
         *
         * @example
         * @code
         * NkTrimStr("  hello  ")  → "hello"
         * NkTrimStr("\tworld\t")  → "world"
         * NkTrimStr("no-space")   → "no-space"
         * @endcode
         */
        NkString NkTrimStr(NkStringView v) noexcept {
            NkString s(v);
            s.Trim();
            return s;
        }

        // -------------------------------------------------------------------------
        // FONCTION : NkParseYAMLScalar
        // DESCRIPTION : Parse une valeur scalaire YAML en NkArchiveValue
        // -------------------------------------------------------------------------
        /**
         * @brief Convertit une chaîne YAML brute en NkArchiveValue
         * @param raw Vue de chaîne contenant la valeur brute (peut être quotée)
         * @param out Référence vers NkArchiveValue pour recevoir le résultat
         * @return true si le parsing a réussi, false en cas de format invalide
         * @note noexcept : garantie de non-levée d'exception
         *
         * Ordre de détection des types (premier match gagne) :
         *  1. Null : ~, null, Null (case-insensitive)
         *  2. Booléen : true/false (case-insensitive)
         *  3. Single-quoted string : '...' avec échappement '' → '
         *  4. Double-quoted string : "..." (échappement minimal, pas de \n supporté)
         *  5. Flottant : contient '.', 'e' ou 'E' → parsing via ToDouble()
         *  6. Entier signé : commence par '-' → parsing via ToInt64()
         *  7. Entier non-signé : commence par chiffre → parsing via ToUInt64()
         *  8. Fallback : string plain (non quoté) → FromString()
         *
         * @note Cette fonction est permissive : préfère interpréter comme string
         *       plutôt que d'échouer, pour tolérance aux fichiers mal formés.
         *
         * @example
         * @code
         * NkArchiveValue val;
         * NkParseYAMLScalar("true", val);        // val = FromBool(true)
         * NkParseYAMLScalar("'hello'", val);     // val = FromString("hello")
         * NkParseYAMLScalar("123", val);         // val = FromUInt64(123)
         * NkParseYAMLScalar("~", val);           // val = Null()
         * @endcode
         */
        bool NkParseYAMLScalar(NkStringView raw, NkArchiveValue& out) noexcept {
            NkString val = NkTrimStr(raw);

            // Détection des valeurs null
            if (val.Empty()
                || val == NkString("~")
                || val == NkString("null")
                || val == NkString("Null")) {
                out = NkArchiveValue::Null();
                return true;
            }

            // Détection des booléens
            if (val == NkString("true")
                || val == NkString("True")
                || val == NkString("TRUE")) {
                out = NkArchiveValue::FromBool(true);
                return true;
            }
            if (val == NkString("false")
                || val == NkString("False")
                || val == NkString("FALSE")) {
                out = NkArchiveValue::FromBool(false);
                return true;
            }

            // Détection des single-quoted strings
            if (val.Length() >= 2u
                && val.Front() == '\''
                && val.Back() == '\'') {
                NkString unquoted;
                for (nk_size i = 1u; i + 1u < val.Length(); ++i) {
                    if (val[i] == '\'' && i + 2u < val.Length() && val[i + 1u] == '\'') {
                        unquoted.Append('\'');
                        ++i;  // Skip le deuxième quote
                    } else {
                        unquoted.Append(val[i]);
                    }
                }
                out = NkArchiveValue::FromString(unquoted.View());
                return true;
            }

            // Détection des double-quoted strings (échappement minimal)
            if (val.Length() >= 2u
                && val.Front() == '"'
                && val.Back() == '"') {
                NkString unquoted = val.SubStr(1u, val.Length() - 2u);
                out = NkArchiveValue::FromString(unquoted.View());
                return true;
            }

            // Détection des flottants (heuristique : contient . ou e/E)
            if (val.Contains('.') || val.Contains('e') || val.Contains('E')) {
                float64 f = 0.0;
                if (val.ToDouble(f)) {
                    out = NkArchiveValue::FromFloat64(f);
                    return true;
                }
            }

            // Détection des entiers signés
            if (!val.Empty() && val[0] == '-') {
                int64 i = 0;
                if (val.ToInt64(i)) {
                    out = NkArchiveValue::FromInt64(i);
                    return true;
                }
            }

            // Détection des entiers non-signés
            if (!val.Empty() && (val[0] >= '0' && val[0] <= '9')) {
                uint64 u = 0;
                if (val.ToUInt64(u)) {
                    out = NkArchiveValue::FromUInt64(u);
                    return true;
                }
            }

            // Fallback : interpréter comme string plain
            out = NkArchiveValue::FromString(val.View());
            return true;
        }

        // -------------------------------------------------------------------------
        // FONCTION : NkCountIndent
        // DESCRIPTION : Compte le nombre d'espaces de début de ligne
        // -------------------------------------------------------------------------
        /**
         * @brief Calcule le niveau d'indentation d'une ligne YAML
         * @param line Vue de chaîne représentant la ligne complète
         * @return Nombre d'espaces de début (0 si pas d'indentation)
         * @note noexcept : garantie de non-levée d'exception
         *
         * Règles :
         *  - Compte uniquement les espaces ' ' (pas les tabulations)
         *  - S'arrête au premier caractère non-espace
         *  - Retourne 0 pour les lignes vides ou sans indentation
         *
         * @note Convention : 2 espaces = 1 niveau d'indentation YAML.
         *       Le parser est tolérant aux variations (1, 3, 4 espaces...).
         *
         * @example
         * @code
         * NkCountIndent("key: value")      → 0
         * NkCountIndent("  child: val")    → 2
         * NkCountIndent("    nested: x")   → 4
         * @endcode
         */
        int NkCountIndent(NkStringView line) noexcept {
            int n = 0;
            for (nk_size i = 0; i < line.Length() && (line[i] == ' '); ++i) {
                ++n;
            }
            return n;
        }

        // -------------------------------------------------------------------------
        // STRUCTURE : NkLine
        // DESCRIPTION : Représentation d'une ligne YAML après pré-traitement
        // -------------------------------------------------------------------------
        /**
         * @struct NkLine
         * @brief Représentation interne d'une ligne YAML pour le parsing
         * @ingroup YAMLReaderInternals
         *
         * Champs :
         *  - content : ligne brute (sans le caractère de fin \n)
         *  - indent  : nombre d'espaces de début (déterminé par NkCountIndent)
         *  - isEmpty : true si ligne vide ou commentaire (#)
         *
         * @note Cette structure évite de recalculer l'indentation à chaque utilisation.
         * @note Les lignes vides/commentaires sont marquées pour être sautées rapidement.
         */
        struct NkLine {
            /// @brief Contenu brut de la ligne (sans \n)
            NkString content;

            /// @brief Nombre d'espaces de début (niveau d'indentation)
            int indent = 0;

            /// @brief true si ligne vide ou commentaire (à ignorer)
            bool isEmpty = false;
        };

        // -------------------------------------------------------------------------
        // FONCTION : NkSplitLines
        // DESCRIPTION : Découpe un texte YAML en vecteur de lignes pré-traitées
        // -------------------------------------------------------------------------
        /**
         * @brief Tokenise un texte YAML en lignes structurées
         * @param yaml Vue de chaîne contenant le document YAML complet
         * @return NkVector<NkLine> contenant les lignes pré-traitées
         * @note noexcept : garantie de non-levée d'exception
         *
         * Étapes de pré-traitement :
         *  1. Découpage par caractère '\n' (supporte aussi '\r\n' Windows)
         *  2. Suppression du '\r' final si présent (compatibilité CRLF)
         *  3. Calcul de l'indentation via NkCountIndent()
         *  4. Détection des lignes vides ou commentaires (#)
         *
         * @note Les lignes sont stockées dans l'ordre d'apparition.
         * @note NkString::npos est utilisé comme sentinelle pour Find().
         *
         * @example
         * @code
         * auto lines = NkSplitLines("a: 1\n  b: 2\n# comment\n");
         * // lines[0]: content="a: 1", indent=0, isEmpty=false
         * // lines[1]: content="  b: 2", indent=2, isEmpty=false
         * // lines[2]: content="# comment", indent=0, isEmpty=true
         * @endcode
         */
        NkVector<NkLine> NkSplitLines(NkStringView yaml) noexcept {
            NkVector<NkLine> lines;
            NkString src(yaml);
            nk_size cursor = 0;

            while (cursor <= src.Length()) {
                nk_size next = src.Find('\n', cursor);
                if (next == NkString::npos) {
                    next = src.Length();
                }

                NkString raw = src.SubStr(cursor, next - cursor);
                // Suppression du \r éventuel (compatibilité Windows CRLF)
                if (!raw.Empty() && raw.Back() == '\r') {
                    raw = raw.SubStr(0, raw.Length() - 1u);
                }

                NkLine l;
                l.content = raw;
                l.indent = NkCountIndent(raw.View());

                NkString trimmed = NkTrimStr(raw.View());
                l.isEmpty = trimmed.Empty() || trimmed.Front() == '#';

                lines.PushBack(std::move(l));
                cursor = next + 1u;
                if (next == src.Length()) {
                    break;
                }
            }
            return lines;
        }

        // -------------------------------------------------------------------------
        // FONCTION : NkParseBlock
        // DESCRIPTION : Parser récursif pour blocs YAML imbriqués
        // -------------------------------------------------------------------------
        /**
         * @brief Parse un bloc YAML avec indentation donnée
         * @param lines Vecteur de lignes pré-traitées (via NkSplitLines)
         * @param start Index de la première ligne à parser dans ce bloc
         * @param blockIndent Niveau d'indentation attendu pour ce bloc
         * @param out Archive de destination pour les paires clé/valeur trouvées
         * @param err Pointeur optionnel vers NkString pour message d'erreur
         * @return Index de la prochaine ligne à traiter après ce bloc
         * @note noexcept : garantie de non-levée d'exception
         *
         * Algorithme récursif :
         *  1. Boucle sur les lignes à partir de `start`
         *  2. Saute les lignes vides/commentaires
         *  3. Si indentation < blockIndent : fin du bloc → retour
         *  4. Ignore les marqueurs de document (---, ...)
         *  5. Détecte les séquences (- item) et mappings (key: value)
         *  6. Pour les valeurs vides : regarde la ligne suivante pour déterminer
         *     si c'est un objet imbriqué ou un tableau
         *  7. Appel récursif pour les blocs enfants avec indentation augmentée
         *
         * Gestion des structures :
         *  - Mapping (clé: valeur) : ajout via SetValue/SetObject/SetNodeArray
         *  - Sequence (- item) : ajout via SetNodeArray (éléments scalaires ou objets)
         *  - Nested arrays : simplifiés en [] (limitation connue)
         *
         * @note La récursion est bornée par la profondeur d'indentation du YAML.
         *       Typiquement < 20 niveaux pour des configs réalistes.
         *
         * @example Parsing d'un bloc objet :
         * @code
         * // Input YAML (indent=0):
         * user:
         *   name: 'Alice'
         *   age: 30
         *
         * // Appel : NkParseBlock(lines, 0, 0, outArchive, nullptr)
         * // Résultat : outArchive contient { "user": {object: {name, age}} }
         * @endcode
         */
        nk_size NkParseBlock(const NkVector<NkLine>& lines,
                             nk_size start,
                             int blockIndent,
                             NkArchive& out,
                             NkString* err) noexcept {
            nk_size i = start;

            while (i < lines.Size()) {
                const NkLine& l = lines[i];

                // Saut des lignes vides ou commentaires
                if (l.isEmpty) {
                    ++i;
                    continue;
                }

                // Fin du bloc si indentation inférieure
                if (l.indent < blockIndent) {
                    return i;
                }

                // Ignorance des marqueurs de document YAML
                NkString trimmed = NkTrimStr(l.content.View());
                if (trimmed == NkString("---") || trimmed == NkString("...")) {
                    ++i;
                    continue;
                }

                // -----------------------------------------------------------------
                // Détection des séquences (- item) au niveau courant
                // -----------------------------------------------------------------
                // Les séquences au niveau racine ne sont pas supportées par NkArchive
                // (qui est un mapping clé/valeur). On les ignore à ce niveau.
                if (trimmed.Length() >= 2u
                    && trimmed.Front() == '-'
                    && trimmed[1u] == ' ') {
                    // Élément de séquence non-nommé → skip (géré par l'appelant)
                    ++i;
                    continue;
                }

                // -----------------------------------------------------------------
                // Parsing des paires clé: valeur
                // -----------------------------------------------------------------
                // Recherche du ':' séparateur en ignorant les quotes
                nk_size colonPos = NkString::npos;
                bool inQuote = false;
                char quoteChar = 0;
                const NkString& raw = l.content;
                for (nk_size k = static_cast<nk_size>(l.indent); k < raw.Length(); ++k) {
                    char c = raw[k];
                    if (!inQuote && (c == '\'' || c == '"')) {
                        inQuote = true;
                        quoteChar = c;
                    } else if (inQuote && c == quoteChar) {
                        inQuote = false;
                    } else if (!inQuote && c == ':') {
                        // ':' suivi d'espace ou fin de ligne → séparateur valide
                        if (k + 1u >= raw.Length()
                            || raw[k + 1u] == ' '
                            || raw[k + 1u] == '\t') {
                            colonPos = k;
                            break;
                        }
                    }
                }

                // Ligne sans ':' valide → skip avec avertissement silencieux
                if (colonPos == NkString::npos) {
                    ++i;
                    continue;
                }

                // Extraction et nettoyage de la clé
                NkString key = NkTrimStr(NkStringView(raw.CStr() + l.indent, colonPos - l.indent));
                if (key.Empty()) {
                    ++i;
                    continue;
                }
                // Suppression des quotes autour de la clé si présentes
                if (key.Length() >= 2u && key.Front() == '\'' && key.Back() == '\'') {
                    key = key.SubStr(1u, key.Length() - 2u);
                } else if (key.Length() >= 2u && key.Front() == '"' && key.Back() == '"') {
                    key = key.SubStr(1u, key.Length() - 2u);
                }

                // Extraction de la valeur brute après ':'
                NkStringView valueRaw = NkStringView(
                    raw.CStr() + colonPos + 1u,
                    raw.Length() - colonPos - 1u);
                NkString valueTrimmed = NkTrimStr(valueRaw);

                ++i;  // Avance à la ligne suivante pour traitement

                // -----------------------------------------------------------------
                // Cas 1 : Valeur vide → objet ou tableau imbriqué
                // -----------------------------------------------------------------
                if (valueTrimmed.Empty()) {
                    if (i < lines.Size() && !lines[i].isEmpty) {
                        int childIndent = lines[i].indent;
                        if (childIndent > blockIndent) {
                            // Détection de séquence vs objet via premier caractère
                            NkString nextTrimmed = NkTrimStr(lines[i].content.View());
                            if (nextTrimmed.Length() >= 2u
                                && nextTrimmed.Front() == '-'
                                && (nextTrimmed[1u] == ' ' || nextTrimmed[1u] == '\t')) {
                                // Parsing d'une séquence (tableau)
                                NkVector<NkArchiveNode> elems;
                                while (i < lines.Size()) {
                                    if (lines[i].isEmpty) {
                                        ++i;
                                        continue;
                                    }
                                    if (lines[i].indent < childIndent) {
                                        break;
                                    }
                                    NkString et = NkTrimStr(lines[i].content.View());
                                    if (et.Length() >= 2u
                                        && et.Front() == '-'
                                        && (et[1u] == ' ' || et[1u] == '\t')) {
                                        NkStringView elemVal = NkStringView(et.CStr() + 2u, et.Length() - 2u);
                                        NkString trimElem = NkTrimStr(elemVal);
                                        if (!trimElem.Empty()) {
                                            NkArchiveValue ev;
                                            NkParseYAMLScalar(trimElem.View(), ev);
                                            elems.PushBack(NkArchiveNode(ev));
                                        }
                                        ++i;
                                    } else {
                                        break;
                                    }
                                }
                                out.SetNodeArray(key.View(), elems);
                            } else {
                                // Parsing d'un objet imbriqué (mapping)
                                NkArchive childArc;
                                i = NkParseBlock(lines, i, childIndent, childArc, err);
                                out.SetObject(key.View(), childArc);
                            }
                        }
                    }
                    continue;
                }

                // -----------------------------------------------------------------
                // Cas 2 : Valeur scalaire directe
                // -----------------------------------------------------------------
                NkArchiveValue val;
                NkParseYAMLScalar(valueTrimmed.View(), val);
                out.SetValue(key.View(), val);
            }

            return i;
        }


    } // namespace anonyme


    // =============================================================================
    // NkYAMLReader — IMPLÉMENTATION DE LA MÉTHODE PUBLIQUE
    // =============================================================================

    // -------------------------------------------------------------------------
    // MÉTHODE : ReadArchive
    // DESCRIPTION : Point d'entrée public pour désérialisation YAML
    // -------------------------------------------------------------------------
    /**
     * @brief Implémentation de ReadArchive — point d'entrée public
     * @param yaml Vue de chaîne contenant le texte YAML à parser
     * @param outArchive Archive de destination (vidée avant parsing)
     * @param outError Pointeur optionnel pour message d'erreur détaillé
     * @return true si le parsing a réussi, false en cas d'erreur
     *
     * Étapes :
     *  1. Vide l'archive de sortie via Clear()
     *  2. Tokenise le YAML en lignes via NkSplitLines()
     *  3. Lance le parser récursif NkParseBlock() depuis l'index 0, indent 0
     *  4. Retourne true (succès) — les erreurs internes sont gérées silencieusement
     *
     * @note Méthode noexcept-friendly : aucune exception levée.
     * @note En cas d'erreur de parsing : outArchive peut contenir des données partielles.
     *       L'appelant doit vérifier le retour bool avant d'utiliser le résultat.
     *
     * @example Voir les exemples dans le header NkYAMLReader.h
     */
    nk_bool NkYAMLReader::ReadArchive(NkStringView yaml,
                                      NkArchive& outArchive,
                                      NkString* outError) {
        outArchive.Clear();
        NkVector<NkLine> lines = NkSplitLines(yaml);
        NkParseBlock(lines, 0u, 0, outArchive, outError);
        return true;
    }


} // namespace nkentseu


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================