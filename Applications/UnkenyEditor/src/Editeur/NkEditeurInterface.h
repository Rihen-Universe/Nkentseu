//
// NkEditeurInterface.h
// =============================================================================
// Description :
//   L'etat de l'INTERFACE de l'editeur (colonnes, menu ouvert, onglets, modeles
//   de l'Outliner et du tiroir) et les fonctions qui la peignent. L'etat de la
//   SCENE, lui, reste dans NkEditeurModele : ce fichier ne sait rien faire,
//   il appelle NkEditeurActions.h.
//
// Caracteristiques :
//   - Disposition UE5 adaptee a la 2D (UI_SPEC de NK3DModeler, §2.2) :
//       menus + onglet de scene / barre d'outils / Outliner a gauche /
//       barre de vue + viseur au centre / Details | Monde a droite /
//       tiroir Acteurs | Journal en bas / barre d'etat.
//   - PEINTE PAR L'APPLICATION, comme NK3DModeler : plus de NkEditorShell,
//     donc plus de barres d'activite ni d'onglets lateraux imposes. On ne
//     prend dans NKEditorKit que des PIECES : NkTheme::Dark(), l'arbre
//     (NkDrawTreeView), le navigateur de contenu (NkDrawContentBrowser), le
//     champ de saisie superpose et la barre de defilement.
//   - UNE table d'actions (NkActionEditeur) : menus, barre d'outils et
//     raccourcis clavier passent tous par NkEditeurExecuter. Deux chemins
//     vers la meme commande finissent toujours par diverger.
//
// ⚠️ LES MENUS DEROULANTS SONT DESSINES ICI, PAS PAR NKGUI
//   Sous Xvfb, aucun element d'un popup NKGui ouvert dans le shell ne recevait
//   le clic (mesure du 2026-09-29, cf. l'ancien NkEditeurPanneaux.cpp). Un
//   menu peint dans `dlOverlay`, dont on calcule soi-meme le rectangle, n'a
//   pas de couche a franchir. Et pendant qu'il est ouvert, les gestes du corps
//   sont NEUTRALISES : un clic destine a « Fichier > Ouvrir » ne doit pas
//   atteindre le viseur qui est dessous.
//
// Fichiers :
//   NkEditeurChrome.cpp    menus, barre d'outils, barre d'etat, cloisons
//   NkEditeurOutliner.cpp  l'Outliner (arbre du kit)
//   NkEditeurDetails.cpp   Details (composants) et Monde (la scene)
//   NkEditeurTiroir.cpp    le tiroir : acteurs du catalogue, journal
//   NkEditeurVue.cpp       la barre de vue, le viseur et sa souris
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURINTERFACE_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURINTERFACE_H__

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurModele.h"

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKEditorKit/Components/NkComponentInstance.h"
#include "NKEditorKit/Components/NkComponentPaint.h"
#include "NKEditorKit/Components/NkContentBrowserModel.h"
#include "NKEditorKit/Components/NkTreeViewModel.h"
#include "NKEditorKit/NkTheme.h"
#include "NKGui/Core/NkGuiContext.h"

namespace nkentseu {
	namespace editeur {

		/// Les menus deroulants : ceux de la barre de menus, puis ceux de la
		/// barre d'outils, puis celui de « + Ajouter » des Details.
		enum class NkMenuEditeur : int32 {
			NK_AUCUN = -1,
			NK_FICHIER = 0,
			NK_EDITION,
			NK_FENETRE,
			NK_AIDE,
			NK_OUTIL,
			NK_AJOUTER,
			NK_APPAREIL,
			NK_REGLAGES,
			NK_COMPOSANT,
			NK_CTX_ENTITE,	 ///< clic droit sur une entite
			NK_CTX_VIDE,	 ///< clic droit dans le vide
			NK_AJOUTER_ICI	 ///< sous-menu « Ajouter ici » : pose au point du clic droit
		};

		/// LA table des actions. Les plages a partir de 100 portent un indice
		/// (outil, acteur, profil...) ajoute a leur base.
		enum NkActionEditeur : int32 {
			NK_A_AUCUNE = 0,
			NK_A_NOUVEAU,
			NK_A_OUVRIR,
			NK_A_ENREGISTRER,
			NK_A_QUITTER,
			NK_A_NOUVELLE_ENTITE,
			NK_A_DUPLIQUER,
			NK_A_SUPPRIMER,
			NK_A_CADRER,
			NK_A_JOUER,
			NK_A_PAUSE,
			NK_A_ARRETER,
			NK_A_PAS,
			NK_A_GRILLE,
			NK_A_COLLISIONNEURS,
			NK_A_LIENS,
			NK_A_PARTICULES,
			NK_A_VITESSES,
			NK_A_PAYSAGE,
			NK_A_VOIR_OUTLINER,
			NK_A_VOIR_DETAILS,
			NK_A_VOIR_TIROIR,
			NK_A_DISPOSITION,
			NK_A_APROPOS,
			NK_A_RACCOURCIS,
			NK_A_ARMER_SIMPLE,			///< arme « Poser » sur l'entite simple (sprite + boite)
			NK_A_FERMER_SCENE,			///< la ✕ de l'onglet de scene
			NK_A_RENOMMER,				///< le champ Nom des Details, focalise, texte choisi
			NK_A_CADRER_SELECTION,		///< la vue se centre sur la selection
			NK_A_ENTITE_ICI,			///< une entite vide au point du clic droit
			NK_A_SIMPLE_ICI,			///< l'entite simple (sprite + boite) au point du clic droit
			NK_A_ACCROCHAGE,			///< l'accrochage des gizmos, allume / eteint
			NK_A_POSER_ICI = 700,		///< + NkActeurSim : pose au point du clic droit
			NK_A_OUTIL = 100,			///< + NkOutil
			NK_A_POSER_ACTEUR = 200,	///< + NkActeurSim : pose au centre de la vue
			NK_A_APPAREIL = 300,		///< + indice de profil
			NK_A_MODE_RENDU = 400,		///< + NkModeRenduParticules
			NK_A_COMPOSANT = 500,		///< + NkComposantEditeur, sur la selection
			NK_A_CORPS_MOU = 600,		///< + NkActeurSim : la matiere du corps mou ajoute
			// 2026-09-30 (NkEditeurLumiere.h) : quatre plages de moins de 10 valeurs.
			NK_A_LUMIERE = 800,			///< + NkTypeLumiere2D : une lumiere sur la selection
			NK_A_EMETTEUR = 850,		///< + NkPresetEffet2D : un emetteur sur la selection
			NK_A_LUMIERE_ICI = 900,		///< + NkTypeLumiere2D : une lumiere au point du clic droit
			NK_A_EMETTEUR_ICI = 950		///< + NkPresetEffet2D : un effet au point du clic droit
		};

		/// Une ligne de menu. `separateur` = un trait, rien d'autre n'est lu.
		struct NkEntreeMenu {
				NkString libelle;
				const char *raccourci = "";
				int32 action = NK_A_AUCUNE;
				bool coche = false;
				bool actif = true;
				bool separateur = false;
				/// Le survol de l'entree ouvre CE menu a sa droite (« Ajouter ▸ »).
				NkMenuEditeur sousMenu = NkMenuEditeur::NK_AUCUN;
		};

		/// Les couleurs du chrome, LUES dans le theme du kit : aucune n'est en dur.
		/// C'est ce qui fera suivre un theme clair sans retoucher une ligne.
		struct NkPaletteEditeur {
				nkgui::NkColor fond;		///< WindowBg  #141414
				nkgui::NkColor panneau;		///< PanelBg   #212121
				nkgui::NkColor entete;		///< PanelHeader #2B2B2B
				nkgui::NkColor bord;
				nkgui::NkColor champ;		///< InputBg
				nkgui::NkColor bouton;
				nkgui::NkColor boutonSurvol;
				nkgui::NkColor texte;
				nkgui::NkColor attenue;
				nkgui::NkColor accent;		///< BLEU : l'etat de l'interface
				nkgui::NkColor selection;	///< AMBRE : la selection dans la scene
				nkgui::NkColor surAccent;
		};

		/// L'etat de l'interface. Tout ce qui n'est pas la scene.
		struct NkEditeurInterface {
				// --- Le plan, recalcule a chaque trame (NkEditeurPlanifier) -----
				nkgui::NkRect ecran{0.f, 0.f, 0.f, 0.f};
				/// La BARRE DE TITRE, peinte par l'editeur (fenetre sans cadre) :
				/// menus a gauche, titre au centre, reduire / agrandir / fermer a droite.
				nkgui::NkRect barreMenus{0.f, 0.f, 0.f, 0.f};
				/// Les onglets de scene, JUSTE SOUS la barre de titre.
				nkgui::NkRect barreOnglets{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect barreOutils{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect outliner{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect vue{0.f, 0.f, 0.f, 0.f};		 ///< la colonne centrale entiere
				nkgui::NkRect barreVue{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect viseur{0.f, 0.f, 0.f, 0.f};	 ///< la vue sous sa barre
				nkgui::NkRect details{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect tiroir{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect statut{0.f, 0.f, 0.f, 0.f};

				// --- Les colonnes -----------------------------------------------
				float32 largeurOutliner = 250.f;
				float32 largeurDetails = 340.f;
				float32 hauteurTiroir = 250.f;
				/// La cloison tenue : 0 Outliner|vue, 1 vue|Details, 2 corps|tiroir.
				int32 cloisonTenue = -1;
				bool voirOutliner = true;
				bool voirDetails = true;
				bool voirTiroir = true;

				// --- Le menu ouvert ---------------------------------------------
				NkMenuEditeur menu = NkMenuEditeur::NK_AUCUN;
				nkgui::NkRect menuAncre{0.f, 0.f, 0.f, 0.f};
				/// Le rectangle du menu, gardé d'une trame a l'autre : c'est lui
				/// qui decide, AVANT le dessin du corps, si la souris est dessus.
				nkgui::NkRect menuRect{0.f, 0.f, 0.f, 0.f};
				/// Le sous-menu ouvert depuis une entree du menu (un seul niveau).
				NkMenuEditeur sousMenu = NkMenuEditeur::NK_AUCUN;
				nkgui::NkRect sousMenuLigne{0.f, 0.f, 0.f, 0.f}; ///< l'entree qui l'a ouvert
				nkgui::NkRect sousMenuRect{0.f, 0.f, 0.f, 0.f};
				/// Le point du MONDE ou le clic droit a eu lieu : « Ajouter ici » y pose.
				NkVec2f pointContexte{0.f, 0.f};

				// --- Les onglets ------------------------------------------------
				int32 ongletDroite = 0; ///< 0 Details, 1 Monde
				int32 ongletTiroir = 0; ///< 0 Acteurs, 1 Journal

				// --- L'Outliner -------------------------------------------------
				editorkit::NkTreeViewModel arbre;
				editorkit::NkComponentInstance arbreReglages;
				bool arbrePret = false;
				/// L'entite de chaque noeud, par indice de noeud (-1 : la racine).
				NkVector<ecs::NkEntityId> arbreEntites;
				bool filtreFocus = false;

				// --- Le tiroir --------------------------------------------------
				editorkit::NkContentBrowserModel contenu;
				editorkit::NkComponentInstance contenuReglages;
				bool contenuPret = false;
				int32 categorie = -1; ///< -1 = toutes les categories
				NkVector<NkString> journal;
				float32 agePrecedent = 99.f;
				float32 defilJournal = 0.f;

				// --- Les Details ------------------------------------------------
				ecs::NkEntityId nomDe; ///< l'entite dont `nom` est le tampon
				char nom[32] = {};
				bool nomFocus = false;
				/// « Renommer » (menu, F2) : le champ prend le focus, texte choisi,
				/// A LA TRAME OU IL SE DESSINE -- la selection vient peut-etre de
				/// changer, et le champ remet son focus a zero quand elle change.
				bool renommerDemande = false;
				float32 defilDetails = 0.f;

				// --- Le viseur --------------------------------------------------
				NkVec2f precMonde{0.f, 0.f}; ///< point precedent du couteau et du pinceau
				bool geste = false;			 ///< saisie, coupe ou pinceau en cours
				/// Le clic gauche a touche une entite : le glisser la deplacera au-dela
				/// du seuil. En deca, c'etait un clic -- il a deja selectionne.
				bool glisserArme = false;
				NkVec2f appuiEcran{0.f, 0.f};

				// --- Les gizmos --------------------------------------------------
				/// La poignee tenue : 0 aucune, 1 axe X, 2 axe Y, 3 plan XY (deplacer)
				/// ou uniforme (echelle), 4 anneau (tourner).
				int32 gizmoTenu = 0;
				int32 gizmoSurvol = 0;	 ///< la poignee sous le curseur (pour la peindre)
				NkVec2f gizmoCentre0{0.f, 0.f}; ///< le centre de l'entite a la saisie
				NkVec2f gizmoMonde0{0.f, 0.f};	///< le point saisi, en metres
				NkVec2f gizmoEcran0{0.f, 0.f};	///< le point saisi, en pixels
				float32 gizmoAngle = 0.f;		///< rotation deja appliquee (rad)
				NkVec2f gizmoEchelle{1.f, 1.f};	///< facteur deja applique
				/// L'accrochage : ACTIF par defaut, Ctrl l'inverse le temps du geste.
				bool accrochage = true;
				float32 pasGrille = 0.25f;	 ///< metres
				float32 pasAngle = 15.f;	 ///< degres
				float32 pasEchelle = 0.1f;	 ///< facteur

				/// Le cadrage anime (F, double-clic de l'Outliner) : la camera va de
				/// son etat courant a la cible en `cadrageDuree` secondes.
				bool cadrageAnime = false;
				float32 cadrageT = 0.f;
				NkVec2f cadrageCentre0{0.f, 0.f};
				float32 cadrageZoom0 = 1.f;
				NkVec2f cadrageCentre1{0.f, 0.f};
				float32 cadrageZoom1 = 1.f;
				/// La zone du cadrage demande, en metres (0 = rien a cadrer). Le zoom
				/// cible se calcule A LA TRAME DU VISEUR, qui seule connait sa taille.
				NkVec2f cadrageZone{0.f, 0.f};
				bool cadrageDemande = false;
				/// Cadrer demande la taille du viseur : on cadre a la PREMIERE trame
				/// ou il l'a, sinon le zoom sort d'un viseur de 1 x 1.
				bool cadrageEnAttente = true;

				// --- Les modifications non enregistrees -------------------------
				/// L'empreinte de la scene au dernier enregistrement (ou ouverture,
				/// ou nouvelle scene). « Modifiee » = l'empreinte courante differe.
				/// ⚠️ UNE EMPREINTE, PAS UN DRAPEAU POSE PAR CHAQUE GESTE : les
				///    Details ecrivent directement dans les composants, et un drapeau
				///    oublie par UN curseur suffirait a perdre un travail sans
				///    prevenir. La scene elle-meme ne peut pas oublier.
				uint64 empreinteEnregistree = 0u;
				bool modifiee = false;
				float32 ageEmpreinte = 0.f;
				/// L'action qui attend la reponse de la boite « non enregistree »
				/// (fermer, nouvelle scene, ouvrir, quitter). NK_A_AUCUNE = pas de boite.
				int32 confirmation = NK_A_AUCUNE;
				/// La croix de la FENETRE a ete cliquee : la question se pose a la
				/// prochaine trame (OnCloseRequested ne dessine pas).
				bool fermetureDemandee = false;

				// --- La fenetre (barre de titre maison) -------------------------
				// ⚠️ DES DEMANDES, PAS DES APPELS. BeginDragMove et BeginResize
				//    entrent dans une boucle MODALE de l'OS : les appeler pendant le
				//    dessin reentrerait dans la trame. L'application les consomme au
				//    debut de la trame suivante (OnTick), comme NK3DModeler les
				//    consomme apres l'envoi de l'image et NkEditorShell en fin de Run.
				bool reduireDemande = false;
				bool agrandirDemande = false; ///< agrandir, ou restaurer si agrandie
				bool deplacerDemande = false;
				float32 deplacerFractionX = 0.5f; ///< ou la barre a ete saisie (0..1)
				int32 redimDemande = -1;		   ///< NkWindow::NkResizeEdge, -1 = aucun
				/// La barre de titre est tenue sans avoir encore bouge : le
				/// deplacement ne part qu'au-dela d'un seuil, sinon un double-clic
				/// (agrandir) deviendrait un deplacement de zero pixel.
				bool titreArme = false;
				nkgui::NkVec2 titreAppui{0.f, 0.f};
				bool fenetreAgrandie = false; ///< relu a chaque trame (icone du bouton)

				// --- Divers -----------------------------------------------------
				float32 ips = 0.f;
				float32 temps = 0.f;			///< secondes depuis le lancement (journal)
				float32 dt = 1.f / 60.f;		///< le pas de la trame (cadrage anime)
				NkString derniereAnnonce;		///< la derniere recopiee au journal
				bool demandeQuitter = false;
				/// L'ordre de l'Outliner, garde d'une trame a l'autre : l'ECS range ses
				/// entites par archetype, et ajouter un composant en deplacait une.
				NkVector<ecs::NkEntityId> ordreArbre;
		};

		/// Ce qu'une fonction de dessin recoit. Rien ne s'y recalcule.
		struct NkEditeurCadre {
				nkgui::NkGuiContext &ctx;
				NkEditeurModele &m;
				NkEditeurInterface &ui;
				const editorkit::NkTheme &theme;
				const NkPaletteEditeur &pal;
				nkgui::NkGuiFont *police;
				nkgui::NkGuiFont *petite;
		};

		// --- Outils communs (NkEditeurChrome.cpp) -----------------------------
		NkPaletteEditeur NkEditeurPalette(const editorkit::NkTheme &theme);
		/// Place toutes les zones. Aucune n'est calculee ailleurs.
		void NkEditeurPlanifier(NkEditeurInterface &ui, float32 largeur, float32 hauteur);
		/// Execute une action de la table. Le seul endroit ou une commande agit.
		void NkEditeurExecuter(NkEditeurCadre &c, int32 action);
		/// Ouvre un menu deroulant sous `ancre` (le ferme s'il etait deja ouvert).
		void NkEditeurOuvrirMenu(NkEditeurCadre &c, NkMenuEditeur menu, const nkgui::NkRect &ancre);
		/// Un bouton PLAT (UI_SPEC §3.3) : bordure discrete, rempli au survol,
		/// bleu plein quand `enfonce`. Rend true au clic.
		/// `dl` : la liste ou peindre (nul = la couche principale ; la boite de
		/// confirmation passe `dlOverlay`).
		bool NkEditeurBouton(NkEditeurCadre &c, const nkgui::NkRect &r, const char *texte, bool enfonce = false,
							 bool actif = true, nkgui::NkGuiDrawList *dl = nullptr);
		/// Une bande d'onglets de panneau. Rend true si l'onglet actif a change.
		bool NkEditeurOnglets(NkEditeurCadre &c, const nkgui::NkRect &bande, const char *const *noms, int32 n,
							  int32 &actif);
		/// L'entree NKGui traduite pour les composants du kit.
		editorkit::NkComponentInput NkEditeurEntreeComposant(const nkgui::NkGuiContext &ctx);
		bool NkEditeurDans(const nkgui::NkRect &r, const nkgui::NkVec2 &p) noexcept;

		// --- Le chrome (NkEditeurChrome.cpp) ----------------------------------
		/// Les bords de la fenetre sans cadre : curseur, et demande de
		/// redimensionnement. Appele EN PREMIER, avec l'entree reelle : il
		/// consomme le clic qui tombe sur un bord.
		void NkEditeurBordsFenetre(NkEditeurCadre &c);
		/// La barre de titre : menus, titre, boutons de fenetre, zone de saisie.
		void NkEditeurDessinerBarreMenus(NkEditeurCadre &c);
		/// Les onglets de scene, sous la barre de titre : [ ● Scene_01  ✕ ].
		void NkEditeurDessinerOnglets(NkEditeurCadre &c);
		/// Recopie au journal l'annonce du modele si elle est neuve. Appelee apres
		/// chaque action qui peut annoncer DEUX choses dans la meme trame
		/// (« enregistree » puis « fermee »), et en fin de trame.
		void NkEditeurJournaliser(NkEditeurModele &m, NkEditeurInterface &ui);
		void NkEditeurDessinerBarreOutils(NkEditeurCadre &c);
		void NkEditeurDessinerStatut(NkEditeurCadre &c);
		/// Les cloisons entre colonnes : saisir, glisser, relacher.
		void NkEditeurCloisons(NkEditeurCadre &c);
		/// Le menu ouvert, dans `dlOverlay`. `menuDebut` est le menu ouvert au
		/// debut de la trame : un clic hors de LUI le ferme, un menu qui vient
		/// de s'ouvrir sur ce clic reste ouvert.
		void NkEditeurDessinerMenuOuvert(NkEditeurCadre &c, NkMenuEditeur menuDebut);

		// --- Les modifications non enregistrees (NkEditeurChrome.cpp) ---------
		/// L'empreinte de la scene : FNV-1a 64 de sa forme JSON, celle-la meme
		/// qu'ecrit l'enregistrement. 0 si la scene ne se serialise pas.
		uint64 NkEditeurEmpreinte(NkEditeurModele &m);
		/// La scene courante devient la reference « enregistree ».
		void NkEditeurRetenirEmpreinte(NkEditeurModele &m, NkEditeurInterface &ui);
		/// Relit l'empreinte au plus une fois par seconde, et seulement en
		/// EDITION : en jeu, la scene change a chaque pas et « Arreter » la rend.
		void NkEditeurSuivreModifications(NkEditeurModele &m, NkEditeurInterface &ui, float32 dt);
		/// La boite « modifications non enregistrees », dans dlOverlay. Rien si
		/// aucune action n'attend de reponse.
		void NkEditeurDessinerConfirmation(NkEditeurCadre &c);

		// --- Les panneaux -----------------------------------------------------
		void NkEditeurDessinerOutliner(NkEditeurCadre &c);
		void NkEditeurDessinerDetails(NkEditeurCadre &c);
		void NkEditeurDessinerTiroir(NkEditeurCadre &c);
		void NkEditeurDessinerVue(NkEditeurCadre &c);
		/// Mene la camera, EN DOUCEUR, sur la selection -- ou sur toute la scene
		/// si `toutLaScene` ou si rien n'est selectionne (NkEditeurZoneACadrer).
		/// Meme si la selection est hors du cadre : c'est tout l'interet.
		void NkEditeurDemanderCadrage(NkEditeurCadre &c, bool toutLaScene);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURINTERFACE_H__
