#pragma once
// -----------------------------------------------------------------------------
// @File    NKGui.h
// @Brief   En-tête parapluie de NKGui — framework UI nouvelle génération.
// @License Proprietary - Free to use and modify
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

#include "NKGui/NkGuiExport.h"
#include "NKGui/Core/NkGuiTypes.h"
#include "NKGui/Core/NkGuiInput.h"
#include "NKGui/Core/NkGuiDrawList.h"
#include "NKGui/Core/NkGuiFont.h"
#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Widgets/NkGuiWidgets.h"
