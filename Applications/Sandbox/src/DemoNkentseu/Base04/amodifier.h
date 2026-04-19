/**
 * PATCH : Intégration NkUIMultiViewportManager dans nkmain.cpp
 *
 * Ce fichier montre UNIQUEMENT les changements à apporter à nkmain.cpp.
 * Cherchez les marqueurs "// ← AJOUT" et "// ← MODIF" pour localiser chaque changement.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * CHANGEMENTS RÉSUMÉS :
 *
 *  1. Include + déclaration de NkUIMultiViewportManager
 *  2. Init du manager après NkUINKRHIBackend::Init
 *  3. Activation optionnelle (enableMultiViewport par défaut = false)
 *  4. BeginFrame du manager avant ctx.BeginFrame
 *  5. Submit du manager après uiBackend.Submit (rendu dans les fenêtres OS)
 *  6. Destroy du manager avant ctx.Destroy
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

// ═══════════════════════════════════════════════════════════════════════════
// SECTION 1 — Includes à ajouter en haut de nkmain.cpp
// ═══════════════════════════════════════════════════════════════════════════

#include "NkUIMultiViewport.h"                    // ← AJOUT

// ═══════════════════════════════════════════════════════════════════════════
// SECTION 2 — Variables à déclarer dans nkmain()
// (après la déclaration de uiBackend)
// ═══════════════════════════════════════════════════════════════════════════

// Dans nkmain(), après la ligne :
//   NkUINKRHIBackend uiBackend;
//   if (!uiBackend.Init(device, device->GetSwapchainRenderPass(), api)) { ... }

/*
    NkUIMultiViewportManager multiViewport;                     // ← AJOUT
    if (!multiViewport.Init(device,
                            device->GetSwapchainRenderPass(),
                            api,
                            &window,
                            static_cast<int32>(W),
                            static_cast<int32>(H)))
    {
        logger.Warn("[nkmain] MultiViewport init failed (non fatal)\n");
    }
    // Activer ou désactiver le comportement multi-fenêtre OS :
    // true  = fenêtres qui sortent du viewport → nouvelle fenêtre OS
    // false = fenêtres toujours dans le viewport principal (overlay classique)
    multiViewport.SetEnabled(true);                             // ← AJOUT (optionnel)
*/

// ═══════════════════════════════════════════════════════════════════════════
// SECTION 3 — Callback resize à modifier
// ═══════════════════════════════════════════════════════════════════════════

// Remplacez le callback NkWindowResizeEvent par :
/*
    events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e) {
        W = static_cast<uint32>(e->GetWidth());
        H = static_cast<uint32>(e->GetHeight());
        ctx.viewW = static_cast<int32>(W);
        ctx.viewH = static_cast<int32>(H);
        // Le MultiViewport sera notifié via BeginFrame()
    });
*/
// → Pas de changement ici, le multiViewport lit W/H dans BeginFrame.

// ═══════════════════════════════════════════════════════════════════════════
// SECTION 4 — Boucle principale : appels à ajouter
// ═══════════════════════════════════════════════════════════════════════════

// Dans la boucle while(running), REMPLACEZ le bloc UI par :
/*

    // ── Frame logique UI ──────────────────────────────────────────────────
    ctx.BeginFrame(input, dt);
    wm.BeginFrame(ctx);
    ls.depth = 0;

    // ← AJOUT : BeginFrame multi-viewport (AVANT demo->Render)
    // Gère la création/destruction des fenêtres OS secondaires
    // et pump leurs événements
    multiViewport.BeginFrame(ctx, wm, dt,
                             static_cast<int32>(W),
                             static_cast<int32>(H));

    demo->Render(ctx, wm, dock, ls, dt);

    ctx.EndFrame();
    wm.EndFrame(ctx);

*/

// ═══════════════════════════════════════════════════════════════════════════
// SECTION 5 — Rendu GPU : Submit multi-viewport
// ═══════════════════════════════════════════════════════════════════════════

// Dans le bloc de rendu GPU, APRÈS cmd->EndRenderPass() :
/*
        if (cmd->BeginRenderPass(rp, fbo, area)) {
            NkViewport vp{0.f, 0.f, (float32)W, (float32)H, 0.f, 1.f};
            cmd->SetViewport(vp);
            cmd->SetScissor(area);
            uiBackend.Submit(cmd, ctx, W, H);
            cmd->EndRenderPass();
        }
        cmd->End();
        device->SubmitAndPresent(cmd);
        device->EndFrame(frame);

        // ← AJOUT : rend dans chaque fenêtre OS secondaire
        // (chaque viewport a son propre BeginFrame GPU / Present)
        multiViewport.Submit(ctx, wm);
*/

// ═══════════════════════════════════════════════════════════════════════════
// SECTION 6 — Nettoyage
// ═══════════════════════════════════════════════════════════════════════════

// Dans le bloc de nettoyage, AVANT ctx.Destroy() :
/*
    device->WaitIdle();
    demo->Destroy();
    multiViewport.Destroy();                     // ← AJOUT (avant uiBackend)
    sBackendForUpload = nullptr;
    uiBackend.Destroy();
    ctx.Destroy();
    ...
*/

// ═══════════════════════════════════════════════════════════════════════════
// SECTION 7 — Utilisation côté DEMO (dans NkUIDemo::Render)
// ═══════════════════════════════════════════════════════════════════════════
//
// Pour créer une fenêtre "sans titre" (full-area panel) :
/*
    NkUIWindow::Begin(ctx, wm, dl, font, ls, "##mypanel", nullptr,
        NkUIWindowFlags::NK_NO_TITLE_BAR |
        NkUIWindowFlags::NK_NO_COLLAPSE  |
        NkUIWindowFlags::NK_NO_CLOSE     |
        NkUIWindowFlags::NK_NO_RESIZE);
    // ... widgets ...
    NkUIWindow::End(ctx, wm, dl, ls);
*/
//
// Pour une fenêtre qui peut sortir du viewport et devenir une fenêtre OS :
// → Aucun changement côté utilisateur ! Si multiViewport.SetEnabled(true),
//   la fenêtre devient automatiquement une fenêtre OS quand elle sort.
//
// Pour désactiver ce comportement sur une fenêtre spécifique :
//   ws->flags |= NkUIWindowFlags::NK_NO_DOCK;  (empêche la "capture" multi-viewport)
//   (vous pouvez réutiliser NK_NO_DOCK ou ajouter un flag NK_NO_OS_WINDOW)

// ═══════════════════════════════════════════════════════════════════════════
// SECTION 8 — nkmain.cpp COMPLET avec les modifications intégrées
// ═══════════════════════════════════════════════════════════════════════════

// Voici le nkmain.cpp complet (copiez-collez en remplacement de votre fichier) :

/*

#include "NkUIMultiViewport.h"   // ← ajouté
// ... (tous vos includes existants) ...

int nkmain(const NkEntryState& state) {
    // ... (tout le code d'init existant) ...

    NkUINKRHIBackend uiBackend;
    if (!uiBackend.Init(device, device->GetSwapchainRenderPass(), api)) { ... }
    sBackendForUpload = &uiBackend;

    // ← NOUVEAU
    NkUIMultiViewportManager multiViewport;
    multiViewport.Init(device, device->GetSwapchainRenderPass(), api,
                       &window, (int32)W, (int32)H);
    multiViewport.SetEnabled(true); // false = comportement standard

    // ... (chargement polices, démo, cmd buffer) ...

    while (running) {
        input.BeginFrame();
        events.PollEvents();
        if (!running) break;
        if (W == 0 || H == 0) continue;

        if (W != device->GetSwapchainWidth() || H != device->GetSwapchainHeight())
            device->OnResize(W, H);

        float32 dt = clock.Tick().delta;
        if (dt <= 0.f || dt > 0.25f) dt = 1.f / 60.f;
        input.dt = dt;

        // ── Frame logique UI
        ctx.BeginFrame(input, dt);
        wm.BeginFrame(ctx);
        ls.depth = 0;

        multiViewport.BeginFrame(ctx, wm, dt, (int32)W, (int32)H); // ← NOUVEAU

        demo->Render(ctx, wm, dock, ls, dt);

        ctx.EndFrame();
        wm.EndFrame(ctx);

        // ... (upload atlas sales, curseur souris) ...

        // ── Rendu GPU principal
        NkFrameContext frame;
        if (!device->BeginFrame(frame)) continue;
        W = device->GetSwapchainWidth();
        H = device->GetSwapchainHeight();
        if (W == 0 || H == 0) { device->EndFrame(frame); continue; }

        const NkRenderPassHandle  rp  = device->GetSwapchainRenderPass();
        const NkFramebufferHandle fbo = device->GetSwapchainFramebuffer();
        const NkRect2D            area{0,0,(int32)W,(int32)H};

        cmd->Reset(); cmd->Begin();
        if (cmd->BeginRenderPass(rp, fbo, area)) {
            NkViewport vp{0.f,0.f,(float32)W,(float32)H,0.f,1.f};
            cmd->SetViewport(vp); cmd->SetScissor(area);
            uiBackend.Submit(cmd, ctx, W, H);
            cmd->EndRenderPass();
        }
        cmd->End();
        device->SubmitAndPresent(cmd);
        device->EndFrame(frame);

        multiViewport.Submit(ctx, wm); // ← NOUVEAU : rendu fenêtres OS secondaires
    }

    device->WaitIdle();
    demo->Destroy();
    multiViewport.Destroy();   // ← NOUVEAU
    sBackendForUpload = nullptr;
    uiBackend.Destroy();
    ctx.Destroy(); wm.Destroy(); dock.Destroy();
    device->DestroyCommandBuffer(cmd);
    NkDeviceFactory::Destroy(device);
    window.Close();
    return 0;
}

*/