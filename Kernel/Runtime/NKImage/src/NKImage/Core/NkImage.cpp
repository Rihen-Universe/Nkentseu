/**
 * @File    NkImage.cpp
 * @Brief   NkImage — Implémentation complète du conteneur d'image CPU.
 *          Couvre :
 *            - NkImageStream  : buffer de lecture/écriture binaire pour les codecs
 *            - NkDeflate      : inflate RFC 1951/1950 (adapté stb_image v2.16,
 *                               public domain, Sean Barrett) + deflate stored-blocks
 *            - NkImage        : cycle de vie, fabriques, codecs, manipulation
 *
 * @note    RÈGLE MÉMOIRE CRITIQUE
 *          Tout buffer alloué via les fabriques statiques (Alloc, Create, Copy, etc.)
 *          DOIT être libéré avec img->Free().
 *          Les buffers encodés (EncodePNG, EncodeJPEG, …) DOIVENT être libérés avec
 *          nkentseu::memory::NkFree(ptr).
 *          Ne JAMAIS utiliser std::free / delete[] : l'allocateur NKMemory n'est pas
 *          compatible avec le heap CRT standard (crash c0000374 sur Windows).
 *
 * @inflate_correctness
 *  stb_image utilise inflate LSB-first (DEFLATE RFC 1951) :
 *  - fill_bits  : bits |= byte << nbits   (accumule le LSB en premier)
 *  - zreceive   : v = bits & mask; bits >>= n  (consomme depuis le LSB)
 *  - fast table : index = bits & FAST_MASK  (direct, pas bit-reversed)
 *  - slow path  : k = bitrev16(bits) pour comparer avec maxcode[] pré-shifté
 *  - zBuildH    : remplit la fast table avec l'index bit-reversed du code
 *
 *  FIX zlib header (critique PNG) :
 *  - FDICT = bit 5 de FLG (in[1]), PAS de CMF (in[0])
 *  - CMF=0x78 a toujours bit5=1 (CINFO=7) → ancienne version if(in[0]&0x20)
 *    rejetait TOUS les PNG standard (0x78 0x9C, 0x78 0xDA, 0x78 0x5E, etc.)
 *  - Corrigé : if(flg & 0x20) return false  où flg = in[1]
 *
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
 */

// ─────────────────────────────────────────────────────────────────────────────
//  Inclusions
// ─────────────────────────────────────────────────────────────────────────────
#include "NKImage/Core/NkImage.h"

// Système de fichiers cross-platform (fallback AAssetManager sur Android)
#include "NKFileSystem/NkFile.h"

// Codecs image (un par format)
#include "NKImage/Codecs/PNG/NkPNGCodec.h"
#include "NKImage/Codecs/JPEG/NkJPEGCodec.h"
#include "NKImage/Codecs/BMP/NkBMPCodec.h"
#include "NKImage/Codecs/TGA/NkTGACodec.h"
#include "NKImage/Codecs/HDR/NkHDRCodec.h"
#include "NKImage/Codecs/EXR/NkEXRCodec.h"
#include "NKImage/Codecs/PPM/NkPPMCodec.h"
#include "NKImage/Codecs/QOI/NkQOICodec.h"
#include "NKImage/Codecs/GIF/NkGIFCodec.h"
#include "NKImage/Codecs/ICO/NkICOCodec.h"
#include "NKImage/Codecs/SVG/NkSVGCodec.h"

// Allocateur NKMemory (NkAlloc / NkFree / NkRealloc)
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"

// Flux NkStream (LoadFromStream / SaveToStream — NKIResource). Interface 100 %
// virtuelle pure : appels indirects, aucun symbole NKStream.lib requis ici.
#include "NKStream/NkStream.h"

// Pour placement new
#include <new>
#include <cstdio>

namespace nkentseu {
    using namespace nkentseu::memory;

    // =========================================================================
    //  §1  Wrappers mémoire internes
    //      Toutes les allocations passent par ces fonctions pour rester
    //      strictement dans l'allocateur NKMemory et éviter tout mélange
    //      accidentel avec le heap CRT.
    // =========================================================================

    /**
     * Alloue `n` octets (non initialisés).
     * Équivalent de malloc() mais via l'allocateur NKMemory.
     */
    static inline void* nkMalloc(usize n) noexcept {
        return NkAlloc(n);
    }

    /**
     * Alloue `n * s` octets mis à zéro.
     * Équivalent de calloc() mais via NKMemory.
     */
    static inline void* nkCalloc(usize n, usize s) noexcept {
        return NkAllocZero(n, s);
    }

    /**
     * Libère un pointeur alloué via nkMalloc/nkCalloc/nkRealloc.
     * Ne fait rien si p == nullptr.
     */
    static inline void nkFree(void* p) noexcept {
        if (p) NkFree(p);
    }

    /**
     * Réalloue un buffer.
     * @param p  Pointeur existant (ou nullptr pour une allocation initiale).
     * @param o  Ancienne taille (nécessaire pour certains allocateurs custom).
     * @param n  Nouvelle taille souhaitée.
     */
    static inline void* nkRealloc(void* p, usize o, usize n) noexcept {
        return NkRealloc(p, o, n);
    }

    /**
     * Copie mémoire interne — évite d'inclure <cstring> directement.
     */
    static inline void nkMemcpy(void* d, const void* s, usize n) noexcept {
        NkCopy(static_cast<uint8*>(d), static_cast<const uint8*>(s), n);
    }

    /**
     * Remplit une zone mémoire avec la valeur `v`.
     */
    static inline void nkMemset(void* d, int v, usize n) noexcept {
        NkSet(static_cast<uint8*>(d), static_cast<uint8>(v), n);
    }

    // =========================================================================
    //  §2  Helper luminance perceptuelle (déclaration forward)
    //      Défini plus bas dans §8 pour rester près de ConvertChannels.
    //      Utilisé aussi dans Create(fmt, color).
    // =========================================================================

    /**
     * Calcule la luminance perceptuelle (ITU-R BT.601) d'un triplet RGB 8 bits.
     * Formule : Y = (77*R + 150*G + 29*B) / 256   (entière, sans flottant).
     */
    static uint8 nkY(uint8 r, uint8 g, uint8 b) noexcept {
        return uint8((uint32(r) * 77u + uint32(g) * 150u + uint32(b) * 29u) >> 8);
    }

    // =========================================================================
    //  §3  NkImageStream — buffer de lecture/écriture binaire
    //      Utilisé par tous les codecs pour lire/écrire les fichiers images.
    //      Mode lecture  : construit avec (data, size), curseur mRdPos.
    //      Mode écriture : construit sans argument, buffer dynamique mWrBuf.
    // =========================================================================

    // -------------------------------------------------------------------------
    //  3.1  Gestion de la capacité du buffer d'écriture
    // -------------------------------------------------------------------------

    /**
     * Agrandit le buffer d'écriture pour pouvoir accueillir `needed` octets
     * supplémentaires.
     *
     * Stratégie de doublage de capacité :
     *   - Capacité initiale : 4096 octets (4 KiB, page typique)
     *   - Doublage jusqu'à atteindre la taille requise
     * Cette stratégie amortit le coût des réallocations en O(log n).
     *
     * @param needed  Nombre d'octets supplémentaires à garantir.
     * @return true si la réallocation a réussi (ou si elle n'était pas nécessaire).
     */
    bool NkImageStream::Grow(usize needed) noexcept {
        // Capacité déjà suffisante → rien à faire
        if (mWrSize + needed <= mWrCap) return true;

        // Calcul de la nouvelle capacité par doublage
        usize nc = mWrCap ? mWrCap * 2 : 4096;
        while (nc < mWrSize + needed) nc *= 2;

        // Réallocation via l'allocateur NKMemory
        uint8* nb = static_cast<uint8*>(nkRealloc(mWrBuf, mWrCap, nc));
        if (!nb) return false; // échec d'allocation

        mWrBuf = nb;
        mWrCap = nc;
        return true;
    }

    // -------------------------------------------------------------------------
    //  3.2  Primitives de lecture
    // -------------------------------------------------------------------------

    /**
     * Lit 1 octet non signé à la position courante et avance le curseur.
     * Si hors bornes, positionne mError=true et retourne 0.
     */
    uint8 NkImageStream::ReadU8() noexcept {
        if (mRdPos >= mRdSize) {
            mError = true;
            return 0;
        }
        return mRdData[mRdPos++];
    }

    /**
     * Lit 2 octets en big-endian (réseau) : octet haut en premier.
     * Utilisé par PNG et JPEG qui suivent la convention réseau.
     */
    uint16 NkImageStream::ReadU16BE() noexcept {
        if (mRdPos + 2 > mRdSize) {
            mError = true;
            return 0;
        }
        uint16 v = (uint16(mRdData[mRdPos]) << 8) | mRdData[mRdPos + 1];
        mRdPos += 2;
        return v;
    }

    /**
     * Lit 2 octets en little-endian : octet bas en premier.
     * Utilisé par BMP, TGA, QOI, EXR.
     */
    uint16 NkImageStream::ReadU16LE() noexcept {
        if (mRdPos + 2 > mRdSize) {
            mError = true;
            return 0;
        }
        uint16 v = mRdData[mRdPos] | (uint16(mRdData[mRdPos + 1]) << 8);
        mRdPos += 2;
        return v;
    }

    /**
     * Lit 4 octets en big-endian.
     * Ordre : [MSB][...][...][LSB].
     */
    uint32 NkImageStream::ReadU32BE() noexcept {
        if (mRdPos + 4 > mRdSize) {
            mError = true;
            return 0;
        }
        uint32 v = (uint32(mRdData[mRdPos    ]) << 24) |
                   (uint32(mRdData[mRdPos + 1]) << 16) |
                   (uint32(mRdData[mRdPos + 2]) <<  8) |
                    uint32(mRdData[mRdPos + 3]);
        mRdPos += 4;
        return v;
    }

    /**
     * Lit 4 octets en little-endian.
     * Ordre : [LSB][...][...][MSB].
     */
    uint32 NkImageStream::ReadU32LE() noexcept {
        if (mRdPos + 4 > mRdSize) {
            mError = true;
            return 0;
        }
        uint32 v =  uint32(mRdData[mRdPos    ])        |
                   (uint32(mRdData[mRdPos + 1]) <<  8) |
                   (uint32(mRdData[mRdPos + 2]) << 16) |
                   (uint32(mRdData[mRdPos + 3]) << 24);
        mRdPos += 4;
        return v;
    }

    /**
     * Lit 2 octets big-endian et les interprète comme un entier signé 16 bits.
     * Utilisé par les codecs EXR pour les champs signés.
     */
    int16 NkImageStream::ReadI16BE() noexcept {
        return int16(ReadU16BE());
    }

    /**
     * Lit 4 octets little-endian et les interprète comme un entier signé 32 bits.
     * Utilisé par BMP (dimensions peuvent être négatives pour le flip vertical).
     */
    int32 NkImageStream::ReadI32LE() noexcept {
        return int32(ReadU32LE());
    }

    /**
     * Lit exactement `n` octets dans le buffer `dst`.
     *
     * Si `dst` est nullptr, le curseur avance quand même (skip).
     * Si `n` dépasse la fin du buffer, mError est positionné et on lit
     * ce qui reste (retourne le nombre d'octets effectivement lus).
     *
     * @param dst  Destination (peut être nullptr pour avancer sans copier).
     * @param n    Nombre d'octets souhaités.
     * @return     Nombre d'octets réellement copiés/avancés.
     */
    usize NkImageStream::ReadBytes(uint8* dst, usize n) noexcept {
        if (mRdPos + n > mRdSize) {
            mError = true;
            n = mRdSize - mRdPos; // tronqué à ce qui reste
        }
        if (dst) nkMemcpy(dst, mRdData + mRdPos, n);
        mRdPos += n;
        return n;
    }

    /**
     * Avance le curseur de `n` octets sans copier.
     * Si `n` dépasse la fin, le curseur se positionne à la fin et mError=true.
     */
    void NkImageStream::Skip(usize n) noexcept {
        if (mRdPos + n > mRdSize) {
            mError = true;
            mRdPos = mRdSize;
        } else {
            mRdPos += n;
        }
    }

    /**
     * Positionne le curseur de lecture à l'offset absolu `pos` depuis le début.
     * Si `pos` > taille, positionne à la fin et mError=true.
     */
    void NkImageStream::Seek(usize pos) noexcept {
        if (pos > mRdSize) {
            mError = true;
            mRdPos = mRdSize;
        } else {
            mRdPos = pos;
        }
    }

    // -------------------------------------------------------------------------
    //  3.3  Primitives d'écriture
    // -------------------------------------------------------------------------

    /**
     * Écrit 1 octet dans le buffer de sortie.
     * Appelle Grow(1) pour garantir la capacité.
     */
    bool NkImageStream::WriteU8(uint8 v) noexcept {
        if (!Grow(1)) return false;
        mWrBuf[mWrSize++] = v;
        return true;
    }

    /**
     * Écrit 2 octets en big-endian.
     * Octet de poids fort en premier (convention réseau PNG/JPEG).
     */
    bool NkImageStream::WriteU16BE(uint16 v) noexcept {
        uint8 b[2] = { uint8(v >> 8), uint8(v) };
        return WriteBytes(b, 2);
    }

    /**
     * Écrit 2 octets en little-endian.
     * Octet de poids faible en premier (BMP, TGA, QOI).
     */
    bool NkImageStream::WriteU16LE(uint16 v) noexcept {
        uint8 b[2] = { uint8(v), uint8(v >> 8) };
        return WriteBytes(b, 2);
    }

    /**
     * Écrit 4 octets en big-endian.
     */
    bool NkImageStream::WriteU32BE(uint32 v) noexcept {
        uint8 b[4] = {
            uint8(v >> 24), uint8(v >> 16),
            uint8(v >>  8), uint8(v)
        };
        return WriteBytes(b, 4);
    }

    /**
     * Écrit 4 octets en little-endian.
     */
    bool NkImageStream::WriteU32LE(uint32 v) noexcept {
        uint8 b[4] = {
            uint8(v),       uint8(v >>  8),
            uint8(v >> 16), uint8(v >> 24)
        };
        return WriteBytes(b, 4);
    }

    /**
     * Écrit 4 octets little-endian signés (cast transparent vers uint32).
     */
    bool NkImageStream::WriteI32LE(int32 v) noexcept {
        return WriteU32LE(uint32(v));
    }

    /**
     * Écrit `n` octets depuis `src` dans le buffer de sortie.
     * Appelle Grow(n) pour garantir la capacité avant la copie.
     *
     * @param src  Données à écrire (doit être non-null et de taille >= n).
     * @param n    Nombre d'octets à écrire.
     */
    bool NkImageStream::WriteBytes(const uint8* src, usize n) noexcept {
        if (!Grow(n)) return false;
        nkMemcpy(mWrBuf + mWrSize, src, n);
        mWrSize += n;
        return true;
    }

    // TakeBuffer() est inline dans le header — transfert de propriété du buffer.

    // =========================================================================
    //  §4  NkDeflate — tables statiques DEFLATE (RFC 1951)
    //      Ces tables sont des constantes de la norme, elles ne changent jamais.
    // =========================================================================

    /**
     * kLenBase[i] : longueur de base pour les codes de longueur 257+i.
     * RFC 1951, section 3.2.5, tableau 1.
     * Les codes 257–285 encodent des longueurs de 3 à 258 octets.
     */
    const uint16 NkDeflate::kLenBase[29] = {
        3,   4,   5,   6,   7,   8,   9,  10,
        11,  13,  15,  17,  19,  23,  27,  31,
        35,  43,  51,  59,  67,  83,  99, 115,
       131, 163, 195, 227, 258
    };

    /**
     * kLenExtra[i] : nombre de bits supplémentaires à lire pour affiner la longueur.
     * Correspond à kLenBase[i].
     */
    const uint8 NkDeflate::kLenExtra[29] = {
        0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4,
        5, 5, 5, 5, 0
    };

    /**
     * kDistBase[i] : distance de base pour les codes de distance 0–29.
     * RFC 1951, section 3.2.5, tableau 2.
     * Les distances vont de 1 à 32768 octets en arrière dans le flux déjà décodé.
     */
    const uint16 NkDeflate::kDistBase[30] = {
           1,    2,    3,    4,    5,    7,    9,   13,
          17,   25,   33,   49,   65,   97,  129,  193,
         257,  385,  513,  769, 1025, 1537, 2049, 3073,
        4097, 6145, 8193,12289,16385,24577
    };

    /**
     * kDistExtra[i] : bits supplémentaires pour affiner la distance.
     */
    const uint8 NkDeflate::kDistExtra[30] = {
         0,  0,  0,  0,  1,  1,  2,  2,
         3,  3,  4,  4,  5,  5,  6,  6,
         7,  7,  8,  8,  9,  9, 10, 10,
        11, 11, 12, 12, 13, 13
    };

    /**
     * kCLOrder[19] : ordre de lecture des longueurs CL dans un bloc dynamique.
     * RFC 1951 : l'ordre spécial minimise les codes les plus fréquents (16, 17, 18).
     */
    const uint8 NkDeflate::kCLOrder[19] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
    };

    /**
     * kZDefLen[288] : longueurs de codes pour les symboles littéraux/longueur
     * dans un bloc à codes fixes (BTYPE=01, RFC 1951 section 3.2.6).
     * Symboles 0–143   → 8 bits
     * Symboles 144–255 → 9 bits
     * Symboles 256–279 → 7 bits
     * Symboles 280–287 → 8 bits
     */
    const uint8 NkDeflate::kZDefLen[288] = {
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
        9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
        9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
        9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
        9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
        9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
        9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,
        8,8,8,8,8,8,8,8
    };

    /**
     * kZDefDist[32] : longueurs de codes pour les codes de distance fixes.
     * Tous sur 5 bits (RFC 1951).
     */
    const uint8 NkDeflate::kZDefDist[32] = {
        5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
        5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5
    };

    // =========================================================================
    //  §5  NkDeflate — inflate LSB-first (algorithme stb_image v2.16 exact)
    //
    //  DEFLATE est un format bit-serial LSB-first :
    //    - Les bits d'un octet sont lus du LSB vers le MSB.
    //    - Le registre d'accumulation `bits` contient les bits dans l'ordre
    //      d'arrivée : bit 0 du registre = prochain bit à consommer.
    //    - L'index dans la fast table Huffman = les FAST premiers bits du
    //      registre, qui correspondent au code Huffman bit-reversed.
    // =========================================================================

    // -------------------------------------------------------------------------
    //  5.1  Utilitaire bit-reverse (identique à stbi__bit_reverse)
    // -------------------------------------------------------------------------

    /**
     * Renverse les 16 bits de poids faible de `n`.
     * Utilisé par zBuildH (construction de la LUT fast) et zDecode (slow path).
     * Algorithme : échange par paires de bits croissantes (butterfly).
     */
    static int32 nkBR16(int32 n) noexcept {
        n = ((n & 0xAAAA) >>  1) | ((n & 0x5555) <<  1);
        n = ((n & 0xCCCC) >>  2) | ((n & 0x3333) <<  2);
        n = ((n & 0xF0F0) >>  4) | ((n & 0x0F0F) <<  4);
        n = ((n & 0xFF00) >>  8) | ((n & 0x00FF) <<  8);
        return n;
    }

    /**
     * Renverse les `bits` premiers bits de `v`.
     * Utilisé lors de la construction de la fast table : pour un code canonique
     * `v` de longueur `bits`, l'index dans la fast table est nkBR(v, bits).
     */
    static int32 nkBR(int32 v, int32 bits) noexcept {
        return nkBR16(v) >> (16 - bits);
    }

    // -------------------------------------------------------------------------
    //  5.2  Primitives de décodage de bits
    // -------------------------------------------------------------------------

    /**
     * Remplit le registre de bits `z.bits` depuis le flux compressé.
     * On accumule au moins 25 bits valides (suffisant pour un code Huffman max
     * de 15 bits + 10 bits d'extras de distance).
     *
     * Accumulation LSB-first :
     *   bits |= byte << nbits
     * Le prochain bit à lire est toujours dans le bit 0 de z.bits.
     *
     * En cas de fin de flux prématurée, z.err=true est positionné.
     */
    void NkDeflate::zFill(ZBuf& z) noexcept {
        // On continue tant qu'il reste de la place dans le registre 32 bits
        while (z.nbits <= 24) {
            if (z.pos >= z.size) {
                // Fin de flux inattendue
                z.err = true;
                return;
            }
            // Accumulation LSB-first : le nouvel octet occupe les bits [nbits..nbits+7]
            z.bits |= uint32(z.data[z.pos++]) << z.nbits;
            z.nbits += 8;
        }
    }

    /**
     * Extrait et consomme `n` bits depuis le registre (LSB-first).
     * Remplit le registre si nécessaire via zFill().
     *
     * @param n  Nombre de bits à extraire (0..25).
     * @return   Les `n` bits de poids faible du registre avant consommation.
     */
    uint32 NkDeflate::zBits(ZBuf& z, int32 n) noexcept {
        if (!n) return 0;
        if (z.nbits < n) zFill(z); // garantit nbits >= n
        uint32 v = z.bits & ((1u << n) - 1); // masque les n bits LSB
        z.bits  >>= n;  // consomme en décalant vers le bas
        z.nbits  -= n;
        return v;
    }

    // -------------------------------------------------------------------------
    //  5.3  Construction des tables Huffman canoniques
    // -------------------------------------------------------------------------

    /**
     * Construit une table de décodage Huffman depuis la liste des longueurs `szList`.
     *
     * Structure duale :
     *  1. Fast table (h.fast[1<<FAST]) : pour les codes <= FAST(=9) bits,
     *     indexée directement par les FAST bits LSB du registre (= code bit-reversed).
     *     Valeur : (longueur << 9) | symbole, ou 0 si non utilisé.
     *  2. Slow table (firstcode, maxcode, firstsym, sizes, values) :
     *     pour les codes > FAST bits, utilise maxcode[] pré-shifté pour
     *     comparer avec le bit-reverse des 16 bits LSB du registre.
     *
     * @param h       Table à initialiser.
     * @param szList  Longueurs de code pour chaque symbole (0 = symbole absent).
     * @param num     Nombre de symboles (taille de szList).
     * @return false si la liste est invalide (trop de codes pour une longueur donnée).
     */
    bool NkDeflate::zBuildH(ZHuff& h, const uint8* szList, int32 num) noexcept {
        int32 i, k = 0, code;
        int32 nc[16];     // compteur de codes courant par longueur (pour l'assignation)
        int32 sizes[17];  // nombre de codes pour chaque longueur 0..16

        // Zéro-init des structures de sortie
        nkMemset(sizes, 0, sizeof(sizes));
        nkMemset(h.fast, 0, sizeof(h.fast));

        // Passe 1 : compte les symboles par longueur de code
        for (i = 0; i < num; ++i) {
            if (szList[i]) ++sizes[szList[i]];
        }
        sizes[0] = 0; // les symboles de longueur 0 n'ont pas de code

        // Validation : on ne peut pas avoir plus de 2^l codes de longueur l
        for (i = 1; i < 16; ++i) {
            if (sizes[i] > (1 << i)) {
                nkMemset(&h, 0, sizeof(h));
                return false;
            }
        }

        // Passe 2 : assigne les codes canoniques (algorithme RFC 1951)
        // et calcule firstcode[], firstsym[], maxcode[] pour le slow path.
        code = 0;
        for (i = 1; i < 16; ++i) {
            nc[i] = code;                      // premier code pour cette longueur
            h.firstcode[i] = uint16(code);
            h.firstsym[i]  = uint16(k);        // premier index dans sizes[]/values[]
            code += sizes[i];
            if (sizes[i] && code - 1 >= (1 << i)) {
                // Dépassement de capacité : liste invalide
                nkMemset(&h, 0, sizeof(h));
                return false;
            }
            // maxcode[i] pré-shifté de (16-i) bits pour le slow path :
            // la comparaison se fait avec le registre bit-reversed sur 16 bits.
            h.maxcode[i] = code << (16 - i);
            code <<= 1; // décalage pour la longueur suivante
            k += sizes[i];
        }
        h.maxcode[16] = 0x10000; // sentinelle (aucun code de longueur > 15)

        // Passe 3 : remplit la fast table et les tableaux sizes[]/values[]
        for (i = 0; i < num; ++i) {
            int32 s = szList[i];
            if (!s) continue; // symbole absent

            // Position de ce symbole dans les tableaux triés par code
            int32 c = nc[s] - h.firstcode[s] + h.firstsym[s];
            uint16 fv = uint16((s << 9) | i); // (longueur << 9) | symbole

            h.sizes[c]  = uint8(s);
            h.values[c] = uint16(i);

            // Remplit la fast table si le code est assez court
            if (s <= ZHuff::FAST) {
                // L'index dans la fast table est le code Huffman bit-reversed
                // (car le registre stocke les bits dans l'ordre LSB-first,
                //  ce qui correspond au code bit-reversed).
                int32 j = nkBR(nc[s], s);
                while (j < (1 << ZHuff::FAST)) {
                    h.fast[j] = fv;
                    j += (1 << s); // toutes les entrées qui partagent ce préfixe
                }
            }
            ++nc[s]; // prochain code pour cette longueur
        }
        return true;
    }

    // -------------------------------------------------------------------------
    //  5.4  Décodage d'un symbole Huffman
    // -------------------------------------------------------------------------

    /**
     * Décode un symbole Huffman depuis le registre de bits.
     *
     * Fast path O(1) : si les FAST bits LSB du registre donnent une entrée
     * valide dans h.fast[], on consomme directement le code et retourne le symbole.
     *
     * Slow path O(longueur) : pour les codes > FAST bits, on bit-reverse les
     * 16 bits LSB et on cherche la longueur par comparaison avec maxcode[].
     * Puis on retrouve le symbole dans values[] via firstsym[].
     *
     * @return Symbole décodé (0..287 pour littéraux/longueurs, 0..29 pour distances),
     *         ou -1 si erreur (code invalide ou fin de flux).
     */
    int32 NkDeflate::zDecode(ZBuf& z, const ZHuff& h) noexcept {
        // Garantit au moins 16 bits valides pour le slow path
        if (z.nbits < 16) zFill(z);

        // ── Fast path : O(1) pour les codes <= FAST bits ──────────────────────
        // Les FAST bits LSB du registre = le code Huffman bit-reversed
        // (car DEFLATE est LSB-first et la fast table est indexée par bit-reverse).
        uint16 fv = h.fast[z.bits & ((1 << ZHuff::FAST) - 1)];
        if (fv) {
            int32 s = fv >> 9;  // longueur du code
            z.bits  >>= s;
            z.nbits  -= s;
            return fv & 511;    // symbole (9 bits de poids faible)
        }

        // ── Slow path : O(longueur) pour les codes > FAST bits ────────────────
        // On bit-reverse les 16 bits LSB du registre pour obtenir le code
        // dans l'espace canonique (MSB-first).
        int32 k = nkBR16(int32(z.bits & 0xFFFF));
        int32 s;
        for (s = ZHuff::FAST + 1; ; ++s) {
            if (k < h.maxcode[s]) break; // longueur trouvée
        }
        if (s == 16) {
            // Aucune longueur valide trouvée : code invalide
            z.err = true;
            return -1;
        }
        {
            // Index du symbole dans le tableau trié
            int32 b = (k >> (16 - s)) - h.firstcode[s] + h.firstsym[s];
            if (b < 0 || b >= 288 || h.sizes[b] != s) {
                z.err = true;
                return -1;
            }
            z.bits  >>= s;
            z.nbits  -= s;
            return h.values[b];
        }
    }

    // -------------------------------------------------------------------------
    //  5.5  Décodage d'un bloc Huffman (types 1 et 2)
    // -------------------------------------------------------------------------

    /**
     * Décode le contenu d'un bloc Huffman (fixed ou dynamic) jusqu'au
     * code fin de bloc (symbole 256).
     *
     * Les symboles < 256 sont copiés tels quels dans le buffer de sortie.
     * Les symboles 257–285 encodent des paires (longueur, distance) qui
     * référencent une copie dans les données déjà décodées (LZ77).
     *
     * @param zl  Table Huffman des littéraux/longueurs.
     * @param zd  Table Huffman des distances.
     */
    bool NkDeflate::zHuffBlock(ZBuf& z, const ZHuff& zl, const ZHuff& zd) noexcept {
        for (;;) {
            int32 sym = zDecode(z, zl);
            if (z.err || sym < 0) return false;

            if (sym < 256) {
                // ── Littéral : copie directe dans le buffer de sortie ──────────
                if (z.outPos >= z.outCap) {
                    z.err = true;
                    return false;
                }
                z.out[z.outPos++] = uint8(sym);

            } else if (sym == 256) {
                // ── Fin de bloc ────────────────────────────────────────────────
                return true;

            } else {
                // ── Référence arrière LZ77 : copie (longueur, distance) ────────
                int32 li = sym - 257; // index dans kLenBase[]
                if (li < 0 || li >= 29) {
                    z.err = true;
                    return false;
                }
                uint32 len  = kLenBase[li]  + zBits(z, kLenExtra[li]);

                int32 ds = zDecode(z, zd);   // code de distance
                if (z.err || ds < 0 || ds >= 30) {
                    z.err = true;
                    return false;
                }
                uint32 dist = kDistBase[ds] + zBits(z, kDistExtra[ds]);

                // Vérification de cohérence : la référence doit pointer dans
                // les données déjà décodées (pas avant le début du buffer).
                if (z.outPos < dist) {
                    z.err = true;
                    return false;
                }
                if (z.outPos + len > z.outCap) {
                    z.err = true;
                    return false;
                }

                // Copie octets par octets (la source peut chevaucher la dest
                // quand dist < len, ce qui est valide en DEFLATE).
                for (uint32 i = 0; i < len; ++i) {
                    z.out[z.outPos] = z.out[z.outPos - dist];
                    ++z.outPos;
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    //  5.6  Bloc non compressé (BTYPE = 00)
    // -------------------------------------------------------------------------

    /**
     * Décode un bloc stored (non compressé).
     *
     * Structure d'un bloc stored :
     *   - Alignement sur octet (jette les bits restants du bloc courant)
     *   - LEN  : longueur sur 2 octets little-endian
     *   - NLEN : complément 1 de LEN (vérification d'intégrité)
     *   - LEN octets de données verbatim
     *
     * ATTENTION : zFill() peut avoir préchargé des octets du flux dans le
     * registre z.bits. Il faut reculer le pointeur z.pos pour les "rendre".
     */
    bool NkDeflate::zStored(ZBuf& z) noexcept {
        // Jette les bits partiels pour s'aligner sur l'octet suivant
        if (z.nbits & 7) zBits(z, z.nbits & 7);

        // Rend les octets préchargés dans le registre (zFill en avance trop)
        {
            int32 extra = z.nbits / 8;
            if (int32(z.pos) >= extra) z.pos -= extra;
        }
        // Le registre est maintenant vide
        z.bits  = 0;
        z.nbits = 0;

        // Lecture de LEN et NLEN
        if (z.pos + 4 > z.size) {
            z.err = true;
            return false;
        }
        uint16 len  = z.data[z.pos] | (uint16(z.data[z.pos + 1]) << 8);
        z.pos += 2;
        uint16 nlen = z.data[z.pos] | (uint16(z.data[z.pos + 1]) << 8);
        z.pos += 2;

        // Vérification LEN ^ NLEN == 0xFFFF
        if (uint16(len ^ nlen) != 0xFFFF) {
            z.err = true;
            return false;
        }

        // Vérification des bornes source et destination
        if (z.pos + len > z.size) {
            z.err = true;
            return false;
        }
        if (z.outPos + len > z.outCap) {
            z.err = true;
            return false;
        }

        // Copie verbatim des données non compressées
        nkMemcpy(z.out + z.outPos, z.data + z.pos, len);
        z.pos    += len;
        z.outPos += len;
        return true;
    }

    // -------------------------------------------------------------------------
    //  5.7  Bloc à codes fixes (BTYPE = 01)
    // -------------------------------------------------------------------------

    /**
     * Décode un bloc à codes Huffman fixes (RFC 1951 section 3.2.6).
     * Les tables de décodage sont construites à partir des longueurs
     * kZDefLen (288 symboles littéraux/longueurs) et kZDefDist (32 distances).
     */
    bool NkDeflate::zFixed(ZBuf& z) noexcept {
        ZHuff zl, zd;
        if (!zBuildH(zl, kZDefLen, 288)) {
            z.err = true;
            return false;
        }
        if (!zBuildH(zd, kZDefDist, 32)) {
            z.err = true;
            return false;
        }
        return zHuffBlock(z, zl, zd);
    }

    // -------------------------------------------------------------------------
    //  5.8  Bloc à codes dynamiques (BTYPE = 10)
    // -------------------------------------------------------------------------

    /**
     * Décode un bloc à codes Huffman dynamiques (RFC 1951 section 3.2.7).
     *
     * En-tête dynamique :
     *   - HLIT  (5 bits) : nombre de codes littéraux/longueur - 257
     *   - HDIST (5 bits) : nombre de codes de distance - 1
     *   - HCLEN (4 bits) : nombre de longueurs de la table CL - 4
     *
     * Étape 1 : on lit HCLEN longueurs pour construire la table CL (code-length).
     * Étape 2 : on utilise la table CL pour décoder HLIT+HDIST longueurs
     *           qui définissent les tables litérales et de distance.
     * Étape 3 : décodage du contenu avec zHuffBlock().
     */
    bool NkDeflate::zDynamic(ZBuf& z) noexcept {
        int32 hlit  = int32(zBits(z, 5)) + 257; // [257..286]
        int32 hdist = int32(zBits(z, 5)) + 1;   // [1..32]
        int32 hclen = int32(zBits(z, 4)) + 4;   // [4..19]
        if (z.err) return false;

        // ── Étape 1 : table CL (code-length, pour décoder les autres tables) ──
        uint8 clen[19] = {};
        for (int32 i = 0; i < hclen; ++i) {
            clen[kCLOrder[i]] = uint8(zBits(z, 3));
        }
        ZHuff zc;
        if (!zBuildH(zc, clen, 19)) {
            z.err = true;
            return false;
        }

        // ── Étape 2 : décodage de HLIT+HDIST longueurs ─────────────────────
        uint8 lens[320] = {}; // 286 + 32 max
        int32 n = 0, total = hlit + hdist;
        while (n < total && !z.err) {
            int32 c = zDecode(z, zc);
            if (z.err || c < 0) return false;

            if (c < 16) {
                // Longueur directe
                lens[n++] = uint8(c);
            } else if (c == 16) {
                // Répétition de la longueur précédente, 3..6 fois
                if (n == 0) { z.err = true; return false; }
                int32 r = int32(zBits(z, 2)) + 3;
                uint8 p = lens[n - 1];
                while (r-- && n < total) lens[n++] = p;
            } else if (c == 17) {
                // Zéros répétés, 3..10 fois
                int32 r = int32(zBits(z, 3)) + 3;
                while (r-- && n < total) lens[n++] = 0;
            } else { // c == 18
                // Zéros répétés, 11..138 fois
                int32 r = int32(zBits(z, 7)) + 11;
                while (r-- && n < total) lens[n++] = 0;
            }
        }

        // ── Étape 3 : construction des tables litérale et de distance ─────────
        ZHuff zl, zd;
        if (!zBuildH(zl, lens,         hlit )) { z.err = true; return false; }
        if (!zBuildH(zd, lens + hlit,  hdist)) { z.err = true; return false; }
        return zHuffBlock(z, zl, zd);
    }

    // -------------------------------------------------------------------------
    //  5.9  Dispatch de bloc
    // -------------------------------------------------------------------------

    /**
     * Décode un bloc DEFLATE : lit BFINAL (1 bit) et BTYPE (2 bits),
     * puis dispatch vers zStored, zFixed ou zDynamic.
     *
     * @param last  Reçoit true si BFINAL=1 (dernier bloc du flux).
     */
    bool NkDeflate::zBlock(ZBuf& z, bool& last) noexcept {
        last = bool(zBits(z, 1));          // BFINAL
        uint32 type = zBits(z, 2);         // BTYPE
        if (z.err) return false;
        switch (type) {
            case 0: return zStored(z);     // non compressé
            case 1: return zFixed(z);      // codes fixes RFC 1951
            case 2: return zDynamic(z);    // codes dynamiques
            default:
                z.err = true;
                return false;             // BTYPE=11 réservé, invalide
        }
    }

    // -------------------------------------------------------------------------
    //  5.10  Entrée inflate principale
    // -------------------------------------------------------------------------

    /**
     * Point d'entrée principal du décompresseur.
     * Si `hdr == true` : parse l'en-tête zlib RFC 1950 (CMF + FLG).
     *
     * ─── FIX CRITIQUE (en-tête zlib) ───────────────────────────────────────
     *  RFC 1950 : CMF = in[0], FLG = in[1].
     *  Le bit FDICT est le bit 5 de FLG (in[1]), PAS de CMF (in[0]).
     *  CMF=0x78 (le plus courant pour PNG : compression niveau 6) a toujours
     *  le bit 5 positionné à 1 (CINFO=7 dans les bits 4..7 de CMF).
     *  L'ancienne version testait if(in[0] & 0x20) et rejetait donc TOUS les
     *  PNG standard. Correction : if(flg & 0x20).
     * ────────────────────────────────────────────────────────────────────────
     *
     * Après l'en-tête (si présent), décode les blocs jusqu'au dernier.
     */
    bool NkDeflate::zInflate(ZBuf& z, bool hdr) noexcept {
        if (hdr) {
            if (z.size < 2) {
                z.err = true;
                return false;
            }
            uint8 cmf = z.data[z.pos++];
            uint8 flg = z.data[z.pos++];

            // CM (bits 0..3 de CMF) doit être 8 (DEFLATE)
            if ((cmf & 0x0F) != 8) {
                z.err = true;
                return false;
            }
            // Vérification de la somme de contrôle zlib : (CMF*256 + FLG) % 31 == 0
            if (((uint16(cmf) << 8) | flg) % 31 != 0) {
                z.err = true;
                return false;
            }
            // FDICT (dictionnaire preset) : bit 5 de FLG — NON supporté
            // (aucun PNG standard n'utilise FDICT=1)
            if (flg & 0x20) {
                z.err = true;
                return false;
            }
        }

        // Décode tous les blocs jusqu'au bloc final (BFINAL=1)
        bool last = false;
        while (!last && !z.err) {
            if (!zBlock(z, last)) return false;
        }
        return !z.err;
    }

    // =========================================================================
    //  §6  NkDeflate — API publique inflate/deflate
    // =========================================================================

    /**
     * Décompresse un flux zlib RFC 1950 (avec en-tête CMF/FLG et Adler-32).
     * C'est la fonction utilisée par le codec PNG pour décoder les blocs IDAT.
     *
     * @param in      Buffer compressé (flux zlib complet).
     * @param inSz    Taille du flux compressé.
     * @param out     Buffer de sortie pré-alloué par l'appelant.
     * @param outCap  Capacité du buffer de sortie en octets.
     * @param w       [out] Nombre d'octets effectivement écrits.
     * @return true si la décompression a réussi sans erreur.
     */
    bool NkDeflate::Decompress(const uint8* in, usize inSz,
                                uint8* out, usize outCap,
                                usize& w) noexcept {
        w = 0;
        if (!in || !out || inSz < 2) return false;

        ZBuf z{};
        z.data   = in;
        z.size   = inSz;
        z.out    = out;
        z.outCap = outCap;

        if (!zInflate(z, /*parseHeader=*/true)) return false;
        w = z.outPos;
        return true;
    }

    /**
     * Décompresse un flux DEFLATE brut RFC 1951 (sans en-tête zlib ni Adler-32).
     * Utile pour les formats qui embarquent du DEFLATE brut (ZIP, gzip body, etc.).
     *
     * @param in      Buffer DEFLATE brut.
     * @param inSz    Taille du flux.
     * @param out     Buffer de sortie pré-alloué.
     * @param outCap  Capacité du buffer de sortie.
     * @param w       [out] Octets écrits.
     */
    bool NkDeflate::DecompressRaw(const uint8* in, usize inSz,
                                   uint8* out, usize outCap,
                                   usize& w) noexcept {
        w = 0;
        if (!in || !out) return false;

        ZBuf z{};
        z.data   = in;
        z.size   = inSz;
        z.out    = out;
        z.outCap = outCap;

        if (!zInflate(z, /*parseHeader=*/false)) return false;
        w = z.outPos;
        return true;
    }

    /**
     * Calcule la somme de contrôle Adler-32 d'un bloc de données.
     *
     * Adler-32 RFC 1950 :
     *   s1 = (s1 + byte)  mod 65521   (65521 = plus grand premier < 2^16)
     *   s2 = (s2 + s1)    mod 65521
     *   résultat = (s2 << 16) | s1
     *
     * @param data  Données à hasher.
     * @param size  Taille en octets.
     * @param prev  Checksum initial (1 pour un nouveau calcul, valeur précédente
     *              pour un calcul incrémental sur plusieurs appels).
     */
    uint32 NkDeflate::Adler32(const uint8* data, usize size, uint32 prev) noexcept {
        uint32 s1 =  prev        & 0xFFFF; // octet bas
        uint32 s2 = (prev >> 16) & 0xFFFF; // octet haut
        for (usize i = 0; i < size; ++i) {
            s1 = (s1 + data[i]) % 65521;
            s2 = (s2 + s1)      % 65521;
        }
        return (s2 << 16) | s1;
    }

    /**
     * Compresse `in` en un flux zlib RFC 1950 avec stored blocks (BTYPE=00).
     *
     * Un stored block est un bloc DEFLATE sans compression :
     *   - BFINAL (1 bit) + BTYPE=00 (2 bits) : 0x01 pour le dernier bloc
     *   - LEN  (2 octets LE) : longueur des données (max 65535)
     *   - NLEN (2 octets LE) : complément 1 de LEN
     *   - données verbatim
     *
     * Le flux produit est un zlib valide avec en-tête CMF/FLG et checksum
     * Adler-32 en fin. Tout décompresseur standard peut le relire.
     *
     * Note : le ratio de compression est 1:1 + ~6 octets d'overhead par bloc.
     * C'est suffisant pour les PNG produits par NkPNGCodec (correctitude > vitesse).
     *
     * @param in      Données brutes à "compresser".
     * @param inSz    Taille des données.
     * @param outData [out] Buffer zlib alloué via NkAlloc — DOIT être NkFree par l'appelant.
     * @param outSz   [out] Taille du buffer produit.
     * @param level   Niveau de compression (ignoré : stored uniquement pour l'instant).
     */
    bool NkDeflate::Compress(const uint8* in, usize inSz,
                              uint8*& outData, usize& outSz,
                              int32 /*level*/) noexcept {
        NkImageStream s; // buffer d'écriture dynamique

        // ── En-tête zlib (CMF + FLG) ──────────────────────────────────────────
        // CMF = 0x78 : CM=8 (DEFLATE), CINFO=7 (fenêtre 32 KiB)
        // FLG : calculé pour que (CMF*256 + FLG) % 31 == 0, FDICT=0, FLEVEL=0
        uint8 cmf = 0x78;
        uint8 flg = uint8(31 - (uint16(cmf) * 256u % 31u));
        flg &= ~0x20u; // s'assure que FDICT=0
        if (((uint16(cmf) << 8) | flg) % 31 != 0) flg = 0; // sécurité
        s.WriteU8(cmf);
        s.WriteU8(flg);

        // ── Blocs stored (taille max 65535 octets par bloc) ───────────────────
        usize        rem = inSz;
        const uint8* src = in;
        while (rem > 0) {
            uint16 bl   = uint16(rem > 65535u ? 65535u : rem);
            uint16 nl   = uint16(~bl);
            bool   last = (bl == rem); // dernier bloc → BFINAL=1

            s.WriteU8(last ? 0x01u : 0x00u); // BFINAL | BTYPE=00
            s.WriteU16LE(bl);                 // LEN
            s.WriteU16LE(nl);                 // NLEN = ~LEN
            s.WriteBytes(src, bl);            // données verbatim

            src += bl;
            rem -= bl;
        }

        // ── Checksum Adler-32 (big-endian, fin du flux zlib) ─────────────────
        s.WriteU32BE(Adler32(in, inSz, 1));

        return s.TakeBuffer(outData, outSz);
    }

    // =========================================================================
    //  §7  NkImage — fabriques bas niveau (Alloc, Wrap)
    // =========================================================================

    /**
     * Alloue une image vide (pixels zeroed) de dimensions (w × h) et de format `fmt`.
     *
     * - Calcule le stride aligné sur 4 octets : stride = (w * bpp + 3) & ~3
     *   (alignement standard attendu par la plupart des API graphiques).
     * - Utilise placement new pour initialiser correctement les membres.
     * - Le struct NkImage ET les pixels sont alloués via NkAlloc.
     *
     * @return Nouvelle image owning, ou nullptr si w/h/fmt invalides ou OOM.
     */
    NkImage* NkImage::Alloc(int32 w, int32 h, NkImagePixelFormat fmt) noexcept {
        if (w <= 0 || h <= 0) return nullptr;
        const int32 bpp = BytesPerPixelOf(fmt);
        if (!bpp) return nullptr;

        // Alloue la structure NkImage
        NkImage* img = static_cast<NkImage*>(nkMalloc(sizeof(NkImage)));
        if (!img) return nullptr;
        new(img) NkImage(); // placement new : initialise les membres par défaut

        // Stride aligné sur 4 octets
        img->mStride = (w * bpp + 3) & ~3;

        // Alloue et zéro-init les pixels
        img->mPixels = static_cast<uint8*>(nkCalloc(usize(img->mStride) * h, 1));
        if (!img->mPixels) {
            nkFree(img);
            return nullptr;
        }

        img->mWidth  = w;
        img->mHeight = h;
        img->mFormat = fmt;
        img->mOwning = true;
        return img;
    }

    /**
     * Crée une vue non-owning sur un buffer pixel externe.
     *
     * L'image résultante n'est PAS owning : ni le destructeur ni Free()
     * ne libéreront les pixels. L'appelant reste responsable de la durée
     * de vie du buffer `pixels`.
     *
     * Utile pour wrapper un buffer GPU, un fichier mappé en mémoire, etc.
     *
     * @param pixels  Pointeur vers les données pixel.
     * @param w, h    Dimensions de l'image.
     * @param fmt     Format pixel des données.
     * @param stride  Stride en octets (0 = calculé automatiquement : w * bpp).
     */
    NkImage* NkImage::Wrap(uint8* pixels, int32 w, int32 h,
                            NkImagePixelFormat fmt, int32 stride) noexcept {
        NkImage* img = static_cast<NkImage*>(nkMalloc(sizeof(NkImage)));
        if (!img) return nullptr;
        new(img) NkImage();

        img->mPixels = pixels;
        img->mWidth  = w;
        img->mHeight = h;
        img->mFormat = fmt;
        img->mStride = (stride > 0) ? stride : w * BytesPerPixelOf(fmt);
        img->mOwning = false; // ne possède pas les pixels
        return img;
    }

    /**
     * Tone-mapping HDR → LDR.
     * Délègue vers NkHDRCodec::ConvertToTexture.
     *
     * @see NkImage::ConvertToTexture dans NkImage.h pour la documentation complète.
     */
    NkImage* NkImage::ConvertToTexture(const NkImage& hdrImage,
                                        float exposure, float gamma) noexcept {
        return NkHDRCodec::ConvertToTexture(hdrImage, exposure, gamma);
    }

    // =========================================================================
    //  §8  NkImage — cycle de vie (destructeur, move, Free)
    // =========================================================================

    /**
     * Destructeur.
     * Si mOwning==true, libère uniquement le buffer pixel (pas le struct lui-même).
     * Le struct est sur la stack (ou dans un autre objet) dans le cas de l'API
     * instance ; il sera détruit normalement par le compilateur.
     *
     * Pour les images allouées via les fabriques statiques (heap), il faut
     * appeler Free() qui libère AUSSI le struct via nkFree(this).
     */
    NkImage::~NkImage() noexcept {
        if (mOwning && mPixels) nkFree(mPixels);
        // mPixels intentionnellement non remis à nullptr : le destructeur n'est
        // appelé qu'une seule fois et l'objet n'est plus accessible après.
    }

    /**
     * Move constructor.
     * Transfère la propriété du buffer sans copie ni allocation.
     * Après le move, `other` est dans un état vide valide (IsValid()==false).
     */
    NkImage::NkImage(NkImage&& other) noexcept
        : mPixels(other.mPixels)
        , mWidth (other.mWidth )
        , mHeight(other.mHeight)
        , mStride(other.mStride)
        , mFormat(other.mFormat)
        , mSrcFmt(other.mSrcFmt)
        , mOwning(other.mOwning)
    {
        // Vide `other` pour éviter le double-free dans son destructeur
        other.mPixels = nullptr;
        other.mWidth  = 0;
        other.mHeight = 0;
        other.mOwning = false;
    }

    /**
     * Move assignment.
     * Libère l'éventuel buffer existant dans *this, puis transfère depuis `other`.
     * Self-assignment sécurisé (this == &other est testé).
     */
    NkImage& NkImage::operator=(NkImage&& other) noexcept {
        if (this == &other) return *this;

        // Libère l'ancien buffer
        if (mOwning && mPixels) nkFree(mPixels);

        // Transfert
        mPixels  = other.mPixels;
        mWidth   = other.mWidth;
        mHeight  = other.mHeight;
        mStride  = other.mStride;
        mFormat  = other.mFormat;
        mSrcFmt  = other.mSrcFmt;
        mOwning  = other.mOwning;

        // Vide `other`
        other.mPixels = nullptr;
        other.mWidth  = 0;
        other.mHeight = 0;
        other.mOwning = false;
        return *this;
    }

    /**
     * Libère les pixels (si owning) ET le struct NkImage lui-même.
     *
     * USAGE : uniquement sur les images créées via les fabriques statiques
     * (Alloc, Create, Copy, CopyAs, Convert, Resize, Crop, Wrap, …).
     * Ne JAMAIS appeler Free() sur une image allouée sur la stack.
     *
     * L'implémentation remet mPixels à nullptr avant de libérer le struct
     * pour éviter tout accès invalide si le compilateur génère du code après
     * le nkFree(this) (peu probable mais sécurisé).
     */
    void NkImage::Free() noexcept {
        if (mOwning && mPixels) nkFree(mPixels);
        mPixels = nullptr;
        nkFree(this); // libère le struct NkImage lui-même
    }

    // -------------------------------------------------------------------------
    // [NKIResource] Unload — libère les pixels et remet *this à l'état vide,
    // SANS libérer le struct (contrairement à Free). Sûr sur pile comme heap.
    // -------------------------------------------------------------------------
    void NkImage::Unload() noexcept {
        if (mOwning && mPixels) nkFree(mPixels);
        mPixels = nullptr;
        mWidth  = 0;
        mHeight = 0;
        mStride = 0;
        mFormat = NkImagePixelFormat::NK_UNKNOWN;
        mSrcFmt = NkImageFormat::NK_UNKNOWN;
        mOwning = true;
    }

    // -------------------------------------------------------------------------
    // [NKIResource] LoadFromStream — lit l'intégralité du flux dans un buffer
    // temporaire puis décode via LoadFromMemory (détection auto du format).
    // -------------------------------------------------------------------------
    bool NkImage::LoadFromStream(NkStream& stream) {
        const usize size = stream.Size();
        if (size == 0) return false;

        uint8* buf = static_cast<uint8*>(nkMalloc(size));
        if (!buf) return false;

        stream.Seek(0);
        const usize read = stream.ReadRaw(buf, size);
        const bool  ok   = (read >= 4) && LoadFromMemory(buf, read, 0);

        nkFree(buf);
        return ok;
    }

    // -------------------------------------------------------------------------
    // [NKIResource] SaveToStream — encode en PNG (lossless universel) et écrit
    // le résultat dans le flux. Le buffer PNG (NkAlloc) est libéré via nkFree.
    // -------------------------------------------------------------------------
    bool NkImage::SaveToStream(NkStream& stream) const {
        if (!IsValid()) return false;

        uint8* png  = nullptr;
        usize  size = 0;
        if (!EncodePNG(png, size) || !png) return false;

        const usize written = stream.WriteRaw(png, size);
        nkFree(png); // EncodePNG alloue via NkAlloc → libération NKMemory
        return written == size;
    }

    // =========================================================================
    //  §9  NkImage — détection de format (magic bytes)
    // =========================================================================

    /**
     * Détecte le format d'un fichier image depuis ses magic bytes.
     *
     * Chaque format image commence par une signature binaire unique :
     *   PNG  : 89 50 4E 47  (‰PNG)
     *   JPEG : FF D8 FF
     *   BMP  : 42 4D       (BM)
     *   QOI  : 71 6F 69 66  (qoif)
     *   GIF  : 47 49 46 38  (GIF8)
     *   ICO  : 00 00 01/02 00
     *   HDR  : 23 3F         (#?)
     *   EXR  : 76 2F 31 01   (little-endian 0x01312F76)
     *   PPM/PGM/PBM : P1..P6
     *   TGA  : détection heuristique (champ image type octet 2)
     *   SVG  : balise <?xml ou <svg dans les 256 premiers octets
     *
     * @param data  Début du buffer image.
     * @param size  Taille totale du buffer.
     * @return Format détecté, ou NK_UNKNOWN si non reconnu.
     */
    NkImageFormat NkImage::DetectFormat(const uint8* data, usize size) noexcept {
        if (size < 4) return NkImageFormat::NK_UNKNOWN;

        // ── PNG : 89 50 4E 47 0D 0A 1A 0A ────────────────────────────────────
        if (size >= 8 &&
            data[0] == 0x89 && data[1] == 'P' &&
            data[2] == 'N'  && data[3] == 'G')
            return NkImageFormat::NK_PNG;

        // ── JPEG : commence par FF D8 FF ──────────────────────────────────────
        if (size >= 3 &&
            data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF)
            return NkImageFormat::NK_JPEG;

        // ── BMP : "BM" ────────────────────────────────────────────────────────
        if (size >= 2 && data[0] == 'B' && data[1] == 'M')
            return NkImageFormat::NK_BMP;

        // ── QOI : "qoif" ──────────────────────────────────────────────────────
        if (size >= 4 &&
            data[0] == 'q' && data[1] == 'o' &&
            data[2] == 'i' && data[3] == 'f')
            return NkImageFormat::NK_QOI;

        // ── GIF : "GIF8" (GIF87a ou GIF89a) ──────────────────────────────────
        if (size >= 4 &&
            data[0] == 'G' && data[1] == 'I' &&
            data[2] == 'F' && data[3] == '8')
            return NkImageFormat::NK_GIF;

        // ── ICO/CUR : 00 00 01/02 00 ──────────────────────────────────────────
        if (size >= 4 &&
            data[0] == 0 && data[1] == 0 &&
            (data[2] == 1 || data[2] == 2) && data[3] == 0)
            return NkImageFormat::NK_ICO;

        // ── HDR (Radiance RGBE) : commence par "#?" ───────────────────────────
        if (size >= 10 && data[0] == '#' && data[1] == '?')
            return NkImageFormat::NK_HDR;

        // ── OpenEXR : magic 0x01312F76 (little-endian : 76 2F 31 01) ─────────
        if (size >= 4 &&
            data[0] == 0x76 && data[1] == 0x2F &&
            data[2] == 0x31 && data[3] == 0x01)
            return NkImageFormat::NK_EXR;

        // ── PBM/PGM/PPM : commence par "P" suivi d'un chiffre 1..6 ────────────
        if (size >= 2 && data[0] == 'P' && data[1] >= '1' && data[1] <= '6') {
            if (data[1] == '1' || data[1] == '4') return NkImageFormat::NK_PBM;
            if (data[1] == '2' || data[1] == '5') return NkImageFormat::NK_PGM;
            return NkImageFormat::NK_PPM; // P3 ou P6
        }

        // ── TGA : détection heuristique via le champ "image type" (octet 2) ───
        // Les valeurs valides connues : 0 (aucune image), 1 (colormap),
        // 2 (true-color), 3 (grayscale), 9/10/11 (variantes RLE).
        // Note : TGA n'a pas de magic bytes, cette détection peut avoir des
        // faux positifs, mais c'est la convention standard.
        if (size >= 18) {
            uint8 it = data[2];
            if (it == 0 || it == 1 || it == 2 || it == 3 ||
                it == 9 || it == 10|| it == 11)
                return NkImageFormat::NK_TGA;
        }

        // ── SVG : fichier XML contenant la balise <svg ─────────────────────────
        // Tolère un BOM UTF-8 (EF BB BF) et des espaces blancs avant la balise.
        // On cherche "<?xml" ou "<svg" dans les ~256 premiers octets.
        {
            usize off = 0;
            // Saute l'éventuel BOM UTF-8
            if (size >= 3 &&
                data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF)
                off = 3;

            // Saute les espaces blancs
            while (off < size &&
                   (data[off] == ' '  || data[off] == '\t' ||
                    data[off] == '\n' || data[off] == '\r'))
                ++off;

            // Limite la recherche aux 256 premiers octets utiles
            const usize end = (size < off + 256) ? size : (off + 256);

            // Helper lambda : comparaison insensible à la casse du littéral
            auto match = [&](const char* lit, usize litLen) -> bool {
                if (off + litLen > size) return false;
                for (usize k = 0; k < litLen; ++k)
                    if (data[off + k] != (uint8)lit[k]) return false;
                return true;
            };

            // Teste "<?xml" et "<svg" au début (après BOM/espaces)
            if (match("<?xml", 5) || match("<svg", 4))
                return NkImageFormat::NK_SVG;

            // Scan plus large : l'en-tête XML peut contenir un DOCTYPE ou
            // des commentaires avant le tag <svg.
            for (usize i = off; i + 4 < end; ++i) {
                if (data[i]   == '<' && data[i+1] == 's' &&
                    data[i+2] == 'v' && data[i+3] == 'g' &&
                    (data[i+4] == ' '  || data[i+4] == '\t' ||
                     data[i+4] == '\n' || data[i+4] == '>'))
                    return NkImageFormat::NK_SVG;
            }
        }

        return NkImageFormat::NK_UNKNOWN;
    }

    // =========================================================================
    //  §10  NkImage — dispatch vers les codecs
    // =========================================================================

    /**
     * Dispatch vers le codec approprié selon le format détecté, puis conversion
     * de canaux si `desired` > 0 et différent du format natif.
     *
     * @param data    Buffer encodé.
     * @param size    Taille du buffer.
     * @param desired Nombre de canaux souhaités (0 = natif).
     * @param fmt     Format identifié par DetectFormat().
     * @return Nouvelle image décodée (owning), ou nullptr en cas d'échec.
     */
    NkImage* NkImage::Dispatch(const uint8* data, usize size,
                                int32 desired, NkImageFormat fmt) noexcept {
        NkImage* img = nullptr;

        // Sélectionne le codec selon le format de fichier
        switch (fmt) {
            case NkImageFormat::NK_PNG:  img = NkPNGCodec::Decode(data, size);  break;
            case NkImageFormat::NK_JPEG: img = NkJPEGCodec::Decode(data, size); break;
            case NkImageFormat::NK_BMP:  img = NkBMPCodec::Decode(data, size);  break;
            case NkImageFormat::NK_TGA:  img = NkTGACodec::Decode(data, size);  break;
            case NkImageFormat::NK_HDR:  img = NkHDRCodec::Decode(data, size);  break;
            case NkImageFormat::NK_EXR:  img = NkEXRCodec::Decode(data, size);  break;
            // PPM/PGM/PBM : même codec (NetPBM)
            case NkImageFormat::NK_PPM:
            case NkImageFormat::NK_PGM:
            case NkImageFormat::NK_PBM:  img = NkPPMCodec::Decode(data, size);  break;
            case NkImageFormat::NK_QOI:  img = NkQOICodec::Decode(data, size);  break;
            case NkImageFormat::NK_GIF:  img = NkGIFCodec::Decode(data, size);  break;
            case NkImageFormat::NK_ICO:  img = NkICOCodec::Decode(data, size);  break;
            case NkImageFormat::NK_SVG:  img = NkSVGCodec::Decode(data, size);  break;
            default: return nullptr;
        }

        if (!img) return nullptr;

        // Conversion de canaux si demandée et différente du décodage natif
        if (desired > 0 && desired != img->Channels()) {
            NkImagePixelFormat tgt =
                desired == 1 ? NkImagePixelFormat::NK_GRAY8    :
                desired == 2 ? NkImagePixelFormat::NK_GRAY_A16 :
                desired == 3 ? NkImagePixelFormat::NK_RGB24    :
                               NkImagePixelFormat::NK_RGBA32;
            NkImage* converted = img->Convert(tgt);
            img->Free(); // libère l'image intermédiaire
            return converted;
        }
        return img;
    }

    // =========================================================================
    //  §12  NkImage — API INSTANCE : Create / Load
    //       Ces méthodes opèrent sur *this, libèrent l'ancien buffer si besoin,
    //       et retournent bool. Pensées pour une utilisation en valeur :
    //         NkImage img; img.Load("foo.png");
    // =========================================================================

    /**
     * (API instance) Crée une image dans *this.
     * Libère l'éventuel buffer précédent avant d'allouer le nouveau.
     *
     * Délègue vers la fabrique statique Create(width, height, desiredChannels, color)
     * puis "vole" le buffer alloué (transfère l'ownership).
     */
    bool NkImage::Create(uint32 width, uint32 height,
                          math::NkColor color,
                          int32 desiredChannels) noexcept {
        // Libère l'ancien buffer si on est owning
        if (mOwning && mPixels) {
            nkFree(mPixels);
            mPixels = nullptr;
        }

        // Crée l'image via la fabrique statique
        NkImage* tmp = NkImage::Create(width, height, desiredChannels, color.ToUint32());
        if (!tmp) return false;

        // Vole le buffer de tmp (tmp n'est plus owning)
        mPixels  = tmp->mPixels;
        mWidth   = tmp->mWidth;
        mHeight  = tmp->mHeight;
        mStride  = tmp->mStride;
        mFormat  = tmp->mFormat;
        mSrcFmt  = tmp->mSrcFmt;
        mOwning  = true;

        // Empêche tmp de libérer les pixels qu'on vient de voler
        tmp->mOwning = false;
        tmp->Free(); // libère uniquement le struct
        return true;
    }

    /**
     * (API instance) Charge une image depuis un fichier dans *this.
     * Libère l'éventuel buffer précédent, délègue vers LoadFromMemoryImpl.
     */
    bool NkImage::Load(const char* path, int32 desiredChannels) noexcept {
        if (!path) return false;

        NkFile file(path, NkFileMode::NK_READ_BINARY);
        if (!file.IsOpen()) return false;

        const nk_int64 sz = file.GetSize();
        if (sz <= 0) return false;

        uint8* buf = static_cast<uint8*>(nkMalloc(usize(sz)));
        if (!buf) return false;

        const usize rd = file.Read(buf, usize(sz));
        bool ok = false;
        if (rd == usize(sz)) {
            ok = LoadFromMemoryImpl(buf, rd, desiredChannels);
        }
        nkFree(buf);
        return ok;
    }

    /**
     * (API instance) Charge depuis un buffer void*.
     * Surcharge pratique pour les APIs C (ex: fread produit des void*).
     * Délègue vers la surcharge uint8* après un cast.
     */
    bool NkImage::LoadFromMemory(const void* data, usize size,
                                  int32 desiredChannels) noexcept {
        if (!data) return false;
        return LoadFromMemory(static_cast<const uint8*>(data), size, desiredChannels);
    }

    /**
     * (API instance) Charge depuis un buffer uint8*.
     * C'est l'implémentation réelle ; délègue vers LoadFromMemoryImpl.
     */
    bool NkImage::LoadFromMemory(const uint8* data, usize size,
                                  int32 desiredChannels) noexcept {
        if (!data || size < 4) return false;
        return LoadFromMemoryImpl(data, size, desiredChannels);
    }

    /**
     * Helper interne partagé par Load() et LoadFromMemory() (API instance).
     *
     * Libère l'ancien buffer, détecte le format, décode, et transfère le résultat
     * dans *this en volant le buffer de l'image temporaire.
     */
    bool NkImage::LoadFromMemoryImpl(const uint8* data, usize size,
                                      int32 desiredChannels) noexcept {
        // Détection et décodage via la voie statique
        NkImageFormat fmt = DetectFormat(data, size);
        if (fmt == NkImageFormat::NK_UNKNOWN) return false;

        NkImage* tmp = Dispatch(data, size, desiredChannels, fmt);
        if (!tmp) return false;

        // Libère l'ancien buffer de *this avant le transfert
        if (mOwning && mPixels) nkFree(mPixels);

        // Vole le buffer de tmp
        mPixels  = tmp->mPixels;
        mWidth   = tmp->mWidth;
        mHeight  = tmp->mHeight;
        mStride  = tmp->mStride;
        mFormat  = tmp->mFormat;
        mSrcFmt  = fmt; // fmt détecté localement, pas celui de tmp
        mOwning  = true;

        tmp->mOwning = false;
        tmp->Free();
        return true;
    }

    // =========================================================================
    //  §13  NkImage — fabriques statiques Create
    // =========================================================================

    /**
     * Crée une image heap-allouée avec format pixel explicite.
     * C'est le cœur de la logique : la surcharge par desiredChannels délègue ici.
     *
     * @param width, height  Dimensions (> 0).
     * @param fmt            Format pixel cible.
     * @param color          RGBA packed big-endian 0xRRGGBBAA (0 = transparent black).
     */
    NkImage* NkImage::Create(uint32 width, uint32 height,
                              NkImagePixelFormat fmt,
                              uint32 color) noexcept {
        if (width == 0 || height == 0) return nullptr;
        if (fmt == NkImagePixelFormat::NK_UNKNOWN) return nullptr;

        NkImage* img = Alloc(int32(width), int32(height), fmt);
        if (!img) return nullptr;

        // color == 0 (transparent black) → Alloc a déjà zerofill, pas de travail
        if (color == 0) return img;

        // Décompose la couleur RGBA packed big-endian (convention 0xRRGGBBAA)
        const uint8 cr = uint8((color >> 24) & 0xFF); // R
        const uint8 cg = uint8((color >> 16) & 0xFF); // G
        const uint8 cb = uint8((color >>  8) & 0xFF); // B
        const uint8 ca = uint8( color        & 0xFF); // A

        const int32 bpp = img->BytesPP();

        // Remplit chaque pixel selon le format
        for (int32 y = 0; y < img->mHeight; ++y) {
            uint8* row = img->RowPtr(y);
            for (int32 x = 0; x < img->mWidth; ++x) {
                uint8* p = row + x * bpp;
                switch (fmt) {
                    case NkImagePixelFormat::NK_GRAY8:
                        // Luminance perceptuelle (pas de canal alpha)
                        p[0] = nkY(cr, cg, cb);
                        break;

                    case NkImagePixelFormat::NK_GRAY_A16:
                        p[0] = nkY(cr, cg, cb);
                        p[1] = ca;
                        break;

                    case NkImagePixelFormat::NK_RGB24:
                        p[0] = cr; p[1] = cg; p[2] = cb;
                        break;

                    case NkImagePixelFormat::NK_RGBA32:
                        p[0] = cr; p[1] = cg; p[2] = cb; p[3] = ca;
                        break;

                    case NkImagePixelFormat::NK_RGB96F: {
                        // Encode la couleur en flottant normalisé [0.0, 1.0]
                        // L'alpha est ignoré (format sans canal alpha)
                        float* fp = reinterpret_cast<float*>(p);
                        fp[0] = cr / 255.0f;
                        fp[1] = cg / 255.0f;
                        fp[2] = cb / 255.0f;
                        break;
                    }
                    case NkImagePixelFormat::NK_RGBA128F: {
                        float* fp = reinterpret_cast<float*>(p);
                        fp[0] = cr / 255.0f;
                        fp[1] = cg / 255.0f;
                        fp[2] = cb / 255.0f;
                        fp[3] = ca / 255.0f;
                        break;
                    }
                    default: break;
                }
            }
        }
        return img;
    }

    /**
     * Crée une image heap-allouée avec desiredChannels.
     * Mappe le nombre de canaux vers un NkImagePixelFormat puis délègue
     * vers Create(width, height, fmt, color).
     */
    NkImage* NkImage::Create(uint32 width, uint32 height,
                              int32 desiredChannels,
                              uint32 color) noexcept {
        NkImagePixelFormat fmt;
        switch (desiredChannels) {
            case 1:  fmt = NkImagePixelFormat::NK_GRAY8;    break;
            case 2:  fmt = NkImagePixelFormat::NK_GRAY_A16; break;
            case 3:  fmt = NkImagePixelFormat::NK_RGB24;    break;
            case 0:  // 0 → RGBA32 par défaut (comportement le plus sûr)
            case 4:  fmt = NkImagePixelFormat::NK_RGBA32;   break;
            default: return nullptr; // valeur invalide → échec explicite
        }
        return Create(width, height, fmt, color);
    }

    // =========================================================================
    //  §14  NkImage — sauvegarde sur disque
    // =========================================================================

    /**
     * Helper : écrit un buffer en mémoire dans un fichier binaire.
     * Utilise fopen/fwrite/fclose standard (pas NkFile, pour éviter les
     * dépendances circulaires dans les chemins de sauvegarde).
     *
     * @param path  Chemin du fichier destination.
     * @param data  Buffer à écrire.
     * @param size  Taille du buffer.
     * @return true si tous les octets ont été écrits.
     */
    static bool nkWF(const char* path, const uint8* data, usize size) noexcept {
        FILE* f = ::fopen(path, "wb");
        if (!f) return false;
        bool ok = (::fwrite(data, 1, size, f) == size);
        ::fclose(f);
        return ok;
    }

    /**
     * Extrait l'extension d'un chemin (sans le point).
     * Retourne un pointeur vers le dernier "." dans le chemin,
     * ou vers la fin de la chaîne si aucun "." n'est trouvé.
     * Exemple : "photo.jpg" → "jpg"
     */
    static const char* nkExt(const char* path) noexcept {
        const char* ext = nullptr;
        for (const char* p = path; *p; ++p) {
            if (*p == '.') ext = p;
        }
        return ext ? ext + 1 : "";
    }

    /**
     * Comparaison insensible à la casse de deux chaînes ASCII.
     * Utilise (c | 32) pour normaliser les majuscules en minuscules.
     */
    static bool nkEq(const char* a, const char* b) noexcept {
        while (*a && *b) {
            if (((*a) | 32) != ((*b) | 32)) return false;
            ++a; ++b;
        }
        return *a == *b;
    }

    /**
     * Sauvegarde dans le format déduit de l'extension du chemin.
     * Extensions reconnues : png, jpg/jpeg, bmp, tga, ppm/pgm, hdr, qoi.
     *
     * @param quality  Qualité JPEG [1–100], ignoré pour les autres formats.
     */
    bool NkImage::Save(const char* path, int32 quality) const noexcept {
        const char* ext = nkExt(path);
        if (nkEq(ext, "png"))                  return SavePNG(path);
        if (nkEq(ext, "jpg") || nkEq(ext, "jpeg")) return SaveJPEG(path, quality);
        if (nkEq(ext, "bmp"))                  return SaveBMP(path);
        if (nkEq(ext, "tga"))                  return SaveTGA(path);
        if (nkEq(ext, "ppm") || nkEq(ext, "pgm")) return SavePPM(path);
        if (nkEq(ext, "hdr"))                  return SaveHDR(path);
        if (nkEq(ext, "qoi"))                  return SaveQOI(path);
        return false; // extension non reconnue
    }

    /** Encode en PNG et écrit sur disque. */
    bool NkImage::SavePNG(const char* path) const noexcept {
        uint8* data = nullptr; usize size = 0;
        if (!EncodePNG(data, size)) return false;
        bool ok = nkWF(path, data, size);
        nkFree(data);
        return ok;
    }

    /** Encode en JPEG et écrit sur disque. */
    bool NkImage::SaveJPEG(const char* path, int32 quality) const noexcept {
        uint8* data = nullptr; usize size = 0;
        if (!EncodeJPEG(data, size, quality)) return false;
        bool ok = nkWF(path, data, size);
        nkFree(data);
        return ok;
    }

    /** Encode en BMP et écrit sur disque. */
    bool NkImage::SaveBMP(const char* path) const noexcept {
        uint8* data = nullptr; usize size = 0;
        if (!EncodeBMP(data, size)) return false;
        bool ok = nkWF(path, data, size);
        nkFree(data);
        return ok;
    }

    /** Encode en TGA et écrit sur disque. */
    bool NkImage::SaveTGA(const char* path) const noexcept {
        uint8* data = nullptr; usize size = 0;
        if (!EncodeTGA(data, size)) return false;
        bool ok = nkWF(path, data, size);
        nkFree(data);
        return ok;
    }

    /** Délègue l'encodage PPM directement au codec (écriture en texte). */
    bool NkImage::SavePPM(const char* path) const noexcept {
        return NkPPMCodec::Encode(*this, path);
    }

    /** Délègue l'encodage HDR directement au codec. */
    bool NkImage::SaveHDR(const char* path) const noexcept {
        return NkHDRCodec::Encode(*this, path);
    }

    /** Encode en QOI et écrit sur disque. */
    bool NkImage::SaveQOI(const char* path) const noexcept {
        uint8* data = nullptr; usize size = 0;
        if (!EncodeQOI(data, size)) return false;
        bool ok = nkWF(path, data, size);
        nkFree(data);
        return ok;
    }

    /**
     * SaveGIF — non implémenté.
     * GIF est un format complexe (LZW, palette, animation) ; l'encodeur
     * n'a pas été intégré dans cette version.
     */
    bool NkImage::SaveGIF(const char*) const noexcept {
        return false;
    }

    /**
     * SaveWebP — non implémenté.
     * Nécessiterait libwebp ou une implémentation custom VP8/VP8L.
     */
    bool NkImage::SaveWebP(const char*, bool, int32) const noexcept {
        return false;
    }

    /**
     * SaveSVG — non implémenté.
     * L'encodage raster → SVG (vectorisation) n'est pas dans le scope.
     */
    bool NkImage::SaveSVG(const char*) const noexcept {
        return false;
    }

    // =========================================================================
    //  §15  NkImage — encodage en mémoire
    //       Les buffers produits par ces méthodes DOIVENT être libérés avec
    //       nkentseu::memory::NkFree(ptr). Ne pas utiliser std::free.
    // =========================================================================

    /** Encode l'image en PNG dans un buffer heap. */
    bool NkImage::EncodePNG(uint8*& out, usize& size) const noexcept {
        return NkPNGCodec::Encode(*this, out, size);
    }

    /** Encode l'image en JPEG dans un buffer heap. */
    bool NkImage::EncodeJPEG(uint8*& out, usize& size, int32 quality) const noexcept {
        return NkJPEGCodec::Encode(*this, out, size, quality);
    }

    /** Encode l'image en BMP dans un buffer heap. */
    bool NkImage::EncodeBMP(uint8*& out, usize& size) const noexcept {
        return NkBMPCodec::Encode(*this, out, size);
    }

    /** Encode l'image en TGA dans un buffer heap. */
    bool NkImage::EncodeTGA(uint8*& out, usize& size) const noexcept {
        return NkTGACodec::Encode(*this, out, size);
    }

    /** Encode l'image en QOI dans un buffer heap. */
    bool NkImage::EncodeQOI(uint8*& out, usize& size) const noexcept {
        return NkQOICodec::Encode(*this, out, size);
    }

    // =========================================================================
    //  §16  NkImage — conversion de canaux (interne)
    // =========================================================================

    /**
     * Conversion bas niveau de canaux pour des données pixel brutes 8 bits.
     *
     * Gère toutes les combinaisons via la luminance perceptuelle nkY() :
     *   - 1→1 : identité
     *   - 1→2 : Y + alpha=255
     *   - 1→3 : Y,Y,Y (gris → RGB)
     *   - 1→4 : Y,Y,Y,255
     *   - 3→1 : Y = luma(R,G,B)
     *   - 3→4 : R,G,B + alpha=255
     *   - 4→1 : Y = luma(R,G,B) (alpha ignoré)
     *   - 4→3 : R,G,B (alpha ignoré)
     *   - etc.
     *
     * Le buffer de destination est aligné sur 4 octets par ligne :
     *   dstStride = (w * dstChannels + 3) & ~3
     *
     * @param src       Buffer source (données pixel brutes).
     * @param w, h      Dimensions de l'image.
     * @param srcCh     Nombre de canaux sources.
     * @param dstCh     Nombre de canaux cibles.
     * @param srcStride Stride du buffer source en octets.
     * @return Buffer destination alloué via nkCalloc, ou nullptr si OOM.
     *
     * @note Uniquement pour les formats LDR 8 bits/canal.
     *       Les conversions HDR sont gérées dans Convert().
     */
    uint8* NkImage::ConvertChannels(const uint8* src, int32 w, int32 h,
                                     int32 srcCh, int32 dstCh,
                                     int32 srcStride) noexcept {
        const int32 dstStride = (w * dstCh + 3) & ~3;
        uint8* dst = static_cast<uint8*>(nkCalloc(usize(dstStride) * h, 1));
        if (!dst) return nullptr;

        for (int32 y = 0; y < h; ++y) {
            const uint8* srcRow = src + usize(y) * srcStride;
            uint8*       dstRow = dst + usize(y) * dstStride;

            for (int32 x = 0; x < w; ++x) {
                const uint8* sp = srcRow + x * srcCh;
                uint8*       dp = dstRow + x * dstCh;

                // Extraction des composantes sources avec repli gracieux
                uint8 r = sp[0];
                uint8 g = (srcCh > 1) ? sp[1] : r;  // grayscale → répète R
                uint8 b = (srcCh > 2) ? sp[2] : r;
                uint8 a = (srcCh > 3) ? sp[3] : 255; // alpha par défaut = opaque

                // Remplissage destination
                switch (dstCh) {
                    case 1:
                        dp[0] = nkY(r, g, b);
                        break;
                    case 2:
                        dp[0] = nkY(r, g, b);
                        dp[1] = a;
                        break;
                    case 3:
                        dp[0] = r; dp[1] = g; dp[2] = b;
                        break;
                    case 4:
                        dp[0] = r; dp[1] = g; dp[2] = b; dp[3] = a;
                        break;
                    default: break;
                }
            }
        }
        return dst;
    }

    // =========================================================================
    //  §17  NkImage — conversion de format (Convert)
    // =========================================================================

    /**
     * Convertit l'image vers un nouveau format pixel `nf`.
     *
     * Si nf == mFormat : clone pur (copie mémoire directe, pas de conversion).
     *
     * Conversions supportées :
     *   - LDR ↔ LDR : via ConvertChannels (luminance perceptuelle).
     *   - HDR → LDR  : via ConvertChannels (troncature des flottants sur uint8).
     *   - LDR → HDR  : via ConvertChannels (normalisation uint8 → float).
     *
     * Note : pour un tone-mapping HDR → LDR de qualité, utiliser
     * ConvertToTexture() qui applique exposure et gamma correction.
     *
     * @return Nouvelle image owning au format `nf`, ou nullptr si échec.
     */
    NkImage* NkImage::Convert(NkImagePixelFormat nf) const noexcept {
        if (!IsValid()) return nullptr;

        // Même format : clone direct sans passer par ConvertChannels
        if (nf == mFormat) {
            NkImage* clone = Alloc(mWidth, mHeight, mFormat);
            if (clone) {
                // Copie la totalité du buffer (stride * height)
                nkMemcpy(clone->mPixels, mPixels, TotalBytes());
                clone->mSrcFmt = mSrcFmt;
            }
            return clone;
        }

        // Nombre de canaux destination
        const int32 dstCh = ChannelsOf(nf);
        if (!dstCh) return nullptr;

        // Conversion bas niveau des pixels
        uint8* pix = ConvertChannels(mPixels, mWidth, mHeight,
                                      Channels(), dstCh, mStride);
        if (!pix) return nullptr;

        // Construit la nouvelle image autour du buffer converti
        NkImage* img = static_cast<NkImage*>(nkMalloc(sizeof(NkImage)));
        if (!img) {
            nkFree(pix);
            return nullptr;
        }
        new(img) NkImage();
        img->mPixels = pix;
        img->mWidth  = mWidth;
        img->mHeight = mHeight;
        img->mFormat = nf;
        img->mStride = (mWidth * BytesPerPixelOf(nf) + 3) & ~3;
        img->mOwning = true;
        img->mSrcFmt = mSrcFmt;
        return img;
    }

    // =========================================================================
    //  §18  NkImage — copies (Copy, CopyTo, CopyAs)
    // =========================================================================

    /**
     * Clone profond (API statique).
     * Retourne une nouvelle image heap-allouée avec les mêmes pixels et format.
     * Copie ligne par ligne (respecte le stride).
     *
     * @return Nouvelle image owning, ou nullptr si *this invalide ou OOM.
     */
    NkImage* NkImage::Copy() const noexcept {
        if (!IsValid()) return nullptr;

        NkImage* dst = Alloc(mWidth, mHeight, mFormat);
        if (!dst) return nullptr;

        // Copie ligne par ligne : Alloc peut calculer un stride différent si
        // l'alignement change, mais pour les mêmes dimensions/format le stride
        // est identique. On copie mWidth*bpp octets utiles (pas le padding).
        const int32 rowBytes = mWidth * BytesPP();
        for (int32 y = 0; y < mHeight; ++y) {
            nkMemcpy(dst->RowPtr(y), RowPtr(y), rowBytes);
        }
        dst->mSrcFmt = mSrcFmt;
        return dst;
    }

    /**
     * Copie *this dans une image existante `dst` (API instance).
     * Aucune allocation. Exige même format ET mêmes dimensions.
     *
     * @return false si les images sont incompatibles, sans modifier `dst`.
     */
    bool NkImage::CopyTo(NkImage& dst) const noexcept {
        if (!IsValid() || !dst.IsValid())          return false;
        if (mFormat  != dst.mFormat)               return false;
        if (mWidth   != dst.mWidth ||
            mHeight  != dst.mHeight)               return false;

        const int32 rowBytes = mWidth * BytesPP();
        for (int32 y = 0; y < mHeight; ++y) {
            nkMemcpy(dst.RowPtr(y), RowPtr(y), rowBytes);
        }
        dst.mSrcFmt = mSrcFmt;
        return true;
    }

    /**
     * Clone avec conversion de format (API statique).
     * Si fmt == mFormat → Copy() direct (pas de conversion inutile).
     * Sinon → Convert() gère les cas LDR↔LDR et HDR↔LDR.
     *
     * @return Nouvelle image owning au format `fmt`, ou nullptr si invalide/inconnu.
     */
    NkImage* NkImage::CopyAs(NkImagePixelFormat fmt) const noexcept {
        if (!IsValid()) return nullptr;
        if (fmt == NkImagePixelFormat::NK_UNKNOWN) return nullptr;

        // Même format → clone direct, plus rapide que passer par Convert()
        if (fmt == mFormat) return Copy();

        // Format différent → Convert() gère toutes les conversions
        NkImage* dst = Convert(fmt);
        if (dst) dst->mSrcFmt = mSrcFmt;
        return dst;
    }

    /**
     * Copie une région de `src` dans *this à (dstX, dstY) (API instance).
     *
     * Résolution de la région source :
     *   - area (0,0,0,0) → image entière.
     *   - area.width/height > 0 → sous-région de src.
     *
     * Modes de débordement :
     *   - clip=true  : ajuste silencieusement les coordonnées pour rester
     *                  dans les bornes (comportement défensif par défaut).
     *   - clip=false : retourne false si quoi que ce soit dépasse les bornes.
     *
     * @param src   Image source (même format que *this).
     * @param dstX  Colonne de destination dans *this.
     * @param dstY  Ligne de destination dans *this.
     * @param area  Région source à copier (left, top, width, height).
     * @param clip  Mode de gestion des débordements.
     */
    bool NkImage::Copy(const NkImage& src, int32 dstX, int32 dstY,
                       const math::NkIntRect& area, bool clip) noexcept {
        if (!IsValid() || !src.IsValid()) return false;
        if (mFormat != src.mFormat)       return false;

        // Résolution de la région source (0,0,0,0 = image entière)
        int32 srcX = area.left;
        int32 srcY = area.top;
        int32 srcW = (area.width  > 0) ? area.width  : src.mWidth;
        int32 srcH = (area.height > 0) ? area.height : src.mHeight;

        if (!clip) {
            // Mode strict : tout doit être dans les bornes
            if (srcX < 0 || srcY < 0 ||
                srcX + srcW > src.mWidth  ||
                srcY + srcH > src.mHeight) return false;
            if (dstX < 0 || dstY < 0 ||
                dstX + srcW > mWidth  ||
                dstY + srcH > mHeight)     return false;
        }

        // Clipping de la région source et destination
        // — bord gauche source
        if (srcX < 0)                   { dstX -= srcX; srcW += srcX; srcX = 0; }
        // — bord haut source
        if (srcY < 0)                   { dstY -= srcY; srcH += srcY; srcY = 0; }
        // — bord droit source
        if (srcX + srcW > src.mWidth)   srcW = src.mWidth  - srcX;
        // — bord bas source
        if (srcY + srcH > src.mHeight)  srcH = src.mHeight - srcY;
        // — bord gauche destination
        if (dstX < 0)                   { srcX -= dstX; srcW += dstX; dstX = 0; }
        // — bord haut destination
        if (dstY < 0)                   { srcY -= dstY; srcH += dstY; dstY = 0; }
        // — bord droit destination
        if (dstX + srcW > mWidth)       srcW = mWidth  - dstX;
        // — bord bas destination
        if (dstY + srcH > mHeight)      srcH = mHeight - dstY;

        // Après clipping il peut ne rien rester (pas une erreur)
        if (srcW <= 0 || srcH <= 0) return true;

        const int32 bpp      = BytesPP();
        const int32 rowBytes = srcW * bpp;

        for (int32 row = 0; row < srcH; ++row) {
            const uint8* srcPtr = src.RowPtr(srcY + row) + srcX * bpp;
            uint8*       dstPtr =     RowPtr(dstY + row) + dstX * bpp;
            nkMemcpy(dstPtr, srcPtr, rowBytes);
        }
        return true;
    }

    // =========================================================================
    //  §19  NkImage — manipulation in-place
    // =========================================================================

    /**
     * Retourne l'image verticalement (flip autour de l'axe horizontal).
     * La ligne 0 devient la dernière ligne, et vice-versa.
     * Utile pour réconcilier les conventions Y-up (OpenGL) et Y-down (DIBs).
     * Alloue temporairement un buffer d'une ligne (mStride octets).
     */
    void NkImage::FlipVertical() noexcept {
        if (!IsValid()) return;

        uint8* tmp = static_cast<uint8*>(nkMalloc(mStride));
        if (!tmp) return; // OOM : on ne peut pas flip sans buffer temporaire

        for (int32 y = 0; y < mHeight / 2; ++y) {
            // Échange la ligne y avec la ligne symétrique
            nkMemcpy(tmp,                         RowPtr(y),               mStride);
            nkMemcpy(RowPtr(y),                   RowPtr(mHeight - 1 - y), mStride);
            nkMemcpy(RowPtr(mHeight - 1 - y),     tmp,                     mStride);
        }
        nkFree(tmp);
    }

    /**
     * Retourne l'image horizontalement (flip autour de l'axe vertical).
     * Le pixel (x, y) est échangé avec le pixel (width-1-x, y).
     * Opère en place, sans allocation supplémentaire.
     */
    void NkImage::FlipHorizontal() noexcept {
        if (!IsValid()) return;
        const int32 bpp = BytesPP();

        for (int32 y = 0; y < mHeight; ++y) {
            uint8* row = RowPtr(y);
            for (int32 x = 0; x < mWidth / 2; ++x) {
                uint8* a = row + x                 * bpp;
                uint8* b = row + (mWidth - 1 - x)  * bpp;
                // Échange les bpp octets des deux pixels
                for (int32 i = 0; i < bpp; ++i) {
                    uint8 t = a[i]; a[i] = b[i]; b[i] = t;
                }
            }
        }
    }

    /**
     * Pré-multiplie les canaux RGB par l'alpha (RGBA32 uniquement).
     *
     * Pré-multiplication : R' = R*A/255, G' = G*A/255, B' = B*A/255
     * Opération destructrice (l'original ne peut pas être reconstruit exactement).
     *
     * Utilité : certains pipelines GPU (Porter-Duff "over") nécessitent des
     * textures pré-multipliées pour un blending correct.
     *
     * Formule entière avec arrondi à mi-chemin : (c * a + 127) / 255.
     */
    void NkImage::PremultiplyAlpha() noexcept {
        if (!IsValid() || mFormat != NkImagePixelFormat::NK_RGBA32) return;

        for (int32 y = 0; y < mHeight; ++y) {
            uint8* p = RowPtr(y);
            for (int32 x = 0; x < mWidth; ++x, p += 4) {
                uint32 a = p[3];
                p[0] = uint8((p[0] * a + 127u) / 255u);
                p[1] = uint8((p[1] * a + 127u) / 255u);
                p[2] = uint8((p[2] * a + 127u) / 255u);
                // p[3] inchangé (l'alpha reste l'alpha original)
            }
        }
    }

    /**
     * Copie (blit) l'image `src` entière dans *this à la position (dstX, dstY).
     *
     * Les deux images doivent avoir le même format pixel.
     * Les débordements (pixels en dehors de *this) sont clippés silencieusement.
     *
     * @param src   Image source.
     * @param dstX  Colonne de destination.
     * @param dstY  Ligne de destination.
     */
    void NkImage::Blit(const NkImage& src, int32 dstX, int32 dstY) noexcept {
        if (!IsValid() || !src.IsValid()) return;
        if (mFormat != src.mFormat)      return;

        // Coordonnées source (pour le clipping côté gauche/haut)
        int32 srcX = 0, srcY = 0;
        int32 w = src.mWidth, h = src.mHeight;

        // Clipping gauche/haut (dstX ou dstY négatifs)
        if (dstX < 0) { srcX -= dstX; w += dstX; dstX = 0; }
        if (dstY < 0) { srcY -= dstY; h += dstY; dstY = 0; }

        // Clipping droite/bas
        if (dstX + w > mWidth)  w = mWidth  - dstX;
        if (dstY + h > mHeight) h = mHeight - dstY;

        if (w <= 0 || h <= 0) return;

        const int32 bpp      = BytesPP();
        const int32 rowBytes = w * bpp;

        for (int32 row = 0; row < h; ++row) {
            nkMemcpy(RowPtr(dstY + row) + dstX * bpp,
                     src.RowPtr(srcY + row) + srcX * bpp,
                     rowBytes);
        }
    }

    // =========================================================================
    //  §20  NkImage — BlitRegion (blit avec scaling optionnel)
    // =========================================================================

    /**
     * Noyau direct de BlitRegion : copie sans redimensionnement.
     * La région source et la destination ont les mêmes dimensions.
     * Précondition : toutes les coordonnées sont valides et dans les bornes.
     */
    void NkImage::BlitRegionDirect(const NkImage& src,
                                    int32 sx, int32 sy, int32 sw, int32 sh,
                                    int32 dx, int32 dy) noexcept {
        const int32 bpp      = BytesPP();
        const int32 rowBytes = sw * bpp;
        for (int32 row = 0; row < sh; ++row) {
            nkMemcpy(RowPtr(dy + row) + dx * bpp,
                     src.RowPtr(sy + row) + sx * bpp,
                     rowBytes);
        }
    }

    /**
     * Noyau scalé de BlitRegion : copie avec interpolation bilinéaire.
     * La région source (sw × sh) est redimensionnée vers (dw × dh) pixels
     * de destination. Toutes les coordonnées sont valides.
     *
     * L'interpolation bilinéaire calcule pour chaque pixel destination le
     * point correspondant dans la source (en virgule flottante) et interpole
     * entre les 4 pixels voisins.
     */
    void NkImage::BlitRegionScaled(const NkImage& src,
                                    int32 sx, int32 sy, int32 sw, int32 sh,
                                    int32 dx, int32 dy, int32 dw, int32 dh) noexcept {
        const int32 bpp = BytesPP();

        // Facteurs de mise à l'échelle (source par pixel destination)
        const float xScale = float(sw) / float(dw);
        const float yScale = float(sh) / float(dh);

        for (int32 row = 0; row < dh; ++row) {
            // Position dans la source (coordonnée flottante centrée)
            const float fy = (row + 0.5f) * yScale - 0.5f;
            const int32 y0 = int32(fy);
            const float yt = fy - float(y0); // fraction [0, 1)
            // Clip bas
            const int32 y0c = (y0     >= sh ? sh - 1 : (y0     < 0 ? 0 : y0    ));
            const int32 y1c = (y0 + 1 >= sh ? sh - 1 : (y0 + 1 < 0 ? 0 : y0 + 1));

            for (int32 col = 0; col < dw; ++col) {
                const float fx = (col + 0.5f) * xScale - 0.5f;
                const int32 x0 = int32(fx);
                const float xt = fx - float(x0);
                const int32 x0c = (x0     >= sw ? sw - 1 : (x0     < 0 ? 0 : x0    ));
                const int32 x1c = (x0 + 1 >= sw ? sw - 1 : (x0 + 1 < 0 ? 0 : x0 + 1));

                // 4 pixels sources (coin haut-gauche, haut-droit, bas-gauche, bas-droit)
                const uint8* p00 = src.RowPtr(sy + y0c) + (sx + x0c) * bpp;
                const uint8* p10 = src.RowPtr(sy + y0c) + (sx + x1c) * bpp;
                const uint8* p01 = src.RowPtr(sy + y1c) + (sx + x0c) * bpp;
                const uint8* p11 = src.RowPtr(sy + y1c) + (sx + x1c) * bpp;

                uint8* dp = RowPtr(dy + row) + (dx + col) * bpp;

                // Interpolation bilinéaire par canal
                for (int32 c = 0; c < bpp; ++c) {
                    float v = float(p00[c]) * (1.0f - xt) * (1.0f - yt) +
                              float(p10[c]) *         xt  * (1.0f - yt) +
                              float(p01[c]) * (1.0f - xt) *         yt  +
                              float(p11[c]) *         xt  *         yt;
                    dp[c] = uint8(v < 0.0f ? 0 : (v > 255.0f ? 255 : v + 0.5f));
                }
            }
        }
    }

    /**
     * Copie (blit) une sous-région de `src` dans une sous-région de *this,
     * avec redimensionnement optionnel par interpolation bilinéaire.
     *
     * Comportement selon les tailles des régions :
     *   - srcRegion vide (w==0 && h==0) → utilise l'image src entière.
     *   - dstRegion vide (w==0 && h==0) → copie sans scaling à (left, top).
     *   - Tailles différentes            → scaling bilinéaire.
     *   - Tailles identiques             → copie directe (plus rapide).
     *
     * Les deux images doivent avoir le même format pixel.
     * Les régions source et destination sont clippées aux bornes avant tout travail.
     *
     * @return false si les images sont invalides ou de formats différents.
     */
    bool NkImage::BlitRegion(const NkImage& src,
                              const math::NkIntRect& srcRegion,
                              const math::NkIntRect& dstRegion) noexcept {
        if (!IsValid() || !src.IsValid()) return false;
        if (mFormat != src.mFormat)       return false;

        // ── Résolution de la région source ────────────────────────────────────
        int32 sx = srcRegion.left;
        int32 sy = srcRegion.top;
        int32 sw = (srcRegion.width  > 0) ? srcRegion.width  : src.mWidth;
        int32 sh = (srcRegion.height > 0) ? srcRegion.height : src.mHeight;

        // ── Résolution de la région destination ───────────────────────────────
        int32 dx = dstRegion.left;
        int32 dy = dstRegion.top;
        // Si dstRegion vide : même taille que la source (pas de scaling)
        bool noScale = (dstRegion.width == 0 && dstRegion.height == 0);
        int32 dw = noScale ? sw : dstRegion.width;
        int32 dh = noScale ? sh : dstRegion.height;

        // ── Clipping de la région source dans src ─────────────────────────────
        // Bord gauche
        if (sx < 0) {
            int32 clip = -sx;
            sx = 0; sw -= clip;
            if (!noScale) {
                // Ajuste la region destination proportionnellement
                int32 dClip = int32(clip * float(dw) / float(sw + clip) + 0.5f);
                dx += dClip; dw -= dClip;
            } else {
                dx += clip; dw -= clip;
            }
        }
        // Bord haut
        if (sy < 0) {
            int32 clip = -sy;
            sy = 0; sh -= clip;
            if (!noScale) {
                int32 dClip = int32(clip * float(dh) / float(sh + clip) + 0.5f);
                dy += dClip; dh -= dClip;
            } else {
                dy += clip; dh -= clip;
            }
        }
        // Bord droit
        if (sx + sw > src.mWidth)  sw = src.mWidth  - sx;
        // Bord bas
        if (sy + sh > src.mHeight) sh = src.mHeight - sy;

        // ── Clipping de la région destination dans *this ──────────────────────
        if (dx < 0) { dx = 0; dw += dx; }
        if (dy < 0) { dy = 0; dh += dy; }
        if (dx + dw > mWidth)  dw = mWidth  - dx;
        if (dy + dh > mHeight) dh = mHeight - dy;

        // Après clipping il peut ne rien rester
        if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return true;

        // ── Sélection du noyau selon le besoin de scaling ─────────────────────
        if (sw == dw && sh == dh) {
            // Pas de scaling : copie directe ligne par ligne
            BlitRegionDirect(src, sx, sy, sw, sh, dx, dy);
        } else {
            // Scaling : interpolation bilinéaire
            BlitRegionScaled(src, sx, sy, sw, sh, dx, dy, dw, dh);
        }
        return true;
    }
 
    // ═════════════════════════════════════════════════════════════════════════════
    //  NkImage — BlitRegion (sous-région src → sous-région dst, avec scale)
    // ═════════════════════════════════════════════════════════════════════════════
 
    /**
     * Copie une sous-région de `src` (srcRegion) vers une sous-région de *this
     * (dstRegion) avec redimensionnement bilinéaire si les tailles diffèrent.
     *
     * Cas d'usage typiques :
     *   1. Scale simple      : srcRegion = pleine image, dstRegion = zone cible
     *   2. Sprite sheet      : srcRegion = frame, dstRegion = position dans l'atlas
     *   3. Thumbnail partiel : srcRegion = zone d'intérêt, dstRegion = miniature
     *
     * Comportement :
     *   - Si srcRegion est vide (w==0 || h==0) → utilise l'image source entière.
     *   - Si dstRegion est vide (w==0 || h==0) → utilise *this entier.
     *   - Si les tailles source et destination sont identiques → copie pixel-perfect
     *     (pas d'interpolation, plus rapide).
     *   - Sinon → interpolation bilinéaire pour le scaling.
     *   - Les deux images DOIVENT avoir le même format pixel.
     *   - Les régions sont clippées aux bornes de leurs images respectives.
     *   - Formats HDR (float) : interpolation bilinéaire sur les floats directement.
     *
     * @param src        Image source.
     * @param srcRegion  Région à lire dans src (NkIntRect). Vide = image entière.
     * @param dstRegion  Région à écrire dans *this (NkIntRect). Vide = *this entier.
     * @param filter     Filtre d'interpolation pour le scaling
     *                   (NK_NEAREST ou NK_BILINEAR, défaut NK_BILINEAR).
     * @return true si l'opération a réussi.
     */
    bool NkImage::BlitRegion(const NkImage& src,
                              const math::NkIntRect& srcRegion,
                              const math::NkIntRect& dstRegion,
                              NkResizeFilter filter) noexcept {
        if (!IsValid() || !src.IsValid()) return false;
        if (mFormat != src.mFormat)       return false;
 
        // ── Résolution des régions (vide → image entière) ─────────────────────────
 
        int32 sX = srcRegion.left;
        int32 sY = srcRegion.top;
        int32 sW = (srcRegion.width  > 0) ? srcRegion.width  : src.mWidth;
        int32 sH = (srcRegion.height > 0) ? srcRegion.height : src.mHeight;
 
        int32 dX = dstRegion.left;
        int32 dY = dstRegion.top;
        int32 dW = (dstRegion.width  > 0) ? dstRegion.width  : mWidth;
        int32 dH = (dstRegion.height > 0) ? dstRegion.height : mHeight;
 
        // ── Clamp des régions aux bornes de leurs images ──────────────────────────
 
        // Source : ne peut pas déborder au-delà des bords de src
        if (sX < 0) { sW += sX; sX = 0; }
        if (sY < 0) { sH += sY; sY = 0; }
        if (sX + sW > src.mWidth)  sW = src.mWidth  - sX;
        if (sY + sH > src.mHeight) sH = src.mHeight - sY;
 
        // Destination : ne peut pas déborder au-delà des bords de *this
        if (dX < 0) { dW += dX; dX = 0; }
        if (dY < 0) { dH += dY; dY = 0; }
        if (dX + dW > mWidth)  dW = mWidth  - dX;
        if (dY + dH > mHeight) dH = mHeight - dY;
 
        // Après clamp, rien à faire
        if (sW <= 0 || sH <= 0 || dW <= 0 || dH <= 0) return true;
 
        const int32 bpp = BytesPP();
 
        // ── Copie directe si les tailles coïncident (pixel-perfect) ───────────────
 
        if (sW == dW && sH == dH) {
            const int32 rowBytes = sW * bpp;
            for (int32 row = 0; row < sH; ++row) {
                nkMemcpy(RowPtr(dY + row)       + dX * bpp,
                         src.RowPtr(sY + row)   + sX * bpp,
                         rowBytes);
            }
            return true;
        }
 
        // ── Scaling avec interpolation ────────────────────────────────────────────
        //
        // Principe : pour chaque pixel de destination (dx, dy), calcule la
        // coordonnée fractionnaire correspondante dans la région source.
        // Le facteur de scale est : srcSize / dstSize (peut être > 1 ou < 1).
        //
        // Coordonnée source fractionnaire :
        //   fx = sX + (x + 0.5) * (sW / dW) - 0.5
        //   fy = sY + (y + 0.5) * (sH / dH) - 0.5
        // Le +0.5/-0.5 centre l'échantillon au milieu du pixel de destination.
 
        const float32 scaleX = float32(sW) / float32(dW);
        const float32 scaleY = float32(sH) / float32(dH);
 
        if (filter == NkResizeFilter::NK_NEAREST) {
 
            // ── Plus proche voisin ────────────────────────────────────────────────
            for (int32 dy = 0; dy < dH; ++dy) {
                // Coordonnée source Y : arrondi au pixel le plus proche
                const int32 sy2 = sY + int32((dy + 0.5f) * scaleY);
                const int32 syC = (sy2 < 0) ? 0 : (sy2 >= src.mHeight ? src.mHeight-1 : sy2);
 
                uint8*       dstRow = RowPtr(dY + dy)       + dX * bpp;
                const uint8* srcRow = src.RowPtr(syC);
 
                for (int32 dx = 0; dx < dW; ++dx) {
                    const int32 sx2 = sX + int32((dx + 0.5f) * scaleX);
                    const int32 sxC = (sx2 < 0) ? 0 : (sx2 >= src.mWidth ? src.mWidth-1 : sx2);
                    nkMemcpy(dstRow + dx * bpp, srcRow + sxC * bpp, bpp);
                }
            }
 
        } else {
 
            // ── Bilinéaire (défaut pour NK_BILINEAR, NK_BICUBIC, NK_LANCZOS3) ─────
            //
            // Note : NK_BICUBIC et NK_LANCZOS3 ne sont pas encore implémentés ici.
            // Ils tombent en fallback bilinéaire, ce qui est correct et safe.
            // L'implémentation bicubique/lanczos peut être ajoutée sans changer
            // l'interface : il suffit d'un switch sur `filter` ici.
            //
            // Bilinéaire : interpolation sur 4 voisins (p00, p10, p01, p11).
            // Pour les formats HDR (float), on travaille directement sur les floats.
 
            const bool isHDR = IsHDR();
            // bppF = nombre de float32 par pixel pour les formats HDR
            const int32 bppF = bpp / int32(sizeof(float32));
 
            for (int32 dy = 0; dy < dH; ++dy) {
                const float32 fy  = sY + (dy + 0.5f) * scaleY - 0.5f;
                const int32   y0  = int32(fy);
                const float32 yt  = fy - float32(y0);
                // Clamp des indices sources aux bornes de src
                const int32   sy0 = (y0 < 0) ? 0 : (y0 >= src.mHeight ? src.mHeight-1 : y0);
                const int32   sy1 = (y0+1 < src.mHeight) ? y0+1 : sy0;
 
                uint8* dstRow = RowPtr(dY + dy) + dX * bpp;
 
                for (int32 dx = 0; dx < dW; ++dx) {
                    const float32 fx  = sX + (dx + 0.5f) * scaleX - 0.5f;
                    const int32   x0  = int32(fx);
                    const float32 xt  = fx - float32(x0);
                    const int32   sx0 = (x0 < 0) ? 0 : (x0 >= src.mWidth ? src.mWidth-1 : x0);
                    const int32   sx1 = (x0+1 < src.mWidth) ? x0+1 : sx0;
 
                    uint8* dp = dstRow + dx * bpp;
 
                    if (isHDR) {
                        // ── Interpolation bilinéaire sur float32 ─────────────────
                        const float32* p00 = reinterpret_cast<const float32*>(src.RowPtr(sy0) + sx0 * bpp);
                        const float32* p10 = reinterpret_cast<const float32*>(src.RowPtr(sy0) + sx1 * bpp);
                        const float32* p01 = reinterpret_cast<const float32*>(src.RowPtr(sy1) + sx0 * bpp);
                        const float32* p11 = reinterpret_cast<const float32*>(src.RowPtr(sy1) + sx1 * bpp);
                        float32*       out = reinterpret_cast<float32*>(dp);
                        for (int32 c = 0; c < bppF; ++c) {
                            out[c] = p00[c] * (1.0f-xt) * (1.0f-yt)
                                   + p10[c] *       xt  * (1.0f-yt)
                                   + p01[c] * (1.0f-xt) *       yt
                                   + p11[c] *       xt  *       yt;
                        }
                    } else {
                        // ── Interpolation bilinéaire sur uint8 ───────────────────
                        const uint8* p00 = src.RowPtr(sy0) + sx0 * bpp;
                        const uint8* p10 = src.RowPtr(sy0) + sx1 * bpp;
                        const uint8* p01 = src.RowPtr(sy1) + sx0 * bpp;
                        const uint8* p11 = src.RowPtr(sy1) + sx1 * bpp;
                        for (int32 c = 0; c < bpp; ++c) {
                            float32 v = float32(p00[c]) * (1.0f-xt) * (1.0f-yt)
                                      + float32(p10[c]) *       xt  * (1.0f-yt)
                                      + float32(p01[c]) * (1.0f-xt) *       yt
                                      + float32(p11[c]) *       xt  *       yt;
                            // Arrondi correct avec clamp [0, 255]
                            dp[c] = uint8(v < 0.0f ? 0 : (v > 255.0f ? 255 : v + 0.5f));
                        }
                    }
                }
            }
        }
        return true;
    }

    // =========================================================================
    //  §21  NkImage — Crop
    // =========================================================================

    /**
     * Retourne une sous-région de l'image comme nouvelle image allouée.
     *
     * La région (x, y, w, h) doit être entièrement dans les bornes de l'image.
     * Utilise Alloc + copie ligne par ligne pour respecter le stride.
     *
     * @return Nouvelle image owning, ou nullptr si la région est invalide.
     */
    NkImage* NkImage::Crop(int32 x, int32 y, int32 w, int32 h) const noexcept {
        if (!IsValid()) return nullptr;
        if (x < 0 || y < 0 || w <= 0 || h <= 0) return nullptr;
        if (x + w > mWidth || y + h > mHeight)   return nullptr;

        NkImage* dst = Alloc(w, h, mFormat);
        if (!dst) return nullptr;

        const int32 bpp      = BytesPP();
        const int32 rowBytes = w * bpp;
        for (int32 row = 0; row < h; ++row) {
            nkMemcpy(dst->RowPtr(row),
                     RowPtr(y + row) + x * bpp,
                     rowBytes);
        }
        dst->mSrcFmt = mSrcFmt;
        return dst;
    }

    // =========================================================================
    //  §22  NkImage — Resize
    // =========================================================================

    /**
     * Redimensionne l'image à (nw × nh) pixels.
     * Retourne une nouvelle image owning.
     *
     * Implémentation :
     *   NK_NEAREST  : accès direct au pixel le plus proche (O(nw*nh)).
     *   NK_BILINEAR : interpolation sur 4 voisins avec pondération bilinéaire.
     *   NK_BICUBIC, NK_LANCZOS3 : fallback bilinéaire (non encore implémentés).
     *
     * Les coordonnées sources fractionnaires sont centrées sur les pixels
     * (+0.5/-0.5) pour éviter le décalage d'un demi-pixel aux bords.
     */
    NkImage* NkImage::Resize(int32 nw, int32 nh, NkResizeFilter f) const noexcept {
        if (!IsValid() || nw <= 0 || nh <= 0) return nullptr;
 
        // BlitRegion gère tous les cas : même format, même taille → copie,
        // tailles différentes → scaling. On crée la destination et on délègue.
        NkImage* dst = Alloc(nw, nh, mFormat);
        if (!dst) return nullptr;
 
        math::NkIntRect srcFull{0, 0, mWidth,  mHeight};
        math::NkIntRect dstFull{0, 0, nw,      nh     };
 
        if (!dst->BlitRegion(*this, srcFull, dstFull, f)) {
            dst->Free();
            return nullptr;
        }
        dst->mSrcFmt = mSrcFmt;
        return dst;
    }

} // namespace nkentseu