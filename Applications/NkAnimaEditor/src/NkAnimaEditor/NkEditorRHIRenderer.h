#pragma once
// =============================================================================
// NkEditorRHIRenderer.h — alias local vers l'implementation GENERALISEE.
//
// L'implementation de reference (validee a l'ecran ici-meme) a ete generalisee
// dans Integrations/NKGui/NkEditorRHIRenderer.h (2026-07-24) pour etre
// reutilisable par toute app moteur (Nogee, ...). Ce header ne fait que
// re-exposer le symbole sous le namespace historique nkanima ; le code de
// l'app (main.cpp, PreUI3D) reste inchange.
// =============================================================================

#include "NKGui/NkEditorRHIRenderer.h" // Integrations/NKGui (version generalisee)

namespace nkanima {

	using NkEditorRHIRenderer = nkentseu::nkgui::NkEditorRHIRenderer;

} // namespace nkanima
