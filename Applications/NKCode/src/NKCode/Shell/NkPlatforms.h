// =============================================================================
// NkPlatforms.h — Gestionnaire de Plateformes & Architectures (plein cadre).
// Statut REEL de chaque plateforme, deduit des toolchains reellement detectes
// (NkToolchainsState). Multiplateforme : le statut depend de l'hote + des SDK.
// =============================================================================
#pragma once
#include "NKCode/Shell/NkUi.h"
#include "NKCode/Shell/NkOpenWs.h"
#include "NKCode/Shell/NkNewWorkspace.h"
#include "NKCode/Shell/NkToolchains.h"
#include "NKCode/Shell/NkI18n.h"
#include "NKCode/Project/NkCodeState.h"
#include <cstdio>

namespace nkentseu {
	namespace nkcode {

		// Statut : 0 Pret, 1 Cross, 2 Requiert hote, 3 Partiel, 4 Licencie.
		// `key` = FAMILLE de plateforme (deduite du target du toolchain) OU nom exact d'un toolchain
		// (ex "zig"), OU pseudo licence ("__gdk__"...). Permet aux VARIANTES (clang-ucrt64,
		// gcc-mingw64, clang-15...) de compter automatiquement, sans les lister une a une.
		struct NkPlatDef {
				const char *id;
				const char *key;
				int32 fallback;
		};

		inline const NkPlatDef *NkPlatTable(int32 *n) {
			static const NkPlatDef T[] = {
				{"windows-x86_64", "windows", 2},
				{"linux-x86_64", "linux", 1},
				{"linux-arm64", "zig", 1},
				{"android-arm64", "android", 3},
				{"android-armeabi-v7a", "android", 3},
				{"android-x86", "android", 3},
				{"android-x86_64", "android", 3},
				{"web-wasm32", "web", 3},
				{"macos-arm64", "macos", 2},
				{"ios-arm64", "ios", 2},
				{"xbox-x86_64", "__gdk__", 3},
				{"xboxone-x86_64", "__gdk__", 3},
				{"ps4-x86_64", "__sony__", 4},
				{"ps5-x86_64", "__sony__", 4},
				{"switch-arm64", "__nintendo__", 4},
				{"harmonyos-arm64", "ohos-ndk", 3},
			};
			*n = 16;
			return T;
		}

		// Famille de plateforme d'un toolchain, deduite de son target (robuste aux variantes).
		inline NkString NkPlatKeyOf(const NkTcDetail &d) {
			// zig d'abord : il ne produit PAS de build macOS/iOS pretes (SDK Apple + codesign requis).
			// Depuis Windows/Linux il sert la cross-compilation Linux (x64/arm64) -> famille "linux".
			if (StrEq(d.name.CStr(), "zig"))
				return NkString("linux");
			const NkString &t = d.target;
			if (t.Contains("Android"))
				return NkString("android");
			if (t.Contains("WASM") || t.Contains("Web"))
				return NkString("web");
			if (t.Contains("HarmonyOS"))
				return NkString("harmonyos");
			if (t.Contains("macOS"))
				return NkString("macos");
			if (t.Contains("Linux"))
				return NkString("linux"); // inclut WSL2 (target "Linux (WSL2 ...)")
			if (t.Contains("Windows"))
				return NkString("windows");
			if (t.Contains("cross"))
				return NkString("linux");
			return NkString();
		}

		// La liste d'aptitudes `caps` (ex zig -> "linux windows macos") contient-elle `key` ?
		inline bool NkCapsHas(const NkString &caps, const char *key) {
			if (caps.Empty())
				return false;
			NkString tok;
			for (const char *c = caps.CStr();; ++c) {
				if (*c == ' ' || *c == '\0') {
					if (tok == NkString(key))
						return true;
					tok = NkString();
					if (*c == '\0')
						break;
				} else
					tok += *c;
			}
			return false;
		}

		// Renvoie TOUS les toolchains satisfaisant `key` (famille deduite, nom exact, OU aptitude testee),
		// separes par " / ". `caps` reflete un TEST reel (ex zig compile macOS mais pas iOS).
		inline bool NkPlatHasTc(NkToolchainsState *tc, const char *key, NkString &outName) {
			NkString all;
			bool any = false;
			for (usize i = 0; i < tc->sys.Size(); ++i) {
				const NkTcDetail &d = tc->sys[i];
				if (!d.found)
					continue;
				if (NkPlatKeyOf(d) == NkString(key) || StrEq(d.name.CStr(), key) || NkCapsHas(d.caps, key)) {
					if (!all.Empty())
						all += " / ";
					all += d.name;
					any = true;
				}
			}
			outName = all;
			return any;
		}

		inline int32 NkPlatStatus(NkToolchainsState *tc, const NkPlatDef &p, NkString &tcText) {
			NkString nm;
			if (p.key[0] == '_') { // pseudo (GDK/Sony/Nintendo) : jamais « pret » ici
				if (StrEq(p.key, "__gdk__")) {
					tcText = NkT("plat.gdkreq");
					return 3;
				}
				if (StrEq(p.key, "__sony__")) {
					tcText = NkT("plat.sonyreq");
					return 4;
				}
				if (StrEq(p.key, "__nintendo__")) {
					tcText = NkT("plat.nintendoreq");
					return 4;
				}
			}
			if (NkPlatHasTc(tc, p.key, nm)) {
				tcText = nm;
				return 0;
			} // toolchain detecte -> Pret
			// sinon : statut de repli
			switch (p.fallback) {
				case 1:
					tcText = StrEq(p.key, "zig") ? NkString(NkT("plat.crosszig")) : NkString(NkT("plat.cross"));
					return 1;
				case 2: {
					const bool apple = NkString(p.id).Contains("mac") || NkString(p.id).Contains("ios");
					tcText = apple ? NkT("plat.reqmacos") : NkT("stat.reqhost");
					return 2;
				}
				case 3:
					tcText = NkT("plat.sdkndkreq");
					return 3; // Partiel
				default:
					tcText = NkT("plat.sdkreq");
					return 4;
			}
		}

		inline int32 NkPlatformsPanel(const NkUi &u, const NkRect &r, NkToolchainsState *tc, NkNewWsState *nw,
									  NkCodeState *st, NkCodeDialogs *dlg, float32 dt, const NkIcons &ic) {
			(void)st;
			tc->EnsureDetectedAsync(); // detection sur thread de fond : ne gele jamais l'UI
			int32 result = 0;
			u.Rect(r, NkCol::background); // fond du panneau (theme-aware) : sinon le fond de base sombre transparaît
			const bool blockBg = tc->assistDlg || (dlg && dlg->pickerOpen);
			// En-tete + placeholder tant que la detection (thread) n'a pas fini.
			if (!tc->detected || tc->bgBusy.Load()) {
				const float32 hH0 = u.s(54);
				u.Rect({r.x, r.y, r.w, hH0}, NkCol::sidebar);
				u.Rect({r.x, r.y + hH0 - 1.f, r.w, 1.f}, NkCol::border);
				NkOwIco(u, ic.platforms, "cpu", {r.x + u.s(28), r.y + (hH0 - u.s(18)) * 0.5f, u.s(18), u.s(18)},
						NkCol::primary);
				u.Text(r.x + u.s(54), r.y + (hH0 - u.Lh()) * 0.5f, NkT("plat.title"), NkCol::foreground);
				const float32 mw = (r.w > u.s(520)) ? u.s(420) : (r.w - u.s(60));
				const float32 mh = u.s(120);
				const NkRect m = {r.x + (r.w - mw) * 0.5f, r.y + hH0 + (r.h - hH0 - mh) * 0.5f, mw, mh};
				u.Panel(m, NkCol::surface, NkCol::border, NkR::lg * u.S);
				u.Text(m.x + u.s(24), m.y + u.s(22), NkT("plat.analyzing"), NkCol::foreground);
				u.TextEllipsis(m.x + u.s(24), m.y + u.s(48), mw - u.s(48), NkT("plat.analyzesub"), NkCol::mutedFg);
				const float32 bx = m.x + u.s(24), bw = mw - u.s(48), by = m.y + u.s(82);
				const int32 ph = tc->bgPhase.Load();
				u.Rect({bx, by, bw, u.s(8)}, NkCol::muted, NkR::sm * u.S);
				u.Rect({bx, by,
						bw * ((ph >= 6)	  ? 1.f
							  : (ph >= 5) ? 0.85f
							  : (ph >= 3) ? 0.55f
										  : 0.2f),
						u.s(8)},
					   NkCol::primary, NkR::sm * u.S);
				if (u.ctx->input.KeyPressed(NkGuiKey::Escape))
					result = 1;
				return result;
			}
			int32 nP = 0;
			const NkPlatDef *P = NkPlatTable(&nP);
			// pre-calcul des statuts + resume
			int32 count[5] = {0, 0, 0, 0, 0};
			NkVector<int32> stat;
			NkVector<NkString> stTc;
			for (int32 i = 0; i < nP; ++i) {
				NkString tt;
				const int32 s = NkPlatStatus(tc, P[i], tt);
				stat.PushBack(s);
				stTc.PushBack(tt);
				count[s]++;
			}

			const float32 hH = u.s(54);
			u.Rect({r.x, r.y, r.w, hH}, NkCol::sidebar);
			u.Rect({r.x, r.y + hH - 1.f, r.w, 1.f}, NkCol::border);
			NkOwIco(u, ic.platforms, "cpu", {r.x + u.s(28), r.y + (hH - u.s(18)) * 0.5f, u.s(18), u.s(18)},
					NkCol::primary);
			u.Text(r.x + u.s(54), r.y + (hH - u.Lh()) * 0.5f, NkT("plat.title"), NkCol::foreground);
			// Bouton « Détection assistée » (droite) : pointer manuellement un SDK/toolchain raté (ex. zig iOS).
			{
				const float32 abw = u.TextW(NkT("as.button")) + u.s(44);
				const NkRect ar = {r.x + r.w - u.s(28) - abw, r.y + (hH - u.s(32)) * 0.5f, abw, u.s(32)};
				const bool ah = !blockBg && u.Hit(ar);
				u.Rect(ar, ah ? NkCol::hover : NkCol::muted, NkR::md * u.S);
				NkOwIco(u, ic.search, "search", {ar.x + u.s(12), ar.y + u.s(9), u.s(13), u.s(13)}, NkCol::foreground);
				u.TextV(ar.x + u.s(32), ar.y, u.s(32), NkT("as.button"), NkCol::foreground);
				if (ah && u.click)
					tc->OpenAssistDialog();
				char hd[40];
				std::snprintf(hd, sizeof(hd), NkT("plat.readyof"), count[0], nP);
				u.Text(ar.x - u.s(14) - u.TextW(hd), r.y + (hH - u.Lh()) * 0.5f, hd, NkCol::success);
			}

			const float32 rightW = (r.w > u.s(860)) ? u.s(280) : u.s(240);
			const float32 cx = r.x + u.s(28), rx = r.x + r.w - u.s(28) - rightW, cw = (rx - u.s(34)) - cx;
			const NkRect view = {r.x, r.y + hH, r.w, r.h - hH};
			float32 y = view.y + u.s(20);

			NkWizLabel(u, cx, y, NkT("plat.available"));
			y += u.s(26);
			// en-tete colonnes
			u.Text(cx + u.s(4), y, NkT("plat.col.id"), NkCol::mutedFg);
			u.Text(cx + cw * 0.42f, y, NkT("plat.col.status"), NkCol::mutedFg);
			u.Text(cx + cw * 0.62f, y, NkT("plat.col.tc"), NkCol::mutedFg);
			y += u.s(20);
			{
				const float32 rowH = u.s(34);
				const NkRect box = {cx, y, cw, nP * rowH + u.s(8)};
				u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
				float32 ry = box.y + u.s(6);
				const char *slab[] = {NkT("stat.ready"), NkT("stat.cross"), NkT("stat.reqhost"), NkT("stat.partial"),
									  NkT("stat.licensed")};
				const NkColor scol[] = {NkCol::success, NkCol::accent, NkCol::mutedFg, NkCol::accent, NkCol::mutedFg};
				for (int32 i = 0; i < nP; ++i) {
					const int32 s = stat[i];
					u.Text(box.x + u.s(12), ry + u.s(8), P[i].id, s == 0 ? NkCol::foreground : NkCol::mutedFg);
					// statut icone + label
					const char *dr = (s == 0) ? "check-circle" : (s == 4 ? "lock" : "alert-triangle");
					const uint32 tex = (s == 0) ? ic.valideSimple : (s == 4 ? ic.lock : 0u);
					NkOwIco(u, tex, dr, {box.x + cw * 0.42f, ry + u.s(9), u.s(13), u.s(13)}, scol[s]);
					u.Text(box.x + cw * 0.42f + u.s(18), ry + u.s(8), slab[s], scol[s]);
					u.TextEllipsis(box.x + cw * 0.62f, ry + u.s(8), cw * 0.36f, stTc[i].CStr(),
								   s == 0 ? NkCol::primary : NkCol::mutedFg);
					if (i + 1 < nP)
						u.Rect({box.x + u.s(10), ry + rowH, cw - u.s(20), 1.f}, NkCol::border);
					ry += rowH;
				}
				y = box.y + box.h + u.s(16);
			}
			// commande jengaall
			{
				const NkRect box = {cx, y, cw, u.s(38)};
				u.Panel(box, NkCol::input, NkCol::border, NkR::md * u.S);
				NkOwIco(u, ic.fileCode, "terminal", {box.x + u.s(12), box.y + u.s(12), u.s(14), u.s(14)},
						NkCol::primary);
				u.Text(box.x + u.s(34), box.y + u.s(11), "jenga build --platform jengaall", NkCol::foreground);
				u.Text(box.x + u.s(34) + u.TextW("jenga build --platform jengaall") + u.s(12), box.y + u.s(11),
					   NkT("plat.buildall"), NkCol::mutedFg);
				y += u.s(48);
			}

			// ===== COLONNE DROITE : legende + resume =====
			float32 ry = view.y + u.s(20);
			NkWizLabel(u, rx, ry, NkT("plat.legend"));
			ry += u.s(24);
			auto leg = [&](const char *drawn, uint32 tex, const NkColor &col, const char *title, const char *sub) {
				NkOwIco(u, tex, drawn, {rx, ry + u.s(1), u.s(14), u.s(14)}, col);
				u.Text(rx + u.s(24), ry - u.s(1), title, NkCol::foreground);
				u.TextEllipsis(rx + u.s(24), ry + u.s(16), rightW - u.s(24), sub, NkCol::mutedFg);
				ry += u.s(40);
			};
			leg("check-circle", ic.valideSimple, NkCol::success, NkT("stat.ready"), NkT("plat.leg.ready.s"));
			leg("alert-triangle", 0u, NkCol::accent, NkT("plat.leg.cross.t"), NkT("plat.leg.cross.s"));
			leg("alert-triangle", 0u, NkCol::mutedFg, NkT("stat.reqhost"), NkT("plat.leg.host.s"));
			leg("lock", ic.lock, NkCol::mutedFg, NkT("stat.licensed"), NkT("plat.leg.lic.s"));
			ry += u.s(6);
			NkWizLabel(u, rx, ry, NkT("plat.summary"));
			ry += u.s(24);
			auto sumRow = [&](const char *lab, int32 v, const NkColor &col) {
				const NkRect b = {rx, ry, rightW, u.s(30)};
				u.Panel(b, NkCol::surface, NkCol::border, NkR::sm * u.S);
				u.TextV(b.x + u.s(12), b.y, u.s(30), lab, NkCol::foreground);
				char n[8];
				std::snprintf(n, sizeof(n), "%d", v);
				const NkRect badge = {b.x + rightW - u.s(38), b.y + u.s(5), u.s(28), u.s(20)};
				u.dl->AddRectFilled(badge, col, u.s(4));
				u.TextV(badge.x + (u.s(28) - u.TextW(n)) * 0.5f, badge.y - u.s(3), u.s(20), n, NkCol::primaryFg);
				ry += u.s(36);
			};
			sumRow(NkT("plat.sum.ready"), count[0], NkCol::success);
			sumRow(NkT("stat.cross"), count[1], NkCol::accent);
			sumRow(NkT("plat.sum.partial"), count[3], NkCol::accent);
			sumRow(NkT("plat.sum.host"), count[2], NkCol::muted);
			sumRow(NkT("plat.sum.lic"), count[4], NkCol::muted);

			// ── Modale « Détection assistée » (partagée avec Toolchains) ──
			const bool assistOpen = NkAssistedDetectDialog(u, r, tc, nw, dlg, dt, ic);
			if (!assistOpen && u.ctx->input.KeyPressed(NkGuiKey::Escape))
				result = 1;
			return result;
		}

	} // namespace nkcode
} // namespace nkentseu
