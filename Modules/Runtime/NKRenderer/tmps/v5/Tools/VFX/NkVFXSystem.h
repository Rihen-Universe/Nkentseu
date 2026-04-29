#pragma once
// =============================================================================
// NkVFXSystem.h  — NKRenderer v4.0  (Tools/VFX/)
// Particules CPU/GPU, trails, decals projetés, lens flares.
// =============================================================================
#include "../../Core/NkRendererTypes.h"
#include "../../Core/NkTextureLibrary.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKContainers/Associative/NkHashMap.h"

namespace nkentseu {
namespace renderer {

    class NkMeshSystem;

    // =========================================================================
    // Descripteur d'émetteur
    // =========================================================================
    enum class NkEmitterShape : uint8 {
        POINT, SPHERE, BOX, CONE, DISK, EDGE,
    };
    enum class NkSimMode    : uint8 { CPU, GPU };

    struct NkEmitterDesc {
        NkEmitterShape  shape        = NkEmitterShape::POINT;
        NkVec3f         position     = {0,0,0};
        float32         radius       = 1.f;
        NkVec3f         boxSize      = {1,1,1};
        float32         coneAngle    = 25.f;
        float32         ratePerSec   = 20.f;
        float32         burstCount   = 0.f;
        float32         lifeMin      = 1.f;
        float32         lifeMax      = 2.5f;
        float32         speedMin     = 1.f;
        float32         speedMax     = 4.f;
        float32         sizeStart    = 0.15f;
        float32         sizeEnd      = 0.f;
        NkVec4f         colorStart   = {1,1,1,1};
        NkVec4f         colorEnd     = {1,1,1,0};
        NkVec3f         gravity      = {0,-9.8f,0};
        NkVec3f         velocityDir  = {0,1,0};   // direction initiale
        float32         velocityRand = 1.f;        // 0=précis, 1=aléatoire
        NkTexHandle     texture;
        NkBlendMode     blend        = NkBlendMode::NK_ADDITIVE;
        NkSimMode       simMode      = NkSimMode::CPU;
        uint32          maxParticles = 1000;
        bool            worldSpace   = true;
        bool            loop         = true;
    };

    struct NkEmitterId { uint64 id=0; bool IsValid() const{return id!=0;} };

    // =========================================================================
    // Trail
    // =========================================================================
    struct NkTrailDesc {
        float32     width       = 0.1f;
        float32     lifetime    = 0.5f;
        uint32      maxPoints   = 64;
        NkVec4f     colorStart  = {1,1,1,1};
        NkVec4f     colorEnd    = {1,1,1,0};
        NkTexHandle texture;
        NkBlendMode blend       = NkBlendMode::NK_ADDITIVE;
        float32     minDistance = 0.05f;  // distance min entre deux points
    };

    struct NkTrailId { uint64 id=0; bool IsValid() const{return id!=0;} };

    // =========================================================================
    // Decal
    // =========================================================================
    struct NkDecalDesc {
        NkMat4f     transform;      // position, orientation, taille
        NkTexHandle albedo;
        NkTexHandle normal;
        float32     opacity       = 1.f;
        float32     normalBlend   = 0.5f;
        float32     lifetime      = -1.f;   // -1 = permanent
        bool        fadeOut       = true;
        uint32      layerMask     = 0xFFFFFFFF;
    };

    struct NkDecalId { uint64 id=0; bool IsValid() const{return id!=0;} };

    // =========================================================================
    // NkVFXSystem
    // =========================================================================
    class NkVFXSystem {
    public:
        NkVFXSystem() = default;
        ~NkVFXSystem();

        bool Init(NkIDevice* device, NkTextureLibrary* texLib, NkMeshSystem* mesh);
        void Shutdown();

        // ── Émetteurs ─────────────────────────────────────────────────────────
        NkEmitterId CreateEmitter   (const NkEmitterDesc& desc);
        void        DestroyEmitter  (NkEmitterId& id);
        void        SetEmitterPos   (NkEmitterId id, NkVec3f pos);
        void        SetEmitterEnabled(NkEmitterId id, bool on);
        void        Burst           (NkEmitterId id, uint32 count = 0);
        NkEmitterDesc* GetEmitterDesc(NkEmitterId id);

        // ── Trails ────────────────────────────────────────────────────────────
        NkTrailId   CreateTrail     (const NkTrailDesc& desc);
        void        DestroyTrail    (NkTrailId& id);
        void        AddTrailPoint   (NkTrailId id, NkVec3f point);
        void        ClearTrail      (NkTrailId id);

        // ── Decals ────────────────────────────────────────────────────────────
        NkDecalId   SpawnDecal      (const NkDecalDesc& desc);
        void        DestroyDecal    (NkDecalId& id);

        // ── Update & Render ───────────────────────────────────────────────────
        void Update(float32 dt, const NkCamera3DData& cam);
        void Render(NkICommandBuffer* cmd, const NkCamera3DData& cam);

        // ── Stats ──────────────────────────────────────────────────────────────
        uint32 GetActiveParticleCount() const { return mTotalParticles; }
        uint32 GetActiveEmitterCount()  const { return (uint32)mEmitters.Size(); }

    private:
        // ── Particule (CPU) ───────────────────────────────────────────────────
        struct Particle {
            NkVec3f pos, vel;
            NkVec4f color;
            float32 size, life, maxLife, rotation, rotSpeed;
            bool    alive = false;
        };

        struct Emitter {
            NkEmitterId    id;
            NkEmitterDesc  desc;
            NkVector<Particle> particles;
            float32        spawnAccum = 0.f;
            bool           enabled    = true;
            NkBufferHandle vbo;   // GPU billboard VBO
            uint32         aliveCount = 0;
        };

        struct TrailPoint { NkVec3f pos; float32 time; };
        struct Trail {
            NkTrailId      id;
            NkTrailDesc    desc;
            NkVector<TrailPoint> points;
            NkBufferHandle vbo;
        };

        struct Decal {
            NkDecalId   id;
            NkDecalDesc desc;
            float32     age = 0.f;
        };

        NkIDevice*        mDevice  = nullptr;
        NkTextureLibrary* mTexLib  = nullptr;
        NkMeshSystem*     mMesh    = nullptr;

        NkVector<Emitter*> mEmitters;
        NkVector<Trail*>   mTrails;
        NkVector<Decal*>   mDecals;
        uint64             mNextId  = 1;
        uint32             mTotalParticles = 0;

        NkPipelineHandle   mPipeParticle;
        NkPipelineHandle   mPipeTrail;
        NkPipelineHandle   mPipeDecal;

        void UpdateEmitter(Emitter* e, float32 dt, const NkCamera3DData& cam);
        void SpawnParticle(Emitter* e);
        void RenderEmitter(NkICommandBuffer* cmd, Emitter* e, const NkCamera3DData& cam);
        void UpdateTrail  (Trail*   t, float32 dt);
        void RenderTrail  (NkICommandBuffer* cmd, Trail* t, const NkCamera3DData& cam);
        void RenderDecals (NkICommandBuffer* cmd);
    };

} // namespace renderer
} // namespace nkentseu
