#pragma once
// =============================================================================
// NkIKSystem.h  — NKRenderer v4.0  (Tools/IK/)
//
// Système de Cinématique Inverse (IK) avancée pour le rendu.
// Intègre avec NkAnimationSystem (fonctionne sur les os du squelette).
//
// Algorithmes disponibles :
//   • Two-Bone     : analytique exact, jambes/bras (NkIKSolver::NK_TWO_BONE)
//   • CCD          : Cyclic Coordinate Descent, chaînes générales
//   • FABRIK       : Forward And Backward Reaching IK, très stable
//   • FBIK         : Full Body IK — hanche, pieds, mains, tête coordonnés
//   • Spline IK    : chaîne de bones guidée par une courbe Bezier/Catmull-Rom
//
// Architecture :
//   NkIKChain  — séquence d'os, contraintes, cible (effector)
//   NkIKRig    — ensemble de chaînes pour un personnage
//   NkIKSystem — gestionnaire global, résout toutes les rigs enregistrées
//
// Usage :
//   sys.Init(device, &animSys);
//   NkIKRig* rig = sys.CreateRig(skeletonId);
//   NkIKChainId leg = rig->AddChain("leg_R", {boneHip, boneKnee, boneAnkle},
//                                   NkIKSolver::NK_TWO_BONE);
//   // per frame
//   rig->SetTarget(leg, targetPos);
//   sys.Solve();   // résout toutes les rigs, écrit dans les bone matrices
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKRHI/Core/NkIDevice.h"

namespace nkentseu {
namespace renderer {

    class NkAnimationSystem;

    // =========================================================================
    // Identifiants opaques
    // =========================================================================
    struct TagIKRig   {};
    struct TagIKChain {};
    using NkIKRigId   = NkRendHandle<TagIKRig>;
    using NkIKChainId = NkRendHandle<TagIKChain>;

    // =========================================================================
    // Algorithme de résolution
    // =========================================================================
    enum class NkIKSolver : uint8 {
        NK_TWO_BONE   = 0, // analytique — bras / jambes
        NK_CCD        = 1, // Cyclic Coordinate Descent
        NK_FABRIK     = 2, // Forward And Backward Reaching IK
        NK_SPLINE     = 3, // Spline IK — colonne, tentacules
        NK_FBIK       = 4, // Full Body IK
    };

    // =========================================================================
    // Contrainte sur une articulation
    // =========================================================================
    enum class NkIKConstraintType : uint8 {
        NK_FREE       = 0, // aucune contrainte
        NK_HINGE      = 1, // rotation autour d'un axe unique (genou, coude)
        NK_BALL_SOCKET= 2, // rotule — angle max configurable
        NK_TWIST      = 3, // contrainte en torsion uniquement
        NK_PLANAR     = 4, // mouvement contraint dans un plan
    };

    struct NkIKConstraint {
        NkIKConstraintType type  = NkIKConstraintType::NK_FREE;
        NkVec3f            axis  = {0,1,0};  // pour NK_HINGE
        float32            minAngleDeg = -180.f;
        float32            maxAngleDeg =  180.f;
        float32            stiffness   =  1.f;  // 0=souple, 1=rigide
    };

    // =========================================================================
    // Os dans une chaîne IK
    // =========================================================================
    struct NkIKBone {
        uint32         boneIdx = 0;           // index dans le skeleton
        NkString       boneName;
        float32        length  = 1.f;          // longueur du segment
        NkIKConstraint constraint;
        NkVec3f        restDir = {0,1,0};      // direction au repos
    };

    // =========================================================================
    // Cible IK (effector)
    // =========================================================================
    struct NkIKTarget {
        NkVec3f  position  = {};
        NkQuatf  rotation  = NkQuatf::Identity();
        float32  weight    = 1.f;        // 0=ignore, 1=full IK
        bool     matchRotation = false;  // contraindre aussi l'orientation
        NkVec3f  poleVector = {};        // guide pour le genou / coude
        bool     usePole    = false;
    };

    // =========================================================================
    // Configuration d'une chaîne IK
    // =========================================================================
    struct NkIKChainDesc {
        NkString            name;
        NkIKSolver          solver       = NkIKSolver::NK_FABRIK;
        NkVector<NkIKBone>  bones;       // racine → effecteur
        NkIKTarget          target;
        uint32              maxIterations= 10;
        float32             tolerance    = 0.001f; // distance au but acceptable
        bool                enabled      = true;
    };

    // =========================================================================
    // NkIKRig — ensemble de chaînes pour un squelette
    // =========================================================================
    class NkIKRig {
    public:
        explicit NkIKRig(uint64 skeletonId);

        NkIKChainId AddChain  (const NkIKChainDesc& desc);
        void        RemoveChain(NkIKChainId id);

        void SetTarget  (NkIKChainId id, NkVec3f pos,
                         NkQuatf rot = NkQuatf::Identity());
        void SetWeight  (NkIKChainId id, float32 w);
        void SetEnabled (NkIKChainId id, bool enabled);
        void EnableAll  (bool enabled);

        const NkIKChainDesc* GetChain(NkIKChainId id) const;
        NkIKChainId          FindChain(const NkString& name) const;
        uint64               GetSkeletonId() const { return mSkeletonId; }
        uint32               GetChainCount() const;

        // Résultats — bone matrices après résolution (world-space)
        const NkMat4f* GetBoneMatrices()    const;
        uint32         GetBoneMatrixCount() const;

    private:
        friend class NkIKSystem;

        uint64 mSkeletonId = 0;
        uint64 mNextChainId = 1;

        struct ChainEntry {
            NkIKChainId   id;
            NkIKChainDesc desc;
            NkIKTarget    target;
            bool          enabled = true;
            float32       weight  = 1.f;
        };

        NkVector<ChainEntry>  mChains;
        NkVector<NkMat4f>     mBoneMatrices; // résultats
    };

    // =========================================================================
    // Configuration globale
    // =========================================================================
    struct NkIKConfig {
        bool   gpuSkinning   = true;  // écriture des résultats vers GPU
        bool   debugDraw     = false;
        uint32 maxRigs       = 128;
        uint32 maxChainsTotal= 1024;
    };

    // =========================================================================
    // NkIKSystem
    // =========================================================================
    class NkIKSystem {
    public:
        NkIKSystem()  = default;
        ~NkIKSystem();

        // ── Cycle de vie ──────────────────────────────────────────────────────
        bool Init(NkIDevice*        device,
                  NkAnimationSystem* animSys,
                  const NkIKConfig&  cfg = {});
        void Shutdown();

        // ── Gestion des rigs ──────────────────────────────────────────────────
        NkIKRig*  CreateRig (uint64 skeletonId);
        void      DestroyRig(NkIKRig*& rig);

        NkIKRig*  GetRig(uint64 skeletonId);

        // ── Résolution (une fois par frame, après Update de l'animation) ──────
        // pose = bone matrices d'entrée (depuis NkAnimationSystem)
        // Écrit les résultats directement dans pose (blend avec weight)
        void Solve(float32 deltaTimeSec);

        // Résoudre un rig unique seulement
        void SolveRig(NkIKRig* rig, float32 deltaTimeSec);

        // ── Config ────────────────────────────────────────────────────────────
        const NkIKConfig& GetConfig() const { return mCfg; }
        void SetDebugDraw(bool enabled);

    private:
        NkIDevice*         mDevice   = nullptr;
        NkAnimationSystem* mAnimSys  = nullptr;
        NkIKConfig         mCfg;
        bool               mReady    = false;

        NkHashMap<uint64, NkIKRig*> mRigs;

        void SolveChain_TwoBone(NkIKRig::ChainEntry& chain,
                                 NkVector<NkMat4f>&   bones);
        void SolveChain_CCD    (NkIKRig::ChainEntry& chain,
                                 NkVector<NkMat4f>&   bones);
        void SolveChain_FABRIK (NkIKRig::ChainEntry& chain,
                                 NkVector<NkMat4f>&   bones);
        void SolveChain_Spline (NkIKRig::ChainEntry& chain,
                                 NkVector<NkMat4f>&   bones);
        void ApplyConstraint(const NkIKConstraint& c, NkQuatf& q,
                             NkQuatf prevQ);
    };

} // namespace renderer
} // namespace nkentseu
