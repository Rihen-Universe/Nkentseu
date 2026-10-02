#pragma once
// =============================================================================
// NkEditeurVide.h — LA PAGE DE L'EDITEUR VIDE (maquette A « accueil », reprise
// dans D ; Rihen : « ca me plait enormement »).
//
// Quand aucun fichier n'est ouvert, l'editeur montre : le mot-logo et la phrase
// de NKCode, DEMARRER (nouveau workspace Jenga, ouvrir un dossier, cloner, un
// exemple, rejoindre une session — a venir), RECENTS (les vrais workspaces
// recents, epingles en tete), RACCOURCIS (les touches REELLEMENT enregistrees),
// OUTILS (toolchains, plateformes, reglages) et la case « Afficher l'accueil
// quand aucun fichier n'est ouvert ».
//
// ⚠️ CE N'EST PAS L'ACCUEIL DU LANCEUR (avant d'ouvrir un workspace, lanceur
//    partage NkHomeLanceur.h) : c'est l'editeur, workspace ouvert ou non.
// ⚠️ LES ENTREES SONT CELLES DE NkHome.h : memes destinations (assistant de
//    nouveau workspace, ouverture de dossier, clonage, exemples, toolchains,
//    plateformes, reglages) — aucune action inventee.
// =============================================================================
#include "NKCode/Shell/NkSynthese.h"

namespace nkentseu {
	namespace nkcode {

		/// L'onglet « Accueil » ferme (×) : la page se retire jusqu'au prochain fichier.
		inline bool &NkAccueilEditeurFerme() {
			static bool f = false;
			return f;
		}

		/// Un chemin tel que l'OS l'ecrit (Windows : barres inverses).
		inline NkString NkCheminAffiche(const NkString &p) {
#if defined(_WIN32)
			NkString r;
			for (usize i = 0; i < p.Size(); ++i)
				r += (p[i] == '/') ? '\\' : p[i];
			return r;
#else
			return p;
#endif
		}

		inline bool NkPageEditeurVide(NkEditorFrameContext &ec, void *user) {
			auto *H = static_cast<NkHomeState *>(user);
			if (!H || !H->dlg || !H->settings.accueilEditeur || NkAccueilEditeurFerme())
				return false;
			auto &ctx = ec.Ui();
			NkCodeDialogs *d = H->dlg;
			NkCodeState *st = d->st;
			const NkApparenceEtat &E = NkApparenceCourante();
			const NkApparencePalette &a = E.pal;
			const bool ilots = E.dispo.ilots;
			const NkRect clip = ctx.DL().CurrentClip();
			NkUi u = NkUiDe(ctx);
			const float32 S = u.S;
			u.Rect(clip, ctx.theme.bgPrimary);
			const NkIcons &ic = H->icons;
			const NkVec2 m = ctx.input.mousePos;
			const bool atteint = ctx.PointReachable(m) && ctx.popupDepth == 0;
			auto hit = [&](const NkRect &r) { return atteint && NkGuiRectContains(r, m); };
			int32 action = 0;

			// ── Cadre : 64 px de marge, 1120 de large au plus, centre ──
			float32 W = clip.w - 128.f * S;
			if (W > 1120.f * S)
				W = 1120.f * S;
			const float32 x0 = clip.x + (clip.w - W) * 0.5f;
			// ── L'onglet « Accueil » (maquette A/D), fermable ──
			{
				const float32 th = 36.f * S;
				const NkRect barre = {clip.x, clip.y, clip.w, th};
				u.Rect(barre, ilots ? a.panneau : ctx.theme.tabBar);
				if (!ilots)
					u.Rect({barre.x, barre.y + th - 1.f, barre.w, 1.f}, a.trait);
				const char *t = "Accueil";
				const float32 tw = 14.f * S + 14.f * S + 7.f * S + u.TextW(t) + 8.f * S + 14.f * S + 10.f * S;
				const NkRect tab = ilots ? NkRect{clip.x + 8.f * S, clip.y + 5.f * S, tw, th - 10.f * S}
										 : NkRect{clip.x, clip.y, tw, th};
				if (ilots)
					u.Rect(tab, a.haut, 8.f * S);
				else {
					u.Rect(tab, ctx.theme.bgPrimary);
					u.Rect({tab.x, tab.y, tab.w, 2.f * S}, a.accent);
				}
				if (ic.accueil)
					NkDrawIcon(u, ic.accueil, {tab.x + 12.f * S, tab.y + (tab.h - 14.f * S) * 0.5f, 14.f * S, 14.f * S}, a.fg);
				u.TextV(tab.x + 33.f * S, tab.y, tab.h, t, a.fg);
				const NkRect xr = {tab.x + tab.w - 24.f * S, tab.y + (tab.h - 16.f * S) * 0.5f, 16.f * S, 16.f * S};
				const bool xh = hit(xr);
				if (xh)
					u.Rect(xr, a.survol, 4.f * S);
				const NkColor xc = xh ? a.fg : a.fg3;
				u.dl->AddLine({xr.x + 4.5f * S, xr.y + 4.5f * S}, {xr.x + 11.5f * S, xr.y + 11.5f * S}, xc, 1.3f * S);
				u.dl->AddLine({xr.x + 4.5f * S, xr.y + 11.5f * S}, {xr.x + 11.5f * S, xr.y + 4.5f * S}, xc, 1.3f * S);
				if (xh && ctx.input.mouseClicked[0]) {
					NkAccueilEditeurFerme() = true;
					ctx.input.mouseClicked[0] = false;
				}
			}
			float32 y = clip.y + 36.f * S + 64.f * S;

			// ── En-tete : mot-logo + phrase ──
			{
				const bool clair = a.clair;
				const uint32 mot = clair ? (H->logoWordDark ? H->logoWordDark : H->logoWord) : H->logoWord;
				const int32 mw = clair && H->logoWordDark ? H->wordWD : H->wordW;
				const int32 mh = clair && H->logoWordDark ? H->wordHD : H->wordH;
				float32 lx = x0;
				const float32 lh = 54.f * S;
				if (mot && mh > 0) {
					const float32 lw = lh * (float32)mw / (float32)mh;
					ctx.DL().AddImage(mot, {x0, y, lw, lh}, {0, 0}, {1, 1}, {255, 255, 255, 255});
					lx += lw + 22.f * S;
				} else if (ic.files) {
					lx += 0.f;
				}
				NkString jv = !H->settings.jengaVersion.Empty() ? H->settings.jengaVersion : NkEmbeddedJenga::EmbeddedVersion();
				u.Text(lx, y + lh - 6.f * S - 2.f * u.Lh(), "L'IDE de la famille Nkentseu, b\xC3\xA2ti sur Jenga.", a.fg2);
				const NkString sous = jv.Empty() ? NkPrintf("NKCode %s", NkCodeVersion())
												 : NkPrintf("NKCode %s \xC2\xB7 Jenga %s d\xC3\xA9tect\xC3\xA9", NkCodeVersion(), jv.CStr());
				u.Text(lx, y + lh - 6.f * S - u.Lh(), sous.CStr(), a.fg3);
				y += lh + 40.f * S;
			}

			const float32 droiteW = 320.f * S, ecart = 40.f * S;
			const float32 gaucheW = W - droiteW - ecart;
			const float32 xd = x0 + gaucheW + ecart;
			auto titre = [&](float32 x, float32 yy, const char *t) { u.Text(x, yy, t, a.fg2); };
			auto kbd = [&](float32 xr, float32 cy, const char *k) {
				const float32 kw = u.TextW(k) * 0.86f + 10.f * S;
				const NkRect kr = {xr - kw, cy - 9.f * S, kw, 18.f * S};
				u.Rect(kr, ilots ? a.panneau : a.haut, 4.f * S);
				NkSynContour(u, kr, 4.f * S, a.trait);
				u.TextV(kr.x + 5.f * S, kr.y, kr.h, k, a.fg2);
			};

			// ═══ COLONNE DE GAUCHE ═══
			float32 yg = y;
			titre(x0, yg, "D\xC3\x89MARRER");
			yg += u.Lh() + 10.f * S;
			struct Act {
					uint32 tex;
					void (*gl)(const NkUi &, const NkRect &, const NkColor &);
					const char *t, *s, *k;
					int32 code;
					bool aVenir;
			};
			const NkString nEx = (st && !st->examples.Empty()) ? NkPrintf("%d exemples livr\xC3\xA9s avec Jenga", (int32)st->examples.Size())
															 : NkString("Les exemples livr\xC3\xA9s avec Jenga");
			const Act acts[5] = {
				{ic.newFile2 ? ic.newFile2 : ic.nouveau, nullptr, "Nouveau workspace Jenga\xE2\x80\xA6",
				 "Assistant : langage, plateformes, configurations", nullptr, 1, false},
				{ic.ouvrirDossier, nullptr, "Ouvrir un dossier\xE2\x80\xA6", "Un dossier avec un .jenga \xC2\xAB with workspace \xC2\xBB",
				 nullptr, 2, false},
				{ic.clonerTel ? ic.clonerTel : ic.cloner, nullptr, "Cloner un d\xC3\xA9p\xC3\xB4t Git\xE2\x80\xA6", "GitHub, GitLab ou une adresse",
				 nullptr, 3, false},
				{ic.exemple, nullptr, "Ouvrir un exemple Jenga", nEx.CStr(), nullptr, 4, false},
				{0, &NkSynLien, "Rejoindre une session en direct\xE2\x80\xA6", "\xC3\x80 venir : coller un lien d'invitation nkcode://session/\xE2\x80\xA6",
				 nullptr, 0, true},
			};
			const int32 nActs = E.dispo.synthese ? 5 : 4;
			for (int32 i = 0; i < nActs; ++i) {
				const Act &c = acts[i];
				const NkRect r = {x0, yg, gaucheW, 52.f * S};
				const bool hov = !c.aVenir && hit(r);
				if (hov)
					u.Rect(r, a.survol, 10.f * S);
				const NkColor ico = c.aVenir ? a.fg3 : a.accent;
				const NkRect ir = {r.x + 14.f * S, r.y + (r.h - 20.f * S) * 0.5f, 20.f * S, 20.f * S};
				if (c.tex)
					NkDrawIcon(u, c.tex, ir, ico);
				else if (c.gl)
					c.gl(u, ir, ico);
				const float32 tx = ir.x + 20.f * S + 14.f * S;
				u.Text(tx, r.y + 8.f * S, c.t, c.aVenir ? a.fg2 : a.fg);
				u.Text(tx + 0.5f * S, r.y + 8.f * S, c.t, c.aVenir ? a.fg2 : a.fg); // gras
				u.Text(tx, r.y + 8.f * S + u.Lh(), c.s, a.fg3);
				if (c.aVenir)
					editorkit::NkTooltip(ctx, hit(r), c.s);
				if (hov && ctx.input.mouseClicked[0])
					action = c.code;
				yg += 52.f * S;
			}
			// ── RECENTS ──
			yg += 30.f * S;
			titre(x0, yg, "R\xC3\x89" "CENTS");
			{
				const char *tout = "Tout afficher";
				const NkRect tr = {x0 + gaucheW - u.TextW(tout), yg, u.TextW(tout), u.Lh()};
				u.Text(tr.x, tr.y, tout, a.accent);
				if (hit(tr) && ctx.input.mouseClicked[0])
					action = 5;
			}
			yg += u.Lh() + 10.f * S;
			if (st) {
				NkVector<NkString> chemins, noms;
				NkVector<bool> epingle;
				for (usize i = 0; i < st->pinned.Size(); ++i) {
					chemins.PushBack(st->pinned[i]);
					noms.PushBack(i < st->pinnedNames.Size() ? st->pinnedNames[i] : NkString());
					epingle.PushBack(true);
				}
				for (usize i = 0; i < st->recents.Size(); ++i) {
					chemins.PushBack(st->recents[i]);
					noms.PushBack(i < st->recentNames.Size() ? st->recentNames[i] : NkString());
					epingle.PushBack(false);
				}
				const int64 now = NkCodeState::NowEpoch();
				const float32 bas = clip.y + clip.h - 20.f * S;
				for (usize i = 0; i < chemins.Size() && i < 8; ++i) {
					const NkRect r = {x0, yg, gaucheW, 52.f * S};
					if (r.y + r.h > bas)
						break;
					const bool hov = hit(r);
					if (ilots)
						u.Rect(r, hov ? NkMelange(a.haut, a.fg, 0.05f) : a.haut, 10.f * S);
					else {
						if (hov)
							u.Rect(r, a.survol, 8.f * S);
						u.Rect({r.x, r.y + r.h - 1.f, r.w, 1.f}, a.trait2);
					}
					const NkRect ir = {r.x + 14.f * S, r.y + (r.h - 22.f * S) * 0.5f, 22.f * S, 22.f * S};
					if (ic.folderRoot)
						NkDrawIcon(u, ic.folderRoot, ir, {255, 255, 255, 255});
					const auto meta = st->WorkspaceMeta(chemins[i].CStr());
					const NkString nom = noms[i].Empty() ? NkPath(chemins[i].CStr()).GetParent().GetFileName() : noms[i];
					const NkString dossier = NkCheminAffiche(NkPath(chemins[i].CStr()).GetParent().ToString());
					float32 tx = ir.x + 22.f * S + 14.f * S;
					// meta a droite (langage · plateformes / age), etoile si epinglee
					float32 xr = r.x + r.w - 14.f * S;
					if (epingle[i]) {
						const NkRect sr = {xr - 14.f * S, r.y + (r.h - 14.f * S) * 0.5f, 14.f * S, 14.f * S};
						if (ic.star)
							NkDrawIcon(u, ic.star, sr, {210, 153, 34, 255});
						xr -= 14.f * S + 10.f * S;
					}
					const NkString l1 = meta.langVer.Empty() ? meta.platforms
										: meta.platforms.Empty() ? meta.langVer
																 : NkPrintf("%s \xC2\xB7 %s", meta.langVer.CStr(), meta.platforms.CStr());
					const NkString l2 = NkCodeState::HumanAge(meta.activity, now);
					const float32 w1 = u.TextW(l1.CStr()), w2 = u.TextW(l2.CStr());
					u.Text(xr - w1, r.y + 8.f * S, l1.CStr(), a.fg2);
					u.Text(xr - w2, r.y + 8.f * S + u.Lh(), l2.CStr(), a.fg2);
					const float32 maxT = xr - (w1 > w2 ? w1 : w2) - 16.f * S - tx;
					u.TextEllipsis(tx, r.y + 8.f * S, maxT, nom.CStr(), a.fg);
					u.TextEllipsis(tx + 0.5f * S, r.y + 8.f * S, maxT, nom.CStr(), a.fg);
					u.TextEllipsis(tx, r.y + 8.f * S + u.Lh(), maxT, dossier.CStr(), a.fg3);
					if (hov && ctx.input.mouseClicked[0])
						action = 100 + (int32)i, H->ctxPath = chemins[i];
					yg += 52.f * S + (ilots ? 4.f * S : 0.f);
				}
				if (chemins.Empty())
					u.Text(x0 + 14.f * S, yg + 8.f * S, "Aucun workspace r\xC3\xA9" "cent.", a.fg3);
			}

			// ═══ COLONNE DE DROITE ═══
			float32 yd = y;
			auto carte = [&](float32 yy, float32 h) {
				const NkRect c = {xd, yy, droiteW, h};
				if (ilots)
					u.Rect(c, a.haut, 12.f * S);
				else {
					u.Rect(c, a.panneau, 10.f * S);
					NkSynContour(u, c, 10.f * S, a.trait);
				}
				return c;
			};
			// RACCOURCIS : les touches REELLEMENT enregistrees (main.cpp, la coquille).
			{
				struct R2 {
						const char *t, *k;
				};
				static const R2 kR[] = {{"Palette de commandes", "Ctrl+P"}, {"Construire (Jenga)", "Ctrl+B"},
										{"D\xC3\xA9marrer (jenga run)", "Ctrl+R"}, {"Enregistrer", "Ctrl+S"},
										{"Formater le document", "Ctrl+Maj+I"}, {"Assistant IA", "Ctrl+Maj+A"}};
				const int32 n = (int32)(sizeof(kR) / sizeof(kR[0]));
				const NkRect c = carte(yd, 16.f * S + u.Lh() + 10.f * S + (float32)n * 30.f * S + 14.f * S);
				titre(c.x + 18.f * S, c.y + 16.f * S, "RACCOURCIS");
				float32 ry = c.y + 16.f * S + u.Lh() + 10.f * S;
				for (int32 i = 0; i < n; ++i) {
					u.TextV(c.x + 18.f * S, ry, 30.f * S, kR[i].t, a.fg);
					kbd(c.x + c.w - 18.f * S, ry + 15.f * S, kR[i].k);
					ry += 30.f * S;
				}
				yd = c.y + c.h + 16.f * S;
			}
			// OUTILS
			{
				NkString tcs;
				if (st)
					for (usize i = 0; i < st->toolchains.Size() && i < 3; ++i) {
						if (!tcs.Empty())
							tcs += " \xC2\xB7 ";
						tcs += st->toolchains[i].name;
					}
				if (tcs.Empty())
					tcs = "Compilateurs d\xC3\xA9tect\xC3\xA9s par Jenga";
				struct O {
						uint32 tex;
						const char *t;
						NkString s;
						int32 code;
				};
				const O outils[3] = {{ic.toolchains, "Toolchains", tcs, 6},
									 {ic.platforms ? ic.platforms : ic.monitor, "Plateformes", NkString("Windows, Linux, Android, Web"), 7},
									 {ic.gear, "R\xC3\xA9glages", NkString("Th\xC3\xA8me, apparence, jeu d'ic\xC3\xB4nes"), 8}};
				const NkRect c = carte(yd, 16.f * S + u.Lh() + 10.f * S + 3.f * 46.f * S + 10.f * S);
				titre(c.x + 18.f * S, c.y + 16.f * S, "OUTILS");
				float32 oy = c.y + 16.f * S + u.Lh() + 6.f * S;
				for (const O &o : outils) {
					const NkRect r = {c.x + 8.f * S, oy, c.w - 16.f * S, 46.f * S};
					const bool hov = hit(r);
					if (hov)
						u.Rect(r, a.survol, 8.f * S);
					const NkRect ir = {r.x + 10.f * S, r.y + (r.h - 18.f * S) * 0.5f, 18.f * S, 18.f * S};
					if (o.tex)
						NkDrawIcon(u, o.tex, ir, a.fg2);
					const float32 tx = ir.x + 18.f * S + 12.f * S;
					u.Text(tx, r.y + 6.f * S, o.t, a.fg);
					u.Text(tx + 0.5f * S, r.y + 6.f * S, o.t, a.fg);
					u.TextEllipsis(tx, r.y + 6.f * S + u.Lh(), r.x + r.w - tx - 6.f * S, o.s.CStr(), a.fg3);
					if (hov && ctx.input.mouseClicked[0])
						action = o.code;
					oy += 46.f * S;
				}
				yd = c.y + c.h + 16.f * S;
			}
			// La case : « Afficher l'accueil quand aucun fichier n'est ouvert »
			{
				const NkRect cr = {xd, yd + 2.f * S, 15.f * S, 15.f * S};
				u.Rect(cr, a.plein, 4.f * S);
				u.dl->AddLine({cr.x + 4.f * S, cr.y + 8.f * S}, {cr.x + 6.5f * S, cr.y + 10.5f * S}, {255, 255, 255, 255}, 2.f * S);
				u.dl->AddLine({cr.x + 6.5f * S, cr.y + 10.5f * S}, {cr.x + 12.f * S, cr.y + 5.f * S}, {255, 255, 255, 255}, 2.f * S);
				const char *t = "Afficher l'accueil quand aucun fichier n'est ouvert";
				u.Text(cr.x + 24.f * S, yd + 9.5f * S - u.Lh() * 0.5f, t, a.fg2);
				const NkRect zone = {cr.x, yd, 24.f * S + u.TextW(t), 19.f * S};
				if (hit(zone) && ctx.input.mouseClicked[0])
					action = 9;
			}

			// ── Les actions : les MEMES destinations que l'accueil du lanceur ──
			if (action) {
				ctx.input.mouseClicked[0] = false;
				switch (action) {
					case 1:
						d->showNewWs = true;
						d->wsAddAsRoot = st && st->HasWorkspace();
						break;
					case 2:
						d->OpenFolderDialog();
						break;
					case 3:
						H->nav = 3; // le panneau « Cloner un depot » du lanceur
						d->ShowStart();
						break;
					case 4:
					case 5:
						H->nav = 0; // l'accueil du lanceur : exemples et tous les recents
						d->ShowStart();
						break;
					case 6:
						d->tcOpen = true;
						break;
					case 7:
						H->nav = 11;
						d->ShowStart();
						break;
					case 8:
						d->showPrefs = true;
						H->settings.cat = 3;
						break;
					case 9:
						H->settings.accueilEditeur = false;
						H->settings.Save();
						break;
					default:
						if (action >= 100 && !H->ctxPath.Empty())
							d->DoLoad(NkCodeState::RecentFolder(H->ctxPath.CStr()));
						break;
				}
			}
			return true;
		}

	} // namespace nkcode
} // namespace nkentseu
