// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKMath/NkSpecialFunctions.h
// (19/10/2026) Les fonctions speciales qui manquaient a NKMath : la fonction d'erreur erf et sa
// complementaire erfc, en double precision, SANS la bibliotheque standard (regle de Rodolf du 19/10 :
// « si nos equivalences ont des limites, alors les rendre plus performantes » ; un equivalent qui manque
// s'ecrit dans son module). Premier usage : la loi normale de Kernel/AI (NkDissimulation, NkLoiNormale).
//
// Methode : pour |z| < 2, la serie entiere de erf (erf(z) = 2/√π Σ (-1)^n z^(2n+1) / (n! (2n+1))) ; au-dela,
// la fraction continue erfc(z) = e^(-z²)/√π · 1/(z + (1/2)/(z + 1/(z + (3/2)/(z + ...)))), evaluee par la
// methode de Lentz (arret a 1e-16 pres). Banc : tests/test_fonctions_speciales.cpp (valeurs connues,
// comparaison a la bibliotheque standard dans le banc seulement, et vitesse).
// =============================================================================

#pragma once

#ifndef NKENTSEU_MATH_NKSPECIALFUNCTIONS_H
#define NKENTSEU_MATH_NKSPECIALFUNCTIONS_H

#include "NKMath/NkMathApi.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace math {

		/// La fonction d'erreur erf(x) = 2/√π ∫0..x e^(-t²) dt.
		NKENTSEU_MATH_API float64 NkErf(float64 x) noexcept;
		/// La fonction d'erreur complementaire erfc(x) = 1 - erf(x), exacte aussi pour x grand (pas de
		/// soustraction qui perdrait les petites valeurs).
		NKENTSEU_MATH_API float64 NkErfc(float64 x) noexcept;
		NKENTSEU_MATH_API float32 NkErf(float32 x) noexcept;
		NKENTSEU_MATH_API float32 NkErfc(float32 x) noexcept;

	} // namespace math
} // namespace nkentseu

#endif // NKENTSEU_MATH_NKSPECIALFUNCTIONS_H
