// =============================================================================
// DemoIKChar.cpp — NkAnima M0 (d / d-bis) : IK FABRIK sur un VRAI personnage glTF
// -----------------------------------------------------------------------------
// Charge CesiumMan (glTF skinne), extrait les transforms MONDE de chaque joint
// via EvaluateGLTFWorldJoints, sélectionne automatiquement un MEMBRE (la plus
// longue chaîne de joints d'une feuille vers la racine), puis fait suivre une
// cible animée à l'effecteur de ce membre via NkIKSystem (FABRIK).
//
// (d)     — squelette en debug-lines piloté par l'IK.
// (d-bis) — le MESH se DÉFORME réellement : on recompose les matrices de
//           skinning (globalTransform × inverseBind) à partir des transforms IK
//           du membre, en PROPAGEANT le delta de chaque joint résolu à ses
//           descendants (la main suit le poignet, etc.), puis SubmitSkinned.
//
//   renderdemo --demo=15           CesiumMan + skinning IK
//   NK_SKIN_MODEL=<chemin>         autre modèle skinné
//   NK_IKCHAR_NOMESH=1             squelette seul (debug, sans le mesh)
// =============================================================================
#include "DemoCommon.h"
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKRenderer/Mesh/NkGLTFMaterialBridge.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/IK/NkIKSystem.h"
#include "NKLogger/NkLog.h"
#include <cmath>
#include <cstdlib>

namespace nkentseu { namespace demo {

    using namespace nkentseu::renderer;

    struct DemoIKCharState {
        NkGLTFMeshData*    gltf = nullptr;
        bool               ready = false;

        NkIKSystem         ik;              // full-CPU
        NkIKRig*           rig = nullptr;
        NkIKChainId        chainId{};

        NkVector<NkMat4f>  bind;            // pose MONDE bind (par joint)
        NkVector<int32>    parentJoint;     // parent de chaque joint (-1 = racine)
        NkVector<int32>    chain;           // indices joints du membre, racine->effecteur
        NkVector<bool>     inChain;         // joint appartient au membre IK ?
        NkVector<int32>    chainAncestor;   // nearest ancetre dans la chaine (-1) par joint

        // (d-bis) skinning GPU
        bool               meshLoaded = false;
        NkMeshHandle       mesh;
        NkGLTFMaterialSet  matSet;
        NkMatInstHandle    skinMat;
        NkVector<NkMatInstHandle> matSlots;
        NkVector<NkMat4f>  world;           // scratch : global par joint apres IK
        NkVector<NkMat4f>  skin;            // scratch : world * inverseBind

        NkVec3f            center = {0,0,0};
        float32            radius = 2.f;
        float32            reach  = 1.f;     // longueur du membre (rayon d'atteinte)
        NkVec3f            anchor = {0,0,0}; // position bind de la racine du membre
    };

    static NkString PickModel() {
        const char* env = getenv("NK_SKIN_MODEL");
        if (env && env[0]) return NkString(env);
        return NkString("Resources/Models/CesiumMan/CesiumMan.glb");
    }

    static NkVec3f JointPos(const NkMat4f& m) {
        return { m.position.x, m.position.y, m.position.z };
    }

    bool DemoIKChar_Init(DemoCtx& ctx) {
        auto* st = new DemoIKCharState();
        ctx.userData = st;

        NkString path = PickModel();
        st->gltf = new NkGLTFMeshData();
        if (!LoadGLTF(path, *st->gltf)) {
            logger.Errorf("[DemoIKChar] echec chargement glTF : %s\n", path.CStr());
            return true;  // garde la demo vivante
        }
        NkGLTFMeshData& data = *st->gltf;

        // ── Pose MONDE bind (animIdx = -1) : transforms globaux par joint ──────
        if (!EvaluateGLTFWorldJoints(data, -1, 0.f, st->bind, st->parentJoint)
            || st->bind.Empty()) {
            logger.Errorf("[DemoIKChar] modele non skinne ou sans joints : %s\n", path.CStr());
            return true;
        }
        const uint32 jc = (uint32)st->bind.Size();

        // ── Bounds (cadrage camera) depuis les positions de joints ────────────
        NkAABB box = NkAABB::Empty();
        for (uint32 j = 0; j < jc; ++j) box.Expand(JointPos(st->bind[j]));
        st->center = { (box.min.x+box.max.x)*0.5f, (box.min.y+box.max.y)*0.5f,
                       (box.min.z+box.max.z)*0.5f };
        NkVec3f ext = { (box.max.x-box.min.x)*0.5f, (box.max.y-box.min.y)*0.5f,
                        (box.max.z-box.min.z)*0.5f };
        st->radius = NkMax(0.6f, sqrtf(ext.x*ext.x+ext.y*ext.y+ext.z*ext.z) * 2.3f);

        // ── Sélection du membre : la plus longue chaîne feuille -> racine ─────
        NkVector<int32> depth;     depth.Resize(jc);
        NkVector<bool>  hasChild;  hasChild.Resize(jc);
        for (uint32 j = 0; j < jc; ++j) { depth[j] = 0; hasChild[j] = false; }
        for (uint32 j = 0; j < jc; ++j) {
            int32 p = st->parentJoint[j];
            if (p >= 0 && p < (int32)jc) hasChild[p] = true;
            int32 d = 0, cur = st->parentJoint[j], guard = 0;
            while (cur >= 0 && cur < (int32)jc && guard++ < (int32)jc) { ++d; cur = st->parentJoint[cur]; }
            depth[j] = d;
        }
        int32 leaf = -1, bestDepth = -1;
        for (uint32 j = 0; j < jc; ++j)
            if (!hasChild[j] && depth[j] > bestDepth) { bestDepth = depth[j]; leaf = (int32)j; }
        if (leaf < 0) leaf = (int32)jc - 1;

        const uint32 maxLen = 5;
        NkVector<int32> upChain;   // feuille -> racine
        { int32 cur = leaf; uint32 n = 0;
          while (cur >= 0 && n < maxLen) { upChain.PushBack(cur); cur = st->parentJoint[cur]; ++n; } }
        st->chain.Clear();
        for (uint32 i = upChain.Size(); i > 0; --i) st->chain.PushBack(upChain[i-1]);

        st->inChain.Resize(jc);
        for (uint32 j = 0; j < jc; ++j) st->inChain[j] = false;
        for (uint32 i = 0; i < (uint32)st->chain.Size(); ++i) st->inChain[(uint32)st->chain[i]] = true;

        // Nearest ancetre dans la chaine (pour propager le delta IK aux descendants).
        st->chainAncestor.Resize(jc);
        for (uint32 j = 0; j < jc; ++j) {
            int32 a = -1, cur = st->parentJoint[j], guard = 0;
            while (cur >= 0 && cur < (int32)jc && guard++ < (int32)jc) {
                if (st->inChain[(uint32)cur]) { a = cur; break; }
                cur = st->parentJoint[cur];
            }
            st->chainAncestor[j] = a;
        }

        // Longueur du membre + ancre racine.
        st->reach = 0.f;
        for (uint32 i = 0; i + 1 < (uint32)st->chain.Size(); ++i) {
            NkVec3f a = JointPos(st->bind[(uint32)st->chain[i]]);
            NkVec3f b = JointPos(st->bind[(uint32)st->chain[i+1]]);
            NkVec3f d = { b.x-a.x, b.y-a.y, b.z-a.z };
            st->reach += sqrtf(d.x*d.x+d.y*d.y+d.z*d.z);
        }
        if (st->reach < 1e-3f) st->reach = st->radius * 0.3f;
        st->anchor = st->chain.Empty() ? st->center : JointPos(st->bind[(uint32)st->chain[0]]);

        // ── Rig + chaîne IK : boneIdx = indice de joint (pose = TOUS les joints) ─
        st->rig = st->ik.CreateRig(/*skeletonId*/ 1);
        NkIKChainDesc desc;
        desc.name = "limb";
        // Solveur configurable : NK_IK_SOLVER=fabrik|ccd|twobone (defaut fabrik).
        desc.solver = NkIKSolver::NK_FABRIK;
        if (const char* sv = getenv("NK_IK_SOLVER")) {
            if      (sv[0]=='c'||sv[0]=='C') desc.solver = NkIKSolver::NK_CCD;
            else if (sv[0]=='t'||sv[0]=='T') desc.solver = NkIKSolver::NK_TWO_BONE;
        }
        desc.maxIterations = 16; desc.tolerance = 0.0005f;
        for (uint32 i = 0; i < (uint32)st->chain.Size(); ++i) {
            NkIKBone b; b.boneIdx = (uint32)st->chain[i];
            if (i == 0) b.length = 0.f;
            else {
                NkVec3f a = JointPos(st->bind[(uint32)st->chain[i-1]]);
                NkVec3f c = JointPos(st->bind[(uint32)st->chain[i]]);
                NkVec3f d = { c.x-a.x, c.y-a.y, c.z-a.z };
                b.length = sqrtf(d.x*d.x+d.y*d.y+d.z*d.z);
            }
            desc.bones.PushBack(b);
        }
        st->chainId = st->rig->AddChain(desc);
        st->ready = st->chain.Size() >= 2;

        // ── (d-bis) Mesh GPU skinne + materiaux (calque DemoSkin) ─────────────
        if (!getenv("NK_IKCHAR_NOMESH") && data.isSkinned && !data.skinnedVertices.Empty()) {
            auto* meshSys = ctx.renderer->GetMeshSystem();
            if (meshSys) {
                NkMeshDesc d;
                d.layout      = renderer::NkVertexLayout::Skinned();
                d.vertices    = data.skinnedVertices.Data();
                d.vertexCount = (uint32)data.skinnedVertices.Size();
                d.indices     = data.indices.Data();
                d.indexCount  = (uint32)data.indices.Size();
                d.subMeshes   = data.subMeshes;
                d.bounds      = data.bounds;
                d.debugName   = data.debugName;
                st->mesh       = meshSys->Create(d);
                st->meshLoaded = st->mesh.IsValid();

                auto* matSys = ctx.renderer->GetMaterials();
                auto* texLib = ctx.renderer->GetTextures();
                if (st->meshLoaded && matSys && texLib) {
                    BuildGLTFMaterials(data, matSys, texLib, st->matSet);
                    int32 matIdx = (!data.subMeshMaterial.Empty()) ? data.subMeshMaterial[0] : -1;
                    st->skinMat = st->matSet.InstanceForMaterial(matIdx);
                    if (!st->skinMat.IsValid())
                        for (uint32 mi = 0; mi < (uint32)st->matSet.instances.Size(); ++mi)
                            if (st->matSet.instances[mi].IsValid()) { st->skinMat = st->matSet.instances[mi]; break; }
                    const uint32 nSubs = meshSys->GetSubMeshCount(st->mesh);
                    st->matSlots.Resize(nSubs);
                    for (uint32 si = 0; si < nSubs; ++si) {
                        int32 mIdx = (si < (uint32)data.subMeshMaterial.Size()) ? data.subMeshMaterial[si] : -1;
                        NkMatInstHandle h = st->matSet.InstanceForMaterial(mIdx);
                        st->matSlots[si] = h.IsValid() ? h : st->skinMat;
                    }
                }
            }
        }
        st->world.Resize(jc);
        st->skin.Resize(jc);

        logger.Info("[DemoIKChar] '{0}' : {1} joints, membre IK = {2} os (feuille={3}, "
                    "reach={4}m), mesh={5}, center=({6},{7},{8}) r={9}\n",
                    path.CStr(), jc, (uint32)st->chain.Size(), leaf, st->reach,
                    st->meshLoaded?1:0, st->center.x, st->center.y, st->center.z, st->radius);
        return true;
    }

    void DemoIKChar_Frame(DemoCtx& ctx, float32 dt) {
        auto* st = (DemoIKCharState*)ctx.userData;
        if (!st) return;
        if (!ctx.renderer->BeginFrame()) return;
        auto* r3d = ctx.renderer->GetRender3D();
        if (!r3d) { ctx.renderer->Present(); ctx.renderer->EndFrame(); return; }

        const float32 t  = ctx.totalTime;
        const uint32  jc = (uint32)st->bind.Size();

        // ── Cible animée autour de l'ancre du membre ──────────────────────────
        NkVec3f target = {
            st->anchor.x + sinf(t * 1.1f) * st->reach * 0.9f,
            st->anchor.y + sinf(t * 0.7f) * st->reach * 0.6f,
            st->anchor.z + cosf(t * 1.1f) * st->reach * 0.9f
        };

        // ── Résolution IK : on repart TOUJOURS de la bind pose complète ───────
        const NkMat4f* res = st->bind.Data();
        uint32 resCount = jc;
        if (st->ready) {
            st->rig->SetWorldPose(st->bind.Data(), jc);
            st->rig->SetTarget(st->chainId, target);
            st->ik.Solve(dt);
            res      = st->rig->GetBoneMatrices();
            resCount = st->rig->GetBoneMatrixCount();
        }

        // ── Recompose le GLOBAL de chaque joint (FK propagée) ─────────────────
        // Joint dans la chaine -> global IK (res). Joint descendant d'un joint
        // résolu -> on lui applique le DELTA de son ancetre-chaine le plus proche
        // (delta = resGlobal(a) * bindGlobal(a)^-1) -> la main suit le poignet.
        // Joint non affecté -> bind.
        for (uint32 j = 0; j < jc; ++j) {
            if (st->inChain[j] && j < resCount) { st->world[j] = res[j]; continue; }
            int32 a = st->chainAncestor[j];
            if (a >= 0 && (uint32)a < resCount) {
                NkMat4f delta = res[(uint32)a] * st->bind[(uint32)a].Inverse();
                st->world[j] = delta * st->bind[j];
            } else {
                st->world[j] = st->bind[j];
            }
        }

        // ── Caméra orbitale ───────────────────────────────────────────────────
        NkCamera3DData camData;
        const float32 orbit = t * 0.25f;
        camData.position  = { st->center.x + sinf(orbit)*st->radius,
                              st->center.y + st->radius*0.10f,
                              st->center.z + cosf(orbit)*st->radius };
        camData.target    = st->center;
        camData.up        = {0,1,0};
        camData.fovY      = 50.f;
        camData.aspect    = (float32)ctx.width / (float32)NkMax(1u, ctx.height);
        camData.nearPlane = 0.02f;
        camData.farPlane  = NkMax(50.f, st->radius*8.f);
        NkCamera3D cam(camData);

        NkSceneContext sctx;
        sctx.camera = cam; sctx.time = t;
        NkLightDesc sun;
        sun.type = NkLightType::NK_DIRECTIONAL;
        sun.direction = {-0.4f,-0.8f,-0.5f};
        sun.color = {1.f,0.97f,0.92f}; sun.intensity = 2.4f; sun.castShadow = true;
        sctx.lights.PushBack(sun);
        NkLightDesc fill;
        fill.type = NkLightType::NK_DIRECTIONAL;
        fill.direction = {0.5f,-0.3f,0.6f};
        fill.color = {0.6f,0.7f,0.9f}; fill.intensity = 0.8f; fill.castShadow = false;
        sctx.lights.PushBack(fill);
        sctx.ambientIntensity = 0.5f;
        r3d->BeginScene(sctx);

        // Sol.
        if (auto* meshSys = ctx.renderer->GetMeshSystem()) {
            float32 floorY = st->center.y - st->radius * 0.5f;
            float32 s = NkMax(2.f, st->radius);
            NkDrawCall3D dc;
            dc.mesh      = meshSys->GetPlane();
            dc.transform = NkMat4f::Translate({st->center.x, floorY, st->center.z}) *
                           NkMat4f::Scale({s,1.f,s});
            dc.aabb      = {{st->center.x-s, floorY-0.01f, st->center.z-s},
                            {st->center.x+s, floorY+0.01f, st->center.z+s}};
            dc.tint      = {0.16f,0.16f,0.20f};
            dc.roughness = 0.95f; dc.castShadow = false;
            r3d->Submit(dc);
        }

        // ── (d-bis) MESH skinne déformé par l'IK ──────────────────────────────
        bool meshSubmitted = false;
        if (st->meshLoaded) {
            for (uint32 j = 0; j < jc; ++j) {
                NkMat4f ib = (j < (uint32)st->gltf->inverseBind.Size())
                           ? st->gltf->inverseBind[j] : NkMat4f::Identity();
                st->skin[j] = st->world[j] * ib;
            }
            NkDrawCallSkinned dc;
            dc.mesh         = st->mesh;
            dc.transform    = NkMat4f::Identity();
            dc.boneMatrices = st->skin;
            dc.material     = st->skinMat;
            dc.materialSlots= st->matSlots;
            dc.tint         = st->skinMat.IsValid() ? NkVec3f{1,1,1} : NkVec3f{0.85f,0.55f,0.45f};
            dc.aabb         = NkAABB{{st->center.x-st->radius, st->center.y-st->radius, st->center.z-st->radius},
                                     {st->center.x+st->radius, st->center.y+st->radius, st->center.z+st->radius}};
            dc.castShadow   = true;
            r3d->SubmitSkinned(dc);
            meshSubmitted = true;
        }

        // ── Squelette debug-lines (toujours utile : montre la chaine IK) ──────
        // Sans mesh : squelette complet. Avec mesh : seulement la chaine IK + la
        // cible (le reste serait noyé dans le mesh opaque).
        auto WP = [&](uint32 j) -> NkVec3f { return JointPos(st->world[j]); };
        const float32 jr = st->reach * 0.04f;
        if (!meshSubmitted) {
            for (uint32 j = 0; j < jc; ++j) {
                int32 p = st->parentJoint[j];
                bool hot = st->inChain[j] && (p >= 0 && st->inChain[(uint32)p]);
                if (p >= 0)
                    r3d->DrawDebugLine(WP((uint32)p), WP(j),
                        hot ? NkVec4f{1.f,0.55f,0.10f,1.f} : NkVec4f{0.55f,0.58f,0.65f,1.f});
                r3d->DrawDebugSphere(WP(j), jr,
                    st->inChain[j] ? NkVec4f{1.f,0.8f,0.2f,1.f} : NkVec4f{0.4f,0.7f,1.f,1.f});
            }
        } else {
            for (uint32 i = 0; i + 1 < (uint32)st->chain.Size(); ++i)
                r3d->DrawDebugLine(WP((uint32)st->chain[i]), WP((uint32)st->chain[i+1]), {1.f,0.55f,0.10f,1.f});
        }
        if (!st->chain.Empty())
            r3d->DrawDebugSphere(WP((uint32)st->chain[st->chain.Size()-1]), jr*1.8f, {1.f,0.4f,0.1f,1.f});
        r3d->DrawDebugSphere(target, jr*2.0f, {1.f,0.2f,0.2f,1.f});
        r3d->DrawDebugLine(st->anchor, target, {1.f,0.3f,0.3f,0.5f});

        // ── Overlay ───────────────────────────────────────────────────────────
        if (auto* overlay = ctx.renderer->GetOverlay()) {
            overlay->BeginOverlay(ctx.renderer->GetCmd(), ctx.width, ctx.height);
            overlay->DrawText({20.f,35.f}, "DemoIKChar (NkAnima M0 d/d-bis)  |  API : %s", NkGraphicsApiName(ctx.api));
            overlay->DrawText({20.f,55.f}, "IK FABRIK sur CesiumMan — membre = %u os  mesh:%d",
                              (uint32)st->chain.Size(), meshSubmitted?1:0);
            overlay->DrawText({20.f,75.f}, "joints:%u  ready:%d  reach:%.3f",
                              jc, st->ready?1:0, st->reach);
            overlay->DrawText({20.f,95.f}, "FPS : %.0f", dt>1e-5f ? 1.f/dt : 0.f);
            overlay->EndOverlay();
        }

        ctx.renderer->Present();
        ctx.renderer->EndFrame();
    }

    void DemoIKChar_Shutdown(DemoCtx& ctx) {
        auto* st = (DemoIKCharState*)ctx.userData;
        if (st) { delete st->gltf; delete st; }
        ctx.userData = nullptr;
    }

}} // namespace nkentseu::demo
