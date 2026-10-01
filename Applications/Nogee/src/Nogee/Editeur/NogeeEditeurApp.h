#pragma once
// -----------------------------------------------------------------------------
// @File    NogeeEditeurApp.h
// @Brief   L'APPLICATION de l'editeur Nogee : une NkApplication (Noge) qui pousse
//          le moteur (NkEngineLayer -- ECS, physique, pont de rendu), le rend dans
//          une vue 3D hors ecran (NogeeVue3D) et peint autour l'interface de la
//          famille (NogeeInterface, l'apparence d'UnkenyEditor).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// LA TRAME (NkApplication::Run) :
//   OnUpdate  : le moteur avance (transforms ; la physique SEULEMENT en jeu)
//   OnRender  : NogeeCouchePrepa (la vue se prepare) -> NkEngineLayer (soumet la
//               scene au renderer de la VUE) -> OnRender de l'application (le
//               lisere de la selection, puis le graphe de la vue execute dans le
//               command buffer de l'image)
//   OnUIRender: l'interface construit sa liste de dessin
//   Present   : le renderer de l'APPLICATION execute SON graphe : la passe
//               Overlay2D peint l'interface (et l'image de la vue) a l'ecran.
//
// `--capture=IMAGE.png` : la meme application, fenetre HORS ECRAN et sans
// focus ; la sortie finale du renderer de l'application est redirigee vers une
// cible hors ecran le temps de quelques images, relue, ecrite, et l'application
// sort. Aucune souris, aucun clavier.
// -----------------------------------------------------------------------------

#include "Noge/Core/NkApplication.h"
#include "Noge/Layers/NkEngineLayer.h"
#include "NKContainers/String/NkString.h"
#include "Nogee/Editeur/NogeeModele.h"
#include "Nogee/Editeur/NogeeVue3D.h"

namespace nkentseu {
	namespace nogee {

		class NogeeInterface;

		struct NogeeOptions {
				NkString capture;		///< non vide : mode capture (voir l'en-tete)
				float32 tempsCapture = 2.5f;
				NkString scene;			///< --scene= : ouvrir ce fichier au lieu de la scene neuve
				NkString contenu = "Resources"; ///< --contenu= : le dossier du Content Browser
				NkString selection;		///< --selection=NOM : l'entite choisie au depart
				int32 outil = -1;		///< --outil=deplacer|tourner|echelle
				int32 tiroir = -1;		///< --tiroir=contenu|journal|terminal
				bool clair = false;		///< --theme=clair
				bool jouer = false;		///< --jouer : lancer le jeu au depart (la capture le montre en cours)
				bool selecteur = false; ///< --selecteur : Fichier > Ouvrir… au depart (le selecteur du kit, captures)
		};

		/// Le moteur, avec la physique tenue par l'etat de jeu de l'editeur.
		class NogeeCoucheMoteur final : public NkEngineLayer {
			public:
				NogeeModele *modele = nullptr;
				void OnFixedUpdate(float fixedDt) override;
		};

		class NogeeEditeurApp final : public NkApplication {
			public:
				NogeeEditeurApp(const NkApplicationConfig &config, const NogeeOptions &options);
				~NogeeEditeurApp() override;
				int32 CodeSortie() const noexcept {
					return mCodeSortie;
				}
				/// AVANT les soumissions du moteur (NogeeCouchePrepa).
				void PreparerVue();

			protected:
				void OnInit() override;
				void OnUpdate(float dt) override;
				void OnRender() override;
				void OnShutdown() override;
				void OnClose() override;

			private:
				void SoumettreSelection();
				void PiloterCapture();
				void AppliquerOptions();

				NogeeOptions mOptions;
				NogeeModele mModele;
				NogeeVue3D mVue;
				NogeeCoucheMoteur *mMoteur = nullptr; ///< possedee par la pile de couches
				NogeeInterface *mUi = nullptr;		  ///< possedee par la pile de couches
				float32 mTemps = 0.f;
				int32 mCodeSortie = 0;
				bool mOptionsAppliquees = false;
				// --- la capture ---------------------------------------------------
				void *mCapture = nullptr; ///< renderer::NkOffscreenTarget
				int32 mPhase = 0;
				int32 mImages = 0;
		};

	} // namespace nogee
} // namespace nkentseu
