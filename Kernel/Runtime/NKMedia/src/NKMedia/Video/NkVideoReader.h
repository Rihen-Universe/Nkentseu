// =============================================================================
// NKMedia/Video/NkVideoReader.h
// -----------------------------------------------------------------------------
// LECTURE VIDÉO (démux + décodage frame par frame) — le pendant de NkVideoWriter.
// NKMedia savait ENCODER (AVI/MP4/MOV, H264/MJPEG/MPEG1) mais pas LIRE. Ce module
// ouvre un fichier vidéo et en sort les images décodées en RGBA8, une par une :
//   - Conteneur AVI  : chunks 'movi' -> MJPEG (décodé via NKImage) ou RGB brut.
//   - Conteneur MOV/MP4 (ISOBMFF) : piste vidéo -> MJPEG (mjpa/jpeg). H264 = à venir.
//   - Séquence d'images : un DOSSIER de PNG/JPEG/BMP joué comme une vidéo.
// H264/AVC (MP4 courant) exige un décodeur complet (gros chantier séparé) : la
// lecture d'une piste avc1 renvoie une erreur claire tant que le décodeur n'existe pas.
// Zero-STL, namespace nkentseu::media. From-scratch.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace media {

		// Une image décodée : pixels RGBA8 (width*height*4, row-major, haut->bas).
		struct NkVideoFrame {
				NkVector<nk_uint8> rgba;
				int32 width = 0;
				int32 height = 0;
				int64 timestampMs = 0;
				int32 index = -1;
		};

		// Métadonnées de la vidéo ouverte.
		struct NkVideoReaderInfo {
				int32 width = 0;
				int32 height = 0;
				int32 frameCount = -1; // -1 = inconnu
				double fps = 0.0;
				NkString codec;		// "mjpeg" | "rawrgb" | "h264" | "image"
				NkString container; // "avi" | "mov" | "mp4" | "sequence"
		};

		class NkVideoReader {
			public:
				NkVideoReader();
				~NkVideoReader();

				NkVideoReader(const NkVideoReader &) = delete;
				NkVideoReader &operator=(const NkVideoReader &) = delete;

				// Ouvre un fichier (.avi/.mov/.mp4) OU un dossier de séquence d'images.
				// Renvoie false si le format n'est pas (encore) lisible (ex. H264).
				bool Open(const char *path);

				bool IsOpen() const;
				const NkVideoReaderInfo &Info() const;

				// Lit la PROCHAINE image (séquentiel). false quand la vidéo est finie.
				bool ReadFrame(NkVideoFrame &out);

				// Positionne la lecture sur l'image `index` (conteneurs à index : AVI/MOV/séquence).
				bool SeekFrame(int32 index);

				int32 CurrentIndex() const;
				void Close();

				// Auto-test headless : écrit un petit AVI MJPEG (via NkVideoWriter) puis le relit
				// et vérifie dimensions + nombre d'images + plausibilité des pixels.
				static bool SelfTest();

			private:
				struct Impl;
				Impl *mImpl = nullptr;
		};

	} // namespace media
} // namespace nkentseu
