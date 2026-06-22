# NKCanvas — le rendu 2D pas à pas (style SFML)

> Guide 5 de la série Nkentseu. ← Retour au [README des guides](README.md).
> Pré-requis conseillés : [NKMemory](01-NKMemory.md), [NKWindow](02-NKWindow.md),
> [NKEvent](03-NKEvent.md), [NKImage](04-NKImage.md).
> Suite logique : [NKAudio](06-NKAudio.md), [NKUI](07-NKUI.md).
> Langue : français. Code réel, compilable, tiré du dépôt.

---

## 1. Introduction — c'est quoi NKCanvas ?

**NKCanvas** est la couche de **rendu 2D conviviale** de Nkentseu. C'est l'équivalent
de la partie *Graphics* de **SFML** : tu disposes d'une **fenêtre de rendu**, tu y
dessines des **sprites**, des **formes**, du **texte**, tu appliques des
**transformations** (position, rotation, échelle), et tu peux descendre au niveau des
**vertex arrays** quand tu veux du sur-mesure.

Ce qui la distingue de SFML :

- **Multi-backend**. Le même code de dessin tourne sur **OpenGL / OpenGL ES / WebGL,
  Vulkan, DirectX 11, DirectX 12, Metal et un rasterizer logiciel (Software)**. Tu choisis
  l'API au démarrage, ou tu laisses NKCanvas décider (mode `AUTO`).
- **Zero-STL**. Conteneurs et types maison (`NkVector`, `NkString`, `NkVec2f`, `NkColor`…).
- Elle s'appuie sur les autres modules : la **fenêtre native** vient de
  [NKWindow](02-NKWindow.md), le décodage d'images vient de [NKImage](04-NKImage.md), les
  polices viennent de NKFont.

Où se situe NKCanvas dans la pile ?

```
NKUI (widgets immédiats)            ← guide 7
NKCanvas (rendu 2D : ce guide)      ← sprites, formes, texte, vertex arrays
   ├── NKWindow  (fenêtre native)   ← guide 2
   ├── NKImage   (pixels CPU)       ← guide 4
   └── NKFont    (rasterisation glyphes)
backend GPU : OpenGL / Vulkan / DX11 / DX12 / Software
```

Tous les types de rendu vivent dans le namespace `nkentseu::renderer`.

> **À retenir** : NKCanvas est la brique « SFML-like ». Pour de l'**interface utilisateur**
> (boutons, panneaux, sliders), regarde plutôt [NKUI](07-NKUI.md), qui dessine *par-dessus*
> NKCanvas. Pour du **3D avancé** (PBR, ombres), c'est NKRenderer — hors de ce guide.

### Les classes que tu vas rencontrer

| Classe | Rôle | Analogue SFML |
|--------|------|---------------|
| `NkRenderWindow` | cible de rendu liée à une fenêtre | `sf::RenderWindow` |
| `NkRenderTexture` | cible de rendu offscreen (texture) | `sf::RenderTexture` |
| `NkTexture` | texture GPU (chargée depuis une image) | `sf::Texture` |
| `NkSprite` | quad texturé, déplaçable | `sf::Sprite` |
| `NkRectangleShape`, `NkCircleShape`, `NkConvexShape`, `NkLineShape` | formes | `sf::*Shape` |
| `NkText` + `NkFont` | texte + police | `sf::Text` + `sf::Font` |
| `NkTransformable` | base position/rotation/échelle/origine | `sf::Transformable` |
| `NkVertexArray` + `NkVertex` | géométrie brute batchée | `sf::VertexArray` |
| `NkRenderStates` | état d'un draw (transform, blend, texture, shader) | `sf::RenderStates` |
| `NkShader` / `NkMaterial` | shaders et effets | `sf::Shader` |

---

## 2. Créer une fenêtre de rendu

NKCanvas ne crée pas la fenêtre : tu lui passes une `NkWindow` (voir
[guide NKWindow](02-NKWindow.md)) et un descripteur de contexte `NkContextDesc` qui dit
**quelle API graphique** utiliser. La classe `NkRenderWindow` se charge alors de créer le
**contexte GPU** + le **moteur de rendu 2D** par-dessus.

### 2.1 Le minimum vital

```cpp
#include "NKWindow/Core/NkWindow.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkGraphicsApi.h"

using namespace nkentseu;
using namespace nkentseu::renderer;

int nkmain(const NkEntryState& state) {
    // 1) La fenêtre native (NKWindow).
    NkWindow window;
    NkWindowConfig cfg;
    cfg.title  = "Hello NKCanvas";
    cfg.width  = 1280;
    cfg.height = 720;
    if (!window.Create(cfg)) return -1;

    // 2) Choisir le backend graphique.
    NkContextDesc desc;
    desc.api = NkGraphicsApi::NK_GFX_API_OPENGL;   // OpenGL ici

    // 3) La cible de rendu = contexte + renderer 2D, liés à la fenêtre.
    NkRenderWindow target(window, desc);
    if (!target.IsValid()) return -1;

    // ... boucle de rendu (section 3) ...
    return 0;
}
```

`NkRenderWindow` **possède** le contexte GPU : à la destruction, il détruit le renderer
puis le contexte dans le bon ordre. Tu n'as rien à libérer toi-même.

> `nkmain` est le point d'entrée portable de Nkentseu (voir le
> [README des guides](README.md), section 0). Sur les applications du dépôt (Mú…) on
> retrouve souvent un objet `App` avec `Initialize()` / `Run()` — c'est la même chose
> répartie en méthodes.

### 2.2 Choisir l'API graphique

Les valeurs de `NkContextDesc::api` (énum `NkGraphicsApi`) :

```cpp
NkGraphicsApi::NK_GFX_API_AUTO       // laisse NKCanvas/ta plateforme décider
NkGraphicsApi::NK_GFX_API_OPENGL     // OpenGL 3.3+ (desktop)
NkGraphicsApi::NK_GFX_API_OPENGLES   // OpenGL ES 2/3 (mobile)
NkGraphicsApi::NK_GFX_API_WEBGL      // WebGL (navigateur, Emscripten)
NkGraphicsApi::NK_GFX_API_VULKAN     // Vulkan 1.0+
NkGraphicsApi::NK_GFX_API_DX11       // DirectX 11 (Windows)
NkGraphicsApi::NK_GFX_API_DX12       // DirectX 12 (Windows / Xbox)
NkGraphicsApi::NK_GFX_API_METAL      // Metal (macOS/iOS) — contexte seulement
NkGraphicsApi::NK_GFX_API_SOFTWARE   // rasterizer CPU, fallback universel
```

Helper pratique : `NkGraphicsApiName(api)` renvoie le nom lisible (`"OpenGL"`, `"Vulkan"`,
`"DirectX 11"`…), parfait pour un log.

État de maturité (mai 2026) : **OpenGL** est le chemin le mieux validé partout.
DX11/DX12/Vulkan/Software fonctionnent mais demandent encore du polissage selon la
plateforme (voir section 14, Pièges).

### 2.3 Le mode AUTO (choix automatique)

`NK_GFX_API_AUTO` signifie « choisis pour moi ». En pratique, beaucoup d'apps font la
résolution elles-mêmes pour rester explicites. Voici le pattern réel de l'application **Mú**
(`Applications/Mou/src/Mou/Platform/MouPlatformApp.cpp`) :

```cpp
NkContextDesc desc;
desc.api = mGraphicsApi;                  // ce que l'utilisateur a demandé (peut être AUTO)
if (desc.api == NkGraphicsApi::NK_GFX_API_AUTO) {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    desc.api = NkGraphicsApi::NK_GFX_API_DX11;     // par défaut sur Windows
#else
    desc.api = NkGraphicsApi::NK_GFX_API_OPENGL;   // ailleurs
#endif
}

mRenderTarget = new NkRenderWindow(mWindow, desc);
if (!mRenderTarget || !mRenderTarget->IsValid()) {
    MOU_LOG_ERROR("Impossible d'initialiser NkRenderWindow");
    delete mRenderTarget; mRenderTarget = nullptr;
    return false;
}
MOU_LOG_INFOF("Backend graphique: %s", NkGraphicsApiName(desc.api));
```

> **Note mémoire** : ici Mú utilise `new`/`delete` parce que le `NkRenderWindow` est un
> membre du cycle de vie de l'app. En suivant la règle d'or NKMemory
> ([guide 1](01-NKMemory.md)), tu peux aussi le stocker **par valeur** (variable locale,
> comme en section 2.1) — c'est le plus simple et c'est ce que fait le squelette SFML.

### 2.4 Fallback automatique avancé

Si tu veux *essayer plusieurs backends dans l'ordre* (par exemple Vulkan, puis OpenGL, puis
Software), tu peux créer le contexte toi-même via la factory et le passer au constructeur
« avancé » de `NkRenderWindow` :

```cpp
#include "NKCanvas/Factory/NkContextFactory.h"

NkContextDesc desc;
desc.api = NkGraphicsApi::NK_GFX_API_VULKAN;
NkIGraphicsContext* ctx = NkContextFactory::CreateWithFallback(window, desc);
// ctx vise Vulkan puis retombe sur GL/Software si indisponible.

NkRenderWindow target(window, ctx);   // NKCanvas NE possède PAS ctx ici :
                                       // c'est à toi de le détruire APRÈS le target.
```

Avec ce constructeur, **c'est toi** qui possèdes le contexte (tu dois le détruire toi-même,
après le `NkRenderWindow`). Avec le constructeur de la section 2.1, NKCanvas s'en charge.

---

## 3. La boucle de rendu

Le cycle 2D est identique à SFML : à chaque image, on **efface** (`Clear`), on **dessine**
(`Draw`), on **présente** (`Display`).

```cpp
while (window.IsOpen()) {
    // 1) Événements (voir guide NKEvent)
    while (NkEvent* ev = NkEvents().PollEvent()) {
        // réagir aux entrées, à la fermeture, au resize...
    }

    // 2) Effacer l'écran avec une couleur de fond.
    target.Clear(NkColor2D{ 30, 30, 30, 255 });   // gris foncé opaque

    // 3) Dessiner.
    target.Draw(/* sprite / forme / texte / vertex array */);

    // 4) Présenter le résultat (swap des buffers).
    target.Display();
}
```

Points clés :

- `NkColor2D` est un alias de `math::NkColor` : composantes **RGBA 0–255** (`uint8`). Des
  constantes existent : `NkColor2D::Black`, `White`, `Red`, `Green`, `Blue`, `Yellow`,
  `Cyan`, `Magenta`, `Transparent`.
- `Clear()` sans argument efface en **noir**.
- **L'ordre compte** : tout `Draw` doit être entre `Clear` et `Display`. Ce qui est dessiné
  en dernier passe **au-dessus** (pas de tri en Z par défaut).
- `Display()` termine la frame (présentation / swapchain). Ne dessine plus rien après.

Extrait réel de la boucle de **Mú** (simplifié) :

```cpp
while (mRunning && mWindow.IsOpen()) {
    float32 dt = mClock.Tick().delta;        // delta-temps en secondes

    mInput.BeginFrame();
    while (NkEvent* ev = NkEvents().PollEvent()) { /* router l'event */ }

    // ... update logique avec dt ...

    mRenderTarget->Clear(C::BG_CREAM());      // couleur de fond
    // ... Draw de la scène courante ...
    mRenderTarget->Display();
}
```

> `mClock.Tick().delta` donne le temps écoulé depuis la frame précédente (en secondes,
> `float32`). C'est ce qu'on multiplie aux vitesses pour une animation indépendante du
> framerate.

---

## 4. Dessiner une forme

Les formes héritent toutes de `NkShape` (lui-même `NkTransformable` + `NkDrawable`). On les
crée une fois, on configure leur couleur/contour, puis on les dessine chaque frame. C'est
plus efficace que de redessiner des primitives à la volée, car les sommets ne sont
régénérés que si tu changes un paramètre.

### 4.1 Rectangle

```cpp
#include "NKCanvas/Renderer/Shapes/NkRectangleShape.h"

NkRectangleShape rect({ 120.f, 60.f });     // taille : largeur × hauteur
rect.SetPosition({ 200.f, 150.f });
rect.SetFillColor({ 255, 128, 0, 255 });    // orange opaque
rect.SetOutlineColor(NkColor2D::White);
rect.SetOutlineThickness(3.f);

target.Draw(rect);                          // dans la boucle, entre Clear et Display
```

API commune à toutes les formes (depuis `NkShape`) :

| Méthode | Effet |
|---------|-------|
| `SetFillColor(color)` | couleur de remplissage |
| `SetOutlineColor(color)` | couleur du contour |
| `SetOutlineThickness(t)` | épaisseur du contour (0 = pas de contour) |
| `SetTexture(const NkTexture*)` | plaque une texture sur la forme |
| `SetTextureRect(NkRect2f)` | sous-région de texture (UV) |
| `GetLocalBounds()` / `GetGlobalBounds()` | boîtes englobantes (local / monde) |

### 4.2 Cercle

```cpp
#include "NKCanvas/Renderer/Shapes/NkCircleShape.h"

NkCircleShape circle(50.f);          // rayon
circle.SetPointCount(64);            // nb de segments (32 par défaut) → plus lisse
circle.SetPosition({ 400.f, 300.f });
circle.SetFillColor(NkColor2D::Cyan);

target.Draw(circle);
```

Le cercle est centré localement en `(radius, radius)` : sa boîte locale commence à `(0,0)`,
comme un rectangle. Pour le faire tourner autour de son centre, mets
`circle.SetOrigin({ radius, radius })` (voir section 5).

### 4.3 Polygone convexe et segment

```cpp
#include "NKCanvas/Renderer/Shapes/NkConvexShape.h"
#include "NKCanvas/Renderer/Shapes/NkLineShape.h"

// Pentagone (5 sommets définis en coords locales).
NkConvexShape pent(5);
pent.SetPoint(0, {  50.f,   0.f });
pent.SetPoint(1, { 100.f,  35.f });
pent.SetPoint(2, {  80.f,  90.f });
pent.SetPoint(3, {  20.f,  90.f });
pent.SetPoint(4, {   0.f,  35.f });
pent.SetPosition({ 600.f, 300.f });
pent.SetFillColor(NkColor2D::Magenta);
target.Draw(pent);

// Segment épais entre deux points.
NkLineShape line({ 100.f, 500.f }, { 700.f, 550.f }, 5.f);  // p1, p2, épaisseur
line.SetFillColor(NkColor2D::Yellow);
target.Draw(line);
```

> **Limite** : le remplissage utilise une triangulation en éventail, valable uniquement pour
> des polygones **convexes**. Pour du concave, découpe-le toi-même en plusieurs convexes.

### 4.4 Variante « ponctuelle » via NkRenderer2D

Quand tu veux juste tracer une primitive sans garder d'objet, passe par la façade
`NkRenderer2D` (accessible avec `target.GetRenderer2D()`) :

```cpp
NkRenderer2D& r = target.GetRenderer2D();

r.DrawLine({ 10, 10 }, { 200, 50 }, NkColor2D::Red, 2.f);
r.DrawFilledRect({ 50, 50, 100, 80 }, NkColor2D::Blue);
r.DrawRect({ 200, 50, 60, 60 }, NkColor2D::White, 2.f, NkColor2D::Yellow);  // contour
r.DrawCircle({ 400, 300 }, 50.f, NkColor2D::Green, 64);                     // 64 segments
r.DrawFilledTriangle({ 500, 100 }, { 550, 200 }, { 450, 200 }, NkColor2D::Magenta);
r.DrawPoint({ 600, 400 }, NkColor2D::White, 4.f);
```

Utilise les `NkShape` persistantes pour les objets de la scène, et `NkRenderer2D` pour le
debug / les tracés jetables.

---

## 5. Les transformations

Toute classe dérivée de `NkTransformable` (donc toutes les formes) expose les mêmes
opérations que `sf::Transformable` :

```cpp
NkRectangleShape r({ 60.f, 60.f });

r.SetPosition({ 400.f, 300.f });   // position monde
r.SetOrigin({ 30.f, 30.f });       // pivot AU CENTRE du rectangle (60/2)
r.SetRotation(0.5f);               // angle en RADIANS, autour de l'origine
r.SetScale({ 2.f, 1.f });          // échelle (x doublé, y inchangé)

// Mutators incrémentaux (typiquement dans la boucle, par frame) :
r.Move({ 1.f, 0.f });              // décale
r.Rotate(0.01f);                   // ajoute à la rotation (radians)
r.Scale({ 1.01f, 1.f });           // multiplie l'échelle

target.Draw(r);
```

Le modèle de composition est celui de SFML :

```
T = Translate(position) · Rotate(rotation) · Scale(scale) · Translate(-origin)
```

Autrement dit, un point local `p` devient :
`monde = position + Rotate(scale · (p - origin))`.

L'**origine** (`origin`) est le pivot autour duquel la rotation et l'échelle s'appliquent.
Pour faire tourner une forme autour de son centre, place l'origine en son centre.

> **Piège des unités d'angle.** `NkTransformable` (donc `NkShape` et les formes) travaille en
> **radians**. En revanche, `NkSprite::SetRotation` / `NkText::SetRotation` prennent des
> **degrés** (conversion interne en radians). Garde ça en tête : `shape.SetRotation(0.5f)`
> ≈ 28°, alors que `sprite.SetRotation(0.5f)` ≈ 0,5°.

### Composer des transforms à la main

Tu peux multiplier des `NkTransform` pour des hiérarchies (parent → enfant) :

```cpp
#include "NKCanvas/Renderer/Core/NkTransform.h"

NkTransform parent;
parent.Translate({ 100.f, 100.f }).Rotate(0.5f);

NkTransform child;
child.Translate({ 50.f, 0.f }).Scale({ 2.f, 2.f });

NkTransform world = parent * child;          // child appliqué d'abord, puis parent
const float32* gpuMatrix = world.GetMatrix();// 16 floats column-major (upload GPU direct)
```

---

## 6. Charger une texture et dessiner un sprite

### 6.1 La texture (NkTexture)

`NkTexture` est l'objet **GPU**. Il se construit à partir d'un fichier image (décodé par
[NKImage](04-NKImage.md), puis uploadé sur le GPU) ou directement depuis une `NkImage` en
mémoire. Comme c'est une ressource GPU, il faut lui passer le **renderer** du target.

```cpp
#include "NKCanvas/Renderer/Resources/NkTexture.h"

NkTexture tex;
if (!tex.LoadFromFile(*target.GetRenderer(), "assets/sprite.png")) {
    // gérer l'échec de chargement
}
tex.SetSmooth(true);     // filtrage linéaire (lisse) — false = NEAREST (pixel-art)
tex.SetRepeated(false);  // mode de wrap (REPEAT si true)
```

Autres entrées : `LoadFromImage(renderer, image, area)` (depuis une `NkImage` déjà chargée,
voir [guide NKImage](04-NKImage.md)) et `LoadFromMemory(renderer, data, sizeBytes)` (depuis
un buffer encodé en RAM). On crée une texture vierge avec
`Create(renderer, w, h, fillColor)` (utile pour la mettre à jour dynamiquement via
`Update(...)`).

Filtre et wrap finement :

```cpp
tex.SetFilter(NkTextureFilter::NK_NEAREST);   // ou NK_LINEAR
tex.SetWrap(NkTextureWrap::NK_REPEAT);         // NK_CLAMP / NK_REPEAT / NK_MIRROR_REPEAT
```

`NkTexture` est **non-copiable** (elle possède une ressource GPU) mais **déplaçable**
(move). Stocke-la donc par valeur dans un membre, ou via un smart pointer NKMemory.

### 6.2 Le sprite (NkSprite)

`NkSprite` est un quad texturé déplaçable :

```cpp
#include "NKCanvas/Renderer/Resources/NkSprite.h"

NkSprite sprite(tex);                 // construit depuis la texture
sprite.SetPosition({ 100.f, 100.f });
sprite.SetColor({ 255, 255, 255, 255 });  // teinte (blanc = couleurs d'origine)

target.Draw(sprite);
```

Découper une feuille de sprites (tileset) avec un `NkRect2i` (pixels) :

```cpp
NkSprite tile(tileset, NkRect2i{ 0, 0, 32, 32 });   // sous-rectangle 32×32 en haut-gauche
tile.SetTextureRect(NkRect2i{ 64, 0, 32, 32 });     // changer de tuile à tout moment
```

Le sprite a les mêmes setters de transform que les formes, plus le retournement :

```cpp
sprite.SetOrigin({ 16.f, 16.f });   // centre, pour tourner autour
sprite.SetRotation(45.f);           // ⚠ DEGRÉS pour NkSprite (≠ radians des shapes)
sprite.SetScale({ 2.f, 2.f });
sprite.SetFlipX(true);              // miroir horizontal
sprite.SetColor({ 255, 255, 255, 128 });  // alpha 128 = semi-transparent
```

`GetLocalBounds()` / `GetGlobalBounds()` renvoient les boîtes englobantes (utile pour le
clic ou la collision grossière).

> **À retenir** : la texture doit **survivre** au sprite — le sprite garde un pointeur vers
> elle, il ne la copie pas. Ne laisse pas une `NkTexture` locale mourir avant ses sprites.

---

## 7. Afficher du texte

Le texte combine une **police** (`NkFont`, qui s'appuie sur le module NKFont) et un objet
**texte** (`NkText`).

```cpp
#include "NKCanvas/Renderer/Resources/NkFont.h"
#include "NKCanvas/Renderer/Resources/NkSprite.h"   // contient aussi NkText

NkFont font;
if (!font.LoadFromFile(*target.GetRenderer(), "assets/Roboto-Regular.ttf")) {
    // gérer l'échec
}

NkText title(font, "Bonjour NKCanvas !", /*characterSize=*/32);
title.SetFillColor(NkColor2D::White);
title.SetOutlineColor(NkColor2D::Black);
title.SetOutlineThickness(1.f);
title.SetPosition({ 50.f, 50.f });

target.Draw(title);
```

Réglages de `NkText` :

```cpp
title.SetString("Nouveau texte");
title.SetCharacterSize(48);
title.SetStyle(NkTextStyle::NK_BOLD);     // REGULAR / BOLD / ITALIC / UNDERLINED / ...
title.SetLetterSpacing(1.1f);             // facteur d'espacement entre lettres
title.SetLineSpacing(1.2f);               // facteur d'interligne
NkRect2f bounds = title.GetLocalBounds(); // mesurer pour centrer
```

> **Le texte de NKCanvas vs NKUI.** `NkText` est parfait pour du texte « libre » dans une
> scène (titres, scores, dialogues). Pour une **interface** (boutons, libellés de panneaux,
> champs), passe par [NKUI](07-NKUI.md), qui a son propre système de polices et de mise en
> page. D'ailleurs, l'app **Mú** dessine tout son texte via NKUI (`NkUIFont::RenderText`),
> pas via `NkText` — c'est un choix légitime selon le besoin.

---

## 8. Le rendu bas niveau : NkVertex + NkVertexArray

Pour de la géométrie sur-mesure (particules, tilemaps, effets), descends au niveau des
**sommets**. Un `NkVertex` est un POD : position, coordonnées de texture, couleur RGBA.

```cpp
struct NkVertex2D {
    float32 x, y;          // position (coords monde / locales selon le transform)
    float32 u, v;          // texture coords (0..1)
    uint8   r, g, b, a;    // couleur
};
using NkVertex = NkVertex2D;
```

Un `NkVertexArray` regroupe des sommets + un **type de primitive** :

```cpp
#include "NKCanvas/Renderer/Core/NkVertexArray.h"

NkVertexArray va(NkPrimitiveType::NK_TRIANGLES, 3);
//                 x      y     u    v    r    g    b    a
va[0] = NkVertex{ 100.f, 100.f, 0.f, 0.f, 255,   0,   0, 255 };
va[1] = NkVertex{ 200.f, 100.f, 1.f, 0.f,   0, 255,   0, 255 };
va[2] = NkVertex{ 150.f, 200.f, 0.5f,1.f,   0,   0, 255, 255 };

target.Draw(va);                 // un triangle dégradé rouge/vert/bleu
```

Types de primitives disponibles (`NkPrimitiveType`) :
`NK_POINTS`, `NK_LINES`, `NK_LINE_STRIP`, `NK_TRIANGLES`, `NK_TRIANGLE_STRIP`,
`NK_TRIANGLE_FAN`.

Construire dynamiquement (ex. nuage de points) :

```cpp
NkVertexArray pts(NkPrimitiveType::NK_POINTS);
pts.Reserve(1000);                          // évite les réallocations
for (uint32 i = 0; i < 1000; ++i) {
    pts.Append(NkVertex{ x[i], y[i], 0, 0, 255, 255, 255, 255 });
}
target.Draw(pts);
```

Tu peux aussi soumettre un buffer brut sans passer par `NkVertexArray` :

```cpp
NkVertex verts[3] = { /* ... */ };
target.Draw(verts, 3, NkPrimitiveType::NK_TRIANGLES);
```

---

## 9. Les render states (transform, blend, texture, shader)

`NkRenderStates` regroupe les 4 paramètres qui pilotent un `Draw` :

```cpp
struct NkRenderStates {
    NkTransform        transform;                       // matrice 2D appliquée aux vertices
    NkBlendMode        blendMode = NkBlendMode::NK_ALPHA;
    const NkTexture*   texture   = nullptr;             // nullptr = couleur vertex seule
    const NkShader*    shader    = nullptr;             // nullptr = pipeline 2D par défaut
};
```

Exemple : dessiner un vertex array texturé, en additif, décalé de (100, 100) :

```cpp
NkRenderStates st = NkRenderStates::Default();
st.texture   = &particleTex;
st.blendMode = NkBlendMode::NK_ADD;            // ALPHA / ADD / MULTIPLY / NONE
st.transform.Translate({ 100.f, 100.f }).Rotate(0.5f);

target.Draw(particleVertexArray, st);
```

Modes de blending (`NkBlendMode`) :

| Mode | Usage typique |
|------|---------------|
| `NK_ALPHA` | transparence standard (par défaut) |
| `NK_ADD` | feu, lueurs, additif |
| `NK_MULTIPLY` | ombres, assombrissement |
| `NK_NONE` | écrasement, pas de blending |

Les drawables qui héritent de `NkTransformable` (sprites, formes) **composent
automatiquement** leur transform local au `states.transform` que tu fournis. Le `transform`
de `NkRenderStates` agit donc comme un transform **parent**.

---

## 10. Caméra 2D (view), viewport et conversion de coordonnées

Une `NkView2D` est une caméra orthographique (comme `sf::View`) : un centre, une taille
visible en coordonnées monde, et une rotation.

```cpp
NkView2D view;
view.center   = { 400.f, 300.f };   // ce point est au centre de l'écran
view.size     = { 800.f, 600.f };   // surface visible en coords monde
view.rotation = 0.f;
target.SetView(view);

// Revenir à la vue par défaut (1:1 avec la fenêtre).
target.SetView(target.GetDefaultView());
```

Convertir entre pixels écran et coordonnées monde (essentiel pour le clic souris) :

```cpp
NkVec2f world = target.MapPixelToCoords({ mouseX, mouseY });  // pixel → monde
NkVec2i pixel = target.MapCoordsToPixel({ wx, wy });          // monde → pixel
```

La taille courante de la cible : `target.GetSize()` renvoie un `math::NkVec2u` (largeur,
hauteur en pixels). C'est ce qu'on utilise pour positionner des éléments en pourcentage
d'écran. Le viewport (zone de dessin en pixels) se règle avec
`SetViewport(NkRect2i)` / `GetViewport()`.

---

## 11. Render to texture (NkRenderTexture)

`NkRenderTexture` est une cible de rendu **offscreen** : tu dessines dedans, puis tu
utilises le résultat comme une texture (post-process, mini-map, capture, blit d'UI).

```cpp
#include "NKCanvas/Renderer/Targets/NkRenderTexture.h"

NkRenderTexture rt;
if (rt.Create(*target.GetRenderer(), 512, 512)) {
    rt.Clear({ 0, 0, 0, 255 });
    rt.Draw(scene);          // rendu dans le FBO offscreen
    rt.Display();            // termine, restaure le framebuffer principal

    // Le résultat est exploitable comme une NkTexture :
    NkTexture rtTex;
    rtTex.SetGPUId(rt.GetColorTextureGPUId());

    NkSprite post(rtTex);
    post.SetPosition({ 100.f, 100.f });
    target.Draw(post);       // affiche le contenu offscreen sur l'écran
}
```

> **État (mai 2026)** : `NkRenderTexture` est implémenté en **OpenGL** (FBO réel). Sur les
> autres backends (Vulkan / DX11 / DX12 / Software / Metal), `Create()` renvoie `false`
> (API posée, implémentation différée). Vérifie toujours la valeur de retour de `Create()`.

---

## 12. Shaders et matériaux (aperçu)

NKCanvas a son propre petit système de shaders, **autonome** (pas de dépendance NKRHI). Tu
fournis le source dans le langage du backend (GLSL pour OpenGL, HLSL pour DX, etc.).

### 12.1 NkShader

```cpp
#include "NKCanvas/Renderer/Resources/NkShader.h"

const char* vs = R"(#version 330 core
    layout(location=0) in vec2 aPos;
    layout(location=1) in vec2 aUV;
    layout(location=2) in vec4 aCol;
    uniform mat4 u_Projection;
    out vec2 vUV; out vec4 vCol;
    void main() { gl_Position = u_Projection * vec4(aPos,0,1); vUV=aUV; vCol=aCol; }
)";
const char* fs = R"(#version 330 core
    in vec2 vUV; in vec4 vCol;
    uniform sampler2D u_Texture; uniform float uTime;
    out vec4 oColor;
    void main() {
        vec2 uv = vUV; uv.x = fract(vUV.x + uTime*0.3);  // scroll horizontal
        oColor = texture(u_Texture, uv) * vCol;
    }
)";

NkShader scroll;
scroll.SetSourceGLSL(vs, fs);
scroll.Compile(*target.GetRenderer());

// Dans la boucle :
scroll.SetFloat("uTime", currentTime);
NkRenderStates st = NkRenderStates::Default();
st.shader  = &scroll;
st.texture = &myTexture;
target.Draw(sprite, st);
```

Uniformes par nom : `SetFloat/SetVec2/SetVec3/SetVec4/SetMat4/SetColor/SetTexture`.

### 12.2 NkMaterial

Un `NkMaterial` empaquette un `NkShader` + ses uniformes préconfigurés + des états GPU
(blend, texture). Idéal pour des effets réutilisables (toon, feu, eau, lumière 2D…) :

```cpp
#include "NKCanvas/Renderer/Resources/NkMaterial.h"

NkMaterial fire;
fire.SetShader(&fireShader);
fire.SetBlendMode(NkBlendMode::NK_ADD);
fire.SetVec3("uColorHot", 1.f, 0.7f, 0.2f);
fire.SetFloat("uIntensity", 1.f);

// Dans la boucle : mets à jour ce qui change, puis dessine avec States().
fire.SetFloat("uTime", time);
target.Draw(flameSprite, fire.States());   // States() applique tous les uniformes
```

> **État (mai 2026)** : `NkShader::Compile()` fonctionne en **OpenGL**. Sur les autres
> backends, c'est un stub (`Compile()` renvoie `false`) — l'architecture est en place mais
> l'intégration pipeline reste à faire par backend. Pour du rendu portable garanti, reste
> sur le pipeline par défaut (shader = `nullptr`).

---

## 13. Créer son propre drawable + clipping

### 13.1 Drawable personnalisé

Hérite de `NkDrawable` (et de `NkTransformable` si tu veux position/rotation/échelle) :

```cpp
#include "NKCanvas/Renderer/Core/NkDrawable.h"
#include "NKCanvas/Renderer/Core/NkTransformable.h"
#include "NKCanvas/Renderer/Targets/NkRenderTarget.h"

class MonGizmo : public NkTransformable, public NkDrawable {
public:
    void Draw(NkRenderTarget& target, const NkRenderStates& parent) const override {
        NkRenderStates s = parent;
        s.transform *= GetTransform();    // compose notre transform local au parent

        NkVertex tri[3] = {
            {  0.f,  0.f, 0,0, 255,   0,   0, 255 },
            { 50.f,  0.f, 0,0,   0, 255,   0, 255 },
            { 25.f, 50.f, 0,0,   0,   0, 255, 255 },
        };
        target.Draw(tri, 3, NkPrimitiveType::NK_TRIANGLES, s);
    }
};

MonGizmo g;
g.SetPosition({ 400.f, 300.f });
target.Draw(g);
```

### 13.2 Clipping / scissor

NKCanvas dispose d'un **scissor par pile** (modèle « push/pop clip rect », façon ImGui),
géré dans le batcher et appliqué par chaque backend. C'est utilisé notamment par
[NKUI](07-NKUI.md) pour limiter le dessin à une zone (panneaux scrollables, listes). Pour de
l'usage 2D direct, le plus simple reste de cadrer ta scène via le **viewport**
(`SetViewport`) ou la **view** (section 10). Le clipping fin est surtout exposé au travers de
NKUI.

---

## 14. Pièges connus (à lire avant de débugger)

1. **Ordre Clear → Draw → Display.** Tout `Draw` doit être entre `Clear` et `Display`. Rien
   ne s'affiche si tu oublies `Display()`, ou si tu dessines après.

2. **Resize / OnResize.** Quand la fenêtre change de taille, il faut recréer la swapchain.
   Deux approches :
   - **Sur événement** : abonne-toi à `NkWindowResizeEvent` et appelle
     `target.OnResize(width, height)` (pixels **physiques**), idem `OnDpiChange()` sur
     `NkWindowDpiEvent`.
   - **Par polling** (pattern Mú, robuste) : compare `target.GetSize()` à la taille
     mémorisée et appelle `OnResize` seulement si elle a **réellement** changé :

   ```cpp
   const math::NkVec2u sz = mRenderTarget->GetSize();
   if (sz.x != mLastW || sz.y != mLastH) {
       if (mLastW != 0 && sz.x > 0 && sz.y > 0)
           mRenderTarget->OnResize(sz.x, sz.y);
       mLastW = sz.x; mLastH = sz.y;
   }
   ```

   > **Pourquoi ce garde-fou ?** Windows émet un `WM_SIZE` *à la création* de la fenêtre. Un
   > handler naïf déclenche alors un `OnResize` inutile **avant** la première frame, ce qui
   > peut crasher DX12 ou réinitialiser la DIB en Software. Ne déclenche `OnResize` que si la
   > taille a vraiment bougé.

3. **Retour d'arrière-plan (Android).** Quand l'app revient au premier plan, la surface
   native a été recréée → appelle `target.RecreateSurface()` (sur `NkWindowShownEvent`),
   sinon écran noir. Mets aussi le rendu en pause tant que l'app est en arrière-plan.

4. **Unités d'angle.** `NkShape` (formes) → **radians** ; `NkSprite` / `NkText` →
   **degrés**. Ne les confonds pas.

5. **Durée de vie des ressources.** `NkTexture` et `NkFont` sont **non-copiables** ; sprites
   et textes gardent un **pointeur** vers elles. La ressource doit survivre à ses
   consommateurs.

6. **Maturité des backends.** OpenGL est le plus solide partout. Vulkan/DX11/DX12/Software
   marchent mais peuvent demander du polissage selon la machine. En cas de doute pour une
   démo portable : `NK_GFX_API_OPENGL`, ou `NK_GFX_API_SOFTWARE` comme filet de sécurité.

7. **`NkRenderTexture` et `NkShader::Compile`** : réels en OpenGL, **stubs** ailleurs
   (`Create()` / `Compile()` renvoient `false`). Vérifie toujours la valeur de retour.

---

## 15. Récapitulatif

```cpp
// 1. Fenêtre + cible de rendu
NkWindow window;  window.Create(cfg);
NkContextDesc desc;  desc.api = NkGraphicsApi::NK_GFX_API_OPENGL;
NkRenderWindow target(window, desc);

// 2. Ressources
NkTexture tex;   tex.LoadFromFile(*target.GetRenderer(), "hero.png");
NkSprite  hero(tex);   hero.SetPosition({ 100, 100 });
NkRectangleShape box({ 80, 80 });  box.SetFillColor(NkColor2D::Orange);

// 3. Boucle
while (window.IsOpen()) {
    while (NkEvent* ev = NkEvents().PollEvent()) { /* ... */ }
    target.Clear({ 30, 30, 30, 255 });
    target.Draw(box);
    target.Draw(hero);
    target.Display();
}
```

Tu sais maintenant : créer une fenêtre de rendu multi-backend, faire tourner la boucle
Clear/Draw/Display, dessiner formes / sprites / texte, les transformer, descendre aux
vertex arrays et render states, faire du render-to-texture et survoler shaders/matériaux.

### Dépendances Jenga

Pour un projet 2D typique, déclare au minimum :

```python
nkentseudependson(
    ["NKCanvas", "NKWindow", "NKEvent", "NKImage", "NKFont",
     "NKMemory", "NKCore", "NKMath", "NKLogger"],
    extra_includes=["src"],
)
```

- **NKCanvas** : ce module (rendu 2D). Tire transitivement le backend GPU choisi.
- **NKWindow** : la fenêtre native (cible du `NkRenderWindow`).
- **NKEvent** : la boucle d'événements (resize, souris, clavier — voir [guide 3](03-NKEvent.md)).
- **NKImage** : décodage des textures (voir [guide 4](04-NKImage.md)).
- **NKFont** : rasterisation des polices pour `NkText`/`NkFont`.

Liens système par plateforme à connaître :

- **Windows** : DX11/DX12 sont liés par défaut sur la toolchain clang-mingw ucrt64
  (`d3d11`, `d3d12`, `dxgi`, `dxguid`) — rien à ajouter côté app.
- **Vulkan** : nécessite le Vulkan SDK (`VULKAN_SDK`). Le backend est activable/désactivable
  via `NKENTSEU_ENABLE_VULKAN_BACKEND`.
- **OpenGL** : le loader GLAD2 est compilé s'il est présent (`__has_include`), sinon
  `NK_NO_GLAD2` et un loader externe peut être utilisé.
- **Android / Web** : OpenGL ES / WebGL (et Software) ; pas de DX.

---

← [NKImage](04-NKImage.md) · [Retour au sommaire](README.md) · [NKAudio](06-NKAudio.md) → · puis [NKUI](07-NKUI.md)
```
