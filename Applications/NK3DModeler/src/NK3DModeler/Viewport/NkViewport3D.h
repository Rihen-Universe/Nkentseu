#pragma once
// -----------------------------------------------------------------------------
// @File    NkViewport3D.h
// @Brief   Facade OPAQUE de la vue 3D. Aucun type NKRenderer n'apparait ici.
// @License Proprietary - Free to use and modify
//
// POURQUOI UNE FACADE, ET POURQUOI SANS AUCUN TYPE DU MOTEUR.
//
// NKRenderer et NKCanvas definissent tous deux `nkentseu::renderer::NkBlendMode`
// et `nkentseu::renderer::NkVertex2D`. Les deux en-tetes ne peuvent donc pas se
// rencontrer dans la meme unite de compilation. NkAnimaEditor a rencontre le
// probleme avant nous et l'a resolu de la seule maniere qui tienne : TOUT le
// code NKRenderer vit dans UNE unite, et le reste de l'application ne voit que
// des fonctions libres a parametres neutres.
//
// D'ou les `void *` : `cmd` est un NkICommandBuffer, `device` un NkIDevice,
// `guiBackend` un NkGuiRHIBackend. Les nommer demanderait leurs en-tetes, ce
// qui reintroduirait exactement la collision qu'on evite. Le prix est une perte
// de typage sur trois appels de cablage, tous concentres dans main.cpp.
// -----------------------------------------------------------------------------

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace nk3d {

		// Identifiant de texture sous lequel la vue 3D se publie aupres de la
		// draw-list. Choisi loin de la police (0) et des icones (16..) pour ne
		// pouvoir entrer en collision avec aucun des deux.
		constexpr uint32 kViewportTexId = 4096u;

		// Types d'objets de scene. EN AJOUT SEUL : ils finiront dans les fichiers
		// de projet, et renumeroter rendrait faux tous les fichiers existants.
		constexpr int32 kVpObjMesh = 0;
		constexpr int32 kVpObjLightPoint = 1;
		constexpr int32 kVpObjLightSun = 2;
		constexpr int32 kVpObjLightSpot = 3;
		constexpr int32 kVpObjCamera = 4;
		constexpr int32 kVpObjEmpty = 5;
		// IMAGE DE REFERENCE : un plan qu'on aligne sur une vue pour modeler
		// par-dessus. Ce n'est pas un maillage ordinaire -- il ne doit ni recevoir
		// d'ombre ni entrer dans le rendu final -- mais il a une transformation,
		// donc il vit dans la meme table.
		constexpr int32 kVpObjReference = 6;

		// Primitives de maillage, dans l'ordre du menu « Ajouter > Maillage ».
		constexpr int32 kVpPrimCube = 0;
		constexpr int32 kVpPrimPlane = 1;
		constexpr int32 kVpPrimSphere = 2;
		constexpr int32 kVpPrimCylinder = 3;
		constexpr int32 kVpPrimCone = 4;
		constexpr int32 kVpPrimTorus = 5;

		// Cablage, appele une fois depuis main.cpp.
		void Viewport3DSetSharedDevice(void *device);

		// Rend la scene dans sa cible hors ecran, sur le command buffer de
		// l'editeur. Appele depuis le crochet PreUI : la frame device est ouverte
		// mais la passe backbuffer n'a pas commence -- on ne peut pas imbriquer
		// une passe de rendu dans une autre.
		void Viewport3DRenderOffscreen(void *cmd);

		// Publie la cible hors ecran aupres du backend NKGui, pour AddImage.
		void Viewport3DRegisterInto(void *guiBackend);

		// Taille utile de la vue, en pixels. La cible n'est reallouee que si la
		// taille change vraiment : recreer une cible par image ferait tomber le
		// nombre d'images par seconde a rien.
		void Viewport3DResize(uint32 w, uint32 h);

		// ── Camera ──────────────────────────────────────────────────────────
		void Viewport3DOrbit(float32 dYaw, float32 dPitch);
		void Viewport3DPan(float32 dx, float32 dy);
		void Viewport3DPanSteps(float32 dx, float32 dy); ///< pan a la molette (crans)
		// Navigation par GLISSEMENT depuis les boutons de la vue : 0 zoom, 1 pan.
		void Viewport3DNavDrag(int32 kind, float32 dx, float32 dy);
		// Orbite libre depuis le gizmo de navigation (autour de la CIBLE).
		void Viewport3DOrbitFree(float32 dYaw, float32 dPitch);
		void Viewport3DZoom(float32 steps);
		void Viewport3DFrameAll(); ///< recadre sur la scene entiere
		// Vues axiales du pave numerique : 0 face, 1 droite, 2 dessus.
		// `opposite` (Ctrl) donne arriere / gauche / dessous.
		void Viewport3DAxisView(int32 which, bool opposite);
		void Viewport3DSetOrtho(bool on);
		bool Viewport3DIsOrtho();

		// ── Gizmo de transformation ─────────────────────────────────────────
		// mode : 0 deplacer, 1 tourner, 2 redimensionner, 3 combine.
		// orient : 0 global, 1 local, 2 normal. pivot : cf. NkGizmo3D.
		void Viewport3DSetGizmoMode(int32 mode);
		void Viewport3DSetGizmoOrientation(int32 orient);
		void Viewport3DSetGizmoPivot(int32 pivot);
		void Viewport3DSetSnap(bool on, float32 translate, float32 rotateDeg, float32 scale);
		void Viewport3DSetGizmoInput(float32 mouseX, float32 mouseY, float32 dx, float32 dy,
									 bool leftPressed, bool leftDown, bool shift, bool ctrl);
		bool Viewport3DGizmoDragging();

		// ── Objets de scene ─────────────────────────────────────────────────
		// La scene NAIT VIDE ; « Ajouter » cree maillages, lumieres, cameras et
		// reperes. Les indices de slot sont STABLES (un tableau a trous, pas une
		// liste compactee) : la hierarchie peut les garder d'une image a l'autre.
		int32 Viewport3DAddObject(int32 type, int32 prim); ///< rend le slot, -1 si plein
		int32 Viewport3DObjectCount();					   ///< taille de la table, trous compris
		bool Viewport3DObjectAlive(int32 i);
		const char *Viewport3DObjectName(int32 i);
		void Viewport3DRenameObject(int32 i, const char *name);
		int32 Viewport3DObjectType(int32 i);
		bool Viewport3DObjectSelected(int32 i);
		bool Viewport3DObjectVisible(int32 i);
		void Viewport3DSetObjectVisible(int32 i, bool on);
		bool Viewport3DObjectLocked(int32 i);
		void Viewport3DSetObjectLocked(int32 i, bool on);
		void Viewport3DSelectObject(int32 i, bool add);
		void Viewport3DDeselectAllObjects();
		int32 Viewport3DActiveObject();
		void Viewport3DDeleteObject(int32 i);
		// Transformations, en clair : position, rotation en degres, echelle.
		// pos/rot/scl sont LA SOURCE -- le panneau Proprietes edite ces valeurs et
		// le gizmo ne les ecrase qu'en fin de glissement.
		bool Viewport3DGetObjectTransform(int32 i, float32 *pos3, float32 *rot3, float32 *scl3);
		void Viewport3DSetObjectTransform(int32 i, const float32 *pos3, const float32 *rot3,
										  const float32 *scl3);

		// Fond de la vue 3D.
		void Viewport3DSetBackground(float32 r, float32 g, float32 b);

		// ── Mode edition et selection ───────────────────────────────────────
		void Viewport3DSetEditMode(bool on);
		bool Viewport3DEditMode();
		// Sous-modes COMBINABLES : bit 1 sommet, 2 arete, 4 face.
		void Viewport3DSetSelectMask(uint32 mask);
		uint32 Viewport3DSelectMask();
		void Viewport3DSetXray(bool on);
		void Viewport3DSelectAll(bool all);
		uint32 Viewport3DSelectedCount();
		// Clic dans la vue. Coordonnees RELATIVES au rectangle de la vue : la cible
		// hors ecran a sa propre origine, des coordonnees fenetre decaleraient tout
		// le picking de la largeur du panneau de gauche.
		bool Viewport3DPick(float32 mx, float32 my, bool add, bool toggle);

		// ── Curseur 3D ──────────────────────────────────────────────────────
		// Point de reference de Blender : pivot, origine des nouveaux objets, centre
		// de la revolution, cible de « souder au curseur ».
		void Viewport3DSetCursor3D(float32 x, float32 y, float32 z);
		void Viewport3DGetCursor3D(float32 *out3);
		void Viewport3DPlaceCursor(float32 mx, float32 my); ///< sous la souris

		// ── Selection par zone ──────────────────────────────────────────────
		// Coordonnees RELATIVES a la vue. mode : 0 remplacer, 1 ajouter, 2 retirer.
		void Viewport3DSelectRect(float32 x0, float32 y0, float32 x1, float32 y1, int32 mode);
		void Viewport3DSelectCircle(float32 cx, float32 cy, float32 radius, int32 mode);
		void Viewport3DSelectLasso(const float32 *pts, uint32 count, int32 mode);
		// Alt+clic : boucle d'aretes sur une arete, anneau de faces sur une face.
		bool Viewport3DSelectLoopAt(float32 mx, float32 my, bool add);

		// ── TRANSFORMATIONS MODALES (G / R / S facon Blender) ───────────────
		// Le geste de Blender : une TOUCHE arme la transformation, la souris la
		// pilote en direct, X / Y / Z la contraignent a un axe, le clic gauche
		// confirme et Echap annule. C'est autre chose que le gizmo -- il n'y a
		// aucune poignee a viser, donc rien a rater, et c'est pour cela que les
		// modeleurs travaillent presque tous au clavier.
		//
		// Rien n'est ecrit tant qu'on n'a pas confirme : annuler restaure
		// exactement l'etat de depart.
		enum : int32 { kVpXformNone = 0, kVpXformMove, kVpXformRotate, kVpXformScale };
		void Viewport3DBeginModal(int32 kind, float32 mouseX, float32 mouseY);
		void Viewport3DModalAxis(int32 axis); ///< 0 X, 1 Y, 2 Z, -1 libre (bascule)
		void Viewport3DModalUpdate(float32 mouseX, float32 mouseY);
		void Viewport3DModalConfirm();
		void Viewport3DModalCancel();
		int32 Viewport3DModalKind();  ///< kVpXformNone si aucune en cours
		int32 Viewport3DModalAxisNow();
		// Libelle pour la barre d'etat (« Deplacement X : 1.240 »).
		const char *Viewport3DModalLabel();

		// Filaire : n'afficher que les VRAIES aretes (les quads gardent leur forme,
		// la diagonale de triangulation ne se voit pas).
		void Viewport3DSetNgonWireframe(bool on);
		// Le gizmo peut etre masque -- l'outil Selection ne transforme rien.
		void Viewport3DSetGizmoVisible(bool on);

		// ── Pile de modificateurs ───────────────────────────────────────────
		// Elle vit sur l'objet ACTIF et n'est PAS destructive : la cage editee
		// reste la base, la pile produit la geometrie affichee. « Appliquer » est
		// le seul geste destructif.
		//
		// L'interface est GENERIQUE : elle interroge la liste des parametres que
		// chaque modificateur publie (nom, type, bornes) au lieu de recopier a la
		// main dix-sept jeux de reglages qui divergeraient au premier ajout.
		uint32 Viewport3DModifierTypeCount();
		const char *Viewport3DModifierTypeName(int32 type);
		int32 Viewport3DAddModifier(int32 type); ///< rend l'indice, -1 si impossible
		uint32 Viewport3DModifierCount();
		int32 Viewport3DModifierTypeAt(uint32 index);
		bool Viewport3DModifierEnabled(uint32 index);
		void Viewport3DSetModifierEnabled(uint32 index, bool on);
		bool Viewport3DRemoveModifier(uint32 index);
		bool Viewport3DMoveModifier(uint32 index, bool up);
		bool Viewport3DApplyModifier(uint32 index); ///< DESTRUCTIF : cuit dans le maillage
		// Parametres du modificateur `index`. `type` : 0 booleen, 1 entier,
		// 2 flottant, 3 vecteur3, 4 enumeration.
		uint32 Viewport3DModifierParamCount(uint32 index);
		bool Viewport3DModifierParamInfo(uint32 index, uint32 p, const char **label, int32 *type,
										 float32 *minV, float32 *maxV);
		float32 Viewport3DGetModifierParam(uint32 index, uint32 p);
		void Viewport3DSetModifierParam(uint32 index, uint32 p, float32 v);

		// Repere de la camera, pour orienter le gizmo de navigation de l'interface.
		void Viewport3DCameraAxes(float32 *right3, float32 *up3, float32 *fwd3);

		// ── Operations d'edition ────────────────────────────────────────────
		// Toutes passent par une commande : annulation et journal acquis d'office.
		bool Viewport3DExtrude(bool individual);
		bool Viewport3DDeleteSelection();
		bool Viewport3DMerge(int32 mode);
		bool Viewport3DMakeFace();
		bool Viewport3DSubdivide(int32 cuts);
		bool Viewport3DLoopCut(int32 cuts);
		bool Viewport3DBevel(float32 offset, int32 segments, bool vertexOnly);
		bool Viewport3DInset(float32 thickness, float32 depth);
		bool Viewport3DDissolve();
		bool Viewport3DMoveSelection(float32 dx, float32 dy, float32 dz);

		bool Viewport3DUndo();
		bool Viewport3DRedo();
		bool Viewport3DCanUndo();
		bool Viewport3DCanRedo();
		uint32 Viewport3DEditCount();

		// ── Reglages d'affichage, pilotes par la barre de la vue ────────────
		// `shading` : 0 solide, 1 materiau, 2 rendu, 3 filaire.
		// `solidLight` : 0 studio, 1 matcap, 2 plat (n'a de sens que si solide).
		void Viewport3DSetShading(int32 shading, int32 solidLight);
		void Viewport3DSetOverlays(uint32 mask); ///< meme masque que NkModelerState

		// ── Etat, pour le pied de page et le panneau Details ────────────────
		bool Viewport3DReady();
		void Viewport3DStats(uint32 &verts, uint32 &edges, uint32 &faces, uint32 &tris);
		const char *Viewport3DError(); ///< nullptr si tout va bien

		void Viewport3DShutdown();

	} // namespace nk3d
} // namespace nkentseu
