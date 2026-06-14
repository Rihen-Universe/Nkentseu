# Les ressources GPU

> Couche **Runtime** · NKCanvas · Les objets qui vivent **sur la carte graphique** : la texture
> `NkTexture`, le sprite `NkSprite`, le texte `NkText`, le shader `NkShader`, le matériau
> `NkMaterial`, la fonte `renderer::NkFont`, et les deux tables d'aiguillage backend
> `NkTextureBackend` / `NkShaderBackend`.

Dès qu'on veut **afficher autre chose que des rectangles unis** — une image, un caractère, un effet
de feu ou d'eau, un dégradé custom — il faut des données qui résident **sur le GPU** : un bloc de
pixels uploadé en mémoire vidéo, un programme compilé qui tourne sur les unités de calcul. NKCanvas
appelle ces objets des *ressources*. Le principe directeur, calqué sur SFML, tient en une idée :
**chaque ressource lourde (texture, shader, fonte) possède sa mémoire GPU, n'est donc pas copiable,
et se libère via un `Destroy()` appelé par son destructeur** ; tandis que les objets légers qui les
*utilisent* (sprite, texte, matériau) ne font que les **référencer sans les posséder** — la
ressource doit donc survivre au drawable qui la pointe.

La deuxième idée fondatrice est l'**autonomie vis-à-vis de NKRHI**. NKCanvas ne dépend d'aucune
abstraction GPU lourde : chaque backend renderer (OpenGL, Vulkan, DX11, DX12, Software) installe à
son `Initialize()` une **table de pointeurs de fonction** — `NkTextureSetBackend(...)`,
`NkShaderSetBackend(...)` — et les ressources passent par cette table sans jamais savoir quel
backend les sert. C'est ce qui rend `NkTexture` ou `NkShader` *backend-agnostiques* : le même code
applicatif tourne sur cinq API graphiques.

- **Namespace** : `nkentseu::renderer` (les forward-decls du module externe NKFont sont dans
  `nkentseu`)
- **Header pratique** : `#include "NKCanvas/Renderer/Resources/NkSprite.h"` tire à lui seul
  `NkTexture.h`, `NkFont.h`, `NkIRenderer2D.h`, `NkDrawable.h` et `NkRenderStates.h` — soit
  sprite + texte + texture + fonte d'un coup.

---

## La texture : `NkTexture`

C'est le bloc de pixels qui vit en mémoire vidéo, l'équivalent de `sf::Texture`. On la **charge**
depuis un fichier, depuis un buffer mémoire, ou depuis un `NkImage` déjà décodé ; en interne elle
décode l'image en RGBA8 puis **upload** ces pixels sur le GPU via la table backend. À partir de là,
elle n'est plus qu'un *handle* : un identifiant que les sprites, le texte et les shaders citeront
pour se faire texturer.

Comme toute ressource lourde, `NkTexture` est **non-copiable** (`=delete` sur la copie) — on ne
duplique pas une allocation GPU par mégarde — mais **movable** : on peut la transférer en `O(1)`
(transfert de propriété, la source devient vide). Son destructeur appelle `Destroy()`, qui libère
la ressource backend *seulement si la table backend est encore active* : après le `Shutdown` du
renderer, les `Destroy` deviennent des no-op inoffensifs.

```cpp
NkTexture tex;
tex.LoadFromFile(renderer, "assets/hero.png");   // décode → upload GPU
tex.SetFilter(NkTextureFilter::NK_NEAREST);  // pixel-art net (enum au scope nkentseu::renderer)
NkVec2f size = tex.GetSize();
```

Deux réglages de visuel : le **filtre** (`NK_NEAREST` = pixels nets, idéal pour le pixel-art ;
`NK_LINEAR` = interpolation lisse, défaut) et le **wrap** (`NK_CLAMP` par défaut, `NK_REPEAT`,
`NK_MIRROR_REPEAT`) selon que les UV débordent. Attention : `GenerateMipmap()` existe mais est
actuellement un **no-op** — pas de mipmaps automatiques.

Ce n'est **pas** un conteneur de pixels CPU général : une fois uploadée, relire les pixels via
`CopyToImage()` est une opération **lente** (download GPU→CPU). Pour les retouches en boucle, on
préfère `Update()` qui ne ré-upload qu'une **sous-région** (fast path, input RGBA8).

> **En résumé.** `NkTexture` = handle GPU de pixels, chargé depuis fichier/mémoire/`NkImage`,
> non-copiable mais movable, `Destroy()` au dtor. `SetFilter`/`SetWrap` règlent l'échantillonnage ;
> `Update` pour les retouches partielles ; `CopyToImage` est lent. `GetWhiteTexture` fournit une
> texture blanche 1×1 pour dessiner du non-texturé via le même chemin shader.

---

## Le sprite et le texte : `NkSprite`, `NkText`

`NkSprite` est un **quad texturé et transformable** — l'équivalent direct de `sf::Sprite`. Il ne
possède **pas** sa texture : il la **référence** (`const NkTexture*`), ce qui veut dire qu'on peut
faire pointer cent sprites sur une seule texture (un atlas), chacun affichant un `TextureRect`
différent. Il porte sa propre transformation 2D (position, rotation en degrés, échelle, origine),
une teinte, et des flips horizontaux/verticaux.

`NkText` est son cousin pour le **texte UTF-8** (l'équivalent de `sf::Text`), déclaré dans le **même
header** `NkSprite.h`. Il référence une `NkFont`, une chaîne, une taille de caractère, un style
(gras/italique/souligné…), une couleur de remplissage et un contour. En interne il génère un quad
texturé par glyphe à partir de l'atlas de la fonte.

```cpp
NkSprite hero(tex);
hero.SetPosition(120.f, 80.f);
hero.SetRotation(15.f);                 // degrés
hero.SetColor(NkColor2D::White);

NkText label(font, "Score : 0", 24);
label.SetFillColor(NkColor2D::Yellow);
label.SetOutlineColor(NkColor2D::Black);
label.SetOutlineThickness(1.f);
```

Tous deux **héritent de deux interfaces de dessin** : le `Draw(NkIRenderer2D&)` *legacy* et le
`Draw(NkRenderTarget&, NkRenderStates)` *moderne* à la SFML. C'est la seconde forme qui compose le
transform du drawable avec le transform parent et soumet la géométrie via le target.

Une **dissymétrie à connaître** : `NkSprite` expose tout l'outillage de transformation
(`SetScale(sx,sy)`, `SetOrigin(ox,oy)`, `Rotate(deg)`, `Scale(factor)`, `GetOrigin`), alors que
`NkText` n'expose **que** le sous-ensemble `SetPosition`/`SetRotation`/`SetScale(NkVec2f)`/
`SetOrigin(NkVec2f)`/`Move` — pas de `Scale(factor)`, `Rotate(deg)`, surcharges scalaires ni getter
`GetOrigin`.

> **En résumé.** `NkSprite` (quad texturé) et `NkText` (texte UTF-8) sont des objets **légers** qui
> référencent texture/fonte sans les posséder. Ils portent transform + teinte/couleur et se
> dessinent via deux interfaces (`NkIDrawable2D` legacy + `NkDrawable` moderne). `NkText` a une API
> de transform réduite par rapport à `NkSprite`.

---

## La fonte : `renderer::NkFont`

`renderer::NkFont` est un **wrapper GPU** du module externe NKFont — l'équivalent de `sf::Font`.
Depuis le refactor de 2026-05, NKCanvas ne réimplémente plus la rasterisation : il **délègue** à
`::nkentseu::NkFontAtlas` et `::nkentseu::NkFont` (dans le namespace `nkentseu`, à ne pas confondre
avec `renderer::NkFont`). Son travail est de prendre les glyphes rasterisés par le module et d'en
créer les **textures atlas sur le GPU**.

Le point central à comprendre : une **« page » = un atlas rasterisé à une taille de caractère
fixe + sa texture GPU**. La résolution dépend donc de `characterSize`, et **chaque taille demandée
alloue une nouvelle page/atlas/texture** (créée *lazy* au premier accès). Afficher le même texte en
16, 24 et 48 pixels coûte trois pages GPU — c'est un coût mémoire à anticiper.

```cpp
NkFont font;
font.LoadFromFile(renderer, "assets/Roboto.ttf");
float32 lh = font.GetLineHeight(24);
const NkGlyph& g = font.GetGlyph('A', 24);
const NkTexture* atlas = font.GetAtlasTexture(24);   // page GPU pour 24 px
```

Deux **limitations réelles** héritées du module : `GetKerning(...)` renvoie **toujours 0** (pas de
kerning par paire), et le paramètre `bold` de `GetGlyph` est **ignoré** — il n'y a pas de faux-gras,
il faut charger une fonte bold dédiée. Comme les autres ressources lourdes, `NkFont` est
non-copiable, movable, et libère ses pages au `Destroy()`.

Autour de la fonte gravitent quelques types : `NkGlyph` (sous-rect atlas en pixels, bbox locale,
advance) et `NkTextStyle` (énum **bitmask** : `NK_REGULAR`, `NK_BOLD`, `NK_ITALIC`, `NK_UNDERLINED`,
`NK_STRIKE_THROUGH`) avec ses helpers `operator|` et `HasStyle`.

> **En résumé.** `renderer::NkFont` est un wrapper GPU du module NKFont : une **page (atlas +
> texture) par taille de caractère**, créée à la demande. `GetKerning` renvoie 0, `bold` est ignoré
> (charger une fonte bold dédiée). `NkTextStyle` est un bitmask combinable par `|`.

---

## Les shaders programmables : `NkShader`

Quand les formes unies et les sprites texturés ne suffisent plus — on veut un effet de feu, d'eau,
un dégradé procédural, une déformation — on écrit un **shader**. `NkShader` est la ressource qui
porte ce programme user-écrit, dans l'esprit de `sf::Shader` ou des shaders de Godot. On l'attache à
un dessin via `NkRenderStates::shader`.

Le parti pris est **multi-langage et autonome de NKRHI** : il n'y a *ni* cross-compile NkSL, *ni*
SPIR-V généré ici. On fournit le source dans **le langage de chaque backend cible** (`SetSourceGLSL`,
`SetSourceHLSL`, `SetSourceMSL`, `SetSourceSPIRV`), et le backend actif **choisit la variante qui le
concerne** et ignore les autres. Si la source du backend courant manque, `Compile()` retourne
`false` et `IsValid()` reste faux — **pas de fallback**, l'app doit le détecter.

```cpp
NkShader fire;
fire.SetSourceGLSL(vsGLSL, fsFireGLSL);
fire.SetSourceHLSL(vsHLSL, fsFireHLSL);     // au cas où on tourne sur DX
if (!fire.Compile(renderer)) { /* IsValid()==false : variante absente */ }

fire.SetFloat("uTime", t);
fire.SetVec2 ("uResolution", res);
fire.SetTexture("uNoise", &noiseTex, 0);
```

Un **piège mémoire** important : `NkShader` ne **copie pas** ses sources — il stocke les pointeurs
ASCII/SPIRV tels quels jusqu'à `Compile`. L'application doit donc garder les chaînes (ou le binaire
SPIR-V) **vivantes** jusqu'à l'appel à `Compile`. Les uniformes (`SetFloat`/`SetVec2`/…/`SetMat4`/
`SetColor`/`SetTexture`) se posent **après** `Compile` et **avant** le dessin, identifiés par nom de
chaîne (résolu et caché par le backend). Il n'y a **pas d'introspection** : pas de moyen de lister
les uniformes du programme.

> **En résumé.** `NkShader` = programme user-écrit, multi-langage (GLSL/HLSL/MSL/SPIR-V), autonome
> de NKRHI. Le backend actif prend sa variante, ignore le reste, pas de fallback. Les sources ne
> sont **pas copiées** : gardez-les vivantes jusqu'à `Compile`. Uniformes par nom, après `Compile`,
> sans introspection.

---

## Le matériau : `NkMaterial`

Là où `NkShader` est la ressource GPU brute, `NkMaterial` est l'objet **pratique** qui l'enrobe,
dans l'esprit Godot : **un shader (référencé, non possédé) + N uniformes différés + des états GPU
par défaut** (blend mode, texture principale). Contrairement aux ressources lourdes, `NkMaterial`
est un **value object copiable et movable** — il ne contient que des handles non-owning, du POD et
un `NkVector`.

L'intérêt est de **préparer un look une fois** puis de l'appliquer à volonté. On accumule les
uniformes dans un side-buffer (`SetFloat`/`SetVec3`/`SetColor`/`SetTexture`…), on règle le blend et
la texture principale, et l'idiome de dessin est `target.Draw(drawable, mat.States())` :

```cpp
NkMaterial water;
water.SetShader(&waterShader);
water.SetMainTexture(&waterTex);
water.SetBlendMode(NkBlendMode::NK_ALPHA);
water.SetColor("uTint", NkColor2D(60, 120, 200, 255));
water.SetFloat("uTime", t);

target.Draw(quad, water.States());   // Apply() implicite + RenderStates prêt
```

Le mécanisme : `Apply()` **upload** tous les uniformes et textures accumulés sur le shader rattaché
(no-op si le shader est nul ou invalide), et `States()` appelle `Apply()` **implicitement** puis
renvoie un `NkRenderStates` rempli (blend + texture principale + shader, transform identité). Poser
deux fois le même uniforme (même *kind* + même nom) le **remplace**, sinon il s'ajoute.

Trois **limites documentées** : pas de hot-reload (recompiler `NkShader::Compile` à la main) ; pas
d'UBO (un appel par uniforme — UBO envisagé en v2) ; **application *lazy*** — si plusieurs materials
partagent un même shader sur la même frame, le **dernier `Apply` gagne** (les uniformes sont un état
GPU global par programme). Corollaire de durée de vie : le `NkShader` rattaché doit vivre au moins
aussi longtemps que le material.

> **En résumé.** `NkMaterial` = shader (non-owning) + uniformes différés + états GPU, **copiable**.
> `States()` applique implicitement les uniformes et renvoie un `NkRenderStates` prêt à dessiner.
> Pas de hot-reload, pas d'UBO, application *lazy* (dernier `Apply` gagne si shader partagé).

---

## Les tables d'aiguillage : `NkTextureBackend`, `NkShaderBackend`

C'est la **mécanique d'autonomie** de NKCanvas, normalement invisible à l'application — mais
essentielle à comprendre pour écrire un backend ou diagnostiquer un « rien ne s'affiche ». Une
*dispatch table* est une **struct de pointeurs de fonction** : chaque backend renderer la remplit
avec ses propres implémentations (glCompileShader, D3DCompile, upload de texture…) et l'**installe**
à la fin de son `Initialize()`. Les ressources appellent ensuite la table active sans rien savoir du
backend.

`NkTextureBackend` porte cinq callbacks — `Create(w,h,rgba)` (renvoie un id interne, **0 =
invalide**), `Update`, `Destroy`, `SetFilter`, `SetWrap` — et s'installe via
`NkTextureSetBackend(table)`. Convention dure : l'input pixel est **toujours RGBA8** (4 octets,
packed, non prémultiplié), pas de mipmaps auto, et les callbacks tournent sur le **thread du
contexte graphique** (pas de garantie cross-thread).

`NkShaderBackend` porte `Create(sources)` / `Destroy` / `Use` plus la famille `SetFloat`…`SetMat4`
et `SetTexture`, et s'installe via `NkShaderSetBackend(table)`. Le `Create` reçoit une struct
`NkShaderSources` qui contient **toutes les variantes** (glslVert/Frag, hlslVert/Frag, mslVert/Frag,
spirvVert/Frag + tailles), chaque pointeur pouvant être null ; il renvoie un id > 0 si OK, **0 si la
variante requise manque**.

```cpp
// Côté backend, à la fin de Initialize() :
NkTextureBackend tb;
tb.Create  = &MyGL_CreateTexture;
tb.Update  = &MyGL_UpdateTexture;
tb.Destroy = &MyGL_DestroyTexture;
tb.SetFilter = &MyGL_SetFilter;
tb.SetWrap   = &MyGL_SetWrap;
NkTextureSetBackend(tb);
```

Pour un backend **sans shader user-custom**, on n'écrit pas de stub à la main :
`NkShaderInstallUnsupportedBackend("NomDuBackend")` installe une table no-op dont le `Create` renvoie
0 (donc `Compile` renvoie false, détectable par `IsValid()`), le nom servant à un log unique.

> **En résumé.** Deux structs de pointeurs de fonction installées par chaque backend
> (`NkTextureSetBackend`/`NkShaderSetBackend`) découplent les ressources du GPU. **0 = id invalide**,
> texture toujours en RGBA8, thread du contexte graphique. `NkShaderInstallUnsupportedBackend` pose
> un stub no-op pour les backends sans shader custom.

---

## Aperçu de l'API

La liste de **tous** les éléments publics, en un coup d'œil. Chacun est détaillé dans la « Référence
complète » qui suit.

### `NkTexture` — handle GPU de texture

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `NkTexture()`, move ctor/assign, `~NkTexture()`, `Destroy` | Non-copiable, movable, dtor appelle `Destroy`. |
| Enums | `NkTextureFilter { NK_NEAREST, NK_LINEAR }`, `NkTextureWrap { NK_CLAMP, NK_REPEAT, NK_MIRROR_REPEAT }` | Échantillonnage et débordement d'UV. |
| Create / load | `Create(renderer,w,h,fill)`, `LoadFromFile`, `LoadFromImage(...,area)`, `LoadFromMemory` | Texture vide / fichier / `NkImage` / buffer image. |
| Update / download | `Update(pixels,w,h,dx,dy)`, `Update(image,dx,dy)`, `CopyToImage()` | Maj de sous-région (RGBA8) ; download GPU→CPU (lent). |
| Paramètres | `SetFilter`, `SetWrap`, `GetFilter`, `GetWrap`, `GenerateMipmap` | Filtre/wrap ; `GenerateMipmap` = **no-op**. |
| Info | `GetWidth`, `GetHeight`, `GetSize`, `IsValid`, `GetTexCoords(rect)` | Dimensions ; validité ; sous-rect pixel → UV. |
| Handle natif | `GetHandle`/`SetHandle`, `GetGPUId`/`SetGPUId` | Pointeur/nom backend (usage interne). |
| CPU | `GetCPUPixels`, `HasCPUPixels` | Accès aux pixels CPU si présents. |
| Static | `GetWhiteTexture(renderer)` | Singleton texture blanche 1×1 par renderer. |

### `NkSprite` — quad texturé transformable

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkSprite()`, `NkSprite(texture)`, `NkSprite(texture, rect)` | Vide / texture / texture + sous-rect. |
| Texture | `SetTexture(...,resetRect)`, `SetTextureRect`, `GetTexture`, `GetTextureRect` | Référence non-owning + zone d'atlas. |
| Transform | `SetPosition` (×2), `SetRotation(deg)`, `SetScale` (×2), `SetOrigin` (×2), `Move`, `Rotate(deg)`, `Scale`, getters, `GetTransform` | Transformation 2D complète. |
| Apparence | `SetColor`/`GetColor`, `SetFlipX`/`SetFlipY`, `GetFlipX`/`GetFlipY` | Teinte (défaut White) et flips. |
| Bounds | `GetLocalBounds`, `GetGlobalBounds` | Boîte locale / monde. |
| Draw | `Draw(NkIRenderer2D&)`, `Draw(NkRenderTarget&, NkRenderStates)` | Legacy / SFML-like. |

### `NkText` — texte UTF-8 drawable

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkText()`, `NkText(font, string, size=24)` | Vide / fonte + chaîne + taille. |
| Contenu | `SetString`, `SetFont`, `SetCharacterSize`, `SetStyle`, `SetLetterSpacing`, `SetLineSpacing`, getters | Texte, fonte, taille, style, espacements. |
| Transform | `SetPosition` (×2), `SetRotation(deg)`, `SetScale(NkVec2f)`, `SetOrigin(NkVec2f)`, `Move`, getters, `GetTransform` | Transform **réduit** (pas de surcharges scalaires). |
| Couleur | `SetFillColor`, `SetOutlineColor`, `SetOutlineThickness`, getters | Remplissage (White), contour (Black), épaisseur (0). |
| Bounds / hit-test | `GetLocalBounds`, `GetGlobalBounds`, `FindCharacterPos(point)` | Boîtes ; position d'un caractère. |
| Géométrie | `struct GlyphVertex { NkVertex2D v[4]; }`, `GetVertices()` | Quads par glyphe (cache lazy). |
| Draw | `Draw(NkIRenderer2D&)`, `Draw(NkRenderTarget&, NkRenderStates)` | Legacy / SFML-like. |

### `NkShader` — programme GPU user-écrit

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `NkShader()`, move ctor/assign, `~NkShader()`, `Destroy` | Non-copiable, movable, dtor appelle `Destroy`. |
| Enum | `NkShaderStage { NK_VERTEX, NK_FRAGMENT }` | Étages (pas de geometry/compute). |
| Sources | `SetSourceGLSL`, `SetSourceHLSL`, `SetSourceMSL`, `SetSourceSPIRV` | Source par langage backend (≥ 1 avant `Compile`). |
| Compile | `Compile(renderer)`, `IsValid`, `GetGPUId` | Compile + link via backend ; pas de fallback. |
| Uniformes | `SetFloat`, `SetVec2`, `SetVec3`, `SetVec4`, `SetColor`, `SetMat4` (×2), `SetTexture(...,slot)` | Par nom, après `Compile`, sans introspection. |

### `NkMaterial` — matériau 2D copiable

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `NkMaterial()`, copy/move (`= default`), `~NkMaterial()` | **Value object** copiable et movable. |
| Shader | `SetShader`, `GetShader` (×2) | Shader référencé non-owning. |
| États GPU | `SetBlendMode`/`GetBlendMode`, `SetMainTexture`/`GetMainTexture` | Blend (défaut `NK_ALPHA`) + texture slot 0. |
| Uniformes | `SetFloat`, `SetVec2`, `SetVec3`, `SetVec4`, `SetColor`, `SetMat4`, `SetTexture(...,slot)` | Accumulés en side-buffer (remplace si même nom). |
| Application | `Apply()`, `States()` | Upload des uniformes ; `RenderStates` prêt (Apply implicite). |

### `renderer::NkFont` — wrapper GPU de fonte

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `NkFont()`, move ctor/assign, `~NkFont()`, `Destroy`, `IsValid` | Non-copiable, movable, dtor appelle `Destroy`. |
| Load | `LoadFromFile(renderer, path)`, `LoadFromMemory(renderer, data, size)` | Le renderer crée les textures atlas. |
| Glyphes | `GetGlyph(cp, size, bold=false)`, `GetKerning(...)`, `GetLineHeight(size)`, `GetAtlasTexture(size)` | Glyphe (`bold` ignoré), kerning (**= 0**), hauteur, page GPU. |
| Types | `struct NkGlyph`, `enum NkTextStyle`, `operator\|`, `HasStyle` | Métriques de glyphe ; style bitmask. |

### Tables d'aiguillage backend

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Texture | `struct NkTextureBackend` (`Create`/`Update`/`Destroy`/`SetFilter`/`SetWrap`) | Callbacks GPU de texture (RGBA8, id 0 = invalide). |
| Texture | `NkTextureSetBackend(table)` | Installe la table de texture active. |
| Shader | `struct NkShaderSources` | Toutes les variantes de source (GLSL/HLSL/MSL/SPIR-V). |
| Shader | `struct NkShaderBackend` (`Create`/`Destroy`/`Use`/`Set*`/`SetTexture`) | Callbacks GPU de shader. |
| Shader | `NkShaderSetBackend(table)`, `NkShaderInstallUnsupportedBackend(name)` | Installe la table active / un stub no-op. |

---

## Référence complète

Chaque élément est repris ici en détail, avec ses usages à travers les domaines du temps réel —
rendu, ECS, gameplay/IA, animation, UI/2D, IO, outils/éditeur. La prose est à puces, pas en
tableaux.

### `NkTexture` à fond

La texture est la brique de tout affichage non-uni. Le cycle est toujours le même : **charger →
régler → utiliser → laisser le dtor libérer**.

- **Chargement.** `LoadFromFile` décode un fichier disque ; `LoadFromMemory` décode un fichier image
  déjà en RAM (utile pour un asset embarqué ou téléchargé) ; `LoadFromImage` upload un `NkImage`
  déjà décodé, avec un `area` optionnel pour ne prendre **qu'une sous-région** (découper un atlas en
  plusieurs textures). `Create(renderer, w, h, fill)` fabrique une texture vide remplie d'une
  couleur — point de départ d'une texture qu'on remplira par `Update`.
- **Mise à jour dynamique.** `Update(pixels, w, h, destX, destY)` réécrit une **sous-région** sans
  ré-uploader tout — le fast path pour : une **caméra/vidéo** dont seule l'image change, une **mini-
  carte** qu'on repeint par tuiles, un **canvas de peinture** (outil/éditeur) où l'utilisateur
  modifie un coin. L'input est du RGBA8. `Update(image, ...)` fait pareil depuis un `NkImage`.
- **Relecture.** `CopyToImage()` ramène les pixels du GPU vers le CPU — **lent**, à réserver aux
  captures d'écran, à l'export d'un rendu (outils/éditeur) ou au débogage, jamais en boucle chaude.
- **Échantillonnage.** `SetFilter(NK_NEAREST)` pour du **pixel-art** net ou une UI à pixels carrés ;
  `NK_LINEAR` (défaut) pour des photos/sprites lissés. `SetWrap` choisit le comportement hors [0,1] :
  `NK_REPEAT` pour une **texture de fond tuilée** (sol, ciel défilant), `NK_MIRROR_REPEAT` pour un
  motif sans couture, `NK_CLAMP` (défaut) pour une image isolée.
- **Info & UV.** `GetSize`/`GetWidth`/`GetHeight` donnent les dimensions ; `IsValid` dit si la
  ressource GPU existe ; `GetTexCoords(rect)` convertit un **sous-rect en pixels** en **UV
  normalisés** — la clé pour piloter un atlas (sprite-sheet d'animation, glyphes, tuiles).
- **Handle natif.** `GetHandle`/`GetGPUId` (et leurs setters) exposent le pointeur ou le nom backend
  (VkImage, `ID3D11Texture2D*`, texture name OpenGL) — réservés à l'écriture d'un backend ou à
  l'interop avec une autre couche GPU, jamais au code applicatif normal.
- **Texture blanche.** `GetWhiteTexture(renderer)` renvoie un **singleton 1×1 blanc** par renderer :
  il permet de dessiner de la géométrie **non-texturée** (rectangles, lignes, formes) en passant par
  le **même chemin shader** que le texturé (texture × couleur), ce qui simplifie le *batching*.

### `NkSprite` et `NkText` à fond

Ce sont les **drawables légers** : ils ne possèdent aucune ressource GPU, ils citent une texture ou
une fonte et portent une transformation. Leur force est le **partage** (mille sprites sur un atlas)
et la **double interface de dessin**.

- **Référence non-owning.** Le sprite tient `const NkTexture*`, le texte un `const NkFont*`. C'est
  un piège classique : si la ressource meurt avant le drawable, on dessine un handle mort. La
  texture/fonte doit **survivre** au sprite/texte.
- **Atlas et sprite-sheets.** `SetTextureRect` choisit la zone d'atlas affichée : on anime un
  personnage en faisant défiler le rect frame par frame (animation), on pioche une icône dans une
  feuille d'UI (UI/2D), on rend une tuile de tileset (gameplay/level).
- **Transform 2D.** Position, rotation (en **degrés** côté API, convertie en radians en interne),
  échelle, origine — l'origine sert de **pivot** (centrer la rotation d'une roue, ancrer un texte en
  haut-gauche). `Move`/`Rotate`/`Scale` appliquent un delta relatif (utile en animation
  incrémentale). `GetTransform()` donne le `NkTransform2D` composé.
- **Apparence du sprite.** `SetColor` **teinte** le sprite (flash de dégât en rouge en gameplay,
  fondu en alpha pour une transition UI, recoloration d'un sprite générique) ; `SetFlipX`/`SetFlipY`
  retournent l'image (un personnage qui change de sens de marche sans seconde texture).
- **Contenu du texte.** `SetString` (UTF-8), `SetCharacterSize`, `SetStyle` (bitmask gras/italique/
  souligné…), `SetLetterSpacing` et `SetLineSpacing` (multiplicateurs sur l'advance et la hauteur de
  ligne) règlent la typographie d'un HUD, d'un sous-titre, d'un panneau d'éditeur.
- **Couleur du texte.** `SetFillColor` (remplissage), `SetOutlineColor` + `SetOutlineThickness`
  (contour) — un contour noir rend un score lisible par-dessus n'importe quel décor.
- **Bounds & hit-test.** `GetLocalBounds`/`GetGlobalBounds` donnent la boîte avant/après transform
  (layout UI, détection de survol, culling). `NkText::FindCharacterPos(point)` mappe un point écran
  vers l'index d'un caractère — la base d'un **curseur de saisie** ou d'une **sélection de texte**
  dans un éditeur.
- **Géométrie exposée du texte.** `GetVertices()` (vecteur de `GlyphVertex`, 4 vertices par glyphe,
  cache *lazy* régénéré sur `mGeometryDirty`) permet à un outil de récupérer directement les quads —
  pour un effet custom, un export, ou un rendu maison.
- **Les deux `Draw`.** Le `Draw(NkIRenderer2D&)` *legacy* dessine directement ; le
  `Draw(NkRenderTarget&, NkRenderStates)` *moderne* (hérité de `NkDrawable`) **compose** le transform
  du drawable avec le transform parent passé dans `states` puis soumet via le target — c'est la voie
  SFML qui permet d'imbriquer des transformations (un sprite enfant dans un nœud parent).
- **Dissymétrie d'API.** `NkSprite` a `Scale(factor)`, `Rotate(deg)`, `SetScale(sx,sy)`,
  `SetOrigin(ox,oy)` et `GetOrigin` ; `NkText` ne les a **pas** — il se limite à
  `SetScale(NkVec2f)`/`SetOrigin(NkVec2f)`/`SetRotation(deg)`/`Move`. À garder en tête lors d'un
  portage de code de sprite vers texte.

### `renderer::NkFont` à fond

La fonte est le **fournisseur de glyphes** texturés. Elle ne dessine rien elle-même : `NkText` lui
demande des glyphes et l'atlas correspondant.

- **Chargement.** `LoadFromFile`/`LoadFromMemory` passent par le module NKFont pour la rasterisation,
  puis demandent au **renderer** de créer les textures atlas GPU. `IsValid` reflète simplement la
  présence des données fonte en mémoire.
- **Pages par taille.** Le point structurant : `GetGlyph(cp, size)` et `GetAtlasTexture(size)`
  travaillent à une **taille donnée**, et chaque taille jamais vue déclenche la création *lazy*
  d'une page (atlas + texture GPU). Afficher un menu en 18 px, des titres en 48 px et des infobulles
  en 12 px = trois pages. Pour un éditeur ou un jeu qui zoome, **limiter le nombre de tailles
  distinctes** maîtrise la mémoire GPU.
- **Métriques.** `NkGlyph` porte le `textureRect` (sous-rect atlas en pixels, pour piocher dans la
  texture), les `bounds` (bearing + taille pour le placement) et l'`advance` (avance horizontale
  jusqu'au glyphe suivant). `GetLineHeight(size)` donne l'interligne — la base de tout layout de
  texte multi-ligne.
- **Limites héritées.** `GetKerning(...)` renvoie **toujours 0** : pas d'ajustement par paire de
  caractères (le rendu reste correct, juste un peu moins serré sur des paires comme « AV »). Le
  `bold` de `GetGlyph` est **ignoré** : pour du gras, charger une **fonte bold dédiée** comme une
  seconde `NkFont`.
- **Style.** `NkTextStyle` est un **bitmask** (`NK_REGULAR`, `NK_BOLD`, `NK_ITALIC`, `NK_UNDERLINED`,
  `NK_STRIKE_THROUGH`) qu'on combine par `operator|` et qu'on teste par `HasStyle(s, f)` — c'est
  `NkText::SetStyle` qui le consomme.

### `NkShader` à fond

Le shader est la **porte vers les effets custom**, et toute sa subtilité tient dans son modèle
multi-langage autonome.

- **Sources multi-backend.** On appelle un ou plusieurs `SetSourceXXX` selon les API qu'on veut
  supporter : `SetSourceGLSL` (OpenGL 3.3+/GLES 3.0+), `SetSourceHLSL` (DX11 sm5 / DX12 sm5_1),
  `SetSourceMSL` (Metal 2.0+, **non livré** — pas de renderer Metal), `SetSourceSPIRV` (binaire
  Vulkan pré-compilé hors-engine via glslangValidator/shaderc). Au `Compile`, le backend actif prend
  **sa** variante et ignore les autres.
- **Pas de fallback.** Si on tourne sur DX et qu'on n'a fourni que du GLSL, `Compile` renvoie `false`
  et `IsValid()` reste faux — l'app **doit** tester et prévoir un comportement de repli (rendu sans
  shader, message). C'est un choix assumé : pas de cross-compile silencieux.
- **Piège de durée de vie des sources.** `NkShader` **ne copie pas** les chaînes ni le SPIR-V : il
  garde les pointeurs jusqu'à `Compile`. Stocker la source dans un buffer temporaire détruit avant
  l'appel = comportement indéfini. On garde les sources vivantes (string littérale, membre, asset
  chargé) jusqu'au `Compile` inclus.
- **Uniformes.** Après `Compile` et avant le dessin, on pousse les paramètres par nom :
  `SetFloat` (temps, force d'effet), `SetVec2`/`SetVec3`/`SetVec4` (résolution, direction, point),
  `SetColor` (RGBA normalisé [0..1]), `SetMat4` (depuis un `NkTransform` **ou** 16 floats
  column-major), `SetTexture(name, tex, slot)` (texture de bruit, lookup, masque). Le backend résout
  et cache le nom.
- **Cas d'usage.** Effets d'eau/feu/portail (rendu), transitions plein écran (UI/2D), distorsion de
  chaleur, *outline* de sélection (outils/éditeur), coloration procédurale d'un terrain (gameplay).
  L'énum `NkShaderStage` se limite à `NK_VERTEX`/`NK_FRAGMENT` — pas de geometry ni de compute ici.

### `NkMaterial` à fond

Le matériau est la **couche de confort** au-dessus du shader, pensée pour réutiliser un look.

- **Value object.** Contrairement aux ressources lourdes, `NkMaterial` se **copie** librement (que
  des handles non-owning + POD + `NkVector`). On peut donc avoir un `NkMaterial` membre d'un
  component ECS, le copier dans une variante, le passer par valeur — sans gérer de propriété GPU.
- **Composition.** `SetShader` rattache le programme (non possédé — il doit vivre ≥ le material),
  `SetMainTexture` pose la texture du slot 0, `SetBlendMode` choisit le mélange (défaut `NK_ALPHA`).
  Les uniformes (`SetFloat`/…/`SetColor`/`SetTexture`) s'accumulent dans un side-buffer : poser deux
  fois le même *kind*+nom **remplace**, sinon **ajoute** (`SetColor` divise par 255 pour donner un
  vec4 [0..1]).
- **Application.** `Apply()` upload tout l'accumulé sur le shader (no-op si shader nul/invalide).
  `States()` fait l'`Apply()` **implicitement** puis renvoie un `NkRenderStates` rempli (blend,
  texture principale, shader, transform identité) — d'où l'idiome `target.Draw(drawable,
  mat.States())`. Très pratique en gameplay (un material « surligné » appliqué à tout objet
  sélectionné) ou en UI (un material de thème partagé).
- **Lazy = dernier gagne.** Comme les uniformes sont un **état GPU global par programme**, si deux
  materials partagent le même shader et sont appliqués dans la même frame, le **dernier `Apply`**
  écrase. Si on a besoin de deux looks distincts du même shader simultanément, séparer les passes ou
  dupliquer le shader.
- **Limites.** Pas de hot-reload (recompiler `NkShader::Compile` à la main après édition) ; pas
  d'UBO — un appel GPU par uniforme (UBO envisagé en v2, ce qui pèse si on a beaucoup d'uniformes par
  frame).

### Les tables d'aiguillage à fond

C'est l'infrastructure qui rend tout le reste **backend-agnostique** — invisible à l'app, centrale
pour qui écrit un backend ou débogue un affichage muet.

- **Principe.** Chaque renderer (OpenGL/Vulkan/DX11/DX12/Software) remplit une struct de pointeurs de
  fonction et l'**installe** à la fin de son `Initialize()` : `NkTextureSetBackend(tb)` et
  `NkShaderSetBackend(sb)`. Les ressources (`NkTexture`, `NkShader`) appellent ensuite la table
  active sans connaître le backend — d'où l'autonomie vis-à-vis de NKRHI.
- **`NkTextureBackend`.** Cinq callbacks : `Create(w,h,rgba)` (renvoie un id interne — texture name
  GL, index Software, slot SRV DX12 ; **0 = invalide**), `Update(id,x,y,w,h,rgba)`, `Destroy(id)`,
  `SetFilter`, `SetWrap`. Conventions dures : input **RGBA8** (4 octets packed, non prémultiplié),
  **pas de mipmaps auto**, callbacks sur le **thread du contexte graphique** (pas de cross-thread).
- **`NkShaderBackend` et `NkShaderSources`.** `NkShaderSources` agrège **toutes** les variantes
  (`glslVert/Frag`, `hlslVert/Frag`, `mslVert/Frag`, `spirvVert/Frag` + tailles en mots), chaque
  pointeur pouvant être null. `NkShaderBackend::Create(sources)` compile+linke et renvoie un id > 0
  (ou **0** si la variante requise manque) ; `Use(id)` active le programme (appelé par
  `NkRenderTarget` juste avant un submit batch quand `states.shader != nullptr`) ;
  `SetFloat`…`SetMat4` et `SetTexture(id, name, texGPUId, slot)` poussent les uniformes (le
  `texGPUId` est l'id renvoyé par `NkTextureBackend::Create`).
- **Installation et nettoyage.** `NkTextureSetBackend`/`NkShaderSetBackend` **remplacent** la table
  précédente ; un `Initialize` successif écrase. À l'idéal, après `Shutdown`, réinstaller une **table
  vide** (callbacks null) pour que les `Destroy` tardifs deviennent no-op (protection présente dans
  `NkTexture::Destroy`).
- **Stub no-op.** `NkShaderInstallUnsupportedBackend(name)` installe une table dont le `Create`
  renvoie 0 — pour un backend qui **ne supporte pas** les shaders user-custom. `Compile` renverra
  donc false, `IsValid()` restera faux, et `name` sert à un log unique. C'est la façon propre de
  signaler « pas de shader custom ici » sans planter.
- **Diagnostic.** Le symptôme « rien ne s'affiche / `IsValid()==false` / id 0 » a presque toujours la
  même cause : la table backend **n'a pas été installée** (`Create`/`Compile` appelé avant
  l'`Initialize` du renderer) ou la **variante de source du backend actif manque**.

### Le socle commun

- **Non-copiable / movable / `Destroy` au dtor.** Les ressources lourdes (`NkTexture`, `NkShader`,
  `renderer::NkFont`) suivent toutes le pattern Create/Destroy : copie supprimée, déplacement
  autorisé, destructeur qui libère. Conforme à la règle dure NKMemory (toute classe avec une
  ressource a son `Destroy`). Voir [NKMemory](../../Foundation/NKMemory.md).
- **Référence non-owning des objets légers.** `NkSprite`/`NkText` (texture/fonte) et `NkMaterial`
  (shader/texture) **pointent** vers les ressources sans les posséder — la ressource doit vivre plus
  longtemps que l'objet qui la cite.
- **Dispatch table avant tout Create.** Aucune ressource GPU ne fonctionne tant que la table backend
  correspondante n'est pas installée par un renderer initialisé.
- **Dépendances de types.** Les ressources reposent sur le module **NKImage** (pixels RGBA8) et sur
  `NkVertex2D`/`NkTransform2D`/`NkColor2D`/`NkBlendMode` de `NkRenderer2DTypes.h`/`NkRenderStates.h`.
- **Header pratique.** Inclure `NkSprite.h` tire `NkTexture.h` + `NkFont.h` + `NkIRenderer2D.h` +
  `NkDrawable.h` + `NkRenderStates.h` ; `NkMaterial.h` tire `NkRenderStates.h` + `NkShader.h` ;
  `NkFont.h` tire `NkTexture.h`.

---

### Exemple

```cpp
#include "NKCanvas/Renderer/Resources/NkSprite.h"     // sprite + text + texture + font
#include "NKCanvas/Renderer/Resources/NkMaterial.h"   // material + shader
using namespace nkentseu::renderer;

// 1) Texture + sprite : on charge un atlas et on en pioche une frame.
NkTexture atlas;
atlas.LoadFromFile(renderer, "assets/hero_sheet.png");
atlas.SetFilter(NkTextureFilter::NK_NEAREST);  // pixel-art net

NkSprite hero(atlas);
hero.SetTextureRect(NkRect2i{ 0, 0, 32, 32 });           // frame 0
hero.SetOrigin(16.f, 16.f);                              // pivot au centre
hero.SetPosition(200.f, 150.f);
hero.SetRotation(10.f);                                  // degrés

// 2) Fonte + texte : un HUD avec contour.
NkFont font;
font.LoadFromFile(renderer, "assets/Roboto.ttf");
NkText score(font, "Score : 1280", 24);
score.SetFillColor(NkColor2D::White);
score.SetOutlineColor(NkColor2D::Black);
score.SetOutlineThickness(1.f);

// 3) Shader + material : un effet d'eau réutilisable.
NkShader waterShader;
waterShader.SetSourceGLSL(vsGLSL, fsWaterGLSL);
if (waterShader.Compile(renderer)) {
    NkMaterial water;
    water.SetShader(&waterShader);
    water.SetMainTexture(&atlas);
    water.SetFloat("uTime", time);
    target.Draw(hero, water.States());                  // Apply() implicite
}

// 4) Dessin SFML-like classique.
target.Draw(hero, NkRenderStates{});
target.Draw(score, NkRenderStates{});
```

---

[← Index NKCanvas](README.md) · [Récap NKCanvas](../NKCanvas.md) · [Couche Runtime](../README.md)
