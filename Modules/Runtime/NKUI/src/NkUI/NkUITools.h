// -----------------------------------------------------------------------------
// @File    NkUITools.h
// @Brief   Point d'entrée agrégateur public pour le sous-système des outils NkUI.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis
// @License Apache-2.0
//
// Ce fichier regroupe les en-têtes des modules d'outils :
//   - Gizmo 3D (manipulation de transformations)
//   - Arbre (TreeView)
//   - Navigateur de fichiers (FileBrowser)
// -----------------------------------------------------------------------------

#pragma once

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Public aggregation header for NKUI Tools subsystem.
 * Main data: Tool APIs for Gizmo/Grid, TreeView and FileBrowser.
 * Change this file when: New tool modules are added/removed from NKUI.
 */

// ============================================================
// Inclusions des modules d'outils
// ============================================================

// Gizmo 3D — manipulation interactive de transformations (translation, rotation, échelle)
#include "NKUI/Tools/Gizmo/NkUIGizmo.h"

// Arbre (TreeView) — affichage hiérarchique de données
#include "NKUI/Tools/Tree/NkUITree.h"

// Navigateur de fichiers — exploration du système de fichiers
#include "NKUI/Tools/FileSystem/NkUIFileBrowser.h"

// ============================================================
// Fin de NkUITools.h
// ============================================================