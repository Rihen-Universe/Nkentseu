#pragma once
/**
 * @File    NkStreamReader.h
 * @Brief   Lecteur de flux binaire big-endian — base de tous les parsers.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  Tous les formats de police (TTF, OTF, CFF, BDF, WOFF) sont des flux
 *  binaires. NkStreamReader fournit une interface uniforme pour lire des
 *  entiers de 1 à 8 octets, en big-endian (défaut) ou little-endian.
 *
 *  Principe : le stream est une vue (uint8*, size) — pas de copie.
 *  Toutes les lectures avancent le curseur. Seek() repositionne absolument.
 *  IsEOF() et HasBytes() permettent la validation sans exception.
 *
 *  Les lectures hors-bornes retournent 0 et posent le flag mError.
 *  L'appelant vérifie IsValid() après parsing pour détecter la corruption.
 */

#include "NKFont/NkFontExport.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {

    class NKENTSEU_FONT_API NkStreamReader {
    public:

        NkStreamReader() noexcept = default;

        NkStreamReader(const uint8* data, usize size) noexcept
            : mData(data), mSize(size), mPos(0), mError(false) {}

        // ── Initialisation ────────────────────────────────────────────────────

        void Init(const uint8* data, usize size) noexcept {
            mData  = data;
            mSize  = size;
            mPos   = 0;
            mError = false;
        }

        // ── Lectures big-endian (format natif TTF/OTF/CFF/WOFF) ──────────────

        uint8  ReadU8()  noexcept { return Read<uint8>();  }
        uint16 ReadU16() noexcept { return ReadBE16();     }
        uint32 ReadU32() noexcept { return ReadBE32();     }
        uint64 ReadU64() noexcept { return ReadBE64();     }
        int8   ReadI8()  noexcept { return static_cast<int8> (ReadU8());  }
        int16  ReadI16() noexcept { return static_cast<int16>(ReadBE16()); }
        int32  ReadI32() noexcept { return static_cast<int32>(ReadBE32()); }

        // ── Lectures little-endian (BDF, PCF, certains segments WOFF2) ────────

        uint16 ReadU16LE() noexcept { return ReadLE16(); }
        uint32 ReadU32LE() noexcept { return ReadLE32(); }
        int16  ReadI16LE() noexcept { return static_cast<int16>(ReadLE16()); }
        int32  ReadI32LE() noexcept { return static_cast<int32>(ReadLE32()); }

        // ── Types TrueType spécifiques ────────────────────────────────────────

        /// Fixed 16.16 (table head, etc.)
        int32  ReadFixed()  noexcept { return ReadI32(); }
        /// FWORD : int16 en unités de design
        int16  ReadFWord()  noexcept { return ReadI16(); }
        /// UFWORD : uint16 en unités de design
        uint16 ReadUFWord() noexcept { return ReadU16(); }
        /// F2Dot14 : virgule fixe 2.14 (vecteurs de normalisation hinter)
        int16  ReadF2Dot14() noexcept { return ReadI16(); }
        /// LONGDATETIME : int64 secondes depuis 1904-01-01
        int64  ReadLongDateTime() noexcept {
            const uint64 hi = ReadU32();
            const uint64 lo = ReadU32();
            return static_cast<int64>((hi << 32) | lo);
        }
        /// Tag : 4 caractères ASCII (ex: 'cmap', 'glyf')
        uint32 ReadTag() noexcept { return ReadBE32(); }

        // ── Tableau d'octets ──────────────────────────────────────────────────

        /**
         * @Brief Lit `count` octets dans `dst`. Retourne le nombre réellement lus.
         */
        usize ReadBytes(uint8* dst, usize count) noexcept {
            if (!HasBytes(count)) { mError = true; count = mSize - mPos; }
            if (dst) {
                for (usize i = 0; i < count; ++i) dst[i] = mData[mPos + i];
            }
            mPos += count;
            return count;
        }

        /// Saute `count` octets.
        void Skip(usize count) noexcept {
            if (!HasBytes(count)) { mError = true; count = mSize - mPos; }
            mPos += count;
        }

        // ── Navigation ────────────────────────────────────────────────────────

        /// Repositionne le curseur à une position absolue.
        void Seek(usize pos) noexcept {
            if (pos > mSize) { mError = true; mPos = mSize; return; }
            mPos = pos;
        }

        /// Sauvegarde la position courante pour un retour ultérieur.
        NKFONT_NODISCARD usize Tell() const noexcept { return mPos; }

        /// Crée un sous-stream sur [offset, offset+size) du stream courant.
        NKFONT_NODISCARD NkStreamReader SubStream(usize offset, usize size) const noexcept {
            if (offset + size > mSize) return {};
            return NkStreamReader(mData + offset, size);
        }

        /// Accès direct au pointeur courant (lecture uniquement).
        NKFONT_NODISCARD const uint8* CurrentPtr() const noexcept {
            return mData + mPos;
        }

        // ── État ──────────────────────────────────────────────────────────────

        NKFONT_NODISCARD bool   IsValid()   const noexcept { return mData != nullptr && !mError; }
        NKFONT_NODISCARD bool   IsEOF()     const noexcept { return mPos >= mSize; }
        NKFONT_NODISCARD bool   HasBytes(usize n) const noexcept { return mPos + n <= mSize; }
        NKFONT_NODISCARD bool   HasError()  const noexcept { return mError; }
        NKFONT_NODISCARD usize  Size()      const noexcept { return mSize; }
        NKFONT_NODISCARD usize  Remaining() const noexcept { return mSize > mPos ? mSize - mPos : 0; }

        void ClearError() noexcept { mError = false; }

    private:
        const uint8* mData  = nullptr;
        usize        mSize  = 0;
        usize        mPos   = 0;
        bool         mError = false;

        template<typename T>
        T Read() noexcept {
            if (NKFONT_UNLIKELY(!HasBytes(sizeof(T)))) { mError = true; return T(0); }
            T v = mData[mPos];
            ++mPos;
            return v;
        }

        uint16 ReadBE16() noexcept {
            if (NKFONT_UNLIKELY(!HasBytes(2))) { mError = true; return 0; }
            const uint16 v = (static_cast<uint16>(mData[mPos]) << 8)
                           |  static_cast<uint16>(mData[mPos+1]);
            mPos += 2;
            return v;
        }
        uint32 ReadBE32() noexcept {
            if (NKFONT_UNLIKELY(!HasBytes(4))) { mError = true; return 0; }
            const uint32 v = (static_cast<uint32>(mData[mPos  ]) << 24)
                           | (static_cast<uint32>(mData[mPos+1]) << 16)
                           | (static_cast<uint32>(mData[mPos+2]) <<  8)
                           |  static_cast<uint32>(mData[mPos+3]);
            mPos += 4;
            return v;
        }
        uint64 ReadBE64() noexcept {
            const uint64 hi = ReadBE32();
            const uint64 lo = ReadBE32();
            return (hi << 32) | lo;
        }
        uint16 ReadLE16() noexcept {
            if (NKFONT_UNLIKELY(!HasBytes(2))) { mError = true; return 0; }
            const uint16 v =  static_cast<uint16>(mData[mPos  ])
                           | (static_cast<uint16>(mData[mPos+1]) << 8);
            mPos += 2;
            return v;
        }
        uint32 ReadLE32() noexcept {
            if (NKFONT_UNLIKELY(!HasBytes(4))) { mError = true; return 0; }
            const uint32 v =  static_cast<uint32>(mData[mPos  ])
                           | (static_cast<uint32>(mData[mPos+1]) <<  8)
                           | (static_cast<uint32>(mData[mPos+2]) << 16)
                           | (static_cast<uint32>(mData[mPos+3]) << 24);
            mPos += 4;
            return v;
        }
    };

    // ── Tag helpers ───────────────────────────────────────────────────────────
    constexpr uint32 MakeTag(char a, char b, char c, char d) noexcept {
        return (static_cast<uint32>(a) << 24)
             | (static_cast<uint32>(b) << 16)
             | (static_cast<uint32>(c) <<  8)
             |  static_cast<uint32>(d);
    }

    // Tags TrueType/OpenType courants
    namespace Tag {
        constexpr uint32 cmap = MakeTag('c','m','a','p');
        constexpr uint32 glyf = MakeTag('g','l','y','f');
        constexpr uint32 loca = MakeTag('l','o','c','a');
        constexpr uint32 head = MakeTag('h','e','a','d');
        constexpr uint32 hhea = MakeTag('h','h','e','a');
        constexpr uint32 hmtx = MakeTag('h','m','t','x');
        constexpr uint32 maxp = MakeTag('m','a','x','p');
        constexpr uint32 name = MakeTag('n','a','m','e');
        constexpr uint32 OS_2 = MakeTag('O','S','/','2');
        constexpr uint32 post = MakeTag('p','o','s','t');
        constexpr uint32 kern = MakeTag('k','e','r','n');
        constexpr uint32 GPOS = MakeTag('G','P','O','S');
        constexpr uint32 GSUB = MakeTag('G','S','U','B');
        constexpr uint32 GDEF = MakeTag('G','D','E','F');
        constexpr uint32 cvt  = MakeTag('c','v','t',' ');
        constexpr uint32 fpgm = MakeTag('f','p','g','m');
        constexpr uint32 prep = MakeTag('p','r','e','p');
        constexpr uint32 CFF  = MakeTag('C','F','F',' ');
        constexpr uint32 CFF2 = MakeTag('C','F','F','2');
        constexpr uint32 VORG = MakeTag('V','O','R','G');
        constexpr uint32 COLR = MakeTag('C','O','L','R');
        constexpr uint32 sbix = MakeTag('s','b','i','x');
    }

} // namespace nkentseu
