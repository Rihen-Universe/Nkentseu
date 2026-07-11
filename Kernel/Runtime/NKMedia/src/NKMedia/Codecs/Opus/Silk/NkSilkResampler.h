// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkResampler.h
// -----------------------------------------------------------------------------
// Décodeur Opus/SILK — RESAMPLER (libopus silk/resampler*.c). Rééchantillonne le
// PCM du débit interne SILK (8/12/16 kHz) vers le débit de sortie (24/48 kHz) :
// upsampling 2x haute qualité (filtres passe-tout) suivi d'une interpolation FIR
// fractionnaire, avec compensation de délai (1 ms). Chemin décodeur uniquement.
// Port bit-exact. Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		struct NkSilkResampler {
				static constexpr int32 kMaxIIROrder = 6;
				static constexpr int32 kMaxFIROrder = 36;
				static constexpr int32 kOrderFIR12 = 8;
				static constexpr int32 kMaxBatchMs = 10;

				int32 sIIR[kMaxIIROrder] = {0};
				int16 sFIR_i16[kMaxFIROrder] = {0};
				int16 delayBuf[96] = {0};
				int32 invRatio_Q16 = 0;
				int32 Fs_in_kHz = 16, Fs_out_kHz = 48, batchSize = 160, inputDelay = 0;
				int32 function = 0; // 0=copy, 1=up2wrapper, 2=IIR_FIR

				// Décodeur : Fs_in ∈ {8000,12000,16000}, Fs_out ∈ {…,24000,48000}.
				void Init(int32 Fs_Hz_in, int32 Fs_Hz_out);
				// Rééchantillonne inLen échantillons → out (inLen*Fs_out/Fs_in échantillons).
				void Process(int16 *out, const int16 *in, int32 inLen);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
