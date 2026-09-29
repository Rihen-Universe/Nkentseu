// =============================================================================
// Physic2D.h — la demo de simulation 2D, sur Unkeny
//
// CE QU'ELLE EST
//   Une application MINCE. Tout ce qui simule vit plus bas :
//     NKPhysics/NkParticules2D   le solveur (corps mous, fluides, atomes)
//     NKPhysics/NkPhysicsWorld   les corps rigides
//     Unkeny/NkScene             les entites NKECS qui portent les deux
//     Unkeny/Rendu               le dessin des corps mous et de la grille
//   Ici il ne reste que ce qui est PROPRE a la demo : ses outils, ses niveaux
//   (Physic2DActeurs), et sa mise en page.
//
// CE QU'ELLE N'EST PAS
//   L'editeur. L'editeur facon UE5 / NK3DModeler (docking, inspecteur,
//   navigateur de contenu) est UnkenyEditor, sur NKEditorKit. La demo, elle,
//   doit demarrer a l'identique sur les six plateformes, doigt compris :
//   ses panneaux se replient en portrait, le pincement zoome.
//
// LES ETATS — ceux d'UE5
//   EDITION   la simulation est arretee, on pose des acteurs
//   JEU       elle tourne ; une PHOTO de la scene a ete prise au lancement
//   PAUSE     elle est suspendue ; "Pas" avance d'un pas fixe
//   ARRET     rend la photo : on retrouve la scene d'avant le lancement
//
// FICHIERS
//   Physic2D.h / .cpp       etats, entrees, outils
//   Physic2DEcran.cpp       mise en page et dessin
//   Physic2DActeurs.{h,cpp} les acteurs et les niveaux
//   Physic2DBanc.{h,cpp}    le banc : --selftest
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#pragma once

#include "Physic2D/Physic2DActeurs.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"

namespace nkentseu {

	class Physic2DBancVisuel; // le banc hors ecran (images) : ami, rien de plus

	class Physic2D : public renderer::NkCanvasGuiApp {
		public:
			Physic2D();

			enum class NkOutil : uint8 {
				NK_POSER = 0,
				NK_SAISIR,
				NK_COUTEAU,
				NK_EXPLOSION,
				NK_AIMANT,
				NK_OBSTACLE,
				NK_GOMME,
				NK_EPINGLE,
				NK_CAMERA,
				NK_COUNT
			};

			enum class NkEtat : uint8 { NK_EDITION = 0, NK_JEU, NK_PAUSE };

		protected:
			NkOptional<int> OnCommandLine(const NkVector<NkString> &args) override;
			bool OnGuiInit() override;
			void OnLayout(const renderer::NkLayoutInfo &info) override;
			void OnTick(float32 dt) override;
			void OnDraw(nkgui::NkGuiDrawList &dl) override;
			bool OnEvent(const NkEvent &e) override;
			bool OnPointer(const NkPointer &p) override;
			bool OnKeyPress(const NkKeyPressEvent &e) override;

		private:
			friend class Physic2DBancVisuel;
			using NkRect = nkgui::NkRect;
			using NkVec2f = math::NkVec2f;

			// --- Ce qui se clique -------------------------------------------
			enum class NkGenre : uint8 { NK_COMMANDE = 0, NK_OUTIL, NK_ACTEUR, NK_REGLAGE };
			enum NkCommande : int32 {
				CMD_JOUER = 0,
				CMD_PAS,
				CMD_ARRET,
				CMD_NIV_PREC,
				CMD_NIV_SUIV,
				CMD_VUE,
				CMD_GRILLE,
				CMD_CADRER,
				CMD_PANNEAU
			};
			struct NkCible {
					NkRect r;
					NkGenre genre;
					int32 valeur;
			};
			/// Les reglages du monde qui ont leurs boutons - / +.
			enum NkReglage : int32 { REG_TEMPERATURE = 0, REG_VENT, REG_GRAVITE, REG_AIR, REG_SOUSPAS, REG_COUNT };

			struct NkPlan {
					NkRect ecran{0, 0, 0, 0};
					NkRect barre{0, 0, 0, 0};
					NkRect palette{0, 0, 0, 0};
					NkRect details{0, 0, 0, 0}; ///< vide quand le panneau est replie
					NkRect vue{0, 0, 0, 0};
					NkRect statut{0, 0, 0, 0};
					NkRect nomNiveau{0, 0, 0, 0};
					bool portrait = false;
					bool detailsFlottant = false; ///< pose PAR-DESSUS la vue (ecran etroit)
					float32 ligne = 20.f;		  ///< hauteur d'une ligne de petite police
					NkVector<NkCible> cibles;
					NkVector<NkRect> titres; ///< entetes de section de la palette
			};

			// --- Etats ------------------------------------------------------
			void Charger(int32 niveau);
			void Jouer();
			void Pause();
			void Arreter();
			void UnPas();
			void Commande(int32 cmd);
			void Regler(int32 reglage, bool plus);
			void AnnulerGeste();

			// --- Outils -----------------------------------------------------
			void GesteDebut(const NkVec2f &ecran);
			void GesteVers(const NkVec2f &ecran);
			void GesteFin(const NkVec2f &ecran);
			void OutilsContinus(float32 dt);
			ecs::NkEntityId RigideSous(const NkVec2f &monde, bool decorAussi) ;
			void Selectionner(const NkVec2f &monde);
			void Zoomer(const NkVec2f &ecran, float32 facteur);

			// --- Ecran (Physic2DEcran.cpp) -----------------------------------
			void Planifier();
			void Cadrer();
			void DessinerVue(nkgui::NkGuiDrawList &dl);
			void DessinerFormes(nkgui::NkGuiDrawList &dl);
			void DessinerSurimpressions(nkgui::NkGuiDrawList &dl);
			void DessinerBarre(nkgui::NkGuiDrawList &dl);
			void DessinerPalette(nkgui::NkGuiDrawList &dl);
			void DessinerDetails(nkgui::NkGuiDrawList &dl);
			void DessinerStatut(nkgui::NkGuiDrawList &dl);
			const NkCible *CibleSous(const NkVec2f &p) const;
			static const char *OutilNom(NkOutil o) noexcept;
			static const char *OutilAide(NkOutil o) noexcept;

			// --- Donnees ----------------------------------------------------
			unkeny::NkScene mScene;
			unkeny::NkTextures2D mTextures;
			unkeny::NkScene::NkPhoto mPhoto;
			unkeny::NkTheme mTheme;
			unkeny::NkOptionsRenduParticules mRendu;
			NkPlan mPlan;
			NkEtat mEtat = NkEtat::NK_JEU;
			int32 mNiveau = 0;
			NkOutil mOutil = NkOutil::NK_POSER;
			physic2d::NkActeur mActeur = physic2d::NkActeur::NK_BLOB;
			bool mGrille = true;
			bool mDetailsOuvert = false; ///< ecran etroit SEULEMENT : le panneau flotte, ferme au depart
			bool mCameraTouchee = false;

			// Le geste en cours dans la vue.
			bool mGeste = false;
			NkVec2f mSouris{-1.f, -1.f}; ///< survol, en pixels (-1 = hors fenetre)
			NkVec2f mGesteDebut{0.f, 0.f};		///< en metres
			NkVec2f mGestePrec{0.f, 0.f};		///< en metres
			NkVec2f mGesteEcranPrec{0.f, 0.f};	///< en pixels (camera)
			ecs::NkEntityId mPinceau = ecs::NkEntityId::Invalid();
			ecs::NkEntityId mRigideSaisi = ecs::NkEntityId::Invalid();
			uint32 mGraine = 0x9E3779B9u;

			// Le doigt : deux contacts = pincement (zoom + deplacement).
			struct NkDoigt {
					uint32 id = 0;
					NkVec2f pos{0.f, 0.f};
					bool actif = false;
			};
			NkDoigt mDoigts[2];
			bool mPincement = false;
			float32 mPinceDistance = 1.f;
			float32 mPinceZoom = 1.f;
			NkVec2f mPinceMonde{0.f, 0.f};

			// Bouton droit / milieu : deplacement de la camera sur ordinateur.
			bool mPanSouris = false;

			// Ce qui se voit apres coup.
			static constexpr int32 kTrace = 24;
			NkVec2f mTrace[kTrace];
			int32 mTraceN = 0;
			float32 mTraceAge = 0.f;
			struct NkEclair {
					NkVec2f pos{0.f, 0.f};
					float32 age = 99.f;
			};
			NkEclair mEclairs[4];
			uint32 mSelection = 0; ///< id STABLE du corps de particules selectionne (0 = aucun)

			// Mesures.
			float32 mMsParPas = 0.f;
			float32 mMsLisse = 0.f;
	};

} // namespace nkentseu
