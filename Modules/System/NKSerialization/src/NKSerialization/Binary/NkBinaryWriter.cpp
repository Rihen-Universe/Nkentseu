// =============================================================================
// NKSerialization/Binary/NkBinaryWriter.cpp
// =============================================================================
// Format "NKS0" : archive plate (pas de récursion).
// Pour les nœuds Object/Array, on sérialise leur représentation texte JSON
// afin de garder le format NKS0 rétro-compatible (les objets imbriqués
// nécessitent le format NkNative).
//
// Header :
//   [Magic:4='NKS0'][Version:2=1][Reserved:2][EntryCount:4]
// Chaque entrée :
//   [KeyLen:4][Key:KeyLen][Type:1][ValueLen:4][Value:ValueLen]
// =============================================================================
#include "NKSerialization/Binary/NkBinaryWriter.h"
#include "NKSerialization/JSON/NkJSONWriter.h"

namespace nkentseu {

namespace {

constexpr nk_uint32 NK_BINARY_MAGIC   = 0x304B534Eu; // "NKS0"
constexpr nk_uint16 NK_BINARY_VERSION = 1u;

void WriteU8(NkVector<nk_uint8>& out, nk_uint8 v) noexcept {
    out.PushBack(v);
}
void WriteU16LE(NkVector<nk_uint8>& out, nk_uint16 v) noexcept {
    out.PushBack(static_cast<nk_uint8>(v));
    out.PushBack(static_cast<nk_uint8>(v >> 8u));
}
void WriteU32LE(NkVector<nk_uint8>& out, nk_uint32 v) noexcept {
    out.PushBack(static_cast<nk_uint8>(v));
    out.PushBack(static_cast<nk_uint8>(v >> 8u));
    out.PushBack(static_cast<nk_uint8>(v >> 16u));
    out.PushBack(static_cast<nk_uint8>(v >> 24u));
}
void WriteBytes(NkVector<nk_uint8>& out, const char* s, nk_size n) noexcept {
    for (nk_size i = 0; i < n; ++i) out.PushBack(static_cast<nk_uint8>(s[i]));
}

// Sérialise un nœud en texte pour le Binary plat
NkString NodeToText(const NkArchiveNode& node) noexcept {
    if (node.IsScalar()) return node.value.text;
    if (node.IsObject()) {
        NkString json;
        NkJSONWriter::WriteArchive(*node.object, json, false);
        return json;
    }
    if (node.IsArray()) {
        NkString s("[");
        for (nk_size i = 0; i < node.array.Size(); ++i) {
            if (i > 0) s.Append(',');
            s.Append(NodeToText(node.array[i]));
        }
        s.Append(']');
        return s;
    }
    return {};
}

// Type binaire : on map les nœuds Object/Array sur NK_VALUE_STRING
nk_uint8 NodeToBinaryType(const NkArchiveNode& node) noexcept {
    if (node.IsScalar()) return static_cast<nk_uint8>(node.value.type);
    return static_cast<nk_uint8>(NkArchiveValueType::NK_VALUE_STRING);
}

} // namespace

nk_bool NkBinaryWriter::WriteArchive(const NkArchive& archive, NkVector<nk_uint8>& outData) {
    outData.Clear();

    const auto& entries = archive.Entries();
    outData.Reserve(16u + entries.Size() * 32u);

    WriteU32LE(outData, NK_BINARY_MAGIC);
    WriteU16LE(outData, NK_BINARY_VERSION);
    WriteU16LE(outData, 0u); // reserved
    WriteU32LE(outData, static_cast<nk_uint32>(entries.Size()));

    for (nk_size i = 0; i < entries.Size(); ++i) {
        const NkArchiveEntry& e = entries[i];
        nk_uint32 keyLen = static_cast<nk_uint32>(e.key.Length());

        NkString valueText = NodeToText(e.node);
        nk_uint32 valLen  = static_cast<nk_uint32>(valueText.Length());
        nk_uint8  typeRaw = NodeToBinaryType(e.node);

        WriteU32LE(outData, keyLen);
        WriteU8(outData, typeRaw);
        WriteU32LE(outData, valLen);
        WriteBytes(outData, e.key.CStr(), keyLen);
        WriteBytes(outData, valueText.CStr(), valLen);
    }

    return true;
}

} // namespace nkentseu
