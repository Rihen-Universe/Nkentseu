#pragma once
/**
 * @File    NkPNGCodec.h
 * @Brief   Codec PNG — décodage et encodage RFC 2083.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */
#include "NKImage/Core/NkImage.h"

namespace nkentseu {

    class NKENTSEU_IMAGE_API NkPNGCodec {
        public:
            /**
             * @Brief Décode un buffer PNG en mémoire.
             * @param data  Pointeur vers les données brutes du fichier PNG.
             * @param size  Taille en octets.
             * @return NkImage alloué (appelant doit appeler Free()), nullptr si échec.
             */
            static NkImage* Decode(const uint8* data, usize size) noexcept;

            /**
             * @Brief Encode une NkImage en PNG vers un buffer mémoire.
             * @param img     Image source (tout format de pixel entier).
             * @param out     Buffer de sortie alloué avec malloc (appelant libère avec free).
             * @param outSize Taille du buffer de sortie.
             * @return true si succès.
             */
            static bool Encode(const NkImage& img, uint8*& out, usize& outSize) noexcept;
    };

} // namespace nkentseu