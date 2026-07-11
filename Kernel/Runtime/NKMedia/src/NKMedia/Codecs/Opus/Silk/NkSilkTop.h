// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkTop.h
// -----------------------------------------------------------------------------
// Décodeur Opus/SILK — TOP-LEVEL paquet (RFC 6716 §4.2.1 / libopus dec_API.c).
// Lit l'en-tête de la couche LP (flags VAD par trame + flag LBRR) puis décode
// chaque trame SILK du paquet (condCoding : 1re trame indépendante, suivantes
// conditionnelles) → PCM au débit INTERNE (8/12/16 kHz). Le rééchantillonnage
// vers le débit de sortie (48 kHz) est une étape ultérieure. Mono. Zero-STL.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NkSilkDecoder.h"
#include "NKMedia/Codecs/Opus/NkOpusRange.h"

namespace nkentseu {
	namespace media {

		struct NkSilkTop {
				NkSilkDecoder dec; // état persistant (inter-paquets)

				void Init(int32 fs_kHz, int32 nb_subfr);

				// Décode un paquet SILK (nFramesPerPacket trames) → PCM interne.
				// Renvoie le nombre d'échantillons écrits dans out.
				// ⚠️ Suppose un flux SANS FEC (LBRR = 0).
				int32 DecodePacket(NkOpusRangeDecoder &rd, int32 nFramesPerPacket, int16 *out);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
