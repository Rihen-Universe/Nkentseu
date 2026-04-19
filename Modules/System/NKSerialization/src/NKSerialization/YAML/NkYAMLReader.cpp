// =============================================================================
// NKSerialization/YAML/NkYAMLReader.cpp
// =============================================================================
// Corrections vs version originale :
//  - namespace entseu supprimé
//  - Gestion du marqueur "---" (début de document YAML)
//  - Support des blocs imbriqués via indentation
//  - Support des séquences (- item)
//  - Trim correct (NkString::Trim() ne prend pas de NkStringView en paramètre)
//  - Parsing single-quote strings avec doublage ''
// =============================================================================
#include "NKSerialization/YAML/NkYAMLReader.h"

namespace nkentseu {

    namespace {

        // ── Trim un NkString ──────────────────────────────────────────────────────────
        NkString NkTrimStr(NkStringView v) noexcept {
            NkString s(v);
            s.Trim();
            return s;
        }

        // ── Parse un scalaire YAML ────────────────────────────────────────────────────
        bool NkParseYAMLScalar(NkStringView raw, NkArchiveValue& out) noexcept {
            NkString val = NkTrimStr(raw);

            if (val.Empty() || val == NkString("~") || val == NkString("null") || val == NkString("Null")) {
                out = NkArchiveValue::Null(); return true;
            }
            if (val == NkString("true") || val == NkString("True") || val == NkString("TRUE")) {
                out = NkArchiveValue::FromBool(true); return true;
            }
            if (val == NkString("false") || val == NkString("False") || val == NkString("FALSE")) {
                out = NkArchiveValue::FromBool(false); return true;
            }

            // Single-quoted string
            if (val.Length() >= 2u && val.Front() == '\'' && val.Back() == '\'') {
                NkString unquoted;
                for (nk_size i = 1u; i + 1u < val.Length(); ++i) {
                    if (val[i] == '\'' && i + 2u < val.Length() && val[i+1u] == '\'') {
                        unquoted.Append('\''); ++i;
                    } else {
                        unquoted.Append(val[i]);
                    }
                }
                out = NkArchiveValue::FromString(unquoted.View());
                return true;
            }

            // Double-quoted string (minimal)
            if (val.Length() >= 2u && val.Front() == '"' && val.Back() == '"') {
                NkString unquoted = val.SubStr(1u, val.Length() - 2u);
                out = NkArchiveValue::FromString(unquoted.View());
                return true;
            }

            // Numeric
            if (val.Contains('.') || val.Contains('e') || val.Contains('E')) {
                float64 f = 0.0;
                if (val.ToDouble(f)) { out = NkArchiveValue::FromFloat64(f); return true; }
            }
            if (!val.Empty() && (val[0] == '-' || (val[0] >= '0' && val[0] <= '9'))) {
                if (val[0] == '-') {
                    int64 i = 0;
                    if (val.ToInt64(i)) { out = NkArchiveValue::FromInt64(i); return true; }
                } else {
                    uint64 u = 0;
                    if (val.ToUInt64(u)) { out = NkArchiveValue::FromUInt64(u); return true; }
                }
            }

            // Plain string
            out = NkArchiveValue::FromString(val.View());
            return true;
        }

        // ── Compte les espaces de leading indent ─────────────────────────────────────
        int NkCountIndent(NkStringView line) noexcept {
            int n = 0;
            for (nk_size i = 0; i < line.Length() && (line[i] == ' '); ++i) ++n;
            return n;
        }

        // ── Structures pour le parsing ligne par ligne ────────────────────────────────
        struct NkLine {
            NkString content; // ligne brute (sans \n)
            int      indent;  // nombre d'espaces de leading indent
            bool     isEmpty; // ligne vide ou commentaire
        };

        NkVector<NkLine> NkSplitLines(NkStringView yaml) noexcept {
            NkVector<NkLine> lines;
            NkString src(yaml);
            nk_size cursor = 0;

            while (cursor <= src.Length()) {
                nk_size next = src.Find('\n', cursor);
                if (next == NkString::npos) next = src.Length();

                NkString raw = src.SubStr(cursor, next - cursor);
                // Enlever \r éventuel
                if (!raw.Empty() && raw.Back() == '\r') raw = raw.SubStr(0, raw.Length() - 1u);

                NkLine l;
                l.content = raw;
                l.indent  = NkCountIndent(raw.View());

                NkString trimmed = NkTrimStr(raw.View());
                l.isEmpty = trimmed.Empty() || trimmed.Front() == '#';

                lines.PushBack(std::move(l));
                cursor = next + 1u;
                if (next == src.Length()) break;
            }
            return lines;
        }

        // ── Parser récursif sur les lignes ────────────────────────────────────────────
        // Retourne l'index de la prochaine ligne à traiter.
        // minIndent : indentation minimale pour les lignes de ce bloc.
        nk_size NkParseBlock(const NkVector<NkLine>& lines, nk_size start, int blockIndent, NkArchive& out, NkString* err) noexcept {
            nk_size i = start;

            while (i < lines.Size()) {
                const NkLine& l = lines[i];

                // Sauter les lignes vides
                if (l.isEmpty) { ++i; continue; }

                // Si l'indentation est inférieure au bloc courant → fin du bloc
                if (l.indent < blockIndent) return i;

                // Document marker
                NkString trimmed = NkTrimStr(l.content.View());
                if (trimmed == NkString("---") || trimmed == NkString("...")) { ++i; continue; }

                // ── Séquence (- value) ────────────────────────────────────────────────
                // Les séquences au niveau racine ne sont pas supportées par l'API NkArchive
                // (qui est une map). On les ignore à ce niveau.
                if (trimmed.Length() >= 2u && trimmed.Front() == '-' && trimmed[1u] == ' ') {
                    // Élément de séquence non-nommé → skip (géré par l'appelant)
                    ++i; continue;
                }

                // ── Paire clé: valeur ────────────────────────────────────────────────
                // Trouver le ':' (pas dans une valeur quotée)
                nk_size colonPos = NkString::npos;
                bool inQuote = false;
                char quoteChar = 0;
                const NkString& raw = l.content;
                for (nk_size k = static_cast<nk_size>(l.indent); k < raw.Length(); ++k) {
                    char c = raw[k];
                    if (!inQuote && (c == '\'' || c == '"')) { inQuote = true; quoteChar = c; }
                    else if (inQuote && c == quoteChar) { inQuote = false; }
                    else if (!inQuote && c == ':') {
                        // ':' suivi d'espace ou fin de ligne → séparateur
                        if (k + 1u >= raw.Length() || raw[k+1u] == ' ' || raw[k+1u] == '\t') {
                            colonPos = k; break;
                        }
                    }
                }

                if (colonPos == NkString::npos) {
                    // Ligne sans ':' → skip avec warning
                    ++i; continue;
                }

                NkString key = NkTrimStr(NkStringView(raw.CStr() + l.indent, colonPos - l.indent));
                if (key.Empty()) { ++i; continue; }

                // Nettoyer les guillemets de la clé si présents
                if (key.Length() >= 2u && key.Front() == '\'' && key.Back() == '\'') {
                    key = key.SubStr(1u, key.Length() - 2u);
                } else if (key.Length() >= 2u && key.Front() == '"' && key.Back() == '"') {
                    key = key.SubStr(1u, key.Length() - 2u);
                }

                NkStringView valueRaw = NkStringView(
                    raw.CStr() + colonPos + 1u,
                    raw.Length() - colonPos - 1u);
                NkString valueTrimmed = NkTrimStr(valueRaw);

                ++i; // passer à la ligne suivante

                // ── Valeur vide → objet ou séquence imbriqué ─────────────────────────
                if (valueTrimmed.Empty()) {
                    // Regarder la ligne suivante
                    if (i < lines.Size() && !lines[i].isEmpty) {
                        int childIndent = lines[i].indent;
                        if (childIndent > blockIndent) {
                            // Est-ce une séquence ?
                            NkString nextTrimmed = NkTrimStr(lines[i].content.View());
                            if (nextTrimmed.Length() >= 2u && nextTrimmed.Front() == '-'
                                && (nextTrimmed[1u] == ' ' || nextTrimmed[1u] == '\t')) {
                                // Séquence → parser les éléments
                                NkVector<NkArchiveNode> elems;
                                while (i < lines.Size()) {
                                    if (lines[i].isEmpty) { ++i; continue; }
                                    if (lines[i].indent < childIndent) break;
                                    NkString et = NkTrimStr(lines[i].content.View());
                                    if (et.Length() >= 2u && et.Front() == '-' && (et[1u]==' '||et[1u]=='\t')) {
                                        NkStringView elemVal = NkStringView(et.CStr()+2u, et.Length()-2u);
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
                                // Objet imbriqué
                                NkArchive childArc;
                                i = NkParseBlock(lines, i, childIndent, childArc, err);
                                out.SetObject(key.View(), childArc);
                            }
                        }
                    }
                    continue;
                }

                // ── Valeur scalaire ───────────────────────────────────────────────────
                NkArchiveValue val;
                NkParseYAMLScalar(valueTrimmed.View(), val);
                out.SetValue(key.View(), val);
            }

            return i;
        }

    } // namespace

    nk_bool NkYAMLReader::ReadArchive(NkStringView yaml, NkArchive& outArchive, NkString* outError) {
        outArchive.Clear();
        NkVector<NkLine> lines = NkSplitLines(yaml);
        NkParseBlock(lines, 0u, 0, outArchive, outError);
        return true;
    }

} // namespace nkentseu
