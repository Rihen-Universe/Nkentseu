# Les types du langage et l'arbre syntaxique

> Couche **Runtime** · NKSL · Le **vocabulaire** du compilateur de shaders : les cibles et les
> étages (`NkSLTarget`, `NkSLStage`), les types du langage (`NkSLBaseType`), l'**arbre syntaxique**
> qui représente un shader analysé (`NkSLNode` et sa famille), et les **capacités** par cible
> (`NkSLFeatureCaps`).

Un shader, avant d'être du GLSL, du HLSL ou du SPIR-V, est d'abord un **texte** qu'il faut lire,
comprendre, puis ré-émettre dans le dialecte de la machine cible. Entre les deux, le compilateur
NKSL en garde une représentation **structurée et neutre** : un arbre où chaque nœud est une
construction du langage (une fonction, une boucle, une addition, un accès de membre). C'est cet
arbre — l'**AST** (*abstract syntax tree*) — et le **vocabulaire de types** qui l'accompagne que
documente cette page. Tout part d'une idée simple : **séparer ce qu'un shader dit de la façon dont
chaque API l'écrit.** On analyse une fois ; on régénère pour autant de cibles qu'on veut.

Ce n'est **pas** le petit transpileur NkSL ad-hoc embarqué dans NKRenderer (celui qui fait du
remplacement de texte sur des `.nksl` annotés `@uniform`/`@target`). C'est le **vrai compilateur** :
il construit un véritable arbre syntaxique, classe les types, modélise les capacités matérielles, et
sait viser dix cibles. Les deux coexistent dans le moteur — ne les confondez pas.

- **Namespace** : `nkentseu` (pas de sous-namespace)
- **Headers** : `#include "NKSL/Core/NkSLTypes.h"` (types de base), `"NKSL/Core/NkSLAST.h"` (arbre),
  `"NKSL/Core/NkSLFeatures.h"` (capacités, stages avancés, générateurs)

---

## Les cibles et les étages : `NkSLTarget`, `NkSLStage`

Avant tout, il faut nommer **où** le shader va tourner et **quand** dans le pipeline. La cible —
`NkSLTarget` — énumère les dialectes de sortie : `NK_GLSL` (OpenGL, bindings aplatis),
`NK_GLSL_VULKAN` (Vulkan GLSL avec `set`+`binding`), `NK_SPIRV` (binaire Vulkan), `NK_HLSL_DX11`
(SM5, *fxc*), `NK_HLSL_DX12` (SM6+, *dxc*), `NK_MSL` (Metal natif), `NK_MSL_SPIRV_CROSS` (Metal via
SPIRV-Cross), `NK_CPLUSPLUS` (rasterizer logiciel, C++ généré), `NK_BYTECODE` (logiciel, exécuté par
NkSLVM). Plutôt que de tester chaque valeur à la main, on dispose de **familles** : `NkSLTargetIsGLSL`,
`NkSLTargetIsHLSL`, `NkSLTargetIsMSL`, `NkSLTargetIsVulkan` répondent par oui/non, et
`NkSLTargetName` donne un libellé lisible pour les logs.

L'étage — `NkSLStage` — dit *quelle* partie du pipeline : `NK_VERTEX`, `NK_FRAGMENT`, `NK_GEOMETRY`,
`NK_TESS_CTRL`, `NK_TESS_EVAL`, `NK_COMPUTE`, `NK_MESH`, `NK_TASK`. La subtilité capitale : ce n'est
**pas** un simple enum, c'est un **bitmask**. Chaque valeur est une puissance de deux, donc on peut
les **combiner** (`NK_VERTEX | NK_FRAGMENT`) pour dire « ce binding sert à la fois au vertex et au
fragment ». Les agrégats `NK_ALL_GRAPHICS` et `NK_ALL` existent déjà construits.

```cpp
if (NkSLTargetIsVulkan(target)) { /* set + binding, layout Vulkan */ }

NkSLStage used = NK_VERTEX | NK_FRAGMENT;
if (NkSLStageHas(used, NK_FRAGMENT)) { /* présent dans le fragment */ }
NkString s = NkSLStageToString(used);   // "VERTEX|FRAGMENT"
```

Attention au piège : `NkSLStageName` ne renvoie le nom singulier que d'une **valeur exacte** (« vertex »,
« fragment »…) et retourne « unknown » sur une combinaison — *et ne couvre même pas* `NK_MESH`/`NK_TASK`.
Pour afficher une combinaison, c'est `NkSLStageToString` qui décompose en `"A|B|C"`. Note d'architecture :
NKRHI ne redéfinit pas son propre enum d'étages, il réutilise celui-ci via `using NkShaderStage = NkSLStage;`.

> **En résumé.** `NkSLTarget` = le dialecte de sortie (dix cibles), interrogé par les familles
> `…IsGLSL/IsHLSL/IsMSL/IsVulkan`. `NkSLStage` = l'étage, **bitmask combinable** : utilisez les
> opérateurs `|`/`&`/`~`, `NkSLStageHas` pour tester, `NkSLStageToString` pour afficher une
> combinaison (`NkSLStageName` ne gère qu'une valeur exacte et ignore MESH/TASK).

---

## Les types du langage : `NkSLBaseType`

Un shader manipule des scalaires, des vecteurs, des matrices, des samplers, des images. `NkSLBaseType`
les énumère **tous**, calqués sur GLSL : `NK_FLOAT`, `NK_VEC2/3/4`, `NK_INT`, `NK_IVEC*`, `NK_UINT`,
`NK_UVEC*`, `NK_DOUBLE`, `NK_DVEC*`, `NK_BOOL`, les matrices `NK_MAT2/3/4` (et leurs variantes
rectangulaires `NK_MAT2X3`…), les `NK_DMAT*`, puis toute la zoologie des **samplers** (`NK_SAMPLER2D`,
`NK_SAMPLER_CUBE`, `NK_SAMPLER2D_SHADOW`, versions `i`/`u`…), des **storage images** (`NK_IMAGE2D`…),
les entrées de subpass Vulkan (`NK_SUBPASS_INPUT`, `NK_SUBPASS_INPUT_MS`), et les composés
`NK_STRUCT`, `NK_ARRAY`, `NK_UNKNOWN`.

Le point à comprendre — et à respecter — est que **l'ordre de cet enum est porteur de sens**
(*load-bearing*). Les classificateurs `NkSLTypeIsSampler` et `NkSLTypeIsImage` ne font pas un long
`switch` : ils testent si la valeur tombe dans une **plage contiguë** (`t >= NK_SAMPLER2D && t <=
NK_USAMPLER_CUBE_ARRAY` pour les samplers). C'est `O(1)`, mais cela impose de **ne jamais réordonner**
l'énumération. Troisième classificateur, `NkSLTypeIsMatrix`, reconnaît `MAT2/3/4` et les rectangulaires
— mais **exclut** volontairement les `NK_DMAT*` (matrices double), piège classique.

```cpp
if (NkSLTypeIsSampler(t)) { /* déclarer une texture échantillonnable */ }
if (NkSLTypeIsImage(t))   { /* storage image : nécessite un imageFormat */ }
if (NkSLTypeIsMatrix(t))  { /* attention : NK_DMAT* renverra false ici */ }
```

> **En résumé.** `NkSLBaseType` = tout le vocabulaire de types GLSL (scalaires, vecteurs, matrices,
> samplers, images, subpass). Classez avec `NkSLTypeIsSampler/IsImage/IsMatrix` (`O(1)` par plage) —
> donc **ne réordonnez jamais l'enum**, et souvenez-vous que `IsMatrix` exclut les `DMAT*`.

---

## L'arbre syntaxique : `NkSLNode` et sa famille

Un shader analysé devient un **arbre** : `NkSLProgramNode` à la racine, ses déclarations globales en
enfants, et chaque fonction qui contient des statements, qui contiennent des expressions, etc. Tout
descend d'un nœud de base, `NkSLNode`, qui porte un `kind` (`NkSLNodeKind`), une position source
(`line`/`column`), un pointeur `parent`, et surtout un vecteur `children`. La spécialisation se fait
par héritage : `NkSLFunctionDeclNode`, `NkSLBinaryNode`, `NkSLIfNode`… sont des `struct : NkSLNode`
qui ajoutent leurs champs propres et fixent leur `kind` au constructeur.

L'**ownership** est le point névralgique, et il diffère du reste du moteur : l'AST utilise `new`/`delete`
**bruts** — *pas* les allocateurs NKMemory. La règle est « chaque nœud possède ses enfants » : le
destructeur `~NkSLNode()` fait `delete` sur chacun de `children`, donc détruire la racine détruit
récursivement tout l'arbre. La méthode `AddChild(n)` est le seul moyen propre d'attacher : elle fixe
`n->parent` *et* pousse `n` dans `children`, établissant le double lien.

```cpp
auto* fn  = new NkSLFunctionDeclNode();       // kind = NK_DECL_FUNCTION
fn->name  = "main";
fn->body  = new NkSLBlockNode();              // pointeur nommé…
fn->AddChild(fn->body);                       // …ET enfant, pour qu'il soit libéré
// delete fn;  -> libère récursivement body et tout le sous-arbre
```

Le piège qui en découle : les pointeurs **nommés** (`body`, `type`, `left`, `condition`…) ne sont
**pas** libérés par le destructeur — seul `children` l'est. Il faut donc placer chaque sous-nœud
référencé par pointeur nommé **aussi** dans `children` (via `AddChild`), mais **une seule fois** :
l'y mettre deux fois, ou l'avoir en nommé sans l'ajouter aux enfants, donne respectivement un
double-`delete` ou une fuite.

> **En résumé.** L'AST est un arbre de `NkSLNode` où la racine **possède** tout via `children` ;
> détruire la racine libère le reste. Ownership en `new`/`delete` **bruts** (pas NKMemory). Attachez
> toujours par `AddChild` (double lien parent/enfant) et veillez à ce que chaque sous-nœud nommé soit
> présent **exactement une fois** dans `children`.

---

## Les capacités par cible : `NkSLFeatureCaps`

Toutes les cibles ne savent pas tout faire. Le bindless n'existe pas en GLSL pur ; les mesh shaders et
le ray tracing demandent SPIR-V ou un DX12 récent ; Metal n'a **pas** de tessellation ni de geometry
shaders natifs. Plutôt que de parsemer le générateur de `if (target == …)`, NKSL centralise tout dans
`NkSLFeatureCaps` : une structure de booléens (`bindless`, `meshShaders`, `rayTracing`, `tessellation`,
`waveIntrinsics`, `pushConstants`…) produite par la fabrique `ForTarget(target, hlslSM)`.

```cpp
auto caps = NkSLFeatureCaps::ForTarget(NK_HLSL_DX12, 66);
if (caps.bindless)      { /* ResourceDescriptorHeap (SM6.6) */ }
if (!caps.tessellation) { /* MSL : émuler ou refuser */ }
```

La logique encapsulée est précieuse : pour `NK_HLSL_DX12`, les capacités **dépendent du shader model**
passé (wave ≥ SM6.0, ray tracing ≥ 6.3, mesh ≥ 6.5, bindless ≥ 6.6) ; pour MSL, tessellation et
geometry sont d'office à `false`. Demander une capacité au lieu de tester une cible, c'est rendre le
code de génération **déclaratif** et robuste aux évolutions.

> **En résumé.** `NkSLFeatureCaps` répond « cette cible sait-elle faire X ? » via des booléens, peuplés
> par `ForTarget(target, hlslSM)`. Interrogez les capacités plutôt que de tester la cible : DX12 dépend
> du shader model, MSL n'a ni tessellation ni geometry.

---

## Aperçu de l'API

Tous les éléments publics, par header. Le détail (sémantique, pièges, usages) suit dans la « Référence
complète ».

### `NkSLTypes.h` — enums et structs de base

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cible | `enum NkSLTarget` (`NK_GLSL`…`NK_BYTECODE`, `NK_COUNT`) | Dialecte de sortie (10 cibles + sentinelle). |
| Cible | alias `NK_HLSL` (= `NK_HLSL_DX11`) | Rétrocompat, ne pas employer en neuf. |
| Cible | `NkSLTargetName` | Libellé lisible pour les logs. |
| Cible | `NkSLTargetIsGLSL/IsHLSL/IsMSL/IsVulkan` | Familles de cibles (oui/non). |
| Étage | `enum NkSLStage` (bitmask : `NK_VERTEX`…`NK_TASK`, `NK_ALL_GRAPHICS`, `NK_ALL`) | Étage(s) de pipeline, **combinable**. |
| Étage | `operator\| & ~ \|= &=` | Algèbre du bitmask d'étages. |
| Étage | `NkSLStageHas`, `NkSLStageToString`, `NkSLStageName` | Test, libellé combiné, nom singulier exact. |
| Type | `enum NkSLBaseType` (scalaires, vecteurs, matrices, samplers, images, subpass, composés) | Vocabulaire de types du langage. |
| Type | `NkSLTypeIsSampler/IsImage/IsMatrix` | Classification `O(1)` par plage. |
| Qualif. | `enum NkSLStorageQual` (`NK_NONE`, `NK_IN`, `NK_OUT`, `NK_INOUT`, `NK_UNIFORM`, `NK_BUFFER`, `NK_PUSH_CONSTANT`, `NK_SHARED`, `NK_WORKGROUP`, `NK_INPUT_ATTACHMENT`) | Qualificateur de stockage. |
| Qualif. | `enum NkSLInterpolation` (`NK_SMOOTH`, `NK_FLAT`, `NK_NOPERSPECTIVE`) | Interpolation d'un varying. |
| Qualif. | `enum NkSLPrecision` (`NK_NONE`, `NK_LOWP`, `NK_MEDIUMP`, `NK_HIGHP`) | Précision (GLSL ES). |
| Binding | `struct NkSLBinding` (`set`, `binding`, `location`, `offset`, `inputAttachment`, `imageFormat`) | Métadonnées de placement. |
| Binding | `Has Binding/Location/Set/InputAttachment/ImageFormat` | Tests de présence. |
| Diagnostic | `struct NkSLCompileError` (`line`, `column`, `file`, `message`, `isFatal`) | Une erreur/un warning. |
| Résultat | `struct NkSLCompileResult` (`success`, `bytecode`, `source`, `errors`, `warnings`, `target`, `stage`) | Sortie d'une compilation. |
| Résultat | `IsText`, `GetSource`, `AddError`, `AddWarning` | Texte ? / source / collecte des diagnostics. |
| Options | `struct NkSLCompileOptions` (versions GLSL/Vulkan/HLSL SM/MSL, comportements, flags DX12/Vulkan, `entryPoint`) | Paramètres de compilation. |
| Reflection | `enum NkSLResourceKind` (`NK_UNIFORM_BUFFER`…`NK_INPUT_ATTACHMENT`) | Nature d'une ressource. |
| Reflection | `struct NkSLResourceBinding` (`name`, `kind`, `set`, `binding`, `location`, `stages`, `baseType`, `typeName`, `arraySize`, `sizeBytes`) | Descripteur de ressource. |
| Reflection | `struct NkSLVertexInput`, `struct NkSLStageOutput` | Entrées vertex / sorties d'étage. |
| Reflection | `struct NkSLReflection` (`resources`, `vertexInputs`, `stageOutputs`, `localSize X/Y/Z`) | Réflexion complète. |
| Reflection | `FindResource`, `FindBinding`, `GetUBOSize` | Recherches dans la réflexion. |

### `NkSLAST.h` — arbre syntaxique

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Kind | `enum NkSLNodeKind` (programme, déclarations, types, expressions, statements, annotations) | Étiquette du type de nœud. |
| Base | `struct NkSLNode` (`kind`, `line`, `column`, `children`, `parent`) | Nœud polymorphe de base. |
| Base | `NkSLNode(kind, ln, col)`, `~NkSLNode()`, `AddChild` | Ctor / destruction récursive / attache. |
| Type | `NkSLTypeNode` (`baseType`, `typeName`, `arraySize`, `isUnsized`) | Type feuille. |
| Annotation | `NkSLAnnotationNode(kind)` (`binding`, `builtinName`, `stage`, `entryPoint`) | Annotation `@binding`/`@stage`/`@entry`. |
| Déclaration | `NkSLVarDeclNode` (`name`, `type`, `initializer`, `storage`, `interp`, `precision`, `binding`, `isConst`, `isInvariant`, `isBuiltin`, `builtinName`) | Déclaration de variable. |
| Déclaration | `NkSLBlockDeclNode` (`blockName`, `instanceName`, `storage`, `binding`, `members`) | Bloc uniform/storage buffer. |
| Déclaration | `NkSLStructDeclNode` (`name`, `members`) | Déclaration de struct. |
| Déclaration | `NkSLParamNode` (`name`, `type`, `storage`, `precision`) | Paramètre de fonction. |
| Déclaration | `NkSLFunctionDeclNode` (`name`, `returnType`, `params`, `body`, `isEntry`, `stage`) | Déclaration de fonction. |
| Expression | `NkSLLiteralNode` (`baseType` + union `intVal`/`uintVal`/`floatVal`/`boolVal`) | Littéral. |
| Expression | `NkSLIdentNode` (`name`) | Identificateur. |
| Expression | `NkSLUnaryNode` (`op`, `prefix`, `operand`) | Opérateur unaire. |
| Expression | `NkSLBinaryNode` (`op`, `left`, `right`) | Opérateur binaire. |
| Expression | `NkSLCallNode` (`callee`, `calleeExpr`, `args`) | Appel / constructeur. |
| Expression | `NkSLMemberNode` (`object`, `member`) | Accès `a.b`. |
| Expression | `NkSLIndexNode` (`array`, `index`) | Accès `a[i]`. |
| Expression | `NkSLCastNode` (`targetType`, `expr`) | Conversion `type(expr)`. |
| Expression | `NkSLAssignNode` (`op`, `lhs`, `rhs`) | Affectation `=`, `+=`… |
| Statement | `NkSLBlockNode` (statements dans `children`) | Bloc `{ … }`. |
| Statement | `NkSLIfNode` (`condition`, `thenBranch`, `elseBranch`) | Conditionnelle. |
| Statement | `NkSLForNode` (`init`, `condition`, `increment`, `body`) | Boucle `for`. |
| Statement | `NkSLWhileNode` (`condition`, `body`) | Boucle `while`. |
| Statement | `NkSLReturnNode` (`value`) | Retour. |
| Racine | `NkSLProgramNode` (`filename`, `stage`, `localSize X/Y/Z`, globales dans `children`) | Racine du programme. |

### `NkSLFeatures.h` — capacités, stages avancés, générateurs

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Stages avancés | `constexpr NK_SL_STAGE_TASK/MESH/RAY_GEN/ANY_HIT/CLOSEST/MISS/INTERSECT/CALLABLE` | Indices numériques (≠ flags bitmask). |
| Capacités | `struct NkSLFeatureCaps` (16 booléens : `bindless`, `meshShaders`, `rayTracing`, `tessellation`, `geometryShaders`, `computeAtomics`, `waveIntrinsics`, `int64Ops`, `fp16Ops`, `variableRateShading`, `subpassInput`, `pushConstants`, `drawParams`, `specializationConst`, `rootSignature`, `bindlessHeapSM66`) | Ce que sait faire une cible. |
| Capacités | `ForTarget(target, hlslSM)`, `ForHLSL_DX12(shaderModel)` | Fabriques par cible. |
| Layouts | `struct NkSLMeshShaderLayout`, `struct NkSLTessLayout` | Layouts mesh / tessellation. |
| Configs | `struct NkSLRayTracingConfig`, `struct NkSLBindlessConfig` | Config ray tracing / bindless. |
| Générateurs | `GenerateMeshShaderHLSL/MSL`, `GenerateTessControlHLSL`, `GenerateTessEvalHLSL` | Émission mesh / tessellation. |
| Générateurs | `GenerateRayGenHLSL`, `GenerateClosestHitHLSL`, `GenerateMissHLSL`, `GenerateAnyHitHLSL` | Émission ray tracing. |
| Générateurs | `GenerateBindlessHLSL`, `GenerateBindlessMSL` | Émission préambule bindless. |

---

## Référence complète

Chaque élément est repris ici en détail, avec ses usages à travers les domaines du temps réel — rendu,
ECS, physique, animation, gameplay/IA, audio, UI/2D, IO, GPU, threading, outils/éditeur. Le module
étant un compilateur de shaders, beaucoup d'usages tournent autour du **GPU et du rendu**, mais l'AST
et les types servent aussi tout l'outillage (réflexion de pipeline, génération de matériaux par
l'éditeur, hot-reload).

### `NkSLTarget` — la cible de compilation

Les dix valeurs couvrent les API du moteur : `NK_GLSL` (OpenGL 4.30+, bindings aplatis car le concept
de `set` Vulkan n'existe pas), `NK_GLSL_VULKAN` (GLSL 4.50+ avec `layout(set=…, binding=…)`),
`NK_SPIRV` (binaire Vulkan), `NK_HLSL_DX11` (SM5, compilé par *fxc*), `NK_HLSL_DX12` (SM6+, compilé par
*dxc*), `NK_MSL` (Metal 2.0+ natif), `NK_MSL_SPIRV_CROSS` (Metal obtenu via SPIRV-Cross), `NK_CPLUSPLUS`
(rasterizer logiciel : on génère du C++), `NK_BYTECODE` (logiciel : un bytecode exécuté par NkSLVM).
`NK_COUNT` n'est pas une cible mais une **sentinelle** pour dimensionner un tableau indexé par cible.
L'alias `NK_HLSL` vaut `NK_HLSL_DX11` ; ne l'employez plus dans du code neuf.

- **Rendu / GPU** : c'est la valeur qui décide quel backend RHI consomme la sortie — `NK_SPIRV` pour
  Vulkan, `NK_HLSL_DX12` pour DirectX 12, etc. Un cache de shaders s'indexe naturellement par couple
  (cible, étage).
- **Outils / éditeur** : un panneau « exporter le shader » liste les cibles via `NkSLTargetName` ;
  un menu de prévisualisation bascule entre dialectes pour le même `.nksl`.
- **Threading** : compiler les dix cibles d'un même shader est *embarrassingly parallel* — une tâche
  par cible, chacune produit son `NkSLCompileResult`.

Les **familles** évitent les comparaisons fragiles : `NkSLTargetIsGLSL` (true pour `NK_GLSL` ou
`NK_GLSL_VULKAN`), `NkSLTargetIsHLSL` (DX11/DX12), `NkSLTargetIsMSL` (les deux Metal),
`NkSLTargetIsVulkan` (`NK_GLSL_VULKAN` *ou* `NK_SPIRV` — toutes deux suivent la convention de layout
Vulkan). On les utilise pour brancher le bon générateur ou la bonne convention de binding sans
énumérer chaque cas.

### `NkSLStage` — l'étage de shader (bitmask)

`NkSLStage` est l'**enum canonique et unique** des étages, partagé avec NKRHI (`using NkShaderStage =
NkSLStage;` — aucune duplication). Ses valeurs sont des **bits** : `NK_VERTEX` (1), `NK_FRAGMENT` (2,
le « pixel »), `NK_GEOMETRY`, `NK_TESS_CTRL` (avec l'alias `NK_TESS_CONTROL`), `NK_TESS_EVAL`,
`NK_COMPUTE`, `NK_MESH`, `NK_TASK`. Deux agrégats sont fournis : `NK_ALL_GRAPHICS` (les cinq étages
graphiques classiques) et `NK_ALL` (graphiques + compute + mesh + task).

- **Rendu** : un binding partagé entre plusieurs étages se note en une seule valeur — une UBO de
  caméra visible au vertex *et* au fragment est `NK_VERTEX | NK_FRAGMENT`. Réflexion et création de
  pipeline n'ont qu'un champ à lire.
- **GPU / compute** : `NK_COMPUTE` isole les passes de calcul (culling GPU, simulation de particules) ;
  `NK_MESH`/`NK_TASK` désignent le pipeline mesh shading moderne.
- **Outils** : afficher « ce shader couvre VERTEX|FRAGMENT » se fait directement avec `NkSLStageToString`.

Le maniement passe par les **opérateurs bitmask** (`|`, `&`, `~`, `|=`, `&=`) et trois fonctions
libres. `NkSLStageHas(flags, s)` est `true` dès qu'un bit est commun (le test d'appartenance courant).
`NkSLStageToString` décompose toute combinaison en `"VERTEX|FRAGMENT|…"` (ou `"NONE"` si vide), en
itérant sur huit bits — `O(1)`. **Piège** : `NkSLStageName` renvoie un nom singulier minuscule mais ne
gère qu'une **valeur exacte** par `switch` ; sur une combinaison il renvoie « unknown », et il ne couvre
**pas** MESH/TASK. Règle pratique : `NkSLStageName` pour un étage unique connu, `NkSLStageToString`
partout ailleurs.

### `NkSLBaseType` — les types du langage

Le type d'une valeur de shader. L'énumération suit GLSL : scalaires et vecteurs (`NK_BOOL`, `NK_INT`/
`NK_IVEC*`, `NK_UINT`/`NK_UVEC*`, `NK_FLOAT`/`NK_VEC*`, `NK_DOUBLE`/`NK_DVEC*`), matrices carrées et
rectangulaires (`NK_MAT2/3/4`, `NK_MAT2X3`…, `NK_DMAT2/3/4`), samplers (toute la gamme 2D/3D/cube/array/
shadow + variantes `i`/`u` + multisample), storage images (`NK_IMAGE*` et leurs `i`/`u`), entrées de
subpass Vulkan (`NK_SUBPASS_INPUT`, `NK_SUBPASS_INPUT_MS`), et les composés `NK_STRUCT`, `NK_ARRAY`,
`NK_UNKNOWN`. `NK_VOID` sert au type de retour des fonctions sans valeur.

- **Rendu** : `NK_VEC4` pour une couleur ou une position homogène, `NK_MAT4` pour les transformations,
  `NK_SAMPLER2D`/`NK_SAMPLER_CUBE` pour les textures, `NK_SAMPLER2D_SHADOW` pour le shadow mapping.
- **GPU / compute** : les `NK_IMAGE*` sont les storage images lues/écrites par un compute shader ;
  `NK_BUFFER` (via le qualificateur) porte les SSBO.
- **Vulkan** : `NK_SUBPASS_INPUT` modélise un attachement lu dans le subpass suivant (deferred,
  post-process intégré au render pass).

La **classification** est ce qui rend cet enum exploitable, et elle repose sur des **plages contiguës** :
`NkSLTypeIsSampler` teste `t >= NK_SAMPLER2D && t <= NK_USAMPLER_CUBE_ARRAY` ; `NkSLTypeIsImage` teste
`NK_IMAGE2D … NK_UIMAGE2D_ARRAY`. C'est `O(1)`, mais cela **interdit de réordonner l'enum** : déplacer
une valeur casse silencieusement les tests. `NkSLTypeIsMatrix` reconnaît `MAT2/3/4` plus les
rectangulaires `MAT2X3/2X4/3X2/3X4/4X2/4X3` — et **exclut** les `NK_DMAT*`, qu'il faut traiter à part
si l'on veut couvrir les matrices double.

### `NkSLStorageQual`, `NkSLInterpolation`, `NkSLPrecision` — les qualificateurs

`NkSLStorageQual` dit **où vit** une variable et comment elle circule : `NK_IN`/`NK_OUT`/`NK_INOUT`
(varyings et paramètres), `NK_UNIFORM` (UBO), `NK_BUFFER` (SSBO), `NK_PUSH_CONSTANT` (push constants
Vulkan), `NK_SHARED`/`NK_WORKGROUP` (mémoire partagée d'un groupe compute), `NK_INPUT_ATTACHMENT`
(subpassInput Vulkan), `NK_NONE` par défaut. C'est le qualificateur qui guide l'émission GLSL/HLSL/MSL
correcte (un `uniform` GLSL devient un `cbuffer` HLSL, etc.).

`NkSLInterpolation` (`NK_SMOOTH` par défaut, `NK_FLAT`, `NK_NOPERSPECTIVE`) contrôle comment un varying
est interpolé entre le vertex et le fragment — `NK_FLAT` pour un index d'instance ou une couleur par
facette, `NK_SMOOTH` pour une normale ou un UV. `NkSLPrecision` (`NK_LOWP`/`NK_MEDIUMP`/`NK_HIGHP`,
`NK_NONE` par défaut) compte surtout en GLSL ES (mobile, WebGL), où la précision déclarée influence
performance et qualité.

- **Rendu mobile / UI 2D** : sur GLES, marquer une couleur `NK_MEDIUMP` allège le fragment ; un UV en
  `NK_HIGHP` évite l'aliasing sur les grands atlas.
- **GPU / compute** : `NK_SHARED` est le tampon de réduction partagé d'un groupe (somme parallèle,
  histogramme), au cœur du threading sur GPU.

### `NkSLBinding` — le placement d'une ressource

Petite structure de métadonnées : `set`, `binding`, `location`, `offset`, `inputAttachment` (index du
subpass Vulkan), et `imageFormat` (le format GLSL d'une storage image en écriture : `"r32f"`, `"rgba8"`,
`"rgba16f"`… — vide si non applicable, **requis** pour les images écrites). Plutôt que d'interpréter des
sentinelles `-1` à la main, on interroge via `HasBinding()`, `HasLocation()`, `HasSet()`,
`HasInputAttachment()`, `HasImageFormat()`.

- **Rendu / GPU** : c'est l'information qui décide du slot d'une texture ou d'une UBO. La cible Vulkan
  utilise `set`+`binding`, la cible GLSL aplatit (d'où l'option d'aplatissement, voir Options) ; DX
  remappe vers ses registres.
- **Outils** : un validateur de pipeline détecte les collisions de `(set, binding)` en lisant ces
  champs sur toutes les ressources.

### `NkSLCompileError` et `NkSLCompileResult` — le résultat et les diagnostics

`NkSLCompileError` est un diagnostic ponctuel : `line`, `column`, `file`, `message`, et `isFatal`
(`true` par défaut — une erreur ; un warning a `isFatal=false`). `NkSLCompileResult` agrège **tout** ce
que produit une compilation : `success`, la sortie binaire `bytecode` (SPIR-V ou bytecode VM), la sortie
texte `source`, les vecteurs `errors`/`warnings`, et le couple (`target`, `stage`) compilé.

Les méthodes structurent l'usage. `IsText()` répond « la sortie est-elle du texte ? » en testant
`target != NK_SPIRV` — donc **toutes les cibles sauf SPIR-V sont textuelles**, y compris `NK_BYTECODE`
(considéré « text » par ce test, bien qu'il soit en réalité du binaire VM : à garder en tête).
`GetSource()` renvoie `source.CStr()`. `AddError(line, msg, fatal=true)` pousse dans `errors` (colonne 0,
fichier vide) et passe `success` à `false` si l'erreur est fatale ; `AddWarning(line, msg)` pousse un
diagnostic non-fatal.

- **Outils / éditeur** : un panneau de compilation lit `errors`/`warnings` pour les afficher avec
  numéro de ligne, et active le bouton « appliquer » selon `success`. Le hot-reload n'installe le
  nouveau shader que si `success`.
- **IO** : on sérialise `bytecode` (binaire) ou `source` (texte) selon `IsText()` pour écrire le cache
  disque.

### `NkSLCompileOptions` — les options de compilation

La structure de réglages, avec des **valeurs par défaut sensées**. Côté versions : `glslVersion=430`
(`#version 430 core`), `glslVulkanVersion=450`, `vulkanVersion=100` (SPIR-V 1.0), `glslEs=false`. Côté
HLSL, le shader model est codé ×10 : `hlslShaderModelDX11=50` (SM5.0), `hlslShaderModelDX12=60` (SM6.0),
plus un `hlslShaderModel=50` de compatibilité (→ DX11). MSL est codé ×100 : `mslVersion=200` (MSL 2.0).

Le **comportement** se règle par booléens : `debugInfo`, `optimize` (true), `strictMode` (true),
`flipUVY`, `invertDepth`, `glFlipYPosition` (GL : injecte `gl_Position.y = -gl_Position.y;` en fin de
`main` vertex — l'équivalent du `flip_vert_y` de SPIRV-Cross, à activer si la source est en convention
Vulkan), `flattenGLSLBindings` (true : pour `NK_GLSL`, aplatit `set=` vers `binding=`),
`preferSpirvCrossForMSL` (true). Spécifiques DX12 : `dx12InlineRootSignature` (`[RootSignature(...)]`),
`dx12DefaultSpace=0`, `dx12BindlessHeap` (SM6.6 `ResourceDescriptorHeap`). Spécifiques Vulkan GLSL :
`vkDrawParams` (`gl_BaseVertex`/`gl_BaseInstance`), `vkDebugPrintf` (`GL_EXT_debug_printf`). Enfin
`entryPoint="main"`.

- **Rendu cross-API** : `glFlipYPosition`, `flipUVY` et `invertDepth` réconcilient les conventions
  d'espace écran (Y bas/haut) et de profondeur (0..1 vs −1..1) entre OpenGL, Vulkan et DirectX — un
  réglage récurrent du moteur (voir les notes de session NKCanvas/NKRHI sur le Y-flip).
- **GPU avancé** : `dx12BindlessHeap` ou `vkDrawParams` activent des chemins matériels modernes ;
  `vkDebugPrintf` aide au débogage GPU.

### `NkSLResourceKind`, `NkSLResourceBinding`, `NkSLVertexInput`, `NkSLStageOutput`, `NkSLReflection` — la réflexion

La **réflexion** extrait de l'AST tout ce dont le moteur a besoin pour créer un pipeline sans relire le
texte. `NkSLResourceKind` classe une ressource : `NK_UNIFORM_BUFFER`, `NK_STORAGE_BUFFER`,
`NK_PUSH_CONSTANT`, `NK_SAMPLED_TEXTURE`, `NK_STORAGE_IMAGE`, `NK_SAMPLER`, `NK_INPUT_ATTACHMENT`.
`NkSLResourceBinding` décrit une ressource complète : `name`, `kind`, `set`/`binding`/`location`,
`stages` (les étages qui l'utilisent — d'où l'intérêt du bitmask), `baseType`/`typeName`, `arraySize`,
`sizeBytes`. `NkSLVertexInput` décrit une entrée vertex (`name`, `location`, `baseType`, `components`) et
`NkSLStageOutput` une sortie d'étage (`name`, `location`, `baseType`).

`NkSLReflection` rassemble le tout : `resources`, `vertexInputs`, `stageOutputs`, et la taille de groupe
compute (`localSizeX/Y/Z`, défaut 1). Trois méthodes de recherche : `FindResource(name)` (linéaire par
nom, `O(n)`, `nullptr` si absent), `FindBinding(set, binding)` (linéaire par slot, `O(n)`),
`GetUBOSize(name)` (les `sizeBytes` d'une ressource nommée, 0 si absente).

- **Rendu / GPU** : créer le descriptor set layout, le root signature ou le pipeline layout se fait
  **directement** à partir de `resources` ; le format du vertex buffer vient de `vertexInputs` ; la
  taille d'un UBO à allouer vient de `GetUBOSize`.
- **ECS / matériaux** : un système de matériaux mappe les propriétés d'un component sur les ressources
  réfléchies (trouver « uAlbedo » via `FindResource` et y écrire).
- **GPU / compute** : `localSizeX/Y/Z` donne la taille de groupe pour calculer le nombre de groupes à
  dispatcher (threading GPU).
- **Outils / éditeur** : un inspecteur de shader liste automatiquement les uniformes éditables à partir
  de la réflexion — la base d'un éditeur de matériaux.

### `NkSLNodeKind` et `NkSLNode` — le socle de l'AST

`NkSLNodeKind` étiquette chaque nœud par famille : programme (`NK_PROGRAM`, `NK_TRANSLATION_UNIT`),
déclarations (`NK_DECL_FUNCTION`, `NK_DECL_VAR`, `NK_DECL_STRUCT`, `NK_DECL_UNIFORM_BLOCK`,
`NK_DECL_STORAGE_BLOCK`, `NK_DECL_PUSH_CONSTANT`, `NK_DECL_INPUT/OUTPUT/PARAM`), types (`NK_TYPE_BASIC`,
`NK_TYPE_ARRAY`, `NK_TYPE_STRUCT_REF`), expressions (`NK_EXPR_LITERAL/IDENT/UNARY/BINARY/TERNARY/CALL/
INDEX/MEMBER/CAST/ASSIGN`), statements (`NK_STMT_BLOCK/EXPR/IF/FOR/WHILE/DO_WHILE/RETURN/BREAK/CONTINUE/
DISCARD/SWITCH/CASE`) et annotations (`NK_ANNOTATION_BINDING/LOCATION/BUILTIN/STAGE/ENTRY`).

`NkSLNode` est le **nœud de base polymorphe** : `kind`, `line`/`column` (position source pour les
messages d'erreur), `children` (les sous-nœuds **possédés**) et `parent` (lien remontant). Son
constructeur `NkSLNode(kind, ln, col)` fixe l'étiquette ; le destructeur **virtuel** `~NkSLNode()` fait
`delete` sur chaque enfant — c'est lui qui assure la **destruction récursive** de tout l'arbre depuis la
racine. `AddChild(n)` est l'unique attache propre : si `n` n'est pas nul, il fixe `n->parent = this` et
pousse `n` dans `children`.

**Ownership — le piège majeur, à répéter** : l'AST alloue en `new`/`delete` **bruts**, pas via NKMemory
(exception locale à la règle générale du moteur). La racine possède tout son sous-arbre par `children`.
Les sous-nœuds **nommés** (`body`, `type`, `left`…) ne sont *pas* libérés en propre par le destructeur :
il faut les avoir ajoutés à `children` (via `AddChild`) pour qu'ils le soient — **une seule fois**, sous
peine de double-`delete` (présent deux fois) ou de fuite (nommé mais absent des enfants).

- **Outils / compilation** : on parcourt l'arbre par `kind` (visiteur) pour générer du code, faire de
  l'analyse sémantique, ou réflechir les ressources. `line`/`column` alimentent les `NkSLCompileError`.
- **Éditeur** : un *graph* de matériau peut être sérialisé en AST puis re-généré vers toutes les cibles.

### Les nœuds dérivés — déclarations, expressions, statements

Tous héritent de `NkSLNode`, fixent un `kind` au constructeur, et ont un constructeur par défaut.

- **Types** — `NkSLTypeNode` (`NK_TYPE_BASIC`) est un type feuille : `baseType`, `typeName` (le nom du
  struct si `NK_STRUCT`), `arraySize` (0 = pas de tableau, `UINT32_MAX` = non borné), `isUnsized`.
- **Annotation** — `NkSLAnnotationNode` (kind passé au ctor, qui est requis : `NkSLAnnotationNode(k)`)
  porte `binding`, `builtinName`, `stage`, `entryPoint` : c'est la trace des annotations `@binding`,
  `@location`, `@builtin`, `@stage`, `@entry` du source NkSL.
- **Déclarations** — `NkSLVarDeclNode` (`NK_DECL_VAR`) modélise une variable complète : `name`, `type`
  (pointeur `NkSLTypeNode`), `initializer`, `storage`, `interp`, `precision`, `binding`, et les drapeaux
  `isConst`/`isInvariant`/`isBuiltin` (+ `builtinName`). `NkSLBlockDeclNode` (`NK_DECL_UNIFORM_BLOCK`)
  est un bloc UBO/SSBO : `blockName` (nom GLSL), `instanceName` (parfois vide), `storage` (`NK_UNIFORM`
  par défaut), `binding`, et le vecteur `members`. `NkSLStructDeclNode` (`name`, `members`) est une
  déclaration de struct. `NkSLParamNode` (`NK_DECL_PARAM`) est un paramètre : `name`, `type`, `storage`,
  `precision`. `NkSLFunctionDeclNode` (`NK_DECL_FUNCTION`) tient `name`, `returnType`, le vecteur
  `params`, `body` (un `NK_STMT_BLOCK`, ou `nullptr` pour un simple prototype), `isEntry` (le `@entry`)
  et `stage`.
- **Expressions** — `NkSLLiteralNode` porte `baseType` et une **union anonyme**
  `{ int64 intVal; uint64 uintVal; double floatVal; bool boolVal; }` : on lit le membre correspondant à
  `baseType` (piège union classique — aucun tag automatique au-delà de `baseType`). `NkSLIdentNode`
  (`name`) est un identificateur. `NkSLUnaryNode` (`op`, `prefix`, `operand`) couvre `-x`, `!x`, `++x`…
  `NkSLBinaryNode` (`op`, `left`, `right`) couvre `a+b`, `a<b`… `NkSLCallNode` (`callee`, `calleeExpr` si
  c'est une méthode, `args`) couvre appel de fonction **et** constructeur (`vec3(…)`). `NkSLMemberNode`
  (`object`, `member`) est l'accès `a.b` (et le swizzle `v.xyz`). `NkSLIndexNode` (`array`, `index`) est
  `a[i]`. `NkSLCastNode` (`targetType`, `expr`) est `type(expr)`. `NkSLAssignNode` (`op`, `lhs`, `rhs`)
  couvre `=`, `+=`, etc.
- **Statements** — `NkSLBlockNode` (`NK_STMT_BLOCK`) tient ses statements **dans `children`** (pas de
  champ dédié). `NkSLIfNode` (`condition`, `thenBranch`, `elseBranch`), `NkSLForNode` (`init`,
  `condition`, `increment`, `body`), `NkSLWhileNode` (`condition`, `body`), `NkSLReturnNode` (`value`).
- **Racine** — `NkSLProgramNode` (`NK_PROGRAM`) : `filename`, `stage`, les déclarations globales dans
  `children` (dans l'ordre source), et la taille de groupe compute `localSizeX/Y/Z` (issue de
  `layout(local_size_x/y/z=…) in;`, ignorée hors compute).

**Note de couverture importante** : plusieurs `NkSLNodeKind` n'ont **pas** de struct dédié dans le
header — `NK_DO_WHILE`, `NK_STMT_BREAK/CONTINUE/DISCARD/SWITCH/CASE`, `NK_EXPR_TERNARY`,
`NK_DECL_INPUT/OUTPUT/STORAGE_BLOCK/PUSH_CONSTANT`, `NK_TYPE_ARRAY/STRUCT_REF`, `NK_TRANSLATION_UNIT`.
Ils sont représentés par un `NkSLNode` de base (générique, avec le bon `kind`) ou par l'un des structs
ci-dessus avec le kind adéquat. Ne supposez donc pas qu'il existe toujours une sous-classe.

### Stages avancés — les `constexpr NK_SL_STAGE_*`

Distincts du bitmask `NkSLStage`, ce sont des **indices numériques** pour les étages avancés :
`NK_SL_STAGE_TASK=6`, `NK_SL_STAGE_MESH=7`, `NK_SL_STAGE_RAY_GEN=8`, `NK_SL_STAGE_ANY_HIT=9`,
`NK_SL_STAGE_CLOSEST=10`, `NK_SL_STAGE_MISS=11`, `NK_SL_STAGE_INTERSECT=12`, `NK_SL_STAGE_CALLABLE=13`.
On les emploie là où un **numéro** d'étage est attendu (table des shaders d'un pipeline ray tracing,
ordre de génération), à ne pas confondre avec les flags `NK_MESH`/`NK_TASK` qui, eux, sont des bits.

- **Rendu / GPU avancé** : indexer les étages d'un pipeline de ray tracing (ray gen, miss, closest/any
  hit, intersection, callable) ou de mesh shading (task → mesh).

### `NkSLFeatureCaps` — les capacités par cible

Seize booléens décrivent ce qu'une cible sait faire : `bindless`, `meshShaders`, `rayTracing`,
`tessellation` (true par défaut), `geometryShaders` (true), `computeAtomics` (true), `waveIntrinsics`,
`int64Ops`, `fp16Ops`, `variableRateShading`, `subpassInput`, `pushConstants`, `drawParams`,
`specializationConst`, `rootSignature`, `bindlessHeapSM66`. On ne les remplit pas à la main : la fabrique
`ForTarget(target, hlslSM = 50)` les déduit de la cible (et, pour DX12, du shader model). Détail du
comportement :

- `NK_GLSL` : tessellation, geometry, computeAtomics **on** ; tout le reste off (pas de bindless, mesh,
  RT, subpass, push constants, wave, draw params en GL classique).
- `NK_GLSL_VULKAN` : en plus, `subpassInput`, `pushConstants`, `drawParams`, `specializationConst` **on** ;
  wave/bindless/mesh/RT off (hors scope de ce chemin).
- `NK_SPIRV` : capacités Vulkan **complètes** — mesh, RT, bindless, wave, int64, fp16, subpass, push
  constants, draw params, spécialisation, tous on.
- `NK_HLSL_DX11` : tessellation, geometry, computeAtomics on, tout le reste off (limites SM5).
- `NK_HLSL_DX12` : **dépend de `hlslSM`** — `waveIntrinsics` (≥60), `rayTracing` (≥63), `meshShaders`
  (≥65), `bindless`/`bindlessHeapSM66` (≥66), `int64Ops` (≥60), `fp16Ops` (≥62), `variableRateShading`
  (≥64) ; `rootSignature` toujours true.
- `NK_MSL` / `NK_MSL_SPIRV_CROSS` : tessellation et geometryShaders **off** (non natifs Metal) ;
  computeAtomics, mesh, RT, bindless, wave, fp16 on.
- défaut (autres cibles) : capacités par défaut inchangées.

`ForHLSL_DX12(shaderModel)` est simplement un alias de `ForTarget(NK_HLSL_DX12, shaderModel)`.

- **Rendu / GPU** : décider si l'on emprunte le chemin mesh shading ou le pipeline classique, si l'on
  peut activer le bindless, ou s'il faut émuler la tessellation absente sur Metal — tout cela se lit ici.
- **Outils** : griser dans l'éditeur les fonctionnalités indisponibles pour la cible sélectionnée.

### Layouts et configs — `NkSLMeshShaderLayout`, `NkSLTessLayout`, `NkSLRayTracingConfig`, `NkSLBindlessConfig`

Ces structures paramètrent les générateurs avancés. `NkSLMeshShaderLayout` : `maxVertices=128`,
`maxPrimitives=128`, `topology="triangle"`, `groupSizeX=128`/`groupSizeY=1`/`groupSizeZ=1`.
`NkSLTessLayout` : `domain="tri"`, `partitioning="fractional_even"`, `outputTopology="triangle_cw"`,
`outputControlPoints=3`, `patchConstantFunc="PatchConstantFunc"`, `defaultTessFactor=4.0f`.
`NkSLRayTracingConfig` : `maxRecursionDepth=1`, `useInlineRT=false`. `NkSLBindlessConfig` :
`useSM66Heap=true`, `useArgumentBuffers=true`, `maxDescriptors=1000000`.

- **Rendu / GPU avancé** : ce sont les réglages d'un pipeline mesh shading (taille de groupe, limites de
  sortie), de tessellation (domaine, partitionnement, facteur), de ray tracing (profondeur de récursion,
  inline RT) ou de bindless (taille du heap, *argument buffers* Metal).

### Les générateurs — `Generate…`

Contrairement à tout le reste de ce header, ces fonctions libres sont **déclarées seulement** (définies
en `.cpp`) et retournent une `NkString`. Elles émettent le code des étages avancés à partir d'une source
et d'un layout/config :

- **Mesh shading** : `GenerateMeshShaderHLSL(meshEntrySource, layout, opts)` et `…MSL(…)`.
- **Tessellation** : `GenerateTessControlHLSL(source, layout, opts)` et `GenerateTessEvalHLSL(…)`.
- **Ray tracing** : `GenerateRayGenHLSL(src, opts)`, `GenerateClosestHitHLSL`, `GenerateMissHLSL`,
  `GenerateAnyHitHLSL`.
- **Bindless** : `GenerateBindlessHLSL(cfg)`, `GenerateBindlessMSL(cfg)` (le préambule de déclaration
  bindless).

Usage : un backend de rendu moderne (DX12, Metal) appelle le générateur correspondant à la fonctionnalité
demandée, après avoir vérifié via `NkSLFeatureCaps` que la cible la supporte.

### Le socle commun et les pièges transverses

- **Distinction fondamentale.** Ce module NKSL est le **vrai compilateur** (AST, types, features
  ci-dessus), à ne pas confondre avec le transpileur NkSL **ad-hoc** de NKRenderer (remplacement de
  texte sur `.nksl` annotés `@uniform`/`@target`).
- **`NkSLStage` est un bitmask.** Combinez avec `|`/`&`/`~` ; testez avec `NkSLStageHas` ; affichez une
  combinaison avec `NkSLStageToString` (`NkSLStageName` ne gère qu'une valeur exacte et ignore MESH/TASK).
- **L'ordre de `NkSLBaseType` est porteur de sens.** `NkSLTypeIsSampler`/`IsImage` reposent sur des
  plages contiguës — ne réordonnez jamais l'enum. `NkSLTypeIsMatrix` exclut les `DMAT*`.
- **Ownership de l'AST en `new`/`delete` bruts** (pas NKMemory). La racine possède tout via `children` ;
  attachez par `AddChild` ; un sous-nœud nommé doit être dans `children` exactement **une** fois.
- **`NkSLLiteralNode` est une union anonyme** : lisez le membre correspondant à `baseType`.
- **`NkSLCompileResult::IsText()`** considère `NK_BYTECODE` comme textuel : seul `NK_SPIRV` est binaire
  au sens de ce test.
- **`NkSLFeatureCaps::ForTarget`** : MSL désactive tessellation et geometry ; DX12 dépend du shader model
  passé.

---

### Exemple

```cpp
#include "NKSL/Core/NkSLTypes.h"
#include "NKSL/Core/NkSLAST.h"
#include "NKSL/Core/NkSLFeatures.h"
using namespace nkentseu;

// 1) Choisir cible + étage(s) (l'étage est un bitmask).
NkSLTarget target = NK_HLSL_DX12;
NkSLStage  used   = NK_VERTEX | NK_FRAGMENT;
if (NkSLTargetIsHLSL(target))
    NK_LOG_INFO("Cible : {}, étages : {}", NkSLTargetName(target), NkSLStageToString(used));

// 2) Construire un bout d'AST : main() { return; } — racine propriétaire.
auto* program = new NkSLProgramNode();
program->stage = NK_FRAGMENT;
auto* fn = new NkSLFunctionDeclNode();
fn->name    = "main";
fn->isEntry = true;
fn->stage   = NK_FRAGMENT;
fn->body    = new NkSLBlockNode();
fn->body->AddChild(new NkSLReturnNode());   // statement dans children
fn->AddChild(fn->body);                     // body nommé ET enfant -> sera libéré
program->AddChild(fn);
// delete program;  -> libère récursivement fn, body, le return, etc.

// 3) Demander les capacités de la cible avant d'emprunter un chemin avancé.
auto caps = NkSLFeatureCaps::ForTarget(target, 66);   // DX12 SM6.6
if (caps.bindless)      { /* ResourceDescriptorHeap */ }
if (!caps.tessellation) { /* émuler ou refuser */ }
```

---

[← Index NKSL](README.md) · [Récap NKSL](../NKSL.md) · [Couche Runtime](../README.md)
