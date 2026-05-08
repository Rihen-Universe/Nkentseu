// =============================================================================
// DemoSubsystems.cpp  — Demo 0
//
// Demontre l'API runtime EnableSubsystem / DisableSubsystem / IsSubsystemActive.
//
// Scenario :
//   - Demarre avec NK_SS_RENDER2D + NK_SS_TEXT + NK_SS_OVERLAY (config minimaliste)
//   - Toutes les 2 secondes, active ou desactive un sous-systeme et logue l'etat
//   - Le rendu 2D affiche en continu un fond colore + texte avec l'etat courant
//
// Ce que le test prouve :
//   - On peut ne PAS allouer Render3D/Shadow/PostProcess/VFX si on en a pas besoin
//     -> empreinte memoire et init time reduits
//   - On peut activer un sous-systeme a chaud sans recreer le renderer
//     -> le RenderGraph est reconstruit automatiquement
// =============================================================================
#include "DemoCommon.h"

namespace nkentseu { namespace demo {

    struct SubsystemsState {
        // Ressources 2D (pour donner un retour visuel)
        NkTexHandle font;             // pas necessairement utilise si TEXT off
        // Etat de la rotation des sous-systemes
        float32     toggleTimer = 0.f;
        uint32      toggleStep  = 0;
    };

    bool DemoSubsystems_Init(DemoCtx& ctx) {
        auto* st = new SubsystemsState();
        ctx.userData = st;

        char buf[256];
        SubsystemFlagsToString(ctx.renderer->GetActiveSubsystems(), buf, sizeof(buf));
        logger.Info("[DemoSubsystems] Init OK — sous-systemes actifs : {0}\n", buf);
        logger.Info("[DemoSubsystems] Toutes les 2s, on toggle un sous-systeme.\n");
        return true;
    }

    void DemoSubsystems_Frame(DemoCtx& ctx, float32 dt) {
        auto* st = (SubsystemsState*)ctx.userData;
        st->toggleTimer += dt;

        // Toggle un sous-systeme toutes les 2 secondes
        if (st->toggleTimer >= 2.0f) {
            st->toggleTimer = 0.f;
            const NkSubsystemFlags rotate[] = {
                NK_SS_RENDER3D,    // active 3D
                NK_SS_SHADOW,      // ajoute shadows
                NK_SS_POST_PROCESS,// ajoute post-process
                NK_SS_VFX,         // ajoute VFX
                NK_SS_ANIMATION,   // ajoute animation
                NK_SS_RENDER3D,    // desactive 3D (cascade : ferme aussi shadow/anim)
            };
            const uint32 N = (uint32)(sizeof(rotate) / sizeof(rotate[0]));
            NkSubsystemFlags target = rotate[st->toggleStep % N];
            st->toggleStep++;

            const char* targetName =
                target == NK_SS_RENDER3D    ? "RENDER3D"     :
                target == NK_SS_SHADOW      ? "SHADOW"       :
                target == NK_SS_POST_PROCESS? "POST_PROCESS" :
                target == NK_SS_VFX         ? "VFX"          :
                target == NK_SS_ANIMATION   ? "ANIMATION"    : "?";

            if (ctx.renderer->IsSubsystemActive(target)) {
                ctx.renderer->DisableSubsystem(target);
                logger.Info("[DemoSubsystems] Disabled {0}\n", targetName);
            } else {
                bool ok = ctx.renderer->EnableSubsystem(target);
                logger.Info("[DemoSubsystems] {0} {1}\n",
                            ok ? "Enabled" : "Failed to enable", targetName);
            }
            char buf[256];
            SubsystemFlagsToString(ctx.renderer->GetActiveSubsystems(), buf, sizeof(buf));
            logger.Info("[DemoSubsystems]   -> sous-systemes actifs : {0}\n", buf);
        }

        // ─── Rendu : si Render2D actif, on dessine ──────────────────────────
        if (!ctx.renderer->BeginFrame()) return;

        if (auto* r2d = ctx.renderer->GetRender2D()) {
            r2d->Begin(ctx.renderer->GetCmd(), ctx.width, ctx.height);

            // Fond color-cycling
            float32 t = ctx.totalTime;
            NkVec4f bg = {
                0.10f + 0.10f * sinf(t * 0.7f),
                0.12f + 0.10f * sinf(t * 0.5f + 1.f),
                0.15f + 0.10f * sinf(t * 0.3f + 2.f),
                1.f
            };
            r2d->FillRect({0, 0, (float32)ctx.width, (float32)ctx.height}, bg);

            // Petite barre d'etat en bas
            r2d->FillRect({0, (float32)ctx.height - 24, (float32)ctx.width, 24},
                          {0, 0, 0, 0.6f});

            r2d->End();
        }

        // ─── Overlay (stats + texte) si Overlay actif ───────────────────────
        if (auto* overlay = ctx.renderer->GetOverlay()) {
            overlay->BeginOverlay(ctx.renderer->GetCmd(), ctx.width, ctx.height);
            overlay->DrawStats(ctx.renderer->GetStats());

            char info[256];
            char flagsBuf[256];
            SubsystemFlagsToString(ctx.renderer->GetActiveSubsystems(), flagsBuf, sizeof(flagsBuf));
            snprintf(info, sizeof(info), "Active: %s", flagsBuf);
            overlay->DrawText({10.f, (float32)ctx.height - 18.f}, "%s", info);

            overlay->EndOverlay();
        }

        ctx.renderer->EndFrame();
        ctx.renderer->Present();
    }

    void DemoSubsystems_Shutdown(DemoCtx& ctx) {
        delete (SubsystemsState*)ctx.userData;
        ctx.userData = nullptr;
        logger.Info("[DemoSubsystems] Shutdown\n");
    }

}} // namespace nkentseu::demo
