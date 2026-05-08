// =============================================================================
// NkRender3D.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkRender3D.h"
#include "../Shadow/NkShadowSystem.h"
#include <cstring>
#include <algorithm>

namespace nkentseu {
    namespace renderer {

        NkRender3D::~NkRender3D() { Shutdown(); }

        bool NkRender3D::Init(NkIDevice* device, NkMeshSystem* mesh,
                            NkMaterialSystem* mat, NkRenderGraph* graph,
                            NkShadowSystem* shadow) {
            mDevice=device; mMesh=mesh; mMat=mat; mGraph=graph; mShadow=shadow;

            // Camera UBO (per-frame, set 0, binding 0)
            mUBOCamera = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(NkCameraUBO)));

            // Lights UBO (per-frame, set 0, binding 2)
            struct LightsUBO { NkVec4f pos[32],color[32],dir[32],angles[32]; int32 count,_p[3]; };
            mUBOLights = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(LightsUBO)));

            // Object UBO (per-object, set 1, binding 1)
            struct ObjectUBO { NkMat4f model,normalMatrix; NkVec4f tint;
                float32 metallic,roughness,ao,emissiveStr,normalStr,_p[3]; };
            mUBOObject = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(ObjectUBO)));

            // Bones SSBO (per-object, set 1, binding 3)
            mSSBOBones = mDevice->CreateBuffer(NkBufferDesc::Storage(256*sizeof(NkMat4f)));

            // Per-frame descriptor layout: binding 0 = camera UBO, binding 2 = lights UBO
            NkDescriptorSetLayoutDesc frameLayout;
            frameLayout.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_ALL_GRAPHICS)
                       .Add(2, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_ALL_GRAPHICS);
            mGlobalLayout = mDevice->CreateDescriptorSetLayout(frameLayout);
            mGlobalSet    = mDevice->AllocateDescriptorSet(mGlobalLayout);

            // Per-object descriptor layout: binding 1 = object UBO, binding 3 = bones SSBO
            NkDescriptorSetLayoutDesc objectLayout;
            objectLayout.Add(1, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_ALL_GRAPHICS)
                        .Add(3, NkDescriptorType::NK_STORAGE_BUFFER, NkShaderStage::NK_ALL_GRAPHICS);
            mObjectLayout = mDevice->CreateDescriptorSetLayout(objectLayout);
            mObjectSet    = mDevice->AllocateDescriptorSet(mObjectLayout);

            // Pre-bind static buffers into descriptor sets
            if (mGlobalSet.IsValid()) {
                mDevice->BindUniformBuffer(mGlobalSet, 0, mUBOCamera);
                mDevice->BindUniformBuffer(mGlobalSet, 2, mUBOLights);
            }
            if (mObjectSet.IsValid()) {
                mDevice->BindUniformBuffer(mObjectSet, 1, mUBOObject);
                NkDescriptorWrite bw{}; bw.set=mObjectSet; bw.binding=3;
                bw.type=NkDescriptorType::NK_STORAGE_BUFFER; bw.buffer=mSSBOBones;
                bw.bufferRange=256*sizeof(NkMat4f);
                mDevice->UpdateDescriptorSets(&bw, 1);
            }

            return mUBOCamera.IsValid();
        }

        void NkRender3D::Shutdown() {
            if (mGlobalSet.IsValid())    { mDevice->FreeDescriptorSet(mGlobalSet); }
            if (mObjectSet.IsValid())    { mDevice->FreeDescriptorSet(mObjectSet); }
            if (mGlobalLayout.IsValid()) { mDevice->DestroyDescriptorSetLayout(mGlobalLayout); }
            if (mObjectLayout.IsValid()) { mDevice->DestroyDescriptorSetLayout(mObjectLayout); }
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
            if (!mCtx.camera.IsAABBVisible(dc.aabb)) return;

            NkVec3f camPos = mCtx.camera.GetPosition();
            NkVec3f center = dc.aabb.Center();
            float32 dx=center.x-camPos.x, dy=center.y-camPos.y, dz=center.z-camPos.z;
            float32 depth = dx*dx + dy*dy + dz*dz;
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
            for (uint32 i=1;i<(uint32)mOpaque.Size();i++) {
                SortedDC key=mOpaque[i];
                int32 j=(int32)i-1;
                while(j>=0 && mOpaque[j].depth>key.depth){
                    mOpaque[j+1]=mOpaque[j]; j--;
                }
                mOpaque[j+1]=key;
            }
        }

        // ── Flush ────────────────────────────────────────────────────────────────
        void NkRender3D::Flush(NkICommandBuffer* cmd) {
            if (!mInScene) return;
            SortDrawCalls();
            UploadUBOs(cmd);

            // Bind per-frame descriptor set (camera + lights) at set index 0
            if (mGlobalSet.IsValid())
                cmd->BindDescriptorSet(mGlobalSet, 0);

            FlushOpaque(cmd);
            FlushInstanced(cmd);
            FlushSkinned(cmd);
            FlushTransparent(cmd);
            FlushDebug(cmd);
            mInScene=false;
        }

        void NkRender3D::UploadUBOs(NkICommandBuffer* cmd) {
            (void)cmd;
            // Camera UBO
            NkCameraUBO camUBO = mCtx.camera.BuildUBO(mCtx.time, mCtx.deltaTime);
            mDevice->WriteBuffer(mUBOCamera, &camUBO, sizeof(camUBO));

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
            mDevice->WriteBuffer(mUBOLights, &lb, sizeof(lb));
        }

        void NkRender3D::FlushOpaque(NkICommandBuffer* cmd) {
            struct ObjBlock {
                NkMat4f model,normalMat; NkVec4f tint;
                float32 metallic,roughness,ao,emissStr,normalStr,_p[3];
            };
            for (auto& sdc : mOpaque) {
                auto& dc = sdc.dc;
                ObjBlock ob{};
                ob.model=dc.transform;
                ob.normalMat=dc.transform.Inverse().Transpose();
                ob.tint={dc.tint.x,dc.tint.y,dc.tint.z,dc.alpha};
                ob.metallic=0; ob.roughness=0.5f; ob.ao=1; ob.emissStr=0; ob.normalStr=1;
                mDevice->WriteBuffer(mUBOObject, &ob, sizeof(ob));

                // Re-bind object descriptor set with updated UBO
                if (mObjectSet.IsValid())
                    cmd->BindDescriptorSet(mObjectSet, 1);

                mMesh->BindMesh(cmd, dc.mesh);
                if (dc.subMeshIdx == 0xFFFFFFFFu)
                    mMesh->DrawAll(cmd, dc.mesh);
                else
                    mMesh->DrawSubMesh(cmd, dc.mesh, dc.subMeshIdx);
            }
        }

        void NkRender3D::FlushTransparent(NkICommandBuffer* cmd) {
            for (auto& sdc : mTransparent) {
                mMesh->BindMesh(cmd, sdc.dc.mesh);
                mMesh->DrawAll(cmd, sdc.dc.mesh);
            }
        }

        void NkRender3D::FlushInstanced(NkICommandBuffer* cmd) {
            for (auto& dc : mInstanced) {
                if (dc.transforms.Empty()) continue;
                uint32 count=(uint32)dc.transforms.Size();
                mDevice->WriteBuffer(mSSBOBones, dc.transforms.Data(),
                                        count*sizeof(NkMat4f));
                if (mObjectSet.IsValid())
                    cmd->BindDescriptorSet(mObjectSet, 1);
                mMesh->BindMesh(cmd, dc.mesh);
                mMesh->DrawAll(cmd, dc.mesh, count);
            }
        }

        void NkRender3D::FlushSkinned(NkICommandBuffer* cmd) {
            for (auto& dc : mSkinned) {
                if (dc.boneMatrices.Empty()) continue;
                uint32 count=(uint32)dc.boneMatrices.Size();
                mDevice->WriteBuffer(mSSBOBones, dc.boneMatrices.Data(),
                                        count*sizeof(NkMat4f));

                struct ObjB { NkMat4f m,nm; NkVec4f tint; float32 p[8]; } ob{};
                ob.m=dc.transform; ob.nm=dc.transform.Inverse().Transpose();
                ob.tint={dc.tint.x,dc.tint.y,dc.tint.z,dc.alpha};
                mDevice->WriteBuffer(mUBOObject,&ob,sizeof(ob));

                if (mObjectSet.IsValid())
                    cmd->BindDescriptorSet(mObjectSet, 1);
                mMesh->BindMesh(cmd,dc.mesh);
                mMesh->DrawAll(cmd,dc.mesh);
            }
        }

        void NkRender3D::FlushDebug(NkICommandBuffer* cmd) {
            (void)cmd;
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
        }

    } // namespace renderer
} // namespace nkentseu
