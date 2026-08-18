#pragma once
// -----------------------------------------------------------------------------
// @File    Panels.h
// @Brief   Les panneaux de NkUIDesign : palette, composition, apercu, proprietes, IA.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUI EST DEDANS, ET CE QUI N'Y EST PAS
// =============================================================================
//  Etat au 2026-08-19. La tranche verticale du 18/08 (un composant, ses
//  reglages, sa sauvegarde) est devenue une APPLICATION :
//
//    - **palette**     : les composants DECLARES, lus dans le registre, poses
//                        dans le document ;
//    - **composition** : l'arbre — un composant DANS un autre, meme mecanisme
//                        aux deux echelles ;
//    - **agencement**  : taille et disposition a la souris, qui ecrivent des
//                        PROPRIETES et jamais une coordonnee ;
//    - **document**    : une interface complete se charge et se sauve, avec sa
//                        PROVENANCE posee des la creation ;
//    - **IA**          : la place — un prompt, un backend remplacable, et une
//                        sortie qui passe par la meme porte que la main.
//
//  ⚠️ HORS TRANCHE, nomme, differe, volontairement absent :
//    - les **blueprints** : les evenements sont declares avec leur charge, rien
//      ne s'y branche encore ;
//    - la **creation de composants ex nihilo** : on compose ce qui est declare ;
//    - le **dessin d'icones** ;
//    - l'**ancrage** au sens complet (marges par bord) — cf. `Document.h` ;
//    - le **modele specialise** — cf. `DesignAI.h`, qui livre sa place, pas lui.
//
// REGLE DE LECTURE POUR CE FICHIER : **aucun panneau ne connait le nom d'un
//   composant.** Palette, arbre, reglages et catalogue d'IA bouclent tous sur le
//   registre ou sur les tables de la declaration. Le seul endroit du programme ou
//   un nom de composant est ecrit en clair est `Renderers.h`, parce qu'il faut
//   bien appeler une fonction — et ce fichier-la dit pourquoi, et ce qu'il
//   faudrait pour s'en passer.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkGuiComponentPaint.h"
#include "NKEditorKit/NkEditorKit.h"
#include "NKEditorKit/NkTheme.h"
#include "NKFileSystem/NkFile.h"

#include "DesignAI.h"
#include "Renderers.h"

#include <cstdio>

namespace nkuidesign {

	using namespace nkentseu::editorkit;
	using namespace nkentseu::nkgui;

	static const char *kDocumentPath = "nkuidesign_document.nkuidoc";

	// ═══════════════════════════════════════════════════════════════════════════
	//  L'ETAT PARTAGE
	// ═══════════════════════════════════════════════════════════════════════════
	// ⚠️ IL NE CONTIENT AUCUNE VALEUR DE REGLAGE NI AUCUN RECTANGLE. Les reglages
	//    vivent dans l'instance de chaque noeud ; les rectangles se recalculent a
	//    chaque image et se jettent. C'est ce qui garantit que ce que l'apercu
	//    dessine est EXACTEMENT ce que la sauvegarde ecrit — deux copies auraient
	//    diverge des la premiere seance.
	struct DesignState {
			NkUIDocument doc;
			NkLayoutResult layout;
			NkDocumentHost host;
			NkTheme theme;

			NkDesignAI ai;
			NkFileBackend fileBackend;

			int32 selected = 0;		 ///< index de noeud ; 0 = la racine
			int32 paletteChoice = 0; ///< 0 = cadre, puis 1+ = index dans le registre
			NkString status;
			char promptBuf[512] = {0};

			void Init() {
				theme = NkTheme::Dark();
				// Le registre est la SOURCE de la palette. On y inscrit ce que cette
				// application connait ; les autres composants s'y inscriront de leur
				// cote, et la palette les affichera sans qu'une ligne bouge ici.
				NkComponentRegistry::Register(NkContentBrowserDecl());
				host.resolve = &NkResolveRole;
				ai.SetBackend(&fileBackend);

				if (!LoadDoc())
					BuildStarterDocument();
			}

			/// Un document de depart qui montre les deux choses a montrer : une
			/// imbrication, et deux modes de taille cote a cote.
			void BuildStarterDocument() {
				doc.NewDocument("Interface de demonstration", NkAuthor::Humain);
				doc.nodes[0].layout.kind = NkLayoutKind::Column;

				const int32 header = doc.AddChild(0, "", NkAuthor::Humain);
				if (doc.IsValidIndex(header)) {
					doc.nodes[(uint32)header].label = NkString("Entete");
					doc.nodes[(uint32)header].height.mode = NkSizeMode::Fixed;
					doc.nodes[(uint32)header].height.value = 56.f;
					doc.nodes[(uint32)header].layout.kind = NkLayoutKind::Row;
				}
				const int32 body = doc.AddChild(0, "", NkAuthor::Humain);
				if (doc.IsValidIndex(body)) {
					doc.nodes[(uint32)body].label = NkString("Corps");
					doc.nodes[(uint32)body].layout.kind = NkLayoutKind::Row;
					doc.AddChild(body, "content_browser", NkAuthor::Humain);
				}
				selected = 0;
				status = NkString("Document de demonstration cree.");
				host.demoModels.Clear();
				host.SyncTo(doc);
			}

			void Recompute(const NkPaintRect &surface) {
				host.SyncTo(doc);
				NkComputeLayout(doc, surface, layout);
			}

			void SaveDoc() {
				NkString out;
				doc.Save(out);
				status = nkentseu::NkFile::WriteAllText(kDocumentPath, out.Data())
							 ? NkString("Document enregistre : ")
							 : NkString("ECHEC d'ecriture : ");
				status.Append(kDocumentPath);
			}

			bool LoadDoc() {
				if (!nkentseu::NkFile::Exists(kDocumentPath))
					return false;
				const NkString text = nkentseu::NkFile::ReadAllText(kDocumentPath);
				uint32 unknown = 0;
				NkUIDocument loaded;
				if (!loaded.Load(text.Data(), &unknown)) {
					// ⚠️ ON NE CHARGE PAS A MOITIE. Un document dont la structure est
					//    incoherente laisse l'ancien en place et le DIT : recuperer un
					//    arbre a demi reconstruit serait pire que ne rien recuperer,
					//    parce que l'utilisateur croirait avoir retrouve son travail.
					status = NkString("Document illisible : rien n'a ete change.");
					return false;
				}
				doc = loaded;
				selected = 0;
				host.demoModels.Clear();
				host.SyncTo(doc);
				char b[192];
				snprintf(b, sizeof(b), "Document charge : %u noeud(s), %u composant(s) inconnu(s)",
						 doc.NodeCount(), unknown);
				status = NkString(b);
				return true;
			}
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  PANNEAU 1 — LA PALETTE
	// ═══════════════════════════════════════════════════════════════════════════
	// ⚠️ ELLE BOUCLE SUR LE REGISTRE, elle ne nomme aucun composant. Une liste
	//    ecrite en dur afficherait aujourd'hui les bons noms sans qu'aucune
	//    declaration soit lue — elle « marcherait » en ne prouvant rien, et le
	//    jour ou un second composant arrive elle ne l'afficherait pas.
	class PalettePanel : public NkEditorPanel {
		public:
			explicit PalettePanel(DesignState *st)
				: NkEditorPanel("Palette", NkEditorDockSide::NK_LEFT), mSt(st) {}

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				ec.Text("Ce que la bibliotheque declare");
				ec.Separator();

				// L'entree 0 n'est pas un composant : c'est le CADRE, un noeud qui
				// n'affiche rien et sert a agencer. Il est dans la palette parce
				// qu'une composition en a besoin avant qu'un composant conteneur
				// existe.
				if (Selectable(ctx, "Cadre (agence, n'affiche rien)", mSt->paletteChoice == 0))
					mSt->paletteChoice = 0;

				const uint16 n = NkComponentRegistry::Count();
				for (uint16 i = 0; i < n; ++i) {
					const NkComponentDecl *d = NkComponentRegistry::At(i);
					if (!d)
						continue;
					const char *label = (d->title && *d->title) ? d->title : d->name;
					if (Selectable(ctx, label, mSt->paletteChoice == (int32)i + 1))
						mSt->paletteChoice = (int32)i + 1;
				}

				ec.Separator();
				char b[192];
				snprintf(b, sizeof(b), "Cible : %s",
						 mSt->doc.IsValidIndex(mSt->selected)
							 ? mSt->doc.nodes[(uint32)mSt->selected].label.Data()
							 : "(aucune)");
				ec.Text(b);
				if (ec.Button("Poser dans la selection"))
					Place();
				ec.Separator();

				const NkComponentDecl *d = Chosen();
				if (d) {
					snprintf(b, sizeof(b), "%u parametres, %u variantes, %u jetons", d->paramCount,
							 d->variantCount, d->tokenCount);
					ec.Text(b);
					snprintf(b, sizeof(b), "%u metriques, %u greffes, %u evenements", d->metricCount,
							 d->hookCount, d->eventCount);
					ec.Text(b);
					ec.Text(d->summary);
				} else {
					ec.Text("Un cadre ne declare rien : il agence ses enfants.");
				}
			}

		private:
			const NkComponentDecl *Chosen() const {
				if (mSt->paletteChoice <= 0)
					return nullptr;
				return NkComponentRegistry::At((uint16)(mSt->paletteChoice - 1));
			}
			void Place() {
				const NkComponentDecl *d = Chosen();
				const int32 created = mSt->doc.AddChild(mSt->selected, d ? d->name : "", NkAuthor::Humain);
				if (created < 0) {
					mSt->status = NkString("Pose refusee : cible invalide ou composant inconnu.");
					return;
				}
				mSt->selected = created;
				mSt->host.SyncTo(mSt->doc);
				mSt->status = NkString("Pose : ");
				mSt->status.Append(mSt->doc.nodes[(uint32)created].label);
			}
			DesignState *mSt;
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  PANNEAU 2 — L'ARBRE DE COMPOSITION
	// ═══════════════════════════════════════════════════════════════════════════
	// « une interface complete est un composant qui en contient d'autres — meme
	// mecanisme aux deux echelles ». Cet arbre est la vue de ce mecanisme.
	class CompositionPanel : public NkEditorPanel {
		public:
			explicit CompositionPanel(DesignState *st)
				: NkEditorPanel("Composition", NkEditorDockSide::NK_LEFT), mSt(st) {}

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				ec.Text(mSt->doc.title.Data());
				ec.Separator();
				DrawNode(ctx, 0, 0);
				ec.Separator();

				// ── Les operations de structure ──────────────────────────────
				// ⚠️ TOUTES ECRIVENT UN PARENT OU UN RANG. Aucune ne deplace un
				//    rectangle : reordonner n'est pas glisser.
				if (ec.Button("Monter"))
					Reorder(-1);
				if (ec.Button("Descendre"))
					Reorder(+1);
				if (ec.Button("Rentrer dans le voisin du dessus"))
					NestIntoPreviousSibling();
				if (ec.Button("Sortir vers le grand-parent"))
					Outdent();
				if (ec.Button("Supprimer"))
					Remove();

				// ── LES METRIQUES DU DOCUMENT ────────────────────────────────
				// Une valeur, tous les noeuds qui la nomment. C'est le benefice
				// direct de la regle « un espacement se nomme » : l'aeration de
				// l'interface entiere se regle ici, pas noeud par noeud.
				ec.Separator();
				ec.Text("Metriques du document (px logiques)");
				for (uint32 i = 0; i < (uint32)mSt->doc.metrics.Size(); ++i) {
					float32 v = mSt->doc.metrics[i].value;
					if (ec.SliderFloat(mSt->doc.metrics[i].name.Data(), v, 0.f, 64.f))
						mSt->doc.metrics[i].value = v;
				}

				ec.Separator();
				char b[192];
				snprintf(b, sizeof(b), "%u noeud(s) — %u pose(s) par l'IA, %u corrige(s)",
						 mSt->doc.NodeCount(), mSt->doc.CountByAuthor(NkAuthor::IA),
						 mSt->doc.CountCorrected());
				ec.Text(b);
				if (ec.Button("Enregistrer le document"))
					mSt->SaveDoc();
				if (ec.Button("Recharger le document"))
					mSt->LoadDoc();
				if (ec.Button("Nouveau document"))
					mSt->BuildStarterDocument();
				ec.Separator();
				ec.Text(mSt->status.Data() ? mSt->status.Data() : "");
			}

		private:
			void DrawNode(NkGuiContext &ctx, int32 node, int32 depth) {
				if (!mSt->doc.IsValidIndex(node))
					return;
				const NkUINode &n = mSt->doc.nodes[(uint32)node];
				char line[256];
				char indent[32];
				int32 k = 0;
				for (; k < depth * 2 && k < 30; ++k)
					indent[k] = ' ';
				indent[k] = 0;
				// La provenance se lit DANS l'arbre : un document ou l'on ne voit pas
				// ce que la machine a pose est un document ou personne ne relit ce que
				// la machine a pose.
				const char *mark = n.prov.corrected				  ? " [ia/corrige]"
								   : (n.prov.author == NkAuthor::IA) ? " [ia]"
																	 : "";
				snprintf(line, sizeof(line), "%s%s%s%s", indent, n.label.Data(),
						 n.IsFrame() ? " (cadre)" : "", mark);
				if (Selectable(ctx, line, node == mSt->selected))
					mSt->selected = node;
				for (uint32 i = 0; i < (uint32)n.children.Size(); ++i)
					DrawNode(ctx, n.children[i], depth + 1);
			}

			int32 RankInParent(int32 node) const {
				if (!mSt->doc.IsValidIndex(node))
					return -1;
				const int32 p = mSt->doc.nodes[(uint32)node].parent;
				if (!mSt->doc.IsValidIndex(p))
					return -1;
				const NkVector<int32> &kids = mSt->doc.nodes[(uint32)p].children;
				for (uint32 i = 0; i < (uint32)kids.Size(); ++i)
					if (kids[i] == node)
						return (int32)i;
				return -1;
			}
			void Reorder(int32 delta) {
				const int32 r = RankInParent(mSt->selected);
				if (r < 0)
					return;
				if (mSt->doc.MoveChild(mSt->selected, r + delta))
					mSt->doc.MarkHumanEdit(mSt->selected);
			}
			/// Imbriquer : le noeud entre DANS son voisin du dessus. C'est le geste
			/// « poser un composant dans un autre » sous sa forme clavier ; a la
			/// souris ce sera un glisser, et il appellera exactement le meme
			/// `Reparent`.
			void NestIntoPreviousSibling() {
				const int32 r = RankInParent(mSt->selected);
				if (r <= 0)
					return;
				const int32 p = mSt->doc.nodes[(uint32)mSt->selected].parent;
				const int32 target = mSt->doc.nodes[(uint32)p].children[(uint32)(r - 1)];
				if (mSt->doc.Reparent(mSt->selected, target)) {
					mSt->doc.MarkHumanEdit(mSt->selected);
					mSt->status = NkString("Imbrique dans : ");
					mSt->status.Append(mSt->doc.nodes[(uint32)target].label);
				}
			}
			void Outdent() {
				if (!mSt->doc.IsValidIndex(mSt->selected))
					return;
				const int32 p = mSt->doc.nodes[(uint32)mSt->selected].parent;
				if (!mSt->doc.IsValidIndex(p))
					return;
				const int32 gp = mSt->doc.nodes[(uint32)p].parent;
				if (!mSt->doc.IsValidIndex(gp))
					return;
				if (mSt->doc.Reparent(mSt->selected, gp))
					mSt->doc.MarkHumanEdit(mSt->selected);
			}
			void Remove() {
				NkVector<int32> remap;
				if (!mSt->doc.RemoveSubtree(mSt->selected, &remap)) {
					mSt->status = NkString("La racine ne se supprime pas.");
					return;
				}
				// ⚠️ LA SUPPRESSION RENUMEROTE : la selection courante est perimee et
				//    doit etre reparee ICI, sinon elle designerait un autre noeud — un
				//    defaut qui ne se voit qu'a la modification suivante, donc loin de
				//    sa cause.
				mSt->selected = 0;
				mSt->host.demoModels.Clear();
				mSt->host.SyncTo(mSt->doc);
				mSt->status = NkString("Noeud supprime.");
			}
			DesignState *mSt;
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  PANNEAU 3 — L'APERCU
	// ═══════════════════════════════════════════════════════════════════════════
	// C'est le CONSOMMATEUR. Il n'affiche pas une image du document : il calcule
	// sa mise en page et appelle les fonctions de dessin memes que l'application
	// finale appellera.
	class PreviewPanel : public NkEditorPanel {
		public:
			explicit PreviewPanel(DesignState *st)
				: NkEditorPanel("Apercu", NkEditorDockSide::NK_CENTER), mSt(st) {}

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				ec.Text("Le document, dessine par le kit. Cliquez pour selectionner ;");
				ec.Text("tirez le bord droit ou bas d'un noeud pour changer sa TAILLE.");
				const NkRect area = ctx.NextItemRect(-1.f, 520.f);
				if (area.w <= 0.f || area.h <= 0.f)
					return;

				const NkPaintRect surface = {area.x, area.y, area.w, area.h};
				mSt->Recompute(surface);

				NkComponentInput in;
				in.surfaceScale = 1.f; ///< ⚠️ A BRANCHER sur le DPI reel de la surface
				in.mouseX = ctx.input.mousePos.x;
				in.mouseY = ctx.input.mousePos.y;
				in.wheel = ctx.input.wheel;
				in.mouseDown = ctx.input.mouseDown[0];
				in.mousePressed = ctx.input.mouseClicked[0];
				in.mouseReleased = ctx.input.mouseReleased[0];
				in.doubleClick = ctx.input.mouseDoubleClicked[0];
				in.rightPressed = ctx.input.mouseClicked[1];
				in.ctrl = ctx.input.ctrlDown;
				in.shift = ctx.input.shiftDown;

				HandleMouse(in);

				NkGuiComponentPaint paint(ctx, mSt->theme);
				NkDrawDocument(paint, in, mSt->doc, mSt->layout, mSt->host);

				// Le liseré de selection se peint APRES le document et n'en fait pas
				// partie : c'est du mobilier d'editeur. La sonde ne le voit pas, et
				// c'est voulu — elle mesure l'interface, pas le decor.
				if (mSt->layout.Has(mSt->selected))
					paint.OutlineSharp(mSt->layout.At(mSt->selected), NkResolveRole("AccentUi"));
			}

		private:
			// ── LA SOURIS, TRADUITE ──────────────────────────────────────────
			// ⚠️ RIEN ICI N'ECRIT UNE POSITION. Un clic ecrit une SELECTION ; un
			//    glisser de bord ecrit une TAILLE ou un POIDS (`NkResizeByDrag`).
			//    C'est la difference entre un outil de design et un constructeur
			//    d'interfaces, et elle se joue exactement dans cette fonction.
			void HandleMouse(const NkComponentInput &in) {
				const float32 kHandle = 6.f;
				if (in.mousePressed) {
					const int32 hit = NkPickNode(mSt->doc, mSt->layout, in.mouseX, in.mouseY);
					if (hit >= 0) {
						mSt->selected = hit;
						const NkPaintRect r = mSt->layout.At(hit);
						const bool nearRight = in.mouseX >= r.x + r.w - kHandle;
						const bool nearBottom = in.mouseY >= r.y + r.h - kHandle;
						if (nearRight || nearBottom) {
							mDragging = true;
							mDragHorizontal = nearRight;
							mDragNode = hit;
						}
					}
				}
				if (mDragging && in.mouseDown) {
					const float32 delta = mDragHorizontal ? in.mouseX - mLastX : in.mouseY - mLastY;
					if (delta != 0.f)
						NkResizeByDrag(mSt->doc, mSt->layout, mDragNode, mDragHorizontal, delta);
				}
				if (!in.mouseDown)
					mDragging = false;
				mLastX = in.mouseX;
				mLastY = in.mouseY;
			}

			DesignState *mSt;
			bool mDragging = false;
			bool mDragHorizontal = true;
			int32 mDragNode = -1;
			float32 mLastX = 0.f, mLastY = 0.f;
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  PANNEAU 4 — LES PROPRIETES
	// ═══════════════════════════════════════════════════════════════════════════
	// ⚠️ AUCUN WIDGET DE COMPOSANT N'EST ECRIT EN DUR ICI. La section « reglages »
	//    BOUCLE sur les tables de la declaration : ajouter un parametre au
	//    composant le fait apparaitre SANS TOUCHER A CE FICHIER. C'est ce qui
	//    distingue « lire la declaration » de « connaitre le composant » — un
	//    panneau ecrit a la main afficherait les memes curseurs aujourd'hui et
	//    mentirait des le premier ajout.
	class PropertiesPanel : public NkEditorPanel {
		public:
			explicit PropertiesPanel(DesignState *st)
				: NkEditorPanel("Proprietes", NkEditorDockSide::NK_RIGHT), mSt(st) {}

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				if (!mSt->doc.IsValidIndex(mSt->selected)) {
					ec.Text("Aucun noeud selectionne.");
					return;
				}
				NkUINode &n = mSt->doc.nodes[(uint32)mSt->selected];

				ec.Text(n.label.Data());
				ec.Separator();
				DrawProvenance(ec, n);
				ec.Separator();
				DrawSizing(ec, ctx, "Largeur", n.width);
				DrawSizing(ec, ctx, "Hauteur", n.height);
				ec.Separator();
				DrawLayout(ec, ctx, n);
				ec.Separator();
				DrawComponentSettings(ec, ctx, n);
			}

		private:
			void Edited() {
				mSt->doc.MarkHumanEdit(mSt->selected);
			}

			void DrawProvenance(NkEditorFrameContext &ec, const NkUINode &n) {
				char b[192];
				snprintf(b, sizeof(b), "Provenance : %s%s%s", NkAuthorName(n.prov.author),
						 n.prov.verified ? " · rejouee" : "", n.prov.corrected ? " · corrigee" : "");
				ec.Text(b);
				if (n.prov.origin.Length() > 0) {
					snprintf(b, sizeof(b), "Origine : %s", n.prov.origin.Data());
					ec.Text(b);
				}
				// ⚠️ AUCUNE CASE A COCHER ICI, ET C'EST DELIBERE. « rejouee » est un
				//    constat de mesure, « corrigee » se deduit d'une edition : les
				//    rendre cochables ferait entrer dans le corpus des tampons poses a
				//    la main, c'est-a-dire du bruit qui ressemble a du signal.
			}

			void DrawSizing(NkEditorFrameContext &ec, NkGuiContext &ctx, const char *title,
							NkSizeDecl &s) {
				ec.Text(title);
				for (uint8 i = 0; i < (uint8)NkSizeMode::Count; ++i)
					if (Selectable(ctx, NkSizeModeName((NkSizeMode)i), s.mode == (NkSizeMode)i)) {
						s.mode = (NkSizeMode)i;
						Edited();
					}
				if (s.mode == NkSizeMode::Fixed) {
					if (ec.SliderFloat("taille (px)", s.value, 0.f, 1200.f))
						Edited();
				} else if (s.mode == NkSizeMode::Weight) {
					if (ec.SliderFloat("poids", s.value, 0.f, 8.f))
						Edited();
				}
				if (ec.SliderFloat("min", s.minVal, 0.f, 800.f))
					Edited();
				if (ec.SliderFloat("max (<= min : non borne)", s.maxVal, 0.f, 1600.f))
					Edited();
			}

			void DrawLayout(NkEditorFrameContext &ec, NkGuiContext &ctx, NkUINode &n) {
				ec.Text("Agencement de ses enfants");
				for (uint8 i = 0; i < (uint8)NkLayoutKind::Count; ++i)
					if (Selectable(ctx, NkLayoutKindName((NkLayoutKind)i),
								   n.layout.kind == (NkLayoutKind)i)) {
						n.layout.kind = (NkLayoutKind)i;
						Edited();
					}
				// ⚠️ NI GOUTTIERE NI MARGE EN NOMBRE ICI, ET C'EST LA REGLE DU KIT :
				//    un espacement est du STYLE, il se NOMME. Le noeud designe deux
				//    metriques ; leurs VALEURS s'editent une fois pour tout le
				//    document, dans le panneau Composition. Mettre un curseur de
				//    pixels ici ferait exister la valeur a deux endroits, et l'editeur
				//    n'en changerait qu'un.
				char nm[160];
				snprintf(nm, sizeof(nm), "espacement = %s (%0.1f px)", n.spacingName.Data(),
						 mSt->doc.Metric(n.spacingName.Data()));
				ec.Text(nm);
				snprintf(nm, sizeof(nm), "remplissage = %s (%0.1f px)", n.padName.Data(),
						 mSt->doc.Metric(n.padName.Data()));
				ec.Text(nm);
				if (n.layout.kind == NkLayoutKind::Grid) {
					float32 cols = (float32)n.layout.gridColumns;
					if (ec.SliderFloat("colonnes", cols, 1.f, 8.f)) {
						n.layout.gridColumns = (uint16)(cols + 0.5f);
						Edited();
					}
				}
				ec.Text("Alignement transverse");
				for (uint8 i = 0; i < (uint8)NkAlign::Count; ++i)
					if (Selectable(ctx, NkAlignName((NkAlign)i),
								   n.layout.crossAlign == (NkAlign)i)) {
						n.layout.crossAlign = (NkAlign)i;
						Edited();
					}
			}

			void DrawComponentSettings(NkEditorFrameContext &ec, NkGuiContext &ctx, NkUINode &n) {
				const NkComponentDecl *d = n.IsFrame() ? nullptr : n.instance.Decl();
				if (!d) {
					ec.Text("Un cadre n'a pas de reglages : il agence.");
					return;
				}
				ec.Text("Representation");
				for (uint16 i = 0; i < d->variantCount; ++i) {
					const bool cur = n.instance.HasVariant() && n.instance.Variant() == (int32)i;
					if (Selectable(ctx, d->variants[i].label, cur)) {
						n.instance.SetVariant((int32)i);
						Edited();
					}
				}

				ec.Separator();
				ec.Text("Parametres");
				for (uint16 i = 0; i < d->paramCount; ++i) {
					const NkParamDecl &pd = d->params[i];
					float32 v = n.instance.Param(pd.name);
					if (pd.kind == NkParamKind::Bool) {
						bool b = v > 0.5f;
						if (ec.Checkbox(pd.label, b)) {
							n.instance.SetParam(pd.name, b ? 1.f : 0.f);
							Edited();
						}
					} else {
						// ⚠️ LES BORNES VIENNENT DE LA DECLARATION, pas de l'editeur. Un
						//    editeur qui bornerait lui-meme creerait une seconde verite,
						//    et le jour ou la declaration change, le curseur resterait
						//    sur l'ancienne.
						const float32 lo = pd.maxVal > pd.minVal ? pd.minVal : 0.f;
						const float32 hi = pd.maxVal > pd.minVal ? pd.maxVal : 1.f;
						if (ec.SliderFloat(pd.label, v, lo, hi)) {
							n.instance.SetParam(pd.name, v);
							Edited();
						}
					}
				}

				ec.Separator();
				ec.Text("Metriques (px logiques)");
				for (uint16 i = 0; i < d->metricCount; ++i) {
					const NkMetricDecl &md = d->metrics[i];
					float32 v = n.instance.Metric(md.name);
					// Plage d'edition derivee du defaut declare : faute de bornes dans
					// `NkMetricDecl`, on ouvre autour de la valeur declaree plutot que
					// d'inventer un maximum absolu.
					if (ec.SliderFloat(md.name, v, 0.f, md.defVal * 4.f + 8.f)) {
						n.instance.SetMetric(md.name, v);
						Edited();
					}
				}

				ec.Separator();
				ec.Text("Jetons de theme");
				for (uint16 i = 0; i < d->tokenCount; ++i) {
					const NkTokenDecl &td = d->tokens[i];
					char line[192];
					snprintf(line, sizeof(line), "%s = %s%s", td.name, n.instance.TokenRole(td.name),
							 n.instance.IsTokenOverridden(td.name) ? "  (modifie)" : "");
					ec.Text(line);
				}
				// ⚠️ LA REAFFECTATION D'UN JETON N'A TOUJOURS PAS DE WIDGET, et c'est
				//    une absence NOMMEE : il faudrait un selecteur de role, qui est un
				//    composant a part entiere (il en existe deja un dans NK3DModeler).
				//    L'ecrire ici en serait une copie de plus. Le mecanisme, lui, est
				//    en place et teste.

				ec.Separator();
				ec.Text("Evenements exposes (declares, non branches)");
				for (uint16 i = 0; i < d->eventCount; ++i) {
					const NkEventDecl &e = d->events[i];
					char line[256];
					int32 k = snprintf(line, sizeof(line), "%s(", e.name);
					for (uint8 a = 0; a < e.argCount && k > 0 && k < (int32)sizeof(line); ++a)
						k += snprintf(line + k, sizeof(line) - (uint32)k, "%s%s: %s", a ? ", " : "",
									  e.args[a].name, NkArgTypeName(e.args[a].kind));
					if (k > 0 && k < (int32)sizeof(line))
						snprintf(line + k, sizeof(line) - (uint32)k, ")");
					ec.Text(line);
				}
				ec.Separator();
				if (ec.Button("Reglages : tout reinitialiser")) {
					n.instance.ResetAll();
					Edited();
				}
				char b[128];
				snprintf(b, sizeof(b), "%u ecart(s) par rapport a la declaration",
						 n.instance.OverrideCount());
				ec.Text(b);
			}

			DesignState *mSt;
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  PANNEAU 5 — L'IA
	// ═══════════════════════════════════════════════════════════════════════════
	// La place, pas le modele. Ce panneau ne sait rien de ce qu'il y a derriere le
	// backend, et c'est exactement ce qui permettra de le remplacer par Ilyana
	// sans toucher a une ligne d'ici.
	class AIPanel : public NkEditorPanel {
		public:
			explicit AIPanel(DesignState *st)
				: NkEditorPanel("IA", NkEditorDockSide::NK_BOTTOM), mSt(st) {}

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				char b[256];
				snprintf(b, sizeof(b), "Backend : %s%s",
						 mSt->ai.Backend() ? mSt->ai.Backend()->Name() : "-",
						 (mSt->ai.Backend() && mSt->ai.Backend()->IsAvailable()) ? "" : " (indisponible)");
				ec.Text(b);
				ec.Text("Decrivez l'interface voulue. Elle produit une DECLARATION,");
				ec.Text("posee dans l'arbre comme si vous l'aviez posee vous-meme.");
				InputText(ctx, "Demande", mSt->promptBuf, (int32)sizeof(mSt->promptBuf));

				if (ec.Button("Demander a l'IA"))
					Ask();
				if (ec.Button("Verifier le document par rejeu"))
					Replay();

				ec.Separator();
				ec.Text(mLast.Data() ? mLast.Data() : "");
				ec.Separator();
				// ⚠️ CE QUI N'EST PAS LA, DIT DANS L'INTERFACE ELLE-MEME. Un editeur
				//    muet sur ce qu'il ne fait pas se fait reprocher des absences qu'il
				//    n'a jamais promises.
				ec.Text("Aucun modele specialise n'existe encore : il s'entrainera sur");
				ec.Text("les documents produits ici. Backend reseau : absent (le client");
				ec.Text("HTTP doit monter dans un module partage, pas etre recopie ici).");
			}

		private:
			void Ask() {
				const NkAIResult r = mSt->ai.Ask(mSt->promptBuf, mSt->doc, mSt->selected);
				char b[320];
				if (r.Accepted()) {
					snprintf(b, sizeof(b), "Acceptee : %u noeud(s) poses, rejeu conforme.", r.nodesAdded);
					mSt->host.SyncTo(mSt->doc);
					mSt->selected = r.graftedRoot;
				} else {
					snprintf(b, sizeof(b), "REFUSEE — %s. Le document n'a pas bouge.",
							 NkAIVerdictName(r.verdict));
				}
				mLast = NkString(b);
				if (r.detail.Length() > 0) {
					mLast.Append("  ");
					mLast.Append(r.detail);
				}
			}
			void Replay() {
				const uint32 diffs = NkDesignAI::ReplayDiffs(mSt->doc, mSt->ai.replaySurface);
				char b[192];
				snprintf(b, sizeof(b), "Rejeu du document : %u divergence(s)%s", diffs,
						 diffs == 0 ? " — fidele." : " — NON fidele.");
				mLast = NkString(b);
				if (diffs == 0)
					mSt->doc.MarkVerified(0);
			}
			DesignState *mSt;
			NkString mLast;
	};

} // namespace nkuidesign
