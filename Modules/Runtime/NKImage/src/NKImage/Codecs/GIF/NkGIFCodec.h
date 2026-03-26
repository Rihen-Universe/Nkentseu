#pragma once
/**
 * @File    NkGIFCodec.h
 * @Brief   Codec GIF production-ready — GIF87a/GIF89a complet.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Support
 *  Lecture  : GIF87a + GIF89a, LZW variable, GCT + LCT, interlacement,
 *              transparence (Graphic Control Extension), première frame.
 *  Écriture : GIF89a, LZW variable (2-12 bits), quantification médiane coupure,
 *              palette 256 couleurs, transparence alpha, Graphic Control Extension.
 */
#include "NKImage/Core/NkImage.h"
#include <cstdio>

namespace nkentseu {
    class NKENTSEU_IMAGE_API NkGIFCodec {
    public:
        /// Décode un GIF — retourne la première frame en RGBA32.
        static NkImage* Decode(const uint8* data, usize size) noexcept;

        /// Encode en GIF89a avec quantification médiane coupure 256 couleurs.
        /// Gère la transparence (canal alpha → entrée palette dédiée).
        static bool Encode(const NkImage& img, uint8*& out, usize& outSize) noexcept;

        /// Sauvegarde directement dans un fichier .gif
        static bool Save(const NkImage& img, const char* path) noexcept;
    };
} // namespace nkentseu
