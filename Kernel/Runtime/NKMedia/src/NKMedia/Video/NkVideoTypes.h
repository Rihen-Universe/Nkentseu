// =============================================================================
// NKMedia/Video/NkVideoTypes.h — types partagés du sous-système vidéo (évite les
// cycles d'inclusion entre NkVideoWriter et les encodeurs de codec). Zero-STL.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		// Format des pixels d'ENTRÉE fournis aux encodeurs vidéo.
		enum class NkVideoInputFormat {
			RGB24,	// 3 octets/pixel R,G,B, haut-en-bas
			RGBA32, // 4 octets/pixel R,G,B,A, haut-en-bas
			BGR24,	// 3 octets/pixel B,G,R, haut-en-bas
		};

	} // namespace media
} // namespace nkentseu
