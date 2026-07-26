// =============================================================================
// NKMedia/Codecs/Video/Mpeg2/NkMpeg2Decoder.h
// -----------------------------------------------------------------------------
// Décodeur MPEG-2 Video (ISO/IEC 13818-2) FROM-SCRATCH (sans ffmpeg). Extension
// directe du chemin MPEG-1 déjà présent (mêmes start codes, mêmes VLC de base,
// même IDCT/motion) enrichie des spécificités MPEG-2 : sequence_extension,
// picture_coding_extension, précision DC intra, quantification linéaire/non-linéaire
// (quantiser_scale = 2*code en linéaire), matrices intra/non-intra, table VLC
// alternative des coefficients (B-15, intra_vlc_format), balayage alternatif,
// reconstruction I/P/B avec DPB (forward + backward pour les B), demi-pel bilinéaire.
//
// Périmètre actuel : Main Profile progressif, prédiction et DCT « frame-based »
// (frame_pred_frame_dct=1, picture_structure=Frame). Les flux entrelacés (champs,
// field/dual-prime motion, DCT de champ) sont refusés proprement (RefusedInterlace()).
// Chroma 4:2:0 (et 4:2:2 partiel). Zero-STL, namespace nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace media {

		// Une image décodée (plans Y/Cb/Cr séparés, 8 bits). Les plans sont dimensionnés
		// aux multiples de 16 (luma) / 8 (chroma 4:2:0) — la zone utile d'affichage est
		// width x height (luma) et chromaW x chromaH (chroma).
		struct NkMpeg2Frame {
				NkVector<uint8> y, cb, cr;
				int32 lumaW = 0, lumaH = 0;	   // plans luma alloués (padding MB)
				int32 chromaW = 0, chromaH = 0; // plans chroma alloués
				int32 width = 0, height = 0;	// dimensions d'affichage (luma)
				int32 dispChromaW = 0, dispChromaH = 0; // dimensions d'affichage chroma
				int32 pictType = 0;			   // 1=I, 2=P, 3=B
				int32 temporalRef = 0;		   // temporal_reference (ordre d'affichage dans le GOP)
				int32 poc = 0;				   // ordre d'affichage global (calculé)
		};

		struct NkMpeg2Decoder {
			public:
				// Décode tout le flux élémentaire `data`/`size` (.m2v : start codes Annex B
				// 00 00 01). Remplit `out` avec les images DANS L'ORDRE D'AFFICHAGE (les B sont
				// réordonnées). Retourne false en cas d'échec dur. `errmsg` (optionnel) reçoit
				// une raison lisible (ex. flux entrelacé refusé).
				static bool DecodeAll(const uint8 *data, usize size, NkVector<NkMpeg2Frame> &out,
									  char *errmsg = nullptr, usize errcap = 0);

				// Auto-test minimal (tables VLC cohérentes).
				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
