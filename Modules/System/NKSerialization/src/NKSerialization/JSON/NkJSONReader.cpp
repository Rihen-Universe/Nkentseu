#include "NKSerialization/JSON/NkJSONReader.h"

#include "NKSerialization/JSON/NkJSONValue.h"

namespace nkentseu {
namespace entseu {

namespace {

struct NkJSONParser {
    NkStringView text;
    nk_size pos = 0;

    void SkipWS() {
        while (pos < text.Length()) {
            const char ch = text[pos];
            if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
                ++pos;
                continue;
            }
            break;
        }
    }

    nk_bool Match(char expected) {
        SkipWS();
        if (pos < text.Length() && text[pos] == expected) {
            ++pos;
            return true;
        }
        return false;
    }

    nk_bool ParseStringLiteral(NkString& outValue) {
        SkipWS();
        if (pos >= text.Length() || text[pos] != '"') {
            return false;
        }

        ++pos;
        NkString raw;
        while (pos < text.Length()) {
            const char ch = text[pos++];
            if (ch == '"') {
                nk_bool ok = true;
                outValue = NkJSONUnescapeString(raw.View(), ok);
                return ok;
            }

            if (ch == '\\') {
                if (pos >= text.Length()) {
                    return false;
                }
                raw.Append('\\');
                raw.Append(text[pos++]);
                continue;
            }

            raw.Append(ch);
        }

        return false;
    }

    nk_bool ParseLiteral(const char* literal) {
        SkipWS();
        if (!literal) {
            return false;
        }

        nk_size i = 0;
        while (literal[i] != '\0') {
            if (pos + i >= text.Length() || text[pos + i] != literal[i]) {
                return false;
            }
            ++i;
        }

        pos += i;
        return true;
    }

    nk_bool ParseNumberToken(NkString& tokenOut) {
        SkipWS();
        const nk_size start = pos;

        if (pos < text.Length() && (text[pos] == '-' || text[pos] == '+')) {
            ++pos;
        }

        nk_bool hasDigits = false;
        while (pos < text.Length() && text[pos] >= '0' && text[pos] <= '9') {
            ++pos;
            hasDigits = true;
        }

        if (pos < text.Length() && text[pos] == '.') {
            ++pos;
            while (pos < text.Length() && text[pos] >= '0' && text[pos] <= '9') {
                ++pos;
                hasDigits = true;
            }
        }

        if (pos < text.Length() && (text[pos] == 'e' || text[pos] == 'E')) {
            ++pos;
            if (pos < text.Length() && (text[pos] == '-' || text[pos] == '+')) {
                ++pos;
            }
            while (pos < text.Length() && text[pos] >= '0' && text[pos] <= '9') {
                ++pos;
                hasDigits = true;
            }
        }

        if (!hasDigits) {
            return false;
        }

        tokenOut = NkString(text.Data() + start, pos - start);
        return true;
    }

    nk_bool ParseScalarValue(NkArchiveValue& outValue) {
        SkipWS();
        if (pos >= text.Length()) {
            return false;
        }

        const char ch = text[pos];
        if (ch == '"') {
            NkString s;
            if (!ParseStringLiteral(s)) {
                return false;
            }
            outValue = NkArchiveValue::FromString(s.View());
            return true;
        }

        if (ch == 't') {
            if (!ParseLiteral("true")) {
                return false;
            }
            outValue = NkArchiveValue::FromBool(true);
            return true;
        }

        if (ch == 'f') {
            if (!ParseLiteral("false")) {
                return false;
            }
            outValue = NkArchiveValue::FromBool(false);
            return true;
        }

        if (ch == 'n') {
            if (!ParseLiteral("null")) {
                return false;
            }
            outValue = NkArchiveValue::Null();
            return true;
        }

        NkString numberToken;
        if (!ParseNumberToken(numberToken)) {
            return false;
        }

        if (numberToken.Contains('.') || numberToken.Contains('e') || numberToken.Contains('E')) {
            float64 v = 0.0;
            if (!numberToken.ToDouble(v)) {
                return false;
            }
            outValue = NkArchiveValue::FromFloat64(v);
            return true;
        }

        if (!numberToken.Empty() && numberToken[0] == '-') {
            int64 v = 0;
            if (!numberToken.ToInt64(v)) {
                return false;
            }
            outValue = NkArchiveValue::FromInt64(v);
            return true;
        }

        uint64 uv = 0;
        if (!numberToken.ToUInt64(uv)) {
            int64 sv = 0;
            if (!numberToken.ToInt64(sv)) {
                return false;
            }
            outValue = NkArchiveValue::FromInt64(sv);
            return true;
        }

        outValue = NkArchiveValue::FromUInt64(uv);
        return true;
    }
};

} // namespace

nk_bool NkJSONReader::ReadArchive(NkStringView json,
                                  NkArchive& outArchive,
                                  NkString* outError) {
    outArchive.Clear();

    NkJSONParser parser{json, 0};
    if (!parser.Match('{')) {
        if (outError) {
            *outError = "JSON object must start with '{'";
        }
        return false;
    }

    parser.SkipWS();
    if (parser.Match('}')) {
        return true;
    }

    while (true) {
        NkString key;
        if (!parser.ParseStringLiteral(key)) {
            if (outError) {
                *outError = "Expected JSON string key";
            }
            return false;
        }

        if (!parser.Match(':')) {
            if (outError) {
                *outError = "Expected ':' after key";
            }
            return false;
        }

        NkArchiveValue value;
        if (!parser.ParseScalarValue(value)) {
            if (outError) {
                *outError = "Expected scalar JSON value";
            }
            return false;
        }

        if (!outArchive.SetValue(key.View(), value)) {
            if (outError) {
                *outError = "Failed to store parsed key/value";
            }
            return false;
        }

        parser.SkipWS();
        if (parser.Match('}')) {
            break;
        }

        if (!parser.Match(',')) {
            if (outError) {
                *outError = "Expected ',' or '}' after value";
            }
            return false;
        }
    }

    parser.SkipWS();
    if (parser.pos != parser.text.Length()) {
        if (outError) {
            *outError = "Trailing characters after JSON object";
        }
        return false;
    }

    return true;
}

} // namespace entseu
} // namespace nkentseu
