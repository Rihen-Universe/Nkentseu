#pragma once
// =============================================================================
// Toolbar.h — Barre d'outils horizontale facon Visual Studio Community.
//   [Construire] [Demarrer(Run)] | <projet cible> <config> <plateforme> [appareil]
//   Un .jenga contient PLUSIEURS projets -> selecteur de projet (liste via
//   `jenga info`). Build OU Run du projet choisi, en Debug/Release, plateforme X.
// =============================================================================
#include "NKEditorKit/NkEditorKit.h"
#include "NKCode/Project/NkCodeState.h"

namespace nkcode {

    using namespace nkentseu;
    using namespace nkentseu::editorkit;
    using namespace nkentseu::nkgui;

    inline void DrawCodeToolbar(NkEditorFrameContext& ec, NkCodeState* s) {
        if (!s) return;
        auto& ctx = ec.Ui();
        s->LoadProjects();    // lazy : lance `jenga info` une seule fois
        s->PollProjects();    // draine + parse la liste des projets

        static const char* kCfg[]  = { "Debug", "Release" };
        static const char* kPlat[] = { "x64 (Windows)", "Linux", "Android", "Web" };
        static const char* kPArg[] = { "", "Linux", "Android", "Web" };
        static const char* kDev[]  = { "Emulateur Android", "Appareil USB" };

        // BeginCombo remplit la largeur restante et affiche son label -> on borne la
        // region (largeur fixe) et on passe un label vide isole par PushId/PopId.
        auto setW = [&](float32 w) { ctx.layout.region.w = (ctx.layout.cursor.x - ctx.layout.region.x) + w; };

        // Largeurs fixes des combos.
        const float32 wProj = ctx.S(190.f), wCfg = ctx.S(120.f), wPlat = ctx.S(150.f), wDev = ctx.S(170.f);
        // CENTRAGE : calcule la largeur totale puis place le curseur de depart au milieu.
        auto btnW = [&](const char* l) {
            return (ctx.font && ctx.font->Valid() ? ctx.font->MeasureWidth(l) : 40.f) + ctx.theme.framePadX * 2.f + 6.f;
        };
        const float32 sp = ctx.layout.itemSpacingX;
        int32   nItems = 5;
        float32 total  = btnW(" Construire ") + btnW("  Demarrer  ") + wProj + wCfg + wPlat;
        if (s->platIdx == 2) { total += wDev; nItems = 6; }
        total += sp * (nItems - 1);
        const float32 left = ctx.layout.region.x + ctx.S(8.f);
        const float32 mid  = ctx.layout.region.x + (ctx.layout.region.w - total) * 0.5f;
        ctx.layout.cursor.x   = mid > left ? mid : left;
        ctx.layout.lineStartX = ctx.layout.cursor.x;

        // Construire (build du projet selectionne).
        if (Button(ctx, " Construire ")) s->BuildSelected(kPArg[s->platIdx]);
        ctx.SameLine();
        // Demarrer (run : build force puis execution).
        if (Button(ctx, "  Demarrer  ")) s->RunSelected(kPArg[s->platIdx], "");
        ctx.SameLine();

        // Projet cible (le .jenga en contient plusieurs).
        setW(ctx.S(190.f));
        const char* projPrev = s->projects.Empty() ? "(chargement...)" : s->SelectedProject();
        ctx.PushId("proj");
        if (BeginCombo(ctx, "", projPrev, static_cast<int32>(s->projects.Size()))) {
            for (usize i = 0; i < s->projects.Size(); ++i)
                if (Selectable(ctx, s->projects[i].CStr(), static_cast<int32>(i) == s->projIdx)) {
                    s->projIdx = static_cast<int32>(i); ctx.ClosePopup();
                }
            EndCombo(ctx);
        }
        ctx.PopId();
        ctx.SameLine();

        // Configuration (Debug / Release).
        setW(ctx.S(120.f));
        ctx.PushId("cfg");
        if (BeginCombo(ctx, "", kCfg[s->cfgIdx], 2)) {
            for (int32 i = 0; i < 2; ++i)
                if (Selectable(ctx, kCfg[i], i == s->cfgIdx)) { s->cfgIdx = i; ctx.ClosePopup(); }
            EndCombo(ctx);
        }
        ctx.PopId();
        ctx.SameLine();

        // Plateforme cible.
        setW(ctx.S(150.f));
        ctx.PushId("plat");
        if (BeginCombo(ctx, "", kPlat[s->platIdx], 4)) {
            for (int32 i = 0; i < 4; ++i)
                if (Selectable(ctx, kPlat[i], i == s->platIdx)) { s->platIdx = i; ctx.ClosePopup(); }
            EndCombo(ctx);
        }
        ctx.PopId();

        // Appareil / emulateur : seulement pour une cible mobile (Android).
        if (s->platIdx == 2) {
            ctx.SameLine();
            setW(ctx.S(170.f));
            ctx.PushId("dev");
            if (BeginCombo(ctx, "", kDev[s->devIdx], 2)) {
                for (int32 i = 0; i < 2; ++i)
                    if (Selectable(ctx, kDev[i], i == s->devIdx)) { s->devIdx = i; ctx.ClosePopup(); }
                EndCombo(ctx);
            }
            ctx.PopId();
        }
    }

} // namespace nkcode
