#include "NKSerialization/JSON/NkJSONWriter.h"

#include "NKSerialization/JSON/NkJSONValue.h"

namespace nkentseu {
namespace entseu {

namespace {

void NkAppendIndent(NkString& out, nk_int32 spaces) {
    for (nk_int32 i = 0; i < spaces; ++i) {
        out.Append(' ');
    }
}

void NkAppendJSONScalar(const NkArchiveValue& value, NkString& out) {
    switch (value.type) {
        case NkArchiveValueType::NK_VALUE_NULL:
            out.Append("null");
            break;
        case NkArchiveValueType::NK_VALUE_BOOL:
        case NkArchiveValueType::NK_VALUE_INT64:
        case NkArchiveValueType::NK_VALUE_UINT64:
        case NkArchiveValueType::NK_VALUE_FLOAT64:
            out.Append(value.text);
            break;
        case NkArchiveValueType::NK_VALUE_STRING:
        default:
            out.Append('"');
            out.Append(NkJSONEscapeString(value.text.View()));
            out.Append('"');
            break;
    }
}

} // namespace

nk_bool NkJSONWriter::WriteArchive(const NkArchive& archive,
                                   NkString& outJson,
                                   nk_bool pretty,
                                   nk_int32 indentSpaces) {
    outJson.Clear();
    outJson.Append('{');

    const NkVector<NkArchiveEntry>& entries = archive.Entries();
    const nk_size count = entries.Size();

    for (nk_size i = 0; i < count; ++i) {
        const NkArchiveEntry& entry = entries[i];

        if (pretty) {
            outJson.Append('\n');
            NkAppendIndent(outJson, indentSpaces);
        }

        outJson.Append('"');
        outJson.Append(NkJSONEscapeString(entry.key.View()));
        outJson.Append('"');
        outJson.Append(':');
        if (pretty) {
            outJson.Append(' ');
        }

        NkAppendJSONScalar(entry.value, outJson);

        if (i + 1 < count) {
            outJson.Append(',');
        }
    }

    if (pretty && count > 0u) {
        outJson.Append('\n');
    }

    outJson.Append('}');
    return true;
}

NkString NkJSONWriter::WriteArchive(const NkArchive& archive, nk_bool pretty, nk_int32 indentSpaces) {
    NkString out;
    WriteArchive(archive, out, pretty, indentSpaces);
    return out;
}

} // namespace entseu
} // namespace nkentseu
