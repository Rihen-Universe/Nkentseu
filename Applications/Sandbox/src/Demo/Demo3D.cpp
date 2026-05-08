// =============================================================================
// Demo3D.cpp  — Demo 2
//
// Demo minimaliste 3D :
//   - Config ForGame (RENDER3D + RENDER2D + TEXT + SHADOW + POST_PROCESS + OVERLAY)
//   - Camera 3D orbite autour de l'origine
//   - Mesh primitives : sol (plane) + 4x4 sphere grid + cube central
//   - 1 lumiere directionnelle + 2 lumieres ponctuelles colorees
//
// Demontre le path complet : NkScene/Lights/DrawCalls -> Render3D::Submit
//                            -> RenderGraph -> Flush.
// =============================================================================
#include "DemoCommon.h"

namespace nkentseu { namespace demo {

    struct Demo3DState {
        NkMeshHandle meshSphere;
        NkMeshHandle meshPlane;
        NkMeshHandle meshCube;
        float32      angle = 0.f;     // orbite camera
    };

    bool Demo3D_Init(DemoCtx& ctx) {
        auto* st = new Demo3DState();
        ctx.userData = st;

        auto* meshSys = ctx.renderer->GetMeshSystem();
        st->meshSphere = meshSys->GetIcosphere();
        st->meshPlane  = meshSys->GetPlane();
        st->meshCube   = meshSys->GetCube();

        logger.Info("[Demo3D] Init OK — meshes : sphere={0} plane={1} cube={2}\n",
                    (uint64)st->meshSphere.id,
                    (uint64)st->meshPlane.id,
                    (uint64)st->meshCube.id);
        return true;
    }

    void Demo3D_Frame(DemoCtx& ctx, float32 dt) {
        auto* st = (Demo3DState*)ctx.userData;
        st->angle += dt * 0.45f;

        if (!ctx.renderer->BeginFrame()) return;

        auto* r3d = ctx.renderer->GetRender3D();
        if (!r3d) {
            ctx.renderer->EndFrame();
            ctx.renderer->Present();
            return;
        }

        // ── Camera (orbite autour de l'origine) ─────────────────────────────
        NkCamera3DData camData;
        camData.position  = {cosf(st->angle) * 5.5f, 2.5f, sinf(st->angle) * 5.5f};
        camData.target    = {0.f, 0.5f, 0.f};
        camData.up        = {0.f, 1.f, 0.f};
        camData.fovY      = 60.f;
        camData.aspect    = (float32)ctx.width / (float32)ctx.height;
        camData.nearPlane = 0.1f;
        camData.farPlane  = 100.f;
        NkCamera3D cam(camData);

        // ── Lights ───────────────────────────────────────────────────────────
        NkSceneContext sctx;
        sctx.camera = cam;
        sctx.time   = ctx.totalTime;

        // Soleil directionnel
        NkLightDesc sun;
        sun.type      = NkLightType::NK_DIRECTIONAL;
        sun.direction = {-0.4f, -1.f, -0.3f};
        sun.color     = {1.f, 0.95f, 0.85f};
        sun.intensity = 3.f;
        sun.castShadow= true;
        sctx.lights.PushBack(sun);

        // Lumiere ponctuelle rouge
        NkLightDesc redLight;
        redLight.type      = NkLightType::NK_POINT;
        redLight.position  = {2.f, 1.5f, 0.f};
        redLight.color     = {1.f, 0.2f, 0.1f};
        redLight.intensity = 4.f;
        redLight.range     = 6.f;
        sctx.lights.PushBack(redLight);

        // Fill bleue
        NkLightDesc blue;
        blue.type      = NkLightType::NK_POINT;
        blue.position  = {-2.f, 1.f, 1.f};
        blue.color     = {0.2f, 0.5f, 1.f};
        blue.intensity = 2.5f;
        blue.range     = 8.f;
        sctx.lights.PushBack(blue);

        sctx.ambientIntensity = 0.15f;

        r3d->BeginScene(sctx);

        // ── Sol ──────────────────────────────────────────────────────────────
        {
            NkDrawCall3D dc;
            dc.mesh      = st->meshPlane;
            dc.transform = NkMat4f::Scale({10.f, 1.f, 10.f});
            dc.aabb      = {{-5, 0, -5}, {5, 0, 5}};
            dc.castShadow= false;
            r3d->Submit(dc);
        }

        // ── Grille 4x4 de spheres ────────────────────────────────────────────
        for (int row = 0; row < 4; row++) {
            for (int col = 0; col < 4; col++) {
                float32 x = (col - 1.5f) * 1.2f;
                float32 z = (row - 1.5f) * 1.2f;

                NkDrawCall3D dc;
                dc.mesh      = st->meshSphere;
                dc.transform = NkMat4f::Translate({x, 0.5f, z}) *
                               NkMat4f::Scale({0.45f, 0.45f, 0.45f});
                dc.aabb      = {{x - 0.25f, 0.25f, z - 0.25f},
                                {x + 0.25f, 0.75f, z + 0.25f}};
                dc.tint      = {(float32)col / 3.f, (float32)row / 3.f, 0.7f};
                r3d->Submit(dc);
            }
        }

        // ── Cube central rotatif ─────────────────────────────────────────────
        {
            NkDrawCall3D dc;
            dc.mesh = st->meshCube;
            float32 y = 0.5f + sinf(ctx.totalTime * 1.5f) * 0.2f;
            dc.transform = NkMat4f::Translate({0, y, 0}) *
                           NkMat4f::RotationY(NkAngle::FromRad(ctx.totalTime * 0.8f)) *
                           NkMat4f::Scale({0.6f, 0.6f, 0.6f});
            dc.aabb = {{-0.35f, 0.1f, -0.35f}, {0.35f, 0.9f, 0.35f}};
            dc.tint = {1.f, 0.8f, 0.3f};
            r3d->Submit(dc);
        }

        // Debug visualizations
        r3d->DrawDebugAxes(NkMat4f::Identity(), 1.f);
        r3d->DrawDebugGrid({0, 0, 0}, 1.f, 20, {0.3f, 0.3f, 0.3f, 1.f});

        // ── Overlay ──────────────────────────────────────────────────────────
        if (auto* overlay = ctx.renderer->GetOverlay()) {
            overlay->BeginOverlay(ctx.renderer->GetCmd(), ctx.width, ctx.height);
            overlay->DrawStats(ctx.renderer->GetStats());
            overlay->DrawText({20.f, 35.f}, "Demo 3D — PBR primitives (orbiting camera)");
            overlay->DrawText({20.f, 55.f}, "FPS approx: %.1f  |  dt: %.2f ms",
                              dt > 1e-4f ? 1.f / dt : 0.f, dt * 1000.f);
            overlay->EndOverlay();
        }

        ctx.renderer->EndFrame();
        ctx.renderer->Present();
    }

    void Demo3D_Shutdown(DemoCtx& ctx) {
        delete (Demo3DState*)ctx.userData;
        ctx.userData = nullptr;
        logger.Info("[Demo3D] Shutdown\n");
    }

}} // namespace nkentseu::demo
