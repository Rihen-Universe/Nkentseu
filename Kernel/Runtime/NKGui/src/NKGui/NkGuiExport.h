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
// jour ou l'un d'eux le ferait, il devrait ecrire `0L` au lieu de `None`, ou
// inclure X11 APRES NKGui.
//
// On ne renomme pas les enumerateurs : ils sont `enum class`, donc scoped, et ne
// polluent rien. Les renommer ferait payer a NKGui et a tous ses appelants le
// prix d'une macro tierce, et voyagerait dans chaque appel.
// =============================================================================
#if defined(None)
#undef None
#endif
