#pragma once
// =============================================================================
// NkModelerFileDialog.h — NK3DModeler (Shell/)
//
// CE QUE CE FICHIER CONTIENT : un dialogue de choix d'emplacement, propre au
// projet — arborescence des dossiers, champ de nom, creation de dossier — et
// rien d'autre. Il ne connait ni les materiaux, ni les modeles, ni les scenes :
// l'appelant lui dit quoi montrer et recoit ce qui a ete choisi.
//
// CE QU'IL NE CONTIENT PAS : l'ecriture du fichier. Le dialogue REND une
// decision (dossier + nom) ; c'est l'appelant qui agit. Un dialogue qui
// ecrirait lui-meme ne servirait qu'une fois.
//
// POURQUOI GENERIQUE DES LE DEPART (Rihen, 12 aout 2026) : « ca va servir
// ailleurs [...] on pourra ajouter le filtre des fichiers ; dans certains cas
// on pourra entrer les extensions, mais dans d'autres comme les materiaux non,
// juste le nom du fichier. »
//
// REGLE INTANGIBLE : on ne sort JAMAIS de la racine du projet. Un dialogue
// interne n'a pas a donner acces au disque entier.
// =============================================================================
#include "NK3DModeler/Shell/NkModelerUI.h"
#include "NK3DModeler/Shell/NkModelerInput.h"
#include "NK3DModeler/Shell/NkModelerWidgets.h"
// L'unicite d'un nom de materiau se verifie sur les emplacements eux-memes, pas
// sur les cartes du navigateur (plafonnees a kMaxBrowser). Voir plus bas.
#include "NK3DModeler/Viewport/NkDemo3DHost.h"

namespace nkentseu {
	namespace nk3d {

		/// Ce que l'appelant demande au dialogue.
		struct NkFileDialogDesc {
				const char *title = "Choisir un emplacement";
				/// Extension IMPOSEE (« nkmat ») : elle ne se tape pas, elle
				/// s'affiche a cote du champ. Vide = l'appelant gere lui-meme.
				const char *forcedExt = nullptr;
				/// Montrer aussi les FICHIERS de ces natures (masque sur
				/// browserKind). 0 = les dossiers seuls, ce qui suffit a choisir
				/// un emplacement.
				uint32 kindFilter = 0u;
				const char *suggested = "";
				bool allowNewFolder = true;
				const char *okLabel = "Creer";
				/// Nature dont le nom doit etre unique dans TOUT le projet, pas
				/// seulement dans le dossier (2 = materiau). 0xFF = pas de regle
				/// globale, l'unicite reste locale au dossier.
				uint8 uniqueKind = 0xFFu;
		};

		/// Etat vivant. Un seul dialogue a la fois : deux dialogues empiles
		/// poseraient la question de savoir lequel recoit les clics, et la
		/// reponse serait toujours « le dernier declare ».
		struct NkFileDialogState {
				bool open = false;
				bool justOpened = false; ///< le clic qui OUVRE ne doit pas fermer
				int32 folder = -1;		 ///< dossier courant (-1 = racine du projet)
				char name[128] = {};
				float32 scrollY = 0.f;
				float32 dx = 0.f, dy = 0.f; ///< deplacement (poignee = le titre)
				bool drag = false;
				float32 grabX = 0.f, grabY = 0.f;
				bool newFolderMode = false;
				char newFolderName[64] = {};
				/// Rempli a la validation ; l'appelant le consomme.
				int32 resultFolder = -1;
				char resultName[128] = {};
		};

		/// Ouvre le dialogue sur le dossier COURANT (Rihen : « le dossier source
		/// est le dossier courant »).
		inline void NkFileDialogOpen(NkFileDialogState &d, const NkFileDialogDesc &desc,
									 int32 currentFolder) {
			d.open = true;
			d.justOpened = true;
			d.folder = currentFolder;
			d.newFolderMode = false;
			d.newFolderName[0] = 0;
			d.scrollY = 0.f;
			snprintf(d.name, sizeof(d.name), "%s", desc.suggested ? desc.suggested : "");
		}

		/// Peint le dialogue et recolte la decision. A appeler EN DERNIER dans la
		/// frame : les zones declarees en dernier gagnent le survol, et c'est ce
		/// qui rend un dialogue modal.
		/// Renvoie true quand l'utilisateur valide — l'appelant lit alors
		/// `resultFolder` et `resultName`, puis agit.
		inline bool NkFileDialogPaint(NkModelerPainter &p, NkHitRegistry &hit, NkWidgetState &ws,
									  const nkgui::NkGuiInput &in, NkModelerState &st,
									  NkFileDialogState &d, const NkFileDialogDesc &desc,
									  const NkRect &screen) {
			if (!d.open)
				return false;
			// LE VOILE, declare ici mais JUGE A LA FIN : teste des maintenant, il
			// gagnerait aussi les clics destines a la boite, qui se refermerait
			// sans jamais rien laisser choisir (piege paye deux fois le 12 aout).
			(void)hit.Add("fdlg.veil", screen);

			const float32 dw = screen.w < S(420.f) ? screen.w - S(24.f) : S(420.f);
			const float32 dh = screen.h < S(360.f) ? screen.h - S(24.f) : S(360.f);
			const NkRect dr{screen.x + (screen.w - dw) * 0.5f + d.dx,
							screen.y + (screen.h - dh) * 0.4f + d.dy, dw, dh};
			p.Outline(dr, NkRole::AccentUi, NkRole::PanelHeader, 4.f);
			hit.Add("fdlg.box", dr); // la boite avale ses propres clics

			// ── TITRE, QUI SERT DE POIGNEE ──────────────────────────────────
			const NkRect tb{dr.x + S(2.f), dr.y + S(2.f), dr.w - S(28.f), S(24.f)};
			p.Fill(tb, NkRole::PanelHeader, 3.f);
			hit.Add("fdlg.title", tb);
			if (hit.Clicked("fdlg.title")) {
				d.drag = true;
				d.grabX = in.mousePos.x - d.dx;
				d.grabY = in.mousePos.y - d.dy;
			}
			if (d.drag) {
				if (hit.MouseDown()) {
					d.dx = in.mousePos.x - d.grabX;
					d.dy = in.mousePos.y - d.grabY;
				} else {
					d.drag = false;
				}
			}
			p.TextV(dr.x + S(8.f), dr.y + S(2.f), S(24.f), desc.title, NkRole::Text);
			const NkRect xb{dr.x + dr.w - S(24.f), dr.y + S(5.f), S(18.f), S(18.f)};
			const bool ovX = hit.Add("fdlg.close", xb);
			p.Outline(xb, ovX ? NkRole::AccentUi : NkRole::Border, NkRole::PanelHeader, 3.f);
			p.IconV(xb.x + S(3.f), xb.y, xb.h, NkIcon::WinClose, NkRole::Text, 11.f);
			if (hit.Clicked("fdlg.close"))
				d.open = false;

			// ── FIL D'ARIANE + REMONTER ─────────────────────────────────────
			// LA RACINE EST UNE LIMITE (Rihen) : « Projet » est le plus haut
			// niveau atteignable, et le bouton remonter s'y eteint.
			const float32 barY = dr.y + S(30.f);
			const NkRect ub{dr.x + S(6.f), barY, S(24.f), S(20.f)};
			const bool canUp = (d.folder >= 0);
			const bool ovU = hit.Add("fdlg.up", ub);
			p.Outline(ub, (ovU && canUp) ? NkRole::AccentUi : NkRole::Border, NkRole::PanelHeader,
					  3.f);
			p.IconV(ub.x + S(5.f), ub.y, ub.h, NkIcon::ArrowUp,
					canUp ? NkRole::Text : NkRole::Border, 11.f);
			if (canUp && hit.Clicked("fdlg.up") && d.folder < st.browserCount)
				d.folder = st.browserParent[d.folder];
			{
				// FIL D'ARIANE construit ICI, sans passer par les assets : ce
				// dialogue ne doit dependre que de l'etat, sinon il ne serait plus
				// reutilisable — et l'en-tete des assets est inclus APRES lui.
				// On remonte les parents, puis on affiche a l'endroit.
				const char *parts[8] = {};
				int32 np = 0;
				int32 cur = d.folder;
				for (int32 g = 0; g < 8 && cur >= 0 && cur < st.browserCount; ++g) {
					if (st.browserKind[cur] != 1)
						break; // seul un DOSSIER fait un niveau
					parts[np++] = st.browserNames[cur];
					cur = st.browserParent[cur];
				}
				char chemin[256];
				usize used = (usize)snprintf(chemin, sizeof(chemin), "Projet/");
				for (int32 i = np - 1; i >= 0 && used < sizeof(chemin) - 1; --i)
					used += (usize)snprintf(chemin + used, sizeof(chemin) - used, "%s/",
											parts[i] ? parts[i] : "");
				p.TextV(ub.x + ub.w + S(6.f), barY, S(20.f), chemin, NkRole::TextMuted);
			}

			// ── LE NIVEAU COURANT ───────────────────────────────────────────
			const float32 listY = barY + S(24.f);
			const float32 listH = dr.h - (listY - dr.y) - S(80.f);
			const NkRect lb{dr.x + S(6.f), listY, dr.w - S(12.f), listH};
			p.Fill(lb, NkRole::InputBg, 3.f);
			p.OutlineSharp(lb, NkRole::Border);
			p.Clip(lb);
			hit.PushClip(lb);
			const float32 lineH = S(20.f);
			float32 ly = lb.y + S(2.f) - d.scrollY;
			int32 nShown = 0;
			for (int32 b = 0; b < st.browserCount; ++b) {
				// kind 1 = DOSSIER. Les fichiers ne s'affichent que si l'appelant
				// les demande : choisir un emplacement n'a pas besoin de les voir.
				const bool isFolder = (st.browserKind[b] == 1);
				const bool passe =
					isFolder || (desc.kindFilter && (desc.kindFilter & (1u << st.browserKind[b])));
				if (!passe || st.browserParent[b] != d.folder)
					continue;
				++nShown;
				if (ly + lineH >= lb.y && ly <= lb.y + lb.h) {
					char lk[24];
					snprintf(lk, sizeof(lk), "fdlg.it.%d", b);
					const NkRect ir{lb.x, ly, lb.w, lineH};
					const bool ovI = hit.Add(lk, ir);
					if (ovI)
						p.Fill(ir, NkRole::PanelHeader, 3.f);
					// MEME CONVENTION QUE LE NAVIGATEUR (Rihen : « meme le panneau
					// materiau doit recuperer les bonnes icones ») : le dossier
					// ouvert est celui ou l'on se trouve, les autres sont fermes,
					// et un fichier porte l'icone de sa NATURE.
					p.IconV(ir.x + S(4.f), ly, lineH,
							isFolder ? NkIcon::Folder : NkIcon::Material, NkRole::TextMuted,
							11.f);
					p.TextV(ir.x + S(22.f), ly, lineH, st.browserNames[b], NkRole::Text);
					// Un dossier s'OUVRE ; un fichier donne son nom au champ.
					if (hit.Clicked(lk)) {
						if (isFolder)
							d.folder = b;
						else
							snprintf(d.name, sizeof(d.name), "%s", st.browserNames[b]);
					}
				}
				ly += lineH;
			}
			if (nShown == 0)
				p.TextV(lb.x + S(6.f), lb.y + S(2.f), lineH, "(vide)", NkRole::TextMuted);
			hit.PopClip();
			p.Unclip();
			hit.Add("fdlg.list", lb);
			{
				const float32 contH = (float32)nShown * lineH + S(4.f);
				if (hit.IsHovered("fdlg.list") && in.wheel != 0.f)
					d.scrollY -= in.wheel * lineH;
				const float32 maxY = contH > lb.h ? contH - lb.h : 0.f;
				if (d.scrollY > maxY)
					d.scrollY = maxY;
				if (d.scrollY < 0.f)
					d.scrollY = 0.f;
				if (maxY > 0.f)
					p.Fill({lb.x + lb.w - S(4.f), lb.y + (d.scrollY / contH) * lb.h, S(3.f),
							lb.h * (lb.h / contH)},
						   NkRole::TextMuted, 2.f);
			}

			// ── NOUVEAU DOSSIER ─────────────────────────────────────────────
			const float32 rowY = lb.y + lb.h + S(6.f);
			if (desc.allowNewFolder) {
				const NkRect fb{dr.x + S(6.f), rowY, S(24.f), S(20.f)};
				const bool ovF = hit.Add("fdlg.newdir", fb);
				p.Outline(fb, ovF ? NkRole::AccentUi : NkRole::Border, NkRole::PanelHeader, 3.f);
				// Creation d un DOSSIER : c est un dossier qu on montre, pas un
					// plus generique.
					p.IconV(fb.x + S(5.f), fb.y, fb.h, NkIcon::FolderOpen, NkRole::Text, 11.f);
				if (hit.Clicked("fdlg.newdir"))
					d.newFolderMode = !d.newFolderMode;
				if (d.newFolderMode) {
					const NkRect nf{fb.x + fb.w + S(4.f), rowY, dr.w - S(46.f), S(20.f)};
					p.Outline(nf, NkRole::Border, NkRole::InputBg, 3.f);
					if (EditableText(p, hit, ws, in, "fdlg.newdirname",
									 {nf.x + S(4.f), rowY, nf.w - S(8.f), S(20.f)},
									 d.newFolderName[0] ? d.newFolderName : "nom du dossier",
									 d.newFolderName[0] ? NkRole::Text : NkRole::TextMuted,
									 d.newFolderName, 63u)) {
						// Le dossier NAIT DANS LE NIVEAU COURANT, jamais ailleurs.
						if (d.newFolderName[0] && st.browserCount < NkModelerState::kMaxBrowser) {
							const int32 c = st.browserCount++;
							st.browserKind[c] = 1;
							st.browserParent[c] = d.folder;
							st.browserSub[c] = 0;
							st.browserDoc[c] = 0;
							st.browserMat[c] = 0;
							st.browserFile[c][0] = 0;
							snprintf(st.browserNames[c], sizeof(st.browserNames[0]), "%s",
									 d.newFolderName);
							d.folder = c; // on entre dedans : c'est ce qu'on attend
							d.newFolderName[0] = 0;
							d.newFolderMode = false;
						}
					}
				}
			}

			// ── NOM + VALIDATION ────────────────────────────────────────────
			const float32 nameY = rowY + S(24.f);
			p.TextV(dr.x + S(6.f), nameY, S(20.f), "Nom", NkRole::TextMuted);
			const NkRect nr{dr.x + S(44.f), nameY, dr.w - S(150.f), S(20.f)};
			p.Outline(nr, NkRole::Border, NkRole::InputBg, 3.f);
			(void)EditableText(p, hit, ws, in, "fdlg.name",
							   {nr.x + S(4.f), nameY, nr.w - S(8.f), S(20.f)},
							   d.name[0] ? d.name : "sans nom",
							   d.name[0] ? NkRole::Text : NkRole::TextMuted, d.name, 127u);
			// L'EXTENSION NE SE TAPE PAS quand l'appelant l'impose : elle
			// s'affiche a cote, en sourdine.
			if (desc.forcedExt && desc.forcedExt[0]) {
				char ext[24];
				snprintf(ext, sizeof(ext), ".%s", desc.forcedExt);
				p.TextV(nr.x + nr.w + S(4.f), nameY, S(20.f), ext, NkRole::TextMuted);
			}
			// UNICITE DES NOMS (regle de Rihen) : un nom deja pris dans CE dossier
			// eteint la validation — mieux vaut un bouton inerte qu'un ecrasement.
			// UNICITE : d'abord dans CE dossier (deux fichiers voisins ne peuvent
			// pas porter le meme nom), mais AUSSI dans tout le projet quand
			// l'appelant le demande. La regle de Rihen est globale — « deux
			// materiaux ne peuvent pas avoir le meme nom » — et un test limite au
			// dossier laissait creer un second « Materiau » alors qu'il existait
			// deja ailleurs (constate le 12 aout).
			bool libre = (d.name[0] != 0);
			for (int32 b = 0; b < st.browserCount && libre; ++b) {
				if (st.browserParent[b] != d.folder)
					continue;
				if (strcmp(st.browserNames[b], d.name) == 0)
					libre = false;
			}
			// Les cartes ne sont PAS l'index du projet : `kMaxBrowser` en plafonne
			// le nombre, et au-dela `NkBrowserSyncMats` renonce a en creer. Un
			// materiau existant peut donc n'avoir aucune carte — c'est ce qui a
			// laisse naitre un second « Materiau » le 12 aout. Pour une nature dont
			// le nom est unique dans TOUT le projet, on interroge donc la source de
			// verite (les emplacements eux-memes), jamais leur affichage.
			// UN NOM DE MATERIAU DEJA PRIS N'ETEINT PLUS LE BOUTON. Il sera numerote
			// a la creation (« Bois » -> « Bois.001 »), comme Blender -- decision de
			// Rihen le 13 aout : renommer plutot que refuser. On le DIT quand meme,
			// pour que le nom finalement retenu ne soit pas une surprise.
			bool seraRenomme = false;
			if (desc.uniqueKind == 2u && d.name[0]) {
				const int32 matMax = demo::Demo3DHostProjMatMax();
				for (int32 m = 0; m < matMax && !seraRenomme; ++m) {
					char nm[64];
					float32 alb[3];
					float32 rough = 0.f, metal = 0.f;
					if (!demo::Demo3DHostProjMatInfo(m, nm, (uint32)sizeof(nm), alb, &rough,
													 &metal))
						continue;
					seraRenomme = (strcmp(nm, d.name) == 0);
				}
			}
			if (seraRenomme)
				p.TextV(dr.x + S(12.f), nameY + S(4.f), S(18.f),
						"Ce nom existe deja : il sera numerote", NkRole::TextMuted);
			const NkRect okb{dr.x + dr.w - S(90.f), nameY + S(26.f), S(84.f), S(22.f)};
			const bool ovOk = hit.Add("fdlg.ok", okb);
			p.Outline(okb, (ovOk && libre) ? NkRole::AccentUi : NkRole::Border,
					  NkRole::PanelHeader, 3.f);
			p.TextV(okb.x + S(8.f), okb.y, okb.h, desc.okLabel,
					libre ? NkRole::Text : NkRole::TextMuted);
			bool valide = false;
			if (libre && hit.Clicked("fdlg.ok")) {
				d.resultFolder = d.folder;
				snprintf(d.resultName, sizeof(d.resultName), "%s", d.name);
				d.open = false;
				valide = true;
			}
			if (!libre && d.name[0])
				p.TextV(dr.x + S(6.f), nameY + S(26.f), S(22.f), "ce nom existe deja ici",
						NkRole::TextMuted);

			// LE CADRE EN DERNIER : rien de ce qui precede ne peut plus le manger.
			p.OutlineSharp(dr, NkRole::AccentUi);

			// CLIC « A COTE » : juge en tout dernier, donc seulement si aucune
			// zone du dialogue ne l'a pris.
			if (!d.justOpened && hit.Clicked("fdlg.veil"))
				d.open = false;
			d.justOpened = false;
			return valide;
		}

	} // namespace nk3d
} // namespace nkentseu
