//
// NkXrSimulatorBackend.cpp
// =============================================================================
// Description :
//   Implémentation du casque simulé : machine d'états, tracking souris/
//   clavier horodaté avec vitesses, prédiction, espaces, actions.
//
// Caractéristiques :
//   - La lecture des entrées vit dans WaitFrame : c'est l'instant OpenXR où
//     le tracking est échantillonné, et ça garantit un échantillon par frame
//     exactement (ni plus — dérive —, ni moins — poses figées).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#include "NKXR/Backend/NkXrSimulatorBackend.h"
#include "NKLogger/NkLog.h"
#include "NKTime/NkChrono.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKEvent/NkEventDispatcher.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"

#include <cstdlib>

namespace nkentseu {
	namespace xr {

		namespace {

			constexpr float32 kMouseSensitivityRad = 0.0025f;      ///< rad par compte souris brut.
			constexpr float32 kPitchLimitRad = 89.f * math::NK_PI_F / 180.f;
			constexpr float32 kDefaultHalfFovVRad = 35.f * math::NK_PI_F / 180.f;

			NkXrTime NkXrNowNs() {
				return NkXrTime(NkChrono::Now().nanoseconds);
			}

			// Lit jusqu'à maxCount flottants séparés par des virgules. Rend le
			// nombre lu — un format partiel est un format refusé par l'appelant.
			uint32 ParseFloatList(const char *text, float32 *out, uint32 maxCount) {
				if (text == nullptr) {
					return 0u;
				}
				uint32 count = 0u;
				const char *cursor = text;
				while (count < maxCount) {
					char *end = nullptr;
					const float64 value = strtod(cursor, &end);
					if (end == cursor) {
						break;
					}
					out[count] = float32(value);
					++count;
					cursor = end;
					while (*cursor == ',' || *cursor == ' ') {
						++cursor;
					}
					if (*cursor == '\0') {
						break;
					}
				}
				return count;
			}

			float32 EnvFloat(const char *name, float32 fallback) {
				const char *text = getenv(name);
				if (text == nullptr || *text == '\0') {
					return fallback;
				}
				return float32(atof(text));
			}

		} // namespace

		// ── Initialisation ───────────────────────────────────────────────────

		bool NkXrSimulatorBackend::Initialize(const NkXrSessionDesc &desc) {
			mDesc = desc;
			mIpdMeters = EnvFloat("NK_XR_SIM_IPD", desc.ipdMeters);
			mEyeHeight = EnvFloat("NK_XR_SIM_EYE_HEIGHT", 1.70f);
			mMoveSpeed = EnvFloat("NK_XR_SIM_SPEED", 2.5f);
			mLatencySeconds = EnvFloat("NK_XR_SIM_LATENCY_MS", 0.f) * 0.001f;

			const float32 hz = EnvFloat("NK_XR_SIM_HZ", 72.f);
			mDisplayPeriod = NkXrTime(1e9 / float64(hz > 1.f ? hz : 72.f));

			mPositionStage = NkVec3f(0.f, mEyeHeight, 0.f);
			mYawRad = 0.f;
			mPitchRad = 0.f;

			// Pose scriptée : la boucle d'agent fige la tête pour que deux
			// exécutions donnent le même pixel — les entrées sont ignorées.
			float32 scripted[5]{};
			if (ParseFloatList(getenv("NK_XR_SIM_POSE"), scripted, 5u) == 5u) {
				mFixedPose = true;
				mYawRad = scripted[0] * math::NK_PI_F / 180.f;
				mPitchRad = scripted[1] * math::NK_PI_F / 180.f;
				mPositionStage = NkVec3f(scripted[2], scripted[3], scripted[4]);
				logger.Infof("[NKXR/Sim] Pose figée par NK_XR_SIM_POSE (yaw %.1f°, pitch %.1f°).\n",
							 scripted[0], scripted[1]);
			}

			float32 fovDeg[4]{};
			if (ParseFloatList(getenv("NK_XR_SIM_FOV"), fovDeg, 4u) == 4u) {
				mFovFromEnv = true;
				mLeftFov.angleLeft = fovDeg[0] * math::NK_PI_F / 180.f;
				mLeftFov.angleRight = fovDeg[1] * math::NK_PI_F / 180.f;
				mLeftFov.angleUp = fovDeg[2] * math::NK_PI_F / 180.f;
				mLeftFov.angleDown = fovDeg[3] * math::NK_PI_F / 180.f;
			}

			const NkXrTime now = NkXrNowNs();
			PushHeadSample(now);

			// Le « système » simulé est prêt dès la création : READY tout de
			// suite, mais via la MÊME mécanique d'événement que le futur
			// backend OpenXR — l'app ne doit pas pouvoir faire la différence.
			mState = NkXrSessionState::NK_XR_STATE_IDLE;
			QueueStateChange(NkXrSessionState::NK_XR_STATE_READY, now);
			return true;
		}

		void NkXrSimulatorBackend::Shutdown() {
			mState = NkXrSessionState::NK_XR_STATE_IDLE;
			mEventCount = 0u;
			mSampleCount = 0u;
		}

		NkXrSystemInfo NkXrSimulatorBackend::GetSystemInfo() const {
			NkXrSystemInfo info;
			info.systemName = "NKXR Simulator";
			info.ipdMeters = mIpdMeters;
			// Recommandation : une moitié de fenêtre par œil — c'est la
			// définition même du côte à côte.
			uint32 width = 1280u;
			uint32 height = 720u;
			if (mDesc.window != nullptr) {
				const math::NkVec2u size = mDesc.window->GetSize();
				width = size.width;
				height = size.height;
			}
			for (uint32 eye = 0u; eye < NK_XR_EYE_COUNT; ++eye) {
				info.views[eye].recommendedWidth = width / 2u;
				info.views[eye].recommendedHeight = height;
			}
			return info;
		}

		NkXrSessionState NkXrSimulatorBackend::GetState() const {
			return mState;
		}

		// ── Événements ───────────────────────────────────────────────────────

		void NkXrSimulatorBackend::QueueStateChange(NkXrSessionState newState, NkXrTime now) {
			mState = newState;
			if (mEventCount >= kMaxEvents) {
				// File pleine = l'app ne dépile pas : le dire vaut mieux que
				// d'écraser en silence un STOPPING qu'elle attendait.
				logger.Warnf("[NKXR/Sim] File d'événements pleine, événement d'état %d perdu.\n", int(newState));
				return;
			}
			const uint32 tail = (mEventHead + mEventCount) % kMaxEvents;
			mEvents[tail].type = NkXrEventType::NK_XR_EVENT_STATE_CHANGED;
			mEvents[tail].state = newState;
			mEvents[tail].time = now;
			++mEventCount;
		}

		bool NkXrSimulatorBackend::PollEvent(NkXrEvent &outEvent) {
			if (mEventCount == 0u) {
				return false;
			}
			outEvent = mEvents[mEventHead];
			mEventHead = (mEventHead + 1u) % kMaxEvents;
			--mEventCount;
			return true;
		}

		// ── Cycle de vie ─────────────────────────────────────────────────────

		bool NkXrSimulatorBackend::BeginSession() {
			if (mState != NkXrSessionState::NK_XR_STATE_READY) {
				return false;
			}
			const NkXrTime now = NkXrNowNs();
			// L'ancre LOCAL est la tête au Begin, YAW SEUL : LOCAL reste
			// aligné sur la gravité même si l'utilisateur regarde le sol au
			// moment du Begin (sinon tout le monde local serait penché).
			const NkXrTimedPose &sample = SampleAt(now);
			mLocalAnchor.position = sample.pose.position;
			mLocalAnchor.orientation = NkQuatf(NkMat4f::RotationY(math::NkAngle::FromRad(mYawRad))).Normalized();
			// La progression SYNCHRONIZED → VISIBLE → FOCUSED est immédiate
			// ici, mais l'app la voit passer événement par événement — la
			// même séquence qu'un vrai runtime, dans le même ordre.
			QueueStateChange(NkXrSessionState::NK_XR_STATE_SYNCHRONIZED, now);
			QueueStateChange(NkXrSessionState::NK_XR_STATE_VISIBLE, now);
			QueueStateChange(NkXrSessionState::NK_XR_STATE_FOCUSED, now);
			mFrameIndex = 0u;
			mLastWaitTime = 0;
			return true;
		}

		bool NkXrSimulatorBackend::EndSession() {
			if (mState != NkXrSessionState::NK_XR_STATE_STOPPING) {
				return false;
			}
			QueueStateChange(NkXrSessionState::NK_XR_STATE_EXITING, NkXrNowNs());
			return true;
		}

		void NkXrSimulatorBackend::RequestExit() {
			const NkXrTime now = NkXrNowNs();
			switch (mState) {
				case NkXrSessionState::NK_XR_STATE_SYNCHRONIZED:
				case NkXrSessionState::NK_XR_STATE_VISIBLE:
				case NkXrSessionState::NK_XR_STATE_FOCUSED: {
					// La boucle de frame tourne : l'app doit d'abord End().
					QueueStateChange(NkXrSessionState::NK_XR_STATE_STOPPING, now);
					break;
				}
				case NkXrSessionState::NK_XR_STATE_IDLE:
				case NkXrSessionState::NK_XR_STATE_READY: {
					QueueStateChange(NkXrSessionState::NK_XR_STATE_EXITING, now);
					break;
				}
				case NkXrSessionState::NK_XR_STATE_STOPPING:
				case NkXrSessionState::NK_XR_STATE_EXITING: {
					break;
				}
			}
		}

		// ── Boucle de frame ──────────────────────────────────────────────────

		bool NkXrSimulatorBackend::WaitFrame(NkXrFrameState &outState) {
			if (mState != NkXrSessionState::NK_XR_STATE_SYNCHRONIZED &&
				mState != NkXrSessionState::NK_XR_STATE_VISIBLE &&
				mState != NkXrSessionState::NK_XR_STATE_FOCUSED &&
				mState != NkXrSessionState::NK_XR_STATE_STOPPING) {
				return false;
			}
			const NkXrTime now = NkXrNowNs();
			float32 dt = float32(float64(now - mLastWaitTime) * 1e-9);
			if (mLastWaitTime == 0 || dt <= 0.f || dt > 0.25f) {
				dt = float32(float64(mDisplayPeriod) * 1e-9);
			}
			mLastWaitTime = now;

			// Souris/clavier — seulement en FOCUSED (le contrat des entrées)
			// et jamais en pose scriptée.
			if (!mFixedPose && mDesc.window != nullptr && mState == NkXrSessionState::NK_XR_STATE_FOCUSED) {
				mYawRad -= float32(NkInput.MouseRawDeltaX()) * kMouseSensitivityRad;
				mPitchRad -= float32(NkInput.MouseRawDeltaY()) * kMouseSensitivityRad;
				mPitchRad = math::NkClamp(mPitchRad, -kPitchLimitRad, kPitchLimitRad);

				// Avant au sol depuis le yaw seul : le déplacement ignore le
				// pitch (regarder le sol ne doit pas y faire plonger).
				const float32 sinYaw = math::NkSin(mYawRad);
				const float32 cosYaw = math::NkCos(mYawRad);
				const NkVec3f forward(-sinYaw, 0.f, -cosYaw);
				const NkVec3f right(cosYaw, 0.f, -sinYaw);

				NkVec3f move(0.f, 0.f, 0.f);
				// ZQSD accepté à côté de WASD : le clavier de Rihen est AZERTY.
				if (NkInput.IsKeyDown(NkKey::NK_W) || NkInput.IsKeyDown(NkKey::NK_Z)) {
					move = move + forward;
				}
				if (NkInput.IsKeyDown(NkKey::NK_S)) {
					move = move - forward;
				}
				if (NkInput.IsKeyDown(NkKey::NK_D)) {
					move = move + right;
				}
				if (NkInput.IsKeyDown(NkKey::NK_A) || NkInput.IsKeyDown(NkKey::NK_Q)) {
					move = move - right;
				}
				if (NkInput.IsKeyDown(NkKey::NK_SPACE)) {
					move.y += 1.f;
				}
				if (NkInput.IsKeyDown(NkKey::NK_C)) {
					move.y -= 1.f;
				}
				const float32 lenSq = move.LenSq();
				if (lenSq > 1e-6f) {
					float32 speed = mMoveSpeed;
					if (NkInput.IsShiftDown()) {
						speed *= 3.f;
					}
					move = move * (speed * dt / math::NkSqrt(lenSq));
					mPositionStage = mPositionStage + move;
				}
			}

			PushHeadSample(now);
			++mFrameIndex;
			mPredictedDisplayTime = now + mDisplayPeriod;

			outState.predictedDisplayTime = mPredictedDisplayTime;
			outState.predictedDisplayPeriod = mDisplayPeriod;
			const bool minimized = (mDesc.window != nullptr) && mDesc.window->IsMinimized();
			outState.shouldRender = !minimized &&
									(mState == NkXrSessionState::NK_XR_STATE_VISIBLE ||
									 mState == NkXrSessionState::NK_XR_STATE_FOCUSED);
			return true;
		}

		bool NkXrSimulatorBackend::BeginFrame() {
			mFrameBegun = true;
			return true;
		}

		bool NkXrSimulatorBackend::EndFrame(const NkXrFrameEndInfo &info) {
			if (!mFrameBegun) {
				return false;
			}
			mFrameBegun = false;
			// Le simulateur ne compose pas (l'app rend déjà côte à côte dans
			// la fenêtre) : il VALIDE. Une projection sans swapchains est une
			// soumission vide déguisée — la dire tout de suite.
			if (info.projection != nullptr) {
				for (uint32 eye = 0u; eye < NK_XR_EYE_COUNT; ++eye) {
					if (info.projection->views[eye].swapchain == nullptr) {
						logger.Warnf("[NKXR/Sim] EndFrame : couche de projection sans swapchain pour l'œil %u.\n", eye);
					}
				}
			}
			return true;
		}

		// ── Tracking ─────────────────────────────────────────────────────────

		void NkXrSimulatorBackend::PushHeadSample(NkXrTime now) {
			// Matrices puis quaternion : RotY*RotX applique le pitch PUIS le
			// yaw (composition colonne-majeure), l'ordre tête standard.
			const NkMat4f rotation = NkMat4f::RotationY(math::NkAngle::FromRad(mYawRad)) *
									 NkMat4f::RotationX(math::NkAngle::FromRad(mPitchRad));
			NkXrPose pose;
			pose.position = mPositionStage;
			pose.orientation = NkQuatf(rotation).Normalized();
			DebugPushSample(pose, now);
		}

		void NkXrSimulatorBackend::DebugPushSample(const NkXrPose &poseStage, NkXrTime time) {
			NkXrTimedPose sample;
			sample.pose = poseStage;
			sample.time = time;
			if (mSampleCount > 0u) {
				const NkXrTimedPose &previous = mSamples[mSampleHead];
				const float32 dt = float32(float64(time - previous.time) * 1e-9);
				if (dt > 1e-6f) {
					sample.velocity.linear = (poseStage.position - previous.pose.position) * (1.f / dt);
					sample.velocity.angular = NkXrAngularVelocity(previous.pose.orientation, poseStage.orientation, dt);
				}
				else {
					sample.velocity = previous.velocity;
				}
			}
			mSampleHead = (mSampleCount == 0u) ? 0u : (mSampleHead + 1u) % kMaxSamples;
			mSamples[mSampleHead] = sample;
			if (mSampleCount < kMaxSamples) {
				++mSampleCount;
			}
		}

		void NkXrSimulatorBackend::DebugSetHead(float32 yawRad, float32 pitchRad, const NkVec3f &positionStage) {
			mYawRad = yawRad;
			mPitchRad = math::NkClamp(pitchRad, -kPitchLimitRad, kPitchLimitRad);
			mPositionStage = positionStage;
			PushHeadSample(NkXrNowNs());
			// Poser la tête est une TÉLÉPORTATION de test : la vitesse déduite
			// du saut serait énorme et l'extrapolation la reprojetterait sur
			// les poses suivantes — un test deviendrait flou par contagion.
			mSamples[mSampleHead].velocity = NkXrVelocity{};
		}

		const NkXrTimedPose &NkXrSimulatorBackend::SampleAt(NkXrTime now) const {
			// Latence nulle : l'échantillon le plus récent, point.
			if (mLatencySeconds <= 0.f || mSampleCount <= 1u) {
				return mSamples[mSampleHead];
			}
			// Latence simulée : on rend l'échantillon d'il y a « latence »
			// secondes — la prédiction doit alors combler l'écart, et c'est
			// exactement ce qu'un test mesure.
			const NkXrTime target = now - NkXrTime(float64(mLatencySeconds) * 1e9);
			uint32 index = mSampleHead;
			for (uint32 i = 0u; i < mSampleCount; ++i) {
				if (mSamples[index].time <= target) {
					return mSamples[index];
				}
				index = (index + kMaxSamples - 1u) % kMaxSamples;
			}
			// Plus vieux disponible : l'historique ne remonte pas si loin.
			return mSamples[(mSampleHead + kMaxSamples - (mSampleCount - 1u)) % kMaxSamples];
		}

		// ── Espaces et vues ──────────────────────────────────────────────────

		bool NkXrSimulatorBackend::SpaceInStage(NkXrSpaceType space, NkXrTime time, NkXrPose &outPose) const {
			switch (space) {
				case NkXrSpaceType::NK_XR_SPACE_STAGE: {
					outPose = NkXrPose::Identity();
					return true;
				}
				case NkXrSpaceType::NK_XR_SPACE_LOCAL: {
					outPose = mLocalAnchor;
					return true;
				}
				case NkXrSpaceType::NK_XR_SPACE_VIEW: {
					if (mSampleCount == 0u) {
						return false;
					}
					outPose = SampleAt(NkXrNowNs()).PredictAt(time);
					return true;
				}
			}
			return false;
		}

		bool NkXrSimulatorBackend::LocateSpace(NkXrSpaceType space, NkXrSpaceType base, NkXrTime time, NkXrPose &outPose) {
			NkXrPose spaceInStage;
			NkXrPose baseInStage;
			if (!SpaceInStage(space, time, spaceInStage) || !SpaceInStage(base, time, baseInStage)) {
				return false;
			}
			outPose = baseInStage.Inverted() * spaceInStage;
			return true;
		}

		NkXrFov NkXrSimulatorBackend::EyeFov(NkXrEye eye) const {
			if (mFovFromEnv) {
				if (eye == NkXrEye::NK_XR_EYE_LEFT) {
					return mLeftFov;
				}
				// Miroir horizontal : l'œil droit voit large vers SA tempe.
				NkXrFov right;
				right.angleLeft = -mLeftFov.angleRight;
				right.angleRight = -mLeftFov.angleLeft;
				right.angleUp = mLeftFov.angleUp;
				right.angleDown = mLeftFov.angleDown;
				return right;
			}
			// Défaut : symétrique, horizontal dérivé de l'aspect d'une moitié
			// de fenêtre — pixels carrés, image non étirée. C'est HONNÊTE pour
			// l'étage 0 : NKRenderer ne sait consommer que des frustums
			// symétriques (l'asymétrie attend l'étage 1).
			float32 aspect = 0.5f * 1280.f / 720.f;
			if (mDesc.window != nullptr) {
				const math::NkVec2u size = mDesc.window->GetSize();
				if (size.height > 0u) {
					aspect = (float32(size.width) * 0.5f) / float32(size.height);
				}
			}
			const float32 halfV = kDefaultHalfFovVRad;
			const float32 halfH = math::NkAtan(math::NkTan(halfV) * aspect);
			(void)eye;
			return NkXrFov::Symmetric(halfH, halfV);
		}

		bool NkXrSimulatorBackend::LocateViews(NkXrSpaceType space, NkXrTime displayTime, NkXrView outViews[NK_XR_EYE_COUNT]) {
			if (mSampleCount == 0u) {
				return false;
			}
			const NkXrPose headStage = SampleAt(NkXrNowNs()).PredictAt(displayTime);

			NkXrPose head;
			switch (space) {
				case NkXrSpaceType::NK_XR_SPACE_STAGE: {
					head = headStage;
					break;
				}
				case NkXrSpaceType::NK_XR_SPACE_LOCAL: {
					head = mLocalAnchor.Inverted() * headStage;
					break;
				}
				case NkXrSpaceType::NK_XR_SPACE_VIEW: {
					head = NkXrPose::Identity();
					break;
				}
			}

			for (uint32 eye = 0u; eye < NK_XR_EYE_COUNT; ++eye) {
				const float32 side = (eye == uint32(NkXrEye::NK_XR_EYE_LEFT)) ? -1.f : 1.f;
				const NkVec3f offset(side * mIpdMeters * 0.5f, 0.f, 0.f);
				outViews[eye].position = head.Transform(offset);
				outViews[eye].orientation = head.orientation;
				outViews[eye].fov = EyeFov(NkXrEye(eye));
			}
			return true;
		}

		// ── Actions ──────────────────────────────────────────────────────────

		void NkXrSimulatorBackend::AttachActions(const NkXrActionDesc *actions, uint32 count) {
			mActions.Clear();
			mBoolStates.Clear();
			mFloatStates.Clear();
			mVec2States.Clear();
			for (uint32 i = 0u; i < count; ++i) {
				mActions.PushBack(actions[i]);
				mBoolStates.PushBack(NkXrActionStateBool{});
				mFloatStates.PushBack(NkXrActionStateFloat{});
				mVec2States.PushBack(NkXrActionStateVec2{});
			}
		}

		bool NkXrSimulatorBackend::SyncActions(NkXrTime now) {
			const bool hasInput = (mDesc.window != nullptr);
			for (nk_size i = 0; i < mActions.Size(); ++i) {
				const NkXrActionDesc &action = mActions[i];
				bool boolValue = false;
				NkVec2f vec2Value(0.f, 0.f);
				bool bound = hasInput;
				if (hasInput) {
					switch (action.usage) {
						case NkXrActionUsage::NK_XR_USAGE_SELECT: {
							boolValue = NkInput.IsLeftDown();
							break;
						}
						case NkXrActionUsage::NK_XR_USAGE_GRAB: {
							boolValue = NkInput.IsRightDown();
							break;
						}
						case NkXrActionUsage::NK_XR_USAGE_MENU: {
							boolValue = NkInput.IsKeyDown(NkKey::NK_TAB);
							break;
						}
						case NkXrActionUsage::NK_XR_USAGE_MOVE: {
							// Flèches, PAS ZQSD/WASD : ceux-là appartiennent au
							// déplacement de la tête simulée — les partager
							// ferait bouger la tête ET le stick à chaque appui.
							if (NkInput.IsKeyDown(NkKey::NK_RIGHT)) {
								vec2Value.x += 1.f;
							}
							if (NkInput.IsKeyDown(NkKey::NK_LEFT)) {
								vec2Value.x -= 1.f;
							}
							if (NkInput.IsKeyDown(NkKey::NK_UP)) {
								vec2Value.y += 1.f;
							}
							if (NkInput.IsKeyDown(NkKey::NK_DOWN)) {
								vec2Value.y -= 1.f;
							}
							break;
						}
						case NkXrActionUsage::NK_XR_USAGE_AIM_POSE:
						case NkXrActionUsage::NK_XR_USAGE_GRIP_POSE: {
							// Les poses se lisent par LocateActionPose, pas ici.
							break;
						}
					}
				}
				NkXrActionStateBool &boolState = mBoolStates[i];
				boolState.changed = (boolValue != boolState.current);
				if (boolState.changed) {
					boolState.lastChangeTime = now;
				}
				boolState.current = boolValue;
				boolState.active = bound;

				NkXrActionStateFloat &floatState = mFloatStates[i];
				const float32 floatValue = boolValue ? 1.f : 0.f;
				floatState.changed = (floatValue != floatState.current);
				floatState.current = floatValue;
				floatState.active = bound;

				NkXrActionStateVec2 &vec2State = mVec2States[i];
				vec2State.changed = (vec2Value.x != vec2State.current.x) || (vec2Value.y != vec2State.current.y);
				vec2State.current = vec2Value;
				vec2State.active = bound;
			}
			return true;
		}

		bool NkXrSimulatorBackend::GetActionStateBool(NkXrActionHandle handle, NkXrActionStateBool &outState) {
			if (handle == NK_XR_ACTION_INVALID || handle > mBoolStates.Size()) {
				return false;
			}
			outState = mBoolStates[handle - 1u];
			return true;
		}

		bool NkXrSimulatorBackend::GetActionStateFloat(NkXrActionHandle handle, NkXrActionStateFloat &outState) {
			if (handle == NK_XR_ACTION_INVALID || handle > mFloatStates.Size()) {
				return false;
			}
			outState = mFloatStates[handle - 1u];
			return true;
		}

		bool NkXrSimulatorBackend::GetActionStateVec2(NkXrActionHandle handle, NkXrActionStateVec2 &outState) {
			if (handle == NK_XR_ACTION_INVALID || handle > mVec2States.Size()) {
				return false;
			}
			outState = mVec2States[handle - 1u];
			return true;
		}

		bool NkXrSimulatorBackend::LocateActionPose(NkXrActionHandle handle, NkXrSpaceType space, NkXrTime time, NkXrPose &outPose) {
			if (handle == NK_XR_ACTION_INVALID || handle > mActions.Size()) {
				return false;
			}
			const NkXrActionDesc &action = mActions[handle - 1u];
			if (action.type != NkXrActionType::NK_XR_ACTION_POSE) {
				return false;
			}
			if (mSampleCount == 0u) {
				return false;
			}
			// Main droite simulée, accrochée à la tête : visée un peu devant
			// et sous le regard, paume plus près du corps. Assez pour exercer
			// tout le chemin « pose d'action » de bout en bout.
			NkXrPose offset;
			if (action.usage == NkXrActionUsage::NK_XR_USAGE_GRIP_POSE) {
				offset.position = NkVec3f(0.18f, -0.30f, -0.25f);
			}
			else {
				offset.position = NkVec3f(0.18f, -0.25f, -0.40f);
			}
			const NkXrPose headStage = SampleAt(NkXrNowNs()).PredictAt(time);
			const NkXrPose handStage = headStage * offset;

			NkXrPose baseInStage;
			if (!SpaceInStage(space, time, baseInStage)) {
				return false;
			}
			outPose = baseInStage.Inverted() * handStage;
			return true;
		}

	} // namespace xr
} // namespace nkentseu
