#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiMonteur.h
// @Brief   LA PORTE MANQUANTE du format `.nkgui` : monter un document lu en
//          widgets NKGui reels, qui se placent et qui dessinent.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUI MANQUAIT, ET CE QUE CE FICHIER EST
// =============================================================================
//  Le format `.nkgui` etait specifie (documents 2, 7, 9), exemplifie (21 fichiers
//  ecrits a la main), lu (`NkGuiArchive`, 2030 lignes, purement syntaxique),
//  reemis a l'octet et valide par role -- et **jamais MONTE**. Mesure du
//  2026-09-14, par ouverture des fichiers et non par comptage de motifs : les
//  seuls consommateurs de `NkGuiArchive.h` etaient `NkGuiRoundTrip.h` (qui
//  mesure l'aller-retour) et `NkGuiValidate.h` (qui pose des diagnostics).
//  **Aucun fichier du depot n'incluait a la fois `NkGuiArchive.h` et
//  `NkGuiWidgets.h`.** Un format que rien n'instancie decrit des interfaces que
//  personne ne voit.
//
//  Ce fichier est cette porte, et rien d'autre : il lit une `NkArchive` produite
//  par `NkGuiArchive::Read` et appelle les widgets que NKGui a DEJA. Il n'ecrit
//  pas un widget de plus -- les dix-huit roles du format avaient tous leur
//  fonction dans `NkGuiWidgets.h` avant qu'une ligne d'ici ne soit ecrite.
//
// =============================================================================
//  OU IL VIT, ET POURQUOI `NKGui.jenga` N'A PAS BOUGE
// =============================================================================
//  `NKGui` ne depend pas de `NKSerialization`. Lui ajouter cette dependance
//  tirerait NKReflection et NKFileSystem chez TOUS ses consommateurs -- la faute
//  exacte que `NKEditorKit` a corrigee le 2026-09-01 en RETIRANT NKCanvas de son
//  jenga. Son commentaire donne la parade, et c'est celle-ci : « l'application
//  qui veut NkEditorCanvasRenderer declare NKCanvas chez elle ».
//
//  Ce fichier est donc un EN-TETE SEUL, non compile (`NKGui.jenga` fait
//  `files(["src/**.cpp"])`, un `.h` n'y ajoute rien), et c'est l'application qui
//  l'inclut qui declare `NKSerialization`. Zero dependance ajoutee a NKGui, et
//  aucun cycle : NKSerialization ne depend pas de NKGui.
//
// =============================================================================
//  CE QU'IL MONTE, ET CE QU'IL NE MONTE PAS -- LA LISTE EST ICI, PAS AILLEURS
// =============================================================================
//  MONTE  :  l'ETAT AU REPOS du document. Les conteneurs (Window, Panel, VBox,
//            HBox, Group), leur `gap`, leur `align`/`justify`, les `Spacer`, et
//            les quatorze roles qui dessinent.
//            Depuis le 2026-09-17, il monte aussi la ZONE HOTE (`Host`) : un
//            rectangle que l'APPLICATION remplit par le crochet `RemplirHote`.
//            C'est la frontiere du format -- le document dit OU et QUOI, l'hote
//            garde QUAND et COMMENT -- et une zone que personne ne remplit se
//            SIGNALE (hachures + nom) au lieu de disparaitre.
//
//  NE MONTE PAS, et ce n'est pas un oubli :
//   - le COMPORTEMENT qui s'execute (`set r = n1.value * 100`, `if`) : la section
//     `behavior` est une TRANCHE DE SOURCE VERBATIM cote lecteur, son sens n'est
//     modelise nulle part. Elle est COMPTEE, pas interpretee ;
//   - les EVENEMENTS et les ANIMATIONS (`duration`, `easing`) : ils demandent un
//     temps et une boucle, ce banc n'a ni l'un ni l'autre ;
//   - les QUATRE ETATS D'APPARENCE autres que le repos (`Hover`, `Pressed`,
//     `Focus`/`FocusVisible`, `Disabled`). Ils sont LUS et COMPTES
//     (`etatsNonAppliques`) mais jamais peints : **sans interaction, aucun d'eux
//     ne peut etre atteint**, et en peindre un serait montrer un rendu que le
//     document n'a pas demande pour cet instant ;
//   - les LIAISONS `bind` : le chemin est lu et conserve comme CLE D'ETAT, mais
//     il ne suit aucune donnee vivante -- il n'y a pas de modele a lier.
//
//  ⚠️ LE `Panel` N'OUVRE PAS `BeginPanel`, ET C'EST DELIBERE. `BeginPanel` pose
//     un clip et sa doc dit « niveau unique pour l'instant ; l'imbrication
//     viendra avec une pile ». Un document en imbrique (`Panel` > `VBox` > `HBox`)
//     et un clip couperait la mesure au lieu de la rendre. Le monteur peint donc
//     le fond (`PanelBackground`, la meme primitive) et laisse le contenu dans le
//     flux. Le jour ou `BeginPanel` empile, cette ligne-ci change, et elle seule.
//
//  ⚠️ `items = [a, b]` ET `values = [1, 2, 3]` SONT DES JETONS NUS. Le lecteur le
//     dit lui-meme : « un outil qui veut iterer dessus doit encore l'analyser
//     lui-meme ». Le monteur le fait donc ici, du cote du CONSOMMATEUR --
//     `LireListeNombres` / `CompterElements`. Le format n'est pas touche.
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Widgets/NkGuiWidgets.h"
#include "NKGui/Doc/NkGuiJetons.h" // P1 : la table des jetons, lue par NkGuiCouleur
#include "NKSerialization/NkGui/NkGuiArchive.h"
#include <cstdlib> // getenv : la mutation de banc `sansmarqueur` de la zone hote

namespace nkentseu {
	namespace nkgui {

		// =====================================================================
		//  LES ROLES -- le vocabulaire du document 7, celui que le corpus emploie
		// =====================================================================
		enum class NkGuiRole : uint8 {
			Inconnu = 0,
			Window,
			Panel,
			VBox,
			HBox,
			Group,
			Text,
			Button,
			RepeatButton,
			Checkbox,
			Slider,
			TextField,
			Dropdown,
			Progress,
			Separator,
			Spacer,
			Image,
			Chart,
			TabBar,
			Expander,
			Splitter,
			ListBox,
			Item,
			TreeItem,
			DockSpace,
			// 🔴 `Callback` ETAIT ICI, ET C'ETAIT UN HOMONYME. Le mot appartient deja DEUX
			//    FOIS au format : `callback` est l'une des huit sections, et
			//    `Callback "alerte"(...)` est un APPEL DE COMPORTEMENT -- la grammaire
			//    l'ecrit (`NkGuiInteraction.h:66`) et le corpus l'emploie ainsi
			//    (`valides/05_animation_comportement.nkgui:29`, qui valide a 0 erreur).
			//    Le role de widget du meme nom n'etait dans AUCUN document et le
			//    validateur le refusait (`E-ROLE-INCONNU`, mesure) : du code que seul un
			//    document invalide pouvait atteindre. On ne le legalise pas, on le retire.
			/// LA ZONE QUE L'APPLICATION REMPLIT -- viseur 3D, toile, editeur de texte.
			/// Le document dit OU et QUOI ; l'hote garde QUAND et COMMENT.
			Host,
			/// LA ZONE DEFILANTE. Elle etait au vocabulaire du VALIDATEUR
			/// (`NkGuiValidate.h`, `axis` et `always`) et absente d'ici : un document
			/// que le format accepte perdait tout son contenu au montage.
			Scroll,
			// =================================================================
			//  LES MENUS (2026-09-26) -- LE FORMAT LES PORTAIT, LE MONTEUR NON
			// =================================================================
			//  Mesure du 25/09, par LANCEMENT et non par lecture : un document
			//  `MenuBar > Menu > MenuItem` validait a **0 erreur** et montait
			//  **0 widget pour 4 roles inconnus** (`NKGuiMonteTest --monter=`). Le
			//  format etait donc PLUS RICHE que le monteur -- l'inverse exact de la
			//  note « le monteur est plus permissif que le format ».
			//
			//  ⚠️ ET RIEN N'A ETE ECRIT POUR EUX ICI. NKGui avait deja `BeginMenuBar`,
			//     `BeginMenu` et `MenuItem` (`NkGuiWidgets.h`, l. 494-511), et la
			//     coquille s'en servait. Il manquait le FIL, comme partout ailleurs
			//     dans ce chantier. `MenuItem` du format porte `label`/`shortcut`/
			//     `checked` ; `MenuItem` de NKGui prend `label, shortcut, enabled,
			//     checked` : les deux vocabulaires coincident sans qu'on invente un mot.
			MenuBar,
			Menu,
			MenuItem,
			// =================================================================
			//  DEUX CONTENEURS DE PLUS (2026-09-26), ET DEUX SEULEMENT
			// =================================================================
			//  Sur les douze roles que le format acceptait sans que le monteur les
			//  monte, ce sont les DEUX SEULS dont le schema du format et la
			//  signature de NKGui coincident sans rien inventer :
			//    `Flow { gap }`           -> `BeginFlow(ctx, gap)`
			//    `Grid { columns, gap }`  -> `BeginGrid(ctx, columns, gap)`
			//
			//  ⚠️ ET LE COMPTE N'EST PAS CELUI QU'ON ESPERAIT. NKGui a bien une
			//     fonction pour presque tous les autres, mais AUCUNE ne correspond
			//     exactement :
			//      - `Switch`, `RadioGroup` : AUCUNE fonction dans NKGui. Du neuf.
			//      - `Stack` : `BeginStack(ctx, width, height)` EXIGE une taille que
			//        le schema (`anchor` seul) ne porte pas -- il faudrait l'inventer.
			//      - `Row`/`Column` : AMBIGUS. Leur schema est identique a celui de
			//        `HBox`/`VBox` (donc des synonymes), mais NKGui a AUSSI un
			//        `BeginRow`/`BeginColumn` a poids de flex, qui est autre chose.
			//        Trancher serait decider a la place de Rodolf.
			//      - `ColorField` : `ColorEdit4` existe, mais `bind` passe par un
			//        modele qui ne stocke QU'UN `float32` -- une couleur n'y tient pas.
			//      - `NumberField`, `Drag`, `ImageButton`, `Table` : plausibles, mais
			//        chacun a un attribut sans equivalent (`dir`, `valueType`, les
			//        references d'image, les colonnes nommees). Recenses, pas montes.
			//
			//  Le detail, role par role, est dans
			//  `echanges/interface-document.reponses.md`.
			Flow,
			Grid,
			// ── LES HUIT DU 27/09 ────────────────────────────────────────
			//  Au schema du format depuis toujours, montes seulement
			//  maintenant. Le `switch` de `MonterBloc` porte, pour chacun, la
			//  limite qui lui reste et l'attribut qu'il COMPTE au lieu de
			//  l'inventer. Voir la note de refus ci-dessus : elle avait raison
			//  sur les faits, la reponse a change.
			NumberField,
			Drag,
			ColorField,
			Switch,
			RadioGroup,
			ImageButton,
			Stack,
			Table
			// 🔴 `ContextMenu` N'EST PAS ICI, ET C'EST UN REFUS, PAS UN OUBLI. NKGui a
			//    bien `BeginPopupMenu`, mais il ne rend vrai que si quelqu'un a appele
			//    `ctx.OpenPopupAt(...)` AU CLIC DROIT -- et ce monteur monte l'etat au
			//    REPOS : il n'a aucun clic droit a offrir. Le monter donnerait un role
			//    qui ne se voit JAMAIS, quoi qu'ecrive le document.
			//
			//    Un role invisible par construction est PIRE qu'un role refuse : il se
			//    compterait parmi les montes, et personne n'irait chercher pourquoi
			//    rien n'apparait. Il reste donc `Inconnu` -- donc COMPTE dans
			//    `rolesInconnus`, donc present dans le verdict.
			//
			//    CE QU'IL FAUDRAIT POUR LE LEVER : un crochet d'ouverture cote hote,
			//    de la meme forme que `RemplirHote` -- l'hote dit « ce clic droit
			//    ouvre le menu nomme X ». C'est une conception, pas un detail.
		};

		/// Comparaison de noms sans <cstring> (le depot est zero-STL).
		inline bool NkGMotEgal(NkStringView v, const char *lit) noexcept {
			uint32 i = 0;
			for (; i < (uint32)v.Size(); ++i) {
				if (lit[i] == '\0' || v.Data()[i] != lit[i])
					return false;
			}
			return lit[i] == '\0';
		}

		inline NkGuiRole NkGuiRoleDepuisNom(NkStringView n) noexcept {
			if (NkGMotEgal(n, "Window")) return NkGuiRole::Window;
			if (NkGMotEgal(n, "Panel")) return NkGuiRole::Panel;
			if (NkGMotEgal(n, "VBox")) return NkGuiRole::VBox;
			if (NkGMotEgal(n, "HBox")) return NkGuiRole::HBox;
			if (NkGMotEgal(n, "Group")) return NkGuiRole::Group;
			if (NkGMotEgal(n, "Text")) return NkGuiRole::Text;
			if (NkGMotEgal(n, "Button")) return NkGuiRole::Button;
			if (NkGMotEgal(n, "RepeatButton")) return NkGuiRole::RepeatButton;
			if (NkGMotEgal(n, "Checkbox")) return NkGuiRole::Checkbox;
			if (NkGMotEgal(n, "Slider")) return NkGuiRole::Slider;
			if (NkGMotEgal(n, "TextField")) return NkGuiRole::TextField;
			if (NkGMotEgal(n, "Dropdown")) return NkGuiRole::Dropdown;
			if (NkGMotEgal(n, "Progress")) return NkGuiRole::Progress;
			if (NkGMotEgal(n, "NumberField")) return NkGuiRole::NumberField;
			if (NkGMotEgal(n, "Drag")) return NkGuiRole::Drag;
			if (NkGMotEgal(n, "ColorField")) return NkGuiRole::ColorField;
			if (NkGMotEgal(n, "Switch")) return NkGuiRole::Switch;
			if (NkGMotEgal(n, "RadioGroup")) return NkGuiRole::RadioGroup;
			if (NkGMotEgal(n, "ImageButton")) return NkGuiRole::ImageButton;
			if (NkGMotEgal(n, "Stack")) return NkGuiRole::Stack;
			if (NkGMotEgal(n, "Table")) return NkGuiRole::Table;
			if (NkGMotEgal(n, "Separator")) return NkGuiRole::Separator;
			if (NkGMotEgal(n, "Spacer")) return NkGuiRole::Spacer;
			if (NkGMotEgal(n, "Image")) return NkGuiRole::Image;
			if (NkGMotEgal(n, "Chart")) return NkGuiRole::Chart;
			if (NkGMotEgal(n, "TabBar")) return NkGuiRole::TabBar;
			if (NkGMotEgal(n, "Expander")) return NkGuiRole::Expander;
			if (NkGMotEgal(n, "Splitter")) return NkGuiRole::Splitter;
			if (NkGMotEgal(n, "ListBox")) return NkGuiRole::ListBox;
			if (NkGMotEgal(n, "Item")) return NkGuiRole::Item;
			if (NkGMotEgal(n, "TreeItem")) return NkGuiRole::TreeItem;
			if (NkGMotEgal(n, "DockSpace")) return NkGuiRole::DockSpace;
			if (NkGMotEgal(n, "Host")) return NkGuiRole::Host;
			if (NkGMotEgal(n, "Scroll")) return NkGuiRole::Scroll;
			if (NkGMotEgal(n, "MenuBar")) return NkGuiRole::MenuBar;
			if (NkGMotEgal(n, "Menu")) return NkGuiRole::Menu;
			if (NkGMotEgal(n, "MenuItem")) return NkGuiRole::MenuItem;
			if (NkGMotEgal(n, "Flow")) return NkGuiRole::Flow;
			if (NkGMotEgal(n, "Grid")) return NkGuiRole::Grid;
			// =================================================================
			//  `Row` ET `Column` SONT DES SYNONYMES (2026-09-26, tranche par Rodolf)
			// =================================================================
			//  Leur schema est DEJA identique a celui de `HBox`/`VBox` (`pBox` :
			//  `gap`, `align`, `justify`). Et les quatre references disent la meme
			//  chose : le poids de flex est un attribut de l'ENFANT, jamais un type
			//  de conteneur de plus --
			//    Qt       `QHBoxLayout`      -> `addWidget(w, stretch)`
			//    JavaFX   `HBox`             -> `HBox.setHgrow(...)`
			//    CSS      `flex-direction`   -> `flex-grow`
			//    Android  `LinearLayout`     -> `layout_weight`
			//  Aucun des quatre n'a DEUX conteneurs horizontaux. En garder deux
			//  obligerait l'auteur a choisir AVANT de savoir s'il voudra un poids.
			//
			//  🔴 ET ON NE PASSE PAS PAR `BeginRow`, PARCE QUE LA CONDITION POSEE
			//     EST REFUTEE. On avait ecrit : « si `BeginRow` a poids nul se comporte
			//     EXACTEMENT comme `BeginHBox`, on emploie `BeginRow` partout ». Mesure
			//     dans `NkGuiContext::NextItemRect` -- ce sont DEUX flux distincts (1 et
			//     5), et ils different sur trois points :
			//
			//       | quand aucune taille n'est donnee | HBox (flux 1) | Row (flux 5) |
			//       | largeur par defaut               |   **120 px**  |  **60 px**   |
			//       | hauteur de l'enfant              | la SIENNE     | ECRASEE par  |
			//       |                                  |               | `flexCross`  |
			//       | `curLineH` suivi                 |     oui       |     non      |
			//
			//     `BeginRow` force donc la hauteur de CHAQUE enfant a `ItemHeight()` et
			//     replie les largeurs inconnues sur 60 px au lieu de 120. Ce n'est pas
			//     « presque pareil » : c'est une autre mise en page. *Un nom de fonction
			//     qui ressemble n'est pas un equivalent.*
			//
			//  ⚠️ LE POIDS DE FLEX RESTE DONC A FAIRE, et c'est dit plutot que
			//     bricole : il demandera un attribut d'ENFANT que le format ne porte pas
			//     encore, et le chemin `BeginRow`/`BeginColumn` de NKGui l'attend deja.
			//     C'est un lot a part, pas un effet de bord de ce renommage.
			if (NkGMotEgal(n, "Row")) return NkGuiRole::HBox;
			if (NkGMotEgal(n, "Column")) return NkGuiRole::VBox;
			// `ContextMenu` n'est PAS traduit : voir le refus motive dans l'enumeration.
			return NkGuiRole::Inconnu;
		}

		/// Vrai pour les blocs du vocabulaire d'APPARENCE (document 9 §3), qui ne
		/// sont pas des widgets : les compter comme tels fausserait tout releve.
		inline bool NkGEstApparence(NkStringView n) noexcept {
			return NkGMotEgal(n, "appearance") || NkGMotEgal(n, "fill") || NkGMotEgal(n, "stroke")
				   || NkGMotEgal(n, "shadow") || NkGMotEgal(n, "font") || NkGMotEgal(n, "text")
				   || NkGMotEgal(n, "layout");
		}

		// =====================================================================
		//  CE QUE LE MONTAGE A PRODUIT -- de quoi mesurer sans regarder l'ecran
		// =====================================================================
		/// Un widget monte, avec le rectangle qu'il a REELLEMENT pris.
		struct NkGuiMonteItem {
				NkString id;		  ///< l'identifiant du fichier (`"colonne"`)
				NkString role;		  ///< le mot qui ouvre le bloc (`"VBox"`)
				NkRect rect;		  ///< ce que le layout lui a donne
				uint32 profondeur = 0;
				bool conteneur = false;
				/// L'axe du conteneur QUI LE PORTE. Deux widgets d'une `HBox`
				/// partagent leur `y` : sans cette information, tout releve
				/// d'empilement vertical les compte comme un chevauchement.
				bool axeHorizontal = false;
				/// ⚠️ CE WIDGET SORT-IL DE LA REGION DE SON CONTENEUR ? Un COMPTE ne dit
				///    pas QUI : le premier releve a rendu 4 debordements sur la sonde du
				///    `Spacer` et 3 sur le temoin SANS `Spacer`. Sans le nom, on aurait
				///    attribue les quatre au `Spacer`.
				bool deborde = false;
				/// ⚠️ LA VALEUR REELLEMENT MONTEE, et elle existe parce qu'un
				///    releve de POSITIONS ne suffit pas. Le curseur de
				///    `01_panneau_reglages` a affiche 0.00 pour un fichier qui
				///    ecrit `min = 0.5` : la poignee, elle, etait au MEME pixel
				///    dans les deux cas (`SliderFloat` borne `tt` a [0,1], donc
				///    0.0 et 0.5 tombent tous deux a l'extremite gauche).
				///    **Un critere en pixels seul n'aurait pas pu rougir.**
				float32 valeur = 0.f;
				bool aValeur = false;
				/// Vrai si un champ de saisie est VIDE -- donc si ce qu'on voit
				/// dedans est une invite, et non une saisie.
				bool champVide = false;
		};

		/// Le releve d'un montage. Tout y est compte et nomme : un banc qui n'a
		/// monte aucun widget doit annoncer autre chose qu'un banc qui les a tous
		/// montes.
		struct NkGuiMonteRapport {
				uint32 sections = 0;		   ///< sections de premier niveau lues
				uint32 widgets = 0;			   ///< blocs de role widget rencontres
				uint32 montes = 0;			   ///< ceux qui ont appele une fonction NKGui
				uint32 behaviors = 0;		   ///< blocs de section `behavior`
				uint32 animations = 0;		   ///< blocs de section `animation`
				uint32 rolesInconnus = 0;	   ///< hors vocabulaire du document 7
				uint32 apparencesLues = 0;	   ///< blocs `appearance` rencontres
				/// ⚠️ CE QUE L'APPARENCE AU REPOS A REELLEMENT PEINT (2026-09-17). Le
				///    monteur la COMPTAIT sans jamais l'appliquer : une demande de vert
				///    pur rendait 0 pixel vert sur 700 000 lus. Deux compteurs, pas un :
				///    « lue » ne dit pas « honoree ».
				uint32 apparencesPeintes = 0;
				/// ⚠️ ET CELLES QU'ON NE SAIT PAS PEINDRE -- un `fill` sur un role sans
				///    surface (`Group`, `Spacer`, `Text`...), ou une propriete que ce
				///    monteur ne rend pas (`radius`, `shadow`, `font`, `stroke`). Un repli
				///    MUET ferait passer un document a moitie honore pour un document
				///    honore : celui-ci se compte.
				uint32 apparencesNonPeintes = 0;
				// ── LE DEBORDEMENT (2026-09-17) ──────────────────────────────
				/// ⚠️ PERSONNE NE REGARDAIT SI UN WIDGET SORT DE SA REGION. Le montage
				///    restait VERT pendant que l'image montrait un bouton a cheval sur le
				///    bord du panneau (mesure du 17/09 : un `Spacer` sans taille vaut 120 px
				///    en dur dans un flux horizontal, et pousse son voisin dehors). C'est la
				///    famille du texte coupe : *la mesure qui manquait*, pas le pixel.
				uint32 debordements = 0;
				/// ⚠️ UN `Spacer` SANS `size` VEUT DIRE « PRENDS LA PLACE QUI RESTE », et
				///    ce monteur ne sait pas l'honorer : pousser le voisin jusqu'au bord
				///    demanderait de connaitre SA largeur avant de le monter, donc deux
				///    passes. Il ne lui invente plus 120 px pour autant. Compte, et nomme.
				uint32 espacesFlexibles = 0;
				uint32 etatsNonAppliques = 0;  ///< `appearance(Hover)` et consorts
				// ── LES COULEURS QUE LE DOCUMENT DEMANDE (2026-09-26) ────────
				/// 🔴 POURQUOI CE RELEVE EXISTE. `apparencesPeintes` et
				///    `apparencesNonPeintes` comptent des APPELS, pas des PIXELS. Le
				///    26/09, `apparencesNonPeintes` est tombe de 17 a 0 pendant que le
				///    compteur de contenu de la sonde ne bougeait pas d'un pixel : rien,
				///    dans le rapport, ne permettait de dire si une couleur DEMANDEE
				///    s'etait posee quelque part. *Un compteur vert n'est pas un rendu
				///    juste.*
				///
				/// ⚠️ CE N'EST PAS UN VERDICT, C'EST UNE LISTE. Le monteur ne sait pas
				///    lire l'image ; il dit seulement QUELLE couleur le document reclame
				///    et COMBIEN de fois. Celui qui rasterise compte ensuite ces
				///    couleurs-la dans les pixels. Les deux moities vivent dans deux
				///    binaires differents, et c'est ce qui donne sa valeur a l'accord.
				///
				/// ⚠️ ET LA CAPACITE EST FINIE, DONC LE DEBORDEMENT SE COMPTE. Une table
				///    pleine qui laisse tomber la 33e couleur EN SILENCE ferait declarer
				///    « toutes les couleurs sont peintes » a un relevé incomplet.
				static const uint32 kCouleursMax = 48u;
				uint32 couleursDistinctes = 0;					 ///< entrees remplies
				uint32 couleurDemandee[kCouleursMax] = {};		 ///< RGBA empaquetee (R<<24…)
				uint32 couleurOccurrences[kCouleursMax] = {};	 ///< combien de widgets la reclament
				uint32 couleursDebordees = 0;					 ///< au-dela de la capacite
				/// ⚠️ UNE COULEUR TRANSLUCIDE NE SE RETROUVE PAS TELLE QUELLE DANS
				///    L'IMAGE : elle se melange a ce qui est dessous, et la chercher au
				///    pixel pres rendrait zero pour un rendu PARFAITEMENT juste. Elle est
				///    donc ECARTEE du releve -- et COMPTEE ici, pour que « ecartee » ne
				///    veuille jamais dire « oubliee ».
				uint32 couleursTranslucides = 0;
				/// Enregistre une couleur reclamee par un `fill`. Idempotent par couleur :
				/// c'est le COMPTE d'occurrences qui monte, pas la liste.
				void NoterCouleurDemandee(uint8 r, uint8 v, uint8 b) {
					const uint32 cle = ((uint32)r << 16) | ((uint32)v << 8) | (uint32)b;
					for (uint32 i = 0; i < couleursDistinctes; ++i)
						if (couleurDemandee[i] == cle) {
							++couleurOccurrences[i];
							return;
						}
					if (couleursDistinctes >= kCouleursMax) {
						++couleursDebordees;
						return;
					}
					couleurDemandee[couleursDistinctes] = cle;
					couleurOccurrences[couleursDistinctes] = 1u;
					++couleursDistinctes;
				}
				// ── LE PLACEMENT PAR WIDGET (2026-09-14) ─────────────────────
				// `pos` est l'interrupteur : un widget qui l'ecrit est POSE.
				uint32 poses = 0;			 ///< widgets qui ecrivent `pos`
				uint32 posesHonores = 0;	 ///< ceux dont le rectangle monte EST celui-la
				/// ⚠️ UN RECTANGLE POSE MAIS NON CONSOMME. `SetNextItemRect` ne vaut
				///    que pour le PROCHAIN widget auto-place ; un role qui ne passe
				///    pas par `NextItemRect` le laisserait ARME pour le suivant. On
				///    le desarme et on le COMPTE, plutot que de contaminer le voisin.
				uint32 posesNonConsommes = 0;
				/// Les FEUILLES qui ecrivent `pos` -- le seul denominateur qui ait un
				/// sens face a `posesHonores`. Un conteneur pose son rectangle dans son
				/// propre `case` et n'atteint jamais la comparaison.
				uint32 posesFeuilles = 0;

				/// Le PREMIER placement qui ne rend pas le rectangle demande, avec
				/// ses chiffres. Un compteur seul ne dit pas si le defaut est dans
				/// le placement ou dans le controle ; ces deux rectangles, si.
				bool aDesaccordPose = false;
				NkString desaccordId;
				NkRect desaccordDemande = {0.f, 0.f, 0.f, 0.f};
				NkRect desaccordMonte = {0.f, 0.f, 0.f, 0.f};
				// ── LA ZONE HOTE (2026-09-17) ────────────────────────────────
				/// Les `Host` rencontres : une zone se compte, remplie ou non.
				uint32 hotes = 0;
				/// ⚠️ CELLES QUE PERSONNE N'A REMPLIES. Ce compteur est la raison d'etre
				///    du marqueur : une zone vide qui ne se compte pas se fait prendre
				///    pour un fond, et le document a l'air monte alors qu'il manque sa
				///    partie la plus importante.
				uint32 hotesNonRemplis = 0;
				/// Les listes qui NOMMENT une source (`bind`) et que personne n'a remplie.
				/// Meme raison que `hotesNonRemplis` : un arbre vide se fait prendre pour un
				/// arbre sans elements, et le document a l'air monte alors qu'il manque sa moitie.
				uint32 listesNonRemplies = 0;
				// ── LES MENUS (2026-09-26) ───────────────────────────────────
				/// Les `MenuBar` montees. Zero alors que le document en ecrit une =
				/// la bande n'a pas pris (region degeneree, ou `BeginMenuBar` refuse).
				uint32 barresMenu = 0;
				/// Les `Menu` (titres) rencontres, ouverts ou non.
				uint32 menus = 0;
				/// ⚠️ CEUX QUI ETAIENT OUVERTS, et ce compteur existe parce que le
				///    contenu d'un menu FERME ne se monte pas -- c'est correct, mais
				///    sans ce chiffre un document dont aucun menu n'est deroule serait
				///    indiscernable d'un document dont les menus sont vides.
				uint32 menusOuverts = 0;
				/// Les `MenuItem` reellement soumis (donc dans un menu ouvert).
				uint32 elementsMenu = 0;
				/// ⚠️ UN `MenuItem` HORS D'UN MENU. NKGui ne sait pas le dessiner
				///    ailleurs que dans une chaine de menus ; le document peut
				///    l'ecrire n'importe ou. On le compte au lieu de le poser dans le
				///    flux, ou il aurait l'air d'un bouton qui n'en est pas un.
				uint32 elementsMenuHorsMenu = 0;
				/// ⚠️ LES ATTRIBUTS QUE LE FORMAT ECRIT ET QUE CE MONTEUR NE REND PAS.
				///    Aujourd'hui : le `sizes` d'une `Grid` (NKGui ne prend que le NOMBRE
				///    de colonnes). Un tel attribut ne fait pas REFUSER le document -- il
				///    le fait rendre AUTREMENT que demande, ce qui est le silence le plus
				///    couteux des deux. Il se compte donc, comme `etatsNonAppliques`.
				uint32 attributsNonHonores = 0;
				/// Les ZONES d'ancrage declarees par le document.
				uint32 zones = 0;
				/// ⚠️ CELLES QUE L'APPLICATION NE FOURNIT PAS. C'est le troisieme pire
				///    resultat du format -- un document dont les noms de zone ne
				///    correspondent a rien -- et il ne doit jamais etre silencieux.
				uint32 zonesSansPanneau = 0;
				uint32 modales = 0;			 ///< `Window { modal = true }` rencontres
				/// Les drapeaux d'INTERACTION (`NoMove`, `NoResize`, `NoClose`,
				/// `NoScrollbar`) : ce monteur monte l'etat AU REPOS, il n'a aucune
				/// interaction a empecher. Comptes, jamais fait semblant.
				uint32 flagsNonAppliques = 0;
				/// Combien d'images ont VU une fenetre se deplacer sous le geste. Ce
				/// n'est pas « combien de fenetres sont deplacables » : c'est le
				/// GESTE, et il vaut zero tant que personne ne traîne rien.
				uint32 fenetresDeplacees = 0;
				// ── LA SECTION `geometry` (document 2 §1 et §3) ───────────────
				uint32 formes = 0;			 ///< blocs `shape` rencontres
				uint32 formesPeintes = 0;	 ///< celles dont la nature sait se peindre
				uint32 formesNonPeintes = 0; ///< `path`, `frame`, `image` sans texture
				NkVector<NkGuiMonteItem> items;
				NkVector<NkString> nomsSections;
		};

		// =====================================================================
		//  L'ETAT -- il vit HORS du document, parce que le document DECRIT
		// =====================================================================
		/**
		 * @brief Les valeurs que les widgets editent (case cochee, valeur de
		 *        curseur, tampon de saisie), rangees par cle stable.
		 *
		 * ⚠️ DEUX PASSES, ET CE N'EST PAS UN CONFORT. `Checkbox` prend un `bool &`,
		 *    `SliderFloat` un `float32 &`. Si le magasin grandissait PENDANT le
		 *    montage, `NkVector` se reallouerait et ces references pointeraient
		 *    dans de la memoire liberee -- un defaut qui ne se voit pas tout de
		 *    suite et qui se voit tres mal ensuite. `Preparer()` recense donc
		 *    toutes les cles AVANT que la moindre reference ne soit prise, et le
		 *    montage n'ajoute plus rien. Pas de plafond, pas de capacite devinee.
		 */
		class NkGuiMonteEtat {
			public:
				struct Entree {
						NkString cle;
						bool b = false;
						float32 f = 0.f;
						char texte[256] = {0};
						/// Le deplacement acquis par un geste, en pixels, relatif a la
						/// position que le DOCUMENT donne. Une fenetre traînee vit ici :
						/// le fichier reste la position de depart, jamais la courante.
						NkVec2 deplacement{0.f, 0.f};
						/// Vrai une fois la valeur initiale posee depuis le document.
						/// Sans lui, chaque trame ecraserait ce que l'utilisateur a
						/// change -- un champ que le document reinitialise sans cesse
						/// n'est pas editable.
						bool initialise = false;
				};

				/// Recense une cle (phase 1). Sans effet si elle existe deja.
				void Declarer(NkStringView cle) noexcept {
					if (Trouver(cle) >= 0)
						return;
					Entree e;
					e.cle = NkString(cle);
					mE.PushBack(e);
				}

				/// Rend l'entree (phase 2). Jamais nulle APRES Declarer ; nulle sinon,
				/// et l'appelant doit le traiter plutot que de le supposer.
				Entree *Get(NkStringView cle) noexcept {
					const int32 i = Trouver(cle);
					return i < 0 ? nullptr : &mE[(uint32)i];
				}

				/// L'entree de rang `i`, ou nullptr hors bornes.
				/// ⚠️ AJOUTE LE 26/09 POUR `Changed`. NKGui ne dit jamais « cette
				///    valeur vient de changer » : elle dit ce qu'elle vaut.
				///    L'evenement se DEDUIT donc d'une comparaison avec l'image
				///    precedente, et comparer demande de PARCOURIR. `Taille()`
				///    existait sans rien pour lire ce qu'elle compte.
				const Entree *A(uint32 i) const noexcept {
					return (i < (uint32)mE.Size()) ? &mE[i] : nullptr;
				}

				uint32 Taille() const noexcept {
					return (uint32)mE.Size();
				}

			private:
				int32 Trouver(NkStringView cle) const noexcept {
					for (uint32 i = 0; i < (uint32)mE.Size(); ++i) {
						if (mE[i].cle.Compare(NkString(cle)) == 0)
							return (int32)i;
					}
					return -1;
				}
				NkVector<Entree> mE;
		};

		// =====================================================================
		//  LIRE LES VALEURS -- le lecteur type ce qui tient en un jeton, pas plus
		// =====================================================================
		/// Le `$body` d'un bloc, ou nullptr. (Meme forme que `NkGCorps` de la
		/// validation : la structure de l'archive est la, elle ne se redecouvre pas.)
		inline const NkArchiveNode *NkGMonteCorps(const NkArchive &bloc) noexcept {
			const NkArchiveNode *b = bloc.FindNode(NkStringView(NkGuiArchive::KeyBody()));
			return (b && b->IsArray()) ? b : nullptr;
		}

		inline float32 NkGNombre(const NkArchive &b, const char *cle, float32 defaut) noexcept {
			float32 v = defaut;
			if (b.GetFloat32(NkStringView(cle), v))
				return v;
			return defaut;
		}

		inline bool NkGBooleen(const NkArchive &b, const char *cle, bool defaut) noexcept {
			bool v = defaut;
			if (b.GetBool(NkStringView(cle), v))
				return v;
			return defaut;
		}

		/// Le texte d'une propriete. Une CHAINE rend son contenu ; un JETON NU rend
		/// son lexeme (`Start`, `ui.filtre`) -- c'est ce que le consommateur veut
		/// dans les deux cas.
		inline NkString NkGTexte(const NkArchive &b, const char *cle, const char *defaut) noexcept {
			NkString out;
			if (b.GetString(NkStringView(cle), out))
				return out;
			return NkString(defaut);
		}

		inline bool NkGA(const NkArchive &b, const char *cle) noexcept {
			return b.FindNode(NkStringView(cle)) != nullptr;
		}

		/// Les nombres presents dans un lexeme, dans l'ordre d'ecriture. Les
		/// delimiteurs ne l'interessent pas : `[1, 2, 3]` et `(120, 80)` se lisent
		/// par la meme porte. Rend le nombre d'elements ECRITS (qui peut depasser
		/// `max` -- l'appelant sait alors qu'il en a perdu).
		inline uint32 NkGNombresDansLexeme(NkStringView lex, float32 *out, uint32 max) noexcept {
			const char *p = lex.Data();
			const uint32 len = (uint32)lex.Size();
			uint32 compte = 0, i = 0;
			while (i < len) {
				const char c = p[i];
				const bool debut = (c >= '0' && c <= '9')
								   || (c == '-' && i + 1 < len && p[i + 1] >= '0' && p[i + 1] <= '9');
				if (!debut) {
					++i;
					continue;
				}
				float32 signe = 1.f;
				if (c == '-') {
					signe = -1.f;
					++i;
				}
				float32 ent = 0.f;
				while (i < len && p[i] >= '0' && p[i] <= '9') {
					ent = ent * 10.f + (float32)(p[i] - '0');
					++i;
				}
				if (i < len && p[i] == '.') {
					++i;
					float32 d = 0.1f;
					while (i < len && p[i] >= '0' && p[i] <= '9') {
						ent += (float32)(p[i] - '0') * d;
						d *= 0.1f;
						++i;
					}
				}
				if (compte < max)
					out[compte] = signe * ent;
				++compte;
			}
			return compte;
		}

		/// Les nombres d'une liste ecrite en JETON NU (`values = [1, 2, 3, 5, 8]`).
		/// Le lecteur ne construit pas de tableau -- il le dit -- donc le
		/// consommateur analyse le lexeme.
		inline uint32 NkGListeNombres(const NkArchive &b, const char *cle, float32 *out,
									  uint32 max) noexcept {
			const NkArchiveNode *n = b.FindNode(NkStringView(cle));
			if (!n)
				return 0u;
			return NkGNombresDansLexeme(n->Lexeme(), out, max);
		}

		/// Le nombre d'elements d'une liste en jeton nu (`items = ["un","deux"]`).
		/// Compte les VIRGULES DE PREMIER NIVEAU plus une, crochets vides exceptes.
		inline uint32 NkGListeCompte(const NkArchive &b, const char *cle) noexcept {
			const NkArchiveNode *n = b.FindNode(NkStringView(cle));
			if (!n)
				return 0u;
			const NkString lex(n->Lexeme());
			const char *p = lex.Data();
			const uint32 len = (uint32)lex.Size();
			uint32 profondeur = 0, virgules = 0, contenu = 0;
			bool chaine = false;
			for (uint32 i = 0; i < len; ++i) {
				const char c = p[i];
				if (chaine) {
					if (c == '\\') {
						++i;
						continue;
					}
					if (c == '"')
						chaine = false;
					continue;
				}
				if (c == '"') {
					chaine = true;
					++contenu;
					continue;
				}
				if (c == '[' || c == '(' || c == '{') {
					++profondeur;
					continue;
				}
				if (c == ']' || c == ')' || c == '}') {
					if (profondeur > 0)
						--profondeur;
					continue;
				}
				if (c == ',' && profondeur == 1) {
					++virgules;
					continue;
				}
				if (c != ' ' && c != '\t' && profondeur == 1)
					++contenu;
			}
			if (contenu == 0)
				return 0u;
			return virgules + 1u;
		}

		/// Retire les espaces de bord d'un element de liste (`[ Scene , Rendu ]`).
		inline NkString NkGCouper(const NkString &s) noexcept {
			uint32 a = 0, b = (uint32)s.Size();
			while (a < b && (s.Data()[a] == ' ' || s.Data()[a] == '\t'))
				++a;
			while (b > a && (s.Data()[b - 1u] == ' ' || s.Data()[b - 1u] == '\t'))
				--b;
			NkString r;
			for (uint32 i = a; i < b; ++i)
				r += s.Data()[i];
			return r;
		}

		/// LES LIBELLES d'une liste en jeton nu : `tabs = [Scene, "Materiaux", Rendu]`.
		///
		/// ⚠️ DU COTE DU CONSOMMATEUR, ET C'EST LA REGLE DE CE FICHIER. Le lecteur le dit
		///    lui-meme : « un outil qui veut iterer dessus doit encore l'analyser lui-meme ».
		///    `NkGListeCompte` rend un NOMBRE -- ce dont `Dropdown` se contente. Pour appeler
		///    `TabBar(ctx, id, labels, count)` il faut les CHAINES. **Le format n'est pas
		///    touche** : ses listes restent des jetons nus, c'est ici qu'on les lit.
		///
		/// Accepte les elements nus (`Scene`) et les chaines (`"Materiaux"`). Les
		/// espaces de bord sont retires ; une liste vide rend zero element.
		inline uint32 NkGListeChaines(const NkArchive &b, const char *cle, NkVector<NkString> &out) noexcept {
			out.Clear();
			const NkArchiveNode *n = b.FindNode(NkStringView(cle));
			if (!n)
				return 0u;
			const NkString lex(n->Lexeme());
			const char *p = lex.Data();
			const uint32 len = (uint32)lex.Size();
			uint32 profondeur = 0;
			bool chaine = false;
			NkString courant;
			bool aDuContenu = false;
			for (uint32 i = 0; i < len; ++i) {
				const char c = p[i];
				if (chaine) {
					if (c == '\\' && i + 1u < len) {
						++i;
						courant += p[i];
						continue;
					}
					if (c == '"') {
						chaine = false;
						continue;
					}
					courant += c;
					aDuContenu = true;
					continue;
				}
				if (c == '"') {
					chaine = true;
					aDuContenu = true;
					continue;
				}
				if (c == '[' || c == '(' || c == '{') {
					++profondeur;
					continue;
				}
				if (c == ']' || c == ')' || c == '}') {
					if (profondeur > 0)
						--profondeur;
					if (profondeur == 0 && aDuContenu) {
						out.PushBack(NkGCouper(courant));
						courant = NkString();
						aDuContenu = false;
					}
					continue;
				}
				if (c == ',' && profondeur == 1) {
					out.PushBack(NkGCouper(courant));
					courant = NkString();
					aDuContenu = false;
					continue;
				}
				if (profondeur >= 1) {
					courant += c;
					if (c != ' ' && c != '\t')
						aDuContenu = true;
				}
			}
			return (uint32)out.Size();
		}

		// =====================================================================
		//  LE PLACEMENT EST UNE PROPRIETE DU CONTENEUR -- tranche le 2026-09-14
		// =====================================================================
		//
		//  Rodolf : « on dois pouvoir avoir du placement absolut comme non absolut
		//  ca va dependre de l'utilisateur », puis « au vu de son parent, un
		//  conteneur ne pourra jamais porter les deux. »
		//
		//  D'ou la regle, et elle tient en une ligne : **un conteneur declare
		//  `placement = absolute`, et alors SES enfants directs portent `pos`.**
		//  Sans cette declaration, le conteneur est en FLUX -- le defaut, inchange.
		//
		//  ⚠️ LE MECANISME EXISTAIT DEJA, ET IL N'A PAS ETE ECRIT POUR CECI.
		//     `NkGuiContext::SetNextItemRect` est la depuis le 2026-08-18, pour
		//     NK3DModeler : « un rectangle POSE pour le PROCHAIN widget seulement,
		//     consomme par NextItemRect ». Le FORMAT n'avait pas le mot ; NKGui
		//     avait deja la porte. On ne construit donc pas un second placement.
		//
		//  ⚠️ LES COORDONNEES SONT RELATIVES A LA REGION DU CONTENEUR. C'est ce
		//     qui fait qu'un dialogue pose EMPORTE son contenu quand on le deplace.
		//     A la racine, la region est la vue : les coordonnees y sont donc celles
		//     de l'ecran, et `03_virgule_vecteur_couleur` ne bouge pas d'un pixel.
		
		/// Vrai si ce bloc declare `placement = absolute`. Le defaut est le FLUX.
		inline bool NkGuiEstAbsolu(const NkArchive &b) noexcept {
			const NkArchiveNode *n = b.FindNode(NkStringView("placement"));
			if (!n)
				return false;
			NkStringView v = n->Lexeme();
			// L'etiquette peut s'ecrire nue ou entre guillemets : le schema dit 'e'.
			if (v.Size() >= 2u && v.Data()[0] == '\"')
				return NkGMotEgal(NkStringView(v.Data() + 1, (uint32)v.Size() - 2u), "absolute");
			return NkGMotEgal(v, "absolute");
		}
		
		/// Le placement ECRIT par un widget, exprime dans la region `reg` qui le
		/// contient. Rend faux quand il n'y a pas de `pos` -- c'est-a-dire dans
		/// l'immense majorite des cas, et c'est le defaut.
		///
		/// ⚠️ LA TAILLE PAR DEFAUT N'EST PAS INVENTEE : c'est celle que le flux
		///    aurait donnee (la largeur restante, `ItemHeight()` en hauteur). `pos`
		///    seul deplace donc un widget SANS le redimensionner, ce qui est
		///    exactement ce qu'on attend de « pose au point (x, y) ».
		struct NkGuiPlacement {
			bool pose = false;
			bool aTaille = false;
			NkRect rect{0.f, 0.f, 0.f, 0.f};
		};
		
		// =====================================================================
		//  LES TAILLES RELATIVES -- « cette colonne fait 16 % de son parent,
		//  au minimum 180 px »
		// =====================================================================
		//  ⚠️ POURQUOI ELLES EXISTENT, ET CE QUE MESURE L'INVENTAIRE DU 17/09 :
		//     `pos` et `size` sont en PIXELS ABSOLUS. Un document qui decrirait
		//     l'interface de NK3DModeler avec eux la FIGERAIT a une seule taille de
		//     fenetre -- alors que sa disposition reelle est ecrite en fractions
		//     (`NkLayout::Compute(W, H, fLeft = 0.16f, fRight = 0.29f)`). C'est ce
		//     qui separe un ecran de demonstration d'une fenetre d'editeur.
		//
		//  ⚠️ ET C'EST STRICTEMENT ADDITIF. `size` garde exactement son sens : il
		//     n'agit qu'avec `pos`. Les 226 noeuds absolus du depot ne changent pas
		//     d'un pixel -- c'est mesure par la non-regression du banc, pas espere.
		//
		//  Une composante <= 0 veut dire « cet axe n'est pas contraint » : on peut
		//  ne dire que la largeur, `sizeRel = (0.16, 0)`.
		struct NkGuiTailleRel {
				bool aW = false, aH = false;
				float32 w = 0.f, h = 0.f;
		};

		inline NkGuiTailleRel NkGuiLireTailleRelative(const NkArchive &b, const NkRect &region) noexcept {
			NkGuiTailleRel t;
			// MUTATION DE BANC, NK_TAILLE_MUTATION=absolu : `sizeRel` est ignore, et la
			// disposition redevient celle du flux. Elle sert a prouver que le critere des
			// deux fenetres teste quelque chose ; sans la variable, rien ne change.
			static const bool kIgnorer = []() {
				const char *v = getenv("NK_TAILLE_MUTATION");
				return v && v[0] == 'a';
			}();
			if (kIgnorer)
				return t;
			const NkArchiveNode *nr = b.FindNode(NkStringView("sizeRel"));
			if (!nr)
				return t;
			float32 v[4];
			if (NkGNombresDansLexeme(nr->Lexeme(), v, 4u) < 2u)
				return t; // `sizeRel = 0.16` n'est pas un vecteur : la validation le dit
			float32 mn[4] = {0.f, 0.f, 0.f, 0.f};
			float32 mx[4] = {0.f, 0.f, 0.f, 0.f};
			const NkArchiveNode *nmn = b.FindNode(NkStringView("minSize"));
			const NkArchiveNode *nmx = b.FindNode(NkStringView("maxSize"));
			if (nmn)
				(void)NkGNombresDansLexeme(nmn->Lexeme(), mn, 4u);
			if (nmx)
				(void)NkGNombresDansLexeme(nmx->Lexeme(), mx, 4u);
			if (v[0] > 0.f) {
				t.aW = true;
				t.w = region.w * v[0];
				if (mn[0] > 0.f && t.w < mn[0])
					t.w = mn[0];
				if (mx[0] > 0.f && t.w > mx[0])
					t.w = mx[0];
			}
			if (v[1] > 0.f) {
				t.aH = true;
				t.h = region.h * v[1];
				if (mn[1] > 0.f && t.h < mn[1])
					t.h = mn[1];
				if (mx[1] > 0.f && t.h > mx[1])
					t.h = mx[1];
			}
			return t;
		}

		inline NkGuiPlacement NkGuiLirePlacement(const NkArchive &w, const NkRect &reg,
						float32 hauteurRangee, bool conteneur) noexcept {
			NkGuiPlacement pl;
			const NkArchiveNode *np = w.FindNode(NkStringView("pos"));
			if (!np)
				return pl;
			float32 v[4];
			if (NkGNombresDansLexeme(np->Lexeme(), v, 4u) < 2u)
				return pl; // `pos = 3` n'est pas un point : la validation le dit deja
			pl.pose = true;
			pl.rect.x = reg.x + v[0];
			pl.rect.y = reg.y + v[1];
			pl.rect.w = conteneur ? (reg.w - v[0]) : 0.f;
			pl.rect.h = conteneur ? (reg.h - v[1]) : hauteurRangee;
			const NkArchiveNode *ns = w.FindNode(NkStringView("size"));
			if (ns && NkGNombresDansLexeme(ns->Lexeme(), v, 4u) >= 2u) {
				pl.aTaille = true;
				pl.rect.w = v[0];
				pl.rect.h = v[1];
			}
			return pl;
		}
		
		/// Vrai si le lexeme de `flags` contient CE drapeau. On travaille sur le
		/// TEXTE : `flags = NoTitleBar | NoMove` est un JETON NU, le lecteur ne
		/// construit aucune liste (il le dit lui-meme), donc c'est au CONSOMMATEUR
		/// de l'analyser -- exactement comme `items` et `values` plus haut.
		inline bool NkGuiDrapeau(const NkArchive &b, const char *nom) noexcept {
			const NkArchiveNode *n = b.FindNode(NkStringView("flags"));
			if (!n)
				return false;
			const NkStringView lex = n->Lexeme();
			uint32 l = 0;
			while (nom[l])
				++l;
			if (l == 0u || (uint32)lex.Size() < l)
				return false;
			for (uint32 i = 0; i + l <= (uint32)lex.Size(); ++i) {
				uint32 k = 0;
				while (k < l && lex.Data()[i + k] == nom[k])
					++k;
				if (k != l)
					continue;
				// ⚠️ ET IL FAUT VERIFIER LA FRONTIERE : sans elle, `NoMove` serait
				//    trouve dans un hypothetique `NoMoveX`. Un drapeau est un
				//    identifiant ENTIER, pas une sous-chaine.
				const char c = (i + l < (uint32)lex.Size()) ? lex.Data()[i + l] : ' ';
				const bool suite = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
								 || (c >= '0' && c <= '9') || c == '_';
				if (!suite)
					return true;
			}
			return false;
		}
		
		/// Une couleur `#RRGGBB` / `#RRGGBBAA` du format, en `NkColor`.
		/// ⚠️ PAS DE PARSEUR MAISON : `NkColorF::FromHex` existe dans NKMath et
		///    traite les deux longueurs. En reecrire un ici aurait donne deux
		///    lectures d'une meme couleur, qui finissent par diverger.
		// ─────────────────────────────────────────────────────────────────────
		//  LA SEULE PORTE QUI LIT UNE COULEUR — hexadécimal OU jeton `@nom`
		// ─────────────────────────────────────────────────────────────────────
		//  🔴 IL Y EN AVAIT DEUX (mesure du 27/09). Celle-ci, et
		//     `NkGuiCouleurDepuisLexeme` dans `NkGuiInteraction.h` : deux
		//     implémentations indépendantes de la même syntaxe, l'une par
		//     `NkColorF::FromHex`, l'autre par `NkParseHex` et des décalages à la
		//     main. Elles s'accordaient ; rien ne les y obligeait. La seconde est
		//     devenue un renvoi vers celle-ci. *Deux analyseurs du même texte
		//     finissent par ne plus être d'accord sur ce qu'il dit.*
		//
		//  ⚠️ `@nom` EST RÉSOLU ICI, PAS DANS UNE PASSE SUR L'ARCHIVE (P1). Une
		//     passe aurait remplacé le jeton par sa valeur dans le document, qui
		//     aurait donc perdu son thème à la première sauvegarde — et changer
		//     de thème aurait exigé de tout relire. Voir l'en-tête de
		//     `NkGuiJetons.h`.
		//
		//  ⚠️ UN JETON INCONNU REND FAUX, il ne rend pas NOIR. Une couleur
		//     absente se compte (`apparencesNonPeintes`) et se voit ; un noir
		//     inventé se confondrait avec un choix.
		inline bool NkGuiCouleur(NkStringView lex, NkColor &out) noexcept {
			const uint32 n = (uint32)lex.Size();
			if (n == 0u)
				return false;
			if (lex.Data()[0] == '@') {
				const NkGuiTableJetons *t = NkGuiJetonsPoses();
				return t && t->Resoudre(lex, out);
			}
			if ((n != 7u && n != 9u) || lex.Data()[0] != '#')
				return false;
			char buf[10];
			for (uint32 i = 0; i < n; ++i)
				buf[i] = lex.Data()[i];
			buf[n] = '\0';
			out = math::NkColorF::FromHex(buf).ToColor();
			return true;
		}
		
		/// ── L'APPARENCE AU REPOS, LUE UNE FOIS ────────────────────────────────────
		/// Ce que le document ecrit dans `appearance { ... }` SANS parenthese d'etat.
		///
		/// ⚠️ LE REPOS SEUL, ET C'EST UNE LIMITE ASSUMEE. `appearance(Hover)` decrit
		///    un etat que ce monteur ne peut pas atteindre -- il monte l'etat au repos,
		///    il n'a ni souris ni focus. Il les compte deja (`etatsNonAppliques`) ; les
		///    peindre serait mentir sur ce qu'on voit.
		struct NkGuiApparenceRepos {
				NkColor fond{0, 0, 0, 0};
				NkColor encre{0, 0, 0, 0};
				float32 rayon = -1.f;
				bool aFond = false;
				bool aEncre = false;
				/// Les proprietes ECRITES que ce monteur ne rend pas. Comptees, jamais tues.
				uint32 nonRendues = 0;
		};

		/// Le lexeme d'etat d'un bloc dit-il LE REPOS ? `$state` porte la parenthese
		/// entiere telle qu'ecrite : `(Normal)`, `(  Normal  )`, `(Hover)`.
		inline bool NkGuiEtatEstRepos(NkStringView lexeme) noexcept {
			// `$state` porte la parenthese entiere telle qu'ecrite :
			// `(Normal)`, `(  Normal  )`, `(Hover)`.
			for (uint32 i = 0; i + 5u < (uint32)lexeme.Size() + 1u; ++i) {
				if (lexeme.Data()[i] == 'N' && i + 6u <= (uint32)lexeme.Size()) {
					bool ok = true;
					const char *m = "Normal";
					for (uint32 j = 0; j < 6u; ++j)
						if (lexeme.Data()[i + j] != m[j])
							ok = false;
					if (ok)
						return true;
				}
			}
			return false;
		}

		inline NkGuiApparenceRepos NkGuiLireApparenceRepos(const NkArchive &w) noexcept {
			NkGuiApparenceRepos a;
			const NkArchiveNode *corps = NkGMonteCorps(w);
			if (!corps)
				return a;
			for (uint32 k = 0; k < (uint32)corps->array.Size(); ++k) {
				if (!corps->array[k].IsObject() || !corps->array[k].object)
					continue;
				const NkArchive &ap = *corps->array[k].object;
				if (!NkGMotEgal(NkGuiArchive::TypeOf(ap), "appearance"))
					continue;
				// ⚠️ LA PARENTHESE D'ETAT EST LE FILTRE, et elle se lit par la MEME porte
				//    que le compteur `etatsNonAppliques` juste au-dessus. Deux lectures de
				//    l'etat finiraient par ne plus etre d'accord sur ce qu'est « le repos ».
				const NkStringView st = NkGuiArchive::StateOf(ap);
				if (st.Size() > 0 && !NkGuiEtatEstRepos(st))
					continue;
				if (NkGA(ap, "radius"))
					a.rayon = NkGNombre(ap, "radius", -1.f);
				if (NkGA(ap, "font"))
					++a.nonRendues; // la chasse : le monteur n'a qu'une fonte
				const NkArchiveNode *c2 = NkGMonteCorps(ap);
				if (!c2)
					continue;
				for (uint32 j = 0; j < (uint32)c2->array.Size(); ++j) {
					if (!c2->array[j].IsObject() || !c2->array[j].object)
						continue;
					const NkArchive &sub = *c2->array[j].object;
					const NkStringView ts = NkGuiArchive::TypeOf(sub);
					const NkArchiveNode *nc = sub.FindNode(NkStringView("color"));
					// 🔴 UNE COULEUR ILLISIBLE DISPARAISSAIT SANS TRACE (27/09). Ces
					//    deux lignes rendaient `false` et personne ne comptait : un
					//    `fill` dont la couleur est mal écrite — ou, depuis P1, un
					//    jeton `@nom` que le thème ne connaît pas — laissait le
					//    widget EXACTEMENT comme s'il n'avait rien demandé. C'est
					//    la faute que tout ce fichier sert à rendre impossible :
					//    *un document rendu autrement que demandé n'a pas l'air
					//    cassé, il a l'air normal.*
					//
					//    Trouvé par le témoin de P1, qui exigeait le compteur avant
					//    d'avoir regardé s'il existait.
					if (NkGMotEgal(ts, "fill") && nc) {
						a.aFond = NkGuiCouleur(nc->Lexeme(), a.fond);
						if (!a.aFond)
							++a.nonRendues;
					} else if (NkGMotEgal(ts, "text") && nc) {
						a.aEncre = NkGuiCouleur(nc->Lexeme(), a.encre);
						if (!a.aEncre)
							++a.nonRendues;
					} else if (NkGMotEgal(ts, "shadow") || NkGMotEgal(ts, "stroke"))
						++a.nonRendues;
				}
			}
			return a;
		}

		// =====================================================================
		//  LE CROCHET D'EXECUTION -- et pourquoi il est ici et pas ailleurs
		// =====================================================================
		/**
		 * @brief Ce que le monteur laisse faire a qui veut EXECUTER le document.
		 *
		 * Le monteur monte l'ETAT AU REPOS, c'est ce que son en-tete promet et
		 * c'est ce qu'il continuera de faire seul. Mais quatre choses du format
		 * ne peuvent se faire qu'au moment PRECIS ou un widget se dessine :
		 *
		 *   - desactiver le widget (`enabled = false`) AVANT qu'il ne peigne,
		 *     parce que `BeginDisabled` est une pile et qu'elle se pose autour ;
		 *   - savoir QUEL widget du document NKGui est en train de peindre, pour
		 *     que le crochet de style (`ctx.styleFn`) puisse choisir la couleur
		 *     que le FICHIER declare pour l'etat courant ;
		 *   - pousser la donnee liee (`bind`) dans la valeur editee juste avant,
		 *     et relire ce que l'utilisateur en a fait juste apres ;
		 *   - peindre par-dessus ce que le widget ne sait pas peindre (l'anneau
		 *     de focus d'un champ de saisie : `InputText` n'a pas de crochet).
		 *
		 * ⚠️ C'EST UN CROCHET, PAS UNE SECONDE PORTE. Le monteur reste le seul a
		 *    monter ; ce qui suit ne fait que l'encadrer. `Monter` garde sa
		 *    signature a quatre arguments -- les 162 criteres de `NKGuiMonteTest`
		 *    l'appellent encore comme avant et restent la mesure de
		 *    non-regression.
		 *
		 * ⚠️ `Apres` EST APPELE MEME QUAND LE WIDGET SORT TOT. Les conteneurs
		 *    (`Window`, `VBox`, `HBox`, `Group`) quittent `MonterBloc` par un
		 *    `return` au milieu du `switch`. Un appariement ecrit a la main
		 *    aurait donc laisse une pile `BeginDisabled` ouverte sur le premier
		 *    conteneur desactive rencontre -- et la faute ne se serait vue que
		 *    beaucoup plus loin, sur un widget qui n'a rien demande. L'appariement
		 *    passe par un objet de pile (`Garde`), pas par de la discipline.
		 */
		struct NkGuiMonteHooks {
				virtual ~NkGuiMonteHooks() = default;
				/// Avant que le widget ne dessine. `e` peut etre nul (role sans etat).
				virtual void Avant(NkGuiContext &ctx, const NkArchive &w, NkStringView role,
								   NkGuiMonteEtat::Entree *e) noexcept {
					(void)ctx;
					(void)w;
					(void)role;
					(void)e;
				}
				/// Apres, quelle que soit la porte de sortie.
				virtual void Apres(NkGuiContext &ctx, const NkArchive &w, NkStringView role,
								   NkGuiMonteEtat::Entree *e) noexcept {
					(void)ctx;
					(void)w;
					(void)role;
					(void)e;
				}

				/// LA ZONE HOTE : l'hote peint `zone`, et rend VRAI s'il l'a fait.
				///
				/// ⚠️ LE DEFAUT EST `false`, ET C'EST VOULU. Un hote qui ne connait pas ce
				///    nom ne doit pas repondre oui : le monteur peindra alors son marqueur
				///    et comptera la zone comme non remplie. Repondre vrai sans peindre
				///    ferait disparaitre la zone en silence -- exactement ce que ce role
				///    existe pour empecher.
				virtual bool RemplirHote(NkGuiContext &ctx, const char *nom, const NkRect &zone) noexcept {
					(void)ctx;
					(void)nom;
					(void)zone;
					return false;
				}

				/// L'ANCRAGE : l'hote fournit-il un panneau portant CE NOM ?
				///
				/// ⚠️ LE NOM EST L'IDENTIFIANT DU PANNEAU, jamais son libelle affiche. Et le
				///    defaut est `false` : un hote qui ne connait pas ce nom ne doit pas
				///    repondre oui. Le monteur peindra alors le marqueur et comptera la zone
				///    -- une zone que personne ne fournit ne se fait pas passer pour un fond.
				virtual bool ZoneAncree(const char *nom, const NkRect &zone) noexcept {
					(void)nom;
					(void)zone;
					return false;
				}
		};

		// =====================================================================
		//  LE MONTEUR
		// =====================================================================
		class NkGuiMonteur {
			public:
				/// Phase 1 : recenser les cles d'etat de tout le document. A appeler
				/// AVANT `Monter` -- voir l'avertissement de `NkGuiMonteEtat`.
				static void Preparer(const NkArchive &doc, NkGuiMonteEtat &etat) noexcept {
					const NkArchiveNode *corps = NkGMonteCorps(doc);
					if (!corps)
						return;
					for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
						if (!corps->array[i].IsObject() || !corps->array[i].object)
							continue;
						const NkArchive &sec = *corps->array[i].object;
						if (!NkGMotEgal(NkGuiArchive::TypeOf(sec), "widgets"))
							continue;
						PreparerCorps(sec, etat);
					}
				}

				/// Phase 2 : monter le document dans `ctx`, au curseur courant.
				/// L'appelant a deja fait `ctx.BeginLayout(region)`.
				static void Monter(NkGuiContext &ctx, const NkArchive &doc, NkGuiMonteEtat &etat,
									   NkGuiMonteRapport &rap,
									   NkGuiMonteHooks *hooks = nullptr) noexcept {
					const NkArchiveNode *corps = NkGMonteCorps(doc);
					if (!corps)
						return;
					// ⚠️ LES CALQUES D'ABORD, QUEL QUE SOIT L'ORDRE DU FICHIER. Le
					//    document 2 §1 appelle `geometry` « les calques du canvas » : un
					//    calque qui passerait devant les widgets ne serait plus un calque.
					//    Une PASSE SEPAREE le garantit ; le peindre dans la boucle d'apres
					//    aurait rendu l'ordre de peinture dependant de l'ordre d'ecriture.
					for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
						if (!corps->array[i].IsObject() || !corps->array[i].object)
							continue;
					// ⚠️ LA PASSE `geometry` NE RECOIT PAS LE CROCHET, ET C'EST MOTIVE.
					//    Une `shape` est un CALQUE : ni role, ni etat, ni evenement. Le
					//    crochet sert a desactiver un widget, a lui choisir la couleur de
					//    son etat et a pousser sa donnee liee -- une forme n'a aucune des
					//    trois. L'appeler ici ferait courir `Avant`/`Apres` sur un objet
					//    qui n'est pas un widget, et fausserait les compteurs par etat.
					if (NkGMotEgal(NkGuiArchive::TypeOf(*corps->array[i].object), "geometry"))
						MonterGeometrie(ctx, *corps->array[i].object, rap);
					}
					for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
						if (!corps->array[i].IsObject() || !corps->array[i].object)
							continue;
						const NkArchive &sec = *corps->array[i].object;
						const NkStringView nom = NkGuiArchive::TypeOf(sec);
						++rap.sections;
						rap.nomsSections.PushBack(NkString(nom));
						if (NkGMotEgal(nom, "behavior")) {
							++rap.behaviors;
							continue;
						}
						if (NkGMotEgal(nom, "animation")) {
							++rap.animations;
							continue;
						}
						if (!NkGMotEgal(nom, "widgets"))
							continue;  // geometry, controller, callback, fonts, include
						// La racine de `widgets` est ABSOLUE par nature : rien ne la contient,
						// donc aucun conteneur ne peut y declarer son mode -- et le corpus
						// l'atteste (`03` pose son `Window` a la racine depuis le premier jour).
						MonterCorps(ctx, sec, etat, rap, 0u, false, /*parentAbsolu=*/true, hooks);
					}
				}

			private:
				// ── phase 1 ──────────────────────────────────────────────────
				// ── LES CALQUES (`geometry`) ─────────────────────────
				// Une forme ne touche NI le curseur NI la region : elle n'est dans aucun
				// flux, elle EST son rectangle. C'est ce qui la distingue d'un widget
				// pose -- un widget garde un role, un etat, un evenement ; un calque n'en
				// a aucun (document 3, a propos de `geometry` : « ce qui en ferait un
				// calque et non un widget, donc sans role, sans etat, sans evenement »).
				static void MonterGeometrie(NkGuiContext &ctx, const NkArchive &sec,
									NkGuiMonteRapport &rap) noexcept {
					const NkArchiveNode *c = NkGMonteCorps(sec);
					if (!c)
						return;
					for (uint32 k = 0; k < (uint32)c->array.Size(); ++k) {
						if (!c->array[k].IsObject() || !c->array[k].object)
							continue;
						const NkArchive &fo = *c->array[k].object;
						if (!NkGMotEgal(NkGuiArchive::TypeOf(fo), "shape"))
							continue; // la validation le signale ; le monteur ne devine pas
						++rap.formes;
						float32 v[4];
						NkRect r{0.f, 0.f, 0.f, 0.f};
						const NkArchiveNode *np = fo.FindNode(NkStringView("pos"));
						const NkArchiveNode *ns = fo.FindNode(NkStringView("size"));
						if (np && NkGNombresDansLexeme(np->Lexeme(), v, 4u) >= 2u) {
							r.x = v[0];
							r.y = v[1];
						}
						if (ns && NkGNombresDansLexeme(ns->Lexeme(), v, 4u) >= 2u) {
							r.w = v[0];
							r.h = v[1];
						}
						const NkString nature = NkGTexte(fo, "kind", "rect");
						const NkString id(NkGuiArchive::IdOf(fo));
						// La couleur : un JETON NU `#RRGGBB`. Faute de couleur ecrite, on prend
						// l'encre du THEME -- jamais un noir invente, invisible sur fond sombre.
						NkColor col = ctx.theme.text;
						const NkArchiveNode *nc = fo.FindNode(NkStringView("color"));
						if (nc)
							(void)NkGuiCouleur(nc->Lexeme(), col);
						bool peinte = true;
						if (NkGMotEgal(NkStringView(nature), "rect")) {
							ctx.DL().AddRectFilled(r, col, NkGNombre(fo, "radius", 0.f));
						} else if (NkGMotEgal(NkStringView(nature), "ellipse")) {
							ctx.DL().AddEllipseFilled({r.x + r.w * 0.5f, r.y + r.h * 0.5f}, r.w * 0.5f,
											   r.h * 0.5f, col);
						} else if (NkGMotEgal(NkStringView(nature), "text")) {
							const NkString txt = NkGTexte(fo, "text", "");
							if (txt.Size() > 0 && ctx.font && ctx.font->Valid())
								ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
												 {r.x, r.y + ctx.font->Ascent()}, txt.CStr(), col);
							else
								peinte = false;
						} else {
							// `image`, `path`, `frame` : la nature est CONNUE du format et ce
							// monteur ne sait pas la peindre (une image demande une texture, un
							// trace demande ses points). Comptee, jamais devinee.
							peinte = false;
						}
						if (peinte)
							++rap.formesPeintes;
						else
							++rap.formesNonPeintes;
						Noter(rap, id, NkStringView("shape"), r, 0u, false, false);
					}
				}
				
				static void PreparerCorps(const NkArchive &bloc, NkGuiMonteEtat &etat) noexcept {
					const NkArchiveNode *c = NkGMonteCorps(bloc);
					if (!c)
						return;
					for (uint32 k = 0; k < (uint32)c->array.Size(); ++k) {
						if (!c->array[k].IsObject() || !c->array[k].object)
							continue;
						const NkArchive &w = *c->array[k].object;
						const NkStringView t = NkGuiArchive::TypeOf(w);
						if (NkGEstApparence(t))
							continue;
						etat.Declarer(NkStringView(CleEtat(w)));
						PreparerCorps(w, etat);
					}
				}

				/// La cle d'etat d'un widget : son `bind` s'il en a un, sinon son id.
				/// Le `bind` d'abord, parce que deux widgets lies a la meme donnee
				/// doivent partager la valeur -- c'est ce que `bind` veut dire.
				static NkString CleEtat(const NkArchive &w) noexcept {
					const NkArchiveNode *b = w.FindNode(NkStringView("bind"));
					if (b)
						return NkString(b->Lexeme());
					return NkString(NkGuiArchive::IdOf(w));
				}

				// ── phase 2 ──────────────────────────────────────────────────
				static void MonterCorps(NkGuiContext &ctx, const NkArchive &bloc, NkGuiMonteEtat &etat,
									NkGuiMonteRapport &rap, uint32 prof, bool horizontal,
									bool parentAbsolu, NkGuiMonteHooks *hooks) noexcept {
					const NkArchiveNode *c = NkGMonteCorps(bloc);
					if (!c)
						return;
					for (uint32 k = 0; k < (uint32)c->array.Size(); ++k) {
						if (!c->array[k].IsObject() || !c->array[k].object)
							continue;
						MonterBloc(ctx, *c->array[k].object, etat, rap, prof, horizontal, parentAbsolu,
									   hooks);
					}
				}

				/// LES DEUX ENFANTS D'UN SEPARATEUR, chacun dans SA moitie.
				///
				/// ⚠️ ELLE EXISTE POUR UN SEUL APPELANT, ET C'EST ASSUME. `MonterCorps` monte
				///    tous les enfants dans la MEME region ; un separateur en donne une
				///    DIFFERENTE a chacun de ses deux premiers enfants. Plutot que d'ajouter
				///    un parametre a `MonterCorps` -- que ses six autres appelants auraient
				///    du passer a vide -- on ecrit la boucle qui fait autre chose.
				/// Les enfants au-dela du deuxieme sont montes dans la seconde moitie : un
				/// document qui en met trois n'en perd aucun, et le releve le montrera.
				static void MonterCorpsBorne(NkGuiContext &ctx, const NkArchive &bloc, NkGuiMonteEtat &etat,
											 NkGuiMonteRapport &rap, uint32 prof, NkGuiMonteHooks *hooks,
											 const NkRect &regionA, const NkRect &regionB) noexcept {
					const NkArchiveNode *c = NkGMonteCorps(bloc);
					if (!c)
						return;
					uint32 rang = 0;
					for (uint32 k = 0; k < (uint32)c->array.Size(); ++k) {
						if (!c->array[k].IsObject() || !c->array[k].object)
							continue;
						const NkGuiLayout sauve = ctx.layout;
						ctx.BeginLayout(rang == 0u ? regionA : regionB);
						MonterBloc(ctx, *c->array[k].object, etat, rap, prof, false, false, hooks);
						ctx.layout = sauve;
						++rang;
					}
				}

				/// Appariement du crochet par objet de PILE : `Apres` part meme quand
				/// le `switch` de `MonterBloc` sort par un `return` (les conteneurs le
				/// font tous). Voir l'avertissement de `NkGuiMonteHooks`.
				struct Garde {
						NkGuiMonteHooks *h = nullptr;
						NkGuiContext *ctx = nullptr;
						const NkArchive *w = nullptr;
						NkStringView role;
						NkGuiMonteEtat::Entree *e = nullptr;
						Garde(NkGuiMonteHooks *hooks, NkGuiContext &c, const NkArchive &bloc,
							  NkStringView r, NkGuiMonteEtat::Entree *ent) noexcept
							: h(hooks), ctx(&c), w(&bloc), role(r), e(ent) {
							if (h)
								h->Avant(*ctx, *w, role, e);
						}
						~Garde() {
							if (h)
								h->Apres(*ctx, *w, role, e);
						}
				};

				static void MonterBloc(NkGuiContext &ctx, const NkArchive &w, NkGuiMonteEtat &etat,
								   NkGuiMonteRapport &rap, uint32 prof, bool horizontal,
								   bool parentAbsolu, NkGuiMonteHooks *hooks) noexcept {
					const NkStringView t = NkGuiArchive::TypeOf(w);

					// L'apparence n'est pas un widget : on la compte, on ne la monte pas.
					if (NkGEstApparence(t)) {
						if (NkGMotEgal(t, "appearance")) {
							++rap.apparencesLues;
							if (NkGuiArchive::StateOf(w).Size() > 0
								&& !EtatEstRepos(NkGuiArchive::StateOf(w)))
								++rap.etatsNonAppliques;
						}
						return;
					}

					const NkGuiRole role = NkGuiRoleDepuisNom(t);
					if (role == NkGuiRole::Inconnu) {
						++rap.rolesInconnus;
						// ⚠️ IL NE DOIT PLUS EMPORTER SES ENFANTS. Le `return` sec qui
						//    vivait ici jetait le SOUS-ARBRE ENTIER, pas seulement le
						//    conteneur inconnu. Mesure du 17/09, deux documents qui ne
						//    different QUE par le role du conteneur :
						//      Panel > Scroll { 3 widgets } + 1  ->  2 montes, 1 utile
						//      Panel > Group  { 3 widgets } + 1  ->  6 montes, 4 utiles
						//    Quatre widgets sur six disparaissaient, et le format, lui,
						//    ACCEPTE le document (`Scroll` est au vocabulaire du
						//    validateur). 17 des 41 roles du format sont dans ce cas.
						//
						//    Perdre le contenu est le PIRE des deux echecs : le document
						//    ne se presente pas comme une erreur, il se presente comme un
						//    document plus pauvre -- et personne ne va chercher ce qui
						//    n'est plus la. On monte donc le sous-arbre dans le flux du
						//    parent, faute de savoir ce que le conteneur imposait.
						//
						// ⚠️ ET LE REPLI N'EST PAS MUET : `rolesInconnus` compte toujours,
						//    donc le verdict d'un document qui nomme l'inconnaissable
						//    reste REFUSE. Ce correctif change ce qu'on PERD, jamais ce
						//    qu'on ANNONCE.
						MonterCorps(ctx, w, etat, rap, prof + 1u, horizontal, parentAbsolu, hooks);
						return;
					}
					++rap.widgets;
					
					// ── LE PLACEMENT ─────────────────────────────────
					// `pos` n'est LU que si le parent est absolu. Sous un conteneur en flux,
					// des coordonnees ne sont pas ignorees en silence : le VALIDATEUR les
					// refuse (`E-PLACEMENT`). Le monteur n'a pas a deviner ce que le format
					// interdit -- il monte ce qui est juste.
					const bool estConteneur = (role == NkGuiRole::Window || role == NkGuiRole::Panel
										  || role == NkGuiRole::Group || role == NkGuiRole::VBox
										  || role == NkGuiRole::HBox
										  // La barre de menus est une BANDE : si le document la
										  // pose, c'est un rectangle de conteneur qu'il pose, pas
										  // la place d'un widget dans un flux.
										  || role == NkGuiRole::MenuBar);
					NkGuiPlacement pl;
					if (parentAbsolu)
						pl = NkGuiLirePlacement(w, ctx.layout.region, ctx.ItemHeight(), estConteneur);
					if (pl.pose)
						++rap.poses;
					// Ce que CE conteneur impose a SES enfants -- independant de ce que son
					// propre parent lui impose. Confondre les deux rendrait l'absolu contagieux
					// vers le bas, et un dialogue pose enfermerait toute sa descendance dans un
					// mode qu'elle n'a pas demande.
					const bool enfantsAbsolus = NkGuiEstAbsolu(w);
					// ⚠️ UNE FEUILLE POSEE PASSE PAR LA PORTE QUE NKGUI A DEJA.
					//    `SetNextItemRect` vaut pour LE PROCHAIN widget auto-place, et le
					//    curseur ne bouge pas. Un conteneur, lui, ouvre une REGION (plus bas) :
					//    ce n'est pas le meme geste, parce qu'un conteneur doit emporter ses
					//    enfants avec lui.
					// 🔴 L'ARMEMENT A CHANGE DE PLACE A LA FUSION, ET CE N'EST PAS COSMETIQUE.
					//    Il vivait ICI, avant que `e` ne soit lu. Depuis que le crochet
					//    d'execution existe, `Avant()` s'execute ENTRE ce point et le widget :
					//    tout ce qu'un crochet ferait qui consomme `NextItemRect` mangerait le
					//    rectangle a la place du widget qui l'attend.
					//    Mesure faite AVANT de deplacer : le seul `Avant()` du depot appelle
					//    `BeginDisabled`, qui ne fait qu'empiler un booleen -- **aujourd'hui**
					//    les deux places donnent le meme resultat. Un crochet est justement
					//    l'endroit ou quelqu'un d'autre ecrira du code demain. On arme donc
					//    AU PLUS PRES DU CONSOMMATEUR, juste avant le `switch`.
					
					const NkString id(NkGuiArchive::IdOf(w));
					const char *lbl = id.CStr();
					NkGuiMonteEtat::Entree *e = etat.Get(NkStringView(CleEtat(w)));
					// Le crochet encadre TOUT ce qui suit, sorties anticipees comprises.
					Garde garde(hooks, ctx, w, t, e);
					// ── L'ARMEMENT, AU PLUS PRES DU CONSOMMATEUR ──────────────────
					// `SetNextItemRect` ne vaut que pour le PROCHAIN widget auto-place. Rien
					// ne doit s'intercaler entre cette ligne et le `switch` -- surtout pas
					// un crochet, qui est du code ecrit par quelqu'un d'autre.
					if (pl.pose && !estConteneur)
						ctx.SetNextItemRect(pl.rect);
					// ── L'APPARENCE AU REPOS ──────────────────────────────────────
					// Elle etait COMPTEE et jamais appliquee : une demande de vert pur rendait
					// 0 pixel vert sur 700 000 lus (mesure du 17/09, decodeur PNG independant).
					// On la lit ici, une fois, et chaque role la rend a SA maniere.
					//
					// ⚠️ PAS DE SURCHARGE GLOBALE DU THEME. Un conteneur vert dont le theme
					//    resterait surcharge pendant le montage de ses enfants TEINDRAIT ses
					//    enfants. La surcharge ne vaut que pour les roles SANS enfants ; un
					//    conteneur, lui, peint son rectangle et rend la main.
					const NkGuiApparenceRepos app = NkGuiLireApparenceRepos(w);
					rap.apparencesNonPeintes += app.nonRendues;
					// LA COULEUR RECLAMEE SE NOTE ICI, ou la lecture a deja eu lieu.
					// L'ecrire ailleurs aurait fait un second analyseur du meme texte,
					// et deux analyseurs du meme texte divergent toujours.
					if (app.aFond) {
						if (app.fond.a == 255u)
							rap.NoterCouleurDemandee(app.fond.r, app.fond.g, app.fond.b);
						else
							++rap.couleursTranslucides;
					}

					// ═════════════════════════════════════════════════════════
					//  `fill` PEINT PARTOUT (26/09) — on RESERVE, puis on peint
					//  DESSOUS, puis le widget se dessine par-dessus
					// ═════════════════════════════════════════════════════════
					//  AVANT : seuls quatre roles avaient « une surface a remplir »
					//  (Button, RepeatButton, Panel, Window) ; partout ailleurs le
					//  `fill` etait COMPTE et jamais tenu. Rodolf, le 26/09 : « il
					//  doit peindre partout ».
					//
					//  LE PROBLEME ETAIT REEL, ET IL EST D'ORDRE IMMEDIAT : en mode
					//  immediat, le rectangle d'un widget n'est connu QU'APRES son
					//  dessin. Peindre un fond apres lui le RECOUVRIRAIT.
					//
					//  LA SORTIE EXISTAIT DEJA DANS NKGui, et on ne l'avait pas
					//  employee : `NextItemRect(w, h)` RESERVE la place et avance le
					//  curseur ; `SetNextItemRect(r)` IMPOSE ce rectangle au widget
					//  suivant, pour UN seul appel. On reserve donc, on peint le
					//  fond, puis le widget prend exactement la place reservee.
					//
					//  ⚠️ ON NE LE FAIT QUE POUR LES FEUILLES. Un conteneur peint son
					//     fond a SA region, avant ses enfants -- chaque `case` le
					//     fait ou le comptera. Reserver une hauteur d'item pour une
					//     `VBox` lui imposerait une taille qu'elle n'a pas, et la
					//     mise en page entiere se replierait sur une ligne.
					//
					//  ⚠️ ET LA HAUTEUR RESERVEE EST CELLE DU THEME, pas une
					//     invention. Une feuille de hauteur non standard -- un `Text`
					//     qui s'enroule, un `Chart` qui porte `height` -- recevrait
					//     une place trop courte. Ces deux-la sont donc EXCLUS et
					//     restent comptes : mieux vaut un fond absent et compte
					//     qu'une mise en page fausse.
					const bool estConteneurFond =
						(role == NkGuiRole::Window || role == NkGuiRole::Panel
						 || role == NkGuiRole::Group || role == NkGuiRole::VBox
						 || role == NkGuiRole::HBox || role == NkGuiRole::Grid
						 || role == NkGuiRole::Flow || role == NkGuiRole::Scroll
						 || role == NkGuiRole::Splitter || role == NkGuiRole::DockSpace
						 || role == NkGuiRole::MenuBar || role == NkGuiRole::Menu
						 || role == NkGuiRole::Expander || role == NkGuiRole::TabBar);
					// Ces deux-la ont une hauteur que le document decide, pas le theme.
					const bool hauteurLibre =
						(role == NkGuiRole::Chart || role == NkGuiRole::Host
						 || role == NkGuiRole::Image
						 || (role == NkGuiRole::Text && NkGBooleen(w, "wrap", false)));
					bool fondPeintIci = false;
					if (app.aFond && !estConteneurFond && !hauteurLibre && !pl.pose) {
						const NkRect rFond = ctx.NextItemRect(-1.f, ctx.ItemHeight());
						ctx.DL().AddRectFilled(rFond, app.fond,
											   app.rayon >= 0.f ? app.rayon : ctx.theme.rounding);
						ctx.SetNextItemRect(rFond);
						++rap.apparencesPeintes;
						fondPeintIci = true;
					}

					// ── LE FOND D'UN CONTENEUR POSE (26/09) ──────────────────
					//  Un conteneur ne peut pas reserver sa place comme une
					//  feuille : sa hauteur depend de ses enfants. Mais quand il
					//  est POSE -- `pos` ecrit, parent absolu -- son rectangle est
					//  connu AVANT de monter son contenu, et rien n'empeche alors
					//  de peindre son fond dessous.
					//
					//  ⚠️ C'EST LE CAS DE TOUS LES CONTENEURS D'UN DESIGN EXPORTE.
					//     NKUIDesign ecrit en absolu : sur son document d'essai,
					//     22 conteneurs sur 22 sont poses. Les 17 apparences
					//     comptees non peintes etaient donc peignables.
					//
					//  ⚠️ `Window` ET `Panel` SONT EXCLUS : leur `case` peint deja
					//     leur fond, APRES `PanelBackground` pour que l'ombre et le
					//     contour de la primitive restent dessous. Peindre ici EN
					//     PLUS donnerait deux couches et compterait deux fois.
					//
					//  ⚠️ ET UN CONTENEUR EN FLUX RESTE COMPTE. Son rectangle
					//     (`BlocConsomme`) n'existe qu'APRES ses enfants : peindre
					//     a ce moment-la les recouvrirait. Mieux vaut un fond
					//     absent et compte qu'un contenu efface.
					//  ⚠️ ET LA CONDITION EST `pl.pose`, PAS « conteneur pose ».
					//     Premiere version de ce bloc : elle ne visait que les
					//     conteneurs, et les 17 apparences comptees n'ont pas
					//     bouge -- parce que ce n'etaient PAS des conteneurs.
					//     C'etaient des FEUILLES POSEES : dans un document venu de
					//     NKUIDesign, tout porte `pos`, donc la branche « feuille
					//     en flux » plus haut (qui exige `!pl.pose`) les sautait,
					//     et la branche « conteneur » aussi. Elles tombaient entre
					//     les deux.
					//
					//     Or un widget POSE a son rectangle connu d'avance, qu'il
					//     soit conteneur ou feuille. C'est la meme raison, donc
					//     c'est la meme branche.
					//  ⚠️ MUTATION DE BANC, `NK_GUI_MUTATION_FOND_POSE=1` : le fond
					//     d'un widget pose n'est PAS peint, et se compte comme
					//     avant. Meme mecanique que `NK_TAILLE_MUTATION` pour
					//     `sizeRel`. Elle existe parce que ce correctif a d'abord
					//     semble ne DEPLACER AUCUNE MESURE -- `contenu` valait
					//     53 239 avant comme apres -- et qu'un correctif
					//     indiscernable d'un placebo doit pouvoir etre eteint pour
					//     qu'on voie ce qu'il change.
					static const bool mutationSansFond = []() {
						const char *v = getenv("NK_GUI_MUTATION_FOND_POSE");
						return v && v[0] && v[0] != '0';
					}();
					if (app.aFond && pl.pose && !fondPeintIci && !mutationSansFond
						&& role != NkGuiRole::Window && role != NkGuiRole::Panel) {
						ctx.DL().AddRectFilled(pl.rect, app.fond,
											   app.rayon >= 0.f ? app.rayon : ctx.theme.rounding);
						++rap.apparencesPeintes;
						fondPeintIci = true;
					}

					// ⚠️ ET TOUT CE QUI N'EST PAS PEINT SE COMPTE ICI, SANS EXCEPTION.
					//    Premiere version de ce bloc (26/09) : les conteneurs autres que
					//    `Window` et `Panel` n'etaient NI peints NI comptes -- un `fill`
					//    sur une `VBox` disparaissait en silence. C'est la faute exacte
					//    que tout ce fichier sert a rendre impossible : *un document
					//    rendu autrement que demande n'a pas l'air casse, il a l'air
					//    normal*.
					//
					//    La condition est donc ecrite a l'envers : on ne liste plus ce
					//    qu'on compte, on liste les QUATRE roles qui peignent leur fond
					//    EUX-MEMES dans leur `case` (et incrementent alors
					//    `apparencesPeintes` de leur cote). Tout le reste tombe dans le
					//    compteur. Ajouter un role a l'avenir le fera compter par
					//    defaut, jamais disparaitre.
					const bool fondTenuAilleurs =
						(role == NkGuiRole::Window || role == NkGuiRole::Panel
						 || role == NkGuiRole::Button || role == NkGuiRole::RepeatButton);
					if (app.aFond && !fondPeintIci && !fondTenuAilleurs)
						++rap.apparencesNonPeintes;

					// ═════════════════════════════════════════════════════════
					//  L'ENCRE DU DOCUMENT — UNE SEULE PORTE POUR TOUS LES ROLES
					// ═════════════════════════════════════════════════════════
					//  🔴 CE QUI SE PASSAIT AVANT (26/09). `text { color }` etait honore
					//     par le `case Text` et par PERSONNE D'AUTRE. Un `Button`, une
					//     `CheckBox`, un `Selectable` recevaient l'encre du theme quoi que
					//     le document ecrive -- et le bouton orange de NkAnimaEditor
					//     rendait **0 pixel** de l'encre qu'il demande. *Deux roles, un
					//     seul des deux lisait le meme attribut du meme document.*
					//
					//  ⚠️ LA SURCHARGE NE VAUT QUE POUR LES ROLES SANS ENFANTS, et cette
					//     regle est deja ecrite plus haut dans ce fichier : un conteneur
					//     dont le theme resterait surcharge pendant le montage de ses
					//     enfants TEINDRAIT ses enfants. Un conteneur qui demande une
					//     encre est donc COMPTE, pas honore.
					//
					//  ⚠️ ET LA RESTAURATION PASSE PAR UN OBJET DE PILE, pas par de la
					//     discipline : le `switch` qui suit compte des dizaines de
					//     `return`, et ce fichier a deja paye la meme lecon sur
					//     `BeginDisabled`.
					struct EncreDuDocument {
							NkGuiContext *c;
							NkColor sauve;
							bool posee;
							EncreDuDocument(NkGuiContext &cx, const NkGuiApparenceRepos &a,
											bool conteneur, NkGuiMonteRapport &r) noexcept
								: c(&cx), sauve(cx.theme.text), posee(false) {
								if (!a.aEncre)
									return;
								if (conteneur) {
									++r.apparencesNonPeintes;
									return;
								}
								cx.theme.text = a.encre;
								++r.apparencesPeintes;
								posee = true;
							}
							~EncreDuDocument() {
								if (posee)
									c->theme.text = sauve;
							}
					};
					EncreDuDocument encre(ctx, app, estConteneurFond, rap);

					bool aDessine = true;
					float32 valeurMontee = 0.f;
					bool aValeurMontee = false;
					bool champVideMonte = false;

					switch (role) {
						// ── CONTENEURS ───────────────────────────────────────
						case NkGuiRole::Window:
						case NkGuiRole::Panel: {
							// Le fond, puis le contenu. `BeginPanel` n'est pas ouvert ici : voir
							// l'en-tete du fichier.
							NkRect r = pl.pose ? pl.rect : RegionCourante(ctx, w);
							// ⚠️ UN CONTENEUR ABSOLU QUI VIT DANS UN FLUX PART DU CURSEUR, pas du
							//    bord de la region. Sans cette ligne il reprendrait TOUTE la region
							//    de son parent et se peindrait par-dessus ses freres deja poses --
							//    un chevauchement que personne n'a demande. Il ne peut pas, lui,
							//    porter de `pos` : son parent est en flux, et le validateur le
							//    refuse. Le curseur est donc la seule chose qui dise ou il commence.
							if (!pl.pose && enfantsAbsolus) {
								r.w = ctx.layout.region.w - (ctx.layout.cursor.x - ctx.layout.region.x);
								r.h = ctx.layout.region.h - (ctx.layout.cursor.y - ctx.layout.region.y);
								r.x = ctx.layout.cursor.x;
								r.y = ctx.layout.cursor.y;
							}
							// ── LA TAILLE RELATIVE : « 16 % de mon parent, au minimum 180 px » ──
							// Un conteneur qui declare `sizeRel` DECOUPE son rectangle dans le flux
							// du parent par la porte que NKGui a deja (`NextItemRect`) : c'est elle
							// qui sait avancer le curseur en X dans une HBox et en Y ailleurs. En
							// ecrire une seconde aurait fait deux verites sur la meme chose.
							const NkGuiTailleRel rel = NkGuiLireTailleRelative(w, ctx.layout.region);
							bool decoupeRel = false;
							if (!pl.pose && (rel.aW || rel.aH)) {
								r = ctx.NextItemRect(rel.aW ? rel.w : -1.f,
													 rel.aH ? rel.h : ctx.AvailHeight());
								decoupeRel = true;
							}
							// ⚠️ LE VOILE D'UNE MODALE, PEINT AVANT LE FOND.
							//    `modal` etait DECLARE par le format (doc 7 §3.6) et lu par PERSONNE :
							//    ni le monteur, ni le validateur au-dela du type. Une propriete que
							//    rien ne lit est une promesse que le fichier fait et que l'outil ne
							//    tient pas. C'est aussi la reponse a « boite de dialogue » : ce n'est
							//    pas un role a inventer, c'est `Window { modal = true }`, que le
							//    document 7 §4 avait deja tranche (« la modalite est une PROPRIETE »).
							//    ⚠️ LA COULEUR EST CELLE QUE LE KIT A DEJA CHOISIE pour ses dialogues
							//       (`NkEditorModal.h` : `NkColor{0, 0, 0, 120}`). En prendre une autre
							//       aurait donne deux voiles differents pour la meme idee.
							if (NkGBooleen(w, "modal", false)) {
								++rap.modales;
								ctx.DL().AddRectFilled(ctx.layout.region, NkColor{0, 0, 0, 120});
							}
							// ═══════════════════════════════════════════════════════
							//  LA FENETRE SE DEPLACE PAR SA BARRE DE TITRE (26/09)
							// ═══════════════════════════════════════════════════════
							//  Rodolf : « des dialog box deplacable, bref la totale ». Le voile
							//  modal existait, le titre se peignait -- et la fenetre etait CLOUEE.
							//
							//  ⚠️ LE DEPLACEMENT VIT DANS L'ETAT DU MONTAGE, PAS DANS LE DOCUMENT.
							//     Meme regle que le `ratio` d'un `Splitter` et que le pli d'un
							//     `Expander` : le fichier donne la position INITIALE, le geste la
							//     deplace, et rien n'est reecrit sur le disque. Un document qui
							//     reimposerait son `pos` a chaque image ramenerait la fenetre sous
							//     le doigt de qui la traine.
							//
							//  ⚠️ IL FAUT UNE PRISE, ET C'EST LA BARRE DE TITRE. Traîner par
							//     n'importe quel point du fond rendrait la fenetre impossible a
							//     utiliser : chaque clic manque sur un widget deviendrait un
							//     deplacement. Une fenetre SANS titre reste donc fixe -- et ce
							//     n'est pas un oubli, c'est qu'elle n'offre aucune prise.
							//
							//  ⚠️ `NoMove` EST MAINTENANT APPLIQUE, donc il ne se compte plus.
							//     Les quatre autres parlent de gestes que ce monteur n'a toujours
							//     pas (fermer, replier, defiler, redimensionner) : eux restent
							//     comptes. *Un drapeau tenu et un drapeau compte ne doivent jamais
							//     tomber dans le meme chiffre.*
							const NkString titreFenetre = NkGTexte(w, "title", "");
							const bool sansBarre = NkGuiDrapeau(w, "NoTitleBar");
							const bool aBarre = !sansBarre && titreFenetre.Size() > 0 && ctx.font
												&& ctx.font->Valid();
							const bool deplacable = pl.pose && aBarre && !NkGuiDrapeau(w, "NoMove");
							if (deplacable && e) {
								// Le deplacement deja acquis, avant tout dessin : la fenetre se
								// peint LA OU ELLE EST, pas la ou le document l'avait mise.
								r.x += e->deplacement.x;
								r.y += e->deplacement.y;
								const NkRect barre{r.x, r.y, r.w, ctx.ItemHeight()};
								const NkGuiId idFen = ctx.GetId(id.CStr());
								bool hovF = false, heldF = false;
								ctx.ButtonBehavior(idFen, barre, NkGuiButtonFlags::None, -1.f, -1.f,
												   &hovF, &heldF);
								if (ctx.activeId == idFen) {
									// ⚠️ UN CUMUL DE DELTAS, PAS UNE POSITION ABSOLUE. Le
									//    `Splitter` derive sa valeur de la position absolue parce
									//    qu'un ratio doit suivre le curseur meme si la zone change
									//    de taille. Ici c'est l'inverse : sans la PRISE (l'ecart
									//    entre le curseur et le coin au moment de l'appui), une
									//    position absolue ferait sauter la fenetre pour coller son
									//    coin sous le curseur au premier pixel de geste.
									r.x += ctx.input.mouseDelta.x;
									r.y += ctx.input.mouseDelta.y;
									e->deplacement.x += ctx.input.mouseDelta.x;
									e->deplacement.y += ctx.input.mouseDelta.y;
									++rap.fenetresDeplacees;
								}
							}
							{
								static const char *kInteraction[] = {"NoResize", "NoClose",
												  "NoCollapse", "NoScrollbar"};
								for (uint32 fi = 0; fi < 4u; ++fi)
									if (NkGuiDrapeau(w, kInteraction[fi]))
										++rap.flagsNonAppliques;
								// `NoMove` ne se compte que s'il n'a rien a empecher : sur une
								// fenetre qui n'etait de toute facon pas deplacable, il reste une
								// promesse que personne ne tient.
								if (NkGuiDrapeau(w, "NoMove") && !(pl.pose && aBarre))
									++rap.flagsNonAppliques;
							}
							PanelBackground(ctx, r);
							// ⚠️ PAR-DESSUS, ET AVANT LES ENFANTS. Le fond du theme est peint d'abord
							//    (il porte l'ombre et le contour que la primitive dessine) ; la couleur
							//    du document le recouvre. Peindre a la place aurait fait disparaitre ce
							//    que la primitive ajoute autour.
							if (app.aFond) {
								ctx.DL().AddRectFilled(r, app.fond, app.rayon >= 0.f ? app.rayon : ctx.theme.rounding);
								++rap.apparencesPeintes;
							}

							// ⚠️ LE `title` DU FICHIER ETAIT PERDU. Le monteur n'ouvre
							//    pas `BeginPanel` (voir l'en-tete du fichier), mais ne
							//    pas ouvrir un conteneur n'est pas une raison de jeter
							//    ce que le document ecrit. Il est peint a la main, au
							//    meme endroit qu'une barre de titre.
							// Le titre et sa barre ont ete lus plus haut : la PRISE du
							// deplacement doit exister avant qu'on peigne quoi que ce soit.
							const NkString &titre = titreFenetre;
							const bool aTitre = aBarre;
							if (aTitre) {
								const NkVec2 coin{r.x + ctx.layout.padding, r.y + ctx.layout.padding * 0.5f};
								(void)TextAt(ctx, coin, titre.CStr());
							}
							Noter(rap, id, t, r, prof, true, horizontal, &ctx.layout.region);
							// ── OUVRIR UNE REGION, OU NON ─────────────────────────
							// ⚠️ STRICTEMENT ADDITIF, ET C'EST CETTE CONDITION QUI LE GARANTIT. Un
							//    conteneur qui n'est ni POSE ni ABSOLU se comporte EXACTEMENT comme
							//    avant : le contenu reste dans le flux du parent, le titre pousse le
							//    curseur exterieur. C'est ce qui laisse les dix `.nkgui` valides du
							//    corpus rendre le meme pixel qu'hier -- mesure, pas espere.
							//
							// ⚠️ ET `BeginLayout` N'A PAS DE PILE. On sauve `ctx.layout` par valeur et
							//    on le repose : c'est exactement ce que NKGui fait deja pour ses popups
							//    (`popupSaved[PopupMax]`, un `NkGuiLayout` par niveau). On reprend son
							//    procede plutot que d'en inventer un second.
							const bool ouvrirRegion = pl.pose || enfantsAbsolus || decoupeRel;
							if (ouvrirRegion) {
								const NkGuiLayout sauve = ctx.layout;
								ctx.BeginLayout(r);
								if (aTitre)
									ctx.layout.cursor.y += ctx.ItemHeight();
								MonterCorps(ctx, w, etat, rap, prof + 1u, false, enfantsAbsolus, hooks);
								ctx.layout = sauve;
							} else {
								if (aTitre)
									ctx.layout.cursor.y += ctx.ItemHeight();
								MonterCorps(ctx, w, etat, rap, prof + 1u, false, enfantsAbsolus, hooks);
							}
							++rap.montes;
							return;
						}
						// ── LA ZONE DEFILANTE ────────────────────────────────
						// Le format la connait depuis toujours (`axis`, `always`) ; le monteur,
						// non -- et son contenu etait donc EMPORTE. `BeginChild` est la brique
						// que NKGui a deja : elle rogne son contenu, tient la molette et pose
						// une barre de defilement. On ne reecrit rien.
						//
						// ⚠️ `axis` EST LU, ET IL NE PEUT PAS TOUT DIRE. `BeginChild` defile
						//    TOUJOURS verticalement et prend l'horizontal EN PLUS ; il n'existe
						//    pas de mode « horizontal SEUL ». `axis = Horizontal` active donc
						//    les deux, et ce n'est pas exactement ce que le document demande.
						//    On l'ecrit ici plutot que de laisser croire a une fidelite qu'on
						//    n'a pas -- le jour ou NKGui saura l'horizontal seul, la ligne a
						//    changer est celle-ci.
						//
						// ⚠️ ET SI `BeginChild` REFUSE, ON MONTE QUAND MEME LE CONTENU, en flux.
						//    Un `return` sec ici aurait reintroduit, pour ce seul role, le defaut
						//    que ce lot corrige : perdre le sous-arbre sans le dire.
						case NkGuiRole::Scroll: {
							const NkString ax = NkGTexte(w, "axis", "Vertical");
							const bool horiz = NkGMotEgal(NkStringView(ax.CStr()), "Horizontal")
											   || NkGMotEgal(NkStringView(ax.CStr()), "Both");
							NkRect r = pl.pose ? pl.rect : ctx.layout.region;
							if (!pl.pose) {
								// Comme un groupe absolu pose dans un flux : on part du curseur,
								// jamais du bord de la region, sinon la zone recouvre ses aines.
								r.w -= (ctx.layout.cursor.x - r.x);
								r.h -= (ctx.layout.cursor.y - r.y);
								r.x = ctx.layout.cursor.x;
								r.y = ctx.layout.cursor.y;
							}
							if (BeginChild(ctx, lbl, r, true, horiz)) {
								MonterCorps(ctx, w, etat, rap, prof + 1u, false, false, hooks);
								EndChild(ctx);
							} else {
								MonterCorps(ctx, w, etat, rap, prof + 1u, horizontal, parentAbsolu, hooks);
							}
							Noter(rap, id, t, r, prof, true, horizontal, &ctx.layout.region);
							++rap.montes;
							return;
						}
						case NkGuiRole::Group: {
							// ⚠️ UN `Group` PEUT DECLARER `placement = absolute` -- c'est l'un des
							//    trois conteneurs NEUTRES (avec `Window` et `Panel`). Une `VBox` ne le
							//    peut pas : son NOM dit deja son agencement.
							if (pl.pose || enfantsAbsolus) {
								NkRect r = pl.pose ? pl.rect : ctx.layout.region;
								if (!pl.pose) {
									// Meme raison que pour `Window`/`Panel` : un groupe absolu pose
									// dans un flux part du curseur, jamais du bord de la region.
									r.w -= (ctx.layout.cursor.x - r.x);
									r.h -= (ctx.layout.cursor.y - r.y);
									r.x = ctx.layout.cursor.x;
									r.y = ctx.layout.cursor.y;
								}
								const NkGuiLayout sauve = ctx.layout;
								ctx.BeginLayout(r);
								MonterCorps(ctx, w, etat, rap, prof + 1u, horizontal, enfantsAbsolus, hooks);
								ctx.layout = sauve;
								Noter(rap, id, t, r, prof, true, horizontal, &ctx.layout.region);
								++rap.montes;
								return;
							}
							const NkVec2 c0 = ctx.layout.cursor;
							BeginGroup(ctx);
							MonterCorps(ctx, w, etat, rap, prof + 1u, horizontal, false, hooks);
							EndGroup(ctx);
							Noter(rap, id, t, BlocConsomme(ctx, c0), prof, true, horizontal, &ctx.layout.region);
							++rap.montes;
							return;
						}
						case NkGuiRole::VBox: {
							const float32 gap = NkGA(w, "gap") ? NkGNombre(w, "gap", -1.f) : -1.f;
							// ⚠️ UNE BOITE SE POSE, MAIS NE DECLARE PAS `placement`. Son NOM dit
							//    deja son agencement, et le validateur refuse `placement` sur elle
							//    (propriete hors schema). Elle se pose quand SON parent est absolu,
							//    et ses enfants a elle restent en FLUX : c'est exactement la toolbox
							//    de Rodolf -- posee dans une fenetre, boutons alignes tout seuls.
							if (pl.pose) {
								const NkGuiLayout sauve = ctx.layout;
								ctx.BeginLayout(pl.rect);
								BeginVBox(ctx, gap);
								MonterCorps(ctx, w, etat, rap, prof + 1u, false, false, hooks);
								EndVBox(ctx);
								ctx.layout = sauve;
								Noter(rap, id, t, pl.rect, prof, true, horizontal, &ctx.layout.region);
								++rap.montes;
								return;
							}
							const NkVec2 c0 = ctx.layout.cursor;
							BeginVBox(ctx, gap);
							MonterCorps(ctx, w, etat, rap, prof + 1u, false, false, hooks);
							EndVBox(ctx);
							Noter(rap, id, t, BlocConsomme(ctx, c0), prof, true, horizontal, &ctx.layout.region);
							++rap.montes;
							return;
						}
						case NkGuiRole::HBox: {
							const float32 gap = NkGA(w, "gap") ? NkGNombre(w, "gap", -1.f) : -1.f;
							// ⚠️ UNE BOITE SE POSE, MAIS NE DECLARE PAS `placement`. Son NOM dit
							//    deja son agencement, et le validateur refuse `placement` sur elle
							//    (propriete hors schema). Elle se pose quand SON parent est absolu,
							//    et ses enfants a elle restent en FLUX : c'est exactement la toolbox
							//    de Rodolf -- posee dans une fenetre, boutons alignes tout seuls.
							if (pl.pose) {
								const NkGuiLayout sauve = ctx.layout;
								ctx.BeginLayout(pl.rect);
								BeginHBox(ctx, gap);
								MonterCorps(ctx, w, etat, rap, prof + 1u, true, false, hooks);
								EndHBox(ctx);
								ctx.layout = sauve;
								Noter(rap, id, t, pl.rect, prof, true, horizontal, &ctx.layout.region);
								++rap.montes;
								return;
							}
							const NkVec2 c0 = ctx.layout.cursor;
							BeginHBox(ctx, gap);
							MonterCorps(ctx, w, etat, rap, prof + 1u, true, false, hooks);
							EndHBox(ctx);
							Noter(rap, id, t, BlocConsomme(ctx, c0), prof, true, horizontal, &ctx.layout.region);
							++rap.montes;
							return;
						}

						// ── FEUILLES ─────────────────────────────────────────
						case NkGuiRole::Text: {
							const NkString s = NkGTexte(w, "text", "");
							// ⚠️ UN `Text` N'A PAS DE SURFACE. Son apparence honoree est son ENCRE
							//    (`text { color }`). Un `fill` ecrit sur lui demande un fond que NKGui
							//    ne peint pas pour un texte : on le COMPTE plutot que de l'inventer
							//    -- inventer un rectangle demanderait de mesurer le texte avant de le
							//    dessiner, ce que ce monteur ne fait pas.
							//  L'encre est POSEE PLUS HAUT, par `EncreDuDocument`, pour tous
							//  les roles a la fois. La poser ici EN PLUS aurait fait deux
							//  ecrivains sur `theme.text` et deux comptes pour un attribut.
							if (NkGBooleen(w, "wrap", false))
								TextWrapped(ctx, s.CStr());
							else
								Text(ctx, s.CStr());
							break;
						}
						case NkGuiRole::Button: {
							const NkString s = NkGTexte(w, "label", id.CStr());
							// ⚠️ UNE FEUILLE : la surcharge ne peut fuir vers personne, et elle est
							//    RENDUE juste apres. C'est ce qui la rend sure ici et interdite sur
							//    un conteneur.
							const NkColor sauve = ctx.theme.button;
							if (app.aFond) {
								ctx.theme.button = app.fond;
								++rap.apparencesPeintes;
							}
							// L'encre du libelle vient de `EncreDuDocument`, pose plus haut.
							(void)Button(ctx, s.CStr());
							ctx.theme.button = sauve;
							break;
						}
						case NkGuiRole::RepeatButton: {
							// Le seul role sans forme auto-placee : on lui donne le
							// rectangle que le layout aurait donne, pas un rectangle
							// invente.
							const NkString s = NkGTexte(w, "label", id.CStr());
							const NkRect r = ctx.NextItemRect(0.f, ctx.ItemHeight());
							(void)RepeatButton(ctx, s.CStr(), r, NkGNombre(w, "repeatDelay", -1.f),
											   NkGNombre(w, "repeatRate", -1.f));
							break;
						}
						case NkGuiRole::Checkbox: {
							const NkString s = NkGTexte(w, "label", id.CStr());
							if (e)
								(void)Checkbox(ctx, s.CStr(), e->b);
							else
								aDessine = false;
							break;
						}
						case NkGuiRole::Slider: {
							if (e) {
								const float32 vmin = NkGNombre(w, "min", 0.f);
								const float32 vmax = NkGNombre(w, "max", 1.f);
								// ⚠️ LA VALEUR INITIALE RESPECTE LES BORNES ECRITES. Vu
								//    sur l'image rendue : le curseur de
								//    `01_panneau_reglages` affichait 0.00 alors que le
								//    fichier ecrit `min = 0.5`. Un montage fidele ne
								//    peut pas poser une valeur que le document
								//    interdit -- et `SliderFloat` ne corrige pas une
								//    valeur d'entree hors bornes, il la dessine.
								if (!e->initialise) {
									e->f = NkGNombre(w, "value", vmin);
									e->initialise = true;
								}
								if (e->f < vmin)
									e->f = vmin;
								if (e->f > vmax)
									e->f = vmax;
								(void)SliderFloat(ctx, lbl, e->f, vmin, vmax);
								valeurMontee = e->f;
								aValeurMontee = true;
							} else {
								aDessine = false;
							}
							break;
						}
						case NkGuiRole::TextField: {
							if (e) {
								// ⚠️ LE `placeholder` N'EST PAS UNE VALEUR. La premiere
								//    version le copiait dans le tampon : l'image
								//    montrait « Filtrer... » comme si l'utilisateur
								//    l'avait tape. Un texte d'invite s'affiche quand le
								//    champ est VIDE et disparait des qu'on ecrit.
								//    LIMITE NOMMEE : `InputText` n'a pas de
								//    placeholder ; le champ part donc vide, et
								//    l'invite du fichier n'est pas rendue. La
								//    perdre est moins faux que de la faire passer
								//    pour une saisie.
								if (!e->initialise) {
									const NkString v = NkGTexte(w, "value", "");
									Copier(e->texte, (int32)sizeof(e->texte), v);
									e->initialise = true;
								}
								(void)InputText(ctx, lbl, e->texte, (int32)sizeof(e->texte));
								champVideMonte = (e->texte[0] == '\0');
								aValeurMontee = false;
								// ⚠️ L'INVITE EST PEINTE, DANS LA COULEUR DU TEXTE
								//    GRISE. La premiere version la copiait dans le
								//    tampon -- l'image montrait « Filtrer... » comme
								//    si quelqu'un l'avait tape, ce qui est le plus
								//    trompeur des trois defauts : rien ne distinguait
								//    une invite d'une valeur reelle. La deuxieme ne la
								//    peignait plus du tout, et perdait ce que le
								//    fichier ecrit. Elle se peint donc PAR-DESSUS le
								//    champ vide, avec `theme.textDisabled` -- la
								//    couleur qui dit « ceci n'est pas votre saisie ».
								//    `InputText` n'ayant pas de placeholder, c'est le
								//    monteur qui le pose ; le jour ou NKGui en portera
								//    un, ces lignes s'en vont.
								if (champVideMonte && ctx.font && ctx.font->Valid()) {
									const NkString ph = NkGTexte(w, "placeholder", "");
									if (ph.Size() > 0) {
										// Le champ occupe la rangee MOINS le libelle,
										// exactement comme `InputTextEx` le calcule.
										const NkRect rang = ctx.layout.prevItem;
										const float32 largeurLbl =
											(lbl && LabelEnd(lbl) != lbl)
												? ctx.font->MeasureWidth(lbl, LabelEnd(lbl)) + 14.f
												: 0.f;
										const float32 lh = ctx.font->LineHeight();
										const NkVec2 coin{rang.x + 6.f,
														  rang.y + (rang.h - lh) * 0.5f};
										(void)largeurLbl;
										(void)TextAt(ctx, coin, ph.CStr(), ctx.theme.textDisabled);
									}
								}
							} else {
								aDessine = false;
							}
							break;
						}
						case NkGuiRole::Dropdown: {
							const uint32 n = NkGListeCompte(w, "items");
							const NkString apercu = NkGTexte(w, "preview", "");
							if (BeginCombo(ctx, lbl, apercu.CStr(), (int32)n))
								EndCombo(ctx);
							break;
						}
						// ═════════════════════════════════════════════════════
						//  LES HUIT ROLES QUE LE FORMAT ACCEPTAIT SANS MONTAGE
						// ═════════════════════════════════════════════════════
						//  Rodolf, 27/09 : « reecris donc tout le vocabulaire
						//  manquant une fois ». Ces huit-la sont au schema du format
						//  depuis toujours (`NkGuiValidate.h`) : un document qui les
						//  ecrit est VALIDE, et le monteur les comptait en
						//  `rolesInconnus` -- donc le document entier etait REFUSE.
						//  Le format disait oui, le monteur disait non.
						//
						//  ⚠️ CHACUN GARDE SA LIMITE, ECRITE A COTE DE LUI. La note
						//     de refus du 26/09 avait raison sur le fond : aucun de
						//     ces roles ne tombe EXACTEMENT sur une fonction de
						//     NKGui. Ce qui a change, c'est la reponse : au lieu de
						//     refuser le role entier, on monte ce qui correspond et
						//     on COMPTE l'attribut sans equivalent
						//     (`attributsNonHonores`) -- l'idiome que `Grid.sizes`
						//     emploie deja. *Un document rendu partiellement et
						//     compte vaut mieux qu'un document refuse en entier.*
						case NkGuiRole::NumberField: {
							// `valueType` decide entier ou flottant ; `step` est le
							// pas des fleches. Le modele ne stocke qu'un `float32` :
							// un entier y tient sans perte jusqu'a 2^24.
							if (!e) {
								aDessine = false;
								break;
							}
							const float32 vmin = NkGNombre(w, "min", 0.f);
							const float32 vmax = NkGNombre(w, "max", 0.f);
							const bool borne = (vmax > vmin);
							if (!e->initialise) {
								e->f = NkGNombre(w, "value", borne ? vmin : 0.f);
								e->initialise = true;
							}
							const NkString type = NkGTexte(w, "valueType", "Float");
							if (NkGMotEgal(NkStringView(type.CStr()), "Int")) {
								int32 vi = (int32)(e->f + (e->f < 0.f ? -0.5f : 0.5f));
								(void)InputInt(ctx, lbl, vi, (int32)NkGNombre(w, "step", 1.f));
								e->f = (float32)vi;
							} else {
								(void)InputFloat(ctx, lbl, e->f, NkGNombre(w, "step", 1.f));
							}
							// ⚠️ LES BORNES SONT APPLIQUEES ICI, PAS PAR `InputFloat`.
							//    Il n'en prend pas : sans cette ligne, `min`/`max`
							//    seraient ecrits par le document et ignores en
							//    silence -- exactement le defaut que ce fichier
							//    combat.
							if (borne) {
								if (e->f < vmin)
									e->f = vmin;
								if (e->f > vmax)
									e->f = vmax;
							}
							valeurMontee = e->f;
							aValeurMontee = true;
							break;
						}
						case NkGuiRole::Drag: {
							if (!e) {
								aDessine = false;
								break;
							}
							const float32 vmin = NkGNombre(w, "min", 0.f);
							const float32 vmax = NkGNombre(w, "max", 0.f);
							const bool borne = (vmax > vmin);
							if (!e->initialise) {
								e->f = NkGNombre(w, "value", borne ? vmin : 0.f);
								e->initialise = true;
							}
							// ⚠️ `dir` N'A PAS D'EQUIVALENT et se compte. `DragFloat`
							//    glisse horizontalement, point. Le document peut
							//    demander `Vertical` ou `Both` : on ne fait pas
							//    semblant.
							if (NkGA(w, "dir"))
								++rap.attributsNonHonores;
							const NkString typeD = NkGTexte(w, "valueType", "Float");
							if (NkGMotEgal(NkStringView(typeD.CStr()), "Int")) {
								int32 vi = (int32)(e->f + (e->f < 0.f ? -0.5f : 0.5f));
								(void)DragInt(ctx, lbl, vi, NkGNombre(w, "speed", 0.25f),
											  borne ? (int32)vmin : 0, borne ? (int32)vmax : 0);
								e->f = (float32)vi;
							} else {
								(void)DragFloat(ctx, lbl, e->f, NkGNombre(w, "speed", 0.1f),
												borne ? vmin : 0.f, borne ? vmax : 0.f);
							}
							valeurMontee = e->f;
							aValeurMontee = true;
							break;
						}
						case NkGuiRole::ColorField: {
							// 🔴 LA RAISON DU REFUS DE 09/26 ETAIT JUSTE, ET ELLE A UNE
							//    SORTIE. « `bind` passe par un modele qui ne stocke
							//    QU'UN `float32` -- une couleur n'y tient pas. » Vrai.
							//    Mais l'entree de magasin porte AUSSI `char texte[256]`,
							//    ou vit deja la saisie d'un `TextField`. Une couleur
							//    s'y ecrit `#RRGGBBAA` : c'est la meme graphie que le
							//    document, donc rien a convertir dans un sens ni dans
							//    l'autre, et la valeur reste LISIBLE dans le magasin.
							if (!e) {
								aDessine = false;
								break;
							}
							if (!e->initialise) {
								const NkString v = NkGTexte(w, "value", "#FFFFFFFF");
								Copier(e->texte, (int32)sizeof(e->texte), v);
								e->initialise = true;
							}
							NkColor c{255, 255, 255, 255};
							(void)NkGuiCouleur(NkStringView(e->texte), c);
							float32 col[4] = {(float32)c.r / 255.f, (float32)c.g / 255.f,
											  (float32)c.b / 255.f, (float32)c.a / 255.f};
							const bool avecAlpha = NkGBooleen(w, "alpha", true);
							if (ColorEdit4(ctx, lbl, col,
										   avecAlpha ? NkGuiColorFlags::None
													 : NkGuiColorFlags::NoAlpha)) {
								// Reecrit la graphie du document : huit chiffres,
								// majuscules, `#` en tete.
								static const char *kHex = "0123456789ABCDEF";
								const uint8 comp[4] = {
									(uint8)(col[0] * 255.f + 0.5f), (uint8)(col[1] * 255.f + 0.5f),
									(uint8)(col[2] * 255.f + 0.5f), (uint8)(col[3] * 255.f + 0.5f)};
								e->texte[0] = '#';
								for (uint32 k = 0; k < 4u; ++k) {
									e->texte[1u + k * 2u] = kHex[(comp[k] >> 4) & 0xF];
									e->texte[2u + k * 2u] = kHex[comp[k] & 0xF];
								}
								e->texte[9] = '\0';
							}
							// `mode` (RGB / HSV / Hex) n'a pas d'equivalent : compte.
							if (NkGA(w, "mode"))
								++rap.attributsNonHonores;
							break;
						}
						case NkGuiRole::Switch: {
							// « AUCUNE fonction dans NKGui. Du neuf. » — c'est exact,
							// et c'est peu : un interrupteur est une piste arrondie,
							// un bouton qui glisse, et le comportement d'un bouton.
							//
							// ⚠️ IL N'EST PAS UN `Checkbox` DEGUISE. Les deux disent
							//    « vrai ou faux », mais l'un coche un choix dans une
							//    liste et l'autre ALLUME quelque chose. Les confondre
							//    dans le rendu ferait mentir le document.
							if (!e) {
								aDessine = false;
								break;
							}
							if (!e->initialise) {
								e->b = NkGBooleen(w, "value", false);
								e->initialise = true;
							}
							const float32 hS = ctx.ItemHeight();
							const NkRect rTout = ctx.NextItemRect(-1.f, hS);
							const float32 largPiste = hS * 1.8f;
							const NkRect piste{rTout.x, rTout.y + hS * 0.2f, largPiste, hS * 0.6f};
							const NkGuiId idS = ctx.GetId(lbl);
							bool hovS = false, heldS = false;
							if (ctx.ButtonBehavior(idS, rTout, NkGuiButtonFlags::None, -1.f, -1.f,
												   &hovS, &heldS))
								e->b = !e->b;
							ctx.DL().AddRectFilled(piste,
												   e->b ? ctx.theme.accent : ctx.theme.border,
												   piste.h * 0.5f);
							const float32 r0 = piste.h * 0.5f - 2.f;
							const float32 cx = e->b ? (piste.x + piste.w - r0 - 2.f)
													: (piste.x + r0 + 2.f);
							ctx.DL().AddCircleFilled({cx, piste.y + piste.h * 0.5f}, r0,
													 ctx.theme.text);
							if (lbl && lbl[0]) {
								const NkVec2 coinS{rTout.x + largPiste + 6.f,
												   rTout.y + (rTout.h - ctx.ItemHeight()) * 0.5f};
								(void)TextAt(ctx, coinS, lbl);
							}
							valeurMontee = e->b ? 1.f : 0.f;
							aValeurMontee = true;
							break;
						}
						case NkGuiRole::RadioGroup: {
							// Un groupe de choix EXCLUSIFS. `options` porte les
							// libelles ; `bind` porte l'INDICE choisi.
							//
							// ⚠️ L'INDICE, PAS LE LIBELLE. Le magasin ne tient qu'un
							//    `float32` ; y mettre un indice est exact et stable
							//    quand les libelles changent de casse ou de langue.
							if (!e) {
								aDessine = false;
								break;
							}
							NkVector<NkString> opts;
							NkGListeChaines(w, "options", opts);
							if (!e->initialise) {
								e->f = NkGNombre(w, "value", 0.f);
								e->initialise = true;
							}
							const bool horiz = NkGMotEgal(
								NkStringView(NkGTexte(w, "orientation", "Vertical").CStr()),
								"Horizontal");
							const NkVec2 cRG = ctx.layout.cursor;
							if (horiz)
								BeginHBox(ctx, 8.f);
							for (uint32 k = 0; k < (uint32)opts.Size(); ++k) {
								const float32 hR = ctx.ItemHeight();
								const NkRect rR = ctx.NextItemRect(horiz ? 0.f : -1.f, hR);
								const NkGuiId idR = ctx.GetId(opts[k].CStr());
								bool hovR = false, heldR = false;
								if (ctx.ButtonBehavior(idR, rR, NkGuiButtonFlags::None, -1.f, -1.f,
													   &hovR, &heldR))
									e->f = (float32)k;
								const bool choisi = ((uint32)(e->f + 0.5f) == k);
								const NkVec2 c{rR.x + hR * 0.5f, rR.y + hR * 0.5f};
								ctx.DL().AddCircle(c, hR * 0.32f, ctx.theme.border, 1.f);
								if (choisi)
									ctx.DL().AddCircleFilled(c, hR * 0.18f, ctx.theme.accent);
								(void)TextAt(ctx, {rR.x + hR, rR.y}, opts[k].CStr());
							}
							if (horiz)
								EndHBox(ctx);
							Noter(rap, id, t, BlocConsomme(ctx, cRG), prof, true, horizontal,
								  &ctx.layout.region);
							++rap.montes;
							valeurMontee = e->f;
							aValeurMontee = true;
							return;
						}
						case NkGuiRole::ImageButton: {
							// ⚠️ SANS TEXTURE, ON NE FAIT PAS SEMBLANT. `image` nomme
							//    une ressource que le DOCUMENT ne porte pas : c'est
							//    l'hote qui la connait. Tant qu'aucun crochet ne la
							//    fournit, la reference est COMPTEE et le bouton se
							//    monte avec son identifiant en libelle -- visible,
							//    cliquable, et honnete sur ce qui manque.
							if (NkGA(w, "image") || NkGA(w, "source"))
								++rap.attributsNonHonores;
							const NkString sIB = NkGTexte(w, "label", id.CStr());
							(void)Button(ctx, sIB.CStr());
							break;
						}
						case NkGuiRole::Stack: {
							// 🔴 LE REFUS DISAIT : « `BeginStack` EXIGE une taille que
							//    le schema (`anchor` seul) ne porte pas -- il faudrait
							//    l'inventer. » Ce n'est plus vrai depuis le 26-27/09 :
							//    un conteneur POSE (`pos`+`size`) ou qui declare
							//    `sizeRel` a son rectangle AVANT ses enfants. La
							//    taille ne s'invente donc plus : elle se lit.
							//
							//    Sans l'une ni l'autre, on ne devine pas : le contenu
							//    est monte EN FLUX et `anchor` est compte.
							{
								const NkGuiTailleRel relS =
									NkGuiLireTailleRelative(w, ctx.layout.region);
								NkRect rS{};
								bool aRectS = false;
								if (pl.pose) {
									rS = pl.rect;
									aRectS = true;
								} else if (relS.aW || relS.aH) {
									rS = ctx.NextItemRect(relS.aW ? relS.w : -1.f,
														  relS.aH ? relS.h : ctx.AvailHeight());
									aRectS = true;
								}
								if (NkGA(w, "anchor") && !aRectS)
									++rap.attributsNonHonores;
								if (aRectS) {
									const NkGuiLayout sauveS = ctx.layout;
									ctx.BeginLayout(rS);
									// ⚠️ CHAQUE ENFANT REPART DU MEME COIN : c'est ce
									//    qui fait une PILE et non une colonne.
									const NkArchiveNode *csS = NkGMonteCorps(w);
									if (csS) {
										for (uint32 k = 0; k < (uint32)csS->array.Size(); ++k) {
											if (!csS->array[k].IsObject() || !csS->array[k].object)
												continue;
											ctx.layout.cursor = {rS.x, rS.y};
											MonterBloc(ctx, *csS->array[k].object, etat, rap,
													   prof + 1u, false, true, hooks);
										}
									}
									ctx.layout = sauveS;
									Noter(rap, id, t, rS, prof, true, horizontal,
										  &ctx.layout.region);
								} else {
									const NkVec2 c0S = ctx.layout.cursor;
									MonterCorps(ctx, w, etat, rap, prof + 1u, false, false, hooks);
									Noter(rap, id, t, BlocConsomme(ctx, c0S), prof, true,
										  horizontal, &ctx.layout.region);
								}
							}
							++rap.montes;
							return;
						}
						case NkGuiRole::Table: {
							// `columns` est une LISTE de noms : son compte donne le
							// nombre de colonnes, et les noms font l'en-tete.
							//
							// ⚠️ LES LIGNES NE VIENNENT PAS DU DOCUMENT. Une table
							//    affiche des DONNEES, que le format ne sait pas
							//    porter. Ce qui se monte ici, c'est la STRUCTURE :
							//    les colonnes, leur en-tete, et les enfants ecrits
							//    dans le document, un par cellule. Une table de
							//    donnees vivantes reste une zone `Host`.
							NkVector<NkString> cols;
							NkGListeChaines(w, "columns", cols);
							const int32 nCols = (int32)cols.Size() > 0 ? (int32)cols.Size() : 1;
							const NkVec2 cT = ctx.layout.cursor;
							if (BeginTable(ctx, lbl, nCols)) {
								for (uint32 k = 0; k < (uint32)cols.Size(); ++k) {
									(void)TableNextColumn(ctx);
									Text(ctx, cols[k].CStr());
								}
								MonterCorps(ctx, w, etat, rap, prof + 1u, false, false, hooks);
								EndTable(ctx);
							}
							if (NkGA(w, "flags"))
								++rap.flagsNonAppliques;
							Noter(rap, id, t, BlocConsomme(ctx, cT), prof, true, horizontal,
								  &ctx.layout.region);
							++rap.montes;
							return;
						}
						case NkGuiRole::Progress: {
							const float32 v = e ? e->f : NkGNombre(w, "value", 0.f);
							ProgressBar(ctx, v);
							break;
						}
						case NkGuiRole::Separator: {
							Separator(ctx);
							break;
						}
						case NkGuiRole::Spacer: {
							// `size = 12` est UN nombre : c'est l'axe du conteneur qui
							// dit lequel des deux cotes il mesure.
							const float32 s = NkGNombre(w, "size", 0.f);
							// 🔴 SANS `size`, EN FLUX HORIZONTAL, NKGUI INVENTAIT 120 PIXELS.
							//    `NextItemRect` porte, pour le flux horizontal, un `if (w <= 0.f)
							//    w = 120.f; // pas de "remplir" en HBox`. Le monteur passait 0 pour
							//    un `Spacer` sans taille ecrite : un bloc INVISIBLE de 120 px
							//    poussait le widget suivant HORS de son panneau (mesure du 17/09 :
							//    un panneau de 280 px, un bouton a (268, 66 de large) -- 44 px
							//    dehors), et le montage restait vert.
							//
							// ⚠️ ON NE TOUCHE PAS AU 120 DE `NextItemRect` : cette valeur sert a
							//    TOUS les appelants de NKGui dans cinq applications, et la changer
							//    deplacerait des interfaces que personne n'a demande de bouger. Le
							//    monteur, lui, SAIT que `Spacer` sans taille veut dire « le reste » :
							//    c'est a lui de le dire.
							//
							// ⚠️ ET IL NE FAIT PAS SEMBLANT DE L'HONORER. Pousser le voisin jusqu'au
							//    bord demanderait de connaitre SA largeur avant de le monter, donc
							//    deux passes. On rend zero -- le voisin reste DEDANS -- et on COMPTE
							//    la demande non tenue, plutot que de la taire ou de l'inventer.
							if (horizontal && !NkGA(w, "size")) {
								++rap.espacesFlexibles;
								break;
							}
							Spacer(ctx, horizontal ? s : 0.f, horizontal ? 0.f : s);
							break;
						}
						case NkGuiRole::Image: {
							const float32 iw = NkGNombre(w, "width", 64.f);
							const float32 ih = NkGNombre(w, "height", 64.f);
							Image(ctx, 0u, iw, ih);
							break;
						}
						case NkGuiRole::Chart: {
							float32 vals[64];
							const uint32 n = NkGListeNombres(w, "values", vals, 64u);
							const uint32 m = n < 64u ? n : 64u;
							if (m > 0u) {
								const NkString kind = NkGTexte(w, "kind", "Lignes");
								if (NkGMotEgal(NkStringView(kind), "Barres"))
									PlotHistogram(ctx, lbl, vals, (int32)m, NkGNombre(w, "min", 0.f),
												  NkGNombre(w, "max", 0.f), NkGNombre(w, "height", 0.f));
								else
									PlotLines(ctx, lbl, vals, (int32)m, NkGNombre(w, "min", 0.f),
											  NkGNombre(w, "max", 0.f), NkGNombre(w, "height", 0.f));
							} else {
								aDessine = false;
							}
							break;
						}
						// ── L'ANCRAGE : le document dit QUELLES ZONES, pas l'arbre ─
						// ⚠️ LA FRONTIERE, ET ELLE EST LE RESULTAT D'UNE MESURE, PAS D'UN GOUT.
						//    La coquille ecrit DEJA un arbre d'ancrage complet -- `dockroot=`,
						//    `node=k|kind|vertical|ratio|c0|c1|activeTab`, `nwin=`, `float=` --
						//    et elle le RELIT (`NkEditorShell::LoadUiState`). Le document n'a
						//    donc AUCUN arbre a decrire : il dirait une seconde verite, et la
						//    premiere fois que l'utilisateur tirerait un separateur, les deux
						//    divergeraient.
						//      le document   : QUELLES zones, leur cote, leurs proportions PAR DEFAUT
						//      la coquille   : l'arbre que l'utilisateur a OBTENU
						//    *Le document dit le defaut, le geste vit a cote* -- la meme frontiere
						//    que pour le separateur, un cran plus haut.
						//
						// ⚠️ ET UNE ZONE SE NOMME PAR L'IDENTIFIANT DU PANNEAU, jamais par son
						//    libelle affiche. Un document qui ecrirait « Propriétés » lierait la
						//    disposition a du texte traduisible -- mesure du 17/09 : renommer
						//    perdait la disposition en silence. L'identifiant existe depuis.
						//
						// ⚠️ LE MONTEUR NE RESOUT AUCUN PANNEAU : il DEMANDE a l'hote. NKGui ne
						//    sait pas quels panneaux une application fournit, et inventer une
						//    table ici en ferait une seconde autorite.
						case NkGuiRole::DockSpace: {
							static const bool kMuet = []() {
								const char *v = getenv("NK_DOCK_MUTATION"); // =muet : pas de marqueur
								return v && v[0] == 'm';
							}();
							const NkGuiTailleRel relD = NkGuiLireTailleRelative(w, ctx.layout.region);
							NkRect zone = pl.pose ? pl.rect : ctx.layout.region;
							if (relD.aW)
								zone.w = relD.w;
							if (relD.aH)
								zone.h = relD.h;
							// Les zones filles, cote a cote dans l'ordre du document. Chacune
							// prend sa fraction si elle en declare une, sinon une part egale.
							const NkArchiveNode *cd = NkGMonteCorps(w);
							uint32 nZones = 0;
							if (cd)
								for (uint32 k = 0; k < (uint32)cd->array.Size(); ++k)
									if (cd->array[k].IsObject() && cd->array[k].object)
										++nZones;
							if (cd && nZones > 0u) {
								// ⚠️ LE RESTE, ET NON UNE PART EGALE. Une zone qui ne declare pas
								//    sa fraction prend ce qui RESTE apres les fractions declarees,
								//    partage entre celles qui sont dans son cas. La regle « part
								//    egale » laissait une bande orpheline -- 197 px noirs a droite
								//    sur la capture du 17/09, alors que les onze criteres de
								//    l'epoque etaient verts. Un dock qui laisse un trou n'est pas
								//    un dock : les zones couvrent TOUT.
								float32 sommeDeclaree = 0.f;
								uint32 nImplicites = 0u;
								for (uint32 k = 0; k < (uint32)cd->array.Size(); ++k) {
									if (!cd->array[k].IsObject() || !cd->array[k].object)
										continue;
									const NkGuiTailleRel rk =
										NkGuiLireTailleRelative(*cd->array[k].object, zone);
									if (rk.aW)
										sommeDeclaree += rk.w;
									else
										++nImplicites;
								}
								float32 reste = zone.w - sommeDeclaree;
								if (reste < 0.f)
									reste = 0.f; // le document sur-declare : on ne rend pas negatif
								const float32 partReste =
									nImplicites > 0u ? reste / (float32)nImplicites : 0.f;
								float32 x = zone.x;
								uint32 vues = 0u;
								for (uint32 k = 0; k < (uint32)cd->array.Size(); ++k) {
									if (!cd->array[k].IsObject() || !cd->array[k].object)
										continue;
									const NkArchive &z = *cd->array[k].object;
									const NkString nom(NkGuiArchive::IdOf(z));
									const NkGuiTailleRel rz = NkGuiLireTailleRelative(z, zone);
									++vues;
									float32 larg = rz.aW ? rz.w : partReste;
									// La DERNIERE zone absorbe l'arrondi jusqu'au bord : trois
									// divisions flottantes ne retombent pas sur le pixel.
									if (vues == nZones && sommeDeclaree + partReste * (float32)nImplicites
															  <= zone.w + 0.5f)
										larg = zone.x + zone.w - x;
									const NkRect r = {x, zone.y, larg, zone.h};
									x += larg;
									++rap.zones;
									// L'HOTE DIT s'il fournit un panneau de ce nom.
									const bool servie = hooks && hooks->ZoneAncree(nom.CStr(), r);
									if (!servie) {
										// ⚠️ UNE ZONE QUE PERSONNE NE FOURNIT NE DOIT PAS ETRE UN
										//    TROU QUI RESSEMBLE A UN FOND. Elle se signale, et elle
										//    ECRIT SON NOM : c'est le nom que l'application doit
										//    servir, et sans lui l'hote cherche a l'aveugle.
										++rap.zonesSansPanneau;
										if (!kMuet) {
											NkGuiDrawList &dl = ctx.DL();
											const NkColor trait = ctx.theme.textMuted;
											dl.AddRect(r, trait, 1.f);
											for (float32 dd = 0.f; dd < r.w + r.h; dd += 12.f) {
												float32 x0 = r.x + dd, y0 = r.y;
												float32 x1 = r.x, y1 = r.y + dd;
												if (x0 > r.x + r.w) {
													y0 += x0 - (r.x + r.w);
													x0 = r.x + r.w;
												}
												if (y1 > r.y + r.h) {
													x1 += y1 - (r.y + r.h);
													y1 = r.y + r.h;
												}
												if (y0 <= r.y + r.h && x1 <= r.x + r.w)
													dl.AddLine({x0, y0}, {x1, y1}, trait, 1.f);
											}
											if (ctx.font && ctx.font->Valid())
												// `AddText` prend la LIGNE DE BASE, pas le haut :
												// sans l'ascendante, le nom sortait par le haut de
												// la fenetre -- la capture l'a montre coupe en deux.
												dl.AddText(ctx.font->Face(), ctx.font->TexId(),
														   {r.x + 6.f, r.y + 4.f + ctx.font->Ascent()},
														   nom.CStr(), trait);
										}
									}
									Noter(rap, nom, NkGuiArchive::TypeOf(z), r, prof + 1u, true, true);
								}
							}
							Noter(rap, id, t, zone, prof, true, horizontal, &ctx.layout.region);
							++rap.montes;
							return;
						}
						// ── LA HIERARCHIE : liste, arbre, element ────────────
						// ⚠️ LA QUESTION DU CONTENU, TRANCHEE ICI. Le format sait dire les
						//    elements de DEUX facons, et elles ne sont pas de meme nature :
						//      1. en les ECRIVANT (`items = [...]`, ou des enfants `Item` /
						//         `TreeItem`) -- c'est une liste STATIQUE, donc un widget,
						//         et le document la dit entierement ;
						//      2. en NOMMANT une source (`bind = ui.scene`) -- et la, RIEN
						//         ne peut la fournir : `NkGuiMonteEtat::Entree` ne porte
						//         qu'un booleen, un flottant et un texte. **Il n'existe
						//         aucun type liste dans l'etat du montage.**
						//    C'est EXACTEMENT la frontiere de la zone hote : la hierarchie
						//    d'une scene est une DONNEE de l'application, pas une
						//    disposition. On ne l'invente pas dans le document -- on la
						//    nomme, et une liste liee que personne ne remplit SE SIGNALE.
						case NkGuiRole::ListBox: {
							static const bool kMuette = []() {
								const char *v = getenv("NK_LISTE_MUTATION"); // =muette : pas de marqueur
								return v && v[0] == 'm';
							}();
							const NkGuiTailleRel relL = NkGuiLireTailleRelative(w, ctx.layout.region);
							const NkRect zone = ctx.NextItemRect(relL.aW ? relL.w : -1.f,
																 relL.aH ? relL.h : ctx.ItemHeight() * 5.f);
							NkVector<NkString> ecrits;
							const uint32 nEcrits = NkGListeChaines(w, "items", ecrits);
							const uint32 montesAvant = rap.montes;
							if (BeginListBox(ctx, lbl, zone)) {
								for (uint32 k = 0; k < nEcrits; ++k)
									(void)SelectItem(ctx, ecrits[k].CStr());
								MonterCorps(ctx, w, etat, rap, prof + 1u, false, false, hooks);
								EndListBox(ctx);
							}
							const bool rempli = (nEcrits > 0u) || (rap.montes > montesAvant);
							const NkString cle = NkGTexte(w, "bind", "");
							if (!rempli && cle.Size() > 0u) {
								// LIEE, ET JAMAIS REMPLIE. Sans ce marqueur, un arbre vide se
								// fait prendre pour un arbre sans elements, et le document a
								// l'air monte alors qu'il manque sa moitie.
								++rap.listesNonRemplies;
								if (!kMuette) {
									NkGuiDrawList &dl = ctx.DL();
									const NkColor trait = ctx.theme.textMuted;
									const float32 pas = 12.f;
									for (float32 dd = 0.f; dd < zone.w + zone.h; dd += pas) {
										float32 x0 = zone.x + dd, y0 = zone.y;
										float32 x1 = zone.x, y1 = zone.y + dd;
										if (x0 > zone.x + zone.w) {
											y0 += x0 - (zone.x + zone.w);
											x0 = zone.x + zone.w;
										}
										if (y1 > zone.y + zone.h) {
											x1 += y1 - (zone.y + zone.h);
											y1 = zone.y + zone.h;
										}
										if (y0 <= zone.y + zone.h && x1 <= zone.x + zone.w)
											dl.AddLine({x0, y0}, {x1, y1}, trait, 1.f);
									}
									// LE CHEMIN DE LA CLE, et pas « liste vide » : l'hote doit
									// savoir QUOI servir, pas seulement qu'il manque quelque chose.
									if (ctx.font && ctx.font->Valid())
										dl.AddText(ctx.font->Face(), ctx.font->TexId(),
												   {zone.x + 6.f, zone.y + 4.f}, cle.CStr(), trait);
								}
							}
							Noter(rap, id, t, zone, prof, true, horizontal, &ctx.layout.region);
							++rap.montes;
							return;
						}
						case NkGuiRole::Item: {
							(void)SelectItem(ctx, NkGTexte(w, "label", lbl).CStr());
							break;
						}
						case NkGuiRole::TreeItem: {
							// MEME FAUTE, MEME CORRECTIF QUE `Expander` : `expanded` est un
							// etat INITIAL. Le reposer a chaque image refermait la branche
							// sous le doigt de qui vient de l'ouvrir.
							const NkString titre = NkGTexte(w, "label", id.CStr());
							if (e && !e->initialise) {
								e->b = NkGBooleen(w, "expanded", false);
								e->initialise = true;
							}
							ctx.SetNodeOpen(ctx.GetId(titre.CStr()),
											e ? e->b : NkGBooleen(w, "expanded", false));
							const bool ouvert = TreeNode(ctx, titre.CStr());
							if (e)
								e->b = ouvert;
							if (ouvert) {
								MonterCorps(ctx, w, etat, rap, prof + 1u, false, false, hooks);
								TreePop(ctx);
							}
							break;
						}
						// ── LE SEPARATEUR : il ne se dessine pas, il PARTAGE ─
						// ⚠️ CE QUI FAIT UN SEPARATEUR N'EST PAS SON TRAIT. Un trait gris de
						//    quatre pixels se dessine sans rien separer. C'est qu'il DEPLACE
						//    la frontiere et que les DEUX voisins se recalculent -- et c'est
						//    pour cela qu'il est un CONTENEUR DE DEUX ENFANTS ici : ses
						//    voisins sont ses enfants, donc « les deux se recalculent » se
						//    mesure sans aller fouiller la fratrie, mecanisme que ce monteur
						//    n'a nulle part ailleurs.
						//
						// ⚠️ LE DOCUMENT POSE LE DEFAUT, LE GESTE VIT A COTE, et le depot
						//    avait deja le mecanisme : `NkGuiMonteEtat::Entree::initialise`
						//    -- « sans lui, chaque trame ecraserait ce que l'utilisateur a
						//    change ». Le ratio tire par l'utilisateur vit dans l'ETAT du
						//    montage, pour la duree de la session ; **rien n'est reecrit dans
						//    le `.nkgui`**, et c'est ce qui garde le fichier partageable.
						//    Le ratio plutot que des pixels : `SplitterRatio` le dit
						//    lui-meme, « un ratio survit au redimensionnement de la fenetre ».
						case NkGuiRole::Splitter: {
							// MUTATION DE BANC, NK_SEPARATEUR_MUTATION=fige : le ratio de
							// l'etat est ignore et le defaut du document repris a chaque
							// image. Le geste n'a alors plus d'effet -- c'est ce que le
							// critere du deplacement doit voir.
							static const bool kFige = []() {
								const char *v = getenv("NK_SEPARATEUR_MUTATION");
								return v && v[0] == 'f';
							}();
							const float32 defaut = NkGNombre(w, "ratio", 0.5f);
							const float32 rmin = NkGNombre(w, "min", 0.1f);
							const float32 rmax = NkGNombre(w, "max", 0.9f);
							const bool vertical = !NkGMotEgal(NkStringView(NkGTexte(w, "orientation", "Vertical").CStr()),
															  "Horizontal");
							if (e && !e->initialise) {
								e->f = defaut;
								e->initialise = true;
							}
							float32 ratio = (e && !kFige) ? e->f : defaut;
							if (ratio < rmin)
								ratio = rmin;
							if (ratio > rmax)
								ratio = rmax;
							const NkRect zone = pl.pose ? pl.rect : ctx.layout.region;
							const float32 epaisseur = 4.f;
							NkRect visuel{}, prise{};
							SplitterRects(zone, vertical, ratio, epaisseur, 12.f, &visuel, &prise);
							// La poignee, et le geste : `SplitterRatio` ecrit dans `ratio`.
							(void)SplitterRatio(ctx, lbl, zone, vertical, &ratio, rmin, rmax, epaisseur, 12.f);
							if (e && !kFige)
								e->f = ratio; // LE GESTE, garde dans l'etat -- jamais dans le document
							// Les deux enfants, chacun dans SA moitie. C'est ici que « les
							// deux voisins se recalculent » se produit.
							NkRect a = zone, b = zone;
							if (vertical) {
								a.w = visuel.x - zone.x;
								b.x = visuel.x + epaisseur;
								b.w = (zone.x + zone.w) - b.x;
							} else {
								a.h = visuel.y - zone.y;
								b.y = visuel.y + epaisseur;
								b.h = (zone.y + zone.h) - b.y;
							}
							MonterCorpsBorne(ctx, w, etat, rap, prof + 1u, hooks, a, b);
							Noter(rap, id, t, zone, prof, true, horizontal, &ctx.layout.region);
							++rap.montes;
							return;
						}
						// ── L'EN-TETE REPLIABLE (l'inspecteur du modeleur) ───
						// ⚠️ CE QUI FAIT UN PLIABLE N'EST PAS SON TRIANGLE, C'EST CE QU'IL
						//    CACHE. Un bloc replie ne monte PAS ses enfants : c'est le seul
						//    critere qui distingue un pliable d'un simple titre, et c'est
						//    celui que le banc mesure (l'enfant absent du releve).
						// ⚠️ `expanded` VIENT DU DOCUMENT, et il faut le poser AVANT d'appeler
						//    `CollapsingHeader` : le widget garde son etat par identifiant, et
						//    sans cette ligne le document ne commanderait rien -- la propriete
						//    serait un decor, exactement ce que ce fichier reproche ailleurs.
						case NkGuiRole::Expander: {
							// MUTATION DE BANC, NK_PLIABLE_MUTATION=toujours : tout est ouvert.
							// Elle prouve que le critere de l'enfant cache teste quelque chose.
							static const bool kToujours = []() {
								const char *v = getenv("NK_PLIABLE_MUTATION");
								return v && v[0] == 't';
							}();
							// 🔴 ET UNE SECONDE, `=fige`, QUI REMET LE DEFAUT DU 26/09. Elle
							//    reprend la valeur du document A CHAQUE IMAGE, comme le faisait
							//    ce `case` : l'accordeon se refermait alors sous le doigt, parce
							//    que le pli suivant ecrasait le geste. `NkGuiMonteEtat::Entree`
							//    porte pourtant `initialise` DEPUIS SA CREATION, et son
							//    commentaire dit exactement ca -- « chaque trame ecraserait ce
							//    que l'utilisateur a change ». Le `Splitter`, dix lignes plus
							//    haut, applique la regle ; le pliable ne l'appliquait pas.
							//    *Une regle ecrite dans le champ qui la porte n'est pas une
							//    regle appliquee.*
							static const bool kFige = []() {
								const char *v = getenv("NK_PLIABLE_MUTATION");
								return v && v[0] == 'f';
							}();
							const NkString titre = NkGTexte(w, "label", id.CStr());
							// LE DOCUMENT DONNE L'ETAT INITIAL, PAS UN ORDRE PERMANENT.
							if (e && !e->initialise) {
								e->b = NkGBooleen(w, "expanded", false);
								e->initialise = true;
							}
							const bool voulu = kToujours
												   ? true
												   : ((e && !kFige) ? e->b
																	: NkGBooleen(w, "expanded", false));
							ctx.SetNodeOpen(ctx.GetId(titre.CStr()), voulu);
							// 🔴 ET SON RECTANGLE RELEVE ETAIT CELUI DE SON ENFANT. Avec un
							//    `break`, le `Noter` generique enregistrait `BlocConsomme` pris
							//    APRES l'en-tete : sur un accordeon d'un seul bouton, le releve
							//    donnait exactement le rectangle du bouton. Qui vise « le milieu
							//    de l'accordeon » cliquait donc DANS son contenu -- c'est ce qui
							//    a fait rougir deux criteres sur un correctif juste. Le bloc est
							//    donc pris AVANT l'en-tete : le releve couvre l'en-tete ET le
							//    contenu, et sa premiere rangee EST la barre cliquable.
							const NkVec2 cExp = ctx.layout.cursor;
							const bool ouvert = CollapsingHeader(ctx, titre.CStr());
							// LE GESTE, garde dans l'etat -- jamais reecrit dans le document.
							if (e && !kToujours && !kFige)
								e->b = ouvert;
							if (ouvert)
								MonterCorps(ctx, w, etat, rap, prof + 1u, false, false, hooks);
							Noter(rap, id, t, BlocConsomme(ctx, cExp), prof, true, horizontal,
								  &ctx.layout.region);
							++rap.montes;
							return;
						}
						// ── LA BANDE D'ONGLETS ───────────────────────────────
						// ⚠️ LE FORMAT DIT `tabs`, PAS `items`. Le schema du role
						//    (`NkGuiValidate.h`) porte `tabs, bind, editable, closable` :
						//    un document qui ecrirait `items` se fait refuser, et c'est le
						//    negatif de ce lot.
						// ⚠️ ET ON N'ECRIT PAS DE WIDGET : `TabBar` existe dans NKGui depuis
						//    toujours. Le monteur appelle ce qui est la, comme pour les
						//    quatorze autres roles.
						case NkGuiRole::TabBar: {
							// MUTATION DE BANC, NK_ONGLETS_MUTATION=vide : la liste est
							// ignoree. Elle prouve que le critere de l'image teste quelque
							// chose ; sans la variable, rien ne change.
							static const bool kVide = []() {
								const char *v = getenv("NK_ONGLETS_MUTATION");
								return v && v[0] == 'v';
							}();
							NkVector<NkString> libelles;
							const uint32 n = kVide ? 0u : NkGListeChaines(w, "tabs", libelles);
							if (n == 0u) {
								// Une bande sans onglet ne peint rien : il n'y a rien a
								// selectionner, donc rien a souligner. On le COMPTE.
								aDessine = false;
								break;
							}
							// `TabBar` attend un tableau de `const char *` : on le construit
							// ici, sur la pile, borne par le plafond du kit.
							const char *ptr[32];
							uint32 m = n > 32u ? 32u : n;
							for (uint32 k = 0; k < m; ++k)
								ptr[k] = libelles[k].CStr();
							(void)TabBar(ctx, lbl, ptr, (int32)m);
							break;
						}
						// ═══════════════════════════════════════════════════════════
						//  LA ZONE HOTE -- le document dit OU, l'application peint QUOI
						// ═══════════════════════════════════════════════════════════
						//  C'est la frontiere du format, et elle est ici. Un viseur 3D, une
						//  toile, un editeur de texte ne sont pas des widgets : ce sont des
						//  RECTANGLES que l'hote remplit. Sans ce role, aucun document ne
						//  pourra jamais decrire une application reelle.
						//
						//  ⚠️ UNE ZONE QUE PERSONNE NE REMPLIT NE DISPARAIT PAS EN SILENCE,
						//     ET NE PEINT PAS DE FAUX CONTENU. Elle se signale : des hachures
						//     et son nom -- ce qu'aucun contenu plausible ne ressemble -- et
						//     elle se COMPTE (`hotesNonRemplis`). Un manque muet se fait
						//     prendre pour un fond ; un manque qui se voit se repare.
						// ═══════════════════════════════════════════════════
						//  DEUX CONTENEURS DE PLUS -- ET DEUX SEULEMENT
						// ═══════════════════════════════════════════════════
						case NkGuiRole::Flow: {
							// Horizontal AVEC retour a la ligne : c'est la seule difference avec
							// une `HBox`, et NKGui la porte deja (`BeginFlow`).
							const float32 gapF = NkGA(w, "gap") ? NkGNombre(w, "gap", -1.f) : -1.f;
							const NkVec2 cF = ctx.layout.cursor;
							BeginFlow(ctx, gapF);
							MonterCorps(ctx, w, etat, rap, prof + 1u, true, false, hooks);
							EndFlow(ctx);
							Noter(rap, id, t, BlocConsomme(ctx, cF), prof, true, horizontal, &ctx.layout.region);
							++rap.montes;
							return;
						}
						case NkGuiRole::Grid: {
							// `columns` est le seul attribut que NKGui demande. Un document qui ne
							// le donne pas decrit une grille d'UNE colonne -- ce qui est une
							// colonne, pas une erreur.
							const int32 colonnes = (int32)NkGNombre(w, "columns", 1.f);
							const float32 gapG = NkGA(w, "gap") ? NkGNombre(w, "gap", -1.f) : -1.f;
							// ⚠️ `sizes` EST ECRIT PAR LE FORMAT ET NKGui NE LE PREND PAS.
							//    `BeginGrid` n'a que le NOMBRE de colonnes. On le COMPTE plutot
							//    que de le jeter en silence : le document obtiendrait des colonnes
							//    EGALES la ou il demande des largeurs, et rien ne le dirait. Un
							//    document rendu AUTREMENT que demande est plus couteux qu'un
							//    document refuse.
							if (NkGA(w, "sizes"))
								++rap.attributsNonHonores;
							const NkVec2 cG = ctx.layout.cursor;
							BeginGrid(ctx, colonnes < 1 ? 1 : colonnes, gapG);
							MonterCorps(ctx, w, etat, rap, prof + 1u, false, false, hooks);
							EndGrid(ctx);
							Noter(rap, id, t, BlocConsomme(ctx, cG), prof, true, horizontal, &ctx.layout.region);
							++rap.montes;
							return;
						}
						// ═════════════════════════════════════════════════════
						//  LES MENUS -- ON APPELLE NKGui, ON NE REDESSINE RIEN
						// ═════════════════════════════════════════════════════
						//  Les trois cas qui suivent ne contiennent PAS UNE SEULE
						//  primitive de dessin, et c'est le point : `BeginMenuBar`,
						//  `BeginMenu` et `MenuItem` existaient. On relie, on n'ecrit pas.
						case NkGuiRole::MenuBar: {
							// La bande prend la largeur de sa region et la hauteur d'un
							// item, sauf si le document pose sa taille. `BeginMenuBar` se
							// sert de `rect.h` pour la hauteur des titres : une bande
							// degeneree donnerait des titres invisibles, et le compteur
							// `barresMenu` reste alors a zero pour le dire.
							const NkGuiTailleRel relB = NkGuiLireTailleRelative(w, ctx.layout.region);
							NkRect bande = pl.pose ? pl.rect : ctx.layout.region;
							if (!pl.pose) {
								bande.h = relB.aH ? relB.h : ctx.ItemHeight();
								if (relB.aW)
									bande.w = relB.w;
							}
							if (!BeginMenuBar(ctx, bande))
								break;
							++rap.barresMenu;
							// Les titres se posent horizontalement, et c'est `menuBarX`
							// qui les avance -- pas le curseur de mise en page.
							MonterCorps(ctx, w, etat, rap, prof + 1u, true, false, hooks);
							EndMenuBar(ctx);
							Noter(rap, id, t, bande, prof, true, horizontal, &ctx.layout.region);
							++rap.montes;
							return;
						}
						case NkGuiRole::Menu: {
							const NkString lbl = NkGTexte(w, "label", id.CStr());
							++rap.menus;
							// ⚠️ LE CONTENU D'UN MENU FERME NE SE MONTE PAS, et ce n'est pas
							//    une perte : NKGui ne dessine ses entrees que dans le popup
							//    ouvert. Mais un document dont aucun menu n'est deroule
							//    serait alors indiscernable d'un document dont les menus
							//    sont VIDES -- d'ou `menusOuverts`, qui separe les deux.
							if (BeginMenu(ctx, lbl.CStr())) {
								++rap.menusOuverts;
								MonterCorps(ctx, w, etat, rap, prof + 1u, false, false, hooks);
								EndMenu(ctx);
							}
							Noter(rap, id, t, ctx.layout.prevItem, prof, true, horizontal,
								  &ctx.layout.region);
							++rap.montes;
							return;
						}
						case NkGuiRole::MenuItem: {
							// ⚠️ HORS D'UNE CHAINE DE MENUS, ON REFUSE ET ON COMPTE. NKGui
							//    ne sait dessiner une entree que dans un popup ouvert
							//    (`curPopupLevel >= 0`) ; le document, lui, peut en ecrire
							//    une n'importe ou. La poser dans le flux en ferait un faux
							//    bouton -- *un chiffre juste sous un nom faux*, applique a
							//    l'interface. On ne l'approxime donc pas.
							if (ctx.curPopupLevel < 0) {
								++rap.elementsMenuHorsMenu;
								aDessine = false;
								break;
							}
							const NkString lbl = NkGTexte(w, "label", id.CStr());
							const NkString raccourci = NkGTexte(w, "shortcut", "");
							// `checked` vient du document ; si une donnee vivante existe
							// pour cette cle, elle fait foi (meme regle que `Checkbox`).
							const bool coche = e ? e->b : NkGBooleen(w, "checked", false);
							const bool actif = NkGBooleen(w, "enabled", true) && !ctx.IsDisabled();
							// L'appui est jete ICI COMME AILLEURS : le format n'a pas
							// d'evenement. Meme manque que `Button`, meme contournement
							// cote hote (l'identifiant nomme l'action).
							(void)MenuItem(ctx, lbl.CStr(), raccourci.Size() > 0u ? raccourci.CStr() : nullptr,
										   actif, coche);
							++rap.elementsMenu;
							break;
						}
						case NkGuiRole::Host: {
							++rap.hotes;
							// Un `pos`/`size` pose a deja arme `SetNextItemRect` avant le switch :
							// `NextItemRect` le rend tel quel. Sinon, la zone lit sa TAILLE RELATIVE
							// (« 60 % de mon parent ») ; a defaut, elle prend la largeur disponible et
							// quatre hauteurs d'item -- assez pour se voir.
							const NkGuiTailleRel relH = NkGuiLireTailleRelative(w, ctx.layout.region);
							const NkRect zone = ctx.NextItemRect(relH.aW ? relH.w : -1.f,
																 relH.aH ? relH.h : ctx.ItemHeight() * 4.f);
							const bool rempli = hooks && hooks->RemplirHote(ctx, id.CStr(), zone);
							if (!rempli) {
								++rap.hotesNonRemplis;
								// MUTATION DE BANC, NK_HOTE_MUTATION=sansmarqueur : la zone vide ne
								// trace plus rien. Elle sert a prouver que le critere du marqueur
								// teste quelque chose ; sans la variable, rien ne change.
								static const bool kSansMarqueur = []() {
									const char *v = getenv("NK_HOTE_MUTATION");
									return v && v[0] == 's';
								}();
								if (kSansMarqueur)
									break;
								NkGuiDrawList &dl = ctx.DL();
								// 🔴 LA COULEUR DU MARQUEUR EST CELLE DU TEXTE SECONDAIRE, PAS CELLE
								//    DE LA BORDURE. Premiere version : `theme.border`. Le banc
								//    comptait 3 778 pixels traces et passait au VERT -- et l'image
								//    ne montrait RIEN : la bordure est a 9 de luminance de l'aplat du
								//    panneau. Un marqueur qu'aucun oeil ne voit ne signale rien, et
								//    c'est exactement le defaut muet que ce role existe pour empecher.
								//    Le critere du banc compte desormais les pixels dont la LUMINANCE
								//    s'ecarte de l'aplat : la bordure y rougirait.
								const NkColor trait = ctx.theme.textMuted;
								dl.AddRect(zone, trait, 1.f);
								// LES HACHURES : elles disent « rien n'est monte ici », et aucun
								// contenu d'application ne leur ressemble.
								const float32 pas = 12.f;
								for (float32 d = 0.f; d < zone.w + zone.h; d += pas) {
									float32 x0 = zone.x + d, y0 = zone.y;
									float32 x1 = zone.x, y1 = zone.y + d;
									if (x0 > zone.x + zone.w) {
										y0 += x0 - (zone.x + zone.w);
										x0 = zone.x + zone.w;
									}
									if (y1 > zone.y + zone.h) {
										x1 += y1 - (zone.y + zone.h);
										y1 = zone.y + zone.h;
									}
									if (y0 <= zone.y + zone.h && x1 <= zone.x + zone.w)
										dl.AddLine({x0, y0}, {x1, y1}, trait, 1.f);
								}
								// ET SON NOM : « zone non remplie » sans dire LAQUELLE renverrait
								// l'hote a chercher. Le nom du noeud est la cle qu'il doit servir.
								if (ctx.font && ctx.font->Valid()) {
									const NkString h = NkGTexte(w, "hint", id.CStr());
									dl.AddText(ctx.font->Face(), ctx.font->TexId(),
											   {zone.x + 6.f, zone.y + 4.f}, h.CStr(), ctx.theme.textMuted);
								}
							}
							break;
						}
						default:
							aDessine = false;
							break;
					}

					// ⚠️ UN RECTANGLE POSE QUI N'A PAS ETE CONSOMME EST DESARME ICI.
					//    `SetNextItemRect` vaut pour le prochain widget AUTO-PLACE ; un role
					//    qui ne passe pas par `NextItemRect` le laisserait arme pour le
					//    SUIVANT, qui partirait aux coordonnees d'un autre. On le retire et on
					//    le COMPTE : un placement qui n'a pas pris doit se voir dans le releve,
					//    pas contaminer son voisin.
					// ⚠️ LE DENOMINATEUR N'EST PAS `poses`, ET C'EST UNE MESURE DU
					//    26/09. `poses` compte TOUT widget qui ecrit `pos`,
					//    conteneurs compris ; `posesHonores` n'est incremente que
					//    pour les FEUILLES, parce qu'un conteneur pose son rectangle
					//    dans son propre `case` et ne passe pas par `NextItemRect`.
					//    Comparer l'un a l'autre donnait « 18 honores sur 40
					//    demandes » sur un document dont AUCUNE feuille n'etait mal
					//    placee -- verifie : le releve du premier desaccord est reste
					//    VIDE. Le placement etait juste ; c'est le rapport qui
					//    comparait un sous-ensemble a un total.
					//
					//    `posesFeuilles` donne le denominateur qui a un sens. Sans
					//    lui, on aurait « corrige » un placement qui marche.
					if (pl.pose && !estConteneur) {
						++rap.posesFeuilles;
						if (ctx.nextItemRectSet) {
							ctx.nextItemRectSet = false;
							++rap.posesNonConsommes;
						} else {
							const NkRect &pv = ctx.layout.prevItem;
							const float32 dx = pv.x - pl.rect.x, dy = pv.y - pl.rect.y;
							if ((dx > -0.5f && dx < 0.5f) && (dy > -0.5f && dy < 0.5f))
								++rap.posesHonores;
							// ⚠️ LE PREMIER DESACCORD GARDE SES CHIFFRES (26/09).
							//    `posesHonores` disait « 18 sur 40 » sans dire OU ni
							//    DE COMBIEN -- et un compteur qui rougit peut rougir
							//    parce que le placement est faux, ou parce que le
							//    CONTROLE compare la mauvaise chose. Sans les deux
							//    rectangles, on ne peut pas trancher, et « corriger »
							//    un placement juste aurait casse ce qui marche.
							else if (!rap.aDesaccordPose) {
								rap.aDesaccordPose = true;
								rap.desaccordId = NkString(id);
								rap.desaccordDemande = pl.rect;
								rap.desaccordMonte = pv;
							}
						}
					}
					if (aDessine)
						++rap.montes;
					Noter(rap, id, t, ctx.layout.prevItem, prof, false, horizontal, &ctx.layout.region);
					if (rap.items.Size() > 0) {
						NkGuiMonteItem &dernier = rap.items[(uint32)rap.items.Size() - 1u];
						dernier.valeur = valeurMontee;
						dernier.aValeur = aValeurMontee;
						dernier.champVide = champVideMonte;
					}
					// Un widget feuille peut porter une apparence : elle se compte.
					CompterApparences(w, rap);
				}

				/// Le bloc qu'un CONTENEUR a consomme : du curseur d'avant celui
				/// d'apres. Un conteneur n'appelle pas `NextItemRect` pour lui-meme --
				/// il empile un sous-flux -- donc ni `prevItem` ni `lastItemRect` ne
				/// parlent de lui. Le curseur, si.
				static NkRect BlocConsomme(const NkGuiContext &ctx, const NkVec2 &c0) noexcept {
					NkRect r;
					r.x = c0.x;
					r.y = c0.y;
					r.w = ctx.layout.region.w;
					r.h = ctx.layout.cursor.y - c0.y;
					if (r.h < 0.f)
						r.h = 0.f;
					return r;
				}

				/// ⚠️ UNE SEULE LECTURE DE « LE REPOS » DANS TOUT LE FICHIER. Elle vit
				///    desormais en fonction libre (`NkGuiEtatEstRepos`) parce que le lecteur
				///    d'apparence, ecrit hors de cette classe, en a besoin lui aussi. Deux
				///    copies auraient fini par ne plus etre d'accord sur ce qu'est le repos,
				///    et le desaccord se serait vu en PIXELS, jamais dans un compteur.
				static bool EtatEstRepos(NkStringView lexeme) noexcept {
					return NkGuiEtatEstRepos(lexeme);
				}

				static void CompterApparences(const NkArchive &w, NkGuiMonteRapport &rap) noexcept {
					const NkArchiveNode *c = NkGMonteCorps(w);
					if (!c)
						return;
					for (uint32 k = 0; k < (uint32)c->array.Size(); ++k) {
						if (!c->array[k].IsObject() || !c->array[k].object)
							continue;
						const NkArchive &a = *c->array[k].object;
						if (!NkGMotEgal(NkGuiArchive::TypeOf(a), "appearance"))
							continue;
						++rap.apparencesLues;
						const NkStringView st = NkGuiArchive::StateOf(a);
						if (st.Size() > 0 && !EtatEstRepos(st))
							++rap.etatsNonAppliques;
					}
				}

				/// La region que prend un conteneur de premier plan : son `pos`/`size`
				/// s'il les ecrit, sinon ce qui reste de la region courante.
				static NkRect RegionCourante(NkGuiContext &ctx, const NkArchive &w) noexcept {
					NkRect r = ctx.layout.region;
					float32 x = 0.f, y = 0.f, cw = 0.f, ch = 0.f;
					if (LireVec2(w, "pos", x, y) && LireVec2(w, "size", cw, ch)) {
						r.x = x;
						r.y = y;
						r.w = cw;
						r.h = ch;
					}
					return r;
				}

				/// `pos = (120, 80)` est un JETON NU : deux nombres entre parentheses.
				static bool LireVec2(const NkArchive &b, const char *cle, float32 &x,
									 float32 &y) noexcept {
					const NkArchiveNode *n = b.FindNode(NkStringView(cle));
					if (!n)
						return false;
					float32 v[4];
					const uint32 c = NkGNombresDansLexeme(n->Lexeme(), v, 4u);
					if (c < 2u)
						return false;
					x = v[0];
					y = v[1];
					return true;
				}

				/// ⚠️ LE RECTANGLE D'UN WIDGET SE LIT DANS `layout.prevItem`, PAS DANS
				/// `lastItemRect`. Mesure du 2026-09-14, et elle a coute un releve faux :
				/// `lastItemRect` n'est pose que par `ButtonBehavior` et `BeginDropTarget`,
				/// c'est-a-dire par les widgets INTERACTIFS. Un `Text`, un `Separator`, un
				/// `Spacer` ne l'ecrivent jamais : le relever apres eux rend le rectangle du
				/// dernier BOUTON rencontre. Le premier releve de ce banc annoncait donc
				/// « 4 chevauchements » sur un empilement parfaitement correct.
				/// `layout.prevItem`, lui, est pose par CHAQUE `NextItemRect` -- tous les
				/// widgets auto-places y passent, interactifs ou non.
				/// ⚠️ ELLE PREND LA REGION, ET CE N'EST PAS UN PARAMETRE DE CONFORT :
				///    c'est le seul endroit du fichier par ou passent TOUS les widgets
				///    montes, avec le rectangle qu'ils ont REELLEMENT pris. Compter le
				///    debordement ailleurs aurait voulu dire le compter dans chaque `case`,
				///    c'est-a-dire en oublier.
				static void Noter(NkGuiMonteRapport &rap, const NkString &id, NkStringView role,
								  const NkRect &r, uint32 prof, bool conteneur,
								  bool axeHorizontal, const NkRect *region = nullptr) noexcept {
					NkGuiMonteItem it;
					it.id = id;
					it.role = NkString(role);
					it.rect = r;
					it.profondeur = prof;
					it.conteneur = conteneur;
					it.axeHorizontal = axeHorizontal;
					// Un demi-pixel de tolerance : les rectangles sont en flottants et un
					// bord qui touche exactement n'est pas un debordement.
					if (region && (r.x + r.w > region->x + region->w + 0.5f
						   || r.y + r.h > region->y + region->h + 0.5f)) {
						++rap.debordements;
						it.deborde = true;
					}
					rap.items.PushBack(it);
				}

				static void Copier(char *dst, int32 taille, const NkString &s) noexcept {
					int32 i = 0;
					for (; i < taille - 1 && i < (int32)s.Size(); ++i)
						dst[i] = s.Data()[i];
					dst[i] = '\0';
				}

		};

	} // namespace nkgui
} // namespace nkentseu

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
