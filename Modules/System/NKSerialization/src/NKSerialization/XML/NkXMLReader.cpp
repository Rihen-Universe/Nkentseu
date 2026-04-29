// =============================================================================
// NKSerialization/XML/NkXMLReader.cpp
// Implémentation du désérialiseur XML pour NkArchive.
//
// Design :
//  - Parsing token par token avec suivi de curseur dans la source
//  - Fonctions internes dans namespace anonyme pour encapsulation
//  - Gestion robuste des entités XML et des attributs
//  - Parsing récursif des blocs <object> et <array>
//  - Message d'erreur optionnel pour debugging en production
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "NKSerialization/XML/NkXMLReader.h"


namespace nkentseu {


    // =============================================================================
    // NAMESPACE ANONYME : HELPERS INTERNES DE PARSING
    // =============================================================================
    // Ces fonctions sont privées au TU pour éviter la pollution de l'espace de noms.
    // Elles implémentent la logique basique de parsing XML token par token.

    namespace {


        // -------------------------------------------------------------------------
        // FONCTION : NkXMLUnescape
        // DESCRIPTION : Décode les entités XML en caractères littéraux
        // -------------------------------------------------------------------------
        /**
         * @brief Décode les entités XML standards en caractères littéraux
         * @param input Vue de chaîne contenant le texte avec entités
         * @return NkString contenant le texte décodé
         * @note noexcept : garantie de non-levée d'exception
         *
         * Entités supportées (conformité XML 1.0) :
         *  - &amp;  → &
         *  - &lt;   → <
         *  - &gt;   → >
         *  - &quot; → "
         *  - &apos; → '
         *
         * Comportement pour entités inconnues :
         *  - Le caractère & est conservé tel quel (tolérant)
         *  - Permet de parser du XML non-strict sans échec brutal
         *
         * @note Pré-allocation via Reserve() pour éviter les réallocations multiples.
         * @note Complexité O(n) où n = longueur de la chaîne d'entrée.
         *
         * @example
         * @code
         * NkXMLUnescape("Tom &amp; Jerry")  → "Tom & Jerry"
         * NkXMLUnescape("&lt;hero&gt;")     → "<hero>"
         * NkXMLUnescape("It&apos;s &quot;ok&quot;") → "It's \"ok\""
         * @endcode
         */
        NkString NkXMLUnescape(NkStringView input) noexcept {
            NkString out;
            out.Reserve(input.Length());
            for (nk_size i = 0; i < input.Length(); ) {
                if (input[i] != '&') {
                    out.Append(input[i++]);
                    continue;
                }
                // Recherche du ';' terminant l'entité
                nk_size j = i + 1;
                while (j < input.Length() && input[j] != ';') {
                    ++j;
                }
                if (j >= input.Length()) {
                    // Pas de ';' trouvé : conserver le & et avancer
                    out.Append(input[i++]);
                    continue;
                }
                // Extraction de l'entité complète (&...;)
                NkStringView entity = input.SubStr(i, j - i + 1);
                if (entity == "&amp;") {
                    out.Append('&');
                    i = j + 1;
                } else if (entity == "&lt;") {
                    out.Append('<');
                    i = j + 1;
                } else if (entity == "&gt;") {
                    out.Append('>');
                    i = j + 1;
                } else if (entity == "&quot;") {
                    out.Append('"');
                    i = j + 1;
                } else if (entity == "&apos;") {
                    out.Append('\'');
                    i = j + 1;
                } else {
                    // Entité inconnue : conserver le & et avancer d'un caractère
                    out.Append('&');
                    ++i;
                }
            }
            return out;
        }

        // -------------------------------------------------------------------------
        // FONCTION : NkGetAttribute
        // DESCRIPTION : Extrait la valeur d'un attribut depuis un tag XML
        // -------------------------------------------------------------------------
        /**
         * @brief Extrait la valeur d'un attribut nommé depuis le texte d'un tag
         * @param tag Vue de chaîne contenant le tag complet (ex: <entry key="x" type="y">)
         * @param name Nom de l'attribut à rechercher (ex: "key")
         * @param out Référence vers NkString pour recevoir la valeur décodée
         * @return true si l'attribut a été trouvé et extrait, false sinon
         * @note noexcept : garantie de non-levée d'exception
         *
         * Algorithme de recherche :
         *  1. Construit le pattern name=" (ex: key=")
         *  2. Recherche linéaire du pattern dans le texte du tag
         *  3. Extrait la valeur jusqu'au prochain guillemet fermant "
         *  4. Applique NkXMLUnescape() pour décoder les entités dans la valeur
         *
         * @note La recherche est case-sensitive : "Key" ≠ "key".
         * @note Les guillemets simples ne sont pas supportés (standard: doubles uniquement).
         *
         * @example
         * @code
         * NkString value;
         * NkGetAttribute("<entry key=\"foo\" type=\"string\">", "key", value);
         * // value = "foo"
         *
         * NkGetAttribute("<entry key=\"foo\" type=\"string\">", "missing", value);
         * // retourne false, value inchangé
         * @endcode
         */
        bool NkGetAttribute(NkStringView tag,
                            const char* name,
                            NkString& out) noexcept {
            // Construction du pattern name="
            NkString pattern(name);
            pattern.Append("=\"");

            const char* tagData = tag.Data();
            nk_size tagLen = tag.Length();
            nk_size patLen = pattern.Length();

            // Recherche linéaire du pattern dans le tag
            for (nk_size i = 0; i + patLen <= tagLen; ++i) {
                if (memcmp(tagData + i, pattern.CStr(), patLen) == 0) {
                    // Pattern trouvé : extraire la valeur jusqu'au prochain "
                    nk_size start = i + patLen;
                    nk_size end = start;
                    while (end < tagLen && tagData[end] != '"') {
                        ++end;
                    }
                    // Décodage des entités XML dans la valeur extraite
                    out = NkXMLUnescape(NkStringView(tagData + start, end - start));
                    return true;
                }
            }
            return false;
        }

        // -------------------------------------------------------------------------
        // FONCTION : NkParseType
        // DESCRIPTION : Convertit une string type= en NkArchiveValueType
        // -------------------------------------------------------------------------
        /**
         * @brief Convertit une chaîne de type XML en enum NkArchiveValueType
         * @param t Vue de chaîne contenant le type (ex: "int64", "string")
         * @return NkArchiveValueType correspondant, ou NK_VALUE_STRING par défaut
         * @note noexcept : garantie de non-levée d'exception
         * @note Comparaisons case-sensitive : "Int64" ≠ "int64"
         *
         * Correspondances string → type :
         *  - "null"    → NK_VALUE_NULL
         *  - "bool"    → NK_VALUE_BOOL
         *  - "int64"   → NK_VALUE_INT64
         *  - "uint64"  → NK_VALUE_UINT64
         *  - "float64" → NK_VALUE_FLOAT64
         *  - autre     → NK_VALUE_STRING (fallback sécurisé)
         *
         * @note Utilisé pour reconstruire le type exact lors du parsing des <entry>.
         *       Le fallback vers string garantit la robustesse face à des XML inconnus.
         *
         * @example
         * @code
         * NkParseType("bool")    → NK_VALUE_BOOL
         * NkParseType("float64") → NK_VALUE_FLOAT64
         * NkParseType("unknown") → NK_VALUE_STRING
         * @endcode
         */
        NkArchiveValueType NkParseType(NkStringView t) noexcept {
            if (t == "null") {
                return NkArchiveValueType::NK_VALUE_NULL;
            }
            if (t == "bool") {
                return NkArchiveValueType::NK_VALUE_BOOL;
            }
            if (t == "int64") {
                return NkArchiveValueType::NK_VALUE_INT64;
            }
            if (t == "uint64") {
                return NkArchiveValueType::NK_VALUE_UINT64;
            }
            if (t == "float64") {
                return NkArchiveValueType::NK_VALUE_FLOAT64;
            }
            return NkArchiveValueType::NK_VALUE_STRING;
        }

        // -------------------------------------------------------------------------
        // FONCTION : NkParseBlock
        // DESCRIPTION : Parser récursif pour blocs XML imbriqués
        // -------------------------------------------------------------------------
        /**
         * @brief Parse un bloc XML avec tag fermant attendu
         * @param source Texte XML complet à parser
         * @param cursor Référence vers la position courante (avancée pendant le parsing)
         * @param endTag Tag fermant attendu pour ce bloc (ex: "</archive>")
         * @param out Archive de destination pour les paires clé/valeur trouvées
         * @param err Pointeur optionnel vers NkString pour message d'erreur
         * @return true si le parsing a réussi, false en cas d'erreur
         * @note noexcept : garantie de non-levée d'exception
         *
         * Algorithme récursif :
         *  1. Boucle infinie : recherche du prochain tag '<'
         *  2. Si tag fermant == endTag : retour succès (fin du bloc)
         *  3. Si <entry ...> : parse la valeur scalaire et ajoute à out
         *  4. Si <object ...> : appel récursif pour parser les enfants
         *  5. Si <array ...> : boucle pour parser chaque élément indexé
         *  6. Tag inconnu : ignoré, avance le curseur
         *
         * Gestion des erreurs :
         *  - Si err != nullptr : message formaté en cas d'échec
         *  - Les erreurs n'altèrent pas l'archive : rollback implicite par non-application
         *  - Retour false immédiat pour arrêter le parsing en cascade
         *
         * @note La récursion est bornée par la profondeur d'imbrication du XML.
         *       Typiquement < 20 niveaux pour des configs réalistes.
         *
         * @example Parsing d'un bloc objet :
         * @code
         * // Input XML:
         * // <object key="user">
         * //   <entry key="name" type="string">Alice</entry>
         * //   <entry key="age" type="int64">30</entry>
         * // </object>
         *
         * // Appel : NkParseBlock(source, cursor, "</object>", outArchive, nullptr)
         * // Résultat : outArchive contient { "name": "Alice", "age": 30 }
         * @endcode
         */
        bool NkParseBlock(const NkString& source,
                          nk_size& cursor,
                          const char* endTag,
                          NkArchive& out,
                          NkString* err) noexcept {
            NkString endTagStr(endTag);

            while (true) {
                // Recherche du prochain tag '<'
                nk_size open = source.Find('<', cursor);
                if (open == NkString::npos) {
                    if (err) {
                        *err = NkString::Fmtf("Expected %s, got EOF", endTag);
                    }
                    return false;
                }

                // Recherche de la fermeture du tag '>'
                nk_size close = source.Find('>', open);
                if (close == NkString::npos) {
                    if (err) {
                        *err = NkString("Malformed tag: missing '>'");
                    }
                    return false;
                }

                NkStringView tagFull = NkStringView(source.CStr() + open, close - open + 1);

                // -----------------------------------------------------------------
                // Cas 1 : Tag fermant du bloc courant → fin du parsing récursif
                // -----------------------------------------------------------------
                if (tagFull.StartsWith(endTag)) {
                    cursor = close + 1;
                    return true;
                }

                // -----------------------------------------------------------------
                // Cas 2 : <entry ...> — valeur scalaire
                // -----------------------------------------------------------------
                if (tagFull.StartsWith("<entry ") || tagFull.StartsWith("<entry\t")) {
                    NkString key, typeStr;
                    if (!NkGetAttribute(tagFull, "key", key)) {
                        if (err) {
                            *err = NkString("<entry> missing key attribute");
                        }
                        return false;
                    }
                    NkGetAttribute(tagFull, "type", typeStr);

                    // Extraction du contenu entre > et </entry>
                    nk_size contentStart = close + 1;
                    nk_size endEntry = source.Find("</entry>", contentStart);
                    if (endEntry == NkString::npos) {
                        if (err) {
                            *err = NkString("Missing </entry>");
                        }
                        return false;
                    }
                    NkString rawContent = source.SubStr(contentStart, endEntry - contentStart);
                    NkString content = NkXMLUnescape(rawContent.View());

                    // Construction de la valeur avec type reconstruit
                    NkArchiveValue val;
                    val.type = NkParseType(typeStr.View());
                    val.text = content;
                    if (val.type == NkArchiveValueType::NK_VALUE_NULL) {
                        val.text.Clear();
                    }

                    // Reconstruction de l'union raw selon le type
                    if (val.type == NkArchiveValueType::NK_VALUE_BOOL) {
                        val.raw.b = (val.text == NkString("true"));
                    } else if (val.type == NkArchiveValueType::NK_VALUE_INT64) {
                        val.text.ToInt64(val.raw.i);
                    } else if (val.type == NkArchiveValueType::NK_VALUE_UINT64) {
                        val.text.ToUInt64(val.raw.u);
                    } else if (val.type == NkArchiveValueType::NK_VALUE_FLOAT64) {
                        val.text.ToDouble(val.raw.f);
                    }

                    out.SetValue(key.View(), val);
                    cursor = endEntry + 8u;  // len("</entry>") = 8
                    continue;
                }

                // -----------------------------------------------------------------
                // Cas 3 : <object ...> — objet imbriqué
                // -----------------------------------------------------------------
                if (tagFull.StartsWith("<object ") || tagFull.StartsWith("<object\t")) {
                    NkString key;
                    if (!NkGetAttribute(tagFull, "key", key)) {
                        if (err) {
                            *err = NkString("<object> missing key attribute");
                        }
                        return false;
                    }
                    cursor = close + 1;
                    NkArchive childArchive;
                    if (!NkParseBlock(source, cursor, "</object>", childArchive, err)) {
                        return false;
                    }
                    out.SetObject(key.View(), childArchive);
                    continue;
                }

                // -----------------------------------------------------------------
                // Cas 4 : <array ...> — tableau d'éléments
                // -----------------------------------------------------------------
                if (tagFull.StartsWith("<array ") || tagFull.StartsWith("<array\t")) {
                    NkString key;
                    if (!NkGetAttribute(tagFull, "key", key)) {
                        if (err) {
                            *err = NkString("<array> missing key attribute");
                        }
                        return false;
                    }
                    cursor = close + 1;

                    // Parsing des éléments jusqu'à </array>
                    NkVector<NkArchiveNode> elems;
                    while (true) {
                        nk_size elemOpen = source.Find('<', cursor);
                        if (elemOpen == NkString::npos) {
                            if (err) {
                                *err = NkString("Expected </array>, got EOF");
                            }
                            return false;
                        }
                        nk_size elemClose = source.Find('>', elemOpen);
                        if (elemClose == NkString::npos) {
                            if (err) {
                                *err = NkString("Malformed array element tag");
                            }
                            return false;
                        }
                        NkStringView elemTag = NkStringView(source.CStr() + elemOpen, elemClose - elemOpen + 1);

                        // Fin du tableau
                        if (elemTag.StartsWith("</array>")) {
                            cursor = elemClose + 1;
                            break;
                        }

                        // Élément scalaire <entry ...>
                        if (elemTag.StartsWith("<entry ")) {
                            NkString eKey, eType;
                            NkGetAttribute(elemTag, "key", eKey);  // index (ignoré)
                            NkGetAttribute(elemTag, "type", eType);
                            nk_size cStart = elemClose + 1;
                            nk_size cEnd = source.Find("</entry>", cStart);
                            if (cEnd == NkString::npos) {
                                if (err) {
                                    *err = NkString("Missing </entry> in array");
                                }
                                return false;
                            }
                            NkString rawContent = source.SubStr(cStart, cEnd - cStart);
                            NkArchiveValue val;
                            val.type = NkParseType(eType.View());
                            val.text = NkXMLUnescape(rawContent.View());
                            if (val.type == NkArchiveValueType::NK_VALUE_NULL) {
                                val.text.Clear();
                            }
                            if (val.type == NkArchiveValueType::NK_VALUE_BOOL) {
                                val.raw.b = (val.text == NkString("true"));
                            } else if (val.type == NkArchiveValueType::NK_VALUE_INT64) {
                                val.text.ToInt64(val.raw.i);
                            } else if (val.type == NkArchiveValueType::NK_VALUE_UINT64) {
                                val.text.ToUInt64(val.raw.u);
                            } else if (val.type == NkArchiveValueType::NK_VALUE_FLOAT64) {
                                val.text.ToDouble(val.raw.f);
                            }
                            elems.PushBack(NkArchiveNode(val));
                            cursor = cEnd + 8u;

                        // Élément objet <object ...>
                        } else if (elemTag.StartsWith("<object ")) {
                            NkString eKey;
                            NkGetAttribute(elemTag, "key", eKey);  // index (ignoré)
                            cursor = elemClose + 1;
                            NkArchive childArc;
                            if (!NkParseBlock(source, cursor, "</object>", childArc, err)) {
                                return false;
                            }
                            NkArchiveNode n;
                            n.SetObject(childArc);
                            elems.PushBack(std::move(n));

                        // Tag inconnu dans un array → ignorer
                        } else {
                            cursor = elemClose + 1;
                        }
                    }

                    out.SetNodeArray(key.View(), elems);
                    continue;
                }

                // -----------------------------------------------------------------
                // Cas 5 : Tag non reconnu → ignorer et avancer
                // -----------------------------------------------------------------
                cursor = close + 1;
            }
        }


    } // namespace anonyme


    // =============================================================================
    // NkXMLReader — IMPLÉMENTATION DE LA MÉTHODE PUBLIQUE
    // =============================================================================

    // -------------------------------------------------------------------------
    // MÉTHODE : ReadArchive
    // DESCRIPTION : Point d'entrée public pour désérialisation XML
    // -------------------------------------------------------------------------
    /**
     * @brief Implémentation de ReadArchive — point d'entrée public
     * @param xml Vue de chaîne contenant le texte XML à parser
     * @param outArchive Archive de destination (vidée avant parsing)
     * @param outError Pointeur optionnel pour message d'erreur détaillé
     * @return true si le parsing a réussi, false en cas d'erreur
     *
     * Étapes :
     *  1. Vide l'archive de sortie via Clear()
     *  2. Recherche du tag racine <archive> — erreur si absent
     *  3. Positionne le curseur après <archive> (len = 9)
     *  4. Lance le parser récursif NkParseBlock() avec endTag="</archive>"
     *  5. Retourne le résultat du parsing récursif
     *
     * @note Méthode noexcept-friendly : aucune exception levée.
     * @note En cas d'erreur de parsing : outArchive peut contenir des données partielles.
     *       L'appelant doit vérifier le retour bool avant d'utiliser le résultat.
     *
     * @example Voir les exemples dans le header NkXMLReader.h
     */
    nk_bool NkXMLReader::ReadArchive(NkStringView xml,
                                     NkArchive& outArchive,
                                     NkString* outError) {
        outArchive.Clear();
        NkString source(xml);

        nk_size start = source.Find("<archive>");
        if (start == NkString::npos) {
            if (outError) {
                *outError = NkString("Missing <archive> root tag");
            }
            return false;
        }

        nk_size cursor = start + 9u;  // len("<archive>") = 9
        return NkParseBlock(source, cursor, "</archive>", outArchive, outError);
    }


} // namespace nkentseu


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================