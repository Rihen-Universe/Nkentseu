// =============================================================================
// Nkentseu/Modeling/NkUndoStack.cpp
// =============================================================================
// [AJOUT 2026-07-24] NkUndoStack.h n'avait AUCUN .cpp (spec à 0 % d'implém.,
// comme Design/Doc). Ce fichier n'implémente QUE ce qui est strictement
// nécessaire au chemin d'import SVG minimal (NkSVGIO::Import) : la
// destruction propre d'un `NkVectorDocument` (qui possède un `NkUndoStack
// undoStack{100};` PAR VALEUR) appelle `~NkUndoStack()`, qui appelle
// `Clear()` -- déclarée dans le header mais sans corps -- ce qui aurait été
// une erreur de lien dès qu'un `NkVectorDocument` sort de portée.
//
// Volontairement NON implémenté ici (déclarés dans NkUndoStack.h, aucun
// corps -- stubs honnêtes, PAS appelés par le chemin d'import donc pas
// d'erreur de lien) : Execute/Undo/Redo/BeginGroup/EndGroup/TrimHistory/
// PushCommand/MemoryUsed -- le système undo/redo complet est hors scope de
// cet incrément (dédié à l'import SVG, pas à l'édition). Un futur incrément
// dédié à l'undo/redo doit les implémenter.
// =============================================================================
#include "NkUndoStack.h"

namespace nkentseu {

	void NkUndoStack::Clear() noexcept {
		for (nk_usize i = 0; i < mHistory.Size(); ++i)
			delete mHistory[i];
		mHistory.Clear();
		mCurrent = -1;
		mCleanIndex = -1;
		if (mCurrentGroup) {
			delete mCurrentGroup;
			mCurrentGroup = nullptr;
		}
	}

} // namespace nkentseu
