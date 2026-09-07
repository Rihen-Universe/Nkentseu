#pragma once
// -----------------------------------------------------------------------------
// @File    ExportDialogue.h
// @Brief   ④ LE DIALOGUE D'EXPORT -- UN SEUL, porte par Ctrl+E ET par le clic droit :
//          format, echelle, etendue, nom, destination.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUE RODOLF A DEMANDE, ET POURQUOI CA TIENT EN UN DIALOGUE
// =============================================================================
//  « Le nom du fichier exporte doit etre celui de sa page. On peut tout exporter, pas
//  seulement les pages : meme les graphiques, les groupes et leurs enfants... Deux
//  raccourcis : clic droit et Ctrl+E, mais l'un ou l'autre doit demander le format,
//  peut-etre directement dans le dialogue approprie. »
//
//  🔴 UN SEUL DIALOGUE, DEUX PORTES -- ET C'EST LA LECON DE CTRL+D DU MATIN. Le menu
//     portait NEUF entrees (page x1/x2/x3, selection x1/x2, SVG, SVG embarque...) : chaque
//     combinaison etait un chemin, et le jour ou l'une aurait diverge, personne n'aurait
//     su laquelle. Ici, le format, l'echelle, l'etendue et le nom sont des CHAMPS d'un
//     dialogue unique ; le menu et le raccourci ne font que l'ouvrir.
//  ⚠️ CTRL+E N'EST DECLARE QU'UNE FOIS (la table de commandes de la coquille). La toile ne
//     le lit pas : deux declarations feraient deux ouvertures, et c'est exactement le
//     defaut ① du matin.
//  ⚠️ LA DESTINATION RESTE AU SELECTEUR DE FICHIER DU KIT : ce dialogue dit QUOI exporter,
//     le selecteur dit OU. Les melanger aurait demande de reecrire un navigateur de
//     fichiers dans une modale de 420 px.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkEditorModal.h"

#include "ExportSVG.h"

namespace nkuidesign {

	/// Ouvre le dialogue. `surSelection` : ce que le geste vise (le clic droit sur un objet
	/// vise la selection ; Ctrl+E vise la selection s'il y en a une, la page sinon).
	inline void NkOuvrirDialogueExport(DesignState &st, bool surSelection) {
		using namespace nkentseu;
		DesignState::NkChoixExport &c = st.choixExport;
		const bool aSel = !st.sel.Empty() || (st.doc.IsValidIndex(st.selected) && st.selected > 0);
		c.selection = surSelection && aSel;
		// ② (07/09) SANS SELECTION, LA CIBLE EST TOUT LE CANVAS -- ET S'IL N'Y A
		//    RIEN, ON N'OUVRE PAS.
		// 🔴 Rodolf : « rien n'est selectionne mais le panneau s'ouvre. Ce n'est
		//    pas normal -- sauf si ca liste tous les elements exportables du canvas. »
		//    Mesure : le panneau ne s'ouvrait pas « sur rien », il retombait sur
		//    `NkPageParDefaut`, c'est-a-dire LA PREMIERE page -- alors que son
		//    document en porte TROIS. Le choix etait arbitraire, et le seul mot qui
		//    le disait partait au pied de fenetre.
		// ⚠️ LE COMPTE VIENT DE `NkElementsExportables`, jamais d'un comptage refait
		//    ici : le panneau annoncerait sinon des elements que l'export refuse.
		// ⚠️ ET ZERO EXPORTABLE REFUSE D'OUVRIR : *un panneau qui liste zero element
		//    est le meme defaut sous un autre nom.*
		if (!c.selection) {
			NkVector<nkentseu::int32> exportables;
			c.nbExportables = NkElementsExportables(st, st.layout, &exportables);
			if (c.nbExportables == 0u) {
				st.DireAuPied("Exporter : rien à exporter — le canvas ne contient aucun "
							  "élément visible.");
				return; // on n'ouvre pas : il n'y a rien a montrer
			}
			c.tout = true;
		} else {
			c.tout = false;
			c.nbExportables = 0u;
		}
		c.dialogue.open = true;
		c.dialogue.posInit = false; // se recentre a chaque ouverture
		NkExportOptions o;
		o.format = (NkExportFormat)c.format;
		o.echelle = c.echelle;
		o.selection = c.selection;
		o.tout = c.tout; // ② la cible << tout le canvas >>, lue par NkZoneExport
		NkNomObjetExport(st, o, c.nom, sizeof(c.nom));
		// ② L'ANCIEN MESSAGE DISAIT « c'est la page qui sera exportee » : il est
		//    devenu FAUX le jour ou la cible est le canvas entier. Il part avec le
		//    comportement qu'il decrivait -- une phrase juste qu'on garde apres avoir
		//    change ce qu'elle decrit devient un mensonge, et c'est la troisieme fois
		//    que ce depot le paie.
		if (!aSel && surSelection)
			st.DireAuPied("Exporter : rien n'est sélectionné — c'est tout le canvas qui "
						  "sera exporté.");
	}

	/// Le dialogue lui-meme, dessine dans le crochet d'overlay (l'entree y est reelle).
	/// Rend vrai quand il a lance l'ouverture du selecteur de destination.
	inline bool NkDessinerDialogueExport(nkentseu::nkgui::NkGuiContext &ctx, DesignState &st) {
		using namespace nkentseu;
		using namespace nkentseu::editorkit;
		DesignState::NkChoixExport &c = st.choixExport;
		// ④ la DEMANDE posée par le menu contextuel (le dispatcheur n'ouvre rien lui-même)
		if (st.exportDemande) {
			st.exportDemande = false;
			NkOuvrirDialogueExport(st, st.exportSurSelection);
		}
		if (!c.dialogue.open)
			return false;
		auto &F = costume::Fontes();
		const float32 cw = 420.f, chh = 232.f;
		NkModalFrame fr = NkModalFrameDraw(ctx, c.dialogue, "Exporter", cw, chh);
		if (!fr.visible)
			return false;
		if (fr.closeAsked) {
			c.dialogue.open = false;
			return false;
		}
		auto &dl = ctx.dlOverlay;
		const NkRect z = fr.content;
		const uint32 nSel = st.sel.Count() > 0u ? st.sel.Count()
											   : ((st.doc.IsValidIndex(st.selected) && st.selected > 0) ? 1u : 0u);
		const bool aSel = nSel > 0u;
		const bool png = (c.format == (int32)NkExportFormat::PNG);
		bool relire = false; // le nom se recompose quand le choix change
		float32 y = z.y + 8.f;
		// une rangee de boutons : rend l'indice clique, -1 sinon
		auto rangee = [&](const char *titre, const char *const *libs, const bool *actifs, int32 n, int32 courant) -> int32 {
			costume::Texte(dl, F.px10, z.x + 12.f, costume::CentrerY(F.px10, y, 22.f), titre, ctx.theme.textMuted);
			const float32 x0 = z.x + 108.f, x1 = z.x + z.w - 12.f;
			const float32 bw = (x1 - x0 - (float32)(n - 1) * 4.f) / (float32)n;
			int32 choisi = -1;
			for (int32 k = 0; k < n; ++k) {
				const NkRect b = {x0 + (bw + 4.f) * (float32)k, y + 1.f, bw, 20.f};
				const bool actif = actifs ? actifs[k] : true;
				const bool sv = actif && NkGuiRectContains(b, ctx.input.mousePos);
				nkgui::NkColor fond = (k == courant) ? ctx.theme.accent : ctx.theme.panel;
				if (k == courant)
					fond.a = 120;
				dl.AddRectFilled(b, sv ? ctx.theme.rowHover : fond, 4.f);
				dl.AddRect(b, (sv || k == courant) ? ctx.theme.accent : ctx.theme.border, 1.f, 4.f);
				costume::TexteTronque(dl, F.px10, b.x + 6.f, costume::CentrerY(F.px10, b.y, b.h), libs[k], b.w - 12.f,
									  actif ? ctx.theme.text : ctx.theme.textDisabled);
				if (sv && ctx.input.mouseClicked[0]) {
					ctx.input.mouseClicked[0] = false;
					choisi = k;
				}
			}
			y += 26.f;
			return choisi;
		};
		// ── FORMAT (PDF et code sont NOMMES et grises : ils existeront, ils n'existent pas) ──
		{
			static const char *const kLib[4] = {"PNG", "SVG", "PDF", "Code"};
			static const bool kActif[4] = {true, true, false, false};
			const int32 k = rangee("Format", kLib, kActif, 4, c.format);
			if (k == 0 || k == 1) {
				c.format = k;
				relire = true;
			} else if (k >= 2)
				st.DireAuPied(k == 2 ? "PDF : à construire — le même arbre que le SVG (document 14, §3b)."
									 : "HTML / CSS / React / Next : à construire — un lecteur de plus du format.");
		}
		// ── ECHELLE (PNG seulement : un SVG n'a pas de pixels) ──────────────────
		{
			static const char *const kLib[3] = {"×1", "×2", "×3"};
			const bool kActif[3] = {png, png, png};
			const int32 cour = c.echelle > 2.5f ? 2 : (c.echelle > 1.5f ? 1 : 0);
			const int32 k = rangee("Échelle", kLib, kActif, 3, png ? cour : -1);
			if (png && k >= 0) {
				c.echelle = 1.f + (float32)k;
				relire = true;
			} else if (!png && k >= 0)
				st.DireAuPied("Un SVG n'a pas de pixels : l'échelle ne s'applique qu'au PNG.");
		}
		// ── ETENDUE ────────────────────────────────────────────────────────────
		{
			char libSel[64];
			snprintf(libSel, sizeof(libSel), aSel ? "Sélection (%u)" : "Sélection", nSel);
			// ② (07/09) LE PANNEAU DIT COMBIEN. Sans selection, la cible est TOUT LE
			//    CANVAS, et le libelle porte le nombre d'elements trouves : *un panneau
			//    qui liste sans dire combien laisse croire qu'il a tout pris.* Le
			//    nombre vient de `c.nbExportables`, pose a l'ouverture par
			//    `NkElementsExportables` -- la MEME fonction que `NkZoneExport` lit,
			//    donc le panneau ne peut pas annoncer ce que l'export refuserait.
			char libTout[72];
			snprintf(libTout, sizeof(libTout), "Tout le canvas (%u)", c.nbExportables);
			const char *kLib[2] = {c.tout ? libTout : "Page", libSel};
			const bool kActif[2] = {true, aSel};
			const int32 k = rangee("Étendue", kLib, kActif, 2, c.selection ? 1 : 0);
			if (k == 0 || (k == 1 && aSel)) {
				c.selection = (k == 1);
				relire = true;
			} else if (k == 1)
				st.DireAuPied("Rien n'est sélectionné — sélectionne un objet, un groupe ou plusieurs.");
		}
		// ── UNE IMAGE OU UN FICHIER PAR OBJET (plusieurs objets seulement) ──────
		{
			static const char *const kLib[2] = {"Une image", "Un fichier par objet"};
			const bool multi = c.selection && nSel >= 2u;
			const bool kActif[2] = {multi, multi};
			const int32 k = rangee("Sortie", kLib, kActif, 2, multi ? (c.parObjet ? 1 : 0) : 0);
			if (multi && k >= 0) {
				c.parObjet = (k == 1);
				relire = true;
			} else if (!multi && k >= 0)
				st.DireAuPied("Un seul objet : les deux sorties donnent le même fichier.");
		}
		// ── LE NOM (recompose a chaque changement de choix, mais jamais pendant la frappe) ──
		if (relire) {
			NkExportOptions o;
			o.format = (NkExportFormat)c.format;
			o.echelle = c.echelle;
			o.selection = c.selection;
			o.tout = c.tout; // ② la cible << tout le canvas >>, lue par NkZoneExport
			NkNomObjetExport(st, o, c.nom, sizeof(c.nom));
		}
		{
			costume::Texte(dl, F.px10, z.x + 12.f, costume::CentrerY(F.px10, y, 22.f), "Nom", ctx.theme.textMuted);
			const NkRect rn = {z.x + 108.f, y + 1.f, z.w - 108.f - 12.f - 46.f, 20.f};
			dl.AddRectFilled(rn, nkgui::NkColor{22, 27, 34, 255}, 4.f);
			dl.AddRect(rn, ctx.theme.border, 1.f, 4.f);
			nkentseu::editorkit::NkOverlayTextField(ctx, dl, ctx.font, rn, c.nom, (int32)sizeof(c.nom), true);
			const char *ext = c.parObjet && c.selection && nSel >= 2u
								  ? "…"
								  : (c.format == (int32)NkExportFormat::PNG ? ".png" : ".svg");
			costume::Texte(dl, F.px10, rn.x + rn.w + 6.f, costume::CentrerY(F.px10, rn.y, 20.f), ext, ctx.theme.textMuted);
			y += 26.f;
		}
		// ── CE QUE CA VA FAIRE, EN UNE PHRASE ──────────────────────────────────
		{
			char phrase[220];
			if (c.selection && c.parObjet && nSel >= 2u)
				snprintf(phrase, sizeof(phrase), "%u fichiers, un par objet, dans le dossier choisi.", nSel);
			else if (c.selection)
				snprintf(phrase, sizeof(phrase), "un fichier : %s de la sélection%s.",
						 nSel >= 2u ? "la boîte englobante" : "l'objet", png ? "" : " (SVG : vectoriel)");
			else if (c.tout)
				// ① (07/09) LA PHRASE SUIT L'ETENDUE, elle aussi. Elle disait « la page »
				//    sous une etendue « tout le canvas » -- SECOND des deux sites cales sur
				//    la page (recensement du 07/09 : le nom propose et cette phrase).
				snprintf(phrase, sizeof(phrase),
						 "un fichier : les %u éléments du canvas, dans leur boîte englobante%s.",
						 c.nbExportables, png ? "" : " (SVG : vectoriel)");
			else
				snprintf(phrase, sizeof(phrase), "un fichier : la page%s.", png ? "" : " (SVG : vectoriel)");
			costume::TexteTronque(dl, F.px9, z.x + 12.f, y + 4.f, phrase, z.w - 24.f, ctx.theme.textMuted);
			y += 22.f;
		}
		// ── LES DEUX BOUTONS ───────────────────────────────────────────────────
		bool lance = false;
		{
			const NkRect bAnn = {z.x + z.w - 12.f - 180.f, z.y + z.h - 34.f, 84.f, 26.f};
			const NkRect bExp = {z.x + z.w - 12.f - 92.f, z.y + z.h - 34.f, 92.f, 26.f};
			const bool svA = NkGuiRectContains(bAnn, ctx.input.mousePos);
			const bool svE = NkGuiRectContains(bExp, ctx.input.mousePos);
			dl.AddRectFilled(bAnn, svA ? ctx.theme.rowHover : ctx.theme.panel, 4.f);
			dl.AddRect(bAnn, ctx.theme.border, 1.f, 4.f);
			costume::Texte(dl, F.px11, bAnn.x + (bAnn.w - costume::Largeur(F.px11, "Annuler")) * 0.5f,
						   costume::CentrerY(F.px11, bAnn.y, bAnn.h), "Annuler", ctx.theme.text);
			nkgui::NkColor fondE = ctx.theme.accent;
			if (!svE)
				fondE.a = 200;
			dl.AddRectFilled(bExp, fondE, 4.f);
			costume::Texte(dl, F.px11, bExp.x + (bExp.w - costume::Largeur(F.px11, "Exporter…")) * 0.5f,
						   costume::CentrerY(F.px11, bExp.y, bExp.h), "Exporter…", nkgui::NkColor{255, 255, 255, 255});
			if (svA && ctx.input.mouseClicked[0]) {
				ctx.input.mouseClicked[0] = false;
				c.dialogue.open = false;
			}
			if (svE && ctx.input.mouseClicked[0]) {
				ctx.input.mouseClicked[0] = false;
				lance = true;
			}
		}
		if (!lance)
			return false;
		// ── LA DESTINATION : le selecteur du kit, avec le nom compose ──────────
		c.dialogue.open = false;
		c.embarquer = false;
		char nomFichier[220];
		const char *ext = (c.format == (int32)NkExportFormat::PNG) ? ".png" : ".svg";
		if (c.parObjet && c.selection && nSel >= 2u)
			snprintf(nomFichier, sizeof(nomFichier), "%s%s", c.nom[0] ? c.nom : "objet", ext); // le dossier compte, pas ce nom
		else
			snprintf(nomFichier, sizeof(nomFichier), "%s%s", c.nom[0] ? c.nom : "export", ext);
		// ⚠️ LA MEME PORTE QUE LE MENU (`NkOuvrirSelecteurExport`). Ouvrir ici par
		//    `OpenPickerBase` marchait -- et laissait le selecteur sans filtre
		//    d'extension, sans vignette, sans role et sans recents, parce que ces
		//    quatre-la etaient poses par l'AUTRE porte. Meme defaut que ①, un cran
		//    plus loin : ce n'est pas la lecture qui manquait, c'est la preparation.
		NkOuvrirSelecteurExport(st, nomFichier);
		return true;
	}

} // namespace nkuidesign
