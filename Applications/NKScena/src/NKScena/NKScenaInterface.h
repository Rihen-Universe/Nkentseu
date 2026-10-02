#pragma once
// -----------------------------------------------------------------------------
// @File    NKScenaInterface.h
// @Brief   L'INTERFACE de NKScena : EXACTEMENT l'apparence d'UnkenyEditor (comme
//          Nogee et NkAnimaEditor), peinte avec les pieces PARTAGEES de
//          NKEditorKit (Famille), autour de la vue 3D de Noge au centre -- et,
//          au premier onglet du tiroir du bas, LA FRISE partagee facon
//          Sequencer d'UE5, sa seule surface propre.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// C'EST UNE COUCHE (NkOverlay) de NkApplication, derivee de la trame de la
// famille (editorkit::NkFamilleEditeur) : NKScena ne peint que son corps, ses
// barres et ses menus, et execute ses actions -- toutes passent par
// `Executer`, et toutes celles du modele sont des fonctions de NkScenaModele
// (eprouvees par `--selftest`).
//
// OU EST QUOI (un fichier par region, comme Nogee et UnkenyEditor) :
//   NKScenaInterface.cpp          la couche, la trame, le chrome, les menus, les actions
//   NKScenaInterfaceVue.cpp       le viseur : image, grille, cameras, gizmo, camera d'edition
//   NKScenaInterfacePanneaux.cpp  Outliner (scene + pistes), Details (cartes), tiroir
//                                 (Frise, Contenu, Journal, Terminal)
// -----------------------------------------------------------------------------

#include "Noge/Core/NkLayer.h"
#include "NKEditorKit/Famille/NkFamille.h"
#include "NKEditorKit/NkTheme.h"
#include "NKEditorKit/Components/NkTimelineModel.h"
#include "NKEditorKit/Terminal/NkTerminalPanneau.h"
#include "NKGui/NKGui.h"
#include "NKGui/NkGuiRHIBackend.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKScena/NKScenaModele.h"

namespace nkentseu {

	namespace editorkit {
		struct NkFilePickerNavState;
	}

	class NkEngineLayer;
	class NkWindow;

	namespace nogee {
		class NogeeVue3D;
	}

	namespace nkscena {

		/// L'etat du RENDU d'une sequence dans l'application (NKScenaApp.cpp le mene).
		struct NkScenaRenduEnCours {
				bool actif = false;
				int32 image = 0;  ///< la prochaine image a relire
				int32 total = 0;
				NkString dossier;
				bool annuler = false;
		};

		/// Ce que l'application prete a l'interface.
		struct NkScenaHote {
				NkIDevice *device = nullptr;
				NkGraphicsApi api = NkGraphicsApi::NK_GFX_API_OPENGL;
				NkWindow *fenetre = nullptr;
				NkScenaModele *modele = nullptr;
				nogee::NogeeVue3D *vue = nullptr;
				NkEngineLayer *moteur = nullptr;
				NkScenaRenduEnCours *rendu = nullptr;
				/// Le dossier montre sous « Contenu ».
				NkString contenu = "Build/NKScena";
				/// Demande de rendu (l'application la prend a l'image suivante).
				bool *demandeRendu = nullptr;
		};

		/// Les menus (0..6 : ceux de la barre de titre).
		enum NkScenaMenu : int32 {
			SCENA_MENU_FICHIER = 0,
			SCENA_MENU_EDITION,
			SCENA_MENU_PISTE,
			SCENA_MENU_RENDU,
			SCENA_MENU_FENETRE,
			SCENA_MENU_AIDE,
			SCENA_MENU_PISTE_PLUS = 10, ///< « + Piste » : les entites, les plans camera
			SCENA_MENU_PLAN_ICI,		///< un plan a l'instant demande : les cameras
			SCENA_MENU_CTX_ENTITE,
			SCENA_MENU_OUTIL,
		};

		/// Les actions : TOUTES passent par NkScenaInterface::Executer.
		enum NkScenaAction : int32 {
			SCENA_A_AUCUNE = 0,
			SCENA_A_NOUVELLE,
			SCENA_A_OUVRIR_SEQUENCE,
			SCENA_A_ENREGISTRER,
			SCENA_A_ENREGISTRER_SOUS,
			SCENA_A_OUVRIR_SCENE,
			SCENA_A_SCENE_DEMO,
			SCENA_A_EXEMPLE,
			SCENA_A_QUITTER,
			SCENA_A_ANNULER,
			SCENA_A_REFAIRE,
			SCENA_A_SUPPRIMER,
			SCENA_A_COPIER,
			SCENA_A_COLLER,
			SCENA_A_JOUER,
			SCENA_A_PAUSE,
			SCENA_A_STOP,
			SCENA_A_IMAGE_SUIVANTE,
			SCENA_A_IMAGE_PRECEDENTE,
			SCENA_A_CLE,			 ///< une cle au curseur sur la piste active (ou l'entite choisie)
			SCENA_A_CLES_ENTITE,	 ///< une cle sur les trois pistes de l'entite choisie
			SCENA_A_PISTE_ENTITE,	 ///< la piste de transformation de l'entite choisie
			SCENA_A_PISTE_PLANS,
			SCENA_A_PLAN_ICI,		 ///< un plan de la camera choisie au curseur
			SCENA_A_RENDRE,
			SCENA_A_ANNULER_RENDU,
			SCENA_A_VUE_CAMERA,		 ///< bascule vue libre / camera du plan
			SCENA_A_CADRER,
			SCENA_A_DESELECTIONNER,
			SCENA_A_VOIR_OUTLINER,
			SCENA_A_VOIR_DETAILS,
			SCENA_A_VOIR_TIROIR,
			SCENA_A_TIROIR_FRISE,
			SCENA_A_TIROIR_CONTENU,
			SCENA_A_TIROIR_JOURNAL,
			SCENA_A_TIROIR_TERMINAL,
			SCENA_A_THEME,
			SCENA_A_DISPOSITION,
			SCENA_A_GRILLE,
			SCENA_A_OUTIL = 200,		///< + 0 selection, 1 deplacer, 2 tourner, 3 echelle
			SCENA_A_PISTE_DE = 1000,	///< + indice d'entite (menu « + Piste »)
			SCENA_A_PLAN_DE = 2000,		///< + indice de camera (menu des plans)
		};

		class NkScenaInterface final : public NkOverlay, public editorkit::NkFamilleEditeur {
			public:
				static constexpr uint32 kTexVue = 4096u; ///< la vue 3D (image hors ecran)

				explicit NkScenaInterface(const NkScenaHote &hote) noexcept;
				~NkScenaInterface() override;

				void OnAttach() override;
				void OnDetach() override;
				void OnUpdate(float dt) override;
				void OnUIRender() override;
				bool OnEvent(NkEvent *event) override;

				bool DemandeQuitter() const noexcept {
					return QuitterDemande();
				}
				/// Le viseur, a la derniere trame (la taille de la vue 3D).
				const nkgui::NkRect &Viseur() const noexcept {
					return plan.viseur;
				}
				/// L'onglet du tiroir (captures : --tiroir=frise|contenu|journal|terminal).
				void PoserOngletTiroir(int32 o) noexcept {
					mOngletTiroir = o;
				}
				/// Avant la destruction du device (le piege de PV3DE, comme Nogee).
				void LibererGpu() noexcept;
				/// Ecrit une ligne au Journal (l'application, pendant un rendu).
				void Journal(const char *texte, editorkit::NkFamilleNiveau niveau = editorkit::NkFamilleNiveau::Info);

				void Executer(int32 a) override;
				void Remplir(int32 m, NkVector<editorkit::NkFamilleEntreeMenu> &sortie) override;

			protected:
				NkString Titre() const override;
				const char *const *MenusBarre(int32 &nombre) const override;
				void PeindreLogo(nkgui::NkGuiDrawList &dl, float32 x, float32 y, float32 cote, bool fondSombre) override;
				void AvantCorps(editorkit::NkFamilleCtx &c) override;
				void PeindreCorps(editorkit::NkFamilleCtx &c) override;
				void PeindreOnglets(editorkit::NkFamilleCtx &c) override;
				void PeindreBarreOutils(editorkit::NkFamilleCtx &c) override;
				void PeindreStatut(editorkit::NkFamilleCtx &c) override;
				void Raccourcis(editorkit::NkFamilleCtx &c) override;
				void Fermer() override;
				bool Modale() const override;
				void PeindreModale(editorkit::NkFamilleCtx &c) override;

			private:
				void Journaliser();
				bool ChampAuClavier() const noexcept;
				void SuivreChoixDeLaFrise();

				// ── Le viseur (NKScenaInterfaceVue.cpp) ────────────────────────
				void Vue(editorkit::NkFamilleCtx &c);
				bool Projeter(const math::NkVec3f &p, nkgui::NkVec2 &s) const noexcept;
				ecs::NkEntityId Prendre(const nkgui::NkVec2 &s) const;
				void Grille(nkgui::NkGuiDrawList &dl);
				void Cameras(editorkit::NkFamilleCtx &c, nkgui::NkGuiDrawList &dl);
				bool Gizmo(editorkit::NkFamilleCtx &c, nkgui::NkGuiDrawList &dl, bool souris);
				void BarreFlottante(editorkit::NkFamilleCtx &c);
				void CadreDuPlan(editorkit::NkFamilleCtx &c, nkgui::NkGuiDrawList &dl);

				// ── Les panneaux (NKScenaInterfacePanneaux.cpp) ────────────────
				void Outliner(editorkit::NkFamilleCtx &c);
				void Details(editorkit::NkFamilleCtx &c);
				void DetailsEntite(editorkit::NkFamilleCtx &c, editorkit::NkFamilleInspecteur &I, ecs::NkEntityId id);
				void DetailsPiste(editorkit::NkFamilleCtx &c, editorkit::NkFamilleInspecteur &I);
				void DetailsCle(editorkit::NkFamilleCtx &c, editorkit::NkFamilleInspecteur &I);
				void DetailsPlan(editorkit::NkFamilleCtx &c, editorkit::NkFamilleInspecteur &I);
				void DetailsSequence(editorkit::NkFamilleCtx &c, editorkit::NkFamilleInspecteur &I);
				void Tiroir(editorkit::NkFamilleCtx &c);
				void Frise(editorkit::NkFamilleCtx &c, const nkgui::NkRect &zone);

			public:
				// Les rappels des composants du kit (fonctions libres) y accedent.
				NkScenaHote mHote;
				NkScenaModele &M() noexcept {
					return *mHote.modele;
				}
				const NkScenaModele &M() const noexcept {
					return *mHote.modele;
				}
				/// Les lignes de l'Outliner : une entite, une piste, ou un en-tete.
				struct NkLigne {
						ecs::NkEntityId entite = ecs::NkEntityId::Invalid();
						nk_uint64 piste = 0;
						NkString objet;
				};
				NkVector<NkLigne> mLignes;
				NkVector<ecs::NkEntityId> mEntites;
				/// Les noms proposes par les menus « + Piste » et « Plan » (indices des actions).
				NkVector<NkString> mNomsMenu;
				float32 mTempsMenuPlan = 0.f;
				editorkit::NkTimelineResult mFriseResultat;

			private:
				// ── Le rendu de l'interface ───────────────────────────────────
				nkgui::NkGuiRHIBackend mBackend;
				const nkgui::NkGuiDrawList *mListe = nullptr;
				bool mPret = false;
				bool mBackendPret = false;
				bool mRappelPose = false;
				uint32 mGenerationVue = 0;

				// ── Les panneaux ──────────────────────────────────────────────
				editorkit::NkFamilleOutliner mOutliner;
				editorkit::NkFamilleDetailsEtat mDetails;
				editorkit::NkFamilleContenu mContenu;
				editorkit::NkFamilleJournal mJournal;
				editorkit::NkTerminalPanneau mTerminal;
				memory::NkUniquePtr<editorkit::NkFilePickerNavState> mSelecteur;
				int32 mUsageSelecteur = 0; ///< 1 ouvrir sequence, 2 enregistrer sous, 3 ouvrir scene
				char mTamponSelecteur[512] = {};
				int32 mOngletDroite = 0; ///< 0 Details, 1 Sequence
				int32 mOngletTiroir = 0; ///< 0 Frise, 1 Contenu, 2 Journal, 3 Terminal

				// ── La frise : ce qu'elle montrait a la trame d'avant ────────
				nk_uint64 mFriseActiveAvant = 0;
				nk_uint64 mFriseClipAvant = 0;
				uint32 mFriseSelectionAvant = 0;
				bool mFriseSuivie = false;

				// ── Le temps ──────────────────────────────────────────────────
				float32 mDt = 1.f / 60.f;

				// ── Le viseur ─────────────────────────────────────────────────
				int32 mOutil = 0; ///< 0 selection, 1 deplacer, 2 tourner, 3 echelle
				bool mVoirGrille = true;
				bool mOrbite = false;
				bool mPan = false;
				nkgui::NkVec2 mSourisAvant{0.f, 0.f};
				int32 mAxeTenu = -1;
				math::NkVec3f mDepartPos{0.f, 0.f, 0.f};
				math::NkVec3f mDepartEchelle{1.f, 1.f, 1.f};
				math::NkQuatf mDepartRot = math::NkQuatf::Identity();
				float32 mCumul = 0.f;
				float32 mGlisseDroit = 0.f;
				nkgui::NkRect mBarre{0.f, 0.f, 0.f, 0.f};
		};

	} // namespace nkscena
} // namespace nkentseu
