// =============================================================================
// Demo3D.cpp  — Demo 2
//
// Demo minimaliste 3D :
//   - Config ForGame (RENDER3D + RENDER2D + TEXT + SHADOW + POST_PROCESS + OVERLAY)
//   - Camera 3D orbite autour de l'origine
//   - Mesh primitives : sol (plane) + 4x4 sphere grid + cube central
//   - 1 lumiere directionnelle + 2 lumieres ponctuelles colorees
//
// Demontre le path complet : NkScene/Lights/DrawCalls -> Render3D::Submit
//                            -> RenderGraph -> Flush.
// =============================================================================
#include "DemoCommon.h"
#include "NKWindow/Core/NkWESystem.h" // NkEvents()
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"	   // NkMouseWheelVerticalEvent, NkMouseButton
#include "NKEvent/NkEventDispatcher.h" // NkInput (IsMouseDown / MouseDeltaX/Y / IsKeyDown)
#include "NKRenderer/Tools/Shadow/NkShadowSystem.h"
#include "NKRenderer/Tools/Shadow/NkVirtualShadowMaps.h"
#include "NKRenderer/Core/NkCameraController.h" // NkOrbitCameraController3D / NkFlyCameraController3D
#include "NKRenderer/Core/NkGizmo.h"			// NkGizmo3D (gizmo éditeur réutilisable)
#include "NKImage/NKImage.h"					// Phase H : test ecriture PNG procedural
#include "NKContainers/Associative/NkHashMap.h" // dedup arêtes Edit Mode
#include "NKRenderer/Mesh/NkEditMesh.h"			// structure demi-arête n-gon
#include "NKFileSystem/NkFile.h"				// save/load session d'édition (journal de commandes)
#include <cstdio>

namespace nkentseu {
	namespace demo {

		struct Demo3DState {
				NkMeshHandle meshSphere;
				NkMeshHandle meshPlane;
				NkMeshHandle meshCube;
				NkTexHandle cookieWindow; // E.6 : cookie 2D pour le spot
				NkTexHandle cookieCube;	  // E.6b : cookie cube pour le point red
				// NkVSM v2 : panneau "feuillage" alpha-teste (validation ombre trouee).
				NkMaterial *maskedMat = nullptr;
				NkTexHandle maskedTex;
				float32 angle = 0.f;	  // orbite camera
				// Mode d'affichage viewport (touche Z, façon Blender) : 0=RENDERED (PBR éclairé),
				// 1=SOLID (unlit, phare caméra), 2=WIREFRAME (unlit + fil de fer).
				int32 shadingMode = 0;
				// Source de COULEUR en modes unlit (SOLID/WIREFRAME) et en EDIT MODE, façon
				// option "Color" du mode Solid de Blender. 0=MATERIAL (couleur du matériau),
				// 1=GRIS uniforme (défaut), 2=CUSTOM (couleur choisie). Touche B pour cycler.
				int32 unlitColorMode = 1;
				NkVec3f unlitGray = {0.38f, 0.39f, 0.42f}; // gris moyen-foncé façon Blender
				NkVec3f unlitCustom = {0.45f, 0.62f, 0.85f};
				// Vues axiales (pavé num 1/3/7 = front/right/top…) : façon Blender, on passe en
				// PROJECTION ORTHO. Une orbite libre (clic-milieu) rebascule en perspective.
				bool orthoView = false;
				// Option (touche G off? non — touche dédiée) : montrer la grille en vue axiale ortho.
				bool viewGridInOrtho = true;
				// Panel debug : index PCF mode courant pour cycle (P)
				int32 pcfIdx = (int32)NkPCFMode::PCF5x5;
				// Phase H : true si le PNG a ete charge avec succes (vs fallback procedural).
				bool phaseHLoadOk = false;
				// [ / ] maintenus : ajustent le bias en continu (taux x dt) via l'update
				// frame (le poll clavier NkInput est casse sur Win32 -> on tracke l'etat
				// presse/relache a la main).
				bool biasUpHeld = false;   // ] enfonce
				bool biasDownHeld = false; // [ enfonce
				// ── Caméras réutilisables du moteur (NkCameraController.h) ──
				// ÉDITEUR (Blender) : orbit = milieu ; pan = Shift+milieu ; zoom = molette.
				renderer::NkOrbitCameraController3D editorCam;
				// SIMULATION (jeu/archviz) : fly/FPS (WASD + regard clic-droit).
				renderer::NkFlyCameraController3D simCam;
				bool useSimCam = false;	  // F = bascule éditeur/simulation
				float64 wheelAccum = 0.0; // molette accumulée (callback -> frame)
				// ── Sélection + gizmo (composant moteur réutilisable NkGizmo3D) ──
				bool pickPending = false;	// front montant du clic gauche (callback)
				int32 pickX = 0, pickY = 0; // position écran du clic (pixels)
				// Delta souris RÉEL par frame = (pos courante - pos précédente). NE PAS utiliser
				// NkInput.MouseDelta*() : ce delta d'événement n'est PAS remis à 0 sans mouvement
				// (valeur périmée conservée) -> le gizmo continuait de transformer souris immobile.
				float32 lastMouseX = 0.f, lastMouseY = 0.f;
				bool mouseTracked = false; // 1re frame : pas de delta
				// Indices des cibles : 16 sphères, 1 cube, 2 colonnes, 64 instanciés = 83.
				static const int32 kNumObj = 16 + 1 + 2 + 64;
				renderer::NkGizmo3D gizmo; // sélection multiple + translate/rotate/scale/combiné
				// Mesh ÉDITÉ propre à un objet (persiste l'édition) : si valide, l'objet est
				// rendu avec CE mesh au lieu de sa primitive partagée. Rempli à la SORTIE d'edit.
				NkMeshHandle objMesh[kNumObj]{};
				// ── Volet 2 : EDIT MODE mesh (édition façon Blender, sur l'OBJET SÉLECTIONNÉ) ──
				// TAB entre en édition de l'objet actif (gizmo.ActiveIndex). On CLONE ses
				// données CPU (modèle Blender : le CPU est l'autorité) en un mesh dynamique
				// propre -> éditer une sphère ne déforme pas les autres. Les vertices sont
				// édités en espace LOCAL ; l'ancre (transform monde de l'objet) sert au
				// rendu, au pick et aux marqueurs.
				NkMeshHandle editMesh;					 // clone dynamique du mesh édité
				NkVector<renderer::NkVertex3D> editRest; // vertices LOCAUX de repos (autorité CPU)
				NkVector<renderer::NkVertex3D> editLive; // rest + delta (re-upload pendant drag)
				NkVector<uint32> editIdx;				 // topologie (triangles)
				NkVector<uint32> editEdges;				 // arêtes UNIQUES (paires) pour la cage — bâti à l'entrée
				renderer::NkEditMesh editHE;			 // AUTORITÉ topologie (n-gon demi-arête)
				renderer::NkEditHistory editHistory;	 // undo/redo (mémento : snapshots de editHE)
				renderer::NkEditMesh editDragSnap;		 // pré-état capturé au DÉBUT d'un drag de sommets
				bool editDragSnapValid = false;
				renderer::NkMeshEditRecorder editRecorder; // journal des commandes (rejeu / modificateurs / IA)
				renderer::NkEditMesh editBase;			   // maillage de BASE (à l'entrée) pour le rejeu
				renderer::NkModifierStack editModifiers; // pile de modificateurs NON-DESTRUCTIFS (Mirror/Array/Subsurf)
				int32 editReplayStep = -1;				 // P : rejeu PAS-À-PAS (-1=off, 0=base, k=base+k commandes)
				bool editSavePending = false;			 // F5 : sauver la session (journal) sur disque
				bool editLoadPending = false;			 // F6 : charger une session + rejouer
				int32 editAddModPending = -1;			 // F7/F8/F9 : ajouter modificateur (0=Mirror 1=Array 2=Subsurf)
				bool editClearModPending = false;		 // F10 : vider la pile de modificateurs
				int32 editActiveMod = -1;				 // index du modificateur en cours de réglage
				int32 editModAdjustPending = 0;			 // [ / ] : ajuster (-1 / +1) le param principal
				bool editModCyclePending = false;		 // \ : changer de modificateur actif
				NkVector<renderer::NkEmId> editTriFace;	 // map triangle de rendu -> face n-gon (pick)
				NkVector<uint8> vertSel;				 // 1 = vertex sélectionné (taille = nb vertices)
				NkMat4f editAnchor = NkMat4f::Identity(); // transform monde de l'objet
				NkMat4f editAnchorInv = NkMat4f::Identity();
				int32 editObjIdx = -1;						 // objet en cours d'édition (index gizmo)
				NkVec3f editObjTint = {0.75f, 0.78f, 0.85f}; // matériau capturé de l'objet
				float32 editObjMetallic = 0.f;
				float32 editObjRoughness = 0.7f;
				int32 editSelMask = 1;			  // bits : 1=VERTEX 2=EDGE 4=FACE (touches 1/2/3 ; Shift+ = combiner)
				int32 editActiveVert = -1;		  // sommet ACTIF (dernier sélectionné) = rendu BLANC façon Blender
				bool editXray = false;			  // Alt+Z : voir/sélectionner à travers (façon Blender)
				bool editMode = false;			  // TAB : bascule objet <-> édition
				bool editTogglePending = false;	  // TAB traité côté frame (accès meshSys)
				bool editWasDragging = false;	  // pour baker le delta en fin de drag
				bool editOverlayDirty = true;	  // reconstruire les buffers overlay (cage/points/faces)
				bool editExtrudePending = false;  // E : extrude région (traité côté frame)
				bool editDeletePending = false;	  // X : supprime faces (traité côté frame)
				bool editMergePending = false;	  // M : soude les vertices sélectionnés
				bool editMakeFacePending = false; // F : crée une face (n-gon) depuis la sélection
				bool editSubdivPending = false;	  // W : subdivise les faces sélectionnées
				bool editLoopCutPending = false;  // Ctrl+R : boucle d'arêtes (loop cut)
			int32 loopCuts = 1;				  // Ctrl+Shift+R : nb de boucles insérées (1..5)
				bool editUndoPending = false;	  // Ctrl+Z : annuler (traité côté frame)
				bool editRedoPending = false;	  // Ctrl+Shift+Z / Ctrl+Y : rétablir
				bool editReplayPending = false;	  // P : rejoue le journal depuis la base
				// Couteau/bisect (K) : trace une ligne (2 clics) -> plan de coupe.
				bool knifeArmed = false;
				bool knifeHasP0 = false;
				NkVec2f knifeP0 = {0.f, 0.f};
				bool editBisectPending = false; // couteau : plan prêt -> couper (côté frame)
				NkVec3f bisectPt = {0.f, 0.f, 0.f}, bisectN = {0.f, 1.f, 0.f};
				// Propriétés des outils (façon Blender) — réglées par Shift+touche, affichées au HUD.
				int32 subdivCuts = 1;			// nb d'itérations de subdivision (Shift+W)
				bool extrudeIndividual = false; // Extrude : région (0) vs faces individuelles (1) (Shift+E)
				int32 mergeMode = 0;			// 0=CENTER 1=FIRST 2=LAST (Shift+M)
				renderer::NkGizmo3D editGizmo;	// 1 seule cible = centroïde de la sélection
		};

		// Transform de BASE (repos, sans animation) d'un objet de la démo par son index
		// gizmo — MÊME disposition que la boucle de soumission. Sert d'ancre à l'Edit Mode.
		static NkMat4f Demo3D_ObjBase(int32 idx) {
			if (idx >= 0 && idx < 16) {
				int32 row = idx / 4, col = idx % 4;
				return NkMat4f::Translate({(col - 1.5f) * 1.2f, 0.5f, (row - 1.5f) * 1.2f}) *
					   NkMat4f::Scale({0.45f, 0.45f, 0.45f});
			}
			if (idx == 16)
				return NkMat4f::Translate({0.f, 0.5f, 0.f}) * NkMat4f::Scale({0.6f, 0.6f, 0.6f}); // cube central (figé)
			if (idx == 17)
				return NkMat4f::Translate({-4.f, 1.f, -2.f}) * NkMat4f::Scale({0.3f, 2.f, 0.3f}); // colonne 0
			if (idx == 18)
				return NkMat4f::Translate({1.f, 1.f, 4.f}) * NkMat4f::Scale({0.3f, 2.f, 0.3f}); // colonne 1
			if (idx >= 19 && idx < 83) {
				int32 g = idx - 19, gz = g / 8, gx = g % 8;
				return NkMat4f::Translate({(gx - 3.5f) * 0.55f, 1.6f, (gz - 3.5f) * 0.55f - 4.5f}) *
					   NkMat4f::Scale({0.18f, 0.18f, 0.18f});
			}
			return NkMat4f::Identity();
		}

		// Matériau (tint/metallic/roughness) d'un objet de la démo par son index — MÊMES
		// valeurs que la boucle de soumission. Sert à rendre le mesh ÉDITÉ avec le matériau
		// d'origine de l'objet (en RENDERED : couleur PBR de l'objet, pas un gris fixe).
		static void Demo3D_ObjMaterial(int32 idx, NkVec3f &tint, float32 &metallic, float32 &roughness) {
			if (idx >= 0 && idx < 16) {
				int32 row = idx / 4, col = idx % 4;
				tint = {(float32)col / 3.f, (float32)row / 3.f, 0.7f};
				metallic = (float32)col / 3.f;
				roughness = 0.05f + (float32)row / 3.f * 0.95f;
				return;
			}
			if (idx == 16) {
				tint = {1.f, 0.8f, 0.3f};
				metallic = 1.f;
				roughness = 0.15f;
				return;
			} // cube or
			if (idx == 17 || idx == 18) {
				tint = {0.7f, 0.7f, 0.7f};
				metallic = 0.f;
				roughness = 0.6f;
				return;
			}
			if (idx >= 19 && idx < 83) {
				int32 g = idx - 19, gz = g / 8, gx = g % 8;
				tint = {(float32)gx / 7.f, 0.6f, (float32)gz / 7.f};
				metallic = 0.f;
				roughness = 0.6f;
				return;
			}
			tint = {0.75f, 0.78f, 0.85f};
			metallic = 0.f;
			roughness = 0.7f;
		}

		// ── Outils d'édition de topologie (Phase C) ──────────────────────────────────
		// Recalcule les normales par vertex = moyenne (pondérée par l'aire) des normales
		// de face. À appeler après toute déformation / changement de topologie.
		// ── Édition sur structure HALF-EDGE / n-gon (AUTORITÉ = editHE) ───────────────
		// Régénère le mesh de RENDU (triangulation de editHE) + cage + map tri->face n-gon,
		// et recrée le mesh GPU dynamique.
		static void Demo3D_SyncFromHE(Demo3DState *st, renderer::NkMeshSystem *ms) {
			// BASE (editHE) -> pick + cage + overlay + sélection : on édite TOUJOURS la base,
			// même quand des modificateurs sont affichés (façon cage Blender : la cage = la base).
			st->editHE.Triangulate(st->editRest, st->editIdx, st->editTriFace);
			st->editLive = st->editRest;
			st->editActiveVert = -1; // la topologie a pu changer -> l'index actif n'est plus fiable
			st->editHE.GetUniqueEdges(st->editEdges);
			if ((uint32)st->vertSel.Size() != st->editHE.VertCount())
				st->vertSel.Resize(st->editHE.VertCount());
			// AFFICHAGE SOLIDE = base, OU résultat des modificateurs (non-destructif : editHE
			// n'est jamais modifiée ; on triangule le résultat juste pour le mesh GPU affiché).
			const renderer::NkVertex3D *dvData = st->editRest.Data();
			uint32 dvCount = (uint32)st->editRest.Size();
			const uint32 *diData = st->editIdx.Data();
			uint32 diCount = (uint32)st->editIdx.Size();
			renderer::NkEditMesh evalMesh;
			NkVector<renderer::NkVertex3D> evV;
			NkVector<uint32> evI;
			NkVector<renderer::NkEmId> evTF;
			if (!st->editModifiers.Empty()) {
				st->editModifiers.Evaluate(st->editHE, evalMesh);
				evalMesh.Triangulate(evV, evI, evTF);
				dvData = evV.Data();
				dvCount = (uint32)evV.Size();
				diData = evI.Data();
				diCount = (uint32)evI.Size();
			}
			if (st->editMesh.IsValid())
				ms->Release(st->editMesh);
			renderer::NkMeshDesc d =
				renderer::NkMeshDesc::Simple(renderer::NkVertexLayout::Default3D(), dvData, dvCount, diData, diCount);
			d.dynamic = true;
			d.debugName = "Demo3D_EditMesh";
			st->editMesh = ms->Create(d);
			st->editOverlayDirty = true;
		}

		// Face n-gon d'un triangle de rendu — map EXACTE (produite par Triangulate).
		static renderer::NkEmId Demo3D_FaceOfTri(Demo3DState *st, uint32 triIdx) {
			return (triIdx < (uint32)st->editTriFace.Size()) ? st->editTriFace[triIdx] : renderer::NK_EM_INVALID;
		}

		// ── P2 — ORIENTATION « NORMAL » DU GIZMO EN EDIT MODE (façon Blender) ────────
		// Calcule le repère de l'élément sélectionné et le pousse dans le gizmo :
		//   Z = normale, X = tangente (une arête de l'élément), Y = Z x X.
		//   • FACE   -> Z = normale de la face,       tangente = 1re arête de la face
		//   • ARÊTE  -> Z = moyenne des normales des faces adjacentes, tangente = direction
		//   • SOMMET -> Z = normale du sommet (moyenne des faces incidentes), tangente = une
		//               arête incidente
		//   • sélection MULTIPLE -> MOYENNE des normales (comme Blender).
		// Priorité FACE > ARÊTE > SOMMET selon le masque actif, avec repli si le niveau
		// prioritaire n'a rien de sélectionné. Tout est converti en espace MONDE (le gizmo
		// raisonne en monde) via l'ancre de l'objet édité.
		static void Demo3D_UpdateEditNormalFrame(Demo3DState *st) {
			auto &M = st->editHE;
			const NkVec3f org = st->editAnchor * NkVec3f{0.f, 0.f, 0.f};
			auto dirW = [&](NkVec3f d) { return (st->editAnchor * d) - org; };
			auto vsel = [&](uint32 i) { return i < (uint32)st->vertSel.Size() && st->vertSel[i] != 0; };
			NkVec3f n{0.f, 0.f, 0.f}, t{0.f, 0.f, 0.f};
			bool haveT = false;
			NkVector<renderer::NkEmId> fvn;
			// 1) FACES entièrement sélectionnées.
			if (st->editSelMask & 4) {
				for (uint32 f = 0; f < (uint32)M.faces.Size(); f++) {
					if (!M.faces[f].alive || !M.FaceIsSelected(f))
						continue;
					n = n + M.faces[f].normal;
					if (!haveT) {
						fvn.Clear();
						M.GetFaceVerts(f, fvn);
						if (fvn.Size() >= 2) {
							t = M.verts[fvn[1]].pos - M.verts[fvn[0]].pos;
							haveT = true;
						}
					}
				}
			}
			// 2) ARÊTES sélectionnées : normale = moyenne des faces adjacentes.
			if (n.Len() < 1e-6f) {
				for (uint32 e = 0; e + 1 < (uint32)st->editEdges.Size(); e += 2) {
					const uint32 a = st->editEdges[e], b = st->editEdges[e + 1];
					if (!vsel(a) || !vsel(b))
						continue;
					renderer::NkEmId f0, f1;
					const uint32 nf = M.EdgeFaces(a, b, f0, f1);
					if (nf >= 1)
						n = n + M.faces[f0].normal;
					if (nf >= 2)
						n = n + M.faces[f1].normal;
					if (!haveT) {
						t = M.verts[b].pos - M.verts[a].pos;
						haveT = true;
					}
				}
			}
			// 3) SOMMETS sélectionnés : normale du sommet + une arête incidente.
			if (n.Len() < 1e-6f) {
				for (uint32 i = 0; i < M.VertCount(); i++) {
					if (!vsel(i))
						continue;
					n = n + M.verts[i].normal;
					if (!haveT) {
						for (uint32 e = 0; e + 1 < (uint32)st->editEdges.Size(); e += 2) {
							const uint32 a = st->editEdges[e], b = st->editEdges[e + 1];
							if (a == i || b == i) {
								t = M.verts[b].pos - M.verts[a].pos;
								haveT = true;
								break;
							}
						}
					}
				}
			}
			if (n.Len() < 1e-6f) {
				st->editGizmo.ClearNormalFrame(); // rien d'exploitable -> repli Local/Global
				return;
			}
			if (!haveT)
				t = NkVec3f{1.f, 0.f, 0.f};
			st->editGizmo.SetNormalFrame(dirW(n), dirW(t));
		}

		// ── Pont sélection UI <-> COUCHE DE COMMANDES (editHE = moteur, ops paramétrées) ──
		// Les opérations d'édition (extrude/delete/merge/makeface/subdivide/loopcut/bisect)
		// vivent désormais dans NkEditMesh (moteur, sans dépendance UI/GPU) : base pour l'undo/
		// redo, les modificateurs et l'espace d'actions IA (NKAI). Ces wrappers poussent/
		// récupèrent la sélection du démo (st->vertSel) autour de l'appel, puis
		// Demo3D_SyncFromHE régénère le rendu (triangulation + cage + mesh GPU).
		static void Demo3D_PushSel(Demo3DState *st) {
			const uint32 n = st->editHE.VertCount();
			for (uint32 i = 0; i < n && i < (uint32)st->vertSel.Size(); ++i)
				st->editHE.verts[i].sel = st->vertSel[i];
		}

		static void Demo3D_PullSel(Demo3DState *st) {
			const uint32 n = st->editHE.VertCount();
			if ((uint32)st->vertSel.Size() != n)
				st->vertSel.Resize(n);
			for (uint32 i = 0; i < n; ++i)
				st->vertSel[i] = st->editHE.verts[i].sel;
		}

		// Applique UNE commande d'édition (DONNÉE) : capture la sélection courante dans la
		// commande, snapshot le pré-état pour l'undo, exécute cmd.Apply, et SEULEMENT si la
		// géométrie a changé -> enregistre l'undo + PUSH la commande dans le journal (rejeu /
		// modificateurs / données IA) + resync. Point d'entrée unique des éditions.
		static bool Demo3D_ApplyCmd(Demo3DState *st, renderer::NkMeshSystem *ms, renderer::NkMeshEditCommand cmd) {
			cmd.selection.Clear(); // sélection = donnée rejouable
			for (uint32 i = 0; i < (uint32)st->vertSel.Size(); ++i)
				if (st->vertSel[i])
					cmd.selection.PushBack(i);
			Demo3D_PushSel(st);
			renderer::NkEditMesh snapshot = st->editHE; // pré-état (avec sélection live)
			if (!cmd.Apply(st->editHE))
				return false; // no-op -> ni undo ni enregistrement
			st->editHistory.Commit(snapshot);
			st->editRecorder.Push(cmd); // journalise la commande
			st->editReplayStep = -1;	// une édition sort du mode rejeu
			Demo3D_PullSel(st);
			Demo3D_SyncFromHE(st, ms);
			return true;
		}

		// EXTRUDE (E) — COMPORTEMENT BLENDER EXACT :
		//   • extrude selon le MODE DE SÉLECTION ACTIF : FACE (bit 4) > EDGE (bit 2) > VERTEX
		//     (bit 1) — face = cap + quads latéraux, arête = quad reliant, sommet = arête fil ;
		//   • OFFSET ZÉRO : la nouvelle géométrie naît EXACTEMENT sur l'originale ;
		//   • elle devient la SÉLECTION ; elle n'est PAS déplacée.
		// L'utilisateur la déplace ensuite lui-même au gizmo (G/R/S), le long de la normale
		// (orientation Normal câblée, cf. Demo3D_UpdateEditNormalFrame) ou contrainte X/Y/Z.
		// Aucun auto-move au runtime. (Shift+E bascule région <-> individuel, faces seulement.)
		static void Demo3D_ExtrudeHE(Demo3DState *st, renderer::NkMeshSystem *ms) {
			renderer::NkMeshEditCommand c;
			if (st->editSelMask & 4)
				c.op = renderer::NkMeshEditOp::Extrude; // FACES
			else if (st->editSelMask & 2)
				c.op = renderer::NkMeshEditOp::ExtrudeEdges; // ARÊTES
			else
				c.op = renderer::NkMeshEditOp::ExtrudeVerts; // SOMMETS
			c.extrude.individual = st->extrudeIndividual;
			c.extrude.offset = 0.f; // Blender : zéro déplacement, l'utilisateur bouge ensuite
			// Repli : en mode FACE sans aucune face entièrement sélectionnée, l'extrusion de
			// faces est un no-op -> on retombe sur l'extrusion d'arêtes puis de sommets, comme
			// Blender qui extrude « ce qui est sélectionné ».
			if (Demo3D_ApplyCmd(st, ms, c))
				return;
			if (c.op == renderer::NkMeshEditOp::Extrude) {
				c.op = renderer::NkMeshEditOp::ExtrudeEdges;
				if (Demo3D_ApplyCmd(st, ms, c))
					return;
			}
			if (c.op != renderer::NkMeshEditOp::ExtrudeVerts) {
				c.op = renderer::NkMeshEditOp::ExtrudeVerts;
				Demo3D_ApplyCmd(st, ms, c);
			}
		}

		// DELETE (X) : supprime les faces sélectionnées, compacte — cf. NkEditMesh.
		static void Demo3D_DeleteHE(Demo3DState *st, renderer::NkMeshSystem *ms) {
			renderer::NkMeshEditCommand c;
			c.op = renderer::NkMeshEditOp::Delete;
			Demo3D_ApplyCmd(st, ms, c);
		}

		// MERGE (M) : soude les sommets sélectionnés (center/first/last, Shift+M) — cf. NkEditMesh.
		static void Demo3D_MergeHE(Demo3DState *st, renderer::NkMeshSystem *ms) {
			renderer::NkMeshEditCommand c;
			c.op = renderer::NkMeshEditOp::Merge;
			c.merge.mode = (int32)st->mergeMode;
			Demo3D_ApplyCmd(st, ms, c);
		}

		// CREATE FACE (F) : une face n-gon depuis les sommets sélectionnés — cf. NkEditMesh.
		static void Demo3D_MakeFaceHE(Demo3DState *st, renderer::NkMeshSystem *ms) {
			renderer::NkMeshEditCommand c;
			c.op = renderer::NkMeshEditOp::MakeFace;
			Demo3D_ApplyCmd(st, ms, c);
		}

		// SUBDIVIDE (W) : Catmull-Clark, faces sélectionnées ou TOUT. subdivCuts passes en UNE
		// commande (donc UN seul undo) — cf. NkEditMesh.
		static void Demo3D_SubdivideHE(Demo3DState *st, renderer::NkMeshSystem *ms) {
			renderer::NkMeshEditCommand c;
			c.op = renderer::NkMeshEditOp::Subdivide;
			c.subdiv.cuts = st->subdivCuts;
			Demo3D_ApplyCmd(st, ms, c);
		}

		// LOOP CUT (Ctrl+R) : anneau de quads depuis l'arête sélectionnée — cf. NkEditMesh.
		// st->loopCuts = nombre de boucles insérées (Ctrl+Shift+R cycle 1..5, façon molette).
		static void Demo3D_LoopCutHE(Demo3DState *st, renderer::NkMeshSystem *ms) {
			renderer::NkMeshEditCommand c;
			c.op = renderer::NkMeshEditOp::LoopCut;
			c.loopcut.cuts = st->loopCuts;
			Demo3D_ApplyCmd(st, ms, c);
		}

		// KNIFE / BISECT (K) : coupe par un PLAN (tracé écran -> monde). Le plan est en espace
		// MONDE : on passe editAnchor (modèle->monde) à la commande — cf. NkEditMesh.
		static void Demo3D_BisectHE(Demo3DState *st, renderer::NkMeshSystem *ms, NkVec3f pPoint, NkVec3f pNormal) {
			renderer::NkMeshEditCommand c;
			c.op = renderer::NkMeshEditOp::Bisect;
			c.planePoint = pPoint;
			c.planeNormal = pNormal;
			c.bisectXform = st->editAnchor;
			Demo3D_ApplyCmd(st, ms, c);
		}

		// REJEU (P) : reconstruit editHE depuis le maillage de BASE (capturé à l'entrée) et
		// rejoue TOUTES les commandes enregistrées. Preuve que la couche de commandes est
		// scriptable (fondation modificateurs non-destructifs + données d'imitation IA).
		// REJEU PAS-À-PAS (P) : chaque appui applique UNE commande de plus depuis la base, pour
		// VOIR le modèle se reconstruire étape par étape (comme regarder rejouer la session —
		// exactement l'observation qu'aurait une IA d'apprentissage). Boucle : après la dernière
		// commande, un appui de plus revient à la base.
		static void Demo3D_ReplayEdits(Demo3DState *st, renderer::NkMeshSystem *ms) {
			const int32 total = (int32)st->editRecorder.Count();
			if (st->editReplayStep < 0)
				st->editReplayStep = 0; // 1er appui -> base (0 commande)
			else
				st->editReplayStep++; // appui suivant -> une commande de plus
			if (st->editReplayStep > total)
				st->editReplayStep = 0; // après la fin -> reboucle à la base

			st->editHE = st->editBase; // repart de la base
			int32 applied = 0;
			for (int32 i = 0; i < st->editReplayStep && i < total; ++i) // rejoue les k premières commandes
				if (st->editRecorder.At((uint32)i).Apply(st->editHE))
					++applied;
			st->vertSel.Clear();
			st->vertSel.Resize(st->editHE.VertCount());
			for (uint32 i = 0; i < st->editHE.VertCount(); ++i)
				st->vertSel[i] = st->editHE.verts[i].sel;
			Demo3D_SyncFromHE(st, ms);
			logger.Info("[Demo3D] Rejeu pas-a-pas: etape {0}/{1} ({2} appliquees) -> {3} faces\n", st->editReplayStep,
						total, applied, (int32)st->editHE.FaceCount());
		}

		// SAUVER (F5) : sérialise le journal de commandes -> fichier binaire .nmec (persistance
		// de la session : donnée d'imitation IA + modificateur sauvegardable).
		static void Demo3D_SaveSession(Demo3DState *st) {
			NkVector<uint8> bytes;
			st->editRecorder.Serialize(bytes);
			const char *path = "edit_session.nkmec";
			const bool ok = NkFile::WriteAllBytes(path, bytes);
			logger.Info("[Demo3D] Sauvegarde '{0}' : {1} commandes, {2} octets -> {3}\n", path,
						st->editRecorder.Count(), (int32)bytes.Size(), ok ? "OK" : "ECHEC");
		}

		// CHARGER (F6) : lit le fichier -> désérialise dans le journal -> rejoue depuis la base
		// (reconstruit le modèle édité). Le maillage de base courant doit correspondre.
		static void Demo3D_LoadSession(Demo3DState *st, renderer::NkMeshSystem *ms) {
			const char *path = "edit_session.nkmec";
			NkVector<uint8> bytes = NkFile::ReadAllBytes(path);
			if (bytes.Empty()) {
				logger.Warn("[Demo3D] Chargement '{0}' : fichier vide/absent\n", path);
				return;
			}
			if (!st->editRecorder.Deserialize(bytes.Data(), (uint32)bytes.Size())) {
				logger.Warn("[Demo3D] Chargement '{0}' : format invalide\n", path);
				return;
			}
			st->editHE = st->editBase;
			const uint32 applied = st->editRecorder.ReplayOnto(st->editHE);
			st->editReplayStep = -1;
			st->vertSel.Clear();
			st->vertSel.Resize(st->editHE.VertCount());
			for (uint32 i = 0; i < st->editHE.VertCount(); ++i)
				st->vertSel[i] = st->editHE.verts[i].sel;
			Demo3D_SyncFromHE(st, ms);
			logger.Info("[Demo3D] Session chargee '{0}' : {1} commandes rejouees -> {2} faces\n", path, applied,
						(int32)st->editHE.FaceCount());
		}

		// MODIFICATEURS — réglage des paramètres du modificateur ACTIF ([ / ]) et changement
		// du modificateur actif (\). Chaque changement ré-évalue la pile -> résultat live.
		static void Demo3D_AdjustMod(Demo3DState *st, renderer::NkMeshSystem *ms, int32 dir) {
			const int32 cnt = (int32)st->editModifiers.Count();
			if (cnt == 0)
				return;
			if (st->editActiveMod < 0 || st->editActiveMod >= cnt)
				st->editActiveMod = cnt - 1;
			renderer::NkMeshModifier &m = st->editModifiers.modifiers[st->editActiveMod];
			const char *nm[3] = {"Mirror", "Array", "Subsurf"};
			const char *ax[3] = {"X", "Y", "Z"};
			if (m.type == renderer::NkModifierType::Mirror) {
				m.mirrorAxis = (m.mirrorAxis + (dir > 0 ? 1 : 2)) % 3;
				logger.Info("[Demo3D] Modif[{0}] Mirror axe = {1}\n", st->editActiveMod, ax[m.mirrorAxis]);
			} else if (m.type == renderer::NkModifierType::Array) {
				m.arrayCount += dir;
				if (m.arrayCount < 1)
					m.arrayCount = 1;
				if (m.arrayCount > 20)
					m.arrayCount = 20;
				logger.Info("[Demo3D] Modif[{0}] Array copies = {1}\n", st->editActiveMod, m.arrayCount);
			} else {
				m.subsurfLevels += dir;
				if (m.subsurfLevels < 1)
					m.subsurfLevels = 1;
				if (m.subsurfLevels > 4)
					m.subsurfLevels = 4;
				logger.Info("[Demo3D] Modif[{0}] Subsurf niveaux = {1}\n", st->editActiveMod, m.subsurfLevels);
			}
			(void)nm;
			Demo3D_SyncFromHE(st, ms);
		}

		static void Demo3D_CycleActiveMod(Demo3DState *st) {
			const int32 cnt = (int32)st->editModifiers.Count();
			if (cnt == 0) {
				st->editActiveMod = -1;
				return;
			}
			st->editActiveMod = (st->editActiveMod + 1) % cnt;
			const char *nm[3] = {"Mirror", "Array", "Subsurf"};
			logger.Info("[Demo3D] Modificateur actif = [{0}] {1}\n", st->editActiveMod,
						nm[(int32)st->editModifiers.modifiers[st->editActiveMod].type]);
		}

		// UNDO / REDO : restaure un snapshot de editHE, resynchronise sélection + rendu.
		static void Demo3D_UndoEdit(Demo3DState *st, renderer::NkMeshSystem *ms) {
			if (!st->editHistory.Undo(st->editHE))
				return;
			Demo3D_PullSel(st);
			Demo3D_SyncFromHE(st, ms);
		}

		static void Demo3D_RedoEdit(Demo3DState *st, renderer::NkMeshSystem *ms) {
			if (!st->editHistory.Redo(st->editHE))
				return;
			Demo3D_PullSel(st);
			Demo3D_SyncFromHE(st, ms);
		}

		// E.6b : cubemap procedurale 128x128x6 pour point light.
		// Chaque face = pattern "X" : 2 bandes diagonales lumineuses sur fond noir.
		// Tres contraste pour etre clairement visible meme avec autres lumieres.
		static NkTexHandle CreateLanternCubeCookie(NkTextureLibrary *texLib, NkIDevice *dev) {
			const uint32 S = 128;
			NkTextureCreateDesc d;
			d.width = S;
			d.height = S;
			d.depth = 1;
			d.format = NkGPUFormat::NK_RGBA8_UNORM;
			d.isCubemap = true;
			d.mipLevels = 1;
			d.debugName = "Demo3D_LanternCube";
			NkTexHandle tex = texLib->Create(d);
			if (!tex.IsValid())
				return tex;

			std::vector<uint8_t> pixels(S * S * 4);
			for (uint32 face = 0; face < 6; face++) {
				for (uint32 y = 0; y < S; y++) {
					for (uint32 x = 0; x < S; x++) {
						// X pattern : bright si proche d'une diagonale (epaisseur 8 px)
						float dx = float(x) - S * 0.5f;
						float dy = float(y) - S * 0.5f;
						float diag1 = std::abs(dx - dy); // diagonale slash
						float diag2 = std::abs(dx + dy); // diagonale antislash
						bool onCross = diag1 < 8.f || diag2 < 8.f;
						// Trou central toujours brillant (faisceau "principal")
						float r = std::sqrt(dx * dx + dy * dy);
						bool centerHole = r < S * 0.10f;
						uint8_t v = (onCross || centerHole) ? 255 : 0; // contraste max
						uint32 idx = (y * S + x) * 4;
						pixels[idx + 0] = v;
						pixels[idx + 1] = v;
						pixels[idx + 2] = v;
						pixels[idx + 3] = 255;
					}
				}
				dev->WriteTextureRegion(texLib->GetRHIHandle(tex), pixels.data(), 0, 0, 0, S, S, 1, 0, face);
			}
			return tex;
		}

		// Genere un cookie procedural 256x256 RGBA : motif "window bars".
		// Barres + bordure noires (~12% de transmittance), centre/cellules blanches.
		static NkTexHandle CreateWindowCookie(NkTextureLibrary *texLib) {
			const uint32 W = 256, H = 256;
			std::vector<uint8_t> pixels(W * H * 4);
			for (uint32 y = 0; y < H; y++) {
				for (uint32 x = 0; x < W; x++) {
					bool barV = (x % 64) < 8;
					bool barH = (y % 64) < 8;
					bool border = (x < 8 || x >= W - 8 || y < 8 || y >= H - 8);
					uint8_t v = (barV || barH || border) ? 30 : 255;
					uint32 idx = (y * W + x) * 4;
					pixels[idx + 0] = v;
					pixels[idx + 1] = v;
					pixels[idx + 2] = v;
					pixels[idx + 3] = 255;
				}
			}
			NkTextureCreateDesc d;
			d.pixels = pixels.data();
			d.width = W;
			d.height = H;
			d.mipLevels = 1;
			d.format = NkGPUFormat::NK_RGBA8_UNORM;
			d.debugName = "Demo3D_WindowCookie";
			return texLib->Create(d);
		}

		// Phase H : test de la pipeline texture file-based.
		//
		// Genere (si absent) un fichier PNG procedural "test_pattern.png" 256x256
		// contenant un motif damier color (4 quadrants RGB + bordures), puis le
		// charge via NkTextureLibrary::Load(). Demontre toute la chaine :
		//   1. NkImage::Alloc + SavePNG  -> ecriture disque
		//   2. NkTextureLibrary::Load    -> decode PNG + upload GPU + mip chain
		//   3. retourne un NkTexHandle utilisable comme tout autre texture.
		//
		// Le fichier est genere dans Resources/NKRenderer/Textures/Defaults/
		// (relativement au CWD ou au repo). On essaie d'abord ce chemin, sinon
		// fallback sur le CWD. La premiere execution genere le fichier ; les
		// suivantes le lisent. Si ecriture echoue (permissions / dir inexistant),
		// on retourne false et l'init utilise le cookie procedural.
		static const char *kPhaseHTestPathPrimary = "Resources/NKRenderer/Textures/Defaults/test_pattern.png";
		static const char *kPhaseHTestPathFallback = "test_pattern.png";

		static bool GenerateTestPatternPNG(const char *outPath) {
			// Verifie d'abord s'il existe deja (skip si present).
			if (FILE *f = ::fopen(outPath, "rb")) {
				::fclose(f);
				return true;
			}

			const int32 W = 256, H = 256;
			NkImage *img = NkImage::Alloc(W, H, NkImagePixelFormat::NK_RGBA32);
			if (!img || !img->IsValid()) {
				if (img)
					img->Free();
				return false;
			}

			uint8_t *px = img->Pixels();
			const int32 stride = img->Stride();
			for (int32 y = 0; y < H; ++y) {
				uint8_t *row = px + (size_t)y * stride;
				for (int32 x = 0; x < W; ++x) {
					// 4 quadrants : rouge / vert / bleu / jaune.
					bool right = (x >= W / 2);
					bool bottom = (y >= H / 2);
					uint8_t r = (!right && !bottom) || (right && bottom) ? 255 : 0;
					uint8_t g = (right && !bottom) || (right && bottom) ? 255 : 0;
					uint8_t b = (!right && bottom) ? 255 : 0;
					// Damier 16x16 pour valider qu'on lit bien le PNG (et pas un
					// buffer noir/blanc).
					bool ck = ((x / 16) ^ (y / 16)) & 1;
					if (ck) {
						r = (uint8_t)(r * 0.6f);
						g = (uint8_t)(g * 0.6f);
						b = (uint8_t)(b * 0.6f);
					}
					// Bordure noire 4px pour visualiser les bords.
					bool border = (x < 4 || x >= W - 4 || y < 4 || y >= H - 4);
					if (border) {
						r = g = b = 0;
					}
					row[x * 4 + 0] = r;
					row[x * 4 + 1] = g;
					row[x * 4 + 2] = b;
					row[x * 4 + 3] = 255;
				}
			}
			bool ok = img->SavePNG(outPath);
			img->Free();
			return ok;
		}

		// Helper d'affichage : nom court de PCFMode
		static const char *PcfModeName(NkPCFMode m) {
			switch (m) {
				case NkPCFMode::NONE:
					return "NONE";
				case NkPCFMode::PCF3x3:
					return "PCF3x3";
				case NkPCFMode::PCF5x5:
					return "PCF5x5";
				case NkPCFMode::POISSON:
					return "POISSON";
				case NkPCFMode::PCSS:
					return "PCSS";
			}
			return "?";
		}

		bool Demo3D_Init(DemoCtx &ctx) {
			auto *st = new Demo3DState();
			ctx.userData = st;

			auto *meshSys = ctx.renderer->GetMeshSystem();
			st->meshSphere = meshSys->GetSphere();
			st->meshPlane = meshSys->GetPlane();
			st->meshCube = meshSys->GetCube();
			// Volet 2 : le mesh éditable n'est plus une grille test — il est CLONÉ à la
			// volée depuis l'objet sélectionné à l'entrée en Edit Mode (TAB), cf. la
			// section « EDIT MODE » dans la frame. Les primitives (sphère/cube) gardent
			// leurs données CPU (NkMeshDesc::keepCPU par défaut) -> clonage sans readback.

			// Pas de SNAP (touche Ctrl) — LIBREMENT ajustables ici par l'application :
			//   translate (unités monde) · rotation (degrés) · échelle (delta).
			st->gizmo.SetSnapSteps(/*translate*/ 0.5f, /*rotation°*/ 15.f, /*échelle*/ 0.1f);

			// ── Phase E.6 : creation procedurale des cookies + bind ──────────────
			auto *texLib = ctx.renderer->GetTextures();
			auto *r3d = ctx.renderer->GetRender3D();
			auto *device = ctx.renderer->GetDevice();

			// ── NkVSM v2 : caster ALPHA-TESTED de validation ─────────────────────
			// Panneau "feuillage" : disques de matiere (alpha=255) sur fond TROUE
			// (alpha=0). SetCastShadowAlphaTest(true) -> l'ombre projetee au sol
			// doit montrer les TROUS entre les disques (pipeline Shadow_AlphaTest)
			// et non le rectangle plein. NB : la passe COULEUR reste opaque (le
			// PBR ne fait pas d'alpha-blend) — c'est l'OMBRE qui valide la feature.
			if (texLib && ctx.renderer->GetMaterials()) {
				const uint32 TW = 128, TH = 128;
				NkVector<uint8> px;
				px.Resize((usize)TW * TH * 4u);
				for (uint32 y = 0; y < TH; y++) {
					for (uint32 x = 0; x < TW; x++) {
						const int32 lx = (int32)(x % 32) - 16;
						const int32 ly = (int32)(y % 32) - 16;
						const bool inDisc = (lx * lx + ly * ly) <= 12 * 12;
						uint8 *p = &px[((usize)y * TW + x) * 4u];
						p[0] = 46;
						p[1] = inDisc ? 168 : 60;
						p[2] = 52;
						p[3] = inDisc ? 255 : 0;
					}
				}
				NkTextureCreateDesc td;
				td.pixels = px.Data();
				td.width = TW;
				td.height = TH;
				td.srgb = true;
				td.debugName = "Demo3D_MaskedLeaf";
				st->maskedTex = texLib->Create(td);
				// NB : le template PBR est enregistre sous "Default_PBR" — on passe
				// par l'overload TYPE (pas de nom en dur qui casse silencieusement).
				st->maskedMat = NkMaterial::Create(ctx.renderer->GetMaterials(), NkMaterialType::NK_PBR_METALLIC);
				if (!st->maskedMat)
					logger.Errorf("[Demo3D] Creation material panneau masked KO\n");
				if (st->maskedMat && st->maskedTex.IsValid())
					st->maskedMat->SetAlbedoMap(st->maskedTex)->SetRoughness(0.8f)->SetCastShadowAlphaTest(true);
			}
			if (texLib && r3d) {
				// Phase H : test de la pipeline file-based.
				// On genere un PNG "test_pattern.png" puis on le charge via
				// NkTextureLibrary::Load(). Si la chaine fonctionne, on l'utilise
				// comme cookie spot (plus visible que le procedural en RAM car
				// sample par texture(...) GLSL avec mips). Fallback : cookie
				// procedural (CreateWindowCookie) si Load echoue.
				const char *pngPath = nullptr;
				if (GenerateTestPatternPNG(kPhaseHTestPathPrimary)) {
					pngPath = kPhaseHTestPathPrimary;
				} else if (GenerateTestPatternPNG(kPhaseHTestPathFallback)) {
					pngPath = kPhaseHTestPathFallback;
				}

				if (pngPath) {
					NkLoadOptions opts;
					// Cookie : on veut le PNG en valeur lineaire (pas de gamma)
					// pour que la modulation lumiere*cookie soit correcte. On
					// pourrait passer srgb=true pour un albedo PBR classique.
					opts.srgb = false;
					opts.genMipmaps = true;	  // mips utiles pour cookie
					opts.useClampEdge = true; // cookie : pas de tiling
					opts.debugName = "Demo3D_PhaseH_TestPattern";
					st->cookieWindow = texLib->Load(NkString(pngPath), opts);
					if (st->cookieWindow.IsValid() && st->cookieWindow.id != texLib->GetError().id) {
						logger.Info("[Demo3D][Phase H] PNG charge OK depuis '{0}'\n", pngPath);
						st->phaseHLoadOk = true;
					} else {
						logger.Errorf("[Demo3D][Phase H] Echec Load PNG, fallback procedural\n");
						st->cookieWindow = CreateWindowCookie(texLib);
					}
				} else {
					logger.Errorf("[Demo3D][Phase H] Impossible d'ecrire le PNG de test, fallback procedural\n");
					st->cookieWindow = CreateWindowCookie(texLib);
				}

				if (st->cookieWindow.IsValid()) {
					r3d->SetLightCookie3D(0, texLib->GetRHIHandle(st->cookieWindow));
					logger.Info("[Demo3D] Cookie 2D bind au slot 0\n");
				}
				// E.6b : cube cookie pour le point light rouge (procedural, OK).
				if (device) {
					st->cookieCube = CreateLanternCubeCookie(texLib, device);
					if (st->cookieCube.IsValid()) {
						r3d->SetLightCookieCube3D(0, texLib->GetRHIHandle(st->cookieCube));
						logger.Info("[Demo3D] Lantern cube cookie cree + bind au slot 0\n");
					}
				}
			}

			// ── Shortcuts clavier pour tweak des params shadow en live ──
			// [ / ] : shadowBias -+ 0.0005
			// , / . : sceneRadius -+ 1.0
			// P     : cycle PCF mode
			// R     : reset to defaults
			// V     : toggle VSync (utile pour mesurer le vrai FPS GPU)
			auto *shadowSys = ctx.renderer->GetShadow();
			auto *renderer = ctx.renderer;
			// Molette souris -> zoom caméra (accumulée ici, appliquée dans Demo3D_Frame).
			NkEvents().AddEventCallback<NkMouseWheelVerticalEvent>(
				[st](NkMouseWheelVerticalEvent *e) { st->wheelAccum += e->GetDeltaY(); });
			// Clic GAUCHE -> demande de sélection (ray-pick, traité dans Demo3D_Frame).
			NkEvents().AddEventCallback<NkMouseButtonPressEvent>([st](NkMouseButtonPressEvent *e) {
				if (e->GetButton() == NkMouseButton::NK_MB_LEFT) {
					st->pickPending = true;
					st->pickX = e->GetX();
					st->pickY = e->GetY();
				}
			});
			// F : bascule caméra ÉDITEUR (orbit) <-> SIMULATION (fly). En EDIT MODE, F crée
			// une face (n-gon) depuis la sélection -> ne pas basculer la caméra.
			NkEvents().AddEventCallback<NkKeyPressEvent>([st](NkKeyPressEvent *e) {
				if (e->GetKey() == NkKey::NK_F && !st->editMode) {
					st->useSimCam = !st->useSimCam;
					logger.Info("[Demo3D] Camera = {0}\n", st->useSimCam
															   ? "SIMULATION (fly: WASD+clic droit)"
															   : "EDITEUR (orbit: milieu/Shift+milieu/molette)");
				}
			});
			// Pavé numérique façon Blender : 1=FRONT (Ctrl=BACK) · 3=RIGHT (Ctrl=LEFT) ·
			// 7=TOP (Ctrl=BOTTOM). Snap de la caméra éditeur.
			NkEvents().AddEventCallback<NkKeyPressEvent>([st](NkKeyPressEvent *e) {
				auto &c = st->editorCam;
				const NkVec3f t = c.GetTarget();
				const float32 d = c.GetDistance();
				const float32 P = 1.55f; // ~90° (clamp pitch)
				const bool ctrl = NkInput.IsKeyDown(NkKey::NK_LCTRL) || NkInput.IsKeyDown(NkKey::NK_RCTRL);
				const NkKey k = e->GetKey();
				bool axisView = false;
				if (k == NkKey::NK_NUMPAD_1) {
					axisView = true;
					if (!ctrl) {
						c.SetCenter(t, d, 1.5708f, 0.f);
						logger.Info("[Demo3D] Vue FRONT (ortho)\n");
					} else {
						c.SetCenter(t, d, -1.5708f, 0.f);
						logger.Info("[Demo3D] Vue BACK (ortho)\n");
					}
				} else if (k == NkKey::NK_NUMPAD_3) {
					axisView = true;
					if (!ctrl) {
						c.SetCenter(t, d, 0.f, 0.f);
						logger.Info("[Demo3D] Vue RIGHT (ortho)\n");
					} else {
						c.SetCenter(t, d, 3.1416f, 0.f);
						logger.Info("[Demo3D] Vue LEFT (ortho)\n");
					}
				} else if (k == NkKey::NK_NUMPAD_7) {
					axisView = true;
					if (!ctrl) {
						c.SetCenter(t, d, 0.f, P);
						logger.Info("[Demo3D] Vue TOP (ortho)\n");
					} else {
						c.SetCenter(t, d, 0.f, -P);
						logger.Info("[Demo3D] Vue BOTTOM (ortho)\n");
					}
				} else if (k == NkKey::NK_NUMPAD_5) {
					st->orthoView = !st->orthoView;
					logger.Info("[Demo3D] Projection = {0}\n", st->orthoView ? "ORTHO" : "PERSPECTIVE");
				} // pavé 5 = toggle ortho/persp
				if (axisView)
					st->orthoView = true; // vue axiale -> ortho auto (façon Blender)
			});
			// Réglages viewport/debug sur F-keys (hors keymap Blender essentiel) :
			//   F1=grille on/off · F2/F3/F4=grille internes/majeures/axes · F11/F12=opacité plan -/+
			//   V=VSync
			NkEvents().AddEventCallback<NkKeyPressEvent>([renderer, st](NkKeyPressEvent *e) {
				const NkKey k = e->GetKey();
				if (k == NkKey::NK_V) {
					static bool vsync = true;
					vsync = !vsync;
					renderer->SetVSync(vsync);
					logger.Info("[Demo3D] VSync = {0}\n", vsync);
				}
				if (auto *r3d = renderer->GetRender3D()) {
					// Z (hors drag, SANS Alt) = cycle mode d'affichage façon Blender :
					// RENDERED -> SOLID -> WIREFRAME. (En drag, Z = verrou d'axe ;
					// Alt+Z = toggle X-ray en Edit Mode, géré dans le keymap.)
					const bool altHeld = NkInput.IsKeyDown(NkKey::NK_LALT) || NkInput.IsKeyDown(NkKey::NK_RALT);
					const bool ctrlHeld = NkInput.IsKeyDown(NkKey::NK_LCTRL) || NkInput.IsKeyDown(NkKey::NK_RCTRL);
					// !ctrl : Ctrl+Z est réservé à l'UNDO en edit mode (ne pas cycler l'affichage).
					if (k == NkKey::NK_Z && !altHeld && !ctrlHeld && !st->gizmo.IsDragging() &&
						!st->editGizmo.IsDragging()) {
						st->shadingMode = (st->shadingMode + 1) % 6;
						const char *sm[6] = {"RENDERED", "SOLID", "WIREFRAME", "NORMAL", "UV", "AO"};
						// viewMode shader : 0=PBR éclairé, 1=matcap unlit, 2=normal, 3=uv, 4=ao.
						const int32 vm[6] = {0, 1, 1, 2, 3, 4};
						r3d->SetWireframe(st->shadingMode == 2); // wireframe = rasterizer fil de fer
						r3d->SetViewMode(vm[st->shadingMode]);
						logger.Info("[Demo3D] Affichage = {0}\n", sm[st->shadingMode]);
					}
					// B : cycle la SOURCE DE COULEUR en edit/unlit (façon "Color" du mode
					// Solid de Blender) : MATERIAL -> GRIS -> CUSTOM.
					if (k == NkKey::NK_B) {
						st->unlitColorMode = (st->unlitColorMode + 1) % 3;
						const char *cm[3] = {"MATERIAL", "GRIS", "CUSTOM"};
						logger.Info("[Demo3D] Couleur unlit/edit = {0}\n", cm[st->unlitColorMode]);
					}
					// M : cycle le preset MatCap (effet en mode SOLID/WIREFRAME).
					// En EDIT MODE, M = Merge (soudure) -> ne pas cycler le matcap.
					if (k == NkKey::NK_M && !st->editMode) {
						r3d->SetMatcap(r3d->Matcap() + 1);
						const char *mc[5] = {"Studio", "Clay", "Metal", "Toon", "Chrome(tex)"};
						logger.Info("[Demo3D] MatCap = {0}\n", mc[r3d->Matcap() % 5]);
					}
					auto &g = r3d->GetInfiniteGridParams();
					if (k == NkKey::NK_F1) {
						bool on = !r3d->IsInfiniteGridEnabled();
						r3d->SetInfiniteGridEnabled(on);
						logger.Info("[Demo3D] Grille = {0}\n", on);
					}
					if (k == NkKey::NK_F2) {
						g.showMinor = !g.showMinor;
						logger.Info("[Demo3D] Grille internes = {0}\n", g.showMinor);
					}
					if (k == NkKey::NK_F3) {
						g.showMajor = !g.showMajor;
						logger.Info("[Demo3D] Grille majeures = {0}\n", g.showMajor);
					}
					if (k == NkKey::NK_F4) {
						g.showAxes = !g.showAxes;
						logger.Info("[Demo3D] Grille axes = {0}\n", g.showAxes);
					}
					if (k == NkKey::NK_F11) {
						g.cellColor.w = NkMax(0.0f, g.cellColor.w - 0.05f);
						logger.Info("[Demo3D] Opacite plan = {0}\n", g.cellColor.w);
					}
					if (k == NkKey::NK_F12) {
						g.cellColor.w = NkMin(1.0f, g.cellColor.w + 0.05f);
						logger.Info("[Demo3D] Opacite plan = {0}\n", g.cellColor.w);
					}
					// F5/F6 = sauver/charger la SESSION d'édition -> ne marchent qu'en EDIT MODE.
					if ((k == NkKey::NK_F5 || k == NkKey::NK_F6) && !st->editMode)
						logger.Info("[Demo3D] {0} : entre d'abord en EDIT MODE (Tab) sur un objet selectionne\n",
									k == NkKey::NK_F5 ? "F5 (sauver session)" : "F6 (charger session)");
				}
			});
			// ── KEYMAP GIZMO façon Blender ────────────────────────────────────────
			//   G / R / S = translate / rotate / scale (hors drag)  ·  C = combiné  ·  TAB = cycle
			//   Alt+G / Alt+R / Alt+S = efface translation / rotation / échelle des sélectionnés
			//   A = tout sélectionner  ·  Alt+A = tout désélectionner  ·  , = orientation (G/L/N)
			//   (pendant un drag : X/Y/Z = verrou d'axe, Ctrl = snap)
			NkEvents().AddEventCallback<NkKeyPressEvent>([st](NkKeyPressEvent *e) {
				const NkKey k = e->GetKey();
				const bool alt = NkInput.IsKeyDown(NkKey::NK_LALT) || NkInput.IsKeyDown(NkKey::NK_RALT);
				const char *mn[4] = {"TRANSLATE", "ROTATE", "SCALE", "COMBINE (T+R+S)"};
				using GZ = renderer::NkGizmo3D;
				// TAB : bascule OBJET <-> EDIT MODE. Traité côté frame (accès meshSys pour
				// cloner le mesh de l'objet sélectionné). Façon Blender.
				if (k == NkKey::NK_TAB) {
					st->editTogglePending = true;
					return;
				}
				// En EDIT MODE : touches 1/2/3 = sous-mode sélection VERTEX / EDGE / FACE.
				if (st->editMode) {
					// 1/2/3 = mode SEUL (vertex/arête/face) ; Shift+1/2/3 = COMBINER (toggle),
					// façon Blender (on peut avoir plusieurs modes actifs à la fois).
					{
						const bool shiftK = NkInput.IsKeyDown(NkKey::NK_LSHIFT) || NkInput.IsKeyDown(NkKey::NK_RSHIFT);
						int32 bit = (k == NkKey::NK_NUM1)	? 1
									: (k == NkKey::NK_NUM2) ? 2
									: (k == NkKey::NK_NUM3) ? 4
															: 0;
						if (bit) {
							if (shiftK)
								st->editSelMask ^= bit;
							else
								st->editSelMask = bit;
							if (st->editSelMask == 0)
								st->editSelMask = bit; // toujours >=1 mode actif
							st->editOverlayDirty = true;
							logger.Info("[Demo3D] Edit modes = {0}{1}{2}\n", (st->editSelMask & 1) ? "V" : "-",
										(st->editSelMask & 2) ? "E" : "-", (st->editSelMask & 4) ? "F" : "-");
							return;
						}
					}
					// Alt+Z : toggle X-RAY (voir/sélectionner à travers le mesh), façon Blender.
					if (k == NkKey::NK_Z && alt) {
						st->editXray = !st->editXray;
						st->editOverlayDirty = true;
						logger.Info("[Demo3D] X-ray = {0}\n", st->editXray);
						return;
					}
					// Ctrl+Z = ANNULER · Ctrl+Shift+Z / Ctrl+Y = RÉTABLIR (historique d'édition).
					// Traité côté frame (accès meshSys pour resync). Façon Blender.
					{
						const bool ctrlZ = NkInput.IsKeyDown(NkKey::NK_LCTRL) || NkInput.IsKeyDown(NkKey::NK_RCTRL);
						const bool shiftZ = NkInput.IsKeyDown(NkKey::NK_LSHIFT) || NkInput.IsKeyDown(NkKey::NK_RSHIFT);
						if (ctrlZ && k == NkKey::NK_Z) {
							if (shiftZ)
								st->editRedoPending = true;
							else
								st->editUndoPending = true;
							return;
						}
						if (ctrlZ && k == NkKey::NK_Y) {
							st->editRedoPending = true;
							return;
						}
					}
					// P : REJOUE le journal des commandes depuis le maillage de base (preuve que
					// la couche de commandes est scriptable -> modificateurs + données IA).
					if (k == NkKey::NK_P) {
						st->editReplayPending = true;
						return;
					}
					// F5 / F6 : SAUVER / CHARGER la session (journal sérialisé sur disque).
					if (k == NkKey::NK_F5) {
						st->editSavePending = true;
						return;
					}
					if (k == NkKey::NK_F6) {
						st->editLoadPending = true;
						return;
					}
					// F7/F8/F9 = ajouter modificateur MIRROR / ARRAY / SUBSURF (non-destructif) · F10 = vider.
					if (k == NkKey::NK_F7) {
						st->editAddModPending = 0;
						return;
					}
					if (k == NkKey::NK_F8) {
						st->editAddModPending = 1;
						return;
					}
					if (k == NkKey::NK_F9) {
						st->editAddModPending = 2;
						return;
					}
					if (k == NkKey::NK_F10) {
						st->editClearModPending = true;
						return;
					}
					// Réglage du modificateur ACTIF : ] augmente · [ diminue le param principal ·
					// \ change de modificateur actif (Mirror=axe, Array=copies, Subsurf=niveaux).
					if (k == NkKey::NK_RBRACKET) {
						st->editModAdjustPending = +1;
						return;
					}
					if (k == NkKey::NK_LBRACKET) {
						st->editModAdjustPending = -1;
						return;
					}
					if (k == NkKey::NK_BACKSLASH) {
						st->editModCyclePending = true;
						return;
					}
					// A / Alt+A : tout sélectionner / désélectionner (les VERTICES).
					if (k == NkKey::NK_A) {
						const uint8 v = alt ? 0 : 1;
						for (uint32 i = 0; i < (uint32)st->vertSel.Size(); i++)
							st->vertSel[i] = v;
						st->editOverlayDirty = true;
						return;
					}
					// Outils topologie (hors drag). Shift+touche = règle la PROPRIÉTÉ de l'outil
					// (façon Blender) ; touche seule = applique.
					if (!st->editGizmo.IsDragging()) {
						const bool shiftK = NkInput.IsKeyDown(NkKey::NK_LSHIFT) || NkInput.IsKeyDown(NkKey::NK_RSHIFT);
						const bool ctrlK = NkInput.IsKeyDown(NkKey::NK_LCTRL) || NkInput.IsKeyDown(NkKey::NK_RCTRL);
						// Ctrl+R = LOOP CUT (boucle d'arêtes) depuis l'arête sélectionnée.
						// Ctrl+Shift+R = règle le NOMBRE de coupes (1..5), façon molette Blender.
						if (k == NkKey::NK_R && ctrlK) {
							if (shiftK) {
								st->loopCuts = (st->loopCuts % 5) + 1;
								logger.Info("[Demo3D] Loop cut : {0} coupe(s)\n", st->loopCuts);
							} else
								st->editLoopCutPending = true;
							return;
						}
						if (k == NkKey::NK_E) {
							if (shiftK) {
								st->extrudeIndividual = !st->extrudeIndividual;
								logger.Info("[Demo3D] Extrude = {0}\n",
											st->extrudeIndividual ? "INDIVIDUEL" : "REGION");
							} else
								st->editExtrudePending = true;
							return;
						}
						if (k == NkKey::NK_X) {
							st->editDeletePending = true;
							return;
						}
						if (k == NkKey::NK_M) {
							if (shiftK) {
								st->mergeMode = (st->mergeMode + 1) % 3;
								const char *mm[3] = {"CENTER", "FIRST", "LAST"};
								logger.Info("[Demo3D] Merge = {0}\n", mm[st->mergeMode]);
							} else
								st->editMergePending = true;
							return;
						}
						if (k == NkKey::NK_F) {
							st->editMakeFacePending = true;
							return;
						}
						if (k == NkKey::NK_W) {
							if (shiftK) {
								st->subdivCuts = (st->subdivCuts % 4) + 1;
								logger.Info("[Demo3D] Subdiv coupes = {0}\n", st->subdivCuts);
							} else
								st->editSubdivPending = true;
							return;
						}
						// K : arme le COUTEAU/BISECT (les 2 prochains clics tracent la ligne de coupe).
						if (k == NkKey::NK_K) {
							st->knifeArmed = !st->knifeArmed;
							st->knifeHasP0 = false;
							logger.Info("[Demo3D] Couteau = {0}\n", st->knifeArmed ? "ARME (clic 2 points)" : "off");
							return;
						}
					}
				}
				// Gizmo ACTIF selon le mode : objet ou vertices.
				renderer::NkGizmo3D &G = st->editMode ? st->editGizmo : st->gizmo;
				if (G.IsDragging())
					return; // en plein drag : X/Y/Z = verrou (pas de switch)
				if (k == NkKey::NK_G) {
					if (alt)
						G.ClearSelectedTranslate();
					else
						G.SetMode(GZ::MODE_TRANSLATE);
				}
				if (k == NkKey::NK_R) {
					if (alt)
						G.ClearSelectedRotation();
					else
						G.SetMode(GZ::MODE_ROTATE);
				}
				if (k == NkKey::NK_S) {
					if (alt)
						G.ClearSelectedScale();
					else
						G.SetMode(GZ::MODE_SCALE);
				}
				if (k == NkKey::NK_C)
					G.SetMode(GZ::MODE_COMBINE);
				if (k == NkKey::NK_A) {
					if (alt)
						G.ClearSelection();
					else
						G.SelectAll();
				}
				if (k == NkKey::NK_COMMA) {
					G.CycleOrientation();
					const char *o[3] = {"GLOBAL", "LOCAL", "NORMAL"};
					logger.Info("[Demo3D] Orientation = {0}\n", o[G.Orientation()]);
				}
				if (k == NkKey::NK_G || k == NkKey::NK_R || k == NkKey::NK_S || k == NkKey::NK_C)
					logger.Info("[Demo3D] Gizmo mode = {0}\n", mn[G.Mode()]);
			});
			if (shadowSys) {
				// ── Scène CLOSE : AUTO-FIT de la cascade directionnelle aux casters ──
				// La cascade est ajustée chaque frame aux bornes RÉELLES de tous les casters
				// (sphères + grille 8x8 de cubes + poteaux) : centre ancré au monde (pas de
				// swimming) + rayon au plus serré (couverture complète SANS gaspiller la
				// résolution). Un rayon fixe trop grand perdait les petites ombres ; un rayon
				// suivant la caméra les faisait glisser/disparaître. L'auto-fit résout les deux.
				shadowSys->GetConfig().autoFitDirectional = true;
				NkEvents().AddEventCallback<NkKeyPressEvent>([shadowSys, st](NkKeyPressEvent *e) {
					auto &cfg = shadowSys->GetConfig();
					// Debug ombres sur F-keys (libère [ ] P N M R pour le keymap Blender) :
					//   F5/F6 = bias -/+ (maintenu = continu) · F7 = cycle PCF ·
					//   F8/F9 = softness -/+ · F10 = reset ombres.
					switch (e->GetKey()) {
						case NkKey::NK_F5:
							if (!st->biasDownHeld)
								cfg.shadowBias = NkMax(0.0001f, cfg.shadowBias - 0.0005f);
							st->biasDownHeld = true;
							break;
						case NkKey::NK_F6:
							if (!st->biasUpHeld)
								cfg.shadowBias += 0.0005f;
							st->biasUpHeld = true;
							break;
						case NkKey::NK_F7:
							st->pcfIdx = (st->pcfIdx + 1) % 5;
							cfg.quality = (NkVSMShadowQuality)st->pcfIdx;
							break;
						case NkKey::NK_F8:
							cfg.softness = NkMax(0.0005f, cfg.softness - 0.001f);
							break;
						case NkKey::NK_F9:
							cfg.softness = NkMin(0.020f, cfg.softness + 0.001f);
							break;
						case NkKey::NK_F10:
							cfg.shadowBias = 0.001f;
							cfg.softness = 0.003f;
							cfg.quality = NkVSMShadowQuality::PCF5x5;
							st->pcfIdx = (int32)NkVSMShadowQuality::PCF5x5;
							break;
						default:
							break;
					}
				});
				// Relache F5 / F6 -> stoppe l'evolution continue du bias.
				NkEvents().AddEventCallback<NkKeyReleaseEvent>([st](NkKeyReleaseEvent *e) {
					if (e->GetKey() == NkKey::NK_F5)
						st->biasDownHeld = false;
					if (e->GetKey() == NkKey::NK_F6)
						st->biasUpHeld = false;
				});
			}

			// ── Grille infinie style Blender (remplace la DrawDebugGrid finie) ──────
			// Intérieur des cellules gris SEMI-transparent (cellColor.w) : on voit à
			// travers mais les lignes restent visibles. Axes X rouge / Z bleu sur le plan.
			if (auto *r3d = ctx.renderer->GetRender3D()) {
				r3d->SetInfiniteGridEnabled(true);
				auto &g = r3d->GetInfiniteGridParams();
				g.cellSize = 1.0f;
				g.majorEvery = 10.0f;
				g.fadeEnd = 10.0f; // FACTEUR de portée : rayon net ~ hauteur_cam * 10 (proportionnel)
				g.planeY = 0.01f;  // 1 cm au-dessus du sol solide -> grille visible, pas de z-fight
				g.lineColor = {0.42f, 0.45f, 0.52f,
							   1.0f}; // gris moyen : bien visible sur fond sombre MAIS sous le seuil du bloom
				g.cellColor = {0.09f, 0.10f, 0.12f, 0.18f}; // intérieur = PLAN INFINI (.w=opacité ; 0=transparent)
				g.axisXColor = {1.0f, 0.0f, 0.0f, 1.0f};	// X rouge PLEIN
				g.axisZColor = {0.0f, 0.0f, 1.0f, 1.0f};	// Z bleu PLEIN
				// Axes du SHADER grille DÉSACTIVÉS : on dessine les 3 axes X/Y/Z en lignes 3D
				// réelles (DrawDebugLine, cf. Frame). Raison : l'axe Y en projection écran dans
				// le FS avait des artefacts (quittait l'origine / pas parallèle aux verticales
				// en perspective). Une vraie ligne 3D est correcte partout (perspective, ancrée,
				// top/bottom) ET cohérente en épaisseur pour les 3.
				g.showAxes = false;
			}

			// ── Caméras réutilisables du moteur ──────────────────────────────────
			// Éditeur (Blender) : orbit autour de (0,0.5,0). Simulation (fly) : recul sur -Z.
			// NK_CAM_DIST : recule la caméra orbit (rayon) pour les captures de test. Défaut 6.5.
			float32 camRadius = 6.5f;
			if (const char *cd = getenv("NK_CAM_DIST")) {
				float32 v = (float32)atof(cd);
				if (v > 0.5f)
					camRadius = v;
			}
			st->editorCam.SetCenter({0.f, 0.5f, 0.f}, camRadius, 0.7f, 0.4f);
			st->simCam.SetPose({0.f, 1.5f, 6.f}, -1.5708f, -0.15f);

			logger.Info("[Demo3D] Init OK — meshes : sphere={0} plane={1} cube={2}\n", (uint64)st->meshSphere.id,
						(uint64)st->meshPlane.id, (uint64)st->meshCube.id);
			return true;
		}

		void Demo3D_Frame(DemoCtx &ctx, float32 dt) {
			auto *st = (Demo3DState *)ctx.userData;
			// Delta souris RÉEL de la frame = (courant - précédent) -> vaut 0 sans mouvement
			// (contrairement à NkInput.MouseDelta*() périmé). Alimente les 2 gizmos (objet + edit).
			const float32 curMouseX = (float32)NkInput.MouseX();
			const float32 curMouseY = (float32)NkInput.MouseY();
			float32 frameMDX = 0.f, frameMDY = 0.f;
			if (st->mouseTracked) {
				frameMDX = curMouseX - st->lastMouseX;
				frameMDY = curMouseY - st->lastMouseY;
			}
			st->lastMouseX = curMouseX;
			st->lastMouseY = curMouseY;
			st->mouseTracked = true;
			// NK_GRID_CLEAN=1 : capture « grille SEULE » -> pas de sol opaque, pas d'axes debug
			// 3D épais ; on montre la VRAIE grille infinie (lignes fines AA + axes sol fins du
			// shader X=rouge/Z=bleu). Sert à juger le shader infinitegrid à l'œil.
			static int gridClean = -1;
			if (gridClean == -1) {
				const char *v = getenv("NK_GRID_CLEAN");
				gridClean = (v && v[0] && v[0] != '0') ? 1 : 0;
			}
			// DIAG (gated NK_FIX_CAM) : fige la caméra + le temps pour comparer DX12/VK
			// au MÊME angle/pose. Pose déterministe identique sur les 2 backends.
			static int fixcam = -1;
			if (fixcam == -1) {
				const char *v = getenv("NK_FIX_CAM");
				fixcam = (v && v[0] && v[0] != '0') ? 1 : 0;
			}
			// NK_FIX_CAM : fige la CAMÉRA uniquement (angle constant), le temps continue
			// -> le spot bouge toujours. Isole "flicker vient de la caméra" vs "de l'ombre".
			if (fixcam) {
				st->angle = 0.6f;
			} else {
				st->angle += dt * 0.45f;
			}

			// [ / ] maintenus : evolution CONTINUE du bias (taux x dt). Permet de
			// balayer rapidement la plage sans marteler la touche.
			if (st->biasUpHeld || st->biasDownHeld) {
				if (auto *ssys = ctx.renderer->GetShadow()) {
					auto &cfg = ssys->GetConfig();
					const float32 kBiasRate = 0.02f; // unites de bias par seconde
					if (st->biasUpHeld)
						cfg.shadowBias += kBiasRate * dt;
					if (st->biasDownHeld)
						cfg.shadowBias = NkMax(0.0001f, cfg.shadowBias - kBiasRate * dt);
				}
			}

			if (!ctx.renderer->BeginFrame())
				return;

			auto *r3d = ctx.renderer->GetRender3D();
			if (!r3d) {
				ctx.renderer->Present();
				ctx.renderer->EndFrame();
				return;
			}

			// ── PILOTE HEADLESS D'EDIT MODE (captures agents/CI, sans souris) ────────────
			// NK_EDIT_MODE=1 : entre en EDIT MODE sur un objet (NK_GIZMO_OBJ, défaut 16 = cube
			// central), sélectionne un sous-ensemble façon démo, et applique éventuellement UNE
			// opération pour une capture avant/après :
			//   NK_EDIT_SELMASK=<1..7> : modes de sél. RENDUS (1=V 2=E 4=F ; défaut 1=vertex).
			//   NK_EDIT_SEL=top|all|face|edge|vert : sous-ensemble sélectionné (défaut top =
			//       faces +Y ; face/edge/vert = UN SEUL élément, idéal pour juger le rendu).
			//   NK_EDIT_OP=extrude|subdivide|loopcut|none : op déclenchée (défaut none).
			//   NK_EDIT_CUTS=<n>        : nombre de coupes du loop cut (défaut 1).
			//   NK_EDIT_ORIENT=g|l|n    : orientation du gizmo d'édition (global/local/normal).
			//   NK_SHADING=<0..5>       : mode d'affichage (0=RENDERED 1=SOLID …), comme en objet.
			// ⚠ NK_EDIT_MOVE n'est PAS le comportement de l'extrude : c'est un ARTEFACT DE
			//   CAPTURE. E crée la géométrie à offset ZÉRO (donc invisible en image fixe) ;
			//   NK_EDIT_MOVE la déplace APRÈS coup, uniquement pour la rendre visible.
			// Séquence multi-frames : frame0 entrer -> frame1 sélectionner -> frame2 (op).
			{
				static int32 gEditDrv = -2;
				if (gEditDrv == -2)
					gEditDrv = getenv("NK_EDIT_MODE") ? 0 : -1;
				if (gEditDrv == 0) {
					int32 obj = 16;
					if (const char *go = getenv("NK_GIZMO_OBJ"))
						obj = atoi(go);
					st->gizmo.Select(obj);
					st->gizmo.SetMode(0);		  // gizmo d'édition en TRANSLATE (flèches pleines)
					st->editTogglePending = true; // consommé juste après -> entre en édition ce frame
					if (const char *sm = getenv("NK_EDIT_SELMASK")) {
						int32 m = atoi(sm) & 7;
						if (m)
							st->editSelMask = m;
					}
					if (const char *sh = getenv("NK_SHADING")) {
						st->shadingMode = atoi(sh) % 6;
						const int32 vm[6] = {0, 1, 1, 2, 3, 4};
						r3d->SetWireframe(st->shadingMode == 2);
						r3d->SetViewMode(vm[st->shadingMode]);
					}
					if (const char *cu = getenv("NK_EDIT_CUTS")) {
						const int32 c = atoi(cu);
						if (c >= 1)
							st->loopCuts = c;
					}
					if (const char *or_ = getenv("NK_EDIT_ORIENT"))
						st->editGizmo.SetOrientationByName(or_);
					gEditDrv = 1;
				} else if (gEditDrv == 1 && st->editMode) {
					// Sélection démo : faces +Y ("top"), TOUT, ou UN SEUL élément (face/edge/vert).
					const char *sel = getenv("NK_EDIT_SEL");
					const char s0 = sel ? sel[0] : 't';
					const bool selAll = (s0 == 'a' || s0 == 'A');
					const bool selNone = (s0 == 'n' || s0 == 'N'); // capture « rien sélectionné »
					const bool selOneFace = (s0 == 'f' || s0 == 'F');
					const bool selOneEdge = (s0 == 'e' || s0 == 'E');
					const bool selOneVert = (s0 == 'v' || s0 == 'V');
					const uint32 vc = st->editHE.VertCount();
					for (uint32 i = 0; i < vc && i < (uint32)st->vertSel.Size(); i++)
						st->vertSel[i] = 0;
					st->editActiveVert = -1;
					const uint32 fcnt = (uint32)st->editHE.faces.Size();
					NkVector<renderer::NkEmId> fvv;
					int32 nsel = 0;
					// DIAGNOSTIC TOPOLOGIE (P0) : compte les faces par nombre de côtés et les
					// arêtes UNIQUES du n-gon. Un cube quadifié doit donner 6 quads / 24 arêtes
					// (12 arêtes géométriques dédoublées car les sommets sont dupliqués par
					// face dans la primitive) et AUCUN triangle -> pas de diagonale possible.
					{
						uint32 nTri = 0, nQuad = 0, nNgon = 0, nWire = 0;
						for (uint32 f = 0; f < fcnt; f++) {
							if (!st->editHE.faces[f].alive)
								continue;
							const uint32 sz = st->editHE.FaceSize(f);
							if (sz == 2)
								nWire++;
							else if (sz == 3)
								nTri++;
							else if (sz == 4)
								nQuad++;
							else if (sz > 4)
								nNgon++;
						}
						logger.Info("[Demo3D] Topologie n-gon : {0} tri / {1} quad / {2} n-gon / {3} fil "
									"| {4} aretes uniques (cage) | {5} triangles de rendu\n",
									nTri, nQuad, nNgon, nWire, (uint32)(st->editEdges.Size() / 2),
									(uint32)(st->editIdx.Size() / 3));
					}
					if (selNone) {
						// rien à faire : tout est déjà désélectionné ci-dessus
					} else if (selOneFace || selOneEdge || selOneVert) {
						// UN SEUL élément : on prend la face la mieux alignée sur une DIRECTION
						// cible (NK_EDIT_FACEDIR, défaut +Y = face du dessus, bien visible), ou
						// sa 1re arête / son 1er sommet. « xz » vise une face OBLIQUE — utile
						// pour juger l'orientation « Normal » du gizmo sur une sphère.
						NkVec3f want{0.f, 1.f, 0.f};
						if (const char *fd = getenv("NK_EDIT_FACEDIR")) {
							if (fd[0] == 'x')
								want = (fd[1] == 'z') ? NkVec3f{0.707f, 0.f, 0.707f} : NkVec3f{1.f, 0.f, 0.f};
							else if (fd[0] == 'z')
								want = NkVec3f{0.f, 0.f, 1.f};
						}
						int32 best = -1;
						float32 bestY = -2.f;
						for (uint32 f = 0; f < fcnt; f++) {
							if (!st->editHE.faces[f].alive || st->editHE.FaceSize(f) < 3)
								continue;
							const float32 y = st->editHE.faces[f].normal.Dot(want);
							if (y > bestY) {
								bestY = y;
								best = (int32)f;
							}
						}
						if (best >= 0) {
							fvv.Clear();
							st->editHE.GetFaceVerts((renderer::NkEmId)best, fvv);
							const uint32 fn = (uint32)fvv.Size();
							// La caméra de capture regarde le coin +X/+Z : on choisit le sommet /
							// l'arête qui maximise (x+z) pour que l'élément sélectionné soit BIEN
							// VISIBLE au centre de l'image (sinon on tombe sur le coin du fond).
							auto score = [&](uint32 vi) {
								return st->editHE.verts[vi].pos.x + st->editHE.verts[vi].pos.z;
							};
							uint32 k0 = 0;
							if (selOneVert) {
								float32 bs = -1e30f;
								for (uint32 k = 0; k < fn; k++)
									if (score(fvv[k]) > bs) {
										bs = score(fvv[k]);
										k0 = k;
									}
							} else if (selOneEdge) {
								float32 bs = -1e30f;
								for (uint32 k = 0; k < fn; k++) {
									const float32 s2 = score(fvv[k]) + score(fvv[(k + 1) % fn]);
									if (s2 > bs) {
										bs = s2;
										k0 = k;
									}
								}
							}
							const uint32 take = selOneVert ? 1u : (selOneEdge ? 2u : fn);
							for (uint32 k = 0; k < take && k < fn; k++) {
								const uint32 vi = fvv[(k0 + k) % fn];
								if (vi < (uint32)st->vertSel.Size()) {
									st->vertSel[vi] = 1;
									st->editActiveVert = (int32)vi;
									nsel++;
								}
							}
						}
					} else {
						for (uint32 f = 0; f < fcnt; f++) {
							if (!st->editHE.faces[f].alive)
								continue;
							if (!selAll && st->editHE.faces[f].normal.y < 0.5f)
								continue; // "top" = faces tournées vers le haut (+Y)
							fvv.Clear();
							st->editHE.GetFaceVerts(f, fvv);
							for (uint32 k = 0; k < (uint32)fvv.Size(); k++) {
								const uint32 vi = fvv[k];
								if (vi < (uint32)st->vertSel.Size()) {
									if (!st->vertSel[vi])
										nsel++;
									st->vertSel[vi] = 1;
									st->editActiveVert = (int32)vi;
								}
							}
						}
					}
					st->editOverlayDirty = true;
					logger.Info("[Demo3D] NK_EDIT_MODE: selection {0} -> {1} sommets (mask={2})\n",
								selAll		 ? "all"
								: selNone	 ? "none"
								: selOneFace ? "face"
								: selOneEdge ? "edge"
								: selOneVert ? "vert"
											 : "top",
								nsel, st->editSelMask);
					gEditDrv = 2;
				} else if (gEditDrv == 2) {
					if (const char *op = getenv("NK_EDIT_OP")) {
						if (op[0] == 'e' || op[0] == 'E')
							st->editExtrudePending = true; // Extrude
						else if (op[0] == 's' || op[0] == 'S')
							st->editSubdivPending = true; // Subdivide
						else if (op[0] == 'l' || op[0] == 'L')
							st->editLoopCutPending = true; // Loop cut
					}
					gEditDrv = 3;
				} else if (gEditDrv == 3 && st->editMode) {
					// NK_EDIT_MOVE=<dy> : après l'op, déplace la sélection le long de +Y (local)
					// -> illustre le « grab » qui SUIT l'extrude façon Blender (et rend le
					// résultat nettement visible en capture). Applique directement sur l'autorité
					// editHE puis resynchronise (undo hors périmètre de ce pilote de capture).
					if (const char *mv = getenv("NK_EDIT_MOVE")) {
						const float32 dy = (float32)atof(mv);
						auto *msMv = ctx.renderer->GetMeshSystem();
						const uint32 hv = st->editHE.VertCount();
						for (uint32 i = 0; i < hv && i < (uint32)st->vertSel.Size(); i++)
							if (st->vertSel[i])
								st->editHE.verts[i].pos.y += dy;
						st->editHE.RecomputeNormals();
						if (msMv)
							Demo3D_SyncFromHE(st, msMv);
					}
					gEditDrv = 4;
				}
			}

			// ── Volet 2 : TAB traité ici (accès meshSys) : entre/sort d'EDIT MODE ──
			// Entrée : CLONE les données CPU de l'objet sélectionné (modèle Blender) en
			// un mesh dynamique propre + capture son ancre (transform monde). Sortie :
			// désactive l'édition (le mesh cloné garde son dernier état).
			if (st->editTogglePending) {
				st->editTogglePending = false;
				auto *ms = ctx.renderer->GetMeshSystem();
				if (st->editMode) {
					// Sortie d'édition : on PERSISTE le mesh édité DANS l'objet -> il garde
					// sa nouvelle forme en mode objet (au lieu de retomber sur la primitive
					// partagée). Transfert de propriété (pas de Release).
					if (st->editObjIdx >= 0 && st->editObjIdx < Demo3DState::kNumObj) {
						if (st->objMesh[st->editObjIdx].IsValid() && ms)
							ms->Release(st->objMesh[st->editObjIdx]);
						st->objMesh[st->editObjIdx] = st->editMesh; // l'objet adopte le mesh édité
						st->editMesh = {};
					}
					st->editMode = false;
					st->editObjIdx = -1;
					r3d->ClearEditOverlay();
				} else {
					const int32 sel = st->gizmo.ActiveIndex();
					if (sel < 0) {
						logger.Info("[Demo3D] Sélectionne un objet (clic) avant TAB.\n");
					} else {
						// Source = le mesh DÉJÀ édité de l'objet s'il existe (on continue
						// l'édition), sinon la primitive partagée.
						const NkMeshHandle prim = (sel < 16) ? st->meshSphere : st->meshCube;
						const bool hadEdit = st->objMesh[sel].IsValid();
						const NkMeshHandle src = hadEdit ? st->objMesh[sel] : prim;
						if (!ms || !ms->HasCPUData(src)) {
							logger.Warn("[Demo3D] Mesh sans copie CPU (keepCPU) — édition impossible.\n");
						} else {
							const uint32 vc = ms->GetVertexCount(src);
							const uint32 ic = ms->GetIndexCount(src);
							const auto *sv = (const renderer::NkVertex3D *)ms->GetVertices(src);
							const uint32 *si = ms->GetIndices(src);
							// Construit l'AUTORITÉ half-edge n-gon (quadify) depuis la primitive
							// triangulée, puis génère le mesh de rendu (Demo3D_SyncFromHE).
							st->editHE.BuildFromIndexed(sv, vc, si, ic, /*quadify*/ true);
							st->editHistory.Clear();   // nouvel objet en édition -> historique neuf
							st->editRecorder.Clear();  // journal des commandes neuf
							st->editModifiers.Clear(); // pile de modificateurs neuve
							st->editActiveMod = -1;
							st->editBase = st->editHE; // maillage de BASE pour le rejeu (modificateurs/IA)
							st->editReplayStep = -1;
							st->vertSel.Clear();
							st->vertSel.Resize(st->editHE.VertCount());
							for (uint32 i = 0; i < st->editHE.VertCount(); i++)
								st->vertSel[i] = 0;
							st->editMesh = {};
							Demo3D_SyncFromHE(st, ms);
							// Le mesh objet a été cloné dans editHE -> on le libère ; il sera
							// ré-adopté (mis à jour) à la sortie d'édition.
							if (hadEdit) {
								ms->Release(st->objMesh[sel]);
								st->objMesh[sel] = {};
							}
							// Ancre = transform MONDE de l'objet (base repos + delta gizmo).
							st->editAnchor = st->gizmo.Apply(sel, Demo3D_ObjBase(sel));
							st->editAnchorInv = st->editAnchor.Inverse();
							st->editObjIdx = sel;
							// Capture le matériau de l'objet -> le mesh édité garde sa couleur PBR.
							Demo3D_ObjMaterial(sel, st->editObjTint, st->editObjMetallic, st->editObjRoughness);
							st->editGizmo.ClearSelection();
							st->editWasDragging = false;
							st->editOverlayDirty = true;
							st->editMode = true;
							logger.Info("[Demo3D] EDIT MODE objet #{0} ({1} vertices).\n", sel, vc);
						}
					}
				}
			}

			// ── Caméra : ÉDITEUR (orbit/pan/zoom, Blender) ou SIMULATION (fly), via
			//    les contrôleurs RÉUTILISABLES du moteur. F bascule. NK_FIX_CAM fige.
			//    Éditeur : orbit=clic MILIEU, pan=Shift+MILIEU, zoom=molette.
			//    Simulation : regard=clic DROIT, déplacement=WASD + E/Q (Shift=rapide).
			NkCamera3DData camData;
			camData.up = {0.f, 1.f, 0.f};
			camData.fovY = 60.f;
			camData.aspect = (float32)ctx.width / (float32)ctx.height;
			camData.nearPlane = 0.1f;
			camData.farPlane = 100.f;
			NkCamera3D cam(camData);

			const float32 wheel = (float32)st->wheelAccum;
			st->wheelAccum = 0.0;
			if (!fixcam) {
				// FIX drift caméra : delta RECALCULÉ par frame (frameMDX/MDY = pos courante -
				// pos précédente, = 0 sans mouvement), et NON NkInput.MouseDelta*() qui reste
				// FIGÉ à sa dernière valeur non nulle quand la souris s'arrête -> sinon
				// clic-milieu/droit maintenu SANS bouger faisait dériver orbit/pan/look.
				// (Même correctif que les 2 gizmos, cf. gin.mouseDX = frameMDX plus bas.)
				const float32 mdx = frameMDX;
				const float32 mdy = frameMDY;
				const bool shift = NkInput.IsKeyDown(NkKey::NK_LSHIFT);
				if (st->useSimCam) {
					if (NkInput.IsMouseDown(NkMouseButton::NK_MB_RIGHT))
						st->simCam.Look(mdx, -mdy);
					const float32 spd = (shift ? 12.f : 4.f) * dt;
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
					st->simCam.Move(fwd, rgt, up);
					if (wheel != 0.f)
						st->simCam.Move(wheel * 0.6f, 0.f, 0.f); // molette = avancer
					st->simCam.Apply(cam);
				} else {
					const bool ctrl = NkInput.IsKeyDown(NkKey::NK_LCTRL) || NkInput.IsKeyDown(NkKey::NK_RCTRL);
					// FIX 2 : pivot d'orbite = CENTROÏDE de la sélection (façon Blender
					// « orbit around selection »). Le centroïde vient du gizmo actif (objet
					// hors édit mode, vertices en édit mode) — barycentre calculé au dernier
					// Update() (GetPivot()). Sans sélection -> pivot inchangé (cible courante).
					bool haveSelPivot = false;
					NkVec3f selPivot = {0.f, 0.f, 0.f};
					if (st->editMode) {
						if (st->editGizmo.HasSelection()) {
							selPivot = st->editGizmo.GetPivot();
							haveSelPivot = true;
						}
					} else if (st->gizmo.HasSelection()) {
						selPivot = st->gizmo.GetPivot();
						haveSelPivot = true;
					}
					if (NkInput.IsMouseDown(NkMouseButton::NK_MB_MIDDLE)) {
						if (shift)
							st->editorCam.Pan(-mdx, -mdy); // "grab" façon Blender : on tire la scène (axes inversés)
						else {
							if (mdx != 0.f || mdy != 0.f)
								st->orthoView = false; // orbite libre -> perspective (Blender)
							// Orbite RIGIDE autour du centroïde de la sélection (position ET
							// cible tournent ENSEMBLE) : aucun re-visée du pivot -> AUCUN saut
							// au premier orbit. Sans sélection : orbite normale autour de la
							// cible courante. Le pan (ci-dessus) reste intact (jamais re-pivoté).
							if (haveSelPivot)
								st->editorCam.OrbitAroundPivot(selPivot, mdx, mdy);
							else
								st->editorCam.Rotate(mdx, mdy);
						}
					}
					// Molette façon Blender : seule = ZOOM ; Shift+molette = PAN VERTICAL ;
					// Ctrl+molette = PAN HORIZONTAL. (mdx/mdy pixels ~10-20 ; une crantée de
					// molette ~1 -> multiplier pour un pan comparable.)
					if (wheel != 0.f) {
						const float32 step = wheel * 22.f;
						if (shift)
							st->editorCam.Pan(0.f, step); // vertical
						else if (ctrl)
							st->editorCam.Pan(step, 0.f); // horizontal
						else
							st->editorCam.Zoom(wheel); // zoom
					}
					// Nav éditeur façon Blender = souris uniquement (molette milieu / Shift+milieu /
					// molette). Pas de WASD ici -> G/R/S/A restent libres pour le gizmo/sélection.
					st->editorCam.Apply(cam);
					// Projection ORTHO en vue axiale (façon Blender) : orthoSize dérivé de la
					// distance pour un cadrage comparable à la perspective (demi-hauteur ≈ d·tan(fov/2)).
					if (st->orthoView) {
						const float32 dist = (cam.GetPosition() - cam.GetTarget()).Len();
						cam.SetOrtho(true, dist * 0.55f);
						// Option : afficher la grille en vue axiale ortho (façon Blender).
						r3d->SetInfiniteGridEnabled(st->viewGridInOrtho);
					} else {
						cam.SetOrtho(false);
					}
				}
			} else {
				st->editorCam.Apply(cam); // NK_FIX_CAM : pose figée déterministe
			}

			// ── Lights ───────────────────────────────────────────────────────────
			NkSceneContext sctx;
			sctx.camera = cam;
			sctx.time = ctx.totalTime;

			// Soleil directionnel
			NkLightDesc sun;
			sun.type = NkLightType::NK_DIRECTIONAL;
			sun.direction = {-0.4f, -1.f, -0.3f};
			sun.color = {1.f, 0.95f, 0.85f};
			sun.intensity = 3.f;
			sun.castShadow = true;
			sun.shadowStatic = true; // NkVSM v1 cache : sun ne bouge pas
			sctx.lights.PushBack(sun);

			// Lumiere ponctuelle rouge — avec cube cookie "lantern" (E.6b).
			// Boostee (intensity 12, range 10) pour que le pattern X soit clair
			// meme face au sun + spot. Position legerement haute pour eviter
			// d'etre dans le sol.
			NkLightDesc redLight;
			redLight.type = NkLightType::NK_POINT;
			redLight.position = {3.f, 2.5f, 0.f};
			redLight.color = {1.f, 0.2f, 0.1f};
			redLight.intensity = 12.f;
			redLight.range = 10.f;
			redLight.cookieIdx = 0;		  // utilise le cube bind au slot 0
			redLight.castShadow = true;	  // NkVSM : cubemap 6 faces shadow
			redLight.shadowStatic = true; // position fixe
			sctx.lights.PushBack(redLight);

			// Fill bleue
			NkLightDesc blue;
			blue.type = NkLightType::NK_POINT;
			blue.position = {-2.f, 1.f, 1.f};
			blue.color = {0.2f, 0.5f, 1.f};
			blue.intensity = 2.5f;
			blue.range = 8.f;
			blue.castShadow = true;	  // NkVSM : cubemap 6 faces shadow
			blue.shadowStatic = true; // position fixe
			sctx.lights.PushBack(blue);

			// E.6 : Spot light avec cookie procedural "window bars" projete au sol.
			// Tournant lentement pour montrer la projection dynamique.
			NkLightDesc spot;
			spot.type = NkLightType::NK_SPOT;
			spot.position = {3.f * cosf(ctx.totalTime * 0.3f), 4.f, 3.f * sinf(ctx.totalTime * 0.3f)};
			spot.direction = NkVec3f{0.f, 0.f, 0.f} - spot.position; // pointe vers origine
			spot.direction = spot.direction.Normalized();
			spot.color = {1.f, 0.95f, 0.85f};
			spot.intensity = 8.f;
			spot.range = 10.f;
			spot.innerAngle = 18.f;
			spot.outerAngle = 28.f;
			spot.cookieIdx = 0;		// utilise le slot bind par Init
			spot.castShadow = true; // NkVSM : 1 tile shadow map per spot
			sctx.lights.PushBack(spot);

			sctx.ambientIntensity = 0.15f;

			r3d->BeginScene(sctx);

			// Transform utilisateur (décalage gizmo) appliqué à un objet : délégué au
			// composant NkGizmo3D (source unique : draw calls ET pick/marqueur passent par lui).
			// FIX 1 (contour de sélection qui suit l'objet) : on MÉMORISE la matrice EXACTE
			// utilisée pour dessiner l'objet actif, afin de la réutiliser telle quelle pour le
			// masque de contour (SubmitSelection). Sinon le contour recalculait userXform APRÈS
			// gizmo.Update() (qui applique le drag) alors que l'objet a été dessiné AVANT ->
			// l'objet et son liseré utilisaient deux états de transform décalés d'une frame
			// pendant translate/scale/rotate (et, pour le cube central animé, la base figée
			// Demo3D_ObjBase au lieu de sa matrice animée). Capturer la matrice dessinée garantit
			// que l'objet ET le contour partagent EXACTEMENT la même transform.
			const int32 selDrawIdx = st->gizmo.ActiveIndex();
			NkMat4f selDrawXform = NkMat4f::Identity();
			bool selDrawValid = false;
			auto userXform = [&](int32 idx, const NkMat4f &base) {
				const NkMat4f m = st->gizmo.Apply(idx, base);
				if (idx == selDrawIdx) {
					selDrawXform = m;
					selDrawValid = true;
				}
				return m;
			};

			// Couleur EFFECTIVE d'un objet : la couleur uniforme (gris/custom) est une
			// propriété du MODE D'AFFICHAGE SOLID/WIREFRAME UNIQUEMENT (façon Blender), PAS
			// de l'edit mode. L'edit mode fonctionne dans N'IMPORTE QUEL mode d'affichage
			// (RENDERED garde le matériau PBR, NORMAL/UV/AO gardent leur canal, etc.).
			const bool grayActive = (st->unlitColorMode != 0) && (st->shadingMode == 1 || st->shadingMode == 2);
			auto effTint = [st, grayActive](NkVec3f matTint) -> NkVec3f {
				if (!grayActive)
					return matTint;
				return (st->unlitColorMode == 2) ? st->unlitCustom : st->unlitGray;
			};
			// Mesh EFFECTIF d'un objet : son mesh édité persistant s'il existe, sinon la
			// primitive partagée. Permet à l'édition de survivre au retour en mode objet.
			auto meshFor = [st](int32 idx, NkMeshHandle prim) -> NkMeshHandle {
				return (idx >= 0 && idx < Demo3DState::kNumObj && st->objMesh[idx].IsValid()) ? st->objMesh[idx] : prim;
			};

			// ── Sol ──────────────────────────────────────────────────────────────
			// RETIRÉ : la grille infinie sert désormais de sol de référence (façon Blender/
			// Unreal). Un sol solide coplanaire au plan y=0 de la grille provoquait du
			// z-fighting sur GL/DX (grille qui semblait NON coplanaire / inclinée). Sans sol,
			// plus aucune surface coplanaire -> grille propre sur tous les backends.
			// NB : sans sol, pas de récepteur d'ombres au sol dans cette démo (les casters
			// castent quand même dans l'atlas). Pour ré-afficher les ombres au sol, remettre
			// un sol ET décaler la grille (planeY) ou la rendre en depth-bias constant.
			// NK_GRID_CLEAN : on saute le sol opaque + on active les axes fins du shader
			// (X rouge / Z bleu) pour voir clairement la grille infinie.
			if (gridClean) {
				static bool gclInit = false;
				if (!gclInit) {
					gclInit = true;
					auto &g = r3d->GetInfiniteGridParams();
					g.showAxes = true;						   // axes sol FINS AA du shader
					g.axisXColor = {0.90f, 0.20f, 0.25f, 1.f}; // X rouge
					g.axisZColor = {0.20f, 0.45f, 0.90f, 1.f}; // Z bleu
					g.lineColor = {0.55f, 0.58f, 0.66f, 1.0f}; // lignes un peu plus lisibles
					g.cellColor = {0.10f, 0.11f, 0.13f, 0.0f}; // pas de remplissage opaque
				}
			}
			if (!gridClean) {
				NkDrawCall3D dc;
				dc.mesh = st->meshPlane;
				dc.transform = NkMat4f::Scale({40.f, 1.f, 40.f}); // sol AGRANDI (80x80)
				dc.aabb = {{-40, 0, -40}, {40, 0, 40}};
				dc.castShadow = false; // reçoit les ombres (pas caster)
				dc.tint = effTint({0.12f, 0.12f, 0.13f});
				dc.metallic = 0.f;
				dc.roughness = 0.92f;
				r3d->Submit(dc);
			}

			// ── Grille 4x4 de spheres : grille PBR canonique ─────────────────────
			// Colonnes -> metallic (0..1) : voir l'effet F0 changer
			// Lignes   -> roughness (~0..1) : voir le blur GGX changer
			// La sphere top-left (col=0, row=0) est dielectric mirror -> reflet net
			// du sky. Top-right (col=3, row=0) est metal poli -> reflet teinte par
			// l'albedo. Bottom-row : surfaces rugueuses, ambient diffus dominant.
			for (int row = 0; row < 4; row++) {
				for (int col = 0; col < 4; col++) {
					float32 x = (col - 1.5f) * 1.2f;
					float32 z = (row - 1.5f) * 1.2f;

					NkDrawCall3D dc;
					dc.mesh = meshFor(row * 4 + col, st->meshSphere);
					dc.transform = userXform(row * 4 + col, // idx pick = row*4+col
											 NkMat4f::Translate({x, 0.5f, z}) * NkMat4f::Scale({0.45f, 0.45f, 0.45f}));
					dc.aabb = {{x - 0.25f, 0.25f, z - 0.25f}, {x + 0.25f, 0.75f, z + 0.25f}};
					dc.tint = effTint({(float32)col / 3.f, (float32)row / 3.f, 0.7f});
					dc.metallic = (float32)col / 3.f;				   // 0, 0.33, 0.66, 1
					dc.roughness = 0.05f + (float32)row / 3.f * 0.95f; // 0.05 .. 1
					// En Edit Mode, l'objet édité est remplacé par son clone (plus bas).
					if (!(st->editMode && st->editObjIdx == row * 4 + col))
						r3d->Submit(dc);
				}
			}

			// ── DÉMO GPU INSTANCING : grille 8x8 de cubes via SubmitInstanced ─────
			// Avec NK_INSTANCING_GPU=1 -> 1 SEUL draw (gl_InstanceID). Sans -> chemin
			// object-UBO (N draws, correct). Dans les deux cas, 64 cubes en grille
			// derrière la scène : s'ils sont bien répartis -> instancing OK.
			{
				NkDrawCallInstanced inst;
				inst.mesh = st->meshCube;
				for (int gz = 0; gz < 8; gz++) {
					for (int gx = 0; gx < 8; gx++) {
						const int32 idx = 19 + gz * 8 + gx;
						const float32 x = (gx - 3.5f) * 0.55f;
						const float32 z = (gz - 3.5f) * 0.55f - 4.5f; // décalé derrière le sol
						const NkMat4f xf =
							userXform(idx, NkMat4f::Translate({x, 1.6f, z}) * NkMat4f::Scale({0.18f, 0.18f, 0.18f}));
						const NkVec3f tint = effTint({(float32)gx / 7.f, 0.6f, (float32)gz / 7.f});
						if (st->editMode && st->editObjIdx == idx)
							continue;					  // édité -> via editMesh
						if (st->objMesh[idx].IsValid()) { // édité persisté -> draw séparé
							NkDrawCall3D dc;
							dc.mesh = st->objMesh[idx];
							dc.transform = xf;
							dc.aabb = {{x - 0.15f, 1.4f, z - 0.15f}, {x + 0.15f, 1.8f, z + 0.15f}};
							dc.tint = tint;
							dc.metallic = 0.f;
							dc.roughness = 0.6f;
							r3d->Submit(dc);
						} else {
							inst.transforms.PushBack(xf);
							inst.tints.PushBack(tint);
						}
					}
				}
				inst.aabb = {{-3.f, 1.f, -9.f}, {3.f, 2.5f, 0.f}};
				if (!inst.transforms.Empty())
					r3d->SubmitInstanced(inst);
			}

			// ── Cube central rotatif : metal or poli (gold metallic, low rough) ──
			// Transform calculé UNE fois -> SOURCE UNIQUE partagée par le draw call ET le
			// marqueur de sélection (plus bas). Le marqueur applique cette même matrice à ses
			// coins -> il suit position + rotation + échelle SANS recalcul ni duplication.
			NkMat4f cubeXform = NkMat4f::Translate({0, 0.5f + sinf(ctx.totalTime * 1.5f) * 0.2f, 0}) *
								NkMat4f::RotationY(NkAngle::FromRad(ctx.totalTime * 0.8f)) *
								NkMat4f::Scale({0.6f, 0.6f, 0.6f});
			{
				NkDrawCall3D dc;
				dc.mesh = meshFor(16, st->meshCube);
				dc.transform = userXform(16, cubeXform); // idx pick cube central = 16
				dc.aabb = {{-0.35f, 0.1f, -0.35f}, {0.35f, 0.9f, 0.35f}};
				dc.tint = effTint({1.f, 0.8f, 0.3f}); // gold albedo (ou gris en edit/unlit)
				dc.metallic = 1.f;
				dc.roughness = 0.15f;
				if (!(st->editMode && st->editObjIdx == 16))
					r3d->Submit(dc);
			}

			// ── Colonnes bloquantes pour visualiser les ombres point/spot ────────
			// NkVSM v0 : ces colonnes castent des ombres pour TOUTES les lights
			// (sun + red + blue + spot) -> on doit voir 4 ombres differentes
			// projetees sur le sol pour chaque colonne.
			// Position colonnes :
			//   - col0 a (-4, 1, -2) : devant la red light pour ombre rouge
			//   - col1 a (1, 1, 4)   : derriere les spheres pour ombre spot
			for (int c = 0; c < 2; c++) {
				float32 cx = (c == 0) ? -4.f : 1.f;
				float32 cz = (c == 0) ? -2.f : 4.f;
				NkDrawCall3D dc;
				dc.mesh = meshFor(17 + c, st->meshCube);
				dc.transform = userXform(17 + c, // idx pick colonnes = 17,18
										 NkMat4f::Translate({cx, 1.f, cz}) *
											 NkMat4f::Scale({0.3f, 2.f, 0.3f})); // colonne 2m haute
				dc.aabb = {{cx - 0.2f, 0.f, cz - 0.2f}, {cx + 0.2f, 2.f, cz + 0.2f}};
				dc.tint = effTint({0.7f, 0.7f, 0.7f});
				dc.metallic = 0.f;
				dc.roughness = 0.6f;
				dc.castShadow = true;
				if (!(st->editMode && st->editObjIdx == 17 + c))
					r3d->Submit(dc);
			}

			// ── NkVSM v2 : panneau feuillage ALPHA-TESTED (ombre trouee) ──────────
			// Panneau vertical au-dessus du sol, entre le soleil et le sol : son
			// ombre doit montrer les trous entre les disques (Shadow_AlphaTest).
			if (st->maskedMat) {
				NkDrawCall3D dc;
				dc.mesh = st->meshCube;
				dc.transform = NkMat4f::Translate({4.f, 1.6f, -1.f}) *
							   NkMat4f::RotationY(NkAngle::FromRad(0.6f)) *
							   NkMat4f::Scale({1.6f, 1.2f, 0.03f});
				dc.aabb = {{4.f - 1.7f, 0.3f, -1.f - 1.7f}, {4.f + 1.7f, 2.9f, -1.f + 1.7f}};
				dc.material = st->maskedMat->GetInstHandle();
				dc.castShadow = true;
				r3d->Submit(dc);
			}

			// ── Volet 2 : EDIT MODE — gizmo centroïde + pick VERTEX/EDGE/FACE + marqueurs ──
			// Modèle Blender : la sélection est un ENSEMBLE de vertices ; le gizmo a UNE
			// seule cible = leur centroïde ; le drag applique la transform de groupe (G,
			// autour du centroïde) à tous les vertices sélectionnés. 1/2/3 changent ce que
			// le clic sélectionne (vertex / arête / face). Marqueurs = taille écran (fins).
			// Outils topologie sur le HALF-EDGE (n-gon) : opèrent sur editHE puis régénèrent
			// le mesh de rendu (Demo3D_SyncFromHE). AVANT le bloc édition (peut le faire sortir).
			if (st->editMode && st->editMesh.IsValid()) {
				auto *meshSysT = ctx.renderer->GetMeshSystem();
				if (meshSysT) {
					// Undo/redo d'abord (historique de editHE).
					if (st->editUndoPending) {
						st->editUndoPending = false;
						Demo3D_UndoEdit(st, meshSysT);
						logger.Info("[Demo3D] Undo -> {0} faces (undo={1} redo={2})\n", (int32)st->editHE.FaceCount(),
									st->editHistory.UndoCount(), st->editHistory.RedoCount());
					}
					if (st->editRedoPending) {
						st->editRedoPending = false;
						Demo3D_RedoEdit(st, meshSysT);
						logger.Info("[Demo3D] Redo -> {0} faces (undo={1} redo={2})\n", (int32)st->editHE.FaceCount(),
									st->editHistory.UndoCount(), st->editHistory.RedoCount());
					}
					if (st->editReplayPending) {
						st->editReplayPending = false;
						Demo3D_ReplayEdits(st, meshSysT);
					}
					if (st->editSavePending) {
						st->editSavePending = false;
						Demo3D_SaveSession(st);
					}
					if (st->editLoadPending) {
						st->editLoadPending = false;
						Demo3D_LoadSession(st, meshSysT);
					}
					// Modificateurs (F7/F8/F9 ajouter, F10 vider) : non-destructif, ré-évalué à l'affichage.
					if (st->editAddModPending >= 0) {
						renderer::NkMeshModifier mod;
						mod.type = (renderer::NkModifierType)st->editAddModPending;
						st->editModifiers.Add(mod);
						st->editAddModPending = -1;
						st->editActiveMod = (int32)st->editModifiers.Count() - 1; // le nouveau = actif (réglable [ ])
						Demo3D_SyncFromHE(st, meshSysT);
						const char *nm[3] = {"Mirror (axe X)", "Array (x3)", "Subsurf (1)"};
						logger.Info(
							"[Demo3D] + Modificateur {0} — pile={1} · reglage [ / ] · change actif \\ · vider F10\n",
							nm[(int32)mod.type], st->editModifiers.Count());
					}
					if (st->editClearModPending) {
						st->editClearModPending = false;
						st->editModifiers.Clear();
						st->editActiveMod = -1;
						Demo3D_SyncFromHE(st, meshSysT);
						logger.Info("[Demo3D] Modificateurs vides -> retour a la base editable\n");
					}
					if (st->editModAdjustPending != 0) {
						Demo3D_AdjustMod(st, meshSysT, st->editModAdjustPending);
						st->editModAdjustPending = 0;
					}
					if (st->editModCyclePending) {
						st->editModCyclePending = false;
						Demo3D_CycleActiveMod(st);
					}
					if (st->editExtrudePending) {
						st->editExtrudePending = false;
						Demo3D_ExtrudeHE(st, meshSysT);
						logger.Info("[Demo3D] Extrude -> {0} faces\n", (int32)st->editHE.FaceCount());
					}
					if (st->editMakeFacePending) {
						st->editMakeFacePending = false;
						Demo3D_MakeFaceHE(st, meshSysT);
						logger.Info("[Demo3D] Create face -> {0} faces\n", (int32)st->editHE.FaceCount());
					}
					if (st->editSubdivPending) {
						st->editSubdivPending = false;
						Demo3D_SubdivideHE(st, meshSysT); // subdivCuts passes en 1 commande (1 undo)
						logger.Info("[Demo3D] Subdivide x{0} -> {1} faces\n", st->subdivCuts,
									(int32)st->editHE.FaceCount());
					}
					if (st->editLoopCutPending) {
						st->editLoopCutPending = false;
						Demo3D_LoopCutHE(st, meshSysT);
						logger.Info("[Demo3D] Loop cut -> {0} faces\n", (int32)st->editHE.FaceCount());
					}
					if (st->editBisectPending) {
						st->editBisectPending = false;
						Demo3D_BisectHE(st, meshSysT, st->bisectPt, st->bisectN);
						logger.Info("[Demo3D] Bisect -> {0} faces\n", (int32)st->editHE.FaceCount());
					}
					if (st->editMergePending) {
						st->editMergePending = false;
						Demo3D_MergeHE(st, meshSysT);
						logger.Info("[Demo3D] Merge -> {0} vertices\n", (int32)st->editHE.VertCount());
					}
					if (st->editDeletePending) {
						st->editDeletePending = false;
						Demo3D_DeleteHE(st, meshSysT);
						if (st->editHE.FaceCount() == 0 || st->editIdx.Empty()) {
							if (st->editMesh.IsValid())
								meshSysT->Release(st->editMesh);
							st->editMesh = {};
							st->editMode = false;
							st->editObjIdx = -1;
							r3d->ClearEditOverlay();
							logger.Info("[Demo3D] Delete : mesh vide -> sortie edit mode\n");
						} else
							logger.Info("[Demo3D] Delete faces -> {0} faces\n", (int32)st->editHE.FaceCount());
					}
				}
			}

			if (st->editMode && st->editMesh.IsValid()) {
				auto *meshSysF = ctx.renderer->GetMeshSystem();
				const int32 nv = (int32)st->editRest.Size();

				// Projection monde->écran (même convention que le gizmo).
				const NkVec3f camPos = cam.GetPosition(), camTgt = cam.GetTarget();
				const NkVec3f fwd = (camTgt - camPos).Normalized();
				const NkVec3f rgt = fwd.Cross(NkVec3f{0.f, 1.f, 0.f}).Normalized();
				const NkVec3f upv = rgt.Cross(fwd).Normalized();
				const float32 thY = tanf(60.f * 0.5f * 3.14159265f / 180.f);
				const float32 thX = thY * ((float32)ctx.width / (float32)ctx.height);
				const float32 VW = (float32)ctx.width, VH = (float32)ctx.height;
				auto project = [&](NkVec3f P, float32 &px, float32 &py) -> bool {
					NkVec3f v = P - camPos;
					float32 zc = v.Dot(fwd);
					if (zc <= 1e-3f)
						return false;
					float32 nx = v.Dot(rgt) / (zc * thX), ny = v.Dot(upv) / (zc * thY);
					px = (nx * 0.5f + 0.5f) * VW;
					py = (0.5f - ny * 0.5f) * VH;
					return true;
				};
				auto worldV = [&](int32 i) { return st->editAnchor * st->editRest[i].pos; };
				// (L'occlusion au pick est gérée par un test de PROFONDEUR par rayon curseur,
				//  plus bas dans le bloc de sélection — indépendant de l'orientation caméra.)

				// Centroïde monde de la sélection courante.
				NkVec3f cen = {0.f, 0.f, 0.f};
				int32 selCnt = 0;
				for (int32 i = 0; i < nv; i++)
					if (st->vertSel[i]) {
						cen = cen + worldV(i);
						selCnt++;
					}
				if (selCnt > 0)
					cen = cen * (1.f / (float32)selCnt);

				// Cible unique du gizmo = centroïde.
				renderer::NkGizmoTarget vt[1];
				vt[0] = {NkMat4f::Translate(cen), {0.001f, 0.001f, 0.001f}, 0.0001f};
				const int32 gcount = (selCnt > 0) ? 1 : 0;

				st->editGizmo.SetCamera(camPos, camTgt, 60.f, VW, VH);
				// P2 : repère « Normal » (Z = normale de l'élément sélectionné) recalculé
				// chaque frame HORS drag — pendant un drag on fige le repère, sinon les axes
				// tourneraient sous la souris au fur et à mesure que la surface se déforme.
				if (!st->editGizmo.IsDragging())
					Demo3D_UpdateEditNormalFrame(st);
				renderer::NkGizmoInput gin;
				gin.mouseX = (float32)NkInput.MouseX();
				gin.mouseY = (float32)NkInput.MouseY();
				// Delta RECALCULÉ par frame (pos - posPrécédente), pas MouseDeltaX() qui
				// reste figé à sa dernière valeur non nulle quand la souris s'arrête —
				// sinon le gizmo d'édition dérive à souris immobile (même bug que l'objet).
				gin.mouseDX = frameMDX;
				gin.mouseDY = frameMDY;
				gin.leftPressed = st->pickPending;
				st->pickPending = false;
				gin.leftDown = NkInput.IsMouseDown(NkMouseButton::NK_MB_LEFT);
				gin.shiftDown = NkInput.IsKeyDown(NkKey::NK_LSHIFT) || NkInput.IsKeyDown(NkKey::NK_RSHIFT);
				gin.ctrlDown = NkInput.IsKeyDown(NkKey::NK_LCTRL) || NkInput.IsKeyDown(NkKey::NK_RCTRL);
				gin.lockAxis = -1;
				if (st->editGizmo.IsDragging()) {
					if (NkInput.IsKeyDown(NkKey::NK_X))
						gin.lockAxis = 0;
					else if (NkInput.IsKeyDown(NkKey::NK_Y))
						gin.lockAxis = 1;
					else if (NkInput.IsKeyDown(NkKey::NK_Z))
						gin.lockAxis = 2;
				}
				const bool wasDrag = st->editGizmo.IsDragging();
				st->editGizmo.Update(vt, gcount, gin);
				const bool grabbedHandle = (!wasDrag && st->editGizmo.IsDragging());

				// Clic qui n'a PAS attrapé une poignée -> pick VERTEX/EDGE/FACE en espace écran.
				// COUTEAU/BISECT armé : les 2 clics tracent la ligne -> plan de coupe.
				if (gin.leftPressed && !grabbedHandle && st->knifeArmed) {
					NkVec2f pc{gin.mouseX, gin.mouseY};
					if (!st->knifeHasP0) {
						st->knifeP0 = pc;
						st->knifeHasP0 = true;
					} else {
						auto rayOf = [&](NkVec2f s) -> NkVec3f {
							float32 nx = s.x / VW * 2.f - 1.f, ny = 1.f - s.y / VH * 2.f;
							NkVec3f d = fwd + rgt * (nx * thX) + upv * (ny * thY);
							float32 l = d.Len();
							return (l > 1e-6f) ? d * (1.f / l) : fwd;
						};
						NkVec3f d0 = rayOf(st->knifeP0), d1 = rayOf(pc), nrm = d0.Cross(d1);
						float32 l = nrm.Len();
						if (l > 1e-4f) {
							st->bisectPt = camPos;
							st->bisectN = nrm * (1.f / l);
							st->editBisectPending = true;
						}
						st->knifeArmed = false;
						st->knifeHasP0 = false;
					}
					st->editOverlayDirty = true;
				}
				// Pick sur la BASE (editRest/editIdx = editHE), même sous modificateurs -> on
				// sélectionne/édite la cage de base et le résultat modifié se recalcule.
				if (gin.leftPressed && !grabbedHandle && !st->knifeArmed) {
					st->editOverlayDirty = true; // la sélection va changer -> reconstruire l'overlay
					const float32 mx = gin.mouseX, my = gin.mouseY;
					if (!gin.shiftDown)
						for (int32 i = 0; i < nv; i++)
							st->vertSel[i] = 0;
					// Rayon curseur -> profondeurs d'ENTRÉE (near) et de SORTIE (far) dans le
					// mesh. Un sommet est "devant" (visible) s'il est dans la moitié NEAR
					// (depth < milieu). Test de PROFONDEUR (pas de normale) -> INDÉPENDANT de
					// l'orientation caméra (corrige "impossible de sélectionner selon l'angle").
					const float32 rNdcX = mx / VW * 2.f - 1.f, rNdcY = 1.f - my / VH * 2.f;
					NkVec3f rDir = fwd + rgt * (rNdcX * thX) + upv * (rNdcY * thY);
					{
						float32 l = rDir.Len();
						if (l > 1e-6f)
							rDir = rDir * (1.f / l);
					}
					float32 tNear = 1e30f, tFar = -1e30f;
					int32 nearestTri = -1;
					for (uint32 t = 0; t + 2 < (uint32)st->editIdx.Size(); t += 3) {
						const NkVec3f v0 = worldV(st->editIdx[t]), v1 = worldV(st->editIdx[t + 1]),
									  v2 = worldV(st->editIdx[t + 2]);
						NkVec3f e1 = v1 - v0, e2 = v2 - v0, h = rDir.Cross(e2);
						float32 aa = e1.Dot(h);
						if (fabsf(aa) < 1e-7f)
							continue;
						float32 f = 1.f / aa;
						NkVec3f s = camPos - v0;
						float32 u = f * s.Dot(h);
						if (u < 0.f || u > 1.f)
							continue;
						NkVec3f q = s.Cross(e1);
						float32 vv = f * rDir.Dot(q);
						if (vv < 0.f || u + vv > 1.f)
							continue;
						float32 tt = f * e2.Dot(q);
						if (tt > 1e-4f) {
							if (tt < tNear) {
								tNear = tt;
								nearestTri = (int32)t;
							}
							if (tt > tFar)
								tFar = tt;
						}
					}
					const float32 depthMid = (tNear < 1e29f) ? 0.5f * (tNear + tFar) : 1e30f;
					auto visibleD = [&](NkVec3f w) -> bool {
						if (st->editXray || depthMid >= 1e29f)
							return true;							  // x-ray ou pas de surface -> tout visible
						return (w - camPos).Len() < depthMid + 1e-3f; // moitié near = devant
					};
					// Modes combinables : on cherche le meilleur candidat de CHAQUE mode actif
					// puis on sélectionne le plus proche du curseur (vertex/arête gagnent près
					// d'eux, la face gagne au centre). Façon Blender (vertex/edge/face combinés).
					// VERTEX : parmi les sommets SOUS le curseur (dans un rayon écran), on prend
					// le plus PROCHE DE LA CAMÉRA (= celui devant, visible), pas le plus proche
					// en 2D -> corrige la sélection d'un sommet DERRIÈRE.
					const float32 kVertPx = 14.f;
					int32 bestV = -1;
					float32 bestVdepth = 1e30f;
					if (st->editSelMask & 1) {
						for (int32 i = 0; i < nv; i++) {
							NkVec3f w = worldV(i);
							if (!visibleD(w))
								continue;
							float32 px, py;
							if (project(w, px, py)) {
								float32 d = sqrtf((px - mx) * (px - mx) + (py - my) * (py - my));
								if (d < kVertPx) {
									float32 dep = (w - camPos).Len();
									if (dep < bestVdepth) {
										bestVdepth = dep;
										bestV = i;
									}
								}
							}
						}
					}
					// EDGE : idem, parmi les arêtes sous le curseur on prend celle dont le milieu
					// est le plus proche de la caméra.
					const float32 kEdgePx = 12.f;
					int32 bestEa = -1, bestEb = -1;
					float32 bestEdepth = 1e30f;
					if (st->editSelMask & 2) {
						for (uint32 t = 0; t + 2 < (uint32)st->editIdx.Size(); t += 3) {
							const uint32 e[3][2] = {{st->editIdx[t], st->editIdx[t + 1]},
													{st->editIdx[t + 1], st->editIdx[t + 2]},
													{st->editIdx[t + 2], st->editIdx[t]}};
							for (int32 k = 0; k < 3; k++) {
								NkVec3f wa = worldV(e[k][0]), wb = worldV(e[k][1]);
								NkVec3f mid = (wa + wb) * 0.5f;
								if (!visibleD(mid))
									continue;
								float32 ax, ay, bx, by;
								if (project(wa, ax, ay) && project(wb, bx, by)) {
									float32 dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy,
											tt = (l2 > 1e-6f) ? ((mx - ax) * dx + (my - ay) * dy) / l2 : 0.f;
									tt = tt < 0 ? 0 : (tt > 1 ? 1 : tt);
									float32 cx = ax + tt * dx, cy = ay + tt * dy,
											d = sqrtf((mx - cx) * (mx - cx) + (my - cy) * (my - cy));
									if (d < kEdgePx) {
										float32 dep = (mid - camPos).Len();
										if (dep < bestEdepth) {
											bestEdepth = dep;
											bestEa = (int32)e[k][0];
											bestEb = (int32)e[k][1];
										}
									}
								}
							}
						}
					}
					// FACE : le triangle le plus PROCHE touché par le rayon curseur (déjà calculé
					// ci-dessus = nearestTri). C'est la face EXTERNE visible, jamais celle de derrière.
					const int32 bestFt = (st->editSelMask & 4) ? nearestTri : -1;
					// Élection PAR PRIORITÉ façon Blender : vertex (près d'un sommet) > arête
					// (près d'une arête) > face (rayon). Chacun n'est retenu que dans son seuil.
					if (bestV >= 0) {
						st->vertSel[bestV] = gin.shiftDown ? (uint8)(1 - st->vertSel[bestV]) : 1;
						st->editActiveVert = st->vertSel[bestV] ? bestV : -1; // actif = dernier sélectionné (blanc)
					} else if (bestEa >= 0) {
						st->vertSel[bestEa] = 1;
						st->vertSel[bestEb] = 1;
						st->editActiveVert = bestEb;
					} else if (bestFt >= 0) {
						// FACE N-GON : triangle touché -> sa face n-gon (quadify) -> tous ses sommets.
						renderer::NkEmId f = Demo3D_FaceOfTri(st, (uint32)bestFt / 3u);
						if (f != renderer::NK_EM_INVALID) {
							NkVector<renderer::NkEmId> fv;
							st->editHE.GetFaceVerts(f, fv);
							for (uint32 k = 0; k < (uint32)fv.Size(); k++)
								st->vertSel[fv[k]] = 1;
						} else {
							st->vertSel[st->editIdx[bestFt]] = 1;
							st->vertSel[st->editIdx[bestFt + 1]] = 1;
							st->vertSel[st->editIdx[bestFt + 2]] = 1;
						}
					}
				}
				// Garder la cible-0 sélectionnée pour que les poignées s'affichent.
				if (selCnt > 0 && !st->editGizmo.IsDragging())
					st->editGizmo.SelectAll();

				// Début de drag -> snapshot du pré-état (positions AVANT déplacement) pour l'undo.
				if (!st->editWasDragging && st->editGizmo.IsDragging() && selCnt > 0) {
					Demo3D_PushSel(st);
					st->editDragSnap = st->editHE;
					st->editDragSnapValid = true;
				}
				// Drag : applique la transform de groupe G (autour du centroïde) aux verts sélectionnés.
				if (st->editGizmo.IsDragging() && selCnt > 0) {
					NkMat4f G =
						st->editGizmo.Apply(0, NkMat4f::Translate(cen)) * NkMat4f::Translate({-cen.x, -cen.y, -cen.z});
					for (int32 i = 0; i < nv; i++) {
						st->editLive[i] = st->editRest[i];
						if (st->vertSel[i])
							st->editLive[i].pos = st->editAnchorInv * (G * worldV(i));
					}
					// Update rapide du mesh solide seulement SANS modificateurs (sinon le solide =
					// résultat évalué, nb de sommets ≠ base -> on laisse la cage bouger, le résultat
					// se recale en fin de drag). L'overlay (cage) suit toujours via editLive.
					if (meshSysF && st->editModifiers.Empty())
						meshSysF->UpdateVertices(st->editMesh, st->editLive.Data(), (uint32)nv);
					st->editOverlayDirty = true; // positions changées -> overlay suit le mesh
				}
				// Fin de drag -> baker les positions dans l'AUTORITÉ editHE + RECALCUL des
				// normales (la surface a changé -> l'éclairage suit). Topologie inchangée
				// -> pas de re-triangulation, juste positions+normales.
				if (st->editWasDragging && !st->editGizmo.IsDragging()) {
					const uint32 hv = st->editHE.VertCount();
					for (int32 i = 0; i < nv; i++) {
						st->editRest[i] = st->editLive[i];
						if ((uint32)i < hv)
							st->editHE.verts[i].pos = st->editLive[i].pos;
					}
					st->editHE.RecomputeNormals();
					for (int32 i = 0; i < nv && (uint32)i < hv; i++)
						st->editRest[i].normal = st->editHE.verts[i].normal;
					st->editLive = st->editRest;
					// Sans modificateurs : update rapide. Avec : re-évaluer la pile -> le résultat
					// affiché se recale sur les nouvelles positions de la base.
					if (st->editModifiers.Empty()) {
						if (meshSysF)
							meshSysF->UpdateVertices(st->editMesh, st->editLive.Data(), (uint32)nv);
					} else if (meshSysF) {
						Demo3D_SyncFromHE(st, meshSysF);
					}
					st->editGizmo.ResetSelected();
					st->editOverlayDirty = true;
					// Enregistre le déplacement dans l'historique (pré-état snapshoté au début)
					// ET comme commande Move (donnée rejouable : delta par sommet déplacé).
					if (st->editDragSnapValid) {
						st->editHistory.Commit(st->editDragSnap);
						renderer::NkMeshEditCommand mc;
						mc.op = renderer::NkMeshEditOp::Move;
						const uint32 hv = st->editHE.VertCount(), bv = st->editDragSnap.VertCount();
						for (uint32 i = 0; i < hv && i < bv; ++i) {
							NkVec3f d = st->editHE.verts[i].pos - st->editDragSnap.verts[i].pos;
							if (d.x != 0.f || d.y != 0.f || d.z != 0.f) {
								mc.selection.PushBack(i);
								mc.moveDeltas.PushBack(d);
							}
						}
						if (mc.selection.Size() > 0)
							st->editRecorder.Push(mc);
						st->editDragSnapValid = false;
					}
				}
				st->editWasDragging = st->editGizmo.IsDragging();

				// Dessin du mesh édité à son ancre (les vertices sont en espace LOCAL).
				{
					NkDrawCall3D dc;
					dc.mesh = st->editMesh;
					dc.transform = st->editAnchor;
					dc.aabb = {{-1.f, -1.f, -1.f}, {1.f, 1.f, 1.f}};
					dc.tint = effTint(st->editObjTint); // matériau de l'objet (gris en SOLID/WIREFRAME)
					dc.metallic = st->editObjMetallic;
					dc.roughness = st->editObjRoughness;
					r3d->Submit(dc);
				}

				// ── Overlay d'édition PERSISTANT (batch GPU) ───────────────────────────
				// Reconstruit SEULEMENT quand ça change (entrée/sélection/drag/mode/xray).
				// L'orbite caméra ne reconstruit RIEN : le GPU redessine les buffers gardés
				// -> fluide même sur mesh dense. Occlusion = depth-test (X-ray OFF) ; offset
				// le long de la NORMALE (indépendant caméra) pour vaincre le z-fighting.
				if (st->editOverlayDirty) {
					st->editOverlayDirty = false;
					auto liveW = [&](int32 i) { return st->editAnchor * st->editLive[i].pos; };
					auto normW = [&](int32 i) -> NkVec3f {
						NkVec3f nW = (st->editAnchor * (st->editLive[i].pos + st->editLive[i].normal)) - liveW(i);
						float32 l = nW.Len();
						return (l > 1e-6f) ? nW * (1.f / l) : NkVec3f{0.f, 1.f, 0.f};
					};
					// Rayon monde (dimensionne points + offset).
					NkVec3f bmin{1e30f, 1e30f, 1e30f}, bmax{-1e30f, -1e30f, -1e30f};
					for (int32 i = 0; i < nv; i++) {
						NkVec3f w = liveW(i);
						bmin.x = NkMin(bmin.x, w.x);
						bmin.y = NkMin(bmin.y, w.y);
						bmin.z = NkMin(bmin.z, w.z);
						bmax.x = NkMax(bmax.x, w.x);
						bmax.y = NkMax(bmax.y, w.y);
						bmax.z = NkMax(bmax.z, w.z);
					}
					float32 rad = (bmax - bmin).Len() * 0.5f;
					if (rad < 1e-4f)
						rad = 1.f;
					const float32 dotS = rad * 0.012f; // demi-taille des points (monde)
					// Plus de décalage le long de la normale : cage/points/faces sont
					// COPLANAIRES à la surface. Le depth-bias des pipelines (lignes + fill)
					// évite le z-fighting -> tout COLLE au modèle (pas de flottement).
					auto pushV = [&](NkVector<float> &A, NkVec3f p, NkVec4f c) {
						A.PushBack(p.x);
						A.PushBack(p.y);
						A.PushBack(p.z);
						A.PushBack(c.x);
						A.PushBack(c.y);
						A.PushBack(c.z);
						A.PushBack(c.w);
					};
					// Palette Blender Edit Mode : cage NOIRE fine, sélection JAUNE-ORANGE VIF.
					const NkVec4f cageCol{0.015f, 0.015f, 0.02f, 1.f}; // arête non sélectionnée
					const NkVec4f selEdgeCol{1.f, 0.70f, 0.13f, 1.f};  // arête sélectionnée (vif)
					// ── P0 — CAGE = ARÊTES RÉELLES DU N-GON ──────────────────────────────
					// st->editEdges vient de NkEditMesh::GetUniqueEdges (topologie demi-arête,
					// chaque arête UNE SEULE FOIS, arêtes internes dissoutes par Quadify
					// exclues). On ne dessine JAMAIS les arêtes des triangles de RENDU : sur
					// un quad la diagonale de triangulation n'existe pas (cube = 12 arêtes,
					// pas 18), exactement comme Blender.
					// Micro-décalage RADIAL (depuis le centre de la bbox) pour tuer le
					// z-fighting qui affichait la cage en POINTILLÉS. Radial et NON le long de
					// la normale du sommet : dans une primitive, une arête vive porte DEUX
					// copies de ses sommets (une par face, avec des normales différentes) — un
					// décalage par normale ÉCARTERAIT les deux copies de la même arête et la
					// dédoublerait à l'écran. Le décalage radial est identique pour tous les
					// sommets coïncidents, donc les copies restent superposées. Indépendant de
					// la caméra : l'orbite ne reconstruit toujours rien.
					const NkVec3f bctr = (bmin + bmax) * 0.5f;
					const float32 edgeLift = rad * 0.006f;
					auto liftW = [&](int32 i) {
						const NkVec3f w = liveW(i);
						NkVec3f d = w - bctr;
						const float32 l = d.Len();
						return (l > 1e-5f) ? (w + d * (edgeLift / l)) : (w + normW(i) * edgeLift);
					};
					NkVector<float> L;
					L.Reserve((uint32)st->editEdges.Size() * 7);
					// Passe 0 = arêtes non sélectionnées, passe 1 = sélectionnées (tracées
					// APRÈS -> elles gagnent le z-fight et restent franches).
					for (int32 pass = 0; pass < 2; pass++) {
						for (uint32 e = 0; e + 1 < (uint32)st->editEdges.Size(); e += 2) {
							const uint32 a = st->editEdges[e], b = st->editEdges[e + 1];
							if (a >= (uint32)st->vertSel.Size() || b >= (uint32)st->vertSel.Size())
								continue;
							const bool sel = (st->vertSel[a] != 0 && st->vertSel[b] != 0);
							if (sel != (pass == 1))
								continue;
							pushV(L, liftW((int32)a), sel ? selEdgeCol : cageCol);
							pushV(L, liftW((int32)b), sel ? selEdgeCol : cageCol);
						}
					}
					r3d->SetEditOverlayLines(L.Empty() ? nullptr : L.Data(), (uint32)(L.Size() / 7));
					// POINTS : marqueurs de vertices en SPRITE ÉCRAN-CONSTANT (taille en pixels
					// fixe quel que soit le zoom, façon Blender). Chaque point = un quad dont
					// chaque sommet porte {centre monde, coin en PIXELS, couleur} (9 floats) ;
					// le vertex shader billboarde en espace écran. Mode VERTEX actif seulement.
					(void)dotS;
					// Les marqueurs de VERTICES / centres de face NE passent PLUS par l'overlay
					// point-sprite (rendu « carré CREUX / crochet d'angle » peu fiable, surtout en
					// DX12). Ils sont dessinés en QUADS PLEINS face-caméra via DrawDebugTriangle (MÊME
					// chemin overlay no-depth que le gizmo solide -> correct OpenGL ET DX12), plus bas,
					// chaque frame, hors du batch persistant. On vide donc le batch de points.
					r3d->SetEditOverlayPoints(nullptr, 0);
					// Le REMPLISSAGE des faces sélectionnées ne passe plus par ce batch
					// persistant : il est tracé chaque frame en DrawDebugTriangle (même
					// chemin que les marqueurs, validé DX12 + GL) — cf. bloc ci-dessous.
					r3d->SetEditOverlayTris(nullptr, 0);
					r3d->SetEditOverlayXray(st->editXray);
				}
				// ── P1 — REMPLISSAGE ORANGE TRANSLUCIDE DES FACES SÉLECTIONNÉES ────────
				// Façon Blender : TOUTE la surface de la face sélectionnée est teintée (pas
				// un simple point au centre). On triangule la face N-GON en éventail sur sa
				// VRAIE boucle (editHE), pas sur les triangles de rendu, et on trace via
				// DrawDebugTriangle. overlay=false -> pipeline « DebugTriFill » (LESS_EQUAL +
				// biais négatif, sans écriture de profondeur) : le fill colle à la surface et
				// reste occlus par la géométrie devant. X-ray -> overlay=true (voir à travers).
				// Une face est sélectionnée si TOUS ses sommets le sont (convention Blender) :
				// le fill apparaît donc aussi en mode VERTEX/EDGE, comme dans Blender.
				{
					auto liveWf = [&](int32 i) { return st->editAnchor * st->editLive[i].pos; };
					const NkVec4f faceFill{1.f, 0.55f, 0.06f, 0.36f};
					const uint32 fcntF = (uint32)st->editHE.faces.Size();
					NkVector<renderer::NkEmId> fvf;
					for (uint32 f = 0; f < fcntF; f++) {
						if (!st->editHE.faces[f].alive)
							continue;
						fvf.Clear();
						st->editHE.GetFaceVerts(f, fvf);
						const uint32 fn = (uint32)fvf.Size();
						if (fn < 3)
							continue; // arête fil : pas de surface
						bool allSel = true;
						for (uint32 k = 0; k < fn && allSel; k++) {
							const uint32 vi = fvf[k];
							if (vi >= (uint32)st->vertSel.Size() || !st->vertSel[vi] ||
								vi >= (uint32)st->editLive.Size())
								allSel = false;
						}
						if (!allSel)
							continue;
						const NkVec3f p0 = liveWf((int32)fvf[0]);
						for (uint32 k = 1; k + 1 < fn; k++) // éventail sur la boucle n-gon
							r3d->DrawDebugTriangle(p0, liveWf((int32)fvf[k]), liveWf((int32)fvf[k + 1]), faceFill,
												   0.f, st->editXray);
					}
				}
				// ── Marqueurs VERTEX / centre-de-FACE façon Blender : petits QUADS PLEINS ──────
				// Carré PLEIN (2 triangles) face-caméra, taille ÉCRAN-CONSTANTE (~3 px de côté),
				// via DrawDebugTriangle en overlay no-depth (même chemin que le gizmo solide ->
				// rendu identique OpenGL / DX12). Non sél. = sombre, sél. = orange, actif = blanc ;
				// PLEIN dans tous les cas (fin liseré sombre dessous pour la lisibilité). Rendu
				// chaque frame (suit la caméra pour rester écran-constant).
				{
					auto liveWv = [&](int32 i) { return st->editAnchor * st->editLive[i].pos; };
					// Les marqueurs sont tracés SANS depth-test (fiabilité DX12) : sans filtre,
					// on verrait aussi ceux du DOS du modèle « à travers ». Blender ne les montre
					// qu'en X-ray. Filtre d'orientation : un marqueur dont la normale tourne le
					// dos à la caméra est caché (X-ray OFF), gardé (X-ray ON).
					const NkVec3f orgW = st->editAnchor * NkVec3f{0.f, 0.f, 0.f};
					auto facingCam = [&](NkVec3f w, NkVec3f nLocal) {
						if (st->editXray)
							return true;
						const NkVec3f nW = (st->editAnchor * nLocal) - orgW;
						return nW.Dot(camPos - w) > 0.f;
					};
					// px (demi-côté écran) -> demi-taille MONDE à la profondeur du point.
					const float32 pxToWorld = (2.f * thY) / VH;
					auto fillQuad = [&](NkVec3f w, float32 halfPx, NkVec4f col) {
						float32 d = (w - camPos).Dot(fwd);
						if (d < 1e-3f)
							d = 1e-3f;
						const float32 h = halfPx * pxToWorld * d;
						const NkVec3f rx = rgt * h, uy = upv * h;
						const NkVec3f c00 = w - rx - uy, c10 = w + rx - uy, c11 = w + rx + uy, c01 = w - rx + uy;
						r3d->DrawDebugTriangle(c00, c10, c11, col, 0.f, true);
						r3d->DrawDebugTriangle(c00, c11, c01, col, 0.f, true);
					};
					// Carré PLEIN + fin liseré sombre dessous (les 2 sont PLEINS -> jamais creux).
					const NkVec4f rim{0.f, 0.f, 0.f, 0.9f};
					auto dot = [&](NkVec3f w, float32 core, NkVec4f col) {
						fillQuad(w, core + 0.7f, rim); // liseré sombre (dessous)
						fillQuad(w, core, col);		   // coeur PLEIN (dessus)
					};
					// VERTICES (mode VERTEX) : ~3 px de côté (half ~1.5), discret.
					// Code couleur Blender : NOIR = non sélectionné, ORANGE = sélectionné,
					// BLANC = sommet ACTIF (dernier sélectionné).
					if (st->editSelMask & 1) {
						for (int32 i = 0; i < nv; i++) {
							NkVec3f w = liveWv(i);
							if (!facingCam(w, st->editLive[i].normal))
								continue; // sommet du dos -> caché (sauf X-ray), façon Blender
							if (i == st->editActiveVert)
								dot(w, 2.0f, NkVec4f{1.f, 1.f, 1.f, 1.f}); // actif = BLANC
							else if (i < (int32)st->vertSel.Size() && st->vertSel[i])
								dot(w, 1.8f, NkVec4f{1.f, 0.55f, 0.05f, 1.f}); // sél. = ORANGE
							else
								dot(w, 1.5f, NkVec4f{0.02f, 0.02f, 0.03f, 1.f}); // non sél. = NOIR
						}
					}
					// CENTRES DE FACE (mode FACE) : petit carré plein au barycentre de chaque face.
					if (st->editSelMask & 4) {
						const uint32 fcnt = (uint32)st->editHE.faces.Size();
						NkVector<renderer::NkEmId> fvd;
						for (uint32 f = 0; f < fcnt; f++) {
							if (!st->editHE.faces[f].alive)
								continue;
							fvd.Clear();
							st->editHE.GetFaceVerts(f, fvd);
							const uint32 fn = (uint32)fvd.Size();
							if (fn < 3)
								continue;
							NkVec3f cW{0.f, 0.f, 0.f};
							bool allSel = true;
							for (uint32 k = 0; k < fn; k++) {
								const uint32 vi = fvd[k];
								if (vi < (uint32)st->editLive.Size())
									cW = cW + liveWv((int32)vi);
								if (vi >= (uint32)st->vertSel.Size() || !st->vertSel[vi])
									allSel = false;
							}
							cW = cW * (1.f / (float32)fn);
							if (!facingCam(cW, st->editHE.faces[f].normal))
								continue; // face du dos -> point caché (sauf X-ray)
							if (allSel)
								dot(cW, 1.8f, NkVec4f{1.f, 0.55f, 0.05f, 1.f}); // face sél. = ORANGE
							else
								dot(cW, 1.4f, NkVec4f{0.02f, 0.02f, 0.03f, 1.f}); // face = point NOIR
						}
					}
				}
				// Poignées du gizmo (OVERLAY) — rendu chaque frame (peu de lignes, négligeable).
				// Rendu PLEIN (façon Blender solide) IDENTIQUE au gizmo objet : 2 callbacks
				// (drawLine pour tiges/liserés fins + drawTri pour formes PLEINES : cônes/cubes/
				// rubans). Le 2e callback active la surcharge Draw(drawLine, drawTri) du gizmo —
				// mêmes couleurs d'axe (X rouge, Y vert, Z bleu) et mêmes formes que l'objet.
				st->editGizmo.Draw(
					[&](NkVec3f a, NkVec3f b, NkVec4f c) { r3d->DrawDebugLine(a, b, c, 0.f, true); },
					[&](NkVec3f a, NkVec3f b, NkVec3f c, NkVec4f col) {
						r3d->DrawDebugTriangle(a, b, c, col, 0.f, true);
					});
			}

			// ── Gizmo éditeur (composant réutilisable NkGizmo3D) ────────────────────
			// Table des CIBLES (transform de BASE + demi-extent mesh + rayon de pick),
			// MÊME ordre/indices que les draw calls. Le gizmo compose le décalage
			// utilisateur lui-même (Apply), gère pick + drag + dessin, et rend en OVERLAY.
			{
				renderer::NkGizmoTarget targets[Demo3DState::kNumObj];
				int32 n = 0;
				const NkVec3f H = {0.5f, 0.5f, 0.5f};
				auto *msG = ctx.renderer->GetMeshSystem();
				// Pour un objet ÉDITÉ, le marqueur OBB épouse l'AABB LOCALE réelle du mesh
				// modifié (centre + demi-extent), au lieu du demi-extent fixe de la primitive.
				auto fitTarget = [&](int32 idx, const NkMat4f &base, float32 pickR) -> renderer::NkGizmoTarget {
					if (idx >= 0 && idx < Demo3DState::kNumObj && st->objMesh[idx].IsValid() && msG &&
						msG->HasCPUData(st->objMesh[idx])) {
						const auto *vv = (const renderer::NkVertex3D *)msG->GetVertices(st->objMesh[idx]);
						const uint32 vc = msG->GetVertexCount(st->objMesh[idx]);
						NkVec3f mn{1e30f, 1e30f, 1e30f}, mx{-1e30f, -1e30f, -1e30f};
						for (uint32 i = 0; i < vc; i++) {
							NkVec3f p = vv[i].pos;
							mn.x = NkMin(mn.x, p.x);
							mn.y = NkMin(mn.y, p.y);
							mn.z = NkMin(mn.z, p.z);
							mx.x = NkMax(mx.x, p.x);
							mx.y = NkMax(mx.y, p.y);
							mx.z = NkMax(mx.z, p.z);
						}
						if (vc > 0) {
							NkVec3f c = (mn + mx) * 0.5f, h = (mx - mn) * 0.5f;
							return {base * NkMat4f::Translate(c), h, pickR};
						}
					}
					return {base, H, pickR};
				};
				for (int row = 0; row < 4; row++)
					for (int col = 0; col < 4; col++) // 16 sphères
						targets[n++] = fitTarget(row * 4 + col,
												 NkMat4f::Translate({(col - 1.5f) * 1.2f, 0.5f, (row - 1.5f) * 1.2f}) *
													 NkMat4f::Scale({0.45f, 0.45f, 0.45f}),
												 0.35f);
				targets[n++] = fitTarget(16, cubeXform, 0.45f); // cube central
				targets[n++] = fitTarget(17, NkMat4f::Translate({-4.f, 1.f, -2.f}) * NkMat4f::Scale({0.3f, 2.f, 0.3f}),
										 1.3f); // colonne 0
				targets[n++] = fitTarget(18, NkMat4f::Translate({1.f, 1.f, 4.f}) * NkMat4f::Scale({0.3f, 2.f, 0.3f}),
										 1.3f); // colonne 1
				for (int gz = 0; gz < 8; gz++)
					for (int gx = 0; gx < 8; gx++) // 64 cubes INSTANCIÉS
						targets[n++] =
							fitTarget(19 + gz * 8 + gx,
									  NkMat4f::Translate({(gx - 3.5f) * 0.55f, 1.6f, (gz - 3.5f) * 0.55f - 4.5f}) *
										  NkMat4f::Scale({0.18f, 0.18f, 0.18f}),
									  0.2f);

				// Gizmo OBJET actif UNIQUEMENT hors Edit Mode (sinon c'est le gizmo vertices).
				if (!st->editMode) {
					// NK_GIZMO_SHOW=<mode> : sélectionne le cube central + fixe le mode
					// (0=Translate 1=Rotate 2=Scale 3=Combine) pour une CAPTURE headless du
					// gizmo (sans souris). One-shot. Sans cette variable : comportement normal.
					static bool gizShowInit = false;
					if (!gizShowInit) {
						gizShowInit = true;
						if (const char *gv = getenv("NK_GIZMO_SHOW")) {
							// NK_GIZMO_OBJ=<idx> : objet à sélectionner (défaut 14 = sphère mate
							// avant, hors halo de la lumière centrale -> gizmo bien lisible).
							int32 gobj = 14;
							if (const char *go = getenv("NK_GIZMO_OBJ"))
								gobj = atoi(go);
							st->gizmo.Select(gobj);
							st->gizmo.SetMode(atoi(gv) & 3);
						}
						// NK_SEL_TEST_XFORM=1 : non-régression du FIX 1 (contour qui suit
						// l'objet). Sélectionne un objet (NK_GIZMO_OBJ, défaut 14) et lui
						// applique une transform NON-IDENTITÉ FIGÉE (translation + rotation Y
						// connues) par le MÊME chemin interne que le drag gizmo
						// (SetSelectedTransform -> mTr/mRot). En capture, le liseré orange DOIT
						// épouser l'objet à sa position TRANSFORMÉE (pas à l'origine).
						if (getenv("NK_SEL_TEST_XFORM")) {
							int32 tobj = 14;
							if (const char *go = getenv("NK_GIZMO_OBJ"))
								tobj = atoi(go);
							st->gizmo.Select(tobj);
							st->gizmo.SetSelectedTransform({1.4f, 0.6f, 0.f},
														   NkMat4f::RotationY(NkAngle::FromRad(0.9f)),
														   {0.f, 0.f, 0.f});
						}
						// NK_GIZMO_OBB=0 : masque la cage OBB de sélection (gizmo SEUL, épuré).
						if (const char *ob = getenv("NK_GIZMO_OBB"))
							st->gizmo.SetDrawObjectBounds(!(ob[0] == '0'));
						// NK_SELECT_OUTLINE=1 : liseré silhouette façon Blender (OPTION
						// distincte de l'AABB) — fin contour orange qui épouse le maillage
						// sélectionné, via post-process de détection de bord. Couleur/épaisseur
						// surchargeables (NK_OUTLINE_THICK). L'AABB reste dispo en parallèle.
						// NB : le liseré silhouette est désormais ACTIF PAR DÉFAUT (indicateur
						// de sélection par défaut, à la place de l'AABB). NK_SELECT_OUTLINE=0
						// le désactive ; NK_OUTLINE_THICK règle l'épaisseur.
						if (const char *so = getenv("NK_SELECT_OUTLINE")) {
							float32 thick = 3.f;
							if (const char *ot = getenv("NK_OUTLINE_THICK"))
								thick = (float32)atof(ot);
							r3d->SetSelectionOutline(!(so[0] == '0'), {1.f, 0.45f, 0.05f, 1.f}, thick);
						} else if (const char *ot = getenv("NK_OUTLINE_THICK")) {
							r3d->SetSelectionOutline(true, {1.f, 0.45f, 0.05f, 1.f}, (float32)atof(ot));
						}
						// NK_SHADING=<0..5> : force le mode d'affichage (0=RENDERED, 1=SOLID
						// unlit, 2=WIREFRAME, 3=NORMAL, 4=UV, 5=AO) pour une capture headless.
						if (const char *sh = getenv("NK_SHADING")) {
							st->shadingMode = atoi(sh) % 6;
							const int32 vm[6] = {0, 1, 1, 2, 3, 4};
							r3d->SetWireframe(st->shadingMode == 2);
							r3d->SetViewMode(vm[st->shadingMode]);
						}
					}
					st->gizmo.SetCamera(cam.GetPosition(), cam.GetTarget(), 60.f, (float32)ctx.width,
										(float32)ctx.height);
					renderer::NkGizmoInput gin;
					gin.mouseX = (float32)NkInput.MouseX();
					gin.mouseY = (float32)NkInput.MouseY();
					gin.mouseDX = frameMDX;
					gin.mouseDY = frameMDY;
					gin.leftPressed = st->pickPending;
					st->pickPending = false;
					gin.leftDown = NkInput.IsMouseDown(NkMouseButton::NK_MB_LEFT);
					gin.shiftDown = NkInput.IsKeyDown(NkKey::NK_LSHIFT) || NkInput.IsKeyDown(NkKey::NK_RSHIFT);
					gin.ctrlDown = NkInput.IsKeyDown(NkKey::NK_LCTRL) || NkInput.IsKeyDown(NkKey::NK_RCTRL); // SNAP
					gin.lockAxis = -1;
					if (st->gizmo.IsDragging()) {
						if (NkInput.IsKeyDown(NkKey::NK_X))
							gin.lockAxis = 0;
						else if (NkInput.IsKeyDown(NkKey::NK_Y))
							gin.lockAxis = 1;
						else if (NkInput.IsKeyDown(NkKey::NK_Z))
							gin.lockAxis = 2;
					}
					st->gizmo.Update(targets, n, gin);

					// ── Sélection « outline silhouette » (option NK_SELECT_OUTLINE) ──
					// Soumet l'objet ACTIF au masque de silhouette : le liseré orange
					// épousera son maillage (post-process edge-detect). Limité aux objets
					// NON instanciés (spheres 0-15, cube 16, colonnes 17-18) ; les cubes
					// instanciés (19+) n'ont pas de transform per-instance côté drawcall
					// simple -> hors périmètre de cette démo.
					if (r3d->IsSelectionOutlineEnabled() && st->gizmo.HasSelection()) {
						const int32 sel = st->gizmo.ActiveIndex();
						if (sel >= 0 && sel < 19) {
							NkDrawCall3D sdc;
							sdc.mesh = meshFor(sel, (sel < 16) ? st->meshSphere : st->meshCube);
							// FIX 1 : réutilise la matrice EXACTE ayant servi à dessiner l'objet
							// actif (capturée AVANT gizmo.Update). Fallback (nouveau pick cette
							// frame, indices désynchronisés) : recalcul depuis la base de repos.
							sdc.transform = (selDrawValid && sel == selDrawIdx)
												? selDrawXform
												: userXform(sel, Demo3D_ObjBase(sel));
							r3d->SubmitSelection(sdc);
						}
					}

					// Rendu PLEIN (façon Blender solide) : lignes fines (tiges/liserés) via
					// DrawDebugLine + formes PLEINES (cônes/cubes/rubans) via DrawDebugTriangle,
					// toutes en overlay (au-dessus de la scène, alpha-blend). Le 2e callback active
					// la surcharge Draw(drawLine, drawTri) du gizmo.
					// NK_OUTLINE_ONLY=1 : masque le gizmo pour une capture « liseré silhouette
					// SEUL » (le contour devient l'unique indicateur de sélection à l'écran).
					static int outlineOnly = -1;
					if (outlineOnly == -1) {
						const char *v = getenv("NK_OUTLINE_ONLY");
						outlineOnly = (v && v[0] && v[0] != '0') ? 1 : 0;
					}
					if (!outlineOnly)
						st->gizmo.Draw(
							[&](NkVec3f a, NkVec3f b, NkVec4f c) { r3d->DrawDebugLine(a, b, c, 0.f, true); },
							[&](NkVec3f a, NkVec3f b, NkVec3f c, NkVec4f col) {
								r3d->DrawDebugTriangle(a, b, c, col, 0.f, true);
							});
				}
			}

			// (Le gizmo d'édition de vertices + pick VERTEX/EDGE/FACE + marqueurs fins
			//  sont désormais gérés plus haut, dans la section « EDIT MODE ».)

			// ── Axes X/Y/Z en LIGNES 3D réelles (DrawDebugLine) : correct partout ───
			// (perspective, ancrés à l'origine, parallèles aux objets verticaux, top/bottom OK).
			// Remplace les axes du SHADER grille (désactivés via g.showAxes=false à l'Init) qui
			// avaient des artefacts de projection sur l'axe Y. Étendus loin -> effet "infini".
			// NK_GRID_CLEAN : on masque ces axes debug 3D épais (la grille du shader dessine
			// alors ses PROPRES axes sol fins AA -> rendu épuré pour juger la grille).
			if (!gridClean) {
				const float32 A = 1000.f;
				const float32 h = 0.02f; // légèrement au-dessus du sol/grille -> pas de z-fight (pointillés)
				r3d->DrawDebugLine({-A, h, 0.f}, {A, h, 0.f}, {1.f, 0.f, 0.f, 1.f});	 // X rouge
				r3d->DrawDebugLine({0.f, -A, 0.f}, {0.f, A, 0.f}, {0.f, 1.f, 0.f, 1.f}); // Y vert
				r3d->DrawDebugLine({0.f, h, -A}, {0.f, h, A}, {0.f, 0.f, 1.f, 1.f});	 // Z bleu
			}

			// ── Overlay ──────────────────────────────────────────────────────────
			if (auto *overlay = ctx.renderer->GetOverlay()) {
				overlay->BeginOverlay(ctx.renderer->GetCmd(), ctx.width, ctx.height);
				overlay->DrawStats(ctx.renderer->GetStats());
				{
					const char *sm[6] = {"RENDERED", "SOLID", "WIREFRAME", "NORMAL", "UV", "AO"};
					const char *mc[5] = {"Studio", "Clay", "Metal", "Toon", "Chrome(tex)"};
					const char *cm[3] = {"MATERIAL", "GRIS", "CUSTOM"};
					int32 mcId = 0;
					if (auto *r3dh = ctx.renderer->GetRender3D())
						mcId = r3dh->Matcap();
					// MatCap pertinent seulement en SOLID/WIREFRAME (modes 1 et 2).
					if (st->shadingMode == 1 || st->shadingMode == 2)
						overlay->DrawText(
							{20.f, 35.f},
							"Demo 3D  |  API : %s  |  Affichage(Z): %s  |  MatCap(M): %s  |  Couleur(B): %s",
							NkGraphicsApiName(ctx.api), sm[st->shadingMode % 6], mc[mcId % 5],
							cm[st->unlitColorMode % 3]);
					else
						overlay->DrawText({20.f, 35.f}, "Demo 3D  |  API : %s  |  Affichage(Z): %s  |  Couleur(B): %s",
										  NkGraphicsApiName(ctx.api), sm[st->shadingMode % 6],
										  cm[st->unlitColorMode % 3]);
				}
				overlay->DrawText({20.f, 55.f}, "FPS approx: %.1f  |  dt: %.2f ms", dt > 1e-4f ? 1.f / dt : 0.f,
								  dt * 1000.f);
				// Phase H : indication visuelle du chargement texture file-based.
				overlay->DrawText({20.f, 75.f}, "[Phase H] Texture file-based : %s",
								  st->phaseHLoadOk ? "test_pattern.png LOAD OK" : "fallback procedural");
				// Aide gizmo : mode + orientation + rappel des touches.
				const char *gmName[4] = {"TRANSLATE", "ROTATE", "SCALE", "COMBINE (T+R+S)"};
				const char *orName[3] = {"GLOBAL", "LOCAL", "NORMAL"};
				const char *seName[3] = {"VERTEX", "EDGE", "FACE"};
				if (st->editMode) {
					char modeStr[8];
					int mi = 0;
					if (st->editSelMask & 1)
						modeStr[mi++] = 'V';
					if (st->editSelMask & 2)
						modeStr[mi++] = 'E';
					if (st->editSelMask & 4)
						modeStr[mi++] = 'F';
					modeStr[mi] = '\0';
					(void)seName;
					// L'orientation courante du gizmo est affichée AUSSI en edit mode (P2) :
					// « NORMAL* » = un repère d'élément (normale de face/arête/sommet) est
					// effectivement posé ; « NORMAL » sans étoile = repli Local.
					const int32 eo = st->editGizmo.Orientation() % 3;
					const bool nf = (eo == 2) && st->editGizmo.HasNormalFrame();
					overlay->DrawText({20.f, 100.f},
									  "EDIT MODE (obj #%d)  |  Modes(1/2/3,Shift=combi): %s  |  Gizmo(G/R/S/C): %s  |  "
									  "Orient(,): %s%s  |  X-ray(Alt+Z): %s",
									  st->editObjIdx, modeStr, gmName[st->editGizmo.Mode() & 3], orName[eo],
									  nf ? "*" : "", st->editXray ? "ON" : "OFF");
					overlay->DrawText({20.f, 118.f},
									  "E=extrude %s(Sh:%s) X=suppr M=souder(Sh:%s) W=subdiv(Sh:x%d) "
									  "Ctrl+R=loopcut(Sh:x%d) K=couteau%s | TAB=sortir",
									  (st->editSelMask & 4)	  ? "FACES"
									  : (st->editSelMask & 2) ? "ARETES"
															  : "SOMMETS",
									  st->extrudeIndividual ? "indiv" : "region",
									  (st->mergeMode == 2)	 ? "last"
									  : (st->mergeMode == 1) ? "first"
															 : "center",
									  st->subdivCuts, st->loopCuts,
									  st->knifeArmed ? (st->knifeHasP0 ? "[2e pt]" : "[1er pt]") : "");
				} else {
					overlay->DrawText(
						{20.f, 100.f},
						"OBJET  |  Gizmo(G/R/S/C): %s  |  Orient(,): %s   |  TAB=editer l'objet selectionne",
						gmName[st->gizmo.Mode() & 3], orName[st->gizmo.Orientation() % 3]);
					overlay->DrawText({20.f, 118.f}, "clic=sel  Shift+clic=multi  A/Alt+A=tout/rien  Alt+G/R/S=clear  "
													 "|  Ctrl=snap  X/Y/Z=verrou axe");
				}

				// ── Debug panel : params shadow live-tunable ───────────────────────
				// Background semi-transparent en haut a droite
				if (auto *r2d = ctx.renderer->GetRender2D()) {
					NkRectF panel = {(float32)ctx.width - 320.f, 10.f, 310.f, 180.f};
					r2d->FillRect(panel, {0.f, 0.f, 0.f, 0.6f});
				}
				const float32 px = (float32)ctx.width - 310.f;
				overlay->DrawText({px, 30.f}, "== Shadow tweak (panel debug) ==");
				if (auto *sh = ctx.renderer->GetShadow()) {
					const auto &cfg = sh->GetConfig();
					overlay->DrawText({px, 50.f}, "F5/F6    bias     : %.4f", cfg.shadowBias);
					overlay->DrawText({px, 70.f}, " VSM atlas : %u px", sh->GetAtlasSize());
					overlay->DrawText({px, 90.f}, "F7       quality  : %d", (int)cfg.quality);
					overlay->DrawText({px, 110.f}, "F8/F9    softness : %.3f", cfg.softness);
					overlay->DrawText({px, 130.f}, " slots: %u (rend %u | cache %u)", sh->GetActiveSlotCount(),
									  sh->GetRenderedSlotsCount(), sh->GetCachedSlotsCount());
				} else {
					overlay->DrawText({px, 50.f}, "(no shadow system)");
				}
				overlay->DrawText({px, 160.f}, "framesInFlight : %u", (uint32)ctx.renderer->GetConfig().framesInFlight);

				overlay->EndOverlay();
			}

			ctx.renderer->Present();
			ctx.renderer->EndFrame();
		}

		void Demo3D_Shutdown(DemoCtx &ctx) {
			auto *st = (Demo3DState *)ctx.userData;
			if (st && st->maskedMat)
				NkMaterial::Destroy(st->maskedMat);
			delete st;
			ctx.userData = nullptr;
			logger.Info("[Demo3D] Shutdown\n");
		}

	} // namespace demo
} // namespace nkentseu
