# La machine virtuelle bytecode

> Couche **Runtime** · NKSL · Exécuter un shader NkSL **sans GPU** : le bytecode à registres
> `NkSLByteProgram`, son interpréteur `NkSLVM`, son environnement `NkSLVMEnv` et le format de fichier
> `.nkbc`.

Un shader, normalement, c'est l'affaire du GPU. Mais le rasterizer **Software** de Nkentseu, lui,
n'a pas de GPU : il dessine pixel par pixel sur le CPU. Comment, alors, exécuter le *même* shader
NkSL que les autres backends ? Historiquement, on l'écrivait à la main en C++, sous forme de lambdas
branchées dans `NkSWShaderBridge` — un travail manuel, fastidieux, et qui divergeait fatalement du
vrai shader. La VM bytecode existe pour **supprimer cette double écriture** : on compile le shader
NkSL une fois, vers une représentation intermédiaire compacte, et on l'**interprète** sur le CPU.

L'idée est exactement celle d'une machine virtuelle de langage (penser à la JVM, ou au bytecode
Lua) : l'AST NkSL est traduit en une suite d'**instructions** simples — charge ce registre,
multiplie ces deux-là, échantillonne cette texture — que `NkSLVM` exécute une par une. Ce n'est
**pas** un transpileur vers GLSL/HLSL (ça, c'est le rôle des générateurs de code des autres
backends) ; et ce n'est **pas** non plus le transpileur ad-hoc NkSL de NKRenderer : c'est le **vrai
compilateur/VM** du module NKSL, avec sa propre ISA et son propre interpréteur.

Trois choix structurants gouvernent tout le reste. D'abord, **tout est un vecteur de floats** : un
scalaire, une couleur, une matrice 4×4 partagent la même boîte `NkSLValue` (au plus 16 floats) ; un
`bool` ou un `int` y vit aussi, encodé en `0.0`/`1.0`. Ensuite, c'est une **machine à registres**,
pas à pile : chaque instruction nomme explicitement son registre de destination et ses sources, et
les sauts visent des **indices d'instruction** résolus à la compilation. Enfin, le dialogue avec
l'hôte (le device Software) se fait par des **tableaux plats de floats** : les entrées et sorties du
shader sont indexées par offset-composante, les uniforms lus par offset-octet dans un blob UBO.

- **Namespace** : `nkentseu` (tous les symboles)
- **Headers** : `#include "NKSL/VM/NkSLByteCode.h"` (ISA + structures + environnement),
  `#include "NKSL/VM/NkSLByteCodeIO.h"` (sérialisation `.nkbc`),
  `#include "NKSL/VM/NkSLVM.h"` (interpréteur)

---

## La valeur universelle : `NkSLValue`

Avant les instructions, il faut comprendre **ce sur quoi elles opèrent**. Dans un shader, on
manipule des scalaires, des `vec2`/`vec3`/`vec4`, des `mat3`, des `mat4` — autant de tailles
différentes. Plutôt que d'avoir un type par forme, la VM choisit une **boîte unique** : `NkSLValue`,
un tableau de 16 floats `v[16]` accompagné d'un compteur `count` qui dit combien sont valides.

La convention de remplissage est limpide : un **scalaire** a `count = 1` (seul `v[0]` compte) ; un
`vecN` a `count = N` ; une **mat3** occupe `count = 9` et une **mat4** `count = 16`, toutes deux
rangées **column-major** (colonne par colonne, comme NKMath et OpenGL/Vulkan). Les types entiers et
booléens n'ont pas de représentation à part : un `bool` vrai est le float `1.0`, un `int` est
simplement son float — les conversions « entières » passent donc par des opcodes comme `OP_FLOOR`.

La fabrique `NkSLValue::Scalar(x)` construit en `O(1)` un scalaire prêt à l'emploi
(`count = 1`, `v[0] = x`) — utile pour injecter une constante ou un résultat intermédiaire.

> **En résumé.** Une seule boîte pour toutes les valeurs : `v[16]` + `count`. scalaire→1, vecN→N,
> mat3→9, mat4→16 (column-major), bool/int→float (`0.0`/`1.0`). `Scalar(x)` pour fabriquer un
> scalaire.

---

## Le programme : `NkSLByteProgram`

Un `NkSLByteProgram` représente **un seul stage** compilé — un vertex shader *ou* un fragment shader.
C'est l'unité que le générateur `NkSLCodeGenBytecode` produit depuis l'AST, que l'on sérialise, et
que l'interpréteur exécute. Il rassemble tout ce qu'il faut pour tourner de façon autonome.

Au cœur, `code` est la suite d'instructions (`NkVector<NkSLInstr>`) et `constants` la table des
valeurs littérales (référencées par index depuis `OP_LOADK`). `regCount` annonce combien de registres
le programme utilise, pour que la VM dimensionne son banc de registres en une fois.

Autour, **les tables d'interface** décrivent comment parler au monde extérieur. `inputs` liste les
attributs (en vertex) ou les varyings (en fragment) ; `outputs` la position et les varyings (vertex)
ou la couleur de fragment (fragment) ; `uniforms` les membres de l'UBO ; `samplers` les textures.
`inputFloats` et `outputFloats` donnent les tailles totales des tableaux plats d'entrée et de sortie,
ce qui permet à l'hôte de les allouer sans parcourir les symboles.

La seule méthode, `IsValid()`, est volontairement minimaliste : elle renvoie `!code.Empty()` en
`O(1)`. C'est un garde-fou contre un programme vide, **pas** une vérification de cohérence des tables
ou des symboles — ne comptez pas dessus pour valider un bytecode douteux.

> **En résumé.** Un `NkSLByteProgram` = un stage autonome : `code` + `constants` + `regCount`, plus
> les tables d'interface `inputs`/`outputs`/`uniforms`/`samplers` et les tailles `inputFloats`/
> `outputFloats`. `IsValid()` ne teste que « le code est non vide ».

---

## L'environnement : `NkSLVMEnv`

Le programme dit *quoi* faire ; l'environnement dit *avec quoi*. `NkSLVMEnv` est la structure que
**l'hôte** (le device Software) remplit avant chaque invocation et relit après. C'est elle qui porte
tout l'état mutable — ce qui est la raison pour laquelle `NkSLVM` n'a, lui, aucun état.

Les trois pointeurs d'I/O suivent le contrat des tableaux plats : `inputs` (lecture seule, taille
`inputFloats`) et `outputs` (écriture, taille `outputFloats`) sont des floats indexés par
offset-composante ; `uniforms` est un **blob d'octets** (l'UBO) lu par byteOffset. Le champ `ctx`
est un pointeur opaque que l'hôte transmet tel quel aux callbacks texture.

Justement, les **callbacks texture** branchent la VM sur le sous-système de textures de l'hôte, car
la VM ne sait pas, par elle-même, lire une `NkSWTexture`. `sampleTex(ctx, sampler, u, v, lod, out[4])`
échantillonne une texture 2D et écrit RGBA ; `sampleShadow(ctx, sampler, u, v, z)` fait une
comparaison de profondeur et renvoie un scalaire ; `texSize(ctx, sampler, out[2])` rend les
dimensions. Si l'un est `nullptr`, l'opcode texture correspondant ne peut pas s'exécuter
correctement. Enfin, `discarded` est positionné à `true` quand le shader exécute `OP_DISCARD`.

> **En résumé.** `NkSLVMEnv` porte l'**état mutable** fourni par l'hôte : pointeurs `inputs`
> (floats), `outputs` (floats), `uniforms` (octets), `ctx` opaque, les trois callbacks
> `sampleTex`/`sampleShadow`/`texSize`, et le drapeau `discarded`. Un environnement par
> thread/invocation.

---

## L'instruction et l'ISA : `NkSLInstr` + `NkSLOp`

Chaque ligne de `code` est un `NkSLInstr` : un opcode `op` et un petit jeu d'opérandes au format
fixe. La convention est uniforme dans toute l'ISA : `a` est le registre **destination**, `b` et `c`
les registres **sources**, `imm` un entier qui sert tantôt d'index (constante, in/out/uniform,
sampler), tantôt de cible de saut, tantôt de masque de swizzle ; `aux` porte un **count** auxiliaire
(nombre de composantes d'une I/O, taille d'un `construct`…).

Les opcodes forment l'enum `NkSLOp` (numérotés à partir de `OP_NOP = 0`, terminé par la sentinelle
`OP_COUNT`). Ils se rangent en familles : chargement/déplacement, construction et accès aux
composantes, arithmétique, comparaisons/logique, builtins mathématiques, textures, et contrôle de
flux. La référence complète plus bas détaille chaque famille ; l'idée à retenir ici est que **chaque
instruction est minuscule et explicite** — pas de magie, pas de pile cachée.

> **En résumé.** `NkSLInstr` = `op` + opérandes au format fixe (`a` dest, `b`/`c` sources, `imm`
> index/saut/masque, `aux` count). L'ISA `NkSLOp` va de `OP_NOP` à `OP_COUNT` et couvre
> charge/move, construct/swizzle, arithmétique, comparaisons, builtins, textures et contrôle de flux.

---

## L'interpréteur : `NkSLVM`

`NkSLVM` est l'exécuteur, et il est d'une simplicité radicale : **une seule méthode statique**,
`Execute(prog, env)`. Pas de constructeur, pas de Create/Destroy, aucun état d'instance. Tout l'état
vit dans le `NkSLVMEnv` que vous lui passez — ce qui rend la classe thread-safe par construction (il
suffit d'un `env` par thread).

La granularité est fine : un appel à `Execute` traite **un seul** vertex ou **un seul** fragment.
C'est à l'hôte de boucler sur tous les sommets ou tous les pixels, en remplissant `env.inputs` et en
relisant `env.outputs` à chaque tour. La valeur de retour signale le **discard** : `false` si le
fragment a été rejeté par `OP_DISCARD` (et alors `env.discarded` est aussi à `true`), `true` sinon.

> **En résumé.** `NkSLVM::Execute(prog, env)` est statique, sans état, thread-safe ; elle exécute
> **un** vertex ou **un** fragment et renvoie `false` en cas de discard. L'hôte boucle et gère les
> tableaux d'`env`.

---

## La sérialisation : le format `.nkbc`

Un bytecode compilé n'a pas à l'être deux fois. Le format `.nkbc` fige un `NkSLByteProgram` sur
disque : magic `"NKBC"`, version, stage, puis les tables (code, constants, in/out/uniformes,
samplers), **toujours écrites en little-endian** (donc portables d'une plateforme à l'autre). Le
pipeline complet devient : `compile NkSL → NkSLByteProgram → Serialize → .nkbc`, puis au runtime
`.nkbc → Deserialize → NkSLByteProgram → NkSLVM::Execute` — **sans toolchain ni recompilation**.

Quatre fonctions libres couvrent les deux sens et les deux supports. `NkSLByteCodeSerialize` écrit le
blob dans un `NkVector<uint8>` fourni (dont l'allocateur NKMemory gère la mémoire) ;
`NkSLByteCodeDeserialize` lit un couple `(data, size)` et **rejette** un magic ou une version
invalides. Les raccourcis fichier `NkSLByteCodeSaveFile`/`NkSLByteCodeLoadFile` passent par
NKFileSystem et renvoient `false` en cas d'échec I/O.

> **En résumé.** `.nkbc` = magic `"NKBC"` + version + tables, little-endian portable. `Serialize`/
> `Deserialize` (mémoire) et `SaveFile`/`LoadFile` (fichier via NKFileSystem) ; le buffer de sortie
> est géré par le `NkVector` (NKMemory), jamais par le heap CRT.

---

## Aperçu de l'API

La liste de **tous** les éléments publics. Chacun est détaillé (rôle, cas d'usage) dans la
« Référence complète » qui suit.

### Structures de données (`NkSLByteCode.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Valeur | `NkSLValue` · `v[16]` · `count` | Boîte universelle ≤ mat4 (column-major). |
| Valeur | `static NkSLValue::Scalar(x)` | Fabrique un scalaire (`count=1`) `[O(1)]`. |
| Instruction | `NkSLInstr` · `op`,`a`,`b`,`c`,`imm`,`aux` | Une instruction (dest/sources/immédiat/count). |
| Symbole I/O | `NkSLVMSymbol` · `name`,`offset`,`count`,`location` | Entrée/sortie/uniforme (offset-composante ou octet). |
| Sampler | `NkSLVMSampler` · `name`,`index`,`isShadow` | Texture liée (slot de binding). |
| Programme | `NkSLByteProgram` | Un stage compilé (code + tables + tailles). |
| Programme | `.stage`,`.code`,`.constants`,`.regCount` | ISA, littéraux, banc de registres. |
| Programme | `.inputs`,`.outputs`,`.uniforms`,`.samplers` | Tables d'interface. |
| Programme | `.inputFloats`,`.outputFloats` | Tailles des tableaux plats. |
| Programme | `IsValid()` | `!code.Empty()` `[O(1)]`. |
| Environnement | `NkSLVMEnv` | État mutable fourni par l'hôte. |
| Environnement | `.inputs`,`.outputs`,`.uniforms`,`.ctx` | Tableaux plats + contexte opaque. |
| Environnement | `.sampleTex`,`.sampleShadow`,`.texSize` | Callbacks texture de l'hôte. |
| Environnement | `.discarded` | Posé par `OP_DISCARD`. |

### Opcodes (`enum class NkSLOp`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Charge/move | `OP_LOADK`,`OP_MOV` | `a = const[imm]` / `a = b`. |
| Charge/move | `OP_LOAD_IN`,`OP_LOAD_UNI`,`OP_STORE_OUT` | Lire inputs / uniforms / écrire outputs (count en `aux`). |
| Composantes | `OP_CONSTRUCT`,`OP_SWIZZLE` | Concaténer des registres / réordonner via masque. |
| Composantes | `OP_WRITE_COMP`,`OP_INDEX` | Écriture masquée / indexation (`b[c]`). |
| Arithmétique | `OP_ADD`,`OP_SUB`,`OP_MUL`,`OP_DIV`,`OP_NEG`,`OP_MOD` | Composante par composante (broadcast scalaire). |
| Arithmétique | `OP_MATMUL` | Produit matriciel (mat·mat, mat·vec, vec·mat). |
| Comparaison | `OP_LT`,`OP_GT`,`OP_LE`,`OP_GE`,`OP_EQ`,`OP_NE` | Comparaisons → scalaire 0/1. |
| Logique | `OP_AND`,`OP_OR`,`OP_NOT` | Booléen → scalaire 0/1. |
| Builtins | `OP_DOT`,`OP_CROSS`,`OP_NORMALIZE`,`OP_LENGTH`,`OP_DISTANCE` | Algèbre vectorielle. |
| Builtins | `OP_REFLECT`,`OP_REFRACT`,`OP_MIX`,`OP_CLAMP`,`OP_SATURATE` | Réflexion/réfraction, interpolation, bornage. |
| Builtins | `OP_STEP`,`OP_SMOOTHSTEP`,`OP_POW`,`OP_SQRT`,`OP_INVSQRT` | Seuils, puissance, racines. |
| Builtins | `OP_ABS`,`OP_SIGN`,`OP_FLOOR`,`OP_CEIL`,`OP_FRACT`,`OP_MODF` | Valeur absolue, signe, arrondis, partie fractionnaire. |
| Builtins | `OP_MIN`,`OP_MAX`,`OP_EXP`,`OP_LOG`,`OP_EXP2`,`OP_LOG2` | Extrema, exponentielles, logarithmes. |
| Builtins | `OP_SIN`,`OP_COS`,`OP_TAN`,`OP_ASIN`,`OP_ACOS`,`OP_ATAN` | Trigonométrie. |
| Builtins | `OP_RADIANS`,`OP_DEGREES`,`OP_TRANSPOSE`,`OP_INVERSE` | Angles, transposée, inverse matricielle. |
| Textures | `OP_TEX_SAMPLE`,`OP_TEX_SAMPLE_SHADOW`,`OP_TEX_SIZE` | Échantillonner 2D / shadow / taille (sampler = `imm`). |
| Flux | `OP_JMP`,`OP_JZ`,`OP_JNZ` | Saut inconditionnel / si `b.x==0` / si `b.x!=0`. |
| Flux | `OP_DISCARD`,`OP_RET`,`OP_COUNT` | Rejeter fragment / fin / sentinelle. |

### Masque de swizzle (free functions inline, `NkSLByteCode.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Swizzle | `NkSLMakeSwizzle(a,b,c,d)` | Encode count + indices (x/y/z/w, `0xFF`=absente) `[O(1)]`. |
| Swizzle | `NkSLSwizzleCount(mask)` | Nombre de composantes du masque `[O(1)]`. |
| Swizzle | `NkSLSwizzleIdx(mask, i)` | i-ème indice du masque `[O(1)]`. |

### Sérialisation (`NkSLByteCodeIO.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Mémoire | `NkSLByteCodeSerialize(prog, out)` | Programme → blob `.nkbc` (buffer géré par `out`). |
| Mémoire | `NkSLByteCodeDeserialize(data, size, prog)` | Blob → programme (false si magic/version invalide). |
| Fichier | `NkSLByteCodeSaveFile(prog, path)` | Raccourci disque via NKFileSystem. |
| Fichier | `NkSLByteCodeLoadFile(path, prog)` | Raccourci disque via NKFileSystem. |

### Interpréteur (`NkSLVM.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Exécution | `static NkSLVM::Execute(prog, env)` | Exécute un vertex/fragment ; `false` si discard. |

---

## Référence complète

Chaque élément est repris ici en détail, avec ses usages dans les différents domaines du temps réel
— rendu, ECS, physique, animation, gameplay/IA, audio, UI/2D, IO, GPU, threading, outils/éditeur.

### `NkSLValue` — la valeur universelle

`NkSLValue` est le quantum de calcul de la VM : `v[16]` + `count`. Tout y rentre car la plus grosse
valeur d'un shader est une `mat4` (16 floats). Le `count` discrimine la forme — et c'est lui que
lisent les opérations pour savoir si elles travaillent sur un scalaire (broadcast), un vecteur
(composante par composante) ou une matrice.

- **Rendu** — une `NkSLValue` de `count = 16` porte la `MVP` column-major qui projette un sommet ;
  une de `count = 4` porte la couleur de fragment finale ; une de `count = 1` porte un facteur
  d'éclairage `N·L`.
- **Animation** — un poids de skinning ou un paramètre d'interpolation `t` vit en scalaire ; une
  matrice de bone en `count = 16`.
- **Physique / collision** — quand un shader de debug visualise une normale de contact ou une
  vitesse, c'est une `NkSLValue` `count = 3`.
- **UI / 2D** — couleur (`count = 4`), coordonnée de texture (`count = 2`), épaisseur de bordure
  (scalaire) cohabitent dans la même boîte.
- **GPU (parité)** — le column-major garantit que le résultat CPU correspond bit pour bit
  conceptuellement à ce que produiraient GL/VK/DX sur la même `mat4`.

`NkSLValue::Scalar(x)` est la fabrique `O(1)` du cas le plus fréquent : un scalaire isolé (`count=1`,
`v[0]=x`). On l'emploie pour injecter une constante, un seuil, ou « envelopper » un float intermédiaire
avant de le ranger dans un registre.

### `NkSLInstr` — l'instruction

Une instruction est un enregistrement plat de six champs. `op` choisit l'opération ; `a` est toujours
le registre **écrit** ; `b`/`c` les registres **lus** ; `imm` un entier polyvalent (index de
constante, d'I/O ou de sampler, cible de saut, masque de swizzle) ; `aux` un compteur de composantes.
Cette régularité est ce qui rend l'interpréteur petit et l'émission de code mécanique côté
`NkSLCodeGenBytecode`.

- **Outils / éditeur** — un désassembleur de `.nkbc` lit directement `op` + `a`/`b`/`c`/`imm`/`aux`
  pour afficher le shader instruction par instruction (debug, inspection de bytecode).
- **Threading** — comme l'instruction est une donnée pure (pas de pointeur, pas d'état), le banc de
  `code` est trivialement partageable en lecture entre threads de rasterization.

### `NkSLByteProgram` — le programme d'un stage

C'est le conteneur autonome d'un stage. `stage` dit s'il s'agit d'un vertex ou d'un fragment ; `code`
et `constants` portent les instructions et leurs littéraux ; `regCount` dimensionne le banc de
registres en une allocation. Les **tables d'interface** — `inputs`, `outputs`, `uniforms`, `samplers`
— décrivent le contrat avec l'hôte, et `inputFloats`/`outputFloats` permettent d'allouer les tableaux
plats sans parcourir les symboles.

- **Rendu** — le device Software charge **deux** `NkSLByteProgram` (un par stage) pour un pipeline :
  le vertex remplit `outputs` (position + varyings), le fragment lit ces varyings comme `inputs` et
  écrit la couleur.
- **GPU (pont)** — `uniforms` reflète exactement le layout d'un UBO côté GPU, ce qui permet de
  partager le même blob de constantes entre le backend Software et un backend matériel.
- **Outils / éditeur** — un *shader hot-reload* recompile le `.nksl`, régénère le `NkSLByteProgram`
  et swappe la VM sans toucher au reste du pipeline.

`IsValid()` est un garde minimal (`!code.Empty()`) : il attrape un programme vide, mais ne dit rien
de la cohérence des symboles — la validation profonde reste à la charge du générateur.

### `NkSLVMSymbol` et `NkSLVMSampler` — décrire l'interface

`NkSLVMSymbol` décrit **un** point d'I/O par son `name`, son `offset`, son `count` et sa `location`.
Le piège tient dans l'unité de l'`offset` : pour les `inputs`/`outputs`, c'est un offset en
**floats** (combiné au `count`) dans le tableau plat ; pour les `uniforms`, c'est un offset en
**octets** dans le blob UBO. La `location` reflète l'ordre déclaré (utile pour aligner attributs de
vertex et varyings).

`NkSLVMSampler` lie une texture par son `name`, son `index` (le slot de binding, identique à la
réflexion) et un drapeau `isShadow` qui distingue une texture de comparaison de profondeur d'une
texture couleur ordinaire.

- **Rendu** — l'hôte lit `inputs`/`outputs` pour empaqueter les varyings entre vertex et fragment ;
  `samplers[i].index` route vers la bonne `NkSWTexture` ; `isShadow` choisit entre `sampleTex` et
  `sampleShadow`.
- **ECS / scène** — un système qui pousse des constantes par instance écrit dans le blob `uniforms`
  aux byteOffsets donnés par les `NkSLVMSymbol` uniformes.
- **Outils** — un inspecteur de matériau liste les `samplers` et `uniforms` d'un programme pour
  exposer ses paramètres dans l'UI de l'éditeur.

### `NkSLVMEnv` — l'environnement d'exécution

`NkSLVMEnv` est le seul porteur d'état mutable de toute la VM. Ses pointeurs `inputs` (floats, lecture),
`outputs` (floats, écriture) et `uniforms` (octets) matérialisent le contrat I/O ; `ctx` est le
contexte opaque relayé aux callbacks. Les callbacks `sampleTex`/`sampleShadow`/`texSize` sont le pont
vers le sous-système de textures de l'hôte, et `discarded` rapporte un éventuel `OP_DISCARD`.

- **Rendu** — pour chaque pixel, le rasterizer interpole les varyings dans `inputs`, branche les
  callbacks sur ses `NkSWTexture` (avec `ctx` = device), appelle `Execute`, puis lit la couleur dans
  `outputs`.
- **Threading** — chaque thread de rasterization possède **son** `NkSLVMEnv` (et ses buffers
  `inputs`/`outputs`) ; la VM, sans état, est alors utilisée en parallèle sans verrou.
- **GPU (callbacks)** — `sampleShadow` implémente côté CPU le PCF/compare-sampler que le GPU ferait
  en matériel ; si un callback manque (`nullptr`), l'opcode texture ne peut pas produire de résultat
  correct — c'est un point à vérifier au branchement.

### Opcodes — l'ISA en détail

**Chargement et déplacement.** `OP_LOADK` charge `const[imm]` dans `a` ; `OP_MOV` copie `b` dans `a`.
`OP_LOAD_IN` lit `count` floats (dans `aux`) du tableau `inputs` à partir de l'index `imm` ;
`OP_LOAD_UNI` lit depuis le blob `uniforms` au **byteOffset** `imm` ; `OP_STORE_OUT` écrit `b` dans
`outputs` à partir de `imm` (count dans `aux`). Ce sont les seules portes entre les registres et
l'environnement.

**Construction et composantes.** `OP_CONSTRUCT` assemble un vecteur de `aux` composantes en
concaténant `imm` registres à partir de `b` (par exemple bâtir un `vec4` depuis un `vec3` + un
scalaire). `OP_SWIZZLE` réordonne les composantes de `b` selon le masque `imm` (`.xyz`, `.bgra`…),
`aux` donnant le count cible. `OP_WRITE_COMP` fait l'assignation masquée inverse (`a.xy = b`).
`OP_INDEX` indexe `b` par l'entier `c` — extrait une colonne d'une matrice (en `vecN`) ou une
composante.

**Arithmétique.** `OP_ADD`/`OP_SUB`/`OP_MUL`/`OP_DIV`/`OP_MOD` opèrent **composante par composante**,
avec broadcast d'un opérande scalaire (multiplier un `vec3` par un float) ; `OP_NEG` nie. `OP_MATMUL`
est à part : c'est le **vrai produit matriciel** (mat·mat, mat·vec ou vec·mat selon les counts), au
cœur de la transformation des sommets en rendu et du skinning en animation.

**Comparaisons et logique.** `OP_LT`/`OP_GT`/`OP_LE`/`OP_GE`/`OP_EQ`/`OP_NE` rendent un **scalaire
0/1**, de même que `OP_AND`/`OP_OR`/`OP_NOT`. Combinés aux sauts, ils implémentent les `if`/`else` du
shader (seuils d'alpha-test, branches de matériau, conditions de gameplay visualisées).

**Builtins mathématiques.** L'ISA embarque la bibliothèque standard d'un shader. Algèbre vectorielle :
`OP_DOT` (éclairage `N·L`, culling, cônes de vision IA), `OP_CROSS` (normale de triangle, repère
caméra, couple en physique), `OP_NORMALIZE`/`OP_LENGTH`/`OP_DISTANCE` (directions, portées).
Interpolation et bornage : `OP_MIX` (fondus, blend de matériaux), `OP_CLAMP`/`OP_SATURATE`,
`OP_STEP`/`OP_SMOOTHSTEP` (transitions douces, masques de contour UI). Réflexion : `OP_REFLECT`/
`OP_REFRACT` (spéculaire, eau, verre). Puissances et arrondis : `OP_POW` (gamma, spéculaire Phong),
`OP_SQRT`/`OP_INVSQRT`, `OP_ABS`/`OP_SIGN`/`OP_FLOOR`/`OP_CEIL`/`OP_FRACT`/`OP_MODF` (motifs de
damier, quantification, tilings). Extrema et exponentielles : `OP_MIN`/`OP_MAX`, `OP_EXP`/`OP_LOG`/
`OP_EXP2`/`OP_LOG2` (atténuation, courbes de ton, audio en visualisation logarithmique).
Trigonométrie : `OP_SIN`/`OP_COS`/`OP_TAN`/`OP_ASIN`/`OP_ACOS`/`OP_ATAN` (ondes, oscillations
d'animation, déformations procédurales). Conversions et matrices : `OP_RADIANS`/`OP_DEGREES`,
`OP_TRANSPOSE`/`OP_INVERSE` (matrice normale, passage d'espaces).

**Textures.** Le sampler est désigné par `imm`. `OP_TEX_SAMPLE` échantillonne une texture 2D en
`b.xy` (avec un lod optionnel en `c.x`) — diffuse, normal maps, atlas UI, frames de caméra.
`OP_TEX_SAMPLE_SHADOW` fait une comparaison de profondeur (`b.xy` = uv, `b.z` = valeur comparée) et
rend un scalaire de visibilité — le cœur du shadow mapping CPU. `OP_TEX_SIZE` rend la taille en
`vec2` (calcul de texel size pour le filtrage, le PCF, l'edge-detect).

**Contrôle de flux.** `OP_JMP` saute inconditionnellement à `imm` ; `OP_JZ` saute si `b.x == 0`,
`OP_JNZ` si `b.x != 0` — les briques des boucles et conditions, dont les cibles sont des **indices
d'instruction** résolus à la génération. `OP_DISCARD` rejette le fragment (et pose `env.discarded`) ;
`OP_RET` termine le shader ; `OP_COUNT` n'est pas un opcode exécutable mais la **sentinelle** qui
compte les opcodes (utile pour dimensionner une table de dispatch).

### Le masque de swizzle — `NkSLMakeSwizzle`, `NkSLSwizzleCount`, `NkSLSwizzleIdx`

Le swizzle (`.xyz`, `.bgr`, `.wzyx`…) est encodé dans un seul `int32`, ce qui permet de le ranger
dans le champ `imm` d'une instruction. Le format : quatre composantes sur 2 bits chacune (0=x, 1=y,
2=z, 3=w), plus le **count** dans les bits hauts ; la valeur `0xFF` marque une composante absente.

`NkSLMakeSwizzle(a, b, c, d)` encode le masque en `O(1)` : il déduit le count du premier `0xFF`
rencontré et compose `m = (a&3) | ((b&3)<<2) | ((c&3)<<4) | ((d&3)<<6) | (n<<8)`. À la lecture,
`NkSLSwizzleCount(mask)` extrait le count (`(mask>>8)&7`) et `NkSLSwizzleIdx(mask, i)` la i-ème
composante (`(mask>>(i*2))&3`).

- **Génération de code** — `NkSLCodeGenBytecode` appelle `NkSLMakeSwizzle` en émettant un `OP_SWIZZLE`
  ou un `OP_WRITE_COMP` à partir d'une expression `.rgb`/`.xy` de l'AST.
- **Interpréteur** — l'exécution d'un `OP_SWIZZLE` relit le masque avec `Count`/`Idx` pour recopier
  les bonnes composantes ; usages typiques : convertir RGBA↔BGRA (UI/2D), extraire `.xyz` d'un `vec4`
  homogène (rendu), réordonner un vecteur de normale.
- **Outils** — un désassembleur décode `imm` via ces helpers pour réafficher le swizzle lisible
  (`.zyx`) dans un dump de bytecode.

### Sérialisation `.nkbc`

`NkSLByteCodeSerialize(prog, out)` écrit le programme dans le `NkVector<uint8>` `out` (alloué par
NKMemory) et renvoie `true` si tout s'est bien passé ; le buffer appartient au vecteur, donc on ne le
libère **jamais** avec le heap CRT. `NkSLByteCodeDeserialize(data, size, prog)` reconstruit un
programme depuis un blob et renvoie `false` si le magic `"NKBC"` ou la version ne correspondent pas —
la première ligne de défense contre un fichier corrompu ou d'une autre version. Les raccourcis
`NkSLByteCodeSaveFile(prog, path)` et `NkSLByteCodeLoadFile(path, prog)` font la même chose vers/depuis
un fichier via NKFileSystem (multi-plateforme, y compris assets Android).

- **IO / build** — un précompilateur de shaders sérialise tous les `.nksl` du projet en `.nkbc` à la
  compilation ; au runtime, le device Software charge directement les `.nkbc`, sans embarquer de
  compilateur NkSL.
- **Outils / éditeur** — un cache de shaders persiste les `.nkbc` à côté des sources, et ne recompile
  que si le `.nksl` a changé (le little-endian rend le cache partageable entre machines).
- **Réseau / déploiement** — un blob `.nkbc` peut être transmis tel quel (format binaire portable) à
  une console ou un client distant qui ne dispose pas de la toolchain.

### `NkSLVM::Execute` — l'exécution

`Execute(prog, env)` est l'unique point d'entrée. Étant statique et sans état, elle est sûre à appeler
depuis plusieurs threads tant que chacun a son `env`. Elle traite **une** invocation (un vertex ou un
fragment), lit les entrées dans `env.inputs`, écrit les sorties dans `env.outputs`, et renvoie `false`
si le shader a fait `OP_DISCARD` (auquel cas `env.discarded` vaut aussi `true`).

- **Rendu** — la boucle de rasterization appelle `Execute` une fois par sommet (étape vertex) puis
  une fois par pixel couvert (étape fragment), en testant le retour pour sauter l'écriture des pixels
  rejetés.
- **Threading** — un `ThreadPool` partage le `prog` (immuable, lecture seule) et donne à chaque thread
  son `NkSLVMEnv` ; la rasterization se parallélise par tuiles sans verrou sur la VM.
- **Outils / debug** — exécuter `Execute` sur un seul fragment avec des `inputs` fabriqués à la main
  permet de tester un shader unitairement, hors du pipeline de rendu complet.

---

### Exemple

```cpp
#include "NKSL/VM/NkSLByteCode.h"
#include "NKSL/VM/NkSLByteCodeIO.h"
#include "NKSL/VM/NkSLVM.h"
using namespace nkentseu;

// 1) Charger un fragment shader précompilé (.nkbc), sans toolchain.
NkSLByteProgram frag;
if (!NkSLByteCodeLoadFile("shaders/lit.frag.nkbc", frag) || !frag.IsValid())
    return; // magic/version invalide, ou code vide

// 2) Préparer l'environnement (un par thread/invocation).
NkSLVMEnv env;
env.inputs   = varyings;          // floats interpolés du vertex (offset-composante)
env.outputs  = fragColorOut;      // taille = frag.outputFloats
env.uniforms = uboBlob;           // blob d'octets (lu par byteOffset)
env.ctx      = swDevice;          // contexte opaque passé aux callbacks
env.sampleTex = &SwDevice::SampleTexture;   // branché sur NkSWTexture

// 3) Exécuter UN fragment ; tester le discard.
if (!NkSLVM::Execute(frag, env))
    return; // fragment rejeté (OP_DISCARD) — ne pas écrire le pixel

// fragColorOut contient maintenant la couleur RGBA calculée.

// --- Construire un swizzle .xyz à l'émission de bytecode ---
int32 mask = NkSLMakeSwizzle(0, 1, 2);          // .xyz (count = 3)
uint32 n   = NkSLSwizzleCount(mask);            // 3
uint8 c0   = NkSLSwizzleIdx(mask, 0);           // 0 (= x)
```

---

[← Index NKSL](README.md) · [Récap NKSL](../NKSL.md) · [Couche Runtime](../README.md)
