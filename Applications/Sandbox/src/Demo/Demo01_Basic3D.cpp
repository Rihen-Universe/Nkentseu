// =============================================================================
// Demo01_Basic3D.cpp  — NKRenderer v4.0
// Rendu PBR 3D : sphères métalliques, sol réfléchissant, lumières, ombres.
// =============================================================================
#include "Demo.h"
#include "../Core/NkCamera.h"
#include <cmath>

namespace nkentseu {
namespace renderer {
namespace demo {

    bool Demo01_Basic3D::Init(NkIDevice* device, const NkSurfaceDesc& surface,
                                uint32 w, uint32 h) {
        mWidth = w; mHeight = h;

        // ── Créer le renderer avec preset Game ────────────────────────────────
        auto cfg = NkRendererConfig::ForGame(NkGraphicsApi::NK_OPENGL, w, h);
        cfg.shadow.enabled    = true;
        cfg.postProcess.ssao  = true;
        cfg.postProcess.bloom = true;
        cfg.postProcess.aces  = true;
        mRenderer = NkRenderer::Create(device, surface, cfg);
        if (!mRenderer) return false;

        auto* meshSys = mRenderer->GetMeshSystem();
        auto* matSys  = mRenderer->GetMaterials();
        auto* texLib  = mRenderer->GetTextures();

        // ── Meshes ────────────────────────────────────────────────────────────
        mMeshSphere = meshSys->GetIcosphere();
        mMeshCube   = meshSys->GetCube();
        mMeshPlane  = meshSys->GetPlane();

        // ── Matériaux ─────────────────────────────────────────────────────────
        // Or métallique
        mMatGold = matSys->CreateInstance(matSys->DefaultPBR());
        mMatGold->SetAlbedo ({1.000f, 0.766f, 0.336f})
                ->SetMetallic(1.0f)
                ->SetRoughness(0.2f);

        // Rouge plastique
        mMatRed = matSys->CreateInstance(matSys->DefaultPBR());
        mMatRed->SetAlbedo ({0.8f, 0.1f, 0.1f})
               ->SetMetallic(0.0f)
               ->SetRoughness(0.6f);

        // Sol en pierre (textures)
        mMatFloor = matSys->CreateInstance(matSys->DefaultPBR());
        NkTexHandle floorAlbedo = texLib->Load("assets/textures/stone_albedo.png");
        NkTexHandle floorNormal = texLib->Load("assets/textures/stone_normal.png");
        NkTexHandle floorORM    = texLib->Load("assets/textures/stone_orm.png");
        if (floorAlbedo.IsValid()) mMatFloor->SetAlbedoMap(floorAlbedo);
        if (floorNormal.IsValid()) mMatFloor->SetNormalMap(floorNormal);
        if (floorORM.IsValid())    mMatFloor->SetORMMap(floorORM);

        // ── Env Map (HDRI) ─────────────────────────────────────────────────────
        mEnvMap = texLib->LoadHDR("assets/hdri/studio_small_09_4k.hdr");

        return true;
    }

    void Demo01_Basic3D::Update(float32 dt) {
        mTime  += dt;
        mAngle += dt * 0.5f;
    }

    void Demo01_Basic3D::Render() {
        if (!mRenderer->BeginFrame()) return;

        auto* r3d = mRenderer->GetRender3D();

        // ── Caméra ────────────────────────────────────────────────────────────
        NkCamera3DData camData;
        camData.position  = {cosf(mAngle)*5.f, 2.5f, sinf(mAngle)*5.f};
        camData.target    = {0, 0.5f, 0};
        camData.fovY      = 65.f;
        camData.aspect    = (float32)mWidth/(float32)mHeight;
        camData.nearPlane = 0.1f;
        camData.farPlane  = 100.f;

        NkCamera3D cam(camData);

        // ── Lumières ──────────────────────────────────────────────────────────
        NkVector<NkLightDesc> lights;

        // Soleil directionnel
        NkLightDesc sun;
        sun.type      = NkLightType::NK_DIRECTIONAL;
        sun.direction = {-0.5f,-1.f,-0.3f};
        sun.color     = {1.f, 0.95f, 0.85f};
        sun.intensity = 3.f;
        sun.castShadow= true;
        lights.PushBack(sun);

        // Lumière ponctuelle rouge
        NkLightDesc ptRed;
        ptRed.type      = NkLightType::NK_POINT;
        ptRed.position  = {2.f, 1.5f, 0.f};
        ptRed.color     = {1.f, 0.2f, 0.1f};
        ptRed.intensity = 5.f;
        ptRed.range     = 6.f;
        lights.PushBack(ptRed);

        // Lumière bleue fill
        NkLightDesc fill;
        fill.type      = NkLightType::NK_POINT;
        fill.position  = {-2.f, 1.f, 1.f};
        fill.color     = {0.2f, 0.4f, 1.f};
        fill.intensity = 2.f;
        fill.range     = 8.f;
        lights.PushBack(fill);

        // ── Scene context ─────────────────────────────────────────────────────
        NkSceneContext ctx;
        ctx.camera             = cam;
        ctx.lights             = lights;
        ctx.envMap             = mEnvMap;
        ctx.ambientIntensity   = 0.15f;
        ctx.time               = mTime;
        r3d->BeginScene(ctx);

        // ── Sol ───────────────────────────────────────────────────────────────
        {
            NkDrawCall3D dc;
            dc.mesh      = mMeshPlane;
            dc.material  = {};   // TODO: handle via matSys
            dc.transform = NkMat4f::Scale({10.f,1.f,10.f});
            dc.aabb      = {{-5,0,-5},{5,0,5}};
            dc.castShadow= false;
            r3d->Submit(dc);
        }

        // ── Sphères en grille 4×4 (metallic/roughness progression) ───────────
        for (int row=0; row<4; row++) {
            for (int col=0; col<4; col++) {
                float32 x = (col - 1.5f) * 1.2f;
                float32 z = (row - 1.5f) * 1.2f;

                NkDrawCall3D dc;
                dc.mesh      = mMeshSphere;
                dc.material  = {};
                dc.transform = NkMat4f::Translate({x, 0.5f, z}) *
                               NkMat4f::Scale({0.45f,0.45f,0.45f});
                dc.aabb      = {{x-0.25f, 0.25f, z-0.25f},{x+0.25f, 0.75f, z+0.25f}};
                dc.tint      = {(float32)col/3.f, (float32)row/3.f, 0.7f};
                r3d->Submit(dc);
            }
        }

        // ── Cube or ───────────────────────────────────────────────────────────
        {
            NkDrawCall3D dc;
            dc.mesh      = mMeshCube;
            dc.material  = {};
            float32 y    = 0.5f + sinf(mTime * 1.5f) * 0.2f;
            dc.transform = NkMat4f::Translate({0, y, 0}) *
                           NkMat4f::RotateY(mTime * 0.8f) *
                           NkMat4f::Scale({0.6f,0.6f,0.6f});
            dc.aabb      = {{-0.35f,0.1f,-0.35f},{0.35f,0.9f,0.35f}};
            r3d->Submit(dc);
        }

        // ── Debug : axes monde + AABB ─────────────────────────────────────────
        r3d->DrawDebugAxes(NkMat4f::Identity(), 1.f);
        r3d->DrawDebugGrid({0,0,0}, 1.f, 20, {0.3f,0.3f,0.3f,1.f});

        // ── Post-process ──────────────────────────────────────────────────────
        auto* pp = mRenderer->GetPostProcess();
        pp->GetConfig().exposure = 1.f + sinf(mTime * 0.3f) * 0.1f;

        // ── Overlay stats ─────────────────────────────────────────────────────
        auto* overlay = mRenderer->GetOverlay();
        overlay->DrawStats(mRenderer->GetStats());
        overlay->DrawText({10, 30},
            "Demo 01 — PBR 3D  |  %.1f fps  |  %.2f ms",
            1.f / 0.016f, 16.f);

        mRenderer->EndFrame();
        mRenderer->Present();
    }

    void Demo01_Basic3D::Shutdown() {
        if (mRenderer) {
            auto* matSys = mRenderer->GetMaterials();
            if (mMatGold)  matSys->DestroyInstance(mMatGold);
            if (mMatRed)   matSys->DestroyInstance(mMatRed);
            if (mMatFloor) matSys->DestroyInstance(mMatFloor);
            NkRenderer::Destroy(mRenderer);
        }
    }

} // namespace demo
} // namespace renderer
} // namespace nkentseu
