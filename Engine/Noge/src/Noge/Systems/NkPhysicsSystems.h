#pragma once
// =============================================================================
// Nkentseu/Physics/NkPhysicsSystems.h
// =============================================================================
// Systèmes ECS qui pilotent la simulation physique sur les meshes.
// Chaque système correspond à un composant de NkPhysicsMesh.h.
//
// ORDRE D'EXÉCUTION (groupe PostUpdate) :
//   1. NkJiggleBoneSystem  (prio 700) — os secondaires, rapide
//   2. NkClothSystem       (prio 600) — tissu GPU compute
//   3. NkHairSystem        (prio 550) — cheveux GPU compute
//   4. NkSoftBodySystem    (prio 500) — corps mous GPU compute
//   5. NkRagdollSystem     (prio 450) — ragdoll ↔ rigidbody bridge
//   6. NkMocapSystem       (prio 400) — playback mocap BVH
//   7. NkFacialSystem      (prio 800) — rig facial (dans NkFacialRig.h)
// =============================================================================

#include "NKECS/System/NkSystem.h"
#include "NKECS/World/NkWorld.h"
#include "NkPhysicsMesh.h"
#include "Nkentseu/ECS/Components/Animation/NkAnimation.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
    using namespace ecs;
    using namespace math;

    // =========================================================================
    // NkJiggleBoneSystem — os secondaires avec inertie
    // =========================================================================
    class NkJiggleBoneSystem final : public NkSystem {
    public:
        [[nodiscard]] NkSystemDesc Describe() const override {
            return NkSystemDesc{}
                .Reads<NkTransform>()
                .Writes<NkJiggleBone>()
                .Writes<NkSkeleton>()
                .InGroup(NkSystemGroup::PostUpdate)
                .WithPriority(700.f)
                .Named("NkJiggleBoneSystem");
        }

        void Execute(NkWorld& world, float32 dt) noexcept override;

    private:
        void UpdateJiggle(NkJiggleBone& jb,
                           NkSkeleton& skeleton,
                           const NkTransform& tf,
                           float32 dt) noexcept;
        NkQuatf ClampAngle(const NkQuatf& q, float32 maxDeg) const noexcept;
    };

    // =========================================================================
    // NkClothSystem — simulation de tissu GPU
    // =========================================================================
    class NkClothSystem final : public NkSystem {
    public:
        explicit NkClothSystem(NkIDevice* device = nullptr) noexcept
            : mDevice(device) {}

        [[nodiscard]] NkSystemDesc Describe() const override {
            return NkSystemDesc{}
                .Reads<NkTransform>()
                .Writes<NkClothSim>()
                .InGroup(NkSystemGroup::PostUpdate)
                .WithPriority(600.f)
                .Sequential()   // compute shaders = séquentiel
                .Named("NkClothSystem");
        }

        void Execute(NkWorld& world, float32 dt) noexcept override;

        void SetCommandBuffer(NkICommandBuffer* cmd) noexcept { mCmd = cmd; }

    private:
        bool  InitCloth    (NkClothSim& cloth, NkWorld& world) noexcept;
        void  SimulateCloth(NkClothSim& cloth, NkWorld& world, float32 dt) noexcept;
        void  ApplyWind    (NkClothSim& cloth, float32 dt) noexcept;
        void  ResolvePins  (NkClothSim& cloth, NkWorld& world) noexcept;
        void  UploadResult (NkClothSim& cloth, NkWorld& world) noexcept;

        NkIDevice*         mDevice = nullptr;
        NkICommandBuffer*  mCmd    = nullptr;
        nk_uint64          mSimPipeline    = 0;  // Compute pipeline Verlet
        nk_uint64          mConstraintPipeline = 0;
        nk_uint64          mNormalPipeline = 0;
        bool               mPipelinesReady = false;
    };

    // =========================================================================
    // NkHairSystem — simulation de cheveux GPU
    // =========================================================================
    class NkHairSystem final : public NkSystem {
    public:
        explicit NkHairSystem(NkIDevice* device = nullptr) noexcept
            : mDevice(device) {}

        [[nodiscard]] NkSystemDesc Describe() const override {
            return NkSystemDesc{}
                .Reads<NkTransform>()
                .Reads<NkSkeleton>()
                .Writes<NkHairSim>()
                .InGroup(NkSystemGroup::PostUpdate)
                .WithPriority(550.f)
                .Sequential()
                .Named("NkHairSystem");
        }

        void Execute(NkWorld& world, float32 dt) noexcept override;
        void SetCommandBuffer(NkICommandBuffer* cmd) noexcept { mCmd = cmd; }

    private:
        bool InitHair   (NkHairSim& hair, NkWorld& world) noexcept;
        void SimulateHair(NkHairSim& hair, NkWorld& world, float32 dt) noexcept;
        void InterpolateStrands(NkHairSim& hair) noexcept;
        void BuildRenderBuffer (NkHairSim& hair) noexcept;

        NkIDevice*        mDevice = nullptr;
        NkICommandBuffer* mCmd    = nullptr;
        nk_uint64         mSimPipeline   = 0;
        nk_uint64         mInterpPipeline = 0;
        bool              mReady         = false;
    };

    // =========================================================================
    // NkSoftBodySystem — corps mous GPU
    // =========================================================================
    class NkSoftBodySystem final : public NkSystem {
    public:
        explicit NkSoftBodySystem(NkIDevice* device = nullptr) noexcept
            : mDevice(device) {}

        [[nodiscard]] NkSystemDesc Describe() const override {
            return NkSystemDesc{}
                .Writes<NkSoftBody>()
                .InGroup(NkSystemGroup::PostUpdate)
                .WithPriority(500.f)
                .Sequential()
                .Named("NkSoftBodySystem");
        }

        void Execute(NkWorld& world, float32 dt) noexcept override;
        void SetCommandBuffer(NkICommandBuffer* cmd) noexcept { mCmd = cmd; }

    private:
        bool InitSoftBody   (NkSoftBody& sb, NkWorld& world) noexcept;
        void SimulateSoftBody(NkSoftBody& sb, float32 dt) noexcept;
        void BuildTetrahedra (NkSoftBody& sb, NkWorld& world) noexcept;

        NkIDevice*        mDevice = nullptr;
        NkICommandBuffer* mCmd    = nullptr;
        nk_uint64         mPipeline = 0;
        bool              mReady  = false;
    };

    // =========================================================================
    // NkRagdollSystem — bridge ragdoll ↔ rigidbody ECS
    // =========================================================================
    class NkRagdollSystem final : public NkSystem {
    public:
        [[nodiscard]] NkSystemDesc Describe() const override {
            return NkSystemDesc{}
                .Reads<NkTransform>()
                .Writes<NkRagdoll>()
                .Writes<NkSkeleton>()
                .InGroup(NkSystemGroup::PostUpdate)
                .WithPriority(450.f)
                .Named("NkRagdollSystem");
        }

        void Execute(NkWorld& world, float32 dt) noexcept override;

    private:
        void TransitionToRagdoll(NkRagdoll& rd, NkSkeleton& sk,
                                  NkWorld& world, float32 dt) noexcept;
        void ApplyRagdollToSkeleton(NkRagdoll& rd, NkSkeleton& sk,
                                     NkWorld& world) noexcept;
        void BlendAnimRagdoll(NkRagdoll& rd, NkSkeleton& sk,
                               NkWorld& world, float32 dt) noexcept;
    };

    // =========================================================================
    // NkMocapSystem — playback données mocap
    // =========================================================================
    class NkMocapSystem final : public NkSystem {
    public:
        [[nodiscard]] NkSystemDesc Describe() const override {
            return NkSystemDesc{}
                .Writes<NkMotionCapture>()
                .Writes<NkSkeleton>()
                .InGroup(NkSystemGroup::PostUpdate)
                .WithPriority(400.f)
                .Named("NkMocapSystem");
        }

        void Execute(NkWorld& world, float32 dt) noexcept override;

    private:
        void PlaybackMocap(NkMotionCapture& mc, NkSkeleton& sk,
                            float32 dt) noexcept;
        NkMat4f InterpolateFrames(const NkMocapFrame& a,
                                   const NkMocapFrame& b,
                                   float32 t, uint32 boneIdx) const noexcept;
    };

    // =========================================================================
    // NkBlendShapeController — pilotage temps réel des morph targets
    // =========================================================================
    struct NkBlendShapeController {
        enum class DriverType : uint8 {
            Manual,       // valeur directe SetWeight()
            BoneAngle,    // angle d'un os → poids (via courbe)
            AudioLevel,   // niveau audio RMS → poids (lip sync procedural)
            FacialAU,     // NkFacialRig.auWeights[X] → poids
            Physics,      // vitesse/contact → poids
            ECSProperty,  // propriété ECS arbitraire via offset
        };

        struct Driver {
            uint32       shapeIdx   = 0;
            DriverType   type       = DriverType::Manual;
            float32      weight     = 0.f;         // valeur courante [0..1]
            float32      minIn      = 0.f;
            float32      maxIn      = 1.f;
            float32      minOut     = 0.f;
            float32      maxOut     = 1.f;
            float32      smoothing  = 0.f;         // lissage temporel
            NkEntityId   srcEntity  = NkEntityId::Invalid();
            uint32       srcBoneIdx = 0;           // pour BoneAngle
            uint32       srcAUIdx   = 0;           // pour FacialAU
        };

        static constexpr uint32 kMaxDrivers = 64u;
        Driver   drivers[kMaxDrivers] = {};
        uint32   driverCount          = 0;
        NkEntityId meshEntity         = NkEntityId::Invalid();

        bool AddDriver(const Driver& d) noexcept {
            if (driverCount >= kMaxDrivers) return false;
            drivers[driverCount++] = d;
            return true;
        }

        void SetWeight(uint32 shapeIdx, float32 w) noexcept {
            for (uint32 i = 0; i < driverCount; ++i)
                if (drivers[i].shapeIdx == shapeIdx &&
                    drivers[i].type == DriverType::Manual) {
                    drivers[i].weight = NkClamp(w, 0.f, 1.f);
                    return;
                }
        }
    };
    NK_COMPONENT(NkBlendShapeController)

    // =========================================================================
    // NkBlendShapeSystem — résout les drivers et applique les blend shapes
    // =========================================================================
    class NkBlendShapeSystem final : public NkSystem {
    public:
        [[nodiscard]] NkSystemDesc Describe() const override {
            return NkSystemDesc{}
                .Writes<NkBlendShapeController>()
                .Reads<NkSkeleton>()
                .Reads<NkFacialRig>()
                .InGroup(NkSystemGroup::PostUpdate)
                .WithPriority(750.f)
                .Named("NkBlendShapeSystem");
        }

        void Execute(NkWorld& world, float32 dt) noexcept override;

    private:
        float32 ResolveDriver(const NkBlendShapeController::Driver& d,
                               NkWorld& world, float32 dt) const noexcept;
    };

} // namespace nkentseu
