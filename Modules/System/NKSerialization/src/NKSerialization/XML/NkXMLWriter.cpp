#include "NKSerialization/XML/NkXMLWriter.h"

namespace nkentseu {
namespace entseu {

namespace {

void NkAppendIndent(NkString& out, nk_int32 spaces) {
    for (nk_int32 i = 0; i < spaces; ++i) {
        out.Append(' ');
    }
}

NkString NkXMLEscape(NkStringView input) {
    NkString out;
    out.Reserve(input.Length() + 8);
    for (nk_size i = 0; i < input.Length(); ++i) {
        const char ch = input[i];
        switch (ch) {
            case '&': out.Append("&amp;"); break;
            case '<': out.Append("&lt;"); break;
            case '>': out.Append("&gt;"); break;
            case '"': out.Append("&quot;"); break;
            case '\'': out.Append("&apos;"); break;
            default: out.Append(ch); break;
        }
    }
    return out;
}

const char* NkTypeToString(NkArchiveValueType type) {
    switch (type) {
        case NkArchiveValueType::NK_VALUE_NULL: return "null";
        case NkArchiveValueType::NK_VALUE_BOOL: return "bool";
        case NkArchiveValueType::NK_VALUE_INT64: return "int64";
        case NkArchiveValueType::NK_VALUE_UINT64: return "uint64";
        case NkArchiveValueType::NK_VALUE_FLOAT64: return "float64";
        case NkArchiveValueType::NK_VALUE_STRING:
        default: return "string";
    }
}

} // namespace

nk_bool NkXMLWriter::WriteArchive(const NkArchive& archive,
                                  NkString& outXml,
                                  nk_bool pretty,
                                  nk_int32 indentSpaces) {
    outXml.Clear();
    outXml.Append("<archive>");

    const NkVector<NkArchiveEntry>& entries = archive.Entries();
    for (nk_size i = 0; i < entries.Size(); ++i) {
        const NkArchiveEntry& entry = entries[i];

        if (pretty) {
            outXml.Append('\n');
            NkAppendIndent(outXml, indentSpaces);
        }

        outXml.Append("<entry key=\"");
        outXml.Append(NkXMLEscape(entry.key.View()));
        outXml.Append("\" type=\"");
        outXml.Append(NkTypeToString(entry.value.type));
        outXml.Append("\">");
        outXml.Append(NkXMLEscape(entry.value.text.View()));
        outXml.Append("</entry>");
    }

    if (pretty && !entries.Empty()) {
        outXml.Append('\n');
    }

    outXml.Append("</archive>");
    return true;
}

NkString NkXMLWriter::WriteArchive(const NkArchive& archive, nk_bool pretty, nk_int32 indentSpaces) {
    NkString out;
    WriteArchive(archive, out, pretty, indentSpaces);
    return out;
}

} // namespace entseu
} // namespace nkentseu
