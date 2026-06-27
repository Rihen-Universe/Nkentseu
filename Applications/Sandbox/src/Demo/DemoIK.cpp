// =============================================================================
// DemoIK.cpp — NkAnima M0 : démo IK FABRIK temps réel
// -----------------------------------------------------------------------------
// Première preuve VISIBLE que l'IK (NkIKSystem) est fonctionnel : une chaîne
// d'os procédurale (racine ancrée) dont l'effecteur SUIT une cible animée via
// FABRIK. Rendu en squelette debug (lignes = os, sphères = joints, sphère rouge
// = cible). Aucun mesh/skinning — on valide le SOLVEUR pur.
//   renderdemo --demo=14   (cf remap dans main.cpp)
// =============================================================================
#include "DemoCommon.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/IK/NkIKSystem.h"
#include "NKLogger/NkLog.h"
#include <cmath>

namespace nkentseu { namespace demo {

    using namespace nkentseu::renderer;

    struct DemoIKState {
        NkIKSystem          ik;             // full-CPU (pas d'Init nécessaire)
        NkIKRig*            rig = nullptr;
        NkIKChainId         chainId{};
        NkVector<NkMat4f>   pose;           // matrices monde par os (persistées)
        uint32              nbones = 5;      // racine + 4 segments
        float32             segLen = 1.f;
    };

    bool DemoIK_Init(DemoCtx& ctx) {
        auto* st = new DemoIKState();
        ctx.userData = st;

        // Rig + chaîne FABRIK : nbones os, le 1er = racine (length 0), les autres
        // séparés de segLen. boneIdx = i (le solveur lit/écrit pose[boneIdx]).
        st->rig = st->ik.CreateRig(/*skeletonId*/ 1);
        NkIKChainDesc desc;
        desc.name          = "demo_chain";
        desc.solver        = NkIKSolver::NK_FABRIK;
        desc.maxIterations = 16;
        desc.tolerance     = 0.0005f;
        for (uint32 i = 0; i < st->nbones; ++i) {
            NkIKBone b;
            b.boneIdx = i;
            b.length  = (i == 0) ? 0.f : st->segLen;   // longueur du segment i-1→i
            desc.bones.PushBack(b);
        }
        st->chainId = st->rig->AddChain(desc);

        // Pose initiale : chaîne droite le long de +Y, racine à l'origine.
        st->pose.Clear();
        for (uint32 i = 0; i < st->nbones; ++i)
            st->pose.PushBack(NkMat4f::Translate({0.f, (float32)i * st->segLen, 0.f}));

        logger.Info("[DemoIK] M0 IK FABRIK : {0} os, atteinte max {1}m\n",
                    st->nbones, (st->nbones - 1) * st->segLen);
        return true;
    }

    void DemoIK_Frame(DemoCtx& ctx, float32 dt) {
        auto* st = (DemoIKState*)ctx.userData;
        if (!st) return;
        if (!ctx.renderer->BeginFrame()) return;
        auto* r3d = ctx.renderer->GetRender3D();
        if (!r3d) { ctx.renderer->Present(); ctx.renderer->EndFrame(); return; }

        const float32 t     = ctx.totalTime;
        const float32 reach = (float32)(st->nbones - 1) * st->segLen;

        // ── Cible animée (lemniscate dans XY + balayage en Z) dans le rayon d'atteinte
        NkVec3f target = {
            sinf(t * 1.3f) * reach * 0.85f,
            reach * 0.5f + cosf(t * 0.9f) * reach * 0.45f,
            sinf(t * 0.7f) * reach * 0.35f
        };

        // ── Résolution IK : pose monde courante -> solve -> récupère le résultat
        st->rig->SetWorldPose(st->pose.Data(), (uint32)st->pose.Size());
        st->rig->SetTarget(st->chainId, target);
        st->ik.Solve(dt);
        const NkMat4f* res = st->rig->GetBoneMatrices();
        const uint32   cnt = st->rig->GetBoneMatrixCount();
        for (uint32 i = 0; i < cnt && i < st->pose.Size(); ++i) st->pose[i] = res[i];

        // ── Caméra orbitale
        NkCamera3DData camData;
        const float32 orbit = t * 0.25f;
        const float32 camR  = reach * 2.2f;
        camData.position  = { sinf(orbit) * camR, reach * 0.6f, cosf(orbit) * camR };
        camData.target    = { 0.f, reach * 0.45f, 0.f };
        camData.up        = {0, 1, 0};
        camData.fovY      = 50.f;
        camData.aspect    = (float32)ctx.width / (float32)NkMax(1u, ctx.height);
        camData.nearPlane = 0.05f;
        camData.farPlane  = NkMax(100.f, camR * 6.f);
        NkCamera3D cam(camData);

        NkSceneContext sctx;
        sctx.camera = cam;
        sctx.time   = t;
        NkLightDesc sun;
        sun.type      = NkLightType::NK_DIRECTIONAL;
        sun.direction = {-0.4f, -0.8f, -0.5f};
        sun.color     = {1.f, 0.98f, 0.95f};
        sun.intensity = 3.2f;            // scène bien éclairée
        sun.castShadow = true;
        sctx.lights.PushBack(sun);
        // 2e lumière d'appoint (fill) côté opposé pour révéler le volume.
        NkLightDesc fill;
        fill.type      = NkLightType::NK_DIRECTIONAL;
        fill.direction = {0.5f, -0.3f, 0.6f};
        fill.color     = {0.6f, 0.7f, 0.9f};
        fill.intensity = 1.0f;
        fill.castShadow = false;
        sctx.lights.PushBack(fill);
        sctx.ambientIntensity = 0.6f;
        r3d->BeginScene(sctx);

        // ── Rendu via MESHES (le debug-draw NkRender3D est un stub) : joints =
        // sphères, cible = sphère rouge, sol = plane. Chemin PBR standard (tint
        // couleur, pas de material explicite).
        auto P = [&](uint32 i) -> NkVec3f {
            return { st->pose[i].position.x, st->pose[i].position.y, st->pose[i].position.z };
        };
        if (auto* mesh = ctx.renderer->GetMeshSystem()) {
            auto sphere = mesh->GetSphere(20, 20);

            auto submitSphere = [&](NkVec3f p, float32 r, NkVec3f color,
                                    float32 rough, bool shadow) {
                NkDrawCall3D dc;
                dc.mesh       = sphere;
                dc.transform  = NkMat4f::Translate(p) * NkMat4f::Scale({r, r, r});
                dc.aabb       = {{p.x-r, p.y-r, p.z-r}, {p.x+r, p.y+r, p.z+r}};
                dc.tint       = color;
                dc.roughness  = rough;
                dc.castShadow = shadow;
                r3d->Submit(dc);
            };

            // Sol
            {
                float32 s = NkMax(4.f, reach * 1.6f);
                NkDrawCall3D dc;
                dc.mesh       = mesh->GetPlane();
                dc.transform  = NkMat4f::Translate({0.f, -0.02f, 0.f}) * NkMat4f::Scale({s, 1.f, s});
                dc.aabb       = {{-s, -0.05f, -s}, {s, 0.05f, s}};
                dc.tint       = {0.17f, 0.17f, 0.21f};
                dc.roughness  = 0.95f;
                dc.castShadow = false;
                r3d->Submit(dc);
            }
            // Joints (racine verte + grosse, reste cyan)
            for (uint32 i = 0; i < st->pose.Size(); ++i)
                submitSphere(P(i), (i == 0) ? 0.20f : 0.13f,
                             (i == 0) ? NkVec3f{0.2f, 1.f, 0.35f} : NkVec3f{0.25f, 0.75f, 1.f},
                             0.4f, true);
            // Os : petites sphères réparties le long de chaque segment (trace l'os
            // sans avoir à orienter un cylindre — suffit pour visualiser la chaîne).
            for (uint32 i = 0; i + 1 < st->pose.Size(); ++i) {
                NkVec3f a = P(i), b = P(i + 1);
                for (uint32 k = 1; k <= 3; ++k) {
                    float32 u = (float32)k / 4.f;
                    NkVec3f m = {a.x + (b.x-a.x)*u, a.y + (b.y-a.y)*u, a.z + (b.z-a.z)*u};
                    submitSphere(m, 0.06f, {1.f, 0.55f, 0.10f}, 0.5f, true);
                }
            }
            // Cible (rouge)
            submitSphere(target, 0.18f, {1.f, 0.2f, 0.2f}, 0.3f, false);
        }

        ctx.renderer->Present();
        ctx.renderer->EndFrame();
    }

    void DemoIK_Shutdown(DemoCtx& ctx) {
        delete (DemoIKState*)ctx.userData;
        ctx.userData = nullptr;
    }

}} // namespace nkentseu::demo
