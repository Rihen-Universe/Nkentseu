# Le sculpting espace-écran (PixolSculpt)

> Couche **Runtime** · NKRenderer · Sculpter une surface comme on peint, en **2.5D « pixol »** à
> la ZBrush : pas de polygones ajoutés, mais un G-buffer écran (profondeur / normale / matériau /
> couleur / masque) **muté par des dispatchs compute** bornés à la tuile sous la brosse.

> ⚠️ **Statut : SQUELETTE.** Cette famille est une charpente. Les **structures de données**, les
> **signatures** et les fonctions `inline`/triviales (accesseurs, `Reset()`, `IsEmpty()`,
> `MakeBrushGPU()`) sont réellement définies ; tout le reste (`Init`/`Shutdown`/`Resize`/`Begin`/
> dispatchs) n'est que **déclaré**. Aucun chemin ne tourne en production aujourd'hui. Documentez-la
> comme un **plan d'architecture**, pas comme une API utilisable.

Sculpter une forme « à la main » a longtemps voulu dire **ajouter de la géométrie** : subdiviser un
maillage, déplacer des milliers de sommets, et payer en mémoire et en temps proportionnellement au
nombre de triangles. PixolSculpt prend le problème par l'autre bout. L'idée — héritée du *pixol* de
ZBrush — est qu'à l'écran, on ne voit jamais qu'**une surface vue de face** : chaque pixel porte une
profondeur, une normale, une couleur. **Pourquoi ne pas sculpter directement ces pixels** ? On garde
un petit **G-buffer dédié** (depth / normal / material / color / mask), et un coup de brosse devient
un **dispatch compute** qui ne touche que les pixels sous le curseur. Le coût n'est plus lié au
nombre de triangles, mais à la **résolution écran** — et même là, borné à la **tuile** sous la
brosse.

Ce n'est **pas** un système de sculpting géométrique classique : aucun sommet n'est créé, aucune
subdivision n'a lieu, et la « forme » vit dans des *storage images* écran, pas dans un *mesh*. (La
vraie voie multirésolution — `NkSculptTileStore` — existe à l'état d'intention, mais son corps est
entièrement commenté : voir plus bas.) Ce n'est **pas** non plus un module de peinture 2D : la
brosse mute une **profondeur** et une **normale**, donc elle déforme un relief, elle ne barbouille
pas un calque.

- **Namespace** : `nkentseu::renderer` (tous les types ; la plupart des headers font
  `using namespace math;`, d'où `NkVec2f` / `NkVec4f` venant de `nkentseu::math`)
- **Headers réels** : `NKRenderer/Tools/PixolSculpt/{NkSculptTypes.h, NkSculptBrush.h,
  NkPixolBuffer.h, NkSculptStroke.h, NkSculptPipelines.h, NkSculptTileStore.h,
  NkPixolSculptSystem.h}`

---

## Le canvas et son vocabulaire : `NkSculptTypes`

Avant toute brosse, il faut décrire **ce qu'on sculpte** et **comment**. `NkSculptTypes.h` pose ce
vocabulaire : la version du format (`kNkPixolSculptVersion`), la taille de tuile compute par défaut
(`kNkSculptTileSize = 16`, c'est-à-dire des groupes 16×16), les **modes de brosse**
(`NkSculptBrushMode`) et les **profils d'atténuation** (`NkSculptFalloff`), les **formats des
cibles** (`NkPixolFormat`), la **config** de création (`NkPixolSculptConfig`), et deux petits
utilitaires : des **statistiques** (`NkSculptStats`) et un **rectangle sale** (`NkSculptRect`).

Les deux enums sont la grammaire du geste. `NkSculptBrushMode` dit **quoi faire** au relief :
soulever (`NK_RAISE`), creuser (`NK_LOWER`), lisser (`NK_SMOOTH`), pincer vers le centre
(`NK_PINCH`), gonfler le long de la normale (`NK_INFLATE`), aplatir vers un plan moyen
(`NK_FLATTEN`), peindre le masque (`NK_MASK`) ou la couleur (`NK_PAINT`). `NkSculptFalloff` dit
**comment l'effet s'éteint** du centre vers le bord : `NK_SMOOTH` (smoothstep, défaut), `NK_LINEAR`,
`NK_CONSTANT` (plein jusqu'au bord), `NK_SHARP`, `NK_SPHERE` (profil hémisphérique). Attention au
**piège de scope** : `NK_SMOOTH` existe dans **les deux** enums — il faut toujours qualifier
(`NkSculptBrushMode::NK_SMOOTH` n'est pas `NkSculptFalloff::NK_SMOOTH`), et `NK_COUNT` est présent
dans les deux aussi.

Le `NkSculptRect` mérite un mot : c'est le **rectangle sale**, la zone d'écran réellement touchée
par le coup de brosse, exprimée en espace tuile/écran. C'est lui qui transforme « sculpter » en
opération **bornée** : on ne re-dispatchera le compute que sur cette région. Son `IsEmpty()`
(`w <= 0 || h <= 0`, défini *inline*) sert de garde « rien à faire cette frame ».

> **En résumé.** `NkSculptTypes` est le dictionnaire du sous-système : version + taille de tuile,
> les **8 modes** de brosse, les **5 profils** d'atténuation, les formats des cibles, la config de
> création, et deux petits objets (`NkSculptStats`, `NkSculptRect`). Qualifiez toujours
> `NK_SMOOTH` / `NK_COUNT` par leur enum.

---

## La brosse et le tampon : `NkSculptBrush`

Un coup de brosse n'est pas un instant unique mais une **série de tampons** (*dabs*) déposés le long
du déplacement du curseur. `NkSculptBrush.h` sépare proprement trois objets : l'**état de l'outil**
côté CPU (`NkSculptBrush` : mode, falloff, rayon, force, dureté, couleur de peinture…), le **tampon
individuel** (`NkSculptDab` : une position écran, un rayon, une pression), et le **bloc GPU**
(`NkSculptBrushGPU`) — la *push-constant* exacte que lit le shader compute.

`NkSculptBrushGPU` est l'endroit où le contrat CPU↔GPU se joue : **64 octets, std430, aligné 16**,
dont le layout **doit** correspondre champ pour champ à `shaders/sculpt_brush.comp.glsl`. Toute
modification de cette structure casse le shader silencieusement — c'est le genre de détail qu'on
documente précisément justement parce qu'il est invisible à la compilation.

Le seul vrai code exécutable de ce header est `MakeBrushGPU(brush, dab, tileOffX, tileOffY)`, une
free function `inline` qui assemble le bloc push-constant : le **centre vient du dab**
(`center = dab.screenPos`), le **rayon aussi** (`radius = dab.radiusPx`), et la **force est modulée
par la pression** (`strength = brush.strength * dab.pressure`) ; on copie ensuite couleur, mode,
falloff, dureté, biais de profondeur, et on pose l'offset de tuile. C'est `O(1)`. À noter
l'**asymétrie volontaire** : `MakeBrushGPU` **ignore** `NkSculptBrush::radiusPx` (le rayon effectif
vient du dab, pas de l'outil) et `NkSculptBrush::invert` (non transmis au bloc GPU).

> **En résumé.** Trois objets : l'outil (`NkSculptBrush`), le tampon (`NkSculptDab`), le bloc GPU
> (`NkSculptBrushGPU`, 64 o std430, à garder synchronisé avec le `.comp.glsl`). `MakeBrushGPU` les
> fond en push-constant : rayon et centre **du dab**, force × pression ; il ignore `invert` et le
> `radiusPx` de la brosse.

---

## Le canvas GPU : `NkPixolBuffer`

`NkPixolBuffer` **est** la surface sculptée : un jeu de *storage images* écran — profondeur,
normale, matériau, couleur, masque — toutes prévues en `NK_UNORDERED_ACCESS` pour que les compute
puissent y écrire. C'est l'analogue d'un G-buffer, mais possédé et muté par le sous-système de
sculpting plutôt que produit par une passe géométrique.

L'objet suit la **règle dure Create/Destroy** : `Init(device, w, h, cfg)` alloue les cibles,
`Shutdown()` les libère, `Resize()` les recrée, `Clear(cmd)` remet le canvas à neuf (profondeur à
`+inf`, masque à 0…). Tout cela est **déclaré seulement** aujourd'hui. Ce qui est réellement défini,
ce sont les **accesseurs** : `IsValid()` / `Width()` / `Height()`, et les **handles RHI** des cinq
cibles (`Depth()`, `Normal()`, `Material()`, `Color()`, `Mask()`).

Le buffer sait aussi s'**importer dans le render graph** (`ImportToGraph`) en conservant les
identifiants de ressource. Ces ResId sont exposés en `uint32` (et non en `NkGraphResId`, pour éviter
d'inclure le header du graph) via `ResDepth()`, `ResNormal()`, `ResColor()`, `ResMask()`. **Détail
important** : il y a un `Material()` (handle) mais **pas de `ResMaterial()`** — asymétrie réelle de
l'API, le matériau n'est pas exposé comme ressource du graph.

> **En résumé.** `NkPixolBuffer` détient les 5 *storage images* (depth/normal/material/color/mask),
> en Create/Destroy. Accesseurs réels : `IsValid/Width/Height`, les 5 handles, et 4 ResId `uint32`
> — mais **pas** de `ResMaterial`.

---

## Le trace : `NkSculptStroke`

Entre le geste de l'utilisateur (« je bouge la souris ») et le GPU (« voici une liste de dabs »), il
faut un accumulateur. `NkSculptStroke` joue ce rôle : il **collecte les dabs** le long d'un trace et
maintient le **dirty rect aligné tuile**. On l'ouvre par `Begin(brush)`, on lui pousse des positions
par `AddSample(screenPos, pressure)` — qui **interpole** entre deux échantillons et **génère des
dabs espacés** —, et on le ferme par `End()`. Après dispatch, le système appelle `ClearPending()`
pour vider la file des dabs en attente sans réinitialiser le trace.

Les accesseurs définis donnent au système ce dont il a besoin : `IsActive()`, `PendingDabs()` (un
`NkVector<NkSculptDab>` de NKContainers), `DirtyRect()` (renvoyé **par valeur**), et `Brush()`. En
interne, `ExpandDirty(dab)` élargit le rectangle sale en l'**alignant sur `kNkSculptTileSize`** —
c'est ce qui garantit que les dispatchs tombent toujours sur des frontières de tuile.

> **Note de cohérence.** Le commentaire d'`AddSample` parle de « dabs espacés de
> `brush.dabSpacing * radius` », mais `dabSpacing` est en réalité un champ de
> **`NkPixolSculptConfig`**, pas de `NkSculptBrush` — c'est un comportement attendu, pas une
> propriété de la brosse.

> **En résumé.** `NkSculptStroke` = accumulateur d'un trace : `Begin/AddSample/End`, dabs interpolés
> et espacés, dirty rect aligné tuile. Le système lit `PendingDabs()`/`DirtyRect()` puis appelle
> `ClearPending()`.

---

## Les pipelines et la façade : `NkSculptPipelines`, `NkPixolSculptSystem`

`NkSculptPipelines` est le **registre des pipelines compute**. Il s'appuie sur
`NkComputeContext::GetOrCompileGLSL` (le chemin actif est **GLSL**, pas NkSL) et expose deux noyaux :
`Brush()` — le kernel de brosse unique, le **mode passant par la push-constant** plutôt que par des
pipelines distincts — et `Resolve()` — qui composite le canvas pixol vers le G-buffer. Ces deux
accesseurs sont **non-const** (compilation/cache **paresseux** : le pipeline n'est compilé qu'au
premier appel). `IsValid()` (`mCtx != nullptr`) est le seul défini ; `Init`/`Shutdown` sont
déclarés.

`NkPixolSculptSystem` est la **façade** — l'objet que le reste du moteur manipule. Il possède le
canvas, l'état de trace et les pipelines, et s'**enregistre dans le `NkRenderGraph`** avec **deux
passes compute** : *Brush* puis *Resolve*. Le pattern est calqué sur `NkDeferredPass` :
`Init(device, graph, texLib, shaderLib, w, h, cfg)` puis `RegisterToRenderGraph()` — à appeler dans
`BuildDefaultRenderGraph()` **avant** la passe d'éclairage deferred. L'**API de trace** publique
reflète l'outil : `SetBrush`/`GetBrush`, `BeginStroke`/`AddStrokeSample`/`EndStroke`,
`ClearCanvas`. Les accesseurs définis sont `IsValid()`, `GetBrush()`, `PixolBuffer()` (référence
**non-const**, mutable) et `Stats()`.

Un détail d'implémentation a son importance : le système possède un `NkComputeContext` **par valeur**
(`mCompute`), déclaré **avant** `mPipelines` qui en dépend — l'ordre garantit que `mPipelines` est
détruit **avant** le contexte. Les vraies passes s'exécutent dans des callbacks
(`RecordBrushPass` / `RecordResolvePass`) au moment du `NkRenderGraph::Execute`, c'est le graph qui
insère les barrières.

> **En résumé.** `NkSculptPipelines` cache deux kernels GLSL (`Brush`, `Resolve`, compilés
> paresseusement, accesseurs non-const). `NkPixolSculptSystem` est la façade Init→RegisterToRenderGraph
> (pattern `NkDeferredPass`), enregistre les passes **Brush puis Resolve** avant l'éclairage deferred,
> et expose l'API de trace.

---

## Aperçu de l'API

Tout ce qui est **réellement défini** est marqué *défini* ; le reste est **déclaré** (squelette).

### Types & constantes (`NkSculptTypes.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Constante | `kNkPixolSculptVersion` | Version du format pixol (= 1). |
| Constante | `kNkSculptTileSize` | Côté de tuile compute par défaut (= 16). |
| Enum | `NkSculptBrushMode` | 8 modes : `NK_RAISE`/`NK_LOWER`/`NK_SMOOTH`/`NK_PINCH`/`NK_INFLATE`/`NK_FLATTEN`/`NK_MASK`/`NK_PAINT` (+`NK_COUNT`). |
| Enum | `NkSculptFalloff` | 5 profils : `NK_SMOOTH`/`NK_LINEAR`/`NK_CONSTANT`/`NK_SHARP`/`NK_SPHERE` (+`NK_COUNT`). |
| Struct | `NkPixolFormat` | Formats des 5 cibles (`NkGPUFormat`, en `NK_UNORDERED_ACCESS`). |
| Struct | `NkPixolSculptConfig` | Config création (taille, tuile, formats, `enableColor/Mask`, `maxDabsPerFrame`, `dabSpacing`…). |
| Struct | `NkSculptStats` | Compteurs frame ; `Reset()` *défini*. |
| Struct | `NkSculptRect` | Dirty region ; `IsEmpty()` *défini*. |

### Brosse (`NkSculptBrush.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Struct | `NkSculptBrush` | État outil CPU : mode, falloff, `radiusPx`, force, dureté, `invert`, `color`, `depthBias`. |
| Struct | `NkSculptDab` | Un tampon : `screenPos`, `radiusPx`, `pressure`. |
| Struct | `NkSculptBrushGPU` | Bloc push-constant (64 o, std430) miroir de `sculpt_brush.comp.glsl`. |
| Free fn | `MakeBrushGPU(b, dab, offX, offY)` | Assemble la push-constant (rayon/centre **du dab**, force×pression) ; *défini*, `O(1)`. |

### Canvas & trace

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| `NkPixolBuffer` | `Init`/`Shutdown`/`Resize`/`Clear` | Cycle de vie des 5 storage images (déclarés). |
| `NkPixolBuffer` | `IsValid`/`Width`/`Height` | État (*définis*). |
| `NkPixolBuffer` | `Depth`/`Normal`/`Material`/`Color`/`Mask` | Handles RHI (*définis*). |
| `NkPixolBuffer` | `ImportToGraph` ; `ResDepth`/`ResNormal`/`ResColor`/`ResMask` | Import graph ; ResId `uint32` (**pas** de `ResMaterial`). |
| `NkSculptStroke` | `Begin`/`AddSample`/`End`/`ClearPending`/`Reset` | Accumulation d'un trace (déclarés). |
| `NkSculptStroke` | `IsActive`/`PendingDabs`/`DirtyRect`/`Brush` | Accesseurs (*définis*). |

### Pipelines & façade

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| `NkSculptPipelines` | `Init`/`Shutdown` ; `Brush()`/`Resolve()` | Registre compute GLSL ; kernels **non-const** (cache paresseux). |
| `NkSculptPipelines` | `IsValid()` | `mCtx != nullptr` (*défini*). |
| `NkPixolSculptSystem` | `Init`/`Shutdown`/`Resize` | Cycle de vie de la façade (déclarés). |
| `NkPixolSculptSystem` | `RegisterToRenderGraph()` | Enregistre passes Brush+Resolve (avant deferred). |
| `NkPixolSculptSystem` | `SetBrush`/`GetBrush` ; `BeginStroke`/`AddStrokeSample`/`EndStroke`/`ClearCanvas` | API de trace. |
| `NkPixolSculptSystem` | `IsValid`/`GetBrush`/`PixolBuffer`/`Stats` | Accesseurs (*définis* ; `PixolBuffer()` non-const). |

### Voie future (`NkSculptTileStore.h` — HORS-MVP)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Struct | `NkSculptDelta` | Delta de position quantifié 16 bits/composante (`dx/dy/dz`). |
| Struct | `NkSculptTile` | Patch local subdivisé (`baseVertex`, `vertexCount`, `lod`, `resident`, `dirty`). |
| Classe | `NkSculptTileStore` | Géométrie HD multirésolution — **corps entièrement commenté**, aucune méthode active. |

---

## Référence complète

Comme tout est squelette, cette section explique **à quoi chaque pièce sert et où elle s'insère**,
plus que comment l'appeler — l'essentiel des corps n'existe pas encore. Le fil rouge utile à retenir
est le **coût borné** : un coup de brosse coûte « le dirty rect, aligné tuile, plafonné par
`maxDabsPerFrame` », jamais « tous les triangles de la scène ».

### `NkSculptBrushMode` et `NkSculptFalloff` à fond

Le **mode** est l'opérateur appliqué au pixol, et chaque valeur a un sens géométrique précis qui se
retrouve dans plusieurs usages :

- **Relief (rendu / outils / éditeur)** : `NK_RAISE` pousse la profondeur vers la caméra, `NK_LOWER`
  creuse, `NK_INFLATE` déplace le long de la normale (gonfle un volume), `NK_PINCH` resserre les
  pixels vers le centre (arêtes nettes), `NK_FLATTEN` rapproche d'un plan moyen local (méplats), et
  `NK_SMOOTH` moyenne le voisinage (lissage du bruit). C'est la palette d'un sculpteur : ajouter,
  retirer, gonfler, pincer, aplatir, lisser.
- **Masquage (workflow)** : `NK_MASK` peint le canal de masque, qui **protège** une zone des autres
  brosses — on sculpte « tout sauf ce que j'ai masqué », pattern central des outils de modelage.
- **Couleur (texture / 2D)** : `NK_PAINT` écrit dans le canal couleur (albedo) — sculpter et peindre
  partagent alors le même canvas, donc la peinture suit naturellement le relief.

Le **falloff** décrit la décroissance radiale du centre vers le bord, et change le « toucher » de la
brosse : `NK_SMOOTH` (smoothstep, le défaut, transitions douces), `NK_LINEAR` (pente régulière),
`NK_CONSTANT` (effet plein jusqu'au bord — un tampon dur), `NK_SHARP` (concentre l'effet au centre),
`NK_SPHERE` (profil hémisphérique, idéal pour déposer des bosses arrondies). Rappel du **piège de
scope** : `NK_SMOOTH` et `NK_COUNT` existent dans les **deux** enums ; ne jamais les utiliser non
qualifiés.

### `NkPixolFormat`, `NkPixolSculptConfig` à fond

`NkPixolFormat` fixe le type des cinq cibles, toutes pensées en `NK_UNORDERED_ACCESS` (le compute y
écrit) : `depth` en `R32_FLOAT` (précision maximale du relief), `normal` en `RGBA16_FLOAT` (assez de
bits pour ne pas étager l'éclairage), `material` et `color` en `RGBA8_UNORM`, `mask` en `R8_UNORM`.
Ces choix sont des points de départ, pas des contraintes gravées.

`NkPixolSculptConfig` règle le sous-système à la création :

- **Dimensions** — `width`/`height` à 0 signifient « suit le swapchain » (le canvas se cale sur la
  taille de la fenêtre).
- **Granularité** — `tileSize` (défaut `kNkSculptTileSize`) pilote la taille des groupes compute et
  l'alignement du dirty rect.
- **Canaux** — `enableColor` / `enableMask` activent (ou non) la peinture et le masquage ;
  `resolveToGBuffer` décide si la passe *Resolve* recompose vers le G-buffer principal.
- **Garde-fous (GPU / threading)** — `maxDabsPerFrame` (256 par défaut) est une **borne dure
  anti-explosion** : même un geste très rapide ne peut pas inonder le GPU ; `dabSpacing` (× rayon)
  espace les tampons le long du trace pour éviter de redessiner mille fois le même pixel.

### `NkSculptStats`, `NkSculptRect` à fond

Ce sont les deux objets « vivants » de `NkSculptTypes` (corps définis). `NkSculptStats` agrège par
frame `dabsDispatched`, `tilesDispatched`, `pixolsTouched` et `cpuMs` — exactement ce qu'un **HUD de
profilage** ou un **panneau d'éditeur** affiche pour comprendre le coût d'un geste ; `Reset()` fait
`*this = NkSculptStats{}` en début de frame. `NkSculptRect` (`x, y, w, h`) est le **dirty rect** : la
zone à re-dispatcher, et son `IsEmpty()` (`w <= 0 || h <= 0`) est la garde « rien à faire » qui évite
un dispatch nul.

### `NkSculptBrush`, `NkSculptDab`, `MakeBrushGPU` à fond

Le découpage **outil / tampon / bloc GPU** est volontaire. `NkSculptBrush` est l'état que **l'UI
manipule** : un panneau d'outils règle `mode`, `falloff`, `radiusPx`, `strength`, `hardness`,
`invert`, `color` (pour `NK_PAINT`) et `depthBias`. `NkSculptDab` est ce que le **système d'entrée**
produit à chaque échantillon de pointeur : une `screenPos`, un `radiusPx` et une `pressure` (une
tablette graphique alimente naturellement ce dernier).

`MakeBrushGPU` est la couture entre les deux et le GPU, et c'est la seule logique réelle ici. Elle
remplit `NkSculptBrushGPU` ainsi : le **centre et le rayon viennent du dab** (pas de l'outil — d'où
le fait que `NkSculptBrush::radiusPx` est **ignoré**), la **force est modulée par la pression**
(`strength = b.strength * dab.pressure`), couleur/mode/falloff/dureté/`depthBias` sont copiés, et
l'offset de tuile est posé. Elle **n'utilise pas** `NkSculptBrush::invert`. Pour qui touche au
shader : `NkSculptBrushGPU` fait **64 octets std430** et son layout est le **contrat** avec
`sculpt_brush.comp.glsl` — toute modif d'un champ doit être répercutée des deux côtés.

### `NkPixolBuffer` à fond

C'est le **canvas GPU** et le cœur de données du sous-système. Pensé en Create/Destroy
(`Init`/`Shutdown`/`Resize`), il porte cinq *storage images* écran et sait s'insérer dans le render
graph. Les **handles** (`Depth/Normal/Material/Color/Mask`, définis) sont ce qu'un shader lie ; les
**ResId** (`ResDepth/ResNormal/ResColor/ResMask`, en `uint32` pour ne pas inclure le header du
graph) sont ce que le graph utilise pour ordonnancer et insérer les barrières. L'**asymétrie** à
mémoriser : `Material()` existe comme handle, mais **aucun `ResMaterial()`** n'est exposé — le
matériau n'est pas, pour l'instant, une ressource déclarée du graph. `Clear(cmd)` réinitialise le
canvas (profondeur à `+inf`, masque à 0) : l'état « page blanche ».

### `NkSculptStroke` à fond

Le trace fait le pont entre l'**entrée** (mouvements de pointeur, IO) et le **GPU** (dabs prêts à
dispatcher). `Begin(brush)` fige la brosse du trace ; `AddSample(pos, pressure)` **interpole** entre
le dernier échantillon et le nouveau et **génère des dabs espacés** (selon `dabSpacing` de la
config) — c'est ce qui transforme un déplacement rapide en une ligne de tampons régulière plutôt
qu'en quelques points isolés ; `End()` clôt le trace. Le système lit `PendingDabs()` et
`DirtyRect()` (renvoyé **par valeur**), dispatche, puis appelle `ClearPending()` pour vider la file
sans perdre le trace. En interne, `ExpandDirty` **aligne le dirty rect sur `kNkSculptTileSize`** :
garantie que les groupes compute tombent sur des frontières de tuile.

### `NkSculptPipelines` et `NkPixolSculptSystem` à fond

`NkSculptPipelines` encapsule deux kernels via `NkComputeContext::GetOrCompileGLSL` :
`Brush()` (un seul kernel, le **mode est une donnée** de la push-constant, pas un pipeline distinct —
moins de combinatoire) et `Resolve()` (composite pixol → G-buffer). Tous deux sont **non-const** car
compilés **paresseusement** au premier appel et mis en cache. Le chemin actif est **GLSL** : NkSL
n'est pas utilisé ici.

`NkPixolSculptSystem` orchestre tout. Comme `NkDeferredPass`, on l'initialise
(`Init(device, graph, texLib, shaderLib, …)`) puis on appelle `RegisterToRenderGraph()` **dans**
`BuildDefaultRenderGraph()`, **avant** la passe d'éclairage deferred — ainsi le relief sculpté est
prêt quand l'éclairage le consomme. Il enregistre **deux passes compute** : *Brush* (applique les
dabs sur le canvas) puis *Resolve* (recompose vers le G-buffer). Les corps réels vivent dans des
**callbacks** (`RecordBrushPass` / `RecordResolvePass`) exécutés au `NkRenderGraph::Execute`, le
graph se chargeant des **barrières**. Subtilité de durée de vie : `mCompute` (un `NkComputeContext`
**par valeur**) est déclaré **avant** `mPipelines`, donc détruit **après** lui — l'ordre de
déclaration **est** la garantie de destruction correcte. Le système garde aussi un `mDabBuffer`
(SSBO de `NkSculptBrushGPU`) comme **alternative batch** aux push-constants quand beaucoup de dabs
tombent dans la même frame.

### La voie future : `NkSculptTileStore` (HORS-MVP)

C'est l'autre embranchement, celui de la **vraie géométrie multirésolution** (HD Geometry à la
ZBrush) : au lieu de muter des pixels, on stockerait des **patchs subdivisés** (`NkSculptTile`,
résidents ou non) et des **deltas de position quantifiés** (`NkSculptDelta`, 16 bits par composante,
pour comprimer des millions de micro-déplacements). Mais **rien n'est actif** : la classe
`NkSculptTileStore` n'a que ses constructeur/destructeur par défaut, et tout le reste
(`Init`/`Shutdown`/`SetActiveRegion`/`ApplyDelta`/`BakeAndEvict`, membres `mTiles`/`mDeltas`) est
**entièrement commenté**. À ne pas considérer comme une API — seulement comme une intention notée
dans le code.

### Idiomes & pièges transversaux

- **Create/Destroy partout.** `NkPixolBuffer`, `NkSculptPipelines` et `NkPixolSculptSystem` suivent
  Init/Shutdown explicite — cohérent avec la règle dure NKMemory (tout `Create` a son `Destroy`,
  jamais de `new`/`delete` brut).
- **Coût borné (GPU/threading).** Le travail réel se fait dans les callbacks du render graph au
  moment d'`Execute` ; le coût est plafonné par `DirtyRect()` (aligné `kNkSculptTileSize`) et
  `maxDabsPerFrame`. C'est l'argument de vente du pixol face au sculpting géométrique.
- **Contrat GPU.** `NkSculptBrushGPU` (64 o, std430) **doit** rester synchronisé avec
  `sculpt_brush.comp.glsl` ; le rayon/centre viennent du dab, la force est × pression.
- **Scope d'enums.** `NK_SMOOTH` et `NK_COUNT` sont ambigus entre `NkSculptBrushMode` et
  `NkSculptFalloff` — toujours qualifier.
- **Asymétries réelles.** `Material()` sans `ResMaterial()` ; `MakeBrushGPU` qui ignore `invert` et
  `radiusPx` de la brosse.
- **Rien n'est fonctionnel.** Seuls les structs, les accesseurs *inline*, `Reset()` / `IsEmpty()` /
  `IsValid()` et `MakeBrushGPU()` ont un corps réel. Tout le reste est déclaré.

---

### Exemple

> ⚠️ **Code illustratif** : il décrit le **flux prévu** (Init → trace → Resolve via le graph). La
> plupart des méthodes appelées sont **déclarées seulement** — ce listing ne compile pas en l'état.

```cpp
#include "NKRenderer/Tools/PixolSculpt/NkPixolSculptSystem.h"
using namespace nkentseu::renderer;

// 1) Création : le canvas suit le swapchain (width/height = 0).
NkPixolSculptConfig cfg;
cfg.maxDabsPerFrame = 256;     // borne dure anti-explosion
cfg.dabSpacing      = 0.25f;   // tampons espacés de 0.25 × rayon

NkPixolSculptSystem sculpt;
sculpt.Init(device, graph, texLib, shaderLib, /*w*/0, /*h*/0, cfg);
sculpt.RegisterToRenderGraph();   // dans BuildDefaultRenderGraph(), AVANT le deferred

// 2) Réglage de la brosse (côté outil / UI).
NkSculptBrush brush;
brush.mode     = NkSculptBrushMode::NK_RAISE;     // soulève le relief
brush.falloff  = NkSculptFalloff::NK_SMOOTH;      // bien qualifier : pas le NK_SMOOTH du mode
brush.strength = 0.6f;
sculpt.SetBrush(brush);

// 3) Un trace : appui, déplacements (pression), relâché.
sculpt.BeginStroke({320.f, 240.f}, /*pressure*/1.f);
sculpt.AddStrokeSample({340.f, 250.f}, 0.8f);     // dabs interpolés + espacés
sculpt.AddStrokeSample({360.f, 245.f}, 0.5f);
sculpt.EndStroke();

// 4) Les passes Brush + Resolve s'exécutent au NkRenderGraph::Execute (callbacks).
const NkSculptStats& s = sculpt.Stats();          // dabs/tiles/pixols touchés, cpuMs
```

---

[← Index NKRenderer](README.md) · [Récap NKRenderer](../NKRenderer.md) · [Couche Runtime](../README.md)
