#pragma once
// =============================================================================
// Noge/ECS/NkEcsUtil.h — petits utilitaires partagés de la couche ECS Noge.
// =============================================================================
#include <cstring>

namespace nkentseu {

	// Comparaison d'egalite de deux chaines C (case-sensitive). nullptr-safe.
	// Remplace l'ancien helper NkStrEqual (qui n'existait plus). Source unique.
	inline bool NkStrEqual(const char *a, const char *b) noexcept {
		if (a == b)
			return true;
		if (!a || !b)
			return false;
		return std::strcmp(a, b) == 0;
	}

} // namespace nkentseu
