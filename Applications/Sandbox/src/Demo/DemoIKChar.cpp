// =============================================================================
// DemoIKChar.cpp — NkAnima M0 (d / d-bis) : IK FABRIK sur un VRAI personnage glTF
// -----------------------------------------------------------------------------
// Charge CesiumMan (glTF skinne), extrait les transforms MONDE de chaque joint
// via EvaluateGLTFWorldJoints, sélectionne automatiquement un MEMBRE (la plus
// longue chaîne de joints d'une feuille vers la racine), puis fait suivre une
// cible animée à l'effecteur de ce membre via NkIKSystem (FABRIK).
//
// (d)     — squelette en debug-lines piloté par l'IK.
// (d-bis) — le MESH se DÉFORME réellement, SANS déchirure : aim-FK hiérarchique
//           (ComputeIKWorld) -> chaque joint de chaîne pivote pour placer son
//           enfant sur la position FABRIK, FK cohérente pour tout le squelette,
//           skin[j] = world[j] * inverseBind[j], puis SubmitSkinned.
//           (Le bug de déchirure venait de NkMat4::Inverse() — corrigé dans NKMath.)
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
#include "NKWindow/Core/NkWESystem.h"   // NkEvents() (file d'événements globale)
#include "NKEvent/NkMouseEvent.h"
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
        NkVector<NkMat4f>  localBind;        // transform LOCAL bind par joint (rel. parent)
        NkVector<uint32>   topo;             // ordre topologique (parent avant enfant)

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

        // (a') effecteur draggable a la souris
        float32            camAngle = 0.f;   // orbite (gelee pendant le drag)
        bool               dragging  = false;
        bool               hasManual = false;// une fois drague, la cible reste posee
        int32              mouseX = 0, mouseY = 0;
        NkVec3f            manualTarget = {0,0,0};
    };

    static NkString PickModel() {
        const char* env = getenv("NK_SKIN_MODEL");
        if (env && env[0]) return NkString(env);
        return NkString("Resources/Models/CesiumMan/CesiumMan.glb");
    }

    static NkVec3f JointPos(const NkMat4f& m) {
        return { m.position.x, m.position.y, m.position.z };
    }

    // Transforms MONDE de tous les joints sous IK (AIM-FK hiérarchique, tear-free).
    // (1) Joints de chaîne racine->tip : chaque joint pivote pour amener son enfant
    // sur la position résolue par FABRIK (res[c].position), puis enfant = parent *
    // localBind -> FK cohérente atteignant les positions FABRIK sans double-rotation.
    // (2) FK topo pour le reste. Réutilisé par le rendu ET le diagnostic.
    static void ComputeIKWorld(DemoIKCharState* st, const NkMat4f* res, uint32 resCount,
                               NkVector<NkMat4f>& out) {
        const uint32 jc = (uint32)st->bind.Size();
        if ((uint32)out.Size() != jc) out.Resize(jc);
        auto rotP = [](const NkMat4f& m){ NkMat4f r=m; r.m30=0;r.m31=0;r.m32=0;r.m33=1; return r; };
        const uint32 cn = (uint32)st->chain.Size();
        if (cn >= 1) out[(uint32)st->chain[0]] = st->bind[(uint32)st->chain[0]];
        for (uint32 i = 0; i + 1 < cn; ++i) {
            uint32 j = (uint32)st->chain[i], c = (uint32)st->chain[i+1];
            NkMat4f childLocalBind = st->bind[j].Inverse() * st->bind[c];
            NkMat4f Gj = out[j];
            NkVec3f jp = { Gj.position.x, Gj.position.y, Gj.position.z };
            NkMat4f curChild = Gj * childLocalBind;
            NkVec3f cp = { curChild.position.x, curChild.position.y, curChild.position.z };
            NkVec3f np = (c < resCount)
                ? NkVec3f{ res[c].position.x, res[c].position.y, res[c].position.z }
                : NkVec3f{ st->bind[c].position.x, st->bind[c].position.y, st->bind[c].position.z };
            NkVec3f a = { cp.x-jp.x, cp.y-jp.y, cp.z-jp.z };
            NkVec3f b = { np.x-jp.x, np.y-jp.y, np.z-jp.z };
            float32 al = sqrtf(a.x*a.x+a.y*a.y+a.z*a.z), bl = sqrtf(b.x*b.x+b.y*b.y+b.z*b.z);
            if (al > 1e-6f && bl > 1e-6f) {
                a.x/=al;a.y/=al;a.z/=al; b.x/=bl;b.y/=bl;b.z/=bl;
                NkQuatf Rw(a, b);
                out[j] = NkMat4f::Translate(jp) * (Rw.ToMat4() * rotP(Gj));
            }
            out[c] = out[j] * childLocalBind;
        }
        for (uint32 oi = 0; oi < (uint32)st->topo.Size(); ++oi) {
            uint32 j = st->topo[oi];
            if (st->inChain[j]) continue;
            int32 p = st->parentJoint[j];
            out[j] = (p >= 0) ? (out[(uint32)p] * st->localBind[j]) : st->localBind[j];
        }
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

        const uint32 maxLen = 3;   // membre DISTAL (ex. épaule/coude/main) : éviter de
                                   // remonter jusqu'aux joints proches de la racine
                                   // (colonne/bassin) qui feraient pivoter tout le corps.
        NkVector<int32> upChain;   // feuille -> racine
        { int32 cur = leaf; uint32 n = 0;
          while (cur >= 0 && n < maxLen) { upChain.PushBack(cur); cur = st->parentJoint[cur]; ++n; } }
        st->chain.Clear();
        for (uint32 i = upChain.Size(); i > 0; --i) st->chain.PushBack(upChain[i-1]);

        st->inChain.Resize(jc);
        for (uint32 j = 0; j < jc; ++j) st->inChain[j] = false;
        for (uint32 i = 0; i < (uint32)st->chain.Size(); ++i) st->inChain[(uint32)st->chain[i]] = true;

        // Pré-calcul FK : transform LOCAL bind par joint + ordre topologique.
        // localBind[j] = inverse(bind[parent]) * bind[j] (telescope -> reproduit bind).
        st->localBind.Resize(jc);
        for (uint32 j = 0; j < jc; ++j) {
            int32 p = st->parentJoint[j];
            st->localBind[j] = (p >= 0) ? (st->bind[(uint32)p].Inverse() * st->bind[j]) : st->bind[j];
        }
        st->topo.Clear();
        { NkVector<bool> placed; placed.Resize(jc);
          for (uint32 j = 0; j < jc; ++j) placed[j] = false;
          uint32 done = 0, guard = 0;
          while (done < jc && guard++ < jc + 2) {
              for (uint32 j = 0; j < jc; ++j) {
                  if (placed[j]) continue;
                  int32 p = st->parentJoint[j];
                  if (p < 0 || placed[(uint32)p]) { st->topo.PushBack(j); placed[j] = true; ++done; }
              }
          }
          for (uint32 j = 0; j < jc; ++j) if (!placed[j]) st->topo.PushBack(j);
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

        // (a') Input souris : clic gauche maintenu = déplacer l'effecteur (la cible
        // suit le curseur, projeté sur le plan passant par l'ancre face caméra).
        // La logique reste DANS la démo — le solveur IK (engine) ignore tout ça.
        {
            auto* sp = st;
            NkEvents().AddEventCallback<NkMouseMoveEvent>([sp](NkMouseMoveEvent* e){
                if (e) { sp->mouseX = e->GetX(); sp->mouseY = e->GetY(); }
            });
            NkEvents().AddEventCallback<NkMouseButtonPressEvent>([sp](NkMouseButtonPressEvent* e){
                if (e && e->GetButton() == NkMouseButton::NK_MB_LEFT) { sp->dragging = true; sp->hasManual = true; }
            });
            NkEvents().AddEventCallback<NkMouseButtonReleaseEvent>([sp](NkMouseButtonReleaseEvent* e){
                if (e && e->GetButton() == NkMouseButton::NK_MB_LEFT) sp->dragging = false;
            });
        }

        // ── DIAGNOSTIC déchirure (NK_IK_TEAR_DIAG) ────────────────────────────
        // Mesure l'étirement MAX des arêtes de triangles (déformé / repos) pour
        // 3 poses : BIND (référence), ANIM glTF native (= chemin DemoSkin validé),
        // et mon RE-SKIN IK. Isole la cause : si ANIM déchire aussi -> skinning/
        // données ; si seul IK déchire -> M0 (d-bis). One-shot, CPU.
        if (getenv("NK_IK_TEAR_DIAG") && st->meshLoaded && !data.skinnedVertices.Empty()) {
            NkGLTFMeshData& D = data;
            auto skinPos = [&](const NkVector<NkMat4f>& sm, uint32 vi) -> NkVec3f {
                const NkVertexSkinned& sv = D.skinnedVertices[vi];
                NkMat4f m; for (int e=0;e<16;e++) m.data[e]=0.f; float32 wsum=0.f;
                for (int b=0;b<4;b++) {
                    int j=(int)(sv.boneIdx[b]+0.5f); float32 w=sv.boneWeight[b];
                    if (j>=0 && j<(int)sm.Size() && w>0.f) { for (int e=0;e<16;e++) m.data[e]+=w*sm[(uint32)j].data[e]; wsum+=w; }
                }
                if (wsum<1e-4f) m=NkMat4f::Identity();
                NkVec4f p = m * NkVec4f{sv.pos.x,sv.pos.y,sv.pos.z,1.f};
                return {p.x,p.y,p.z};
            };
            auto metric = [&](const NkVector<NkMat4f>& sm, const char* tag) {
                float32 maxR=0.f; uint32 bad=0, ne=0;
                for (uint32 i=0; i+2<(uint32)D.indices.Size(); i+=3) {
                    uint32 idx[3]={D.indices[i],D.indices[i+1],D.indices[i+2]};
                    for (int e=0;e<3;e++) {
                        uint32 a=idx[e], b=idx[(e+1)%3];
                        NkVec3f pa=skinPos(sm,a), pb=skinPos(sm,b);
                        const auto& ra=D.skinnedVertices[a].pos; const auto& rb=D.skinnedVertices[b].pos;
                        float32 dl=sqrtf((pa.x-pb.x)*(pa.x-pb.x)+(pa.y-pb.y)*(pa.y-pb.y)+(pa.z-pb.z)*(pa.z-pb.z));
                        float32 bl=sqrtf((ra.x-rb.x)*(ra.x-rb.x)+(ra.y-rb.y)*(ra.y-rb.y)+(ra.z-rb.z)*(ra.z-rb.z));
                        if (bl>1e-6f) { float32 r=dl/bl; if (r>maxR)maxR=r; if (r>3.f)bad++; ne++; }
                    }
                }
                logger.Info("[DemoIKChar][TEAR] {0} : maxEdgeStretch={1}x  edges>3x={2}/{3}\n", tag, maxR, bad, ne);
            };
            // (A) BIND
            { NkVector<NkMat4f> bb; EvaluateGLTFPose(D, -1, 0.f, bb); metric(bb, "BIND   "); }
            // (B) ANIM glTF native (animIdx 0) a t=1.0s
            { NkVector<NkMat4f> ab; EvaluateGLTFPose(D, 0, 1.0f, ab); metric(ab, "ANIM   "); }
            // (B2) FK de contrôle : ComputeIKWorld avec res = BIND (aucun mouvement
            // IK). DOIT donner ~1.0x ; sinon la FK elle-même est buggée.
            {
                NkVector<NkMat4f> wb; ComputeIKWorld(st, st->bind.Data(), jc, wb);
                // Erreur de reconstruction : |wb[j].position - bind[j].position| max.
                float32 maxErr=0.f; uint32 worst=0;
                for (uint32 j=0;j<jc;++j) {
                    NkVec3f a=JointPos(wb[j]), b=JointPos(st->bind[j]);
                    float32 e=sqrtf((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));
                    if (e>maxErr){maxErr=e;worst=j;}
                }
                // Test Inverse() : max|bind[j]*bind[j]^-1 - I|.
                float32 invErr=0.f;
                for (uint32 j=0;j<jc;++j) {
                    NkMat4f p = st->bind[j] * st->bind[j].Inverse();
                    for (int r=0;r<4;r++) for(int c=0;c<4;c++){ float32 id=(r==c)?1.f:0.f; float32 e=fabsf(p.mat[c][r]-id); if(e>invErr)invErr=e; }
                }
                logger.Info("[DemoIKChar][TEAR] FK-recon : topo.Size={0}/{1}  maxPosErr={2} (joint {3}, parent={4})  invErr={5}\n",
                            (uint32)st->topo.Size(), jc, maxErr, worst, st->parentJoint[worst], invErr);
                NkVector<NkMat4f> ib; ib.Resize(jc);
                for (uint32 j=0;j<jc;++j) { NkMat4f m=(j<(uint32)D.inverseBind.Size())?D.inverseBind[j]:NkMat4f::Identity(); ib[j]=wb[j]*m; }
                metric(ib, "FK-BIND ");
            }
            // (C) RE-SKIN IK : solve vers une cible qui plie le membre, puis skin.
            if (st->ready) {
                NkVec3f tgt = { st->anchor.x + st->reach*0.55f, st->anchor.y - st->reach*0.35f, st->anchor.z + st->reach*0.4f };
                st->rig->SetWorldPose(st->bind.Data(), jc);
                st->rig->SetTarget(st->chainId, tgt);
                st->ik.Solve(0.016f);
                const NkMat4f* res = st->rig->GetBoneMatrices();
                uint32 rc = st->rig->GetBoneMatrixCount();
                NkVector<NkMat4f> wq; ComputeIKWorld(st, res, rc, wq);
                NkVector<NkMat4f> ik; ik.Resize(jc);
                for (uint32 j=0;j<jc;++j) {
                    NkMat4f ibm = (j<(uint32)D.inverseBind.Size())?D.inverseBind[j]:NkMat4f::Identity();
                    ik[j] = wq[j] * ibm;
                }
                metric(ik, "RESKIN-IK");
            }
        }

        if (getenv("NK_IK_TEAR_DIAG"))
            for (uint32 i = 0; i < (uint32)st->chain.Size(); ++i)
                logger.Info("[DemoIKChar][CHAIN] os {0} : joint={1} parent={2}\n",
                            i, (uint32)st->chain[i], st->parentJoint[(uint32)st->chain[i]]);

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

        // ── Caméra orbitale (orbite GELÉE pendant le drag) ────────────────────
        if (!st->dragging) st->camAngle += dt * 0.25f;
        const float32 orbit = st->camAngle;
        NkCamera3DData camData;
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

        // ── Cible : drag souris (a') sinon animée ─────────────────────────────
        NkVec3f animTarget = {
            st->anchor.x + sinf(t * 1.1f) * st->reach * 0.9f,
            st->anchor.y + sinf(t * 0.7f) * st->reach * 0.6f,
            st->anchor.z + cosf(t * 1.1f) * st->reach * 0.9f
        };
        if (st->dragging) {
            // Rayon caméra->curseur (unprojection via inv(viewProj)), intersecté
            // avec le plan passant par l'ancre (normale = visée caméra) -> point 3D.
            NkMat4f invVP = cam.GetViewProj().Inverse();
            float32 nx = 2.f*((float32)st->mouseX/(float32)NkMax(1u,ctx.width)) - 1.f;
            float32 ny = 1.f - 2.f*((float32)st->mouseY/(float32)NkMax(1u,ctx.height));
            NkVec4f fp = invVP * NkVec4f{nx, ny, 1.f, 1.f};
            if (fabsf(fp.w) > 1e-8f) {
                NkVec3f O = camData.position;
                NkVec3f D = { fp.x/fp.w - O.x, fp.y/fp.w - O.y, fp.z/fp.w - O.z };
                float32 dl = sqrtf(D.x*D.x+D.y*D.y+D.z*D.z);
                NkVec3f N = { camData.target.x-O.x, camData.target.y-O.y, camData.target.z-O.z };
                float32 nl = sqrtf(N.x*N.x+N.y*N.y+N.z*N.z);
                if (dl > 1e-6f && nl > 1e-6f) {
                    D.x/=dl; D.y/=dl; D.z/=dl; N.x/=nl; N.y/=nl; N.z/=nl;
                    float32 denom = D.x*N.x+D.y*N.y+D.z*N.z;
                    if (fabsf(denom) > 1e-5f) {
                        NkVec3f AO = { st->anchor.x-O.x, st->anchor.y-O.y, st->anchor.z-O.z };
                        float32 tt = (AO.x*N.x+AO.y*N.y+AO.z*N.z) / denom;
                        if (tt > 0.f) {
                            NkVec3f hit = { O.x+D.x*tt, O.y+D.y*tt, O.z+D.z*tt };
                            NkVec3f rel = { hit.x-st->anchor.x, hit.y-st->anchor.y, hit.z-st->anchor.z };
                            float32 rd = sqrtf(rel.x*rel.x+rel.y*rel.y+rel.z*rel.z);
                            float32 mx = st->reach * 0.98f;       // garde l'IK solvable
                            if (rd > mx && rd > 1e-6f) {
                                float32 s = mx/rd;
                                hit = { st->anchor.x+rel.x*s, st->anchor.y+rel.y*s, st->anchor.z+rel.z*s };
                            }
                            st->manualTarget = hit;
                        }
                    }
                }
            }
        }
        NkVec3f target = st->hasManual ? st->manualTarget : animTarget;

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

        // ── Recompose le GLOBAL par AIM-FK hiérarchique (tear-free) ───────────
        // (1) Joints de chaîne, racine->tip : chaque joint PIVOTE (autour de son
        // origine) pour amener son enfant sur la position résolue par FABRIK, puis
        // enfant = parent * localBind (FK cohérente -> atteint les positions FABRIK
        // SANS double-rotation, contrairement à un compoundage naïf). (2) FK topo
        // pour tous les autres joints (descendants suivent leur ancêtre de chaîne).
        ComputeIKWorld(st, res, resCount, st->world);

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
            overlay->DrawText({20.f,75.f}, "joints:%u  reach:%.3f  drag:%d (clic gauche = deplacer l'effecteur)",
                              jc, st->reach, st->dragging?1:0);
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
