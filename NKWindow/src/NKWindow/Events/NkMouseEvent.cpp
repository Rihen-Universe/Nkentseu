// =============================================================================
// NkMouseEvents.cpp
// Implémentation des événements souris.
// Toute la logique est inline dans le .h — ce fichier satisfait l'unité
// de compilation et accueillera les futures implémentations non-inline.
// =============================================================================

#include "NkMouseEvent.h"

namespace nkentseu {
    // Toutes les classes d'événements souris sont entièrement définies (inline)
    // dans NkMouseEvents.h. Ce .cpp existe pour :
    //   1. Satisfaire les systèmes de build.
    //   2. Accueillir à l'avenir des implémentations non-inline si nécessaire
    //      (ex: tables de correspondance lourdes, fonctions utilitaires…).
} // namespace nkentseu