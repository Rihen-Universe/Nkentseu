#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkProjectLauncherModel.h — LE LANCEUR DE PROJETS PARTAGE (2026-10-01).
//
// L'ecran qu'on voit AVANT d'avoir un projet ouvert, facon Unreal Engine 5 /
// Unity Hub : une colonne de navigation a icones (Projets, Apprendre,
// Communaute, Installations...), la marque de l'application en tete, de
// grandes vignettes de projets recents (apercu, nom, dossier, date), les
// modeles de nouveau projet en cartes illustrees, une recherche, des vues
// grille / liste, et le theme sombre ou clair que le theme de l'application
// donne.
//
// ⚠️ UN SEUL COMPOSANT POUR TOUTE LA FAMILLE. NKCraft, Nogee, NkAnimaEditor,
//    PV3DE, UnkenyEditor, NKUIDesign, NKCode -- et le futur Launcher du moteur
//    (R23 de la feuille de route d'UnkenyEditor). CHAQUE APPLICATION A SA TOUCHE
//    PAR LA DONNEE, jamais par une copie du dessin : son nom, son logo (crochet
//    ou glyphe), sa couleur d'accent, ses modeles, ses extensions, ses pages.
//
// LA FORME DES COMPOSANTS DU KIT (cf. NkComponentPaint.h) :
//   resultat = NkDrawProjectLauncher(PEINTRE, ENTREE, RECTANGLE, MODELE, STYLE,
//                                    CROCHETS)
//   - le PEINTRE est l'interface du kit : le composant ne connait ni NKGui, ni
//     NKCanvas, ni la couleur d'un theme -- il passe des ROLES ;
//   - l'ENTREE est une structure plate (souris, molette) : rejouable sans
//     fenetre, donc photographiable par une sonde ;
//   - le RESULTAT dit CE QUE L'UTILISATEUR A DEMANDE (ouvrir tel projet, creer
//     depuis tel modele...) : c'est l'HOTE qui ouvre les dialogues et touche au
//     disque. Le composant n'ecrit jamais un fichier.
//
// ⚠️ LE CLAVIER N'ENTRE PAS ICI (meme contournement assume que le Content
//    Browser) : le composant RESERVE la boite de recherche et rapporte son
//    rectangle ; l'hote, qui a le clavier, y peint le champ et ecrit dans
//    `filtre`. Cf. NkProjectLauncherHost.h pour le monde NKGui.
//
// ⚠️ LES ICONES SONT DESSINEES PAR LE COMPOSANT (traits et polygones du
//    peintre), pas tirees d'une police ou d'un jeu d'images : le kit ne connait
//    aucun jeu d'icones d'application (regle B de NkComponentPaint.h), et un
//    lanceur doit etre beau dans TOUTES les applications, y compris celles dont
//    le peintre dessine un carre a la place d'une icone inconnue.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKEditorKit/Components/NkComponentPaint.h"
#include "NKEditorKit/NkTheme.h" // NkRole : les roles par defaut du style (en-tete sans NKGui)

namespace nkentseu {
	namespace editorkit {

		// ── LES GLYPHES DU LANCEUR ──────────────────────────────────────────────
		// Une bibliotheque FERMEE de dessins vectoriels (NkProjectLauncherDraw.cpp,
		// `NkLanceurPeindreGlyphe`). ⚠️ Append-only : une application peut les
		// ecrire dans un fichier de configuration.
		enum class NkLanceurGlyphe : uint8 {
			Aucun = 0,
			Projets,	   ///< quatre tuiles
			Apprendre,	   ///< un livre ouvert
			Communaute,	   ///< deux silhouettes
			Installations, ///< une boite et une fleche descendante
			Nouveau,	   ///< un plus
			Ouvrir,		   ///< un dossier
			Recherche,	   ///< une loupe
			Epingle,
			Corbeille,
			Grille,
			Liste,
			Soleil,
			Lune,
			Lien,		///< fleche sortante (lien externe)
			Cube,		///< modelisation 3D
			Sphere,
			Personnage, ///< silhouette (animation, personnage)
			Os,			///< squelette, animation
			Coeur,		///< medical, vie
			Calques,	///< interfaces, design
			Code,		///< chevrons < >
			Manette,	///< jeu
			Paysage,	///< terrain, monde
			Camera,		///< film, rendu
			Ampoule,	///< lumiere, idee
			Vide,		///< cadre pointille : projet vierge
			Horloge,	///< date
			Document,
			Reglages,	///< engrenage
			Etoile,
			Croix,		///< fermer
			Moins,		///< reduire
			Carre,		///< agrandir
			Pinceau,	///< peinture, texture
			Moteur,		///< engrenage + eclair : moteur, installations
			Dossier,	///< un dossier ferme (etat vide)
			Count
		};

		/// Un projet RECENT, tel que l'hote le connait.
		struct NkLanceurProjet {
				NkString nom;
				NkString chemin; ///< chemin du fichier projet (affiche : son DOSSIER)
				NkString date;	 ///< deja formatee par l'hote (« 2026-09-30 14:02 »)
				/// Vignette : poignee de texture DU PEINTRE (0 = aucune -> tuile
				/// dessinee aux initiales). Dimensions natives pour le cadrage
				/// « couvrir » (l'apercu remplit la carte sans etre deforme).
				uint32 image = 0u;
				int32 imageW = 0, imageH = 0;
				bool epingle = false;
				/// 0 = sain ; 1 = INTROUVABLE ; 2 = VIDE (le fichier existe, rien
				/// dedans) ; 3 = autre, dit par `etatTexte`. Un projet malade se
				/// montre, il ne s'ouvre pas : le clic propose de le retirer.
				uint8 etat = 0u;
				NkString etatTexte;
				/// Libre pour l'hote (indice dans SA liste, identifiant...).
				uint32 hote = 0u;
		};

		/// Un MODELE de nouveau projet : une carte illustree.
		struct NkLanceurModele {
				NkString nom;
				NkString description;
				NkString categorie; ///< petite etiquette au-dessus du nom (« 3D », « Jeu »)
				NkLanceurGlyphe glyphe = NkLanceurGlyphe::Vide;
				/// Teinte de l'illustration (0xRRGGBBAA) ; 0 = l'accent de l'application.
				uint32 couleur = 0u;
				uint32 image = 0u; ///< illustration en image (poignee du peintre), 0 = dessinee
				int32 imageW = 0, imageH = 0;
				/// Un modele affiche FAIT ce qu'il annonce. S'il n'est pas encore
				/// branche, il se montre grise avec sa raison -- jamais un faux bouton.
				bool disponible = true;
				NkString raison; ///< « a venir » ...
		};

		/// Une carte d'une page de LIENS (Apprendre, Communaute, Installations).
		struct NkLanceurLien {
				NkString titre;
				NkString description;
				NkString url;	///< vide = carte d'information ou « a venir » (cf. `disponible`)
				NkString badge; ///< petite pastille (« installee », « a venir », « 0.1.0 »)
				NkLanceurGlyphe glyphe = NkLanceurGlyphe::Lien;
				/// faux = grisee (lien pas encore ouvert). Vrai sans URL = simple
				/// information (une installation, un chemin) : lisible, pas cliquable.
				bool disponible = true;
		};

		enum class NkLanceurPageType : uint8 {
			Projets = 0, ///< modeles + projets recents
			Liens,		 ///< une grille de `NkLanceurLien`
		};

		/// Une entree de la colonne de navigation, et la page qu'elle montre.
		struct NkLanceurPage {
				NkString libelle;	///< dans la colonne (« Projets »)
				NkString titre;		///< en tete de page ; vide = `libelle`
				NkString sousTitre; ///< sous le titre ; vide = calcule (Projets) ou rien
				NkLanceurGlyphe glyphe = NkLanceurGlyphe::Projets;
				NkLanceurPageType type = NkLanceurPageType::Projets;
				NkVector<NkLanceurLien> liens;
		};

		/// LA TOUCHE DE L'APPLICATION.
		struct NkLanceurIdentite {
				NkString nom = NkString("Application"); ///< « NKCraft »
				/// Debut du nom ecrit dans la couleur d'accent (« NK »), vide = aucun.
				NkString prefixe;
				NkString sousTitre; ///< « Modelisation 3D — Nkentseu »
				NkString version;	///< « 0.1.0 »
				/// Extensions des fichiers projet, pour les textes (« .nk3dm »). Le
				/// filtre du dialogue d'ouverture reste a l'hote.
				NkString extensions;
				/// Le mot qui dit ce qu'on ouvre : « projet », « workspace », « cas »...
				NkString motProjet = NkString("projet");
				NkString motProjets = NkString("projets");
				/// Accent de l'application (0xRRGGBBAA) ; 0 = le role `accent` du style.
				uint32 accent = 0u;
				/// Logo par defaut : une tuile a l'accent portant ce glyphe. Le
				/// crochet `peindreLogo` le remplace (l'araignee de NKCraft).
				NkLanceurGlyphe glyphe = NkLanceurGlyphe::Projets;
				uint32 logoImage = 0u; ///< logo en image (poignee du peintre), prioritaire sur le glyphe
		};

		/// L'image de version facon Blender (NKCraft) : une bande en tete de la
		/// page Projets. Sans image, elle n'existe pas -- pas de cadre vide.
		struct NkLanceurBanniere {
				uint32 image = 0u;
				int32 w = 0, h = 0;
				NkString legende; ///< « NKCraft 0.1.0 »
				NkString credit;  ///< « Titre — Auteur »
		};

		/// Ce que l'utilisateur a DEMANDE cette image. L'hote agit.
		enum class NkLanceurAction : uint8 {
			Aucune = 0,
			NouveauProjet,		 ///< bouton principal (modele par defaut de l'hote)
			NouveauDepuisModele, ///< `index` = modele
			Ouvrir,				 ///< « Ouvrir... » : le dialogue de l'hote
			OuvrirRecent,		 ///< `index` = projet
			Epingler,			 ///< `index` = projet (bascule)
			Retirer,			 ///< `index` = projet (de la liste, jamais du disque)
			Purger,				 ///< retirer tous les projets introuvables ou vides
			OuvrirLien,			 ///< `index` = lien de la page `page`, `url` renseigne
			BasculerTheme,
			FenetreReduire,
			FenetreAgrandir,
			FenetreFermer,
			FenetreGlisser, ///< appui dans la barre de titre (fenetres sans cadre)
			ActionHote,		///< le bouton propre a l'application (`actionHote`)
		};

		struct NkProjectLauncherModel {
				// ── DONNEES (posees par l'hote) ────────────────────────────────
				NkLanceurIdentite identite;
				NkVector<NkLanceurPage> pages; ///< vide = une seule page « Projets »
				NkVector<NkLanceurProjet> projets;
				NkVector<NkLanceurModele> modeles;
				NkLanceurBanniere banniere;
				NkString astuce;	 ///< pied de page, a droite (« Astuce : ... »)
				NkString erreur;	 ///< pied de page, a gauche, en rouge ; prioritaire
				NkString piedDePage; ///< pied de page, a gauche, quand il n'y a pas d'erreur
				/// Barre de titre DESSINEE avec reduire / agrandir / fermer : pour
				/// les fenetres SANS CADRE (NKCraft). Les coquilles qui ont deja la
				/// leur laissent faux.
				bool chromeFenetre = false;
				bool fenetreMaximisee = false;
				/// Le bouton de theme en bas de colonne (l'hote sait basculer).
				bool themeBasculable = false;
				bool themeSombre = true; ///< ce que le bouton affiche
				/// Le bouton « Ouvrir... » existe (faux : l'application n'ouvre
				/// pas de fichier, ex. un lanceur de moteur sans projet).
				bool ouvrirPossible = true;
				/// La COLONNE de navigation (marque, pages, theme). Faux : le lanceur
				/// s'INCRUSTE dans une application qui a deja la sienne (NKCode et
				/// son accueil) ; seule la page courante est peinte.
				bool colonne = true;
				/// UN bouton propre a l'application, a gauche de « Ouvrir... »
				/// (vide = aucun) : rend `NkLanceurAction::ActionHote`.
				NkString actionHote;
				NkLanceurGlyphe glypheActionHote = NkLanceurGlyphe::Reglages;

				// ── ETAT (ecrit par le composant, garde par l'hote) ─────────────
				int32 page = 0;
				char filtre[128] = {0}; ///< la recherche ; l'HOTE y ecrit la frappe
				bool rechercheFocus = false;
				bool vueListe = false;
				uint8 tri = 0u; ///< 0 = ordre de l'hote (recents), 1 = par nom
				float32 defilement = 0.f;
		};

		/// Les ROLES. Les defauts sont ceux du theme du kit : un hote n'a rien a
		/// poser pour un lanceur juste en sombre ET en clair.
		struct NkProjectLauncherStyle {
				uint16 fond = (uint16)NkRole::WindowBg;
				uint16 colonne = (uint16)NkRole::PanelBg;
				uint16 carte = (uint16)NkRole::PanelBg;
				uint16 carteSurvol = (uint16)NkRole::PanelHeader;
				uint16 champ = (uint16)NkRole::InputBg;
				uint16 bord = (uint16)NkRole::Border;
				uint16 texte = (uint16)NkRole::Text;
				uint16 texteDiscret = (uint16)NkRole::TextMuted;
				uint16 accent = (uint16)NkRole::AccentUi;
				uint16 texteSurAccent = (uint16)NkRole::TextOnAccent;
				uint16 erreur = (uint16)NkRole::StatusErr;
				uint16 epingle = (uint16)NkRole::AccentSel;
				/// Les polices que le peintre sait servir (`TextePolice`) : le TITRE
				/// de page, l'INTERTITRE de section, le texte GRAS des cartes, le
				/// PETIT texte des metadonnees. Un peintre qui ne les connait pas
				/// retombe sur sa police normale -- moins beau, jamais faux.
				uint8 policeTitre = 3u;
				uint8 policeIntertitre = 4u;
				uint8 policeGrasse = 1u;
				uint8 policePetite = 5u;
		};

		struct NkProjectLauncherHooks {
				/// Peint le LOGO de l'application dans `r` (carre). Nul = la tuile
				/// d'accent et son glyphe.
				void (*peindreLogo)(void *user, NkComponentPaint &p, const NkPaintRect &r) = nullptr;
				void *user = nullptr;
		};

		struct NkProjectLauncherResult {
				NkLanceurAction action = NkLanceurAction::Aucune;
				int32 index = -1;
				const char *url = nullptr; ///< pour `OuvrirLien`
				/// La boite de recherche (zone de SAISIE, icone exclue) : l'hote y
				/// peint son champ quand `rechercheFocus` est vrai. w == 0 : aucune.
				NkPaintRect recherche;
				/// La barre de titre (fenetres sans cadre) : glisser = deplacer.
				NkPaintRect barreTitre;
				bool curseurMain = false; ///< un element cliquable est survole
				int32 projetsVisibles = 0; ///< apres recherche (pour les sondes)
				float32 contenuH = 0.f;	   ///< hauteur du contenu defilant (sondes)
		};

		/// Dessine le lanceur dans `r` et rend la demande de l'utilisateur.
		NkProjectLauncherResult NkDrawProjectLauncher(NkComponentPaint &p, const NkComponentInput &in,
													  const NkPaintRect &r, NkProjectLauncherModel &m,
													  const NkProjectLauncherStyle &s,
													  const NkProjectLauncherHooks &h);

		/// Peint un glyphe du lanceur dans le carre `r`, couleur 0xRRGGBBAA. Expose
		/// pour les hotes qui veulent la meme icone ailleurs (un menu, un logo).
		void NkLanceurPeindreGlyphe(NkComponentPaint &p, NkLanceurGlyphe g, const NkPaintRect &r,
									uint32 rgba);

	} // namespace editorkit
} // namespace nkentseu
