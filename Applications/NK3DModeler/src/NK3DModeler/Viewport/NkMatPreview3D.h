#pragma once
// -----------------------------------------------------------------------------
// @File    NkMatPreview3D.h
// @Brief   APERCU DE MATERIAU RENDU PAR LE MOTEUR : une mini-scene de studio
//          (sol + objet) rendue hors ecran avec le VRAI pipeline, et publiee
//          comme une image d'interface.
//
// POURQUOI REMPLACER LE RENDU ANALYTIQUE
//   L'apercu etait calcule par formules, pixel par pixel (HostMatPreviewRender).
//   Assez juste pour un PBR ordinaire, mais il ne connaissait QUE cela : le
//   verre, l'emissif et le toon s'y affichaient comme des plastiques, puisque
//   aucun de leurs shaders n'etait execute. Passer par le moteur, c'est montrer
//   la matiere telle qu'elle sera rendue -- et non telle qu'une approximation
//   l'imagine. « Il faut aussi les vrais modeles » (Rihen, 13 aout 2026).
//
// COMMENT IL S'INSERE DANS LA FRAME
//   L'editeur possede la frame device et le command buffer ; ce module n'ouvre
//   donc PAS de frame a lui. Il rejoue ce qu'un BeginFrame ferait pour son
//   propre renderer (reconstruction du graphe, reset du pool d'UBO, upload des
//   materiaux), soumet ses deux draw calls, puis execute son graphe DANS le
//   command buffer qu'on lui passe. C'est exactement la mecanique de la vue 3D,
//   et elle impose une contrainte : tout cela doit se faire AVANT que la passe
//   backbuffer ne s'ouvre, une passe de rendu ne pouvant pas en contenir une
//   autre.
//
// CE QU'IL NE FAIT PAS
//   Il ne rend qu'UNE image par frame, celle du grand apercu du panneau. Les
//   vignettes des cartes du navigateur ne passent pas par ici : elles recoivent
//   une CAPTURE de cette meme image, figee au moment ou le materiau est
//   enregistre (choix de Rihen). Rendre soixante-quatre scenes pour des
//   vignettes de quarante pixels couterait cher pour un gain invisible.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkRenderGraph.h"
#include "NKRenderer/Core/NkSceneContext.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Materials/NkMaterialCollection.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKGui/NkGuiRHIBackend.h"
#include "NKLogger/NkLog.h"
// Pour l'identifiant de texture, partage avec le panneau (qui, lui, ne connait
// pas NKRenderer). Ce header n'apporte aucun type NKRenderer -- c'est sa regle.
#include "NK3DModeler/Viewport/NkDemo3DHost.h"

namespace nkentseu {
	namespace nk3d {
		namespace matprev {

			using namespace nkentseu::renderer;

			/// Identifiant de la texture d'apercu dans le backend d'interface.
			/// Declare dans NkDemo3DHost.h, que le PANNEAU inclut deja : il pose
			/// l'image sans avoir a connaitre NKRenderer. Distinct des 4400+
			/// (vignettes analytiques des cartes), qui coexistent tant que la
			/// capture a l'enregistrement n'est pas en place.
			constexpr uint32 kPreviewTexId = demo::kNkMatPreviewTexId;

			/// Les sept formes, dans l'ordre des valeurs serialisees `prevShape`.
			/// Les trois premieres viennent du moteur (NkMeshSystem en fournit deja
			/// les primitives) ; les quatre autres sont des modeles charges.
			enum class NkPrevMesh : int32 {
				Plan = 0,
				Sphere = 1,
				Cube = 2,
				Liquide = 3,
				Cheveux = 4,
				Tissu = 5,
				Tete = 6,
				Count = 7
			};

			struct NkMatPreviewState {
					NkRenderer *rd = nullptr;
					NkOffscreenTarget *rt = nullptr;
					uint32 w = 260, h = 150;
					bool tried = false;
					bool ok = false;
					const char *err = nullptr;
					/// Les sept maillages. Ceux qui restent invalides sont remplaces
					/// par la sphere : mieux vaut un apercu approximatif qu'un trou
					/// noir, et le journal dit lequel manque.
					NkMeshHandle mesh[(int32)NkPrevMesh::Count];
					NkMeshHandle meshSol;
			};

			inline NkMatPreviewState &St() {
				static NkMatPreviewState s;
				return s;
			}

			// ── CREATION ────────────────────────────────────────────────────────
			// Un SECOND renderer sur le MEME device : c'est deja ce que fait la vue
			// 3D, et c'est la seule facon d'avoir une scene independante sans ouvrir
			// une seconde pile GPU dans la fenetre -- ce que le depot interdit.
			inline bool Init(NkIDevice *device, uint32 w, uint32 h) {
				NkMatPreviewState &s = St();
				if (s.tried)
					return s.ok;
				s.tried = true;
				if (!device || !device->IsValid()) {
					s.err = "device partage absent";
					NkLog::Instance().Info("[apercu] ECHEC : {0}", s.err);
					return false;
				}
				s.w = w < 32u ? 32u : w;
				s.h = h < 32u ? 32u : h;

				NkRendererConfig cfg = NkRendererConfig::ForGame(device->GetApi(), s.w, s.h);
				// UNE SEULE CASCADE D'OMBRE : la scene tient dans trois unites.
				cfg.shadow.cascadeCount = 1;
				cfg.Enable(NK_SS_OFFSCREEN);
				// Ni SSAO ni bloom : un apercu montre une MATIERE. Un halo ou un
				// depot sombre au pied de l'objet changerait la lecture de la
				// rugosite, ce qui est precisement ce qu'on vient y juger.
				cfg.postProcess.ssao = false;
				cfg.postProcess.bloom = false;
				s.rd = NkRenderer::Create(device, cfg);
				if (!s.rd) {
					s.err = "creation du renderer d'apercu refusee";
					NkLog::Instance().Info("[apercu] ECHEC : {0}", s.err);
					return false;
				}

				NkOffscreenDesc od;
				od.width = s.w;
				od.height = s.h;
				od.hdr = false;
				od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
				od.hasDepth = true;
				od.readable = true;
				// readback : c'est par la que la vignette d'une carte sera capturee
				// a l'enregistrement du materiau.
				od.readback = true;
				od.name = "NK3DModelerMatPreview";
				s.rt = s.rd->CreateOffscreen(od);
				if (!s.rt || !s.rt->IsValid()) {
					s.err = "cible hors ecran d'apercu refusee";
					NkLog::Instance().Info("[apercu] ECHEC : {0}", s.err);
					return false;
				}
				// Sans l'override, le graphe rendrait a la taille de la FENETRE dans
				// une cible bien plus petite.
				s.rd->SetRenderSizeOverride(s.w, s.h);
				if (auto *texLib = s.rd->GetTextures())
					s.rd->SetFinalColorTarget(texLib->GetRHIHandle(s.rt->GetColorResult()));

				// ── LES FORMES ──────────────────────────────────────────────────
				// Les trois primitives sont celles du moteur : les livrer en
				// fichiers ferait deux verites sur ce qu'est un cube.
				if (auto *ms = s.rd->GetMeshSystem()) {
					s.meshSol = ms->GetPlane();
					s.mesh[(int32)NkPrevMesh::Plan] = ms->GetPlane();
					s.mesh[(int32)NkPrevMesh::Sphere] = ms->GetSphere();
					s.mesh[(int32)NkPrevMesh::Cube] = ms->GetCube();
				}
				s.ok = true;
				NkLog::Instance().Info(
					"[apercu] pret : {0}x{1}, meshes plan={2} sphere={3} cube={4}", s.w, s.h,
					s.mesh[(int32)NkPrevMesh::Plan].IsValid() ? 1 : 0,
					s.mesh[(int32)NkPrevMesh::Sphere].IsValid() ? 1 : 0,
					s.mesh[(int32)NkPrevMesh::Cube].IsValid() ? 1 : 0);
				return true;
			}

			/// Redimensionne la cible si le panneau a change de largeur. Rendre a la
			/// taille exacte d'affichage evite l'etirement ET le flou.
			inline void EnsureSize(uint32 w, uint32 h) {
				NkMatPreviewState &s = St();
				if (!s.ok || !s.rt)
					return;
				if (w < 32u)
					w = 32u;
				if (h < 32u)
					h = 32u;
				if (w == s.w && h == s.h)
					return;
				if (!s.rt->Resize(w, h))
					return;
				s.w = w;
				s.h = h;
				s.rd->SetRenderSizeOverride(w, h);
				if (auto *texLib = s.rd->GetTextures())
					s.rd->SetFinalColorTarget(texLib->GetRHIHandle(s.rt->GetColorResult()));
			}

			// ── UNE IMAGE ───────────────────────────────────────────────────────
			// `mat` est l'INSTANCE MOTEUR du materiau du projet : c'est elle qui
			// porte le type (verre, toon, emissif...) et donc le shader. Passer par
			// elle est tout l'interet de ce module -- les surcharges par draw call
			// ne decrivent qu'un PBR.
			inline void RenderOne(NkICommandBuffer *cmd, NkMatInstHandle mat, int32 shape,
								  uint32 w, uint32 h, float32 time) {
				NkMatPreviewState &s = St();
				if (!s.ok || !cmd || !s.rd)
					return;
				EnsureSize(w, h);
				auto *r3d = s.rd->GetRender3D();
				if (!r3d) {
					static bool sDit = false;
					if (!sDit) {
						sDit = true;
						NkLog::Instance().Info("[apercu] pas de Render3D");
					}
					return;
				}
				{
					static int32 sVu = -2;
					if (sVu != shape) {
						sVu = shape;
						NkLog::Instance().Info("[apercu] rendu forme={0} taille={1}x{2}", shape,
											   s.w, s.h);
					}
				}
				// L'editeur possede la frame : on rejoue ce que ferait un BeginFrame
				// pour NOTRE renderer, pas un de plus.
				s.rd->FlushGraphRebuilds();
				r3d->ResetFrame();
				if (auto *mc = s.rd->GetMaterialCollection())
					mc->Upload();

				// ── CAMERA DE STUDIO ────────────────────────────────────────────
				// Fixe et legerement plongeante : on voit le dessus de l'objet et sa
				// pose sur le sol. L'aspect suit la LARGEUR de l'image -- c'est ce
				// qui elargit le champ sans grossir l'objet quand le panneau
				// s'agrandit.
				NkCamera3DData camData;
				camData.up = {0.f, 1.f, 0.f};
				camData.fovY = 32.f;
				camData.aspect = (float32)s.w / (float32)s.h;
				camData.nearPlane = 0.05f;
				camData.farPlane = 40.f;
				NkCamera3D cam(camData);
				cam.SetPosition({0.f, 1.35f, 3.6f});
				cam.SetTarget({0.f, 0.25f, 0.f});

				NkSceneContext sctx;
				sctx.camera = cam;
				sctx.time = time;
				sctx.ambientIntensity = 0.22f;
				// Deux lumieres : une CLE qui sculpte, un APPOINT froid a l'oppose
				// pour que la face d'ombre garde de la matiere. Sans l'appoint, une
				// moitie de la sphere est noire et la rugosite ne s'y lit plus.
				NkLightDesc key;
				key.type = NkLightType::NK_DIRECTIONAL;
				key.direction = {-0.45f, -0.78f, -0.44f};
				key.color = {1.f, 0.97f, 0.92f};
				key.intensity = 3.1f;
				sctx.lights.PushBack(key);
				NkLightDesc fill;
				fill.type = NkLightType::NK_DIRECTIONAL;
				fill.direction = {0.62f, -0.35f, 0.70f};
				fill.color = {0.72f, 0.80f, 1.f};
				fill.intensity = 0.9f;
				sctx.lights.PushBack(fill);

				r3d->BeginScene(sctx);

				// ── LE SOL ──────────────────────────────────────────────────────
				// Il recoit les ombres sans en projeter : un sol qui alimente la
				// cascade la gaspille pour une surface qu'on ne voit qu'a plat.
				if (s.meshSol.IsValid()) {
					NkDrawCall3D dc;
					dc.mesh = s.meshSol;
					dc.transform = NkMat4f::Scale({6.f, 1.f, 6.f});
					dc.aabb = {{-6.f, -0.01f, -6.f}, {6.f, 0.01f, 6.f}};
					dc.tint = {0.19f, 0.19f, 0.20f};
					dc.roughness = 0.94f;
					dc.metallic = 0.f;
					dc.castShadow = false;
					r3d->Submit(dc);
				}

				// ── L'OBJET ─────────────────────────────────────────────────────
				const int32 si = (shape < 0 || shape >= (int32)NkPrevMesh::Count) ? 1 : shape;
				NkMeshHandle mh = s.mesh[si];
				if (!mh.IsValid())
					mh = s.mesh[(int32)NkPrevMesh::Sphere]; // repli : jamais de trou noir
				if (mh.IsValid()) {
					NkDrawCall3D dc;
					dc.mesh = mh;
					dc.material = mat; // LE VRAI MATERIAU, donc le vrai shader
					// Le PLAN se couche et s'elargit ; les autres gardent leur echelle.
					dc.transform = (si == (int32)NkPrevMesh::Plan)
									   ? NkMat4f::Translate({0.f, 0.02f, 0.f}) * NkMat4f::Scale({1.5f, 1.f, 1.5f})
									   : NkMat4f::Translate({0.f, 0.5f, 0.f});
					dc.aabb = {{-1.6f, -0.1f, -1.6f}, {1.6f, 1.6f, 1.6f}};
					dc.castShadow = true;
					dc.receiveShadow = true;
					r3d->Submit(dc);
				}

				// Presenter = executer le graphe DANS le command buffer de l'editeur.
				if (auto *graph = s.rd->GetRenderGraph())
					graph->Execute(cmd);
			}

			/// Publie la texture d'apercu aupres du backend d'interface, sous
			/// `kPreviewTexId`. A appeler apres le rendu, comme la vue 3D.
			inline void RegisterInto(void *guiBackend) {
				NkMatPreviewState &s = St();
				if (!s.ok || !s.rt || !guiBackend)
					return;
				auto *b = (nkentseu::nkgui::NkGuiRHIBackend *)guiBackend;
				if (auto *texLib = s.rd->GetTextures())
					b->RegisterTexture(kPreviewTexId, texLib->GetRHIHandle(s.rt->GetColorResult()));
			}

			/// Lit la derniere image rendue (pour la vignette figee d'une carte).
			inline bool Readback(uint8 *dst) {
				NkMatPreviewState &s = St();
				return s.ok && s.rt && dst && s.rt->ReadbackPixels(dst, s.w * 4u);
			}

		} // namespace matprev
	} // namespace nk3d
} // namespace nkentseu
