# Dessiner en 2D

> Couche **Runtime** · NKCanvas · La couche **SFML-like** du moteur : le renderer `NkRenderer2D`,
> les objets dessinables `NkDrawable` / `NkTransformable`, le transform affine `NkTransform`, le
> tableau de sommets `NkVertexArray`, et le batcher partagé qui nourrit les 5 backends GPU.

Tout ce qui apparaît à l'écran en deux dimensions — une interface, un sprite, un éditeur de niveau,
le HUD d'un jeu, un graphe de débogage — finit par passer par **un renderer 2D**. La question n'est
jamais « comment poser un pixel » (le GPU le fait), mais « comment exprimer une scène 2D **de façon
portable** » : un même code qui rend identique sur OpenGL, Vulkan, DirectX 11, DirectX 12 et le
rasterizer logiciel. NKCanvas répond exactement comme **SFML** : on **transforme** des objets
(`NkTransformable`), on les rend via une **cible** qui compose un **état** (`NkRenderStates`), et un
**batcher** regroupe tout en un minimum de draw calls. Cette page vous apprend à dessiner.

Le cœur du compromis tient en une phrase : **on accumule la géométrie côté CPU, on la groupe par
(texture, blend), puis on la soumet en bloc** — un dessin immédiat naïf (un draw call par rectangle)
écroulerait n'importe quel GPU, le batching transforme des milliers d'objets en une poignée
d'appels. Le reste de l'API n'est qu'une façon ergonomique d'alimenter ce batcher.

- **Namespace** : `nkentseu::renderer` (sauf `NkGraphicsApi`, alias de `nkentseu::graphics`)
- **Header racine des types** : `#include "NKCanvas/Renderer/Core/NkRenderer2DTypes.h"`

---

## Le renderer : `NkRenderer2D` et son interface

`NkRenderer2D` est la **façade** publique qu'on manipule au quotidien (décision d'architecture
2026-05-29) : elle wrappe un backend `NkIRenderer2D*` et une cible `NkRenderTarget*`, **sans rien
posséder** — c'est une *projection* dont la durée de vie suit le target englobant. Tous ses appels
sont forwardés *inline* au backend, donc aucun coût de virtualisation supplémentaire. On préfère
toujours `NkRenderer2D` à l'interface brute `NkIRenderer2D`, qui n'est qu'un détail
d'implémentation partagé par les 5 backends.

Le cycle d'une frame est immuable, calqué sur SFML : `Begin()` ouvre l'accumulation, on enchaîne
les `Clear` / `Draw...`, et `End()` *flush* le batch puis le soumet. Entre les deux, on pose la
**vue** (`SetView`, une caméra orthographique), on règle le **blend** (`SetBlendMode`), on
restreint le rendu à un rectangle (`SetClip` / `PopClip`), et on dessine — soit des **primitives**
prêtes à l'emploi (`DrawFilledRect`, `DrawCircle`, `DrawLine`…), soit des **objets** dessinables.

```cpp
if (renderer.Begin()) {
    renderer.Clear(NkColor2D::Black);
    renderer.DrawFilledRect({ 10, 10, 200, 80 }, NkColor2D::Blue);
    renderer.DrawCircle({ 400, 300 }, 50.f, NkColor2D::Red);
    renderer.End();
}
```

Ce n'est **pas** un mode *retained* : rien n'est mémorisé entre deux frames, on redécrit la scène à
chaque tour de boucle (immediate-mode). Et ce n'est **pas** un renderer 3D : pas de profondeur, pas
de matériaux PBR — pour ça, on monte d'un cran vers NKRenderer. La façade tolère un backend `null`
(tous les appels deviennent des no-op, les getters renvoient des valeurs par défaut) : on appelle
`IsValid()` avant de s'en servir.

> **En résumé.** `NkRenderer2D` = la façade SFML-like, non-propriétaire, forwardée *inline* au
> backend. Une frame = `Begin()` → `Clear`/`SetView`/`Draw...` → `End()`. Immediate-mode, 2D pur,
> portable sur 5 backends. Vérifiez `IsValid()`, créez via `NkRenderer2DFactory` (jamais `new`).

---

## Transformer : `NkTransform` et `NkTransformable`

Avant de dessiner, il faut **placer**. Deux types s'en chargent, à ne pas confondre. `NkTransform`
est la **matrice affine 2D** elle-même (équivalent `sf::Transform`) : un wrapper sur `NkMat4f`
column-major dont les opérations (`Translate`, `Rotate`, `Scale`, `Combine`) mutent l'objet et se
**composent**. La règle de composition est celle de l'algèbre matricielle : `T = A * B` applique
**B d'abord, puis A** (`v' = A·(B·v)`) — d'où l'idiome parent/enfant `world = parent * local`.

`NkTransformable` est la **classe de base** (équivalent `sf::Transformable`) qu'on **hérite** pour
donner à un objet une position, une rotation, un scale et une **origine** (le pivot local). Elle ne
stocke pas une matrice mais ces quatre attributs, et **calcule** la matrice à la demande selon le
modèle SFML `T = Translate(position) · Rotate(rotation) · Scale(scale) · Translate(-origin)`. Le
résultat est **mis en cache** : `GetTransform()` ne recalcule que si un setter a marqué l'objet
*dirty*, sinon c'est `O(1)`.

```cpp
class Player : public NkTransformable, public NkDrawable { /* ... */ };

Player p;
p.SetOrigin({ 32, 32 });        // pivot au centre d'un sprite 64x64
p.SetPosition({ 400, 300 });
p.Rotate(0.05f);                // incrémental : ajoute 0.05 rad à la rotation
```

Le piège classique : `NkTransformable::Rotate/Scale` sont **incrémentaux** (ils *ajoutent* au state
courant et marquent dirty), tandis que `NkTransform::Rotate/Scale` *retournent un `NkTransform&`* et
composent une matrice. Ce ne sont **pas** les mêmes opérations malgré le nom partagé.

> **En résumé.** `NkTransform` = la **matrice** affine 2D, composable (`A * B` = B puis A).
> `NkTransformable` = la **base** position/rotation/scale/origin, à hériter, qui calcule sa matrice
> en cache. `Set...` pose, `Move/Rotate/Scale` (sur `NkTransformable`) ajoutent. `SetOrigin` choisit
> le pivot.

---

## Décrire de la géométrie : `NkVertexArray` et `NkRenderStates`

Pour les formes que les primitives ne couvrent pas — un trait épais, un dégradé, un maillage 2D
arbitraire — on descend au niveau du **sommet**. Un `NkVertex` (alias SFML-friendly de
`NkVertex2D`) est un POD minimal : position `(x, y)`, texcoords `(u, v)`, couleur `(r, g, b, a)`. Un
`NkVertexArray` est un **tableau dynamique** de ces sommets accompagné d'un `NkPrimitiveType` (points,
lignes, triangles, *strips*, *fans*) — l'équivalent exact de `sf::VertexArray`. C'est de la
**composition** sur `NkVector`, pas un héritage privé.

L'autre brique est `NkRenderStates` (équivalent `sf::RenderStates`) : le conteneur des **quatre
états** d'un draw call — un `NkTransform`, un `NkBlendMode`, une `NkTexture*`, un `NkShader*`.
C'est ce qu'on passe au moment du rendu pour dire « dessine ceci, en l'ayant transformé comme ça,
en mélangeant comme ça, avec cette texture ». Attention : les cinq constructeurs sont **`explicit`
et mutuellement exclusifs** (chacun ne pose qu'un seul champ) ; pour combiner plusieurs états, on
construit puis on assigne les champs un par un.

```cpp
NkVertexArray strip(NkPrimitiveType::NK_TRIANGLE_STRIP, 4);
strip[0] = { 0, 0, 0, 0, 255, 0, 0, 255 };   // rouge
strip[3] = { 64, 64, 1, 1, 0, 0, 255, 255 }; // bleu  → dégradé par interpolation
NkRect2f box = strip.GetBounds();            // AABB des positions
```

Ce n'est **pas** au programmeur de gérer le buffer GPU : `NkVertexArray` vit côté CPU, le batcher
s'occupe de l'uploader. Et `NkShader` est pour l'instant **forward-déclaré mais non implémenté**
dans NKCanvas — c'est un placeholder, le champ existe mais le pipeline custom viendra plus tard.

> **En résumé.** `NkVertex` = sommet POD (pos + uv + couleur). `NkVertexArray` = tableau de sommets
> + un type de primitive, côté CPU, pour la géométrie arbitraire. `NkRenderStates` = les 4 états
> d'un draw (transform/blend/texture/shader), aux ctors `explicit` exclusifs. `NkShader` =
> placeholder non implémenté.

---

## Aperçu de l'API

Tous les éléments publics, regroupés par fichier. Le détail (cas d'usage par domaine) suit dans la
« Référence complète ».

### `NkRenderer2DTypes.h` — types partagés

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Alias couleur/vec | `NkColor2D`, `NkVec2i`, `NkVec2f` | Re-exports NKMath (RGBA 0-255, vecteurs 2D). |
| Alias rectangle | `NkRect2i` / `NkRect2f` / `NkRect2u` / `NkRect2d` | Rectangles int (clip/viewport) / float (géométrie) / uint / double. |
| Alias sommet | `NkVertex` = `NkVertex2D` | Nom SFML-friendly du POD sommet. |
| Transform POD | `NkTransform2D` (`position`, `rotation`, `scale`, `origin`, `ToMatrix4`) | Transform 2D simple, écrit une matrice `float[16]`. |
| Caméra | `NkView2D` (`center`, `size`, `rotation`, `ToProjectionMatrix`) | Caméra orthographique 2D. |
| Blend | `NkBlendMode` : `NK_ALPHA`, `NK_ADD`, `NK_MULTIPLY`, `NK_NONE` | Mode de mélange. |
| Primitive | `NkPrimitiveType` : `NK_POINTS`/`LINES`/`LINE_STRIP`/`TRIANGLES`/`TRIANGLE_STRIP`/`TRIANGLE_FAN` | Type de primitive (pas de QUADS). |
| Sommet | `NkVertex2D` (`x,y`, `u,v`, `r,g,b,a`) | POD sommet. |
| Stats | `NkRenderStats2D` (`drawCalls`, `vertexCount`, `indexCount`, `textureSwap`) | Compteurs d'une frame. |

### `NkTransform` — matrice affine 2D

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkTransform()`, `NkTransform(a00..a12)`, `static Identity()` | Identité / 6 composantes affines / singleton identité. |
| Accès matrice | `GetMatrix()` (`float*` 16), `GetMatrix4()` (`NkMat4f&`) | Upload GPU / interop NKRenderer-NKCamera. |
| Opérations | `Combine`, `Translate`, `Rotate(a)`, `Rotate(a, c)`, `Scale(f)`, `Scale(f, c)` | Composent dans `*this`, retournent `NkTransform&`. |
| Application | `TransformPoint(p)`, `TransformRect(r)` | Transforme un point / AABB des 4 coins. |
| Inverse | `GetInverse()` | Inverse affine 2D (`Identity()` si dégénéré). |
| Opérateurs | `*`, `*=`, `* p`, `==`, `!=` | Composition, point, égalité stricte 16 floats. |

### `NkTransformable` — base position/rotation/scale/origin

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Setters | `SetPosition`, `SetRotation`, `SetScale`, `SetOrigin` (vec ou x,y) | Posent l'attribut, marquent dirty. |
| Getters | `GetPosition`, `GetRotation`, `GetScale`, `GetOrigin` | Lisent l'attribut. |
| Incrémentaux | `Move`, `Rotate(delta)`, `Scale(factors)` | Ajoutent/multiplient au state courant. |
| Transform | `GetTransform()`, `GetInverseTransform()` | Matrice calculée (cache) / son inverse. |

### `NkVertexArray` — tableau de sommets

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkVertexArray()`, `NkVertexArray(type, count)` | Vide / typé + pré-dimensionné. |
| Primitive | `SetPrimitiveType`, `GetPrimitiveType` | Type de primitive. |
| Taille | `GetVertexCount`, `IsEmpty`, `Clear`, `Resize`, `Reserve`, `Append` | Compte / vide / vider / redim / pré-réserver / push fin. |
| Accès | `operator[]`, `Data()` | Sommet indexé / buffer contigu. |
| Bornes | `GetBounds()` | AABB des positions `[O(n)]`. |

### `NkDrawable` / `NkIDrawable2D` — interfaces dessinables

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Nouveau style | `NkDrawable::Draw(NkRenderTarget&, const NkRenderStates&)` (pur, `const`) | Se dessine sur une cible en composant l'état. |
| Ancien style | `NkIDrawable2D::Draw(NkIRenderer2D&)` (pur, `const`) | Compat, se dessine sur un renderer. |

### `NkRenderStates` — états d'un draw

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Champs | `transform`, `blendMode`, `texture`, `shader` | Les 4 états d'un draw call. |
| Ctors | `NkRenderStates()`, `(NkTransform)`, `(NkTexture*)`, `(NkBlendMode)`, `(NkShader*)` | `explicit`, exclusifs (un champ chacun). |
| Static | `Default()` | Singleton const d'état neutre. |

### `NkRenderer2D` / `NkIRenderer2D` — le renderer

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Bind (façade) | `NkRenderer2D(backend, target)`, `Bind`, `IsValid` | Lier un backend + target non possédés. |
| Frame | `Begin`, `End`, `Flush`, `Clear` | Ouvrir / flush+submit / flush manuel / effacer. |
| Vue | `SetView`, `GetView`, `GetDefaultView`, `SetViewport`, `GetViewport`, `OnResize` | Caméra ortho / zone écran / notif resize. |
| Clip | `SetClip`, `PopClip`, `ResetClip`, `HasClip`, `GetClip` | Pile de scissor (pixels, origine haut-gauche). |
| Blend | `SetBlendMode`, `GetBlendMode` | Mode de mélange courant. |
| Draw objets | `Draw(NkDrawable, states)`, `Draw(NkIDrawable2D)`, `Draw(NkSprite)`, `Draw(NkText)` | Dessine un objet (nouveau / ancien style / sprite / texte). |
| Draw primitives | `DrawPoint`, `DrawLine`, `DrawRect`, `DrawFilledRect`, `DrawCircle`, `DrawFilledCircle`, `DrawTriangle`, `DrawFilledTriangle` | Formes prêtes à l'emploi. |
| Draw contours | `DrawRectOutline`, `DrawCircleOutline` | Contours composés (no-op si épaisseur ≤ 0). |
| Draw avancé | `DrawVertices`, `DrawTexturedRect` (uv ou pixels) | Sommets bruts / quad texturé. |
| Stats | `GetStats`, `ResetStats` | Compteurs de frame. |
| Coords | `MapPixelToCoords`, `MapCoordsToPixel` | Conversion pixel ↔ monde. |
| Interne | `GetBackend`, `GetTarget` (façade) | Accès avancé (non possédés). |
| Alias | `NkRenderer2DPtr` = `NkUniquePtr<NkIRenderer2D>` | Pointeur unique sur l'interface. |

### `NkRenderer2DFactory` — fabrique

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Type | `NkGraphicsApi` = `graphics::NkGraphicsApi` | Alias du type d'API graphique. |
| Création | `Create(ctx)`, `CreateUnique(ctx)` | Crée+initialise un renderer (le contexte partage le device). |
| Destruction | `Destroy(renderer)` | `Shutdown` + libération NKMemory. |
| Requête | `IsApiSupported(api)` | L'API a-t-elle un renderer 2D ? |

### `NkBatchRenderer2D` — batcher partagé (base des backends)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Groupe | `NkBatchGroup` (`texture`, `blendMode`, `indexStart`, `indexCount`) | Un groupe = un draw call. |
| Limites | `kMaxVertices` (65536), `kMaxIndices` (~98304) | Capacité d'un batch avant flush auto. |
| Overrides | tous les membres de `NkIRenderer2D` (frame/vue/clip/blend/primitives/coords/stats) | Implémentation du batching CPU. |
| Hooks backend | `BeginBackend`, `EndBackend`, `SubmitBatches` (pur), `UploadProjection` (pur), `ApplyScissor` | Points d'extension par API GPU. |

---

## Référence complète

Chaque élément repris en détail, avec ses usages multi-domaines. Les types triviaux sont décrits
brièvement ; le renderer, les transforms et le batcher le sont **à fond**.

### Les types partagés (`NkRenderer2DTypes.h`)

Ce header est le **vocabulaire** de toute la couche. Les alias re-exportent NKMath pour que le code
2D parle un seul dialecte : `NkColor2D` (RGBA 0-255), `NkVec2f`/`NkVec2i`, et les quatre rectangles
typés. La séparation des rectangles est intentionnelle et porte un usage :

- **`NkRect2i`** (entier) — pour ce qui se mesure en **pixels écran** : un rectangle de clip, un
  viewport, une zone d'UI. C'est aussi le type d'une sous-région d'atlas en pixels.
- **`NkRect2f`** (float) — pour la **géométrie monde** : la boîte d'un sprite, l'AABB d'un
  `NkVertexArray`, l'argument des primitives `DrawRect` / `DrawFilledRect`.
- **`NkRect2u` / `NkRect2d`** — variantes uint/double pour les cas spécialisés (tailles non
  signées, précision étendue).

`NkBlendMode` choisit comment un fragment se mêle au fond : `NK_ALPHA` (transparence standard,
*premultiplied* src / 1−src_alpha dst) est le défaut universel ; `NK_ADD` (additif) sert aux
**effets lumineux** — feu, étincelles, glow, lasers, où les couleurs s'accumulent vers le blanc ;
`NK_MULTIPLY` assombrit, idéal pour des **ombres** ou des teintes ; `NK_NONE` écrase sans mélanger
(arrière-plans opaques, *clear* manuel).

`NkPrimitiveType` énumère ce que le GPU sait tracer. Le batcher décompose tout en **triangles** pour
la compatibilité cross-API — il n'existe **pas** de type QUADS, un quad devient deux triangles.
`NkVertex2D` est le POD transmis au GPU ; `NkTransform2D` est un **transform POD** (position,
rotation, scale, origine) avec `ToMatrix4` qui écrit une `float[16]` column-major prête pour un
uniform — à ne pas confondre avec la classe `NkTransform` (ci-dessous). `NkView2D` est la **caméra
ortho** (centre, taille visible, rotation) avec `ToProjectionMatrix`. `NkRenderStats2D` agrège les
compteurs d'une frame (draw calls, sommets, indices, changements de texture) — l'outil de base du
**profilage** et de l'overlay de débogage d'un éditeur.

### `NkTransform` à fond

`NkTransform` enveloppe une `NkMat4f` mais n'expose que ce qui a un sens en 2D affine (col0/col1/col3).
On le construit en identité, par ses 6 composantes affines (les deux premières lignes du 3×3, la
troisième étant `(0,0,1)`), ou via le singleton `Identity()`.

**Composer, c'est tout.** Les opérations `Translate`, `Rotate` (autour de l'origine ou d'un centre),
`Scale` et `Combine` **mutent** `*this` et le **retournent** — on chaîne donc `t.Translate(...).Rotate(...)`.
La règle d'or est celle des matrices : `T = A * B` applique **B avant A**. `Combine(other)` fait
`this = this * other`, c'est-à-dire *other appliqué d'abord*. Le code est **spécialisé 2D** : une
`Combine` coûte 12 multiplications / 8 additions au lieu des 64/48 d'une multiplication Mat4 générale.

Usages par domaine :

- **Rendu / scène** — bâtir la matrice monde d'un objet à partir de celle de son parent :
  `world = parent.GetMatrix() * local` (hiérarchie de transforms, comme un bras articulé).
- **UI / 2D** — empiler des repères imbriqués (un panneau, ses boutons, leurs icônes) : chaque
  niveau `Combine` le sien sur celui du parent.
- **Outils / éditeur** — `TransformPoint` projette la position d'un clic dans le repère d'un objet ;
  `GetInverse()` fait l'inverse (passer du monde au local), indispensable pour la **sélection** et le
  *gizmo* de manipulation.
- **Collision / physique** — `TransformRect` donne l'AABB englobante des 4 coins transformés (elle
  **grossit** sous rotation/skew) : un *broad-phase* rapide pour un objet tourné.

`TransformPoint(p)` renvoie `T·(p,1)` ; `TransformRect(r)` renvoie l'AABB des 4 coins, en `O(1)`.
`GetInverse()` calcule l'inverse affine 2D spécialisée et retombe sur `Identity()` si le déterminant
est nul (transform dégénérée, scale zéro). Les opérateurs `*` / `*=` composent, `* point` est un
raccourci de `TransformPoint`, et `==` / `!=` comparent **strictement** les 16 floats.

### `NkTransformable` à fond

`NkTransformable` est la **base à hériter** pour tout objet positionnable — un sprite, un panneau, un
acteur 2D, un curseur d'éditeur. Elle stocke quatre attributs (position, rotation en radians, scale,
origine) plutôt qu'une matrice, et **génère** la matrice à la demande selon le modèle SFML :
`T = Translate(position) · Rotate(rotation) · Scale(scale) · Translate(-origin)`.

**L'origine est le pivot.** `SetOrigin` choisit le point local autour duquel tournent rotation et
scale — poser l'origine au **centre** d'un sprite 64×64 (`SetOrigin({32,32})`) fait pivoter le
sprite sur lui-même au lieu de son coin. C'est le levier de presque toutes les animations 2D.

Setters (`SetPosition/Rotation/Scale/Origin`) **posent** une valeur absolue et marquent l'objet
*dirty* ; les mutateurs **incrémentaux** (`Move` ajoute à la position, `Rotate` ajoute à l'angle,
`Scale` **multiplie** le scale) modifient relativement. C'est la distinction à ne jamais rater :
`Rotate(0.1f)` sur un `NkTransformable` *ajoute* 0,1 rad à chaque frame (rotation continue), là où
`NkTransform::Rotate` compose une matrice.

**Le cache.** `GetTransform()` ne recalcule la matrice que si un setter l'a invalidée, sinon c'est
un retour `O(1)` ; `GetInverseTransform()` a son propre cache distinct. La matrice produite suit la
convention SFML (a00 = sx·cos, a01 = −sy·sin, a10 = sx·sin, a11 = sy·cos) — un bug d'inversion de ces
termes avait rendu paddles et balle invisibles dans Pong, corrigé le 2026-05-30.

Usages par domaine :

- **Gameplay** — un projectile : `SetPosition` au tir, `Move(vel * dt)` chaque frame.
- **Animation** — faire tourner une roue / une aiguille : `Rotate(omega * dt)`, origine au moyeu.
- **UI / 2D** — placer un widget, le faire « respirer » via un `Scale` oscillant autour de son centre.
- **Outils / éditeur** — un *handle* de manipulation qu'on déplace, tourne, redimensionne ; on lit le
  state via les getters pour l'inspecteur.

L'idiome de rendu recommandé, dans une classe héritant aussi de `NkDrawable` :
`states.transform *= GetTransform();` puis on dessine.

### `NkVertexArray` à fond

`NkVertexArray` est le tableau de sommets de bas niveau, pour la géométrie que les primitives ne
savent pas faire. Construit vide (TRIANGLES par défaut) ou typé+pré-dimensionné, il offre l'API d'un
vecteur : `Append` (push en fin, `O(1)` amorti), `Resize` (éléments POD **non initialisés**),
`Reserve`, `Clear` (vide en gardant capacité et primitive), `operator[]`, `Data()` (buffer contigu).
`GetBounds()` calcule l'AABB des positions en `O(n)` (zéro si vide).

Usages par domaine :

- **Rendu 2D** — un **dégradé** (couleurs différentes par sommet, interpolées), une **bande de
  trail** derrière un projectile (`NK_TRIANGLE_STRIP`), un **secteur** ou un **camembert**
  (`NK_TRIANGLE_FAN`).
- **Outils / éditeur** — tracer un **graphe** de débogage (courbe FPS, profil mémoire) en
  `NK_LINE_STRIP`, ou une **grille** en `NK_LINES`.
- **Gameplay / 2D** — un maillage de terrain 2D, un polygone de collision visualisé, un *light cone*.

Ce n'est qu'un conteneur CPU : il ne se dessine pas seul (pas encore `NkDrawable`, prévu étape A.3) —
on passe son `Data()` à `DrawVertices`, et le batcher l'uploade.

### `NkDrawable` et `NkIDrawable2D`

Deux interfaces dessinables **coexistent**. `NkDrawable` est le **nouveau style** (équivalent
`sf::Drawable`) : sa méthode pure `Draw(NkRenderTarget&, const NkRenderStates&) const` reçoit une
**cible** et un **état** à composer. Elle est `const` parce qu'un objet ne se mute pas pendant son
rendu — les caches de `NkTransformable` restent valides. Le **pattern de composition** est canonique :
copier `states`, faire `local.transform *= GetTransform()`, attacher `local.texture`, puis appeler
`target.Draw(...)`. C'est ainsi qu'un parent propage sa transform à ses enfants.

`NkIDrawable2D` est l'**ancienne** interface (compat), dont le `Draw(NkIRenderer2D&) const` se
dessine sur un renderer plutôt que sur une cible. `NkRenderer2D` accepte les **deux**. Pour du code
neuf, on hérite de `NkDrawable` (+ `NkTransformable` pour le placement).

Usages par domaine : tout objet visuel (sprite custom, barre de vie, mini-carte, particule, widget)
gagne à implémenter `NkDrawable` — on le dessine alors uniformément via `renderer.Draw(obj, states)`,
et la hiérarchie de transforms se compose toute seule.

### `NkRenderStates`

Les **quatre états** d'un draw call, dans une structure légère : `transform` (l'affine appliquée aux
sommets avant projection, composée parent→enfant), `blendMode` (défaut `NK_ALPHA`), `texture`
(`nullptr` = couleur vertex seule), `shader` (`nullptr` = pipeline 2D par défaut — `NkShader` est un
placeholder non implémenté). `Default()` renvoie un singleton const neutre, l'argument par défaut de
`NkRenderer2D::Draw(NkDrawable, …)`.

**Piège des constructeurs.** Les cinq ctors sont `explicit` et **mutuellement exclusifs** : chacun
ne pose qu'**un seul** champ (transform seule, ou texture seule, ou blend seul, ou shader seul). Il
n'existe pas de ctor combiné — pour un draw avec transform **et** texture, on construit puis on
assigne :

```cpp
NkRenderStates st;
st.transform = sprite.GetTransform();
st.texture   = &atlas;
st.blendMode = NkBlendMode::NK_ADD;   // effet lumineux
```

### `NkRenderer2D` et `NkIRenderer2D` à fond

`NkIRenderer2D` est l'**interface abstraite** que les 5 backends (GL/VK/DX11/DX12/SW) implémentent —
un détail interne. `NkRenderer2D` est la **façade concrète** qu'on utilise : elle wrappe un backend
et un target **non possédés** (c'est une projection, durée de vie = celle du target) et forwarde tout
*inline*. On la construit liée (`NkRenderer2D(backend, target)`), on peut **rebind** a posteriori
(`Bind`), et `IsValid()` teste `backend && backend->IsValid()`. **Toute** méthode tolère un backend
null : no-op pour les actions, valeurs par défaut pour les getters.

**Le cycle de frame.** `Begin()` ouvre l'accumulation (renvoie `false` si invalide), `End()` flush +
soumet le batch, `Flush()` soumet le batch courant et remet les accumulateurs à zéro — automatique
dans `End()`, mais à appeler **manuellement** lorsqu'on change de vue en cours de frame (un batch ne
peut pas mélanger deux projections). `Clear(color)` efface (noir par défaut).

**La vue et le viewport.** `SetView(NkView2D)` pose la caméra ortho ; `GetDefaultView()` renvoie la
vue plein-cadre qui suit l'écran. `SetViewport(NkRect2i)` restreint le rendu à une sous-zone du
framebuffer. `OnResize(w, h)` notifie une nouvelle taille : la vue **par défaut** se réajuste à
l'écran, mais une vue **custom** posée par `SetView` **reste intacte** — comportement clé pour un
HUD à résolution fixe sur un monde qui *scroll*. Usages : `SetView` fait le **scroll de caméra** et le
**zoom** d'un jeu 2D ; `SetViewport` partage l'écran en **split-screen** ou réserve une zone à un
panneau d'éditeur ; `MapPixelToCoords` convertit la souris en coordonnées monde (sélection, *pick*)
et `MapCoordsToPixel` fait l'inverse (placer un label écran sur un objet monde).

**Le clip (scissor).** Une **pile** de rectangles en pixels (origine haut-gauche) : `SetClip`
empile en **intersectant** avec le clip courant, `PopClip` dépile, `ResetClip` vide, `HasClip` /
`GetClip` interrogent. C'est le mécanisme des **fenêtres scrollables** d'UI (clipper le contenu à son
cadre), des *masks* 2D, du rognage d'un mini-jeu dans un panneau d'éditeur.

**Le blend.** `SetBlendMode` / `GetBlendMode` règlent le mélange courant (défaut `NK_ALPHA`).

**Dessiner.** Trois familles. Les **objets** : `Draw(NkDrawable, states)` (nouveau, utilise le target
englobant et compose l'état — no-op si target null), `Draw(NkIDrawable2D)` (ancien), `Draw(NkSprite)`,
`Draw(NkText)`. Les **primitives** prêtes à l'emploi couvrent l'essentiel sans toucher au moindre
sommet :

- `DrawPoint(pos, color, size)`, `DrawLine(a, b, color, thickness)`.
- `DrawRect(rect, color, outline, outlineColor)` (rempli + contour) et `DrawFilledRect(rect, color)`.
- `DrawCircle(center, radius, color, segments=32, outline, outlineColor)` et
  `DrawFilledCircle(...)`.
- `DrawTriangle(a, b, c, color, outline, outlineColor)` et `DrawFilledTriangle(...)`.
- `DrawRectOutline(rect, color, thickness)` (4 `DrawFilledRect`) et
  `DrawCircleOutline(center, radius, color, thickness, segments)` (anneau de segments via
  `math::NkCos/NkSin`) — tous deux **no-op** si l'épaisseur est ≤ 0 (et le cercle si segments < 3).
  Ce sont des impls **composées** : aucun backend à modifier.

Les **avancés** : `DrawVertices(vertices, count, indices, indexCount, texture)` soumet de la
géométrie brute (le débouché de `NkVertexArray`) ; `DrawTexturedRect` dessine un quad texturé, soit
avec une **UV normalisée** `{0,0,1,1}`, soit (surcharge façade) avec une **sous-région en pixels**
`NkRect2i sourcePixels` convertie en interne via `texture->GetTexCoords(...)` — exactement ce qu'il
faut pour piocher une tuile dans un **atlas / spritesheet**. No-op si la texture est null.

Usages des primitives par domaine :

- **UI / 2D** — boutons (`DrawFilledRect` + `DrawRectOutline`), curseurs, jauges, séparateurs.
- **Outils / éditeur** — grille (`DrawLine`), gizmos (`DrawCircle`, `DrawLine`), *bounding boxes* de
  sélection (`DrawRectOutline`).
- **Gameplay** — formes simples d'un prototype (Pong, casse-briques), zones de *trigger* visualisées.
- **Débogage / physique** — afficher les AABB, les rayons, les points de contact, les normales.

**Stats et profilage.** `GetStats()` renvoie un `NkRenderStats2D` (draw calls, sommets, indices,
*texture swaps*), `ResetStats()` remet à zéro — le tableau de bord d'un éditeur pour vérifier que le
batching fait son travail. `GetBackend()` / `GetTarget()` exposent les pointeurs internes (avancé,
non possédés). L'alias `NkRenderer2DPtr` est un `NkUniquePtr<NkIRenderer2D>` pour la possession
explicite du backend.

### `NkRenderer2DFactory`

La **fabrique** qui crée le bon backend pour un contexte donné. `Create(ctx)` instancie et initialise
un renderer **adossé** au contexte graphique (il **partage** son device et sa command queue, **aucun**
nouveau device GPU n'est créé) ; renvoie `nullptr` si l'API n'est pas supportée ou si l'init échoue.
`CreateUnique(ctx)` en donne une version `NkUniquePtr`. `Destroy(renderer)` fait `Shutdown()` puis
libère via l'allocateur NKMemory (symétrique de `Create`, no-op si null). `IsApiSupported(api)`
indique si une API a un renderer 2D. L'alias `NkGraphicsApi` (de `nkentseu::graphics`) désigne le
type d'API.

**Piège d'ownership critique.** Ne **jamais** faire `delete` sur le pointeur de `Create` (alloué via
NKMemory) : on le libère par `Destroy()` ou on le confie à `NkUniquePtr` — sinon **heap corruption**
Windows `c0000374`. C'est la règle dure NKMemory de tout le moteur (tout `Create` a son `Destroy`).

### `NkBatchRenderer2D` à fond

C'est la **machine** sous la façade : la classe de base que les 5 backends dérivent. Elle implémente
le **batching CPU** — accumuler `NkVertex2D` + indices côté CPU, les **grouper** par (texture,
blendMode) pour minimiser les changements d'état GPU, et flush quand le batch est plein ou qu'un état
change. Un `NkBatchGroup` décrit un groupe = **un draw call** (texture, blend, plage d'indices).

Les limites `kMaxVertices` (65536) et `kMaxIndices` (~98304) bornent un batch : dès qu'on les atteint
(ou qu'on change de clip/blend/texture), un **flush automatique** survient. Le système est
**single-thread** : accumulation CPU puis submit, pas de rendu concurrent.

`NkBatchRenderer2D` **override** tout `NkIRenderer2D` (frame, vue, viewport, clip — chaque changement
de clip flush le batch en cours —, blend, primitives, coords, stats). Ce qui reste **abstrait**, ce
sont les **hooks** que chaque backend doit câbler :

- `SubmitBatches(groups, …, vertices, …, indices, …)` (**pur**) — upload + draw calls réels.
- `UploadProjection(proj[16])` (**pur**) — pousse la projection ortho de la vue courante.
- `BeginBackend()` / `EndBackend()` (défaut no-op) — setup état GPU début de frame / fin de render
  pass + present.
- `ApplyScissor(enabled, rect)` (défaut no-op) — applique/retire le scissor GPU, appelé par `Flush()`
  juste avant `SubmitBatches` (tout le batch partage ce clip) ; un backend qui ne l'implémente pas
  **ignore** simplement le clip sans casser.

C'est un point d'extension : **écrire un nouveau backend** revient à dériver `NkBatchRenderer2D` et
implémenter ces hooks — toute la logique de batching, de vue et de clip est déjà faite. Le débogage
GPU et le profilage s'appuient sur cette couche (un seul endroit où les draw calls sont émis).

---

### Exemple

```cpp
#include "NKCanvas/Renderer/Core/NkRenderer2D.h"
#include "NKCanvas/Renderer/Core/NkRenderer2DFactory.h"
#include "NKCanvas/Renderer/Core/NkTransformable.h"
#include "NKCanvas/Renderer/Core/NkDrawable.h"
using namespace nkentseu::renderer;

// 1. Un objet dessinable, placé via NkTransformable, rendu via NkDrawable.
class Paddle : public NkTransformable, public NkDrawable {
public:
    void Draw(NkRenderTarget& target, const NkRenderStates& states) const override {
        NkRenderStates local = states;
        local.transform *= GetTransform();          // compose le placement
        // ... émettre la géométrie du paddle sur 'target' avec 'local'
    }
};

// 2. Fabrique : on crée le backend depuis un contexte existant (jamais 'new').
NkIRenderer2D* backend = NkRenderer2DFactory::Create(context);   // partage le device
NkRenderer2D   renderer(backend, &target);

Paddle paddle;
paddle.SetOrigin({ 8, 40 });            // pivot au centre du paddle 16x80
paddle.SetPosition({ 40, 360 });

// 3. La boucle de frame, style SFML.
if (renderer.Begin()) {
    renderer.Clear(NkColor2D::Black);

    renderer.SetView(camera);                       // caméra ortho 2D
    renderer.Draw(paddle);                          // objet (compose sa transform)
    renderer.DrawFilledCircle({ 640, 360 }, 12.f, NkColor2D::White);   // la balle
    renderer.DrawRectOutline({ 0, 0, 1280, 720 }, NkColor2D::Blue, 2.f);

    renderer.SetClip({ 0, 0, 320, 100 });           // clipper le HUD
    renderer.DrawFilledRect({ 8, 8, 120, 24 }, NkColor2D::Red);
    renderer.PopClip();

    renderer.End();                                 // flush + submit
}

// 4. Libération symétrique : Destroy, jamais delete.
NkRenderer2DFactory::Destroy(backend);
```

---

[← Index NKCanvas](README.md) · [Récap NKCanvas](../NKCanvas.md) · [Couche Runtime](../README.md)
