#include "NkRenderer3D.h"
#include "NKLogger/NkLog.h"

#include "NKMath/NKMath.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::math;

namespace nkentseu {
    namespace renderer {

        namespace {
            struct NkSceneUBOData {
                NkMat4f view;
                NkMat4f proj;
                NkMat4f viewProj;
                NkVec4f cameraPos;
                NkVec4f ambient;
            };

            static NkVec3f V3(float x, float y, float z) {
                return {x, y, z};
            }

            static NkVec3f V3Add(const NkVec3f& a, const NkVec3f& b) {
                return {a.x + b.x, a.y + b.y, a.z + b.z};
            }

            static NkVec3f V3Sub(const NkVec3f& a, const NkVec3f& b) {
                return {a.x - b.x, a.y - b.y, a.z - b.z};
            }

            static NkVec3f V3Mul(const NkVec3f& a, float s) {
                return {a.x * s, a.y * s, a.z * s};
            }

            static float V3Dot(const NkVec3f& a, const NkVec3f& b) {
                return a.x * b.x + a.y * b.y + a.z * b.z;
            }

            static NkVec3f V3Cross(const NkVec3f& a, const NkVec3f& b) {
                return {
                    a.y * b.z - a.z * b.y,
                    a.z * b.x - a.x * b.z,
                    a.x * b.y - a.y * b.x
                };
            }

            static float V3Len(const NkVec3f& v) {
                return NkSqrt(v.x * v.x + v.y * v.y + v.z * v.z);
            }

            static NkVec3f V3Norm(const NkVec3f& v, const NkVec3f& fallback = {0.f, 1.f, 0.f}) {
                const float l = V3Len(v);
                if (l <= 1e-6f) return fallback;
                const float inv = 1.0f / l;
                return {v.x * inv, v.y * inv, v.z * inv};
            }

            static NkVec3f TransformPoint(const NkMat4f& m, const NkVec3f& p) {
                return {
                    m.data[0] * p.x + m.data[4] * p.y + m.data[8] * p.z + m.data[12],
                    m.data[1] * p.x + m.data[5] * p.y + m.data[9] * p.z + m.data[13],
                    m.data[2] * p.x + m.data[6] * p.y + m.data[10] * p.z + m.data[14]
                };
            }

            static float MaxAxisScale(const NkMat4f& m) {
                const float sx = NkSqrt(m.data[0] * m.data[0] + m.data[1] * m.data[1] + m.data[2] * m.data[2]);
                const float sy = NkSqrt(m.data[4] * m.data[4] + m.data[5] * m.data[5] + m.data[6] * m.data[6]);
                const float sz = NkSqrt(m.data[8] * m.data[8] + m.data[9] * m.data[9] + m.data[10] * m.data[10]);
                return NkMax(sx, NkMax(sy, sz));
            }

            static NkVec3f ExtractTranslation(const NkMat4f& m) {
                return {m.data[12], m.data[13], m.data[14]};
            }

            static bool IsTransparentMaterial(const NkMaterial* m) {
                if (!m) return false;
                const NkBlendMode b = m->GetBlendMode();
                return b == NkBlendMode::NK_TRANSLUCENT
                    || b == NkBlendMode::NK_ADDITIVE
                    || b == NkBlendMode::NK_SCREEN
                    || b == NkBlendMode::NK_PREMULTIPLIED;
            }

            static NkAABB TransformAABB(const NkAABB& local, const NkMat4f& m) {
                const NkVec3f mn = local.min;
                const NkVec3f mx = local.max;

                const NkVec3f corners[8] = {
                    {mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z},
                    {mn.x, mx.y, mn.z}, {mx.x, mx.y, mn.z},
                    {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z},
                    {mn.x, mx.y, mx.z}, {mx.x, mx.y, mx.z}
                };

                NkAABB out{};
                out.min = TransformPoint(m, corners[0]);
                out.max = out.min;
                for (int i = 1; i < 8; ++i) {
                    const NkVec3f p = TransformPoint(m, corners[i]);
                    out.min.x = NkMin(out.min.x, p.x);
                    out.min.y = NkMin(out.min.y, p.y);
                    out.min.z = NkMin(out.min.z, p.z);
                    out.max.x = NkMax(out.max.x, p.x);
                    out.max.y = NkMax(out.max.y, p.y);
                    out.max.z = NkMax(out.max.z, p.z);
                }
                return out;
            }

            static bool RayAABB(const NkVec3f& ro, const NkVec3f& rd, const NkAABB& b, float& outT) {
                const float invX = (NkFabs(rd.x) > 1e-8f) ? 1.0f / rd.x : 1e30f;
                const float invY = (NkFabs(rd.y) > 1e-8f) ? 1.0f / rd.y : 1e30f;
                const float invZ = (NkFabs(rd.z) > 1e-8f) ? 1.0f / rd.z : 1e30f;

                float t1 = (b.min.x - ro.x) * invX;
                float t2 = (b.max.x - ro.x) * invX;
                float tmin = NkMin(t1, t2);
                float tmax = NkMax(t1, t2);

                t1 = (b.min.y - ro.y) * invY;
                t2 = (b.max.y - ro.y) * invY;
                tmin = NkMax(tmin, NkMin(t1, t2));
                tmax = NkMin(tmax, NkMax(t1, t2));

                t1 = (b.min.z - ro.z) * invZ;
                t2 = (b.max.z - ro.z) * invZ;
                tmin = NkMax(tmin, NkMin(t1, t2));
                tmax = NkMin(tmax, NkMax(t1, t2));

                if (tmax < 0.0f || tmin > tmax) {
                    return false;
                }
                outT = (tmin >= 0.0f) ? tmin : tmax;
                return outT >= 0.0f;
            }

        } // namespace

        void NkRenderScene::SubmitInstanced(NkStaticMesh* mesh, NkMaterial* mat,
                                            const NkMat4f* transforms, uint32 count) {
            if (!mesh || !transforms || count == 0) return;
            for (uint32 i = 0; i < count; ++i) {
                Submit(mesh, mat, transforms[i], true);
            }
        }

        void NkRenderScene::Sort() {
            const NkVec3f camPos = mCamera.position;

            std::stable_sort(mDrawCalls.Begin(), mDrawCalls.End(),
                [&](const NkDrawCall& a, const NkDrawCall& b) {
                    const bool ta = IsTransparentMaterial(a.material);
                    const bool tb = IsTransparentMaterial(b.material);
                    if (ta != tb) return !ta && tb;

                    const NkVec3f pa = ExtractTranslation(a.transform);
                    const NkVec3f pb = ExtractTranslation(b.transform);
                    const NkVec3f da = V3Sub(pa, camPos);
                    const NkVec3f db = V3Sub(pb, camPos);
                    const float d2a = V3Dot(da, da);
                    const float d2b = V3Dot(db, db);

                    if (!ta) {
                        if (d2a != d2b) return d2a < d2b;
                    } else {
                        if (d2a != d2b) return d2a > d2b;
                    }

                    if (a.material != b.material) return a.material < b.material;
                    if (a.mesh != b.mesh) return a.mesh < b.mesh;
                    return a.objectID < b.objectID;
                });
        }

        void NkRenderScene::FrustumCull(const NkCamera& cam, float aspect) {
            const NkVec3f forward = V3Norm(V3Sub(cam.target, cam.position), {0.f, 0.f, -1.f});
            const NkVec3f right = V3Norm(V3Cross(forward, cam.up), {1.f, 0.f, 0.f});
            const NkVec3f up = V3Norm(V3Cross(right, forward), {0.f, 1.f, 0.f});
            const float nearZ = NkMax(1e-4f, cam.nearPlane);
            const float farZ = NkMax(nearZ + 1e-3f, cam.farPlane);
            float tanHalf = 0.0f;
            if (!cam.isOrtho) {
                const float clampedFov = NkClamp(cam.fovDeg, 1.0f, 179.0f);
                tanHalf = NkFabs(NkTan(NkToRadians(clampedFov) * 0.5f));
                if (!std::isfinite(tanHalf) || tanHalf < 1e-5f) {
                    tanHalf = NkTan(NkToRadians(60.0f) * 0.5f);
                }
            }

            for (auto& dc : mDrawCalls) {
                if (!dc.visible) continue;
                if (!dc.mesh && !dc.dynMesh) continue;

                NkVec3f center = ExtractTranslation(dc.transform);
                float radius = 0.75f;
                if (dc.mesh) {
                    center = TransformPoint(dc.transform, dc.mesh->GetAABB().Center());
                    radius = dc.mesh->GetAABB().Radius() * NkMax(0.01f, MaxAxisScale(dc.transform));
                }

                const NkVec3f to = V3Sub(center, cam.position);
                const float z = V3Dot(to, forward);
                if (z + radius < nearZ || z - radius > farZ) {
                    dc.visible = false;
                    continue;
                }

                const float x = V3Dot(to, right);
                const float y = V3Dot(to, up);

                if (cam.isOrtho) {
                    const float halfV = NkMax(0.01f, cam.orthoSize);
                    const float halfH = halfV * NkMax(0.01f, aspect);
                    if (NkFabs(x) > halfH + radius || NkFabs(y) > halfV + radius) {
                        dc.visible = false;
                    }
                } else {
                    const float halfV = z * tanHalf;
                    const float halfH = halfV * NkMax(0.01f, aspect);
                    if (NkFabs(x) > halfH + radius || NkFabs(y) > halfV + radius) {
                        dc.visible = false;
                    }
                }
            }
        }

        bool NkRenderer3D::Init(NkIDevice* device, uint32 width, uint32 height) {
            Shutdown();
            if (!device || !device->IsValid() || width == 0 || height == 0) {
                return false;
            }

            mDevice = device;
            mWidth = width;
            mHeight = height;
            mActiveRenderPass = {};
            mPipelineRenderPass = {};

            if (!InitBuffers()) {
                Shutdown();
                return false;
            }
            if (!InitDescriptors()) {
                Shutdown();
                return false;
            }
            if (!InitShaders()) {
                Shutdown();
                return false;
            }
            if (!InitRenderTargets(width, height)) {
                Shutdown();
                return false;
            }

            mRenderMode = NkRenderMode::NK_SOLID;
            mIsValid = true;
            return true;
        }

        void NkRenderer3D::Shutdown() {
            if (!mDevice) {
                mIsValid = false;
                return;
            }

            if (mVertexPointPipeline.IsValid()) mDevice->DestroyPipeline(mVertexPointPipeline);
            if (mPickingPipeline.IsValid()) mDevice->DestroyPipeline(mPickingPipeline);
            if (mFXAAPipeline.IsValid()) mDevice->DestroyPipeline(mFXAAPipeline);
            if (mSSAOPipeline.IsValid()) mDevice->DestroyPipeline(mSSAOPipeline);
            if (mBloomUpPipeline.IsValid()) mDevice->DestroyPipeline(mBloomUpPipeline);
            if (mBloomDownPipeline.IsValid()) mDevice->DestroyPipeline(mBloomDownPipeline);
            if (mTonemapPipeline.IsValid()) mDevice->DestroyPipeline(mTonemapPipeline);
            if (mNormalVisPipeline.IsValid()) mDevice->DestroyPipeline(mNormalVisPipeline);
            if (mWireframePipeline.IsValid()) mDevice->DestroyPipeline(mWireframePipeline);
            if (mSkyboxPipeline.IsValid()) mDevice->DestroyPipeline(mSkyboxPipeline);

            if (mShadowDescSet.IsValid()) mDevice->FreeDescriptorSet(mShadowDescSet);
            if (mIBLDescSet.IsValid()) mDevice->FreeDescriptorSet(mIBLDescSet);
            if (mSceneDescSet.IsValid()) mDevice->FreeDescriptorSet(mSceneDescSet);

            if (mShadowLayout.IsValid()) mDevice->DestroyDescriptorSetLayout(mShadowLayout);
            if (mIBLLayout.IsValid()) mDevice->DestroyDescriptorSetLayout(mIBLLayout);
            if (mSceneLayout.IsValid()) mDevice->DestroyDescriptorSetLayout(mSceneLayout);

            if (mFullscreenVBO.IsValid()) mDevice->DestroyBuffer(mFullscreenVBO);
            if (mSkyboxVBO.IsValid()) mDevice->DestroyBuffer(mSkyboxVBO);
            if (mShadowUBO.IsValid()) mDevice->DestroyBuffer(mShadowUBO);
            if (mLightSSBO.IsValid()) mDevice->DestroyBuffer(mLightSSBO);
            if (mSceneUBO.IsValid()) mDevice->DestroyBuffer(mSceneUBO);

            mHDRTarget.Destroy();
            mShadowMapRT.Destroy();
            mSSAOTarget.Destroy();
            mPickingTarget.Destroy();
            for (auto& rt : mPostTarget) rt.Destroy();
            for (auto& rt : mBloomTarget) rt.Destroy();
            mGBuffer.rt = nullptr;

            mPBRShader.Destroy();
            mUnlitShader.Destroy();
            mShadowShader.Destroy();
            mSkyboxShader.Destroy();
            mWireframeShader.Destroy();
            mNormalVisShader.Destroy();
            mTonemapShader.Destroy();
            mBloomShader.Destroy();
            mSSAOShader.Destroy();
            mFXAAShader.Destroy();
            mPickingShader.Destroy();
            for (auto& s : mIBLShaders) s.Destroy();

            mSkyboxPipeline = {};
            mWireframePipeline = {};
            mNormalVisPipeline = {};
            mTonemapPipeline = {};
            mBloomDownPipeline = {};
            mBloomUpPipeline = {};
            mSSAOPipeline = {};
            mFXAAPipeline = {};
            mPickingPipeline = {};
            mVertexPointPipeline = {};

            mSceneLayout = {};
            mSceneDescSet = {};
            mIBLLayout = {};
            mIBLDescSet = {};
            mShadowLayout = {};
            mShadowDescSet = {};

            mSceneUBO = {};
            mLightSSBO = {};
            mShadowUBO = {};
            mSkyboxVBO = {};
            mFullscreenVBO = {};

            mActiveRenderPass = {};
            mPipelineRenderPass = {};
            mDevice = nullptr;
            mWidth = 0;
            mHeight = 0;
            mIsValid = false;
            ResetStats();
        }

        void NkRenderer3D::Resize(uint32 width, uint32 height) {
            if (!mIsValid || width == 0 || height == 0) return;
            if (mWidth == width && mHeight == height) return;

            mWidth = width;
            mHeight = height;
            InitRenderTargets(width, height);
        }

        void NkRenderer3D::SetSkybox(NkTextureCube* skybox, float exposure) {
            mSkyboxTex = skybox;
            mSkyExposure = NkMax(0.0f, exposure);
            mIBLDirty = true;
        }

        void NkRenderer3D::SetIBL(NkTextureCube* irradiance, NkTextureCube* prefilter,
                                  NkTexture2D* brdfLUT) {
            mIrradianceMap = irradiance;
            mPrefilterMap = prefilter;
            mBRDF_LUT = brdfLUT;
            mIBLDirty = false;
        }

        void NkRenderer3D::SetEnvironmentFromHDRI(const char* hdriPath) {
            if (!mDevice || !hdriPath || !hdriPath[0]) return;

            NkTextureCube* env = new NkTextureCube();
            if (!env->LoadFromEquirectangular(mDevice, hdriPath, 512)) {
                delete env;
                logger_src.Infof("[NkRenderer3D] Failed to load HDRI: %s\n", hdriPath);
                return;
            }

            SetSkybox(env, 1.0f);
            SetIBL(GenerateIrradianceMap(env, 32),
                   GeneratePrefilterMap(env, 128),
                   GenerateBRDF_LUT(256));
        }

        void NkRenderer3D::Render(NkICommandBuffer* cmd, NkRenderScene& scene) {
            if (!mIsValid || !cmd || !mDevice) return;

            mActiveRenderPass = mDevice->GetSwapchainRenderPass();
            const NkFramebufferHandle fb = mDevice->GetSwapchainFramebuffer();
            if (!mActiveRenderPass.IsValid() || !fb.IsValid()) return;

            if (!InitPipelines()) return;

            ResetStats();
            UpdateSceneUBO(scene);
            UpdateLightSSBO(scene);

            NkRect2D area{0, 0, (int32)mWidth, (int32)mHeight};
            if (!cmd->BeginRenderPass(mActiveRenderPass, fb, area)) {
                return;
            }

            NkViewport vp{};
            vp.x = 0.0f;
            vp.y = 0.0f;
            vp.width = (float)mWidth;
            vp.height = (float)mHeight;
            vp.minDepth = 0.0f;
            vp.maxDepth = 1.0f;
            vp.flipY = true;
            cmd->SetViewport(vp);
            cmd->SetScissor({0, 0, (int32)mWidth, (int32)mHeight});

            PassForward(cmd, scene);
            PassSkybox(cmd, scene.GetSky());

            cmd->EndRenderPass();
        }

        void NkRenderer3D::RenderTo(NkICommandBuffer* cmd, NkRenderScene& scene, NkRenderTarget* target) {
            if (!mIsValid || !cmd || !target || !target->IsValid()) return;

            mActiveRenderPass = target->GetRenderPass();
            if (!mActiveRenderPass.IsValid()) return;

            if (!InitPipelines()) return;

            ResetStats();
            UpdateSceneUBO(scene);
            UpdateLightSSBO(scene);

            const uint32 w = target->GetWidth();
            const uint32 h = target->GetHeight();
            NkRect2D area{0, 0, (int32)w, (int32)h};
            if (!cmd->BeginRenderPass(target->GetRenderPass(), target->GetFramebuffer(), area)) {
                return;
            }

            NkViewport vp{};
            vp.x = 0.0f;
            vp.y = 0.0f;
            vp.width = (float)w;
            vp.height = (float)h;
            vp.minDepth = 0.0f;
            vp.maxDepth = 1.0f;
            vp.flipY = true;
            cmd->SetViewport(vp);
            cmd->SetScissor({0, 0, (int32)w, (int32)h});

            PassForward(cmd, scene);
            PassSkybox(cmd, scene.GetSky());

            cmd->EndRenderPass();
        }

        void NkRenderer3D::RenderShadowPass(NkICommandBuffer* cmd, NkRenderScene& scene,
                                            const NkLight& light, uint32 cascadeIdx) {
            (void)light;
            (void)cascadeIdx;
            PassShadow(cmd, scene);
        }

        void NkRenderer3D::RenderEditOverlay(NkICommandBuffer* cmd, NkEditableMesh* mesh,
                                             const NkMat4f& transform,
                                             bool showVerts, bool showEdges, bool showFaces,
                                             NkColor4f vertColor, NkColor4f edgeColor,
                                             NkColor4f selectColor) {
            (void)vertColor;
            (void)edgeColor;
            (void)selectColor;
            if (!cmd || !mesh || !mDevice) return;

            NkMeshData md = mesh->ToMeshData();
            if (md.vertices.Empty()) return;
            NkStaticMesh tmp;
            if (!tmp.Create(mDevice, md)) return;

            NkDrawCall dc{};
            dc.mesh = &tmp;
            dc.transform = transform;
            dc.drawVertices = showVerts;
            dc.drawWireframe = showEdges;
            dc.visible = showFaces || showEdges || showVerts;
            DrawMesh(cmd, dc, showEdges ? mWireframePipeline : (showVerts ? mVertexPointPipeline : mSkyboxPipeline));
        }

        void NkRenderer3D::RenderNormals(NkICommandBuffer* cmd, NkStaticMesh* mesh,
                                         const NkMat4f& transform, float length,
                                         NkColor4f color) {
            (void)length;
            (void)color;
            if (!cmd || !mesh) return;
            NkDrawCall dc{};
            dc.mesh = mesh;
            dc.transform = transform;
            dc.drawVertices = true;
            DrawMesh(cmd, dc, mVertexPointPipeline);
        }

        void NkRenderer3D::RenderBoundingBox(NkICommandBuffer* cmd, const NkAABB& aabb,
                                             const NkMat4f& transform, NkColor4f color) {
            (void)color;
            if (!cmd || !mDevice || !mWireframePipeline.IsValid() || !mSceneDescSet.IsValid()) return;

            const NkVec3f mn = aabb.min;
            const NkVec3f mx = aabb.max;
            const NkVec3f c[8] = {
                {mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z},
                {mn.x, mx.y, mn.z}, {mx.x, mx.y, mn.z},
                {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z},
                {mn.x, mx.y, mx.z}, {mx.x, mx.y, mx.z}
            };

            NkVector<NkVertex3D> verts;
            verts.Resize(8);
            for (uint32 i = 0; i < 8; ++i) {
                verts[i].position = TransformPoint(transform, c[i]);
                verts[i].normal = {0, 1, 0};
                verts[i].color = color;
            }

            static const uint32 kLineIdx[24] = {
                0,1, 1,3, 3,2, 2,0,
                4,5, 5,7, 7,6, 6,4,
                0,4, 1,5, 2,6, 3,7
            };

            NkBufferHandle vbo = mDevice->CreateBuffer(NkBufferDesc::Vertex((uint64)verts.Size() * sizeof(NkVertex3D), verts.Data()));
            NkBufferHandle ibo = mDevice->CreateBuffer(NkBufferDesc::Index((uint64)sizeof(kLineIdx), kLineIdx));
            if (!vbo.IsValid() || !ibo.IsValid()) {
                if (vbo.IsValid()) mDevice->DestroyBuffer(vbo);
                if (ibo.IsValid()) mDevice->DestroyBuffer(ibo);
                return;
            }

            const NkMat4f identity = NkMat4f::Identity();
            mDevice->WriteBuffer(mShadowUBO, &identity, sizeof(identity), 0);

            cmd->BindGraphicsPipeline(mWireframePipeline);
            cmd->BindDescriptorSet(mSceneDescSet, 0);
            cmd->BindVertexBuffer(0, vbo, 0);
            cmd->BindIndexBuffer(ibo, NkIndexFormat::NK_UINT32, 0);
            cmd->DrawIndexed(24, 1, 0, 0, 0);

            mDevice->DestroyBuffer(ibo);
            mDevice->DestroyBuffer(vbo);
        }

        uint32 NkRenderer3D::PickObject(NkRenderScene& scene, uint32 pixelX, uint32 pixelY) {
            if (mWidth == 0 || mHeight == 0) return 0;

            const NkCamera& cam = scene.GetCamera();
            const float nx = (2.0f * (float)pixelX) / (float)mWidth - 1.0f;
            const float ny = 1.0f - (2.0f * (float)pixelY) / (float)mHeight;

            const NkMat4f view = cam.GetView();
            const NkMat4f proj = cam.GetProjection((float)mWidth / (float)mHeight);
            const NkMat4f invVP = (proj * view).Inverse();

            const NkVec4f nearP = invVP * NkVec4f(nx, ny, -1.0f, 1.0f);
            const NkVec4f farP  = invVP * NkVec4f(nx, ny,  1.0f, 1.0f);
            const NkVec3f p0 = (nearP.w != 0.0f) ? NkVec3f(nearP.x / nearP.w, nearP.y / nearP.w, nearP.z / nearP.w)
                                                 : NkVec3f(nearP.x, nearP.y, nearP.z);
            const NkVec3f p1 = (farP.w != 0.0f) ? NkVec3f(farP.x / farP.w, farP.y / farP.w, farP.z / farP.w)
                                                : NkVec3f(farP.x, farP.y, farP.z);
            const NkVec3f rd = V3Norm(V3Sub(p1, p0), {0.f, 0.f, -1.f});

            float bestT = 1e30f;
            uint32 bestID = 0;

            const auto& dcs = scene.GetDrawCalls();
            for (uint32 i = 0; i < (uint32)dcs.Size(); ++i) {
                const NkDrawCall& dc = dcs[i];
                if (!dc.visible || !dc.mesh) continue;

                const NkAABB worldAABB = TransformAABB(dc.mesh->GetAABB(), dc.transform);
                float t = 0.0f;
                if (RayAABB(p0, rd, worldAABB, t) && t < bestT) {
                    bestT = t;
                    bestID = dc.objectID ? dc.objectID : (i + 1);
                }
            }

            return bestID;
        }

        NkRenderTarget* NkRenderer3D::GetGBufferRT() const {
            return mGBuffer.rt;
        }

        NkRenderTarget* NkRenderer3D::GetHDRTarget() const {
            return mHDRTarget.IsValid() ? const_cast<NkRenderTarget*>(&mHDRTarget) : nullptr;
        }

        NkRenderTarget* NkRenderer3D::GetShadowMap() const {
            return mShadowMapRT.IsValid() ? const_cast<NkRenderTarget*>(&mShadowMapRT) : nullptr;
        }

        NkTextureCube* NkRenderer3D::GenerateIrradianceMap(NkTextureCube* envMap, uint32 size) {
            (void)size;
            if (!envMap) return nullptr;
            return envMap;
        }

        NkTextureCube* NkRenderer3D::GeneratePrefilterMap(NkTextureCube* envMap, uint32 size) {
            (void)size;
            if (!envMap) return nullptr;
            return envMap;
        }

        NkTexture2D* NkRenderer3D::GenerateBRDF_LUT(uint32 size) {
            if (!mDevice || size == 0) return nullptr;

            NkTexture2D* tex = new NkTexture2D();
            NkVector<uint8> pixels;
            pixels.Resize((usize)size * (usize)size * 4u);
            for (uint32 y = 0; y < size; ++y) {
                for (uint32 x = 0; x < size; ++x) {
                    const float u = (float)x / (float)NkMax(1u, size - 1u);
                    const float v = (float)y / (float)NkMax(1u, size - 1u);
                    const usize i = ((usize)y * (usize)size + (usize)x) * 4u;
                    pixels[i + 0] = (uint8)(NkClamp(u, 0.0f, 1.0f) * 255.0f);
                    pixels[i + 1] = (uint8)(NkClamp(v, 0.0f, 1.0f) * 255.0f);
                    pixels[i + 2] = 0;
                    pixels[i + 3] = 255;
                }
            }

            NkTextureParams params = NkTextureParams::Linear();
            params.generateMips = false;
            params.srgb = false;
            params.wrapU = NkAddressMode::NK_CLAMP_TO_EDGE;
            params.wrapV = NkAddressMode::NK_CLAMP_TO_EDGE;
            if (!tex->LoadFromMemory(mDevice, pixels.Data(), size, size, NkGPUFormat::NK_RGBA8_UNORM, params)) {
                delete tex;
                return nullptr;
            }
            return tex;
        }

        bool NkRenderer3D::InitShaders() {
            if (!mDevice) return false;

            NkShaderAssetDesc desc{};
            desc.name = "NkRenderer3D.Basic";
            desc.glslVertSrc = NkBuiltinShaders::Basic3DVert();
            desc.glslFragSrc = NkBuiltinShaders::Basic3DFrag();
            desc.glslVkVertSrc = NkBuiltinShaders::Basic3DVertVK();
            desc.glslVkFragSrc = NkBuiltinShaders::Basic3DFragVK();
            desc.hlslVSSrc = NkBuiltinShaders::Basic3DVertHLSL();
            desc.hlslPSSrc = NkBuiltinShaders::Basic3DFragHLSL();
            desc.mslSrc = NkString(NkBuiltinShaders::Basic3DVertMSL()) + NkString("\n") + NkString(NkBuiltinShaders::Basic3DFragMSL());
            desc.vertEntry = "VSMain";
            desc.fragEntry = "PSMain";

            if (!mUnlitShader.Compile(mDevice, desc)) {
                return false;
            }
            return true;
        }

        bool NkRenderer3D::InitRenderTargets(uint32 w, uint32 h) {
            if (!mDevice || w == 0 || h == 0) return false;

            NkRenderTargetDesc hdr{};
            hdr.width = w;
            hdr.height = h;
            hdr.colorFormat = NkGPUFormat::NK_RGBA16_FLOAT;
            hdr.depthFormat = NkGPUFormat::NK_D32_FLOAT;
            hdr.samples = 1;
            if (!mHDRTarget.Create(mDevice, hdr)) return false;

            NkRenderTargetDesc shadow{};
            shadow.width = NkMax(256u, mShadowSettings.shadowMapSize);
            shadow.height = shadow.width;
            shadow.colorFormat = NkGPUFormat::NK_R8_UNORM;
            shadow.depthFormat = NkGPUFormat::NK_D32_FLOAT;
            shadow.samples = 1;
            if (!mShadowMapRT.Create(mDevice, shadow)) return false;

            mGBuffer.rt = &mHDRTarget;
            return true;
        }

        bool NkRenderer3D::InitBuffers() {
            if (!mDevice) return false;

            mSceneUBO = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(NkSceneUBOData)));
            mShadowUBO = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(NkMat4f)));
            mLightSSBO = mDevice->CreateBuffer(NkBufferDesc::Storage(sizeof(NkLight) * 256u));

            return mSceneUBO.IsValid() && mShadowUBO.IsValid() && mLightSSBO.IsValid();
        }

        bool NkRenderer3D::InitIBL() {
            if (!mBRDF_LUT) {
                mBRDF_LUT = GenerateBRDF_LUT(256);
            }
            return true;
        }

        bool NkRenderer3D::InitDescriptors() {
            if (!mDevice || !mSceneUBO.IsValid() || !mShadowUBO.IsValid()) return false;

            if (!mSceneLayout.IsValid()) {
                NkDescriptorSetLayoutDesc layout{};
                layout.debugName = "NkRenderer3D.SceneLayout";
                layout.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_VERTEX | NkShaderStage::NK_FRAGMENT);
                layout.Add(1, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_VERTEX);
                mSceneLayout = mDevice->CreateDescriptorSetLayout(layout);
                if (!mSceneLayout.IsValid()) return false;
            }

            if (!mSceneDescSet.IsValid()) {
                mSceneDescSet = mDevice->AllocateDescriptorSet(mSceneLayout);
                if (!mSceneDescSet.IsValid()) return false;
            }

            NkDescriptorWrite writes[2]{};
            writes[0].set = mSceneDescSet;
            writes[0].binding = 0;
            writes[0].type = NkDescriptorType::NK_UNIFORM_BUFFER;
            writes[0].buffer = mSceneUBO;
            writes[0].bufferRange = sizeof(NkSceneUBOData);

            writes[1].set = mSceneDescSet;
            writes[1].binding = 1;
            writes[1].type = NkDescriptorType::NK_UNIFORM_BUFFER;
            writes[1].buffer = mShadowUBO;
            writes[1].bufferRange = sizeof(NkMat4f);

            mDevice->UpdateDescriptorSets(writes, 2);
            return true;
        }

        bool NkRenderer3D::InitPipelines() {
            if (!mDevice || !mSceneLayout.IsValid() || !mUnlitShader.IsValid()) return false;

            NkRenderPassHandle rp = mActiveRenderPass.IsValid() ? mActiveRenderPass : mDevice->GetSwapchainRenderPass();
            if (!rp.IsValid()) return false;

            if (mPipelineRenderPass == rp
                && mSkyboxPipeline.IsValid()
                && mWireframePipeline.IsValid()
                && mVertexPointPipeline.IsValid()) {
                return true;
            }

            if (mVertexPointPipeline.IsValid()) mDevice->DestroyPipeline(mVertexPointPipeline);
            if (mWireframePipeline.IsValid()) mDevice->DestroyPipeline(mWireframePipeline);
            if (mSkyboxPipeline.IsValid()) mDevice->DestroyPipeline(mSkyboxPipeline);
            mVertexPointPipeline = {};
            mWireframePipeline = {};
            mSkyboxPipeline = {};

            const NkShaderHandle shader = mUnlitShader.GetDefault();
            if (!shader.IsValid()) return false;

            const NkVertexLayout layout = NkVertex3D::GetLayout();

            auto mkPipe = [&](NkPrimitiveTopology topo, const NkRasterizerDesc& rast, const char* name) -> NkPipelineHandle {
                NkGraphicsPipelineDesc d{};
                d.shader = shader;
                d.vertexLayout = layout;
                d.topology = topo;
                d.rasterizer = rast;
                d.depthStencil = NkDepthStencilDesc::Default();
                d.blend = NkBlendDesc::Opaque();
                d.renderPass = rp;
                d.descriptorSetLayouts.PushBack(mSceneLayout);
                d.debugName = name;
                return mDevice->CreateGraphicsPipeline(d);
            };

            NkRasterizerDesc solidR = NkRasterizerDesc::Default();
            solidR.scissorTest = true;

            NkRasterizerDesc lineR = NkRasterizerDesc::NoCull();
            lineR.scissorTest = true;

            NkRasterizerDesc pointR = NkRasterizerDesc::NoCull();
            pointR.scissorTest = true;

            mSkyboxPipeline = mkPipe(NkPrimitiveTopology::NK_TRIANGLE_LIST, solidR, "NkRenderer3D.Solid");
            mWireframePipeline = mkPipe(NkPrimitiveTopology::NK_LINE_LIST, lineR, "NkRenderer3D.Line");
            mVertexPointPipeline = mkPipe(NkPrimitiveTopology::NK_POINT_LIST, pointR, "NkRenderer3D.Point");

            mPipelineRenderPass = rp;
            return mSkyboxPipeline.IsValid() && mWireframePipeline.IsValid() && mVertexPointPipeline.IsValid();
        }

        void NkRenderer3D::UpdateSceneUBO(const NkRenderScene& scene) {
            if (!mDevice || !mSceneUBO.IsValid()) return;

            const NkCamera& cam = scene.GetCamera();
            NkSceneUBOData d{};
            d.view = cam.GetView();
            d.proj = cam.GetProjection((float)mWidth / (float)NkMax(1u, mHeight));
            d.viewProj = d.proj * d.view;
            d.cameraPos = {cam.position.x, cam.position.y, cam.position.z, cam.nearPlane};
            d.ambient = {1.f, 1.f, 1.f, NkClamp(mPPSettings.ambient, 0.0f, 4.0f)};

            mDevice->WriteBuffer(mSceneUBO, &d, sizeof(d), 0);
        }

        void NkRenderer3D::UpdateLightSSBO(const NkRenderScene& scene) {
            if (!mDevice || !mLightSSBO.IsValid()) return;
            const auto& lights = scene.GetLights();
            if (lights.Empty()) return;
            const uint64 maxBytes = sizeof(NkLight) * 256u;
            const uint64 bytes = NkMin<uint64>((uint64)lights.Size() * sizeof(NkLight), maxBytes);
            mDevice->WriteBuffer(mLightSSBO, lights.Data(), bytes, 0);
        }

        void NkRenderer3D::PassShadow(NkICommandBuffer* cmd, NkRenderScene& scene) {
            (void)cmd;
            (void)scene;
        }

        void NkRenderer3D::PassDepthPrepass(NkICommandBuffer* cmd, NkRenderScene& scene) {
            (void)cmd;
            (void)scene;
        }

        void NkRenderer3D::PassGBuffer(NkICommandBuffer* cmd, NkRenderScene& scene) {
            (void)cmd;
            (void)scene;
        }

        void NkRenderer3D::PassLighting(NkICommandBuffer* cmd, NkRenderScene& scene) {
            (void)cmd;
            (void)scene;
        }

        void NkRenderer3D::PassForward(NkICommandBuffer* cmd, NkRenderScene& scene) {
            if (!cmd) return;
            for (const auto& dc : scene.GetDrawCalls()) {
                if (!dc.visible) continue;
                DrawMesh(cmd, dc);
            }
        }

        void NkRenderer3D::PassSkybox(NkICommandBuffer* cmd, const NkSkySettings& sky) {
            (void)cmd;
            (void)sky;
        }

        void NkRenderer3D::PassSSAO(NkICommandBuffer* cmd) {
            (void)cmd;
        }

        void NkRenderer3D::PassBloom(NkICommandBuffer* cmd) {
            (void)cmd;
        }

        void NkRenderer3D::PassPostProcess(NkICommandBuffer* cmd) {
            (void)cmd;
        }

        void NkRenderer3D::PassFXAA(NkICommandBuffer* cmd) {
            (void)cmd;
        }

        void NkRenderer3D::PassBlit(NkICommandBuffer* cmd, NkRenderTarget* from) {
            (void)cmd;
            (void)from;
        }

        void NkRenderer3D::DrawMesh(NkICommandBuffer* cmd, const NkDrawCall& dc,
                                    NkPipelineHandle overridePipe) {
            if (!cmd || !dc.visible) return;
            if (!mSceneDescSet.IsValid()) return;

            const bool wantWire = (mRenderMode == NkRenderMode::NK_WIREFRAME) || dc.drawWireframe;
            const bool wantPoints = (mRenderMode == NkRenderMode::NK_POINTS) || dc.drawVertices;

            NkPipelineHandle pipe = overridePipe;
            if (!pipe.IsValid()) {
                if (wantPoints) pipe = mVertexPointPipeline;
                else if (wantWire) pipe = mWireframePipeline;
                else pipe = mSkyboxPipeline;
            }
            if (!pipe.IsValid()) return;

            if (!mDevice->WriteBuffer(mShadowUBO, &dc.transform, sizeof(dc.transform), 0)) return;

            cmd->BindGraphicsPipeline(pipe);
            cmd->BindDescriptorSet(mSceneDescSet, 0);

            if (dc.mesh) {
                if (wantPoints) {
                    dc.mesh->BuildPointVBO(mDevice);
                    if (!dc.mesh->GetPointVBO().IsValid()) return;
                    cmd->BindVertexBuffer(0, dc.mesh->GetPointVBO(), 0);
                    cmd->Draw(dc.mesh->GetVertexCount(), 1, 0, 0);
                    ++mStats.drawCalls;
                    mStats.vertices += dc.mesh->GetVertexCount();
                    return;
                }

                cmd->BindVertexBuffer(0, dc.mesh->GetVBO(), 0);

                if (wantWire) {
                    dc.mesh->BuildWireframe(mDevice);
                    if (!dc.mesh->HasWireframe()) return;
                    cmd->BindIndexBuffer(dc.mesh->GetWireframeIBO(), NkIndexFormat::NK_UINT32, 0);
                    const uint32 lineIndexCount = dc.mesh->GetIndexCount() * 2u;
                    cmd->DrawIndexed(lineIndexCount, NkMax(1u, dc.instanceCount), 0, 0, 0);
                    ++mStats.drawCalls;
                    mStats.vertices += dc.mesh->GetVertexCount();
                    return;
                }

                if (dc.mesh->HasIndices()) {
                    cmd->BindIndexBuffer(dc.mesh->GetIBO(), NkIndexFormat::NK_UINT32, 0);
                    const auto& sms = dc.mesh->GetSubMeshes();
                    if (!sms.Empty() && dc.subMeshIndex < (uint32)sms.Size()) {
                        const NkSubMesh& sm = sms[dc.subMeshIndex];
                        const uint32 indexCount = sm.indexCount > 0 ? sm.indexCount : dc.mesh->GetIndexCount();
                        cmd->DrawIndexed(indexCount, NkMax(1u, dc.instanceCount), sm.firstIndex, sm.firstVertex, 0);
                        mStats.triangles += indexCount / 3;
                    } else {
                        cmd->DrawIndexed(dc.mesh->GetIndexCount(), NkMax(1u, dc.instanceCount), 0, 0, 0);
                        mStats.triangles += dc.mesh->GetIndexCount() / 3;
                    }
                } else {
                    cmd->Draw(dc.mesh->GetVertexCount(), NkMax(1u, dc.instanceCount), 0, 0);
                }

                ++mStats.drawCalls;
                mStats.vertices += dc.mesh->GetVertexCount();
                return;
            }

            if (dc.dynMesh && dc.dynMesh->IsValid()) {
                cmd->BindVertexBuffer(0, dc.dynMesh->GetVBO(), 0);
                if (dc.dynMesh->HasIndices()) {
                    cmd->BindIndexBuffer(dc.dynMesh->GetIBO(), NkIndexFormat::NK_UINT32, 0);
                    cmd->DrawIndexed(dc.dynMesh->GetIndexCount(), NkMax(1u, dc.instanceCount), 0, 0, 0);
                    mStats.triangles += dc.dynMesh->GetIndexCount() / 3;
                } else {
                    cmd->Draw(dc.dynMesh->GetVertexCount(), NkMax(1u, dc.instanceCount), 0, 0);
                }
                ++mStats.drawCalls;
                mStats.vertices += dc.dynMesh->GetVertexCount();
            }
        }

        void NkRenderer3D::DrawFullscreen(NkICommandBuffer* cmd) {
            if (!cmd) return;
            if (!mFullscreenVBO.IsValid()) {
                static const float kTri[6] = {-1.f, -1.f, 3.f, -1.f, -1.f, 3.f};
                mFullscreenVBO = mDevice->CreateBuffer(NkBufferDesc::Vertex(sizeof(kTri), kTri));
                if (!mFullscreenVBO.IsValid()) return;
            }
            cmd->BindVertexBuffer(0, mFullscreenVBO, 0);
            cmd->Draw(3, 1, 0, 0);
        }

        void NkRenderer3D::DrawSkybox(NkICommandBuffer* cmd) {
            (void)cmd;
        }

        void NkRenderer3D::ResetStats() {
            mStats = {};
        }

    } // namespace renderer
} // namespace nkentseu
