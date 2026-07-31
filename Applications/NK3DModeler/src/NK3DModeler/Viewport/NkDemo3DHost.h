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

		// Vrai des que la demo rend dans sa cible (l'interface peut poser la
		// texture 4096).
		bool Demo3DHostReady();

		// Dernier echec d'initialisation, ou nullptr.
		const char *Demo3DHostError();

		void Demo3DHostShutdown();

	} // namespace demo
} // namespace nkentseu
