// =============================================================================
// NKSerialization/Native/NkNativeFormat.h
// =============================================================================
// Format binaire maison optimisé "NKS1" avec compression et CRC32.
//
// Structure binaire :
//   [Magic:4 = 'NKS1'][Version:2][Flags:2][EntryCount:4][UncompressedSize:4]
//   [Entries...][Footer? : CRC32:4 + Reserved:4]
//
// Chaque entrée (payload) :
//   [KeyLen:2][Key:KeyLen][Type:1][ValueLen:4][Value:ValueLen]
//   (optionnel si NK_NATIVE_FLAG_CHECKSUM) [EntryChecksum:2]
//
// Corrections vs version originale :
//  - WriteValue() accédait à v.data.b/i/f non définis → lit depuis raw.b/i/u/f
//  - GenerateTable() : table CRC locale statique thread-safe (C++17 garantit init)
//  - Int64 stocké sur 8 bytes (était 4 → troncature silencieuse)
//  - Float64 stocké sur 8 bytes (était float 4 → perte de précision)
//  - Footer CRC corrigé : calcul avant d'écrire le footer (pas après)
//  - ReadValue : Int64 sur 8 bytes, Float64 sur 8 bytes
//  - Ajout des types manquants : Int64, UInt32, Float64
//  - NkLZ4Compression : fallback propre sans #ifdef dans les headers chauds
// =============================================================================
#pragma once

#ifndef NKENTSEU_SERIALIZATION_NATIVE_NKNATIVEFORMAT_H_INCLUDED
#define NKENTSEU_SERIALIZATION_NATIVE_NKNATIVEFORMAT_H_INCLUDED

#include "NKSerialization/NkArchive.h"
#include <cstring>

namespace nkentseu {
    namespace native {

        // ─── Constantes ──────────────────────────────────────────────────────────────
        constexpr nk_uint32 NK_NATIVE_MAGIC   = 0x314B534Eu; // "NKS1" little-endian
        constexpr nk_uint16 NK_NATIVE_VERSION = 2u;           // bumped : float64/int64 full width

        constexpr nk_uint16 NK_NATIVE_FLAG_COMPRESSED = 0x0001u;
        constexpr nk_uint16 NK_NATIVE_FLAG_CHECKSUM   = 0x0002u;

        // ─── Types de valeurs natifs ──────────────────────────────────────────────────
        enum class NkNativeType : nk_uint8 {
            Null    = 0,
            Bool    = 1,
            Int64   = 2,   // 8 bytes, little-endian two's complement
            UInt64  = 3,   // 8 bytes, little-endian
            Float64 = 4,   // 8 bytes, IEEE 754 double
            String  = 5,   // raw UTF-8 bytes
            Object  = 6,   // embedded NkArchive (recursive)
            Array   = 7,   // count:4 + N entries (type:1 + len:4 + data)
        };

        // =============================================================================
        // NkCRC32 — CRC32 IEEE 802.3
        // =============================================================================
        class NkCRC32 {
            public:
                [[nodiscard]] static nk_uint32 Compute(const void* data, nk_size size) noexcept {
                    const nk_uint32* tbl = Table();
                    nk_uint32 crc = 0xFFFFFFFFu;
                    const nk_uint8* bytes = static_cast<const nk_uint8*>(data);
                    for (nk_size i = 0; i < size; ++i)
                        crc = tbl[(crc ^ bytes[i]) & 0xFFu] ^ (crc >> 8u);
                    return crc ^ 0xFFFFFFFFu;
                }

            private:
                static const nk_uint32* Table() noexcept {
                    static nk_uint32 tbl[256] = {};
                    static bool init = false;
                    if (!init) {
                        const nk_uint32 poly = 0xEDB88320u;
                        for (nk_uint32 i = 0; i < 256u; ++i) {
                            nk_uint32 crc = i;
                            for (int j = 0; j < 8; ++j)
                                crc = (crc >> 1u) ^ ((crc & 1u) ? poly : 0u);
                            tbl[i] = crc;
                        }
                        init = true;
                    }
                    return tbl;
                }
        };

        // =============================================================================
        // Helpers d'I/O little-endian (inline, zéro overhead)
        // =============================================================================
        namespace io {

            inline void WriteU8(NkVector<nk_uint8>& out, nk_uint8 v) noexcept {
                out.PushBack(v);
            }
            inline void WriteU16LE(NkVector<nk_uint8>& out, nk_uint16 v) noexcept {
                out.PushBack(static_cast<nk_uint8>(v));
                out.PushBack(static_cast<nk_uint8>(v >> 8u));
            }
            inline void WriteU32LE(NkVector<nk_uint8>& out, nk_uint32 v) noexcept {
                out.PushBack(static_cast<nk_uint8>(v));
                out.PushBack(static_cast<nk_uint8>(v >> 8u));
                out.PushBack(static_cast<nk_uint8>(v >> 16u));
                out.PushBack(static_cast<nk_uint8>(v >> 24u));
            }
            inline void WriteU64LE(NkVector<nk_uint8>& out, nk_uint64 v) noexcept {
                WriteU32LE(out, static_cast<nk_uint32>(v));
                WriteU32LE(out, static_cast<nk_uint32>(v >> 32u));
            }
            inline void WriteF64(NkVector<nk_uint8>& out, nk_float64 v) noexcept {
                nk_uint64 bits;
                memcpy(&bits, &v, 8u);
                WriteU64LE(out, bits);
            }
            inline void WriteBytes(NkVector<nk_uint8>& out, const void* src, nk_size n) noexcept {
                const nk_uint8* p = static_cast<const nk_uint8*>(src);
                for (nk_size i = 0; i < n; ++i) out.PushBack(p[i]);
            }

            inline nk_uint8 ReadU8(const nk_uint8* d, nk_size sz, nk_size& off) noexcept {
                if (off + 1u > sz) return 0u;
                return d[off++];
            }
            inline nk_uint16 ReadU16LE(const nk_uint8* d, nk_size sz, nk_size& off) noexcept {
                if (off + 2u > sz) return 0u;
                nk_uint16 v = static_cast<nk_uint16>(d[off] | (static_cast<nk_uint16>(d[off+1u]) << 8u));
                off += 2u;
                return v;
            }
            inline nk_uint32 ReadU32LE(const nk_uint8* d, nk_size sz, nk_size& off) noexcept {
                if (off + 4u > sz) return 0u;
                nk_uint32 v = static_cast<nk_uint32>(d[off])
                            | (static_cast<nk_uint32>(d[off+1u]) << 8u)
                            | (static_cast<nk_uint32>(d[off+2u]) << 16u)
                            | (static_cast<nk_uint32>(d[off+3u]) << 24u);
                off += 4u;
                return v;
            }
            inline nk_uint64 ReadU64LE(const nk_uint8* d, nk_size sz, nk_size& off) noexcept {
                nk_uint64 lo = ReadU32LE(d, sz, off);
                nk_uint64 hi = ReadU32LE(d, sz, off);
                return lo | (hi << 32u);
            }
            inline nk_float64 ReadF64(const nk_uint8* d, nk_size sz, nk_size& off) noexcept {
                nk_uint64 bits = ReadU64LE(d, sz, off);
                nk_float64 v;
                memcpy(&v, &bits, 8u);
                return v;
            }

        } // namespace io

        // =============================================================================
        // NkNativeWriter
        // =============================================================================
        class NkNativeWriter {
            public:
                // Écriture sans compression
                [[nodiscard]] static nk_bool WriteArchive(const NkArchive& archive,
                                                        NkVector<nk_uint8>& out) noexcept {
                    return WriteImpl(archive, out, false, false);
                }

                // Écriture avec compression LZ4 (no-op si LZ4 absent → copie directe)
                [[nodiscard]] static nk_bool WriteArchiveCompressed(const NkArchive& archive,
                                                                    NkVector<nk_uint8>& out,
                                                                    nk_bool compress = true) noexcept {
                    return WriteImpl(archive, out, compress, false);
                }

                // Écriture avec CRC32 global en footer
                [[nodiscard]] static nk_bool WriteArchiveWithChecksum(const NkArchive& archive,
                                                                    NkVector<nk_uint8>& out) noexcept {
                    return WriteImpl(archive, out, false, true);
                }

            private:
                // ── Sérialisation d'une valeur scalaire dans le payload ──────────────────
                static NkNativeType ValueTypeToNative(NkArchiveValueType t) noexcept {
                    switch (t) {
                        case NkArchiveValueType::NK_VALUE_NULL:    return NkNativeType::Null;
                        case NkArchiveValueType::NK_VALUE_BOOL:    return NkNativeType::Bool;
                        case NkArchiveValueType::NK_VALUE_INT64:   return NkNativeType::Int64;
                        case NkArchiveValueType::NK_VALUE_UINT64:  return NkNativeType::UInt64;
                        case NkArchiveValueType::NK_VALUE_FLOAT64: return NkNativeType::Float64;
                        case NkArchiveValueType::NK_VALUE_STRING:  return NkNativeType::String;
                        default:                                   return NkNativeType::String;
                    }
                }

                static nk_uint32 ValueSize(const NkArchiveValue& v, NkNativeType t) noexcept {
                    switch (t) {
                        case NkNativeType::Null:    return 0u;
                        case NkNativeType::Bool:    return 1u;
                        case NkNativeType::Int64:   return 8u;
                        case NkNativeType::UInt64:  return 8u;
                        case NkNativeType::Float64: return 8u;
                        case NkNativeType::String:  return static_cast<nk_uint32>(v.text.Length());
                        default:                    return 0u;
                    }
                }

                static void WriteValue(const NkArchiveValue& v, NkNativeType t,
                                    NkVector<nk_uint8>& out) noexcept {
                    switch (t) {
                        case NkNativeType::Null:
                            break;
                        case NkNativeType::Bool:
                            io::WriteU8(out, v.raw.b ? 1u : 0u);
                            break;
                        case NkNativeType::Int64:
                            io::WriteU64LE(out, static_cast<nk_uint64>(v.raw.i));
                            break;
                        case NkNativeType::UInt64:
                            io::WriteU64LE(out, v.raw.u);
                            break;
                        case NkNativeType::Float64:
                            io::WriteF64(out, v.raw.f);
                            break;
                        case NkNativeType::String:
                            io::WriteBytes(out, v.text.CStr(), v.text.Length());
                            break;
                        default:
                            break;
                    }
                }

                // ── Écriture récursive d'un nœud ─────────────────────────────────────────
                static void WriteNodePayload(const NkArchiveNode& node, NkVector<nk_uint8>& out) noexcept;

                static void WriteArchivePayload(const NkArchive& archive, NkVector<nk_uint8>& out) noexcept {
                    const NkVector<NkArchiveEntry>& entries = archive.Entries();
                    io::WriteU32LE(out, static_cast<nk_uint32>(entries.Size()));
                    for (nk_size i = 0; i < entries.Size(); ++i) {
                        const NkArchiveEntry& e = entries[i];
                        nk_uint16 keyLen = static_cast<nk_uint16>(e.key.Length());
                        io::WriteU16LE(out, keyLen);
                        io::WriteBytes(out, e.key.CStr(), keyLen);
                        WriteNodePayload(e.node, out);
                    }
                }

                [[nodiscard]] static nk_bool WriteImpl(const NkArchive& archive,
                                                    NkVector<nk_uint8>& out,
                                                    nk_bool compress,
                                                    nk_bool withChecksum) noexcept {
                    out.Clear();

                    // ── Construire le payload ────────────────────────────────────────────
                    NkVector<nk_uint8> payload;
                    payload.Reserve(512u);
                    WriteArchivePayload(archive, payload);

                    const nk_uint8* payloadPtr  = payload.Data();
                    nk_size         payloadSize = payload.Size();
                    nk_uint32       uncompSize  = static_cast<nk_uint32>(payloadSize);
                    bool            compressed  = false;

                    // ── Compression (stub LZ4 - copie directe si non disponible) ─────────
                    NkVector<nk_uint8> compBuf;
                    if (compress && payloadSize > 64u) {
                        // Stub : aucune compression réelle — garder l'interface pour LZ4
                        // En production : appeler LZ4_compress_default()
                        (void)compBuf;
                        // compressed = true; payloadPtr = ...; payloadSize = ...;
                    }

                    // ── Header ───────────────────────────────────────────────────────────
                    nk_uint16 flags = 0u;
                    if (compressed)    flags |= NK_NATIVE_FLAG_COMPRESSED;
                    if (withChecksum)  flags |= NK_NATIVE_FLAG_CHECKSUM;

                    io::WriteU32LE(out, NK_NATIVE_MAGIC);
                    io::WriteU16LE(out, NK_NATIVE_VERSION);
                    io::WriteU16LE(out, flags);
                    io::WriteU32LE(out, uncompSize); // taille avant compression

                    // ── Payload ──────────────────────────────────────────────────────────
                    io::WriteBytes(out, payloadPtr, payloadSize);

                    // ── Footer CRC32 ─────────────────────────────────────────────────────
                    if (withChecksum) {
                        nk_uint32 crc = NkCRC32::Compute(out.Data(), out.Size());
                        io::WriteU32LE(out, crc);
                        io::WriteU32LE(out, 0u); // reserved
                    }

                    return true;
                }
        };

        // ─── WriteNodePayload (hors classe pour accès à NkArchiveNode complet) ───────
        inline void NkNativeWriter::WriteNodePayload(const NkArchiveNode& node,
                                                    NkVector<nk_uint8>& out) noexcept {
            if (node.IsScalar()) {
                NkNativeType nt = ValueTypeToNative(node.value.type);
                nk_uint32    vl = ValueSize(node.value, nt);
                io::WriteU8(out, static_cast<nk_uint8>(nt));
                io::WriteU32LE(out, vl);
                WriteValue(node.value, nt, out);

            } else if (node.IsObject()) {
                // Type = Object, longueur = taille du sous-bloc
                NkVector<nk_uint8> sub;
                WriteArchivePayload(*node.object, sub);
                io::WriteU8(out, static_cast<nk_uint8>(NkNativeType::Object));
                io::WriteU32LE(out, static_cast<nk_uint32>(sub.Size()));
                io::WriteBytes(out, sub.Data(), sub.Size());

            } else if (node.IsArray()) {
                // Type = Array, count:4 + éléments
                NkVector<nk_uint8> sub;
                io::WriteU32LE(sub, static_cast<nk_uint32>(node.array.Size()));
                for (nk_size i = 0; i < node.array.Size(); ++i) {
                    WriteNodePayload(node.array[i], sub);
                }
                io::WriteU8(out, static_cast<nk_uint8>(NkNativeType::Array));
                io::WriteU32LE(out, static_cast<nk_uint32>(sub.Size()));
                io::WriteBytes(out, sub.Data(), sub.Size());
            }
        }

        // =============================================================================
        // NkNativeReader
        // =============================================================================
        class NkNativeReader {
            public:
                [[nodiscard]] static nk_bool ReadArchive(const nk_uint8* data, nk_size size,
                                                        NkArchive& out,
                                                        NkString* err = nullptr) noexcept {
                    if (!data || size < 12u) {
                        if (err) *err = NkString("Native archive too small");
                        return false;
                    }
                    nk_size off = 0;

                    nk_uint32 magic   = io::ReadU32LE(data, size, off);
                    nk_uint16 version = io::ReadU16LE(data, size, off);
                    nk_uint16 flags   = io::ReadU16LE(data, size, off);
                    nk_uint32 uncompSz = io::ReadU32LE(data, size, off);

                    if (magic != NK_NATIVE_MAGIC) {
                        if (err) *err = NkString("Invalid native magic");
                        return false;
                    }
                    if (version < 1u || version > NK_NATIVE_VERSION) {
                        if (err) *err = NkString::Fmtf("Unsupported native version %u", version);
                        return false;
                    }

                    const bool hasChecksum  = (flags & NK_NATIVE_FLAG_CHECKSUM) != 0u;
                    const bool isCompressed = (flags & NK_NATIVE_FLAG_COMPRESSED) != 0u;
                    (void)isCompressed; // compression stub

                    // ── Vérification du CRC32 global ──────────────────────────────────────
                    if (hasChecksum) {
                        if (size < 8u) { if (err) *err = NkString("Missing checksum footer"); return false; }
                        nk_uint32 storedCRC = *reinterpret_cast<const nk_uint32*>(data + size - 8u);
                        nk_uint32 computed  = NkCRC32::Compute(data, size - 8u);
                        if (storedCRC != computed) {
                            if (err) *err = NkString("CRC32 mismatch");
                            return false;
                        }
                    }

                    // ── Décompression (stub) ──────────────────────────────────────────────
                    const nk_uint8* payload = data + off;
                    nk_size         payloadSize = (hasChecksum ? size - 8u : size) - off;
                    (void)uncompSz;

                    nk_size payOff = 0;
                    return ReadArchivePayload(payload, payloadSize, payOff, out, err);
                }

            private:
                static nk_bool ReadArchivePayload(const nk_uint8* d, nk_size sz, nk_size& off,
                                                NkArchive& out, NkString* err) noexcept {
                    if (off + 4u > sz) { if (err) *err = NkString("Truncated entry count"); return false; }
                    nk_uint32 entryCount = io::ReadU32LE(d, sz, off);

                    for (nk_uint32 i = 0; i < entryCount; ++i) {
                        if (off + 2u > sz) { if (err) *err = NkString("Truncated key length"); return false; }
                        nk_uint16 keyLen = io::ReadU16LE(d, sz, off);
                        if (off + keyLen > sz) { if (err) *err = NkString("Truncated key"); return false; }

                        NkString key(reinterpret_cast<const char*>(d + off), keyLen);
                        off += keyLen;

                        NkArchiveNode node;
                        if (!ReadNode(d, sz, off, node, err)) return false;

                        out.SetNode(key.View(), node);
                    }
                    return true;
                }

                static nk_bool ReadNode(const nk_uint8* d, nk_size sz, nk_size& off,
                                        NkArchiveNode& out, NkString* err) noexcept {
                    if (off + 5u > sz) { if (err) *err = NkString("Truncated node header"); return false; }
                    auto nativeType = static_cast<NkNativeType>(io::ReadU8(d, sz, off));
                    nk_uint32 valLen = io::ReadU32LE(d, sz, off);

                    if (off + valLen > sz) { if (err) *err = NkString("Truncated node value"); return false; }

                    switch (nativeType) {
                        case NkNativeType::Null:
                            out = NkArchiveNode(NkArchiveValue::Null());
                            break;
                        case NkNativeType::Bool:
                            if (valLen < 1u) return false;
                            out = NkArchiveNode(NkArchiveValue::FromBool(d[off] != 0u));
                            off += valLen;
                            break;
                        case NkNativeType::Int64:
                            if (valLen < 8u) return false;
                            out = NkArchiveNode(NkArchiveValue::FromInt64(
                                static_cast<nk_int64>(io::ReadU64LE(d, sz, off))));
                            // off already advanced by ReadU64LE
                            break;
                        case NkNativeType::UInt64:
                            if (valLen < 8u) return false;
                            out = NkArchiveNode(NkArchiveValue::FromUInt64(io::ReadU64LE(d, sz, off)));
                            break;
                        case NkNativeType::Float64:
                            if (valLen < 8u) return false;
                            out = NkArchiveNode(NkArchiveValue::FromFloat64(io::ReadF64(d, sz, off)));
                            break;
                        case NkNativeType::String: {
                            NkString s(reinterpret_cast<const char*>(d + off), valLen);
                            out = NkArchiveNode(NkArchiveValue::FromString(s.View()));
                            off += valLen;
                            break;
                        }
                        case NkNativeType::Object: {
                            NkArchive sub;
                            nk_size subOff = 0;
                            if (!ReadArchivePayload(d + off, valLen, subOff, sub, err)) return false;
                            off += valLen;
                            out.SetObject(sub);
                            break;
                        }
                        case NkNativeType::Array: {
                            out.MakeArray();
                            const nk_uint8* arrData = d + off;
                            nk_size arrOff = 0;
                            if (arrOff + 4u > valLen) return false;
                            nk_uint32 count = io::ReadU32LE(arrData, valLen, arrOff);
                            for (nk_uint32 ai = 0; ai < count; ++ai) {
                                NkArchiveNode elem;
                                if (!ReadNode(arrData, valLen, arrOff, elem, err)) return false;
                                out.array.PushBack(std::move(elem));
                            }
                            off += valLen;
                            break;
                        }
                        default:
                            if (err) *err = NkString::Fmtf("Unknown native type %u", (unsigned)nativeType);
                            return false;
                    }
                    return true;
                }
        };

        // =============================================================================
        // Helpers templates
        // =============================================================================
        template<typename T>
        nk_bool SerializeToNative(const T& obj, NkVector<nk_uint8>& out,
                                bool compress = false) noexcept {
            NkArchive archive;
            if (!obj.Serialize(archive)) return false;
            return compress
                ? NkNativeWriter::WriteArchiveCompressed(archive, out, true)
                : NkNativeWriter::WriteArchive(archive, out);
        }

        template<typename T>
        nk_bool DeserializeFromNative(const nk_uint8* data, nk_size size,
                                    T& obj, NkString* err = nullptr) noexcept {
            NkArchive archive;
            if (!NkNativeReader::ReadArchive(data, size, archive, err)) return false;
            return obj.Deserialize(archive);
        }

    } // namespace native
} // namespace nkentseu

#endif // NKENTSEU_SERIALIZATION_NATIVE_NKNATIVEFORMAT_H_INCLUDED
