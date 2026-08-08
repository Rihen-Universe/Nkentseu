# NkCanvas : dessiner en deux dimensions

Nous descendons maintenant d'un étage pour examiner `NkCanvas` en
profondeur. C'est le module qui, en dernier ressort, fait apparaître des pixels.
Tout ce que fera NKGui au chapitre suivant finira ici.

Le plan de ce chapitre : d'abord *quelle est la forme de l'API* (deux
niveaux, une cible de rendu, un repère) ; ensuite *ce qu'on peut dessiner*
et surtout *ce qu'on ne peut pas* ; puis *comment ça arrive au GPU* —
c'est-à-dire le *batching*, qui est le vrai sujet du module ; enfin un
programme complet qui tourne.

## Ce que NkCanvas est, en une phrase

**`Guides/05-NKCanvas.md:11-19 (extrait)`**

```
**NKCanvas** est la couche de **rendu 2D conviviale** de Nkentseu. C'est
l'équivalent de la partie *Graphics* de **SFML** : … multi-backend …
```

La comparaison avec SFML est juste et vous fera gagner du temps si vous
connaissez cette bibliothèque : on retrouve la cible de rendu qui ouvre et ferme
l'image, les formes persistantes qu'on positionne et qu'on dessine, le
*drawable*, la vue. Ce qui change : le multi-backend, c'est-à-dire cinq API
graphiques possibles derrière une seule et même interface.

Environ 23 400 lignes réparties sur 101 fichiers, organisées ainsi :

| **Dossier** | **Contenu** |
|---|---|
| `Core/` | `NkIGraphicsContext`, `NkContextDesc`, `NkGraphicsApi` |
| `Factory/` | `NkContextFactory` — choisit et crée le contexte GPU |
| `Backend/` | `OpenGL/`, `Vulkan/`, `DirectX/`, `Metal/`, `Software/` |
| `Renderer/Core/` | `NkIRenderer2D`, `NkRenderer2D`, `NkVertexArray`, `NkTransform` |
| `Renderer/Batch/` | `NkBatchRenderer2D` — *le* batcher, base commune |
| `Renderer/Resources/` | `NkTexture`, `NkFont`, `NkSprite`/`NkText`, `NkShader` |
| `Renderer/Shapes/` | `NkRectangleShape`, `NkCircleShape`, `NkLineShape`… |
| `Renderer/Targets/` | `NkRenderTarget`, `NkRenderWindow`, `NkRenderTexture` |
| `UI/` | `NkGuiCanvasBackend.h` — le pont vu au chapitre 1 |
| `Compute/` | `NkIComputeContext` — GPGPU, hors sujet ici |

Tous les types vivent dans l'espace de noms `nkentseu::renderer`.

## Les deux niveaux d'API

### `NkIRenderer2D` : l'interface

`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Core/NkIRenderer2D.h`
déclare l'interface de rendu 2D. Il y a une implémentation par API graphique,
mais elles héritent toutes d'une classe intermédiaire, `NkBatchRenderer2D`,
qui fait l'essentiel du travail (section 2.9).

### `NkRenderer2D` : la façade

`.../Renderer/Core/NkRenderer2D.h` est la classe concrète que manipule
le code applicatif. Tout est transféré *inline* vers l'interface. Elle
n'ajoute presque rien — sauf une surcharge de commodité — et son intérêt est
ailleurs : elle documente la propriété.

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Core/NkRenderer2D.h:31-35`**

```cpp
/// PROPRIETE
///   NkRenderer2D ne POSSEDE pas son backend NkIRenderer2D* — le NkRenderTarget
///   qui le contient est proprietaire (typiquement via memory::NkUniquePtr).
///   Le NkRenderer2D est juste une projection. La duree de vie suit celle du
///   NkRenderTarget englobant.
```

avec, en clair, dans les membres privés (lignes 314-315) :
`NkIRenderer2D *mBackend{nullptr}; ///< non-owning` et
`NkRenderTarget *mTarget{nullptr}; ///< non-owning`.

> **⚠️ La façade meurt avec sa cible**
>
> Un `NkRenderer2D` récupéré par `target.GetRenderer2D()` n'est
> qu'une vue sur le renderer possédé par la cible. Le conserver au-delà de la
> durée de vie de la `NkRenderWindow` — dans un membre de classe, par
> exemple — donne un objet qui pointe vers de la mémoire libérée. C'est le même
> raisonnement, et le même risque, que pour `ctx.font` au chapitre 4.

## La cible de rendu : `NkRenderWindow`

En pratique, on ne manipule ni le contexte graphique ni le renderer directement :
on crée une `NkRenderWindow`, qui fait les deux et possède le cycle
d'image.

**`Applications/Sandbox/src/DemoNkentseu/NkCanvas/NkCanvasDemo.cpp:69-77`**

```cpp
    // ── 2. Cible de rendu NKCanvas (la voie haut-niveau, comme Pong) ────────────
    NkContextDesc desc;
    desc.api = ParseBackend(state.args);
    NkRenderWindow target(window, desc);
    if (!target.IsValid()) {
        logger.Error("[nkcanvas] NkRenderWindow init FAILED");
        window.Close();
        return -2;
    }
```

Une ligne de commentaire, en tête de cette même démo, résume la doctrine
d'usage — il faut la retenir :

**`Applications/Sandbox/src/DemoNkentseu/NkCanvas/NkCanvasDemo.cpp:6-7`**

```cpp
// NkRenderWindow possede le cycle de frame -> Clear() ouvre+efface, on dessine,
// Display() termine+presente (pas de Begin/End/SetView/SetViewport a la main).
```

Voici ce que ces deux méthodes font réellement :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Targets/NkRenderWindow.cpp:145-171 (abrégé)`**

```cpp
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

Autrement dit : `Clear` *ouvre* l'image (si elle ne l'est pas déjà)
et l'efface ; `Display` la *ferme* — ce qui vide les tampons
accumulés vers le GPU — puis la présente à l'écran. Vous n'appellerez jamais
`Begin()` ni `End()` vous-même.

> **⚠️ Ne pas ré-initialiser le renderer**
>
> **`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Targets/NkRenderWindow.cpp:95-98`**
>
> ```cpp
> // NB : NE PAS appeler mRenderer->Initialize(mContext) ici !
> // NkRenderer2DFactory::Create l'a deja fait (cf. NkRenderer2DFactory.cpp:83).
> // Double-init -> NkOpenGLRenderer2D::Initialize log "Already initialized"
> // ERR et corrompt l'etat (rectangles creux observes en runtime).
> ```
>
> Symptôme mémorable : des « rectangles creux » — les formes apparaissent en
> contour, vides. Si vous voyez ça, cherchez une double initialisation ou une
> matrice dégénérée (section 2.8).

### Le redimensionnement

`OnResize(w, h)` recrée la chaîne d'échange (*swapchain*) et recale
la vue par défaut. Deux subtilités :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Targets/NkRenderWindow.cpp:353-358 (extrait)`**

```cpp
// Si une frame est en cours, on la termine avant la recreation
// (sinon submit sur swapchain detruite -> UB sur Vulkan/DX12).
if (mFrameOpen && mRenderer) { mRenderer->End(); mFrameOpen = false; }
```

et, du côté du batcher, `OnResize` recale la vue *par défaut* mais
**préserve une vue personnalisée** posée par `SetView`
(`.../Renderer/Batch/NkBatchRenderer2D.cpp:169-196`).

> **⚠️ Détecter le resize sur la fenêtre, pas sur la swapchain**
>
> **`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:444-448`**
>
> ```cpp
>         // Synchroniser la SWAPCHAIN à la taille de la FENÊTRE. target->GetSize()
>         // renvoie la taille de la swapchain (qui ne change QU'À OnResize) — s'en
>         // servir pour détecter le resize était faux : la swapchain restait figée à
>         // sa taille de création (rendu basse-résolution étiré). On pilote OnResize
>         // depuis la taille fenêtre (inclut la 1re frame → sync initiale).
> ```
>
> Le code correct compare `target->GetWindow().GetSize()` — la fenêtre — à
> la dernière taille connue, et appelle `OnResize` si elle a changé. Se
> servir de `target->GetSize()` pour *détecter* le changement est une
> boucle logique : cette valeur ne change qu'*après* `OnResize`. Le
> symptôme est très reconnaissable : l'application semble marcher, mais tout est
> flou et étiré dès qu'on agrandit la fenêtre.

## Le repère : Y vers le bas

**L'origine est en haut à gauche et l'axe Y descend.** C'est la convention
de toutes les interfaces graphiques, et elle est confirmée dans le batcher :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Batch/NkBatchRenderer2D.cpp:183 (commentaire)`**

```cpp
// Vue par defaut = ecran plein-cadre (origine haut-gauche, Y-down)
```

La projection orthographique correspondante est construite ici :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Core/NkRenderer2DTypes.cpp:85-87`**

```cpp
  const float32 a =  2.f / size.x;
  const float32 b = -2.f / size.y; // Y-down (positive Y goes down in screen space)
```

Le signe moins sur `b` est toute l'histoire : c'est lui qui
retourne l'axe.

Quelques lignes plus haut, une note sur la convention mémoire des matrices, qui
est un piège classique dès qu'on vise plusieurs API :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Core/NkRenderer2DTypes.cpp:81-84`**

```cpp
// Column-major output (compatible with glUniformMatrix4fv with transpose=GL_FALSE,
// and with HLSL cbuffer when uploaded as-is since HLSL is row-major — we compensate
// by transposing at upload in the DX backends)
```

### La caméra 2D

`NkView2D` (`.../Renderer/Core/NkRenderer2DTypes.h:41-47`) porte un
`center`, une `size` et une `rotation`. On la pose par
`SetView`. Deux conversions utiles existent pour passer du monde à
l'écran et réciproquement : `MapPixelToCoords(NkVec2i)` et
`MapCoordsToPixel(NkVec2f)`
(`.../Renderer/Batch/NkBatchRenderer2D.cpp:538-550`).

Tant que vous dessinez une interface, vous n'y toucherez pas : la vue par défaut
est exactement l'écran en pixels, ce qui est ce qu'on veut.

## Les primitives disponibles

Toutes sont déclarées dans `NkIRenderer2D.h` et **toutes sont
implémentées** dans `NkBatchRenderer2D`, donc disponibles sur les cinq
backends actifs.

| **Signature (abrégée)** | **Ligne** | **Réalisation** |
|---|---|---|
| `Draw(const NkSprite &)` | 135 | quad texturé |
| `Draw(const NkText &)` | 138 | délègue à `NkText::Draw` |
| `DrawPoint(pos, color, size)` | 141 | quad `size`×`size` |
| `DrawLine(a, b, color, thickness)` | 143 | quad orienté |
| `DrawRect(rect, color, outline, outlineColor)` | 146 | rempli + contour |
| `DrawFilledRect(rect, color)` | 149 | un quad |
| `DrawCircle(center, radius, color, segments, …)` | 151 | polygone |
| `DrawFilledCircle(center, radius, color, segments)` | 155 | éventail de triangles |
| `DrawTriangle(a, b, c, …)` | 158 | contour |
| `DrawFilledTriangle(a, b, c, color)` | 161 | un triangle |
| `DrawVertices(verts, n, indices, m, texture)` | 192 | **le point d'entrée bas niveau** |

La dernière ligne est celle qui compte : `DrawVertices` est la primitive
*universelle*, celle qu'utilise le pont NKGui, et à travers laquelle
transite absolument toute l'interface d'une application.

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Core/NkIRenderer2D.h:192-193`**

```cpp
virtual void DrawVertices(const NkVertex2D *vertices, uint32 vertexCount,
                          const uint32 *indices, uint32 indexCount,
                          const NkTexture *texture = nullptr) = 0;
```

### Le sommet

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Core/NkRenderer2DTypes.h:73-77`**

```cpp
struct NkVertex2D { float32 x, y; float32 u, v; uint8 r, g, b, a; };
```

Vingt octets : deux flottants de position, deux de coordonnées de texture,
quatre octets de couleur. C'est délibérément compact — on en pousse des dizaines
de milliers par image.

### Les primitives composites, réalisées dans l'en-tête

Trois fonctions sont implémentées *par défaut* directement dans
`NkIRenderer2D.h`, donc sans que le moindre backend ait à s'en occuper :

- `DrawRectOutline(rect, color, thickness)` — quatre
  `DrawFilledRect` (lignes 167-175) ;
- `DrawCircleOutline(center, radius, color, thickness, segments)` —
  un anneau en segments de `DrawLine` (lignes 178-189) ;
- `DrawTexturedRect(rect, texture, color, uv)` — construit un quad
  `NkVertex2D[4]` et appelle `DrawVertices` (lignes 199-233).

C'est un patron de conception qu'on retrouvera : **on n'ajoute une méthode
virtuelle que si elle a besoin du backend**. Tout ce qui se compose à partir des
primitives existantes s'écrit une fois, dans l'interface.

### Les types de primitives, et l'absence de QUADS

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Core/NkRenderer2DTypes.h:60-70`**

```cpp
// Pas de QUADS — les quads sont decomposes en 2 triangles par le batcher
// (compatible cross-API : Vulkan/DX/Metal n'ont pas de QUADS natif)
enum class NkPrimitiveType : uint8 {
    NK_POINTS = 0, NK_LINES = 1, NK_LINE_STRIP = 2,
    NK_TRIANGLES = 3, NK_TRIANGLE_STRIP = 4, NK_TRIANGLE_FAN = 5,
};
```

Le commentaire est une bonne illustration de ce que « multi-backend » veut dire
en pratique : l'API commune ne peut offrir que l'intersection de ce que les API
sous-jacentes savent faire. OpenGL avait des quads ; Vulkan, Direct3D et Metal
n'en ont pas. Donc l'abstraction n'en a pas non plus, et le batcher découpe.

## Ce que NkCanvas n'a PAS

Cette section est courte mais elle vous fera gagner des heures. Voici ce que
vous chercherez en vain :

- **Pas de `DrawText`.** Il n'existe aucune méthode qui prenne
  une chaîne et l'affiche. Le texte passe obligatoirement par le
  *drawable* `NkText` — `Draw(const NkText &)` — ou par
  le patron SFML `target.Draw(drawable, states)`.
- **Pas de `DrawImage`.** Même chose : on passe par
  `NkSprite`, ou par `DrawTexturedRect`, ou par
  `DrawVertices` avec une texture.
- **Pas de dégradé.** Aucune primitive `DrawGradient*`. Le seul
  moyen d'obtenir un dégradé est de donner des couleurs différentes aux
  sommets d'un même triangle et de laisser le GPU interpoler — donc via
  `DrawVertices` ou `NkVertexArray`.
- **Pas de coins arrondis.** Aucun `DrawRoundedRect`.

> **✅ Ce qu'il faut retenir**
>
> Ces quatre absences ne sont pas des oublis : ce sont des choix de niveau.
> NkCanvas dessine des *formes géométriques*. Le dégradé, les coins arrondis
> et le texte mis en page sont des affaires de *présentation*, et c'est NKGui
> qui les fabrique — en trianguleant lui-même des arcs de coin, en donnant quatre
> couleurs à un quad, en émettant un quad par glyphe. Nous verrons tout cela au
> chapitre 3.

## Les couleurs

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Core/NkRenderer2DTypes.h:12`**

```cpp
using NkColor2D = math::NkColor;
```

`math::NkColor`
(`Kernel/Foundation/NKMath/src/NKMath/NkColor.h:1046`) est une classe
**RGBA sur 8 bits**, quatre octets en tout (`uint8 r, g, b, a`). On
l'écrit en agrégat : `NkColor2D{18, 20, 28, 255}`.

Il existe un pendant flottant, `NkColorF` (composantes dans [0,1], 16
octets), qui porte tout l'arsenal colorimétrique : HSL, HSV, LAB, LCH, XYZ,
OKLab, OKLch, CMYK, hexadécimal, luminance WCAG, DeltaE et DeltaE2000, modes de
fusion *Multiply*, *Screen*, *Overlay*, *SoftLight*,
*HardLight*. Utile pour calculer une palette ; inutile pour dessiner. Le
rendu 2D n'emploie que les quatre `uint8`.

## Les textures

`.../Renderer/Resources/NkTexture.h:36` — une ressource GPU, ni copiable
ni déplaçable.

### Créer et remplir

| **Méthode** | **Rôle** |
|---|---|
| `Create(renderer, w, h, fillColor)` | texture vide d'une couleur |
| `LoadFromFile(renderer, path)` | depuis un fichier image |
| `LoadFromImage(renderer, image, area)` | depuis un `NkImage` déjà décodé |
| `LoadFromMemory(…)` | depuis un tampon d'octets |
| `Update(pixels, destX, destY)` | mise à jour *partielle* |
| `SetFilter` / `SetWrap` / `GenerateMipmap` | paramètres d'échantillonnage |
| `GetTexCoords(const NkRect2i &)` | sous-rectangle en pixels → UV dans [0,1]² |

Les filtres et modes de répétition sont deux petites énumérations :
`NkTextureFilter{NK_NEAREST, NK_LINEAR}` et
`NkTextureWrap{NK_CLAMP, NK_REPEAT, NK_MIRROR_REPEAT}`
(`NkTexture.h:22-33`).

> **⚠️ `SetFilter` ne fait rien partout**
>
> Sur DX12 et Vulkan, il n'y a pas de *sampler* par texture : le
> *sampler* est global et immuable, donc `SetFilter` et
> `SetWrap` sont des *no-op*. C'est écrit dans les « pièges connus » du
> module (`Kernel/Runtime/NKCanvas/USAGE.md:482-488`), avec deux autres
> limites de la même liste : `NkRenderTexture` n'est un vrai tampon
> hors-écran que sur OpenGL (ailleurs c'est un *stub*), et
> `NkShader::Compile()` ne fonctionne que sur OpenGL — il retourne
> `false` ailleurs.

### La texture blanche de 1 pixel

Voici une idée simple et instructive :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Resources/NkTexture.h:121`**

```cpp
static NkTexture *NkTexture::GetWhiteTexture(NkIRenderer2D &);
```

Cette texture d'un pixel blanc est utilisée par *toutes* les primitives
*non texturées* : `DrawLine`, `DrawFilledRect`,
`DrawFilledCircle`, `DrawFilledTriangle`
(`.../Renderer/Batch/NkBatchRenderer2D.cpp:394, 401, 426, 479`). Pourquoi ?
Pour que **tout passe par le même *shader* et le même
*pipeline***. Il n'y a pas un chemin « uni » et un chemin « texturé » : il y
a un seul chemin, texturé, et le uni est simplement du blanc multiplié par la
couleur du sommet.

> **✅ Ce qu'il faut retenir**
>
> « Tout est texturé ; le uni, c'est juste du blanc. » Cette astuce supprime un
> changement d'état GPU par forme, et surtout permet de regrouper dans un même
> appel de dessin des rectangles pleins et du texte — à condition qu'ils partagent
> la même texture, ce qui n'est pas le cas ici, mais nous verrons que le principe
> se généralise.

### Les pixels conservés côté CPU

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Resources/NkTexture.cpp:109-111`**

```cpp
// (=> GetCPUPixels()==null). Le rasterizer Software echantillonne
// directement mCPUPixels ; sans cette copie -> textures/texte
// INVISIBLES en Software. Fix bug 2026-06-05.
```

Chaque texture garde donc une copie de ses pixels en mémoire centrale
(`mCPUPixels`). C'est du gaspillage sur GPU, mais c'est la seule façon de
faire fonctionner le rasteriseur logiciel. À connaître si vous vous demandez
pourquoi une texture consomme deux fois sa taille.

## Les polices et le texte

`NkFont`, dans NkCanvas, est un **enveloppeur GPU** au-dessus du
module externe `NKFont`, qui fait la rasterisation (celle-ci a été sortie
de NkCanvas le 28 mai 2026, cf. `.../Renderer/Resources/NkFont.h:56-63`).

- **Chargement** :
  `NkFont::LoadFromFile(NkIRenderer2D &renderer, const char *path)`
  ou `LoadFromMemory(…)`. L'implémentation lit les octets et
  *conserve la copie en mémoire* (`mFontData`,
  `NkFont.cpp:65-81`) : la fonte doit rester disponible pour
  rasteriser de nouvelles tailles.
- **Atlas** : une *page* par taille de caractère demandée
  (`struct Page`, `NkFont.h:102-107`), créée paresseusement
  par `GetOrCreatePage` (`NkFont.cpp:86-128`). Chaque page
  construit un atlas puis l'envoie dans une `NkTexture`.
- **Glyphes** :
  `GetGlyph(codepoint, characterSize, bold)` renvoie un
  `NkGlyph` = `textureRect` (pixels dans l'atlas),
  `bounds` (décalage et taille), `advance` (avance
  horizontale).

Deux limites à connaître, écrites dans le code :

- **Le crénage n'est pas exposé.** `GetKerning` retourne
  toujours `0.f` (`NkFont.cpp:163-167`) : « Le module NKFont
  n'expose pas le kerning par paire (integre dans l'advance des glyphes) ».
- **Le faux-gras n'existe pas.** Le paramètre `bold` est
  ignoré (`NkFont.cpp:133`, paramètre nommé `/*bold*/`) :
  « charger une fonte bold dediee si necessaire ».

### Comment un texte devient des triangles

Chaque glyphe devient un quad de quatre `NkVertex2D`
(`.../Renderer/Resources/NkSprite.cpp:213-228`). Puis
`NkText::Draw` applique la transformation — rotation, échelle, origine —
**côté CPU**, sommet par sommet (`NkSprite.cpp:290-316`), et soumet
le tout d'un coup :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Resources/NkSprite.cpp:318`**

```cpp
renderer.DrawVertices(tverts.Data(), …, atlas);
```

Résultat : **un seul appel de dessin par texte**, quelle que soit
sa longueur. C'est la logique du *batching*, appliquée localement.

Une fonction reste non implémentée et il vaut mieux le savoir avant de compter
dessus :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Resources/NkSprite.cpp:332-334`**

```cpp
return 0; // TODO: binary search on glyph bounds
```

(`NkText::FindCharacterPos` — convertir un index de caractère en
position à l'écran).

## Les formes persistantes

Pour du contenu qui ne change pas à chaque image, NkCanvas offre des objets
persistants façon SFML. Ils héritent tous de
`NkShape : public NkTransformable, public NkDrawable`
(`.../Renderer/Shapes/NkShape.h:40`), qui factorise le remplissage
(triangulation en éventail), le contour et une texture optionnelle.

| **Classe** | **API propre** |
|---|---|
| `NkRectangleShape` | `explicit NkRectangleShape(NkVec2f size)`, `SetSize`/`GetSize` |
| `NkCircleShape` | `explicit NkCircleShape(float32 r, uint32 segments = 32)` |
| `NkLineShape` | `NkLineShape(NkVec2f a, NkVec2f b, float32 thickness = 1.f)` |
| `NkConvexShape` | `SetPointCount(n)`, `SetPoint(i, p)` |

Limite documentée : la triangulation en éventail n'est valable que pour des
polygones **convexes** (`NkShape.h:21-26`,
`NkConvexShape.h:19-23`). Un polygone concave dessiné ainsi produira des
recouvrements aberrants.

> **⚠️ Le tampon temporaire détruit avant l'envoi**
>
> C'est le meilleur exemple pédagogique du dépôt et il mérite d'être lu deux fois :
>
> **`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Shapes/NkShape.cpp:62-71`**
>
> ```cpp
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
>
> La leçon dépasse largement ce cas : **dès qu'un système accumule des
> données pour les envoyer plus tard, tout tampon local devient suspect**. Vous
> retrouverez exactement ce raisonnement au chapitre 3 (une surcouche différée ne
> doit jamais détenir de pointeur vers la pile de l'appelant) et au chapitre 4
> (la police doit survivre à toute la boucle).

> **⚠️ Une matrice dégénérée aplatit tout**
>
> Corrigé depuis, mais très instructif :
>
> **`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Core/NkTransformable.h:161-172`**
>
> ```cpp
> /// a01 = -sy*sin = -sys     <- fix 2026-05-30 : etait -syc (FAUX, degenerait)
> /// a11 = sy*cos = syc       <- fix 2026-05-30 : etait sys (FAUX, degenerait)
> ///
> /// Bug d'origine : avec rotation=0, scale=(1,1) on obtenait sys=0
> /// et syc=1, et le code mettait m01=-syc=-1, m11=sys=0 -> matrice
> /// degeneree transformant tout en ligne Y=ty. Consequence :
> /// paddles NkRectangleShape invisibles (vertices alignes en ligne),
> /// meme symptome sur ball/dashes/circles. Les scores marchent car
> /// DrawFilledRect ne passe PAS par NkTransformable.
> ```
>
> Notez le détail final, qui est la clé du diagnostic : *ce qui marchait
> encore* indiquait précisément le chemin de code fautif. Quand une partie de
> l'affichage disparaît et pas une autre, cherchez ce qui les distingue.

## Le batching : le vrai sujet du module

Nous arrivons au cœur de NkCanvas. Fichier central :
`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Batch/NkBatchRenderer2D.cpp`
(552 lignes), classe de base de *tous* les backends actifs.

L'idée : au lieu d'envoyer chaque forme au GPU au moment où on la dessine — ce
qui coûterait un appel de pilote par rectangle — on **accumule** tout dans
des tableaux en mémoire centrale, et on n'envoie qu'à la fin, en gros paquets.

### L'accumulation

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Batch/NkBatchRenderer2D.h:157-159`**

```cpp
NkVector<NkVertex2D> mVertices;
NkVector<uint32>     mIndices;
NkVector<NkBatchGroup> mGroups;
```

Un `NkBatchGroup` est **un futur appel de dessin** :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Batch/NkBatchRenderer2D.h:23-28 (abrégé)`**

```cpp
struct NkBatchGroup {
    const NkTexture *texture;
    NkBlendMode blendMode;
    uint32 indexStart;
    uint32 indexCount;
};
```

Et voici la règle qui gouverne la performance de toute votre interface :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Batch/NkBatchRenderer2D.cpp:207-229 (principe)`**

```cpp
// EnsureGroup(tex, blend) ferme le groupe courant et en ouvre un nouveau
// DES QUE la texture ou le mode de fusion change (et incremente mStats.textureSwap).
```

> **✅ La règle d'or du 2D**
>
> **Un appel de dessin par groupe ; un nouveau groupe à chaque changement de
> texture ou de mode de fusion.** Donc : moins on change de texture, moins il y a
> d'appels de dessin. C'est la raison d'être des *atlas* — une seule texture
> qui contient tous les glyphes, ou toutes les icônes — et c'est pourquoi
> alterner « une icône, un texte, une icône, un texte » coûte quatre appels là où
> « deux icônes puis deux textes » n'en coûte que deux.

### La capacité, et l'histoire qui va avec

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Batch/NkBatchRenderer2D.h:33-40`**

```cpp
// Maximum vertices / indices before an automatic flush.
// 262144 (~5 Mo GPU) : une UI dense (IDE plein écran : arbre + onglets +
// icônes) dépasse 65536 sommets ENTRE DEUX CLIPS — la draw-list NkGui
// arrive alors en UNE commande plus grosse que l'ancien buffer, et
// l'upload écrivait HORS du buffer GPU (crash memcpy). Les 3 backends
// (DX11/DX12/GL) créent leurs buffers avec ces constantes.
static constexpr uint32 kMaxVertices = 262144;
static constexpr uint32 kMaxIndices  = kMaxVertices * 6 / 4;
```

Deux cent soixante-deux mille sommets, environ 5 Mo côté GPU. Le commentaire
raconte pourquoi : une interface dense produit, *entre deux changements de
découpe*, une seule commande de plus de 65 536 sommets. L'ancien tampon faisait
exactement cette taille ; l'envoi écrivait donc au-delà, et le programme
plantait dans un `memcpy`.

Quand l'accumulation risque de déborder, un vidage est déclenché
automatiquement :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Batch/NkBatchRenderer2D.cpp:235-237`**

```cpp
  if (mVertices.Size() + 4 > kMaxVertices || mIndices.Size() + 6 > kMaxIndices) {
      Flush();
  }
```

### La soumission : `Flush()`

`Flush()` fait quatre choses (`NkBatchRenderer2D.cpp:56-98`) :
fermer le dernier groupe et compacter les groupes vides ; appliquer le découpage
courant ; vérifier qu'on ne dépasse pas la capacité ; envoyer.

Le troisième point est une garde défensive exemplaire :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Batch/NkBatchRenderer2D.cpp:80-88`**

```cpp
// GARDE-FOU : ne JAMAIS soumettre plus que la capacité des buffers GPU
// (l'upload écrirait hors buffer -> crash). Troncature (indices en
// multiple de 3) : dégradation visuelle plutôt que corruption mémoire.
uint32 vSub = mVertices.Size(), iSub = mIndices.Size();
if (vSub > kMaxVertices) vSub = kMaxVertices;
if (iSub > kMaxIndices)  iSub = kMaxIndices - (kMaxIndices % 3);
SubmitBatches(mGroups.Data(), validCount, mVertices.Data(), vSub, mIndices.Data(), iSub);
```

> **✅ Ce qu'il faut retenir**
>
> « Dégradation visuelle plutôt que corruption mémoire. » Cette phrase est un
> principe de conception à part entière : quand on ne peut pas satisfaire une
> demande, on rend un résultat incomplet mais *sûr*, jamais un résultat qui
> détruit la mémoire. Notez aussi le `% 3` : tronquer des indices de
> triangles à un multiple de trois, faute de quoi le dernier triangle serait
> incomplet.

### Le vrai appel de dessin

`SubmitBatches` est la seule méthode virtuelle pure que chaque backend
doit fournir. Voici la version OpenGL, la plus lisible :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Backend/OpenGL/NkOpenGLRenderer2D.cpp:591-613`**

```cpp
void NkOpenGLRenderer2D::SubmitBatches(const NkBatchGroup *groups, uint32 groupCount,
                                       const NkVertex2D *verts, uint32 vCount,
                                       const uint32 *idx, uint32 iCount) {
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

Tout le lot est envoyé en **un seul** `glBufferSubData` par vidage,
puis **un** `glDrawElements` par groupe. Voilà, très concrètement,
ce que « batching » veut dire.

> **⚠️ `Begin()` n'est pas ré-entrant**
>
> **`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Batch/NkBatchRenderer2D.cpp:27-31`**
>
> ```cpp
> bool NkBatchRenderer2D::Begin() {
>     if (mInFrame) {
>         logger.Warnf("[NkBatch] Begin() called twice");
>         return false;
>     }
> ```
>
> Si ce message apparaît dans `logs/app.log`, c'est que quelque chose ouvre
> une image sans la fermer. En pratique, cela signifie qu'on a appelé
> `Begin()` à la main alors que `NkRenderWindow::Clear()` le fait
> déjà.

## Le découpage (*scissor*) et sa pile

Le découpage restreint le rendu à un rectangle. C'est indispensable pour les
panneaux défilables : un élément qui dépasse doit être coupé net, pas dessiné
par-dessus le voisin.

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Core/NkIRenderer2D.h:105-121`**

```cpp
virtual void SetClip(const NkRect2i &rectPixels) { (void)rectPixels; }
virtual void PopClip() {}
virtual void ResetClip() {}
virtual bool HasClip() const { return false; }
virtual NkRect2i GetClip() const { return NkRect2i{}; }
```

Le contrat est écrit juste au-dessus :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Core/NkIRenderer2D.h:98-104`**

```cpp
// Pile : SetClip empile le rect (intersecte avec le clip courant) ; PopClip
// depile. ResetClip vide la pile. Implementation backend : glScissor (GL),
// VkRect2D dynamique (Vulkan), RSSetScissorRects (DX11/12), clamp CPU (Software).
```

Deux propriétés à retenir : **c'est une pile**, et **l'empilement
intersecte**. Un enfant ne peut donc jamais dessiner en dehors de son parent,
quoi qu'il demande.

### Changer de découpe force un vidage

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Batch/NkBatchRenderer2D.cpp:120-126`**

```cpp
void NkBatchRenderer2D::SetClip(const NkRect2i &rect) {
    Flush(); // committe la geometrie en cours avec le clip actuel
    const NkRect2i clip = mHasClip ? NkIntersectClip(mClipRect, rect) : rect;
    mClipStack.PushBack(clip);
    mClipRect = clip;
    mHasClip = true;
}
```

C'est logique — le *scissor* est un état GPU, pas un attribut de sommet :
tout ce qui est accumulé avant le changement doit partir avec l'ancien
rectangle. Mais c'est aussi une conséquence de performance directe :

> **✅ Ce qu'il faut retenir**
>
> **Chaque `SetClip` et chaque `PopClip` coûte un vidage,
> donc au moins un appel de dessin.** Une interface très découpée — un
> `PushClipRect` par ligne de liste, par exemple — multiplie les appels de
> dessin. Le bon réflexe est de poser *une* découpe pour toute une zone, puis
> d'écarter soi-même les éléments hors vue avant de les émettre (nous verrons ce
> « culling manuel » au chapitre 3).

### Le retournement d'axe sous OpenGL

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Backend/OpenGL/NkOpenGLRenderer2D.cpp:532-546 (extrait)`**

```cpp
    // Clip en pixels, origine haut-gauche -> glScissor a l'origine bas-gauche :
    // on inverse Y avec la hauteur de la surface (= mViewport.height, viewport
    // plein ecran a top=0).
    const int32 y = mViewport.height - rect.y - h; // flip Y
    glEnable(GL_SCISSOR_TEST);
    glScissor((GLint)x, (GLint)y, (GLsizei)w, (GLsizei)h);
```

OpenGL place son origine en bas à gauche. Le module a choisi la convention
« haut-gauche » pour toute son API et compense dans le seul backend concerné.
C'est exactement le bon endroit pour faire ce genre de correction : *une
fois, au plus près de l'API*, et jamais dans le code applicatif.

> **⚠️ L'état OpenGL survit d'une image à l'autre**
>
> **`Kernel/Runtime/NKCanvas/src/NKCanvas/Backend/OpenGL/NkOpenGLRenderer2D.cpp:524-526`**
>
> ```cpp
> // Chaque frame demarre sans scissor (sinon un Clear serait clippe par le
> // scissor laisse par la frame precedente, l'etat GL etant persistant)
> ```
>
> Symptôme, si on l'oublie : l'écran ne s'efface que partiellement, et une zone
> garde l'image de la frame précédente comme une traînée. Les machines à états
> globales — OpenGL en est l'archétype — demandent qu'on remette explicitement
> tout à zéro au début de chaque image.

## La surface réellement utilisée en pratique

Nous venons de parcourir une API large : des dizaines de méthodes, des formes,
des sprites, des vues, des shaders. Voici maintenant un fait qui remet tout en
perspective.

L'IDE `NKCode` — l'application la plus grosse du dépôt, plus de trente
mille lignes d'interface — n'utilise de NkCanvas que **sept méthodes** :

| **Méthode** | **Ligne** | **Appelée par** |
|---|---|---|
| `bool Begin()` | 59 | `NkRenderWindow::Clear` |
| `void End()` | 60 | `NkRenderWindow::Display` |
| `void Clear(const NkColor2D &)` | 73 | le fond de l'image |
| `void OnResize(uint32, uint32)` | 89 | redimensionnement |
| `void SetClip(const NkRect2i &)` | 105 | découpage |
| `void PopClip()` | 109 | dépile le découpage |
| `void DrawVertices(…)` | 192 | **tout le dessin** |

plus, côté ressources, `NkTexture::Create`,
`NkTexture::Update` et `NkTexture::SetFilter`.

Voilà tout. NKCode ne dessine ni sprite, ni forme, ni texte NkCanvas : il envoie
des triangles pré-calculés par NKGui. Une recherche exhaustive des
`#include "NKCanvas/..."` dans tout `Applications/NKCode/src/` ne
renvoie **aucun** résultat — le module n'apparaît que dans les dépendances
d'édition de liens.

> **✅ Ce qu'il faut retenir**
>
> Pour écrire une interface, la surface utile de NkCanvas se réduit à :
> `Clear` / `Display` au niveau de la fenêtre,
> `SetClip` / `PopClip` pour découper, `DrawVertices` pour
> dessiner, et `NkTexture` pour les atlas. Le reste — formes, sprites,
> vues, shaders — sert aux jeux et aux démos, pas aux interfaces.

## Un programme complet

Assemblons. Le programme ci-dessous est la démo NkCanvas du dépôt, prise telle
quelle : une fenêtre, une balle qui rebondit, un rectangle qui pulse, un triangle
qui tourne, un éventail de lignes. Aucun NKGui, aucune police.

### L'ossature

**`Applications/Sandbox/src/DemoNkentseu/NkCanvas/NkCanvasDemo.cpp:11-33 (extrait)`**

```cpp
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKTime/NkTime.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NkColor.h"
#include "NKMath/NKMath.h" // math::NkCos / NkSin

#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkGraphicsApi.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/Renderer/Core/NkRenderer2D.h"

using namespace nkentseu;
using namespace nkentseu::renderer;

NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName = "NkCanvas Demo";
    d.appVersion = "1.0.0";
    return d;
})());
```

### Fenêtre, cible, état

**`Applications/Sandbox/src/DemoNkentseu/NkCanvas/NkCanvasDemo.cpp:57-93 (abrégé)`**

```cpp
int nkmain(const NkEntryState &state) {
    // ── 1. Fenetre ──────────────────────────────────────────────────────────────
    NkWindowConfig cfg;
    cfg.title = "NkCanvas Demo (NKCanvas seul, style SFML/SDL)";
    cfg.width = 900;
    cfg.height = 600;
    cfg.centered = true;
    cfg.resizable = true;
    NkWindow window;
    if (!window.Create(cfg)) {
        logger.Error("[nkcanvas] window failed");
        return -1;
    }

    // ── 2. Cible de rendu NKCanvas ──────────────────────────────────────────────
    NkContextDesc desc;
    desc.api = ParseBackend(state.args);
    NkRenderWindow target(window, desc);
    if (!target.IsValid()) { window.Close(); return -2; }
    logger.Infof("[nkcanvas] backend = %s", NkGraphicsApiName(desc.api));

    // ── 3. Etat de la scene ─────────────────────────────────────────────────────
    bool running = true;
    auto &events = NkEvents();
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) { running = false; });

    NkClock clock;
    uint32 lastW = 0, lastH = 0;
    float32 t = 0.f;

    math::NkVec2f ball{220.f, 200.f};
    math::NkVec2f vel{260.f, 215.f}; // pixels / seconde
    const float32 R = 42.f;
```

### La boucle

**`Applications/Sandbox/src/DemoNkentseu/NkCanvas/NkCanvasDemo.cpp:96-118 (abrégé)`**

```cpp
    while (running && window.IsOpen()) {
        float32 dt = clock.Tick().delta;
        if (dt > 0.1f)
            dt = 1.0f / 60.0f;
        t += dt;
        while (NkEvent *ev = events.PollEvent()) {
            (void)ev;
        }
        if (!running)
            break;

        // Resize : la cible suit la fenetre (vue par defaut = ecran) -> pas de clip.
        const math::NkVec2u sz = target.GetSize();
        if (sz.x != lastW || sz.y != lastH) {
            if (lastW != 0 && sz.x > 0 && sz.y > 0)
                target.OnResize(sz.x, sz.y);
            lastW = sz.x;
            lastH = sz.y;
        }
        const float32 W = static_cast<float32>(sz.x), H = static_cast<float32>(sz.y);
```

### Le rendu

**`Applications/Sandbox/src/DemoNkentseu/NkCanvas/NkCanvasDemo.cpp:139-170`**

```cpp
        // ── 5. Rendu : NkRenderer2D direct (Clear -> formes -> Display) ──────────
        target.Clear(NkColor2D{18, 20, 28, 255});
        NkRenderer2D &r = target.GetRenderer2D();

        // Rectangle "pulsant" au centre (remplissage + contour).
        const float32 hp = 60.f + 28.f * math::NkSin(t * 2.f);
        const NkRect2f box{cx - hp, cy - hp, hp * 2.f, hp * 2.f};
        r.DrawFilledRect(box, NkColor2D{52, 84, 150, 255});
        r.DrawRectOutline(box, NkColor2D{140, 180, 255, 255}, 2.f);

        // Triangle qui tourne autour du centre.
        const float32 tr = 95.f;
        const math::NkVec2f p0{cx + tr * math::NkCos(t), cy + tr * math::NkSin(t)};
        const math::NkVec2f p1{cx + tr * math::NkCos(t + 2.094f), cy + tr * math::NkSin(t + 2.094f)};
        const math::NkVec2f p2{cx + tr * math::NkCos(t + 4.188f), cy + tr * math::NkSin(t + 4.188f)};
        r.DrawFilledTriangle(p0, p1, p2, NkColor2D{255, 180, 60, 200});

        // Eventail de lignes depuis le coin haut-gauche.
        for (int32 i = 0; i < 7; ++i) {
            const float32 a = t * 0.6f + static_cast<float32>(i) * 0.22f;
            r.DrawLine(math::NkVec2f{0.f, 0.f},
                       math::NkVec2f{110.f + 90.f * math::NkCos(a), 110.f + 90.f * math::NkSin(a)},
                       NkColor2D{80, 200, 160, 180}, 1.5f);
        }

        // La balle, par-dessus (remplissage + contour clair).
        r.DrawFilledCircle(ball, R, NkColor2D{255, 110, 80, 255}, 48);
        r.DrawCircleOutline(ball, R, NkColor2D{255, 220, 200, 255}, 2.f, 48);

        target.Display();
    }
```

### Ce que ce programme raconte

Quelques observations qui valent pour toute application NkCanvas :

- **L'ordre de dessin est l'ordre de recouvrement.** La balle est
  dessinée en dernier, elle passe donc devant. Il n'y a ni tampon de
  profondeur, ni tri : ce qui est émis après recouvre.
- **Le temps est explicite.** `clock.Tick().delta` donne le
  temps écoulé depuis l'image précédente ; toute animation se calcule en
  « unités par seconde » multipliées par `dt`. Notez la borne
  `if (dt > 0.1f) dt = 1/60`, qui empêche un bond énorme quand la
  fenêtre a été bloquée (déplacement, mise en veille).
- **Aucune ressource n'est allouée dans la boucle.** La position de la
  balle est une variable locale à `nkmain` ; il n'y a ni
  `new`, ni conteneur créé par image.
- **On ne touche jamais à `Begin`/`End`.**
  `Clear` et `Display` suffisent.

## Récapitulatif des pièges du chapitre

1. Ne jamais appeler `Initialize` sur un renderer déjà initialisé
   par la fabrique (*rectangles creux*).
2. Détecter le redimensionnement sur la taille de la *fenêtre*, jamais
   sur celle de la *swapchain* (rendu basse résolution étiré).
3. Terminer l'image avant de recréer la swapchain (comportement indéfini
   sur Vulkan et DX12).
4. Ne jamais conserver un tampon local vers des données que le batcher lira
   plus tard (données parasites, formes creuses).
5. Chaque `SetClip` / `PopClip` coûte un vidage : découper
   largement, et filtrer soi-même le hors-vue.
6. Sous OpenGL, l'état est persistant entre les images : le *scissor*
   doit être explicitement désactivé au début de chaque image.
7. `SetFilter` / `SetWrap` sont sans effet sur DX12 et Vulkan ;
   `NkShader::Compile` ne fonctionne que sur OpenGL ;
   `NkRenderTexture` n'est réel que sur OpenGL.
8. `NkFont::GetKerning` retourne toujours zéro et le paramètre
   `bold` est ignoré.

## Exercices

> **✏️ 1 — Une horloge**
>
> Partez de `NkCanvasDemo`. Remplacez la scène par une horloge analogique :
> un `DrawCircleOutline` pour le cadran, douze `DrawLine` courts pour
> les graduations, trois `DrawLine` d'épaisseurs différentes pour les
> aiguilles. Faites tourner les aiguilles avec `t`. Contrainte : *aucune*
> allocation dans la boucle.

> **✏️ 2 — Compter les appels de dessin**
>
> Le batcher tient des statistiques (`mStats`, dont `textureSwap`).
> Trouvez comment y accéder depuis l'application, puis affichez leur valeur dans le
> journal une fois par seconde. Dessinez ensuite cent rectangles pleins :
> combien d'appels de dessin ? Ajoutez maintenant, *entre chaque rectangle*,
> une forme utilisant une autre texture. Que devient le compte ? Concluez.

> **✏️ 3 — Un dégradé sans primitive de dégradé**
>
> NkCanvas n'a pas de `DrawGradient`. Construisez-en un : remplissez un
> tableau de quatre `NkVertex2D` formant un rectangle, avec quatre couleurs
> différentes, un tableau de six indices, et appelez `DrawVertices` avec
> `texture = nullptr`. Vérifiez que le GPU interpole. Puis expliquez, en
> vous appuyant sur la section « la texture blanche de 1 pixel », pourquoi passer
> `nullptr` fonctionne.

> **✏️ 4 — Le découpage, et son coût**
>
> Dessinez une grille de 20 × 20 petits carrés. Version A : un seul
> `SetClip` autour de toute la grille. Version B : un `SetClip` et un
> `PopClip` autour de *chaque* carré. Comparez les statistiques du
> batcher et le temps par image. Vous devriez mesurer, à l'écran, la règle énoncée
> à la section 2.10.
