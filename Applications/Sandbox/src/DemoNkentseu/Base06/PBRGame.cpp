// =============================================================================
// Example_PBRGame.cpp
// Exemple complet : scène 3D PBR style jeu vidéo (type Unreal/UPBGE)
//
// Démontre :
//   - Initialisation NkRenderer avec préréglage "game"
//   - Chargement de textures et matériaux PBR
//   - Lumières multiples (directionnelle + points)
//   - Ombres CSM
//   - Post-process (bloom, SSAO, tone mapping ACES)
//   - Debug overlay
//   - Camera first-person
// =============================================================================
#include "Core/NkRenderer.h"
#include "Materials/NkMaterialSystem.h"
#include "NKLogger/NkLog.h"

using namespace nkentseu;

// ─── Données d'exemple ────────────────────────────────────────────────────────

// Géométrie d'une sphère UV (normalement chargée depuis un .obj / .gltf)
static const NkVertexStatic kSphereVertices[] = { /* ... */ };
static const uint32         kSphereIndices[]  = { /* ... */ };
static const NkAABB         kSphereBounds     = {{-1,-1,-1},{1,1,1}};

// ─── Scène ───────────────────────────────────────────────────────────────────

struct PBRGameScene {
    NkRenderer*   renderer   = nullptr;
    NkIDevice*    device     = nullptr;

    // Assets
    NkTexId   texAlbedo   = NK_INVALID_TEX;
    NkTexId   texNormal   = NK_INVALID_TEX;
    NkTexId   texMetalRough = NK_INVALID_TEX;
    NkTexId   texAO       = NK_INVALID_TEX;
    NkTexId   texSkyHDRI  = NK_INVALID_TEX;
    NkMeshId  meshSphere  = NK_INVALID_MESH;
    NkMeshId  meshGround  = NK_INVALID_MESH;

    // Matériaux
    NkMatId matIron      = NK_INVALID_MAT;
    NkMatId matGround    = NK_INVALID_MAT;
    NkMatId matEmissive  = NK_INVALID_MAT;
    NkMatId matGlass     = NK_INVALID_MAT;

    // Caméra
    NkCamera3D camera;
    float32    yaw = 0.f, pitch = 0.f;

    // ── Initialisation ────────────────────────────────────────────────────────────
    bool Init(NkIDevice* dev, uint32 w, uint32 h) {
        device = dev;

        // Configuration du renderer style jeu haute qualité
        NkRendererConfig cfg = NkRendererConfig::ForGame(dev->GetApi(), w, h);
        cfg.shadow.enabled       = true;
        cfg.shadow.cascadeCount  = 4;
        cfg.shadow.resolution    = 2048;
        cfg.shadow.softShadows   = true;
        cfg.postProcess.bloom    = true;
        cfg.postProcess.bloomStrength = 0.08f;
        cfg.postProcess.ssao     = true;
        cfg.postProcess.ssaoRadius = 0.3f;
        cfg.postProcess.fxaa     = true;
        cfg.postProcess.toneMapping = true;
        cfg.sky.mode = NkSkyMode::NK_HDRI;

        renderer = NkRenderer::Create(dev, cfg);
        if (!renderer) return false;

        auto* tex   = renderer->GetTextures();
        auto* mesh  = renderer->GetMeshCache();
        auto* mats  = renderer->GetMaterials();

        // ── Textures ──────────────────────────────────────────────────────────────
        texAlbedo    = tex->Load("Assets/Textures/iron_albedo.png",   true,  true);
        texNormal    = tex->Load("Assets/Textures/iron_normal.png",   false, false);
        texMetalRough= tex->Load("Assets/Textures/iron_metalrough.png",false, true);
        texAO        = tex->Load("Assets/Textures/iron_ao.png",       false, true);
        texSkyHDRI   = tex->LoadCubemapEquirect("Assets/HDR/sky_clear.hdr");

        // Fallback si les textures ne sont pas disponibles
        if (texAlbedo == NK_INVALID_TEX)
            texAlbedo = tex->CreateSolid(200, 200, 200);
        if (texNormal == NK_INVALID_TEX)
            texNormal = tex->GetNormal();

        // ── Meshes ────────────────────────────────────────────────────────────────
        meshSphere = mesh->GetPrimitive(NkPrimitive::NK_ICOSPHERE, 4);
        meshGround = mesh->GetPrimitive(NkPrimitive::NK_PLANE, 1);

        // ── Matériaux ─────────────────────────────────────────────────────────────
        // Iron PBR
        matIron = mats->CreatePBR(texAlbedo, texNormal, 0.9f, 0.4f);
        auto* matIronPtr = mats->Get(matIron);
        matIronPtr->textures.metalRough = texMetalRough;
        matIronPtr->textures.ao         = texAO;
        matIronPtr->pbr.uvScale         = {2.f, 2.f};
        mats->MarkDirty(matIron);

        // Ground (sol légèrement rugueux)
        NkTexId groundAlbedo = tex->CreateCheckerboard(512, 64,
            {0.3f,0.3f,0.3f,1}, {0.4f,0.4f,0.4f,1});
        matGround = mats->CreatePBR(groundAlbedo, NK_INVALID_TEX, 0.0f, 0.8f);
        auto* matGnd = mats->Get(matGround);
        matGnd->pbr.uvScale = {10.f, 10.f};
        mats->MarkDirty(matGround);

        // Emissive (boules lumineuses)
        matEmissive = mats->CreateEmissive({1.f, 0.6f, 0.1f}, 8.f);

        // Verre
        matGlass = mats->CreateGlass(1.52f, 0.02f);

        // ── Sky / Environment ─────────────────────────────────────────────────────
        auto* r3d = renderer->GetRender3D();
        if (texSkyHDRI != NK_INVALID_TEX)
            r3d->SetEnvironmentMap(texSkyHDRI, 1.f, 0.f);

        // ── Caméra ────────────────────────────────────────────────────────────────
        camera.position   = {0.f, 2.f, 8.f};
        camera.target     = {0.f, 0.f, 0.f};
        camera.fovY       = 60.f;
        camera.nearPlane  = 0.1f;
        camera.farPlane   = 500.f;
        camera.aspectRatio= (float32)w / (float32)h;

        logger.Infof("[PBRGame] Scene initialized\n");
        return true;
    }

    // ── Update ────────────────────────────────────────────────────────────────────
    void Update(float32 dt) {
        // Rotation automatique de la caméra
        yaw += dt * 10.f;  // degrés/s
        float32 rad = yaw * 3.14159f / 180.f;
        camera.position = {sinf(rad)*8.f, 2.f, cosf(rad)*8.f};
        camera.target   = {0.f, 0.5f, 0.f};
    }

    // ── Render ────────────────────────────────────────────────────────────────────
    void Render(float32 dt, float32 time) {
        if (!renderer->BeginFrame()) return;

        auto* r3d = renderer->GetRender3D();

        // ── Caméra ────────────────────────────────────────────────────────────────
        r3d->SetCamera(camera);

        // ── Soleil (lumière directionnelle avec ombres) ────────────────────────────
        r3d->SetSun(
            {-0.3f, -1.f, -0.5f},   // direction
            {1.f, 0.95f, 0.8f},     // couleur chaude
            5.f,                     // intensité
            true                     // cast shadows
        );
        r3d->SetAmbient({0.05f, 0.07f, 0.12f}, 1.f);

        // ── Lumières ponctuelles colorées ─────────────────────────────────────────
        r3d->ClearLights();
        r3d->AddLight(NkLight::Directional({-0.3f,-1.f,-0.5f},
                                            {1.f,0.95f,0.8f}, 5.f, true));
        // Points lights dynamiques
        float32 a = time * 0.5f;
        r3d->AddLight(NkLight::Point(
            {sinf(a)*3.f, 1.5f, cosf(a)*3.f}, {1,0.3f,0.1f}, 4.f, 8.f));
        r3d->AddLight(NkLight::Point(
            {sinf(a+2.1f)*3.f, 1.5f, cosf(a+2.1f)*3.f}, {0.1f,0.5f,1.f}, 4.f, 8.f));
        r3d->AddLight(NkLight::Point(
            {sinf(a+4.2f)*3.f, 1.5f, cosf(a+4.2f)*3.f}, {0.2f,1.f,0.3f}, 4.f, 8.f));

        // ── Sol ───────────────────────────────────────────────────────────────────
        r3d->DrawMesh(meshGround,
                       NkTransform3D::TRS({0,-0.5f,0},{0,0,0},{20,1,20}),
                       matGround);

        // ── Grille de sphères PBR ─────────────────────────────────────────────────
        // Variation de metallic et roughness sur une grille 7x7
        for (int32 ix = -3; ix <= 3; ++ix) {
            for (int32 iz = -3; iz <= 3; ++iz) {
                float32 metallic  = (float32)(ix+3) / 6.f;
                float32 roughness = (float32)(iz+3) / 6.f;

                NkMatId mat = mats->Create(NkMaterialType::NK_PBR_METALLIC);
                auto* m = mats->Get(mat);
                m->pbr.albedo   = {0.8f, 0.6f, 0.4f, 1.f};
                m->pbr.metallic  = metallic;
                m->pbr.roughness = roughness;
                mats->MarkDirty(mat);

                r3d->DrawMesh(meshSphere,
                               NkTransform3D::TRS(
                                   {ix*2.f, 0.f, iz*2.f},
                                   {0,0,0}, {0.8f,0.8f,0.8f}),
                               mat);
            }
        }

        // ── Sphère de verre au centre ─────────────────────────────────────────────
        r3d->DrawMesh(meshSphere,
                       NkTransform3D::TRS({0, 1.5f, 0},{0,0,0},{1.5f,1.5f,1.5f}),
                       matGlass);

        // ── Sphères émissives (lampes) ─────────────────────────────────────────────
        for (int32 i = 0; i < 3; ++i) {
            float32 angle = time * 0.5f + i * 2.094f;
            r3d->DrawMesh(meshSphere,
                           NkTransform3D::TRS(
                               {sinf(angle)*3.f, 1.5f, cosf(angle)*3.f},
                               {0,0,0}, {0.2f,0.2f,0.2f}),
                           matEmissive);
        }

        // ── Debug overlay ─────────────────────────────────────────────────────────
        auto* overlay = renderer->GetOverlay();
        overlay->Begin(mCmd_placeholder(), camera);
        overlay->DrawGrid(30, 1.f);
        overlay->DrawAxes({0,0,0}, 2.f);
        overlay->DrawStats(true, {10,10});
        overlay->End();

        renderer->EndFrame();
    }

    // Placeholder — dans l'implémentation réelle, le cmd est géré par NkRenderer
    NkICommandBuffer* mCmd_placeholder() { return renderer->GetCommandBuffer(); }

    void Destroy() {
        if (renderer) NkRenderer::Destroy(renderer);
    }
};

// ─── Entrée ──────────────────────────────────────────────────────────────────

// Dans nkmain() :
//
//   PBRGameScene scene;
//   scene.Init(device, 1920, 1080);
//
//   while (running) {
//       scene.Update(dt);
//       scene.Render(dt, totalTime);
//   }
//
//   scene.Destroy();