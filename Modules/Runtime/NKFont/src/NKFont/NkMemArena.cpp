/**
 * @File    NkMemArena.cpp
 * @Brief   Implémentation de NkMemArena.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Notes
 *  Tout est inline dans le .h (AllocRaw, Init, Reset, etc.).
 *  Ce .cpp existe pour :
 *    1. Forcer l'instanciation des symboles exportés sur MSVC (/W4).
 *    2. Fournir un point d'entrée pour de futurs diagnostics ou
 *       une instrumentation de profiling (ex: tracking du peak usage).
 */

#include "pch.h"
#include "NKFont/NkMemArena.h"

namespace nkentseu {

    // Tout est défini inline dans NkMemArena.h.
    // Aucune implémentation hors-ligne requise.
    //
    // Point d'extension futur :
    //   - Ajout d'un callback OnOutOfMemory(usize requested, usize available)
    //   - Tracking statistique (peak_usage, alloc_count) en mode debug
    //   - Poison des zones libérées en mode ASAN

} // namespace nkentseu
