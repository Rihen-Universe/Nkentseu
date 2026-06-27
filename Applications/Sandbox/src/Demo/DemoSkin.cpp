// =============================================================================
// DemoSkin.cpp  — Demo skinning GPU : charge un glTF skinne et l'anime.
//
// Charge un modele glTF 2.0 SKINNE (JOINTS_0/WEIGHTS_0 + skins[] + animations[])
// via NkGLTFLoader, cree un mesh GPU au vertex layout NkVertexSkinned, evalue la
// pose squelettique par frame (EvaluateGLTFPose), et soumet via SubmitSkinned.
// Le vertex shader skin (Resources/.../Shaders/Skin/VK/skin.vert.vk.glsl) fait le
// linear blend skinning sur GPU -> le mesh se DEFORME a l'ecran.
//
// Modele par defaut : Resources/Models/SimpleSkin/SimpleSkin.gltf
//   (exemple canonique Khronos : strip vertical 2 joints qui se plie).
// Override : NK_SKIN_MODEL=<chemin>.
//
// Preuve visuelle : la pose change dans le temps -> les pixels different entre
// 2 instants. F12 (capture) si dispo, sinon observation directe.
// =============================================================================
#include "DemoCommon.h"
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKRenderer/Mesh/NkGLTFMaterialBridge.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKLogger/NkLog.h"
#include <cmath>
#include <cstdlib>

namespace nkentseu { namespace demo {

    using namespace nkentseu::renderer;

    struct DemoSkinState {
        NkMeshHandle      mesh;
        NkAABB            bounds;
        NkString          modelName;
        bool              loaded   = false;
        bool              skinned  = false;
        uint32            vertexCount = 0;
        uint32            indexCount  = 0;
        uint32            jointCount  = 0;
        uint32            animCount   = 0;
        float32           animTime    = 0.f;
        float32           radius      = 4.f;
        NkVec3f           center      = {0,0,0};
        // Donnees CPU du loader conservees pour evaluer la pose chaque frame.
        // NkGLTFMeshData n'est pas copiable -> alloue sur le tas.
        NkGLTFMeshData*   gltf = nullptr;
        NkVector<NkMat4f> bones;
        // Materiaux/textures glTF reels (via le pont) pour le mesh skinne.
        NkGLTFMaterialSet matSet;
        NkMatInstHandle   skinMat;   // instance du 1er submesh (mono-draw skin, fallback)
        // Multi-material : un NkMatInstHandle par sous-mesh (BrainStem 59 prims).
        NkVector<NkMatInstHandle> matSlots;
    };

    static NkString PickSkinModel() {
        const char* env = getenv("NK_SKIN_MODEL");
        if (env && env[0]) return NkString(env);
        return NkString("Resources/Models/SimpleSkin/SimpleSkin.gltf");
    }

    bool DemoSkin_Init(DemoCtx& ctx) {
        auto* st = new DemoSkinState();
        ctx.userData = st;

        auto* meshSys = ctx.renderer->GetMeshSystem();
        if (!meshSys) {
            logger.Errorf("[DemoSkin] MeshSystem manquant\n");
            delete st; ctx.userData = nullptr; return false;
        }

        NkString path = PickSkinModel();
        st->modelName = path;

        st->gltf = new NkGLTFMeshData();
        if (!LoadGLTF(path, *st->gltf)) {
            logger.Errorf("[DemoSkin] echec chargement glTF : %s\n", path.CStr());
            return true;  // garde la demo vivante (overlay d'erreur)
        }

        NkGLTFMeshData& data = *st->gltf;
        st->skinned     = data.isSkinned && !data.skinnedVertices.Empty();
        st->vertexCount = (uint32)data.vertices.Size();
        st->indexCount  = (uint32)data.indices.Size();
        st->jointCount  = (uint32)data.skinJoints.Size();
        st->animCount   = (uint32)data.animations.Size();
        st->bounds      = data.bounds;

        // ── Cree le mesh GPU au layout skinne (si skinne) ────────────────────
        NkMeshDesc d;
        if (st->skinned) {
            d.layout      = renderer::NkVertexLayout::Skinned();
            d.vertices    = data.skinnedVertices.Data();
            d.vertexCount = (uint32)data.skinnedVertices.Size();
        } else {
            d.layout      = renderer::NkVertexLayout::Default3D();
            d.vertices    = data.vertices.Data();
            d.vertexCount = (uint32)data.vertices.Size();
        }
        d.indices     = data.indices.Data();
        d.indexCount  = (uint32)data.indices.Size();
        d.subMeshes   = data.subMeshes;
        d.bounds      = data.bounds;
        d.debugName   = data.debugName;
        st->mesh      = meshSys->Create(d);
        st->loaded    = st->mesh.IsValid() && st->vertexCount > 0;

        // ── Materiaux/textures glTF reels (pont mesh->renderer) ───────────────
        // Sans ca, le mesh skinne tombe sur le materiau FALLBACK (textures
        // blanches) -> CesiumMan vert / Fox noir. On construit les
        // NkMaterialInstance + uploade les images decodees, puis on retient
        // l'instance du 1er submesh (le skinning est mono-draw ; le
        // multi-submesh multi-materiau par sous-maillage = amelioration future).
        {
            auto* matSys = ctx.renderer->GetMaterials();
            auto* texLib = ctx.renderer->GetTextures();
            if (matSys && texLib) {
                BuildGLTFMaterials(data, matSys, texLib, st->matSet);
                int32 matIdx = (!data.subMeshMaterial.Empty()) ? data.subMeshMaterial[0] : -1;
                st->skinMat = st->matSet.InstanceForMaterial(matIdx);
                // Fallback : si le 1er submesh n'a pas de materiau valide, prend
                // la 1ere instance valide du set.
                if (!st->skinMat.IsValid()) {
                    for (uint32 mi = 0; mi < (uint32)st->matSet.instances.Size(); ++mi)
                        if (st->matSet.instances[mi].IsValid()) { st->skinMat = st->matSet.instances[mi]; break; }
                }
                logger.Info("[DemoSkin] materiaux glTF : {0} instances, {1} textures, 1er submesh matIdx={2} valid={3}\n",
                            (uint32)st->matSet.instances.Size(), (uint32)st->matSet.textures.Size(),
                            matIdx, st->skinMat.IsValid()?1:0);

                // ── Multi-material : une instance materiau PAR sous-mesh ──────
                // Chaque submesh (= primitive glTF) a son index materiau dans
                // data.subMeshMaterial. On resout l'instance via le set ; pour
                // BrainStem (59 submeshes, 59 couleurs unies) chaque sous-mesh
                // recoit ainsi SA couleur. Mono-submesh (CesiumMan/Fox/Simple) :
                // 1 seul slot == skinMat -> comportement identique a avant.
                const uint32 nSubs = meshSys->GetSubMeshCount(st->mesh);
                st->matSlots.Clear();
                st->matSlots.Resize(nSubs);
                uint32 nValid = 0;
                for (uint32 si = 0; si < nSubs; ++si) {
                    int32 mIdx = (si < (uint32)data.subMeshMaterial.Size())
                                 ? data.subMeshMaterial[si] : -1;
                    NkMatInstHandle h = st->matSet.InstanceForMaterial(mIdx);
                    if (!h.IsValid()) h = st->skinMat;  // fallback global
                    st->matSlots[si] = h;
                    if (h.IsValid()) ++nValid;
                }
                logger.Info("[DemoSkin] multi-material : {0} sous-meshes, {1} slots valides\n",
                            nSubs, nValid);
            }
        }

        // Pose initiale (bind pose) pour avoir un nombre de bones >0 dès le départ.
        if (st->skinned)
            EvaluateGLTFPose(data, st->animCount > 0 ? 0 : -1, 0.f, st->bones);

        // ── Bounds depuis les vertices REELLEMENT skinnes (pas le bind-pose brut) ──
        // Les positions brutes (skinnedVertices.pos) sont en espace mesh-local (ex.
        // Z-up pour CesiumMan), alors que le mesh est RENDU via les matrices de
        // joints (world Y-up). Cadrer la camera/sol sur les bounds brutes visait au
        // mauvais endroit -> modele hors-champ/sous le sol. On recalcule l'AABB sur
        // les positions skinnees a t=0 (= ce qui est reellement affiche).
        if (st->skinned && !st->bones.Empty() && !data.skinnedVertices.Empty()) {
            NkAABB sk = NkAABB::Empty();
            for (uint32 vi = 0; vi < (uint32)data.skinnedVertices.Size(); ++vi) {
                const NkVertexSkinned& sv = data.skinnedVertices[vi];
                NkMat4f m; for (int e=0;e<16;e++) m.data[e]=0.f;
                float32 wsum=0.f;
                for (int b=0;b<4;b++) {
                    int j=(int)(sv.boneIdx[b]+0.5f); float32 w=sv.boneWeight[b];
                    if (j>=0 && j<(int)st->bones.Size() && w>0.f) {
                        for (int e=0;e<16;e++) m.data[e]+=w*st->bones[(uint32)j].data[e];
                        wsum+=w;
                    }
                }
                if (wsum<1e-4f) m = NkMat4f::Identity();
                NkVec4f wp = m * NkVec4f{sv.pos.x, sv.pos.y, sv.pos.z, 1.f};
                sk.Expand(NkVec3f{wp.x, wp.y, wp.z});
            }
            st->bounds = sk;
        }

        // ── Cadre la camera ──────────────────────────────────────────────────
        NkVec3f mn = st->bounds.min, mx = st->bounds.max;
        st->center = { (mn.x+mx.x)*0.5f, (mn.y+mx.y)*0.5f, (mn.z+mx.z)*0.5f };
        NkVec3f ext = { (mx.x-mn.x)*0.5f, (mx.y-mn.y)*0.5f, (mx.z-mn.z)*0.5f };
        float32 diag = std::sqrt(ext.x*ext.x + ext.y*ext.y + ext.z*ext.z);
        st->radius = (diag > 1e-4f) ? diag * 2.2f : 4.f;
        if (st->radius < 1.5f) st->radius = 1.5f;

        // ── PREUVE CPU de deformation ────────────────────────────────────────
        // Calcule la position SKINNEE d'un vertex du haut (totalement pondere au
        // joint anime) a plusieurs instants. Le GPU applique exactement la meme
        // mat skinMat*pos ; si ces positions DIFFERENT dans le temps, alors le
        // mesh se deforme bien a l'ecran. (Verifiable sans capture pixel.)
        if (st->skinned && st->animCount > 0 && !data.skinnedVertices.Empty()) {
            uint32 topIdx = (uint32)data.skinnedVertices.Size() - 1; // dernier = haut
            const NkVertexSkinned& sv = data.skinnedVertices[topIdx];
            const float32 ts[3] = { 0.0f, 0.75f, 1.5f };
            for (int s = 0; s < 3; ++s) {
                NkVector<NkMat4f> pose;
                EvaluateGLTFPose(data, 0, ts[s], pose);
                // skinMat = somme ponderee des joint matrices.
                NkMat4f sk; for (int e=0;e<16;e++) sk.data[e]=0.f;
                for (int b=0;b<4;b++) {
                    int j = (int)(sv.boneIdx[b]+0.5f);
                    float32 w = sv.boneWeight[b];
                    if (j>=0 && j<(int)pose.Size() && w>0.f) {
                        NkMat4f m = pose[(uint32)j];
                        for (int e=0;e<16;e++) sk.data[e] += w*m.data[e];
                    }
                }
                NkVec4f p0{sv.pos.x, sv.pos.y, sv.pos.z, 1.f};
                NkVec4f wp = sk * p0;
                logger.Info("[DemoSkin][PREUVE] top vert t={0}s -> skinned pos = ({1}, {2}, {3})\n",
                            ts[s], wp.x, wp.y, wp.z);
            }
        }

        logger.Info("[DemoSkin] Init OK — '{0}' : skinned={1} {2}v/{3}i joints={4} anims={5} "
                    "skinnedVerts={6} center=({7},{8},{9}) r={10}\n",
                    path.CStr(), st->skinned?1:0, st->vertexCount, st->indexCount,
                    st->jointCount, st->animCount,
                    (uint32)data.skinnedVertices.Size(),
                    st->center.x, st->center.y, st->center.z, st->radius);
        return true;
    }

    void DemoSkin_Frame(DemoCtx& ctx, float32 dt) {
        auto* st = (DemoSkinState*)ctx.userData;

        // NK_FREEZE[=secondes] : fige l'animation a une frame deterministe
        // (defaut 2.0s). Diagnostic A1.5 : tester plusieurs valeurs -> si l'effet
        // est present a TOUTES les poses figees, il est independant de la pose
        // (donc du chemin skinning, pas de l'animation/mouvement).
        if (const char* fz = getenv("NK_FREEZE")) {
            float32 t = (float32)atof(fz);
            st->animTime = (t > 0.f) ? t : 2.0f;
        } else {
            st->animTime += dt;
        }

        // Camera FIXE par defaut (pour bien voir le pli du skinning). Orbit
        // optionnelle via NK_ORBIT=1 (comparaison backends / inspection).
        static int orbit = -1;
        if (orbit == -1) { const char* v = getenv("NK_ORBIT"); orbit = (v && v[0] && v[0]!='0') ? 1 : 0; }
        float32 angle = orbit ? st->animTime * 0.4f : 0.6f;

        if (!ctx.renderer->BeginFrame()) return;
        auto* r3d = ctx.renderer->GetRender3D();
        if (!r3d) { ctx.renderer->Present(); ctx.renderer->EndFrame(); return; }

        // ── Camera orbitale ──────────────────────────────────────────────────
        NkCamera3DData camData;
        camData.position  = { st->center.x + sinf(angle)*st->radius,
                              st->center.y + st->radius*0.15f,
                              st->center.z + cosf(angle)*st->radius };
        camData.target    = st->center;
        camData.up        = {0,1,0};
        camData.fovY      = 50.f;
        camData.aspect    = (float32)ctx.width / (float32)ctx.height;
        camData.nearPlane = 0.05f;
        camData.farPlane  = NkMax(100.f, st->radius*8.f);
        NkCamera3D cam(camData);

        NkSceneContext sctx;
        sctx.camera = cam;
        sctx.time   = ctx.totalTime;

        NkLightDesc sun;
        sun.type       = NkLightType::NK_DIRECTIONAL;
        sun.direction  = {-0.4f,-0.8f,-0.5f};
        sun.color      = {1.f,0.97f,0.92f};
        sun.intensity  = 1.5f;  // modere : evite que les textures saturees bloom
        sun.castShadow = true;
        sctx.lights.PushBack(sun);
        sctx.ambientIntensity = 0.35f;

        r3d->BeginScene(sctx);

        // ── Sol ──────────────────────────────────────────────────────────────
        if (auto* meshSys = ctx.renderer->GetMeshSystem()) {
            // Sol legerement sous le pied le plus bas (evite la coplanarite a
            // bounds.min.y exact). N.B. ne corrige PAS l'irisation A1.5 (testee).
            float32 floorY = st->bounds.min.y - (st->bounds.max.y - st->bounds.min.y) * 0.005f;
            float32 s = NkMax(4.f, st->radius*2.f);
            NkDrawCall3D dc;
            dc.mesh      = meshSys->GetPlane();
            dc.transform = NkMat4f::Translate({st->center.x, floorY, st->center.z}) *
                           NkMat4f::Scale({s,1.f,s});
            dc.aabb      = {{st->center.x-s, floorY-0.01f, st->center.z-s},
                            {st->center.x+s, floorY+0.01f, st->center.z+s}};
            dc.tint      = {0.3f,0.3f,0.35f};
            dc.roughness = 0.9f;
            dc.castShadow= false;
            r3d->Submit(dc);
        }

        // ── Modele skinne : evalue la pose et soumet ─────────────────────────
        // NK_NOMODEL : ne soumet PAS le modele (sol seul) -> diagnostic A1.5
        // (si l'irisation du sol disparait sans modele -> bleed/influence du modele).
        bool submitted = false;
        if (getenv("NK_NOMODEL")) { /* sol seul */ }
        else if (st->loaded && st->skinned && st->gltf) {
            // Echantillonne l'animation 0 (ou bind pose si aucune) au temps courant.
            // NK_BINDPOSE : force la BIND POSE (animIdx=-1) -> AUCUNE animation, juste
            // le skinning au repos. Diagnostic A1.5 : si l'irisation du sol apparait
            // meme en bind pose -> c'est le CHEMIN SKINNING (FlushSkinned), pas l'anim.
            int32 animIdx = getenv("NK_BINDPOSE") ? -1 : ((st->animCount > 0) ? 0 : -1);
            EvaluateGLTFPose(*st->gltf, animIdx, st->animTime, st->bones);

            if (!st->bones.Empty()) {
                NkDrawCallSkinned dc;
                dc.mesh         = st->mesh;
                dc.transform    = NkMat4f::Identity();
                dc.boneMatrices = st->bones;     // copie (NkVector)
                dc.material     = st->skinMat;   // materiau/texture glTF reel (set=2, fallback)
                dc.materialSlots= st->matSlots;  // un materiau par sous-mesh (multi-mat)
                dc.tint         = st->skinMat.IsValid() ? NkVec3f{1.f,1.f,1.f}
                                                        : NkVec3f{0.85f,0.55f,0.45f};
                dc.alpha        = 1.f;
                dc.aabb         = st->bounds;
                dc.castShadow   = true;
                r3d->SubmitSkinned(dc);
                submitted = true;
            }
        } else if (st->loaded) {
            // Fallback non-skinne : affiche le mesh statique (preuve de chargement).
            NkDrawCall3D dc;
            dc.mesh      = st->mesh;
            dc.transform = NkMat4f::Identity();
            dc.aabb      = st->bounds;
            dc.tint      = {0.8f,0.8f,0.8f};
            r3d->Submit(dc);
        }

        r3d->DrawDebugAxes(NkMat4f::Translate(st->center), st->radius*0.3f);

        // ── Overlay ──────────────────────────────────────────────────────────
        if (auto* overlay = ctx.renderer->GetOverlay()) {
            overlay->BeginOverlay(ctx.renderer->GetCmd(), ctx.width, ctx.height);
            overlay->DrawStats(ctx.renderer->GetStats());
            overlay->DrawText({20.f,35.f}, "DemoSkin  |  API : %s", NkGraphicsApiName(ctx.api));
            overlay->DrawText({20.f,55.f}, "Model : %s", st->modelName.CStr());
            if (st->loaded) {
                overlay->DrawText({20.f,75.f},
                    "skinned:%d verts:%u idx:%u joints:%u anims:%u bones:%u",
                    st->skinned?1:0, st->vertexCount, st->indexCount,
                    st->jointCount, st->animCount, (uint32)st->bones.Size());
                overlay->DrawText({20.f,95.f}, "animTime: %.2fs  submitted:%d",
                                  st->animTime, submitted?1:0);
            } else {
                overlay->DrawText({20.f,75.f}, "** CHARGEMENT ECHOUE **");
            }
            overlay->DrawText({20.f,115.f}, "FPS : %.0f", dt>1e-5f ? 1.f/dt : 0.f);
            overlay->EndOverlay();
        }

        ctx.renderer->Present();
        ctx.renderer->EndFrame();
    }

    void DemoSkin_Shutdown(DemoCtx& ctx) {
        auto* st = (DemoSkinState*)ctx.userData;
        if (st) {
            delete st->gltf;
            delete st;
        }
        ctx.userData = nullptr;
        logger.Info("[DemoSkin] Shutdown\n");
    }

}} // namespace nkentseu::demo
