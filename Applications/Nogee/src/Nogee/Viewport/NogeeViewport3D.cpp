// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Nogee/Viewport/NogeeViewport3D.cpp — SEULE unite de Nogee a inclure NKRenderer
// =============================================================================
// Calibre sur le controle positif le plus proche : `NkAnimaEditor/AnimBridge.cpp`
// (meme NkEditorShell, meme NkEditorRHIRenderer, meme crochet preUI). Ce qui
// change ici, et c'est tout le lot : la scene n'est pas construite a la main,
// elle est LUE DANS LE MONDE ECS par `NkRenderSystem`.
//
// TROIS CHOSES QU'IL FAUT REJOUER A LA MAIN, parce que l'editeur possede la
// frame device et que `NkRenderer::BeginFrame()` n'est donc jamais appele :
//   1. `r3d->ResetFrame()` — une fois par frame, avant toute passe ;
//   2. `GetMaterialCollection()->Upload()` — sinon les materiaux GPU sont vides ;
//   3. `graph->Execute(cmd)` a la place de `Present()`.
// C'est exactement la liste qu'`AnimBridge.cpp` l.885-887 et l.1039-1040 tient.
//
// ⚠️ ET SURTOUT PAS `Flush` A LA MAIN. `NkRender3D::Flush(cmd)` (NkRender3D.cpp
// l.2046) enregistre des draws en supposant une passe DEJA ouverte, et met
// `mInScene = false` ; c'est le RenderGraph qui l'appelle, depuis sa passe
// Geometry (`NkRendererImpl.cpp` l.925). Or `NkRenderSystem::Execute` l.53
// l'appelait lui-meme : la scene etait consommee AVANT le graphe, et la passe
// Geometry ressortait a sa premiere ligne. D'ou `SetOwnsFlush(false)` ci-dessous
// — le plus petit changement de contrat qui ouvre ce chemin, et celui que les
// deux hotes qui marchent respectent deja de fait.
// =============================================================================

#include "Nogee/Viewport/NogeeViewport3D.h"

#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkRendererConfig.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Core/NkSceneContext.h"
#include "NKRenderer/Core/NkRenderGraph.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Materials/NkMaterialCollection.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKGui/NkGuiRHIBackend.h"

#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Components/Core/NkCoreComponents.h"
#include "Noge/ECS/Components/Rendering/NkRenderComponents.h"
#include "Noge/ECS/Systems/NkTransformSystem.h"
#include "Noge/ECS/Systems/NkRenderSystem.h"
#include "Nogee/Editor/NkSelectionManager.h" // LA selection de l'editeur, pas une copie

#include "NKTime/NkChrono.h"
#include "NKLogger/NkLog.h"
#include <cmath>
#include <cstdio>

namespace nkentseu {
	namespace noge {

		using namespace nkentseu::renderer;
		using namespace nkentseu::math;

		namespace {

			struct Vp3D {
					// ── Monte par l'hote ──────────────────────────────────────
					NkIDevice *sharedDev = nullptr;
					ecs::NkWorld *world = nullptr;
					// LE meme objet que l'Outliner et Details lisent. Jamais une copie.
					NkSelectionManager *selection = nullptr;

					// ── Pile de rendu (creee paresseusement) ──────────────────
					bool tried = false;
					bool ok = false;
					NkRenderer *r3 = nullptr;
					NkOffscreenTarget *rt = nullptr;
					uint32 rtW = 1280, rtH = 720;
					uint32 wantW = 1280, wantH = 720;

					// ── Les deux systemes ECS, possedes ici ───────────────────
					// Ils sont sans etat partage : les instancier hors scheduler
					// est le meme geste que `NkEngineLayer` fait avec le sien.
					NkTransformSystem transforms;
					NkRenderSystem render;

					// ── Camera d'orbite (entite ECS SANS NkSceneNode) ─────────
					// Sans NkSceneNode elle n'entre pas dans la requete de
					// l'Outliner (`WorldOutlinerPanel.cpp` l.82) : le viewport a
					// sa camera, l'arbre de scene ne change pas d'une ligne.
					ecs::NkEntityId camId{};
					ecs::NkEntityId sunId{};
					float32 yawDeg = 35.f;
					float32 pitchDeg = 22.f;
					float32 dist = 6.f;
					NkVec3f target{0.f, 0.f, 0.f};

					// ── Temoins ───────────────────────────────────────────────
					int32 frames = 0;
					int32 selSoumises = 0; ///< entites REELLEMENT soumises au lisere
					char nomsDessines[512] = {}; ///< « Nom(source du maillage) », lisible tel quel
					int32 eligible = 0; ///< entites qui satisfont la requete de SubmitMeshes
					uint32 dcGraphe = 0;  ///< appels de dessin ENREGISTRES pendant graph->Execute
					uint32 triGraphe = 0; ///< triangles enregistres pendant graph->Execute
					float64 lastNs = 0.0;

					// Rectangle ECRAN de la vue, depose par le panneau chaque image.
					// Sert a traduire la souris fenetre -> souris vue ; personne ne le
					// devine, c'est le panneau qui le MESURE.
					float32 vueX = 0.f, vueY = 0.f, vueW = 0.f, vueH = 0.f;
					bool vueSurvol = false;

					NkMeshHandle cube{};

					// Controle positif interne (--viewport-controle) : un cube soumis
					// a la main, qui partage toutes les causes du cube ECS sauf le
					// pont ECS lui-meme.
					bool controle = false;

					// Capture differee : on ecrit l'image N APRES qu'elle a ete rendue.
					char capPath[512] = {};
					int32 capFrame = -1;
			};

			Vp3D g;

			// Le meme predicat que `NkRenderSystem::SubmitMeshes` (NkRenderSystem.cpp
			// l.142-148), recompte ICI. Deux raisons : un temoin qui vient du meme
			// endroit que la chose mesuree ne prouve rien, et les statistiques du
			// renderer sont figees par `EndFrame()` — que l'editeur ne nous donne pas.
			// Le meme predicat que `NkRenderSystem::SubmitMeshes` (NkRenderSystem.cpp
			// l.142-148), recompte ICI — un temoin qui vient du meme endroit que la
			// chose mesuree ne prouve rien. Il collecte AUSSI les noms : si Rodolf
			// doit nous dire ce qu'il voit, autant que son ecran le lui dise.
			int32 CountEligible(ecs::NkWorld &w, char *noms, size_t tailleNoms) {
				int32 n = 0;
				size_t pos = 0;
				if (noms && tailleNoms > 0)
					noms[0] = '\0';
				w.Query<ecs::NkTransform, ecs::NkMeshComponent, ecs::NkMaterialComponent>().ForEach(
					[&](ecs::NkEntityId id, const ecs::NkTransform &, const ecs::NkMeshComponent &m,
						const ecs::NkMaterialComponent &) {
						if (w.Has<ecs::NkInactive>(id))
							return;
						if (!m.visible)
							return;
						++n;
						if (!noms || pos + 1 >= tailleNoms)
							return;
						// Le NOM de l'entite, et D'OU vient son maillage. « primitive »
						// quand il n'a pas de fichier : c'est precisement le cas qui ne
						// survit pas a une sauvegarde, et le voir ecrit evite de le
						// confondre avec un maillage importe.
						const ecs::NkName *nm = w.Get<ecs::NkName>(id);
						const char *src = m.meshPath.Empty() ? "primitive" : m.meshPath.CStr();
						// LA POIGNEE ET LE NOMBRE DE TRIANGLES, par entite. Deux pistes
						// s'eliminent avec ces deux chiffres : si deux entites portent la
						// MEME poignee, la bibliotheque a reutilise un emplacement ; si une
						// entite annonce le nombre de triangles d'une AUTRE, c'est
						// l'intervalle d'indices qui a bouge. On ne devine ni l'un ni
						// l'autre : on les ecrit.
						unsigned long long poignee = (unsigned long long)m.meshHandle;
						int tri = -1;
						if (NkMeshSystem *ms = g.r3 ? g.r3->GetMeshSystem() : nullptr) {
							NkMeshHandle h{m.meshHandle};
							if (h.IsValid())
								tri = (int)(ms->GetIndexCount(h) / 3u);
						}
						const int ecrit = std::snprintf(noms + pos, tailleNoms - pos,
														"%s%s[maillage %s, poignee %llu, %d triangle(s)]",
														pos ? ", " : "", nm ? nm->value : "(sans nom)", src,
														poignee, tri);
						if (ecrit > 0)
							pos += (size_t)ecrit;
					});
				if (noms && pos == 0 && tailleNoms > 8)
					std::snprintf(noms, tailleNoms, "AUCUN");
				return n;
			}

			bool Init3D() {
				if (g.tried)
					return g.ok;
				g.tried = true;
				if (!g.sharedDev || !g.sharedDev->IsValid()) {
					logger.Error("[Nogee/Viewport3D] device partage absent — "
								 "NogeeViewport3DSetDevice n'a pas ete appele\n");
					return false;
				}

				// UNE pile GPU par fenetre : on PARTAGE le device de l'editeur.
				NkRendererConfig cfg = NkRendererConfig::ForGame(g.sharedDev->GetApi(), g.wantW, g.wantH);
				cfg.Enable(NK_SS_OFFSCREEN);
				cfg.Enable(NK_SS_POST_PROCESS);
				cfg.shadow.cascadeCount = 1;
				cfg.postProcess.toneMapping = true;
				cfg.postProcess.aces = true;
				cfg.postProcess.gamma = 2.2f;
				cfg.postProcess.bloom = false;
				cfg.postProcess.ssao = false;
				cfg.voxelAOEnabled = false;
				// Ambiant procedural : sans lui, toutes les faces qui ne regardent
				// pas la directionnelle sont NOIRES, et un cube noir sur fond sombre
				// ne se distingue pas d'un viewport vide. C'est la meme raison, et le
				// meme reglage, que dans AnimBridge (« cle de la visibilite »).
				cfg.ibl.useHDR = false;
				cfg.ibl.iblStrength = 1.2f;
				g.r3 = NkRenderer::Create(g.sharedDev, cfg);
				if (!g.r3) {
					logger.Error("[Nogee/Viewport3D] NkRenderer::Create a echoue\n");
					return false;
				}

				NkOffscreenDesc od;
				od.width = g.wantW;
				od.height = g.wantH;
				od.hdr = false;
				od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
				od.hasDepth = true;
				od.readable = true;
				// `Capture()` l'exige (NkOffscreenTarget.h l.33-36). C'est notre seul
				// instrument de mesure : relire la cible, pas l'ecran.
				od.readback = true;
				od.name = "NogeeViewport";
				g.rt = g.r3->CreateOffscreen(od);
				if (!g.rt || !g.rt->IsValid()) {
					logger.Error("[Nogee/Viewport3D] cible hors ecran : echec\n");
					return false;
				}
				g.rtW = g.wantW;
				g.rtH = g.wantH;
				// ⚠️ MESURE DU 13/09, ET ELLE COUTE UN ECRAN ENTIER : sans cette
				// ligne, le render graph dimensionne TOUTES ses cibles transitoires
				// sur la SWAPCHAIN (1600x900, la fenetre de Nogee) alors que la
				// cible finale, la notre, fait 1280x720. OpenGL refuse l'assemblage
				// — « Framebuffer incomplete: 0x8CD6 » (INCOMPLETE_ATTACHMENT), 14
				// fois, puis GL_INVALID_FRAMEBUFFER_OPERATION a chaque glClear et
				// chaque glDrawArrays. Rien ne se dessine, et aucune couche au-dessus
				// ne dit pourquoi. NkAnimaEditor n'a jamais paye ce defaut parce que
				// sa fenetre fait 1280x720, exactement la taille de sa cible : le
				// controle positif etait d'accord par COINCIDENCE.
				g.r3->SetRenderSizeOverride(g.wantW, g.wantH);
				// La sortie du render graph (normalement la swapchain) est redirigee
				// vers notre cible : le pipeline COMPLET (ombres, eclairage, IBL,
				// tonemap) rend dans le viewport, et l'interface l'echantillonne.
				if (auto *texLib = g.r3->GetTextures())
					g.r3->SetFinalColorTarget(texLib->GetRHIHandle(g.rt->GetColorResult()));

				if (auto *meshSys = g.r3->GetMeshSystem())
					g.cube = meshSys->GetCube();

				// Le pont ECS -> NKRenderer, tel quel. Le command buffer change a
				// chaque frame : il est repose dans NogeeViewport3DFrame.
				g.render.Init(g.r3, nullptr);
				// ⚠️ Le contrat ouvert pour ce chemin : l'hote flushe (via le graphe).
				g.render.SetOwnsFlush(false);
				g.render.SetAmbientIntensity(0.45f);

				g.ok = true;
				logger.Info("[Nogee/Viewport3D] pile prete : renderer + cible {0}x{1} + NkRenderSystem\n",
							g.rtW, g.rtH);
				return true;
			}

			// La camera et le soleil sont des ENTITES : c'est la seule maniere
			// d'alimenter `NkRenderSystem` sans lui inventer une porte de service.
			void EnsureCameraEntity() {
				if (!g.world)
					return;
				if (!g.camId.IsValid()) {
					g.camId = g.world->CreateEntity();
					ecs::NkCameraComponent cam;
					cam.fovDeg = 50.f;
					cam.nearClip = 0.05f;
					cam.farClip = 500.f;
					cam.priority = 100; // la vue de l'editeur passe devant toute camera de jeu
					g.world->Add<ecs::NkCameraComponent>(g.camId, cam);
					g.world->Add<ecs::NkTransform>(g.camId);
					logger.Info("[Nogee/Viewport3D] camera d'editeur creee (entite ECS sans NkSceneNode : "
								"INVISIBLE dans l'Outliner, par construction)\n");
				}
				if (!g.sunId.IsValid()) {
					g.sunId = g.world->CreateEntity();
					ecs::NkLightComponent sun;
					sun.type = ecs::NkLightType::Directional;
					sun.intensity = 3.0f;
					sun.castShadow = true;
					g.world->Add<ecs::NkLightComponent>(g.sunId, sun);
					ecs::NkTransform tf;
					tf.SetLocalRotationEuler(-50.f, 30.f, 0.f);
					g.world->Add<ecs::NkTransform>(g.sunId, tf);
				}
			}

			void UpdateCameraEntity() {
				if (!g.world || !g.camId.IsValid())
					return;
				auto *tf = g.world->Get<ecs::NkTransform>(g.camId);
				auto *cam = g.world->Get<ecs::NkCameraComponent>(g.camId);
				if (!tf || !cam)
					return;

				const float32 kDeg2Rad = 3.14159265358979f / 180.f;
				const float32 y = g.yawDeg * kDeg2Rad;
				const float32 p = g.pitchDeg * kDeg2Rad;
				const float32 cp = std::cos(p), sp = std::sin(p);
				const NkVec3f eye{g.target.x + std::sin(y) * cp * g.dist, g.target.y + sp * g.dist,
								  g.target.z + std::cos(y) * cp * g.dist};

				// Position + orientation LOCALES : c'est NkTransformSystem qui en
				// fera worldMatrix, et NkRenderSystem qui en fera view/proj. On ne
				// court-circuite rien.
				tf->SetLocalPosition(eye);

				// ⚠️ NE PAS PASSER PAR NkQuatf::LookAt ICI, ET C'EST MESURE.
				// Avec `SetLocalRotation(NkQuatf::LookAt(eye, cible, up))`, la
				// camera regardait A L'OPPOSE de la scene : avant REELLE
				// (0.83, 0.36, 0.43) contre avant ATTENDUE (-0.53, -0.37, -0.76),
				// produit scalaire -0.901. Ce n'est meme pas une simple negation —
				// les deux ne sont pas colineaires : `LookAt` aligne son repere
				// selon une convention CAMERA, et `NkTransform::GetWorldForward()`
				// (NkTransform.h l.86-88) lit `-colonne2` de la matrice monde. Les
				// deux ne se repondent pas.
				// Consequence en cascade, et rien ne la signalait :
				// `NkRenderSystem::UpdateActiveCamera` (l.79-84) construit sa cible
				// par `pos + GetWorldForward()` -> la camera vise le vide -> le
				// frustum ecarte TOUT dans `NkRender3D::Submit` (l.1704-1707) ->
				// `mOpaque` est vide quand `Flush` s'execute -> aucun pixel.
				//
				// On construit donc le repere DANS LA CONVENTION QUI SERA LUE :
				// colonne 2 = -avant (puisque GetWorldForward rend -colonne2), puis
				// right et up par produits vectoriels. Le passage matrice ->
				// quaternion est la conversion GENERALE (trace-based, Mike Day),
				// pas la reconstruction par LookAt que NkQuat.h l.579-581 declare
				// justement FAUSSE hors cas camera.
				NkVec3f avant{g.target.x - eye.x, g.target.y - eye.y, g.target.z - eye.z};
				{
					const float32 n = std::sqrt(avant.x * avant.x + avant.y * avant.y + avant.z * avant.z);
					if (n > 1e-6f) {
						avant.x /= n;
						avant.y /= n;
						avant.z /= n;
					} else {
						avant = {0.f, 0.f, -1.f};
					}
				}
				// La construction du repere a ete HISSEE DANS NKMATH le meme jour :
				// le piege n'est pas propre a Nogee, et une copie locale en aurait
				// fait un correctif par site. `NkQuatf::FromForwardUp` garantit la
				// seule chose qu'on relit — `-colonne2 == avant` — et elle est
				// gardee par `NKMath/tests/test_camera_axis.cpp`, banc vu ROUGE
				// (avec LookAt) puis vert. Une seule implementation, surveillee.
				tf->SetLocalRotation(NkQuatf::FromForwardUp(avant, NkVec3f{0.f, 1.f, 0.f}));
				cam->aspect = (float32)g.rtW / (float32)(g.rtH > 0 ? g.rtH : 1);
			}

		} // namespace

		// =====================================================================
		// Facade
		// =====================================================================
		void NogeeViewport3DSetDevice(void *device) {
			g.sharedDev = (NkIDevice *)device;
		}

		void NogeeViewport3DBindWorld(void *world) {
			g.world = (ecs::NkWorld *)world;
		}

		void NogeeViewport3DBindSelection(void *selectionManager) {
			g.selection = (NkSelectionManager *)selectionManager;
		}

		bool NogeeViewport3DInit() {
			if (!Init3D())
				return false;
			EnsureCameraEntity();
			return true;
		}

		bool NogeeViewport3DReady() {
			return g.ok && g.rt != nullptr;
		}

		nk_uint64 NogeeViewport3DCubeMeshHandle() {
			return g.cube.IsValid() ? g.cube.id : 0ull;
		}

		void NogeeViewport3DResize(uint32 w, uint32 h) {
			// Deuxieme garde, et elle est deliberee : l'appelant borne deja, mais
			// une cible de rendu absurde ne se rate pas proprement — elle rend un
			// framebuffer incomplet et un ecran vide sans message. On refuse ici.
			if (w < 16u)
				w = 16u;
			if (h < 16u)
				h = 16u;
			if (w > 4096u)
				w = 4096u;
			if (h > 4096u)
				h = 4096u;
			g.wantW = w;
			g.wantH = h;
			if (!g.ok || !g.rt || (w == g.rtW && h == g.rtH))
				return;
			if (g.rt->Resize(w, h)) {
				g.rtW = w;
				g.rtH = h;
				g.r3->SetRenderSizeOverride(w, h);
				if (auto *texLib = g.r3->GetTextures())
					g.r3->SetFinalColorTarget(texLib->GetRHIHandle(g.rt->GetColorResult()));
			}
		}

		void NogeeViewport3DFrame(void *cmdv) {
			if (!Init3D() || !cmdv || !g.world)
				return;
			NkICommandBuffer *cmd = (NkICommandBuffer *)cmdv;
			NkRender3D *r3d = g.r3->GetRender3D();
			if (!r3d)
				return;
			EnsureCameraEntity();

			// ── CAPTURE, EN DEUX TEMPS, et c'est oblige ──────────────────────
			// On rend a l'image N, on relit a l'image N+1 : une relecture doit
			// porter sur une image TERMINEE. Meme discipline que les vignettes de
			// materiau de NK3DModeler.
			if (g.capPath[0] != '\0' && g.capFrame >= 0 && g.frames >= g.capFrame) {
				const bool ok = g.rt->Capture(g.capPath);
				char m[640];
				std::snprintf(m, sizeof(m),
							  "[Nogee/Viewport3D] CAPTURE image %d -> '%s' : %s (cible %ux%u, %d mesh eligible(s))\n",
							  (int)g.frames, g.capPath, ok ? "ecrite" : "ECHEC", g.rtW, g.rtH, (int)g.eligible);
				logger.Info(m);
				g.capPath[0] = '\0';
			}

			const float64 nowNs = NkChrono::Now().nanoseconds;
			float32 dt = g.lastNs > 0.0 ? (float32)((nowNs - g.lastNs) / 1.0e9) : (1.f / 60.f);
			g.lastNs = nowNs;
			if (dt <= 0.f || dt > 0.25f)
				dt = 1.f / 60.f;

			// 1. Setup par frame que NkRenderer::BeginFrame ferait (cf. en-tete).
			// ⚠️ TROISIEME chose, et elle manquait : les reconstructions du render
			// graph sont DIFFEREES (NkRendererImpl.cpp l.1518, « reconstruire en
			// pleine frame est le piege du resize »), et c'est BeginFrame qui les
			// vide. Un hote qui possede sa frame ne l'appelle jamais : un
			// SetRenderSizeOverride ou un changement de passes reste alors EN
			// ATTENTE pour toujours, et le graphe tourne avec des cibles d'une
			// autre taille. A l'aplomb de la frame, comme l'original.
			g.r3->FlushGraphRebuilds();
			r3d->ResetFrame();
			if (auto *mc = g.r3->GetMaterialCollection())
				mc->Upload();

			// 2. La camera d'orbite -> l'entite camera ; puis les matrices monde.
			UpdateCameraEntity();
			g.transforms.Execute(*g.world, dt);

			// 3. Le pont ECS : BeginScene + Submit. PAS de Flush (SetOwnsFlush(false)).
			g.eligible = CountEligible(*g.world, g.nomsDessines, sizeof(g.nomsDessines));
			g.render.SetCommandBuffer(cmd);
			g.render.Execute(*g.world, dt);

			// ── LA SELECTION SE VOIT, ET SANS UNE PASSE DE PLUS ───────────────
			// MESURE, et c'est ce qui a decide du procede : les passes
			// `SelectionMask` et `SelectionOutline` sont DEJA dans le graphe et
			// s'executent DEJA a chaque image (releve dans le journal :
			// « pass 'SelectionMask' enabled=true hasExecute=true »). Elles ne
			// peignent rien tant que rien ne leur est soumis — le masque est vide
			// (NkRender3D.h l.958-961). Les alimenter n'ajoute donc AUCUNE passe :
			// le cout est deja paye, on cesse simplement de le gaspiller.
			// L'autre procede envisage — reteinter l'objet — aurait coute zero lui
			// aussi, mais il CHANGE la couleur de l'objet : on ne verrait plus sa
			// matiere. Le lisere entoure sans repeindre. A cout egal, il informe
			// plus et ment moins.
			//
			// On lit `g.selection`, l'objet que l'Outliner et Details lisent aussi.
			// La file est videe par `BeginScene` : il faut donc resoumettre a
			// chaque image, ce que ce bloc fait.
			g.selSoumises = 0;
			if (g.selection && g.selection->HasSelection()) {
				NkMeshSystem *ms = g.r3->GetMeshSystem();
				const ecs::NkEntityId primaire = g.selection->Primary();
				for (const ecs::NkEntityId id : g.selection->All()) {
					if (!id.IsValid() || !g.world->IsAlive(id))
						continue;
					// MEME PREDICAT que le rendu : on n'entoure que ce qui est
					// dessine. Entourer un objet invisible dessinerait un lisere
					// autour de rien.
					if (g.world->Has<ecs::NkInactive>(id))
						continue;
					const ecs::NkTransform *tf = g.world->Get<ecs::NkTransform>(id);
					const ecs::NkMeshComponent *mc = g.world->Get<ecs::NkMeshComponent>(id);
					if (!tf || !mc || !mc->visible)
						continue;
					NkMeshHandle h{mc->meshHandle};
					if (!h.IsValid() || !ms)
						continue;
					NkDrawCall3D dc;
					dc.mesh = h;
					dc.transform = tf->worldMatrix;
					dc.aabb = ms->GetBounds(h);
					r3d->SubmitSelection(dc, id == primaire);
					++g.selSoumises;
				}
			}

			// ── CONTROLE POSITIF INTERNE (--viewport-controle) ────────────────
			// Un cube soumis A LA MAIN, ici, entre le BeginScene du pont ECS (qui
			// n'a pas flushe : SetOwnsFlush(false)) et le graphe. S'il apparait et
			// que le cube ECS n'apparait pas, le defaut est dans le pont ECS ; si
			// aucun des deux n'apparait, il est dans le montage de l'hote. Sans ce
			// partage, un viewport uniforme ne designe aucun coupable.
			if (g.controle && g.cube.IsValid()) {
				NkDrawCall3D dc;
				dc.mesh = g.cube;
				dc.transform = NkMat4f::Translate(NkVec3f{2.5f, 0.f, 0.f});
				dc.tint = {1.f, 0.35f, 0.05f}; // orange Rihen : impossible a confondre
				dc.roughness = 0.6f;
				dc.aabb = NkAABB{{1.5f, -1.f, -1.f}, {3.5f, 1.f, 1.f}};
				r3d->Submit(dc);
			}

			// 4. Le graphe ouvre ses passes, appelle Flush dans la passe Geometry,
			//    et ecrit le resultat final dans notre cible.
			//
			// ── LE SEUL COMPTEUR VIVANT DE CET HOTE ──────────────────────────
			// `NkRendererStats.drawCalls` est MUET ici : il est ecrit par
			// `EndFrame()` (NkRendererImpl.cpp l.1588-1595), que l'editeur
			// n'appelle jamais — il rend 0 en permanence, et s'en servir comme
			// temoin reviendrait a mesurer son propre silence. Le compteur du
			// COMMAND BUFFER, lui, est incremente par le tampon a chaque appel de
			// dessin (NkICommandBuffer.h l.188-201) : c'est le seul point par ou
			// TOUT dessin passe. On le lit AVANT et APRES, et la difference dit si
			// la chaine a dessine quoi que ce soit — et combien.
			const uint32 dcAvant = cmd->Stats().drawCalls;
			const uint32 triAvant = cmd->Stats().triangles;
			if (auto *graph = g.r3->GetRenderGraph())
				graph->Execute(cmd);
			g.dcGraphe = cmd->Stats().drawCalls - dcAvant;
			g.triGraphe = cmd->Stats().triangles - triAvant;

			++g.frames;
			// Un temoin PERIODIQUE, pas un par image : sans lui, un viewport
			// silencieux et un viewport mort se ressemblent dans le journal.
			if (g.frames == 1 || g.frames % 60 == 0) {
				// Les statistiques du RENDERER, pas les miennes : `eligible` dit ce
				// que le monde ECS offre, `drawCalls` dit ce que le GPU a recu. Les
				// deux ensemble separent « rien a dessiner » de « dessine et
				// invisible » — que rien d'autre ne distingue a l'ecran.
				const NkRendererStats &st = g.r3->GetStats();
				// LE compteur qui separe « rien soumis » de « soumis puis ecarte » :
				// `NkRender3D::Submit` (NkRender3D.cpp l.1703-1707) incremente
				// opaqueSubmitted, teste `mCtx.camera.IsAABBVisible(dc.aabb)` et
				// SORT avant d'empiler. Un objet ecarte la n'atteint jamais mOpaque,
				// donc jamais Flush, donc jamais un pixel — et rien ne le dit.
				const NkRender3D::NkCullStats &cs = r3d->GetCullStats();
				const ecs::NkTransform *ctf =
					g.camId.IsValid() ? g.world->Get<ecs::NkTransform>(g.camId) : nullptr;
				const NkVec3f eye = ctf ? ctf->GetWorldPosition() : NkVec3f{0.f, 0.f, 0.f};
				// OU REGARDE-T-ELLE VRAIMENT ? `NkRenderSystem::UpdateActiveCamera`
				// (l.79-84) construit sa cible par `pos + tf.GetWorldForward()`. Si
				// cette avant pointe a l'OPPOSE de la scene, la camera regarde le
				// vide : tout est ecarte par le frustum, et rien, nulle part, ne le
				// dit. On l'imprime a cote de l'avant ATTENDUE (vers la cible
				// d'orbite) au lieu de la deduire.
				const NkVec3f fwd = ctf ? ctf->GetWorldForward() : NkVec3f{0.f, 0.f, 0.f};
				NkVec3f att{g.target.x - eye.x, g.target.y - eye.y, g.target.z - eye.z};
				{
					const float32 n = std::sqrt(att.x * att.x + att.y * att.y + att.z * att.z);
					if (n > 1e-6f) {
						att.x /= n;
						att.y /= n;
						att.z /= n;
					}
				}
				const float32 accord = fwd.x * att.x + fwd.y * att.y + fwd.z * att.z;
				// ⚠️ UNE SEULE LIGNE, EN TOUTES LETTRES. C'est celle que Rodolf lira
				// pour nous dire ce qui se passe chez lui : elle doit se suffire, sans
				// qu'il ait a connaitre le vocabulaire du moteur.
				//   eligibles = ce que le monde offre au rendu
				//   soumises  = ce qui est parti vers la file de dessin
				//   ecartees  = ce que le tronc de vue a rejete AVANT cette file
				// Si eligibles vaut 0, le defaut est dans la scene ; si soumises egale
				// ecartees, la camera regarde ailleurs ; si les deux sont bons et que
				// rien ne se voit, le defaut est APRES la soumission.
				char m[1100];
				std::snprintf(m, sizeof(m),
							  "[Nogee/Viewport3D] image %d | %d entite(s) eligible(s) au rendu, %u "
							  "soumise(s), %u ecartee(s) par le tronc de vue | MAILLAGES : %s | "
							  "vue %ux%u | camera (%.2f,%.2f,%.2f) accord avant %.3f | graphe %u "
							  "dessin(s) %u triangle(s)\n",
							  (int)g.frames, (int)g.eligible, cs.opaqueSubmitted, cs.opaqueCulled,
							  g.nomsDessines[0] ? g.nomsDessines : "AUCUN", g.rtW, g.rtH, (double)eye.x,
							  (double)eye.y, (double)eye.z, (double)accord, g.dcGraphe, g.triGraphe);
				logger.Info(m);
			}
		}

		void NogeeViewport3DRegisterInto(void *guiBackend) {
			if (!g.ok || !g.rt || !guiBackend)
				return;
			auto *b = (nkentseu::nkgui::NkGuiRHIBackend *)guiBackend;
			auto *texLib = g.r3->GetTextures();
			if (!texLib)
				return;
			b->RegisterTexture(kNogeeViewportTexId, texLib->GetRHIHandle(g.rt->GetColorResult()));
		}

		// ── LES MATRICES QUI ONT DESSINE L'IMAGE, ET PAS D'AUTRES ────────────
		// On lit l'entite camera, ou `NkRenderSystem::UpdateActiveCamera` a ecrit
		// `viewProjMatrix` pendant CETTE image. Recalculer ici un view/proj
		// « equivalent » donnerait un aller-retour parfaitement vert et un rayon
		// faux a l'ecran : le temoin mesurerait sa propre arithmetique.
		namespace {
			bool CameraDeLImage(NkMat4f *viewProj, NkVec3f *position) {
				if (!g.world || !g.camId.IsValid())
					return false;
				const ecs::NkCameraComponent *cam = g.world->Get<ecs::NkCameraComponent>(g.camId);
				const ecs::NkTransform *tf = g.world->Get<ecs::NkTransform>(g.camId);
				if (!cam || !tf)
					return false;
				if (viewProj)
					*viewProj = cam->viewProjMatrix;
				if (position)
					*position = tf->GetWorldPosition();
				return true;
			}
		} // namespace

		bool NogeeViewport3DProjectToView(const float32 monde[3], float32 *vx, float32 *vy) {
			NkMat4f vp;
			if (!monde || g.vueW <= 0.f || g.vueH <= 0.f || !CameraDeLImage(&vp, nullptr))
				return false;
			const NkVec4f clip = vp * NkVec4f{monde[0], monde[1], monde[2], 1.f};
			// w <= 0 : le point est DERRIERE le plan de la camera. Diviser rendrait
			// une position d'ecran plausible et fausse — le symetrique exact du
			// defaut de camera d'hier. On refuse plutot que de rendre un chiffre.
			if (clip.w <= 1e-6f)
				return false;
			const float32 ndcX = clip.x / clip.w;
			const float32 ndcY = clip.y / clip.w;
			if (vx)
				*vx = (ndcX * 0.5f + 0.5f) * g.vueW;
			// Y INVERSE : le NDC monte, les pixels d'ecran descendent.
			if (vy)
				*vy = (1.f - (ndcY * 0.5f + 0.5f)) * g.vueH;
			return true;
		}

		bool NogeeViewport3DRayFromView(float32 vx, float32 vy, float32 origine[3], float32 direction[3]) {
			NkMat4f vp;
			NkVec3f pos;
			if (g.vueW <= 0.f || g.vueH <= 0.f || !CameraDeLImage(&vp, &pos))
				return false;
			const NkMat4f inv = vp.Inverse();
			const float32 ndcX = 2.f * (vx / g.vueW) - 1.f;
			const float32 ndcY = 1.f - 2.f * (vy / g.vueH); // meme inversion, en sens inverse
			NkVec4f p = inv * NkVec4f{ndcX, ndcY, 0.f, 1.f};
			if (p.w > -1e-9f && p.w < 1e-9f)
				return false;
			p.x /= p.w;
			p.y /= p.w;
			p.z /= p.w;
			NkVec3f d{p.x - pos.x, p.y - pos.y, p.z - pos.z};
			const float32 n = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
			if (n < 1e-9f)
				return false;
			d.x /= n;
			d.y /= n;
			d.z /= n;
			if (origine) {
				origine[0] = pos.x;
				origine[1] = pos.y;
				origine[2] = pos.z;
			}
			if (direction) {
				direction[0] = d.x;
				direction[1] = d.y;
				direction[2] = d.z;
			}
			return true;
		}

		void NogeeViewport3DSetView(float32 offX, float32 offY, float32 w, float32 h, bool survol) {
			g.vueX = offX;
			g.vueY = offY;
			g.vueW = w;
			g.vueH = h;
			g.vueSurvol = survol;
		}

		// ── SELECTION : LES DEUX TESTS GEOMETRIQUES ──────────────────────────
		namespace {

			// Rayon / boite alignee, methode des dalles. Rend la distance d'ENTREE
			// (0 si l'origine est deja dedans) ; faux si le rayon la manque ou si
			// elle est entierement derriere.
			bool RayonContreBoite(const NkVec3f &o, const NkVec3f &d, const NkVec3f &bmin, const NkVec3f &bmax,
								  float32 *tEntree) {
				float32 t0 = 0.f, t1 = 1e30f;
				const float32 od[3] = {o.x, o.y, o.z};
				const float32 dd[3] = {d.x, d.y, d.z};
				const float32 mn[3] = {bmin.x, bmin.y, bmin.z};
				const float32 mx[3] = {bmax.x, bmax.y, bmax.z};
				for (int32 i = 0; i < 3; ++i) {
					// Rayon parallele a cette paire de plans : dedans ou jamais.
					if (dd[i] > -1e-9f && dd[i] < 1e-9f) {
						if (od[i] < mn[i] || od[i] > mx[i])
							return false;
						continue;
					}
					const float32 inv = 1.f / dd[i];
					float32 ta = (mn[i] - od[i]) * inv;
					float32 tb = (mx[i] - od[i]) * inv;
					if (ta > tb) {
						const float32 tmp = ta;
						ta = tb;
						tb = tmp;
					}
					if (ta > t0)
						t0 = ta;
					if (tb < t1)
						t1 = tb;
					if (t0 > t1)
						return false;
				}
				if (tEntree)
					*tEntree = t0;
				return true;
			}

			// Rayon / triangle, Moller-Trumbore. Faces des DEUX cotes : dans un
			// editeur on selectionne aussi ce qu'on regarde par l'interieur.
			bool RayonContreTriangle(const NkVec3f &o, const NkVec3f &d, const NkVec3f &a, const NkVec3f &b,
									 const NkVec3f &c, float32 *tOut) {
				const NkVec3f e1{b.x - a.x, b.y - a.y, b.z - a.z};
				const NkVec3f e2{c.x - a.x, c.y - a.y, c.z - a.z};
				const NkVec3f p = d.Cross(e2);
				const float32 det = e1.Dot(p);
				if (det > -1e-9f && det < 1e-9f)
					return false; // rayon dans le plan du triangle
				const float32 invDet = 1.f / det;
				const NkVec3f s{o.x - a.x, o.y - a.y, o.z - a.z};
				const float32 u = s.Dot(p) * invDet;
				if (u < 0.f || u > 1.f)
					return false;
				const NkVec3f q = s.Cross(e1);
				const float32 v = d.Dot(q) * invDet;
				if (v < 0.f || u + v > 1.f)
					return false;
				const float32 t = e2.Dot(q) * invDet;
				if (t <= 1e-5f)
					return false; // derriere l'origine du rayon
				if (tOut)
					*tOut = t;
				return true;
			}

			// Boite MONDE d'une entite. ⚠️ On transforme les HUIT COINS par la
			// matrice monde, au lieu d'ajouter la position comme le fait
			// `NkRenderSystem::SubmitMeshes` (l.160-163). Sa version ignore la
			// rotation et l'echelle : elle suffit a un culling prudent — une boite
			// trop petite ne fait que garder un objet de trop — mais elle designerait
			// le mauvais objet au pointage. Le culling a le droit d'etre approximatif,
			// la selection n'a pas ce droit.
			void BoiteMonde(const NkAABB &locale, const NkMat4f &monde, NkVec3f *bmin, NkVec3f *bmax) {
				NkVec3f mn{1e30f, 1e30f, 1e30f}, mx{-1e30f, -1e30f, -1e30f};
				for (int32 i = 0; i < 8; ++i) {
					const NkVec3f coin{(i & 1) ? locale.max.x : locale.min.x, (i & 2) ? locale.max.y : locale.min.y,
									   (i & 4) ? locale.max.z : locale.min.z};
					const NkVec3f p = monde * coin;
					if (p.x < mn.x) mn.x = p.x;
					if (p.y < mn.y) mn.y = p.y;
					if (p.z < mn.z) mn.z = p.z;
					if (p.x > mx.x) mx.x = p.x;
					if (p.y > mx.y) mx.y = p.y;
					if (p.z > mx.z) mx.z = p.z;
				}
				*bmin = mn;
				*bmax = mx;
			}

		} // namespace

		bool NogeeViewport3DPick(float32 vx, float32 vy, nk_uint64 *entite, float32 *distance, int32 *precision) {
			if (entite)
				*entite = 0ull;
			if (distance)
				*distance = 0.f;
			if (precision)
				*precision = 0;
			if (!g.world || !g.ok)
				return false;
			float32 ro[3], rd[3];
			if (!NogeeViewport3DRayFromView(vx, vy, ro, rd))
				return false;
			NkMeshSystem *meshSys = g.r3 ? g.r3->GetMeshSystem() : nullptr;
			if (!meshSys)
				return false;

			const NkVec3f o{ro[0], ro[1], ro[2]};
			const NkVec3f d{rd[0], rd[1], rd[2]};

			ecs::NkEntityId gagnante{};
			float32 meilleure = 1e30f;
			int32 precisionGagnante = 0;

			// MEME PREDICAT que SubmitMeshes : on ne selectionne que ce qui est
			// dessine. Un objet invisible qui repondrait au clic serait un fantome.
			g.world->Query<ecs::NkTransform, ecs::NkMeshComponent, ecs::NkMaterialComponent>().ForEach(
				[&](ecs::NkEntityId id, const ecs::NkTransform &tf, const ecs::NkMeshComponent &mc,
					const ecs::NkMaterialComponent &) {
					if (g.world->Has<ecs::NkInactive>(id))
						return;
					if (!mc.visible)
						return;
					NkMeshHandle h{mc.meshHandle};
					if (!h.IsValid())
						return;

					NkVec3f bmin, bmax;
					BoiteMonde(meshSys->GetBounds(h), tf.worldMatrix, &bmin, &bmax);
					float32 tBoite = 0.f;
					if (!RayonContreBoite(o, d, bmin, bmax, &tBoite))
						return;
					// Deja battu par un objet plus proche : inutile d'aller au triangle.
					if (tBoite >= meilleure)
						return;

					// ── AFFINAGE AU TRIANGLE, quand la geometrie CPU existe ──
					// On amene le RAYON dans l'espace local plutot que les sommets
					// dans le monde : une inversion de matrice contre N transformations.
					float32 tMeilleurLocal = 1e30f;
					bool touche = false;
					if (meshSys->HasCPUData(h)) {
						const uint8 *verts = (const uint8 *)meshSys->GetVertices(h);
						const uint32 *idx = meshSys->GetIndices(h);
						const uint32 nIdx = meshSys->GetIndexCount(h);
						const uint32 stride = meshSys->GetVertexStride(h);
						if (verts && idx && nIdx >= 3 && stride >= sizeof(float32) * 3) {
							const NkMat4f versLocal = tf.worldMatrix.Inverse();
							// Un POINT se transforme avec la translation, une DIRECTION
							// non : on transforme deux points et on soustrait. C'est le
							// piege classique, et il ne se voit que sur un objet
							// deplace loin de l'origine.
							const NkVec3f oL = versLocal * o;
							const NkVec3f o2L = versLocal * NkVec3f{o.x + d.x, o.y + d.y, o.z + d.z};
							NkVec3f dL{o2L.x - oL.x, o2L.y - oL.y, o2L.z - oL.z};
							const float32 nL = std::sqrt(dL.x * dL.x + dL.y * dL.y + dL.z * dL.z);
							if (nL > 1e-9f) {
								const float32 echelle = 1.f / nL; // t local -> t monde
								dL.x /= nL;
								dL.y /= nL;
								dL.z /= nL;
								for (uint32 i = 0; i + 2 < nIdx; i += 3) {
									const float32 *pa = (const float32 *)(verts + (size_t)idx[i] * stride);
									const float32 *pb = (const float32 *)(verts + (size_t)idx[i + 1] * stride);
									const float32 *pc = (const float32 *)(verts + (size_t)idx[i + 2] * stride);
									float32 tt = 0.f;
									if (RayonContreTriangle(oL, dL, NkVec3f{pa[0], pa[1], pa[2]},
															NkVec3f{pb[0], pb[1], pb[2]},
															NkVec3f{pc[0], pc[1], pc[2]}, &tt)) {
										const float32 tMonde = tt * echelle;
										if (tMonde < tMeilleurLocal) {
											tMeilleurLocal = tMonde;
											touche = true;
										}
									}
								}
							}
						}
					}

					// Le triangle a tranche : soit il touche (distance exacte), soit
					// il ne touche PAS et l'objet est ecarte — la boite l'avait dit a
					// tort. Sans geometrie CPU, la boite fait foi et on le DIT.
					float32 tRetenu;
					int32 prec;
					if (meshSys->HasCPUData(h)) {
						if (!touche)
							return;
						tRetenu = tMeilleurLocal;
						prec = 1;
					} else {
						tRetenu = tBoite;
						prec = 0;
					}
					if (tRetenu < meilleure) {
						meilleure = tRetenu;
						gagnante = id;
						precisionGagnante = prec;
					}
				});

			if (!gagnante.IsValid())
				return false;
			if (entite)
				*entite = gagnante.Pack();
			if (distance)
				*distance = meilleure;
			if (precision)
				*precision = precisionGagnante;
			return true;
		}

		void NogeeViewport3DViewRect(float32 *x, float32 *y, float32 *w, float32 *h) {
			if (x)
				*x = g.vueX;
			if (y)
				*y = g.vueY;
			if (w)
				*w = g.vueW;
			if (h)
				*h = g.vueH;
		}

		bool NogeeViewport3DMouseToView(float32 winX, float32 winY, float32 *outX, float32 *outY) {
			const float32 x = winX - g.vueX;
			const float32 y = winY - g.vueY;
			if (outX)
				*outX = x;
			if (outY)
				*outY = y;
			// Le dedans est STRICT : un point sur le bord droit ou bas appartient
			// deja au pixel suivant, qui n'est plus la vue.
			return (g.vueW > 0.f && g.vueH > 0.f) && x >= 0.f && y >= 0.f && x < g.vueW && y < g.vueH;
		}

		void NogeeViewport3DOrbit(float32 dYawDeg, float32 dPitchDeg, float32 dZoom) {
			g.yawDeg += dYawDeg;
			g.pitchDeg += dPitchDeg;
			if (g.pitchDeg > 85.f)
				g.pitchDeg = 85.f;
			if (g.pitchDeg < -85.f)
				g.pitchDeg = -85.f;
			g.dist *= (1.f + dZoom);
			if (g.dist < 0.5f)
				g.dist = 0.5f;
			if (g.dist > 500.f)
				g.dist = 500.f;
		}

		void NogeeViewport3DSetOrbit(float32 yawDeg, float32 pitchDeg) {
			g.yawDeg = yawDeg;
			g.pitchDeg = pitchDeg;
			if (g.pitchDeg > 85.f)
				g.pitchDeg = 85.f;
			if (g.pitchDeg < -85.f)
				g.pitchDeg = -85.f;
		}

		void NogeeViewport3DGetOrbit(float32 *yawDeg, float32 *pitchDeg) {
			if (yawDeg)
				*yawDeg = g.yawDeg;
			if (pitchDeg)
				*pitchDeg = g.pitchDeg;
		}

		void NogeeViewport3DControle(bool on) {
			g.controle = on;
		}

		void NogeeViewport3DCaptureAt(int32 numeroImage, const char *chemin) {
			if (!chemin || !chemin[0])
				return;
			std::snprintf(g.capPath, sizeof(g.capPath), "%s", chemin);
			g.capFrame = numeroImage < 1 ? 1 : numeroImage;
		}

		int32 NogeeViewport3DDrawCount() {
			return g.eligible;
		}

		int32 NogeeViewport3DFrameCount() {
			return g.frames;
		}

	} // namespace noge
} // namespace nkentseu
