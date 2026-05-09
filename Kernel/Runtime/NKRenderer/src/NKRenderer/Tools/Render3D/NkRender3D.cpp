// =============================================================================
// NkRender3D.cpp  — NKRenderer v5.0
// =============================================================================
#include "NkRender3D.h"
#include "../Shadow/NkShadowSystem.h"
#include "../Environment/NkEnvironmentSystem.h"
#include "NKRenderer/Shader/NkShaderLibrary.h"
#include "NKRenderer/Core/NkResources.h"
#include "NkRender3D_PBRShaders.inl"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <algorithm>

namespace nkentseu {
    namespace renderer {

        NkRender3D::~NkRender3D() { Shutdown(); }

        bool NkRender3D::Init(NkIDevice* device, NkMeshSystem* mesh,
                            NkMaterialSystem* mat, NkRenderGraph* graph,
                            NkShadowSystem* shadow,
                            NkEnvironmentSystem* env,
                            NkShaderLibrary* shaderLib,
                            NkResources* resources) {
            mDevice=device; mMesh=mesh; mMat=mat; mGraph=graph;
            mShadow=shadow; mEnv=env; mShaderLib=shaderLib; mResources=resources;

            // ── UBOs (matchent le shader pbr.vert/frag.gl.glsl) ──────────────────
            // Camera UBO — binding 0
            mUBOCamera = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(NkCameraUBO)));

            // Lights UBO — binding 2 (32 lights max)
            struct LightsUBO { NkVec4f pos[32],color[32],dir[32],angles[32]; int32 count,_p[3]; };
            mUBOLights = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(LightsUBO)));

            // Object UBO — binding 1. Layout std140 doit matcher EXACTEMENT le shader
            // pbr.vert/frag.gl.glsl (sinon le linker GL refuse, ou les reads out-of-buffer
            // donnent un undefined behavior selon le driver).
            struct ObjectUBO {
                NkMat4f model;            // 0
                NkMat4f normalMatrix;     // 64
                NkVec4f tint;             // 128
                float32 metallic;         // 144
                float32 roughness;        // 148
                float32 aoStrength;       // 152
                float32 emissiveStrength; // 156
                float32 normalStrength;   // 160
                float32 clearcoat;        // 164
                float32 clearcoatRough;   // 168
                float32 subsurface;       // 172
                NkVec4f subsurfaceColor;  // 176 (aligned to 16)
            };                            // total 192
            static_assert(sizeof(ObjectUBO) == 192, "ObjectUBO std140 layout");
            mUBOObject = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(ObjectUBO)));

            // Bones SSBO (utilise pour skinning — pas par le shader PBR de base)
            mSSBOBones = mDevice->CreateBuffer(NkBufferDesc::Storage(256*sizeof(NkMat4f)));

            // ── Descriptor set layouts ────────────────────────────────────────────
            // Frame set (set 0) : Camera(0) + Lights(2) + Shadow(3) + 4 textures
            // materiel par defaut(4-7) + Env irradiance/prefilter/BRDFLUT(8/9/10)
            // + Shadow map(11). Bindings matchent le shader PBR.
            NkDescriptorSetLayoutDesc frameLayout;
            frameLayout
                .Add(0,  NkDescriptorType::NK_UNIFORM_BUFFER,        ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(2,  NkDescriptorType::NK_UNIFORM_BUFFER,        ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(3,  NkDescriptorType::NK_UNIFORM_BUFFER,        ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(4,  NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(5,  NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(6,  NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(7,  NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(8,  NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(9,  NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(10, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(11, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
            mGlobalLayout = mDevice->CreateDescriptorSetLayout(frameLayout);
            mGlobalSet    = mDevice->AllocateDescriptorSet(mGlobalLayout);

            // Object set (set 1) : Object UBO(1)
            NkDescriptorSetLayoutDesc objectLayout;
            objectLayout.Add(1, NkDescriptorType::NK_UNIFORM_BUFFER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
            mObjectLayout = mDevice->CreateDescriptorSetLayout(objectLayout);
            mObjectSet    = mDevice->AllocateDescriptorSet(mObjectLayout);

            // ── Pre-bind static buffers + textures into descriptor sets ──────────
            if (mGlobalSet.IsValid()) {
                mDevice->BindUniformBuffer(mGlobalSet, 0, mUBOCamera);
                mDevice->BindUniformBuffer(mGlobalSet, 2, mUBOLights);

                // ShadowUBO depuis le ShadowSystem (cascadeCount=0 en mode stub D.3a)
                if (mShadow && mShadow->GetShadowUBO().IsValid()) {
                    mDevice->BindUniformBuffer(mGlobalSet, 3, mShadow->GetShadowUBO());
                }

                // Textures materiel par defaut (4-7) — utilisees tant que NkMaterialSystem
                // n'est pas wire. Albedo=white, Normal=normal-up, ORM=white (=> ao=1, rough=1
                // multiplie par uObj.roughness, metal=1 multiplie par uObj.metallic), Emissive=black.
                NkTextureHandle defAlbedo  = mResources ? mResources->GetWhiteTex()  : NkTextureHandle{};
                NkTextureHandle defNormal  = mResources ? mResources->GetNormalTex() : NkTextureHandle{};
                NkTextureHandle defORM     = mResources ? mResources->GetWhiteTex()  : NkTextureHandle{};
                NkTextureHandle defEmissive= mResources ? mResources->GetBlackTex()  : NkTextureHandle{};
                NkSamplerHandle defSampler = mResources ? mResources->GetSamplerLinearRepeat() : NkSamplerHandle{};

                if (defAlbedo.IsValid())   mDevice->BindTextureSampler(mGlobalSet, 4, defAlbedo,   defSampler);
                if (defNormal.IsValid())   mDevice->BindTextureSampler(mGlobalSet, 5, defNormal,   defSampler);
                if (defORM.IsValid())      mDevice->BindTextureSampler(mGlobalSet, 6, defORM,      defSampler);
                if (defEmissive.IsValid()) mDevice->BindTextureSampler(mGlobalSet, 7, defEmissive, defSampler);

                // Environment maps depuis le EnvSystem
                if (mEnv) {
                    NkSamplerHandle envSamp = mEnv->GetEnvSampler();
                    NkSamplerHandle lutSamp = mEnv->GetLUTSampler();
                    if (mEnv->GetIrradianceCubemap().IsValid())
                        mDevice->BindTextureSampler(mGlobalSet, 8, mEnv->GetIrradianceCubemap(), envSamp);
                    if (mEnv->GetPrefilterCubemap().IsValid())
                        mDevice->BindTextureSampler(mGlobalSet, 9, mEnv->GetPrefilterCubemap(), envSamp);
                    if (mEnv->GetBRDFLUT().IsValid())
                        mDevice->BindTextureSampler(mGlobalSet, 10, mEnv->GetBRDFLUT(), lutSamp);
                }

                // Shadow atlas + sampler compare-mode
                if (mShadow && mShadow->GetAtlasTexture().IsValid()) {
                    mDevice->BindTextureSampler(mGlobalSet, 11,
                        mShadow->GetAtlasTexture(), mShadow->GetAtlasSampler());
                }
            }
            if (mObjectSet.IsValid()) {
                mDevice->BindUniformBuffer(mObjectSet, 1, mUBOObject);
            }

            // ── Compile shader PBR + cree pipeline ───────────────────────────────
            if (mShaderLib) {
                auto progHandle = mShaderLib->CompileVF(kPBR_VS, kPBR_FS, "PBR");
                if (progHandle.IsValid()) {
                    mPBRShader = mShaderLib->GetRHIHandle(progHandle);
                }
            }

            if (mPBRShader.IsValid()) {
                NkGraphicsPipelineDesc pd;
                pd.shader       = mPBRShader;
                pd.depthStencil = NkDepthStencilDesc::Default();   // depth test enabled
                // D.1 : NoCull tant que les meshes primitifs n'ont pas de winding
                // CCW garanti (le plane par exemple a un winding inverse).
                pd.rasterizer   = NkRasterizerDesc::NoCull();
                pd.blend        = NkBlendDesc::Opaque();
                pd.debugName    = "PBR_Opaque";
                pd.descriptorSetLayouts.PushBack(mGlobalLayout);
                pd.descriptorSetLayouts.PushBack(mObjectLayout);

                // Vertex layout — NkVertex3D : pos(vec3), normal(vec3), tangent(vec3),
                //   uv(vec2), uv2(vec2), color(uint32 RGBA8)
                //   Stride = 12+12+12+8+8+4 = 56 bytes
                pd.vertexLayout
                  .AddBinding(0, sizeof(NkVertex3D), false)
                  .AddAttribute(0, 0, NkVertexFormat::NK_RGB32_FLOAT, 0,  "POSITION", 0)
                  .AddAttribute(1, 0, NkVertexFormat::NK_RGB32_FLOAT, 12, "NORMAL",   0)
                  .AddAttribute(2, 0, NkVertexFormat::NK_RGB32_FLOAT, 24, "TANGENT",  0)
                  .AddAttribute(3, 0, NkVertexFormat::NK_RG32_FLOAT,  36, "TEXCOORD", 0)
                  .AddAttribute(4, 0, NkVertexFormat::NK_RG32_FLOAT,  44, "TEXCOORD", 1)
                  .AddAttribute(5, 0, NkVertexFormat::NK_RGBA8_UNORM, 52, "COLOR",    0);

                mPBRPipeline = mDevice->CreateGraphicsPipeline(pd);
            }

            return mUBOCamera.IsValid() && mPBRPipeline.IsValid();
        }

        void NkRender3D::Shutdown() {
            if (mPBRPipeline.IsValid()) { mDevice->DestroyPipeline(mPBRPipeline); mPBRPipeline={}; }
            // Le shader handle est detenu par NkShaderLibrary, pas a detruire ici.
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

            // Bind le pipeline PBR (configure le programme + render state + VAO).
            // Doit etre fait AVANT le bind des descriptor sets, car BindGraphicsPipeline
            // change le VAO actif sur OpenGL et reset les bindings de buffers.
            if (mPBRPipeline.IsValid()) {
                cmd->BindGraphicsPipeline(mPBRPipeline);
            }

            // Bind per-frame descriptor set (camera + lights + shadow + env + textures defaut)
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
            // Camera UBO — layout std140 qui matche EXACTEMENT le shader PBR (binding=0).
            // NB : NkCameraUBO (NkRendererTypes.h) a une layout differente (avec invView/
            // invProj separes) ; on ne peut pas l'utiliser directement.
            struct PBRCamUBO {
                NkMat4f view;
                NkMat4f proj;
                NkMat4f viewProj;
                NkMat4f invViewProj;
                NkVec4f camPos;       // .xyz = pos, .w = near
                NkVec4f camDir;       // .xyz = forward, .w = far
                float32 viewportX, viewportY;
                float32 time, deltaTime;
            };
            PBRCamUBO cb{};
            cb.view        = mCtx.camera.GetView();
            cb.proj        = mCtx.camera.GetProj();
            cb.viewProj    = mCtx.camera.GetViewProj();
            cb.invViewProj = cb.viewProj.Inverse();
            NkVec3f pos = mCtx.camera.GetPosition();
            NkVec3f fwd = mCtx.camera.GetForward();
            cb.camPos    = {pos.x, pos.y, pos.z, mCtx.camera.GetNear()};
            cb.camDir    = {fwd.x, fwd.y, fwd.z, mCtx.camera.GetFar()};
            cb.viewportX = (float32)mW;
            cb.viewportY = (float32)mH;
            cb.time      = mCtx.time;
            cb.deltaTime = mCtx.deltaTime;
            mDevice->WriteBuffer(mUBOCamera, &cb, sizeof(cb));

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
            // Doit matcher EXACTEMENT la layout std140 du shader pbr.{vert,frag}.gl.glsl.
            struct ObjBlock {
                NkMat4f model;
                NkMat4f normalMatrix;
                NkVec4f tint;
                float32 metallic;
                float32 roughness;
                float32 aoStrength;
                float32 emissiveStrength;
                float32 normalStrength;
                float32 clearcoat;
                float32 clearcoatRough;
                float32 subsurface;
                NkVec4f subsurfaceColor;
            };
            static_assert(sizeof(ObjBlock) == 192, "ObjBlock std140");
            for (auto& sdc : mOpaque) {
                auto& dc = sdc.dc;
                ObjBlock ob{};
                ob.model            = dc.transform;
                ob.normalMatrix     = dc.transform.Inverse().Transpose();
                ob.tint             = {dc.tint.x, dc.tint.y, dc.tint.z, dc.alpha};
                ob.metallic         = 0.f;
                ob.roughness        = 0.5f;
                ob.aoStrength       = 1.f;
                ob.emissiveStrength = 0.f;
                ob.normalStrength   = 1.f;
                // clearcoat / subsurface : 0 par defaut (zero-init via ObjBlock{}).
                // IMPORTANT : enqueuer l'ecriture dans le command buffer plutot que
                // mDevice->WriteBuffer immediat — sinon tous les drawcalls de la frame
                // partagent la DERNIERE valeur ecrite (la boucle ecrase a chaque
                // iteration, et les Draws sont enqueued -> ils voient tous le dernier).
                cmd->UpdateBuffer(mUBOObject, 0, sizeof(ob), &ob);

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
