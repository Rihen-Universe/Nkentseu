#pragma once
// =============================================================================
// Noge/Panels/ConsolePanel.h
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
#include "Nogee/Panels/Model/NkConsoleModel.h" // NkConsoleLine + etat/logique neutres

namespace nkentseu {
	namespace noge {

		// Le modele (lignes, filtres, compteurs, PushLine/Clear/Passes/LevelPrefix)
		// vit dans NkConsoleModel, en-tete NEUTRE sans dependance d'interface —
		// PARTAGE avec ConsolePanelGui, la version portee sur NKGui. Seule la
		// COULEUR d'un niveau reste ici : `nkui::NkColor` et `nkgui::NkColor` sont
		// deux types distincts, sans conversion possible.
		class ConsolePanel : public NkConsoleModel {
			public:
				ConsolePanel() = default;

				// PushLine / Clear / Passes / LevelPrefix : hérités de NkConsoleModel.

				// ── Rendu ─────────────────────────────────────────────────────────
				void Render(nkui::NkUIContext &ctx, nkui::NkUIWindowManager &wm, nkui::NkUIDrawList &dl,
							nkui::NkUIFont &font, nkui::NkUILayoutStack &ls, nkui::NkRect rect) noexcept;

			private:
				static nkui::NkColor LevelColor(NkLogLevel lv) noexcept;
		};

	} // namespace noge
} // namespace nkentseu
