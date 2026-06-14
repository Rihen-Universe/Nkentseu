# Shaders et outils 3D

> Couche **Runtime** · NKRHI · Brancher le **vrai compilateur NkSL** (module NKSL) sur le device :
> `NkSLIntegration` pour les six backends GPU, `NkSWShaderBridge` pour fabriquer des lambdas
> exécutables par le rasterizer software, et les outils 3D `NkGrid3D` (grille de référence) +
> les **types** de gizmo.

Un moteur ne se contente pas de *posséder* un compilateur de shaders : il doit le **brancher** sur
chaque API graphique (OpenGL, Vulkan, DirectX, Metal, software), traduire un même langage source
vers la cible de chaque backend, et exposer quelques **outils de scène** (une grille, des gizmos)
qui s'appuient eux-mêmes sur ce pipeline. C'est exactement ce que regroupe la famille
*Shaders-Tools* de NKRHI. La question n'est pas « comment écrire un shader » — ça, c'est le rôle du
langage NkSL — mais « **comment transformer une source NkSL en quelque chose que ce device-ci sait
exécuter** », qu'il s'agisse d'un binaire SPIR-V, d'un HLSL SM6, ou même d'une paire de lambdas C++
pour un rasterizer logiciel.

Un point de vocabulaire crucial, parce qu'il y a **deux NkSL** dans le projet. Ce n'est **PAS** le
transpileur texte ad-hoc de NKRenderer (`NkShaderBackendNkSL`, avec ses `@uniform`/`@target`) : ici
on parle du **vrai compilateur**, le module NKSL (`NkSLCompiler`, avec `@binding`/`@stage`/`@entry`,
reflection structurée, multi-cibles). Toute cette famille — `NkSLIntegration`, `NkSWShaderBridge`,
les shaders des outils — passe par ce compilateur-là.

- **Namespaces** : `nkentseu::nksl` (intégration GPU) · `nkentseu::swbridge` (pont software) ·
  `nkentseu` (outils 3D) · `nkentseu::gizmoshaders` (sources shaders des gizmos)
- **Headers réels** : `NKRHI/SL/NkSLIntegration.h` · `NKRHI/SL/NkSWShaderBridge.h` ·
  `NKRHI/Tools/Grid3D/NkGrid3D.h` · `NKRHI/Tools/Grid3D/NkGrid3DShaders.h` ·
  `NKRHI/Tools/NkGizmo/NkGizmo3DTypes.h`

> **Note factuelle.** Trois pièges de fichiers, documentés tels quels : `NkGizmo3D.h` est **vide**
> (aucune classe de manipulation gizmo n'est livrée) ; le seul header gizmo réel,
> `NkGizmo3DTypes.h`, ne contient que des **types de données** ; et `NkGrid3DShaders.h`, malgré son
> nom, déclare le namespace `gizmoshaders` (les shaders des **gizmos**, pas de la grille).

---

## Brancher NkSL sur le device : `NkSLIntegration`

Le compilateur NkSL sait produire du GLSL, du SPIR-V, du HLSL, du MSL ou du C++. Mais un `NkIDevice`
n'attend pas « du NkSL » : il attend la forme **native** de son API. Tout le rôle de
`NkSLIntegration` est ce **pont** — choisir la bonne cible selon l'API, lancer la compilation, et
construire le `NkShaderHandle` (ou le `NkShaderDesc`) sur le device. On ne réinvente pas la roue à
chaque backend : on appelle `CreateShaderFromSource` et le mapping API→cible se fait tout seul.

Le mapping par défaut, `ApiToTarget`, encode le **chemin recommandé** pour chaque backend :
OpenGL→GLSL texte, Vulkan→**SPIR-V binaire**, DX11→HLSL SM5 (fxc), DX12→HLSL SM6 (dxc), Metal→MSL,
et software→C++ (`NK_CPLUSPLUS`). Deux variantes existent pour forcer une forme particulière :
`ApiToGLSLTarget` donne du **GLSL texte** même sous Vulkan (`NK_GLSL_VULKAN`, set/binding Vulkan) —
utile pour déboguer sans passer par SPIR-V —, et `ApiToHLSLTarget` choisit DX12 ou DX11. Ce sont des
fonctions `inline` pures, sans état : on peut les appeler n'importe où.

```cpp
nksl::InitCompiler();                              // une fois au démarrage
NkShaderHandle sh = nksl::CreateShaderFromSource(
    device, source, { NK_VERTEX, NK_FRAGMENT }, "phong");
// la cible (SPIR-V, HLSL, GLSL…) est choisie selon device->GetApi()
```

Ce n'est **pas** un système qui *possède* le shader : `device` est non-possédé, et le handle retourné
suit la convention de destruction de `NkIDevice` — c'est à vous de le détruire comme tout autre
ressource GPU. Et ce n'est pas non plus le transpileur de NKRenderer : la reflection ici
(`NkSLReflection`) est structurée et exploitable (layouts, bindings, types).

> **En résumé.** `NkSLIntegration` branche le **vrai** compilateur NkSL sur un `NkIDevice` :
> `ApiToTarget` choisit automatiquement la cible native (SPIR-V/HLSL/GLSL/MSL/C++),
> `CreateShaderFromSource`/`FromFile` compile et crée le shader, les variantes `*WithReflection`
> ajoutent les métadonnées. `device` non-possédé, handle à détruire soi-même.

### Reflection, validation, debug

Au-delà de la simple création, le pont expose tout l'outillage **sans toucher au GPU**.
`GetReflection` rend les métadonnées d'un stage isolé (entrées, uniforms, samplers) sans allouer la
moindre ressource ; `Validate` rend la liste des `NkSLCompileError` (vide = la source est correcte) ;
et `GetGeneratedSource` rend le **code traduit** (le GLSL, le HLSL… effectivement produit) pour
qu'on puisse le lire et comprendre ce que le compilateur a généré. Enfin, deux générateurs de
**layout** — `GenerateLayoutCPP` (du C++ déclarant la structure d'entrée) et `GenerateLayoutJSON`
(le même en JSON) — servent à câbler le vertex layout côté application ou à alimenter un outil.

> **En résumé.** Toute l'analyse est **hors GPU** : `GetReflection`/`Validate`/`GetGeneratedSource`
> n'allouent rien et servent au diagnostic ; `GenerateLayoutCPP`/`GenerateLayoutJSON` émettent le
> layout d'entrée pour le câblage applicatif. Construisez un `NkShaderDesc` complet avec
> `BuildShaderDesc` quand vous gérez vous-même la création.

---

## Faire tourner un shader sur le CPU : `NkSWShaderBridge`

Tous les backends ne sont pas du silicium GPU. Le **rasterizer software** de NKRHI a besoin de
fonctions C++ exécutables — une lambda vertex, une lambda fragment — pas d'un binaire SPIR-V.
`NkSWShaderBridge` est le pont qui **compile une source NkSL vers `NK_CPLUSPLUS`** puis **fabrique
ces lambdas**. C'est un header **entièrement inline** (toutes les fonctions sont `static`/`static
inline`, définies dans le header) : l'inclure suffit, mais il suppose un symbole `logger`
accessible dans la portée appelante (les macros `NKSW_LOG`/`NKSW_ERR` l'utilisent).

L'API d'usage tient en quelques fabriques. `NkCompileSources` est le cœur : elle compile la source
vertex (point d'entrée `vert_main`) et la source fragment (`frag_main`), compte les samplers et les
UBOs, détecte si le vertex utilise une **projection** (un UBO/push-constant), construit le layout
d'attributs et renvoie un `NkSWBridgeResult` portant les deux lambdas prêtes. Autour, des variantes
de chargement : `NkCompileFiles` (deux fichiers, via `NkFile` — jamais `fopen` CRT), `NkCompileFile`
(un fichier à deux stages), et les surcharges `NkCompile` pour des sources inline.

```cpp
auto r = swbridge::NkCompileFiles("quad.vert.sksl", "quad.frag.sksl");
if (r.success) {
    swDevice->SetVertexShader(r.vertFn);   // NkVertexShaderSoftware
    swDevice->SetPixelShader(r.fragFn);    // NkPixelShaderSoftware
}
```

Le point délicat est l'**interprétation des attributs vertex**. Le bridge ne se fie **pas** aux noms
(le NkSL compilé en C++ ne les conserve pas) : par défaut il déduit la sémantique d'un attribut à
partir de sa **location et de son type** (`AutoSemantic` — loc0 = position, loc1 VEC2 = UV, etc.).
Quand cette convention ne colle pas à votre layout, vous fournissez un `NkSWVertexMapping` qui
**surcharge** la détection location par location. Deux conventions dures à retenir : les matrices
sont **column-major**, et **l'UBO[0] du vertex est interprété comme la matrice de projection**.

> **En résumé.** `NkSWShaderBridge` compile NkSL→C++ et fabrique des lambdas
> `NkVertexShaderSoftware`/`NkPixelShaderSoftware` pour le rasterizer software. Points d'entrée
> `vert_main`/`frag_main`, header inline qui exige un `logger` en portée, attributs déduits par
> location+type (surchargeables via `NkSWVertexMapping`), matrices column-major, UBO[0] vertex =
> projection. Pour les normal-maps ou le multi-texture, écrivez un fragment custom plutôt que
> `MakeFragmentFn`.

---

## Une grille de référence : `NkGrid3D`

Tout éditeur, tout viewport de débogage a besoin d'une **grille de sol** — ce damier sur le plan XZ
qui donne l'échelle et l'horizon. `NkGrid3D` la rend via un shader plein-écran (donc « infinie »,
avec un *fade* à distance), configurable par un `NkGrid3DConfig` (unité de base, subdivisions,
distances de fondu, épaisseurs, couleurs des axes X/Z…). Elle suit scrupuleusement le pattern
**Create/Destroy** du moteur : `Init(device, renderPass)` crée pipeline/shader/descriptor/sampler,
et `Shutdown()` libère tout — l'objet **possède** ses handles GPU, donc on n'oublie jamais le
`Shutdown`.

```cpp
NkGrid3D grid;
grid.Init(device, renderPass);
grid.SetConfig(cfg);
// chaque frame, dans une passe active :
grid.Draw(cmd, view, proj, width, height);
// à la fin :
grid.Shutdown();
```

Ce n'est **pas** un objet qui possède le device ou la passe : `device` et `renderPass` sont
non-possédés. `Draw` exige un command buffer **actif** et les dimensions réelles du viewport (le
rendu s'appuie sur des push-constants invView/invProj). `IsValid()` reflète simplement l'état
initialisé.

> **En résumé.** `NkGrid3D` rend une grille de référence XZ infinie/fade par shader plein-écran,
> configurée par `NkGrid3DConfig`. Cycle de vie **Create/Destroy** strict (`Init`/`Shutdown`,
> ownership des handles GPU), `Draw` dans une passe active avec les dimensions du viewport. Device et
> renderPass non-possédés.

---

## Les types de gizmo : `NkGizmo3DTypes`

Un gizmo de manipulation (les flèches de translation, les anneaux de rotation, les cubes d'échelle
qu'on voit dans tout éditeur 3D) se décrit avant d'être implémenté. `NkGizmo3DTypes` fournit ce
**vocabulaire de données** : l'espace de manipulation (`GizmoSpace` — Local, World, Gimbal, Normal),
le mode (`GizmoMode` — Translate, Rotate, Scale), l'axe sous forme de **bitmask** (`GizmoAxis` —
X/Y/Z combinables en XY, XZ, YZ, XYZ), un `Gizmo3DConfig` complet (tailles, couleurs RGBA, options
de *snap*) et un `Gizmo3DResult` (les deltas de position/rotation/scale + l'axe actif).

Le point honnête à connaître : ces types décrivent une **intention** d'API, mais **aucune
fonction ni classe de manipulation n'existe** — le header d'implémentation `NkGizmo3D.h` est vide.
On les documente donc comme ce qu'ils sont : des structures de données, pas un widget fonctionnel.

> **En résumé.** `NkGizmo3DTypes` n'est **que des types** : enums (`GizmoSpace`/`GizmoMode`/
> `GizmoAxis` bitmask + helpers `operator|`/`HasFlag`), config (`Gizmo3DConfig`) et résultat
> (`Gizmo3DResult`). Le `.h` de manipulation est **vide** — pas de gizmo fonctionnel livré, juste le
> contrat de données.

---

## Aperçu de l'API

La liste de **tous** les éléments publics. Chacun est repris en détail dans la « Référence
complète ».

### `nkentseu::nksl` — intégration NkSL ↔ device (`NkSLIntegration.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Singleton | `GetCompiler()` | Référence au `NkSLCompiler` global (thread-safe). |
| Singleton | `InitCompiler(cacheDir = "")` | Initialise le compilateur (cache optionnel). |
| Singleton | `ShutdownCompiler()` | Libère le singleton. |
| Mapping | `ApiToTarget(api)` | API → cible **recommandée** (VK→SPIR-V, DX11→SM5, DX12→SM6, SW→C++…). |
| Mapping | `ApiToGLSLTarget(api)` | Force du **GLSL texte** (VK→`NK_GLSL_VULKAN`). |
| Mapping | `ApiToHLSLTarget(api)` | DX12→`NK_HLSL_DX12`, sinon `NK_HLSL_DX11`. |
| Création | `CreateShaderFromSource(device, source, stages, name, opts)` | Compile une source + crée le shader. |
| Création | `CreateShaderFromFile(device, path, stages, opts)` | Variante fichier `.nksl`. |
| Création + reflection | `CreateShaderWithReflection(...)` | Idem + `NkSLReflection` en sortie. |
| Création + reflection | `CreateShaderFromFileWithReflection(...)` | Variante fichier + reflection. |
| Reflection | `GetReflection(source, stage, filename)` | Reflection d'un stage **sans** créer de shader. |
| Desc | `BuildShaderDesc(api, source, stages, outDesc, name, opts)` | Remplit un `NkShaderDesc`. |
| Desc | `BuildShaderDescWithReflection(...)` | Idem + reflection en sortie. |
| Debug | `Validate(source, filename)` | Liste des `NkSLCompileError` (vide = valide). |
| Debug | `GetGeneratedSource(api, source, stage, opts)` | Code généré (GLSL/HLSL/MSL/C++). |
| Layout | `GenerateLayoutCPP(source, stage, varName, filename)` | Layout d'entrée en C++. |
| Layout | `GenerateLayoutJSON(source, stage, filename)` | Layout d'entrée en JSON. |
| Struct | `ShaderWithReflection { handle; reflection; success }` | Résultat des créations avec reflection. |

### `nkentseu::swbridge` — pont shader software (`NkSWShaderBridge.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Compilation | `NkCompileSources(vertSrc, fragSrc, vertName, fragName, mapping)` | Cœur : compile 2 stages → lambdas. |
| Compilation | `NkCompileFiles(vertPath, fragPath, mapping)` | Charge 2 fichiers puis compile. |
| Compilation | `NkCompileFile(path, mapping)` | Un fichier à 2 stages. |
| Compilation | `NkCompile(vertSrc, fragSrc, name, mapping)` | Sources inline (suffixes `.vert`/`.frag`). |
| Compilation | `NkCompile(src, name, hint)` | Surcharge source unique (`hint` ignoré). |
| IO | `LoadFile(path, out)` | Lit un fichier via `NkFile` (jamais `fopen`). |
| Résultat | `NkSWBridgeResult { success; error; vertFn; fragFn; vertRefl; fragRefl; stride; texCount; uboCount }` | Sortie de compilation. |
| Mapping | `NkSWVertexMapping` · `Set(loc, sem)` · `Get(loc)` | Surcharge sémantique par location (≤16). |
| Sémantique | `AttrSemantic { POSITION, COLOR, UV, NORMAL, TANGENT, GENERIC, AUTO }` | Rôle d'un attribut vertex. |
| Attribut | `AttrDesc { location; offset; type; bytes; semantic; genericSlot }` | Attribut résolu. |
| Fabrique | `MakeVertexFn(attrs, stride, hasProj)` | Construit la lambda vertex. |
| Fabrique | `MakeFragmentFn(texCount)` | Construit la lambda fragment (0/1/>1 textures). |
| Helper | `ByteSize(t)` | Taille octets d'un `NkSLBaseType` (UVEC4=4). |
| Helper | `AutoSemantic(loc, type, hasPos, hasCol, hasUV, hasNorm)` | Déduction sémantique location+type. |
| Helper | `BuildLayout(vertRefl, outStride, mapping)` | Trie/compacte le layout d'attributs. |
| Helper | `RdF(p, off)` · `ReadAttr(src, a, x,y,z,w)` | Lecture float / décodage attribut typé. |
| Helper | `ProjMV(m, vx,vy,vz, ox,oy,oz,ow)` | mat4 column-major × vec. |
| Helper | `SampleTexNN(tex, u, v, r,g,b,a)` | Échantillonnage nearest-neighbor (wrap). |
| Macro | `NKSW_LOG(...)` · `NKSW_ERR(...)` | Logs préfixés (exigent un `logger` en portée). |
| Alias | `NkVertexShaderSoftware` · `NkPixelShaderSoftware` | Types des lambdas (re-export). |

### `nkentseu` — outils 3D (`NkGrid3D.h`, `NkGizmo3DTypes.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Grille | `NkGrid3D()` / `~NkGrid3D()` | Construction / destruction. |
| Grille | `Init(device, renderPass)` · `Shutdown()` | Create/Destroy des ressources GPU. |
| Grille | `Draw(cmd, view, proj, width, height)` | Enregistre le draw dans le command buffer. |
| Grille | `SetConfig(cfg)` · `GetConfig()` · `IsValid()` | Config / état initialisé. |
| Grille | `NkGrid3DConfig { baseUnit; subDivisions; extent; infinite; fade…; lineWidth…; showAxes; showSolidFloor; couleurs }` | Réglages de la grille. |
| Gizmo | `GizmoSpace { Local, World, Gimbal, Normal }` | Espace de manipulation. |
| Gizmo | `GizmoMode { Translate, Rotate, Scale }` | Mode. |
| Gizmo | `GizmoAxis { None, X, Y, Z, XY, XZ, YZ, XYZ }` | Axe(s), bitmask. |
| Gizmo | `operator|(a, b)` · `HasFlag(a, f)` | Combinaison / test de bits. |
| Gizmo | `Gizmo3DConfig { mode; space; visibleAxes; tailles; snap…; couleurs }` | Réglages du gizmo (données). |
| Gizmo | `Gizmo3DResult { changed; deltaPosition; deltaRotation; deltaScale; activeAxis }` | Résultat d'une manipulation (données). |

### `nkentseu::gizmoshaders` — sources shaders des gizmos (`NkGrid3DShaders.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| OpenGL | `kVertexGL_GLSL` · `kFragmentGL_GLSL` | GLSL 4.30 (`uniform u_mvp`, `a_pos`/`a_color`). |
| Vulkan | `kVertexVK_GLSL` · `kFragmentVK_GLSL` | GLSL 4.50 (push constant `mvp`). |
| DX11 | `kVertexDX11_HLSL` · `kFragmentDX11_HLSL` | HLSL (`cbuffer MVP : register(b0)`). |
| DX12 | `kVertexDX12_HLSL` · `kFragmentDX12_HLSL` | HLSL (`ConstantBuffer<matrix> u_mvp : register(b0, space0)`). |

---

## Référence complète

Chaque élément est repris en détail, avec ses usages dans les différents domaines du moteur. Les
éléments de structure (enums, structs de config) sont décrits brièvement ; les mécanismes
(compilation, mapping, fabriques de lambdas, cycle de vie) le sont **à fond**.

### Le singleton compilateur

`GetCompiler()` rend une **référence** au `NkSLCompiler` global, initialisé de façon thread-safe (un
`std::call_once` selon l'en-tête). `InitCompiler(cacheDir)` l'amorce — le dossier de cache optionnel
sert à mémoriser les compilations entre lancements, ce qui change tout pour un **éditeur** qui
recharge des dizaines de shaders au démarrage. `ShutdownCompiler()` le libère. Dans la pratique,
appelez `InitCompiler` une fois au boot du moteur ; toutes les autres fonctions s'appuient ensuite
dessus de façon transparente.

- **Rendu** — un compilateur partagé pour tous les matériaux d'une scène, alimentant chaque backend.
- **Outils / éditeur** — le `cacheDir` évite de recompiler la bibliothèque de shaders à chaque
  ouverture du projet.

### Le mapping API → cible

C'est la clef du « un seul source, tous les backends ». `ApiToTarget` encode le chemin que le moteur
considère **optimal** pour chaque API : OpenGL reçoit du GLSL texte, Vulkan du **SPIR-V binaire**
(`NK_SPIRV`, le format que vkCreateShaderModule consomme directement), DX11 du HLSL SM5 (compilé
par fxc), DX12 du HLSL SM6 (dxc), Metal du MSL, et le backend software du **C++** (`NK_CPLUSPLUS`,
voir le bridge). Le `default` retombe sur GLSL.

Deux dérivations existent pour les cas où l'on veut **forcer une forme texte**. `ApiToGLSLTarget`
renvoie toujours du GLSL : sous Vulkan il choisit `NK_GLSL_VULKAN` (GLSL 4.50 avec la convention
set/binding Vulkan) au lieu de SPIR-V — précieux pour **lire** ce que produit le compilateur ou pour
un pipeline qui veut le texte intermédiaire. `ApiToHLSLTarget` distingue DX12 (`NK_HLSL_DX12`) du
reste (`NK_HLSL_DX11`). Toutes trois sont `inline` et **sans état** : aucun coût, appelables partout.

- **Rendu** — câbler un même matériau NkSL sur cinq backends sans `#ifdef` dans le code de scène.
- **Outils / éditeur** — un panneau « voir le code généré » qui bascule entre SPIR-V désassemblé et
  GLSL texte via `ApiToGLSLTarget`.

### Créer un shader : `CreateShaderFromSource` / `FromFile`

Le geste central. `CreateShaderFromSource(device, source, stages, name, opts)` compile la source NkSL
pour les `stages` demandés (typiquement `{ NK_VERTEX, NK_FRAGMENT }`), choisit la cible via l'API du
device, et crée le `NkShaderHandle` dessus. `CreateShaderFromFile` fait pareil depuis un fichier
`.nksl` (sans paramètre `name`, le nom dérivant du chemin). Le `device` est **non-possédé** : on ne
lui prête que le temps de la création, et le handle retourné se détruit comme toute ressource GPU
selon la convention de `NkIDevice` — ne l'oubliez pas, sous peine de fuite.

- **Rendu** — chaque passe (geometry, shadow, post-process) crée son shader au chargement de la
  scène et le réutilise frame après frame.
- **Gameplay / IA** — un effet de surbrillance d'ennemi compilé à la volée quand le mode debug
  s'active.
- **UI / 2D** — le shader du batcher 2D, compilé une fois et partagé par tous les widgets.

### Créer avec reflection : `*WithReflection` et `ShaderWithReflection`

Souvent, créer le shader ne suffit pas : il faut **savoir ce qu'il attend** (ses uniforms, ses
samplers, son vertex layout) pour câbler les bindings côté application. Les variantes
`CreateShaderWithReflection` et `CreateShaderFromFileWithReflection` renvoient un
`ShaderWithReflection` — un agrégat `{ handle, reflection, success }` qui porte à la fois la
ressource GPU et la `NkSLReflection` structurée. Le booléen `success` évite de tester un handle nul.

- **Rendu** — déterminer automatiquement quels descriptor sets allouer à partir de la reflection.
- **Outils / éditeur** — générer un panneau d'inspection des uniforms d'un matériau (sliders,
  color pickers) directement à partir de la reflection.

### Reflection seule : `GetReflection`

Quand on veut **uniquement** analyser un shader sans en créer la moindre ressource GPU,
`GetReflection(source, stage, filename)` rend la `NkSLReflection` d'un **stage unique**. C'est une
opération purement CPU, sans device. Idéale pour valider un layout, pré-calculer des tailles d'UBO,
ou alimenter un outil hors-ligne.

- **Outils / éditeur** — un validateur de bibliothèque de shaders qui parcourt tous les `.nksl` et
  vérifie la cohérence des bindings, sans contexte graphique.
- **IO / pipeline d'assets** — calculer les métadonnées d'un shader à l'import, avant tout rendu.

### Construire un `NkShaderDesc` : `BuildShaderDesc`

Quand on gère **soi-même** la création de la ressource (par exemple pour mutualiser plusieurs
shaders ou injecter des options spécifiques), `BuildShaderDesc(api, source, stages, outDesc, …)`
remplit un `NkShaderDesc` par référence et renvoie `true` en cas de succès — la cible est choisie via
l'`api` passée. La variante `BuildShaderDescWithReflection` ajoute la reflection en sortie. C'est le
niveau « bas » du pont : on récupère le descriptor, libre à nous de le passer au device au moment
voulu.

- **Rendu** — un cache de `NkShaderDesc` pré-construits, instanciés en pipelines à la demande.
- **GPU / backend** — préparer les descriptors d'un backend avant même que le device soit créé.

### Validation et debug : `Validate`, `GetGeneratedSource`

`Validate(source, filename)` compile pour vérifier et rend le vecteur des `NkSLCompileError` —
**vide signifie correct**. Aucune ressource GPU n'est touchée : c'est la brique d'un **linter** de
shaders. `GetGeneratedSource(api, source, stage, opts)` va plus loin : il rend le **code
effectivement généré** (le GLSL, le HLSL, le MSL ou le C++ produit pour cette API et ce stage) — la
fenêtre par laquelle on comprend ce que le compilateur fait réellement, indispensable pour traquer
une divergence entre backends.

- **Outils / éditeur** — un éditeur de shaders avec soulignement des erreurs en direct (via
  `Validate`) et un onglet « code généré » (via `GetGeneratedSource`).
- **Rendu** — diagnostiquer pourquoi un matériau rend différemment en DX12 et en Vulkan en comparant
  les deux sources générées.

### Génération de layout : `GenerateLayoutCPP`, `GenerateLayoutJSON`

Le **vertex layout** (la disposition des attributs en entrée du vertex shader) doit être déclaré
côté application pour câbler les buffers. Plutôt que de le retaper à la main, ces deux fonctions
l'**émettent** depuis la source : `GenerateLayoutCPP` produit du C++ déclarant une variable de
layout (`varName`), `GenerateLayoutJSON` produit la même information en JSON — pratique pour un
pipeline d'outils ou un format d'asset.

- **Outils / éditeur** — générer automatiquement le code de binding d'un mesh à partir du shader qui
  le consomme.
- **IO** — sérialiser le layout en JSON dans un manifeste d'asset.

### Le pont software : `NkCompileSources` et ses variantes

`NkCompileSources` est le **cœur** du bridge. Il compile la source vertex (point d'entrée
`vert_main`) et la source fragment (`frag_main`) en `NK_CPLUSPLUS` via
`GetCompiler().CompileWithReflection`, **compte** les samplers (`NK_SAMPLED_TEXTURE`) et les UBOs
(`NK_UNIFORM_BUFFER`, sur les deux stages), **détecte** `hasProj` (un UBO ou push-constant au
vertex), construit le layout d'attributs et fabrique les **deux lambdas**. Le tout revient dans un
`NkSWBridgeResult` (avec `success`, l'`error` concaténée, les deux fonctions, les deux reflections,
le `stride`, et les compteurs `texCount`/`uboCount`).

Autour, des entrées plus pratiques : `NkCompileFiles` charge deux fichiers (`.vert`/`.frag`) puis
délègue ; `NkCompileFile` lit un fichier contenant les deux stages (déconseillé si les locations
`in` entrent en conflit) ; et les deux surcharges `NkCompile` acceptent des sources **inline**
(la seconde, à source unique, ignore son paramètre `hint`). Le chargement passe toujours par
`LoadFile`, qui s'appuie sur `NkFile::Exists` + `NkFile::ReadAllText` — **jamais** `fopen` CRT, pour
rester multi-plateforme (Android inclus).

- **GPU / software** — faire tourner exactement le même shader NkSL sur le rasterizer CPU que sur le
  GPU, pour un fallback sans pilote ou pour un test de référence.
- **Outils / éditeur** — un mode « rendu logiciel » garanti reproductible pour comparer pixel à
  pixel avec un backend GPU.
- **IO** — charger les shaders depuis le système de fichiers virtuel du moteur, pas depuis le CRT.

### Le résultat software : `NkSWBridgeResult`

Cet agrégat porte tout ce qu'il faut câbler le rasterizer : `success` et `error` (diagnostic),
`vertFn`/`fragFn` (les lambdas exécutables), `vertRefl`/`fragRefl` (les métadonnées), `stride` (la
taille d'un vertex en octets, calculée par le layout), et `texCount`/`uboCount` (combien de textures
et d'UBOs le shader attend). On teste `success`, puis on installe `vertFn`/`fragFn` sur le device
software.

### Décrire et surcharger les attributs : `AttrSemantic`, `NkSWVertexMapping`, `AttrDesc`

Le compilé C++ ne conserve **pas** les noms d'attributs, donc le bridge raisonne en **sémantiques**.
`AttrSemantic` les énumère : `POSITION`, `COLOR`, `UV`, `NORMAL`, `TANGENT`, `GENERIC` (un attribut
quelconque, rangé dans `attrs[]`), et `AUTO` (déduction automatique, la valeur par défaut). Quand la
déduction ne correspond pas à votre layout, `NkSWVertexMapping` la **surcharge** location par
location : `Set(location, sem)` assigne (ignore au-delà de 16 locations), `Get(location)` relit
(`GENERIC` hors borne) — un mapping explicite **prime** toujours sur la déduction automatique.
`AttrDesc` est l'attribut **résolu** final (location, offset, type, taille en octets, sémantique,
et le slot dans `attrs[]` pour les `GENERIC`).

- **Rendu / software** — un mesh dont l'attribut de couleur est en location 2 plutôt que 1 : on
  corrige avec `mapping.Set(2, COLOR)` au lieu de laisser la déduction se tromper.
- **Animation** — exposer des poids de skinning comme `GENERIC` et les lire dans `out.attrs[]`.

### La déduction automatique : `AutoSemantic`

Sans nom ni mapping, `AutoSemantic` devine le rôle d'un attribut à partir de sa **location** et de
son **type**, en tenant compte de ce qui a déjà été vu : loc0 est toujours POSITION ; loc1 vaut UV
si c'est un VEC2, sinon COLOR (VEC3/VEC4/UVEC4) ; loc2 prend UV, COLOR ou NORMAL selon ce qui manque
encore ; loc3 complète par NORMAL ou UV ; au-delà, c'est GENERIC. C'est une heuristique pragmatique
qui couvre les layouts classiques (position+couleur+UV+normale) sans configuration.

### Calculer le layout : `BuildLayout` et `ByteSize`

`BuildLayout(vertRefl, outStride, mapping)` prend la reflection du vertex, **trie** ses
`vertexInputs` par location (un tri à bulles, suffisant vu le faible nombre d'attributs), calcule
des **offsets et un stride compacts**, et résout chaque sémantique (le `mapping` explicite primant
sur `AutoSemantic`). Il s'appuie sur `ByteSize`, qui rend la taille en octets d'un `NkSLBaseType` —
avec un détail à connaître : **UVEC4 ne fait que 4 octets** (il représente une couleur RGBA8 packée,
pas quatre entiers 32 bits).

### Lire et transformer : `RdF`, `ReadAttr`, `ProjMV`, `SampleTexNN`

Ces helpers sont la **plomberie** que les lambdas exécutent. `RdF(p, off)` lit un float via `memcpy`
(pas de déréférencement non aligné). `ReadAttr(src, a, x,y,z,w)` décode un attribut **typé** en
quadruplet flottant — un UVEC4 est lu comme 4 octets normalisés `/255`, les IVEC* sont convertis en
float. `ProjMV(m, …)` applique une mat4 **column-major** à `(x,y,z,1)` — c'est la convention de tout
le software. `SampleTexNN(tex, u, v, …)` échantillonne une texture en **nearest-neighbor** avec wrap
(`u - floor(u)`) sur le mip 0, gère les bpp ≥4/==3/1, et rend du blanc si la texture est nulle ou
vide.

- **Rendu / software** — c'est littéralement ce qui s'exécute par fragment dans le rasterizer CPU.

### Fabriquer les lambdas : `MakeVertexFn`, `MakeFragmentFn`

`MakeVertexFn(attrs, stride, hasProj)` construit la lambda vertex (qui capture le layout par copie) :
elle lit chaque attribut, remplit `out.uv`/`out.color`/`out.normal`/`out.position`, et range les
`GENERIC` dans `out.attrs[]` (16 floats max). Si `hasProj` et qu'un buffer uniform est présent,
**l'UBO[0] est interprété comme la matrice de projection** column-major. `MakeFragmentFn(texCount)`
distingue trois cas : `0` texture = passthrough de la couleur ; `1` = échantillonne `tex[0]`
multiplié par la couleur (accepte un `NkSWTextureBatch*` ou un `NkSWTexture*` direct en repli) ;
`>1` = échantillonne seulement `tex[0]` (diffuse) × couleur — les shaders plus complexes
(normal-mapping, multi-texture) ne sont **pas** gérés et demandent un fragment custom.

### Macros et alias

`NKSW_LOG`/`NKSW_ERR` préfixent les messages (`[NkSWBridge] …`) et appellent `logger.Infof`/`Errorf`
— elles **supposent une variable `logger` dans la portée appelante**, conséquence directe du fait
que le header est inline. Les alias `NkVertexShaderSoftware` et `NkPixelShaderSoftware` ne sont que
des re-exports des types globaux du même nom, par commodité dans le namespace `swbridge`.

### La grille : `NkGrid3D` et `NkGrid3DConfig`

`NkGrid3D` rend une grille de sol (plan XZ) « infinie » avec fondu à distance, via un shader
plein-écran. Son cycle de vie suit le pattern **Create/Destroy** strict du moteur : `Init(device,
renderPass)` crée pipeline, shader (vert+frag combinés), descriptor set et sampler, puis
`Shutdown()` libère le tout — l'objet **possède** ses handles GPU, donc le `Shutdown` n'est jamais
facultatif (sinon fuite GPU). `Draw(cmd, view, proj, width, height)` enregistre le draw dans un
command buffer **actif** et a besoin des dimensions réelles du viewport (le shader s'appuie sur des
push-constants invView/invProj). `SetConfig`/`GetConfig` (inline) gèrent la configuration, `IsValid`
reflète l'état initialisé.

Le `NkGrid3DConfig` règle tout : `baseUnit`/`subDivisions`/`extent` (l'échelle), `infinite` +
`fadeStart`/`fadeEnd` (le fondu lointain), `lineWidthMajor`/`lineWidthMinor` (les épaisseurs),
`showAxes`/`showSolidFloor` (les options), et les **couleurs** `NkColor` (RGBA 0-255) des lignes
majeures/mineures, des axes X (rouge) et Z (bleu), et du sol plein. `device` et `renderPass` restent
**non-possédés**.

- **Outils / éditeur** — la grille de référence d'un viewport, avec axes colorés pour l'orientation.
- **Rendu / debug** — un repère de sol pour situer une scène en cours de mise au point.

### Les types de gizmo : enums, helpers, structs

`GizmoSpace` décrit l'**espace** de manipulation : `Local` (repère de l'objet), `World` (repère
monde), `Gimbal` (axes d'Euler, pour la rotation) et `Normal` (perpendiculaire à la face, pour une
translation contrainte). `GizmoMode` choisit l'opération : `Translate`, `Rotate`, `Scale`.
`GizmoAxis` est un **bitmask** : `None`, les axes simples `X`/`Y`/`Z`, leurs plans `XY`/`XZ`/`YZ`, et
`XYZ` — combinables via `operator|` et testables via `HasFlag`.

`Gizmo3DConfig` rassemble les réglages (données) : `mode`, `space`, `visibleAxes`, les dimensions
visuelles (`axisLength`, `arrowHeadSize`, `lineWidth`, `planeOpacity`, `rotationCircleRadius`,
`scaleHandleOffset`), les bascules (`showCenter`, `showAxisLabels`, `snapEnabled`), les pas de snap
(`snapTranslate`, `snapRotate` en degrés, `snapScale`) et les **couleurs** `NkColor` des axes, de la
sélection, des plans et du centre. `Gizmo3DResult` porte la sortie d'une manipulation :
`changed`, les deltas `deltaPosition`/`deltaRotation` (degrés Euler)/`deltaScale`, et l'`activeAxis`.

Le rappel essentiel : ce ne sont **que des données**. Le header d'implémentation `NkGizmo3D.h` est
vide — il n'existe aucune fonction ni classe qui *exécute* une manipulation gizmo. Ces types
documentent un contrat, pas un widget livré.

- **Outils / éditeur** — la spécification de données vers laquelle pointe une future implémentation
  de gizmo de transformation.

### Les sources shaders des gizmos : `gizmoshaders`

Malgré son chemin `Grid3DShaders.h`, ce header déclare `nkentseu::gizmoshaders` (commentaire interne
« NkGizmoShaders.h ») : ce sont les sources shaders des **gizmos** (lignes épaisses, cercles), pas
de la grille. Que des `static const char*` : un couple GLSL OpenGL 4.30 (`u_mvp`, `a_pos`/`a_color`),
un couple GLSL Vulkan 4.50 (push constant `mvp`), un couple HLSL DX11 (`cbuffer MVP : register(b0)`,
sémantiques `POSITION`/`COLOR`) et un couple HLSL DX12 (`ConstantBuffer<matrix> u_mvp : register(b0,
space0)`). Le principe est identique partout : un vertex `vec3 pos` + `vec3 color` projeté par la
MVP, la couleur interpolée vers `fragColor`/`SV_TARGET`. Ce sont des sources **par-API** : leur
valeur est documentaire (l'interface publique se limite aux symboles `static const char*`).

---

### Exemple

```cpp
#include "NKRHI/SL/NkSLIntegration.h"
#include "NKRHI/SL/NkSWShaderBridge.h"
#include "NKRHI/Tools/Grid3D/NkGrid3D.h"
using namespace nkentseu;

// 1) Compiler un shader NkSL pour le backend courant (cible auto selon l'API).
nksl::InitCompiler();
auto sr = nksl::CreateShaderWithReflection(
    device, source, { NK_VERTEX, NK_FRAGMENT }, "phong");
if (sr.success) {
    // sr.handle est prêt ; sr.reflection donne uniforms/samplers pour le câblage.
}

// 2) Le même shader, mais exécuté sur le rasterizer software (lambdas C++).
auto sw = swbridge::NkCompileFiles("phong.vert.sksl", "phong.frag.sksl");
if (sw.success) {
    swDevice->SetVertexShader(sw.vertFn);   // UBO[0] = projection (column-major)
    swDevice->SetPixelShader(sw.fragFn);    // texCount textures attendues
}

// 3) Une grille de sol dans le viewport (Create/Destroy).
NkGrid3D grid;
grid.Init(device, renderPass);
grid.Draw(cmd, view, proj, width, height);  // chaque frame, passe active
grid.Shutdown();                            // ne jamais l'oublier
```

---

[← Index NKRHI](README.md) · [Récap NKRHI](../NKRHI.md) · [Couche Runtime](../README.md)
