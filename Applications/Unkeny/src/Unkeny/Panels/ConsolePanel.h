#pragma once
// =============================================================================
// Unkeny/Panels/ConsolePanel.h
// =============================================================================
// Panel console qui affiche les messages NkLogger en temps réel.
// Supporte filtres par niveau, recherche texte, copie, effacement.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKUI/NKUI.h"
#include "NKUI/NkUIWidgets.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKLogger/NkLogLevel.h"

namespace nkentseu {
    namespace Unkeny {

        struct NkConsoleLine {
            NkString     text;
            NkLogLevel   level  = NkLogLevel::NK_INFO;
            nk_uint32    count  = 1;     // répétitions consécutives
        };

        class ConsolePanel {
            public:
                static constexpr nk_uint32 kMaxLines = 2048;

                ConsolePanel() = default;

                // ── API de log (à appeler depuis NkLogger sink) ───────────────────
                void PushLine(const char* text, NkLogLevel level) noexcept;
                void Clear() noexcept;

                // ── Rendu ─────────────────────────────────────────────────────────
                void Render(nkui::NkUIContext& ctx,
                            nkui::NkUIWindowManager& wm,
                            nkui::NkUIDrawList& dl,
                            nkui::NkUIFont& font,
                            nkui::NkUILayoutStack& ls,
                            nkui::NkUIRect rect) noexcept;

            private:
                NkVector<NkConsoleLine> mLines;
                bool      mAutoScroll    = true;
                bool      mScrollToEnd   = false;

                // Filtres
                bool      mShowInfo      = true;
                bool      mShowWarn      = true;
                bool      mShowError     = true;
                bool      mShowDebug     = false;
                char      mFilterBuf[128]= {};

                nk_uint32 mErrorCount    = 0;
                nk_uint32 mWarnCount     = 0;

                static nkui::NkColor LevelColor(NkLogLevel lv) noexcept;
                static const char*   LevelPrefix(NkLogLevel lv) noexcept;
        };

    } // namespace Unkeny
} // namespace nkentseu
