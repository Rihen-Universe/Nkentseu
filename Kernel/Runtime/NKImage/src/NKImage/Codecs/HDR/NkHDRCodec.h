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
#include "NKImage/Core/NkImage.h"

namespace nkentseu {

    // Structure optionnelle pour retourner des données brutes
    // struct NkTextureData {
    //     uint8* data;   // 4 × uint8 par pixel (RGBA)
    //     int32  width;
    //     int32  height;
    //     usize  size;   // width * height * 4
    // };
    
    class NKENTSEU_IMAGE_API NkHDRCodec {
        public:
            /// Décode un fichier HDR en NkImage RGB96F (float32).
            static NkImage* Decode(const uint8* data, usize size) noexcept;
            /// Écrit l'image dans un fichier .hdr avec RLE compressé.
            static bool     Encode(const NkImage& img, const char* path) noexcept;
            /// Encode en mémoire (buffer à libérer avec free).
            static bool     EncodeToMemory(const NkImage& img, uint8*& out, usize& outSize) noexcept;

            /// Convertit une image HDR (RGB96F) en texture RGBA8 avec tone mapping.
            /// @param hdrImage   Image HDR valide au format NK_RGB96F.
            /// @param exposure   Facteur d’exposition supplémentaire (optionnel, défaut = 1.0).
            /// @param gamma      Correction gamma (défaut = 2.2). Si <= 0, pas de gamma.
            /// @return           Nouvelle image NkImage au format NK_RGBA8, à libérer avec Free().
            static NkImage* ConvertToTexture(const NkImage& hdrImage,
                                            float exposure = 1.0f,
                                            float gamma = 2.2f) noexcept;

            /// Version retournant une structure simple (données brutes).
            /// @param outData    Remplie avec les pixels RGBA8 (à libérer via NkFree).
            // static bool ConvertToTextureData(const NkImage& hdrImage,
            //                                 NkTextureData& outData,
            //                                 float exposure = 1.0f,
            //                                 float gamma = 2.2f) noexcept;
    };
} // namespace nkentseu
