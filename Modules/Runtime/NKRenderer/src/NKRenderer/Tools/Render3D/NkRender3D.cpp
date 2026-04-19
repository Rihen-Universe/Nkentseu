// =============================================================================
// NkRender3D.cpp — Renderer 3D PBR complet
// BeginScene / Submit / EndScene / Shadows CSM / Instancing / Debug
// =============================================================================
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Core/NkResourceManager.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKRenderer/Core/NkRenderGraph.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRHI/Core/NkDescs.h"
#include <algorithm>
#include <cmath>

// ── Shaders de debug embarqués ────────────────────────────────────────────────
namespace {
static const char* kDebugVert_GL = R"(
#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec4 aColor;
layout(std140,binding=0) uniform SceneUBO {
    mat4 uModel,uView,uProj,uLightVP;
    vec4 uLightDir,uEyePos; float uTime,uDt,uNdcZS,uNdcZO;
};
out vec4 vColor;
void main(){ vColor=aColor; gl_Position=uProj*uView*vec4(aPos,1.0); }
)";
static const char* kDebugFrag_GL = R"(
#version 460 core
in vec4 vColor; out vec4 FragColor;
void main(){ FragColor=vColor; }
)";
} // anonymous

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        bool NkRender3D::Init(NkIDevice* device, NkResourceManager* resources,
                               NkMaterialSystem* materials, NkRenderGraph* graph,
                               uint32 width, uint32 height)
        {
            if (!device || !resources || !materials || !graph) return false;
            mDevice    = device;
            mResources = resources;
            mMaterials = materials;
            mGraph     = graph;
            mWidth     = width;
            mHeight    = height;

            // ── Scene UBO ────────────────────────────────────────────────────────────
            struct SceneUBOLayout {
                NkMat4f model,view,proj,lightVP;
                NkVec4f lightDir,eyePos;
                float   time,dt,ndcZS,ndcZO;
            };
            NkBufferHandle sceneUBO = device->CreateBuffer(
                NkBufferDesc::Uniform(sizeof(SceneUBOLayout)));
            if (!sceneUBO.IsValid()) return false;
            mSceneUBORHI = sceneUBO.id;

            // ── Shadow UBO (matrices CSM, jusqu'à 4 cascades) ────────────────────────
            struct ShadowUBOLayout { NkMat4f lightVP[4]; float splits[4]; };
            NkBufferHandle shadowUBO = device->CreateBuffer(
                NkBufferDesc::Uniform(sizeof(ShadowUBOLayout)));
            if (!shadowUBO.IsValid()) return false;
            mShadowUBORHI = shadowUBO.id;

            // ── Shadow maps CSM ───────────────────────────────────────────────────────
            if (mShadowsEnabled) {
                mShadowMaps.Reserve(4);
                mShadowVP.Reserve(4);
                for (uint32 i = 0; i < 4; ++i) {
                    char dbgName[32]; snprintf(dbgName, 32, "ShadowMap_CSM%u", i);
                    NkTextureHandle sh = mResources->CreateDepthTexture(
                        mShadowMapSize, mShadowMapSize,
                        NkPixelFormat::NK_D32F, NkMSAA::NK_1X, dbgName);
                    NkRenderTargetHandle rt = mResources->CreateRenderTarget(
                        NkRenderTargetDesc{}.SetDepth(sh));
                    mShadowMaps.PushBack(rt);
                    mShadowVP.PushBack(NkMat4f::Identity());
                }
            }

            // ── Debug VBO (lignes) ────────────────────────────────────────────────────
            NkBufferHandle dbgVBO = device->CreateBuffer(
                NkBufferDesc::VertexDynamic((uint64)8192 * sizeof(NkVertexDebug)));
            if (!dbgVBO.IsValid()) return false;
            mDebugVBORHI = dbgVBO.id;

            // ── Debug shader & pipeline ───────────────────────────────────────────────
            {
                NkShaderDesc sd{};
                sd.debugName = "DebugLines";
                sd.AddGLSL(NkShaderStage::NK_VERTEX,   kDebugVert_GL);
                sd.AddGLSL(NkShaderStage::NK_FRAGMENT, kDebugFrag_GL);
                NkShaderHandle sh = device->CreateShader(sd);
                if (sh.IsValid()) mDebugShaderRHI = sh.id;
            }

            SetupRenderGraph();
            return true;
        }

        // =============================================================================
        void NkRender3D::Shutdown() {
            if (!mDevice) return;

            auto buf = [&](uint64& id) {
                if (!id) return;
                NkBufferHandle h; h.id=id; mDevice->DestroyBuffer(h); id=0;
            };
            buf(mSceneUBORHI);
            buf(mShadowUBORHI);
            buf(mDebugVBORHI);

            if (mDebugShaderRHI) {
                NkShaderHandle h; h.id=mDebugShaderRHI;
                mDevice->DestroyShader(h); mDebugShaderRHI=0;
            }
            if (mDebugPipeRHI) {
                NkPipelineHandle h; h.id=mDebugPipeRHI;
                mDevice->DestroyPipeline(h); mDebugPipeRHI=0;
            }

            mShadowMaps.Clear();
            mShadowVP.Clear();
            mOpaqueCalls.Clear();
            mAlphaCalls.Clear();
            mInstancedCalls.Clear();
            mDebugLines.Clear();

            mDevice = nullptr;
        }

        // =============================================================================
        void NkRender3D::Resize(uint32 w, uint32 h) {
            mWidth = w; mHeight = h;
        }

        // =============================================================================
        void NkRender3D::SetupRenderGraph() {
            // Le RenderGraph est configuré par NkRenderer (façade).
            // NkRender3D expose juste les callbacks nécessaires.
        }

        // =============================================================================
        void NkRender3D::BeginScene(const NkSceneContext3D& ctx) {
            NK_ASSERT(!mSceneOpen, "BeginScene appelé sans EndScene précédent");
            mCurrentScene = ctx;
            mSceneOpen    = true;
            mOpaqueCalls.Clear();
            mAlphaCalls.Clear();
            mInstancedCalls.Clear();
            mDebugLines.Clear();
            mStats = Stats3D{};
        }

        // =============================================================================
        void NkRender3D::Submit(const NkDrawCall3D& dc) {
            if (!mSceneOpen || !dc.visible) return;
            // Tri opaque vs transparent via sortKey
            bool isTransparent = (dc.sortKey >> 31) & 1;
            if (isTransparent) mAlphaCalls.PushBack(dc);
            else               mOpaqueCalls.PushBack(dc);
        }

        void NkRender3D::Submit(NkMeshHandle mesh, NkMaterialInstHandle material,
                                  const NkMat4f& transform, bool castShadow)
        {
            NkDrawCall3D dc{};
            dc.mesh       = mesh;
            dc.material   = material;
            dc.transform  = transform;
            dc.castShadow = castShadow;
            dc.aabb       = mResources->GetMeshAABB(mesh).Transform(transform);
            Submit(dc);
        }

        void NkRender3D::SubmitInstanced(const NkDrawCallInstanced& dc) {
            if (!mSceneOpen || dc.transforms.IsEmpty()) return;
            mInstancedCalls.PushBack(dc);
        }

        void NkRender3D::DrawDebugLine(const NkVec3f& a, const NkVec3f& b,
                                        const NkColorF& c, float32 thick)
        {
            mDebugLines.PushBack({a, b, c, thick});
        }

        void NkRender3D::DrawDebugAABB(const NkAABB& aabb, const NkColorF& c) {
            const NkVec3f& mn = aabb.min;
            const NkVec3f& mx = aabb.max;
            // 12 arêtes du cube
            DrawDebugLine({mn.x,mn.y,mn.z},{mx.x,mn.y,mn.z},c);
            DrawDebugLine({mx.x,mn.y,mn.z},{mx.x,mx.y,mn.z},c);
            DrawDebugLine({mx.x,mx.y,mn.z},{mn.x,mx.y,mn.z},c);
            DrawDebugLine({mn.x,mx.y,mn.z},{mn.x,mn.y,mn.z},c);
            DrawDebugLine({mn.x,mn.y,mx.z},{mx.x,mn.y,mx.z},c);
            DrawDebugLine({mx.x,mn.y,mx.z},{mx.x,mx.y,mx.z},c);
            DrawDebugLine({mx.x,mx.y,mx.z},{mn.x,mx.y,mx.z},c);
            DrawDebugLine({mn.x,mx.y,mx.z},{mn.x,mn.y,mx.z},c);
            DrawDebugLine({mn.x,mn.y,mn.z},{mn.x,mn.y,mx.z},c);
            DrawDebugLine({mx.x,mn.y,mn.z},{mx.x,mn.y,mx.z},c);
            DrawDebugLine({mx.x,mx.y,mn.z},{mx.x,mx.y,mx.z},c);
            DrawDebugLine({mn.x,mx.y,mn.z},{mn.x,mx.y,mx.z},c);
        }

        void NkRender3D::DrawDebugAxes(const NkMat4f& t, float32 scale) {
            NkVec3f o = {t[3][0],t[3][1],t[3][2]};
            NkVec3f x = {o.x+t[0][0]*scale, o.y+t[0][1]*scale, o.z+t[0][2]*scale};
            NkVec3f y = {o.x+t[1][0]*scale, o.y+t[1][1]*scale, o.z+t[1][2]*scale};
            NkVec3f z = {o.x+t[2][0]*scale, o.y+t[2][1]*scale, o.z+t[2][2]*scale};
            DrawDebugLine(o, x, {1,0,0,1});
            DrawDebugLine(o, y, {0,1,0,1});
            DrawDebugLine(o, z, {0,0,1,1});
        }

        // =============================================================================
        void NkRender3D::EndScene(NkICommandBuffer* cmd) {
            NK_ASSERT(mSceneOpen, "EndScene sans BeginScene");
            mSceneOpen = false;

            // 1. Frustum cull
            NkMat4f vp = mCurrentScene.camera.Projection() * mCurrentScene.camera.View();
            FrustumCull(vp);

            // 2. Tri front-to-back (opaques) et back-to-front (transparents)
            SortDrawCalls();

            // 3. Mise à jour UBO scène
            UpdateSceneUBO();

            // 4. Passes shadow CSM
            if (mShadowsEnabled && !mCurrentScene.lights.IsEmpty()) {
                for (uint32 i = 0; i < (uint32)mCurrentScene.lights.Size(); ++i) {
                    if (mCurrentScene.lights[i].type == NkLightType::NK_DIRECTIONAL) {
                        ComputeShadowCSM(mCurrentScene.camera,
                                          mCurrentScene.lights[i], 4);
                        break;
                    }
                }
                RenderShadowPasses(cmd);
            }

            // 5. Géométrie principale
            RenderGeometry(cmd, false);

            // 6. Géométrie instanciée
            if (mInstancingEnabled) {
                for (uint32 i = 0; i < (uint32)mInstancedCalls.Size(); ++i) {
                    const NkDrawCallInstanced& dc = mInstancedCalls[i];
                    if (dc.transforms.IsEmpty()) continue;

                    // Upload des matrices d'instance dans un SSBO dynamique
                    uint64 instSz = (uint64)dc.transforms.Size() * sizeof(NkMat4f);
                    NkBufferHandle instBuf = mDevice->CreateBuffer(
                        NkBufferDesc::StorageDynamic(instSz));
                    if (instBuf.IsValid()) {
                        mDevice->WriteBuffer(instBuf, dc.transforms.Data(), instSz);
                        NkBufferHandle vbo; vbo.id = mResources->GetMeshVBORHIId(dc.mesh);
                        NkBufferHandle ibo; ibo.id = mResources->GetMeshIBORHIId(dc.mesh);
                        uint32 idxCount   = mResources->GetMeshIndexCount(dc.mesh);
                        uint32 instCount  = (uint32)dc.transforms.Size();
                        if (vbo.IsValid()) {
                            cmd->BindVertexBuffer(0, vbo);
                            if (ibo.IsValid()) {
                                cmd->BindIndexBuffer(ibo, NkIndexFormat::NK_UINT32);
                                cmd->DrawIndexed(idxCount, instCount, 0, 0, 0);
                            } else {
                                cmd->Draw(mResources->GetMeshVertexCount(dc.mesh),
                                           instCount, 0, 0);
                            }
                            mStats.instancedCalls++;
                            mStats.triangles += (idxCount / 3) * instCount;
                        }
                        mDevice->DestroyBuffer(instBuf);
                    }
                }
            }

            // 7. Transparents (back to front, additive ou alpha blend)
            RenderGeometry(cmd, false); // mAlphaCalls seulement (séparé dans impl)

            // 8. Debug
            if (mDebugEnabled) {
                RenderDebugGeometry(cmd);
            }
        }

        // =============================================================================
        void NkRender3D::FrustumCull(const NkMat4f& viewProj) {
            // Extraction des 6 plans du frustum (Gribb & Hartmann)
            NkVec4f planes[6];
            for (int i = 0; i < 4; ++i) planes[0].arr[i] = viewProj[i][3]+viewProj[i][0]; // Left
            for (int i = 0; i < 4; ++i) planes[1].arr[i] = viewProj[i][3]-viewProj[i][0]; // Right
            for (int i = 0; i < 4; ++i) planes[2].arr[i] = viewProj[i][3]+viewProj[i][1]; // Bottom
            for (int i = 0; i < 4; ++i) planes[3].arr[i] = viewProj[i][3]-viewProj[i][1]; // Top
            for (int i = 0; i < 4; ++i) planes[4].arr[i] = viewProj[i][3]+viewProj[i][2]; // Near
            for (int i = 0; i < 4; ++i) planes[5].arr[i] = viewProj[i][3]-viewProj[i][2]; // Far

            // Normaliser
            for (int p = 0; p < 6; ++p) {
                float len = sqrtf(planes[p].x*planes[p].x+planes[p].y*planes[p].y+planes[p].z*planes[p].z);
                if (len > 0.f) { planes[p].x/=len; planes[p].y/=len; planes[p].z/=len; planes[p].w/=len; }
            }

            auto testAABB = [&](const NkAABB& aabb) -> bool {
                NkVec3f center = {(aabb.min.x+aabb.max.x)*0.5f,
                                   (aabb.min.y+aabb.max.y)*0.5f,
                                   (aabb.min.z+aabb.max.z)*0.5f};
                NkVec3f ext    = {(aabb.max.x-aabb.min.x)*0.5f,
                                   (aabb.max.y-aabb.min.y)*0.5f,
                                   (aabb.max.z-aabb.min.z)*0.5f};
                for (int p = 0; p < 6; ++p) {
                    float d = center.x*planes[p].x + center.y*planes[p].y + center.z*planes[p].z + planes[p].w;
                    float r = ext.x*fabsf(planes[p].x) + ext.y*fabsf(planes[p].y) + ext.z*fabsf(planes[p].z);
                    if (d + r < 0.f) return false;
                }
                return true;
            };

            for (uint32 i = 0; i < (uint32)mOpaqueCalls.Size(); ++i) {
                if (!testAABB(mOpaqueCalls[i].aabb)) {
                    mOpaqueCalls[i].visible = false;
                    mStats.culledObjects++;
                }
            }
            for (uint32 i = 0; i < (uint32)mAlphaCalls.Size(); ++i) {
                if (!testAABB(mAlphaCalls[i].aabb)) {
                    mAlphaCalls[i].visible = false;
                    mStats.culledObjects++;
                }
            }
        }

        // =============================================================================
        void NkRender3D::SortDrawCalls() {
            // Tri opaques : front-to-back (minimise overdraw)
            NkVec3f eye = mCurrentScene.camera.GetPosition();
            auto sortFTB = [&](const NkDrawCall3D& a, const NkDrawCall3D& b) {
                NkVec3f ca = {(a.aabb.min.x+a.aabb.max.x)*0.5f,
                               (a.aabb.min.y+a.aabb.max.y)*0.5f,
                               (a.aabb.min.z+a.aabb.max.z)*0.5f};
                NkVec3f cb = {(b.aabb.min.x+b.aabb.max.x)*0.5f,
                               (b.aabb.min.y+b.aabb.max.y)*0.5f,
                               (b.aabb.min.z+b.aabb.max.z)*0.5f};
                float da = (ca.x-eye.x)*(ca.x-eye.x)+(ca.y-eye.y)*(ca.y-eye.y)+(ca.z-eye.z)*(ca.z-eye.z);
                float db = (cb.x-eye.x)*(cb.x-eye.x)+(cb.y-eye.y)*(cb.y-eye.y)+(cb.z-eye.z)*(cb.z-eye.z);
                // Trier aussi par sortKey (matériau) pour minimiser les state changes
                if (a.sortKey != b.sortKey) return a.sortKey < b.sortKey;
                return da < db;
            };
            std::sort(mOpaqueCalls.begin(), mOpaqueCalls.end(), sortFTB);

            // Tri transparents : back-to-front
            auto sortBTF = [&](const NkDrawCall3D& a, const NkDrawCall3D& b) {
                NkVec3f ca = {(a.aabb.min.x+a.aabb.max.x)*0.5f,
                               (a.aabb.min.y+a.aabb.max.y)*0.5f,
                               (a.aabb.min.z+a.aabb.max.z)*0.5f};
                NkVec3f cb = {(b.aabb.min.x+b.aabb.max.x)*0.5f,
                               (b.aabb.min.y+b.aabb.max.y)*0.5f,
                               (b.aabb.min.z+b.aabb.max.z)*0.5f};
                float da = (ca.x-eye.x)*(ca.x-eye.x)+(ca.y-eye.y)*(ca.y-eye.y)+(ca.z-eye.z)*(ca.z-eye.z);
                float db = (cb.x-eye.x)*(cb.x-eye.x)+(cb.y-eye.y)*(cb.y-eye.y)+(cb.z-eye.z)*(cb.z-eye.z);
                return da > db;
            };
            std::sort(mAlphaCalls.begin(), mAlphaCalls.end(), sortBTF);
        }

        // =============================================================================
        void NkRender3D::UpdateSceneUBO() {
            if (!mSceneUBORHI) return;
            struct SceneBlock {
                NkMat4f model,view,proj,lightVP;
                NkVec4f lightDir,eyePos;
                float   time,dt,ndcZS,ndcZO;
            } sb{};
            sb.model   = NkMat4f::Identity();
            sb.view    = mCurrentScene.camera.View();
            sb.proj    = mCurrentScene.camera.Projection();
            sb.lightVP = mShadowVP.IsEmpty() ? NkMat4f::Identity() : mShadowVP[0];
            if (!mCurrentScene.lights.IsEmpty()) {
                const auto& l = mCurrentScene.lights[0];
                sb.lightDir = {l.direction.x,l.direction.y,l.direction.z,0};
            }
            NkVec3f ep = mCurrentScene.camera.GetPosition();
            sb.eyePos  = {ep.x,ep.y,ep.z,1};
            sb.time    = mCurrentScene.time;
            sb.dt      = mCurrentScene.deltaTime;
            sb.ndcZS   = mDevice->GetNDCZScale();
            sb.ndcZO   = mDevice->GetNDCZOffset();
            NkBufferHandle h; h.id=mSceneUBORHI;
            mDevice->WriteBuffer(h, &sb, sizeof(sb));
        }

        // =============================================================================
        void NkRender3D::ComputeShadowCSM(const NkCamera3D& cam,
                                            const NkLightDesc& sun,
                                            uint32 numCascades)
        {
            mShadowVP.Clear();
            float nearP = cam.GetNear(), farP = cam.GetFar();
            float lambda = 0.75f; // blend pratique vs logarithmique

            for (uint32 c = 0; c < numCascades; ++c) {
                float iN = (float)c     / numCascades;
                float iF = (float)(c+1) / numCascades;
                float splitLog = nearP * powf(farP/nearP, iF);
                float splitLin = nearP + (farP-nearP)*iF;
                float zNear = (c == 0) ? nearP
                    : nearP + (farP-nearP)*(lambda*((float)c/numCascades));
                float zFar  = nearP + (farP-nearP)*(lambda*iF+(1-lambda)*iF);

                // Frustum corners dans world-space
                NkMat4f invVP = (cam.Projection()*cam.View()).Inverse();
                NkVec4f corners[8];
                int idx=0;
                for (int x=-1;x<=1;x+=2) for (int y=-1;y<=1;y+=2) for (int z=0;z<=1;z++) {
                    NkVec4f pt = invVP * NkVec4f{(float)x,(float)y,z==0?0.f:1.f,1.f};
                    corners[idx++] = pt / pt.w;
                }

                // Centre du frustum
                NkVec3f center = {0,0,0};
                for (int i=0;i<8;++i) { center.x+=corners[i].x; center.y+=corners[i].y; center.z+=corners[i].z; }
                center.x/=8; center.y/=8; center.z/=8;

                // Rayon englobant
                float radius = 0.f;
                for (int i=0;i<8;++i) {
                    float dx=corners[i].x-center.x, dy=corners[i].y-center.y, dz=corners[i].z-center.z;
                    radius = std::max(radius, sqrtf(dx*dx+dy*dy+dz*dz));
                }
                radius = ceilf(radius * 16.f) / 16.f;

                NkVec3f ld = {sun.direction.x,sun.direction.y,sun.direction.z};
                NkVec3f lpos = {center.x - ld.x*radius*2, center.y - ld.y*radius*2, center.z - ld.z*radius*2};
                NkMat4f lightView = NkMat4f::LookAt(lpos, center, {0,1,0});
                NkMat4f lightProj = NkMat4f::Orthographic(-radius, radius, -radius, radius, 0.f, radius*4.f);
                mShadowVP.PushBack(lightProj * lightView);
            }
        }

        // =============================================================================
        void NkRender3D::RenderShadowPasses(NkICommandBuffer* cmd) {
            if (mShadowMaps.IsEmpty()) return;
            uint32 numCascades = std::min((uint32)mShadowMaps.Size(),
                                           (uint32)mShadowVP.Size());

            for (uint32 c = 0; c < numCascades; ++c) {
                NkRenderTargetHandle rt = mShadowMaps[c];
                cmd->BeginRenderTarget(rt, NkClearFlags::NK_DEPTH, {}, 1.f, 0);
                cmd->SetViewport({0,0,(float)mShadowMapSize,(float)mShadowMapSize});
                cmd->SetScissor ({0,0,(int32)mShadowMapSize,(int32)mShadowMapSize});

                // Update LightVP push constant
                struct LightPC { NkMat4f lightVP; NkMat4f model; };
                LightPC lpc; lpc.lightVP = mShadowVP[c];

                for (uint32 i = 0; i < (uint32)mOpaqueCalls.Size(); ++i) {
                    const NkDrawCall3D& dc = mOpaqueCalls[i];
                    if (!dc.visible || !dc.castShadow) continue;
                    lpc.model = dc.transform;
                    cmd->PushConstants(&lpc, sizeof(LightPC));
                    NkBufferHandle vbo; vbo.id = mResources->GetMeshVBORHIId(dc.mesh);
                    NkBufferHandle ibo; ibo.id = mResources->GetMeshIBORHIId(dc.mesh);
                    if (!vbo.IsValid()) continue;
                    cmd->BindVertexBuffer(0, vbo);
                    if (ibo.IsValid()) {
                        cmd->BindIndexBuffer(ibo, NkIndexFormat::NK_UINT32);
                        cmd->DrawIndexed(mResources->GetMeshIndexCount(dc.mesh),1,0,0,0);
                    }
                    mStats.shadowCasters++;
                }
                cmd->EndRenderTarget();
            }
        }

        // =============================================================================
        void NkRender3D::RenderGeometry(NkICommandBuffer* cmd, bool shadowPass) {
            auto renderList = [&](NkVector<NkDrawCall3D>& calls) {
                uint64 lastPipeline = 0;
                uint64 lastDescSet  = 0;

                for (uint32 i = 0; i < (uint32)calls.Size(); ++i) {
                    const NkDrawCall3D& dc = calls[i];
                    if (!dc.visible) continue;

                    // Pipeline (matériau template)
                    NkMaterialInstHandle instH = dc.material;
                    NkMaterialHandle tmplH = mMaterials->GetTemplate(instH);
                    uint64 pipe = mMaterials->GetPipelineRHIId(tmplH);
                    if (pipe && pipe != lastPipeline) {
                        NkPipelineHandle ph; ph.id=pipe;
                        cmd->BindGraphicsPipeline(ph);
                        lastPipeline = pipe;
                    }

                    // Descriptor set (textures/UBOs du matériau)
                    uint64 ds = mMaterials->GetDescSetRHIId(instH);
                    if (ds && ds != lastDescSet) {
                        NkDescSetHandle dsh; dsh.id=ds;
                        cmd->BindDescriptorSet(dsh, 1);
                        lastDescSet = ds;
                    }

                    // Push constant : matrice model + tint
                    struct ObjPC { NkMat4f model; NkVec4f tint; };
                    ObjPC pc; pc.model=dc.transform;
                    pc.tint = {dc.tint.r,dc.tint.g,dc.tint.b,dc.tint.a};
                    cmd->PushConstants(&pc, sizeof(ObjPC));

                    // Draw
                    NkBufferHandle vbo; vbo.id = mResources->GetMeshVBORHIId(dc.mesh);
                    NkBufferHandle ibo; ibo.id = mResources->GetMeshIBORHIId(dc.mesh);
                    if (!vbo.IsValid()) continue;
                    cmd->BindVertexBuffer(0, vbo);
                    if (ibo.IsValid()) {
                        cmd->BindIndexBuffer(ibo, NkIndexFormat::NK_UINT32);
                        uint32 idxCount = mResources->GetMeshIndexCount(dc.mesh);
                        cmd->DrawIndexed(idxCount, 1, 0, 0, 0);
                        mStats.triangles += idxCount / 3;
                    } else {
                        uint32 vtxCount = mResources->GetMeshVertexCount(dc.mesh);
                        cmd->Draw(vtxCount, 1, 0, 0);
                        mStats.triangles += vtxCount / 3;
                    }
                    mStats.drawCalls++;
                }
            };

            renderList(mOpaqueCalls);
            renderList(mAlphaCalls);
        }

        // =============================================================================
        void NkRender3D::RenderDebugGeometry(NkICommandBuffer* cmd) {
            if (mDebugLines.IsEmpty() || !mDebugVBORHI) return;

            // Construire le buffer de debug
            NkVector<NkVertexDebug> verts;
            verts.Reserve(mDebugLines.Size() * 2);
            for (uint32 i = 0; i < (uint32)mDebugLines.Size(); ++i) {
                const DebugLine& l = mDebugLines[i];
                verts.PushBack({l.a.x,l.a.y,l.a.z, l.c.r,l.c.g,l.c.b,l.c.a});
                verts.PushBack({l.b.x,l.b.y,l.b.z, l.c.r,l.c.g,l.c.b,l.c.a});
            }

            NkBufferHandle vbo; vbo.id = mDebugVBORHI;
            mDevice->WriteBuffer(vbo, verts.Data(), (uint64)verts.Size()*sizeof(NkVertexDebug));

            // Pipeline debug (lazy)
            if (!mDebugPipeRHI && mDebugShaderRHI) {
                NkGraphicsPipelineDesc pd{};
                NkShaderHandle sh; sh.id=mDebugShaderRHI; pd.shader=sh;
                pd.topology     = NkPrimitiveTopology::NK_LINE_LIST;
                pd.rasterizer   = NkRasterizerDesc::NoCull();
                pd.depthStencil = NkDepthStencilDesc::ReadOnly();
                pd.blend        = NkBlendDesc::Opaque();
                pd.vertexLayout = NkVertexDebug::Layout();
                pd.renderPass   = mDevice->GetSwapchainRenderPass();
                NkPipelineHandle ph = mDevice->CreateGraphicsPipeline(pd);
                mDebugPipeRHI = ph.id;
            }

            if (mDebugPipeRHI) {
                NkPipelineHandle ph; ph.id=mDebugPipeRHI;
                cmd->BindGraphicsPipeline(ph);
                cmd->BindVertexBuffer(0, vbo);
                cmd->Draw((uint32)verts.Size(), 1, 0, 0);
                mStats.debugLines = (uint32)mDebugLines.Size();
            }
        }

    } // namespace renderer
} // namespace nkentseu
