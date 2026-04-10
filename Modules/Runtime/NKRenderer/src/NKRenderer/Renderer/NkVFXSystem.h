#pragma once
// =============================================================================
// NkVFXSystem.h
// Système de VFX / Particules du NKRenderer.
//
// Fonctionnalités :
//   • CPU Particles — flexibles, comportements complexes
//   • GPU Particles — millions de particules via compute shader
//   • Trails / rubans (pour épées, magie, véhicules)
//   • Decals projetés
//   • Lens Flares
//   • Lumières volumétriques (rayons divins)
// =============================================================================
#include "NKRenderer/Core/NkRenderTypes.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Resources/NkMesh.h"
#include "NKRenderer/Resources/NkMaterial.h"
#include "NKRenderer/Resources/NkTexture.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        // Module d'émission
        // =============================================================================
        enum class NkEmitterShape : uint32 {
            NK_POINT,
            NK_SPHERE,
            NK_BOX,
            NK_CONE,
            NK_DISK,
            NK_EDGE,
            NK_MESH
        };

        struct NkEmitterModule {
            NkEmitterShape shape       = NkEmitterShape::NK_POINT;
            NkVec3f        position    = {0,0,0};
            NkVec3f        rotation    = {0,0,0};
            float          radius      = 1.f;
            NkVec3f        boxSize     = {1,1,1};
            float          coneAngle   = 25.f;
            float          rateOverTime= 10.f;  // particules/seconde
            float          burst       = 0.f;   // burst instantané
            bool           randomDirection = true;
            float          spread      = 0.1f;  // dispersion directionnelle
        };

        // =============================================================================
        // Particule individuelle
        // =============================================================================
        struct NkParticle {
            NkVec3f  position;
            NkVec3f  velocity;
            NkVec3f  acceleration = {0,-9.8f,0};
            NkColor4f color       = {1,1,1,1};
            NkColor4f colorEnd    = {1,1,1,0};
            float    size         = 0.1f;
            float    sizeEnd      = 0.f;
            float    rotation     = 0.f;
            float    rotationSpeed= 0.f;
            float    life         = 1.f;
            float    maxLife      = 1.f;
            float    age          = 0.f;        // age/maxLife [0..1]
            uint32   frameIndex   = 0;          // pour sprite sheet animé
            bool     alive        = false;
        };

        // =============================================================================
        // Courbe (gradient / curve over lifetime)
        // =============================================================================
        struct NkCurveKey {
            float t, value;
        };

        struct NkCurve {
            NkVector<NkCurveKey> keys;

            float Evaluate(float t) const;

            static NkCurve Constant(float v) {
                NkCurve c; c.keys.PushBack({0.f, v}); c.keys.PushBack({1.f, v}); return c;
            }
            static NkCurve Linear(float from, float to) {
                NkCurve c; c.keys.PushBack({0.f, from}); c.keys.PushBack({1.f, to}); return c;
            }
            static NkCurve EaseOut(float from = 1.f, float to = 0.f) {
                NkCurve c;
                c.keys.PushBack({0.f, from});
                c.keys.PushBack({0.5f, (from+to)*0.6f});
                c.keys.PushBack({1.f, to});
                return c;
            }
        };

        // =============================================================================
        // Gradient couleur over lifetime
        // =============================================================================
        struct NkGradientKey {
            float     t;
            NkColor4f color;
        };

        struct NkColorGradient {
            NkVector<NkGradientKey> keys;
            NkColor4f Evaluate(float t) const;

            static NkColorGradient WhiteToTransparent() {
                NkColorGradient g;
                g.keys.PushBack({0.f, {1,1,1,1}});
                g.keys.PushBack({1.f, {1,1,1,0}});
                return g;
            }
            static NkColorGradient FireColors() {
                NkColorGradient g;
                g.keys.PushBack({0.f,   {1.f, 0.9f, 0.1f, 1.f}});
                g.keys.PushBack({0.3f,  {1.f, 0.4f, 0.f,  1.f}});
                g.keys.PushBack({0.7f,  {0.5f,0.1f, 0.f,  0.7f}});
                g.keys.PushBack({1.f,   {0.1f,0.1f, 0.1f, 0.f}});
                return g;
            }
            static NkColorGradient SmokeColors() {
                NkColorGradient g;
                g.keys.PushBack({0.f, {0.7f,0.7f,0.7f,0.f}});
                g.keys.PushBack({0.2f,{0.5f,0.5f,0.5f,0.4f}});
                g.keys.PushBack({0.8f,{0.3f,0.3f,0.3f,0.2f}});
                g.keys.PushBack({1.f, {0.1f,0.1f,0.1f,0.f}});
                return g;
            }
        };

        // =============================================================================
        // Système de particules CPU
        // =============================================================================
        struct NkParticleSystemDesc {
            NkString   name;
            uint32     maxParticles  = 1000;

            // Émission
            NkEmitterModule emitter;

            // Durée de vie
            float      minLife       = 0.5f;
            float      maxLife       = 2.f;

            // Vitesse initiale
            float      minSpeed      = 1.f;
            float      maxSpeed      = 3.f;
            NkVec3f    direction     = {0, 1, 0};

            // Forces
            NkVec3f    gravity       = {0,-9.8f, 0};
            float      gravityScale  = 1.f;
            float      drag          = 0.f;          // amortissement aérien
            float      turbulence    = 0.f;          // bruit de Perlin

            // Taille
            float      startSize     = 0.1f;
            float      endSize       = 0.f;
            NkCurve    sizeCurve     = NkCurve::EaseOut(1.f, 0.f);

            // Couleur
            NkColorGradient colorOverLife = NkColorGradient::WhiteToTransparent();

            // Rotation
            float      rotationSpeed = 0.f;          // degrés/s
            bool       alignToVelocity = false;

            // Sprite sheet
            NkTexture2D* texture     = nullptr;
            uint32       sheetCols   = 1;
            uint32       sheetRows   = 1;
            float        animFPS     = 12.f;
            bool         randomStartFrame = true;

            // Rendu
            NkBlendMode blendMode    = NkBlendMode::NK_ADDITIVE;
            bool        sortByDepth  = false;        // nécessaire pour alpha correct
            bool        castShadow   = false;
            float       emissiveBoost= 1.f;

            // Simulation
            bool        worldSpace   = true;         // false = espace local
            bool        loop         = true;
            float       duration     = 5.f;          // si !loop
            float       startDelay   = 0.f;
            float       warmupTime   = 0.f;          // simuler N secondes au départ

            // Collisions (optionnel)
            bool        collisions   = false;
            float       bounciness   = 0.3f;
            float       lifetimeLoss = 0.1f;         // perte de vie sur collision
        };

        class NkParticleSystem {
            public:
                NkParticleSystem()  = default;
                ~NkParticleSystem() { Destroy(); }
                NkParticleSystem(const NkParticleSystem&) = delete;
                NkParticleSystem& operator=(const NkParticleSystem&) = delete;

                bool Create(NkIDevice* device, const NkParticleSystemDesc& desc);
                void Destroy();

                // ── Simulation ────────────────────────────────────────────────────────────
                void Update(float dt);
                void Play()    { mPlaying = true;  mTime = 0; }
                void Pause()   { mPlaying = false; }
                void Stop()    { mPlaying = false; mTime = 0; KillAll(); }
                void Restart() { Stop(); Play(); }
                void Emit(uint32 count = 1);

                // ── Position / Transform ──────────────────────────────────────────────────
                void SetPosition(const NkVec3f& p) { mWorldPos = p; }
                void SetRotation(const NkVec3f& r) { mWorldRot = r; }

                // ── Upload GPU ────────────────────────────────────────────────────────────
                void UploadToGPU();
                NkBufferHandle GetParticleVBO() const { return mVBO; }
                uint32         GetAliveCount()  const { return mAliveCount; }
                bool           IsAlive()        const { return mAliveCount > 0 || mPlaying; }
                bool           IsPlaying()      const { return mPlaying; }
                NkMaterial*    GetMaterial()    const { return mMaterial; }
                const NkParticleSystemDesc& GetDesc() const { return mDesc; }
                const NkAABB&  GetAABB()        const { return mAABB; }

            private:
                NkIDevice*            mDevice    = nullptr;
                NkParticleSystemDesc  mDesc;
                NkVector<NkParticle>  mParticles;
                uint32                mAliveCount= 0;
                float                 mTime      = 0.f;
                float                 mEmitAccum = 0.f;
                bool                  mPlaying   = false;
                NkVec3f               mWorldPos  = {0,0,0};
                NkVec3f               mWorldRot  = {0,0,0};
                NkBufferHandle        mVBO;       // billboards GPU [pos,size,color,uv]
                NkMaterial*           mMaterial  = nullptr;
                NkAABB                mAABB;

                void KillAll();
                void SpawnParticle();
                void SimulateParticle(NkParticle& p, float dt);
                NkVec3f SampleEmitterPos() const;
                NkVec3f SampleEmitterDir() const;
                float   RandRange(float a, float b) const;
                void    UpdateAABB();
        };

        // =============================================================================
        // Trail (ruban)
        // =============================================================================
        struct NkTrailDesc {
            uint32    maxPoints    = 64;
            float     lifetime     = 1.f;   // durée de vie d'un point
            float     minDistance  = 0.1f;  // distance min entre points
            float     startWidth   = 0.2f;
            float     endWidth     = 0.f;
            NkColorGradient colorOverLife = NkColorGradient::WhiteToTransparent();
            NkTexture2D*    texture = nullptr;
            NkBlendMode blendMode  = NkBlendMode::NK_ADDITIVE;
            bool      worldSpace   = true;
        };

        struct NkTrailPoint {
            NkVec3f  pos;
            float    age;
            float    maxAge;
            float    width;
            NkColor4f color;
        };

        class NkTrail {
            public:
                NkTrail()  = default;
                ~NkTrail() { Destroy(); }
                NkTrail(const NkTrail&) = delete;
                NkTrail& operator=(const NkTrail&) = delete;

                bool Create(NkIDevice* device, const NkTrailDesc& desc);
                void Destroy();

                void Update(float dt, const NkVec3f& emitterPos);
                void Clear();

                void UploadToGPU();
                NkBufferHandle GetVBO()       const { return mVBO; }
                uint32         GetVertCount() const { return mVertCount; }
                NkMaterial*    GetMaterial()  const { return mMaterial; }
                bool           HasGeometry()  const { return mVertCount > 0; }

            private:
                NkIDevice*        mDevice = nullptr;
                NkTrailDesc       mDesc;
                NkVector<NkTrailPoint> mPoints;
                NkBufferHandle    mVBO;
                uint32            mVertCount = 0;
                NkMaterial*       mMaterial = nullptr;
                NkVec3f           mLastPos  = {0,0,0};
                bool              mFirstPoint = true;
        };

        // =============================================================================
        // Decal projeté
        // =============================================================================
        struct NkDecalDesc {
            NkVec3f    position  = {0,0,0};
            NkVec3f    normal    = {0,1,0};  // surface normale
            NkVec3f    size      = {1,1,0.5f};
            NkMaterial* material = nullptr;
            float       fade     = 0.f;      // 0=permanent, >0=durée fade-out
            float       angle    = 0.f;      // rotation dans le plan
        };

        // =============================================================================
        // Lens Flare
        // =============================================================================
        struct NkFlareElement {
            NkTexture2D* texture   = nullptr;
            float        offset    = 0.f;   // position sur l'axe occulaire [0=source, 1=centre écran]
            float        size      = 0.1f;
            NkColor4f    color     = {1,1,0.8f,0.5f};
            float        rotation  = 0.f;
        };

        struct NkLensFlareDesc {
            NkVec3f  lightPos;
            NkColor4f color         = {1,1,0.8f,1};
            float     intensity     = 1.f;
            bool      occlusionTest = true;
            NkVector<NkFlareElement> elements;
        };

        // =============================================================================
        // NkVFXRenderer — rendu de tous les VFX
        // =============================================================================
        class NkVFXRenderer {
            public:
                NkVFXRenderer()  = default;
                ~NkVFXRenderer() { Shutdown(); }
                NkVFXRenderer(const NkVFXRenderer&) = delete;
                NkVFXRenderer& operator=(const NkVFXRenderer&) = delete;

                bool Init(NkIDevice* device, NkRenderPassHandle renderPass,
                        uint32 w, uint32 h);
                void Shutdown();
                void Resize(uint32 w, uint32 h);

                // ── Update (CPU sim) ──────────────────────────────────────────────────────
                void Update(float dt);

                // ── Enregistrement ────────────────────────────────────────────────────────
                void AddSystem(NkParticleSystem* sys);
                void RemoveSystem(NkParticleSystem* sys);
                void AddTrail(NkTrail* trail);
                void RemoveTrail(NkTrail* trail);
                void AddDecal(const NkDecalDesc& decal);
                void AddLensFlare(const NkLensFlareDesc& flare);

                // ── Rendu ─────────────────────────────────────────────────────────────────
                void Render(NkICommandBuffer* cmd,
                            const NkCamera& camera,
                            const NkMat4f& view, const NkMat4f& proj,
                            NkDepthStencilDesc dsState = NkDepthStencilDesc::ReadOnly());

                // ── Presets VFX ───────────────────────────────────────────────────────────
                NkParticleSystem* CreateFire(const NkVec3f& pos, float scale = 1.f);
                NkParticleSystem* CreateSmoke(const NkVec3f& pos, float scale = 1.f);
                NkParticleSystem* CreateExplosion(const NkVec3f& pos, float scale = 1.f);
                NkParticleSystem* CreateSparks(const NkVec3f& pos, float scale = 1.f);
                NkParticleSystem* CreateMagic(const NkVec3f& pos, const NkColor4f& color,
                                                float scale = 1.f);
                NkParticleSystem* CreateRain(const NkVec3f& center, float area = 10.f);
                NkParticleSystem* CreateSnow(const NkVec3f& center, float area = 10.f);
                NkParticleSystem* CreateDust(const NkVec3f& pos);

            private:
                NkIDevice*         mDevice = nullptr;
                uint32             mWidth  = 0;
                uint32             mHeight = 0;
                NkRenderPassHandle mRenderPass;

                NkVector<NkParticleSystem*> mSystems;
                NkVector<NkTrail*>          mTrails;
                NkVector<NkDecalDesc>       mDecals;
                NkVector<NkLensFlareDesc>   mFlares;

                NkShaderHandle   mParticleShader;
                NkShaderHandle   mTrailShader;
                NkPipelineHandle mParticlePipeAdd;    // additive
                NkPipelineHandle mParticlePipeAlpha;  // alpha blend
                NkPipelineHandle mTrailPipeline;

                NkDescSetHandle  mLayout;
                NkDescSetHandle  mDescSet;
                NkBufferHandle   mCameraUBO;

                NkTexture2D*     mDefaultParticleTex = nullptr;

                bool InitShaders(NkRenderPassHandle rp);
                bool InitPipelines(NkRenderPassHandle rp);
                bool InitDefaultTextures();

                void RenderSystem(NkICommandBuffer* cmd, NkParticleSystem* sys,
                                const NkMat4f& viewProj);
                void RenderTrail(NkICommandBuffer* cmd, NkTrail* trail,
                                const NkMat4f& viewProj);
                void RenderDecals(NkICommandBuffer* cmd);
                void RenderFlares(NkICommandBuffer* cmd, const NkCamera& cam);

                // Présets internes
                NkParticleSystemDesc MakeFireDesc   (float scale) const;
                NkParticleSystemDesc MakeSmokeDesc  (float scale) const;
                NkParticleSystemDesc MakeExplodeDesc(float scale) const;
                NkParticleSystemDesc MakeSparksDesc (float scale) const;
                NkParticleSystemDesc MakeMagicDesc  (const NkColor4f& col, float scale) const;
                NkParticleSystemDesc MakeRainDesc   (float area) const;
                NkParticleSystemDesc MakeSnowDesc   (float area) const;
        };

    } // namespace renderer
} // namespace nkentseu
