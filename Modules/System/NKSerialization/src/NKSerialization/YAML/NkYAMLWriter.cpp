#include "NKSerialization/YAML/NkYAMLWriter.h"

namespace nkentseu {
namespace entseu {

namespace {

NkString NkYAMLEscapeString(NkStringView input) {
    NkString out;
    out.Append('\'');
    for (nk_size i = 0; i < input.Length(); ++i) {
        if (input[i] == '\'') {
            out.Append("''");
        } else {
            out.Append(input[i]);
        }
    }
    out.Append('\'');
    return out;
}

} // namespace

nk_bool NkYAMLWriter::WriteArchive(const NkArchive& archive, NkString& outYaml) {
    outYaml.Clear();

    const NkVector<NkArchiveEntry>& entries = archive.Entries();
    for (nk_size i = 0; i < entries.Size(); ++i) {
        const NkArchiveEntry& entry = entries[i];

        outYaml.Append(entry.key);
        outYaml.Append(": ");

        switch (entry.value.type) {
            case NkArchiveValueType::NK_VALUE_NULL:
                outYaml.Append("~");
                break;
            case NkArchiveValueType::NK_VALUE_BOOL:
            case NkArchiveValueType::NK_VALUE_INT64:
            case NkArchiveValueType::NK_VALUE_UINT64:
            case NkArchiveValueType::NK_VALUE_FLOAT64:
                outYaml.Append(entry.value.text);
                break;
            case NkArchiveValueType::NK_VALUE_STRING:
            default:
                outYaml.Append(NkYAMLEscapeString(entry.value.text.View()));
                break;
        }

        if (i + 1 < entries.Size()) {
            outYaml.Append('\n');
        }
    }

    return true;
}

NkString NkYAMLWriter::WriteArchive(const NkArchive& archive) {
    NkString out;
    WriteArchive(archive, out);
    return out;
}

} // namespace entseu
} // namespace nkentseu
