#pragma once
// =============================================================================
// Nkentseu/Rigging/NkIKSolver.h
// =============================================================================
// Solveurs d'Inverse Kinematics pour l'animation de personnages et d'objets.
//
// ALGORITHMES :
//   • FABRIK  — Forward And Backward Reaching Inverse Kinematics
//               Meilleur pour les chaînes longues (bras robot, tentacules, queues)
//               Converge rapidement (~10 itérations), supporte les contraintes d'angle
//
//   • CCD     — Cyclic Coordinate Descent
//               Simple et rapide pour les chaînes courtes
//               Peut oscilller sur les chaînes longues
//
//   • TwoBone — Solution analytique exacte pour les membres bipartites (bras/jambe)
//               Instantané (O(1)), résultat déterministe
//               Supporte le pole target pour orienter le genou/coude
//
//   • Spline  — IK Spline : déplace les os pour qu'ils suivent une NkBezierCurve
//               Style Blender "Spline IK", utilisé pour serpents, câbles, cheveux
//
// USAGE (FABRIK) :
//   NkIKSolver solver;
//   NkIKSolver::Chain chain;
//   chain.boneIndices = {0, 1, 2, 3};  // root → effecteur
//   chain.targetPosition = {5, 2, 0};
//   chain.weight = 1.f;
//   solver.SolveFABRIK(skeleton, chain);
//
// USAGE (TwoBone) :
//   NkIKSolver::TwoBoneChain chain;
//   chain.rootBone   = 5;   // épaule
//   chain.midBone    = 6;   // coude
//   chain.tipBone    = 7;   // poignet
//   chain.target     = handPosition;
//   chain.poleTarget = elbowHint;     // direction du coude
//   solver.SolveTwoBone(skeleton, chain);
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKMath/NkMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "Nkentseu/ECS/Components/Animation/NkAnimation.h"

namespace nkentseu {

    using namespace math;

    // =========================================================================
    // NkIKConstraint — contraintes d'angle sur les os
    // =========================================================================
    struct NkIKConstraint {
        bool    enabled         = false;
        float32 minAngleX       = -180.f;  ///< Limite rotation X (degrés)
        float32 maxAngleX       =  180.f;
        float32 minAngleY       = -180.f;
        float32 maxAngleY       =  180.f;
        float32 minAngleZ       = -180.f;
        float32 maxAngleZ       =  180.f;
    };

    // =========================================================================
    // NkIKSolver
    // =========================================================================
    class NkIKSolver {
        public:
            NkIKSolver()  noexcept = default;
            ~NkIKSolver() noexcept = default;

            // ── Chaîne générique (FABRIK / CCD) ──────────────────────────
            struct Chain {
                NkVector<uint32>        boneIndices;    ///< Root → tip (indices dans NkSkeleton)
                NkVec3f                 targetPosition  = {};
                NkQuatf                 targetRotation  = NkQuatf::Identity();
                float32                 weight          = 1.f;       ///< Influence IK [0..1]
                uint32                  maxIterations   = 10;
                float32                 tolerance       = 0.0001f;  ///< Distance de convergence
                bool                    constrainTip    = false;     ///< Orienter le tip vers target
                NkVector<NkIKConstraint> constraints;   ///< Une contrainte par os (optionnel)
            };

            // ── Chaîne bipartite (TwoBone — analytique) ──────────────────
            struct TwoBoneChain {
                uint32  rootBone    = 0;       ///< Épaule / cuisse
                uint32  midBone     = 0;       ///< Coude / genou
                uint32  tipBone     = 0;       ///< Poignet / cheville
                NkVec3f target      = {};      ///< Position cible de l'effecteur
                NkVec3f poleTarget  = {};      ///< Hint de direction du joint intermédiaire
                bool    usePole     = false;
                float32 weight      = 1.f;
                bool    stretchable = false;   ///< Étirer les os pour atteindre le target
                float32 maxStretch  = 1.5f;    ///< Facteur d'étirement max
            };

            // ── Chaîne spline ─────────────────────────────────────────────
            struct SplineChain {
                NkVector<uint32>  boneIndices;  ///< Os à déformer le long de la courbe
                NkVector<NkVec3f> splinePoints; ///< Points de contrôle de la courbe Bézier
                float32           weight        = 1.f;
                bool              useYUp        = true;  ///< Orienter Y vers le haut de la courbe
            };

            // ── Solveurs ──────────────────────────────────────────────────

            /**
             * @brief FABRIK — solveur itératif pour chaînes longues.
             * Forward-And-Backward Reaching IK.
             * @param skeleton Squelette à modifier (pose courante).
             * @param chain    Chaîne IK à résoudre.
             * @return Nombre d'itérations effectuées.
             */
            uint32 SolveFABRIK(ecs::NkSkeleton& skeleton,
                                const Chain& chain) noexcept;

            /**
             * @brief CCD — Cyclic Coordinate Descent.
             * Simple pour les chaînes courtes.
             */
            uint32 SolveCCD(ecs::NkSkeleton& skeleton,
                             const Chain& chain) noexcept;

            /**
             * @brief TwoBone — solution analytique exacte pour membres bipartites.
             * Instantané, déterministe, supporte le pole target.
             */
            void SolveTwoBone(ecs::NkSkeleton& skeleton,
                               const TwoBoneChain& chain) noexcept;

            /**
             * @brief Spline IK — déforme les os pour suivre une courbe Bézier.
             */
            void SolveSpline(ecs::NkSkeleton& skeleton,
                              const SplineChain& chain) noexcept;

            // ── Utilitaires ───────────────────────────────────────────────

            /**
             * @brief Calcule la longueur totale d'une chaîne d'os.
             */
            [[nodiscard]] float32 ChainLength(const ecs::NkSkeleton& skeleton,
                                               const NkVector<uint32>& boneIndices) const noexcept;

            /**
             * @brief Vérifie si le target est atteignable depuis la chaîne.
             */
            [[nodiscard]] bool IsReachable(const ecs::NkSkeleton& skeleton,
                                            const NkVector<uint32>& boneIndices,
                                            const NkVec3f& target) const noexcept;

        private:
            // ── FABRIK internal ───────────────────────────────────────────
            void FABRIKForward (NkVector<NkVec3f>& positions,
                                 const NkVec3f& target) const noexcept;
            void FABRIKBackward(NkVector<NkVec3f>& positions,
                                 const NkVec3f& root) const noexcept;
            void ApplyPositionsToSkeleton(ecs::NkSkeleton& sk,
                                           const NkVector<uint32>& boneIds,
                                           const NkVector<NkVec3f>& worldPos) const noexcept;

            // ── TwoBone internal ──────────────────────────────────────────
            [[nodiscard]] float32 ComputeBendAngle(float32 a, float32 b,
                                                    float32 c) const noexcept;

            // ── Spline internal ───────────────────────────────────────────
            [[nodiscard]] NkVec3f EvalCubicBezier(const NkVector<NkVec3f>& pts,
                                                   float32 t) const noexcept;
            [[nodiscard]] NkVec3f EvalCubicBezierTangent(const NkVector<NkVec3f>& pts,
                                                          float32 t) const noexcept;

            // ── Constraints ───────────────────────────────────────────────
            [[nodiscard]] NkQuatf ApplyAngleConstraint(
                const NkQuatf& rotation,
                const NkIKConstraint& constraint) const noexcept;

            // ── Cache frame ───────────────────────────────────────────────
            mutable NkVector<NkVec3f> mPosBuffer;  ///< Buffer réutilisé
            mutable NkVector<float32> mLenBuffer;  ///< Longueurs d'os
    };

    // =========================================================================
    // NkConstraintSystem — composant ECS + système de contraintes
    // =========================================================================

    enum class NkConstraintType : uint8 {
        CopyLocation,   ///< Copie la position d'une entité cible
        CopyRotation,   ///< Copie la rotation d'une entité cible
        CopyScale,      ///< Copie l'échelle d'une entité cible
        CopyTransform,  ///< Copie toute la transform d'une entité cible
        LookAt,         ///< Oriente vers une entité cible
        TrackTo,        ///< Track-To (maintient un axe vers la cible)
        StretchTo,      ///< Étire l'os vers la cible
        ClampTo,        ///< Contraindre la position sur une courbe
        LimitLocation,  ///< Limites de position min/max
        LimitRotation,  ///< Limites de rotation min/max
        LimitScale,     ///< Limites d'échelle min/max
        Floor,          ///< Empêche de passer sous un plan
        ChildOf,        ///< Parentage inverse avec correction de bind
    };

    struct NkConstraint {
        NkConstraintType type      = NkConstraintType::LookAt;
        ecs::NkEntityId  target    = ecs::NkEntityId::Invalid();
        float32          influence = 1.f;    ///< Poids [0..1]
        bool             enabled   = true;

        // Configuration par type
        struct {
            bool   useX = true, useY = true, useZ = true;
            bool   invert = false;
            float32 min[3] = {-1e9f,-1e9f,-1e9f};
            float32 max[3] = { 1e9f, 1e9f, 1e9f};
            uint8  trackAxis = 1;   ///< 0=X, 1=Y, 2=Z, 3=-X, 4=-Y, 5=-Z
            uint8  upAxis    = 1;
        } cfg;
    };
    NK_COMPONENT(NkConstraint)

    /**
     * @brief Système ECS qui résout toutes les contraintes actives.
     * Exécuté dans PostUpdate après NkTransformSystem.
     */
    class NkConstraintSystem final : public ecs::NkSystem {
        public:
            [[nodiscard]] ecs::NkSystemDesc Describe() const override {
                return ecs::NkSystemDesc{}
                    .Reads<ecs::NkTransform>()
                    .Writes<NkConstraint>()
                    .InGroup(ecs::NkSystemGroup::PostUpdate)
                    .WithPriority(500.f)
                    .Named("NkConstraintSystem");
            }

            void Execute(ecs::NkWorld& world, float32 dt) noexcept override;
        private:
            void ResolveConstraint(ecs::NkWorld& world,
                                    ecs::NkEntityId entity,
                                    NkConstraint& c) noexcept;
    };

} // namespace nkentseu
