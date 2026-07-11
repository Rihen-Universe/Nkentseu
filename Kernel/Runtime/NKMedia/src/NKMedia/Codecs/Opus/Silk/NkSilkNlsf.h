// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkNlsf.h
// -----------------------------------------------------------------------------
// Décodeur Opus/SILK — brique NLSF (RFC 6716 §4.2.7.5). Port fidèle de libopus
// (NLSF_decode.c, NLSF_unpack.c, NLSF_stabilize.c, decode_indices.c section LSF).
// Décode le vecteur NLSF Q15 depuis les index range-codés : étage-1 VQ (CB1) +
// résidu prédictif étage-2 (dérivé du range coder par contexte) + poids inverses
// + stabilisation (ordre + distance min). Puis conversion LPC via NkSilkLpc.
// Tables codebook NB/MB + WB générées depuis la référence (bit-exactes).
// Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMedia/Codecs/Opus/NkOpusRange.h"

namespace nkentseu {
	namespace media {

		// POD miroir de silk_NLSF_CB_struct (libopus).
		struct NkSilkNlsfCB {
				int16 nVectors;
				int16 order;
				int16 quantStepSize_Q16;
				int16 invQuantStepSize_Q6;
				const uint8 *CB1_NLSF_Q8;
				const int16 *CB1_Wght_Q9;
				const uint8 *CB1_iCDF;
				const uint8 *pred_Q8;
				const uint8 *ec_sel;
				const uint8 *ec_iCDF;
				const uint8 *ec_Rates_Q5;
				const int16 *deltaMin_Q15;
		};

		// Codebooks NLSF (définis dans NkSilkNlsfTables.cpp, générés depuis libopus).
		extern const NkSilkNlsfCB kNlsfCB_NB_MB; // order 10 (NB/MB)
		extern const NkSilkNlsfCB kNlsfCB_WB;	 // order 16 (WB)

		struct NkSilkNlsf {
				// Décode le chemin d'index NLSF (CB1 + résidus étage-2) depuis le range
				// decoder → NLSFIndices[ order + 1 ]. signalType ∈ {0,1,2}.
				static void DecodeIndices(NkOpusRangeDecoder &dec, int8 *NLSFIndices, const NkSilkNlsfCB *cb,
										  int32 signalType);

				// Reconstruit le vecteur NLSF Q15 [order] depuis NLSFIndices + codebook.
				static void Decode(int16 *pNLSF_Q15, const int8 *NLSFIndices, const NkSilkNlsfCB *cb);

				// Stabilise un vecteur NLSF (ordre croissant + distance min). [order]
				static void Stabilize(int16 *NLSF_Q15, const int16 *NDeltaMin_Q15, int32 L);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
