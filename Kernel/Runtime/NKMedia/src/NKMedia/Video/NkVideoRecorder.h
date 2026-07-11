// =============================================================================
// NKMedia/Video/NkVideoRecorder.h
// -----------------------------------------------------------------------------
// Enregistreur A/V from-scratch pour CAPTURER le rendu du moteur (NKRenderer/NKCanvas)
// + le son (NKAudio) dans un MP4 (H.264 vidéo + LPCM audio). Boucle type :
//   rec.Begin("capture.mp4", w, h, fps); int a = rec.AddAudio(48000, 2);
//   pour chaque frame : <rendu+Display> ; window.Capture(...) → pixels ;
//                       rec.PushVideo(pixels, RGBA32) ; rec.PushAudio(a, mix, n) ;
//   rec.End();
// Gère le retournement vertical (framebuffers bottom-up type OpenGL). Facade au-dessus
// de NkH264Encoder (qui fait déjà MP4 + audio + langues). Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMedia/Video/NkVideoTypes.h" // NkVideoInputFormat
#include "NKMedia/Codecs/Video/H264/NkH264Encoder.h"

namespace nkentseu {
	namespace media {

		struct NkVideoRecorder {
			public:
				// Démarre un enregistrement MP4 (H.264). `qp` 0..51 (petit = meilleure qualité).
				bool Begin(const char *path, int32 width, int32 height, int32 fpsNum = 60, int32 fpsDen = 1,
						   int32 qp = 24);

				// Ajoute une piste audio PCM (à appeler après Begin). Plusieurs pistes = choix de LANGUE
				// (`lang3` = code ISO-639-2/BCP-47, ex "fre","eng","bbj"). Renvoie l'index de piste.
				int32 AddAudio(int32 sampleRate, int32 channels, const char *lang3 = nullptr);

				// Ajoute une piste de SOUS-TITRES (langue libre). Renvoie l'index.
				int32 AddSubtitleTrack(const char *lang3 = nullptr);
				// Ajoute un sous-titre (UTF-8) affiché de `startMs` pendant `durMs` sur la piste `trackIdx`.
				void AddSubtitle(int32 trackIdx, const char *utf8, uint32 startMs, uint32 durMs);

				// Pousse une trame capturée du framebuffer. `flipVertical` = true pour un framebuffer
				// bottom-up (OpenGL). Formats : RGBA32 (capture DX11), RGB24, BGR24.
				bool PushVideo(const uint8 *pixels, NkVideoInputFormat fmt, bool flipVertical = false);

				// Pousse des trames audio capturées (int16 entrelacé) sur la piste `trackIdx`.
				void PushAudio(int32 trackIdx, const int16 *interleaved, uint32 frames);

				// Termine et ferme le fichier.
				bool End();

				bool IsRecording() const {
					return mOpen;
				}
				int32 FrameCount() const {
					return mEnc.FrameCount();
				}

			private:
				NkH264Encoder mEnc;
				int32 mWidth = 0, mHeight = 0;
				bool mOpen = false;
				NkVector<uint8> mFlip; // buffer temporaire pour le retournement vertical
		};

	} // namespace media
} // namespace nkentseu
