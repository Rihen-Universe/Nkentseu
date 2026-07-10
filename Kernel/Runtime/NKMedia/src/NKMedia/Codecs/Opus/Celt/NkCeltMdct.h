// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltMdct.h
// -----------------------------------------------------------------------------
// Décodeur Opus/CELT — MDCT / IMDCT (transformée en cosinus modifiée). L'IMDCT est
// la dernière étape de CELT (fréquence → temps), suivie de l'overlap-add fenêtré.
// Implémentation directe O(N²) (correcte, optimisable en FFT plus tard) + fenêtre
// sinus (Princen-Bradley) → reconstruction parfaite (TDAC) prouvée en self-test.
// Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		struct NkCeltMdct {
			public:
				// MDCT : 2N échantillons temps `in` → N coefficients fréquence `out`.
				static void Forward(const float32 *in, int32 N, float32 *out);
				// IMDCT : N coefficients `in` → 2N échantillons temps `out` (à fenêtrer + overlap-add).
				static void Inverse(const float32 *in, int32 N, float32 *out);

				// Fenêtre sinus de longueur 2N (analyse = synthèse, satisfait Princen-Bradley).
				static void SineWindow(int32 N, float32 *out2N);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
