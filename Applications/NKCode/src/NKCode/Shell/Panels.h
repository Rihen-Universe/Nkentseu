#pragma once
// =============================================================================
// Panels.h — Panneaux de l'IDE NKCode (sur NKEditorKit / NKGui).
//   Explorateur (arbre de fichiers reel) · Editeur (onglets + saisie multi-ligne)
//   · Sortie (resultat de jenga build).
// =============================================================================
#include "NKEditorKit/NkEditorKit.h"
#include "NKCode/Project/NkCodeState.h"
#include "NKCode/Project/NkLogSink.h"
#include "NKCode/Project/NkPty.h"
#include "NKCode/Project/NkTerm.h"
#include "NKCode/Editor/NkTextDraw.h"
#include "NKCode/Editor/NkMarkdown.h" // viewer .md (preview rendu)
#include "NKCode/Editor/NkJsonView.h" // viewer .json (arbre repliable colore)
#include "NKCode/Editor/NkCsvView.h"  // viewer .csv (table)
#include "NKCode/Shell/NkI18n.h"  // NkT() : bannière mojibake traduite
#include "NKCode/Shell/NkShell.h" // NkCodeShellRun (révéler dans l'explorateur / terminal)
#include "NKCode/Shell/NkExplorer.h" // ExplorerPanel (arbre + git + filtre, maquette Banani)
#include "NKContainers/String/NkFormat.h" // NkPrintf (formatage maison)
#include "NKPlatform/NkEnv.h"			  // env::GetEnvVar (variables d'environnement maison)
#include "NKImage/NKImage.h"			  // NkImage : viewer media (image)
#include "NKCode/Shell/NkAudioViewer.h"	  // DrawAudioViewer : lecteur audio (onde + play/seek)

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu;
		using namespace nkentseu::editorkit;
		using namespace nkentseu::nkgui;

		// ── Viewer MEDIA (image/video/audio) : dessine dans `r` a la place de l'editeur
		//    quand l'onglet est un media. Image : fit-fenetre + zoom molette + pan glisser
		//    (double-clic = re-fit) sur fond DAMIER (transparence). Video/audio : placeholder. ──
		inline void DrawMediaViewer(NkGuiContext &ctx, editorkit::NkEditorShell *shell, OpenFile &f, const NkRect &r) {
			NkGuiDrawList &dl = ctx.DL();
			const NkGuiFont *font = ctx.font;
			const float32 lh = (font && font->Valid()) ? font->LineHeight() : 16.f;
			const float32 asc = (font && font->Valid()) ? font->Ascent() : 12.f;
			dl.PushClipRect(r, true);
			dl.AddRectFilled(r, NkColor{28, 30, 34, 255}); // fond
			const float32 cs = 16.f;					   // damier (transparence)
			for (float32 yy = r.y; yy < r.y + r.h; yy += cs)
				for (float32 xx = r.x; xx < r.x + r.w; xx += cs)
					if (((((int32)((xx - r.x) / cs)) + ((int32)((yy - r.y) / cs))) & 1) == 0)
						dl.AddRectFilled({xx, yy, cs, cs}, NkColor{38, 40, 45, 255});

			auto centered = [&](const char *msg, const NkColor &col) {
				if (!font || !font->Valid())
					return;
				const float32 tw = font->MeasureWidth(msg);
				dl.AddText(font->Face(), font->TexId(), {r.x + (r.w - tw) * 0.5f, r.y + (r.h - lh) * 0.5f + asc}, msg, col);
			};

			NkString info;
			const NkVec2 mp = ctx.input.mousePos;
			const bool over = NkGuiRectContains(r, mp);

			if (f.mediaKind == 1) { // ── IMAGE ──
				if (!f.mediaLoaded) {
					f.mediaLoaded = true;
					NkImage img;
					if (img.Load(f.path.ToString().CStr(), 4) && img.IsValid() && shell) {
						f.mediaW = img.Width();
						f.mediaH = img.Height();
						f.mediaTex = shell->UploadRGBA(img.Pixels(), f.mediaW, f.mediaH);
					}
				}
				if (f.mediaTex && f.mediaW > 0 && f.mediaH > 0) {
					// Zoom molette -> desactive le fit.
					if (over && ctx.input.wheel != 0.f) {
						const float32 base = f.mediaFit ? 1.f : f.mediaZoom;
						float32 z = base * (ctx.input.wheel > 0.f ? 1.15f : 1.f / 1.15f);
						if (z < 0.05f)
							z = 0.05f;
						if (z > 32.f)
							z = 32.f;
						// si on etait en fit, part de l'echelle fit courante
						if (f.mediaFit) {
							const float32 aw = r.w - 40.f, ah = r.h - 40.f;
							float32 fit = aw / f.mediaW < ah / f.mediaH ? aw / f.mediaW : ah / f.mediaH;
							if (fit > 1.f)
								fit = 1.f;
							z = fit * (ctx.input.wheel > 0.f ? 1.15f : 1.f / 1.15f);
						}
						f.mediaZoom = z;
						f.mediaFit = false;
						ctx.input.wheel = 0.f;
					}
					// Double-clic -> re-ajuster.
					if (over && ctx.input.mouseDoubleClicked[0]) {
						f.mediaFit = true;
						f.mediaPanX = f.mediaPanY = 0.f;
					}
					// Pan par glisser (etat statique par onglet).
					static const void *s_drag = nullptr;
					static NkVec2 s_m0;
					static float32 s_px0, s_py0;
					if (over && ctx.input.mouseClicked[0] && !ctx.input.mouseDoubleClicked[0]) {
						s_drag = &f;
						s_m0 = mp;
						s_px0 = f.mediaPanX;
						s_py0 = f.mediaPanY;
					}
					if (s_drag == &f) {
						if (ctx.input.mouseDown[0]) {
							f.mediaPanX = s_px0 + (mp.x - s_m0.x);
							f.mediaPanY = s_py0 + (mp.y - s_m0.y);
						} else
							s_drag = nullptr;
					}
					// Echelle effective.
					float32 scale;
					if (f.mediaFit) {
						const float32 aw = r.w - 40.f, ah = r.h - 40.f;
						scale = aw / f.mediaW < ah / f.mediaH ? aw / f.mediaW : ah / f.mediaH;
						if (scale > 1.f)
							scale = 1.f;
						f.mediaPanX = f.mediaPanY = 0.f;
					} else
						scale = f.mediaZoom;
					const float32 dw = f.mediaW * scale, dh = f.mediaH * scale;
					const NkRect disp = {r.x + (r.w - dw) * 0.5f + f.mediaPanX, r.y + (r.h - dh) * 0.5f + f.mediaPanY, dw,
										 dh};
					dl.AddImage(f.mediaTex, disp, {0.f, 0.f}, {1.f, 1.f}, NkColor{255, 255, 255, 255});
					info = NkPrintf("%s      %d x %d      %d Ko      %d%%", f.Name().CStr(), f.mediaW, f.mediaH,
									(int32)(f.mediaSize / 1024), (int32)(scale * 100.f + 0.5f));
				} else
					centered("Impossible de charger l'image", NkColor{240, 120, 110, 255});
			} else { // ── VIDEO / AUDIO : placeholder ──
				centered(f.mediaKind == 2 ? "Apercu video - lecture a venir" : "Apercu audio - lecture a venir",
						 ctx.theme.textDisabled);
				info = NkPrintf("%s      %d Ko", f.Name().CStr(), (int32)(f.mediaSize / 1024));
			}

			// Barre d'info en bas.
			if (!info.Empty() && font && font->Valid()) {
				const float32 bh = lh + 10.f;
				const NkRect bar = {r.x, r.y + r.h - bh, r.w, bh};
				dl.AddRectFilled(bar, NkColor{20, 22, 26, 225});
				dl.AddText(font->Face(), font->TexId(), {bar.x + 12.f, bar.y + 5.f + asc}, info.CStr(), ctx.theme.text);
			}
			dl.PopClipRect();
		}

		// Texte COLORE sur une ligne : reserve un rect de la LARGEUR DU TEXTE (pas
		// pleine largeur, sinon un SameLine() suivant pousse l'item hors champ) et
		// dessine `s` en `col`. Avance le curseur (nouvelle ligne par defaut).
		inline void TermText(NkGuiContext &ctx, const char *s, const NkColor &col) {
			const float32 h = ctx.ItemHeight();
			const float32 w = (ctx.font && ctx.font->Valid() && s) ? ctx.font->MeasureWidth(s) + 4.f : 40.f;
			const NkRect r = ctx.NextItemRect(w, h);
			if (ctx.font && ctx.font->Valid() && s && *s)
				ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
								 {r.x, r.y + (h - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent()}, s, col);
		}

		// ── Explorateur : ARBRE repliable facon VSCode. Les dossiers s'ouvrent/ferment
		//    en place (chevron + indentation) ; clic fichier = ouvrir dans l'editeur. ──
		inline bool SideSameStr(const char *a, const char *b) {
			while (*a && *a == *b) {
				++a;
				++b;
			}
			return *a == *b;
		}

		// ── SIDEBARS EXCLUSIVES (façon VSCode) : le panneau REMPLACE celui du côté. ──
		// Les autres panneaux du groupe partageant la MÊME feuille sont fermés ET
		// détachés (sinon la feuille accumule des onglets fantômes fermés). Un panneau
		// déplacé AILLEURS à la main (feuille différente) est indépendant : intact.
		inline void OpenSideExclusive(editorkit::NkEditorShell *sh, const char *const *titles, int32 n,
									  const char *want) {
			if (!sh)
				return;
			// Panneau FERMÉ mais encore ancré quelque part (il avait été déplacé puis
			// fermé) : rouvrir via l'icône le ramène TOUJOURS à son côté par défaut —
			// le déplacement ne vaut que tant que le panneau reste ouvert (indépendant).
			if (!sh->IsPanelOpen(want) && sh->PanelDockNode(want) >= 0)
				sh->DetachPanel(want);
			sh->FocusPanel(want); // ouvre + ancre au côté par défaut (no-op si déjà placé)
			const int32 node = sh->PanelDockNode(want);
			for (int32 i = 0; i < n; ++i) {
				if (SideSameStr(titles[i], want))
					continue;
				if (node >= 0 && sh->PanelDockNode(titles[i]) == node) {
					sh->ClosePanel(titles[i]);
					sh->DetachPanel(titles[i]);
				}
			}
		}

		inline void ToggleSideExclusive(editorkit::NkEditorShell *sh, const char *const *titles, int32 n,
										const char *want) {
			if (!sh)
				return;
			if (sh->IsPanelOpen(want)) { // re-clic sur l'icône -> le côté se replie (VSCode)
				sh->ClosePanel(want);
				sh->DetachPanel(want); // feuille vidée -> collapse -> la sidebar disparaît
				return;
			}
			OpenSideExclusive(sh, titles, n, want);
		}

		// Groupes des sidebars (titres) — partagés entre l'activity bar et les raccourcis.
		inline const char *const *SideLeftGroup(int32 &n) {
			static const char *kG[] = {"Explorateur", "Recherche",	 "Controle de version",
									   "Debogueur",	  "Live Collab", "Extensions",
									   "Profiler",	  "Structure"};
			n = 8;
			return kG;
		}

		inline const char *const *SideRightGroup(int32 &n) {
			static const char *kG[] = {"Assistant IA", "Claude Code", "Codex", "Moteur"};
			n = 4;
			return kG;
		}


		// ── Panneau STRUCTURE / OUTLINE : symboles du fichier actif (namespaces, classes,
		//    structs, enums, fonctions ; def/class Python ; titres Markdown). Clic sur un
		//    symbole -> révèle sa ligne dans l'éditeur (wantReveal). Extraction heuristique
		//    par ligne (cache tant que le fichier actif / nb de lignes ne change pas). ──
		class OutlinePanel : public NkEditorPanel {
			public:
				explicit OutlinePanel(NkCodeState *s) : NkEditorPanel("Structure", NkEditorDockSide::NK_LEFT), mS(s) {
				}

				void OnUI(NkEditorFrameContext &ec) override {
					auto &ctx = ec.Ui();
					ec.Text(NkT("outline.title"));
					ec.Separator();
					if (mS->files.Empty() || mS->active < 0 || mS->active >= static_cast<int32>(mS->files.Size())) {
						mCacheActive = -1;
						ec.Text(NkT("outline.empty"));
						return;
					}
					OpenFile &f = mS->files[mS->active];
					Rebuild(f);
					if (mSyms.Empty()) {
						ec.Text(NkT("outline.nosym"));
						return;
					}
					for (usize i = 0; i < mSyms.Size(); ++i) {
						const Sym &sy = mSyms[i];
						char lbl[320];
						int32 n = 0;
						for (int32 d = 0; d < sy.depth && n < 10; ++d) {
							lbl[n++] = ' ';
							lbl[n++] = ' ';
						}
						lbl[n++] = kKindGlyph[sy.kind];
						lbl[n++] = ' ';
						NkStrCopy(lbl + n, sizeof(lbl) - n, sy.name.CStr()); // copie bornée maison (NkText.h)
						if (Selectable(ctx, lbl, false)) {
							f.doc.curLine = (sy.line < f.doc.LineCount()) ? sy.line : f.doc.LineCount() - 1;
							f.doc.curCol = 0;
							f.doc.Collapse();
							f.doc.wantReveal = true;
						}
					}
				}

			private:
				struct Sym {
						NkString name;
						int32 line;
						int32 depth;
						int32 kind;
				}; // kind: 0 titre,1 fn,2 type,3 ns,4 enum

				static constexpr char kKindGlyph[5] = {'#', 'f', 'C', 'N', 'E'};
				NkCodeState *mS;
				NkVector<Sym> mSyms;
				int32 mCacheActive = -1;
				int32 mCacheLines = -1;

				static bool Starts(const char *s, const char *p) {
					for (; *p; ++s, ++p)
						if (*s != *p)
							return false;
					return true;
				}

				static bool IsIdent(char c) {
					return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
				}

				static bool ExtEq(const char *e, const char *w) {
					if (*e == '.')
						++e;
					for (; *e && *w; ++e, ++w) {
						char a = *e, b = *w;
						if (a >= 'A' && a <= 'Z')
							a += 32;
						if (a != b)
							return false;
					}
					return !*e && !*w;
				}

				static bool IsCtrlKw(const char *n) {
					static const char *kw[] = {"if",   "for", "while", "switch",  "catch",	  "return",		   "sizeof",
											   "else", "do",  "using", "typedef", "template", "new",		   "delete",
											   "and",  "or",  "not",   "case",	  "alignof",  "static_assert", nullptr};
					for (int32 i = 0; kw[i]; ++i) {
						const char *a = n;
						const char *b = kw[i];
						bool eq = true;
						for (; *a && *b; ++a, ++b)
							if (*a != *b) {
								eq = false;
								break;
							}
						if (eq && !*a && !*b)
							return true;
					}
					return false;
				}

				void AddName(const char *p, int32 li, int32 depth, int32 kind) {
					while (*p == ' ')
						++p;
					char nm[128];
					int32 k = 0;
					while (*p && IsIdent(*p) && k < 127)
						nm[k++] = *p++;
					nm[k] = 0;
					if (k > 0)
						mSyms.PushBack({NkString(nm), li, depth, kind});
				}

				void Rebuild(OpenFile &f) {
					const int32 lc = f.doc.LineCount();
					if (mCacheActive == mS->active && mCacheLines == lc)
						return; // cache (invalidé au changement de fichier / nb lignes)
					mCacheActive = mS->active;
					mCacheLines = lc;
					mSyms.Clear();
					const NkString ext = f.path.GetExtension();
					const bool py = ExtEq(ext.CStr(), "py") || ExtEq(ext.CStr(), "pyi") ||
									ExtEq(ext.CStr(), "jenga"); // .jenga = DSL Python
					const bool md = ExtEq(ext.CStr(), "md") || ExtEq(ext.CStr(), "markdown");
					char buf[512];
					for (int32 li = 0; li < lc; ++li) {
						const NkCodeLine &ln = f.doc.lines[li];
						int32 m = 0;
						for (usize k = 0; k < ln.Size() && m < 510; ++k)
							buf[m++] = ln[k];
						buf[m] = 0;
						ExtractLine(buf, li, py, md);
					}
				}

				void ExtractLine(const char *raw, int32 li, bool py, bool md) {
					const char *p = raw;
					int32 indent = 0;
					while (*p == ' ' || *p == '\t') {
						++p;
						indent += (*p == '\t') ? 4 : 1;
					}
					if (!*p)
						return;
					if (Starts(p, "//") || Starts(p, "/*") || *p == '*')
						return; // commentaires
					if (md) {
						if (*p == '#') {
							int32 lvl = 0;
							const char *q = p;
							while (*q == '#') {
								++lvl;
								++q;
							}
							while (*q == ' ')
								++q;
							if (*q)
								mSyms.PushBack({NkString(q), li, lvl - 1 < 0 ? 0 : lvl - 1, 0});
						}
						return;
					}
					if (py) {
						if (Starts(p, "def "))
							AddName(p + 4, li, indent / 4, 1);
						else if (Starts(p, "class "))
							AddName(p + 6, li, indent / 4, 2);
						return;
					}
					// C/C++/ObjC (et défaut)
					if (Starts(p, "namespace ")) {
						AddName(p + 10, li, 0, 3);
						return;
					}
					if (Starts(p, "class ")) {
						AddName(p + 6, li, 0, 2);
						return;
					}
					if (Starts(p, "struct ")) {
						AddName(p + 7, li, 0, 2);
						return;
					}
					if (Starts(p, "enum class ")) {
						AddName(p + 11, li, 0, 4);
						return;
					}
					if (Starts(p, "enum ")) {
						AddName(p + 5, li, 0, 4);
						return;
					}
					// Fonction : identifiant juste avant '(' ; ligne finissant par '{' (déf) ou
					// par ')' sans ';' (déf sur plusieurs lignes) ; hors mots-clés de contrôle.
					const char *paren = nullptr;
					for (const char *q = p; *q; ++q) {
						if (*q == '(') {
							paren = q;
							break;
						}
						if (*q == ';' || *q == '=')
							break;
					}
					if (!paren || paren == p)
						return;
					const char *e = paren;
					while (e > p && e[-1] == ' ')
						--e;
					const char *b = e;
					while (b > p && (IsIdent(b[-1])))
						--b;
					if (e <= b)
						return;
					char nm[128];
					int32 k = 0;
					for (const char *q = b; q < e && k < 127; ++q)
						nm[k++] = *q;
					nm[k] = 0;
					if (k == 0 || (nm[0] >= '0' && nm[0] <= '9') || IsCtrlKw(nm))
						return;
					int32 L = 0;
					while (p[L])
						++L;
					while (L > 0 && (p[L - 1] == ' ' || p[L - 1] == '\r' || p[L - 1] == '\t'))
						--L;
					const bool endsBrace = (L > 0 && p[L - 1] == '{');
					const bool endsParen = (L > 0 && p[L - 1] == ')');
					bool hasSemi = false;
					for (const char *q = p; *q; ++q)
						if (*q == ';') {
							hasSemi = true;
							break;
						}
					if (endsBrace || (endsParen && !hasSemi))
						mSyms.PushBack({NkString(nm), li, 1, 1});
				}
		};

		// ── Editeur : onglets des fichiers ouverts + saisie multi-ligne du fichier actif. ──
		// ── Panneau « Recherche » : plein texte WORKSPACE (Ctrl+Maj+F), résultats groupés par
		//    fichier, remplacement multi-fichiers. Remplace la maquette (structure conservée). ──
		class SearchPanel : public NkEditorPanel {
			public:
				explicit SearchPanel(NkCodeState *s) : NkEditorPanel("Recherche", NkEditorDockSide::NK_LEFT), mS(s) {
				}

				void OnUI(NkEditorFrameContext &ec) override {
					auto &ctx = ec.Ui();
					auto &dl = ctx.DL();
					const float32 w = ctx.ContentWidth();
					const float32 rowH = ctx.ItemHeight() + ctx.S(6.f);
					float32 x0 = ctx.layout.cursor.x, y = ctx.layout.cursor.y;
					// Ctrl+Maj+F : focus + préremplissage depuis la sélection de l'éditeur.
					if (mS->wsFocusReq) {
						mS->wsFocusReq = false;
						mFocus = mS->wsFocusField;
						if (!mS->wsPrefill.Empty()) {
							int32 i = 0;
							for (const char *q = mS->wsPrefill.CStr(); *q && i < 255; ++q)
								mQuery[i++] = *q;
							mQuery[i] = 0;
							mS->wsPrefill = NkString();
						}
					}
					// ── Champ « Rechercher » ──
					const NkRect qr = {x0, y, w, rowH};
					if (ctx.input.mouseClicked[0])
						mFocus = detail::InRect(qr, ctx.input.mousePos) ? 1 : mFocus;
					NkOverlayTextField(ctx, dl, ctx.font, qr, mQuery, sizeof(mQuery), mFocus == 1);
					y += rowH + ctx.S(4.f);
					// ── Champ « Remplacer » ──
					const NkRect rr = {x0, y, w, rowH};
					if (ctx.input.mouseClicked[0] && detail::InRect(rr, ctx.input.mousePos))
						mFocus = 2;
					NkOverlayTextField(ctx, dl, ctx.font, rr, mRepl, sizeof(mRepl), mFocus == 2);
					y += rowH + ctx.S(4.f);
					// ── Options + actions : [Aa] [mot] [Rechercher] [Tout remplacer] ──
					auto tog = [&](float32 &bx, const char *lbl, bool on, bool enabled) {
						const float32 bw =
							(ctx.font && ctx.font->Valid() ? ctx.font->MeasureWidth(lbl) : 24.f) + ctx.S(14.f);
						const NkRect b = {bx, y, bw, rowH};
						const bool hov = detail::InRect(b, ctx.input.mousePos);
						dl.AddRectFilled(b, on ? ctx.theme.accent : (hov ? ctx.theme.buttonHover : ctx.theme.button),
										 4.f);
						if (ctx.font && ctx.font->Valid())
							dl.AddText(
								ctx.font->Face(), ctx.font->TexId(),
								{b.x + ctx.S(7.f), y + (rowH - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent()},
								lbl, enabled ? ctx.theme.text : ctx.theme.textDisabled);
						bx += bw + ctx.S(5.f);
						return enabled && hov && ctx.input.mouseClicked[0] && ctx.popupDepth == 0;
					};
					float32 bx = x0;
					if (tog(bx, "Aa", mCase, true))
						mCase = !mCase;
					if (tog(bx, NkT("search.word"), mWord, true))
						mWord = !mWord;
					if (tog(bx, NkT("search.go"), false, mQuery[0] != 0))
						mS->StartWsFind(NkString(mQuery), mCase, mWord);
					if (tog(bx, NkT("search.replall"), false, !mS->wsBusy && !mS->wsResults.Empty()))
						mS->WsReplaceAll(NkString(mQuery), NkString(mRepl));
					y += rowH + ctx.S(6.f);
					// Entrée dans le champ recherche -> lance ; Échap -> défocus.
					if (mFocus == 1 && ctx.input.KeyPressed(NkGuiKey::Enter) && mQuery[0])
						mS->StartWsFind(NkString(mQuery), mCase, mWord);
					if (ctx.input.KeyPressed(NkGuiKey::Escape))
						mFocus = 0;
					// ── Statut ── (NkPrintf maison)
					const NkString st =
						mS->wsBusy ? NkPrintf("%s %d/%d", NkT("search.busy"), mS->wsScanned, mS->wsTotal)
								   : NkPrintf("%d %s / %d %s", static_cast<int32>(mS->wsResults.Size()),
											  NkT("search.results"), mS->wsFileCount, NkT("search.files"));
					// Avance le layout du shell : la LISTE en dessous profite du scroll de la fenêtre.
					ctx.layout.cursor.x = x0;
					ctx.layout.cursor.y = y;
					ctx.layout.lineStartX = x0;
					ctx.layout.curLineH = 0.f;
					ec.Text(st.CStr());
					ec.Separator();
					// ── Résultats groupés par fichier (en-tête repliable + hits cliquables) ──
					NkString cur;
					bool folded = false;
					for (usize i = 0; i < mS->wsResults.Size(); ++i) {
						const NkCodeState::WsHit &h = mS->wsResults[i];
						if (!StrEq(cur.CStr(), h.file.CStr())) {
							cur = h.file;
							int32 nf = 0;
							for (usize j = i;
								 j < mS->wsResults.Size() && StrEq(mS->wsResults[j].file.CStr(), cur.CStr()); ++j)
								++nf;
							folded = IsFolded(cur);
							const NkString hd = NkPrintf("%s %s  (%d)", folded ? ">" : "v",
														 NkPath(cur).GetFileName().CStr(), nf); // NkPrintf maison
							if (Selectable(ctx, hd.CStr(), false))
								ToggleFold(cur);
						}
						if (folded)
							continue;
						const NkString row = NkPrintf("   L%d : %s", h.line + 1, h.preview.CStr()); // NkPrintf maison
						if (Selectable(ctx, row.CStr(), false)) { // ouverture DIFFÉRÉE (poll) : jamais OpenPath au rendu
							mS->wsOpenFile = h.file;
							mS->wsOpenLine = h.line;
						}
					}
				}

			private:
				NkCodeState *mS;
				char mQuery[256] = {};
				char mRepl[256] = {};
				int32 mFocus = 0; // 1 = rechercher, 2 = remplacer
				bool mCase = false, mWord = false;
				NkVector<NkString> mFolded; // fichiers repliés dans la liste

				bool IsFolded(const NkString &f) const {
					for (usize i = 0; i < mFolded.Size(); ++i)
						if (StrEq(mFolded[i].CStr(), f.CStr()))
							return true;
					return false;
				}

				void ToggleFold(const NkString &f) {
					for (usize i = 0; i < mFolded.Size(); ++i)
						if (StrEq(mFolded[i].CStr(), f.CStr())) {
							mFolded.Erase(mFolded.Begin() + i);
							return;
						}
					mFolded.PushBack(f);
				}
		};

		class EditorPanel : public NkEditorPanel {
			public:
				EditorPanel(NkCodeState *s, NkEditorShell *shell)
					: NkEditorPanel("Editeur", NkEditorDockSide::NK_CENTER), mS(s), mShell(shell) {
				}

				void OnUI(NkEditorFrameContext &ec) override {
					auto &ctx = ec.Ui();
					// FOCUS CLAVIER GLOBAL : quand l'EXPLORATEUR a le focus-clic, l'éditeur
					// ignore le CLAVIER (sinon Ctrl+D/Suppr/Entrée tireraient des DEUX côtés
					// à la fois). La souris reste active ; un clic DANS l'éditeur reprend le
					// clavier. RAII : l'input est restauré à toute sortie de OnUI.
					struct KbShield {
							NkGuiContext *c = nullptr;
							NkGuiInput saved;
							~KbShield() {
								if (c)
									c->input = saved;
							}
					} kb;
					if (mS->explorerFocus) {
						if (ctx.input.mouseClicked[0] &&
							NkGuiRectContains(ctx.DL().CurrentClip(), ctx.input.mousePos))
							mS->explorerFocus = false; // clic dans l'éditeur : reprend le clavier
						else {
							kb.c = &ctx;
							kb.saved = ctx.input;
							ctx.input.charCount = 0;
							for (int32 k = 0; k < NkGuiInput::KeyCount; ++k) {
								ctx.input.keyDown[k] = false;
								ctx.input.keyInit[k] = false;
							}
							ctx.input.wantCopy = ctx.input.wantCut = ctx.input.wantPaste = false;
							ctx.input.wantSelectAll = false;
						}
					}
					// ── CIBLE de DRAG & DROP global : déposer des fichiers de l'explorateur
					// sur l'éditeur les OUVRE (les dossiers sont ignorés). ──
					if (mS->dragActive && ctx.input.mouseReleased[0] &&
						NkGuiRectContains(ctx.DL().CurrentClip(), ctx.input.mousePos)) {
						for (usize di = 0; di < mS->dragPaths.Size(); ++di)
							if (!NkDirectory::Exists(mS->dragPaths[di].CStr()))
								mS->OpenPath(NkPath(mS->dragPaths[di]));
					}
					// ── Drop de fichiers depuis l'OS (Explorateur Windows) : OUVRE. ──
					if (!mS->osDropPaths.Empty() &&
						NkGuiRectContains(ctx.DL().CurrentClip(),
										  {static_cast<float32>(mS->osDropX), static_cast<float32>(mS->osDropY)})) {
						for (usize di = 0; di < mS->osDropPaths.Size(); ++di)
							if (!NkDirectory::Exists(mS->osDropPaths[di].CStr()))
								mS->OpenPath(NkPath(mS->osDropPaths[di]));
						mS->osDropPaths.Clear();
						mS->osDropTtl = 0;
					}
					if (mS->files.Empty()) {
						if (mShell)
							mShell->SetFooter("NKCode", "Jenga");
						ec.Text("Ouvrez un fichier depuis l'Explorateur.");
						return;
					}

					// Bandeau d'onglets de fichiers CUSTOM (pilote par mS->active) : onglet
					// actif surligne, point "modifie", bouton X. Remplace le TabBar NKGui
					// (qui gardait son propre index et empechait la revelation au clic).
					DrawFileTabs(ctx);
					if (mS->active < 0 || mS->active >= static_cast<int32>(mS->files.Size()))
						mS->active = 0;
					if (mS->files.Empty())
						return;

					OpenFile &f = mS->files[mS->active];
					// AvailHeight()/ContentWidth() = taille du CONTENU (scrollable, ~1e9), PAS
					// la taille visible -> on borne par le rect de CLIP (zone visible du dock)
					// sinon viewH gigantesque (pas de scrollbar, barre H hors ecran).
					const NkRect clip = ctx.DL().CurrentClip();
					NkRect r = {ctx.layout.cursor.x, ctx.layout.cursor.y, ctx.ContentWidth(), ctx.AvailHeight()};
					if (r.x + r.w > clip.x + clip.w)
						r.w = clip.x + clip.w - r.x;
					if (r.y + r.h > clip.y + clip.h)
						r.h = clip.y + clip.h - r.y;

					// ── Fil d'Ariane (workspace › dossier › … › fichier) ──
					{
						const float32 bcH = DrawBreadcrumb(ctx, f, r);
						r.y += bcH;
						if (r.h > bcH)
							r.h -= bcH;
					}

					// ── Bannière « encodage double (mojibake) détecté » + bouton Réparer ──
					if (f.doc.mojibake) {
						const float32 lh = (ctx.font && ctx.font->Valid()) ? ctx.font->LineHeight() : 16.f;
						const float32 asc = (ctx.font && ctx.font->Valid()) ? ctx.font->Ascent() : 12.f;
						const float32 bh = lh + 12.f;
						const NkRect bar = {r.x, r.y, r.w, bh};
						ctx.DL().AddRectFilled(bar, NkColor{58, 46, 20, 255}); // ambre sombre
						ctx.DL().AddRectFilled({bar.x, bar.y + bh - 1.f, bar.w, 1.f}, ctx.theme.border);
						if (ctx.font && ctx.font->Valid())
							ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
											 {bar.x + 12.f, bar.y + (bh - lh) * 0.5f + asc}, NkT("edit.mojibake"),
											 NkColor{240, 210, 140, 255});
						const char *blab = NkT("edit.repairenc");
						const float32 bw =
							((ctx.font && ctx.font->Valid()) ? ctx.font->MeasureWidth(blab) : 80.f) + 24.f;
						const NkRect btn = {bar.x + bar.w - bw - 12.f, bar.y + 5.f, bw, bh - 10.f};
						const NkVec2 mm = ctx.input.mousePos;
						const bool hov = mm.x >= btn.x && mm.x < btn.x + btn.w && mm.y >= btn.y && mm.y < btn.y + btn.h;
						ctx.DL().AddRectFilled(btn, hov ? ctx.theme.buttonHover : ctx.theme.button, 4.f);
						if (ctx.font && ctx.font->Valid())
							ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
											 {btn.x + 12.f, btn.y + (btn.h - lh) * 0.5f + asc}, blab, ctx.theme.text);
						if (hov && ctx.input.mouseClicked[0])
							f.doc.RepairEncoding();
						// Bouton « Tout réparer (N) » : répare en une fois TOUS les fichiers ouverts affectés.
						const int32 nMoji = mS->CountMojibake();
						if (nMoji > 1 && ctx.font && ctx.font->Valid()) {
							const NkString alab = NkPrintf("%s (%d)", NkT("edit.repairall"), nMoji); // NkPrintf maison
							const float32 aw = ctx.font->MeasureWidth(alab.CStr()) + 24.f;
							const NkRect abtn = {btn.x - aw - 8.f, bar.y + 5.f, aw, bh - 10.f};
							const bool ahov =
								mm.x >= abtn.x && mm.x < abtn.x + abtn.w && mm.y >= abtn.y && mm.y < abtn.y + abtn.h;
							ctx.DL().AddRectFilled(abtn, ahov ? ctx.theme.buttonHover : ctx.theme.button, 4.f);
							ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
											 {abtn.x + 12.f, abtn.y + (abtn.h - lh) * 0.5f + asc}, alab.CStr(),
											 ctx.theme.text);
							if (ahov && ctx.input.mouseClicked[0])
								mS->RepairAllOpenEncodings();
						}
						r.y += bh;
						if (r.h > bh)
							r.h -= bh;
					}

					// ── Bannière « fichier supprimé / modifié en dehors de NKCode » + action ──
					if ((f.deletedOnDisk || f.changedOnDisk) && ctx.font && ctx.font->Valid()) {
						const float32 lh = ctx.font->LineHeight();
						const float32 asc = ctx.font->Ascent();
						const float32 bh = lh + 12.f;
						const NkRect bar = {r.x, r.y, r.w, bh};
						const bool del = f.deletedOnDisk;
						ctx.DL().AddRectFilled(bar, del ? NkColor{66, 30, 30, 255} : NkColor{30, 46, 66, 255});
						ctx.DL().AddRectFilled({bar.x, bar.y + bh - 1.f, bar.w, 1.f}, ctx.theme.border);
						ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
										 {bar.x + 12.f, bar.y + (bh - lh) * 0.5f + asc},
										 NkT(del ? "edit.deleted" : "edit.changed"),
										 del ? NkColor{240, 170, 170, 255} : NkColor{170, 200, 240, 255});
						const char *blab = NkT(del ? "edit.resave" : "edit.reload");
						const float32 bw = ctx.font->MeasureWidth(blab) + 24.f;
						const NkRect btn = {bar.x + bar.w - bw - 12.f, bar.y + 5.f, bw, bh - 10.f};
						const NkVec2 mm = ctx.input.mousePos;
						const bool hov = mm.x >= btn.x && mm.x < btn.x + btn.w && mm.y >= btn.y && mm.y < btn.y + btn.h;
						ctx.DL().AddRectFilled(btn, hov ? ctx.theme.buttonHover : ctx.theme.button, 4.f);
						ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
										 {btn.x + 12.f, btn.y + (btn.h - lh) * 0.5f + asc}, blab, ctx.theme.text);
						if (hov && ctx.input.mouseClicked[0]) {
							if (del)
								mS->reqSaveAs = true;
							else
								mS->ReloadActive();
						} // supprimé -> demande OÙ ré-enregistrer
						r.y += bh;
						if (r.h > bh)
							r.h -= bh;
					}

					// ── Zoom éditeur : Ctrl+molette / Ctrl+= (zoom) / Ctrl+- (dézoom) sur la police du
					//    code. Consommé AVANT l'éditeur pour ne pas défiler à la place de zoomer. ──
					if (mShell) {
						const NkVec2 zm = ctx.input.mousePos;
						const bool overEd = zm.x >= r.x && zm.x < r.x + r.w && zm.y >= r.y && zm.y < r.y + r.h;
						// Ctrl+molette -> zoom de l'ONGLET ACTIF (via le handler enregistre par NKCode).
						// (Ctrl+= / Ctrl+- / Ctrl+0 sont geres cote shell -> meme handler.)
						if (ctx.input.ctrlDown && overEd && ctx.input.wheel != 0.f) {
							mShell->NudgeCodeFontSize(ctx.input.wheel > 0.f ? 1.f : -1.f);
							ctx.input.wheel = 0.f;
						}
						// Rend la taille de l'onglet actif via le CACHE d'atlas par taille : une taille
						// deja vue = atlas pret -> revenir sur un onglet zoome n'a AUCUN « saut ». Au
						// changement d'onglet, immediate=true (rasterise des la frame suivante si pas en cache).
						const int32 act = (mS && mS->HasActive()) ? mS->active : -1;
						const bool switched = (act != mZoomLastActive);
						mZoomLastActive = act;
						const float32 sz = act >= 0 ? mS->files[act].codeZoom : 0.f;
						mShell->EnsureCodeSize(sz, switched);
						ctx.codeFont = mShell->CodeFontForSize(sz); // police de CETTE frame (cache)
					}

					// ── Picker « aller à la définition » : INPUT traité AVANT l'éditeur, et clic CONSOMMÉ
					//    -> l'éditeur dessous ne déplace pas le caret / ne démarre pas de sélection (drag). ──
					if (mS->navPickerOpen && !mS->navResults.Empty() && ctx.font && ctx.font->Valid()) {
						const float32 lh = ctx.font->LineHeight();
						const int32 n = static_cast<int32>(mS->navResults.Size());
						const int32 shown = n < 12 ? n : 12;
						const float32 rowH = lh + 8.f;
						const float32 bw = (r.w - 60.f) < 640.f ? (r.w - 60.f) : 640.f;
						const float32 headH = lh + 10.f;
						const float32 bh = headH + shown * rowH + 8.f;
						const NkRect box = {r.x + (r.w - bw) * 0.5f, r.y + 44.f, bw, bh};
						const NkVec2 mm = ctx.input.mousePos;
						for (int32 i = 0; i < shown; ++i) {
							const NkRect row = {box.x + 4.f, box.y + headH + i * rowH, bw - 8.f, rowH};
							if (mm.x >= row.x && mm.x < row.x + row.w && mm.y >= row.y && mm.y < row.y + row.h)
								mS->navPickerSel = i;
						}
						if (ctx.input.mouseClicked[0]) {
							bool onRow = false;
							for (int32 i = 0; i < shown; ++i) {
								const NkRect row = {box.x + 4.f, box.y + headH + i * rowH, bw - 8.f, rowH};
								if (mm.x >= row.x && mm.x < row.x + row.w && mm.y >= row.y && mm.y < row.y + row.h) {
									mS->NavPick(i);
									onRow = true;
									break;
								}
							}
							const bool inBox =
								mm.x >= box.x && mm.x < box.x + box.w && mm.y >= box.y && mm.y < box.y + box.h;
							if (!onRow && !inBox)
								mS->navPickerOpen = false;
							ctx.input.mouseClicked[0] =
								false; // CONSOMME : pas de déplacement caret / drag dans l'éditeur
						}
						if (ctx.input.KeyPressed(NkGuiKey::Escape))
							mS->navPickerOpen = false;
					}

					// Bascule preview/edition pour les .md (bouton haut-droite).
					const bool isMd = !f.IsMedia() && NkCodeState::EndsWithI(f.Name().CStr(), ".md");
					const bool isJson = !f.IsMedia() && NkCodeState::EndsWithI(f.Name().CStr(), ".json");
					const bool isCsv = !f.IsMedia() && (NkCodeState::EndsWithI(f.Name().CStr(), ".csv") ||
														NkCodeState::EndsWithI(f.Name().CStr(), ".tsv"));
					if (f.IsMedia()) { // MEDIA -> viewer dedie a la place de l'editeur
						if (f.mediaKind == 3) // AUDIO : onde + play/pause/seek (NKAudio natif)
							DrawAudioViewer(ctx, f, r);
						else // IMAGE / VIDEO
							DrawMediaViewer(ctx, mShell, f, r);
					} else if (isMd && f.mdPreview) { // MARKDOWN -> preview rendu
						NkDrawMarkdown(ctx, f.doc.GetText().CStr(), r, f.mdScroll);
					} else if (isJson && f.mdPreview) { // JSON -> arbre EDITABLE
						// Ctrl+Z / Ctrl+Y : undo/redo du doc (l'apercu ne passe pas par l'editeur).
						if (ctx.input.ctrlDown && !ctx.input.altDown) {
							if (ctx.input.KeyPressed(nkgui::NkGuiKey::Z)) {
								if (ctx.input.shiftDown)
									f.doc.Redo();
								else
									f.doc.Undo();
							}
							if (ctx.input.KeyPressed(nkgui::NkGuiKey::Y))
								f.doc.Redo();
						}
						const NkString jtxt = f.doc.GetText();
						NkString jnew;
						bool jchanged = false;
						NkDrawJson(ctx, &f, jtxt.CStr(), r, f.mdScroll, jnew, jchanged);
						if (jchanged) {
							f.doc.Checkpoint(3); // snapshot pre-edition -> undo/redo
							f.doc.SetText(jnew.CStr());
						}
					} else if (isCsv && f.mdPreview) { // CSV/TSV -> table
						const NkString ctxt = f.doc.GetText();
						NkDrawCsv(ctx, &f, ctxt.CStr(), r, f.mdScroll, f.mdScrollX);
					} else {
						mS->StartProjectIndex(); // index sémantique niveau projet (async, une fois)
						const NkVector<NkString> *ppDefs = mS->EffectiveDefines(
							f.path.ToString()); // macros effectives (dump compilo) -> grisage préproc exact
						CodeEditor(ctx, "##code", f.doc, r, NkLangFromExt(f.path.GetExtension().CStr()),
								   mS->projReady ? &mS->projTypes : nullptr, mS->projReady ? &mS->projFuncs : nullptr,
								   ppDefs);
					}
					// Bascule Apercu/Editer pour .md/.json/.csv — dessinee AU-DESSUS du contenu (sinon la preview la recouvre).
					if (isMd || isJson || isCsv) {
						const float32 lhh = (ctx.font && ctx.font->Valid()) ? ctx.font->LineHeight() : 16.f;
						const char *tlab = f.mdPreview ? "</> Code source"
													   : (isJson ? "Arbre JSON" : isCsv ? "Table CSV" : "Apercu rendu");
						const float32 bw = ((ctx.font && ctx.font->Valid()) ? ctx.font->MeasureWidth(tlab) : 60.f) + 22.f;
						const NkRect tbb = {r.x + r.w - bw - 16.f, r.y + 8.f, bw, lhh + 10.f};
						const NkVec2 mm = ctx.input.mousePos;
						const bool th = mm.x >= tbb.x && mm.x < tbb.x + tbb.w && mm.y >= tbb.y && mm.y < tbb.y + tbb.h;
						ctx.DL().AddRectFilled(tbb, th ? ctx.theme.buttonHover : ctx.theme.button, 6.f);
						ctx.DL().AddRect(tbb, th ? ctx.theme.accent : ctx.theme.border, 1.f);
						if (ctx.font && ctx.font->Valid())
							ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
											 {tbb.x + 11.f, tbb.y + 5.f + ctx.font->Ascent()}, tlab, ctx.theme.text);
						if (th && ctx.input.mouseClicked[0]) {
							f.mdPreview = !f.mdPreview;
							ctx.input.mouseClicked[0] = false;
						}
					}

					// ── Overlay Ctrl+clic : barre de PROGRESSION (recherche) + LISTE de toutes les
					//    occurrences (façon VSCode « aller à la définition » multi-résultats). ──
					if ((mS->navBusy || mS->navPickerOpen) && ctx.font && ctx.font->Valid()) {
						const float32 lh = ctx.font->LineHeight();
						const float32 asc = ctx.font->Ascent();
						if (mS->navBusy) {
							const float32 bw = 360.f, bh = lh + 28.f;
							const NkRect box = {r.x + (r.w - bw) * 0.5f, r.y + 14.f, bw, bh};
							ctx.DL().AddRectFilled(box, ctx.theme.header, 6.f);
							ctx.DL().AddRect(box, ctx.theme.border, 1.f);
							NkString t = NkString("Recherche de « ") + mS->navSym.CStr() + " »…";
							ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(), {box.x + 12.f, box.y + 7.f + asc},
											 t.CStr(), ctx.theme.text);
							const float32 pbY = box.y + bh - 11.f, pbW = bw - 24.f;
							ctx.DL().AddRectFilled({box.x + 12.f, pbY, pbW, 4.f}, ctx.theme.border, 2.f);
							float32 frac = mS->navTotal > 0 ? (float32)mS->navScanned / (float32)mS->navTotal : 0.15f;
							if (frac < 0.05f)
								frac = 0.05f;
							if (frac > 1.f)
								frac = 1.f;
							ctx.DL().AddRectFilled({box.x + 12.f, pbY, pbW * frac, 4.f}, ctx.theme.accent, 2.f);
						}
						if (mS->navPickerOpen && !mS->navResults.Empty()) {
							const NkVec2 mm = ctx.input.mousePos;
							const int32 n = static_cast<int32>(mS->navResults.Size());
							const int32 shown = n < 12 ? n : 12;
							const float32 rowH = lh + 8.f;
							const float32 bw = (r.w - 60.f) < 640.f ? (r.w - 60.f) : 640.f;
							const float32 headH = lh + 10.f;
							const float32 bh = headH + shown * rowH + 8.f;
							const NkRect box = {r.x + (r.w - bw) * 0.5f, r.y + 44.f, bw, bh};
							ctx.DL().AddRectFilled(box, ctx.theme.header, 8.f);
							ctx.DL().AddRect(box, ctx.theme.accent, 1.5f);
							const NkString hb =
								NkPrintf("Aller à la définition — %d résultats  (clic · Échap)", n); // NkPrintf maison
							ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(), {box.x + 12.f, box.y + 6.f + asc},
											 hb.CStr(), ctx.theme.textDisabled);
							if (ctx.input.KeyPressed(NkGuiKey::Escape))
								mS->navPickerOpen = false; // souris-primaire : survol + clic
							const NkColor selC = {ctx.theme.accent.r, ctx.theme.accent.g, ctx.theme.accent.b, 70};
							for (int32 i = 0; i < shown; ++i) {
								const NkCodeState::NavHit &hit = mS->navResults[static_cast<usize>(i)];
								const NkRect row = {box.x + 4.f, box.y + headH + i * rowH, bw - 8.f, rowH};
								const bool hov =
									mm.x >= row.x && mm.x < row.x + row.w && mm.y >= row.y && mm.y < row.y + row.h;
								if (hov)
									mS->navPickerSel = i;
								if (i == mS->navPickerSel)
									ctx.DL().AddRectFilled(row, selC, 4.f);
								NkString base = NkPath(hit.file).GetFileName();
								base += NkPrintf(":%d", hit.line + 1); // NkPrintf maison
								ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(), {row.x + 8.f, row.y + 4.f + asc},
												 base.CStr(), ctx.theme.text);
								const float32 bx = row.x + 8.f + ctx.font->MeasureWidth(base.CStr()) + 16.f;
								ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(), {bx, row.y + 4.f + asc},
												 hit.preview.CStr(), ctx.theme.textDisabled);
							}
						}
					}

					// Footer VSCode : nom du fichier (gauche) + Ln/Col + langage (droite).
					if (mShell) {
						// Chord Ctrl+K arme -> guide visible (facon VSCode « (Ctrl+K) en attente... »).
						const bool chordArmed = (f.doc.tick - f.doc.chordK <= 90);
						const bool qfEmpty = (f.doc.tick - f.doc.qfEmptyTick <= 120);
						// Statut d'action (ex. « Panneau Structure affiché ») : VISIBLE ici, pas seulement
						// dans l'en-tete du panneau Sortie. (NkPrintf maison)
						const NkString rbuf =
							NkPrintf("%s%s%s%sLn %d, Col %d     Espaces : 4     UTF-8     %s",
									 mS->status.Empty() ? "" : mS->status.CStr(), mS->status.Empty() ? "" : "      ",
									 chordArmed ? "(Ctrl+K)  0 = replier   J = deplier   I = info      " : "",
									 qfEmpty ? "(Ctrl+.)  aucune action rapide sur cette ligne      " : "",
									 f.doc.curLine + 1, f.doc.curCol + 1, LangOf(f.path));
						NkString left = f.Name();
						if (f.doc.dirty)
							left = NkString("* ") + left.CStr();
						mShell->SetFooter(left.CStr(), rbuf.CStr());

						// Infos centrees dans la barre de titre : "fichier - NKCode".
						const NkString center =
							NkPrintf("%s%s - NKCode", f.doc.dirty ? "* " : "", f.Name().CStr()); // NkPrintf maison
						mShell->SetTitleInfo(center.CStr());
					}
				}

			private:
				// Fil d'Ariane : chemin du fichier actif relatif au workspace, façon VS Code
				// (workspace › dossier › … › fichier). Dessiné en haut de la zone code ;
				// renvoie la hauteur consommée (la zone code est décalée d'autant).
				float32 DrawBreadcrumb(NkGuiContext &ctx, const OpenFile &f, const NkRect &r) {
					auto &dl = ctx.DL();
					const float32 h = ctx.ItemHeight() + ctx.S(4.f);
					dl.AddRectFilled({r.x, r.y, r.w, h}, ctx.theme.bgPrimary);
					dl.AddRectFilled({r.x, r.y + h - 1.f, r.w, 1.f}, ctx.theme.border);
					if (!ctx.font || !ctx.font->Valid())
						return h;
					const float32 by = r.y + (h - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent();

					// Strip du préfixe racine (séparateurs / et \ traités à l'identique).
					auto norm = [](char c) -> char {
						return (c == '\\') ? '/' : ((c >= 'A' && c <= 'Z') ? (char)(c + 32) : c);
					};
					const NkString full = f.path.ToString();
					const NkString base = mS->root.ToString();
					const char *fp = full.CStr();
					const char *bp = base.CStr();
					bool pref = true;
					{
						const char *a = fp;
						const char *b = bp;
						for (; *b; ++a, ++b)
							if (norm(*a) != norm(*b)) {
								pref = false;
								break;
							}
					}
					const char *rp = fp;
					if (pref) {
						rp += base.Size();
					}
					while (*rp == '/' || *rp == '\\')
						++rp;

					// Segments : nom du workspace + chemin relatif découpé.
					NkVector<NkString> segs;
					segs.PushBack(mS->root.GetFileName());
					NkString cur;
					for (const char *q = rp;; ++q) {
						if (*q == '/' || *q == '\\' || *q == '\0') {
							if (!cur.Empty()) {
								segs.PushBack(cur);
								cur = NkString();
							}
							if (!*q)
								break;
						} else {
							char cb[2] = {*q, 0};
							cur += cb;
						}
					}
					const char *chev = "\xE2\x80\xBA";
					const float32 chevW = ctx.font->MeasureWidth(chev);
					float32 x = r.x + ctx.S(12.f);
					const NkVec2 m = ctx.input.mousePos;
					for (usize i = 0; i < segs.Size(); ++i) {
						const bool last = (i + 1 == segs.Size());
						const float32 sw = ctx.font->MeasureWidth(segs[i].CStr());
						if (x + sw > r.x + r.w - ctx.S(16.f)) {
							dl.AddText(ctx.font->Face(), ctx.font->TexId(), {x, by}, "…", ctx.theme.textDisabled);
							break;
						}
						const bool hov = (m.x >= x - 2.f && m.x < x + sw + 2.f && m.y >= r.y && m.y < r.y + h);
						const NkColor col = last ? ctx.theme.text : (hov ? ctx.theme.text : ctx.theme.textDisabled);
						dl.AddText(ctx.font->Face(), ctx.font->TexId(), {x, by}, segs[i].CStr(), col);
						x += sw + ctx.S(7.f);
						if (!last) {
							dl.AddText(ctx.font->Face(), ctx.font->TexId(), {x, by}, chev, ctx.theme.textDisabled);
							x += chevW + ctx.S(7.f);
						}
					}
					return h;
				}

				// Bandeau d'onglets de fichiers (facon VSCode) : dessine chaque fichier
				// ouvert, gere clic (activer) + X (fermer), puis avance le curseur de layout
				// sous le bandeau. Pilote par mS->active (source de verite).
				void DrawFileTabs(NkGuiContext &ctx) {
					const float32 h = ctx.ItemHeight();
					const float32 x0 = ctx.layout.cursor.x, y0 = ctx.layout.cursor.y;
					const float32 fullW = ctx.ContentWidth();
					auto &dl = ctx.DL();
					dl.AddRectFilled({x0, y0, fullW, h}, ctx.theme.tabBar);
					const NkVec2 m = ctx.input.mousePos;
					float32 x = x0;
					int32 toClose = -1;
					// ── Raccourcis onglets (façon VSCode) : Ctrl+W ferme, Ctrl+Maj+T rouvre,
					// Ctrl+Tab / Ctrl+Maj+Tab cycle dans l'ordre MRU (snapshot figé pendant le cycle). ──
					// Trace de DIAGNOSTIC clavier : chaque Ctrl+<touche QoL> recue est loguee dans
					// OUTPUT ([keys]) -> permet de voir si une touche n'arrive pas jusqu'a NKGui.
					if (ctx.input.ctrlDown) {
						auto trace = [&](NkGuiKey k, const char *nm) {
							if (ctx.input.KeyPressed(k)) // NkPrintf maison
								GlobalLogBuffer().Push(
									NkPrintf("[keys] Ctrl%s+%s", ctx.input.shiftDown ? "+Maj" : "", nm));
						};
						trace(NkGuiKey::K, "K");
						trace(NkGuiKey::J, "J");
						trace(NkGuiKey::I, "I");
						trace(NkGuiKey::O, "O");
						trace(NkGuiKey::W, "W");
						trace(NkGuiKey::T, "T");
						trace(NkGuiKey::Num0, "0");
						trace(NkGuiKey::Backslash, "AntiSlash");
						trace(NkGuiKey::Space, "Espace");
					}
					if (ctx.input.KeyPressed(NkGuiKey::F8))
						GlobalLogBuffer().Push(NkString("[keys] F8"));
					if (ctx.input.KeyPressed(NkGuiKey::F12))
						GlobalLogBuffer().Push(NkString("[keys] F12"));
					if (ctx.popupDepth == 0 && ctx.input.ctrlDown) {
						if (ctx.input.KeyPressed(NkGuiKey::W) && mS->HasActive() && !mS->files[mS->active].pinned)
							toClose = mS->active;
						if (ctx.input.shiftDown && ctx.input.KeyPressed(NkGuiKey::T))
							mS->ReopenClosed();
						if (ctx.input.shiftDown &&
							ctx.input.KeyPressed(NkGuiKey::F)) { // Ctrl+Maj+F : recherche workspace
							mS->wsFocusField = 1;
							if (mShell) { // sidebar exclusive : la Recherche remplace la vue gauche courante
								int32 gN = 0;
								const char *const *g = SideLeftGroup(gN);
								OpenSideExclusive(mShell, g, gN, "Recherche");
							} else
								DockFocusWindow(ctx, "Recherche");
							mS->wsFocusReq = true;
							if (mS->HasActive() && mS->files[mS->active].doc.HasSel())
								mS->wsPrefill = mS->files[mS->active].doc.GetSelectedText();
						}
						if (ctx.input.shiftDown &&
							ctx.input.KeyPressed(NkGuiKey::H)) { // Ctrl+Maj+H : REMPLACER (workspace)
							mS->wsFocusField = 2;				 // focus direct sur le champ « Remplacer »
							if (mShell) {
								int32 gN = 0;
								const char *const *g = SideLeftGroup(gN);
								OpenSideExclusive(mShell, g, gN, "Recherche");
							} else
								DockFocusWindow(ctx, "Recherche");
							mS->wsFocusReq = true;
							if (mS->HasActive() && mS->files[mS->active].doc.HasSel())
								mS->wsPrefill = mS->files[mS->active].doc.GetSelectedText();
						}
						if (ctx.input.shiftDown &&
							ctx.input.KeyPressed(NkGuiKey::O)) { // Ctrl+Maj+O : panneau Structure
							if (mShell) {
								int32 gN = 0;
								const char *const *g = SideLeftGroup(gN);
								OpenSideExclusive(mShell, g, gN, "Structure");
								mS->status = NkString("Panneau Structure affiché");
							} else
								mS->status = DockFocusWindow(ctx, "Structure")
												 ? NkString("Panneau Structure affiché")
												 : NkString("Panneau Structure indisponible");
						}
						const bool tabFree = // Tab réservé si la barre de recherche ou la complétion le consomme
							!(mS->HasActive() &&
							  (mS->files[mS->active].doc.findOpen || mS->files[mS->active].doc.acOpen));
						if (tabFree && ctx.input.KeyPressedRepeat(NkGuiKey::Tab)) {
							if (!mMruCyc) {
								mMruSnap = mS->MruOrder();
								mMruPos = 0;
								mMruCyc = true;
							}
							const int32 n = static_cast<int32>(mMruSnap.Size());
							if (n > 1) {
								mMruPos = (mMruPos + (ctx.input.shiftDown ? n - 1 : 1)) % n;
								mS->SyncActiveTo(mMruSnap[static_cast<usize>(mMruPos)]);
							}
						}
					}
					if (mMruCyc && !ctx.input.ctrlDown)
						mMruCyc = false;	   // fin de cycle : TickMru enregistre le nouvel actif
					NkVector<NkRect> tabRects; // frontières de CETTE frame (drag réordonner)
					// ── Débordement (façon VSCode/VS) : largeurs précalculées, molette = défilement H,
					//    onglet actif auto-révélé, boutons ▾ et « + » FIXES à droite. ──
					const float32 rightW = 58.f;
					const float32 viewTabsW = fullW - rightW;
					NkVector<float32> tabWs;
					NkVector<uint8> dup;
					float32 totalW = 0.f;
					for (usize i = 0; i < mS->files.Size(); ++i) {
						const NkString nm2 = mS->files[i].Name();
						uint8 d2 = 0;
						for (usize j = 0; j < mS->files.Size() && !d2; ++j)
							if (j != i && StrEq(mS->files[j].Name().CStr(), nm2.CStr()))
								d2 = 1;
						dup.PushBack(d2);
					}
					for (usize i = 0; i < mS->files.Size(); ++i) {
						const NkString lb2 = TabLabel(i, dup[i] != 0);
						const float32 nw2 = (ctx.font && ctx.font->Valid()) ? ctx.font->MeasureWidth(lb2.CStr()) : 40.f;
						tabWs.PushBack((mS->files[i].pinned ? 13.f : 0.f) + nw2 + 14.f + 16.f + 6.f);
						totalW += tabWs[i];
					}
					// ── MULTI-RANGEES (option) : les onglets s'enroulent sur N lignes, pas de scroll. ──
					const bool multiRow = NkCodeTabRowsOn();
					NkVector<float32> tabX, tabY;
					int32 rowsN = 1;
					if (multiRow) {
						mTabScroll = 0.f;
						float32 cx2 = x0, cy2 = y0;
						for (usize i = 0; i < tabWs.Size(); ++i) {
							const float32 limit =
								(cy2 <= y0 + 0.5f) ? x0 + viewTabsW : x0 + fullW; // rangée 1 : boutons
							if (cx2 + tabWs[i] > limit && cx2 > x0 + 0.5f) {
								cx2 = x0;
								cy2 += h;
							}
							tabX.PushBack(cx2);
							tabY.PushBack(cy2);
							cx2 += tabWs[i];
						}
						rowsN = static_cast<int32>((tabY.Empty() ? 0.f : tabY[tabY.Size() - 1] - y0) / h) + 1;
						if (rowsN > 1) // fond des rangées supplémentaires
							dl.AddRectFilled({x0, y0 + h, fullW, h * (rowsN - 1)}, ctx.theme.tabBar);
					}
					const float32 barH2 = h * rowsN;
					if (!multiRow && m.y >= y0 && m.y < y0 + h && m.x >= x0 && m.x < x0 + viewTabsW &&
						ctx.input.wheel != 0.f) {
						mTabScroll -= ctx.input.wheel * 48.f; // molette sur la barre = défilement (consommée)
						ctx.input.wheel = 0.f;
					}
					if (!multiRow && mS->active != mTabLastActive && mS->active >= 0 &&
						mS->active < static_cast<int32>(tabWs.Size())) {
						mTabLastActive = mS->active; // révèle l'onglet actif quand il change
						float32 ax = 0.f;
						for (int32 j = 0; j < mS->active; ++j)
							ax += tabWs[static_cast<usize>(j)];
						const float32 aw = tabWs[static_cast<usize>(mS->active)];
						if (ax < mTabScroll)
							mTabScroll = ax;
						if (ax + aw > mTabScroll + viewTabsW)
							mTabScroll = ax + aw - viewTabsW;
					}
					const float32 tabMaxScroll = totalW > viewTabsW ? totalW - viewTabsW : 0.f;
					if (mTabScroll < 0.f)
						mTabScroll = 0.f;
					if (mTabScroll > tabMaxScroll)
						mTabScroll = tabMaxScroll;
					// Souris sur un menu OUVERT (rect de la frame precedente) -> les onglets/boutons,
					// rendus AVANT le menu, ne doivent ni survoler ni recevoir de clic a travers.
					const bool overTabMenus = (mTabMenu.open && detail::InRect(mTabMenu.rect, m)) ||
											  (mTabList.open && detail::InRect(mTabList.rect, m));
					int32 hovNow = -1;
					if (multiRow)
						dl.PushClipRect({x0, y0, fullW, barH2}, true);
					else
						dl.PushClipRect({x0, y0, viewTabsW, h}, true);
					x = x0 - mTabScroll;
					for (usize i = 0; i < mS->files.Size(); ++i) {
						OpenFile &f = mS->files[i];
						const NkString nm = TabLabel(i, dup[i] != 0);
						const float32 dotW = 16.f;
						const float32 pinW = f.pinned ? 13.f : 0.f; // icône épingle en tête
						const float32 tabW = tabWs[i];
						const float32 nameW = tabW - pinW - 14.f - dotW - 6.f;
						const NkRect tab = {multiRow ? tabX[i] : x, multiRow ? tabY[i] : y0, tabW, h};
						const bool active = (static_cast<int32>(i) == mS->active);
						const bool hov = m.x >= tab.x && m.x < tab.x + tab.w && m.y >= tab.y && m.y < tab.y + tab.h &&
										 (multiRow || m.x < x0 + viewTabsW) && !overTabMenus;
						if (hov)
							hovNow = static_cast<int32>(i);
						dl.AddRectFilled(tab,
										 active ? ctx.theme.tabActive : (hov ? ctx.theme.tabHover : ctx.theme.tab));
						if (active)
							dl.AddRectFilled({tab.x, tab.y + h - 2.f, tab.w, 2.f}, ctx.theme.accent);
						float32 tx = tab.x + 8.f;
						if (f.pinned) {
							DrawPin(dl, {tx, tab.y + h * 0.5f}, active ? ctx.theme.accent : ctx.theme.textDisabled);
							tx += pinW;
						}
						if (ctx.font && ctx.font->Valid())
							dl.AddText(ctx.font->Face(), ctx.font->TexId(),
									   {tx, tab.y + (h - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent()},
									   nm.CStr(), active ? ctx.theme.text : ctx.theme.textDisabled, nameW);
						// Zone droite : épingle -> pas de X ; sinon point "modifié" (si dirty non survolé) sinon X.
						const NkRect cl = {tab.x + tabW - dotW - 5.f, tab.y + (h - dotW) * 0.5f, dotW, dotW};
						const bool clHov = m.x >= cl.x && m.x < cl.x + cl.w && m.y >= cl.y && m.y < cl.y + cl.h;
						if (f.pinned) {
							if (f.doc.dirty)
								dl.AddCircleFilled({cl.x + cl.w * 0.5f, cl.y + cl.h * 0.5f}, 4.f, ctx.theme.text);
						} else if (f.doc.dirty && !clHov) {
							dl.AddCircleFilled({cl.x + cl.w * 0.5f, cl.y + cl.h * 0.5f}, 4.f, ctx.theme.text);
						} else {
							if (clHov)
								dl.AddRectFilled(cl, ctx.theme.buttonHover);
							const float32 cx = cl.x + cl.w * 0.5f, cy = cl.y + cl.h * 0.5f, a = 3.5f;
							dl.AddLine({cx - a, cy - a}, {cx + a, cy + a}, ctx.theme.text, 1.2f);
							dl.AddLine({cx - a, cy + a}, {cx + a, cy - a}, ctx.theme.text, 1.2f);
						}
						tabRects.PushBack(tab);
						if (ctx.input.mouseClicked[0] && hov) {
							if (clHov && !f.pinned)
								toClose = static_cast<int32>(i);
							else {
								mS->active = static_cast<int32>(i);
								mDragTab = static_cast<int32>(i); // arme le drag (réordonner)
								mDragX = m.x;
								mDragMoved = false;
							}
						}
						// Clic-molette (bouton milieu) = fermer (sauf épinglé).
						if (ctx.input.mouseClicked[2] && hov && !f.pinned)
							toClose = static_cast<int32>(i);
						// Clic droit = menu contextuel de l'onglet.
						if (ctx.input.mouseClicked[1] && hov && ctx.popupDepth == 0) {
							mTabMenu.open = true;
							mTabMenu.pos = m;
							mTabMenuIdx = static_cast<int32>(i);
						}
						// Séparateur NET entre onglets (légèrement en retrait, bien visible).
						dl.AddRectFilled(
							{tab.x + tabW - 1.f, tab.y + 4.f, 1.f, h - 8.f},
							NkColor{ctx.theme.textDisabled.r, ctx.theme.textDisabled.g, ctx.theme.textDisabled.b, 120});
						x += tabW;
					}
					// ── Drag d'onglet : réordonne en LIVE quand la souris franchit un onglet voisin ──
					if (mDragTab >= 0) {
						if (!ctx.input.mouseDown[0] || mDragTab >= static_cast<int32>(tabRects.Size())) {
							mDragTab = -1;
						} else {
							if (!mDragMoved && (m.x - mDragX > 8.f || mDragX - m.x > 8.f))
								mDragMoved = true;
							if (mDragMoved) {
								int32 tgt = mDragTab;
								for (int32 j = 0; j < static_cast<int32>(tabRects.Size()); ++j)
									if (m.x >= tabRects[static_cast<usize>(j)].x &&
										m.x < tabRects[static_cast<usize>(j)].x + tabRects[static_cast<usize>(j)].w &&
										m.y >= tabRects[static_cast<usize>(j)].y && // multi-rangées : la BONNE ligne
										m.y < tabRects[static_cast<usize>(j)].y + tabRects[static_cast<usize>(j)].h)
										tgt = j;
								if (tgt != mDragTab) {
									mS->MoveTab(mDragTab, tgt);
									mDragTab = tgt;
								}
							}
						}
					}
					dl.PopClipRect();
					// ── Bouton ▾ (façon Visual Studio) : liste déroulante de TOUS les fichiers ouverts. ──
					const NkRect ddB = {x0 + viewTabsW + 2.f, y0 + (h - 22.f) * 0.5f, 24.f, 22.f};
					{
						const bool dHov =
							m.x >= ddB.x && m.x < ddB.x + ddB.w && m.y >= ddB.y && m.y < ddB.y + ddB.h && !overTabMenus;
						if (dHov)
							dl.AddRectFilled(ddB, ctx.theme.buttonHover, ctx.theme.rounding);
						const float32 cx = ddB.x + ddB.w * 0.5f, cy = ddB.y + ddB.h * 0.5f, a = 4.f;
						dl.AddTriangleFilled({cx - a, cy - a * 0.5f}, {cx + a, cy - a * 0.5f}, {cx, cy + a * 0.9f},
											 ctx.theme.textDisabled);
						if (totalW > viewTabsW) // témoin discret : des onglets débordent
							dl.AddCircleFilled({ddB.x + ddB.w - 4.f, ddB.y + 4.f}, 2.f, ctx.theme.accent);
						if (dHov && ctx.input.mouseClicked[0] && ctx.popupDepth == 0) {
							mTabListLabels.Clear();
							for (usize i = 0; i < mS->files.Size(); ++i) {
								NkString lb2 = TabLabel(i, dup[i] != 0);
								if (mS->files[i].doc.dirty)
									lb2 += " \xE2\x97\x8F"; // point « modifié »
								mTabListLabels.PushBack(lb2);
							}
							mTabList.open = true;
							mTabList.pos = {ddB.x, y0 + h};
							// CONSOMME le clic d'ouverture : le menu s'ouvre SOUS le bouton (souris hors boîte)
							// et l'interpréterait sinon comme un clic extérieur -> fermeture immédiate.
							ctx.input.mouseClicked[0] = false;
						}
					}
					if (mTabList.open && !mTabListLabels.Empty()) { // clic = activer (le menu scrolle V/H au besoin)
						const char *items2[64];
						bool en2[64];
						const int32 n2 = static_cast<int32>(mTabListLabels.Size()) < 64
											 ? static_cast<int32>(mTabListLabels.Size())
											 : 64;
						for (int32 i = 0; i < n2; ++i) {
							items2[i] = mTabListLabels[static_cast<usize>(i)].CStr();
							en2[i] = true;
						}
						const int32 act2 = NkCtxMenuDraw(ctx, mTabList, items2, en2, n2);
						if (act2 >= 0 && act2 < static_cast<int32>(mS->files.Size()))
							mS->active = act2;
					}
					// ── « Ouvrir dans le terminal » : choix du SHELL (défaut / PowerShell / CMD / Bash /
					//    WSL-Ubuntu). WSL démarre dans le dossier demandé (wsl.exe traduit le cwd Windows). ──
					if (mTermPick.open && !mTermPickDir.Empty()) {
						const char *items3[5] = {NkT("term.shdefault"), "PowerShell", "CMD", "Bash", "WSL (Ubuntu)"};
						const bool en3[5] = {true, true, true, true, true};
						const int32 act3 = NkCtxMenuDraw(ctx, mTermPick, items3, en3, 5);
						if (act3 >= 0) {
							static const int32 kMap[5] = {-1, 0, 3, 2,
														  1}; // -1 défaut, SH_PWSH, SH_CMD, SH_BASH, SH_WSL
							mS->termOpenKind = kMap[act3];
							mS->termOpenAt = mTermPickDir;
							mTermPickDir = NkString();
							mTabMenu.open = false; // choix fait : referme le menu principal aussi
							if (mShell)
								mShell->FocusPanel("Terminal");
						}
					}
					// Tooltip : CHEMIN COMPLET après ~0,6 s de survol (désambiguïsation totale).
					if (hovNow == mTabHovIdx && hovNow >= 0)
						mTabHovTime += ctx.input.dt;
					else {
						mTabHovIdx = hovNow;
						mTabHovTime = 0.f;
					}
					if (mTabHovIdx >= 0 && mTabHovIdx < static_cast<int32>(mS->files.Size()) && mTabHovTime > 0.6f &&
						!mTabMenu.open && !mTabList.open && ctx.font && ctx.font->Valid()) {
						const NkString full = mS->files[static_cast<usize>(mTabHovIdx)].path.ToString();
						if (!full.Empty()) {
							const float32 tw2 = ctx.font->MeasureWidth(full.CStr()) + 16.f;
							float32 tx2 = m.x + 10.f;
							if (tx2 + tw2 > static_cast<float32>(ctx.viewW))
								tx2 = static_cast<float32>(ctx.viewW) - tw2;
							const NkRect tt = {tx2, y0 + barH2 + 3.f, tw2, ctx.font->LineHeight() + 8.f};
							ctx.dlOverlay.AddRectFilled(tt, NkColor{32, 38, 46, 250}, 4.f);
							ctx.dlOverlay.AddRect(tt, NkColor{60, 66, 74, 255}, 1.f);
							ctx.dlOverlay.AddText(ctx.font->Face(), ctx.font->TexId(),
												  {tt.x + 8.f, tt.y + 4.f + ctx.font->Ascent()}, full.CStr(),
												  NkColor{223, 223, 223, 255});
						}
					}
					// Bouton « + » (nouvel onglet vierge) — FIXE à droite, après le ▾.
					const NkRect plus = {x0 + viewTabsW + 30.f, y0 + (h - 22.f) * 0.5f, 24.f, 22.f};
					const bool pHov = m.x >= plus.x && m.x < plus.x + plus.w && m.y >= plus.y &&
									  m.y < plus.y + plus.h && !overTabMenus;
					if (pHov)
						dl.AddRectFilled(plus, ctx.theme.buttonHover, ctx.theme.rounding);
					{
						const float32 cx = plus.x + plus.w * 0.5f, cy = plus.y + plus.h * 0.5f, a = 5.f;
						dl.AddLine({cx - a, cy}, {cx + a, cy}, ctx.theme.textDisabled, 1.4f);
						dl.AddLine({cx, cy - a}, {cx, cy + a}, ctx.theme.textDisabled, 1.4f);
					}
					if (pHov && ctx.input.mouseClicked[0] && ctx.popupDepth == 0) {
						mS->NewUntitled();
						mS->reqSaveAs = true;
					} // + : demande direct où enregistrer (projet/dossier/extension)

					// ── Menu contextuel de l'onglet (clic droit) ──
					if (mTabMenu.open && mTabMenuIdx >= 0 && mTabMenuIdx < static_cast<int32>(mS->files.Size())) {
						OpenFile &tf = mS->files[mTabMenuIdx];
						const char *items[8] = {NkT("tab.close"),	   NkT("tab.closeothers"),
												NkT("tab.closeright"), tf.pinned ? NkT("tab.unpin") : NkT("tab.pin"),
												NkT("tab.copypath"),   NkT("ctx.reveal"),
												NkT("ctx.openterm"),   NkT("tab.multirow")};
						const bool en[8] = {!tf.pinned, true, true, true, true, true, true, true};
						int32 tabMenuHov = -1;
						static const bool kSub[8] = {false, false, false, false,
													 false, false, true,  false}; // ▸ sur « Ouvrir dans le terminal »
						const int32 act = NkCtxMenuDraw(ctx, mTabMenu, items, en, 8, &tabMenuHov, kSub);
						// SOUS-MENU « Ouvrir dans le terminal » : s'ouvre au SURVOL de l'item, à sa droite.
						if (mTabMenu.open && tabMenuHov == 6 && ctx.font && ctx.font->Valid()) {
							const float32 rowH2 = ctx.font->LineHeight() + 8.f;
							mTermPickDir = mS->files[mTabMenuIdx].path.GetParent().ToString();
							mTermPick.open = true;
							mTermPick.pos = {mTabMenu.rect.x + mTabMenu.rect.w - 2.f,
											 mTabMenu.rect.y + 4.f + 6.f * rowH2 - mTabMenu.sy};
						}
						// referme le sous-menu si on survole un AUTRE item ou qu'on quitte les deux boîtes
						if (mTermPick.open) {
							const bool overPick = detail::InRect({mTermPick.rect.x - 10.f, mTermPick.rect.y - 10.f,
																  mTermPick.rect.w + 20.f, mTermPick.rect.h + 20.f},
																 m);
							if ((tabMenuHov >= 0 && tabMenuHov != 6 && !overPick) || (!mTabMenu.open && !overPick))
								mTermPick.open = false;
						}
						if (act >= 0) {
							const int32 idx = mTabMenuIdx;
							const NkString full = mS->files[idx].path.ToString();
							switch (act) {
								case 0:
									if (!mS->files[idx].pinned)
										mS->CloseFile(idx);
									break;
								case 1:
									mS->CloseOthers(idx);
									break;
								case 2:
									mS->CloseToRight(idx);
									break;
								case 3:
									mS->TogglePin(idx);
									break;
								case 4:
									ctx.SetClipboard(full.CStr());
									break;
								case 5:
									RevealInExplorer(full);
									break;
								case 6: // Ouvrir dans le TERMINAL INTÉGRÉ : le SOUS-MENU (survol) choisit le shell ;
									// un clic direct = shell PAR DÉFAUT immédiatement.
									mS->termOpenKind = -1;
									mS->termOpenAt = mS->files[idx].path.GetParent().ToString();
									mTermPick.open = false;
									if (mShell)
										mShell->FocusPanel("Terminal");
									break;
								case 7: // onglets multi-rangées (option, façon Visual Studio)
									NkCodeTabRowsOn() = !NkCodeTabRowsOn();
									break;
							}
							mTabMenuIdx = -1;
						}
					} else if (!mTabMenu.open)
						mTabMenuIdx = -1;

					// Avance le curseur de layout SOUS le bandeau (l'editeur suit dessous).
					ctx.layout.cursor.x = x0;
					ctx.layout.cursor.y = y0 + barH2;
					ctx.layout.lineStartX = x0;
					ctx.layout.curLineH = 0.f;
					if (toClose >= 0)
						mS->CloseFile(toClose);
				}

				// Petite épingle vectorielle (tête + aiguille) centrée verticalement en `c`.
				static void DrawPin(NkGuiDrawList &dl, const NkVec2 &c, const NkColor &col) {
					dl.AddCircleFilled({c.x + 2.f, c.y - 2.f}, 3.f, col);
					dl.AddLine({c.x + 2.f, c.y - 2.f}, {c.x - 2.f, c.y + 4.f}, col, 1.4f);
				}

				// Libellé d'onglet : nom + « · dossier parent » quand un AUTRE onglet porte le même nom
				// (désambiguïsation projet/module, façon VSCode).
				NkString TabLabel(usize i, bool dup) const {
					NkString lb = mS->files[i].Name();
					if (dup) {
						const NkString par = mS->files[i].path.GetParent().GetFileName();
						if (!par.Empty()) {
							lb += " \xC2\xB7 ";
							lb += par.CStr();
						}
					}
					return lb;
				}

				// Révèle un FICHIER dans le gestionnaire de fichiers (le sélectionne).
				static void RevealInExplorer(const NkString &path) {
#ifdef _WIN32
					NkString bs;
					for (const char *p = path.CStr(); *p; ++p)
						bs += (*p == '/') ? '\\' : *p;
					NkCodeShellRun((NkString("explorer /select,\"") + bs + "\"").CStr());
#elif defined(__APPLE__)
					NkCodeShellRun((NkString("open -R \"") + path + "\"").CStr());
#else
					NkCodeShellRun(
						(NkString("xdg-open \"") + NkPath(path.CStr()).GetParent().ToString().CStr() + "\"").CStr());
#endif
				}

				static void NkCodeShellRunTermAt(const NkString &folder) {
#ifdef _WIN32
					NkString bs;
					for (const char *p = folder.CStr(); *p; ++p)
						bs += (*p == '/') ? '\\' : *p;
					NkCodeShellRun((NkString("start \"\" cmd /K cd /d \"") + bs + "\"").CStr());
#elif defined(__APPLE__)
					NkCodeShellRun((NkString("open -a Terminal \"") + folder + "\"").CStr());
#else
					NkCodeShellRun((NkString("(x-terminal-emulator --working-directory=\"") + folder +
									"\" || gnome-terminal --working-directory=\"" + folder + "\") &")
									   .CStr());
#endif
				}

				// Langage devine a partir de l'extension (affiche dans le footer).
				static const char *LangOf(const NkPath &p) {
					const NkString e = p.GetExtension();
					const char *x = e.CStr();
					if (StrEq(x, ".cpp") || StrEq(x, ".cc") || StrEq(x, ".cxx") || StrEq(x, ".h") || StrEq(x, ".hpp"))
						return "C++";
					if (StrEq(x, ".c"))
						return "C";
					if (StrEq(x, ".py"))
						return "Python";
					if (StrEq(x, ".jenga"))
						return "Jenga";
					if (StrEq(x, ".md"))
						return "Markdown";
					if (StrEq(x, ".json"))
						return "JSON";
					if (StrEq(x, ".txt"))
						return "Texte";
					return "Texte";
				}

				NkCodeState *mS;
				NkEditorShell *mShell;
				NkCtxMenu mTabMenu;		 // menu contextuel de la barre d'onglets (clic droit)
				int32 mTabMenuIdx = -1;	 // onglet ciblé par le menu
				int32 mDragTab = -1;	 // onglet en cours de drag (réordonner)
				float32 mDragX = 0.f;	 // x souris au press (seuil anti-clic)
				bool mDragMoved = false; // seuil franchi -> réordonne en live
				bool mMruCyc = false;	 // cycle Ctrl+Tab en cours (snapshot figé)
				NkVector<NkString> mMruSnap;
				int32 mMruPos = 0;
				float32 mTabScroll = 0.f;  // défilement horizontal de la barre d'onglets
				int32 mTabLastActive = -1; // auto-révélation de l'onglet actif
				NkCtxMenu mTermPick;	   // « Ouvrir dans le terminal » : choix du shell
				NkString mTermPickDir;
				NkCtxMenu mTabList; // bouton ▾ : liste déroulante des fichiers ouverts
				NkVector<NkString> mTabListLabels;
				int32 mTabHovIdx = -1; // tooltip chemin complet (survol prolongé)
				float32 mTabHovTime = 0.f;
				int32 mZoomLastActive = -1; // dernier onglet actif (detection changement -> rebuild atlas immediat,
											// anti « saut » de taille)
		};

		// ── OUTPUT : VRAI affichage NKLogger (logs du moteur) + sortie du build jenga.
		//    Draine le tampon de logs partage, colore par niveau, suit le bas. ──
		class OutputPanel : public NkEditorPanel {
			public:
				explicit OutputPanel(NkCodeState *s) : NkEditorPanel("OUTPUT", NkEditorDockSide::NK_BOTTOM), mS(s) {
				}

				void OnUI(NkEditorFrameContext &ec) override {
					auto &ctx = ec.Ui();
					auto &dl = ctx.DL();
					mS->PollBuild();
					// Draine les nouveaux logs + sortie build, en parsant la progression.
					NkVector<NkString> fresh;
					GlobalLogBuffer().Drain(fresh);
					for (usize i = 0; i < mS->output.Size(); ++i)
						fresh.PushBack(mS->output[i]);
					mS->output.Clear();
					for (usize i = 0; i < fresh.Size(); ++i) {
						ParseProgress(fresh[i].CStr());
						mLogs.PushBack(fresh[i]);
					}
					if (!fresh.Empty())
						mFollow = true;
					while (mLogs.Size() > 5000)
						mLogs.Erase(mLogs.Begin());

					const NkRect clip = dl.CurrentClip();
					dl.AddRectFilled(clip, ctx.theme.bgPrimary); // fond #0D1117

					NkCodeFontScope _cfs(ctx); // police monospace (box-drawing + unicode)
					const float32 lineH = (ctx.font && ctx.font->Valid()) ? ctx.font->LineHeight() : 16.f;
					const NkVec2 m = ctx.input.mousePos;
					auto inR = [&](const NkRect &r) {
						return m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h;
					};

					// ── En-tete : progression (barre + %) a gauche, bouton Effacer a droite ──
					const float32 hdrH = ctx.S(22.f), pad = 6.f;
					const NkRect hdr = {clip.x, clip.y, clip.w, hdrH};
					dl.AddRectFilled(hdr, ctx.theme.header);
					const float32 by = clip.y + (hdrH - lineH) * 0.5f + (ctx.font ? ctx.font->Ascent() : 11.f);
					// Bouton Effacer (a droite).
					const float32 clrW = ctx.S(74.f);
					const NkRect clrR = {clip.x + clip.w - clrW - 4.f, clip.y + 2.f, clrW, hdrH - 4.f};
					{
						const bool h = inR(clrR);
						dl.AddRectFilled(clrR, h ? ctx.theme.buttonHover : ctx.theme.button, 4.f);
						if (ctx.font && ctx.font->Valid())
							dl.AddText(ctx.font->Face(), ctx.font->TexId(), {clrR.x + 10.f, by}, "Effacer",
									   ctx.theme.text);
						if (h && ctx.input.mouseClicked[0] && ctx.popupDepth == 0) {
							mLogs.Clear();
							ClearSel();
						}
					}
					// Progression : barre + pourcentage (pendant/juste apres un build).
					if (mS->buildTotal > 0 || mS->IsBuilding()) {
						const char *sp = "|/-\\";
						mSpin += ctx.input.dt;
						const char spc[2] = {sp[((int)(mSpin * 8.f)) & 3], 0};
						float32 x = clip.x + pad;
						if (mS->IsBuilding() && ctx.font && ctx.font->Valid()) {
							dl.AddText(ctx.font->Face(), ctx.font->TexId(), {x, by}, spc, ctx.theme.accent);
							x += ctx.S(16.f);
						}
						const float32 barW = ctx.S(160.f), barH = ctx.S(8.f);
						const NkRect bar = {x, clip.y + (hdrH - barH) * 0.5f, barW, barH};
						dl.AddRectFilled(bar, ctx.theme.button, 3.f);
						const float32 prog = mS->BuildProgress();
						dl.AddRectFilled({bar.x, bar.y,
										  barW * (prog < 0.f   ? 0.f
												  : prog > 1.f ? 1.f
															   : prog),
										  barH},
										 ctx.theme.accent, 3.f);
						const NkString info = NkPrintf("  %d/%d  (%d%%)", mS->buildDone, mS->buildTotal,
													   (int)(prog * 100.f + 0.5f)); // NkPrintf maison
						if (ctx.font && ctx.font->Valid())
							dl.AddText(ctx.font->Face(), ctx.font->TexId(), {bar.x + barW + 4.f, by}, info.CStr(),
									   ctx.theme.text);
					} else if (ctx.font && ctx.font->Valid()) {
						dl.AddText(ctx.font->Face(), ctx.font->TexId(), {clip.x + pad, by},
								   mS->status.Empty() ? "Sortie" : mS->status.CStr(), ctx.theme.textDisabled);
					}

					// ── Console (lecture seule) : defilable + selectionnable + unicode ──
					const NkRect out = {clip.x, clip.y + hdrH, clip.w, clip.h - hdrH};
					// Clic droit -> menu Copier / Tout selectionner / Effacer.
					if (ctx.input.mouseClicked[1] && inR(out) && ctx.popupDepth == 0) {
						mMenu.open = true;
						mMenu.pos = m;
					}
					DrawConsole(ctx, out, lineH, pad);
					const char *items[] = {"Copier", "Tout selectionner", "Effacer"};
					const bool en[] = {HasSel(), !mLogs.Empty(), !mLogs.Empty()};
					const int32 act = NkCtxMenuDraw(ctx, mMenu, items, en, 3);
					if (act == 0)
						CopySel(ctx);
					else if (act == 1)
						SelectAll();
					else if (act == 2) {
						mLogs.Clear();
						ClearSel();
					}
				}

			private:
				// Couleur d'une ligne de sortie : par niveau de log [E]/[W]/[D] ET par
				// mots-cles de build (error/warning/✓/✗/success...) -> coloration riche.
				static NkColor LogColor(const char *s) {
					const NkColor red = {248, 81, 73, 255}, yellow = {210, 153, 34, 255}, green = {63, 185, 80, 255},
								  gray = {110, 118, 129, 255}, blue = {88, 166, 255, 255}, white = {204, 204, 204, 255};
					// Niveau NKLogger : "[E.../[C.../[F..." etc.
					if (s[0] == '[') {
						const char c = s[1];
						if (c == 'E' || c == 'C' || c == 'F')
							return red;
						if (c == 'W')
							return yellow;
						if (c == 'D' || c == 'T')
							return gray;
					}
					if (s[0] == '$')
						return blue; // commande echo "$ jenga ..."
					if (FindI(s, "error") || FindI(s, "failed") || FindI(s, "echec") || FindI(s, "\xE2\x9C\x97"))
						return red; // error/failed/✗
					if (FindI(s, "warning") || FindI(s, "attention") || FindI(s, "\xE2\x9A\xA0"))
						return yellow; // warning/⚠
					if (FindI(s, "success") || FindI(s, "built:") || FindI(s, "completed") || FindI(s, "\xE2\x9C\x93"))
						return green; // success/built/✓
					return white;
				}

				// Recherche de sous-chaine, insensible a la casse.
				static bool FindI(const char *h, const char *n) {
					for (; *h; ++h) {
						const char *a = h;
						const char *b = n;
						while (*a && *b) {
							char x = *a, y = *b;
							if (x >= 'A' && x <= 'Z')
								x += 32;
							if (y >= 'A' && y <= 'Z')
								y += 32;
							if (x != y)
								break;
							++a;
							++b;
						}
						if (!*b)
							return true;
					}
					return false;
				}

				static bool Find(const char *h, const char *n) {
					for (; *h; ++h) {
						const char *a = h, *b = n;
						while (*a && *b && *a == *b) {
							++a;
							++b;
						}
						if (!*b)
							return true;
					}
					return false;
				}

				void ParseProgress(const char *L) {
					if (const char *p = FindP(L, "Build Order (")) {
						int n = 0;
						for (const char *q = p; *q >= '0' && *q <= '9'; ++q)
							n = n * 10 + (*q - '0');
						if (n > 0) {
							mS->buildTotal = n;
							mS->buildDone = 0;
						}
					} else if (Find(L, "Built:") && !Find(L, "Projects")) {
						++mS->buildDone;
					}
				}

				static const char *FindP(const char *h, const char *n) {
					for (; *h; ++h) {
						const char *a = h, *b = n;
						while (*a && *b && *a == *b) {
							++a;
							++b;
						}
						if (!*b)
							return a;
					}
					return nullptr;
				}

				bool HasSel() const {
					return mSAL != mSBL || mSAC != mSBC;
				}

				void ClearSel() {
					mSAL = mSAC = mSBL = mSBC = 0;
				}

				void SelectAll() {
					mSAL = 0;
					mSAC = 0;
					mSBL = (int32)mLogs.Size() - 1;
					mSBC = mLogs.Empty() ? 0 : (int32)mLogs[mLogs.Size() - 1].Size();
				}

				void CopySel(NkGuiContext &ctx) {
					int32 aL = mSAL, aC = mSAC, bL = mSBL, bC = mSBC;
					if (aL > bL || (aL == bL && aC > bC)) {
						int32 tl = aL, tc = aC;
						aL = bL;
						aC = bC;
						bL = tl;
						bC = tc;
					}
					const int32 n = (int32)mLogs.Size();
					NkVector<char> buf;
					for (int32 l = aL; l <= bL && l < n; ++l) {
						if (l < 0)
							continue;
						const char *s = mLogs[l].CStr();
						const int32 ln = (int32)mLogs[l].Size();
						const int32 c0 = (l == aL) ? aC : 0;
						int32 c1 = (l == bL) ? bC : ln;
						if (c1 > ln)
							c1 = ln;
						for (int32 c = (c0 < 0 ? 0 : c0); c < c1; ++c)
							buf.PushBack(s[c]);
						if (l < bL)
							buf.PushBack('\n');
					}
					buf.PushBack('\0');
					if (buf.Size() > 1)
						ctx.SetClipboard(buf.Data());
				}

				// Rend les lignes (clippees) + selection + scrollbars V/H + suivi du bas.
				void DrawConsole(NkGuiContext &ctx, const NkRect &out, float32 lineH, float32 pad) {
					auto &dl = ctx.DL();
					const bool sbLight =
						((int32)ctx.theme.bgPrimary.r + ctx.theme.bgPrimary.g + ctx.theme.bgPrimary.b) > 384;
					const NkColor kTrk = sbLight ? NkColor{0, 0, 0, 20} : NkColor{255, 255, 255, 16};
					const NkColor kThb = sbLight ? NkColor{168, 176, 185, 255} : NkColor{80, 88, 98, 255};
					const NkColor kThbH = sbLight ? NkColor{130, 138, 148, 255} : NkColor{120, 130, 142, 255};
					const float32 sbW = 14.f;
					const NkFont *face = (ctx.font && ctx.font->Valid()) ? ctx.font->Face() : nullptr;
					const float32 viewW = out.w - sbW - pad * 2.f, viewH = out.h - sbW;
					const float32 topPad = lineH, botPad = lineH;
					const int32 nLines = (int32)mLogs.Size();
					// Largeur max (cache incremental) pour la barre H.
					while (mMeasured < mLogs.Size()) {
						const float32 w = face ? face->CalcTextSizeX(mLogs[mMeasured].CStr()) : 0.f;
						if (w > mMaxW)
							mMaxW = w;
						++mMeasured;
					}
					const float32 contentH = nLines * lineH + topPad + botPad;
					const float32 maxSY = contentH > viewH ? contentH - viewH : 0.f;
					const float32 maxSX = mMaxW > viewW ? mMaxW - viewW : 0.f;
					const NkVec2 m = ctx.input.mousePos;
					auto in = [&](const NkRect &r) {
						return m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h;
					};
					if (in(out)) {
						if (ctx.input.wheel != 0.f) {
							mScrollY -= ctx.input.wheel * lineH * 3.f;
							ctx.input.wheel = 0.f;
							mFollow = false;
						}
						if (ctx.input.wheelH != 0.f) {
							mScrollX -= ctx.input.wheelH * 40.f;
							ctx.input.wheelH = 0.f;
						}
					}
					if (mFollow)
						mScrollY = maxSY;
					if (mScrollY < 0.f)
						mScrollY = 0.f;
					if (mScrollY > maxSY)
						mScrollY = maxSY;
					if (mScrollX < 0.f)
						mScrollX = 0.f;
					if (mScrollX > maxSX)
						mScrollX = maxSX;

					// Selection souris.
					const float32 left = out.x + pad;
					auto colAtX = [&](int32 L, float32 x) -> int32 {
						if (!face || L < 0 || L >= nLines)
							return 0;
						const char *s = mLogs[L].CStr();
						const int32 n = (int32)mLogs[L].Size();
						int32 bc = 0;
						float32 best = 1e9f;
						for (int32 c = 0; c <= n; ++c) {
							float32 d = face->CalcTextSizeX(s, s + c) - x;
							if (d < 0)
								d = -d;
							if (d < best) {
								best = d;
								bc = c;
							}
						}
						return bc;
					};
					auto rowAtY = [&](float32 y) -> int32 {
						int32 L = (int32)((y - out.y - topPad + mScrollY) / lineH);
						if (L < 0)
							L = 0;
						if (L >= nLines)
							L = nLines - 1;
						return L;
					};
					const NkRect selArea = {out.x, out.y, out.w - sbW, viewH};
					if (ctx.input.mouseClicked[0] && in(selArea) && ctx.popupDepth == 0 && !mMenu.open) {
						int32 L = rowAtY(m.y);
						mSAL = mSBL = L;
						mSAC = mSBC = colAtX(L, m.x - left + mScrollX);
						mDragging = true;
					}
					if (mDragging && ctx.input.mouseDown[0]) {
						int32 L = rowAtY(m.y);
						mSBL = L;
						mSBC = colAtX(L, m.x - left + mScrollX);
					}
					if (!ctx.input.mouseDown[0])
						mDragging = false;
					if (ctx.input.wantCopy && HasSel())
						CopySel(ctx);
					if (ctx.input.wantSelectAll && in(out))
						SelectAll();
					int32 nAL = mSAL, nAC = mSAC, nBL = mSBL, nBC = mSBC;
					if (nAL > nBL || (nAL == nBL && nAC > nBC)) {
						int32 tl = nAL, tc = nAC;
						nAL = nBL;
						nAC = nBC;
						nBL = tl;
						nBC = tc;
					}

					// Lignes visibles.
					dl.PushClipRect({out.x, out.y, out.w - sbW, viewH}, true);
					int32 first = (int32)((mScrollY - topPad) / lineH);
					if (first < 0)
						first = 0;
					const int32 last = first + (int32)(viewH / lineH) + 2;
					const float32 asc = ctx.font ? ctx.font->Ascent() : 12.f;
					for (int32 i = first; i <= last && i < nLines; ++i) {
						if (i < 0)
							continue;
						const float32 ytop = out.y + topPad + i * lineH - mScrollY;
						if (HasSel() && i >= nAL && i <= nBL) {
							const char *s = mLogs[i].CStr();
							const int32 n = (int32)mLogs[i].Size();
							const int32 c0 = (i == nAL) ? nAC : 0, c1 = (i == nBL) ? nBC : n;
							const float32 x0 = left - mScrollX + (face ? face->CalcTextSizeX(s, s + c0) : 0.f);
							float32 x1 = left - mScrollX + (face ? face->CalcTextSizeX(s, s + c1) : 0.f);
							if (i < nBL)
								x1 += 4.f;
							dl.AddRectFilled({x0, ytop, x1 - x0, lineH}, NkColor{31, 111, 235, 90});
						}

						const char *L = mLogs[i].CStr();
						NkDrawTextU(ctx, left - mScrollX, ytop + asc, ytop, lineH, L, L + mLogs[i].Size(), LogColor(L));
					}
					dl.PopClipRect();

					// Scrollbars V + H avec fleches.
					auto arrow = [&](const NkRect &r, int32 dir) -> bool {
						const bool h = in(r);
						if (h)
							dl.AddRectFilled(r, ctx.theme.button);
						const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f, a = 3.2f;
						const NkColor c = h ? kThbH : kThb;
						if (dir == 0)
							dl.AddTriangleFilled({cx, cy - a}, {cx - a, cy + a}, {cx + a, cy + a}, c);
						else if (dir == 1)
							dl.AddTriangleFilled({cx - a, cy - a}, {cx + a, cy - a}, {cx, cy + a}, c);
						else if (dir == 2)
							dl.AddTriangleFilled({cx - a, cy}, {cx + a, cy - a}, {cx + a, cy + a}, c);
						else
							dl.AddTriangleFilled({cx - a, cy - a}, {cx + a, cy}, {cx - a, cy + a}, c);
						return h && ctx.input.mouseDown[0];
					};
					const NkRect vT = {out.x + out.w - sbW, out.y, sbW, viewH};
					const NkRect hT = {out.x, out.y + viewH, out.w - sbW, sbW};
					dl.AddRectFilled(vT, kTrk);
					dl.AddRectFilled(hT, kTrk);
					dl.AddRectFilled({vT.x, hT.y, sbW, sbW}, kTrk);
					{
						const NkRect up = {vT.x, vT.y, sbW, sbW}, dn = {vT.x, vT.y + viewH - sbW, sbW, sbW};
						const NkRect iv = {vT.x, vT.y + sbW, sbW, viewH - 2.f * sbW};
						if (arrow(up, 0)) {
							mScrollY -= lineH * 0.8f;
							mFollow = false;
						}
						if (arrow(dn, 1))
							mScrollY += lineH * 0.8f;
						if (maxSY > 0.f && iv.h > 8.f) {
							float32 th = iv.h * (viewH / contentH);
							if (th < 24.f)
								th = 24.f;
							if (th > iv.h)
								th = iv.h;
							const float32 ty = iv.y + (mScrollY / maxSY) * (iv.h - th);
							if (ctx.input.mouseClicked[0] && in(iv))
								ctx.activeId = ctx.GetId("##ovbar");
							const bool a = (ctx.activeId == ctx.GetId("##ovbar"));
							if (a && ctx.input.mouseDown[0]) {
								const float32 u = (m.y - iv.y - th * 0.5f) / (iv.h - th);
								mScrollY = (u < 0 ? 0 : u > 1 ? 1 : u) * maxSY;
								mFollow = false;
							}
							dl.AddRectFilled({iv.x + 3.f, ty, sbW - 6.f, th}, (a || in(iv)) ? kThbH : kThb, 3.f);
						}
					}
					{
						const NkRect lf = {hT.x, hT.y, sbW, sbW}, rt = {hT.x + hT.w - sbW, hT.y, sbW, sbW};
						const NkRect ih = {hT.x + sbW, hT.y, hT.w - 2.f * sbW, sbW};
						if (arrow(lf, 2))
							mScrollX -= 18.f;
						if (arrow(rt, 3))
							mScrollX += 18.f;
						if (maxSX > 0.f && ih.w > 8.f) {
							float32 tw = ih.w * (viewW / mMaxW);
							if (tw < 24.f)
								tw = 24.f;
							if (tw > ih.w)
								tw = ih.w;
							const float32 tx = ih.x + (mScrollX / maxSX) * (ih.w - tw);
							if (ctx.input.mouseClicked[0] && in(ih))
								ctx.activeId = ctx.GetId("##ohbar");
							const bool a = (ctx.activeId == ctx.GetId("##ohbar"));
							if (a && ctx.input.mouseDown[0]) {
								const float32 u = (m.x - ih.x - tw * 0.5f) / (ih.w - tw);
								mScrollX = (u < 0 ? 0 : u > 1 ? 1 : u) * maxSX;
							}
							dl.AddRectFilled({tx, hT.y + 3.f, tw, sbW - 6.f}, (a || in(ih)) ? kThbH : kThb, 3.f);
						}
					}
					if (mScrollY < 0.f)
						mScrollY = 0.f;
					if (mScrollY > maxSY)
						mScrollY = maxSY;
					if (mScrollX < 0.f)
						mScrollX = 0.f;
					if (mScrollX > maxSX)
						mScrollX = maxSX;
					if (mScrollY >= maxSY - 1.f)
						mFollow = true;
				}

				NkCodeState *mS;
				NkVector<NkString> mLogs;
				float32 mScrollX = 0.f, mScrollY = 0.f, mMaxW = 0.f, mSpin = 0.f;
				usize mMeasured = 0;
				bool mFollow = true, mDragging = false;
				int32 mSAL = 0, mSAC = 0, mSBL = 0, mSBC = 0;
				NkCtxMenu mMenu;
		};

		// ── Terminal MULTI-SHELL facon VSCode : plusieurs terminaux internes
		//    (PowerShell / WSL / cmd / Jenga), onglets + bouton "+" + selecteur de
		//    shell. Chaque commande est routee vers le shell de l'onglet. Fond noir,
		//    invite coloree, execution ASYNC -> sortie en flux. ──
		class TerminalPanel : public NkEditorPanel {
			public:
				enum Shell { SH_PWSH = 0, SH_WSL, SH_BASH, SH_CMD, SH_JENGA, SH_DOCKER, SH_COUNT };

				// Entree du selecteur de shell : un type (kind) + un libelle + une distro
				// WSL optionnelle. La liste est construite dynamiquement (distros WSL2 reelles).
				struct ShellDef {
						int32 kind;
						NkString label;
						NkString distro;
						NkString cmd; // commande explicite (pwsh 7, Git Bash...) ; vide = PtyCommand(kind)
				};

				TerminalPanel() : NkEditorPanel("TERMINAL", NkEditorDockSide::NK_BOTTOM) {
					// Le terminal par defaut est cree quand le WORKSPACE est connu (bon repertoire,
					// pas de persistance de session ; l historique = celui du shell, ex. PSReadLine).
				}

				NkEditorShell *mShell = nullptr; // pour la police propre du terminal (TermCodeFont)
				NkCodeState *mState = nullptr;	 // racine du workspace -> repertoire de demarrage des shells
				float32 mZoom = 0.f;			 // taille PROPRE du terminal (0 = globale), zoom au survol

				void OnUI(NkEditorFrameContext &ec) override {
					auto &ctx = ec.Ui();
					auto &dl = ctx.DL();
					const NkRect clip = dl.CurrentClip();
					dl.AddRectFilled(clip, ctx.theme.bgPrimary); // fond terminal #0D1117
					// ── CIBLE de DRAG & DROP global : déposer des fichiers/dossiers de
					// l'explorateur COLLE leurs chemins (quotés) dans le shell actif. ──
					if (mState && mState->dragActive && ctx.input.mouseReleased[0] &&
						NkGuiRectContains(clip, ctx.input.mousePos) && mActive >= 0 && mActive < 8 &&
						mTerm[mActive].alive) {
						Term &dt = mTerm[mActive];
						for (usize di = 0; di < mState->dragPaths.Size(); ++di) {
							NkString q = "\"";
							q += mState->dragPaths[di];
							q += "\" ";
							dt.pty.Write(q.CStr(), q.Size());
						}
						dt.touched = true;
					}
					// ── Drop de fichiers depuis l'OS : colle les chemins quotés. ──
					if (mState && !mState->osDropPaths.Empty() &&
						NkGuiRectContains(clip, {static_cast<float32>(mState->osDropX),
												 static_cast<float32>(mState->osDropY)}) &&
						mActive >= 0 && mActive < 8 && mTerm[mActive].alive) {
						Term &dt = mTerm[mActive];
						for (usize di = 0; di < mState->osDropPaths.Size(); ++di) {
							NkString q = "\"";
							q += mState->osDropPaths[di];
							q += "\" ";
							dt.pty.Write(q.CStr(), q.Size());
						}
						dt.touched = true;
						mState->osDropPaths.Clear();
						mState->osDropTtl = 0;
					}
					// Changement de WORKSPACE : recycle les terminaux JAMAIS utilises (touched=false)
					// pour que le defaut redemarre dans la NOUVELLE racine (celle du boot peut etre
					// un faux workspace : le CWD de l exe). Ceux ou l on a tape restent en vie.
					const bool wsReady = mState && mState->HasWorkspace() && !mState->root.ToString().Empty();
					const NkString wsRoot = wsReady ? mState->root.ToString() : NkString();
					if (wsReady && !StrEq(mSpawnedRoot.CStr(), wsRoot.CStr())) {
						for (int32 i = 0; i < 8; ++i)
							if (mTerm[i].alive && !mTerm[i].touched) {
								mTerm[i].pty.Stop();
								mTerm[i].alive = false;
								mTerm[i].started = false;
							}
						mSpawnedRoot = wsRoot;
						if (!mTerm[mActive].alive)
							mActive = FirstAlive();
					}
					// Terminal PAR DEFAUT : cree seulement quand la racine du workspace est connue
					// -> le shell demarre au bon endroit, avec le TYPE choisi par l utilisateur.
					if (AliveCount() == 0) {
						const bool rootReady = wsReady;
						if (!rootReady) {
							if (ctx.font && ctx.font->Valid())
								dl.AddText(ctx.font->Face(), ctx.font->TexId(),
										   {clip.x + ctx.S(12.f), clip.y + ctx.S(20.f)},
										   "Ouvre un workspace pour demarrer le terminal...", ctx.theme.textDisabled);
							return;
						}
						EnsurePrefs();
						AddTermKind(mDefShell, mDefDistro); // shell par defaut (preference persistee)
					}
					// « Ouvrir dans le terminal » : NOUVEL onglet au dossier demandé (façon VSCode).
					if (mState && !mState->termOpenAt.Empty()) {
						EnsurePrefs();
						const int32 k2 = mState->termOpenKind >= 0 ? mState->termOpenKind : mDefShell;
						AddTermKind(k2, k2 == SH_WSL ? mDefDistro : NkString());
						mState->termOpenKind = -1;
						mTerm[mActive].cwd = mState->termOpenAt;
						if (!mState->termOpenCmd.Empty()) { // agent CLI : la commande EST le « shell »
							mTerm[mActive].cmdOverride = mState->termOpenCmd;
							mTerm[mActive].label = mState->termOpenCmd;
							mState->termOpenCmd = NkString();
						}
						mState->termOpenAt = NkString();
					}
					if (!mTerm[mActive].alive)
						mActive = FirstAlive();
					Term &t = mTerm[mActive];

					// Disposition VSCode : terminal a GAUCHE, LISTE des terminaux a DROITE.
					const float32 listW = ctx.S(190.f);
					const NkRect mainR = {clip.x, clip.y, clip.w - listW, clip.h};
					const NkRect listR = {clip.x + clip.w - listW, clip.y, listW, clip.h};
					DrawTermList(ctx, listR);

					// A partir d'ici : police MONOSPACE du TERMINAL (atlas propre, taille globale fixe :
					// decouple du zoom par-onglet de l'editeur).
					NkCodeFontScope _cfs(ctx, mShell ? mShell->TermCodeFont() : nullptr);

					// Lance le shell (ConPTY) au premier affichage de cet onglet.
					StartTerm(t);
					// Recupere la sortie brute et la passe a l'emulateur VT.
					mDrain.Clear();
					t.pty.Drain(mDrain);
					if (mDrain.Size() > 0)
						t.screen.Feed(mDrain.Data(), mDrain.Size());

					const NkVec2 m = ctx.input.mousePos;
					const bool inMain =
						m.x >= mainR.x && m.x < mainR.x + mainR.w && m.y >= mainR.y && m.y < mainR.y + mainR.h;
					const bool inClip =
						m.x >= clip.x && m.x < clip.x + clip.w && m.y >= clip.y && m.y < clip.y + clip.h;

					// Zoom du TERMINAL : Ctrl+molette quand la souris SURVOLE le terminal ajuste SA taille
					// (indépendant de l'éditeur). On consomme la molette pour ne pas défiler en même temps.
					if (mShell) {
						if (ctx.input.ctrlDown && inMain && ctx.input.wheel != 0.f) {
							float32 s =
								(mZoom > 0.f ? mZoom : mShell->TermSize()) + (ctx.input.wheel > 0.f ? 1.f : -1.f);
							if (s < 8.f)
								s = 8.f;
							if (s > 40.f)
								s = 40.f;
							mZoom = s;
							ctx.input.wheel = 0.f;
						}
						mShell->RequestTermSize(mZoom); // pilote l'atlas terminal à SA taille (debounce)
					}

					// Focus clavier : clic gauche dans la zone -> focus ; clic hors panneau -> defocus.
					if (ctx.input.mouseClicked[0] && ctx.popupDepth == 0) {
						if (inMain) {
							mFocused = true;
							NkCodeFocusId() = NKGUI_ID_NONE;
						} else if (!inClip)
							mFocused = false;
					}
					// Clic droit dans la zone -> menu contextuel Copier/Coller.
					if (ctx.input.mouseClicked[1] && inMain && ctx.popupDepth == 0) {
						mMenu.open = true;
						mMenu.pos = m;
						mFocused = true;
					}

					const float32 lineH = (ctx.font && ctx.font->Valid()) ? ctx.font->LineHeight() : 16.f;
					const float32 pad = 6.f;
					if (mainR.h > lineH)
						DrawGrid(ctx, t, mainR, lineH, pad);

					// ── Clavier : frappes routees vers le pty (pas de boite de saisie) ──
					// Ctrl+F / Ctrl+H : ouvre la recherche DANS le terminal (lecture seule -> pas de remplacement).
					if (mFocused && ctx.input.ctrlDown && ctx.popupDepth == 0 &&
						(ctx.input.KeyPressed(nkgui::NkGuiKey::F) || ctx.input.KeyPressed(nkgui::NkGuiKey::H)))
						mFindOpen = true;
					if (mFindOpen)
						DrawTermFind(ctx, t, mainR);
					if (mFocused && !mMenu.open && !mFindOpen && ctx.popupDepth == 0)
						RouteKeyboard(ctx, t);

					// ── Menu contextuel (overlay) ──
					const char *items[] = {"Copier", "Coller", "Tout selectionner"};
					const bool en[] = {t.HasSel(), true, true};
					const int32 act = NkCtxMenuDraw(ctx, mMenu, items, en, 3);
					if (act == 0)
						CopySelection(ctx, t);
					else if (act == 1)
						PasteClipboard(ctx, t);
					else if (act == 2)
						SelectAll(t);
				}

				// Actions sur la BARRE D'ONGLETS (a droite) quand TERMINAL est l'onglet actif :
				// bouton "+" + combobox de shell. Apparait sur la meme ligne que OUTPUT/TERMINAL.
				void OnTabBarActions(NkGuiContext &ctx, const NkRect &bar) noexcept override {
					auto &dl = ctx.DL();
					const float32 h = bar.h;
					const NkVec2 m = ctx.input.mousePos;
					auto inR = [&](const NkRect &r) {
						return m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h;
					};
					// Bouton "+" (a l'extreme droite).
					const NkRect addR = {bar.x + bar.w - h - 2.f, bar.y + 1.f, h, h - 2.f};
					{
						const bool hov = inR(addR);
						if (hov)
							dl.AddRectFilled(addR, ctx.theme.buttonHover);
						const float32 cx = addR.x + h * 0.5f, cy = addR.y + (h - 2.f) * 0.5f, a = 5.f;
						dl.AddRectFilled({cx - a, cy - 1.f, 2.f * a, 2.f}, ctx.theme.text);
						dl.AddRectFilled({cx - 1.f, cy - a, 2.f, 2.f * a}, ctx.theme.text);
						if (hov && ctx.input.mouseClicked[0] && ctx.popupDepth == 0)
							AddTerm(mNewShell);
					}
					// Combobox de shell (a gauche du "+").
					const float32 cw = ctx.S(178.f); // combo elargi (colle au bouton +)
					// Etoile « definir par defaut » : le shell du combo devient le TERMINAL PAR DEFAUT
					// (persiste). Pleine (accent) quand le combo == defaut courant.
					{
						EnsurePrefs();
						EnsureBaseShells();
						if (mNewShell < 0 || mNewShell >= static_cast<int32>(mShells.Size()))
							mNewShell = 0;
						const NkRect stR = {addR.x - cw - 1.f - h, bar.y + 1.f, h, h - 2.f};
						const bool hov = inR(stR);
						const ShellDef &cur = mShells[mNewShell];
						const bool isDef = (cur.kind == mDefShell) && StrEq(cur.distro.CStr(), mDefDistro.CStr());
						if (hov)
							dl.AddRectFilled(stR, ctx.theme.buttonHover);
						// Vraie ETOILE (« définir par défaut ») : pleine (accent) si c'est le shell
						// par défaut, sinon grisée. Éventail de triangles sur 5 pointes.
						const float32 scx = stR.x + h * 0.5f, scy = stR.y + (h - 2.f) * 0.5f;
						const NkColor sc = isDef ? ctx.theme.accent : ctx.theme.textDisabled;
						static const float32 SUX[10] = {0.000f,  0.225f,  0.951f, 0.363f,  0.588f,
														0.000f,  -0.588f, -0.363f, -0.951f, -0.225f};
						static const float32 SUY[10] = {-1.000f, -0.309f, -0.309f, 0.118f,  0.809f,
														0.382f,  0.809f,  0.118f, -0.309f, -0.309f};
						const float32 sr = 6.5f;
						for (int32 k = 0; k < 10; ++k)
							dl.AddTriangleFilled({scx, scy}, {scx + SUX[k] * sr, scy + SUY[k] * sr},
												 {scx + SUX[(k + 1) % 10] * sr, scy + SUY[(k + 1) % 10] * sr}, sc);
						if (hov && ctx.input.mouseClicked[0] && ctx.popupDepth == 0) {
							mDefShell = cur.kind;
							mDefDistro = cur.distro;
							SavePrefs();
						}
					}
					// ââ Replier (chevron) + maximiser la REGION du panneau bas, a gauche de l'etoile ââ
					if (mShell) {
						const float32 starX = addR.x - cw - 1.f - h;
						const float32 bh2 = h - 4.f;
						const int32 rmode = mShell->PanelRegionMode();
						// Maximiser / restaurer.
						const NkRect maxR = {starX - bh2 - 6.f, bar.y + 2.f, bh2, bh2};
						const bool mh = inR(maxR);
						if (mh)
							dl.AddRectFilled(maxR, ctx.theme.buttonHover, 3.f);
						{
							const NkColor c = mh ? ctx.theme.text : ctx.theme.textDisabled;
							const float32 s2 = bh2 - 12.f;
							if (rmode == 2) {
								dl.AddRect({maxR.x + 5.f, maxR.y + 7.f, s2, s2}, c, 1.4f);
								dl.AddRect({maxR.x + 8.f, maxR.y + 4.f, s2, s2}, c, 1.4f);
							} else {
								const NkRect w = {maxR.x + 5.f, maxR.y + 5.f, bh2 - 10.f, bh2 - 10.f};
								dl.AddRect(w, c, 1.4f);
								dl.AddRectFilled({w.x, w.y, w.w, 2.5f}, c);
							}
							if (mh && ctx.input.mouseClicked[0] && ctx.popupDepth == 0)
								mShell->ToggleMaximizePanel("TERMINAL");
						}
						// Replier / restaurer (chevron bas quand etendu, haut quand replie).
						const NkRect colR = {maxR.x - bh2 - 4.f, bar.y + 2.f, bh2, bh2};
						const bool ch2 = inR(colR);
						if (ch2)
							dl.AddRectFilled(colR, ctx.theme.buttonHover, 3.f);
						{
							const NkColor c = ch2 ? ctx.theme.text : ctx.theme.textDisabled;
							const float32 cx = colR.x + bh2 * 0.5f, cy = colR.y + bh2 * 0.5f;
							if (rmode == 1) {
								dl.AddLine({cx - 4.f, cy + 2.f}, {cx, cy - 2.f}, c, 1.6f);
								dl.AddLine({cx, cy - 2.f}, {cx + 4.f, cy + 2.f}, c, 1.6f);
							} else {
								dl.AddLine({cx - 4.f, cy - 2.f}, {cx, cy + 2.f}, c, 1.6f);
								dl.AddLine({cx, cy + 2.f}, {cx + 4.f, cy - 2.f}, c, 1.6f);
							}
							if (ch2 && ctx.input.mouseClicked[0] && ctx.popupDepth == 0)
								mShell->ToggleCollapsePanel("TERMINAL");
						}
					}
					const float32 savedW = ctx.layout.region.w;
					const float32 comboH = h - ctx.S(9.f); // combo un peu plus court que la barre (centre)
					ctx.layout.cursor = {addR.x - cw - 1.f, bar.y + (h - comboH) * 0.5f};
					ctx.layout.lineStartX = ctx.layout.cursor.x;
					ctx.layout.curLineH = 0.f;
					ctx.layout.region.w = (ctx.layout.cursor.x - ctx.layout.region.x) + cw;
					EnsureBaseShells();
					if (mNewShell < 0 || mNewShell >= static_cast<int32>(mShells.Size()))
						mNewShell = 0;
					ctx.PushId("shellhdr");
					if (BeginCombo(ctx, "", mShells[mNewShell].label.CStr(), static_cast<int32>(mShells.Size()),
								   comboH)) {
						DetectWslDistros(); // ajoute les distros WSL2 reelles (une seule fois)
						for (int32 i = 0; i < static_cast<int32>(mShells.Size()); ++i)
							if (Selectable(ctx, mShells[i].label.CStr(), i == ctx.comboNav) ||
								(i == ctx.comboNav && ctx.comboEnter)) {
								mNewShell = i;
								AddTerm(i); // choisir un shell CREE le terminal (facon VSCode)
								ctx.ClosePopup();
							}
						EndCombo(ctx);
					}
					ctx.PopId();
					ctx.layout.region.w = savedW;
				}

			private:
				struct Term {
						NkString cwd;		  // dossier de démarrage (vide = racine du workspace)
						NkString cmdOverride; // commande à exécuter à la place du shell (agent CLI)
						NkPty pty;			  // shell interactif (ConPTY)
						NkTerm screen;		  // emulateur VT (grille de cellules)
						int32 shell = SH_PWSH;
						NkString distro;			   // distro WSL ciblee (si shell == SH_WSL)
						NkString label = "powershell"; // libelle affiche (onglet/liste)
						bool alive = false;
						bool started = false; // pty deja lance ?
						bool touched = false; // l utilisateur y a TAPE (ne pas recycler au changement de workspace)
						float32 scrollX = 0.f, scrollY = 0.f;
						bool follow = true; // colle au bas (desactive au scroll manuel)
						// Selection en cellules : ancre (A) + curseur (B), en (ligne ABSOLUE, colonne).
						int32 sAL = 0, sAC = 0, sBL = 0, sBC = 0;
						bool dragging = false;

						bool HasSel() const {
							return sAL != sBL || sAC != sBC;
						}
				};

				// ── Lance le shell interactif (ConPTY) pour ce terminal, une seule fois. ──
				void StartTerm(Term &t) {
					if (t.started)
						return;
					t.started = true;
					GlobalLogBuffer().Push(NkString("[term] demarre dans: ") + ((mState && mState->HasWorkspace())
																					? mState->root.ToString()
																					: NkString("(cwd exe)")));
					t.pty.Start(!t.cmdOverride.Empty() ? t.cmdOverride : PtyCommand(t.shell, t.distro), t.screen.Cols(),
								t.screen.Rows(),
								!t.cwd.Empty() ? t.cwd // « Ouvrir dans le terminal » : dossier demandé
											   : ((mState && mState->HasWorkspace() && !mState->root.ToString().Empty())
													  ? mState->root.ToString()
													  : NkString()));
				}

				// Programme reel a lancer pour chaque type de shell.
				// Invite COLOREE injectee (chemin bleu + « > » vert) : PowerShell/cmd n'emettent
				// pas de couleurs par defaut (contrairement a bash Ubuntu) -> on definit une
				// invite ANSI a leur lancement pour un rendu colore homogene.
				static NkString PwshColored(const NkString &exe) {
					return exe +
						   " -NoLogo -NoExit -Command \"function prompt { $e=[char]27; \\\"$e[38;2;88;166;255m$($PWD.Path)$e[0m$e[38;2;120;200;120m> $e[0m\\\" }\"";
				}
				static NkString CmdColored(const char *pre) {
					const NkString p = "prompt $E[38;2;88;166;255m$P$E[0m$E[38;2;120;200;120m$G$E[0m$S";
					if (pre && pre[0])
						return NkString("cmd.exe /K \"") + p + " & " + pre + "\"";
					return NkString("cmd.exe /K ") + p;
				}

				static NkString PtyCommand(int32 s, const NkString &distro) {
					switch (s) {
						case SH_PWSH:
						case SH_JENGA:
							return PwshColored("powershell.exe");
						case SH_WSL:
							return distro.Empty() ? NkString("wsl.exe") : (NkString("wsl.exe -d ") + distro);
						case SH_BASH:
							return NkString("bash.exe");
						case SH_DOCKER:
							return CmdColored("docker ps");
						default:
							return CmdColored("");
					}
				}

				// ── Grille du terminal : rend les cellules visibles + curseur + selection +
				//    scrollbars V/H avec fleches + auto-suivi du bas (vrai terminal). ──
				// Recalcule les correspondances de la recherche (Ctrl+F) sur TOUT le terminal.
				void RecomputeFind(Term &t) {
					mFindHits.Clear();
					mFindCur = 0;
					if (!mFindBuf[0])
						return;
					char q[128];
					int32 ql = 0;
					for (const char *pp = mFindBuf; *pp && ql < 127; ++pp)
						q[ql++] = (!mFindCase && *pp >= 'A' && *pp <= 'Z') ? (char)(*pp + 32) : *pp;
					const int32 total = (int32)t.screen.TotalLines();
					for (int32 i = 0; i < total; ++i) {
						const NkTerm::Line &ln = t.screen.LineAt((usize)i);
						const int32 sl = (int32)ln.Size();
						for (int32 c = 0; c + ql <= sl; ++c) {
							bool mm = true;
							for (int32 k = 0; k < ql && mm; ++k) {
								const uint32 cp = ln[c + k].cp;
								char ch = (cp < 128) ? (char)cp : '?';
								if (!mFindCase && ch >= 'A' && ch <= 'Z')
									ch = (char)(ch + 32);
								if (ch != q[k])
									mm = false;
							}
							if (mm) {
								FindHit fh;
								fh.line = i;
								fh.col = c;
								fh.len = ql;
								mFindHits.PushBack(fh);
								c += ql - 1;
							}
						}
						if (mFindHits.Size() > 5000)
							break;
					}
				}

				// Barre de recherche DANS le terminal (Ctrl+F) : champ + compteur + prec/suiv/fermer.
				void DrawTermFind(NkGuiContext &ctx, Term &t, const NkRect &out) {
					auto &dl = ctx.DL();
					const NkGuiFont *font = ctx.font;
					const float32 S = ctx.S(1.f);
					const int32 total = (int32)t.screen.TotalLines();
					if (!StrEq(mFindBuf, mFindLast) || total != mFindTotalSeen) {
						RecomputeFind(t);
						int32 k = 0;
						for (const char *pp = mFindBuf; *pp && k < 128; ++pp)
							mFindLast[k++] = *pp;
						mFindLast[k] = 0;
						mFindTotalSeen = total;
					}
					const float32 bw = 340.f * S, bh = 28.f * S;
					const NkRect bar = {out.x + out.w - bw - 20.f * S, out.y + 6.f * S, bw, bh};
					dl.AddRectFilled(bar, NkColor{30, 33, 40, 255}, 6.f * S);
					dl.AddRect(bar, NkColor{70, 78, 90, 255}, 1.f);
					const NkRect fr = {bar.x + 8.f * S, bar.y + 4.f * S, bw - 164.f * S, bh - 8.f * S};
					editorkit::NkOverlayTextField(ctx, dl, font, fr, mFindBuf, (int32)sizeof(mFindBuf), true);
					const NkVec2 m = ctx.input.mousePos;
					const float32 lineH = font ? font->LineHeight() : 16.f;
					auto goHit = [&](int32 d) {
						if (mFindHits.Empty())
							return;
						mFindCur = (mFindCur + d + (int32)mFindHits.Size()) % (int32)mFindHits.Size();
						t.follow = false;
						const float32 sy = (float32)mFindHits[mFindCur].line * lineH - out.h * 0.4f;
						t.scrollY = sy < 0.f ? 0.f : sy;
					};
					if (font && font->Valid()) {
						const NkString cnt = mFindHits.Empty() ? NkString(mFindBuf[0] ? "0" : "")
						                                       : NkPrintf("%d/%d", mFindCur + 1, (int32)mFindHits.Size());
						dl.AddText(font->Face(), font->TexId(),
						           {fr.x + fr.w + 6.f * S, bar.y + (bh - lineH) * 0.5f + font->Ascent()}, cnt.CStr(),
						           ctx.theme.textDisabled);
					}
					auto iconBtn = [&](float32 bx, int32 kind) -> bool {
						const NkRect r = {bx, bar.y + 4.f * S, 22.f * S, bh - 8.f * S};
						const bool hh = m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h;
						if (hh)
							dl.AddRectFilled(r, ctx.theme.buttonHover, 3.f);
						const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f, a = 4.f * S;
						const NkColor c = hh ? ctx.theme.text : ctx.theme.textDisabled;
						if (kind == 0)
							dl.AddTriangleFilled({cx, cy - a}, {cx - a, cy + a}, {cx + a, cy + a}, c);
						else if (kind == 1)
							dl.AddTriangleFilled({cx - a, cy - a}, {cx + a, cy - a}, {cx, cy + a}, c);
						else {
							dl.AddLine({cx - a, cy - a}, {cx + a, cy + a}, c, 1.5f);
							dl.AddLine({cx - a, cy + a}, {cx + a, cy - a}, c, 1.5f);
						}
						return hh && ctx.input.mouseClicked[0] && ctx.popupDepth == 0;
					};
					const float32 b0 = bar.x + bw - 74.f * S;
					// Bouton « Aa » : respecter la casse (recherche exacte).
					{
						const NkRect ar = {b0 - 26.f * S, bar.y + 4.f * S, 22.f * S, bh - 8.f * S};
						const bool ah = m.x >= ar.x && m.x < ar.x + ar.w && m.y >= ar.y && m.y < ar.y + ar.h;
						dl.AddRectFilled(ar, mFindCase ? NkColor{15, 115, 213, 255}
						                               : (ah ? ctx.theme.buttonHover : NkColor{0, 0, 0, 0}), 3.f);
						if (font && font->Valid())
							dl.AddText(font->Face(), font->TexId(),
							           {ar.x + (ar.w - font->MeasureWidth("Aa")) * 0.5f, bar.y + (bh - lineH) * 0.5f + font->Ascent()},
							           "Aa", mFindCase ? NkColor{255, 255, 255, 255} : ctx.theme.textDisabled);
						if (ah && ctx.input.mouseClicked[0] && ctx.popupDepth == 0) {
							mFindCase = !mFindCase;
							RecomputeFind(t); // recalcul immediat avec/sans casse
						}
					}
					if (iconBtn(b0, 0))
						goHit(-1);
					if (iconBtn(b0 + 24.f * S, 1))
						goHit(1);
					if (iconBtn(b0 + 48.f * S, 2))
						mFindOpen = false;
					if (ctx.input.KeyPressed(nkgui::NkGuiKey::Enter))
						goHit(ctx.input.shiftDown ? -1 : 1);
					if (ctx.input.KeyPressed(nkgui::NkGuiKey::Escape))
						mFindOpen = false;
				}

				void DrawGrid(NkGuiContext &ctx, Term &t, const NkRect &out, float32 lineH, float32 pad) {
					auto &dl = ctx.DL();
					const bool sbLight =
						((int32)ctx.theme.bgPrimary.r + ctx.theme.bgPrimary.g + ctx.theme.bgPrimary.b) > 384;
					const NkColor kTrk = sbLight ? NkColor{0, 0, 0, 20} : NkColor{255, 255, 255, 16};
					const NkColor kThb = sbLight ? NkColor{168, 176, 185, 255} : NkColor{80, 88, 98, 255};
					const NkColor kThbH = sbLight ? NkColor{130, 138, 148, 255} : NkColor{120, 130, 142, 255};
					const float32 sbW = 14.f;
					const NkFont *face = (ctx.font && ctx.font->Valid()) ? ctx.font->Face() : nullptr;
					const float32 cellW = face ? face->CalcTextSizeX("M") : 8.f;
					const float32 cw = cellW > 1.f ? cellW : 8.f;
					const float32 viewW = out.w - sbW - pad * 2.f;
					const float32 viewH = out.h - sbW;
					const float32 left = out.x + pad;
					const NkVec2 m = ctx.input.mousePos;
					auto in = [&](const NkRect &r) {
						return m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h;
					};

					// Recale la taille de la grille (+ le pty) sur la zone visible.
					int16 cols = static_cast<int16>(viewW / cw);
					if (cols < 1)
						cols = 1;
					if (cols > 500)
						cols = 500;
					int16 rows = static_cast<int16>(viewH / lineH);
					if (rows < 1)
						rows = 1;
					if (rows > 300)
						rows = 300;
					if (t.started && (cols != t.screen.Cols() || rows != t.screen.Rows())) {
						t.screen.Resize(cols, rows);
						t.pty.Resize(cols, rows);
					}

					const float32 topPad = lineH;
					const int32 total = static_cast<int32>(t.screen.TotalLines());
					const float32 contentH = total * lineH + topPad;
					// « Coller au bas » = afficher l'ECRAN (les rows dernieres lignes) epingle.
					// On ne defile QUE dans le scrollback : borne basse = followY. Pas de marge
					// basse over-scrollable -> evite le va-et-vient (clignotement) au scroll bas.
					float32 followY = static_cast<float32>(total - t.screen.Rows()) * lineH;
					if (followY < 0.f)
						followY = 0.f;
					const float32 maxSY = followY; // on ne descend pas en dessous de l'ecran
					const float32 maxSX = 0.f;	   // contenu cale sur cols -> pas de defilement H

					if (in(out)) {
						if (ctx.input.wheel != 0.f) {
							t.scrollY -= ctx.input.wheel * lineH * 3.f;
							ctx.input.wheel = 0.f;
							t.follow = false;
						}
					}
					if (t.follow)
						t.scrollY = followY;
					if (t.scrollY < 0.f)
						t.scrollY = 0.f;
					if (t.scrollY > maxSY)
						t.scrollY = maxSY;
					t.scrollX = 0.f;

					// ── Selection souris (cellules) ──
					const NkRect selArea = {out.x, out.y, out.w - sbW, viewH};
					auto rowAtY = [&](float32 y) -> int32 {
						int32 L = static_cast<int32>((y - out.y - topPad + t.scrollY) / lineH);
						if (L < 0)
							L = 0;
						if (L >= total)
							L = total - 1;
						return L;
					};
					auto colAtX = [&](float32 x) -> int32 {
						int32 c = static_cast<int32>((x - left) / cw + 0.5f);
						if (c < 0)
							c = 0;
						return c;
					};
					if (ctx.input.mouseClicked[0] && in(selArea) && ctx.popupDepth == 0 && !mMenu.open) {
						const int32 L = rowAtY(m.y);
						t.sAL = t.sBL = L;
						t.sAC = t.sBC = colAtX(m.x);
						t.dragging = true;
					}
					if (t.dragging && ctx.input.mouseDown[0]) {
						t.sBL = rowAtY(m.y);
						t.sBC = colAtX(m.x);
					}
					if (!ctx.input.mouseDown[0])
						t.dragging = false;
					// Selection normalisee (aL,aC) <= (bL,bC).
					int32 nAL = t.sAL, nAC = t.sAC, nBL = t.sBL, nBC = t.sBC;
					if (nAL > nBL || (nAL == nBL && nAC > nBC)) {
						int32 tl = nAL, tc = nAC;
						nAL = nBL;
						nAC = nBC;
						nBL = tl;
						nBC = tc;
					}
					// Ctrl+C : copie si selection, sinon laisse RouteKeyboard envoyer SIGINT.
					if (ctx.input.wantCopy && t.HasSel())
						CopySelection(ctx, t);

					// ── Rendu des cellules ──
					const NkRect txtClip = {out.x, out.y, out.w - sbW, viewH};
					dl.PushClipRect(txtClip, true);
					int32 first = static_cast<int32>((t.scrollY - topPad) / lineH);
					if (first < 0)
						first = 0;
					const int32 last = first + static_cast<int32>(viewH / lineH) + 2;
					const float32 asc = ctx.font ? ctx.font->Ascent() : 12.f;
					for (int32 i = first; i <= last && i < total; ++i) {
						if (i < 0)
							continue;
						const float32 ytop = out.y + topPad + i * lineH - t.scrollY;
						const NkTerm::Line &ln = t.screen.LineAt(static_cast<usize>(i));
						// Surlignage de selection (en colonnes de cellules).
						if (t.HasSel() && i >= nAL && i <= nBL) {
							const int32 c0 = (i == nAL) ? nAC : 0;
							const int32 c1 = (i == nBL) ? nBC : cols;
							if (c1 > c0)
								dl.AddRectFilled({left + c0 * cw, ytop, (c1 - c0) * cw, lineH},
												 NkColor{31, 111, 235, 90});
						}
						// Surlignage des correspondances de recherche (Ctrl+F) sur cette ligne.
						if (mFindOpen && !mFindHits.Empty()) {
							for (usize hI = 0; hI < mFindHits.Size(); ++hI) {
								const FindHit &fh = mFindHits[hI];
								if (fh.line != i)
									continue;
								const bool cur = ((int32)hI == mFindCur);
								dl.AddRectFilled({left + fh.col * cw, ytop, fh.len * cw, lineH},
												 cur ? NkColor{240, 190, 40, 175} : NkColor{240, 190, 40, 80});
							}
						}
						const int32 ncell = static_cast<int32>(ln.Size());
						for (int32 c = 0; c < ncell; ++c) {
							const NkTermCell &cell = ln[c];
							const float32 x = left + c * cw;
							if (x >= out.x + out.w - sbW)
								break;
							if (cell.bg.a != 0)
								dl.AddRectFilled({x, ytop, cw + 0.5f, lineH}, cell.bg);
							if (cell.cp != 0x20 && cell.cp != 0 && face) {
								char u8[5];
								const int32 n = NkEncodeU8(cell.cp, u8);
								// Cellule NON colorée (fg par défaut #CCCCCC) -> couleur de texte du
								// THÈME (présente + lisible en clair comme en sombre). Les cellules
								// colorées par ANSI gardent leur couleur.
								NkColor fg = cell.fg;
								if (fg.r == 204 && fg.g == 204 && fg.b == 204 && fg.a == 255)
									fg = ctx.theme.text;
								NkDrawTextU(ctx, x, ytop + asc, ytop, lineH, u8, u8 + n, fg);
							}
						}
					}
					// Curseur (bloc) si focus.
					if (mFocused && t.screen.CursorVisible()) {
						const int32 cl = static_cast<int32>(t.screen.CursorLine());
						const int32 cc = t.screen.CursorCol();
						const float32 cx = left + cc * cw;
						const float32 cy = out.y + topPad + cl * lineH - t.scrollY;
						dl.AddRectFilled({cx, cy, cw, lineH}, ctx.theme.text);
					}
					dl.PopClipRect();

					// ── Scrollbars V + H avec fleches ──
					auto arrow = [&](const NkRect &r, int32 dir) -> bool {
						const bool h = in(r);
						if (h)
							dl.AddRectFilled(r, ctx.theme.button);
						const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f, a = 3.2f;
						const NkColor c = h ? kThbH : kThb;
						if (dir == 0)
							dl.AddTriangleFilled({cx, cy - a}, {cx - a, cy + a}, {cx + a, cy + a}, c);
						else if (dir == 1)
							dl.AddTriangleFilled({cx - a, cy - a}, {cx + a, cy - a}, {cx, cy + a}, c);
						else if (dir == 2)
							dl.AddTriangleFilled({cx - a, cy}, {cx + a, cy - a}, {cx + a, cy + a}, c);
						else
							dl.AddTriangleFilled({cx - a, cy - a}, {cx + a, cy}, {cx - a, cy + a}, c);
						return h && ctx.input.mouseDown[0];
					};
					const NkRect vT = {out.x + out.w - sbW, out.y, sbW, viewH};
					const NkRect hT = {out.x, out.y + viewH, out.w - sbW, sbW};
					dl.AddRectFilled(vT, kTrk);
					dl.AddRectFilled(hT, kTrk);
					dl.AddRectFilled({vT.x, hT.y, sbW, sbW}, kTrk);
					{
						const NkRect up = {vT.x, vT.y, sbW, sbW}, dn = {vT.x, vT.y + viewH - sbW, sbW, sbW};
						const NkRect iv = {vT.x, vT.y + sbW, sbW, viewH - 2.f * sbW};
						if (arrow(up, 0)) {
							t.scrollY -= lineH * 0.8f;
							t.follow = false;
						}
						if (arrow(dn, 1))
							t.scrollY += lineH * 0.8f;
						if (maxSY > 0.f && iv.h > 8.f) {
							float32 th = iv.h * (viewH / contentH);
							if (th < 24.f)
								th = 24.f;
							if (th > iv.h)
								th = iv.h;
							const float32 ty = iv.y + (t.scrollY / maxSY) * (iv.h - th);
							if (ctx.input.mouseClicked[0] && in(iv))
								ctx.activeId = ctx.GetId("##tvbar");
							const bool actv = (ctx.activeId == ctx.GetId("##tvbar"));
							if (actv && ctx.input.mouseDown[0]) {
								const float32 u = (m.y - iv.y - th * 0.5f) / (iv.h - th);
								t.scrollY = (u < 0 ? 0 : u > 1 ? 1 : u) * maxSY;
								t.follow = false;
							}
							dl.AddRectFilled({iv.x + 3.f, ty, sbW - 6.f, th}, (actv || in(iv)) ? kThbH : kThb, 3.f);
						}
					}
					{
						const NkRect lf = {hT.x, hT.y, sbW, sbW}, rt = {hT.x + hT.w - sbW, hT.y, sbW, sbW};
						const NkRect ih = {hT.x + sbW, hT.y, hT.w - 2.f * sbW, sbW};
						arrow(lf, 2);
						arrow(rt, 3);
						dl.AddRectFilled({ih.x + 3.f, hT.y + 3.f, ih.w - 6.f, sbW - 6.f}, kThb,
										 3.f); // H inactif (contenu cale)
					}
					if (t.scrollY < 0.f)
						t.scrollY = 0.f;
					if (t.scrollY > maxSY)
						t.scrollY = maxSY;
					if (t.scrollY >= followY - 1.f)
						t.follow = true; // revenu au bas -> re-suit le flux
				}

				// ── Clavier : route les frappes vers l'entree du pty (UTF-8 + sequences VT). ──
				void RouteKeyboard(NkGuiContext &ctx, Term &t) {
					NkVector<char> seq;
					auto put = [&](const char *s) {
						for (; *s; ++s)
							seq.PushBack(*s);
					};
					// Caracteres tapes (hors touches d'edition + hors Ctrl-C/A/V/X geres en flags).
					for (int32 i = 0; i < ctx.input.charCount; ++i) {
						const uint32 cp = ctx.input.chars[i];
						if (cp == 9) {
							seq.PushBack('\t');
							continue;
						}
						if (cp == 10 || cp == 13 || cp == 8 || cp == 127)
							continue; // touches dediees
						if (cp < 32) {
							if (cp == 3 || cp == 1 || cp == 22 || cp == 24)
								continue;
							seq.PushBack(static_cast<char>(cp));
							continue;
						} // Ctrl+lettre
						char u8[5];
						const int32 n = NkEncodeU8(cp, u8);
						for (int32 k = 0; k < n; ++k)
							seq.PushBack(u8[k]);
					}
					// Touches d'edition -> sequences.
					auto K = [&](NkGuiKey k) { return ctx.input.KeyPressedRepeat(k); };
					if (K(NkGuiKey::Enter))
						put("\r");
					if (K(NkGuiKey::Backspace))
						put("\x7f");
					if (K(NkGuiKey::Delete))
						put("\x1b[3~");
					if (K(NkGuiKey::Up))
						put("\x1b[A");
					if (K(NkGuiKey::Down))
						put("\x1b[B");
					if (K(NkGuiKey::Right))
						put("\x1b[C");
					if (K(NkGuiKey::Left))
						put("\x1b[D");
					if (K(NkGuiKey::Home))
						put("\x1b[H");
					if (K(NkGuiKey::End))
						put("\x1b[F");
					if (ctx.input.KeyPressed(NkGuiKey::Escape))
						put("\x1b");
					// Raccourcis : coller / copier (->SIGINT si pas de selection) / tout selectionner.
					if (ctx.input.wantPaste)
						PasteClipboard(ctx, t);
					if (ctx.input.wantSelectAll)
						SelectAll(t);
					if (ctx.input.wantCopy && !t.HasSel())
						put("\x03"); // Ctrl+C = interruption
					if (seq.Size() > 0) {
						t.scrollY = 1.0e9f;
						t.follow = true;
						t.pty.Write(seq.Data(), seq.Size());
						t.touched = true;
					}
				}

				// Texte de la selection (cellules -> UTF-8), espaces de fin retires par ligne.
				void CopySelection(NkGuiContext &ctx, Term &t) {
					int32 aL = t.sAL, aC = t.sAC, bL = t.sBL, bC = t.sBC;
					if (aL > bL || (aL == bL && aC > bC)) {
						int32 tl = aL, tc = aC;
						aL = bL;
						aC = bC;
						bL = tl;
						bC = tc;
					}
					const int32 total = static_cast<int32>(t.screen.TotalLines());
					NkVector<char> buf;
					for (int32 L = aL; L <= bL && L < total; ++L) {
						if (L < 0)
							continue;
						const NkTerm::Line &ln = t.screen.LineAt(static_cast<usize>(L));
						const int32 ncell = static_cast<int32>(ln.Size());
						const int32 c0 = (L == aL) ? aC : 0;
						int32 c1 = (L == bL) ? bC : ncell;
						if (c1 > ncell)
							c1 = ncell;
						int32 end = c1;
						while (end > c0 && (ln[end - 1].cp == 0x20 || ln[end - 1].cp == 0))
							--end; // trim fin
						for (int32 c = (c0 < 0 ? 0 : c0); c < end; ++c) {
							char u8[5];
							const int32 n = NkEncodeU8(ln[c].cp ? ln[c].cp : 0x20, u8);
							for (int32 k = 0; k < n; ++k)
								buf.PushBack(u8[k]);
						}
						if (L < bL)
							buf.PushBack('\n');
					}
					buf.PushBack('\0');
					if (buf.Size() > 1)
						ctx.SetClipboard(buf.Data());
				}

				void PasteClipboard(NkGuiContext &ctx, Term &t) {
					const NkString clip = ctx.GetClipboard();
					if (!clip.Empty())
						t.pty.Write(clip.CStr(), clip.Size());
					t.touched = true;
				}

				void SelectAll(Term &t) {
					t.sAL = 0;
					t.sAC = 0;
					t.sBL = static_cast<int32>(t.screen.TotalLines()) - 1;
					t.sBC = t.screen.Cols();
				}

				// Construit la liste de base (toujours dispo, sans cout) : PowerShell, cmd,
				// jenga, bash. Les distros WSL sont ajoutees a la demande (DetectWslDistros).
				void EnsureBaseShells() {
					if (mShellsBuilt)
						return;
					mShellsBuilt = true;
					mShells.PushBack(ShellDef{SH_PWSH, "Windows PowerShell", "", ""});
					// PowerShell 7 (pwsh.exe) si installe (a cote de Windows PowerShell).
					const char *pf = env::GetEnvVar("ProgramFiles");
					if (pf) {
						const NkString pwsh = NkString(pf) + "\\PowerShell\\7\\pwsh.exe";
						if (NkFile::Exists(NkPath(pwsh)))
							mShells.PushBack(ShellDef{SH_PWSH, "PowerShell 7", "", PwshColored(NkString("\"") + pwsh + "\"")});
					}
					mShells.PushBack(ShellDef{SH_CMD, "cmd", "", ""});
					// Git Bash si installe (sinon bash.exe generique du PATH).
					bool gitBash = false;
					if (pf) {
						const NkString gb = NkString(pf) + "\\Git\\bin\\bash.exe";
						if (NkFile::Exists(NkPath(gb))) {
							mShells.PushBack(ShellDef{SH_BASH, "Git Bash", "", NkString("\"") + gb + "\" -i -l"});
							gitBash = true;
						}
					}
					if (!gitBash)
						mShells.PushBack(ShellDef{SH_BASH, "bash", "", ""});
					// Docker Desktop : entree "Docker" seulement si docker.exe est present.
					if (pf) {
						const NkString dk = NkString(pf) + "\Docker\Docker\resources\bin\docker.exe";
						if (NkFile::Exists(NkPath(dk)))
							mShells.PushBack(ShellDef{SH_DOCKER, "Docker", "", CmdColored("docker ps")});
					}
				}

				// Detecte les distributions WSL2 INSTALLEES (`wsl --list --quiet`) et ajoute
				// une entree par distro. WSL_UTF8=1 force une sortie UTF-8 (sinon UTF-16LE) ;
				// on filtre quand meme les octets 0x00 / BOM par robustesse. Appel UNE fois,
				// a la 1re ouverture du combo (evite de geler le demarrage).
				void DetectWslDistros() {
					if (mWslDetected)
						return;
					mWslDetected = true;
#if defined(_WIN32)
					FILE *pipe = _popen("set \"WSL_UTF8=1\" && wsl --list --quiet 2>nul", "r");
					if (!pipe)
						return;
					char buf[256];
					usize j = 0;
					int ch;
					int32 found = 0;
					auto flush = [&]() {
						while (j > 0 && (buf[j - 1] == ' ' || buf[j - 1] == '\t'))
							--j; // trim fin
						buf[j] = '\0';
						if (j > 0) {
							mShells.PushBack(ShellDef{SH_WSL, NkString("WSL: ") + buf, NkString(buf)});
							++found;
						}
						j = 0;
					};
					// fgetc sur PIPE process : conservé (cf. wrapper désigné NkProcess.h).
					while ((ch = std::fgetc(pipe)) != EOF) {
						if (ch == 0x00 || ch == '\r' || ch == 0xFF || ch == 0xFE)
							continue; // nuls UTF-16 + BOM
						if (ch == '\n') {
							flush();
							continue;
						}
						if (j + 1 < sizeof(buf))
							buf[j++] = static_cast<char>(ch);
					}
					flush();
					_pclose(pipe);
					if (found == 0)
						mShells.PushBack(ShellDef{SH_WSL, "wsl", ""}); // repli : wsl generique
#else
					mShells.PushBack(ShellDef{SH_WSL, "wsl", ""});
#endif
				}

				int32 FirstAlive() const {
					for (int32 i = 0; i < 8; ++i)
						if (mTerm[i].alive)
							return i;
					return 0;
				}

				int32 AliveCount() const {
					int32 n = 0;
					for (int32 i = 0; i < 8; ++i)
						if (mTerm[i].alive)
							++n;
					return n;
				}

				// `idx` = index dans mShells (selecteur). Copie kind + distro + libelle.
				void AddTerm(int32 idx) {
					EnsureBaseShells();
					if (idx < 0 || idx >= static_cast<int32>(mShells.Size()))
						idx = 0;
					const ShellDef &sd = mShells[idx];
					for (int32 i = 0; i < 8; ++i)
						if (!mTerm[i].alive) {
							mTerm[i].pty.Stop(); // recycle un eventuel ancien pty du slot
							mTerm[i].screen.Clear();
							mTerm[i].alive = true;
							mTerm[i].started = false;
							mTerm[i].shell = sd.kind;
							mTerm[i].distro = sd.distro;
							mTerm[i].label = sd.label;
							mTerm[i].scrollY = 0.f;
							mTerm[i].follow = true;
							mTerm[i].cwd = NkString();
							mTerm[i].cmdOverride = sd.cmd; // pwsh 7 / Git Bash : commande explicite detectee
							mTerm[i].sAL = mTerm[i].sAC = mTerm[i].sBL = mTerm[i].sBC = 0;
							mTerm[i].dragging = false;
							mActive = i;
							GlobalLogBuffer().Push(NkString("[term] nouveau terminal: ") + sd.label);
							return;
						}
				}

				// ── Préférence GLOBALE « shell par défaut » (persistée %APPDATA%/NKCode/terminal.cfg,
				//    format `shell=N` + `distro=S`). L'HISTORIQUE des commandes n'est PAS géré ici :
				//    c'est celui du shell lui-même (PSReadLine, .bash_history…), déjà persistant. ──
				static NkString PrefPath() {
					const char *base = env::GetEnvVar("APPDATA"); // API maison (NkEnv.h)
					if (!base)
						base = env::GetEnvVar("HOME");
					if (!base)
						return NkString();
					NkString dir = NkString(base) + "/NKCode";
					NkDirectory::Create(dir.CStr());
					return dir + "/terminal.cfg";
				}

				void EnsurePrefs() {
					if (mPrefLoaded)
						return;
					mPrefLoaded = true;
					const NkString p = PrefPath();
					if (p.Empty() || !NkFile::Exists(NkPath(p)))
						return;
					const NkString txt = NkFile::ReadAllText(NkPath(p));
					const char *s = txt.CStr();
					while (*s) {
						const char *e = s;
						while (*e && *e != '\n' && *e != '\r')
							++e;
						if (e - s > 6 && s[0] == 's' && s[1] == 'h' && s[2] == 'e' && s[3] == 'l' && s[4] == 'l' &&
							s[5] == '=')
							mDefShell = NkAtoi(s + 6);
						else if (e - s > 7 && s[0] == 'd' && s[1] == 'i' && s[2] == 's' && s[3] == 't' && s[4] == 'r' &&
								 s[5] == 'o' && s[6] == '=') {
							NkString d;
							for (const char *q = s + 7; q < e; ++q)
								d += *q;
							mDefDistro = d;
						}
						while (*e == '\n' || *e == '\r')
							++e;
						s = e;
					}
					if (mDefShell < 0 || mDefShell >= SH_COUNT)
						mDefShell = SH_PWSH;
				}

				void SavePrefs() {
					const NkString p = PrefPath();
					if (p.Empty())
						return;
					NkFile::WriteAllText(NkPath(p),
										 NkPrintf("shell=%d\ndistro=%s\n", mDefShell, mDefDistro.CStr())); // maison
				}

				// Crée un terminal d'un TYPE donné (sert au terminal par défaut) : libellé/distro
				// récupérés dans mShells ; le combo du « + » reflète ce choix.
				void AddTermKind(int32 kind, const NkString &distro) {
					EnsureBaseShells();
					if (kind == SH_WSL)
						DetectWslDistros(); // les entrées WSL n'existent qu'après détection
					int32 idx = -1;
					for (int32 i = 0; i < static_cast<int32>(mShells.Size()); ++i)
						if (mShells[i].kind == kind && StrEq(mShells[i].distro.CStr(), distro.CStr())) {
							idx = i;
							break;
						}
					if (idx < 0 && distro.Empty()) // distro non précisée : premier shell du même TYPE
						for (int32 i = 0; i < static_cast<int32>(mShells.Size()); ++i)
							if (mShells[i].kind == kind) {
								idx = i;
								break;
							}
					if (idx < 0) { // toujours rien (ex. WSL sans distro détectée) : crée l'entrée demandée
						mShells.PushBack(ShellDef{kind, kind == SH_WSL ? "wsl" : "shell", distro});
						idx = static_cast<int32>(mShells.Size()) - 1;
					}
					mNewShell = idx;
					AddTerm(idx);
				}

				void CloseTerm(int32 i) {
					if (AliveCount() <= 1)
						return;
					mTerm[i].pty.Stop();
					mTerm[i].alive = false;
					mTerm[i].started = false;
					if (mActive == i)
						mActive = FirstAlive();
				}

				static NkColor ShellColor(int32 s) {
					switch (s) {
						case SH_PWSH:
							return {31, 111, 235, 255};   // bleu PowerShell
						case SH_CMD:
							return {120, 130, 145, 255};  // gris-bleu cmd
						case SH_WSL:
							return {233, 84, 32, 255};    // orange WSL
						case SH_BASH:
							return {77, 160, 79, 255};    // vert bash
						case SH_JENGA:
							return {200, 150, 40, 255};   // ambre Jenga
						case SH_DOCKER:
							return {33, 150, 243, 255};  // bleu Docker
						default:
							return {150, 158, 168, 255};
					}
				}

				// Icone (texture) associee a un type de shell ; 0 si non chargee (-> pastille couleur).
				uint32 ShellIcon(int32 kind) const {
					const NkIcons *ic = mState ? mState->icons : nullptr;
					if (!ic)
						return 0;
					switch (kind) {
						case SH_PWSH:
							return ic->windowsLogo ? ic->windowsLogo : ic->kConsole;
						case SH_CMD:
							return ic->kConsole ? ic->kConsole : ic->windowsLogo;
						case SH_JENGA:
							return ic->kConsole ? ic->kConsole : ic->windowsLogo;
						case SH_BASH: // Git Bash / bash
						case SH_WSL:  // distro Linux
							return ic->linux ? ic->linux : ic->kConsole;
						case SH_DOCKER:
							return ic->docker ? ic->docker : ic->kConsole;
						default:
							return ic->kConsole;
					}
				}

				void DrawTermList(NkGuiContext &ctx, const NkRect &R) {
					if (R.w < 8.f)
						return;
					auto &dl = ctx.DL();
					dl.AddRectFilled(R, ctx.theme.header);
					dl.AddRectFilled({R.x, R.y, 1.f, R.h}, ctx.theme.button); // bord gauche
					const NkVec2 m = ctx.input.mousePos;
					auto inR = [&](const NkRect &r) {
						return m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h;
					};
					const NkIcons *ic = mState ? mState->icons : nullptr;
					const float32 h = ctx.ItemHeight() + ctx.S(4.f); // lignes plus aerees
					const float32 by = (h - (ctx.font ? ctx.font->LineHeight() : 14.f)) * 0.5f +
						   (ctx.font ? ctx.font->Ascent() : 11.f);
					float32 y = R.y + 6.f;
					int32 toClose = -1;
					for (int32 i = 0; i < 8; ++i) {
						if (!mTerm[i].alive)
							continue;
						const NkRect row = {R.x + 4.f, y, R.w - 8.f, h - 3.f};
						const bool active = (i == mActive), hov = inR(row);
						const NkColor sc = ShellColor(mTerm[i].shell);
						if (active) {
							dl.AddRectFilled(row, NkColor{sc.r, sc.g, sc.b, 38}, 5.f);
							dl.AddRect(row, NkColor{sc.r, sc.g, sc.b, 150}, 1.f);
							dl.AddRectFilled({row.x, row.y + 4.f, 3.f, row.h - 8.f}, sc, 2.f);
						} else if (hov)
							dl.AddRectFilled(row, ctx.theme.buttonHover, 5.f);
						// Icone du shell (tintee couleur shell) ; repli = pastille coloree.
						const uint32 ico = ShellIcon(mTerm[i].shell);
						const NkRect ir = {row.x + 10.f, y + (h - 14.f) * 0.5f - 1.f, 14.f, 14.f};
						if (ico)
							dl.AddImage(ico, ir, {0.f, 0.f}, {1.f, 1.f}, sc);
						else
							dl.AddRectFilled({ir.x + 2.f, ir.y + 2.f, 10.f, 10.f}, sc, 3.f);
						if (mRenaming == i) { // renommage inline (double-clic)
							const NkRect fr = {row.x + 28.f, y + 2.f, row.w - 54.f, h - 6.f};
							mRenameRect = fr;
							editorkit::NkOverlayTextField(ctx, dl, ctx.font, fr, mRenameBuf, (int32)sizeof(mRenameBuf),
														  true);
						} else if (ctx.font && ctx.font->Valid()) {
							dl.AddText(ctx.font->Face(), ctx.font->TexId(), {row.x + 32.f, y + by - 1.f},
								   mTerm[i].label.CStr(), active ? ctx.theme.text : ctx.theme.textDisabled);
							if (hov && ctx.input.mouseDoubleClicked[0] && ctx.popupDepth == 0) {
								if (mRenaming >= 0 && mRenaming != i && mRenameBuf[0])
									mTerm[mRenaming].label = NkString(mRenameBuf);
								mRenaming = i;
								mRenameArmed = false;
								int32 k = 0;
								for (const char *c = mTerm[i].label.CStr(); *c && k < 127; ++c)
									mRenameBuf[k++] = *c;
								mRenameBuf[k] = 0;
							}
						}
						// Corbeille (fermer) au survol / actif, si plus d'un terminal.
						bool closeClicked = false;
						if ((hov || active) && AliveCount() > 1) {
							const NkRect cl = {row.x + row.w - 22.f, y + (h - 16.f) * 0.5f - 1.f, 16.f, 16.f};
							const bool ch = inR(cl);
							if (ch)
								dl.AddRectFilled(cl, ctx.theme.button, 3.f);
							const NkColor tc = ch ? NkColor{248, 81, 73, 255} : ctx.theme.textDisabled;
							if (ic && ic->corbeille)
								dl.AddImage(ic->corbeille, {cl.x + 2.f, cl.y + 2.f, 12.f, 12.f}, {0.f, 0.f}, {1.f, 1.f}, tc);
							else {
								const float32 cx = cl.x + 8.f, cy = cl.y + 8.f, a = 3.f;
								dl.AddLine({cx - a, cy - a}, {cx + a, cy + a}, tc, 1.4f);
								dl.AddLine({cx - a, cy + a}, {cx + a, cy - a}, tc, 1.4f);
							}
							if (ch && ctx.input.mouseClicked[0] && ctx.popupDepth == 0) {
								toClose = i;
								closeClicked = true;
							}
						}
						if (hov && !closeClicked && ctx.input.mouseClicked[0] && ctx.popupDepth == 0) {
							if (mRenaming >= 0 && mRenaming != i) { // clic autre ligne -> valide le renommage
								if (mRenameBuf[0])
									mTerm[mRenaming].label = NkString(mRenameBuf);
								mRenaming = -1;
							}
							mActive = i;
						}
						y += h;
					}
					if (toClose >= 0)
						CloseTerm(toClose);
					// Renommage : Entree valide, Echap annule.
					if (mRenaming >= 0) {
						if (mRenaming >= 8 || !mTerm[mRenaming].alive)
							mRenaming = -1;
						else if (ctx.input.KeyPressed(nkgui::NkGuiKey::Enter)) {
							if (mRenameBuf[0])
								mTerm[mRenaming].label = NkString(mRenameBuf);
							mRenaming = -1;
						} else if (ctx.input.KeyPressed(nkgui::NkGuiKey::Escape))
							mRenaming = -1;
						else if (mRenameArmed && ctx.input.mouseClicked[0] && !ctx.input.mouseDoubleClicked[0] &&
								 !NkGuiRectContains(mRenameRect, ctx.input.mousePos)) {
							if (mRenameBuf[0]) // clic HORS du champ -> valide
								mTerm[mRenaming].label = NkString(mRenameBuf);
							mRenaming = -1;
						}
						mRenameArmed = (mRenaming >= 0); // arme pour la frame suivante
					}
				}

				Term mTerm[8];
				int32 mActive = 0;
				int32 mRenaming = -1;	   // terminal en cours de renommage (double-clic) ; -1 = aucun
				char mRenameBuf[128] = {}; // saisie du nouveau nom
				NkRect mRenameRect{};	   // rect du champ (pour valider au clic HORS du champ)
				bool mRenameArmed = false; // vrai des la 2e frame (evite de valider sur le double-clic d'ouverture)
				// ── Recherche DANS le terminal (Ctrl+F) : surlignage + navigation ──
				bool mFindOpen = false;
				bool mFindCase = false; // « Aa » : respecter la casse (recherche exacte)
				char mFindBuf[128] = {};
				char mFindLast[130] = {(char)1, 0}; // != mFindBuf au depart -> 1er calcul
				int32 mFindTotalSeen = -1;
				int32 mFindCur = 0;
				struct FindHit {
						int32 line = 0, col = 0, len = 0;
				};
				NkVector<FindHit> mFindHits;
				int32 mNewShell = 0;	   // index dans mShells (0 = powershell)
				int32 mDefShell = SH_PWSH; // TYPE du terminal par defaut (preference persistee)
				NkString mDefDistro;	   // distro WSL du defaut (si SH_WSL)
				NkString mSpawnedRoot;	   // racine au moment du spawn (recycle si le workspace change)
				bool mPrefLoaded = false;
				NkVector<ShellDef> mShells; // selecteur de shells (base + distros WSL)
				bool mShellsBuilt = false;
				bool mWslDetected = false;
				bool mFocused = false; // le terminal capte-t-il le clavier ?
				NkCtxMenu mMenu;	   // menu contextuel (clic droit) Copier/Coller
				NkVector<char> mDrain; // tampon de drain pty (reutilise)
		};

	} // namespace nkcode
} // namespace nkentseu
