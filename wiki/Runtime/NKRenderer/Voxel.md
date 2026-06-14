# Le système Voxel

> Couche **Runtime** · NKRenderer · Sculpter un **volume de voxels** sur GPU : la grille de
> storage 3D `NkVoxelVolume`, la brosse `NkVoxelBrush`, le tracé `NkVoxelStroke`, les pipelines
> compute `NkVoxelPipelines` et la façade `NkVoxelSystem` qui orchestre tout.

Sculpter de la matière en temps réel — ajouter de la pâte sous le pinceau, en creuser, la lisser,
la peindre — ce n'est pas du rendu de maillage classique. Un maillage est une **surface** figée
(des sommets, des triangles) ; un *voxel field* est un **volume** : une grille 3D où chaque cellule
porte une densité (suis-je dedans ou dehors ?), un matériau et une couleur. Sculpter revient alors à
**écrire dans cette grille**, puis à la **rendre** en la traversant. Le système Voxel de NKRenderer
est exactement cela : une pile de sculpture GPU façon ZBrush, où la brosse devient un kernel compute
et le tracé du stylet une suite de tampons écrits dans des textures 3D.

Le fil rouge de toute cette page est le **coût borné**. Une grille `256³` fait 16 millions de
cellules ; il serait absurde de les retoucher toutes à chaque frame. Tout le module est donc
construit autour d'une idée : **ne travailler que sur la région réellement modifiée** — la *dirty
box* 3D autour du coup de pinceau, alignée sur des tuiles, plafonnée par une borne dure de tampons
par frame. Le GPU ne touche que ce que vous sculptez vraiment.

> **Attention — statut.** Ce module est déclaré **SQUELETTE** dans ses en-têtes : la plupart des
> méthodes sont posées sans implémentation garantie (« à implémenter/tester plus tard »), et
> `NkVoxelBrickPool` est explicitement **futur / hors-MVP** (corps commenté). Cette page documente
> l'**API telle qu'elle est déclarée** ; les seules parties pleinement écrites sont les structures de
> données (`NkVoxelTypes`, `NkVoxelBrush`) et la fonction inline `MakeVoxelGPU`.

- **Namespace** : `nkentseu::renderer` (les interfaces RHI `NkIDevice` / `NkICommandBuffer` /
  `NkComputeContext` vivent au scope `nkentseu`)
- **Headers** : `#include "NKRenderer/Tools/Voxel/NkVoxelSystem.h"` (façade), plus
  `NkVoxelTypes.h`, `NkVoxelBrush.h`, `NkVoxelStroke.h`, `NkVoxelVolume.h`, `NkVoxelPipelines.h`,
  `NkVoxelBrickPool.h` selon le besoin

---

## Le volume : une grille de storage 3D

Au cœur du système, `NkVoxelVolume` détient **trois textures 3D** en accès non ordonné
(`NK_UNORDERED_ACCESS`) : la **densité** (`NK_R16_FLOAT`, une valeur signée façon SDF, ou une
occupation), le **matériau** (`NK_RGBA8_UNORM` : id de matériau, roughness, metallic) et la
**couleur** (`NK_RGBA8_UNORM`, l'albédo peint). Ce sont ces cibles que les kernels de brosse
**écrivent**, et que le raymarch **lit** pour produire l'image.

Le volume ne se manipule pas directement par l'application : on le crée avec `Init(device, cfg)` —
où la `NkVoxelConfig` fixe la résolution (`dimX/Y/Z`, `256³` par défaut), la taille d'un voxel en
unités monde (`voxelSize`) et le coin du repère (`originWorld`) — puis on le libère avec
`Shutdown()`. La paire est symétrique, comme partout dans le module.

Ce n'est **pas** un buffer que vous remplissez à la main côté CPU : tout transit par le GPU. Pour
remettre le volume à zéro, `Clear(cmd)` enregistre une **commande GPU** (pas un `memset` CPU). Et
pour brancher le volume dans le pipeline de rendu différé, `ImportToGraph(graph)` l'importe dans le
[render graph](RenderGraph.md) avec l'état `NK_UNORDERED_ACCESS`, exposant ensuite ses IDs de
ressource (`ResDensity()`, `ResMaterial()`, `ResColor()`, de type `NkGraphResId == uint32`).

> **En résumé.** `NkVoxelVolume` = trois textures 3D storage (densité / matériau / couleur),
> écrites par les brosses et lues par le raymarch. Cycle `Init`/`Shutdown`, `Clear(cmd)` côté GPU,
> `ImportToGraph` pour le brancher dans le render graph. Conçu pour être **partagé** avec
> `NkVoxelAOSystem` plutôt que dupliqué — pas pour être édité octet par octet côté CPU.

---

## La brosse et le tampon : décrire un coup de pinceau

Sculpter, c'est appliquer une **brosse** en un point. `NkVoxelBrush` décrit l'outil côté
CPU : son **mode** (`NkVoxelBrushMode` — ajouter, soustraire, peindre, lisser, aplatir), son profil
d'atténuation radiale (`NkVoxelFalloff` — lisse, linéaire, constant, sphère), son **rayon** en voxels
(`radiusVox`), son **intensité** (`strength`, dans `[0..1]`) et, pour le mode peinture, sa **couleur**.

Un point unique du tracé est un `NkVoxelDab` (« tampon ») : un centre en **espace-grille** (des
coordonnées voxel, éventuellement fractionnaires), un rayon, et une **pression** de stylet dans
`[0..1]`. La distinction est volontaire : la brosse est *l'outil* (réglages durables), le dab est
*une empreinte* de cet outil à un endroit précis avec une certaine pression.

Pour parler au kernel compute, il faut traduire tout cela en un **bloc push-constant** strictement
aligné sur le shader. C'est le rôle de `NkVoxelBrushGPU` : un struct **`std430`, aligné 16, de 64
octets exactement**, dont chaque offset est documenté (centre @0, rayon @12, couleur @16, mode @32,
falloff @36, strength @40, l'origine de la boîte dispatchée @48/52/56, padding). La fonction inline
`MakeVoxelGPU(brush, dab, boxX, boxY, boxZ)` le remplit : elle recopie centre, rayon et couleur,
caste `mode`/`falloff` en `uint32`, place l'origine de la boîte, et — détail important — calcule
`strength = brush.strength * dab.pressure` : **la pression du stylet module l'intensité**.

> **En résumé.** `NkVoxelBrush` = l'outil (mode, falloff, rayon, force, couleur). `NkVoxelDab` =
> une empreinte (centre voxel, rayon, pression). `NkVoxelBrushGPU` = leur traduction en
> push-constant `std430` de **64 octets** ; `MakeVoxelGPU` la fabrique et y mêle la pression.
> **Piège dur** : toute modification de `NkVoxelBrushGPU` doit être répercutée à l'identique dans le
> shader `voxel_edit.*`, sinon le layout casse.

---

## Le tracé : du geste à la dirty box

Un coup de pinceau n'est pas un point isolé : c'est un **geste continu**. `NkVoxelStroke` accumule
les dabs le long de ce geste **et** maintient la *dirty box* — l'union 3D des boîtes touchées depuis
le dernier dispatch. C'est elle qui garantit le coût borné : le GPU ne traitera **que** ce volume.

Le cycle est naturel : `Begin(brush)` démarre le tracé avec une brosse, puis chaque mouvement du
stylet appelle `AddSample(centerVox, pressure)`. Cet appel ne pose pas un dab brut au point reçu : il
**interpole** depuis l'échantillon précédent et **génère des dabs régulièrement espacés** (selon
`brush.dabSpacing × rayon`), pour qu'un trait rapide ne laisse pas de trous. `End()` clôt le geste.

Une fois le dispatch GPU effectué, on appelle `ClearPending()` pour vider les dabs déjà appliqués
(et `Reset()` pour tout remettre à zéro). Entre-temps, on lit l'état du tracé : `IsActive()`,
`PendingDabs()` (le `NkVector<NkVoxelDab>` en attente), `DirtyBox()` et `Brush()`.

Le mécanisme clé est l'agrandissement de la dirty box. En interne, `ExpandDirty(dab)` étend la
boîte pour englober chaque nouveau dab, puis l'**aligne sur la grille de tuiles** `kNkVoxelTileSize`
(4, soit des tuiles `4×4×4` = 64 threads par groupe compute) et la **clampe aux dimensions** de la
grille. C'est ce qui fait que le travail dispatché tombe pile sur des frontières de groupe et ne
déborde jamais du volume.

> **En résumé.** `NkVoxelStroke` transforme un geste (`Begin` → `AddSample`* → `End`) en une suite
> de dabs **espacés et interpolés**, et maintient la `DirtyBox()` (alignée tuile, clampée à la
> grille) — la région exacte à dispatcher. Après le dispatch : `ClearPending()`. Le coût suit le
> **volume édité**, pas la grille entière.

---

## Les pipelines et la façade : l'orchestration

Deux pièces restent à brancher. `NkVoxelPipelines` est le **registre des pipelines compute** : il
s'appuie sur un `NkComputeContext` (qui possède le cache et compile les kernels GLSL Vulkan-style
inline, sans I/O fichier) et expose deux pipelines — `Edit()` (le **kernel d'édition unique** ; le
mode de brosse arrive en push-constant, on ne change pas de pipeline par mode) et `Raymarch()` (qui
traverse le volume pour écrire dans le G-buffer). Il **n'est pas propriétaire** du contexte
(pointeur emprunté).

Au sommet, `NkVoxelSystem` est la **façade** que l'application utilise réellement. Il possède le
volume, le tracé, les pipelines et son propre `NkComputeContext`, et il s'enregistre dans le
[render graph](RenderGraph.md) via `RegisterToRenderGraph()` : une passe `NK_COMPUTE` « Voxel_Edit »
qui applique les dabs, suivie d'une passe de rendu « Voxel_Raymarch » qui résout vers le G-buffer
différé. Toute l'**API d'édition se fait en coordonnées MONDE** (`BeginStrokeWorld`,
`AddStrokeSampleWorld`, `EndStroke`) : le système convertit en interne via `WorldToVoxel` /
`VoxelToWorld` (selon `origin` + `voxelSize` de la config). Vous pensez en mètres, pas en indices de
cellule.

Ce n'est **pas** un sous-système qu'on pilote frame par frame en bas niveau : on l'`Init`-ialise une
fois (avec device, graph, librairies de textures et de shaders, config), on règle la brosse
(`SetBrush`), on lui envoie des gestes en coordonnées monde, on lui donne la caméra (`SetCamera`,
pour le raymarch), et il fait le reste.

> **En résumé.** `NkVoxelPipelines` fournit les deux compute pipelines (Edit à kernel unique +
> mode en push-constant, Raymarch). `NkVoxelSystem` est la façade : il **possède** volume / tracé /
> pipelines / contexte compute, s'enregistre dans le render graph (Edit puis Raymarch), et prend
> toutes ses entrées d'édition en **coordonnées monde**. On l'`Init`/`Shutdown`, on règle la brosse,
> on lui envoie des gestes — il convertit et dispatche.

---

## Aperçu de l'API

Les six en-têtes du dossier `Tools/Voxel/`. Tout est `noexcept` ; toute la mémoire passe par
NKMemory (conteneurs `NkVector`), jamais par `new`/`delete`.

### Types et configuration — `NkVoxelTypes.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Constantes | `kNkVoxelVersion` (=1), `kNkVoxelTileSize` (=4) | Version ; côté de tuile compute 3D (`4³` = 64 threads/groupe). |
| Mode brosse | `enum class NkVoxelBrushMode : uint8` | `NK_ADD`, `NK_SUB`, `NK_PAINT`, `NK_SMOOTH`, `NK_FLATTEN`, `NK_COUNT`. |
| Atténuation | `enum class NkVoxelFalloff : uint8` | `NK_SMOOTH`, `NK_LINEAR`, `NK_CONSTANT`, `NK_SPHERE`, `NK_COUNT`. |
| Formats | `struct NkVoxelFormat` | Formats des cibles 3D : `density` (R16F), `material` (RGBA8), `color` (RGBA8). |
| Config | `struct NkVoxelConfig` | `dim{X,Y,Z}`, `voxelSize`, `originWorld`, `formats`, `enableColor`, `resolveToGBuffer`, `maxDabsPerFrame`, `dabSpacing`. |
| Stats | `struct NkVoxelStats` + `Reset()` | `dabsDispatched`, `bricksDispatched`, `voxelsTouched`, `cpuMs`. |
| Région | `struct NkVoxelBox` + `IsEmpty`/`Width`/`Height`/`Depth` | Dirty region 3D en espace-grille. |

### Brosse, tampon, push-constant — `NkVoxelBrush.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Outil | `struct NkVoxelBrush` | `mode`, `falloff`, `radiusVox`, `strength`, `color`. |
| Empreinte | `struct NkVoxelDab` | `centerVox` (fractionnaire OK), `radiusVox`, `pressure`. |
| Push-constant | `struct NkVoxelBrushGPU` | Bloc `std430`, aligné 16, **64 octets** ; doit matcher `voxel_edit.*`. |
| Fabrique | `MakeVoxelGPU(b, dab, boxX, boxY, boxZ)` | Remplit le bloc ; `strength = b.strength * dab.pressure`. |

### Tracé — `NkVoxelStroke.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle | `Begin(brush)`, `End`, `Reset` | Démarrer / clore / réinitialiser un geste. |
| Échantillons | `AddSample(centerVox, pressure)` | Interpole et génère les dabs espacés. |
| État | `IsActive`, `PendingDabs`, `DirtyBox`, `Brush` | Actif ? ; dabs en attente ; boîte salie ; brosse. |
| Après dispatch | `ClearPending` | Vide les dabs appliqués. |

### Volume — `NkVoxelVolume.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle | `Init(device, cfg)`, `Shutdown`, `IsValid` | Crée / libère les cibles 3D ; prêt ? |
| GPU | `Clear(cmd)` | Efface le volume (commande GPU). |
| Dimensions | `DimX`/`DimY`/`DimZ` | Résolution depuis la config. |
| Handles | `Density`/`Material`/`Color` (`NkTextureHandle`) | Les trois cibles storage 3D. |
| Render graph | `ImportToGraph(graph)`, `ResDensity`/`ResMaterial`/`ResColor` | Importe (état UAV) ; IDs de ressource. |

### Pipelines compute — `NkVoxelPipelines.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle | `Init(ctx)`, `Shutdown`, `IsValid` | Branche le `NkComputeContext` (emprunté). |
| Pipelines | `Edit()`, `Raymarch()` (`NkPipelineHandle`) | Édition (kernel unique) / raymarch → G-buffer. |

### Façade — `NkVoxelSystem.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle | `Init(device, graph, texLib, shaderLib, cfg)`, `Shutdown`, `IsValid` | Crée tout le sous-système. |
| Render graph | `RegisterToRenderGraph`, `SetCamera(invViewProj, camPos)` | Passes Edit + Raymarch ; caméra du raymarch. |
| Brosse | `SetBrush`, `GetBrush` | Régler / lire la brosse courante. |
| Édition (monde) | `BeginStrokeWorld`, `AddStrokeSampleWorld`, `EndStroke`, `ClearVolume` | Gestes en coordonnées **monde**. |
| Conversion | `WorldToVoxel`, `VoxelToWorld` | Monde ↔ espace-grille (origin + voxelSize). |
| Accès | `Volume`, `Stats` | Le volume ; les stats de frame. |

### Briques éparses — `NkVoxelBrickPool.h` (futur / hors-MVP)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Constante | `kNkBrickSize` (=16) | `16³` voxels par brique. |
| Brique | `struct NkVoxelBrick` | `poolSlot`, `resident`, `dirty`, padding. |
| Pool | `class NkVoxelBrickPool` (ctor/dtor seuls actifs) | Squelette **non implémenté** (le reste est commenté). |

---

## Référence complète

### Choisir son point d'entrée

Trois niveaux de lecture, selon ce que vous faites :

- **Vous utilisez le système** (un éditeur de sculpture, un outil de terrain) → vous ne touchez que
  `NkVoxelSystem` et `NkVoxelBrush`. Tout le reste est interne.
- **Vous reliez le système au rendu** → vous passez par `NkVoxelVolume::ImportToGraph` et les passes
  de `RegisterToRenderGraph`, et vous lisez les `NkTextureHandle` / `NkGraphResId`.
- **Vous écrivez le kernel `voxel_edit`** → c'est `NkVoxelBrushGPU` qui vous concerne : son layout
  `std430` de 64 octets est votre contrat avec le shader.

### Les enums : modes et atténuation à fond

`NkVoxelBrushMode` énumère les **opérations** de sculpture. Conceptuellement chaque mode est un
kernel distinct, mais dans l'implémentation le pipeline d'édition est **unique** et reçoit le mode
en push-constant (un `switch` côté GPU) :

- **`NK_ADD` / `NK_SUB`** — déposer ou creuser de la matière (la densité monte / descend dans le
  rayon). C'est le geste de base de toute sculpture : bâtir une forme, y tailler une cavité.
- **`NK_PAINT`** — n'altère pas la densité mais écrit la **couleur** (`NkVoxelBrush::color`) dans la
  cible albédo. C'est la peinture de surface : texturer une roche, colorer un personnage sculpté.
- **`NK_SMOOTH`** — moyenne le voisinage pour **adoucir** les arêtes (anti-bruit, finition d'une
  forme grossière). L'équivalent du « smooth » de ZBrush.
- **`NK_FLATTEN`** — rabat la matière vers un **plan** (planches, facettes, surfaces dures). Utile
  pour passer d'un blob organique à des plans nets.

`NkVoxelFalloff` décrit **comment l'intensité décroît** du centre vers le bord du rayon —
`NK_SMOOTH` (transition douce, bords fondus, le réglage naturel), `NK_LINEAR` (décroissance droite),
`NK_CONSTANT` (plein partout jusqu'au bord, pour un effet « tampon » net) et `NK_SPHERE` (profil
sphérique, pour des dépôts en boule). Le falloff change radicalement le **toucher** de la brosse à
mode identique.

`kNkVoxelTileSize` (=4) n'est pas cosmétique : il fixe la granularité du dispatch (`4×4×4` = 64
threads, soit `local_size_x/y/z` du kernel), et c'est sur **cette** grille que la dirty box est
alignée. `kNkVoxelVersion` (=1) tague le format pour la sérialisation future.

### `NkVoxelConfig` et `NkVoxelFormat` : le contrat de création

`NkVoxelConfig` est l'unique paramètre de `Init`. Quelques champs gouvernent tout :

- **`dimX/dimY/dimZ`** (256 chacun) — la résolution. Le compromis est brutal : la mémoire et le coût
  du raymarch croissent en `O(dim³)`. C'est la raison d'être de toute la mécanique de dirty box.
- **`voxelSize`** (0.05) et **`originWorld`** — l'ancrage dans la scène : un voxel mesure
  `voxelSize` unités monde, et la cellule `(0,0,0)` tombe à `originWorld`. Ce sont eux qui pilotent
  `WorldToVoxel` / `VoxelToWorld`.
- **`maxDabsPerFrame`** (128) — la **borne dure** de travail par frame. Même un trait frénétique ne
  dispatche jamais plus que ce plafond : c'est la garantie de stabilité du framerate.
- **`dabSpacing`** (0.35) — l'espacement des tampons le long du tracé, en fraction du rayon. Plus
  petit = trait plus dense (plus lisse, plus cher) ; plus grand = trait pointillé.
- **`resolveToGBuffer`** (true) — choisit la voie raymarch → G-buffer différé ; **`enableColor`**
  active la cible couleur.

`NkVoxelFormat` fixe les formats des trois cibles. Les défauts traduisent leur rôle : `R16_FLOAT`
pour la densité (une valeur signée, façon SDF, qui supporte le lissage), `RGBA8_UNORM` pour le
matériau et la couleur (4 canaux 8 bits suffisent pour id/roughness/metallic et un albédo peint).

### `NkVoxelStats` et `NkVoxelBox` : mesurer et borner

`NkVoxelStats` est l'instrument de bord d'une frame : combien de dabs (`dabsDispatched`) et de tuiles
3D (`bricksDispatched`) ont été dispatchés, combien de voxels touchés (`voxelsTouched`, une
estimation bornée par `dim³`), et le temps CPU (`cpuMs`). `Reset()` repart de zéro (`*this =
NkVoxelStats{}`). C'est le retour à brancher dans un overlay de profilage d'éditeur.

`NkVoxelBox` est la **dirty region** elle-même : six entiers (`min/max` × XYZ) en espace-grille.
Ses accesseurs disent l'essentiel — `IsEmpty()` (rien à faire : `max <= min` sur un axe), et
`Width()`/`Height()`/`Depth()` donnent les dimensions du sous-volume à dispatcher. C'est ce que
`NkVoxelStroke::DirtyBox()` renvoie et que la passe Edit consomme.

### La brosse, le dab et le push-constant à fond

La séparation `NkVoxelBrush` (outil) / `NkVoxelDab` (empreinte) est le cœur ergonomique. La brosse
porte les réglages *persistants* qu'un utilisateur ajuste dans un panneau (mode, falloff, rayon,
force, couleur) ; le dab est l'application *ponctuelle* de cet outil — un centre en coordonnées
voxel (fractionnaire, car le stylet ne tombe pas pile sur une cellule) et la **pression** instantanée
du stylet.

`NkVoxelBrushGPU` est le seul endroit où le moteur **dialogue bit à bit avec le shader**. Son layout
`std430`, aligné 16, de **64 octets**, n'est pas négociable : chaque offset (centre @0, rayon @12,
couleur @16, mode @32, falloff @36, strength @40, padding @44, boxOffset @48/52/56, padding @60)
correspond à un champ du bloc déclaré dans `voxel_edit.*`. C'est le **piège dur** du module :
ajouter un champ ou réordonner ici sans toucher le shader produit un décalage silencieux qui corrompt
toute l'édition. Les deux `_pad` ne sont pas du gaspillage — ils maintiennent l'alignement `std430`.

`MakeVoxelGPU` (inline, complète) est la seule logique « écrite » de cette partie : elle recopie
centre/rayon/couleur, caste `mode` et `falloff` en `uint32`, pose l'origine de la boîte dispatchée,
et calcule **`strength = b.strength * dab.pressure`**. Ce produit est ce qui donne au stylet sa
sensibilité : appuyer fort sculpte plus profond, effleurer dépose à peine.

### `NkVoxelStroke` à fond : le coût qui suit le geste

Le tracé est la pièce qui transforme un module « volume + brosse » en outil de sculpture utilisable.
Trois responsabilités :

- **Échantillonnage propre.** `AddSample` ne pose pas un dab par appel : il **interpole** depuis le
  point précédent et sème des dabs tous les `brush.dabSpacing × rayon`. Sans cela, un mouvement
  rapide de souris/stylet laisserait une ligne de pointillés au lieu d'un trait continu.
- **Dirty box minimale.** À chaque dab, `ExpandDirty` agrandit la boîte juste assez pour l'englober,
  puis **arrondit aux tuiles** `kNkVoxelTileSize` (pour tomber sur des frontières de groupe compute)
  et **clampe aux dimensions** (pour ne jamais déborder). Le résultat de `DirtyBox()` est donc
  toujours dispatchable tel quel et toujours dans les bornes du volume.
- **Cycle clair.** `Begin(brush)` arme le tracé, `AddSample`* le nourrit, `End()` le clôt ;
  `IsActive()` dit où on en est. Après que la passe GPU a consommé les dabs, `ClearPending()` les
  retire (la dirty box repart d'une base propre pour le prochain segment), `Reset()` remet tout à
  zéro. Les dabs en attente sont un `NkVector<NkVoxelDab>` — donc alloués via NKMemory, jamais via
  `new`.

Au-delà de la sculpture, ce motif « accumuler un geste, maintenir une région sale bornée, dispatcher
seulement le delta » se transpose : un **outil de terrain** (élévation par pinceau), un **système de
fluide/fumée** voxelisé édité à la souris, un **éditeur de peinture 3D** sur volume. Partout où l'on
édite un grand champ régulier de façon locale, la dirty box est le bon réflexe.

### `NkVoxelVolume` à fond : posséder les trois cibles

Le volume **possède** ses trois `NkTextureHandle` (densité, matériau, couleur) et suit la discipline
du module : `Init(device, cfg)` / `Shutdown()` symétriques, drapeau `IsValid()` (interne `mReady`),
membres privés `CreateTargets()` / `DestroyTargets()`. Les dimensions se relisent par
`DimX/Y/Z()` (depuis la config), et les handles par `Density()` / `Material()` / `Color()`.

Deux interactions sont GPU-only et méritent attention. `Clear(cmd)` n'est **pas** un effacement CPU :
il enregistre une commande dans un `NkICommandBuffer`, parce qu'effacer 16 M de cellules se fait sur
le GPU. `ImportToGraph(graph)` insère les cibles dans le [render graph](RenderGraph.md) à l'état
`NK_UNORDERED_ACCESS` (puisque les kernels y écrivent), après quoi `ResDensity/Material/Color()`
livrent les `NkGraphResId` (`uint32`) que les passes référencent.

L'**idiome d'ownership** noté dans l'en-tête est important : le volume est conçu pour être **partagé**
avec `NkVoxelAOSystem` (le sous-système d'occlusion ambiante voxel) plutôt que dupliqué — un seul
champ de densité sert à la fois à la sculpture et à l'AO.

### `NkVoxelPipelines` à fond : un kernel d'édition, pas cinq

Le registre des pipelines repose entièrement sur le `NkComputeContext` qu'on lui passe à `Init(ctx)`
— et qu'il **n'emprunte que** (pointeur `mCtx`, jamais possédé). C'est ce contexte qui détient le
cache de pipelines et compile les kernels via `GetOrCompileGLSL` : les sources GLSL Vulkan-style sont
**inline dans le code**, sans aucune I/O fichier. `IsValid()` se réduit à `mCtx != nullptr`.

Le choix de conception le plus marquant : `Edit()` renvoie **un seul** pipeline pour les cinq modes
de brosse. Plutôt que cinq kernels à brancher, on en compile un, et `NkVoxelBrushGPU::mode`
sélectionne l'opération côté shader. Moins de pipelines à gérer, moins de changements d'état GPU.
`Raymarch()` fournit le second pipeline, celui qui traverse le volume pour alimenter le G-buffer.

### `NkVoxelSystem` à fond : la façade et son ordre de destruction

C'est l'objet que l'application tient. Il **possède par valeur** `mVolume`, `mStroke`, `mPipelines`
et `mCompute` (le `NkComputeContext`), et **emprunte** `mDevice`, `mGraph`, `mTexLib`, `mShaders`
(non possédés). `Init(device, graph, texLib, shaderLib, cfg = {})` câble le tout ; `Shutdown()` /
`IsValid()` (interne `mReady`) ferment le cycle.

Un **piège d'ownership** est explicitement noté dans l'en-tête : `mCompute` est déclaré **avant**
`mPipelines`, parce que le contexte compute possède le cache que les pipelines utilisent. L'ordre de
destruction étant l'inverse de la déclaration, les pipelines sont donc détruits **avant** le contexte
qui les sous-tend — sans cet ordre, on libérerait le cache avant ses utilisateurs.

Le service rendu à l'application est la **conversion automatique monde ↔ voxel**. Toute l'édition
publique parle en coordonnées monde : `BeginStrokeWorld(posWorld, pressure)`,
`AddStrokeSampleWorld(...)`, `EndStroke()`, `ClearVolume()`. En interne, `WorldToVoxel` /
`VoxelToWorld` (basées sur `origin` + `voxelSize`) font la traduction, exposées au cas où on en a
besoin. `SetCamera(invViewProj, camPos)` fournit au raymarch de quoi reconstruire les rayons, et
`RegisterToRenderGraph()` pose la passe `NK_COMPUTE` « Voxel_Edit » suivie de la passe rendu
« Voxel_Raymarch » → G-buffer. Le header prévoit deux voies de rendu en config — **RAYMARCH** (par
défaut) et **MESHING** (futur) — mais seule la première est en place. Les `RecordEditPass` /
`RecordRaymarchPass` privés sont les corps de ces passes.

### `NkVoxelBrickPool` : à connaître, pas à utiliser

Cet en-tête est un **squelette de réflexion**, explicitement **futur / hors-MVP**. L'idée : un
volume **épars en briques** (`kNkBrickSize = 16`, soit `16³` voxels par brique), l'équivalent 3D de
la « HD Geometry » de ZBrush, où l'on ne garde résidentes en mémoire que les briques effectivement
sculptées. `NkVoxelBrick` décrit une telle brique (slot de pool, drapeaux `resident`/`dirty`).

Mais concrètement : **seuls le constructeur et le destructeur de `NkVoxelBrickPool` sont déclarés**.
Tout le reste — `Init`, `Shutdown`, `EnsureResident`, `EvictOutside`, `SlotOf`, la page table, l'atlas
GPU — est **en commentaires** dans le header. Ce n'est donc **pas** de l'API : ne l'appelez pas, ne
construisez rien dessus. Le MVP fonctionne sur la grille dense de `NkVoxelVolume`.

### Le socle commun

- **Cycle de vie uniforme.** Partout `Init(...) noexcept` → `Shutdown() noexcept` + un drapeau de
  validité (`IsValid()` / `mReady` / `mCtx != nullptr`). Pas de fabrique statique : les objets sont
  stockés **par valeur** dans `NkVoxelSystem`.
- **Tout est `noexcept`.** L'intégralité des méthodes, fonctions, constructeurs et destructeurs
  déclarés du module est marquée `noexcept`.
- **Mémoire NKMemory.** Aucune allocation CRT exposée ; les dabs en attente passent par un
  `NkVector<NkVoxelDab>` (voir [NKContainers](../../Foundation/NKContainers.md)). Les handles
  viennent du RHI : `NkTextureHandle`, `NkPipelineHandle`, `NkGPUFormat` (NKRHI), et
  `NkGraphResId == uint32` pour le render graph.
- **Coût borné par construction.** Le travail GPU est proportionnel à la **dirty box 3D** (alignée
  `kNkVoxelTileSize`, clampée aux dims), pas à la grille entière, et plafonné par `maxDabsPerFrame`.
- **Synchro CPU↔GPU manuelle.** `NkVoxelBrushGPU` (`std430`, 64 octets) doit rester aligné sur
  `voxel_edit.*` — c'est la seule vigilance permanente du module.

---

### Exemple

```cpp
#include "NKRenderer/Tools/Voxel/NkVoxelSystem.h"
using namespace nkentseu::renderer;

// 1. Création : une grille 256³, branchée sur le render graph.
NkVoxelSystem voxels;
NkVoxelConfig cfg;                       // 256³, voxelSize 0.05, raymarch → G-buffer
voxels.Init(device, graph, texLib, shaderLib, cfg);
voxels.RegisterToRenderGraph();          // passes Voxel_Edit (compute) + Voxel_Raymarch

// 2. Réglage de la brosse : on ajoute de la matière, bords fondus.
NkVoxelBrush brush;
brush.mode     = NkVoxelBrushMode::NK_ADD;
brush.falloff  = NkVoxelFalloff::NK_SMOOTH;
brush.radiusVox = 8.f;
brush.strength = 0.5f;
voxels.SetBrush(brush);

// 3. Un coup de pinceau, en coordonnées MONDE (le système convertit en voxels).
voxels.BeginStrokeWorld(hitWorld, /*pression*/ 1.f);
voxels.AddStrokeSampleWorld(hitWorld + dir * 0.1f, 0.8f);  // dabs interpolés et espacés
voxels.EndStroke();

// 4. Caméra pour le raymarch + lecture des stats de frame.
voxels.SetCamera(invViewProj, camPosWorld);
const NkVoxelStats& s = voxels.Stats();   // dabsDispatched, voxelsTouched, cpuMs…
```

---

[← Index NKRenderer](README.md) · [Récap NKRenderer](../NKRenderer.md) · [Couche Runtime](../README.md)
