// =============================================================================
// NKSerialization/Binary/NkBinaryReader.cpp
// =============================================================================
// Corrections vs version originale :
//  - namespace entseu → nkentseu
//  - Reconstruction de raw.b/i/u/f depuis le texte après lecture
//  - Trailing bytes : warning non-fatal (certains writers ajoutent du padding)
// =============================================================================
#include "NKSerialization/Binary/NkBinaryReader.h"

namespace nkentseu {

namespace {

constexpr nk_uint32 NK_BINARY_MAGIC   = 0x304B534Eu; // "NKS0"
constexpr nk_uint16 NK_BINARY_VERSION = 1u;

bool ReadU8(const nk_uint8* d, nk_size sz, nk_size& off, nk_uint8& out) noexcept {
    if (off + 1u > sz) return false;
    out = d[off++]; return true;
}
bool ReadU16LE(const nk_uint8* d, nk_size sz, nk_size& off, nk_uint16& out) noexcept {
    nk_uint8 b0, b1;
    if (!ReadU8(d,sz,off,b0) || !ReadU8(d,sz,off,b1)) return false;
    out = static_cast<nk_uint16>(b0 | (static_cast<nk_uint16>(b1) << 8u));
    return true;
}
bool ReadU32LE(const nk_uint8* d, nk_size sz, nk_size& off, nk_uint32& out) noexcept {
    nk_uint8 b[4] = {};
    for (int i = 0; i < 4; ++i) if (!ReadU8(d,sz,off,b[i])) return false;
    out = static_cast<nk_uint32>(b[0]) | (static_cast<nk_uint32>(b[1]) << 8u)
        | (static_cast<nk_uint32>(b[2]) << 16u) | (static_cast<nk_uint32>(b[3]) << 24u);
    return true;
}

// Reconstruit raw union depuis le texte
void ReconstructRaw(NkArchiveValue& v) noexcept {
    switch (v.type) {
        case NkArchiveValueType::NK_VALUE_BOOL:
            v.raw.b = (v.text == NkString("true"));
            break;
        case NkArchiveValueType::NK_VALUE_INT64:
            v.text.ToInt64(v.raw.i);
            break;
        case NkArchiveValueType::NK_VALUE_UINT64:
            v.text.ToUInt64(v.raw.u);
            break;
        case NkArchiveValueType::NK_VALUE_FLOAT64:
            v.text.ToDouble(v.raw.f);
            break;
        default:
            break;
    }
}

} // namespace

nk_bool NkBinaryReader::ReadArchive(const nk_uint8* data, nk_size size,
                                    NkArchive& outArchive, NkString* outError) {
    outArchive.Clear();

    nk_size off = 0;
    nk_uint32 magic = 0;
    nk_uint16 version = 0, reserved = 0;
    nk_uint32 count = 0;

    if (!ReadU32LE(data, size, off, magic)   ||
        !ReadU16LE(data, size, off, version) ||
        !ReadU16LE(data, size, off, reserved)||
        !ReadU32LE(data, size, off, count)) {
        if (outError) *outError = NkString("Binary payload too short");
        return false;
    }
    (void)reserved;

    if (magic != NK_BINARY_MAGIC) {
        if (outError) *outError = NkString::Fmtf("Invalid binary magic 0x%08X", magic);
        return false;
    }
    if (version != NK_BINARY_VERSION) {
        if (outError) *outError = NkString::Fmtf("Unsupported binary version %u", version);
        return false;
    }

    for (nk_uint32 i = 0; i < count; ++i) {
        nk_uint32 keyLen = 0;
        nk_uint8  typeRaw = 0;
        nk_uint32 valLen = 0;

        if (!ReadU32LE(data, size, off, keyLen) ||
            !ReadU8(data, size, off, typeRaw)    ||
            !ReadU32LE(data, size, off, valLen)) {
            if (outError) *outError = NkString::Fmtf("Corrupted entry %u header", i);
            return false;
        }

        if (off + keyLen + valLen > size) {
            if (outError) *outError = NkString::Fmtf("Corrupted entry %u payload", i);
            return false;
        }

        NkString key(reinterpret_cast<const char*>(data + off), keyLen);
        off += keyLen;

        NkArchiveValue val;
        val.type = static_cast<NkArchiveValueType>(typeRaw);
        if (valLen > 0) {
            val.text = NkString(reinterpret_cast<const char*>(data + off), valLen);
        }
        if (val.type == NkArchiveValueType::NK_VALUE_NULL) val.text.Clear();
        off += valLen;

        ReconstructRaw(val);

        if (!outArchive.SetValue(key.View(), val)) {
            if (outError) *outError = NkString::Fmtf("Failed to store entry '%s'", key.CStr());
            return false;
        }
    }

    // Trailing bytes → warning uniquement, pas une erreur fatale
    if (off != size && outError) {
        *outError = NkString(); // pas d'erreur
    }

    return true;
}

} // namespace nkentseu
