# NkCanvas & NKGui — notes de référence exhaustives

> Document de travail rédigé en lecture seule sur le dépôt
> `D:\Projets\2026\Nkentseu\Nkentseu` (moteur C++17 « Nkentseu », commentaires en
> français, **règle absolue : zéro STL** — on n'utilise que `NkString`, `NkVector`,
> `NkFileSystem`, `math::Nk*`, et la mémoire passe par **NKMemory**, jamais
> `new`/`delete`).
>
> **Destination** : servir de matière première à un cours pour débutants. Tout est
> sourcé (`chemin:ligne`) et accompagné d'extraits réels. Aucune source n'a été
> modifiée, rien n'a été compilé ni exécuté.
>
> Deux briques distinctes, souvent confondues :
> - **NKCanvas** = le moteur de rendu **2D** (dessine des triangles à l'écran, style SFML).
> - **NKGui** = le framework **d'interface** (boutons, fenêtres, docking) qui *produit*
>   des listes de commandes, et laisse un « backend » les faire dessiner — souvent par NKCanvas.
>
> NKGui **ne dépend pas** de NKCanvas : c'est le pont `NkGuiCanvasBackend` (qui vit
> dans NKCanvas) qui les marie.

---

## Table des matières

1. [Où vivent les deux modules / organisation / build](#1-où-vivent-les-deux-modules)
2. [NKCanvas — le modèle de dessin](#2-nkcanvas--le-modèle-de-dessin)
3. [NKGui — le modèle de widgets](#3-nkgui--le-modèle-de-widgets)
4. [Catalogue exhaustif des widgets NKGui](#4-catalogue-exhaustif-des-widgets-nkgui)
5. [Raccordement au reste du moteur + squelette minimal d'application](#5-raccordement-au-reste-du-moteur)
6. [Pièges et invariants — citations du dépôt](#6-pièges-et-invariants)
7. [Ce qui est implémenté vs déclaré/vide](#7-état-réel--implémenté-vs-déclaré)

---

## 1. Où vivent les deux modules

### 1.1 Emplacements

| Brique | Chemin | Nature |
|---|---|---|
| **NKCanvas** | `D:\Projets\2026\Nkentseu\Nkentseu\Kernel\Runtime\NKCanvas` | bibliothèque (≈ 23 400 lignes, 101 fichiers) |
| **NKGui** | `D:\Projets\2026\Nkentseu\Nkentseu\Kernel\Runtime\NKGui` | bibliothèque (≈ 7 780 lignes, 13 fichiers source) |
| Pont NKGui → NKCanvas | `Kernel\Runtime\NKCanvas\src\NKCanvas\UI\NkGuiCanvasBackend.h` | **header-only** |
| Pont NKGui → NKRHI (3D) | `Integrations\NKGui\NkGuiRHIBackend.h/.cpp` | staticlib `NKGuiIntegration` |
| Démo de référence | `Applications\NKGuiDemo\src\NKGuiDemo\main.cpp` | ≈ 1 067 lignes, prouve le pipeline complet |

### 1.2 Arborescence NKGui (petite et lisible — c'est par là qu'on commence)

```
Kernel/Runtime/NKGui/
  NKGui.jenga · ARCHITECTURE.md · README.md · ROADMAP.md
  src/NKGui/
    NKGui.h                     ← en-tête PARAPLUIE (la seule chose à inclure)
    NkGuiExport.h → NkGuiApi.h  ← macros d'export (défaut : statique, aucune décoration)
    Core/
      NkGuiTypes.h      (333 l.) types de base : NkGuiId, enums, thèmes de flags
      NkGuiInput.h      (145 l.) état d'entrée par frame (header-only, tout inline)
      NkGuiDrawList.h/.cpp (101/342 l.) la LISTE DE COMMANDES de dessin
      NkGuiFont.h/.cpp  ( 81/158 l.) police + atlas (wrapper NKFont)
      NkGuiContext.h/.cpp (551/573 l.) LE contexte : état complet de l'UI
    Widgets/
      NkGuiWidgets.h/.cpp (405/4973 l.) TOUS les widgets
```

`src/NKGui/NKGui.h:29-35` — l'ordre d'inclusion est l'ordre des dépendances :

```cpp
#include "NKGui/NkGuiExport.h"
#include "NKGui/Core/NkGuiTypes.h"
#include "NKGui/Core/NkGuiInput.h"
#include "NKGui/Core/NkGuiDrawList.h"
#include "NKGui/Core/NkGuiFont.h"
#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Widgets/NkGuiWidgets.h"
```

**Usage côté application** — `NKGui.h:11-12` :

```cpp
//   #include "NKGui/NKGui.h"
//   using namespace nkentseu::nkgui;
```

**Piège de plateforme documenté dès la ligne 18** de `NKGui.h` (à connaître,
c'est typique du genre de bug qui fait perdre une journée) :

> ```
> // Retire les macros X11 (None, Bool, Status...) AVANT toute declaration de
> // NKGui. Sur Linux, NKWindow tire <X11/Xlib.h>, dont le `#define None 0L`
> // transforme ensuite `None = 0` — membre de plusieurs enumerations de NkGuiTypes
> // — en `0L = 0`. Le compilateur signale alors « expected identifier » sur des
> // lignes parfaitement correctes, tres loin de la vraie cause.
> ```
> (`Kernel\Runtime\NKGui\src\NKGui\NKGui.h:18-26`, d'où le `#include "NKPlatform/NkX11Clean.h"` en ligne 27)

### 1.3 Arborescence NKCanvas

```
Kernel/Runtime/NKCanvas/
  NKCanvas.jenga · ROADMAP.md · USAGE.md
  src/NKCanvas/
    Core/       NkIGraphicsContext.h, NkContextDesc.h, NkGraphicsApi.h, NkGpuPolicy…
    Factory/    NkContextFactory (choisit le backend GPU)
    Backend/    OpenGL/ · Vulkan/ · DirectX/ (DX11+DX12) · Metal/ · Software/
    Compute/    NkIComputeContext (GPGPU — hors sujet UI)
    Renderer/
      Core/       NkIRenderer2D (interface), NkRenderer2D (façade), NkRenderStates,
                  NkDrawable, NkTransform/NkTransformable, NkVertexArray, NkRenderer2DFactory
      Batch/      NkBatchRenderer2D  ← LE batcher CPU, base commune aux 5 backends
      Resources/  NkTexture, NkFont, NkSprite/NkText, NkShader, NkMaterial
      Shapes/     NkShape, NkRectangleShape, NkCircleShape, NkLineShape, NkConvexShape
      Targets/    NkRenderTarget, NkRenderWindow, NkRenderTexture
    UI/         NkGuiCanvasBackend.h (NKGui) · NkUICanvasBackend.h/.cpp (ancien NKUI)
```

### 1.4 Dépendances déclarées (fichiers `.jenga` — le système de build maison, en Python)

**NKGui** — `Kernel\Runtime\NKGui\NKGui.jenga:22-27` :

```python
nkentseudependson(
    ["NKPlatform", "NKCore", "NKMemory", "NKMath", "NKThreading",
     "NKLogger", "NKContainers", "NKEvent", "NKFont", "NKImage"],
    selfexport="NKGui",
    extra_includes=["src"],
)
files(["src/**.cpp"])
```

→ **NKGui ne dépend NI de NKCanvas NI de NKRHI.** C'est volontaire : le cœur est
« render-agnostique ». Il ne connaît que les maths, les conteneurs, les polices,
les images et les événements.

**NKCanvas** — `Kernel\Runtime\NKCanvas\NKCanvas.jenga:65` :

```python
_canvasDeps = ["NKWindow", "NKFont", "NKImage", "NKStream", "NKTime", "NKGlad", "NKThreading"]
```
(+ `NKUI` optionnelle sous `USE_CANVAS_NKUI`, lignes 85-87 ; sur Windows desktop,
`d3d11/d3d12/dxgi/dxguid` sont liés en dur, lignes 118-127 ; Vulkan est
conditionnel à `VULKAN_SDK`/`NK_ENABLE_VULKAN`, lignes 40-55 ; sur iOS, OpenGL et
Vulkan sont exclus du build — Metal uniquement, lignes 203-206).

**NKGuiIntegration** (pont vers NKRHI) — `Integrations\NKGui\NKGuiIntegration.jenga:39-42` :

```python
_DEPS = [
    "NKGui", "NKRHI", "NKSL", "NKEvent", "NKWindow",
    "NKImage", "NKFont",
    "NKMemory", "NKCore", "NKMath", "NKContainers",
```

Doctrine de répartition, citée telle quelle (`NKGuiIntegration.jenga:5-8`) :

> ```
> Rend une UI NKGui (nkgui::NkGuiDrawList) via NKRHI/NKRenderer, sans NKCanvas.
> Sert aux applications 2D/3D (app d'animation, moteur de jeu). NKCanvas reste
> le backend de l'IDE (NKCode).
> ```

### 1.5 Macros d'export (bon à savoir pour lire les signatures)

`Kernel\Runtime\NKGui\src\NKGui\NkGuiApi.h:32-40` — par défaut le module est
**statique**, donc `NKENTSEU_NKGUI_API` est **vide**. Quand on lit
`NKENTSEU_NKGUI_API void Text(...)`, il faut mentalement lire `void Text(...)`.
Autres formes : `NKENTSEU_NKGUI_CLASS_EXPORT` (classe entière),
`NKENTSEU_NKGUI_API_INLINE` (fonction inline exportée).

---

## 2. NKCanvas — le modèle de dessin

### 2.1 Les deux niveaux d'API

1. **`NkIRenderer2D`** — l'interface bas niveau, une implémentation par API graphique.
   `Kernel\Runtime\NKCanvas\src\NKCanvas\Renderer\Core\NkIRenderer2D.h`
2. **`NkRenderer2D`** — la façade concrète, celle qu'on manipule dans du code
   applicatif. Tout est forwardé *inline* vers l'interface.
   `…\Renderer\Core\NkRenderer2D.h`

**Propriété / durée de vie** — piège n°1, cité mot pour mot
(`…\Renderer\Core\NkRenderer2D.h:31-35`) :

> ```
> /// PROPRIETE
> ///   NkRenderer2D ne POSSEDE pas son backend NkIRenderer2D* — le NkRenderTarget
> ///   qui le contient est proprietaire (typiquement via memory::NkUniquePtr).
> ///   Le NkRenderer2D est juste une projection. La duree de vie suit celle du
> ///   NkRenderTarget englobant.
> ```
> avec, en clair, `NkIRenderer2D *mBackend{nullptr}; ///< non-owning` (ligne 314)
> et `NkRenderTarget *mTarget{nullptr}; ///< non-owning` (ligne 315).

### 2.2 Primitives disponibles — signatures exactes

Toutes déclarées dans `…\Renderer\Core\NkIRenderer2D.h`, toutes **implémentées**
dans `NkBatchRenderer2D` (la base commune aux 5 backends actifs) :

| Signature | Ligne | État |
|---|---|---|
| `virtual void Draw(const NkSprite &sprite) = 0;` | 135 | implémenté |
| `virtual void Draw(const NkText &text) = 0;` | 138 | implémenté (délègue à `NkText::Draw`) |
| `virtual void DrawPoint(NkVec2f pos, const NkColor2D &color=White, float32 size=1.f) = 0;` | 141 | implémenté (quad `size×size`) |
| `virtual void DrawLine(NkVec2f a, NkVec2f b, const NkColor2D &color=White, float32 thickness=1.f) = 0;` | 143-144 | implémenté (quad orienté) |
| `virtual void DrawRect(NkRect2f rect, const NkColor2D &color=White, float32 outline=0.f, const NkColor2D &outlineColor=Black) = 0;` | 146-147 | implémenté |
| `virtual void DrawFilledRect(NkRect2f rect, const NkColor2D &color=White) = 0;` | 149 | implémenté |
| `virtual void DrawCircle(NkVec2f center, float32 radius, const NkColor2D &color=White, uint32 segments=32, float32 outline=0.f, const NkColor2D &outlineColor=Black) = 0;` | 151-153 | implémenté |
| `virtual void DrawFilledCircle(NkVec2f center, float32 radius, const NkColor2D &color=White, uint32 segments=32) = 0;` | 155-156 | implémenté (triangulation en éventail) |
| `virtual void DrawTriangle(NkVec2f a, NkVec2f b, NkVec2f c, …, float32 outline=0.f, …) = 0;` | 158-159 | implémenté |
| `virtual void DrawFilledTriangle(NkVec2f a, NkVec2f b, NkVec2f c, const NkColor2D &color=White) = 0;` | 161-162 | implémenté |
| `virtual void DrawVertices(const NkVertex2D *vertices, uint32 vertexCount, const uint32 *indices, uint32 indexCount, const NkTexture *texture=nullptr) = 0;` | 192-193 | implémenté — **le point d'entrée bas niveau**, c'est celui qu'utilise le pont NKGui |

**Primitives composites** — implémentation *par défaut* directement dans
l'en-tête (non virtuelles pures : aucun backend à toucher) :

- `DrawRectOutline(NkRect2f, const NkColor2D&, float32 thickness=1.f)` — `NkIRenderer2D.h:167-175`, composée de 4 `DrawFilledRect`.
- `DrawCircleOutline(NkVec2f, float32 radius, const NkColor2D&, float32 thickness=1.f, uint32 segments=32)` — `NkIRenderer2D.h:178-189`, anneau en segments de `DrawLine`.
- `DrawTexturedRect(NkRect2f, const NkTexture*, const NkColor2D &color=White, NkRect2f uv={0,0,1,1})` — `NkIRenderer2D.h:199-233`, construit un quad `NkVertex2D[4]` et appelle `DrawVertices`.
- Surcharge en pixels (façade seulement) : `NkRenderer2D::DrawTexturedRect(NkRect2f, const NkTexture*, NkRect2i sourcePixels, const NkColor2D&)` — `NkRenderer2D.h:259-264`.

**Ce qui N'EXISTE PAS au niveau NKCanvas — à savoir pour ne pas le chercher :**

- **Aucun `DrawText(...)` ni `DrawImage(...)` direct.** Le texte et les images
  passent obligatoirement par les *drawables* `NkText` / `NkSprite`
  (`Draw(const NkText&)` / `Draw(const NkSprite&)`), ou par le pattern SFML
  `target.Draw(drawable, states)`.
- **Aucune primitive de dégradé** (`DrawGradient*`). Le seul moyen d'obtenir un
  dégradé est de fournir des couleurs différentes par sommet via `DrawVertices` /
  `NkVertexArray` (interpolation GPU classique). *(C'est exactement ce que fait
  NKGui avec `AddRectFilledMultiColor` / `AddTriangleMultiColor` — voir §3.4.)*
- **Aucun rectangle arrondi natif.** Là aussi, NKGui le fabrique lui-même en
  triangulant des arcs de coin (§3.4).

### 2.3 Formes persistantes (« Shapes », style SFML)

Toutes héritent de `NkShape : public NkTransformable, public NkDrawable`
(`…\Renderer\Shapes\NkShape.h:40`), qui factorise le remplissage (triangulation en
éventail, `mFillColor`), le contour (`NK_LINE_STRIP`, `mOutlineColor` /
`mOutlineThickness`) et une texture optionnelle.

| Classe | Fichier:ligne | API propre |
|---|---|---|
| `NkRectangleShape` | `NkRectangleShape.h:22` | `explicit NkRectangleShape(NkVec2f size)`, `SetSize/GetSize` |
| `NkCircleShape` | `NkCircleShape.h:21` | `explicit NkCircleShape(float32 radius, uint32 segments=32)`, `SetPointCount` |
| `NkLineShape` | `NkLineShape.h:30` | `NkLineShape(NkVec2f a, NkVec2f b, float32 thickness=1.f)` |
| `NkConvexShape` | `NkConvexShape.h:32` | `SetPointCount(n)`, `SetPoint(i, p)` |

Limite documentée : la triangulation en éventail n'est valable que pour des
polygones **convexes** (`NkShape.h:21-26`, `NkConvexShape.h:19-23`).

### 2.4 `NkVertexArray` et les types de primitives

`…\Renderer\Core\NkVertexArray.h:34` — conteneur `NkVector<NkVertex>` +
`NkPrimitiveType`, avec `Append/Resize/Reserve/GetBounds`.

`…\Renderer\Core\NkRenderer2DTypes.h:63-70` :

```cpp
enum class NkPrimitiveType : uint8 {
    NK_POINTS = 0, NK_LINES = 1, NK_LINE_STRIP = 2,
    NK_TRIANGLES = 3, NK_TRIANGLE_STRIP = 4, NK_TRIANGLE_FAN = 5,
};
```

Commentaire explicatif juste au-dessus (lignes 60-62) — **bonne question de cours** :

> *« Pas de QUADS — les quads sont decomposes en 2 triangles par le batcher
> (compatible cross-API : Vulkan/DX/Metal n'ont pas de QUADS natif) »*

Le vertex du rendu 2D — `NkRenderer2DTypes.h:73-77` :

```cpp
struct NkVertex2D { float32 x, y; float32 u, v; uint8 r, g, b, a; };
```

### 2.5 Système de coordonnées

- **Origine haut-gauche, axe Y vers le bas** (« Y-down »). Confirmé dans
  `…\Renderer\Batch\NkBatchRenderer2D.cpp:183` : *« Vue par defaut = ecran
  plein-cadre (origine haut-gauche, Y-down) »*.
- **Caméra 2D** : `NkView2D` (`NkRenderer2DTypes.h:41-47`) — `center`, `size`, `rotation`.
  Projection orthographique construite dans
  `…\Renderer\Core\NkRenderer2DTypes.cpp:85` :

  ```cpp
  const float32 a =  2.f / size.x;
  const float32 b = -2.f / size.y; // Y-down (positive Y goes down in screen space)
  ```

  avec, lignes 81-84, la note sur la convention mémoire (piège classique multi-API) :

  > *« Column-major output (compatible with glUniformMatrix4fv with transpose=GL_FALSE,
  > and with HLSL cbuffer when uploaded as-is since HLSL is row-major — we compensate
  > by transposing at upload in the DX backends) »*

- **Viewport** : `SetViewport(NkRect2i)` / `GetViewport()` — `NkIRenderer2D.h:81-82`.
- **Redimensionnement** : `OnResize` recale la vue par DÉFAUT mais **préserve une vue
  custom** posée par `SetView` (`NkBatchRenderer2D.cpp:169-196`).
- **Conversion pixel ↔ monde** : `MapPixelToCoords(NkVec2i)` / `MapCoordsToPixel(NkVec2f)`
  (`NkBatchRenderer2D.cpp:538-550`).
- **Cycle de frame** : `Begin()` / `End()` / `Flush()` (`NkIRenderer2D.h:59-70`).
  Mais dans la pratique **on n'y touche pas** : `NkRenderWindow::Clear()` ouvre
  implicitement la frame (`NkRenderWindow.cpp:157-160`) et `Display()` la ferme
  puis présente (`NkRenderWindow.cpp:164-171`).

### 2.6 Découpage (clip / scissor)

API sur `NkIRenderer2D` (les défauts sont des *no-op*, l'implémentation réelle est
dans `NkBatchRenderer2D`) :

```cpp
virtual void SetClip(const NkRect2i &rectPixels) { (void)rectPixels; }   // NkIRenderer2D.h:105-107
virtual void PopClip() {}                                                // :109-110
virtual void ResetClip() {}                                              // :112-113
virtual bool HasClip() const { return false; }                           // :115-117
virtual NkRect2i GetClip() const { return NkRect2i{}; }                  // :119-121
```

Contrat, cité tel quel (`NkIRenderer2D.h:98-104`) :

> *« Pile : SetClip empile le rect (intersecte avec le clip courant) ; PopClip
> depile. ResetClip vide la pile. Implementation backend : glScissor (GL),
> VkRect2D dynamique (Vulkan), RSSetScissorRects (DX11/12), clamp CPU (Software). »*

Implémentation — pile `NkVector<NkRect2i> mClipStack` (`NkBatchRenderer2D.h:169`),
intersection stricte (`NkIntersectClip`, `NkBatchRenderer2D.cpp:103-117`), et
**chaque changement de clip force un `Flush()`** (`NkBatchRenderer2D.cpp:120-126`) :

```cpp
void NkBatchRenderer2D::SetClip(const NkRect2i &rect) {
    Flush(); // committe la geometrie en cours avec le clip actuel
    const NkRect2i clip = mHasClip ? NkIntersectClip(mClipRect, rect) : rect;
    mClipStack.PushBack(clip);
    mClipRect = clip;
    mHasClip = true;
}
```

Côté OpenGL, le scissor inverse Y (origine GL en bas-gauche) —
`…\Backend\OpenGL\NkOpenGLRenderer2D.cpp:532-546` :

```cpp
const int32 y = mViewport.height - rect.y - h; // flip Y
glEnable(GL_SCISSOR_TEST);
glScissor((GLint)x, (GLint)y, (GLsizei)w, (GLsizei)h);
```

Et à chaque `Begin()` le scissor GL est explicitement désactivé — commentaire à
retenir (`NkOpenGLRenderer2D.cpp:524-526`) :

> *« Chaque frame demarre sans scissor (sinon un Clear serait clippe par le
> scissor laisse par la frame precedente, l'etat GL etant persistant) »*

### 2.7 Couleurs

`…\Renderer\Core\NkRenderer2DTypes.h:12` :

```cpp
using NkColor2D = math::NkColor;
```

`math::NkColor` (`Kernel\Foundation\NKMath\src\NKMath\NkColor.h:1046`) est une
classe **RGBA 8 bits compacte** (`uint8 r,g,b,a`, 4 octets, lignes 1058-1061). Un
pendant flottant `NkColorF` (`[0,1]`, 16 octets) porte tout l'arsenal
colorimétrique : HSL/HSV, LAB/LCH, XYZ, OKLab/OKLch, CMYK, hex, luminance WCAG,
DeltaE/DeltaE2000, modes de fusion Multiply/Screen/Overlay/SoftLight/HardLight
(`NkColor.h:180-998`). Le rendu 2D, lui, n'utilise que les 4 `uint8`.

### 2.8 Polices et texte

`NkFont` (NKCanvas) est un **wrapper GPU** au-dessus du module externe **NKFont**
(rasterisation FreeType déportée hors NKCanvas depuis 2026-05-28, cf.
`…\Renderer\Resources\NkFont.h:56-63`).

- **Chargement** : `NkFont::LoadFromFile(NkIRenderer2D &renderer, const char *path)`
  / `LoadFromMemory(...)` — `NkFont.h:79-80`. L'implémentation lit les octets via
  `NkFile::ReadAllBytes` et **conserve la copie en mémoire** (`mFontData`) —
  `NkFont.cpp:65-81`.
- **Atlas** : une **page** par taille de caractère demandée (`struct Page`,
  `NkFont.h:102-107`), créée paresseusement par `GetOrCreatePage`
  (`NkFont.cpp:86-128`) via `NkFontAtlas::AddFontFromMemory + Build()`, puis
  `GetTexDataAsRGBA32` → upload en `NkTexture` (`NkFont.cpp:116-124`).
- **Glyphes** : `const NkGlyph &GetGlyph(uint32 codepoint, uint32 characterSize, bool bold=false) const`
  (`NkFont.h:85`). `NkGlyph` = `textureRect` (pixels dans l'atlas), `bounds`
  (bearing + taille), `advance` (`NkFont.h:29-34`).
- **Kerning non exposé** : `GetKerning` retourne toujours `0.f`
  (`NkFont.cpp:163-167` — *« Le module NKFont n'expose pas le kerning par paire
  (integre dans l'advance des glyphes) »*).
- **Faux-gras non géré** : le paramètre `bold` est ignoré (`NkFont.cpp:133`,
  paramètre nommé `/*bold*/`) — *« bold ignoré (le module ne fait pas de faux-gras ;
  charger une fonte bold dediee si necessaire) »* (`NkFont.h:83-84`).
- **Rendu** : chaque glyphe devient un quad `GlyphVertex{ NkVertex2D v[4] }`
  (`NkSprite.cpp:213-228`), `NkText::Draw` transforme les sommets **côté CPU**
  (rotation/échelle/origine manuelles, `NkSprite.cpp:290-316`) puis soumet via
  `renderer.DrawVertices(tverts.Data(), ..., atlas)` (`NkSprite.cpp:318`) →
  **un seul draw call par texte**.
- **Non implémenté** : `FindCharacterPos` → `return 0; // TODO: binary search on glyph bounds`
  (`NkSprite.cpp:332-334`).

### 2.9 Textures / atlas

`…\Renderer\Resources\NkTexture.h:36` — ressource GPU non copiable/déplaçable.

- Chargement : `Create(renderer, w, h, fillColor)`, `LoadFromFile(renderer, path)`,
  `LoadFromImage(renderer, image, area)`, `LoadFromMemory(...)` (lignes 53-58).
- Mise à jour partielle : `Update(pixels/image, destX, destY)` (lignes 60-62).
- Handle : `void *mHandle` (natif : `VkImage`, `ID3D11Texture2D*`…) +
  `uint32 mGPUId` (nom OpenGL) — champs privés lignes 136-137, accesseurs
  `GetHandle()/GetGPUId()/SetHandle()/SetGPUId()` (103-117).
- Filtrage/wrap : `NkTextureFilter{NK_NEAREST, NK_LINEAR}`,
  `NkTextureWrap{NK_CLAMP, NK_REPEAT, NK_MIRROR_REPEAT}` (lignes 22-33),
  `SetFilter/SetWrap/GenerateMipmap` (68-70).
- **Texture blanche 1×1** : `static NkTexture *NkTexture::GetWhiteTexture(NkIRenderer2D&)`
  (`NkTexture.h:121`) — utilisée par toutes les primitives *non texturées*
  (`DrawLine`, `DrawFilledRect`, `DrawFilledCircle`, `DrawFilledTriangle`) pour
  passer par le **même shader/pipeline texturé** (`NkBatchRenderer2D.cpp:394, 401, 426, 479`).
  *Idée de cours : « tout est texturé, le uni est juste du blanc ».*
- **UV d'atlas** : `NkRect2f GetTexCoords(const NkRect2i &rect) const` (`NkTexture.h:98`)
  — normalise un sous-rectangle en pixels vers `[0,1]²`.
- **Pixels CPU dupliqués** (`mCPUPixels`, `NkTexture.h:143`) : nécessaires au
  rasterizer Software. Voir le piège §6.

### 2.10 Accumulation puis soumission (le batching) — le cœur du sujet

Fichier central : `…\Renderer\Batch\NkBatchRenderer2D.cpp` (552 lignes), classe de
base de **tous** les backends actifs (`NkBatchRenderer2D : public NkIRenderer2D`,
`NkBatchRenderer2D.h:31`).

**Accumulation** :

- `NkVector<NkVertex2D> mVertices; NkVector<uint32> mIndices; NkVector<NkBatchGroup> mGroups;`
  (`NkBatchRenderer2D.h:157-159`).
- `NkBatchGroup` = **un futur draw call** : `{ texture, blendMode, indexStart, indexCount }`
  (`NkBatchRenderer2D.h:23-28`).
- Capacité : `kMaxVertices = 262144`, `kMaxIndices = kMaxVertices * 6 / 4`
  (`NkBatchRenderer2D.h:39-40`). Le commentaire justifiant ce dimensionnement est
  une histoire de bug vécu, à citer en cours (`NkBatchRenderer2D.h:36-38`) :

  > *« 262144 (~5 Mo GPU) : une UI dense (IDE plein écran : arbre + onglets +
  > icônes) dépasse 65536 sommets ENTRE DEUX CLIPS — la draw-list NkGui arrive
  > alors en UNE commande plus grosse que l'ancien buffer, et l'upload écrivait
  > HORS du buffer GPU (crash memcpy). »*

- `EnsureGroup(tex, blend)` (`NkBatchRenderer2D.cpp:207-229`) ferme le groupe
  courant et en ouvre un nouveau **dès que la texture ou le mode de fusion change**
  (et incrémente `mStats.textureSwap`). → *Règle pédagogique : moins on change de
  texture, moins il y a de draw calls.*
- `PushQuad(...)` (`NkBatchRenderer2D.cpp:232-274`) déclenche un **flush
  automatique** si l'ajout déborderait la capacité (`cpp:235-237`) :

  ```cpp
  if (mVertices.Size() + 4 > kMaxVertices || mIndices.Size() + 6 > kMaxIndices) {
      Flush();
  }
  ```

**Soumission** — `NkBatchRenderer2D::Flush()` (`cpp:56-98`) :

1. ferme le dernier groupe, compacte les groupes vides ;
2. applique le scissor courant : `ApplyScissor(mHasClip, mClipRect);` (ligne 79) ;
3. **garde-fou anti-corruption mémoire** (`cpp:80-88`) :

   ```cpp
   // GARDE-FOU : ne JAMAIS soumettre plus que la capacité des buffers GPU
   // (l'upload écrirait hors buffer -> crash). Troncature (indices en
   // multiple de 3) : dégradation visuelle plutôt que corruption mémoire.
   uint32 vSub = mVertices.Size(), iSub = mIndices.Size();
   if (vSub > kMaxVertices) vSub = kMaxVertices;
   if (iSub > kMaxIndices)  iSub = kMaxIndices - (kMaxIndices % 3);
   SubmitBatches(mGroups.Data(), validCount, mVertices.Data(), vSub, mIndices.Data(), iSub);
   ```

4. `SubmitBatches` (virtuelle pure, `NkBatchRenderer2D.h:141-142`) est **l'appel
   qui déclenche le vrai draw GPU**.

**Le vrai draw call, exemple OpenGL** — `…\Backend\OpenGL\NkOpenGLRenderer2D.cpp:591-613` :

```cpp
void NkOpenGLRenderer2D::SubmitBatches(const NkBatchGroup *groups, uint32 groupCount, const NkVertex2D *verts,
                                       uint32 vCount, const uint32 *idx, uint32 iCount) {
    glBindVertexArray((GLuint)mVAO);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)mVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(vCount * sizeof(NkVertex2D)), verts);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)mEBO);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, (GLsizeiptr)(iCount * sizeof(uint32)), idx);
    // Issue one draw call per group
    for (uint32 i = 0; i < groupCount; ++i) {
        const auto &g = groups[i];
        ApplyBlendMode(g.blendMode);
        BindTexture(g.texture);
        glDrawElements(GL_TRIANGLES, (GLsizei)g.indexCount, GL_UNSIGNED_INT,
                       (void *)(uintptr_t)(g.indexStart * sizeof(uint32)));
    }
}
```

→ **1 `glDrawElements` par groupe**, tout le batch uploadé en **un seul**
`glBufferSubData` par flush. `UploadProjection` (ligne 616-621) pousse la matrice
orthographique via `glUniformMatrix4fv`.

`Begin()` (`NkBatchRenderer2D.cpp:27-44`) vide les accumulateurs, réinitialise la
pile de clip, appelle `BeginBackend()`. `End()` (`cpp:47-53`) appelle `Flush()`
puis `EndBackend()`.

**Dispatch des primitives non-triangles** : `NkRenderWindow::Draw(const NkVertex*,
count, primitive, states)` (`…\Renderer\Targets\NkRenderWindow.cpp:208-325`)
applique la transformation CPU puis route selon `NkPrimitiveType` :
`NK_TRIANGLES` → indices identité + `DrawVertices` ; `NK_POINTS`/`NK_LINES`/
`NK_LINE_STRIP` → boucle de `DrawPoint`/`DrawLine` ; `NK_TRIANGLE_STRIP`/
`NK_TRIANGLE_FAN` → expansion manuelle en liste de triangles (lignes 272-320).

### 2.11 Backends existants et leur choix

| Backend | Taille du `.cpp` | État |
|---|---|---|
| **Vulkan** | 1524 (`NkVulkanRenderer2D.cpp`) | le plus volumineux ; SPIR-V embarqué (`NkRenderer2DVkSpv.inl`) |
| **DX12** | 1320 | root signature + tableau de textures bindless |
| **OpenGL** | 926 | VBO/VAO/EBO simple, shader unique |
| **DX11** | 764 | InputLayout + VS/PS compilés au runtime |
| **Software** | 674 | rasterizer CPU scanline + SIMD SSE2/NEON |
| **Metal** | *(absent)* | **pas de Renderer2D** — seulement contexte + compute (`.mm`) |

**Choix du renderer** — `…\Renderer\Core\NkRenderer2DFactory.cpp:32-105` : un
`switch` sur `ctx->GetApi()`. Pour Metal (lignes 86-89) :

```cpp
case NkGraphicsApi::NK_GFX_API_METAL:
    NK_R2D_FACTORY_ERR("Metal 2D renderer not yet implemented");
    return nullptr;
```
et `IsApiSupported()` confirme : `case NkGraphicsApi::NK_GFX_API_METAL: return false; // not yet implemented` (137-139).

**Choix du contexte GPU** — `…\Factory\NkContextFactory.cpp:45-107`
(`Create(window, desc)` selon `NkContextDesc::api`), ou automatique via
`CreateWithFallback(window, preferenceOrder, count)` (115-167). Ordre conseillé
documenté : `{DX12, DX11, Metal, Vulkan, OpenGL, Software}` (`NkContextFactory.h:46`).

**Nuance importante pour un cours** : la complétude *du code* place Vulkan/DX12 en
tête, mais la complétude *validée à l'exécution* est **OpenGL**. Selon
`Kernel\Runtime\NKCanvas\ROADMAP.md:185-200` et `601-607`, au 2026-05-30 seul
OpenGL était validé runtime sur la démo Pong ; Vulkan/DX12 crashaient,
DX11/Software affichaient un écran vide/noir.

Autres limites listées dans `Kernel\Runtime\NKCanvas\USAGE.md:482-488` (« Pièges
connus ») : `NkRenderTexture` est un **stub** sur tous les backends sauf OpenGL
(FBO réel) ; DX12/Vulkan n'ont pas de sampler par texture (sampler global
immuable → `SetFilter`/`SetWrap` sont des *no-op*) ; `NkShader::Compile()` ne
fonctionne **que** sur OpenGL, les autres retournent `false`.

---

## 3. NKGui — le modèle de widgets

### 3.1 Immédiat ou retenu ? — **immédiat aujourd'hui**, retenu prévu

La documentation annonce les deux (`Kernel\Runtime\NKGui\ARCHITECTURE.md:5-6`) :

> *« Deux paradigmes : **immédiat** (façon ImGui) **et retenu** (façon Qt/Unity). »*

Mais **le mode retenu n'existe pas dans le code** : `ARCHITECTURE.md:191` le place
en Phase 6, et l'arborescence réelle du module (§1.2) ne contient **ni** dossier
`Retained/`, **ni** `Layout/`, **ni** `Window/`, **ni** `Dock/`, **ni** `Backend/`
— tout ce qui est implémenté tient dans `Core/` + `Widgets/`. Le fenêtrage et le
docking, annoncés en dossiers séparés, sont en fait **dans `NkGuiWidgets.cpp`**
(lignes 2269-3410).

**Donc, pour le cours : NKGui est un GUI immédiat.** On redéclare tout, à chaque
image, du haut vers le bas :

```cpp
ctx.BeginFrame(dt);
    Text(ctx, "Bonjour");
    if (Button(ctx, "Cliquez-moi")) { /* réaction au clic */ }
ctx.EndFrame();
```

Il n'y a **pas d'objet bouton** qui vit entre deux images. Ce qui survit, c'est
uniquement l'**état** stocké dans `NkGuiContext` et retrouvé par **identifiant**.

Note contradictoire du dépôt à signaler : `README.md:14` et `ARCHITECTURE.md:185`
annoncent « Phase 1 — squelette », alors que `NkGuiWidgets.cpp` fait 4 973 lignes
et couvre tables, docking, color picker et menus imbriqués. **Les fichiers d'état
sont en retard sur le code.** Se fier au code.

### 3.2 L'identité d'un widget : `NkGuiId`

`Core\NkGuiTypes.h:26-46` :

```cpp
using NkGuiId = uint32;
static constexpr NkGuiId NKGUI_ID_NONE = 0;

NKENTSEU_NKGUI_API_INLINE NkGuiId NkGuiHashStr(const char *s, NkGuiId seed = 2166136261u) noexcept {
    NkGuiId h = seed;
    while (s && *s) { h ^= static_cast<uint8>(*s++); h *= 16777619u; }
    return h ? h : 1u;                                  // jamais 0 (0 = « aucun »)
}
NKENTSEU_NKGUI_API_INLINE NkGuiId NkGuiHashPtr(const void *p, NkGuiId seed = 2166136261u) noexcept { … }
```

C'est un **hash FNV-1a 32 bits** du libellé. C'est **la** clé de voûte du mode
immédiat : un widget se reconnaît d'une image à l'autre par son id, pas par son
adresse mémoire.

**Pile d'ID (scoping)** — pour éviter les collisions quand deux boutons ont le
même libellé dans deux panneaux différents (`Core\NkGuiContext.cpp:144-164`) :

```cpp
void NkGuiContext::PushId(const char *s) noexcept {
    const NkGuiId seed = idDepth > 0 ? idStack[idDepth - 1] : 2166136261u;
    if (idDepth < 32) idStack[idDepth++] = NkGuiHashStr(s, seed);
}
void NkGuiContext::PopId() noexcept { if (idDepth > 0) --idDepth; }
NkGuiId NkGuiContext::GetId(const char *s) const noexcept {
    const NkGuiId seed = idDepth > 0 ? idStack[idDepth - 1] : 2166136261u;
    return NkGuiHashStr(s, seed);
}
```

*(La graine de chaque niveau est l'id du niveau précédent → l'id final dépend du
chemin complet.)*

### 3.3 Le contexte `NkGuiContext` — où vit TOUT l'état

`Core\NkGuiContext.h:131` — c'est un `struct` volumineux (plus de 100 champs) qui
porte tout. Le contexte est **explicite et multi-instance**, mais un « contexte
courant » *thread-local* permet une API terse (`NkGuiContext.cpp:14-25`) :

```cpp
namespace {
    // Contexte courant per-thread (non-singleton, comme NkUIContext).
    thread_local NkGuiContext *gCurrentContext = nullptr;
}
void SetCurrentContext(NkGuiContext *ctx) noexcept { gCurrentContext = ctx; }
NkGuiContext *GetCurrentContext() noexcept { return gCurrentContext; }
```

Regroupement des champs par famille (avec lignes de `NkGuiContext.h`) :

| Famille | Champs clés | Lignes |
|---|---|---|
| Vue / échelle | `viewW`, `viewH`, `scale` (DPI) | 132-134 |
| Thème | `theme` (`NkGuiTheme`), `syntax` (`NkGuiSyntax`, coloration de code) | 135-136 |
| Entrée | `input` (`NkGuiInput`) | 137 |
| Dessin | `dl` (couche principale), `dlOverlay` (popups/overlay) | 138-139 |
| Mise en page | `layout` (`NkGuiLayout`) | 140 |
| Polices | `font`, `codeFont` (**pointeurs bruts, non possédés**) | 141-142 |
| Popups | `popupStack[8]`, `popupRects[8]`, `popupSaved[8]`, `popupDepth`, `curPopupLevel` | 147-152 |
| Occlusion | `occlRects/occlLayers/occlCount` (lecture) + `…New` (écriture), `curInputLayer` | 175-182 |
| Fenêtres | `winDL[32]`, `winMeta[32]`, `winZ[32]`, `winCount`, `curWindow`, `windowMeta` | 228-245 |
| Docking | `dockNodes`, `dockRoot`, `dockSpaceId`, `movingWindowId`, `dockTargetNode/Zone` | 248-259 |
| Presse-papiers | `clipboardUser`, `clipboardGetFn`, `clipboardSetFn` | 278-292 |
| Interaction | `hotId`, `hotIdPrev`, `activeId`, `interact`, `lastItemHovered` | 317-321 |
| Piles | `idStack[32]`, `disabledStack[16]`, `childStack[8]`, `containerSaved[32]` | 324-388 |
| Saisie | `inputId`, `inputCaret`, `inputAnchor`, `inputDrag`, `inputScroll`, `inputClickConsumed` | 333-338 |
| Renommage | `renameId`, `renameBackup[128]` | 342-343 |
| Champs numériques | `dragEditId`, `dragBuf[48]`, `dragLastX/Y` | 347-350 |
| Stockage persistant | `openNodes`, `tabBarKeys/tabBarSel`, `scrollKeys/scrollVals`, `tblKeys/tblWidths`, `pickerKeys/pickerHSV` | 358-425 |
| Hook de style | `styleFn`, `styleUser` | 429-431 |

**Thème par défaut** — `NkGuiContext.h:28-49` (bleu-gris sombre, façon éditeur) :

```cpp
struct NKENTSEU_NKGUI_CLASS_EXPORT NkGuiTheme {
        NkColor bgPrimary   = {26, 29, 36, 255};
        NkColor panel       = {38, 42, 52, 255};
        NkColor header      = {46, 51, 63, 255};
        NkColor button      = {58, 64, 80, 255};
        NkColor buttonHover = {78, 92, 120, 255};
        NkColor buttonActive= {96, 150, 230, 255};
        NkColor border      = {86, 94, 112, 255};
        NkColor text        = {230, 232, 240, 255};
        NkColor textDisabled= {120, 124, 134, 255};
        NkColor selection   = {64, 110, 200, 235};
        NkColor accent      = {96, 165, 250, 255};
        NkColor track       = {30, 34, 43, 255};
        NkColor tabBar      = {22, 25, 31, 255};
        NkColor tab         = {40, 45, 56, 255};
        NkColor tabHover    = {58, 64, 80, 255};
        NkColor tabActive   = {52, 58, 72, 255};
        float32 rounding  = 5.f;
        float32 framePadX = 10.f;
        float32 framePadY = 6.f;
};
```

Il n'y a **pas de « rôles » sémantiques abstraits** (pas de `Role::Primary` etc.) :
ce sont des couleurs nommées par usage, lues directement (`ctx.theme.button`).
Pour re-styler *totalement*, on n'écrase pas les couleurs, on branche le **hook de
dessin** (§3.9).

Il existe en plus un thème de **coloration syntaxique** partagé avec l'éditeur de
code (`NkGuiSyntax`, `NkGuiContext.h:53-66`) : `keyword`, `type`, `string`,
`comment`, `number`, `preproc`, `heading`, `mdcode`, `function`, `constant`, `oper`.

### 3.4 `NkGuiDrawList` — la liste de commandes

`Core\NkGuiDrawList.h`. C'est la **sortie** de NKGui : purement géométrique,
indépendante du backend.

**Le vertex et la commande** (`NkGuiDrawList.h:23-46`) :

```cpp
struct NkGuiVertex {
        NkVec2 pos;
        NkVec2 uv;   ///< (0,0) = couleur unie (pas de texture)
        uint32 col;
};

enum class NkGuiDrawCmdType : uint8 {
    Triangles,         ///< triangles unis
    TexturedTriangles  ///< triangles texturés (texte/atlas/image)
};

struct NkGuiDrawCmd {
        NkGuiDrawCmdType type = NkGuiDrawCmdType::Triangles;
        uint32 idxOffset = 0;
        uint32 idxCount  = 0;
        uint32 texId     = 0;
        NkRect clipRect  = {0.f, 0.f, 1.0e9f, 1.0e9f};   // 1e9 = « pas de clip »
};

NKENTSEU_NKGUI_API_INLINE uint32 NkGuiPackColor(const NkColor &c) noexcept {
    return (static_cast<uint32>(c.a) << 24) | (static_cast<uint32>(c.b) << 16) |
           (static_cast<uint32>(c.g) << 8)  |  static_cast<uint32>(c.r);
}
```

→ Couleur **empaquetée ABGR** (`a<<24 | b<<16 | g<<8 | r`) — le backend la
dépaquette dans le même ordre (voir §5.2).

**Le conteneur** (`NkGuiDrawList.h:48-98`) :

```cpp
struct NKENTSEU_NKGUI_CLASS_EXPORT NkGuiDrawList {
        NkVector<NkGuiVertex> vtx;
        NkVector<uint32>      idx;
        NkVector<NkGuiDrawCmd> cmds;
        NkRect  clipStack[32] = {};
        int32   clipDepth = 0;
        float32 thickScale = 1.f; ///< échelle DPI des épaisseurs (préservée par Reset)
        …
};
```

**Primitives disponibles** (toutes implémentées dans `NkGuiDrawList.cpp`) :

| Primitive | Déclaration | Implémentation |
|---|---|---|
| `AddRectFilled(const NkRect&, const NkColor&, float32 rounding = 0.f)` | `.h:66` | `.cpp:94-142` — **coins arrondis inclus** |
| `AddRect(const NkRect&, const NkColor&, float32 thickness = 1.f)` | `.h:67` | `.cpp:204-214` |
| `AddRectFilledMultiColor(r, tl, tr, br, bl)` | `.h:69-70` | `.cpp:171-184` — **dégradé bilinéaire** |
| `AddImage(uint32 texId, const NkRect&, uv0, uv1, tint)` | `.h:73-74` | `.cpp:155-169` |
| `AddLine(a, b, col, thickness = 1.f)` | `.h:75` | `.cpp:186-202` |
| `AddTriangleFilled(a, b, c, col)` | `.h:76` | `.cpp:216-224` |
| `AddTriangleMultiColor(a, b, c, ca, cb, cc)` | `.h:78-79` | `.cpp:144-153` — **dégradé barycentrique** |
| `AddCircleFilled(center, r, col, segs = 0)` | `.h:80` | `.cpp:318-339` — `segs=0` → auto (12…128) |
| `AddText(face, texId, baseline, text, col, maxWidth = -1.f, skew = 0.f)` | `.h:87-88` | `.cpp:237-284` |
| `AddTextRange(face, texId, baseline, begin, end, col)` | `.h:91-92` | `.cpp:286-316` |

**Il n'y a PAS** de `AddPolyline`, `AddBezier`, `AddArc`, `AddCircle` (contour non
plein), contrairement à ce qu'annonce `ARCHITECTURE.md:101-102`. Le contour de
rectangle (`AddRect`) est fabriqué avec quatre rectangles pleins.

**Le rectangle arrondi** (à montrer en cours : comment on fabrique un coin rond
sans primitive dédiée) — `NkGuiDrawList.cpp:114-141` : quatre arcs de coin (4
segments chacun) triangulés en **éventail depuis le centre** du rectangle.

**La bordure `AddRect`** — le commentaire explique un bug de scissor vécu
(`NkGuiDrawList.cpp:205-208`) :

> ```
> // Bordure = 4 rectangles pleins tracés STRICTEMENT à l'intérieur du rect.
> // (AddLine centrerait l'épaisseur sur l'arête → la moitié extérieure sort
> // du rect et est rognée par le clip — scissor exclusif à droite/bas → bord
> // droit invisible/aminci.) Ici chaque bord tient dans [r.x, r.x+r.w] etc.
> ```

**Le découpage** — pile de 32 rects, intersection avec le parent par défaut
(`NkGuiDrawList.cpp:34-55`) :

```cpp
NkRect NkGuiDrawList::CurrentClip() const noexcept {
    return clipDepth > 0 ? clipStack[clipDepth - 1] : NkRect{0.f, 0.f, 1.0e9f, 1.0e9f};
}
void NkGuiDrawList::PushClipRect(const NkRect &r, bool intersect) noexcept { … }
void NkGuiDrawList::PopClipRect() noexcept { if (clipDepth > 0) --clipDepth; }
```

**Découpage des commandes** — `CurCmd(texId)` (`NkGuiDrawList.cpp:57-75`) ouvre une
nouvelle `NkGuiDrawCmd` **dès que le `texId` OU le clip change** :

```cpp
if (need) {
    NkGuiDrawCmd c;
    c.texId = texId;
    c.clipRect = clip;
    c.idxOffset = static_cast<uint32>(idx.Size());
    c.idxCount = 0;
    c.type = texId ? NkGuiDrawCmdType::TexturedTriangles : NkGuiDrawCmdType::Triangles;
    cmds.PushBack(c);
}
```

**Fusion de listes** — `Append` (`NkGuiDrawList.cpp:18-32`) concatène en décalant
les indices ET les `idxOffset` : c'est ce qui permet de fusionner les draw-lists
de fenêtres triées par z-order (§3.7).

**Le rendu du texte et l'alignement pixel** — commentaire long et très instructif
(`NkGuiDrawList.cpp:226-235`) :

> ```
> // ── ALIGNEMENT D'UN GLYPHE SUR LA GRILLE DE PIXELS ──────────────────────
> // Le curseur de texte accumule des avances FRACTIONNAIRES. Sans arrondi,
> // seul le PREMIER glyphe d'une chaîne tombe sur un pixel entier ; tous les
> // suivants dérivent et échantillonnent l'atlas ENTRE deux texels, ce qui
> // rend le texte uniformément flou. Le défaut est d'autant plus marqué que le
> // corps est petit : l'erreur vaut un demi-texel CONSTANT, soit 4 % de la
> // hauteur d'un caractère à 13 px et 3,3 % à 15 px.
> ```

et la subtilité de la solution (`NkGuiDrawList.cpp:258-266`) :

> ```
> // On arrondit la POSITION du quad, jamais l'AVANCE : `x` continue
> // d'accumuler la valeur exacte, donc la largeur totale de la chaîne
> // est inchangée et MeasureWidth reste d'accord avec le rendu. […]
> // La LARGEUR est reportée telle quelle : arrondir les deux bords
> // séparément étirerait le glyphe d'un pixel et le rééchantillonnerait,
> // ce qu'on cherche précisément à éviter.
> ```

Le paramètre `skew` d'`AddText` fait de **l'italique factice** en décalant chaque
sommet selon sa hauteur au-dessus de la ligne de base (`.cpp:271-278`).

### 3.5 `NkGuiInput` — l'entrée par frame

`Core\NkGuiInput.h:30-142`. Structure **header-only, tout inline**. Deux catégories
de champs :

**(a) Ce que l'APPLICATION pose (état brut)** :
`mousePos`, `mouseDown[3]` (0=gauche 1=droit 2=milieu), `wheel`, `wheelH`,
`ctrlDown`/`shiftDown`/`altDown`, `chars[32]`+`charCount` (via `PushChar(cp)`),
`keyDown[KeyCount]` (via `SetKey(NkGuiKey, bool)`), `wantCopy/wantCut/wantPaste/wantSelectAll`,
et le double-clic OS via `SetDoubleClick(button)`.

**(b) Ce que NKGui CALCULE** (dans `NewFrame()`, `NkGuiInput.h:103-135`) :
`mouseClicked[]`, `mouseReleased[]`, `mouseDoubleClicked[]`, `mouseDownDur[]`,
`keyInit[]`, `keyDur[]`.

```cpp
void NewFrame() noexcept {
    for (int32 i = 0; i < 3; ++i) {
        mouseClicked[i]  =  mouseDown[i] && !mousePrev[i];
        mouseReleased[i] = !mouseDown[i] &&  mousePrev[i];
        …
        // Double-clic : (a) détection interne (2e clic < 0.40 s) OU
        // (b) injection OS via SetDoubleClick (consommée puis remise à 0).
        …
    }
    for (int32 i = 0; i < KeyCount; ++i) {
        keyInit[i] = keyDown[i] && !keyPrev[i];
        …
    }
}
```

**Répétition au maintien (« typematic »)** — `NkGuiInput.h:17-28` +
`KeyPressedRepeat(k, delay=0.30f, rate=0.04f)` (lignes 87-94) : renvoie le nombre
de déclenchements franchis entre deux durées d'appui. Utilisé pour les flèches,
Backspace, et les boutons `Repeat`.

**Touches suivies** — `NkGuiKey` (`NkGuiTypes.h:75-129`) : NKGui ne suit pas tout
le clavier, seulement une liste explicite : `Left, Right, Up, Down, Home, End,
Backspace, Delete, Enter, Escape, Tab, F2, F5, C, D, H, L, N, Num0…Num2, G, K,
Slash, LBracket, RBracket, Z, Y, Minus, Equal, F, Space, F8, F12, J, W, T, X, P,
V, I, O, Backslash, Period, Count`. La saisie de texte, elle, passe par `chars[]`
(codepoints Unicode), pas par les touches.

### 3.6 Le cycle de frame — `BeginFrame` / `EndFrame`

**`BeginFrame(dt)`** (`Core\NkGuiContext.cpp:39-75`) — à appeler **APRÈS** que
l'application a posé l'entrée brute :

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
    idDepth = 0;
    disabledDepth = 0;
    inputClickConsumed = false;
    curPopupLevel = -1;    // le dessin reprend sur la couche principale
    // Routeur d'occlusion : la liste ecrite la frame PRECEDENTE devient la
    // liste LUE (stable toute la frame, comme hotIdPrev) ; on repart a zero
    // pour l'ecriture de cette frame.
    occlCount = occlCountNew;
    for (int32 i = 0; i < occlCountNew; ++i) { occlRects[i] = occlRectsNew[i]; occlLayers[i] = occlLayersNew[i]; }
    occlCountNew = 0;
    curInputLayer = 0;
    winCount = 0;          // pool de fenêtres ré-attribué cette frame
    curWindow = -1; curWindowId = NKGUI_ID_NONE; curWindowDocked = false;
    containerDepth = 0;    // pile de conteneurs ré-attribuée
    overlayDepth = 0;
    for (uint32 i = 0; i < windowMeta.Size(); ++i) {
        windowMeta[i].hostRendered = false;
        windowMeta[i].frameDL = -1;
        windowMeta[i].dockDL = -2;
    }
    dl.Reset();
    dlOverlay.Reset();
}
```

**`EndFrame()`** (`Core\NkGuiContext.cpp:77-142`) fait cinq choses :

1. **Fusionne les draw-lists de fenêtres dans `dl`, triées par z-order** (tri par
   insertion, lignes 80-94) → recouvrement correct.
2. **Détermine la fenêtre survolée** pour la frame suivante (`hoveredWindowId`,
   lignes 96-106), en prenant celle de z-order le plus haut sous le curseur.
3. **Ferme la chaîne de popups** : Échap ferme le niveau le plus profond ; un clic
   hors de tous les popups et hors de l'ancre ferme tout (lignes 113-124).
4. **Anti-gel de `activeId`** — commentaire précieux (lignes 125-132) :

   ```cpp
   // ANTI-GEL : aucun glissement légitime ne conserve activeId bouton RELÂCHÉ.
   // Si le widget détenant activeId a disparu (hôte redevenu flottant, onglet
   // caché, fenêtre fermée…) il ne libère jamais activeId et l'occlusion bloque
   // TOUTE interaction. Souris haute + activeId encore posé ⇒ on libère d'office.
   if (!input.mouseDown[0] && activeId != NKGUI_ID_NONE) {
       activeId = NKGUI_ID_NONE;
       movingWindowId = NKGUI_ID_NONE;
   }
   ```

5. **Consomme** la molette et le texte : `input.wheel = 0.f; input.wheelH = 0.f;
   input.ClearPerFrameText();` (lignes 139-141), et défocalise le champ texte si
   un clic a eu lieu hors de tout champ (137-138).

> **Invariant d'ordre à retenir (le plus important du framework) :**
> `événements (callbacks NKEvent) → ctx.BeginFrame(dt) → widgets → ctx.EndFrame() → backend.Submit(...)`.
> `BeginFrame` appelle `input.NewFrame()` en interne : si on pompe les événements
> *après* `BeginFrame`, les transitions clic/relâche portent sur l'état de la
> frame précédente.

### 3.7 Comment un widget conserve son état entre deux images

**Réponse : il ne conserve rien lui-même.** L'état vit dans `NkGuiContext`, indexé
par `NkGuiId`, dans des tableaux parallèles clé/valeur :

| État persistant | Stockage | Lignes `NkGuiContext.h` |
|---|---|---|
| Nœuds d'arbre ouverts | `NkVector<NkGuiId> openNodes` | 358 |
| Onglet sélectionné par barre | `tabBarKeys` + `tabBarSel` | 359-360 |
| Défilement par zone | `scrollKeys` + `scrollVals` (`NkGuiScrollState`) | 377-378 |
| Focus/ancre de liste | `selKeys` + `selFocusStore` + `selAnchorStore` | 363-365 |
| Largeurs de colonnes de table | `tblKeys` + `tblWidths` (`NkGuiTableW`) | 419-420 |
| HSV du color picker | `pickerKeys` + `pickerHSV` | 424-425 |
| Fenêtres (pos/taille/repli/z/dock) | `NkVector<NkGuiWindowMeta> windowMeta` | 236 |
| Arbre de dock | `NkVector<NkGuiDockNode> dockNodes` | 248 |

Exemple minimal — l'ouverture d'un `TreeNode` (`NkGuiContext.cpp:336-355`) :

```cpp
bool NkGuiContext::IsNodeOpen(NkGuiId id) const noexcept {
    for (uint32 i = 0; i < openNodes.Size(); ++i)
        if (openNodes[i] == id) return true;
    return false;
}
void NkGuiContext::SetNodeOpen(NkGuiId id, bool open) noexcept { … /* swap-remove */ }
```

*(Recherche linéaire — pas de table de hachage. C'est assumé : les listes sont
petites.)*

**Attention, distinction à faire en cours** : les **valeurs métier** (le `bool`
d'une case à cocher, le `float` d'un slider, le tampon `char[]` d'un champ texte)
appartiennent à **l'application**, pas à NKGui — elles sont passées par référence
ou par pointeur (`Checkbox(ctx, "X", bool &value)`). NKGui ne stocke que l'état
*d'interaction* (est-ce ouvert ? où est le caret ? quelle est la largeur des
colonnes ?).

**État de fenêtre** — `NkGuiWindowMeta` (`NkGuiTypes.h:263-284`) : `id`, `rect`,
`collapsed`, `init`, `zOrder`, puis la partie docking (`dockNode`, `dockRect`,
`dockActiveTab`, `hideSingleTab`, `floatRect`, `title[48]`, `hostRoot`,
`hostRect`, `dockHost`, `hostRendered`, `frameDL`, `dockDL`).

**Nœud de dock** — `NkGuiDockNode` (`NkGuiTypes.h:288-299`) : `kind` (0=vide,
1=split, 2=feuille), `vertical`, `ratio`, `child0`/`child1`/`parent`,
`windows[8]` (max 8 onglets par feuille), `winCount`, `activeTab`, `rect`.

### 3.8 Comment un widget reçoit les événements — le trio hot/active/occlusion

**`ItemHoverable(rect, id)`** est LE point de passage obligé de tout widget
interactif. Le code entier (`Core\NkGuiContext.cpp:491-528`) — chaque `return
false` est une leçon :

```cpp
bool NkGuiContext::ItemHoverable(const NkRect &r, NkGuiId id) noexcept {
    // Routeur d'occlusion UNIFIE : un widget n'est jamais survolable si une
    // surface flottante d'une couche SUPERIEURE (modal, palette, popover
    // declares via PushOcclusion) recouvre le pointeur — quel que soit
    // l'ordre de dessin des panneaux.
    if (!PointReachable(input.mousePos)) return false;
    // Désactivé : aucune interaction.
    if (IsDisabled()) return false;
    // Un popup ouvert capture le pointeur : un widget ne réagit pas si le
    // pointeur est au-dessus d'un popup PLUS PROFOND que le niveau courant
    // (couche principale = -1 ; un item de menu ne capture pas sous son
    // sous-menu déployé).
    for (int32 i = curPopupLevel + 1; i < popupDepth; ++i)
        if (NkGuiRectContains(popupRects[i], input.mousePos)) return false;
    // Occlusion par fenêtre : hors popup, un widget ne réagit que si SA fenêtre
    // est celle survolée au-dessus (curWindowId). Bloque le fond ET les fenêtres
    // recouvertes. (hoveredWindowId/curWindowId = NONE pour le fond hors fenêtre.)
    if (curPopupLevel < 0 && hoveredWindowId != NKGUI_ID_NONE && hoveredWindowId != curWindowId)
        return false;
    // Hors du CLIP courant (zone défilable, panneau) : pas d'interaction —
    // un item scrollé hors-vue ne doit pas capturer le pointeur.
    if (!NkGuiRectContains(DL().CurrentClip(), input.mousePos)) return false;
    // Bloqué si un AUTRE widget capture le pointeur.
    if (activeId != NKGUI_ID_NONE && activeId != id) return false;
    if (!NkGuiRectContains(r, input.mousePos)) return false;
    // Greedy : le DERNIER widget soumis sous le pointeur écrase hotId →
    // celui dessiné par-dessus gagne. On ne déclare « survolé » QUE le
    // front-most de la frame précédente (hotIdPrev) ; le widget masqué
    // dessous, lui, met à jour hotId mais retourne false → ne capture pas.
    hotId = id;
    return hotIdPrev == id;
}
```

**Le mécanisme « greedy + une frame de retard »** est le point conceptuel le plus
difficile du framework, et il vaut la peine d'être expliqué lentement en cours :
en mode immédiat on ne connaît pas les rectangles des widgets *à venir*, donc on
ne peut pas savoir qui est au-dessus. NKGui résout par un décalage d'une image :
chaque widget sous le curseur **écrase** `hotId` (donc le dernier dessiné, celui
au-dessus, gagne), et le résultat sert **à la frame suivante** via `hotIdPrev`.

**`ButtonBehavior(...)`** — le comportement générique de tout élément cliquable
(`Core\NkGuiContext.cpp:530-570`), signature complète (`NkGuiContext.h:541-543`) :

```cpp
bool ButtonBehavior(NkGuiId id, const NkRect &r, NkGuiButtonFlags flags = NkGuiButtonFlags::None,
                    float32 repeatDelayOverride = -1.f, float32 repeatRateOverride = -1.f,
                    bool *outHovered = nullptr, bool *outHeld = nullptr) noexcept;
```

Sémantique (extraits du corps) :

```cpp
const bool hovered = ItemHoverable(r, id);   // respecte le z-ordre
…
if (hovered && input.mouseClicked[0]) {
    activeId = id;
    if (repeat) pressed = true;              // rafale : déclenche dès l'appui
}
const bool held = (activeId == id);
if (held) {
    interact = NkGuiInteract::EditWidget;
    …
    if (input.mouseReleased[0]) {
        // Sans Repeat : clic validé au relâchement DANS le rect.
        if (!repeat && NkGuiRectContains(r, input.mousePos)) pressed = true;
        activeId = NKGUI_ID_NONE;
    }
} else if (hovered) {
    interact = NkGuiInteract::HoverWidget;
}
…
lastItemHovered = hovered;   // pour IsItemHovered() / SetTooltip
```

→ **Un clic « standard » se valide au RELÂCHEMENT dans le rectangle** (on peut
donc annuler en glissant hors du bouton avant de relâcher). Avec le flag `Repeat`,
il se déclenche à l'appui puis en rafale.

**La machine à états d'interaction** — `NkGuiTypes.h:52-61`, avec sa raison d'être :

```cpp
// ── Machine à états d'interaction (⭐ le cœur du fix UX) ───────────────
// À chaque frame, UN SEUL mode actif et explicite. Zones de préhension
// disjointes et priorisées (resize > move > contenu), chacune avec son
// curseur et son affordance. Voir ARCHITECTURE.md §4.
enum class NkGuiInteract : uint8 {
    None = 0,
    HoverWidget,  ///< survol d'un widget
    EditWidget,   ///< édition active (slider/input…)
    MoveWindow,   ///< déplacement d'une fenêtre (barre de titre / onglet)
    ResizeWindow, ///< redimensionnement (bord/coin — cf. edge)
    DragSplitter, ///< glissement d'un séparateur de dock
    DragTab,      ///< glissement d'un onglet (réordre / détache)
    DockTarget    ///< visée d'une cible de dock (boussole)
};
```

Règle d'or citée en `ARCHITECTURE.md:121-124` :

> *« les zones de préhension sont disjointes et priorisées (bord/coin de resize >
> barre de titre/onglet pour move > contenu), chacune avec son curseur
> (`NkWindow::SetCursor`, déjà en place) et son affordance visuelle.
> → Plus jamais « je voulais redimensionner et ça a docké ». »*

**Le routeur d'occlusion** — pour les surfaces flottantes « maison » de
l'application (modals, palettes, popovers). Le commentaire d'en-tête décrit
précisément le problème résolu (`NkGuiContext.h:163-182`) :

> ```
> // ── ROUTEUR D'OCCLUSION UNIFIÉ (surfaces flottantes « maison ») ─────
> // PROBLÈME DE FOND résolu ici : les overlays applicatifs (modals, palettes,
> // popovers, peek…) faisaient des hit-tests BRUTS (rect + mouseClicked) qui
> // ignoraient ce qui est dessiné AU-DESSUS d'eux → traversées de clics,
> // boutons inertes, consommations manuelles fragiles dupliquées partout.
> // PRINCIPE : chaque surface flottante déclare son rect + sa couche CHAQUE
> // frame (PushOcclusion) pendant son dessin ; tous les hit-tests passent par
> // InputHits/ClickIn qui refusent un point recouvert par une couche PLUS
> // HAUTE que celle en cours de dessin (curInputLayer). Comme hotIdPrev, la
> // liste lue est celle de la FRAME PRÉCÉDENTE (stable pour toute la frame,
> // indépendante de l'ordre de dessin des panneaux). Couches conseillées :
> // 0 fond/panneaux · 50 menus/palettes/popovers · 100 modals · 200 debug.
> ```

API associée (`NkGuiContext.h:185-223`) :

```cpp
void PushOcclusion(const NkRect &r, int32 layer) noexcept;   // déclarer une surface
bool PointReachable(const NkVec2 &p) const noexcept;          // point atteignable ?
bool InputHits(const NkRect &r) const noexcept;               // hit-test UNIFIÉ
bool ClickIn(const NkRect &r) const noexcept;                 // clic gauche unifié

// RAII : fixe la couche d'input pendant le dessin d'une surface flottante.
struct NkInputLayerScope {
        NkGuiContext *c; int32 saved;
        NkInputLayerScope(NkGuiContext &ctx, int32 layer) : c(&ctx), saved(ctx.curInputLayer) {
            ctx.curInputLayer = layer;
        }
        ~NkInputLayerScope() { c->curInputLayer = saved; }
};
```

Directive du header, à répéter en cours (`NkGuiContext.h:202-203`) :

> *« Hit-test UNIFIÉ : souris dans `r` ET non recouverte par une couche
> supérieure. À utiliser PARTOUT à la place de `NkGuiRectContains(mousePos)`. »*

**Le focus clavier** est *par domaine*, pas global :
- champ texte focalisé → `ctx.inputId` (+ `inputCaret`, `inputAnchor`, `inputScroll`) ;
- liste ayant le focus clavier → `ctx.activeSelList` (+ `selFocus`, `selAnchor`) ;
- fenêtre au premier plan → `ctx.windowZTop` / `zOrder` de `NkGuiWindowMeta`.
Il n'existe **pas** de navigation Tab entre widgets (« nav clavier » est en Phase 8
selon `ARCHITECTURE.md:193`).

### 3.9 La mise en page (layout)

**Le modèle** : un **curseur** qui descend, comme une machine à écrire.
`NkGuiLayout` (`NkGuiContext.h:71-95`) : `region`, `cursor`, `lineStartX`,
`curLineH`, `prevItem`, `maxX`/`maxY`, `padding=10`, `itemSpacingX=8`,
`itemSpacingY=6`, plus les champs de flux.

**Les 7 modes de flux** (champ `layout.flow`), implémentés dans
`NextItemRect` (`Core\NkGuiContext.cpp:194-318`) :

| `flow` | Mode | Comportement | Lignes |
|---|---|---|---|
| 0 | **Vertical** (défaut) | un item par ligne, le curseur descend ; `w<=0` = remplir la largeur | 303-317 |
| 1 | **HBox** | le curseur avance en X, pas de retour à la ligne ; `w<=0` → 120 px | 195-209 |
| 2 | **Grid** | colonnes régulières, retour à la ligne tous les `gridCols` | 210-233 |
| 3 | **Stack** | tous les enfants superposés, ancrés dans une boîte `stackW×stackH` (9 ancres) | 285-302 |
| 4 | **Flow** | comme HBox mais passe à la ligne quand ça déborde (tags, toolbars) | 264-284 |
| 5 | **Row** (flex) | rangée horizontale, largeurs pré-calculées `flexSlots[]`, hauteur étirée | 234-248 |
| 6 | **Column** (flex) | colonne verticale, hauteurs pré-calculées, largeur étirée | 249-263 |

Le mode vertical, le plus simple, pour illustrer le principe (`cpp:303-317`) :

```cpp
if (w <= 0.f) w = ContentWidth();
const NkRect rect = {layout.cursor.x, layout.cursor.y, w, h};
layout.prevItem = rect;
if (rect.x + rect.w > layout.maxX) layout.maxX = rect.x + rect.w;  // largeur contenu
if (h > layout.curLineH) layout.curLineH = h;
layout.cursor.x = layout.lineStartX;
layout.cursor.y += layout.curLineH + layout.itemSpacingY;
if (layout.cursor.y > layout.maxY) layout.maxY = layout.cursor.y;
layout.curLineH = 0.f;
return rect;
```

**Les helpers de curseur** (`NkGuiContext.cpp:320-334` et `.h:453-466`) :

```cpp
void    BeginLayout(const NkRect &region) noexcept;   // ouvre une région de contenu
NkRect  NextItemRect(float32 w, float32 h) noexcept;  // w<=0 = remplir la largeur
void    SameLine(float32 spacingX = -1.f) noexcept;   // item suivant à droite du précédent
void    Spacing(float32 px = -1.f) noexcept;          // saut vertical
float32 ContentWidth() const noexcept;                // largeur restante au curseur
float32 AvailHeight() const noexcept;                 // hauteur restante sous le curseur
float32 ItemHeight() const noexcept;                  // hauteur standard d'un widget
float32 S(float32 px) const noexcept { return px * scale; }  // px logiques → px écran
void    Indent(float32 w) noexcept;                   // décale le début de ligne (arbres)
```

`ItemHeight()` est simplement `lineHeight + 2 * theme.framePadY`
(`NkGuiContext.cpp:189-192`), avec 16 px de repli si aucune police n'est chargée.

**Les conteneurs empilables** (`NkGuiWidgets.h:142-190`) sauvegardent le layout
parent puis le restaurent, en avançant le parent du bloc consommé
(`NkGuiWidgets.cpp:1322-1365`, helpers `BeginContainer`/`EndContainer`) :
`BeginVBox/EndVBox`, `BeginHBox/EndHBox`, `BeginGrid/EndGrid`,
`BeginGroup/EndGroup`, `BeginFlow/EndFlow`, `BeginRow/EndRow`,
`BeginColumn/EndColumn`, `BeginStack/StackAnchor/EndStack`, plus `Spacer`,
`SpringRight`, `Splitter`. Ils sont **composables** (un HBox dans une cellule de
Grid, etc. — `NkGuiWidgets.h:144`).

**Le flex à poids** — `NkGuiWidgets.h:159-160` :

> *« `sizes[i] > 0` = px fixes ; `sizes[i] < 0` = poids (-1 = poids 1) qui se
> partagent l'espace RESTANT. Single-pass exact (total connu). »*

**Le DPI** — `SetUiScale(ctx, s)` multiplie padding/rounding/espacement, et
l'application doit **recharger la police** à `tailleBase * scale`
(`NkGuiWidgets.h:171-174`, impl. `NkGuiWidgets.cpp:1467`).

### 3.10 Panneaux, fenêtres, docking

**`BeginPanel` / `EndPanel`** — le conteneur simple (`NkGuiWidgets.cpp:2047-2078`).
Le code entier, court et lisible, à donner en exemple :

```cpp
bool BeginPanel(NkGuiContext &ctx, const char *title, const NkRect &r) noexcept {
    ctx.DL().AddRectFilled(r, ctx.theme.panel, ctx.theme.rounding); // fond
    float32 top = r.y;
    if (title && *title) {
        const float32 th = ctx.ItemHeight();
        ctx.DL().AddRectFilled({r.x, r.y, r.w, th}, ctx.theme.header, ctx.theme.rounding);
        ctx.DL().AddRectFilled({r.x, r.y + th - 1.f, r.w, 1.f}, ctx.theme.border); // séparateur
        if (ctx.font && ctx.font->Valid()) {
            ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
                             {r.x + 10.f, r.y + (th - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent()},
                             title, ctx.theme.text);
        }
        top = r.y + th;
    }
    // Bordure EN DERNIER → entoure tout le panneau (en-tête inclus), n'est
    // plus recouverte par le fond de la barre de titre.
    ctx.DL().AddRect(r, ctx.theme.border, 1.f);
    // Le contenu (sous l'en-tête) est une ZONE DÉFILABLE : si les widgets
    // dépassent la hauteur visible, une scrollbar apparaît automatiquement…
    const NkRect content = {r.x, top, r.w, r.y + r.h - top};
    const NkGuiId id = ctx.GetId((title && *title) ? title : "##panel");
    return BeginScrollFrame(ctx, id, content, false);
}

void EndPanel(NkGuiContext &ctx) noexcept { EndScrollFrame(ctx); }
```

**`Begin` / `EndWindow`** — fenêtres flottantes (`NkGuiWidgets.cpp:2269-2560`).
Convention capitale, citée dans le header (`NkGuiWidgets.h:253-254`) :

> *« Appeler End UNIQUEMENT si Begin retourne true (convention NKGui, comme
> BeginPanel).
>   `if (Begin(ctx,"Inspecteur",&open)) { …widgets… EndWindow(ctx); }` »*

Structure interne de `Begin` en deux phases (`NkGuiWidgets.cpp:2396-2397`) :

> ```
> // ── PHASE 1 : INTERACTIONS — mettent à jour `wr` AVANT le dessin (sinon le
> //    contenu, dessiné à la nouvelle position, « court après » le chrome). ──
> ```

À la création, la fenêtre reçoit un décalage en cascade pour ne pas empiler toutes
les fenêtres au même endroit (`NkGuiWidgets.cpp:2286-2288`) :

```cpp
const float32 off = static_cast<float32>(ctx.windowMeta.Size() % 8) * 28.f;
nm.rect = {90.f + off, 80.f + off, 320.f, 210.f};
```
puis `SetNextWindowPos`/`SetNextWindowSize` sont appliqués **à la création
seulement** (« FirstUseEver ») et consommés : `ctx.hasNextPos = ctx.hasNextSize = false;`
(`cpp:2293`).

**Chaque fenêtre dessine dans SA propre draw-list** (`NkGuiContext.h:225-232`) :

> ```
> // ── Fenêtres flottantes (Begin/End) ───────────────────────────────
> // Chaque fenêtre dessine dans SA draw-list (pool `winDL`), fusionnées dans
> // `dl` triées par z-order à EndFrame → recouvrement correct + passage devant.
> static constexpr int32 WinMax = 32;
> NkGuiDrawList winDL[WinMax];
> ```

**La sélection de la draw-list active** (`NkGuiContext.h:299-303`) — trois cas :

```cpp
NkGuiDrawList &DL() noexcept {
    if (curPopupLevel >= 0 || overlayDepth > 0)
        return dlOverlay;                  // popup / couche overlay forcée
    return curWindow >= 0 ? winDL[curWindow] : dl;
}
```

**Docking** — arbre de nœuds (`NkGuiDockNode`), fonctions dans
`NkGuiWidgets.cpp:2562-3410` : `DockSpace`, `DockSpaceOverViewport`,
`DockBuilderDock`, `DockBuilderDockTab`, `DockFocusWindow`, `DockIsWindowDocked`,
`DockWindowNode`, `DockDetachWindow`, `DockWindowHideSingleTab`,
`DockWindowIntoWindow`, `DockTabAddRequest`, `DockAddTab`.
Ordre d'appel documenté (`NkGuiWidgets.h:266-267`) : *« À appeler AVANT les Begin
des fenêtres dockables »*.

### 3.11 Le hook de style — re-skin total sans toucher aux widgets

`NkGuiTypes.h:301-325` :

```cpp
// ── Hook de style : override de DESSIN par widget ─────────────────────
// L'app enregistre `ctx.styleFn` ; pour chaque élément visuel, NKGui l'appelle
// AVANT le rendu par défaut. Si le callback retourne true, il a dessiné lui-même
// (le défaut est sauté) → re-skin TOTAL sans toucher la logique du widget.
enum class NkGuiStyleKind : uint8 {
    Button = 0, FrameBg, CheckMark, Header, Selectable, DockTarget, Count
};

struct NkGuiStyleItem {
        NkGuiStyleKind kind = NkGuiStyleKind::Button;
        NkRect rect = {0.f, 0.f, 0.f, 0.f};
        NkGuiId id = NKGUI_ID_NONE;
        const char *label = nullptr;
        bool hovered = false;
        bool active = false;   ///< pressé / en cours d'édition / cible visée
        bool selected = false;
        bool disabled = false;
        int32 value = 0;       ///< donnée par type (DockTarget : 0=onglet,1=G,2=D,3=H,4=B,5=bord)
};
```

Branchement (`NkGuiContext.h:429-431`) :

```cpp
using NkGuiStyleFn = bool (*)(NkGuiContext &, const NkGuiStyleItem &, void *);
NkGuiStyleFn styleFn = nullptr;
void *styleUser = nullptr;
```

Le dispatcheur interne est `StyleDraw(...)` (`NkGuiWidgets.cpp:76-92`), avec une
variante pour les cibles de dock qui **route temporairement `ctx.DL()` vers
l'overlay** le temps du callback (`NkGuiWidgets.cpp:95-104`).

---

## 4. Catalogue exhaustif des widgets NKGui

Toutes les signatures ci-dessous viennent de
`Kernel\Runtime\NKGui\src\NKGui\Widgets\NkGuiWidgets.h`. La macro
`NKENTSEU_NKGUI_API` est vide en build statique (défaut) — je l'omets pour la
lisibilité. **Toutes** ont une définition dans `NkGuiWidgets.cpp` (vérifié
fonction par fonction) : aucune déclaration vide.

### 4.1 Texte et boutons

```cpp
void    PanelBackground(NkGuiContext &ctx, const NkRect &r) noexcept;                     // .h:16  · .cpp:106
float32 TextAt(NkGuiContext &ctx, const NkVec2 &topLeft, const char *s) noexcept;         // .h:20  · .cpp:120
float32 TextAt(NkGuiContext &ctx, const NkVec2 &topLeft, const char *s, const NkColor &col) noexcept; // .h:21 · .cpp:111
void    Text(NkGuiContext &ctx, const char *s) noexcept;                                  // .h:39  · .cpp:166
void    TextWrapped(NkGuiContext &ctx, const char *text, float32 wrapWidth = -1.f) noexcept; // .h:43 · .cpp:235
bool    Button(NkGuiContext &ctx, const char *label, const NkRect &r) noexcept;           // .h:26  · .cpp:155
bool    Button(NkGuiContext &ctx, const char *label) noexcept;             // rect auto    // .h:44  · .cpp:256
bool    ButtonEx(NkGuiContext &ctx, const char *label, const NkRect &r, NkGuiButtonFlags flags,
                 float32 repeatDelay = -1.f, float32 repeatRate = -1.f) noexcept;          // .h:30  · .cpp:135
bool    RepeatButton(NkGuiContext &ctx, const char *label, const NkRect &r,
                     float32 repeatDelay = -1.f, float32 repeatRate = -1.f) noexcept;      // .h:34  · .cpp:159
void    Separator(NkGuiContext &ctx) noexcept;                                            // .h:140 · .cpp:1306
```

`Text` en entier (`NkGuiWidgets.cpp:166-172`) — le widget le plus simple, parfait
pour un premier cours :

```cpp
void Text(NkGuiContext &ctx, const char *s) noexcept {
    const float32 lh = (ctx.font && ctx.font->Valid()) ? ctx.font->LineHeight() : 16.f;
    const float32 w  = (ctx.font && ctx.font->Valid()) ? ctx.font->MeasureWidth(s) : 0.f;
    const NkRect r = ctx.NextItemRect(w, lh);
    TextAt(ctx, {r.x, r.y}, s, ctx.IsDisabled() ? ctx.theme.textDisabled : ctx.theme.text);
}
```

Et `Button` (auto-layout) se réduit à `ButtonEx` (`cpp:155-157`), lui-même à
`ctx.ButtonBehavior` + dessin (`cpp:135-153`).

### 4.2 Cases à cocher et sélection hiérarchique

```cpp
bool Checkbox(NkGuiContext &ctx, const char *label, bool &value) noexcept;                 // .h:45  · .cpp:296
bool CheckboxTristate(NkGuiContext &ctx, const char *label, NkGuiCheck &state) noexcept;   // .h:48  · .cpp:303
bool CheckBox3(NkGuiContext &ctx, const char *idStr, NkGuiCheck &state) noexcept;          // .h:53  · .cpp:310
void NkGuiTreeCascade(NkGuiCheck *states, const int32 *parent, int32 count, int32 node,
                      NkGuiCheck value) noexcept;                                          // .h:60  · .cpp:337
void NkGuiTreeRecomputeMixed(NkGuiCheck *states, const int32 *parent, int32 count) noexcept; // .h:64 · .cpp:356
```

Modèle documenté (« hybride ») — `NkGuiWidgets.h:55-58` :

> *« NKGui ne stocke PAS la hiérarchie (façon ImGui) : l'app possède `states[]`
> (1 NkGuiCheck par nœud) + `parent[]` (index du parent, -1 = racine). Ces
> utilitaires agissent sur ces tableaux quand l'app le décide (ex. Alt+clic). »*

### 4.3 Valeurs numériques

```cpp
bool SliderFloat(NkGuiContext &ctx, const char *label, float32 &value, float32 vmin, float32 vmax) noexcept; // .h:65 · .cpp:390
bool DragFloat  (NkGuiContext &ctx, const char *label, float32 &v, float32 speed = 0.1f,
                 float32 vmin = -1.0e30f, float32 vmax = 1.0e30f,
                 NkGuiDragDir dir = NkGuiDragDir::Horizontal) noexcept;                    // .h:112 · .cpp:576
bool DragInt    (NkGuiContext &ctx, const char *label, int32 &v, float32 speed = 0.25f,
                 int32 vmin = -2147483640, int32 vmax = 2147483640,
                 NkGuiDragDir dir = NkGuiDragDir::Horizontal) noexcept;                    // .h:115 · .cpp:581
bool InputFloat (NkGuiContext &ctx, const char *label, float32 &v, float32 step = 1.f,
                 float32 vmin = -1.0e30f, float32 vmax = 1.0e30f,
                 NkGuiDragDir dir = NkGuiDragDir::Horizontal) noexcept;                    // .h:118 · .cpp:591
bool InputInt   (NkGuiContext &ctx, const char *label, int32 &v, int32 step = 1,
                 int32 vmin = -2147483640, int32 vmax = 2147483640,
                 NkGuiDragDir dir = NkGuiDragDir::Horizontal) noexcept;                    // .h:121 · .cpp:596
```

Comportement documenté (`NkGuiWidgets.h:106-111`) :

> *« DragFloat/DragInt : GLISSER sur le champ pour changer la valeur (vitesse
> `speed`/pixel) + DOUBLE-CLIC pour saisir au clavier. InputFloat/InputInt : idem
> + boutons -/+ (pas `step`). […] Pendant le survol/glisser, le widget pose
> `ctx.wantCursor` (↔ ou ↕) que l'app peut appliquer. »*

### 4.4 Saisie de texte

```cpp
bool InputText(NkGuiContext &ctx, const char *label, char *buf, int32 bufSize) noexcept;   // .h:126 · .cpp:794
bool InputTextEx(NkGuiContext &ctx, const char *label, char *buf, int32 bufSize,
                 NkGuiInputFlags flags, int32 maxChars = -1) noexcept;                     // .h:130 · .cpp:762
bool InputTextMultiline(NkGuiContext &ctx, const char *idStr, char *buf, int32 bufSize,
                        const NkRect &rect, NkGuiInputFlags flags = NkGuiInputFlags::None,
                        int32 maxChars = -1, bool wrap = false) noexcept;                  // .h:137 · .cpp:894
```

Flags disponibles (`NkGuiTypes.h:212-220`) : `Password`, `CharsDecimal`,
`CharsHex`, `Uppercase`, `NoBlank`, `ReadOnly`.
Distinction à souligner (`NkGuiWidgets.h:128-131`) : `maxChars` compte en
**codepoints**, `bufSize` borne les **octets**.
`InputText` renvoie `true` **à la pression d'Entrée** ; `InputTextMultiline`
renvoie `true` **si le texte a changé**. Ce n'est pas la même sémantique.

### 4.5 Graphiques et couleur

```cpp
void ProgressBar(NkGuiContext &ctx, float32 fraction, const char *overlay = nullptr) noexcept;   // .h:71  · .cpp:3947
void PlotLines(NkGuiContext &ctx, const char *label, const float32 *values, int32 count,
               float32 minV = 0.f, float32 maxV = 0.f, float32 height = 0.f) noexcept;           // .h:76  · .cpp:4039
void PlotHistogram(NkGuiContext &ctx, const char *label, const float32 *values, int32 count,
                   float32 minV = 0.f, float32 maxV = 0.f, float32 height = 0.f) noexcept;       // .h:78  · .cpp:4044

bool ColorButton(NkGuiContext &ctx, const char *idStr, const float32 *col, float32 w = 0.f,
                 float32 h = 0.f, NkGuiColorFlags flags = NkGuiColorFlags::None) noexcept;       // .h:84  · .cpp:4219
bool ColorPicker4(NkGuiContext &ctx, const char *label, float32 *col,
                  NkGuiColorFlags flags = NkGuiColorFlags::None) noexcept;                       // .h:89  · .cpp:4232
bool ColorEdit4(NkGuiContext &ctx, const char *label, float32 *col,
                NkGuiColorFlags flags = NkGuiColorFlags::None) noexcept;                         // .h:93  · .cpp:4449
```

`NkGuiColorFlags` (`NkGuiTypes.h:151-159`) : `NoAlpha`, `Wheel` (roue + triangle SV),
`NoInputs`, `Disc` (disque plein), `Hexagon`, `Honeycomb` (nid d'abeille).
Détail de conception à citer (`NkGuiWidgets.h:87-88`) : *« La TEINTE est préservée
même au noir/blanc (HSV mémorisé par id). »* — d'où `pickerKeys`/`pickerHSV` dans
le contexte.

Amélioration revendiquée sur ImGui (`NkGuiWidgets.h:75`) : *« la valeur sous le
curseur s'affiche en direct (survol) »* pour les plots.

### 4.6 Images

```cpp
void Image(NkGuiContext &ctx, uint32 texId, float32 w, float32 h,
           NkColor tint = NkColor{255,255,255,255},
           NkVec2 uv0 = NkVec2{0.f,0.f}, NkVec2 uv1 = NkVec2{1.f,1.f}) noexcept;          // .h:99  · .cpp:4509
bool ImageButton(NkGuiContext &ctx, const char *idStr, uint32 texId, float32 w, float32 h,
                 NkColor tint = NkColor{255,255,255,255},
                 NkVec2 uv0 = NkVec2{0.f,0.f}, NkVec2 uv1 = NkVec2{1.f,1.f}) noexcept;    // .h:103 · .cpp:4518
```

`texId` est **l'identifiant côté backend** — l'application doit avoir appelé
`backend.UploadImageRGBA(texId, …)` au préalable (voir §5.2).

### 4.7 Conteneurs de layout

```cpp
void BeginVBox(NkGuiContext &ctx, float32 gap = -1.f) noexcept;  void EndVBox(NkGuiContext&) noexcept;  // .h:146-147 · .cpp:1366/1370
void BeginHBox(NkGuiContext &ctx, float32 gap = -1.f) noexcept;  void EndHBox(NkGuiContext&) noexcept;  // .h:148-149 · .cpp:1374/1378
void BeginGrid(NkGuiContext &ctx, int32 columns, float32 gap = -1.f) noexcept; void EndGrid(…) noexcept; // .h:150-152 · .cpp:1382/1386
void BeginGroup(NkGuiContext &ctx) noexcept;  void EndGroup(NkGuiContext&) noexcept;                    // .h:153-154 · .cpp:1390/1394
void BeginFlow(NkGuiContext &ctx, float32 gap = -1.f) noexcept;  void EndFlow(NkGuiContext&) noexcept;  // .h:155-158 · .cpp:1398/1402
void BeginRow(NkGuiContext &ctx, float32 height, const float32 *sizes, int32 count, float32 gap = -1.f) noexcept;
void EndRow(NkGuiContext &ctx) noexcept;                                                                // .h:163-165 · .cpp:1437/1443
void BeginColumn(NkGuiContext &ctx, float32 width, const float32 *sizes, int32 count,
                 float32 totalHeight = -1.f, float32 gap = -1.f) noexcept;
void EndColumn(NkGuiContext &ctx) noexcept;                                                             // .h:168-170 · .cpp:1447/1455
void BeginStack(NkGuiContext &ctx, float32 width, float32 height) noexcept;
void StackAnchor(NkGuiContext &ctx, int32 anchor) noexcept;  // 0=HG 1=HC 2=HD 3=CG 4=C 5=CD 6=BG 7=BC 8=BD
void EndStack(NkGuiContext &ctx) noexcept;                                                              // .h:182-185 · .cpp:1524/1531/1535
void  Spacer(NkGuiContext &ctx, float32 w, float32 h) noexcept;                                         // .h:186 · .cpp:1540
void  SpringRight(NkGuiContext &ctx, float32 width) noexcept;                                           // .h:175 · .cpp:1487
bool  Splitter(NkGuiContext &ctx, const char *idStr, const NkRect &handle, bool vertical,
               float32 *value, float32 minV, float32 maxV) noexcept;                                    // .h:179 · .cpp:1495
void  SetUiScale(NkGuiContext &ctx, float32 s) noexcept;                                                // .h:173 · .cpp:1467
float32 Scaled(NkGuiContext &ctx, float32 px) noexcept;                                                 // .h:174 · .cpp:1460
void  PushOverlay(NkGuiContext &ctx) noexcept;  void PopOverlay(NkGuiContext&) noexcept;                // .h:188-189 · .cpp:1546/1550
```

### 4.8 Arbres, sélection, onglets

```cpp
bool CollapsingHeader(NkGuiContext &ctx, const char *label) noexcept;                        // .h:191 · .cpp:1557
bool TreeNode(NkGuiContext &ctx, const char *label) noexcept;                                // .h:194 · .cpp:1582
void TreePop(NkGuiContext &ctx) noexcept;                                                    // .h:195 · .cpp:1615
bool TreeNodeEditable(NkGuiContext &ctx, const char *idStr, char *label, int32 bufSize,
                      bool allowRename = true) noexcept;                                     // .h:200 · .cpp:1634
bool Selectable(NkGuiContext &ctx, const char *label, bool selected) noexcept;               // .h:206 · .cpp:1710
bool SelectableEditable(NkGuiContext &ctx, const char *idStr, char *label, int32 bufSize,
                        bool selected, bool allowRename = true) noexcept;                    // .h:213 · .cpp:1736
bool SelectItem(NkGuiContext &ctx, const char *label) noexcept;                              // .h:218 · .cpp:1803
bool SelectItemEditable(NkGuiContext &ctx, const char *idStr, char *label, int32 bufSize,
                        bool allowRename = true) noexcept;                                   // .h:224 · .cpp:1837
int32 TabBar(NkGuiContext &ctx, const char *id, const char *const *labels, int32 count) noexcept;   // .h:229 · .cpp:1954
int32 TabBarEx(NkGuiContext &ctx, const char *id, const char *const *labels, int32 count,
               const bool *enabled) noexcept;                                                // .h:233 · .cpp:1912
int32 TabBarEditable(NkGuiContext &ctx, const char *id, char *const *labels, int32 count,
                     int32 labelBufSize, const bool *enabled = nullptr,
                     const bool *allowRename = nullptr) noexcept;                            // .h:239 · .cpp:1958
```

Les variantes `*Editable` sont une signature de NKGui (le renommage inline par
double-clic ou Maj+Entrée est intégré au socle, pas laissé à l'application). Elles
prennent toutes un **`idStr` STABLE** distinct du libellé, précisément parce que
le libellé change pendant l'édition (`NkGuiWidgets.h:198`, `209-210`, `221-222`).

### 4.9 Panneaux, fenêtres, docking

```cpp
bool BeginPanel(NkGuiContext &ctx, const char *title, const NkRect &r) noexcept;             // .h:245 · .cpp:2047
void EndPanel(NkGuiContext &ctx) noexcept;                                                   // .h:246 · .cpp:2076

void SetNextWindowPos(NkGuiContext &ctx, float32 x, float32 y) noexcept;                     // .h:259 · .cpp:2259
void SetNextWindowSize(NkGuiContext &ctx, float32 w, float32 h) noexcept;                    // .h:260 · .cpp:2264
bool Begin(NkGuiContext &ctx, const char *title, bool *open = nullptr,
           NkGuiWindowFlags flags = NkGuiWindowFlags::None) noexcept;                        // .h:261 · .cpp:2269
void EndWindow(NkGuiContext &ctx) noexcept;                                                  // .h:263 · .cpp:2545

void  DockSpace(NkGuiContext &ctx, const char *idStr, const NkRect &rect) noexcept;          // .h:271 · .cpp:3173
void  DockBuilderDock(NkGuiContext &ctx, const char *windowTitle, int32 zone) noexcept;      // .h:274 · .cpp:3211
void  DockBuilderDockTab(NkGuiContext &ctx, const char *windowTitle, const char *targetTitle) noexcept; // .h:276 · .cpp:3275
bool  DockFocusWindow(NkGuiContext &ctx, const char *windowTitle) noexcept;                  // .h:280 · .cpp:3298
bool  DockIsWindowDocked(NkGuiContext &ctx, const char *windowTitle) noexcept;               // .h:283 · .cpp:3319
int32 DockWindowNode(NkGuiContext &ctx, const char *windowTitle) noexcept;                   // .h:285 · .cpp:3326
void  DockDetachWindow(NkGuiContext &ctx, const char *windowTitle) noexcept;                 // .h:288 · .cpp:3334
void  DockWindowHideSingleTab(NkGuiContext &ctx, const char *windowTitle, bool hide) noexcept; // .h:292 · .cpp:3338
void  DockSpaceOverViewport(NkGuiContext &ctx, float32 topMargin = 0.f) noexcept;            // .h:297 · .cpp:3397
void  DockWindowIntoWindow(NkGuiContext &ctx, const char *hostTitle, const char *winTitle) noexcept; // .h:303 · .cpp:3355
int32 DockTabAddRequest(NkGuiContext &ctx) noexcept;                                         // .h:305 · .cpp:3375
void  DockAddTab(NkGuiContext &ctx, const char *windowTitle, int32 node) noexcept;           // .h:306 · .cpp:3379
```

`NkGuiWindowFlags` (`NkGuiTypes.h:245-252`) : `NoResize`, `NoMove`, `NoCollapse`,
`NoTitleBar`, `NoClose`.
Zones de `DockBuilderDock` (`NkGuiWidgets.h:273`) : *« 0 = onglet, 1 = gauche,
2 = droite, 3 = haut, 4 = bas (sur la 1re feuille) »*.

### 4.10 Zones défilables et tables

```cpp
bool BeginChild(NkGuiContext &ctx, const char *idStr, const NkRect &rect, bool border = true,
                bool horizontal = false) noexcept;                                           // .h:315 · .cpp:3631
void EndChild(NkGuiContext &ctx) noexcept;                                                   // .h:317 · .cpp:3641
bool BeginListBox(NkGuiContext &ctx, const char *idStr, const NkRect &rect) noexcept;        // .h:319 · .cpp:3645
void EndListBox(NkGuiContext &ctx) noexcept;                                                 // .h:320 · .cpp:3649

bool BeginTable(NkGuiContext &ctx, const char *idStr, int32 columns,
                NkGuiTableFlags flags = NkGuiTableFlags::Borders) noexcept;                  // .h:339 · .cpp:3700
void TableSetupColumn(NkGuiContext &ctx, const char *label, float32 width = 0.f) noexcept;   // .h:343 · .cpp:3733
void TableHeadersRow(NkGuiContext &ctx) noexcept;                                            // .h:345 · .cpp:3741
void TableNextRow(NkGuiContext &ctx, float32 minHeight = 0.f) noexcept;                      // .h:347 · .cpp:3827
bool TableNextColumn(NkGuiContext &ctx) noexcept;                                            // .h:351 · .cpp:3871
bool TableSetColumnIndex(NkGuiContext &ctx, int32 n) noexcept;                               // .h:353 · .cpp:3854
void EndTable(NkGuiContext &ctx) noexcept;                                                   // .h:354 · .cpp:3882
bool TableGetSortColumn(NkGuiContext &ctx, int32 *outCol, bool *outAscending) noexcept;      // .h:358 · .cpp:3938
bool TableCellText(NkGuiContext &ctx, const char *idStr, char *buf, int32 bufSize,
                   bool editable = false) noexcept;                                          // .h:364 · .cpp:4537
```

`NkGuiTableFlags` (`NkGuiTypes.h:132-140`) : `Borders`, `RowBg`, `Resizable`
(séparateurs internes, redistribue), `Header`, `ResizableOuter` (poignée au bord
droit → largeur globale, opt-in), `Sortable` (opt-in).
Limite de conception : `NkGuiTableMaxCols = 16` (`NkGuiContext.h:120`), et **une
seule table active à la fois** — *« pas d'imbrication v1 »* (`NkGuiContext.h:397`).

L'idiome complet est donné en commentaire dans le header (`NkGuiWidgets.h:323-334`) :

```cpp
if (BeginTable(ctx,"t",3, Borders|RowBg|Resizable)) {
    TableSetupColumn(ctx,"Nom",   0);     // 0 = colonne étirable
    TableSetupColumn(ctx,"Type", 90);     // largeur fixe px
    TableSetupColumn(ctx,"Taille",70);
    TableHeadersRow(ctx);
    for (…) { TableNextRow(ctx);
              TableNextColumn(ctx); Text(ctx,nom);
              TableNextColumn(ctx); Text(ctx,type);
              TableNextColumn(ctx); Text(ctx,taille); }
    EndTable(ctx);
}
```

### 4.11 Popups, combos, menus, tooltips

```cpp
bool BeginCombo(NkGuiContext &ctx, const char *label, const char *preview, int32 itemCount,
                float32 heightOverride = 0.f) noexcept;                                      // .h:373 · .cpp:4640
void EndCombo(NkGuiContext &ctx) noexcept;                                                   // .h:375 · .cpp:4735
void EndPopup(NkGuiContext &ctx) noexcept;                                                   // .h:378 · .cpp:4631

bool BeginMenuBar(NkGuiContext &ctx, const NkRect &rect) noexcept;                           // .h:382 · .cpp:4759
void EndMenuBar(NkGuiContext &ctx) noexcept;                                                 // .h:383 · .cpp:4767
bool BeginMenu(NkGuiContext &ctx, const char *label) noexcept;                               // .h:388 · .cpp:4773
void EndMenu(NkGuiContext &ctx) noexcept;                                                    // .h:389 · .cpp:4865
bool MenuItem(NkGuiContext &ctx, const char *label, const char *shortcut = nullptr,
              bool enabled = true) noexcept;                                                 // .h:392 · .cpp:4875
bool BeginPopupMenu(NkGuiContext &ctx, const char *idStr) noexcept;                          // .h:397 · .cpp:4918
void EndPopupMenu(NkGuiContext &ctx) noexcept;                                               // .h:398 · .cpp:4938

void SetTooltip(NkGuiContext &ctx, const char *text) noexcept;                               // .h:402 · .cpp:4946
```

Usage documenté du tooltip (`NkGuiWidgets.h:400-401`) :

> *« À appeler quand le dernier widget est survolé :
> `if (ctx.IsItemHovered()) SetTooltip(ctx, "…");` »*

Menu contextuel (`NkGuiWidgets.h:394-396`) : l'application appelle
`ctx.OpenPopupAt(ctx.GetId(idStr), mousePos)` au clic droit, puis dessine entre
`BeginPopupMenu` / `EndPopupMenu`.

### 4.12 API du contexte utilisée comme un widget

Ces méthodes de `NkGuiContext` s'emploient au milieu des widgets :

```cpp
void  PushId(const char *s) / PushId(const void *p) / PopId();                          // .h:448-450
NkGuiId GetId(const char *s) const;                                                      // .h:451
void  BeginDisabled(bool disabled = true); void EndDisabled(); bool IsDisabled() const;  // .h:482-487
void  BeginSelectList(const char *id, bool *mask, int32 count, NkGuiSelectFlags flags);  // .h:477
void  ApplySelectClick(int32 idx);  void EndSelectList();                                // .h:478-479
void  OpenPopup(NkGuiId) / OpenPopupAt(NkGuiId, NkVec2) / ClosePopup() / OpenPopupLevel(NkGuiId, int32);
bool  IsPopupOpen(NkGuiId) const;                                                        // .h:492-520
bool  IsHovered(const NkRect &r) const;  bool IsItemHovered() const;                     // .h:523-528
bool  ItemHoverable(const NkRect &r, NkGuiId id);                                        // .h:535
bool  ButtonBehavior(…);                                                                 // .h:541-543
NkString GetClipboard() const;  void SetClipboard(const char *t);                        // .h:282-292
```

---

## 5. Raccordement au reste du moteur

### 5.1 Vue d'ensemble des couches

```
   Application (NKGuiDemo, NKCode, NK3DModeler…)
        │  crée NkWindow, pompe NKEvent → remplit ctx.input
        │  déclare ses widgets entre BeginFrame / EndFrame
        ▼
   NKGui  (NkGuiContext → NkGuiDrawList : vtx + idx + cmds)
        │  ne connaît AUCUN GPU
        ▼
   Backend (au choix)
     ├─ NkGuiCanvasBackend   (header-only, dans NKCanvas/UI/)  → NkIRenderer2D
     └─ NkGuiRHIBackend      (Integrations/NKGui/)             → NKRHI (DX11/12/VK/GL)
        ▼
   NKCanvas (NkBatchRenderer2D → SubmitBatches)  |  NKRHI
        ▼
   GPU
```

**Deux backends, deux publics** — doctrine du dépôt
(`Integrations\NKGui\NKGuiIntegration.jenga:5-8`) : NKCanvas pour l'IDE (NKCode),
NKRHI pour les applications 2D/3D (animation, moteur de jeu).
**La démo `NKGuiDemo` utilise le backend Canvas** — c'est explicite ligne 25 de
son `main.cpp` :

```cpp
#include "NKCanvas/UI/NkGuiCanvasBackend.h" // backend NKGui->NKCanvas (lib reutilisable)
```

### 5.2 `NkGuiCanvasBackend` — le pont NKGui → NKCanvas

`Kernel\Runtime\NKCanvas\src\NKCanvas\UI\NkGuiCanvasBackend.h`. **Header-only**,
raison expliquée en ligne 8-9 :

> *« HEADER-ONLY : NKCanvas ne le compile pas (pas de .cpp) ; seuls les
> consommateurs (qui dépendent déjà de NKGui ET NKCanvas) l'incluent. »*

API complète :

```cpp
bool Init(nkentseu::renderer::NkIRenderer2D *renderer);                              // :34
bool UploadFontGray8(uint32 texId, const uint8 *gray, int32 w, int32 h);             // :41
bool UploadImageRGBA(uint32 texId, const uint8 *rgba, int32 w, int32 h);             // :94
void Submit(const nkentseu::nkgui::NkGuiDrawList &dl, uint32 fbW, uint32 fbH);       // :120
```

**Conversion du vertex** (`NkGuiCanvasBackend.h:126-137`) — c'est là qu'on voit
concrètement le dépaquetage de la couleur :

```cpp
mScratch.Resize(dl.vtx.Size());
for (uint32 i = 0; i < dl.vtx.Size(); ++i) {
    const nkgui::NkGuiVertex &s = dl.vtx[i];
    renderer::NkVertex2D &d = mScratch[i];
    d.x = s.pos.x;  d.y = s.pos.y;
    d.u = s.uv.x;   d.v = s.uv.y;
    d.r = static_cast<uint8>( s.col        & 0xFFu);
    d.g = static_cast<uint8>((s.col >>  8) & 0xFFu);
    d.b = static_cast<uint8>((s.col >> 16) & 0xFFu);
    d.a = static_cast<uint8>((s.col >> 24) & 0xFFu);
}
```

**Boucle sur les commandes** (`:139-194`) : détection du clip par la sentinelle
`1e8`, résolution de la texture (**polices d'abord, images ensuite**), puis
soumission à `mRenderer->DrawVertices(...)`.

**LE piège majeur du pont**, cité mot pour mot (`NkGuiCanvasBackend.h:176-179`) :

> ```
> // Ne soumet que le SOUS-ENSEMBLE de vertices reference par cette
> // commande (indices rebases). Indispensable : passer tout le buffer
> // depasse kMaxVertices (65536) des qu'un draw list est gros -> crash.
> ```

Code correspondant (`:179-190`) :

```cpp
uint32 lo = 0xFFFFFFFFu, hi = 0u;
for (uint32 k = 0; k < dc.idxCount; ++k) {
    const uint32 v = dl.idx[dc.idxOffset + k];
    if (v < lo) lo = v;
    if (v > hi) hi = v;
}
mIdxTmp.Resize(dc.idxCount);
for (uint32 k = 0; k < dc.idxCount; ++k)
    mIdxTmp[k] = dl.idx[dc.idxOffset + k] - lo;
mRenderer->DrawVertices(mScratch.Data() + lo, hi - lo + 1u, mIdxTmp.Data(), dc.idxCount, tex);
```

*(Note : le commentaire dit 65536 ; `NkBatchRenderer2D.h:39` porte aujourd'hui
`kMaxVertices = 262144`. Le commentaire garde la trace du dimensionnement
d'origine — c'est cette même famille de bug qui a motivé l'agrandissement, §2.10.)*

**Upload de police, et pourquoi on recrée la texture** (`NkGuiCanvasBackend.h:65-67`) :

> ```
> // (Re)créer la texture si elle n'existe pas OU si la taille change (rechargement
> // de police à une autre taille = zoom / DPI). Sinon Update() déborderait l'ancienne taille.
> ```

L'atlas gris 8 bits est étendu en RGBA « blanc + alpha » (`:47-55`) :

```cpp
for (usize i = 0; i < n; ++i) {
    d[i*4u+0u] = 255u; d[i*4u+1u] = 255u; d[i*4u+2u] = 255u;
    d[i*4u+3u] = gray[i];
}
```

Le backend gère **plusieurs polices** par `texId` (`:203-204`) : *« Un atlas de
police par texId (interface, code, ...). Plusieurs polices = plusieurs textures
resolues par Submit selon le texId de la commande. »*

Le destructeur libère les textures via l'allocateur NKMemory (`:24-32`) — **jamais
`delete`** :

```cpp
~NkGuiCanvasBackend() {
    auto &alloc = nkentseu::memory::NkGetDefaultAllocator();
    for (uint32 i = 0; i < mFonts.Size();  ++i) if (mFonts[i].tex)  alloc.Delete(mFonts[i].tex);
    for (uint32 i = 0; i < mImages.Size(); ++i) if (mImages[i].tex) alloc.Delete(mImages[i].tex);
}
```

### 5.3 `NkGuiRHIBackend` — le pont NKGui → NKRHI (alternative 3D)

`Integrations\NKGui\NkGuiRHIBackend.h:43-105`. Contrat d'usage cité tel quel
(`.h:32-42`) :

> ```
> // Backend NKGui -> NKRHI reutilisable.
> //
> //   NkGuiRHIBackend backend;
> //   backend.Init(device, renderPass, api);
> //   // chaque frame, dans une render pass active :
> //   backend.Submit(cmd, drawList, fbW, fbH);   // appele par draw-list
> //   backend.Destroy();
> //
> // texId == 0 est reserve a la texture blanche interne (geometrie unie).
> // Les atlas/police + images sont fournis via UploadTextureGray8/RGBA8, ou
> // une texture GPU externe (ex. viewport 3D offscreen) via RegisterTexture.
> ```

API (`.h:45-59`) :

```cpp
bool Init(NkIDevice* device, NkRenderPassHandle renderPass, NkGraphicsApi api);
void Destroy();
void Submit(NkICommandBuffer* cmd, const NkGuiDrawList& dl, uint32 fbW, uint32 fbH);
bool UploadTextureRGBA8(uint32 texId, const uint8* data, int32 width, int32 height);
bool UploadTextureGray8(uint32 texId, const uint8* data, int32 width, int32 height);
bool RegisterTexture(uint32 texId, NkTextureHandle texture);
bool HasTexture(uint32 texId) const noexcept;
```

Points notables :
- buffers dynamiques (croissance ×1.5, rétrécissement différé de
  `kShrinkDelayFrames = 240` frames, `.cpp:64`) ;
- vertex layout `POSITION(RG32F) / TEXCOORD(RG32F) / COLOR(R32_UINT)` (`.cpp:339-344`) ;
- conversion de scissor : top-left pour DX/VK/Software, **bas-gauche pour OpenGL**
  (`.cpp:514-528`) ;
- destruction de textures **différée** de `kRetireDelayFrames = 8` frames, pour ne
  pas détruire une texture encore référencée par des frames en vol GPU
  (`.cpp:453-475`).

Différence de modèle avec l'ancien NKUI, expliquée en tête de fichier
(`NkGuiRHIBackend.h:8-13`) :

> ```
> // Porte de Integrations/NKUI/NkUIRHIBackend : meme pipeline, memes buffers
> // dynamiques (VBO/IBO/UBO), meme upload texture/atlas (RGBA8 + Gray8 -> RGBA8),
> // meme gestion scissor (top-left DX/VK/SW, bottom-left OpenGL), meme conversion
> // de vertices (couleur RGBA8 packee -> vec4). Difference : NKGui expose UNE
> // draw-list (NkGuiDrawList) avec un clipRect PAR commande (et non des couches
> // avec commande NK_CLIP_RECT comme NKUI) ; le scissor est applique par commande.
> ```

### 5.4 Les polices : `NkGuiFont`

`Kernel\Runtime\NKGui\src\NKGui\Core\NkGuiFont.h:19-69` :

```cpp
struct NKENTSEU_NKGUI_CLASS_EXPORT NkGuiFont {
        NkFontAtlas atlas;          ///< possède la texture + les glyphes
        NkFont *face = nullptr;     ///< face produite (détenue par l'atlas)
        uint32 texId = 0x4E4B4654u; ///< 'NKFT' — id stable pour le backend
        uint8 *pixels = nullptr;    ///< atlas alpha8 (détenu par l'atlas)
        int32 atlasW = 0;
        int32 atlasH = 0;
        bool dirty = false;         ///< à (ré)uploader côté backend

        NkGuiFont() = default;
        NkGuiFont(const NkGuiFont &) = delete;            // NON COPIABLE
        NkGuiFont &operator=(const NkGuiFont &) = delete;

        bool LoadEmbedded(NkEmbeddedFontId id, float32 sizePx, bool extFallback = true) noexcept;
        bool LoadFromFile(const char *path, float32 sizePx, bool extFallback = true) noexcept;

        const NkFont *Face() const noexcept;
        uint32  TexId() const noexcept;
        bool    Valid() const noexcept;
        float32 Ascent() const noexcept;
        float32 Descent() const noexcept;
        float32 LineHeight() const noexcept;
        float32 MeasureWidth(const char *s) const noexcept;
};
```

- `texId` vaut la constante fixe `0x4E4B4654` (« NKFT »), **jamais 0** (0 est
  réservé à la texture blanche interne, cf. §5.3).
- `extFallback=false` sert aux polices **monospace** (code) — commentaire
  (`NkGuiFont.h:34-35`) : *« n'incorpore AUCUN repli externe (broad/CJK/emoji =
  plusieurs milliers de glyphes) […] atlas minuscule => reconstruction RAPIDE au
  zoom »*.
- Replis externes globaux (`NkGuiFont.h:78`) :
  ```cpp
  NKENTSEU_NKGUI_API void NkSetFallbackFontPaths(const char *broad, const char *cjk, const char *emoji) noexcept;
  ```
  À poser **AVANT** de charger les polices (`NkGuiFont.h:76-77`).

**Durée de vie — piège** : `ctx.font` et `ctx.codeFont` (`NkGuiContext.h:141-142`)
sont des **pointeurs bruts non possédés**. L'objet `NkGuiFont` doit survivre à
toute la boucle (la démo le tient dans un `memory::NkUniquePtr`).

### 5.5 L'entrée : NKEvent → `NkGuiInput`

L'application enregistre des callbacks typés puis pompe la file. Extrait réel de
`Applications\NKGuiDemo\src\NKGuiDemo\main.cpp:326-421` (abrégé) :

```cpp
auto &events = NkEvents();
bool running = true;
events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) { running = false; });

events.AddEventCallback<NkMouseMoveEvent>([&](NkMouseMoveEvent *e) {
    ctx.input.mousePos = {static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY())};
});
events.AddEventCallback<NkMouseButtonPressEvent>([&](NkMouseButtonPressEvent *e) {
    if (e->GetButton() == NkMouseButton::NK_MB_LEFT)  ctx.input.mouseDown[0] = true;
    if (e->GetButton() == NkMouseButton::NK_MB_RIGHT) ctx.input.mouseDown[1] = true;
    ctx.input.ctrlDown  = e->GetModifiers().ctrl;
    ctx.input.shiftDown = e->GetModifiers().shift;
    ctx.input.altDown   = e->GetModifiers().alt;
});
events.AddEventCallback<NkMouseButtonReleaseEvent>([&](NkMouseButtonReleaseEvent *e) {
    if (e->GetButton() == NkMouseButton::NK_MB_LEFT)  ctx.input.mouseDown[0] = false;
    if (e->GetButton() == NkMouseButton::NK_MB_RIGHT) ctx.input.mouseDown[1] = false;
});
events.AddEventCallback<NkMouseWheelVerticalEvent>([&](NkMouseWheelVerticalEvent *e) {
    ctx.input.wheel += static_cast<float32>(e->GetDeltaY()); // >0 = haut ; consommé en EndFrame
    const auto m = e->GetModifiers();                        // modificateurs au moment de la molette
    ctx.input.ctrlDown = m.ctrl; ctx.input.shiftDown = m.shift; ctx.input.altDown = m.alt;
});
events.AddEventCallback<NkMouseWheelHorizontalEvent>([&](NkMouseWheelHorizontalEvent *e) {
    ctx.input.wheelH += static_cast<float32>(e->GetDeltaX());
});
// Double-clic détecté par l'OS : NKEvent l'émet comme événement DÉDIÉ (le 2e
// clic n'arrive PAS comme un mouseDown normal) → on l'injecte explicitement.
events.AddEventCallback<NkMouseDoubleClickEvent>([&](NkMouseDoubleClickEvent *e) {
    ctx.input.mousePos = {static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY())};
    if (e->GetButton() == NkMouseButton::NK_MB_LEFT) ctx.input.SetDoubleClick(0);
});
// Saisie texte + touches d'édition (pour InputText). On pose l'état ENFONCÉ
// (press/release) pour permettre la répétition au maintien.
events.AddEventCallback<NkTextInputEvent>([&](NkTextInputEvent *e) { ctx.input.PushChar(e->GetCodepoint()); });

auto setKey = [&](NkKey k, bool down) {
    switch (k) {
        case NkKey::NK_LEFT:   ctx.input.SetKey(NkGuiKey::Left, down);      break;
        case NkKey::NK_RIGHT:  ctx.input.SetKey(NkGuiKey::Right, down);     break;
        case NkKey::NK_UP:     ctx.input.SetKey(NkGuiKey::Up, down);        break;
        case NkKey::NK_DOWN:   ctx.input.SetKey(NkGuiKey::Down, down);      break;
        case NkKey::NK_HOME:   ctx.input.SetKey(NkGuiKey::Home, down);      break;
        case NkKey::NK_END:    ctx.input.SetKey(NkGuiKey::End, down);       break;
        case NkKey::NK_BACK:   ctx.input.SetKey(NkGuiKey::Backspace, down); break;
        case NkKey::NK_DELETE: ctx.input.SetKey(NkGuiKey::Delete, down);    break;
        case NkKey::NK_ENTER:  ctx.input.SetKey(NkGuiKey::Enter, down);     break;
        case NkKey::NK_ESCAPE: ctx.input.SetKey(NkGuiKey::Escape, down);    break;
        default: break;
    }
};
events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent *e)   { setKey(e->GetKey(), true);  … });
events.AddEventCallback<NkKeyReleaseEvent>([&](NkKeyReleaseEvent *e){ setKey(e->GetKey(), false); … });
```

Et dans la boucle, le pompage lui-même ne fait **rien** — ce sont les callbacks qui
travaillent (`main.cpp:438-440`) :

```cpp
while (NkEvent *ev = NkEvents().PollEvent()) { (void)ev; }
```

### 5.6 SQUELETTE MINIMAL D'UNE APPLICATION NKGui

Reconstitué fidèlement depuis `Applications\NKGuiDemo\src\NKGuiDemo\main.cpp`
(lignes 240-1067), réduit au strict nécessaire :

```cpp
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkGraphicsApi.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKTime/NkClock.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKGui/NKGui.h"
#include "NKCanvas/UI/NkGuiCanvasBackend.h"

using namespace nkentseu;
using namespace nkentseu::nkgui;
using namespace nkentseu::renderer;

NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName    = "MonApp";
    d.appVersion = "0.1.0";
    return d;
})());

int nkmain(const NkEntryState &state) {
    (void)state;

    // ── 1) FENÊTRE OS (NKWindow) ────────────────────────────────────────────
    NkWindow window;
    NkWindowConfig cfg;
    cfg.title = "Mon app NKGui";
    cfg.width = 1100; cfg.height = 800;
    cfg.centered = true; cfg.resizable = true;
    if (!window.Create(cfg)) return -1;

    // ── 2) CONTEXTE GRAPHIQUE + CIBLE DE RENDU (NKCanvas) ───────────────────
    NkContextDesc desc;
    desc.api = NkGraphicsApi::NK_GFX_API_AUTO;
    if (desc.api == NkGraphicsApi::NK_GFX_API_AUTO) {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
        desc.api = NkGraphicsApi::NK_GFX_API_DX11;
#else
        desc.api = NkGraphicsApi::NK_GFX_API_OPENGL;
#endif
    }
    auto target = memory::NkMakeUnique<NkRenderWindow>(window, desc);
    if (!target || !target->IsValid()) return -1;

    // ── 3) CONTEXTE NKGui ───────────────────────────────────────────────────
    auto ctxPtr = memory::NkMakeUnique<NkGuiContext>();
    if (!ctxPtr) return -1;
    NkGuiContext &ctx = *ctxPtr;
    ctx.Init(static_cast<int32>(cfg.width), static_cast<int32>(cfg.height));
    SetCurrentContext(&ctx);

    // ── 4) BACKEND (NKGui → NKCanvas) ───────────────────────────────────────
    renderer::NkGuiCanvasBackend backend;
    if (!backend.Init(target->GetRenderer())) return -1;

    // ── 5) POLICE : charger PUIS uploader l'atlas ───────────────────────────
    auto fontPtr = memory::NkMakeUnique<NkGuiFont>();   // DOIT survivre à la boucle
    if (!fontPtr->LoadEmbedded(NkEmbeddedFontId::DroidSans, 18.f)) {
        fontPtr->LoadEmbedded(NkEmbeddedFontId::ProggyClean, 16.f);
    }
    ctx.font = fontPtr.Get();                            // pointeur brut NON possédé
    if (fontPtr->Valid()) {
        backend.UploadFontGray8(fontPtr->TexId(), fontPtr->pixels, fontPtr->atlasW, fontPtr->atlasH);
    }

    // ── 6) GLUE D'ENTRÉE (callbacks NKEvent → ctx.input) — cf. §5.5 ─────────
    auto &events = NkEvents();
    bool running = true;
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) { running = false; });
    events.AddEventCallback<NkMouseMoveEvent>([&](NkMouseMoveEvent *e) {
        ctx.input.mousePos = {static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY())};
    });
    events.AddEventCallback<NkMouseButtonPressEvent>([&](NkMouseButtonPressEvent *e) {
        if (e->GetButton() == NkMouseButton::NK_MB_LEFT) ctx.input.mouseDown[0] = true;
    });
    events.AddEventCallback<NkMouseButtonReleaseEvent>([&](NkMouseButtonReleaseEvent *e) {
        if (e->GetButton() == NkMouseButton::NK_MB_LEFT) ctx.input.mouseDown[0] = false;
    });
    // (+ molette, texte, touches — voir §5.5)

    NkClock clock;
    uint32 lastW = 0, lastH = 0;

    // ── 7) BOUCLE PRINCIPALE ────────────────────────────────────────────────
    while (running && window.IsOpen()) {
        float32 dt = clock.Tick().delta;
        if (dt <= 0.f)  dt = 1.f / 60.f;
        if (dt >  0.1f) dt = 0.1f;                       // borne anti-saut

        // a) ÉVÉNEMENTS — AVANT BeginFrame (BeginFrame appelle input.NewFrame())
        while (NkEvent *ev = NkEvents().PollEvent()) { (void)ev; }
        if (!running) break;

        // b) Synchroniser la SWAPCHAIN sur la taille de la FENÊTRE (cf. piège 6.3)
        const math::NkVec2u wsz = target->GetWindow().GetSize();
        if (wsz.x > 0 && wsz.y > 0 && (wsz.x != lastW || wsz.y != lastH)) {
            target->OnResize(wsz.x, wsz.y);
            lastW = wsz.x; lastH = wsz.y;
        }
        const math::NkVec2u sz = target->GetSize();
        if (sz.x > 0 && sz.y > 0) { ctx.viewW = (int32)sz.x; ctx.viewH = (int32)sz.y; }

        // c) DÉBUT DE FRAME UI
        ctx.BeginFrame(dt);
        NkGuiDrawList &dl = ctx.dl;

        // d) LES WIDGETS
        ctx.BeginLayout({20.f, 20.f, 400.f, 300.f});     // ouvrir une région de layout
        Text(ctx, "Bonjour NKGui");
        if (Button(ctx, "Cliquez-moi")) {
            // réaction au clic
        }

        // e) FIN DE FRAME UI
        ctx.EndFrame();

        // f) PRÉSENTATION : Clear ouvre la frame, Display la ferme et présente
        target->Clear();
        backend.Submit(dl,            sz.x, sz.y);        // couche principale
        backend.Submit(ctx.dlOverlay, sz.x, sz.y);        // popups/overlay AU-DESSUS
        target->Display();
    }

    SetCurrentContext(nullptr);
    ctx.Shutdown();
    return 0;
}
```

**Présentation / swap / clear** — il n'y a **rien à faire à la main** :
`NkRenderWindow` encapsule tout. Commentaire de la démo NKCanvas
(`Applications\Sandbox\src\DemoNkentseu\NkCanvas\NkCanvasDemo.cpp:6-7`) :

> *« NkRenderWindow possede le cycle de frame -> Clear() ouvre+efface, on dessine,
> Display() termine+presente (pas de Begin/End/SetView/SetViewport a la main) »*

Lignes réelles de la démo NKGui (`main.cpp:1045-1048`) :

```cpp
target->Clear();
backend.Submit(dl, sz.x, sz.y);            // couche principale
backend.Submit(ctx.dlOverlay, sz.x, sz.y); // couche popups/overlay (au-dessus)
target->Display(); // Capture d'ecran (F12) : APRES Display() — la frame presentee. Self-capture
```

> **Deux `Submit`, pas un.** `ctx.dl` puis `ctx.dlOverlay`. Oublier le second =
> popups, combos, menus et tooltips invisibles.

### 5.7 Ce que la démo utilise réellement (repères de lecture)

`Applications\NKGuiDemo\src\NKGuiDemo\main.cpp` couvre pratiquement toute l'API.
Repères utiles :

| Fonctionnalité | Lignes |
|---|---|
| `ctx.Init` / `SetCurrentContext` | 270, 272, 1064 |
| `BeginPanel`/`EndPanel` | 519, 528, 534, 641, 645, 733, 738, 800 |
| `Text` / `TextAt` | 477, 520-527, 535, 606, 626, 636, 671, 687, 699 |
| `Button` / `RepeatButton` / `ImageButton` | 536, 543, 550, 634 |
| `Checkbox` / `CheckboxTristate` / `CheckBox3` | 521-527, 558, 562-564, 708 |
| `SliderFloat` / `DragFloat` / `InputInt` | 568-575 |
| `InputText` / `InputTextEx` / `InputTextMultiline` | 580-590, 849 |
| `BeginCombo`/`Selectable`/`EndCombo` | 596-603 |
| `ProgressBar` / `PlotLines` / `PlotHistogram` | 611, 614, 616 |
| `ColorEdit4` | 621, 623 |
| `TreeNodeEditable` / `SelectableEditable` / `TreePop` | 653-668, 713-729 |
| `TabBarEditable` | 678 |
| `BeginSelectList`/`SelectItemEditable`/`EndSelectList` | 691-694 |
| Table complète (`BeginTable`…`EndTable`) | 745-798 |
| `BeginListBox`/`EndListBox` · `BeginChild`/`EndChild` | 832-839, 857-865 |
| `ctx.OpenPopupAt` / `BeginPopupMenu` / `MenuItem` / `BeginMenu` | 871-881, 480-511 |
| `DockBuilderDock` / `DockSpace` / `DockTabAddRequest` / `DockAddTab` | 888-905 |
| `Begin`/`EndWindow` / `SetNextWindowSize` | 907-1017 |
| Conteneurs (`BeginRow`, `BeginFlow`, `BeginStack`, `BeginHBox`, `BeginGrid`) | 956-1007 |
| `SetUiScale` (F9 = cycle DPI, + rechargement de police + ré-upload) | 461-466 |
| Hook `ctx.styleFn` (`DemoStyle`) | 271, 193-238 |
| `ctx.BeginFrame` / `ctx.EndFrame` / `backend.Submit` | 468, 1043, 1046-1047 |

---

## 6. Pièges et invariants

> Section la plus utile pour un débutant : chacun de ces commentaires est la trace
> d'un bug **réellement** rencontré dans le dépôt. Citations mot pour mot.

### 6.1 Durée de vie et allocation mémoire

**(a) Ne jamais mélanger `new` et l'allocateur NKMemory** —
`Kernel\Runtime\NKCanvas\src\NKCanvas\Renderer\Core\NkRenderer2DFactory.cpp:41-46`
(répété en `.cpp:112-113` et `NkContextFactory.cpp:363-365`) :

> ```
> // IMPORTANT : on alloue via l'allocateur NKMemory (NkGetDefaultAllocator)
> // et NON via `new`. Raison : le NkUniquePtr<NkIRenderer2D> (CreateUnique)
> // desalloue via NkDefaultDelete -> ce MEME allocateur (-> _aligned_free).
> // Allouer avec `new` (malloc) puis liberer avec _aligned_free = mismatch
> // d'allocateur -> heap corruption c0000374 au free du renderer (shutdown).
> // cf BugReports/NKCanvas/heap-c0000374-renderer-alloc-mismatch.md.
> ```
> et, en règle générale (`NkContextFactory.h:39`) : *« RÈGLE : ne JAMAIS faire
> `delete` (objet alloue par NKMemory) »*.

**(b) La façade ne possède pas son backend** — `NkRenderer2D.h:31-35`, déjà cité
en §2.1. `NkRenderer2D` est *« juste une projection »* ; sa durée de vie suit
celle du `NkRenderTarget`.

**(c) Buffer scratch détruit avant le flush** — le meilleur exemple pédagogique du
dépôt, `Kernel\Runtime\NKCanvas\src\NKCanvas\Renderer\Shapes\NkShape.cpp:62-71` :

> ```
> // On triangule en fan COTE NkShape (1 triangle = (p0, p_i, p_{i+1}))
> // et on emet directement en NK_TRIANGLES. Pourquoi pas NK_TRIANGLE_FAN
> // (n vertices, plus economique) ? Car NkRenderWindow::Draw raw fait
> // une expansion TRIANGLE_FAN -> TRIANGLES dans un buffer scratch
> // local au switch ; si le backend batch lazyement ce buffer (et ne
> // recopie pas immediatement), le scratch est detruit avant flush ->
> // donnees garbage / artefacts (visible : « rectangles creux »).
> // En emettant TRIANGLES direct, le buffer `v` de NkShape reste vivant
> // pendant tout l'appel target.Draw, et NkRenderWindow se contente
> // d'un identity-indices submit deterministe.
> ```

**(d) Pointeurs vers la pile dans les surcouches différées** — mandat explicite du
socle, `Kernel\Runtime\NKGui\ROADMAP.md` (« Défaut 2 ») :

> *« `NkComboPending` (NK3DModeler) conserve `const char *const *items` **et**
> `int32 *selected` pour peindre la liste plus tard, hors de la portée qui l'a
> créée. […] **Pointeurs morts** — un tableau de libellés ou une variable de
> sélection *locale* est détruit avant que la liste ne soit peinte. Symptôme : le
> combo s'ouvre, choisir ne change rien (au mieux) ou l'application plante (au
> pire […]). »*
>
> *« **Directive.** Une surcouche différée ne doit **jamais** détenir de pointeur
> vers la pile de l'appelant. Deux voies acceptables : copier les libellés et la
> valeur dans le socle, ou fonctionner par **identifiant** (le socle rend le
> choix, l'appelant l'interroge). La seconde est préférable en mode immédiat. »*

**(e) `ctx.font` / `ctx.codeFont` sont des pointeurs bruts non possédés**
(`NkGuiContext.h:141-142`). Idem pour le renderer injecté dans un shell éditeur —
`Integrations\NKGui\NkEditorRHIRenderer.h:46` :

> ```
> // Backend de rendu NKRHI injectable dans NkEditorShell (cfg.renderer).
> // Doit survivre au shell (le shell ne possede pas un renderer injecte).
> ```

### 6.2 Ordre d'appel

**(a) `événements → BeginFrame → widgets → EndFrame → Submit`.** `BeginFrame`
appelle `input.NewFrame()` (`NkGuiContext.cpp:41`) : poser l'entrée après, c'est
travailler avec les transitions de la frame précédente. À noter, le contraste avec
l'ancien NKUI, où l'ordre était *inversé* et documenté ainsi
(`Applications\Sandbox\src\DemoNkentseu\NkUICanvas\NkUICanvasDemo.cpp:268-270`) :

> ```
> // Ordre input CORRECT : BeginFrame (vide les deltas clic/release) PUIS les
> // events (re-remplissent pos/boutons/clics) -> les widgets voient le clic.
> ```
> **Ne pas transposer cette règle à NKGui** : la démo NKGui pompe les événements
> **avant** `BeginFrame` (`main.cpp:438` vs `468`), parce que c'est `BeginFrame`
> qui fait le travail de `NewFrame()`.

**(b) `End*` UNIQUEMENT si `Begin*` a retourné `true`** (`NkGuiWidgets.h:253-254`).

**(c) `DockSpace` AVANT les `Begin` des fenêtres dockables** (`NkGuiWidgets.h:266-267`).

**(d) `TableSetupColumn` × N juste après `BeginTable`, avant `TableHeadersRow`**
(`NkGuiWidgets.h:341-342`) ; `TableGetSortColumn` **après** `TableHeadersRow`
(`NkGuiWidgets.h:356`).

**(e) Interactions AVANT dessin dans `Begin` de fenêtre** (`NkGuiWidgets.cpp:2396-2397`) :

> ```
> // ── PHASE 1 : INTERACTIONS — mettent à jour `wr` AVANT le dessin (sinon le
> //    contenu, dessiné à la nouvelle position, « court après » le chrome). ──
> ```

**(f) Clip du popup AVANT le fond** (`NkGuiWidgets.cpp:4613-4619`) :

> ```
> // Clip EN PREMIER, intersect=false : un popup est une surface de PREMIER
> // PLAN ; son clip ne doit PAS être réduit par celui du menu parent (sinon
> // un sous-menu, qui sort du parent vers la droite, serait clippé à zéro).
> // Doit précéder le dessin du fond, sinon le fond reste sous le clip parent.
> ```

**(g) Ne pas double-initialiser le renderer** —
`Kernel\Runtime\NKCanvas\src\NKCanvas\Renderer\Targets\NkRenderWindow.cpp:95-98` :

> ```
> // NB : NE PAS appeler mRenderer->Initialize(mContext) ici !
> // NkRenderer2DFactory::Create l'a deja fait (cf. NkRenderer2DFactory.cpp:83).
> // Double-init -> NkOpenGLRenderer2D::Initialize log "Already initialized"
> // ERR et corrompt l'etat (rectangles creux observes en runtime).
> ```

**(h) `Begin()` du batcher n'est pas ré-entrant** (`NkBatchRenderer2D.cpp:27-31`) :

```cpp
bool NkBatchRenderer2D::Begin() {
    if (mInFrame) {
        logger.Warnf("[NkBatch] Begin() called twice");
        return false;
    }
```

**(i) Terminer la frame avant de recréer la swapchain** (`NkRenderWindow.cpp:353-358`) :

```cpp
// Si une frame est en cours, on la termine avant la recreation
// (sinon submit sur swapchain detruite -> UB sur Vulkan/DX12).
if (mFrameOpen && mRenderer) { mRenderer->End(); mFrameOpen = false; }
```
(idem `RecreateSurface`, lignes 372-375 : *« ne pas presenter sur une surface morte »*)

**(j) Capture d'écran APRÈS `Display()`** (`main.cpp:1048`) :

```cpp
target->Display(); // Capture d'ecran (F12) : APRES Display() — la frame presentee. Self-capture
```

**(k) Hook pré-UI entre `Begin()` et la passe backbuffer**
(`Integrations\NKGui\NkEditorRHIRenderer.h:155-160`) :

> ```
> // Hook pre-UI (viewport 3D offscreen) : rendu sur le MEME cmd, APRES
> // Begin() (sinon Reset() efface ses commandes) et AVANT la passe
> // backbuffer (ses BeginCapture/EndCapture ouvrent leurs propres
> // passes ; pas de render pass imbriquee avec celle du backbuffer).
> ```

### 6.3 Couches, occlusion et blocage de clics

**(a) La règle centrale, tirée d'un bug réel** —
`Kernel\Runtime\NKGui\ROADMAP.md` (« Défaut 1 », point 4) :

> *« **ma propre correction du point 3** : en armant le blocage, j'ai neutralisé
> la liste elle-même, peinte après. D'où la règle qui manquait, à inscrire dans le
> socle : **le blocage protège ce qui est dessous, jamais ce qui est au-dessus.** »*
>
> et la directive : *« Dans un socle correct, l'ordre de peinture définit la
> priorité d'interaction, sans rien à armer. Ce qui est peint plus tard capte le
> clic en premier ; le reste en découle. »*

Les quatre bugs de la même famille, listés le 4 août 2026 (même fichier) :

> *« 1. le menu d'en-tête de groupe peint sur la couche du panneau : clics
> inopérants et menu qui ne se refermait pas ;
> 2. le panneau de l'édition proportionnelle laissait passer ses clics ;
> 3. la liste déroulante du format de sortie : « je ne peux choisir aucun format »
> — le panneau Propriétés ne consultait pas `UiBlocks`, alors qu'il porte le plus
> de listes de toute l'application ; […] »*

**(b) Toujours passer par `InputHits`/`ClickIn`, jamais par un test brut**
(`NkGuiContext.h:202-203`, déjà cité en §3.8).

**(c) Couches conseillées** (`NkGuiContext.h:174`) :
*« 0 fond/panneaux · 50 menus/palettes/popovers · 100 modals · 200 debug »*.

**(d) L'anti-gel d'`activeId`** (`NkGuiContext.cpp:125-128`) — quand tout devient
inerte, c'est presque toujours ça :

> ```
> // ANTI-GEL : aucun glissement légitime ne conserve activeId bouton RELÂCHÉ.
> // Si le widget détenant activeId a disparu (hôte redevenu flottant, onglet
> // caché, fenêtre fermée…) il ne libère jamais activeId et l'occlusion bloque
> // TOUTE interaction. Souris haute + activeId encore posé ⇒ on libère d'office.
> ```

**(e) Un widget dessiné après capte le clic** — commentaire de la démo
(`Applications\NKGuiDemo\src\NKGuiDemo\main.cpp:806-808`) :

> ```
> // Soumise APRÈS les boutons → au-dessus. ItemHoverable résout le z-ordre :
> // au-dessus d'un bouton, c'est la boîte qui capture, pas le bouton dessous.
> ```

**(f) Cohérence fond/contenu dans les onglets dockés**
(`NkGuiWidgets.cpp:2979-2981`) :

> ```
> // dockDL = la draw-list où l'on vient de peindre le FOND : le contenu de
> // l'onglet DOIT s'y dessiner (sinon fond et texte divergent de couche).
> ```

**(g) L'« hôte fantôme »** — bug reproduit en vidéo (`NkGuiWidgets.cpp:2683-2686`) :

> ```
> // TRANSFERT D'HÔTE : si on désancre la fenêtre qui EST l'hôte de son arbre, le
> // rôle d'hôte (hostRoot + hostRect) passe à une autre fenêtre encore dans l'arbre.
> // Sinon l'ancien hôte reste « hôte fantôme » (rend le chrome mais dockNode=-1 →
> // son contenu part en couche de fond, invisible). Cause du bug vidéo (Calque C).
> ```

**(h) Ouverture de menu au PRESS, pas au clic complet** (`NkGuiWidgets.cpp:4806-4812`) :

> ```
> // Ouverture au PRESS via test geometrique direct : ne depend PAS du
> // survol resolu (hotIdPrev) de la frame precedente. Sinon un clic sur
> // un titre jamais survole avant n'ouvrait pas le menu au 1er coup.
> // On N'utilise PAS `clicked` (relachement) ici : sinon press ouvrirait
> // et release refermerait aussitot (double bascule).
> ```

### 6.4 Rendu, buffers, resize

**(a) Débordement de buffer GPU** — deux endroits, même famille de bug :
`NkGuiCanvasBackend.h:176-179` (rebasage d'indices, §5.2) et
`NkBatchRenderer2D.h:36-38` + `.cpp:80-82` (agrandissement + troncature, §2.10).

**(b) Détecter le resize sur la taille de la FENÊTRE, pas de la swapchain**
(`Applications\NKGuiDemo\src\NKGuiDemo\main.cpp:444-448`) :

> ```
> // Synchroniser la SWAPCHAIN à la taille de la FENÊTRE. target->GetSize()
> // renvoie la taille de la swapchain (qui ne change QU'À OnResize) — s'en
> // servir pour détecter le resize était faux : la swapchain restait figée à
> // sa taille de création (rendu basse-résolution étiré). On pilote OnResize
> // depuis la taille fenêtre (inclut la 1re frame → sync initiale).
> ```

**(c) Minimisation de fenêtre — course réelle**
(`Integrations\NKGui\NkEditorRHIRenderer.h:123-130`) :

> ```
> // GARDE ANTI-MINIMISATION : une fenetre en cours de reduction
> // glisse son rect placeholder (~160x28 sous Windows) entre le
> // test « minimisee ? » de la boucle principale et la lecture
> // de taille -- la course est reelle (defaut 4.3, reproduite).
> // Sous 32 px, les cibles divisees du rendu (bloom /32) tombent
> // a zero et CreateTexture2D echoue en E_INVALIDARG -> mort a
> // la restauration. On refuse net : la vraie taille arrivera
> // avec le retour de la fenetre.
> ```

**(d) État GL persistant entre frames** (`NkOpenGLRenderer2D.cpp:524-526`, §2.6).

**(e) Compilation SPIR-V au runtime → corruption de tas**
(`Integrations\NKGui\NkGuiRHIBackend.cpp:165-168`) :

> ```
> // SPIR-V PRÉ-COMPILÉ de kVertVk/kFragVk (glslangValidator). Utilisé au lieu de
> // CompileVkSpirv : la compilation glslang au RUNTIME corrompt le heap sous
> // clang-mingw (crash Vulkan à l'init de ce backend — reproduit par la démo
> // Model). Régénérer si kVertVk/kFragVk changent.
> ```

**(f) Matrice dégénérée dans `NkTransformable::GetTransform()`** — corrigé, mais
la trace est très instructive (`NkTransformable.h:161-172`) :

> ```
> /// a01 = -sy*sin = -sys     ← fix 2026-05-30 : etait -syc (FAUX, dégénerait)
> /// a11 = sy*cos = syc       ← fix 2026-05-30 : etait sys (FAUX, dégénerait)
> ///
> /// Bug d'origine : avec rotation=0, scale=(1,1) on obtenait sys=0
> /// et syc=1, et le code mettait m01=-syc=-1, m11=sys=0 → matrice
> /// dégénérée transformant tout en ligne Y=ty. Conséquence :
> /// paddles NkRectangleShape invisibles (vertices alignés en ligne),
> /// même symptôme sur ball/dashes/circles. Les scores marchent car
> /// DrawFilledRect ne passe PAS par NkTransformable.
> ```

**(g) Pixels CPU obligatoires pour le rasterizer Software**
(`NkTexture.cpp:109-111`) :

> ```
> // (=> GetCPUPixels()==null). Le rasterizer Software echantillonne
> // directement mCPUPixels ; sans cette copie -> textures/texte
> // INVISIBLES en Software. Fix bug 2026-06-05.
> ```

**(h) Race non résolue, assumée** (`Kernel\Runtime\NKCanvas\ROADMAP.md:680-682`) :

> ```
> - **`NkSoftwareContext` OnResize** : si appelé pendant que le pixel
>   buffer est en cours de présentation, race condition possible
>   (mCachedSurface read pendant que Resize écrit). Single-thread assumé.
> ```

### 6.5 Conventions d'identifiants

- `texId == 0` est **réservé** à la texture blanche interne côté RHI
  (`NkGuiRHIBackend.h:40`). Un `AddImage` avec `texId == 0` est ignoré
  (`NkGuiDrawList.cpp:160-161`).
- `NkGuiId` ne vaut **jamais 0** (les hash renvoient `1u` en cas de collision à 0,
  `NkGuiTypes.h:35`, `45`) : `0` = `NKGUI_ID_NONE` = « aucun widget ».
- Les widgets `*Editable` exigent un **`idStr` stable** distinct du libellé
  mutable — sinon l'identité change à chaque frappe pendant l'édition.

### 6.6 Le texte contraint par son conteneur (défaut ouvert)

`Kernel\Runtime\NKGui\ROADMAP.md` (« Défaut 3 ») :

> *« Le système de groupes de NK3DModeler cadre les **widgets** — leurs rectangles
> se calculent depuis la largeur du groupe — mais pas les **chaînes**, qui se
> peignent où on leur dit. Un libellé plus large que la colonne débordait tel
> quel. »*
>
> *« **Directive.** Vérifier que `TextWrapped` couvre ces cas, et que le socle
> expose une notion de **conteneur** dont le texte hérite — pas seulement un
> `wrapWidth` que chaque appelant doit calculer. »*

---

## 7. État réel — implémenté vs déclaré

### 7.1 NKGui

| Élément | État |
|---|---|
| Mode **immédiat** | ✅ complet et riche (4 973 lignes de widgets) |
| Mode **retenu** | ❌ **inexistant** — annoncé Phase 6 (`ARCHITECTURE.md:191`), aucun code |
| Dossiers `Layout/`, `Window/`, `Dock/`, `Retained/`, `Backend/` annoncés (`ARCHITECTURE.md:86-92`) | ❌ n'existent pas ; tout est dans `Core/` + `Widgets/` |
| `NkGuiId.h`, `NkGuiInteraction.h` annoncés | ❌ fusionnés dans `NkGuiTypes.h` |
| Widgets déclarés dans `NkGuiWidgets.h` | ✅ **tous** définis dans le `.cpp` (vérifié un par un) |
| Primitives `AddPolyline` / `AddBezier` / `AddArc` annoncées (`ARCHITECTURE.md:101-102`) | ❌ absentes de `NkGuiDrawList` |
| Multi-viewport OS (fenêtres hors de la fenêtre principale) | ❌ non-objectif déclaré (`ARCHITECTURE.md:24`, `NkGuiWidgets.h:255`) |
| Navigation clavier globale (Tab entre widgets) / manette | ❌ Phase 8 (`ARCHITECTURE.md:193`) |
| Tri de table (`Sortable`) | ⚠️ `TableGetSortColumn` implémenté (`cpp:3938`), mais `NkGuiWidgets.h:338` porte encore *« Tri = à venir »* — le commentaire est en retard sur le code |
| Imbrication de tables | ❌ *« Une seule table active à la fois (pas d'imbrication v1) »* (`NkGuiContext.h:397`) |
| `README.md` / `ARCHITECTURE.md` : « Phase 1 — squelette » | ⚠️ **obsolète** — se fier au code |

**Limites dures (constantes de compilation)** à connaître :
`PopupMax = 8` (`NkGuiContext.h:147`), `OcclMax = 16` (`:175`), `WinMax = 32`
(`:228`), `ChildMax = 8` (`:379`), `ContainerMax = 32` (`:385`), pile d'ID 32
(`:324`), pile `disabled` 16 (`:328`), `NkGuiTableMaxCols = 16` (`:120`),
8 onglets max par feuille de dock (`NkGuiTypes.h:295`), 32 caractères saisis par
frame (`NkGuiInput.h:53`), clip stack de 32 (`NkGuiDrawList.h:52`).

### 7.2 NKCanvas

| Élément | État |
|---|---|
| Batching CPU + soumission (`NkBatchRenderer2D`) | ✅ complet, partagé par 5 backends |
| Backend **OpenGL** | ✅ le seul validé runtime (`ROADMAP.md:185-200`) |
| Backends **Vulkan / DX12** | ✅ code le plus complet, ⚠️ crashes runtime signalés au 2026-05-30 |
| Backends **DX11 / Software** | ⚠️ écran vide/noir signalé au 2026-05-30 |
| Backend **Metal** | ❌ **pas de Renderer2D** (`NkRenderer2DFactory.cpp:86-89`, `137-139`) |
| `NkRenderTexture` | ⚠️ **stub** hors OpenGL (`USAGE.md:482-488`) |
| `NkShader::Compile()` | ⚠️ OpenGL uniquement, `false` ailleurs (`USAGE.md:482-488`) |
| `SetFilter`/`SetWrap` sur DX12/Vulkan | ⚠️ *no-op* (sampler global immuable) |
| `NkFont::GetKerning` | ❌ retourne toujours `0.f` (`NkFont.cpp:163-167`) |
| Faux-gras (`bold`) | ❌ ignoré (`NkFont.cpp:133`) |
| `NkText::FindCharacterPos` | ❌ `return 0; // TODO: binary search on glyph bounds` (`NkSprite.cpp:332-334`) |
| Primitives de dégradé / rectangle arrondi | ❌ inexistantes au niveau NKCanvas (NKGui les fabrique) |
| `DrawText` / `DrawImage` directs | ❌ passer par `NkText` / `NkSprite` / `DrawVertices` |

---

## Annexe — fichiers à lire en priorité, dans l'ordre

Pour construire un cours progressif :

1. `Kernel\Runtime\NKGui\src\NKGui\Core\NkGuiTypes.h` (333 l.) — le vocabulaire.
2. `Kernel\Runtime\NKGui\src\NKGui\Core\NkGuiInput.h` (145 l.) — l'entrée, tout inline.
3. `Kernel\Runtime\NKGui\src\NKGui\Core\NkGuiDrawList.h/.cpp` (101 + 342 l.) — la sortie.
4. `Kernel\Runtime\NKGui\src\NKGui\Core\NkGuiContext.cpp:39-142` — le cycle de frame.
5. `Kernel\Runtime\NKGui\src\NKGui\Core\NkGuiContext.cpp:491-570` — `ItemHoverable` + `ButtonBehavior`.
6. `Kernel\Runtime\NKGui\src\NKGui\Widgets\NkGuiWidgets.cpp:106-262` — Text, Button, TextWrapped.
7. `Kernel\Runtime\NKCanvas\src\NKCanvas\UI\NkGuiCanvasBackend.h` (221 l.) — le pont, en entier.
8. `Applications\NKGuiDemo\src\NKGuiDemo\main.cpp:240-480` puis `1035-1067` — le squelette réel.
9. `Kernel\Runtime\NKCanvas\src\NKCanvas\Renderer\Batch\NkBatchRenderer2D.cpp` (552 l.) — le batching.
10. `Kernel\Runtime\NKGui\ROADMAP.md` (« MANDAT ») — les trois défauts structurels d'interface.
