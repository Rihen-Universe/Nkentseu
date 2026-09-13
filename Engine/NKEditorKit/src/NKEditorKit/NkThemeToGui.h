#pragma once
// -----------------------------------------------------------------------------
// @File    NkThemeToGui.h
// @Brief   LA conversion NkTheme -> NkGuiTheme. Une seule, dans le kit.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE N EST PAS UN POINT DE SYNCHRONISATION, C EST UNE CONVERSION
// =============================================================================
// La formulation vient de l agent NK3DModeler, le 2026-08-29, et elle corrige
// une decision qui disait « `NkEditorShell::ApplyTheme` est le point unique ou
// le theme est pousse ». Deux choses clochaient :
//
//   1. ca confond LE LIEU DE L APPEL et LE LIEU DE LA VERITE ;
//   2. ca ne pouvait pas couvrir NK3DModeler, qui **n utilise deliberement pas
//      `NkEditorShell`** (`NkModelerUI.h:5` : la coquille apporte son propre
//      chrome, ce qui empeche de coller a la maquette au pixel pres).
//
// > **Deux appelants d une conversion ne font pas deux autorites.**
//
// Un seul convertisseur, plusieurs appelants -- exactement comme une commande
// et plusieurs entrees. La coquille l appelle depuis `ApplyTheme` ; une
// application SANS coquille l appelle depuis son propre point nomme.
//
// ⚠️ POURQUOI CE FICHIER EXISTE ALORS QUE LE CODE EXISTAIT DEJA. La conversion
//    etait ecrite -- et **enfermee dans un `namespace {}` anonyme de
//    `NkEditorShell.cpp`**. Ce n est pas « pas encore extrait » : c est
//    INACCESSIBLE, au sens de l editeur de liens, pour toute application qui n a
//    pas de coquille. NK3DModeler ne pouvait donc pas l appeler meme en le
//    voulant, et sa seule sortie etait d en ecrire une seconde. C est le
//    mecanisme exact des deux registres de roles homonymes et des deux objets
//    theme payes cette semaine : **la deuxieme copie n est presque jamais un
//    caprice, c est la seule porte restee ouverte.**
//
// ⚠️ CE FICHIER NE DECIDE D AUCUNE COULEUR. Il traduit des ROLES en champs. Les
//    couleurs vivent dans `NkTheme` (et dans les themes que l utilisateur
//    ecrira) ; les regles de derivation sont NOMMEES ci-dessous et suivent donc
//    n importe quel theme, y compris ceux qui n existent pas encore.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkEditorExport.h"
#include "NKEditorKit/NkTheme.h"
#include "NKGui/NKGui.h"

namespace nkentseu {
	namespace editorkit {

		/// `NkThemeColor` (0xRRGGBBAA empaquete) -> couleur NKGui.
		NKENTSEU_FORCE_INLINE nkgui::NkColor NkThemeUnpack(NkThemeColor c) noexcept {
			return {(uint8)((c >> 24) & 0xFFu), (uint8)((c >> 16) & 0xFFu), (uint8)((c >> 8) & 0xFFu),
					(uint8)(c & 0xFFu)};
		}

		/// La regle NOMMEE qui derive les etats : un melange lineaire vers une
		/// autre couleur du meme theme.
		/// ⚠️ CE N EST PAS UNE COULEUR INVENTEE, c est une FONCTION des roles.
		///    C est ce qui fait qu un theme ecrit par un utilisateur obtient des
		///    survols coherents sans avoir a les declarer -- et qu on n a pas
		///    seize valeurs de plus a maintenir a la main.
		/// ⚠️ L ALPHA DE `a` EST CONSERVE, jamais melange : un voile a 160 melange
		///    vers un accent opaque deviendrait opaque, et le voile disparaitrait.
		NKENTSEU_FORCE_INLINE nkgui::NkColor NkThemeMix(nkgui::NkColor a, nkgui::NkColor b, float32 t) noexcept {
			const float32 u = 1.f - t;
			return {(uint8)(a.r * u + b.r * t), (uint8)(a.g * u + b.g * t), (uint8)(a.b * u + b.b * t), a.a};
		}

		// =====================================================================
		//  LA CONVERSION
		// =====================================================================
		// ⚠️ LA COUVERTURE EST EXHAUSTIVE, ET C EST LE POINT LE PLUS IMPORTANT DE
		//    CE FICHIER. `NkGuiTheme` porte **35 champs**. Chacun tombe dans
		//    exactement une des trois categories ci-dessous, et aucune n est
		//    « le reste » :
		//
		//      (M) MAPPE       -- il vient d un role, directement ;
		//      (D) DERIVE      -- il vient d une regle nommee sur d autres champs ;
		//      (I) INCHANGE    -- il est laisse tel quel, AVEC SA RAISON ECRITE.
		//
		//    Une recopie partielle -- 23 champs sur 35, les 12 autres au hasard --
		//    est precisement ce qui divergera au premier role ajoute. Le
		//    `static_assert` en fin de fichier existe pour ca : il tombe le jour
		//    ou quelqu un ajoute un champ a `NkGuiTheme` sans venir le classer
		//    ici. **Un manque doit se voir a la compilation, pas a l ecran.**
		//
		// ⚠️ CE QUE `NkTheme` A ET QUE `NkGuiTheme` N A PAS -- dit, pas devine.
		//    Sur les 30 roles, **quinze n ont aucun equivalent** dans un theme de
		//    widgets, et c est normal : ils decrivent un modeleur 3D et un graphe
		//    de noeuds, pas un bouton.
		//      AxisX · AxisY · AxisZ                 (gizmo)
		//      TypeMesh · TypeAnim · TypeMat · TypeTex · TypeFolder   (pastilles d actifs)
		//      NodeDataHeader · NodeDataHeaderHot · NodeActionHeader · NodeBody · NodeWire
		//      ViewportTop · ViewportBottom · GridLine
		//    L application qui en a besoin les lit **directement** dans son
		//    `NkTheme` -- les faire transiter par `NkGuiTheme` obligerait NKGui a
		//    connaitre le vocabulaire d un modeleur, ce qui est exactement la
		//    dependance qu on refuse.
		//
		//    Trois autres roles existent et ne sont PAS lus ici, faute de champ
		//    correspondant : `LabelCol`, `AccentSel`, `ElemActive` / `ElemSelected`
		//    / `ElemIdle`. ⚠️ `AccentSel` porte l AMBRE de la selection (regle du
		//    depot : « le BLEU dit l etat de l INTERFACE, l AMBRE dit la
		//    selection ») ; le mapper sur `g.selection` changerait la couleur de
		//    selection de TOUTES les applications, ce qui n est pas une decision
		//    de convertisseur. **Nomme, pas resolu.**

		/// Traduit un theme d editeur en theme de widgets NKGui.
		///
		/// ⚠️ LA SIGNATURE NE DEMANDE NI COQUILLE NI CONTEXTE, et c est ce qui la
		///    rend utilisable par NK3DModeler. Elle ne prend que ce qu elle
		///    ecrit et ce qu elle lit.
		NKENTSEU_FORCE_INLINE void NkThemeVersGui(nkgui::NkGuiTheme &g, const NkTheme &t) noexcept {
			auto R = [&t](NkRole r) { return NkThemeUnpack(t.Get(r)); };
			const nkgui::NkColor accent = R(NkRole::AccentUi);

			// ── (M) MAPPES : un role, un champ ───────────────────────────────
			g.bgPrimary = R(NkRole::WindowBg);
			g.panel = R(NkRole::PanelBg);
			g.header = R(NkRole::PanelHeader);
			g.card = R(NkRole::PanelHeader);
			g.border = R(NkRole::Border);
			g.separator = R(NkRole::Border);
			g.text = R(NkRole::Text);
			g.textMuted = R(NkRole::TextMuted);
			g.textDisabled = R(NkRole::TextMuted);
			g.onAccent = R(NkRole::TextOnAccent);
			g.accent = accent;
			g.selection = accent;
			g.track = R(NkRole::InputBg);
			// ⚠️ `ButtonBg` AVEC REPLI SUR `InputBg`, jamais `InputBg` seul. La
			//    mesure du 2026-08-29 : la palette de `NkEditorShell::Init` donne
			//    `button` #191D23 et `track` #0D1117 -- deux couleurs, un seul role.
			//    Le role manquait ; il existe maintenant, et tant qu'un theme ne le
			//    pose pas, le repli rend exactement ce que rendait la version d'avant.
			g.button = NkThemeUnpack(t.GetOuRepli(NkRole::ButtonBg, NkRole::InputBg));
			g.buttonActive = accent;

			// Les onglets : la barre recule, l inactif se pose dessus, l ACTIF
			// prend la couleur du panneau qu il ouvre -- ce que le document 3 §6
			// demande (« --bg-canvas pour l actif, --bg-subtle pour les inactifs »).
			// Meme raison : la barre d'onglets n'est pas le fond de fenetre.
			g.tabBar = NkThemeUnpack(t.GetOuRepli(NkRole::TabBarBg, NkRole::WindowBg));
			g.tab = R(NkRole::PanelHeader);
			g.tabActive = R(NkRole::PanelBg);

			// ── (D) DERIVES : une regle nommee, pas une couleur inventee ──────
			g.buttonHover = NkThemeMix(g.button, accent, 0.20f);
			g.tabHover = NkThemeMix(g.tab, accent, 0.20f);
			g.rowHover = NkThemeMix(g.panel, accent, 0.14f);
			g.scrollbar = NkThemeMix(g.panel, g.text, 0.30f);
			g.scrollbarHover = NkThemeMix(g.panel, g.text, 0.50f);

			// ── (I) INCHANGES, et chacun avec sa raison ───────────────────────
			// g.success / g.warning / g.danger / g.info
			//     AUCUN role equivalent cote editeur. Ce sont des couleurs
			//     SEMANTIQUES (« ceci a rate »), et une semantique ne se derive
			//     pas d une palette : un vert melange vers l accent d un theme
			//     bleu cesse de dire « reussite ». Elles resteront des constantes
			//     tant que `NkTheme` n aura pas ses quatre roles d etat.
			// g.scrim / g.shadow
			//     Volontairement NEUTRES et semi-transparents. Un voile de modale
			//     teinte par l accent colore tout l ecran du dessous ; c est un
			//     effet, pas une couleur de marque. ⚠️ Et les teinter casserait la
			//     conservation d alpha de `NkThemeMix` sur laquelle ils reposent.
			// g.rounding / g.roundingSmall / g.roundingLarge
			// g.borderThickness / g.framePadX / g.framePadY
			//     CE NE SONT PAS DES COULEURS. `NkTheme` est une palette de roles ;
			//     la geometrie d un widget (rayon d angle, epaisseur, marges) n y a
			//     pas d equivalent et ne doit pas en avoir un invente ici.
			//     ⚠️ Consequence a connaitre : une application qui veut des coins
			//     droits pose `g.rounding = 0.f` **apres** l appel. La conversion
			//     ne les ecrase pas, justement pour que ce reglage survive a un
			//     changement de theme.
		}

		/// Meme conversion, ecrite dans le theme d un contexte NKGui.
		/// Sucre : deux appelants d une conversion ne font pas deux autorites, et
		/// deux surcharges encore moins.
		NKENTSEU_FORCE_INLINE void NkThemeVersGui(nkgui::NkGuiContext &ctx, const NkTheme &t) noexcept {
			NkThemeVersGui(ctx.theme, t);
		}

		// ⚠️ LA GARDE. Elle tombe le jour ou `NkGuiTheme` gagne (ou perd) un champ.
		//    Ce n est pas une verification de valeur -- c est impossible -- c est
		//    une CONVOCATION : « quelqu un a touche au theme, viens dire dans
		//    quelle des trois categories tombe le nouveau champ ».
		//
		//    29 NkColor (4 octets) + 6 float32 (4 octets) = 140 octets, plus le
		//    drapeau d'onglet et son remplissage (4) = 144.
		//
		//    ⚠️ SI ELLE TOMBE, NE CHANGE PAS LE CHIFFRE SANS CLASSER LE CHAMP.
		//       Corriger le nombre pour faire taire l assertion la transforme en
		//       decoration, et le champ neuf restera au hasard -- exactement ce
		//       que ce fichier existe pour empecher.
		//
		//    CLASSEMENT DU 2026-08-31 -- `tabActiveIsWindowBg` (bool, opt-in) :
		//    INCHANGE-AVEC-RAISON. Ce n'est pas une couleur et il ne derive d'aucun
		//    role : c'est une CONVENTION D'APPLICATION (Banani V2 : l'onglet actif
		//    rejoint le fond de la zone document), posee par l'app qui opte
		//    (NkUIDesign). La conversion NE LE TOUCHE PAS -- si elle le remettait a
		//    sa valeur par defaut, chaque changement de theme ecraserait l'opt-in.
		static_assert(sizeof(nkgui::NkGuiTheme) == 29 * 4 + 6 * 4 + 4,
					  "NkGuiTheme a change de forme : classe le champ neuf dans NkThemeVersGui "
					  "(mappe / derive / inchange-avec-raison), PUIS ajuste ce chiffre.");

	} // namespace editorkit
} // namespace nkentseu
