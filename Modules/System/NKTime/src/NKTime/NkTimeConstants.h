#pragma once
/**
 * @File    NkTimeConstants.h
 * @Brief   Constantes de conversion temporelle — source de vérité unique.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  Ce fichier centralise TOUTES les constantes de conversion du module NKTime.
 *  Avant cette centralisation, les mêmes valeurs étaient redéfinies dans :
 *    - NkDuration.cpp   (NANOSECONDS_PER_* — namespace scope)
 *    - NkTime.h         (NK_NANOSECONDS_PER_* — membres statiques de NkTime)
 *    - NkTimeSpan.cpp   (NS_PER_* — anonymous namespace)
 *    - NkTimeZone.h     (NK_NS_PER_* — namespace timeconstants)
 *  Toutes ces définitions sont supprimées au profit de ce fichier.
 *
 *  Règles d'utilisation :
 *    - Dans les .h  : inclure NkTimeConstants.h directement.
 *    - Dans les .cpp: l'inclusion transite via le .h correspondant.
 *    - Pas de using namespace nkentseu::time_constants dans les headers publics.
 */

#include "NKCore/NkTypes.h"

namespace nkentseu {
    namespace time {

        // ── Facteurs de conversion montants (ns → unité supérieure) ──────────────

        static constexpr int64 NS_PER_MICROSECOND  = 1'000LL;
        static constexpr int64 NS_PER_MILLISECOND  = 1'000'000LL;
        static constexpr int64 NS_PER_SECOND       = 1'000'000'000LL;
        static constexpr int64 NS_PER_MINUTE       = 60LL  * NS_PER_SECOND;
        static constexpr int64 NS_PER_HOUR         = 3'600LL * NS_PER_SECOND;
        static constexpr int64 NS_PER_DAY          = 86'400LL * NS_PER_SECOND;

        // ── Facteurs de conversion en secondes ───────────────────────────────────

        static constexpr int64 SECONDS_PER_MINUTE  = 60LL;
        static constexpr int64 SECONDS_PER_HOUR    = 3'600LL;
        static constexpr int64 SECONDS_PER_DAY     = 86'400LL;

        // ── Bornes des composants de NkTime ──────────────────────────────────────

        static constexpr int32 HOURS_PER_DAY               = 24;
        static constexpr int32 MINUTES_PER_HOUR            = 60;
        static constexpr int32 SECONDS_PER_MINUTE_I32      = 60;
        static constexpr int32 MILLISECONDS_PER_SECOND     = 1'000;
        static constexpr int32 MICROSECONDS_PER_MILLISECOND= 1'000;
        /// Nanosecondes dans une milliseconde : [0, 999'999]
        static constexpr int32 NANOSECONDS_PER_MILLISECOND = 1'000'000;

    } // namespace time_constants
} // namespace nkentseu