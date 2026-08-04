#pragma once
// =============================================================================
// NkHome.h — Ecran d'accueil (Launcher Home), reecriture propre d'apres le
// design Banani « Launcher — Accueil ». Sidebar (marque + navigation + versions)
// + panneau (filtres, workspaces recents groupes, actions rapides, exemples).
// =============================================================================
#include "NKCode/Shell/NkShell.h" // nkcode::NkShellRun (std::system gardé iOS)
#include "NKEditorKit/NkEditorScrollbar.h"
#include "NKCode/Shell/NkUi.h"
#include "NKCode/Project/NkCodeState.h"
#include "NKCode/Shell/Dialogs.h"		 // reutilise la logique d'actions (ouvrir/creer)
#include "NKCode/Shell/NkOpenWs.h"		 // vue « Ouvrir un Workspace » (navigateur de fichiers)
#include "NKCode/Shell/NkNewWorkspace.h" // wizard « Nouveau Workspace »
#include "NKCode/Shell/NkCloneGit.h"	 // panneau « Cloner un depot Git »
#include "NKCode/Shell/NkToolchains.h"	 // gestionnaire de toolchains
#include "NKCode/Shell/NkPlatforms.h"	 // gestionnaire de plateformes
#include "NKCode/Shell/NkSettings.h"	 // parametres du launcher (nav==12)
#include "NKCode/Shell/NkLoading.h"		  // ecran de chargement (section 14)
#include "NKContainers/String/NkFormat.h" // NkPrintf (formatage maison, ex-<cstdio>)
#include "NKMath/NkFunctions.h"			  // math::NkSin / math::NkCos (maths maison, ex-<cmath>)

namespace nkentseu {
	namespace nkcode {

		struct NkHomeState {
				NkCodeState *st = nullptr;
				NkCodeDialogs *dlg = nullptr;
				uint32 logoIcon = 0, logoWord = 0,
					   logoWordDark = 0; // textures (0 = repli) : wordmark clair (nkcode_white) + sombre (nkcode_dark)
				int32 wordW = 0, wordH = 0;	  // dimensions naturelles du wordmark clair (aspect)
				int32 wordWD = 0, wordHD = 0; // dimensions du wordmark sombre (thème Light)
				NkIcons icons;				  // icones SVG (data/textures/icon)
				int32 nav = 0;				  // item de nav actif (0 = Accueil, 1 = Ouvrir, 2 = Nouveau Workspace)
				NkOpenWsState ow;			  // etat de la vue « Ouvrir un Workspace »
				NkNewWsState nw;			  // etat du wizard « Nouveau Workspace »
				NkCloneGitState cg;			  // etat du panneau « Cloner un depot Git »
				NkToolchainsState tc;		  // etat du gestionnaire de toolchains
				NkSettingsState settings;	  // etat des parametres du launcher (nav==12)
				float32 scroll = 0.f;		  // centre : defilement vertical (recents)
				float32 scrollR = 0.f;		  // droite : defilement vertical
				float32 scrollMax = 0.f;	  // borne max du centre (frame precedente) -> anti-clignotement
				float32 scrollRMax = 0.f;	  // borne max de droite (frame precedente)
				int32 barDrag = 0;			  // scrollbar en cours de drag (0 aucun)
				float32 barOff = 0.f;		  // offset souris->thumb pendant le drag
				// Menu contextuel "..." d'une carte workspace.
				int32 ctxIdx = -2;		   // -2 = ferme ; sinon index carte (2000 courant / 1000+i epingle / i recent)
				NkVec2 ctxPos{};		   // coin haut-gauche du menu
				NkString ctxPath;		   // chemin .jenga de la cible
				bool ctxPinned = false;	   // la cible est-elle deja epinglee ?
				bool ctxIsCurrent = false; // la cible est-elle le workspace ouvert ?
				// Filtres (barre du panneau) + recherche d'exemples.
				char searchText[64] = {}; // filtre des recents (nom/chemin)
				int32 langFilter = 0;	  // index dans NkLangFilters (0 = Tous)
				int32 sysFilter = 0;	  // index dans NkSysFilters (0 = Tous)
				int32 focusField = 0;	  // 0 aucun, 1 recherche recents, 2 recherche exemples
				bool fieldClaim = false;  // un champ a capte le clic cette frame ? (sinon clic vide = defocus)
				char exSearch[64] = {};	  // filtre des exemples
				// Combo box des chips (langage/systeme) : menu deroulant.
				int32 comboOpen = 0;	  // 0 ferme, 1 langage, 2 systeme
				NkVec2 comboPos{};		  // coin haut-gauche du deroulant
				float32 comboW = 0.f;	  // largeur du deroulant
				float32 caretBlink = 0.f; // accumulateur (s) pour le clignotement du caret
				bool groupCollapsed[24] =
					{};			  // pliage : 0 epingles ; 1..N groupes (date/plateforme/langage selon groupBy)
				NkString exePath; // chemin de l'executable NKCode (pour "nouvelle fenetre")
				// Popup de renommage dans les recents.
				bool renameOpen = false;
				NkString renamePath; // chemin .jenga cible
				NkVec2 renamePos{};
				char renameBuf[96] = {};
		};

		// Options des chips de filtre (0 = "Tous" -> pas de filtre).
		static const char *const NkLangFilters[] = {"Tous", "C++", "C", "Python", "Rust", "Zig"};
		static const char *const NkSysFilters[] = {"Tous", "Windows", "Linux",	   "macOS", "Android",
												   "Web",  "iOS",	  "HarmonyOS", "Xbox"};

		// Seul l'index 0 « Tous » est traduit ; le reste = noms propres.
		inline const char *NkFilterLabel(const char *const *opts, int32 idx) {
			return idx == 0 ? NkT("filter.all") : opts[idx];
		}

		// Libelle traduit d'un groupe de recents (bucket 0..3).
		inline const char *NkHomeBucketLabel(int32 b) {
			return NkT(b == 0 ? "bucket.today" : b == 1 ? "bucket.week" : b == 2 ? "bucket.month" : "bucket.older");
		}

		// (Les exemples sont enumeres dynamiquement via `jenga examples list` -> st->examples.)

		// ── Scrollbar VERTICALE draggable au bord droit de `area`. `id` unique. ──
		inline void NkVScroll(const NkUi &u, const NkRect &area, float32 contentH, float32 &scroll, int32 id,
							  NkHomeState *H) {
			const float32 maxS = contentH > area.h ? contentH - area.h : 0.f;
			if (scroll < 0.f)
				scroll = 0.f;
			if (scroll > maxS)
				scroll = maxS;
			if (maxS <= 0.5f) {
				if (H->barDrag == id)
					H->barDrag = 0;
				return;
			}
			(void)H; // drag gere par ctx.activeId dans le widget
			const float32 sw = editorkit::NkScrollbarWidth();
			const NkRect track = {area.x + area.w - sw, area.y, sw, area.h};
			editorkit::NkVScrollbar(*u.ctx, *u.dl, track, scroll, contentH, area.h, 0x40E00000u + (uint32)id,
									u.s(28)); // scrollbar general (= editeur)
		}

		// ── Item de navigation de la sidebar ──
		inline bool NkNavItem(const NkUi &u, const NkRect &r, uint32 icon, const char *label, bool active,
							  const NkColor &accent) {
			const bool hov = u.Hit(r);
			if (active)
				u.Rect(r, NkCol::selection, NkR::md * u.S);
			else if (hov)
				u.Rect(r, NkCol::hover, NkR::md * u.S);
			if (active)
				u.Rect({r.x, r.y + u.s(6), u.s(3), r.h - u.s(12)}, accent, u.s(2));
			const NkColor ic = active ? accent : NkColor{188, 196, 206, 255}; // icone plus lumineuse
			const float32 isz = u.s(19);
			NkDrawIcon(u, icon, {r.x + u.s(13), r.y + (r.h - isz) * 0.5f, isz, isz}, ic);
			u.TextV(r.x + u.s(42), r.y, r.h, label, active ? NkCol::foreground : NkCol::sidebarFg);
			return hov && u.click;
		}

		// ── Sidebar ──
		inline void NkHomeSidebar(const NkUi &u, const NkRect &r, NkHomeState *H) {
			u.Rect(r, NkCol::sidebar);
			u.Rect({r.x + r.w - 1.f, r.y, 1.f, r.h}, NkCol::border);

			// Logo : PAS de fond/cadre — le wordmark existe en 2 versions PRÊTES (nkcode_white pour
			// fond sombre, nkcode_dark pour fond clair), donc il s'affiche net directement sur la sidebar.
			const float32 logoH = u.s(78);
			const NkRect box = {r.x + u.s(12), r.y + u.s(12), r.w - u.s(24), logoH - u.s(22)};
			const bool lightTheme = NkThemeIsLight();
			const bool useDark = lightTheme && H->logoWordDark;
			const uint32 wordTex = useDark ? H->logoWordDark : H->logoWord;
			const int32 ww = useDark ? H->wordWD : H->wordW, wh = useDark ? H->wordHD : H->wordH;
			if (wordTex) {
				const float32 aspect = (wh > 0) ? (float32)ww / (float32)wh : 4.f;
				const float32 padX = u.s(12), padY = u.s(8);
				float32 lw = box.w - padX * 2.f, lh2 = lw / aspect;
				if (lh2 > box.h - padY * 2.f) {
					lh2 = box.h - padY * 2.f;
					lw = lh2 * aspect;
				}
				u.dl->AddImage(wordTex, {box.x + (box.w - lw) * 0.5f, box.y + (box.h - lh2) * 0.5f, lw, lh2}, {0, 0},
							   {1, 1}, NkColor{255, 255, 255, 255}); // image telle quelle
			} else {
				const NkColor tint = lightTheme ? NkColor{20, 26, 32, 255} : NkColor{235, 240, 238, 255};
				NkBrandMark(u, {box.x + u.s(8), box.y + (box.h - u.s(28)) * 0.5f, u.s(28), u.s(28)}, tint);
				u.TextV(box.x + u.s(44), box.y, box.h, "nkcode", tint);
			}

			auto section = [&](float32 y, const char *title) { u.Text(r.x + u.s(16), y, title, NkCol::mutedFg); };
			float32 y = r.y + logoH + u.s(14);
			section(y, NkT("sb.main"));
			y += u.s(20);

			struct NavDef {
					uint32 icon;
					const char *key;
			};

			const NavDef main_[] = {{H->icons.accueil, "nav.home"},
									{H->icons.ouvrir, "nav.open"},
									{H->icons.nouveau, "nav.newws"},
									{H->icons.cloner, "nav.clone"}};
			for (int32 i = 0; i < 4; ++i) {
				const NkRect ir = {r.x + u.s(8), y, r.w - u.s(16), u.s(34)};
				if (NkNavItem(u, ir, main_[i].icon, NkT(main_[i].key), H->nav == i, NkCol::primary)) {
					H->nav = i;
					// i == 0 Accueil, i == 1 Ouvrir, i == 2 Nouveau Workspace (wizard plein cadre)
					if (i == 3) { /* TODO clone git */
					}
				}
				y += u.s(36);
			}
			y += u.s(8);
			u.Rect({r.x + u.s(16), y, r.w - u.s(32), 1.f}, NkCol::border);
			y += u.s(12);
			section(y, NkT("sb.tools"));
			y += u.s(20);
			const NavDef tools[] = {{H->icons.toolchains, "nav.toolchains"},
									{H->icons.platforms, "nav.platforms"},
									{H->icons.gear, "nav.settings"}};
			for (int32 i = 0; i < 3; ++i) {
				const NkRect ir = {r.x + u.s(8), y, r.w - u.s(16), u.s(34)};
				if (NkNavItem(u, ir, tools[i].icon, NkT(tools[i].key), H->nav == 10 + i, NkCol::secondary)) {
					H->nav = 10 + i;
					// i == 0 -> gestionnaire de toolchains (panneau plein cadre nav==10, plus de modale)
				}
				y += u.s(36);
			}

			// ── Footer versions (bas de sidebar) ──
			// Version IDE = constante UNIQUE (NkCodeVersion(), plus de litteral divergent
			// entre ici et la fenetre « A propos »). Version Jenga = celle REELLEMENT
			// detectee sur la machine (`jenga --version`, cf. NkSettingsState::DetectSync),
			// plus un litteral code en dur qui mentait des que l'utilisateur mettait
			// Jenga a jour (affichait 2.0.7 avec un Jenga 2.0.9 installe).
			// Amorce la detection (thread de fond, idempotente) : sans cet appel elle
			// ne demarrait QUE dans le panneau Parametres -> la version Jenga restait
			// a « … » sur l'accueil jusqu'a ce que l'utilisateur y aille.
			H->settings.EnsureDetected();
			const float32 fy = r.y + r.h - u.s(44);
			u.Rect({r.x, fy - u.s(8), r.w, 1.f}, NkCol::border);
			u.Text(r.x + u.s(16), fy, "IDE", NkCol::mutedFg);
			const char *ideV = NkCodeVersion();
			u.Text(r.x + r.w - u.s(16) - u.TextW(ideV), fy, ideV, NkCol::mutedFg);
			u.Text(r.x + u.s(16), fy + u.s(16), "Jenga", NkCol::mutedFg);
			// « … » tant que la detection asynchrone n'a pas repondu ; « n/d » si Jenga
			// est introuvable (aucune version a afficher, on ne l'invente pas).
			const NkString jv = H->settings.jengaVersion.Empty()
									? (H->settings.detected ? NkString("n/d") : NkString("\xE2\x80\xA6"))
									: H->settings.jengaVersion;
			u.Text(r.x + r.w - u.s(16) - u.TextW(jv.CStr()), fy + u.s(16), jv.CStr(), NkCol::accent);
		}

		// Proprietes affichees sur la carte d'un workspace (cf. maquette).
		struct NkWsInfo {
				const char *name = "";
				const char *path = "";
				const char *langVer = "";	  // "C++20"
				const char *configs = "";	  // "Debug, Release"
				const char *platforms = "";	  // "Windows, Linux, Android, Web"
				const char *projects = "";	  // "Renderer, Physics, Audio, UI, Main"
				int32 projCount = 0;		  // nombre total de projets -> "(N)"
				bool projCountExact = true;   // false -> affiche « ~N » (estimation)
				const char *buildConfig = ""; // "Debug"
				const char *modified = "";	  // "il y a 2h"
				int32 build = 0;			  // 0 inconnu,1 ok,2 erreur,3 partiel
				bool pinned = false;
				uint32 icon = 0;
				NkColor iconBg{15, 115, 213, 255};
		};

		// ── Carte d'un workspace : nom/chemin, langage·configs·plateformes, projets,
		//    statut de build + modif, etoile + menu (format maquette) ──
		inline int32 NkWorkspaceCard(const NkUi &u, const NkRect &r, const NkWsInfo &w, uint32 starIcon) {
			const bool hov = u.Hit(r);
			u.Panel(r, hov ? NkCol::hover : NkCol::surface, hov ? NkColor{48, 54, 61, 255} : NkCol::border,
					NkR::lg * u.S);
			const float32 lh = u.Lh();
			// pastille icone (logo projet)
			const float32 ic = u.s(38);
			const NkRect icR = {r.x + u.s(14), r.y + u.s(14), ic, ic};
			u.Rect(icR, w.iconBg, NkR::sm * u.S);
			if (w.icon)
				NkDrawIcon(u, w.icon, {icR.x + u.s(8), icR.y + u.s(8), ic - u.s(16), ic - u.s(16)}, NkCol::foreground);
			const float32 tx = r.x + u.s(62);
			float32 y = r.y + u.s(12);
			const float32 nameRight = r.x + r.w - u.s(66); // 1re ligne (nom) s'arrete avant etoile/...
			const float32 lineRight = r.x + r.w - u.s(16);
			// Separateur " · " centre verticalement sur la ligne courante (avance cx).
			auto dot = [&](float32 &cx) {
				if (cx >= lineRight)
					return;
				u.dl->AddRectFilled({cx + u.s(6), y + lh * 0.5f - u.s(1.5f), u.s(3), u.s(3)}, NkCol::mutedFg,
									u.s(1.5f));
				cx += u.s(16);
			};
			// "Label: " (muted) + valeur (couleur), clippe a droite, avance cx.
			auto field = [&](float32 &cx, const char *label, const char *val, const NkColor &vc) {
				if (cx >= lineRight || !val || !*val)
					return;
				u.Text(cx, y, label, NkCol::mutedFg);
				cx += u.TextW(label);
				cx += u.TextEllipsis(cx, y, lineRight - cx, val, vc);
			};

			// Ligne 1 : NOM
			u.TextEllipsis(tx, y, nameRight - tx, w.name, NkCol::foreground);
			y += lh + u.s(6);
			// Ligne 2 : CHEMIN complet (cliquable -> revele dans l'explorateur)
			bool pathHov = false;
			if (w.path && w.path[0]) {
				const float32 pavail = lineRight - tx;
				float32 pw = u.TextW(w.path);
				if (pw > pavail)
					pw = pavail;
				const NkRect pathRect = {tx, y, pw, lh + u.s(2)};
				pathHov = u.Hit(pathRect);
				u.TextEllipsis(tx, y, pavail, w.path, pathHov ? NkCol::primary : NkCol::mutedFg);
				if (pathHov)
					u.dl->AddRectFilled({tx, y + lh, pw, u.s(1)}, NkCol::primary, 0.f); // souligne
			}
			y += lh + u.s(6);
			// Ligne 3 : Langages: X  ·  Configs: Y
			{
				float32 cx = tx;
				field(cx, "Langages: ", (w.langVer && *w.langVer) ? w.langVer : "C++", NkCol::foreground);
				if (w.configs && *w.configs) {
					dot(cx);
					field(cx, "Configs: ", w.configs, NkCol::foreground);
				}
			}
			y += lh + u.s(5);
			// Ligne 4 : Plateformes: Z
			if (w.platforms && *w.platforms) {
				float32 cx = tx;
				field(cx, "Plateformes: ", w.platforms, NkCol::foreground);
				y += lh + u.s(5);
			}
			// Ligne 5 : Projets: A, B, C (N)
			if (w.projects && *w.projects) {
				float32 cx = tx;
				u.Text(cx, y, NkT("card.projects"), NkCol::mutedFg);
				cx += u.TextW(NkT("card.projects"));
				NkString cnt; // NkPrintf maison (ex-std::snprintf)
				if (w.projCount > 0)
					cnt = w.projCountExact ? NkPrintf("  (%d)", w.projCount) : NkPrintf("  (~%d)", w.projCount);
				const float32 cntw = !cnt.Empty() ? u.TextW(cnt.CStr()) : 0.f;
				cx += u.TextEllipsis(cx, y, lineRight - cx - cntw, w.projects, NkCol::foreground);
				if (!cnt.Empty())
					u.Text(cx, y, cnt.CStr(), NkCol::mutedFg);
				y += lh + u.s(5);
			}
			// Ligne 6 : Dernier build: [statut] Cfg  ·  Modifie: ...
			{
				float32 cx = tx;
				u.Text(cx, y, NkT("card.lastbuild"), NkCol::mutedFg);
				cx += u.TextW(NkT("card.lastbuild"));
				const NkColor stc = w.build == 1   ? NkCol::success
									: w.build == 2 ? NkCol::danger
									: w.build == 3 ? NkCol::accent
												   : NkCol::mutedFg;
				const NkRect sb = {cx, y + u.s(1), u.s(14), u.s(14)};
				u.Rect(sb, w.build ? NkColor{stc.r, stc.g, stc.b, 40} : NkCol::muted, NkR::sm * u.S);
				if (w.build == 2)
					u.Icon("x", {sb.x + u.s(2), sb.y + u.s(2), u.s(10), u.s(10)}, stc);
				else {
					u.dl->AddLine({sb.x + u.s(3), sb.y + u.s(7)}, {sb.x + u.s(6), sb.y + u.s(10)}, stc, u.s(1.6f));
					u.dl->AddLine({sb.x + u.s(6), sb.y + u.s(10)}, {sb.x + u.s(11), sb.y + u.s(4)}, stc, u.s(1.6f));
				}
				cx = sb.x + u.s(20);
				if (w.buildConfig && *w.buildConfig) {
					u.Text(cx, y, w.buildConfig, NkCol::foreground);
					cx += u.TextW(w.buildConfig);
				} else {
					u.Text(cx, y, NkT("card.unknown"), NkCol::mutedFg);
					cx += u.TextW(NkT("card.unknown"));
				}
				if (w.modified && *w.modified) {
					dot(cx);
					field(cx, "Modifie: ", w.modified, NkCol::mutedFg);
				}
			}

			// ── etoile (favori) + menu (...) : BOUTONS a focus PRIORITAIRE (le clic agit
			//    sur le bouton, jamais sur la carte ; survol = fond + icone plus lumineuse) ──
			int32 act = 0; // 1 charger, 2 (des)epingler, 3 menu/..., 4 reveler le chemin
			const NkRect bStar = {r.x + r.w - u.s(60), r.y + u.s(12), u.s(22), u.s(22)};
			const NkRect bDots = {r.x + r.w - u.s(34), r.y + u.s(12), u.s(22), u.s(22)};
			const bool hStar = u.Hit(bStar), hDots = u.Hit(bDots), overBtn = hStar || hDots;
			if (hStar)
				u.Rect(bStar, NkColor{48, 54, 61, 255}, NkR::sm * u.S);
			NkDrawIcon(u, starIcon, {bStar.x + u.s(2), bStar.y + u.s(2), u.s(18), u.s(18)},
					   w.pinned ? NkCol::accent : (hStar ? NkCol::foreground : NkCol::mutedFg));
			if (hDots)
				u.Rect(bDots, NkColor{48, 54, 61, 255}, NkR::sm * u.S);
			const NkColor dotsC = hDots ? NkCol::foreground : NkCol::mutedFg;
			for (int32 i = 0; i < 3; ++i)
				u.dl->AddRectFilled({bDots.x + u.s(5) + i * u.s(5), bDots.y + u.s(10), u.s(2.5f), u.s(2.5f)}, dotsC,
									u.s(1.2f));
			if (hStar && u.click)
				act = 2; // priorite : etoile
			else if (hDots && u.click)
				act = 3; // priorite : menu (...)
			else if (pathHov && u.click)
				act = 4; // priorite : chemin -> reveler
			else if (hov && !overBtn && !pathHov && u.click)
				act = 1; // carte : sinon charger
			return act;
		}

		// ── Action rapide (colonne droite) ──
		inline bool NkQuickAction(const NkUi &u, const NkRect &r, uint32 icon, const char *label, const char *sub,
								  const NkColor &borderCol, const NkColor &iconCol) {
			const bool hov = u.Hit(r);
			u.Panel(r, hov ? NkCol::hover : NkCol::surface, borderCol, NkR::lg * u.S);
			const float32 isz = u.s(18);
			NkDrawIcon(u, icon, {r.x + u.s(12), r.y + (r.h - isz) * 0.5f, isz, isz}, iconCol);
			u.Text(r.x + u.s(42), r.y + u.s(8), label, NkCol::foreground);
			u.Text(r.x + u.s(42), r.y + u.s(8) + u.Lh(), sub, NkCol::mutedFg);
			return hov && u.click;
		}

		// Ouvre un dossier dans l'explorateur de fichiers de l'OS.
		inline void NkHomeOpenFolder(const NkString &folder) {
#ifdef _WIN32
			NkString bs;
			for (const char *p = folder.CStr(); *p; ++p)
				bs += (*p == '/') ? '\\' : *p;
			NkCodeShellRun((NkString("explorer \"") + bs + "\"").CStr());
#elif defined(__APPLE__)
			NkCodeShellRun((NkString("open \"") + folder + "\"").CStr());
#else
			NkCodeShellRun((NkString("xdg-open \"") + folder + "\"").CStr());
#endif
		}

		// Ouvre un terminal dans le dossier.
		inline void NkHomeOpenTerminal(const NkString &folder) {
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

		// Lance une NOUVELLE fenetre NKCode : sur `folder` si fourni, sinon SANS
		// argument -> la fenetre s'ouvre sur le LAUNCHER (ecran de demarrage).
		inline void NkHomeOpenNewWindow(const NkString &exe, const NkString &folder) {
			if (exe.Empty())
				return;
#ifdef _WIN32
			if (folder.Empty())
				NkCodeShellRun((NkString("start \"\" \"") + exe + "\"").CStr());
			else
				NkCodeShellRun((NkString("start \"\" \"") + exe + "\" \"" + folder + "\"").CStr());
#else
			if (folder.Empty())
				NkCodeShellRun((NkString("\"") + exe + "\" &").CStr());
			else
				NkCodeShellRun((NkString("\"") + exe + "\" \"" + folder + "\" &").CStr());
#endif
		}

		// ── Menu contextuel "..." d'une carte : 8 actions + separateurs (cf. spec). Modal. ──
		inline void NkHomeCardMenu(const NkUi &u, NkHomeState *H, NkCodeState *st, NkCodeDialogs *dlg,
								   bool justOpened) {
			if (H->ctxIdx == -2)
				return;

			struct Item {
					const char *label;
					int32 id;
					bool sep;
			};

			const NkString pinLbl = H->ctxPinned ? NkString(NkT("ctx.unpin")) : NkString(NkT("ctx.pin"));
			const Item items[] = {
				{NkT("ctx.open"), 1, false},	 {NkT("ctx.opennew"), 2, false}, {"", 0, true},
				{NkT("ctx.openterm"), 3, false}, {NkT("ctx.reveal"), 4, false},	 {"", 0, true},
				{pinLbl.CStr(), 5, false},		 {NkT("ctx.rename"), 6, false},	 {"", 0, true},
				{NkT("ctx.copypath"), 7, false}, {NkT("ctx.remove"), 8, false},
			};
			const int32 N = (int32)(sizeof(items) / sizeof(items[0]));
			const float32 ih = u.s(26), sh = u.s(9), w = u.s(248);
			float32 h = u.s(8);
			for (int32 i = 0; i < N; ++i)
				h += items[i].sep ? sh : ih;
			NkRect box = {H->ctxPos.x, H->ctxPos.y, w, h};
			if (box.x + box.w > (float32)u.ctx->viewW)
				box.x = (float32)u.ctx->viewW - box.w - u.s(8);
			if (box.y + box.h > (float32)u.ctx->viewH)
				box.y = (float32)u.ctx->viewH - box.h - u.s(8);
			if (box.x < u.s(4))
				box.x = u.s(4);
			if (box.y < u.s(4))
				box.y = u.s(4);
			u.dl->AddRectFilled({box.x + u.s(2), box.y + u.s(3), box.w, box.h}, NkColor{0, 0, 0, 90},
								NkR::md * u.S); // ombre
			u.Panel(box, NkCol::surface, NkColor{48, 54, 61, 255}, NkR::md * u.S);

			int32 clicked = 0;
			float32 iy = box.y + u.s(4);
			for (int32 i = 0; i < N; ++i) {
				if (items[i].sep) {
					u.dl->AddRectFilled({box.x + u.s(8), iy + sh * 0.5f, w - u.s(16), 1.f}, NkCol::border, 0.f);
					iy += sh;
					continue;
				}
				const NkRect ir = {box.x + u.s(4), iy, w - u.s(8), ih};
				const bool hv = u.Hit(ir);
				if (hv)
					u.Rect(ir, NkCol::hover, NkR::sm * u.S);
				u.TextV(ir.x + u.s(12), ir.y, ih, items[i].label, items[i].id == 8 ? NkCol::danger : NkCol::foreground);
				if (hv && u.click)
					clicked = items[i].id;
				iy += ih;
			}
			if (justOpened)
				return; // ignore le clic d'ouverture
			const bool outside = u.click && !u.Hit(box);
			if (!clicked && !outside)
				return;
			const NkString path = H->ctxPath;
			const bool isCur = H->ctxIsCurrent;
			// Dossier de l'entree : le chemin LUI-MEME si c'est un dossier (ouvert sans
			// workspace), son parent si c'est un fichier « .jenga ». Voir RecentFolder.
			const NkString folder = NkCodeState::RecentFolder(path.CStr()).ToString();
			u.ctx->input.mouseClicked[0] = false; // consomme le clic (menu prioritaire)
			H->ctxIdx = -2;						  // ferme le menu
			switch (clicked) {
				case 1:
					dlg->DoLoad(NkCodeState::RecentFolder(path.CStr()));
					break; // ecran de chargement (courant OU recent)
				case 2:
					NkHomeOpenNewWindow(H->exePath, folder);
					break;
				case 3:
					NkHomeOpenTerminal(folder);
					break;
				case 4:
					NkHomeOpenFolder(folder);
					break;
				case 5:
					if (st->IsPinned(path.CStr()))
						st->UnpinRecent(path);
					else
						st->PinRecent(path);
					break;
				case 6: { // ouvre le popup de renommage (prefill = nom actuel)
					H->renameOpen = true;
					H->renamePath = path;
					H->renamePos = H->ctxPos;
					const char *cur = st->NameOverride(path.CStr());
					const NkString nm = cur ? NkString(cur) : NkCodeState::WorkspaceNameOf(path.CStr());
					int32 k = 0;
					for (; nm.CStr()[k] && k + 1 < (int32)sizeof(H->renameBuf); ++k)
						H->renameBuf[k] = nm.CStr()[k];
					H->renameBuf[k] = '\0';
					break;
				}
				case 7:
					u.ctx->SetClipboard(folder.CStr());
					break;
				case 8:
					st->RemoveRecent(path);
					break;
			}
		}

		// ── Popup de renommage dans les recents (champ texte + OK/Annuler). Modal. ──
		inline void NkHomeRenamePopup(const NkUi &u, NkHomeState *H, NkCodeState *st, bool justOpened) {
			if (!H->renameOpen)
				return;
			const float32 w = u.s(290), h = u.s(104);
			NkRect box = {H->renamePos.x, H->renamePos.y, w, h};
			if (box.x + box.w > (float32)u.ctx->viewW)
				box.x = (float32)u.ctx->viewW - box.w - u.s(8);
			if (box.y + box.h > (float32)u.ctx->viewH)
				box.y = (float32)u.ctx->viewH - box.h - u.s(8);
			if (box.x < u.s(4))
				box.x = u.s(4);
			if (box.y < u.s(4))
				box.y = u.s(4);
			u.dl->AddRectFilled({box.x + u.s(2), box.y + u.s(3), box.w, box.h}, NkColor{0, 0, 0, 110}, NkR::md * u.S);
			u.Panel(box, NkCol::surface, NkColor{48, 54, 61, 255}, NkR::md * u.S);
			u.Text(box.x + u.s(14), box.y + u.s(10), NkT("ctx.rename"), NkCol::foreground);
			const NkRect field = {box.x + u.s(14), box.y + u.s(34), w - u.s(28), u.s(28)};
			u.Panel(field, NkCol::input, NkCol::primary, NkR::md * u.S);
			const float32 fty = field.y + (field.h - u.Lh()) * 0.5f;
			if (H->renameBuf[0] == '\0')
				u.Text(field.x + u.s(8), fty, NkT("rename.placeholder"), NkCol::mutedFg); // placeholder
			if (!justOpened)
				NkOwEditA(u, field, H->renameBuf, (int32)sizeof(H->renameBuf), u.ctx->input.dt,
						  u.s(8)); // saisie complete
			else if (H->renameBuf[0])
				u.Text(field.x + u.s(8), fty, H->renameBuf, NkCol::foreground);
			const NkRect okR = {box.x + w - u.s(160), box.y + h - u.s(32), u.s(72), u.s(24)};
			const NkRect caR = {box.x + w - u.s(82), box.y + h - u.s(32), u.s(72), u.s(24)};
			const bool okClick = u.Button(okR, "OK", NkCol::primary, NkCol::primary, NkCol::primaryFg, NkR::md * u.S);
			const bool caClick =
				u.Button(caR, NkT("btn.cancel"), NkCol::input, NkCol::hover, NkCol::foreground, NkR::md * u.S);
			if (justOpened)
				return;
			if (okClick) {
				st->SetRecentName(H->renamePath, NkString(H->renameBuf));
				H->renameOpen = false;
				u.ctx->input.mouseClicked[0] = false;
			} else if (caClick) {
				H->renameOpen = false;
				u.ctx->input.mouseClicked[0] = false;
			} else if (u.click && !u.Hit(box)) {
				H->renameOpen = false;
				u.ctx->input.mouseClicked[0] = false;
			}
		}

		// ── Combo box d'un chip de filtre : liste deroulante des options. Modal (consomme le clic). ──
		inline void NkHomeCombo(const NkUi &u, NkHomeState *H, bool justOpened) {
			if (H->comboOpen == 0)
				return;
			const char *const *opts;
			int32 nopts;
			int32 *target;
			if (H->comboOpen == 1) {
				opts = NkLangFilters;
				nopts = (int32)(sizeof(NkLangFilters) / sizeof(NkLangFilters[0]));
				target = &H->langFilter;
			} else {
				opts = NkSysFilters;
				nopts = (int32)(sizeof(NkSysFilters) / sizeof(NkSysFilters[0]));
				target = &H->sysFilter;
			}
			const float32 ih = u.s(28), w = (H->comboW > u.s(80)) ? H->comboW : u.s(120), h = nopts * ih + u.s(8);
			NkRect box = {H->comboPos.x, H->comboPos.y, w, h};
			if (box.y + box.h > (float32)u.ctx->viewH)
				box.y = (float32)u.ctx->viewH - box.h - u.s(8);
			if (box.x + box.w > (float32)u.ctx->viewW)
				box.x = (float32)u.ctx->viewW - box.w - u.s(8);
			u.dl->AddRectFilled({box.x + u.s(2), box.y + u.s(3), box.w, box.h}, NkColor{0, 0, 0, 90},
								NkR::md * u.S); // ombre
			u.Panel(box, NkCol::surface, NkColor{48, 54, 61, 255}, NkR::md * u.S);
			int32 picked = -1;
			for (int32 i = 0; i < nopts; ++i) {
				const NkRect ir = {box.x + u.s(4), box.y + u.s(4) + i * ih, w - u.s(8), ih};
				const bool hv = u.Hit(ir);
				if (hv)
					u.Rect(ir, NkCol::hover, NkR::sm * u.S);
				if (i == *target)
					u.dl->AddRectFilled({ir.x + u.s(2), ir.y + u.s(6), u.s(3), ih - u.s(12)}, NkCol::primary,
										u.s(1.5f));
				u.TextV(ir.x + u.s(12), ir.y, ih, NkFilterLabel(opts, i),
						(i == *target) ? NkCol::foreground : NkCol::sidebarFg);
				if (hv && u.click)
					picked = i;
			}
			if (justOpened)
				return; // ignore le clic d'ouverture
			const bool outside = u.click && !u.Hit(box);
			if (picked >= 0 || outside) {
				u.ctx->input.mouseClicked[0] = false; // consomme (priorite combo)
				if (picked >= 0)
					*target = picked;
				H->comboOpen = 0;
			}
		}

		// ── Champ de recherche : panneau + loupe + saisie (backspace/frappe) + placeholder.
		//    Renvoie true si clique (pour poser le focus). `focused` -> capte le clavier. ──
		inline bool NkHomeSearch(const NkUi &u, const NkRect &r, char *buf, int32 cap, bool focused, uint32 icon,
								 const char *placeholder, bool caretOn = true) {
			u.Panel(r, NkCol::input, focused ? NkCol::primary : NkCol::border, NkR::md * u.S);
			const float32 isz = u.s(14);
			if (icon)
				NkDrawIcon(u, icon, {r.x + u.s(9), r.y + (r.h - isz) * 0.5f, isz, isz}, NkCol::mutedFg);
			else
				u.Icon("search", {r.x + u.s(9), r.y + (r.h - u.s(13)) * 0.5f, u.s(13), u.s(13)}, NkCol::mutedFg);
			(void)caretOn;
			const float32 tx = r.x + u.s(28), ty = r.y + (r.h - u.Lh()) * 0.5f;
			const float32 availW = r.x + r.w - u.s(10) - tx;
			// Saisie complete (selection, copier/couper/coller, double-clic) via l'editeur unifie.
			if (focused)
				NkOwEditA(u, r, buf, cap, u.ctx->input.dt, u.s(28));
			else if (buf[0])
				u.TextEllipsis(tx, ty, availW, buf, NkCol::foreground);
			else
				u.Text(tx, ty, placeholder, NkCol::mutedFg);
			return u.Hit(r) && u.click;
		}

		// ── Panneau principal du Home ──
		inline void NkHomePanel(const NkUi &u, const NkRect &r, NkHomeState *H) {
			NkCodeState *st = H->st;
			NkCodeDialogs *dlg = H->dlg;
			u.Rect(r, NkCol::background);
			const float32 pad = u.s(24);
			const float32 rightW = u.s(272), gap = u.s(24); // colonne droite un peu plus large (actions rapides)
			const NkRect left = {r.x + pad, r.y + u.s(18), r.w - pad * 2.f - rightW - gap, r.h - u.s(36)};
			const NkRect right = {left.x + left.w + gap, r.y + u.s(18), rightW, r.h - u.s(36)};

			// Etat des popups (menu "..." ou combo de filtre) -> gele les clics des autres widgets.
			const bool menuOpen = (H->ctxIdx != -2);
			const bool comboWasOpen = (H->comboOpen != 0);
			const bool renameWasOpen = H->renameOpen;
			const bool anyPopup = menuOpen || comboWasOpen || renameWasOpen || NkTxtMenu().open;
			const bool caretOn = (H->caretBlink - (float32)(int32)H->caretBlink) < 0.55f; // clignotement ~1s
			H->fieldClaim = false; // reinitialise : un champ de recherche posera true s'il capte le clic

			// ── Raccourcis clavier du launcher ───────────────────────────────
			//
			// Ils etaient ANNONCES dans l'ecran Parametres (table ScKey) mais
			// n'existaient NULLE PART : ce fichier ne gerait qu'Echap, et les
			// codes de touche Num3..Num6 / Comma n'etaient meme pas acheminés
			// jusqu'a NKGui. Un utilisateur lisait « Ctrl+N : nouveau
			// workspace » et n'obtenait rien — signale en beta (issue #12).
			//
			// Valeurs de `nav` : 0 Accueil, 1 Ouvrir, 2 Nouveau, 3 Cloner, puis
			// 10 Toolchains, 11 Plateformes, 12 Parametres (elles ne se suivent
			// PAS : les outils commencent a 10).
			//
			// Geles pendant qu'un popup est ouvert, comme les clics : sinon
			// Ctrl+1 changerait de page sous un menu encore affiche.
			if (!anyPopup) {
				auto &kin = u.ctx->input;
				auto kp = [&](NkGuiKey k) { return kin.ctrlDown && kin.KeyPressed(k); };
				if (kp(NkGuiKey::Num1))
					H->nav = 0; // Accueil
				else if (kp(NkGuiKey::Num2))
					H->nav = 1; // Ouvrir
				else if (kp(NkGuiKey::Num3))
					H->nav = 2; // Nouveau workspace
				else if (kp(NkGuiKey::Num4))
					H->nav = 3; // Cloner
				else if (kp(NkGuiKey::Num5))
					H->nav = 10; // Toolchains
				else if (kp(NkGuiKey::Num6))
					H->nav = 12; // Parametres
				else if (kp(NkGuiKey::N))
					H->nav = 2; // Ctrl+N : nouveau workspace
				else if (kp(NkGuiKey::O))
					H->nav = 1; // Ctrl+O : ouvrir un workspace
				else if (kp(NkGuiKey::G))
					H->nav = 3; // Ctrl+G : cloner un depot
				else if (kp(NkGuiKey::Comma))
					H->nav = 12; // Ctrl+, : parametres
			}

			// ── Barre de filtres : recherche + combo langage + combo systeme (fonctionnels) ──
			const float32 fh = u.s(30);
			const float32 cw = u.s(110);
			const NkRect searchR = {left.x, left.y, left.w - cw * 2.f - u.s(16), fh};
			{
				const bool clk = NkHomeSearch(u, searchR, H->searchText, (int32)sizeof(H->searchText),
											  H->focusField == 1, H->icons.search, NkT("home.search.recents"), caretOn);
				if (!anyPopup && clk) {
					H->focusField = 1;
					H->fieldClaim = true;
				}
			}
			// Chip = bouton ouvrant un COMBO BOX (liste deroulante des options).
			auto chip = [&](float32 x, int32 which, int32 idx, const char *const *opts) {
				const NkRect c = {x, left.y, cw, fh};
				const bool active = (idx != 0);
				u.Panel(c, NkCol::input, (active || H->comboOpen == which) ? NkCol::primary : NkCol::border,
						NkR::md * u.S);
				u.TextV(c.x + u.s(10), c.y, fh, NkFilterLabel(opts, idx), active ? NkCol::foreground : NkCol::mutedFg);
				u.Icon("chevron-down", {c.x + cw - u.s(18), c.y + (fh - u.s(12)) * 0.5f, u.s(12), u.s(12)},
					   NkCol::mutedFg);
				if (!anyPopup && u.Hit(c) && u.click) {
					H->comboOpen = which;
					H->comboPos = {c.x, c.y + fh + u.s(3)};
					H->comboW = cw;
					H->focusField = 0;
				}
			};
			chip(left.x + left.w - cw * 2.f - u.s(8), 1, H->langFilter, NkLangFilters);
			chip(left.x + left.w - cw, 2, H->sysFilter, NkSysFilters);

			// Filtre commun (recherche nom/chemin + langage + systeme).
			auto passFilter = [&](const char *name, const char *path, const char *lang, const char *plats) -> bool {
				if (H->searchText[0] && !NkCodeState::ContainsI(name, H->searchText) &&
					!NkCodeState::ContainsI(path, H->searchText))
					return false;
				if (H->langFilter != 0 && !NkCodeState::ContainsI(lang, NkLangFilters[H->langFilter]))
					return false;
				if (H->sysFilter != 0 && !NkCodeState::ContainsI(plats, NkSysFilters[H->sysFilter]))
					return false;
				return true;
			};

			// ── Liste des recents (defilable) ──
			const NkRect listArea = {left.x, left.y + fh + u.s(14), left.w, left.h - fh - u.s(14)};
			if (u.Hit(listArea) && u.ctx->input.wheel != 0.f) {
				H->scroll -= u.ctx->input.wheel * u.s(34);
				u.ctx->input.wheel = 0.f;
				if (H->scroll < 0.f)
					H->scroll = 0.f;
				if (H->scroll > H->scrollMax)
					H->scroll = H->scrollMax; // borne AVANT le dessin (anti-clignotement)
			}
			u.dl->PushClipRect(listArea, true);
			float32 y = listArea.y - H->scroll;
			int32 doLoad = -1, doPin = -1, doRem = -1;
			bool loadCur = false;
			NkString revealPath; // chemin a reveler dans l'explorateur (clic sur le chemin d'une carte)
			NkString curWsPath;	 // chemin .jenga du workspace courant (pour favori/... sur sa carte)
			if (st && st->HasWorkspace() && st->wsIdx >= 0 && st->wsIdx < (int32)st->wsPaths.Size())
				curWsPath = st->wsPaths[st->wsIdx];

			// En-tete de groupe PLIABLE (clic sur la ligne -> replie/deplie). Renvoie l'etat ouvert.
			auto groupHeader = [&](int32 gid, const char *label, const NkColor &col) -> bool {
				const NkRect hr = {listArea.x, y - u.s(2), listArea.w, u.s(20)};
				if (!anyPopup && u.Hit(hr) && u.click)
					H->groupCollapsed[gid] = !H->groupCollapsed[gid];
				const bool open = !H->groupCollapsed[gid];
				u.Icon(open ? "chevron-down" : "chevron-right", {listArea.x, y + u.s(1), u.s(12), u.s(12)},
					   NkCol::mutedFg);
				u.Text(listArea.x + u.s(18), y, label, col);
				y += u.s(20);
				return open;
			};
			// Ouvre le menu contextuel d'une carte, positionne sous son bouton "...".
			auto openMenu = [&](int32 index, const NkRect &cr, const NkString &path, bool pinned, bool isCur) {
				H->ctxIdx = index;
				H->ctxPath = path;
				H->ctxPinned = pinned;
				H->ctxIsCurrent = isCur;
				H->ctxPos = {cr.x + cr.w - u.s(190), cr.y + u.s(36)};
			};
			const NkColor cols[] = {NkCol::primary, NkCol::accent, NkCol::secondary, NkColor{51, 177, 160, 255}};

			const float32 cardLh = u.Lh();								   // 6 lignes libellees -> carte plus haute
			const float32 CH = u.s(24) + 5.f * (cardLh + u.s(5)) + cardLh; // nom+chemin+langs+plats+projets+build
			const float32 CSTEP = CH + u.s(12);
			const int64 now = NkCodeState::NowEpoch();
			NkString curName, curPath, curProj; // gardes en vie pour la carte du workspace courant

			// Carte d'un workspace epingle (factorisee : utilisee pour le pre-check et le dessin).
			auto pinnedPasses = [&](usize i) -> bool {
				if (StrEq(st->pinned[i].CStr(), curWsPath.CStr()))
					return false; // doublon avec le courant
				const auto meta = st->WorkspaceMeta(st->pinned[i].CStr());
				const char *nm = (i < st->pinnedNames.Size() && !st->pinnedNames[i].Empty()) ? st->pinnedNames[i].CStr()
																							 : st->pinned[i].CStr();
				return passFilter(nm, st->pinned[i].CStr(), meta.langVer.CStr(), meta.platforms.CStr());
			};

			// ── ÉPINGLÉS (favoris, pliable) — SAUF le workspace courant (montre dans son bucket) ──
			if (st && !st->pinned.Empty()) {
				bool hasAny = false;
				for (usize i = 0; i < st->pinned.Size() && !hasAny; ++i)
					hasAny = pinnedPasses(i);
				if (hasAny) {
					const bool open = groupHeader(0, NkT("home.pinned"), NkCol::accent);
					for (usize i = 0; open && i < st->pinned.Size(); ++i) {
						if (!pinnedPasses(i))
							continue;
						const auto meta = st->WorkspaceMeta(st->pinned[i].CStr());
						const char *nm = (i < st->pinnedNames.Size() && !st->pinnedNames[i].Empty())
											 ? st->pinnedNames[i].CStr()
											 : st->pinned[i].CStr();
						const NkString mod = NkCodeState::HumanAge(meta.activity, now);
						NkWsInfo wi;
						wi.name = nm;
						wi.path = st->pinned[i].CStr();
						wi.pinned = true;
						wi.iconBg = cols[i % 4];
						wi.icon = H->icons.workspace;
						wi.modified = mod.CStr();
						wi.langVer = meta.langVer.CStr();
						wi.configs = meta.configs.CStr();
						wi.platforms = meta.platforms.CStr();
						wi.projects = meta.projects.CStr();
						wi.projCount = meta.projCount;
						wi.projCountExact = meta.projCountExact;
						const NkRect cr = {listArea.x, y, listArea.w, CH};
						const int32 a = NkWorkspaceCard(u, cr, wi, H->icons.star);
						if (!anyPopup) {
							if (a == 1)
								doLoad = (int32)(1000 + i);
							else if (a == 2)
								doPin = (int32)(1000 + i);
							else if (a == 3)
								openMenu((int32)(1000 + i), cr, st->pinned[i], true, false);
							else if (a == 4)
								revealPath = NkCodeState::RecentFolder(st->pinned[i].CStr()).ToString(); // reveler
						}
						y += CSTEP;
					}
				}
			}

			// ── RÉCENTS groupes PAR DATE (AUJOURD'HUI / CETTE SEMAINE / PLUS ANCIEN).
			//    Le workspace COURANT est fondu dans ces groupes (idx -1), trie par mtime. ──
			{
				struct Row {
						int32 idx;
						int64 mtime;
						NkString gkey;
				}; // idx -1 = courant ; gkey = libelle de groupe

				NkVector<Row> rows;
				// groupBy : 0 Date (buckets d'age), 1 Plateforme (1re plateforme), 2 Langage.
				const int32 gmode = H->settings.groupBy;
				auto firstTok = [](const char *s) -> NkString {
					NkString t;
					for (const char *c = s; *c && *c != ',' && *c != ';'; ++c) {
						if (*c == ' ' && t.Empty())
							continue;
						t += *c;
					}
					return t;
				};
				auto gkeyOf = [&](int64 mt, const char *platforms, const char *langVer) -> NkString {
					if (gmode == 1) {
						const NkString p = firstTok(platforms ? platforms : "");
						return p.Empty() ? NkString(NkT("home.nogroup")) : p;
					}
					if (gmode == 2) {
						return (langVer && *langVer) ? NkString(langVer) : NkString(NkT("home.nogroup"));
					}
					return NkString(NkHomeBucketLabel(NkCodeState::AgeBucket(mt, now)));
				};
				if (!curWsPath.Empty()) { // le courant : montre (riche) dans son groupe, si filtre OK
					const NkString cn = st->root.GetFileName(), cp = st->root.ToString();
					if (passFilter(cn.CStr(), cp.CStr(), "C++20", st->infoOSes.CStr())) {
						const int64 mt = st->WorkspaceMeta(curWsPath.CStr()).activity;
						rows.PushBack({-1, mt, gkeyOf(mt, st->infoOSes.CStr(), "C++20")});
					}
				}
				for (usize i = 0; st && i < st->recents.Size(); ++i) {
					if (!curWsPath.Empty() && StrEq(curWsPath.CStr(), st->recents[i].CStr()))
						continue; // pas de doublon avec le courant
					const char *rp = st->recents[i].CStr();
					if (!NkDirectory::Exists(rp) && !NkFile::Exists(rp))
						continue; // workspace disparu du disque -> ne pas l'afficher
					const auto meta = st->WorkspaceMeta(st->recents[i].CStr());
					const char *nm = (i < st->recentNames.Size() && !st->recentNames[i].Empty())
										 ? st->recentNames[i].CStr()
										 : st->recents[i].CStr();
					if (!passFilter(nm, st->recents[i].CStr(), meta.langVer.CStr(), meta.platforms.CStr()))
						continue; // filtres actifs
					rows.PushBack(
						{(int32)i, meta.activity, gkeyOf(meta.activity, meta.platforms.CStr(), meta.langVer.CStr())});
				}
				auto keyLess = [](const NkString &A, const NkString &B) -> bool {
					const char *a = A.CStr();
					const char *b = B.CStr();
					while (*a && *a == *b) {
						++a;
						++b;
					}
					return (unsigned char)*a < (unsigned char)*b;
				};
				for (usize a = 0; a < rows.Size(); ++a) // tri : Date = mtime desc ; sinon groupe (asc) puis mtime desc
					for (usize b = a + 1; b < rows.Size(); ++b) {
						bool swap;
						if (gmode == 0)
							swap = rows[b].mtime > rows[a].mtime;
						else {
							if (keyLess(rows[b].gkey, rows[a].gkey))
								swap = true;
							else if (keyLess(rows[a].gkey, rows[b].gkey))
								swap = false;
							else
								swap = rows[b].mtime > rows[a].mtime;
						}
						if (swap) {
							Row t = rows[a];
							rows[a] = rows[b];
							rows[b] = t;
						}
					}
				// recentsMax : plafonne le nombre de workspaces affiches (0/vide = illimite).
				{
					const int32 rmax = NkAtoi(H->settings.recentsMax); // conversion maison (NkText.h)
					if (rmax > 0)
						while ((int32)rows.Size() > rmax)
							rows.Erase(rows.Begin() + (rows.Size() - 1));
				}

				if (rows.Empty()) {
					u.Icon("chevron-down", {listArea.x, y + u.s(1), u.s(12), u.s(12)}, NkCol::mutedFg);
					u.Text(listArea.x + u.s(18), y, NkT("home.recents"), NkCol::mutedFg);
					y += u.s(20);
					u.Text(listArea.x + u.s(2), y, NkT("home.norecent"), NkCol::mutedFg);
					y += u.s(24);
				}
				NkString lastKey;
				bool haveKey = false;
				int32 gcount = 0;
				bool bucketOpen = true;
				for (usize r = 0; r < rows.Size(); ++r) {
					if (!haveKey || !StrEq(rows[r].gkey.CStr(), lastKey.CStr())) { // nouvel en-tete de groupe (pliable)
						lastKey = rows[r].gkey;
						haveKey = true;
						int32 gid = 1 + gcount;
						if (gid > 23)
							gid = 23;
						++gcount;
						bucketOpen = groupHeader(gid, lastKey.CStr(), NkCol::mutedFg);
					}
					if (!bucketOpen)
						continue; // groupe plie -> cartes masquees
					const int32 idx = rows[r].idx;
					const NkString mod = NkCodeState::HumanAge(rows[r].mtime, now);
					NkWsInfo wi;
					wi.modified = mod.CStr();
					const NkRect cr = {listArea.x, y, listArea.w, CH};
					if (idx < 0) { // workspace COURANT (meta complete)
						curName = st->root.GetFileName();
						curPath = st->root.ToString();
						curProj.Clear();
						for (usize p = 0; p < st->projects.Size() && p < 6; ++p) {
							if (!curProj.Empty())
								curProj += ", ";
							curProj += st->projects[p];
						}
						wi.name = curName.CStr();
						wi.path = curPath.CStr();
						wi.langVer = "C++20";
						wi.configs = st->infoConfigs.CStr();
						wi.platforms = st->infoOSes.CStr();
						wi.projects = curProj.CStr();
						wi.projCount = st->TotalProjectCount(); // meme definition que l'ecran de chargement
						wi.buildConfig = st->ConfigName();
						wi.build = 1;
						wi.iconBg = NkCol::primary;
						wi.icon = H->icons.workspace;
						wi.pinned = st->IsPinned(curWsPath.CStr()); // etoile reflete l'etat epingle
						const int32 a = NkWorkspaceCard(u, cr, wi, H->icons.star);
						if (!anyPopup) {
							if (a == 1)
								loadCur = true;
							else if (a == 2)
								doPin = 2000; // 2000 = courant
							else if (a == 3)
								openMenu(2000, cr, curWsPath, wi.pinned, true);
							else if (a == 4)
								revealPath = st->root.ToString();
						}
					} else { // recent : meta parsee du .jenga
						const auto meta = st->WorkspaceMeta(st->recents[idx].CStr());
						wi.name = ((usize)idx < st->recentNames.Size() && !st->recentNames[idx].Empty())
									  ? st->recentNames[idx].CStr()
									  : st->recents[idx].CStr();
						wi.path = st->recents[idx].CStr();
						wi.iconBg = cols[idx % 4];
						wi.icon = H->icons.workspace;
						wi.langVer = meta.langVer.CStr();
						wi.configs = meta.configs.CStr();
						wi.platforms = meta.platforms.CStr();
						wi.projects = meta.projects.CStr();
						wi.projCount = meta.projCount;
						wi.projCountExact = meta.projCountExact;
						const int32 a = NkWorkspaceCard(u, cr, wi, H->icons.star);
						if (!anyPopup) {
							if (a == 1)
								doLoad = idx;
							else if (a == 2)
								doPin = idx;
							else if (a == 3)
								openMenu(idx, cr, st->recents[idx], false, false);
							else if (a == 4)
								revealPath = NkCodeState::RecentFolder(st->recents[idx].CStr()).ToString();
						}
					}
					y += CSTEP;
				}
			}
			const float32 contentH = (y + H->scroll) - listArea.y;
			H->scrollMax =
				contentH > listArea.h ? contentH - listArea.h : 0.f; // memorise pour borner la frame suivante
			u.dl->PopClipRect();
			NkVScroll(u, listArea, contentH, H->scroll, 1, H); // scrollbar verticale du centre

			// applique les actions differees (apres le dessin)
			if (!revealPath.Empty())
				NkHomeOpenFolder(revealPath); // chemin clique -> revele dans l'explorateur
			if (loadCur) {
				if (dlg && !curWsPath.Empty())
					dlg->DoLoad(NkCodeState::RecentFolder(curWsPath.CStr()));
				return;
			} // ecran de chargement
			auto pathOf = [&](int32 idx) -> NkString {
				return idx >= 2000 ? curWsPath : idx >= 1000 ? st->pinned[idx - 1000] : st->recents[idx];
			};
			if (doLoad >= 0) {
				dlg->DoLoad(NkCodeState::RecentFolder(pathOf(doLoad).CStr()));
				return;
			}
			if (doPin >= 0) { // (des)epingle : courant (2000) ou epingle (>=1000) -> retire ; recent -> epingle
				const NkString p = pathOf(doPin);
				if (doPin >= 2000) {
					if (st->IsPinned(p.CStr()))
						st->UnpinRecent(p);
					else
						st->PinRecent(p);
				} else if (doPin >= 1000)
					st->UnpinRecent(p);
				else
					st->PinRecent(p);
				return;
			}
			if (doRem >= 0) {
				st->RemoveRecent(pathOf(doRem));
				return;
			}

			// ── Colonne droite : HAUT FIXE (actions rapides + en-tete/recherche exemples)
			//    puis BAS DEFILABLE = UNIQUEMENT la liste des exemples. ──
			const float32 rcw = right.w; // zone fixe : pas de scrollbar
			float32 ry = right.y + u.s(2);
			u.Text(right.x, ry, NkT("home.actions"), NkCol::mutedFg);
			ry += u.s(22);
			const float32 qh = u.s(52);
			{
				const bool c = NkQuickAction(u, {right.x, ry, rcw, qh}, H->icons.nouveau, NkT("nav.newws"),
											 NkT("qa.newws.sub"), NkCol::primary, NkCol::primary);
				if (!anyPopup && c)
					H->nav = 2;
			}
			ry += qh + u.s(10);
			{
				const bool c = NkQuickAction(u, {right.x, ry, rcw, qh}, H->icons.ouvrir, NkT("qa.openws"),
											 NkT("qa.openws.sub"), NkCol::border, NkCol::mutedFg);
				if (!anyPopup && c)
					H->nav = 1;
			}
			ry += qh + u.s(10);
			{
				const bool c = NkQuickAction(u, {right.x, ry, rcw, qh}, H->icons.ouvrirDossier, NkT("qa.openfolder"),
											 NkT("qa.openfolder.sub"), NkCol::border, NkCol::mutedFg);
				if (!anyPopup && c)
					H->nav = 1;
			}
			ry += qh + u.s(10);
			{
				const bool c = NkQuickAction(u, {right.x, ry, rcw, qh}, H->icons.cloner, NkT("qa.clone"),
											 NkT("qa.clone.sub"), NkCol::secondary, NkCol::secondaryFg);
				if (!anyPopup && c)
					H->nav = 3;
			}
			ry += qh + u.s(18);

			u.Rect({right.x, ry - u.s(8), rcw, 1.f}, NkCol::border);
			// En-tete + compteur (exemples enumeres via `jenga examples list`).
			{
				const NkString hdr = NkPrintf("%s (%d)", NkT("home.examples"), st ? (int)st->examples.Size() : 0);
				u.Text(right.x, ry, hdr.CStr(), NkCol::mutedFg);
			}
			ry += u.s(20);
			// Recherche d'exemple (champ fixe, hors scroll).
			const NkRect exSearchR = {right.x, ry, rcw, u.s(28)};
			{
				const bool clk = NkHomeSearch(u, exSearchR, H->exSearch, (int32)sizeof(H->exSearch), H->focusField == 2,
											  H->icons.search, NkT("home.search.examples"), caretOn);
				if (!anyPopup && clk) {
					H->focusField = 2;
					H->fieldClaim = true;
				}
			}
			ry += u.s(28) + u.s(10);

			// ── Zone DEFILABLE : la LISTE des exemples uniquement ──
			const NkRect exList = {right.x, ry, right.w, right.y + right.h - ry};
			if (!anyPopup && u.Hit(exList) && u.ctx->input.wheel != 0.f) {
				H->scrollR -= u.ctx->input.wheel * u.s(34);
				u.ctx->input.wheel = 0.f;
				if (H->scrollR < 0.f)
					H->scrollR = 0.f;
				if (H->scrollR > H->scrollRMax)
					H->scrollR = H->scrollRMax;
			}
			u.dl->PushClipRect(exList, true);
			const float32 elw = exList.w - u.s(14); // place pour la scrollbar
			float32 ey = exList.y - H->scrollR;
			int32 shown = 0;
			for (usize i = 0; st && i < st->examples.Size(); ++i) {
				const auto &e = st->examples[i];
				if (H->exSearch[0] && !NkCodeState::ContainsI(e.id.CStr(), H->exSearch) &&
					!NkCodeState::ContainsI(e.desc.CStr(), H->exSearch) &&
					!NkCodeState::ContainsI(e.platforms.CStr(), H->exSearch))
					continue;
				const NkRect er = {exList.x, ey, elw, u.s(40)};
				const bool exHov = !anyPopup && u.Hit(er);
				if (exHov)
					u.Rect(er, NkCol::hover, NkR::sm * u.S);
				// CLIC = cloner l'exemple : le picker maison demande l'EMPLACEMENT de
				// destination, puis `jenga examples copy` (async) cree le clone et le
				// clone s'ouvre comme workspace (RoutePickerResult/PollExampleCopy).
				if (exHov && u.click && H->dlg && !H->dlg->exCopyBusy && !H->dlg->pickerOpen) {
					H->dlg->exCopyId = e.id;
					const char *homeDir = env::GetEnvVar("USERPROFILE");
					if (!homeDir || !*homeDir)
						homeDir = env::GetEnvVar("HOME");
					H->dlg->OpenPicker(NkCodeDialogs::PK_ExampleCopy, (homeDir && *homeDir) ? homeDir : ".");
				}
				NkDrawIcon(u, H->icons.exemple, {exList.x + u.s(8), ey + u.s(7), u.s(14), u.s(14)}, NkCol::accent);
				u.TextEllipsis(exList.x + u.s(28), ey + u.s(4), elw - u.s(34), e.id.CStr(), NkCol::foreground);
				if (e.platforms.CStr()[0])
					u.TextEllipsis(exList.x + u.s(28), ey + u.s(4) + u.Lh(), elw - u.s(34), e.platforms.CStr(),
								   NkCol::mutedFg);
				ey += u.s(42);
				++shown;
			}
			if (st && st->examples.Empty()) {
				u.Text(exList.x, ey, NkT("home.ex.loading"), NkCol::mutedFg);
				ey += u.s(20);
			} else if (shown == 0) {
				u.Text(exList.x, ey, NkT("home.ex.none"), NkCol::mutedFg);
				ey += u.s(20);
			}

			const float32 elContentH = (ey + H->scrollR) - exList.y;
			H->scrollRMax =
				elContentH > exList.h ? elContentH - exList.h : 0.f; // memorise pour borner la frame suivante
			u.dl->PopClipRect();
			NkVScroll(u, exList, elContentH, H->scrollR, 2, H); // scrollbar de la LISTE d'exemples

			// Clic dans le vide (hors champ de recherche, hors popup) -> defocus des champs.
			if (u.click && !anyPopup && !H->fieldClaim && u.Hit(r))
				H->focusField = 0;
			NkHomeCardMenu(u, H, st, dlg,
						   /*justOpened=*/!menuOpen);		 // menu "..." dessine EN DERNIER (au-dessus de tout)
			NkHomeCombo(u, H, /*justOpened=*/!comboWasOpen); // combo box des filtres (au-dessus de tout)
			NkHomeRenamePopup(u, H, st, /*justOpened=*/!renameWasOpen); // popup de renommage (au-dessus de tout)
		}

		// ── Écran de Chargement (section 14) : workspace sélectionné -> éditeur ──
		// Progression pilotée par le VRAI chargement (LoadFolder + jenga info). Escape = annuler.
		inline void NkDrawLoading(const NkUi &u, NkHomeState *H, float32 dt) {
			NkLoadingState &L = H->dlg->loading;
			NkCodeState *st = H->st;
			if (!L.error)
				L.Tick(st, dt);

			const float32 W = (float32)u.ctx->viewW, Ht = (float32)u.ctx->viewH;
			const float32 top = u.ctx->titleBarH > 1.f ? u.ctx->titleBarH : 0.f;
			u.Rect({0, 0, W, Ht}, NkCol::background);
			// Halos d'accent subtils, légèrement animés (fond vivant).
			auto glow = [&](float32 gx, float32 gy, float32 rad, uint8 al) {
				u.dl->AddRectFilled({gx - rad, gy - rad, rad * 2, rad * 2},
									NkColor{NkCol::primary.r, NkCol::primary.g, NkCol::primary.b, al}, rad);
			};
			glow(W * 0.30f, Ht * 0.30f + math::NkSin(L.anim * 0.6f) * u.s(18), u.s(230), 14);
			glow(W * 0.72f, Ht * 0.60f + math::NkCos(L.anim * 0.5f) * u.s(22), u.s(260), 10);

			const float32 colW = u.s(460);
			const float32 cx = (W - colW) * 0.5f;
			float32 y = top + Ht * 0.10f;
			auto center = [&](const char *t, const NkColor &c, float32 yy) {
				u.Text((W - u.TextW(t)) * 0.5f, yy, t, c);
			};

			// Logo + NKCode + version
			if (H->logoIcon)
				u.dl->AddImage(H->logoIcon, {(W - u.s(52)) * 0.5f, y, u.s(52), u.s(52)}, {0, 0}, {1, 1},
							   NkColor{255, 255, 255, 255});
			else
				NkBrandMark(u, {(W - u.s(40)) * 0.5f, y, u.s(40), u.s(40)}, NkCol::primary);
			y += u.s(66);
			center("NKCode", NkCol::foreground, y);
			y += u.s(24);
			center(NkT("load.version"), NkCol::mutedFg, y);
			y += u.s(34);

			if (!L.error) {
				{
					const NkString b = NkPrintf(NkT("load.loading"), L.wsName.CStr()); // NkPrintf maison
					center(b.CStr(), NkCol::foreground, y);
				}
				y += u.s(30);
				{
					const NkString pc = NkPrintf("%d%%", (int32)(L.Progress() * 100.f));
					u.Text(cx, y, pc.CStr(), NkCol::mutedFg);
				}
				y += u.s(18);
				u.dl->AddRectFilled({cx, y, colW, u.s(6)}, NkCol::muted, u.s(3));
				u.dl->AddRectFilled({cx, y, colW * L.Progress(), u.s(6)}, NkCol::primary, u.s(3));
				y += u.s(26);
				for (int32 i = 0; i < NkLoadingState::STEPS; ++i) {
					const bool done = i < L.step, cur = (i == L.step);
					NkColor tint = done ? NkCol::success : (cur ? NkCol::primary : NkCol::mutedFg);
					if (cur) {
						const float32 p = 0.5f + 0.5f * math::NkSin(L.anim * 4.f);
						tint.a = (uint8)(120 + (int32)(135 * p));
					}
					u.Icon(done ? "check-circle" : (cur ? "refresh" : "circle"), {cx, y + u.s(1), u.s(14), u.s(14)},
						   tint);
					u.Text(cx + u.s(24), y, L.StepLabel(i).CStr(), (done || cur) ? NkCol::foreground : NkCol::mutedFg);
					y += u.s(24);
				}
				y += u.s(16);
				{
					NkString info = NkT("load.startupproj");
					info += " ";
					info += st->SelectedProject();
					info += "    \xC2\xB7    ";
					info += NkT("load.config");
					info += " ";
					info += st->ConfigName();
					center(info.CStr(), NkCol::mutedFg, y);
				}
				y += u.s(28);
				{
					NkString jv = "Jenga ";
					jv += H->settings.jengaVersion.Empty() ? NkString("\xE2\x80\xA6") : H->settings.jengaVersion;
					center(jv.CStr(), NkCol::mutedFg, y);
				}
				y += u.s(30);
				center(NkT("load.escape"), NkCol::mutedFg, y);
				if (u.ctx->input.KeyPressed(NkGuiKey::Escape))
					L.Cancel();
			} else {
				// ── État d'erreur ──
				for (int32 i = 0; i < 5; ++i) {
					const bool done = i < L.step;
					u.Icon(done ? "check-circle" : "circle", {cx, y + u.s(1), u.s(13), u.s(13)},
						   done ? NkCol::success : NkCol::mutedFg);
					u.Text(cx + u.s(22), y, L.StepLabel(i).CStr(), done ? NkCol::foreground : NkCol::mutedFg);
					y += u.s(22);
				}
				y += u.s(12);
				const NkRect box = {cx, y, colW, u.s(150)};
				u.Panel(box, NkColor{40, 20, 22, 255}, NkCol::danger, NkR::md * u.S);
				u.Icon("alert-triangle", {box.x + u.s(12), box.y + u.s(12), u.s(15), u.s(15)}, NkCol::danger);
				u.Text(box.x + u.s(34), box.y + u.s(11), NkT("load.err.title"), NkCol::danger);
				{
					const NkString b = NkPrintf(NkT("load.err.cannotparse"), L.wsName.CStr()); // NkPrintf maison
					u.TextEllipsis(box.x + u.s(14), box.y + u.s(36), colW - u.s(28), b.CStr(), NkCol::foreground);
				}
				const NkRect code = {box.x + u.s(14), box.y + u.s(58), colW - u.s(28), u.s(52)};
				u.Panel(code, NkCol::input, NkCol::border, NkR::sm * u.S);
				u.TextEllipsis(code.x + u.s(10), code.y + u.s(8), code.w - u.s(20), L.errLine.CStr(), NkCol::danger);
				if (!L.errHint.Empty()) {
					NkString h = NkT("load.cause");
					h += " ";
					h += L.errHint;
					u.TextEllipsis(code.x + u.s(10), code.y + u.s(28), code.w - u.s(20), h.CStr(), NkCol::mutedFg);
				}
				y = box.y + box.h + u.s(16);
				const float32 bw = (colW - u.s(20)) / 3.f;
				auto ebtn = [&](float32 bx, const char *icon, const char *lab, const NkColor &bg,
								const NkColor &fg) -> bool {
					const NkRect b = {bx, y, bw, u.s(34)};
					const bool hv = u.Hit(b);
					u.Rect(b, hv ? NkCol::hover : bg, NkR::md * u.S);
					const float32 tw = u.TextW(lab) + u.s(20);
					u.Icon(icon, {b.x + (bw - tw) * 0.5f, b.y + u.s(10), u.s(13), u.s(13)}, fg);
					u.TextV(b.x + (bw - tw) * 0.5f + u.s(20), b.y, u.s(34), lab, fg);
					return hv && u.click;
				};
				if (ebtn(cx, "edit", NkT("load.openjenga"), NkCol::muted, NkCol::foreground)) {
					if (H->dlg->shell)
						H->dlg->shell->LoadUiState(st->UiConfigPath().CStr());
					H->dlg->showStart = false;
					L.active = false;
					L.error = false;
					H->dlg->Close();
				}
				if (ebtn(cx + bw + u.s(10), "copy", NkT("load.copyerr"), NkCol::muted, NkCol::foreground)) {
					NkString all = L.errLine;
					if (!L.errHint.Empty()) {
						all += "\n";
						all += L.errHint;
					}
					u.ctx->SetClipboard(all.CStr());
				}
				if (ebtn(cx + (bw + u.s(10)) * 2.f, "chevron-left", NkT("load.backlauncher"), NkCol::primary,
						 NkCol::primaryFg))
					L.Cancel();
				if (u.ctx->input.KeyPressed(NkGuiKey::Escape))
					L.Cancel();
			}
		}

		// ── Point d'entree : ecran d'accueil PLEIN CADRE (via SetStartScreen) ──
		inline void DrawHome(NkEditorFrameContext &ec, NkHomeState *H) {
			if (!H || !H->dlg || !H->dlg->showStart)
				return;
			H->caretBlink += ec.dt; // avance le clignotement du caret
			if (H->caretBlink > 1.0e6f)
				H->caretBlink = 0.f; // anti-overflow lointain
			NkUi u = NkUi::From(ec);
			if (!u.Valid())
				return;
			// Langue GLOBALE : chargee des le demarrage + appliquee chaque frame -> toute l'UI se traduit
			// en temps reel (pas seulement l'ecran Parametres).
			if (!H->settings.loaded)
				H->settings.Load();
			NkI18nSet(H->settings.lang);
			NkApplyTheme(H->settings.theme,
						 H->settings.accent); // thème GLOBAL (Dark Pro/Dark/Midnight/Light) + accent, temps réel
			// Écran de chargement (section 14) : prioritaire sur le launcher tant qu'un workspace charge.
			if (H->dlg->loading.active) {
				NkDrawLoading(u, H, ec.dt);
				if (H->dlg->loading.finished) { // -> bascule vers l'éditeur
					if (H->dlg->shell)
						H->dlg->shell->LoadUiState(H->st->UiConfigPath().CStr());
					H->dlg->showStart = false;
					H->dlg->loading.active = false;
					H->dlg->loading.finished = false;
					H->dlg->Close();
				}
				return;
			}
			NkTextMenuInput(u); // menu contextuel : traite le clic AVANT les panneaux et le CONSOMME (rien ne passe)
			// Menu ouvert = MODAL : on neutralise le fond (souris hors-champ) -> plus de survol ni de clic
			// derriere ; on restaure la vraie position juste avant de dessiner le menu.
			const bool txtMenuOpen = NkTxtMenu().open;
			const NkVec2 savedMp = u.mp;
			const bool savedClick = u.click;
			if (txtMenuOpen) {
				u.mp = NkVec2{-100000.f, -100000.f};
				u.click = false;
			}
			// pompe jenga info (toolchains/projets) comme avant
			if (H->st) {
				H->st->ScanWorkspaces();
				H->st->LoadProjects();
				H->st->PollProjects();
				H->st->PollConfig();
				H->st->LoadExamples();
				H->st->PollExamples();
			}
			const float32 W = (float32)u.ctx->viewW, Ht = (float32)u.ctx->viewH;
			const float32 top = u.ctx->titleBarH > 1.f ? u.ctx->titleBarH : u.ctx->ItemHeight();
			const float32 sbW = u.s(220);
			NkHomeSidebar(u, {0.f, top, sbW, Ht - top}, H);
			const NkRect panel = {sbW, top, W - sbW, Ht - top};
			if (H->nav == 1) { // vue « Ouvrir un Workspace »
				if (NkOpenWsPanel(u, panel, &H->ow, H->st, H->dlg, ec.dt, H->icons) == 1)
					H->nav = 0;		  // Annuler -> retour Accueil
			} else if (H->nav == 2) { // wizard « Nouveau Workspace »
				if (NkNewWsPanel(u, panel, &H->nw, H->st, H->dlg, ec.dt, H->icons) == 1)
					H->nav = 0;		  // Annuler -> retour Accueil
			} else if (H->nav == 3) { // panneau « Cloner un depot Git »
				if (NkCloneGitPanel(u, panel, &H->cg, H->st, H->dlg, ec.dt, H->icons) == 1)
					H->nav = 0;		   // Annuler -> retour Accueil
			} else if (H->nav == 10) { // gestionnaire de toolchains
				if (NkToolchainsPanel(u, panel, &H->tc, &H->nw, H->st, H->dlg, ec.dt, H->icons) == 1)
					H->nav = 0;
			} else if (H->nav == 11) { // gestionnaire de plateformes
				if (NkPlatformsPanel(u, panel, &H->tc, &H->nw, H->st, H->dlg, ec.dt, H->icons) == 1)
					H->nav = 0;
			} else if (H->nav == 12) { // parametres du launcher
				if (NkSettingsPanel(u, panel, &H->settings, H->st, H->dlg, ec.dt, H->icons, &H->nav) == 1)
					H->nav = 0;
			} else {
				NkHomePanel(u, panel, H); // Accueil (recents + actions + exemples)
			}
			if (txtMenuOpen) {
				u.mp = savedMp;
				u.click = savedClick;
			}				   // restaure la vraie souris pour le menu
			NkDrawTextMenu(u); // menu contextuel des champs (clic droit) — AU-DESSUS de tout
		}

	} // namespace nkcode
} // namespace nkentseu
