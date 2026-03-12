#include "NKSerialization/XML/NkXMLReader.h"

namespace nkentseu {
namespace entseu {

namespace {

NkString NkXMLUnescape(NkStringView input) {
    NkString out;
    out.Reserve(input.Length());

    for (nk_size i = 0; i < input.Length(); ++i) {
        if (input[i] != '&') {
            out.Append(input[i]);
            continue;
        }

        if (input.SubStr(i, 5) == "&amp;") {
            out.Append('&');
            i += 4;
            continue;
        }
        if (input.SubStr(i, 4) == "&lt;") {
            out.Append('<');
            i += 3;
            continue;
        }
        if (input.SubStr(i, 4) == "&gt;") {
            out.Append('>');
            i += 3;
            continue;
        }
        if (input.SubStr(i, 6) == "&quot;") {
            out.Append('"');
            i += 5;
            continue;
        }
        if (input.SubStr(i, 6) == "&apos;") {
            out.Append('\'');
            i += 5;
            continue;
        }

        out.Append('&');
    }

    return out;
}

NkArchiveValueType NkParseType(NkStringView typeText) {
    if (typeText == "null") return NkArchiveValueType::NK_VALUE_NULL;
    if (typeText == "bool") return NkArchiveValueType::NK_VALUE_BOOL;
    if (typeText == "int64") return NkArchiveValueType::NK_VALUE_INT64;
    if (typeText == "uint64") return NkArchiveValueType::NK_VALUE_UINT64;
    if (typeText == "float64") return NkArchiveValueType::NK_VALUE_FLOAT64;
    return NkArchiveValueType::NK_VALUE_STRING;
}

nk_bool NkExtractAttribute(NkStringView tag, NkStringView name, NkString& outValue) {
    const NkString pattern = NkString(name) + "=\"";
    const nk_size pos = tag.Find(pattern.View());
    if (pos == NkString::npos) {
        return false;
    }

    const nk_size start = pos + pattern.Length();
    const nk_size end = tag.Find('"', start);
    if (end == NkString::npos || end <= start) {
        return false;
    }

    outValue = tag.SubStr(start, end - start);
    return true;
}

} // namespace

nk_bool NkXMLReader::ReadArchive(NkStringView xml,
                                 NkArchive& outArchive,
                                 NkString* outError) {
    outArchive.Clear();

    NkString source(xml);
    const nk_size archiveStart = source.Find("<archive>");
    const nk_size archiveEnd = source.RFind("</archive>");

    if (archiveStart == NkString::npos || archiveEnd == NkString::npos || archiveEnd < archiveStart) {
        if (outError) {
            *outError = "Invalid XML archive envelope";
        }
        return false;
    }

    nk_size cursor = archiveStart;
    while (true) {
        const nk_size open = source.Find("<entry", cursor);
        if (open == NkString::npos || open > archiveEnd) {
            break;
        }

        const nk_size closeBracket = source.Find('>', open);
        if (closeBracket == NkString::npos) {
            if (outError) {
                *outError = "Malformed <entry> tag";
            }
            return false;
        }

        const nk_size endTag = source.Find("</entry>", closeBracket);
        if (endTag == NkString::npos) {
            if (outError) {
                *outError = "Missing </entry> end tag";
            }
            return false;
        }

        NkString tagText = source.SubStr(open, closeBracket - open + 1);
        NkString key;
        NkString typeText;

        if (!NkExtractAttribute(tagText.View(), "key", key)) {
            if (outError) {
                *outError = "Entry is missing key attribute";
            }
            return false;
        }

        if (!NkExtractAttribute(tagText.View(), "type", typeText)) {
            typeText = "string";
        }

        NkString valueText = source.SubStr(closeBracket + 1, endTag - (closeBracket + 1));
        NkArchiveValue value;
        value.type = NkParseType(typeText.View());
        value.text = NkXMLUnescape(valueText.View());

        if (value.type == NkArchiveValueType::NK_VALUE_NULL) {
            value.text.Clear();
        }

        NkString decodedKey = NkXMLUnescape(key.View());
        if (!outArchive.SetValue(decodedKey.View(), value)) {
            if (outError) {
                *outError = "Failed to store XML entry";
            }
            return false;
        }

        cursor = endTag + 8;
    }

    return true;
}

} // namespace entseu
} // namespace nkentseu
