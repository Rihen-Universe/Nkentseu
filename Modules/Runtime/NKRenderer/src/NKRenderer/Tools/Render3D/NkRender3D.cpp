// =============================================================================
// NkRender3D.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkRender3D.h"
#include "NKRenderer/Tools/Shadow/NkShadowSystem.h"
#include <cstring>
#include <algorithm>

namespace nkentseu {
namespace renderer {

    NkRender3D::~NkRender3D() { Shutdown(); }

    bool NkRender3D::Init(NkIDevice* device, NkMeshSystem* mesh,
                           NkMaterialSystem* mat, NkRenderGraph* graph,
                           NkShadowSystem* shadow) {
        mDevice=device; mMesh=mesh; mMat=mat; mGraph=graph; mShadow=shadow;

        // Camera UBO (binding 0)
        NkBufferDesc bd; bd.type=NkBufferType::NK_UNIFORM;
        bd.usage=NkBufferUsage::NK_DYNAMIC;
        bd.size=sizeof(NkCameraUBO); mUBOCamera=mDevice->CreateBuffer(bd);

        // Object UBO (binding 1)
        struct ObjectUBO { NkMat4f model,normalMatrix; NkVec4f tint;
            float32 metallic,roughness,ao,emissiveStr,normalStr,_p[3]; };
        bd.size=sizeof(ObjectUBO); mUBOObject=mDevice->CreateBuffer(bd);

        // Lights UBO (binding 2)
        struct LightsUBO { NkVec4f pos[32],color[32],dir[32],angles[32]; int32 count,_p[3]; };
        bd.size=sizeof(LightsUBO); mUBOLights=mDevice->CreateBuffer(bd);

        // Bones SSBO (binding 3)
        bd.type=NkBufferType::NK_STORAGE; bd.size=256*sizeof(NkMat4f);
        mSSBOBones=mDevice->CreateBuffer(bd);

        return mUBOCamera.IsValid();
    }

    void NkRender3D::Shutdown() {
        if(mUBOCamera.IsValid()){mDevice->DestroyBuffer(mUBOCamera);mUBOCamera={};}
        if(mUBOObject.IsValid()){mDevice->DestroyBuffer(mUBOObject);mUBOObject={};}
        if(mUBOLights.IsValid()){mDevice->DestroyBuffer(mUBOLights);mUBOLights={};}
        if(mSSBOBones.IsValid()){mDevice->DestroyBuffer(mSSBOBones);mSSBOBones={};}
    }

    // ── Scene ─────────────────────────────────────────────────────────────────
    void NkRender3D::BeginScene(const NkSceneContext& ctx) {
        mCtx = ctx;
        mInScene = true;
        mOpaque.Clear(); mTransparent.Clear();
        mInstanced.Clear(); mSkinned.Clear();
    }

    // ── Submit ────────────────────────────────────────────────────────────────
    void NkRender3D::Submit(const NkDrawCall3D& dc) {
        if (!dc.visible) return;
        // Frustum culling
        if (!mCtx.camera.IsAABBVisible(dc.aabb)) return;

        NkVec3f camPos = mCtx.camera.GetPosition();
        NkVec3f center = dc.aabb.Center();
        float32 depth  = (center.x-camPos.x)*(center.x-camPos.x)+
                          (center.y-camPos.y)*(center.y-camPos.y)+
                          (center.z-camPos.z)*(center.z-camPos.z);

        // TODO: lookup queue du matériau
        // Pour l'instant, tout en opaque
        mOpaque.PushBack({dc, depth});
    }

    void NkRender3D::SubmitMany(const NkDrawCall3D* dcs, uint32 count) {
        for (uint32 i=0; i<count; i++) Submit(dcs[i]);
    }

    void NkRender3D::SubmitInstanced(const NkDrawCallInstanced& dc) {
        mInstanced.PushBack(dc);
    }

    void NkRender3D::SubmitSkinned(const NkDrawCallSkinned& dc) {
        mSkinned.PushBack(dc);
    }

    void NkRender3D::SubmitSkinnedTinted(const NkDrawCallSkinned& dc,
                                           NkVec3f tint, float32 alpha) {
        NkDrawCallSkinned copy = dc;
        copy.tint  = tint;
        copy.alpha = alpha;
        mSkinned.PushBack(copy);
    }

    // ── Sort ──────────────────────────────────────────────────────────────────
    void NkRender3D::SortDrawCalls() {
        // Front-to-back pour opaques (early-z)
        // std sort sur NkVector via raw pointer
        if (!mOpaque.Empty()) {
            // Simple insertion sort (assez rapide pour <1000 objs typiques)
            for (uint32 i=1;i<(uint32)mOpaque.Size();i++) {
                SortedDC key=mOpaque[i];
                int32 j=(int32)i-1;
                while(j>=0 && mOpaque[j].depth>key.depth){
                    mOpaque[j+1]=mOpaque[j]; j--;
                }
                mOpaque[j+1]=key;
            }
        }
    }

    // ── Flush ────────────────────────────────────────────────────────────────
    void NkRender3D::Flush(NkICommandBuffer* cmd) {
        if (!mInScene) return;
        SortDrawCalls();
        UploadUBOs(cmd);
        cmd->BindUniformBuffer(0, mUBOCamera);
        cmd->BindUniformBuffer(2, mUBOLights);
        FlushOpaque(cmd);
        FlushInstanced(cmd);
        FlushSkinned(cmd);
        FlushTransparent(cmd);
        FlushDebug(cmd);
        mInScene=false;
    }

    void NkRender3D::UploadUBOs(NkICommandBuffer* cmd) {
        // Camera UBO
        NkCameraUBO camUBO = mCtx.camera.BuildUBO(mCtx.time, mCtx.deltaTime);
        mDevice->UpdateBuffer(mUBOCamera, &camUBO, sizeof(camUBO));

        // Lights UBO
        struct LightsBlock {
            NkVec4f pos[32],color[32],dir[32],angles[32]; int32 count,_p[3];
        } lb{};
        lb.count=(int32)mCtx.lights.Size();
        for(int32 i=0;i<lb.count&&i<32;i++){
            auto& l=mCtx.lights[i];
            lb.pos[i]   ={l.position.x,l.position.y,l.position.z,(float32)l.type};
            lb.color[i] ={l.color.x,l.color.y,l.color.z,l.intensity};
            lb.dir[i]   ={l.direction.x,l.direction.y,l.direction.z,l.range};
            lb.angles[i]={l.innerAngle,l.outerAngle,(float32)l.castShadow,0};
        }
        mDevice->UpdateBuffer(mUBOLights, &lb, sizeof(lb));
    }

    void NkRender3D::FlushOpaque(NkICommandBuffer* cmd) {
        struct ObjBlock {
            NkMat4f model,normalMat; NkVec4f tint;
            float32 metallic,roughness,ao,emissStr,normalStr,_p[3];
        };
        for (auto& sdc : mOpaque) {
            auto& dc = sdc.dc;
            // Object UBO
            ObjBlock ob{}; ob.model=dc.transform;
            // Normal matrix = inverse transpose of upper-left 3x3
            ob.normalMat=NkMat4f::Transpose(NkMat4f::Inverse(dc.transform));
            ob.tint={dc.tint.x,dc.tint.y,dc.tint.z,dc.alpha};
            ob.metallic=0; ob.roughness=0.5f; ob.ao=1; ob.emissStr=0; ob.normalStr=1;
            mDevice->UpdateBuffer(mUBOObject, &ob, sizeof(ob));
            cmd->BindUniformBuffer(1, mUBOObject);

            // Bind material
            // TODO: lookup matière pour l'instance
            // Pour l'instant draw direct
            mMesh->BindMesh(cmd, dc.mesh);
            if (dc.subMeshIdx == 0xFFFFFFFFu)
                mMesh->DrawAll(cmd, dc.mesh);
            else
                mMesh->DrawSubMesh(cmd, dc.mesh, dc.subMeshIdx);
        }
    }

    void NkRender3D::FlushTransparent(NkICommandBuffer* cmd) {
        // Transparent triés back-to-front (si dans mTransparent)
        for (auto& sdc : mTransparent) {
            mMesh->BindMesh(cmd, sdc.dc.mesh);
            mMesh->DrawAll(cmd, sdc.dc.mesh);
        }
    }

    void NkRender3D::FlushInstanced(NkICommandBuffer* cmd) {
        for (auto& dc : mInstanced) {
            if (dc.transforms.Empty()) continue;
            // Upload instance transforms (SSBO ou VBO instancié)
            uint32 count=(uint32)dc.transforms.Size();
            mDevice->UpdateBuffer(mSSBOBones, dc.transforms.Data(),
                                    count*sizeof(NkMat4f));
            cmd->BindStorageBuffer(3, mSSBOBones);
            mMesh->BindMesh(cmd, dc.mesh);
            mMesh->DrawAll(cmd, dc.mesh, count);
        }
    }

    void NkRender3D::FlushSkinned(NkICommandBuffer* cmd) {
        for (auto& dc : mSkinned) {
            if (dc.boneMatrices.Empty()) continue;
            uint32 count=(uint32)dc.boneMatrices.Size();
            mDevice->UpdateBuffer(mSSBOBones, dc.boneMatrices.Data(),
                                    count*sizeof(NkMat4f));
            cmd->BindStorageBuffer(3, mSSBOBones);
            // Object UBO
            struct ObjB { NkMat4f m,nm; NkVec4f tint; float32 p[8]; } ob{};
            ob.m=dc.transform; ob.nm=NkMat4f::Transpose(NkMat4f::Inverse(dc.transform));
            ob.tint={dc.tint.x,dc.tint.y,dc.tint.z,dc.alpha};
            mDevice->UpdateBuffer(mUBOObject,&ob,sizeof(ob));
            cmd->BindUniformBuffer(1,mUBOObject);
            mMesh->BindMesh(cmd,dc.mesh);
            mMesh->DrawAll(cmd,dc.mesh);
        }
    }

    void NkRender3D::FlushDebug(NkICommandBuffer* cmd) {
        (void)cmd;
        // Mise à jour vie des debug lines
        for (uint32 i=0;i<(uint32)mDebugLines.Size();) {
            if (mDebugLines[i].life > 0.f) {
                mDebugLines[i].life -= mCtx.deltaTime;
                if (mDebugLines[i].life <= 0.f) { mDebugLines.RemoveAt(i); continue; }
            } else {
                mDebugLines.RemoveAt(i); continue;
            }
            i++;
        }
    }

    // ── Debug gizmos ─────────────────────────────────────────────────────────
    void NkRender3D::DrawDebugLine(NkVec3f a, NkVec3f b, NkVec4f color, float32 life) {
        mDebugLines.PushBack({a,b,color,life>0?life:0.016f});
    }
    void NkRender3D::DrawDebugSphere(NkVec3f c, float32 r, NkVec4f color) {
        const int N=16;
        for(int i=0;i<N;i++){
            float32 a0=2*3.14159f*i/N, a1=2*3.14159f*(i+1)/N;
            DrawDebugLine({c.x+cosf(a0)*r,c.y+sinf(a0)*r,c.z},
                           {c.x+cosf(a1)*r,c.y+sinf(a1)*r,c.z},color);
        }
    }
    void NkRender3D::DrawDebugAABB(const NkAABB& box, NkVec4f color) {
        NkVec3f mn=box.min,mx=box.max;
        // 12 arêtes
        DrawDebugLine({mn.x,mn.y,mn.z},{mx.x,mn.y,mn.z},color);
        DrawDebugLine({mx.x,mn.y,mn.z},{mx.x,mx.y,mn.z},color);
        DrawDebugLine({mx.x,mx.y,mn.z},{mn.x,mx.y,mn.z},color);
        DrawDebugLine({mn.x,mx.y,mn.z},{mn.x,mn.y,mn.z},color);
        DrawDebugLine({mn.x,mn.y,mx.z},{mx.x,mn.y,mx.z},color);
        DrawDebugLine({mx.x,mn.y,mx.z},{mx.x,mx.y,mx.z},color);
        DrawDebugLine({mx.x,mx.y,mx.z},{mn.x,mx.y,mx.z},color);
        DrawDebugLine({mn.x,mx.y,mx.z},{mn.x,mn.y,mx.z},color);
        DrawDebugLine({mn.x,mn.y,mn.z},{mn.x,mn.y,mx.z},color);
        DrawDebugLine({mx.x,mn.y,mn.z},{mx.x,mn.y,mx.z},color);
        DrawDebugLine({mx.x,mx.y,mn.z},{mx.x,mx.y,mx.z},color);
        DrawDebugLine({mn.x,mx.y,mn.z},{mn.x,mx.y,mx.z},color);
    }
    void NkRender3D::DrawDebugAxes(const NkMat4f& t, float32 s) {
        NkVec3f orig={t[3][0],t[3][1],t[3][2]};
        DrawDebugLine(orig,{orig.x+t[0][0]*s,orig.y+t[0][1]*s,orig.z+t[0][2]*s},{1,0,0,1});
        DrawDebugLine(orig,{orig.x+t[1][0]*s,orig.y+t[1][1]*s,orig.z+t[1][2]*s},{0,1,0,1});
        DrawDebugLine(orig,{orig.x+t[2][0]*s,orig.y+t[2][1]*s,orig.z+t[2][2]*s},{0,0,1,1});
    }
    void NkRender3D::DrawDebugGrid(NkVec3f o, float32 sp, int32 lines, NkVec4f color) {
        float32 ext=sp*lines*0.5f;
        for(int32 i=-lines/2;i<=lines/2;i++){
            float32 f=(float32)i*sp;
            DrawDebugLine({o.x+f,o.y,o.z-ext},{o.x+f,o.y,o.z+ext},color);
            DrawDebugLine({o.x-ext,o.y,o.z+f},{o.x+ext,o.y,o.z+f},color);
        }
    }
    void NkRender3D::DrawDebugArrow(NkVec3f from, NkVec3f to, NkVec4f color) {
        DrawDebugLine(from,to,color);
        // TODO: petite pointe
    }

} // namespace renderer
} // namespace nkentseu
