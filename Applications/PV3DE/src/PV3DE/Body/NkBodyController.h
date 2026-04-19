#pragma once

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"
#include "PV3DE/Emotion/NkEmotionFSM.h"

namespace nkentseu {
    namespace pv3de {

        // =====================================================================
        // Paramètres de respiration
        // =====================================================================
        struct NkBreathParams {
            nk_float32 rate      = 16.f;  // respirations/min
            nk_float32 amplitude = 0.30f; // amplitude cage thoracique (0–1)
            nk_float32 phase     = 0.f;   // phase courante (radians)

            // Types pathologiques
            bool cheyneStokes  = false;   // apnée cyclique
            bool paradoxical   = false;   // respiration paradoxale (tirage)
        };

        // =====================================================================
        // NkBodyPose — indices de poses prédéfinies
        // =====================================================================
        enum class NkBodyPose : nk_uint8 {
            Neutral   = 0,
            PainCurl,        // recroquevillé sur la douleur
            Tense,           // musculature contractée
            Slumped,         // affaissé, épuisé
            RigidBreath,     // thorax rigide, dyspnée
            COUNT
        };

        // =====================================================================
        // NkBodyController
        // Pilote l'animation du corps :
        //   - Respiration (amplitude, rythme, type)
        //   - Postures (blend entre NkBodyPose)
        //   - Tremblements (mains, tête)
        //   - Swaying (balancement de détresse)
        //
        // Phase 5 : intègre NkSkeletonInstance et NkIKSolver.
        // Phase actuelle : produit des paramètres numériques utilisables
        //                  par un AnimationSystem externe.
        // =====================================================================
        class NkBodyController {
        public:
            NkBodyController()  = default;
            ~NkBodyController() = default;

            void Init();

            // ── Pilotage depuis NkEmotionOutput ───────────────────────────────
            void ApplyEmotionOutput(const NkEmotionOutput& emotion);

            // ── Contrôle direct ───────────────────────────────────────────────
            void SetTargetPose(NkBodyPose pose, nk_float32 blendTime = 0.5f);
            void SetTremorAmplitude(nk_float32 amp); // 0–1
            void SetBreathParams(const NkBreathParams& p) { mBreathTarget = p; }

            // ── Update ────────────────────────────────────────────────────────
            void Update(nk_float32 dt);

            // ── Sorties ───────────────────────────────────────────────────────

            // Déplacement thoracique normalisé (0–1) pour animer la cage
            nk_float32 GetChestDisplacement() const { return mChestDisp; }

            // Déplacement abdominal normalisé
            nk_float32 GetAbdomenDisplacement() const { return mChestDisp * 0.6f; }

            // Amplitude tremblement membres (0–1)
            nk_float32 GetTremorAmplitude() const { return mTremorAmp; }

            // Offset de position tremblement (x, y, z) — à additionner au mesh
            NkVec3f GetTremorOffset() const { return mTremorOffset; }

            // Poids de blend de la pose courante (0=Neutral, 1=cible)
            nk_float32 GetPoseBlend() const { return mPoseBlend; }
            NkBodyPose GetCurrentPose() const { return mCurrentPose; }

            // Paramètres de respiration courants (interpolés)
            const NkBreathParams& GetBreathParams() const { return mBreath; }

            // Rotation tête par balancement (degrés)
            nk_float32 GetHeadSwayX() const { return mHeadSwayX; }
            nk_float32 GetHeadSwayZ() const { return mHeadSwayZ; }

        private:
            void UpdateBreath(nk_float32 dt);
            void UpdateTremor(nk_float32 dt);
            void UpdatePoseBlend(nk_float32 dt);
            void UpdateSway(nk_float32 dt);

            // ── État breath ───────────────────────────────────────────────────
            NkBreathParams mBreath;         // courant interpolé
            NkBreathParams mBreathTarget;   // cible
            nk_float32     mChestDisp = 0.f;

            // ── Tremblements ──────────────────────────────────────────────────
            nk_float32 mTremorAmp    = 0.f;
            nk_float32 mTremorTarget = 0.f;
            nk_float32 mTremorTime   = 0.f;
            NkVec3f    mTremorOffset;

            // ── Pose ──────────────────────────────────────────────────────────
            NkBodyPose mCurrentPose  = NkBodyPose::Neutral;
            NkBodyPose mTargetPose   = NkBodyPose::Neutral;
            nk_float32 mPoseBlend    = 0.f;     // 0=départ, 1=cible
            nk_float32 mPoseBlendSpd = 2.f;     // vitesse blend/s

            // ── Balancement ───────────────────────────────────────────────────
            nk_float32 mSwayTarget = 0.f;   // amplitude cible
            nk_float32 mSwayTime   = 0.f;
            nk_float32 mHeadSwayX  = 0.f;
            nk_float32 mHeadSwayZ  = 0.f;
        };

    } // namespace pv3de
} // namespace nkentseu
