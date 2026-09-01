// =============================================================================
// NkEchecsBanc.h — le banc des regles (verdict par code de sortie)
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace jeux {
		namespace echecs {

			/// Rend 0 si tous les cas passent, 1 sinon.
			int32 NkEchecsLancerBanc();

		} // namespace echecs
	} // namespace jeux
} // namespace nkentseu
