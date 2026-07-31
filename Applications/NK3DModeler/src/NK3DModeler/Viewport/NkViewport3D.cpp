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
#include "NKRenderer/Core/NkCameraController.h"
#include "NKRenderer/Core/NkGizmo.h"
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
#include "NK3DModeler/Viewport/NkVpPick.h"

#include <cmath>

namespace nkentseu {
	namespace nk3d {

		using namespace nkentseu::renderer;
		using math::NkMat4f;
		using math::NkVec3f;
		using math::NkVec4f;

		namespace {

			// ── PROJECTION MONDE -> ECRAN ───────────────────────────────────
			// Copiee telle quelle de Demo3D.cpp (l. 1069-1099), ou elle avait deja
			// ete extraite d'une lambda pour servir le mode objet ET le mode
			// edition. Elle est indispensable des qu'on veut savoir sur quoi le
			// curseur pointe : le picking, la selection par zone et l'arbitrage
			// gizmo/maillage la demandent tous.
			struct ScreenProj {
					NkVec3f camPos{}, fwd{}, rgt{}, upv{};
					float32 thX = 1.f, thY = 1.f, vw = 1.f, vh = 1.f;

					static ScreenProj Make(NkVec3f pos, NkVec3f target, float32 fovYDeg, float32 w,
										   float32 h) {
						ScreenProj p;
						p.camPos = pos;
						p.fwd = (target - pos).Normalized();
						p.rgt = p.fwd.Cross(NkVec3f{0.f, 1.f, 0.f}).Normalized();
						p.upv = p.rgt.Cross(p.fwd).Normalized();
						p.thY = tanf(fovYDeg * 0.5f * 3.14159265f / 180.f);
						p.thX = p.thY * (h > 0.f ? (w / h) : 1.f);
						p.vw = w;
						p.vh = h;
						return p;
					}

					// false si le point est DERRIERE la camera : sans ce test, un objet
					// dans le dos se projetterait a l'ecran par symetrie et serait
					// selectionne par une zone qui ne le contient pas visuellement.
					bool operator()(NkVec3f P, float32 &px, float32 &py) const {
						const NkVec3f v = P - camPos;
						const float32 zc = v.Dot(fwd);
						if (zc <= 1e-3f)
							return false;
						const float32 nx = v.Dot(rgt) / (zc * thX), ny = v.Dot(upv) / (zc * thY);
						px = (nx * 0.5f + 0.5f) * vw;
						py = (0.5f - ny * 0.5f) * vh;
						return true;
					}
			};

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

					// ── Camera : CELLE DU MOTEUR ────────────────────────────
					// J'avais ecrit yaw/pitch/dist a la main. C'etait une erreur, et
					// Rihen l'a relevee : NkOrbitCameraController3D existe, il est
					// utilise par Demo3D, et il fait deja ce que ma version faisait
					// mal -- notamment OrbitAroundPivot, qui fait tourner la POSITION
					// ET LA CIBLE ensemble. Ma version revisait le pivot a chaque
					// image, ce qui produit un saut au premier mouvement des qu'on
					// orbite autour d'autre chose que l'origine.
					NkOrbitCameraController3D cam;
					bool ortho = false;		///< projection orthographique (pave 5)
					float32 radius = 1.8f;	///< rayon englobant, pour le recadrage

					// ── Gizmo : CELUI DU MOTEUR AUSSI ───────────────────────────
					// NkGizmo3D apporte d'un bloc translation / rotation / echelle /
					// combine, les orientations global / local / normal, l'accrochage
					// au Ctrl, cinq modes de pivot et le curseur 3D. Ce sont
					// exactement les commandes de la barre de la vue -- les
					// reecrire aurait donne une version pauvre de ce qui existe.
					NkGizmo3D gizmo;
					NkGizmoInput gin{};

					// ── Mode edition ────────────────────────────────────────
					// `selMask` : bits 1 sommet, 2 arete, 4 face -- COMBINABLES, comme
					// dans Blender ou Maj+1/2/3 additionne les sous-modes. Un entier
					// plutot qu'une enumeration exclusive, parce que travailler en
					// sommet ET face en meme temps est courant.
					bool editMode = false;
					uint32 selMask = 1u;
					bool xray = false;
					NkVector<uint8> vertSel;   ///< 1 = sommet selectionne
					NkVector<uint32> editEdges; ///< paires, cf. GetUniqueEdges
					NkVector<float32> overlayLines; ///< pos3 + rgba4 par sommet

					// ── Annulation et journal ───────────────────────────────
					// Deux mecanismes DISTINCTS, et c'est voulu :
					//  - `history` garde des INSTANTANES du maillage avant chaque
					//    operation. C'est ce qui rend Ctrl+Z immediat, quelle que soit
					//    la complexite de l'operation annulee ;
					//  - `recorder` garde les COMMANDES elles-memes, avec leur
					//    selection. C'est ce qui permet de rejouer une session sur un
					//    autre maillage, de la sauver dans un fichier, et de comprendre
					//    ce qui a ete fait. Un instantane ne dit pas « biseau de 0,2 sur
					//    ces quatre aretes », il dit seulement « voici l'etat d'avant ».
					NkEditHistory history;
					NkMeshEditRecorder recorder;
					int32 activeVert = -1;
					int32 activeEdgeA = -1, activeEdgeB = -1;
					bool overlayDirty = true;
					NkMat4f anchor = NkMat4f::Identity(); ///< objet -> monde

					// Derniere camera connue. Le picking arrive pendant la PEINTURE,
					// donc avant que la frame 3D suivante n'ait pose sa camera : sans
					// cette memoire il faudrait recalculer la matrice de vue a deux
					// endroits, avec le risque qu'elles divergent.
					NkVec3f lastCamPos{};
					ScreenProj lastProj{};
					NkVec3f center{0.f, 0.f, 0.f}; ///< centre de la scene, pour le recadrage

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

			// Apres une operation topologique, la selection qui compte est celle du
			// MAILLAGE : lui seul sait quels sommets il vient de creer. Une extrusion
			// selectionne la nouvelle geometrie, et c'est ce qu'on veut retrouver.
			void PullSelection() {
				const uint32 nv = g.edit.VertCount();
				g.vertSel.Resize(nv);
				for (uint32 i = 0; i < nv; ++i)
					g.vertSel[i] = g.edit.verts[i].sel;
				g.activeVert = -1;
				g.activeEdgeA = g.activeEdgeB = -1;
				g.overlayDirty = true;
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
				// La CAGE vient de GetUniqueEdges, pas des triangles de rendu. Sur un
				// quad la diagonale de triangulation n'existe pas : un cube a 12
				// aretes, pas 18. Dessiner les aretes des triangles donnerait une
				// cage qui ne correspond a rien de modifiable.
				g.edit.GetUniqueEdges(g.editEdges);
				// La selection est indexee par sommet : elle doit suivre la taille du
				// maillage, sinon une extrusion laisse un tableau trop court et tout
				// ce qui vient apres lit hors bornes.
				const uint32 nv = (uint32)g.triV.Size();
				if ((uint32)g.vertSel.Size() != nv) {
					NkVector<uint8> old = g.vertSel;
					g.vertSel.Resize(nv);
					for (uint32 i = 0; i < nv; ++i)
						g.vertSel[i] = (i < (uint32)old.Size()) ? old[i] : (uint8)0;
				}
				g.overlayDirty = true;
				g.meshDirty = false;
			}

			// ── POINT D'ENTREE UNIQUE DE TOUTE MODIFICATION ─────────────────
			// Porte de Demo3D (l. 1273-1297). Rien ne modifie le maillage sans passer
			// par ici, et ce n'est pas une precaution de style : c'est ce qui garantit
			// que TOUTE operation est annulable et rejouable. Une seule modification
			// faite en direct, et l'historique ment.
			//
			// La selection est copiee DANS la commande. Elle en fait partie : rejouer
			// « biseauter » sans savoir quoi biseauter n'a aucun sens.
			bool ApplyCmd(NkMeshEditCommand cmd) {
				if (!g.ok)
					return false;
				cmd.selection.Clear();
				for (uint32 i = 0; i < (uint32)g.vertSel.Size(); ++i)
					if (g.vertSel[i])
						cmd.selection.PushBack(i);
				// La selection est POUSSEE dans le maillage avant l'operation : les
				// operations de NkEditMesh travaillent sur SA selection interne, pas
				// sur notre tableau.
				g.edit.SetVertSelection(g.vertSel.Data(), (uint32)g.vertSel.Size());
				NkEditMesh snapshot = g.edit; // pre-etat
				if (!cmd.Apply(g.edit))
					return false; // sans effet : ni annulation ni journal
				g.history.Commit(snapshot);
				g.recorder.Push(cmd);
				PullSelection();
				g.meshDirty = true;
				SyncMesh();
				return true;
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
			// ORBITE RIGIDE autour du pivot de la selection : position ET cible
			// tournent ensemble, donc aucune revisee du pivot, donc aucun saut au
			// premier mouvement. C'est le comportement de Blender, et c'est ce que
			// ma camera maison ratait. Sans selection on retombe sur une orbite
			// autour de la cible courante.
			if (g.gizmo.HasSelection())
				g.cam.OrbitAroundPivot(g.gizmo.GetPivot(), dYaw, dPitch);
			else
				g.cam.Rotate(dYaw, dPitch);
			// Orbiter librement REPASSE en perspective : rester en orthographique
			// apres avoir quitte l'axe donne une image plate ou plus rien ne fuit,
			// et on ne comprend plus ce qu'on regarde. Blender fait de meme.
			if (dYaw != 0.f || dPitch != 0.f)
				g.ortho = false;
		}

		void Viewport3DPan(float32 dx, float32 dy) {
			// Axes INVERSES : on tire la scene, on ne deplace pas la camera. Tirer
			// vers la droite doit amener vers la droite ce qu'on regarde.
			g.cam.Pan(-dx, -dy);
		}

		void Viewport3DZoom(float32 steps) {
			g.cam.Zoom(steps);
		}

		void Viewport3DPanSteps(float32 dx, float32 dy) {
			// Molette + modificateur : une crantee vaut ~1 la ou un deplacement de
			// souris vaut 10 a 20 pixels. Sans ce facteur, Maj+molette ne bougerait
			// visiblement rien.
			g.cam.Pan(dx * 22.f, dy * 22.f);
		}

		void Viewport3DFrameAll() {
			// Recadrage : on place la cible au centre de la scene et on recule de
			// quoi faire tenir la sphere englobante dans le champ. La marge de 1.25
			// evite que l'objet touche le bord -- un cadrage exact donne l'impression
			// que quelque chose est coupe.
			const float32 fovY = 45.f * 3.14159265f / 180.f;
			float32 d = (g.radius * 1.25f) / tanf(fovY * 0.5f);
			if (d < 0.5f)
				d = 0.5f;
			g.cam.SetCenter(g.center, d, g.cam.GetYaw(), g.cam.GetPitch());
		}

		void Viewport3DAxisView(int32 which, bool opposite) {
			// Vues axiales du pave numerique, portees de Demo3D (l. 2322-2390) :
			// 0 = face, 1 = droite, 2 = dessus ; `opposite` (Ctrl) donne l'arriere,
			// la gauche et le dessous. Le tangage est limite a ~89 degres, pas 90 :
			// a la verticale exacte la matrice de vue degenere.
			const NkVec3f t = g.cam.GetTarget();
			const float32 d = g.cam.GetDistance();
			const float32 P = 1.55f;
			switch (which) {
				case 0:
					g.cam.SetCenter(t, d, opposite ? -1.5708f : 1.5708f, 0.f);
					break;
				case 1:
					g.cam.SetCenter(t, d, opposite ? 3.1416f : 0.f, 0.f);
					break;
				default:
					g.cam.SetCenter(t, d, 0.f, opposite ? -P : P);
					break;
			}
			// Une vue axiale passe en ORTHOGRAPHIQUE : c'est tout son interet. En
			// perspective, une vue de face garde des fuyantes et ne permet ni de
			// comparer deux longueurs ni d'aligner quoi que ce soit.
			g.ortho = true;
		}

		void Viewport3DSetOrtho(bool on) {
			g.ortho = on;
		}

		bool Viewport3DIsOrtho() {
			return g.ortho;
		}

		// ── Gizmo ───────────────────────────────────────────────────────────
		void Viewport3DSetGizmoMode(int32 mode) {
			g.gizmo.SetMode(mode);
		}

		void Viewport3DSetGizmoOrientation(int32 orient) {
			g.gizmo.SetOrientation(orient);
		}

		void Viewport3DSetGizmoPivot(int32 pivot) {
			g.gizmo.SetPivotMode(pivot);
		}

		void Viewport3DSetSnap(bool on, float32 translate, float32 rotateDeg, float32 scale) {
			g.gizmo.SetSnapEnabled(on);
			g.gizmo.SetSnapSteps(translate, rotateDeg, scale);
		}

		void Viewport3DSetGizmoInput(float32 mouseX, float32 mouseY, float32 dx, float32 dy,
									 bool leftPressed, bool leftDown, bool shift, bool ctrl) {
			g.gin.mouseX = mouseX;
			g.gin.mouseY = mouseY;
			g.gin.mouseDX = dx;
			g.gin.mouseDY = dy;
			g.gin.leftPressed = leftPressed;
			g.gin.leftDown = leftDown;
			g.gin.shiftDown = shift;
			g.gin.ctrlDown = ctrl;
		}

		bool Viewport3DGizmoDragging() {
			return g.gizmo.IsDragging();
		}

		// ── Mode edition et selection ───────────────────────────────────────
		void Viewport3DSetEditMode(bool on) {
			g.editMode = on;
			g.overlayDirty = true;
			if (!on) {
				// Sortir d'edition NE VIDE PAS la selection de sommets : y revenir
				// doit retrouver ce qu'on avait, comme dans Blender. C'est ce qui
				// permet d'aller regarder l'objet en entier puis de reprendre.
				g.gizmo.ClearSelection();
			}
		}

		bool Viewport3DEditMode() {
			return g.editMode;
		}

		void Viewport3DSetSelectMask(uint32 mask) {
			// Toujours AU MOINS un sous-mode actif : a zero, plus rien n'est
			// selectionnable et l'application parait cassee sans qu'on sache pourquoi.
			g.selMask = mask ? mask : 1u;
			g.overlayDirty = true;
		}

		uint32 Viewport3DSelectMask() {
			return g.selMask;
		}

		void Viewport3DSetXray(bool on) {
			g.xray = on;
			g.overlayDirty = true;
		}

		void Viewport3DSelectAll(bool all) {
			for (uint32 i = 0; i < (uint32)g.vertSel.Size(); ++i)
				g.vertSel[i] = all ? (uint8)1 : (uint8)0;
			if (!all) {
				g.activeVert = -1;
				g.activeEdgeA = g.activeEdgeB = -1;
			}
			g.overlayDirty = true;
		}

		uint32 Viewport3DSelectedCount() {
			uint32 n = 0;
			for (uint32 i = 0; i < (uint32)g.vertSel.Size(); ++i)
				n += g.vertSel[i] ? 1u : 0u;
			return n;
		}

		// Clic dans la vue. Coordonnees RELATIVES au rectangle de la vue, pas a la
		// fenetre : la cible hors ecran a sa propre origine, et lui passer des
		// coordonnees fenetre decalerait tout le picking de la largeur du panneau
		// de gauche.
		bool Viewport3DPick(float32 mx, float32 my, bool add, bool toggle) {
			if (!g.ok || !g.editMode)
				return false;
			NkPickCtx c;
			c.mesh = &g.edit;
			c.rest = &g.triV;
			c.tris = &g.triI;
			c.edges = &g.editEdges;
			c.anchor = g.anchor;
			c.camPos = g.lastCamPos;
			c.fwd = g.lastProj.fwd;
			c.rgt = g.lastProj.rgt;
			c.upv = g.lastProj.upv;
			c.thX = g.lastProj.thX;
			c.thY = g.lastProj.thY;
			c.vpW = (float32)g.rtW;
			c.vpH = (float32)g.rtH;
			c.selMask = g.selMask;
			c.xray = g.xray;
			const NkPickResult r = NkPickEditElement(c, mx, my);

			// PRIORITE façon Blender : sommet, puis arete, puis face -- chacun dans
			// son propre seuil. Ce n'est pas « le plus proche gagne » toutes
			// categories confondues : viser un sommet doit prendre le sommet meme si
			// une arete passe plus pres du curseur, sinon les sommets deviennent
			// inatteignables sur un maillage dense.
			if (!add && !toggle)
				Viewport3DSelectAll(false);

			auto touch = [&](int32 i) {
				if (i < 0 || i >= (int32)g.vertSel.Size())
					return;
				g.vertSel[(uint32)i] = toggle ? (uint8)(g.vertSel[(uint32)i] ? 0 : 1) : (uint8)1;
			};

			bool got = false;
			if (r.vert >= 0) {
				touch(r.vert);
				g.activeVert = r.vert;
				g.activeEdgeA = g.activeEdgeB = -1;
				got = true;
			} else if (r.edgeA >= 0) {
				touch(r.edgeA);
				touch(r.edgeB);
				g.activeEdgeA = r.edgeA;
				g.activeEdgeB = r.edgeB;
				g.activeVert = -1;
				got = true;
			} else if (r.tri >= 0) {
				// Une face selectionnee, ce sont SES SOMMETS marques. C'est le modele
				// de Blender : la selection est portee par les sommets, et les aretes
				// et faces s'en deduisent. Sans cela il faudrait tenir trois tableaux
				// coherents entre eux a chaque operation topologique.
				for (uint32 k = 0; k < 3u; ++k)
					touch((int32)g.triI[(uint32)r.tri + k]);
				g.activeVert = -1;
				g.activeEdgeA = g.activeEdgeB = -1;
				got = true;
			}
			g.overlayDirty = true;
			return got;
		}

		// ── OPERATIONS D'EDITION ────────────────────────────────────────────
		// Chacune est un mince emballage autour d'ApplyCmd : elle remplit une
		// commande et la soumet. C'est la forme de Demo3D, et elle a un avantage
		// concret sur des appels directs a NkEditMesh -- l'annulation et le journal
		// sont acquis d'office, sans que chaque operation ait a y penser.

		bool Viewport3DExtrude(bool individual) {
			// EXTRUSION SELON LE SOUS-MODE ACTIF : face > arete > sommet. Une face
			// donne un chapeau et des quads lateraux, une arete un quad de liaison,
			// un sommet une arete filaire.
			//
			// DECALAGE ZERO, comme Blender : la nouvelle geometrie nait exactement sur
			// l'ancienne et devient la selection ; c'est l'utilisateur qui la deplace
			// ensuite au gizmo. Un decalage automatique obligerait a corriger a chaque
			// fois la distance choisie a sa place.
			NkMeshEditCommand c;
			if (g.selMask & 4u)
				c.op = NkMeshEditOp::Extrude;
			else if (g.selMask & 2u)
				c.op = NkMeshEditOp::ExtrudeEdges;
			else
				c.op = NkMeshEditOp::ExtrudeVerts;
			c.extrude.individual = individual;
			c.extrude.offset = 0.f;
			if (ApplyCmd(c))
				return true;
			// REPLI : en mode face sans aucune face ENTIEREMENT selectionnee,
			// l'extrusion de faces ne fait rien. On retombe alors sur les aretes puis
			// les sommets -- Blender extrude « ce qui est selectionne », pas « ce que
			// le sous-mode designe ».
			if (c.op == NkMeshEditOp::Extrude) {
				c.op = NkMeshEditOp::ExtrudeEdges;
				if (ApplyCmd(c))
					return true;
			}
			if (c.op != NkMeshEditOp::ExtrudeVerts) {
				c.op = NkMeshEditOp::ExtrudeVerts;
				return ApplyCmd(c);
			}
			return false;
		}

		bool Viewport3DDeleteSelection() {
			NkMeshEditCommand c;
			c.op = NkMeshEditOp::Delete;
			return ApplyCmd(c);
		}

		bool Viewport3DMerge(int32 mode) {
			NkMeshEditCommand c;
			c.op = NkMeshEditOp::Merge;
			c.merge.mode = mode;
			return ApplyCmd(c);
		}

		bool Viewport3DMakeFace() {
			NkMeshEditCommand c;
			c.op = NkMeshEditOp::MakeFace;
			return ApplyCmd(c);
		}

		bool Viewport3DSubdivide(int32 cuts) {
			NkMeshEditCommand c;
			c.op = NkMeshEditOp::Subdivide;
			c.subdiv.cuts = cuts < 1 ? 1 : cuts;
			return ApplyCmd(c);
		}

		bool Viewport3DLoopCut(int32 cuts) {
			NkMeshEditCommand c;
			c.op = NkMeshEditOp::LoopCut;
			c.loopcut.cuts = cuts < 1 ? 1 : cuts;
			return ApplyCmd(c);
		}

		bool Viewport3DBevel(float32 offset, int32 segments, bool vertexOnly) {
			NkMeshEditCommand c;
			c.op = NkMeshEditOp::Bevel;
			c.bevel.offset = offset;
			c.bevel.segments = segments < 1 ? 1 : segments;
			c.bevel.vertexOnly = vertexOnly;
			return ApplyCmd(c);
		}

		bool Viewport3DInset(float32 thickness, float32 depth) {
			NkMeshEditCommand c;
			c.op = NkMeshEditOp::Inset;
			c.inset.thickness = thickness;
			c.inset.depth = depth;
			return ApplyCmd(c);
		}

		bool Viewport3DDissolve() {
			NkMeshEditCommand c;
			c.op = NkMeshEditOp::Dissolve;
			return ApplyCmd(c);
		}

		bool Viewport3DMoveSelection(float32 dx, float32 dy, float32 dz) {
			// Le deplacement porte un delta PAR SOMMET, pas un delta global : c'est
			// ce qui permettra plus tard l'edition proportionnelle, ou les sommets
			// voisins suivent en s'attenuant. Ici tous les selectionnes recoivent le
			// meme, ce qui est le cas courant.
			NkMeshEditCommand c;
			c.op = NkMeshEditOp::Move;
			uint32 n = 0;
			for (uint32 i = 0; i < (uint32)g.vertSel.Size(); ++i)
				if (g.vertSel[i])
					++n;
			c.moveDeltas.Resize(n);
			for (uint32 i = 0; i < n; ++i)
				c.moveDeltas[i] = NkVec3f{dx, dy, dz};
			return ApplyCmd(c);
		}

		// ── ANNULATION ──────────────────────────────────────────────────────
		bool Viewport3DUndo() {
			if (!g.ok || !g.history.CanUndo())
				return false;
			g.history.Undo(g.edit);
			PullSelection();
			g.meshDirty = true;
			SyncMesh();
			return true;
		}

		bool Viewport3DRedo() {
			if (!g.ok || !g.history.CanRedo())
				return false;
			g.history.Redo(g.edit);
			PullSelection();
			g.meshDirty = true;
			SyncMesh();
			return true;
		}

		bool Viewport3DCanUndo() {
			return g.ok && g.history.CanUndo();
		}

		bool Viewport3DCanRedo() {
			return g.ok && g.history.CanRedo();
		}

		uint32 Viewport3DEditCount() {
			return g.ok ? g.recorder.Count() : 0u;
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
			const float32 dist = g.cam.GetDistance();
			NkCamera3DData cd;
			cd.up = {0.f, 1.f, 0.f};
			cd.fovY = 45.f;
			cd.aspect = (float32)g.rtW / (float32)(g.rtH > 0u ? g.rtH : 1u);
			// Le plan proche suit la distance : fixe a 2 cm, il gaspille toute la
			// precision du tampon de profondeur des qu'on regarde une scene de
			// cinquante metres, et les surfaces coplanaires se mettent a clignoter.
			cd.nearPlane = dist * 0.005f;
			if (cd.nearPlane < 0.01f)
				cd.nearPlane = 0.01f;
			cd.farPlane = dist * 20.f + 100.f;
			NkCamera3D cam(cd);
			g.cam.Apply(cam); // c'est le controleur qui pose position et cible
			// Orthographique : la demi-hauteur est derivee de la distance pour que le
			// cadrage reste comparable a la perspective. Sans cela, basculer en ortho
			// donne un changement d'echelle brutal qui fait croire a un zoom.
			if (g.ortho)
				cam.SetOrtho(true, dist * 0.55f);
			else
				cam.SetOrtho(false);

			// ── Gizmo : mise a jour puis rendu ──────────────────────────────
			// Il est alimente APRES la camera (il projette ses poignees a l'ecran) et
			// AVANT BeginScene (son trace passe par les lignes de debogage, qui sont
			// accumulees pour la frame).
			g.gizmo.SetCamera(cam.GetPosition(), cam.GetTarget(), 45.f, (float32)g.rtW,
							  (float32)g.rtH);
			// Memorise pour le picking, qui a lieu pendant la peinture de l'interface
			// -- donc hors de cette fonction.
			g.lastCamPos = cam.GetPosition();
			g.lastProj = ScreenProj::Make(cam.GetPosition(), cam.GetTarget(), 45.f, (float32)g.rtW,
										  (float32)g.rtH);

			// La CIBLE du gizmo. En mode edition c'est le barycentre de la selection ;
			// en mode objet, l'objet lui-meme. L'extent minuscule en edition n'est pas
			// un bricolage : le gizmo dessine un marqueur de boite englobante autour
			// de sa cible, et un point de selection n'a pas de volume.
			{
				NkGizmoTarget vt[1];
				bool haveTarget = false;
				if (g.editMode) {
					NkVec3f piv{0.f, 0.f, 0.f};
					uint32 n = 0;
					for (uint32 i = 0; i < (uint32)g.vertSel.Size() && i < (uint32)g.triV.Size(); ++i)
						if (g.vertSel[i]) {
							const NkVec3f w = g.anchor * g.triV[i].pos;
							piv.x += w.x;
							piv.y += w.y;
							piv.z += w.z;
							++n;
						}
					if (n) {
						piv = piv * (1.f / (float32)n);
						vt[0].base = NkMat4f::Translate(piv);
						vt[0].localHalf = {0.001f, 0.001f, 0.001f};
						vt[0].pickRadius = 0.0001f;
						haveTarget = true;
					}
				} else if (g.meshOk) {
					vt[0].base = g.anchor;
					vt[0].localHalf = {g.radius, g.radius, g.radius};
					vt[0].pickRadius = g.radius;
					haveTarget = true;
				}
				if (haveTarget) {
					if (!g.gizmo.HasSelection())
						g.gizmo.Select(0);
					g.gizmo.Update(vt, 1, g.gin);
				} else {
					g.gizmo.ClearSelection();
				}
			}

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

			// ── OVERLAY D'EDITION ───────────────────────────────────────────
			// Porte de Demo3D (l. 5647-5762). Deux choses a ne pas defaire :
			//  - la cage vient de GetUniqueEdges, jamais des triangles de rendu ;
			//  - la couleur est portee PAR EXTREMITE, et le GPU interpole. Une arete
			//    dont un seul bout est selectionne apparait donc en degrade
			//    orange -> noir, gratuitement, sans decouper le segment.
			// Et surtout : AUCUN decalage geometrique. Le decalage precedent etait
			// proportionnel a la taille de l'objet, donc la cage ne suivait plus la
			// silhouette des objets allonges, et les marqueurs de sommets, eux traces
			// sans decalage, ne coincidaient plus avec elle. Le z-fighting est deja
			// traite par le depth-bias des passes overlay.
			if (g.editMode) {
				if (g.overlayDirty) {
					g.overlayDirty = false;
					const NkVec4f cageCol{0.015f, 0.015f, 0.02f, 1.f};
					const NkVec4f selEdgeCol{1.f, 0.70f, 0.13f, 1.f};
					const NkVec4f actCol{1.f, 1.f, 1.f, 1.f};
					auto pushV = [](NkVector<float32> &A, NkVec3f p, NkVec4f c) {
						A.PushBack(p.x);
						A.PushBack(p.y);
						A.PushBack(p.z);
						A.PushBack(c.x);
						A.PushBack(c.y);
						A.PushBack(c.z);
						A.PushBack(c.w);
					};
					g.overlayLines.Clear();
					g.overlayLines.Reserve((uint32)g.editEdges.Size() * 7u);
					const uint32 ns = (uint32)g.vertSel.Size();
					// Passe 0 = aretes neutres, passe 1 = aretes touchees. Les secondes
					// sont tracees APRES, donc elles gagnent le z-fight et restent
					// franches.
					for (int32 pass = 0; pass < 2; ++pass) {
						for (uint32 e = 0; e + 1 < (uint32)g.editEdges.Size(); e += 2) {
							const uint32 a = g.editEdges[e], b = g.editEdges[e + 1];
							if (a >= ns || b >= ns)
								continue;
							const bool selA = (g.vertSel[a] != 0), selB = (g.vertSel[b] != 0);
							if ((selA || selB) != (pass == 1))
								continue;
							// ARETE ACTIVE : blanche sur TOUTE sa longueur. Un degrade
							// dirait « cette extremite est le sommet actif », ce qui est
							// une autre information.
							const bool actE = (g.activeEdgeA >= 0 &&
											   (((int32)a == g.activeEdgeA && (int32)b == g.activeEdgeB) ||
												((int32)a == g.activeEdgeB && (int32)b == g.activeEdgeA)));
							auto vcol = [&](uint32 v, bool sel) -> NkVec4f {
								if (actE)
									return actCol;
								if (sel && (int32)v == g.activeVert)
									return actCol;
								return sel ? selEdgeCol : cageCol;
							};
							pushV(g.overlayLines, g.anchor * g.triV[a].pos, vcol(a, selA));
							pushV(g.overlayLines, g.anchor * g.triV[b].pos, vcol(b, selB));
						}
					}
					r3d->SetEditOverlayLines(g.overlayLines.Empty() ? nullptr : g.overlayLines.Data(),
											 (uint32)(g.overlayLines.Size() / 7u));
					// Les marqueurs de sommets ne passent PLUS par le lot persistant :
					// le rendu en point-sprite s'est revele peu fiable en DX12. Ils sont
					// traces chaque image en triangles pleins, plus bas.
					r3d->SetEditOverlayPoints(nullptr, 0);
					r3d->SetEditOverlayTris(nullptr, 0);
					r3d->SetEditOverlayXray(g.xray);
				}
			} else {
				r3d->ClearEditOverlay();
			}

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

			// ── MARQUEURS DE SOMMETS ────────────────────────────────────────
			// En quads pleins face-camera, pas en point-sprites : c'est le meme chemin
			// overlay sans test de profondeur que le gizmo, correct en OpenGL comme en
			// DX12. Taille indexee sur la DISTANCE -- un marqueur en unites monde
			// disparaitrait de loin et couvrirait l'objet de pres.
			if (g.editMode && (g.selMask & 1u)) {
				const NkVec3f rgt = g.lastProj.rgt, upv = g.lastProj.upv;
				const uint32 ns = (uint32)g.vertSel.Size();
				for (uint32 i = 0; i < ns && i < (uint32)g.triV.Size(); ++i) {
					const NkVec3f w = g.anchor * g.triV[i].pos;
					const float32 hs = (w - g.lastCamPos).Len() * 0.0035f;
					const NkVec4f col = ((int32)i == g.activeVert)
											? NkVec4f{1.f, 1.f, 1.f, 1.f}
											: (g.vertSel[i] ? NkVec4f{1.f, 0.70f, 0.13f, 1.f}
															: NkVec4f{0.05f, 0.05f, 0.06f, 1.f});
					const NkVec3f a = w - rgt * hs - upv * hs, b = w + rgt * hs - upv * hs;
					const NkVec3f c2 = w + rgt * hs + upv * hs, d2 = w - rgt * hs + upv * hs;
					r3d->DrawDebugTriangle(a, b, c2, col, 0.f, true);
					r3d->DrawDebugTriangle(a, c2, d2, col, 0.f, true);
				}
			}

			// ── GIZMO ───────────────────────────────────────────────────────
			// Trace en lignes ET en triangles : les fleches pleines se lisent bien
			// mieux que des chevrons en fil de fer. Le `true` final demande le trace en
			// surimpression, sans test de profondeur -- un gizmo a moitie enfoui dans
			// le maillage serait inutilisable.
			if (g.gizmo.HasSelection())
				g.gizmo.Draw(
					[&](NkVec3f a, NkVec3f b, NkVec4f c) { r3d->DrawDebugLine(a, b, c, 0.f, true); },
					[&](NkVec3f a, NkVec3f b, NkVec3f c, NkVec4f col) {
						r3d->DrawDebugTriangle(a, b, c, col, 0.f, true);
					});

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
