#include "ConsolePanel.h"
#include "NKUI/NkUIMenu.h"
#include <cstring>
#include <cstdio>

namespace nkentseu {
    namespace Unkeny {

        using namespace nkui;

        // =====================================================================
        void ConsolePanel::PushLine(const char* text, NkLogLevel level) noexcept {
            if (!text) return;

            // Fusion des lignes identiques consécutives
            if (!mLines.IsEmpty()) {
                auto& last = mLines[mLines.Size() - 1];
                if (last.level == level && last.text == text) {
                    ++last.count;
                    mScrollToEnd = mAutoScroll;
                    return;
                }
            }

            if (mLines.Size() >= kMaxLines) mLines.EraseAt(0);

            NkConsoleLine line;
            line.text  = NkString(text);
            line.level = level;
            line.count = 1;
            mLines.PushBack(line);

            if (level == NkLogLevel::NK_ERROR   || level == NkLogLevel::NK_CRITICAL) ++mErrorCount;
            if (level == NkLogLevel::NK_WARN)  ++mWarnCount;

            mScrollToEnd = mAutoScroll;
        }

        void ConsolePanel::Clear() noexcept {
            mLines.Clear();
            mErrorCount = 0;
            mWarnCount  = 0;
        }

        // =====================================================================
        nkui::NkColor ConsolePanel::LevelColor(NkLogLevel lv) noexcept {
            switch (lv) {
                case NkLogLevel::NK_ERROR:
                case NkLogLevel::NK_CRITICAL:   return {255, 80,  80,  255};
                case NkLogLevel::NK_WARN:       return {255, 200, 60,  255};
                case NkLogLevel::NK_DEBUG:      return {120, 180, 255, 255};
                case NkLogLevel::NK_TRACE:      return {150, 150, 150, 255};
                default:                        return {220, 220, 220, 255};
            }
        }

        const char* ConsolePanel::LevelPrefix(NkLogLevel lv) noexcept {
            switch (lv) {
                case NkLogLevel::NK_ERROR:      return "[ERR] ";
                case NkLogLevel::NK_CRITICAL:   return "[CRT] ";
                case NkLogLevel::NK_WARN:       return "[WRN] ";
                case NkLogLevel::NK_DEBUG:      return "[DBG] ";
                case NkLogLevel::NK_TRACE:      return "[TRC] ";
                default:                        return "[INF] ";
            }
        }

        // =====================================================================
        void ConsolePanel::Render(NkUIContext& ctx,
                                  NkUIWindowManager& wm,
                                  NkUIDrawList& dl,
                                  NkUIFont& font,
                                  NkUILayoutStack& ls,
                                  NkUIRect rect) noexcept {

            NkUIWindow::SetNextWindowPos ({rect.x, rect.y});
            NkUIWindow::SetNextWindowSize({rect.w, rect.h});

            bool open = true;
            if (!NkUIWindow::Begin(ctx, wm, dl, font, ls,
                                   "Console",
                                   &open,
                                   NkUIWindowFlags::NK_NO_MOVE |
                                   NkUIWindowFlags::NK_NO_RESIZE)) {
                NkUIWindow::End(ctx, wm, dl, ls);
                return;
            }

            // ── Barre d'outils ────────────────────────────────────────────────
            NkUI::BeginRow(ctx, ls, 24.f);

            if (NkUI::ButtonSmall(ctx, ls, dl, font, "Effacer")) Clear();
            NkUI::SameLine(ctx, ls);

            // Compteurs d'erreurs/warnings
            if (mErrorCount > 0) {
                char buf[32]; snprintf(buf, sizeof(buf), "Err:%u", mErrorCount);
                NkUI::TextColored(ctx, ls, dl, font, {255,80,80,255}, buf);
                NkUI::SameLine(ctx, ls);
            }
            if (mWarnCount > 0) {
                char buf[32]; snprintf(buf, sizeof(buf), "Avert:%u", mWarnCount);
                NkUI::TextColored(ctx, ls, dl, font, {255,200,60,255}, buf);
                NkUI::SameLine(ctx, ls);
            }

            NkUI::EndRow(ctx, ls);

            // ── Filtres ───────────────────────────────────────────────────────
            NkUI::BeginRow(ctx, ls, 22.f);
            NkUI::Toggle(ctx, ls, dl, font, "Info##ci",  mShowInfo);
            NkUI::SameLine(ctx, ls);
            NkUI::Toggle(ctx, ls, dl, font, "Avert##cw", mShowWarn);
            NkUI::SameLine(ctx, ls);
            NkUI::Toggle(ctx, ls, dl, font, "Err##ce",   mShowError);
            NkUI::SameLine(ctx, ls);
            NkUI::Toggle(ctx, ls, dl, font, "Dbg##cd",   mShowDebug);
            NkUI::EndRow(ctx, ls);

            // ── Filtre texte ──────────────────────────────────────────────────
            NkUI::BeginRow(ctx, ls, 22.f);
            NkUI::SetNextGrow(ctx, ls);
            NkUI::InputText(ctx, ls, dl, font, "##cfilt", mFilterBuf, (int)sizeof(mFilterBuf));
            NkUI::EndRow(ctx, ls);

            NkUI::Separator(ctx, ls, dl);

            // ── Lignes (zone scrollable) ──────────────────────────────────────
            float32 scrollY = 0.f;
            NkUIRect contentRect = {
                rect.x + 4.f,
                rect.y + 80.f,
                rect.w - 8.f,
                rect.h - 88.f
            };

            if (NkUI::BeginScrollRegion(ctx, ls, "console_scroll", contentRect, nullptr, &scrollY)) {
                for (nk_usize i = 0; i < mLines.Size(); ++i) {
                    const auto& line = mLines[i];

                    // Filtre niveau
                    switch (line.level) {
                        case NkLogLevel::NK_INFO:     if (!mShowInfo)  continue; break;
                        case NkLogLevel::NK_WARN:  if (!mShowWarn)  continue; break;
                        case NkLogLevel::NK_ERROR:
                        case NkLogLevel::NK_CRITICAL: if (!mShowError) continue; break;
                        case NkLogLevel::NK_DEBUG:
                        case NkLogLevel::NK_TRACE:    if (!mShowDebug) continue; break;
                        default: break;
                    }

                    // Filtre texte
                    if (mFilterBuf[0] != '\0') {
                        if (!line.text.Contains(mFilterBuf)) continue;
                    }

                    // Ligne
                    char prefix[8]; snprintf(prefix, sizeof(prefix), "%s", LevelPrefix(line.level));
                    NkString display = NkString(prefix) + line.text;
                    if (line.count > 1) {
                        char cnt[16]; snprintf(cnt, sizeof(cnt), " (%u)", line.count);
                        display += NkString(cnt);
                    }

                    NkUI::TextColored(ctx, ls, dl, font,
                                      LevelColor(line.level),
                                      display.CStr());
                }

                // Auto-scroll vers le bas
                if (mScrollToEnd) {
                    // TODO : forcer scroll à max via NkUILayoutStack cursor
                    mScrollToEnd = false;
                }

                NkUI::EndScrollRegion(ctx, ls);
            }

            // ── Auto-scroll toggle ────────────────────────────────────────────
            NkUI::BeginRow(ctx, ls, 18.f);
            NkUI::Checkbox(ctx, ls, dl, font, "Auto-scroll", mAutoScroll);
            NkUI::EndRow(ctx, ls);

            NkUIWindow::End(ctx, wm, dl, ls);
        }

    } // namespace Unkeny
} // namespace nkentseu
