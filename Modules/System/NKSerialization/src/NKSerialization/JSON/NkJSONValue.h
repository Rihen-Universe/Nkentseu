// =============================================================================
// NKSerialization/JSON/NkJSONValue.h
// Utilitaires de manipulation de chaînes JSON — échappement et unescaping.
//
// Design :
//  - NkJSONEscapeString : échappe une chaîne pour conformité JSON RFC 8259
//  - NkJSONUnescapeString : décode une chaîne JSON échappée en texte brut
//  - Gestion des caractères de contrôle via \uXXXX (Unicode escape)
//  - Export contrôlé via NKENTSEU_SERIALIZATION_API (fonctions libres)
//  - Aucune allocation dynamique inutile : pré-allocation via Reserve()
//
// Caractères échappés (conformité JSON) :
//  - " → \" (double quote)
//  - \ → \\ (backslash)
//  - / → \/ (slash, optionnel mais recommandé pour </script>)
//  - \b → \\b (backspace, 0x08), \f → \\f (form feed, 0x0C)
//  - \n → \\n (newline, 0x0A), \r → \\r (carriage return, 0x0D)
//  - \t → \\t (tab, 0x09)
//  - Caractères < 0x20 → \uXXXX (Unicode escape hexadécimal)
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_SERIALIZATION_JSON_NKJSONVALUE_H
#define NKENTSEU_SERIALIZATION_JSON_NKJSONVALUE_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusions des dépendances du module de sérialisation.
    // NkSerializationApi.h fournit les macros d'export requises.

    #include "NKSerialization/NkSerializationApi.h"
    #include "NKContainers/String/NkString.h"
    #include "NKContainers/String/NkStringView.h"
    #include "NKCore/NkTypes.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : DÉCLARATION DU NAMESPACE PRINCIPAL
    // -------------------------------------------------------------------------
    // Tous les symboles du module serialization sont dans le namespace nkentseu.

    namespace nkentseu {


        // =============================================================================
        // FONCTION : NkJSONEscapeString
        // DESCRIPTION : Échappe une chaîne pour conformité JSON RFC 8259
        // =============================================================================
        /**
         * @brief Échappe une chaîne pour conformité JSON RFC 8259
         * @param input Vue de chaîne source à échapper
         * @return NkString contenant la chaîne échappée prête pour JSON
         * @note noexcept : garantie de non-levée d'exception
         * @ingroup JSONUtilities
         *
         * Caractères échappés :
         *  - " → \" (double quote, requis dans les strings JSON)
         *  - \ → \\ (backslash, requis pour éviter les séquences d'échappement)
         *  - / → \/ (slash, optionnel mais recommandé pour </script> dans HTML)
         *  - \b → \\b (backspace, 0x08), \f → \\f (form feed, 0x0C)
         *  - \n → \\n (newline, 0x0A), \r → \\r (carriage return, 0x0D)
         *  - \t → \\t (tab, 0x09)
         *  - Caractères de contrôle (< 0x20) → \uXXXX (Unicode escape hexadécimal)
         *
         * @note Pré-allocation via Reserve(input.Length() + 8) pour éviter les réallocations.
         * @note Complexité O(n) où n = longueur de la chaîne d'entrée.
         *
         * @example
         * @code
         * nkentseu::NkJSONEscapeString("Hello \"World\"")  → "Hello \\\"World\\\""
         * nkentseu::NkJSONEscapeString("Line1\nLine2")     → "Line1\\nLine2"
         * nkentseu::NkJSONEscapeString("Tab\tHere")        → "Tab\\tHere"
         * @endcode
         */
        NKENTSEU_SERIALIZATION_API NkString NkJSONEscapeString(NkStringView input) noexcept;


        // =============================================================================
        // FONCTION : NkJSONUnescapeString
        // DESCRIPTION : Décode une chaîne JSON échappée en texte brut
        // =============================================================================
        /**
         * @brief Décode une chaîne JSON échappée en texte brut
         * @param input Vue de chaîne contenant le texte JSON échappé
         * @param ok Référence vers bool pour indiquer le succès de l'opération
         * @return NkString contenant le texte décodé, ou chaîne vide si erreur
         * @note noexcept : garantie de non-levée d'exception
         * @ingroup JSONUtilities
         *
         * Séquences d'échappement supportées :
         *  - \" → " (double quote)
         *  - \\ → \ (backslash), \/ → / (slash)
         *  - \b → backspace (0x08), \f → form feed (0x0C)
         *  - \n → newline (0x0A), \r → carriage return (0x0D), \t → tab (0x09)
         *  - \uXXXX → caractère Unicode (implémentation minimale : émet '?')
         *
         * Gestion des erreurs :
         *  - Si échappement incomplet (ex: \ en fin de chaîne) : ok=false, retour vide
         *  - Si \uXXXX incomplet (<4 hex digits) : ok=false, retour vide
         *  - Si séquence d'échappement inconnue : ok=false, retour vide
         *
         * @note Pré-allocation via Reserve(input.Length()) pour éviter les réallocations.
         * @note Complexité O(n) où n = longueur de la chaîne d'entrée.
         *
         * @example
         * @code
         * nk_bool success;
         * auto result = nkentseu::NkJSONUnescapeString("Hello \\\"World\\\"", success);
         * // result = "Hello \"World\"", success = true
         *
         * auto bad = nkentseu::NkJSONUnescapeString("Incomplete\\", success);
         * // result = "", success = false
         * @endcode
         */
        NKENTSEU_SERIALIZATION_API NkString NkJSONUnescapeString(NkStringView input,
                                                                  nk_bool& ok) noexcept;


    } // namespace nkentseu


#endif // NKENTSEU_SERIALIZATION_JSON_NKJSONVALUE_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKJSONVALUE.H
// =============================================================================
// Ce fichier fournit les utilitaires d'échappement/déchappement JSON.
// Il combine conformité RFC 8259 et gestion robuste des erreurs.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Échappement de chaînes pour insertion dans JSON
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/JSON/NkJSONValue.h>

    void BuildJSONManually() {
        nkentseu::NkString json("{ \"message\": \"");

        // Échappement automatique des caractères spéciaux
        nkentseu::NkString userMessage = "User said: \"Hello!\"";
        json.Append(nkentseu::NkJSONEscapeString(userMessage.View()));

        json.Append("\" }");
        // Résultat : { "message": "User said: \"Hello!\"" }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Déchappement de valeurs JSON lues depuis un fichier
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/JSON/NkJSONValue.h>

    void ParseJSONValue(const char* jsonValue) {
        nk_bool ok = false;
        nkentseu::NkString decoded = nkentseu::NkJSONUnescapeString(jsonValue, ok);

        if (ok) {
            printf("Decoded: %s\n", decoded.CStr());
        } else {
            printf("Failed to decode JSON string\n");
        }
    }

    // Usage :
    // ParseJSONValue("\"Line1\\nLine2\\tTabbed\"");  // → Line1\nLine2\tTabbed
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Gestion des caractères Unicode \uXXXX (implémentation minimale)
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/JSON/NkJSONValue.h>

    void HandleUnicodeEscape() {
        // Note : l'implémentation actuelle émet '?' pour \uXXXX
        // Pour un support Unicode complet, étendre la logique dans NkJSONUnescapeString

        nk_bool ok;
        auto result = nkentseu::NkJSONUnescapeString("\\u0041", ok);  // 'A' en Unicode
        // Résultat actuel : result = "?", ok = true

        // Pour un support complet : parser les 4 hex digits et convertir en UTF-8
    }
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Validation d'entrée utilisateur avant insertion JSON
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/JSON/NkJSONValue.h>

    bool SafeJSONInsert(nkentseu::NkString& jsonBuffer,
                        const char* key,
                        const char* value) {
        // Échappement automatique pour sécurité
        nkentseu::NkString escapedKey = nkentseu::NkJSONEscapeString(key);
        nkentseu::NkString escapedValue = nkentseu::NkJSONEscapeString(value);

        jsonBuffer.Append("\"");
        jsonBuffer.Append(escapedKey);
        jsonBuffer.Append("\": \"");
        jsonBuffer.Append(escapedValue);
        jsonBuffer.Append("\"");

        return true;  // Toujours succès car échappement garanti
    }

    // Usage : empêche l'injection de code via des quotes non échappées
    // SafeJSONInsert(buf, "user", "admin\"; DROP TABLE users; --");
    // → "user": "admin\"; DROP TABLE users; --"  (sécurisé)
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================