#pragma once
// -----------------------------------------------------------------------------
// @File    NkFamilleCadre.h
// @Brief   LE CADRE D'UNREAL 5 de la famille : le plan (ou va chaque panneau),
//          la barre de titre maison (logo au coin sur deux lignes, menus, titre,
//          boutons de fenetre), les onglets de scene, la barre d'outils, la
//          barre d'etat, les cloisons, les bords d'une fenetre sans cadre et les
//          menus deroulants.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// D'OU CA VIENT : UnkenyEditor, NkEditeurChrome.cpp (NkEditeurPlanifier,
// NkEditeurDessinerBarreMenus, ...Onglets, ...BarreOutils, ...Statut,
// NkEditeurCloisons, NkEditeurBordsFenetre, NkEditeurDessinerMenuOuvert). Les
// cotes sont les siennes, au pixel : c'est ce qui fait que deux editeurs de la
// famille se ressemblent EXACTEMENT, et non « a peu pres ».
//
// CE QUI RESTE A L'APPLICATION : le logo (un rappel qui le peint dans le carre
// du coin), les NOMS des menus et leurs entrees, les ACTIONS (des entiers
// qu'elle interprete), le titre, les libelles de la barre d'outils. Rien ici ne
// sait ce qu'est une scene.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Famille/NkFamilleStyle.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace editorkit {

		// =====================================================================
		// LE PLAN
		// =====================================================================
		/// Ou va chaque zone, recalcule a chaque trame. La disposition d'UE5 :
		///   [logo][menus ......... titre ......... _ □ ✕]
		///   [logo][onglets de scene                     ]
		///   [barre d'outils                              ]
		///   [Placer][Outliner][barre de vue    ][Details ]
		///   [      ][        ][viseur          ][        ]
		///   [tiroir : Contenu | Journal | Terminal       ]
		///   [barre d'etat                                ]
		struct NkFamillePlan {
				nkgui::NkRect ecran{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect barreMenus{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect barreOnglets{0.f, 0.f, 0.f, 0.f};
				/// Le CARRE du coin haut gauche : il couvre la ligne des menus ET celle
				/// des onglets, comme le logo rond d'Unreal.
				nkgui::NkRect logo{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect barreOutils{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect placer{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect outliner{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect vue{0.f, 0.f, 0.f, 0.f};		///< la colonne centrale entiere
				nkgui::NkRect barreVue{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect viseur{0.f, 0.f, 0.f, 0.f};	///< la vue sous sa barre
				nkgui::NkRect details{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect tiroir{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect statut{0.f, 0.f, 0.f, 0.f};

				// --- Les colonnes (les valeurs d'UnkenyEditor) -----------------
				float32 largeurPlacer = 236.f;
				float32 largeurOutliner = 250.f;
				float32 largeurDetails = 340.f;
				float32 hauteurTiroir = 250.f;
				bool voirPlacer = true;
				bool voirOutliner = true;
				bool voirDetails = true;
				bool voirTiroir = true;
				/// La cloison tenue : 0 Outliner|vue, 1 vue|Details, 2 corps|tiroir,
				/// 3 Placer|Outliner ; -1 aucune.
				int32 cloisonTenue = -1;
		};

		/// Pose toutes les zones pour un ecran de `largeur` x `hauteur`. Les bornes
		/// des colonnes sont re-appliquees a chaque trame : une fenetre retrecie
		/// rend la place au viseur.
		void NkFamillePlanifier(NkFamillePlan &plan, float32 largeur, float32 hauteur) noexcept;

		/// Les cloisons : le survol change le curseur, l'appui les tient, le
		/// glisser deplace la colonne. Rend vrai si une cloison a la souris.
		bool NkFamilleCloisons(NkFamilleCtx &c, NkFamillePlan &plan);

		// =====================================================================
		// LA FENETRE SANS CADRE
		// =====================================================================
		/// Ce que la barre de titre et les bords DEMANDENT a la fenetre. Consomme
		/// par l'application HORS du dessin (NkWindow::Minimize, Maximize,
		/// BeginDragMove, BeginResize) : une boucle modale de l'OS ne doit pas
		/// tourner au milieu d'une trame.
		struct NkFamilleFenetre {
				bool agrandie = false;		///< pose par l'application (NkWindow::IsMaximized)
				bool reduireDemande = false;
				bool agrandirDemande = false; ///< agrandir OU restaurer
				bool fermerDemande = false;
				bool deplacerDemande = false;
				float32 deplacerFractionX = 0.5f; ///< ou la souris tenait la barre (restauration sous le curseur)
				int32 redimDemande = -1;		  ///< NkWindow::NkResizeEdge, -1 = aucun
				bool titreArme = false;
				nkgui::NkVec2 titreAppui{0.f, 0.f};
		};

		/// Les bords (5 px) d'une fenetre sans cadre : curseur, puis demande de
		/// redimensionnement. Un clic au bord n'atteint rien d'autre.
		void NkFamilleBordsFenetre(NkFamilleCtx &c, const NkFamillePlan &plan, NkFamilleFenetre &fenetre);

		// =====================================================================
		// LA BARRE DE TITRE ET LES ONGLETS DE SCENE
		// =====================================================================
		/// Le logo, peint par l'application dans le carre (x, y, cote). `fondSombre`
		/// dit quelle version de la marque prendre.
		using NkFamilleLogoFn = void (*)(nkgui::NkGuiDrawList &dl, float32 x, float32 y, float32 cote, bool fondSombre,
										 void *user);

		/// Un fond est-il SOMBRE (luminance percue < 0,5) ? La marque suit le fond.
		bool NkFamilleFondSombre(const nkgui::NkColor &fond) noexcept;

		struct NkFamilleTitre {
				const char *const *menus = nullptr; ///< « Fichier », « Édition », ...
				int32 nbMenus = 0;
				const char *titre = "";				///< au centre : l'application et ce qu'on edite
				NkFamilleLogoFn logo = nullptr;
				void *logoUser = nullptr;
		};

		/// La barre de titre. `menuOuvert` : l'indice du menu de barre ouvert (-1
		/// aucun ; un autre menu de l'application ouvert : `autreMenuOuvert`).
		/// Rend l'indice du menu a ouvrir (clic, ou survol quand un voisin est
		/// ouvert), -1 sinon ; `ancre` recoit son rectangle.
		int32 NkFamilleBarreTitre(NkFamilleCtx &c, const NkFamillePlan &plan, const NkFamilleTitre &titre,
								  int32 menuOuvert, bool autreMenuOuvert, nkgui::NkRect &ancre, NkFamilleFenetre &fenetre);

		struct NkFamilleOngletScene {
				const char *nom = "";
				bool modifie = false; ///< le point ambre : modifie depuis l'enregistrement
		};
		/// Ce que rendent les onglets de scene.
		struct NkFamilleOngletsResultat {
				int32 choisi = -1;	///< l'onglet clique
				int32 fermer = -1;	///< la croix cliquee
				nkgui::NkRect rect{0.f, 0.f, 0.f, 0.f}; ///< le rectangle de l'onglet ACTIF (bancs, captures)
		};
		NkFamilleOngletsResultat NkFamilleOngletsScene(NkFamilleCtx &c, const NkFamillePlan &plan,
													   const NkFamilleOngletScene *onglets, int32 n, int32 actif);

		// =====================================================================
		// LA BARRE D'OUTILS
		// =====================================================================
		void NkFamilleFondBarreOutils(NkFamilleCtx &c, const nkgui::NkRect &b);
		/// Le trait vertical qui separe deux groupes. Rend le x suivant.
		float32 NkFamilleTrait(NkFamilleCtx &c, const nkgui::NkRect &b, float32 x);
		/// Un bouton a la largeur de son libelle (au moins `largeurMin` : la place
		/// d'un libelle qui VARIE, pour que la barre ne bouge pas). `deroulant`
		/// ajoute la pointe ; `ouvert` le montre enfonce. Rend le x suivant ;
		/// `clic` et `rect` recoivent le geste et la place.
		float32 NkFamilleBoutonOutil(NkFamilleCtx &c, const nkgui::NkRect &b, float32 x, const char *texte, bool deroulant,
									 bool ouvert, bool &clic, nkgui::NkRect &rect, float32 largeurMin = 0.f,
									 bool actif = true);
		/// La largeur que prendra un bouton de la barre (pour reserver la place
		/// du plus long de plusieurs libelles).
		float32 NkFamilleLargeurBoutonOutil(NkFamilleCtx &c, const char *texte, bool deroulant);

		enum class NkFamilleEtatJeu : uint8 { Edition = 0, Jeu, Pause };
		/// Jouer / Pause / Arreter / Un pas, glyphes traces. Rend l'indice clique
		/// (0..3) ou -1 ; `x` avance.
		int32 NkFamilleBoutonsLecture(NkFamilleCtx &c, const nkgui::NkRect &b, float32 &x, NkFamilleEtatJeu etat);

		// =====================================================================
		// LA BARRE D'ETAT
		// =====================================================================
		/// La pastille de l'etat (ÉDITION / EN JEU / EN PAUSE), le message du
		/// moment, et les compteurs a droite.
		void NkFamilleBarreEtat(NkFamilleCtx &c, const nkgui::NkRect &b, NkFamilleEtatJeu etat, const char *message,
								const char *compteurs);

		// =====================================================================
		// LES MENUS DEROULANTS
		// =====================================================================
		/// Une entree. `action` 0 = aucune (un intitule) ; `separateur` = un trait.
		struct NkFamilleEntreeMenu {
				NkString libelle;
				const char *raccourci = "";
				int32 action = 0;
				bool coche = false;
				bool actif = true;
				bool separateur = false;
				/// Le survol de l'entree ouvre CE menu a sa droite (« Ajouter ▸ »).
				int32 sousMenu = -1;
		};

		inline NkFamilleEntreeMenu NkFamilleLigneMenu(const char *libelle, int32 action, const char *raccourci = "",
												   bool coche = false, bool actif = true) {
			NkFamilleEntreeMenu e;
			e.libelle = NkString(libelle);
			e.action = action;
			e.raccourci = raccourci;
			e.coche = coche;
			e.actif = actif;
			return e;
		}
		inline NkFamilleEntreeMenu NkFamilleSeparateur() {
			NkFamilleEntreeMenu e;
			e.separateur = true;
			return e;
		}
		inline NkFamilleEntreeMenu NkFamilleIntitule(const char *libelle) {
			NkFamilleEntreeMenu e;
			e.libelle = NkString(libelle);
			e.actif = false;
			return e;
		}
		inline NkFamilleEntreeMenu NkFamilleSousMenu(const char *libelle, int32 sous) {
			NkFamilleEntreeMenu e;
			e.libelle = NkString(libelle);
			e.sousMenu = sous;
			return e;
		}

		/// L'etat du menu ouvert (un seul a la fois, et un sous-menu).
		struct NkFamilleMenus {
				int32 menu = -1;
				nkgui::NkRect ancre{0.f, 0.f, 0.f, 0.f};
				/// Le rectangle du menu, garde d'une trame a l'autre : c'est lui qui
				/// decide, AVANT le dessin du corps, si la souris est dessus.
				nkgui::NkRect rect{0.f, 0.f, 0.f, 0.f};
				int32 sousMenu = -1;
				nkgui::NkRect sousLigne{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect sousRect{0.f, 0.f, 0.f, 0.f};

				bool Ouvert() const noexcept {
					return menu >= 0;
				}
				void Fermer() noexcept {
					menu = -1;
					sousMenu = -1;
				}
				/// Ouvre `m` sous `ancre` ; le meme deja ouvert se referme.
				void Ouvrir(int32 m, const nkgui::NkRect &a) noexcept {
					if (menu == m) {
						Fermer();
						return;
					}
					menu = m;
					ancre = a;
					sousMenu = -1;
				}
				/// La souris est-elle sur le menu ou son sous-menu ?
				bool Contient(const nkgui::NkVec2 &p) const noexcept {
					return menu >= 0 && (NkFamilleDans(rect, p) || (sousMenu >= 0 && NkFamilleDans(sousRect, p)));
				}
		};

		/// Remplit les entrees du menu `menu` (l'application les connait).
		using NkFamilleRemplirMenuFn = void (*)(void *user, int32 menu, NkVector<NkFamilleEntreeMenu> &sortie);

		/// Peint le menu ouvert et son sous-menu (dans la SURCOUCHE : par-dessus
		/// tout). Rend l'action choisie (0 = rien) -- le menu est alors ferme. Un
		/// clic hors du menu le ferme, sauf s'il vient de s'ouvrir sur ce clic
		/// (`menuDebut` : le menu ouvert au debut de la trame).
		int32 NkFamilleDessinerMenu(NkFamilleCtx &c, const nkgui::NkRect &ecran, NkFamilleMenus &menus, int32 menuDebut,
									NkFamilleRemplirMenuFn remplir, void *user);

		// =====================================================================
		// LES GESTES SOUS UN MENU
		// =====================================================================
		/// Les gestes de la souris d'une trame, mis de cote pendant que le corps
		/// se dessine sous un menu ouvert (le corps ne doit pas recevoir le clic
		/// qui ferme le menu), puis rendus pour les menus.
		struct NkFamilleGestes {
				nkgui::NkVec2 position{0.f, 0.f};
				bool bas[3] = {};
				bool clic[3] = {};
				bool relache[3] = {};
				bool double_[3] = {};
				float32 molette = 0.f;
				float32 moletteH = 0.f;
		};
		NkFamilleGestes NkFamilleSauverGestes(const nkgui::NkGuiInput &in) noexcept;
		void NkFamilleRendreGestes(nkgui::NkGuiInput &in, const NkFamilleGestes &g) noexcept;
		/// Neutralise les gestes ; la position aussi si la souris est SUR le menu
		/// (rien dessous ne doit se croire survole).
		void NkFamilleNeutraliserGestes(nkgui::NkGuiInput &in, bool surLeMenu) noexcept;

	} // namespace editorkit
} // namespace nkentseu
