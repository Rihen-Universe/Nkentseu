// =============================================================================
// tests/test_xr.cpp — Self-test NKXR (standalone, ZÉRO stdlib, sans fenêtre).
// Journalisation via NKLogger (pas de printf/<cstdio>). Couvre : projection
// asymétrique (bords du frustum → bords du clip), poses (aller-retour
// monde/vue, composition, inverse), vitesses angulaires + extrapolation,
// conventions de tête du simulateur (souris/yaw/pitch → avant -Z), espaces
// LOCAL/STAGE/VIEW, discipline de swapchain, cycle de vie de session et
// entrées par actions. Chaque cas est choisi pour qu'une implémentation
// fausse (signe inversé, ordre de composition, wrap d'angle) ÉCHOUE.
// =============================================================================
#include "NKXR/NKXR.h"
#include "NKXR/Backend/NkXrSimulatorBackend.h"
#include "NKLogger/NkLog.h"

using namespace nkentseu;
using namespace nkentseu::xr;

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg)                                                                                               \
	do {                                                                                                               \
		if (cond) {                                                                                                    \
			++g_pass;                                                                                                  \
		} else {                                                                                                       \
			++g_fail;                                                                                                  \
			logger.Errorf("  [FAIL] %s\n", msg);                                                                       \
		}                                                                                                              \
	} while (0)

static bool Near(float32 a, float32 b, float32 e = 1e-3f) {
	float32 d = a - b;
	return (d < 0 ? -d : d) <= e;
}

static bool NearV3(const math::NkVec3f &a, const math::NkVec3f &b, float32 e = 1e-3f) {
	return Near(a.x, b.x, e) && Near(a.y, b.y, e) && Near(a.z, b.z, e);
}

// Produit matrice (colonne-majeure) × vec4, écrit à la main pour ne dépendre
// d'aucun opérateur du moteur : le test vérifie la MATRICE, pas l'opérateur.
static math::NkVec4f MulPoint(const math::NkMat4f &m, const math::NkVec4f &v) {
	math::NkVec4f r;
	r.x = m.mat[0][0] * v.x + m.mat[1][0] * v.y + m.mat[2][0] * v.z + m.mat[3][0] * v.w;
	r.y = m.mat[0][1] * v.x + m.mat[1][1] * v.y + m.mat[2][1] * v.z + m.mat[3][1] * v.w;
	r.z = m.mat[0][2] * v.x + m.mat[1][2] * v.y + m.mat[2][2] * v.z + m.mat[3][2] * v.w;
	r.w = m.mat[0][3] * v.x + m.mat[1][3] * v.y + m.mat[2][3] * v.z + m.mat[3][3] * v.w;
	return r;
}

static constexpr float32 kDeg = math::NK_PI_F / 180.f;

int main() {
	logger.Info("=== NKXR self-test ===\n");

	// ── Projection asymétrique : bords du FOV → bords du clip ───────────────
	{
		// Frustum volontairement décentré : un code qui symétrise en douce
		// (moyenne des angles) rate les quatre bords à la fois.
		const NkXrFov fov{ -49.f * kDeg, 43.f * kDeg, 41.f * kDeg, -47.f * kDeg };
		const float32 nearP = 0.1f, farP = 100.f;
		const math::NkMat4f proj = NkXrProjectionFromFov(fov, nearP, farP, true);

		// Point posé SUR le bord droit/haut à z = -1 : doit sortir en x/w = 1.
		const float32 z = -1.f;
		math::NkVec4f clip = MulPoint(proj, { math::NkTan(fov.angleRight) * (-z), math::NkTan(fov.angleUp) * (-z), z, 1.f });
		CHECK(Near(clip.x / clip.w, 1.f), "proj asym : bord droit -> x/w = +1");
		CHECK(Near(clip.y / clip.w, 1.f), "proj asym : bord haut  -> y/w = +1");
		clip = MulPoint(proj, { math::NkTan(fov.angleLeft) * (-z), math::NkTan(fov.angleDown) * (-z), z, 1.f });
		CHECK(Near(clip.x / clip.w, -1.f), "proj asym : bord gauche -> x/w = -1");
		CHECK(Near(clip.y / clip.w, -1.f), "proj asym : bord bas    -> y/w = -1");

		// Profondeur [0,1] : near → 0, far → 1.
		clip = MulPoint(proj, { 0.f, 0.f, -nearP, 1.f });
		CHECK(Near(clip.z / clip.w, 0.f), "proj asym [0,1] : near -> 0");
		clip = MulPoint(proj, { 0.f, 0.f, -farP, 1.f });
		CHECK(Near(clip.z / clip.w, 1.f), "proj asym [0,1] : far -> 1");

		// Profondeur [-1,1] : near → -1 (convention GL).
		const math::NkMat4f projGl = NkXrProjectionFromFov(fov, nearP, farP, false);
		clip = MulPoint(projGl, { 0.f, 0.f, -nearP, 1.f });
		CHECK(Near(clip.z / clip.w, -1.f), "proj asym [-1,1] : near -> -1");
	}
	{
		// Cas symétrique : doit coïncider coefficient par coefficient avec la
		// Perspective du moteur (même FOV vertical, aspect depuis les tangentes)
		// — c'est la preuve que NKXR et NKRenderer parlent la même projection.
		const float32 halfV = 30.f * kDeg;
		const float32 aspect = 16.f / 9.f;
		const float32 halfH = math::NkAtan(math::NkTan(halfV) * aspect);
		const math::NkMat4f ours = NkXrProjectionFromFov(NkXrFov::Symmetric(halfH, halfV), 0.1f, 100.f, true);
		const math::NkMat4f theirs = math::NkMat4f::Perspective(math::NkAngle::FromRad(2.f * halfV), aspect, 0.1f, 100.f, true);
		bool same = true;
		for (int i = 0; i < 16; ++i) {
			if (!Near(ours.data[i], theirs.data[i], 1e-4f)) {
				same = false;
			}
		}
		CHECK(same, "proj symétrique == NkMat4::Perspective (16 coefficients)");
	}

	// ── Poses : monde/vue, composition, inverse ──────────────────────────────
	{
		NkXrPose pose;
		pose.position = { 1.f, 2.f, 3.f };
		pose.orientation = NkQuatf(math::NkMat4f::RotationY(math::NkAngle::FromRad(90.f * kDeg))).Normalized();

		// Yaw +90° tourne l'avant (-Z) vers -X : un signe inversé dans la
		// convention envoie vers +X, le test le voit immédiatement.
		CHECK(NearV3(NkXrForward(pose.orientation), { -1.f, 0.f, 0.f }), "yaw +90° : avant -Z -> -X");

		// Transform == ToMat4 sur un point quelconque.
		const math::NkVec3f p{ 0.5f, -1.f, 2.f };
		const math::NkVec3f byPose = pose.Transform(p);
		const math::NkVec4f byMat = MulPoint(pose.ToMat4(), { p.x, p.y, p.z, 1.f });
		CHECK(NearV3(byPose, { byMat.x, byMat.y, byMat.z }), "Transform == ToMat4 * point");

		// view * monde = identité : LA propriété d'une matrice de vue.
		const math::NkMat4f ident = pose.ToViewMatrix() * pose.ToMat4();
		bool isIdentity = true;
		for (int c = 0; c < 4; ++c) {
			for (int r = 0; r < 4; ++r) {
				if (!Near(ident.mat[c][r], (c == r) ? 1.f : 0.f, 1e-4f)) {
					isIdentity = false;
				}
			}
		}
		CHECK(isIdentity, "ToViewMatrix * ToMat4 == identité");

		// pose * pose^-1 == identité (position ET orientation).
		const NkXrPose id = pose * pose.Inverted();
		CHECK(NearV3(id.position, { 0.f, 0.f, 0.f }) && Near(math::NkAbs(id.orientation.w), 1.f, 1e-4f),
			  "pose * pose.Inverted() == pose identité");
	}

	// ── Vitesse angulaire + extrapolation : aller-retour ─────────────────────
	{
		const NkQuatf from{};
		const NkQuatf to = NkQuatf(math::NkMat4f::RotationY(math::NkAngle::FromRad(30.f * kDeg))).Normalized();
		const math::NkVec3f omega = NkXrAngularVelocity(from, to, 0.1f);
		// 30° en 0,1 s = 5,236 rad/s autour de Y — le signe de l'axe compte.
		CHECK(Near(omega.y, (30.f * kDeg) / 0.1f, 1e-2f) && Near(omega.x, 0.f) && Near(omega.z, 0.f),
			  "vitesse angulaire : 30°/0,1s autour de +Y");

		NkXrPose pose;
		NkXrVelocity vel;
		vel.angular = omega;
		const NkXrPose ahead = NkXrExtrapolate(pose, vel, 0.1f);
		CHECK(Near(math::NkAbs(ahead.orientation.Dot(to)), 1.f, 1e-4f), "extrapolation 0,1s retombe sur RotY(30°)");

		// La fenêtre de prédiction est bornée à 100 ms : au-delà, même
		// résultat — un modèle qui extrapole 10 s serait un générateur de mal
		// des transports, pas un prédicteur.
		NkXrTimedPose timed;
		timed.pose = pose;
		timed.velocity = vel;
		timed.time = 1000000000;                              // t0 = 1 s.
		const NkXrPose at100ms = timed.PredictAt(1100000000); // +100 ms.
		const NkXrPose at10s = timed.PredictAt(11000000000);  // +10 s.
		CHECK(Near(math::NkAbs(at100ms.orientation.Dot(at10s.orientation)), 1.f, 1e-4f),
			  "prédiction bornée : +10 s == +100 ms");
	}

	// ── Simulateur : conventions de tête, yeux, espaces ──────────────────────
	{
		NkXrSessionDesc desc;
		desc.window = nullptr; // sans tête : tests numériques purs.
		NkXrSimulatorBackend backend;
		CHECK(backend.Initialize(desc), "simulateur : Initialize sans fenêtre");
		CHECK(backend.GetState() == NkXrSessionState::NK_XR_STATE_READY, "simulateur : READY après Initialize");

		// Pitch +45° (regard vers le haut) : l'avant monte en +Y. Un ordre de
		// composition yaw/pitch inversé donnerait un roll parasite ici.
		backend.DebugSetHead(0.f, 45.f * kDeg, { 0.f, 1.7f, 0.f });
		NkXrView views[NK_XR_EYE_COUNT];
		CHECK(backend.LocateViews(NkXrSpaceType::NK_XR_SPACE_STAGE, NkXrSession::Now(), views), "LocateViews STAGE");
		CHECK(NearV3(NkXrForward(views[0].orientation), { 0.f, 0.70711f, -0.70711f }, 1e-3f),
			  "pitch +45° : avant (0, +0.707, -0.707)");

		// Séparation des yeux = IPD, portée par l'axe DROITE de la tête.
		backend.DebugSetHead(90.f * kDeg, 0.f, { 0.f, 1.7f, 0.f });
		CHECK(backend.LocateViews(NkXrSpaceType::NK_XR_SPACE_STAGE, NkXrSession::Now(), views), "LocateViews yaw 90°");
		const math::NkVec3f gap = views[1].position - views[0].position;
		// Tête tournée de +90° : droite locale (+X) pointe vers -Z monde.
		CHECK(Near(gap.Len(), desc.ipdMeters, 1e-4f), "écart des yeux == IPD");
		CHECK(Near(gap.z, -desc.ipdMeters, 1e-4f) && Near(gap.x, 0.f, 1e-4f), "écart porté par la droite TOURNÉE de la tête");

		// FOV : symétrique par défaut (l'étage 0 l'assume — NKRenderer ne
		// consomme pas encore l'asymétrie), identique pour les deux yeux.
		CHECK(views[0].fov.IsSymmetric() && views[1].fov.IsSymmetric(), "FOV simulateur symétrique par défaut");

		// Cycle de vie + LOCAL : l'ancre est prise au Begin, yaw seul.
		CHECK(backend.BeginSession(), "BeginSession en READY");
		CHECK(backend.GetState() == NkXrSessionState::NK_XR_STATE_FOCUSED, "FOCUSED après Begin");
		NkXrEvent ev;
		int stateEvents = 0;
		while (backend.PollEvent(ev)) {
			if (ev.type == NkXrEventType::NK_XR_EVENT_STATE_CHANGED) {
				++stateEvents;
			}
		}
		// READY + SYNCHRONIZED + VISIBLE + FOCUSED = 4 transitions annoncées.
		CHECK(stateEvents == 4, "4 événements d'état (READY..FOCUSED)");

		// Dans LOCAL, la tête du Begin est à l'origine, avant = -Z, même si
		// elle était tournée en STAGE : c'est la définition de LOCAL.
		CHECK(backend.LocateViews(NkXrSpaceType::NK_XR_SPACE_LOCAL, NkXrSession::Now(), views), "LocateViews LOCAL");
		const math::NkVec3f mid = (views[0].position + views[1].position) * 0.5f;
		CHECK(NearV3(mid, { 0.f, 0.f, 0.f }, 1e-3f), "LOCAL : tête du Begin à l'origine");
		CHECK(NearV3(NkXrForward(views[0].orientation), { 0.f, 0.f, -1.f }, 1e-3f), "LOCAL : avant du Begin == -Z");

		// LocateSpace : VIEW dans STAGE == pose de tête ; cohérence inverse.
		NkXrPose viewInStage;
		CHECK(backend.LocateSpace(NkXrSpaceType::NK_XR_SPACE_VIEW, NkXrSpaceType::NK_XR_SPACE_STAGE,
								  NkXrSession::Now(), viewInStage),
			  "LocateSpace VIEW/STAGE");
		CHECK(NearV3(viewInStage.position, { 0.f, 1.7f, 0.f }, 1e-3f), "VIEW dans STAGE == position de tête");

		// Prédiction de bout en bout : deux échantillons synthétiques à
		// 1 rad/s de yaw, la vue demandée 50 ms plus tard doit avoir tourné
		// de ~0,15 rad — un LocateViews qui ignore displayTime rend 0,10.
		NkXrPose sample;
		sample.position = { 0.f, 1.7f, 0.f };
		sample.orientation = NkQuatf{};
		backend.DebugPushSample(sample, 5000000000);
		sample.orientation = NkQuatf(math::NkMat4f::RotationY(math::NkAngle::FromRad(0.1f))).Normalized();
		backend.DebugPushSample(sample, 5100000000);
		CHECK(backend.LocateViews(NkXrSpaceType::NK_XR_SPACE_STAGE, 5150000000, views), "LocateViews à T+50ms");
		const math::NkVec3f fwd = NkXrForward(views[0].orientation);
		CHECK(Near(fwd.x, -math::NkSin(0.15f), 5e-3f) && Near(fwd.z, -math::NkCos(0.15f), 5e-3f),
			  "prédiction : yaw extrapolé à 0,15 rad");

		// Frame loop + arrêt propre.
		NkXrFrameState frame;
		CHECK(backend.WaitFrame(frame), "WaitFrame en FOCUSED");
		CHECK(frame.predictedDisplayTime > 0 && frame.shouldRender, "frame state peuplé, shouldRender");
		CHECK(backend.BeginFrame(), "BeginFrame");
		NkXrFrameEndInfo endInfo;
		CHECK(backend.EndFrame(endInfo), "EndFrame (soumission vide tolérée backend)");
		backend.RequestExit();
		CHECK(backend.GetState() == NkXrSessionState::NK_XR_STATE_STOPPING, "RequestExit -> STOPPING");
		CHECK(backend.EndSession(), "EndSession en STOPPING");
		CHECK(backend.GetState() == NkXrSessionState::NK_XR_STATE_EXITING, "EndSession -> EXITING");
		backend.Shutdown();
	}

	// ── Session : garde-fous et actions ──────────────────────────────────────
	{
		NkXrSessionDesc desc;
		desc.window = nullptr;
		NkXrSession *session = NkXrSession::Create(desc);
		CHECK(session != nullptr, "NkXrSession::Create (simulateur)");
		if (session != nullptr) {
			NkXrActionSet actions;
			const NkXrActionHandle select = actions.CreateAction(
				{ "selectionner", NkXrActionType::NK_XR_ACTION_BOOL, NkXrActionUsage::NK_XR_USAGE_SELECT });
			const NkXrActionHandle aim = actions.CreateAction(
				{ "viser", NkXrActionType::NK_XR_ACTION_POSE, NkXrActionUsage::NK_XR_USAGE_AIM_POSE });
			CHECK(session->AttachActionSet(actions), "AttachActionSet");
			CHECK(!session->AttachActionSet(actions), "2e AttachActionSet refusé (handles figés)");

			CHECK(session->Begin(), "session.Begin en READY");
			CHECK(!session->Begin(), "2e Begin refusé (plus READY)");

			NkXrFrameState frame;
			CHECK(session->WaitFrame(frame), "session.WaitFrame");
			CHECK(session->BeginFrame(), "session.BeginFrame");
			CHECK(!session->BeginFrame(), "double BeginFrame refusé");
			CHECK(!session->WaitFrame(frame), "WaitFrame refusé pendant frame ouverte");

			// EndFrame doit refuser une image encore acquise : c'est LE bug de
			// synchronisation qu'on veut attraper à la source.
			NkXrSwapchainDesc scDesc;
			scDesc.width = 640u;
			scDesc.height = 720u;
			scDesc.imageCount = 2u;
			NkXrSwapchain *sc = session->CreateSwapchain(scDesc);
			CHECK(sc != nullptr, "CreateSwapchain 640x720x2");
			uint32 imageIndex = 0u;
			CHECK(sc->AcquireImage(imageIndex) && imageIndex == 0u, "Acquire -> image 0");
			CHECK(!sc->AcquireImage(imageIndex), "double Acquire refusé");
			NkXrLayerProjection layer;
			layer.views[0].swapchain = sc;
			layer.views[1].swapchain = sc;
			NkXrFrameEndInfo endInfo;
			endInfo.displayTime = frame.predictedDisplayTime;
			endInfo.projection = &layer;
			CHECK(!session->EndFrame(endInfo), "EndFrame refusé : image encore acquise");
			CHECK(sc->ReleaseImage(), "ReleaseImage");
			CHECK(sc->GetLastReleasedIndex() == 0u, "dernière image relâchée = 0");
			CHECK(session->BeginFrame(), "BeginFrame (frame refusée = frame fermée)");
			CHECK(session->EndFrame(endInfo), "EndFrame accepté après Release");
			CHECK(sc->AcquireImage(imageIndex) && imageIndex == 1u, "Acquire suivant -> image 1 (rotation)");
			sc->ReleaseImage();

			// Actions sans fenêtre : sync OK, état inactif (rien de lié), la
			// pose de main existe quand même (accrochée à la tête simulée).
			CHECK(session->SyncActions(), "SyncActions en FOCUSED");
			NkXrActionStateBool selectState;
			CHECK(session->GetActionStateBool(select, selectState), "GetActionStateBool");
			CHECK(!selectState.active, "sans fenêtre : action inactive (rien de lié)");
			NkXrPose aimPose;
			NkXrSpace stage(NkXrSpaceType::NK_XR_SPACE_STAGE);
			CHECK(session->LocateActionPose(aim, stage, NkXrSession::Now(), aimPose), "LocateActionPose (main simulée)");
			CHECK(aimPose.position.y < 1.7f && aimPose.position.z < 0.f, "main simulée : sous le regard, devant");
			CHECK(!session->LocateActionPose(select, stage, NkXrSession::Now(), aimPose),
				  "LocateActionPose refusé sur une action non-POSE");

			session->DestroySwapchain(sc);
			session->RequestExit();
			CHECK(session->End(), "session.End en STOPPING");
			NkXrSession::Destroy(session);
			CHECK(session == nullptr, "Destroy remet le pointeur à nullptr");
		}
	}

	logger.Infof("=== NKXR self-test : %d OK, %d ÉCHECS ===\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
