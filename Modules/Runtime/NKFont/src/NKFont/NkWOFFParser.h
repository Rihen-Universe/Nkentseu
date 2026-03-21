#pragma once
/**
 * @File    NkWOFFParser.h
 * @Brief   Parser WOFF/WOFF2 — Web Open Font Format.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  WOFF  (v1) : conteneur compressé zlib autour d'un fichier TTF/OTF.
 *               Chaque table est compressée indépendamment (ou non).
 *  WOFF2 (v2) : compression brotli du fichier entier (optionnel ici —
 *               on implémente inflate zlib from scratch pour WOFF1,
 *               et un décodeur brotli minimal pour WOFF2).
 *
 *  Pipeline :
 *    1. Valide le header WOFF (signature, version, numTables)
 *    2. Pour chaque table :
 *       - Si comprimée (origLength > compLength) : inflate zlib
 *       - Sinon : copie directe
 *    3. Reconstruit un fichier TTF/OTF valide en mémoire
 *    4. Passe le résultat à NkTTFParser
 *
 *  Inflate zlib implémenté from scratch (RFC 1950 + RFC 1951) :
 *    - Blocs non-compressés (BTYPE=00)
 *    - Blocs fixed Huffman (BTYPE=01)
 *    - Blocs dynamic Huffman (BTYPE=10)
 *    - Zéro dépendance externe
 */

#include "NKFont/NkFontExport.h"
#include "NKFont/NkMemArena.h"
#include "NKFont/NkTTFParser.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {

    // ── Signatures WOFF ───────────────────────────────────────────────────────
    static constexpr uint32 kWOFF_SIGNATURE  = 0x774F4646u; // 'wOFF'
    static constexpr uint32 kWOFF2_SIGNATURE = 0x774F4632u; // 'wOF2'

    // =========================================================================
    //  NkInflate — décompresseur deflate/zlib from scratch
    // =========================================================================

    class NKENTSEU_FONT_API NkInflate {
    public:
        /**
         * @Brief Décompresse un flux zlib (RFC 1950) vers un buffer de sortie.
         * @param in       Données compressées.
         * @param inSize   Taille des données compressées.
         * @param out      Buffer de sortie (alloué par l'appelant).
         * @param outSize  Taille du buffer de sortie.
         * @param written  Nombre d'octets réellement écrits.
         * @return true si succès, false si données corrompues.
         */
        static bool DecompressZlib(
            const uint8* in,  usize inSize,
            uint8*       out, usize outSize,
            usize&       written
        ) noexcept;

        /**
         * @Brief Décompresse un flux deflate brut (RFC 1951, sans en-tête zlib).
         */
        static bool DecompressDeflate(
            const uint8* in,  usize inSize,
            uint8*       out, usize outSize,
            usize&       written
        ) noexcept;

    private:
        // ── Contexte de décompression ─────────────────────────────────────────
        struct Context {
            const uint8* in;
            usize        inSize;
            usize        inPos;
            uint8*       out;
            usize        outSize;
            usize        outPos;
            uint32       bitBuf;
            int32        bitCount;
            bool         error;
        };

        // ── Huffman ───────────────────────────────────────────────────────────
        struct HuffTree {
            static constexpr int32 MAX_BITS   = 16;
            static constexpr int32 MAX_SYMS   = 288;
            int16  counts[MAX_BITS + 1];
            int16  symbols[MAX_SYMS];
            int32  numSymbols;
        };

        static uint32 ReadBits    (Context& ctx, int32 n) noexcept;
        static uint8  ReadByte    (Context& ctx) noexcept;
        static void   WriteByte   (Context& ctx, uint8 b) noexcept;

        static bool   BuildHuffTree (HuffTree& tree, const uint8* lengths, int32 n) noexcept;
        static int32  DecodeSymbol  (Context& ctx, const HuffTree& tree) noexcept;

        static bool   DecompressBlock      (Context& ctx, bool& lastBlock) noexcept;
        static bool   DecompressStored     (Context& ctx) noexcept;
        static bool   DecompressFixed      (Context& ctx) noexcept;
        static bool   DecompressDynamic    (Context& ctx) noexcept;
        static bool   DecompressHuffman    (Context& ctx,
                                             const HuffTree& litTree,
                                             const HuffTree& distTree) noexcept;

        // Tables de décompression deflate
        static const uint16 kLenBase[29];
        static const uint8  kLenExtra[29];
        static const uint16 kDistBase[30];
        static const uint8  kDistExtra[30];
        static const uint8  kCodeLenOrder[19];
    };

    // =========================================================================
    //  NkWOFFParser
    // =========================================================================

    class NKENTSEU_FONT_API NkWOFFParser {
    public:
        /**
         * @Brief Décompresse un fichier WOFF/WOFF2 et retourne un NkTTFFont parsé.
         * @param data     Données WOFF brutes.
         * @param size     Taille.
         * @param arena    Arena pour toutes les allocations.
         * @param out      Police parsée (NkTTFFont).
         * @return true si succès.
         */
        static bool Parse(
            const uint8* data,
            usize        size,
            NkMemArena&  arena,
            NkTTFFont&   out
        ) noexcept;

        /**
         * @Brief Décompresse seulement — retourne le buffer TTF/OTF reconstruit.
         *        Utile si l'appelant veut passer le résultat à un autre parser.
         */
        static bool Decompress(
            const uint8* data,
            usize        size,
            NkMemArena&  arena,
            uint8*&      outData,
            usize&       outSize
        ) noexcept;

    private:
        struct TableRecord {
            uint32 tag;
            uint32 offset;
            uint32 compLength;
            uint32 origLength;
            uint32 origChecksum;
        };

        static bool DecompressWOFF1(const uint8* data, usize size,
                                     NkMemArena& arena,
                                     uint8*& out, usize& outSize) noexcept;
        static bool DecompressWOFF2(const uint8* data, usize size,
                                     NkMemArena& arena,
                                     uint8*& out, usize& outSize) noexcept;
        static uint32 ComputeChecksum(const uint8* data, uint32 length) noexcept;
        static void   PatchHeadChecksum(uint8* sfnt, usize sfntSize) noexcept;
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkTTCParser — TrueType Collection (.ttc)
    // ─────────────────────────────────────────────────────────────────────────────

    class NKENTSEU_FONT_API NkTTCParser {
    public:
        /// Retourne le nombre de faces dans un fichier TTC.
        static bool GetFaceCount(const uint8* data, usize size, int32& count) noexcept;

        /// Parse la face à l'index donné (0-based) depuis un fichier TTC.
        static bool ParseFace(const uint8* data, usize size,
                               int32 faceIndex, NkMemArena& arena,
                               NkTTFFont& out) noexcept;
    };

} // namespace nkentseu
