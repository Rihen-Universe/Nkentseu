#pragma once
// =============================================================================
// NkUi.h — Tokens de design + helpers de dessin NKGui (reecriture propre de l'UI).
// Source : design system Banani « NKCode IDE ». Palette GitHub-dark + accents.
// Tout est dessine en primitives NKGui -> identique sur toutes les plateformes.
// =============================================================================
#include "NKEditorKit/NkEditorKit.h"

namespace nkentseu {
namespace nkcode {

    using namespace nkentseu;
    using namespace nkentseu::nkgui;

    // ── Palette (tokens Banani) ──────────────────────────────────────────────
    namespace NkCol {
        inline constexpr NkColor background  { 13, 17, 23, 255 };   // #0d1117
        inline constexpr NkColor foreground  { 230, 237, 243, 255 };// #e6edf3
        inline constexpr NkColor border      { 33, 38, 45, 255 };   // #21262d
        inline constexpr NkColor input       { 22, 27, 34, 255 };   // #161b22
        inline constexpr NkColor surface      { 22, 27, 34, 255 };  // #161b22
        inline constexpr NkColor primary     { 15, 115, 213, 255 }; // #0F73D5
        inline constexpr NkColor primaryFg   { 255, 255, 255, 255 };
        inline constexpr NkColor secondary   { 10, 85, 95, 255 };   // #0A555F
        inline constexpr NkColor secondaryFg { 220, 235, 238, 255 };
        inline constexpr NkColor accent      { 247, 154, 40, 255 }; // #F79A28
        inline constexpr NkColor sidebar     { 1, 4, 9, 255 };      // #010409
        inline constexpr NkColor sidebarFg   { 139, 148, 158, 255 };// #8b949e
        inline constexpr NkColor muted       { 33, 38, 45, 255 };   // #21262d
        inline constexpr NkColor mutedFg     { 110, 118, 129, 255 };// #6e7681
        inline constexpr NkColor success     { 63, 185, 80, 255 };  // #3fb950
        inline constexpr NkColor danger      { 248, 81, 73, 255 };  // #f85149
        inline constexpr NkColor hover       { 28, 33, 40, 255 };   // survol discret
    }
    // Rayons (px @1x)
    namespace NkR { inline constexpr float32 sm = 4.f, md = 6.f, lg = 10.f, xl = 16.f; }

    // ── Contexte de dessin partage (passe aux fonctions d'UI) ─────────────────
    struct NkUi {
        NkGuiContext*  ctx = nullptr;
        NkGuiDrawList* dl  = nullptr;
        const NkGuiFont* f = nullptr;
        NkVec2  mp{};
        bool    click = false, down = false;
        float32 S = 1.f;

        static NkUi From(NkEditorFrameContext& ec, bool overlay = false) {
            NkUi u; u.ctx = &ec.Ui(); u.f = u.ctx->font;
            u.dl = overlay ? &u.ctx->dlOverlay : &u.ctx->DL();
            u.mp = u.ctx->input.mousePos; u.click = u.ctx->input.mouseClicked[0];
            u.down = u.ctx->input.mouseDown[0]; u.S = u.ctx->S(1.f);
            return u;
        }
        bool Valid() const { return f && f->Valid(); }
        float32 s(float32 px) const { return px * S; }
        bool Hit(const NkRect& r) const { return NkGuiRectContains(r, mp); }
        float32 Asc() const { return f ? f->Ascent() : 0.f; }
        float32 Lh()  const { return f ? f->LineHeight() : 0.f; }
        float32 TextW(const char* t) const { return f ? f->MeasureWidth(t) : 0.f; }

        void Rect(const NkRect& r, const NkColor& c, float32 round = 0.f) const { dl->AddRectFilled(r, c, round); }
        void Stroke(const NkRect& r, const NkColor& c, float32 round = 0.f, float32 th = 1.f) const { (void)round; dl->AddRect(r, c, th); }
        // Panneau a BORD ARRONDI : fond borde (le contour suit le rayon, contrairement
        // a AddRect qui est carre). border colore -> fond inset par 1px.
        void Panel(const NkRect& r, const NkColor& fill, const NkColor& border, float32 round, float32 bw = 1.f) const {
            const float32 b = bw * (S > 1.f ? S : 1.f);
            dl->AddRectFilled(r, border, round);
            dl->AddRectFilled({ r.x + b, r.y + b, r.w - 2.f * b, r.h - 2.f * b }, fill, round - b > 0.f ? round - b : 0.f);
        }
        void Text(float32 x, float32 y, const char* t, const NkColor& c) const {
            if (f && t) dl->AddText(f->Face(), f->TexId(), { x, y + Asc() }, t, c);
        }
        // Texte centre verticalement dans une hauteur h a partir de y.
        void TextV(float32 x, float32 y, float32 h, const char* t, const NkColor& c) const {
            Text(x, y + (h - Lh()) * 0.5f, t, c);
        }
        // Bouton plein (renvoie true au clic). bg/hover/texte personnalisables.
        bool Button(const NkRect& r, const char* label, const NkColor& bg, const NkColor& bgH, const NkColor& fg, float32 round) const {
            const bool h = Hit(r);
            Rect(r, h ? bgH : bg, round);
            const float32 tw = TextW(label);
            TextV(r.x + (r.w - tw) * 0.5f, r.y, r.h, label, fg);
            return h && click;
        }

        // ── Icones (style Lucide, dessinees au trait) dans un carre `r` ────────
        void Icon(const char* name, const NkRect& box, const NkColor& c) const;
    };

    // Segment au trait (le draw-list NKGui gere l'epaisseur + l'AA).
    inline void NkLine(const NkUi& u, NkVec2 a, NkVec2 b, const NkColor& c, float32 th) {
        u.dl->AddLine(a, b, c, th);
    }

    // Jeu d'icones SVG (data/textures/icon) rasterisees en textures au demarrage.
    // 0 = non chargee (NkDrawIcon ne dessine rien). Teintees au rendu.
    struct NkIcons {
        uint32 accueil = 0, ouvrir = 0, ouvrirDossier = 0, nouveau = 0, cloner = 0,
               toolchains = 0, platforms = 0, gear = 0, exemple = 0, star = 0, shape = 0,
               search = 0, workspace = 0;
    };
    inline void NkDrawIcon(const NkUi& u, uint32 tex, const NkRect& r, const NkColor& tint) {
        if (tex) u.dl->AddImage(tex, r, { 0,0 }, { 1,1 }, tint);
    }

    inline void NkUi::Icon(const char* name, const NkRect& r, const NkColor& c) const {
        const float32 x = r.x, y = r.y, w = r.w, h = r.h;
        const float32 th = w * 0.10f < 1.4f ? 1.4f : w * 0.10f;
        auto rectS = [&](float32 rx, float32 ry, float32 rw, float32 rh) { dl->AddRectFilled({ rx, ry, rw, rh }, c, 1.f); };
        auto strokeBox = [&](float32 rx, float32 ry, float32 rw, float32 rh, float32 rad) { (void)rad; dl->AddRect({ rx, ry, rw, rh }, c, th); };
        auto cmp = [&](const char* a, const char* b) { while (*a && *b) { if (*a != *b) return false; ++a; ++b; } return *a == *b; };

        if (cmp(name, "house")) {
            NkLine(*this, { x + w*0.5f, y + h*0.12f }, { x + w*0.08f, y + h*0.5f }, c, th);
            NkLine(*this, { x + w*0.5f, y + h*0.12f }, { x + w*0.92f, y + h*0.5f }, c, th);
            strokeBox(x + w*0.18f, y + h*0.5f, w*0.64f, h*0.38f, 1.f);
        } else if (cmp(name, "folder-open") || cmp(name, "folder")) {
            strokeBox(x + w*0.08f, y + h*0.3f, w*0.84f, h*0.46f, 2.f);
            rectS(x + w*0.08f, y + h*0.22f, w*0.4f, h*0.12f);
        } else if (cmp(name, "plus-circle") || cmp(name, "plus")) {
            strokeBox(x + w*0.12f, y + h*0.12f, w*0.76f, h*0.76f, w*0.38f);
            rectS(x + w*0.46f, y + h*0.28f, th, h*0.44f);
            rectS(x + w*0.28f, y + h*0.46f, w*0.44f, th);
        } else if (cmp(name, "git-branch")) {
            rectS(x + w*0.26f, y + h*0.15f, th, h*0.7f);                 // branche gauche
            strokeBox(x + w*0.18f, y + h*0.06f, w*0.16f, w*0.16f, w*0.08f);
            strokeBox(x + w*0.18f, y + h*0.78f, w*0.16f, w*0.16f, w*0.08f);
            strokeBox(x + w*0.62f, y + h*0.30f, w*0.16f, w*0.16f, w*0.08f);
            NkLine(*this, { x + w*0.30f, y + h*0.4f }, { x + w*0.66f, y + h*0.4f }, c, th);
        } else if (cmp(name, "wrench")) {
            NkLine(*this, { x + w*0.25f, y + h*0.75f }, { x + w*0.7f, y + h*0.3f }, c, th*1.4f);
            strokeBox(x + w*0.55f, y + h*0.1f, w*0.32f, w*0.32f, w*0.16f);
        } else if (cmp(name, "cpu")) {
            strokeBox(x + w*0.22f, y + h*0.22f, w*0.56f, h*0.56f, 2.f);
            rectS(x + w*0.36f, y + h*0.36f, w*0.28f, h*0.28f);
            for (int i = 0; i < 3; ++i) { const float32 o = 0.3f + i*0.2f;
                rectS(x + w*o, y + h*0.1f, th, h*0.12f); rectS(x + w*o, y + h*0.78f, th, h*0.12f);
                rectS(x + w*0.1f, y + h*o, w*0.12f, th); rectS(x + w*0.78f, y + h*o, w*0.12f, th); }
        } else if (cmp(name, "settings")) {
            strokeBox(x + w*0.3f, y + h*0.3f, w*0.4f, h*0.4f, w*0.2f);   // moyeu
            // 8 dents (offsets precalcules, sans trigo)
            static const float32 ox[8] = { 0.f, 0.30f, 0.42f, 0.30f, 0.f, -0.30f, -0.42f, -0.30f };
            static const float32 oy[8] = { -0.42f, -0.30f, 0.f, 0.30f, 0.42f, 0.30f, 0.f, -0.30f };
            for (int i = 0; i < 8; ++i) {
                const float32 cxp = x + w*0.5f + ox[i]*w, cyp = y + h*0.5f + oy[i]*h;
                rectS(cxp - th*0.8f, cyp - th*0.8f, th*1.6f, th*1.6f);
            }
        } else if (cmp(name, "search")) {
            strokeBox(x + w*0.15f, y + h*0.15f, w*0.5f, h*0.5f, w*0.25f);
            NkLine(*this, { x + w*0.58f, y + h*0.58f }, { x + w*0.85f, y + h*0.85f }, c, th);
        } else if (cmp(name, "chevron-down")) {
            NkLine(*this, { x + w*0.25f, y + h*0.4f }, { x + w*0.5f, y + h*0.62f }, c, th);
            NkLine(*this, { x + w*0.75f, y + h*0.4f }, { x + w*0.5f, y + h*0.62f }, c, th);
        } else if (cmp(name, "chevron-right")) {
            NkLine(*this, { x + w*0.4f, y + h*0.25f }, { x + w*0.62f, y + h*0.5f }, c, th);
            NkLine(*this, { x + w*0.4f, y + h*0.75f }, { x + w*0.62f, y + h*0.5f }, c, th);
        } else if (cmp(name, "book-open")) {
            strokeBox(x + w*0.1f, y + h*0.2f, w*0.8f, h*0.6f, 1.f);
            rectS(x + w*0.5f - th*0.5f, y + h*0.2f, th, h*0.6f);
        } else if (cmp(name, "minus")) {
            rectS(x + w*0.2f, y + h*0.5f - th*0.5f, w*0.6f, th);
        } else if (cmp(name, "square")) {
            strokeBox(x + w*0.22f, y + h*0.22f, w*0.56f, h*0.56f, 2.f);
        } else if (cmp(name, "x")) {
            NkLine(*this, { x + w*0.25f, y + h*0.25f }, { x + w*0.75f, y + h*0.75f }, c, th);
            NkLine(*this, { x + w*0.75f, y + h*0.25f }, { x + w*0.25f, y + h*0.75f }, c, th);
        } else {
            strokeBox(x + w*0.2f, y + h*0.2f, w*0.6f, h*0.6f, 2.f);   // fallback
        }
    }

    // ── Icone de marque NKCode : accolades { } encadrant un arbre/reseau ───────
    inline void NkBrandMark(const NkUi& u, const NkRect& r, const NkColor& c) {
        const float32 x = r.x, y = r.y, w = r.w, h = r.h;
        const float32 th = u.s(1.6f);
        // accolade gauche
        NkLine(u, { x + w*0.22f, y + h*0.1f }, { x + w*0.12f, y + h*0.2f }, c, th);
        NkLine(u, { x + w*0.12f, y + h*0.2f }, { x + w*0.12f, y + h*0.42f }, c, th);
        NkLine(u, { x + w*0.12f, y + h*0.42f }, { x + w*0.04f, y + h*0.5f }, c, th);
        NkLine(u, { x + w*0.04f, y + h*0.5f }, { x + w*0.12f, y + h*0.58f }, c, th);
        NkLine(u, { x + w*0.12f, y + h*0.58f }, { x + w*0.12f, y + h*0.8f }, c, th);
        NkLine(u, { x + w*0.12f, y + h*0.8f }, { x + w*0.22f, y + h*0.9f }, c, th);
        // accolade droite
        NkLine(u, { x + w*0.78f, y + h*0.1f }, { x + w*0.88f, y + h*0.2f }, c, th);
        NkLine(u, { x + w*0.88f, y + h*0.2f }, { x + w*0.88f, y + h*0.42f }, c, th);
        NkLine(u, { x + w*0.88f, y + h*0.42f }, { x + w*0.96f, y + h*0.5f }, c, th);
        NkLine(u, { x + w*0.96f, y + h*0.5f }, { x + w*0.88f, y + h*0.58f }, c, th);
        NkLine(u, { x + w*0.88f, y + h*0.58f }, { x + w*0.88f, y + h*0.8f }, c, th);
        NkLine(u, { x + w*0.88f, y + h*0.8f }, { x + w*0.78f, y + h*0.9f }, c, th);
        // arbre central : tronc + branches + noeuds
        NkLine(u, { x + w*0.5f, y + h*0.2f }, { x + w*0.5f, y + h*0.82f }, c, th);
        NkLine(u, { x + w*0.5f, y + h*0.32f }, { x + w*0.32f, y + h*0.22f }, c, th);
        NkLine(u, { x + w*0.5f, y + h*0.32f }, { x + w*0.68f, y + h*0.22f }, c, th);
        NkLine(u, { x + w*0.5f, y + h*0.5f }, { x + w*0.34f, y + h*0.42f }, c, th);
        NkLine(u, { x + w*0.5f, y + h*0.5f }, { x + w*0.66f, y + h*0.42f }, c, th);
        NkLine(u, { x + w*0.5f, y + h*0.82f }, { x + w*0.38f, y + h*0.9f }, c, th);
        NkLine(u, { x + w*0.5f, y + h*0.82f }, { x + w*0.62f, y + h*0.9f }, c, th);
        auto node = [&](float32 nx, float32 ny) { u.dl->AddRectFilled({ x + w*nx - u.s(1.8f), y + h*ny - u.s(1.8f), u.s(3.6f), u.s(3.6f) }, c, u.s(1.f)); };
        node(0.5f, 0.2f); node(0.32f, 0.22f); node(0.68f, 0.22f); node(0.34f, 0.42f); node(0.66f, 0.42f);
    }

} // namespace nkcode
} // namespace nkentseu
