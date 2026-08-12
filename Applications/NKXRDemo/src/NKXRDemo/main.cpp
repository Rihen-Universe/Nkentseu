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
#include "NKRHI/Vulkan/NkVulkanDevice.h" // liaison OpenXR : handles Vulkan natifs
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
#include <cstdio>  // snprintf : chemins des captures d'agent, comme le modeleur
#include <cstring> // strcmp : sélection de backend NK_XR_BACKEND

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

	// Caméra NKRenderer depuis une vue XR (pose + FOV, symétrique ou non).
	// viewerOffset = position du SPECTATEUR dans le monde (locomotion stick) :
	// ajoutée aux poses trackées, jamais écrite dedans.
	NkCamera3D CameraFromXrView(const nkxr::NkXrView &view, const math::NkVec3f &viewerOffset) {
		NkCamera3DData cd;
		cd.position = view.position + viewerOffset;
		cd.target = cd.position + nkxr::NkXrForward(view.orientation);
		cd.up = nkxr::NkXrUp(view.orientation);
		cd.fovY = (view.fov.angleUp - view.fov.angleDown) * 180.f / math::NK_PI_F;
		const float32 tanW = math::NkTan(view.fov.angleRight) - math::NkTan(view.fov.angleLeft);
		const float32 tanH = math::NkTan(view.fov.angleUp) - math::NkTan(view.fov.angleDown);
		cd.aspect = (tanH > 1e-6f) ? (tanW / tanH) : 1.f;
		cd.nearPlane = 0.05f;
		cd.farPlane = 200.f;
		// Étage 1 : le FOV asymétrique du runtime est consommé TEL QUEL
		// (note de coordination dans la ROADMAP NKRenderer). fovY/aspect
		// restent remplis avec l'équivalent symétrique englobant pour les
		// consommateurs qui ne connaissent que l'ancien contrat.
		cd.useFovAsym = true;
		cd.fovLeft = view.fov.angleLeft;
		cd.fovRight = view.fov.angleRight;
		cd.fovUp = view.fov.angleUp;
		cd.fovDown = view.fov.angleDown;
		return NkCamera3D(cd);
	}

	uint64 EnvU64(const char *name, uint64 fallback) {
		const char *text = getenv(name);
		if (text == nullptr || *text == '\0') {
			return fallback;
		}
		return uint64(atoll(text));
	}

	float32 EnvF32(const char *name, float32 fallback) {
		const char *text = getenv(name);
		if (text == nullptr || *text == '\0') {
			return fallback;
		}
		return float32(atof(text));
	}

	// Découpe EN PLACE une liste d'extensions « séparées par espaces » (le
	// format des fonctions xrGetVulkan*ExtensionsKHR) : les espaces deviennent
	// des fins de chaîne, les pointeurs restent dans le buffer d'origine.
	uint32 SplitExtensionList(char *list, const char **out, uint32 cap) {
		uint32 count = 0;
		char *cursor = list;
		while (*cursor != '\0' && count < cap) {
			while (*cursor == ' ') {
				++cursor;
			}
			if (*cursor == '\0') {
				break;
			}
			out[count] = cursor;
			++count;
			while (*cursor != '\0' && *cursor != ' ') {
				++cursor;
			}
			if (*cursor == ' ') {
				*cursor = '\0';
				++cursor;
			}
		}
		return count;
	}

	// Crochet NkVulkanDesc.pickPhysicalDevice : le runtime OpenXR répond,
	// NKRHI obéit. user = la session XR.
	void *NkXrPickPhysicalDevice(void *vkInstance, void *user) {
		return static_cast<nkxr::NkXrSession *>(user)->GetVulkanPhysicalDevice(vkInstance);
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

	// ── 2) Session XR D'ABORD, device ENSUITE ─────────────────────────────────
	// L'ordre est dicté par OpenXR : le runtime impose au device Vulkan ses
	// extensions et son physical device AVANT sa création. Le simulateur, lui,
	// n'exige rien — le même code marche pour les deux.
	nkxr::NkXrSessionDesc xrDesc;
	xrDesc.window = &window;
	const char *backendEnv = getenv("NK_XR_BACKEND");
	const bool wantOpenXR = (backendEnv != nullptr) && (strcmp(backendEnv, "openxr") == 0);
	if (wantOpenXR) {
		xrDesc.backend = nkxr::NkXrBackendType::NK_XR_BACKEND_OPENXR;
	}
	nkxr::NkXrSession *xrSession = nkxr::NkXrSession::Create(xrDesc);

	NkDeviceInitInfo devInfo{};
	devInfo.surface = window.GetSurfaceDesc();
	devInfo.width = (uint32)window.GetSize().width;
	devInfo.height = (uint32)window.GetSize().height;

	NkIDevice *device = nullptr;
	bool xrBound = false;
	nkxr::NkXrVulkanRequirements xrVkReqs; // doit survivre à la création du device
	const char *xrInstExt[32];
	const char *xrDevExt[32];
	if (xrSession != nullptr && wantOpenXR) {
		if (xrSession->GetVulkanRequirements(xrVkReqs)) {
			// Les listes OpenXR sont « séparées par espaces » : découpe en
			// place, les pointeurs restent dans xrVkReqs.
			devInfo.api = NkGraphicsApi::NK_GFX_API_VULKAN;
			devInfo.context.vulkan.extraInstanceExt = xrInstExt;
			devInfo.context.vulkan.extraInstanceExtCount =
				SplitExtensionList(xrVkReqs.instanceExtensions, xrInstExt, 32);
			devInfo.context.vulkan.extraDeviceExt = xrDevExt;
			devInfo.context.vulkan.extraDeviceExtCount = SplitExtensionList(xrVkReqs.deviceExtensions, xrDevExt, 32);
			devInfo.context.vulkan.pickPhysicalDevice = &NkXrPickPhysicalDevice;
			devInfo.context.vulkan.pickPhysicalDeviceUser = xrSession;
			device = NkDeviceFactory::CreateForApi(NkGraphicsApi::NK_GFX_API_VULKAN, devInfo);
			if (device != nullptr && device->IsValid()) {
				auto *vkDevice = static_cast<NkVulkanDevice *>(device);
				nkxr::NkXrVulkanBinding binding;
				binding.instance = vkDevice->GetVkInstance();
				binding.physicalDevice = vkDevice->GetNativePhysicalDevice();
				binding.device = vkDevice->GetVkDevice();
				binding.queueFamilyIndex = vkDevice->GetGraphicsQueueFamilyIndex();
				binding.queueIndex = 0;
				xrBound = xrSession->BindVulkan(binding);
			}
			if (!xrBound) {
				logger.Warn("[NKXRDemo] Liaison Vulkan/OpenXR échouée — repli sur le SIMULATEUR.");
			}
		}
		else {
			logger.Warn("[NKXRDemo] Runtime sans XR_KHR_vulkan_enable — sonde seule, repli SIMULATEUR.");
		}
		if (!xrBound) {
			// La sonde a pu réussir (2a) mais pas la liaison : l'identité du
			// casque est déjà au journal ; on repart proprement en simulé.
			nkxr::NkXrSession::Destroy(xrSession);
		}
	}
	if (device == nullptr || !device->IsValid()) {
		// Chemin simulateur (ou échec Vulkan) : device au choix de la machine,
		// sans les crochets XR.
		devInfo.api = NkGraphicsApi::NK_GFX_API_NONE;
		devInfo.context.vulkan.extraInstanceExtCount = 0;
		devInfo.context.vulkan.extraDeviceExtCount = 0;
		devInfo.context.vulkan.pickPhysicalDevice = nullptr;
		devInfo.context.vulkan.pickPhysicalDeviceUser = nullptr;
		if (device != nullptr) {
			NkDeviceFactory::Destroy(device);
		}
		device = NkDeviceFactory::CreateAutoDetect(devInfo);
	}
	if (!device || !device->IsValid()) {
		logger.Error("[NKXRDemo] Création device KO");
		if (xrSession != nullptr) {
			nkxr::NkXrSession::Destroy(xrSession);
		}
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
	// Casque lié : c'est xrWaitFrame qui cadence (72 Hz Quest 2) — la vsync
	// fenêtre à 60 Hz créerait un double-métronome et du judder dans le casque.
	if (xrBound) {
		cfgMain.vsync = false;
	}
	NkRenderer *rMain = NkRenderer::Create(device, cfgMain);
	if (!rMain) {
		logger.Error("[NKXRDemo] Init compositeur KO");
		NkDeviceFactory::Destroy(device);
		window.Close();
		return 3;
	}

	// ── 3) Repli simulateur si le casque n'a pas pris ─────────────────────────
	if (xrSession == nullptr) {
		if (wantOpenXR) {
			logger.Warn("[NKXRDemo] OpenXR indisponible — repli sur le SIMULATEUR.");
		}
		xrDesc.backend = nkxr::NkXrBackendType::NK_XR_BACKEND_SIMULATOR;
		xrSession = nkxr::NkXrSession::Create(xrDesc);
	}
	if (xrSession == nullptr) {
		logger.Error("[NKXRDemo] Création session XR KO");
		NkRenderer::Destroy(rMain);
		NkDeviceFactory::Destroy(device);
		window.Close();
		return 4;
	}
	const nkxr::NkXrSystemInfo xrInfo = xrSession->GetSystemInfo();
	// Casque lié : le rendu se fait à la taille des swapchains du runtime —
	// recommandé × NK_XR_RENDER_SCALE (0,7 par défaut : 2080x2096 ×2 yeux
	// ≈ 8,7 Mpx, la 3070 Laptop bridée ne tiendra pas le plein tarif à 72 Hz ;
	// on monte l'échelle quand la mesure le permet). Simulateur : demi-fenêtre.
	uint32 eyeW = W / 2;
	uint32 eyeH = H;
	if (xrBound) {
		// 0,9 : point d'équilibre MESURÉ sur Quest 2 + RTX 3070 Laptop bridée
		// (2026-08-12) — 1872x1886 par œil à 68-71 i/s pour 72 Hz visés. À 1,0
		// le GPU décroche (18 ms, 40 i/s), à 0,8 on perd des pixels pour rien.
		// La résolution est la première arme contre l'aliasing en casque.
		const float32 renderScale = math::NkClamp(EnvF32("NK_XR_RENDER_SCALE", 0.9f), 0.2f, 2.f);
		eyeW = uint32(float32(xrInfo.views[0].recommendedWidth) * renderScale);
		eyeH = uint32(float32(xrInfo.views[0].recommendedHeight) * renderScale);
		if (eyeW < 64u) {
			eyeW = 64u;
		}
		if (eyeH < 64u) {
			eyeH = 64u;
		}
		if (!xrSession->CreateHmdSwapchains(eyeW, eyeH)) {
			logger.Warn("[NKXRDemo] Swapchains casque KO : session réelle mais AUCUNE image ne partira au casque.");
		}
	}
	logger.Infof("[NKXRDemo] Système : %s (%s) — recommandé %ux%u par œil, rendu %ux%u.\n",
				 xrInfo.systemName, xrBound ? "SESSION CASQUE RÉELLE" : "simulateur",
				 xrInfo.views[0].recommendedWidth, xrInfo.views[0].recommendedHeight, eyeW, eyeH);

	if (xrBound) {
		// Cadence : la CHOISIR plutôt que la subir. NK_XR_HZ=0 laisse le
		// runtime décider ; sinon on demande la valeur proposée la plus proche
		// — demander un taux non proposé est refusé par la spec.
		float32 rates[16];
		const uint32 rateCount = xrSession->GetDisplayRefreshRates(rates, 16);
		if (rateCount > 0) {
			char rateList[256] = {};
			int written = 0;
			for (uint32 i = 0; i < rateCount && written < int(sizeof(rateList)) - 8; ++i) {
				written += snprintf(rateList + written, sizeof(rateList) - nk_size(written), "%.0f ", rates[i]);
			}
			logger.Infof("[NKXRDemo] Cadences proposées : %s(actuelle %.0f Hz)\n", rateList,
						 xrSession->GetDisplayRefreshRate());
			const float32 wanted = EnvF32("NK_XR_HZ", 0.f);
			if (wanted > 0.f) {
				float32 best = rates[0];
				for (uint32 i = 1; i < rateCount; ++i) {
					if (math::NkAbs(rates[i] - wanted) < math::NkAbs(best - wanted)) {
						best = rates[i];
					}
				}
				logger.Infof("[NKXRDemo] Cadence demandée %.0f Hz -> %s.\n", best,
							 xrSession->RequestDisplayRefreshRate(best) ? "accordée" : "REFUSÉE");
			}
		}
		// Masque de visibilité : mesurer ce qu'il y a à gagner. Sa
		// CONSOMMATION (prépasse de profondeur) vit dans NKRenderer — note de
		// coordination posée ; ici on prouve que la géométrie arrive.
		for (uint32 e = 0; e < nkxr::NK_XR_EYE_COUNT; ++e) {
			nkxr::NkXrVisibilityMask mask;
			if (xrSession->GetVisibilityMask(nkxr::NkXrEye(e), mask)) {
				logger.Infof("[NKXRDemo] Masque de visibilité œil %u : %u sommets, %u triangles cachés.\n", e,
							 uint32(mask.vertices.Size()), uint32(mask.indices.Size() / 3u));
			}
			else {
				logger.Infof("[NKXRDemo] Masque de visibilité œil %u : non fourni par ce runtime.\n", e);
			}
		}
	}

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
		if (xrBound) {
			// VR : le SSAO (effet écran) scintille en TACHES NOIRES sur les
			// modèles sous les micro-mouvements permanents de la tête —
			// constaté par Rihen dans le casque. Coupé par défaut en VR
			// (pratique standard), NK_XR_SSAO=1 pour le remettre ; le GPU
			// libéré sert à monter NK_XR_RENDER_SCALE, la vraie arme
			// anti-aliasing tant que le MSAA œil n'est pas branché.
			cfgEye.postProcess.ssao = (EnvU64("NK_XR_SSAO", 0) != 0);
			cfgEye.postProcess.fxaa = true;
			// Bloom OFF par défaut en VR : une chaîne Dual-Kawase 11 passes,
			// PAR ŒIL, pour un halo qu'un casque rend à peine — c'est le
			// meilleur rapport ips gagnés / qualité perdue vers le 72 Hz.
			cfgEye.postProcess.bloom = (EnvU64("NK_XR_BLOOM", 0) != 0);
			// Ombres : 1024 au lieu de 2048 (×2 yeux) — la carte d'ombre est
			// rendue une fois PAR ŒIL tant que le graphe n'est pas partagé.
			cfgEye.shadow.resolution = uint32(EnvU64("NK_XR_SHADOW_RES", 1024));
			cfgEye.shadow.cascadeCount = uint32(EnvU64("NK_XR_SHADOW_CASCADES", 2));
			// TAA en VR : le piège habituel est l'historique PARTAGÉ entre les
			// deux yeux (fantômes garantis). Ici il ne peut pas se produire —
			// chaque œil a SON renderer, donc son propre historique. Le vrai
			// risque restant est le trainage sous rotation de tête : c'est un
			// jugement d'œil, d'où l'interrupteur. NK_XR_TAA=1 pour essayer.
			// (Le MSAA, lui, n'existe pas dans NKRenderer — champ mort,
			// signalé dans sa ROADMAP ; c'est un chantier à coordonner.)
			cfgEye.postProcess.taa = (EnvU64("NK_XR_TAA", 0) != 0);
		}
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
	const nkxr::NkXrActionHandle actMove = actions.CreateAction(
		{ "deplacer", nkxr::NkXrActionType::NK_XR_ACTION_VEC2, nkxr::NkXrActionUsage::NK_XR_USAGE_MOVE });
	const nkxr::NkXrActionHandle actHaptic = actions.CreateAction(
		{ "vibrer", nkxr::NkXrActionType::NK_XR_ACTION_HAPTIC, nkxr::NkXrActionUsage::NK_XR_USAGE_HAPTIC });
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
	// Mesure de cadence casque : les « déchirures » à basse cadence viennent
	// de la reprojection du compositeur — il faut des CHIFFRES, pas des
	// impressions, pour distinguer un artefact GPU d'un manque d'images.
	NkChrono paceChrono;
	uint64 paceLastFrame = 0;
	// Verrou DEMI-CADENCE (36 i/s pour un Quest 72 Hz) : mesuré 39-51 i/s
	// INSTABLES — le compositeur bascule sans cesse entre reprojection et
	// ASW, et chaque bascule se voit (saccades, cisaillements). Un rythme
	// STABLE à 72/2 vaut mieux qu'un rythme oscillant plus haut : l'ASW se
	// cale et lisse. NK_XR_HALF_RATE=0 pour le couper quand le moteur saura
	// tenir 72 (chantier d'optimisation deux-vues).
	const bool xrHalfRate = (EnvU64("NK_XR_HALF_RATE", 1) != 0);
	NkChrono frameLimiter;
	// NK_XR_SHADOW=0 coupe les ombres de la scène : le plus gros poste de
	// coût GPU — l'interrupteur du test de cadence.
	const bool xrShadowOn = (EnvU64("NK_XR_SHADOW", 1) != 0);
	// Vue moniteur : NK_XR_SPECTATOR=1 remplit la fenêtre avec l'œil gauche
	// (ce que voit le porteur) au lieu du côte à côte. Défaut = spectateur dès
	// qu'un vrai casque est lié : la fenêtre PC n'est alors plus un outil de
	// mise au point mais l'écran que REGARDE quelqu'un d'autre.
	const bool spectatorView = (EnvU64("NK_XR_SPECTATOR", xrBound ? 1 : 0) != 0);
	// Locomotion au stick : un DÉCALAGE DE MONDE, jamais une écriture des
	// poses — le tracking reste la vérité, on déplace la scène sous lui.
	NkVec3f worldOffset(0.f, 0.f, 0.f);
	NkChrono locoChrono;

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
			// Ne jamais tourner à vide en attendant un événement de session :
			// un cœur CPU à 100 % pour rien, et la fenêtre paraît figée.
			NkChrono::SleepMilliseconds(5);
			continue;
		}

		if (pendingResize) {
			pendingResize = false;
			W = pendingW;
			H = pendingH;
			rMain->OnResize(W, H);
			if (xrBound) {
				// Les tailles d'œil appartiennent aux swapchains du CASQUE :
				// la fenêtre n'est qu'un miroir, DrawImage remet à l'échelle.
				continue;
			}
			eyeW = W / 2;
			eyeH = H;
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
		// Front montant de « sélectionner » → pulsation haptique : la preuve
		// tactile que la chaîne actions marche dans les deux sens.
		if (selectState.changed && selectState.current) {
			xrSession->ApplyHaptic(actHaptic, 0.7f, 0.06f);
		}

		// Les poses des yeux, PRÉDITES pour l'instant d'affichage.
		nkxr::NkXrView views[nkxr::NK_XR_EYE_COUNT];
		const bool haveViews = xrSession->LocateViews(stageSpace, frameState.predictedDisplayTime, views);

		// Locomotion au stick gauche (flèches sur le simulateur) : direction
		// du regard aplatie au sol — regarder en bas ne fait pas creuser.
		float32 locoDt = float32(locoChrono.Reset().seconds);
		if (locoDt < 0.f || locoDt > 0.25f) {
			locoDt = 1.f / 36.f;
		}
		nkxr::NkXrActionStateVec2 moveState;
		if (xrSession->GetActionStateVec2(actMove, moveState) && moveState.active && haveViews) {
			NkVec3f flatForward = nkxr::NkXrForward(views[0].orientation);
			flatForward.y = 0.f;
			const float32 forwardLen = flatForward.Len();
			if (forwardLen > 1e-4f) {
				flatForward = flatForward * (1.f / forwardLen);
				const NkVec3f flatRight(-flatForward.z, 0.f, flatForward.x);
				worldOffset = worldOffset +
							  (flatForward * moveState.current.y + flatRight * moveState.current.x) * (2.5f * locoDt);
			}
		}

		// Main droite (manette réelle ou main simulée) : localisée pour être
		// DESSINÉE dans la scène — la preuve visible du 6DoF des manettes.
		nkxr::NkXrPose aimPose;
		const bool haveHand = xrSession->LocateActionPose(actAim, stageSpace, frameState.predictedDisplayTime, aimPose);

		// VRAIES MAINS (sans manettes) : 26 articulations par main quand le
		// casque les voit. Les deux systèmes coexistent — poser les manettes
		// suffit à passer de l'un à l'autre, aucun mode à basculer.
		nkxr::NkXrHand hands[2];
		bool haveHandJoints[2] = { false, false };
		for (uint32 h = 0; h < 2u; ++h) {
			haveHandJoints[h] = xrSession->LocateHand(nkxr::NkXrHandSide(h), stageSpace,
													  frameState.predictedDisplayTime, hands[h]);
		}

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
				sctx.camera = CameraFromXrView(views[e], worldOffset);
				sctx.time = animTime;
				sctx.ambientIntensity = 0.15f;
				NkLightDesc sun;
				sun.type = NkLightType::NK_DIRECTIONAL;
				sun.direction = { -0.4f, -1.f, -0.3f };
				sun.color = { 1.f, 0.95f, 0.85f };
				sun.intensity = 3.f;
				sun.castShadow = xrShadowOn;
				sun.shadowStatic = false;
				sctx.lights.PushBack(sun);

				r3d->BeginScene(sctx);
				SubmitScene(r3d, r->GetMeshSystem(), animTime, selectState.current);
				if (haveHand) {
					// La main droite : un petit pavé allongé qui suit la
					// manette (ou la main simulée) — orientation comprise.
					NkDrawCall3D dc;
					dc.mesh = r->GetMeshSystem()->GetCube();
					const NkVec3f handPos = aimPose.position + worldOffset;
					dc.transform = NkMat4f::Translate(handPos) * aimPose.orientation.ToMat4() *
								   NkMat4f::Scale({ 0.05f, 0.05f, 0.16f });
					dc.aabb = { { handPos.x - 0.2f, handPos.y - 0.2f, handPos.z - 0.2f },
								{ handPos.x + 0.2f, handPos.y + 0.2f, handPos.z + 0.2f } };
					dc.tint = selectState.current ? NkVec3f{ 1.f, 0.3f, 0.2f } : NkVec3f{ 0.2f, 0.9f, 1.f };
					dc.metallic = 0.f;
					dc.roughness = 0.4f;
					dc.castShadow = false;
					r3d->Submit(dc);
				}
				// Vraies mains : une petite sphère par articulation, au rayon
				// que donne le runtime — un « squelette de perles » suffit à
				// prouver le suivi ; le maillage skinné viendra avec un
				// modèle de main (NK3DModeler).
				for (uint32 h = 0; h < 2u; ++h) {
					if (!haveHandJoints[h]) {
						continue;
					}
					for (uint32 j = 0; j < nkxr::NK_XR_HAND_JOINT_COUNT; ++j) {
						const nkxr::NkXrHandJoint &joint = hands[h].joints[j];
						if (!joint.valid) {
							continue;
						}
						const NkVec3f jointPos = joint.position + worldOffset;
						const float32 radius = (joint.radius > 0.f) ? joint.radius : 0.008f;
						NkDrawCall3D dc;
						dc.mesh = r->GetMeshSystem()->GetSphere();
						dc.transform = NkMat4f::Translate(jointPos) *
									   NkMat4f::Scale({ radius * 2.f, radius * 2.f, radius * 2.f });
						dc.aabb = { { jointPos.x - 0.05f, jointPos.y - 0.05f, jointPos.z - 0.05f },
									{ jointPos.x + 0.05f, jointPos.y + 0.05f, jointPos.z + 0.05f } };
						dc.tint = (h == 0) ? NkVec3f{ 0.3f, 1.f, 0.5f } : NkVec3f{ 1.f, 0.8f, 0.3f };
						dc.metallic = 0.f;
						dc.roughness = 0.5f;
						dc.castShadow = false;
						r3d->Submit(dc);
					}
				}
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
				if (spectatorView) {
					// Vue MONITEUR : un seul œil en plein écran — ce que voit
					// le porteur du casque, lisible par un formateur ou une
					// caméra. Coût nul : l'image est déjà rendue, on la
					// présente autrement. (Le côte à côte reste le mode de
					// mise au point, il montre la stéréo elle-même.)
					const float32 srcAspect = float32(eyeW) / float32(eyeH > 0u ? eyeH : 1u);
					const float32 dstAspect = float32(W) / float32(H > 0u ? H : 1u);
					NkRectF dst{ 0.f, 0.f, float32(W), float32(H) };
					// Respecter les proportions du casque : des barres plutôt
					// qu'un visage étiré — un formateur juge une posture.
					if (srcAspect > dstAspect) {
						dst.height = float32(W) / srcAspect;
						dst.y = (float32(H) - dst.height) * 0.5f;
					}
					else {
						dst.width = float32(H) * srcAspect;
						dst.x = (float32(W) - dst.width) * 0.5f;
					}
					r2d->DrawImage(eyeTargets[0]->GetColorResult(), dst);
				}
				else {
					r2d->DrawImage(eyeTargets[0]->GetColorResult(), { 0.f, 0.f, float32(W) * 0.5f, float32(H) });
					r2d->DrawImage(eyeTargets[1]->GetColorResult(),
								   { float32(W) * 0.5f, 0.f, float32(W) * 0.5f, float32(H) });
					// Séparateur central : sans lui, l'œil cherche la couture.
					r2d->FillRect({ float32(W) * 0.5f - 1.f, 0.f, 2.f, float32(H) }, { 0.f, 0.f, 0.f, 1.f });
				}
				overlay->DrawText({ 12.f, 20.f }, "NKXRDemo — étage 0 : stéréo SIMULÉE (NKXR Simulator)");
				overlay->DrawText({ 12.f, 40.f }, "souris = tête | ZQSD/WASD | Espace/C = monter/descendre | Maj = sprint");
				overlay->DrawText({ 12.f, 60.f }, "clic gauche = action « sélectionner » (le cube rougit) | Échap = quitter");
				overlay->EndOverlay();
			}

			rMain->Present();
			rMain->EndFrame();

			// Casque : remettre nos deux yeux au compositeur — copie vers les
			// images du runtime + couche de projection au EndFrame session.
			// APRÈS le Present : notre soumission GPU suit la sienne dans la
			// file, le compositeur attend la fin par Acquire/Wait.
			if (xrBound) {
				auto *vkDevice = static_cast<NkVulkanDevice *>(device);
				const uint64 imageLeft = uint64(uintptr_t(
					vkDevice->GetVkImage(rMain->GetTextures()->GetRHIHandle(eyeTargets[0]->GetColorResult()).id)));
				const uint64 imageRight = uint64(uintptr_t(
					vkDevice->GetVkImage(rMain->GetTextures()->GetRHIHandle(eyeTargets[1]->GetColorResult()).id)));
				// La PROFONDEUR part avec la couleur : le compositeur peut
				// alors reprojeter en translation (moins de sauts visibles).
				const uint64 depthLeft = uint64(uintptr_t(
					vkDevice->GetVkImage(rMain->GetTextures()->GetRHIHandle(eyeTargets[0]->GetDepthResult()).id)));
				const uint64 depthRight = uint64(uintptr_t(
					vkDevice->GetVkImage(rMain->GetTextures()->GetRHIHandle(eyeTargets[1]->GetDepthResult()).id)));
				xrSession->SubmitEyes(views, imageLeft, imageRight, eyeW, eyeH, depthLeft, depthRight, 0.05f, 200.f);
			}
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

		// Casque posé / app masquée : xrWaitFrame ne freine plus rien (mesuré
		// 2400 i/s à brûler un cœur pour zéro image). La boucle doit rester
		// synchronisée, mais elle peut respirer.
		if (!frameState.shouldRender) {
			NkChrono::SleepMilliseconds(4);
		}

		// Demi-cadence : compléter la frame jusqu'à 2 périodes d'affichage
		// (dormir le reliquat) — le compositeur reçoit un battement régulier.
		if (xrBound && xrHalfRate && frameState.predictedDisplayPeriod > 0) {
			const float64 targetSeconds = 2.0 * float64(frameState.predictedDisplayPeriod) * 1e-9;
			const float64 elapsedSeconds = frameLimiter.Elapsed().seconds;
			if (elapsedSeconds < targetSeconds) {
				NkChrono::SleepNanoseconds(int64((targetSeconds - elapsedSeconds) * 1e9));
			}
			frameLimiter.Reset();
		}

		// Cadence réelle toutes les ~144 frames (2 s à 72 Hz) quand le casque
		// est lié : c'est le chiffre qui explique (ou innocente) le compositeur.
		if (xrBound && frameIdx - paceLastFrame >= 144u) {
			const float64 seconds = paceChrono.Reset().seconds;
			const float64 fps = float64(frameIdx - paceLastFrame) / (seconds > 1e-6 ? seconds : 1.0);
			// Notre cadence ET celle mesurée par le COMPOSITEUR : c'est lui
			// qui sait où part le temps (et combien de frames il a dû
			// réafficher — la cause exacte des « sauts »).
			nkxr::NkXrPerfMetrics metrics;
			if (xrSession->GetPerfMetrics(metrics) && metrics.available) {
				logger.Infof("[NKXRDemo] Cadence %.1f i/s (%.1f ms) | app CPU %.2f GPU %.2f ms | compositeur CPU "
							 "%.2f GPU %.2f ms | frames rejouees %d | latence %.1f ms\n",
							 fps, 1000.0 / (fps > 1e-6 ? fps : 1.0), metrics.appCpuMs, metrics.appGpuMs,
							 metrics.compositorCpuMs, metrics.compositorGpuMs, metrics.staleFrames,
							 metrics.appMotionToPhotonMs);
			}
			else {
				logger.Infof("[NKXRDemo] Cadence casque : %.1f i/s (%.1f ms/frame).\n", fps,
							 1000.0 / (fps > 1e-6 ? fps : 1.0));
			}
			paceLastFrame = frameIdx;
		}

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
