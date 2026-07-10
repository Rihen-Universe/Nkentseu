// =============================================================================
// NKMedia/Video/NkImageSequenceWriter.h
// -----------------------------------------------------------------------------
// Export d'une SÉQUENCE D'IMAGES (comme Blender : frame_0001.png, frame_0002.png…).
// Chaque trame RVB/RVBA est encodée via un codec image de NKImage (PNG/JPEG/BMP/TGA/
// QOI) et écrite dans un fichier numéroté. Complète NkVideoWriter (fichiers vidéo AVI/
// MOV) par le workflow « rendu → séquence d'images » très utilisé en production.
//
// Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMedia/Video/NkVideoWriter.h" // NkVideoInputFormat

namespace nkentseu {
	namespace media {

		enum class NkImageSeqFormat {
			PNG,  // sans perte
			JPEG, // compressé
			BMP,  // brut
			TGA,  // brut/RLE
			QOI,  // sans perte rapide
		};

		struct NkImageSequenceWriter {
			public:
				// `dir` = dossier de sortie ; `basename` = préfixe (ex. "frame"). Les fichiers seront
				// `dir/basename_0001.png`, etc. `padding` = nombre de chiffres. `quality` pour JPEG.
				bool Open(const char *dir, const char *basename, int32 width, int32 height, NkImageSeqFormat fmt,
						  int32 padding = 4, int32 quality = 90);

				// Écrit la trame suivante (numéro auto-incrémenté).
				bool WriteFrame(const uint8 *pixels, NkVideoInputFormat fmt);

				bool Close() {
					mOpen = false;
					return true;
				}
				int32 FrameCount() const {
					return mFrame;
				}

			private:
				char mDir[512] = {0};
				char mBase[128] = {0};
				const char *mExt = "png";
				int32 mWidth = 0, mHeight = 0, mPadding = 4, mQuality = 90, mFrame = 0;
				NkImageSeqFormat mFmt = NkImageSeqFormat::PNG;
				bool mOpen = false;
		};

	} // namespace media
} // namespace nkentseu
