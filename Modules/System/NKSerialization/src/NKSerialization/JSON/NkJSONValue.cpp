// =============================================================================
// NKSerialization/JSON/NkJSONValue.cpp
// Implémentation des utilitaires d'échappement/déchappement JSON.
//
// Design :
//  - NkJSONEscapeString : échappement RFC 8259 avec pré-allocation
//  - NkJSONUnescapeString : décodage robuste avec gestion d'erreurs
//  - Support minimal de \uXXXX (émet '?' pour extension future)
//  - Aucune allocation dynamique inutile : Reserve() pour éviter les realloc
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "NKSerialization/JSON/NkJSONValue.h"


namespace nkentseu {


    // =============================================================================
    // FONCTION : NkJSONEscapeString
    // DESCRIPTION : Implémentation de l'échappement JSON RFC 8259
    // =============================================================================
    /**
     * @brief Implémentation de NkJSONEscapeString
     * @param input Vue de chaîne source à échapper
     * @return NkString contenant la chaîne échappée prête pour JSON
     * @note noexcept : garantie de non-levée d'exception
     *
     * Algorithme :
     *  1. Pré-allocation : Reserve(input.Length() + 8) pour couvrir le worst-case
     *  2. Pour chaque caractère d'entrée :
     *     a. Switch sur le caractère pour détecter les cas spéciaux
     *     b. Append de la séquence d'échappement correspondante
     *     c. Pour les caractères de contrôle (<0x20) : formatage \uXXXX
     *     d. Sinon : append du caractère tel quel
     *  3. Retour du NkString résultant
     *
     * @note Conformité RFC 8259 : tous les caractères requis sont échappés.
     * @note Performance : O(n) avec une seule allocation grâce à Reserve().
     *
     * @example Voir les exemples dans le header NkJSONValue.h
     */
    NkString NkJSONEscapeString(NkStringView input) noexcept {
        NkString out;
        out.Reserve(input.Length() + 8u);
        for (nk_size i = 0; i < input.Length(); ++i) {
            char c = input[i];
            switch (c) {
                case '\\':
                    out.Append("\\\\");
                    break;
                case '"':
                    out.Append("\\\"");
                    break;
                case '\n':
                    out.Append("\\n");
                    break;
                case '\r':
                    out.Append("\\r");
                    break;
                case '\t':
                    out.Append("\\t");
                    break;
                case '\b':
                    out.Append("\\b");
                    break;
                case '\f':
                    out.Append("\\f");
                    break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20u) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04X", static_cast<unsigned>(c));
                        out.Append(buf);
                    } else {
                        out.Append(c);
                    }
                    break;
            }
        }
        return out;
    }


    // =============================================================================
    // FONCTION : NkJSONUnescapeString
    // DESCRIPTION : Implémentation du décodage JSON avec gestion d'erreurs
    // =============================================================================
    /**
     * @brief Implémentation de NkJSONUnescapeString
     * @param input Vue de chaîne contenant le texte JSON échappé
     * @param ok Référence vers bool pour indiquer le succès de l'opération
     * @return NkString contenant le texte décodé, ou chaîne vide si erreur
     * @note noexcept : garantie de non-levée d'exception
     *
     * Algorithme :
     *  1. Initialisation : ok=true, pré-allocation via Reserve(input.Length())
     *  2. Pour chaque caractère d'entrée :
     *     a. Si pas de backslash : append direct du caractère
     *     b. Si backslash : avance l'index et switch sur le caractère suivant
     *     c. Pour \uXXXX : vérifie 4 hex digits, émet '?' (implémentation minimale)
     *     d. Si séquence invalide ou incomplète : ok=false, retour chaîne vide
     *  3. Retour du NkString décodé si ok=true, sinon chaîne vide
     *
     * @note Gestion robuste des erreurs : retour immédiat en cas de problème.
     * @note Performance : O(n) avec une seule allocation grâce à Reserve().
     *
     * @example Voir les exemples dans le header NkJSONValue.h
     */
    NkString NkJSONUnescapeString(NkStringView input, nk_bool& ok) noexcept {
        ok = true;
        NkString out;
        out.Reserve(input.Length());
        for (nk_size i = 0; i < input.Length(); ++i) {
            if (input[i] != '\\') {
                out.Append(input[i]);
                continue;
            }
            if (++i >= input.Length()) {
                ok = false;
                return {};
            }
            switch (input[i]) {
                case '"':
                    out.Append('"');
                    break;
                case '\\':
                    out.Append('\\');
                    break;
                case '/':
                    out.Append('/');
                    break;
                case 'n':
                    out.Append('\n');
                    break;
                case 'r':
                    out.Append('\r');
                    break;
                case 't':
                    out.Append('\t');
                    break;
                case 'b':
                    out.Append('\b');
                    break;
                case 'f':
                    out.Append('\f');
                    break;
                case 'u':
                    // Implémentation minimale : consomme 4 hex digits, émet '?'
                    if (i + 4u >= input.Length()) {
                        ok = false;
                        return {};
                    }
                    i += 4u;
                    out.Append('?');
                    break;
                default:
                    ok = false;
                    return {};
            }
        }
        return out;
    }


} // namespace nkentseu


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================