// =============================================================================
// NKMedia/Codecs/Opus/NkOpusPacket.h
// -----------------------------------------------------------------------------
// Décodeur Opus (RFC 6716) — ÉTAPE 1 : structure des paquets. Analyse l'octet TOC
// (mode SILK/Hybrid/CELT, bande passante, durée de trame, mono/stéréo) et découpe
// le paquet en TRAMES (codes 0-3, VBR/CBR, padding). C'est le préalable au décodage
// entropique (range decoder, étape 2) puis CELT/SILK. Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		enum class NkOpusMode { NK_SILK_ONLY, NK_HYBRID, NK_CELT_ONLY };
		enum class NkOpusBandwidth { NK_NB, NK_MB, NK_WB, NK_SWB, NK_FB }; // 4/6/8/12/20 kHz

		struct NkOpusFrame {
				usize offset = 0; // position de la trame dans le paquet
				usize size = 0;	  // taille (octets)
		};

		struct NkOpusPacketInfo {
				int32 config = 0; // 0..31 (haut de l'octet TOC)
				bool stereo = false;
				NkOpusMode mode = NkOpusMode::NK_CELT_ONLY;
				NkOpusBandwidth bandwidth = NkOpusBandwidth::NK_FB;
				float32 frameSizeMs = 20.0f;
				int32 frameCount = 0;
				NkOpusFrame frames[48];
		};

		struct NkOpusPacket {
			public:
				// Analyse un paquet Opus (data/len). Renvoie false si malformé.
				static bool Parse(const uint8 *data, usize len, NkOpusPacketInfo &out);

				// Auto-test headless (table de config + découpage codes 0-3 synthétiques).
				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
