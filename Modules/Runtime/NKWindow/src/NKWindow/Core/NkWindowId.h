#pragma once
// =============================================================================
// NkWindowId.h
// Identifiant unique de fenêtre — inclus de NkEvent.h pour éviter la
// duplication de définition.
// =============================================================================


#include "NKMath/NkTypes.h"

namespace nkentseu {

    // =========================================================================
    // Alias de rétrocompatibilité pour noms d'événements anciens
    // =========================================================================

    // =========================================================================
    // NkWindowId — identifiant unique de fenêtre transmis avec chaque événement
    // =========================================================================
    using NkWindowId = NkU64;
    static constexpr NkWindowId NK_INVALID_WINDOW_ID = 0;
} // namespace nkentseu
