// =============================================================================
// DemoNKGen.cpp — EXEMPLE : rendre un maillage GÉNÉRÉ par l'IA (NKGen) dans le
// renderdemo (NKRenderer v5.0). Fidèle à Demo3D.cpp.
//
// ── Intégration (renderdemo = Applications/Sandbox) ─────────────────────────
//  1) Copier ce fichier dans  Applications/Sandbox/src/Demo/  (il sera globé par
//     `files(["src/Demo/**.cpp"])`).
//  2) Dans Applications/Sandbox/src/Demo/main.cpp :
//     • Ajouter la forward-decl (près des autres, ~ligne 55) :
//         bool DemoNKGen_Init(DemoCtx&); void DemoNKGen_Frame(DemoCtx&, float32); void DemoNKGen_Shutdown(DemoCtx&);
//     • Ajouter une entrée dans kDemos[] (à la fin) :
//         { "NKGen", "Maillage genere par l'IA (NKGen -> SurfaceNets -> rendu)",
//             DemoNKGen_Init, DemoNKGen_Frame, DemoNKGen_Shutdown },
//     • (config) dans BuildConfig, faire retomber le nouvel index sur `ForGame`
//       (comme le case 2 de Demo3D) — ou ajouter `case <idx>: { auto c =
//       NkRendererConfig::ForGame(api,w,h); c.shadow.cascadeCount = 1; return c; }`.
//  3) Dans RendererSandbox.jenga : donner accès à NKGen (+ NKNN, NKAutograd,
//     NKTensor) au projet renderdemo — ajouter leurs libs/inc au `useappdeps` /
//     `_SANDBOX_INCLUDE_DIRS` (ces 4 modules AI + leurs .location/src).
//  4) Lancer :  .\Build\Bin\Debug-Windows\renderdemo\renderdemo.exe --demo=<idx> -bdx11
//
// La forme est générée EN MÉMOIRE au Init (métaballs -> Surface Nets), centrée,
// normales calculées, écrite en OBJ temporaire puis chargée par le mesh system
// du moteur (meshSys->Import). Le rendu réutilise le path prouvé de Demo3D.
// =============================================================================
#include "DemoCommon.h"
#include "NKRenderer/Core/NkCameraController.h" // NkOrbitCameraController3D
#include "NKLogger/Sinks/NkFileSink.h"			// redirection logger -> fichier

#include "NKGen/NkGen.h" // gen::SurfaceNets / ComputeNormals / SaveMeshObj

#include <cstdio>
#include <cmath>

namespace nkentseu {
	namespace demo {

		using namespace nkentseu::ai;

		struct DemoNKGenState {
				NkMeshHandle mesh;
				renderer::NkOrbitCameraController3D cam;
				float32 angle = 0.f;
				renderer::NkOffscreenTarget *rt = nullptr; // cible offscreen pour screenshot
				int32 frameCount = 0;
				bool redirected = false;
				bool captured = false;
		};

		// Génère un champ de métaballs NxN xN et en extrait une surface lisse.
		static gen::NkMesh GenerateShape() {
			const uint32 N = 40;
			NkVector<float> field;
			field.Resize(N * N * N);
			const float centers[3][3] = {{-0.6f, 0.f, 0.f}, {0.6f, 0.f, 0.f}, {0.f, 0.7f, 0.2f}};
			for (uint32 z = 0; z < N; ++z)
				for (uint32 y = 0; y < N; ++y)
					for (uint32 x = 0; x < N; ++x) {
						float px = -2.f + 4.f * (float)x / (N - 1), py = -2.f + 4.f * (float)y / (N - 1),
							  pz = -2.f + 4.f * (float)z / (N - 1);
						float s = 0.f;
						for (int b = 0; b < 3; ++b) {
							float dx = px - centers[b][0], dy = py - centers[b][1], dz = pz - centers[b][2];
							s += 1.0f / (dx * dx + dy * dy + dz * dz + 0.05f);
						}
						field[x + N * (y + N * z)] = s;
					}
			gen::NkMesh m = gen::SurfaceNets(field.Data(), N, N, N, /*iso*/ 3.0f, 1.0f);
			// Centre + met à l'échelle (repère ~[-1,1], posé au-dessus du sol).
			const uint32 vc = m.VertexCount();
			if (vc) {
				float mn[3] = {1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f, -1e30f, -1e30f};
				for (uint32 i = 0; i < vc; ++i)
					for (int k = 0; k < 3; ++k) {
						float p = m.positions[i * 3 + k];
						if (p < mn[k])
							mn[k] = p;
						if (p > mx[k])
							mx[k] = p;
					}
				float c[3] = {(mn[0] + mx[0]) * .5f, (mn[1] + mx[1]) * .5f, (mn[2] + mx[2]) * .5f}, ext = 0.f;
				for (int k = 0; k < 3; ++k) {
					float e = mx[k] - mn[k];
					if (e > ext)
						ext = e;
				}
				if (ext < 1e-6f)
					ext = 1e-6f;
				float sc = 2.0f / ext;
				for (uint32 i = 0; i < vc; ++i) {
					m.positions[i * 3 + 0] = (m.positions[i * 3 + 0] - c[0]) * sc;
					m.positions[i * 3 + 1] = (m.positions[i * 3 + 1] - c[1]) * sc + 1.2f;
					m.positions[i * 3 + 2] = (m.positions[i * 3 + 2] - c[2]) * sc;
				}
			}
			gen::ComputeNormals(m);
			return m;
		}

		bool DemoNKGen_Init(DemoCtx &ctx) {
			// Redirige le logger vers un fichier (analyse hors-écran de la sortie moteur).
			// Une seule fois, tronqué à chaque lancement.
			static bool sSinkAdded = false;
			if (!sSinkAdded) {
				memory::NkSharedPtr<NkISink> fileSink(
					new NkFileSink(NkString("Build/nkgen_demo.log"), /*truncate*/ true));
				fileSink->SetLevel(NkLogLevel::NK_TRACE); // tout capturer
				logger.AddSink(fileSink);
				sSinkAdded = true;
			}
			logger.Info("[DemoNKGen] === Init : generation + rendu du maillage IA ===\n");

			auto *st = new DemoNKGenState();
			ctx.userData = st;

			// 1) Générer -> OBJ temporaire -> Import par le mesh system du moteur.
			gen::NkMesh m = GenerateShape();
			logger.Info("[DemoNKGen] Forme generee : {0} sommets, {1} triangles\n", (uint64)m.VertexCount(),
						(uint64)m.TriangleCount());
			const char *path = "Build/nkgen_demo.obj";
			gen::SaveMeshObj(path, m);

			auto *meshSys = ctx.renderer->GetMeshSystem();
			st->mesh = meshSys->Import(NkString(path));
			if (!st->mesh.IsValid()) {
				logger.Errorf("[DemoNKGen] Import du maillage genere a echoue (%s)\n", path);
				// Fallback : une sphere primitive pour ne pas planter.
				st->mesh = meshSys->GetSphere();
			} else {
				logger.Info("[DemoNKGen] Maillage IA charge : {0} sommets\n", (uint64)m.VertexCount());
			}

			st->cam.SetCenter({0.f, 1.2f, 0.f}, 4.5f, 0.7f, 0.35f);

			if (auto *r3d = ctx.renderer->GetRender3D())
				r3d->SetInfiniteGridEnabled(true);

			// Cible offscreen (readback) pour capturer une image du rendu moteur -> PNG.
			renderer::NkOffscreenDesc od;
			od.width = ctx.width;
			od.height = ctx.height;
			od.hdr = false;
			od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
			od.hasDepth = true;
			od.readable = true;
			od.readback = true;
			od.name = "NKGenCapture";
			st->rt = ctx.renderer->CreateOffscreen(od);

			logger.Info("[DemoNKGen] Init termine, mesh valide={0}, offscreen={1}\n", st->mesh.IsValid() ? 1 : 0,
						(st->rt && st->rt->IsValid()) ? 1 : 0);
			logger.Flush(); // persiste les logs d'init immediatement (analyse hors-ecran)
			return true;
		}

		void DemoNKGen_Frame(DemoCtx &ctx, float32 dt) {
			auto *st = (DemoNKGenState *)ctx.userData;
			st->angle += dt * 0.4f;
			st->frameCount++;

			// Screenshot en 2 temps (robuste au RebuildRenderGraph) :
			//   frame 60 : rediriger le render graph vers l'offscreen (rebuild)
			//   frames 60..63 : le pipeline complet rend dans l'offscreen
			//   frame 63 (apres EndFrame) : Capture(png) puis restaurer le swapchain
			if (!st->captured && st->rt && st->rt->IsValid() && !st->redirected && st->frameCount == 60) {
				auto *texLib = ctx.renderer->GetTextures();
				ctx.renderer->SetFinalColorTarget(texLib->GetRHIHandle(st->rt->GetColorResult()));
				st->redirected = true;
				logger.Info("[DemoNKGen] frame {0} : graph redirige -> offscreen\n", st->frameCount);
				logger.Flush();
			}

			if (!ctx.renderer->BeginFrame())
				return;
			auto *r3d = ctx.renderer->GetRender3D();
			if (!r3d) {
				ctx.renderer->Present();
				ctx.renderer->EndFrame();
				return;
			}

			// Caméra orbitale (comme Demo3D).
			NkCamera3DData camData;
			camData.up = {0.f, 1.f, 0.f};
			camData.fovY = 60.f;
			camData.aspect = (float32)ctx.width / (float32)ctx.height;
			camData.nearPlane = 0.1f;
			camData.farPlane = 100.f;
			NkCamera3D cam(camData);
			st->cam.SetCenter({0.f, 1.2f, 0.f}, 4.5f, st->angle, 0.35f);
			st->cam.Apply(cam);

			// Scène : 1 soleil directionnel + fill.
			NkSceneContext sctx;
			sctx.camera = cam;
			sctx.time = ctx.totalTime;
			NkLightDesc sun;
			sun.type = NkLightType::NK_DIRECTIONAL;
			sun.direction = {-0.4f, -1.f, -0.3f};
			sun.color = {1.f, 0.96f, 0.9f};
			sun.intensity = 3.f;
			sun.castShadow = true;
			sun.shadowStatic = true;
			sctx.lights.PushBack(sun);
			NkLightDesc fill;
			fill.type = NkLightType::NK_POINT;
			fill.position = {2.5f, 2.f, 1.5f};
			fill.color = {0.5f, 0.6f, 1.f};
			fill.intensity = 4.f;
			fill.range = 12.f;
			sctx.lights.PushBack(fill);
			sctx.ambientIntensity = 0.18f;

			r3d->BeginScene(sctx);

			// Sol (plane primitif).
			auto *meshSys = ctx.renderer->GetMeshSystem();
			{
				NkDrawCall3D dc;
				dc.mesh = meshSys->GetPlane();
				dc.transform = NkMat4f::Scale({40.f, 1.f, 40.f});
				dc.aabb = {{-40, 0, -40}, {40, 0, 40}};
				dc.castShadow = false;
				dc.tint = {0.12f, 0.12f, 0.13f};
				dc.metallic = 0.f;
				dc.roughness = 0.92f;
				r3d->Submit(dc);
			}
			// Le maillage GÉNÉRÉ par l'IA — matériau type "terracotta" + rotation lente.
			{
				NkDrawCall3D dc;
				dc.mesh = st->mesh;
				dc.transform = NkMat4f::RotationY(NkAngle::FromRad(ctx.totalTime * 0.5f));
				dc.aabb = {{-1.2f, 0.f, -1.2f}, {1.2f, 2.6f, 1.2f}};
				dc.tint = {0.85f, 0.45f, 0.28f};
				dc.metallic = 0.f;
				dc.roughness = 0.55f;
				dc.castShadow = true;
				r3d->Submit(dc);
			}

			if (auto *overlay = ctx.renderer->GetOverlay()) {
				overlay->BeginOverlay(ctx.renderer->GetCmd(), ctx.width, ctx.height);
				overlay->DrawStats(ctx.renderer->GetStats());
				overlay->DrawText({20.f, 35.f}, "DemoNKGen — maillage genere par l'IA (NKGen)  |  API : %s",
								  NkGraphicsApiName(ctx.api));
				overlay->EndOverlay();
			}

			ctx.renderer->Present();
			ctx.renderer->EndFrame();

			if (!st->captured && st->redirected && st->frameCount >= 63) {
				const bool ok = st->rt->Capture("Build/nkgen_engine.png");
				logger.Info("[DemoNKGen] frame {0} : Capture rendu moteur -> Build/nkgen_engine.png : {1}\n",
							st->frameCount, ok ? 1 : 0);
				logger.Flush();
				ctx.renderer->SetFinalColorTarget(NkTextureHandle{}); // restaure le swapchain
				st->captured = true;
			}
		}

		void DemoNKGen_Shutdown(DemoCtx &ctx) {
			auto *st = (DemoNKGenState *)ctx.userData;
			if (st && st->rt)
				ctx.renderer->DestroyOffscreen(st->rt);
			delete st;
			ctx.userData = nullptr;
		}

	} // namespace demo
} // namespace nkentseu
