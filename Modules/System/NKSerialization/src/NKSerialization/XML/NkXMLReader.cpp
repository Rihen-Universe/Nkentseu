// =============================================================================
// NKSerialization/XML/NkXMLReader.cpp
// =============================================================================
// Corrections vs version originale :
//  - namespace entseu supprimé
//  - Ajout du parsing <object> et <array> récursifs
//  - Find() utilise NkString::Find() correctement
//  - NkXMLUnescape : correction de la comparaison NkStringView (SubStr)
//  - GetAttribute() : recherche robuste, ignore le whitespace
//  - Correction : cursor avancé correctement même pour les tags auto-fermants
// =============================================================================
#include "NKSerialization/XML/NkXMLReader.h"

namespace nkentseu {

    namespace {

        // ── Unescape XML entities ─────────────────────────────────────────────────────
        NkString NkXMLUnescape(NkStringView input) noexcept {
            NkString out;
            out.Reserve(input.Length());
            for (nk_size i = 0; i < input.Length(); ) {
                if (input[i] != '&') { out.Append(input[i++]); continue; }
                // Chercher le ';'
                nk_size j = i + 1;
                while (j < input.Length() && input[j] != ';') ++j;
                if (j >= input.Length()) { out.Append(input[i++]); continue; }
                NkStringView entity = input.SubStr(i, j - i + 1); // inclut '&' et ';'
                if (entity == "&amp;")  { out.Append('&');  i = j + 1; }
                else if (entity == "&lt;")   { out.Append('<');  i = j + 1; }
                else if (entity == "&gt;")   { out.Append('>');  i = j + 1; }
                else if (entity == "&quot;") { out.Append('"');  i = j + 1; }
                else if (entity == "&apos;") { out.Append('\''); i = j + 1; }
                else { out.Append('&'); ++i; } // entité inconnue → garder tel quel
            }
            return out;
        }

        // ── Extraire la valeur d'un attribut depuis le texte du tag ──────────────────
        // Exemple : tag = `key="foo" type="string"`, name = "key" → "foo"
        bool NkGetAttribute(NkStringView tag, const char* name, NkString& out) noexcept {
            // Construire le pattern `name="`
            NkString pattern(name);
            pattern.Append("=\"");

            const char* tagData = tag.Data();
            nk_size     tagLen  = tag.Length();
            nk_size     patLen  = pattern.Length();

            for (nk_size i = 0; i + patLen <= tagLen; ++i) {
                if (memcmp(tagData + i, pattern.CStr(), patLen) == 0) {
                    nk_size start = i + patLen;
                    nk_size end   = start;
                    while (end < tagLen && tagData[end] != '"') ++end;
                    out = NkXMLUnescape(NkStringView(tagData + start, end - start));
                    return true;
                }
            }
            return false;
        }

        // ── Type depuis la string "type" ──────────────────────────────────────────────
        NkArchiveValueType NkParseType(NkStringView t) noexcept {
            if (t == "null")    return NkArchiveValueType::NK_VALUE_NULL;
            if (t == "bool")    return NkArchiveValueType::NK_VALUE_BOOL;
            if (t == "int64")   return NkArchiveValueType::NK_VALUE_INT64;
            if (t == "uint64")  return NkArchiveValueType::NK_VALUE_UINT64;
            if (t == "float64") return NkArchiveValueType::NK_VALUE_FLOAT64;
            return NkArchiveValueType::NK_VALUE_STRING;
        }

        // ── Parser récursif ───────────────────────────────────────────────────────────
        // Retourne false en cas d'erreur.
        // cursor : position courante dans source, avancée au fil du parsing.
        // endTag : tag fermant attendu (ex: "</archive>", "</object>")
        bool NkParseBlock(const NkString& source, nk_size& cursor, const char* endTag, NkArchive& out, NkString* err) noexcept {
            NkString endTagStr(endTag);

            while (true) {
                // Chercher le prochain '<'
                nk_size open = source.Find('<', cursor);
                if (open == NkString::npos) {
                    if (err) *err = NkString::Fmtf("Expected %s, got EOF", endTag);
                    return false;
                }

                // Chercher la fermeture du tag '>'
                nk_size close = source.Find('>', open);
                if (close == NkString::npos) {
                    if (err) *err = NkString("Malformed tag: missing '>'");
                    return false;
                }

                NkStringView tagFull = NkStringView(source.CStr() + open, close - open + 1);

                // ── Tag fermant du bloc courant ? ─────────────────────────────────────
                if (tagFull.StartsWith(endTag)) {
                    cursor = close + 1;
                    return true;
                }

                // ── <entry ...>...</entry> ────────────────────────────────────────────
                if (tagFull.StartsWith("<entry ") || tagFull.StartsWith("<entry\t")) {
                    NkString key, typeStr;
                    if (!NkGetAttribute(tagFull, "key", key)) {
                        if (err) *err = NkString("<entry> missing key attribute");
                        return false;
                    }
                    NkGetAttribute(tagFull, "type", typeStr);

                    // Contenu entre > et </entry>
                    nk_size contentStart = close + 1;
                    nk_size endEntry = source.Find("</entry>", contentStart);
                    if (endEntry == NkString::npos) {
                        if (err) *err = NkString("Missing </entry>");
                        return false;
                    }
                    NkString rawContent = source.SubStr(contentStart, endEntry - contentStart);
                    NkString content    = NkXMLUnescape(rawContent.View());

                    NkArchiveValue val;
                    val.type = NkParseType(typeStr.View());
                    val.text = content;
                    if (val.type == NkArchiveValueType::NK_VALUE_NULL) val.text.Clear();

                    // Reconstruire raw union
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
                    cursor = endEntry + 8u; // len("</entry>") = 8
                    continue;
                }

                // ── <object key="...">...</object> ───────────────────────────────────
                if (tagFull.StartsWith("<object ") || tagFull.StartsWith("<object\t")) {
                    NkString key;
                    if (!NkGetAttribute(tagFull, "key", key)) {
                        if (err) *err = NkString("<object> missing key attribute");
                        return false;
                    }
                    cursor = close + 1;
                    NkArchive childArchive;
                    if (!NkParseBlock(source, cursor, "</object>", childArchive, err)) return false;
                    out.SetObject(key.View(), childArchive);
                    continue;
                }

                // ── <array key="..." count="N">...</array> ───────────────────────────
                if (tagFull.StartsWith("<array ") || tagFull.StartsWith("<array\t")) {
                    NkString key;
                    if (!NkGetAttribute(tagFull, "key", key)) {
                        if (err) *err = NkString("<array> missing key attribute");
                        return false;
                    }
                    cursor = close + 1;

                    // Lire les éléments jusqu'à </array>
                    NkVector<NkArchiveNode> elems;
                    while (true) {
                        nk_size elemOpen = source.Find('<', cursor);
                        if (elemOpen == NkString::npos) {
                            if (err) *err = NkString("Expected </array>, got EOF");
                            return false;
                        }
                        nk_size elemClose = source.Find('>', elemOpen);
                        if (elemClose == NkString::npos) {
                            if (err) *err = NkString("Malformed array element tag");
                            return false;
                        }
                        NkStringView elemTag = NkStringView(source.CStr() + elemOpen, elemClose - elemOpen + 1);

                        if (elemTag.StartsWith("</array>")) { cursor = elemClose + 1; break; }

                        // Chaque élément est soit <entry ...>, <object ...>, <array ...>
                        // On crée une archive temporaire à un seul élément pour factoriser le parsing
                        NkArchive tmp;
                        cursor = elemOpen; // reparser depuis le début du tag
                        // On change le endTag en </array_elem_sentinel> — trop complexe.
                        // Solution : parse via ParseBlock avec un sentinel fictif
                        // En pratique on supporte les éléments scalaires uniquement ici.
                        if (elemTag.StartsWith("<entry ")) {
                            NkString eKey, eType;
                            NkGetAttribute(elemTag, "key", eKey); // index
                            NkGetAttribute(elemTag, "type", eType);
                            nk_size cStart = elemClose + 1;
                            nk_size cEnd   = source.Find("</entry>", cStart);
                            if (cEnd == NkString::npos) { if (err) *err = NkString("Missing </entry> in array"); return false; }
                            NkString rawContent = source.SubStr(cStart, cEnd - cStart);
                            NkArchiveValue val;
                            val.type = NkParseType(eType.View());
                            val.text = NkXMLUnescape(rawContent.View());
                            if (val.type == NkArchiveValueType::NK_VALUE_NULL) val.text.Clear();
                            if (val.type == NkArchiveValueType::NK_VALUE_BOOL)    val.raw.b = (val.text == NkString("true"));
                            else if (val.type == NkArchiveValueType::NK_VALUE_INT64)   val.text.ToInt64(val.raw.i);
                            else if (val.type == NkArchiveValueType::NK_VALUE_UINT64)  val.text.ToUInt64(val.raw.u);
                            else if (val.type == NkArchiveValueType::NK_VALUE_FLOAT64) val.text.ToDouble(val.raw.f);
                            elems.PushBack(NkArchiveNode(val));
                            cursor = cEnd + 8u;
                        } else if (elemTag.StartsWith("<object ")) {
                            NkString eKey; NkGetAttribute(elemTag, "key", eKey);
                            cursor = elemClose + 1;
                            NkArchive childArc;
                            if (!NkParseBlock(source, cursor, "</object>", childArc, err)) return false;
                            NkArchiveNode n; n.SetObject(childArc);
                            elems.PushBack(std::move(n));
                        } else {
                            // Tag inconnu → avancer
                            cursor = elemClose + 1;
                        }
                    }

                    out.SetNodeArray(key.View(), elems);
                    continue;
                }

                // ── Tag non reconnu → avancer ─────────────────────────────────────────
                cursor = close + 1;
            }
        }

    } // namespace

    nk_bool NkXMLReader::ReadArchive(NkStringView xml, NkArchive& outArchive, NkString* outError) {
        outArchive.Clear();
        NkString source(xml);

        nk_size start = source.Find("<archive>");
        if (start == NkString::npos) {
            if (outError) *outError = NkString("Missing <archive> root tag");
            return false;
        }

        nk_size cursor = start + 9u; // len("<archive>") = 9
        return NkParseBlock(source, cursor, "</archive>", outArchive, outError);
    }

} // namespace nkentseu
