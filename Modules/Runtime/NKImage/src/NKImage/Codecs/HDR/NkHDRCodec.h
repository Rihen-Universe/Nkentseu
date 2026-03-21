#pragma once
/**
 * @File    NkHDRCodec.h
 * @Brief   Codec Radiance HDR (.hdr/.rgbe) production-ready.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Support
 *  Lecture  : Header complet (EXPOSURE, FORMAT), nouveau RLE 4-canaux,
 *              ancien RLE, raw, orientations ±X ±Y.
 *  Écriture : Nouveau RLE compressé, header Radiance standard.
 *  Pixels   : float32 RGB96F (3 × float32 par pixel).
 */
#include "NKImage/NkImage.h"
namespace nkentseu {
class NKENTSEU_IMAGE_API NkHDRCodec {
public:
    /// Décode un fichier HDR en NkImage RGB96F (float32).
    static NkImage* Decode(const uint8* data, usize size) noexcept;
    /// Écrit l'image dans un fichier .hdr avec RLE compressé.
    static bool     Encode(const NkImage& img, const char* path) noexcept;
    /// Encode en mémoire (buffer à libérer avec free).
    static bool     EncodeToMemory(const NkImage& img, uint8*& out, usize& outSize) noexcept;
};
} // namespace nkentseu
