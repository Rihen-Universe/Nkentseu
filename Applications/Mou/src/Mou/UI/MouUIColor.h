// =============================================================================
// UI/MouUIColor.h
// Palette "cartoon flat chaleureux" pour Mou (jeux maternelle 3-5 ans).
// Drop-in equivalent de NkoungUIColor.h, mais pensee enfant :
// fond clair, couleurs vives non-neon, contour brun doux, zero degrade.
// =============================================================================
#pragma once

#include "NKMath/NkColor.h"

namespace mou::ui {

using namespace nkentseu;
using namespace nkentseu::math;

// ============================================================================
// Couleurs Nkana — voir DESIGN_STYLE_GUIDE.md
// ============================================================================
class MouUIColor {
public:
    // ------------------------------------------------------------------------
    // Fonds & surfaces (clairs et chauds — jamais de noir)
    // ------------------------------------------------------------------------
    static constexpr NkColor BG_CREAM()  { return { 255, 246, 229, 255 }; }  // #FFF6E5 fond principal
    static constexpr NkColor BG_SKY()    { return { 174, 227, 255, 255 }; }  // #AEE3FF fond alternatif
    static constexpr NkColor SURFACE()   { return { 255, 255, 255, 255 }; }  // #FFFFFF carte/surface

    // ------------------------------------------------------------------------
    // Couleur heros + accents (vives mais pas neon)
    // ------------------------------------------------------------------------
    static constexpr NkColor SUNNY()  { return { 255, 201,  60, 255 }; }  // #FFC93C jaune soleil (heros)
    static constexpr NkColor CORAL()  { return { 255, 107, 107, 255 }; }  // #FF6B6B corail (rouge)
    static constexpr NkColor LEAF()   { return {  81, 207, 102, 255 }; }  // #51CF66 vert pousse
    static constexpr NkColor SKYB()   { return {  77, 171, 247, 255 }; }  // #4DABF7 bleu ciel
    static constexpr NkColor GRAPE()  { return { 177, 151, 252, 255 }; }  // #B197FC raisin
    static constexpr NkColor ORANGE() { return { 255, 146,  43, 255 }; }  // #FF922B orange
    static constexpr NkColor CHEEK()  { return { 255, 143, 163, 255 }; }  // #FF8FA3 rose joues

    // ------------------------------------------------------------------------
    // Recompense & statut (PAS de "rouge erreur" — aucun echec dans Nkana)
    // ------------------------------------------------------------------------
    static constexpr NkColor STAR_GOLD() { return { 255, 212,  59, 255 }; }  // #FFD43B etoile
    static constexpr NkColor SUCCESS()   { return {  81, 207, 102, 255 }; }  // #51CF66 reussite

    // ------------------------------------------------------------------------
    // Contour (encre) & texte
    // ------------------------------------------------------------------------
    static constexpr NkColor INK()           { return {  74,  55,  40, 255 }; }  // #4A3728 contour brun doux
    static constexpr NkColor TEXT_DARK()     { return {  74,  55,  40, 255 }; }  // texte sur fond clair
    static constexpr NkColor TEXT_ON_COLOR() { return { 255, 255, 255, 255 }; }  // texte sur aplat colore

    // ------------------------------------------------------------------------
    // Couleurs "tri" associees a un symbole (accessibilite daltonienne)
    //   rouge=coeur  bleu=rond  jaune=etoile  vert=feuille
    // ------------------------------------------------------------------------
    static constexpr NkColor SORT_RED()    { return CORAL(); }
    static constexpr NkColor SORT_BLUE()   { return SKYB();  }
    static constexpr NkColor SORT_YELLOW() { return SUNNY(); }
    static constexpr NkColor SORT_GREEN()  { return LEAF();  }
};

// ----------------------------------------------------------------------------
// Helpers (identiques a Nkoung pour coherence)
// ----------------------------------------------------------------------------
inline NkColor WithAlpha(const NkColor& color, uint8 alpha) noexcept {
    return { color.r, color.g, color.b, alpha };
}

inline NkColor LerpColor(const NkColor& from, const NkColor& to, float32 t) noexcept {
    t = (t < 0.f) ? 0.f : (t > 1.f) ? 1.f : t;
    return {
        static_cast<uint8>(from.r + (to.r - from.r) * t),
        static_cast<uint8>(from.g + (to.g - from.g) * t),
        static_cast<uint8>(from.b + (to.b - from.b) * t),
        static_cast<uint8>(from.a + (to.a - from.a) * t)
    };
}

}  // namespace mou::ui
