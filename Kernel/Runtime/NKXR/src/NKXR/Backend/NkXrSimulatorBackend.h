//
// NkXrSimulatorBackend.h
// =============================================================================
// Description :
//   Le backend n°1 de NKXR : un casque SIMULÉ sur desktop. Souris = tête
//   (delta brut, sans accélération OS), ZQSD/WASD = déplacement, stéréo côte
//   à côte à charge de l'application. Aucun matériel requis : c'est lui qui
//   force la bonne abstraction et c'est sur lui que tournent tous les tests.
//
// Caractéristiques :
//   - Tête = yaw/pitch NON bornés par NkAngle (stockés en radians bruts :
//     le wrap (-180,180] de NkAngle casserait un yaw cumulé) ; pitch clampé
//     à ±89° AVANT construction des angles.
//   - Orientation construite par MATRICES (RotY * RotX) puis convertie en
//     quaternion : l'ordre de composition matriciel est sans ambiguïté,
//     contrairement au sens de lecture de l'operator* des quaternions.
//   - Tracking horodaté avec vitesses → la prédiction à predictedDisplayTime
//     est RÉELLE (extrapolation), et une latence simulée peut être injectée
//     pour vérifier qu'elle compense (NK_XR_SIM_LATENCY_MS).
//   - Pose scriptable par variable d'environnement pour les captures
//     déterministes de la boucle d'agent (NK_XR_SIM_POSE).
//
// Crochets d'environnement (tous optionnels) :
//   NK_XR_SIM_POSE="yaw,pitch,x,y,z"  pose FIGÉE (degrés, mètres) — entrées ignorées
//   NK_XR_SIM_LATENCY_MS=<f>          latence de tracking simulée (défaut 0)
//   NK_XR_SIM_HZ=<f>                  cadence d'affichage simulée (défaut 72)
//   NK_XR_SIM_FOV="l,r,u,d"           FOV œil GAUCHE en degrés signés (défaut :
//                                     symétrique, dérivé de l'aspect fenêtre)
//   NK_XR_SIM_IPD=<f>                 écart interpupillaire en mètres
//   NK_XR_SIM_EYE_HEIGHT=<f>          hauteur des yeux au départ (défaut 1,70 m)
//   NK_XR_SIM_SPEED=<f>               vitesse de déplacement m/s (défaut 2,5)
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_XR_NKXRSIMULATORBACKEND_H__
#define __NKENTSEU_XR_NKXRSIMULATORBACKEND_H__

#include "NKXR/NKIXrBackend.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace xr {

		class NkXrSimulatorBackend final : public NKIXrBackend {
			public:
				NkXrSimulatorBackend() = default;
				~NkXrSimulatorBackend() override = default;

				bool Initialize(const NkXrSessionDesc &desc) override;
				void Shutdown() override;

				NkXrSystemInfo GetSystemInfo() const override;
				NkXrSessionState GetState() const override;
				bool PollEvent(NkXrEvent &outEvent) override;

				bool BeginSession() override;
				bool EndSession() override;
				void RequestExit() override;

				bool WaitFrame(NkXrFrameState &outState) override;
				bool BeginFrame() override;
				bool EndFrame(const NkXrFrameEndInfo &info) override;

				bool LocateViews(NkXrSpaceType space, NkXrTime displayTime, NkXrView outViews[NK_XR_EYE_COUNT]) override;
				bool LocateSpace(NkXrSpaceType space, NkXrSpaceType base, NkXrTime time, NkXrPose &outPose) override;

				void AttachActions(const NkXrActionDesc *actions, uint32 count) override;
				bool SyncActions(NkXrTime now) override;
				bool GetActionStateBool(NkXrActionHandle handle, NkXrActionStateBool &outState) override;
				bool GetActionStateFloat(NkXrActionHandle handle, NkXrActionStateFloat &outState) override;
				bool GetActionStateVec2(NkXrActionHandle handle, NkXrActionStateVec2 &outState) override;
				bool LocateActionPose(NkXrActionHandle handle, NkXrSpaceType space, NkXrTime time, NkXrPose &outPose) override;

				// ── Crochets de test (déterministes, sans fenêtre) ───────────
				// Poser la tête directement — ce que fait NK_XR_SIM_POSE, mais
				// pilotable depuis un test numérique sans environnement.
				void DebugSetHead(float32 yawRad, float32 pitchRad, const NkVec3f &positionStage);
				// Injecter un échantillon de tracking horodaté (pour tester la
				// prédiction sans dépendre de l'horloge réelle).
				void DebugPushSample(const NkXrPose &poseStage, NkXrTime time);

			private:
				// Recalcule la pose de tête depuis yaw/pitch/position et pousse
				// l'échantillon horodaté (vitesses déduites du précédent).
				void PushHeadSample(NkXrTime now);
				// Échantillon de tracking effectif à « now » compte tenu de la
				// latence simulée.
				const NkXrTimedPose &SampleAt(NkXrTime now) const;
				// Pose de l'origine de « space » exprimée en STAGE, à « time ».
				bool SpaceInStage(NkXrSpaceType space, NkXrTime time, NkXrPose &outPose) const;
				void QueueStateChange(NkXrSessionState newState, NkXrTime now);
				NkXrFov EyeFov(NkXrEye eye) const;

				NkXrSessionDesc mDesc{};
				NkXrSessionState mState = NkXrSessionState::NK_XR_STATE_IDLE;

				// File d'événements : anneau fixe — le producteur (nous) et le
				// consommateur (l'app) vivent sur le même thread, 16 suffisent.
				static constexpr uint32 kMaxEvents = 16u;
				NkXrEvent mEvents[kMaxEvents]{};
				uint32 mEventHead = 0u;
				uint32 mEventCount = 0u;

				// ── Tête simulée ─────────────────────────────────────────────
				float32 mYawRad = 0.f;
				float32 mPitchRad = 0.f;
				NkVec3f mPositionStage{ 0.f, 1.70f, 0.f };
				bool mFixedPose = false;

				// Historique de tracking (anneau) : nécessaire dès qu'une
				// latence est simulée, et c'est lui qui porte les vitesses.
				static constexpr uint32 kMaxSamples = 64u;
				NkXrTimedPose mSamples[kMaxSamples]{};
				uint32 mSampleHead = 0u;   ///< Index du plus récent.
				uint32 mSampleCount = 0u;

				// Ancre LOCAL : pose de tête (yaw seul) capturée à Begin().
				NkXrPose mLocalAnchor{};

				// ── Cadence et frame ─────────────────────────────────────────
				NkXrTime mDisplayPeriod = 0;
				NkXrTime mLastWaitTime = 0;
				NkXrTime mPredictedDisplayTime = 0;
				bool mFrameBegun = false;
				uint64 mFrameIndex = 0u;

				// ── Réglages ─────────────────────────────────────────────────
				float32 mIpdMeters = 0.063f;
				float32 mEyeHeight = 1.70f;
				float32 mMoveSpeed = 2.5f;
				float32 mLatencySeconds = 0.f;
				bool mFovFromEnv = false;
				NkXrFov mLeftFov{};        ///< Valide si mFovFromEnv.

				// ── Actions attachées ────────────────────────────────────────
				NkVector<NkXrActionDesc> mActions;
				NkVector<NkXrActionStateBool> mBoolStates;
				NkVector<NkXrActionStateFloat> mFloatStates;
				NkVector<NkXrActionStateVec2> mVec2States;
		};

	} // namespace xr
} // namespace nkentseu

#endif // __NKENTSEU_XR_NKXRSIMULATORBACKEND_H__
