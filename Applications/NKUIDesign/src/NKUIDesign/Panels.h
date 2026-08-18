#pragma once
// -----------------------------------------------------------------------------
// @File    Panels.h
// @Brief   Les trois panneaux de NkUIDesign : la liste, l'apercu, les reglages.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  LA TRANCHE VERTICALE — ce qui est dedans, et ce qui n'y est pas
// =============================================================================
//  Rodolf, 18/08 : NkUIDesign se livre MAINTENANT, en TRANCHE VERTICALE et non
//  « la totale ». La raison est celle que ce chantier a lui-meme mesuree : une
//  declaration SANS CONSOMMATEUR deviendrait le quatrieme systeme dormant, a
//  cote de `NKReflection`, de l'interpreteur blueprint et de
//  `NkEditorInspector.h`. **Le consommateur construit en meme temps rend la
//  declaration rentable tout de suite.**
//
//  DANS LA TRANCHE, et livre ici :
//    - UN composant reel declare (le navigateur de contenu) ;
//    - l'editeur CHARGE la declaration, AFFICHE le composant, EDITE ses
//      parametres et ses jetons EN DIRECT, et SAUVE ;
//    - sur la coquille NKEditorKit existante.
//
//  ⚠️ HORS DE LA TRANCHE — nomme, differe, et volontairement absent :
//    - les **blueprints** (les evenements sont DECLARES avec leur charge, rien
//      ne s'y branche encore) ;
//    - l'**IA specialisee** ;
//    - la **creation de composants ex nihilo** (on edite ce qui est declare) ;
//    - le **canevas libre** ;
//    - les **variantes multiples** au-dela de ce qu'il faut pour prouver la
//      chaine (`columns` est declaree et rendue comme `dense_list`).
//
//  ⚠️ AUCUN TEMOIN VISUEL. Seance sans GPU : **rien de ce fichier n'a ete vu a
//     l'ecran.** Ce qui est prouve l'est par `--probe`, qui ne juge pas un
//     pixel. Le premier lancement fenetre reste a faire, et il est nomme comme
//     tel dans `ROADMAP.md`.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkComponentInstance.h"
#include "NKEditorKit/Components/NkContentBrowserModel.h"
#include "NKEditorKit/Components/NkGuiComponentPaint.h"
#include "NKEditorKit/NkEditorKit.h"
#include "NKFileSystem/NkFile.h"

namespace nkuidesign {

	using nkentseu::float32;
	using nkentseu::int32;
	using nkentseu::uint16;
	using nkentseu::uint32;
	using nkentseu::NkString;
	using namespace nkentseu::editorkit;
	using namespace nkentseu::nkgui;

	static const char *kOverridePath = "nkuidesign_content_browser.nkuicomp";

	// ── L'ETAT PARTAGE ──────────────────────────────────────────────────────
	// ⚠️ IL NE CONTIENT AUCUNE VALEUR DE REGLAGE. Les valeurs vivent dans
	//    `instance` ; les panneaux les lisent et les ecrivent par son API. C'est
	//    ce qui garantit que ce que l'apercu dessine est EXACTEMENT ce que la
	//    sauvegarde ecrit — deux copies auraient diverge des la premiere seance.
	struct DesignState {
			const NkComponentDecl *decl = nullptr;
			NkComponentInstance instance;

			NkContentBrowserModel model;
			NkContentBrowserStyle style;
			NkTheme theme;

			NkString status;
			int32 selectedComponent = 0;

			void Init() {
				theme = NkTheme::Dark();
				decl = &NkContentBrowserDecl();
				NkComponentRegistry::Register(*decl);
				instance.Bind(*decl);
				BuildDemoContent();
				ResolveStyle();
				// On tente de recharger les reglages de la seance precedente : c'est
				// la moitie « sans recompiler » du temoin, cote utilisateur.
				Load();
			}

			// ── La resolution des JETONS -> roles de theme ───────────────────
			// C'EST ICI QUE LA DEUXIEME EXIGENCE DE RODOLF DEVIENT REELLE. Le
			// composant ne connait que des jetons (« card_bg ») ; l'instance dit de
			// quel ROLE chacun herite ; `NkResolveRole` en fait un identifiant.
			// Rebrancher un jeton dans le panneau de reglages change donc la
			// couleur A L'IMAGE SUIVANTE, sans recompiler et sans que le dessin
			// sache que quelque chose a bouge.
			void ResolveStyle() {
				style.panelBg = Role("panel_bg");
				style.headerBg = Role("header_bg");
				style.border = Role("border");
				style.text = Role("text");
				style.textMuted = Role("text_muted");
				style.cardBg = Role("card_bg");
				style.cardFooterBg = Role("card_footer_bg");
				style.activeMark = Role("active_mark");
				style.chosenMark = Role("chosen_mark");
				style.folderTint = Role("folder_tint");
				style.values = &instance;
			}

			uint16 Role(const char *token) const {
				const uint16 id = NkResolveRole(instance.TokenRole(token));
				// ⚠️ ON NE SUBSTITUE PAS EN SILENCE. Un role inconnu (l'utilisateur
				//    a tape n'importe quoi dans le champ) rendrait `NK_ROLE_INVALID`,
				//    et `NkTheme::Get` peint alors du magenta franc. C'est voulu :
				//    une couleur de repli « qui va bien » ferait passer une faute de
				//    frappe pour un choix.
				return id;
			}

			void BuildDemoContent() {
				struct Row {
						const char *name;
						const char *kind;
						bool folder;
				};
				static const Row kRows[] = {
					{"Materiaux", "dossier", true},		  {"Maillages", "dossier", true},
					{"Textures", "dossier", true},		  {"caisse.nkmesh", "maillage", false},
					{"sol.nkmat", "materiau", false},	  {"bois_albedo.nktex", "texture", false},
					{"metal.nkmat", "materiau", false},	  {"perso.nkmesh", "maillage", false},
					{"ciel.nktex", "texture", false},	  {"herbe.nkmat", "materiau", false},
					{"rocher.nkmesh", "maillage", false}, {"eau.nkmat", "materiau", false},
				};
				model.entries.Clear();
				for (uint32 i = 0; i < sizeof(kRows) / sizeof(kRows[0]); ++i) {
					NkAssetEntry e;
					e.name = NkString(kRows[i].name);
					e.path = NkString("/projet/");
					e.path.Append(kRows[i].name);
					e.isFolder = kRows[i].folder;
					e.kindLabel = kRows[i].kind;
					e.kindRole = NkResolveRole(kRows[i].folder ? "TypeFolder" : "AccentUi");
					model.entries.PushBack(e);
				}
				model.breadcrumb.Clear();
				model.breadcrumb.PushBack(NkString("projet"));
				model.breadcrumb.PushBack(NkString("assets"));
				model.breadcrumb.PushBack(NkString("niveau1"));
			}

			void Save() {
				NkString out;
				instance.Save(out);
				status = nkentseu::NkFile::WriteAllText(kOverridePath, out.Data())
							 ? NkString("Enregistre : ")
							 : NkString("ECHEC d'ecriture : ");
				status.Append(kOverridePath);
			}

			void Load() {
				if (!nkentseu::NkFile::Exists(kOverridePath)) {
					status = NkString("Aucun fichier de reglages : la declaration fait foi.");
					return;
				}
				const NkString text = nkentseu::NkFile::ReadAllText(kOverridePath);
				uint32 unknown = 0, applied = 0;
				const bool ok = instance.Load(text.Data(), &unknown, &applied);
				ResolveStyle();
				// ⚠️ ON AFFICHE LE COMPTE D'INCONNUS. Un chargement « reussi » qui
				//    n'applique rien est le pire des deux mondes : l'utilisateur
				//    croit ses reglages revenus. Le compte le dit.
				char b[192];
				snprintf(b, sizeof(b), "Charge : %u applique(s), %u inconnu(s)%s", applied, unknown,
						 ok ? "" : " [en-tete manquant]");
				status = NkString(b);
			}
	};

	// ── PANNEAU 1 : LES COMPOSANTS ──────────────────────────────────────────
	// Il lit le REGISTRE, jamais une liste ecrite en dur. Une liste en dur
	// afficherait les bons noms sans qu'aucune declaration soit lue — elle
	// « marcherait » en ne prouvant rien.
	class ComponentsPanel : public NkEditorPanel {
		public:
			explicit ComponentsPanel(DesignState *st)
				: NkEditorPanel("Composants", NkEditorDockSide::NK_LEFT), mSt(st) {}

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				ec.Text("Bibliotheque NKEditorKit");
				ec.Separator();
				const uint16 n = NkComponentRegistry::Count();
				for (uint16 i = 0; i < n; ++i) {
					const NkComponentDecl *d = NkComponentRegistry::At(i);
					if (!d)
						continue;
					if (Selectable(ctx, d->title && *d->title ? d->title : d->name,
								   i == (uint16)mSt->selectedComponent))
						mSt->selectedComponent = (int32)i;
				}
				ec.Separator();
				const NkComponentDecl *d = mSt->decl;
				if (d) {
					char b[256];
					snprintf(b, sizeof(b), "%u parametres, %u variantes, %u jetons", d->paramCount,
							 d->variantCount, d->tokenCount);
					ec.Text(b);
					snprintf(b, sizeof(b), "%u metriques, %u greffes, %u evenements", d->metricCount,
							 d->hookCount, d->eventCount);
					ec.Text(b);
					ec.Separator();
					ec.Text(d->summary);
				}
				ec.Separator();
				// ⚠️ CE QUI N'EST PAS LA, DIT DANS L'INTERFACE ELLE-MEME. Un editeur
				//    muet sur ce qu'il ne fait pas se fait reprocher des absences
				//    qu'il n'a jamais promises.
				ec.Text("Hors tranche : blueprints, IA, creation ex nihilo,");
				ec.Text("canevas libre. Les evenements sont declares, pas branches.");
			}

		private:
			DesignState *mSt;
	};

	// ── PANNEAU 2 : L'APERCU ────────────────────────────────────────────────
	// C'est le CONSOMMATEUR. Il n'affiche pas une image du composant : il appelle
	// `NkDrawContentBrowser`, la fonction meme qu'une application appellera.
	class PreviewPanel : public NkEditorPanel {
		public:
			explicit PreviewPanel(DesignState *st)
				: NkEditorPanel("Apercu", NkEditorDockSide::NK_CENTER), mSt(st) {}

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				ec.Text("Le composant, dessine par le kit — pas une capture.");
				const NkRect area = ctx.NextItemRect(-1.f, 520.f);
				if (area.w <= 0.f || area.h <= 0.f)
					return;

				NkGuiComponentPaint paint(ctx, mSt->theme);

				// ── Conversion NKGui -> entree neutre du composant ───────────
				// L'hote traduit ce qu'il a. C'est exactement la vertu de la
				// structure plate : le composant est pilotable par n'importe quelle
				// source, et c'est ce qui rend `--probe` possible sans fenetre.
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

				NkContentBrowserHooks hooks;
				hooks.user = mSt;
				hooks.onSelect = &OnSelect;
				hooks.onDoubleClick = &OnDoubleClick;

				NkDrawContentBrowser(paint, in, {area.x, area.y, area.w, area.h}, mSt->model,
									 mSt->style, hooks);
			}

		private:
			// Les evenements arrivent bien jusqu'a l'application : ils ecrivent
			// dans la barre d'etat. C'est modeste, et c'est le point — le composant
			// SIGNALE, l'application decide.
			static void OnSelect(void *u, int32 i, const char *path) {
				auto *st = (DesignState *)u;
				st->status = NkString("onSelect -> ");
				st->status.Append(path ? path : "");
				(void)i;
			}
			static void OnDoubleClick(void *u, int32 i, const char *path) {
				auto *st = (DesignState *)u;
				st->status = NkString("onDoubleClick -> ");
				st->status.Append(path ? path : "");
				(void)i;
			}

			DesignState *mSt;
	};

	// ── PANNEAU 3 : LES REGLAGES ────────────────────────────────────────────
	// ⚠️ AUCUN WIDGET N'EST ECRIT EN DUR ICI. Le panneau BOUCLE sur les tables de
	//    la declaration : autant de curseurs qu'il y a de parametres declares,
	//    autant de champs qu'il y a de jetons. Ajouter un parametre au composant
	//    le fait apparaitre dans l'editeur SANS TOUCHER A CE FICHIER.
	//
	//    C'est ce qui distingue « lire la declaration » de « connaitre le
	//    composant » : un panneau ecrit a la main afficherait les memes curseurs
	//    aujourd'hui et mentirait des le premier ajout.
	class SettingsPanel : public NkEditorPanel {
		public:
			explicit SettingsPanel(DesignState *st)
				: NkEditorPanel("Reglages", NkEditorDockSide::NK_RIGHT), mSt(st) {}

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				const NkComponentDecl *d = mSt->decl;
				if (!d) {
					ec.Text("Aucun composant selectionne.");
					return;
				}

				// ── VARIANTES ────────────────────────────────────────────────
				ec.Text("Representation");
				ec.Separator();
				for (uint16 i = 0; i < d->variantCount; ++i) {
					const bool cur = mSt->instance.HasVariant() && mSt->instance.Variant() == (int32)i;
					if (Selectable(ctx, d->variants[i].label, cur))
						mSt->instance.SetVariant((int32)i);
				}

				// ── PARAMETRES ───────────────────────────────────────────────
				ec.Separator();
				ec.Text("Parametres");
				for (uint16 i = 0; i < d->paramCount; ++i) {
					const NkParamDecl &pd = d->params[i];
					float32 v = mSt->instance.Param(pd.name);
					if (pd.kind == NkParamKind::Bool) {
						bool b = v > 0.5f;
						if (ec.Checkbox(pd.label, b))
							mSt->instance.SetParam(pd.name, b ? 1.f : 0.f);
					} else {
						// ⚠️ LES BORNES VIENNENT DE LA DECLARATION, pas de l'editeur.
						//    Un editeur qui bornerait lui-meme creerait une seconde
						//    verite, et le jour ou la declaration change, le curseur
						//    resterait sur l'ancienne.
						const float32 lo = pd.maxVal > pd.minVal ? pd.minVal : 0.f;
						const float32 hi = pd.maxVal > pd.minVal ? pd.maxVal : 1.f;
						if (ec.SliderFloat(pd.label, v, lo, hi))
							mSt->instance.SetParam(pd.name, v);
					}
				}

				// ── METRIQUES ────────────────────────────────────────────────
				ec.Separator();
				ec.Text("Metriques (px logiques)");
				for (uint16 i = 0; i < d->metricCount; ++i) {
					const NkMetricDecl &md = d->metrics[i];
					float32 v = mSt->instance.Metric(md.name);
					// Plage d'edition derivee du defaut declare : faute de bornes
					// dans `NkMetricDecl`, on ouvre autour de la valeur declaree
					// plutot que d'inventer un maximum absolu.
					if (ec.SliderFloat(md.name, v, 0.f, md.defVal * 4.f + 8.f))
						mSt->instance.SetMetric(md.name, v);
				}

				// ── JETONS ───────────────────────────────────────────────────
				ec.Separator();
				ec.Text("Jetons de theme");
				for (uint16 i = 0; i < d->tokenCount; ++i) {
					const NkTokenDecl &td = d->tokens[i];
					char line[192];
					snprintf(line, sizeof(line), "%s = %s%s", td.name, mSt->instance.TokenRole(td.name),
							 mSt->instance.IsTokenOverridden(td.name) ? "  (modifie)" : "");
					ec.Text(line);
				}
				// ⚠️ LA REAFFECTATION D'UN JETON N'A PAS DE WIDGET D'EDITION ICI, et
				//    c'est une absence NOMMEE : il faudrait un selecteur de role, qui
				//    est un composant a part entiere (il en existe deja un dans
				//    NK3DModeler). L'ecrire ici en serait une copie de plus —
				//    exactement ce que ce chantier existe pour arreter. Le mecanisme,
				//    lui, est en place et teste (`--probe`, essai 7) : c'est le
				//    WIDGET qui manque, pas la chaine.

				// ── EVENEMENTS ───────────────────────────────────────────────
				ec.Separator();
				ec.Text("Evenements exposes (declares, non branches)");
				for (uint16 i = 0; i < d->eventCount; ++i) {
					const NkEventDecl &e = d->events[i];
					char line[256];
					int32 n = snprintf(line, sizeof(line), "%s(", e.name);
					for (uint8 a = 0; a < e.argCount && n > 0 && n < (int32)sizeof(line); ++a)
						n += snprintf(line + n, sizeof(line) - (uint32)n, "%s%s: %s", a ? ", " : "",
									  e.args[a].name, NkArgTypeName(e.args[a].kind));
					if (n > 0 && n < (int32)sizeof(line))
						snprintf(line + n, sizeof(line) - (uint32)n, ")");
					ec.Text(line);
				}

				// ── FICHIER ──────────────────────────────────────────────────
				ec.Separator();
				if (ec.Button("Enregistrer"))
					mSt->Save();
				if (ec.Button("Recharger"))
					mSt->Load();
				if (ec.Button("Tout reinitialiser")) {
					mSt->instance.ResetAll();
					mSt->status = NkString("Reglages remis a la declaration.");
				}
				char b[128];
				snprintf(b, sizeof(b), "%u ecart(s) par rapport a la declaration",
						 mSt->instance.OverrideCount());
				ec.Text(b);
				ec.Separator();
				ec.Text(mSt->status.Data() ? mSt->status.Data() : "");

				// La resolution des jetons se refait CHAQUE image : c'est ce qui rend
				// l'edition « en direct » sans aucun mecanisme d'invalidation. Le
				// cout est de dix recherches de chaine par image.
				mSt->ResolveStyle();
			}

		private:
			DesignState *mSt;
	};

} // namespace nkuidesign
