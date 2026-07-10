// =============================================================================
// NKMedia/Codecs/Video/Mpeg1/NkMpeg1Tables.h
// -----------------------------------------------------------------------------
// Tables du format MPEG-1 Video (ISO/IEC 11172-2) : matrices de quantification par
// défaut, VLC des coefficients DC (luma/chroma), VLC des coefficients AC (run/level),
// balayage zig-zag. Ce sont des CONSTANTES DU FORMAT (réutilisables comme les eBands
// d'Opus). L'algorithme d'encodage est réécrit à la sauce Nkentseu. Zero-STL.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		struct NkMpeg1Tables {
			public:
				static const uint16 *IntraMatrix();	  // 64, ordre raster
				static const uint8 *ZigZag();		  // 64 : index raster pour la position de scan
				static const uint16 *DcLumCode();	  // 12
				static const uint8 *DcLumBits();	  // 12
				static const uint16 *DcChromaCode();  // 12
				static const uint8 *DcChromaBits();	  // 12
				// AC : 111 entrées (run/level) + escape(111) + EOB(112).
				static const uint16 (*AcVlc())[2]; // [113][2] : {code, bits}
				static const int8 *AcRun();		   // 111
				static const int8 *AcLevel();		   // 111
				static constexpr int32 kAcCount = 111;
				static constexpr int32 kAcEscape = 111;
				static constexpr int32 kAcEob = 112;

				// Cherche l'index VLC d'un couple (run, |level|), ou -1 si absent (→ escape).
				static int32 FindAc(int32 run, int32 absLevel);
		};

	} // namespace media
} // namespace nkentseu
