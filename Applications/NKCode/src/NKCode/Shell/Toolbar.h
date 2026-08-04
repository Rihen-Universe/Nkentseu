#pragma once
// =============================================================================
// Toolbar.h — Barre d'outils de la VUE PRINCIPALE (dessin custom NkUi + vraies
// icônes, fidèle à la maquette Banani) :
//   [Solution ▸] › [Projet] | [Plateforme][Config][Archi] | [Tests] |
//   Build Rebuild Nettoyer | ▶Exécuter Déboguer Tests(n) | 🔍 Recherche rapide
// 100 % piloté par la structure Jenga réelle (`jenga info`). Combos fonctionnels
// (dropdown différé). Les actions appellent DoBuildAction/DoClean/DoRun/DoTest.
// =============================================================================
#include "NKEditorKit/NkEditorKit.h"
#include "NKEditorKit/NkEditorScrollbar.h" // NkVScrollbar/NkHScrollbar (scrollbar general)
#include "NKEditorKit/NkEditorTextField.h" // NkOverlayTextField (champ recherche editable)
#include "NKEditorKit/NkEditorTooltip.h"   // NkTooltip : infobulle avec delai (valeurs tronquees + boutons)
#include "NKCode/Project/NkCodeState.h"
#include "NKCode/Shell/NkUi.h"
#include "NKCode/Shell/NkOpenWs.h" // NkOwIco
#include "NKCode/Shell/NkI18n.h"   // NkT
#include "NKContainers/String/NkFormat.h" // NkPrintf (formatage maison)

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu;
		using namespace nkentseu::editorkit;
		using namespace nkentseu::nkgui;

		// État local (un seul combo ouvert à la fois). `scroll` = défilement du dropdown
		// (listes longues : projets/tests/compilateurs) ; `scrollDrag` = glissement du pouce.
		struct NkTbState {
				int32 open = -1;
				NkRect openR{};
				bool justOpened = false;
				float32 scroll = 0.f;
				bool scrollDrag = false;
				float32 scrollDragOff = 0.f;
				// Filtre de la liste deroulante. Sur un workspace de 186 projets,
				// derouler jusqu'a celui qu'on cherche n'est pas praticable. Le champ est
				// ANCRE en haut, HORS de la zone defilante : il reste donc visible quelle
				// que soit la position du defilement.
				char filter[64] = {};
				bool filterFocus = false;
		};

		// Kind d'un projet (colonne Kind de `jenga info`) -> icone. Reutilise CELLES
		// de l'assistant de creation de projet : deja chargees, et deja associees a
		// ces memes notions (terminal, fenetre, archive, lien). 0 = pas d'icone pour
		// ce Kind -> l'appelant retombe sur le nom en toutes lettres, ce qui garde
		// lisible un Kind ajoute plus tard sans toucher a l'interface.
		inline uint32 NkKindTex(const NkIcons *ic, const char *kind) {
			if (!ic || !kind || !*kind)
				return 0;
			if (NkFindSub(kind, "Console"))
				return ic->kConsole;
			if (NkFindSub(kind, "Windowed"))
				return ic->kWindowed;
			if (NkFindSub(kind, "Static"))
				return ic->kStatic;
			if (NkFindSub(kind, "Shared"))
				return ic->kShared;
			if (NkFindSub(kind, "Test"))
				return ic->kTest;
			return 0;
		}

		inline NkTbState &NkTb() {
			static NkTbState s;
			return s;
		}

		inline void DrawCodeToolbar(NkEditorFrameContext &ec, NkCodeState *s) {
			if (!s)
				return;
			s->ScanWorkspaces();
			s->TickWatch(ec.dt);
			s->LoadProjects();
			s->PollProjects();
			s->PollFlags();
			s->PollMacros();
			s->PollDiagnostics();
			s->TickDiagnostics(ec.dt);
			s->TickLsp(ec.dt); // clangd (étape A : traces [lsp] dans OUTPUT)	   // flags + macros préproc +
							   // diagnostics LIVE (squiggles, débounce)
			s->ProcessCompletionRequest(); // complétion CONTEXTUELLE ('.', '->', '::') compile-first
			s->ProcessHover();			   // hover documentation (survol ~0,5 s -> carte signature + doc)
			s->PollCompletion();		   // récupère les membres renvoyés par le compilateur -> popup
			s->TickMru();				   // MRU des onglets (Ctrl+Tab) : enregistre l'onglet actif
			s->ProcessNavigation();
			s->PollNav();
			if (s->HasActive() && s->files[s->active].doc.renGo) { // F2 : rename (clangd si pret, sinon textuel)
				NkCodeDoc &rd = s->files[s->active].doc;
				rd.renGo = false;
				if (s->lspState == 1 && s->lsp.Ready() && rd.renLine >= 0) { // SEMANTIQUE (WorkspaceEdit)
					s->lsp.ReqRename(s->files[s->active].path.ToString(), rd.renLine, rd.renCol, NkString(rd.renBuf));
					rd.renLine = -1;
					s->status = NkString("Renommage (clangd)…");
				} else {
					s->wsRenameTo = NkString(rd.renBuf);
					s->wsRenamePending = true;
					s->StartWsFind(rd.renWord, true, true); // casse stricte + mot entier (textuel)
				}
			}
			s->PollWsFind(); // recherche workspace (Ctrl+Maj+F)
			s->ProcessWsOpen();
			s->ProcessNavPick();   // Ctrl+clic : include / go-to-def (thread) / choix liste (différé hors rendu)
			s->TickSession(ec.dt); // persistance session : onglets + contenu non sauvegardé (hot-exit / reprise crash)
			s->TickFileWatch(ec.dt); // détecte suppression / modification EXTERNE des fichiers ouverts
			s->TickBgCheck(ec.dt);	 // vérif fond des fichiers STRATÉGIQUES modifiés hors éditeur
									 // (git status -> -fsyntax-only) : icônes rouges/oranges + voyant CODE

			const NkUi u = NkUi::From(ec);
			const NkRect r = ec.Ui().layout.region; // bornes de la toolbar (46px)
			NkTbState &tb = NkTb();
			const NkIcons *ic = s->icons;
			auto TEX = [&](uint32 v) -> uint32 { return ic ? v : 0u; };

			u.Rect(r, NkCol::sidebar); // fond sidebar (maquette)
			u.Rect({r.x, r.y + r.h - 1.f, r.w, 1.f}, NkCol::border);

			if (!s->HasWorkspace()) {
				u.Text(r.x + u.s(14), r.y + (r.h - u.Lh()) * 0.5f, NkT("tb.noworkspace"), NkCol::mutedFg);
				return;
			}

			// Hauteur UNIQUE pour combos / champ recherche / boutons (= hauteur des boutons Build),
			// centrée verticalement dans la toolbar (dont la hauteur globale ne change pas). Plus de libellés
			// au-dessus.
			const float32 ctrlH = u.s(26);
			const float32 ctrlY = r.y + (r.h - ctrlH) * 0.5f;
			const float32 cyBtn = ctrlY + ctrlH * 0.5f;

			// ── Combo (sans libellé) : toggle l'ouverture ──
			auto combo = [&](float32 x, const char *label, const char *value, uint32 tex, const char *drawn,
							 bool accent, int32 id, float32 w) -> void {
				(void)label; // maquette simplifiée : plus de libellé au-dessus
				const NkRect box = {x, ctrlY, w, ctrlH};
				const bool open = (tb.open == id);
				u.Panel(box, accent ? NkCol::secondary : NkCol::muted,
						(open || accent) ? NkCol::primary : NkCol::border, NkR::sm * u.S);
				float32 tx = x + u.s(9);
				if (tex || drawn) {
					// Le logo JENGA est une icône COLORÉE : rendu tel quel (teinte blanche),
					// sinon la teinte grise du thème le décolorerait.
					const bool colored = (ic && tex && tex == ic->jenga);
					NkOwIco(u, tex, drawn, {tx, ctrlY + (ctrlH - u.s(12)) * 0.5f, u.s(12), u.s(12)},
							colored ? NkColor{255, 255, 255, 255} : (accent ? NkCol::primary : NkCol::mutedFg));
					tx += u.s(18);
				}
				u.TextEllipsis(tx, ctrlY + (ctrlH - u.Lh()) * 0.5f, (x + w) - tx - u.s(16), value,
							   accent ? NkCol::primary : NkCol::foreground);
				NkOwIco(u, 0u, "chevron-down", {x + w - u.s(15), ctrlY + (ctrlH - u.s(9)) * 0.5f, u.s(9), u.s(9)},
						NkCol::mutedFg);
				// Infobulle : valeur COMPLETE au survol (la valeur affichee est tronquee par
				// ellipse quand elle depasse la largeur du combo — noms de projets/tests longs).
				NkTooltip(ec.Ui(), u.Hit(box) && tb.open < 0, value);
				if (u.Hit(box) && u.click) {
					if (open)
						tb.open = -1;
					else {
						tb.open = id;
						tb.openR = box;
						tb.justOpened = true;
					}
				}
			};
			// ── Séparateur vertical ──
			auto vsep = [&](float32 x) { u.Rect({x, ctrlY, 1.f, ctrlH}, NkCol::border); };
			// ── Bouton d'action (icône + label + variante + badge) ──
			auto btn = [&](float32 x, const char *label, uint32 tex, const char *drawn, int32 variant,
						   const char *badge) -> float32 {
				const float32 tw = u.TextW(label);
				const float32 w = u.s(12) + u.s(6) + tw + u.s(20);
				const NkRect b = {x, cyBtn - u.s(13), w, u.s(26)};
				const bool hv = u.Hit(b);
				NkColor bg = NkCol::muted, brd = NkCol::border, fg = NkCol::foreground;
				if (variant == 1) {
					bg = NkCol::primary;
					brd = NkCol::primary;
					fg = NkCol::primaryFg;
				} else if (variant == 2) {
					bg = NkCol::success;
					brd = NkCol::success;
					fg = NkCol::primaryFg;
				}
				if (hv)
					bg = (variant == 0)
							 ? NkCol::hover
							 : NkColor{(uint8)(bg.r > 20 ? bg.r - 18 : bg.r), (uint8)(bg.g > 20 ? bg.g - 18 : bg.g),
									   (uint8)(bg.b > 20 ? bg.b - 18 : bg.b), 255};
				u.Panel(b, bg, brd, NkR::sm * u.S);
				NkOwIco(u, tex, drawn, {b.x + u.s(10), b.y + u.s(7), u.s(12), u.s(12)}, fg);
				u.Text(b.x + u.s(10) + u.s(12) + u.s(6), b.y + (b.h - u.Lh()) * 0.5f, label, fg);
				if (badge && badge[0]) { // pastille en haut à droite
					const float32 bw = u.TextW(badge) + u.s(6);
					const NkRect pr = {b.x + w - bw * 0.5f - u.s(2), b.y - u.s(4), bw, u.s(13)};
					u.dl->AddRectFilled(pr, NkCol::primary, pr.h * 0.5f);
					u.Text(pr.x + u.s(3), pr.y - u.s(1), badge, NkCol::primaryFg);
				}
				return w;
			};

			// ── Données réelles ──
			int32 nSys = 0;
			const NkCodeState::SysDef *sysd = NkCodeState::Systems(&nSys);
			if (s->sysIdx < 0 || s->sysIdx >= nSys)
				s->sysIdx = 0;
			const NkCodeState::SysDef &SY = sysd[s->sysIdx];
			if (s->archIdx < 0 || s->archIdx > SY.nArch)
				s->archIdx = 0;
			static const char *kCfg[] = {"Debug", "Release", "Toutes"};
			const int32 cfgI = (s->cfgIdx >= 0 && s->cfgIdx < 3) ? s->cfgIdx : 0;
			const char *wsPrev =
				(s->wsIdx >= 0 && s->wsIdx < (int32)s->wsNames.Size()) ? s->wsNames[s->wsIdx].CStr() : "(workspace)";
			const char *projPrev =
				s->projects.Empty() ? "…" : (s->AllProjects() ? NkT("tb.allprojects") : s->SelectedProject());
			const char *archPrev = (s->archIdx >= SY.nArch) ? NkT("tb.all") : SY.archs[s->archIdx];
			int32 nTestVis = 0;
			for (int32 i = 0; i < (int32)s->tests.Size(); ++i)
				if (s->TestVisible(i))
					++nTestVis;
			const bool hasTests = !s->tests.Empty();
			if (s->testIdx >= 0 && !s->TestVisible(s->testIdx))
				s->testIdx = -1;
			const char *testPrev =
				(s->testIdx >= 0 && s->TestVisible(s->testIdx)) ? s->tests[s->testIdx].CStr() : NkT("tb.alltests");

			// Compilateur : combo CONDITIONNEL — n'apparait QUE si la plateforme courante
			// a >=2 compilateurs detectes (sinon aucun choix a faire). Filtre reel `jenga info`.
			s->ValidateCompilerForPlatform();
			const NkVector<int32> comps = s->CompilersForCurrentPlatform();
			const bool hasCompiler = comps.Size() >= 2;
			const char *compPrev = s->compilerName.Empty() ? NkT("tb.compilerauto") : s->compilerName.CStr();

			// ── Largeurs des contrôles ──
			const float32 wSol = u.s(140), wProj = u.s(120), wPlat = u.s(104), wCfg = u.s(92), wArch = u.s(92),
						  wTest = u.s(140), wComp = u.s(120), wSearch = u.s(150);
			const float32 gap = u.s(6), sep = u.s(14);
			auto btnW = [&](const char *l) { return u.s(12) + u.s(6) + u.TextW(l) + u.s(20); };
			(void)vsep;

			// ── Modèle de SEGMENTS (ordre visuel) : permet un repli PROGRESSIF quand la
			// barre est trop étroite. Chaque segment porte sa largeur + une priorité (plus
			// haute = gardée plus longtemps ; 1000 = jamais repliée). Build/Run ne sont
			// jamais repliés. Le surplus va derrière un bouton « » » qui ouvre un menu.
			struct Seg {
					int32 kind;
					int32 id;
					float32 w;
					int32 prio;
					bool grp;
					const char *label;
					const char *value;
					uint32 tex;
					const char *drawn;
					bool accent;
					int32 variant;
					const char *badge;
			};

			NkVector<Seg> segs;
			auto C = [&](int32 id, float32 w, int32 prio, bool grp, const char *label, const char *value, uint32 tex,
						 const char *drawn, bool accent) {
				segs.PushBack({0, id, w, prio, grp, label, value, tex, drawn, accent, 0, nullptr});
			};
			auto Bt = [&](int32 id, int32 prio, bool grp, const char *label, uint32 tex, const char *drawn,
						  int32 variant, const char *badge) {
				segs.PushBack({1, id, btnW(label), prio, grp, label, nullptr, tex, drawn, false, variant, badge});
			};
			NkString testBadge; // NkPrintf maison (ex-std::snprintf)
			if (nTestVis > 0)
				testBadge = NkPrintf("%d", nTestVis);
			C(0, wSol, 90, false, NkT("tb.solution"), wsPrev, TEX(ic ? ic->jenga : 0), "git-branch", true);
			C(1, wProj, 40, false, NkT("tb.projet"), projPrev, TEX(ic ? ic->pkg : 0), "package", false);
			// Icône de la PLATEFORME cible : logo dédié (Android, Apple, Tux, Windows,
			// Web) selon le nom sélectionné ; générique (écran) sinon.
			uint32 platTex = ic ? ic->monitor : 0;
			if (ic && SY.name) {
				auto has = [&](const char *k) {
					auto low = [](char ch) { return (ch >= 'A' && ch <= 'Z') ? char(ch + 32) : ch; };
					for (const char *h = SY.name; *h; ++h) {
						const char *a = h;
						const char *b = k;
						while (*a && *b && low(*a) == low(*b)) {
							++a;
							++b;
						}
						if (!*b)
							return true;
					}
					return false;
				};
				if (has("android"))
					platTex = ic->android;
				else if (has("ios") || has("macos") || has("tvos") || has("watchos") || has("visionos") ||
						 has("apple"))
					platTex = ic->apple;
				else if (has("linux"))
					platTex = ic->linux;
				else if (has("web") || has("emscripten") || has("wasm"))
					platTex = ic->globe;
				else if (has("windows") || has("win"))
					platTex = ic->windowsLogo;
			}
			C(2, wPlat, 85, true, NkT("tb.plateforme"), SY.name, TEX(platTex), "monitor", false);
			C(3, wCfg, 70, false, NkT("tb.config"), kCfg[cfgI], TEX(ic ? ic->kConfig : 0), "settings", false);
			C(4, wArch, 35, false, NkT("tb.archi"), archPrev, TEX(ic ? ic->platforms : 0), "cpu", false);
			if (hasCompiler)
				C(6, wComp, 30, false, NkT("tb.compiler"), compPrev, TEX(ic ? ic->toolchains : 0), "config", false);
			if (hasTests)
				C(5, wTest, 25, false, NkT("tb.tests"), testPrev, TEX(ic ? ic->kTest : 0), "flask", false);
			Bt(0, 1000, true, NkT("tb.build"), TEX(ic ? ic->hammer : 0), "hammer", 0, nullptr);
			Bt(1, 55, false, NkT("tb.rebuild"), TEX(ic ? ic->rebuild : 0), "refresh", 0, nullptr);
			Bt(2, 50, false, NkT("tb.clean"), TEX(ic ? ic->eraser : 0), "eraser", 0, nullptr);
			Bt(3, 1000, true, NkT("tb.run"), TEX(ic ? ic->play : 0), "play", 1, nullptr);
			Bt(4, 45, false, NkT("tb.debug"), TEX(ic ? ic->bug : 0), "bug", 0, nullptr);
			Bt(5, 65, false, NkT("tb.test"), TEX(ic ? ic->kTest : 0), "flask", 2, hasTests ? testBadge.CStr() : nullptr);
			// Bouton "Emulateur" : visible seulement pour Android/HarmonyOS (plateformes
			// avec un emulateur PC lancable) — ouvre un onglet terminal avec la commande
			// de lancement DEJA TAPEE (l'utilisateur valide lui-meme, cf. ResolveEmulatorLaunchHint).
			{
				auto sysHas = [&](const char *k) {
					auto low = [](char ch) { return (ch >= 'A' && ch <= 'Z') ? char(ch + 32) : ch; };
					for (const char *h = SY.name; h && *h; ++h) {
						const char *a = h, *b = k;
						while (*a && *b && low(*a) == low(*b)) {
							++a;
							++b;
						}
						if (!*b)
							return true;
					}
					return false;
				};
				if (sysHas("android") || sysHas("harmonyos"))
					Bt(6, 40, false, NkT("tb.emulator"), TEX(ic ? ic->monitor : 0), "monitor", 0, nullptr);
			}
			// ── Champ « Arguments » (kind 3) : parametres passes a l'EXECUTABLE par
			// « Demarrer » — equivalent de `jenga run <proj> --args ...`. Priorite 15 :
			// se replie juste apres la recherche quand la fenetre rétrécit.
			segs.PushBack({3, -2, u.s(150), 15, true, NkT("tb.runargs"), nullptr, TEX(ic ? ic->play : 0), "play",
						   false, 0, nullptr});
			segs.PushBack({2, -1, wSearch, 10, true, NkT("tb.quicksearch"), nullptr, TEX(ic ? ic->search : 0), "search",
						   false, 0, nullptr}); // recherche : repliée en 1er, absente du menu

			// Largeur totale + décision de repli.
			auto sgGap = [&](usize i) { return (i == 0) ? 0.f : (segs[i].grp ? sep : gap); };
			float32 contentW = 0.f;
			for (usize i = 0; i < segs.Size(); ++i)
				contentW += segs[i].w + sgGap(i);
			const float32 margin = u.s(8), ovBtnW = u.s(30);
			const float32 avail = r.w - 2.f * margin;
			const bool overflow = contentW > avail;

			bool vis[24];
			for (usize i = 0; i < segs.Size(); ++i)
				vis[i] = true;
			NkVector<int32> dropped;
			if (overflow) {
				while (true) {
					float32 w = 0.f;
					bool first = true;
					for (usize i = 0; i < segs.Size(); ++i)
						if (vis[i]) {
							w += segs[i].w + (first ? 0.f : (segs[i].grp ? sep : gap));
							first = false;
						}
					if (w + ovBtnW + gap <= avail)
						break;
					int32 lo = -1;
					for (usize i = 0; i < segs.Size(); ++i)
						if (vis[i] && segs[i].prio < 1000 && (lo < 0 || segs[i].prio < segs[(usize)lo].prio))
							lo = (int32)i;
					if (lo < 0)
						break;
					vis[(usize)lo] = false;
					dropped.PushBack(lo);
				}
			}

			// Actions boutons (id) + dessin d'un segment à x.
			auto doBtn = [&](int32 aid) {
				switch (aid) {
					case 0:
						s->DoBuildAction("build");
						break;
					case 1:
						s->DoBuildAction("rebuild");
						break;
					case 2:
						s->DoClean();
						break;
					case 3:
						s->DoRun();
						break;
					case 4:
						s->DoRun();
						break;
					case 5:
						if (hasTests)
							s->DoTest();
						break;
					case 6: {
						NkString cwd;
						const NkString cmd = s->ResolveEmulatorLaunchHint(cwd);
						if (!cmd.Empty()) {
							s->termOpenKind = -1;
							s->termOpenAt = cwd;
							s->termOpenRun = false; // shells, pas le panneau EXECUTION
							s->termOpenType = cmd;
						}
						break;
					}
				}
			};
			auto drawSeg = [&](const Seg &sg, float32 sx) {
				if (sg.kind == 0)
					combo(sx, sg.label, sg.value, sg.tex, sg.drawn, sg.accent, sg.id, sg.w);
				else if (sg.kind == 1) {
					btn(sx, sg.label, sg.tex, sg.drawn, sg.variant, sg.badge);
					const NkRect b = {sx, cyBtn - u.s(13), sg.w, u.s(26)};
					if (tb.open < 0 && u.Hit(b) && u.click)
						doBtn(sg.id);
				} else if (sg.kind == 3) {
					// ── Champ « Arguments » : passes a l'EXECUTABLE par « Demarrer »
					// (equivalent de `jenga run <proj> --args ...`). Persistes avec la
					// session du workspace.
					const NkRect ab = {sx, ctrlY, sg.w, ctrlH};
					u.Panel(ab, NkCol::input, NkCol::border, NkR::sm * u.S);
					if (u.click && tb.open < 0)
						s->tbArgsFocus = u.Hit(ab);
					editorkit::NkOverlayTextField(*u.ctx, *u.dl, u.ctx->font, ab, s->runArgs,
												  (int32)sizeof(s->runArgs), s->tbArgsFocus);
					if (s->runArgs[0] == 0 && !s->tbArgsFocus)
						u.TextV(ab.x + u.s(8), ab.y, ab.h, NkT("tb.runargs.ph"), NkCol::mutedFg);
				} else {
					// ── Champ « Recherche rapide » = QUICK-OPEN façon VSCode ──
					// Défaut : recherche FICHIERS (liste déroulante live, fuzzy sur le nom/chemin).
					// Préfixe `?` : recherche TEXTE projet (Entrée -> panneau Recherche).
					// Entrée ouvre le résultat sélectionné ; ↑/↓ navigue ; Échap ferme.
					const NkRect sb = {sx, ctrlY, sg.w, ctrlH};
					s->StartFileIndex(); // index fichiers (tâche de fond, une seule fois)
					const bool textMode = s->tbSearch[0] == '?';
					const char *filter = textMode ? s->tbSearch + 1 : s->tbSearch;
					if (!textMode) {
						if (!StrEq(s->tbSearch, s->tbQoLast.CStr())) {
							s->QoRefresh(filter);
							s->tbQoLast = NkString(s->tbSearch);
						}
					} else
						s->tbQoHits.Clear();
					// ── Géométrie de la liste déroulante quick-open (sous le champ) ──
					const int32 n = (int32)s->tbQoHits.Size();
					const bool showDD = s->tbSearchFocus && !textMode && n > 0;
					const float32 ddRowH = u.s(24), pad = u.s(4), sbW = u.s(14.f); // 14 = NkScrollbarWidth()
					const float32 fullW = sg.w + u.s(230);
					// Largeur du contenu (scroll horizontal) = la ligne la plus large.
					float32 contentW = 0.f;
					if (showDD)
						for (int32 r2 = 0; r2 < n; ++r2) {
							const char *rel = s->qoRel[s->tbQoHits[r2]].CStr();
							const char *fn = NkCodeState::QoFileName(rel);
							float32 w2 = u.s(5) + u.TextW(fn) + u.s(9);
							if (fn != rel) {
								NkString d; for (const char *z = rel; z < fn; ++z) d += *z;
								w2 += u.s(8) + u.TextW(d.CStr());
							}
							if (w2 > contentW) contentW = w2;
						}
					const int32 visRows = n < 12 ? n : 12;
					const float32 viewH = (float32)visRows * ddRowH;
					const float32 contentH = (float32)n * ddRowH;
					const bool needV = contentH > viewH + 0.5f;
					const float32 viewW = fullW - (needV ? sbW : 0.f) - pad * 2.f;
					const bool needH = contentW > viewW + 0.5f;
					const float32 ddH = viewH + pad * 2.f + (needH ? sbW : 0.f);
					const NkRect dd = {sb.x, sb.y + ctrlH + u.s(3), fullW, ddH};
					const bool hitDD = showDD && u.Hit(dd);
					// Focus : clic dans le champ OU la liste -> garde ; clic ailleurs -> défocus.
					if (u.click && tb.open < 0)
						s->tbSearchFocus = (u.Hit(sb) || hitDD);
					editorkit::NkOverlayTextField(*u.ctx, *u.dl, u.ctx->font, sb, s->tbSearch,
						                              (int32)sizeof(s->tbSearch), s->tbSearchFocus);
					if (s->tbSearch[0] == 0 && !s->tbSearchFocus)
						u.Text(sb.x + u.s(9), ctrlY + (ctrlH - u.Lh()) * 0.5f, sg.label, NkCol::mutedFg);
					NkOwIco(u, sg.tex, "search",
						        {sb.x + sg.w - u.s(18), ctrlY + (ctrlH - u.s(12)) * 0.5f, u.s(12), u.s(12)},
						        textMode ? NkCol::primary : NkCol::mutedFg);
					// ── Liste (fichiers) : calque OVERLAY, contenu CLIPPÉ + scrollbars thémés ──
					if (showDD) {
						const NkUi uo = NkUi::From(ec, true);
						auto &in = u.ctx->input;
						uo.dl->AddRectFilled(dd, NkCol::surface, NkR::sm * u.S);
						uo.dl->AddRect(dd, NkCol::border, 1.f);
						if (s->tbQoSel >= n) s->tbQoSel = n - 1;
						if (s->tbQoSel < 0) s->tbQoSel = 0;
						if (in.KeyPressedRepeat(nkgui::NkGuiKey::Down)) s->tbQoSel = (s->tbQoSel + 1) % n;
						if (in.KeyPressedRepeat(nkgui::NkGuiKey::Up)) s->tbQoSel = (s->tbQoSel - 1 + n) % n;
						// Molette (verticale/horizontale) quand la souris est sur la liste.
						if (hitDD && in.wheel != 0.f) { s->tbQoScrollY -= in.wheel * ddRowH * 3.f; in.wheel = 0.f; }
						if (hitDD && in.wheelH != 0.f) { s->tbQoScrollX -= in.wheelH * u.s(40); in.wheelH = 0.f; }
						// Auto-scroll : garde la ligne sélectionnée visible (navigation clavier).
						const float32 selTop = (float32)s->tbQoSel * ddRowH;
						if (selTop < s->tbQoScrollY) s->tbQoScrollY = selTop;
						if (selTop + ddRowH > s->tbQoScrollY + viewH) s->tbQoScrollY = selTop + ddRowH - viewH;
						const float32 maxY = contentH - viewH > 0.f ? contentH - viewH : 0.f;
						const float32 maxX = contentW - viewW > 0.f ? contentW - viewW : 0.f;
						s->tbQoScrollY = s->tbQoScrollY < 0.f ? 0.f : (s->tbQoScrollY > maxY ? maxY : s->tbQoScrollY);
						s->tbQoScrollX = s->tbQoScrollX < 0.f ? 0.f : (s->tbQoScrollX > maxX ? maxX : s->tbQoScrollX);
						// Zone des lignes, CLIPPÉE (le texte ne déborde plus).
						const NkRect clip = {dd.x + pad, dd.y + pad, viewW, viewH};
						uo.dl->PushClipRect(clip, true);
						for (int32 r2 = 0; r2 < n; ++r2) {
							const float32 rowY = clip.y - s->tbQoScrollY + (float32)r2 * ddRowH;
							if (rowY + ddRowH <= clip.y || rowY >= clip.y + clip.h) continue; // hors vue
							const NkRect vis = {clip.x, rowY, viewW, ddRowH};
							const bool rhov = u.Hit(vis) && u.Hit(clip);
							if (rhov) s->tbQoSel = r2;
							if (r2 == s->tbQoSel) uo.dl->AddRectFilled(vis, NkCol::selection, NkR::sm * u.S);
							const char *rel = s->qoRel[s->tbQoHits[r2]].CStr();
							const char *fn = NkCodeState::QoFileName(rel);
							const float32 tx = clip.x + u.s(5) - s->tbQoScrollX;
							const float32 ty = rowY + (ddRowH - u.Lh()) * 0.5f;
							uo.Text(tx, ty, fn, NkCol::foreground);
							if (fn != rel) {
								NkString dir2; for (const char *z = rel; z < fn; ++z) dir2 += *z;
								uo.Text(tx + u.TextW(fn) + u.s(8), ty, dir2.CStr(), NkCol::mutedFg);
							}
							if (rhov && u.click) {
								s->OpenPath(NkPath(s->qoAbs[s->tbQoHits[r2]]));
								s->tbSearch[0] = 0; s->tbQoHits.Clear(); s->tbSearchFocus = false;
							}
						}
						uo.dl->PopClipRect();
						// Scrollbars standard (NKEditorKit) — thémés, personnalisables par l'utilisateur.
						if (needV) {
							const NkRect vtrack = {dd.x + fullW - sbW, dd.y, sbW, viewH + pad * 2.f};
							editorkit::NkVScrollbar(*u.ctx, *uo.dl, vtrack, s->tbQoScrollY, contentH, viewH, 0x5EA3C401u, ddRowH);
						}
						if (needH) {
							const NkRect htrack = {dd.x, dd.y + viewH + pad * 2.f, fullW - (needV ? sbW : 0.f), sbW};
							editorkit::NkHScrollbar(*u.ctx, *uo.dl, htrack, s->tbQoScrollX, contentW, viewW, 0x5EA3C402u);
						}
						if (u.click && hitDD) in.mouseClicked[0] = false; // consommé -> pas de fuite vers l'éditeur
					}
					// Entrée : ouvre le fichier sélectionné (mode fichiers) ou lance la recherche
					// texte projet (mode `?`). Échap : ferme/défocus.
					if (s->tbSearchFocus && u.ctx->input.KeyPressed(nkgui::NkGuiKey::Enter) && s->tbSearch[0]) {
						if (textMode && filter[0]) {
							s->wsPrefill = NkString(filter);
							s->reqSearch = true;
							s->tbSearchFocus = false;
						} else if (!textMode && s->tbQoHits.Size() > 0) {
							const int32 sel = (s->tbQoSel >= 0 && s->tbQoSel < (int32)s->tbQoHits.Size()) ? s->tbQoSel : 0;
							s->OpenPath(NkPath(s->qoAbs[s->tbQoHits[sel]]));
							s->tbSearch[0] = 0; s->tbQoHits.Clear(); s->tbSearchFocus = false;
						}
					}
					if (s->tbSearchFocus && u.ctx->input.KeyPressed(nkgui::NkGuiKey::Escape))
						s->tbSearchFocus = false;
				}
			};

			float32 startX = overflow ? (r.x + margin) : (r.x + (r.w - contentW) * 0.5f);
			if (startX < r.x + margin)
				startX = r.x + margin;
			{
				float32 cx = startX;
				bool first = true;
				for (usize i = 0; i < segs.Size(); ++i)
					if (vis[i]) {
						if (!first)
							cx += (segs[i].grp ? sep : gap);
						drawSeg(segs[i], cx);
						cx += segs[i].w;
						first = false;
					}
			}

			// Bouton de repli « » » (seulement si des segments ont été repliés).
			NkRect ovBtn = {0, 0, 0, 0};
			const bool hasOv = !dropped.Empty();
			if (hasOv) {
				ovBtn = {r.x + r.w - margin - ovBtnW, ctrlY, ovBtnW, ctrlH};
				const bool ovOpen = (tb.open == 100);
				u.Panel(ovBtn, ovOpen ? NkCol::secondary : NkCol::muted, ovOpen ? NkCol::primary : NkCol::border,
						NkR::sm * u.S);
				u.Text(ovBtn.x + (ovBtnW - u.TextW("\xC2\xBB")) * 0.5f, ctrlY + (ctrlH - u.Lh()) * 0.5f, "\xC2\xBB",
					   NkCol::foreground);
				if (u.Hit(ovBtn) && u.click) {
					if (ovOpen)
						tb.open = -1;
					else {
						tb.open = 100;
						tb.openR = ovBtn;
						tb.justOpened = true;
					}
				}
			} else if (tb.open == 100)
				tb.open = -1; // plus rien de replié -> ferme le menu

			// ── Dropdown différé (par-dessus TOUT : couche overlay, sinon l'éditeur le recouvre) ──
			const bool ddWasOpen =
				(tb.open >= 0); // (le clic de fermeture est consommé aussi -> pas de fuite vers le corps)
			if (tb.open >= 0 && tb.open < 100) {
				const NkUi uo = NkUi::From(ec, /*overlay*/ true); // dessine dans dlOverlay (composité en dernier)
				const NkRect a = tb.openR;

				struct Item {
						NkString label;
						int32 idx;
						// Colonne de GAUCHE. `icon` prime quand elle existe (bien plus
						// compact que « (WindowedApp) ») ; `hint` sert de repli en toutes
						// lettres pour un Kind sans icone — un nouveau Kind reste donc
						// lisible sans avoir a dessiner quoi que ce soit.
						NkString hint;
						uint32 icon = 0;
				};

				NkVector<Item> items;
				int32 curSel = -1;
				switch (tb.open) {
					case 0:
						for (usize i = 0; i < s->wsNames.Size(); ++i)
							items.PushBack({s->wsNames[i], (int32)i});
						curSel = s->wsIdx;
						break;
					case 1: {
						const int32 np = (int32)s->projects.Size();
						for (int32 i = 0; i < np; ++i) {
							// Le Kind en colonne de gauche : on voit d'un coup d'oeil ce
							// qui est executable et ce qui ne l'est pas, sans avoir a
							// essayer pour le decouvrir. Icone quand elle existe (les
							// memes que l'assistant de creation de projet), sinon le nom
							// du Kind en toutes lettres.
							const NkString k = (i < (int32)s->projKinds.Size()) ? s->projKinds[i] : NkString();
							// `hint` est TOUJOURS rempli, meme quand une icone existe : il
							// reste la matiere du filtre (taper « console » doit marcher,
							// une icone n'est pas cherchable) et le repli d'affichage.
							Item it{s->projects[i], i, k.Empty() ? NkString() : (NkString("(") + k.CStr() + ")"), 0};
							it.icon = NkKindTex(s->icons, k.CStr());
							items.PushBack(it);
						}
						items.PushBack({NkString(NkT("tb.allprojects")), np, NkString(), 0});
						curSel = s->projIdx;
					} break;
					case 2:
						for (int32 i = 0; i < nSys; ++i)
							items.PushBack({NkString(sysd[i].name), i});
						curSel = s->sysIdx;
						break;
					case 3:
						for (int32 i = 0; i < 3; ++i)
							items.PushBack({NkString(kCfg[i]), i});
						curSel = cfgI;
						break;
					case 4: {
						for (int32 i = 0; i < SY.nArch; ++i)
							items.PushBack({NkString(SY.archs[i]), i});
						items.PushBack({NkString(NkT("tb.all")), SY.nArch});
						curSel = s->archIdx;
					} break;
					case 5: {
						items.PushBack({NkString(NkT("tb.alltests")), -1});
						for (int32 i = 0; i < (int32)s->tests.Size(); ++i)
							if (s->TestVisible(i))
								items.PushBack({s->tests[i], i});
						curSel = s->testIdx;
					} break;
					case 6: {
						items.PushBack({NkString(NkT("tb.compilerauto")), -1});
						const NkVector<int32> cs = s->CompilersForCurrentPlatform();
						for (usize i = 0; i < cs.Size(); ++i) {
							items.PushBack({s->toolchains[cs[i]].name, (int32)i});
							if (NkCodeState::StrEqI(s->toolchains[cs[i]].name.CStr(), s->compilerName.CStr()))
								curSel = (int32)i;
						}
					} break;
				}
				const float32 ih = u.s(24);
				if (tb.justOpened) {
					tb.scroll = 0.f;
					tb.filter[0] = '\0';   // chaque ouverture repart d'une liste complete
					tb.filterFocus = true; // on peut taper immediatement
				}
				// ── Champ de recherche, seulement quand la liste le justifie ────────
				// En dessous de ce seuil tout tient a l'ecran : un champ n'apporterait
				// rien et volerait une ligne.
				const bool avecFiltre = items.Size() > 8;
				const float32 searchH = avecFiltre ? u.s(26) : 0.f;
				// Filtrage insensible a la casse, sur le libelle ET sur le Kind — taper
				// « console » liste toutes les applications console.
				NkVector<Item> shown;
				if (avecFiltre && tb.filter[0]) {
					for (usize i = 0; i < items.Size(); ++i)
						if (NkCodeState::ContainsI(items[i].label.CStr(), tb.filter) ||
							NkCodeState::ContainsI(items[i].hint.CStr(), tb.filter))
							shown.PushBack(items[i]);
				} else
					shown = items;

				// Largeur de la COLONNE de gauche (le Kind), pour que tous les noms
				// demarrent au meme x. Une icone tient dans un carre ; un repli
				// textuel demande la largeur du plus long. Le maximum des deux gagne :
				// une seule ligne sans icone suffit a elargir la colonne, et
				// l'alignement reste parfait.
				// Mesuree sur ce qui est REELLEMENT AFFICHE : une icone occupe un carre,
				// un repli textuel sa largeur de texte. `hint` est aussi rempli quand
				// une icone existe (il sert au filtre) — le mesurer donnerait une
				// colonne large et vide.
				const float32 iconW = u.s(16);
				float32 kindW = 0.f;
				for (usize i = 0; i < items.Size(); ++i) {
					const float32 w = items[i].icon ? iconW
												: (items[i].hint.Empty() ? 0.f : u.TextW(items[i].hint.CStr()));
					if (w > kindW)
						kindW = w;
				}
				if (kindW > u.s(120))
					kindW = u.s(120);

				// Largeur BORNÉE (ellipse au-delà) -> pas de scroll horizontal nécessaire.
				float32 ddw = a.w;
				for (usize i = 0; i < items.Size(); ++i) {
					float32 tw = u.TextW(items[i].label.CStr()) + u.s(28);
					if (kindW > 0.f)
						tw += kindW + u.s(10);
					if (tw > ddw)
						ddw = tw;
				}
				// Plafond de largeur : releve quand il y a une colonne de Kind, sinon
				// celle-ci mangerait la place des noms et tout finirait en ellipse.
				const float32 maxDdw = kindW > 0.f ? u.s(460) : u.s(340);
				if (ddw > maxDdw)
					ddw = maxDdw;
				// Hauteur PLAFONNÉE par l'écran -> défilement vertical si trop d'items.
				const float32 pad = u.s(6);
				const float32 contentH = (float32)shown.Size() * ih + pad;
				float32 maxH = (float32)ec.Ui().viewH - (a.y + a.h + u.s(2)) - u.s(12);
				if (maxH > u.s(440))
					maxH = u.s(440);
				if (maxH < ih + pad)
					maxH = ih + pad;
				// La bande de recherche a sa PROPRE hauteur, hors du calcul de
				// defilement : c'est precisement ce qui la rend insensible au scroll.
				if (maxH > searchH + ih + pad)
					maxH -= searchH;
				const bool scroll = contentH > maxH;
				const float32 ddh = (scroll ? maxH : contentH) + searchH;
				const float32 sbW = scroll ? editorkit::NkScrollbarWidth() : 0.f; // largeur canonique (= editeur)
				float32 ddx = a.x;
				if (ddx + ddw > r.x + r.w - u.s(8))
					ddx = r.x + r.w - u.s(8) - ddw;
				const NkRect dd = {ddx, a.y + a.h + u.s(2), ddw, ddh};
				uo.dl->AddRectFilled({dd.x + u.s(2), dd.y + u.s(3), dd.w, dd.h}, NkColor{0, 0, 0, 110}, NkR::md * u.S);
				uo.Panel(dd, NkCol::surface, NkCol::border, NkR::md * u.S); // bordure discrete (coherence menus)
				const float32 maxScroll = scroll ? (contentH - ddh) : 0.f;
				if (scroll && u.Hit(dd)) {
					tb.scroll -= ec.Ui().input.wheel * ih * 2.f;
					ec.Ui().input.wheel = 0.f;
				}
				if (tb.scroll < 0.f)
					tb.scroll = 0.f;
				if (tb.scroll > maxScroll)
					tb.scroll = maxScroll;
				// ── Bande de recherche ANCREE : dessinee AVANT le clip de la liste,
				// donc jamais rognee ni deplacee par le defilement.
				if (avecFiltre) {
					const NkRect fr = {dd.x + u.s(4), dd.y + u.s(4), dd.w - u.s(8), searchH - u.s(6)};
					if (u.Hit(fr) && u.click)
						tb.filterFocus = true;
					editorkit::NkOverlayTextField(ec.Ui(), *uo.dl, ec.Ui().font, fr, tb.filter,
												  (int32)sizeof(tb.filter), tb.filterFocus);
					if (!tb.filter[0] && ec.Ui().font && ec.Ui().font->Valid())
						uo.TextEllipsis(fr.x + u.s(8), fr.y + (fr.h - u.Lh()) * 0.5f, fr.w - u.s(16),
										NkT("tb.filter.hint"), NkCol::mutedFg);
					// Repartir du haut UNIQUEMENT quand le filtre change : sinon on
					// resterait a defiler dans le vide au-dessus d'une liste devenue
					// courte. Le remettre a zero a chaque frame desactiverait le
					// defilement pur et simple.
					static char s_prev[64] = {};
					bool change = false;
					for (int32 k = 0; k < (int32)sizeof(s_prev); ++k) {
						if (s_prev[k] != tb.filter[k]) {
							change = true;
							break;
						}
						if (!tb.filter[k])
							break;
					}
					if (change) {
						for (int32 k = 0; k < (int32)sizeof(s_prev); ++k)
							s_prev[k] = tb.filter[k];
						tb.scroll = 0.f;
					}
				}
				const NkRect inner = {dd.x, dd.y + searchH + u.s(3), dd.w - sbW, ddh - searchH - u.s(6)};
				uo.dl->PushClipRect(inner, true);
				bool chose = false;
				for (usize i = 0; i < shown.Size(); ++i) {
					const float32 iy = dd.y + searchH + u.s(3) + (float32)i * ih - tb.scroll;
					if (iy + ih < inner.y || iy > inner.y + inner.h)
						continue; // hors vue
					const NkRect ir = {dd.x + u.s(4), iy, dd.w - u.s(8) - sbW, ih};
					const bool vis2 = (iy >= inner.y - 1.f && iy + ih <= inner.y + inner.h + 1.f);
					const bool hv = vis2 && u.Hit(ir);
					const bool selrow = (shown[i].idx == curSel);
					if (hv || selrow)
						uo.Rect(ir, NkCol::hover, NkR::sm * u.S);
					// Deux colonnes, toutes deux alignees a GAUCHE. Le Kind D'ABORD, dans
					// une colonne de largeur FIXE calee sur le plus long : c'est ce qui
					// fait que les noms demarrent tous au meme x, donc que la liste se
					// lise comme un tableau. Le Kind passe en premier parce que les noms
					// d'application, eux, sont de longueur tres variable — les mettre a
					// gauche decalerait la seconde colonne a chaque ligne.
					const float32 ty = ir.y + (ih - u.Lh()) * 0.5f;
					float32 nx = ir.x + u.s(8);
					if (kindW > 0.f) {
						if (shown[i].icon) {
							const NkRect icr = {nx, ir.y + (ih - iconW) * 0.5f, iconW, iconW};
							NkDrawIcon(uo, shown[i].icon, icr, NkCol::mutedFg);
						} else if (!shown[i].hint.Empty())
							uo.TextEllipsis(nx, ty, kindW, shown[i].hint.CStr(), NkCol::mutedFg);
						nx += kindW + u.s(10);
					}
					float32 lw = ir.x + ir.w - u.s(6) - nx;
					if (lw < u.s(40))
						lw = u.s(40);
					uo.TextEllipsis(nx, ty, lw, shown[i].label.CStr(),
									selrow ? NkCol::primary : NkCol::foreground);
					if (hv && u.click) {
						switch (tb.open) {
							case 0:
								s->wsIdx = shown[i].idx;
								s->RequestReload();
								break;
							case 1:
								s->projIdx = shown[i].idx;
								break;
							case 2:
								s->sysIdx = shown[i].idx;
								s->archIdx = 0;
								break;
							case 3:
								s->cfgIdx = shown[i].idx;
								break;
							case 4:
								s->archIdx = shown[i].idx;
								break;
							case 5:
								s->testIdx = shown[i].idx;
								break;
							case 6:
								if (shown[i].idx < 0)
									s->compilerName = NkString();
								else {
									const NkVector<int32> cs = s->CompilersForCurrentPlatform();
									if (shown[i].idx < (int32)cs.Size())
										s->compilerName = s->toolchains[cs[shown[i].idx]].name;
								}
								break;
						}
						chose = true;
					}
				}
				uo.dl->PopClipRect();
				// Barre de defilement GENERALE (widget moteur NKEditorKit, identique a l'editeur :
				// gouttiere theme-aware + fleches + pouce draggable + clic-piste).
				if (scroll) {
					const NkRect track = {dd.x + dd.w - sbW, dd.y + u.s(2), sbW, ddh - u.s(4)};
					editorkit::NkVScrollbar(ec.Ui(), *uo.dl, track, tb.scroll, contentH, track.h, 0x5010D0Du, ih);
				}
				if (chose || (u.click && !u.Hit(dd) && !tb.justOpened && !tb.scrollDrag))
					tb.open = -1;
				// MODAL : rien DERRIERE le dropdown ne doit voir ces evenements (clic/molette).
				if (u.Hit(dd)) {
					ec.Ui().input.mouseClicked[0] = false;
					ec.Ui().input.mouseClicked[1] = false;
					ec.Ui().input.wheel = 0.f;
				}
				tb.justOpened = false;
			}

			// ── Menu de REPLI (overflow) : liste les segments masqués. Un combo y ouvre
			// son propre dropdown (bascule tb.open sur son id) ; un bouton exécute son action. ──
			if (tb.open == 100 && hasOv) {
				const NkUi uo = NkUi::From(ec, /*overlay*/ true);
				int32 nrows = 0;
				float32 mw = u.s(190);
				for (usize k = 0; k < dropped.Size(); ++k) {
					const Seg &sg = segs[dropped[k]];
					if (sg.kind == 2)
						continue;
					++nrows;
					float32 tw = u.TextW(sg.label) + (sg.value ? u.TextW(sg.value) : 0.f) + u.s(70);
					if (tw > mw)
						mw = tw;
				}
				const float32 ih = u.s(28);
				float32 mx = ovBtn.x + ovBtn.w - mw;
				if (mx < r.x + u.s(8))
					mx = r.x + u.s(8);
				const NkRect mm = {mx, ovBtn.y + ovBtn.h + u.s(2), mw, nrows * ih + u.s(6)};
				uo.dl->AddRectFilled({mm.x + u.s(2), mm.y + u.s(3), mm.w, mm.h}, NkColor{0, 0, 0, 110}, NkR::md * u.S);
				uo.Panel(mm, NkCol::surface, NkCol::primary, NkR::md * u.S);
				bool chose = false;
				float32 ry = mm.y + u.s(3);
				for (usize k = 0; k < dropped.Size(); ++k) {
					const Seg &sg = segs[dropped[k]];
					if (sg.kind == 2)
						continue;
					const NkRect ir = {mm.x + u.s(4), ry, mm.w - u.s(8), ih};
					const bool hv = u.Hit(ir);
					if (hv)
						uo.Rect(ir, NkCol::hover, NkR::sm * u.S);
					NkOwIco(uo, sg.tex, sg.drawn, {ir.x + u.s(8), ry + (ih - u.s(13)) * 0.5f, u.s(13), u.s(13)},
							sg.accent ? NkCol::primary : NkCol::mutedFg);
					uo.Text(ir.x + u.s(32), ry + (ih - u.Lh()) * 0.5f, sg.label, NkCol::foreground);
					if (sg.kind == 0) {
						if (sg.value)
							uo.TextEllipsis(ir.x + mm.w * 0.5f, ry + (ih - u.Lh()) * 0.5f, mm.w * 0.5f - u.s(24),
											sg.value, NkCol::mutedFg);
						NkOwIco(uo, 0u, "chevron-right",
								{ir.x + ir.w - u.s(14), ry + (ih - u.s(9)) * 0.5f, u.s(9), u.s(9)}, NkCol::mutedFg);
					}
					if (hv && u.click) {
						if (sg.kind == 0) {
							tb.open = sg.id;
							tb.openR = ir;
							tb.justOpened = true;
						} else {
							doBtn(sg.id);
							tb.open = -1;
						}
						chose = true;
					}
					ry += ih;
				}
				if (!chose && u.click && !u.Hit(mm) && !u.Hit(ovBtn) && !tb.justOpened)
					tb.open = -1;
				tb.justOpened = false;
			}
			// MODAL : tant qu'un dropdown est (ou vient d'être) ouvert, le corps
			// (éditeur/panneaux, dessinés APRÈS la toolbar) ne doit recevoir ni survol ni
			// clic. On lève `appModal` : le shell masque alors l'input du corps de façon
			// NON DESTRUCTIVE (save/restore de ec.Ui().input autour des panneaux). L'ancienne
			// approche écrasait mousePos/mouseDown en PERSISTANT — or mousePos n'est rafraîchi
			// que sur mouvement souris — ce qui gelait l'input et rendait les combos non
			// modifiables. `ddWasOpen` (état AVANT fermeture) protège aussi le clic de fermeture.
			if (ddWasOpen)
				ec.Ui().appModal = true;
		}

	} // namespace nkcode
} // namespace nkentseu
