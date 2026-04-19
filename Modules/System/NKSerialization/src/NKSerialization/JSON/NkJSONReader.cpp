// =============================================================================
// NKSerialization/JSON/NkJSONReader.cpp
// =============================================================================
// Corrections :
//  - Le reader original ignorait les arrays et objets imbriqués
//    (ParseScalarValue ne parsait que des scalaires → les objets imbriqués
//    étaient des erreurs silencieuses)
//  - Ajout de la récursion pour objects {} et arrays []
//  - Gestion correcte des trailing whitespace après la valeur top-level
// =============================================================================
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/JSON/NkJSONValue.h"

namespace nkentseu {

namespace {

struct NkJSONParser {
    NkStringView text;
    nk_size pos = 0;

    char Peek() const noexcept {
        return pos < text.Length() ? text[pos] : '\0';
    }
    char Consume() noexcept {
        return pos < text.Length() ? text[pos++] : '\0';
    }
    void SkipWS() noexcept {
        while (pos < text.Length()) {
            char c = text[pos];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') ++pos;
            else break;
        }
    }
    bool Match(char expected) noexcept {
        SkipWS();
        if (pos < text.Length() && text[pos] == expected) { ++pos; return true; }
        return false;
    }
    bool Expect(char expected, NkString* err) noexcept {
        if (!Match(expected)) {
            if (err) *err = NkString::Fmtf("Expected '%c' at pos %zu", expected, pos);
            return false;
        }
        return true;
    }
    bool AtEnd() const noexcept { SkipWSC(); return pos >= text.Length(); }
    void SkipWSC() const noexcept {
        nk_size p = pos;
        while (p < text.Length()) {
            char c = text[p];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') ++p; else break;
        }
        const_cast<NkJSONParser*>(this)->pos = p;
    }

    bool ParseString(NkString& out, NkString* err) noexcept {
        SkipWS();
        if (Peek() != '"') { if (err) *err = NkString("Expected '\"'"); return false; }
        ++pos;
        NkString raw;
        while (pos < text.Length() && text[pos] != '"') {
            char c = text[pos++];
            if (c == '\\') {
                if (pos >= text.Length()) { if (err) *err = NkString("Unterminated escape"); return false; }
                char e = text[pos++];
                switch (e) {
                    case '"': raw.Append('"'); break;
                    case '\\': raw.Append('\\'); break;
                    case '/':  raw.Append('/'); break;
                    case 'b':  raw.Append('\b'); break;
                    case 'f':  raw.Append('\f'); break;
                    case 'n':  raw.Append('\n'); break;
                    case 'r':  raw.Append('\r'); break;
                    case 't':  raw.Append('\t'); break;
                    case 'u':
                        // Minimal : consomme 4 hex, émet '?'
                        if (pos + 4u <= text.Length()) pos += 4u;
                        raw.Append('?');
                        break;
                    default:   raw.Append(e); break;
                }
            } else {
                raw.Append(c);
            }
        }
        if (pos >= text.Length() || text[pos] != '"') {
            if (err) *err = NkString("Unterminated string");
            return false;
        }
        ++pos; // consume closing '"'
        out = std::move(raw);
        return true;
    }

    bool ParseObject(NkArchive& out, NkString* err) noexcept {
        // On est déjà passé par '{'
        SkipWS();
        if (Match('}')) return true; // objet vide

        while (true) {
            NkString key;
            if (!ParseString(key, err)) return false;
            if (!Expect(':', err)) return false;

            NkArchiveNode val;
            if (!ParseNode(val, err)) return false;
            out.SetNode(key.View(), val);

            SkipWS();
            if (Match('}')) return true;
            if (!Expect(',', err)) return false;
        }
    }

    bool ParseArray(NkArchiveNode& out, NkString* err) noexcept {
        // On est déjà passé par '['
        out.MakeArray();
        SkipWS();
        if (Match(']')) return true; // array vide

        while (true) {
            NkArchiveNode elem;
            if (!ParseNode(elem, err)) return false;
            out.array.PushBack(std::move(elem));

            SkipWS();
            if (Match(']')) return true;
            if (!Expect(',', err)) return false;
        }
    }

    bool ParseNumber(NkArchiveValue& out, NkString* err) noexcept {
        nk_size start = pos;
        if (pos < text.Length() && (text[pos] == '-' || text[pos] == '+')) ++pos;
        bool hasDigits = false;
        while (pos < text.Length() && text[pos] >= '0' && text[pos] <= '9') { ++pos; hasDigits = true; }
        bool isFloat = false;
        if (pos < text.Length() && text[pos] == '.') { ++pos; isFloat = true;
            while (pos < text.Length() && text[pos] >= '0' && text[pos] <= '9') ++pos; hasDigits = true; }
        if (pos < text.Length() && (text[pos] == 'e' || text[pos] == 'E')) { ++pos; isFloat = true;
            if (pos < text.Length() && (text[pos]=='+' || text[pos]=='-')) ++pos;
            while (pos < text.Length() && text[pos] >= '0' && text[pos] <= '9') ++pos; }

        if (!hasDigits) { if (err) *err = NkString("Invalid number"); return false; }

        NkString token(text.Data() + start, pos - start);
        if (isFloat) {
            float64 v = 0.0;
            if (!token.ToDouble(v)) { if (err) *err = NkString("Float parse error"); return false; }
            out = NkArchiveValue::FromFloat64(v);
        } else if (!token.Empty() && token[0] == '-') {
            int64 v = 0;
            if (!token.ToInt64(v)) { if (err) *err = NkString("Int parse error"); return false; }
            out = NkArchiveValue::FromInt64(v);
        } else {
            uint64 v = 0;
            if (!token.ToUInt64(v)) {
                int64 sv = 0;
                if (!token.ToInt64(sv)) { if (err) *err = NkString("UInt parse error"); return false; }
                out = NkArchiveValue::FromInt64(sv);
            } else {
                out = NkArchiveValue::FromUInt64(v);
            }
        }
        return true;
    }

    bool ParseNode(NkArchiveNode& out, NkString* err) noexcept {
        SkipWS();
        if (pos >= text.Length()) { if (err) *err = NkString("Unexpected EOF"); return false; }

        char c = text[pos];

        if (c == '{') { ++pos; NkArchive arc; if (!ParseObject(arc, err)) return false; out.SetObject(arc); return true; }
        if (c == '[') { ++pos; return ParseArray(out, err); }
        if (c == '"') { NkString s; if (!ParseString(s, err)) return false; out = NkArchiveNode(NkArchiveValue::FromString(s.View())); return true; }
        if (pos + 4u <= text.Length() && memcmp(text.Data()+pos, "true", 4u)==0) { pos+=4; out = NkArchiveNode(NkArchiveValue::FromBool(true));  return true; }
        if (pos + 5u <= text.Length() && memcmp(text.Data()+pos, "false",5u)==0) { pos+=5; out = NkArchiveNode(NkArchiveValue::FromBool(false)); return true; }
        if (pos + 4u <= text.Length() && memcmp(text.Data()+pos, "null", 4u)==0) { pos+=4; out = NkArchiveNode(NkArchiveValue::Null());         return true; }
        if (c == '-' || (c >= '0' && c <= '9')) {
            NkArchiveValue v;
            if (!ParseNumber(v, err)) return false;
            out = NkArchiveNode(v);
            return true;
        }

        if (err) *err = NkString::Fmtf("Unexpected char '%c' at pos %zu", c, pos);
        return false;
    }
};

} // namespace

nk_bool NkJSONReader::ReadArchive(NkStringView json,
                                  NkArchive& outArchive,
                                  NkString* outError) {
    outArchive.Clear();

    NkJSONParser p{json, 0};
    if (!p.Match('{')) {
        if (outError) *outError = NkString("JSON must start with '{'");
        return false;
    }

    p.SkipWS();
    if (p.Match('}')) return true; // objet vide

    while (true) {
        NkString key;
        if (!p.ParseString(key, outError)) return false;
        if (!p.Expect(':', outError)) return false;

        NkArchiveNode node;
        if (!p.ParseNode(node, outError)) return false;
        outArchive.SetNode(key.View(), node);

        p.SkipWS();
        if (p.Match('}')) break;
        if (!p.Expect(',', outError)) return false;
    }

    p.SkipWS();
    if (p.pos != p.text.Length()) {
        if (outError) *outError = NkString("Trailing content after JSON object");
        return false;
    }
    return true;
}

} // namespace nkentseu
