#pragma once
// -----------------------------------------------------------------------------
// @File    TemoinRendu.h
// @Brief   `--temoin-rendu` : LE FLUX DE COMMANDES DU PEINTRE, ECRIT TEL QUEL,
//          pour qu'une refonte d'apparence se juge sur ce qui BOUGE.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  ⚠️ POURQUOI CE TEMOIN EXISTE, ET POURQUOI CE N'EST PAS UNE CAPTURE
// =============================================================================
//  Le document 16 mesure 34 valeurs distinctes d'espacement et demande de les
//  ramener a six. Cette refonte-la DEPLACE DES PIXELS PARTOUT.
//
//  🔴 ET LA GARDE NE PEUT PAS ETRE « le compte est passe a six ». Ce serait un
//     COMPTEUR D'INTENTIONS : il dirait que le code cite six valeurs, jamais que
//     l'ecran est reste juste. C'est exactement la faute que ce depot a payee
//     tout le 02/09 -- une condition, un banc ou un compteur qui interroge ce
//     qu'on a VOULU au lieu de ce qui a ETE FAIT.
//
//  🔴 CE QUE CE TEMOIN NE VOIT PAS -- mesure le 02/09, apres qu'un deplacement
//     de texte de 2 px l'a traverse SANS UNE LIGNE DE DIFF :
//       1. LE TEXTE DES PANNEAUX. En headless, aucune police n'est chargee :
//          `AddText` sans face n'emet AUCUN sommet. Un libelle qui bouge, ou
//          disparait, est invisible ici.
//       2. L'INTERIEUR D'UNE COMMANDE FUSIONNEE. Chaque ecran de panneau tient
//          en UNE commande « unis » dont l'englobant est le panneau entier :
//          tout mouvement INTERNE (une pastille, un filet) reste sous
//          l'enveloppe.
//     La toile (NkRecordingPaint, commande par commande) n'a pas ces angles
//     morts -- les panneaux, si. POUR UN CHANGEMENT DE PANNEAU, LA PREUVE EST
//     LA CAPTURE : `--capture` + `--selectionner=<nom>` (l'etat PLEIN), en
//     paire avant/apres, diff pixel par pixel. Un « temoin identique » sur un
//     changement de panneau ne prouve RIEN et ne doit jamais etre cite seul.
//
//  LE TEMOIN EST DONC LE FLUX DE COMMANDES DU PEINTRE, geometrie comprise :
//  chaque `Fill`, `Text`, `Icon` avec ses x/y/w/h. Deplacer une marge de 3 px
//  change une ligne du fichier, et le `diff` la NOMME.
//
//  ⚠️ POURQUOI PAS UNE CAPTURE PNG, alors qu'on en parlait :
//    - une image dit « ce n'est pas pareil », le flux dit **quelle commande** a
//      bouge et **de combien** -- c'est la difference entre constater et
//      diagnostiquer ;
//    - il se `diff` avec les outils du depot, se relit dans une revue, et se
//      commite comme du texte ;
//    - il ne demande NI fenetre NI GPU, donc il tourne dans les memes conditions
//      que toutes les autres gardes ;
//    - une capture reste utile pour juger l'ALIGNEMENT et les PROPORTIONS, que
//      ce flux ne voit pas. Elle viendra, et elle ne remplacera pas ceci.
//
//  *Une refonte visuelle sans temoin se fait en aveugle ; celui-ci coute un
//  fichier texte.*
// -----------------------------------------------------------------------------

#include <cstdio>

#include "RecetteProprietes.h"

namespace nkuidesign {

	inline const char *NkNomOp(nkentseu::editorkit::NkPaintOp op) {
		using nkentseu::editorkit::NkPaintOp;
		switch (op) {
			case NkPaintOp::Fill: return "Fill";
			case NkPaintOp::FillColor: return "FillColor";
			case NkPaintOp::Outline: return "Outline";
			case NkPaintOp::OutlineSharp: return "OutlineSharp";
			case NkPaintOp::HLine: return "HLine";
			case NkPaintOp::VLine: return "VLine";
			case NkPaintOp::Text: return "Text";
			case NkPaintOp::Icon: return "Icon";
			case NkPaintOp::PushClip: return "PushClip";
			case NkPaintOp::PopClip: return "PopClip";
			case NkPaintOp::Ellipse: return "Ellipse";
			case NkPaintOp::Line: return "Line";
			default: return "?";
		}
	}

	/// Ecrit le flux d'un peintre enregistreur, une commande par ligne.
	/// ⚠️ FORMAT STABLE ET TRIVIAL A DIFFER : un champ par colonne, une decimale
	///    fixe. Pas de JSON, pas d'alignement variable -- un `diff` doit pointer
	///    LA ligne qui a bouge, pas un bloc reindente.
	inline void NkEcrireFlux(nkentseu::NkString &out, const char *ecran,
							 const nkentseu::editorkit::NkRecordingPaint &pv) {
		char l[320];
		snprintf(l, sizeof(l), "\n# ecran %s : %u commande(s)\n", ecran,
				 (nkentseu::uint32)pv.cmds.Size());
		out.Append(l);
		for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)pv.cmds.Size(); ++i) {
			const nkentseu::editorkit::NkPaintCmd &c = pv.cmds[i];
			const char *t = c.text.Data();
			snprintf(l, sizeof(l), "%-13s %8.1f %8.1f %8.1f %8.1f  r=%-6.1f role=%-4u rgba=%08x %s\n",
					 NkNomOp(c.op), (double)c.x, (double)c.y, (double)c.w, (double)c.h,
					 (double)c.rounding, (nkentseu::uint32)c.role, c.rgba, t ? t : "");
			out.Append(l);
		}
	}

	/// `--temoin-rendu[=<fichier>]`
	inline nkentseu::int32 NkTemoinRendu(const char *chemin) {
		using namespace nkentseu;
		const char *sortie = (chemin && *chemin) ? chemin : "nkuidesign_temoin_rendu.txt";

		static nkgui::NkGuiContext ctx;
		if (!ctx.Init(300, 900)) {
			printf("[temoin] le contexte sans fenetre a refuse\n");
			return 1;
		}
		nkgui::NkGuiIntrospectActiver(ctx, true);

		// ── LE SUJET : un document representatif, pas un cas fabrique ────────
		// ⚠️ UN NOEUD PAR NATURE, ET CHACUN AVEC SES PROPRIETES POSEES. Un temoin
		//    sur un rectangle nu ne verrait bouger que trois commandes : il
		//    passerait au vert pendant qu'une refonte deregle les listes, le
		//    texte et les effets. *Le temoin doit contenir ce qu'on risque de
		//    casser.*
		static DesignState st;
		st.doc.NewDocument("temoin", NkAuthor::Humain);
		const int32 pg = st.doc.AddChild(0, "", NkAuthor::Humain);
		st.doc.nodes[(uint32)pg].shape = NkString("frame");
		st.doc.nodes[(uint32)pg].layout.kind = NkLayoutKind::Free;
		st.doc.nodes[(uint32)pg].width.mode = NkSizeMode::Fixed;
		st.doc.nodes[(uint32)pg].width.value = 400.f;
		st.doc.nodes[(uint32)pg].height.mode = NkSizeMode::Fixed;
		st.doc.nodes[(uint32)pg].height.value = 300.f;
		const int32 rc = st.doc.AddChild(pg, "", NkAuthor::Humain);
		st.doc.nodes[(uint32)rc].shape = NkString("rect");
		st.doc.nodes[(uint32)rc].label = NkString("Bouton");
		st.doc.nodes[(uint32)rc].posX = 20.f;
		st.doc.nodes[(uint32)rc].posY = 20.f;
		st.doc.nodes[(uint32)rc].width.mode = NkSizeMode::Fixed;
		st.doc.nodes[(uint32)rc].width.value = 160.f;
		st.doc.nodes[(uint32)rc].height.mode = NkSizeMode::Fixed;
		st.doc.nodes[(uint32)rc].height.value = 48.f;
		st.doc.nodes[(uint32)rc].radius = 6.f;
		{
			NkRemplissage f1;
			f1.couleur = NkString("#0969da");
			NkRemplissage f2;
			f2.couleur = NkString("#f6f8fa");
			f2.opacite = 60.f;
			st.doc.nodes[(uint32)rc].fills.PushBack(f1);
			st.doc.nodes[(uint32)rc].fills.PushBack(f2);
			NkBordure b;
			b.couleur = NkString("#30363d");
			b.epaisseur = 2.f;
			b.position = NkBordurePos::Interieur;
			st.doc.nodes[(uint32)rc].borders.PushBack(b);
			NkEffet e;
			e.type = NkEffetType::OmbrePortee;
			e.flou = 6.f;
			st.doc.nodes[(uint32)rc].effets.PushBack(e);
		}
		const int32 tx = st.doc.AddChild(rc, "", NkAuthor::Humain);
		st.doc.nodes[(uint32)tx].shape = NkString("text");
		st.doc.nodes[(uint32)tx].text = NkString("Valider");
		st.doc.nodes[(uint32)tx].width.mode = NkSizeMode::Fixed;
		st.doc.nodes[(uint32)tx].width.value = 90.f;
		st.doc.nodes[(uint32)tx].height.mode = NkSizeMode::Fixed;
		st.doc.nodes[(uint32)tx].height.value = 20.f;
		st.doc.nodes[(uint32)rc].layout.kind = NkLayoutKind::Free;
		st.SelectSingle(rc);

		NkString out = NkString("# temoin de rendu NkUIDesign -- flux de commandes du peintre\n"
								"# Une ligne par commande : op, x, y, w, h, rayon, role, rgba,\n"
								"# texte. Un `diff` de ce fichier NOMME ce qui a bouge, et de\n"
								"# combien -- ce qu'une capture ne dit pas.\n"
								"# ⚠️ Il se REGENERE : `--temoin-rendu`. Toute difference non\n"
								"#    voulue est une regression visuelle.\n");

		// ── 1. LA TOILE ──────────────────────────────────────────────────────
		{
			NkLayoutResult lay;
			NkComputeLayout(st.doc, NkPaintRect{0.f, 0.f, 400.f, 300.f}, lay);
			NkComponentInput in;
			nkentseu::editorkit::NkRecordingPaint pv;
			NkDocumentHost hote;
			hote.SyncTo(st.doc);
			pv.Reset();
			NkDrawDocument(pv, in, st.doc, lay, hote, 0);
			NkEcrireFlux(out, "toile", pv);
		}

		// ── 2. LES SECTIONS DE L'INSPECTEUR ──────────────────────────────────
		// C'est LA que vivent les 34 espacements : le document 16 les compte dans
		// `Panels.h`. Un temoin qui ne montrerait que la toile raterait le sujet.
		{
			static InspectorPanel insp(&st);
			const char *noms[3] = {"inspecteur.bordures", "inspecteur.remplissages",
								   "inspecteur.effets"};
			for (int32 s = 0; s < 3; ++s) {
				// deux images : la premiere etablit les etats (survol, tampons),
				// la seconde est celle qu'on enregistre. Sans ca, le temoin
				// changerait d'un lancement a l'autre sur des transitions.
				RecetteProprietesAcces::Image(ctx, insp, -500.f, -500.f, false, s);
				RecetteProprietesAcces::Image(ctx, insp, -500.f, -500.f, false, s);
				// ⚠️ ON DUMPE LA LISTE DE DESSIN, PAS LES NOTES D'INTROSPECTION.
				//    Ma premiere version ecrivait les notes : seules les SAISIES en
				//    publient, donc le temoin d'une section entiere tenait en UNE
				//    ligne. Il serait reste identique pendant qu'une refonte
				//    d'espacement deplacait tous ses libelles et tous ses filets --
				//    *un temoin qui ne bouge jamais ne temoigne de rien.*
				//
				// ⚠️ ET ON RESUME CHAQUE COMMANDE PAR L'ENGLOBANT DE SES SOMMETS
				//    plutot que d'ecrire les sommets un a un : un dump de milliers
				//    de points serait illisible en revue, et un `diff` y noierait
				//    la ligne qui compte. L'englobant bouge des qu'une marge bouge,
				//    et il tient sur une ligne.
				const nkgui::NkGuiDrawList &dl = ctx.dl;
				char l[128];
				snprintf(l, sizeof(l), "\n# ecran %s : %u commande(s), %u sommet(s)\n", noms[s],
						 (uint32)dl.cmds.Size(), (uint32)dl.vtx.Size());
				out.Append(l);
				for (uint32 c = 0; c < (uint32)dl.cmds.Size(); ++c) {
					const nkgui::NkGuiDrawCmd &cm = dl.cmds[c];
					float32 x0 = 1.0e9f, y0 = 1.0e9f, x1 = -1.0e9f, y1 = -1.0e9f;
					for (uint32 k = 0; k < cm.idxCount; ++k) {
						const uint32 ii = cm.idxOffset + k;
						if (ii >= (uint32)dl.idx.Size())
							break;
						const uint32 vi = dl.idx[ii];
						if (vi >= (uint32)dl.vtx.Size())
							continue;
						const nkgui::NkVec2 &p = dl.vtx[vi].pos;
						if (p.x < x0)
							x0 = p.x;
						if (p.y < y0)
							y0 = p.y;
						if (p.x > x1)
							x1 = p.x;
						if (p.y > y1)
							y1 = p.y;
					}
					char b[224];
					snprintf(b, sizeof(b),
							 "cmd %-9s idx=%-5u englobant %8.1f %8.1f %8.1f %8.1f\n",
							 cm.type == nkgui::NkGuiDrawCmdType::Triangles ? "unis" : "texture",
							 cm.idxCount, (double)(x1 >= x0 ? x0 : 0.f),
							 (double)(y1 >= y0 ? y0 : 0.f), (double)(x1 >= x0 ? x1 - x0 : 0.f),
							 (double)(y1 >= y0 ? y1 - y0 : 0.f));
					out.Append(b);
				}
			}
		}

		if (!nkentseu::NkFile::WriteAllText(sortie, out.Data())) {
			printf("[temoin] ecriture refusee : %s\n", sortie);
			return 1;
		}
		printf("[temoin] flux ecrit dans %s (%u octets)\n", sortie, (uint32)out.Size());
		return 0;
	}

} // namespace nkuidesign
