#include "NKSerialization/Binary/NkBinaryWriter.h"

namespace nkentseu {
namespace entseu {

namespace {

constexpr nk_uint32 NK_BINARY_MAGIC = 0x314B534Eu; // NKS1
constexpr nk_uint16 NK_BINARY_VERSION = 1u;

void NkWriteU8(NkVector<nk_uint8>& out, nk_uint8 v) {
    out.PushBack(v);
}

void NkWriteU16LE(NkVector<nk_uint8>& out, nk_uint16 v) {
    NkWriteU8(out, static_cast<nk_uint8>(v & 0xFFu));
    NkWriteU8(out, static_cast<nk_uint8>((v >> 8u) & 0xFFu));
}

void NkWriteU32LE(NkVector<nk_uint8>& out, nk_uint32 v) {
    NkWriteU8(out, static_cast<nk_uint8>(v & 0xFFu));
    NkWriteU8(out, static_cast<nk_uint8>((v >> 8u) & 0xFFu));
    NkWriteU8(out, static_cast<nk_uint8>((v >> 16u) & 0xFFu));
    NkWriteU8(out, static_cast<nk_uint8>((v >> 24u) & 0xFFu));
}

void NkWriteBytes(NkVector<nk_uint8>& out, const char* bytes, nk_size size) {
    if (!bytes || size == 0u) {
        return;
    }
    for (nk_size i = 0; i < size; ++i) {
        out.PushBack(static_cast<nk_uint8>(bytes[i]));
    }
}

} // namespace

nk_bool NkBinaryWriter::WriteArchive(const NkArchive& archive, NkVector<nk_uint8>& outData) {
    outData.Clear();

    const NkVector<NkArchiveEntry>& entries = archive.Entries();
    outData.Reserve(16u + entries.Size() * 24u);

    NkWriteU32LE(outData, NK_BINARY_MAGIC);
    NkWriteU16LE(outData, NK_BINARY_VERSION);
    NkWriteU16LE(outData, 0u);
    NkWriteU32LE(outData, static_cast<nk_uint32>(entries.Size()));

    for (nk_size i = 0; i < entries.Size(); ++i) {
        const NkArchiveEntry& entry = entries[i];
        const nk_uint32 keyLen = static_cast<nk_uint32>(entry.key.Length());
        const nk_uint32 valueLen = static_cast<nk_uint32>(entry.value.text.Length());

        NkWriteU32LE(outData, keyLen);
        NkWriteU8(outData, static_cast<nk_uint8>(entry.value.type));
        NkWriteU32LE(outData, valueLen);

        NkWriteBytes(outData, entry.key.CStr(), keyLen);
        NkWriteBytes(outData, entry.value.text.CStr(), valueLen);
    }

    return true;
}

} // namespace entseu
} // namespace nkentseu
