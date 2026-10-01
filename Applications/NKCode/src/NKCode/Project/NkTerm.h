#pragma once
// =============================================================================
// NkTerm.h — COUCHE MINCE (2026-10-01). L'emulateur VT de NKCode est DESCENDU
//   dans le kit : NKEditorKit/Terminal/NkTerminalEmulateur.h. Ce fichier garde
//   les noms historiques (`nkcode::NkTerm`, `nkcode::NkTermCell`).
//
//   ⚠️ UNE CELLULE PORTE DESORMAIS UN INDICE DE COULEUR, plus un RGB : la
//      couleur se resout AU DESSIN contre la palette du theme
//      (NkTerminalVue.h, NkTerminalResoudre). Le terminal de NKCode dessine
//      donc sa grille par le kit, comme UnkenyEditor.
// =============================================================================
#include "NKEditorKit/Terminal/NkTerminalEmulateur.h"

namespace nkentseu {
	namespace nkcode {
		using editorkit::NkTerm;
		using editorkit::NkTermCell;
	} // namespace nkcode
} // namespace nkentseu
