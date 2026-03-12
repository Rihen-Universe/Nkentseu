#include "NKSerialization/YAML/NkYAMLReader.h"

namespace nkentseu {
namespace entseu {

namespace {

NkString NkTrimCopy(NkStringView view) {
    NkString s(view);
    s.Trim();
    return s;
}

nk_bool NkParseScalarYaml(NkStringView text, NkArchiveValue& outValue) {
    NkString value = NkTrimCopy(text);

    if (value.Empty() || value == "~" || value == "null") {
        outValue = NkArchiveValue::Null();
        return true;
    }

    if (value == "true") {
        outValue = NkArchiveValue::FromBool(true);
        return true;
    }

    if (value == "false") {
        outValue = NkArchiveValue::FromBool(false);
        return true;
    }

    if (value.Length() >= 2 && value.Front() == '\'' && value.Back() == '\'') {
        NkString unquoted;
        for (nk_size i = 1; i + 1 < value.Length(); ++i) {
            const char ch = value[i];
            if (ch == '\'' && i + 2 < value.Length() && value[i + 1] == '\'') {
                unquoted.Append('\'');
                ++i;
                continue;
            }
            unquoted.Append(ch);
        }
        outValue = NkArchiveValue::FromString(unquoted.View());
        return true;
    }

    if (value.Contains('.') || value.Contains('e') || value.Contains('E')) {
        float64 fv = 0.0;
        if (value.ToDouble(fv)) {
            outValue = NkArchiveValue::FromFloat64(fv);
            return true;
        }
    } else {
        if (!value.Empty() && value[0] == '-') {
            int64 iv = 0;
            if (value.ToInt64(iv)) {
                outValue = NkArchiveValue::FromInt64(iv);
                return true;
            }
        } else {
            uint64 uv = 0;
            if (value.ToUInt64(uv)) {
                outValue = NkArchiveValue::FromUInt64(uv);
                return true;
            }
        }
    }

    outValue = NkArchiveValue::FromString(value.View());
    return true;
}

} // namespace

nk_bool NkYAMLReader::ReadArchive(NkStringView yaml,
                                  NkArchive& outArchive,
                                  NkString* outError) {
    outArchive.Clear();

    NkString source(yaml);
    nk_size cursor = 0;

    while (cursor < source.Length()) {
        nk_size nextLine = source.Find('\n', cursor);
        if (nextLine == NkString::npos) {
            nextLine = source.Length();
        }

        NkString line = source.SubStr(cursor, nextLine - cursor);
        line.Trim();

        cursor = (nextLine == source.Length()) ? source.Length() : (nextLine + 1);

        if (line.Empty() || line.StartsWith("#")) {
            continue;
        }

        const nk_size colon = line.Find(':');
        if (colon == NkString::npos) {
            if (outError) {
                *outError = "Invalid YAML line: missing ':'";
            }
            return false;
        }

        NkString key = line.SubStr(0, colon);
        key.Trim();
        if (key.Empty()) {
            if (outError) {
                *outError = "Invalid YAML line: empty key";
            }
            return false;
        }

        NkStringView valueView(line.CStr() + colon + 1, line.Length() - (colon + 1));
        NkArchiveValue value;
        if (!NkParseScalarYaml(valueView, value)) {
            if (outError) {
                *outError = "Invalid YAML scalar";
            }
            return false;
        }

        if (!outArchive.SetValue(key.View(), value)) {
            if (outError) {
                *outError = "Failed to store YAML key/value";
            }
            return false;
        }
    }

    return true;
}

} // namespace entseu
} // namespace nkentseu
