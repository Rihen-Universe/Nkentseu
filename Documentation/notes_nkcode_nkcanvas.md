# NKCode & NkCanvas — notes d'investigation (lecture de code)

> Document de travail destiné à la rédaction d'un cours pour débutants.
> Tout est vérifié dans les sources du dépôt `D:\Projets\2026\Nkentseu\Nkentseu`.
> Chaque affirmation est accompagnée de son `chemin:ligne`.
> Rappel de la règle du dépôt : **aucune STL** — `NkString`, `NkVector`, `math::Nk*`.

---

## 0. Le résultat le plus important, à connaître avant tout le reste

**NKCode n'appelle JAMAIS NkCanvas directement.**

Une recherche exhaustive des `#include "NKCanvas/..."` dans tout `Applications/NKCode/src/`
ne renvoie **aucun** résultat. NKCode inclut `NKGui/NKGui.h` (9 fichiers) et
`NKEditorKit/...` (22 fichiers), jamais NKCanvas.

NKCanvas apparaît quand même dans les dépendances de build :

```python
# D:\Projets\2026\Nkentseu\Nkentseu\Applications\NKCode\NKCode.jenga:82-85
nkentseudependson(
    ["NKEditorKit", "NKCanvas", "NKGui", "NKWindow", "NKEvent", "NKGlad",
     "NKFont", "NKImage", "NKAudio", "NKMedia", "NKFileSystem", "NKThreading",
     "NKLogger", "NKMath", "NKTime", "NKStream", "NKContainers", "NKMemory",
     "NKCore", "NKPlatform"],
```

… uniquement pour l'**édition de liens** : c'est `NKEditorKit` qui, lui, instancie
NkCanvas dans son backend de rendu par défaut.

La chaîne réelle est donc :

```
NKCode (panneaux, éditeur, explorateur…)      Applications/NKCode/src/NKCode/
   │  écrit des widgets et des primitives
   ▼
NKGui  (contexte immédiat + draw-lists)       Kernel/Runtime/NKGui/src/NKGui/
   │  produit UN NkGuiDrawList (vertices + indices + commandes)
   ▼
NKEditorKit (coquille : fenêtre, dock, frame) Engine/NKEditorKit/src/NKEditorKit/
   │  interface NkIEditorRenderer  ← point de branchement
   ▼
NkEditorCanvasRenderer  (impl. par DÉFAUT)    NkEditorCanvasRenderer.h
   │
   ├── renderer::NkRenderWindow  (NKCanvas : contexte GPU + cible fenêtre)
   └── renderer::NkGuiCanvasBackend (NKCanvas : draw-list NKGui → NkIRenderer2D)
   ▼
NkIRenderer2D → backend GPU (DX11 / DX12 / OpenGL / Vulkan / Metal / Software)
```

Autrement dit : **NkCanvas est le « pilote d'affichage » de NKCode, pas son API de dessin.**
L'API de dessin, du point de vue de NKCode, c'est NKGui.

Ce découplage est explicite dans le code :

```cpp
// Engine/NKEditorKit/src/NKEditorKit/NkIEditorRenderer.h:7-15
// NkEditorShell delegue TOUT le rendu (creation du contexte GPU, frame, soumission
// des draw-lists NKGui, upload des atlas de police/images) a un NkIEditorRenderer.
// Ainsi la coquille (menus / docking / palette / panneaux) est INDEPENDANTE du
// systeme de rendu :
//   - IDE (NKCode)            -> impl NKCanvas (NkEditorCanvasRenderer, defaut),
//   - app anim / moteur de jeu-> impl NKRHI/NKRenderer (fournie par l'app).
//
// L'interface n'expose QUE des types NKGui / NKWindow purs : NKEditorKit reste
// « 2D pur » (aucune dependance NKRHI ni NKRenderer ; l'impl NKRHI vit AILLEURS).
```

Et la preuve que le branchement est réellement utilisé ailleurs :

```
/* Integrations/NKGui/NkEditorRHIRenderer.h:8-13 */
* Generalisation (2026-07-24) de l'implementation de reference validee a
* l'ecran dans Applications/NkAnimaEditor : toute application moteur
* (NkAnimaEditor, Nogee, ...) qui veut la coquille NkEditorShell rendue sur
* NKRHI/NKRenderer (et PAS NKCanvas — regle « une fenetre = une pile »)
* injecte cette classe via NkEditorShellConfig::renderer.
```

**Règle à retenir : « une fenêtre = une pile ».** Une fenêtre est rendue soit par
NkCanvas, soit par NKRHI, jamais les deux.

---

## 1. Où vit NKCode et comment son point d'entrée est structuré

### 1.1 Arborescence

```
Applications/NKCode/
├── NKCode.jenga                     # description de build (Jenga)
├── data/                            # polices, textures, logos, traductions, icons.cfg
└── src/NKCode/
    ├── main.cpp                     (311 l.)  point d'entrée
    ├── Shell/                       la coquille applicative
    │   ├── NkUi.h            (690)  boîte à outils de dessin maison (tokens + helpers)
    │   ├── Panels.h         (3508)  Explorateur, Éditeur, Sortie, Terminal, Recherche
    │   ├── NkAiPanel.h      (4892)  panneaux IA
    │   ├── NkNewWorkspace.h (4438)  assistant « nouveau workspace »
    │   ├── NkI18n.h         (2832)  traductions
    │   ├── NkToolchains.h   (2671)
    │   ├── NkOpenWs.h       (2326)
    │   ├── Dialogs.h        (2182)  dialogues modaux + sélecteur de fichiers
    │   ├── NkExplorer.h     (1840)
    │   ├── NkHome.h         (1392)  écran d'accueil (launcher)
    │   ├── NkSettings.h     (1105)
    │   ├── NkMenuBar.h       (834)  barre de menus complète (11 menus)
    │   ├── Toolbar.h         (810)  barre d'outils façon Visual Studio
    │   ├── NkAppCommands.h   (475)  commandes + « thunks » branchés sur le shell
    │   ├── NkAppIcons.h      (429)  icônes/logos
    │   ├── NkAppFonts.h       (55)  polices de repli
    │   └── …
    ├── Editor/
    │   ├── NkCodeEditor.h   (4992)  éditeur de code (le gros morceau)
    │   ├── NkSyntax.h       (1422)  coloration syntaxique
    │   ├── NkTextDraw.h      (400)  primitives de dessin de texte
    │   ├── NkJsonView.h / NkCsvView.h / NkMarkdown.h / NkSyntaxLangs.h
    ├── Project/
    │   ├── NkCodeState.h    (7339)  ÉTAT global de l'IDE (le plus gros fichier)
    │   ├── NkTerm.h / NkText.h / NkLsp.cpp / NkPty.cpp / NkProcess.h …
    └── Pdf/                         lecteur PDF maison (.cpp, hors UI)
```

Remarque de style : NKCode est presque **entièrement header-only** — un seul
`.cpp` d'UI (`main.cpp`) ; tout le reste est en `.h` avec des fonctions `inline`.
Le `.jenga` ne compile que `files(["src/**.cpp"])`
(`Applications/NKCode/NKCode.jenga:67`).

### 1.2 Le point d'entrée

`main.cpp` **ne contient ni boucle principale, ni création de fenêtre, ni
initialisation de renderer**. Tout cela est dans `NkEditorShell`. `main` se contente
de :

```cpp
// Applications/NKCode/src/NKCode/main.cpp:55-104 (extraits)
int nkmain(const NkEntryState &state) {
    …
    auto shell = memory::NkMakeUnique<NkEditorShell>();
    NkEditorShellConfig cfg;
    cfg.title  = "NKCode - IDE (Jenga)";
    cfg.width  = 1440;   // grande fenetre centree, REDIMENSIONNABLE (pas maximisee de force)
    cfg.height = 900;
    if (!shell || !shell->Init(cfg))
        return -1;
    …
    static nkcode::ExplorerPanel explorer(&g_state, shell.Get());
    static nkcode::OutlinePanel  outline(&g_state);
    static nkcode::EditorPanel   editor(&g_state, shell.Get());
    static nkcode::OutputPanel   output(&g_state);
    static nkcode::TerminalPanel terminal;
    shell->AddPanel(&explorer);
    shell->AddPanel(&outline);
    shell->AddPanel(&editor);
    shell->AddPanel(&output);
    shell->AddPanel(&terminal);
```

puis d'accrocher des **callbacks (« thunks »)** à des points d'extension du shell :

| appel | rôle |
|---|---|
| `shell->SetActivityHandler(&ActivityThunk, shell.Get())` | clic sur la barre d'activité (sidebars exclusives) — `main.cpp:145` |
| `shell->SetDropFilesHandler(…)` | glisser-déposer de fichiers depuis l'OS — `main.cpp:147` |
| `shell->SetToolbar(&ToolbarThunk, &g_state)` | barre d'outils — `main.cpp:156` |
| `shell->SetZoomHandler(&ZoomHandler, &NkZoomCtx())` | zoom Ctrl+molette routé vers l'onglet actif — `main.cpp:158` |
| `shell->SetAppMenu(&AppFlagsThunk, &g_home)` | code exécuté chaque frame (pose `appFullScreen`/`appModal`) — `main.cpp:165` |
| `shell->SetMenuBar(&MainMenuBarThunk, &g_menuBar)` | remplace TOUTE la barre de menus — `main.cpp:174` |
| `shell->SetOverlay(&OverlayThunk, &g_dialogs)` | dialogues modaux, dessinés après les panneaux — `main.cpp:175` |
| `shell->SetStartScreen(&StartScreenThunk, &g_home)` | écran d'accueil plein cadre — `main.cpp:281` |
| `shell->RegisterCommand(nom, fn, user, raccourci)` | palette de commandes + raccourci — `main.cpp:287-298` |

et enfin :

```cpp
// Applications/NKCode/src/NKCode/main.cpp:300-310
const int rc = shell->Run();
// Sauvegarde l'etat d'interface : par WORKSPACE si un projet est ouvert (dock +
// geometrie fenetre + moniteur), sinon la geometrie GLOBALE du launcher.
if (!g_dialogs.showStart && g_state.HasWorkspace())
    shell->SaveUiState(g_state.UiConfigPath().CStr());
else
    shell->SaveWindowGeom(g_launcherGeom.CStr());
nkcode::NkEmbeddedJenga::Shutdown();
return rc;
```

Point à souligner pour un débutant : `nkmain` n'est pas `main`. Le vrai `main`
plateforme est fourni par `NKWindow/NKMain.h` (inclus en `main.cpp:8`), et
`NKENTSEU_DEFINE_APP_DATA` (`main.cpp:33-38`) déclare le nom/version de l'app.

### 1.3 Ce que fait `main` AVANT de créer la fenêtre (ordre imposé)

```cpp
// Applications/NKCode/src/NKCode/main.cpp:58-74
// ── Dossier de l'EXECUTABLE, calcule EN PREMIER ──────────────────────────
// Demande a l'OS (GetModuleFileNameW / /proc/self/exe / _NSGetExecutablePath),
// PAS deduit de argv[0] : argv[0] n'a pas de dossier quand l'exe est lance
// via le PATH, et est relatif quand il est lance depuis un autre dossier.
// Doit etre pose AVANT le chargement des polices/icones (qui cherchent
// aussi a cote de l'exe) et avant NkEmbeddedJenga::Configure.
{
    NkString exeDir = NkPath::GetExecutableDirectory().ToString();
    …
}
nkcode::InstallLogSink();        // capture les logs NKLogger -> panneau OUTPUT
nkcode::NkSynInitDefaultLangs(); // coloration data-driven (CSS, JS, Lua, Rust…)
nkcode::NkLoadFallbackFonts();   // polices de repli (broad/CJK/emoji)
```

C'est un **invariant d'ordre** : les polices de repli doivent être déclarées avant
que `shell->Init()` ne charge et rasterise les polices.

---

## 2. Le shell : création de fenêtre, du contexte GPU et de la boucle

### 2.1 `NkEditorShell::Init` — ce qui est réellement construit

`Engine/NKEditorKit/src/NKEditorKit/NkEditorShell.cpp:121-251`

```cpp
bool NkEditorShell::Init(const NkEditorShellConfig &config) noexcept {
    mGraphicsApi = config.graphicsApi;

    NkWindowConfig wc;
    wc.title = config.title;
    wc.width = config.width;   wc.height = config.height;
    wc.minWidth = 1024;        // taille MINI de l'IDE (sidebar + panneau lisibles)
    wc.minHeight = 640;
    wc.centered = true;
    wc.resizable = config.resizable;
    wc.frame = false;          // SANS bordure OS -> barre de titre custom (VSCode)
    if (!mWindow.Create(wc)) return false;
    …
    // Backend de rendu : injecte (app NKRHI/NKRenderer) ou NKCanvas par
    // defaut (IDE). Resolution AUTO->API faite par l'impl elle-meme.
    if (config.renderer) { mRenderer = config.renderer; mOwnsRenderer = false; }
    else { mRenderer = memory::NkGetDefaultAllocator().New<NkEditorCanvasRenderer>();
           mOwnsRenderer = true; }
    if (!mRenderer || !mRenderer->Init(mWindow, mGraphicsApi)) { … return false; }

    mUI.Init((int32)config.width, (int32)config.height);   // contexte NKGui
    …
}
```

Détails notables :
* `wc.frame = false` → **pas de décoration OS**. La barre de titre, les boutons
  min/max/close et le redimensionnement par les bords sont dessinés/gérés par le shell
  (`DrawTitleBar` ligne 934, `HandleEdgeResize` ligne 1109).
* Le thème GitHub Dark est écrit à la main dans `Init` (`NkEditorShell.cpp:155-179`),
  puis `NkLoadTheme(mUI.theme)` applique le thème utilisateur sauvegardé s'il existe.
* Le presse-papiers NKGui est branché sur la fenêtre OS via deux pointeurs de
  fonction (`NkEditorShell.cpp:185-190`) — NKGui reste ignorant de NKWindow.
* Deux polices distinctes, atlas séparés :
  ```cpp
  // NkEditorShell.cpp:228-234
  // ── DEUX polices distinctes (comme VSCode), pilotees par les reglages ──
  // INTERFACE (proportionnelle, defaut Inter) + CODE/TERMINAL (monospace,
  // defaut DejaVu Sans Mono). Atlas SEPARES => texId distincts.
  mCodeFont.texId = mFont.TexId() + 1u; // atlas distinct (anti-collision backend)
  ```

### 2.2 Choix de l'API graphique (le seul endroit où NkCanvas est nommé)

`Engine/NKEditorKit/src/NKEditorKit/NkEditorCanvasRenderer.h:25-57`

```cpp
bool Init(NkWindow &window, NkEditorGfxApi api) override {
    NkContextDesc desc;
    switch (api) {
        case NkEditorGfxApi::OpenGL:   desc.api = NkGraphicsApi::NK_GFX_API_OPENGL;   break;
        case NkEditorGfxApi::Vulkan:   desc.api = NkGraphicsApi::NK_GFX_API_VULKAN;   break;
        case NkEditorGfxApi::DX11:     desc.api = NkGraphicsApi::NK_GFX_API_DX11;     break;
        case NkEditorGfxApi::DX12:     desc.api = NkGraphicsApi::NK_GFX_API_DX12;     break;
        case NkEditorGfxApi::Software: desc.api = NkGraphicsApi::NK_GFX_API_SOFTWARE; break;
        default:
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            desc.api = NkGraphicsApi::NK_GFX_API_DX11;
#else
            desc.api = NkGraphicsApi::NK_GFX_API_OPENGL;
#endif
            break;
    }
    mTarget = memory::NkMakeUnique<renderer::NkRenderWindow>(window, desc);
    if (!mTarget || !mTarget->IsValid()) { mTarget.Reset(); return false; }
    return mBackend.Init(mTarget->GetRenderer());
}
```

NKCode laisse `graphicsApi = NkEditorGfxApi::Auto` (valeur par défaut de
`NkEditorShellConfig`, `NkEditorShell.h:43`) → **DX11 sur Windows, OpenGL ailleurs**.

Un `NkEditorGfxApi` **neutre** est introduit exprès :

```cpp
// Engine/NKEditorKit/src/NKEditorKit/NkIEditorRenderer.h:28-31
// Choix d'API graphique NEUTRE (decouple de NKCANVAS ET NKRHI : leurs enums
// NkGraphicsApi se dupliquent dans le namespace nkentseu et ne peuvent
// cohabiter dans un meme TU). Chaque impl mappe vers son propre enum.
enum class NkEditorGfxApi : uint8 { Auto = 0, OpenGL, Vulkan, DX11, DX12, Software };
```

### 2.3 La boucle principale

`Engine/NKEditorKit/src/NKEditorKit/NkEditorShell.cpp:539-561`

```cpp
int NkEditorShell::Run() noexcept {
    NkEvents().SetSizeMoveFrameCallback(&SizeMoveFrameThunk, this); // anti-stretch pendant le resize natif
    while (mRunning && mWindow.IsOpen()) {
        while (NkEvent *ev = NkEvents().PollEvent()) { (void)ev; }
        if (!mRunning) break;
        RenderFrame();
        // Hand-off NATIF differe : BeginResize/BeginDragMove BLOQUENT (boucle modale
        // OS) ; on les lance ICI (hors frame) pour eviter la re-entrance de RenderFrame.
        // Pendant la boucle modale, le callback ci-dessus rappelle RenderFrame -> rendu live.
        if (mPendingDragMove) { mPendingDragMove = false; mWindow.BeginDragMove(); }
        else if (mPendingResizeEdge >= 0) {
            const NkWindow::NkResizeEdge e = (NkWindow::NkResizeEdge)mPendingResizeEdge;
            mPendingResizeEdge = -1;
            mWindow.BeginResize(e);
        }
    }
    return 0;
}
```

À noter : `PollEvent()` est **vidé sans regarder le résultat** — les événements ont
déjà été routés vers les callbacks enregistrés par `HookEvents()` (§5.1).

---

## 3. Le cycle de vie d'une image (la séquence d'appels réelle)

Tout se passe dans `NkEditorShell::RenderFrame()`
(`Engine/NKEditorKit/src/NKEditorKit/NkEditorShell.cpp:563-733`).

### 3.1 Séquence complète, dans l'ordre

```
Run()
└── RenderFrame()
    1.  dt = mClock.Tick().delta ; clampé à [1/60 (si 0), 0.1]              :564-568
    2.  si la taille de la fenêtre OS a changé -> mRenderer->OnResize(w,h)  :571-576
    3.  cache de la géométrie fenêtre (max/pos/taille) pour la sauvegarde   :577-589
    4.  mUI.viewW / mUI.viewH  <-  mRenderer->Size()                        :590-594
    5.  RECHARGEMENT DE POLICE DIFFÉRÉ (avant BeginFrame, jamais pendant)   :596-621
        - mFontReloadPending -> LoadFontsFromPrefs()
        - sinon décompte du debounce -> BuildCodeSlot(px) / LoadTermFont()
    6.  mUI.BeginFrame(dt)                                                  :623
    7.  mUI.dl.AddRectFilled({0,0,W,H}, theme.bgPrimary)   // fond          :627
    8.  NkEditorFrameContext ec { .ui = &mUI, .dt = dt }                    :629-631
    9.  mAppMenuFn(ec, user)          // NKCode: AppFlagsThunk              :638-639
    10. fullScreen = mUI.appFullScreen && mStartScreenFn                    :643
    11. calcul des hauteurs : titleH, toolbarH, footerH, activityW          :645-650
    12. DrawTitleBar(ec, {0,0,W,titleH})   -> inclut BuildMenuBar           :653
    13. DrawToolbar(ec, {0,titleH,W,toolbarH})  (si non fullScreen)         :655-656
    14. MASQUAGE MODAL de l'input (popup/Préférences/appModal/menu ctx.)    :667-688
    15. si fullScreen : mStartScreenFn(ec, user)      // écran d'accueil    :690-692
        sinon :
          DrawActivityBar(...)                                              :694
          DrawActivityBarRight(...)                                         :695
          DockSpace(mUI, "##EditorDock", {...})                             :696
          DockWindowHideSingleTab(...) pour chaque panneau                  :700-703
          BootstrapDocking()                                                :704
          DrawPanels(ec)   ->  Begin(...) / panel->OnUI(ec) / EndWindow()   :705
          DrawStatusBar(footerH)                                            :706
    16. HandleEdgeResize(W,H)   // bords de redimensionnement               :708
    17. restauration de l'input masqué                                      :710-712
    18. DrawContextMenu()          // menu contextuel shell-level           :713
    19. DrawCommandPalette(ec)     // Ctrl+P                                :716
    20. DrawPreferences(ec)                                                 :717
    21. mOverlayFn(ec, user)       // NKCode: OverlayThunk (modales)        :718-719
    22. bordure de fenêtre si non maximisée -> mUI.dlOverlay.AddRect(...)   :722-723
    23. mUI.EndFrame()             // fusion des draw-lists + nettoyage     :725
    24. mWindow.SetCursor(MapCursor(mUI.wantCursor))                        :727
    25. mRenderer->BeginFrame()                        -> NkCanvas          :729
    26. mRenderer->SubmitDrawList(mUI.dl,        sz.x, sz.y)                :730
    27. mRenderer->SubmitDrawList(mUI.dlOverlay, sz.x, sz.y)                :731
    28. mRenderer->EndFrame()                          -> NkCanvas          :732
```

Le code exact des 4 dernières lignes :

```cpp
// Engine/NKEditorKit/src/NKEditorKit/NkEditorShell.cpp:725-732
mUI.EndFrame();

mWindow.SetCursor(MapCursor(mUI.wantCursor));

mRenderer->BeginFrame();
mRenderer->SubmitDrawList(mUI.dl, sz.x, sz.y);
mRenderer->SubmitDrawList(mUI.dlOverlay, sz.x, sz.y);
mRenderer->EndFrame();
```

**Deux couches, toujours dans cet ordre** : `dl` (contenu), puis `dlOverlay`
(popups, modales, infobulles, bordure de fenêtre). C'est ce qui garantit que les
menus passent au-dessus des panneaux.

### 3.2 Ce que fait `NkGuiContext::BeginFrame`

`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiContext.cpp:38-73`

```cpp
void NkGuiContext::BeginFrame(float32 dt) noexcept {
    input.dt = dt;
    input.NewFrame();      // transitions clic/relâche
    time += dt;            // blink du caret
    hotIdPrev = hotId;     // le survol résolu de la frame précédente
    hotId = NKGUI_ID_NONE; // re-calculé par les widgets (greedy)
    interact = NkGuiInteract::None;
    lastItemHovered = false;
    wantCursor = NkGuiCursor::Arrow;
    idDepth = 0;  disabledDepth = 0;  inputClickConsumed = false;
    curPopupLevel = -1;    // le dessin reprend sur la couche principale
    // Routeur d'occlusion : la liste ecrite la frame PRECEDENTE devient la
    // liste LUE (stable toute la frame, comme hotIdPrev) ; on repart a zero
    // pour l'ecriture de cette frame.
    occlCount = occlCountNew;  … occlCountNew = 0;  curInputLayer = 0;
    winCount = 0;  curWindow = -1;  …
    dl.Reset();
    dlOverlay.Reset();
}
```

Deux mécanismes « une frame de retard » à comprendre absolument :
1. `hotIdPrev` — le survol effectif est celui **résolu à la frame précédente**.
2. `occlRects` / `occlLayers` — les rectangles d'occlusion lus sont ceux
   **déclarés à la frame précédente**. Comme ça, l'ordre de dessin des panneaux
   n'influence pas le résultat.

### 3.3 Ce que fait `NkGuiContext::EndFrame`

`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiContext.cpp:75-142`

1. **Tri des fenêtres par z-order (tri par insertion)** puis `dl.Append(winDL[order[i]])` :
   chaque fenêtre a dessiné dans SA propre draw-list ; elles sont concaténées à la fin,
   du plus bas au plus haut.
2. Calcul de `hoveredWindowId` pour la frame suivante.
3. Fermeture de la chaîne de popups (Échap = 1 niveau ; clic hors de tout = tout fermer).
4. **Garde anti-gel** :
   ```cpp
   // NkGuiContext.cpp:125-133
   // ANTI-GEL : aucun glissement légitime ne conserve activeId bouton RELÂCHÉ.
   // Si le widget détenant activeId a disparu (hôte redevenu flottant, onglet
   // caché, fenêtre fermée…) il ne libère jamais activeId et l'occlusion bloque
   // TOUTE interaction. Souris haute + activeId encore posé ⇒ on libère d'office.
   if (!input.mouseDown[0] && activeId != NKGUI_ID_NONE) { activeId = …NONE; movingWindowId = …NONE; }
   ```
5. Défocus du champ texte si un clic n'a été consommé par aucun champ.
6. `input.wheel = 0` ; `input.wheelH = 0` ; `input.ClearPerFrameText()`.

### 3.4 Ce que fait le côté NkCanvas (les 4 appels du §3.1)

```cpp
// Engine/NKEditorKit/src/NKEditorKit/NkEditorCanvasRenderer.h:76-96
void BeginFrame()  override { if (mTarget) mTarget->Clear(); }
void SubmitDrawList(const nkgui::NkGuiDrawList &dl, uint32 fbW, uint32 fbH) override {
    mBackend.Submit(dl, fbW, fbH);
}
void EndFrame()    override { if (mTarget) mTarget->Display(); }
bool UploadFontGray8(uint32 texId, const uint8 *px, int32 w, int32 h) override { … }
bool UploadImageRGBA(uint32 texId, const uint8 *px, int32 w, int32 h) override { … }
```

`NkRenderWindow::Clear` ouvre la frame du backend, `Display` la ferme et présente :

```cpp
// Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Targets/NkRenderWindow.cpp:145-171
void NkRenderWindow::Clear(const NkColor2D &color) {
    if (!mRenderer) return;
    // Pousser la couleur de clear au contexte AVANT Begin() : sur Vulkan/
    // Software, BeginFrame() ouvre le render pass / clear le back-buffer avec
    // cette couleur (sinon ils utilisent un gris en dur -> fond incorrect).
    // No-op sur OpenGL/DX (ils clearent via le renderer->Clear ci-dessous).
    if (mContext) mContext->SetClearColor(color.r/255.f, …);
    // Begin() ouvre la frame si pas deja ouverte (idempotent par convention
    // du backend) ; Clear() est appelable en milieu de frame.
    if (!mFrameOpen) { mRenderer->Begin(); mFrameOpen = true; }
    mRenderer->Clear(color);
}

void NkRenderWindow::Display() {
    if (mRenderer && mFrameOpen) { mRenderer->End(); mFrameOpen = false; }
    if (mContext) mContext->Present();
}
```

**Séquence de bas niveau résultante, par image :**

```
NkRenderWindow::Clear
   -> NkIGraphicsContext::SetClearColor
   -> NkIRenderer2D::Begin()        [NkBatchRenderer2D::Begin : vide vertices/indices/groupes,
                                     réinitialise la pile de clip, BeginBackend()]
   -> NkIRenderer2D::Clear(color)

pour chaque commande de la draw-list (NkGuiCanvasBackend::Submit) :
   -> NkIRenderer2D::SetClip(rect)  [Flush() du batch en cours + push d'un clip intersecté]
   -> NkIRenderer2D::DrawVertices(vertices, n, indices, m, texture)
   -> NkIRenderer2D::PopClip()      [Flush() + dépile]

NkRenderWindow::Display
   -> NkIRenderer2D::End()          [Flush final + EndBackend()]
   -> NkIGraphicsContext::Present()
```

---

## 4. NkCanvas : ce que c'est exactement

### 4.1 Positionnement

```
/* Guides/05-NKCanvas.md:11-19 */
**NKCanvas** est la couche de **rendu 2D conviviale** de Nkentseu. C'est l'équivalent
de la partie *Graphics* de **SFML** : … multi-backend … Zero-STL.
```

Modules : `Kernel/Runtime/NKCanvas/src/NKCanvas/`

| dossier | contenu |
|---|---|
| `Core/` | `NkIGraphicsContext.h`, `NkContextDesc.h`, `NkGraphicsApi.h`, `NkSurfaceDesc.h`, `NkGpuPolicy` |
| `Factory/` | `NkContextFactory` (crée le contexte GPU selon l'API, avec fallback) |
| `Backend/` | `OpenGL/`, `Vulkan/`, `DirectX/` (DX11+DX12), `Metal/`, `Software/` |
| `Renderer/Core/` | `NkIRenderer2D.h` (interface), `NkRenderer2D.h` (façade), `NkRenderer2DTypes.h`, `NkVertexArray.h`, `NkTransform(able).h`, `NkRenderStates.h`, `NkDrawable.h` |
| `Renderer/Batch/` | `NkBatchRenderer2D` — la base commune de tous les backends |
| `Renderer/Resources/` | `NkTexture`, `NkSprite` (+ `NkText`), `NkFont`, `NkShader`, `NkMaterial` |
| `Renderer/Shapes/` | `NkRectangleShape`, `NkCircleShape`, `NkConvexShape`, `NkLineShape` |
| `Renderer/Targets/` | `NkRenderTarget`, `NkRenderWindow`, `NkRenderTexture` |
| `UI/` | **`NkGuiCanvasBackend.h`** (le pont NKGui → NkCanvas) et `NkUICanvasBackend` (idem pour l'ancien module NKUI) |
| `Compute/` | `NkIComputeContext` (chemin compute parallèle) |

Tous les types vivent dans `nkentseu::renderer`.

### 4.2 La surface d'API « SFML-like » (celle documentée dans USAGE.md)

C'est celle qu'utilisent les jeux/démos (`Applications/Sandbox/src/DemoNkentseu/NkCanvas/NkCanvasDemo.cpp`,
`Applications/Pong/…`), **PAS NKCode** :

```cpp
// Kernel/Runtime/NKCanvas/USAGE.md:66-76
while (window.IsOpen()) {
    window.PollEvents();
    target.Clear({30, 30, 30, 255});
    target.Draw(sprite);        // nouveau pattern NkDrawable
    target.Display();
}
```

Primitives immédiates (`USAGE.md:113-122`) :

```cpp
NkRenderer2D& r = target.GetRenderer2D();
r.DrawLine({10,10},{200,50}, NkColor2D::Red, 2.f);
r.DrawFilledRect({50,50,100,80}, NkColor2D::Blue);
r.DrawRect({200,50,60,60}, NkColor2D::White, 2.f, NkColor2D::Yellow);
r.DrawCircle({400,300}, 50.f, NkColor2D::Green, 64);
r.DrawFilledTriangle({500,100},{550,200},{450,200}, NkColor2D::Magenta);
r.DrawPoint({600,400}, NkColor2D::White, 4.f);
```

Formes persistantes, transformations, texte, `NkVertexArray`, `NkRenderStates`,
`NkShader`, `NkMaterial`, `NkRenderTexture`, caméra 2D (`NkView2D`) : tout est
détaillé dans `Kernel/Runtime/NKCanvas/USAGE.md` §5 à §15.

### 4.3 La surface d'API RÉELLEMENT utilisée par NKCode (via NKEditorKit)

C'est un **sous-ensemble minuscule** de `NkIRenderer2D`
(`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Core/NkIRenderer2D.h`) :

| méthode | ligne | usage dans la chaîne NKCode |
|---|---|---|
| `bool Begin()` | 59 | ouvert par `NkRenderWindow::Clear` |
| `void End()` | 60 | fermé par `NkRenderWindow::Display` |
| `void Clear(const NkColor2D&)` | 73 | fond de l'image |
| `void OnResize(uint32,uint32)` | 89 | redimensionnement de la swapchain |
| `void SetClip(const NkRect2i&)` | 105 | découpage (scissor GPU) |
| `void PopClip()` | 109 | dépile le clip |
| `void DrawVertices(const NkVertex2D*, uint32, const uint32*, uint32, const NkTexture*)` | 192 | **LA méthode de dessin** |

Plus, côté ressources : `NkTexture::Create` / `NkTexture::Update` / `NkTexture::SetFilter`
(`Renderer/Resources/NkTexture.h`).

**Voilà tout.** NKCode ne dessine ni sprite, ni forme, ni texte NkCanvas : il envoie
des triangles pré-calculés par NKGui.

Documentation du clip, mot pour mot :

```cpp
// Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Core/NkIRenderer2D.h:94-104
// ── Clip / Scissor ────────────────────────────────────────────────────
// Restreint le rendu a un rectangle (scissor test GPU), en pixels,
// origine haut-gauche de la surface. Tout ce qui sort du rect est
// ecarte. Utile pour les panneaux UI scrollables, le masquage, etc.
//
// Pile : SetClip empile le rect (intersecte avec le clip courant) ;
// PopClip depile. ResetClip vide la pile (plus aucune restriction).
// Implementation backend : glScissor (GL), VkRect2D dynamique
// (Vulkan), RSSetScissorRects (DX11/12), clamp CPU (Software).
```

### 4.4 Le pont : `NkGuiCanvasBackend`

`Kernel/Runtime/NKCanvas/src/NKCanvas/UI/NkGuiCanvasBackend.h` (221 lignes, **header-only**).

```cpp
// NkGuiCanvasBackend.h:2-9
// NkGuiCanvasBackend.h — rend un nkgui::NkGuiDrawList via NKCanvas (NkIRenderer2D).
// Backend RÉUTILISABLE (lib) : le cœur NKGui reste render-agnostique ; ce pont
// traduit ses draw-lists en appels NkIRenderer2D. Gère l'atlas de police
// (gray8 -> RGBA blanc+alpha) et les images RGBA pour les commandes texturées.
//
// HEADER-ONLY : NKCanvas ne le compile pas (pas de .cpp) ; seuls les consommateurs
// (qui dépendent déjà de NKGui ET NKCanvas) l'incluent. Modelé sur NkUICanvasBackend.
```

Trois responsabilités :

**(a) `UploadFontGray8`** — un atlas de police est en niveaux de gris (alpha8) ;
il est étendu en RGBA blanc + alpha :

```cpp
// NkGuiCanvasBackend.h:47-55
const usize n = (usize)w * (usize)h;
mExpand.Resize(n * 4u);
uint8 *d = mExpand.Data();
for (usize i = 0; i < n; ++i) {
    d[i*4+0] = 255u; d[i*4+1] = 255u; d[i*4+2] = 255u; d[i*4+3] = gray[i];
}
```

avec ce garde-fou (piège de zoom / DPI) :

```cpp
// NkGuiCanvasBackend.h:65-66
// (Re)créer la texture si elle n'existe pas OU si la taille change (rechargement
// de police à une autre taille = zoom / DPI). Sinon Update() déborderait l'ancienne taille.
```

**(b) `UploadImageRGBA`** — table `mImages` (texId → `NkTexture*`), même logique.

**(c) `Submit`** — la traduction proprement dite :

```cpp
// NkGuiCanvasBackend.h:120-195 (condensé)
void Submit(const NkGuiDrawList &dl, uint32 fbW, uint32 fbH) {
    if (!mRenderer || dl.vtx.Size() == 0 || dl.idx.Size() == 0) return;

    // 1) conversion des sommets NKGui -> NkVertex2D (couleur dépaquetée)
    mScratch.Resize(dl.vtx.Size());
    for (uint32 i = 0; i < dl.vtx.Size(); ++i) {
        const nkgui::NkGuiVertex &s = dl.vtx[i];
        renderer::NkVertex2D &d = mScratch[i];
        d.x = s.pos.x; d.y = s.pos.y; d.u = s.uv.x; d.v = s.uv.y;
        d.r = (uint8)( s.col        & 0xFF);
        d.g = (uint8)((s.col >>  8) & 0xFF);
        d.b = (uint8)((s.col >> 16) & 0xFF);
        d.a = (uint8)((s.col >> 24) & 0xFF);
    }

    // 2) une passe par commande
    for (uint32 ci = 0; ci < dl.cmds.Size(); ++ci) {
        const nkgui::NkGuiDrawCmd &dc = dl.cmds[ci];
        if (dc.idxCount == 0u) continue;

        // 2a) clip : le rect « infini » (1e9) signifie « pas de clip »
        const bool hasClip = (dc.clipRect.w < 1.0e8f && dc.clipRect.h < 1.0e8f);
        if (hasClip) {
            … clamp à [0, fbW] × [0, fbH] …
            if (x1 <= x0 || y1 <= y0) continue;          // clip vide -> on saute
            mRenderer->SetClip(math::NkRect2i{…});
        }

        // 2b) résolution de la texture : d'abord les atlas de police, sinon les images
        renderer::NkTexture *tex = nullptr;
        if (dc.type == nkgui::NkGuiDrawCmdType::TexturedTriangles) { … }

        // 2c) sous-ensemble de sommets + indices REBASÉS
        uint32 lo = 0xFFFFFFFFu, hi = 0u;
        for (uint32 k = 0; k < dc.idxCount; ++k) { const uint32 v = dl.idx[dc.idxOffset+k];
                                                   if (v<lo) lo=v; if (v>hi) hi=v; }
        mIdxTmp.Resize(dc.idxCount);
        for (uint32 k = 0; k < dc.idxCount; ++k) mIdxTmp[k] = dl.idx[dc.idxOffset+k] - lo;
        mRenderer->DrawVertices(mScratch.Data() + lo, hi - lo + 1u, mIdxTmp.Data(), dc.idxCount, tex);

        if (hasClip) mRenderer->PopClip();
    }
}
```

Le commentaire de la partie (2c) est un des plus instructifs du dépôt :

```cpp
// NkGuiCanvasBackend.h:176-178
// Ne soumet que le SOUS-ENSEMBLE de vertices reference par cette
// commande (indices rebases). Indispensable : passer tout le buffer
// depasse kMaxVertices (65536) des qu'un draw list est gros -> crash.
```

### 4.5 `NkBatchRenderer2D` — le socle commun de tous les backends

`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Batch/NkBatchRenderer2D.h/.cpp`

Il accumule vertices/indices dans des `NkVector` et les soumet par « groupes »
(texture + blend mode). `Begin()`/`End()`/`Flush()` :

```cpp
// NkBatchRenderer2D.cpp:26-53
bool NkBatchRenderer2D::Begin() {
    if (mInFrame) { logger.Warnf("[NkBatch] Begin() called twice"); return false; }
    mInFrame = true;
    mVertices.Clear(); mIndices.Clear(); mGroups.Clear(); mCurrentTexture = nullptr;
    // Chaque frame demarre sans clip (le scissor sera (re)desactive au
    // premier Flush via ApplyScissor(false, ...)).
    mClipStack.Clear(); mClipRect = NkRect2i{}; mHasClip = false;
    BeginBackend();
    return true;
}
void NkBatchRenderer2D::End() { if (!mInFrame) return; Flush(); EndBackend(); mInFrame = false; }
```

Le clip **force un flush** — c'est la raison pour laquelle une UI très découpée
coûte des draw-calls :

```cpp
// NkBatchRenderer2D.cpp:120-141
void NkBatchRenderer2D::SetClip(const NkRect2i &rect) {
    Flush(); // committe la geometrie en cours avec le clip actuel
    const NkRect2i clip = mHasClip ? NkIntersectClip(mClipRect, rect) : rect;
    mClipStack.PushBack(clip);  mClipRect = clip;  mHasClip = true;
}
void NkBatchRenderer2D::PopClip() { … Flush(); mClipStack.PopBack(); … }
```

Et le garde-fou mémoire, dans `Flush` :

```cpp
// NkBatchRenderer2D.cpp:85-91
// GARDE-FOU : ne JAMAIS soumettre plus que la capacité des buffers GPU
// (l'upload écrirait hors buffer -> crash). Troncature (indices en
// multiple de 3) : dégradation visuelle plutôt que corruption mémoire.
uint32 vSub = mVertices.Size(), iSub = mIndices.Size();
if (vSub > kMaxVertices) vSub = kMaxVertices;
if (iSub > kMaxIndices)  iSub = kMaxIndices - (kMaxIndices % 3);
```

La constante et son histoire :

```cpp
// NkBatchRenderer2D.h:33-40
// Maximum vertices / indices before an automatic flush.
// 262144 (~5 Mo GPU) : une UI dense (IDE plein écran : arbre + onglets +
// icônes) dépasse 65536 sommets ENTRE DEUX CLIPS — la draw-list NkGui
// arrive alors en UNE commande plus grosse que l'ancien buffer, et
// l'upload écrivait HORS du buffer GPU (crash memcpy). Les 3 backends
// (DX11/DX12/GL) créent leurs buffers avec ces constantes.
static constexpr uint32 kMaxVertices = 262144;
static constexpr uint32 kMaxIndices  = kMaxVertices * 6 / 4;
```

Le retournement d'axe Y du scissor est fait dans le backend OpenGL seulement :

```cpp
// Kernel/Runtime/NKCanvas/src/NKCanvas/Backend/OpenGL/NkOpenGLRenderer2D.cpp:532-546
void NkOpenGLRenderer2D::ApplyScissor(bool enabled, const NkRect2i &rect) {
    if (!enabled) { glDisable(GL_SCISSOR_TEST); return; }
    // Clip en pixels, origine haut-gauche -> glScissor a l'origine bas-gauche :
    // on inverse Y avec la hauteur de la surface (= mViewport.height, viewport
    // plein ecran a top=0).
    …
    const int32 y = mViewport.height - rect.y - h; // flip Y
    glEnable(GL_SCISSOR_TEST);
    glScissor((GLint)x, (GLint)y, (GLsizei)w, (GLsizei)h);
}
```

---

## 5. NKGui : l'API que NKCode utilise réellement

### 5.1 La draw-list, structure de sortie

`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiDrawList.h`

```cpp
// NkGuiDrawList.h:22-40
// Sommet minimal pour le GPU. col = RGBA empaqueté (a<<24|b<<16|g<<8|r).
struct NkGuiVertex { NkVec2 pos; NkVec2 uv; /* (0,0) = couleur unie */ uint32 col; };

enum class NkGuiDrawCmdType : uint8 { Triangles, TexturedTriangles };

struct NkGuiDrawCmd {
    NkGuiDrawCmdType type = NkGuiDrawCmdType::Triangles;
    uint32 idxOffset = 0;
    uint32 idxCount  = 0;
    uint32 texId     = 0;
    NkRect clipRect  = {0.f, 0.f, 1.0e9f, 1.0e9f};   // 1e9 = « pas de clip »
};

struct NkGuiDrawList {
    NkVector<NkGuiVertex> vtx;
    NkVector<uint32>      idx;
    NkVector<NkGuiDrawCmd> cmds;
    NkRect clipStack[32] = {};
    int32  clipDepth = 0;
    float32 thickScale = 1.f; ///< échelle DPI des épaisseurs de traits/bordures (préservée par Reset)
    …
};
```

Primitives disponibles (`NkGuiDrawList.h:66-92`) :
`AddRectFilled`, `AddRect`, `AddRectFilledMultiColor`, `AddImage`, `AddLine`,
`AddTriangleFilled`, `AddTriangleMultiColor`, `AddCircleFilled`, `AddText`,
`AddTextRange`, plus `PushClipRect` / `PopClipRect` / `CurrentClip`.

**Règle de regroupement des commandes** : une nouvelle `NkGuiDrawCmd` n'est créée
que si la texture OU le rectangle de clip changent —

```cpp
// Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiDrawList.cpp:57-73
NkGuiDrawCmd &NkGuiDrawList::CurCmd(uint32 texId) noexcept {
    const NkRect clip = CurrentClip();
    bool need = (cmds.Size() == 0);
    if (!need) {
        const NkGuiDrawCmd &b = cmds.Back();
        need = (b.texId != texId) || b.clipRect.x != clip.x || … ;
    }
    if (need) { … c.type = texId ? TexturedTriangles : Triangles; cmds.PushBack(c); }
    return cmds.Back();
}
```

`PushClipRect` **intersecte** avec le clip parent (`NkGuiDrawList.cpp:38-50`) :
un enfant ne peut jamais déborder de son parent.

### 5.2 Le contexte

`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiContext.h` — c'est LE point central
(551 lignes). Contenu :

* `viewW/viewH`, `scale`, `theme` (`NkGuiTheme`), `syntax` (`NkGuiSyntax`), `input`
* **deux draw-lists** : `dl` (principale) et `dlOverlay` (popups) — ligne 138-139
* `layout` (curseur immédiat), `font`, `codeFont`
* pile de popups, routeur d'occlusion, pool de draw-lists de fenêtres (`winDL[32]`),
  arbre de docking, état des zones défilables, tables, sélection, color picker…

Le sélecteur de draw-list courante — **à connaître par cœur** :

```cpp
// NkGuiContext.h:299-303
NkGuiDrawList &DL() noexcept {
    if (curPopupLevel >= 0 || overlayDepth > 0)
        return dlOverlay;      // popup / couche overlay forcée
    return curWindow >= 0 ? winDL[curWindow] : dl;
}
```

Autrement dit : **on n'écrit jamais dans `ctx.dl` en dur depuis un panneau**, on
passe par `ctx.DL()`, qui route vers la bonne couche.

Helper DPI :

```cpp
// NkGuiContext.h:462-464
float32 S(float32 px) const noexcept { return px * scale; }  ///< px logiques → px écran (DPI)
```

(NKCode n'appelle jamais `SetUiScale`, donc `scale` vaut 1 en pratique ; l'idiome
`ctx.S(…)` est utilisé partout quand même, pour être prêt au HiDPI.)

### 5.3 Le thème

```cpp
// NkGuiContext.h:28-49  (NkGuiTheme)
NkColor bgPrimary, panel, header, button, buttonHover, buttonActive, border,
        text, textDisabled, selection, accent, track,
        tabBar, tab, tabHover, tabActive;
float32 rounding = 5.f, framePadX = 10.f, framePadY = 6.f;

// NkGuiContext.h:51-66  (NkGuiSyntax — coloration syntaxique, défauts VS Code Dark+)
NkColor text, keyword, type, string, comment, number, preproc,
        heading, mdcode, function, constant, oper;
```

Le shell les écrase avec la palette GitHub Dark dans `Init`
(`NkEditorShell.cpp:155-179`), puis NKCode superpose SA propre palette
(`NkCol::…` dans `Applications/NKCode/src/NKCode/Shell/NkUi.h:31-49`) qui est
**mutable** et réécrite à chaud par `NkApplyTheme()`.

Détection clair/sombre récurrente dans tout le dépôt (idiome) :

```cpp
// Engine/NKEditorKit/src/NKEditorKit/NkEditorScrollbar.h:29
const bool light = ((int32)th.bgPrimary.r + th.bgPrimary.g + th.bgPrimary.b) > 384;
```

### 5.4 L'entrée (`NkGuiInput`)

`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiInput.h`

```cpp
// NkGuiInput.h:33-66
NkVec2 mousePos, mouseDelta;
bool  mouseDown[3], mousePrev[3], mouseClicked[3], mouseReleased[3], mouseDoubleClicked[3];
float32 mouseDownDur[3], clickTime[3];
float32 wheel, wheelH, dt;
bool  ctrlDown, shiftDown, altDown;
uint32 chars[32]; int32 charCount;               // saisie texte (codepoints)
bool keyDown[KeyCount], keyPrev[…], keyInit[…];  float32 keyDur[…];
bool wantCopy, wantCut, wantPaste, wantSelectAll;
```

* L'application pose l'état **brut** (`mouseDown`, `keyDown`, `PushChar`).
* `NewFrame()` (appelé par `BeginFrame`) en déduit les **transitions** :
  `mouseClicked = mouseDown && !mousePrev`, `keyInit = keyDown && !keyPrev`,
  durée d'appui, double-clic (interne < 0,40 s **ou** injecté par l'OS).
* `KeyPressed(k)` = one-shot ; `KeyPressedRepeat(k, delay, rate)` = appui + rafale.

---

## 6. La saisie clavier/souris de bout en bout (NKCode)

### 6.1 Le pont événements OS → NKGui

Tout est dans `NkEditorShell::HookEvents()`
(`Engine/NKEditorKit/src/NKEditorKit/NkEditorShell.cpp:253-365`) :

```cpp
events.AddEventCallback<NkMouseMoveEvent>([this](NkMouseMoveEvent *e) {
    mUI.input.mousePos = {(float32)e->GetX(), (float32)e->GetY()};
});
events.AddEventCallback<NkMouseButtonPressEvent>([this](NkMouseButtonPressEvent *e) {
    if (e->GetButton() == NkMouseButton::NK_MB_LEFT)   mUI.input.mouseDown[0] = true;
    if (e->GetButton() == NkMouseButton::NK_MB_RIGHT)  mUI.input.mouseDown[1] = true;
    if (e->GetButton() == NkMouseButton::NK_MB_MIDDLE) mUI.input.mouseDown[2] = true; // [0]=G [1]=D [2]=milieu
    mUI.input.ctrlDown  = e->GetModifiers().ctrl; …
});
// Molette : scroll vertical + horizontal (consommee en EndFrame par NKGui).
events.AddEventCallback<NkMouseWheelVerticalEvent>(…  mUI.input.wheel += e->GetDeltaY() …);
// Saisie texte (codepoints) -> file de caracteres NKGui.
events.AddEventCallback<NkTextInputEvent>([this](NkTextInputEvent *e) {
    mUI.input.PushChar(e->GetCodepoint());
});
```

Distinction fondamentale, à expliquer dans le cours :
* **`NkTextInputEvent` → `PushChar`** : le TEXTE tapé (déjà décodé par l'OS,
  claviers AZERTY/IME compris).
* **`NkKeyPressEvent` → `MapEditKey`** : les TOUCHES (flèches, Suppr, F2, Ctrl+…).

`MapEditKey` (`NkEditorShell.cpp:370-502`) est un gros `switch` qui traduit
`NkKey` (NKEvent) → `NkGuiKey` (NKGui) :

```cpp
// NkEditorShell.cpp:367-370
// Mappe une touche OS d'edition vers l'etat ENFONCE NKGui (press/release ->
// repetition au maintien geree par NKGui). Sans ce pont, aucune navigation
// clavier ni edition de texte dans les panneaux.
```

Les raccourcis d'édition sont posés en drapeaux (et non consommés ici) :

```cpp
// NkEditorShell.cpp:318-326
if (e->GetModifiers().ctrl) { // raccourcis copier/couper/coller/tout-selectionner
    if      (k == NkKey::NK_C) mUI.input.wantCopy = true;
    else if (k == NkKey::NK_X) mUI.input.wantCut = true;
    else if (k == NkKey::NK_V) mUI.input.wantPaste = true;
    else if (k == NkKey::NK_A) mUI.input.wantSelectAll = true;
```

et le zoom clavier est traité **là**, avec sa justification :

```cpp
// NkEditorShell.cpp:327-335
// Zoom éditeur au CLAVIER : Ctrl+= / Ctrl++ (pavé) zoome, Ctrl+- / Ctrl+pavé- dézoome,
// Ctrl+0 réinitialise. Traité ICI (fiable, comme les autres Ctrl+touche) plutôt que
// via KeyPressedRepeat dans un panneau, qui ne déclenchait pas.
```

### 6.2 Survol et clic : le cœur de `NkGuiContext`

Trois niveaux, du plus simple au plus correct :

**Niveau 1 — brut** (à éviter sauf chrome de bas niveau) :
```cpp
// NkGuiTypes.h:328-330
inline bool NkGuiRectContains(const NkRect &r, const NkVec2 &p) noexcept {
    return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}
```

**Niveau 2 — occlusion unifiée** (`NkGuiContext.h:163-223`) :
```cpp
// Hit-test UNIFIÉ : souris dans `r` ET non recouverte par une couche
// supérieure. À utiliser PARTOUT à la place de NkGuiRectContains(mousePos).
bool InputHits(const NkRect &r) const noexcept;
bool ClickIn (const NkRect &r) const noexcept;   // = mouseClicked[0] && InputHits(r)
void PushOcclusion(const NkRect &r, int32 layer) noexcept;
struct NkInputLayerScope { … };                  // RAII, fixe curInputLayer
```

Le commentaire qui explique POURQUOI ce routeur existe est le plus important
du fichier :

```cpp
// NkGuiContext.h:163-174
// ── ROUTEUR D'OCCLUSION UNIFIÉ (surfaces flottantes « maison ») ─────
// PROBLÈME DE FOND résolu ici : les overlays applicatifs (modals, palettes,
// popovers, peek…) faisaient des hit-tests BRUTS (rect + mouseClicked) qui
// ignoraient ce qui est dessiné AU-DESSUS d'eux → traversées de clics,
// boutons inertes, consommations manuelles fragiles dupliquées partout.
// PRINCIPE : chaque surface flottante déclare son rect + sa couche CHAQUE
// frame (PushOcclusion) pendant son dessin ; tous les hit-tests passent par
// InputHits/ClickIn qui refusent un point recouvert par une couche PLUS
// HAUTE que celle en cours de dessin (curInputLayer). Comme hotIdPrev, la
// liste lue est celle de la FRAME PRÉCÉDENTE (stable pour toute la frame,
// indépendante de l'ordre de dessin des panneaux). Couches conseillées :
// 0 fond/panneaux · 50 menus/palettes/popovers · 100 modals · 200 debug.
```

**Niveau 3 — widget complet** : `ItemHoverable` + `ButtonBehavior`
(`NkGuiContext.cpp:491-570`).

```cpp
bool NkGuiContext::ItemHoverable(const NkRect &r, NkGuiId id) noexcept {
    if (!PointReachable(input.mousePos)) return false;   // occlusion par couche
    if (IsDisabled()) return false;                      // BeginDisabled
    for (int32 i = curPopupLevel + 1; i < popupDepth; ++i)
        if (NkGuiRectContains(popupRects[i], input.mousePos)) return false;  // popup plus profond
    if (curPopupLevel < 0 && hoveredWindowId != NKGUI_ID_NONE && hoveredWindowId != curWindowId)
        return false;                                    // fenêtre recouverte
    // Hors du CLIP courant (zone défilable, panneau) : pas d'interaction —
    // un item scrollé hors-vue ne doit pas capturer le pointeur.
    if (!NkGuiRectContains(DL().CurrentClip(), input.mousePos)) return false;
    if (activeId != NKGUI_ID_NONE && activeId != id) return false;  // capture par un autre
    if (!NkGuiRectContains(r, input.mousePos)) return false;
    // Greedy : le DERNIER widget soumis sous le pointeur écrase hotId →
    // celui dessiné par-dessus gagne. On ne déclare « survolé » QUE le
    // front-most de la frame précédente (hotIdPrev) ; le widget masqué
    // dessous, lui, met à jour hotId mais retourne false → ne capture pas.
    hotId = id;
    return hotIdPrev == id;
}
```

```cpp
// NkGuiContext.cpp:530-570 — sémantique « clic » standard
bool NkGuiContext::ButtonBehavior(NkGuiId id, const NkRect &r, NkGuiButtonFlags flags, …) {
    const bool hovered = ItemHoverable(r, id);
    if (hovered && input.mouseClicked[0]) { activeId = id; if (repeat) pressed = true; }
    const bool held = (activeId == id);
    if (held) {
        interact = NkGuiInteract::EditWidget;
        …rafale typematic si Repeat…
        if (input.mouseReleased[0]) {
            // Sans Repeat : clic validé au relâchement DANS le rect.
            if (!repeat && NkGuiRectContains(r, input.mousePos)) pressed = true;
            activeId = NKGUI_ID_NONE;
        }
    } else if (hovered) interact = NkGuiInteract::HoverWidget;
    lastItemHovered = hovered;  // pour IsItemHovered() / SetTooltip
    return pressed;
}
```

**Un clic se valide au RELÂCHEMENT dans le rectangle, pas à l'appui.**

### 6.3 Identifiants de widget (`NkGuiId`)

Hash FNV-1a 32 bits (`NkGuiTypes.h:29-46`), avec une **pile de scopes** :

```cpp
// NkGuiContext.cpp:144-166
void NkGuiContext::PushId(const char *s) noexcept {
    const NkGuiId seed = idDepth > 0 ? idStack[idDepth-1] : 2166136261u;
    if (idDepth < 32) idStack[idDepth++] = NkGuiHashStr(s, seed);
}
NkGuiId NkGuiContext::GetId(const char *s) const noexcept {
    const NkGuiId seed = idDepth > 0 ? idStack[idDepth-1] : 2166136261u;
    return NkGuiHashStr(s, seed);
}
```

Conséquence pratique : deux boutons portant le même libellé dans le même scope
partagent le même id → il faut `PushId(index)` autour de chaque élément de liste,
ou utiliser un `idStr` explicite (`SelectableEditable`, `TableCellText`, `Splitter`…
prennent tous un `idStr` « STABLE » distinct du libellé affiché).

---

## 7. Le dessin de texte

### 7.1 La police NKGui

```cpp
// Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiFont.h:19-69
struct NkGuiFont {
    NkFontAtlas atlas;          ///< possède la texture + les glyphes
    NkFont *face = nullptr;     ///< face produite (détenue par l'atlas)
    uint32 texId = 0x4E4B4654u; ///< 'NKFT' — id stable pour le backend
    uint8 *pixels = nullptr;    ///< atlas alpha8 (détenu par l'atlas)
    int32 atlasW = 0, atlasH = 0;
    bool dirty = false;         ///< à (ré)uploader côté backend

    bool LoadEmbedded(NkEmbeddedFontId id, float32 sizePx, bool extFallback = true) noexcept;
    bool LoadFromFile(const char *path, float32 sizePx, bool extFallback = true) noexcept;

    float32 Ascent() const;      // face->ascent
    float32 Descent() const;     // face->descent
    float32 LineHeight() const;  // face->lineAdvance
    float32 MeasureWidth(const char *s) const;  // face->CalcTextSizeX(s)
};
```

Polices de repli externes (à poser par l'APPLICATION) :

```cpp
// NkGuiFont.h:71-78
// Polices de REPLI EXTERNES (fichiers .ttf charges au runtime) : tout glyphe
// absent des polices principales (Inter/DejaVu) y est cherche. Roles :
//   broad : large couverture (latin etendu, grec, cyrillique, hebreu, arabe, symboles)
//   cjk   : ideogrammes (中文/日本語/한국어) — volumineux, opt-in
//   emoji : emoji monochrome
// A poser par l'APPLICATION (ex. NKCode, depuis son dossier data/fonts) AVANT
// de charger les polices. Un chemin nullptr/vide = role desactive.
void NkSetFallbackFontPaths(const char *broad, const char *cjk, const char *emoji) noexcept;
```

C'est exactement ce que fait `nkcode::NkLoadFallbackFonts()` en `main.cpp:74`.

#### Comment l'atlas est construit

`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiFont.cpp:119-135` :

```cpp
bool NkGuiFont::LoadEmbedded(NkEmbeddedFontId id, float32 sizePx, bool extFallback) noexcept {
    atlas.Clear();  face = nullptr;  pixels = nullptr;   // rechargeable
    NkFontConfig cfg;
    cfg.glyphRanges = NkGlyphRanges();
    face = NkFontEmbedded::AddToAtlas(atlas, id, sizePx, &cfg);
    if (!face) return false;
    NkMergeFallback(atlas, sizePx, extFallback);          // repli pour les glyphes manquants
    if (!atlas.Build()) return false;
    int32 bpp = 0;
    atlas.GetTexDataAsAlpha8(&pixels, &atlasW, &atlasH, &bpp);
    dirty = (pixels != nullptr && atlasW > 0 && atlasH > 0);
    return dirty;
}
```

`NkGlyphRanges()` (`NkGuiFont.cpp:14-39`) = **22 plages** : latin/latin-1, latin étendu A/B,
API, diacritiques, grec, cyrillique, ponctuation générale, exposants/indices, monétaires,
type-lettre, formes numériques, flèches, opérateurs mathématiques, technique divers,
alphanumériques encerclés, **box-drawing (0x2500-0x257F)**, blocs, formes géométriques,
symboles divers, dingbats, flèches 0x2B00, ligatures latines 0xFB00-0xFB06.

La fusion des replis se fait en **`mergeMode`** dans le même atlas (`NkGuiFont.cpp:103-117`) :

```cpp
static void NkMergeFallback(NkFontAtlas &atlas, float32 sizePx, bool ext) noexcept {
    if (!ext) return;
    auto add = [&](const char *path, const uint32 *ranges) {
        if (!NkFileExists(path)) return;
        NkFontConfig fb;
        fb.glyphRanges = ranges;
        fb.mergeMode = true;
        atlas.AddFontFromFile(path, sizePx > 0.f ? sizePx : 16.f, &fb);
    };
    add(gFbBroad, NkBroadRanges());
    add(gFbCjk,   NkCjkRanges());
    add(gFbEmoji, NkEmojiRanges());
}
```

Trois chemins statiques (`NkGuiFont.cpp:70`) : `static char gFbBroad[600], gFbCjk[600], gFbEmoji[600];`.

#### Où NKCode cherche les fichiers de repli

`Applications/NKCode/src/NKCode/Shell/NkAppFonts.h:17-52` (`NkLoadFallbackFonts`) —
dossiers candidats (`:22-25`) : `Applications/NKCode/data/fonts/`, `data/fonts/`,
`NKCode/data/fonts/`, `NkPath::GetExecutableDirectory() + "/data/fonts/"`, `""`.
Noms recherchés (`:44-46`) : `NotoSans-Regular.ttf` (broad),
`NotoSansSC-Regular.ttf` / `NotoSansSC.ttf` / `NotoSansCJKsc-Regular.otf` (CJK),
`NotoEmoji-Regular.ttf` (emoji).

```cpp
// NkAppFonts.h:18-21
// Candidats RELATIFS AU CWD (dev, lancement depuis la racine du repo) PUIS relatifs a
// l'EXECUTABLE : indispensable pour une distribution, ou l'utilisateur peut lancer
// NKCode.exe depuis n'importe quel dossier (raccourci, PATH, ligne de commande) —
// sinon aucune police trouvee.
```

#### Résolution par NOM : embarquée, puis police système

`Engine/NKEditorKit/src/NKEditorKit/NkFontPrefs.h` :

* réglages persistés (`:23-28`) : `uiFont = "Inter"`, `codeFont = "DejaVuSansMono"`,
  `uiSize = 16.f`, `codeSize = 15.f` ;
* listes proposées : `NkUiFontNames` (`:31-36`), `NkCodeFontNames` (`:38-43`) ;
* table nom → id embarqué : `NkEmbeddedIdFromName` (`:46-70`, 10 entrées) ;
* table nom → fichier `C:\Windows\Fonts` : `NkSystemFontFile` (`:73-95`) ;
* le résolveur (`:101-112`) :

```cpp
inline bool NkResolveFont(nkgui::NkGuiFont &font, const NkString &name, float32 size,
                          bool extFallback = true) {
    NkEmbeddedFontId id;
    if (NkEmbeddedIdFromName(name, id))
        return font.LoadEmbedded(id, size, extFallback);
#if defined(_WIN32)
    if (const char *file = NkSystemFontFile(name)) {
        NkString path = NkString("C:\\Windows\\Fonts\\") + file;
        return font.LoadFromFile(path.CStr(), size, extFallback);
    }
#endif
    return false;
}
```

Persistance : `~/.nkcode_fonts.cfg` (`NkFontPrefsPath()` `:115-125`, clés `ui.font`,
`ui.size`, `code.font`, `code.size`, clamp 8..40 px) ; le même fichier héberge aussi
`NkSaveTheme`/`NkLoadTheme` (`:220-264`) et les couleurs de syntaxe
(`~/.nkcode_syntax.cfg`, `:267-338`).

> **Exception à signaler** : `NkFontPrefs.h:16-18` inclut `<cstdio>/<cstdlib>/<cstring>`
> et utilise `fopen/fgets/fprintf/atof/sscanf`. C'est le **seul** endroit de la chaîne
> polices/texte qui déroge à la règle « pas de libc ».

### 7.2 La formule de la ligne de base — l'idiome à retenir

`AddText` prend une **baseline**, pas un coin haut-gauche. Les deux conversions
canoniques sont dans NKGui :

```cpp
// Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.cpp:111-118  (texte aligné en haut)
float32 TextAt(NkGuiContext &ctx, const NkVec2 &topLeft, const char *s, const NkColor &col) noexcept {
    if (!ctx.font || !ctx.font->Valid() || !s) return 0.f;
    // topLeft = coin haut-gauche → ligne de base = y + ascender.
    const float32 baseY = topLeft.y + ctx.font->Ascent();
    ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(), {topLeft.x, baseY}, s, col);
    return ctx.font->MeasureWidth(s);
}
```

```cpp
// NkGuiWidgets.cpp:125-133  (texte centré dans un rectangle)
static void DrawCenteredLabel(NkGuiContext &ctx, const NkRect &r, const char *label, const NkColor &col) {
    const float32 tw = ctx.font->MeasureWidth(label);
    const float32 tx = r.x + (r.w - tw) * 0.5f;
    const float32 baseY = r.y + (r.h - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent();
    ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(), {tx, baseY}, label, col, r.w - 6.f);
}
```

> **Formule à mémoriser** :
> `baseline.y = rect.y + (rect.h - font->LineHeight()) * 0.5f + font->Ascent();`
> et pour un simple alignement haut : `baseline.y = y + font->Ascent();`

Le shell l'applique tel quel, par exemple dans la barre de titre :

```cpp
// Engine/NKEditorKit/src/NKEditorKit/NkEditorShell.cpp:997-998
const float32 by = bar.y + (bar.h - mUI.font->LineHeight()) * 0.5f + mUI.font->Ascent();
dl.AddText(mUI.font->Face(), mUI.font->TexId(), {ix, by}, info, {150, 150, 150, 255});
```

#### La mesure : il n'existe AUCUNE fonction `MeasureText` dans le dépôt

(Recherche exhaustive : 0 résultat.) Les équivalents sont :

```cpp
// Kernel/Runtime/NKFont/src/NKFont/NkFontAtlas.cpp:791-802
nkft_float32 NkFont::CalcTextSizeX(const char *text, const char *textEnd) const {
    nkft_float32 w = 0;
    const char *p = text;
    while (p < textEnd || (!textEnd && *p)) {
        NkFontCodepoint cp = DecodeUTF8(&p, textEnd);
        if (!cp) break;
        const NkFontGlyph *g = FindGlyph(cp);
        w += g ? g->advanceX : fallbackAdvanceX;
    }
    return w;
}
```

Distinction capitale (`NkFontAtlas.cpp:776-789`) :
* **`FindGlyph(cp)`** retombe sur `fallbackGlyph` puis sur `'?'` (0x003F) ;
* **`FindGlyphNoFallback(cp)`** renvoie `nullptr` si le glyphe est vraiment absent.
  C'est ce dernier qui sert de test « la police a-t-elle ce caractère ? » (§7.7).

Métriques de la face (`Kernel/Runtime/NKFont/src/NKFont/NkFont.h:210-235`) :
`fontSize, ascent, descent, lineAdvance, scale` + `FindGlyph`, `FindGlyphNoFallback`,
`GetCharAdvance`, `CalcTextSizeX`.
Attention : `NkGuiFont::LineHeight()` renvoie `face->lineAdvance`, **pas**
`ascent + descent`.

#### Métriques de l'éditeur de code

`Applications/NKCode/src/NKCode/Editor/NkCodeEditor.h:2348-2351` — tout le rendu
de l'éditeur découle de ces quatre lignes :

```cpp
const float32 lineGap = ctx.S(5.f);                      // espace entre les lignes (interligne)
const float32 lineH   = ctx.font->LineHeight() + lineGap; // hauteur d'une ligne
const float32 asc     = ctx.font->Ascent() + lineGap * 0.5f; // baseline centree dans la ligne
const float32 pad     = 4.f;
```

puis dans la boucle (`:3974-3975`) : `const float32 baseline = y + asc;`
avec `segYof(sg) = textTop + (vrow + sg) * lineH - d.scrollY` (`:3963`).

Positionnement horizontal — la fonction pivot (`NkCodeEditor.h:2173-2181`) :

```cpp
inline float32 PrefixW(NkGuiContext &ctx, const NkCodeDoc &d, int32 l, int32 col) {
    if (col <= 0 || l < 0 || l >= d.LineCount()) return 0.f;
    const NkCodeLine &ln = d.lines[l];
    if (ln.Size() == 0) return 0.f;
    const int32 c = col > (int32)ln.Size() ? (int32)ln.Size() : col;
    return ctx.font->Face()->CalcTextSizeX(ln.Data(), ln.Data() + c);
}
```

L'inverse (pixel → colonne) : `ColAtX` (`:2189-2202`) et `ColAtXWrap` (`:2204-2217`).
Largeur d'un caractère monospace : `const float32 chW = ctx.font->MeasureWidth("0");` (`:3824`).
Largeur de gouttière : `MeasureWidth(NkPrintf("%d", d.LineCount())) + pad*2`, plus
`foldW = ctx.S(13.f)`, `bpW = lineH`, `gitW = 3.f` (`:2355-2360`).
Colonnes de word-wrap : `viewW / MeasureWidth("0") - 1`, plancher 8 (`:2378-2382`).

### 7.3 `AddText` en détail, et le piège du flou

`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiDrawList.cpp:226-283`

```cpp
// ── ALIGNEMENT D'UN GLYPHE SUR LA GRILLE DE PIXELS ──────────────────────
// Le curseur de texte accumule des avances FRACTIONNAIRES. Sans arrondi,
// seul le PREMIER glyphe d'une chaîne tombe sur un pixel entier ; tous les
// suivants dérivent et échantillonnent l'atlas ENTRE deux texels, ce qui
// rend le texte uniformément flou. Le défaut est d'autant plus marqué que le
// corps est petit : l'erreur vaut un demi-texel CONSTANT, soit 4 % de la
// hauteur d'un caractère à 13 px et 3,3 % à 15 px.
static inline float32 NkGuiPixelSnap(float32 v) noexcept {
    return (float32)(int32)(v < 0.f ? v - 0.5f : v + 0.5f);
}
```

et dans la boucle :

```cpp
// On arrondit la POSITION du quad, jamais l'AVANCE : `x` continue
// d'accumuler la valeur exacte, donc la largeur totale de la chaîne
// est inchangée et MeasureWidth reste d'accord avec le rendu. Seul
// l'espacement entre deux glyphes varie de moins d'un pixel, ce qui
// ne se voit pas — alors que le flou, lui, se voyait.
//
// La LARGEUR est reportée telle quelle : arrondir les deux bords
// séparément étirerait le glyphe d'un pixel et le rééchantillonnerait,
// ce qu'on cherche précisément à éviter.
const float32 x0 = NkGuiPixelSnap(x + g->x0), y0 = NkGuiPixelSnap(y + g->y0);
const float32 x1 = x0 + (g->x1 - g->x0), y1 = y0 + (g->y1 - g->y0);
if (x1 > xEnd) break;   // troncature simple (maxWidth)
…
x += g->advanceX;
```

`AddTextRange(face, texId, baseline, begin, end, col)` dessine une **sous-chaîne**
sans troncature — c'est la brique du retour à la ligne et de la coloration
syntaxique (un `AddTextRange` par jeton coloré). Son commentaire (`:302-304`)
rappelle qu'il doit appliquer le MÊME snapping, « sinon ce chemin resterait flou
alors que l'autre serait net ».

Décodage UTF-8 : `NkFontDecodeUTF8(&p, end)` puis `face->FindGlyph(cp)`.
Un glyphe absent est simplement sauté (`continue`).

### 7.4 Les trois (quatre) polices vivantes dans NKCode

| police | membre | atlas texId | rôle |
|---|---|---|---|
| interface | `mFont` | `0x4E4B4654` ('NKFT') | menus, panneaux, boutons (proportionnelle, Inter) |
| code (repli global) | `mCodeFont` | `mFont.TexId() + 1` | monospace, taille globale des préférences |
| terminal | `mTermFont` | `mFont.TexId() + 2` | monospace, taille GLOBALE fixe, non zoomée |
| cache de zoom | `mCodeSlots[0..7].font` | `mFont.TexId() + 8 .. +15` | une taille rasterisée par slot (LRU) |

Sources : `NkEditorShell.cpp:232` (`+1`), `:2158` (`+2`), `:2271` (`+8+idx`).

Chargement, avec repli en cascade :

```cpp
// NkEditorShell.cpp:2105-2122
void NkEditorShell::LoadUiFont() noexcept {
    // Police chargee a uiSize x DPI : le LAYOUT est mis a l'echelle (ctx.S) mais
    // la police etait a une taille ABSOLUE -> texte trop petit sur ecran scale.
    const float32 dpi   = mUI.S(1.f) > 0.5f ? mUI.S(1.f) : 1.f;
    const float32 uiPx  = mFontPrefs.uiSize * dpi;
    mFontOk = NkResolveFont(mFont, mFontPrefs.uiFont, uiPx);
    if (!mFontOk) mFontOk = mFont.LoadEmbedded(NkEmbeddedFontId::Inter,        uiPx);
    if (!mFontOk) mFontOk = mFont.LoadEmbedded(NkEmbeddedFontId::Karla,        16.f*dpi);
    if (!mFontOk) mFontOk = mFont.LoadEmbedded(NkEmbeddedFontId::ProggyClean,  13.f*dpi);
    mUI.font = &mFont;
    if (mFontOk && mRenderer)
        mRenderer->UploadFontGray8(mFont.TexId(), mFont.pixels, mFont.atlasW, mFont.atlasH);
}
```

La police du CODE est chargée **sans replis externes**, et c'est expliqué :

```cpp
// NkEditorShell.cpp:2131-2134
// Police MONOSPACE : AUCUN repli externe (broad/CJK/emoji = plusieurs milliers de
// glyphes). L'atlas reste petit -> reconstruction rapide (l'interface garde les
// replis complets pour l'i18n). La police embarquee couvre deja Latin/accents/box-drawing.
bool codeOk = NkResolveFont(mCodeFont, mFontPrefs.codeFont, codePx, /*extFallback=*/false);
```

### 7.5 Le zoom du code : cache d'atlas + debounce

C'est le mécanisme le plus subtil du shell, et il vaut un chapitre entier du cours.

**API pour l'éditeur** (`NkEditorShell.h:334-336`) :

```cpp
// CACHE d'atlas de code PAR TAILLE : l'editeur appelle EnsureCodeSize (arme la
// rasterisation d'une taille) + CodeFontForSize (police a utiliser MAINTENANT, non
// bloquant). Une taille deja rasterisee reste en cache -> revenir sur un onglet zoome
// = atlas DEJA pret (aucun rebuild, aucun « saut »).
void EnsureCodeSize(float32 logicalPx, bool immediate = false) noexcept;
nkgui::NkGuiFont *CodeFontForSize(float32 logicalPx) noexcept;
```

**Implémentation** (`NkEditorShell.cpp:2207-2285`) :
* `EnsureCodeSize` : si la taille est déjà en cache → rien ; sinon arme
  `mCodePendingPx` + `mCodeReloadCountdown` (0 si `immediate`, sinon
  `kCodeReloadDebounce = 0.12f`).
* `CodeFontForSize` : rend **la taille exacte si elle est en cache, sinon la plus
  proche déjà rasterisée** — jamais bloquant, jamais de trou visuel.
* `BuildCodeSlot(px)` : rasterise dans un slot libre, sinon évince le LRU ;
  uploade l'atlas ; met à jour `s.px` et `s.lru`.

**Où le rebuild a lieu** — jamais au milieu d'une frame :

```cpp
// NkEditorShell.cpp:596-621
// Rechargement de police differe (zoom Ctrl+molette / Ctrl+±) : execute ICI,
// avant BeginFrame, donc aucune draw list ne reference l'ancien atlas pendant
// la re-rasterisation + re-upload backend.
// Full = les deux polices (changement UI/prefs, immediat). Zoom = DEBOUNCE :
// l'atlas code n'est reconstruit qu'apres kCodeReloadDebounce sans nouveau cran
// -> molette fluide (pas de rebuild par cran), un seul rebuild a la fin.
```

**Routage du zoom vers l'onglet actif** (NKCode) :

```cpp
// Applications/NKCode/src/NKCode/Shell/NkAppCommands.h:107-122
inline void ZoomHandler(void *u, nkentseu::float32 delta, bool reset) {
    auto *z = static_cast<ZoomCtx *>(u);
    if (!z || !z->st || !z->st->HasActive()) return;
    auto &f = z->st->files[z->st->active];
    if (reset) { f.codeZoom = 0.f; return; }
    nkentseu::float32 s = (f.codeZoom > 0.f ? f.codeZoom : z->shell->CodeFontSize()) + delta;
    if (s <  8.f) s =  8.f;
    if (s > 40.f) s = 40.f;
    f.codeZoom = s;
}
```

Le shell délègue dès qu'un handler existe :

```cpp
// NkEditorShell.cpp:2288-2291
void NkEditorShell::NudgeCodeFontSize(float32 delta) noexcept {
    if (mZoomFn) { mZoomFn(mZoomUser, delta, false); return; } // zoom PAR ONGLET (app)
    …
}
```

**Ce que le panneau Éditeur fait chaque frame**, juste avant d'appeler `CodeEditor`
(`Applications/NKCode/src/NKCode/Shell/Panels.h:774-792`) :

```cpp
if (ctx.input.ctrlDown && overEd && ctx.input.wheel != 0.f) {
    mShell->NudgeCodeFontSize(ctx.input.wheel > 0.f ? 1.f : -1.f);
    ctx.input.wheel = 0.f;                   // molette CONSOMMÉE
}
…
const int32 act = (mS && mS->HasActive()) ? mS->active : -1;
const bool switched = (act != mZoomLastActive);
mZoomLastActive = act;
const float32 sz = act >= 0 ? mS->files[act].codeZoom : 0.f;
mShell->EnsureCodeSize(sz, switched);
ctx.codeFont = mShell->CodeFontForSize(sz);  // police de CETTE frame (cache)
```

Le zoom vit **par onglet** : `NkCodeState.h:55`
`float32 codeZoom = 0.f; // taille police PROPRE a cet onglet (0 = taille globale)`,
persisté en session (`NkCodeState.h:4250`, restauré `:4361`).

### 7.6 Le dessin de l'éditeur de code, ligne par ligne

**(a) Bascule de police, en RAII** — `Applications/NKCode/src/NKCode/Editor/NkTextDraw.h:55-76` :

```cpp
struct NkCodeFontScope {
    NkGuiContext &c;
    NkGuiFont *prev;
    explicit NkCodeFontScope(NkGuiContext &ctx) : c(ctx), prev(ctx.font) {
        if (ctx.codeFont && ctx.codeFont->Valid()) ctx.font = ctx.codeFont;
    }
    // Variante avec police EXPLICITE (ex. terminal -> atlas propre, decouple du zoom
    // par-onglet de l'editeur). Repli sur ctx.codeFont si f nul/invalide.
    NkCodeFontScope(NkGuiContext &ctx, NkGuiFont *f) : c(ctx), prev(ctx.font) { … }
    ~NkCodeFontScope() { c.font = prev; }
};
```

Toute première ligne de l'éditeur (`NkCodeEditor.h:2312`) :

```cpp
NkCodeFontScope _cfs(ctx); // tout l'editeur dessine avec la police monospace (code)
```

**(b) Fenêtre de lignes visibles** (`NkCodeEditor.h:3791-3796`) :

```cpp
int32 firstVis = (int32)((d.scrollY - topPad) / lineH);
if (firstVis < 0) firstVis = 0;
const int32 lastVis = firstVis + (int32)(viewH / lineH) + 2;
// Repli : 1re ligne DOC affichée au row visuel `firstVis` (mapping lignes visibles).
const int32 startDoc = d.LineAtRow(firstVis);
```

**(c) Couleurs de syntaxe** (`NkCodeEditor.h:3811-3813`) :

```cpp
const NkSynColors &syn = ctx.syntax; // couleurs editables via Preferences > Langages
const NkFont *face = ctx.font->Face();
const uint32 tex   = ctx.font->TexId();
```

`NkSynColors` est un alias : `Applications/NKCode/src/NKCode/Editor/NkSyntax.h:867`
→ `using NkSynColors = nkentseu::nkgui::NkGuiSyntax;`.

Le tokenizer est un **template à callback**, jamais une allocation (`NkSyntax.h:982-991`) :

```cpp
// Tokenise [L, L+n). `inBlock` = on est dans un /*..*/ ouvert. emit(a,b,color)
// pour CHAQUE plage (les trous sont comblés en couleur texte). Retourne le nouvel
// etat de bloc-commentaire. Emit signature : void(int32, int32, const NkColor&).
template <class Emit>
inline int32 TokenizeLine(NkLang lang, const char *L, int32 n, int32 st, const NkSynColors &C, Emit emit, …);
```

L'état de commentaire multi-lignes est propagé depuis le **début du document** jusqu'à la
première ligne visible, avec un emit vide (`NkCodeEditor.h:3818-3821`, et `:3933-3937`
pour les lignes repliées) :

```cpp
inBlock = TokenizeLine(lang, d.lines[i].Data(), (int32)d.lines[i].Size(), inBlock, syn,
                       [](int32, int32, const NkColor &) {});
```

**(d) Ordre exact du dessin d'une ligne** — `dl.PushClipRect(textArea, true);` (`:3930`),
boucle `for (int32 i = startDoc; …)` (`:3931`), et pour chaque ligne visible :

1. calcul des segments de wrap `segA/segXo` (`:3944-3961`) et des lambdas
   `segYof` / `segOfCol` / `colX` (`:3963-3973`) ;
2. guides d'indentation → `AddLine` (`:3989-3993`) ;
3. correspondance de parenthèses → `AddRectFilled` (`:3996-3999`) ;
4. occurrences de la sélection → `AddRectFilled` alpha 34 (`:4002-4025`) ;
5. sélection → `AddRectFilled(kSel)` par segment (`:4027-4042`) ;
6. **le texte** (`:4045-4114`) ;
7. badge `…` de repli → `dl.AddText(face, tex, {bx + chW*0.5f, baseline}, "...", ctx.theme.textDisabled);`
   (`:4070` et `:4107`).

Puis `dl.PopClipRect();` (`:4236`), après diagnostics, lien Ctrl-clic, caret et multi-curseurs.

Le cœur, chemin **sans wrap** (`:4047-4061`) :

```cpp
float32 sx = textLeft - d.scrollX;
inBlock = TokenizeLine(
    lang, data, n, inBlock, syn,
    [&](int32 a, int32 b, const NkColor &col) {
        NkColor c = col;
        if (dim) {                       // branche préprocesseur morte : 42 % couleur / 58 % fond
            c.r = (uint8)((col.r * 42 + bgR * 58) / 100);
            c.g = (uint8)((col.g * 42 + bgG * 58) / 100);
            c.b = (uint8)((col.b * 42 + bgB * 58) / 100);
        }
        sx = NkDrawTextU(ctx, sx, baseline, y, lineH, data + a, data + b,
                         c); // box-drawing en primitives
    },
    &d.symTypes, &d.symFuncs, projTypes, projFuncs); // coloration sémantique (fichier + projet)
```

Chemin **avec wrap** (`:4078-4098`) : la plage de token est découpée sur les segments traversés :

```cpp
int32 a2 = a;
while (a2 < b) {
    const int32 sg = segOfCol(a2);
    const int32 b2 = b < segA[sg + 1] ? b : segA[sg + 1];
    if (b2 <= a2) break;
    NkDrawTextU(ctx, textLeft + PrefixW(ctx, d, i, a2) - segXo[sg], segYof(sg) + asc,
                segYof(sg), lineH, data + a2, data + b2, c);
    a2 = b2;
}
```

### 7.7 `NkDrawTextU` : dessiner les glyphes que la police n'a pas

`Applications/NKCode/src/NKCode/Editor/NkTextDraw.h:363-393` :

```cpp
inline float32 NkDrawTextU(NkGuiContext &ctx, float32 x, float32 baseline, float32 cellTop,
                           float32 cellH, const char *begin, const char *end, const NkColor &col) {
    if (!ctx.font || !ctx.font->Valid()) return x;
    const NkFont *face = ctx.font->Face();
    const uint32 tex   = ctx.font->TexId();
    const char *run = begin, *p = begin;
    auto flush = [&](const char *e) {
        if (e > run) {
            ctx.DL().AddTextRange(face, tex, {x, baseline}, run, e, col);
            x += face->CalcTextSizeX(run, e);
        }
    };
    while (p < end) {
        const char *q = p;
        const uint32 cp = NkDecodeU8(q, end);
        // Primitive UNIQUEMENT si la police n'a pas le glyphe (DejaVu a le
        // box-drawing -> on le laisse rendre ; DroidSans non -> on dessine).
        if (NkIsDrawable(cp) && !face->FindGlyphNoFallback(cp)) {
            flush(p);
            const float32 w = face->CalcTextSizeX(p, q); // avance (glyphe de repli)
            NkDrawGlyphPrim(ctx.DL(), cp, x, cellTop, w, cellH, col);
            x += w;
            run = q;
        }
        p = q;
    }
    flush(p);
    return x;
}
```

`NkIsDrawable` (`:235-237`) = `[0x2500, 0x25FF] ∪ [0x2190, 0x2193]`.
`NkDrawGlyphPrim` (`:240-359`) dessine ~30 codepoints en `AddLine` / `AddRectFilled` /
`AddCircleFilled` / `AddTriangleFilled` : box-drawing simple et double, blocs pleins et
demi, ombrages `░▒▓` (alphas 64 / 110 / 170), `■□●▲▼`, flèches `←→↑↓`, avec un petit
cadre par défaut.

> C'est un **bel exemple pédagogique** : quand la ressource manque, on dégrade en
> primitives géométriques plutôt que d'afficher un carré vide — et on regroupe les
> plages non concernées en un seul `AddTextRange` (`flush`).

Réparation du **double-encodage UTF-8** (mojibake), même fichier :

```cpp
// NkTextDraw.h:102-105
// ── Réparation du DOUBLE-ENCODAGE UTF-8 (« mojibake ») ────────────────────
// Un fichier UTF-8 relu en CP1252 puis re-sauvé en UTF-8 donne "Ã©" au lieu de "é".
// Réparer = décoder l'UTF-8 courant en codepoints, remapper chaque codepoint vers son
// OCTET CP1252 d'origine ; le flux d'octets obtenu EST l'UTF-8 correct.
```

Détection : `NkMojibakeDetect` (`:209-228`), seuil `hits >= 4` (« seuil prudent »).

### 7.8 La gouttière (numéros de ligne) : un clip séparé

`NkCodeEditor.h:4563-4612` :

```cpp
dl.PushClipRect({area.x, area.y, gutterW, area.h}, true);
… chevrons de repli (triangles, :4574-4584)
… bande Git (:4586-4592)
… breakpoints (cercles, :4594-4597)
const NkString nb = NkPrintf("%d", i + 1);
const float32 nw = ctx.font->MeasureWidth(nb.CStr());
dl.AddText(ctx.font->Face(), ctx.font->TexId(),
           {area.x + numAreaW - pad - nw, baseline}, nb.CStr(), nColor);   // aligné à DROITE
dl.PopClipRect();
```

Autres `PushClipRect` de l'éditeur : autocomplétion `:4307`/`:4322`, carte de survol
`:4428`/`:4445`/`:4481`, chips `:4547`, gouttière `:4563`, minimap `:4682`,
panneau annexe `:4788`.

---

## 8. Les images et les icônes

### 8.1 Le mécanisme générique

Une image = **une texture backend enregistrée sous un `texId`**, puis un quad
texturé dans la draw-list.

Côté allocation d'id, le shell tient un compteur :

```cpp
// Engine/NKEditorKit/src/NKEditorKit/NkEditorShell.h:392
uint32 mNextTexId = 0x4E4B0100u;  // ids de textures app (logos/icones) — distincts des polices

// NkEditorShell.h:242-257
uint32 UploadRGBA(const uint8 *pixels, int32 w, int32 h) noexcept {
    if (!mRenderer || !pixels || w <= 0 || h <= 0) return 0;
    const uint32 id = mNextTexId++;
    return mRenderer->UploadImageRGBA(id, pixels, w, h) ? id : 0;
}
// Re-uploade des pixels RGBA8 dans une texture DEJA allouee (meme texId).
// Pour le contenu qui change chaque frame (video : NkVideoReader -> onglet)
// sans allouer une nouvelle texture a chaque image. false si texId==0.
bool UpdateRGBA(uint32 texId, const uint8 *pixels, int32 w, int32 h) noexcept;
```

Côté dessin, deux voies :

```cpp
// bas niveau (draw-list) — Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiDrawList.h:71-74
void AddImage(uint32 texId, const NkRect &r, const NkVec2 &uv0, const NkVec2 &uv1,
              const NkColor &tint) noexcept;

// widget auto-layout — NkGuiWidgets.h:96-105
void Image(NkGuiContext &ctx, uint32 texId, float32 w, float32 h,
           NkColor tint = {255,255,255,255}, NkVec2 uv0 = {0,0}, NkVec2 uv1 = {1,1}) noexcept;
bool ImageButton(NkGuiContext &ctx, const char *idStr, uint32 texId, float32 w, float32 h, …) noexcept;
```

`AddImage` réutilise exactement le chemin du texte :

```cpp
// NkGuiDrawList.cpp:155-169
// Quad TEXTURÉ (texId backend) — image/icône. La couleur de sommet `tint`
// multiplie l'échantillon (blanc = image telle quelle). Même chemin que le
// texte (TexturedTriangles), donc le backend résout déjà `texId`.
```

Et côté backend, la résolution est : **atlas de police d'abord, images ensuite**
(`NkGuiCanvasBackend.h:161-174`) — d'où l'importance des plages de `texId`
disjointes (`0x4E4B4654+n` pour les polices, `0x4E4B0100+n` pour les images).

Exemple concret dans la barre de titre :

```cpp
// Engine/NKEditorKit/src/NKEditorKit/NkEditorShell.cpp:949-961
if (mTitleLogoTex) {
    const float32 lg = bar.h * 0.62f;
    if (mTitleLogoAspect > 0.f) {           // logo complet (ratio preserve)
        const float32 lw = lg * mTitleLogoAspect;
        dl.AddImage(mTitleLogoTex, {cursorX, cy - lg*0.5f, lw, lg}, {0.f,0.f}, {1.f,1.f}, {255,255,255,255});
        cursorX += lw + mUI.S(10.f);  wordmark = true;
    } else {                                 // icone carree
        dl.AddImage(mTitleLogoTex, {cursorX, cy - lg*0.5f, lg, lg}, {0.f,0.f}, {1.f,1.f}, {255,255,255,255});
        cursorX += lg + mUI.S(8.f);
    }
}
```

Le shell propose aussi des icônes de barre d'activité **texturées, teintées au
rendu**, avec repli sur un dessin au trait :

```cpp
// NkEditorShell.h:163-173
// Icônes TEXTURE des activity bars (teintées au rendu ; 0 = le dessin au
// trait par défaut reste). left[0..6] = vues gauche, gear = réglages,
// right[0..2] = IA droite.
void SetActivityIcons(const uint32 *left, int32 nLeft, uint32 gear, const uint32 *right, int32 nRight) noexcept;
```

NKCode remplit tout ça dans `nkcode::NkLoadAppIcons(shell.Get(), g_home, g_state)`
(`main.cpp:280`, implémenté dans `Shell/NkAppIcons.h`).

### 8.2 Les icônes de NKCode : trois familles, AUCUNE police d'icônes

**Il n'y a pas de police d'icônes dans NKCode.** Les icônes sont :

1. **des textures RGBA8** uploadées au démarrage à partir de PNG (priorité) ou de SVG
   rasterisés — c'est le gros du système, `Shell/NkAppIcons.h` ;
2. **du dessin au trait vectoriel** en repli — `NkUi::Icon(name, rect, color)`
   (`Shell/NkUi.h:523+`), ~25 icônes façon Lucide dessinées en
   `AddRectFilled`/`AddRect`/`AddLine` ;
3. **des primitives Unicode dans le texte** — `NkDrawGlyphPrim` (§7.7).

```cpp
// Applications/NKCode/src/NKCode/Shell/NkUi.h:381-382
// Jeu d'icones SVG (data/textures/icon) rasterisees en textures au demarrage.
// 0 = non chargee (NkDrawIcon ne dessine rien). Teintees au rendu.
```

L'appel réellement utilisé dans le code combine texture + repli vectoriel
(`Applications/NKCode/src/NKCode/Shell/NkOpenWs.h:784-790`) :

```cpp
// Dessine une icone TEXTURE si disponible, sinon le trace vectoriel en repli.
inline void NkOwIco(const NkUi &u, uint32 texId, const char *drawn, const NkRect &r, const NkColor &c) {
    if (texId) NkDrawIcon(u, texId, r, c);
    else       u.Icon(drawn, r, c);
}
```

### 8.3 `NkLoadAppIcons` : le pipeline de chargement

`Applications/NKCode/src/NKCode/Shell/NkAppIcons.h:19-426`, appelé **après**
`shell->Init()` (l'upload GPU exige le renderer).

**(a) `upload(...)` (`:26-99`)** — fit en préservant l'aspect, **downscale progressif
par demi-pas**, puis `shell->UploadRGBA(...)` :

```cpp
// NkAppIcons.h:44-46
// Downscale PROGRESSIF par demi-pas : un bilinéaire direct 128->~35 ne prend que
// 2x2 texels et CRÉNÈLE le line-art (pas de mipmaps). En halvant (128->64->35),
// chaque étape moyenne 2x2 -> approxime un filtre surface -> icônes NETTES.
```

**(b) `trimAlpha(src)` (`:103-131`)** — bounding box des pixels d'alpha > 10, puis `Crop` :

```cpp
// NkAppIcons.h:100-102
// Rogne les marges TRANSPARENTES (bounding box alpha) -> le glyphe remplit son bitmap.
// Sans ca, une icone 128x128 avec grande marge interne parait plus PETITE qu'une
// icone qui remplit son bitmap (ex. Ouvrir 40x32) -> tailles inegales.
```

**(c) `loadTex(base, …)` (`:151-186`)** — parcourt les dossiers dans l'ordre :

```cpp
const char *dirs[] = {ovrDir, "Applications/NKCode/data/textures/", "data/textures/",
                      "NKCode/data/textures/", exeTex.CStr(), ""};
```

`ovrDir` = `%APPDATA%/NKCode/` (sinon `$HOME/.config/nkcode/`) — l'override utilisateur
gagne. Pour chaque dossier : `<base>.png` d'abord, sinon `<base>.svg` rasterisé au
**double** de la taille cible :

```cpp
NkImage *im = NkSVGCodec::DecodeFromFile(svg.CStr(), tw * 2, th * 2); // rasterise large puis reduit = net
```

**(d) Taille d'upload DPI-aware (`:204-207`)** :

```cpp
const float32 dpi = shell->DpiScale();
int32 IS = (int32)(32.f * dpi + 0.5f);
if (IS < 24) IS = 24; // source 128px -> downscale progressif net
```

**(e) La table unique** (`:209-317`) :

```cpp
// ═══ TABLE UNIQUE des icônes de l'application ═══════════════════════
// TOUTES les icônes nommées se déclarent ICI (champ <- data/textures/…)
// et nulle part ailleurs : une ligne par icône, chargée par la boucle.
struct IconDef { uint32 *slot; const char *path; };
const IconDef kAppIcons[] = {
    {&ic.accueil, "icon/Home"},
    {&ic.ouvrir,  "icon/FolderOpened"},
    …
    {&ic.linux,   "icon/TerminalLinux"},
};
for (const IconDef &d : kAppIcons) *d.slot = loadTex(d.path, IS, IS);
```

≈ 90 entrées. Suivent : les **logos** (`:193-199`, dont
`shell->SetTitleLogo(home.logoIcon, 1.0f)`), les **dossiers spéciaux** (`:318-342`,
table `kDirs` : `FolderSrc`, `FolderTest`, `FolderInclude`, `FolderDocs`,
`FolderResource`, `FolderShader`, `FolderConfig`, `FolderGit`, `FolderScripts`,
`FolderDist`, chargés par paires fermée/ouverte), les **barres d'activité**
(`:344-350`, `shell->SetActivityIcons(L, 7, ic.gear, R, 4)`), et un **registre
d'extensions data-driven** (`:353-425`) : défauts intégrés (`.cpp=Cpp`, `.h=Header`,
`.py=Python`, `.rs=Rust`, `.jenga=Jenga`, `.md=Markdown`…), puis `data/icons.cfg`
livré, puis l'override utilisateur **en dernier**.

### 8.4 La structure `NkIcons` et la résolution nom → texture

`Applications/NKCode/src/NKCode/Shell/NkUi.h:383-517` — des dizaines de champs `uint32`
(0 = non chargée) plus deux registres parallèles, **sans STL** :

```cpp
NkVector<NkString> extKey;  NkVector<uint32> extTex;              // ".cpp" -> texture
NkVector<NkString> dirKey;  NkVector<uint32> dirTexC, dirTexO;    // "src"  -> fermé/ouvert
```

* `SetDir` (`:419-423`), `ForDir(name, open)` (`:426-444`, comparaison **insensible à la casse**) ;
* `ForFile(filename)` (`:447-481`) : extension après le **dernier** point, minusculisée
  dans un `char ext[24]`, comparaison caractère par caractère, 0 si rien ;
* `SetExt(ext, tex)` (`:484-517`) : ajoute **ou remplace**.

### 8.5 Dessiner une icône

```cpp
// Applications/NKCode/src/NKCode/Shell/NkUi.h:518-521
inline void NkDrawIcon(const NkUi &u, uint32 tex, const NkRect &r, const NkColor &tint) {
    if (tex)
        u.dl->AddImage(tex, r, {0, 0}, {1, 1}, tint);
}
```

Chaîne de repli réelle à **quatre niveaux**, dans l'explorateur
(`Applications/NKCode/src/NKCode/Shell/NkExplorer.h:1064-1089`) :

```cpp
// 1) dossier SPÉCIAL Material (coloré, sans teinte) ; 2) dossier
// Material générique ; 3) icône blanche maison teintée ; 4) trait.
uint32 mtex = 0;
if (mS->icons) {
    if (r.root) mtex = r.open ? mS->icons->folderRootOpen : mS->icons->folderRoot;
    else        mtex = mS->icons->ForDir(r.name.CStr(), r.open);
}
if (!mtex && mS->icons) mtex = r.open ? mS->icons->folderMOpen : mS->icons->folderM;
const uint32 tex = mtex ? 0u : (!mS->icons ? 0u : (r.open ? mS->icons->folderOpen : mS->icons->folder));
if (mtex)      dl.AddImage(mtex, ir, {0.f, 0.f}, {1.f, 1.f}, {255, 255, 255, 255});
else if (tex)  dl.AddImage(tex,  ir, {0.f, 0.f}, {1.f, 1.f}, tint);
else if (r.open) { /* repli au trait : dossier ouvert (rabat incliné) */ }
```

> **Règle de teinte** : icône **Material colorée → tint blanc** `{255,255,255,255}`
> (image telle quelle) ; icône **monochrome maison → tint thématique**.
> Taille d'affichage : `const float32 is = rowH - 9.f; // ~16 px : taille façon VSCode` (`:1055`).

---

## 9. Le découpage (clip) et le défilement

### 9.1 Clip

Trois niveaux empilés, tous en **intersection** avec le parent :

| niveau | API | effet |
|---|---|---|
| draw-list NKGui | `dl.PushClipRect(r, intersect=true)` / `PopClipRect()` | pose `clipRect` sur les commandes suivantes |
| interaction | `ItemHoverable` refuse un point hors de `DL().CurrentClip()` | un item scrollé hors-vue ne capte pas la souris |
| GPU | `NkIRenderer2D::SetClip` / `PopClip` | scissor test réel |

```cpp
// Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiDrawList.cpp:38-50
void NkGuiDrawList::PushClipRect(const NkRect &r, bool intersect) noexcept {
    NkRect c = r;
    if (intersect && clipDepth > 0) { /* intersection avec clipStack[clipDepth-1] */ }
    if (clipDepth < 32) clipStack[clipDepth++] = c;
}
```

Profondeur maximale : **32** (`NkGuiDrawList.h:52`). Le rect « pas de clip » est
`{0, 0, 1e9, 1e9}` et le backend le reconnaît par `w < 1e8 && h < 1e8`
(`NkGuiCanvasBackend.h:144`).

L'idiome minimal, tel qu'on le trouve partout dans NKCode :

```cpp
// Applications/NKCode/src/NKCode/Shell/NkAppCommands.h:344-354
u.dl->PushClipRect(body, true);
float32 y = body.y - d->helpScroll;
for (int32 i = 0; i < N; ++i) {
    if (y + rowH >= body.y && y <= body.y + body.h && SC[i].k[0]) {   // culling manuel
        u.Text(body.x,          y + u.s(4), SC[i].k, nkcode::NkCol::primary);
        u.Text(body.x + keyW,   y + u.s(4), SC[i].a, nkcode::NkCol::foreground);
    }
    y += SC[i].k[0] ? rowH : u.s(10.f);
}
u.dl->PopClipRect();
```

> Noter le **double filtrage** : le clip garantit la correction visuelle, le test
> `if (y + rowH >= body.y && y <= body.y + body.h)` évite d'émettre des milliers de
> quads inutiles. Les deux sont systématiques dans NKCode.

### 9.2 Défilement — voie 1 : `BeginChild` / `EndChild` de NKGui

`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.cpp:3425-3483` (`BeginScrollFrame`) :

```cpp
static bool BeginScrollFrame(NkGuiContext &ctx, NkGuiId id, const NkRect &area, bool horizontal, bool fillWidth) {
    if (ctx.childDepth >= NkGuiContext::ChildMax) return false;   // max 8 niveaux
    NkGuiScrollState st = ScrollGet(ctx, id);

    if (NkGuiRectContains(area, ctx.input.mousePos)) {
        bool modHeld = …;                                          // ctx.hscrollMod (défaut Maj)
        if (horizontal && (modHeld || ctx.input.wheelH != 0.f))
            st.x -= (ctx.input.wheelH != 0.f ? ctx.input.wheelH : ctx.input.wheel) * 36.f;
        else
            st.y -= ctx.input.wheel * 36.f;
    }
    // Bornage par les MAX de la frame précédente → empêche l'overscroll (sinon
    // 1 frame de contenu décalé puis re-clamp à End = clignotement en butée).
    …clamp st.x/st.y…

    const float32 gV = st.barV ? kScrollBarW : 0.f;   // gouttières (frame préc.)
    const float32 gH = (horizontal && st.barH) ? kScrollBarW : 0.f;
    const NkRect inner = {area.x, area.y, area.w - gV, area.h - gH};

    NkGuiChildFrame &f = ctx.childStack[ctx.childDepth++];
    …
    f.contentTop  = area.y - st.y;
    f.contentLeft = inner.x - st.x;
    f.savedLayout = ctx.layout;

    ctx.DL().PushClipRect(inner, true);
    const float32 regionW = (horizontal && !fillWidth) ? 1.0e6f : inner.w;
    ctx.BeginLayout({f.contentLeft, f.contentTop, regionW, 1.0e6f});
    return true;
}
```

`EndScrollFrame` (`:3485+`) mesure le contenu réellement dessiné
(`ctx.layout.cursor.y - f.contentTop`), en déduit `maxY`/`maxX`, dépile le clip et
dessine les barres. `kScrollBarW = 12.f` (`NkGuiWidgets.cpp:3420`).

> **Le mécanisme « bornes de la frame précédente » (`NkGuiScrollState::maxX/maxY`,
> `NkGuiContext.h:99-103`) est l'invariant à retenir : sans lui, le contenu
> clignote en butée.**

### 9.3 Défilement — voie 2 : la scrollbar réutilisable de NKEditorKit

`Engine/NKEditorKit/src/NKEditorKit/NkEditorScrollbar.h` — **c'est celle que
l'éditeur, la sortie et le terminal utilisent** :

```cpp
// NkEditorScrollbar.h:2-8
// @Brief   Scrollbar STANDARD reutilisable (vertical + horizontal). C'est le
//          scrollbar de l'editeur de code EXTRAIT TEL QUEL (memes couleurs, memes
//          fleches, memes tailles) -> une seule barre pour TOUTE l'UI Nkentseu.
//          NE PAS "ameliorer" le rendu : il doit rester identique a l'editeur.
```

Signature et logique de drag :

```cpp
// NkEditorScrollbar.h:82-126
inline bool NkVScrollbar(NkGuiContext &ctx, NkGuiDrawList &dl, const NkRect &track, float32 &scroll,
                         float32 contentLen, float32 viewLen, uint32 id,
                         float32 lineStep = 0.f, bool arrows = true) {
    …
    if (ctx.input.mouseClicked[0] && NkGuiRectContains(inner, m)) ctx.activeId = id;
    const bool act = (ctx.activeId == id);
    if (act && ctx.input.mouseDown[0] && inner.h - th > 0.f) {
        const float32 t = (m.y - inner.y - th * 0.5f) / (inner.h - th);
        scroll = (t < 0.f ? 0.f : t > 1.f ? 1.f : t) * maxScroll;
    }
    dl.AddRectFilled(thumb, (act || NkGuiRectContains(inner, m)) ? c.thumbHover : c.thumb, 3.f);
    …
    if (ctx.activeId == id && !ctx.input.mouseDown[0]) ctx.activeId = 0;  // relache le drag
    if (scroll < 0.f) scroll = 0.f;
    if (scroll > maxScroll) scroll = maxScroll;
    return scroll != before;
}
```

> **Idiome de drag NKGui** : poser `ctx.activeId = <mon id>` à l'appui, agir tant
> que `ctx.activeId == <mon id> && mouseDown[0]`, et **remettre `ctx.activeId = 0`
> au relâchement**. C'est ce qui garantit qu'un seul élément à la fois « capture »
> la souris (cf. `ItemHoverable`, qui refuse tout survol si un autre `activeId` est posé).

### 9.4 Défilement — voie 3 : à la main (le plus fréquent dans NKCode)

```cpp
// Applications/NKCode/src/NKCode/Shell/NkAppCommands.h:335-343
if (u.Hit(body) && ctx.input.wheel != 0.f) {
    d->helpScroll -= ctx.input.wheel * rowH;
    ctx.input.wheel = 0.f;              // CONSOMMÉ : personne d'autre ne le verra
}
const float32 maxS = contentH - body.h > 0.f ? contentH - body.h : 0.f;
if (d->helpScroll < 0.f)   d->helpScroll = 0.f;
if (d->helpScroll > maxS)  d->helpScroll = maxS;
```

Trois gestes obligatoires : (1) tester le survol de la zone, (2) **remettre
`ctx.input.wheel` à zéro** pour éviter le double défilement, (3) borner.

La version complète, canonique, est dans `OutputPanel::DrawConsole`
(`Applications/NKCode/src/NKCode/Shell/Panels.h:2061-2088`) :

```cpp
const float32 contentH = nLines * lineH + topPad + botPad;
const float32 maxSY = contentH > viewH ? contentH - viewH : 0.f;
const float32 maxSX = mMaxW > viewW ? mMaxW - viewW : 0.f;
if (in(out)) {
    if (ctx.input.wheel != 0.f) {
        mScrollY -= ctx.input.wheel * lineH * 3.f;
        ctx.input.wheel = 0.f;              // CONSOMMÉE
        mFollow = false;                    // l'utilisateur a repris la main
    }
    if (ctx.input.wheelH != 0.f) { mScrollX -= ctx.input.wheelH * 40.f; ctx.input.wheelH = 0.f; }
}
if (mFollow) mScrollY = maxSY;              // « coller au bas »
if (mScrollY < 0.f)     mScrollY = 0.f;
if (mScrollY > maxSY)   mScrollY = maxSY;
```

Trois invariants supplémentaires visibles ici :
* le clamp est **répété après le dessin** (`Panels.h:2245-2252`), parce que les
  scrollbars ont pu bouger `mScroll*` entre-temps ;
* pas de conteneur dynamique dans la boucle : on calcule `first`/`last` et on saute
  le hors-vue ;
* le « coller au bas » a sa propre borne pour éviter le va-et-vient
  (`Panels.h:2798-2805`) :

```cpp
// « Coller au bas » = afficher l'ECRAN (les rows dernieres lignes) epingle.
// On ne defile QUE dans le scrollback : borne basse = followY. Pas de marge
// basse over-scrollable -> evite le va-et-vient (clignotement) au scroll bas.
```

**Clamp AVANT le dessin** (anti-clignotement), quand `scrollMax` vient de la frame
précédente (`Applications/NKCode/src/NKCode/Shell/NkHome.h:726-733`) :

```cpp
if (u.Hit(listArea) && u.ctx->input.wheel != 0.f) {
    H->scroll -= u.ctx->input.wheel * u.s(34);
    u.ctx->input.wheel = 0.f;
    if (H->scroll < 0.f)            H->scroll = 0.f;
    if (H->scroll > H->scrollMax)   H->scroll = H->scrollMax; // borne AVANT le dessin (anti-clignotement)
}
```

avec, dans l'état (`NkHome.h:41-42`) :

```cpp
float32 scrollMax  = 0.f;  // borne max du centre (frame precedente) -> anti-clignotement
float32 scrollRMax = 0.f;  // borne max de droite (frame precedente)
```

**Auto-scroll vers l'élément sélectionné** (navigation clavier, `Toolbar.h:472-475`) :

```cpp
// Auto-scroll : garde la ligne sélectionnée visible (navigation clavier).
const float32 selTop = (float32)s->tbQoSel * ddRowH;
if (selTop < s->tbQoScrollY)                 s->tbQoScrollY = selTop;
if (selTop + ddRowH > s->tbQoScrollY + viewH) s->tbQoScrollY = selTop + ddRowH - viewH;
```

**Saut du hors-vue**, deux dialectes équivalents :

```cpp
// NkExplorer.h:1028-1029
if (row.y + rowH < clip.y || row.y > clip.y + clip.h)
    continue; // hors vue : la place est réservée, pas de dessin
// Toolbar.h:485
if (rowY + ddRowH <= clip.y || rowY >= clip.y + clip.h) continue; // hors vue
```

Les scrollbars sont soit maison (flèches + piste + pouce, `Panels.h:2173-2252`), soit
le widget partagé de NKEditorKit avec un `id` magique unique :
`editorkit::NkVScrollbar(ctx, dl, track, scroll, contentH, viewH, 0x5EA3C401u, step)`
(`Toolbar.h:508`, `:512`, `:655`, `:726`, `NkHome.h:106-107`), largeur canonique
`editorkit::NkScrollbarWidth()` (= 14 px).

---

## 10. Docking, panneaux et « qui dessine où »

### 10.1 Un panneau NKCode

```cpp
// Engine/NKEditorKit/src/NKEditorKit/NkEditorPanel.h:22-83
class NkEditorPanel {
  public:
    explicit NkEditorPanel(const char *title, NkEditorDockSide defaultSide = NK_CENTER) noexcept;
    const char *Title() const noexcept;
    bool *OpenPtr() noexcept;  bool IsOpen() const;  void SetOpen(bool);
    bool Dockable() const;     NkEditorDockSide DefaultSide() const;

    // Appele par le shell entre Begin/End (flottant) ou BeginDocked/EndDocked
    // (ancre). Dessiner via les helpers de `ec` (ec.Text, ec.Button, ...).
    virtual void OnUI(NkEditorFrameContext &ec) = 0;

    // Actions du panneau dessinees sur la BARRE D'ONGLETS du dock (a droite),
    // quand ce panneau est l'onglet ACTIF. Defaut : rien. (Ex. Terminal : +/combo.)
    virtual void OnTabBarActions(nkgui::NkGuiContext &ctx, const nkgui::NkRect &tabBar) noexcept;
  protected:
    char mTitle[64] = {};
    bool mOpen = true, mDockable = true;
    NkEditorDockSide mDefaultSide = NK_CENTER;
};
```

Le contexte de frame est **volontairement minuscule** :

```cpp
// Engine/NKEditorKit/src/NKEditorKit/NkEditorContext.h:26-55
struct NkEditorFrameContext {
    nkgui::NkGuiContext *ui = nullptr;  ///< contexte NKGui courant
    float32 dt = 0.f;                   ///< delta time de la frame
    nkgui::NkGuiContext &Ui() const noexcept { return *ui; }
    void Text(const char *s) const noexcept        { nkgui::Text(*ui, s); }
    void Separator() const noexcept                { nkgui::Separator(*ui); }
    bool Button(const char *label) const noexcept  { return nkgui::Button(*ui, label); }
    bool Checkbox(const char *l, bool &v) const;
    bool SliderFloat(const char *l, float32 &v, float32 a, float32 b) const;
};
```

avec la justification :

```cpp
// NkEditorContext.h:8-14
// NkEditorFrameContext = le contexte NKGui courant + le delta time. Sur NKGui,
// tout (fenetres, dock, layout, police, draw lists) vit DANS NkGuiContext : ce
// contexte se reduit donc a un pointeur + dt, avec des helpers minces (l'API
// NKGui etant deja terse : `ec.Text("x")` -> `nkgui::Text(*ui, "x")`).
//
// L'Editor Kit ne REIMPLEMENTE pas l'UI : il ASSEMBLE NKGui (docking, fenetres,
// widgets deja complets) dans un cadre « application d'edition ».
```

### 10.2 Comment un panneau obtient sa zone de dessin

```cpp
// Engine/NKEditorKit/src/NKEditorKit/NkEditorShell.cpp:1985-2019
void NkEditorShell::DrawPanels(NkEditorFrameContext &ec) noexcept {
    for (int32 i = 0; i < mNumPanels; ++i) {
        NkEditorPanel *p = mPanels[i];
        if (!p->IsOpen()) continue;
        if (mDockBootstrap && !p->Dockable()) { SetNextWindowPos(…); SetNextWindowSize(…); }
        if (Begin(mUI, p->Title(), p->OpenPtr())) {
            // Une fenêtre FLOTTANTE recouvre la souris et ce n'est pas la nôtre ->
            // souris neutralisée pendant OnUI : le code custom des panneaux (éditeur,
            // arbres) lit l'input en direct et recevrait sinon clics/molette À TRAVERS
            // la fenêtre du dessus — et lui volerait son drag de barre de titre.
            const bool shielded = mUI.hoveredWindowId != NKGUI_ID_NONE
                               && mUI.hoveredWindowId != mUI.curWindowId;
            nkgui::NkGuiInput saved;
            if (shielded) { saved = mUI.input; /* souris envoyée à -100000 */ … }
            p->OnUI(ec);
            if (shielded) mUI.input = saved;
            EndWindow(mUI);
        }
    }
}
```

Et dans `Begin`, pour une fenêtre **ancrée** :

```cpp
// Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.cpp:2224-2235
bool BeginDockedTabContent(NkGuiContext &ctx, NkGuiWindowMeta *m, NkGuiId id, int32 dlSlot, NkGuiId hostWinId) {
    ctx.curWindow   = dlSlot;
    ctx.curWindowId = (dlSlot >= 0) ? hostWinId : NKGUI_ID_NONE;
    ctx.curWindowDocked = false;
    if (!m->dockActiveTab) return false;      // onglet inactif → contenu caché
    ctx.curWindowDocked = true;
    ctx.winSavedLayout = ctx.layout;
    BeginScrollFrame(ctx, id ^ 0x5555u, m->dockRect, /*horizontal=*/true, /*fillWidth=*/true);
    return true;
}
```

**Conséquence pratique pour un panneau :** entre `Begin` et `EndWindow`,
* `ctx.layout.region` est la zone de contenu (déjà décalée par le scroll) ;
* un clip est déjà posé sur cette zone ;
* `ctx.DL()` route vers `winDL[curWindow]` — la draw-list PROPRE à la fenêtre,
  qui sera fusionnée par z-order dans `EndFrame`.

C'est pourquoi un panneau qui écrirait dans `ctx.dl` directement se dessinerait
**sous** tout le reste et **sans clip**.

### 10.3 Bootstrap du docking

```cpp
// NkEditorShell.cpp:2022-2048
void NkEditorShell::BootstrapDocking() noexcept {
    if (!mDockBootstrap) return;
    // Centre d'abord, puis les côtés. Les panneaux d'un MÊME côté sont
    // regroupés en ONGLETS (ex. Terminal + Sortie partagent la barre du bas).
    for (int32 pass = 0; pass < 2; ++pass) { … DockBuilderDock / DockBuilderDockTab … }
    mDockBootstrap = false;
}
```

`ResetLayout()` remet simplement `mDockBootstrap = true` (`NkEditorShell.h:276-278`).

### 10.4 Les DEUX styles de panneau observés dans NKCode

**Style A — « flux »** : on empile des widgets NKGui, le curseur de layout fait le
placement. Le plus court. Exemple `OutlinePanel`
(`Applications/NKCode/src/NKCode/Shell/Panels.h:226-264`) :

```cpp
class OutlinePanel : public NkEditorPanel {
  public:
    explicit OutlinePanel(NkCodeState *s)
        : NkEditorPanel("Structure", NkEditorDockSide::NK_LEFT), mS(s) {}
    void OnUI(NkEditorFrameContext &ec) override {
        auto &ctx = ec.Ui();
        ec.Text(NkT("outline.title"));
        ec.Separator();
        …
        if (Selectable(ctx, lbl, false)) { … }
    }
};
```

**Style B — « dessin direct »** : c'est le style DOMINANT (Éditeur, Sortie, Terminal,
Explorateur). L'incantation d'ouverture est toujours la même — **les trois premières
lignes de tout panneau NKCode** :

```cpp
void OnUI(NkEditorFrameContext &ec) override {
    auto &ctx = ec.Ui();
    auto &dl  = ctx.DL();
    const NkRect clip = dl.CurrentClip();     // <-- LA zone de dessin
    dl.AddRectFilled(clip, ctx.theme.bgPrimary);
    …
}
```

Occurrences : `OutputPanel` `Panels.h:1729-1749`, `TerminalPanel` `Panels.h:2292-2296`,
`ExplorerPanel` `NkExplorer.h:35-37` puis `:67`.

> **Il n'existe NI `ec.dl` NI `ec.WindowDrawList()`.** On passe toujours par
> `ec.Ui()` puis `ctx.DL()`.

### 10.5 Le piège n°1 du débutant : `CurrentClip()` vs `ContentWidth()/AvailHeight()`

`Applications/NKCode/src/NKCode/Shell/Panels.h:676-685` (EditorPanel), mot pour mot :

```cpp
OpenFile &f = mS->files[mS->active];
// AvailHeight()/ContentWidth() = taille du CONTENU (scrollable, ~1e9), PAS
// la taille visible -> on borne par le rect de CLIP (zone visible du dock)
// sinon viewH gigantesque (pas de scrollbar, barre H hors ecran).
const NkRect clip = ctx.DL().CurrentClip();
NkRect r = {ctx.layout.cursor.x, ctx.layout.cursor.y, ctx.ContentWidth(), ctx.AvailHeight()};
if (r.x + r.w > clip.x + clip.w) r.w = clip.x + clip.w - r.x;
if (r.y + r.h > clip.y + clip.h) r.h = clip.y + clip.h - r.y;
```

> **Règle : `CurrentClip()` = le VISIBLE ; `ContentWidth()/AvailHeight()` = le
> CONTENU (potentiellement ~1e6). Toujours intersecter.**

Le patron « bandeau qui rogne le reste » se répète (bannière mojibake `Panels.h:735-737`,
bannière fichier supprimé `:767-769`, fil d'Ariane `:689-694`) :

```cpp
r.y += bh;
if (r.h > bh) r.h -= bh;
```

Autre variante, pour une zone donnée par le shell : `const NkRect r = ec.Ui().layout.region;`
(barre d'outils, `Toolbar.h:84`).

Pour remettre le curseur de layout en place après un dessin « hors flux »
(`Panels.h:531-535`) :

```cpp
// Avance le layout du shell : la LISTE en dessous profite du scroll de la fenêtre.
ctx.layout.cursor.x   = x0;
ctx.layout.cursor.y   = y;
ctx.layout.lineStartX = x0;
ctx.layout.curLineH   = 0.f;
```

Et pour **réserver** de la place dans le flux sans rien dessiner :
`ctx.NextItemRect(ctx.ContentWidth(), headH); // réserve du flux` (`NkExplorer.h:81`).

L'en-tête fixe de l'explorateur explique bien la technique (`NkExplorer.h:64-66`) :

```cpp
// L'EN-TÊTE est FIXE (il ne défile pas) : l'espace est réservé dans le
// flux, les rows défilent DESSOUS, le chrome est dessiné PAR-DESSUS.
// Disposition : [titre + actions] / [barre de recherche PERMANENTE] / [arbre].
```

### 10.6 `NkUi` : la boîte à outils de dessin de NKCode

`Applications/NKCode/src/NKCode/Shell/NkUi.h` (691 lignes). En-tête (`:2-6`) :

```cpp
// NkUi.h — Tokens de design + helpers de dessin NKGui (reecriture propre de l'UI).
// Source : design system Banani « NKCode IDE ». Palette GitHub-dark + accents.
// Tout est dessine en primitives NKGui -> identique sur toutes les plateformes.
```

La structure capture, **une fois par frame**, tout ce dont on a besoin (`:249-374`) :

```cpp
struct NkUi {
    NkGuiContext *ctx; NkGuiDrawList *dl; const NkGuiFont *f;
    NkVec2 mp{}; bool click = false, down = false; float32 S = 1.f;

    static NkUi From(editorkit::NkEditorFrameContext &ec, bool overlay = false) {
        NkUi u;
        u.ctx   = &ec.Ui();
        u.f     = u.ctx->font;
        u.dl    = overlay ? &u.ctx->dlOverlay : &u.ctx->DL();   // choix de la COUCHE
        u.mp    = u.ctx->input.mousePos;
        u.click = u.ctx->input.mouseClicked[0];
        u.down  = u.ctx->input.mouseDown[0];
        u.S     = u.ctx->S(1.f);
        return u;
    }
```

| ligne | membre | rôle |
|---|---|---|
| 269 | `Valid()` | `f && f->Valid()` — la garde police, une fois pour toutes |
| 273 | `s(px)` | `px * S` — mise à l'échelle DPI |
| **288** | **`Hit(r)`** | `NkGuiRectContains(r, mp) && (!ctx \|\| ctx->PointReachable(mp))` — **LE** hit-test |
| 292-302 | `Asc()`, `Lh()`, `TextW(t)` | métriques, repli `0.f` sans police |
| 304 | `Rect(r, c, round)` | `dl->AddRectFilled` |
| 308 | `Stroke(r, c, round, th)` | `dl->AddRect` (`round` ignoré : `(void)round;`) |
| 315 | `Panel(r, fill, border, round, bw)` | fond bordé à coins arrondis (2 `AddRectFilled` imbriqués) |
| 323 | `Text(x, y, t, c)` | **prend le HAUT du texte** : ajoute `Asc()` en interne |
| 329 | `TextV(x, y, h, t, c)` | texte centré verticalement dans une hauteur `h` |
| 334 | `TextEllipsis(x, y, maxW, t, c)` | troncature `…`, buffer `char buf[260]` borné à 252 |
| 363 | `Button(r, label, bg, bgH, fg, round)` | bouton plein, libellé centré, `true` au clic |

**Le commentaire le plus important du fichier** (`NkUi.h:277-290`) :

```cpp
// Survol — INSCRIT AU ROUTEUR D'OCCLUSION (cf. NkGuiContext::
// PushOcclusion / PointReachable). `NkUi::Hit` est LE point de
// hit-test de toute l'UI « maison » (launcher, wizard Nouveau
// Workspace, Paramètres, Toolchains, Plateformes, toolbar…) : le
// rendre conscient de l'occlusion migre ~180 points d'interaction
// d'un coup, au lieu de les protéger un par un.
// Une surface flottante déclare son rect via PushOcclusion(rect,
// couche) ; tout Hit() d'une couche INFÉRIEURE sous ce rect renvoie
// alors false — plus de clic qui « traverse » vers le panneau du
// dessous. Les surfaces qui dessinent leur propre contenu ouvrent un
// NkInputLayerScope, donc leurs propres Hit() passent normalement.
```

Fonctions libres du même fichier :

| ligne | fonction | raison d'être (citée) |
|---|---|---|
| 25 | `NkCodeVersion()` | source unique de version (cf. §13) |
| 96 | `NkParseHex(h, fallback)` | `"#RRGGBB"` → `NkColor` |
| 118 | `NkApplyTheme(id, accentHex)` | « Applique le thème + la couleur d'accent perso (temps réel). A appeler chaque frame. » |
| 143 | `NkApplyEditorTheme(ctx, id, accentHex)` | « tout le chrome de l'éditeur … suit Dark/Light. A appeler chaque frame. `rounding`/`framePad*` sont conservés. » |
| 196 | `NkThemeIsLight()` | `(bg.r + bg.g + bg.b) > 384` |
| 203 | `NkScrim(a)` | « dim NOIR sur thème sombre …, mais dim beaucoup plus LÉGER + teinté sur thème clair (sinon le noir opaque « éteint » toute la page). » |
| 212-228 | `NkScrollTrack(C)` / `NkScrollThumb(C)` | « Scrollbars UNIFORMES + VISIBLES (tout NKCode : launcher + editeur). » |
| 232 | `NkColHover(c)` | « Remplace les bleus/verts de survol codés en dur (qui ne suivaient pas le thème). » |
| 377 | `NkLine(u, a, b, c, th)` | « Segment au trait (le draw-list NKGui gere l'epaisseur + l'AA). » |
| 518 | `NkDrawIcon(u, tex, r, tint)` | no-op si `tex == 0` |
| 523 | `NkUi::Icon(name, box, color)` | ~25 icônes Lucide dessinées au trait, `strokeBox` en repli (`:648`) |
| 653 | `NkBrandMark(u, r, c)` | logo NKCode vectoriel : accolades `{ }` + arbre |

### 10.7 Les DEUX palettes de couleurs coexistantes

C'est une source de confusion pour un débutant, il faut le dire explicitement :

| source | portée | fichiers |
|---|---|---|
| `ctx.theme.*` (`NkGuiTheme`) | chrome de l'editorkit + panneaux « dessin direct » | `Panels.h`, `NkExplorer.h` |
| `NkCol::*` (globales **mutables**) | UI « maison » NKCode : launcher, toolbar, wizards | `NkUi.h:32-51`, `Toolbar.h`, `NkHome.h` |

Le pont est refait **chaque frame** : `NkApplyTheme(id, accent)` réécrit `NkCol::*` ;
`NkApplyEditorTheme(ctx, id, accent)` réécrit `ctx.theme` **et** `ctx.syntax`
(appelé depuis `AppFlagsThunk`, `NkAppCommands.h:471`).

```cpp
// Applications/NKCode/src/NKCode/Shell/NkUi.h:30-31
// MUTABLE (pas constexpr) : le thème actif (Paramètres > Thème) réécrit ces
// valeurs à chaud via NkApplyTheme(). Tout le dessin lit NkCol::X chaque frame.
```

### 10.8 Rectangle + texte : le patron répété partout

Sans `NkUi` (dans les panneaux), `Panels.h:1766-1771` :

```cpp
const bool h = inR(clrR);
dl.AddRectFilled(clrR, h ? ctx.theme.buttonHover : ctx.theme.button, 4.f);
if (ctx.font && ctx.font->Valid())
    dl.AddText(ctx.font->Face(), ctx.font->TexId(), {clrR.x + 10.f, by}, "Effacer", ctx.theme.text);
if (h && ctx.input.mouseClicked[0] && ctx.popupDepth == 0) { … }
```

La garde `if (ctx.font && ctx.font->Valid())` est **systématique**, avec repli chiffré
(`Panels.h:39-40`, `:698-699`, `:1752`, `:2457`) :

```cpp
const float32 lineH = (ctx.font && ctx.font->Valid()) ? ctx.font->LineHeight() : 16.f;
const float32 asc   = (ctx.font && ctx.font->Valid()) ? ctx.font->Ascent()     : 12.f;
```

Le survol est factorisé en lambda locale (`Panels.h:1754-1756`, `:2065-2067`, `:2490-2492`) :

```cpp
auto inR = [&](const NkRect &r) {
    return m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h;
};
```

Et le clic est **toujours** ce triplet (27 occurrences dans `Panels.h`) :

```cpp
if (hov && ctx.input.mouseClicked[0] && ctx.popupDepth == 0) { … }
```

Boutons de souris : `[0]` gauche, `[1]` droit (menu contextuel), **`[2]` milieu —
fermer un onglet au clic-molette** (`Panels.h:1317`).

### 10.9 Le motif « focus par clic »

Répété à l'identique partout :

```cpp
if (ctx.input.mouseClicked[0]) mFocus = NkGuiRectContains(clip, m);        // NkExplorer.h:1011-1012
if (u.click && tb.open < 0)    s->tbSearchFocus = (u.Hit(sb) || hitDD);    // Toolbar.h:450-451
```

puis les raccourcis sont gardés par ce focus :
`if (mFocus && !mFilterOn && !mPeekOpen) { … }` (`NkExplorer.h:1356`).

Saisie de texte brute (`NkExplorer.h:970-993`) :

```cpp
for (int32 i = 0; i < ctx.input.charCount; ++i) {
    const uint32 cp = ctx.input.chars[i];
    if (cp >= 32 && cp < 127 && len < 62) { mFilter[len++] = (char)cp; changed = true; }
}
if (ctx.input.KeyPressed(NkGuiKey::Backspace) && len > 0) { --len; changed = true; }
```

> Astuce répétée : **la barre Espace se détecte via `chars[i] == 32`**, pas via une
> touche (`NkExplorer.h:1364-1369`, `:1608-1613`).

Pour un champ complet, on délègue :
`editorkit::NkOverlayTextField(ctx, dl, font, rect, buf, cap, focused)`
(`Panels.h:487`, `:493`, `:3403`, `Toolbar.h:402`, `:452`) — caret, sélection,
copier-coller inclus.

### 10.10 Les actions différées (invariant de conception)

On n'exécute **jamais** une action lourde ou mutante pendant le dessin : on pose un
drapeau, consommé par un `Poll*` de la frame suivante.

```cpp
// Applications/NKCode/src/NKCode/Shell/Panels.h:558-561
if (Selectable(ctx, row.CStr(), false)) { // ouverture DIFFÉRÉE (poll) : jamais OpenPath au rendu
    mS->wsOpenFile = h.file;
    mS->wsOpenLine = h.line;
}
```

```cpp
// NkExplorer.h:1331
// Mutations APRÈS la boucle (mRows est reconstruit par BuildRows).
```

```cpp
// NkMenuBar.h:201-202
// Resultat ASYNCHRONE du picker « Ouvrir un fichier » (PK_File remplit le
// buffer a la confirmation, une frame plus tard) -> ouverture ici.
```

---

## 11. Le masquage modal : comment NKCode empêche les clics de « traverser »

C'est un thème qui revient à **quatre** endroits différents ; il faut le traiter
comme un chapitre à part entière.

### 11.1 Au niveau du shell (une fois par frame)

```cpp
// NkEditorShell.cpp:661-688
// MODALE : quand Preferences est ouvert, le corps (panneaux/editeur)
// ne doit pas reagir aux clics/molette/frappes destines au popup. On
// masque l'input pour le corps puis on le restaure pour DrawPreferences.
// IDEM quand la souris est au-dessus d'un menu DEROULANT ouvert (deja
// dessine + gere dans la barre de titre ci-dessus) : sinon l'editeur /
// les zones a hit-test « brut » derriere le menu recoivent les clics.
bool overPopup = false;
for (int32 i = 0; i < mUI.popupDepth; ++i)
    if (nkgui::NkGuiRectContains(mUI.popupRects[i], mUI.input.mousePos)) { overPopup = true; break; }
const bool modal = mShowPrefs || mUI.appModal || overPopup || mCtxOpen;
nkgui::NkGuiInput savedInput;
if (modal) {
    savedInput = mUI.input;
    mPopupMasked = overPopup && !mShowPrefs && !mUI.appModal; // cf. dockHeaderFn
    mRealInput = savedInput;
    mUI.input.mousePos = {-100000.f, -100000.f};             // souris « nulle part »
    for (int32 i = 0; i < 3; ++i) { mouseClicked/mouseDown/mouseDoubleClicked = false; }
    mUI.input.wheel = mUI.input.wheelH = 0.f;
    mUI.input.charCount = 0;
    mUI.input.wantCopy = … = false;
}
```

Technique : **on téléporte la souris à (-100000, -100000)** et on efface les
transitions. Restauration en ligne 710-712, avant les overlays.

### 11.2 L'exception documentée (barre d'onglets)

```cpp
// NkEditorShell.cpp:203-206
// Le masquage anti clic-a-travers (souris sur un popup) vise les corps en
// hit-test brut ; les ACTIONS de barre d onglets (vrais widgets NKGui, dont
// le combo de shells et SON popup) recoivent l input REEL, sinon le menu
// deroulant ouvert par le panneau est injouable a la souris.
if (self->mPopupMasked) {
    const nkgui::NkGuiInput masked = c.input;
    c.input = self->mRealInput;
    self->mPanels[i]->OnTabBarActions(c, innerBar);
    c.input = masked;
} else self->mPanels[i]->OnTabBarActions(c, innerBar);
```

### 11.3 Au niveau d'une modale applicative NKCode

Recette **littérale** répétée par `DrawPrefsModal`, `DrawNewWsModal`, `DrawHelpModal`
(`Applications/NKCode/src/NKCode/Shell/NkAppCommands.h:137-361`) :

```cpp
nkcode::NkUi u = nkcode::NkUi::From(ec, /*overlay=*/true);   // 1. draw-list OVERLAY
if (!u.Valid()) return;
NkGuiContext &ctx = *u.ctx;
const NkRect box = { (W-pw)*0.5f, (Vh-ph)*0.5f, pw, ph };

ctx.PushOcclusion(box, 100);                                  // 2. routeur d'occlusion, couche 100
NkGuiContext::NkInputLayerScope _layer(ctx, 100);             // 3. RAII : mes hit-tests passent

// MODALITE GLOBALE (meme mecanisme que NkModalDraw) : popupDepth > 0 est
// LE signal verifie par les points d'interaction des panneaux NKCode —
// sans lui les clics TRAVERSENT vers l'editeur/l'explorateur derriere.
// popupRects[0] doit pointer la modale, sinon Update() (debut de frame)
// retombe a 0 au premier clic et un panneau traite avant nous reagit.
if (ctx.popupDepth == 0) ctx.popupDepth = 1;                  // 4. modalité globale
ctx.popupRects[0]  = box;
ctx.popupAnchor    = box;

u.Rect({0.f, 0.f, W, Vh}, NkColor{0, 0, 0, 150});             // 5. backdrop assombri
if (u.click && !u.Hit(box) && !d->pickerOpen) {               // 6. clic dehors = fermer
    d->showPrefs = false;  ctx.popupDepth = 0;  return;       //    (+ libérer la modalité)
}
u.Panel(box, NkCol::background, NkCol::border, NkR::lg);      // 7. cadre
…contenu…
ctx.appModal = true;                                          // 8. le shell masquera le corps
```

Les **8 gestes** ci-dessus forment un idiome complet, à reproduire tel quel.
Noter que `ctx.appModal` est **app-géré** :

```cpp
// Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiContext.h:261-265
// Modale applicative : l'app (ex. NKCode) leve ce flag tant qu'un dialogue
// modal (creation de projet, proprietes...) est ouvert. Le shell masque
// alors l'input du corps (panneaux) au profit de l'overlay. App-gere
// (mis a true a l'ouverture, false a la fermeture) — le shell ne le reset pas.
bool appModal = false;
```

### 11.4 ⚠ LA règle absolue : ne JAMAIS écraser `ctx.input.mousePos` de façon persistante

`Applications/NKCode/src/NKCode/Shell/NkExplorer.h:89-98` — le commentaire le plus
important du dépôt sur l'entrée :

```cpp
// PEEK OUVERT = MODAL : l'explorateur est le 1er panneau dessiné ; on
// neutralise l'input APRÈS le peek -> les panneaux suivants (éditeur,
// terminal…) ne reçoivent aucun CLIC sous l'overlay. Le peek, lui, a
// utilisé l'input RÉEL (même chemin que DrawRows).
// ⚠ NE PAS toucher mousePos : il n'est mis à jour QUE sur un événement
// de DÉPLACEMENT (jamais re-sondé) -> l'écraser le GÈLE pour toute
// l'app à la frame suivante (clic sans bouger = position figée). On
// neutralise donc SEULEMENT les clics/molette/frappes.
```

La neutralisation correcte, **sélective** (`NkExplorer.h:99-107`) :

```cpp
if (mPeekOpen || mDelMenu.open) {
    for (int32 b = 0; b < 3; ++b) {
        ctx.input.mouseClicked[b] = false;
        ctx.input.mouseDown[b] = false;
        ctx.input.mouseDoubleClicked[b] = false;
    }
    ctx.input.wheel = ctx.input.wheelH = 0.f;
    ctx.input.charCount = 0;
}
```

Le corollaire, côté barre d'outils (`Applications/NKCode/src/NKCode/Shell/Toolbar.h:798-806`) :

```cpp
// MODAL : tant qu'un dropdown est (ou vient d'être) ouvert, le corps
// (éditeur/panneaux, dessinés APRÈS la toolbar) ne doit recevoir ni survol ni
// clic. On lève `appModal` : le shell masque alors l'input du corps de façon
// NON DESTRUCTIVE (save/restore de ec.Ui().input autour des panneaux). L'ancienne
// approche écrasait mousePos/mouseDown en PERSISTANT — or mousePos n'est rafraîchi
// que sur mouvement souris — ce qui gelait l'input et rendait les combos non
// modifiables. `ddWasOpen` (état AVANT fermeture) protège aussi le clic de fermeture.
if (ddWasOpen)
    ec.Ui().appModal = true;
```

> Le shell, lui, a le droit de téléporter la souris (§11.1) **parce qu'il restaure
> `mUI.input` intégralement quelques lignes plus loin**. Un panneau ne le peut pas.
> Le launcher le fait aussi, mais sur `u.mp` — une **copie locale** dans `NkUi`
> (`NkHome.h:1339-1348` puis `:1384-1387`).

### 11.5 Les autres mécanismes de non-traversée (7 au total)

**(a) `ctx.popupDepth == 0`** — la garde la plus légère, sur *chaque* test de clic.
C'est l'idiome n°1 (27 occurrences dans `Panels.h`).

**(b) Consommation explicite du clic** :

```cpp
// Toolbar.h:514
if (u.click && hitDD) in.mouseClicked[0] = false; // consommé -> pas de fuite vers l'éditeur
// NkHome.h:501
u.ctx->input.mouseClicked[0] = false; // consomme le clic (menu prioritaire)
```

Et le cas subtil du **clic d'ouverture** (`Panels.h:1376-1378`) :

```cpp
// CONSOMME le clic d'ouverture : le menu s'ouvre SOUS le bouton (souris hors boîte)
// et l'interpréterait sinon comme un clic extérieur -> fermeture immédiate.
ctx.input.mouseClicked[0] = false;
```

Variante sans consommation : un drapeau `justOpened` que la surface teste pour ignorer
la première frame (`Toolbar.h:33`, `:728`, `NkHome.h:491-492`, `:629-630`).

**(c) `ec.Ui().appModal = true`** — cf. (b) ci-dessus.

**(d) Neutralisation sélective** — cf. §11.4.

**(e) Bouclier clavier RAII** — `Applications/NKCode/src/NKCode/Shell/Panels.h:598-625` :

```cpp
// FOCUS CLAVIER GLOBAL : quand l'EXPLORATEUR a le focus-clic, l'éditeur
// ignore le CLAVIER (sinon Ctrl+D/Suppr/Entrée tireraient des DEUX côtés
// à la fois). La souris reste active ; un clic DANS l'éditeur reprend le
// clavier. RAII : l'input est restauré à toute sortie de OnUI.
struct KbShield {
    NkGuiContext *c = nullptr;
    NkGuiInput saved;
    ~KbShield() { if (c) c->input = saved; }
} kb;
if (mS->explorerFocus) {
    if (ctx.input.mouseClicked[0] && NkGuiRectContains(ctx.DL().CurrentClip(), ctx.input.mousePos))
        mS->explorerFocus = false; // clic dans l'éditeur : reprend le clavier
    else {
        kb.c = &ctx; kb.saved = ctx.input;
        ctx.input.charCount = 0;
        for (int32 k = 0; k < NkGuiInput::KeyCount; ++k) { ctx.input.keyDown[k] = false; ctx.input.keyInit[k] = false; }
        ctx.input.wantCopy = ctx.input.wantCut = ctx.input.wantPaste = false;
        ctx.input.wantSelectAll = false;
    }
}
```

**(f) Traiter l'input d'une surface flottante AVANT le contenu du dessous**
(`Panels.h:794-796` puis `:825-826`) :

```cpp
// ── Picker « aller à la définition » : INPUT traité AVANT l'éditeur, et clic CONSOMMÉ
//    -> l'éditeur dessous ne déplace pas le caret / ne démarre pas de sélection (drag). ──
…
ctx.input.mouseClicked[0] = false; // CONSOMME : pas de déplacement caret / drag dans l'éditeur
```

Idem pour le zoom (`Panels.h:772-773`) :

```cpp
// ── Zoom éditeur : Ctrl+molette / Ctrl+= (zoom) / Ctrl+- (dézoom) sur la police du
//    code. Consommé AVANT l'éditeur pour ne pas défiler à la place de zoomer. ──
```

**(g) Le rect de la frame précédente** (`Panels.h:1247-1250`) :

```cpp
// Souris sur un menu OUVERT (rect de la frame precedente) -> les onglets/boutons,
// rendus AVANT le menu, ne doivent ni survoler ni recevoir de clic a travers.
const bool overTabMenus = (mTabMenu.open && detail::InRect(mTabMenu.rect, m)) ||
                          (mTabList.open && detail::InRect(mTabList.rect, m));
```

### 11.6 Le piège des tests NÉGATIFS (à faire lire à tout débutant)

`Applications/NKCode/src/NKCode/Shell/Panels.h:3461-3465` :

```cpp
// « Clic en dehors du champ -> valider » : test NEGATIF, donc on
// exige d'abord que le clic ATTEIGNE notre couche
// (PointReachable). Sinon un clic destine a une surface
// flottante posee au-dessus (modale, menu) validait le
// renommage au passage.
else if (mRenameArmed && ctx.input.mouseClicked[0] && !ctx.input.mouseDoubleClicked[0] &&
         ctx.PointReachable(ctx.input.mousePos) &&
         !NkGuiRectContains(mRenameRect, ctx.input.mousePos)) {
```

Même raisonnement pour une cible de dépôt (`Panels.h:2297-2302`) :

```cpp
// Cible de depot : InputHits (routeur d'occlusion) et pas un
// NkGuiRectContains brut — sinon un depot fait SUR une surface
// flottante posee au-dessus du terminal (modale, menu, picker)
// serait quand meme colle dans le shell.
```

et pour l'arbre de l'explorateur (`NkExplorer.h:1004-1007`) :

```cpp
// Peek ouvert : l'arbre ne réagit plus (le peek modal a la main).
// + routeur d'occlusion : aveugle sous un modal/palette (couche > 0).
const bool inClip =
    NkGuiRectContains(clip, m) && m.y >= topY && !mPeekOpen && ctx.PointReachable(m);
```

---

## 12. Récapitulatif des IDIOMES (à reproduire dans tout code NkCanvas/NKGui)

1. **Récupérer la draw-list par `ctx.DL()`**, jamais `ctx.dl` en dur — la couche
   correcte (fenêtre / overlay) est choisie pour vous (`NkGuiContext.h:299-303`).
2. **Texte** : convertir un coin haut-gauche en baseline par
   `y + font->Ascent()`, ou centrer par
   `r.y + (r.h - font->LineHeight())*0.5f + font->Ascent()`.
   Mesurer avec `font->MeasureWidth(s)`. Toujours vérifier
   `if (!ctx.font || !ctx.font->Valid()) return;`.
3. **Un `AddText` par couleur** : la coloration syntaxique se fait par
   `AddTextRange(face, texId, baseline, begin, end, couleur)` jeton par jeton.
4. **Clip** : `dl.PushClipRect(zone, true)` … `dl.PopClipRect()` — TOUJOURS
   apparié, et TOUJOURS avec `intersect = true`. Profondeur max 32.
5. **Culling manuel en plus du clip** : ne pas émettre les lignes hors vue.
6. **Molette** : `if (Hit(zone) && ctx.input.wheel != 0.f) { scroll -= wheel*step;
   ctx.input.wheel = 0.f; }` puis bornage `[0, contentH - viewH]`.
7. **Drag** : `ctx.activeId = monId` à l'appui, agir tant que
   `ctx.activeId == monId && mouseDown[0]`, remettre `ctx.activeId = 0` au relâchement.
8. **Hit-test** : préférer `ctx.InputHits(r)` / `ctx.ClickIn(r)` (occlusion
   respectée) à `NkGuiRectContains(r, ctx.input.mousePos)`.
9. **Surface flottante** : `ctx.PushOcclusion(rect, couche)` +
   `NkGuiContext::NkInputLayerScope _l(ctx, couche)` (couches 0 / 50 / 100 / 200).
10. **Modale applicative** : les 8 gestes du §11.3, dont `ctx.appModal = true` et
    la remise à zéro de `ctx.popupDepth` à la fermeture.
11. **DPI** : toute taille en dur passe par `ctx.S(px)` (ou `u.s(px)` côté NKCode).
12. **Identifiants** : id STABLE distinct du libellé affiché, `PushId`/`PopId`
    autour des éléments d'une liste.
13. **Couleurs** : lire `ctx.theme.*` / `ctx.syntax.*` (ou `NkCol::*` côté NKCode),
    jamais de littéral RGBA dans le contenu ; détecter clair/sombre par
    `(r+g+b) > 384`.
14. **Textures** : `texId` alloué une fois (`shell->UploadRGBA`), réutilisé ensuite ;
    `UpdateRGBA` pour un contenu qui change chaque frame (vidéo).
15. **Rechargement d'atlas** : jamais pendant une frame — le différer avant
    `BeginFrame` (le shell le fait pour vous via `EnsureCodeSize` + debounce).
16. **Les trois premières lignes d'un panneau** :
    `auto &ctx = ec.Ui(); auto &dl = ctx.DL(); const NkRect clip = dl.CurrentClip();`
17. **`CurrentClip()` = le visible ; `ContentWidth()/AvailHeight()` = le contenu**
    (potentiellement ~1e6). Toujours intersecter les deux.
18. **Garde police obligatoire** avant tout `AddText`/`MeasureWidth` :
    `if (ctx.font && ctx.font->Valid())`, avec repli chiffré (`16.f` / `12.f`).
19. **Clic = `hov && ctx.input.mouseClicked[0] && ctx.popupDepth == 0`.**
    Le style « dessin direct » de NKCode n'utilise PAS d'id de widget.
20. **Ne jamais écraser `ctx.input.mousePos`** hors du shell : il n'est rafraîchi que
    sur un événement de déplacement. Neutraliser seulement clics / molette / `charCount`.
21. **Surface flottante** : dessiner dans `dlOverlay` via `NkUi::From(ec, true)`,
    consommer le clic d'ouverture (ou utiliser un drapeau `justOpened`), lever
    `ctx.appModal`, et repositionner le rect dans l'écran.
22. **Tests NÉGATIFS** (« clic en dehors ⇒ valider/fermer ») : exiger d'abord
    `ctx.PointReachable(mousePos)`, sinon un clic destiné à une surface au-dessus
    déclenche l'action.
23. **Toute mutation d'état est différée** hors du rendu (drapeau + `Poll*` à la
    frame suivante) : jamais d'`OpenPath`, de reconstruction d'arbre ou d'E/S disque
    au milieu d'un `OnUI`.

---

## 13. Récapitulatif des PIÈGES (chacun documenté par un commentaire du dépôt)

| # | Piège | Où c'est écrit |
|---|---|---|
| 1 | Soumettre toute la draw-list d'un coup dépasse la capacité du buffer GPU → crash. Il faut **rebaser les indices par commande**. | `NkGuiCanvasBackend.h:176-178` |
| 2 | Une UI dense dépasse 65536 sommets **entre deux clips** ; `kMaxVertices` a dû passer à 262144. | `NkBatchRenderer2D.h:33-40` |
| 3 | Ne jamais soumettre plus que la capacité : troncature (multiples de 3) plutôt que corruption mémoire. | `NkBatchRenderer2D.cpp:85-91` |
| 4 | Ré-uploader un atlas de police à une **nouvelle taille** sans recréer la texture ⇒ `Update()` déborde. | `NkGuiCanvasBackend.h:65-66` |
| 5 | Texte flou : sans `NkGuiPixelSnap`, les glyphes dérivent d'un demi-texel. Arrondir la **position**, jamais l'**avance**. | `NkGuiDrawList.cpp:226-235` puis `:255-266` |
| 6 | `AddRect` ne doit **pas** être fait avec `AddLine` : l'épaisseur centrée sur l'arête sort du rect et est rognée par le scissor (bord droit invisible). | `NkGuiDrawList.cpp:204-208` |
| 7 | Rechargement de police **pendant** une frame : la draw-list référencerait l'ancien atlas. À faire avant `BeginFrame`. | `NkEditorShell.cpp:596-601` |
| 8 | Rebuild d'atlas à chaque cran de molette = saccades. Debounce 120 ms + cache par taille. | `NkEditorShell.cpp:2288-2306`, `NkEditorShell.h:330-336` |
| 9 | `activeId` jamais relâché (widget disparu) ⇒ **toute** l'interaction se fige. Garde anti-gel en `EndFrame`. | `NkGuiContext.cpp:125-133` |
| 10 | Hit-tests bruts ⇒ clics qui traversent les overlays. D'où le routeur d'occlusion. | `NkGuiContext.h:163-174` |
| 11 | Un panneau lisant l'input « en direct » reçoit les clics à travers une fenêtre du dessus ⇒ bouclier dans `DrawPanels`. | `NkEditorShell.cpp:1996-1999` |
| 12 | Modale sans `popupDepth`/`popupRects[0]` ⇒ les clics traversent vers l'éditeur. | `NkAppCommands.h:157-164` |
| 13 | Masquer l'input **aussi** pour les actions de barre d'onglets rend leur combo injouable ⇒ exception explicite. | `NkEditorShell.cpp:203-206` |
| 14 | `BeginResize`/`BeginDragMove` **bloquent** (boucle modale OS) ⇒ à différer hors frame, sinon re-entrance de `RenderFrame`. | `NkEditorShell.cpp:548-550`, `NkEditorShell.h:380-383` |
| 15 | Overscroll : sans bornage par les max de la frame précédente, clignotement en butée. | `NkGuiWidgets.cpp:3452-3454` |
| 16 | Deadlock launcher→éditeur : en plein écran la barre de menus n'est pas dessinée, donc le hook qui repose `appFullScreen` ne tournait plus. D'où l'appel **inconditionnel** de `mAppMenuFn` avant la décision. | `NkEditorShell.cpp:633-639` |
| 17 | Double `Initialize()` du renderer (déjà fait par la factory) ⇒ « Already initialized » + rectangles creux. | `NkRenderWindow.cpp:95-98` |
| 18 | Détruire le contexte avec `delete` global au lieu de `NkContextFactory::Destroy` ⇒ corruption de tas `c0000374`. | `NkRenderWindow.cpp:126-132` |
| 19 | `SetView` / `SetClip` / `PopClip` provoquent un `Flush()` ⇒ un draw-call de plus à chaque changement. | `NkBatchRenderer2D.cpp:120-160` |
| 20 | Scissor OpenGL : origine bas-gauche ⇒ flip Y obligatoire. | `NkOpenGLRenderer2D.cpp:537-543` |
| 21 | Sur Linux, `<X11/Xlib.h>` définit `None` ⇒ casse les énumérations NKGui. D'où `NKPlatform/NkX11Clean.h` inclus **en tête** de `NKGui.h`. | `NKGui.h:18-27` |
| 22 | `NkRenderTexture` et `NkShader` (hors OpenGL) sont encore des **stubs** (`Create()`/`Compile()` renvoient `false`). | `USAGE.md:364-366`, `:445-447`, `:484-488` |
| 23 | Écraser `ctx.input.mousePos` **gèle** la souris pour toute l'app (il n'est mis à jour que sur événement de déplacement). | `NkExplorer.h:89-98`, `Toolbar.h:798-806` |
| 24 | `AvailHeight()/ContentWidth()` valent ~1e9 dans une zone défilable ⇒ `viewH` gigantesque, pas de scrollbar, barre H hors écran. | `Panels.h:677-679` |
| 25 | Un menu qui s'ouvre **sous** le curseur interprète le clic d'ouverture comme un « clic dehors » et se referme aussitôt ⇒ consommer ce clic. | `Panels.h:1376-1378` |
| 26 | Un champ texte focalisé une fois **vole `wantPaste/wantCopy/wantCut` globalement**, même invisible ⇒ effacer AUSSI `ctx.inputId`, pas seulement le focus custom. | `Panels.h:2436-2441` |
| 27 | Un test NÉGATIF (« clic dehors ⇒ valider ») déclenche sur un clic destiné à une surface au-dessus ⇒ exiger `PointReachable`. | `Panels.h:3461-3465` |
| 28 | Un dépôt fait **sur** une modale posée au-dessus du terminal était quand même collé dans le shell ⇒ `InputHits` et non `NkGuiRectContains`. | `Panels.h:2297-2302` |
| 29 | Icônes floues : sans mipmaps, un bilinéaire direct 128 → 35 ne prend que 2×2 texels ⇒ downscale progressif par demi-pas. | `NkAppIcons.h:44-46`, `:200-203` |
| 30 | Icônes de tailles visuellement inégales : marges transparentes internes ⇒ rognage alpha (`trimAlpha`). | `NkAppIcons.h:100-102` |
| 31 | Chemins relatifs au CWD : en distribution, l'utilisateur lance l'exe depuis n'importe où ⇒ chercher AUSSI à côté de l'exécutable. | `NkAppFonts.h:18-21`, `NkAppIcons.h:146-148` |
| 32 | Le wordmark complet réduit à la taille de la barre de titre est illisible ⇒ pré-redimensionner CPU, et utiliser l'icône + texte vectoriel dans la barre. | `NkAppIcons.h:187-192` |
| 33 | Le texte du panneau clignote en butée de scroll si on borne APRÈS le dessin ⇒ borner avant, avec le max de la frame précédente. | `NkHome.h:726-733`, `:41-42` |
| 34 | Deux versions divergentes (footer vs « À propos ») + désynchro avec le tag GitHub ⇒ source unique `NkCodeVersion()`. | `NkUi.h:15-27` |
| 35 | Deux lignes virtuelles identiques en édition (dédoublement à la création d'un fichier à la racine, issue #4). | `NkExplorer.h:405-409` |
| 36 | Jenga crée/supprime des fichiers hors de l'IDE ⇒ re-scan de l'arbre en fin de build (issue beta #2). | `NkExplorer.h:56-57` |
| 37 | Fermer un onglet modifié perdait les modifications silencieusement ⇒ confirmation d'abord. | `Panels.h:1530-1532` |
| 38 | Une sélection RESTAURÉE (session) peut pointer hors du contenu actuel ⇒ valider ligne ET colonnes avant de lire. | `NkCodeEditor.h:3908-3910` |
| 39 | Le caret pouvait rester sur une ligne repliée (invisible) ⇒ remappage avant souris/scroll/rendu. | `NkCodeEditor.h:2317-2324` |
| 40 | Le `TabBar` NKGui gardait son propre index et empêchait la révélation au clic ⇒ bandeau d'onglets custom. | `Panels.h:667-669` |

### 13.1 Gardes défensives récurrentes (à imiter)

```cpp
if (depth > 24) return;                             // NkExplorer.h:524-526  récursion d'arborescence
if (subs.Size() <= 64)                              // NkExplorer.h:1672     2e niveau si l'arbre reste raisonnable
if (mS->active < 0 || mS->active >= (int32)mS->files.Size()) mS->active = 0;  // Panels.h:671-674
if (cols < 1) cols = 1;  if (cols > 500) cols = 500;                          // Panels.h:2781-2789
while (mLogs.Size() > 5000) mLogs.Erase(mLogs.Begin());                       // Panels.h:1746  borne mémoire
H->caretBlink += ec.dt; if (H->caretBlink > 1.0e6f) H->caretBlink = 0.f;      // NkHome.h:1313-1315 anti-overflow
// GARDE : jamais coller un dossier dans LUI-MÊME/sa descendance.             // NkExplorer.h:1390
dst += " - copie";                                  // NkExplorer.h:1266 / 1415  ne pas écraser l'existant
```

### 13.2 Deux invariants « produit » énoncés dans le code

```cpp
// Applications/NKCode/src/NKCode/Shell/NkMenuBar.h:6-9
//   Regle d'HONNETETE : un item n'est CLIQUABLE que si son backend existe
//   reellement ; sinon AFFICHE GRISE — jamais un clic muet. Les commandes
//   externes (git, checksum, mises a jour) passent par le TERMINAL integre :
//   visibles, reelles, transparentes.
```

```cpp
// Engine/NKEditorKit/src/NKEditorKit/NkEditorScrollbar.h:5-8
// C'est le scrollbar de l'editeur de code EXTRAIT TEL QUEL (memes couleurs, memes
// fleches, memes tailles) -> une seule barre pour TOUTE l'UI Nkentseu.
// NE PAS "ameliorer" le rendu : il doit rester identique a l'editeur.
```

---

## 14. Liens entre NkCanvas, NKGui et NKEditorKit

### 14.1 Graphe de dépendances (extrait des `.jenga`)

```
NKGui        -> NKPlatform NKCore NKMemory NKMath NKThreading NKLogger
                NKContainers NKEvent NKFont NKImage
                (Kernel/Runtime/NKGui/NKGui.jenga:22-27)
                ► NKGui NE DÉPEND PAS de NKCanvas.

NKCanvas     -> _canvasDeps (Kernel/Runtime/NKCanvas/NKCanvas.jenga:88-93)
                ► NKCanvas NE DÉPEND PAS de NKGui non plus…
                  … sauf le header-only NKCanvas/UI/NkGuiCanvasBackend.h, qui
                  inclut "NKGui/NKGui.h" mais n'est compilé par personne dans
                  NKCanvas (« NKCanvas ne le compile pas (pas de .cpp) »).

NKEditorKit  -> NKPlatform NKCore NKMemory NKContainers NKMath NKLogger NKTime
                NKStream NKFileSystem NKEvent NKWindow NKCanvas NKFont NKImage NKGui
                (Engine/NKEditorKit/NKEditorKit.jenga:25-31)
                ► C'est LE point de jonction : NKEditorKit connaît les deux.

NKCode       -> NKEditorKit + tout le reste (Applications/NKCode/NKCode.jenga:82-95)
```

### 14.2 Qui dépend de qui, et pour quoi

| Couche | Rôle | Ne connaît PAS |
|---|---|---|
| **NKGui** | Contexte immédiat, widgets, docking, thème, saisie, **production de draw-lists**. Zéro appel GPU. | NKCanvas, NKRHI, NKWindow |
| **NKCanvas** | Contexte GPU multi-backend + rendu 2D SFML-like + **consommation de draw-lists** (`NkGuiCanvasBackend`, header-only). | NKGui (au niveau compilation), NKEditorKit |
| **NKEditorKit** | Coquille d'application d'édition : fenêtre, boucle, barre de titre, dock, panneaux, palette, préférences, thèmes, polices. **Choisit le backend de rendu.** | NKRHI, NKRenderer (l'impl NKRHI vit dans `Integrations/`) |
| **NKCode** | L'IDE : panneaux, éditeur de code, explorateur, terminal, IA, PDF… | NKCanvas (jamais inclus) |

### 14.3 Le point de branchement, en une image

```
                     ┌──────────────────────────────┐
   NKCode  ────────► │ NkEditorShell (NKEditorKit)  │
   (panneaux)        │  mRenderer : NkIEditorRenderer* │
                     └───────────────┬──────────────┘
                                     │  interface pure (types NKGui/NKWindow)
                    ┌────────────────┴────────────────┐
                    ▼                                 ▼
   NkEditorCanvasRenderer                 NkEditorRHIRenderer
   (NKEditorKit, DÉFAUT — IDE)            (Integrations/NKGui — apps moteur)
        │                                       │
        ├── renderer::NkRenderWindow            ├── NkIDevice + swapchain (NKRHI)
        └── renderer::NkGuiCanvasBackend        └── nkgui::NkGuiRHIBackend
                │                                       │
                └────────► NkIRenderer2D                └────────► command buffers
                            (DX11/DX12/GL/VK/SW)
```

### 14.4 Les deux backends de draw-list, côte à côte

```
/* Integrations/NKGui/NkGuiRHIBackend.h:7-18 */
* Porte de Integrations/NKUI/NkUIRHIBackend : meme pipeline, memes buffers
* dynamiques (VBO/IBO/UBO), meme upload texture/atlas (RGBA8 + Gray8 -> RGBA8),
* meme gestion scissor (top-left DX/VK/SW, bottom-left OpenGL), meme conversion
* de vertices (couleur RGBA8 packee -> vec4). Difference : NKGui expose UNE
* draw-list (NkGuiDrawList) avec un clipRect PAR commande (et non des couches
* avec commande NK_CLIP_RECT comme NKUI) ; le scissor est applique par commande.
*
* Sert au rendu de l'UI des applications 2D/3D (app d'animation, moteur de jeu)
* qui utilisent NKRenderer/NKRHI ; NKCanvas reste pour l'IDE.
```

À noter aussi : `Kernel/Runtime/NKCanvas/src/NKCanvas/UI/NkUICanvasBackend.h/.cpp`
est le **jumeau plus ancien** de `NkGuiCanvasBackend`, pour le module NKUI (l'UI
précédente). NKCode n'utilise que NKGui.

---

### 14.5 Le flux complet, de bout en bout (à mettre en tête du cours)

```
DÉMARRAGE
  nkmain()                                        Applications/NKCode/src/NKCode/main.cpp
    NkPath::GetExecutableDirectory()              (AVANT tout le reste)
    InstallLogSink() ; NkSynInitDefaultLangs()
    NkLoadFallbackFonts()   [NkAppFonts.h]     -> nkgui::NkSetFallbackFontPaths(broad, cjk, emoji)
    shell->Init(cfg)        [NkEditorShell]
       NkWindow::Create (frame=false, min 1024x640)
       new NkEditorCanvasRenderer -> NkRenderWindow(window, {api = DX11|GL}) + NkGuiCanvasBackend
       mUI.Init(w, h) ; thème GitHub Dark ; presse-papiers ; hook barre d'onglets
       NkLoadFontPrefs()    [NkFontPrefs.h]    -> ~/.nkcode_fonts.cfg
       LoadFontsFromPrefs() -> NkResolveFont -> NkGuiFont::LoadEmbedded / LoadFromFile
                            -> NkFontAtlas::Build -> GetTexDataAsAlpha8
                            -> mRenderer->UploadFontGray8(texId, pixels, w, h)   [NKCanvas]
       HookEvents()  (souris, molette, texte, touches -> mUI.input)
    AddPanel(...) x21 ; SetToolbar / SetMenuBar / SetOverlay / SetStartScreen / SetZoomHandler
    NkLoadAppIcons()        [NkAppIcons.h]     -> PNG/SVG -> trimAlpha -> downscale ½
                                               -> shell->UploadRGBA -> texId
    RegisterCommand(...) x9
    shell->Run()

CHAQUE IMAGE                                     NkEditorShell::RenderFrame()
  dt ; OnResize ; rechargement d'atlas DIFFÉRÉ (debounce 120 ms) ; mUI.BeginFrame(dt)
  fond -> AppFlagsThunk -> barre de titre (+ menus) -> barre d'outils
  masquage modal éventuel de l'input
  activity bars -> DockSpace -> BootstrapDocking -> DrawPanels
      Begin(mUI, titre)  -> ctx.curWindow = winDL[k], clip + scroll sur m->dockRect
        panel->OnUI(ec)
            auto &ctx = ec.Ui(); auto &dl = ctx.DL(); const NkRect clip = dl.CurrentClip();
            [éditeur] EnsureCodeSize / CodeFontForSize -> NkCodeFontScope
                      PushClipRect(textArea)
                        ∀ ligne visible : TokenizeLine(..., emit(a, b, couleur))
                                            -> NkDrawTextU -> AddTextRange | NkDrawGlyphPrim
                      PopClipRect()
                      PushClipRect(gutter) -> AddText(numéros) -> PopClipRect()
      EndWindow(mUI)
  barre d'état -> menu contextuel -> palette -> Préférences -> OverlayThunk (modales)
  mUI.EndFrame()      (fusion des winDL par z-order dans dl ; nettoyage input)
  mRenderer->BeginFrame()                       -> NkRenderWindow::Clear -> NkIRenderer2D::Begin
  mRenderer->SubmitDrawList(mUI.dl,        w, h) -> NkGuiCanvasBackend::Submit
  mRenderer->SubmitDrawList(mUI.dlOverlay, w, h)      ∀ commande : SetClip / DrawVertices / PopClip
  mRenderer->EndFrame()                         -> NkRenderWindow::Display -> End + Present

HORS IMAGE (debounce expiré)
  BuildCodeSlot(px) -> nouvel atlas monospace + UploadFontGray8 (texId = base+8..+15)
```

---

## 15. Où lire quoi — index rapide pour le rédacteur du cours

| Question | Fichier |
|---|---|
| Comment démarre l'app ? | `Applications/NKCode/src/NKCode/main.cpp` |
| Où est la boucle principale ? | `Engine/NKEditorKit/src/NKEditorKit/NkEditorShell.cpp:539` (`Run`) |
| Où est la frame ? | idem `:563` (`RenderFrame`) |
| Comment NKGui produit-il des triangles ? | `Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiDrawList.cpp` |
| Quelle est l'API de dessin disponible ? | `Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.h` (405 l., tout commenté) |
| Comment les triangles arrivent au GPU ? | `Kernel/Runtime/NKCanvas/src/NKCanvas/UI/NkGuiCanvasBackend.h` |
| Quelle est l'API NkCanvas « pour de vrai » ? | `Kernel/Runtime/NKCanvas/USAGE.md` + `Guides/05-NKCanvas.md` |
| Comment écrire un panneau ? | `Engine/NKEditorKit/src/NKEditorKit/NkEditorPanel.h` + `Applications/NKCode/src/NKCode/Shell/Panels.h` |
| Quels helpers de dessin NKCode s'est-il faits ? | `Applications/NKCode/src/NKCode/Shell/NkUi.h` |
| Une scrollbar prête à l'emploi ? | `Engine/NKEditorKit/src/NKEditorKit/NkEditorScrollbar.h` |
| Un exemple complet et court de dessin brut ? | `NkEditorShell.cpp:934-1086` (`DrawTitleBar`) et `NkAppCommands.h:234-361` (`DrawHelpModal`) |
| Bugs GPU historiques et leur résolution | `BugReports/NKCanvas/*.md` (6 fiches) |
