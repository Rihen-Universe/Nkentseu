#pragma once
// =============================================================================
// Noge/Panels/ConsolePanelGui.h
// =============================================================================
// PORTAGE PILOTE (2026-08-17) — meme console que Panels/ConsolePanel.h, ecrite
// sur NKGui/NKEditorKit au lieu de NKUI.
//
// POURQUOI DEUX FICHIERS, ET POURQUOI L'ANCIEN RESTE :
//   Nogee peint aujourd'hui sur NKUI (UILayer + 4 panneaux). La coque d'editeur
//   NkEditorShell, elle, ne consomme que des draw-lists NKGui
//   (NkIEditorRenderer::SubmitDrawList prend un nkgui::NkGuiDrawList, et rien
//   d'autre). Un corps de panneau NKUI ne peut donc PAS s'afficher dans un dock
//   NKGui — cf. ROADMAP Noge §9quater.
//   Ce fichier est le PREMIER des quatre panneaux porte, livre SEUL : le chemin
//   NKUI reste vivant et intact tant que les trois autres ne sont pas portes.
//
// CE QUI CHANGE PAR RAPPORT A LA VERSION NKUI :
//   - le panneau derive de editorkit::NkEditorPanel et implemente OnUI(ec) ;
//     il ne cree plus sa fenetre (SetNextWindowPos/Begin/End) — le shell la
//     possede, l'ancre et la decore.
//   - la disposition passe par le curseur du contexte (SameLine/NextItemRect)
//     au lieu de BeginRow/EndRow/SameLine(ctx, ls) de NKUI.
//   - le texte colore passe par nkgui::TextAt(ctx, pos, s, col) : NKGui n'a pas
//     de TextColored, contrairement a NKUI.
//
// CE QUI NE CHANGE PAS : le modele de donnees et la logique de log (PushLine,
// fusion des repetitions, compteurs, filtres). C'est deliberement recopie plutot
// qu'inclus depuis ConsolePanel.h, parce que cet en-tete tire NKUI/NKUI.h : le
// mutualiser demanderait d'extraire le modele dans un fichier neutre, ce qui
// touche le chemin NKUI vivant. A faire quand les quatre panneaux seront portes.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKEditorKit/NkEditorPanel.h"
#include "NKEditorKit/NkEditorContext.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKLogger/NkLogLevel.h"

namespace nkentseu {
	namespace noge {

		struct NkConsoleLineGui {
				NkString text;
				NkLogLevel level = NkLogLevel::NK_INFO;
				nk_uint32 count = 1; // repetitions consecutives
		};

		class ConsolePanelGui final : public editorkit::NkEditorPanel {
			public:
				static constexpr nk_uint32 kMaxLines = 2048;

				ConsolePanelGui() noexcept
					: editorkit::NkEditorPanel("Console", editorkit::NkEditorDockSide::NK_BOTTOM) {
				}

				// ── API de log (identique a la version NKUI) ──────────────────────
				void PushLine(const char *text, NkLogLevel level) noexcept;
				void Clear() noexcept;

				// ── Rendu : le shell appelle ceci entre Begin/End du dock ─────────
				void OnUI(editorkit::NkEditorFrameContext &ec) override;

			private:
				NkVector<NkConsoleLineGui> mLines;
				bool mAutoScroll = true;

				// Filtres
				bool mShowInfo = true;
				bool mShowWarn = true;
				bool mShowError = true;
				bool mShowDebug = false;
				char mFilterBuf[128] = {};

				nk_uint32 mErrorCount = 0;
				nk_uint32 mWarnCount = 0;

				static nkgui::NkColor LevelColor(NkLogLevel lv) noexcept;
				static const char *LevelPrefix(NkLogLevel lv) noexcept;
				bool Passes(const NkConsoleLineGui &line) const noexcept;
		};

	} // namespace noge
} // namespace nkentseu
