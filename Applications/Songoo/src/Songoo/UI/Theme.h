#pragma once
// =============================================================================
// Theme.h
// -----------------------------------------------------------------------------
// Palette neon et helpers couleur conformes aux maquettes HTML (docs/).
// Les couleurs sont reprises EXACTEMENT du CSS :root des fichiers HTML.
//
// Source : docs/01_splash_et_menus.html
//   --neon-cyan   : #00F5FF
//   --neon-orange : #FF6B00
//   --neon-gold   : #FFD700
//   --neon-green  : #00FF64
//   --neon-red    : #FF4040
//   --neon-purple : #CC77FF
//   --dark        : #050A14
// =============================================================================

#include "NKMath/NkColor.h"

namespace nkentseu
{
    namespace songoo
    {
        namespace theme
        {

            // ── Palette principale (hex 1:1 avec le CSS) ─────────────────────
            inline math::NkColor Cyan()    { return {   0, 245, 255, 255 }; }  ///< #00F5FF — Joueur 1
            inline math::NkColor Orange()  { return { 255, 107,   0, 255 }; }  ///< #FF6B00 — Joueur 2
            inline math::NkColor Gold()    { return { 255, 215,   0, 255 }; }  ///< #FFD700 — Titres, bonus
            inline math::NkColor Green()   { return {   0, 255, 100, 255 }; }  ///< #00FF64 — Success
            inline math::NkColor Red()     { return { 255,  64,  64, 255 }; }  ///< #FF4040 — Danger
            inline math::NkColor Purple()  { return { 204, 119, 255, 255 }; }  ///< #CC77FF — Chaos
            inline math::NkColor White()   { return { 255, 255, 255, 255 }; }
            inline math::NkColor Dark()    { return {   5,  10,  20, 255 }; }  ///< #050A14 — Fond

            // ── Variantes décoratives ─────────────────────────────────────────
            inline math::NkColor GridLine(){ return {   0, 245, 255,  10 }; }  ///< rgba(0,245,255,0.04)
            inline math::NkColor PanelBG() { return {  10,  18,  40, 220 }; }  ///< Fond panneau semi-transparent

        } // namespace theme

        // ─────────────────────────────────────────────────────────────────────
        // AlphaF — applique un facteur alpha [0..1] a une couleur (par
        // remplacement du canal a, pas multiplication de l'existant).
        // ─────────────────────────────────────────────────────────────────────
        inline math::NkColor AlphaF(math::NkColor c, float a) noexcept
        {
            if (a < 0.0f) a = 0.0f;
            if (a > 1.0f) a = 1.0f;
            c.a = static_cast<uint8_t>(a * 255.0f);
            return c;
        }

    } // namespace songoo
} // namespace nkentseu
