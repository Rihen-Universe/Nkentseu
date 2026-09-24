// NKGui/NkGuiExport.h — Alias vers NkGuiApi.h (export/import NKGui)
#pragma once
#include "NKGui/NkGuiApi.h"

// =============================================================================
// ⚠️ X11 DEFINIT `None`, ET NKGUI L'EMPLOIE COMME ENUMERATEUR
//
// `X11/X.h:115` fait `#define None 0L`. NKGui declare `None` dans neuf
// enumerations (`NkGuiButtonFlags::None`, `NkGuiInteract::None`,
// `NkGuiEdge::None`...). Toute application Linux qui inclut NKWindow — lequel
// tire X11 par `NkEntry.h` et `NkSurface.h` — puis NKGui echouait sur
// « expected identifier ». NKGui n'avait donc JAMAIS compile sous Linux.
// Mesure du 2026-09-01, sous WSL2 Ubuntu 22.04.
//
// ⚠️ POURQUOI ICI ET PAS DANS `NkGuiTypes.h`. Premiere tentative : un
// `push_macro`/`pop_macro` autour de NkGuiTypes.h. Les neuf declarations
// passaient — puis `NkGuiContext.h`, qui l'inclut, employait
// `NkGuiButtonFlags::None` APRES la restitution. Une neutralisation limitee a un
// fichier ne protege pas ceux qui l'incluent : le probleme est de PORTEE. Ce
// fichier-ci est inclus par les 6 en-tetes de `NKGui/Core/` : c'est la porte.
//
// ⚠️ CE QUE CELA COUTE, ET A QUELLE CONDITION CELA CESSERAIT D'ETRE GRATUIT :
// une unite de compilation qui inclut NKGui perd le `None` de X11. Le backend
// XLib l'emploie (NkXLibDropTarget.h:190,341 ; NkXLibWindow.cpp:128,565,620,853)
// — mais AUCUN fichier XLib n'inclut NKGui (verifie avant d'ecrire ceci). Le
// jour ou l'un d'eux le ferait, il devrait ecrire `0L` au lieu de `None`.
//
// ⚠️ CORRECTION DU 2026-09-24 — LE `#undef` SEUL NE TENAIT QUE SI X11 VENAIT
// AVANT. Ce commentaire conseillait aussi « inclure X11 APRES NKGui » : c'est
// exactement ce qui recassait. NkEditorShell.cpp inclut NKGui (par
// NkEditorShell.h) PUIS NKEvent, qui tire X11 : `None` etait REDEFINI apres
// l'effacement -> « expected unqualified-id » a NkEditorShell.cpp:2885, et
// NKCode ne compilait plus sous Linux ni macOS (mesure : build-remote rouge,
// puis WSL2). On inclut donc X.h ICI, AVANT d'effacer : sa garde d'inclusion
// rend toute inclusion ulterieure inerte, quel que soit l'ordre des #include.
// X.h ne contient que des constantes (aucune fonction, aucun lien).
//
// On ne renomme pas les enumerateurs : ils sont `enum class`, donc scoped, et ne
// polluent rien. Les renommer ferait payer a NKGui et a tous ses appelants le
// prix d'une macro tierce, et voyagerait dans chaque appel.
// =============================================================================
#if defined(__linux__) && !defined(__ANDROID__) && defined(__has_include)
#if __has_include(<X11/X.h>)
#include <X11/X.h>
#endif
#endif
#if defined(None)
#undef None
#endif
