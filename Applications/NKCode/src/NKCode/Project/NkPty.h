#pragma once
// =============================================================================
// NkPty.h — COUCHE MINCE (2026-10-01). Le pseudo-terminal de NKCode est
//   DESCENDU dans le kit : NKEditorKit/Terminal/NkTerminalPty.h, ou il sert
//   toute la famille d'editeurs (UnkenyEditor d'abord). Ce fichier ne garde que
//   le nom historique, pour que `nkcode::NkPty` continue de designer LE moteur.
//
//   Ce qui a change en descendant (voir l'en-tete du kit) : le shell n'herite
//   plus des sorties redirigees de NKCode (terminal vide quand NKCode etait
//   lance depuis une console), sa descendance meurt avec lui (Job), Running()
//   voit un `exit`, et le moteur POSIX lit sans bloquer.
// =============================================================================
#include "NKEditorKit/Terminal/NkTerminalPty.h"

namespace nkentseu {
	namespace nkcode {
		using editorkit::NkPty;
	} // namespace nkcode
} // namespace nkentseu
