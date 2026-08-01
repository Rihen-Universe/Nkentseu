#pragma once
// -----------------------------------------------------------------------------
// @File    NkDemo3DHost.h
// @Brief   Facade OPAQUE de la vue 3D portee de renderdemo --demo=2.
// @License Proprietary - Free to use and modify
//
// MEME REGLE QUE NkViewport3D.h : aucun type NKRenderer ici, car NKRenderer et
// NKCanvas declarent tous deux `renderer::NkBlendMode`/`NkVertex2D` et ne
// peuvent pas se rencontrer dans une unite de compilation. `device` est un
// NkIDevice, `cmd` un NkICommandBuffer, `guiBackend` un NkGuiRHIBackend.
//
// CE QUE C'EST : le pont vers NkDemo3D.cpp, portage INTEGRAL de Demo3D
// (--demo=2 de renderdemo), rendu dans une cible hors ecran publiee sous la
// texture id 4096 — le meme id qu'avant, l'interface n'a pas change.
// La demo garde SES raccourcis et SES gestes (TAB, G/R/S, Z, clic-milieu...) ;
// le cablage des boutons de l'interface vers ses etats viendra ensuite.
// -----------------------------------------------------------------------------

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace demo {

		// Le device de l'editeur (NkEditorRHIRenderer::GetDevice). A poser une
		// fois avant la premiere frame ; l'initialisation reelle est paresseuse.
		void Demo3DHostSetDevice(void *device);

		// Taille de la VUE (le panneau, pas la fenetre). Ne refait la cible que
		// si la taille change vraiment.
		void Demo3DHostResize(uint32 w, uint32 h);

		// Origine de la vue dans la fenetre (traduction souris), survol, et
		// autorisation d'entree (faux pendant une saisie de texte).
		void Demo3DHostSetView(float32 offX, float32 offY, bool hover, bool inputOn);

		// Rend la frame de la demo dans la cible hors ecran, sur le command
		// buffer de l'editeur (crochet preUI). Calcule son dt lui-meme.
		void Demo3DHostFrame(void *cmd);

		// Publie la cible aupres du backend NKGui sous l'id 4096.
		void Demo3DHostRegisterInto(void *guiBackend);

		// ── CABLAGE DES BOUTONS DE LA VUE ───────────────────────────────────
		// Chaque fonction refait EXACTEMENT ce que fait le raccourci
		// correspondant de la demo (Z, M, pave numerique, G/R/S, virgule,
		// Shift+TAB, F1..F4) — memes ecritures, aucun etat parallele.
		void Demo3DHostSetShading(int32 mode); // 0 rendu, 1 solide, 2 filaire, 3 normales, 4 uv, 5 ao
		int32 Demo3DHostShading();
		void Demo3DHostSetUnlitColor(int32 mode); // 0 materiau, 1 gris, 2 personnalisee
		int32 Demo3DHostUnlitColor();
		void Demo3DHostSetMatcap(int32 id);
		int32 Demo3DHostMatcap();
		int32 Demo3DHostMatcapCount();
		const char *Demo3DHostMatcapName(int32 id);
		void Demo3DHostMatcapBall(int32 id, uint8 *rgba, uint32 size);
		void Demo3DHostSetOrtho(bool on);
		bool Demo3DHostIsOrtho();
		void Demo3DHostAxisView(int32 which, bool opposite); // 0 avant/arriere, 1 droite/gauche, 2 dessus/dessous
		void Demo3DHostResetView();
		void Demo3DHostStoreCamera();
		bool Demo3DHostRecallCamera();
		void Demo3DHostOrbit(float32 dx, float32 dy);
		void Demo3DHostPan(float32 dx, float32 dy);
		void Demo3DHostZoomWheel(float32 notches);
		void Demo3DHostToggleFlyCam();
		bool Demo3DHostIsFlyCam();
		void Demo3DHostSetCamSpeed(float32 mult);
		void Demo3DHostCameraAxes(float32 *rgt, float32 *upv, float32 *fwd);
		void Demo3DHostSetGizmoOp(int32 op); // 0 deplacer, 1 tourner, 2 echelle, 3 combine
		int32 Demo3DHostGizmoOp();
		void Demo3DHostSetOrientation(int32 o); // 0 monde, 1 local, 2 normale
		int32 Demo3DHostOrientation();
		void Demo3DHostSetSnap(bool on, float32 t, float32 rotDeg, float32 scl);
		bool Demo3DHostSnapEnabled();
		void Demo3DHostSetGizmoHidden(bool hidden);
		bool Demo3DHostInEditMode();
		void Demo3DHostSetEditSelMask(int32 mask); // bits 1 sommet, 2 arete, 4 face
		int32 Demo3DHostEditSelMask();
		void Demo3DHostSetZoneTool(int32 shape); // -1 off, 0 rectangle, 1 cercle, 2 lasso
		void Demo3DHostSetCursorTool(bool on);
		void Demo3DHostSetGridFlags(bool grid, bool minor, bool major, bool axes);
		void Demo3DHostGridFlags(bool *grid, bool *minor, bool *major, bool *axes);
		void Demo3DHostSetOutline(bool on);
		bool Demo3DHostOutline();
		void Demo3DHostSetHud(bool on);
		bool Demo3DHostHud();
		void Demo3DHostSetBackground(float32 r, float32 g, float32 b);

		// ── Objets de la scene (hierarchie, panneau Objet) ──────────────────
		int32 Demo3DHostObjectCount();
		void Demo3DHostObjectName(int32 i, char *out, uint32 cap);
		bool Demo3DHostObjectSelected(int32 i);
		int32 Demo3DHostActiveObject();
		void Demo3DHostSelectObject(int32 i, bool additive);
		void Demo3DHostSelectGroup(int32 start, int32 count, bool additive);
		void Demo3DHostSelectAllLights();
		bool Demo3DHostAllLightsSelected();
		void Demo3DHostDeselectAll();
		void Demo3DHostObjectPosition(int32 i, float32 *out3);
		int32 Demo3DHostLightCount();
		void Demo3DHostLightName(int32 li, char *out, uint32 cap);
		int32 Demo3DHostSelectedLight();
		void Demo3DHostSelectLight(int32 li);
		void Demo3DHostLightPosition(int32 li, float32 *out3);
		void Demo3DHostSetLightPosition(int32 li, const float32 *xyz);
		int32 Demo3DHostLightType(int32 li); // 0 dir, 1 point, 2 spot, 3 area
		void Demo3DHostLightDir(int32 li, float32 *out3);
		void Demo3DHostSetLightDir(int32 li, const float32 *xyz);
		void Demo3DHostLightParams(int32 li, float32 *color3, float32 *intensity);
		void Demo3DHostSetLightParams(int32 li, const float32 *color3, float32 intensity);
		void Demo3DHostSetObjectHidden(int32 i, bool hidden);
		bool Demo3DHostObjectHidden(int32 i);
		void Demo3DHostSetObjectLocked(int32 i, bool locked);
		bool Demo3DHostObjectLocked(int32 i);
		void Demo3DHostSetLightHidden(int32 li, bool hidden);
		bool Demo3DHostLightHidden(int32 li);
		void Demo3DHostSetAllHidden(bool hidden); // scene VIERGE d'un nouvel onglet
		bool Demo3DHostObjectTransform(int32 i, float32 *pos3, float32 *rotDeg3, float32 *scl3);
		void Demo3DHostApplyDeltaToSelection(const float32 *dPos, const float32 *dRotDeg,
											 const float32 *sclRatio, int32 except);
		void Demo3DHostSetObjectTransform(int32 i, const float32 *pos3, const float32 *rotDeg3,
										  const float32 *scl3);

		// ── PARENTE DE SCENE ────────────────────────────────────────────────
		// TOUT NOEUD peut etre parent ou enfant : 0..85 objets, 86..89
		// lumieres, 90..95 EMPTIES (les anciens groupes, devenus de vrais
		// objets vides). La transformation d'un parent est repercutee a son
		// sous-arbre par l'hote (semantique orbite) ; selectionner un parent
		// ne selectionne PAS ses enfants.
		int32 Demo3DHostNodeCount();		 // 96 (plafond, empties compris)
		int32 Demo3DHostNodeParent(int32 node); // -1 = racine
		bool Demo3DHostSetNodeParent(int32 child, int32 parent); // refuse les cycles
		bool Demo3DHostNodeHasChildren(int32 node);
		// Masque de TRANSMISSION du parent : bit 1 position, bit 2 rotation,
		// bit 4 echelle -- une composante eteinte ne se propage plus.
		int32 Demo3DHostNodeXmitMask(int32 node);
		void Demo3DHostSetNodeXmitMask(int32 node, int32 mask);
		// Selection d'un EMPTY (gizmo dedie dans la vue ; -1 = aucun).
		void Demo3DHostSelectEmptyNode(int32 node);
		int32 Demo3DHostSelectedEmptyNode();
		void Demo3DHostToggleEmptyNode(int32 node); // Maj/Ctrl+clic : multi successif
		bool Demo3DHostEmptyNodeSelected(int32 node);
		// Transform EFFECTIVE d'un empty (node >= 90), drag du gizmo compris.
		bool Demo3DHostEmptyTransform(int32 node, float32 *pos3, float32 *rotDeg3, float32 *scl3);
		void Demo3DHostSetEmptyTransform(int32 node, const float32 *pos3, const float32 *rotDeg3,
										 const float32 *scl3);

		// ── Materiau par objet (panneau Modele) ─────────────────────────────
		// Lecture = valeurs EFFECTIVES vues a la derniere soumission ;
		// ecriture = surcharge persistante (teinte / metallique+rugosite).
		bool Demo3DHostMeshMaterial(int32 i, float32 *tint3, float32 *metallic, float32 *roughness);
		void Demo3DHostSetMeshTint(int32 i, const float32 *rgb3);
		void Demo3DHostSetMeshMetalRough(int32 i, float32 metallic, float32 roughness);
		void Demo3DHostResetMeshMat(int32 i);

		// ── Suppression / duplication / presse-papiers ──────────────────────
		// Les noeuds 96..159 sont les OBJETS UTILISATEUR (crees ici).
		bool Demo3DHostNodeDeleted(int32 node);
		void Demo3DHostDeleteNode(int32 node, bool withChildren);
		int32 Demo3DHostDuplicateNode(int32 node); // -1 si impossible (lumiere v1)
		void Demo3DHostCopyNode(int32 node);
		int32 Demo3DHostPasteNode();
		int32 Demo3DHostUserKind(int32 node); // 0 aucun, 1 sphere, 2 cube, 3 plan, 4 empty
		// Raccourcis par POLLING : bits 1 dupliquer, 2 copier, 4 coller,
		// 8 supprimer, 16 parenter, 32 deparenter. Consommes a la lecture.
		int32 Demo3DHostTakeShortcuts();
		// Menu AJOUTER : cree un noeud utilisateur (kind 1..9, sub = variante).
		int32 Demo3DHostAddNode(int32 kind, int32 sub);
		// Parametres du mesh cree (segments / anneaux-subdivisions) : le
		// panneau « Ajuster la creation » les edite AVANT validation.
		int32 Demo3DHostUserSub(int32 node); // variante demandee au menu Ajouter
		bool Demo3DHostMeshParams(int32 node, int32 *segs, int32 *rings, float32 *aux);
		void Demo3DHostSetMeshParams(int32 node, int32 segs, int32 rings, float32 aux);
		// Lumiere UTILISATEUR (kind 5) : couleur + intensite.
		bool Demo3DHostUserLightParams(int32 node, float32 *color3, float32 *intensity);
		void Demo3DHostSetUserLightParams(int32 node, const float32 *color3, float32 intensity);
		void Demo3DHostNodeBaseSize(int32 node, float32 *out3); // taille locale par nature
		void Demo3DHostSetNodeBaseSize(int32 node, const float32 *in3); // decouplee de l'echelle

		// ── Reglages de vue (panneau Scene / Outil) ─────────────────────────
		void Demo3DHostSetViewFar(float32 f); // 0 = auto
		float32 Demo3DHostViewFar();
		void Demo3DHostSetOrthoScale(float32 f);
		float32 Demo3DHostOrthoScale();
		void Demo3DHostSetGridExtent(int32 n);
		int32 Demo3DHostGridExtent();
		void Demo3DHostResetCursor();
		void Demo3DHostCursorToSelection();
		void Demo3DHostSelectionToCursor();
		void Demo3DHostClearXform(int32 which); // 0 translation, 1 rotation, 2 echelle
		// Pose de camera : les ONGLETS DE SCENE s'en servent pour donner a
		// chaque scene sa propre vue (memorisee a la bascule d'onglet).
		void Demo3DHostGetCameraPose(float32 *t3, float32 *dist, float32 *yaw, float32 *pitch,
									 bool *ortho);
		void Demo3DHostSetCameraPose(const float32 *t3, float32 dist, float32 yaw, float32 pitch,
									 bool ortho);

		// Vrai des que la demo rend dans sa cible (l'interface peut poser la
		// texture 4096).
		bool Demo3DHostReady();

		// Dernier echec d'initialisation, ou nullptr.
		const char *Demo3DHostError();

		void Demo3DHostShutdown();

	} // namespace demo
} // namespace nkentseu
