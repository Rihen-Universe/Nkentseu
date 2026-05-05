#pragma once
// =============================================================================
// Nkentseu/Physics/NkClothSim.h
// =============================================================================
// Simulation de tissu par GPU Compute (Verlet + Position-Based Dynamics).
//
// ALGORITHME : Position-Based Dynamics (PBD)
//   Chaque frame (substepps fois) :
//   1. Intégration Verlet (positions prédites depuis vélocités)
//   2. Application des contraintes (distance, bend, shear, pin)
//   3. Détection de collision avec les colliders de la scène
//   4. Self-collision (optionnel, plus coûteux)
//   5. Mise à jour des vélocités
//   6. Upload vers NkRender3D (UpdateMeshVertices)
//
// PERFORMANCE :
//   • ~100K vertices à 60fps sur GPU moderne (compute shader)
//   • Substeps : 8 par défaut (équilibre qualité/performance)
//   • SDF collision (Signed Distance Field) pour colliders complexes
//
// USAGE :
//   auto& cloth = go.AddComponent<NkClothSim>().Get();
//   cloth.stiffness   = 0.9f;    // tissu rigide (jeans)
//   cloth.damping     = 0.02f;
//   cloth.PinVertex(0, {});       // vertex 0 fixé dans l'espace
//   cloth.PinToBone(3, skeleton, 5);  // vertex 3 suit l'os 5
//   // NkClothSystem::Init() upload le mesh et démarre la sim
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKMath/NkMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "Nkentseu/Modeling/NkEditableMesh.h"

namespace nkentseu {
    using namespace math;
    using namespace ecs;

    // =========================================================================
    // NkClothConstraint — contrainte de simulation
    // =========================================================================
    struct NkClothConstraint {
        enum class Type : uint8 {
            Distance,   ///< Maintient distance entre v0 et v1
            Bend,       ///< Résistance au pliage (quads adjacents)
            Shear,      ///< Résistance au cisaillement (diagonale du quad)
            Pin,        ///< Vertex fixé (au monde ou à un os)
        };

        Type    type     = Type::Distance;
        uint32  v0       = 0;
        uint32  v1       = 0;       ///< Inutilisé pour Pin
        float32 restLen  = 1.f;     ///< Distance de repos (auto-calculée à l'init)
        float32 stiffness = 1.f;    ///< [0..1] override local de la stiffness globale
    };

    // =========================================================================
    // NkPinConstraint — vertex épinglé à un point ou un os
    // =========================================================================
    struct NkPinConstraint {
        uint32      vertexIdx  = 0;
        NkVec3f     worldPos   = {};        ///< Position fixe (si boneEntity invalide)
        NkEntityId  boneEntity = NkEntityId::Invalid();
        uint32      boneIndex  = 0;         ///< Os dans NkSkeleton de boneEntity
        float32     weight     = 1.f;       ///< Force du pin [0=libre, 1=fixé]
    };

    // =========================================================================
    // NkClothSim — composant ECS de simulation cloth
    // =========================================================================
    struct NkClothSim {
        // ── Paramètres physiques ──────────────────────────────────────────
        float32 stiffness       = 0.8f;    ///< Rigidité [0=gelée, 1=rigide]
        float32 bendStiffness   = 0.2f;    ///< Résistance pliage
        float32 shearStiffness  = 0.5f;    ///< Résistance cisaillement
        float32 damping         = 0.01f;   ///< Amortissement vélocité
        float32 mass            = 0.1f;    ///< Masse par vertex (kg)

        NkVec3f gravity         = {0.f, -9.81f, 0.f};
        NkVec3f wind            = {};
        float32 windTurbulence  = 0.3f;    ///< Variation du vent
        float32 windFrequency   = 0.5f;    ///< Fréquence Hz des rafales

        float32 friction        = 0.5f;    ///< Friction contre colliders
        float32 selfCollisionDist = 0.02f; ///< Distance min self-collision

        // ── Simulation ────────────────────────────────────────────────────
        uint32  substeps        = 8;       ///< Sous-pas par frame
        float32 fixedDt         = 1.f/60.f;
        bool    selfCollision   = false;   ///< Coûteux — désactivé par défaut
        bool    simulating      = true;

        // ── Contraintes de pins ────────────────────────────────────────────
        static constexpr uint32 kMaxPins = 512u;
        NkPinConstraint pins[kMaxPins] = {};
        uint32          pinCount = 0;

        bool AddPin(uint32 vertIdx, const NkVec3f& worldPos, float32 weight = 1.f) noexcept {
            if (pinCount >= kMaxPins) return false;
            pins[pinCount++] = { vertIdx, worldPos, NkEntityId::Invalid(), 0, weight };
            return true;
        }

        bool AddBonePin(uint32 vertIdx, NkEntityId boneEnt,
                        uint32 boneIdx, float32 weight = 1.f) noexcept {
            if (pinCount >= kMaxPins) return false;
            pins[pinCount++] = { vertIdx, {}, boneEnt, boneIdx, weight };
            return true;
        }

        void RemovePin(uint32 vertIdx) noexcept {
            for (uint32 i = 0; i < pinCount; ++i) {
                if (pins[i].vertexIdx == vertIdx) {
                    pins[i] = pins[--pinCount];
                    return;
                }
            }
        }

        // ── Handles GPU (gérés par NkClothSystem) ────────────────────────
        nk_uint64 posBufferA        = 0;  ///< SSBO positions courantes (compute ping)
        nk_uint64 posBufferB        = 0;  ///< SSBO positions précédentes (Verlet)
        nk_uint64 constraintBuffer  = 0;  ///< SSBO contraintes
        nk_uint64 normalBuffer      = 0;  ///< SSBO normales recalculées
        nk_uint64 pinBuffer         = 0;  ///< SSBO pins

        nk_uint32 vertexCount       = 0;
        nk_uint32 constraintCount   = 0;
        bool      initialized       = false;

        // ── Mesh source ───────────────────────────────────────────────────
        NkEntityId meshEntity = NkEntityId::Invalid(); ///< Entité avec NkMeshComponent
    };
    NK_COMPONENT(NkClothSim)

    // =========================================================================
    // NkHairSim — simulation de cheveux/poils/fourrure
    // =========================================================================
    struct NkHairStrand {
        static constexpr uint32 kMaxParticles = 16u;
        NkVec3f particles[kMaxParticles] = {};  ///< Positions des particules
        NkVec3f prevPos [kMaxParticles]  = {};  ///< Positions précédentes
        uint32  particleCount            = 0;
        NkVec3f rootPos                  = {};  ///< Position du follicle (fixe)
        uint32  boneIndex                = 0;   ///< Os suivant pour le follicle
    };

    struct NkHairSim {
        // ── Physique ──────────────────────────────────────────────────────
        float32 stiffness    = 0.5f;
        float32 damping      = 0.05f;
        float32 gravity      = -9.81f;
        NkVec3f wind         = {};
        float32 friction     = 0.3f;
        float32 length       = 0.15f;   ///< Longueur d'un strand (mètres)
        uint32  particlesPerStrand = 8;

        // ── Style ─────────────────────────────────────────────────────────
        float32 thickness    = 0.001f;  ///< Épaisseur des mèches (m)
        float32 curliness    = 0.f;     ///< 0=droit, 1=frisé
        float32 waviness     = 0.f;     ///< Ondulation
        NkVec3f rootColor    = {0.1f, 0.05f, 0.02f};  ///< Couleur à la racine
        NkVec3f tipColor     = {0.3f, 0.15f, 0.05f};  ///< Couleur à la pointe

        // ── Guide strands (low-res sim) ───────────────────────────────────
        static constexpr uint32 kMaxGuideStrands = 2048u;
        nk_uint64 guideBuffer    = 0;   ///< SSBO guide strands (compute)
        nk_uint64 interpBuffer   = 0;   ///< SSBO strands interpolés pour rendu
        nk_uint32 guideCount     = 0;
        nk_uint32 renderCount    = 0;   ///< Nombre total de strands rendus

        float32  guideInfluenceRadius = 0.02f; ///< Rayon d'influence par guide

        bool     simulating   = true;
        bool     initialized  = false;

        NkEntityId bodyEntity = NkEntityId::Invalid(); ///< Corps pour les collisions
    };
    NK_COMPONENT(NkHairSim)

    // =========================================================================
    // NkJiggleBone — os secondaires dynamiques
    // =========================================================================
    /**
     * @struct NkJiggleBone
     * @brief Os secondaires avec inertie (jiggle physics).
     *
     * Simule l'inertie d'éléments mous attachés au squelette :
     * seins, ventre, oreilles, queue, cheveux lourds, vêtements...
     * Plus léger et plus contrôlable que NkClothSim pour ces cas.
     */
    struct NkJiggleBone {
        uint32  boneIndex   = 0;       ///< Os cible dans NkSkeleton

        // ── Physique ──────────────────────────────────────────────────────
        float32 stiffness   = 0.7f;   ///< Rappel vers la pose d'animation [0..1]
        float32 damping     = 0.3f;   ///< Amortissement
        float32 mass        = 1.f;    ///< Inertie
        float32 gravity     = 0.f;    ///< Influence gravité

        // ── Contraintes d'angle ───────────────────────────────────────────
        float32 maxAngleDeg = 45.f;   ///< Angle max de déviation
        bool    constrainX  = true;
        bool    constrainY  = true;
        bool    constrainZ  = true;

        // ── État interne ─────────────────────────────────────────────────
        NkVec3f velocity    = {};
        NkVec3f currentPos  = {};     ///< Position courante du tip de l'os
        NkVec3f targetPos   = {};     ///< Position cible (depuis animation)
        NkQuatf currentRot  = NkQuatf::Identity();

        bool    enabled     = true;
    };
    NK_COMPONENT(NkJiggleBone)

    // =========================================================================
    // NkRagdoll — ragdoll physics pour chutes et impacts
    // =========================================================================
    struct NkRagdollBoneLink {
        uint32      skeletonBoneIdx = 0;       ///< Os dans NkSkeleton
        NkEntityId  rigidbodyEntity;           ///< Entité ECS avec NkRigidbodyComponent
        NkMat4f     boneToBody = NkMat4f::Identity();  ///< Offset d'alignement
        float32     mass       = 1.f;
    };

    struct NkRagdoll {
        static constexpr uint32 kMaxBones = 64u;

        NkRagdollBoneLink bones[kMaxBones] = {};
        uint32            boneCount        = 0;

        enum class State : uint8 {
            Animated,     ///< Purement animé (ragdoll désactivé)
            Blending,     ///< Transition animation → ragdoll
            FullRagdoll,  ///< Purement physique
            Kinematic     ///< Ragdoll pose le corps pour le moteur physique (active ragdoll)
        };

        State   state           = State::Animated;
        float32 blendWeight     = 0.f;   ///< 0=animation, 1=ragdoll
        float32 blendSpeed      = 3.f;   ///< Vitesse de transition (units/sec)

        float32 impactThreshold = 10.f;  ///< Vélocité (m/s) déclenchant auto-ragdoll
        bool    autoActivate    = true;  ///< Active auto depuis NkPhysicsSystem

        NkEntityId skeletonEntity = NkEntityId::Invalid();  ///< Entité avec NkSkeleton

        void SetState(State s) noexcept { state = s; }
        void BlendIn (float32 speed = 2.f) noexcept { state = State::Blending; blendSpeed = speed; }
        void BlendOut(float32 speed = 2.f) noexcept { blendSpeed = -speed; }
    };
    NK_COMPONENT(NkRagdoll)

    // =========================================================================
    // NkMotionCapture — import et playback données mocap
    // =========================================================================
    struct NkMocapFrame {
        float32          time = 0.f;
        NkVector<NkMat4f> boneTransforms;  ///< Transform monde de chaque os
    };

    struct NkMotionCapture {
        NkVector<NkMocapFrame> frames;
        float32 fps          = 30.f;
        float32 duration     = 0.f;
        float32 currentTime  = 0.f;
        bool    playing      = false;
        bool    loop         = true;
        float32 speed        = 1.f;
        float32 blendWeight  = 1.f;    ///< Blend avec animation normale

        NkEntityId skeletonEntity = NkEntityId::Invalid();

        // ── Import ────────────────────────────────────────────────────────
        /**
         * @brief Importe depuis un fichier BVH (Biovision Hierarchy).
         * Format standard pour les données de capture de mouvement.
         */
        bool LoadBVH(const char* path) noexcept;

        void Play()    noexcept { playing = true; }
        void Stop()    noexcept { playing = false; currentTime = 0.f; }
        void Pause()   noexcept { playing = false; }

        /**
         * @brief Applique la pose courante au squelette.
         * @param skeleton NkSkeleton cible.
         */
        void Apply(NkSkeleton& skeleton) const noexcept;

        /**
         * @brief Retargeting vers un squelette différent.
         * @param srcSkeleton Squelette source (du mocap).
         * @param dstSkeleton Squelette destination (du personnage).
         */
        void Retarget(const NkSkeleton& srcSkeleton,
                       NkSkeleton& dstSkeleton) const noexcept;
    };
    NK_COMPONENT(NkMotionCapture)

    // =========================================================================
    // NkSoftBody — corps mous (gélatine, chair, muscles)
    // =========================================================================
    struct NkSoftBody {
        float32 stiffness      = 0.5f;
        float32 volumeStiffness = 0.8f;  ///< Contrainte de volume (incompressibilité)
        float32 damping        = 0.1f;
        float32 mass           = 1.f;
        NkVec3f gravity        = {0,-9.81f,0};
        uint32  substeps       = 4;
        bool    simulating     = true;

        // Handles GPU
        nk_uint64 posBuffer    = 0;
        nk_uint64 prevPosBuffer = 0;
        nk_uint64 tetraBuffer  = 0;  ///< Tétraèdres pour contraintes volumétriques
        nk_uint32 vertexCount  = 0;
        nk_uint32 tetraCount   = 0;
        bool      initialized  = false;

        NkEntityId meshEntity  = NkEntityId::Invalid();
    };
    NK_COMPONENT(NkSoftBody)

} // namespace nkentseu
