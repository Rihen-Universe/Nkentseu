// =============================================================================
// NKSerialization/JSON/NkJSONWriter.cpp
// =============================================================================
// Corrections :
//  - Support des noeuds Object et Array (recursive)
//  - Indentation correcte pour les niveaux imbriqués
// =============================================================================
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/JSON/NkJSONValue.h"

namespace nkentseu {

namespace {

void Indent(NkString& out, int level, int spaces) noexcept {
    for (int i = 0; i < level * spaces; ++i) out.Append(' ');
}

void WriteNodeJSON(const NkArchiveNode& node, NkString& out,
                   bool pretty, int indent, int level) noexcept;

void WriteValueJSON(const NkArchiveValue& v, NkString& out) noexcept {
    switch (v.type) {
        case NkArchiveValueType::NK_VALUE_NULL:
            out.Append("null");
            break;
        case NkArchiveValueType::NK_VALUE_BOOL:
        case NkArchiveValueType::NK_VALUE_INT64:
        case NkArchiveValueType::NK_VALUE_UINT64:
        case NkArchiveValueType::NK_VALUE_FLOAT64:
            out.Append(v.text);
            break;
        case NkArchiveValueType::NK_VALUE_STRING:
        default:
            out.Append('"');
            out.Append(NkJSONEscapeString(v.text.View()));
            out.Append('"');
            break;
    }
}

void WriteArchiveJSON(const NkArchive& archive, NkString& out,
                      bool pretty, int indent, int level) noexcept {
    const auto& entries = archive.Entries();
    out.Append('{');
    for (nk_size i = 0; i < entries.Size(); ++i) {
        if (pretty) { out.Append('\n'); Indent(out, level + 1, indent); }
        out.Append('"');
        out.Append(NkJSONEscapeString(entries[i].key.View()));
        out.Append('"');
        out.Append(':');
        if (pretty) out.Append(' ');
        WriteNodeJSON(entries[i].node, out, pretty, indent, level + 1);
        if (i + 1 < entries.Size()) out.Append(',');
    }
    if (pretty && !entries.Empty()) { out.Append('\n'); Indent(out, level, indent); }
    out.Append('}');
}

void WriteNodeJSON(const NkArchiveNode& node, NkString& out,
                   bool pretty, int indent, int level) noexcept {
    if (node.IsScalar()) {
        WriteValueJSON(node.value, out);
    } else if (node.IsObject()) {
        WriteArchiveJSON(*node.object, out, pretty, indent, level);
    } else if (node.IsArray()) {
        out.Append('[');
        for (nk_size i = 0; i < node.array.Size(); ++i) {
            if (pretty) { out.Append('\n'); Indent(out, level + 1, indent); }
            WriteNodeJSON(node.array[i], out, pretty, indent, level + 1);
            if (i + 1 < node.array.Size()) out.Append(',');
        }
        if (pretty && !node.array.Empty()) { out.Append('\n'); Indent(out, level, indent); }
        out.Append(']');
    }
}

} // namespace

nk_bool NkJSONWriter::WriteArchive(const NkArchive& archive,
                                   NkString& outJson,
                                   nk_bool pretty,
                                   nk_int32 indentSpaces) {
    outJson.Clear();
    WriteArchiveJSON(archive, outJson, pretty, static_cast<int>(indentSpaces), 0);
    return true;
}

NkString NkJSONWriter::WriteArchive(const NkArchive& archive, nk_bool pretty, nk_int32 indentSpaces) {
    NkString out;
    WriteArchive(archive, out, pretty, indentSpaces);
    return out;
}

} // namespace nkentseu
