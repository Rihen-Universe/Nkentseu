#pragma once
/**
 * @File    NkEXRCodec.h
 * @Brief   Codec OpenEXR (.exr) production-ready, from scratch.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
 *
 * @Format
 *  OpenEXR 1.x scanline single-part (le plus repandu).
 *  Magic : 0x762F3101 little-endian (versions ILM Industrial Light & Magic).
 *  Attributs typiques : channels (chlist), compression, dataWindow,
 *  displayWindow, lineOrder, pixelAspectRatio, screenWindowCenter,
 *  screenWindowWidth.
 *
 * @Support
 *  Lecture  :
 *    - Pixel types  : HALF (16-bit float IEEE-754), FLOAT (32-bit float),
 *                     UINT (32-bit, converti en float).
 *    - Compressions : NONE, RLE, ZIPS (per-scanline zlib),
 *                     ZIP (16-scanline blocks zlib) -> 100% fonctionnels.
 *                     PIZ (wavelet + Huffman) -> BETA, decode structurellement
 *                       correct mais corruption residuelle (a finaliser v1).
 *    - Layouts      : R/G/B/A (alpha optionnel), Y/RY/BY (luminance/chroma
 *                     converti en RGB), Z (depth retourne en grayscale).
 *    - Orientation  : lineOrder INCREASING_Y et DECREASING_Y.
 *  Lecture non-supportee (retourne nullptr avec log clair) :
 *    - PXR24, B44, B44A, DWAA, DWAB : lossy, ~5% des EXR en circulation.
 *      Workaround pour l'utilisateur : "oiiotool input.exr --compression zip
 *      -o output.exr" pour reconvertir en compression standard.
 *    - Tiles (TILE_* flag dans version) : niveaux mip/multipart non supportes.
 *    - Multipart EXR 2.0 : un seul "part" lu, les autres ignores.
 *
 *  Ecriture : non implementee dans v0 (uniquement Decode). L'export HDR
 *             standard via NkHDRCodec couvre le besoin "sauvegarder en HDR".
 *
 * @Pixel_output
 *  - 3 canaux (R/G/B ou Y/RY/BY)  -> NK_RGB96F   (3 * float32)
 *  - 4 canaux (R/G/B/A)           -> NK_RGBA128F (4 * float32)
 *  - 1 canal  (Y ou Z seul)       -> NK_RGB96F  (R=G=B=valeur)
 *
 *  Le caller peut ensuite passer le NkImage a NkHDRCodec::ConvertToTexture()
 *  pour obtenir un RGBA8 tone-mappe (Reinhard + gamma).
 */
#include "NKImage/Core/NkImage.h"

namespace nkentseu {

    class NKENTSEU_IMAGE_API NkEXRCodec {
    public:
        /// Decode un buffer EXR en NkImage RGB96F ou RGBA128F.
        /// @param data  Buffer EXR complet (entete + offset table + chunks).
        /// @param size  Taille du buffer en octets.
        /// @return      NkImage* a liberer avec ->Free(), ou nullptr si format
        ///              invalide / compression non supportee.
        static NkImage* Decode(const uint8* data, usize size) noexcept;
    };

} // namespace nkentseu
