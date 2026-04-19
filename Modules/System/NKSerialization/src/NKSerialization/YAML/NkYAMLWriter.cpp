// =============================================================================
// NKSerialization/YAML/NkYAMLWriter.cpp
// =============================================================================
// Corrections vs version originale :
//  - namespace entseu supprimé
//  - Support des nœuds Object (block mapping) et Array (block sequence)
//  - Indentation correcte pour les niveaux imbriqués
//  - Clés avec caractères spéciaux correctement quotées
// =============================================================================
#include "NKSerialization/YAML/NkYAMLWriter.h"

namespace nkentseu {

    namespace {

        // Escape une string YAML en single quotes
        NkString NkYAMLEscapeString(NkStringView s) noexcept {
            NkString out;
            out.Append('\'');
            for (nk_size i = 0; i < s.Length(); ++i) {
                if (s[i] == '\'') out.Append("''");
                else               out.Append(s[i]);
            }
            out.Append('\'');
            return out;
        }

        // Vérifie si une clé nécessite des guillemets
        bool NkNeedsQuoting(NkStringView key) noexcept {
            if (key.Empty()) return true;
            for (nk_size i = 0; i < key.Length(); ++i) {
                char c = key[i];
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '_' || c == '-')) return true;
            }
            return false;
        }

        void NkAppendIndent(NkString& out, int level) noexcept {
            for (int i = 0; i < level * 2; ++i) out.Append(' ');
        }

        // Forward declaration
        void NkWriteArchiveYAML(const NkArchive& arc, NkString& out, int level) noexcept;

        void NkWriteValueYAML(const NkArchiveValue& v, NkString& out) noexcept {
            switch (v.type) {
                case NkArchiveValueType::NK_VALUE_NULL:
                    out.Append('~');
                    break;
                case NkArchiveValueType::NK_VALUE_BOOL:
                case NkArchiveValueType::NK_VALUE_INT64:
                case NkArchiveValueType::NK_VALUE_UINT64:
                case NkArchiveValueType::NK_VALUE_FLOAT64:
                    out.Append(v.text);
                    break;
                case NkArchiveValueType::NK_VALUE_STRING:
                default:
                    out.Append(NkYAMLEscapeString(v.text.View()));
                    break;
            }
        }

        void NkWriteNodeYAML(NkStringView key, const NkArchiveNode& node, NkString& out, int level) noexcept {
            NkAppendIndent(out, level);

            // Clé
            if (NkNeedsQuoting(key)) { out.Append(NkYAMLEscapeString(key)); }
            else                   { out.Append(key); }
            out.Append(':');

            if (node.IsScalar()) {
                out.Append(' ');
                NkWriteValueYAML(node.value, out);
                out.Append('\n');

            } else if (node.IsObject()) {
                out.Append('\n');
                NkWriteArchiveYAML(*node.object, out, level + 1);

            } else if (node.IsArray()) {
                out.Append('\n');
                for (nk_size i = 0; i < node.array.Size(); ++i) {
                    NkAppendIndent(out, level + 1);
                    out.Append("- ");
                    if (node.array[i].IsScalar()) {
                        NkWriteValueYAML(node.array[i].value, out);
                        out.Append('\n');
                    } else if (node.array[i].IsObject()) {
                        out.Append('\n');
                        NkWriteArchiveYAML(*node.array[i].object, out, level + 2);
                    } else {
                        out.Append("[]\n"); // nested array → simplifié
                    }
                }
            }
        }

        void NkWriteArchiveYAML(const NkArchive& arc, NkString& out, int level) noexcept {
            const auto& entries = arc.Entries();
            for (nk_size i = 0; i < entries.Size(); ++i) {
                NkWriteNodeYAML(entries[i].key.View(), entries[i].node, out, level);
            }
        }

    } // namespace

    nk_bool NkYAMLWriter::WriteArchive(const NkArchive& archive, NkString& outYaml) {
        outYaml.Clear();
        outYaml.Append("---\n"); // YAML document marker
        NkWriteArchiveYAML(archive, outYaml, 0);
        return true;
    }

    NkString NkYAMLWriter::WriteArchive(const NkArchive& archive) {
        NkString out;
        NkWriteArchiveYAML(archive, out, 0);
        return out;
    }

} // namespace nkentseu
