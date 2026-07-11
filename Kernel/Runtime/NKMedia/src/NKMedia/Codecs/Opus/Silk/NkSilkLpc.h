// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkLpc.h
// -----------------------------------------------------------------------------
// Décodeur Opus/SILK — couche MATH LPC (RFC 6716). Port fidèle de libopus :
//   - silk_bwexpander_32 (bwexpander_32.c)
//   - silk_LPC_fit (LPC_fit.c)
//   - silk_LPC_inverse_pred_gain (LPC_inv_pred_gain.c) — test de stabilité
//   - silk_NLSF2A (NLSF2A.c) : NLSF Q15 → coefficients LPC a_Q12 (filtre de
//     blanchiment monique), via table cosinus LSF + convolution polynomiale +
//     ajustement + expansion de bande jusqu'à stabilité.
// Sert de socle à la brique NLSF (décodage codebook → NLSF → LPC). Zero-STL.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		struct NkSilkLpc {
				static constexpr int32 kMaxOrderLpc = 24; // SILK_MAX_ORDER_LPC

				// Expansion de bande d'un filtre AR (sans le 1 de tête), chirp en Q16.
				static void bwexpander32(int32 *ar, int32 d, int32 chirp_Q16);

				// Ajuste les coefficients LPC (domaine QIN) en int16 (domaine QOUT),
				// bornant l'amplitude max pour tenir dans int16 (expansion si besoin).
				static void lpcFit(int16 *a_QOUT, int32 *a_QIN, int32 QOUT, int32 QIN, int32 d);

				// Gain de prédiction inverse (Q30) ; 0 si le filtre est instable.
				static int32 lpcInversePredGain(const int16 *A_Q12, int32 order);

				// NLSF (Q15, [d]) → coefficients LPC a_Q12 (monique, [d]). d = 10 ou 16.
				static void nlsf2a(int16 *a_Q12, const int16 *NLSF, int32 d);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
