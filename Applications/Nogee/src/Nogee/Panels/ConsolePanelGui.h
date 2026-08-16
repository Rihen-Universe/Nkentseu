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
// CE QUI NE CHANGE PAS : le modele de donnees et la logique de log. Il vit
// desormais dans `Model/NkConsoleModel.h`, en-tete NEUTRE (sans NKUI ni NKGui)
// dont HERITENT les deux panneaux — la version NKUI et celle-ci. Il n'y a donc
// qu'UNE verite pour la console.
//   ⚠️ La premiere version de ce fichier (2026-08-17, le meme jour) RECOPIAIT
//   ces ~35 lignes, parce que le modele etait alors piege dans `ConsolePanel.h`
//   qui inclut `NKUI/NKUI.h`. C'est cette duplication, payee une fois, qui a
//   fait extraire les quatre modeles AVANT de porter les trois panneaux
//   restants — plutot que de la payer trois fois de plus.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKEditorKit/NkEditorPanel.h"
#include "NKEditorKit/NkEditorContext.h"
#include "NKLogger/NkLogLevel.h"
#include "Nogee/Panels/Model/NkConsoleModel.h" // modele PARTAGE avec ConsolePanel

namespace nkentseu {
	namespace noge {

		class ConsolePanelGui final : public editorkit::NkEditorPanel, public NkConsoleModel {
			public:
				ConsolePanelGui() noexcept
					: editorkit::NkEditorPanel("Console", editorkit::NkEditorDockSide::NK_BOTTOM) {
				}

				// PushLine / Clear / Passes / LevelPrefix : herites de NkConsoleModel,
				// strictement les memes que ceux du panneau NKUI.

				// ── Rendu : le shell appelle ceci entre Begin/End du dock ─────────
				void OnUI(editorkit::NkEditorFrameContext &ec) override;

			private:
				// Seule la COULEUR reste locale : nkgui::NkColor et nkui::NkColor
				// sont deux types distincts, sans conversion.
				static nkgui::NkColor LevelColor(NkLogLevel lv) noexcept;
		};

	} // namespace noge
} // namespace nkentseu
