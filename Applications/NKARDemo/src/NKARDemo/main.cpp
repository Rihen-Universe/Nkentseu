// =============================================================================
// NKARDemo — Étage 3 : la réalité augmentée, de la caméra à l'objet ancré.
//
// Ce que cette démo prouve (et rien de plus) :
//   - la caméra du poste alimente la chaîne AR de NKXR (NKCamera → NkArSession) ;
//   - un marqueur imprimé est détecté, identifié et SUIVI dans l'image ;
//   - un objet 3D est ancré sur lui : il tient sa place quand la feuille bouge,
//     et il est rendu par NKRenderer par-dessus la vidéo plein écran.
//
// TOUT SE RÈGLE PAR LE CODE (principe moteur n°4, docs/IDEES_ARCHITECTURE.md) :
// la structure NkArSessionConfig porte les réglages, et l'environnement n'est
// qu'une couche facultative appliquée explicitement plus bas.
//
// Sans caméra branchée, la démo ne ment pas : elle le DIT et bascule sur une
// image de synthèse animée — on peut donc la lancer partout, et la chaîne
// reste exercée de bout en bout.
//
// Imprimer le marqueur : la démo écrit « nkar_marqueur.png » au démarrage.
// L'imprimer sur A4, mesurer le côté du carré noir, et le donner dans
// NkArSessionConfig::markerSizeMeters — c'est lui qui donne l'ÉCHELLE.
// =============================================================================
#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKTime/NkTime.h"
#include "NKLogger/NkLog.h"
#include "NKImage/NkImage.h"

#include "NKCamera/NkCameraSystem.h"

#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKRenderer/Tools/Overlay/NkOverlayRenderer.h"

#include "NKXR/AR/NkArSession.h"

#include <cstdlib>
#include <cstdio>

#ifdef DrawText
#undef DrawText
#endif

using namespace nkentseu;
using namespace nkentseu::renderer;
namespace nkxr = nkentseu::xr;

static void ConfigureAppData(NkAppData &d) {
	d.appName = "NKARDemo";
}
NK_REGISTER_ENTRY_APPDATA_UPDATER(ConfigureAppData)

namespace {

	constexpr uint32 kCamWidth = 1280;
	constexpr uint32 kCamHeight = 720;
	constexpr int32 kMarkerId = 0x2D;

	// Image de repli quand aucune caméra n'est branchée : le marqueur y
	// dérive lentement, ce qui exerce le SUIVI (et pas seulement la détection).
	void SynthesizeFallback(uint8 *rgba, uint32 w, uint32 h, const uint8 *pattern, uint32 patternSize,
							float32 time) {
		for (uint32 i = 0; i < w * h; ++i) {
			rgba[i * 4u + 0u] = 190;
			rgba[i * 4u + 1u] = 195;
			rgba[i * 4u + 2u] = 205;
			rgba[i * 4u + 3u] = 255;
		}
		const float32 size = 260.f + 60.f * math::NkSin(time * 0.7f);
		const float32 cx = float32(w) * 0.5f + 180.f * math::NkSin(time * 0.5f);
		const float32 cy = float32(h) * 0.5f + 90.f * math::NkCos(time * 0.35f);
		const int32 x0 = int32(cx - size * 0.5f);
		const int32 y0 = int32(cy - size * 0.5f);
		const int32 side = int32(size);
		for (int32 y = 0; y < side; ++y) {
			for (int32 x = 0; x < side; ++x) {
				const int32 px = x0 + x;
				const int32 py = y0 + y;
				if (px < 0 || py < 0 || uint32(px) >= w || uint32(py) >= h) {
					continue;
				}
				const uint32 tu = uint32((float32(x) / float32(side)) * float32(patternSize - 1u));
				const uint32 tv = uint32((float32(y) / float32(side)) * float32(patternSize - 1u));
				const uint8 value = pattern[tv * patternSize + tu];
				uint8 *dst = &rgba[(uint32(py) * w + uint32(px)) * 4u];
				dst[0] = value;
				dst[1] = value;
				dst[2] = value;
				dst[3] = 255;
			}
		}
	}

} // namespace

int nkmain(const NkEntryState &state) {
	(void)state;

	// ── 1) Le marqueur à imprimer ─────────────────────────────────────────────
	{
		const uint32 size = 512;
		NkImage marker;
		if (marker.Create(size, size, math::NkColor(0, 0, 0, 255), 4)) {
			uint8 *gray = static_cast<uint8 *>(memory::NkAlloc(size * size));
			nkxr::NkArRenderMarker(kMarkerId, 4, gray, size);
			uint8 *pixels = marker.Pixels();
			for (uint32 i = 0; i < size * size; ++i) {
				pixels[i * 4u + 0u] = gray[i];
				pixels[i * 4u + 1u] = gray[i];
				pixels[i * 4u + 2u] = gray[i];
				pixels[i * 4u + 3u] = 255;
			}
			memory::NkFree(gray);
			marker.SaveToFile("nkar_marqueur.png");
			logger.Infof("[NKARDemo] Marqueur à imprimer écrit : nkar_marqueur.png (identifiant %d).\n", kMarkerId);
		}
	}

	// ── 2) Fenêtre + device + renderer ────────────────────────────────────────
	NkWindowConfig winCfg;
	winCfg.title = "NKARDemo — réalité augmentée à marqueurs (Échap = quitter)";
	winCfg.width = 1280;
	winCfg.height = 720;
	winCfg.centered = true;
	NkWindow window(winCfg);
	if (!window.IsValid()) {
		logger.Error("[NKARDemo] Création fenêtre KO");
		return 1;
	}

	NkDeviceInitInfo devInfo{};
	devInfo.surface = window.GetSurfaceDesc();
	devInfo.width = (uint32)window.GetSize().width;
	devInfo.height = (uint32)window.GetSize().height;
	NkIDevice *device = NkDeviceFactory::CreateAutoDetect(devInfo);
	if (!device || !device->IsValid()) {
		logger.Error("[NKARDemo] Création device KO");
		window.Close();
		return 2;
	}
	uint32 W = devInfo.width;
	uint32 H = devInfo.height;

	// Un seul renderer : la vidéo est un fond 2D, la 3D se pose dessus.
	NkRendererConfig cfg = NkRendererConfig::ForGame(devInfo.api, W, H);
	cfg.postProcess.bloom = false;
	cfg.postProcess.ssao = false;
	NkRenderer *renderer = NkRenderer::Create(device, cfg);
	if (!renderer) {
		logger.Error("[NKARDemo] Init renderer KO");
		NkDeviceFactory::Destroy(device);
		window.Close();
		return 3;
	}

	// ── 3) Caméra ─────────────────────────────────────────────────────────────
	NkCameraSystem &camera = NkCameraSystem::Instance();
	bool cameraOk = false;
	NkCameraConfig camCfg;
	camCfg.width = kCamWidth;
	camCfg.height = kCamHeight;
	camCfg.fps = 30;
	camCfg.outputFormat = NkPixelFormat::NK_PIXEL_RGBA8;
	if (camera.Init()) {
		const auto devices = camera.EnumerateDevices();
		if (devices.Size() > 0) {
			cameraOk = camera.StartStreaming(camCfg);
			logger.Infof("[NKARDemo] Caméra : %u périphérique(s), flux %s.\n", uint32(devices.Size()),
						 cameraOk ? "démarré" : "REFUSÉ");
		}
	}
	if (!cameraOk) {
		// Le dire franchement plutôt que d'afficher un écran noir : la chaîne
		// AR reste exercée sur une image de synthèse animée.
		logger.Warn("[NKARDemo] Aucune caméra exploitable — repli sur une image de SYNTHÈSE animée. "
					"La chaîne AR complète reste exercée.");
	}

	// ── 4) Session AR — TOUT PAR LE CODE ──────────────────────────────────────
	nkxr::NkArSessionConfig arCfg;
	arCfg.markerSizeMeters = 0.10f;      // côté du carré noir imprimé
	arCfg.fallbackFovXDegrees = 60.f;    // webcam typique, non calibrée
	arCfg.lostToleranceFrames = 8;
	arCfg.smoothing = 0.35f;
	arCfg.detector.minEdgePixels = 40;
	// L'environnement n'est qu'un raccourci de développement, explicite :
	{
		const char *sizeEnv = getenv("NK_AR_MARKER_SIZE");
		if (sizeEnv != nullptr && *sizeEnv != '\0') {
			arCfg.markerSizeMeters = float32(atof(sizeEnv));
		}
		arCfg.detector.debugCounters = (getenv("NK_AR_DEBUG") != nullptr);
		// Seuillage adaptatif : à essayer quand le marqueur est affiché sur un
		// ÉCRAN (halos) ou éclairé de biais — c'est exactement son terrain.
		arCfg.detector.adaptive = (getenv("NK_AR_ADAPTIVE") != nullptr);
	}
	uint32 arWidth = kCamWidth;
	uint32 arHeight = kCamHeight;
	bool loggedFormat = false;
	nkxr::NkArSession arSession;
	if (!arSession.Initialize(arCfg, arWidth, arHeight)) {
		logger.Error("[NKARDemo] Init session AR KO");
		return 4;
	}
	// Dire l'état EFFECTIF des réglages : une variable d'environnement survit à
	// toute une session de terminal, et un réglage hérité d'un essai précédent
	// explique bien des « ça ne marche plus » — vécu ici même, l'adaptatif
	// resté armé rendait le masque bruité et le code illisible.
	logger.Infof("[NKARDemo] Marqueur attendu : %.1f cm de côté | seuillage : %s.\n",
				 arCfg.markerSizeMeters * 100.f, arCfg.detector.adaptive ? "ADAPTATIF" : "Otsu global");

	// Texture vidéo : mise à jour à chaque image (pas recréée — recréer une
	// texture par frame ferait tousser l'allocateur GPU en quelques secondes).
	NkTextureCreateDesc texDesc;
	texDesc.width = kCamWidth;
	texDesc.height = kCamHeight;
	texDesc.format = NkGPUFormat::NK_RGBA8_UNORM;
	texDesc.debugName = "NKARDemo_Video";
	NkTexHandle videoTex = renderer->GetTextures()->Create(texDesc);

	// Tampon de travail : les octets que l'on donne à la session AR.
	uint8 *frameRGBA = static_cast<uint8 *>(memory::NkAlloc(nk_size(kCamWidth) * kCamHeight * 4u));
	uint8 *pattern = static_cast<uint8 *>(memory::NkAlloc(256u * 256u));
	nkxr::NkArRenderMarker(kMarkerId, 4, pattern, 256);

	// ── 5) Boucle ─────────────────────────────────────────────────────────────
	bool running = true;
	NkEventSystem &events = NkEvents();
	events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) { running = false; });
	events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent *e) {
		if (e->GetKey() == NkKey::NK_ESCAPE) {
			running = false;
		}
	});
	events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent *e) {
		const uint32 w = (uint32)e->GetWidth(), h = (uint32)e->GetHeight();
		if (w >= 64 && h >= 64 && (w != W || h != H)) {
			W = w;
			H = h;
			renderer->OnResize(w, h);
		}
	});

	NkClock clock;
	float32 total = 0.f;
	uint64 frameIndex = 0;
	const uint64 exitFrame = (getenv("NK_AR_EXIT") != nullptr) ? uint64(atoll(getenv("NK_AR_EXIT"))) : 0u;
	const uint64 dumpFrame = (getenv("NK_AR_DUMP") != nullptr) ? uint64(atoll(getenv("NK_AR_DUMP"))) : 0u;

	while (running && window.IsOpen()) {
		events.PollEvents();
		if (!running) {
			break;
		}
		total += clock.Tick().delta;
		++frameIndex;

		// ── Image ────────────────────────────────────────────────────────────
		bool haveFrame = false;
		if (cameraOk) {
			NkCameraFrame frame;
			if (camera.GetLastFrame(frame) && frame.IsValid()) {
				// La caméra livre CE QU'ELLE VEUT (YUYV, NV12, MJPEG…) même
				// quand on demande du RGBA : le pilote a le dernier mot. Lire
				// ses octets comme du RGBA donnait une image répétée en
				// mosaïque — un format de 2 octets/pixel lu comme 4 se replie
				// sur lui-même. On CONVERTIT donc, et on le dit une fois.
				if (frame.format != NkPixelFormat::NK_PIXEL_RGBA8) {
					if (!loggedFormat) {
						logger.Infof("[NKARDemo] La caméra livre le format %d — conversion en RGBA8.\n",
									 int(frame.format));
						loggedFormat = true;
					}
					NkCameraSystem::ConvertToRGBA8(frame);
				}
				// La résolution obtenue peut différer de celle demandée : c'est
				// la CAMÉRA qui décide, on s'adapte au lieu de refuser.
				if (frame.format == NkPixelFormat::NK_PIXEL_RGBA8 && frame.width > 0u && frame.height > 0u) {
					if (frame.width != arWidth || frame.height != arHeight) {
						logger.Infof("[NKARDemo] Résolution caméra réelle : %ux%u (demandé %ux%u) — session AR "
									 "réinitialisée à cette taille.\n",
									 frame.width, frame.height, arWidth, arHeight);
						arWidth = frame.width;
						arHeight = frame.height;
						memory::NkFree(frameRGBA);
						frameRGBA = static_cast<uint8 *>(memory::NkAlloc(nk_size(arWidth) * arHeight * 4u));
						arSession.Shutdown();
						arSession.Initialize(arCfg, arWidth, arHeight);
						NkTextureCreateDesc redesc;
						redesc.width = arWidth;
						redesc.height = arHeight;
						redesc.format = NkGPUFormat::NK_RGBA8_UNORM;
						redesc.debugName = "NKARDemo_Video";
						videoTex = renderer->GetTextures()->Create(redesc);
					}
					const uint32 stride = frame.stride ? frame.stride : frame.width * 4u;
					for (uint32 y = 0; y < frame.height; ++y) {
						const uint8 *src = frame.data.Data() + nk_size(y) * stride;
						uint8 *dst = frameRGBA + nk_size(y) * frame.width * 4u;
						for (uint32 x = 0; x < frame.width * 4u; ++x) {
							dst[x] = src[x];
						}
					}
					haveFrame = true;
				}
			}
		}
		if (!haveFrame) {
			SynthesizeFallback(frameRGBA, arWidth, arHeight, pattern, 256, total);
		}

		// ── AR : l'image entre, les poses sortent ────────────────────────────
		const uint32 visible = arSession.ProcessFrame(frameRGBA, arWidth, arHeight, arWidth * 4u,
													  nkxr::NkArImageFormat::NK_AR_RGBA8);
		renderer->GetTextures()->Update(videoTex, frameRGBA, arWidth * 4u);

		if (!renderer->BeginFrame()) {
			continue;
		}

		// ── Caméra virtuelle = la VRAIE caméra ───────────────────────────────
		// Les poses des marqueurs sont dans le repère caméra : la caméra
		// virtuelle est donc à l'origine, regardant -Z, avec le MÊME champ que
		// l'objectif — sinon l'objet ne se superposerait pas à l'image.
		const nkxr::NkArCameraIntrinsics &K = arSession.GetIntrinsics();
		NkCamera3DData camData;
		camData.position = { 0.f, 0.f, 0.f };
		camData.target = { 0.f, 0.f, -1.f };
		camData.up = { 0.f, 1.f, 0.f };
		camData.nearPlane = 0.01f;
		camData.farPlane = 50.f;
		camData.useFovAsym = true;
		// Le frustum décentré (étage 1) sert ici sa VRAIE raison d'être : le
		// centre optique d'un objectif n'est jamais exactement au milieu.
		camData.fovLeft = -math::NkAtan(K.cx / K.fx);
		camData.fovRight = math::NkAtan((float32(arWidth) - K.cx) / K.fx);
		camData.fovUp = math::NkAtan(K.cy / K.fy);
		camData.fovDown = -math::NkAtan((float32(arHeight) - K.cy) / K.fy);

		NkSceneContext sctx;
		sctx.camera = NkCamera3D(camData);
		sctx.time = total;
		sctx.ambientIntensity = 0.45f;
		NkLightDesc key;
		key.type = NkLightType::NK_DIRECTIONAL;
		key.direction = { -0.3f, -0.7f, -0.6f };
		key.color = { 1.f, 0.97f, 0.92f };
		key.intensity = 2.5f;
		key.castShadow = false; // pas d'ombre : le sol réel n'est pas modélisé
		sctx.lights.PushBack(key);

		NkRender3D *r3d = renderer->GetRender3D();
		r3d->BeginScene(sctx);

		const auto &tracked = arSession.GetTracked();
		for (nk_size i = 0; i < tracked.Size(); ++i) {
			const nkxr::NkArTrackedMarker &marker = tracked[i];
			// Un cube POSÉ sur le marqueur : demi-hauteur au-dessus du plan,
			// car le marqueur est le SOL de l'objet, pas son centre.
			const float32 side = arCfg.markerSizeMeters;
			const NkMat4f anchor = NkMat4f::Translate(marker.pose.position) * marker.pose.orientation.ToMat4();
			NkDrawCall3D dc;
			dc.mesh = renderer->GetMeshSystem()->GetCube();
			dc.transform = anchor * NkMat4f::Translate({ 0.f, 0.f, side * 0.5f }) *
						   NkMat4f::RotationZ(NkAngle::FromRad(total * 0.9f)) *
						   NkMat4f::Scale({ side, side, side });
			dc.aabb = { { marker.pose.position.x - side, marker.pose.position.y - side,
						  marker.pose.position.z - side },
						{ marker.pose.position.x + side, marker.pose.position.y + side,
						  marker.pose.position.z + side } };
			// Marqueur perdu mais encore suivi : teinte froide — l'utilisateur
			// voit que l'objet est en sursis, il ne disparaît pas d'un coup.
			dc.tint = marker.visibleThisFrame ? NkVec3f{ 1.f, 0.75f, 0.25f } : NkVec3f{ 0.35f, 0.55f, 0.9f };
			dc.metallic = 0.1f;
			dc.roughness = 0.45f;
			dc.castShadow = false;
			r3d->Submit(dc);
		}

		// ── Vidéo en fond + HUD ──────────────────────────────────────────────
		NkOverlayRenderer *overlay = renderer->GetOverlay();
		NkRender2D *r2d = renderer->GetRender2D();
		if (overlay && r2d) {
			overlay->BeginOverlay(renderer->GetCmd(), W, H);
			// Proportions de l'image respectées : une vidéo étirée fausserait
			// la lecture de ce que voit la caméra.
			const float32 srcAspect = float32(arWidth) / float32(arHeight);
			NkRectF dst{ 0.f, 0.f, float32(W), float32(H) };
			if (srcAspect > float32(W) / float32(H)) {
				dst.height = float32(W) / srcAspect;
				dst.y = (float32(H) - dst.height) * 0.5f;
			}
			else {
				dst.width = float32(H) * srcAspect;
				dst.x = (float32(W) - dst.width) * 0.5f;
			}
			r2d->DrawImage(videoTex, dst);
			overlay->DrawText({ 12.f, 20.f }, "NKARDemo — %s | marqueurs vus : %u / suivis : %u",
							  cameraOk ? "camera" : "SYNTHESE (pas de camera)", visible, uint32(tracked.Size()));
			overlay->DrawText({ 12.f, 40.f }, "Imprimer nkar_marqueur.png (cote %.1f cm) et le montrer a la camera",
							  arCfg.markerSizeMeters * 100.f);
			for (nk_size i = 0; i < tracked.Size(); ++i) {
				overlay->DrawText({ 12.f, 60.f + 20.f * float32(i) },
								  "  id %d : %.2f m devant%s", tracked[i].id, -tracked[i].pose.position.z,
								  tracked[i].visibleThisFrame ? "" : " (perdu, en sursis)");
			}
			overlay->EndOverlay();
		}

		renderer->Present();
		renderer->EndFrame();

		// ── Vidage de diagnostic : ce que le detecteur VOIT ─────────────────
		// Une image du gris et une du masque seuille : si le marqueur est
		// lisible sur la premiere mais absent de la seconde, le probleme est
		// le SEUILLAGE ; s'il manque aux deux, c'est l'optique ou le cadrage.
		if (dumpFrame != 0 && frameIndex == dumpFrame) {
			NkImage img;
			if (img.Create(arWidth, arHeight, math::NkColor(0, 0, 0, 255), 4)) {
				uint8 *px = img.Pixels();
				const uint8 *gray = arSession.GetGray();
				if (gray != nullptr) {
					for (uint32 i = 0; i < arWidth * arHeight; ++i) {
						px[i * 4u + 0u] = gray[i];
						px[i * 4u + 1u] = gray[i];
						px[i * 4u + 2u] = gray[i];
						px[i * 4u + 3u] = 255;
					}
					img.SaveToFile("nkar_diag_gris.png");
				}
				const uint8 *mask = arSession.GetMask();
				if (mask != nullptr) {
					for (uint32 i = 0; i < arWidth * arHeight; ++i) {
						px[i * 4u + 0u] = mask[i];
						px[i * 4u + 1u] = mask[i];
						px[i * 4u + 2u] = mask[i];
						px[i * 4u + 3u] = 255;
					}
					img.SaveToFile("nkar_diag_masque.png");
				}
				logger.Infof("[NKARDemo] Diagnostic ecrit : nkar_diag_gris.png et nkar_diag_masque.png.\n");
			}
		}

		if (exitFrame != 0 && frameIndex >= exitFrame) {
			running = false;
		}
	}

	// ── 6) Fermeture ──────────────────────────────────────────────────────────
	device->WaitIdle();
	memory::NkFree(frameRGBA);
	memory::NkFree(pattern);
	arSession.Shutdown();
	if (cameraOk) {
		camera.StopStreaming();
	}
	camera.Shutdown();
	NkRenderer::Destroy(renderer);
	NkDeviceFactory::Destroy(device);
	window.Close();
	logger.Info("[NKARDemo] Terminé proprement.");
	return 0;
}
