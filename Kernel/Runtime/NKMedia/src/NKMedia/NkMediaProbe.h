// =============================================================================
// NKMedia/NkMediaProbe.h
// -----------------------------------------------------------------------------
// Brique 1 de NKMedia : DÉTECTION DE CONTENEUR + DÉMUX D'EN-TÊTE. Identifie le
// format conteneur (MP4/ISOBMFF, WebM/Matroska, WAV, OGG, MP3, FLAC) et liste les
// PISTES avec leur CODEC et leurs paramètres (audio : Hz/canaux ; vidéo : W×H).
// Parseurs from-scratch : boîtes ISOBMFF (MP4) et éléments EBML (Matroska/WebM).
// Zero-STL, namespace nkentseu::media. Ne DÉCODE pas encore (voir ROADMAP briques 2+).
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace media {

		enum class NkMediaContainer {
			NK_UNKNOWN,
			NK_MP4,	 // ISOBMFF (mp4/m4a/mov)
			NK_WEBM, // Matroska/WebM (EBML)
			NK_WAV,
			NK_OGG,
			NK_MP3,
			NK_FLAC
		};

		enum class NkMediaTrackType { NK_UNKNOWN, NK_AUDIO, NK_VIDEO };

		// Une piste (audio ou vidéo) décrite par l'en-tête du conteneur.
		struct NkMediaTrack {
				NkMediaTrackType type = NkMediaTrackType::NK_UNKNOWN;
				NkString codec;			 // "aac","opus","vorbis","vp8","vp9","h264","pcm","mp3",...
				int32 sampleRate = 0;	 // audio (Hz)
				int32 channels = 0;		 // audio
				int32 width = 0;		 // vidéo
				int32 height = 0;		 // vidéo
		};

		struct NkMediaInfo {
				NkMediaContainer container = NkMediaContainer::NK_UNKNOWN;
				NkVector<NkMediaTrack> tracks;

				const char *ContainerName() const;
				// Première piste audio (nullptr si aucune).
				const NkMediaTrack *FirstAudio() const;
				const NkMediaTrack *FirstVideo() const;
		};

		struct NkMediaProbe {
			public:
				// Analyse un buffer déjà en mémoire (l'en-tête suffit ; le buffer complet est mieux).
				static bool Probe(const uint8 *data, usize size, NkMediaInfo &out);
				// Analyse un fichier (lecture disque via NKFileSystem).
				static bool ProbeFile(const char *path, NkMediaInfo &out);

				// Détection rapide du conteneur (magic bytes) sans démux complet.
				static NkMediaContainer DetectContainer(const uint8 *data, usize size);

				// Auto-test headless (magies + varints EBML).
				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
