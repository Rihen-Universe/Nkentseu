// =============================================================================
// Demo2D.cpp  — Demo 1
//
// Demonstration de Render2D avec lighting 2D (Phase E) :
//   - Config For2D : RENDER2D + TEXT + UI + OVERLAY uniquement
//   - 3 point lights colorees qui orbitent autour du centre
//   - Sphere centrale + sprites peripheriques en mode LIT (recoivent le lighting)
//   - UI/text en mode UNLIT (ignore le lighting)
//   - Sprite anime + grille de fond + panels
// =============================================================================
#include "DemoCommon.h"
#include "NKWindow/Core/NkWESystem.h"   // NkEvents()
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkKeyboardEvent.h"

namespace nkentseu { namespace demo {

    struct Demo2DState {
        NkVec2f spritePos     = {0, 0};
        NkVec2f spriteVel     = {180.f, 140.f};
        float32 ringRotation  = 0.f;
        float32 lightTime     = 0.f;
        bool    lightOn[3]    = {true, true, true};   // toggle 1/2/3
        bool    shadowsOn     = true;                  // toggle 0 (zero)
    };

    bool Demo2D_Init(DemoCtx& ctx) {
        auto* st = new Demo2DState();
        ctx.userData = st;
        st->spritePos = {(float32)ctx.width * 0.5f, (float32)ctx.height * 0.5f};

        // Toggle des lumieres 1/2/3 + ombres globales (0)
        NkEvents().AddEventCallback<NkKeyPressEvent>([st](NkKeyPressEvent* e) {
            switch (e->GetKey()) {
                case NkKey::NK_NUM1: st->lightOn[0] = !st->lightOn[0]; break;
                case NkKey::NK_NUM2: st->lightOn[1] = !st->lightOn[1]; break;
                case NkKey::NK_NUM3: st->lightOn[2] = !st->lightOn[2]; break;
                case NkKey::NK_NUM0: st->shadowsOn  = !st->shadowsOn;  break;
                default: break;
            }
        });

        logger.Info("[Demo2D] Init OK — keys: 1/2/3 toggle lights, 0 toggle shadows\n");
        return true;
    }

    void Demo2D_Frame(DemoCtx& ctx, float32 dt) {
        auto* st = (Demo2DState*)ctx.userData;

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
        st->lightTime    += dt;

        if (!ctx.renderer->BeginFrame()) return;

        auto* r2d = ctx.renderer->GetRender2D();
        if (!r2d) {
            ctx.renderer->Present();
            ctx.renderer->EndFrame();
            return;
        }

        // ── Phase E : 3 point lights orbitant autour du centre ────────────────
        const float32 cx = (float32)ctx.width  * 0.5f;
        const float32 cy = (float32)ctx.height * 0.5f;
        const float32 orbitR = 250.f;
        NkLight2DDesc lights[3];
        lights[0].position  = {cx + cosf(st->lightTime * 0.7f) * orbitR,
                               cy + sinf(st->lightTime * 0.7f) * orbitR};
        lights[0].color     = {1.f, 0.3f, 0.2f};   // rouge
        lights[0].intensity = 1.5f;
        lights[0].radius    = 350.f;
        lights[0].enabled   = st->lightOn[0];
        lights[0].castShadow= st->shadowsOn;
        lights[1].position  = {cx + cosf(st->lightTime * 0.7f + 2.094f) * orbitR,
                               cy + sinf(st->lightTime * 0.7f + 2.094f) * orbitR};
        lights[1].color     = {0.2f, 1.f, 0.4f};   // vert
        lights[1].intensity = 1.5f;
        lights[1].radius    = 350.f;
        lights[1].enabled   = st->lightOn[1];
        lights[1].castShadow= st->shadowsOn;
        lights[2].position  = {cx + cosf(st->lightTime * 0.7f + 4.188f) * orbitR,
                               cy + sinf(st->lightTime * 0.7f + 4.188f) * orbitR};
        lights[2].color     = {0.3f, 0.5f, 1.f};   // bleu
        lights[2].intensity = 1.5f;
        lights[2].radius    = 350.f;
        lights[2].enabled   = st->lightOn[2];
        lights[2].castShadow= st->shadowsOn;
        r2d->SetLights2D(lights, 3, /*ambient=*/{0.06f, 0.06f, 0.10f});

        // ── Phase E.5 : Shadow casters (cercles qui bloquent la lumiere) ──────
        // 12 disques en ring + sphere centrale + sprite balle = 14 occluders.
        NkShadowCaster2D casters[16];
        uint32 ncasters = 0;
        for (int i = 0; i < 12; i++) {
            float32 a = (float32)i / 12.f * 6.2832f;
            float32 r = 200.f;
            casters[ncasters++] = { {cx + cosf(a) * r, cy + sinf(a) * r}, 30.f };
        }
        casters[ncasters++] = { {cx, cy}, 60.f };
        casters[ncasters++] = { st->spritePos,  28.f };
        r2d->SetShadowCasters2D(casters, ncasters);

        // E.7a : AABB casters (walls / plateformes typiques platformer).
        // 3 boxes : 2 piliers de chaque cote du mur central + 1 plateforme en bas.
        const float32 _wallW = 700.f, _wallH = 350.f;
        NkShadowCasterAABB2D aabbs[3];
        aabbs[0].min = {cx - _wallW * 0.5f - 80.f, cy - 30.f};
        aabbs[0].max = {cx - _wallW * 0.5f - 20.f, cy + 30.f};
        aabbs[1].min = {cx + _wallW * 0.5f + 20.f, cy - 30.f};
        aabbs[1].max = {cx + _wallW * 0.5f + 80.f, cy + 30.f};
        aabbs[2].min = {cx - 60.f, cy + _wallH * 0.5f + 20.f};
        aabbs[2].max = {cx + 60.f, cy + _wallH * 0.5f + 50.f};
        r2d->SetShadowCastersAABB2D(aabbs, 3);

        r2d->Begin(ctx.renderer->GetCmd(), ctx.width, ctx.height);

        // ── Background LIT : sol gris-medium qui revele les pools de lumiere ─
        // En mode LIT le shader applique uL2D sur chaque fragment, donc on voit
        // partout l'effet des 3 lights (pas seulement sous les "objets").
        r2d->SetLit(true);
        r2d->FillRect({0, 0, (float32)ctx.width, (float32)ctx.height},
                        {0.5f, 0.5f, 0.5f, 1.f});

        // Mur central blanc plus brillant (fond different de la grille)
        const float32 wallW = 700.f, wallH = 350.f;
        r2d->FillRect({cx - wallW * 0.5f, cy - wallH * 0.5f, wallW, wallH},
                        {1.f, 1.f, 1.f, 1.f});

        // Disques decoratifs blancs : montrent que le lighting suit la position
        for (int i = 0; i < 12; i++) {
            float32 a = (float32)i / 12.f * 6.2832f;
            float32 r = 200.f;
            NkVec2f p = {cx + cosf(a) * r, cy + sinf(a) * r};
            r2d->FillCircle(p, 30.f, {1.f, 1.f, 1.f, 1.f}, 24);
        }

        // Sphere centrale : recoit la convergence des 3 lights
        r2d->FillCircle({cx, cy}, 60.f, {1.f, 1.f, 1.f, 1.f}, 64);

        // Sprite anime LIT (montre que le lighting suit le sprite quand il bouge)
        r2d->FillCircle(st->spritePos, 28.f, {1.f, 1.f, 1.f, 1.f}, 32);

        // E.7a : Render des AABB casters (visualisation des murs/plateformes)
        for (int i = 0; i < 3; i++) {
            r2d->FillRect({aabbs[i].min.x, aabbs[i].min.y,
                            aabbs[i].max.x - aabbs[i].min.x,
                            aabbs[i].max.y - aabbs[i].min.y},
                           {1.f, 1.f, 1.f, 1.f});
        }

        // ── UNLIT : grille decorative + lights markers + UI panels ───────────
        r2d->SetLit(false);
        const float32 step = 64.f;
        const NkVec4f gridCol = {1.f, 1.f, 1.f, 0.06f};
        for (float32 x = 0; x < (float32)ctx.width + ctx.height; x += step) {
            r2d->DrawLine({x, 0}, {x - (float32)ctx.height, (float32)ctx.height}, gridCol, 1.f);
        }

        // Visualisation des lights (cercles colores). Light disabled = X gris.
        for (int i = 0; i < 3; i++) {
            NkVec4f col = st->lightOn[i]
                ? NkVec4f{lights[i].color.x, lights[i].color.y, lights[i].color.z, 1.f}
                : NkVec4f{0.4f, 0.4f, 0.4f, 1.f};
            r2d->DrawCircle(lights[i].position, 8.f, col, 2.f, 16);
            if (st->lightOn[i]) {
                r2d->FillCircle(lights[i].position, 4.f, col, 12);
            } else {
                // X marqueur pour light off
                r2d->DrawLine({lights[i].position.x - 5, lights[i].position.y - 5},
                               {lights[i].position.x + 5, lights[i].position.y + 5}, col, 2.f);
                r2d->DrawLine({lights[i].position.x - 5, lights[i].position.y + 5},
                               {lights[i].position.x + 5, lights[i].position.y - 5}, col, 2.f);
            }
        }

        // Panel d'info
        const float32 panelW = 240.f, panelH = 80.f;
        r2d->FillRoundRect({10, 10, panelW, panelH}, {0, 0, 0, 0.6f}, 8.f);

        r2d->End();

        // ── Overlay (stats + texte) ───────────────────────────────────────────
        if (auto* overlay = ctx.renderer->GetOverlay()) {
            overlay->BeginOverlay(ctx.renderer->GetCmd(), ctx.width, ctx.height);
            overlay->DrawStats(ctx.renderer->GetStats());
            overlay->DrawText({20.f, 35.f}, "Demo 2D - Phase E lighting + shadows");
            overlay->DrawText({20.f, 55.f}, "FPS approx: %.1f  |  dt: %.2f ms",
                              dt > 1e-4f ? 1.f / dt : 0.f, dt * 1000.f);
            overlay->DrawText({20.f, 75.f}, "1/2/3: toggle R/G/B  |  0: shadows %s",
                              st->shadowsOn ? "ON" : "OFF");
            overlay->DrawText({20.f, 95.f}, "lights: R=%s G=%s B=%s",
                              st->lightOn[0] ? "ON" : "off",
                              st->lightOn[1] ? "ON" : "off",
                              st->lightOn[2] ? "ON" : "off");
            overlay->EndOverlay();
        }

        ctx.renderer->Present();
        ctx.renderer->EndFrame();
    }

    void Demo2D_Shutdown(DemoCtx& ctx) {
        delete (Demo2DState*)ctx.userData;
        ctx.userData = nullptr;
        logger.Info("[Demo2D] Shutdown\n");
    }

}} // namespace nkentseu::demo
