// =============================================================================
// NKNetwork/Protocol/NkBitStream.cpp
// =============================================================================
// DESCRIPTION :
//   Implémentation des méthodes de NkBitWriter et NkBitReader qui ne sont
//   pas définies inline dans le header pour raisons de compilation ou de taille.
//
// NOTES D'IMPLÉMENTATION :
//   • La majorité des méthodes restent inline dans le header pour performance
//   • Ce fichier contient uniquement les méthodes trop complexes pour l'inlining
//   • Toutes les méthodes sont noexcept — gestion d'erreur via flags internes
//
// DÉPENDANCES INTERNES :
//   • NkBitStream.h : Déclarations des classes implémentées
//   • NkNetDefines.h : Types fondamentaux (uint8, float32, etc.)
//
// AUTEUR   : Rihen
// DATE     : 2026-02-10
// VERSION  : 1.0.0
// LICENCE  : Propriétaire - libre d'utilisation et de modification
// =============================================================================

// -------------------------------------------------------------------------
// SECTION 1 : INCLUSIONS (ordre strict requis)
// -------------------------------------------------------------------------

#include "pch.h"
#include "NKNetwork/Protocol/NkBitStream.h"

// En-têtes standards pour opérations mémoire
#include <cstring>
#include <cmath>

// -------------------------------------------------------------------------
// SECTION 2 : NAMESPACE PRINCIPAL
// -------------------------------------------------------------------------

namespace nkentseu {

    namespace net {

        // =====================================================================
        // IMPLÉMENTATION : NkBitWriter — Constructeur
        // =====================================================================

        NkBitWriter::NkBitWriter(uint8* buf, uint32 capacity) noexcept
            : mBuf(buf)
            , mCap(capacity)
            , mBitPos(0)
            , mOverflow(false)
        {
            // Initialisation défensive : buffer null ou capacité nulle → overflow immédiat
            if (mBuf == nullptr || mCap == 0)
            {
                mOverflow = true;
            }
        }

        // =====================================================================
        // IMPLÉMENTATION : NkBitWriter — Types primitifs
        // =====================================================================

        void NkBitWriter::WriteBool(bool v) noexcept
        {
            WriteBits(v ? 1u : 0u, 1);
        }

        void NkBitWriter::WriteU8(uint8 v) noexcept
        {
            WriteBits(v, 8);
        }

        void NkBitWriter::WriteU16(uint16 v) noexcept
        {
            WriteBits(v, 16);
        }

        void NkBitWriter::WriteU32(uint32 v) noexcept
        {
            WriteBits(v, 32);
        }

        void NkBitWriter::WriteU64(uint64 v) noexcept
        {
            // Écriture little-endian : low 32 bits d'abord
            WriteBits(static_cast<uint32>(v & 0xFFFFFFFFu), 32);
            WriteBits(static_cast<uint32>(v >> 32), 32);
        }

        void NkBitWriter::WriteI8(int8 v) noexcept
        {
            // Cast vers uint8 pour écriture bit-à-bit (représentation two's complement)
            WriteBits(static_cast<uint32>(static_cast<uint8>(v)), 8);
        }

        void NkBitWriter::WriteI16(int16 v) noexcept
        {
            WriteBits(static_cast<uint32>(static_cast<uint16>(v)), 16);
        }

        void NkBitWriter::WriteI32(int32 v) noexcept
        {
            // Cast direct : la représentation binaire est préservée
            WriteBits(static_cast<uint32>(v), 32);
        }

        void NkBitWriter::WriteF32(float32 v) noexcept
        {
            // Copie de la représentation IEEE 754 sans conversion
            uint32 u = 0;
            std::memcpy(&u, &v, sizeof(float32));
            WriteBits(u, 32);
        }

        // =====================================================================
        // IMPLÉMENTATION : NkBitWriter — Types quantifiés
        // =====================================================================

        void NkBitWriter::WriteF32Q(float32 v, float32 minV, float32 maxV, float32 prec) noexcept
        {
            // Clamp de la valeur dans la plage pour éviter les dépassements
            if (v < minV) { v = minV; }
            if (v > maxV) { v = maxV; }

            // Calcul du nombre de pas de quantification
            const float32 range = maxV - minV;
            if (range <= 0.f || prec <= 0.f)
            {
                // Paramètres invalides : écrire 0 sur 1 bit
                WriteBits(0u, 1);
                return;
            }

            const uint32 steps = static_cast<uint32>((range / prec) + 0.5f);
            const uint32 bits = BitsRequired(steps);

            // Normalisation et quantification
            const float32 norm = (v - minV) / range;
            uint32 quant = static_cast<uint32>(norm * static_cast<float32>(steps) + 0.5f);

            // Clamp final pour sécurité
            if (quant >= steps) { quant = steps - 1; }

            WriteBits(quant, bits);
        }

        void NkBitWriter::WriteInt(int32 v, int32 minV, int32 maxV) noexcept
        {
            // Validation en debug (désactivée en release pour performance)
            NK_NET_ASSERT(v >= minV && v <= maxV, "WriteInt: valeur hors plage");

            // Clamp en release pour robustesse
            if (v < minV) { v = minV; }
            if (v > maxV) { v = maxV; }

            const uint32 range = static_cast<uint32>(maxV - minV);
            const uint32 bits = BitsRequired(range);
            const uint32 offset = static_cast<uint32>(v - minV);

            WriteBits(offset, bits);
        }

        // =====================================================================
        // IMPLÉMENTATION : NkBitWriter — Types composites
        // =====================================================================

        void NkBitWriter::WriteVec3f(const NkVec3f& v) noexcept
        {
            WriteF32(v.x);
            WriteF32(v.y);
            WriteF32(v.z);
        }

        void NkBitWriter::WriteVec3fQ(const NkVec3f& v, float32 minV, float32 maxV, float32 prec) noexcept
        {
            WriteF32Q(v.x, minV, maxV, prec);
            WriteF32Q(v.y, minV, maxV, prec);
            WriteF32Q(v.z, minV, maxV, prec);
        }

        void NkBitWriter::WriteQuatf(const NkQuatf& q) noexcept
        {
            // Algorithme "smallest 3 components" pour compression quaternion
            // Référence : "Quaternion Compression for Networked Games" — GDC 2014

            // Étape 1 : Trouver la composante de plus grande valeur absolue.
            // NkFabs utilisé explicitement pour float32 — NkAbs est réservé aux entiers.
            uint8 largest = 0;
            float32 absMax = (q.w < 0.0f ? -q.w : q.w);

            if ((q.x < 0.0f ? -q.x : q.x) > absMax) { absMax = (q.x < 0.0f ? -q.x : q.x); largest = 1; }
            if ((q.y < 0.0f ? -q.y : q.y) > absMax) { absMax = (q.y < 0.0f ? -q.y : q.y); largest = 2; }
            if ((q.z < 0.0f ? -q.z : q.z) > absMax) { largest = 3; }

            // Étape 2 : Extraire les 3 composantes restantes avec signe ajusté
            const float32* comps = &q.x;  // {x, y, z, w} en mémoire
            float32 a = 0.f;
            float32 b = 0.f;
            float32 d = 0.f;

            // Signe basé sur la composante la plus grande pour reconstruction cohérente
            const float32 sign = (comps[largest] >= 0.f) ? 1.f : -1.f;

            int idx = 0;
            for (int i = 0; i < 4; ++i)
            {
                if (i == static_cast<int>(largest)) { continue; }

                const float32 val = comps[i] * sign;
                if (idx == 0) { a = val; }
                else if (idx == 1) { b = val; }
                else { d = val; }
                ++idx;
            }

            // Étape 3 : Encoder l'index et les 3 composantes quantifiées
            // Plage [-0.7072, +0.7072] car pour un quaternion unitaire,
            // si une composante est la plus grande, les autres sont ≤ 1/√2 ≈ 0.7071
            WriteBits(largest, 2);
            WriteF32Q(a, -0.7072f, 0.7072f, 0.0001f);
            WriteF32Q(b, -0.7072f, 0.7072f, 0.0001f);
            WriteF32Q(d, -0.7072f, 0.7072f, 0.0001f);
        }

        void NkBitWriter::WriteString(const char* s, uint32 maxLen) noexcept
        {
            // Gestion du cas nullptr
            if (s == nullptr)
            {
                WriteU16(0);
                return;
            }

            // Calcul de la longueur effective (sans dépasser maxLen)
            uint32 len = 0;
            while (s[len] != '\0' && len < maxLen)
            {
                ++len;
            }

            // Écriture de la longueur puis des caractères
            WriteU16(static_cast<uint16>(len));
            for (uint32 i = 0; i < len; ++i)
            {
                WriteU8(static_cast<uint8>(s[i]));
            }
        }

        void NkBitWriter::WriteBytes(const uint8* data, uint32 size) noexcept
        {
            // Alignement sur byte requis pour accès mémoire direct
            AlignToByte();

            // Vérification de capacité
            const uint32 currentBytes = BytesWritten();
            if (currentBytes + size <= mCap)
            {
                std::memcpy(mBuf + currentBytes, data, size);
                mBitPos += size * 8;
            }
            else
            {
                mOverflow = true;
            }
        }

        // =====================================================================
        // IMPLÉMENTATION : NkBitWriter — Bits bruts et utilitaires
        // =====================================================================

        void NkBitWriter::WriteBits(uint32 v, uint32 numBits) noexcept
        {
            // Early-out si déjà en overflow
            if (mOverflow) { return; }

            // Vérification de capacité : (bitPos + numBits + 7) / 8 donne les bytes nécessaires
            if ((mBitPos + numBits + 7u) / 8u > mCap)
            {
                mOverflow = true;
                return;
            }

            // Écriture bit-à-bit : bit de poids fort de v en premier
            for (uint32 i = 0; i < numBits; ++i)
            {
                const uint32 byteIdx = mBitPos / 8u;
                const uint32 bitIdx = 7u - (mBitPos % 8u);  // MSB en premier dans le byte

                // Extraction du bit courant de v (MSB d'abord)
                const uint32 bitMask = 1u << (numBits - 1u - i);
                if ((v & bitMask) != 0u)
                {
                    mBuf[byteIdx] |= (1u << bitIdx);  // Set bit
                }
                else
                {
                    mBuf[byteIdx] &= ~(1u << bitIdx);  // Clear bit
                }

                ++mBitPos;
            }
        }

        void NkBitWriter::AlignToByte() noexcept
        {
            const uint32 rem = mBitPos % 8u;
            if (rem != 0u)
            {
                mBitPos += (8u - rem);  // Sauter aux bits de bourrage
            }
        }

        uint32 NkBitWriter::BitsWritten() const noexcept
        {
            return mBitPos;
        }

        uint32 NkBitWriter::BytesWritten() const noexcept
        {
            return (mBitPos + 7u) / 8u;
        }

        bool NkBitWriter::IsOverflowed() const noexcept
        {
            return mOverflow;
        }

        uint32 NkBitWriter::BitsRequired(uint32 maxVal) noexcept
        {
            // Cas particulier : maxVal = 0 → 1 bit suffisant pour encoder 0
            if (maxVal == 0u) { return 1u; }

            // Comptage du nombre de bits : position du bit de poids fort + 1
            uint32 bits = 0u;
            uint32 val = maxVal;
            while (val > 0u)
            {
                ++bits;
                val >>= 1;
            }
            return bits;
        }

        // =====================================================================
        // IMPLÉMENTATION : NkBitReader — Constructeur
        // =====================================================================

        NkBitReader::NkBitReader(const uint8* buf, uint32 size) noexcept
            : mBuf(buf)
            , mSize(size * 8u)  // Conversion bytes → bits
            , mBitPos(0)
            , mOverflow(false)
        {
            // Initialisation défensive
            if (mBuf == nullptr || mSize == 0u)
            {
                mOverflow = true;
            }
        }

        // =====================================================================
        // IMPLÉMENTATION : NkBitReader — Types primitifs
        // =====================================================================

        bool NkBitReader::ReadBool() noexcept
        {
            return ReadBits(1u) != 0u;
        }

        uint8 NkBitReader::ReadU8() noexcept
        {
            return static_cast<uint8>(ReadBits(8u));
        }

        uint16 NkBitReader::ReadU16() noexcept
        {
            return static_cast<uint16>(ReadBits(16u));
        }

        uint32 NkBitReader::ReadU32() noexcept
        {
            return ReadBits(32u);
        }

        uint64 NkBitReader::ReadU64() noexcept
        {
            const uint64 lo = ReadBits(32u);
            const uint64 hi = ReadBits(32u);
            return lo | (hi << 32u);
        }

        int8 NkBitReader::ReadI8() noexcept
        {
            return static_cast<int8>(static_cast<uint8>(ReadBits(8u)));
        }

        int16 NkBitReader::ReadI16() noexcept
        {
            return static_cast<int16>(static_cast<uint16>(ReadBits(16u)));
        }

        int32 NkBitReader::ReadI32() noexcept
        {
            return static_cast<int32>(ReadBits(32u));
        }

        float32 NkBitReader::ReadF32() noexcept
        {
            const uint32 u = ReadBits(32u);
            float32 v = 0.f;
            std::memcpy(&v, &u, sizeof(float32));
            return v;
        }

        // =====================================================================
        // IMPLÉMENTATION : NkBitReader — Types quantifiés
        // =====================================================================

        float32 NkBitReader::ReadF32Q(float32 minV, float32 maxV, float32 prec) noexcept
        {
            const float32 range = maxV - minV;
            if (range <= 0.f || prec <= 0.f)
            {
                return minV;  // Paramètres invalides → retour valeur minimale
            }

            const uint32 steps = static_cast<uint32>((range / prec) + 0.5f);
            const uint32 bits = BitsRequired(steps);
            const uint32 quant = ReadBits(bits);

            // Déquantification : interpolation linéaire
            return minV + (static_cast<float32>(quant) / static_cast<float32>(steps)) * range;
        }

        int32 NkBitReader::ReadInt(int32 minV, int32 maxV) noexcept
        {
            const uint32 range = static_cast<uint32>(maxV - minV);
            const uint32 bits = BitsRequired(range);
            const uint32 offset = ReadBits(bits);
            return minV + static_cast<int32>(offset);
        }

        // =====================================================================
        // IMPLÉMENTATION : NkBitReader — Types composites
        // =====================================================================

        NkVec3f NkBitReader::ReadVec3f() noexcept
        {
            return NkVec3f{ ReadF32(), ReadF32(), ReadF32() };
        }

        NkVec3f NkBitReader::ReadVec3fQ(float32 minV, float32 maxV, float32 prec) noexcept
        {
            return NkVec3f{
                ReadF32Q(minV, maxV, prec),
                ReadF32Q(minV, maxV, prec),
                ReadF32Q(minV, maxV, prec)
            };
        }

        NkQuatf NkBitReader::ReadQuatf() noexcept
        {
            // Lecture de l'index de la composante manquante
            const uint8 largest = static_cast<uint8>(ReadBits(2u));

            // Lecture des 3 composantes quantifiées
            const float32 a = ReadF32Q(-0.7072f, 0.7072f, 0.0001f);
            const float32 b = ReadF32Q(-0.7072f, 0.7072f, 0.0001f);
            const float32 d = ReadF32Q(-0.7072f, 0.7072f, 0.0001f);

            // Reconstruction de la composante manquante via norme unitaire
            // w = ±sqrt(1 - x² - y² - z²)
            const float32 sumSquares = a*a + b*b + d*d;
            const float32 missing = NkSqrt(NkMax(0.f, 1.f - sumSquares));

            // Reconstruction du quaternion complet
            float32 comps[4] = {};
            int idx = 0;
            for (int i = 0; i < 4; ++i)
            {
                if (i == static_cast<int>(largest))
                {
                    comps[i] = missing;
                    continue;
                }

                if (idx == 0) { comps[i] = a; }
                else if (idx == 1) { comps[i] = b; }
                else { comps[i] = d; }
                ++idx;
            }

            return NkQuatf{ comps[0], comps[1], comps[2], comps[3] };
        }

        void NkBitReader::ReadString(NkString& out, uint32 maxLen) noexcept
        {
            const uint16 len = ReadU16();

            // Validation de la longueur
            if (len > maxLen || IsOverflowed())
            {
                mOverflow = true;
                out.Clear();
                return;
            }

            // Lecture caractère par caractère dans un buffer temporaire
            char buf[257] = {};  // +1 pour le terminateur nul
            const uint32 readLen = (len < 256u) ? len : 256u;

            for (uint16 i = 0; i < readLen; ++i)
            {
                buf[i] = static_cast<char>(ReadU8());
            }
            buf[readLen] = '\0';  // Terminaison explicite

            out = buf;
        }

        void NkBitReader::ReadBytes(uint8* dst, uint32 size) noexcept
        {
            // Alignement sur byte requis
            AlignToByte();

            // Vérification de capacité
            const uint32 bytesRead = mBitPos / 8u;
            if (bytesRead + size <= mSize / 8u)
            {
                std::memcpy(dst, mBuf + bytesRead, size);
                mBitPos += size * 8u;
            }
            else
            {
                mOverflow = true;
            }
        }

        // =====================================================================
        // IMPLÉMENTATION : NkBitReader — Bits bruts et utilitaires
        // =====================================================================

        uint32 NkBitReader::ReadBits(uint32 numBits) noexcept
        {
            // Vérifications d'erreur
            if (mOverflow || mBitPos + numBits > mSize)
            {
                mOverflow = true;
                return 0u;
            }

            // Reconstruction de la valeur bit-à-bit (MSB en premier)
            uint32 result = 0u;
            for (uint32 i = 0; i < numBits; ++i)
            {
                const uint32 byteIdx = mBitPos / 8u;
                const uint32 bitIdx = 7u - (mBitPos % 8u);  // MSB en premier dans le byte

                // Extraction du bit depuis le buffer
                if ((mBuf[byteIdx] & (1u << bitIdx)) != 0u)
                {
                    result |= (1u << (numBits - 1u - i));
                }

                ++mBitPos;
            }

            return result;
        }

        void NkBitReader::AlignToByte() noexcept
        {
            const uint32 rem = mBitPos % 8u;
            if (rem != 0u)
            {
                mBitPos += (8u - rem);
            }
        }

        uint32 NkBitReader::BitsRead() const noexcept
        {
            return mBitPos;
        }

        uint32 NkBitReader::BytesRead() const noexcept
        {
            return (mBitPos + 7u) / 8u;
        }

        uint32 NkBitReader::BitsLeft() const noexcept
        {
            return (mBitPos <= mSize) ? (mSize - mBitPos) : 0u;
        }

        bool NkBitReader::IsOverflowed() const noexcept
        {
            return mOverflow;
        }

        bool NkBitReader::IsEmpty() const noexcept
        {
            return BitsLeft() == 0u;
        }

        uint32 NkBitReader::BitsRequired(uint32 maxVal) noexcept
        {
            if (maxVal == 0u) { return 1u; }

            uint32 bits = 0u;
            uint32 val = maxVal;
            while (val > 0u)
            {
                ++bits;
                val >>= 1;
            }
            return bits;
        }

    } // namespace net

} // namespace nkentseu

// =============================================================================
// NOTES D'IMPLÉMENTATION ET OPTIMISATIONS
// =============================================================================
/*
    Écriture/lecture bit-à-bit :
    ---------------------------
    - Les bits sont écrits MSB (Most Significant Bit) en premier dans chaque byte
    - Ordre : bit 7, bit 6, ..., bit 0 dans le byte, puis byte suivant
    - Cette convention assure la compatibilité big-endian pour les multi-byte values

    Quantification des floats :
    --------------------------
    - Formule : quant = round((v - min) / (max - min) * steps)
    - Déquantification : v = min + (quant / steps) * (max - min)
    - Précision effective : (max - min) / steps ≈ prec
    - Clamping intégré pour éviter les dépassements de plage

    Compression quaternion :
    -----------------------
    - Basé sur la propriété : pour q unitaire, max(|x|,|y|,|z|,|w|) ≥ 1/2
    - Si une composante est la plus grande, les autres sont ≤ 1/√2 ≈ 0.7071
    - Reconstruction : missing = sign * sqrt(1 - a² - b² - d²)
    - Perte de précision : ~0.0001 sur angles, acceptable pour la plupart des jeux

    Gestion d'erreur :
    -----------------
    - Flag mOverflow : une fois true, toutes les opérations suivantes sont no-op
    - Valeurs par défaut : 0 pour les numériques, false pour bool, nullptr pour pointeurs
    - IsOverflowed() doit être vérifié après chaque séquence de lecture/écriture

    Performance :
    ------------
    - Boucles bit-à-bit : O(numBits) — éviter pour > 64 bits, préférer WriteBytes
    - Méthodes simples : inline-friendly, optimisées par le compilateur
    - AlignToByte() : O(1) — essentiel avant accès mémoire direct

    Thread-safety :
    --------------
    - Non thread-safe par conception : un writer/reader par buffer/thread
    - Si partage requis : protéger avec mutex externe autour des appels
    - Alternative : pools de writers/readers pré-alloués par thread

    Extensions futures :
    -------------------
    - Support des tableaux avec préfixe de longueur : WriteArray<T>(data, count)
    - Compression delta : encoder les différences entre valeurs consécutives
    - Huffman coding : encodage variable-length pour données à distribution non-uniforme
    - Chiffrement intégré : WriteBytes chiffrés avec clé de session
*/

// ============================================================
// Copyright © 2024-2026 Rihen. Tous droits réservés.
// Licence Propriétaire - Libre d'utilisation et de modification
// ============================================================