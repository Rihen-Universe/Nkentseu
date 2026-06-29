// =============================================================================
// DemoGLTF.cpp  — Demo glTF : chargement + affichage 3D d'un modele glTF
//
// Charge un modele glTF 2.0 (geometrie + materiaux PBR + textures) via le
// loader from-scratch NkGLTFLoader, convertit les materiaux en
// NkMaterialInstance via le pont NkGLTFMaterialBridge, et affiche le modele
// avec une camera orbitale + 1 soleil directionnel + ambient.
//
// Modele par defaut : Resources/Models/rubber_duck/scene.gltf
//   (5676 vertices / 33216 indices, 1 materiau "Duck" avec baseColorTexture
//    Duck_baseColor.png en URI externe).
//
// Path overridable via --model=<chemin>.
//
// Demontre le path complet glTF -> mesh GPU -> materiau PBR -> Render3D.
// =============================================================================
#include "DemoCommon.h"
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKRenderer/Mesh/NkOBJLoader.h"
#include "NKRenderer/Mesh/NkSTLLoader.h"
#include "NKRenderer/Mesh/NkPLYLoader.h"
#include "NKRenderer/Mesh/NkFBXLoader.h"
#include "NKRenderer/Mesh/NkDAELoader.h"
#include "NKRenderer/Mesh/NkUSDALoader.h"
#include "NKRenderer/Mesh/NkGLTFMaterialBridge.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKWindow/Core/NkWESystem.h"
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkKeyboardEvent.h"
#include <cmath>

namespace nkentseu { namespace demo {

    struct DemoGLTFState {
        NkMeshHandle      mesh;
        NkGLTFMaterialSet materials;        // 1 NkMatInstHandle par materiau glTF
        NkVector<int32>   subMeshMaterial;  // index materiau glTF par submesh
        NkAABB            bounds;
        NkString          modelName;
        float32           angle    = 0.f;   // orbite camera
        float32           radius   = 4.f;   // distance camera (calculee depuis bounds)
        NkVec3f           center   = {0.f, 0.f, 0.f};
        bool              loaded   = false;
        uint32            vertexCount = 0;
        uint32            indexCount  = 0;
        uint32            matCount    = 0;
    };

    // Path du modele de test : rubber_duck (geometrie + materiau PBR +
    // baseColorTexture externe). Variable d'env NK_GLTF_MODEL pour override
    // rapide sans toucher au code (ex. tester car.glb).
    static NkString PickModelPath() {
        const char* env = getenv("NK_GLTF_MODEL");
        if (env && env[0]) return NkString(env);
        return NkString("Resources/Models/rubber_duck/scene.gltf");
    }

    bool DemoGLTF_Init(DemoCtx& ctx) {
        auto* st = new DemoGLTFState();
        ctx.userData = st;

        auto* meshSys = ctx.renderer->GetMeshSystem();
        auto* matSys  = ctx.renderer->GetMaterials();
        auto* texLib  = ctx.renderer->GetTextures();
        if (!meshSys || !matSys || !texLib) {
            logger.Errorf("[DemoGLTF] sous-systemes manquants (mesh/mat/tex)\n");
            delete st; ctx.userData = nullptr; return false;
        }

        NkString path = PickModelPath();
        st->modelName = path;

        // ── Chargement geometrie + materiaux (dispatch par extension) ────────
        // Tous les loaders from-scratch produisent NkGLTFMeshData (format CPU
        // commun) -> le pont materiaux + ce viewer marchent pour TOUS les formats.
        NkString lower = path;
        for (uint32 i = 0; i < (uint32)lower.Size(); ++i) {
            char c = lower[i]; if (c >= 'A' && c <= 'Z') lower[i] = (char)(c + 32);
        }
        NkGLTFMeshData data;
        bool loaded = false;
        if      (lower.EndsWith(".gltf") || lower.EndsWith(".glb")) loaded = LoadGLTF(path, data);
        else if (lower.EndsWith(".obj"))                            loaded = LoadOBJ(path, data);
        else if (lower.EndsWith(".stl"))                            loaded = LoadSTL(path, data);
        else if (lower.EndsWith(".ply"))                            loaded = LoadPLY(path, data);
        else if (lower.EndsWith(".fbx"))                            loaded = LoadFBX(path, data);
        else if (lower.EndsWith(".dae"))                            loaded = LoadDAE(path, data);
        else if (lower.EndsWith(".usda") || lower.EndsWith(".usd"))  loaded = LoadUSDA(path, data);
        if (!loaded) {
            logger.Errorf("[DemoGLTF] echec chargement modele : %s\n", path.CStr());
            // On garde la demo vivante (affiche overlay d'erreur) mais sans mesh.
            return true;
        }

        // Cree le mesh GPU depuis les buffers CPU du loader.
        NkMeshDesc d;
        d.layout      = renderer::NkVertexLayout::Default3D();
        d.vertices    = data.vertices.Data();
        d.vertexCount = (uint32)data.vertices.Size();
        d.indices     = data.indices.Data();
        d.indexCount  = (uint32)data.indices.Size();
        d.subMeshes   = data.subMeshes;
        d.bounds      = data.bounds;
        d.debugName   = data.debugName;
        st->mesh        = meshSys->Create(d);
        st->bounds      = data.bounds;
        st->vertexCount = (uint32)data.vertices.Size();
        st->indexCount  = (uint32)data.indices.Size();

        // Copie le mapping submesh -> materiau glTF (parallele a subMeshes).
        st->subMeshMaterial = data.subMeshMaterial;

        // Convertit les materiaux glTF en NkMaterialInstance via le pont.
        BuildGLTFMaterials(data, matSys, texLib, st->materials);
        st->matCount = (uint32)st->materials.instances.Size();

        // ── Cadre la camera sur l'englobant du modele ────────────────────────
        // center = milieu de l'AABB, radius = ~1.8x la demi-diagonale.
        NkVec3f mn = st->bounds.min, mx = st->bounds.max;
        st->center = { (mn.x + mx.x) * 0.5f,
                       (mn.y + mx.y) * 0.5f,
                       (mn.z + mx.z) * 0.5f };
        NkVec3f ext = { (mx.x - mn.x) * 0.5f,
                        (mx.y - mn.y) * 0.5f,
                        (mx.z - mn.z) * 0.5f };
        float32 diag = std::sqrt(ext.x*ext.x + ext.y*ext.y + ext.z*ext.z);
        st->radius = (diag > 1e-4f) ? diag * 2.4f : 4.f;
        if (st->radius < 0.5f) st->radius = 0.5f;

        st->loaded = st->mesh.IsValid() && st->vertexCount > 0;

        logger.Info("[DemoGLTF] Init OK — '{0}' : {1} v / {2} i, {3} materiaux, "
                    "center=({4},{5},{6}) radius={7}\n",
                    path.CStr(), st->vertexCount, st->indexCount, st->matCount,
                    st->center.x, st->center.y, st->center.z, st->radius);
        return true;
    }

    void DemoGLTF_Frame(DemoCtx& ctx, float32 dt) {
        auto* st = (DemoGLTFState*)ctx.userData;

        // Camera fixe optionnelle (compare backends) : NK_FIX_CAM.
        static int fixcam = -1;
        if (fixcam == -1) { const char* v = getenv("NK_FIX_CAM"); fixcam = (v && v[0] && v[0] != '0') ? 1 : 0; }
        if (fixcam) { st->angle = 0.7f; ctx.totalTime = 1.0f; }
        else        { st->angle += dt * 0.5f; }

        if (!ctx.renderer->BeginFrame()) return;

        auto* r3d = ctx.renderer->GetRender3D();
        if (!r3d) {
            ctx.renderer->Present();
            ctx.renderer->EndFrame();
            return;
        }

        // ── Camera orbitale autour du centre du modele ───────────────────────
        NkCamera3DData camData;
        camData.position  = { st->center.x + cosf(st->angle) * st->radius,
                              st->center.y + st->radius * 0.45f,
                              st->center.z + sinf(st->angle) * st->radius };
        camData.target    = st->center;
        camData.up        = {0.f, 1.f, 0.f};
        camData.fovY      = 55.f;
        camData.aspect    = (float32)ctx.width / (float32)ctx.height;
        camData.nearPlane = 0.05f;
        camData.farPlane  = NkMax(100.f, st->radius * 8.f);
        NkCamera3D cam(camData);

        // ── Lumieres : 1 soleil directionnel + fill + ambient ────────────────
        NkSceneContext sctx;
        sctx.camera = cam;
        sctx.time   = ctx.totalTime;

        NkLightDesc sun;
        sun.type       = NkLightType::NK_DIRECTIONAL;
        sun.direction  = {-0.4f, -1.f, -0.35f};
        sun.color      = {1.f, 0.97f, 0.9f};
        sun.intensity  = 3.2f;
        sun.castShadow = true;
        sun.shadowStatic = true;
        sctx.lights.PushBack(sun);

        NkLightDesc fill;
        fill.type      = NkLightType::NK_POINT;
        fill.position  = { st->center.x, st->center.y + st->radius,
                           st->center.z + st->radius };
        fill.color     = {0.4f, 0.5f, 0.9f};
        fill.intensity = 2.0f;
        fill.range     = st->radius * 6.f;
        sctx.lights.PushBack(fill);

        sctx.ambientIntensity = 0.25f;

        r3d->BeginScene(sctx);

        // ── Sol sous le modele (reference visuelle + receveur d'ombre) ───────
        if (auto* meshSys = ctx.renderer->GetMeshSystem()) {
            float32 floorY = st->bounds.min.y;
            float32 s = NkMax(4.f, st->radius * 2.f);
            NkDrawCall3D dc;
            dc.mesh      = meshSys->GetPlane();
            dc.transform = NkMat4f::Translate({st->center.x, floorY, st->center.z}) *
                           NkMat4f::Scale({s, 1.f, s});
            dc.aabb      = {{st->center.x - s, floorY - 0.01f, st->center.z - s},
                            {st->center.x + s, floorY + 0.01f, st->center.z + s}};
            dc.tint      = {0.35f, 0.35f, 0.4f};
            dc.roughness = 0.85f;
            dc.castShadow= false;
            r3d->Submit(dc);
        }

        // ── Le modele glTF ───────────────────────────────────────────────────
        if (st->loaded) {
            NkDrawCall3D dc;
            dc.mesh      = st->mesh;
            dc.transform = NkMat4f::Identity();
            dc.aabb      = st->bounds;
            dc.castShadow    = true;
            dc.receiveShadow = true;
            dc.tint      = {1.f, 1.f, 1.f};
            dc.roughness = 0.6f;

            // Multi-material : un slot par submesh, mappe via subMeshMaterial.
            // Si un seul materiau s'applique a tout, on l'assigne en `material`
            // global (plus simple et evite une boucle inutile).
            uint32 subCount = (uint32)st->subMeshMaterial.Size();
            bool   anySlot  = false;
            if (subCount > 0 && st->matCount > 0) {
                for (uint32 s = 0; s < subCount; ++s) {
                    NkMatInstHandle h =
                        st->materials.InstanceForMaterial(st->subMeshMaterial[s]);
                    dc.materialSlots.PushBack(h);
                    if (h.IsValid()) anySlot = true;
                }
            }
            // Fallback : premier materiau valide en single-material.
            if (!anySlot) {
                dc.materialSlots.Clear();
                for (uint32 m = 0; m < st->materials.instances.Size(); ++m) {
                    if (st->materials.instances[m].IsValid()) {
                        dc.material = st->materials.instances[m];
                        break;
                    }
                }
            }
            r3d->Submit(dc);
        }

        r3d->DrawDebugAxes(NkMat4f::Translate(st->center), st->radius * 0.4f);

        // ── Overlay ──────────────────────────────────────────────────────────
        if (auto* overlay = ctx.renderer->GetOverlay()) {
            overlay->BeginOverlay(ctx.renderer->GetCmd(), ctx.width, ctx.height);
            overlay->DrawStats(ctx.renderer->GetStats());
            overlay->DrawText({20.f, 35.f}, "DemoGLTF  |  API : %s",
                              NkGraphicsApiName(ctx.api));
            overlay->DrawText({20.f, 55.f}, "Model : %s", st->modelName.CStr());
            if (st->loaded) {
                overlay->DrawText({20.f, 75.f},
                    "verts: %u  indices: %u  materials: %u",
                    st->vertexCount, st->indexCount, st->matCount);
            } else {
                overlay->DrawText({20.f, 75.f}, "** CHARGEMENT ECHOUE **");
            }
            overlay->DrawText({20.f, 95.f}, "FPS : %.0f  dt: %.2f ms",
                              dt > 1e-5f ? 1.f / dt : 0.f, dt * 1000.f);
            overlay->EndOverlay();
        }

        ctx.renderer->Present();
        ctx.renderer->EndFrame();
    }

    void DemoGLTF_Shutdown(DemoCtx& ctx) {
        delete (DemoGLTFState*)ctx.userData;
        ctx.userData = nullptr;
        logger.Info("[DemoGLTF] Shutdown\n");
    }

}} // namespace nkentseu::demo
