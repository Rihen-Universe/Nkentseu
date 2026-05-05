#pragma once
/**
 * @File    NkPPMCodec.h
 * @Brief   Codec PPM/PGM/PBM — formats NetPBM P1-P6.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */
#include "NKImage/Core/NkImage.h"

namespace nkentseu {

    class NKENTSEU_IMAGE_API NkPPMCodec {
        public:
            /**
             * @Brief Décode un buffer PPM/PGM/PBM (P1-P6) en mémoire.
             * P1/P4 : bitmap noir/blanc → NK_GRAY8
             * P2/P5 : grayscale → NK_GRAY8
             * P3/P6 : RGB couleur → NK_RGB24
             */
            static NkImage* Decode(const uint8* data, usize size) noexcept;

            /**
             * @Brief Encode une NkImage en PPM ou PGM binaire (P5/P6) vers un fichier.
             * @param path Chemin du fichier de sortie.
             */
            static bool Encode(const NkImage& img, const char* path) noexcept;
    };

} // namespace nkentseu
