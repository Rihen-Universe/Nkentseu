// =============================================================================
// DemoStream.cpp — Streaming REEL visible (NkStreamingSystem)
// -----------------------------------------------------------------------------
// Une allee de panneaux textures espaces le long de X. La camera fait des
// allers-retours : les panneaux PROCHES streament leur texture (worker disque
// + decode CPU -> upload GPU), les panneaux LOINTAINS sont EVINCES (LRU +
// stream-out distance). Le budget est volontairement SERRE pour voir
// l'eviction travailler en continu.
//
// Code couleur des panneaux (tint) :
//   GRIS ......... UNLOADED (pas en VRAM)
//   ORANGE ....... PENDING  (dans la file)
//   JAUNE ........ LOADING  (worker en cours : disque + decode)
//   TEXTURE ...... RESIDENT (texture reelle streamee)
//
//   renderdemo --demo=19
// =============================================================================
#include "DemoCommon.h"
#include "NKRenderer/Streaming/NkStreamingSystem.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKRenderer/Materials/NkMaterial.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Core/NkCameraController.h" // camera libre (WASD + souris, cf. Demo3D)
#include "NKEvent/NkEventDispatcher.h"			// NkInput
#include "NKLogger/NkLog.h"
#include <cmath>

namespace nkentseu {
	namespace demo {

		using namespace nkentseu::renderer;

		namespace {

			struct StreamPanel {
					uint64 id = 0;
					float32 x = 0.f;
					NkMaterial *mat = nullptr;
					bool bound = false;	  // texture streamee actuellement bindee au materiau
					NkTexHandle boundTex; // handle binde (change au RAFFINEMENT mip V2)
					float32 fadeIn = 0.f; // 0..1 : fondu a l'apparition (anti-pop)
					const char *label = "";
			};

			struct DemoStreamState {
					NkStreamingSystem stream;
					NkVector<StreamPanel> panels;
					NkMeshHandle meshCube;
					float32 camX = 0.f;
					// Camera : AUTO (aller-retour) ou LIBRE (WASD + clic droit,
					// comme la demo 2). Touche C pour basculer. Le mode libre
					// s'active aussi des qu'on bouge (WASD / clic droit).
					NkFlyCameraController3D flyCam;
					bool freeCam = false;
					bool cKeyWasDown = false;
			};

		} // namespace

		bool DemoStream_Init(DemoCtx &ctx) {
			auto *st = new DemoStreamState();
			ctx.userData = st;

			auto *meshSys = ctx.renderer->GetMeshSystem();
			auto *texLib = ctx.renderer->GetTextures();
			auto *matSys = ctx.renderer->GetMaterials();
			if (!meshSys || !texLib || !matSys) {
				logger.Errorf("[DemoStream] sous-systemes manquants\n");
				return true;
			}
			st->meshCube = meshSys->GetCube();

			// Budget SERRE : les textures 2k (~22 MiB avec mips) ne tiennent pas
			// toutes -> on VOIT l'eviction quand la camera change de zone.
			NkStreamingConfig cfg;
			cfg.budgetBytes = 48ULL * 1024 * 1024;
			cfg.maxJobsPerFrame = 2;
			cfg.streamInDist = 14.f;  // stream quand la camera est a moins de 14 m
			cfg.streamOutDist = 22.f; // evince au-dela de 22 m
			// Mip streaming V2 : la version FLOUE arrive d'abord ; la pleine
			// resolution remplace sous refineDist = 14 * 0.85 ≈ 11.9 m -> le
			// panneau EN FACE est net, les voisins restent flous jusqu'a
			// l'approche (l'effet est visible en se deplacant, touche C/WASD).
			cfg.refineDistMult = 0.85f;
			cfg.lowResMax = 96;
			cfg.async = true;
			st->stream.Init(ctx.renderer->GetDevice(), texLib, meshSys, cfg);

			// L'allee de panneaux (8 m d'ecart). Melange gros JPG 2k (latence de
			// decode VISIBLE) et petits PNG (stream quasi instantane).
			const struct {
					const char *path;
					const char *label;
			} kSrc[] = {
				{"Resources/NKRenderer/Textures/Vracs/Brick/brick_wall_001_diffuse_2k.jpg", "brick 2k"},
				{"Resources/NKRenderer/Textures/Vracs/awesomeface.png", "face"},
				{"Resources/NKRenderer/Textures/Vracs/Brick/brick_wall_001_normals_2k.jpg", "normals 2k"},
				{"Resources/NKRenderer/Textures/Vracs/background.jpg", "background"},
				{"Resources/NKRenderer/Textures/Vracs/Brick/brick_wall_001_ao_2k.jpg", "ao 2k"},
				{"Resources/NKRenderer/Textures/Defaults/test_pattern.png", "pattern"},
				{"Resources/NKRenderer/Textures/Vracs/block.png", "block"},
			};
			const uint32 nPanels = (uint32)(sizeof(kSrc) / sizeof(kSrc[0]));
			for (uint32 i = 0; i < nPanels; i++) {
				StreamPanel p;
				p.id = (uint64)(i + 1);
				p.x = ((float32)i - (float32)(nPanels - 1) * 0.5f) * 8.f;
				p.label = kSrc[i].label;
				p.mat = NkMaterial::Create(matSys, NkMaterialType::NK_PBR_METALLIC);
				if (p.mat)
					p.mat->SetRoughness(0.85f)->SetMetallic(0.f);
				st->stream.RegisterTexture(p.id, NkString(kSrc[i].path));
				st->panels.PushBack(p);
			}

			st->flyCam.SetPose({0.f, 3.2f, 10.5f}, -1.5708f, -0.1f); // regarde vers -Z (l'allee)

			logger.Info("[DemoStream] {0} panneaux, budget {1} MiB, in<{2}m out>{3}m\n", nPanels,
						(uint32)(cfg.budgetBytes / (1024 * 1024)), cfg.streamInDist, cfg.streamOutDist);
			logger.Info("[DemoStream] C = camera auto/libre | libre : WASD + E/Q + clic DROIT pour regarder\n");
			return true;
		}

		void DemoStream_Frame(DemoCtx &ctx, float32 dt) {
			auto *st = (DemoStreamState *)ctx.userData;
			if (!ctx.renderer->BeginFrame())
				return;
			auto *r3d = ctx.renderer->GetRender3D();
			auto *texLib = ctx.renderer->GetTextures();
			if (!r3d || !texLib) {
				ctx.renderer->Present();
				ctx.renderer->EndFrame();
				return;
			}

			// ── Camera : AUTO (aller-retour) ou LIBRE (WASD, cf. Demo3D) ─────────
			// Touche C = bascule ; bouger (WASD/clic droit) active le mode libre.
			const bool cDown = NkInput.IsKeyDown(NkKey::NK_C);
			if (cDown && !st->cKeyWasDown)
				st->freeCam = !st->freeCam;
			st->cKeyWasDown = cDown;

			NkCamera3DData camData;
			camData.up = {0, 1, 0};
			camData.fovY = 55.f;
			camData.aspect = (float32)ctx.width / (float32)ctx.height;
			camData.nearPlane = 0.05f;
			camData.farPlane = 200.f;

			const bool shift = NkInput.IsKeyDown(NkKey::NK_LSHIFT);
			const float32 spd = (shift ? 14.f : 5.f) * dt;
			float32 fwd = 0.f, rgt = 0.f, up = 0.f;
			if (NkInput.IsKeyDown(NkKey::NK_W) || NkInput.IsKeyDown(NkKey::NK_UP))
				fwd += spd;
			if (NkInput.IsKeyDown(NkKey::NK_S) || NkInput.IsKeyDown(NkKey::NK_DOWN))
				fwd -= spd;
			if (NkInput.IsKeyDown(NkKey::NK_D) || NkInput.IsKeyDown(NkKey::NK_RIGHT))
				rgt += spd;
			if (NkInput.IsKeyDown(NkKey::NK_A) || NkInput.IsKeyDown(NkKey::NK_LEFT))
				rgt -= spd;
			if (NkInput.IsKeyDown(NkKey::NK_E))
				up += spd;
			if (NkInput.IsKeyDown(NkKey::NK_Q))
				up -= spd;
			const bool rmb = NkInput.IsMouseDown(NkMouseButton::NK_MB_RIGHT);
			if (fwd != 0.f || rgt != 0.f || up != 0.f || rmb)
				st->freeCam = true; // toute action de deplacement passe en libre

			const float32 span = !st->panels.Empty() ? (st->panels[st->panels.Size() - 1].x + 4.f) : 20.f;
			NkVec3f camPos;
			if (!st->freeCam) {
				st->camX = sinf(ctx.totalTime * 0.22f) * span;
				camPos = {st->camX, 3.2f, 10.5f};
				camData.position = camPos;
				camData.target = {st->camX, 1.8f, 0.f};
			}
			NkCamera3D cam(camData);
			if (st->freeCam) {
				if (rmb)
					st->flyCam.Look((float32)NkInput.MouseDeltaX(), -(float32)NkInput.MouseDeltaY());
				st->flyCam.Move(fwd, rgt, up);
				st->flyCam.Apply(cam);
				camPos = st->flyCam.GetPosition();
				st->camX = camPos.x;
			}

			// ── Streaming : requetes par DISTANCE 3D + update (jobs/uploads) ─────
			st->stream.SetCameraPosition(camPos);
			for (auto &p : st->panels) {
				const float32 dx = p.x - camPos.x;
				const float32 dy = 2.2f - camPos.y;
				const float32 dz = 0.f - camPos.z;
				st->stream.Request(p.id, sqrtf(dx * dx + dy * dy + dz * dz));
			}
			st->stream.Update(dt);

			// ── Reglage LIVE des distances de streaming (touches 1/2, +Shift) ────
			// 1 = stream-in -/+ , 2 = stream-out -/+ (Shift = augmente).
			// L'ecart in<out est preserve (hysterese anti ping-pong).
			{
				NkStreamingConfig &scfg = st->stream.GetConfig();
				const float32 step = 8.f * dt;
				if (NkInput.IsKeyDown(NkKey::NK_NUM1))
					scfg.streamInDist = NkMax(2.f, scfg.streamInDist + (shift ? step : -step));
				if (NkInput.IsKeyDown(NkKey::NK_NUM2))
					scfg.streamOutDist = NkMax(4.f, scfg.streamOutDist + (shift ? step : -step));
				if (scfg.streamOutDist < scfg.streamInDist + 2.f)
					scfg.streamOutDist = scfg.streamInDist + 2.f;
			}

			// Bind/unbind la texture streamee sur le materiau du panneau,
			// avec FONDU a l'apparition (anti-pop : gris -> texture en ~0.4 s).
			for (auto &p : st->panels) {
				if (!p.mat)
					continue;
				const bool res = st->stream.IsResident(p.id);
				const NkTexHandle cur = st->stream.GetTexture(p.id);
				if (res && (!p.bound || cur.id != p.boundTex.id)) {
					// Premiere apparition OU raffinement mip (flou -> net) : le
					// handle change, on re-binde. Le fondu ne joue qu'a l'arrivee.
					p.mat->SetAlbedoMap(cur);
					if (!p.bound)
						p.fadeIn = 0.f; // demarre le fondu (apparition)
					p.bound = true;
					p.boundTex = cur;
				} else if (!res && p.bound) {
					// Texture evincee : rebinde le blanc (le handle streame est mort).
					p.mat->SetAlbedoMap(texLib->GetWhite1x1());
					p.bound = false;
					p.boundTex = {};
					p.fadeIn = 0.f;
				}
				if (p.bound && p.fadeIn < 1.f)
					p.fadeIn = NkMin(1.f, p.fadeIn + dt * 2.5f);
			}

			// ── Scene ────────────────────────────────────────────────────────────
			NkSceneContext sctx;
			sctx.camera = cam;
			NkLightDesc sun;
			sun.type = NkLightType::NK_DIRECTIONAL;
			sun.direction = {-0.35f, -0.8f, -0.5f};
			sun.color = {1.f, 0.98f, 0.95f};
			sun.intensity = 2.4f;
			sctx.lights.PushBack(sun);
			sctx.ambientIntensity = 0.5f;
			r3d->BeginScene(sctx);

			// Sol.
			if (auto *meshSys = ctx.renderer->GetMeshSystem()) {
				NkDrawCall3D dc;
				dc.mesh = meshSys->GetPlane();
				dc.transform = NkMat4f::Translate({0.f, 0.f, 0.f}) * NkMat4f::Scale({span + 12.f, 1.f, 14.f});
				dc.aabb = {{-span - 12.f, -0.1f, -14.f}, {span + 12.f, 0.1f, 14.f}};
				dc.tint = {0.16f, 0.17f, 0.2f};
				dc.roughness = 0.95f;
				r3d->Submit(dc);
			}

			// Panneaux : 4 m x 4 m, code couleur par etat (cf. entete).
			for (auto &p : st->panels) {
				const NkStreamState s = st->stream.GetState(p.id);
				NkDrawCall3D dc;
				dc.mesh = st->meshCube;
				dc.transform = NkMat4f::Translate({p.x, 2.2f, 0.f}) * NkMat4f::Scale({4.f, 4.f, 0.15f});
				dc.aabb = {{p.x - 2.1f, 0.1f, -0.2f}, {p.x + 2.1f, 4.3f, 0.2f}};
				dc.castShadow = true;
				if (s == NkStreamState::NK_RESIDENT && p.mat) {
					dc.material = p.mat->GetInstHandle();
					// Fondu gris -> blanc : la texture "arrive" en douceur.
					const float32 f = 0.35f + 0.65f * p.fadeIn;
					dc.tint = {f, f, f};
				} else if (s == NkStreamState::NK_LOADING) {
					dc.tint = {0.9f, 0.85f, 0.2f}; // jaune : worker en cours
				} else if (s == NkStreamState::NK_PENDING) {
					dc.tint = {0.9f, 0.55f, 0.15f}; // orange : en file
				} else {
					dc.tint = {0.35f, 0.36f, 0.4f}; // gris : pas en VRAM
				}
				dc.roughness = 0.85f;
				r3d->Submit(dc);
			}

			if (auto *overlay = ctx.renderer->GetOverlay()) {
				overlay->BeginOverlay(ctx.renderer->GetCmd(), ctx.width, ctx.height);
				overlay->DrawText({20.f, 35.f}, "DemoStream : streaming REEL  |  API : %s", NkGraphicsApiName(ctx.api));
				overlay->DrawText({20.f, 55.f}, "GRIS=unloaded  ORANGE=pending  JAUNE=loading  TEXTURE=resident");
				overlay->DrawText({20.f, 75.f}, "resident:%u  pending:%u  loading:%u  evicted:%u  failed:%u",
								  st->stream.GetResidentCount(), st->stream.GetPendingCount(),
								  st->stream.GetLoadingCount(), st->stream.GetEvictedCount(),
								  st->stream.GetFailedCount());
				overlay->DrawText({20.f, 95.f}, "VRAM stream : %.1f / %.0f MiB  (%.0f%%)",
								  (float64)st->stream.GetUsedBytes() / (1024.0 * 1024.0),
								  (float64)st->stream.GetBudgetBytes() / (1024.0 * 1024.0),
								  st->stream.GetUsageRatio() * 100.f);
				overlay->DrawText({20.f, 115.f}, "cam %s : x=%.1f  |  in<%.1fm  out>%.1fm  |  FPS:%.0f",
								  st->freeCam ? "LIBRE" : "AUTO", st->camX, st->stream.GetConfig().streamInDist,
								  st->stream.GetConfig().streamOutDist, dt > 1e-5f ? 1.f / dt : 0.f);
				overlay->DrawText({20.f, 135.f}, "C=auto/libre  WASD+E/Q+clic DROIT  |  1/2=dist in/out (-, Shift=+)");
				overlay->EndOverlay();
			}

			ctx.renderer->Present();
			ctx.renderer->EndFrame();
		}

		void DemoStream_Shutdown(DemoCtx &ctx) {
			auto *st = (DemoStreamState *)ctx.userData;
			if (st) {
				for (auto &p : st->panels)
					if (p.mat)
						NkMaterial::Destroy(p.mat);
				st->panels.Clear();
				st->stream.Shutdown();
				delete st;
			}
			ctx.userData = nullptr;
			logger.Info("[DemoStream] Shutdown\n");
		}

	} // namespace demo
} // namespace nkentseu
