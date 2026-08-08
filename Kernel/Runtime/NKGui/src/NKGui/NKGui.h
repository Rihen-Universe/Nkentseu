#pragma once
// -----------------------------------------------------------------------------
// @File    NKGui.h
// @Brief   En-tête parapluie de NKGui — framework UI nouvelle génération.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// NKGui : réécriture complète de l'UI Nkentseu (noms neufs, zéro lien ImGui),
// deux paradigmes — immédiat ET retenu. Construit à partir de l'étalon
// Applications/ImGuiRef. Voir ARCHITECTURE.md + ROADMAP_UI_REWRITE.private.md.
//
//   #include "NKGui/NKGui.h"
//   using namespace nkentseu::nkgui;
//
// État : Phase 1 (squelette). Le cœur (DrawList/Context/Input/Interaction),
// les widgets, fenêtres, docking et le mode retenu arrivent aux phases 2→6.
// -----------------------------------------------------------------------------

// Retire les macros X11 (None, Bool, Status...) AVANT toute declaration de
// NKGui. Sur Linux, NKWindow tire <X11/Xlib.h>, dont le `#define None 0L`
// transforme ensuite `None = 0` — membre de plusieurs enumerations de NkGuiTypes
// — en `0L = 0`. Le compilateur signale alors « expected identifier » sur des
// lignes parfaitement correctes, tres loin de la vraie cause.
//
// Ce point d'insertion est deliberé : tout ce qui inclut NKGui obtient des
// macros propres, tandis que le code de fenetrage X11 — qui n'inclut PAS NKGui
// — conserve les constantes dont il a besoin.
#include "NKPlatform/NkX11Clean.h"

#include "NKGui/NkGuiExport.h"
#include "NKGui/Core/NkGuiTypes.h"
#include "NKGui/Core/NkGuiInput.h"
#include "NKGui/Core/NkGuiDrawList.h"
#include "NKGui/Core/NkGuiFont.h"
#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Widgets/NkGuiWidgets.h"
