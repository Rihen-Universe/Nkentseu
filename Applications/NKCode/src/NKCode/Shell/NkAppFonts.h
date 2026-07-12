#pragma once
// =============================================================================
// NkAppFonts.h — Polices de REPLI externes de NKCode (chargées au runtime,
// pas embarquées) : tout glyphe absent d'Inter/DejaVu est cherché dans
// data/fonts. Rôles : broad (large couverture), cjk (idéogrammes, opt-in),
// emoji. Extrait de main.cpp (modularisation).
// =============================================================================
#include "NKEditorKit/NkEditorKit.h"
#include "NKFileSystem/NkFile.h"
#include "NKContainers/String/NkFormat.h" // NkPrintf (outils maison)

namespace nkentseu {
	namespace nkcode {

		// Localise les polices de repli et les déclare à NKGui. À appeler AVANT
		// l'Init du shell (l'atlas est construit à l'Init).
		inline void NkLoadFallbackFonts() {
			static const char *dirs[] = {"Applications/NKCode/data/fonts/", "data/fonts/", "NKCode/data/fonts/", ""};
			auto find = [&](const char *const *names, char *out, nk_size cap) {
				out[0] = '\0';
				for (const char *const *np = names; *np; ++np)
					for (const char *const *dp = dirs;; ++dp) {
						const NkString p = NkPrintf("%s%s", *dp, *np);
						if (NkFile::Exists(p.CStr())) {
							usize k = 0;
							for (const char *q = p.CStr(); *q && k + 1 < cap; ++q)
								out[k++] = *q;
							out[k] = '\0';
							return;
						}
						if (!**dp)
							break;
					}
				out[0] = '\0';
			};
			static char broad[600], cjk[600], emoji[600];
			const char *broadN[] = {"NotoSans-Regular.ttf", nullptr};
			const char *cjkN[] = {"NotoSansSC-Regular.ttf", "NotoSansSC.ttf", "NotoSansCJKsc-Regular.otf", nullptr};
			const char *emojiN[] = {"NotoEmoji-Regular.ttf", nullptr};
			find(broadN, broad, sizeof(broad));
			find(cjkN, cjk, sizeof(cjk));
			find(emojiN, emoji, sizeof(emoji));
			nkgui::NkSetFallbackFontPaths(broad[0] ? broad : nullptr, cjk[0] ? cjk : nullptr,
										  emoji[0] ? emoji : nullptr);
		}

	} // namespace nkcode
} // namespace nkentseu
