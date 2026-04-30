#include "NkVFXSystem.h"

#include "NKLogger/NkLog.h"
#include "NKRenderer/Core/NkShaderAsset.h"
#include "NKRHI/Core/NkGraphicsApi.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>

namespace nkentseu {
    namespace renderer {

        namespace {
            template<typename T>
            static void RemovePtr(NkVector<T*>& vec, T* ptr) {
                for (usize i = 0; i < vec.Size(); ++i) {
                    if (vec[i] == ptr) {
                        vec.Erase(vec.Begin() + i);
                        return;
                    }
                }
            }

            static float Clamp01(float v) {
                return NkClamp(v, 0.0f, 1.0f);
            }

            static NkVec3f Vec3Add(const NkVec3f& a, const NkVec3f& b) {
                return {a.x + b.x, a.y + b.y, a.z + b.z};
            }

            static NkVec3f Vec3Sub(const NkVec3f& a, const NkVec3f& b) {
                return {a.x - b.x, a.y - b.y, a.z - b.z};
            }

            static NkVec3f Vec3Mul(const NkVec3f& a, float s) {
                return {a.x * s, a.y * s, a.z * s};
            }

            static float Vec3Len(const NkVec3f& v) {
                return NkSqrt(v.x * v.x + v.y * v.y + v.z * v.z);
            }

            static NkVec3f Vec3Normalize(const NkVec3f& v, const NkVec3f& fallback = {0, 1, 0}) {
                const float l = Vec3Len(v);
                if (l <= 1e-6f) return fallback;
                const float inv = 1.0f / l;
                return {v.x * inv, v.y * inv, v.z * inv};
            }

            static NkColor4f ColorLerp(const NkColor4f& a, const NkColor4f& b, float t) {
                return {
                    a.r + (b.r - a.r) * t,
                    a.g + (b.g - a.g) * t,
                    a.b + (b.b - a.b) * t,
                    a.a + (b.a - a.a) * t
                };
            }

            static float FloatLerp(float a, float b, float t) {
                return a + (b - a) * t;
            }

            static std::unordered_map<const NkVFXRenderer*, std::unordered_set<NkParticleSystem*>> gOwnedSystems;

            struct VFXCameraUBOData {
                NkMat4f view;
                NkMat4f proj;
                NkMat4f viewProj;
                NkVec4f cameraPos;
                NkVec4f ambient;
            };
        } // namespace

        float NkCurve::Evaluate(float t) const {
            if (keys.Empty()) return 0.0f;
            if (keys.Size() == 1) return keys[0].value;

            const float clamped = Clamp01(t);
            if (clamped <= keys[0].t) return keys[0].value;
            if (clamped >= keys[keys.Size() - 1].t) return keys[keys.Size() - 1].value;

            for (usize i = 0; i + 1 < keys.Size(); ++i) {
                const NkCurveKey& a = keys[i];
                const NkCurveKey& b = keys[i + 1];
                if (clamped >= a.t && clamped <= b.t) {
                    const float denom = NkMax(1e-6f, b.t - a.t);
                    const float localT = (clamped - a.t) / denom;
                    return FloatLerp(a.value, b.value, localT);
                }
            }
            return keys[keys.Size() - 1].value;
        }

        NkColor4f NkColorGradient::Evaluate(float t) const {
            if (keys.Empty()) return NkColor4f::White();
            if (keys.Size() == 1) return keys[0].color;

            const float clamped = Clamp01(t);
            if (clamped <= keys[0].t) return keys[0].color;
            if (clamped >= keys[keys.Size() - 1].t) return keys[keys.Size() - 1].color;

            for (usize i = 0; i + 1 < keys.Size(); ++i) {
                const NkGradientKey& a = keys[i];
                const NkGradientKey& b = keys[i + 1];
                if (clamped >= a.t && clamped <= b.t) {
                    const float denom = NkMax(1e-6f, b.t - a.t);
                    const float localT = (clamped - a.t) / denom;
                    return ColorLerp(a.color, b.color, localT);
                }
            }
            return keys[keys.Size() - 1].color;
        }

        bool NkParticleSystem::Create(NkIDevice* device, const NkParticleSystemDesc& desc) {
            Destroy();
            if (!device || !device->IsValid() || desc.maxParticles == 0) {
                return false;
            }

            mDevice = device;
            mDesc = desc;
            mParticles.Resize(desc.maxParticles);
            KillAll();
            mAliveCount = 0;
            mTime = 0.0f;
            mEmitAccum = 0.0f;
            mPlaying = true;
            mAABB = {};

            mVBO = mDevice->CreateBuffer(NkBufferDesc::VertexDynamic((uint64)desc.maxParticles * sizeof(NkVertex3D)));
            if (!mVBO.IsValid()) {
                Destroy();
                return false;
            }

            if (mDesc.warmupTime > 0.0f) {
                const float step = 1.0f / 30.0f;
                float remaining = mDesc.warmupTime;
                while (remaining > 0.0f) {
                    const float dt = NkMin(step, remaining);
                    Update(dt);
                    remaining -= dt;
                }
            }

            return true;
        }

        void NkParticleSystem::Destroy() {
            if (mDevice && mVBO.IsValid()) {
                mDevice->DestroyBuffer(mVBO);
            }
            mVBO = {};
            mParticles.Clear();
            mAliveCount = 0;
            mTime = 0.0f;
            mEmitAccum = 0.0f;
            mPlaying = false;
            mDevice = nullptr;
            mMaterial = nullptr;
        }

        void NkParticleSystem::Update(float dt) {
            if (!mDevice || dt <= 0.0f) return;

            if (mPlaying) {
                mTime += dt;

                if (!mDesc.loop && mTime > mDesc.duration) {
                    mPlaying = false;
                }

                if (mTime >= mDesc.startDelay) {
                    mEmitAccum += mDesc.emitter.rateOverTime * dt;
                    const uint32 toEmit = (uint32)NkFloor(mEmitAccum);
                    if (toEmit > 0) {
                        Emit(toEmit);
                        mEmitAccum -= (float)toEmit;
                    }
                }
            }

            for (auto& p : mParticles) {
                if (!p.alive) continue;
                SimulateParticle(p, dt);
            }

            mAliveCount = 0;
            for (const auto& p : mParticles) {
                if (p.alive) ++mAliveCount;
            }

            if (mDesc.sortByDepth && mAliveCount > 1) {
                std::sort(mParticles.Begin(), mParticles.End(), [](const NkParticle& a, const NkParticle& b) {
                    if (a.alive != b.alive) return a.alive > b.alive;
                    return a.age > b.age;
                });
            }

            UpdateAABB();
        }

        void NkParticleSystem::Emit(uint32 count) {
            if (!mDevice) return;
            for (uint32 i = 0; i < count; ++i) {
                SpawnParticle();
            }
        }

        void NkParticleSystem::UploadToGPU() {
            if (!mDevice || !mVBO.IsValid()) return;

            NkVector<NkVertex3D> verts;
            verts.Reserve((usize)mAliveCount);
            for (const auto& p : mParticles) {
                if (!p.alive) continue;
                NkVertex3D v{};
                v.position = p.position;
                v.color = p.color;
                v.uv0 = {p.size, (float)p.frameIndex};
                verts.PushBack(v);
            }

            if (!verts.Empty()) {
                mDevice->WriteBuffer(mVBO, verts.Data(), (uint64)(verts.Size() * sizeof(NkVertex3D)), 0);
            }
        }

        void NkParticleSystem::KillAll() {
            for (auto& p : mParticles) {
                p.alive = false;
                p.life = 0.0f;
                p.age = 1.0f;
            }
            mAliveCount = 0;
        }
        void NkParticleSystem::SpawnParticle() {
            for (auto& p : mParticles) {
                if (p.alive) continue;

                p.alive = true;
                p.maxLife = RandRange(mDesc.minLife, mDesc.maxLife);
                p.life = p.maxLife;
                p.age = 0.0f;
                p.rotation = RandRange(0.0f, 360.0f);
                p.rotationSpeed = mDesc.rotationSpeed;

                p.position = SampleEmitterPos();
                p.velocity = Vec3Mul(SampleEmitterDir(), RandRange(mDesc.minSpeed, mDesc.maxSpeed));
                p.acceleration = Vec3Mul(mDesc.gravity, mDesc.gravityScale);

                p.size = mDesc.startSize;
                p.sizeEnd = mDesc.endSize;
                p.color = mDesc.colorOverLife.Evaluate(0.0f);
                p.colorEnd = mDesc.colorOverLife.Evaluate(1.0f);

                const uint32 frameCount = NkMax(1u, mDesc.sheetCols * mDesc.sheetRows);
                p.frameIndex = mDesc.randomStartFrame ? (uint32)(std::rand() % frameCount) : 0u;
                return;
            }
        }

        void NkParticleSystem::SimulateParticle(NkParticle& p, float dt) {
            if (!p.alive) return;

            p.life -= dt;
            if (p.life <= 0.0f) {
                p.alive = false;
                return;
            }

            const float t = Clamp01(1.0f - (p.life / NkMax(1e-6f, p.maxLife)));
            p.age = t;

            p.velocity = Vec3Add(p.velocity, Vec3Mul(p.acceleration, dt));
            if (mDesc.drag > 0.0f) {
                const float dragFactor = 1.0f / (1.0f + mDesc.drag * dt);
                p.velocity = Vec3Mul(p.velocity, dragFactor);
            }
            if (mDesc.turbulence > 0.0f) {
                const NkVec3f n = {
                    RandRange(-1.0f, 1.0f),
                    RandRange(-1.0f, 1.0f),
                    RandRange(-1.0f, 1.0f)
                };
                p.velocity = Vec3Add(p.velocity, Vec3Mul(n, mDesc.turbulence * dt));
            }

            p.position = Vec3Add(p.position, Vec3Mul(p.velocity, dt));
            p.rotation += p.rotationSpeed * dt;

            const float curve = Clamp01(mDesc.sizeCurve.Evaluate(t));
            p.size = FloatLerp(mDesc.startSize, mDesc.endSize, curve);
            p.color = mDesc.colorOverLife.Evaluate(t);

            if (mDesc.collisions && p.position.y < 0.0f) {
                p.position.y = 0.0f;
                if (p.velocity.y < 0.0f) p.velocity.y = -p.velocity.y * mDesc.bounciness;
                p.life -= mDesc.lifetimeLoss * p.maxLife;
                if (p.life <= 0.0f) {
                    p.alive = false;
                }
            }
        }

        NkVec3f NkParticleSystem::SampleEmitterPos() const {
            const NkEmitterModule& e = mDesc.emitter;
            NkVec3f local = e.position;
            switch (e.shape) {
                case NkEmitterShape::NK_POINT:
                    break;
                case NkEmitterShape::NK_SPHERE: {
                    const float u = RandRange(0.0f, 1.0f);
                    const float v = RandRange(0.0f, 1.0f);
                    const float theta = 2.0f * (float)NkPi * u;
                    const float phi = NkAcos(2.0f * v - 1.0f);
                    const float r = e.radius * NkCbrt(RandRange(0.0f, 1.0f));
                    local.x += r * NkSin(phi) * NkCos(theta);
                    local.y += r * NkCos(phi);
                    local.z += r * NkSin(phi) * NkSin(theta);
                    break;
                }
                case NkEmitterShape::NK_BOX:
                    local.x += RandRange(-e.boxSize.x * 0.5f, e.boxSize.x * 0.5f);
                    local.y += RandRange(-e.boxSize.y * 0.5f, e.boxSize.y * 0.5f);
                    local.z += RandRange(-e.boxSize.z * 0.5f, e.boxSize.z * 0.5f);
                    break;
                case NkEmitterShape::NK_CONE: {
                    const float a = RandRange(0.0f, 2.0f * (float)NkPi);
                    const float r = RandRange(0.0f, e.radius);
                    local.x += NkCos(a) * r;
                    local.z += NkSin(a) * r;
                    break;
                }
                case NkEmitterShape::NK_DISK: {
                    const float a = RandRange(0.0f, 2.0f * (float)NkPi);
                    const float r = RandRange(0.0f, e.radius);
                    local.x += NkCos(a) * r;
                    local.z += NkSin(a) * r;
                    break;
                }
                case NkEmitterShape::NK_EDGE:
                    local.x += RandRange(-e.radius, e.radius);
                    break;
                case NkEmitterShape::NK_MESH:
                    break;
            }

            if (mDesc.worldSpace) return Vec3Add(mWorldPos, local);
            return local;
        }

        NkVec3f NkParticleSystem::SampleEmitterDir() const {
            const NkEmitterModule& e = mDesc.emitter;
            if (e.randomDirection) {
                return Vec3Normalize({
                    RandRange(-1.0f, 1.0f),
                    RandRange(-1.0f, 1.0f),
                    RandRange(-1.0f, 1.0f)
                }, {0, 1, 0});
            }

            NkVec3f dir = Vec3Normalize(mDesc.direction, {0, 1, 0});
            if (e.spread > 0.0f) {
                dir = Vec3Normalize({
                    dir.x + RandRange(-e.spread, e.spread),
                    dir.y + RandRange(-e.spread, e.spread),
                    dir.z + RandRange(-e.spread, e.spread)
                }, dir);
            }

            if (e.shape == NkEmitterShape::NK_CONE) {
                const float coneRad = NkToRadians(e.coneAngle);
                const float jitter = NkTan(coneRad) * RandRange(0.0f, 1.0f);
                dir = Vec3Normalize({dir.x + RandRange(-jitter, jitter), dir.y, dir.z + RandRange(-jitter, jitter)}, dir);
            }
            return dir;
        }

        float NkParticleSystem::RandRange(float a, float b) const {
            const float t = (float)std::rand() / (float)RAND_MAX;
            return a + (b - a) * t;
        }

        void NkParticleSystem::UpdateAABB() {
            bool hasAlive = false;
            NkVec3f minV = { 1e9f, 1e9f, 1e9f };
            NkVec3f maxV = {-1e9f,-1e9f,-1e9f };

            for (const auto& p : mParticles) {
                if (!p.alive) continue;
                hasAlive = true;
                const float r = NkMax(0.001f, p.size * 0.5f);
                minV.x = NkMin(minV.x, p.position.x - r);
                minV.y = NkMin(minV.y, p.position.y - r);
                minV.z = NkMin(minV.z, p.position.z - r);
                maxV.x = NkMax(maxV.x, p.position.x + r);
                maxV.y = NkMax(maxV.y, p.position.y + r);
                maxV.z = NkMax(maxV.z, p.position.z + r);
            }

            if (!hasAlive) {
                mAABB.min = mWorldPos;
                mAABB.max = mWorldPos;
                return;
            }
            mAABB.min = minV;
            mAABB.max = maxV;
        }

        bool NkTrail::Create(NkIDevice* device, const NkTrailDesc& desc) {
            Destroy();
            if (!device || !device->IsValid() || desc.maxPoints < 2) {
                return false;
            }
            mDevice = device;
            mDesc = desc;
            mPoints.Clear();
            mPoints.Reserve(desc.maxPoints);
            mVertCount = 0;
            mFirstPoint = true;
            mLastPos = {0, 0, 0};
            mVBO = mDevice->CreateBuffer(NkBufferDesc::VertexDynamic((uint64)desc.maxPoints * 2ull * sizeof(NkVertex3D)));
            return mVBO.IsValid();
        }

        void NkTrail::Destroy() {
            if (mDevice && mVBO.IsValid()) {
                mDevice->DestroyBuffer(mVBO);
            }
            mVBO = {};
            mPoints.Clear();
            mVertCount = 0;
            mMaterial = nullptr;
            mDevice = nullptr;
            mFirstPoint = true;
        }
        void NkTrail::Update(float dt, const NkVec3f& emitterPos) {
            if (dt <= 0.0f) return;

            for (usize i = 0; i < mPoints.Size();) {
                mPoints[i].age += dt;
                if (mPoints[i].age >= mPoints[i].maxAge) {
                    mPoints.Erase(mPoints.Begin() + i);
                    continue;
                }
                const float t = Clamp01(mPoints[i].age / NkMax(1e-6f, mPoints[i].maxAge));
                mPoints[i].width = FloatLerp(mDesc.startWidth, mDesc.endWidth, t);
                mPoints[i].color = mDesc.colorOverLife.Evaluate(t);
                ++i;
            }

            if (mFirstPoint) {
                NkTrailPoint p{};
                p.pos = emitterPos;
                p.age = 0.0f;
                p.maxAge = NkMax(0.01f, mDesc.lifetime);
                p.width = mDesc.startWidth;
                p.color = mDesc.colorOverLife.Evaluate(0.0f);
                mPoints.PushBack(p);
                mLastPos = emitterPos;
                mFirstPoint = false;
            } else {
                const NkVec3f delta = Vec3Sub(emitterPos, mLastPos);
                if (Vec3Len(delta) >= mDesc.minDistance) {
                    NkTrailPoint p{};
                    p.pos = emitterPos;
                    p.age = 0.0f;
                    p.maxAge = NkMax(0.01f, mDesc.lifetime);
                    p.width = mDesc.startWidth;
                    p.color = mDesc.colorOverLife.Evaluate(0.0f);
                    mPoints.PushBack(p);
                    mLastPos = emitterPos;
                }
            }

            while (mPoints.Size() > mDesc.maxPoints) {
                mPoints.Erase(mPoints.Begin());
            }
        }

        void NkTrail::Clear() {
            mPoints.Clear();
            mVertCount = 0;
            mFirstPoint = true;
        }

        void NkTrail::UploadToGPU() {
            if (!mDevice || !mVBO.IsValid()) return;
            if (mPoints.Size() < 2) {
                mVertCount = 0;
                return;
            }

            NkVector<NkVertex3D> verts;
            verts.Reserve((mPoints.Size() - 1) * 2);
            for (usize i = 0; i + 1 < mPoints.Size(); ++i) {
                NkVertex3D a{};
                NkVertex3D b{};
                a.position = mPoints[i].pos;
                b.position = mPoints[i + 1].pos;
                a.color = mPoints[i].color;
                b.color = mPoints[i + 1].color;
                a.uv0 = {mPoints[i].width, 0.0f};
                b.uv0 = {mPoints[i + 1].width, 1.0f};
                verts.PushBack(a);
                verts.PushBack(b);
            }
            mVertCount = (uint32)verts.Size();
            if (!verts.Empty()) {
                mDevice->WriteBuffer(mVBO, verts.Data(), (uint64)(verts.Size() * sizeof(NkVertex3D)), 0);
            }
        }

        bool NkVFXRenderer::Init(NkIDevice* device, NkRenderPassHandle renderPass, uint32 w, uint32 h) {
            Shutdown();
            if (!device || !device->IsValid()) return false;

            mDevice = device;
            mRenderPass = renderPass;
            mWidth = w;
            mHeight = h;

            mCameraUBO = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(VFXCameraUBOData)));
            if (!mCameraUBO.IsValid()) {
                Shutdown();
                return false;
            }

            if (!InitDefaultTextures()) {
                Shutdown();
                return false;
            }

            if (!InitShaders(renderPass)) {
                Shutdown();
                return false;
            }
            if (!InitPipelines(renderPass)) {
                Shutdown();
                return false;
            }
            return true;
        }

        void NkVFXRenderer::Shutdown() {
            if (mDevice) {
                if (mCameraUBO.IsValid()) mDevice->DestroyBuffer(mCameraUBO);
                if (mParticlePipeAdd.IsValid()) mDevice->DestroyPipeline(mParticlePipeAdd);
                if (mParticlePipeAlpha.IsValid()) mDevice->DestroyPipeline(mParticlePipeAlpha);
                if (mTrailPipeline.IsValid()) mDevice->DestroyPipeline(mTrailPipeline);
                if (mParticleShader.IsValid()) mDevice->DestroyShader(mParticleShader);
                if (mTrailShader.IsValid()) mDevice->DestroyShader(mTrailShader);
                if (mDescSet.IsValid()) mDevice->FreeDescriptorSet(mDescSet);
                if (mLayout.IsValid()) mDevice->DestroyDescriptorSetLayout(mLayout);
            }

            auto ownedIt = gOwnedSystems.find(this);
            if (ownedIt != gOwnedSystems.end()) {
                for (NkParticleSystem* sys : ownedIt->second) {
                    if (sys) {
                        sys->Destroy();
                        delete sys;
                    }
                }
                gOwnedSystems.erase(ownedIt);
            }

            mSystems.Clear();
            mTrails.Clear();
            mDecals.Clear();
            mFlares.Clear();

            if (mDefaultParticleTex) {
                delete mDefaultParticleTex;
                mDefaultParticleTex = nullptr;
            }

            mCameraUBO = {};
            mParticlePipeAdd = {};
            mParticlePipeAlpha = {};
            mTrailPipeline = {};
            mParticleShader = {};
            mTrailShader = {};
            mLayout = {};
            mDescSet = {};
            mDevice = nullptr;
            mWidth = 0;
            mHeight = 0;
            mRenderPass = {};
        }

        void NkVFXRenderer::Resize(uint32 w, uint32 h) {
            mWidth = w;
            mHeight = h;
        }

        void NkVFXRenderer::Update(float dt) {
            for (auto* sys : mSystems) {
                if (!sys) continue;
                sys->Update(dt);
                sys->UploadToGPU();
            }
            for (auto* trail : mTrails) {
                if (!trail) continue;
                trail->UploadToGPU();
            }
        }

        void NkVFXRenderer::AddSystem(NkParticleSystem* sys) {
            if (!sys) return;
            for (auto* s : mSystems) {
                if (s == sys) return;
            }
            mSystems.PushBack(sys);
        }

        void NkVFXRenderer::RemoveSystem(NkParticleSystem* sys) {
            if (!sys) return;
            RemovePtr(mSystems, sys);
            auto ownedIt = gOwnedSystems.find(this);
            if (ownedIt != gOwnedSystems.end()) {
                ownedIt->second.erase(sys);
            }
        }

        void NkVFXRenderer::AddTrail(NkTrail* trail) {
            if (!trail) return;
            for (auto* t : mTrails) {
                if (t == trail) return;
            }
            mTrails.PushBack(trail);
        }

        void NkVFXRenderer::RemoveTrail(NkTrail* trail) {
            if (!trail) return;
            RemovePtr(mTrails, trail);
        }

        void NkVFXRenderer::AddDecal(const NkDecalDesc& decal) {
            mDecals.PushBack(decal);
        }

        void NkVFXRenderer::AddLensFlare(const NkLensFlareDesc& flare) {
            mFlares.PushBack(flare);
        }

        void NkVFXRenderer::Render(NkICommandBuffer* cmd,
                                   const NkCamera& camera,
                                   const NkMat4f& view, const NkMat4f& proj,
                                   NkDepthStencilDesc dsState) {
            (void)camera;
            (void)dsState;
            if (!cmd || !mDevice) return;

            const NkMat4f vp = proj * view;
            if (mCameraUBO.IsValid()) {
                VFXCameraUBOData ubo{};
                ubo.view = view;
                ubo.proj = proj;
                ubo.viewProj = vp;
                ubo.cameraPos = {camera.position.x, camera.position.y, camera.position.z, camera.nearPlane};
                ubo.ambient = {0.2f, 0.2f, 0.2f, 1.0f};
                mDevice->WriteBuffer(mCameraUBO, &ubo, sizeof(ubo), 0);
            }

            for (auto* sys : mSystems) {
                RenderSystem(cmd, sys, vp);
            }
            for (auto* trail : mTrails) {
                RenderTrail(cmd, trail, vp);
            }
            RenderDecals(cmd);
            RenderFlares(cmd, camera);
        }
        NkParticleSystem* NkVFXRenderer::CreateFire(const NkVec3f& pos, float scale) {
            auto* sys = new NkParticleSystem();
            NkParticleSystemDesc desc = MakeFireDesc(scale);
            if (!sys->Create(mDevice, desc)) {
                delete sys;
                return nullptr;
            }
            sys->SetPosition(pos);
            sys->Play();
            AddSystem(sys);
            gOwnedSystems[this].insert(sys);
            return sys;
        }

        NkParticleSystem* NkVFXRenderer::CreateSmoke(const NkVec3f& pos, float scale) {
            auto* sys = new NkParticleSystem();
            NkParticleSystemDesc desc = MakeSmokeDesc(scale);
            if (!sys->Create(mDevice, desc)) {
                delete sys;
                return nullptr;
            }
            sys->SetPosition(pos);
            sys->Play();
            AddSystem(sys);
            gOwnedSystems[this].insert(sys);
            return sys;
        }

        NkParticleSystem* NkVFXRenderer::CreateExplosion(const NkVec3f& pos, float scale) {
            auto* sys = new NkParticleSystem();
            NkParticleSystemDesc desc = MakeExplodeDesc(scale);
            if (!sys->Create(mDevice, desc)) {
                delete sys;
                return nullptr;
            }
            sys->SetPosition(pos);
            sys->Emit((uint32)(200 * NkMax(0.1f, scale)));
            AddSystem(sys);
            gOwnedSystems[this].insert(sys);
            return sys;
        }

        NkParticleSystem* NkVFXRenderer::CreateSparks(const NkVec3f& pos, float scale) {
            auto* sys = new NkParticleSystem();
            NkParticleSystemDesc desc = MakeSparksDesc(scale);
            if (!sys->Create(mDevice, desc)) {
                delete sys;
                return nullptr;
            }
            sys->SetPosition(pos);
            sys->Play();
            AddSystem(sys);
            gOwnedSystems[this].insert(sys);
            return sys;
        }

        NkParticleSystem* NkVFXRenderer::CreateMagic(const NkVec3f& pos, const NkColor4f& color, float scale) {
            auto* sys = new NkParticleSystem();
            NkParticleSystemDesc desc = MakeMagicDesc(color, scale);
            if (!sys->Create(mDevice, desc)) {
                delete sys;
                return nullptr;
            }
            sys->SetPosition(pos);
            sys->Play();
            AddSystem(sys);
            gOwnedSystems[this].insert(sys);
            return sys;
        }

        NkParticleSystem* NkVFXRenderer::CreateRain(const NkVec3f& center, float area) {
            auto* sys = new NkParticleSystem();
            NkParticleSystemDesc desc = MakeRainDesc(area);
            if (!sys->Create(mDevice, desc)) {
                delete sys;
                return nullptr;
            }
            sys->SetPosition(center);
            sys->Play();
            AddSystem(sys);
            gOwnedSystems[this].insert(sys);
            return sys;
        }

        NkParticleSystem* NkVFXRenderer::CreateSnow(const NkVec3f& center, float area) {
            auto* sys = new NkParticleSystem();
            NkParticleSystemDesc desc = MakeSnowDesc(area);
            if (!sys->Create(mDevice, desc)) {
                delete sys;
                return nullptr;
            }
            sys->SetPosition(center);
            sys->Play();
            AddSystem(sys);
            gOwnedSystems[this].insert(sys);
            return sys;
        }

        NkParticleSystem* NkVFXRenderer::CreateDust(const NkVec3f& pos) {
            return CreateSmoke(pos, 0.35f);
        }

        bool NkVFXRenderer::InitShaders(NkRenderPassHandle rp) {
            (void)rp;
            if (!mDevice) return false;

            const bool useVulkanGLSL = (mDevice->GetApi() == NkGraphicsApi::NK_GFX_API_VULKAN);
            const char* vsrc = useVulkanGLSL ? NkBuiltinShaders::Basic3DVertVK() : NkBuiltinShaders::Basic3DVert();
            const char* fsrc = useVulkanGLSL ? NkBuiltinShaders::Basic3DFragVK() : NkBuiltinShaders::Basic3DFrag();

            NkShaderDesc particle{};
            particle.debugName = "NkVFXRenderer.Particle";
            particle.AddGLSL(NkShaderStage::NK_VERTEX, vsrc, "main");
            particle.AddGLSL(NkShaderStage::NK_FRAGMENT, fsrc, "main");
            particle.AddHLSL(NkShaderStage::NK_VERTEX, NkBuiltinShaders::Basic3DVertHLSL(), "VSMain");
            particle.AddHLSL(NkShaderStage::NK_FRAGMENT, NkBuiltinShaders::Basic3DFragHLSL(), "PSMain");
            particle.AddMSL(NkShaderStage::NK_VERTEX, NkBuiltinShaders::Basic3DVertMSL(), "VSMain");
            particle.AddMSL(NkShaderStage::NK_FRAGMENT, NkBuiltinShaders::Basic3DFragMSL(), "PSMain");
            mParticleShader = mDevice->CreateShader(particle);
            if (!mParticleShader.IsValid()) {
                return false;
            }

            NkShaderDesc trail{};
            trail.debugName = "NkVFXRenderer.Trail";
            trail.AddGLSL(NkShaderStage::NK_VERTEX, vsrc, "main");
            trail.AddGLSL(NkShaderStage::NK_FRAGMENT, fsrc, "main");
            trail.AddHLSL(NkShaderStage::NK_VERTEX, NkBuiltinShaders::Basic3DVertHLSL(), "VSMain");
            trail.AddHLSL(NkShaderStage::NK_FRAGMENT, NkBuiltinShaders::Basic3DFragHLSL(), "PSMain");
            trail.AddMSL(NkShaderStage::NK_VERTEX, NkBuiltinShaders::Basic3DVertMSL(), "VSMain");
            trail.AddMSL(NkShaderStage::NK_FRAGMENT, NkBuiltinShaders::Basic3DFragMSL(), "PSMain");
            mTrailShader = mDevice->CreateShader(trail);
            return mTrailShader.IsValid();
        }

        bool NkVFXRenderer::InitPipelines(NkRenderPassHandle rp) {
            if (!mDevice || !rp.IsValid() || !mParticleShader.IsValid() || !mTrailShader.IsValid()) {
                return false;
            }

            NkDescriptorSetLayoutDesc layout{};
            layout.debugName = "NkVFXRenderer.Layout";
            layout.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_VERTEX | NkShaderStage::NK_FRAGMENT);
            mLayout = mDevice->CreateDescriptorSetLayout(layout);
            if (!mLayout.IsValid()) return false;

            mDescSet = mDevice->AllocateDescriptorSet(mLayout);
            if (!mDescSet.IsValid()) return false;

            NkDescriptorWrite write{};
            write.set = mDescSet;
            write.binding = 0;
            write.type = NkDescriptorType::NK_UNIFORM_BUFFER;
            write.buffer = mCameraUBO;
            write.bufferRange = sizeof(VFXCameraUBOData);
            mDevice->UpdateDescriptorSets(&write, 1);

            const NkVertexLayout vtx = NkVertex3D::GetLayout();

            NkGraphicsPipelineDesc pAdd{};
            pAdd.shader = mParticleShader;
            pAdd.vertexLayout = vtx;
            pAdd.topology = NkPrimitiveTopology::NK_POINT_LIST;
            pAdd.rasterizer = NkRasterizerDesc::NoCull();
            pAdd.rasterizer.scissorTest = true;
            pAdd.depthStencil = NkDepthStencilDesc::ReadOnly();
            pAdd.blend = NkBlendDesc::Additive();
            pAdd.renderPass = rp;
            pAdd.descriptorSetLayouts.PushBack(mLayout);
            pAdd.debugName = "NkVFXRenderer.ParticleAdd";
            mParticlePipeAdd = mDevice->CreateGraphicsPipeline(pAdd);

            NkGraphicsPipelineDesc pAlpha = pAdd;
            pAlpha.blend = NkBlendDesc::Alpha();
            pAlpha.debugName = "NkVFXRenderer.ParticleAlpha";
            mParticlePipeAlpha = mDevice->CreateGraphicsPipeline(pAlpha);

            NkGraphicsPipelineDesc pTrail{};
            pTrail.shader = mTrailShader;
            pTrail.vertexLayout = vtx;
            pTrail.topology = NkPrimitiveTopology::NK_LINE_LIST;
            pTrail.rasterizer = NkRasterizerDesc::NoCull();
            pTrail.rasterizer.scissorTest = true;
            pTrail.depthStencil = NkDepthStencilDesc::ReadOnly();
            pTrail.blend = NkBlendDesc::Alpha();
            pTrail.renderPass = rp;
            pTrail.descriptorSetLayouts.PushBack(mLayout);
            pTrail.debugName = "NkVFXRenderer.Trail";
            mTrailPipeline = mDevice->CreateGraphicsPipeline(pTrail);

            return mParticlePipeAdd.IsValid() && mParticlePipeAlpha.IsValid() && mTrailPipeline.IsValid();
        }

        bool NkVFXRenderer::InitDefaultTextures() {
            if (!mDevice) return false;
            mDefaultParticleTex = new NkTexture2D();
            if (!mDefaultParticleTex->CreateWhite(mDevice)) {
                delete mDefaultParticleTex;
                mDefaultParticleTex = nullptr;
                return false;
            }
            return true;
        }

        void NkVFXRenderer::RenderSystem(NkICommandBuffer* cmd, NkParticleSystem* sys, const NkMat4f& viewProj) {
            (void)viewProj;
            if (!cmd || !sys) return;
            if (!sys->GetParticleVBO().IsValid() || sys->GetAliveCount() == 0) return;

            NkPipelineHandle pipe{};
            if (sys->GetDesc().blendMode == NkBlendMode::NK_ADDITIVE) pipe = mParticlePipeAdd;
            else pipe = mParticlePipeAlpha.IsValid() ? mParticlePipeAlpha : mParticlePipeAdd;
            if (!pipe.IsValid() || !mDescSet.IsValid()) return;

            cmd->BindGraphicsPipeline(pipe);
            cmd->BindDescriptorSet(mDescSet, 0);
            cmd->BindVertexBuffer(0, sys->GetParticleVBO(), 0);
            cmd->Draw(sys->GetAliveCount(), 1, 0, 0);
        }

        void NkVFXRenderer::RenderTrail(NkICommandBuffer* cmd, NkTrail* trail, const NkMat4f& viewProj) {
            (void)viewProj;
            if (!cmd || !trail) return;
            if (!trail->GetVBO().IsValid() || trail->GetVertCount() == 0) return;
            if (!mTrailPipeline.IsValid() || !mDescSet.IsValid()) return;
            cmd->BindGraphicsPipeline(mTrailPipeline);
            cmd->BindDescriptorSet(mDescSet, 0);
            cmd->BindVertexBuffer(0, trail->GetVBO(), 0);
            cmd->Draw(trail->GetVertCount(), 1, 0, 0);
        }

        void NkVFXRenderer::RenderDecals(NkICommandBuffer* cmd) {
            (void)cmd;
            for (const auto& decal : mDecals) {
                (void)decal;
            }
        }

        void NkVFXRenderer::RenderFlares(NkICommandBuffer* cmd, const NkCamera& cam) {
            (void)cmd;
            (void)cam;
            for (const auto& flare : mFlares) {
                (void)flare;
            }
        }

        NkParticleSystemDesc NkVFXRenderer::MakeFireDesc(float scale) const {
            NkParticleSystemDesc d{};
            d.name = "VFX_Fire";
            d.maxParticles = (uint32)(NkMax(64.0f, 700.0f * scale));
            d.emitter.shape = NkEmitterShape::NK_CONE;
            d.emitter.radius = NkMax(0.03f, 0.18f * scale);
            d.emitter.rateOverTime = 180.0f * NkMax(0.1f, scale);
            d.emitter.coneAngle = 22.0f;
            d.emitter.randomDirection = false;
            d.emitter.spread = 0.35f;
            d.minLife = 0.25f;
            d.maxLife = 0.85f;
            d.minSpeed = 0.35f * scale;
            d.maxSpeed = 1.8f * scale;
            d.direction = {0.0f, 1.0f, 0.0f};
            d.gravity = {0.0f, -0.5f, 0.0f};
            d.gravityScale = 0.2f;
            d.drag = 0.45f;
            d.turbulence = 0.3f * scale;
            d.startSize = 0.08f * scale;
            d.endSize = 0.02f * scale;
            d.sizeCurve = NkCurve::EaseOut(0.0f, 1.0f);
            d.colorOverLife = NkColorGradient::FireColors();
            d.blendMode = NkBlendMode::NK_ADDITIVE;
            d.sortByDepth = false;
            d.loop = true;
            d.duration = 0.0f;
            d.texture = mDefaultParticleTex;
            return d;
        }

        NkParticleSystemDesc NkVFXRenderer::MakeSmokeDesc(float scale) const {
            NkParticleSystemDesc d{};
            d.name = "VFX_Smoke";
            d.maxParticles = (uint32)(NkMax(64.0f, 500.0f * scale));
            d.emitter.shape = NkEmitterShape::NK_SPHERE;
            d.emitter.radius = NkMax(0.03f, 0.16f * scale);
            d.emitter.rateOverTime = 85.0f * NkMax(0.1f, scale);
            d.emitter.randomDirection = false;
            d.emitter.spread = 0.7f;
            d.minLife = 1.0f;
            d.maxLife = 2.4f;
            d.minSpeed = 0.1f * scale;
            d.maxSpeed = 0.55f * scale;
            d.direction = {0.0f, 1.0f, 0.0f};
            d.gravity = {0.0f, -0.35f, 0.0f};
            d.gravityScale = 0.08f;
            d.drag = 0.75f;
            d.turbulence = 0.15f * scale;
            d.startSize = 0.12f * scale;
            d.endSize = 0.45f * scale;
            d.sizeCurve = NkCurve::Linear(0.0f, 1.0f);
            d.colorOverLife = NkColorGradient::SmokeColors();
            d.blendMode = NkBlendMode::NK_TRANSLUCENT;
            d.sortByDepth = true;
            d.loop = true;
            d.texture = mDefaultParticleTex;
            return d;
        }

        NkParticleSystemDesc NkVFXRenderer::MakeExplodeDesc(float scale) const {
            NkParticleSystemDesc d{};
            d.name = "VFX_Explosion";
            d.maxParticles = (uint32)(NkMax(96.0f, 1200.0f * scale));
            d.emitter.shape = NkEmitterShape::NK_SPHERE;
            d.emitter.radius = NkMax(0.05f, 0.28f * scale);
            d.emitter.rateOverTime = 0.0f;
            d.emitter.randomDirection = true;
            d.minLife = 0.18f;
            d.maxLife = 1.1f;
            d.minSpeed = 1.4f * scale;
            d.maxSpeed = 6.5f * scale;
            d.direction = {0.0f, 1.0f, 0.0f};
            d.gravity = {0.0f, -9.8f, 0.0f};
            d.gravityScale = 0.15f;
            d.drag = 0.28f;
            d.turbulence = 1.1f * scale;
            d.startSize = 0.1f * scale;
            d.endSize = 0.03f * scale;
            d.sizeCurve = NkCurve::EaseOut(0.0f, 1.0f);
            d.colorOverLife = NkColorGradient::FireColors();
            d.rotationSpeed = 240.0f;
            d.blendMode = NkBlendMode::NK_ADDITIVE;
            d.sortByDepth = false;
            d.loop = false;
            d.duration = 0.25f;
            d.texture = mDefaultParticleTex;
            return d;
        }

        NkParticleSystemDesc NkVFXRenderer::MakeSparksDesc(float scale) const {
            NkParticleSystemDesc d{};
            d.name = "VFX_Sparks";
            d.maxParticles = (uint32)(NkMax(64.0f, 600.0f * scale));
            d.emitter.shape = NkEmitterShape::NK_POINT;
            d.emitter.rateOverTime = 120.0f * NkMax(0.1f, scale);
            d.emitter.randomDirection = false;
            d.emitter.spread = 0.9f;
            d.minLife = 0.2f;
            d.maxLife = 0.8f;
            d.minSpeed = 3.0f * scale;
            d.maxSpeed = 10.0f * scale;
            d.direction = {0.0f, 1.0f, 0.0f};
            d.gravity = {0.0f, -9.8f, 0.0f};
            d.gravityScale = 1.0f;
            d.drag = 0.12f;
            d.turbulence = 0.2f * scale;
            d.startSize = 0.03f * scale;
            d.endSize = 0.0f;
            d.sizeCurve = NkCurve::EaseOut(1.0f, 0.0f);
            d.colorOverLife.keys = {
                {0.0f, {1.0f, 1.0f, 0.8f, 1.0f}},
                {0.5f, {1.0f, 0.6f, 0.2f, 0.8f}},
                {1.0f, {0.7f, 0.1f, 0.0f, 0.0f}}
            };
            d.rotationSpeed = 540.0f;
            d.blendMode = NkBlendMode::NK_ADDITIVE;
            d.sortByDepth = false;
            d.loop = true;
            d.texture = mDefaultParticleTex;
            d.collisions = true;
            d.bounciness = 0.35f;
            d.lifetimeLoss = 0.2f;
            return d;
        }

        NkParticleSystemDesc NkVFXRenderer::MakeMagicDesc(const NkColor4f& col, float scale) const {
            NkParticleSystemDesc d{};
            d.name = "VFX_Magic";
            d.maxParticles = (uint32)(NkMax(96.0f, 900.0f * scale));
            d.emitter.shape = NkEmitterShape::NK_SPHERE;
            d.emitter.radius = NkMax(0.06f, 0.25f * scale);
            d.emitter.rateOverTime = 130.0f * NkMax(0.1f, scale);
            d.emitter.randomDirection = true;
            d.minLife = 0.45f;
            d.maxLife = 1.9f;
            d.minSpeed = 0.2f * scale;
            d.maxSpeed = 1.7f * scale;
            d.direction = {0.0f, 1.0f, 0.0f};
            d.gravity = {0.0f, -0.3f, 0.0f};
            d.gravityScale = 0.12f;
            d.drag = 0.35f;
            d.turbulence = 0.6f * scale;
            d.startSize = 0.06f * scale;
            d.endSize = 0.01f * scale;
            d.sizeCurve = NkCurve::EaseOut(0.0f, 1.0f);
            d.colorOverLife.keys = {
                {0.0f,  {col.r, col.g, col.b, 0.0f}},
                {0.25f, {col.r, col.g, col.b, 1.0f}},
                {1.0f,  {col.r * 0.2f, col.g * 0.2f, col.b * 0.2f, 0.0f}}
            };
            d.rotationSpeed = 120.0f;
            d.blendMode = NkBlendMode::NK_ADDITIVE;
            d.sortByDepth = false;
            d.loop = true;
            d.texture = mDefaultParticleTex;
            return d;
        }

        NkParticleSystemDesc NkVFXRenderer::MakeRainDesc(float area) const {
            NkParticleSystemDesc d{};
            d.name = "VFX_Rain";
            const float safeArea = NkMax(1.0f, area);
            d.maxParticles = (uint32)NkClamp(safeArea * safeArea * 20.0f, 500.0f, 20000.0f);
            d.emitter.shape = NkEmitterShape::NK_BOX;
            d.emitter.boxSize = {safeArea, 0.5f, safeArea};
            d.emitter.position = {0.0f, 5.0f, 0.0f};
            d.emitter.rateOverTime = (float)d.maxParticles * 0.7f;
            d.emitter.randomDirection = false;
            d.emitter.spread = 0.03f;
            d.minLife = 0.4f;
            d.maxLife = 1.1f;
            d.minSpeed = 7.0f;
            d.maxSpeed = 14.0f;
            d.direction = {0.0f, -1.0f, 0.0f};
            d.gravity = {0.0f, -9.8f, 0.0f};
            d.gravityScale = 0.4f;
            d.drag = 0.03f;
            d.startSize = 0.015f;
            d.endSize = 0.01f;
            d.sizeCurve = NkCurve::Linear(0.0f, 1.0f);
            d.colorOverLife.keys = {
                {0.0f, {0.7f, 0.8f, 1.0f, 0.0f}},
                {0.1f, {0.7f, 0.8f, 1.0f, 0.75f}},
                {1.0f, {0.7f, 0.8f, 1.0f, 0.0f}}
            };
            d.blendMode = NkBlendMode::NK_TRANSLUCENT;
            d.sortByDepth = true;
            d.loop = true;
            d.texture = mDefaultParticleTex;
            d.collisions = true;
            d.bounciness = 0.05f;
            d.lifetimeLoss = 1.0f;
            return d;
        }

        NkParticleSystemDesc NkVFXRenderer::MakeSnowDesc(float area) const {
            NkParticleSystemDesc d{};
            d.name = "VFX_Snow";
            const float safeArea = NkMax(1.0f, area);
            d.maxParticles = (uint32)NkClamp(safeArea * safeArea * 8.0f, 300.0f, 12000.0f);
            d.emitter.shape = NkEmitterShape::NK_BOX;
            d.emitter.boxSize = {safeArea, 0.5f, safeArea};
            d.emitter.position = {0.0f, 4.0f, 0.0f};
            d.emitter.rateOverTime = (float)d.maxParticles * 0.35f;
            d.emitter.randomDirection = false;
            d.emitter.spread = 0.35f;
            d.minLife = 2.5f;
            d.maxLife = 6.0f;
            d.minSpeed = 0.2f;
            d.maxSpeed = 1.3f;
            d.direction = {0.0f, -1.0f, 0.0f};
            d.gravity = {0.0f, -1.5f, 0.0f};
            d.gravityScale = 0.4f;
            d.drag = 0.25f;
            d.turbulence = 0.25f;
            d.startSize = 0.04f;
            d.endSize = 0.02f;
            d.sizeCurve = NkCurve::Linear(0.0f, 1.0f);
            d.colorOverLife.keys = {
                {0.0f, {1.0f, 1.0f, 1.0f, 0.0f}},
                {0.2f, {1.0f, 1.0f, 1.0f, 0.9f}},
                {1.0f, {1.0f, 1.0f, 1.0f, 0.0f}}
            };
            d.blendMode = NkBlendMode::NK_TRANSLUCENT;
            d.sortByDepth = true;
            d.loop = true;
            d.texture = mDefaultParticleTex;
            d.collisions = true;
            d.bounciness = 0.03f;
            d.lifetimeLoss = 0.4f;
            return d;
        }

    } // namespace renderer
} // namespace nkentseu
