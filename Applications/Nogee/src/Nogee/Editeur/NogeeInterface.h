#pragma once
// -----------------------------------------------------------------------------
// @File    NogeeInterface.h
// @Brief   L'INTERFACE de Nogee : la disposition et le dessin d'UnkenyEditor
//          (feuille de route R32 : « Nogee doit ressembler EXACTEMENT a
//          UnkenyEditor »), peints avec les pieces PARTAGEES de NKEditorKit
//          (NKEditorKit/Famille), autour de la vue 3D du moteur au centre.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// C'EST UNE COUCHE (NkOverlay) de NkApplication : elle tient le contexte NKGui,
// ses polices et le backend NKGui -> NKRHI, et se soumet dans la passe
// Overlay2D du renderer de l'application (SetUIOverlayCallback, le patron de
// PV3DE/MedicalUILayer). La vue 3D, rendue a part (NogeeVue3D), y est une IMAGE.
//
// OU EST QUOI (un fichier par region, comme UnkenyEditor) :
//   NogeeInterface.cpp          la couche, la trame, le chrome, les menus, les actions
//   NogeeInterfaceVue.cpp       le viseur : image, grille, selection, gizmo, camera
//   NogeeInterfacePanneaux.cpp  Placer des acteurs, Outliner, Details, tiroir
// -----------------------------------------------------------------------------

#include "Noge/Core/NkLayer.h"
#include "NKEditorKit/Famille/NkFamille.h"
#include "NKEditorKit/NkTheme.h"
#include "NKEditorKit/Terminal/NkTerminalPanneau.h"
#include "NKGui/NKGui.h"
#include "NKGui/NkGuiRHIBackend.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKMemory/NkUniquePtr.h"
#include "Nogee/Editeur/NogeeModele.h"

namespace nkentseu {

	namespace editorkit {
		struct NkFilePickerNavState;
	}

	class NkEngineLayer;
	class NkWindow;

	namespace nogee {

		class NogeeVue3D;

		/// Ce que l'application prete a l'interface.
		struct NogeeHote {
				NkIDevice *device = nullptr;
				NkGraphicsApi api = NkGraphicsApi::NK_GFX_API_OPENGL;
				NkWindow *fenetre = nullptr;
				NogeeModele *modele = nullptr;
				NogeeVue3D *vue = nullptr;
				NkEngineLayer *moteur = nullptr;
				/// Le dossier montre sous « Contenu » (Content Browser).
				NkString contenu = "Resources";
		};

		/// Les menus (0..3 : ceux de la barre de titre).
		enum NogeeMenu : int32 {
			NOGEE_MENU_FICHIER = 0,
			NOGEE_MENU_EDITION,
			NOGEE_MENU_FENETRE,
			NOGEE_MENU_AIDE,
			NOGEE_MENU_OUTIL = 10,
			NOGEE_MENU_AJOUTER,
			NOGEE_MENU_APPAREIL,
			NOGEE_MENU_REGLAGES,
			NOGEE_MENU_CTX_ENTITE,
			NOGEE_MENU_CTX_VIDE,
			NOGEE_MENU_AJOUTER_ICI,
			NOGEE_MENU_COMPOSANT,
			NOGEE_MENU_CARTE,
			NOGEE_MENU_PAS_GRILLE,
			NOGEE_MENU_PAS_ANGLE,
			NOGEE_MENU_PAS_ECHELLE,
		};

		/// Les actions : TOUTES passent par NogeeInterface::Executer.
		enum NogeeAction : int32 {
			NOGEE_A_AUCUNE = 0,
			NOGEE_A_NOUVEAU,
			NOGEE_A_OUVRIR,
			NOGEE_A_ENREGISTRER,
			NOGEE_A_QUITTER,
			NOGEE_A_ANNULER,
			NOGEE_A_REFAIRE,
			NOGEE_A_DUPLIQUER,
			NOGEE_A_SUPPRIMER,
			NOGEE_A_RENOMMER,
			NOGEE_A_DESELECTIONNER,
			NOGEE_A_CADRER,
			NOGEE_A_DETACHER,
			NOGEE_A_JOUER,
			NOGEE_A_PAUSE,
			NOGEE_A_ARRETER,
			NOGEE_A_PAS,
			NOGEE_A_VOIR_PLACER,
			NOGEE_A_VOIR_OUTLINER,
			NOGEE_A_VOIR_DETAILS,
			NOGEE_A_VOIR_TIROIR,
			NOGEE_A_TIROIR_CONTENU,
			NOGEE_A_TIROIR_JOURNAL,
			NOGEE_A_TIROIR_TERMINAL,
			NOGEE_A_THEME,
			NOGEE_A_DISPOSITION,
			NOGEE_A_ANCIENNE_COQUILLE,
			NOGEE_A_GRILLE,
			NOGEE_A_REPERE_LOCAL,
			NOGEE_A_ACCROCHE_GRILLE,
			NOGEE_A_ACCROCHE_ANGLE,
			NOGEE_A_ACCROCHE_ECHELLE,
			NOGEE_A_FILAIRE,
			NOGEE_A_NOUVELLE_ENTITE,
			NOGEE_A_RETIRER_COMPOSANT,
			NOGEE_A_OUVRIR_FICHIER,	   ///< le selecteur de fichiers du kit
			NOGEE_A_ENREGISTRER_SOUS, ///< le selecteur de fichiers du kit
			NOGEE_A_OUTIL = 200,		///< + NogeeOutil
			NOGEE_A_PAS_GRILLE = 300,	///< + indice du pas
			NOGEE_A_PAS_ANGLE = 320,
			NOGEE_A_PAS_ECHELLE = 340,
			NOGEE_A_COMPOSANT = 400,	///< + NogeeComposant
			NOGEE_A_PLACER = 1000,		///< + indice du catalogue : au centre de la vue
			NOGEE_A_PLACER_ICI = 1200,	///< + indice du catalogue : au point du clic droit
		};

		/// Les composants que les Details montrent en cartes, et que « + Ajouter »
		/// sait poser (ce que Noge rend ou simule DEJA).
		enum class NogeeComposant : int32 {
			Transform = 0,
			Maillage,
			Materiau,
			Lumiere,
			Camera,
			Corps,
			Collisionneur,
			Hierarchie,
			Count
		};

		class NogeeInterface final : public NkOverlay {
			public:
				static constexpr uint32 kTexVue = 4096u; ///< la vue 3D (image hors ecran)

				explicit NogeeInterface(const NogeeHote &hote) noexcept;
				~NogeeInterface() override;

				void OnAttach() override;
				void OnDetach() override;
				void OnUpdate(float dt) override;
				void OnUIRender() override;
				bool OnEvent(NkEvent *event) override;

				bool Prete() const noexcept {
					return mPret && mBackendPret;
				}
				bool DemandeQuitter() const noexcept {
					return mDemandeQuitter;
				}
				/// Le viseur, a la derniere trame (la taille de la vue 3D).
				const nkgui::NkRect &Viseur() const noexcept {
					return mPlan.viseur;
				}
				/// Pose l'onglet du tiroir (captures : --tiroir=journal|terminal|contenu).
				void PoserOngletTiroir(int32 o) noexcept {
					mOngletTiroir = o;
				}
				void PoserTheme(bool clair);
				/// Rend au device ce que l'interface y tient (backend, rappel de la passe
				/// d'interface). A appeler AVANT la destruction du device : la pile de
				/// couches n'est detachee qu'apres (~NkLayerStack), device deja mort --
				/// le meme piege que PV3DE (MedicalUILayer::ReleaseGpu).
				void LibererGpu() noexcept;

				/// L'action `a` (menus, barres, raccourcis, banc).
				void Executer(int32 a);
				/// Les entrees du menu `m`.
				void Remplir(int32 m, NkVector<editorkit::NkFamilleEntreeMenu> &sortie);

			private:
				// ── La trame (NogeeInterface.cpp) ──────────────────────────────
				void Trame(editorkit::NkFamilleCtx &c);
				void BarreTitre(editorkit::NkFamilleCtx &c);
				void OngletsScene(editorkit::NkFamilleCtx &c);
				void BarreOutils(editorkit::NkFamilleCtx &c);
				void Statut(editorkit::NkFamilleCtx &c);
				void Raccourcis(editorkit::NkFamilleCtx &c);
				void AppliquerFenetre();
				void Journaliser();
				bool ChampAuClavier() const noexcept;

				// ── Le viseur (NogeeInterfaceVue.cpp) ──────────────────────────
				void Vue(editorkit::NkFamilleCtx &c);
				bool Projeter(const math::NkVec3f &p, nkgui::NkVec2 &s) const noexcept;
				bool RayonSouris(const nkgui::NkVec2 &s, math::NkVec3f &origine, math::NkVec3f &direction) const noexcept;
				math::NkVec3f PointAuSol(const nkgui::NkVec2 &s) const noexcept;
				math::NkVec3f PointDePose(int32 element, bool auCentre, const nkgui::NkVec2 &s) const noexcept;
				ecs::NkEntityId Prendre(const nkgui::NkVec2 &s) const;
				void Grille(nkgui::NkGuiDrawList &dl);
				bool Gizmo(editorkit::NkFamilleCtx &c, nkgui::NkGuiDrawList &dl, bool souris);
				void BarreFlottante(editorkit::NkFamilleCtx &c);

				// ── Les panneaux (NogeeInterfacePanneaux.cpp) ──────────────────
				void Placer(editorkit::NkFamilleCtx &c);
				void Outliner(editorkit::NkFamilleCtx &c);
				void Details(editorkit::NkFamilleCtx &c);
				void Cartes(editorkit::NkFamilleCtx &c, editorkit::NkFamilleInspecteur &I, ecs::NkEntityId id);
				void Monde(editorkit::NkFamilleCtx &c, const nkgui::NkRect &zone);
				void Tiroir(editorkit::NkFamilleCtx &c);
				bool PeutAjouter(NogeeComposant k) const;
				void AjouterComposant(NogeeComposant k, int32 variante);

			public:
				// Les rappels des composants du kit (fonctions libres) y accedent.
				NogeeHote mHote;
				NogeeModele &M() noexcept {
					return *mHote.modele;
				}
				const NogeeModele &M() const noexcept {
					return *mHote.modele;
				}
				editorkit::NkFamilleMenus mMenus;
				ecs::NkEntityId mCibleMenu = ecs::NkEntityId::Invalid();
				math::NkVec3f mPointMenu{0.f, 0.f, 0.f};
				int32 mCarteMenu = -1;
				NkVector<ecs::NkEntityId> mArbreEntites;

			private:
				// ── NKGui et son rendu ────────────────────────────────────────
				nkgui::NkGuiContext mCtx;
				nkgui::NkGuiFont mPolice, mPetite, mMono;
				nkgui::NkGuiRHIBackend mBackend;
				nkgui::NkGuiDrawList mFusion; ///< dl + dlOverlay : UNE Submit par image
				bool mPret = false;
				bool mBackendPret = false;
				bool mRappelPose = false;
				uint32 mGenerationVue = 0;
				editorkit::NkTheme mTheme;
				editorkit::NkFamillePalette mPal;
				bool mClair = false;
				editorkit::NkFamilleEntree mEntree;

				// ── La disposition et la fenetre ──────────────────────────────
				editorkit::NkFamillePlan mPlan;
				editorkit::NkFamilleFenetre mFen;
				bool mDemandeQuitter = false;

				// ── Les panneaux ──────────────────────────────────────────────
				editorkit::NkFamillePlacer mPlacer;
				editorkit::NkFamilleOutliner mOutliner;
				editorkit::NkFamilleDetailsEtat mDetails;
				editorkit::NkFamilleContenu mContenu;
				editorkit::NkFamilleJournal mJournal;
				editorkit::NkTerminalPanneau mTerminal;
				/// LE selecteur de fichiers de NKEditorKit (celui d'UnkenyEditor),
				/// modal : Ouvrir…, Enregistrer sous…
				memory::NkUniquePtr<editorkit::NkFilePickerNavState> mSelecteur;
				int32 mUsageSelecteur = 0; ///< 1 ouvrir, 2 enregistrer sous
				char mTamponSelecteur[512] = {};
				int32 mOngletDroite = 0; ///< 0 Details, 1 Monde
				int32 mOngletTiroir = 0; ///< 0 Contenu, 1 Journal, 2 Terminal

				// ── Le temps ──────────────────────────────────────────────────
				float32 mDt = 1.f / 60.f;
				float32 mTemps = 0.f;
				float32 mIps = 0.f;
				float32 mTempsIps = 0.f;
				int32 mTramesIps = 0;

				// ── Le viseur ─────────────────────────────────────────────────
				bool mOrbite = false;
				bool mPan = false;
				nkgui::NkVec2 mSourisAvant{0.f, 0.f};
				int32 mAxeTenu = -1;		  ///< le gizmo tenu : 0 X, 1 Y, 2 Z, 3 le centre
				math::NkVec3f mDepartPos{0.f, 0.f, 0.f};
				math::NkVec3f mDepartEchelle{1.f, 1.f, 1.f};
				math::NkQuatf mDepartRot = math::NkQuatf::Identity();
				float32 mCumul = 0.f;
				float32 mGlisseDroit = 0.f; ///< le chemin du clic droit (court : un menu)
				nkgui::NkRect mBarre{0.f, 0.f, 0.f, 0.f};
				bool mFilaire = false;
		};

	} // namespace nogee
} // namespace nkentseu
