# 7 — NKUI : l'interface utilisateur en mode immédiat

> Guide pédagogique pas à pas, dans l'esprit de la documentation SFML.
> On part de zéro et on ajoute une brique à la fois, avec du **code réel et compilable**.
> Pré-requis : avoir lu [NKCanvas](05-NKCanvas.md) (rendu 2D) et [NKEvent](03-NKEvent.md) (entrées).
> Retour au sommaire : [README](README.md).

---

## 7.1 Introduction — qu'est-ce qu'une UI en « mode immédiat » ?

**NKUI** est la couche d'interface utilisateur de Nkentseu. Elle fonctionne en
**mode immédiat** (immediate-mode), exactement comme *Dear ImGui* : il n'existe pas
d'arbre de widgets persistant que l'on crée puis met à jour. À la place, **chaque
frame** tu :

1. donnes l'état des entrées (souris / tactile / clavier),
2. décris ce que tu veux voir à l'écran (un rectangle ici, du texte là, un bouton ici),
3. demandes le rendu.

Il n'y a rien à « détruire » entre deux frames : tout est reconstruit. C'est très
naturel pour les jeux et les outils, parce que ton UI suit directement l'état de ton
programme — pas de synchronisation à maintenir.

Concrètement, NKUI ne dessine rien tout seul : il **remplit une liste de commandes de
dessin** (la *draw list*). Un **backend** se charge ensuite de transformer cette liste
en triangles pour le GPU. Dans ce guide, le backend est **`NkUICanvasBackend`** : il
relie NKUI au renderer 2D de [NKCanvas](05-NKCanvas.md). Tu profites donc de tous les
backends graphiques de NKCanvas (OpenGL, DX11, DX12, Vulkan, Software) sans rien
changer à ton code UI.

```
  Tes entrées (NKEvent)  ─────►  NkUIInputState
                                      │
                                      ▼
                         NkUIContext.BeginFrame()
                                      │  tu remplis ctx.dl  (la draw list)
                                      ▼
                         NkUIContext.EndFrame()
                                      │
                                      ▼
                  NkUICanvasBackend.Submit()  ──►  NkRenderer2D (NKCanvas)  ──►  GPU
```

> NKUI fournit aussi une grosse bibliothèque de widgets prêts à l'emploi (boutons,
> sliders, fenêtres flottantes, docking, menus, color picker…). Ce guide se concentre
> sur le **niveau dessin** (draw list + texte) et montre comment **construire ses propres
> widgets** — l'approche utilisée par les jeux du dépôt (Mú), idéale pour des interfaces
> sur mesure et tactiles.

---

## 7.2 Mise en place

Trois objets suffisent pour démarrer :

- **`NkUIContext`** — l'état de l'UI pour la frame courante (la draw list `ctx.dl`, le
  gestionnaire de polices `ctx.fontManager`, le thème, le viewport…).
- **`NkUICanvasBackend`** — le pont vers le rendu, relié à un `NkIRenderer2D` de NKCanvas.
- **`NkUIInputState`** — l'état des entrées que tu renseignes à chaque frame.

On suppose que tu as déjà une fenêtre et une cible de rendu NKCanvas (voir
[NKCanvas](05-NKCanvas.md)). Typiquement un `renderer::NkRenderWindow` qui expose un
`NkIRenderer2D*` via `GetRenderer()`.

```cpp
#include "NKUI/NKUI.h"                       // umbrella NKUI
#include "NKCanvas/UI/NkUICanvasBackend.h"   // le backend NKUI -> NKCanvas

using namespace nkentseu;
using namespace nkentseu::nkui;
using namespace nkentseu::renderer;

// ... quelque part dans ton application ...
NkRenderWindow*    renderTarget = /* créé depuis NKCanvas */;

NkUIContext        uiContext;
NkUICanvasBackend  uiBackend;
NkUIInputState     uiInput;     // rempli à chaque frame
```

### 7.2.1 Initialiser le contexte (avec la config de police)

`NkUIContext::Init` prend la taille du viewport (en pixels) et une
**`NkUIFontConfig`**. Cette config décide notamment de l'orientation de l'axe Y et de
l'atlas de police.

```cpp
NkUIFontConfig fontConfig;
fontConfig.yAxisUp              = false;  // Y vers le bas (comme l'écran / OpenGL)
fontConfig.enableAtlas          = true;   // glyphes haute qualité via atlas texture
fontConfig.enableBitmapFallback = true;   // police bitmap intégrée si l'atlas manque un glyphe
fontConfig.defaultFontSize      = 24.f;

if (!uiContext.Init(1280, 720, fontConfig)) {
    // échec d'initialisation
    return false;
}

uiContext.SetTheme(NkUITheme::Dark());     // thème prédéfini (cf. §7.8)
```

> `Init` doit recevoir la **vraie taille** de ta surface. Si la fenêtre est
> redimensionnée, mets à jour `uiContext.viewW` / `uiContext.viewH` (voir §7.10).

### 7.2.2 Relier le backend au renderer 2D

Le backend a besoin du `NkIRenderer2D` actif. Une seule fois après création du renderer :

```cpp
uiBackend.Init(renderTarget->GetRenderer());
```

C'est tout pour la plomberie. `NkUICanvasBackend` possède (et libère) les textures
qu'il crée pour l'UI (atlas de police, images), indexées par leur `texId` NKUI. Son
destructeur appelle `Destroy()` — rien à gérer à la main si l'objet vit aussi longtemps
que le renderer.

---

## 7.3 La boucle UI

Le cœur d'une frame NKUI tient en cinq étapes, toujours dans le même ordre :

1. **Renseigner les entrées** dans `NkUIInputState`.
2. **`BeginFrame(input, dt)`** — démarre la frame, réinitialise les draw lists.
3. **Remplir la draw list** (`ctx.dl`) — c'est là que tu décris ton UI.
4. **`EndFrame()`** — finalise les listes de commandes.
5. **`backend.Submit(ctx, fbW, fbH)`** — envoie tout au renderer (entre `Begin()`/`End()` de la cible).

```cpp
// Boucle principale (modèle "direct", cf. guides 0 et 2).
while (running) {
    float32 dt = clock.Tick().delta;   // temps écoulé (NKTime)

    // (1) ENTRÉES — voir §7.3.1
    uiInput.BeginFrame();              // remet les deltas/clics de la frame à zéro
    while (NkEvent* ev = NkEvents().PollEvent()) {
        // ... router les événements vers uiInput (voir §7.3.1) ...
    }
    uiInput.dt = dt;

    // (2) DÉBUT DE FRAME UI
    uiContext.BeginFrame(uiInput, dt);
    NkUIDrawList& dl = *uiContext.dl;  // la draw list courante

    // (3) DESSIN — tu décris ton interface ici (§7.4, §7.5, §7.6)
    dl.AddRectFilled(NkRect{ 0, 0, 1280, 720 }, NkColor{ 25, 28, 36, 255 });
    // ...

    // (4) FIN DE FRAME UI
    uiContext.EndFrame();

    // (5) RENDU
    renderTarget->Clear(NkColor{ 0, 0, 0, 255 });
    const math::NkVec2u sz = renderTarget->GetSize();
    uiBackend.Submit(uiContext, sz.x, sz.y);   // place du Submit : voir §7.9
    renderTarget->Display();
}
```

> `NkRect`, `NkVec2`, `NkColor` sont les alias géométriques exposés par NKUI (vers
> NKMath). Tu peux écrire `NkRect{ x, y, w, h }` directement.

### 7.3.1 Remplir `NkUIInputState` depuis NKEvent

NKUI ne connaît pas ton système d'événements : **tu** lui pousses l'état des entrées.
`NkUIInputState` possède des helpers (`SetMousePos`, `SetMouseButton`, `SetKey`,
`AddInputChar`, `AddMouseWheel`). Le branchement avec [NKEvent](03-NKEvent.md) est
direct. Exemple inspiré de Mú (souris **et** tactile unifiés) :

```cpp
auto& events = NkEvents();

events.AddEventCallback<NkMouseMoveEvent>([&](NkMouseMoveEvent* e) {
    uiInput.SetMousePos((float32)e->GetX(), (float32)e->GetY());
});
events.AddEventCallback<NkMouseButtonPressEvent>([&](NkMouseButtonPressEvent* e) {
    if (e->GetButton() == NkMouseButton::NK_MB_LEFT) uiInput.SetMouseButton(0, true);
});
events.AddEventCallback<NkMouseButtonReleaseEvent>([&](NkMouseButtonReleaseEvent* e) {
    if (e->GetButton() == NkMouseButton::NK_MB_LEFT) uiInput.SetMouseButton(0, false);
});

// Tactile : on mappe le 1er contact sur le "bouton gauche" virtuel.
events.AddEventCallback<NkTouchBeginEvent>([&](NkTouchBeginEvent* e) {
    if (e->GetNumTouches() == 0) return;
    const auto& t = e->GetTouch(0);
    uiInput.SetMousePos((float32)t.clientX, (float32)t.clientY);
    uiInput.SetMouseButton(0, true);
});
events.AddEventCallback<NkTouchEndEvent>([&](NkTouchEndEvent*) {
    uiInput.SetMouseButton(0, false);
});
```

Les transitions sont calculées pour toi :

- `uiInput.IsMouseDown(0)` — bouton actuellement enfoncé (état continu),
- `uiInput.IsMouseClicked(0)` — appuyé **cette frame** (front montant),
- `uiInput.IsMouseReleased(0)` — relâché **cette frame** (front descendant),
- `uiInput.mousePos` — position du curseur.

> **`BeginFrame()` sur l'input est obligatoire** : il remet à zéro `mouseClicked` /
> `mouseReleased` / les caractères saisis / la molette, et calcule `mouseDelta`. Sans
> lui, les fronts montants/descendants ne se réinitialisent jamais.

---

## 7.4 Dessiner des primitives

Tout passe par la draw list `ctx.dl` (`NkUIDrawList`). Les primitives les plus utiles :

```cpp
NkUIDrawList& dl = *uiContext.dl;

// Rectangle plein. Les 2 derniers paramètres = rayon d'arrondi (rx, ry). 0 = carré.
dl.AddRectFilled(NkRect{ 100, 100, 300, 120 }, NkColor{ 80, 160, 255, 255 }, 24.f, 24.f);

// Contour de rectangle : couleur, épaisseur, puis rayons d'arrondi.
dl.AddRect(NkRect{ 100, 100, 300, 120 }, NkColor{ 0, 0, 0, 255 }, 5.f, 24.f, 24.f);

// Cercle plein. Dernier paramètre = nombre de segments (0 = auto selon le rayon).
dl.AddCircleFilled(NkVec2{ 250, 400 }, 60.f, NkColor{ 255, 200, 40, 255 }, 0);

// Contour de cercle : couleur, épaisseur, segments.
dl.AddCircle(NkVec2{ 250, 400 }, 60.f, NkColor{ 0, 0, 0, 255 }, 4.f, 0);

// Ligne : a, b, couleur, épaisseur.
dl.AddLine(NkVec2{ 60, 60 }, NkVec2{ 500, 300 }, NkColor{ 255, 255, 255, 255 }, 3.f);
```

### 7.4.1 Images / textures

Pour afficher une image, tu as besoin d'un **`texId`** (un identifiant entier non nul).
Tu l'obtiens en uploadant des pixels via le backend :

```cpp
// Quelque part au chargement : on choisit un texId unique (!= 0) et on uploade.
uint32 myTexId = 1;
uiBackend.UploadTextureRGBA8(myTexId, pixelsRGBA, width, height);
// ou, pour une texture en niveaux de gris (étendue en blanc + alpha) :
// uiBackend.UploadTextureGray8(myTexId, pixelsGray, width, height);
```

Puis, dans la frame :

```cpp
// AddImage(texId, dst, uvMin, uvMax, tint)
dl.AddImage(myTexId,
            NkRect{ 200, 150, 256, 256 },
            NkVec2{ 0.f, 0.f }, NkVec2{ 1.f, 1.f },   // UV plein cadre
            NkColor{ 255, 255, 255, 255 });           // teinte (blanc = pas de modif)
```

La teinte multiplie la couleur de la texture : un `alpha < 255` donne un fondu, très
pratique pour les intros (voir l'exemple Mú en §7.11).

> D'autres primitives existent (`AddTriangleFilled`, `AddConvexPolyFilled`,
> `AddEllipseFilled`, `AddArc`, `AddBezierCubic`, le *path builder* `PathMoveTo` /
> `PathLineTo` / `PathStroke`…). Voir `NkUIDrawList.h` pour la liste complète.

---

## 7.5 Afficher du texte

C'est le point le plus important à comprendre dans NKUI.

> **RÈGLE — le texte se dessine via `NkUIFont::RenderText`, et `pos.y` est la BASELINE,
> pas le haut du texte.** La primitive `NkUIDrawList::AddText` existe mais peut être un
> *stub* (ne rien dessiner) : **n'utilise pas `dl.AddText`, utilise `font->RenderText`.**

### 7.5.1 Charger une police

Le gestionnaire de polices est accessible via `uiContext.fontManager`. Le plus simple
est de charger une **police embarquée** (compilée dans le binaire, zéro fichier à
livrer) avec `LoadEmbedded` :

```cpp
// LoadEmbedded(id, taillePx) -> index de police (>= 0), ou -1 si échec.
int32 bodyId = uiContext.fontManager.LoadEmbedded(NkEmbeddedFontId::DroidSans, 24.f);
if (bodyId < 0)
    bodyId = uiContext.fontManager.LoadEmbedded(NkEmbeddedFontId::ProggyClean, 18.f);

NkUIFont* bodyFont = uiContext.fontManager.Get(bodyId < 0 ? 0 : (uint32)bodyId);

// Une 2e police, plus grande, pour les titres.
int32 titleId = uiContext.fontManager.LoadEmbedded(NkEmbeddedFontId::DroidSans, 48.f);
NkUIFont* titleFont = uiContext.fontManager.Get(titleId < 0 ? 0 : (uint32)titleId);
if (!titleFont) titleFont = bodyFont;
```

> Tu peux aussi charger une police TTF/OTF depuis un fichier
> (`fontManager.LoadFromFile(path, sizePx)`) ou depuis la mémoire
> (`LoadFromMemory(...)`). Le module [NKFont](../Kernel/Runtime/NKFont/ROADMAP.md) fait
> le rasterizing ; NKUI gère l'atlas. Voir `NkUIFont.h`.

### 7.5.2 Dessiner une ligne de texte

`RenderText(dl, pos, texte, couleur, maxWidth=-1, ellipsis=false)`. Le `pos.y` est la
**baseline**. Pour positionner le texte à partir de son **haut** (ce qu'on veut
presque toujours), on ajoute l'ascender de la police :

```cpp
float32 topY = 80.f;   // haut souhaité du texte
if (bodyFont)
    bodyFont->RenderText(dl,
        NkVec2{ 40.f, topY + bodyFont->metrics.ascender },   // baseline = haut + ascender
        "Bonjour Nkentseu",
        NkColor{ 230, 230, 235, 255 });
```

### 7.5.3 Mesurer pour centrer

`MeasureWidth(texte)` renvoie la largeur en pixels. Pour centrer un texte dans une
zone `[x, x+w]` :

```cpp
void DrawTextCentered(NkUIDrawList& dl, NkUIFont* f,
                      float32 x, float32 w, float32 topY,
                      const char* s, NkColor c) {
    if (!f || !s) return;
    const float32 tx = x + (w - f->MeasureWidth(s)) * 0.5f;
    f->RenderText(dl, NkVec2{ tx, topY + f->metrics.ascender }, s, c);
}

// Centrer un titre sur toute la largeur de l'écran :
DrawTextCentered(dl, titleFont, 0.f, 1280.f, 40.f, "Menu principal",
                 NkColor{ 255, 210, 60, 255 });
```

Les métriques utiles dans `font->metrics` : `ascender`, `descender`, `lineHeight`,
`spaceWidth`. Pour empiler des lignes, avance de `lineHeight` à chaque ligne.

---

## 7.6 Construire un bouton enfant (exemple complet)

En mode immédiat, un bouton n'est qu'une combinaison « dessine un rectangle + un label,
et teste l'entrée ». Voici un bouton autonome, réutilisable, qui renvoie `true` quand
on **relâche** le pointeur dessus (comportement attendu d'un tap) :

```cpp
// Renvoie true si le bouton a été activé (relâché au-dessus).
bool UiButton(NkUIDrawList& dl, const NkUIInputState& in, NkUIFont* font,
              float32 x, float32 y, float32 w, float32 h,
              const char* label,
              NkColor bg, NkColor bgHover, NkColor fg, NkColor border)
{
    // 1) Survol : le pointeur est-il dans le rectangle ?
    const float32 px = in.mousePos.x, py = in.mousePos.y;
    const bool hovered = (px >= x && px < x + w && py >= y && py < y + h);

    // 2) Dessin : fond arrondi (couleur différente au survol) + contour.
    const float32 round = h * 0.28f;
    dl.AddRectFilled(NkRect{ x, y, w, h }, hovered ? bgHover : bg, round, round);
    dl.AddRect(NkRect{ x, y, w, h }, border, 5.f, round, round);

    // 3) Label centré (horizontal + vertical).
    if (font && label) {
        const float32 tx = x + (w - font->MeasureWidth(label)) * 0.5f;
        const float32 ty = y + (h - font->metrics.lineHeight) * 0.5f;
        font->RenderText(dl, NkVec2{ tx, ty + font->metrics.ascender }, label, fg);
    }

    // 4) Activation : relâché cette frame, et le pointeur est dessus.
    return hovered && in.IsMouseReleased(0);
}
```

Utilisation dans la frame :

```cpp
if (UiButton(dl, uiInput, bodyFont,
             540, 320, 200, 80, "Jouer",
             NkColor{ 60, 130, 90, 255 },    // fond
             NkColor{ 90, 180, 120, 255 },   // fond survolé
             NkColor{ 255, 255, 255, 255 },  // texte
             NkColor{ 0, 0, 0, 255 }))       // contour
{
    StartGame();   // déclenché au tap
}
```

> Le projet **Mú** encapsule exactement ce motif dans `MouFrame::Button` (cf.
> `Applications/Mou/src/Mou/Games/Common/MouFrame.h`) : un *wrapper* léger qui regroupe
> draw list + police + état du pointeur, avec des helpers `Rect`, `Border`, `Line`,
> `Circle`, `Image`, `Text`, `TextCentered`, `PointIn`, `Button`. C'est une excellente
> façon d'organiser une UI de jeu sans dépendre des widgets complets de NKUI.

---

## 7.7 Détecter survol et clic : le bon réflexe

Comme on reconstruit tout chaque frame, l'« état » d'un widget se déduit de **l'entrée
de la frame**, pas d'une variable figée :

- **Survol** = test géométrique `pointeur ∈ rect` (recalculé chaque frame).
- **Appui** = `in.IsMouseClicked(0)` (front montant cette frame).
- **Relâché** = `in.IsMouseReleased(0)` (front descendant cette frame).
- **Maintenu** (drag d'un slider) = `in.IsMouseDown(0)` + position courante.

Exemple : un **slider** horizontal 0..1 réglé tant que le pointeur est maintenu sur sa
zone (motif réel des Réglages de Mú) :

```cpp
void UiSlider(NkUIDrawList& dl, const NkUIInputState& in,
              float32 x, float32 y, float32 w, float32 h, float32& value /*0..1*/)
{
    // Rail + remplissage + pastille.
    dl.AddRectFilled(NkRect{ x, y, w, h }, NkColor{ 50, 50, 50, 255 }, h*0.5f, h*0.5f);
    dl.AddRectFilled(NkRect{ x, y, w * value, h }, NkColor{ 90, 200, 120, 255 }, h*0.5f, h*0.5f);
    const float32 kx = x + w * value, ky = y + h * 0.5f;
    dl.AddCircleFilled(NkVec2{ kx, ky }, h, NkColor{ 255, 210, 60, 255 }, 0);

    // Réglage tant que le pointeur est enfoncé sur la zone (avec une marge).
    const float32 px = in.mousePos.x, py = in.mousePos.y;
    if (in.IsMouseDown(0) && py >= y - 26.f && py <= y + h + 26.f
                          && px >= x - 26.f && px <= x + w + 26.f) {
        float32 v = (px - x) / w;
        value = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
    }
}
```

> Piège classique : tester `pressed` **et** la position du clic d'origine séparément.
> En mode immédiat, base-toi sur **l'état d'appui de la frame** (`IsMouseDown` /
> `IsMouseReleased`) et la **position courante**, sinon un survol « figé » d'une frame
> précédente reste surligné à tort.

---

## 7.8 Thèmes

NKUI fournit une palette sémantique (`NkUITheme`) : couleurs (`bgPrimary`, `accent`,
`border`, `textPrimary`…), métriques (paddings, rayons, hauteurs d'item) et 4 presets :

```cpp
uiContext.SetTheme(NkUITheme::Default());
uiContext.SetTheme(NkUITheme::Dark());
uiContext.SetTheme(NkUITheme::Minimal());
uiContext.SetTheme(NkUITheme::HighContrast());
```

Tu peux lire le thème courant et t'en servir pour colorer tes propres widgets :

```cpp
const NkUITheme& th = uiContext.GetTheme();
dl.AddRectFilled(NkRect{ 0, 0, 1280, 720 }, th.colors.bgPrimary);
// ... bouton accentué :
dl.AddRectFilled(rect, hovered ? th.colors.accentHover : th.colors.accent,
                 uiContext.GetCornerRadius(), uiContext.GetCornerRadius());
```

Le thème est aussi consommé automatiquement par les widgets complets de NKUI (Button,
Slider, fenêtres…) si tu choisis de les utiliser. Pour une UI de jeu, beaucoup de
projets préfèrent une **palette maison** (cf. `MouUIColor` dans Mú) et ne se servent du
thème que pour le fond et les accents.

---

## 7.9 Intégration avec NKCanvas — où placer `Submit`

`NkUICanvasBackend::Submit(ctx, fbW, fbH)` parcourt toutes les couches de la draw list
du contexte et les rend via le `NkIRenderer2D` de NKCanvas. **`fbW`/`fbH` sont la taille
du framebuffer** (sert au clipping et à la projection).

L'ordre de rendu dans une frame est donc :

```cpp
// 1) Préparer la frame UI (entrées -> begin -> dessin -> end), cf. §7.3.
uiContext.BeginFrame(uiInput, dt);
/* ... dl.AddRectFilled(...), font->RenderText(...) ... */
uiContext.EndFrame();

// 2) Rendre : effacer la cible, puis soumettre l'UI, puis présenter.
renderTarget->Clear(NkColor{ 18, 20, 26, 255 });
const math::NkVec2u sz = renderTarget->GetSize();
uiBackend.Submit(uiContext, sz.x, sz.y);   // dessine l'UI sur la cible
renderTarget->Display();                    // présente la frame
```

Avec un `NkRenderWindow`, `Submit` peut être appelé entre le `Clear` et le `Display` :
le renderer 2D gère lui-même son `Begin/End` interne. Si tu utilises une cible
bas-niveau qui expose un `Begin()/End()` explicite, place le `Submit` **entre** les deux
(c'est l'usage documenté dans `NkUICanvasBackend.h`).

> Tu peux dessiner ton **jeu** d'abord (sprites, monde) puis l'**UI** par-dessus :
> appelle d'abord le rendu du jeu, puis `uiBackend.Submit(...)`, puis `Display()`.
> L'UI sera composée au-dessus.

---

## 7.10 Redimensionnement (resize)

Quand la fenêtre change de taille, mets à jour le viewport du contexte **et** la cible :

```cpp
const math::NkVec2u sz = renderTarget->GetSize();
if (sz.x != lastW || sz.y != lastH) {
    if (lastW != 0 && sz.x > 0 && sz.y > 0) {
        renderTarget->OnResize(sz.x, sz.y);
        uiContext.viewW = (int32)sz.x;
        uiContext.viewH = (int32)sz.y;
    }
    lastW = sz.x; lastH = sz.y;
}
```

Comme l'UI est en mode immédiat, il n'y a rien d'autre à faire : la frame suivante sera
recalculée avec les nouvelles dimensions. (Vois aussi le piège « OnResize redondant au
démarrage » côté fenêtre/swapchain dans le guide [NKCanvas](05-NKCanvas.md).)

---

## 7.11 Exemple Mú : un menu réel de bout en bout

Le projet **Mú** (`Applications/Mou/`) est l'exemple de référence. Sa plateforme
(`MouPlatformApp.cpp`) initialise NKUI exactement comme ci-dessus puis dessine un menu
en grille de cartes. Extrait condensé d'une frame de menu :

```cpp
// (déjà fait : uiInput.BeginFrame() + routage des événements + uiInput.dt = dt)
uiContext.BeginFrame(uiInput, dt);
NkUIDrawList& dl = *uiContext.dl;

// Fond plein écran.
dl.AddRectFilled(NkRect{ 0, 0, W, H }, BG_CREAM);

// Titre centré (police de titre).
DrawTextCentered(dl, titleFont, 0.f, W, 40.f, "Mu", SUNNY);

// Une carte cliquable.
const NkRect card{ cx, cy, cardW, cardH };
const bool hovered = (px >= cx && px < cx + cardW && py >= cy && py < cy + cardH);

dl.AddRectFilled(NkRect{ cx, cy + 10.f, cardW, cardH }, SHADOW, 28.f, 28.f); // ombre
dl.AddRectFilled(card, cardColor, 28.f, 28.f);                              // carte
dl.AddRect(card, INK, hovered ? 8.f : 5.f, 28.f, 28.f);                     // contour réactif

// Icône (image uploadée au chargement) + libellés.
dl.AddImage(iconTexId, NkRect{ bcx - is*0.5f, bcy - is*0.5f, is, is });
DrawTextCentered(dl, titleFont, cx, cardW, txtY, "Couleurs", TEXT_ON_COLOR);

// Lancement au tap.
if (hovered && uiInput.IsMouseReleased(0))   // (Mú utilise son propre flag "pressedThisFrame")
    LaunchGame();

uiContext.EndFrame();
uiBackend.Submit(uiContext, (uint32)W, (uint32)H);
```

Et une **intro** avec image en fondu (teinte alpha animée), qui montre `AddImage` +
texte centré mesuré :

```cpp
uiContext.BeginFrame(uiInput, dt);
NkUIDrawList& dl = *uiContext.dl;

dl.AddRectFilled(NkRect{ 0, 0, W, H }, NkColor{ 255, 255, 255, 255 });

const uint8 alpha8 = (uint8)(255.f * fade);   // fade calculé selon le temps
dl.AddImage(logoTex, NkRect{ W*0.5f - lw*0.5f, logoY, lw, lh },
            NkVec2{ 0, 0 }, NkVec2{ 1, 1 },
            NkColor{ 255, 255, 255, alpha8 }); // teinte alpha = fondu

NkColor tc = brandColor; tc.a = alpha8;
titleFont->RenderText(dl,
    NkVec2{ (W - titleFont->MeasureWidth("Mu")) * 0.5f,
            titleY + titleFont->metrics.ascender },
    "Mu", tc);

uiContext.EndFrame();
uiBackend.Submit(uiContext, (uint32)W, (uint32)H);
```

> Tout le code ci-dessus provient directement de
> `Applications/Mou/src/Mou/Platform/MouPlatformApp.cpp` et du wrapper
> `Applications/Mou/src/Mou/Games/Common/MouFrame.h`. Va les lire : ce sont des
> exemples complets et fonctionnels (souris + tactile, multi-plateforme dont Android).

---

## 7.12 Pièges à connaître

1. **`dl.AddText` peut ne rien dessiner (stub).** Toujours dessiner le texte avec
   `NkUIFont::RenderText`. Rappelle-toi que **`pos.y` est la baseline** : passe
   `topY + font->metrics.ascender` pour aligner sur le haut.

2. **L'atlas de police est uploadé automatiquement par `Submit`.** `NkUICanvasBackend`
   détecte les atlas « dirty » et les pousse sur le GPU à la soumission. Tu n'as donc
   pas à uploader l'atlas toi-même — mais cela implique que `Submit` **doit** être
   appelé chaque frame pour que le texte s'affiche net (sinon, fallback bitmap crénelé).

3. **Le survol/clic se base sur l'APPUI de la frame, pas sur une position figée.**
   Recalcule `hovered` chaque frame et utilise `IsMouseClicked` / `IsMouseReleased` /
   `IsMouseDown`. Un état conservé d'une frame à l'autre cause des surlignages fantômes.

4. **`NkUIInputState::BeginFrame()` est indispensable** au début de chaque frame, avant
   de pousser les nouveaux événements : il réinitialise les fronts (clic/relâché),
   la molette et les caractères saisis, et calcule `mouseDelta`.

5. **Donne la bonne taille au contexte et à `Submit`.** `viewW`/`viewH` et les `fbW/fbH`
   de `Submit` doivent suivre la taille réelle de la surface (cf. §7.10), sinon clip et
   mise à l'échelle seront faux.

6. **`texId` doit être non nul et unique.** `AddImage` avec `texId == 0` ne dessine pas
   d'image (c'est la valeur réservée « couleur solide »). Choisis tes identifiants à
   partir de 1 et uploade-les via le backend avant de les utiliser.

7. **`Submit` entre `Begin()`/`End()` de la cible** si ta cible expose ce cycle
   explicitement (cf. en-tête `NkUICanvasBackend.h`). Avec `NkRenderWindow`, place-le
   simplement entre `Clear()` et `Display()`.

---

## 7.13 Récapitulatif

- NKUI est une UI **en mode immédiat** : chaque frame on renseigne l'entrée, on remplit
  une **draw list**, puis on soumet au rendu. Rien de persistant entre frames.
- Trois objets : **`NkUIContext`** (état + `dl` + `fontManager` + thème),
  **`NkUICanvasBackend`** (pont vers [NKCanvas](05-NKCanvas.md)),
  **`NkUIInputState`** (entrées, alimentées depuis [NKEvent](03-NKEvent.md)).
- Cycle : `input.BeginFrame()` → router les events → `ctx.BeginFrame()` → remplir
  `ctx.dl` → `ctx.EndFrame()` → `backend.Submit(ctx, w, h)`.
- Primitives via `dl` : `AddRectFilled` / `AddRect` (arrondis), `AddCircleFilled` /
  `AddCircle`, `AddLine`, `AddImage` (par `texId`).
- **Texte** : `font = fontManager.LoadEmbedded(...)`, puis `font->RenderText(dl, {x,
  topY + font->metrics.ascender}, ...)`, `font->MeasureWidth(...)` pour centrer.
- Widgets sur mesure : combine dessin + test d'entrée (`IsMouseReleased`,
  `IsMouseDown`) — voir `MouFrame::Button` / `UiButton` ci-dessus.
- Thèmes : `NkUITheme::Default/Dark/Minimal/HighContrast()`.

### Dépendances Jenga

Pour utiliser NKUI avec le backend NKCanvas, déclare dans ton `*.jenga` :

```python
nkentseudependson(
    ["NKUI",        # le module UI (draw list, texte, widgets, thèmes)
     "NKCanvas",    # fournit NkUICanvasBackend + le renderer 2D (le rendu)
     "NKFont",      # rasterizing des polices TTF/OTF (atlas)
     # + tes modules habituels : "NKWindow", "NKEvent", "NKMemory", "NKCore", "NKMath", ...
    ],
    extra_includes=["src"],
)
```

> Le backend `NkUICanvasBackend` est compilé côté NKCanvas et n'est actif que si NKCanvas
> est construit avec le support NKUI (flag `NK_CANVAS_WITH_NKUI`, cf. `NKCanvas.jenga`).
> Le header reste inclusible sans NKUI (il *forward-declare* `NkUIContext`).

---

Suite logique : assembler tout dans un mini-jeu — voir le
[Projet 2D complet](09-Projet-2D-complet.md). Retour au [sommaire](README.md).
Modules liés : [NKCanvas](05-NKCanvas.md) (rendu 2D), [NKEvent](03-NKEvent.md) (entrées).
