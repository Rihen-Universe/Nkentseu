// =============================================================================
// NKSerialization/XML/NkXMLWriter.cpp
// =============================================================================
// Corrections vs version originale :
//  - namespace entseu supprimé (tout dans nkentseu)
//  - Support des nœuds Object et Array (récursifs)
//  - Indentation correcte sur plusieurs niveaux
//  - Attribut type="object" / type="array" pour le parsing
// =============================================================================
#include "NKSerialization/XML/NkXMLWriter.h"

namespace nkentseu {

    namespace {

        void NkAppendIndent(NkString& out, nk_bool pretty, nk_int32 spaces, nk_int32 level) noexcept {
            if (!pretty) return;
            out.Append('\n');
            for (nk_int32 i = 0; i < level * spaces; ++i) out.Append(' ');
        }

        NkString NkXMLEscape(NkStringView input) noexcept {
            NkString out;
            out.Reserve(input.Length() + 8u);
            for (nk_size i = 0; i < input.Length(); ++i) {
                switch (input[i]) {
                    case '&':  out.Append("&amp;");  break;
                    case '<':  out.Append("&lt;");   break;
                    case '>':  out.Append("&gt;");   break;
                    case '"':  out.Append("&quot;"); break;
                    case '\'': out.Append("&apos;"); break;
                    default:   out.Append(input[i]); break;
                }
            }
            return out;
        }

        const char* NkTypeStr(NkArchiveValueType t) noexcept {
            switch (t) {
                case NkArchiveValueType::NK_VALUE_NULL:    return "null";
                case NkArchiveValueType::NK_VALUE_BOOL:    return "bool";
                case NkArchiveValueType::NK_VALUE_INT64:   return "int64";
                case NkArchiveValueType::NK_VALUE_UINT64:  return "uint64";
                case NkArchiveValueType::NK_VALUE_FLOAT64: return "float64";
                default:                                   return "string";
            }
        }

        // Forward declaration
        void NkWriteArchiveXML(const NkArchive& archive, NkString& out, nk_bool pretty, nk_int32 spaces, nk_int32 level) noexcept;

        void NkWriteNodeXML(NkStringView key, const NkArchiveNode& node, NkString& out, nk_bool pretty, nk_int32 spaces, nk_int32 level) noexcept {
            if (node.IsScalar()) {
                NkAppendIndent(out, pretty, spaces, level);
                out.Append("<entry key=\"");
                out.Append(NkXMLEscape(key));
                out.Append("\" type=\"");
                out.Append(NkTypeStr(node.value.type));
                out.Append("\">");
                if (node.value.type != NkArchiveValueType::NK_VALUE_NULL)
                    out.Append(NkXMLEscape(node.value.text.View()));
                out.Append("</entry>");

            } else if (node.IsObject()) {
                NkAppendIndent(out, pretty, spaces, level);
                out.Append("<object key=\"");
                out.Append(NkXMLEscape(key));
                out.Append("\">");
                NkWriteArchiveXML(*node.object, out, pretty, spaces, level + 1);
                NkAppendIndent(out, pretty, spaces, level);
                out.Append("</object>");

            } else if (node.IsArray()) {
                NkAppendIndent(out, pretty, spaces, level);
                out.Append("<array key=\"");
                out.Append(NkXMLEscape(key));
                out.Append("\" count=\"");
                out.Append(NkString::Fmtf("%zu", node.array.Size()));
                out.Append("\">");
                for (nk_size i = 0; i < node.array.Size(); ++i) {
                    NkString idx = NkString::Fmtf("%zu", i);
                    NkWriteNodeXML(idx.View(), node.array[i], out, pretty, spaces, level + 1);
                }
                NkAppendIndent(out, pretty, spaces, level);
                out.Append("</array>");
            }
        }

        void NkWriteArchiveXML(const NkArchive& archive, NkString& out, nk_bool pretty, nk_int32 spaces, nk_int32 level) noexcept {
            const auto& entries = archive.Entries();
            for (nk_size i = 0; i < entries.Size(); ++i) {
                NkWriteNodeXML(entries[i].key.View(), entries[i].node, out, pretty, spaces, level);
            }
        }

    } // namespace

    nk_bool NkXMLWriter::WriteArchive(const NkArchive& archive, NkString& outXml, nk_bool pretty, nk_int32 indentSpaces) {
        outXml.Clear();
        outXml.Append("<archive>");
        NkWriteArchiveXML(archive, outXml, pretty, indentSpaces, 1);
        if (pretty && !archive.Empty()) outXml.Append('\n');
        outXml.Append("</archive>");
        return true;
    }

    NkString NkXMLWriter::WriteArchive(const NkArchive& archive, nk_bool pretty, nk_int32 indentSpaces) {
        NkString out;
        NkWriteArchiveXML(archive, out, pretty, indentSpaces, 1);
        return out;
    }

} // namespace nkentseu
