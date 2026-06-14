# Le compilateur NkSL : compilation, génération de code et conversion

> Couche **Runtime** · NKSL · Le **vrai** compilateur de shaders NkSL — le pipeline qui prend une
> source, choisit une cible (GLSL, Vulkan, HLSL DX11/DX12, MSL, C++), et en sort du code natif, du
> SPIR-V ou de la réflexion. Distinct du transpileur ad-hoc que NKRenderer embarque pour ses
> matériaux.

Un moteur qui veut rendre la **même scène** sur OpenGL, Vulkan, Direct3D 11, Direct3D 12, Metal *et*
un rasterizer logiciel ne peut pas écrire six fois chaque shader. La seule solution viable est
d'écrire le shader **une fois**, dans un langage source unique, puis de le **traduire** vers chaque
API au moment du build ou au démarrage. C'est exactement le travail de NKSL : un *front-end* lit la
source NkSL, en construit un arbre syntaxique (AST), une passe sémantique le valide et en extrait la
**réflexion** (la liste des bindings, leurs tailles, leurs types), et une famille de **backends de
génération de code** émet le dialecte de chaque cible. Cette page couvre la moitié *aval* de ce
pipeline : la façade `NkSLCompiler`, les backends `NkSLCodeGen*`, le module de conversion
`NkShaderConverter`, et les annotations sémantiques pour l'éditeur de matériaux.

Une règle d'architecture domine tout le reste, et il faut la garder en tête du début à la fin :
**NKSL ne connaît pas NKRHI**. C'est une inversion de dépendance volontaire. Le compilateur ne
produit jamais de `NkShaderDesc` (le type de description de shader de NKRHI) ; il ne sort que des
formes *natives* — du texte source, du bytecode SPIR-V, de la réflexion. Le pont vers NKRHI vit de
l'autre côté, dans `NkSLIntegration`. Si vous cherchez les anciens `FillShaderDesc*`, ils ont
déménagé là-bas.

- **Namespace** : `nkentseu` (tous les types et fonctions de cette page)
- **Module** : `Kernel/Runtime/NKSL`
- **Headers réels** :
  - `#include "NKSL/Compiler/NkSLCompiler.h"`
  - `#include "NKSL/Compiler/NkGLSLCompiler.h"`
  - `#include "NKSL/CodeGen/NkSLCodeGen.h"`
  - `#include "NKSL/ShaderConvert/NkShaderConvert.h"`
  - `#include "NKSL/ShaderConvert/NkShaderAnnotations.h"`

---

## Compiler une source : la façade `NkSLCompiler`

Le point d'entrée normal n'est pas un backend, c'est la **façade** `NkSLCompiler`. On lui donne une
source, un *stage* (vertex, fragment, compute…) et une *cible*, et elle orchestre tout : front-end,
sémantique, choix du bon backend, mise en cache. Elle possède son propre cache mémoire interne, et
l'expose si on veut le manipuler.

```cpp
NkSLCompiler compiler;                                  // cache activé par défaut
NkSLCompileResult vs = compiler.Compile(source,
                                        NkSLStage::NK_VERTEX,
                                        NkSLTarget::NK_GLSL);
if (vs.success) {
    // vs contient le GLSL OpenGL généré
}
```

`Compile` est la voie simple : une source, une cible, un résultat. Mais le besoin réel est souvent
plus riche. `CompileWithReflection` fait la **même chose en une passe** et renvoie en plus la
réflexion (les bindings du shader) — précieux quand on veut générer un layout de descripteurs juste
après. Quand on n'a besoin **que** de la réflexion (pour bâtir un layout sans encore produire de
code), `Reflect` fait l'analyse seule, sans génération.

Et puisqu'on cible plusieurs APIs, `CompileAllTargets` compile une source vers **toute une liste de
cibles** en partageant une seule passe de réflexion — c'est l'appel naturel d'un pipeline de build
qui veut produire GLSL + Vulkan + HLSL d'un coup. Son résultat imbriqué `MultiTargetResult` regroupe
les `results` (un par cible) et la `reflection` commune ; son `allSucceeded()` renvoie `false` dès
qu'**une** cible a échoué.

```cpp
NkVector<NkSLTarget> cibles{ NkSLTarget::NK_GLSL,
                             NkSLTarget::NK_GLSL_VULKAN,
                             NkSLTarget::NK_HLSL_DX11 };
auto multi = compiler.CompileAllTargets(source, NkSLStage::NK_FRAGMENT, cibles);
if (multi.allSucceeded()) { /* multi.results[0..2], multi.reflection partagée */ }
```

Ce n'est **pas** une simple boucle d'appels `Compile` : la réflexion n'est calculée qu'une fois et
partagée, ce qui garantit que toutes les cibles voient exactement le même layout.

> **En résumé.** `NkSLCompiler` est la façade : `Compile` (simple), `CompileWithReflection` (code +
> bindings en une passe), `Reflect` (bindings seuls), `CompileAllTargets` (plusieurs cibles, une
> réflexion partagée, `allSucceeded()`). Elle gère le cache et choisit le backend pour vous.

### Les chemins spécialisés

Au-delà de la voie générique, la façade expose des chemins ciblés. `CompileToSPIRV` prend du **GLSL
déjà écrit** (pas du NkSL) et le passe par la chaîne glslang/shaderc pour produire du SPIR-V.
`CompileToMSL_SpirvCross` produit du Metal Shading Language via SPIRV-Cross — la route alternative,
plus robuste, vers Metal. `CompileWithSemantic` est `Compile` précédé d'une **analyse sémantique
explicite** quand on veut forcer la validation complète avant la génération.

Deux outils sans génération de code complètent la panoplie. `Validate` parcourt la source et renvoie
la liste des `NkSLCompileError` **sans rien produire** — l'appel d'un linter ou d'un éditeur qui veut
souligner les fautes en direct. `Preprocess` exécute le préprocesseur (gardes d'inclusion via
`#pragma once`) et renvoie la source dépliée ; ses deux pointeurs optionnels remontent les erreurs et
la **liste des fichiers inclus** — exactement ce qu'il faut pour qu'un *watcher* de hot-reload sache
quels fichiers surveiller.

> **En résumé.** Chemins ciblés : `CompileToSPIRV` (GLSL→SPIR-V), `CompileToMSL_SpirvCross`
> (Metal via SPIRV-Cross), `CompileWithSemantic` (avec analyse forcée). Sans codegen : `Validate`
> (juste les erreurs, pour un linter) et `Preprocess` (déplie + remonte les fichiers inclus).

### Le cache mémoire : `NkSLCache`

Compiler coûte cher ; recompiler une source inchangée est du gaspillage. `NkSLCache` est un cache
**en mémoire**, indexé en `O(1)` par une `NkUnorderedMap`, et **thread-safe** (un mutex interne
protège toutes ses méthodes publiques). Chaque `NkSLCacheEntry` retient le hash de la source, sa
cible, son stage, le bytecode produit, l'horodatage et un drapeau `isSpirv`.

Le piège à connaître tient à la **clé**. La map n'est pas indexée par le seul hash de la source mais
par une clé `uint64` *fabriquée* à partir du triplet (hash, cible, stage). Conséquence directe et
voulue : **une même source compilée vers deux cibles, ou pour deux stages, produit deux entrées
distinctes**. C'est ce qui permet de cacher GLSL et Vulkan côte à côte sans collision.

```cpp
NkSLCache& cache = compiler.GetCache();   // le cache interne de la façade
NkSLCacheEntry e;
if (cache.Has(hash, NkSLTarget::NK_GLSL, NkSLStage::NK_VERTEX)) {
    cache.Get(hash, NkSLTarget::NK_GLSL, NkSLStage::NK_VERTEX, e);  // e rempli
}
```

`Put` insère depuis un `NkSLCompileResult`, `Has`/`Get` interrogent, `Clear` vide, `Size` compte.
`Flush` et `Load` assurent la **persistance disque** (écrire / relire le cache entre deux sessions),
et `SetDirectory` choisit où. On l'active ou le désactive par `EnableCache`.

> **En résumé.** `NkSLCache` = cache mémoire `O(1)`, thread-safe. La clé combine (hash, cible,
> stage) → une source produit une entrée **par cible et par stage**. `Flush`/`Load` persistent sur
> disque. Ne le confondez pas avec `NkShaderCache` (disque, côté conversion — vu plus bas).

### La bibliothèque hot-reload : `NkSLShaderLibrary`

Au-dessus du compilateur, `NkSLShaderLibrary` gère une **collection nommée** de shaders avec
**rechargement à chaud**. On enregistre des shaders (depuis un fichier avec `Register`, ou depuis une
source inline avec `RegisterInline`), on les compile tous pour une cible avec `CompileAll`, et on les
récupère par nom avec `Get` / `GetReflection`. Le cœur de la fonctionnalité est `HotReload` : il
recompile **uniquement** les entrées marquées `dirty` (modifiées) et renvoie le nombre rechargé —
idéal dans une boucle d'éditeur qui surveille les fichiers et veut voir l'effet d'une retouche sans
relancer l'application. La library est thread-safe (mutex interne protégeant ses entrées).

Deux pièges d'**ownership** sont à respecter scrupuleusement. D'abord, la library **n'est pas
propriétaire** du compilateur : son constructeur prend un `NkSLCompiler*` *emprunté*, et ne le
détruit jamais — c'est à vous de garantir que le compilateur survit à la library. Ensuite, les
pointeurs renvoyés par `Get` et `GetReflection` pointent **à l'intérieur** des entrées de la
library : leur durée de vie est liée à elle, et un `CompileAll` ou un `HotReload` peut les
**invalider**. Ne les conservez pas à travers un rechargement.

```cpp
NkSLShaderLibrary lib(&compiler, "Assets/Shaders");
lib.Register("phong", "phong.nksl", { NkSLStage::NK_VERTEX, NkSLStage::NK_FRAGMENT });
lib.CompileAll(NkSLTarget::NK_GLSL);
// ... plus tard, dans la boucle d'édition :
uint32 rechargés = lib.HotReload(NkSLTarget::NK_GLSL);   // ne recompile que le modifié
```

> **En résumé.** `NkSLShaderLibrary` = collection nommée + hot-reload (`HotReload` ne recompile que
> le `dirty`). Elle **emprunte** son `NkSLCompiler*` (ne le détruit pas). Les pointeurs `Get` /
> `GetReflection` vivent dans la library et sont **invalidés** par `CompileAll` / `HotReload`.

---

## Compiler du GLSL en SPIR-V : `NkGLSLCompiler.h`

Quand la source est déjà du **GLSL pur** (sans annotation NkSL) et qu'on veut du SPIR-V, le chemin le
plus direct est la fonction libre `NkGLSLToSPIRV`, adossée à **libglslang in-tree** (le module
NKGLSlang). Elle ne dépend **que** de glslang — **pas de Vulkan** — et renvoie un
`NkGLSLCompileResult` : un drapeau `success`, le tableau `spirv` de mots `uint32` (vide en cas
d'échec), et un `errorLog`.

```cpp
NkGLSLCompilerInit();                                   // idempotent
auto r = NkGLSLToSPIRV(NkSLStage::NK_FRAGMENT, glslSrc, "main");
if (r.success) { /* r.spirv = mots SPIR-V */ }
NkGLSLCompilerShutdown();
```

Deux précautions. D'abord, la déclaration de ces fonctions est **toujours visible**, mais
l'implémentation réelle n'existe que si `NK_RHI_GLSLANG_ENABLED` est défini ; sinon le `.cpp` fournit
un **stub** — donc on teste toujours `success`. Ensuite, `errorLog` pointe vers un **buffer statique
interne** : ne le libérez pas, et sachez qu'il n'est pas réentrant entre appels concurrents — copiez
le message si vous en avez besoin au-delà de l'appel. `NkGLSLCompilerInit` initialise glslang (c'est
idempotent, thread-safe via `std::call_once` côté impl) et `NkGLSLCompilerShutdown` le libère.

> **En résumé.** `NkGLSLToSPIRV` compile du **GLSL sans annotation** en SPIR-V via glslang
> (sans Vulkan). Implémentation réelle seulement sous `NK_RHI_GLSLANG_ENABLED` (stub sinon → tester
> `success`). `errorLog` = buffer statique interne, non réentrant : à copier, jamais à libérer.

---

## Générer du code depuis l'AST : `NkSLCodeGen.h`

C'est ici que NkSL devient *réellement* multi-cible. Tous les backends dérivent de
`NkSLCodeGenBase` et partagent un unique point d'entrée polymorphe : `Generate(ast, stage, opts)`. On
ne les appelle presque jamais à la main — la façade s'en charge — mais comprendre leur découpage est
indispensable pour savoir *ce que* chaque cible attend et pourquoi un même shader rend différemment
ailleurs.

### Le contrat de base : `NkSLCodeGenBase`

La base abstraite fixe la cible (`GetTarget()`) et déclare `Generate` *pur*. Elle fournit aux
sous-classes une boîte à outils d'**émission** : indentation (`IndentPush`/`IndentPop`/`Indent`),
écriture (`Emit`/`EmitLine`/`EmitNewLine`), traduction de types (`TypeToString`/`BaseTypeToString`/
`StorageToString`) et report d'erreurs (`AddError`/`AddWarning`). Un détail compte pour le compute :
chaque backend renseigne `mLocalSizeX/Y/Z` (défaut 1,1,1) depuis le `NkSLProgramNode`, pour émettre
le bon `layout(local_size_*)` en GLSL/Vulkan ou `[numthreads(...)]` en HLSL.

### Les backends concrets

Chaque backend est un *dialecte* avec ses contraintes propres :

- **`NkSLCodeGenGLSL`** — GLSL **OpenGL 4.30+**, bindings *aplatis* (pas de `set=`). Sa stratégie
  notable : il **auto-assigne** les `location` d'entrée/sortie et les bindings d'UBO quand la source
  les omet (compteurs séparés). C'est **indispensable en GL** — sans ces locations, l'ordre des
  attributs n'est pas garanti, ce qui donne typiquement un écran noir. Il émule aussi les
  *push constants* (absents en GL) via un tableau `uniform vec4 _PushConstants[N]` en réécrivant les
  accès `inst.membre` → `_PushConstants[...]`, et détecte le besoin de **Y-flip** (sans flipper un VS
  depth-only ni un shader 2D purement push-constant).
- **`NkSLCodeGenGLSLVulkan`** — GLSL **4.50 Vulkan**. Ici `layout(set=N, binding=M)` est
  **obligatoire**, avec `#extension GL_KHR_vulkan_glsl`, `push_constant`, `subpassInput`/
  `subpassLoad`, `gl_VertexIndex`/`gl_InstanceIndex` et — si `opts.vkDrawParams` — `gl_BaseVertex`/
  `gl_BaseInstance`. Vulkan **exige** des `location` explicites, d'où la même auto-assignation
  (location, set, binding) que pour GL.
- **`NkSLCodeGenHLSL_DX11`** — HLSL **SM5 / DX11 / fxc**, registres `b`/`t`/`s`/`u`. Son compteur de
  binding `mReg` est **partagé** entre les classes de registre, et doit matcher le compteur partagé
  côté device/démo (par convention : ubo→0, shadow→1, albedo→2). L'alias
  `NkSLCodeGenHLSL = NkSLCodeGenHLSL_DX11` existe pour la **rétrocompatibilité**.
- **`NkSLCodeGenHLSL_DX12`** — HLSL **SM6+ / DX12 / dxc**, avec `register(bN, spaceM)`, RootSignature
  inline (`opts.dx12InlineRootSignature`), *wave intrinsics* SM6.0 et **bindless** SM6.6
  (`opts.dx12BindlessHeap` → `ResourceDescriptorHeap[idx]`). Même règle de compteur `mReg` partagé
  que DX11.
- **`NkSLCodeGen_MSL`** — **Metal Shading Language natif** depuis l'AST.
- **`NkSLCodeGenMSLSpirvCross`** — MSL **via SPIRV-Cross**, le chemin alternatif plus robuste. Il
  expose en plus `GenerateFromSPIRV(spirvWords, stage, opts)` pour partir directement de SPIR-V.
- **`NkSLCodeGenCPP`** — génère du **C++** pour le rasterizer logiciel (cible `NK_CPLUSPLUS`).

Ce partage de compteur de registres entre backends HLSL n'est **pas** un détail cosmétique : c'est ce
qui permet à un binding de descripteur de mapper *directement* son slot de registre. Repartir de
compteurs séparés par classe (`b`/`t`/`s`/`u`) casserait l'alignement avec le device.

> **En résumé.** Tous les backends dérivent de `NkSLCodeGenBase` et implémentent `Generate`. GLSL =
> bindings aplatis + auto-location (sinon écran noir) ; Vulkan = `set/binding` obligatoires ; HLSL
> DX11/DX12 = compteur `mReg` partagé à aligner avec le device ; deux routes MSL (natif / SPIRV-Cross)
> ; un backend C++ pour le software.

### Les utilitaires de type et la réflexion

À côté des backends, des fonctions libres traduisent les types NkSL. `NkSLBaseTypeName`/
`NkSLTypeName` donnent le nom d'un `NkSLBaseType`. `NkSLBaseTypeSize` donne sa taille **en octets,
suivant std140 pour les UBO** — une subtilité capitale : `NK_MAT3` vaut **48** (trois `vec4`, pas
36 !), `NK_MAT4` 64, `NK_VEC3` 12, `NK_DVEC4` 32. C'est *cette* fonction qu'on utilise pour calculer
la taille d'un UBO sans se tromper sur le rembourrage std140. `NkSLBaseTypeComponents` donne le nombre
de composantes (mat3→9, vec4→4, scalaire→1). Enfin, deux mappeurs convertissent un format d'image NkSL
(`"r32f"`, `"rgba8i"`…) vers le type d'élément attendu par `RWTexture2D` en HLSL
(`NkSLImageFormatToHLSLElem`) ou par `texture2d<T,...>` en MSL (`NkSLImageFormatToMSLElem`).

`NkSLReflector` extrait automatiquement les bindings depuis l'AST : `Reflect(ast, stage)` produit une
`NkSLReflection`, et deux générateurs en font une **sortie outillable** —
`GenerateLayoutJSON` sérialise le layout en JSON (pour un éditeur, un outil d'inspection), et
`GenerateLayoutCPP` émet une **déclaration C++** du layout (pour figer un layout dans le code généré).

> **En résumé.** `NkSLBaseTypeSize` suit **std140** (mat3 = 48 octets !) — c'est l'outil du calcul de
> taille d'UBO. `NkSLReflector` extrait les bindings et sait les sérialiser en JSON ou en C++
> (`GenerateLayoutJSON`/`GenerateLayoutCPP`).

---

## Convertir et résoudre des fichiers : `NkShaderConvert.h`

Le module de conversion répond à un autre besoin : prendre un shader **déjà sous une forme** (fichier
GLSL, SPIR-V, HLSL, MSL) et le **transformer** ou le **résoudre par convention de nommage**. La
résolution de fichiers est **toujours** disponible ; les conversions, elles, sont **conditionnelles**
— `NK_RHI_GLSLANG_ENABLED` débloque GLSL→SPIR-V, `NK_RHI_SPIRVCROSS_ENABLED` débloque
SPIR-V→GLSL/HLSL/MSL.

La convention d'extensions structure tout : `shader.<stage>.<format>`, avec les stages `vert`/`frag`/
`comp` et les formats `glsl`/`spirv`/`spv`/`hlsl`/`msl`. `NkShaderFileResolver` (entièrement statique)
décortique ces noms : `BasePath` retire la dernière extension de format, `FormatExt` et `StageExt`
extraient les deux dernières extensions, `StageFrom` déduit le `NkSLStage`, `ResolveVariant` change le
format en gardant le stage (`shader.vert.glsl` → `shader.vert.spirv`), et `FindVariants` liste les
variantes **réellement présentes sur disque**.

```cpp
NkSLStage s = NkShaderFileResolver::StageFrom("phong.frag.glsl");   // NK_FRAGMENT
NkString spv = NkShaderFileResolver::ResolveVariant("phong.frag.glsl", "spirv");
// → "phong.frag.spirv"
```

> **En résumé.** Convention `shader.<stage>.<format>`. `NkShaderFileResolver` (statique) déduit
> stage/format d'un nom et trouve les variantes sur disque. La **résolution est toujours dispo** ; les
> **conversions** dépendent de `NK_RHI_GLSLANG_ENABLED` / `NK_RHI_SPIRVCROSS_ENABLED`.

### Le convertisseur : `NkShaderConverter`

`NkShaderConverter` (statique) est le moteur de conversion. **Avant tout appel**, on interroge ses
capacités : `CanGlslToSpirv`, `CanSpirvToGlsl`, `CanSpirvToHlsl`, `CanSpirvToMsl` reflètent les flags
de compilation — si la capacité manque, la conversion est un stub. Toujours vérifier `CanXxx()` (ou,
à défaut, le `success` du résultat).

Le résultat universel est `NkShaderConvertResult` : `success`, la `source` (texte GLSL/HLSL/MSL), le
`binary` (mots SPIR-V *empaquetés en octets*), les `errors`, et — pour la route HLSL — un vecteur
`dxBindings` de remaps. Comme le SPIR-V est stocké en octets, trois accesseurs aident à le relire :
`SpirvWords()` réinterprète `binary` en `uint32*` (ou `nullptr` si vide), `SpirvWordCount()` donne le
nombre de mots, et `GetSpirvWordsCopy()` rend une **copie alignée** — à préférer si l'alignement de
`binary` n'est pas garanti.

```cpp
if (NkShaderConverter::CanGlslToSpirv()) {
    auto r = NkShaderConverter::GlslToSpirv(glslSrc, NkSLStage::NK_VERTEX, "phong");
    if (r.success && NkShaderConverter::CanSpirvToHlsl()) {
        auto h = NkShaderConverter::SpirvToHlsl(r, NkSLStage::NK_VERTEX, 50);  // surcharge helper
        // h.source = HLSL ; h.dxBindings = remap compact des registres
    }
}
```

Les conversions se déclinent en plusieurs familles. **GLSL→SPIR-V** : `GlslToSpirv` (la source est du
GLSL 4.30+ **sans annotation NkSL**). **SPIR-V→texte** : `SpirvToGlsl`, `SpirvToHlsl` (avec un *shader
model* HLSL, 50 par défaut), `SpirvToMsl`, chacun ayant une surcharge *helper* qui prend directement
un `NkShaderConvertResult` (elle en extrait les mots toute seule). **Chargement** : `LoadFile`
auto-détecte le format par l'extension (`.spirv`/`.spv` → binaire, le reste → source), tandis que
`LoadAsSpirv` charge directement un `.spirv`/`.spv`, compile un `.glsl` si possible, et **refuse** un
`.hlsl`/`.msl` (ce module ne compile pas vers SPIR-V depuis ces formats). **Raccourcis fichier→cible**
enfin : `GlslFileToHlsl`/`GlslFileToMsl`/`GlslFileToGlsl` enchaînent `LoadFile` + `GlslToSpirv` +
`SpirvToXxx`, et les variantes *texte* `GlslToHlsl`/`GlslToMsl`/`GlslToGlsl` prennent une source GLSL
**Vulkan-style** (la forme canonique : `layout(set=,binding=)`, `push_constant`…) et la traduisent en
chaînant glslang puis SPIRV-Cross.

Le `dxBindings` mérite un mot. Chaque `NkDXResourceBinding` *remappe* un binding Vulkan `(set,
binding)` vers des registres DX **compacts** (`cbvReg` pour `register(bN)`, `srvReg` pour `register(tN)`,
`samplerReg` pour `register(sN)`, `uavReg` pour `register(uN)`, plus un `space`, 0 en DX11). La valeur
`0xFFFFFFFFu` (`~0u`) signale qu'une classe de registre n'est **pas utilisée**. C'est ce remap, produit
par `SpirvToHlsl`, qui permet de brancher correctement les ressources côté D3D.

> **En résumé.** `NkShaderConverter` (statique) : **vérifier `CanXxx()` avant tout**. Conversions
> GLSL↔SPIR-V↔HLSL/MSL, chargements (`LoadFile`/`LoadAsSpirv`), raccourcis fichier→cible. Le SPIR-V est
> en octets → relire via `SpirvWords()`/`GetSpirvWordsCopy()`. `dxBindings` = remap compact (set,binding)
> → registres DX (`0xFFFFFFFFu` = inutilisé).

### Le cache disque : `NkShaderCache`

Distinct du cache mémoire du compilateur, `NkShaderCache` met en cache les **conversions** sur
**disque**. Sa clé est un FNV-1a 64-bit calculé sur (source + stage + format-cible), et chaque entrée
est un fichier `.nksc` au format simple `[magic 'NKSC'][clé 8o][taille 4o][octets]`. Toutes ses
méthodes sont `noexcept`.

On choisit le répertoire (`SetCacheDir`, créé si absent), on calcule une clé statiquement
(`ComputeKey`), on `Load` (résultat à `success == false` si absent) et on `Save` (remplace si présent).
`Invalidate` retire une entrée, `Clear` supprime tous les `.nksc`. Trois niveaux de **ramasse-miettes**
existent, du plus sûr au plus agressif : `PurgeOlderThan(maxAgeSeconds)` (par date de modification,
conservateur — usage CI), `PurgeUnused(livingKeys)` (supprime ce qui n'est pas dans la liste vivante),
et `PurgeUnusedThisSession()` (supprime ce qui n'a été ni chargé ni écrit pendant la session — **très
agressif, à ne pas appeler en développement partiel** sous peine de vider des entrées encore utiles).
Un singleton optionnel `Global()` est disponible.

```cpp
uint64 key = NkShaderCache::ComputeKey(source, NkSLStage::NK_FRAGMENT, "spirv");
NkShaderConvertResult cached = NkShaderCache::Global().Load(key);
if (!cached.success) {
    cached = NkShaderConverter::GlslToSpirv(source, NkSLStage::NK_FRAGMENT);
    NkShaderCache::Global().Save(key, cached);
}
```

> **En résumé.** `NkShaderCache` = cache **disque** `.nksc` des conversions (FNV-1a, `noexcept`).
> GC à trois niveaux : `PurgeOlderThan` (sûr, CI), `PurgeUnused` (liste vivante), `PurgeUnusedThisSession`
> (très agressif). C'est le **jumeau disque** de `NkSLCache` (mémoire) — deux caches distincts.

---

## Annoter pour l'éditeur : `NkShaderAnnotations.h`

Le dernier header répond à un besoin d'**outillage**, pas de runtime. Un éditeur de matériaux veut
connaître, pour chaque paramètre d'un shader, son libellé, sa plage, sa couleur par défaut, son
groupe… Ces métadonnées sont écrites sous forme d'**annotations sémantiques** `@xxx` directement dans
le GLSL Vulkan natif. Elles **enrichissent** le shader pour l'UI, puis sont **strippées** avant
glslang (qui ne les comprendrait pas).

La convention est **un fichier par stage** (`.vert.vk.glsl`, `.frag.vk.glsl`, `.comp.vk.glsl`), le
stage étant auto-déduit du nom. Deux choses sont **volontairement non supportées** : le multi-stage
dans un même fichier, et la génération de code (le codegen, c'est le job de `NkShaderConverter` — ici
on ne fait qu'extraire des métadonnées et nettoyer la source).

### Le vocabulaire des annotations

`NkShaderAnnotationKind` énumère les genres : les paramètres exposés (`@param`, `@color` avec option
sRGB, `@range`), les textures (`@texture2D`, `@cubemap`, `@texture3D`, `@textureArray`), les choix
(`@enum`), l'organisation (`@group`, `@hidden`), et les méta de shader (`@material`, `@stage`,
`@entry`). À cela s'ajoutent des **alias de types natifs** (`@float`, `@int`, `@bool`, `@vec2/3/4`,
`@ivec2/3/4`, `@mat3`, `@mat4`) qui sont du sucre pour `@param` avec le `glslType` pré-rempli.

Chaque annotation parsée est un `NkShaderAnnotation` (forme générique dont le `kind` discrimine les
champs utiles) : nom, libellé, info-bulle, groupe, type GLSL, valeurs par défaut/min/max
(`NkShaderAnnotationValue`, typée par son `Type`), slot de texture, chemin par défaut, drapeau `srgb`,
valeurs d'énumération, ligne source. Le résultat global d'un parsing est un `NkShaderMetadata` : nom du
matériau (de `@material(name=)`), stage, point d'entrée, et le vecteur d'annotations — avec un
`FindByName` (recherche linéaire `O(n)`) pour retrouver un paramètre.

### Parser, stripper

`NkShaderAnnotationParser` (statique) fait le travail. `Parse(rawSource, hintStage)` extrait les
`@xxx` et renvoie un `NkShaderAnnotationResult` : `success`, la `cleanSource` (GLSL Vulkan **annotations
strippées, mais lignes préservées** pour que les numéros d'erreur de glslang restent justes), la
`metadata`, et les `errors` (les annotations mal formées génèrent des warnings non bloquants). Le
`hintStage` (suggéré par le nom de fichier) est **overridé par `@stage()`** s'il est présent.
`StripAnnotations` fait le nettoyage seul, sans extraire les métadonnées — plus rapide quand on veut
juste compiler.

```cpp
auto a = NkShaderAnnotationParser::Parse(rawVkGlsl);
if (a.success) {
    NkGLSLToSPIRV(a.metadata.stage, a.cleanSource.Cstr());   // cleanSource → glslang
    // a.metadata.annotations → exposées à l'UI de l'éditeur de matériaux
}
```

Le pipeline type est donc : `Parse(raw)` → `cleanSource` vers glslang (→ SPIR-V), pendant que
`metadata` est stockée par la shader library et présentée à l'éditeur.

> **En résumé.** Les annotations `@xxx` décrivent les paramètres d'un matériau **pour l'UI** ; elles
> sont strippées avant glslang. `NkShaderAnnotationParser::Parse` rend `cleanSource` (lignes
> préservées) + `metadata` ; `@stage()` override le stage déduit du nom. Pas de multi-stage ni de
> codegen ici (c'est le rôle de `NkShaderConverter`).

---

## Aperçu de l'API

### `NkSLCompiler.h` — façade, caches, bibliothèque

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Façade ctor | `NkSLCompiler(cacheDir = "")`, `~NkSLCompiler()` | Compilateur (cache activé par défaut). |
| Compilation | `Compile(src, stage, target, opts, filename)` | Compilation simple vers une cible. |
| Compilation | `CompileWithReflection(...)` | Code + réflexion en une passe. |
| Compilation | `Reflect(src, stage, filename)` | Réflexion seule, sans codegen. |
| Compilation | `CompileAllTargets(src, stage, targets, opts, filename)` | Plusieurs cibles, réflexion partagée. |
| Compilation | `struct MultiTargetResult { results; reflection; allSucceeded() }` | Résultat multi-cibles. |
| Spécialisé | `CompileToSPIRV(glslSrc, stage, opts)` | GLSL → SPIR-V (glslang/shaderc). |
| Spécialisé | `CompileToMSL_SpirvCross(...)` | MSL via SPIRV-Cross. |
| Spécialisé | `CompileWithSemantic(...)` | Compilation avec analyse sémantique préalable. |
| Validation | `Validate(src, filename)` | Erreurs seules (linter), sans codegen. |
| Prétraitement | `Preprocess(src, baseDir, errors*, includedFiles*)` | Déplie + remonte erreurs/fichiers inclus. |
| Configuration | `SetCacheDir`, `SetGlslangPath`, `EnableCache`, `EnableDebug`, `GetCache` | Réglages et accès au cache interne. |
| Cache (entrée) | `struct NkSLCacheEntry { sourceHash; target; stage; bytecode; source; timestamp; isSpirv }` | Une entrée de cache. |
| Cache | `NkSLCache(cacheDir = "")` | Cache mémoire `O(1)`, thread-safe. |
| Cache | `Has`, `Get`, `Put`, `Clear`, `Size` | Interroger / insérer / vider / compter. |
| Cache | `SetDirectory`, `Flush`, `Load` | Répertoire + persistance disque. |
| Résultat | `struct NkSLCompileResultWithReflection { result; reflection; hasReflection }` | Résultat enrichi. |
| Library | `NkSLShaderLibrary(compiler*, baseDir = "")` | Collection nommée + hot-reload (compilateur **emprunté**). |
| Library | `struct ShaderEntry { name; sourcePath; source; stages; compiled; reflection; dirty }` | Une entrée enregistrée. |
| Library | `Register`, `RegisterInline` | Enregistrer depuis fichier / source inline. |
| Library | `CompileAll(target, opts)` | Compiler toutes les entrées pour une cible. |
| Library | `Get(name, stage, target)`, `GetReflection(name)` | Accès (pointeurs **invalidés** par recompile). |
| Library | `HotReload(target, opts)`, `Count` | Recompiler le `dirty` / compter. |
| Alias | `NkSLMutexLock` | Lock interne (`NkScopedLockMutex`). |

### `NkGLSLCompiler.h` — GLSL → SPIR-V (glslang)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Résultat | `struct NkGLSLCompileResult { success; spirv; errorLog }` | `errorLog` = buffer statique interne. |
| Fonction | `NkGLSLToSPIRV(stage, glslSrc, entry = "main")` | Compile GLSL → SPIR-V (sans Vulkan). |
| Cycle de vie | `NkGLSLCompilerInit()`, `NkGLSLCompilerShutdown()` | Init (idempotent) / libération de glslang. |

### `NkSLCodeGen.h` — backends et utilitaires

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Utilitaire type | `NkSLBaseTypeName`, `NkSLTypeName` | Nom d'un type de base. |
| Utilitaire type | `NkSLBaseTypeSize` | Taille en octets **std140** (mat3 = 48 !). |
| Utilitaire type | `NkSLBaseTypeComponents` | Nombre de composantes. |
| Utilitaire type | `NkSLImageFormatToHLSLElem`, `NkSLImageFormatToMSLElem` | Format image → type d'élément HLSL / MSL. |
| Base | `NkSLCodeGenBase(target)`, `~NkSLCodeGenBase` | Base abstraite des backends. |
| Base | `Generate(ast, stage, opts)` (pur), `GetTarget` | Point d'entrée polymorphe + cible. |
| Base (protégé) | `IndentPush/Pop`, `Indent`, `Emit*`, `TypeToString`, `AddError/Warning` | Helpers d'émission pour sous-classes. |
| Backend | `NkSLCodeGenGLSL` | GLSL OpenGL 4.30+ (bindings aplatis, auto-location). |
| Backend | `NkSLCodeGenGLSLVulkan` | GLSL 4.50 Vulkan (`set/binding`, push_constant). |
| Backend | `NkSLCodeGenHLSL_DX11` | HLSL SM5 / fxc (compteur `mReg` partagé). |
| Backend | `NkSLCodeGenHLSL` | Alias de `NkSLCodeGenHLSL_DX11` (rétrocompat). |
| Backend | `NkSLCodeGenHLSL_DX12` | HLSL SM6+ / dxc (RootSignature, bindless). |
| Backend | `NkSLCodeGen_MSL` | MSL natif depuis l'AST. |
| Backend | `NkSLCodeGenMSLSpirvCross` (+ `GenerateFromSPIRV`) | MSL via SPIRV-Cross. |
| Backend | `NkSLCodeGenCPP` | C++ pour le rasterizer logiciel. |
| Réflexion | `NkSLReflector::Reflect(ast, stage)` | Extraction des bindings depuis l'AST. |
| Réflexion | `GenerateLayoutJSON`, `GenerateLayoutCPP` | Sérialiser le layout en JSON / C++. |

### `NkShaderConvert.h` — conversion et résolution

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Remap | `struct NkDXResourceBinding { set; binding; cbvReg; srvReg; samplerReg; uavReg; space }` | (set,binding) Vulkan → registres DX (`~0u` = inutilisé). |
| Résultat | `struct NkShaderConvertResult { success; source; binary; errors; dxBindings }` | Texte ou SPIR-V (octets) + remap. |
| Résultat | `SpirvWords`, `SpirvWordCount`, `GetSpirvWordsCopy` | Relire le SPIR-V (copie **alignée** au besoin). |
| Résolution | `NkShaderFileResolver::BasePath/FormatExt/StageExt` | Décomposer `shader.<stage>.<format>`. |
| Résolution | `StageFrom`, `ResolveVariant`, `FindVariants`, `FileExists` | Déduire le stage / variantes (disque). |
| Capacités | `CanGlslToSpirv/CanSpirvToGlsl/CanSpirvToHlsl/CanSpirvToMsl` | À tester **avant** toute conversion. |
| Conversion | `GlslToSpirv(glslSrc, stage, debugName)` | GLSL 4.30+ sans annotation → SPIR-V. |
| Conversion | `SpirvToGlsl/SpirvToHlsl/SpirvToMsl` (+ surcharges helper) | SPIR-V → texte (HLSL : shader model). |
| Chargement | `LoadFile(path)`, `LoadAsSpirv(path)` | Auto-détection / chargement en SPIR-V. |
| Raccourci | `GlslFileToHlsl/GlslFileToMsl/GlslFileToGlsl` | Fichier GLSL → cible (chaîne complète). |
| Conversion | `GlslToHlsl/GlslToMsl/GlslToGlsl` | Source GLSL Vulkan-style → cible. |
| Cache disque | `NkShaderCache` : `SetCacheDir`, `CacheDir`, `ComputeKey` | Cache `.nksc` (FNV-1a, `noexcept`). |
| Cache disque | `Load`, `Save`, `Invalidate`, `Clear` | Lire / écrire / invalider / vider. |
| Cache disque | `PurgeUnused`, `PurgeUnusedThisSession`, `PurgeOlderThan` | GC (du sûr au très agressif). |
| Cache disque | `Global()` | Singleton optionnel. |

### `NkShaderAnnotations.h` — métadonnées éditeur

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Genre | `enum class NkShaderAnnotationKind` | `@param`/`@color`/`@range`/textures/`@enum`/`@group`/… + alias de types. |
| Valeur | `struct NkShaderAnnotationValue { Type; f[4]; i; b; s }` | Valeur typée (default/min/max). |
| Annotation | `struct NkShaderAnnotation { kind; name; label; tooltip; group; glslType; default/min/maxValue; slot; defaultPath; srgb; enumValues; sourceLine }` | Une annotation parsée. |
| Métadonnées | `struct NkShaderMetadata { materialName; stage; stageExplicit; entryPoint; annotations }` (+ `FindByName`) | Résultat global du parsing. |
| Résultat | `struct NkShaderAnnotationResult { success; cleanSource; metadata; errors }` | `cleanSource` strippée, lignes préservées. |
| Parser | `NkShaderAnnotationParser::Parse(raw, hintStage)` | Extraire `@xxx` + nettoyer (`@stage()` override). |
| Parser | `StripAnnotations(raw)` | Strip seul (compile-only, plus rapide). |

---

## Référence complète

Chaque élément public est repris ici à fond, avec ses cas d'usage à travers les domaines du moteur —
le rendu d'abord, mais aussi l'animation, le post-traitement, l'UI/2D, le compute, l'outillage
d'éditeur et le build.

### `NkSLCompiler` — la façade et ses voies de compilation

La façade est le seul objet que la plupart du code touche. Elle agrège front-end, sémantique,
backends et cache, et choisit le bon backend selon la `NkSLTarget` demandée.

- **`Compile`** — la voie ordinaire. *Rendu* : compiler le shader Phong/PBR d'un matériau vers la
  cible du backend GPU courant. *2D/UI* : compiler un shader de quad texturé pour NKCanvas ou
  l'éditeur. *Compute* : compiler un kernel de simulation (particules, post-traitement). Le `filename`
  ne sert qu'à étiqueter les erreurs lisiblement.
- **`CompileWithReflection`** — quand on veut bâtir un layout de descripteurs *immédiatement* après la
  compilation : le résultat porte à la fois le code et la `NkSLReflection`, donc une seule passe. Utile
  partout où le code consommateur (NKRHI via NkSLIntegration, un outil d'inspection) a besoin de savoir
  *quels* bindings le shader attend.
- **`Reflect`** — réflexion **sans** générer de code. *Outils/éditeur* : afficher la liste des uniforms
  d'un shader, valider qu'un matériau fournit bien tous ses paramètres, générer une UI d'inspecteur,
  tout cela sans payer le coût d'une génération de code.
- **`CompileAllTargets` / `MultiTargetResult`** — la voie du **build cross-plateforme**. On donne la
  liste des cibles à produire (GLSL pour le poste de dev, Vulkan + HLSL + MSL pour la distribution), et
  une **seule** réflexion est calculée et partagée — garantissant que toutes les variantes voient le
  même layout. `allSucceeded()` court-circuite au premier échec : un script de build s'en sert pour
  marquer la passe comme rouge dès qu'une cible casse.
- **`CompileToSPIRV`** — entrée **GLSL** (pas NkSL) vers SPIR-V. Pratique pour intégrer du GLSL
  existant (shaders tiers, prototypes) dans le pipeline sans le réécrire en NkSL.
- **`CompileToMSL_SpirvCross`** — Metal par la route SPIRV-Cross, la plus robuste pour les cibles
  Apple (macOS/iOS) où le MSL natif peut buter sur des constructions difficiles.
- **`CompileWithSemantic`** — force l'analyse sémantique avant la génération : on l'utilise quand on
  veut être **certain** que la source est valide (et obtenir des diagnostics complets) plutôt que de
  laisser un éventuel chemin rapide.
- **`Validate`** — *outillage* pur : un éditeur de shader appelle `Validate` à chaque frappe (ou à la
  sauvegarde) pour souligner les erreurs, sans jamais produire de code. Renvoie le vecteur des
  `NkSLCompileError`.
- **`Preprocess`** — déplie les `#include` avec garde `#pragma once`, et **remonte les fichiers
  inclus** via son pointeur optionnel. C'est *la* brique du **hot-reload** : connaître l'ensemble des
  fichiers dont dépend un shader permet de surveiller le bon ensemble de fichiers et de marquer le
  shader `dirty` dès qu'un de ses inclus change.
- **Configuration** — `EnableCache` (activé par défaut) coupe/active le cache mémoire ; `EnableDebug`
  (désactivé par défaut) ajoute des informations de debug ; `SetCacheDir`/`SetGlslangPath`
  paramètrent les chemins ; `GetCache` rend la main sur le cache interne.

### `NkSLCache` — le cache mémoire

Un cache `O(1)` thread-safe au cœur de la façade. Le point qui surprend est la **clé** : fabriquée
depuis (hash source, cible, stage), elle isole chaque combinaison.

- *Build incrémental* — sur un projet à des centaines de shaders, le cache évite de recompiler ce qui
  n'a pas changé : `Has`/`Get` court-circuitent, `Put` mémorise. La distinction par cible/stage permet
  de cacher simultanément les variantes GLSL, Vulkan et HLSL d'un même shader sans qu'elles se
  marchent dessus.
- *Persistance* — `Flush` écrit le cache, `Load` le relit : entre deux lancements de l'éditeur, on
  repart d'un cache chaud (`SetDirectory` choisit l'emplacement). `Clear` repart de zéro, `Size`
  diagnostique le nombre d'entrées.
- *Thread-safety* — un `mMutex` mutable protège toutes les méthodes publiques (`NkSLMutexLock`), donc
  plusieurs threads de compilation (un *thread pool* de build) peuvent partager un cache sans verrou
  externe.

### `NkSLShaderLibrary` — collection nommée et hot-reload

La library transforme une poignée de fichiers en une **base de shaders vivante**.

- *Pipeline d'assets* — `Register` enregistre chaque shader par nom et chemin, `CompileAll` les bâtit
  tous pour une cible. Le moteur récupère ensuite un shader par son nom logique (`Get("phong", ...)`)
  sans connaître son chemin disque.
- *Itération en édition* — `HotReload` est le cœur de la boucle d'auteur : un *watcher* marque les
  entrées modifiées `dirty`, `HotReload` ne recompile **que** celles-là et renvoie combien ont été
  rechargées — on voit l'effet d'une retouche de shader instantanément, sans relancer.
- *Réflexion par nom* — `GetReflection("phong")` donne le layout d'un shader nommé, pour brancher ses
  bindings ou peupler une UI.
- *Ownership* — deux règles dures. La library **emprunte** son `NkSLCompiler*` : elle ne le détruit
  jamais, vous restez responsable de sa durée de vie. Et les pointeurs renvoyés par `Get`/
  `GetReflection` vivent **dans** la library : un `CompileAll`/`HotReload` peut les invalider, donc on
  ne les conserve pas à travers un rechargement — on re-`Get` après.

### `NkGLSLToSPIRV` et le cycle de vie glslang

La fonction libre `NkGLSLToSPIRV` est la route la plus courte de GLSL **pur** vers SPIR-V, via
libglslang in-tree, **sans dépendre de Vulkan**.

- *Pipeline Vulkan* — produire le SPIR-V à charger dans un `VkShaderModule` à partir d'un GLSL généré.
- *Outils* — vérifier qu'un GLSL compile (le `errorLog` donne les diagnostics).
- Précautions : tester `success` (l'implémentation est un **stub** sans `NK_RHI_GLSLANG_ENABLED`),
  appeler `NkGLSLCompilerInit` une fois au démarrage (idempotent) et `NkGLSLCompilerShutdown` à
  l'arrêt, et **copier** `errorLog` si on le garde (buffer statique interne, non réentrant).

### Les utilitaires de type `NkSLBaseType*`

Ces fonctions libres traduisent et mesurent les types — la plomberie de tout calcul de layout.

- **`NkSLBaseTypeName` / `NkSLTypeName`** — afficher ou émettre le nom d'un type (génération, logs,
  inspecteur).
- **`NkSLBaseTypeSize`** — taille **std140 pour UBO**. C'est l'outil pour calculer la taille d'un
  buffer uniforme **sans se tromper sur le rembourrage** : `NK_MAT3` occupe **48** octets (trois
  `vec4`, pas 36), `NK_MAT4` 64, `NK_VEC3` 12, `NK_DVEC4` 32, l'inconnu 0. *Rendu/compute* : tout code
  qui alloue un UBO côté CPU pour matcher le shader doit utiliser cette fonction, pas `sizeof`.
- **`NkSLBaseTypeComponents`** — nombre de composantes (mat3→9, vec4→4, scalaire→1) : utile pour
  l'introspection, les conversions de données, le remplissage d'attributs de vertex.
- **`NkSLImageFormatToHLSLElem` / `NkSLImageFormatToMSLElem`** — convertissent un format d'image NkSL
  (`"r32f"`, `"rgba8i"`…) vers le type d'élément d'un `RWTexture2D` HLSL ou d'un `texture2d<T,...>`
  MSL : indispensables au codegen des shaders **compute** qui écrivent dans des images de stockage.

### Les backends `NkSLCodeGen*`

Tous dérivent de `NkSLCodeGenBase` et implémentent `Generate(ast, stage, opts)`. La base fournit les
helpers d'émission (indentation, `Emit*`, traduction de types, report d'erreurs) et porte la taille de
workgroup compute `mLocalSizeX/Y/Z`.

- **`NkSLCodeGenGLSL`** (OpenGL 4.30+) — bindings aplatis, et surtout **auto-assignation des
  locations** d'entrée/sortie et des bindings d'UBO quand la source les omet : sans cela, l'ordre des
  attributs n'est pas garanti côté GL → **écran noir**, le bug classique. Émule les push constants
  (tableau `uniform vec4 _PushConstants[N]` + réécriture des accès) et gère le Y-flip GL (sans flipper
  un VS depth-only ni un 2D pur push-constant). C'est le backend du **poste de dev** et de tout ce qui
  tourne sur OpenGL (NKCanvas 2D, démos).
- **`NkSLCodeGenGLSLVulkan`** (GLSL 4.50 Vulkan) — `layout(set,binding)` **obligatoires**,
  `push_constant`, `subpassInput`/`subpassLoad` (passes de *deferred*/post-traitement),
  `gl_VertexIndex`/`gl_InstanceIndex` et, sous `opts.vkDrawParams`, `gl_BaseVertex`/`gl_BaseInstance`
  (rendu **instancié**, multi-draw indirect). Auto-assigne locations/set/binding car Vulkan les exige
  (sinon SPIR-V invalide). C'est la source canonique du pipeline de distribution.
- **`NkSLCodeGenHLSL_DX11`** (SM5/fxc) et **`NkSLCodeGenHLSL_DX12`** (SM6+/dxc) — les cibles Direct3D.
  DX12 ajoute `register(bN, spaceM)`, la RootSignature inline (`opts.dx12InlineRootSignature`), les
  *wave intrinsics* SM6.0 et le **bindless** SM6.6 (`opts.dx12BindlessHeap`). Les deux partagent un
  compteur `mReg` **unique** entre classes de registre (`b`/`t`/`s`/`u`) qui doit matcher le compteur
  côté device — règle critique pour que les ressources se branchent au bon slot (ubo→0, shadow→1,
  albedo→2). `NkSLCodeGenHLSL` est un alias de la variante DX11 (rétrocompat).
- **`NkSLCodeGen_MSL`** et **`NkSLCodeGenMSLSpirvCross`** — les deux routes vers Metal (macOS/iOS) :
  natif depuis l'AST, ou via SPIRV-Cross (plus robuste, et `GenerateFromSPIRV` permet de repartir
  directement d'un SPIR-V déjà produit).
- **`NkSLCodeGenCPP`** — émet du **C++** pour exécuter le shader sur le **rasterizer logiciel** : la
  cible qui rend sans GPU (CI sans carte, fallback, tests de référence pixel).

### `NkSLReflector` — extraire les bindings

`Reflect(ast, stage)` parcourt l'AST et en sort une `NkSLReflection` (la liste des bindings, leurs
types, leurs tailles). Au-delà du runtime, deux générateurs servent l'**outillage** :
`GenerateLayoutJSON` produit une description JSON du layout (un éditeur la lit pour afficher la
structure d'un shader, un outil de diff la compare entre deux versions), et `GenerateLayoutCPP` émet
une **déclaration C++** du layout — pour figer un layout dans du code généré et le compiler en dur.

### `NkShaderConverter` et `NkShaderFileResolver`

Le couple résolution + conversion gère les shaders **déjà sur disque** ou **déjà compilés**.

- **Résolution** (`NkShaderFileResolver`, statique, **toujours** dispo) — comprend la convention
  `shader.<stage>.<format>` : `StageFrom` déduit le stage, `ResolveVariant` bascule d'un format à un
  autre en gardant le stage, `FindVariants` liste ce qui existe réellement. *Pipeline d'assets* :
  préférer la variante `.spirv` si elle est présente, sinon compiler le `.glsl` — `FindVariants` +
  `ResolveVariant` orchestrent ce choix.
- **Capacités** (`CanGlslToSpirv`/`CanSpirvToGlsl`/`CanSpirvToHlsl`/`CanSpirvToMsl`) — à interroger
  **avant** toute conversion, car elles reflètent les flags de build : sans `NK_RHI_GLSLANG_ENABLED`
  ou `NK_RHI_SPIRVCROSS_ENABLED`, la conversion est un stub.
- **Conversions** — `GlslToSpirv` (GLSL **sans annotation** → SPIR-V) ; `SpirvToGlsl`/`SpirvToHlsl`
  (avec shader model) / `SpirvToMsl` (et leurs surcharges *helper* qui acceptent directement un
  `NkShaderConvertResult`) ; les raccourcis fichier `GlslFileToHlsl`/`GlslFileToMsl`/`GlslFileToGlsl`
  et les variantes texte `GlslToHlsl`/`GlslToMsl`/`GlslToGlsl` à partir d'un GLSL **Vulkan-style**
  canonique. C'est le pipeline de **distribution cross-API** : un GLSL Vulkan unique devient HLSL et
  MSL.
- **Chargement** — `LoadFile` auto-détecte le format ; `LoadAsSpirv` ramène toujours du SPIR-V (charge
  un `.spv`, compile un `.glsl`, **refuse** un `.hlsl`/`.msl`).
- **Le résultat** — `NkShaderConvertResult` porte `source` (texte) ou `binary` (SPIR-V en octets).
  Relire le SPIR-V passe par `SpirvWords()`/`SpirvWordCount()`, ou `GetSpirvWordsCopy()` pour une copie
  **alignée** si l'alignement de `binary` n'est pas garanti. Pour la route HLSL, `dxBindings` (vecteur
  de `NkDXResourceBinding`) donne le **remap compact** des registres DX : chaque (set,binding) Vulkan
  reçoit un `cbvReg`/`srvReg`/`samplerReg`/`uavReg` (et un `space`), `0xFFFFFFFFu` marquant les classes
  inutilisées — c'est ce qui permet de brancher les ressources côté D3D sans collision.

### `NkShaderCache` — le cache disque des conversions

Distinct du `NkSLCache` mémoire, ce cache met les **conversions** sur disque en fichiers `.nksc`
(clé FNV-1a, méthodes `noexcept`).

- *Build et runtime* — `ComputeKey` puis `Load`/`Save` évitent de re-convertir un shader inchangé d'un
  lancement à l'autre. `Global()` offre un cache partagé sans le câbler partout.
- *Hygiène disque* — trois GC du plus sûr au plus risqué : `PurgeOlderThan` (par date, conservateur,
  pensé **CI**), `PurgeUnused` (garde une liste de clés vivantes), et `PurgeUnusedThisSession`
  (supprime tout ce qui n'a été ni lu ni écrit pendant la session — **très agressif**, à proscrire en
  développement partiel où l'on n'a touché qu'une fraction des shaders). `Invalidate` cible une entrée,
  `Clear` rase tout.

### `NkShaderAnnotations` — les métadonnées d'éditeur

Le sous-module d'annotations sert **l'éditeur de matériaux**, pas le rendu.

- **Le vocabulaire** (`NkShaderAnnotationKind`) — décrit comment exposer un paramètre dans l'UI :
  `@param`/`@color`(+sRGB)/`@range` pour les valeurs, `@texture2D`/`@cubemap`/`@texture3D`/
  `@textureArray` pour les textures, `@enum`/`@group`/`@hidden` pour l'organisation, `@material`/
  `@stage`/`@entry` pour les méta, plus les alias de types (`@float`…`@mat4`) qui pré-remplissent le
  `glslType`.
- **Les structures** — `NkShaderAnnotationValue` (valeur typée : defaults/min/max),
  `NkShaderAnnotation` (la forme générique, dont `kind` discrimine les champs), `NkShaderMetadata` (le
  résultat global : nom du matériau, stage, point d'entrée, vecteur d'annotations, avec `FindByName`
  pour retrouver un paramètre), `NkShaderAnnotationResult` (l'enveloppe `Parse`).
- **Le parsing** (`NkShaderAnnotationParser`, statique) — `Parse` extrait les `@xxx` et produit la
  `cleanSource` (annotations strippées, **lignes préservées** pour que les numéros d'erreur glslang
  restent exacts) plus la `metadata` ; le `hintStage` déduit du nom est **overridé par `@stage()`**.
  `StripAnnotations` fait le nettoyage seul, plus rapide pour un compile-only. Le pipeline est
  toujours : `Parse(raw)` → `cleanSource` vers glslang, `metadata` vers la shader library puis l'UI.
  Le multi-stage par fichier et le codegen sont **volontairement** hors périmètre (le codegen, c'est
  `NkShaderConverter`).

### Idiomes et pièges transverses

- **Inversion de dépendance** — NKSL n'inclut **jamais** NKRHI. Pour un `NkShaderDesc`, passer par
  `NkSLIntegration` côté NKRHI ; ici on ne sort que source / SPIR-V / réflexion.
- **Capacités conditionnelles** — `CanXxx()` (et la présence d'implémentation pour `NkGLSLToSPIRV`)
  reflètent `NK_RHI_GLSLANG_ENABLED` / `NK_RHI_SPIRVCROSS_ENABLED`. Sans ces flags, ce sont des stubs :
  tester `success`/`CanXxx()`.
- **Deux caches distincts** — `NkSLCache` (mémoire, par compilateur) vs `NkShaderCache` (disque
  `.nksc`, conversions). À ne pas confondre.
- **Thread-safety** — `NkSLCache` et `NkSLShaderLibrary` ont chacun un mutex interne.
  `NkGLSLCompileResult::errorLog` pointe sur un **buffer statique interne** non réentrant : copier le
  message.
- **Ownership** — `NkSLShaderLibrary` **emprunte** son `NkSLCompiler*` (ne le détruit pas) ; les
  pointeurs `Get`/`GetReflection` sont **invalidés** par `CompileAll`/`HotReload`.
- **Compteur de binding partagé** — dans les backends HLSL DX11/DX12, `mReg` mappe directement le slot
  de registre et doit matcher le compteur partagé côté device (ubo→0, shadow→1, albedo→2) ; ne pas
  repartir de compteurs séparés par `b`/`t`/`s`/`u`.
- **Auto-location/binding** — GLSL et GLSL-Vulkan auto-assignent les locations in/out et les bindings
  d'UBO omis (obligatoire pour éviter écran noir en GL et SPIR-V invalide en VK).
- **std140** — `NkSLBaseTypeSize` suit std140 (mat3 = 48 octets) : l'utiliser pour dimensionner les
  UBO côté CPU.

---

### Exemple

```cpp
#include "NKSL/Compiler/NkSLCompiler.h"
#include "NKSL/ShaderConvert/NkShaderConvert.h"
using namespace nkentseu;

// 1) Compiler une source NkSL vers plusieurs cibles, réflexion partagée.
NkSLCompiler compiler;
NkVector<NkSLTarget> cibles{ NkSLTarget::NK_GLSL,
                             NkSLTarget::NK_GLSL_VULKAN,
                             NkSLTarget::NK_HLSL_DX11 };
auto multi = compiler.CompileAllTargets(src, NkSLStage::NK_FRAGMENT, cibles);
if (multi.allSucceeded()) {
    // multi.results[0..2] = GLSL / Vulkan-GLSL / HLSL ; multi.reflection = layout commun
}

// 2) Bibliothèque hot-reload (le compilateur est emprunté, pas possédé).
NkSLShaderLibrary lib(&compiler, "Assets/Shaders");
lib.Register("phong", "phong.nksl", { NkSLStage::NK_VERTEX, NkSLStage::NK_FRAGMENT });
lib.CompileAll(NkSLTarget::NK_GLSL);
uint32 n = lib.HotReload(NkSLTarget::NK_GLSL);   // ne recompile que le modifié

// 3) Convertir un GLSL Vulkan-style en HLSL — toujours vérifier la capacité.
if (NkShaderConverter::CanGlslToSpirv() && NkShaderConverter::CanSpirvToHlsl()) {
    auto h = NkShaderConverter::GlslToHlsl(glslVk, NkSLStage::NK_VERTEX, 50);
    if (h.success) { /* h.source = HLSL, h.dxBindings = remap compact des registres */ }
}

// 4) Dimensionner un UBO en respectant std140 (mat3 = 48 octets !).
uint32 uboSize = NkSLBaseTypeSize(NkSLBaseType::NK_MAT4)   // 64
               + NkSLBaseTypeSize(NkSLBaseType::NK_VEC3);  // 12
```

---

[← Index NKSL](README.md) · [Récap NKSL](../NKSL.md) · [Couche Runtime](../README.md)
