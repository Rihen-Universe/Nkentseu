# ROADMAP — VM bytecode NkSL sur le backend software

> Objectif : exécuter **réellement** n'importe quel shader NkSL sur le rasterizer CPU (tous stages :
> vertex, fragment, **compute**), pour que **NKRenderer** rende son pipeline complet (PBR, ombres,
> post-process, bloom, tonemap) sur software — sans le fixed-function taillé.
>
> **Découverte clé** : la VM existe déjà dans NKSL (`CodeGen/Bytecode/` + `VM/`, cible `NK_BYTECODE`,
> sérialisation `.nkbc`). On **branche + étend**, on ne réécrit pas. Template fonctionnel :
> `Applications/Sandbox/src/DemoNkentseu/Base03/NkRHIDemoFullSL.cpp:1247-1330` (opt-in `NK_SW_VM=1`).
>
> **Contrainte** : tout le travail bytecode/VM est **software-only dans NKSL** — les backends GPU
> utilisent les codegen GLSL/HLSL/MSL/SPIRV, pas le bytecode. **Aucun impact GPU / NKRenderer autres backends.**

## ▶ ÉTAT 2026-07-05 : Phase A FAITE et VALIDÉE

Chemin VM générique câblé dans `NkSoftwareDevice::CreateShader` (opt-in `NK_SW_VM=1`, fallback
fixed-function). `swSource` plombé dans **`NkShaderLibrary::CompileVF`** (l.577/591) ET `::Recompile`
(software-only, ignoré GPU). Programmes bytecode heap dans `NkSWShader` (libérés au `DestroyShader`).

**Test `renderdemo --backend=sw --demo=2 NK_SW_VM=1`** : **1 shader compilé+installé via VM**, 7 en
fallback. Le compilateur bytecode **liste précisément les gaps** (→ priorise Phase E) :
- **E4 (le plus gros) — fonctions utilisateur** : `D_GGX`, `G_Smith`, `F_Schlick`, `F_SchlickR`,
  `NkTriplanarWeights/SampleRGBA/RGB/NormalUDN`, `NkComputeVoxelAO`, `SampleLight3DCookie/CubeCookie`,
  `SampleLightShadowEx`. Le codegen bytecode n'inline/n'appelle pas les fonctions NkSL définies.
- **Push constants** `@push` → `symbole inconnu: pc` (vertex).
- **Constantes globales** → `symbole inconnu: PI`.
- **Dérivées** `dFdx`/`dFdy` (délicat : différences finies sur la tuile).

Tous ces gaps = **codegen bytecode NKSL** (`CodeGen/Bytecode/NkSLCodeGenBytecode.cpp`), **software-only**
(les backends GPU utilisent GLSL/HLSL/SPIRV) → aucun impact GPU.

## ▶ ÉTAT 2026-07-05 (suite) : Phase B (multi-UBO) FAITE

Implémenté en 5 fichiers (additif, ne casse pas le mono-UBO) : `NkSLByteCode.h` (struct `NkSLUBOBlock`,
`NkSLByteProgram.uboBlocks`, `NkSLVMEnv.uboBlobs[8]`), `NkSLByteCodeIO` (v2 : sérialise uboBlocks),
`NkSLCodeGenBytecode` (index de bloc via `b->binding.set/binding`, `Sym.block`, `OP_LOAD_UNI.c=index`),
`NkSLVM` (lit `uboBlobs[in.c]`), `NkSoftwareDevice` (remplit `uboBlobs` via `SwGetUBOBytes(set,binding)`).
Build vert (NKSL + renderdemo), **aucune régression** (VM toujours 1 install + fallback). Les uniformes
sont maintenant résolus **par (set,binding)** → un shader 3D lira `viewProj`(set0) × `model`(set1) correctement.

**Prochaine prio = E4 (fonctions utilisateur)** : c'est CE gap qui bloque le PBR (D_GGX, F_Schlick…) et
limite les installs à 1/8. Le codegen bytecode doit inliner/appeler les fonctions NkSL définies. Puis
push constants (`pc`) + globals (`PI`).

## ▶ ÉTAT 2026-07-05 (suite) : Phase E4 (fonctions utilisateur) FAITE

**Inlining implémenté** dans `NkSLCodeGenBytecode` : registre `mFuncs` (fonctions hors entrée), `SymTable`
en scan inverse + `Push`/`Truncate` (scoping des params), inline au site d'appel `NK_EXPR_CALL`
(binder params → regs des args, générer le corps, `return`→ MOV résultat + saut fixé en fin d'inline via
pile `mInline`). Build vert. **Validé** : `D_GGX`, `F_Schlick`, `gridCoverage`, `NkTriplanarWeights`… ont
**disparu du diagnostic** (inlinées). L'install reste 1/8 car chaque shader a d'AUTRES gaps.

**Nouveaux blocages par ordre d'occurrence (diagnostic renderdemo) :**
1. **`pc` — push constants `@push`** (×25, LE #1) : traiter `@push` comme un bloc uniforme dans le codegen
   (byteOffsets) + brancher `NkSoftwareCommandBuffer::PushConstants` (actuellement no-op `(void)b`) pour
   fournir le blob à l'env VM. → nouvel item roadmap.
2. **`any()`** (×10) + autres builtins manquants → compléter `BuiltinOp`.
3. **globals** `PI`, `kCascadeFadeStart` (×6) : `const` déclarés hors fonction → collecter comme constantes.
4. **dérivées** `fwidth`/`dFdx`/`dFdy` (×7) : différences finies sur la tuile (délicat) OU stub (retour 0).
5. **`gl_VertexID`/`gl_InstanceID`** (×7) : built-ins vertex → fournir via l'env.

## ▶ ÉTAT 2026-07-05 (suite) : couverture langage — 7/8 shaders via VM !

Ajouté (tous **software-only NKSL**, zéro impact GPU) :
- **Push constants `@push`** : traité comme bloc uniforme marqué `set=0xFFFF` (codegen), device stocke le
  blob (`NkSoftwareCommandBuffer::PushConstants` → `SwSetPushConstants`), VM lit `uboBlobs[i]`. `pc` réglé.
- **Globals** (`PI`, seuils) : `const` hors fonction générés comme locaux au début du shader.
- **Dérivées** `dFdx`/`dFdy`/`fwidth` : stub (0 / ε, même count) — incalculables en VM par-pixel.
- **`any`/`all`** : opcodes `OP_ANY`/`OP_ALL` (réduction).
- **Comparaisons vectorielles** `lessThan`/`greaterThan`/… : opcodes `OP_CMP_*` (composante).
- **`isnan`/`isinf`** : stub → false.
- **`gl_VertexID`/`gl_InstanceID`** : opcodes `OP_LOAD_VID`/`IID` + `env.vertexID` (device passe `idx`).

**Résultat `renderdemo --backend=sw --demo=2 NK_SW_VM=1` : 1 → 7 installs VM / 8** (1 fallback).

**DERNIER blocage = le PBR** (frag L429) : `vec2[]( ... )` = **constructeur de tableau** (Poisson/PCF).
→ **Phase E3 (arrays)** : const arrays + `type[](…)` + indexation. C'est LE gap restant pour le PBR
(le shader clé). Puis vérifier le RENDU correct des 7 shaders installés (uniformes/samplers/géométrie).

**Validation visuelle (config directe `NK_SW_MINCFG=1`)** : la VM **rend la géométrie NKRenderer**
(cube 3D ombré + sol + ciel s'affichent — les 7 shaders VM s'exécutent). ✅ Géométrie correcte.
⚠️ **Couleurs approximatives** (sol rouge, ciel bleu vif ≠ GPU) : les shaders tournent mais la sortie
n'est pas pixel-perfect → correctness à affiner (résolution uniformes std140 exacte, samplers par
(set,binding), math lighting). Full config = noir (cibles HDR RGBA16F reçoivent 4 o/px + PBR fallback).

**Prochaines prios :**
1. **E3 (arrays)** → débloque le PBR (`vec2[]` Poisson/PCF) = le shader clé.
2. **Correctness couleurs** : vérifier std140 (offsets uniformes), câbler les **samplers par (set,binding)**
   (C1-C2, actuellement heuristique), lighting.
3. **HDR** : le rasterizer écrit 4 o/px → gérer les cibles RGBA16F (ou rendu direct RGBA8).
4. C (samplers multi-set) · D (compute).

## Synthèse

| Étape | Description | Statut | Phase | Effort | Prio |
|-------|-------------|:------:|:-----:|:------:|:----:|
| A1 | Plomber `swSource` (NkSL original) jusqu'au device (software target) | ⏳ | A | S | P0 |
| A2 | `CreateShader` : compiler `swSource`→`NK_BYTECODE`, deserialize, stocker les programmes | ⏳ | A | M | P0 |
| A3 | Installer `vertFn` VM (env inputs=vertex, uniforms, outputs=position+varyings) | ⏳ | A | M | P0 |
| A4 | Installer `fragFn` VM (env inputs=varyings, uniforms, samplers callbacks) | ⏳ | A | M | P0 |
| A5 | Opt-in `NK_SW_VM` + fallback fixed-function si échec compile/deserialize | ⏳ | A | S | P0 |
| A6 | Cache des programmes par shader (pas de recompilation par frame) | ⏳ | A | S | P1 |
| B1 | `NkSLVMSymbol` uniform : ajouter `set` + `binding` | ⏳ | B | S | P0 |
| B2 | `NkSLByteProgram` : uniforms groupés/table par `(set,binding)` | ⏳ | B | M | P0 |
| B3 | `NkSLVMEnv` : plusieurs blobs uniformes (par binding) ou callback `getUniform(set,binding)` | ⏳ | B | M | P0 |
| B4 | Codegen : chaque `@uniform N` block → binding ; accès membre → bon blob+byteOffset | ⏳ | B | L | P0 |
| B5 | VM `OP_LOAD_UNI` : sélectionner le blob par binding | ⏳ | B | M | P0 |
| B6 | Device : fournir les blobs via `SwGetUBOBytes(set,binding)` (déjà en place) | ⏳ | B | S | P0 |
| C1 | `NkSLVMSampler` : `set`+`binding` (pas juste index) | ⏳ | C | S | P1 |
| C2 | Callbacks `sampleTex` : résoudre texture par `(set,binding)` via `SwGetTexAt` | ⏳ | C | M | P1 |
| C3 | Sampling cubemaps / arrays / shadow atlas | ⏳ | C | L | P2 |
| D1 | `computeFn` via VM (dispatch par thread group, `gl_GlobalInvocationID`) | ⏳ | D | L | P1 |
| D2 | Storage buffers (SSBO) read/write dans l'env | ⏳ | D | L | P2 |
| D3 | Shared memory / barriers compute | ⏳ | D | L | P3 |
| E1 | Couverture builtins : vérifier + compléter (textureLod, matrices, etc.) | ⏳ | E | M | P1 |
| E2 | Control flow complet (if/for/while, break/continue) | ⏳ | E | M | P1 |
| E3 | Arrays / structs / const arrays (`lights[32]`) | ⏳ | E | L | P1 |
| E4 | Fonctions utilisateur (inline ou call stack) | ⏳ | E | L | P2 |
| F1 | Perf : la VM interprète par pixel (lente) — s'appuyer sur le tuilage multi-thread | ⏳ | F | M | P2 |
| F2 | Optimisations codegen (`bcOpts.optimize`) | ⏳ | F | M | P2 |
| G1 | Cache `.nkbc` disque (NkSLByteCodeIO) réutilisant `NkShaderCache` | ⏳ | G | M | P2 |
| H1 | Validation : `renderdemo --backend=sw --demo=2` config complète → image correcte | ⏳ | H | S | P0 |
| H2 | Comparer au GPU (`--backend=opengl`) | ⏳ | H | S | P1 |
| H3 | Non-régression fixed-function + démos existantes (`NkRHIDemoFull`, RT) | ⏳ | H | S | P0 |

Légende : ✅ livré · 🔶 partiel · ⏳ à venir · ❌ bug · 🚫 hors périmètre.

## Phase A — Chemin VM générique (wiring)
Brancher la VM existante dans `NkSoftwareDevice::CreateShader`, opt-in `NK_SW_VM`, en gardant le
fixed-function taillé comme défaut. Valide le pipeline VM sur un shader **mono-UBO** (ex. `Render2D`,
`PP_Tonemap`). Réf. template : `NkRHIDemoFullSL.cpp:1247-1330`.

## Phase B — Multi-UBO (LE gros morceau)
NKRenderer bind Camera(set0,b0) + Object(set1,b0) + Lights(set0,b1). La VM ne lit qu'un blob. Étendre
le bytecode pour taguer chaque uniform par `(set,binding)` et fournir plusieurs blobs. **Software-only
dans NKSL.** Sans ça, les shaders 3D (PBR : `viewProj`×`model`) ne peuvent pas résoudre leurs uniformes.

## Phase C — Samplers multi-set
NKRenderer : albedo(set2,b1), shadow atlas(set0,b4), IBL cubemaps(set0,b5-6), BRDF LUT(set0,b7).
Résoudre par `(set,binding)`. Cubemaps/arrays = sampling non trivial.

## Phase D — Compute
La VM doit aussi exécuter les **compute shaders** (dispatch par groupe de threads). NKRenderer/NKGen
en utilisent (voxel AO, clusters, etc.). Prévoir SSBO + `gl_GlobalInvocationID`.

## Phase E — Couverture du langage
S'assurer que le codegen + VM couvrent ce que NKRenderer utilise : arrays de lumières, structs,
boucles, fonctions, tous les builtins PBR (distributionGGX, fresnelSchlick via pow/mix/…).

## Phase F — Perf
Interprétation par pixel = lent (surtout Debug). S'appuyer sur le rasterizer tuilé multi-thread déjà
en place. Optimisations codegen. JIT = hors périmètre.

## Phase G — Cache
`.nkbc` sur disque (NkSLByteCodeIO existe) branché sur `NkShaderCache` (FNV-1a) → compile une seule
fois, charge le bytecode ensuite.

## Phase H — Validation
Cible finale : `renderdemo --backend=sw --demo=2` en config COMPLÈTE affiche une image correcte
(géométrie PBR + ombres + post-process), comparable au backend OpenGL. Zéro régression sur le
fixed-function, les démos `NkRHIDemoFull` et le ray-tracer BPR.
