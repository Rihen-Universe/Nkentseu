// =============================================================================
// Physic2D.h — l'application : un editeur de simulation physique facon UE5
//
// ELLE HERITE DE NkCanvasApp, comme le gabarit fourni : la coquille donne la
// fenetre, la boucle, le cycle de vie mobile ; l'application remplit
// OnInit / OnUpdate / OnRender / OnEvent.
//
// ET ELLE DESSINE AVEC NKGui : une NkGuiDrawList par trame, des NkGuiFont
// chargees a la taille exacte de l'interface, soumises par NkGuiCanvasBackend
// au moteur 2D de NKCanvas (OpenGL, Vulkan, DirectX, Metal ou logiciel).
//
// CE QUE L'ON VOIT (disposition d'UE5)
//   barre de menus    Fichier  Edition  Fenetre  Niveaux  Aide
//   barre d'outils    outils (Q W E R T Y G P) | Jouer Pause Pas Arreter | niveau
//   gauche            "Placer des acteurs" (1..9)
//   centre            la vue : grille, monde, gizmos, barre de vue
//   droite            Outliner + Details (du corps choisi, sinon du monde)
//   bas               Journal de sortie | Statistiques | Aide
//
// JOUER / ARRETER, comme "Play In Editor" : Jouer photographie le monde,
// Arreter le restaure. Ce qu'on casse en jouant ne casse pas le niveau.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#pragma once

#include "NKCanvas/App/NkCanvasApp.h"
#include "NKCanvas/UI/NkGuiCanvasBackend.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKGui/Core/NkGuiDrawList.h"
#include "NKGui/Core/NkGuiFont.h"

#include "Physic2D/Editeur/NkPhysRendu.h"
#include "Physic2D/Editeur/NkUE5Ui.h"
#include "Physic2D/Physique/NkPhysFabrique.h"
#include "Physic2D/Physique/NkPhysMonde.h"

namespace nkentseu {

	class Physic2D : public renderer::NkCanvasApp {
		public:
			Physic2D();

			/// Le banc VISUEL (hors depot) rend l'editeur dans une image sans
			/// fenetre ; il lit l'etat prive pour composer ses scenes.
			friend class Physic2DBancVisuel;

		protected:
			NkOptional<int> OnCommandLine(const NkVector<NkString> &args) override;
			bool OnInit() override;
			void OnUpdate(float32 deltaTime) override;
			void OnRender(renderer::NkRenderWindow &target) override;
			bool OnEvent(const NkEvent &event) override;
			bool OnPointer(const NkPointer &pointer) override;
			bool OnKeyPress(const NkKeyPressEvent &event) override;
			void OnLayout(const renderer::NkLayoutInfo &layout) override;
			void OnPause() override;

		private:
			// --- Types ------------------------------------------------------
			enum class Etat : uint8 { EDITION, JEU, PAUSE };
			enum class Outil : uint8 { SELECTION, SAISIR, COUPER, EXPLOSION, AIMANT, OBSTACLE, GOMME, EPINGLE, PLACER, COUNT };
			enum class Popup : uint8 {
				AUCUN,
				MENU_FICHIER,
				MENU_EDITION,
				MENU_FENETRE,
				MENU_NIVEAUX,
				MENU_AIDE,
				NIVEAU,
				VUE_MODE,
				VUE_AFFICHER
			};
			enum class Niveau : uint8 { INFO, SUCCES, AVERT, ERREUR };

			struct LigneJournal {
					char categorie[16] = {};
					char texte[160] = {};
					Niveau niveau = Niveau::INFO;
					float32 temps = 0.f;
			};

			struct ElementMenu {
					const char *libelle = "";
					const char *raccourci = "";
					int32 action = 0;
					bool coche = false;
					bool separateur = false;
					bool actif = true;
			};

			struct Disposition {
					physic2d::NkRect menu, outils, gauche, droite, outliner, details, bas, vue;
			};

			// --- Cycle ------------------------------------------------------
			void ChargerPolices();
			void Disposer(float32 w, float32 h);
			void Journal(Niveau n, const char *categorie, const char *fmt, ...);

			// --- Simulation -------------------------------------------------
			void Jouer();
			void MettreEnPause();
			void Arreter();
			void UnPas();
			void ChargerNiveau(int32 i);
			void Simuler(float32 dt);

			// --- Interaction dans la vue ------------------------------------
			void InteragirVue(float32 dt);
			void DebutOutil(const physic2d::NkV2 &w);
			void ContinuerOutil(const physic2d::NkV2 &w, float32 dt);
			void FinOutil(const physic2d::NkV2 &w);
			void ChoisirOutil(Outil o);
			void ChoisirActeur(physic2d::NkActeur a);
			void Selectionner(uint32 id);
			void SupprimerSelection();
			void Focaliser();
			void Recadrer();
			bool SourisDansVue() const;

			// --- Dessin (Physic2DPanneaux.cpp) ------------------------------
			void DessinerVue();
			void DessinerSurcouchesVue();
			void DessinerMenu();
			void DessinerBarreOutils();
			void DessinerPlacerActeurs();
			void DessinerOutliner();
			void DessinerDetails();
			void DessinerDetailsCorps(physic2d::NkCorps &c, uint32 index, float32 &y, const physic2d::NkRect &zone);
			void DessinerDetailsMonde(float32 &y, const physic2d::NkRect &zone);
			void DessinerBas();
			void DessinerJournal(const physic2d::NkRect &r);
			void DessinerStats(const physic2d::NkRect &r);
			void DessinerAide(const physic2d::NkRect &r);
			void DessinerPopup();
			int32 ElementsPopup(Popup p, ElementMenu *out, int32 max) const;
			void ExecuterMenu(Popup p, int32 action);
			void OuvrirPopup(Popup p, const physic2d::NkRect &declencheur);
			physic2d::NkRect RectPopup() const;
			void EnTetePanneau(const physic2d::NkRect &r, const char *titre, physic2d::NkIcone ic);
			bool LigneProp(const char *id, float32 &y, const physic2d::NkRect &zone, const char *libelle, float32 &v,
						   float32 mn, float32 mx, const char *fmt);
			bool LigneCase(const char *id, float32 &y, const physic2d::NkRect &zone, const char *libelle, bool &v);
			void LigneTexte(float32 &y, const physic2d::NkRect &zone, const char *libelle, const char *valeur);

			static physic2d::NkIcone IconeActeur(physic2d::NkActeur a);
			static physic2d::NkIcone IconeMateriau(physic2d::NkMateriau m);
			static const char *NomOutil(Outil o);
			static physic2d::NkIcone IconeOutil(Outil o);

			// --- Rendu ------------------------------------------------------
			renderer::NkGuiCanvasBackend mBackend;
			nkgui::NkGuiDrawList mDl;
			nkgui::NkGuiFont mPolice;
			nkgui::NkGuiFont mPoliceGrande;
			nkgui::NkGuiFont mPoliceMono;
			float32 mTaillePolice = 13.f;
			float32 mTailleGrande = 17.f;
			float32 mTailleMono = 12.f;
			float32 mEchelle = 1.f;
			float32 mEchellePolices = 0.f;
			bool mPret = false;

			physic2d::NkUE5Ui mUi;
			physic2d::NkUE5Entree mEntree;
			Disposition mDisp;
			bool mMontrerGauche = true;
			bool mMontrerDroite = true;
			bool mMontrerBas = true;

			// --- Monde ------------------------------------------------------
			physic2d::NkMonde mMonde;
			physic2d::NkMonde mPhoto; ///< l'etat au moment de "Jouer"
			bool mPhotoValide = false;
			Etat mEtat = Etat::EDITION;
			float32 mAccumulateur = 0.f;
			int32 mNiveau = 0;
			physic2d::NkAlea mAlea;

			// --- Camera & vue -----------------------------------------------
			physic2d::NkCamera2D mCam;
			physic2d::NkOptionsVue mOptions;

			// --- Outils -----------------------------------------------------
			Outil mOutil = Outil::SAISIR;
			physic2d::NkActeur mActeur = physic2d::NkActeur::NK_A_BLOB;
			uint32 mSelection = 0; ///< identifiant STABLE du corps (0 = aucun)
			bool mOutilEnCours = false;
			physic2d::NkV2 mDepartOutil;
			physic2d::NkV2 mDernierMonde;
			physic2d::NkV2 mVitesseSouris;
			int32 mCorpsPinceau = -1;
			bool mPanoramique = false;
			float32 mPanoX = 0.f;
			float32 mPanoY = 0.f;
			float32 mFlashExplosion = 0.f;
			physic2d::NkV2 mPosExplosion;
			physic2d::NkV2 mTraineCouteau[24];
			int32 mNbTraine = 0;
			bool mSourisDansVuePrec = false;
			bool mPopupAuDebut = false;
			bool mGlisseActeur = false; ///< glisser-deposer depuis "Placer des acteurs"
			float32 mAttenteRuptures = 0.f;

			// --- Panneaux ---------------------------------------------------
			Popup mPopup = Popup::AUCUN;
			physic2d::NkRect mDeclencheur = {0.f, 0.f, 0.f, 0.f};
			int32 mCategorie = -1; ///< -1 = toutes
			int32 mOngletBas = 0;
			float32 mDefilOutliner = 0.f;
			float32 mDefilDetails = 0.f;
			float32 mDefilJournal = 0.f;
			float32 mDefilActeurs = 0.f;
			bool mCatOuverte[6] = {true, true, true, true, true, true};
			NkVector<LigneJournal> mJournal;
			float32 mTemps = 0.f;

			// --- Mesures ----------------------------------------------------
			static constexpr int32 kHist = 240;
			float32 mHistPhys[kHist] = {};
			float32 mHistImage[kHist] = {};
			float32 mHistEnergie[kHist] = {};
			int32 mHistIndex = 0;
			float32 mMsPhysique = 0.f;
			float32 mFps = 60.f;
			uint32 mRupturesVues = 0;
	};

} // namespace nkentseu
