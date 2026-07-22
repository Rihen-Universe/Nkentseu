// =============================================================================
// NKMedia/Codecs/Opus/NkOpusDecoder.h
// -----------------------------------------------------------------------------
// Décodeur Opus complet (RFC 6716) — DISPATCHER. Analyse le TOC d'un paquet puis
// route chaque trame Opus vers le bon décodeur :
//   - CELT-only (config 16-31) → NkCeltDecoder (48 kHz natif) ;
//   - SILK-only (config 0-11)  → NkSilkTop + NkSilkResampler (interne → 48 kHz) ;
//   - Hybride (config 12-15)   → SILK bande basse + CELT bande haute (même range decoder).
// Sortie PCM int16 à 48 kHz, entrelacée L R L R… en stéréo (DecodePacket renvoie le
// nombre TOTAL de valeurs int16 écrites : échantillons × canaux). Zero-STL.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NkOpusPacket.h"
#include "Silk/NkSilkTop.h"
#include "Silk/NkSilkResampler.h"
#include "Celt/NkCeltDecoder.h"

namespace nkentseu {
	namespace media {

		struct NkOpusDecoder {
				NkSilkTop mSilk;
				NkSilkResampler mResampler;		// canal gauche (ou mono)
				NkSilkResampler mResamplerSide; // canal droit (stéréo)
				NkCeltDecoder mCelt;
				int16 mSMid[2] = {0, 0};
				int32 mSilkFsKHz = 0; // 0 = SILK pas encore configuré
				int32 mSilkNbSubfr = 0;
				int32 mSilkInternalCh = 0; // canaux internes SILK du dernier paquet (TOC)
				int32 mChannels = 1;
				int32 mPrevMode = -1; // mode de la trame précédente (reset CELT au changement)

				void Init(int32 channels);

				// Décode un paquet Opus → PCM int16 48 kHz (entrelacé si stéréo).
				// Renvoie le nombre total de valeurs int16 écrites (échantillons × canaux).
				int32 DecodePacket(const uint8 *data, int32 len, int16 *out48);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
