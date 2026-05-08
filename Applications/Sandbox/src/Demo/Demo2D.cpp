// =============================================================================
// Demo2D.cpp  — Demo 1
//
// Demonstration minimaliste d'une frame 2D :
//   - Config For2D : RENDER2D + TEXT + UI + OVERLAY uniquement
//   - Pas de RENDER3D, pas de SHADOW, pas de POST_PROCESS — empreinte minimale
//   - Sprites animees, formes geometriques (cercles, lignes, gradient), texte
//
// Utile comme test smoke pour Render2D apres modifications de Core.
// =============================================================================
#include "DemoCommon.h"

namespace nkentseu { namespace demo {

    struct Demo2DState {
        NkVec2f spritePos     = {0, 0};
        NkVec2f spriteVel     = {180.f, 140.f};
        float32 ringRotation  = 0.f;
    };

    bool Demo2D_Init(DemoCtx& ctx) {
        auto* st = new Demo2DState();
        ctx.userData = st;
        st->spritePos = {(float32)ctx.width * 0.5f, (float32)ctx.height * 0.5f};
        logger.Info("[Demo2D] Init OK\n");
        return true;
    }

    void Demo2D_Frame(DemoCtx& ctx, float32 dt) {
        auto* st = (Demo2DState*)ctx.userData;

        // Update sprite position (rebondit sur les bords).
        // Apres resize, ctx.width/height changent : on clamp la position dans la
        // nouvelle zone pour eviter que la balle disparaisse hors-ecran.
        const float32 minX = 32.f, minY = 32.f;
        const float32 maxX = (float32)ctx.width  - 32.f;
        const float32 maxY = (float32)ctx.height - 32.f;
        st->spritePos.x += st->spriteVel.x * dt;
        st->spritePos.y += st->spriteVel.y * dt;
        if (st->spritePos.x < minX) { st->spritePos.x = minX; st->spriteVel.x = -st->spriteVel.x; }
        if (st->spritePos.x > maxX) { st->spritePos.x = maxX; st->spriteVel.x = -st->spriteVel.x; }
        if (st->spritePos.y < minY) { st->spritePos.y = minY; st->spriteVel.y = -st->spriteVel.y; }
        if (st->spritePos.y > maxY) { st->spritePos.y = maxY; st->spriteVel.y = -st->spriteVel.y; }
        st->ringRotation += dt * 1.5f;

        if (!ctx.renderer->BeginFrame()) return;

        auto* r2d = ctx.renderer->GetRender2D();
        if (!r2d) {
            ctx.renderer->EndFrame();
            ctx.renderer->Present();
            return;
        }

        r2d->Begin(ctx.renderer->GetCmd(), ctx.width, ctx.height);

        // Fond gradient vertical
        r2d->FillRectGradV({0, 0, (float32)ctx.width, (float32)ctx.height},
                            {0.08f, 0.10f, 0.18f, 1.f},
                            {0.02f, 0.04f, 0.08f, 1.f});

        // Grille de fond (lignes diagonales)
        const float32 step = 64.f;
        const NkVec4f gridCol = {1.f, 1.f, 1.f, 0.05f};
        for (float32 x = 0; x < (float32)ctx.width + ctx.height; x += step) {
            r2d->DrawLine({x, 0}, {x - (float32)ctx.height, (float32)ctx.height}, gridCol, 1.f);
        }

        // Cercles concentriques anime au centre
        NkVec2f c = {(float32)ctx.width * 0.5f, (float32)ctx.height * 0.5f};
        for (int i = 0; i < 6; i++) {
            float32 r  = 40.f + (float32)i * 25.f;
            float32 a  = 0.7f - (float32)i * 0.10f;
            float32 hue= sinf(ctx.totalTime * 0.5f + i * 0.5f) * 0.5f + 0.5f;
            r2d->DrawCircle(c, r, {hue, 0.4f, 1.f - hue, a}, 2.f, 64);
        }

        // Sprite anime (cercle plein remplace texture)
        r2d->FillCircle(st->spritePos, 24.f, {1.f, 0.85f, 0.2f, 1.f}, 32);
        r2d->DrawCircle(st->spritePos, 26.f, {1.f, 1.f, 1.f, 1.f}, 2.f, 32);

        // Coins : panneaux semi-transparents
        const float32 panelW = 220.f, panelH = 80.f;
        r2d->FillRoundRect({10, 10, panelW, panelH}, {0, 0, 0, 0.5f}, 8.f);
        r2d->FillRoundRect({(float32)ctx.width - panelW - 10, 10, panelW, panelH},
                            {0, 0, 0, 0.5f}, 8.f);

        r2d->End();

        // Overlay (stats + texte)
        if (auto* overlay = ctx.renderer->GetOverlay()) {
            overlay->BeginOverlay(ctx.renderer->GetCmd(), ctx.width, ctx.height);
            overlay->DrawStats(ctx.renderer->GetStats());
            overlay->DrawText({20.f, 35.f}, "Demo 2D — sprite + shapes");
            overlay->DrawText({20.f, 55.f}, "FPS approx: %.1f", dt > 1e-4f ? 1.f / dt : 0.f);
            overlay->EndOverlay();
        }

        ctx.renderer->EndFrame();
        ctx.renderer->Present();
    }

    void Demo2D_Shutdown(DemoCtx& ctx) {
        delete (Demo2DState*)ctx.userData;
        ctx.userData = nullptr;
        logger.Info("[Demo2D] Shutdown\n");
    }

}} // namespace nkentseu::demo
