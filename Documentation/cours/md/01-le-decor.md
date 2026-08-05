# Le décor : la pile complète

Avant d'écrire une seule ligne de code, il faut savoir *qui fait quoi*. Ce
chapitre pose la carte du terrain. Il se lit de bas en haut : de la fenêtre que
le système d'exploitation nous donne, jusqu'au bouton sur lequel l'utilisateur
clique. À la fin, vous saurez nommer chaque étage, dire de quoi il dépend, et —
c'est le point le plus important du chapitre — vous comprendrez pourquoi
l'étage du haut ne connaît pas l'étage du bas.

## Vue d'ensemble

**`Vue d'ensemble — synthèse (voir Documentation, section « Raccordement au reste du moteur »)`**

```
   Application  (NKGuiDemo, NKCode, NK3DModeler…)
        |  cree une NkWindow, pompe les NKEvent -> remplit ctx.input
        |  declare ses widgets entre BeginFrame / EndFrame
        v
   NKGui  (NkGuiContext -> NkGuiDrawList : vtx + idx + cmds)
        |  ne connait AUCUN GPU
        v
   Backend (au choix)
     |- NkGuiCanvasBackend   (header-only, dans NKCanvas/UI/)  -> NkIRenderer2D
     `- NkGuiRHIBackend      (Integrations/NKGui/)             -> NKRHI
        v
   NKCanvas (NkBatchRenderer2D -> SubmitBatches)  |  NKRHI
        v
   GPU
```

Cinq étages, donc, mais qui ne s'empilent pas comme on l'imaginerait. Prenons-les
un par un.

## Étage 1 — la fenêtre : `NKWindow`

`NKWindow` est l'abstraction de fenêtre native. C'est le seul module de la
pile qui parle au système d'exploitation : `HWND` sous Windows,
`Window` X11 ou surface Wayland sous Linux, `NSWindow` sous macOS.

Son usage se réduit à trois gestes :

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:243-251`**

```cpp
    NkWindow window;
    NkWindowConfig cfg;
    cfg.title = "NKGui - Demo Phase 2 (Coeur : drawlist + interaction)";
    cfg.width = 1100;
    cfg.height = 800;
    cfg.centered = true;
    cfg.resizable = true;
    if (!window.Create(cfg))
        return -1;
```

puis, dans la boucle, `window.IsOpen()` pour savoir si elle vit
encore, et `window.SetCursor(…)` pour changer le pointeur. Le reste —
taille courante, position, état maximisé — s'interroge par des accesseurs
(`GetSize()` renvoie un `math::NkVec2u`).

Une option mérite d'être signalée dès maintenant parce qu'elle explique
l'apparence des applications du dépôt :

**`Engine/NKEditorKit/src/NKEditorKit/NkEditorShell.cpp:234`**

```cpp
    wc.frame = false;          // SANS bordure OS -> barre de titre custom (VSCode)
```

Avec `frame = false`, le système ne dessine plus ni barre de titre ni
bordure : l'application les dessine elle-même, comme le fait Visual Studio Code.
C'est ce que fait `NKCode`. Notre application du chapitre 4 gardera la
décoration native, qui est plus simple.

> **⚠️ X11 définit `None`**
>
> Sous Linux, `NKWindow` tire `<X11/Xlib.h>`, qui contient
> `#define None 0L`. Or plusieurs énumérations de NKGui ont un membre
> nommé `None`. Le préprocesseur le transforme alors en `0L = 0` et
> le compilateur signale « expected identifier » sur des lignes parfaitement
> correctes, très loin de la vraie cause. La parade est en tête de l'en-tête
> parapluie de NKGui :
>
> **`Kernel/Runtime/NKGui/src/NKGui/NKGui.h:18-27 (abrégé)`**
>
> ```cpp
> // Retire les macros X11 (None, Bool, Status...) AVANT toute declaration de
> // NKGui. Sur Linux, NKWindow tire <X11/Xlib.h>, dont le `#define None 0L`
> // transforme ensuite `None = 0` — membre de plusieurs enumerations de NkGuiTypes
> // — en `0L = 0`. Le compilateur signale alors « expected identifier » sur des
> // lignes parfaitement correctes, tres loin de la vraie cause.
> #include "NKPlatform/NkX11Clean.h"
> ```
>
> La leçon générale : **incluez toujours `NKGui/NKGui.h` et jamais un
> en-tête interne de NKGui**. L'ordre d'inclusion de l'en-tête parapluie n'est pas
> arbitraire, c'est l'ordre des dépendances.

## Étage 2 — les événements : `NKEvent`

`NKEvent` transforme les messages du système en **événements
typés** : `NkWindowCloseEvent`, `NkMouseMoveEvent`,
`NkMouseButtonPressEvent`, `NkKeyPressEvent`,
`NkTextInputEvent`…

Le modèle est celui des *callbacks* enregistrés une fois pour toutes, puis
d'une file qu'on vide à chaque image :

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:326-331 (extrait)`**

```cpp
    auto &events = NkEvents();
    bool running = true;
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) { running = false; });
    events.AddEventCallback<NkMouseMoveEvent>([&](NkMouseMoveEvent *e) {
        ctx.input.mousePos = {static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY())};
    });
```

et, dans la boucle :

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:438-440`**

```cpp
        while (NkEvent *ev = NkEvents().PollEvent()) {
            (void)ev;
        }
```

Ce `while` déroutant mérite un mot : **il ne fait rien du résultat**.
Le travail a déjà été accompli par les *callbacks* enregistrés plus haut ;
`PollEvent` sert uniquement à *pomper* la file jusqu'à ce qu'elle
soit vide, ce qui déclenche au passage l'appel de tous les *callbacks*. On
retrouve exactement le même idiome dans la boucle principale de
`NKEditorKit` (`Engine/NKEditorKit/src/NKEditorKit/NkEditorShell.cpp:314`).

Une distinction fondamentale, qu'on retrouvera au chapitre 4 :

- `NkTextInputEvent` porte le **texte** tapé, déjà décodé par
  le système en *codepoint* Unicode — claviers AZERTY et méthodes de
  saisie asiatiques comprises. C'est lui qui alimente les champs de
  saisie ;
- `NkKeyPressEvent` / `NkKeyReleaseEvent` portent les
  **touches** : flèches, Suppr, Entrée, F2, Ctrl+quelque chose. Ce
  sont elles qui alimentent la navigation.

Confondre les deux est un classique : on branche les flèches sur le texte, et
plus rien ne fonctionne. Le commentaire du pont de NKEditorKit le dit :

**`Engine/NKEditorKit/src/NKEditorKit/NkEditorShell.cpp:367-370`**

```cpp
// Mappe une touche OS d'edition vers l'etat ENFONCE NKGui (press/release ->
// repetition au maintien geree par NKGui). Sans ce pont, aucune navigation
// clavier ni edition de texte dans les panneaux.
```

## Étage 3 — le GPU. Attention, il y a deux abstractions

C'est ici que le débutant se perd, et c'est normal : le moteur contient
**deux** abstractions GPU distinctes, qui ne se superposent pas.

### `NKRHI` — l'abstraction GPU bas niveau

`Kernel/Runtime/NKRHI` est l'abstraction « moderne » du GPU :
périphérique (`NkIDevice`), tampons de commandes, passes de rendu,
*pipelines*, descripteurs. Son arborescence de backends parle d'elle-même :
`DirectX11`, `DirectX12`, `Metal`, `Opengl`,
`Software`, `Vulkan`. C'est sur elle que repose le rendu 3D
(`NKRenderer`) et, par ricochet, les applications qui font de la 3D.

### `NKCanvas` — sa propre abstraction, pour la 2D

`NKCanvas` **ne passe pas par NKRHI**. Il possède ses propres
contextes graphiques et ses propres renderers 2D. La preuve tient en une ligne
de son fichier de build :

**`Kernel/Runtime/NKCanvas/NKCanvas.jenga:65`**

```cpp
    _canvasDeps = ["NKWindow", "NKFont", "NKImage", "NKStream", "NKTime", "NKGlad", "NKThreading"]
```

Pas de `NKRHI` dans la liste. L'arborescence
`Kernel/Runtime/NKCanvas/src/NKCanvas/Backend/` contient
`DirectX/` (DX11 et DX12), `Metal/`, `OpenGL/`,
`Software/` et `Vulkan/` : ce sont les implémentations *de
NKCanvas*, pas celles de NKRHI.

Il y a donc, dans le dépôt, deux familles de backends portant les mêmes noms
d'API graphique et ne se connaissant pas. Cette duplication est assumée et même
documentée à l'endroit où elle pose problème :

**`Engine/NKEditorKit/src/NKEditorKit/NkIEditorRenderer.h:28-31`**

```cpp
// Choix d'API graphique NEUTRE (decouple de NKCANVAS ET NKRHI : leurs enums
// NkGraphicsApi se dupliquent dans le namespace nkentseu et ne peuvent
// cohabiter dans un meme TU). Chaque impl mappe vers son propre enum.
enum class NkEditorGfxApi : uint8 { Auto = 0, OpenGL, Vulkan, DX11, DX12, Software };
```

Lisez bien : les deux énumérations `NkGraphicsApi` — celle de
NKCanvas et celle de NKRHI — *ne peuvent pas cohabiter dans une même unité
de traduction*. D'où le besoin d'un troisième énuméré, neutre, quand une couche
doit pouvoir parler aux deux.

> **✅ Une fenêtre = une pile**
>
> C'est la doctrine du dépôt, énoncée mot pour mot :
>
> **`Integrations/NKGui/NkEditorRHIRenderer.h:8-13 (extrait)`**
>
> ```cpp
> * toute application moteur (NkAnimaEditor, Nogee, ...) qui veut la coquille
> * NkEditorShell rendue sur NKRHI/NKRenderer (et PAS NKCanvas — regle
> * « une fenetre = une pile ») injecte cette classe via
> * NkEditorShellConfig::renderer.
> ```
>
> Une fenêtre est rendue **soit** par NKCanvas, **soit** par NKRHI —
> jamais les deux. Le choix se fait une fois, à la création. Pour ce cours, ce sera
> toujours NKCanvas.

### Les cinq backends 2D de NKCanvas, et leur état réel

Les cinq dossiers de backends ne fournissent pas tous un renderer 2D, et ceux
qui le fournissent ne sont pas tous validés à l'exécution. Voici l'état, tel
qu'il ressort des sources et des feuilles de route :

| **Backend** | **Renderer 2D** | **État à l'exécution** |
|---|---|---|
| OpenGL | `NkOpenGLRenderer2D` (926 l.) | le seul **validé** |
| Vulkan | `NkVulkanRenderer2D` (1524 l.) | le code le plus complet ; plantages signalés |
| DX12 | `NkDX12Renderer2D` (1320 l.) | plantages signalés |
| DX11 | `NkDX11Renderer2D` (764 l.) | écran vide/noir signalé |
| Software | `NkSoftwareRenderer2D` (674 l.) | écran vide/noir signalé |
| Metal | **aucun** | non implémenté |

La ligne « Metal » n'est pas une supposition, elle est écrite dans la fabrique :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Core/NkRenderer2DFactory.cpp:86-89`**

```cpp
case NkGraphicsApi::NK_GFX_API_METAL:
    NK_R2D_FACTORY_ERR("Metal 2D renderer not yet implemented");
    return nullptr;
```

Quant aux autres verdicts, ils viennent de
`Kernel/Runtime/NKCanvas/ROADMAP.md` (lignes 185-200 et 601-607), à la
date du 30 mai 2026 : seul OpenGL était alors validé à l'exécution sur la démo
Pong. **Ces états sont datés et méritent d'être revérifiés** avant de tirer
une conclusion définitive : le dépôt bouge. Mais ils expliquent pourquoi, quand
une application « ne montre rien », la première chose à essayer est de changer de
backend.

Le choix, lui, est explicite. Soit on nomme l'API :

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:253-262`**

```cpp
    NkContextDesc desc;
    desc.api = NkGraphicsApi::NK_GFX_API_AUTO;
    if (desc.api == NkGraphicsApi::NK_GFX_API_AUTO) {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
        desc.api = NkGraphicsApi::NK_GFX_API_DX11;
#else
        desc.api = NkGraphicsApi::NK_GFX_API_OPENGL;
#endif
    }
```

soit on laisse la fabrique essayer une liste, par
`NkContextFactory::CreateWithFallback(window, preferenceOrder, count)`.
L'ordre conseillé documenté en `.../Factory/NkContextFactory.h:46` est
`{DX12, DX11, Metal, Vulkan, OpenGL, Software}`.

## Étage 4 — `NkCanvas`, le rendu 2D

`NkCanvas` est décrit dans `Guides/05-NKCanvas.md` comme
« la couche de rendu 2D conviviale de Nkentseu, l'équivalent de la partie
*Graphics* de SFML ». Il fournit :

- un **contexte graphique** (`NkIGraphicsContext`) créé par une
  fabrique selon l'API choisie ;
- une **cible de rendu** (`NkRenderTarget`), dont la
  spécialisation `NkRenderWindow` est celle qu'on utilise
  99 % du temps : elle possède le cycle d'image ;
- un **renderer 2D** (`NkIRenderer2D`, façade
  `NkRenderer2D`) qui offre les primitives : lignes, rectangles,
  cercles, triangles, et le point d'entrée bas niveau
  `DrawVertices` ;
- des **ressources** : `NkTexture`, `NkFont`,
  `NkSprite`, `NkText`, `NkShader` ;
- des **formes persistantes** façon SFML : `NkRectangleShape`,
  `NkCircleShape`, `NkLineShape`, `NkConvexShape`.

Le chapitre 2 lui est entièrement consacré. Retenez pour l'instant une seule
chose : NkCanvas dessine des **triangles**. Tout le reste — cercles,
lignes épaisses, texte — est fabriqué à partir de triangles par le module
lui-même.

## Étage 5 — `NKGui`, les widgets

`NKGui` est le framework d'interface : boutons, cases à cocher, sliders,
champs de saisie, arbres, tables, menus, fenêtres flottantes, docking. Environ
7 780 lignes réparties sur 13 fichiers source, dont
`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.cpp` qui pèse à lui
seul 4 973 lignes.

Son arborescence est petite et lisible — c'est par là qu'il faut commencer :

**`Kernel/Runtime/NKGui/ — arborescence`**

```
Kernel/Runtime/NKGui/
  NKGui.jenga · ARCHITECTURE.md · README.md · ROADMAP.md
  src/NKGui/
    NKGui.h                     <- en-tete PARAPLUIE (la seule chose a inclure)
    NkGuiExport.h -> NkGuiApi.h <- macros d'export (defaut : statique)
    Core/
      NkGuiTypes.h      (333 l.) types de base : NkGuiId, enums, themes de flags
      NkGuiInput.h      (145 l.) etat d'entree par frame (header-only, tout inline)
      NkGuiDrawList.h/.cpp (101/342 l.) la LISTE DE COMMANDES de dessin
      NkGuiFont.h/.cpp  ( 81/158 l.) police + atlas (wrapper NKFont)
      NkGuiContext.h/.cpp (551/573 l.) LE contexte : etat complet de l'UI
    Widgets/
      NkGuiWidgets.h/.cpp (405/4973 l.) TOUS les widgets
```

### Le point capital : NKGui ne connaît aucun GPU

Voici la ligne la plus importante de ce chapitre :

**`Kernel/Runtime/NKGui/NKGui.jenga:22-27`**

```cpp
nkentseudependson(
    ["NKPlatform", "NKCore", "NKMemory", "NKMath", "NKThreading",
     "NKLogger", "NKContainers", "NKEvent", "NKFont", "NKImage"],
    selfexport="NKGui",
    extra_includes=["src"],
)
files(["src/**.cpp"])
```

Relisez la liste. Il n'y a **ni `NKCanvas`, ni `NKRHI`, ni
même `NKWindow`**. NKGui ne connaît que les mathématiques, les conteneurs,
la mémoire, les polices, les images et les événements. Il ne sait pas ce qu'est
une texture GPU, ni un *shader*, ni une fenêtre.

> **✅ Ce qu'il faut retenir**
>
> **NKGui ne dépend ni de NkCanvas ni de NKRHI.** Il ne dessine rien : il
> *produit une liste de commandes* — des sommets, des indices, des commandes
> de dessin — et laisse quelqu'un d'autre les envoyer au GPU. Ce quelqu'un
> s'appelle un **backend**, et le mariage se fait par un **pont**.

### Ce que NKGui produit : `NkGuiDrawList`

La sortie de NKGui est purement géométrique :

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiDrawList.h:23-46 (abrégé)`**

```cpp
struct NkGuiVertex {
        NkVec2 pos;
        NkVec2 uv;   ///< (0,0) = couleur unie (pas de texture)
        uint32 col;
};

enum class NkGuiDrawCmdType : uint8 {
    Triangles,         ///< triangles unis
    TexturedTriangles  ///< triangles textures (texte/atlas/image)
};

struct NkGuiDrawCmd {
        NkGuiDrawCmdType type = NkGuiDrawCmdType::Triangles;
        uint32 idxOffset = 0;
        uint32 idxCount  = 0;
        uint32 texId     = 0;
        NkRect clipRect  = {0.f, 0.f, 1.0e9f, 1.0e9f};   // 1e9 = « pas de clip »
};
```

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiDrawList.h:48-56 (abrégé)`**

```cpp
struct NkGuiDrawList {
        NkVector<NkGuiVertex> vtx;
        NkVector<uint32>      idx;
        NkVector<NkGuiDrawCmd> cmds;
        NkRect  clipStack[32] = {};
        int32   clipDepth = 0;
        float32 thickScale = 1.f; ///< echelle DPI des epaisseurs (preservee par Reset)
};
```

Trois tableaux, donc : les sommets, les indices, et les commandes. Une
*commande* décrit une tranche d'indices à dessiner avec une texture donnée
et un rectangle de découpe donné. Notez le `texId` : c'est un simple
entier, pas un pointeur vers une texture GPU. NKGui manipule des
*identifiants*, à charge du backend de savoir à quoi ils correspondent.

Notez aussi la sentinelle `1e9` pour « pas de découpe ». Le backend la
reconnaît par un test tolérant (`w < 1e8`), ce qui évite toute
comparaison exacte de flottants.

## Le pont : marier NKGui et NkCanvas

Puisque NKGui ne connaît pas NkCanvas, et que NkCanvas ne connaît pas NKGui, il
faut un tiers qui connaisse les deux. Le dépôt en fournit deux, selon le public.

### `NkGuiCanvasBackend` — le pont vers NkCanvas

Il vit dans `Kernel/Runtime/NKCanvas/src/NKCanvas/UI/NkGuiCanvasBackend.h`
et il est **header-only**. Ce détail est délibéré et sa justification est en
tête de fichier :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/UI/NkGuiCanvasBackend.h:2-9`**

```cpp
// NkGuiCanvasBackend.h — rend un nkgui::NkGuiDrawList via NKCanvas (NkIRenderer2D).
// Backend RÉUTILISABLE (lib) : le cœur NKGui reste render-agnostique ; ce pont
// traduit ses draw-lists en appels NkIRenderer2D. Gère l'atlas de police
// (gray8 -> RGBA blanc+alpha) et les images RGBA pour les commandes texturées.
//
// HEADER-ONLY : NKCanvas ne le compile pas (pas de .cpp) ; seuls les consommateurs
// (qui dépendent déjà de NKGui ET NKCanvas) l'incluent. Modelé sur NkUICanvasBackend.
```

Comprenez bien l'astuce. Ce fichier inclut `"NKGui/NKGui.h"`, donc il
dépend de NKGui. Mais il vit dans NKCanvas, dont le `files([...])` ne
prend que les `.cpp` : **personne ne le compile dans NKCanvas**. Il
n'est compilé que dans l'application qui l'inclut — application qui, elle,
dépend bien des deux modules. Résultat : NKCanvas n'acquiert aucune dépendance
vers NKGui, et le pont existe quand même.

Son API tient en quatre méthodes :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/UI/NkGuiCanvasBackend.h:34, 41, 94, 120`**

```cpp
bool Init(nkentseu::renderer::NkIRenderer2D *renderer);
bool UploadFontGray8(uint32 texId, const uint8 *gray, int32 w, int32 h);
bool UploadImageRGBA(uint32 texId, const uint8 *rgba, int32 w, int32 h);
void Submit(const nkentseu::nkgui::NkGuiDrawList &dl, uint32 fbW, uint32 fbH);
```

C'est tout. `Init` lui donne le renderer 2D ; les deux `Upload`
enregistrent une texture sous un identifiant (un atlas de police, une image) ;
`Submit` traduit une liste de commandes en appels
`NkIRenderer2D`. Le chapitre 4 montrera la séquence complète.

### `NkGuiRHIBackend` — le pont vers NKRHI

L'alternative vit dans `Integrations/NKGui/NkGuiRHIBackend.h`, compilée
dans une bibliothèque statique `NKGuiIntegration`. Son API est le miroir
de la précédente, avec un tampon de commandes en plus :

**`Integrations/NKGui/NkGuiRHIBackend.h:45-59`**

```cpp
bool Init(NkIDevice* device, NkRenderPassHandle renderPass, NkGraphicsApi api);
void Destroy();
void Submit(NkICommandBuffer* cmd, const NkGuiDrawList& dl, uint32 fbW, uint32 fbH);
bool UploadTextureRGBA8(uint32 texId, const uint8* data, int32 width, int32 height);
bool UploadTextureGray8(uint32 texId, const uint8* data, int32 width, int32 height);
bool RegisterTexture(uint32 texId, NkTextureHandle texture);
bool HasTexture(uint32 texId) const noexcept;
```

La doctrine de répartition est écrite dans le fichier de build de l'intégration :

**`Integrations/NKGui/NKGuiIntegration.jenga:5-8`**

```
Rend une UI NKGui (nkgui::NkGuiDrawList) via NKRHI/NKRenderer, sans NKCanvas.
Sert aux applications 2D/3D (app d'animation, moteur de jeu). NKCanvas reste
le backend de l'IDE (NKCode).
```

Un point de conception intéressant de ce second pont : il enregistre aussi des
textures *externes* (`RegisterTexture`). C'est ainsi qu'une
application 3D affiche le rendu de sa scène dans un panneau de son interface :
elle rend la scène dans une texture hors écran, l'enregistre sous un
`texId`, et NKGui la dessine comme une image ordinaire.

### Pourquoi cette séparation ?

On pourrait trouver la construction alambiquée. Elle rapporte trois choses
concrètes :

1. **On peut changer de moteur de rendu sans toucher un seul
   widget.** L'IDE utilise NkCanvas, l'application d'animation utilise NKRHI,
   et le code des boutons est le même. Mieux : NKEditorKit rend cette
   substitution explicite, avec une interface
   `NkIEditorRenderer` injectable
   (`Engine/NKEditorKit/src/NKEditorKit/NkIEditorRenderer.h:7-15`).
2. **NKGui est testable sans GPU.** Une liste de commandes est une
   structure de données : on peut la produire, l'inspecter et la comparer
   sans jamais ouvrir de fenêtre.
3. **Les dépendances restent en arbre, pas en graphe.** NKGui ne
   connaît pas NkCanvas ; NkCanvas ne connaît pas NKGui ; l'application
   connaît les deux. Aucun cycle.

Il y a un prix, et il faut le connaître : **c'est à l'application de faire
le raccordement**. Personne ne le fera à votre place. Si vous oubliez d'appeler
`Submit`, rien ne s'affiche et aucun message d'erreur n'apparaît. Si vous
oubliez d'uploader l'atlas de police, tous les textes sont invisibles et aucun
message d'erreur n'apparaît non plus. Le chapitre 4 revient longuement sur ces
silences.

## Le chemin complet d'un clic, de bout en bout

Récapitulons en suivant un seul clic de souris, de l'appui jusqu'au pixel qui
change de couleur.

1. L'utilisateur appuie. Le système envoie un message ; `NKWindow` le
   reçoit, `NKEvent` le transforme en
   `NkMouseButtonPressEvent`.
2. La boucle appelle `PollEvent()` ; le *callback* enregistré
   écrit `ctx.input.mouseDown[0] = true`.
3. L'application appelle `ctx.BeginFrame(dt)`. NKGui en déduit la
   *transition* : `mouseClicked[0] = mouseDown[0] && !mousePrev[0]`.
4. L'application appelle `Button(ctx, "OK")`. Le widget calcule son
   rectangle, demande son identité, teste le survol, voit le clic, et
   retourne `true` — au *relâchement*, comme nous le verrons au
   chapitre 3. Au passage, il écrit deux rectangles et six sommets dans la
   liste de commandes.
5. L'application appelle `ctx.EndFrame()`. NKGui fusionne les listes
   des fenêtres par ordre de profondeur, ferme les popups, remet à zéro la
   molette et le texte consommés.
6. L'application appelle `target->Clear()`, puis soumet les deux
   listes au pont : `ctx.dl` d'abord, `ctx.dlOverlay`
   ensuite, chacune par `backend.Submit(liste, w, h)`. Le pont
   convertit chaque sommet, résout chaque `texId` en
   `NkTexture*`, applique chaque rectangle de découpe et appelle
   `DrawVertices`.
7. NkCanvas accumule tout dans ses tampons, regroupe par texture, et à la
   fermeture de l'image envoie un appel de dessin par groupe.
8. L'application appelle `target->Display()` : l'image est présentée.

> **✅ L'invariant d'ordre**
>
> `événements` → `ctx.BeginFrame(dt)` →
> `widgets` → `ctx.EndFrame()` →
> `Clear` → `Submit` × 2 →
> `Display`.
>
> Cet ordre n'est pas négociable et le chapitre 3 expliquera pourquoi
> `BeginFrame` doit venir *après* le pompage des événements.

## Deux mots sur les macros d'export

En lisant les en-têtes, vous rencontrerez partout des macros du genre
`NKENTSEU_NKGUI_API` :

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.h (forme générale)`**

```cpp
NKENTSEU_NKGUI_API void Text(NkGuiContext &ctx, const char *s) noexcept;
```

Par défaut, le module est construit en **statique** et cette macro est
**vide** (`Kernel/Runtime/NKGui/src/NKGui/NkGuiApi.h:32-40`). Quand
vous lisez une signature, effacez-la mentalement : il faut lire
`void Text(NkGuiContext &ctx, const char *s) noexcept`. Il existe deux
variantes : `NKENTSEU_NKGUI_CLASS_EXPORT` (classe entière) et
`NKENTSEU_NKGUI_API_INLINE` (fonction `inline` exportée). Dans
la suite de ce cours, nous les omettons pour la lisibilité.

## Exercices

> **✏️ 1 — Vérifier la carte soi-même**
>
> Ouvrez les trois fichiers de build suivants et relevez, pour chacun, la liste
> des modules dont il dépend :
> `Kernel/Runtime/NKGui/NKGui.jenga`,
> `Kernel/Runtime/NKCanvas/NKCanvas.jenga`,
> `Engine/NKEditorKit/NKEditorKit.jenga`.
> Dessinez le graphe. Vérifiez qu'il n'y a aucun cycle, et identifiez le seul
> module qui connaît à la fois NKGui et NKCanvas.

> **✏️ 2 — Trouver le pont**
>
> Ouvrez
> `Kernel/Runtime/NKCanvas/src/NKCanvas/UI/NkGuiCanvasBackend.h` et
> comptez : combien de méthodes publiques ? Combien de lignes au total ? Comparez
> avec les 4 973 lignes de `NkGuiWidgets.cpp`. Que vous dit ce rapport sur
> la quantité de travail nécessaire pour porter NKGui vers un nouveau moteur de
> rendu ?

> **✏️ 3 — Changer de backend à chaud**
>
> Reprenez la démo `NkCanvasDemo` du dossier Sandbox, qui accepte un
> argument de ligne de commande :
>
> ```
> NkCanvasDemo.exe --backend=opengl
> NkCanvasDemo.exe --backend=dx11
> NkCanvasDemo.exe --backend=sw
> ```
>
> Lancez-la avec chacun des backends et notez ce que vous observez. Recoupez avec
> le tableau d'état de la section 1.4 et avec `logs/app.log`. Ce petit
> exercice vous évitera plus tard de chercher un bug dans votre code alors que le
> problème est dans le choix du backend.
