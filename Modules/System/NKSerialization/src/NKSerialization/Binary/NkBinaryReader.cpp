#include "NKSerialization/Binary/NkBinaryReader.h"

namespace nkentseu {
namespace entseu {

namespace {

constexpr nk_uint32 NK_BINARY_MAGIC = 0x314B534Eu; // NKS1
constexpr nk_uint16 NK_BINARY_VERSION = 1u;

nk_bool NkReadU8(const nk_uint8* data, nk_size size, nk_size& offset, nk_uint8& out) {
    if (!data || offset + 1u > size) {
        return false;
    }
    out = data[offset];
    offset += 1u;
    return true;
}

nk_bool NkReadU16LE(const nk_uint8* data, nk_size size, nk_size& offset, nk_uint16& out) {
    nk_uint8 b0 = 0;
    nk_uint8 b1 = 0;
    if (!NkReadU8(data, size, offset, b0) || !NkReadU8(data, size, offset, b1)) {
        return false;
    }
    out = static_cast<nk_uint16>(b0 | (static_cast<nk_uint16>(b1) << 8u));
    return true;
}

nk_bool NkReadU32LE(const nk_uint8* data, nk_size size, nk_size& offset, nk_uint32& out) {
    nk_uint8 b0 = 0;
    nk_uint8 b1 = 0;
    nk_uint8 b2 = 0;
    nk_uint8 b3 = 0;
    if (!NkReadU8(data, size, offset, b0) || !NkReadU8(data, size, offset, b1) ||
        !NkReadU8(data, size, offset, b2) || !NkReadU8(data, size, offset, b3)) {
        return false;
    }
    out = static_cast<nk_uint32>(b0) |
          (static_cast<nk_uint32>(b1) << 8u) |
          (static_cast<nk_uint32>(b2) << 16u) |
          (static_cast<nk_uint32>(b3) << 24u);
    return true;
}

} // namespace

nk_bool NkBinaryReader::ReadArchive(const nk_uint8* data,
                                    nk_size size,
                                    NkArchive& outArchive,
                                    NkString* outError) {
    outArchive.Clear();

    nk_size offset = 0;
    nk_uint32 magic = 0;
    nk_uint16 version = 0;
    nk_uint16 reserved = 0;
    nk_uint32 count = 0;

    if (!NkReadU32LE(data, size, offset, magic) ||
        !NkReadU16LE(data, size, offset, version) ||
        !NkReadU16LE(data, size, offset, reserved) ||
        !NkReadU32LE(data, size, offset, count)) {
        if (outError) {
            *outError = "Binary payload is too short";
        }
        return false;
    }

    (void)reserved;

    if (magic != NK_BINARY_MAGIC) {
        if (outError) {
            *outError = "Invalid binary serialization magic";
        }
        return false;
    }

    if (version != NK_BINARY_VERSION) {
        if (outError) {
            *outError = "Unsupported binary serialization version";
        }
        return false;
    }

    for (nk_uint32 i = 0; i < count; ++i) {
        nk_uint32 keyLen = 0;
        nk_uint8 typeRaw = 0;
        nk_uint32 valueLen = 0;

        if (!NkReadU32LE(data, size, offset, keyLen) ||
            !NkReadU8(data, size, offset, typeRaw) ||
            !NkReadU32LE(data, size, offset, valueLen)) {
            if (outError) {
                *outError = "Corrupted binary entry header";
            }
            return false;
        }

        if (offset + keyLen + valueLen > size) {
            if (outError) {
                *outError = "Corrupted binary entry payload";
            }
            return false;
        }

        NkString key(reinterpret_cast<const char*>(data + offset), keyLen);
        offset += keyLen;

        NkString valueText(reinterpret_cast<const char*>(data + offset), valueLen);
        offset += valueLen;

        NkArchiveValue value;
        value.type = static_cast<NkArchiveValueType>(typeRaw);
        value.text = valueText;
        if (value.type == NkArchiveValueType::NK_VALUE_NULL) {
            value.text.Clear();
        }

        if (!outArchive.SetValue(key.View(), value)) {
            if (outError) {
                *outError = "Failed to store binary entry";
            }
            return false;
        }
    }

    if (offset != size) {
        if (outError) {
            *outError = "Trailing bytes found after binary archive";
        }
        return false;
    }

    return true;
}

} // namespace entseu
} // namespace nkentseu
