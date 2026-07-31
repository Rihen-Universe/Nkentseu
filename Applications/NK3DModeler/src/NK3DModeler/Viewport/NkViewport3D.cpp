// -----------------------------------------------------------------------------
// @File    NkViewport3D.cpp
// @Brief   SEULE unite de compilation qui voit NKRenderer.
// @License Proprietary - Free to use and modify
//
// Elle est seule pour une raison mecanique, pas par gout de l'isolation :
// NKRenderer et NKCanvas declarent tous deux `renderer::NkBlendMode` et
// `renderer::NkVertex2D`. Les melanger dans une unite ne compile pas. Le reste
// de NK3DModeler ne voit donc que NkViewport3D.h et ses `void *`.
//
// MODELE : Applications/NkAnimaEditor/src/NkAnimaEditor/AnimBridge.cpp, dont la
// composition « scene 3D hors ecran + interface NKGui, meme fenetre, meme
// device, sans relecture CPU » est validee a l'ecran. On la reprend telle
// quelle plutot que d'en inventer une seconde.
//
// LE POINT DELICAT, et c'est le meme que chez NkAnimaEditor : NOUS NE PILOTONS
// PAS LA FRAME. L'editeur ouvre la frame device et le command buffer ; nous ne
// faisons qu'ajouter nos passes dessus. Donc pas de BeginFrame, pas de
// EndFrame, pas de Present ici -- mais il faut rejouer a la main les deux
// choses que NkRenderer::BeginFrame ferait pour nous : ResetFrame() et l'envoi
// des materiaux.
// -----------------------------------------------------------------------------

#include "NK3DModeler/Viewport/NkViewport3D.h"

#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkRendererConfig.h"
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Core/NkRenderGraph.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Mesh/NkEditMesh.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Materials/NkMaterialCollection.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKGui/NkGuiRHIBackend.h" // Integrations/NKGui
#include "NKMath/NkMat.h"

#include <cmath>

namespace nkentseu {
	namespace nk3d {

		using namespace nkentseu::renderer;
		using math::NkMat4f;
		using math::NkVec3f;
		using math::NkVec4f;

		namespace {

			struct Viewport3D {
					// ── Pile GPU ────────────────────────────────────────────
					NkIDevice *sharedDev = nullptr;
					NkRenderer *r3 = nullptr;
					NkOffscreenTarget *rt = nullptr;
					uint32 rtW = 1280, rtH = 720;
					uint32 wantW = 1280, wantH = 720;
					bool tried = false;
					bool ok = false;
					const char *err = nullptr;

					// ── Scene ───────────────────────────────────────────────
					NkEditMesh edit;			 ///< LA source de verite : demi-aretes
					NkMeshHandle mesh;			 ///< sa triangulation, cote GPU
					bool meshOk = false;
					bool meshDirty = true;		 ///< la triangulation doit etre refaite
					NkVector<NkVertex3D> triV;	 ///< tampons reutilises d'une image a
					NkVector<uint32> triI;		 ///< l'autre : les reallouer par image
					NkVector<NkEmId> triF;		 ///< couterait plus que le rendu

					// ── Camera orbitale ─────────────────────────────────────
					NkVec3f center{0.f, 0.f, 0.f};
					float32 yaw = 0.6f;		///< radians, autour de Y
					float32 pitch = 0.45f;	///< radians, au-dessus de l'horizon
					float32 dist = 6.f;		///< distance a la cible
					float32 radius = 1.8f;	///< rayon englobant, pour le recadrage

					// ── Affichage ───────────────────────────────────────────
					int32 shading = 0;		///< 0 solide, 1 materiau, 2 rendu, 3 filaire
					int32 solidLight = 0;	///< 0 studio, 1 matcap, 2 plat
					uint32 overlays = 0x0Fu;
			};

			Viewport3D g;

			// Bits du masque de surimpressions, dans l'ordre de NkOverlayItems.
			enum : uint32 {
				kOvGrid = 1u << 0,
				kOvAxes = 1u << 1,
				kOvOutline = 1u << 2,
				kOvGizmos = 1u << 3,
				kOvNormals = 1u << 4,
				kOvStats = 1u << 5,
				kOvWire = 1u << 6,
				kOvOrigins = 1u << 7,
			};

			// ── Le cube de depart, construit en DEMI-ARETES ─────────────────
			// On ne prend PAS le cube tout fait de NkMeshSystem. Celui-la est un
			// tampon de triangles : il n'a ni faces ni aretes, donc rien a editer,
			// rien a compter, rien a selectionner. Ici la source de verite est le
			// NkEditMesh, et le tampon GPU n'en est qu'une projection -- c'est le
			// sens de la fleche pour tout le reste du modeleur.
			void BuildStartCube() {
				const float32 h = 1.f;
				NkVertex3D v[8];
				const float32 px[8] = {-h, h, h, -h, -h, h, h, -h};
				const float32 py[8] = {-h, -h, -h, -h, h, h, h, h};
				const float32 pz[8] = {-h, -h, h, h, -h, -h, h, h};
				for (uint32 i = 0; i < 8; ++i) {
					v[i] = NkVertex3D{};
					v[i].pos = {px[i], py[i], pz[i]};
					v[i].normal = {0.f, 1.f, 0.f};
					v[i].color = 0xFFFFFFFFu; // RGBA empaquete, pas un vec4
				}
				// Douze triangles, deux par face, en sens anti-horaire vu de
				// l'exterieur. `quadify` les recolle ensuite deux a deux : on
				// retrouve les six QUADS d'origine, et c'est ce qui donne 8 sommets,
				// 12 aretes et 6 faces au lieu de 8 / 18 / 12.
				static const uint32 idx[36] = {
					0, 2, 1, 0, 3, 2, // bas  (-Y)
					4, 5, 6, 4, 6, 7, // haut (+Y)
					0, 1, 5, 0, 5, 4, // avant (-Z)
					3, 7, 6, 3, 6, 2, // arriere (+Z)
					0, 4, 7, 0, 7, 3, // gauche (-X)
					1, 2, 6, 1, 6, 5, // droite (+X)
				};
				g.edit.BuildFromIndexed(v, 8, idx, 36, true);
				g.edit.RecomputeNormals();
				g.meshDirty = true;
				g.radius = 1.8f;
				g.center = {0.f, 0.f, 0.f};
			}

			// Retriangule et renvoie le resultat au GPU. Appelee seulement quand le
			// maillage a bouge : la triangulation d'un maillage dense n'est pas
			// gratuite, et rien ne change entre deux images qui ne l'editent pas.
			void SyncMesh() {
				if (!g.meshDirty || !g.r3)
					return;
				auto *meshSys = g.r3->GetMeshSystem();
				if (!meshSys)
					return;
				// TriangulateShaded, pas Triangulate : elle respecte le drapeau
				// `smooth` de chaque face. Sur un cube toutes les faces sont plates,
				// et une normale moyennee aux coins arrondirait visuellement des
				// aretes qui sont vives -- le cube aurait l'air d'un galet.
				g.edit.TriangulateShaded(g.triV, g.triI, g.triF);
				if (g.triV.Empty() || g.triI.Empty())
					return;
				if (g.meshOk) {
					// Mise a jour en place tant que la taille tient : recreer le
					// maillage a chaque edition fragmenterait la memoire GPU.
					meshSys->UpdateVertices(g.mesh, g.triV.Data(), (uint32)g.triV.Size());
					meshSys->UpdateIndices(g.mesh, g.triI.Data(), (uint32)g.triI.Size());
				} else {
					NkMeshDesc md = NkMeshDesc::Simple(renderer::NkVertexLayout::Default3D(), g.triV.Data(),
													   (uint32)g.triV.Size(), g.triI.Data(),
													   (uint32)g.triI.Size());
					md.keepCPU = true;
					md.debugName = "NK3DModeler.EditMesh";
					g.mesh = meshSys->Create(md);
					g.meshOk = g.mesh.IsValid();
				}
				g.meshDirty = false;
			}

			bool Init3D() {
				if (g.tried)
					return g.ok;
				g.tried = true;
				if (!g.sharedDev || !g.sharedDev->IsValid()) {
					g.err = "device partage absent";
					return false;
				}
				// ForEditor, pas ForGame : le preset editeur privilegie la latence et
				// la lisibilite sur la richesse de l'image. Un modeleur veut voir la
				// forme, pas un rendu de film.
				NkRendererConfig cfg = NkRendererConfig::ForEditor(g.sharedDev->GetApi(), g.wantW, g.wantH);
				cfg.Enable(NK_SS_OFFSCREEN);
				cfg.postProcess.toneMapping = true;
				cfg.postProcess.aces = true;
				cfg.postProcess.gamma = 2.2f;
				cfg.postProcess.bloom = false;
				cfg.postProcess.ssao = false;
				cfg.ibl.useHDR = false;
				cfg.ibl.iblStrength = 1.1f;
				g.r3 = NkRenderer::Create(g.sharedDev, cfg);
				if (!g.r3) {
					g.err = "creation du renderer refusee";
					return false;
				}

				NkOffscreenDesc od;
				od.width = g.wantW;
				od.height = g.wantH;
				od.hdr = false;
				od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
				od.hasDepth = true;
				od.readable = true;
				od.readback = false;
				od.name = "NK3DModelerViewport";
				g.rt = g.r3->CreateOffscreen(od);
				if (!g.rt || !g.rt->IsValid()) {
					g.err = "cible hors ecran refusee";
					return false;
				}
				g.rtW = g.wantW;
				g.rtH = g.wantH;
				// Redirige la SORTIE FINALE du graphe vers notre cible : on recupere
				// donc le pipeline COMPLET (ombres, eclairage, IBL, tonemap), pas une
				// passe geometrie nue. C'est ce qui evite d'avoir a reimplementer un
				// eclairage d'apercu a cote du vrai.
				if (auto *texLib = g.r3->GetTextures())
					g.r3->SetFinalColorTarget(texLib->GetRHIHandle(g.rt->GetColorResult()));

				BuildStartCube();
				SyncMesh();
				g.ok = true;
				g.err = nullptr;
				return true;
			}

		} // namespace

		void Viewport3DSetSharedDevice(void *device) {
			g.sharedDev = (NkIDevice *)device;
		}

		void Viewport3DResize(uint32 w, uint32 h) {
			if (w < 16u)
				w = 16u;
			if (h < 16u)
				h = 16u;
			g.wantW = w;
			g.wantH = h;
			// La cible n'est refaite que si la taille CHANGE. Sans ce test, chaque
			// image detruirait et recreerait une texture de plusieurs megaoctets --
			// et le redimensionnement d'une fenetre appelle cette fonction a chaque
			// pixel parcouru.
			if (!g.ok || !g.rt || (w == g.rtW && h == g.rtH))
				return;
			if (g.rt->Resize(w, h)) {
				g.rtW = w;
				g.rtH = h;
				if (auto *texLib = g.r3->GetTextures())
					g.r3->SetFinalColorTarget(texLib->GetRHIHandle(g.rt->GetColorResult()));
			}
		}

		void Viewport3DOrbit(float32 dYaw, float32 dPitch) {
			g.yaw += dYaw;
			g.pitch += dPitch;
			// Le tangage est BORNE juste avant les poles. A la verticale exacte, la
			// direction de vue devient colineaire au vecteur « haut » et la matrice
			// de vue degenere : l'image bascule d'un quart de tour d'un coup.
			const float32 lim = 1.55f;
			if (g.pitch > lim)
				g.pitch = lim;
			if (g.pitch < -lim)
				g.pitch = -lim;
		}

		void Viewport3DPan(float32 dx, float32 dy) {
			// Le deplacement se fait DANS LE PLAN DE L'ECRAN, pas selon les axes du
			// monde : tirer vers la droite doit deplacer la scene vers la droite quel
			// que soit l'angle de la camera. On reconstruit donc la droite et le haut
			// de la camera a partir de son orientation.
			const float32 cp = cosf(g.pitch), sp = sinf(g.pitch);
			const float32 cy = cosf(g.yaw), sy = sinf(g.yaw);
			const NkVec3f fwd{-sy * cp, -sp, -cy * cp};
			const NkVec3f right{cy, 0.f, -sy};
			const NkVec3f up{fwd.z * right.y - fwd.y * right.z, fwd.x * right.z - fwd.z * right.x,
							 fwd.y * right.x - fwd.x * right.y};
			// L'amplitude suit la DISTANCE : de loin un pixel couvre beaucoup de
			// monde, de pres tres peu. Un pas fixe rendrait le cadrage inutilisable
			// a l'une des deux echelles.
			const float32 k = g.dist * 0.0015f;
			g.center.x += (-right.x * dx + up.x * dy) * k;
			g.center.y += (-right.y * dx + up.y * dy) * k;
			g.center.z += (-right.z * dx + up.z * dy) * k;
		}

		void Viewport3DZoom(float32 steps) {
			// Zoom MULTIPLICATIF : chaque cran divise ou multiplie la distance par le
			// meme facteur. Un pas additif avancerait par bonds enormes de loin et
			// n'avancerait plus du tout de pres.
			g.dist *= powf(0.88f, steps);
			if (g.dist < 0.2f)
				g.dist = 0.2f;
			if (g.dist > 500.f)
				g.dist = 500.f;
		}

		void Viewport3DFrameAll() {
			g.center = {0.f, 0.f, 0.f};
			g.dist = g.radius * 3.2f;
		}

		void Viewport3DSetShading(int32 shading, int32 solidLight) {
			g.shading = shading;
			g.solidLight = solidLight;
		}

		void Viewport3DSetOverlays(uint32 mask) {
			g.overlays = mask;
		}

		bool Viewport3DReady() {
			return g.ok;
		}

		const char *Viewport3DError() {
			return g.err;
		}

		void Viewport3DStats(uint32 &verts, uint32 &edges, uint32 &faces, uint32 &tris) {
			if (!g.ok) {
				verts = edges = faces = tris = 0u;
				return;
			}
			verts = g.edit.VertCount();
			edges = g.edit.EdgeCount();
			// FaceCount() rend la TAILLE DE LA TABLE, faces mortes comprises. Les
			// operations d'edition laissent des trous ; les compter donnerait un
			// nombre qui grimpe a chaque fusion alors que le maillage se simplifie.
			// On ne peut pas interroger les faces une par une depuis l'exterieur --
			// il n'y a pas d'accesseur. Mais la triangulation, elle, ne visite QUE
			// les faces vivantes : `triF` porte un identifiant de face par triangle,
			// donc le nombre de faces distinctes qu'il contient est exactement le
			// nombre de faces vivantes. Les identifiants y sont consecutifs par face
			// (une face donne ses n-2 triangles d'affilee), un simple comptage des
			// changements suffit.
			uint32 alive = 0u;
			NkEmId prev = (NkEmId)0xFFFFFFFFu;
			for (uint32 t = 0; t < (uint32)g.triF.Size(); ++t) {
				if (g.triF[t] != prev) {
					++alive;
					prev = g.triF[t];
				}
			}
			faces = alive;
			tris = (uint32)(g.triI.Size() / 3u);
		}

		void Viewport3DRenderOffscreen(void *cmdv) {
			if (!Init3D())
				return;
			NkICommandBuffer *cmd = (NkICommandBuffer *)cmdv;
			auto *r3d = g.r3->GetRender3D();
			if (!cmd || !r3d)
				return;

			SyncMesh();

			// Ce que NkRenderer::BeginFrame ferait si nous possedions la frame.
			r3d->ResetFrame();
			if (auto *mc = g.r3->GetMaterialCollection())
				mc->Upload();

			// ── Camera ──────────────────────────────────────────────────────
			const float32 cp = cosf(g.pitch), sp = sinf(g.pitch);
			NkCamera3DData cd;
			cd.position = {g.center.x + sinf(g.yaw) * cp * g.dist, g.center.y + sp * g.dist,
						   g.center.z + cosf(g.yaw) * cp * g.dist};
			cd.target = g.center;
			cd.up = {0.f, 1.f, 0.f};
			cd.fovY = 45.f;
			cd.aspect = (float32)g.rtW / (float32)(g.rtH > 0u ? g.rtH : 1u);
			// Le plan proche suit la distance : un plan proche fixe a 2 cm gaspille
			// toute la precision du tampon de profondeur quand on regarde une scene
			// de cinquante metres, et fait clignoter les surfaces coplanaires.
			cd.nearPlane = g.dist * 0.005f;
			if (cd.nearPlane < 0.01f)
				cd.nearPlane = 0.01f;
			cd.farPlane = g.dist * 20.f + 100.f;
			NkCamera3D cam(cd);

			// ── Grille ──────────────────────────────────────────────────────
			// Elle vient du moteur, pas d'un tas de lignes que nous dessinerions :
			// la grille de NkRender3D est calculee par pixel, donc elle ne moire pas
			// a l'horizon et ne coute pas un sommet.
			const bool wantGrid = (g.overlays & kOvGrid) != 0u;
			r3d->SetInfiniteGridEnabled(wantGrid || (g.overlays & kOvAxes) != 0u);
			NkInfiniteGridParams gp = r3d->GetInfiniteGridParams();
			gp.showMinor = wantGrid;
			gp.showMajor = wantGrid;
			gp.showAxes = (g.overlays & kOvAxes) != 0u;
			gp.cellSize = 1.f;
			gp.majorEvery = 10.f;
			r3d->SetInfiniteGridParams(gp);

			// ── Mode d'affichage ────────────────────────────────────────────
			// Filaire = un MODE, pas une surimpression : la surface disparait. Le
			// filaire de la case « Filaire » est autre chose -- la cage par-dessus la
			// surface -- et viendra avec la selection d'aretes.
			const bool wire = (g.shading == 3);
			r3d->SetWireframe(wire);

			NkSceneContext sctx;
			sctx.camera = cam;
			sctx.viewMode = wire ? NkViewMode::NK_WIREFRAME
								 : ((g.shading == 2) ? NkViewMode::NK_SOLID	   // Rendu = PBR complet
													: NkViewMode::NK_UNLIT); // Solide / Materiau
			// Deux directionnelles : une cle chaude en haut a gauche, un remplissage
			// froid en bas a droite. C'est l'eclairage d'atelier de tous les
			// modeleurs -- il donne du volume sans que rien ne tombe dans le noir,
			// ce qui compte plus ici que le realisme.
			NkLightDesc key;
			key.type = NkLightType::NK_DIRECTIONAL;
			key.direction = {-0.4f, -0.8f, -0.5f};
			key.color = {1.f, 0.97f, 0.92f};
			key.intensity = 3.f;
			key.castShadow = false; // pas d'ombres portees dans un viewport d'edition
			sctx.lights.PushBack(key);
			NkLightDesc fill;
			fill.type = NkLightType::NK_DIRECTIONAL;
			fill.direction = {0.5f, -0.3f, 0.6f};
			fill.color = {0.62f, 0.7f, 0.9f};
			fill.intensity = 1.1f;
			fill.castShadow = false;
			sctx.lights.PushBack(fill);
			sctx.iblIntensity = 1.f;
			sctx.ambientIntensity = 0.45f;
			r3d->BeginScene(sctx);

			if (g.meshOk) {
				NkDrawCall3D dc;
				dc.mesh = g.mesh;
				dc.transform = NkMat4f::Identity();
				dc.tint = {0.78f, 0.78f, 0.80f};
				dc.metallic = 0.f;
				dc.roughness = 0.62f;
				dc.castShadow = false;
				dc.aabb = NkAABB{{g.center.x - g.radius, g.center.y - g.radius, g.center.z - g.radius},
								 {g.center.x + g.radius, g.center.y + g.radius, g.center.z + g.radius}};
				r3d->Submit(dc);
			}

			// Le graphe fait tout : ombres, geometrie HDR, tonemap, et ecrit dans
			// NOTRE cible parce qu'on l'y a redirigee. Il gere ses propres passes et
			// son effacement -- surtout pas de BeginCapture manuel par-dessus.
			if (auto *graph = g.r3->GetRenderGraph())
				graph->Execute(cmd);
		}

		void Viewport3DRegisterInto(void *guiBackend) {
			if (!g.ok || !g.rt || !guiBackend)
				return;
			auto *b = (nkentseu::nkgui::NkGuiRHIBackend *)guiBackend;
			auto *texLib = g.r3->GetTextures();
			if (!texLib)
				return;
			b->RegisterTexture(kViewportTexId, texLib->GetRHIHandle(g.rt->GetColorResult()));
		}

		void Viewport3DShutdown() {
			if (g.r3) {
				if (g.rt)
					g.r3->DestroyOffscreen(g.rt);
				NkRenderer::Destroy(g.r3);
			}
			g.rt = nullptr;
			g.r3 = nullptr;
			g.ok = false;
			g.tried = false;
		}

	} // namespace nk3d
} // namespace nkentseu
