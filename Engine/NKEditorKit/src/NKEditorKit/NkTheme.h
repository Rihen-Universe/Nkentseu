#pragma once
// -----------------------------------------------------------------------------
// @File    NkTheme.h
// @Brief   Systeme de themes : roles de couleur nommes, heritage, chargement.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// A QUOI CA SERT, ET POURQUOI CE N'EST PAS QU'UNE LISTE DE COULEURS
//   Regle posee dans UI_SPEC 10bis, et c'est elle qui gouverne tout le fichier :
//   les couleurs PORTEUSES DE SENS vivent DANS le theme -- axes X/Y/Z, etats de
//   selection, types d'assets compris. Laissees en dur dans le code, le theme
//   clair serait illisible : un vert d'axe pense pour un fond #141414 disparait
//   sur un fond #F5F5F5, et personne ne s'en apercevrait avant la capture d'ecran.
//
// EN-TETE PUR, ZERO DEPENDANCE — meme raison que NkShortcutTable : le harnais
//   doit pouvoir le tester sans lier NKUI, NKCanvas ni NKFont. Les couleurs sont
//   donc des uint32 empaquetes 0xRRGGBBAA, pas des nkgui::NkColor.
//
// ROLE = ENUM + NOM, et les deux servent
//   L'ENUM est l'acces du code : indexation directe d'un tableau, verifiee a la
//   compilation, sans recherche de chaine a chaque trait dessine.
//   Le NOM est l'acces du FICHIER : un theme utilisateur est un fichier texte
//   qu'on ecrit a la main. C'est exactement le couple deja retenu pour les
//   modificateurs (NkModifierType serialise + NkModParam::name) et les raccourcis.
//   L'enum est donc APPEND-ONLY : on ajoute a la fin, jamais au milieu.
//
// LA FONCTION QUI COMPTE VRAIMENT : L'HERITAGE
//   Un theme utilisateur ne redefinit presque jamais 40 couleurs. Il en change
//   trois et garde le reste. Un chargeur naif laisserait les 37 autres a zero,
//   c'est-a-dire NOIR — et le theme paraitrait « charge » tout en etant
//   inutilisable. Load() part donc TOUJOURS d'une base complete et n'ecrase que
//   ce que le fichier mentionne.
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace editorkit {

		// Couleur empaquetee 0xRRGGBBAA.
		using NkThemeColor = uint32;

		// ── ROLES ───────────────────────────────────────────────────────────────
		// APPEND-ONLY : ajouter a la FIN. Un role insere au milieu decalerait tous
		// les suivants et un theme enregistre relirait les mauvaises couleurs.
		enum class NkRole : uint16 {
			// Structure — la hierarchie a TROIS niveaux de UI_SPEC 10bis.1. Trois
			// valeurs se lisent sans effort ; un quatrieme gris rendrait les ecarts
			// indistinguables.
			WindowBg = 0,  ///< #141414 — le plus sombre, il recule derriere tout
			PanelBg,	   ///< #212121 — le panneau se detache du vide
			PanelHeader,   ///< #2B2B2B — en-tetes, barres d'outils : ce qui structure se lit d'abord
			Border,
			InputBg,	   ///< fond des champs de saisie
			LabelCol,	   ///< colonne de libelles des lignes de propriete

			// Texte
			Text,
			TextMuted,
			TextOnAccent,

			// LES DEUX FAMILLES D'ACCENT, et c'est une regle, pas une nuance :
			// le BLEU dit l'etat de l'INTERFACE, l'AMBRE dit la selection 3D.
			AccentUi,	 ///< #1177D1
			AccentSel,	 ///< #F2980E — orange UNIQUE du produit (cf. 10bis.2)

			// Les trois etats de selection d'un element de maillage. Ils DOIVENT
			// etre dans le theme : en clair, un « presque noir » d'element non
			// selectionne devient invisible.
			ElemActive,	  ///< l'element ACTIF — blanc pur en sombre
			ElemSelected, ///< selectionne
			ElemIdle,	  ///< ni l'un ni l'autre

			// Axes. Dans le theme pour la meme raison.
			AxisX,
			AxisY,
			AxisZ,

			// Types d'assets du navigateur de projet
			TypeMesh,
			TypeAnim,
			TypeMat,
			TypeTex,

			// Editeur de noeuds. Les deux sarcelles ne different que de 1 a 3 points
			// par canal : elles sont affectees a des etats qui NE COEXISTENT JAMAIS
			// (repos / survol du MEME en-tete), sinon l'utilisateur croirait a une
			// seule couleur qui « bave ». Cf. UI_SPEC 10bis.3.
			NodeDataHeader,	   ///< #0A545E — en-tete de noeud de donnees, au repos
			NodeDataHeaderHot, ///< #095461 — le MEME, survole ou selectionne
			NodeActionHeader,  ///< noeud d'action (ambre)
			NodeBody,
			NodeWire,

			// Vue 3D
			ViewportTop,
			ViewportBottom,
			GridLine,

			// Dossier. AJOUTE EN FIN, comme l'exige la regle append-only : l'inserer
			// pres des autres types aurait decale tous les roles suivants et un theme
			// deja enregistre aurait relu les mauvaises couleurs.
			// Il lui faut un role a lui : un dossier n'est pas un type d'asset, c'est
			// un CONTENANT, et il apparait a la fois dans la hierarchie et dans le
			// navigateur de projet -- les deux doivent parler la meme couleur.
			TypeFolder,

			// ⚠️ DEUX ROLES AJOUTES LE 2026-08-29, SUR UNE INSUFFISANCE DEMONTREE
			//    DU VOCABULAIRE -- pas sur une preference de conception.
			//
			//    La mesure : dans `NkEditorShell::Init`, `button` vaut #191D23 et
			//    `track` vaut #0D1117. **Ils different.** Or la conversion tire les
			//    deux du meme role `InputBg` : un seul role ne peut pas rendre deux
			//    couleurs. Meme histoire pour la barre d'onglets, #191D23 ici et
			//    `WindowBg` (#0D1117) dans la conversion. Tant que ces roles
			//    manquaient, la palette de la coquille etait **inexprimable** dans
			//    le vocabulaire des themes -- et c'est pour ca qu'elle etait restee
			//    recopiee a la main.
			//
			//    AJOUTES EN FIN, comme l'exige la regle append-only ci-dessus.
			//    (La serialisation, elle, se fait par NOM -- `button_bg = RRGGBBAA`
			//    -- donc un theme deja enregistre ne connait simplement pas ces deux
			//    lignes, et c'est exactement le cas que le repli ci-dessous traite.)

			/// Fond d'un BOUTON. Distinct du fond d'un CHAMP (`InputBg`) : un bouton
			/// se pose SUR une surface, un champ se creuse DEDANS.
			/// ⚠️ `NkThemeNonDefini` par defaut -> repli sur `InputBg`.
			ButtonBg,

			/// Fond de la BARRE d'onglets, distinct du fond de fenetre (`WindowBg`)
			/// et de l'onglet lui-meme (`PanelHeader`).
			/// ⚠️ `NkThemeNonDefini` par defaut -> repli sur `WindowBg`.
			TabBarBg,

			// ⚠️ SIX ROLES AJOUTES LE 2026-08-31 (reference Banani, doc 11 §3) --
			//    APPEND-ONLY, discipline ButtonBg/TabBarBg : NkThemeNonDefini par
			//    defaut, repli sur un role source (GetOuRepli). Un theme
			//    enregistre avant eux reste valide, rien ne devient magenta, et
			//    AUCUNE conversion existante ne les lit : le rendu d'avant ne
			//    peut pas bouger.

			/// Fond de la TOILE de design. ⚠️ IL SUIT LE THEME (test de Rodolf,
			/// 31/08 : « cette couleur blanche c'est pour le theme light ; en
			/// Design il faut la meme couleur de fond que pour Behavior et les
			/// autres ») : sombre en theme sombre (#0d1117, le fond de la vue
			/// Behavior), clair en theme clair (#f5f7fb, la valeur Banani V2).
			/// L'ancienne doctrine « toile claire meme en editeur sombre » etait
			/// la generalisation abusive d'UN ecran de la maquette. Repli : PanelBg.
			CanvasBg,
			/// Les points de la grille de toile — suivent le theme comme le fond
			/// (clair : #d4dce8, pas 20 ; sombre : la bordure #30363d). Repli : Border.
			CanvasDot,
			/// Vert d'etat : « Pret », simulation, modele LOCAL. Repli : AccentUi.
			StatusOk,
			/// Rouge d'etat : erreurs console, rejets, fermer. Repli : AccentSel.
			StatusErr,
			/// Violet de TOUT ce qui est IA (etoile, badges, bande modele) --
			/// la maquette est systematique la-dessus. Repli : AccentUi.
			AccentAI,
			/// Lignes de magnetisme de la toile (#ff4fd8). Repli : AccentSel.
			SnapLine,
			/// Fond d'un ARTBOARD pose sur la toile (Banani V2 : cadre BLANC —
			/// le document se concoit clair, quel que soit le fond de toile, qui
			/// lui suit le theme depuis le 31/08). Repli : CanvasBg.
			ArtboardBg,
			/// Texte d'un element DU DOCUMENT (dessine sur ArtboardBg) — sombre,
			/// parce que le document se concoit clair, quel que soit le theme de
			/// l'editeur. PROVISOIRE jusqu'au vocabulaire d'apparence (§8ter) :
			/// le jour ou un element porte sa couleur, elle prime. Repli : Text.
			DocText,
			/// Fond d'un rectangle/champ DU DOCUMENT (les champs du formulaire de
			/// la maquette : gris clair sur cadre blanc). Meme statut provisoire.
			/// Repli : InputBg.
			DocFieldBg,
			/// Texte SECONDAIRE du document (etiquette d'artboard « Connexion —
			/// Mobile 390 x 844 », libelles de champs : #656d76 dans la maquette,
			/// un gris de TOILE CLAIRE distinct du TextMuted de l'editeur).
			/// Repli : DocText. Ajout 31/08, meme regime facultatif-avec-repli.
			DocMuted,

			// ⚠️ UN SEUL ROLE AJOUTE LE 2026-09-14 — L'AVERTISSEMENT. Et UN seul,
			//    pas trois : le compte a ete verifie AVANT d'etre livre.
			//
			//    LA MESURE QUI L'IMPOSE — trois sites qui empruntaient faute de
			//    mieux, dans trois applications differentes :
			//      NkModelerToast.h:182    `NkToastKind::Partiel` -> `AccentSel`
			//      NkModelerJournal.h:385  `NkJnvNiveau::Warn`    -> `AccentUi`
			//      NkcLabTheme.h:86        `NkcPalette::Warn()`   -> #D29922 EN DUR
			//    Le commentaire de NkModelerJournal.h portait deja sa condition de
			//    retrait : « Le jour ou ces roles existeront, c'est ici qu'il
			//    faudra changer -- et nulle part ailleurs. » C'est ce jour.
			//
			//    POURQUOI PAS TROIS. Le succes et l'erreur EXISTAIENT DEJA
			//    (`StatusOk` #3FB950, `StatusErr` #F85149, poses le 31/08) : il ne
			//    manquait que le troisieme membre de la triade, et ConquerorLab
			//    l'avait deja ecrit en dur sous le nom EXACT de GitHub —
			//    `attention.fg` #D29922, a cote de `Ok()` #3FB950 et `Error()`
			//    #F85149 qui sont, elles, les deux valeurs du theme. Quant a
			//    « Danger » et « Error » : AUCUN comportement ne les distingue
			//    dans ce depot -- le journal peint `Erreur` ET `Fatal` de la meme
			//    couleur (`nv >= Erreur`), et le seul autre vocabulaire de
			//    gravite, `NkToastKind`, n'a que trois membres. Deux noms pour la
			//    meme couleur ne font pas deux familles : on n'en livre qu'un.
			//
			//    ⚠️ IL N'EST PAS `AccentSel`, ET IL NE DOIT PAS L'ETRE. Ecart
			//       mesure entre l'ambre d'alerte et l'ambre de selection 3D :
			//       dE76 = 16,8 en sombre (#D29922 / #F2980E), 17,2 en clair
			//       (#9A6700 / #C97A08). Il FAUT cet ecart : une pastille
			//       d'avertissement et un element 3D selectionne COEXISTENT a
			//       l'ecran. La tolerance accordee aux deux sarcelles (10bis.3) ne
			//       vaut que pour des etats qui NE COEXISTENT JAMAIS ; ce n'est
			//       pas le cas ici, donc elle ne s'applique pas.

			/// Ambre d'ALERTE : « attention », « partiel », « ca a marche mais pas
			/// entierement ». Distinct de l'ERREUR (qui, elle, n'a PAS eu lieu) et
			/// de l'ambre de selection 3D (`AccentSel`). ⚠️ Repli : `AccentSel` --
			/// l'approximation la moins fausse, et exactement ce que les sites
			/// emprunteurs faisaient deja.
			StatusWarn,

			// ⚠️ TROIS ROLES AJOUTES LE 2026-09-20 — LES SURFACES DU PANNEAU IA.
			//    Et TROIS, pas quatre : j'en avais annonce quatre au chiffrage, la
			//    mesure en a refuse un. Le filet vertical du fil sort a #33363D
			//    dans la capture, contre `Border` #35383D -- deux unites d'ecart
			//    par canal. Un role neuf pour deux unites serait un doublon qui se
			//    mettrait a diverger ; le rail prend `Border` et n'a pas de role.
			//    *Un attendu qui n'a pas le pouvoir de dire non ne mesure rien* :
			//    celui-la l'avait, et il a dit non.
			//
			//    D'OU VIENNENT LES VALEURS. Capture de reference du 20/09
			//    (`Screenshot 2026-09-20 111040.png`, 695x1292), echantillonnee
			//    par couleur DOMINANTE d'une zone, jamais par un pixel isole.
			//    Elle est en GitHub Dark Pro -- c'est-a-dire NOTRE theme : son
			//    fond #030409 est notre `WindowBg` #010409, sa surface #0F1117
			//    notre `PanelBg` #0D1117, son filet #35383D notre `Border`
			//    #30363D. Les valeurs ci-dessous ne sont donc pas importees d'un
			//    autre produit : elles retombent dans la famille qu'on a deja.
			//
			//    ⚠️ POURQUOI CES TROIS-LA NE PEUVENT PAS ETRE DES REUTILISATIONS,
			//       ET C'EST LE THEME CLAIR QUI TRANCHE, PAS LE SOMBRE. En sombre,
			//       `CodeBg` vaut la meme chose que `PanelHeader` : on pourrait
			//       croire au doublon. En clair, `PanelBg` est #FFFFFF -- un pave
			//       de code pose dessus SANS role propre serait blanc sur blanc,
			//       invisible. C'est exactement l'argument d'ouverture de ce
			//       fichier (« le theme clair serait illisible »), applique a la
			//       lettre.

			/// Fond d'un pave de code a chasse fixe, et du compartiment d'ENTREE
			/// (`IN`) d'une etape d'outil. Un cran AU-DESSUS du fond du panneau :
			/// le pave se detache sans dependre de son filet. Repli : `LabelCol`
			/// -- l'autre surface « un cran au-dessus » du theme, juste dans les
			/// deux (et non `InputBg`, qui vaut #FFFFFF en clair et redonnerait
			/// le blanc sur blanc).
			CodeBg,
			/// Fond du compartiment de SORTIE (`OUT`), distinctement plus clair
			/// que l'entree : c'est ce qui separe ce que l'outil a RECU de ce
			/// qu'il a RENDU, sans ajouter un titre ni un filet.
			///
			/// ⚠️ MESURE, PARCE QUE J'AI FAILLI ME TROMPER. Sur un seul bloc,
			///    #373942 pouvait etre un SURLIGNAGE DE SELECTION -- et en faire
			///    un role aurait grave dans le theme un accident de capture.
			///    Attendu ecrit d'abord : une surface couvre la marge DROITE du
			///    compartiment, la ou aucun glyphe ne se peint, et elle le fait
			///    sur TOUS les blocs. Mesure sur les QUATRE blocs de la capture :
			///    #373942 dans les quatre marges, #0F1117 dans les quatre `IN`
			///    correspondants. Quatre blocs independants ne sont pas selectionnes
			///    de la meme facon : c'est une surface. Repli : `CodeBg`.
			CodeOutBg,
			/// Fond d'une pastille de code EN LIGNE dans une phrase de prose
			/// (`durcir`, `lisser` dans la capture) : une surface courte, posee sur
			/// la ligne de texte, pas un pave. Repli : `Border` -- la valeur la
			/// plus proche dans les deux themes, et un repli reste une
			/// approximation nommee, pas un synonyme.
			InlineCodeBg,

			/// 🔴 LE ROLE AJOUTE LE 2026-09-26 — « PREVU, PAS DISPONIBLE ».
			///
			/// Texte d'un element d'interface DESACTIVE : une entree de menu dont
			/// le panneau n'existe pas encore, un bouton qu'un etat rend inerte.
			/// Il ne dit pas « secondaire » (c'est `TextMuted`) : il dit « on ne
			/// peut pas agir dessus ». Deux messages differents demandent deux
			/// roles -- reutiliser `TextMuted` reviendrait a dire qu'un libelle
			/// secondaire et un bouton mort se ressemblent.
			///
			/// ⚠️ POURQUOI IL MANQUAIT, ET POURQUOI CE N'EST PAS UNE FONCTION NEUVE
			///    La table de theme de NKGui porte `textDisabled` depuis toujours
			///    (`NkGuiContext.h` : {120,124,134,255}), et `NkCtxMenu` du kit
			///    l'emploie deja pour griser ses entrees. Le vocabulaire de ROLES,
			///    lui, ne l'avait pas : une application qui peint par roles -- donc
			///    NK3DModeler, Nogee, NkAnimaEditor -- n'avait aucun moyen de dire
			///    « desactive » et se rabattait sur `TextMuted`. On complete un
			///    vocabulaire qui a deja son equivalent ailleurs.
			///    Precedent exact : *« Theme : un seul role manquait »* (31/08) --
			///    deux statuts existaient, le troisieme manquait, et les statuts
			///    s'effacaient en theme clair.
			///
			/// ⚠️ AJOUTE EN FIN D'ENUMERATION, ET C'EST OBLIGATOIRE. La table
			///    `RoleNames()` est POSITIONNELLE et la cle est un CONTRAT DE
			///    FICHIER (son en-tete le dit). L'inserer apres `TextMuted` aurait
			///    decale toutes les cles suivantes : chaque theme deja enregistre
			///    sur disque aurait relu ses couleurs sous les mauvais noms.
			///
			/// ⚠️ IL N'EST PAS DANS LA TABLE DE CONTRASTE A 4,5, ET C'EST UN CHOIX
			///    MESURE, PAS UN OUBLI. WCAG exempte explicitement les elements
			///    DESACTIVES de son seuil de texte : un gris qui atteint 4,5 ne se
			///    lit plus comme desactive, il se lit comme du texte normal -- le
			///    seuil detruirait la fonction du role. Ce qui est garde, et
			///    eprouve au banc (famille 29), c'est un PLANCHER DE VISIBILITE et
			///    un ORDRE : plus attenue que `TextMuted`, jamais confondu avec le
			///    fond.
			/// Repli : `TextMuted` -- une approximation nommee, pas un synonyme.
			TextDisabled,

			Count
		};

		// Cle STABLE du role, telle qu'elle apparait dans un fichier de theme.
		const char *NkRoleName(NkRole r);
		// Role correspondant a une cle, ou Count si inconnue.
		NkRole NkRoleFromName(const char *name);

		// ── LA TABLE DE REPLI — UNE SEULE, ET C'EST LA SOURCE DE VERITE ─────────
		// Elle existait deja, mais en PROSE : chaque role facultatif porte dans son
		// commentaire « Repli : X ». Un commentaire ne s'execute pas, et c'est
		// exactement ce qui a laisse vivre le repli muet : la regle etait ecrite et
		// personne ne l'appliquait. Elle est desormais du code, et les commentaires
		// ci-dessus la decrivent au lieu de la remplacer.
		//
		/// Role SOURCE dont `r` prend la couleur s'il n'a jamais ete pose.
		/// `NkRole::Count` = AUCUN repli : ce role est obligatoire, son absence est
		/// un oubli, et c'est le magenta du constructeur qui doit le dire.
		NkRole NkRoleRepli(NkRole r);

		// Arrondis. Banani les exporte, la specification s'y refere : ils font
		// partie du theme au meme titre que les couleurs.
		enum class NkRadius : uint8 { Sm = 0, Md, Lg, Xl, Count };

		static const uint16 NK_ROLE_INVALID = 0xFFFFu;

		// ── ROLES D'APPLICATION ─────────────────────────────────────────────────
		// LE GARDE-FOU N.1 DE NKGRAPH, APPLIQUE ICI. NK3DModeler aura besoin d'un
		// « anneau de brosse », NkAnima d'une « cle d'animation », Nogee d'autre
		// chose encore. Les mettre dans l'enumeration COMMUNE ferait trainer a
		// Nogee des roles de sculpt qui ne le concernent pas -- et l'enumeration
		// grossirait a chaque application.
		//
		// Meme reponse que pour les types de sockets de NKGraph : l'APPLICATION
		// ENREGISTRE ses roles au demarrage et recoit un identifiant. Les roles du
		// coeur restent indexes par enumeration (rapides, verifies a la
		// compilation) ; ceux des applications vivent dans une table d'extension.
		//
		// Consequence recherchee : un fichier de theme ecrit pour NK3DModeler se
		// charge SANS ERREUR dans Nogee -- ses roles inconnus sont comptes, pas
		// rejetes. Convention de nommage : « nk3d.anneau_brosse ».
		class NkRoleRegistry {
			public:
				// Idempotent : deux appels avec le meme nom rendent le meme
				// identifiant. Une application peut donc enregistrer ses roles sans
				// se demander si une autre l'a deja fait.
				//
				// ⚠️ IL COMPARE OCTET POUR OCTET, sans canoniser, et c'est voulu :
				//    il ENREGISTRE un nom, il ne le resout pas. Canoniser ici
				//    changerait ce qu'une application a demande (« MonRole »
				//    deviendrait « mon_role ») et polluerait l'audit de `Find` d'une
				//    faute imaginaire a chaque premier enregistrement.
				static uint16 Register(const char *name);

				// LA RESOLUTION. Trois tentatives, dans cet ordre, et l'ordre n'est
				// pas indifferent :
				//   1. le nom BRUT, coeur puis extensions ;
				//   2. sa forme CANONISEE (`NkCanonicalRoleName`), meme parcours ;
				//   3. echec -> compte et nomme dans `NkRoleAudit`, rend
				//      NK_ROLE_INVALID.
				//
				// ⚠️ LA CANONISATION VIENT APRES LE NOM BRUT, JAMAIS AVANT. Une
				//    application enregistre ses propres roles sous n'importe quelle
				//    graphie ; canoniser d'abord ferait manquer un role d'extension
				//    legitimement nomme « MonRole ». Dans cet ordre, la canonisation
				//    ne peut QUE rattraper — elle ne casse aucun nom qui resolvait
				//    deja. Le banc NKEditorKitTest tient ce point par deux controles
				//    negatifs (essais 1h et 1i).
				static uint16 Find(const char *name); ///< NK_ROLE_INVALID si absent
				static const char *Name(uint16 id);
				static uint16 Total(); ///< roles du coeur + extensions enregistrees
		};

		// ── LA CANONISATION D'UN NOM DE ROLE ────────────────────────────────────
		// PascalCase / camelCase -> snake_case. PURE : aucune allocation, aucun
		// etat, aucune dependance au theme. Elle est publique parce qu'un outil
		// (editeur de theme, verificateur de declaration) doit pouvoir dire a
		// l'auteur quelle forme SON nom aurait prise.
		//
		// REGLE, en une phrase : on insere un « _ » devant toute MAJUSCULE qui
		// suit une minuscule ou un chiffre, puis on met tout en minuscules.
		//
		//   PanelBg      -> panel_bg          TextOnAccent -> text_on_accent
		//   PanelHeader  -> panel_header      AccentUi     -> accent_ui
		//   TypeFolder   -> type_folder       panel_bg     -> panel_bg  (inchange)
		//   nk3d.AnneauBrosse -> nk3d.anneau_brosse
		//
		// ⚠️ SA LIMITE, ECRITE AVEC ELLE PLUTOT QUE DECOUVERTE PLUS TARD : un
		//    ACRONYME colle ne se coupe pas. « NKThing » donne « nkthing », pas
		//    « nk_thing » -- la regle ne peut pas savoir ou finit l'acronyme.
		//    Aucun des roles du coeur n'est dans ce cas ; le jour ou l'un le sera,
		//    c'est le REPLI FRANC (NkRoleAudit) qui le dira, pas le magenta.
		//
		// Rend `false` si l'entree est nulle, vide, ou si le resultat ne tient pas
		// dans `cap` — une troncature silencieuse fabriquerait un nom qui ne resout
		// pas et DEPLACERAIT le defaut au lieu de le signaler.
		bool NkCanonicalRoleName(const char *in, char *out, uint32 cap);

		// ── LE REPLI FRANC ──────────────────────────────────────────────────────
		// CE QUE LE MAGENTA NE DISAIT PAS : *quel* role. Une couleur criarde dit
		// qu'il y a un probleme ; elle ne dit ni lequel, ni combien, ni ou. Le
		// 18/08, NkUIDesign a peint un panneau entier en magenta pendant que sa
		// sonde annoncait 72/72 — personne ne pouvait nommer le role fautif.
		//
		// ⚠️ IL RETIENT AUSSI LES RATTRAPAGES (les noms que la canonisation a
		//    sauves), et ce n'est PAS de la decoration : c'est la LISTE DE TRAVAIL
		//    de la correction a la source. Sans elle, la canonisation rendrait les
		//    declarations fausses invisibles — l'ecran serait juste, personne ne
		//    corrigerait rien, et le jour ou la regle de canonisation bougerait,
		//    tous les jetons casseraient d'un coup. C'est exactement « une
		//    protection qui empeche d'aller verifier », et la seule facon de ne pas
		//    la subir est de PUBLIER ce qu'elle a masque.
		//
		// ⚠️ POURQUOI UN PUITS PLUTOT QU'UN APPEL A NKLogger : l'en-tete declare
		//    ZERO DEPENDANCE, et cette propriete porte (le banc NKEditorKitTest en
		//    vit). Le puits par defaut n'est donc pas NUL — il ecrit sur `stderr`.
		//    Un puits par defaut nul aurait fait de ce repli « un parametre qui
		//    n'est pas honore » : present dans la signature, muet en pratique.
		//    Une application qui a un vrai journal remplace le puits par le sien.
		using NkRoleAuditSink = void (*)(void *user, const char *line);

		class NkRoleAudit {
			public:
				struct Entry {
						NkString name;  ///< le nom tel qu'il est DECLARE
						NkString canon; ///< la forme qui a resolu, vide si aucune
				};

				/// Un nom qui n'a resolu sous AUCUNE forme. C'est le magenta.
				static NkVector<Entry> &Faults();
				/// Un nom qui n'a resolu qu'APRES canonisation : l'ecran est juste,
				/// mais la declaration est a corriger a la source.
				static NkVector<Entry> &Rescued();

				/// ⚠️ TROISIEME LISTE, ajoutee le 2026-09-14 avec le repli de
				///    COULEUR. Elle ne se fond PAS dans `Rescued` : celle-la parle
				///    d'un NOM mal ecrit dans un composant, celle-ci d'une COULEUR
				///    absente du theme charge. Les deux se corrigent a des endroits
				///    differents -- l'une dans la declaration du composant, l'autre
				///    dans le fichier de theme -- et une liste qui melange les deux
				///    ne dit plus quoi faire.
				///    `name` = le role qui manquait, `canon` = celui dont il a pris
				///    la couleur.
				static NkVector<Entry> &Replis();

				static void Reset();
				static uint32 FaultCount();
				static uint32 RescuedCount();
				static uint32 RepliCount();

				/// Annonce un repli de COULEUR. Meme deduplication, meme puits, et
				/// une phrase a lui : « role X absent du theme -> peint avec Y ».
				static void NoteRepli(const char *role, const char *repli);

				/// Remplace le puits. `fn == nullptr` fait taire la sortie — reserve
				/// aux bancs qui veulent lire les listes sans polluer leur console.
				static void SetSink(NkRoleAuditSink fn, void *user);

				/// Une ligne lisible par un humain, pour le journal et pour un
				/// bandeau d'interface. BORNEE : un bandeau qui deborde ne se lit
				/// pas, et une liste de 23 noms n'apprend rien de plus que les cinq
				/// premiers suivis du compte.
				static void Summary(NkString &out, uint32 maxNames = 5);

				/// Ajout DEDUPLIQUE. Le dessin passe par la resolution a CHAQUE
				/// image : sans deduplication, la liste grossirait de N entrees par
				/// image et la fuite se presenterait comme un ralentissement, jamais
				/// comme un defaut de roles.
				static void Note(NkVector<Entry> &into, const char *name, const char *canon);
		};

		// Resout un nom vers un identifiant, coeur PUIS extensions, PUIS forme
		// canonisee. Un nom qui n'aboutit sous aucune forme est compte et nomme
		// dans NkRoleAudit avant que NK_ROLE_INVALID ne soit rendu.
		uint16 NkResolveRole(const char *name);

		// Une paire qui n'atteint pas le contraste exige pour son usage.
		struct NkThemeIssue {
				NkRole fg = NkRole::Count;
				NkRole bg = NkRole::Count;
				float32 ratio = 0.f;
				float32 required = 0.f;
				bool isText = false;
		};

		/// ⚠️ « CE ROLE N'A JAMAIS ETE POSE » -- et le zero est choisi contre le
		///    magenta a dessein. Le constructeur peint tous les roles en magenta
		///    criard pour qu'un oubli SAUTE AUX YEUX ; c'est une bonne regle, et
		///    elle interdit d'utiliser le magenta comme sentinelle : on ne
		///    distinguerait plus « oublie » de « pas encore invente ». Un alpha
		///    NUL, lui, n'est la valeur legitime d'aucun fond -- une surface
		///    entierement transparente ne se peint pas. La sentinelle est donc
		///    lisible sans ambiguite, et un role neuf qui la porte se replie au
		///    lieu de crier.
		static constexpr NkThemeColor NkThemeNonDefini = 0x00000000u;

		class NkTheme {
			public:
				NkTheme(); ///< construit le theme SOMBRE par defaut

				static NkTheme Dark();
				static NkTheme Light();

				// ══ LE REPLI EST DANS `Get`, PAS AU SITE D'APPEL (2026-09-14) ══
				//
				// CE QUI ETAIT LA AVANT, et pourquoi ca ne pouvait pas tenir. Le
				// fichier portait DEUX politiques qui s'excluent :
				//   (A) `GetOuRepli(role, repli)` -- le repli NOMME au site d'appel ;
				//   (B) rien du tout -- `Get` rendait la sentinelle telle quelle,
				//       c'est-a-dire 0x00000000, un noir ENTIEREMENT TRANSPARENT.
				//       Le role ne criait pas : il DISPARAISSAIT.
				//
				// LE COMPTE QUI TRANCHE, pris sur l'arbre le 14/09 :
				//   politique (A) : 2 sites la portent (NkThemeToGui.h:134 et :141,
				//                   `ButtonBg` et `TabBarBg`) ;
				//   politique (B) : 51 sites lisent les autres roles a sentinelle --
				//                   5 par l'enumeration (`StatusOk`, `StatusErr`,
				//                   `AccentAI`, `DocText`, `DocMuted`) et 46 PAR NOM
				//                   via `NkResolveRole` + `Get(uint16)`
				//                   (`doc_field_bg` 17, `doc_text` 13, `artboard_bg`
				//                   5, `snap_line` 4, `doc_muted` 4, `canvas_bg` 2,
				//                   `canvas_dot` 1).
				//
				// ⚠️ ET LE CHIFFRE N'EST MEME PAS LE PLUS FORT : la politique (A)
				//    EST INAPPLICABLE a ces 46 sites. Ils passent par `Get(uint16)`,
				//    qui n'a pas -- et ne peut pas avoir -- de surcharge
				//    « OuRepli » : a ce niveau le role n'est qu'un entier resolu
				//    depuis une chaine, l'appelant ne connait pas le role source et
				//    n'a aucun moyen de le nommer. Une politique qui ne peut pas
				//    s'appliquer a 90 % des sites n'est pas la politique du fichier.
				//
				// LA REGLE RETENUE, une seule, exercable par un banc : le repli est
				// une propriete DU ROLE (`NkRoleRepli`), pas du site d'appel. Toute
				// lecture -- par enumeration ou par identifiant -- le traverse, et
				// CHAQUE repli traverse est ANNONCE une fois dans
				// `NkRoleAudit::Replis()`. Un repli muet n'existe plus.
				//
				// `GetOuRepli` RESTE, et n'est pas un doublon : il permet a un
				// appelant d'imposer un AUTRE repli que celui du role. Ses deux
				// sites gardent exactement le comportement qu'ils avaient.
				NkThemeColor Get(NkRole r) const {
					if ((uint16)r >= (uint16)NkRole::Count)
						return 0xFF00FFFFu;
					const NkThemeColor c = mColors[(uint16)r];
					// Chemin chaud : une comparaison 32 bits. Tout le reste est
					// hors ligne, dans `Replier`, et ne s'execute que si le role
					// n'a effectivement jamais ete pose.
					return c != NkThemeNonDefini ? c : Replier(r);
				}

				/// Suit la chaine de repli de `r` ET L'ANNONCE. Hors ligne a
				/// dessein : `Get` est appele des milliers de fois par image.
				NkThemeColor Replier(NkRole r) const;

				/// Lit `r`, et retombe sur `repli` si `r` n'a jamais ete defini.
				///
				/// ⚠️ DEPUIS LE 14/09 `Get(r)` REPLIE DEJA. Cette fonction ne sert
				///    donc plus qu'a IMPOSER un repli different de celui du role --
				///    c'est le cas de ses deux appelants, qui nomment le meme repli
				///    que la table et gardent donc, au bit pres, leur rendu d'avant.
				///
				/// ⚠️ C'EST LE MECANISME QUI REND UN ROLE NEUF GRATUIT. Ajouter un
				///    role a une enumeration ne coute rien ; lui donner une valeur
				///    par defaut, si. Une constante fixe serait fausse pour tout
				///    theme qui a change la couleur d'a cote -- `GitHubDarkPro`
				///    part de `Dark()` puis remplace `InputBg`, donc un `ButtonBg`
				///    fige a la valeur de `Dark()` aurait diverge dans CE theme-la
				///    seulement, c'est-a-dire de la pire facon : une fois sur deux.
				///    Ici le role neuf **suit** son role source tant que personne ne
				///    l'a pose. Aucun theme existant ne change d'apparence, et un
				///    theme qui veut la distinction l'ecrit.
				NkThemeColor GetOuRepli(NkRole r, NkRole repli) const {
					// ⚠️ `GetBrut` et NON `Get` : depuis que `Get` replie tout seul,
					//    l'ancienne ecriture aurait rendu la comparaison morte et le
					//    repli DU ROLE aurait silencieusement prime sur celui que
					//    l'appelant nomme -- le contraire de ce que cette fonction
					//    promet. La sentinelle doit etre vue BRUTE ici.
					const NkThemeColor c = GetBrut(r);
					return c == NkThemeNonDefini ? Get(repli) : c;
				}

				/// La valeur POSEE, sentinelle comprise, sans aucun repli. Reservee
				/// a qui doit repondre « ce role a-t-il ete pose ? » -- `GetOuRepli`
				/// et les bancs. Ce n'est PAS l'accesseur de dessin.
				NkThemeColor GetBrut(NkRole r) const {
					return (uint16)r < (uint16)NkRole::Count ? mColors[(uint16)r] : 0xFF00FFFFu;
				}
				void Set(NkRole r, NkThemeColor c) {
					if ((uint16)r < (uint16)NkRole::Count)
						mColors[(uint16)r] = c;
				}

				// Acces par identifiant : coeur SI id < NkRole::Count, extension
				// au-dela. Pas d'ambiguite avec les surcharges ci-dessus, `NkRole`
				// etant une enumeration a portee (aucune conversion implicite).
				NkThemeColor Get(uint16 id) const;
				void Set(uint16 id, NkThemeColor c);
				float32 Radius(NkRadius k) const {
					return (uint8)k < (uint8)NkRadius::Count ? mRadius[(uint8)k] : 0.f;
				}

				const NkString &Name() const {
					return mName;
				}
				void SetName(const char *n) {
					mName = NkString(n ? n : "");
				}
				bool IsDark() const {
					return mDark;
				}

				// ── FICHIER ─────────────────────────────────────────────────────
				// Format texte, une ligne « role = #RRGGBB[AA] ». Un theme se lit,
				// se compare avec git diff et s'ecrit a la main -- c'est la condition
				// pour que l'utilisateur puisse creer le sien, qui est la demande.
				void Save(NkString &out) const;

				// CHARGE EN HERITANT : `*this` doit deja contenir une base complete
				// (Dark() ou Light()), et seules les lignes presentes l'ecrasent.
				// C'est ce qui permet a un theme de trois lignes de fonctionner.
				// outUnknown compte les roles non reconnus : un theme ecrit pour une
				// version plus recente doit se charger quand meme, en le SIGNALANT
				// plutot qu'en echouant.
				bool Load(const char *text, uint32 *outUnknown = nullptr, uint32 *outApplied = nullptr);

				// ── VALIDATION ──────────────────────────────────────────────────
				// L'utilisateur pouvant fabriquer ses themes, il en fabriquera un
				// illisible : mieux vaut le lui dire que le laisser decouvrir.
				//
				// LE SEUIL N'EST PAS UNIQUE, et le croire etait une erreur que la
				// premiere mesure a revelee. WCAG demande 4,5 pour du TEXTE, mais
				// seulement 3,0 pour un element GRAPHIQUE -- un contour de selection
				// de trois pixels n'a pas a se lire comme un paragraphe. Un seuil
				// unique a 4,5 condamnerait des paires parfaitement lisibles ; un
				// seuil unique a 3,0 laisserait passer du texte trop faible.
				// CE QUE LA VALIDATION NE COUVRE PAS, et il faut le savoir pour ne pas
				// s'y fier a tort : le rapport de contraste est une mesure de
				// LUMINANCE. Il dit si un trait se detache de son FOND ; il ne dit
				// rien de la capacite a distinguer DEUX MARQUES COLOREES l'une de
				// l'autre, qui est une affaire de TEINTE. Blanc et ambre sortent a
				// 2,27 alors que personne ne les confond. On ne verifie donc que ce
				// que la mesure sait juger.
				uint32 Validate(NkThemeIssue *outWorst = nullptr) const;

				// Rapport de contraste le plus faible, tous seuils confondus. Utile
				// comme chiffre brut ; c'est Validate() qui juge.
				float32 WorstContrast(NkRole *outA = nullptr, NkRole *outB = nullptr) const;

				static float32 Contrast(NkThemeColor a, NkThemeColor b);
				static NkThemeColor FromHex(const char *hex); ///< « #RRGGBB » ou « #RRGGBBAA »
				static void ToHex(NkThemeColor c, char out[10]);

			private:
				NkThemeColor mColors[(uint16)NkRole::Count] = {};
				// Roles d'application. Peut etre plus COURTE que le registre : une
				// application peut enregistrer un role apres qu'un theme a ete
				// construit. Get() rend alors le magenta de « role oublie » plutot
				// que de lire hors des bornes.
				NkVector<NkThemeColor> mExt;
				float32 mRadius[(uint8)NkRadius::Count] = {2.f, 3.f, 4.f, 8.f};
				NkString mName;
				bool mDark = true;
		};

		// ── BIBLIOTHEQUE ────────────────────────────────────────────────────────
		// « Choisir un theme parmi plusieurs ou creer le sien » suppose une LISTE,
		// pas deux fonctions ecrites en dur dans le code.
		//
		// ELLE NE LIT AUCUN FICHIER, et c'est deliberé : lire le disque exigerait
		// NKFileSystem, et cet en-tete perdrait sa propriete d'etre testable sans
		// rien lier. L'APPLICATION lit le texte -- elle sait ou sont ses dossiers,
		// livre puis surcharge utilisateur, exactement comme pour les icones -- et
		// le passe ici.
		/// Le theme par defaut de la COQUILLE (palette GitHub Dark d origine,
		/// exprimee en roles). N est PAS dans `AddBuiltins` : c est un defaut,
		/// pas un choix du menu. Voir sa definition pour la raison complete.
		NkTheme NkThemeCoquilleDefaut();

		class NkThemeLibrary {
			public:
				void AddBuiltins(); ///< Sombre et Clair

				// Ajoute un theme DEJA CONSTRUIT, ou remplace celui de meme nom.
				// C'est la porte d'entree de base : elle permet a l'application de
				// composer son theme AVANT de le deposer -- poser ses roles propres,
				// puis laisser le fichier les ecraser s'il le souhaite. Sans elle,
				// AddFromText fabriquait sa base en interne et l'application n'avait
				// aucun moyen d'y glisser ses defauts.
				int32 AddOrReplace(const NkTheme &t);

				// Ajoute un theme depuis un fichier texte. `baseDark` choisit la base
				// dont il HERITE : un theme de trois lignes doit hériter des 26
				// autres, et de la bonne famille. Renvoie l'index, ou -1.
				int32 AddFromText(const char *text, bool baseDark = true);

				uint32 Count() const {
					return (uint32)mThemes.Size();
				}
				const NkTheme &At(uint32 i) const {
					return mThemes[i];
				}
				int32 Find(const char *name) const;
				bool SetCurrent(const char *name);
				void SetCurrentIndex(uint32 i) {
					if (i < (uint32)mThemes.Size())
						mCurrent = i;
				}
				uint32 CurrentIndex() const {
					return mCurrent;
				}
				const NkTheme &Current() const {
					return mThemes[mCurrent];
				}

			private:
				NkVector<NkTheme> mThemes;
				uint32 mCurrent = 0;
		};

	} // namespace editorkit
} // namespace nkentseu

#include "NKEditorKit/NkTheme.inl"
