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
#include "Editeur/NkEditeurContenu.h"
#include "Editeur/NkEditeurModele.h"

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKEditorKit/Components/NkComponentInstance.h"
#include "NKEditorKit/Components/NkComponentPaint.h"
#include "NKEditorKit/Components/NkContentBrowserDisque.h"
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
			NK_AJOUTER_ICI,	 ///< sous-menu « Ajouter ici » : pose au point du clic droit
			// La barre flottante du viseur (2026-09-30) : les pas d'accrochage.
			NK_PAS_GRILLE,
			NK_PAS_ANGLE,
			NK_PAS_ECHELLE,
			NK_CARTE, ///< le menu « ⋮ » d'une carte de l'inspecteur (2026-09-30)
			// Le clic droit hors d'une entite (2026-09-30, lot 1) : AJOUTES A LA FIN.
			NK_CTX_ARBRE,		///< l'Outliner hors d'une ligne d'entite (la racine, le vide)
			NK_CTX_CONTENU,		///< une carte ou un dossier du navigateur de contenu
			NK_CTX_CONTENU_VIDE, ///< le fond du navigateur de contenu
			// Le navigateur a la maniere d'UE5 (2026-10-01, document 02 §3) : AJOUTES A LA FIN.
			NK_CONTENU_AJOUTER,	   ///< « + Ajouter » : creer un dossier, une scene, un prefab...
			NK_CONTENU_REGLAGES,   ///< « Reglages » : taille des vignettes, dossiers, liste...
			NK_CONTENU_TRI,		   ///< le bouton de tri : nom, type, date, taille ; sens
			NK_CONTENU_DEPOSER,	   ///< apres un glisser : « Deplacer ici / Copier ici »
			NK_CONTENU_COULEUR,	   ///< sous-menu « Couleur du dossier »
			NK_CONTENU_COLLECTION, ///< sous-menu « Ajouter a la collection »
			NK_CTX_COLLECTION	   ///< clic droit sur une collection
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
			NK_A_ENTREES,				///< le panneau Entrees (liaisons du jeu), ouvert / ferme
			NK_A_CREER_PREFAB,			///< un prefab de la selection (2026-09-29)
			NK_A_DETACHER,				///< la selection devient une racine, a sa place
			NK_A_ANNULER,				///< Ctrl+Z (2026-10-01, NkHistoriqueEditeur)
			NK_A_REFAIRE,				///< Ctrl+Y / Ctrl+Maj+Z
			NK_A_POSER_ICI = 700,		///< + NkActeurSim : pose au point du clic droit
			NK_A_OUTIL = 100,			///< + NkOutil
			NK_A_POSER_ACTEUR = 200,	///< + NkActeurSim : pose au centre de la vue
			NK_A_APPAREIL = 300,		///< + indice de profil
			NK_A_MODE_RENDU = 400,		///< + NkModeRenduParticules
			NK_A_COMPOSANT = 500,		///< + NkComposantEditeur, sur la selection
			NK_A_CORPS_MOU = 600,		///< + NkActeurSim : la matiere du corps mou ajoute
			NK_A_CONSTRUIRE = 900,		///< Fichier > Construire… (U5, Livraison/)
			// 2026-09-30 (NkEditeurLumiere.h) : quatre plages de moins de 10 valeurs,
			// AU-DESSUS de 999. Les numeros 800-999 sont laisses libres : la branche
			// de livraison y a pris NK_A_CONSTRUIRE = 900 (comble/livrer-u5), et deux
			// chantiers qui se partagent une centaine finissent par s'y rencontrer.
			NK_A_LUMIERE = 1000,		///< + NkTypeLumiere2D : une lumiere sur la selection
			NK_A_EMETTEUR = 1050,		///< + NkPresetEffet2D : un emetteur sur la selection
			NK_A_LUMIERE_ICI = 1100,	///< + NkTypeLumiere2D : une lumiere au point du clic droit
			NK_A_EMETTEUR_ICI = 1150,	///< + NkPresetEffet2D : un effet au point du clic droit
			// Les appareils simules (2026-10-01, NkEditeurAppareils.h) : 1400-1499.
			NK_A_ORIENTATION = 1400,	///< + NkOrientation
			NK_A_OPTION_APPAREIL = 1410, ///< + NkOptionAppareil (cadre, zone sure, decoupe...)
			NK_A_REGLE_CAMERA = 1440,	///< + NkRegleCamera (camera du jeu selon l'ecran)
			// La barre flottante du viseur (2026-09-30).
			NK_A_ACCROCHE_GRILLE = 1200, ///< l'accrochage des DEPLACEMENTS, allume / eteint
			NK_A_ACCROCHE_ANGLE,		///< celui des ROTATIONS
			NK_A_ACCROCHE_ECHELLE,		///< celui des ECHELLES
			NK_A_REPERE_LOCAL,			///< le gizmo Deplacer : axes de l'entite / du monde
			NK_A_PAS_GRILLE = 1220,		///< + indice dans NkPasGrille
			NK_A_PAS_ANGLE = 1240,		///< + indice dans NkPasAngle
			NK_A_PAS_ECHELLE = 1260,		///< + indice dans NkPasEchelle
			// Le menu « ⋮ » d'une carte (NkEditeurInterface::carteMenu).
			NK_A_CARTE_REINIT = 1280,
			NK_A_CARTE_RETIRER,
			NK_A_CARTE_MONTER,
			NK_A_CARTE_DESCENDRE,
			NK_A_CARTE_COPIER,
			NK_A_CARTE_COLLER,
			// Le menu du navigateur de contenu (2026-09-30, lot 1) : il vise
			// l'element du clic droit (NkEditeurInterface::contenuMenuChemin).
			NK_A_CONTENU_POSER = 1300, ///< l'acteur vise, pose au centre de la vue
			NK_A_CONTENU_ARMER,		   ///< l'acteur vise arme « Poser » (comme un clic sur sa carte)
			NK_A_CONTENU_OUVRIR,	   ///< le dossier vise s'ouvre
			NK_A_CONTENU_RACINE,	   ///< retour a la racine du navigateur (ou de « Contenu »)
			NK_A_CONTENU_IMPORTER,	   ///< « Importer… » : le dialogue, puis la copie dans le Contenu
			NK_A_CONTENU_EXPORTER,	   ///< « Exporter… » : les assets choisis, vers un dossier de l'OS
			// Le navigateur a la maniere d'UE5 (2026-10-01, document 02 §3.1) : les
			// gestes sur les fichiers ET les dossiers (NkContentBrowserDisque.h du kit).
			NK_A_CONTENU_NOUVEAU_DOSSIER = 1306, ///< « Nouveau dossier » (dans le dossier vise ou courant)
			NK_A_CONTENU_NOUVELLE_SCENE,		 ///< une scene vide (.nkscene)
			NK_A_CONTENU_NOUVEAU_PREFAB,		 ///< un prefab de la selection de la scene
			NK_A_CONTENU_NOUVEAU_CONTROLEUR,	 ///< un controleur d'animation (.nkanimctl)
			NK_A_CONTENU_COUPER,				 ///< Ctrl+X
			NK_A_CONTENU_COPIER,				 ///< Ctrl+C
			NK_A_CONTENU_COLLER,				 ///< Ctrl+V
			NK_A_CONTENU_DUPLIQUER,				 ///< Ctrl+D
			NK_A_CONTENU_RENOMMER,				 ///< F2 : le nom s'edite EN PLACE
			NK_A_CONTENU_SUPPRIMER,				 ///< Suppr : la CONFIRMATION s'ouvre
			NK_A_CONTENU_SUPPRIMER_OUI,			 ///< la confirmation acceptee : vers la corbeille
			NK_A_CONTENU_DEPLACER_ICI,			 ///< le menu du glisser
			NK_A_CONTENU_COPIER_ICI,			 ///< le menu du glisser
			NK_A_CONTENU_FAVORI,				 ///< le dossier vise entre aux / sort des Favoris
			NK_A_CONTENU_TOUT_SELECTIONNER,		 ///< Ctrl+A dans le navigateur
			NK_A_CONTENU_COPIER_CHEMIN,			 ///< le chemin dans le presse-papiers
			NK_A_CONTENU_OUVRIR_ASSET,			 ///< double-clic : une scene s'ouvre, un dossier aussi
			NK_A_CONTENU_POSER_ASSET,			 ///< un prefab ou une image, au centre de la vue
			NK_A_CONTENU_AFFICHER_DOSSIERS,		 ///< reglage « Afficher les dossiers »
			NK_A_CONTENU_FILTRES,				 ///< reglage « Filtres par type »
			NK_A_CONTENU_VUE_LISTE,				 ///< reglage « Vue en liste »
			NK_A_CONTENU_NOUVELLE_COLLECTION,	 ///< le « + » des Collections
			NK_A_CONTENU_SUPPRIMER_COLLECTION,	 ///< clic droit sur une collection
			NK_A_CONTENU_RETIRER_COLLECTION,	 ///< l'asset sort de la collection regardee
			NK_A_CONTENU_TRI_SENS,				 ///< croissant / decroissant
			NK_A_CONTENU_COULEUR = 1340,		 ///< + indice de NkCouleursDossier (0 = celle par defaut)
			NK_A_CONTENU_TAILLE = 1352,			 ///< + indice de taille (petite, moyenne, grande, enorme)
			NK_A_CONTENU_TRI = 1360,			 ///< + NkBrowserTri
			NK_A_CONTENU_COLLECTION = 1370		 ///< + indice de collection : la selection y entre
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
				int32 ongletTiroir = 0; ///< 0 Acteurs, 1 Journal, 2 Terminal (NkEditeurTerminal.h)

				// --- L'Outliner -------------------------------------------------
				editorkit::NkTreeViewModel arbre;
				editorkit::NkComponentInstance arbreReglages;
				bool arbrePret = false;
				/// L'entite de chaque noeud, par indice de noeud (-1 : la racine).
				NkVector<ecs::NkEntityId> arbreEntites;
				bool filtreFocus = false;
				/// « Renommer » (F2, menus) : la saisie s'ouvre EN PLACE, sur la
				/// ligne de la selection, a la trame ou l'Outliner se dessine.
				bool renommerEnPlace = false;
				/// Le clic LENT (a la maniere d'UE5) : un clic sur le nom d'une
				/// ligne DEJA choisie arme le renommage ; il part si aucun second
				/// clic (le double-clic cadre) ni glisser ne suit dans la demi-seconde.
				nk_uint64 clicLentNoeud = 0;
				float32 clicLentAge = 0.f;
				NkVec2f clicLentPos{0.f, 0.f};

				// --- Le tiroir --------------------------------------------------
				editorkit::NkContentBrowserModel contenu;
				editorkit::NkComponentInstance contenuReglages;
				bool contenuPret = false;
				int32 categorie = -1; ///< -1 = toutes les categories
				/// Le clic droit du navigateur (2026-09-30) : le CHEMIN de ce qu'il visait
				/// (« acteur:7 », « simple », un dossier ; vide = le fond) et son nom. Un
				/// chemin, pas un indice : les cartes sont reconstruites a chaque trame.
				NkString contenuMenuChemin;
				NkString contenuMenuNom;
				bool contenuMenuDossier = false;
				/// Le rectangle de chaque carte A L'ECRAN a la derniere trame, par indice
				/// d'entree (vide = hors champ), releve par le crochet `cardOverlay` du kit :
				/// seul le composant connait sa grille. Le banc y vise ses clics.
				NkVector<nkgui::NkRect> contenuCartes;
				/// (2026-09-30, lot 1) LE CONTENU DU PROJET (NkEditeurContenu.h) : un
				/// second dossier du rail, « Contenu », a cote du catalogue « Acteurs ».
				bool contenuProjet = false;	   ///< le navigateur montre le Contenu, pas le catalogue
				NkString contenuDossier;	   ///< le dossier courant, RELATIF a Contenu (« » = sa racine)
				/// Les assets CHOISIS (chemins du navigateur, Ctrl+clic) : ce qu'exporte
				/// « Exporter… ». Des chemins : les cartes sont reconstruites a chaque trame.
				NkVector<NkString> contenuChoisis;
				/// Le dossier courant et le rail, relus du DISQUE au plus une fois par
				/// seconde, et aussitot apres un import ou un changement de dossier.
				NkVector<NkElementContenu> contenuListe;
				NkVector<NkString> contenuSousDossiers;
				NkString contenuListeDe;
				float32 contenuListeAge = 99.f;
				bool contenuPerime = true;
				/// Les puces de filtre de L'AUTRE section (natures du Contenu, ou
				/// categories du catalogue) : echangees quand la section change.
				NkVector<editorkit::NkBrowserKind> pucesAutres;
				bool pucesContenu = false;
				/// « Importer… » / « Exporter… » : des DEMANDES, consommees au debut de
				/// la trame suivante, qui ouvre LE selecteur de fichiers de l'editeur
				/// (NkEditeurSelecteur.h, celui de NKEditorKit) -- modal.
				bool importDemande = false;
				bool exportDemande = false;
				NkString importCible;		   ///< le dossier (chemin du navigateur) ou importer ; vide = le courant
				/// La case « active » de l'en-tete des Details (2026-10-01), relevee au
				/// dessin (vide = pas de selection).
				nkgui::NkRect caseActif{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect boutonImporter{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect boutonExporter{0.f, 0.f, 0.f, 0.f};
				bool cloisonContenu = false; ///< la cloison dossiers | cartes est tenue
				// --- Le navigateur a la maniere d'UE5 (2026-10-01, document 02 §3) ---
				/// « Tout » : la racine du fil d'Ariane (Unreal « All ») -- ses deux
				/// dossiers, « Contenu » et « Acteurs », en cartes.
				bool contenuTout = false;
				/// La collection regardee (indice dans la memoire), -1 = aucune.
				int32 contenuCollection = -1;
				/// L'entree ACTIVE et l'ANCRE de la selection, par CHEMIN : les cartes
				/// sont reconstruites a chaque trame, un indice ne survivrait pas.
				NkString contenuActif;
				NkString contenuAncre;
				/// Le presse-papiers du navigateur (Ctrl+X / Ctrl+C), des chemins.
				NkVector<NkString> pressePapierContenu;
				bool pressePapierCouper = false;
				/// La SUPPRESSION en attente de confirmation (modale tant que non vide).
				NkVector<NkString> contenuASupprimer;
				NkVector<NkString> contenuReferences; ///< les scenes et prefabs qui les citent
				/// Le GLISSER lache, en attente du menu « Deplacer ici / Copier ici ».
				NkVector<NkString> deposeSources;
				NkString deposeCible;
				/// Le RENOMMAGE en place : le chemin vise (vide = aucun).
				NkString renommeChemin;
				/// La memoire du navigateur (couleurs, favoris, collections), relue
				/// quand elle est perimee (NkContentBrowserDisque.h).
				editorkit::NkDisqueMeta contenuMeta;
				bool contenuMetaPerimee = true;
				/// La collection visee par le clic droit.
				int32 collectionMenu = -1;
				/// Les vignettes REELLES des images : chemin -> texture (0 = echec).
				NkVector<NkString> vignettesCles;
				NkVector<uint32> vignettesTex;
				/// Le rectangle du navigateur (sous ses onglets) a la derniere trame.
				nkgui::NkRect contenuZone{0.f, 0.f, 0.f, 0.f};
				/// Precedent / suivant de la barre du navigateur (le banc y vise).
				nkgui::NkRect contenuPrecedent{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect contenuSuivant{0.f, 0.f, 0.f, 0.f};
				/// Les demandes de DEMARRAGE pour une capture sans souris (--contenu=,
				/// --contenu-choisir=, --contenu-menu=, --contenu-deposer=).
				NkString demContenu;
				NkString demChoisir;
				NkString demMenu;
				NkString demDeposer;
				int32 demTrame = 0;
				bool demFiltres = false; ///< --contenu-filtres : la rangee des puces ouverte au depart
				/// Supprimer = vers la CORBEILLE de l'OS (recuperable). Le banc le met a
				/// faux : ses dossiers temporaires ne doivent pas remplir la corbeille.
				bool contenuCorbeille = true;
				/// La scene a ouvrir (double-clic sur une scene du Contenu), chemin
				/// ABSOLU, consommee par la question « non enregistree ».
				NkString sceneAOuvrir;
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
				/// Les cartes REPLIEES, par nature (bit = NkCarteEditeur) : replier le
				/// Collisionneur le replie pour toutes les entites, comme Unity.
				uint32 cartesRepliees = 0u;
				/// L'ORDRE des cartes (le Transform reste en tete). Monter / Descendre
				/// le changent pour toutes les entites : NKECS ne range pas les
				/// composants d'une entite, il n'y a pas d'ordre propre a garder.
				uint8 ordreCartes[static_cast<uint32>(NkCarteEditeur::NK_COUNT)] = {0, 1, 2, 3, 4, 5, 6, 9, 10, 7, 8};
				int32 carteMenu = -1;			   ///< la carte dont le menu « ⋮ » est ouvert
				NkPressePapierComposant pressePapier; ///< « Copier les valeurs »
				/// Le libelle d'un nombre qu'on FROTTE (Unity : tirer sur le libelle).
				uint32 frotteId = 0u;
				float32 frotteX = 0.f;
				/// La recherche du menu « Ajouter un composant » : on tape, il filtre.
				char menuFiltre[32] = {};

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
				/// UN PAR GESTE (2026-09-30, barre flottante d'UE5) : deplacer sur la
				/// grille sans forcer les angles ronds, ou l'inverse.
				bool accrocheGrille = true;
				bool accrocheAngle = true;
				bool accrocheEchelle = true;
				/// Le gizmo Deplacer suit les axes de l'ENTITE (sa rotation) au lieu
				/// de ceux du monde. L'echelle, elle, est toujours locale : elle est
				/// cuite dans les dimensions propres de l'entite (NkEditeurMettreAEchelle).
				bool repereLocal = false;
				float32 gizmoRot0 = 0.f; ///< la rotation de l'entite a la saisie (rad)
				/// Le rectangle de la barre flottante, garde d'une trame a l'autre :
				/// le viseur ne prend pas un clic qui tombe dessus.
				nkgui::NkRect barreFlottante{0.f, 0.f, 0.f, 0.f};
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
				/// Fichier > Construire… : la fenetre s'ouvre a la trame suivante
				/// (son etat est a l'application, Livraison/NkEditeurFenetreConstruire.h).
				bool construireDemande = false;

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
				/// Le panneau Entrees (NkEditeurEntrees.cpp) : ouvert par Fenetre >
				/// Entrees, ferme par sa croix.
				bool panneauEntrees = false;
				/// Le glisser d'une ligne de l'Outliner (2026-09-29) : il ne COMMENCE
				/// qu'au-dela de quelques pixels, sans quoi chaque clic de selection
				/// serait un depot sur soi-meme.
				bool glisseArbre = false;
				bool appuiArbre = false; ///< l'appui qui a commence le geste etait DANS l'arbre
				nkgui::NkVec2 departGlisseArbre{0.f, 0.f};
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

		// --- Les pas d'accrochage proposes par la barre flottante -------------
		inline const float32 *NkPasGrille(int32 &n) noexcept {
			static const float32 k[] = {0.05f, 0.1f, 0.25f, 0.5f, 1.f, 2.f};
			n = static_cast<int32>(sizeof(k) / sizeof(k[0]));
			return k;
		}
		inline const float32 *NkPasAngle(int32 &n) noexcept {
			static const float32 k[] = {1.f, 5.f, 10.f, 15.f, 30.f, 45.f, 90.f};
			n = static_cast<int32>(sizeof(k) / sizeof(k[0]));
			return k;
		}
		inline const float32 *NkPasEchelle(int32 &n) noexcept {
			static const float32 k[] = {0.05f, 0.1f, 0.25f, 0.5f, 1.f};
			n = static_cast<int32>(sizeof(k) / sizeof(k[0]));
			return k;
		}

		// --- Outils communs (NkEditeurChrome.cpp) -----------------------------
		NkPaletteEditeur NkEditeurPalette(const editorkit::NkTheme &theme);
		/// Place toutes les zones. Aucune n'est calculee ailleurs.
		void NkEditeurPlanifier(NkEditeurInterface &ui, float32 largeur, float32 hauteur);
		/// Execute une action de la table. Le seul endroit ou une commande agit.
		void NkEditeurExecuter(NkEditeurCadre &c, int32 action);
		/// Ouvre un menu deroulant sous `ancre` (le ferme s'il etait deja ouvert).
		void NkEditeurOuvrirMenu(NkEditeurCadre &c, NkMenuEditeur menu, const nkgui::NkRect &ancre);
		/// Monte (sens -1) ou descend (+1) la carte `carteMenu` d'un cran PARMI
		/// celles que la selection affiche (NkEditeurDetails.cpp).
		void NkEditeurDeplacerCarte(NkEditeurCadre &c, int32 sens);
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
		/// Les actions du menu du navigateur (NK_A_CONTENU_*), sur l'element du
		/// clic droit (NkEditeurInterface::contenuMenuChemin) -- NkEditeurTiroir.cpp.
		void NkEditeurActionContenu(NkEditeurCadre &c, int32 action);
		/// Importe `sources` (chemins de l'OS) dans le dossier du Contenu `relatif`
		/// (nul = le dossier courant du navigateur, ou la racine du Contenu), puis
		/// MONTRE le resultat : le navigateur passe sur ce dossier, les fichiers
		/// crees choisis. Le bouton, le menu et le depot de l'OS passent tous ici.
		NkRapportImport NkEditeurImporterIci(NkEditeurModele &m, NkEditeurInterface &ui, const NkVector<NkString> &sources,
											 const char *relatif = nullptr);
		/// Le DEPOT de fichiers de l'OS (NkDropFileEvent), au point (x, y) de la
		/// fenetre : sur une carte de dossier du Contenu, dans ce dossier ; sinon,
		/// comme « Importer… » (le dossier courant).
		NkRapportImport NkEditeurDeposerFichiers(NkEditeurModele &m, NkEditeurInterface &ui, const NkVector<NkString> &sources,
												 float32 x, float32 y);
		/// Exporte les assets choisis (NkEditeurInterface::contenuChoisis) vers le
		/// dossier ABSOLU `destination`.
		NkRapportExport NkEditeurExporterChoisis(NkEditeurModele &m, NkEditeurInterface &ui, const char *destination);
		/// (2026-10-01) La CONFIRMATION d'une suppression du Contenu (modale tant que
		/// NkEditeurInterface::contenuASupprimer n'est pas vide) : la liste, les
		/// scenes et prefabs qui citent ces assets, Supprimer / Annuler.
		void NkEditeurDessinerSuppressionContenu(NkEditeurCadre &c);
		/// Le CLAVIER du navigateur quand il a le focus (le dernier clic est tombe
		/// dedans) : Ctrl+C / X / V / D / A, F2, Suppr. Rend vrai si une touche a
		/// ete prise -- la scene ne la recoit pas aussi.
		bool NkEditeurContenuAuClavier(NkEditeurCadre &c);
		/// La palette des couleurs de dossier (Unreal « Set Color ») : son nom et sa
		/// couleur (0 = celle du theme).
		int32 NkEditeurNbCouleursDossier() noexcept;
		const char *NkEditeurCouleurDossier(int32 k, uint32 &rgba) noexcept;
		void NkEditeurDessinerVue(NkEditeurCadre &c);
		/// Mene la camera, EN DOUCEUR, sur la selection -- ou sur toute la scene
		/// si `toutLaScene` ou si rien n'est selectionne (NkEditeurZoneACadrer).
		/// Meme si la selection est hors du cadre : c'est tout l'interet.
		void NkEditeurDemanderCadrage(NkEditeurCadre &c, bool toutLaScene);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURINTERFACE_H__
