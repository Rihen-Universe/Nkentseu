#pragma once
// -----------------------------------------------------------------------------
// @File    NkEditorKit.h
// @Brief   En-tete parapluie de NKEditorKit — inclut toute l'API publique.
// @Author  Rihen
// @License Proprietary - Free to use and modify
//
// NKEditorKit : socle d'editeur partage de Nkentseu (coquille dockable, panneaux,
// commandes/palette, themes), construit PAR-DESSUS le docking complet de NKUI.
// Consomme par NKCode (IDE) aujourd'hui ; destine a accueillir Nogee (editeur de
// moteur) plus tard. 2D pur (NKCanvas/NKUI) : aucune dependance NKRenderer.
//
//   #include "NKEditorKit/NkEditorKit.h"
//   using namespace nkentseu::editorkit;
//
//   class MyPanel : public NkEditorPanel { ... void OnUI(NkEditorFrameContext&) ... };
//
//   NkEditorShell shell;
//   shell.Init({ "Mon Editeur", 1280, 720 });
//   shell.AddPanel(&myPanel);
//   shell.RegisterCommand("Fichier: Nouveau", &OnNew);
//   return shell.Run();
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkEditorExport.h"
#include "NKEditorKit/NkEditorContext.h"
#include "NKEditorKit/NkEditorPanel.h"
#include "NKEditorKit/NkEditorCommand.h"
#include "NKEditorKit/NkEditorShell.h"
