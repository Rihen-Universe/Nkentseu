// =============================================================================
// Tutoriels3D / Étape 4 — CAMÉRA INTERACTIVE (souris + clavier)
//
// Même scène qu'à l'étape 3, mais la caméra devient vivante :
//   clic MILIEU + glisser         = orbiter autour de la scène
//   Shift + clic MILIEU + glisser = panoramique
//   molette                       = zoom
//   W/A/S/D                       = déplacer la cible de la caméra
//
// Toute la traduction événements -> caméra vit dans un FICHIER SÉPARÉ,
// camera_input.h : le main reste concentré sur la scène. C'est le découpage
// recommandé : le contrôleur du moteur (NkOrbitCameraController3D) ne connaît
// pas NKEvent, l'application fait le pont — une seule fois, dans un seul fichier.
// =============================================================================
#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkSafeArea.h"
#include "NKTime/NkTime.h"
#include "NKLogger/NkLog.h"

#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Tools/Shadow/NkVirtualShadowMaps.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKRenderer/Tools/Overlay/NkOverlayRenderer.h"

#include "camera_input.h" // <- le second fichier : souris/clavier -> caméra orbite

// Win32 définit DrawText en macro (GDI) — collision avec NkOverlayRenderer::DrawText.
#ifdef DrawText
#undef DrawText
#endif

using namespace nkentseu;
using namespace nkentseu::renderer;

static void ConfigureAppData(NkAppData &d) {
	d.appName = "Tuto04Camera";
}
NK_REGISTER_ENTRY_APPDATA_UPDATER(ConfigureAppData)

int nkmain(const NkEntryState &state) {
	(void)state;

	// ── 1) Fenêtre ────────────────────────────────────────────────────────────
	NkWindowConfig winCfg;
	winCfg.title = "Tuto 04 — Camera orbite (milieu/Shift+milieu/molette, WASD)";
	winCfg.width = 1280;
	winCfg.height = 720;
	winCfg.centered = true;
	winCfg.resizable = true;

	NkWindow window(winCfg);
	if (!window.IsValid()) {
		logger.Error("[Tuto04] Creation fenetre KO");
		return 1;
	}

	// ── 2) Device + renderer (comme aux étapes 2-3) ───────────────────────────
	NkDeviceInitInfo devInfo{};
	devInfo.surface = window.GetSurfaceDesc();
	devInfo.width = (uint32)window.GetSize().width;
	devInfo.height = (uint32)window.GetSize().height;

	NkIDevice *device = NkDeviceFactory::CreateAutoDetect(devInfo);
	if (!device || !device->IsValid()) {
		logger.Error("[Tuto04] Creation device KO");
		window.Close();
		return 2;
	}

	NkRendererConfig cfg = NkRendererConfig::ForGame(devInfo.api, devInfo.width, devInfo.height);
	NkRenderer *renderer = NkRenderer::Create(device, cfg);
	if (!renderer || !renderer->Initialize()) {
		logger.Error("[Tuto04] Init renderer KO");
		NkRenderer::Destroy(renderer);
		NkDeviceFactory::Destroy(device);
		window.Close();
		return 3;
	}

	NkRender3D *r3d = renderer->GetRender3D();
	NkRender2D *r2d = renderer->GetRender2D();
	NkOverlayRenderer *overlay = renderer->GetOverlay();

	// Ombres STABLES malgré la caméra mobile : cascade ancrée au monde (auto-fit
	// aux casters), sinon les bords d'ombre vibrent pendant l'orbite (swimming).
	if (auto *shadow = renderer->GetShadow())
		shadow->GetConfig().autoFitDirectional = true;

	NkMeshHandle meshCube = renderer->GetMeshSystem()->GetCube();
	NkMeshHandle meshSphere = renderer->GetMeshSystem()->GetSphere();
	NkMeshHandle meshPlane = renderer->GetMeshSystem()->GetPlane();

	// ── 3) Événements fenêtre + LA CAMÉRA INTERACTIVE ─────────────────────────
	bool running = true;
	uint32 W = devInfo.width;
	uint32 H = devInfo.height;
	NkEventSystem &events = NkEvents();

	events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) { running = false; });
	events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent *e) {
		if (e->GetKey() == NkKey::NK_ESCAPE)
			running = false;
	});
	events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent *e) {
		const uint32 w = (uint32)e->GetWidth(), h = (uint32)e->GetHeight();
		if (w > 0 && h > 0 && (w != W || h != H)) {
			W = w;
			H = h;
			renderer->OnResize(w, h);
		}
	});

	// Deux lignes suffisent pour brancher la caméra (tout est dans camera_input.h).
	tuto::TutoCameraInput camInput;
	camInput.orbit.SetCenter({0.f, 0.5f, 0.f}, 6.f, 0.9f, -0.35f);
	camInput.Install();

	// ── 4) Boucle principale ──────────────────────────────────────────────────
	NkClock clock;
	float32 total = 0.f;

	while (running && window.IsOpen()) {
		events.PollEvents();
		if (!running)
			break;

		const float32 dt = clock.Tick().delta;
		total += dt;
		camInput.Update(dt); // WASD (état clavier continu)

		if (!renderer->BeginFrame())
			continue;

		NkCamera3DData camData;
		camData.up = {0.f, 1.f, 0.f};
		camData.fovY = 60.f;
		camData.aspect = (float32)W / (float32)H;
		camData.nearPlane = 0.1f;
		camData.farPlane = 100.f;
		NkCamera3D cam(camData);
		camInput.orbit.Apply(cam); // <- la caméra suit la souris/le clavier

		NkSceneContext sctx;
		sctx.camera = cam;
		sctx.time = total;
		sctx.ambientIntensity = 0.15f;

		NkLightDesc sun;
		sun.type = NkLightType::NK_DIRECTIONAL;
		sun.direction = {-0.4f, -1.f, -0.3f};
		sun.color = {1.f, 0.95f, 0.85f};
		sun.intensity = 3.f;
		sun.castShadow = true;
		sun.shadowStatic = false; // caméra mobile + cube en rotation -> pas de cache d'ombre
		sctx.lights.PushBack(sun);

		r3d->BeginScene(sctx);

		{ // sol
			NkDrawCall3D dc;
			dc.mesh = meshPlane;
			dc.transform = NkMat4f::Scale({12.f, 1.f, 12.f});
			dc.aabb = {{-12.f, 0.f, -12.f}, {12.f, 0.f, 12.f}};
			dc.castShadow = false;
			dc.tint = {0.14f, 0.14f, 0.16f};
			dc.metallic = 1.f;
			dc.roughness = 1.0f;
			r3d->Submit(dc);
		}
		{ // cube doré en rotation
			NkDrawCall3D dc;
			dc.mesh = meshCube;
			dc.transform = NkMat4f::Translate({0.f, 0.5f, 0.f}) *
						   NkMat4f::RotationY(NkAngle::FromRad(total * 0.8f)) *
						   NkMat4f::Scale({0.8f, 0.8f, 0.8f});
			dc.aabb = {{-0.6f, -0.1f, -0.6f}, {0.6f, 1.1f, 0.6f}};
			dc.tint = {1.f, 0.8f, 0.3f};
			dc.metallic = 1.f;
			dc.roughness = 0.2f;
			r3d->Submit(dc);
		}
		{ // sphère
			NkDrawCall3D dc;
			dc.mesh = meshSphere;
			dc.transform = NkMat4f::Translate({1.8f, 0.5f, 0.f}) * NkMat4f::Scale({0.5f, 0.5f, 0.5f});
			dc.aabb = {{1.3f, 0.f, -0.5f}, {2.3f, 1.f, 0.5f}};
			dc.tint = {0.9f, 0.9f, 0.95f};
			dc.metallic = 0.f;
			dc.roughness = 1.0f;
			r3d->Submit(dc);
		}

		if (overlay) {
			// Safe area : sur mobile (encoche, barres système) on décale le
			// panneau pour rester dans la zone visible ; ailleurs insets = 0.
			const NkSafeAreaInsets sa = window.GetSafeAreaInsets();
			const float32 ox = 10.f + sa.left, oy = 10.f + sa.top;
			overlay->BeginOverlay(renderer->GetCmd(), W, H);
			if (r2d)
				r2d->FillRect({ox, oy, 420.f, 90.f}, {0.f, 0.f, 0.f, 0.6f});
			overlay->DrawText({ox + 10.f, oy + 20.f}, "== Tuto 04 : camera interactive ==");
			overlay->DrawText({ox + 10.f, oy + 40.f}, "Orbite: drag (ou 1 doigt) | Pan: Shift+drag (2 doigts)");
			overlay->DrawText({ox + 10.f, oy + 60.f}, "Zoom: molette/pincer | WASD | FPS ~ %.1f",
							  dt > 1e-4f ? 1.f / dt : 0.f);
			overlay->DrawText({ox + 10.f, oy + 80.f}, "Echap = quitter");
			overlay->EndOverlay();
		}

		renderer->Present();
		renderer->EndFrame();
	}

	// ── 5) Fermeture propre ───────────────────────────────────────────────────
	device->WaitIdle();
	NkRenderer::Destroy(renderer);
	NkDeviceFactory::Destroy(device);
	window.Close();
	logger.Info("[Tuto04] Termine proprement.");
	return 0;
}
