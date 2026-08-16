// =============================================================================
// Noge/Panels/ConsolePanelGui.cpp — portage pilote NKUI -> NKGui (cf. .h)
// =============================================================================
#include "ConsolePanelGui.h"
#include "NKGui/NKGui.h"
#include <cstdio>

namespace nkentseu {
	namespace noge {

		using namespace nkgui;

		// PushLine / Clear / Passes / LevelPrefix vivent dans NkConsoleModel
		// (en-tete neutre PARTAGE avec le panneau NKUI). Corps inchange.

		// =====================================================================
		// Seule la COULEUR reste ici : nkgui::NkColor et nkui::NkColor sont deux
		// types distincts, sans conversion possible.
		// =====================================================================
		nkgui::NkColor ConsolePanelGui::LevelColor(NkLogLevel lv) noexcept {
			switch (lv) {
				case NkLogLevel::NK_ERROR:
				case NkLogLevel::NK_CRITICAL:
					return {255, 80, 80, 255};
				case NkLogLevel::NK_WARN:
					return {255, 200, 60, 255};
				case NkLogLevel::NK_DEBUG:
					return {120, 180, 255, 255};
				case NkLogLevel::NK_TRACE:
					return {150, 150, 150, 255};
				default:
					return {220, 220, 220, 255};
			}
		}

		// =====================================================================
		// Corps du panneau. Le shell a deja ouvert la fenetre ancree : on ne
		// dessine QUE le contenu, et jamais une couleur en dur hors des niveaux
		// de log (qui sont une semantique du produit, pas un habillage).
		// =====================================================================
		void ConsolePanelGui::OnUI(editorkit::NkEditorFrameContext &ec) {
			NkGuiContext &ctx = ec.Ui();

			// ── Barre d'outils : bouton + compteurs sur une meme ligne ────────
			if (ec.Button("Effacer"))
				Clear();

			if (mErrorCount > 0) {
				ctx.SameLine();
				char buf[32];
				std::snprintf(buf, sizeof(buf), "Err:%u", mErrorCount);
				const NkRect r = ctx.NextItemRect(70.f, ctx.font ? ctx.font->LineHeight() : 16.f);
				TextAt(ctx, {r.x, r.y}, buf, NkColor{255, 80, 80, 255});
			}
			if (mWarnCount > 0) {
				ctx.SameLine();
				char buf[32];
				std::snprintf(buf, sizeof(buf), "Avert:%u", mWarnCount);
				const NkRect r = ctx.NextItemRect(80.f, ctx.font ? ctx.font->LineHeight() : 16.f);
				TextAt(ctx, {r.x, r.y}, buf, NkColor{255, 200, 60, 255});
			}

			// ── Filtres par niveau ────────────────────────────────────────────
			Checkbox(ctx, "Info##ci", mShowInfo);
			ctx.SameLine();
			Checkbox(ctx, "Avert##cw", mShowWarn);
			ctx.SameLine();
			Checkbox(ctx, "Err##ce", mShowError);
			ctx.SameLine();
			Checkbox(ctx, "Dbg##cd", mShowDebug);

			// ── Filtre texte ──────────────────────────────────────────────────
			InputText(ctx, "##cfilt", mFilterBuf, static_cast<int32>(sizeof(mFilterBuf)));

			Separator(ctx);

			// ── Lignes : zone defilante fournie par NKGui ─────────────────────
			// BeginChild gere le defilement et le decoupage ; il est persistant
			// par identifiant, donc la position de scroll survit aux frames.
			const float32 footerH = 24.f;
			const float32 h = ctx.AvailHeight() - footerH;
			const NkRect area = ctx.NextItemRect(-1.f, h > 40.f ? h : 40.f);

			if (BeginChild(ctx, "console_scroll", area, true)) {
				const float32 lh = ctx.font ? ctx.font->LineHeight() : 16.f;
				for (nk_usize i = 0; i < mLines.Size(); ++i) {
					const auto &line = mLines[i];
					if (!Passes(line))
						continue;

					NkString display = NkString(LevelPrefix(line.level)) + line.text;
					if (line.count > 1) {
						char cnt[16];
						std::snprintf(cnt, sizeof(cnt), " (%u)", line.count);
						display += NkString(cnt);
					}

					const NkRect lr = ctx.NextItemRect(-1.f, lh);
					TextAt(ctx, {lr.x, lr.y}, display.CStr(), LevelColor(line.level));
				}
				EndChild(ctx);
			}

			// ── Bascule d'auto-defilement ─────────────────────────────────────
			Checkbox(ctx, "Auto-scroll", mAutoScroll);
		}

	} // namespace noge
} // namespace nkentseu
