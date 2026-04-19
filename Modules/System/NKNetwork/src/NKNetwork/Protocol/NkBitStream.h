#pragma once
// =============================================================================
// NKNetwork/Protocol/NkBitStream.h
// =============================================================================
// Sérialisation bit-à-bit pour les messages réseau.
//
// POURQUOI BIT-LEVEL ?
//   Un bool coûte 1 bit, pas 1 byte.
//   Un entier [0..3] coûte 2 bits, pas 4 bytes.
//   → Réduction significative de la bande passante.
//
// EXEMPLES :
//   PacketHeader (seqNum uint16 + ackMask uint32 + flags uint8) = 7 bytes
//   Position (float16 × 3) = 6 bytes au lieu de 12 bytes
//   Delta position (int8 × 3 si petit mouvement) = 3 bytes
//
// UTILISATION :
//   // Écriture
//   NkBitWriter w(buf, sizeof(buf));
//   w.WriteU32(seqNum);
//   w.WriteBool(isAlive);
//   w.WriteF32(posX, -1000.f, 1000.f, 0.01f);  // précision 1cm
//   w.WriteString(playerName);
//
//   // Lecture
//   NkBitReader r(buf, size);
//   uint32 seq = r.ReadU32();
//   bool alive = r.ReadBool();
//   float px = r.ReadF32(-1000.f, 1000.f, 0.01f);
//   NkString name; r.ReadString(name);
//
// SÉCURITÉ :
//   Les méthodes Read* retournent des valeurs par défaut si le buffer est épuisé.
//   Tester IsOverflowed() après une séquence de lectures pour détecter la corruption.
// =============================================================================

#include "Core/NkNetDefines.h"
#include "NKContainers/String/NkString.h"
#include "NKMath/NkMath.h"

namespace nkentseu {
    namespace net {

        using namespace math;

        // =====================================================================
        // NkBitWriter — écriture bit-à-bit dans un buffer
        // =====================================================================
        class NkBitWriter {
        public:
            NkBitWriter(uint8* buf, uint32 capacity) noexcept
                : mBuf(buf), mCap(capacity) {}

            // ── Types primitifs ───────────────────────────────────────────────
            void WriteBool (bool v)              noexcept { WriteBits(v ? 1u : 0u, 1); }
            void WriteU8   (uint8 v)             noexcept { WriteBits(v, 8); }
            void WriteU16  (uint16 v)            noexcept { WriteBits(v, 16); }
            void WriteU32  (uint32 v)            noexcept { WriteBits(v, 32); }
            void WriteU64  (uint64 v)            noexcept {
                WriteBits((uint32)(v & 0xFFFFFFFF), 32);
                WriteBits((uint32)(v >> 32),        32);
            }
            void WriteI8   (int8 v)              noexcept { WriteBits((uint32)(uint8)v, 8); }
            void WriteI16  (int16 v)             noexcept { WriteBits((uint32)(uint16)v, 16); }
            void WriteI32  (int32 v)             noexcept { WriteBits((uint32)v, 32); }
            void WriteF32  (float32 v)           noexcept {
                uint32 u; std::memcpy(&u, &v, 4); WriteBits(u, 32);
            }

            // ── Types quantifiés (précision variable = économie bande passante) ─
            /**
             * @brief Écrit un float32 quantifié sur N bits.
             * @param v     Valeur à écrire.
             * @param minV  Minimum de la plage.
             * @param maxV  Maximum de la plage.
             * @param prec  Précision (ex: 0.01f = 1cm).
             */
            void WriteF32Q(float32 v, float32 minV, float32 maxV,
                            float32 prec) noexcept {
                const float32 norm = (v - minV) / (maxV - minV);
                const uint32  steps = (uint32)((maxV - minV) / prec + 0.5f);
                const uint32  bits  = BitsRequired(steps);
                const uint32  quant = (uint32)(norm * steps + 0.5f);
                WriteBits(quant < steps ? quant : steps - 1, bits);
            }

            /**
             * @brief Écrit un entier borné [minV..maxV] (bits optimaux).
             */
            void WriteInt(int32 v, int32 minV, int32 maxV) noexcept {
                NK_NET_ASSERT(v >= minV && v <= maxV, "WriteInt out of range");
                const uint32 bits = BitsRequired((uint32)(maxV - minV));
                WriteBits((uint32)(v - minV), bits);
            }

            // ── Types composites ─────────────────────────────────────────────
            void WriteVec3f(const NkVec3f& v) noexcept {
                WriteF32(v.x); WriteF32(v.y); WriteF32(v.z);
            }
            void WriteVec3fQ(const NkVec3f& v,
                              float32 minV, float32 maxV, float32 prec) noexcept {
                WriteF32Q(v.x, minV, maxV, prec);
                WriteF32Q(v.y, minV, maxV, prec);
                WriteF32Q(v.z, minV, maxV, prec);
            }
            void WriteQuatf(const NkQuatf& q) noexcept {
                // Compression quaternion "smallest 3" :
                // Le composant le plus grand est omis (recalculé à la lecture)
                // → 2+10+10+10 = 32 bits au lieu de 128 bits (float32 × 4)
                uint8 largest = 0; float32 absMax = NkAbs(q.w);
                if (NkAbs(q.x) > absMax) { absMax = NkAbs(q.x); largest = 1; }
                if (NkAbs(q.y) > absMax) { absMax = NkAbs(q.y); largest = 2; }
                if (NkAbs(q.z) > absMax) { absMax = NkAbs(q.z); largest = 3; }

                const float32* c = &q.x;
                float32 a=0, b=0, d=0;
                float32 sign = c[largest] >= 0 ? 1.f : -1.f;
                int idx = 0;
                for (int i = 0; i < 4; ++i) {
                    if (i == largest) continue;
                    float32 val = c[i] * sign;
                    if (idx == 0) a = val;
                    else if (idx == 1) b = val;
                    else d = val;
                    ++idx;
                }
                WriteBits(largest, 2);
                WriteF32Q(a, -0.7072f, 0.7072f, 0.0001f);
                WriteF32Q(b, -0.7072f, 0.7072f, 0.0001f);
                WriteF32Q(d, -0.7072f, 0.7072f, 0.0001f);
            }

            void WriteString(const char* s, uint32 maxLen = 256) noexcept {
                if (!s) { WriteU16(0); return; }
                uint32 len = 0;
                while (s[len] && len < maxLen) ++len;
                WriteU16((uint16)len);
                for (uint32 i = 0; i < len; ++i) WriteU8((uint8)s[i]);
            }

            void WriteBytes(const uint8* data, uint32 size) noexcept {
                AlignToByte();
                if (BytesWritten() + size <= mCap) {
                    std::memcpy(mBuf + BytesWritten(), data, size);
                    mBitPos += size * 8;
                } else {
                    mOverflow = true;
                }
            }

            // ── Bits bruts ────────────────────────────────────────────────────
            void WriteBits(uint32 v, uint32 numBits) noexcept {
                if (mOverflow) return;
                if ((mBitPos + numBits + 7) / 8 > mCap) { mOverflow = true; return; }
                for (uint32 i = 0; i < numBits; ++i) {
                    const uint32 byteIdx = mBitPos / 8;
                    const uint32 bitIdx  = 7 - (mBitPos % 8);
                    if (v & (1u << (numBits - 1 - i)))
                        mBuf[byteIdx] |= (1u << bitIdx);
                    else
                        mBuf[byteIdx] &= ~(1u << bitIdx);
                    ++mBitPos;
                }
            }

            void AlignToByte() noexcept {
                const uint32 rem = mBitPos % 8;
                if (rem != 0) mBitPos += (8 - rem);
            }

            // ── Accès ─────────────────────────────────────────────────────────
            [[nodiscard]] uint32 BitsWritten()  const noexcept { return mBitPos; }
            [[nodiscard]] uint32 BytesWritten() const noexcept { return (mBitPos + 7) / 8; }
            [[nodiscard]] bool   IsOverflowed() const noexcept { return mOverflow; }

        private:
            [[nodiscard]] static uint32 BitsRequired(uint32 maxVal) noexcept {
                uint32 bits = 0;
                while ((1u << bits) <= maxVal) ++bits;
                return bits > 0 ? bits : 1;
            }

            uint8*  mBuf      = nullptr;
            uint32  mCap      = 0;
            uint32  mBitPos   = 0;
            bool    mOverflow = false;
        };

        // =====================================================================
        // NkBitReader — lecture bit-à-bit depuis un buffer
        // =====================================================================
        class NkBitReader {
        public:
            NkBitReader(const uint8* buf, uint32 size) noexcept
                : mBuf(buf), mSize(size * 8) {}

            // ── Types primitifs ───────────────────────────────────────────────
            [[nodiscard]] bool   ReadBool() noexcept { return ReadBits(1) != 0; }
            [[nodiscard]] uint8  ReadU8()   noexcept { return (uint8)ReadBits(8); }
            [[nodiscard]] uint16 ReadU16()  noexcept { return (uint16)ReadBits(16); }
            [[nodiscard]] uint32 ReadU32()  noexcept { return ReadBits(32); }
            [[nodiscard]] uint64 ReadU64()  noexcept {
                uint64 lo = ReadBits(32);
                uint64 hi = ReadBits(32);
                return lo | (hi << 32);
            }
            [[nodiscard]] int8   ReadI8()   noexcept { return (int8)(uint8)ReadBits(8); }
            [[nodiscard]] int16  ReadI16()  noexcept { return (int16)(uint16)ReadBits(16); }
            [[nodiscard]] int32  ReadI32()  noexcept { return (int32)ReadBits(32); }
            [[nodiscard]] float32 ReadF32() noexcept {
                uint32 u = ReadBits(32); float32 v; std::memcpy(&v, &u, 4); return v;
            }

            // ── Types quantifiés ─────────────────────────────────────────────
            [[nodiscard]] float32 ReadF32Q(float32 minV, float32 maxV,
                                            float32 prec) noexcept {
                const uint32 steps = (uint32)((maxV - minV) / prec + 0.5f);
                const uint32 bits  = BitsRequired(steps);
                const uint32 quant = ReadBits(bits);
                return minV + (quant / (float32)steps) * (maxV - minV);
            }

            [[nodiscard]] int32 ReadInt(int32 minV, int32 maxV) noexcept {
                const uint32 bits = BitsRequired((uint32)(maxV - minV));
                return minV + (int32)ReadBits(bits);
            }

            // ── Types composites ─────────────────────────────────────────────
            [[nodiscard]] NkVec3f ReadVec3f() noexcept {
                return { ReadF32(), ReadF32(), ReadF32() };
            }
            [[nodiscard]] NkVec3f ReadVec3fQ(float32 minV, float32 maxV,
                                               float32 prec) noexcept {
                return { ReadF32Q(minV,maxV,prec), ReadF32Q(minV,maxV,prec),
                          ReadF32Q(minV,maxV,prec) };
            }
            [[nodiscard]] NkQuatf ReadQuatf() noexcept {
                const uint8 largest = (uint8)ReadBits(2);
                const float32 a = ReadF32Q(-0.7072f, 0.7072f, 0.0001f);
                const float32 b = ReadF32Q(-0.7072f, 0.7072f, 0.0001f);
                const float32 d = ReadF32Q(-0.7072f, 0.7072f, 0.0001f);
                const float32 w = NkSqrt(NkMax(0.f, 1.f - a*a - b*b - d*d));
                float32 comps[4]; int idx = 0;
                for (int i = 0; i < 4; ++i) {
                    if (i == largest) { comps[i] = w; continue; }
                    if (idx == 0) comps[i] = a;
                    else if (idx == 1) comps[i] = b;
                    else comps[i] = d;
                    ++idx;
                }
                return NkQuatf(comps[0], comps[1], comps[2], comps[3]);
            }

            void ReadString(NkString& out, uint32 maxLen = 256) noexcept {
                uint16 len = ReadU16();
                if (len > maxLen) { mOverflow = true; return; }
                char buf[257] = {};
                for (uint16 i = 0; i < len && i < 256; ++i) buf[i] = (char)ReadU8();
                out = buf;
            }

            void ReadBytes(uint8* dst, uint32 size) noexcept {
                AlignToByte();
                const uint32 bytesRead = mBitPos / 8;
                if (bytesRead + size <= mSize / 8) {
                    std::memcpy(dst, mBuf + bytesRead, size);
                    mBitPos += size * 8;
                } else {
                    mOverflow = true;
                }
            }

            // ── Bits bruts ────────────────────────────────────────────────────
            [[nodiscard]] uint32 ReadBits(uint32 numBits) noexcept {
                if (mOverflow || mBitPos + numBits > mSize) { mOverflow = true; return 0; }
                uint32 result = 0;
                for (uint32 i = 0; i < numBits; ++i) {
                    const uint32 byteIdx = mBitPos / 8;
                    const uint32 bitIdx  = 7 - (mBitPos % 8);
                    if (mBuf[byteIdx] & (1u << bitIdx)) result |= (1u << (numBits - 1 - i));
                    ++mBitPos;
                }
                return result;
            }

            void AlignToByte() noexcept {
                const uint32 rem = mBitPos % 8;
                if (rem != 0) mBitPos += (8 - rem);
            }

            // ── Accès ─────────────────────────────────────────────────────────
            [[nodiscard]] uint32 BitsRead()    const noexcept { return mBitPos; }
            [[nodiscard]] uint32 BytesRead()   const noexcept { return (mBitPos + 7) / 8; }
            [[nodiscard]] uint32 BitsLeft()    const noexcept {
                return mBitPos <= mSize ? mSize - mBitPos : 0;
            }
            [[nodiscard]] bool   IsOverflowed()const noexcept { return mOverflow; }
            [[nodiscard]] bool   IsEmpty()     const noexcept { return BitsLeft() == 0; }

        private:
            [[nodiscard]] static uint32 BitsRequired(uint32 maxVal) noexcept {
                uint32 bits = 0;
                while ((1u << bits) <= maxVal) ++bits;
                return bits > 0 ? bits : 1;
            }

            const uint8* mBuf      = nullptr;
            uint32       mSize     = 0;    ///< Total en bits
            uint32       mBitPos   = 0;
            bool         mOverflow = false;
        };

    } // namespace net
} // namespace nkentseu
