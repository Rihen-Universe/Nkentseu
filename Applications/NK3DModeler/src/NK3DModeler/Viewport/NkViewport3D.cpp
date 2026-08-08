// -----------------------------------------------------------------------------
// @File    NkViewport3D.cpp
// @Brief   SEULE unite de compilation qui voit NKRenderer.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// Elle est seule pour une raison mecanique, pas par gout de l'isolation :
// NKRenderer et NKCanvas declarent tous deux `renderer::NkBlendMode` et
// `renderer::NkVertex2D`. Les melanger dans une unite ne compile pas. Le reste
// de NK3DModeler ne voit donc que NkViewport3D.h et ses `void *`.
//
// MODELE : Applications/NkAnimaEditor/src/NkAnimaEditor/AnimBridge.cpp pour la
// composition hors ecran, et Applications/Sandbox/src/Demo/Demo3D.cpp pour
// l'edition -- les deux tournent a l'ecran depuis des mois.
//
// LE POINT DELICAT : NOUS NE PILOTONS PAS LA FRAME. L'editeur ouvre la frame
// device et le command buffer ; nous ne faisons qu'ajouter nos passes dessus.
// Donc pas de BeginFrame / EndFrame / Present ici -- mais il faut rejouer a la
// main ce que NkRenderer::BeginFrame ferait : ResetFrame() et l'envoi des
// materiaux.
//
// LA SCENE EST MULTI-OBJETS ET NAIT VIDE. Demo3D fige 86 objets en dur ; un
// modeleur fait l'inverse : rien au depart, et « Ajouter » cree maillages,
// lumieres, cameras et reperes vides. Chaque objet maillage possede SON
// NkEditMesh -- la source de verite -- et le tampon GPU n'en est qu'une
// projection. Sortir du mode edition puis y revenir retrouve la topologie
// exacte, pas une version retriangulee.
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
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace nk3d {

		using namespace nkentseu::renderer;
		using math::NkAngle;
		using math::NkMat4f;
		using math::NkVec3f;
		using math::NkVec4f;

		namespace {

			// ── PROJECTION MONDE -> ECRAN ───────────────────────────────────
			// Copiee telle quelle de Demo3D.cpp (l. 1069-1099). Indispensable des
			// qu'on veut savoir sur quoi le curseur pointe : le picking, la
			// selection par zone et l'arbitrage gizmo/maillage la demandent tous.
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

			// ── UN OBJET DE SCENE ───────────────────────────────────────────
			// Un seul type de fiche pour tout : maillage, lumiere, camera, repere
			// vide. Les champs de maillage restent vides pour les autres. Une
			// hierarchie de classes serait plus « propre » sur le papier, mais elle
			// interdirait le tableau plat a indices stables dont la hierarchie de
			// l'interface a besoin.
			struct VpObject {
					bool alive = false;
					int32 type = 0; ///< cf. kVpObj* dans NkViewport3D.h
					char name[32] = {};
					bool visible = true;
					bool selected = false;
					// VERROUILLE : ni selectionnable ni modifiable. C'est ce que
					// promet le cadenas de la hierarchie, et il ne servirait a rien
					// si le clic dans la vue passait outre.
					bool locked = false;

					// Transformation. pos/rot/scl sont LA SOURCE : le panneau
					// Proprietes les edite en direct, et la matrice monde n'est
					// qu'une composition. L'inverse (matrice source, champs derives)
					// obligerait a decomposer a chaque affichage, et la decomposition
					// d'Euler n'est pas unique : les champs sauteraient de -180 a 180
					// sous les yeux de l'utilisateur.
					NkVec3f pos{0.f, 0.f, 0.f};
					NkVec3f rotDeg{0.f, 0.f, 0.f};
					NkVec3f scl{1.f, 1.f, 1.f};
					NkMat4f world = NkMat4f::Identity(); ///< compose de pos/rot/scl
					NkMat4f dragBase = NkMat4f::Identity(); ///< monde fige au debut d'un glissement

					// ── Maillage (type == kVpObjMesh) ───────────────────────
					NkEditMesh edit;
					NkMeshHandle mesh;
					bool meshOk = false;
					bool meshDirty = true;
					// DEUX TRIANGULATIONS, et la distinction est cruciale :
					//  - `restV` / `restI` viennent de Triangulate, dont le contrat est
					//    1:1 avec les sommets du maillage. C'est la SEULE base valide
					//    pour la cage, le picking, la selection et les marqueurs : les
					//    indices d'arete rendus par GetUniqueEdges designent des
					//    sommets DU MAILLAGE ;
					//  - `triV` / `triI` viennent de TriangulateShaded, qui DEDOUBLE
					//    les coins des faces plates pour leur donner des normales
					//    franches. Ses indices ne correspondent donc plus a ceux du
					//    maillage -- s'en servir pour la cage donnait des aretes qui
					//    reliaient n'importe quoi.
					// C'est exactement le partage de Demo3D (editRest / editLive).
					NkVector<NkVertex3D> restV;
					NkVector<uint32> restI;
					NkVector<NkEmId> restF;
					NkVector<NkVertex3D> triV;
					NkVector<uint32> triI;
					NkVector<NkEmId> triF;
					NkVector<uint32> edges;
					float32 radius = 1.f; ///< rayon englobant local
					// PILE DE MODIFICATEURS, non destructive : `edit` reste la base,
					// `evaluated` recoit le resultat de la pile. C'est cette derniere
					// qui part au GPU -- sans quoi ajouter un miroir modifierait la
					// cage qu'on est en train d'editer.
					NkModifierStack mods;
					NkEditMesh evaluated;
					bool modsDirty = true;

					// ── Lumiere (types kVpObjLight*) ────────────────────────
					NkVec3f lightColor{1.f, 1.f, 1.f};
					float32 lightIntensity = 3.f;
					float32 lightRange = 10.f;
			};

			constexpr int32 kMaxObjects = 64;

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
					VpObject objs[kMaxObjects];
					int32 activeObj = -1; ///< l'objet dont on edite le maillage
					uint32 nameCounter = 0; ///< suffixe des noms par defaut

					// Selection demandee par l'interface (hierarchie), appliquee au
					// gizmo juste avant sa mise a jour. Le gizmo reste la SOURCE de la
					// selection en mode objet -- c'est lui qui pick au clic -- mais la
					// hierarchie doit pouvoir la forcer.
					int32 pendingSelect = -1;
					bool pendingSelectAdd = false;
					bool pendingDeselectAll = false;

					// ── Camera : celle du moteur ────────────────────────────
					NkOrbitCameraController3D cam;
					bool ortho = false;

					// ── Gizmo : celui du moteur ─────────────────────────────
					NkGizmo3D gizmo;
					NkGizmoInput gin{};
					bool wasDragging = false;
					// Correspondance cible du gizmo -> slot d'objet, refaite chaque
					// image. Le gizmo ne connait que des indices contigus ; la scene a
					// des trous (objets supprimes).
					int32 tgtSlot[kMaxObjects];
					int32 tgtCount = 0;

					// ── Mode edition (opere sur l'objet ACTIF) ──────────────
					bool editMode = false;
					uint32 selMask = 1u;
					bool xray = false;
					NkVector<uint8> vertSel;
					NkVector<float32> overlayLines;
					NkVector<float32> wireLines; ///< aretes n-gon, pos3 + rgba4
					NkEditHistory history;
					NkMeshEditRecorder recorder;
					int32 activeVert = -1;
					int32 activeEdgeA = -1, activeEdgeB = -1;
					bool overlayDirty = true;
					bool overlayCleared = false; ///< garde : ClearEditOverlay UNE fois

					// Derniere camera connue. Le picking arrive pendant la PEINTURE,
					// donc avant que la frame 3D suivante n'ait pose sa camera.
					NkVec3f lastCamPos{};
					ScreenProj lastProj{};

					// ── Affichage ───────────────────────────────────────────
					int32 shading = 0;
					int32 solidLight = 0;
					uint32 overlays = 0x0Fu;
					bool gizmoVisible = true;
					// CURSEUR 3D : le point de reference de Blender. Il sert de pivot,
					// d'origine aux nouveaux objets, de centre a la revolution et de
					// cible a « souder au curseur ». Sans lui, ces quatre commandes
					// n'ont aucun point d'application.
					NkVec3f cursor3D{0.f, 0.f, 0.f};
					int32 orientation = 0; ///< 0 global, 1 local, 2 normal
					bool ngonWire = true;

					// ── Transformation modale en cours ──────────────────────
					int32 modalKind = 0;  ///< cf. kVpXform*
					int32 modalAxis = -1; ///< -1 libre, 0 X, 1 Y, 2 Z
					float32 modalStartX = 0.f, modalStartY = 0.f;
					float32 modalCurX = 0.f, modalCurY = 0.f;
					NkVec3f modalPivot{};	 ///< barycentre de la selection, fige au depart
					float32 modalValue = 0.f; ///< la grandeur reglee, pour l'affichage
					char modalLabel[64] = {};
					// Etat de DEPART de chaque objet : annuler doit restaurer
					// exactement, pas « a peu pres ».
					NkVec3f modalPos0[kMaxObjects];
					NkVec3f modalRot0[kMaxObjects];
					NkVec3f modalScl0[kMaxObjects];

					NkVec3f bg{0.05f, 0.05f, 0.07f}; ///< fond de la vue
					bool bgDirty = false;
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

			// ── Transformations ─────────────────────────────────────────────
			inline void ComposeWorld(VpObject &o) {
				o.world = NkMat4f::Translate(o.pos)
						  * NkMat4f::RotationZ(NkAngle::FromRad(o.rotDeg.z * 0.0174532925f))
						  * NkMat4f::RotationY(NkAngle::FromRad(o.rotDeg.y * 0.0174532925f))
						  * NkMat4f::RotationX(NkAngle::FromRad(o.rotDeg.x * 0.0174532925f))
						  * NkMat4f::Scale(o.scl);
			}

			// Decomposition inverse EXACTE de la composition ci-dessus (T * Rz *
			// Ry * Rx * S, stockage colonne-majeur). Elle ne sert qu'a la fin d'un
			// glissement de gizmo : le reste du temps, pos/rot/scl sont la source
			// et personne ne decompose rien.
			inline void DecomposeWorld(const NkMat4f &M, NkVec3f &pos, NkVec3f &rotDeg, NkVec3f &scl) {
				pos = {M.mat[3][0], M.mat[3][1], M.mat[3][2]};
				const NkVec3f c0{M.mat[0][0], M.mat[0][1], M.mat[0][2]};
				const NkVec3f c1{M.mat[1][0], M.mat[1][1], M.mat[1][2]};
				const NkVec3f c2{M.mat[2][0], M.mat[2][1], M.mat[2][2]};
				scl = {c0.Len(), c1.Len(), c2.Len()};
				const float32 sx = scl.x > 1e-8f ? 1.f / scl.x : 0.f;
				const float32 sy = scl.y > 1e-8f ? 1.f / scl.y : 0.f;
				const float32 sz = scl.z > 1e-8f ? 1.f / scl.z : 0.f;
				// R = Rz*Ry*Rx ; en colonne-majeur mat[col][row] :
				//   mat[0][0]=cz*cy  mat[0][1]=sz*cy  mat[0][2]=-sy
				//   mat[1][2]=cy*sx  mat[2][2]=cy*cx
				const float32 r00 = M.mat[0][0] * sx, r01 = M.mat[0][1] * sx, r02 = M.mat[0][2] * sx;
				const float32 r12 = M.mat[1][2] * sy, r22 = M.mat[2][2] * sz;
				float32 syn = -r02;
				if (syn < -1.f)
					syn = -1.f;
				if (syn > 1.f)
					syn = 1.f;
				const float32 kRad2Deg = 57.29577951f;
				rotDeg.y = asinf(syn) * kRad2Deg;
				rotDeg.x = atan2f(r12, r22) * kRad2Deg;
				rotDeg.z = atan2f(r01, r00) * kRad2Deg;
			}

			inline VpObject *ActMesh() {
				if (g.activeObj < 0 || g.activeObj >= kMaxObjects)
					return nullptr;
				VpObject &o = g.objs[g.activeObj];
				return (o.alive && o.type == kVpObjMesh) ? &o : nullptr;
			}

			// Bornes de la scene entiere, pour le recadrage. Une scene vide rend
			// l'origine et un rayon de confort : recadrer sur rien doit quand meme
			// donner un point de vue utilisable.
			inline void SceneBounds(NkVec3f &center, float32 &radius) {
				NkVec3f mn{1e30f, 1e30f, 1e30f}, mx{-1e30f, -1e30f, -1e30f};
				bool any = false;
				for (int32 i = 0; i < kMaxObjects; ++i) {
					const VpObject &o = g.objs[i];
					if (!o.alive)
						continue;
					const float32 r = (o.type == kVpObjMesh) ? o.radius : 0.5f;
					const NkVec3f p = o.pos;
					mn.x = mn.x < p.x - r ? mn.x : p.x - r;
					mn.y = mn.y < p.y - r ? mn.y : p.y - r;
					mn.z = mn.z < p.z - r ? mn.z : p.z - r;
					mx.x = mx.x > p.x + r ? mx.x : p.x + r;
					mx.y = mx.y > p.y + r ? mx.y : p.y + r;
					mx.z = mx.z > p.z + r ? mx.z : p.z + r;
					any = true;
				}
				if (!any) {
					center = {0.f, 0.f, 0.f};
					radius = 2.f;
					return;
				}
				center = (mn + mx) * 0.5f;
				radius = (mx - mn).Len() * 0.5f;
				if (radius < 0.5f)
					radius = 0.5f;
			}

			// ── GENERATEURS DE PRIMITIVES ───────────────────────────────────
			// Ils produisent des TRIANGLES ; BuildFromIndexed(quadify=true) recolle
			// ensuite les paires coplanaires en quads. On ne prend pas les
			// primitives GPU de NkMeshSystem : elles n'ont ni faces ni aretes,
			// donc rien a editer.
			struct GenOut {
					NkVector<NkVertex3D> v;
					NkVector<uint32> i;
					void Vtx(float32 x, float32 y, float32 z) {
						NkVertex3D vv{};
						vv.pos = {x, y, z};
						vv.normal = {0.f, 1.f, 0.f};
						vv.color = 0xFFFFFFFFu;
						v.PushBack(vv);
					}
					void Tri(uint32 a, uint32 b, uint32 c) {
						i.PushBack(a);
						i.PushBack(b);
						i.PushBack(c);
					}
					void Quad(uint32 a, uint32 b, uint32 c, uint32 d) {
						Tri(a, b, c);
						Tri(a, c, d);
					}
			};

			void GenCube(GenOut &o) {
				const float32 h = 1.f;
				const float32 px[8] = {-h, h, h, -h, -h, h, h, -h};
				const float32 py[8] = {-h, -h, -h, -h, h, h, h, h};
				const float32 pz[8] = {-h, -h, h, h, -h, -h, h, h};
				for (uint32 i = 0; i < 8; ++i)
					o.Vtx(px[i], py[i], pz[i]);
				o.Quad(0, 2, 1, 3); // ecrit en triangles ci-dessous pour garder l'ordre historique
				o.i.Clear();
				static const uint32 idx[36] = {
					0, 2, 1, 0, 3, 2, // bas  (-Y)
					4, 5, 6, 4, 6, 7, // haut (+Y)
					0, 1, 5, 0, 5, 4, // avant (-Z)
					3, 7, 6, 3, 6, 2, // arriere (+Z)
					0, 4, 7, 0, 7, 3, // gauche (-X)
					1, 2, 6, 1, 6, 5, // droite (+X)
				};
				for (uint32 k = 0; k < 36; ++k)
					o.i.PushBack(idx[k]);
			}

			void GenPlane(GenOut &o) {
				o.Vtx(-1.f, 0.f, -1.f);
				o.Vtx(1.f, 0.f, -1.f);
				o.Vtx(1.f, 0.f, 1.f);
				o.Vtx(-1.f, 0.f, 1.f);
				o.Quad(0, 3, 2, 1); // normale vers +Y
			}

			void GenSphere(GenOut &o, uint32 seg = 24, uint32 rings = 12) {
				// Sphere UV : deux poles et un treillis de quads entre les deux.
				const float32 kPi = 3.14159265f;
				o.Vtx(0.f, 1.f, 0.f); // pole nord
				for (uint32 r = 1; r < rings; ++r) {
					const float32 phi = kPi * (float32)r / (float32)rings;
					const float32 y = cosf(phi), rad = sinf(phi);
					for (uint32 s = 0; s < seg; ++s) {
						const float32 th = 2.f * kPi * (float32)s / (float32)seg;
						o.Vtx(rad * cosf(th), y, rad * sinf(th));
					}
				}
				o.Vtx(0.f, -1.f, 0.f); // pole sud
				const uint32 south = (uint32)o.v.Size() - 1u;
				auto ring = [&](uint32 r, uint32 s) { return 1u + (r - 1u) * seg + (s % seg); };
				for (uint32 s = 0; s < seg; ++s)
					o.Tri(0u, ring(1, s + 1), ring(1, s));
				for (uint32 r = 1; r + 1 < rings; ++r)
					for (uint32 s = 0; s < seg; ++s)
						o.Quad(ring(r, s), ring(r, s + 1), ring(r + 1, s + 1), ring(r + 1, s));
				for (uint32 s = 0; s < seg; ++s)
					o.Tri(south, ring(rings - 1, s), ring(rings - 1, s + 1));
			}

			void GenCylinder(GenOut &o, uint32 seg = 24) {
				const float32 kPi = 3.14159265f;
				for (uint32 s = 0; s < seg; ++s) {
					const float32 th = 2.f * kPi * (float32)s / (float32)seg;
					o.Vtx(cosf(th), 1.f, sinf(th));
					o.Vtx(cosf(th), -1.f, sinf(th));
				}
				o.Vtx(0.f, 1.f, 0.f);  // centre haut
				o.Vtx(0.f, -1.f, 0.f); // centre bas
				const uint32 cTop = seg * 2u, cBot = seg * 2u + 1u;
				for (uint32 s = 0; s < seg; ++s) {
					const uint32 t0 = s * 2u, b0 = s * 2u + 1u;
					const uint32 t1 = ((s + 1u) % seg) * 2u, b1 = ((s + 1u) % seg) * 2u + 1u;
					o.Quad(t0, t1, b1, b0);	  // flanc
					o.Tri(cTop, t1, t0);	  // couvercle
					o.Tri(cBot, b0, b1);	  // fond
				}
			}

			void GenCone(GenOut &o, uint32 seg = 24) {
				const float32 kPi = 3.14159265f;
				o.Vtx(0.f, 1.f, 0.f); // pointe
				for (uint32 s = 0; s < seg; ++s) {
					const float32 th = 2.f * kPi * (float32)s / (float32)seg;
					o.Vtx(cosf(th), -1.f, sinf(th));
				}
				o.Vtx(0.f, -1.f, 0.f); // centre du fond
				const uint32 cBot = seg + 1u;
				for (uint32 s = 0; s < seg; ++s) {
					const uint32 a = 1u + s, b = 1u + ((s + 1u) % seg);
					o.Tri(0u, b, a);
					o.Tri(cBot, a, b);
				}
			}

			void GenTorus(GenOut &o, uint32 segU = 24, uint32 segV = 12) {
				const float32 kPi = 3.14159265f;
				const float32 R = 1.f, r = 0.35f;
				for (uint32 u = 0; u < segU; ++u) {
					const float32 tu = 2.f * kPi * (float32)u / (float32)segU;
					for (uint32 v = 0; v < segV; ++v) {
						const float32 tv = 2.f * kPi * (float32)v / (float32)segV;
						o.Vtx((R + r * cosf(tv)) * cosf(tu), r * sinf(tv),
							  (R + r * cosf(tv)) * sinf(tu));
					}
				}
				auto at = [&](uint32 u, uint32 v) { return (u % segU) * segV + (v % segV); };
				for (uint32 u = 0; u < segU; ++u)
					for (uint32 v = 0; v < segV; ++v)
						o.Quad(at(u, v), at(u + 1, v), at(u + 1, v + 1), at(u, v + 1));
			}

			// Retriangule l'objet et renvoie le resultat au GPU. Appelee seulement
			// quand son maillage a bouge.
			void SyncMesh(VpObject &o) {
				if (!o.meshDirty || !g.r3 || o.type != kVpObjMesh)
					return;
				auto *meshSys = g.r3->GetMeshSystem();
				if (!meshSys)
					return;
				// TriangulateShaded, pas Triangulate : elle respecte le drapeau
				// `smooth` de chaque face. Une normale moyennee aux coins d'un cube
				// arrondirait visuellement des aretes qui sont vives.
				// LA PILE EST EVALUEE AVANT LA TRIANGULATION. Sans modificateur actif
			// on triangule la base directement : recopier le maillage pour rien
			// couterait a chaque edition.
			bool anyMod = false;
			for (uint32 m = 0; m < o.mods.Count(); ++m)
				if (o.mods.modifiers[m].enabled) {
					anyMod = true;
					break;
				}
			if (anyMod) {
				o.mods.Evaluate(o.edit, o.evaluated);
				o.evaluated.TriangulateShaded(o.triV, o.triI, o.triF);
				// La cage suit le resultat de la pile : c'est ce qu'on voit, donc
				// c'est ce qu'on doit pouvoir viser.
				o.evaluated.Triangulate(o.restV, o.restI, o.restF);
				o.evaluated.GetUniqueEdges(o.edges);
			} else {
				o.edit.TriangulateShaded(o.triV, o.triI, o.triF);
				o.edit.Triangulate(o.restV, o.restI, o.restF);
				o.edit.GetUniqueEdges(o.edges);
			}
				if (o.triV.Empty() || o.triI.Empty())
					return;
				if (o.meshOk) {
					meshSys->UpdateVertices(o.mesh, o.triV.Data(), (uint32)o.triV.Size());
					meshSys->UpdateIndices(o.mesh, o.triI.Data(), (uint32)o.triI.Size());
				} else {
					NkMeshDesc md = NkMeshDesc::Simple(renderer::NkVertexLayout::Default3D(),
													   o.triV.Data(), (uint32)o.triV.Size(),
													   o.triI.Data(), (uint32)o.triI.Size());
					md.keepCPU = true;
					md.debugName = "NK3DModeler.Objet";
					o.mesh = meshSys->Create(md);
					o.meshOk = o.mesh.IsValid();
				}
				// Rayon englobant local, pour le pick d'objet et le recadrage.
				float32 r2max = 0.f;
				for (uint32 i = 0; i < (uint32)o.restV.Size(); ++i) {
					const NkVec3f &p = o.restV[i].pos;
					const float32 r2 = p.x * p.x + p.y * p.y + p.z * p.z;
					if (r2 > r2max)
						r2max = r2;
				}
				o.radius = sqrtf(r2max);
				if (o.radius < 0.1f)
					o.radius = 0.1f;
				// La selection est indexee par sommet : elle doit suivre la taille.
				if (&o == ActMesh()) {
					const uint32 nv = (uint32)o.restV.Size();
					if ((uint32)g.vertSel.Size() != nv) {
						NkVector<uint8> old = g.vertSel;
						g.vertSel.Resize(nv);
						for (uint32 i = 0; i < nv; ++i)
							g.vertSel[i] = (i < (uint32)old.Size()) ? old[i] : (uint8)0;
					}
					g.overlayDirty = true;
				}
				o.meshDirty = false;
			}

			// Apres une operation topologique, la selection qui compte est celle du
			// MAILLAGE : lui seul sait quels sommets il vient de creer.
			void PullSelection() {
				VpObject *A = ActMesh();
				if (!A)
					return;
				const uint32 nv = A->edit.VertCount();
				g.vertSel.Resize(nv);
				for (uint32 i = 0; i < nv; ++i)
					g.vertSel[i] = A->edit.verts[i].sel;
				g.activeVert = -1;
				g.activeEdgeA = g.activeEdgeB = -1;
				g.overlayDirty = true;
			}

			// Du triangle de RENDU a la FACE du n-gon.
			inline NkEmId FaceOfTri(const VpObject &o, uint32 triIndex) {
				return (triIndex < (uint32)o.restF.Size()) ? o.restF[triIndex] : NK_EM_INVALID;
			}

			// ── SELECTION PAR ZONE (Demo3D l. 1143-1220) ────────────────────
			// mode : 0 remplacer, 1 ajouter (Maj), 2 retirer (Ctrl).
			template <class InZone>
			void SelectInZone(int32 mode, InZone inZone) {
				VpObject *A = ActMesh();
				if (!A)
					return;
				const uint32 nv = (uint32)A->restV.Size();
				if ((uint32)g.vertSel.Size() < nv)
					g.vertSel.Resize(nv);
				if (mode == 0)
					for (uint32 i = 0; i < nv; ++i)
						g.vertSel[i] = 0;
				const NkVec3f orgW = A->world * NkVec3f{0.f, 0.f, 0.f};
				auto wpos = [&](uint32 i) { return A->world * A->restV[i].pos; };
				// FILTRE « DOS-CAMERA » ASSOUPLI. Un element de SILHOUETTE a une
				// normale quasi perpendiculaire a la vue : avec un test strict `> 0`
				// il basculait au hasard du bruit numerique. Tolerance -0,2.
				auto facing = [&](NkVec3f w, NkVec3f nLocal) {
					if (g.xray)
						return true;
					const NkVec3f nW = (A->world * nLocal) - orgW;
					const NkVec3f toCam = g.lastCamPos - w;
					const float32 ln = nW.Len(), lv = toCam.Len();
					if (ln < 1e-6f || lv < 1e-6f)
						return true;
					return (nW.Dot(toCam) / (ln * lv)) > -0.2f;
				};
				auto touches = [&](NkVec3f w) {
					float32 px, py;
					return g.lastProj(w, px, py) && inZone(px, py);
				};
				const uint8 on = (mode == 2) ? (uint8)0 : (uint8)1;
				auto apply = [&](uint32 vi) {
					if (vi < (uint32)g.vertSel.Size())
						g.vertSel[vi] = on;
				};
				if (g.selMask & 1u) { // SOMMET
					for (uint32 i = 0; i < nv; ++i)
						if (facing(wpos(i), A->restV[i].normal) && touches(wpos(i)))
							apply(i);
				}
				if (g.selMask & 2u) { // ARETE, testee par son milieu
					for (uint32 e = 0; e + 1 < (uint32)A->edges.Size(); e += 2) {
						const uint32 a = A->edges[e], b = A->edges[e + 1];
						if (a >= nv || b >= nv)
							continue;
						const NkVec3f wa = wpos(a), wb = wpos(b), mid = (wa + wb) * 0.5f;
						if (!facing(mid, A->restV[a].normal) && !facing(mid, A->restV[b].normal))
							continue;
						if (touches(mid)) {
							apply(a);
							apply(b);
						}
					}
				}
				if (g.selMask & 4u) { // FACE, testee par son centre
					NkVector<NkEmId> fvz;
					for (uint32 f = 0; f < (uint32)A->edit.faces.Size(); ++f) {
						// FaceCount() rend la taille de la TABLE : les faces mortes en
						// font partie et leur cycle est casse.
						if (!A->edit.faces[f].alive)
							continue;
						fvz.Clear();
						A->edit.GetFaceVerts(f, fvz);
						if (fvz.Size() < 3)
							continue;
						NkVec3f c{0.f, 0.f, 0.f};
						for (uint32 k = 0; k < (uint32)fvz.Size(); ++k)
							c = c + wpos(fvz[k]);
						c = c * (1.f / (float32)fvz.Size());
						if (!facing(c, A->edit.faces[f].normal))
							continue;
						if (touches(c))
							for (uint32 k = 0; k < (uint32)fvz.Size(); ++k)
								apply(fvz[k]);
					}
				}
				g.overlayDirty = true;
			}

			// Point dans polygone (lasso) : rayon horizontal, regle pair/impair.
			inline bool PointInPoly(const NkVector<float32> &poly, float32 px, float32 py) {
				bool in = false;
				const uint32 n = (uint32)poly.Size() / 2u;
				if (n < 3u)
					return false;
				for (uint32 i = 0, j = n - 1; i < n; j = i++) {
					const float32 ax = poly[i * 2], ay = poly[i * 2 + 1];
					const float32 bx = poly[j * 2], by = poly[j * 2 + 1];
					if (((ay > py) != (by > py)) &&
						(px < (bx - ax) * (py - ay) / ((by - ay) != 0.f ? (by - ay) : 1e-6f) + ax))
						in = !in;
				}
				return in;
			}

			// ── POINT D'ENTREE UNIQUE DE TOUTE MODIFICATION (Demo3D l. 1273) ─
			// Rien ne modifie le maillage sans passer par ici : c'est ce qui
			// garantit que TOUTE operation est annulable et rejouable.
			bool ApplyCmd(NkMeshEditCommand cmd) {
				VpObject *A = ActMesh();
				if (!g.ok || !A)
					return false;
				cmd.selection.Clear();
				for (uint32 i = 0; i < (uint32)g.vertSel.Size(); ++i)
					if (g.vertSel[i])
						cmd.selection.PushBack(i);
				A->edit.SetVertSelection(g.vertSel.Data(), (uint32)g.vertSel.Size());
				NkEditMesh snapshot = A->edit; // pre-etat
				if (!cmd.Apply(A->edit))
					return false; // sans effet : ni annulation ni journal
				g.history.Commit(snapshot);
				g.recorder.Push(cmd);
				PullSelection();
				A->meshDirty = true;
				SyncMesh(*A);
				return true;
			}

			// Contexte de picking sur l'objet actif.
			bool FillPickCtx(NkPickCtx &c) {
				VpObject *A = ActMesh();
				if (!A)
					return false;
				c.mesh = &A->edit;
				c.rest = &A->restV;
				c.tris = &A->restI;
				c.edges = &A->edges;
				c.anchor = A->world;
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
				// la lisibilite sur la richesse de l'image.
				NkRendererConfig cfg =
					NkRendererConfig::ForEditor(g.sharedDev->GetApi(), g.wantW, g.wantH);
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
				// Sans l'override, le graphe rendrait a la taille de la FENETRE dans
				// une cible a la taille de la VUE : la scene paraitrait retrecir en
				// agrandissant le panneau.
				g.r3->SetRenderSizeOverride(g.rtW, g.rtH);
				if (auto *texLib = g.r3->GetTextures())
					g.r3->SetFinalColorTarget(texLib->GetRHIHandle(g.rt->GetColorResult()));
				g.r3->SetBackgroundColor({g.bg.x, g.bg.y, g.bg.z, 1.f});
				// LISERE DE SILHOUETTE, porte de Demo3D : les objets selectionnes
				// sont rendus seuls dans un masque, puis une passe plein-ecran en
				// tire un contour d'epaisseur constante. C'est LE retour visuel de
				// selection -- une teinte modifiee ment des que l'objet est deja
				// colore.
				if (auto *r3d0 = g.r3->GetRender3D())
					r3d0->SetSelectionOutline(true, {1.f, 0.45f, 0.05f, 1.f}, 2.5f);

				// VUE PAR DEFAUT : AU-DESSUS DE LA GRILLE, tournee vers le centre a
				// 45 degres. C'est la pose de Blender, et elle n'est pas
				// arbitraire : de face on ne voit pas le sol, de dessus on ne voit
				// pas les hauteurs. A 45 degres les trois axes sont lisibles d'un
				// coup, ce qui est exactement ce qu'on demande a une vue d'ouverture.
				// Un fichier enregistre imposera SA pose -- c'est le seul cas ou
				// celle-ci ne s'applique pas.
				g.cam.SetCenter({0.f, 0.f, 0.f}, 8.f, 0.7854f, 0.7854f);

				// LA SCENE NAIT VIDE. Le cube de depart de la version precedente
				// venait de Demo3D ; un modeleur commence sur une feuille blanche et
				// « Ajouter » cree ce qu'on lui demande.
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
			// La cible n'est refaite que si la taille CHANGE : le redimensionnement
			// d'une fenetre appelle cette fonction a chaque pixel parcouru.
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

		// ── Fond de la vue ──────────────────────────────────────────────────
		void Viewport3DSetBackground(float32 r, float32 gr, float32 b) {
			g.bg = {r, gr, b};
			g.bgDirty = true;
		}

		// ── Camera ──────────────────────────────────────────────────────────
		void Viewport3DOrbit(float32 dYaw, float32 dPitch) {
			// ORBITE RIGIDE autour du pivot de la selection : position ET cible
			// tournent ensemble, donc aucun saut au premier mouvement.
			if (g.gizmo.HasSelection())
				g.cam.OrbitAroundPivot(g.gizmo.GetPivot(), dYaw, dPitch);
			else
				g.cam.Rotate(dYaw, dPitch);
			// Orbiter librement REPASSE en perspective, comme Blender.
			if (dYaw != 0.f || dPitch != 0.f)
				g.ortho = false;
		}

		void Viewport3DPan(float32 dx, float32 dy) {
			// Axes INVERSES : on tire la scene, on ne deplace pas la camera.
			g.cam.Pan(-dx, -dy);
		}

		void Viewport3DZoom(float32 steps) {
			g.cam.Zoom(steps);
		}

		void Viewport3DPanSteps(float32 dx, float32 dy) {
			// Une crantee de molette vaut ~1 la ou un deplacement de souris vaut
			// 10 a 20 pixels : sans ce facteur, Maj+molette ne bougerait rien.
			g.cam.Pan(dx * 22.f, dy * 22.f);
		}

		void Viewport3DFrameAll() {
			NkVec3f center;
			float32 radius;
			SceneBounds(center, radius);
			const float32 fovY = 45.f * 3.14159265f / 180.f;
			float32 d = (radius * 1.25f) / tanf(fovY * 0.5f);
			if (d < 0.5f)
				d = 0.5f;
			g.cam.SetCenter(center, d, g.cam.GetYaw(), g.cam.GetPitch());
		}

		void Viewport3DAxisView(int32 which, bool opposite) {
			// Vues axiales de Demo3D (l. 2322-2390). Tangage limite a ~89 degres :
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
			// Une vue axiale passe en ORTHOGRAPHIQUE : c'est tout son interet.
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
			// Retenue AUSSI ici : les transformations modales n'ont pas de gizmo a
			// interroger, et elles doivent pourtant respecter le meme repere.
			g.orientation = orient;
		}

		void Viewport3DSetCursor3D(float32 x, float32 y, float32 z) {
			g.cursor3D = {x, y, z};
			g.gizmo.Set3DCursor(g.cursor3D);
		}

		void Viewport3DGetCursor3D(float32 *out3) {
			if (!out3)
				return;
			out3[0] = g.cursor3D.x;
			out3[1] = g.cursor3D.y;
			out3[2] = g.cursor3D.z;
		}

		// Pose le curseur 3D sous la souris : sur la surface visee si le rayon
		// touche un maillage, sinon sur le plan du sol. Un curseur qui refuserait
		// de se poser faute de surface serait inutilisable dans une scene vide.
		void Viewport3DPlaceCursor(float32 mx, float32 my) {
			if (!g.ok)
				return;
			const float32 nx = mx / (float32)(g.rtW ? g.rtW : 1u) * 2.f - 1.f;
			const float32 ny = 1.f - my / (float32)(g.rtH ? g.rtH : 1u) * 2.f;
			NkVec3f dir = g.lastProj.fwd + g.lastProj.rgt * (nx * g.lastProj.thX)
						  + g.lastProj.upv * (ny * g.lastProj.thY);
			const float32 l = dir.Len();
			if (l > 1e-6f)
				dir = dir * (1.f / l);
			float32 best = 1e30f;
			bool hitAny = false;
			for (int32 i = 0; i < kMaxObjects; ++i) {
				VpObject &o = g.objs[i];
				if (!o.alive || !o.visible || o.type != kVpObjMesh)
					continue;
				for (uint32 t = 0; t + 2 < (uint32)o.restI.Size(); t += 3) {
					const NkVec3f v0 = o.world * o.restV[o.restI[t]].pos;
					const NkVec3f v1 = o.world * o.restV[o.restI[t + 1]].pos;
					const NkVec3f v2 = o.world * o.restV[o.restI[t + 2]].pos;
					NkVec3f e1 = v1 - v0, e2 = v2 - v0, h = dir.Cross(e2);
					const float32 aa = e1.Dot(h);
					if (fabsf(aa) < 1e-7f)
						continue;
					const float32 f = 1.f / aa;
					NkVec3f sv = g.lastCamPos - v0;
					const float32 u = f * sv.Dot(h);
					if (u < 0.f || u > 1.f)
						continue;
					NkVec3f q = sv.Cross(e1);
					const float32 vv = f * dir.Dot(q);
					if (vv < 0.f || u + vv > 1.f)
						continue;
					const float32 tt = f * e2.Dot(q);
					if (tt > 1e-4f && tt < best) {
						best = tt;
						hitAny = true;
					}
				}
			}
			if (!hitAny && fabsf(dir.y) > 1e-5f) {
				const float32 t = -g.lastCamPos.y / dir.y;
				if (t > 0.f) {
					best = t;
					hitAny = true;
				}
			}
			if (hitAny) {
				g.cursor3D = g.lastCamPos + dir * best;
				g.gizmo.Set3DCursor(g.cursor3D);
			}
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

		// ── Objets ──────────────────────────────────────────────────────────
		int32 Viewport3DAddObject(int32 type, int32 prim) {
			if (!g.ok && !Init3D())
				return -1;
			int32 slot = -1;
			for (int32 i = 0; i < kMaxObjects; ++i)
				if (!g.objs[i].alive) {
					slot = i;
					break;
				}
			if (slot < 0)
				return -1;
			VpObject &o = g.objs[slot];
			o = VpObject{}; // repart d'une fiche neuve : le slot a pu servir
			o.alive = true;
			o.type = type;
			++g.nameCounter;
			static const char *const kMeshNames[] = {"Cube",	 "Plan", "Sphere",
													 "Cylindre", "Cone", "Tore"};
			const char *base = "Objet";
			switch (type) {
				case kVpObjMesh:
					base = (prim >= 0 && prim < 6) ? kMeshNames[prim] : "Maillage";
					break;
				case kVpObjLightPoint:
					base = "Point";
					break;
				case kVpObjLightSun:
					base = "Soleil";
					break;
				case kVpObjLightSpot:
					base = "Spot";
					break;
				case kVpObjCamera:
					base = "Camera";
					break;
				case kVpObjReference:
					base = "Reference";
					break;
				default:
					base = "Repere";
					break;
			}
			snprintf(o.name, sizeof(o.name), "%s.%03u", base, g.nameCounter);

			if (type == kVpObjReference) {
				// Un simple plan, dresse FACE A LA CAMERA courante : on ajoute une
				// reference pour la voir tout de suite, pas pour la chercher.
				GenOut gen;
				GenPlane(gen);
				o.edit.BuildFromIndexed(gen.v.Data(), (uint32)gen.v.Size(), gen.i.Data(),
										(uint32)gen.i.Size(), true);
				o.edit.RecomputeNormals();
				o.meshDirty = true;
				o.rotDeg = {90.f, 0.f, 0.f}; // le plan est horizontal : on le redresse
				o.scl = {2.f, 1.f, 2.f};
				SyncMesh(o);
			} else if (type == kVpObjMesh) {
				GenOut gen;
				switch (prim) {
					case 1:
						GenPlane(gen);
						break;
					case 2:
						GenSphere(gen);
						break;
					case 3:
						GenCylinder(gen);
						break;
					case 4:
						GenCone(gen);
						break;
					case 5:
						GenTorus(gen);
						break;
					default:
						GenCube(gen);
						break;
				}
				o.edit.BuildFromIndexed(gen.v.Data(), (uint32)gen.v.Size(), gen.i.Data(),
										(uint32)gen.i.Size(), true);
				o.edit.RecomputeNormals();
				o.meshDirty = true;
				SyncMesh(o);
			} else if (type == kVpObjLightPoint || type == kVpObjLightSun ||
					   type == kVpObjLightSpot) {
				o.pos = {0.f, 3.f, 0.f};
				o.lightIntensity = (type == kVpObjLightSun) ? 3.f : 60.f;
				// Un soleil pointe vers le bas par defaut ; un spot aussi.
				o.rotDeg = {-90.f, 0.f, 0.f};
			}
			ComposeWorld(o);
			// Le nouvel objet devient la selection ET l'objet actif : c'est ce
			// qu'on attend apres « Ajouter » -- pouvoir le deplacer tout de suite.
			for (int32 i = 0; i < kMaxObjects; ++i)
				g.objs[i].selected = false;
			o.selected = true;
			g.activeObj = slot;
			g.pendingSelect = slot;
			g.pendingSelectAdd = false;
			return slot;
		}

		int32 Viewport3DObjectCount() {
			return kMaxObjects;
		}

		bool Viewport3DObjectAlive(int32 i) {
			return i >= 0 && i < kMaxObjects && g.objs[i].alive;
		}

		const char *Viewport3DObjectName(int32 i) {
			return Viewport3DObjectAlive(i) ? g.objs[i].name : "";
		}

		void Viewport3DRenameObject(int32 i, const char *name) {
			if (!Viewport3DObjectAlive(i) || !name || !*name)
				return;
			snprintf(g.objs[i].name, sizeof(g.objs[i].name), "%s", name);
		}

		int32 Viewport3DObjectType(int32 i) {
			return Viewport3DObjectAlive(i) ? g.objs[i].type : -1;
		}

		bool Viewport3DObjectSelected(int32 i) {
			return Viewport3DObjectAlive(i) && g.objs[i].selected;
		}

		bool Viewport3DObjectVisible(int32 i) {
			return Viewport3DObjectAlive(i) && g.objs[i].visible;
		}

		void Viewport3DSetObjectVisible(int32 i, bool on) {
			if (Viewport3DObjectAlive(i))
				g.objs[i].visible = on;
		}

		bool Viewport3DObjectLocked(int32 i) {
			return Viewport3DObjectAlive(i) && g.objs[i].locked;
		}

		void Viewport3DSetObjectLocked(int32 i, bool on) {
			if (!Viewport3DObjectAlive(i))
				return;
			g.objs[i].locked = on;
			// Verrouiller DESELECTIONNE : garder selectionne un objet qu'on ne peut
			// plus toucher laisserait le gizmo agir dessus.
			if (on) {
				g.objs[i].selected = false;
				g.pendingDeselectAll = true;
				if (g.activeObj == i)
					g.activeObj = -1;
			}
		}

		void Viewport3DSelectObject(int32 i, bool add) {
			if (!Viewport3DObjectAlive(i) || g.objs[i].locked)
				return;
			// La demande est APPLIQUEE a la prochaine image, juste avant la mise a
			// jour du gizmo : c'est lui qui tient la selection en mode objet, et le
			// mapping cible -> objet n'existe que la.
			g.pendingSelect = i;
			g.pendingSelectAdd = add;
			if (!add)
				for (int32 k = 0; k < kMaxObjects; ++k)
					g.objs[k].selected = false;
			g.objs[i].selected = true;
			g.activeObj = i;
		}

		void Viewport3DDeselectAllObjects() {
			for (int32 k = 0; k < kMaxObjects; ++k)
				g.objs[k].selected = false;
			g.pendingDeselectAll = true;
			g.activeObj = -1;
		}

		int32 Viewport3DActiveObject() {
			return g.activeObj;
		}

		void Viewport3DDeleteObject(int32 i) {
			if (!Viewport3DObjectAlive(i))
				return;
			// Le tampon GPU n'est pas rendu au systeme : NkMeshSystem ne publie pas
			// de destruction unitaire aujourd'hui. Fuite bornee par kMaxObjects --
			// consigne, a resorber quand l'API existera.
			g.objs[i].alive = false;
			g.objs[i].selected = false;
			if (g.activeObj == i) {
				g.activeObj = -1;
				g.editMode = false;
			}
			g.pendingDeselectAll = true;
			g.pendingSelect = -1;
			// LE GIZMO EST VIDE TOUT DE SUITE. Le laisser au prochain passage de
			// frame le faisait survivre au dernier objet supprime : il restait
			// plante au centre d'une scene vide.
			g.gizmo.ClearSelection();
			for (int32 k = 0; k < kMaxObjects; ++k)
				if (g.objs[k].alive) {
					g.objs[k].selected = false;
				}
		}

		bool Viewport3DGetObjectTransform(int32 i, float32 *pos3, float32 *rot3, float32 *scl3) {
			if (!Viewport3DObjectAlive(i))
				return false;
			const VpObject &o = g.objs[i];
			if (pos3) {
				pos3[0] = o.pos.x;
				pos3[1] = o.pos.y;
				pos3[2] = o.pos.z;
			}
			if (rot3) {
				rot3[0] = o.rotDeg.x;
				rot3[1] = o.rotDeg.y;
				rot3[2] = o.rotDeg.z;
			}
			if (scl3) {
				scl3[0] = o.scl.x;
				scl3[1] = o.scl.y;
				scl3[2] = o.scl.z;
			}
			return true;
		}

		void Viewport3DSetObjectTransform(int32 i, const float32 *pos3, const float32 *rot3,
										  const float32 *scl3) {
			if (!Viewport3DObjectAlive(i))
				return;
			VpObject &o = g.objs[i];
			if (pos3)
				o.pos = {pos3[0], pos3[1], pos3[2]};
			if (rot3)
				o.rotDeg = {rot3[0], rot3[1], rot3[2]};
			if (scl3)
				o.scl = {scl3[0], scl3[1], scl3[2]};
			ComposeWorld(o);
		}

		// ── Mode edition et selection ───────────────────────────────────────
		void Viewport3DSetEditMode(bool on) {
			// L'edition demande un objet MAILLAGE actif : entrer en edition sur une
			// lumiere n'a pas de sens, et sur rien encore moins.
			if (on && !ActMesh())
				on = false;
			if (on == g.editMode)
				return;
			g.editMode = on;
			g.overlayDirty = true;
			g.overlayCleared = false;
			if (on) {
				// L'historique est PAR SESSION D'EDITION, comme chez Demo3D : il
				// s'applique a un maillage precis, le garder en changeant d'objet
				// restaurerait la topologie d'un autre.
				g.history.Clear();
				g.recorder.Clear();
				PullSelection();
			} else {
				g.gizmo.ClearSelection();
				// La selection de sommets reste DANS le maillage : y revenir la
				// retrouve, comme dans Blender.
				if (VpObject *A = ActMesh())
					A->edit.SetVertSelection(g.vertSel.Data(), (uint32)g.vertSel.Size());
			}
		}

		bool Viewport3DEditMode() {
			return g.editMode;
		}

		void Viewport3DSetSelectMask(uint32 mask) {
			// Toujours AU MOINS un sous-mode actif : a zero, plus rien n'est
			// selectionnable et l'application parait cassee.
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

		// Clic dans la vue, coordonnees RELATIVES a la vue.
		bool Viewport3DPick(float32 mx, float32 my, bool add, bool toggle) {
			if (!g.ok || !g.editMode)
				return false;
			NkPickCtx c;
			if (!FillPickCtx(c))
				return false;
			const NkPickResult r = NkPickEditElement(c, mx, my);
			VpObject *A = ActMesh();

			// PRIORITE façon Blender : sommet, puis arete, puis face -- chacun dans
			// son propre seuil.
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
			} else if (r.tri >= 0 && A) {
				// Une face selectionnee, ce sont SES SOMMETS marques : la selection
				// est portee par les sommets, aretes et faces s'en deduisent.
				for (uint32 k = 0; k < 3u; ++k)
					touch((int32)A->restI[(uint32)r.tri + k]);
				g.activeVert = -1;
				g.activeEdgeA = g.activeEdgeB = -1;
				got = true;
			}
			g.overlayDirty = true;
			return got;
		}

		// ── SELECTION PAR ZONE ──────────────────────────────────────────────
		void Viewport3DSelectRect(float32 x0, float32 y0, float32 x1, float32 y1, int32 mode) {
			if (!g.ok || !g.editMode)
				return;
			const float32 lo_x = x0 < x1 ? x0 : x1, hi_x = x0 < x1 ? x1 : x0;
			const float32 lo_y = y0 < y1 ? y0 : y1, hi_y = y0 < y1 ? y1 : y0;
			SelectInZone(mode, [&](float32 px, float32 py) {
				return px >= lo_x && px <= hi_x && py >= lo_y && py <= hi_y;
			});
		}

		void Viewport3DSelectCircle(float32 cx, float32 cy, float32 radius, int32 mode) {
			if (!g.ok || !g.editMode)
				return;
			const float32 r2 = radius * radius;
			SelectInZone(mode, [&](float32 px, float32 py) {
				const float32 dx = px - cx, dy = py - cy;
				return dx * dx + dy * dy <= r2;
			});
		}

		void Viewport3DSelectLasso(const float32 *pts, uint32 count, int32 mode) {
			if (!g.ok || !g.editMode || !pts || count < 3u)
				return;
			NkVector<float32> poly;
			poly.Resize(count * 2u);
			for (uint32 i = 0; i < count * 2u; ++i)
				poly[i] = pts[i];
			SelectInZone(mode, [&](float32 px, float32 py) { return PointInPoly(poly, px, py); });
		}

		// ── BOUCLES (Alt+clic) ──────────────────────────────────────────────
		bool Viewport3DSelectLoopAt(float32 mx, float32 my, bool add) {
			if (!g.ok || !g.editMode)
				return false;
			VpObject *A = ActMesh();
			NkPickCtx c;
			if (!A || !FillPickCtx(c))
				return false;
			c.selMask = g.selMask | 2u | 4u; // il faut au moins l'arete et la face
			const NkPickResult r = NkPickEditElement(c, mx, my);

			uint32 la = 0, lb = 0;
			bool faceLoop = false, okp = false;
			if (r.edgeA >= 0) {
				la = (uint32)r.edgeA;
				lb = (uint32)r.edgeB;
				okp = true;
			} else if (r.tri >= 0) {
				const NkEmId f = FaceOfTri(*A, (uint32)r.tri / 3u);
				NkVector<NkEmId> fvl;
				if (f != NK_EM_INVALID)
					A->edit.GetFaceVerts(f, fvl);
				if (fvl.Size() >= 2) {
					la = fvl[0];
					lb = fvl[1];
					faceLoop = true;
					okp = true;
				}
			}
			if (!okp)
				return false;

			const uint32 nv = (uint32)g.vertSel.Size();
			if (!add)
				for (uint32 i = 0; i < nv; ++i)
					g.vertSel[i] = 0;
			if (faceLoop) {
				NkVector<NkEmId> loopF, fvl;
				A->edit.GetFaceLoop(la, lb, loopF);
				for (uint32 k = 0; k < (uint32)loopF.Size(); ++k) {
					fvl.Clear();
					A->edit.GetFaceVerts(loopF[k], fvl);
					for (uint32 j = 0; j < (uint32)fvl.Size(); ++j)
						if (fvl[j] < nv)
							g.vertSel[fvl[j]] = 1;
				}
			} else {
				NkVector<uint32> loopE;
				A->edit.GetEdgeLoop(la, lb, loopE);
				for (uint32 k = 0; k + 1 < (uint32)loopE.Size(); k += 2) {
					if (loopE[k] < nv)
						g.vertSel[loopE[k]] = 1;
					if (loopE[k + 1] < nv)
						g.vertSel[loopE[k + 1]] = 1;
				}
			}
			g.overlayDirty = true;
			return true;
		}

		// ── NAVIGATION MODALE (loupe et main de la colonne de boutons) ──────
		// Cliquer la loupe puis TIRER zoome, cliquer la main puis tirer deplace.
		// C'est le fonctionnement de Blender, et il a un interet concret : sur un
		// portable sans molette ni bouton du milieu, ce sont les seuls acces a la
		// navigation. Le geste s'applique en direct et reste acquis au relachement.
		void Viewport3DNavDrag(int32 kind, float32 dx, float32 dy) {
			if (!g.ok)
				return;
			if (kind == 0)
				g.cam.Zoom(-dy * 0.02f); // vers le haut = approcher
			else
				g.cam.Pan(-dx, -dy);
		}

		void Viewport3DOrbitFree(float32 dYaw, float32 dPitch) {
			// Orbite depuis le GIZMO DE NAVIGATION : autour de la cible courante,
			// jamais du pivot de selection. Le gizmo represente l'ORIENTATION DE LA
			// VUE ; le faire tourner autour d'un objet donnerait un mouvement qui ne
			// correspond pas a ce qu'on manipule.
			g.cam.Rotate(dYaw, dPitch);
			if (dYaw != 0.f || dPitch != 0.f)
				g.ortho = false;
		}

		// ── TRANSFORMATIONS MODALES ─────────────────────────────────────────
		void Viewport3DSetGizmoVisible(bool on) {
			g.gizmoVisible = on;
		}

		void Viewport3DSetNgonWireframe(bool on) {
			g.ngonWire = on;
		}

		int32 Viewport3DModalKind() {
			return g.modalKind;
		}

		int32 Viewport3DModalAxisNow() {
			return g.modalAxis;
		}

		const char *Viewport3DModalLabel() {
			return g.modalLabel;
		}

		void Viewport3DBeginModal(int32 kind, float32 mouseX, float32 mouseY) {
			if (!g.ok || kind == kVpXformNone)
				return;
			// Pivot : le barycentre des objets selectionnes, FIGE au depart. Le
			// recalculer pendant le geste ferait fuir la rotation, puisque les
			// objets qui tournent deplacent leur propre barycentre.
			NkVec3f piv{0.f, 0.f, 0.f};
			uint32 n = 0;
			for (int32 i = 0; i < kMaxObjects; ++i) {
				const VpObject &o = g.objs[i];
				if (!o.alive || !o.selected)
					continue;
				g.modalPos0[i] = o.pos;
				g.modalRot0[i] = o.rotDeg;
				g.modalScl0[i] = o.scl;
				piv = piv + o.pos;
				++n;
			}
			if (!n)
				return; // rien de selectionne : la touche ne fait rien, et c'est juste
			g.modalPivot = piv * (1.f / (float32)n);
			g.modalKind = kind;
			g.modalAxis = -1;
			g.modalStartX = g.modalCurX = mouseX;
			g.modalStartY = g.modalCurY = mouseY;
			g.modalValue = 0.f;
			g.modalLabel[0] = 0;
		}

		void Viewport3DModalAxis(int32 axis) {
			if (g.modalKind == kVpXformNone)
				return;
			// Retaper la meme touche LIBERE l'axe : c'est le comportement de
			// Blender, et il evite d'annuler pour corriger une contrainte posee par
			// erreur.
			g.modalAxis = (g.modalAxis == axis) ? -1 : axis;
			Viewport3DModalUpdate(g.modalCurX, g.modalCurY);
		}

		void Viewport3DModalUpdate(float32 mouseX, float32 mouseY) {
			if (g.modalKind == kVpXformNone)
				return;
			g.modalCurX = mouseX;
			g.modalCurY = mouseY;
			const float32 dx = mouseX - g.modalStartX;
			const float32 dy = mouseY - g.modalStartY;

			// Le repere de l'ECRAN, pour convertir des pixels en monde.
			const NkVec3f rgt = g.lastProj.rgt, upv = g.lastProj.upv;
			// Echelle monde-par-pixel a la distance du pivot : sans elle, un
			// deplacement d'un pixel bougerait autant de pres que de loin.
			const float32 dist = (g.modalPivot - g.lastCamPos).Len();
			const float32 wpp = (dist * 2.f * g.lastProj.thY) / (float32)(g.rtH ? g.rtH : 1u);

			switch (g.modalKind) {
				case kVpXformMove: {
					NkVec3f delta = rgt * (dx * wpp) - upv * (dy * wpp);
					if (g.modalAxis >= 0) {
						// CONTRAINTE D'AXE : on projette le deplacement libre sur
						// l'axe. Projeter le MOUVEMENT (et non prendre bêtement dx)
						// donne le bon sens quel que soit l'angle de la camera --
						// tirer vers la droite quand X pointe a gauche doit reculer.
						NkVec3f ax{0.f, 0.f, 0.f};
						((float32 *)&ax)[g.modalAxis] = 1.f;
						const float32 t = delta.Dot(ax);
						delta = ax * t;
						g.modalValue = t;
					} else {
						g.modalValue = delta.Len();
					}
					for (int32 i = 0; i < kMaxObjects; ++i) {
						VpObject &o = g.objs[i];
						if (!o.alive || !o.selected)
							continue;
						o.pos = g.modalPos0[i] + delta;
						ComposeWorld(o);
					}
					snprintf(g.modalLabel, sizeof(g.modalLabel), "Deplacement %s : %.3f",
							 g.modalAxis == 0 ? "X" : (g.modalAxis == 1 ? "Y" : (g.modalAxis == 2 ? "Z" : "libre")),
							 g.modalValue);
					break;
				}
				case kVpXformRotate: {
					// L'ANGLE VIENT DE LA ROTATION DU CURSEUR AUTOUR DU PIVOT, pas
					// d'un simple dx : c'est ce qui donne la sensation de tenir
					// l'objet. On projette le pivot a l'ecran et on mesure l'angle
					// balaye entre le point de depart et le point courant.
					float32 px = 0.f, py = 0.f;
					g.lastProj(g.modalPivot, px, py);
					const float32 a0 = atan2f(g.modalStartY - py, g.modalStartX - px);
					const float32 a1 = atan2f(mouseY - py, mouseX - px);
					float32 da = a1 - a0;
					const float32 kPi = 3.14159265f;
					while (da > kPi)
						da -= 2.f * kPi;
					while (da < -kPi)
						da += 2.f * kPi;
					// Sens : a l'ecran l'axe Y descend, donc l'angle mesure tourne a
					// l'envers de la convention mathematique.
					float32 deg = -da * 57.29577951f;
					int32 axis = g.modalAxis;
					if (axis < 0) {
						// Sans contrainte, on tourne autour de l'axe de VUE -- c'est
						// ce que fait Blender, et c'est le seul axe pour lequel le
						// geste circulaire a un sens a l'ecran. On le ramene a l'axe
						// du monde le plus proche pour rester sur des angles d'Euler.
						const NkVec3f f = g.lastProj.fwd;
						const float32 ax2 = fabsf(f.x), ay2 = fabsf(f.y), az2 = fabsf(f.z);
						axis = (ax2 > ay2) ? ((ax2 > az2) ? 0 : 2) : ((ay2 > az2) ? 1 : 2);
						const float32 sgn = ((const float32 *)&f)[axis];
						if (sgn > 0.f)
							deg = -deg;
					}
					g.modalValue = deg;
					for (int32 i = 0; i < kMaxObjects; ++i) {
						VpObject &o = g.objs[i];
						if (!o.alive || !o.selected)
							continue;
						o.rotDeg = g.modalRot0[i];
						((float32 *)&o.rotDeg)[axis] += deg;
						// Rotation AUTOUR DU PIVOT : la position tourne aussi, sinon
						// une selection multiple pivoterait sur place au lieu de
						// tourner en groupe.
						const NkVec3f rel = g.modalPos0[i] - g.modalPivot;
						const float32 c = cosf(deg * 0.0174532925f), sn = sinf(deg * 0.0174532925f);
						NkVec3f rot = rel;
						if (axis == 0) {
							rot.y = rel.y * c - rel.z * sn;
							rot.z = rel.y * sn + rel.z * c;
						} else if (axis == 1) {
							rot.x = rel.x * c + rel.z * sn;
							rot.z = -rel.x * sn + rel.z * c;
						} else {
							rot.x = rel.x * c - rel.y * sn;
							rot.y = rel.x * sn + rel.y * c;
						}
						o.pos = g.modalPivot + rot;
						ComposeWorld(o);
					}
					snprintf(g.modalLabel, sizeof(g.modalLabel), "Rotation %s : %.1f deg",
							 axis == 0 ? "X" : (axis == 1 ? "Y" : "Z"), deg);
					break;
				}
				default: { // ECHELLE
					// LE FACTEUR EST LE RAPPORT DES DISTANCES AU PIVOT, a l'ecran.
					// C'est la definition de Blender, et c'est la seule qui soit
					// stable : un facteur tire de dx seul dependrait de la direction
					// du geste.
					float32 px = 0.f, py = 0.f;
					g.lastProj(g.modalPivot, px, py);
					const float32 d0 = sqrtf((g.modalStartX - px) * (g.modalStartX - px)
											 + (g.modalStartY - py) * (g.modalStartY - py));
					const float32 d1 = sqrtf((mouseX - px) * (mouseX - px)
											 + (mouseY - py) * (mouseY - py));
					float32 f = (d0 > 1e-3f) ? (d1 / d0) : 1.f;
					if (f < 0.001f)
						f = 0.001f;
					g.modalValue = f;
					NkVec3f fv{f, f, f};
					if (g.modalAxis >= 0) {
						fv = {1.f, 1.f, 1.f};
						((float32 *)&fv)[g.modalAxis] = f;
					}
					for (int32 i = 0; i < kMaxObjects; ++i) {
						VpObject &o = g.objs[i];
						if (!o.alive || !o.selected)
							continue;
						// L'ECHELLE PORTE SUR LES AXES DE L'OBJET, pas sur ceux du
						// monde : `scl` est appliquee AVANT la rotation dans la
						// composition (T * R * S). En repere GLOBAL avec un objet
						// tourne, contraindre a X doit donc etirer selon le X DU MONDE,
						// ce qui n'est pas representable par un facteur d'echelle
						// local -- on prend alors l'axe local le plus proche du X
						// monde. C'est l'approximation de tous les modeleurs qui
						// gardent une echelle non cisaillante ; l'exact demanderait
						// une matrice de deformation par objet.
						NkVec3f use = fv;
						if (g.modalAxis >= 0 && g.orientation == 0) {
							const NkVec3f col0{o.world.mat[0][0], o.world.mat[0][1],
											   o.world.mat[0][2]};
							const NkVec3f col1{o.world.mat[1][0], o.world.mat[1][1],
											   o.world.mat[1][2]};
							const NkVec3f col2{o.world.mat[2][0], o.world.mat[2][1],
											   o.world.mat[2][2]};
							NkVec3f wax{0.f, 0.f, 0.f};
							((float32 *)&wax)[g.modalAxis] = 1.f;
							const float32 d0 = fabsf(col0.Normalized().Dot(wax));
							const float32 d1 = fabsf(col1.Normalized().Dot(wax));
							const float32 d2 = fabsf(col2.Normalized().Dot(wax));
							const int32 la = (d0 > d1) ? ((d0 > d2) ? 0 : 2) : ((d1 > d2) ? 1 : 2);
							use = {1.f, 1.f, 1.f};
							((float32 *)&use)[la] = f;
						}
						o.scl = {g.modalScl0[i].x * use.x, g.modalScl0[i].y * use.y,
								 g.modalScl0[i].z * use.z};
						// Le DEPLACEMENT autour du pivot, lui, reste en repere MONDE :
						// c'est la position dans la scene qui s'ecarte, pas une
						// dimension de l'objet.
						const NkVec3f rel = g.modalPos0[i] - g.modalPivot;
						o.pos = g.modalPivot
								+ NkVec3f{rel.x * fv.x, rel.y * fv.y, rel.z * fv.z};
						ComposeWorld(o);
					}
					snprintf(g.modalLabel, sizeof(g.modalLabel), "Echelle %s : %.3f",
							 g.modalAxis == 0 ? "X" : (g.modalAxis == 1 ? "Y" : (g.modalAxis == 2 ? "Z" : "libre")),
							 f);
					break;
				}
			}
			(void)dy;
		}

		void Viewport3DModalConfirm() {
			// Les valeurs sont deja ecrites dans les objets : confirmer, c'est
			// simplement cesser de pouvoir annuler.
			g.modalKind = kVpXformNone;
			g.modalAxis = -1;
			g.modalLabel[0] = 0;
		}

		void Viewport3DModalCancel() {
			if (g.modalKind == kVpXformNone)
				return;
			for (int32 i = 0; i < kMaxObjects; ++i) {
				VpObject &o = g.objs[i];
				if (!o.alive || !o.selected)
					continue;
				o.pos = g.modalPos0[i];
				o.rotDeg = g.modalRot0[i];
				o.scl = g.modalScl0[i];
				ComposeWorld(o);
			}
			g.modalKind = kVpXformNone;
			g.modalAxis = -1;
			g.modalLabel[0] = 0;
		}

		// ── PILE DE MODIFICATEURS ───────────────────────────────────────────
		uint32 Viewport3DModifierTypeCount() {
			return 17u; // cf. NkModifierType, en ajout seul
		}

		const char *Viewport3DModifierTypeName(int32 type) {
			return NkModifierTypeName((NkModifierType)type);
		}

		int32 Viewport3DAddModifier(int32 type) {
			VpObject *A = ActMesh();
			if (!A || type < 0 || type >= 17)
				return -1;
			NkMeshModifier m;
			m.type = (NkModifierType)type;
			m.enabled = true;
			A->mods.Add(m);
			A->meshDirty = true;
			SyncMesh(*A);
			return (int32)A->mods.Count() - 1;
		}

		uint32 Viewport3DModifierCount() {
			VpObject *A = ActMesh();
			return A ? A->mods.Count() : 0u;
		}

		int32 Viewport3DModifierTypeAt(uint32 index) {
			VpObject *A = ActMesh();
			if (!A || index >= A->mods.Count())
				return -1;
			return (int32)A->mods.modifiers[index].type;
		}

		bool Viewport3DModifierEnabled(uint32 index) {
			VpObject *A = ActMesh();
			return A && index < A->mods.Count() && A->mods.modifiers[index].enabled;
		}

		void Viewport3DSetModifierEnabled(uint32 index, bool on) {
			VpObject *A = ActMesh();
			if (!A || index >= A->mods.Count())
				return;
			A->mods.SetEnabled(index, on);
			A->meshDirty = true;
			SyncMesh(*A);
		}

		bool Viewport3DRemoveModifier(uint32 index) {
			VpObject *A = ActMesh();
			if (!A || !A->mods.Remove(index))
				return false;
			A->meshDirty = true;
			SyncMesh(*A);
			return true;
		}

		bool Viewport3DMoveModifier(uint32 index, bool up) {
			// L'ORDRE EST UN PARAMETRE DE RESULTAT : miroir puis reseau ne donne
			// pas la meme chose que reseau puis miroir.
			VpObject *A = ActMesh();
			if (!A)
				return false;
			const bool okm = up ? A->mods.MoveUp(index) : A->mods.MoveDown(index);
			if (!okm)
				return false;
			A->meshDirty = true;
			SyncMesh(*A);
			return true;
		}

		bool Viewport3DApplyModifier(uint32 index) {
			VpObject *A = ActMesh();
			if (!A || index >= A->mods.Count())
				return false;
			// DESTRUCTIF : on prend donc un instantane pour que Ctrl+Z le defasse.
			NkEditMesh snapshot = A->edit;
			bool warnNotFirst = false;
			if (!A->mods.ApplyToBase(index, A->edit, &warnNotFirst))
				return false;
			g.history.Commit(snapshot);
			A->meshDirty = true;
			SyncMesh(*A);
			return true;
		}

		uint32 Viewport3DModifierParamCount(uint32 index) {
			VpObject *A = ActMesh();
			if (!A || index >= A->mods.Count())
				return 0u;
			uint32 n = 0;
			NkModifierParams(A->mods.modifiers[index].type, n);
			return n;
		}

		bool Viewport3DModifierParamInfo(uint32 index, uint32 p, const char **label, int32 *type,
										 float32 *minV, float32 *maxV) {
			VpObject *A = ActMesh();
			if (!A || index >= A->mods.Count())
				return false;
			uint32 n = 0;
			const NkModParam *ps = NkModifierParams(A->mods.modifiers[index].type, n);
			if (!ps || p >= n)
				return false;
			if (label)
				*label = ps[p].label;
			if (type)
				*type = (int32)ps[p].type;
			if (minV)
				*minV = ps[p].minV;
			if (maxV)
				*maxV = ps[p].maxV;
			return true;
		}

		// Lecture/ecriture par DECALAGE dans la structure : c'est ce que publie
		// NkModParam, et c'est ce qui permet a l'interface d'ignorer quels champs
		// existent. Les vecteurs sont exposes par leur composante X -- une interface
		// vectorielle viendra avec l'editeur de parametres.
		float32 Viewport3DGetModifierParam(uint32 index, uint32 p) {
			VpObject *A = ActMesh();
			if (!A || index >= A->mods.Count())
				return 0.f;
			uint32 n = 0;
			const NkModParam *ps = NkModifierParams(A->mods.modifiers[index].type, n);
			if (!ps || p >= n)
				return 0.f;
			const uint8 *base = (const uint8 *)&A->mods.modifiers[index];
			const void *field = base + ps[p].offset;
			switch (ps[p].type) {
				case NkModParamType::Bool:
					return *(const bool *)field ? 1.f : 0.f;
				case NkModParamType::Int:
					return (float32)(*(const int32 *)field);
				case NkModParamType::Vec3:
					return ((const NkVec3f *)field)->x;
				default:
					return *(const float32 *)field;
			}
		}

		void Viewport3DSetModifierParam(uint32 index, uint32 p, float32 v) {
			VpObject *A = ActMesh();
			if (!A || index >= A->mods.Count())
				return;
			uint32 n = 0;
			const NkModParam *ps = NkModifierParams(A->mods.modifiers[index].type, n);
			if (!ps || p >= n)
				return;
			uint8 *base = (uint8 *)&A->mods.modifiers[index];
			void *field = base + ps[p].offset;
			switch (ps[p].type) {
				case NkModParamType::Bool:
					*(bool *)field = (v != 0.f);
					break;
				case NkModParamType::Int:
					*(int32 *)field = (int32)(v + (v < 0.f ? -0.5f : 0.5f));
					break;
				case NkModParamType::Vec3:
					((NkVec3f *)field)->x = v;
					break;
				default:
					*(float32 *)field = v;
					break;
			}
			A->meshDirty = true;
			SyncMesh(*A);
		}

		void Viewport3DCameraAxes(float32 *right3, float32 *up3, float32 *fwd3) {
			// Le gizmo de navigation de l'interface doit TOURNER AVEC LA VUE, comme
			// celui de Blender : ses axes se projettent avec la meme base que la
			// scene, sinon il indique une orientation qui n'est pas celle qu'on
			// regarde -- et il devient un decor.
			if (right3) {
				right3[0] = g.lastProj.rgt.x;
				right3[1] = g.lastProj.rgt.y;
				right3[2] = g.lastProj.rgt.z;
			}
			if (up3) {
				up3[0] = g.lastProj.upv.x;
				up3[1] = g.lastProj.upv.y;
				up3[2] = g.lastProj.upv.z;
			}
			if (fwd3) {
				fwd3[0] = g.lastProj.fwd.x;
				fwd3[1] = g.lastProj.fwd.y;
				fwd3[2] = g.lastProj.fwd.z;
			}
		}

		// ── OPERATIONS D'EDITION ────────────────────────────────────────────
		bool Viewport3DExtrude(bool individual) {
			// EXTRUSION SELON LE SOUS-MODE ACTIF : face > arete > sommet, decalage
			// ZERO comme Blender -- la nouvelle geometrie nait sur l'ancienne et
			// c'est l'utilisateur qui la deplace ensuite au gizmo.
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
			// REPLI : Blender extrude « ce qui est selectionne », pas « ce que le
			// sous-mode designe ».
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
			// Delta PAR SOMMET : c'est ce qui permettra l'edition proportionnelle.
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
			VpObject *A = ActMesh();
			if (!g.ok || !A || !g.history.CanUndo())
				return false;
			g.history.Undo(A->edit);
			PullSelection();
			A->meshDirty = true;
			SyncMesh(*A);
			return true;
		}

		bool Viewport3DRedo() {
			VpObject *A = ActMesh();
			if (!g.ok || !A || !g.history.CanRedo())
				return false;
			g.history.Redo(A->edit);
			PullSelection();
			A->meshDirty = true;
			SyncMesh(*A);
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
			VpObject *A = ActMesh();
			if (!g.ok || !A) {
				verts = edges = faces = tris = 0u;
				return;
			}
			verts = A->edit.VertCount();
			edges = A->edit.EdgeCount();
			// FaceCount() rend la TAILLE DE LA TABLE, faces mortes comprises. La
			// triangulation ne visite que les vivantes : compter les changements
			// d'identifiant dans triF donne le compte exact.
			uint32 alive = 0u;
			NkEmId prev = (NkEmId)0xFFFFFFFFu;
			for (uint32 t = 0; t < (uint32)A->restF.Size(); ++t) {
				if (A->restF[t] != prev) {
					++alive;
					prev = A->restF[t];
				}
			}
			faces = alive;
			tris = (uint32)(A->triI.Size() / 3u);
		}

		void Viewport3DRenderOffscreen(void *cmdv) {
			if (!Init3D())
				return;
			NkICommandBuffer *cmd = (NkICommandBuffer *)cmdv;
			auto *r3d = g.r3->GetRender3D();
			if (!cmd || !r3d)
				return;

			// Fond : applique seulement quand il change -- le setter du moteur
			// reconstruit le graphe, ce n'est pas un reglage par image.
			if (g.bgDirty) {
				g.bgDirty = false;
				g.r3->SetBackgroundColor({g.bg.x, g.bg.y, g.bg.z, 1.f});
			}

			for (int32 i = 0; i < kMaxObjects; ++i)
				if (g.objs[i].alive && g.objs[i].type == kVpObjMesh)
					SyncMesh(g.objs[i]);

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
			// Le plan proche suit la distance : fixe, il gaspille la precision du
			// tampon de profondeur des que la scene est grande.
			cd.nearPlane = dist * 0.005f;
			if (cd.nearPlane < 0.01f)
				cd.nearPlane = 0.01f;
			cd.farPlane = dist * 20.f + 100.f;
			NkCamera3D cam(cd);
			g.cam.Apply(cam);
			if (g.ortho)
				cam.SetOrtho(true, dist * 0.55f);
			else
				cam.SetOrtho(false);

			g.gizmo.SetCamera(cam.GetPosition(), cam.GetTarget(), 45.f, (float32)g.rtW,
							  (float32)g.rtH);
			g.lastCamPos = cam.GetPosition();
			g.lastProj = ScreenProj::Make(cam.GetPosition(), cam.GetTarget(), 45.f, (float32)g.rtW,
										  (float32)g.rtH);

			// ── CIBLES DU GIZMO ─────────────────────────────────────────────
			// En mode edition : une cible unique, le barycentre de la selection de
			// sommets. En mode objet : TOUS les objets vivants -- c'est le gizmo qui
			// pick au clic (modele Demo3D), et la hierarchie force sa selection via
			// pendingSelect.
			{
				NkGizmoTarget vt[kMaxObjects];
				g.tgtCount = 0;
				if (g.editMode) {
					VpObject *A = ActMesh();
					NkVec3f piv{0.f, 0.f, 0.f};
					uint32 n = 0;
					if (A)
						for (uint32 i = 0;
							 i < (uint32)g.vertSel.Size() && i < (uint32)A->restV.Size(); ++i)
							if (g.vertSel[i]) {
								const NkVec3f w = A->world * A->restV[i].pos;
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
						g.tgtSlot[0] = g.activeObj;
						g.tgtCount = 1;
					}
					if (g.tgtCount) {
						if (!g.gizmo.HasSelection())
							g.gizmo.Select(0);
						g.gizmo.Update(vt, 1, g.gin);
					} else {
						g.gizmo.ClearSelection();
					}
				} else {
					// PENDANT UN GLISSEMENT, la cible passee au gizmo est la matrice
					// FIGEE au debut du geste (dragBase), jamais le monde recompose :
					// decomposer puis recomposer chaque image accumule les arrondis
					// d'Euler, et le gizmo « quittait l'objet » en cours de geste.
					const bool dragNow = g.gizmo.IsDragging();
					for (int32 i = 0; i < kMaxObjects; ++i) {
						VpObject &o = g.objs[i];
						if (!o.alive || !o.visible || o.locked)
							continue; // verrouille : hors d'atteinte du gizmo
						if (!(dragNow && o.selected))
							ComposeWorld(o);
						vt[g.tgtCount].base = (dragNow && o.selected) ? o.dragBase : o.world;
						const float32 r = (o.type == kVpObjMesh) ? o.radius : 0.45f;
						vt[g.tgtCount].localHalf = {r, r, r};
						vt[g.tgtCount].pickRadius = r;
						g.tgtSlot[g.tgtCount] = i;
						++g.tgtCount;
					}
					// Demande de la hierarchie, appliquee ici ou le mapping existe.
					if (g.pendingDeselectAll) {
						g.pendingDeselectAll = false;
						g.gizmo.ClearSelection();
					}
					if (g.pendingSelect >= 0) {
						for (int32 t = 0; t < g.tgtCount; ++t)
							if (g.tgtSlot[t] == g.pendingSelect) {
								if (g.pendingSelectAdd)
									g.gizmo.AddToSelection(t);
								else
									g.gizmo.Select(t);
								break;
							}
						g.pendingSelect = -1;
					}
					if (g.tgtCount) {
						// Pendant une transformation MODALE, le gizmo ne recoit rien :
						// deux mecanismes qui deplacent le meme objet en meme temps se
						// contrediraient.
						NkGizmoInput gi = g.gin;
						if (g.modalKind != kVpXformNone) {
							gi.leftPressed = false;
							gi.leftDown = false;
							gi.mouseDX = 0.f;
							gi.mouseDY = 0.f;
						}
						g.gizmo.Update(vt, g.tgtCount, gi);
					} else {
						g.gizmo.ClearSelection();
					}

					// La selection du gizmo redescend dans les objets : en mode
					// objet c'est LUI qui pick, la hierarchie ne fait que lire.
					for (int32 t = 0; t < g.tgtCount; ++t)
						g.objs[g.tgtSlot[t]].selected = g.gizmo.IsSelected(t);
					const int32 act = g.gizmo.ActiveIndex();
					if (act >= 0 && act < g.tgtCount)
						g.activeObj = g.tgtSlot[act];

					// ── Application des transformations ─────────────────
					// Pendant le glissement : la matrice affichee est base + delta
					// du gizmo. Au RELACHEMENT : on decompose une fois vers
					// pos/rot/scl -- la source de verite -- puis on remet le gizmo
					// a zero. Decomposer a chaque image ferait deriver les champs
					// par accumulation d'erreurs d'arrondi.
					const bool dragging = g.gizmo.IsDragging();
					if (dragging && !g.wasDragging)
						for (int32 t = 0; t < g.tgtCount; ++t)
							g.objs[g.tgtSlot[t]].dragBase = g.objs[g.tgtSlot[t]].world;
					if (dragging) {
						// Le monde AFFICHE suit le geste ; pos/rot/scl ne sont PAS
						// touches -- ils ne le seront qu'une fois, au relachement.
						// Les reecrire ici puis recomposer a l'image suivante faisait
						// deriver l'objet ET le gizmo par accumulation d'arrondis.
						for (int32 t = 0; t < g.tgtCount; ++t)
							if (g.gizmo.IsSelected(t)) {
								VpObject &o = g.objs[g.tgtSlot[t]];
								o.world = g.gizmo.Apply(t, o.dragBase);
							}
					} else if (g.wasDragging) {
						for (int32 t = 0; t < g.tgtCount; ++t)
							if (g.gizmo.IsSelected(t)) {
								VpObject &o = g.objs[g.tgtSlot[t]];
								o.world = g.gizmo.Apply(t, o.dragBase);
								DecomposeWorld(o.world, o.pos, o.rotDeg, o.scl);
								ComposeWorld(o);
							}
						g.gizmo.ResetSelected();
					}
					g.wasDragging = dragging;
				}
			}

			// ── Grille ──────────────────────────────────────────────────────
			const bool wantGrid = (g.overlays & kOvGrid) != 0u;
			r3d->SetInfiniteGridEnabled(wantGrid || (g.overlays & kOvAxes) != 0u);
			NkInfiniteGridParams gp = r3d->GetInfiniteGridParams();
			gp.showMinor = wantGrid;
			gp.showMajor = wantGrid;
			gp.showAxes = (g.overlays & kOvAxes) != 0u;
			gp.cellSize = 1.f;
			gp.majorEvery = 10.f;
			r3d->SetInfiniteGridParams(gp);

			// ── Mode d'affichage, calque sur Blender ────────────────────────
			// SOLIDE et FIL DE FER sont des vues d'ATELIER : elles ignorent les
			// lumieres de la scene -- c'est un eclairage d'etude fixe, et il serait
			// faux d'y voir l'effet d'un spot qu'on vient de poser. MATERIAU montre
			// les materiaux sous un eclairage neutre, et RENDU est le seul mode ou
			// les lumieres de la scene agissent.
			//   0 Solide   -> NK_UNLIT (albedo plat, eclairage d'etude)
			//   1 Materiau -> NK_SOLID sous eclairage d'ATELIER (pas celui de la scene)
			//   2 Rendu    -> NK_SOLID sous les lumieres DE LA SCENE
			//   3 Filaire  -> NK_WIREFRAME, sans lumiere par definition
			const bool wire = (g.shading == 3);
			const bool sceneLit = (g.shading == 2); // seul RENDU voit les lumieres
			r3d->SetWireframe(wire);
			// FILAIRE N-GON : on ne montre que les VRAIES aretes. Un quad est rendu
			// en deux triangles, mais sa diagonale n'existe pas topologiquement --
			// l'afficher donnerait un cube a 18 aretes au lieu de 12, ce qui n'est
			// ni ce que Blender montre ni ce qu'on peut selectionner.
			// UNIQUEMENT EN MODE FILAIRE. Actif en permanence, il remplacait le
			// rendu des surfaces par des lignes -- et comme on vidait le lot hors
			// mode filaire, les objets devenaient INVISIBLES tout en gardant leur
			// silhouette dans le masque de selection. C'est exactement ce que Rihen
			// decrivait : un maillage qu'on ne voit pas mais dont le contour de
			// selection apparait.
			r3d->SetNgonWireframe(wire && g.ngonWire);
			if (wire && g.ngonWire) {
				g.wireLines.Clear();
				const NkVec4f wc{0.75f, 0.78f, 0.82f, 1.f};
				for (int32 i = 0; i < kMaxObjects; ++i) {
					const VpObject &o = g.objs[i];
					if (!o.alive || !o.visible || o.type != kVpObjMesh)
						continue;
					for (uint32 e = 0; e + 1 < (uint32)o.edges.Size(); e += 2) {
						const uint32 a = o.edges[e], b = o.edges[e + 1];
						if (a >= (uint32)o.restV.Size() || b >= (uint32)o.restV.Size())
							continue;
						const NkVec3f wa = o.world * o.restV[a].pos;
						const NkVec3f wb = o.world * o.restV[b].pos;
						const float32 pts[2][3] = {{wa.x, wa.y, wa.z}, {wb.x, wb.y, wb.z}};
						for (int32 k = 0; k < 2; ++k) {
							g.wireLines.PushBack(pts[k][0]);
							g.wireLines.PushBack(pts[k][1]);
							g.wireLines.PushBack(pts[k][2]);
							g.wireLines.PushBack(wc.x);
							g.wireLines.PushBack(wc.y);
							g.wireLines.PushBack(wc.z);
							g.wireLines.PushBack(wc.w);
						}
					}
				}
				r3d->SetNgonWireLines(g.wireLines.Empty() ? nullptr : g.wireLines.Data(),
									  (uint32)(g.wireLines.Size() / 7u));
			} else {
				r3d->ClearNgonWire();
			}

			NkSceneContext sctx;
			sctx.camera = cam;
			sctx.viewMode = wire ? NkViewMode::NK_WIREFRAME
								 : ((g.shading >= 1) ? NkViewMode::NK_SOLID
													 : NkViewMode::NK_UNLIT);
			// ── Lumieres : celles de la SCENE d'abord ───────────────────────
			// L'eclairage d'atelier (cle chaude + remplissage froid) ne sert que
			// tant que l'utilisateur n'a cree AUCUNE lumiere : des la premiere, la
			// scene est a lui. Melanger les deux rendrait ses reglages illisibles.
			bool anyUserLight = false;
			for (int32 i = 0; sceneLit && i < kMaxObjects; ++i) {
				// Hors du mode RENDU, les lumieres de la scene n'entrent pas dans le
				// contexte : Solide, Materiau et Filaire gardent leur eclairage
				// d'etude quoi que contienne la scene.
				const VpObject &o = g.objs[i];
				if (!o.alive || !o.visible)
					continue;
				if (o.type != kVpObjLightPoint && o.type != kVpObjLightSun &&
					o.type != kVpObjLightSpot)
					continue;
				NkLightDesc L;
				L.color = o.lightColor;
				L.intensity = o.lightIntensity;
				L.position = o.pos;
				// La direction est l'axe -Y de l'objet tourne : une lumiere neuve
				// pointe vers le bas, et la tourner au gizmo l'oriente.
				const NkVec3f dir = {-o.world.mat[1][0], -o.world.mat[1][1], -o.world.mat[1][2]};
				L.direction = dir;
				L.castShadow = false;
				switch (o.type) {
					case kVpObjLightSun:
						L.type = NkLightType::NK_DIRECTIONAL;
						break;
					case kVpObjLightSpot:
						L.type = NkLightType::NK_SPOT;
						L.range = o.lightRange;
						break;
					default:
						L.type = NkLightType::NK_POINT;
						L.range = o.lightRange;
						break;
				}
				sctx.lights.PushBack(L);
				anyUserLight = true;
			}
			if (!anyUserLight) {
				NkLightDesc key;
				key.type = NkLightType::NK_DIRECTIONAL;
				key.direction = {-0.4f, -0.8f, -0.5f};
				key.color = {1.f, 0.97f, 0.92f};
				key.intensity = 3.f;
				key.castShadow = false;
				sctx.lights.PushBack(key);
				NkLightDesc fill;
				fill.type = NkLightType::NK_DIRECTIONAL;
				fill.direction = {0.5f, -0.3f, 0.6f};
				fill.color = {0.62f, 0.7f, 0.9f};
				fill.intensity = 1.1f;
				fill.castShadow = false;
				sctx.lights.PushBack(fill);
			}
			sctx.iblIntensity = 1.f;
			sctx.ambientIntensity = 0.45f;
			r3d->BeginScene(sctx);

			// ── OVERLAY D'EDITION (Demo3D l. 5647-5762) ─────────────────────
			// Cage de GetUniqueEdges, couleur PAR EXTREMITE interpolee par le GPU,
			// AUCUN decalage geometrique -- le depth-bias des passes overlay traite
			// deja le z-fighting.
			VpObject *A = ActMesh();
			if (g.editMode && A) {
				g.overlayCleared = false;
				if (g.overlayDirty) {
					g.overlayDirty = false;
					const NkVec4f cageCol{0.015f, 0.015f, 0.02f, 1.f};
					const NkVec4f selEdgeCol{1.f, 0.70f, 0.13f, 1.f};
					const NkVec4f actCol{1.f, 1.f, 1.f, 1.f};
					auto pushV = [](NkVector<float32> &Aq, NkVec3f p, NkVec4f c) {
						Aq.PushBack(p.x);
						Aq.PushBack(p.y);
						Aq.PushBack(p.z);
						Aq.PushBack(c.x);
						Aq.PushBack(c.y);
						Aq.PushBack(c.z);
						Aq.PushBack(c.w);
					};
					g.overlayLines.Clear();
					g.overlayLines.Reserve((uint32)A->edges.Size() * 7u);
					const uint32 ns = (uint32)g.vertSel.Size();
					// Passe 0 = aretes neutres, passe 1 = aretes touchees (tracees
					// APRES : elles gagnent le z-fight).
					for (int32 pass = 0; pass < 2; ++pass) {
						for (uint32 e = 0; e + 1 < (uint32)A->edges.Size(); e += 2) {
							const uint32 a = A->edges[e], b = A->edges[e + 1];
							if (a >= ns || b >= ns)
								continue;
							const bool selA = (g.vertSel[a] != 0), selB = (g.vertSel[b] != 0);
							if ((selA || selB) != (pass == 1))
								continue;
							// ARETE ACTIVE : blanche sur TOUTE sa longueur.
							const bool actE =
								(g.activeEdgeA >= 0 &&
								 (((int32)a == g.activeEdgeA && (int32)b == g.activeEdgeB) ||
								  ((int32)a == g.activeEdgeB && (int32)b == g.activeEdgeA)));
							auto vcol = [&](uint32 v, bool sel) -> NkVec4f {
								if (actE)
									return actCol;
								if (sel && (int32)v == g.activeVert)
									return actCol;
								return sel ? selEdgeCol : cageCol;
							};
							pushV(g.overlayLines, A->world * A->restV[a].pos, vcol(a, selA));
							pushV(g.overlayLines, A->world * A->restV[b].pos, vcol(b, selB));
						}
					}
					r3d->SetEditOverlayLines(g.overlayLines.Empty() ? nullptr
																	: g.overlayLines.Data(),
											 (uint32)(g.overlayLines.Size() / 7u));
					// Marqueurs en triangles pleins plus bas : le point-sprite est
					// peu fiable en DX12.
					r3d->SetEditOverlayPoints(nullptr, 0);
					r3d->SetEditOverlayTris(nullptr, 0);
					r3d->SetEditOverlayXray(g.xray);
				}
			} else if (!g.overlayCleared) {
				// UNE seule fois. L'appeler chaque image invalidait le lot overlay
				// en continu -- c'est ce qui faisait CLIGNOTER le gizmo, qui partage
				// ce chemin de rendu.
				r3d->ClearEditOverlay();
				g.overlayCleared = true;
			}

			// ── SOUMISSION DES OBJETS ───────────────────────────────────────
			for (int32 i = 0; i < kMaxObjects; ++i) {
				VpObject &o = g.objs[i];
				if (!o.alive || !o.visible)
					continue;
				if ((o.type == kVpObjMesh || o.type == kVpObjReference) && o.meshOk) {
					NkDrawCall3D dc;
					dc.mesh = o.mesh;
					dc.transform = o.world;
					dc.tint = {0.78f, 0.78f, 0.80f};
					dc.metallic = 0.f;
					dc.roughness = 0.62f;
					dc.castShadow = false;
					const float32 r = o.radius * (o.scl.x > o.scl.y ? (o.scl.x > o.scl.z ? o.scl.x : o.scl.z)
																	 : (o.scl.y > o.scl.z ? o.scl.y : o.scl.z));
					dc.aabb = NkAABB{{o.pos.x - r, o.pos.y - r, o.pos.z - r},
									 {o.pos.x + r, o.pos.y + r, o.pos.z + r}};
					r3d->Submit(dc);
					// LISERE : la silhouette est soumise avec la matrice EXACTE qui a
					// servi au rendu de cette image -- pendant un glissement, toute
					// autre matrice decale le contour de l'objet. L'actif ressort
					// d'une teinte plus claire, comme Blender.
					if (o.selected)
						r3d->SubmitSelection(dc, i == g.activeObj);
				} else if (o.type != kVpObjMesh && o.type != kVpObjReference) {
					// ── Widgets des objets sans surface ─────────────────
					// Une lumiere, une camera ou un repere n'ont rien a rendre :
					// sans widget ils seraient invisibles ET introuvables. Traces en
					// overlay, taille fixe en monde -- ce sont des reperes de scene,
					// pas des marqueurs d'ecran.
					const NkVec4f col = o.selected ? NkVec4f{1.f, 0.70f, 0.13f, 1.f}
												   : NkVec4f{0.85f, 0.85f, 0.55f, 1.f};
					const NkVec3f p = o.pos;
					auto line = [&](NkVec3f a2, NkVec3f b2) {
						r3d->DrawDebugLine(a2, b2, col, 0.f, true);
					};
					const float32 s = 0.28f;
					if (o.type == kVpObjEmpty) {
						const NkVec4f cx{0.95f, 0.35f, 0.32f, 1.f}, cy{0.25f, 0.75f, 0.30f, 1.f},
							cz{0.35f, 0.65f, 0.95f, 1.f};
						r3d->DrawDebugLine(p - NkVec3f{s, 0, 0}, p + NkVec3f{s, 0, 0},
										   o.selected ? col : cx, 0.f, true);
						r3d->DrawDebugLine(p - NkVec3f{0, s, 0}, p + NkVec3f{0, s, 0},
										   o.selected ? col : cy, 0.f, true);
						r3d->DrawDebugLine(p - NkVec3f{0, 0, s}, p + NkVec3f{0, 0, s},
										   o.selected ? col : cz, 0.f, true);
					} else if (o.type == kVpObjCamera) {
						// Petit tronc de pyramide : la base regarde -Z local.
						const NkVec3f fwd{-o.world.mat[2][0], -o.world.mat[2][1],
										  -o.world.mat[2][2]};
						const NkVec3f rgt{o.world.mat[0][0], o.world.mat[0][1], o.world.mat[0][2]};
						const NkVec3f up{o.world.mat[1][0], o.world.mat[1][1], o.world.mat[1][2]};
						const NkVec3f tip = p + fwd * 0.75f;
						const NkVec3f c1 = tip + rgt * 0.4f + up * 0.28f;
						const NkVec3f c2 = tip + rgt * 0.4f - up * 0.28f;
						const NkVec3f c3 = tip - rgt * 0.4f - up * 0.28f;
						const NkVec3f c4 = tip - rgt * 0.4f + up * 0.28f;
						line(p, c1);
						line(p, c2);
						line(p, c3);
						line(p, c4);
						line(c1, c2);
						line(c2, c3);
						line(c3, c4);
						line(c4, c1);
					} else {
						// Lumieres : une etoile pour le point, une fleche pour le
						// soleil, un cone pour le spot.
						const NkVec3f dir{-o.world.mat[1][0], -o.world.mat[1][1],
										  -o.world.mat[1][2]};
						line(p - NkVec3f{s, 0, 0}, p + NkVec3f{s, 0, 0});
						line(p - NkVec3f{0, s, 0}, p + NkVec3f{0, s, 0});
						line(p - NkVec3f{0, 0, s}, p + NkVec3f{0, 0, s});
						if (o.type == kVpObjLightSun) {
							line(p, p + dir * 1.2f);
						} else if (o.type == kVpObjLightSpot) {
							const NkVec3f rgt{o.world.mat[0][0], o.world.mat[0][1],
											  o.world.mat[0][2]};
							const NkVec3f fw2{o.world.mat[2][0], o.world.mat[2][1],
											  o.world.mat[2][2]};
							const NkVec3f e = p + dir * 1.0f;
							line(p, e + rgt * 0.45f);
							line(p, e - rgt * 0.45f);
							line(p, e + fw2 * 0.45f);
							line(p, e - fw2 * 0.45f);
						}
					}
				}
			}

			// ── AXES DU MONDE ───────────────────────────────────────────────
			// La grille du moteur ne trace que X (rouge) et Z (bleu) : elle vit dans
			// le PLAN du sol, donc l'axe vertical n'y a pas sa place. Le Y vert
			// manquait pour cette raison exacte, et Demo3D le trace separement en
			// vraie ligne 3D (l. 6706-6712). On fait pareil.
			if (g.overlays & kOvAxes) {
				const float32 A = 1000.f;
				const float32 h = 0.02f; // au-dessus du sol : pas de z-fight
				r3d->DrawDebugLine({-A, h, 0.f}, {A, h, 0.f}, {1.f, 0.f, 0.f, 1.f});
				r3d->DrawDebugLine({0.f, -A, 0.f}, {0.f, A, 0.f}, {0.f, 1.f, 0.f, 1.f});
				r3d->DrawDebugLine({0.f, h, -A}, {0.f, h, A}, {0.f, 0.f, 1.f, 1.f});
			}

			// ── CURSEUR 3D ──────────────────────────────────────────────────
			// Cercle en pointilles + croix, de taille CONSTANTE A L'ECRAN : c'est un
			// repere, il doit rester lisible de pres comme de loin.
			{
				const NkVec3f c3 = g.cursor3D;
				const float32 rr = (c3 - g.lastCamPos).Len() * 0.016f;
				const NkVec3f rg = g.lastProj.rgt, up = g.lastProj.upv;
				const NkVec4f cw{1.f, 1.f, 1.f, 1.f}, cr{0.85f, 0.1f, 0.1f, 1.f};
				const int32 kSeg = 16;
				for (int32 k = 0; k < kSeg; ++k) {
					// Un segment sur deux : le pointille distingue le curseur d'une
					// sphere de scene.
					if (k & 1)
						continue;
					const float32 a0 = 6.2831853f * (float32)k / (float32)kSeg;
					const float32 a1 = 6.2831853f * (float32)(k + 1) / (float32)kSeg;
					const NkVec3f p0 = c3 + rg * (cosf(a0) * rr) + up * (sinf(a0) * rr);
					const NkVec3f p1 = c3 + rg * (cosf(a1) * rr) + up * (sinf(a1) * rr);
					r3d->DrawDebugLine(p0, p1, (k % 4 == 0) ? cw : cr, 0.f, true);
				}
				r3d->DrawDebugLine(c3 - rg * (rr * 1.6f), c3 + rg * (rr * 1.6f), cw, 0.f, true);
				r3d->DrawDebugLine(c3 - up * (rr * 1.6f), c3 + up * (rr * 1.6f), cw, 0.f, true);
			}

			// ── MARQUEURS DE SOMMETS ────────────────────────────────────────
			if (g.editMode && A && (g.selMask & 1u)) {
				const NkVec3f rgt = g.lastProj.rgt, upv = g.lastProj.upv;
				const uint32 ns = (uint32)g.vertSel.Size();
				for (uint32 i = 0; i < ns && i < (uint32)A->restV.Size(); ++i) {
					const NkVec3f w = A->world * A->restV[i].pos;
					const float32 hs = (w - g.lastCamPos).Len() * 0.0035f;
					const NkVec4f col = ((int32)i == g.activeVert)
											? NkVec4f{1.f, 1.f, 1.f, 1.f}
											: (g.vertSel[i] ? NkVec4f{1.f, 0.70f, 0.13f, 1.f}
															: NkVec4f{0.05f, 0.05f, 0.06f, 1.f});
					const NkVec3f a2 = w - rgt * hs - upv * hs, b2 = w + rgt * hs - upv * hs;
					const NkVec3f c2 = w + rgt * hs + upv * hs, d2 = w - rgt * hs + upv * hs;
					r3d->DrawDebugTriangle(a2, b2, c2, col, 0.f, true);
					r3d->DrawDebugTriangle(a2, c2, d2, col, 0.f, true);
				}
			}

			// ── GIZMO ───────────────────────────────────────────────────────
			// Le gizmo disparait avec l'outil Selection (rien a transformer) et
			// pendant une modale (le geste ne passe pas par lui).
			if (g.gizmoVisible && g.modalKind == kVpXformNone && g.gizmo.HasSelection())
				g.gizmo.Draw(
					[&](NkVec3f a2, NkVec3f b2, NkVec4f c2) {
						r3d->DrawDebugLine(a2, b2, c2, 0.f, true);
					},
					[&](NkVec3f a2, NkVec3f b2, NkVec3f c2, NkVec4f col) {
						r3d->DrawDebugTriangle(a2, b2, c2, col, 0.f, true);
					});

			// Le graphe fait tout et ecrit dans NOTRE cible.
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
