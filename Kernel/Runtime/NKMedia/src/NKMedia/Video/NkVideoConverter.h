// =============================================================================
// NKMedia/Video/NkVideoConverter.h
// -----------------------------------------------------------------------------
// Pipeline de conversion vidéo from-scratch : décode une SOURCE de trames (séquence
// d'images via NKImage, ou vidéo MJPEG) → RGB → réencode vers un SINK (format déduit
// de l'extension de sortie : .mp4/.mov = H.264, .avi = MJPEG, .m1v = MPEG-1). Réutilise
// tout ce qui existe : décodeurs NKImage, encodeurs NkVideoWriter / NkH264Encoder.
// Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		struct NkVideoConverter {
			public:
				// Séquence d'images → vidéo. Le nom de chaque image = `prefix` + index (zéro-paddé sur
				// `digits`) + `suffix` (ex prefix="frame_", digits=3, suffix=".png" → "frame_007.png").
				// `outPath` : extension → format (.mp4/.mov=H.264, .avi=MJPEG, .m1v/.mpg=MPEG-1).
				// Renvoie le nombre de trames converties (0 = échec / aucune image lue).
				static int32 ImageSequenceToVideo(const char *prefix, int32 digits, const char *suffix, int32 first,
												  int32 count, const char *outPath, int32 fpsNum, int32 fpsDen,
												  int32 quality = 90);

				// Transcode une vidéo MJPEG (conteneur AVI) → tout format de sortie (décode chaque JPEG via
				// NKImage puis réencode). Ex : MJPEG AVI → H.264 MP4. Renvoie le nombre de trames.
				static int32 MjpegAviToVideo(const char *aviPath, const char *outPath, int32 quality = 90);
		};

	} // namespace media
} // namespace nkentseu
