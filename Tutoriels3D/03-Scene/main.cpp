// =============================================================================
// Tutoriels3D / Étape 3 — UNE VRAIE SCÈNE 3D
//
// On ajoute à l'étape 2 :
//   • une caméra 3D (position + cible),
//   • un soleil directionnel qui projette des OMBRES,
//   • trois objets : un sol, un cube doré en rotation, une sphère —
//     chaque objet = un NkDrawCall3D (mesh + transform + matériau PBR).
//
// Contrôles : Échap ou fermeture de la fenêtre = quitter.
// (La caméra est encore fixe : elle devient interactive à l'étape 4.)
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

// Win32 définit DrawText en macro (GDI) — collision avec NkOverlayRenderer::DrawText.
#ifdef DrawText
#undef DrawText
#endif

using namespace nkentseu;
using namespace nkentseu::renderer;

static void ConfigureAppData(NkAppData &d) {
	d.appName = "Tuto03Scene";
}
NK_REGISTER_ENTRY_APPDATA_UPDATER(ConfigureAppData)

int nkmain(const NkEntryState &state) {
	(void)state;

	// ── 1) Fenêtre ────────────────────────────────────────────────────────────
	NkWindowConfig winCfg;
	winCfg.title = "Tuto 03 — Cube + sphere + sol + ombres";
	winCfg.width = 1280;
	winCfg.height = 720;
	winCfg.centered = true;
	winCfg.resizable = true;

	NkWindow window(winCfg);
	if (!window.IsValid()) {
		logger.Error("[Tuto03] Creation fenetre KO");
		return 1;
	}

	// ── 2) Device GPU (auto-détection du meilleur backend) ────────────────────
	NkDeviceInitInfo devInfo{};
	devInfo.surface = window.GetSurfaceDesc();
	devInfo.width = (uint32)window.GetSize().width;
	devInfo.height = (uint32)window.GetSize().height;

	NkIDevice *device = NkDeviceFactory::CreateAutoDetect(devInfo);
	if (!device || !device->IsValid()) {
		logger.Error("[Tuto03] Creation device KO");
		window.Close();
		return 2;
	}
	logger.Info("[Tuto03] Backend : {0}", NkGraphicsApiName(device->GetApi()));

	// ── 3) Renderer haut niveau ───────────────────────────────────────────────
	NkRendererConfig cfg = NkRendererConfig::ForGame(devInfo.api, devInfo.width, devInfo.height);
	NkRenderer *renderer = NkRenderer::Create(device, cfg);
	if (!renderer || !renderer->Initialize()) {
		logger.Error("[Tuto03] Init renderer KO");
		NkRenderer::Destroy(renderer);
		NkDeviceFactory::Destroy(device);
		window.Close();
		return 3;
	}

	NkRender3D *r3d = renderer->GetRender3D();
	NkRender2D *r2d = renderer->GetRender2D();
	NkOverlayRenderer *overlay = renderer->GetOverlay();

	// Ombres STABLES : la cascade directionnelle s'ajuste chaque frame aux bornes
	// réelles des objets (ancrée au MONDE). Sans ça, elle est recadrée sur la vue
	// et les bords d'ombre "vibrent" dès que la caméra bouge (shadow swimming).
	if (auto *shadow = renderer->GetShadow())
		shadow->GetConfig().autoFitDirectional = true;

	// Les primitives sont fournies par le moteur : aucun fichier à charger.
	NkMeshHandle meshCube = renderer->GetMeshSystem()->GetCube();
	NkMeshHandle meshSphere = renderer->GetMeshSystem()->GetSphere();
	NkMeshHandle meshPlane = renderer->GetMeshSystem()->GetPlane();

	// ── 4) Événements ─────────────────────────────────────────────────────────
	bool running = true;
	uint32 W = devInfo.width;
	uint32 H = devInfo.height;
	NkEventSystem &events = NkEvents();

	events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) { running = false; });
	events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent *e) {
		if (e->GetKey() == NkKey::NK_ESCAPE)
			running = false;
	});
	// Resize appliqué immédiatement (DX12 flip-model n'aime pas les present à
	// l'ancienne taille) ; le no-op même-taille est géré côté device.
	events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent *e) {
		const uint32 w = (uint32)e->GetWidth(), h = (uint32)e->GetHeight();
		if (w > 0 && h > 0 && (w != W || h != H)) {
			W = w;
			H = h;
			renderer->OnResize(w, h);
		}
	});

	// ── 5) Boucle principale ──────────────────────────────────────────────────
	NkClock clock;
	float32 total = 0.f;

	while (running && window.IsOpen()) {
		events.PollEvents();
		if (!running)
			break;

		const float32 dt = clock.Tick().delta;
		total += dt;

		if (!renderer->BeginFrame())
			continue; // fenêtre minimisée, etc.

		// ── Scène 3D : caméra + soleil ───────────────────────────────────────
		NkCamera3DData camData;
		camData.up = {0.f, 1.f, 0.f};
		camData.fovY = 60.f;
		camData.aspect = (float32)W / (float32)H;
		camData.nearPlane = 0.1f;
		camData.farPlane = 100.f;
		NkCamera3D cam(camData);
		cam.SetPosition({3.5f, 2.5f, 4.5f});
		cam.SetTarget({0.f, 0.5f, 0.f});

		NkSceneContext sctx;
		sctx.camera = cam;
		sctx.time = total;
		sctx.ambientIntensity = 0.15f;

		NkLightDesc sun;
		sun.type = NkLightType::NK_DIRECTIONAL;
		sun.direction = {-0.4f, -1.f, -0.3f};
		sun.color = {1.f, 0.95f, 0.85f};
		sun.intensity = 3.f;
		// shadowStatic=false : la shadow map est re-rendue chaque frame. Le cache
		// (shadowStatic=true) ne convient qu'aux scènes 100% figées : il ne suit
		// ni les objets qui bougent (notre cube tourne) ni le recadrage de
		// l'ombre directionnelle quand la caméra se déplace.
		sun.castShadow = true;
		sun.shadowStatic = false;
		sctx.lights.PushBack(sun);

		r3d->BeginScene(sctx);

		// Sol : grand plan gris, récepteur d'ombres (pas caster).
		{
			NkDrawCall3D dc;
			dc.mesh = meshPlane;
			dc.transform = NkMat4f::Scale({12.f, 1.f, 12.f});
			dc.aabb = {{-12.f, 0.f, -12.f}, {12.f, 0.f, 12.f}};
			dc.castShadow = false;
			dc.tint = {0.14f, 0.14f, 0.16f};
			dc.metallic = 0.f;
			dc.roughness = 0.9f;
			r3d->Submit(dc);
		}

		// Cube doré en rotation, posé au centre.
		{
			NkDrawCall3D dc;
			dc.mesh = meshCube;
			dc.transform = NkMat4f::Translate({0.f, 0.5f, 0.f}) *
						   NkMat4f::RotationY(NkAngle::FromRad(total * 0.8f)) *
						   NkMat4f::Scale({0.8f, 0.8f, 0.8f});
			dc.aabb = {{-0.6f, -0.1f, -0.6f}, {0.6f, 1.1f, 0.6f}};
			dc.tint = {1.f, 0.8f, 0.3f}; // or
			dc.metallic = 1.f;
			dc.roughness = 0.2f;
			r3d->Submit(dc);
		}

		// Sphère, à droite du cube.
		{
			NkDrawCall3D dc;
			dc.mesh = meshSphere;
			dc.transform = NkMat4f::Translate({1.8f, 0.5f, 0.f}) * NkMat4f::Scale({0.5f, 0.5f, 0.5f});
			dc.aabb = {{1.3f, 0.f, -0.5f}, {2.3f, 1.f, 0.5f}};
			dc.tint = {0.9f, 0.9f, 0.95f};
			dc.metallic = 0.f;
			dc.roughness = 1.0f;
			r3d->Submit(dc);
		}

		// ── Panneau de texte (overlay 2D) ────────────────────────────────────
		if (overlay) {
			// Safe area : sur mobile (encoche, barres système) on décale le
			// panneau pour rester dans la zone visible ; ailleurs insets = 0.
			const NkSafeAreaInsets sa = window.GetSafeAreaInsets();
			const float32 ox = 10.f + sa.left, oy = 10.f + sa.top;
			overlay->BeginOverlay(renderer->GetCmd(), W, H);
			if (r2d)
				r2d->FillRect({ox, oy, 320.f, 90.f}, {0.f, 0.f, 0.f, 0.6f});
			overlay->DrawText({ox + 10.f, oy + 20.f}, "== Tuto 03 : cube + sphere + sol ==");
			overlay->DrawText({ox + 10.f, oy + 40.f}, "Backend : %s", NkGraphicsApiName(device->GetApi()));
			overlay->DrawText({ox + 10.f, oy + 60.f}, "FPS ~ %.1f  |  dt %.2f ms", dt > 1e-4f ? 1.f / dt : 0.f,
							  dt * 1000.f);
			overlay->DrawText({ox + 10.f, oy + 80.f}, "Echap = quitter");
			overlay->EndOverlay();
		}

		renderer->Present();
		renderer->EndFrame();
	}

	// ── 6) Fermeture propre (ordre inverse de la création) ────────────────────
	device->WaitIdle();
	NkRenderer::Destroy(renderer);
	NkDeviceFactory::Destroy(device);
	window.Close();
	logger.Info("[Tuto03] Termine proprement.");
	return 0;
}
