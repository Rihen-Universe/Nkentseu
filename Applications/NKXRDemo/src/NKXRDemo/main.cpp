// =============================================================================
// NKXRDemo — Étage 0 : une scène NKRenderer rendue en STÉRÉO SIMULÉE via NKXR.
//
// Ce que cette démo prouve (et rien de plus — étage 0 assumé) :
//   - le cycle de vie XR complet (événements READY→FOCUSED, STOPPING→End) ;
//   - la boucle de frame OpenXR-style (WaitFrame → BeginFrame → LocateViews
//     au predictedDisplayTime → rendu par œil → EndFrame avec couche) ;
//   - deux yeux séparés d'un IPD réel, souris = tête, ZQSD/WASD = déplacement ;
//   - les entrées par ACTIONS (clic gauche = « sélectionner » teinte le cube).
//
// Architecture rendu (patron éprouvé NK3DModeler/NkAnimaEditor — AUCUNE passe
// de NKRenderer modifiée, c'est la contrainte de l'étage 0) :
//   rMain (For2D)   : possède la frame — BeginFrame/Present/EndFrame, compose
//                     les deux yeux côte à côte via Render2D::DrawImage ;
//   rEye[2] (ForGame): un renderer par œil, MÊME device, rendu offscreen en
//                     mode partagé — jamais BeginFrame, on rejoue à la main
//                     FlushGraphRebuilds + ResetFrame + Upload (piège n°8 du
//                     modeleur), puis graph->Execute(cmd) AVANT le Present du
//                     compositeur (passes imbriquées interdites sous Vulkan).
//   Un renderer par œil parce que SetFinalColorTarget/SetRenderSizeOverride
//   RECONSTRUISENT le render graph : basculer une cible par frame est banni.
//
// Crochets d'agent (captures déterministes, cf. boucle du modeleur) :
//   NK_XR_SIM_POSE="yaw,pitch,x,y,z"  fige la tête (module NKXR)
//   NK_XR_SHOT=<frame>                capture les deux yeux en PNG à la frame
//   NK_XR_SHOT_PREFIX=<prefixe>       défaut "nkxr" -> nkxr_L.png / nkxr_R.png
//   NK_XR_EXIT=<frame>                sortie propre (par RequestExit → End)
//   L'animation est cadencée sur l'INDEX de frame, pas l'horloge murale :
//   même frame => même image, le diff pixel a un sens.
// =============================================================================
#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKTime/NkTime.h"
#include "NKLogger/NkLog.h"

#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Core/NkRenderGraph.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Materials/NkMaterialCollection.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKRenderer/Tools/Shadow/NkVirtualShadowMaps.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKRenderer/Tools/Overlay/NkOverlayRenderer.h"

#include "NKXR/NKXR.h"

#include <cstdlib>
#include <cstdio> // snprintf : chemins des captures d'agent, comme le modeleur

// Win32 définit DrawText en macro (GDI) — collision avec NkOverlayRenderer::DrawText.
#ifdef DrawText
#undef DrawText
#endif

using namespace nkentseu;
using namespace nkentseu::renderer;
namespace nkxr = nkentseu::xr;

static void ConfigureAppData(NkAppData &d) {
	d.appName = "NKXRDemo";
}
NK_REGISTER_ENTRY_APPDATA_UPDATER(ConfigureAppData)

namespace {

	// La scène, soumise à l'IDENTIQUE aux deux yeux : la stéréo vient des
	// caméras, jamais du contenu. selectTint prouve le chemin des actions.
	void SubmitScene(NkRender3D *r3d, NkMeshSystem *meshes, float32 animTime, bool selectHeld) {
		{ // sol
			NkDrawCall3D dc;
			dc.mesh = meshes->GetPlane();
			dc.transform = NkMat4f::Scale({ 12.f, 1.f, 12.f });
			dc.aabb = { { -12.f, 0.f, -12.f }, { 12.f, 0.f, 12.f } };
			dc.castShadow = false;
			dc.tint = { 0.14f, 0.14f, 0.16f };
			dc.metallic = 1.f;
			dc.roughness = 1.f;
			r3d->Submit(dc);
		}
		{ // cube pivot, à 2 m devant la position de départ (STAGE : y=0 au sol)
			NkDrawCall3D dc;
			dc.mesh = meshes->GetCube();
			dc.transform = NkMat4f::Translate({ 0.f, 1.f, -2.f }) *
						   NkMat4f::RotationY(NkAngle::FromRad(animTime * 0.8f)) *
						   NkMat4f::Scale({ 0.5f, 0.5f, 0.5f });
			dc.aabb = { { -0.8f, 0.2f, -2.8f }, { 0.8f, 1.8f, -1.2f } };
			dc.tint = selectHeld ? NkVec3f{ 1.f, 0.25f, 0.2f } : NkVec3f{ 1.f, 0.8f, 0.3f };
			dc.metallic = 1.f;
			dc.roughness = 0.2f;
			r3d->Submit(dc);
		}
		{ // sphère décalée : la disparité gauche/droite se lit dessus
			NkDrawCall3D dc;
			dc.mesh = meshes->GetSphere();
			// Cube/sphère unitaires du MeshSystem = demi-étendue 0,5 : une
			// échelle S donne un diamètre S — poser au sol = translater de S/2.
			dc.transform = NkMat4f::Translate({ 1.2f, 0.35f, -1.2f }) * NkMat4f::Scale({ 0.7f, 0.7f, 0.7f });
			dc.aabb = { { 0.8f, 0.f, -1.6f }, { 1.6f, 0.8f, -0.8f } };
			dc.tint = { 0.9f, 0.9f, 0.95f };
			dc.metallic = 0.f;
			dc.roughness = 1.f;
			r3d->Submit(dc);
		}
		// Quatre piliers : des repères verticaux à des profondeurs étagées —
		// c'est sur eux que l'œil juge la parallaxe en tournant la tête.
		const NkVec3f pillars[4] = { { -3.f, 0.f, -4.f }, { 3.f, 0.f, -4.f }, { -3.f, 0.f, 2.f }, { 3.f, 0.f, 2.f } };
		const NkVec3f colors[4] = { { 0.8f, 0.3f, 0.3f }, { 0.3f, 0.8f, 0.3f }, { 0.3f, 0.4f, 0.9f }, { 0.9f, 0.8f, 0.2f } };
		for (int i = 0; i < 4; ++i) {
			NkDrawCall3D dc;
			dc.mesh = meshes->GetCube();
			dc.transform = NkMat4f::Translate({ pillars[i].x, 1.25f, pillars[i].z }) *
						   NkMat4f::Scale({ 0.25f, 2.5f, 0.25f });
			dc.aabb = { { pillars[i].x - 0.3f, 0.f, pillars[i].z - 0.3f },
						{ pillars[i].x + 0.3f, 2.6f, pillars[i].z + 0.3f } };
			dc.tint = colors[i];
			dc.metallic = 0.f;
			dc.roughness = 0.7f;
			r3d->Submit(dc);
		}
	}

	// Caméra NKRenderer depuis une vue XR. L'étage 0 SYMÉTRISE le FOV
	// (NkCamera3D ne connaît que fovY+aspect) : exact tant que le simulateur
	// rend des FOV symétriques — l'asymétrie attend l'étage 1.
	NkCamera3D CameraFromXrView(const nkxr::NkXrView &view) {
		NkCamera3DData cd;
		cd.position = view.position;
		cd.target = view.position + nkxr::NkXrForward(view.orientation);
		cd.up = nkxr::NkXrUp(view.orientation);
		cd.fovY = 2.f * view.fov.angleUp * 180.f / math::NK_PI_F;
		const float32 tanW = math::NkTan(view.fov.angleRight) - math::NkTan(view.fov.angleLeft);
		const float32 tanH = math::NkTan(view.fov.angleUp) - math::NkTan(view.fov.angleDown);
		cd.aspect = (tanH > 1e-6f) ? (tanW / tanH) : 1.f;
		cd.nearPlane = 0.05f;
		cd.farPlane = 200.f;
		return NkCamera3D(cd);
	}

	uint64 EnvU64(const char *name, uint64 fallback) {
		const char *text = getenv(name);
		if (text == nullptr || *text == '\0') {
			return fallback;
		}
		return uint64(atoll(text));
	}

} // namespace

int nkmain(const NkEntryState &state) {
	(void)state;

	const uint64 shotFrame = EnvU64("NK_XR_SHOT", 0u);
	const uint64 exitFrame = EnvU64("NK_XR_EXIT", 0u);
	const char *shotPrefixEnv = getenv("NK_XR_SHOT_PREFIX");
	const char *shotPrefix = (shotPrefixEnv && *shotPrefixEnv) ? shotPrefixEnv : "nkxr";

	// ── 1) Fenêtre ────────────────────────────────────────────────────────────
	NkWindowConfig winCfg;
	winCfg.title = "NKXRDemo — simulateur stéréo (souris = tête, ZQSD/WASD, Échap = quitter)";
	winCfg.width = 1280;
	winCfg.height = 720;
	winCfg.centered = true;
	winCfg.resizable = true;

	NkWindow window(winCfg);
	if (!window.IsValid()) {
		logger.Error("[NKXRDemo] Création fenêtre KO");
		return 1;
	}

	// ── 2) Device + compositeur + renderers d'œil ─────────────────────────────
	NkDeviceInitInfo devInfo{};
	devInfo.surface = window.GetSurfaceDesc();
	devInfo.width = (uint32)window.GetSize().width;
	devInfo.height = (uint32)window.GetSize().height;

	NkIDevice *device = NkDeviceFactory::CreateAutoDetect(devInfo);
	if (!device || !device->IsValid()) {
		logger.Error("[NKXRDemo] Création device KO");
		window.Close();
		return 2;
	}

	uint32 W = devInfo.width;
	uint32 H = devInfo.height;

	// Le compositeur possède la frame ; 2D seul, mais OFFSCREEN activé car
	// c'est LUI qui crée les cibles d'œil (leurs handles doivent vivre dans SA
	// NkTextureLibrary pour que DrawImage les connaisse — il n'existe aucun
	// import de texture RHI externe dans la bibliothèque).
	NkRendererConfig cfgMain = NkRendererConfig::For2D(devInfo.api, W, H);
	cfgMain.Enable(NK_SS_OFFSCREEN);
	NkRenderer *rMain = NkRenderer::Create(device, cfgMain);
	if (!rMain) {
		logger.Error("[NKXRDemo] Init compositeur KO");
		NkDeviceFactory::Destroy(device);
		window.Close();
		return 3;
	}

	// ── 3) Session XR (simulateur) ────────────────────────────────────────────
	nkxr::NkXrSessionDesc xrDesc;
	xrDesc.window = &window;
	nkxr::NkXrSession *xrSession = nkxr::NkXrSession::Create(xrDesc);
	if (xrSession == nullptr) {
		logger.Error("[NKXRDemo] Création session XR KO");
		NkRenderer::Destroy(rMain);
		NkDeviceFactory::Destroy(device);
		window.Close();
		return 4;
	}
	const nkxr::NkXrSystemInfo xrInfo = xrSession->GetSystemInfo();
	uint32 eyeW = xrInfo.views[0].recommendedWidth;
	uint32 eyeH = xrInfo.views[0].recommendedHeight;
	logger.Infof("[NKXRDemo] Système : %s — %ux%u par œil, IPD %.1f mm\n",
				 xrInfo.systemName, eyeW, eyeH, xrInfo.ipdMeters * 1000.f);

	// Cibles d'œil + un renderer 3D complet par œil (mode partagé).
	NkOffscreenTarget *eyeTargets[nkxr::NK_XR_EYE_COUNT] = { nullptr, nullptr };
	NkRenderer *rEye[nkxr::NK_XR_EYE_COUNT] = { nullptr, nullptr };
	nkxr::NkXrSwapchain *eyeSwapchains[nkxr::NK_XR_EYE_COUNT] = { nullptr, nullptr };
	for (uint32 e = 0; e < nkxr::NK_XR_EYE_COUNT; ++e) {
		NkOffscreenDesc od;
		od.width = eyeW;
		od.height = eyeH;
		od.hasDepth = true;
		od.hdr = false;
		od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
		od.readable = true;
		od.readback = true; // captures d'agent par œil
		const char *eyeName = (e == 0) ? "NKXRDemo_OeilGauche" : "NKXRDemo_OeilDroit";
		od.name = eyeName;
		eyeTargets[e] = rMain->CreateOffscreen(od);
		if (eyeTargets[e] == nullptr) {
			logger.Errorf("[NKXRDemo] Cible œil %u KO\n", e);
			return 5;
		}

		NkRendererConfig cfgEye = NkRendererConfig::ForGame(devInfo.api, eyeW, eyeH);
		// Par œil : la 3D seule. Le 2D/texte/overlay appartient au compositeur,
		// et chaque sous-système en moins est une passe en moins × 2 yeux.
		cfgEye.Disable(NK_SS_RENDER2D);
		cfgEye.Disable(NK_SS_TEXT);
		cfgEye.Disable(NK_SS_OVERLAY);
		cfgEye.Enable(NK_SS_OFFSCREEN);
		rEye[e] = NkRenderer::Create(device, cfgEye);
		if (rEye[e] == nullptr) {
			logger.Errorf("[NKXRDemo] Renderer œil %u KO\n", e);
			return 6;
		}
		// L'override de taille va TOUJOURS avec la cible : sans lui le graphe
		// rendrait à la taille de la FENÊTRE dans une cible de demi-fenêtre.
		rEye[e]->SetRenderSizeOverride(eyeW, eyeH);
		rEye[e]->SetFinalColorTarget(rMain->GetTextures()->GetRHIHandle(eyeTargets[e]->GetColorResult()));
		rEye[e]->SetBackgroundColor({ 0.05f, 0.06f, 0.09f, 1.f });
		// Cascades d'ombres ancrées au MONDE (auto-fit aux casters) : sans ça,
		// la cascade se recale sur la caméra à chaque frame et les surfaces
		// clignotent clair/foncé dès que la tête bouge (le « swimming » déjà
		// documenté par Tutoriels3D/04-Camera). En VR la tête bouge TOUJOURS.
		if (auto *shadow = rEye[e]->GetShadow()) {
			shadow->GetConfig().autoFitDirectional = true;
		}

		nkxr::NkXrSwapchainDesc scDesc;
		scDesc.width = eyeW;
		scDesc.height = eyeH;
		scDesc.imageCount = 1; // une cible par œil : le compositeur lit APRÈS le rendu
		scDesc.eye = nkxr::NkXrEye(e);
		scDesc.name = eyeName;
		eyeSwapchains[e] = xrSession->CreateSwapchain(scDesc);
		eyeSwapchains[e]->SetImageNative(0, uint64(rMain->GetTextures()->GetRHIHandle(eyeTargets[e]->GetColorResult()).id));
	}

	// ── 4) Actions ────────────────────────────────────────────────────────────
	nkxr::NkXrActionSet actions;
	const nkxr::NkXrActionHandle actSelect = actions.CreateAction(
		{ "selectionner", nkxr::NkXrActionType::NK_XR_ACTION_BOOL, nkxr::NkXrActionUsage::NK_XR_USAGE_SELECT });
	const nkxr::NkXrActionHandle actAim = actions.CreateAction(
		{ "viser", nkxr::NkXrActionType::NK_XR_ACTION_POSE, nkxr::NkXrActionUsage::NK_XR_USAGE_AIM_POSE });
	xrSession->AttachActionSet(actions);

	// ── 5) Événements fenêtre ─────────────────────────────────────────────────
	bool running = true;
	bool pendingResize = false;
	uint32 pendingW = W, pendingH = H;
	NkEventSystem &events = NkEvents();

	events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) {
		// La fenêtre EST l'affichage du simulateur : sans elle, inutile de
		// dérouler la fin de session en douceur.
		running = false;
	});
	events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent *e) {
		if (e->GetKey() == NkKey::NK_ESCAPE) {
			// Échap suit le chemin PROPRE : RequestExit → STOPPING → End —
			// exactement ce qu'exigera le runtime du casque.
			xrSession->RequestExit();
		}
	});
	events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent *e) {
		const uint32 w = (uint32)e->GetWidth(), h = (uint32)e->GetHeight();
		if (w >= 64 && h >= 32 && (w != W || h != H)) {
			// >= 64 : ApplyRenderSize refuse < 32 px, et un œil = w/2.
			pendingResize = true;
			pendingW = w;
			pendingH = h;
		}
	});

	// Souris façon FPS : cachée + confinée, la tête lit le delta brut.
	window.ShowMouse(false);
	window.ClipMouseToClient(true);

	// ── 6) Boucle principale ──────────────────────────────────────────────────
	nkxr::NkXrSpace stageSpace(nkxr::NkXrSpaceType::NK_XR_SPACE_STAGE);
	uint64 frameIdx = 0;
	bool sessionRunning = false;

	while (running && window.IsOpen()) {
		events.PollEvents();
		if (!running) {
			break;
		}

		// La pompe d'événements XR : c'est ELLE qui pilote le cycle de vie.
		nkxr::NkXrEvent xev;
		while (xrSession->PollEvent(xev)) {
			if (xev.type != nkxr::NkXrEventType::NK_XR_EVENT_STATE_CHANGED) {
				continue;
			}
			switch (xev.state) {
				case nkxr::NkXrSessionState::NK_XR_STATE_READY: {
					sessionRunning = xrSession->Begin();
					break;
				}
				case nkxr::NkXrSessionState::NK_XR_STATE_STOPPING: {
					xrSession->End();
					sessionRunning = false;
					break;
				}
				case nkxr::NkXrSessionState::NK_XR_STATE_EXITING: {
					running = false;
					break;
				}
				default: {
					break;
				}
			}
		}
		if (!running || !sessionRunning) {
			continue;
		}

		if (pendingResize) {
			pendingResize = false;
			W = pendingW;
			H = pendingH;
			eyeW = W / 2;
			eyeH = H;
			rMain->OnResize(W, H);
			for (uint32 e = 0; e < nkxr::NK_XR_EYE_COUNT; ++e) {
				eyeTargets[e]->Resize(eyeW, eyeH);
				rEye[e]->SetRenderSizeOverride(eyeW, eyeH);
				// La cible a pu être réallouée : re-pointer le graphe dessus.
				rEye[e]->SetFinalColorTarget(rMain->GetTextures()->GetRHIHandle(eyeTargets[e]->GetColorResult()));
				xrSession->DestroySwapchain(eyeSwapchains[e]);
				nkxr::NkXrSwapchainDesc scDesc;
				scDesc.width = eyeW;
				scDesc.height = eyeH;
				scDesc.imageCount = 1;
				scDesc.eye = nkxr::NkXrEye(e);
				eyeSwapchains[e] = xrSession->CreateSwapchain(scDesc);
				eyeSwapchains[e]->SetImageNative(0, uint64(rMain->GetTextures()->GetRHIHandle(eyeTargets[e]->GetColorResult()).id));
			}
		}

		// ── La boucle de frame XR ────────────────────────────────────────────
		nkxr::NkXrFrameState frameState;
		if (!xrSession->WaitFrame(frameState)) {
			continue;
		}
		xrSession->BeginFrame();
		++frameIdx;

		xrSession->SyncActions();
		nkxr::NkXrActionStateBool selectState;
		xrSession->GetActionStateBool(actSelect, selectState);

		// Les poses des yeux, PRÉDITES pour l'instant d'affichage.
		nkxr::NkXrView views[nkxr::NK_XR_EYE_COUNT];
		const bool haveViews = xrSession->LocateViews(stageSpace, frameState.predictedDisplayTime, views);

		// Cadence d'animation sur l'index de frame : le déterminisme des
		// captures avant le confort visuel — c'est une démo de VÉRIFICATION.
		const float32 animTime = float32(frameIdx) / 72.f;

		if (frameState.shouldRender && haveViews && rMain->BeginFrame()) {
			NkICommandBuffer *cmd = rMain->GetCmd();

			for (uint32 e = 0; e < nkxr::NK_XR_EYE_COUNT; ++e) {
				uint32 imageIndex = 0;
				eyeSwapchains[e]->AcquireImage(imageIndex);

				NkRenderer *r = rEye[e];
				NkRender3D *r3d = r->GetRender3D();
				// Ce que BeginFrame ferait si ce renderer possédait la frame
				// (mode partagé — oublier ResetFrame = UBOs piétinés entre
				// les deux yeux, le bug déjà payé par la passe miroir).
				r->FlushGraphRebuilds();
				r3d->ResetFrame();
				if (auto *mc = r->GetMaterialCollection()) {
					mc->Upload();
				}

				NkSceneContext sctx;
				sctx.camera = CameraFromXrView(views[e]);
				sctx.time = animTime;
				sctx.ambientIntensity = 0.15f;
				NkLightDesc sun;
				sun.type = NkLightType::NK_DIRECTIONAL;
				sun.direction = { -0.4f, -1.f, -0.3f };
				sun.color = { 1.f, 0.95f, 0.85f };
				sun.intensity = 3.f;
				sun.castShadow = true;
				sun.shadowStatic = false;
				sctx.lights.PushBack(sun);

				r3d->BeginScene(sctx);
				SubmitScene(r3d, r->GetMeshSystem(), animTime, selectState.current);
				// Le graphe de l'œil s'exécute sur le command buffer du
				// COMPOSITEUR, avant son Present (passes imbriquées interdites).
				if (auto *graph = r->GetRenderGraph()) {
					graph->Execute(cmd);
				}

				eyeSwapchains[e]->ReleaseImage();
			}

			// Composition côte à côte + HUD, dans la passe Overlay2D de rMain.
			NkOverlayRenderer *overlay = rMain->GetOverlay();
			NkRender2D *r2d = rMain->GetRender2D();
			if (overlay && r2d) {
				overlay->BeginOverlay(cmd, W, H);
				r2d->DrawImage(eyeTargets[0]->GetColorResult(), { 0.f, 0.f, float32(W) * 0.5f, float32(H) });
				r2d->DrawImage(eyeTargets[1]->GetColorResult(), { float32(W) * 0.5f, 0.f, float32(W) * 0.5f, float32(H) });
				// Séparateur central : sans lui, l'œil cherche la couture.
				r2d->FillRect({ float32(W) * 0.5f - 1.f, 0.f, 2.f, float32(H) }, { 0.f, 0.f, 0.f, 1.f });
				overlay->DrawText({ 12.f, 20.f }, "NKXRDemo — étage 0 : stéréo SIMULÉE (NKXR Simulator)");
				overlay->DrawText({ 12.f, 40.f }, "souris = tête | ZQSD/WASD | Espace/C = monter/descendre | Maj = sprint");
				overlay->DrawText({ 12.f, 60.f }, "clic gauche = action « sélectionner » (le cube rougit) | Échap = quitter");
				overlay->EndOverlay();
			}

			rMain->Present();
			rMain->EndFrame();
		}

		// EndFrame TOUJOURS soumis (même sans rendu) : la boucle reste calée.
		nkxr::NkXrLayerProjection projLayer;
		projLayer.space = nkxr::NkXrSpaceType::NK_XR_SPACE_STAGE;
		for (uint32 e = 0; e < nkxr::NK_XR_EYE_COUNT; ++e) {
			projLayer.views[e].pose.position = views[e].position;
			projLayer.views[e].pose.orientation = views[e].orientation;
			projLayer.views[e].fov = views[e].fov;
			projLayer.views[e].swapchain = eyeSwapchains[e];
			projLayer.views[e].imageIndex = eyeSwapchains[e]->GetLastReleasedIndex();
		}
		nkxr::NkXrFrameEndInfo endInfo;
		endInfo.displayTime = frameState.predictedDisplayTime;
		endInfo.projection = haveViews ? &projLayer : nullptr;
		xrSession->EndFrame(endInfo);

		// ── Crochets d'agent ─────────────────────────────────────────────────
		if (shotFrame != 0 && frameIdx == shotFrame) {
			device->WaitIdle();
			char path[256];
			snprintf(path, sizeof(path), "%s_L.png", shotPrefix);
			const bool okL = eyeTargets[0]->Capture(path);
			snprintf(path, sizeof(path), "%s_R.png", shotPrefix);
			const bool okR = eyeTargets[1]->Capture(path);
			logger.Infof("[NKXRDemo] NK_XR_SHOT frame %llu -> %s_{L,R}.png (%s/%s)\n",
						 (unsigned long long)frameIdx, shotPrefix, okL ? "ok" : "ECHEC", okR ? "ok" : "ECHEC");
		}
		if (exitFrame != 0 && frameIdx == exitFrame) {
			xrSession->RequestExit();
		}
	}

	// ── 7) Fermeture propre ───────────────────────────────────────────────────
	device->WaitIdle();
	for (uint32 e = 0; e < nkxr::NK_XR_EYE_COUNT; ++e) {
		if (eyeSwapchains[e] != nullptr) {
			xrSession->DestroySwapchain(eyeSwapchains[e]);
		}
		NkRenderer::Destroy(rEye[e]);
		rMain->DestroyOffscreen(eyeTargets[e]);
	}
	nkxr::NkXrSession::Destroy(xrSession);
	NkRenderer::Destroy(rMain);
	NkDeviceFactory::Destroy(device);
	window.Close();
	logger.Info("[NKXRDemo] Terminé proprement.");
	return 0;
}
