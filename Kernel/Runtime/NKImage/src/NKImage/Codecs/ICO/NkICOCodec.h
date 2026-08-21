#pragma once
/**
 * @File    NkICOCodec.h
 * @Brief   Codec ICO/CUR — lecture seule, sélectionne la plus grande image.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - All Rights Reserved (see LICENSE)
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
			 * @Return L'image décodée PAR VALEUR ; en cas d'échec elle est
			 *         INVALIDE (IsValid()==false). Il n'y a plus de nullptr.
			 */
			static NkImage Decode(const uint8 *data, usize size) noexcept;
	};

} // namespace nkentseu
