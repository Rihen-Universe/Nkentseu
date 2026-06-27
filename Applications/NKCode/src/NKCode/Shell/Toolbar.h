#pragma once
// =============================================================================
// Toolbar.h — Barre de build complete, pilotee par la structure Jenga reelle.
//   [Recharger] | Workspace | Projet | System | Config | Architecture |
//               | Construire | Recompiler | Nettoyer | Demarrer
//   - Workspace = fichier .jenga a la racine contenant `with workspace`.
//   - Projet : liste de `jenga info` du workspace (+ « Tous les projets »).
//   - System/Config/Architecture pilotent --platform <OS>-<arch> et --config.
//   - Config/Architecture acceptent « Toutes » -> compilation en rafale (file).
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

        s->ScanWorkspaces();   // detecte les workspaces (.jenga avec `with workspace`)
        s->TickWatch(ec.dt);   // auto-detection des modifs (.jenga racine)
        s->LoadProjects();     // `jenga info` du workspace courant (recharge au besoin)
        s->PollProjects();

        // Borne la region a une largeur fixe pour chaque combo + label isole.
        auto setW = [&](float32 w) { ctx.layout.region.w = (ctx.layout.cursor.x - ctx.layout.region.x) + w; };
        const float32 savedRegionW = ctx.layout.region.w;

        // CENTRAGE : largeur totale (bouton + 5 combos + 4 actions) -> depart au milieu.
        auto bW = [&](const char* l) { return (ctx.font && ctx.font->Valid() ? ctx.font->MeasureWidth(l) : 40.f) + ctx.theme.framePadX * 2.f + 6.f; };
        const float32 sp = ctx.layout.itemSpacingX;
        const float32 wWs = ctx.S(150.f), wProj = ctx.S(180.f), wSys = ctx.S(130.f), wCfg = ctx.S(110.f), wArch = ctx.S(130.f), wTest = ctx.S(150.f);
        // Tests VISIBLES = ceux du projet selectionne (ou tous si « Tous les projets »).
        int32 nTestVis = 0;
        for (int32 i = 0; i < (int32)s->tests.Size(); ++i) if (s->TestVisible(i)) ++nTestVis;
        const bool hasTests = !s->tests.Empty();   // le combo reste tant qu'il y a des tests
        if (s->testIdx >= 0 && !s->TestVisible(s->testIdx)) s->testIdx = -1;   // -> « Tous les tests »
        const float32 left = ctx.layout.region.x + ctx.S(8.f);
        float32 startX = left;
        if (s->HasWorkspace()) {
            float32 total = bW(" \xE2\x86\xBB ") + wWs + wProj + wSys + wCfg + wArch
                          + bW(" Construire ") + bW(" Recompiler ") + bW(" Nettoyer ") + bW(" Demarrer ")
                          + sp * 9.f;
            if (hasTests) total += wTest + bW(" Tester ") + sp * 2.f;
            const float32 mid = ctx.layout.region.x + (savedRegionW - total) * 0.5f;
            startX = mid > left ? mid : left;
        }
        ctx.layout.cursor.x   = startX;
        ctx.layout.lineStartX = startX;

        // ── Bouton Recharger (re-scan + jenga info) ──
        if (Button(ctx, " \xE2\x86\xBB ")) s->RequestReload();   // glyphe ↻
        ctx.SameLine();

        if (!s->HasWorkspace()) {
            ctx.SameLine();
            ctx.layout.region.w = savedRegionW;
            ec.Text("  Aucun workspace (.jenga avec 'with workspace') a la racine.");
            return;
        }

        // ── 1) Workspace ──
        setW(ctx.S(150.f));
        ctx.PushId("ws");
        const char* wsPrev = (s->wsIdx >= 0 && s->wsIdx < (int32)s->wsNames.Size()) ? s->wsNames[s->wsIdx].CStr() : "(workspace)";
        if (BeginCombo(ctx, "", wsPrev, (int32)s->wsNames.Size())) {
            for (usize i = 0; i < s->wsNames.Size(); ++i)
                if (Selectable(ctx, s->wsNames[i].CStr(), (int32)i == s->wsIdx)) { s->wsIdx = (int32)i; s->RequestReload(); ctx.ClosePopup(); }
            EndCombo(ctx);
        }
        ctx.PopId(); ctx.SameLine();

        // ── 2) Projet (+ « Tous les projets ») ──
        setW(ctx.S(180.f));
        ctx.PushId("proj");
        const int32 nProj = (int32)s->projects.Size();
        const char* projPrev = s->projects.Empty() ? "(chargement...)" : (s->AllProjects() ? "Tous les projets" : s->SelectedProject());
        if (BeginCombo(ctx, "", projPrev, nProj + 1)) {
            for (int32 i = 0; i < nProj; ++i)
                if (Selectable(ctx, s->projects[i].CStr(), i == s->projIdx)) { s->projIdx = i; ctx.ClosePopup(); }
            if (Selectable(ctx, "Tous les projets", s->AllProjects())) { s->projIdx = nProj; ctx.ClosePopup(); }
            EndCombo(ctx);
        }
        ctx.PopId(); ctx.SameLine();

        // ── 3) System (OS cible) ──
        int32 nSys = 0; const NkCodeState::SysDef* sys = NkCodeState::Systems(&nSys);
        if (s->sysIdx < 0 || s->sysIdx >= nSys) s->sysIdx = 0;
        setW(ctx.S(130.f));
        ctx.PushId("sys");
        if (BeginCombo(ctx, "", sys[s->sysIdx].name, nSys)) {
            for (int32 i = 0; i < nSys; ++i)
                if (Selectable(ctx, sys[i].name, i == s->sysIdx)) { s->sysIdx = i; s->archIdx = 0; ctx.ClosePopup(); }
            EndCombo(ctx);
        }
        ctx.PopId(); ctx.SameLine();

        // ── 4) Config (Debug / Release / Toutes) ──
        static const char* kCfg[] = { "Debug", "Release", "Toutes" };
        setW(ctx.S(110.f));
        ctx.PushId("cfg");
        if (BeginCombo(ctx, "", kCfg[s->cfgIdx >= 0 && s->cfgIdx < 3 ? s->cfgIdx : 0], 3)) {
            for (int32 i = 0; i < 3; ++i)
                if (Selectable(ctx, kCfg[i], i == s->cfgIdx)) { s->cfgIdx = i; ctx.ClosePopup(); }
            EndCombo(ctx);
        }
        ctx.PopId(); ctx.SameLine();

        // ── 5) Architecture (du system courant + « Toutes ») ──
        const NkCodeState::SysDef& S = sys[s->sysIdx];
        if (s->archIdx < 0 || s->archIdx > S.nArch) s->archIdx = 0;
        setW(ctx.S(130.f));
        ctx.PushId("arch");
        const char* archPrev = (s->archIdx >= S.nArch) ? "Toutes" : S.archs[s->archIdx];
        if (BeginCombo(ctx, "", archPrev, S.nArch + 1)) {
            for (int32 i = 0; i < S.nArch; ++i)
                if (Selectable(ctx, S.archs[i], i == s->archIdx)) { s->archIdx = i; ctx.ClosePopup(); }
            if (Selectable(ctx, "Toutes", s->archIdx >= S.nArch)) { s->archIdx = S.nArch; ctx.ClosePopup(); }
            EndCombo(ctx);
        }
        ctx.PopId(); ctx.SameLine();

        // ── 6) Tests (combo « Tous les tests » + chaque test visible) + bouton Tester ──
        if (hasTests) {
            setW(wTest);
            ctx.PushId("test");
            const char* testPrev = (s->testIdx >= 0 && s->TestVisible(s->testIdx)) ? s->tests[s->testIdx].CStr() : "Tous les tests";
            if (BeginCombo(ctx, "", testPrev, nTestVis + 1)) {
                if (Selectable(ctx, "Tous les tests", s->testIdx < 0)) { s->testIdx = -1; ctx.ClosePopup(); }
                for (int32 i = 0; i < (int32)s->tests.Size(); ++i)
                    if (s->TestVisible(i) && Selectable(ctx, s->tests[i].CStr(), i == s->testIdx)) { s->testIdx = i; ctx.ClosePopup(); }
                EndCombo(ctx);
            }
            ctx.PopId(); ctx.SameLine();
        }

        // ── 7) Actions ──
        ctx.layout.region.w = savedRegionW;
        if (Button(ctx, " Construire ")) s->DoBuildAction("build");
        ctx.SameLine();
        if (Button(ctx, " Recompiler ")) s->DoBuildAction("rebuild");
        ctx.SameLine();
        if (Button(ctx, " Nettoyer "))   s->DoClean();
        ctx.SameLine();
        if (Button(ctx, " Demarrer "))   s->DoRun();
        if (hasTests) { ctx.SameLine(); if (Button(ctx, " Tester ")) s->DoTest(); }
    }

} // namespace nkcode
