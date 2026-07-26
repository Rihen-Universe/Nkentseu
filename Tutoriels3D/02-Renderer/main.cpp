// =============================================================================
// Tutoriels3D / Étape 2 — INITIALISER NKRENDERER
//
// On ajoute à l'étape 1 :
//   • un device GPU (NKRHI choisit tout seul le meilleur backend : Vulkan,
//     OpenGL, DX11, DX12 ou rendu logiciel),
//   • le renderer haut niveau NKRenderer,
//   • la boucle de rendu BeginFrame / Present / EndFrame,
//   • du texte à l'écran (overlay 2D) : backend utilisé + FPS.
// =============================================================================
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
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKRenderer/Tools/Overlay/NkOverlayRenderer.h"

// Win32 définit DrawText en macro (GDI) — collision avec NkOverlayRenderer::DrawText.
#ifdef DrawText
#undef DrawText
#endif

using namespace nkentseu;
using namespace nkentseu::renderer;

static void ConfigureAppData(NkAppData &d) {
	d.appName = "Tuto02Renderer";
}
NK_REGISTER_ENTRY_APPDATA_UPDATER(ConfigureAppData)

int nkmain(const NkEntryState &state) {
	(void)state;

	// ── 1) Fenêtre (étape 1) ──────────────────────────────────────────────────
	NkWindowConfig winCfg;
	winCfg.title = "Tuto 02 — NKRenderer initialise";
	winCfg.width = 1280;
	winCfg.height = 720;
	winCfg.centered = true;
	winCfg.resizable = true;

	NkWindow window(winCfg);
	if (!window.IsValid()) {
		logger.Error("[Tuto02] Creation fenetre KO");
		return 1;
	}

	// ── 2) Device GPU : 4 lignes suffisent ────────────────────────────────────
	NkDeviceInitInfo devInfo{};
	devInfo.surface = window.GetSurfaceDesc();
	devInfo.width = (uint32)window.GetSize().width;
	devInfo.height = (uint32)window.GetSize().height;

	NkIDevice *device = NkDeviceFactory::CreateAutoDetect(devInfo);
	if (!device || !device->IsValid()) {
		logger.Error("[Tuto02] Creation device KO");
		window.Close();
		return 2;
	}
	logger.Info("[Tuto02] Backend : {0}", NkGraphicsApiName(device->GetApi()));

	// ── 3) Le renderer haut niveau ────────────────────────────────────────────
	NkRendererConfig cfg = NkRendererConfig::ForGame(devInfo.api, devInfo.width, devInfo.height);
	NkRenderer *renderer = NkRenderer::Create(device, cfg);
	if (!renderer || !renderer->Initialize()) {
		logger.Error("[Tuto02] Init renderer KO");
		NkRenderer::Destroy(renderer);
		NkDeviceFactory::Destroy(device);
		window.Close();
		return 3;
	}
	NkRender2D *r2d = renderer->GetRender2D();
	NkOverlayRenderer *overlay = renderer->GetOverlay();

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
	events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent *e) {
		const uint32 w = (uint32)e->GetWidth(), h = (uint32)e->GetHeight();
		if (w > 0 && h > 0 && (w != W || h != H)) {
			W = w;
			H = h;
			renderer->OnResize(w, h);
		}
	});
	// Mobile : Android/HarmonyOS peuvent DETRUIRE puis RECREER la fenetre native
	// (retour d'arriere-plan, relayout plein ecran). NkWindowShownEvent est emis a
	// chaque (re)creation : on re-attache la surface de presentation du device a
	// la fenetre courante — sans ca, le rendu continue "avec succes" dans une
	// fenetre morte et l'ecran reste noir. No-op si la fenetre n'a pas change.
	events.AddEventCallback<NkWindowShownEvent>([&](NkWindowShownEvent *) {
		device->RecreateSurface(window.GetSurfaceDesc());
		const NkVec2u sz = window.GetSize();
		if (sz.width > 0 && sz.height > 0 && (sz.width != W || sz.height != H)) {
			W = sz.width;
			H = sz.height;
			renderer->OnResize(W, H);
		}
	});

	// ── 5) Boucle de rendu ────────────────────────────────────────────────────
	NkClock clock;
	while (running && window.IsOpen()) {
		events.PollEvents();
		if (!running)
			break;

		const float32 dt = clock.Tick().delta;

		if (!renderer->BeginFrame())
			continue; // fenêtre minimisée, etc.

		// Pour l'instant : juste un texte par-dessus l'écran vide.
		if (overlay) {
			// Safe area : sur mobile (encoche, barres système) on décale le
			// panneau pour rester dans la zone visible ; ailleurs insets = 0.
			const NkSafeAreaInsets sa = window.GetSafeAreaInsets();
			const float32 ox = 10.f + sa.left, oy = 10.f + sa.top;
			overlay->BeginOverlay(renderer->GetCmd(), W, H);
			if (r2d)
				r2d->FillRect({ox, oy, 330.f, 70.f}, {0.f, 0.f, 0.f, 0.6f});
			overlay->DrawText({ox + 10.f, oy + 20.f}, "== Tuto 02 : NKRenderer est initialise ==");
			overlay->DrawText({ox + 10.f, oy + 40.f}, "Backend : %s", NkGraphicsApiName(device->GetApi()));
			overlay->DrawText({ox + 10.f, oy + 60.f}, "FPS ~ %.1f", dt > 1e-4f ? 1.f / dt : 0.f);
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
	logger.Info("[Tuto02] Termine proprement.");
	return 0;
}
