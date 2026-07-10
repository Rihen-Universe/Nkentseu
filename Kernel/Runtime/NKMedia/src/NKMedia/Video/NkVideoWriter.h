// =============================================================================
// NKMedia/Video/NkVideoWriter.h
// -----------------------------------------------------------------------------
// Création de vidéo from-scratch (SANS ffmpeg) — API haut niveau. On pousse des
// trames RVB/RVBA, le writer les encode (brut BGR ou MJPEG via le codec JPEG de
// NKImage) et les muxe dans un conteneur (AVI pour l'instant ; MP4/WebM à venir).
// Objectif « comme Blender » : rendre des images → produire un fichier vidéo lisible
// par les lecteurs standard (VLC, navigateurs, éditeurs), sans dépendance externe.
//
// Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMedia/Video/NkVideoTypes.h"
#include "NKMedia/Video/Containers/NkAviWriter.h"
#include "NKMedia/Video/Containers/NkMovWriter.h"
#include "NKMedia/Codecs/Video/Mpeg1/NkMpeg1Encoder.h"

namespace nkentseu {
	namespace media {

		// Codec vidéo (from-scratch). Extensible (H264/VP8… = travaux futurs lourds).
		enum class NkVideoCodec {
			RAW_BGR, // non compressé (DIB) — gros fichiers, qualité parfaite
			MJPEG,	 // Motion-JPEG (chaque trame = JPEG, via NKImage) — compressé, universel
			MPEG1,	 // MPEG-1 Video (vrai codec DCT, I-frames) → flux élémentaire .m1v
		};

		// Conteneur de sortie.
		enum class NkVideoContainer {
			AVI,		// RIFF — implémenté (RAW + MJPEG)
			MOV,		// QuickTime/MP4 (ISOBMFF) — implémenté (MJPEG)
			ELEMENTARY, // flux élémentaire brut (MPEG-1 → .m1v)
		};

		// (NkVideoInputFormat est défini dans NkVideoTypes.h.)

		struct NkVideoConfig {
				int32 width = 0;
				int32 height = 0;
				int32 fpsNum = 30; // fps = fpsNum / fpsDen
				int32 fpsDen = 1;
				NkVideoCodec codec = NkVideoCodec::MJPEG;
				NkVideoContainer container = NkVideoContainer::AVI;
				int32 quality = 90; // qualité MJPEG (1..100)
		};

		class NkVideoWriter {
			public:
				// Ouvre le fichier de sortie et écrit les en-têtes.
				bool Open(const char *path, const NkVideoConfig &cfg);

				// Écrit UNE trame. `pixels` = width*height dans `fmt` (haut-en-bas). Encode + muxe.
				bool WriteFrame(const uint8 *pixels, NkVideoInputFormat fmt);

				// Termine (index + tailles) et ferme.
				bool Close();

				bool IsOpen() const {
					return mOpen;
				}
				int32 FrameCount() const {
					if (mCfg.codec == NkVideoCodec::MPEG1)
						return mMpeg1.FrameCount();
					return (mCfg.container == NkVideoContainer::MOV) ? mMov.FrameCount() : mAvi.FrameCount();
				}

			private:
				NkAviWriter mAvi;
				NkMovWriter mMov;
				NkMpeg1Encoder mMpeg1;
				NkVideoConfig mCfg;
				bool mOpen = false;

				// Écrit une trame déjà encodée vers le conteneur actif.
				bool WriteEncoded(const uint8 *data, uint32 size, uint32 chunkKind);

				// Buffers de travail réutilisés entre trames (évite les alloc par frame).
				uint8 *mScratch = nullptr; // conversion BGR / bottom-up
				usize mScratchCap = 0;

				bool EnsureScratch(usize n);
				bool WriteFrameRaw(const uint8 *pixels, NkVideoInputFormat fmt);
				bool WriteFrameMjpeg(const uint8 *pixels, NkVideoInputFormat fmt);
		};

	} // namespace media
} // namespace nkentseu
