#pragma once
/**
 * @File    NkPNGCodec.h
 * @Brief   Codec PNG — décodage et encodage RFC 2083.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
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
             * @param out     Buffer de sortie alloué via l'allocateur NKMemory
             *                (nkentseu::memory::NkAlloc). L'appelant DOIT le
             *                libérer avec `nkentseu::memory::NkFree(out)`.
             *                NE PAS utiliser `std::free` / `delete[]` :
             *                l'allocateur custom n'est pas compatible avec le
             *                heap CRT et un free CRT cause une heap corruption
             *                (crash c0000374 sur Windows).
             * @param outSize Taille du buffer de sortie.
             * @return true si succès.
             */
            static bool Encode(const NkImage& img, uint8*& out, usize& outSize) noexcept;
    };

} // namespace nkentseu