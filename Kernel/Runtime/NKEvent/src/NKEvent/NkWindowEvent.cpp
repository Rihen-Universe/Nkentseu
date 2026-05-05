#include "pch.h"
// =============================================================================
// NkWindowEvent.cpp
// Implémentation des événements fenêtre (corps vide — toute la logique
// est inline dans le .h, ce fichier satisfait l'unité de compilation).
// =============================================================================

#include "NkWindowEvent.h"

namespace nkentseu {
    // Toutes les classes d'événements fenêtre sont entièrement définies dans le
    // header (méthodes inline / final). Ce .cpp existe pour :
    //   1. Satisfaire les systèmes de build qui exigent une unité de compilation.
    //   2. Accueillir à l'avenir des implémentations non-inline si nécessaire.
} // namespace nkentseu