// =============================================================================
// NKMedia/Codecs/Video/VP8/NkVp8Decoder.h
// -----------------------------------------------------------------------------
// Décodeur vidéo VP8 (RFC 6386) — from-scratch, zero-STL, namespace nkentseu::media.
// ⚠️ CHANTIER EN COURS (démarré 2026-07-21) : construit brique par brique, comme le
// décodeur H.264 (voir Codecs/Video/H264/). État actuel : brique 1 (décodeur booléen,
// NkVp8BoolDecoder.h) + brique 2 (cette brique : en-tête non compressé — "frame tag" —
// et démarrage du décodeur booléen sur la 1ère partition). Reste : en-tête compressé
// complet (segmentation, filtre de boucle, quantification, probabilités de token/mv —
// §9), prédiction intra (16x16/4x4/chroma), prédiction inter (vecteurs de mouvement +
// interpolation sous-pel 6-tap), transformée (WHT DC + IDCT 4x4), filtre de boucle.
// Ne PAS prétendre décoder d'images tant que ces briques ne sont pas livrées.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		// §9.1 : en-tête NON compressé (3 octets communs + 7 de plus si key_frame).
		// ⚠️ Convention RFC : le bit 0 du "frame tag" vaut 0 pour une image CLÉ (inversé
		// par rapport à l'intuition) — `keyFrame` ci-dessous est déjà normalisé (true =
		// image clé).
		struct NkVp8FrameTag {
				bool keyFrame = false;
				int32 version = 0;
				bool showFrame = false;
				uint32 firstPartSize = 0; // taille en octets de la 1ère partition (compressée)
				// Présents uniquement si keyFrame :
				int32 width = 0, height = 0;
				int32 horizScale = 0, vertScale = 0; // 2 bits chacun (§9.1, mise à l'échelle d'affichage)
				usize headerSize = 0;				  // taille totale de CET en-tête non compressé (3 ou 10 octets)
		};

		// Parse le frame tag (en-tête non compressé) en tête de CHAQUE frame VP8. Ne touche
		// PAS au décodeur booléen (lecture directe d'octets, little-endian sur 24/16 bits).
		// Renvoie false si trop court ou (keyFrame) si le start code 0x9d 0x01 0x2a est absent.
		inline bool NkVp8ParseFrameTag(const uint8 *data, usize size, NkVp8FrameTag &out) {
			if (size < 3)
				return false;
			const uint32 tmp = (uint32)data[0] | ((uint32)data[1] << 8) | ((uint32)data[2] << 16);
			out.keyFrame = (tmp & 0x1u) == 0; // 0 = clé (§9.1)
			out.version = (int32)((tmp >> 1) & 0x7u);
			out.showFrame = ((tmp >> 4) & 0x1u) != 0;
			out.firstPartSize = (tmp >> 5) & 0x7FFFFu; // 19 bits
			if (!out.keyFrame) {
				out.headerSize = 3;
				return true;
			}
			if (size < 10)
				return false;
			if (data[3] != 0x9D || data[4] != 0x01 || data[5] != 0x2A)
				return false; // start code absent -> pas un flux VP8 clé valide
			const uint32 w16 = (uint32)data[6] | ((uint32)data[7] << 8);
			const uint32 h16 = (uint32)data[8] | ((uint32)data[9] << 8);
			out.width = (int32)(w16 & 0x3FFFu);
			out.horizScale = (int32)(w16 >> 14);
			out.height = (int32)(h16 & 0x3FFFu);
			out.vertScale = (int32)(h16 >> 14);
			out.headerSize = 10;
			return true;
		}

	} // namespace media
} // namespace nkentseu
