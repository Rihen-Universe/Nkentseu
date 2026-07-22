// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkTop.h
// -----------------------------------------------------------------------------
// Décodeur Opus/SILK — TOP-LEVEL paquet (RFC 6716 §4.2.1 / libopus dec_API.c).
// Lit l'en-tête de la couche LP (flags VAD par trame + flag LBRR — ×2 canaux en
// stéréo) puis décode chaque trame SILK du paquet (condCoding : 1re trame
// indépendante, suivantes conditionnelles) → PCM au débit INTERNE (8/12/16 kHz).
// STÉRÉO : poids de prédiction mid/side (stereo_decode_pred), flag mid-only quand
// le VAD side est nul, reset du canal side à la reprise, conversion MS→LR à
// interpolation de prédicteurs sur 8 ms (stereo_MS_to_LR, tampons 2 échantillons).
// Le rééchantillonnage vers 48 kHz est une étape ultérieure. Zero-STL.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NkSilkDecoder.h"
#include "NKMedia/Codecs/Opus/NkOpusRange.h"

namespace nkentseu {
	namespace media {

		struct NkSilkTop {
				NkSilkDecoder dec;	   // canal mid (ou mono) — état persistant (inter-paquets)
				NkSilkDecoder decSide; // canal side (stéréo seulement)
				// État stéréo (stereo_dec_state de libopus).
				int32 predPrevQ13[2] = {0, 0};
				int16 sMid[2] = {0, 0};
				int16 sSide[2] = {0, 0};
				int32 prevDecodeOnlyMiddle = 0;
				int32 stereo = 0;

				void Init(int32 fs_kHz, int32 nb_subfr) {
					Init(fs_kHz, nb_subfr, 1);
				}
				void Init(int32 fs_kHz, int32 nb_subfr, int32 nChannels);

				// Décode un paquet SILK (nFramesPerPacket trames) → PCM interne.
				// MONO : out = trames concaténées (frame_length échantillons chacune) ;
				// renvoie le nombre d'échantillons écrits.
				// STÉRÉO : out = par trame, (frame_length+2) échantillons GAUCHE puis
				// (frame_length+2) DROITE — les 2 premiers de chaque bloc sont le tampon
				// inter-trames (convention libopus : le resampler amont lit &x[1]) ;
				// renvoie le nombre total d'int16 écrits.
				// ⚠️ Suppose un flux SANS FEC (LBRR = 0).
				int32 DecodePacket(NkOpusRangeDecoder &rd, int32 nFramesPerPacket, int16 *out);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
