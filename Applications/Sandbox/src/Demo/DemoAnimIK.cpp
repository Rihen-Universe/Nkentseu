// =============================================================================
// DemoAnimIK.cpp — NkAnima M1+M0 : IK PAR-DESSUS une animation (signature Cascadeur)
// -----------------------------------------------------------------------------
// Le corps MARCHE (clip .nkanim rejoué par NkAnimationPlayer) ET en même temps un
// BRAS atteint une cible via IK FABRIK, appliqué SUR la pose animée :
//   1. player.Update -> pose animée (matrices de skinning)
//   2. on récupère les transforms MONDE animés : animGlobal = animSkin * bindGlobal
//   3. FABRIK sur la chaîne du bras vers une cible (NkIKSystem)
//   4. aim-FK cohérente avec la pose animée comme base -> newGlobal
//   5. skin = newGlobal * inverseBind -> SubmitSkinned
// = animation + IK ensemble. Engine-level (sert le jeu : foot-lock, hand-IK).
//   renderdemo --demo=17        NK_SKIN_MODEL=<chemin>
// =============================================================================
#include "DemoCommon.h"
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKRenderer/Mesh/NkGLTFMaterialBridge.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/Animation/NkAnimationSystem.h"
#include "NKRenderer/Tools/IK/NkIKSystem.h"
#include "NKLogger/NkLog.h"
#include <cmath>
#include <cstdlib>

namespace nkentseu { namespace demo {

    using namespace nkentseu::renderer;

    struct DemoAnimIKState {
        NkGLTFMeshData*   gltf = nullptr;
        bool              ready = false;
        NkMeshHandle      mesh;
        NkGLTFMaterialSet matSet;
        NkMatInstHandle   skinMat;
        NkVector<NkMatInstHandle> matSlots;

        NkAnimationClip   clip;            // walk rechargé de .nkanim
        NkAnimationPlayer player;

        // squelette
        NkVector<NkMat4f> bindGlobal;       // transforms monde bind par joint
        NkVector<int32>   parentJoint;
        NkVector<uint32>  topo;
        NkVector<int32>   chain;            // bras (racine->main)
        NkVector<bool>    inChain;
        float32           reach = 1.f;

        NkIKSystem        ik;
        NkIKRig*          rig = nullptr;
        NkIKChainId       chainId{};

        // scratch
        NkVector<NkMat4f> animGlobal, baseLocal, world, skin;

        NkVec3f center={0,0,0}; float32 radius=4.f; NkVec3f anchorBind={0,0,0};
    };

    static NkString PickModel() {
        const char* env = getenv("NK_SKIN_MODEL");
        if (env && env[0]) return NkString(env);
        return NkString("Resources/Models/CesiumMan/CesiumMan.glb");
    }
    static NkVec3f JP(const NkMat4f& m){ return {m.position.x,m.position.y,m.position.z}; }

    bool DemoAnimIK_Init(DemoCtx& ctx) {
        auto* st = new DemoAnimIKState();
        ctx.userData = st;
        auto* meshSys = ctx.renderer->GetMeshSystem();
        if (!meshSys) { logger.Errorf("[DemoAnimIK] MeshSystem manquant\n"); return true; }

        NkString path = PickModel();
        st->gltf = new NkGLTFMeshData();
        if (!LoadGLTF(path, *st->gltf)) { logger.Errorf("[DemoAnimIK] echec glTF\n"); return true; }
        NkGLTFMeshData& data = *st->gltf;
        if (!data.isSkinned || data.skinnedVertices.Empty()) { logger.Errorf("[DemoAnimIK] non skinne\n"); return true; }

        // Mesh + matériaux
        NkMeshDesc d; d.layout=renderer::NkVertexLayout::Skinned();
        d.vertices=data.skinnedVertices.Data(); d.vertexCount=(uint32)data.skinnedVertices.Size();
        d.indices=data.indices.Data(); d.indexCount=(uint32)data.indices.Size();
        d.subMeshes=data.subMeshes; d.bounds=data.bounds; d.debugName=data.debugName;
        st->mesh=meshSys->Create(d);
        if (auto* matSys=ctx.renderer->GetMaterials()) if (auto* texLib=ctx.renderer->GetTextures()) {
            BuildGLTFMaterials(data, matSys, texLib, st->matSet);
            int32 mi0=(!data.subMeshMaterial.Empty())?data.subMeshMaterial[0]:-1;
            st->skinMat=st->matSet.InstanceForMaterial(mi0);
            if(!st->skinMat.IsValid()) for(uint32 mi=0;mi<(uint32)st->matSet.instances.Size();++mi)
                if(st->matSet.instances[mi].IsValid()){st->skinMat=st->matSet.instances[mi];break;}
            uint32 nSubs=meshSys->GetSubMeshCount(st->mesh); st->matSlots.Resize(nSubs);
            for(uint32 si=0;si<nSubs;++si){ int32 m=(si<(uint32)data.subMeshMaterial.Size())?data.subMeshMaterial[si]:-1;
                NkMatInstHandle h=st->matSet.InstanceForMaterial(m); st->matSlots[si]=h.IsValid()?h:st->skinMat; }
        }

        // Clip walk : bake + reload .nkanim + play
        {
            int32 animIdx = data.animations.Empty()? -1 : 0;
            NkAnimationClip baked;
            if (baked.BakeFromGLTF(data, animIdx, 30.f)) {
                NkString p("Build/Bin/Debug-Windows/renderdemo/cesiumman_walk.nkanim");
                if (baked.SaveBinary(p) && st->clip.LoadBinary(p)) {
                    st->player.SetClip(&st->clip);
                    st->player.Play(NkPlayMode::NK_LOOP, 1.f);
                }
            }
        }

        // Squelette bind : on prend la HIÉRARCHIE (parentJoint) d'EvaluateGLTFWorldJoints,
        // mais bindGlobal[j] = inverse(inverseBind[j]) -> garantit bindGlobal*inverseBind=I,
        // donc animGlobal = animSkin*bindGlobal exact (sinon pose contorsionnée : le global
        // d'EvaluateGLTFWorldJoints diffère du global implicite des inverseBind du skin).
        if (!EvaluateGLTFWorldJoints(data, -1, 0.f, st->bindGlobal, st->parentJoint)) { logger.Errorf("[DemoAnimIK] world joints fail\n"); return true; }
        const uint32 jc=(uint32)st->bindGlobal.Size();
        for (uint32 j=0;j<jc;++j)
            st->bindGlobal[j] = (j<(uint32)data.inverseBind.Size()) ? data.inverseBind[j].Inverse() : NkMat4f::Identity();
        // topo (parent avant enfant)
        { NkVector<bool> placed; placed.Resize(jc); for(uint32 j=0;j<jc;++j) placed[j]=false;
          uint32 done=0,g=0; while(done<jc && g++<jc+2){ for(uint32 j=0;j<jc;++j){ if(placed[j])continue;
            int32 p=st->parentJoint[j]; if(p<0||placed[(uint32)p]){ st->topo.PushBack(j); placed[j]=true; ++done; } } }
          for(uint32 j=0;j<jc;++j) if(!placed[j]) st->topo.PushBack(j); }

        // Chaîne = bras distal (plus longue chaîne feuille->racine, 3 os)
        { NkVector<int32> depth; depth.Resize(jc); NkVector<bool> hasChild; hasChild.Resize(jc);
          for(uint32 j=0;j<jc;++j){depth[j]=0;hasChild[j]=false;}
          for(uint32 j=0;j<jc;++j){ int32 p=st->parentJoint[j]; if(p>=0)hasChild[p]=true;
            int32 dd=0,c=st->parentJoint[j],gg=0; while(c>=0&&gg++<(int32)jc){++dd;c=st->parentJoint[c];} depth[j]=dd; }
          int32 leaf=-1,best=-1; for(uint32 j=0;j<jc;++j) if(!hasChild[j]&&depth[j]>best){best=depth[j];leaf=(int32)j;}
          if(leaf<0) leaf=(int32)jc-1;
          NkVector<int32> up; { int32 c=leaf; uint32 n=0; while(c>=0&&n<3){ up.PushBack(c); c=st->parentJoint[c]; ++n; } }
          for(uint32 i=up.Size();i>0;--i) st->chain.PushBack(up[i-1]); }
        st->inChain.Resize(jc); for(uint32 j=0;j<jc;++j) st->inChain[j]=false;
        for(uint32 i=0;i<(uint32)st->chain.Size();++i) st->inChain[(uint32)st->chain[i]]=true;
        st->reach=0.f;
        for(uint32 i=0;i+1<(uint32)st->chain.Size();++i){ NkVec3f a=JP(st->bindGlobal[(uint32)st->chain[i]]),b=JP(st->bindGlobal[(uint32)st->chain[i+1]]);
            NkVec3f dd={b.x-a.x,b.y-a.y,b.z-a.z}; st->reach+=sqrtf(dd.x*dd.x+dd.y*dd.y+dd.z*dd.z); }
        if(st->reach<1e-3f) st->reach=0.3f;
        st->anchorBind = st->chain.Empty()?st->center:JP(st->bindGlobal[(uint32)st->chain[0]]);

        // Rig FABRIK (boneIdx = indice de joint, pose = tous les joints)
        st->rig=st->ik.CreateRig(1);
        NkIKChainDesc desc; desc.name="arm"; desc.solver=NkIKSolver::NK_FABRIK; desc.maxIterations=16; desc.tolerance=0.0005f;
        for(uint32 i=0;i<(uint32)st->chain.Size();++i){ NkIKBone b; b.boneIdx=(uint32)st->chain[i]; b.length=0.f; desc.bones.PushBack(b); }
        st->chainId=st->rig->AddChain(desc);
        st->ready = st->chain.Size()>=2;

        st->animGlobal.Resize(jc); st->baseLocal.Resize(jc); st->world.Resize(jc); st->skin.Resize(jc);

        // Cadre caméra sur le MESH skinné à la pose initiale (pas les joints, trop petits).
        NkAABB box=NkAABB::Empty();
        st->player.Update(0.f);
        const NkVector<NkMat4f>& b0 = st->player.GetState().boneMatrices;
        if (!b0.Empty()) {
            for (uint32 vi=0; vi<(uint32)data.skinnedVertices.Size(); ++vi) {
                const NkVertexSkinned& sv=data.skinnedVertices[vi];
                NkMat4f m; for(int e=0;e<16;e++) m.data[e]=0.f; float32 ws=0.f;
                for(int b=0;b<4;b++){ int j=(int)(sv.boneIdx[b]+0.5f); float32 w=sv.boneWeight[b];
                    if(j>=0&&j<(int)b0.Size()&&w>0.f){ for(int e=0;e<16;e++) m.data[e]+=w*b0[(uint32)j].data[e]; ws+=w; } }
                if(ws<1e-4f) m=NkMat4f::Identity();
                NkVec4f wp=m*NkVec4f{sv.pos.x,sv.pos.y,sv.pos.z,1.f}; box.Expand(NkVec3f{wp.x,wp.y,wp.z});
            }
        } else { for(uint32 j=0;j<jc;++j) box.Expand(JP(st->bindGlobal[j])); }
        st->center={(box.min.x+box.max.x)*0.5f,(box.min.y+box.max.y)*0.5f,(box.min.z+box.max.z)*0.5f};
        NkVec3f full={box.max.x-box.min.x, box.max.y-box.min.y, box.max.z-box.min.z};
        float32 diag=sqrtf(full.x*full.x+full.y*full.y+full.z*full.z);  // diag COMPLET
        st->radius=NkMax(2.5f, diag*1.7f);

        logger.Info("[DemoAnimIK] Init : {0} joints, bras={1} os, reach={2}, ready={3}\n",
                    jc, (uint32)st->chain.Size(), st->reach, st->ready?1:0);
        return true;
    }

    void DemoAnimIK_Frame(DemoCtx& ctx, float32 dt) {
        auto* st=(DemoAnimIKState*)ctx.userData; if(!st) return;
        if(!ctx.renderer->BeginFrame()) return;
        auto* r3d=ctx.renderer->GetRender3D();
        if(!r3d){ ctx.renderer->Present(); ctx.renderer->EndFrame(); return; }
        const uint32 jc=(uint32)st->bindGlobal.Size();
        // Garde-fou : glTF non chargé (modèle manquant) -> squelette vide. On présente
        // une frame vide au lieu d'indexer des NkVector vides (assert/crash).
        if(jc==0 || st->chain.Empty()){ ctx.renderer->Present(); ctx.renderer->EndFrame(); return; }
        const float32 t=ctx.totalTime;

        // 1) pose animée -> matrices de skinning
        st->player.Update(dt);
        const NkVector<NkMat4f>& animSkin = st->player.GetState().boneMatrices;

        // 2) transforms MONDE animés : animGlobal = animSkin * bindGlobal
        for(uint32 j=0;j<jc;++j)
            st->animGlobal[j] = (j<(uint32)animSkin.Size()) ? (animSkin[j]*st->bindGlobal[j]) : st->bindGlobal[j];

        // base locale de la pose ANIMÉE (pour la FK cohérente)
        for(uint32 j=0;j<jc;++j){ int32 p=st->parentJoint[j];
            st->baseLocal[j] = (p>=0)? (st->animGlobal[(uint32)p].Inverse()*st->animGlobal[j]) : st->animGlobal[j]; }

        // 3) cible du bras : point monde qui tourne autour de l'épaule animée
        NkVec3f shoulder = JP(st->animGlobal[(uint32)st->chain[0]]);
        NkVec3f target = { shoulder.x + sinf(t*1.3f)*st->reach*0.95f,
                           shoulder.y + cosf(t*1.0f)*st->reach*0.6f,
                           shoulder.z + cosf(t*1.3f)*st->reach*0.95f };

        const uint32 cn=(uint32)st->chain.Size();
        // Bypass IK (diagnostic) : world = pose animée directe (doit = DemoAnim propre).
        static int noik=-1; if(noik<0){ const char* v=getenv("NK_ANIMIK_NOIK"); noik=(v&&v[0]&&v[0]!='0')?1:0; }
        if(noik){
            for(uint32 j=0;j<jc;++j) st->world[j]=st->animGlobal[j];
            for(uint32 j=0;j<jc;++j){ NkMat4f ib=(j<(uint32)st->gltf->inverseBind.Size())?st->gltf->inverseBind[j]:NkMat4f::Identity(); st->skin[j]=st->world[j]*ib; }
        } else {

        // FABRIK sur la pose animée -> positions résolues
        const NkMat4f* res=st->animGlobal.Data(); uint32 rc=jc;
        if(st->ready){
            st->rig->SetWorldPose(st->animGlobal.Data(), jc);
            st->rig->SetTarget(st->chainId, target);
            st->ik.Solve(dt);
            res=st->rig->GetBoneMatrices(); rc=st->rig->GetBoneMatrixCount();
        }

        // 4) aim-FK cohérente : base = pose animée, chaîne pivote vers les positions FABRIK
        if(cn>=1) st->world[(uint32)st->chain[0]] = st->animGlobal[(uint32)st->chain[0]];
        auto rotP=[](const NkMat4f& m){ NkMat4f r=m; r.m30=0;r.m31=0;r.m32=0;r.m33=1; return r; };
        for(uint32 i=0;i+1<cn;++i){
            uint32 j=(uint32)st->chain[i], c=(uint32)st->chain[i+1];
            NkMat4f childLocal = st->animGlobal[j].Inverse()*st->animGlobal[c];
            NkMat4f Gj=st->world[j]; NkVec3f jp=JP(Gj);
            NkVec3f cp=JP(Gj*childLocal);
            NkVec3f np=(c<rc)?JP(res[c]):cp;
            NkVec3f a={cp.x-jp.x,cp.y-jp.y,cp.z-jp.z}, b={np.x-jp.x,np.y-jp.y,np.z-jp.z};
            float32 al=sqrtf(a.x*a.x+a.y*a.y+a.z*a.z), bl=sqrtf(b.x*b.x+b.y*b.y+b.z*b.z);
            if(al>1e-6f&&bl>1e-6f){ a.x/=al;a.y/=al;a.z/=al; b.x/=bl;b.y/=bl;b.z/=bl;
                NkQuatf Rw(a,b); st->world[j]=NkMat4f::Translate(jp)*(Rw.ToMat4()*rotP(Gj)); }
            st->world[c]=st->world[j]*childLocal;
        }
        // FK topo pour les joints hors chaîne (suivent la pose animée)
        for(uint32 oi=0;oi<(uint32)st->topo.Size();++oi){ uint32 j=st->topo[oi];
            if(st->inChain[j]) continue; int32 p=st->parentJoint[j];
            st->world[j]=(p>=0)?(st->world[(uint32)p]*st->baseLocal[j]):st->baseLocal[j]; }

        // 5) skin = world * inverseBind
        for(uint32 j=0;j<jc;++j){ NkMat4f ib=(j<(uint32)st->gltf->inverseBind.Size())?st->gltf->inverseBind[j]:NkMat4f::Identity();
            st->skin[j]=st->world[j]*ib; }
        } // fin else (IK actif)

        // ── Caméra + scène ────────────────────────────────────────────────────
        NkCamera3DData cd; const float32 a=t*0.35f;
        cd.position={st->center.x+sinf(a)*st->radius, st->center.y+st->radius*0.12f, st->center.z+cosf(a)*st->radius};
        cd.target=st->center; cd.up={0,1,0}; cd.fovY=50.f; cd.aspect=(float32)ctx.width/(float32)NkMax(1u,ctx.height);
        cd.nearPlane=0.02f; cd.farPlane=NkMax(50.f,st->radius*8.f); NkCamera3D cam(cd);
        NkSceneContext sc; sc.camera=cam; sc.time=t;
        NkLightDesc sun; sun.type=NkLightType::NK_DIRECTIONAL; sun.direction={-0.4f,-0.8f,-0.5f};
        sun.color={1.f,0.97f,0.92f}; sun.intensity=2.2f; sun.castShadow=true; sc.lights.PushBack(sun);
        NkLightDesc fill; fill.type=NkLightType::NK_DIRECTIONAL; fill.direction={0.5f,-0.3f,0.6f};
        fill.color={0.6f,0.7f,0.9f}; fill.intensity=0.8f; sc.lights.PushBack(fill);
        sc.ambientIntensity=0.45f; r3d->BeginScene(sc);

        if(auto* ms=ctx.renderer->GetMeshSystem()){ float32 fy=st->center.y-st->radius*0.5f; float32 s=NkMax(2.f,st->radius);
            NkDrawCall3D dc; dc.mesh=ms->GetPlane(); dc.transform=NkMat4f::Translate({st->center.x,fy,st->center.z})*NkMat4f::Scale({s,1.f,s});
            dc.aabb={{st->center.x-s,fy-0.01f,st->center.z-s},{st->center.x+s,fy+0.01f,st->center.z+s}};
            dc.tint={0.18f,0.18f,0.22f}; dc.roughness=0.95f; r3d->Submit(dc); }

        bool submitted=false;
        if(st->mesh.IsValid()){
            NkDrawCallSkinned dc; dc.mesh=st->mesh; dc.transform=NkMat4f::Identity();
            dc.boneMatrices=st->skin; dc.material=st->skinMat; dc.materialSlots=st->matSlots;
            dc.tint=st->skinMat.IsValid()?NkVec3f{1,1,1}:NkVec3f{0.85f,0.55f,0.45f};
            dc.aabb=NkAABB{{st->center.x-st->radius,st->center.y-st->radius,st->center.z-st->radius},
                           {st->center.x+st->radius,st->center.y+st->radius,st->center.z+st->radius}};
            dc.castShadow=true; r3d->SubmitSkinned(dc); submitted=true;
        }
        // cible + chaîne IK en debug-lines
        for(uint32 i=0;i+1<cn;++i) r3d->DrawDebugLine(JP(st->world[(uint32)st->chain[i]]), JP(st->world[(uint32)st->chain[i+1]]), {1.f,0.55f,0.1f,1.f});
        r3d->DrawDebugSphere(target, st->reach*0.08f, {1.f,0.2f,0.2f,1.f});

        if(auto* ov=ctx.renderer->GetOverlay()){
            ov->BeginOverlay(ctx.renderer->GetCmd(), ctx.width, ctx.height);
            ov->DrawText({20.f,35.f}, "DemoAnimIK (NkAnima M1+M0)  |  API : %s", NkGraphicsApiName(ctx.api));
            ov->DrawText({20.f,55.f}, "le corps MARCHE (clip) + le bras atteint la cible (IK) en meme temps");
            ov->DrawText({20.f,75.f}, "bras=%u os  reach=%.3f  t=%.2fs  submitted:%d", cn, st->reach, st->player.GetTime(), submitted?1:0);
            ov->DrawText({20.f,95.f}, "FPS:%.0f", dt>1e-5f?1.f/dt:0.f);
            ov->EndOverlay();
        }
        ctx.renderer->Present(); ctx.renderer->EndFrame();
    }

    void DemoAnimIK_Shutdown(DemoCtx& ctx){ auto* st=(DemoAnimIKState*)ctx.userData; if(st){ delete st->gltf; delete st; } ctx.userData=nullptr; }

}} // namespace nkentseu::demo
