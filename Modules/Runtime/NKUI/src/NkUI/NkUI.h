#pragma once

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Public NKUI umbrella API and module-level usage contract.
 * Main data: Top-level includes and integration notes.
 * Change this file when: You add/remove exposed NKUI subsystems or update public integration examples.
 */
/**
 * @File    NkUI.h
 * @Brief   Include unique NkUI — bibliothèque UI complète v1.0.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * ═══════════════════════════════════════════════════════════════════════════
 *  NkUI — système UI immédiat-mode, sans dépendances, offline + realtime
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * @Session 1 — Core
 *   NkUIExport    — types, couleurs RGBA, géométrie 2D
 *   NkUIInput     — abstraction input platform-agnostic (clavier, souris, touch)
 *   NkUITheme     — thèmes complets (couleurs, métriques, fonts, animations)
 *                   4 thèmes prédéfinis : Default, Dark, Minimal, HighContrast
 *   NkUIDrawList  — toutes les primitives : rect, cercle, ellipse, arc, bézier,
 *                   path SVG-like, image, texte, clip rect, widget helpers
 *   NkUIContext   — état global : IDs (FNV-1a), animations smooth, style stack,
 *                   shape overrides, layout cursor, 4 layers de DrawLists
 *   NkUIRenderer  — interface abstraite + NkUICPURenderer (offline, sans GPU)
 *
 * @Session 2 — Layout + Font + Widgets
 *   NkUILayout    — Row/Column/Grid/Group/Scroll/Split + contraintes flex
 *   NkUIFont      — police bitmap 6×10 embarquée (0 dépendance), atlas, mesure
 *   NkUIWidgets   — 27 widgets : Button, Checkbox, Toggle, Radio, SliderFloat/Int,
 *                   DragFloat/Int, InputText/Int/Float/Multiline, Combo, ListBox,
 *                   ProgressBar, ColorButton, TreeNode, Table, Text, Tooltip...
 *
 * @Session 3 — Fenêtres + Docking
 *   NkUIWindow    — fenêtres flottantes : titre, fermeture, minimisation,
 *                   maximisation, drag, resize 6 axes, scroll, collapse,
 *                   z-order, animation, BeginChild, SetNextPos/Size
 *   NkUIDock      — docking complet : arbre binaire split/leaf, drag & drop,
 *                   dropzones visuelles, tabs multiples, detach, splitters
 *
 * @Session 4 — MenuBar, Animations, ColorPicker, SaveLayout, OpenGL
 *   NkUIMenu      — MenuBar, Menu déroulant, MenuItem (raccourcis, checkmarks),
 *                   ContextMenu (clic droit), BeginPopup, BeginModal
 *   NkUIAnimation — 22 fonctions d'easing (Linear, Quad, Cubic, Elastic,
 *                   Bounce, Back...), NkUITween pool, effets : Shake, Pulse,
 *                   FadeIn/Out, SlideIn, Bounce
 *   NkUILayout2   — ColorPicker complet (carré SV, barre hue, barre alpha,
 *                   sliders RGBA, input hex), SaveLayout/LoadLayout JSON,
 *                   NkUIFontNKFont (intégration NKFont TTF/OTF/WOFF),
 *                   NkUIOpenGLRenderer (GPU OpenGL 3.3+ Core Profile)
 *
 * ═══════════════════════════════════════════════════════════════════════════
 *  Usage complet
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * @code
 *   #include "NKUI/NkUI.h"
 *   using namespace nkentseu;
 *
 *   // Initialisation
 *   NkUIContext ctx;        ctx.Init(1280, 720);
 *   ctx.SetTheme(NkUITheme::Dark());
 *   NkUIFontManager fonts;  fonts.Init();
 *   NkUIFont* font = fonts.Default();
 *   NkUIWindowManager wm;   wm.Init();
 *   NkUIDockManager dock;   dock.Init({0,25,1280,695});
 *   NkUIAnimator anim;
 *   NkUILayoutStack ls;
 *
 *   // Renderer (CPU offline ou OpenGL GPU)
 *   NkUICPURenderer renderer;   renderer.Init(1280, 720);
 *   // OU : NkUIOpenGLRenderer glr; glr.Init(1280, 720); // avec -DNKUI_WITH_OPENGL
 *
 *   float val=0.5f; char name[64]=""; bool open=true;
 *   NkColor color={80,160,255,255};
 *
 *   while(running) {
 *       // --- Input ---
 *       NkUIInputState input; // ... remplir depuis SDL/GLFW/Win32/etc. ...
 *       anim.Update(input.dt);
 *
 *       // --- Frame ---
 *       ctx.BeginFrame(input, 0.016f);
 *       wm.BeginFrame(ctx);
 *
 *       // Fond
 *       ctx.layers[0].AddRectFilled({0,0,1280,720}, ctx.theme.colors.bgPrimary);
 *
 *       // ── MenuBar ─────────────────────────────────────────────────────────
 *       if(NkUIMenu::BeginMenuBar(ctx, *ctx.dl, *font, {0,0,1280,25})) {
 *           if(NkUIMenu::BeginMenu(ctx, *ctx.dl, *font, "Fichier")) {
 *               if(NkUIMenu::MenuItem(ctx, *ctx.dl, *font, "Nouveau", "Ctrl+N")) {}
 *               NkUIMenu::Separator(ctx, *ctx.dl);
 *               if(NkUIMenu::MenuItem(ctx, *ctx.dl, *font, "Quitter")) running=false;
 *               NkUIMenu::EndMenu(ctx);
 *           }
 *           NkUIMenu::EndMenuBar(ctx);
 *       }
 *
 *       // ── Fenêtres ─────────────────────────────────────────────────────────
 *       if(NkUIWindow::Begin(ctx, wm, ls, *font, "Propriétés", &open,
 *                             NkUIWindowFlags::Dockable)) {
 *           NkUI::Text(ctx, ls, *ctx.dl, *font, "Couleur :");
 *           NkUIColorPickerFull::Draw(ctx, *ctx.dl, *font, ls, "##col", color);
 *           NkUI::Separator(ctx, ls, *ctx.dl);
 *           NkUI::SliderFloat(ctx, ls, *ctx.dl, *font, "Valeur", val, 0, 1);
 *           NkUI::InputText(ctx, ls, *ctx.dl, *font, "Nom", name, sizeof(name));
 *       }
 *       NkUIWindow::End(ctx, wm, ls);
 *
 *       // ── Dock ─────────────────────────────────────────────────────────────
 *       dock.Update(ctx, wm);
 *
 *       ctx.EndFrame();
 *       wm.EndFrame(ctx);
 *
 *       // ── Rendu ────────────────────────────────────────────────────────────
 *       renderer.BeginFrame(1280, 720);
 *       renderer.Submit(ctx);
 *       renderer.EndFrame();
 *       // renderer.GetPixels() → RGBA32 1280×720
 *
 *       // ── Sauvegarde layout ─────────────────────────────────────────────────
 *       // NkUILayoutSerializer::Save(wm, dock, "layout.json");
 *   }
 * @endcode
 *
 * ═══════════════════════════════════════════════════════════════════════════
 *  ShapeOverride — formes entièrement personnalisées
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * @code
 *   // Bouton hexagonal — remplace le fond par défaut, garde l'interaction
 *   ctx.SetShapeOverride(NkUIWidgetType::Button,
 *     [](NkUIShapeOverrideCtx& s) {
 *       const NkVec2 c = {s.rect.x + s.rect.w*0.5f, s.rect.y + s.rect.h*0.5f};
 *       const float32 r = s.rect.h * 0.5f;
 *       NkVec2 pts[6];
 *       for(int i=0;i<6;++i) {
 *           pts[i] = {c.x + r*cosf(i*3.14159f/3),
 *                     c.y + r*sinf(i*3.14159f/3)};
 *       }
 *       const bool hov = HasState(s.state, NkUIWidgetState::Hovered);
 *       s.dl->AddConvexPolyFilled(pts, 6,
 *           hov ? s.theme->colors.accentHover : s.theme->colors.accent);
 *   });
 * @endcode
 *
 * ═══════════════════════════════════════════════════════════════════════════
 *  Compilation
 * ═══════════════════════════════════════════════════════════════════════════
 *
 *  Sources à compiler :
 *    NkUIContext.cpp  NkUIDrawList.cpp  NkUIFont.cpp     NkUILayout.cpp
 *    NkUIRenderer.cpp NkUIWidgets.cpp  NkUIWindow.cpp   NkUIDock.cpp
 *    NkUIMenu.cpp     NkUIAnimation.cpp NkUILayout2.cpp NkUIMath.cpp
 *
 *  Options :
 *    -DNKUI_WITH_OPENGL   Active NkUIOpenGLRenderer (nécessite OpenGL 3.3+)
 *    -DNKUI_WITH_NKFONT   Active NkUIFontNKFont (nécessite NKFont)
 *    -DNKUI_BUILD_DLL     Export DLL (Windows)
 *    -DNKUI_USE_DLL       Import DLL (Windows)
 */

// ── Session 1 — Core ─────────────────────────────────────────────────────────
#include "NKUI/NkUIExport.h"
#include "NKUI/NkUIInput.h"
#include "NKUI/NkUITheme.h"
#include "NKUI/NkUIDrawList.h"
#include "NKUI/NkUIContext.h"
#include "NKUI/NkUIRenderer.h"

// ── Session 2 — Layout + Font + Widgets ──────────────────────────────────────
#include "NKUI/NkUILayout.h"
#include "NKUI/NkUIFont.h"
#include "NKUI/NkUIWidgets.h"

// ── Session 3 — Fenêtres + Docking ───────────────────────────────────────────
#include "NKUI/NkUIWindow.h"
#include "NKUI/NkUIDock.h"

// ── Session 4 — MenuBar, Animations, ColorPicker, SaveLayout, OpenGL ─────────
#include "NKUI/NkUIMenu.h"
#include "NKUI/NkUIAnimation.h"
#include "NKUI/NkUILayout2.h"
