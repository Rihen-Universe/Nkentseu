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
