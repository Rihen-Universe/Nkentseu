#pragma once
// =============================================================================
// Theme.h — Palette africaine camerounaise Songo'o
// Copie du theme.h existant dans le repo Nkentseu Songoo
// =============================================================================

#include "NKMath/NkColor.h"

namespace nkentseu { namespace songoo { namespace theme {

    // ── Couleurs de base ──────────────────────────────────────────────────────
    inline math::NkColor Black()      { return {   0,   0,   0, 255 }; }
    inline math::NkColor White()      { return { 255, 255, 255, 255 }; }
    inline math::NkColor Orange()     { return { 255, 107,   0, 255 }; }  // terre cuite
    inline math::NkColor Cyan()       { return {   0, 200, 255, 255 }; }  // accent froid
    inline math::NkColor Gold()       { return { 210, 160,  30, 255 }; }  // or chaud
    inline math::NkColor ForestGreen(){ return {  30, 100,  40, 255 }; }  // vert forêt
    inline math::NkColor DarkBrown()  { return {  90,  45,  10, 255 }; }  // brun cacao
    inline math::NkColor Parchment()  { return { 255, 235, 184, 255 }; }  // parchemin

    // Kente (bandeau décoratif)
    inline math::NkColor Kente0()     { return { 180,  70,  15, 220 }; }
    inline math::NkColor Kente1()     { return { 210, 160,  30, 220 }; }
    inline math::NkColor Kente2()     { return {  30, 100,  40, 220 }; }

}}} // namespace nkentseu::songoo::theme

// ── Helper global (utilisé partout dans le projet) ──────────────────────────
namespace nkentseu { namespace songoo {
    /// Retourne la même couleur avec un alpha modifié (0.f..1.f)
    inline math::NkColor AlphaF(math::NkColor c, float a) {
        c.a = static_cast<uint8_t>(255.f * (a < 0.f ? 0.f : a > 1.f ? 1.f : a));
        return c;
    }
}} // namespace nkentseu::songoo
