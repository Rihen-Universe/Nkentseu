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
		void Viewport3DZoom(float32 steps);
		void Viewport3DFrameAll(); ///< recadre sur la scene entiere

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
