// =============================================================================
// render_usage_example.cpp
// Exemple complet : NkWindow + NkRHIDevice + NkSceneRenderer
// Rendu de plusieurs objets 3D avec Vulkan, multi-plateforme.
// =============================================================================

#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Graphics/NkGraphicsContextFactory.h"
#include "NKLogger/NkLog.h"
#include "NkRHIDevice.h"
#include "NkVulkanDevice.h"
#include "NkRenderer.cpp"  // en production : header séparé

using namespace nkentseu;
using namespace nkentseu::rhi;

// Géométrie d'un cube simple
static const NkVertex kCubeVertices[] = {
    // front face
    {{-0.5f,-0.5f, 0.5f},{0,0,1},{0,0},{1,1,1,1}},
    {{ 0.5f,-0.5f, 0.5f},{0,0,1},{1,0},{1,1,1,1}},
    {{ 0.5f, 0.5f, 0.5f},{0,0,1},{1,1},{1,1,1,1}},
    {{-0.5f, 0.5f, 0.5f},{0,0,1},{0,1},{1,1,1,1}},
    // back face
    {{ 0.5f,-0.5f,-0.5f},{0,0,-1},{0,0},{1,1,1,1}},
    {{-0.5f,-0.5f,-0.5f},{0,0,-1},{1,0},{1,1,1,1}},
    {{-0.5f, 0.5f,-0.5f},{0,0,-1},{1,1},{1,1,1,1}},
    {{ 0.5f, 0.5f,-0.5f},{0,0,-1},{0,1},{1,1,1,1}},
    // top face
    {{-0.5f, 0.5f, 0.5f},{0,1,0},{0,0},{1,1,1,1}},
    {{ 0.5f, 0.5f, 0.5f},{0,1,0},{1,0},{1,1,1,1}},
    {{ 0.5f, 0.5f,-0.5f},{0,1,0},{1,1},{1,1,1,1}},
    {{-0.5f, 0.5f,-0.5f},{0,1,0},{0,1},{1,1,1,1}},
    // bottom face
    {{-0.5f,-0.5f,-0.5f},{0,-1,0},{0,0},{1,1,1,1}},
    {{ 0.5f,-0.5f,-0.5f},{0,-1,0},{1,0},{1,1,1,1}},
    {{ 0.5f,-0.5f, 0.5f},{0,-1,0},{1,1},{1,1,1,1}},
    {{-0.5f,-0.5f, 0.5f},{0,-1,0},{0,1},{1,1,1,1}},
    // right face
    {{ 0.5f,-0.5f, 0.5f},{1,0,0},{0,0},{1,1,1,1}},
    {{ 0.5f,-0.5f,-0.5f},{1,0,0},{1,0},{1,1,1,1}},
    {{ 0.5f, 0.5f,-0.5f},{1,0,0},{1,1},{1,1,1,1}},
    {{ 0.5f, 0.5f, 0.5f},{1,0,0},{0,1},{1,1,1,1}},
    // left face
    {{-0.5f,-0.5f,-0.5f},{-1,0,0},{0,0},{1,1,1,1}},
    {{-0.5f,-0.5f, 0.5f},{-1,0,0},{1,0},{1,1,1,1}},
    {{-0.5f, 0.5f, 0.5f},{-1,0,0},{1,1},{1,1,1,1}},
    {{-0.5f, 0.5f,-0.5f},{-1,0,0},{0,1},{1,1,1,1}},
};

static const NkU32 kCubeIndices[] = {
    0,1,2, 2,3,0,   // front
    4,5,6, 6,7,4,   // back
    8,9,10, 10,11,8, // top
    12,13,14, 14,15,12, // bottom
    16,17,18, 18,19,16, // right
    20,21,22, 22,23,20, // left
};

// Utilitaire : créer une texture 1×1 de couleur unie
static NkTextureHandle CreateSolidTexture(NkRHIDevice& dev,
                                           uint8_t r, uint8_t g, uint8_t b, uint8_t a=255) {
    uint8_t data[4] = { r, g, b, a };
    NkTextureDesc td;
    td.width=1; td.height=1;
    td.format = NkTextureFormat::RGBA8_SRGB;
    td.usage   = NkTextureUsage::Sampled | NkTextureUsage::TransferDst;
    NkTextureHandle tex = dev.CreateTexture(td);
    // TODO: upload via staging (voir NkVulkanDevice::UploadTexture)
    return tex;
}

// Matrice identité simplifiée (en production : utiliser NkMat4x4)
static void MakeIdentity(float m[16]) {
    for (int i = 0; i < 16; ++i) m[i] = 0.f;
    m[0]=m[5]=m[10]=m[15]=1.f;
}
static void MakeTranslation(float m[16], float x, float y, float z) {
    MakeIdentity(m);
    m[12]=x; m[13]=y; m[14]=z;
}

int main() {
    // ── 1. Fenêtre ───────────────────────────────────────────────────────────
    NkWindowConfig wcfg;
    wcfg.title  = "NkRenderer — Multi-objet Vulkan";
    wcfg.width  = 1280;
    wcfg.height = 720;

    // Vulkan n'a pas besoin de PrepareWindowConfig
    NkWindow window(wcfg);

    // ── 2. RHI Device ────────────────────────────────────────────────────────
    auto gcfg    = NkGraphicsContextConfig::ForVulkan(/*validation=*/true);
    NkSurfaceDesc surface = window.GetSurfaceDesc();

    auto device = NkRHIDeviceFactory::Create(surface, gcfg);
    if (!device) { logger.Error("Vulkan init failed"); return -1; }

    // ── 3. Scene Renderer ────────────────────────────────────────────────────
    NkSceneRenderer renderer(*device);
    renderer.Init(wcfg.width, wcfg.height);

    // ── 4. Ressources de scène ───────────────────────────────────────────────

    // Mesh cube partagé par tous les objets
    NkMesh cubeMesh = NkMesh::Create(*device,
        kCubeVertices, 24,
        kCubeIndices,  36);

    // Textures de couleur
    NkTextureHandle whiteAlbedo  = CreateSolidTexture(*device, 255, 255, 255);
    NkTextureHandle redAlbedo    = CreateSolidTexture(*device, 255,  50,  50);
    NkTextureHandle greenAlbedo  = CreateSolidTexture(*device,  50, 200,  50);
    NkTextureHandle blueAlbedo   = CreateSolidTexture(*device,  50, 100, 255);
    NkTextureHandle flatNormal   = CreateSolidTexture(*device, 128, 128, 255); // normal plat

    // Matériaux
    NkMaterial matWhite = renderer.CreatePBRMaterial(whiteAlbedo, flatNormal, 0.8f, 0.0f);
    NkMaterial matRed   = renderer.CreatePBRMaterial(redAlbedo,   flatNormal, 0.4f, 0.0f);
    NkMaterial matMetal = renderer.CreatePBRMaterial(blueAlbedo,  flatNormal, 0.2f, 1.0f);

    // ── 5. Ajouter des objets en scène ───────────────────────────────────────
    // Cubes à différentes positions

    // Cube rouge au centre
    {
        NkRenderObject obj;
        obj.mesh = &cubeMesh;
        obj.material = &matRed;
        MakeTranslation(obj.transform, 0.f, 0.f, 0.f);
        obj.color[0]=1.f; obj.color[1]=0.3f; obj.color[2]=0.3f; obj.color[3]=1.f;
        renderer.AddObject(obj);
    }
    // Cube blanc à gauche
    {
        NkRenderObject obj;
        obj.mesh = &cubeMesh;
        obj.material = &matWhite;
        MakeTranslation(obj.transform, -2.5f, 0.f, 0.f);
        obj.color[0]=1.f; obj.color[1]=1.f; obj.color[2]=1.f; obj.color[3]=1.f;
        renderer.AddObject(obj);
    }
    // Cube métallique à droite
    {
        NkRenderObject obj;
        obj.mesh = &cubeMesh;
        obj.material = &matMetal;
        MakeTranslation(obj.transform, 2.5f, 0.f, 0.f);
        obj.color[0]=0.3f; obj.color[1]=0.6f; obj.color[2]=1.f; obj.color[3]=1.f;
        renderer.AddObject(obj);
    }
    // 10 cubes verts en grille
    {
        NkMaterial matGreen = renderer.CreatePBRMaterial(greenAlbedo, flatNormal, 0.6f, 0.0f);
        for (int row = 0; row < 2; ++row) {
            for (int col = 0; col < 5; ++col) {
                NkRenderObject obj;
                obj.mesh = &cubeMesh;
                obj.material = &matGreen;
                MakeTranslation(obj.transform,
                    (col - 2.f) * 1.5f,
                    -2.5f + row * 1.5f,
                    -3.f);
                obj.color[0]=0.2f; obj.color[1]=0.8f; obj.color[2]=0.2f; obj.color[3]=1.f;
                renderer.AddObject(obj);
            }
        }
    }

    // ── 6. Camera ────────────────────────────────────────────────────────────
    // Matrices simplifiées — en production utiliser NkMath::LookAt / Perspective
    CameraUBO cam{};
    // view = identité (caméra à l'origine, regardant -Z)
    MakeIdentity(cam.view);
    cam.view[14] = -5.f;   // recul de 5 unités
    // proj = perspective approximée (en production : NkMat4x4::Perspective)
    MakeIdentity(cam.proj);
    cam.proj[0]  =  1.f;   // cotangente du demi-angle
    cam.proj[5]  =  1.f;
    cam.proj[10] = -1.f;
    cam.proj[11] = -1.f;
    cam.proj[14] = -0.1f;
    cam.viewPos[0] = 0.f; cam.viewPos[1] = 0.f; cam.viewPos[2] = 5.f;
    renderer.SetCamera(cam);

    // ── 7. Boucle principale ─────────────────────────────────────────────────
    NkU32 frameIndex = 0;

    while (window.IsOpen()) {
        NkSystem::Events().PollEvents();

        // Vérifier resize
        NkVec2u sz = window.GetSize();
        if (sz.x != device->GetWidth() || sz.y != device->GetHeight()) {
            device->WaitIdle();
            device->Recreate(sz.x, sz.y);
            renderer.OnResize(sz.x, sz.y);
            // Mettre à jour le viewport proj
            cam.proj[0] = (float)sz.y / (float)sz.x;
            renderer.SetCamera(cam);
        }

        if (!device->BeginFrame()) {
            device->Recreate(sz.x, sz.y);
            renderer.OnResize(sz.x, sz.y);
            continue;
        }

        auto& cmd = device->GetCurrentCommandBuffer();
        renderer.Render(cmd, frameIndex);

        device->EndFrame(cmd);
        ++frameIndex;
    }

    // ── 8. Nettoyage ─────────────────────────────────────────────────────────
    device->WaitIdle();
    renderer.Shutdown();
    cubeMesh.Destroy(*device);
    device->DestroyTexture(whiteAlbedo);
    device->DestroyTexture(redAlbedo);
    device->DestroyTexture(greenAlbedo);
    device->DestroyTexture(blueAlbedo);
    device->DestroyTexture(flatNormal);
    device->Shutdown();

    return 0;
}
