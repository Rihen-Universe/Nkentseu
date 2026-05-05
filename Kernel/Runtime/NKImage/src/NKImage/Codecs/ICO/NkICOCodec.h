#pragma once
/**
 * @File    NkICOCodec.h
 * @Brief   Codec ICO/CUR — lecture seule, sélectionne la plus grande image.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * Supporte les embeds PNG et BMP DIB dans les fichiers ICO/CUR.
 */
#include "NKImage/Core/NkImage.h"

namespace nkentseu {

    class NKENTSEU_IMAGE_API NkICOCodec {
    public:
        /**
         * @Brief Décode un buffer ICO/CUR. Sélectionne automatiquement
         *        l'entrée de plus grande résolution. Supporte PNG et BMP DIB.
         */
        static NkImage* Decode(const uint8* data, usize size) noexcept;
    };

} // namespace nkentseu
