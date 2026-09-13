#pragma once
// -----------------------------------------------------------------------------
// @File    RecetteEdition.h
// @Brief   LA RECETTE DU CONTRAT UNIVERSEL D'EDITION (Rodolf, 31/08) — prouvee
//          PAR SITE : Hierarchie (arbre du kit), etiquette d'artboard, texte de
//          toile. Pour CHAQUE site, les trois sorties : ENTREE valide et sort ;
//          ECHAP annule et sort (valeur d'origine intacte au round-trip) ; un
//          CLIC AILLEURS valide et sort, PUIS le clic fait son effet normal.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// ⚠️ POURQUOI CETTE RECETTE EXISTE — la classe de defaut qu'elle interdit de
//    revenir, mesuree le 01/09 : « plus possible de desactiver l'edition ».
//    DEUX mecanismes d'edition coexistent (la saisie du composant arbre pour la
//    Hierarchie ; l'overlay de la toile pour l'etiquette et le texte), et le
//    volet « clic ailleurs » du contrat se jugeait, dans les deux, contre le
//    MAUVAIS rectangle : la zone entiere de l'arbre (un clic sur une AUTRE
//    rangee ne sortait jamais), la toile entiere (un clic sur un AUTRE panneau
//    ne validait jamais, un clic DANS le champ validait a tort). Le bon
//    rectangle est celui de la RANGEE editee (arbre — juge par le composant,
//    seul a le connaitre) et celui du CHAMP superpose (toile — mEditRect).
//
// ⚠️ CE QUE CETTE RECETTE EXERCE, ET CE QU'ELLE N'EXERCE PAS. Elle appelle le
//    VRAI code de sortie : `NkDrawTreeView` (le composant que la Hierarchie
//    dessine, peintre enregistreur — piege n.4 evite) et
//    `PreviewPanel::FermerEditionTexte` / `HandleMouse` (les fonctions memes de
//    l'application). Ce qu'elle ne peut pas exercer sans fenetre : la
//    TRADUCTION des touches en drapeaux (ClavierRenommage lit le clavier et
//    leve renameCommit/renameCancel — une ligne par touche) et le POINTAGE reel
//    de la toile (NkPick* sur une disposition calculee). Ces deux-la sont
//    prouves au releve par injection (rapport Q41 : releves A2/B2/C2/D2/G).
// -----------------------------------------------------------------------------

#include <cstdio>
#include <cstring>

#include "NKEditorKit/Components/NkTreeViewProbe.h"
#include "Panels.h"

namespace nkuidesign {

	/// L'acces de banc aux membres prives de `PreviewPanel` (declare ami). Un
	/// banc qui passerait par l'interface publique devrait simuler la fenetre
	/// entiere ; celui-ci exerce exactement les fonctions de sortie.
	struct RecetteEditionAcces {

			static bool Identiques(const NkString &a, const NkString &b) {
				const char *x = a.Data() ? a.Data() : "";
				const char *y = b.Data() ? b.Data() : "";
				while (*x && *x == *y) {
					++x;
					++y;
				}
				return *x == *y;
			}

			static nkentseu::int32 Lancer() {
				using namespace nkentseu;
				using namespace nkentseu::editorkit;
				int32 echecs = 0, total = 0;
				auto check = [&](const char *nom, bool ok) {
					printf("%s  %s\n", ok ? "OK   " : "ECHEC", nom);
					++total;
					if (!ok)
						++echecs;
				};

				// ═══ SITE 1 — LA HIERARCHIE (le composant arbre du kit) ═══════
				// Le contrat « clic ailleurs » vit DANS `NkDrawTreeView` : la
				// recette exerce le composant lui-meme, pas une reecriture.
				{
					struct Temoin {
							char recu[128] = {0};
							int32 fois = 0;
							nk_uint64 dernierSelect = 0;
					} temoin;
					NkTreeViewHooks hooks;
					hooks.user = &temoin;
					hooks.onRename = [](void *u, int32, const char *, const char *,
										const char *nouveau) {
						auto *t = static_cast<Temoin *>(u);
						snprintf(t->recu, sizeof(t->recu), "%s", nouveau ? nouveau : "");
						++t->fois;
					};

					NkTreeViewModel m;
					treeprobe::FillDemo(m);
					NkRecordingPaint rec;
					NkComponentInput repos;
					repos.surfaceScale = 1.f;

					// Une passe au repos pour RELEVER le rectangle du libelle
					// « Sol » — la geometrie vient du flux dessine, pas d'une
					// reconstruction du calcul de rangee.
					treeprobe::RenderInto(rec, m, nullptr, repos, &hooks);
					auto rectTexte = [&](const char *libelle, float32 &x, float32 &y) -> bool {
						for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i)
							if (rec.cmds[i].op == NkPaintOp::Text
								&& rec.cmds[i].text.Data()
								&& 0 == strcmp(rec.cmds[i].text.Data(), libelle)) {
								x = rec.cmds[i].x + 2.f;
								y = rec.cmds[i].y + 2.f;
								return true;
							}
						return false;
					};
					float32 sx = 0.f, sy = 0.f, ex = 0.f, ey = 0.f;
					const bool trouves = rectTexte("Sol", sx, sy) && rectTexte("Environnement", ex, ey);
					check("arbre : les deux libelles temoins sont dessines", trouves);

					auto dblclicSur = [&](float32 x, float32 y) {
						NkComponentInput in;
						in.surfaceScale = 1.f;
						in.mouseX = x;
						in.mouseY = y;
						in.mouseDown = true;
						in.mousePressed = true;
						in.doubleClick = true;
						treeprobe::RenderInto(rec, m, nullptr, in, &hooks);
					};
					auto clicSur = [&](float32 x, float32 y) {
						NkComponentInput in;
						in.surfaceScale = 1.f;
						in.mouseX = x;
						in.mouseY = y;
						in.mouseDown = true;
						in.mousePressed = true;
						treeprobe::RenderInto(rec, m, nullptr, in, &hooks);
					};
					auto reposer = [&] { treeprobe::RenderInto(rec, m, nullptr, repos, &hooks); };
					const nk_uint64 idSol = m.nodes[1].id;
					const nk_uint64 idEnv = m.nodes[0].id;

					// 1a. Le double-clic OUVRE (l'etat de depart du bogue).
					dblclicSur(sx, sy);
					check("arbre : double-clic sur le libelle -> la saisie s'ouvre",
						  m.renaming == idSol && 0 == strcmp(m.renameBuf, "Sol"));

					// 1b. ENTREE valide et sort. (L'hote traduit la touche en
					// `renameCommit` — ClavierRenommage, une ligne ; prouve au
					// releve B2. Ici : le drapeau -> onRename part, la saisie
					// se ferme.)
					snprintf(m.renameBuf, sizeof(m.renameBuf), "SolRenomme");
					m.renameCommit = true;
					reposer();
					check("arbre : ENTREE -> valide (onRename porte la nouvelle valeur) et sort",
						  m.renaming == 0 && temoin.fois == 1
							  && 0 == strcmp(temoin.recu, "SolRenomme"));

					// 1c. ECHAP annule et sort — valeur d'origine INTACTE :
					// aucun onRename ne part, le libelle du modele n'a pas bouge.
					dblclicSur(sx, sy);
					snprintf(m.renameBuf, sizeof(m.renameBuf), "NeDoitPasSortir");
					m.renameCancel = true;
					reposer();
					check("arbre : ECHAP -> annule (aucun onRename, libelle intact) et sort",
						  m.renaming == 0 && temoin.fois == 1
							  && Identiques(m.nodes[1].label, NkString("Sol")));

					// 1d. CLIC AILLEURS (une AUTRE rangee) : valide ET le clic
					// fait son effet normal — la selection passe a la rangee
					// cliquee. C'etait LE geste piege du 01/09.
					dblclicSur(sx, sy);
					snprintf(m.renameBuf, sizeof(m.renameBuf), "SolParLeClic");
					clicSur(ex, ey); // l'image du clic : la selection agit
					const bool selectionAgit = (m.active == idEnv);
					reposer(); // l'image suivante : le commit leve au clic aboutit
					check("arbre : CLIC sur une autre rangee -> valide ET selectionne la rangee cliquee",
						  m.renaming == 0 && temoin.fois == 2
							  && 0 == strcmp(temoin.recu, "SolParLeClic") && selectionAgit);

					// 1e. Le clic d'OUVERTURE programmee se mange (le [+] de
					// Pages) : `renameEatClick` absorbe UN clic hors rangee,
					// le suivant valide.
					m.renaming = idSol;
					snprintf(m.renameBuf, sizeof(m.renameBuf), "PageNee");
					m.renameEatClick = true;
					clicSur(ex, ey + 400.f); // le clic d'ouverture (hors rangee)
					const bool mange = (m.renaming == idSol);
					clicSur(ex, ey + 400.f); // un clic ORDINAIRE hors rangee
					reposer();
					check("arbre : le clic d'ouverture du [+] se mange, le suivant valide",
						  mange && m.renaming == 0 && temoin.fois == 3);
				}

				// ═══ SITE 4 — VAGUE 2 : `Alt`+GLISSER DUPLIQUE ═══════════════
				// Source `/layers`. CONTEXTE : la toile, corps d'un noeud POSE.
				//
				// ⚠️ CE CAS PASSE PAR `HandleMouse`, PAS PAR UNE REGLE EXTRAITE, et
				//    c'est voulu : ce qu'il faut prouver n'est pas « copier marche »
				//    (`CopierSousArbre` est deja tenu ailleurs) mais QUE LA COPIE SE
				//    FAIT UNE FOIS, A L'ARMEMENT. La boucle de glisser passe des
				//    dizaines de fois par seconde ; une copie posee dedans laisserait
				//    une trainee d'objets derriere le curseur, et aucune recette de
				//    modele ne le verrait. *Un geste qui cree se declenche une fois,
				//    au moment ou la main s'engage.*
				{
					static DesignState st4;
					st4.BuildStarterDocument();
					const int32 pere = st4.doc.AddChild(0, "", NkAuthor::Humain);
					st4.doc.nodes[(uint32)pere].shape = NkString("frame");
					st4.doc.nodes[(uint32)pere].layout.kind = NkLayoutKind::Free;
					// ⚠️ LE SECOND ARGUMENT EST UN COMPOSANT DU REGISTRE, PAS UNE
					//    FORME. Ma premiere version passait `"rect"` : `AddChild`
					//    rend **-1** (aucun composant de ce nom), et la ligne
					//    suivante indexait `nodes[(uint32)-1]`. La forme se pose
					//    apres, par `shape`, exactement comme les sites 2 et 3.
					const int32 obj = st4.doc.AddChild(pere, "", NkAuthor::Humain);
					st4.doc.nodes[(uint32)obj].shape = NkString("rect");
					st4.doc.nodes[(uint32)obj].posX = 40.f;
					st4.doc.nodes[(uint32)obj].posY = 30.f;
					st4.doc.nodes[(uint32)obj].width.mode = NkSizeMode::Fixed;
					st4.doc.nodes[(uint32)obj].width.value = 120.f;
					st4.doc.nodes[(uint32)obj].height.mode = NkSizeMode::Fixed;
					st4.doc.nodes[(uint32)obj].height.value = 80.f;
					st4.view.viewport = {0.f, 0.f, 800.f, 600.f};
					st4.SelectSingle(obj);
					static PreviewPanel pv4(&st4);
					// un ecran a la main : le corps de `obj` occupe 40,30 -> 160,110.
					NkLayoutResult ecran;
					for (uint32 i = 0; i < (uint32)st4.doc.nodes.Size(); ++i) {
						ecran.rects.PushBack(NkPaintRect{0.f, 0.f, 0.f, 0.f});
						ecran.valid.PushBack(0);
					}
					ecran.rects[(uint32)pere] = {0.f, 0.f, 400.f, 300.f};
					ecran.valid[(uint32)pere] = 1;
					ecran.rects[(uint32)obj] = {40.f, 30.f, 120.f, 80.f};
					ecran.valid[(uint32)obj] = 1;
					st4.layout = ecran;
					const uint32 avant = (uint32)st4.doc.nodes.Size();
					const float32 origX = st4.doc.nodes[(uint32)obj].posX;
					// l'appui AVEC Alt, au CENTRE du corps (loin des huit poignees)
					NkComponentInput in;
					in.surfaceScale = 1.f;
					in.mouseX = 100.f;
					in.mouseY = 70.f;
					in.mouseDown = true;
					in.mousePressed = true;
					in.alt = true;
					pv4.HandleMouse(in, ecran);
					const uint32 apresAppui = (uint32)st4.doc.nodes.Size();
					const int32 tire = pv4.mMoveNode;
					// (a) UN noeud de plus, et UN SEUL
					const bool uneCopie = apresAppui == avant + 1u;
					// (b) C'EST LA COPIE QU'ON TRAINE, PAS L'ORIGINAL. L'inverse rend
					//     le meme document et une experience differente : la forme sous
					//     le doigt ne serait pas celle qu'on croit tenir, et
					//     l'annulation retirerait celle qui n'a pas bouge.
					const bool traineLaCopie = tire != obj && st4.doc.IsValidIndex(tire);
					// (c) ⚠️ LE VOLET QUI COMPTE : DIX images de glisser ensuite, `Alt`
					//     toujours enfonce -> AUCUN noeud de plus. Une copie posee dans
					//     la boucle en ferait dix.
					for (uint32 k = 0; k < 10u; ++k) {
						in.mousePressed = false;
						in.mouseX += 4.f;
						in.mouseY += 2.f;
						pv4.HandleMouse(in, ecran);
					}
					const bool uneSeuleFois = (uint32)st4.doc.nodes.Size() == apresAppui;
					// (d) CONSERVATION : l'original n'a pas bouge d'un iota.
					const bool originalIntact = st4.doc.nodes[(uint32)obj].posX == origX;
					// (e) ET LE CONTROLE NEGATIF : SANS `Alt`, aucune copie. Sans ce
					//     volet, un code qui dupliquerait a CHAQUE appui passerait
					//     (a), (b), (c) et (d).
					in.mouseDown = false;
					pv4.HandleMouse(in, ecran);
					const uint32 avantNu = (uint32)st4.doc.nodes.Size();
					in.alt = false;
					in.mouseDown = true;
					in.mousePressed = true;
					in.mouseX = 100.f;
					in.mouseY = 70.f;
					pv4.HandleMouse(in, ecran);
					const bool sansAltRien = (uint32)st4.doc.nodes.Size() == avantNu;
					in.mouseDown = false;
					in.mousePressed = false;
					pv4.HandleMouse(in, ecran);
					check("toile : `Alt`+glisser DUPLIQUE -- une seule copie, faite A L'ARMEMENT "
						  "(dix images de glisser n'en font pas dix), c'est la COPIE qu'on traine, "
						  "l'original est intact, et sans `Alt` rien n'est cree",
						  uneCopie && traineLaCopie && uneSeuleFois && originalIntact
							  && sansAltRien);
				}

				// ═══ SITES 2 et 3 — LA TOILE (etiquette d'artboard, texte) ════
				// Les fonctions de sortie REELLES de PreviewPanel, sur un
				// document minimal ; l'ECHAP se juge au round-trip octet pour
				// octet (la serialisation, prouvee ailleurs par --roundtrip).
				{
					static DesignState st;
					st.BuildStarterDocument();
					const int32 f = st.doc.AddChild(0, "", NkAuthor::Humain);
					st.doc.nodes[(uint32)f].shape = NkString("frame");
					st.doc.nodes[(uint32)f].label = NkString("PageRecette");
					const int32 t = st.doc.AddChild(f, "", NkAuthor::Humain);
					st.doc.nodes[(uint32)t].shape = NkString("text");
					st.doc.nodes[(uint32)t].text = NkString("Origine");
					st.doc.nodes[(uint32)t].label = NkString("TexteRecette");
					st.view.viewport = {0.f, 0.f, 800.f, 600.f};
					static PreviewPanel pv(&st);
					const NkLayoutResult ecranVide; // le pointage reel est prouve au releve
					auto ser = [&] {
						NkString o;
						st.doc.Save(o);
						return o;
					};
					// ⚠️ L'ANCRE — LE VOLET *CONSERVATION*. Les deux cas « ECHAP ->
					//    document identique octet pour octet » ne comparaient que
					//    DEUX SORTIES DU MEME PRODUCTEUR : mesure du 01/09, avec
					//    `NkUIDocument::Save` neutralisee, cette recette rendait
					//    **13/13 PROUVEE** sur un ecrivain mort — deux chaines
					//    vides sont egales. « Stable » ne veut pas dire « juste » :
					//    on exige donc AUSSI que la serialisation DISE quelque
					//    chose, et que le texte d'origine soit ENCORE LA.
					auto ancree = [](const NkString &s) -> bool {
						return s.Size() > 0 && s.Data() && s.Contains("nkuidoc")
							   && s.Contains("noeud");
					};

					// ── SITE 2 : LE TEXTE DE TOILE ────────────────────────────
					auto ouvrirTexte = [&](const char *frappe) {
						pv.mEditNode = t;
						pv.mEditEtiquette = false;
						pv.mEditRect = {100.f, 100.f, 120.f, 22.f};
						snprintf(pv.mEditBuf, sizeof(pv.mEditBuf), "%s", frappe);
					};
					// 2a. ENTREE valide et sort (le champ traduit la touche en
					// FermerEditionTexte(true) — une ligne ; releve du 31/08).
					ouvrirTexte("Origine modifiee");
					pv.FermerEditionTexte(true);
					check("toile/texte : ENTREE -> la valeur est ecrite, l'edition fermee",
						  pv.mEditNode == -1
							  && Identiques(st.doc.nodes[(uint32)t].text,
											NkString("Origine modifiee")));

					// 2b. ECHAP : valeur d'origine intacte AU ROUND-TRIP.
					const NkString avantTexte = ser();
					ouvrirTexte("NeDoitJamaisSortir");
					pv.FermerEditionTexte(false);
					// conservation : le document DIT quelque chose, ET le texte
					// d'origine est encore la (pas seulement « stable »).
					check("toile/texte : ECHAP -> document identique octet pour octet, ET le "
						  "texte d'origine est encore la",
						  pv.mEditNode == -1 && ancree(avantTexte)
							  && Identiques(ser(), avantTexte)
							  && Identiques(st.doc.nodes[(uint32)t].text,
											NkString("Origine modifiee")));

					// 2c. CLIC AILLEURS — le cas casse du 01/09 : un clic HORS
					// de la toile (un autre panneau) doit VALIDER. Il se juge en
					// TETE de HandleMouse, avant le retour « hors de la toile ».
					ouvrirTexte("Ecrit par le clic");
					{
						NkComponentInput in;
						in.surfaceScale = 1.f;
						in.mouseX = 900.f; // hors du viewport {0,0,800,600}
						in.mouseY = 300.f;
						in.mouseDown = true;
						in.mousePressed = true;
						pv.HandleMouse(in, ecranVide);
					}
					check("toile/texte : CLIC ailleurs (autre panneau) -> valide et sort",
						  pv.mEditNode == -1
							  && Identiques(st.doc.nodes[(uint32)t].text,
											NkString("Ecrit par le clic")));

					// 2d. Le clic DANS le champ appartient au champ : rien ne
					// se ferme (le contrat ne dit pas « tout clic valide »).
					ouvrirTexte("EncoreEnSaisie");
					{
						NkComponentInput in;
						in.surfaceScale = 1.f;
						in.mouseX = 110.f; // dans mEditRect {100,100,120,22}
						in.mouseY = 110.f;
						in.mouseDown = true;
						in.mousePressed = true;
						pv.HandleMouse(in, ecranVide);
					}
					check("toile/texte : un clic DANS le champ ne ferme pas la saisie",
						  pv.mEditNode == t);
					pv.FermerEditionTexte(false); // on referme proprement

					// ── SITE 3 : L'ETIQUETTE D'ARTBOARD (le NOM = `label`, la
					//    cle que la Hierarchie lit aussi) ──────────────────────
					auto ouvrirEtiquette = [&](const char *frappe) {
						pv.mEditNode = f;
						pv.mEditEtiquette = true;
						pv.mEditRect = {200.f, 74.f, 160.f, 22.f};
						snprintf(pv.mEditBuf, sizeof(pv.mEditBuf), "%s", frappe);
					};
					// 3a. ENTREE.
					ouvrirEtiquette("PageRenommee");
					pv.FermerEditionTexte(true);
					check("etiquette : ENTREE -> le label est ecrit, l'edition fermee",
						  pv.mEditNode == -1
							  && Identiques(st.doc.nodes[(uint32)f].label,
											NkString("PageRenommee")));

					// 3b. ECHAP, au round-trip.
					const NkString avantEtiquette = ser();
					ouvrirEtiquette("JamaisEcrit");
					pv.FermerEditionTexte(false);
					check("etiquette : ECHAP -> document identique octet pour octet, ET le "
						  "libelle d'origine est encore la",
						  pv.mEditNode == -1 && ancree(avantEtiquette)
							  && Identiques(ser(), avantEtiquette)
							  && Identiques(st.doc.nodes[(uint32)f].label,
											NkString("PageRenommee")));

					// 3c. CLIC AILLEURS.
					ouvrirEtiquette("EcritParLeClic");
					{
						NkComponentInput in;
						in.surfaceScale = 1.f;
						in.mouseX = 900.f;
						in.mouseY = 300.f;
						in.mouseDown = true;
						in.mousePressed = true;
						pv.HandleMouse(in, ecranVide);
					}
					check("etiquette : CLIC ailleurs -> valide et sort",
						  pv.mEditNode == -1
							  && Identiques(st.doc.nodes[(uint32)f].label,
											NkString("EcritParLeClic")));
				}

				// ═══ SITE 4 — LA RESERVE DE DROITE (ecart E2 du cote a cote) ═══
				// Le badge de composant se coupait en plein mot (« Butto ») parce
				// que l'overlay se placait APRES le nom. Le contrat est desormais :
				// `rowRightReserve` borne le libelle AVANT la zone du badge — le
				// nom cede, jamais le badge.
				//
				// ⚠️ LA PREUVE EST LE RECTANGLE QUE LE PEINTRE A RECU, pas le hook
				//    appele : on dessine DEUX FOIS le meme arbre, sans puis avec
				//    une reserve de 40 px, et on exige que le rectangle du libelle
				//    ait perdu EXACTEMENT ces 40 px. La mutation « le dessin
				//    ignore la reserve » rend les deux largeurs egales -> ECHEC.
				{
					NkTreeViewModel m;
					treeprobe::FillDemo(m);
					NkComponentInput repos;
					repos.surfaceScale = 1.f;
					auto largeurDe = [](NkRecordingPaint &rec, const char *libelle) -> float32 {
						for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i)
							if (rec.cmds[i].op == NkPaintOp::Text && rec.cmds[i].text.Data()
								&& 0 == strcmp(rec.cmds[i].text.Data(), libelle))
								return rec.cmds[i].w;
						return -1.f;
					};
					NkRecordingPaint sans;
					treeprobe::RenderInto(sans, m, nullptr, repos, nullptr);
					const float32 wSans = largeurDe(sans, "Sol");

					NkTreeViewHooks hooks;
					hooks.rowRightReserve = [](void *, NkComponentPaint &, int32) -> float32 {
						return 40.f;
					};
					NkRecordingPaint avec;
					treeprobe::RenderInto(avec, m, nullptr, repos, &hooks);
					const float32 wAvec = largeurDe(avec, "Sol");

					// ⚠️ wSans > 40 d'abord : sur un libelle deja plus etroit que
					//    la reserve, le « -40 » serait inverifiable et le cas
					//    passerait A VIDE — la famille « donnees degenerees ».
					check("reserve : le libelle temoin est assez large pour la mesurer",
						  wSans > 40.f);
					check("reserve : 40 px demandes -> le rectangle du libelle perd 40 px",
						  wSans > 0.f && wAvec > 0.f && wSans - wAvec > 39.5f
							  && wSans - wAvec < 40.5f);
				}

				printf("RECETTE EDITION : %d/%d %s\n", total - echecs, total,
					   echecs == 0 ? "PROUVEE" : "EN ECHEC");
				return echecs == 0 ? 0 : 1;
			}
	};

	inline nkentseu::int32 RecetteEdition() {
		return RecetteEditionAcces::Lancer();
	}

} // namespace nkuidesign
