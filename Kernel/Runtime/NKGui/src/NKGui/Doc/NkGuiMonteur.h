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
#include "NKGui/Core/NkGuiIcons.h" // P12 : `icon`, dessine par le jeu que l'hote pose
#include "NKGui/Widgets/NkGuiWidgets.h"
#include "NKGui/Doc/NkGuiCibles.h" // P27 : `platform`, le design par plateforme
#include "NKGui/Doc/NkGuiDispositions.h" // P10 : `layout`, APPLIQUE par `DockSpace`
#include "NKGui/Doc/NkGuiEcouteurs.h" // la table des ecouteurs, remplie par l'hote
#include "NKGui/Doc/NkGuiLangues.h"	 // `@t:cle` -> le texte dans la langue courante
#include "NKGui/Doc/NkGuiImages.h" // le registre nom -> texId, televerse une fois
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
			/// LA TOILE (27/09). Une zone hote QUI PORTE SON CADRAGE : le format tient
			/// le deplacement, l'echelle et la grille ; l'application dessine son
			/// contenu en coordonnees MONDE et repond aux evenements par la table
			/// d'ecouteurs.
			///
			/// ⚠️ CE N'EST PAS UN `Host` AVEC DES OPTIONS. Un `Host` rend un rectangle
			///    et s'arrete la ; une `Canvas` possede un etat qui SURVIT d'une image
			///    a l'autre (ou elle regarde, de combien elle grossit) et des gestes qui
			///    le changent. C'est cet etat qui justifie un role a part -- sans lui,
			///    chaque application reecrirait son zoom au curseur, et trois sur quatre
			///    zoomeraient au centre.
			Canvas,
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
			Table,
			// ── LES HUIT ROLES PROPOSES, ENTRES LE MEME JOUR ─────────────
			//  Decrits par les deux specifications d'interface (NkAnimaEditor
			//  doc 09 §1, NKUIDesign doc 21 §1), chacune donnant pour chacun
			//  ce qu'il fallait ecrire « EN ATTENDANT ». Ils cessent d'etre en
			//  attendant : leur schema entre au format le meme jour, dans le
			//  meme lot, parce qu'un role au schema que le monteur ignore fait
			//  REFUSER le document entier.
			ToggleButton,
			Badge,
			Tile,
			KeyDiamond,
			SplitButton,
			TimecodeField,
			VectorField,
			TokenField,
			// ── `ContextMenu` : LE REFUS EST LEVE (27/09), ET PAR L'OUTIL QU'IL
			//    RECLAMAIT LUI-MEME ─────────────────────────────────────────────
			// L'ancienne note disait : « NKGui a bien `BeginPopupMenu`, mais il ne rend
			// vrai que si quelqu'un a appele `ctx.OpenPopupAt(...)` AU CLIC DROIT -- et
			// ce monteur monte l'etat au REPOS : il n'a aucun clic droit a offrir. [...]
			// CE QU'IL FAUDRAIT POUR LE LEVER : un crochet d'ouverture cote hote, de la
			// meme forme que `RemplirHote` -- l'hote dit "ce clic droit ouvre le menu
			// nomme X". C'est une conception, pas un detail. »
			//
			// Le crochet existe : `NkGuiMonteHooks::MenuContextuelOuvre`. Rodolf, 27/09 :
			// « il faut les definir, rien ne doit manquer, donc met les outils qu'il faut
			// pour les atteindre ». C'est l'outil.
			//
			// ⚠️ ET LE RAISONNEMENT DE L'ANCIENNE NOTE RESTE VRAI POUR LE CAS FERME. Un
			//    role invisible par construction serait pire qu'un role refuse : il se
			//    compterait parmi les montes et personne n'irait chercher pourquoi rien
			//    n'apparait. Le `case` compte donc SEPAREMENT ce qui s'est ouvert
			//    (`menusContextuelsOuverts`) et ce qui est reste ferme faute de crochet
			//    (`menusContextuelsSansHote`) : le second est le chiffre qui dit « ce
			//    document declare un menu que personne n'ouvrira jamais ».
			ContextMenu,
			// ── P7 — `CurveField` : la courbe EDITABLE (27/09) ────────────────
			// Doc 09 §P7 : « la vitesse le long d'une trajectoire, la part de physique
			// dans le temps, l'intensite d'un vent — une courbe editable EN PLACE dans
			// une ligne de Details. `Chart` n'est pas editable. »
			//
			// ⚠️ LES POINTS APPARTIENNENT A L'HOTE, PAS AU DOCUMENT. Une courbe est une
			//    DONNEE : un fichier d'interface qui la porterait la figerait, et
			//    `NkGuiMonteEtat::Entree` ne sait tenir qu'un `float32`. La porte est
			//    donc `NkGuiMonteHooks::CourbeLiee` / `CourbeModifiee`, de la meme forme
			//    que `RemplirHote` et `ZoneAncree` : le document dit OU, COMMENT HAUT et
			//    ENTRE QUELLES BORNES, l'application dit QUOI et recoit les changements.
			CurveField,
			// ── QUATRE CONTENEURS DE PLUS (27/09) ─────────────────────────────
			// Rodolf : « je pense qu'il y a encore plein de conteneurs qu'on peut
			// ajouter, donc integre-les. »
			//
			// ⚠️ ET `Wrap` ET `Columns` N'EN SONT PAS, PARCE QU'ILS EXISTENT DEJA SOUS
			//    UN AUTRE NOM. `Flow` est une `HBox` AVEC retour a la ligne (c'est sa
			//    seule difference, et NKGui porte `BeginFlow`) ; `Grid` prend son nombre
			//    de colonnes. Les ajouter aurait donne deux noms pour un meme agencement
			//    — et le jour ou l'un gagne un reglage, les documents se partagent en
			//    deux familles qui ne se relisent plus.
			//
			// Les quatre ci-dessous ne se composent PAS a la main dans le format : il
			// n'existe aucune facon d'ecrire « centre ceci », « rentre ceci de 12 px »,
			// « garde ce rapport » ni « peins ceci par-dessus » avec les roles existants.
			Center,	 ///< centre ses enfants dans son rectangle
			Padding, ///< rentre son contenu d'une marge
			Aspect,	 ///< impose un rapport largeur/hauteur
			Overlay	 ///< peint ses enfants PAR-DESSUS le reste
		};

		/// La cle d'interaction d'un point de courbe : `<base>#<i>`.
		///
		/// ⚠️ ELLE EST ECRITE A LA MAIN, SANS `snprintf`. Ce noyau est zero-libc — il a
		///    son propre formateur (`NkFormat`, snprintf maison depuis le 20/09) et
		///    ce fichier n'en depend pas. Deux chiffres suffisent : la table de points
		///    plafonne a 64.
		///
		/// ⚠️ ET LA BASE EST LE `bind`, PAS L'INDICE SEUL. Deux courbes dans le meme
		///    document donneraient sinon les memes identifiants a leurs points, et
		///    tirer le troisieme point de l'une bougerait celui de l'autre — un defaut
		///    qui ne se voit qu'a deux courbes, c'est-a-dire jamais pendant l'ecriture.
		inline void NkGuiFormaterCleCourbe(char *out, uint32 taille, const char *base,
										   uint32 i) noexcept {
			if (!out || taille == 0u)
				return;
			uint32 k = 0u;
			if (base)
				for (; base[k] != '\0' && k + 5u < taille; ++k)
					out[k] = base[k];
			if (k + 4u < taille) {
				out[k++] = '#';
				if (i >= 10u)
					out[k++] = (char)('0' + (char)((i / 10u) % 10u));
				out[k++] = (char)('0' + (char)(i % 10u));
			}
			out[k] = '\0';
		}

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
			if (NkGMotEgal(n, "ToggleButton")) return NkGuiRole::ToggleButton;
			if (NkGMotEgal(n, "Badge")) return NkGuiRole::Badge;
			if (NkGMotEgal(n, "Tile")) return NkGuiRole::Tile;
			if (NkGMotEgal(n, "KeyDiamond")) return NkGuiRole::KeyDiamond;
			if (NkGMotEgal(n, "SplitButton")) return NkGuiRole::SplitButton;
			if (NkGMotEgal(n, "TimecodeField")) return NkGuiRole::TimecodeField;
			if (NkGMotEgal(n, "VectorField")) return NkGuiRole::VectorField;
			if (NkGMotEgal(n, "TokenField")) return NkGuiRole::TokenField;
			if (NkGMotEgal(n, "ContextMenu")) return NkGuiRole::ContextMenu;
			if (NkGMotEgal(n, "CurveField")) return NkGuiRole::CurveField;
			if (NkGMotEgal(n, "Center")) return NkGuiRole::Center;
			if (NkGMotEgal(n, "Padding")) return NkGuiRole::Padding;
			if (NkGMotEgal(n, "Aspect")) return NkGuiRole::Aspect;
			if (NkGMotEgal(n, "Overlay")) return NkGuiRole::Overlay;
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
			if (NkGMotEgal(n, "Canvas")) return NkGuiRole::Canvas;
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
				// ── L'INFOBULLE ET LE RACCOURCI (2026-09-27) ─────────────────
				/// ⚠️ DEUX COMPTEURS, PAS UN, et ils ne disent pas la meme chose.
				///    `raccourcisAffiches` compte les widgets qui DECLARENT un
				///    `shortcut` : il ne depend pas de la souris, donc un document
				///    se juge sans elle. `infobullesPosees` compte les infobulles
				///    REELLEMENT posees, ce qui exige un survol -- il vaut zero sur
				///    un montage au repos, et c'est normal.
				///
				///    Les confondre aurait fait croire qu'aucune infobulle n'existe
				///    dans un document qui en porte cent.
				uint32 raccourcisAffiches = 0;
				uint32 infobullesPosees = 0;
				/// Les widgets que `visible = false` — ou un `hide` — a retires du
				/// montage. ⚠️ Compte, parce qu'un document dont la moitie disparait
				/// doit pouvoir se LIRE dans le releve : sans ce chiffre, un `hide`
				/// de trop ressemble a un document plus pauvre.
				uint32 invisibles = 0;
				/// Les `icon` demandes, et ceux que le jeu d'icones n'a pas su
				/// rendre. ⚠️ Le second n'est pas « pas d'icone » : c'est « une
				/// icone a ete demandee et le glyphe manque » -- le jeu distingue
				/// deja `Glyph` de `Fallback`, et un carre muet est exactement le
				/// defaut que ce compteur existe pour rendre visible.
				uint32 iconesDemandees = 0;
				uint32 iconesManquantes = 0;
				/// LES IMAGES (27/09). Meme forme que les icones, et pour la meme
				/// raison : une image demandee qui ne se peint pas doit se COMPTER.
				/// ⚠️ `imagesNonResolues` INCREMENTE AUSSI `attributsNonHonores`, et
				///    ce n'est pas un double comptage : le premier dit « cette image
				///    manque », le second « ce document a demande quelque chose qui
				///    n'est pas arrive ». Les bancs qui surveillent le second depuis
				///    des mois continuent donc de voir la perte -- retirer
				///    l'increment aurait rendu MUET un defaut qu'ils attrapaient.
				///    Le detail des causes (fichier absent, hote pas pret,
				///    televersement echoue) vit dans `NkGuiImagesReleve()`.
				uint32 imagesPeintes = 0;
				uint32 imagesNonResolues = 0;
				/// LES EVENEMENTS EXPOSES (27/09) : combien de widgets ont remis au
				/// moins un evenement a un ecouteur, pendant cette image.
				/// ⚠️ CE N'EST PAS LE NOMBRE D'EVENEMENTS. Un widget qui recoit un
				///    deplacement, un appui et une molette dans la meme image compte
				///    UNE fois. Le detail par evenement vit dans la table elle-meme
				///    (`servis`, `traites`, `sansDestinataire`) -- deux compteurs pour
				///    deux questions, et les melanger rendrait les deux illisibles.
				uint32 evenementsExposes = 0;
				/// LES TOILES (27/09). `toilesNonRemplies` a la meme fonction que
				/// `hotesNonRemplis` : une toile que personne ne peint se SIGNALE.
				/// ⚠️ `toilesZoomees` ET `toilesDeplacees` NE SONT PAS DE LA
				///    STATISTIQUE : ce sont les deux seuls chiffres qui disent que les
				///    GESTES ont eu un effet. Sans eux, une toile qui s'affiche mais ne
				///    bouge plus aurait exactement le meme releve qu'une toile saine.
				uint32 toiles = 0;
				uint32 toilesNonRemplies = 0;
				uint32 toilesZoomees = 0;
				uint32 toilesDeplacees = 0;
				/// LES CONTENEURS ET LEUR TAILLE, EN FLUX (27/09).
				/// ⚠️ DEUX CHIFFRES PARCE QU'IL Y A DEUX SITUATIONS, ET UNE SEULE EST
				///    UN DEFAUT. `conteneursTailleFlux` compte ceux qui DECLARENT
				///    `size` et decoupent desormais leur rectangle dans le flux --
				///    avant le 27/09 ce `size` n'etait lu par personne et deux `Panel`
				///    freres se repeignaient l'un l'autre, exactement.
				///    `conteneursSansTaille` compte ceux qui ne disent NI `size` NI
				///    `sizeRel` : ils prennent « tout ce qui reste », ce qui est juste
				///    pour le dernier et ambigu des qu'il a un frere. Les additionner
				///    aurait rendu le defaut invisible dans le total.
				uint32 conteneursTailleFlux = 0;
				uint32 conteneursSansTaille = 0;
				/// Un conteneur qui DECLARE `size` et dont la brique ne sait pas
				/// prendre de rectangle. Aujourd'hui : `Table`, parce que
				/// `BeginTable` tient sa propre mise en page de colonnes.
				///
				/// ⚠️ IL EXISTE POUR NE PAS REFAIRE LE DEFAUT QU'ON VIENT DE
				///    CORRIGER. `size` ignore en silence, c'est exactement ce que
				///    `Window`/`Panel` faisaient jusqu'au 27/09 : le document
				///    demandait une taille, personne ne la lisait, et rien ne le
				///    disait. Ici la taille n'est pas honoree non plus — mais elle
				///    est COMPTEE, donc le document peut savoir qu'il demande
				///    quelque chose que l'outil ne tient pas.
				uint32 taillesNonHonorees = 0;
				/// P10 : les zones d'un `DockSpace` placees par la section `layout` du
				/// document (`dock "outils" left 0.16`).
				///
				/// 🔴 IL EXISTE PARCE QUE `layout` N'ETAIT LU PAR PERSONNE. Le lecteur
				///    `NkGuiLireDispositions` etait ecrit, valide et compte depuis le
				///    27/09 au matin — et son SEUL appelant etait un banc. Le monteur ne
				///    l'appelait jamais. Ce chiffre est ce qui separe « la section est
				///    analysee » de « la section AGIT » : un lecteur prouve par un banc ne
				///    prouve pas que le produit s'en sert.
				uint32 zonesAmarrees = 0;
				/// Les menus contextuels que l'hote a OUVERTS, et ceux qui restent
				/// fermes faute de crochet.
				///
				/// ⚠️ DEUX CHIFFRES, ET C'EST LE REFUS D'ORIGINE QUI LES EXIGE. Il disait
				///    qu'un role invisible par construction serait pire qu'un role
				///    refuse, « parce qu'il se compterait parmi les montes et que
				///    personne n'irait chercher pourquoi rien n'apparait ». Un seul total
				///    aurait fait exactement cela : `menusContextuelsSansHote` est le
				///    chiffre qui dit « ce document declare un menu que personne
				///    n'ouvrira jamais ».
				uint32 menusContextuelsOuverts = 0;
				uint32 menusContextuelsSansHote = 0;
				/// P7 : les `CurveField` rencontres, ceux que l'hote n'a pas servis, et
				/// les points qu'un geste a REELLEMENT deplaces.
				///
				/// ⚠️ LE TROISIEME EST LE SEUL QUI PROUVE L'EDITION. `courbes` monte pour
				///    une courbe seulement DESSINEE, et c'est precisement ce que le
				///    document 09 §P7 refuse (« `Chart` n'est pas editable »). Un banc qui
				///    ne compterait que les deux premiers serait vert sur un champ de
				///    courbe qu'on ne peut pas toucher.
				uint32 courbes = 0;
				uint32 courbesSansHote = 0;
				uint32 pointsCourbeDeplaces = 0;
				/// P27 : les widgets qu'une cible a ECARTES, et les noms de cible que le
				/// lecteur n'a pas reconnus.
				///
				/// 🔴 LE PREMIER DOIT FIGURER DANS LE VERDICT DES SONDES, A COTE DE
				///    `invisibles`. Un document qui ecrirait `platform = Mobile` partout
				///    donne, sur un PC, une fenetre PARFAITEMENT VIDE — et aucun message
				///    d'erreur, parce que rien n'est invalide. C'est le pire des silences :
				///    un document juste qui n'affiche rien. Sans ce chiffre, « mon
				///    interface a disparu » se cherche a la main.
				///
				/// ⚠️ ET LES DEUX SONT SEPARES PARCE QU'ILS ONT DEUX REMEDES. Un ecart
				///    voulu se corrige en changeant de cible d'apercu ; une cible inconnue
				///    se corrige dans le DOCUMENT, c'est une faute de frappe.
				uint32 ecartesParPlateforme = 0;
				uint32 ciblesInconnues = 0;
				/// Le montage par RACINE NOMMEE : celles qu'on a trouvees, et celles
				/// qu'on a demandees en vain — avec le DERNIER nom demande.
				///
				/// 🔴 LE NOM EST GARDE, ET C'EST LE POINT. Un appelant qui ignore le
				///    retour de `Monter(ctx, doc, "barre", ...)` monterait une region
				///    VIDE, sans aucune erreur, et chercherait a la main pourquoi sa
				///    barre a disparu. Une faute de frappe dans un nom de racine est la
				///    faute la plus probable de ce mecanisme ; un compteur sans le nom
				///    dirait qu'il manque quelque chose sans dire quoi.
				uint32 racinesMontees = 0;
				uint32 racinesIntrouvables = 0;
				NkString derniereRacineIntrouvable;
				/// Les quatre conteneurs du 27/09. `surimpressions` compte les
				/// `Overlay` ouverts ; `centrages` les `Center` qui ont VRAIMENT
				/// centre, et `centragesImpossibles` les enfants qui ne declarent pas
				/// leur taille.
				///
				/// 🔴 LES DEUX DERNIERS SONT SEPARES PARCE QU'UN `Center` PEUT
				///    PARFAITEMENT NE RIEN CENTRER. Une interface immediate ne connait
				///    la taille d'un enfant qu'APRES l'avoir monte ; ce role centre
				///    donc ce que le document DECLARE. Un enfant sans `size` n'est pas
				///    centre — et si ces deux chiffres n'en faisaient qu'un, un
				///    document entier pourrait etre colle en haut a gauche pendant
				///    qu'un compteur dirait « centre ».
				uint32 surimpressions = 0;
				uint32 centrages = 0;
				uint32 centragesImpossibles = 0;
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
						// ── CE QU'UN COMPORTEMENT PEUT CHANGER (27/09) ───────
						/// `show` / `hide` / `toggle` / `enable` / `disable` ecrivent
						/// ICI, et le monteur les lit a la place du document.
						///
						/// ⚠️ DEUX DRAPEAUX ET UN « QUELQU'UN L'A-T-IL DIT ? ». Sans
						///    `visibiliteDite`, on ne distinguerait pas « le document
						///    dit visible » de « personne n'a rien dit » : la valeur
						///    du document serait ecrasee des la premiere image par un
						///    defaut, et `visible = false` ecrit dans le fichier
						///    n'aurait plus aucun effet.
						bool visible = true;
						bool visibiliteDite = false;
						bool actif = true;
						bool activiteDite = false;
						/// La raison d'un `disable x because "..."`, lue au survol.
						/// ⚠️ Un element grise SANS raison est ce que la famille
						///    interdit (doc 3 §14quater) : ce champ existe pour que la
						///    raison VOYAGE jusqu'a l'infobulle.
						char raison[128] = {0};
						// ── CE QU'UN COMPORTEMENT PEUT CHANGER AUSSI (Rodolf, 27/09) ──
						/// L'ICONE et l'IMAGE posees par `set x.icon = ...` et
						/// `set x.image = ...`.
						///
						/// Pourquoi elles existent : un chevron d'accordeon qui passe de
						/// `>` a `v` est une ICONE dans la plupart des interfaces, et
						/// `set` ne savait poser que `text`, `value`, `checked`,
						/// `visible`, `enabled`. Un document devait donc ecrire DEUX
						/// widgets et les `show`/`hide` l'un apres l'autre : deux
						/// identifiants, deux entrees d'etat, et deux fois le risque
						/// d'en oublier un.
						///
						/// ⚠️ LEUR DRAPEAU « QUELQU'UN L'A-T-IL DIT ? », POUR LA MEME
						///    RAISON QUE `visibiliteDite` juste au-dessus. Une chaine
						///    vide est une valeur LEGITIME (« retire l'icone ») : sans
						///    le drapeau on ne la distinguerait pas de « personne n'a
						///    rien dit », et l'icone ecrite dans le document serait
						///    effacee des la premiere image.
						char icone[128] = {0};
						bool iconeDite = false;
						char image[256] = {0};
						bool imageDite = false;
				};

				// ── P10 : LES DISPOSITIONS D'AMARRAGE DU DOCUMENT ────────────────
				// 🔴 ELLES ETAIENT LUES PAR PERSONNE — SAUF UN BANC. `NkGuiLireDispositions`
				//    existait depuis le 27/09 au matin et son SEUL appelant etait
				//    `NKGuiMonteTest` : le monteur ne l'appelait jamais, donc un document
				//    qui ecrivait `dock "outils" left 0.16` etait analyse, valide,
				//    compte... et ignore. *Un banc qui prouve son propre lecteur ne prouve
				//    pas que le produit s'en sert* — c'est la forme exacte de « declarer
				//    n'est pas livrer », et je l'avais annoncee comme livree.
				//
				// ⚠️ ELLES VIVENT DANS L'ETAT, PAS DANS LE RAPPORT. Le rapport dit ce
				//    qu'un montage a FAIT ; ceci est une ENTREE du montage, lue une fois
				//    par `Preparer`. Les mettre dans le rapport aurait fait d'un compte
				//    rendu une source de verite, et l'ordre des deux aurait fini par
				//    compter sans que rien ne le dise.
				NkVector<NkGuiDisposition> dispositions;
				NkGuiRapportDispositions rapportDispositions;

				/// La disposition a appliquer : celle que le document nomme `defaut`, ou
				/// la premiere s'il n'y en a pas. Rend nul quand le document n'en a aucune
				/// — et alors `DockSpace` garde son partage par fractions, inchange.
				/// ⚠️ P27 : LA CIBLE PASSE AVANT LE NOM, ET L'ORDRE EST LE POINT. Un
				///    document qui porte `layout "defaut"` ET `layout "defaut"
				///    { platform = Mobile }` doit rendre le SECOND sur un telephone,
				///    alors que les deux portent le meme nom. Chercher d'abord « celle
				///    qui s'appelle defaut » aurait rendu la premiere ecrite, c'est-a-dire
				///    la version bureau, et le design mobile n'aurait jamais servi — un
				///    document juste dont une moitie ne s'affiche nulle part.
				///
				/// ⚠️ ET LE REPLI EST EXPLICITE, EN QUATRE TEMPS : (1) ma cible et le nom
				///    `defaut`, (2) ma cible, quel que soit son nom, (3) `Toutes` et
				///    `defaut`, (4) la premiere. Sans le temps (3), une cible sans
				///    disposition propre n'aurait rien — alors que le document en a une
				///    qui vaut partout.
				const NkGuiDisposition *DispositionActive() const noexcept {
					if (dispositions.Size() == 0u)
						return nullptr;
					const NkGuiCible ici = NkGuiCibleCourante();
					const uint32 n = (uint32)dispositions.Size();
					for (uint32 i = 0; i < n; ++i)
						if (dispositions[i].cible == ici
							&& NkGMotEgal(NkStringView(dispositions[i].nom.CStr()), "defaut"))
							return &dispositions[i];
					for (uint32 i = 0; i < n; ++i)
						if (dispositions[i].cible == ici)
							return &dispositions[i];
					for (uint32 i = 0; i < n; ++i)
						if (dispositions[i].cible == NkGuiCible::Toutes
							&& NkGMotEgal(NkStringView(dispositions[i].nom.CStr()), "defaut"))
							return &dispositions[i];
					for (uint32 i = 0; i < n; ++i)
						if (dispositions[i].cible == NkGuiCible::Toutes)
							return &dispositions[i];
					return &dispositions[0];
				}

				/// Le cote demande pour un panneau, dans la disposition active. Rend faux
				/// quand ce panneau n'y figure pas : « pas d'amarrage ecrit » et « amarre
				/// au centre » sont deux choses, et les confondre ferait disparaitre les
				/// zones que le document n'a pas nommees.
				/// ⚠️ ELLE PREND UN `const char *`, PAS UNE VUE, ET C'EST `NkGMotEgal` QUI
				///    LE DICTE : sa signature est (vue, litteral). Comparer deux vues
				///    aurait demande une SECONDE fonction de comparaison dans ce fichier,
				///    c'est-a-dire deux facons de dire « meme nom ».
				bool Amarrage(const char *panneau, NkGuiCote &coteOut,
							  float32 &fractionOut) const noexcept {
					const NkGuiDisposition *d = DispositionActive();
					if (!d || !panneau)
						return false;
					for (uint32 i = 0; i < (uint32)d->amarrages.Size(); ++i) {
						if (!NkGMotEgal(NkStringView(d->amarrages[i].panneau.CStr()), panneau))
							continue;
						coteOut = d->amarrages[i].cote;
						fractionOut = d->amarrages[i].fraction;
						return true;
					}
					return false;
				}

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
		/// LE LIBELLE D'UN WIDGET : celui du document, ou — a defaut — la traduction
		/// de son IDENTIFIANT.
		///
		/// 🔴 LA CLE EST DEJA L'IDENTIFIANT, ET L'ECRIRE DEUX FOIS EST UNE DETTE.
		///    Apres le passage de NKUIDesign au multilingue, vingt-deux lignes du
		///    document disaient litteralement deux fois la meme chose :
		///
		///        MenuItem "design.enregistrer" { label = "@t:design.enregistrer" }
		///
		///    Rodolf, 27/09 : « suis d'accord ». Un widget SANS `label` cherche
		///    desormais `@t:<son id>`.
		///
		/// ⚠️ ET C'EST RETRO-COMPATIBLE, ce qui est la raison pour laquelle on peut
		///    le faire sans rien casser. Sans entree dans la table, la recherche
		///    rend LA CLE -- c'est-a-dire l'identifiant -- exactement ce que ces
		///    sites affichaient avant en passant `id.CStr()` en defaut. Aucun
		///    document existant ne change d'apparence.
		///
		/// ⚠️ UN `label` EXPLICITE GAGNE TOUJOURS, et il reste indispensable : deux
		///    widgets peuvent porter le MEME identifiant d'action avec des libelles
		///    differents (`design.grouper` s'ecrit « Grouper la selection » dans le
		///    menu et « Grouper » dans la barre d'outils). Le repli sert le cas
		///    courant ; il ne supprime pas le cas particulier.
		inline NkString NkGLibelle(const NkArchive &w, const NkString &id) noexcept;

		/// LA FORME STABLE d'un texte du document : sa cle s'il est traduit, sa valeur
		/// brute sinon. A employer PARTOUT ou un texte sert d'identite.
		///
		/// 🔴 ELLE EXISTE PARCE QUE SIX SITES FABRIQUAIENT UNE IDENTITE A PARTIR D'UN
		///    LIBELLE, et que la bascule de langue est A CHAUD : l'identite changeait
		///    donc en pleine session, et les sections repliees se rouvraient au moment
		///    meme ou l'utilisateur changeait de langue. Voir `NkGuiFormeStable`.
		inline NkString NkGTexteStable(const NkArchive &b, const char *cle,
									   const char *defaut) noexcept {
			NkString out;
			if (!b.GetString(NkStringView(cle), out))
				out = NkString(defaut);
			return NkGuiFormeStable(out);
		}

		/// Un texte du document, TRADUIT quand il se donne comme une cle.
		///
		/// 🔴 `@t:` EST EXPLICITE, ET C'EST TOUT L'INTERET. Deux facons plus courtes
		///    ont ete ecartees :
		///
		///    - traduire TOUT texte en le cherchant dans la table (le repli rend la
		///      cle, donc rien ne casserait). Non : un libelle qui ressemble par
		///      hasard a une cle se ferait traduire, et le defaut serait invisible
		///      dans la langue ou les deux coincident ;
		///    - le prefixe `@` seul, comme les jetons de theme. Non : `@` sert deja
		///      aux couleurs, et un document qui ecrit `text = "@rihenuniverse"` --
		///      ce qui arrive -- verrait son arobase mangee.
		///
		///    `@t:design.enregistrer` ne peut se confondre avec rien, et se cherche
		///    d'un `grep` le jour ou on voudra recenser ce qui est traduit.
		///
		/// ⚠️ ET LE REPLI RESTE LA CLE. `@t:` sur une cle absente affiche
		///    `design.enregistrer` : laid, visible, corrigeable. Voir `NkGuiLangues.h`.
		inline NkString NkGTexte(const NkArchive &b, const char *cle, const char *defaut) noexcept {
			NkString out;
			if (!b.GetString(NkStringView(cle), out))
				out = NkString(defaut);
			const char *s = out.CStr();
			if (s && s[0] == '@' && s[1] == 't' && s[2] == ':' && s[3] != '\0')
				return NkString(NkGuiTexteLangue(s + 3));
			return out;
		}

		/// Definie APRES `NkGTexte`, qu'elle appelle. Sa documentation est au-dessus,
		/// a sa declaration -- c'est la que le lecteur la cherche.
		inline NkString NkGLibelle(const NkArchive &w, const NkString &id) noexcept {
			NkString parDefaut("@t:");
			parDefaut.Append(id);
			return NkGTexte(w, "label", parDefaut.CStr());
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
		//     NKCraft : « un rectangle POSE pour le PROCHAIN widget seulement,
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
		//     l'interface de NKCraft avec eux la FIGERAIT a une seule taille de
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

		/// MUTATION DE BANC, `NK_TAILLE_MUTATION=flux` : `size` redevient illisible
		/// pour un conteneur EN FLUX, c'est-a-dire le defaut du 27/09 remis a la
		/// demande. Elle existe pour qu'un temoin puisse ROUGIR -- un critere qui ne
		/// peut pas echouer ne prouve rien.
		///
		/// ⚠️ ELLE NE MET RIEN EN CACHE, ET C'EST LA DIFFERENCE AVEC CELLE DE
		///    `sizeRel`. Un `static const bool` initialise une fois par PROCESSUS
		///    obligerait le banc a se relancer pour comparer les deux mondes ; ici il
		///    pose la variable entre deux montages et lit les deux images dans le meme
		///    processus, ce qui rend la comparaison pixel a pixel possible. Le cout est
		///    un `getenv` par conteneur au montage, pas par image.
		inline bool NkGuiTailleFluxIgnoree() noexcept {
			const char *v = getenv("NK_TAILLE_MUTATION");
			return v && v[0] == 'f';
		}

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
		
		/// Deux chiffres, zero devant, sans passer par la bibliotheque C.
		///
		/// ⚠️ ELLE EXISTE PARCE QU'UN FORMATAGE PEUT DEPENDRE DE LA CULTURE. Ce
		///    depot a deja vu un `%.2f` rendre « 0,9 » en fr-FR la ou un point
		///    etait attendu. Des chiffres poses un par un ne dependent de rien.
		inline void PoserDeuxChiffres(char *out, int32 &pos, int32 v) noexcept {
			if (v < 0)
				v = 0;
			if (v > 99)
				v = 99;
			out[pos++] = (char)('0' + (v / 10));
			out[pos++] = (char)('0' + (v % 10));
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
		//  LES HACHURES — P11, et la marque d'une zone non servie
		// ─────────────────────────────────────────────────────────────────────
		//  🔴 ELLES ETAIENT ECRITES DANS LE `case Host`, ET P11 EN DEMANDAIT UNE
		//     SECONDE COPIE. « Une proposition non appliquee doit se voir au
		//     premier coup d'oeil, partout » (doc 09 §1 P11) -- donc sur une
		//     surface quelconque, pas seulement sur une zone vide. Recopier la
		//     boucle aurait donne deux motifs qui se seraient mis a differer :
		//     ce depot a deja paye « deux compteurs sans code commun ».
		//
		//  ⚠️ LE MOTIF DIT « RIEN N'EST MONTE ICI » **ET** « PAS ENCORE A VOUS ».
		//     Les deux sens se rejoignent : une surface hachuree n'est pas une
		//     surface ordinaire. C'est aussi pour ca qu'aucun contenu
		//     d'application ne doit lui ressembler.
		inline void NkGuiHachurer(NkGuiDrawList &dl, const NkRect &zone, const NkColor &trait,
								  float32 pas = 12.f) noexcept {
			if (zone.w <= 0.f || zone.h <= 0.f || pas <= 0.f)
				return;
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
		}

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
				/// P11 — `appearance { pattern = Hatch }`. « Une proposition non
				/// appliquee doit se voir au premier coup d'oeil, PARTOUT. La
				/// couleur seule ne suffit pas (daltonisme), et la hachure est
				/// deja le langage du monteur pour "zone non servie" — ici, pour
				/// "pas encore a vous". » (doc 09 §1 P11)
				bool aHachures = false;
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
				// P11 : `pattern = Hatch`. Un motif que ce monteur ne connait pas
				// se COMPTE au lieu d'en peindre un autre -- une surface pointillee
				// la ou le document demandait des hachures dirait faux.
				if (NkGA(ap, "pattern")) {
					if (NkGMotEgal(NkStringView(NkGTexte(ap, "pattern", "").CStr()), "Hatch"))
						a.aHachures = true;
					else
						++a.nonRendues;
				}
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
		/// LE CADRAGE D'UNE TOILE : ou elle regarde, et de combien elle grossit.
		///
		/// ⚠️ `zoom` EST UN FACTEUR, PAS UN POURCENTAGE, et il ne vaut JAMAIS ZERO.
		///    Un zoom nul rendrait la transformation non inversible : l'ecran ne se
		///    reconvertirait plus en monde, et tout clic tomberait a l'infini. Le
		///    monteur le borne, et l'application qui le pose devrait en faire autant.
		struct NkGuiVue {
				NkVec2 centre{0.f, 0.f}; ///< le point du MONDE au centre de la zone
				float32 zoom = 1.f;		 ///< pixels par unite de monde

				/// Monde -> ecran, dans la zone `r`.
				NkVec2 VersEcran(const NkVec2 &monde, const NkRect &r) const noexcept {
					return NkVec2(r.x + r.w * 0.5f + (monde.x - centre.x) * zoom,
								  r.y + r.h * 0.5f + (monde.y - centre.y) * zoom);
				}
				/// Ecran -> monde. L'inverse EXACT de `VersEcran` : les deux vivent
				/// cote a cote pour que personne n'en reecrive une seule des deux.
				NkVec2 VersMonde(const NkVec2 &ecran, const NkRect &r) const noexcept {
					const float32 z = (zoom > 0.0001f) ? zoom : 0.0001f;
					return NkVec2(centre.x + (ecran.x - r.x - r.w * 0.5f) / z,
								  centre.y + (ecran.y - r.y - r.h * 0.5f) / z);
				}
		};

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

				/// LA TOILE : comme `RemplirHote`, mais la VUE vient avec.
				///
				/// Le format tient le cadrage (deplacement et echelle), la grille et les
				/// gestes qui les changent ; l'application dessine son contenu EN
				/// COORDONNEES MONDE et repond aux evenements par la table d'ecouteurs.
				///
				/// 🔴 POURQUOI LE FORMAT TIENT LA VUE ET PAS L'APPLICATION. Quatre
				///    surfaces de la famille veulent exactement le meme cadrage : la toile
				///    Design, la toile des comportements, la frise d'animation et le
				///    graphe des inclusions. Laisser chacune l'ecrire donnerait quatre
				///    implementations du zoom au curseur -- dont trois finiraient par
				///    zoomer au centre, ce qui ne se remarque que quand on l'a longtemps
				///    utilise ailleurs.
				///
				/// ⚠️ ET LE FORMAT NE VA PAS PLUS LOIN. Il ne sait pas ce qu'il y a dans
				///    la toile, donc il ne fait AUCUN picking : c'est l'ecouteur qui
				///    recoit le clic, en coordonnees locales, et qui sait seul quel objet
				///    est dessous. La frontiere est la, et elle est nette.
				///
				/// ⚠️ LE DEFAUT RETOMBE SUR `RemplirHote`, ET PAS SUR `false`. Une
				///    application ecrite avant ce role continue donc de remplir sa toile ;
				///    seule la vue lui manque. Rendre `false` par defaut aurait fait
				///    disparaitre le contenu de toutes les toiles existantes le jour de
				///    la mise a jour.
				virtual bool RemplirToile(NkGuiContext &ctx, const char *nom, const NkRect &zone,
										  const NkGuiVue &vue) noexcept {
					(void)vue;
					return RemplirHote(ctx, nom, zone);
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

				/// L'OUVERTURE D'UN MENU CONTEXTUEL : l'hote doit-il ouvrir le menu
				/// nomme `nom` a cette image, et OU ? Rend vrai et remplit `posOut`.
				///
				/// ⚠️ C'EST L'OUTIL QUE LE REFUS DE `ContextMenu` RECLAMAIT, ET IL EST
				///    CHEZ L'HOTE POUR UNE RAISON. Le monteur monte l'etat au REPOS : il
				///    n'a pas de clic droit a offrir, et il ne DOIT pas en inventer un.
				///    C'est l'application qui sait quel clic droit, sur quoi, ouvre quel
				///    menu — un monteur qui ouvrirait « le menu du widget survole » aurait
				///    decide a sa place, et le document n'aurait plus aucun moyen de dire
				///    autre chose.
				///
				/// ⚠️ ET LE DEFAUT EST `false`, COMME POUR `ZoneAncree`. Un hote qui ne
				///    connait pas ce nom laisse le menu FERME, et le monteur le compte
				///    dans `menusContextuelsSansHote`. Repondre vrai sans position ferait
				///    surgir un menu au coin de l'ecran : un role qui « marche » a un
				///    endroit que personne n'a demande.
				virtual bool MenuContextuelOuvre(const char *nom, NkVec2 &posOut) noexcept {
					(void)nom;
					(void)posOut;
					return false;
				}

				/// Combien de points une courbe peut porter. Fixe : ce noyau est
				/// zero-STL, et une courbe de reglage en compte quelques-uns.
				static const uint32 kPointsCourbeMax = 64u;

				/// LA COURBE LIEE : l'hote rend ses points dans `points` (x dans [0,1],
				/// y dans les bornes que le document declare) et le compte dans `nOut`.
				/// Rend faux = « je ne connais pas ce `bind` ».
				///
				/// ⚠️ LES POINTS SONT A L'HOTE, ET CE N'EST PAS UN DETAIL D'ARCHITECTURE.
				///    Une courbe est une DONNEE : un fichier d'interface qui la porterait
				///    la figerait, et l'etat du monteur ne sait tenir qu'un `float32` par
				///    cle. Le format dit la FORME (hauteur, bornes, etiquettes), jamais le
				///    contenu — meme partage que pour une zone `Host`.
				virtual bool CourbeLiee(const char *bind, NkVec2 *points, uint32 maxPoints,
										uint32 &nOut) noexcept {
					(void)bind;
					(void)points;
					(void)maxPoints;
					(void)nOut;
					return false;
				}

				/// L'utilisateur a deplace un point : l'hote recoit la courbe ENTIERE.
				///
				/// ⚠️ LA COURBE ENTIERE, PAS « LE POINT i A CHANGE ». Un indice n'est pas
				///    un nom : si l'hote reordonne ses points entre deux images, un indice
				///    envoye designe un autre point que celui qu'on a tire. Ce depot a
				///    paye cette lecon deux fois le meme jour sur des index sans noms.
				virtual void CourbeModifiee(const char *bind, const NkVec2 *points,
											uint32 n) noexcept {
					(void)bind;
					(void)points;
					(void)n;
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
					// ── P10 : LES DISPOSITIONS, LUES ICI ET NULLE PART AILLEURS ──
					// Une seule lecture par document, avant le montage : `DockSpace` les
					// consulte ensuite sans relire l'archive. Les lire dans le `case`
					// aurait reparse la section `layout` a chaque image.
					etat.dispositions.Clear();
					etat.rapportDispositions = NkGuiRapportDispositions();
					(void)NkGuiLireDispositions(doc, etat.dispositions, etat.rapportDispositions);
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
						// ⚠️ LA RACINE PORTE LA POLITIQUE QUAND L'HOTE A OUVERT LA BANDE.
						//    Le cas est reel et c'est celui de NKUIDesign : la barre
						//    appartient a la COQUILLE (`BuildMenuBar`), le document n'a
						//    donc aucun noeud `MenuBar` ou accrocher `maxMenus`. Sans
						//    cette lecture, un document monte dans une bande d'hote ne
						//    pourrait DECLARER son plafond -- il faudrait une ligne de
						//    C++ par application, exactement ce que Rodolf refuse
						//    (28/09 : « pense toujours aux utilisateurs autres que nous »).
						//    Hors bande ouverte, les deux cles sont ignorees : elles ne
						//    veulent rien dire ailleurs.
						if (ctx.menuBarOuverte) {
							const float32 reserve = NkGNombre(sec, "reserveDroite", 0.f);
							const float32 maxM = NkGNombre(sec, "maxMenus", -1.f);
							if (reserve > 0.f || maxM >= 0.f)
								NkGuiMenuBarPolitique(ctx,
													  reserve > 0.f ? (ctx.menuBarRect.x
																	   + ctx.menuBarRect.w - reserve)
																	: ctx.menuBarLimite,
													  maxM >= 0.f ? (int32)maxM : -1);
						}
						// La racine de `widgets` est ABSOLUE par nature : rien ne la contient,
						// donc aucun conteneur ne peut y declarer son mode -- et le corpus
						// l'atteste (`03` pose son `Window` a la racine depuis le premier jour).
						MonterCorps(ctx, sec, etat, rap, 0u, false, /*parentAbsolu=*/true, hooks);
					}
				}

				// =====================================================================
				//  MONTER UNE RACINE NOMMEE — un document, plusieurs regions
				// =====================================================================
				/// Monte UNIQUEMENT la partie du document que `racine` nomme, au curseur
				/// courant. Rend vrai si la racine a ete trouvee.
				///
				/// 🔴 LE MANQUE QUE TROIS APPLICATIONS PAYAIENT DEJA. `Monter` monte
				///    TOUTES les sections `widgets` d'un document, dans l'ordre du
				///    fichier, au curseur courant — et `MonterCorps` est privee. Or une
				///    coquille d'editeur pose la barre de menu, la barre d'outils, la
				///    barre d'etat et ses panneaux a des INSTANTS et dans des REGIONS
				///    differents : un seul document ne pouvait donc pas les servir tous.
				///
				///    Le prix se comptait en fichiers : NkAntenne a **25 documents la ou
				///    5 auraient suffi**, NkAnimaEditor en a quatre, NKUIDesign deux.
				///    L'agent de NkAntenne l'a nomme dans son rapport comme « la raison
				///    premiere » de son eparpillement, et l'exemplaire de NkAnimaEditor
				///    portait deja la phrase : « **il manque
				///    `Monter(ctx, doc, "racine")`** ». La voici.
				///
				/// ⚠️ DEUX FACONS DE NOMMER UNE RACINE, ET L'ORDRE EST ECRIT. On cherche
				///    d'abord une SECTION `widgets "nom" { ... }`, puis, a defaut, un
				///    WIDGET de premier niveau portant cet identifiant. Les deux sont de
				///    vraies racines : la premiere regroupe plusieurs widgets, la seconde
				///    en designe un (avec sa descendance). Chercher le widget d'abord
				///    aurait rendu impossible d'avoir une section et un widget du meme
				///    nom — et c'est la section qui est le cas general.
				///
				/// ⚠️ ET UNE RACINE INTROUVABLE NE SE TAIT PAS. Rendre faux ne suffit
				///    pas : un appelant qui ignore le retour monterait une region VIDE,
				///    sans erreur, et chercherait a la main pourquoi sa barre a disparu.
				///    `racinesIntrouvables` est compte ET le nom demande est garde, parce
				///    qu'un compteur sans le nom dit qu'il manque quelque chose sans dire
				///    quoi. C'est la faute de frappe la plus probable de tout ce lot.
				///
				/// ⚠️ ELLE NE MONTE PAS LES CALQUES `geometry`. `Monter` les peint en
				///    passe separee, AVANT tout, pour qu'un calque ne passe jamais devant
				///    un widget ; les repeindre a chaque racine nommee les empilerait
				///    autant de fois qu'il y a de bandes. Un document qui a des calques
				///    ET des racines nommees passe donc par `Monter` pour ses calques.
				static bool Monter(NkGuiContext &ctx, const NkArchive &doc, const char *racine,
								   NkGuiMonteEtat &etat, NkGuiMonteRapport &rap,
								   NkGuiMonteHooks *hooks = nullptr) noexcept {
					const NkArchiveNode *corps = NkGMonteCorps(doc);
					if (!corps || !racine) {
						++rap.racinesIntrouvables;
						if (racine)
							rap.derniereRacineIntrouvable = NkString(racine);
						return false;
					}
					// ── 1. une SECTION `widgets "nom"` ──────────────────────────
					for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
						if (!corps->array[i].IsObject() || !corps->array[i].object)
							continue;
						const NkArchive &sec = *corps->array[i].object;
						if (!NkGMotEgal(NkGuiArchive::TypeOf(sec), "widgets"))
							continue;
						const NkString id(NkGuiArchive::IdOf(sec));
						if (id.Size() == 0u || !NkGMotEgal(NkStringView(id.CStr()), racine))
							continue;
						++rap.sections;
						rap.nomsSections.PushBack(NkString("widgets"));
						++rap.racinesMontees;
						// Meme raison qu'en haut : la racine d'un `widgets` est ABSOLUE,
						// rien ne la contient.
						MonterCorps(ctx, sec, etat, rap, 0u, false, /*parentAbsolu=*/true, hooks);
						return true;
					}
					// ── 2. a defaut, un WIDGET de premier niveau ────────────────
					for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
						if (!corps->array[i].IsObject() || !corps->array[i].object)
							continue;
						const NkArchive &sec = *corps->array[i].object;
						if (!NkGMotEgal(NkGuiArchive::TypeOf(sec), "widgets"))
							continue;
						const NkArchiveNode *cw = NkGMonteCorps(sec);
						if (!cw)
							continue;
						for (uint32 k = 0; k < (uint32)cw->array.Size(); ++k) {
							if (!cw->array[k].IsObject() || !cw->array[k].object)
								continue;
							const NkArchive &w = *cw->array[k].object;
							const NkString id(NkGuiArchive::IdOf(w));
							if (id.Size() == 0u || !NkGMotEgal(NkStringView(id.CStr()), racine))
								continue;
							++rap.racinesMontees;
							MonterBloc(ctx, w, etat, rap, 0u, false, /*parentAbsolu=*/true, hooks);
							return true;
						}
					}
					++rap.racinesIntrouvables;
					rap.derniereRacineIntrouvable = NkString(racine);
					return false;
				}

				// ═══════════════════════════════════════════════════════════════
				//  MONTER UN BLOC ISOLE, hors de toute section (2026-09-28)
				// ═══════════════════════════════════════════════════════════════
				//  C'est la porte dont `NkGuiMonterPatron` a besoin : instancier un
				//  patron a l'execution, c'est monter UN bloc qui ne vient d'aucune
				//  section `widgets` du document.
				//
				//  ⚠️ ELLE N'OUVRE RIEN DE NEUF : `MonterCorps` est exactement ce
				//     que `Monter` appelle pour chaque section. La rendre
				//     accessible ne change aucun chemin existant -- c'est une porte
				//     de plus sur la meme piece, pas une seconde piece.
				//
				//  ⚠️ `parentAbsolu = false` PAR DEFAUT, et c'est l'inverse du choix
				//     des sections. Une section `widgets` est absolue « par nature :
				//     rien ne la contient ». Un patron, lui, est monte DANS quelque
				//     chose -- une zone hote, un menu deja ouvert : il suit le flux
				//     de son hote, sinon il se poserait a l'origine de l'ecran.
				static void MonterBloc(NkGuiContext &ctx, const NkArchive &bloc,
									   NkGuiMonteEtat &etat, NkGuiMonteRapport &rap,
									   NkGuiMonteHooks *hooks = nullptr, uint32 prof = 0u,
									   bool horizontal = false,
									   bool parentAbsolu = false) noexcept {
					MonterCorps(ctx, bloc, etat, rap, prof, horizontal, parentAbsolu, hooks);
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
						Noter(ctx, rap, id, NkStringView("shape"), r, 0u, false, false);
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

					// ═════════════════════════════════════════════════════════
					//  `visible` — ET PERSONNE NE LE LISAIT (27/09)
					// ═════════════════════════════════════════════════════════
					//  🔴 `visible` EST AU VOCABULAIRE UNIVERSEL DU FORMAT DEPUIS
					//     TOUJOURS. Mesure du 27/09 : aucune lecture, nulle part.
					//     Un document qui ecrivait `visible = false` montait son
					//     widget exactement comme les autres. Cinquieme membre de la
					//     meme famille que `tooltip` et la couleur illisible : *le
					//     document declare, personne ne lit, et rien ne le dit.*
					//
					//  ⚠️ DEUX SOURCES, ET L'ORDRE EST CELUI DE `enabled` : le
					//     DOCUMENT pose l'etat de depart, un COMPORTEMENT le change
					//     ensuite (`show` / `hide` / `toggle`). Le second l'emporte,
					//     sinon l'instruction n'aurait aucun effet.
					//
					//  ⚠️ ET UN WIDGET CACHE NE SE MONTE PAS DU TOUT. Le dessiner
					//     transparent le laisserait cliquable ; ne pas l'appeler est
					//     la seule facon de le rendre absent. Il est COMPTE — un
					//     document dont la moitie disparait doit pouvoir se lire dans
					//     le releve.
					// ── P27 : LA CIBLE, AVANT TOUT LE RESTE ───────────────────────
					// Rodolf, 27/09 : « des design specifiques par plateforme […] web,
					// mobile et PC ».
					//
					// ⚠️ ELLE SE LIT AVANT `visible`, ET L'ORDRE EST UN CHOIX. Un widget
					//    ecarte par la plateforme n'est pas « invisible » : il n'existe
					//    pas sur cette cible. Les confondre aurait melange dans un meme
					//    compteur « le document cache ceci » et « ceci n'est pas pour
					//    cette plateforme » — deux causes, deux remedes.
					//
					// ⚠️ ET UN ATTRIBUT SUFFIT PARCE QUE LE MONTEUR NE DESCEND PAS DANS CE
					//    QU'IL NE MONTE PAS. `platform = Mobile` sur une `VBox` ecarte la
					//    boite ET sa descendance, sans une ligne de plus.
					if (NkGA(w, "platform")) {
						NkVector<NkString> cibles;
						uint32 nC = NkGListeChaines(w, "platform", cibles);
						// 🔴 UN LECTEUR DE LISTE N'EST PAS UN LECTEUR DE VALEUR, et je
						//    l'avais suppose. Mesure du 27/09 : `platform = [Mobile, Web]`
						//    etait lu, `platform = Mobile` rendait ZERO entree — donc
						//    aucune cible lue, donc aucun ecart et aucune faute comptee.
						//    Le cas SIMPLE, celui que tout document ecrira, ne faisait
						//    rien du tout, et la seule chose qui marchait etait la forme
						//    rare. Trois criteres l'ont dit d'un coup ; un seul, pose sur
						//    la liste, aurait ete vert.
						if (nC == 0u) {
							const NkString un = NkGTexte(w, "platform", "");
							if (un.Size() > 0u) {
								cibles.PushBack(un);
								nC = 1u;
							}
						}
						const NkGuiCible courante = NkGuiCibleCourante();
						bool correspond = false;
						bool auMoinsUneLue = false;
						for (uint32 i = 0; i < nC; ++i) {
							NkGuiCible c = NkGuiCible::Toutes;
							if (!NkGuiCibleDepuisNom(cibles[i].CStr(), (uint32)cibles[i].Size(),
													 c)) {
								// ⚠️ UN NOM INCONNU N'ECARTE PAS, IL SE COMPTE. `platform =
								//    Mobil` ne doit pas faire disparaitre un panneau en
								//    silence : ce serait un document juste qui perd une
								//    partie de lui-meme sans qu'aucun chiffre ne le dise.
								//    C'est la politique INVERSE de celle d'un ROLE inconnu,
								//    qui fait refuser le document entier — et la difference
								//    se justifie : un role inconnu rend le document
								//    inmontable, une cible inconnue laisse un document
								//    parfaitement montable dont une intention est perdue.
								++rap.ciblesInconnues;
								continue;
							}
							auMoinsUneLue = true;
							if (c == NkGuiCible::Toutes || c == courante)
								correspond = true;
						}
						if (auMoinsUneLue && !correspond) {
							++rap.ecartesParPlateforme;
							return;
						}
					}
					{
						const bool visibleDoc = NkGBooleen(w, "visible", true);
						const bool visible =
							(e && e->visibiliteDite) ? e->visible : visibleDoc;
						// ⚠️ LA VISIBILITE EFFECTIVE EST ECRITE DANS L'ETAT, et ce
						//    n'est pas une commodite : c'est ce que `x.visible` lit
						//    dans un comportement. Sans cette ligne, un widget cache
						//    PAR LE DOCUMENT se disait visible — son entree gardait
						//    son defaut, faute d'avoir jamais ete montee. Trouve par
						//    le temoin, qui lisait `true` sur un widget absent de
						//    l'image.
						if (e)
							e->visible = visible;
						if (!visible) {
							++rap.invisibles;
							return;
						}
					}

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
					// P11 : vrai des que le motif a ete pose sur un rectangle connu,
					// pour que le bloc generique d'apres le `switch` ne le repose pas
					// sur `prevItem` -- deux motifs superposes doubleraient le compte
					// et epaissiraient le trait sans que personne l'ait demande.
					bool hachuresPeintes = false;
					if (app.aFond && !estConteneurFond && !hauteurLibre && !pl.pose) {
						const NkRect rFond = ctx.NextItemRect(-1.f, ctx.ItemHeight());
						ctx.DL().AddRectFilled(rFond, app.fond,
											   app.rayon >= 0.f ? app.rayon : ctx.theme.rounding);
						ctx.SetNextItemRect(rFond);
						++rap.apparencesPeintes;
						fondPeintIci = true;
						// P11 : le motif suit le fond, sur le MEME rectangle.
						if (app.aHachures) {
							NkGuiHachurer(ctx.DL(), rFond, ctx.theme.textMuted);
							++rap.apparencesPeintes;
							hachuresPeintes = true;
						}
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
						// P11 : le motif suit le fond, sur le MEME rectangle.
						if (app.aHachures) {
							NkGuiHachurer(ctx.DL(), pl.rect, ctx.theme.textMuted);
							++rap.apparencesPeintes;
							hachuresPeintes = true;
						}
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
							// ── `size` SEUL, DANS UN FLUX ────────────────────────────────
							// ⚠️ SEPTIEME ABANDON SILENCIEUX, ET CELUI-CI SE VOYAIT EN PIXELS.
							//    `NkGuiLirePlacement` ne rend `pose` QUE s'il a lu un `pos` ; en
							//    flux il n'y en a pas, donc `RegionCourante` etait appelee sur un
							//    document sans `pos` -- et sa branche `pos`+`size` ne pouvait
							//    JAMAIS s'executer. Autrement dit `size` sur un conteneur en flux
							//    etait lu par PERSONNE, et deux `Panel { size = ... }` dans une
							//    `VBox` prenaient tous les deux TOUTE la region : le second
							//    repeignait le premier, exactement. C'est le defaut que Rodolf a
							//    demande de corriger le 27/09.
							//
							// ⚠️ ET ON PASSE PAR `NextItemRect`, PAS PAR UNE ARITHMETIQUE A NOUS.
							//    C'est elle qui sait avancer le curseur en X dans une HBox et en Y
							//    ailleurs ; ecrire le calcul ici aurait fait deux verites sur « ou
							//    commence le frere suivant », et elles auraient fini par diverger.
							NkRect rTaille;
							if (!pl.pose && !decoupeRel && !enfantsAbsolus
								&& TailleEnFlux(ctx, w, rTaille)) {
								r = rTaille;
								decoupeRel = true; // le contenu vit DANS ce rectangle
								++rap.conteneursTailleFlux;
							} else if (!pl.pose && !decoupeRel && !enfantsAbsolus) {
								// ⚠️ ON NE CHANGE PAS SA GEOMETRIE, ON LA COMPTE. Un conteneur qui
								//    ne dit ni `size` ni `sizeRel` veut dire « tout ce qui reste » :
								//    c'est un sens legitime pour le DERNIER, et une ambiguite des
								//    qu'il a un frere. Le faire partir du curseur aurait rogne le
								//    `Window` racine de TOUS les documents du corpus (le curseur y
								//    est deja decale de la marge du theme), c'est-a-dire casser dix
								//    fichiers valides pour en reparer un mal ecrit. On NOMME donc le
								//    cas au lieu de deviner : `conteneursSansTaille` le rend visible
								//    dans le rapport, et le document se corrige avec un `size`.
								++rap.conteneursSansTaille;
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
							// P11 : SUR LE RECTANGLE DU CONTENEUR, et ici parce que ce
							// `case` REND LA MAIN — le bloc generique d'apres le
							// `switch` ne le verrait jamais. C'est ce qu'une premiere
							// version a mesure : 0 pixel de difference sur un `Panel`
							// hachure, alors que c'est LE cas que la specification
							// demande (« une proposition non appliquee »).
							if (app.aHachures) {
								NkGuiHachurer(ctx.DL(), r, ctx.theme.textMuted);
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
							Noter(ctx, rap, id, t, r, prof, true, horizontal, &ctx.layout.region);
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
							Noter(ctx, rap, id, t, r, prof, true, horizontal, &ctx.layout.region);
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
								Noter(ctx, rap, id, t, r, prof, true, horizontal, &ctx.layout.region);
								++rap.montes;
								return;
							}
							// `size` en flux, par la meme porte que les boites.
							NkRect rGT;
							if (MonterDansTaille(ctx, w, pl, rap, rGT, [&]() {
									BeginGroup(ctx);
									MonterCorps(ctx, w, etat, rap, prof + 1u, horizontal, false, hooks);
									EndGroup(ctx);
								})) {
								Noter(ctx, rap, id, t, rGT, prof, true, horizontal, &ctx.layout.region);
								++rap.montes;
								return;
							}
							const NkVec2 c0 = ctx.layout.cursor;
							BeginGroup(ctx);
							MonterCorps(ctx, w, etat, rap, prof + 1u, horizontal, false, hooks);
							EndGroup(ctx);
							Noter(ctx, rap, id, t, BlocConsomme(ctx, c0), prof, true, horizontal, &ctx.layout.region);
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
								Noter(ctx, rap, id, t, pl.rect, prof, true, horizontal, &ctx.layout.region);
								++rap.montes;
								return;
							}
							// ── `size` EN FLUX : la boite se CONFINE ────────────────────
							// Sans cela, une `VBox { size = (180, 50) }` prenait tout le
							// reste de la region et son frere partait du curseur qu'elle
							// avait laisse — c'est-a-dire ailleurs que sous elle des que le
							// contenu etait plus court que la taille demandee.
							NkRect rVB;
							if (MonterDansTaille(ctx, w, pl, rap, rVB, [&]() {
									BeginVBox(ctx, gap);
									MonterCorps(ctx, w, etat, rap, prof + 1u, false, false, hooks);
									EndVBox(ctx);
								})) {
								Noter(ctx, rap, id, t, rVB, prof, true, horizontal, &ctx.layout.region);
								++rap.montes;
								return;
							}
							const NkVec2 c0 = ctx.layout.cursor;
							BeginVBox(ctx, gap);
							MonterCorps(ctx, w, etat, rap, prof + 1u, false, false, hooks);
							EndVBox(ctx);
							Noter(ctx, rap, id, t, BlocConsomme(ctx, c0), prof, true, horizontal, &ctx.layout.region);
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
								Noter(ctx, rap, id, t, pl.rect, prof, true, horizontal, &ctx.layout.region);
								++rap.montes;
								return;
							}
							// Meme regle que la `VBox`, et par la MEME fonction : une
							// seconde ecriture du calcul aurait pu diverger sur l'axe.
							NkRect rHB;
							if (MonterDansTaille(ctx, w, pl, rap, rHB, [&]() {
									BeginHBox(ctx, gap);
									MonterCorps(ctx, w, etat, rap, prof + 1u, true, false, hooks);
									EndHBox(ctx);
								})) {
								Noter(ctx, rap, id, t, rHB, prof, true, horizontal, &ctx.layout.region);
								++rap.montes;
								return;
							}
							const NkVec2 c0 = ctx.layout.cursor;
							BeginHBox(ctx, gap);
							MonterCorps(ctx, w, etat, rap, prof + 1u, true, false, hooks);
							EndHBox(ctx);
							Noter(ctx, rap, id, t, BlocConsomme(ctx, c0), prof, true, horizontal, &ctx.layout.region);
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
							const NkString s = NkGLibelle(w, id);
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
							const NkString s = NkGLibelle(w, id);
							const NkRect r = ctx.NextItemRect(0.f, ctx.ItemHeight());
							(void)RepeatButton(ctx, s.CStr(), r, NkGNombre(w, "repeatDelay", -1.f),
											   NkGNombre(w, "repeatRate", -1.f));
							break;
						}
						case NkGuiRole::Checkbox: {
							const NkString s = NkGLibelle(w, id);
							if (e) {
								// 🔴 ELLE NE LISAIT PAS SON `value` (corrige le 27/09).
								//    Tous les autres roles editables posent leur valeur
								//    initiale depuis le document — `Slider`, `TextField`,
								//    `Switch`, `RadioGroup`, `Expander`, `Splitter`… —
								//    et celui-ci partait TOUJOURS decoche. Un document
								//    qui ecrit `value = true` obtenait une case vide, et
								//    rien ne le disait. Trouve par le temoin de `.checked`
								//    du blueprint, qui lisait 0 sur une case declaree
								//    cochee.
								if (!e->initialise) {
									e->b = NkGBooleen(w, "value", false);
									e->initialise = true;
								}
								(void)Checkbox(ctx, s.CStr(), e->b);
								valeurMontee = e->b ? 1.f : 0.f;
								aValeurMontee = true;
							} else {
								aDessine = false;
							}
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
							// L'identifiant, pas le libelle : voir `TreeNodeEx`.
							const NkGuiId idS = ctx.GetId(id.CStr());
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
								// 🔴 L'IDENTIFIANT ET LE RANG, JAMAIS LE TEXTE DE L'OPTION.
								//    Le commentaire du magasin, dix lignes plus haut,
								//    disait deja pourquoi pour la VALEUR : « l'indice est
								//    exact et stable quand les libelles changent de casse
								//    ou de langue ». L'IDENTITE d'interaction avait le
								//    meme besoin et ne l'avait pas : basculer la langue
								//    deplacait le survol d'une option a l'autre.
								NkString idOpt(id);
								idOpt.Append("#");
								{
									char num[12];
									uint32 v = k, nd = 0;
									do { num[nd++] = (char)('0' + (v % 10u)); v /= 10u; } while (v);
									for (uint32 q = 0; q < nd; ++q)
										idOpt.Append(NkStringView(&num[nd - 1u - q], 1u));
								}
								const NkGuiId idR = ctx.GetId(idOpt.CStr());
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
							Noter(ctx, rap, id, t, BlocConsomme(ctx, cRG), prof, true, horizontal,
								  &ctx.layout.region);
							++rap.montes;
							valeurMontee = e->f;
							aValeurMontee = true;
							return;
						}
						case NkGuiRole::ImageButton: {
							// L'IMAGE EST DESSINEE (27/09) -- voir `NkGuiImages.h`. Le
							// bouton garde son comportement et son libellé ; l'image se
							// pose PAR-DESSUS son rectangle.
							//
							// ⚠️ CE QUI NE SE RESOUT PAS RESTE COMPTE, exactement comme
							//    avant. `image` nomme un fichier : il peut manquer, ne
							//    pas se decoder, ou arriver avant que l'hote ait pose son
							//    televerseur. Le registre compte chacun de ces cas
							//    separement -- un compteur unique aurait melange « le
							//    fichier n'existe pas » avec « l'hote n'est pas pret »,
							//    qui ne se corrigent pas du tout de la meme facon.
							// ⚠️ ET L'IMAGE POSEE PAR UN COMPORTEMENT PASSE PAR LA MEME
							//    PORTE : `set x.image = "..."` prime sur le document,
							//    comme `set x.icon`.
							const NkString sIB = NkGLibelle(w, id);
							(void)Button(ctx, sIB.CStr());
							{
								NkString src = NkGTexte(w, "image", "");
								if (src.Empty())
									src = NkGTexte(w, "source", "");
								if (e && e->imageDite)
									src = NkString(e->image);
								if (!src.Empty())
									PeindreImage(ctx, src, ctx.layout.prevItem, rap);
							}
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
								} else if (TailleEnFlux(ctx, w, rS)) {
									// ⚠️ ET `size` SEUL COMPTE AUSSI, DEPUIS LE 27/09.
									//    Le refus ci-dessus disait « il faudrait inventer
									//    la taille » : `size` la DIT, il n'etait
									//    simplement lu par personne en flux. Sans cette
									//    branche, `BeginStack` restait refuse sur le cas
									//    le plus naturel — un empilement de taille fixe.
									aRectS = true;
									++rap.conteneursTailleFlux;
								}
								if (NkGA(w, "anchor") && !aRectS)
									++rap.attributsNonHonores;
								if (aRectS) {
									const NkGuiLayout sauveS = ctx.layout;
									ctx.BeginLayout(rS);
									// ⚠️ CHAQUE ENFANT REPART DU MEME COIN : c'est ce
									//    qui fait une PILE et non une colonne.
									//
									// 🔴 ET CE COIN EST CELUI QUE `BeginLayout` A CHOISI,
									//    PAS `{rS.x, rS.y}`. Mesure du 27/09, temoin
									//    (b14) : le `Stack` placait son enfant a (0, 0)
									//    de son rectangle quand `VBox`, `HBox` et `Group`
									//    le placaient a (10, 10) — la marge du theme, que
									//    `BeginLayout` venait de poser et que la ligne
									//    d'origine jetait en reecrivant le curseur avec le
									//    coin BRUT. L'intention etait juste, le coin non :
									//    un document ecrit pour une `VBox` ne se relisait
									//    pas dans un `Stack`.
									//
									//    ⚠️ ET LE PREMIER CRITERE NE POUVAIT PAS LE VOIR :
									//       il exigeait un pas « entre 40 et 56 px », et
									//       40 comme 50 y tenaient. Un intervalle assez
									//       large pour accepter les deux ne refute rien.
									const NkVec2 origineS = ctx.layout.cursor;
									const NkArchiveNode *csS = NkGMonteCorps(w);
									if (csS) {
										for (uint32 k = 0; k < (uint32)csS->array.Size(); ++k) {
											if (!csS->array[k].IsObject() || !csS->array[k].object)
												continue;
											ctx.layout.cursor = origineS;
											MonterBloc(ctx, *csS->array[k].object, etat, rap,
													   prof + 1u, false, true, hooks);
										}
									}
									ctx.layout = sauveS;
									Noter(ctx, rap, id, t, rS, prof, true, horizontal,
										  &ctx.layout.region);
								} else {
									const NkVec2 c0S = ctx.layout.cursor;
									MonterCorps(ctx, w, etat, rap, prof + 1u, false, false, hooks);
									Noter(ctx, rap, id, t, BlocConsomme(ctx, c0S), prof, true,
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
							// ⚠️ `size` N'EST PAS HONORE ICI, ET C'EST DIT. `BeginTable`
							//    tient sa propre mise en page de colonnes et ne prend pas
							//    de rectangle ; lui en imposer un aurait demande de
							//    reecrire la brique. On COMPTE la demande au lieu de
							//    l'ignorer en silence — c'est precisement le defaut que
							//    `Window`/`Panel` portaient jusqu'a ce matin.
							{
								float32 sT0 = 0.f, sT1 = 0.f;
								if (!pl.pose && LireVec2(w, "size", sT0, sT1)
									&& (sT0 > 0.f || sT1 > 0.f))
									++rap.taillesNonHonorees;
							}
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
							Noter(ctx, rap, id, t, BlocConsomme(ctx, cT), prof, true, horizontal,
								  &ctx.layout.region);
							++rap.montes;
							return;
						}
						// ═════════════════════════════════════════════════════
						//  LES HUIT ROLES PROPOSES (27/09)
						// ═════════════════════════════════════════════════════
						case NkGuiRole::ToggleButton: {
							// P2. « Un `Checkbox` n'a pas la forme d'un bouton
							// d'outil ; un `Button` n'a pas d'etat. »
							//
							// ⚠️ `group` EST LA RAISON D'ETRE DU ROLE : les boutons
							//    d'un meme groupe sont EXCLUSIFS. L'exclusivite vit
							//    dans le magasin, a la cle du groupe : enfoncer l'un
							//    y ecrit SON identifiant, et chacun se dessine actif
							//    s'il s'y reconnait. Deux gardes-fous en un — pas
							//    deux actifs, et l'etat survit au remontage.
							if (!e) {
								aDessine = false;
								break;
							}
							const NkString grp = NkGTexte(w, "group", "");
							const NkString sTB = NkGLibelle(w, id);
							NkGuiMonteEtat::Entree *eg =
								grp.Size() > 0u ? etat.Get(NkStringView(grp.CStr())) : nullptr;
							if (!e->initialise) {
								e->b = NkGBooleen(w, "value", false);
								e->initialise = true;
								if (e->b && eg)
									Copier(eg->texte, (int32)sizeof(eg->texte), id);
							}
							const bool actif =
								eg ? (NkGMotEgal(NkStringView(eg->texte), id.CStr())) : e->b;
							// L'etat ACTIF est peint avec l'accent, « sauf
							// surcharge » : une `appearance(Active)` du document
							// passe devant, et c'est la couche d'execution qui
							// l'applique. Ici, le defaut.
							const NkColor sauveTB = ctx.theme.button;
							if (actif && !app.aFond)
								ctx.theme.button = ctx.theme.accent;
							if (Button(ctx, sTB.CStr())) {
								if (eg) {
									// ⚠️ UN GROUPE N'A PAS D'ETAT « AUCUN » ICI. Un
									//    outil de vue est toujours l'un des six :
									//    re-cliquer l'actif ne le deselectionne pas.
									Copier(eg->texte, (int32)sizeof(eg->texte), id);
									eg->initialise = true;
								} else {
									e->b = !e->b;
								}
							}
							ctx.theme.button = sauveTB;
							if (eg)
								e->b = NkGMotEgal(NkStringView(eg->texte), id.CStr());
							valeurMontee = e->b ? 1.f : 0.f;
							aValeurMontee = true;
							break;
						}
						case NkGuiRole::Badge: {
							// P3. « Non interactif, petite pilule. Un `Text` n'a pas
							// de fond, et `fill` n'est pas peint sur `Text`. »
							//
							// ⚠️ `tone` EST UN NOM DE JETON SANS SON `@` — c'est la
							//    graphie que la specification pose. On le prefixe
							//    ici : le document ecrit `tone: origine.main`, et le
							//    theme resout `@origine.main`. Une pilule dont le
							//    ton est inconnu prend le bord du theme et le
							//    COMPTE, elle ne devient pas invisible.
							const NkString txtB = NkGTexte(w, "text", "");
							NkColor fondB = ctx.theme.border;
							const NkString ton = NkGTexte(w, "tone", "");
							if (ton.Size() > 0u) {
								NkString avecArobase("@");
								avecArobase.Append(ton.CStr());
								if (!NkGuiCouleur(NkStringView(avecArobase.CStr()), fondB))
									++rap.attributsNonHonores;
							}
							const float32 hB = ctx.ItemHeight();
							const float32 lB = ctx.font && ctx.font->Valid()
												   ? ctx.font->MeasureWidth(txtB.CStr(), nullptr)
														 + hB * 0.6f
												   : hB;
							const NkRect rB = ctx.NextItemRect(lB, hB * 0.8f);
							ctx.DL().AddRectFilled(rB, fondB, rB.h * 0.5f);
							if (txtB.Size() > 0u)
								(void)TextAt(ctx, {rB.x + rB.h * 0.3f, rB.y}, txtB.CStr());
							break;
						}
						case NkGuiRole::Tile: {
							// P4. Vignette + libelle + legende, cliquable.
							//
							// ⚠️ LA VIGNETTE EST COMPTEE, PAS INVENTEE — meme raison
							//    que `ImageButton` : `image` nomme une ressource que
							//    le document ne porte pas.
							// La vignette est PEINTE depuis le 27/09, par la meme porte
							// qu'`ImageButton` (`NkGuiImages.h`).
							const NkString sT = NkGLibelle(w, id);
							const NkString cap = NkGTexte(w, "caption", "");
							// La taille par defaut est celle de la specification (96x96) ;
							// `size = (w, h)` se lit par la MEME porte que partout ailleurs.
							float32 tw = 96.f, th = 96.f;
							{
								const NkArchiveNode *nsT = w.FindNode(NkStringView("size"));
								float32 dim[4];
								if (nsT && NkGNombresDansLexeme(nsT->Lexeme(), dim, 4u) >= 2u
									&& dim[0] > 0.f && dim[1] > 0.f) {
									tw = dim[0];
									th = dim[1];
								}
							}
							const NkRect rT = ctx.NextItemRect(tw, th);
							// L'identifiant, pas le libelle : voir `TreeNodeEx`.
							const NkGuiId idT = ctx.GetId(id.CStr());
							bool hovT = false, heldT = false;
							(void)ctx.ButtonBehavior(idT, rT, NkGuiButtonFlags::None, -1.f, -1.f,
													 &hovT, &heldT);
							ctx.DL().AddRectFilled(rT, hovT ? ctx.theme.buttonHover : ctx.theme.button,
												   ctx.theme.rounding);
							// LA VIGNETTE, sous le cadre et sous les libelles : c'est la
							// tuile qui encadre l'image, pas l'inverse.
							{
								NkString srcT = NkGTexte(w, "image", "");
								if (e && e->imageDite)
									srcT = NkString(e->image);
								if (!srcT.Empty())
									PeindreImage(ctx, srcT, rT, rap);
							}
							ctx.DL().AddRect(rT, ctx.theme.border, 1.f, ctx.theme.rounding);
							if (ctx.font && ctx.font->Valid()) {
								(void)TextAt(ctx, {rT.x + 6.f, rT.y + rT.h - ctx.ItemHeight() * 2.f},
											 sT.CStr());
								if (cap.Size() > 0u)
									(void)TextAt(ctx,
												 {rT.x + 6.f, rT.y + rT.h - ctx.ItemHeight()},
												 cap.CStr(), ctx.theme.textDisabled);
							}
							break;
						}
						case NkGuiRole::KeyDiamond: {
							// P6. Trois etats : vide (non anime), plein (cle a
							// l'image courante), demi (anime, pas de cle ici).
							//
							// ⚠️ L'ETAT VIENT DU MAGASIN, PAS DU DOCUMENT. Un
							//    document ne sait pas s'il y a une cle a l'image
							//    courante : c'est l'application qui l'ecrit, a la
							//    cle que `bind` designe. 0 = vide, 1 = plein,
							//    0.5 = demi.
							{
								const float32 etatK = e ? e->f : 0.f;
								const float32 hK = ctx.ItemHeight();
								const NkRect rK = ctx.NextItemRect(hK, hK);
								const NkGuiId idK = ctx.GetId(id.CStr());
								bool hovK = false, heldK = false;
								(void)ctx.ButtonBehavior(idK, rK, NkGuiButtonFlags::None, -1.f,
														 -1.f, &hovK, &heldK);
								const NkVec2 c{rK.x + rK.w * 0.5f, rK.y + rK.h * 0.5f};
								const float32 r0 = hK * 0.3f;
								const NkVec2 haut{c.x, c.y - r0}, bas{c.x, c.y + r0};
								const NkVec2 gauche{c.x - r0, c.y}, droite{c.x + r0, c.y};
								// Le survol le teinte de l'encre « il ecrit ».
								NkColor teinte = ctx.theme.text;
								if (hovK)
									(void)NkGuiCouleur(NkStringView("@info.ecrit"), teinte);
								if (etatK >= 0.75f) {
									ctx.DL().AddTriangleFilled(haut, droite, bas, teinte);
									ctx.DL().AddTriangleFilled(haut, gauche, bas, teinte);
								} else if (etatK >= 0.25f) {
									// Demi : le losange n'est que contour.
									ctx.DL().AddLine(haut, droite, teinte, 1.f);
									ctx.DL().AddLine(droite, bas, teinte, 1.f);
									ctx.DL().AddLine(bas, gauche, teinte, 1.f);
									ctx.DL().AddLine(gauche, haut, teinte, 1.f);
								} else {
									const NkColor faible = ctx.theme.textDisabled;
									ctx.DL().AddLine(haut, droite, faible, 1.f);
									ctx.DL().AddLine(droite, bas, faible, 1.f);
									ctx.DL().AddLine(bas, gauche, faible, 1.f);
									ctx.DL().AddLine(gauche, haut, faible, 1.f);
								}
							}
							break;
						}
						case NkGuiRole::SplitButton: {
							// P18. « Le bouton affiche l'outil courant, la fleche la
							// famille. » Deux zones cliquables dans un rectangle.
							//
							// ⚠️ LA LISTE NE S'OUVRE PAS ICI. Ouvrir un popup demande
							//    un `OpenPopupAt` que ce monteur n'a pas a offrir —
							//    meme raison que `ContextMenu`. La fleche se dessine,
							//    se survole, et son clic est COMPTE non servi : le
							//    jour ou un crochet d'ouverture existera, elle le
							//    tirera sans que le document change.
							{
								const NkString sSB = NkGLibelle(w, id);
								const uint32 nItems = NkGListeCompte(w, "items");
								if (nItems > 0u)
									++rap.attributsNonHonores;
								const float32 hSB = ctx.ItemHeight();
								const NkRect rSB = ctx.NextItemRect(-1.f, hSB);
								const float32 largFleche = hSB;
								const NkRect rPrinc{rSB.x, rSB.y, rSB.w - largFleche, rSB.h};
								const NkRect rFleche{rSB.x + rSB.w - largFleche, rSB.y, largFleche,
													 rSB.h};
								bool hovP = false, heldP = false, hovF = false, heldF = false;
								// ⚠️ DEUX MOITIES, DEUX IDENTITES DERIVEES DE L'IDENTIFIANT --
								//    et surtout PAS la meme. La partie principale prenait
								//    son identite du LIBELLE (instable a la bascule de
								//    langue) ; lui donner simplement `id` l'aurait fait
								//    COLLISIONNER avec la fleche, qui emploie deja `id` :
								//    survoler l'une aurait allume l'autre. La fleche prend
								//    donc un suffixe.
								(void)ctx.ButtonBehavior(ctx.GetId(id.CStr()), rPrinc,
														 NkGuiButtonFlags::None, -1.f, -1.f, &hovP,
														 &heldP);
								NkString idFleche(id);
								idFleche.Append("#fleche");
								(void)ctx.ButtonBehavior(ctx.GetId(idFleche.CStr()), rFleche,
														 NkGuiButtonFlags::None, -1.f, -1.f, &hovF,
														 &heldF);
								ctx.DL().AddRectFilled(rPrinc,
													   hovP ? ctx.theme.buttonHover
															: ctx.theme.button,
													   ctx.theme.rounding);
								ctx.DL().AddRectFilled(rFleche,
													   hovF ? ctx.theme.buttonHover
															: ctx.theme.button,
													   ctx.theme.rounding);
								ctx.DL().AddLine({rFleche.x, rFleche.y + 2.f},
												 {rFleche.x, rFleche.y + rFleche.h - 2.f},
												 ctx.theme.border, 1.f);
								const float32 a = hSB * 0.18f;
								const NkVec2 cf{rFleche.x + rFleche.w * 0.5f,
												rFleche.y + rFleche.h * 0.5f};
								ctx.DL().AddTriangleFilled({cf.x - a, cf.y - a * 0.5f},
														   {cf.x + a, cf.y - a * 0.5f},
														   {cf.x, cf.y + a * 0.7f},
														   ctx.theme.text);
								if (ctx.font && ctx.font->Valid())
									(void)TextAt(ctx, {rPrinc.x + 6.f, rPrinc.y}, sSB.CStr());
							}
							break;
						}
						case NkGuiRole::TimecodeField: {
							// P13. Un temps qui se LIT en heures:minutes:secondes:images.
							//
							// ⚠️ LE `fps` VIENT DU DOCUMENT ET N'EST PAS DEVINE. Sans
							//    lui, 0,5 s ne peut pas s'ecrire en images : 12 a 24
							//    i/s, 15 a 30. Le defaut est 24 — le cinema — et il
							//    est ECRIT ici plutot que suppose ailleurs.
							{
								const float32 fps = NkGNombre(w, "fps", 24.f);
								const float32 secondes = e ? e->f : NkGNombre(w, "value", 0.f);
								const int32 total = (int32)(secondes * fps + 0.5f);
								const int32 imgs = (int32)fps > 0 ? total % (int32)fps : 0;
								const int32 sec = (int32)fps > 0 ? total / (int32)fps : 0;
								// ⚠️ ECRIT A LA MAIN, SANS `snprintf`. Cette couche est
								//    zero-libc, et ce depot a deja paye un formatage
								//    qui depend de la CULTURE : une virgule decimale
								//    en fr-FR la ou un point etait attendu. Des
								//    chiffres poses un par un ne dependent de rien.
								char tc[32];
								int32 pos = 0;
								const int32 hh = sec / 3600, mm = (sec / 60) % 60, ss = sec % 60;
								PoserDeuxChiffres(tc, pos, hh);
								tc[pos++] = ':';
								PoserDeuxChiffres(tc, pos, mm);
								tc[pos++] = ':';
								PoserDeuxChiffres(tc, pos, ss);
								tc[pos++] = ':';
								PoserDeuxChiffres(tc, pos, imgs);
								tc[pos] = '\0';
								(void)Text(ctx, tc);
							}
							break;
						}
						case NkGuiRole::VectorField: {
							// P5. « Le champ X/Y/Z a lisere rouge/vert/bleu est LA
							// signature du panneau Details UE5. La couleur porte
							// l'axe : c'est de l'information. »
							//
							// ⚠️ CHAQUE COMPOSANTE A SA PROPRE CLE DE MAGASIN
							//    (`<bind>.x`, `.y`, `.z`, `.w`) : le magasin ne tient
							//    qu'un `float32` par cle, et trois valeurs dans une
							//    seule en perdraient deux.
							{
								uint32 nComp = (uint32)NkGNombre(w, "components", 3.f);
								if (nComp < 2u)
									nComp = 2u;
								if (nComp > 4u)
									nComp = 4u;
								const bool axes = NkGBooleen(w, "axisColors", nComp == 3u);
								const float32 vitesse = NkGNombre(w, "speed", 0.1f);
								const float32 vmin = NkGNombre(w, "min", 0.f);
								const float32 vmax = NkGNombre(w, "max", 0.f);
								const bool borne = (vmax > vmin);
								static const char *kSuffixe[4] = {".x", ".y", ".z", ".w"};
								static const char *kJeton[4] = {"@axe.x", "@axe.y", "@axe.z", ""};
								const NkString cle = NkGTexte(w, "bind", id.CStr());
								const NkVec2 cV = ctx.layout.cursor;
								BeginHBox(ctx, 2.f);
								for (uint32 k = 0; k < nComp; ++k) {
									NkString cleK = cle;
									cleK.Append(kSuffixe[k]);
									NkGuiMonteEtat::Entree *ek =
										etat.Get(NkStringView(cleK.CStr()));
									float32 v = ek ? ek->f : 0.f;
									const float32 hV = ctx.ItemHeight();
									const NkRect rV = ctx.NextItemRect(0.f, hV);
									ctx.SetNextItemRect(rV);
									(void)DragFloat(ctx, cleK.CStr(), v, vitesse,
													borne ? vmin : 0.f, borne ? vmax : 0.f);
									if (ek)
										ek->f = v;
									// Le lisere d'axe : 2 px a gauche du champ.
									if (axes && k < 3u) {
										NkColor ca{0, 0, 0, 0};
										if (NkGuiCouleur(NkStringView(kJeton[k]), ca))
											ctx.DL().AddRectFilled({rV.x, rV.y, 2.f, rV.h}, ca,
																   0.f);
										else
											++rap.attributsNonHonores;
									}
								}
								EndHBox(ctx);
								Noter(ctx, rap, id, t, BlocConsomme(ctx, cV), prof, true, horizontal,
									  &ctx.layout.region);
								++rap.montes;
								return;
							}
						}
						case NkGuiRole::TokenField: {
							// P23. « `ColorField` choisit une VALEUR ; un jeton est
							// un NOM qui se resout par theme. »
							//
							// ⚠️ IL MONTRE LA PASTILLE ET LE NOM, pas un nuancier :
							//    c'est ce qui le distingue d'un `ColorField`. La
							//    LISTE des jetons ne s'ouvre pas ici (meme raison que
							//    `SplitButton` : pas de popup au repos) ; `families`
							//    est donc compte.
							{
								if (NkGA(w, "families"))
									++rap.attributsNonHonores;
								const NkString nomJ =
									e && e->texte[0] ? NkString(e->texte)
													 : NkGTexte(w, "value", "@accent");
								NkColor cJ{0, 0, 0, 0};
								const bool resolu = NkGuiCouleur(NkStringView(nomJ.CStr()), cJ);
								const float32 hJ = ctx.ItemHeight();
								const NkRect rJ = ctx.NextItemRect(-1.f, hJ);
								const NkRect past{rJ.x + 2.f, rJ.y + 2.f, hJ - 4.f, hJ - 4.f};
								if (resolu)
									ctx.DL().AddRectFilled(past, cJ, 2.f);
								else
									ctx.DL().AddRect(past, ctx.theme.border, 1.f, 2.f);
								ctx.DL().AddRect(rJ, ctx.theme.border, 1.f, ctx.theme.rounding);
								if (ctx.font && ctx.font->Valid())
									(void)TextAt(ctx, {rJ.x + hJ + 4.f, rJ.y}, nomJ.CStr(),
												 resolu ? ctx.theme.text : ctx.theme.textDisabled);
								// ⚠️ UN JETON QUI NE SE RESOUT PAS SE COMPTE. Le champ
								//    reste lisible — son nom s'affiche en grise — mais
								//    le document ne doit pas croire qu'il a ete honore.
								if (!resolu)
									++rap.attributsNonHonores;
							}
							break;
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
							// ── P10 : LA DISPOSITION DU DOCUMENT, SI ELLE EXISTE ────────
							NkVector<NkRect> placees;
							const bool parAmarrage =
								PlacerParAmarrage(etat, cd, zone, placees, rap);
							if (cd && nZones > 0u && parAmarrage) {
								uint32 vuA = 0u;
								for (uint32 k = 0; k < (uint32)cd->array.Size(); ++k) {
									if (!cd->array[k].IsObject() || !cd->array[k].object)
										continue;
									const NkArchive &z = *cd->array[k].object;
									const NkString nom(NkGuiArchive::IdOf(z));
									const NkRect r = placees[vuA++];
									++rap.zones;
									const bool servie = hooks && hooks->ZoneAncree(nom.CStr(), r);
									if (!servie)
										++rap.zonesSansPanneau;
									// ⚠️ LA MARQUE D'UNE ZONE NON SERVIE PASSE PAR LE MEME
									//    CODE QUE L'AUTRE CHEMIN. L'ecrire une seconde fois
									//    ici aurait fait deux facons de signaler le meme
									//    manque, et elles auraient fini par differer.
									if (!servie && !kMuet)
										MarquerZoneVide(ctx, r, nom.CStr());
									Noter(ctx, rap, nom, NkGuiArchive::TypeOf(z), r, prof + 1u, true,
										  true);
								}
								Noter(ctx, rap, id, t, zone, prof, true, horizontal, &ctx.layout.region);
								++rap.montes;
								return;
							}
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
										if (!kMuet)
											MarquerZoneVide(ctx, r, nom.CStr());
									}
									Noter(ctx, rap, nom, NkGuiArchive::TypeOf(z), r, prof + 1u, true, true);
								}
							}
							Noter(ctx, rap, id, t, zone, prof, true, horizontal, &ctx.layout.region);
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
							Noter(ctx, rap, id, t, zone, prof, true, horizontal, &ctx.layout.region);
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
							const NkString titre = NkGLibelle(w, id);
							if (e && !e->initialise) {
								e->b = NkGBooleen(w, "expanded", false);
								e->initialise = true;
							}
							// 🔴 L'IDENTITE VIENT DE L'IDENTIFIANT, LE TEXTE DU LIBELLE.
							//    Les deux lignes DOIVENT employer la meme cle : ranger
							//    l'etat sous une cle et interroger le widget sous une
							//    autre donnerait un arbre qui se referme tout seul --
							//    pire que l'instabilite qu'on corrige.
							ctx.SetNodeOpen(ctx.GetId(id.CStr()),
											e ? e->b : NkGBooleen(w, "expanded", false));
							const bool ouvert = TreeNodeEx(ctx, id.CStr(), titre.CStr());
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
							Noter(ctx, rap, id, t, zone, prof, true, horizontal, &ctx.layout.region);
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
							const NkString titre = NkGLibelle(w, id);
							// LE DOCUMENT DONNE L'ETAT INITIAL, PAS UN ORDRE PERMANENT.
							if (e && !e->initialise) {
								e->b = NkGBooleen(w, "expanded", false);
								e->initialise = true;
							}
							const bool voulu = kToujours
												   ? true
												   : ((e && !kFige) ? e->b
																	: NkGBooleen(w, "expanded", false));
							// L'identifiant fait foi, pas le libelle -- meme raison qu'au
							// `TreeItem` plus haut, et les deux lignes emploient la meme
							// cle.
							ctx.SetNodeOpen(ctx.GetId(id.CStr()), voulu);
							// 🔴 ET SON RECTANGLE RELEVE ETAIT CELUI DE SON ENFANT. Avec un
							//    `break`, le `Noter` generique enregistrait `BlocConsomme` pris
							//    APRES l'en-tete : sur un accordeon d'un seul bouton, le releve
							//    donnait exactement le rectangle du bouton. Qui vise « le milieu
							//    de l'accordeon » cliquait donc DANS son contenu -- c'est ce qui
							//    a fait rougir deux criteres sur un correctif juste. Le bloc est
							//    donc pris AVANT l'en-tete : le releve couvre l'en-tete ET le
							//    contenu, et sa premiere rangee EST la barre cliquable.
							const NkVec2 cExp = ctx.layout.cursor;
							const bool ouvert = CollapsingHeaderEx(ctx, id.CStr(), titre.CStr());
							// LE GESTE, garde dans l'etat -- jamais reecrit dans le document.
							if (e && !kToujours && !kFige)
								e->b = ouvert;
							if (ouvert)
								MonterCorps(ctx, w, etat, rap, prof + 1u, false, false, hooks);
							Noter(ctx, rap, id, t, BlocConsomme(ctx, cExp), prof, true, horizontal,
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
							// ⚠️ L'ORIENTATION SE DECLARE, ET LE DEFAUT NE BOUGE PAS.
							//    Rodolf, 28/09 : « on a deja des onglets horizontaux,
							//    on doit aussi avoir des onglets verticaux ». Un
							//    document ecrit avant aujourd'hui n'a pas la cle et
							//    reste horizontal, au pixel pres.
							//    `orientation = "verticale"` (ou `"vertical"`) suffit.
							const NkString orient = NkGTexte(w, "orientation", "");
							const bool vertical = orient.Size() > 0u
												  && (orient.Data()[0] == 'v'
													  || orient.Data()[0] == 'V');
							(void)TabBarOriente(ctx, lbl, ptr, (int32)m, nullptr, vertical);
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
						// ── QUATRE CONTENEURS DE PLUS (27/09) ───────────────
						case NkGuiRole::Padding: {
							// `padding = 12` ou `padding = (gauche, haut, droite, bas)`.
							// Le plus simple des quatre, et le plus demande : il n'existe
							// aucune facon d'ecrire « rentre ceci » avec les roles existants.
							float32 m[4] = {0.f, 0.f, 0.f, 0.f};
							const NkArchiveNode *np = w.FindNode(NkStringView("padding"));
							if (np) {
								const uint32 c = NkGNombresDansLexeme(np->Lexeme(), m, 4u);
								if (c == 1u)
									m[1] = m[2] = m[3] = m[0]; // un seul nombre = les quatre cotes
								else if (c == 2u) {
									m[2] = m[0];
									m[3] = m[1]; // deux = horizontal, vertical
								} else if (c != 4u) {
									// ⚠️ NI UN, NI DEUX, NI QUATRE : on ne devine pas. Trois
									//    nombres pourraient vouloir dire n'importe quoi, et
									//    choisir a la place du document rendrait une marge que
									//    personne n'a ecrite.
									++rap.attributsNonHonores;
									m[0] = m[1] = m[2] = m[3] = 0.f;
								}
							}
							NkRect rP = pl.pose ? pl.rect : ctx.layout.region;
							if (!pl.pose) {
								rP.w -= (ctx.layout.cursor.x - rP.x);
								rP.h -= (ctx.layout.cursor.y - rP.y);
								rP.x = ctx.layout.cursor.x;
								rP.y = ctx.layout.cursor.y;
							}
							NkRect rTailleP;
							if (!pl.pose && TailleEnFlux(ctx, w, rTailleP))
								rP = rTailleP;
							NkRect dedans{rP.x + m[0], rP.y + m[1], rP.w - m[0] - m[2],
										  rP.h - m[1] - m[3]};
							if (dedans.w < 0.f)
								dedans.w = 0.f;
							if (dedans.h < 0.f)
								dedans.h = 0.f;
							{
								const NkGuiLayout sauveP = ctx.layout;
								ctx.BeginLayout(dedans);
								// 🔴 LE CURSEUR SE FORCE, ET LA MESURE L'A EXIGE. `BeginLayout`
								//    pose `cursor = region + layout.padding` : ma marge
								//    s'AJOUTAIT a celle du theme, et `padding = 20` decalait de
								//    30. Un conteneur dont le seul metier est de dire une marge
								//    exacte ne peut pas en rendre une autre — c'est la
								//    definition meme d'un attribut non honore.
								ctx.layout.cursor = {dedans.x, dedans.y};
								ctx.layout.lineStartX = dedans.x;
								ctx.layout.prevItem = {dedans.x, dedans.y, 0.f, 0.f};
								MonterCorps(ctx, w, etat, rap, prof + 1u, horizontal, false, hooks);
								ctx.layout = sauveP;
							}
							Noter(ctx, rap, id, t, rP, prof, true, horizontal, &ctx.layout.region);
							++rap.montes;
							return;
						}
						case NkGuiRole::Aspect: {
							// `ratio = 1.777` — la largeur du flux decide, la hauteur suit.
							// C'est ce qu'un apercu de camera, une vignette ou une zone de
							// rendu demandent, et aucun role existant ne sait le dire.
							const float32 ratio = NkGNombre(w, "ratio", 1.f);
							if (ratio <= 0.f) {
								// ⚠️ UN RAPPORT NUL OU NEGATIF NE SE CORRIGE PAS EN SILENCE :
								//    il donnerait une hauteur absurde ou une division par zero.
								//    On le compte et on monte le contenu EN FLUX, sans imposer
								//    de rectangle — le document garde ce qu'il a ecrit ailleurs.
								++rap.attributsNonHonores;
								const NkVec2 cA0 = ctx.layout.cursor;
								MonterCorps(ctx, w, etat, rap, prof + 1u, horizontal, false, hooks);
								Noter(ctx, rap, id, t, BlocConsomme(ctx, cA0), prof, true, horizontal,
									  &ctx.layout.region);
								++rap.montes;
								return;
							}
							float32 larg = ctx.layout.region.w
										   - (ctx.layout.cursor.x - ctx.layout.region.x);
							float32 sW = 0.f, sH = 0.f;
							if (LireVec2(w, "size", sW, sH) && sW > 0.f)
								larg = sW;
							const NkRect rA = ctx.NextItemRect(larg, larg / ratio);
							{
								const NkGuiLayout sauveA = ctx.layout;
								ctx.BeginLayout(rA);
								MonterCorps(ctx, w, etat, rap, prof + 1u, horizontal, false, hooks);
								ctx.layout = sauveA;
							}
							Noter(ctx, rap, id, t, rA, prof, true, horizontal, &ctx.layout.region);
							++rap.montes;
							return;
						}
						case NkGuiRole::Overlay: {
							// ⚠️ LA BRIQUE EXISTE DEJA : `PushOverlay`/`PopOverlay`. Ce role
							//    ne fait que l'ouvrir au format — et c'est precisement ce qui
							//    manquait, puisque le monteur n'avait aucune facon de dire
							//    « ceci se peint par-dessus » alors que NKGui le sait faire.
							//
							// ⚠️ ET LA SURIMPRESSION N'EST PAS UNE REGION : elle ne deplace
							//    rien, elle change la COUCHE. Le contenu garde donc le flux
							//    courant, et le rectangle note est ce qu'il a consomme.
							const NkVec2 cO = ctx.layout.cursor;
							PushOverlay(ctx);
							MonterCorps(ctx, w, etat, rap, prof + 1u, horizontal, false, hooks);
							PopOverlay(ctx);
							++rap.surimpressions;
							Noter(ctx, rap, id, t, BlocConsomme(ctx, cO), prof, true, horizontal,
								  &ctx.layout.region);
							++rap.montes;
							return;
						}
						case NkGuiRole::Center: {
							// ⚠️ CENTRER DEMANDE DE CONNAITRE LA TAILLE AVANT DE PLACER, ET
							//    UNE INTERFACE IMMEDIATE NE LA CONNAIT QU'APRES. C'est la
							//    vraie difficulte de ce role, et elle ne se contourne pas
							//    sans mentir. Deux issues honnetes existaient :
							//
							//      (a) mesurer a l'image N et centrer a l'image N+1 — le
							//          contenu saute une fois a l'ouverture, et il faut
							//          garder un etat par widget ;
							//      (b) centrer ce que le document DECLARE, c'est-a-dire la
							//          somme des `size` de ses enfants.
							//
							//    (b) est retenue : elle est exacte quand le document dit sa
							//    taille — ce que `size` en flux permet depuis ce matin — et
							//    elle ne fabrique rien quand il ne la dit pas.
							//
							// ⚠️ UN ENFANT SANS `size` N'EST PAS CENTRE, ET C'EST COMPTE. Le
							//    faux confort aurait ete de centrer sur une taille devinee :
							//    le contenu se serait pose a un endroit que personne n'a
							//    demande, et aucun chiffre ne l'aurait dit.
							NkRect rC = pl.pose ? pl.rect : ctx.layout.region;
							if (!pl.pose) {
								rC.w -= (ctx.layout.cursor.x - rC.x);
								rC.h -= (ctx.layout.cursor.y - rC.y);
								rC.x = ctx.layout.cursor.x;
								rC.y = ctx.layout.cursor.y;
							}
							NkRect rTailleC;
							if (!pl.pose && TailleEnFlux(ctx, w, rTailleC))
								rC = rTailleC;
							// La taille DECLAREE du contenu : somme en hauteur, maximum en
							// largeur (l'axe du flux interieur est vertical par defaut).
							float32 totalH = 0.f, maxW = 0.f;
							uint32 sansTaille = 0u;
							const NkArchiveNode *cc = NkGMonteCorps(w);
							if (cc) {
								for (uint32 k = 0; k < (uint32)cc->array.Size(); ++k) {
									if (!cc->array[k].IsObject() || !cc->array[k].object)
										continue;
									float32 ew = 0.f, eh = 0.f;
									if (LireVec2(*cc->array[k].object, "size", ew, eh)
										&& (ew > 0.f || eh > 0.f)) {
										totalH += (eh > 0.f) ? eh : ctx.ItemHeight();
										if (ew > maxW)
											maxW = ew;
									} else {
										++sansTaille;
									}
								}
							}
							NkRect dedansC = rC;
							if (sansTaille == 0u && totalH > 0.f) {
								const float32 dy = (rC.h - totalH) * 0.5f;
								const float32 dx = (maxW > 0.f) ? (rC.w - maxW) * 0.5f : 0.f;
								dedansC.x = rC.x + (dx > 0.f ? dx : 0.f);
								dedansC.y = rC.y + (dy > 0.f ? dy : 0.f);
								dedansC.w = rC.w - (dx > 0.f ? dx : 0.f);
								dedansC.h = rC.h - (dy > 0.f ? dy : 0.f);
								++rap.centrages;
							} else if (sansTaille > 0u) {
								rap.centragesImpossibles += sansTaille;
							}
							{
								const NkGuiLayout sauveC = ctx.layout;
								ctx.BeginLayout(dedansC);
								// Meme raison que pour `Padding` : `BeginLayout` ajouterait la
								// marge du theme au point calcule, et les quatre marges du
								// centrage ne seraient plus egales. Mesure : 110/90 contre
								// 90/70, soit l'ecart d'exactement deux marges.
								ctx.layout.cursor = {dedansC.x, dedansC.y};
								ctx.layout.lineStartX = dedansC.x;
								ctx.layout.prevItem = {dedansC.x, dedansC.y, 0.f, 0.f};
								MonterCorps(ctx, w, etat, rap, prof + 1u, false, false, hooks);
								ctx.layout = sauveC;
							}
							Noter(ctx, rap, id, t, rC, prof, true, horizontal, &ctx.layout.region);
							++rap.montes;
							return;
						}
						case NkGuiRole::Flow: {
							// Horizontal AVEC retour a la ligne : c'est la seule difference avec
							// une `HBox`, et NKGui la porte deja (`BeginFlow`).
							const float32 gapF = NkGA(w, "gap") ? NkGNombre(w, "gap", -1.f) : -1.f;
							const NkVec2 cF = ctx.layout.cursor;
							BeginFlow(ctx, gapF);
							MonterCorps(ctx, w, etat, rap, prof + 1u, true, false, hooks);
							EndFlow(ctx);
							Noter(ctx, rap, id, t, BlocConsomme(ctx, cF), prof, true, horizontal, &ctx.layout.region);
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
							Noter(ctx, rap, id, t, BlocConsomme(ctx, cG), prof, true, horizontal, &ctx.layout.region);
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
							// ═══════════════════════════════════════════════════
							//  LE DEPASSEMENT SE DECLARE, IL NE SE PROGRAMME PAS
							// ═══════════════════════════════════════════════════
							//  Rodolf, 28/09 : « si c'est en C++ comment les
							//  utilisateurs peuvent reproduire ce comportement sur
							//  leurs applications ? pense toujours aux utilisateurs
							//  autres que nous ».
							//
							//  Deux cles sur le noeud `MenuBar`, et rien a ecrire :
							//    `maxMenus`      = nombre de titres toleres dans la
							//                      bande ; au-dela, « … ».
							//    `reserveDroite` = pixels gardes libres a droite --
							//                      la place d'un titre de document
							//                      centre, d'un champ de recherche,
							//                      de ce que l'hote y met.
							//
							//  ⚠️ `reserveDroite` PLUTOT QU'UN `x` ABSOLU, et c'est
							//     delibere : un document ne connait pas la largeur de
							//     la fenetre ou il sera monte. Une abscisse ecrite en
							//     dur serait juste sur l'ecran de l'auteur et fausse
							//     sur celui de l'utilisateur. La reserve se mesure
							//     depuis le bord, donc elle voyage.
							//
							//  ⚠️ ABSENTES = ANCIEN COMPORTEMENT A L'IDENTIQUE.
							//     `maxMenus` vaut -1 (pas de plafond) et la reserve
							//     0 : un document ecrit avant aujourd'hui monte
							//     exactement comme avant. C'est la condition pour que
							//     ce soit un ajout et non une rupture de format.
							{
								const float32 reserve = NkGNombre(w, "reserveDroite", 0.f);
								const float32 maxM = NkGNombre(w, "maxMenus", -1.f);
								// ⚠️ « NE CHANGE RIEN » S'ECRIT AVEC LA VALEUR COURANTE,
								//    jamais avec un zero : une limite calculee peut
								//    valoir zero ou moins, et un zero-sentinelle
								//    l'aurait alors avalee en silence.
								if (reserve > 0.f || maxM >= 0.f)
									NkGuiMenuBarPolitique(
										ctx,
										reserve > 0.f ? (bande.x + bande.w - reserve)
													  : ctx.menuBarLimite,
										maxM >= 0.f ? (int32)maxM : -1);
							}
							++rap.barresMenu;
							// Les titres se posent horizontalement, et c'est `menuBarX`
							// qui les avance -- pas le curseur de mise en page.
							MonterCorps(ctx, w, etat, rap, prof + 1u, true, false, hooks);
							EndMenuBar(ctx);
							Noter(ctx, rap, id, t, bande, prof, true, horizontal, &ctx.layout.region);
							++rap.montes;
							return;
						}
						case NkGuiRole::Menu: {
							const NkString lbl = NkGLibelle(w, id);
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
							Noter(ctx, rap, id, t, ctx.layout.prevItem, prof, true, horizontal,
								  &ctx.layout.region);
							++rap.montes;
							return;
						}
						// ── P7 : LA COURBE EDITABLE ──────────────────────────
						case NkGuiRole::CurveField: {
							const NkString lien = NkGTexte(w, "bind", "");
							const float32 vmin = NkGNombre(w, "min", 0.f);
							const float32 vmax = NkGNombre(w, "max", 1.f);
							const float32 haut = NkGNombre(w, "height", 72.f);
							const NkString xLab = NkGTexte(w, "xLabel", "");
							const NkString yLab = NkGTexte(w, "yLabel", "");
							const NkRect cadre = ctx.NextItemRect(-1.f, haut);
							++rap.courbes;
							NkGuiDrawList &dlC = ctx.DL();
							// Le cadre et la grille d'abord : ils existent meme sans courbe,
							// et c'est ce qui rend une courbe ABSENTE reperable.
							// `track` est le fond que le theme donne deja aux curseurs : un
							// champ de courbe est de la meme famille, et en inventer un
							// second aurait fait deux fonds pour une meme idee.
							dlC.AddRectFilled(cadre, ctx.theme.track, ctx.theme.rounding);
							dlC.AddRect(cadre, ctx.theme.border, 1.f, ctx.theme.rounding);
							for (uint32 g = 1; g < 4u; ++g) {
								const float32 gy = cadre.y + cadre.h * ((float32)g / 4.f);
								dlC.AddLine({cadre.x + 1.f, gy}, {cadre.x + cadre.w - 1.f, gy},
											ctx.theme.border, 1.f);
							}
							NkVec2 pts[NkGuiMonteHooks::kPointsCourbeMax];
							uint32 nPts = 0u;
							const bool servie =
								hooks
								&& hooks->CourbeLiee(lien.Size() > 0 ? lien.CStr() : id.CStr(), pts,
													 NkGuiMonteHooks::kPointsCourbeMax, nPts)
								&& nPts >= 2u;
							if (!servie) {
								// ⚠️ MEME POLITIQUE QU'UNE ZONE `Host` NON SERVIE : on ne
								//    dessine pas une courbe inventee, on DIT qu'il n'y en a
								//    pas. Une droite de repli aurait ete indiscernable d'une
								//    vraie courbe plate — c'est-a-dire un reglage faux qui a
								//    l'air juste.
								++rap.courbesSansHote;
								MarquerZoneVide(ctx, cadre, lien.Size() > 0 ? lien.CStr()
																			: id.CStr());
								Noter(ctx, rap, id, t, cadre, prof, false, horizontal,
									  &ctx.layout.region);
								++rap.montes;
								return;
							}
							// ── LES POINTS EN PIXELS ────────────────────────────
							// ⚠️ UNE PLAGE NULLE NE DIVISE PAS. `min == max` est un document
							//    fautif, pas un plantage : on le compte et on aplatit.
							const float32 plage = (vmax - vmin);
							const bool plageNulle = (plage <= 0.f && plage >= 0.f);
							if (plageNulle)
								++rap.attributsNonHonores;
							NkVec2 ecran[NkGuiMonteHooks::kPointsCourbeMax];
							const float32 marge = 4.f;
							const float32 x0 = cadre.x + marge, larg = cadre.w - 2.f * marge;
							const float32 y0 = cadre.y + marge, hUtile = cadre.h - 2.f * marge;
							for (uint32 i = 0; i < nPts; ++i) {
								float32 fx = pts[i].x;
								if (fx < 0.f)
									fx = 0.f;
								if (fx > 1.f)
									fx = 1.f;
								float32 fy = plageNulle ? 0.5f : (pts[i].y - vmin) / plage;
								if (fy < 0.f)
									fy = 0.f;
								if (fy > 1.f)
									fy = 1.f;
								// L'axe des ordonnees monte : `y = max` en HAUT de l'ecran.
								ecran[i] = {x0 + larg * fx, y0 + hUtile * (1.f - fy)};
							}
							dlC.AddPolyline(ecran, (int32)nPts, ctx.theme.accent, 2.f);
							// ── LE GESTE : ON TIRE UN POINT EN Y ────────────────
							// ⚠️ EN Y SEULEMENT, ET C'EST ECRIT PARCE QUE CA SE VOIT. Tirer
							//    aussi en X voudrait dire REORDONNER les points, donc decider
							//    ce que devient une abscisse qui depasse sa voisine — une
							//    question de conception que le document 09 §P7 ne tranche pas
							//    (« la vitesse LE LONG d'une trajectoire » : l'abscisse est
							//    l'avancement, elle ne se deplace pas). Le jour ou elle sera
							//    tranchee, la ligne a changer est celle-ci.
							bool bouge = false;
							for (uint32 i = 0; i < nPts; ++i) {
								const float32 rayon = 4.f;
								const NkRect poignee{ecran[i].x - 6.f, ecran[i].y - 6.f, 12.f,
													 12.f};
								char cle[96];
								NkGuiFormaterCleCourbe(cle, sizeof(cle),
													   lien.Size() > 0 ? lien.CStr() : id.CStr(),
													   i);
								bool survolP = false, tenu = false;
								(void)ctx.ButtonBehavior(ctx.GetId(cle), poignee,
														 NkGuiButtonFlags::None, -1.f, -1.f,
														 &survolP, &tenu);
								if (tenu && !plageNulle && hUtile > 0.f) {
									const NkVec2 g = ctx.PositionGeste();
									float32 f = 1.f - (g.y - y0) / hUtile;
									if (f < 0.f)
										f = 0.f;
									if (f > 1.f)
										f = 1.f;
									const float32 nv = vmin + plage * f;
									if (nv > pts[i].y || nv < pts[i].y) {
										pts[i].y = nv;
										bouge = true;
									}
								}
								// ⚠️ `onAccent` ET NON UNE NUANCE D'ACCENT. Le theme n'a pas de
								//    `accentHover`, et les candidats proches (`buttonActive` =
								//    96,150,230 contre accent = 96,165,250) sont a quinze
								//    unites l'un de l'autre : un critere en pixels ne les
								//    distinguerait pas, donc le survol serait invisible a la
								//    mesure comme a l'oeil. `onAccent` est le blanc que le
								//    theme destine a ce qui se POSE sur un accent.
								dlC.AddCircleFilled(ecran[i], rayon,
													(tenu || survolP) ? ctx.theme.onAccent
																	  : ctx.theme.accent);
							}
							if (bouge) {
								++rap.pointsCourbeDeplaces;
								hooks->CourbeModifiee(lien.Size() > 0 ? lien.CStr() : id.CStr(),
													  pts, nPts);
							}
							// Les etiquettes, en dernier : elles se lisent PAR-DESSUS.
							if (ctx.font && ctx.font->Valid()) {
								if (xLab.Size() > 0)
									dlC.AddText(ctx.font->Face(), ctx.font->TexId(),
												{cadre.x + 6.f,
												 cadre.y + cadre.h - 4.f},
												xLab.CStr(), ctx.theme.textMuted);
								if (yLab.Size() > 0)
									dlC.AddText(ctx.font->Face(), ctx.font->TexId(),
												{cadre.x + 6.f,
												 cadre.y + 4.f + ctx.font->Ascent()},
												yLab.CStr(), ctx.theme.textMuted);
							}
							Noter(ctx, rap, id, t, cadre, prof, false, horizontal, &ctx.layout.region);
							++rap.montes;
							return;
						}
						// ── LE MENU CONTEXTUEL (refus leve le 27/09) ────────
						case NkGuiRole::ContextMenu: {
							// Le crochet decide, et le document dit CE QU'IL Y A DEDANS.
							// C'est le partage que le refus d'origine demandait : l'hote
							// sait QUAND et OU, le fichier sait QUOI.
							NkVec2 pos{0.f, 0.f};
							const bool ouvre =
								hooks && hooks->MenuContextuelOuvre(id.CStr(), pos);
							if (ouvre)
								ctx.OpenPopupAt(ctx.GetId(id.CStr()), pos);
							if (BeginPopupMenu(ctx, id.CStr())) {
								++rap.menusContextuelsOuverts;
								MonterCorps(ctx, w, etat, rap, prof + 1u, false, false, hooks);
								EndPopupMenu(ctx);
							} else if (!ouvre) {
								// ⚠️ LE CHIFFRE QUI EMPECHE UN ROLE INVISIBLE DE SE FAIRE
								//    PASSER POUR MONTE. Un menu que personne n'ouvre est
								//    exactement ce que le refus d'origine redoutait : « il
								//    se compterait parmi les montes, et personne n'irait
								//    chercher pourquoi rien n'apparait ». Ici, il se
								//    compte A PART.
								++rap.menusContextuelsSansHote;
							}
							// ⚠️ ET ON NE NOTE PAS `prevItem`. Un menu contextuel ne prend
							//    aucune place dans le flux — le lire donnerait le rectangle
							//    du widget d'AVANT, c'est-a-dire un releve faux qui a l'air
							//    juste. Meme piege que `lastItemRect` en 09/14.
							Noter(ctx, rap, id, t, NkRect{pos.x, pos.y, 0.f, 0.f}, prof, true,
								  horizontal, &ctx.layout.region);
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
							const NkString lbl = NkGLibelle(w, id);
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
						case NkGuiRole::Canvas: {
							// ── LE RECTANGLE, comme une zone hote ───────────────
							const NkGuiTailleRel relC = NkGuiLireTailleRelative(w, ctx.layout.region);
							const NkRect zone = ctx.NextItemRect(relC.aW ? relC.w : -1.f,
																 relC.aH ? relC.h : ctx.ItemHeight() * 8.f);
							if (zone.w <= 0.f || zone.h <= 0.f)
								break;
							++rap.toiles;

							// ── LE CADRAGE, ET IL SURVIT A L'IMAGE ──────────────
							// ⚠️ IL VIT DANS L'ETAT DU WIDGET, PAS DANS UNE VARIABLE DE
							//    CE `case`. Un cadrage recalcule a chaque image serait
							//    remis a zero soixante fois par seconde : la toile
							//    reviendrait a son origine des qu'on lache la souris.
							//    `deplacement` porte le centre, `f` le zoom -- deux
							//    champs qui existent deja et qu'aucun autre role
							//    n'emploie sur une toile.
							NkGuiVue vue;
							if (e) {
								if (!e->initialise) {
									// Premier montage : zoom 1, centre a l'origine du monde.
									e->f = 1.f;
									e->deplacement = NkVec2(0.f, 0.f);
									e->initialise = true;
								}
								vue.centre = e->deplacement;
								vue.zoom = e->f > 0.0001f ? e->f : 1.f;
							}

							const NkVec2 souris = ctx.input.mousePos;
							const bool dedansC = (souris.x >= zone.x && souris.x <= zone.x + zone.w
												  && souris.y >= zone.y
												  && souris.y <= zone.y + zone.h);

							// ── LE ZOOM, AU CURSEUR ─────────────────────────────
							// 🔴 AU CURSEUR, PAS AU CENTRE, ET C'EST TOUT L'INTERET DE
							//    METTRE CE CALCUL DANS LE FORMAT. Zoomer au centre est
							//    ce qu'on ecrit quand on va vite, et c'est ce qui rend
							//    une toile penible a l'usage : le point qu'on regarde
							//    s'enfuit. La regle tient en trois lignes -- le point du
							//    MONDE sous le curseur ne doit pas bouger -- mais elle ne
							//    s'invente pas, et quatre surfaces de la famille la
							//    voudront.
							if (e && dedansC && ctx.input.wheel != 0.f) {
								const NkVec2 avant = vue.VersMonde(souris, zone);
								float32 z = vue.zoom * (ctx.input.wheel > 0.f ? 1.1f : (1.f / 1.1f));
								// Bornes : un zoom nul rendrait la transformation non
								// inversible, et tout clic tomberait a l'infini.
								if (z < 0.02f) z = 0.02f;
								if (z > 64.f) z = 64.f;
								vue.zoom = z;
								// MUTATION DE BANC, NK_TOILE_MUTATION=centre : le zoom se
								// fait au CENTRE. C'est la version qu'on ecrit quand on va
								// vite, et elle existe ici pour que le critere « le point
								// du monde sous le curseur ne bouge pas » ait quelque
								// chose a refuter. Sans la variable, ce bloc ne change
								// rien du tout.
								static const bool kZoomCentre = []() {
									const char *v = getenv("NK_TOILE_MUTATION");
									return v && v[0] == 'c';
								}();
								if (!kZoomCentre) {
									const NkVec2 apres = vue.VersMonde(souris, zone);
									vue.centre.x += avant.x - apres.x;
									vue.centre.y += avant.y - apres.y;
								}
								++rap.toilesZoomees;
							}

							// ── LE DEPLACEMENT, au bouton du MILIEU ─────────────
							// Le milieu parce que le gauche appartient au contenu
							// (selectionner, tirer une poignee) : le format ne doit pas
							// se servir avant l'application.
							if (e && dedansC && ctx.input.mouseDown[2]
								&& (ctx.input.mouseDelta.x != 0.f || ctx.input.mouseDelta.y != 0.f)) {
								const float32 z = vue.zoom > 0.0001f ? vue.zoom : 1.f;
								vue.centre.x -= ctx.input.mouseDelta.x / z;
								vue.centre.y -= ctx.input.mouseDelta.y / z;
								++rap.toilesDeplacees;
							}

							if (e) {
								e->deplacement = vue.centre;
								e->f = vue.zoom;
							}

							// ── LE FOND ET LA GRILLE ────────────────────────────
							NkGuiDrawList &dlC = ctx.DL();
							dlC.AddRectFilled(zone, ctx.theme.bgPrimary, 0.f);
							if (!NkGMotEgal(NkStringView(NkGTexte(w, "grid", "on").CStr()), "off"))
								PeindreGrille(ctx, zone, vue);

							// ── LE CONTENU, PAR L'APPLICATION ───────────────────
							const bool peint = hooks && hooks->RemplirToile(ctx, id.CStr(), zone, vue);
							if (!peint) {
								// Meme marqueur que la zone hote : visible et NOMME.
								++rap.toilesNonRemplies;
								MarquerZoneVide(ctx, zone, id.CStr());
							}
							dlC.AddRect(zone, ctx.theme.border, 1.f);
							Noter(ctx, rap, id, t, zone, prof, true, horizontal, &ctx.layout.region);
							++rap.montes;
							return;
						}
						case NkGuiRole::Host: {
							++rap.hotes;
							// Un `pos`/`size` pose a deja arme `SetNextItemRect` avant le switch :
							// `NextItemRect` le rend tel quel. Sinon, la zone lit sa TAILLE RELATIVE
							// (« 60 % de mon parent ») ; a defaut, elle prend la largeur disponible et
							// quatre hauteurs d'item -- assez pour se voir.
							// ═══════════════════════════════════════════════════
							//  🔴 DANS UN MENU, UNE ZONE HOTE NE RESERVE RIEN
							// ═══════════════════════════════════════════════════
							//  Rodolf, 28/09 : « plusieurs menus ou sous-menus sont
							//  mal alignes ». Capture : le sous-menu « Theme »
							//  montrait un grand vide, puis « Sombre » et « Clair »
							//  tout en bas.
							//
							//  LA CAUSE : cette ligne reservait
							//  `ItemHeight() * 4` AVANT d'appeler l'hote -- et les
							//  hotes de menu, eux, dessinent AU FIL (`MenuItem`
							//  prend `NextItemRect`). On obtenait donc quatre
							//  rangees vides, puis les entrees en dessous.
							//
							//  ⚠️ LA RESERVATION RESTE PARTOUT AILLEURS. Un `Host`
							//     de panneau EST une surface que l'hote peint :
							//     c'est le contrat, et `zone` a un sens pour lui.
							//     Dans un popup, il n'y a pas de surface -- il n'y
							//     a qu'un flux d'entrees. Le distinguer par
							//     `curPopupLevel` ne demande aucune cle nouvelle
							//     au document : le monteur SAIT ou il est.
							const bool dansMenu = ctx.curPopupLevel >= 0;
							const NkGuiTailleRel relH = NkGuiLireTailleRelative(w, ctx.layout.region);
							NkRect zone;
							if (dansMenu)
								zone = {ctx.layout.cursor.x, ctx.layout.cursor.y,
										ctx.layout.region.w, 0.f};
							else
								zone = ctx.NextItemRect(relH.aW ? relH.w : -1.f,
														relH.aH ? relH.h : ctx.ItemHeight() * 4.f);
							const bool rempli = hooks && hooks->RemplirHote(ctx, id.CStr(), zone);
							if (!rempli && dansMenu) {
								// ⚠️ LE MARQUEUR, LUI, A BESOIN D'UNE PLACE. Une
								//    zone non remplie doit SE VOIR ; sans rect, elle
								//    disparaitrait en silence -- exactement ce que ce
								//    role existe pour empecher.
								zone = ctx.NextItemRect(-1.f, ctx.ItemHeight());
							}
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
								// LES HACHURES, par la porte unique : elles disent « rien n'est
								// monte ici », et aucun contenu d'application ne leur ressemble.
								NkGuiHachurer(dl, zone, trait);
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

					// ═════════════════════════════════════════════════════════
					//  L'INFOBULLE ET LE RACCOURCI — `tooltip`, et P8
					// ═════════════════════════════════════════════════════════
					//  🔴 `tooltip` EST UNE PROPRIETE UNIVERSELLE DU FORMAT QUE
					//     PERSONNE NE LISAIT. Mesure du 27/09 : ZERO occurrence de
					//     « tooltip » dans tout ce fichier. Chaque infobulle de
					//     chaque document -- et les specifications d'interface en
					//     ecrivent des centaines -- tombait en silence, alors que
					//     `SetTooltip` existe dans NKGui depuis toujours.
					//
					//  ⚠️ P8, LE `shortcut` UNIVERSEL, ENTRE PAR LA MEME PORTE, et
					//     c'est ce que la specification demande : « sens :
					//     AFFICHAGE SEULEMENT. La verite des raccourcis reste dans
					//     l'application (sinon deux sources). » Il s'affiche donc
					//     DANS l'infobulle -- « Deplacer (W) » -- et ne declare
					//     aucune touche a personne.
					// ═════════════════════════════════════════════════════════
					//  LES HACHURES (P11) — par-dessus le fond, sous l'icône
					// ═════════════════════════════════════════════════════════
					//  ⚠️ ELLES SE POSENT SUR LE RECTANGLE MONTÉ, pas sur celui que
					//     le fond a peint : un widget dont le fond est tenu par son
					//     `case` (un `Button`, une `Window`) n'a pas de rectangle
					//     réservé ici. `prevItem` est le seul que tous partagent.
					if (app.aHachures && !hachuresPeintes) {
						const NkRect rh = ctx.layout.prevItem;
						if (rh.w > 0.f && rh.h > 0.f) {
							NkGuiHachurer(ctx.DL(), rh, ctx.theme.textMuted);
							++rap.apparencesPeintes;
						} else {
							++rap.apparencesNonPeintes;
						}
					}

					// ═════════════════════════════════════════════════════════
					//  L'ICÔNE (P12) — dessinée par le jeu que l'hôte a posé
					// ═════════════════════════════════════════════════════════
					//  ⚠️ ELLE SE POSE SUR LE RECTANGLE DÉJÀ MONTÉ, et c'est une
					//     limite assumée : les primitives de NKGui ne prennent pas
					//     d'icône, donc le monteur ne peut pas la faire entrer DANS
					//     la mise en page du bouton. Elle occupe le carré de gauche
					//     de l'élément. Un libellé long peut donc passer dessous.
					//     *Le dire vaut mieux que de découvrir un chevauchement.*
					//
					//  ⚠️ ET UNE ICÔNE MANQUANTE SE COMPTE. `AddIcon` distingue le
					//     glyphe demandé du glyphe de SECOURS : sans ce compteur, un
					//     carré muet passerait pour l'icône qu'on voulait.
					{
						// ⚠️ L'ETAT PRIME SUR LE DOCUMENT, exactement comme pour
						//    `visible` et `enabled` plus haut. Sans cette ligne,
						//    `set x.icon = "chevron-down"` ecrirait dans l'etat une
						//    valeur que PERSONNE ne lirait : le document garderait la
						//    main, le chevron ne tournerait jamais, et rien ne s'en
						//    plaindrait. C'est la forme exacte du defaut que ce depot
						//    a paye sur `size` en flux.
						NkString icone = NkGTexte(w, "icon", "");
						if (e && e->iconeDite)
							icone = NkString(e->icone);
						if (icone.Size() > 0u) {
							++rap.iconesDemandees;
							const NkGuiIconSet *jeu = NkGuiIconesPosees();
							const NkRect ri = ctx.layout.prevItem;
							if (!jeu || ri.h <= 0.f) {
								++rap.iconesManquantes;
							} else {
								const float32 cote = ri.h * 0.7f;
								const NkRect carre{ri.x + (ri.h - cote) * 0.5f,
												   ri.y + (ri.h - cote) * 0.5f, cote, cote};
								const NkGuiIconDraw fait =
									AddIcon(ctx.DL(), *jeu, jeu->Find(icone.CStr()), carre,
											ctx.theme.text);
								if (fait != NkGuiIconDraw::Glyph)
									++rap.iconesManquantes;
							}
						}
					}

					{
						const NkString bulle = NkGTexte(w, "tooltip", "");
						const NkString racc = NkGTexte(w, "shortcut", "");
						if (racc.Size() > 0u)
							++rap.raccourcisAffiches;
						if ((bulle.Size() > 0u || racc.Size() > 0u) && ctx.IsItemHovered()) {
							NkString texte = bulle;
							if (racc.Size() > 0u) {
								texte.Append(texte.Size() > 0u ? " (" : "(");
								texte.Append(racc.CStr());
								texte.Append(")");
							}
							SetTooltip(ctx, texte.CStr());
							++rap.infobullesPosees;
						}
					}

					Noter(ctx, rap, id, t, ctx.layout.prevItem, prof, false, horizontal, &ctx.layout.region);
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

				/// Une zone d'ancrage que PERSONNE ne fournit : contour, hachures, et SON
				/// NOM ecrit dedans.
				///
				/// ⚠️ ELLE EST UNE FONCTION PARCE QU'IL Y A DEUX CHEMINS DE PLACEMENT
				///    (les fractions horizontales, et la disposition `layout` depuis le
				///    27/09). Recopier le marquage dans le second aurait fait deux facons
				///    de signaler le meme manque, et le jour ou l'une change l'autre
				///    ment. Le nom est ECRIT parce que c'est celui que l'application doit
				///    servir : sans lui, l'hote cherche a l'aveugle.
				static void MarquerZoneVide(NkGuiContext &ctx, const NkRect &r,
											const char *nom) noexcept {
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
						// `AddText` prend la LIGNE DE BASE, pas le haut : sans
						// l'ascendante, le nom sortait par le haut de la fenetre -- la
						// capture l'a montre coupe en deux.
						dl.AddText(ctx.font->Face(), ctx.font->TexId(),
								   {r.x + 6.f, r.y + 4.f + ctx.font->Ascent()}, nom, trait);
				}

				// =====================================================================
				//  P10 — LES AMARRAGES DU DOCUMENT, APPLIQUES AU `DockSpace`
				// =====================================================================
				/// Place les zones d'un `DockSpace` d'apres la disposition active du
				/// document. Rend faux quand AUCUNE zone n'est amarree — l'appelant garde
				/// alors son partage par fractions horizontales, inchange.
				///
				/// ⚠️ STRICTEMENT ADDITIF, ET C'EST CE `return false` QUI LE GARANTIT. Les
				///    documents sans section `layout` — c'est-a-dire tout le corpus
				///    d'avant — passent par le meme code qu'hier, au pixel.
				///
				/// ⚠️ LA FRACTION SE RAPPORTE A LA ZONE ENTIERE, PAS AU RESTE. Deux
				///    raisons, et la seconde est la vraie : la disposition de NKCraft
				///    est deja ecrite ainsi (`NkLayout::Compute(W, H, fLeft = 0.16f,
				///    fRight = 0.29f)`), et surtout une fraction du RESTE dependrait de
				///    l'ORDRE d'ecriture des amarrages — `left 0.2` puis `right 0.2` ne
				///    donneraient pas la meme largeur que l'inverse. Une TAILLE qui depend
				///    de l'ordre est une taille qu'on ne peut pas relire.
				///
				/// ⚠️ LA FORME, ELLE, DEPEND DE L'ORDRE — ET C'EST VRAI DE TOUT DOCK. Le
				///    premier cote amarre traverse la zone entiere ; les suivants ne
				///    traversent que ce qui reste. `left` puis `top` donne une colonne
				///    pleine hauteur et une barre courte ; l'inverse donne une barre pleine
				///    largeur. Mesure (b15) : `outils` sort en 80 x 300 et `barre` en
				///    220 x 30. Ce n'est pas une imprecision a corriger, c'est ce que
				///    l'ordre du document VEUT DIRE — mais il ne faut pas confondre les
				///    deux affirmations, et la premiere version de ce commentaire les
				///    confondait.
				static bool PlacerParAmarrage(const NkGuiMonteEtat &etat,
											  const NkArchiveNode *cd, const NkRect &zone,
											  NkVector<NkRect> &out,
											  NkGuiMonteRapport &rap) noexcept {
					if (!cd || !etat.DispositionActive())
						return false;
					out.Clear();
					NkRect reste = zone;
					uint32 amarrees = 0u;
					// Passe 1 : les cotes decoupent. Les centres et les zones que la
					// disposition ne nomme pas attendent la passe 2.
					for (uint32 k = 0; k < (uint32)cd->array.Size(); ++k) {
						if (!cd->array[k].IsObject() || !cd->array[k].object)
							continue;
						const NkString nom(NkGuiArchive::IdOf(*cd->array[k].object));
						NkGuiCote cote = NkGuiCote::Centre;
						float32 frac = 0.f;
						NkRect r{0.f, 0.f, 0.f, 0.f};
						if (etat.Amarrage(nom.CStr(), cote, frac)
							&& cote != NkGuiCote::Centre) {
							float32 d = 0.f;
							switch (cote) {
								case NkGuiCote::Gauche:
								case NkGuiCote::Droite:
									d = zone.w * frac;
									if (d > reste.w)
										d = reste.w;
									break;
								default:
									d = zone.h * frac;
									if (d > reste.h)
										d = reste.h;
									break;
							}
							if (cote == NkGuiCote::Gauche) {
								r = {reste.x, reste.y, d, reste.h};
								reste.x += d;
								reste.w -= d;
							} else if (cote == NkGuiCote::Droite) {
								r = {reste.x + reste.w - d, reste.y, d, reste.h};
								reste.w -= d;
							} else if (cote == NkGuiCote::Haut) {
								r = {reste.x, reste.y, reste.w, d};
								reste.y += d;
								reste.h -= d;
							} else {
								r = {reste.x, reste.y + reste.h - d, reste.w, d};
								reste.h -= d;
							}
							++amarrees;
						}
						out.PushBack(r);
					}
					if (amarrees == 0u)
						return false; // rien d'amarre : l'appelant garde son chemin
					// Passe 2 : ce qui RESTE se partage entre les zones non amarrees. Sans
					// elle, une zone que la disposition ne nomme pas serait un rectangle
					// nul, c'est-a-dire un panneau que l'hote fournit et qu'on n'affiche
					// pas -- exactement le « trou qui ressemble a un fond » que ce `case`
					// combat deja par ses hachures.
					uint32 nCentres = 0u, vu = 0u;
					for (uint32 k = 0; k < (uint32)cd->array.Size(); ++k) {
						if (!cd->array[k].IsObject() || !cd->array[k].object)
							continue;
						if (out[vu].w <= 0.f && out[vu].h <= 0.f)
							++nCentres;
						++vu;
					}
					if (nCentres > 0u) {
						const float32 part = reste.w / (float32)nCentres;
						float32 x = reste.x;
						uint32 place = 0u;
						vu = 0u;
						for (uint32 k = 0; k < (uint32)cd->array.Size(); ++k) {
							if (!cd->array[k].IsObject() || !cd->array[k].object)
								continue;
							if (out[vu].w <= 0.f && out[vu].h <= 0.f) {
								++place;
								// Le DERNIER centre absorbe l'arrondi jusqu'au bord : des
								// divisions flottantes ne retombent pas sur le pixel, et un
								// dock qui laisse une bande noire n'est pas un dock.
								const float32 larg = (place == nCentres)
														 ? (reste.x + reste.w - x)
														 : part;
								out[vu] = {x, reste.y, larg, reste.h};
								x += larg;
							}
							++vu;
						}
					}
					rap.zonesAmarrees += amarrees;
					return true;
				}

				/// `size` SUR UN CONTENEUR EN FLUX : decoupe son rectangle dans le flux
				/// du parent. Rend faux quand le document ne dit pas de taille — alors
				/// le conteneur prend « tout ce qui reste », et l'appelant le COMPTE.
				///
				/// ⚠️ UN SEUL SITE POUR CETTE REGLE, ET C'EST TOUT LE POINT. Six roles
				///    de conteneur en ont besoin (`Window`, `Panel`, `VBox`, `HBox`,
				///    `Group`, `Stack`). Ecrire le calcul dans chacun aurait fait six
				///    verites sur « ou commence le frere suivant », et ce depot connait
				///    la suite : *le meme calcul a deux sites finit garde a un seul*.
				///    La garde qui manquait, ce jour-la, etait a dix lignes de l'autre.
				///
				/// ⚠️ ET ELLE PASSE PAR `NextItemRect`, PAS PAR UNE SOUSTRACTION A NOUS.
				///    C'est elle qui sait avancer le curseur en X dans une `HBox` et en
				///    Y ailleurs — le seul endroit du fichier qui connaisse l'axe du
				///    flux courant.
				static bool TailleEnFlux(NkGuiContext &ctx, const NkArchive &w,
										 NkRect &out) noexcept {
					if (NkGuiTailleFluxIgnoree())
						return false; // mutation de banc : le defaut, a la demande
					float32 sW = 0.f, sH = 0.f;
					if (!LireVec2(w, "size", sW, sH))
						return false;
					if (sW <= 0.f && sH <= 0.f)
						return false; // `size = (0, 0)` ne contraint rien
					out = ctx.NextItemRect(sW > 0.f ? sW : -1.f,
										   sH > 0.f ? sH : ctx.ItemHeight());
					return true;
				}

				/// Monte `corps` DANS le rectangle que `size` decoupe, et repose la mise
				/// en page du parent. Rend faux quand il n'y a pas de taille a honorer —
				/// l'appelant garde alors son chemin d'avant, inchange.
				///
				/// ⚠️ `BeginLayout` N'A PAS DE PILE : on sauve `ctx.layout` par valeur et
				///    on le repose, exactement comme NKGui le fait pour ses popups. Et la
				///    sauvegarde se prend APRES `TailleEnFlux`, donc apres que
				///    `NextItemRect` a avance le curseur : c'est ce qui fait que le frere
				///    suivant part SOUS ce conteneur et non par-dessus.
				template <typename F>
				static bool MonterDansTaille(NkGuiContext &ctx, const NkArchive &w,
											 const NkGuiPlacement &pl,
											 NkGuiMonteRapport &rap, NkRect &rOut,
											 F corps) noexcept {
					if (pl.pose)
						return false;
					NkRect r;
					if (!TailleEnFlux(ctx, w, r))
						return false;
					const NkGuiLayout sauve = ctx.layout;
					ctx.BeginLayout(r);
					corps();
					ctx.layout = sauve;
					++rap.conteneursTailleFlux;
					rOut = r;
					return true;
				}

				/// La region que prend un conteneur de premier plan : son `pos`/`size`
				/// s'il les ecrit, sinon ce qui reste de la region courante.
				///
				/// 🔴 SA BRANCHE `pos`+`size` EST MORTE, ET ELLE L'A TOUJOURS ETE. Elle
				///    n'est appelee que depuis `!pl.pose`, et `NkGuiLirePlacement` ne rend
				///    `pose` faux que s'il n'y a PAS de `pos` : le `LireVec2(w, "pos", …)`
				///    ci-dessous echoue donc a chaque fois. C'est ce qui faisait que
				///    `size` en flux n'etait lu par personne, et que deux `Panel` freres
				///    se repeignaient l'un l'autre. On la laisse — la retirer changerait
				///    le sens d'une fonction que six `case` appellent — mais on ecrit ici
				///    qu'elle ne rend, en pratique, QUE la region courante.
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

				/// Peint une image dans `r`, ou signale VISIBLEMENT qu'elle manque.
				///
				/// 🔴 LE MARQUEUR N'EST PAS UN ORNEMENT, ET SA COULEUR A DEJA ETE PAYEE.
				///    Le role `Host` a appris cette lecon en septembre : sa premiere
				///    version tracait son cadre en `theme.border`, le banc comptait
				///    3 778 pixels et passait au VERT alors que l'image ne montrait
				///    RIEN -- la bordure est a neuf de luminance de l'aplat du panneau.
				///    Un marqueur qu'aucun oeil ne voit ne signale rien. On reprend donc
				///    la porte unique : `textMuted` et les hachures, qui disent « rien
				///    n'est monte ici » et qu'aucun contenu d'application n'imite.
				///
				/// ⚠️ ELLE RESPECTE LES PROPORTIONS. Etirer une image pour remplir un
				///    rectangle est un defaut qui ne se voit pas sur un carre et saute
				///    aux yeux sur une photo -- donc qui passerait tous les bancs et
				///    aucune relecture. L'image est CONTENUE et centree.
				static void PeindreImage(NkGuiContext &ctx, const NkString &nom, const NkRect &r,
										 NkGuiMonteRapport &rap) noexcept {
					if (r.w <= 0.f || r.h <= 0.f)
						return;
					uint32 tex = 0;
					NkVec2 taille{0.f, 0.f};
					if (!NkGuiResoudreImage(nom.CStr(), tex, taille) || taille.x <= 0.f
						|| taille.y <= 0.f) {
						++rap.imagesNonResolues;
						++rap.attributsNonHonores;
						NkGuiDrawList &dl = ctx.DL();
						const NkColor trait = ctx.theme.textMuted;
						dl.AddRect(r, trait, 1.f);
						NkGuiHachurer(dl, r, trait);
						return;
					}
					// MUTATION DE BANC, NK_IMAGE_MUTATION=etire : l'image remplit son
					// rectangle sans egard aux proportions -- la version naive, celle
					// qu'on ecrit en premier. Elle existe pour que le critere C4 de
					// `--sonde-images` ait quelque chose a refuter : un critere
					// qu'aucune mutation ne fait rougir ne prouve rien. Sans la
					// variable, ce bloc ne change absolument rien.
					static const bool kEtire = []() {
						const char *v = getenv("NK_IMAGE_MUTATION");
						return v && v[0] == 'e';
					}();
					// « Contenir » : le plus petit des deux facteurs, jamais le plus grand.
					const float32 fx = r.w / taille.x;
					const float32 fy = r.h / taille.y;
					const float32 f = fx < fy ? fx : fy;
					const float32 w = kEtire ? r.w : taille.x * f;
					const float32 h = kEtire ? r.h : taille.y * f;
					const NkRect dst{r.x + (r.w - w) * 0.5f, r.y + (r.h - h) * 0.5f, w, h};
					ctx.DL().AddImage(tex, dst, NkVec2(0.f, 0.f), NkVec2(1.f, 1.f),
									  NkColor(255, 255, 255, 255));
					++rap.imagesPeintes;
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
				/// La grille d'une toile, au pas du MONDE, decoupee sur la zone.
				///
				/// ⚠️ LE PAS S'ADAPTE AU ZOOM, ET SANS CA LA GRILLE EST INUTILISABLE.
				///    Un pas fixe en unites de monde donne, a zoom 0,05, une bouillie de
				///    traits a un pixel d'intervalle -- et a zoom 40, une seule ligne a
				///    l'ecran. On monte donc par puissances de dix jusqu'a ce qu'un pas
				///    fasse au moins huit pixels : la grille garde toujours une densite
				///    lisible, quel que soit le cadrage.
				static void PeindreGrille(NkGuiContext &ctx, const NkRect &r,
										  const NkGuiVue &vue) noexcept {
					const float32 z = vue.zoom > 0.0001f ? vue.zoom : 1.f;
					float32 pas = 10.f;
					while (pas * z < 8.f)
						pas *= 10.f;
					while (pas * z > 160.f)
						pas *= 0.1f;
					NkGuiDrawList &dl = ctx.DL();
					const NkColor trait = ctx.theme.border;
					const NkVec2 hg = vue.VersMonde(NkVec2(r.x, r.y), r);
					const NkVec2 bd = vue.VersMonde(NkVec2(r.x + r.w, r.y + r.h), r);
					// On part du premier multiple du pas APRES le bord : sinon la grille
					// glisserait avec le cadrage au lieu d'etre ancree au monde.
					float32 x = (float32)((int64)(hg.x / pas) - 1) * pas;
					for (; x < bd.x; x += pas) {
						const float32 sx = vue.VersEcran(NkVec2(x, 0.f), r).x;
						if (sx >= r.x && sx <= r.x + r.w)
							dl.AddLine(NkVec2(sx, r.y), NkVec2(sx, r.y + r.h), trait, 1.f);
					}
					float32 y = (float32)((int64)(hg.y / pas) - 1) * pas;
					for (; y < bd.y; y += pas) {
						const float32 sy = vue.VersEcran(NkVec2(0.f, y), r).y;
						if (sy >= r.y && sy <= r.y + r.h)
							dl.AddLine(NkVec2(r.x, sy), NkVec2(r.x + r.w, sy), trait, 1.f);
					}
				}

				/// Remet a l'ecouteur de `id`, s'il y en a un, ce qui vient de lui arriver.
				///
				/// ⚠️ ELLE SORT TOUT DE SUITE QUAND PERSONNE N'ECOUTE, et ce n'est pas une
				///    micro-optimisation : `Exposer` est appelee pour CHAQUE widget de
				///    CHAQUE image. Sans ce test en tete, un document de mille widgets
				///    paierait mille recherches dans une table vide, soixante fois par
				///    seconde.
				///
				/// ⚠️ ELLE N'INVENTE PAS DE SOURCE POUR CE QUI N'EN A PAS. Seuls les
				///    evenements que `NkGuiInput` porte VRAIMENT partent d'ici : pointeur,
				///    molette, et le focus clavier. La manette, le tactile, la fenetre et
				///    les customs attendent leur acheminement -- et
				///    `NkGuiEvenementADesSources()` continue de le dire.
				static void Exposer(NkGuiContext &ctx, NkGuiMonteRapport &rap, const NkString &id,
									const NkRect &r) noexcept {
					NkGuiEcouteurs *tab = NkGuiEcouteursPoses();
					if (!tab || !tab->ADesEcouteurs() || id.Empty() || r.w <= 0.f || r.h <= 0.f)
						return;
					const NkGuiInput &in = ctx.input;
					const NkVec2 p = in.mousePos;
					const bool dedans = (p.x >= r.x && p.x <= r.x + r.w && p.y >= r.y
										 && p.y <= r.y + r.h);
					// ⚠️ LE SURVOL DE L'IMAGE PRECEDENTE VIENT DE LA TABLE, PAS DE LA
					//    SOURIS -- et la premiere version s'y est trompee. `NewFrame`
					//    consomme `mousePosPrec` puis l'ECRASE avec la position courante :
					//    apres lui, « la position de l'image d'avant » EST la position
					//    courante, donc `dedansAvant` valait toujours `dedans` et
					//    `Entre`/`Sorti` ne partaient jamais. Voir
					//    `NkGuiEcouteurs::EtaitSurvole`, ou la mesure est ecrite.
					const bool dedansAvant = tab->EtaitSurvole(id);
					tab->PoserSurvol(id, dedans);

					NkGuiEvtDetail e;
					e.zone = id;
					e.rect = r;
					e.ecran = p;
					e.local = NkVec2(p.x - r.x, p.y - r.y);
					e.ctrl = in.ctrlDown;
					e.maj = in.shiftDown;
					e.alt = in.altDown;
					e.survole = dedans;
					e.pointeur = NkGuiEvtDetail::Appareil::Souris;

					if (dedans && !dedansAvant) {
						e.type = NkGuiEvenement::PointerEnter;
						(void)tab->Servir(e);
					} else if (!dedans && dedansAvant) {
						// ⚠️ LA SORTIE SE REMET MEME SI LE POINTEUR EST LOIN. C'est le
						//    seul evenement qu'on envoie a une zone NON survolee : sans
						//    lui, un widget garderait son etat de survol pour toujours
						//    des que la souris en sort vite.
						e.type = NkGuiEvenement::PointerLeave;
						(void)tab->Servir(e);
						++rap.evenementsExposes;
						return;
					}
					if (!dedans)
						return;
					if (in.mouseDelta.x != 0.f || in.mouseDelta.y != 0.f) {
						e.type = NkGuiEvenement::PointerMove;
						e.delta = in.mouseDelta;
						(void)tab->Servir(e);
						e.delta = NkVec2(0.f, 0.f);
					}
					for (int32 b = 0; b < 3; ++b) {
						e.bouton = b;
						if (in.mouseClicked[b]) {
							e.type = NkGuiEvenement::PointerDown;
							(void)tab->Servir(e);
						}
						if (in.mouseReleased[b]) {
							e.type = NkGuiEvenement::PointerUp;
							(void)tab->Servir(e);
						}
						if (in.mouseDoubleClicked[b]) {
							e.type = NkGuiEvenement::DoubleClick;
							(void)tab->Servir(e);
						}
					}
					e.bouton = -1;
					// ⚠️ LA MOLETTE RESERVEE EST RESPECTEE. `ctx.input.wheel` vaut deja
					//    zero quand un menu ou un popup l'a reservee (regle du 05/09) : la
					//    lire telle quelle suffit, et c'est ce qui empeche une toile de
					//    zoomer « a travers » un menu ouvert.
					if (in.wheel != 0.f || in.wheelH != 0.f) {
						e.type = NkGuiEvenement::Wheel;
						e.delta = NkVec2(in.wheelH, in.wheel);
						(void)tab->Servir(e);
					}
					++rap.evenementsExposes;
				}

				/// Le releve d'un widget monte -- ET, depuis le 27/09, le seul endroit d'ou
				/// les evenements partent vers l'application.
				///
				/// 🔴 POURQUOI ICI, ET NULLE PART AILLEURS. Son commentaire d'origine le
				///    disait deja pour le debordement : « c'est le seul endroit du fichier
				///    par ou passent TOUS les widgets montes, avec le rectangle qu'ils ont
				///    REELLEMENT pris. Compter le debordement ailleurs aurait voulu dire le
				///    compter dans chaque `case`, c'est-a-dire en oublier. » Exposer les
				///    evenements a exactement le meme besoin : trente-sept `case` moins un,
				///    c'est un widget muet que personne ne remarquera.
				///
				/// ⚠️ `ctx` EST DEVENU SON PREMIER PARAMETRE, et les 37 appels ont ete
				///    changes d'un seul coup. Un parametre ajoute avec une valeur par
				///    defaut aurait compile en laissant la plupart des sites muets -- la
				///    forme exacte de « declarer n'est pas livrer ».
				static void Noter(NkGuiContext &ctx, NkGuiMonteRapport &rap, const NkString &id,
								  NkStringView role, const NkRect &r, uint32 prof, bool conteneur,
								  bool axeHorizontal, const NkRect *region = nullptr) noexcept {
					// ═══════════════════════════════════════════════════════
					//  🔴 L'IDENTIFIANT DU DOCUMENT DEVIENT LA CLE DU RELEVE
					// ═══════════════════════════════════════════════════════
					//  Mesure du 28/09 : une sonde qui cherchait
					//  « design.exporter » dans la barre d'outils trouvait SEPT
					//  boutons, tous avec une cle VIDE et un libelle VIDE (ils
					//  portent une icone, pas un texte). Aucun n'etait nommable :
					//  le releve ne montrait que sept rectangles anonymes.
					//
					//  Or la cle stable existe exactement pour ca --
					//  `NkGuiIntrospectCler` : « le libelle CHANGE [...] la cle
					//  stable de l'application ». Un document a DEJA un
					//  identifiant par widget ; il n'arrivait simplement jamais
					//  jusqu'au releve.
					//
					//  ⚠️ SANS ELLE, TOUTE SONDE QUI VISE UN WIDGET MONTE DOIT
					//     LE DEVINER PAR SON RANG. Un rang se decale des qu'on
					//     insere un separateur -- et le banc continue de passer,
					//     en mesurant le voisin.
					//  ⚠️ LES CONTENEURS SONT EXCLUS, ET LA MESURE L'A EXIGE.
					//     `NkGuiIntrospectCler` nomme LA DERNIERE NOTE POSEE
					//     (son contrat le dit). Un `HBox` ne pose aucune note
					//     lui-meme et se `Noter` APRES ses enfants : sa cle
					//     atterrissait donc sur le DERNIER bouton. Mesure :
					//     « bouton 7 : cle = "outils" » -- l'identifiant de la
					//     boite, colle a `design.exporter`. Un widget portait le
					//     nom de son parent, ce qui est pire que pas de nom.
					if (!conteneur && id.Size() > 0u)
						NkGuiIntrospectCler(ctx, id.CStr());
					Exposer(ctx, rap, id, r);
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
