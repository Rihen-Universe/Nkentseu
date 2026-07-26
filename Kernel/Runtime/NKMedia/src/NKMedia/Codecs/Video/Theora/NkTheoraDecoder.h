// =============================================================================
// NKMedia/Codecs/Video/Theora/NkTheoraDecoder.h
// -----------------------------------------------------------------------------
// Décodeur vidéo Theora (Xiph.org, basé VP3) — FROM SCRATCH, zero-STL,
// namespace nkentseu::media. Écrit intégralement à la main depuis la
// spécification Theora I (Xiph.Org Foundation, 3 juin 2017). AUCUN code de
// libtheora / ffmpeg n'a été copié : ces outils servent uniquement d'ORACLE
// pour générer des flux de test et comparer les pixels décodés.
//
// Couverture : conteneur Ogg (pages/paquets/lacing, paquets multi-pages),
// en-tête d'identification (0x80), en-tête commentaire (0x81, ignoré),
// en-tête setup (0x82 : limites de filtre de boucle, paramètres de
// quantification + matrices de base, arbres de Huffman des tokens DCT),
// décodage d'une trame (en-tête, drapeaux de blocs codés, modes de macrobloc,
// vecteurs de mouvement, qi par bloc, tokens DCT DC/AC, dé-prédiction DC,
// déquantification, iDCT 8x8 VP3 entière bit-exacte, prédiction intra +
// compensation de mouvement pleine/demi-pel, filtre de boucle VP3). Pixels
// YUV 4:2:0 / 4:2:2 / 4:4:4.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace media {

		// Une trame décodée, ROGNÉE à la région d'affichage (picture region) et
		// orientée TOP-DOWN (ligne 0 = haut), comme la sortie `yuv420p` de ffmpeg.
		// Les plans chroma sont sous-échantillonnés selon le format de pixel.
		struct NkTheoraFrame {
				NkVector<uint8> y, cb, cr;
				int32 width = 0, height = 0;			 // dims luma affichées (picture region)
				int32 chromaWidth = 0, chromaHeight = 0; // dims chroma affichées
				bool isKeyFrame = false;
				int64 frameIndex = -1; // index d'affichage (0-based)
		};

		// Décodeur Theora complet. Cycle : Open(buffer) → boucle DecodeNextFrame.
		// L'état (trames de référence golden/previous, contexte) est persistant.
		struct NkTheoraDecoder {
			public:
				NkTheoraDecoder();
				~NkTheoraDecoder();

				// Détection : le buffer est un flux Ogg dont un flux logique est Theora.
				static bool Probe(const uint8 *data, usize size);

				// Parse le conteneur Ogg + les 3 en-têtes Theora. `data` doit rester
				// valide pendant toute la durée de vie (les paquets sont copiés en
				// interne, donc en réalité on peut libérer `data` après Open).
				bool Open(const uint8 *data, usize size, NkString *outError = nullptr);

				// Décode la prochaine trame (dans l'ordre d'affichage = ordre de
				// décodage, Theora n'a pas de réordonnancement). Renvoie false quand
				// il n'y a plus de paquet de données (fin de flux) ou sur erreur.
				bool DecodeNextFrame(NkTheoraFrame &out, NkString *outError = nullptr);

				bool HasMoreFrames() const;

				int32 FrameWidthMBs() const { return mFmbw; }
				int32 FrameHeightMBs() const { return mFmbh; }
				int32 PictureWidth() const { return mPicW; }
				int32 PictureHeight() const { return mPicH; }
				int32 PixelFormat() const { return mPf; } // 0=4:2:0, 2=4:2:2, 3=4:4:4

			private:
				struct Impl;
				Impl *mImpl = nullptr;

				int32 mFmbw = 0, mFmbh = 0;
				int32 mPicW = 0, mPicH = 0, mPicX = 0, mPicY = 0;
				int32 mPf = 0;
		};

	} // namespace media
} // namespace nkentseu
