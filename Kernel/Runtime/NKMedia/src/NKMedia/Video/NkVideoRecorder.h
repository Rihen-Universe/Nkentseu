// =============================================================================
// NKMedia/Video/NkVideoRecorder.h
// -----------------------------------------------------------------------------
// Enregistreur A/V from-scratch pour CAPTURER le rendu du moteur (NKRenderer/NKCanvas)
// + le son (NKAudio) dans un MP4 (H.264 vidéo + LPCM audio). **Encodage sur un THREAD
// de fond** : la boucle de rendu pousse les trames capturées dans une file (rapide) et
// un worker encode en arrière-plan → l'application reste FLUIDE pendant la capture
// (l'encodeur H.264 est lourd). Fonctionne pour toute app NKCanvas (`CaptureToImage`)
// ou NKRHI/NKRenderer (`NkOffscreenTarget::ReadbackPixels`).
//
// Boucle type :
//   rec.Begin("capture.mp4", w, h, fps); int a = rec.AddAudio(48000, 2, "fre");
//   pour chaque frame : <rendu+capture> ; rec.PushVideo(pixels, RGBA32) ; rec.PushAudio(a, mix, n) ;
//   rec.End();   // draine la file + finalise (moov) — TOUJOURS un fichier lisible
//
// Gère le retournement vertical (framebuffers bottom-up OpenGL). Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMedia/Video/NkVideoTypes.h" // NkVideoInputFormat
#include "NKMedia/Codecs/Video/H264/NkH264Encoder.h"
#include "NKThreading/NkThread.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkSemaphore.h"

namespace nkentseu {
	namespace media {

		struct NkVideoRecorder {
			public:
				// Démarre un enregistrement MP4 (H.264) + lance le thread d'encodage.
				bool Begin(const char *path, int32 width, int32 height, int32 fpsNum = 60, int32 fpsDen = 1,
						   int32 qp = 24);

				// Pistes audio PCM — plusieurs = choix de LANGUE. À appeler juste après Begin (avant les trames).
				int32 AddAudio(int32 sampleRate, int32 channels, const char *lang3 = nullptr);
				// Pistes de SOUS-TITRES (langue libre). À appeler juste après Begin.
				int32 AddSubtitleTrack(const char *lang3 = nullptr);

				// Pousse une trame capturée (mise en file, encodée en fond). `flipVertical` pour framebuffer
				// bottom-up (OpenGL). Formats : RGBA32 (capture DX11), RGB24, BGR24.
				bool PushVideo(const uint8 *pixels, NkVideoInputFormat fmt, bool flipVertical = false);
				// Pousse des trames audio capturées (int16 entrelacé) sur la piste `trackIdx`.
				void PushAudio(int32 trackIdx, const int16 *interleaved, uint32 frames);
				// Ajoute un sous-titre (mis en file, appliqué dans l'ordre par le worker).
				void AddSubtitle(int32 trackIdx, const char *utf8, uint32 startMs, uint32 durMs);

				// Draine la file, encode le reste, finalise (moov) et ferme. Toujours un MP4 lisible.
				bool End();

				bool IsRecording() const {
					return mOpen;
				}
				int32 FrameCount() const {
					return mPushed; // trames mises en file (compteur côté producteur)
				}

			private:
				// Un élément de la file d'encodage. type : 0=vidéo, 1=audio, 2=stop, 3=sous-titre.
				struct Item {
						int32 type = 0;
						NkVector<uint8> data; // pixels (vidéo) / pcm (audio) / texte UTF-8 (sous-titre)
						NkVideoInputFormat fmt = NkVideoInputFormat::RGB24;
						int32 track = 0;
						uint32 frames = 0, startMs = 0, durMs = 0;
				};
				void WorkerLoop();

				NkH264Encoder mEnc;
				int32 mWidth = 0, mHeight = 0, mPushed = 0;
				bool mOpen = false;
				NkVector<int32> mAudioCh; // nb de canaux par piste audio (pour bufferiser le PCM)
				NkVector<Item> mQueue;
				uint64 mHead = 0;
				threading::NkMutex mMutex;
				threading::NkSemaphore mSem;
				threading::NkThread mWorker;
		};

	} // namespace media
} // namespace nkentseu
