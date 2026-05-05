// =============================================================================
// NKSerialization/JSON/NkJSONReader.cpp
// Implémentation du désérialiseur JSON pour NkArchive.
//
// Design :
//  - Parser récursif avec struct NkJSONParser pour état de parsing
//  - Gestion robuste des whitespace et erreurs de syntaxe JSON
//  - Parsing des scalaires, objets {} et tableaux [] avec récursion
//  - Message d'erreur optionnel avec position pour debugging
//  - Aucune allocation dynamique inutile : parsing in-place sur NkStringView
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/JSON/NkJSONValue.h"


namespace nkentseu {


    // =============================================================================
    // NAMESPACE ANONYME : PARSER JSON INTERNE
    // =============================================================================
    // Ce parser est privé au TU pour encapsulation.
    // Il implémente la logique de parsing JSON récursif avec gestion d'état.

    namespace {


        // -------------------------------------------------------------------------
        // STRUCTURE : NkJSONParser
        // DESCRIPTION : État de parsing pour le parser JSON récursif
        // -------------------------------------------------------------------------
        /**
         * @struct NkJSONParser
         * @brief État interne du parser JSON avec méthodes utilitaires
         * @ingroup JSONReaderInternals
         *
         * Membres :
         *  - text : vue sur le texte JSON source (non modifié)
         *  - pos  : position courante dans le texte (avancée pendant le parsing)
         *
         * Méthodes utilitaires :
         *  - Peek() : regarde le caractère courant sans avancer
         *  - Consume() : retourne et avance le caractère courant
         *  - SkipWS() : saute les whitespace (espace, tab, newline, CR)
         *  - Match() : vérifie et consomme un caractère attendu
         *  - Expect() : comme Match() mais avec message d'erreur si échec
         *  - AtEnd() : vérifie si la fin du texte est atteinte
         *
         * @note Cette structure permet un parsing single-pass sans allocation dynamique.
         * @note La position pos est mutable même dans les méthodes const via const_cast.
         */
        struct NkJSONParser {
            /// @brief Texte JSON source en lecture seule
            NkStringView text;

            /// @brief Position courante dans le texte (avancée pendant le parsing)
            nk_size pos = 0;

            /**
             * @brief Regarde le caractère courant sans avancer la position
             * @return Caractère à la position courante, ou '\0' si fin de texte
             * @note noexcept : garantie de non-levée d'exception
             */
            char Peek() const noexcept {
                return pos < text.Length() ? text[pos] : '\0';
            }

            /**
             * @brief Retourne et avance le caractère courant
             * @return Caractère consommé, ou '\0' si fin de texte
             * @note noexcept : garantie de non-levée d'exception
             */
            char Consume() noexcept {
                return pos < text.Length() ? text[pos++] : '\0';
            }

            /**
             * @brief Saute tous les whitespace à partir de la position courante
             * @note noexcept : garantie de non-levée d'exception
             * @note Whitespace supportés : ' ', '\t', '\r', '\n'
             */
            void SkipWS() noexcept {
                while (pos < text.Length()) {
                    char c = text[pos];
                    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                        ++pos;
                    } else {
                        break;
                    }
                }
            }

            /**
             * @brief Vérifie et consomme un caractère attendu (avec SkipWS préalable)
             * @param expected Caractère attendu
             * @return true si le caractère correspond et est consommé, false sinon
             * @note noexcept : garantie de non-levée d'exception
             */
            bool Match(char expected) noexcept {
                SkipWS();
                if (pos < text.Length() && text[pos] == expected) {
                    ++pos;
                    return true;
                }
                return false;
            }

            /**
             * @brief Comme Match() mais avec message d'erreur si échec
             * @param expected Caractère attendu
             * @param err Pointeur optionnel vers NkString pour message d'erreur
             * @return true si succès, false si échec avec message optionnel
             * @note noexcept : garantie de non-levée d'exception
             */
            bool Expect(char expected, NkString* err) noexcept {
                if (!Match(expected)) {
                    if (err) {
                        *err = NkString::Fmtf("Expected '%c' at pos %zu", expected, pos);
                    }
                    return false;
                }
                return true;
            }

            /**
             * @brief Vérifie si la fin du texte est atteinte (après SkipWS)
             * @return true si pos >= text.Length() après avoir sauté les whitespace
             * @note noexcept : garantie de non-levée d'exception
             */
            bool AtEnd() noexcept {
                SkipWS();
                return pos >= text.Length();
            }

            /**
             * @brief Parse une chaîne JSON entre quotes avec échappement
             * @param out Référence vers NkString pour recevoir la chaîne décodée
             * @param err Pointeur optionnel vers NkString pour message d'erreur
             * @return true si le parsing a réussi, false en cas d'erreur de syntaxe
             * @note noexcept : garantie de non-levée d'exception
             *
             * Format attendu : "chaîne échappée"
             * Échappements supportés : \", \\, \/, \b, \f, \n, \r, \t, \uXXXX
             *
             * @note Gestion des erreurs : quote manquante, échappement incomplet → false
             */
            bool ParseString(NkString& out, NkString* err) noexcept {
                SkipWS();
                if (Peek() != '"') {
                    if (err) {
                        *err = NkString("Expected '\"'");
                    }
                    return false;
                }
                ++pos;  // Consomme la quote ouvrante

                NkString raw;
                while (pos < text.Length() && text[pos] != '"') {
                    char c = text[pos++];
                    if (c == '\\') {
                        if (pos >= text.Length()) {
                            if (err) {
                                *err = NkString("Unterminated escape");
                            }
                            return false;
                        }
                        char e = text[pos++];
                        switch (e) {
                            case '"':
                                raw.Append('"');
                                break;
                            case '\\':
                                raw.Append('\\');
                                break;
                            case '/':
                                raw.Append('/');
                                break;
                            case 'b':
                                raw.Append('\b');
                                break;
                            case 'f':
                                raw.Append('\f');
                                break;
                            case 'n':
                                raw.Append('\n');
                                break;
                            case 'r':
                                raw.Append('\r');
                                break;
                            case 't':
                                raw.Append('\t');
                                break;
                            case 'u':
                                // Minimal : consomme 4 hex, émet '?'
                                if (pos + 4u <= text.Length()) {
                                    pos += 4u;
                                }
                                raw.Append('?');
                                break;
                            default:
                                raw.Append(e);
                                break;
                        }
                    } else {
                        raw.Append(c);
                    }
                }
                if (pos >= text.Length() || text[pos] != '"') {
                    if (err) {
                        *err = NkString("Unterminated string");
                    }
                    return false;
                }
                ++pos;  // Consomme la quote fermante
                out = traits::NkMove(raw);
                return true;
            }

            /**
             * @brief Parse un objet JSON récursivement
             * @param out Référence vers NkArchive pour recevoir les paires clé/valeur
             * @param err Pointeur optionnel vers NkString pour message d'erreur
             * @return true si le parsing a réussi, false en cas d'erreur de syntaxe
             * @note noexcept : garantie de non-levée d'exception
             *
             * Format attendu : { "key1": value1, "key2": value2, ... }
             *
             * Algorithme :
             *  1. SkipWS + vérification de '}' pour objet vide
             *  2. Boucle infinie :
             *     a. ParseString pour la clé
             *     b. Expect(':') pour le séparateur
             *     c. ParseNode récursif pour la valeur
             *     d. SetNode pour insérer dans out
             *     e. SkipWS + Match('}') pour fin ou Expect(',') pour continuation
             *
             * @note Gestion des erreurs : propagation immédiate en cas d'échec
             */
            bool ParseObject(NkArchive& out, NkString* err) noexcept {
                // On est déjà passé par '{'
                SkipWS();
                if (Match('}')) {
                    return true;  // Objet vide
                }

                while (true) {
                    NkString key;
                    if (!ParseString(key, err)) {
                        return false;
                    }
                    if (!Expect(':', err)) {
                        return false;
                    }

                    NkArchiveNode val;
                    if (!ParseNode(val, err)) {
                        return false;
                    }
                    out.SetNode(key.View(), val);

                    SkipWS();
                    if (Match('}')) {
                        return true;
                    }
                    if (!Expect(',', err)) {
                        return false;
                    }
                }
            }

            /**
             * @brief Parse un tableau JSON récursivement
             * @param out Référence vers NkArchiveNode pour recevoir le tableau
             * @param err Pointeur optionnel vers NkString pour message d'erreur
             * @return true si le parsing a réussi, false en cas d'erreur de syntaxe
             * @note noexcept : garantie de non-levée d'exception
             *
             * Format attendu : [ value1, value2, ... ]
             *
             * Algorithme :
             *  1. out.MakeArray() pour initialiser le nœud comme tableau
             *  2. SkipWS + vérification de ']' pour tableau vide
             *  3. Boucle infinie :
             *     a. ParseNode récursif pour l'élément
             *     b. PushBack dans out.array
             *     c. SkipWS + Match(']') pour fin ou Expect(',') pour continuation
             *
             * @note Gestion des erreurs : propagation immédiate en cas d'échec
             */
            bool ParseArray(NkArchiveNode& out, NkString* err) noexcept {
                // On est déjà passé par '['
                out.MakeArray();
                SkipWS();
                if (Match(']')) {
                    return true;  // Tableau vide
                }

                while (true) {
                    NkArchiveNode elem;
                    if (!ParseNode(elem, err)) {
                        return false;
                    }
                    out.array.PushBack(traits::NkMove(elem));

                    SkipWS();
                    if (Match(']')) {
                        return true;
                    }
                    if (!Expect(',', err)) {
                        return false;
                    }
                }
            }

            /**
             * @brief Parse un nombre JSON (entier ou flottant)
             * @param out Référence vers NkArchiveValue pour recevoir la valeur parsée
             * @param err Pointeur optionnel vers NkString pour message d'erreur
             * @return true si le parsing a réussi, false en cas d'erreur de syntaxe
             * @note noexcept : garantie de non-levée d'exception
             *
             * Format supporté :
             *  - Optionnel : signe '-' ou '+'
             *  - Requis : un ou plusieurs chiffres (0-9)
             *  - Optionnel : partie décimale '.digits'
             *  - Optionnel : exposant 'e' ou 'E' avec signe optionnel et chiffres
             *
             * Type de retour :
             *  - Si exposant ou point décimal : NK_VALUE_FLOAT64 via ToDouble()
             *  - Si signe '-' : NK_VALUE_INT64 via ToInt64()
             *  - Sinon : NK_VALUE_UINT64 via ToUInt64(), fallback sur Int64 si échec
             *
             * @note Gestion des erreurs : pas de chiffres → false, parsing échoué → false
             */
            bool ParseNumber(NkArchiveValue& out, NkString* err) noexcept {
                nk_size start = pos;
                if (pos < text.Length() && (text[pos] == '-' || text[pos] == '+')) {
                    ++pos;
                }
                bool hasDigits = false;
                while (pos < text.Length() && text[pos] >= '0' && text[pos] <= '9') {
                    ++pos;
                    hasDigits = true;
                }
                bool isFloat = false;
                if (pos < text.Length() && text[pos] == '.') {
                    ++pos;
                    isFloat = true;
                    while (pos < text.Length() && text[pos] >= '0' && text[pos] <= '9') {
                        ++pos;
                    }
                    hasDigits = true;
                }
                if (pos < text.Length() && (text[pos] == 'e' || text[pos] == 'E')) {
                    ++pos;
                    isFloat = true;
                    if (pos < text.Length() && (text[pos] == '+' || text[pos] == '-')) {
                        ++pos;
                    }
                    while (pos < text.Length() && text[pos] >= '0' && text[pos] <= '9') {
                        ++pos;
                    }
                }

                if (!hasDigits) {
                    if (err) {
                        *err = NkString("Invalid number");
                    }
                    return false;
                }

                NkString token(text.Data() + start, pos - start);
                if (isFloat) {
                    float64 v = 0.0;
                    if (!token.ToDouble(v)) {
                        if (err) {
                            *err = NkString("Float parse error");
                        }
                        return false;
                    }
                    out = NkArchiveValue::FromFloat64(v);
                } else if (!token.Empty() && token[0] == '-') {
                    int64 v = 0;
                    if (!token.ToInt64(v)) {
                        if (err) {
                            *err = NkString("Int parse error");
                        }
                        return false;
                    }
                    out = NkArchiveValue::FromInt64(v);
                } else {
                    uint64 v = 0;
                    if (!token.ToUInt64(v)) {
                        int64 sv = 0;
                        if (!token.ToInt64(sv)) {
                            if (err) {
                                *err = NkString("UInt parse error");
                            }
                            return false;
                        }
                        out = NkArchiveValue::FromInt64(sv);
                    } else {
                        out = NkArchiveValue::FromUInt64(v);
                    }
                }
                return true;
            }

            /**
             * @brief Parse un nœud JSON générique (scalaire, objet ou tableau)
             * @param out Référence vers NkArchiveNode pour recevoir le résultat
             * @param err Pointeur optionnel vers NkString pour message d'erreur
             * @return true si le parsing a réussi, false en cas d'erreur de syntaxe
             * @note noexcept : garantie de non-levée d'exception
             *
             * Dispatch par premier caractère :
             *  - '{' : appel à ParseObject() pour objet imbriqué
             *  - '[' : appel à ParseArray() pour tableau
             *  - '"' : appel à ParseString() pour chaîne, puis FromString()
             *  - 't' : vérifie "true" → FromBool(true)
             *  - 'f' : vérifie "false" → FromBool(false)
             *  - 'n' : vérifie "null" → Null()
             *  - '-' ou chiffre : appel à ParseNumber() pour nombre
             *  - Autre : erreur avec caractère et position
             *
             * @note Gestion des erreurs : propagation immédiate en cas d'échec
             */
            bool ParseNode(NkArchiveNode& out, NkString* err) noexcept {
                SkipWS();
                if (pos >= text.Length()) {
                    if (err) {
                        *err = NkString("Unexpected EOF");
                    }
                    return false;
                }

                char c = text[pos];

                if (c == '{') {
                    ++pos;
                    NkArchive arc;
                    if (!ParseObject(arc, err)) {
                        return false;
                    }
                    out.SetObject(arc);
                    return true;
                }
                if (c == '[') {
                    ++pos;
                    return ParseArray(out, err);
                }
                if (c == '"') {
                    NkString s;
                    if (!ParseString(s, err)) {
                        return false;
                    }
                    out = NkArchiveNode(NkArchiveValue::FromString(s.View()));
                    return true;
                }
                if (pos + 4u <= text.Length() && memcmp(text.Data() + pos, "true", 4u) == 0) {
                    pos += 4;
                    out = NkArchiveNode(NkArchiveValue::FromBool(true));
                    return true;
                }
                if (pos + 5u <= text.Length() && memcmp(text.Data() + pos, "false", 5u) == 0) {
                    pos += 5;
                    out = NkArchiveNode(NkArchiveValue::FromBool(false));
                    return true;
                }
                if (pos + 4u <= text.Length() && memcmp(text.Data() + pos, "null", 4u) == 0) {
                    pos += 4;
                    out = NkArchiveNode(NkArchiveValue::Null());
                    return true;
                }
                if (c == '-' || (c >= '0' && c <= '9')) {
                    NkArchiveValue v;
                    if (!ParseNumber(v, err)) {
                        return false;
                    }
                    out = NkArchiveNode(v);
                    return true;
                }

                if (err) {
                    *err = NkString::Fmtf("Unexpected char '%c' at pos %zu", c, pos);
                }
                return false;
            }

        }; // struct NkJSONParser


    } // namespace anonyme


    // =============================================================================
    // NkJSONReader — IMPLÉMENTATION DE LA MÉTHODE PUBLIQUE
    // =============================================================================

    // -------------------------------------------------------------------------
    // MÉTHODE : ReadArchive
    // DESCRIPTION : Point d'entrée public pour désérialisation JSON
    // -------------------------------------------------------------------------
    /**
     * @brief Implémentation de ReadArchive — point d'entrée public
     * @param json Vue de chaîne contenant le texte JSON à parser
     * @param outArchive Archive de destination (vidée avant parsing)
     * @param outError Pointeur optionnel pour message d'erreur détaillé
     * @return true si le parsing a réussi, false en cas d'erreur
     *
     * Étapes :
     *  1. Clear() de l'archive de sortie
     *  2. Création du parser NkJSONParser avec le texte source
     *  3. Validation : Match('{') pour objet racine — erreur si échec
     *  4. Gestion de l'objet vide : Match('}') immédiat → retour true
     *  5. Boucle de parsing des entrées :
     *     a. ParseString pour la clé
     *     b. Expect(':') pour le séparateur
     *     c. ParseNode récursif pour la valeur
     *     d. SetNode pour insérer dans outArchive
     *     e. SkipWS + Match('}') pour fin ou Expect(',') pour continuation
     *  6. Validation finale : pas de contenu trailing après '}'
     *  7. Retour du résultat
     *
     * @note Méthode noexcept-friendly : aucune exception levée.
     * @note En cas d'erreur partielle : outArchive peut contenir des données partielles.
     *       L'appelant doit vérifier le retour bool avant d'utiliser le résultat.
     *
     * @example Voir les exemples dans le header NkJSONReader.h
     */
    nk_bool NkJSONReader::ReadArchive(NkStringView json,
                                      NkArchive& outArchive,
                                      NkString* outError) {
        outArchive.Clear();

        NkJSONParser p{json, 0};
        if (!p.Match('{')) {
            if (outError) {
                *outError = NkString("JSON must start with '{'");
            }
            return false;
        }

        p.SkipWS();
        if (p.Match('}')) {
            return true;  // Objet vide
        }

        while (true) {
            NkString key;
            if (!p.ParseString(key, outError)) {
                return false;
            }
            if (!p.Expect(':', outError)) {
                return false;
            }

            NkArchiveNode node;
            if (!p.ParseNode(node, outError)) {
                return false;
            }
            outArchive.SetNode(key.View(), node);

            p.SkipWS();
            if (p.Match('}')) {
                break;
            }
            if (!p.Expect(',', outError)) {
                return false;
            }
        }

        p.SkipWS();
        if (p.pos != p.text.Length()) {
            if (outError) {
                *outError = NkString("Trailing content after JSON object");
            }
            return false;
        }
        return true;
    }


} // namespace nkentseu


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================