#pragma once
// -----------------------------------------------------------------------------
// @File    NKScenaApp.h
// @Brief   L'APPLICATION NKScena : une NkApplication (Noge) qui pousse le moteur
//          (NkEngineLayer : transforms et rendu ; la physique ne tourne JAMAIS --
//          le temps est celui de la sequence), le rend dans la vue 3D hors ecran
//          empruntee a Nogee (NogeeVue3D), et peint autour l'interface de la
//          famille (NkScenaInterface). Elle mene aussi le RENDU de la sequence,
//          image par image, dans cette vue.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// LA TRAME (NkApplication::Run) :
//   OnUpdate des couches, DANS L'ORDRE :
//     NkScenaCoucheTemps  le TEMPS : la sequence pose la scene a l'instant de la
//                         frise (ou d'une image du rendu) -- AVANT le moteur, pour
//                         que ses transforms soient vus a cette image-ci ;
//     NkScenaCoucheMoteur NkEngineLayer : les matrices monde (NkTransformSystem) ;
//     NkScenaInterface    (surcouche) son pas de temps.
//   OnRender : NkScenaCoucheTemps prepare la vue -> le moteur soumet la scene au
//              renderer de la VUE -> l'application execute le graphe de la vue.
//   OnUIRender : l'interface construit sa liste ; Present la peint a l'ecran.
//
// LE RENDU « Rendre » : a l'image N la sequence est posee a t(i) et rendue dans
// la vue (a la taille de la SORTIE, par la camera du plan) ; a l'image N+1,
// avant tout, le GPU est attendu, la vue RELUE (NogeeVue3D::Relire), l'image
// ecrite (NkImageSequenceWriter, PNG numerotes), la progression notee au
// Journal, et t(i+1) pose. Rien d'autre ne change la sequence pendant ce temps.
// -----------------------------------------------------------------------------

#include "Noge/Core/NkApplication.h"
#include "Noge/Layers/NkEngineLayer.h"
#include "NKContainers/String/NkString.h"
#include "NKMedia/Video/NkImageSequenceWriter.h"
#include "NKScena/NKScenaInterface.h"
#include "NKScena/NKScenaModele.h"
#include "Nogee/Editeur/NogeeVue3D.h"

namespace nkentseu {
	namespace nkscena {

		struct NkScenaOptions {
				NkString capture;		 ///< non vide : capture de la fenetre (hors ecran), puis sortie
				float32 tempsCapture = 2.5f;
				NkString scene;			 ///< --scene=  : ouvrir cette scene
				NkString sequence;		 ///< --sequence= : ouvrir cette sequence (et SA scene)
				bool exemple = false;	 ///< --exemple : la sequence de demonstration
				float32 curseur = -1.f;	 ///< --temps=S : le curseur de la frise
				bool vueCamera = false;	 ///< --vue-camera : regarder par la camera du plan
				bool jouer = false;		 ///< --jouer : lecture au depart
				int32 tiroir = -1;		 ///< --tiroir=frise|contenu|journal|terminal
				bool clair = false;		 ///< --theme=clair
				NkString selection;		 ///< --selection=NOM : l'entite choisie
				NkString choix;			 ///< --details=piste|cle|plan|sequence
				int32 outil = -1;		 ///< --outil=deplacer|tourner|echelle
				bool rendre = false;	 ///< --rendre : lancer « Rendre » au depart, puis sortir
				NkString sortie;		 ///< --sortie=DOSSIER : le dossier du rendu
				int32 captureImage = 0;	 ///< --capture-image=N : capturer PENDANT le rendu, a l'image N
				bool horsEcran = false;	 ///< la fenetre est hors de tout ecran (capture, rendu scripte)
		};

		/// Le moteur, SANS physique : NKScena pose le temps, elle ne le simule pas.
		class NkScenaCoucheMoteur final : public NkEngineLayer {
			public:
				void OnFixedUpdate(float) override {
				}
		};

		class NkScenaApp final : public NkApplication {
			public:
				NkScenaApp(const NkApplicationConfig &config, const NkScenaOptions &options);
				~NkScenaApp() override;
				int32 CodeSortie() const noexcept {
					return mCodeSortie;
				}
				/// AVANT le moteur (NkScenaCoucheTemps::OnUpdate) : le temps de la scene.
				void Temps(float32 dt);
				/// AVANT les soumissions du moteur (NkScenaCoucheTemps::OnRender).
				void PreparerVue();

			protected:
				void OnInit() override;
				void OnUpdate(float dt) override;
				void OnRender() override;
				void OnShutdown() override;
				void OnClose() override;

			private:
				void AppliquerOptions();
				void SoumettreSelection();
				void PiloterCapture();
				void DemarrerRendu();
				void PiloterRendu();
				void FinirRendu(bool complet);

				NkScenaOptions mOptions;
				NkScenaModele mModele;
				nogee::NogeeVue3D mVue;
				NkScenaCoucheMoteur *mMoteur = nullptr; ///< possedee par la pile de couches
				NkScenaInterface *mUi = nullptr;		///< possedee par la pile de couches
				float32 mTemps = 0.f;
				int32 mCodeSortie = 0;
				bool mOptionsAppliquees = false;
				// --- le rendu de la sequence -------------------------------------
				NkScenaRenduEnCours mRendu;
				bool mDemandeRendu = false;
				media::NkImageSequenceWriter mEcrivain;
				uint8 *mPixels = nullptr;
				uint32 mRenduW = 0, mRenduH = 0;
				int32 mAttente = 0;
				float32 mRenduDebut = 0.f;
				float32 mCurseurAvant = 0.f;
				bool mVueCameraAvant = false;
				bool mRenduFini = false;
				// --- la capture --------------------------------------------------
				void *mCapture = nullptr; ///< renderer::NkOffscreenTarget
				int32 mPhase = 0;
				int32 mImages = 0;
		};

	} // namespace nkscena
} // namespace nkentseu
