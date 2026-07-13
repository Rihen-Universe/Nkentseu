// =============================================================================
// NKMedia/Codecs/Video/H264/NkH264Decoder.h
// -----------------------------------------------------------------------------
// DÉCODEUR H.264 (AVC) — BRIQUE 1 : découpage NAL (Annex-B) + parsing SPS/PPS.
// Le pendant de NkH264Encoder. Un décodeur H.264 baseline complet est un GROS
// chantier (slice header, CAVLC, prédictions intra 4x4/16x16 + inter/MC, IDCT,
// déblocage, gestion des références) réparti sur plusieurs sessions. Cette 1re
// brique lit déjà la STRUCTURE du bitstream : elle isole les unités NAL, retire
// l'anti-émulation (00 00 03), et décode le SPS (profil, niveau, dimensions) via
// Exp-Golomb — la fondation indispensable de tout décodeur. Zero-STL, nkentseu::media.
//
// ÉTAT : parsing SPS ✅ (dimensions/profil). Décodage des IMAGES = à venir.
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace media {

		// Une unité NAL isolée (pointe dans le buffer source ; ne copie pas).
		struct NkH264Nal {
				usize offset = 0; // position de l'octet d'en-tête NAL dans le buffer
				usize size = 0;	  // taille (en-tête + RBSP, hors start code)
				int32 type = 0;	  // nal_unit_type (7=SPS, 8=PPS, 5=IDR, 1=non-IDR, …)
				int32 refIdc = 0;
		};

		// Infos extraites d'un SPS.
		struct NkH264Sps {
				bool valid = false;
				int32 profileIdc = 0;
				int32 levelIdc = 0;
				int32 width = 0;  // en pixels (après cropping, hypothèse 4:2:0)
				int32 height = 0; // en pixels
				int32 numRefFrames = 0;
		};

		class NkH264Decoder {
			public:
				// Découpe un flux Annex-B (start codes 00 00 01 / 00 00 00 01) en unités NAL.
				static void SplitNalsAnnexB(const uint8 *data, usize size, NkVector<NkH264Nal> &out);

				// Décode un SPS depuis une unité NAL (type 7). data/size = l'unité NAL complète
				// (en-tête inclus). Retire l'anti-émulation et lit les champs Exp-Golomb.
				static bool ParseSps(const uint8 *nal, usize size, NkH264Sps &out);

				// Auto-test headless : parse un SPS baseline 176x144 connu et vérifie profil+dimensions.
				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
