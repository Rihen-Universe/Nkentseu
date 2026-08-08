# Étape 4 — Quantification 4 bits + LoRA (QLoRA) : étude de conception

> Étude préalable à l'implémentation de l'étape 4 du plan
> `D:\Projets\Camrail\AI\05_MISE_A_NIVEAU_STACK.md` : affiner localement
> Qwen2.5 7B Instruct (GGUF Q4_K_M, blob Ollama déjà présent) sur la
> RTX 3070 Laptop 8 Go, avec le corpus SFT
> `D:\Projets\Camrail\AI\BulkGen\dlg_ollama_fr.txt` (100 017 paires
> « Question:/Reponse: », 25,4 Mo, ≈ 6-7 M de tokens).
>
> Document d'ANALYSE (lecture seule du dépôt, aucune source modifiée).
> Toutes les affirmations sont sourcées `chemin:ligne`.

---

## 1. La stack telle qu'elle est (état vérifié)

### 1.1 NKTensor — tenseurs, dtypes, dispatch GPU

- **Types** : `NkDType` = F32/F64/I32/I64/U8/**F16** —
  `Kernel/AI/NKTensor/src/NKTensor/NkDType.h:16-24`. Pas de bf16, pas d'int8,
  **aucun type quantifié par blocs**. Le dispatch dtype est un macro
  `NK_DTYPE_DISPATCH(_FLOAT)` (`NkDType.h:72-124`).
- **F16** : purement LOGICIEL CPU (`NkFp16.h`, conversions bit-exactes RNE,
  « brique 1/3 » — `NkFp16.h:2-15`). Le calcul f16 passe par un aller-retour
  float32 ; **aucun noyau GPU f16 n'existe**.
- **Tenseur** : `NkTensor` = stockage refcompté partagé + shape + strides (en
  ÉLÉMENTS) + dtype + device, row-major C-order
  (`NkTensor.h:1-10,30-39,152-158`). **La copie est une VUE partagée**
  (incref), la copie profonde est `Clone()` (`NkTensor.h:49-51`).
  `ToGPU()/ToCPU()` transfèrent via un handle opaque `gpuBuffer` (uint64)
  (`NkTensor.h:137-142`, `NkTensorStorage::gpuBuffer` `NkTensor.h:34`).
- **Comment un matmul s'exécute réellement**
  (`Kernel/AI/NKTensor/src/NKTensor/NkTensorOps.cpp:385-427`) :
  1. si un opérande est device GPU → `NkGpuMatmul` (kernel NkSL) ;
  2. sinon, si dtype F32 et `M·N·K ≥ 8e6` → **auto-bascule GPU**
     (upload + `NkGpuMatmul` + download, `NkTensorOps.cpp:410-416`) ;
  3. sinon boucle CPU naïve avec `NK_DTYPE_DISPATCH_FLOAT`.
- **Backend GPU** : `NkTensorGpu` (pimpl) = chemin compute **NkSL → GLSL-Vulkan
  → glslang/SPIRV-Cross → HLSL/SPIRV/MSL → pipeline compute NKRHI**, device
  headless (`NkTensorGpu.h:1-11`). Ordre d'essai : **Vulkan préféré**, puis
  Metal/OpenGL/DX11/DX12 ; override `NK_TENSOR_API=vulkan|dx11|...`
  (`NkTensorGpu.cpp:162-211`). Noyaux existants : élémentaires, `RunMatMul`
  (f32, workgroup 16×16, `NkTensorGpu.h:54-57`), réductions, gather/contiguous,
  im2col, softmax causal, LayerNorm, embedding+scatter-add, et un **pas d'Adam
  FUSÉ** `RunAdam` param/grad/m/v en place (`NkTensorGpu.h:84-88`,
  `NkGpuAdamStep` `NkTensorGpu.h:135-136`). **Tout est f32** ; aucun noyau
  RMSNorm, RoPE, SiLU, ni déquantification.

### 1.2 NKAutograd / NKOptim — entraînement

- `NkVar` = graphe à TAGS d'opération (`NkAutoOp`, enum fermé —
  `Kernel/AI/NKAutograd/src/NKAutograd/NkVar.h:24-57`). Ops couvertes :
  Matmul, LayerNorm, SoftmaxCausal, GELU, Embedding, Permute, CE indexée
  (cibles `-1` = position MASQUÉE, `NkVar.h:193-196`)…
  **Il n'existe PAS d'op autograd RMSNorm, RoPE, SiLU/SwiGLU, ni attention
  GQA.** Mode sans gradient : `NkNoGradGuard` (`NkVar.h:157-174`).
  `NkVar::Leaf(value, requiresGrad)` permet déjà le GEL de paramètres
  (`NkVar.h:96`).
- `optim::NkAdam` : moments m/v par paramètre, AdamW si weightDecay>0,
  `SetMoments/SetStepCount` pour la reprise
  (`Kernel/AI/NKOptim/src/NKOptim/NkOptim.h:95-164`) ; clipping global
  (`NkOptim.h:32-41`). `optim::NkGradScaler` (loss scaling dynamique) existe
  déjà — brique 2/3 mixed precision (`NkGradScaler.h:1-21`).

### 1.3 NKGpt — le GPT maison (ce qu'il N'est PAS)

- Modèle `nn::NkGPT` : **embeddings positionnels appris + LayerNorm + GELU +
  attention multi-têtes classique (pas de GQA)** —
  `Kernel/AI/NKNN/src/NKNN/NkTransformer.h:226-258` (posEmb `:234`, blocs
  LayerNorm/GELU `:185-191`). **Aucune brique architecturale commune avec
  Llama/Qwen** (ni RMSNorm, ni RoPE, ni SwiGLU, ni GQA) : ces briques
  n'existent QUE côté NKInfer (cf. 1.5), en CPU sans autograd.
- Entraîneur : `NkGptTrainer::Fit()` — AdamW résident GPU
  (`Kernel/AI/NKGpt/src/NKGpt/NkGptTrainer.cpp:442`), accumulation de
  gradient, LR warmup+cosine, CE indexée avec **masquage instruction-tuning
  « Question:/Réponse: » déjà en place** (`NkGptTrainer.cpp:33`,
  `MakeBatchIdxFrom` `:164-190`) — le corpus SFT BulkGen a EXACTEMENT ce
  format.
- Tokenizer : BPE maison `data::NkBpe` (256 octets + fusions APPRISES sur le
  corpus, pré-tokenisation espace collé au mot suivant —
  `Kernel/AI/NKData/src/NKData/NkTokenizer.h:41-70`). **Incompatible avec le
  BPE byte-level de Qwen2** (vocabulaire 152 064, fusions livrées dans le
  GGUF, table octet↔unicode GPT-2, tokens spéciaux `<|im_start|>`…).
- Checkpoint `.nkgp` « NKGP » v3/v4 : dims + fusions BPE + langues + poids
  (+ moments Adam optionnels v4), lecteurs v3 compatibles
  (`Kernel/AI/NKGpt/src/NKGpt/NkGptCore.h:46-68`).

### 1.4 NKInfer — lecture GGUF : COMPLÈTE (métadonnées + tenseurs + quant)

Contrairement à ce que suppose l'énoncé de l'étape 4 (« GGUF est déjà
lisible : NKGGUFInspectTest » laisse entendre en-têtes seulement), le dépôt
contient DÉJÀ bien plus :

- **`NkGGUFLoader`** (`Kernel/AI/NKInfer/src/NKInfer/NkGGUFLoader.{h,cpp}`,
  703 lignes) : GGUF v2/v3 complet — toutes métadonnées typées, tous les
  `tensor_info` (nom, dims ordre ggml, type quant, offset), tailles validées
  au octet près contre le fichier (`NkGGUFLoader.h:26-33,142-173`).
  Accesseurs `NkGGUFGetUInt/Float/String`, et
  `NkGGUFReadFullStringArray(path, key, out)` pour matérialiser un tableau
  ENTIER (ex. `tokenizer.ggml.tokens`, 152 064 entrées) sans toucher `Load()`
  (`NkGGUFLoader.h:214-226`).
- **`NkGGUFDequant`** : déquantification RÉELLE **Q4_K, Q6_K, Q8_0, F16, F32**
  → `NkTensor` NK_F32 contigu, transcrite fidèlement de ggml-quants.c (sources
  citées, `NkGGUFDequant.h:9-31`). API :
  `NkGGUFDequantizeRaw(rawType, rawData, rawByteCount, numElements, out, err)`
  (testable SANS fichier, `NkGGUFDequant.h:68-69`),
  `NkGGUFReadTensorRawBytes`, `NkGGUFDequantizeTensor` (qui INVERSE l'ordre
  des dims ggml → row-major NkTensor, `NkGGUFDequant.h:79-89`).
  **Non supportés (refus explicite)** : Q4_0/Q4_1/Q5_*/Q2_K/Q3_K/Q5_K/Q8_K/
  IQ*/TQ*/BF16 (`NkGGUFDequant.h:33-37`). Validé 33/33 dont tenseurs réels du
  blob Qwen2.5 7B (`NKInfer/ROADMAP.md:59-98`).
- **NKGGUFInspectTest** (`Applications/NKGGUFInspectTest/src/main.cpp`,
  512 l.) : 13/13 sur le vrai blob Ollama `sha256-2bada8a745…` = **Qwen2.5 7B
  Instruct, GGUF v3, 4,36 Go, 339 tenseurs, Q4_K_M** (`general.file_type=15`),
  somme des tailles = 4 683 073 952 octets exacts (`NKInfer/ROADMAP.md:33-43`).

### 1.5 NKInfer — un chemin d'inférence de modèle externe EXISTE DÉJÀ

- **`NkQwen2Block`** (`Kernel/AI/NKInfer/src/NKInfer/NkQwen2Block.{h,cpp}`,
  344 l.) : bloc Qwen2 complet **CPU f32, SANS autograd** — RMSNorm
  (`NkQwen2Block.h:105`), SiLU (`:109`), RoPE style NEOX en place (`:121-131`),
  GQA `kvHead = qHead/(nHeads/nKVHeads)`, SwiGLU, biais Q/K/V seulement.
  Hyperparamètres TOUJOURS lus du GGUF (`NkQwen2Config`, `:64-78`). Poids par
  couche en convention **[out_features, in_features]**
  (`NkQwen2LayerWeights`, `:85-99`). Forward d'une couche :
  `NkQwen2LayerForward(cfg, w, x /*[T,d]*/, cache)` (`:142-143`).
- **`NkLinearNoBias(x, w)`** : projection x[T,K]·W[N,K]ᵀ ligne-par-ligne SANS
  matérialiser de transposée et SANS dispatch GPU surprise (`:111-119`) —
  optimisation qui a fait passer une couche réelle de ~25 s à ~2,9 s (Debug)
  (`NKInfer/ROADMAP.md:109-116,170-176`).
- **`NkKVCache`** par couche `[nKVHeads,Tpast,headDim]`, extension en place
  (`NkKVCache.h:29-61`) ; **`NkSampling`** greedy + top-k reproductible.
- **NKLLMInferTest** (`Applications/NKLLMInferTest/src/main.cpp`, 597 l.) :
  38/38 — (1) préfill == incrémental KV-cache à 0.0 près sur mini-modèle
  jouet ; (2) **forward RÉEL des 28 couches du 7B**, streaming une couche à la
  fois (`LoadLayerWeights` `main.cpp:284`, `DequantEmbeddingRows` `:312`,
  lm_head via `NkLinearNoBias` `:538`), génération autorégressive réelle de
  3 tokens (`<|endoftext|>` → "Human" → ":" → espace), **266,9 s au total en
  Debug CPU** (~2,9 s/couche/token) (`NKInfer/ROADMAP.md:139-176`).
  **Limite explicite : AUCUN encodeur BPE** — le prompt est le seul token BOS
  (`main.cpp:28-31`).

### 1.6 Synthèse : état des briques

| Brique nécessaire à l'étape 4 | État | Où |
|---|---|---|
| Lecture GGUF (méta + tenseurs + tailles) | **EXISTE** (v2/v3, validée au octet) | `NkGGUFLoader` |
| Déquantification 4/6/8 bits par blocs, CPU | **EXISTE** (Q4_K/Q6_K/Q8_0/F16/F32) | `NkGGUFDequant` |
| Déquantification Q4_0 / NF4 | **MANQUE** — mais INUTILE ici : le blob local est Q4_K_M ; NF4 n'est pas un format GGUF (bitsandbytes) | — |
| Forward transformer Llama/Qwen (RoPE, GQA, RMSNorm, SwiGLU) | **EXISTE**, CPU f32, sans autograd, B=1 | `NkQwen2Block` + NKLLMInferTest |
| Modèle 7B assemblé réutilisable (structure + chargement) | **PARTIEL** — l'orchestration vit dans le main du TEST, pas dans NKInfer | `NKLLMInferTest/src/main.cpp:284-597` |
| Génération + KV-cache + échantillonnage | **EXISTE** (B=1) | `NkKVCache`, `NkSampling` |
| Tokenizer BPE byte-level Qwen (encode) | **MANQUE** (le BPE maison est un autre algorithme ; le vocab/merges GGUF sont lisibles via `NkGGUFReadFullStringArray`) | — |
| Poids quantifiés RÉSIDENTS (type tenseur Q4 par blocs) | **MANQUE** (aujourd'hui : dequant → f32, 932 Mo/couche, streaming) | — |
| Noyaux GPU dequant-Q4 + matmul fusionné | **MANQUE** (GPU = f32 uniquement) | — |
| Couches LoRA (A·B, gel du socle) | **MANQUE** | — |
| Backward du bloc Qwen2 (pour propager le gradient jusqu'aux A/B) | **MANQUE** (autograd ne connaît ni RMSNorm ni RoPE ni SwiGLU ni GQA ; le forward Qwen2 est hors-graphe) | — |
| Boucle d'entraînement LoRA (Adam sur adaptateurs seuls) | **MANQUE** (mais `NkAdam` + masquage QA + accumulation réutilisables tels quels) | `NkOptim.h:95`, `NkGptTrainer.cpp:33,442-497` |
| Checkpoint adaptateurs | **MANQUE** (format nouveau, indépendant de « NKGP ») | — |

---

## 2. Le trou entre l'existant et l'objectif — analyse brique par brique

### 2.1 Tokenizer (bloquant n°1, purement CPU)

Qwen2 utilise un BPE **byte-level GPT-2** : alphabet = 256 octets remappés en
caractères unicode imprimables, fusions PRÉ-APPRISES livrées dans
`tokenizer.ggml.merges` (tableau de chaînes « a b »), vocabulaire
`tokenizer.ggml.tokens` (152 064), types `tokenizer.ggml.token_type`
(tokens spéciaux `<|im_start|>`, `<|im_end|>`, `<|endoftext|>`).
Le `data::NkBpe` maison (`NkTokenizer.h:41-70`) apprend SES fusions sur un
corpus et pré-tokenise autrement : il ne peut PAS produire les ids attendus
par le 7B. À écrire : un **encodeur** (le décodeur existe de fait : le test
relit `tokenizer.ggml.tokens` et concatène — `main.cpp` partie 2) :

1. table octet↔unicode GPT-2 (256 entrées, reproductible depuis la spec) ;
2. pré-tokenisation par la regex GPT-2/Qwen (approximation raisonnable :
   découpe lettres/chiffres/ponctuation/espaces — à valider par round-trip) ;
3. fusions gloutonnes par rang (réutiliser l'idée de `NkI64Map` rank,
   `NkTokenizer.h:24-34`, avec des ids 32 bits) ;
4. tokens spéciaux traités AVANT le BPE (gabarit ChatML Qwen :
   `<|im_start|>user\n…<|im_end|>\n<|im_start|>assistant\n…<|im_end|>`).

Test sans GPU : round-trip `decode(encode(x)) == x` sur des lignes réelles du
corpus + vérification d'ids sur des cas construits à la main depuis les
merges lus du GGUF (+ si possible une liste d'ids de référence générée une
fois pour toutes par un outil externe, consignée en dur dans le test).

### 2.2 Poids quantifiés résidents + noyaux (bloquant n°2)

Aujourd'hui le seul chemin est : Q4_K (fichier) → f32 (932 Mo/couche) —
tenable en streaming inférence, INTENABLE pour l'entraînement (le forward+
backward retouche chaque couche à chaque pas ; re-déquantifier 28 couches
depuis le disque à chaque pas = minutes/pas).

Il faut garder les blocs Q4_K/Q6_K **tels quels en mémoire** (4,36 Go au
total) et déquantifier À LA VOLÉE dans le noyau de projection :

- **CPU d'abord (référence)** : une `NkLinearNoBiasQ(x, rawBlocks, rawType,
  N, K)` qui déquantifie chaque ligne de W (un super-bloc de 256 à la fois,
  en réutilisant EXACTEMENT le code de bloc de `NkGGUFDequant.cpp` —
  factoriser `DequantBlockQ4K/Q6K` en fonctions internes exposées) puis fait
  le produit scalaire ligne-par-ligne comme `NkLinearNoBias`
  (`NkQwen2Block.h:111-119`). Erreur attendue vs « dequant complet puis
  NkLinearNoBias » : 0 exactement (mêmes flottants, même ordre de somme si on
  garde l'ordre k croissant).
- **GPU ensuite** : nouveau noyau NkSL `matmul_q4k` (et `matmul_q6k`) : W
  reste un buffer d'octets Q4_K, x et y f32 ; chaque thread déquantifie les
  sous-blocs nécessaires dans des registres/partagé. S'ajoute dans
  `NkTensorGpu` comme `RunMatMulQ4K(bufW, bufX, bufY, M, N, K)` à côté de
  `RunMatMul` (`NkTensorGpu.h:54-57`). Le GPU n'a AUCUN besoin de f16 pour
  cela : accumulation f32 comme les noyaux existants.

Point de vigilance backend : les buffers Q4 par tenseur restent petits
(gate/up/down ≈ 38-56 Mo, embedding/output ≈ 307-447 Mo) mais > 128 Mo pour
les deux gros — **forcer Vulkan** (`NK_TENSOR_API=vulkan`, préféré par défaut
`NkTensorGpu.cpp:164`) ; DX11 ne garantit que 128 Mo par ressource.

### 2.3 Backward du bloc Qwen2 (bloquant n°3 — le plus gros morceau neuf)

Deux voies possibles :

- (a) étendre `NkAutoOp` (RMSNORM, ROPE, SILU, ATTENTION_GQA…) et reconstruire
  le forward Qwen2 en `NkVar` ;
- (b) **backward MANUEL par couche** (recommandé) : le forward est déjà écrit
  hors-graphe (`NkQwen2LayerForward`), symétriquement on écrit
  `NkQwen2LayerBackward(cfg, w, lora, saved, dOut) -> {dX, dLoRA}`.

La voie (b) est recommandée parce que : le socle est GELÉ (on ne veut AUCUN
gradient pour wq/wk/… — l'autograd générique en calculerait ou obligerait à
des feuilles requiresGrad=false traversées), le **checkpointing par couche
devient trivial** (on recalcule le forward de la couche avant son backward —
c'est l'étape 2 du plan obtenue « gratuitement », sans machinerie de graphe),
et la mémoire est bornée par UNE couche à la fois. Les dérivées à écrire :
RMSNorm (fermée, connue), RoPE (rotation orthogonale : le backward est la
rotation d'angle opposé), SiLU, softmax causal (formule déjà codée côté GPU :
`NkGpuSoftmaxBackward`, `NkTensorGpu.h:192`), GQA (somme des gradients des
têtes Q d'un même groupe vers la tête KV), et les projections LoRA.

### 2.4 LoRA proprement dit (petit, une fois 2.3 en place)

`y = W₀x + (α/r)·B(Ax)` avec A[r,in] init N(0,σ), B[out,r] init 0 (sortie
initiale = socle exact), A/B en **f32** (40,4 M paramètres à r=16 sur les
7 projections × 28 couches ; 10,1 M en attention seule à r=8 — cf. §3).
Les moments Adam ne portent QUE sur ces feuilles : réutiliser
`optim::NkAdam` tel quel en ne lui passant que les `NkVar` A/B
(`NkOptim.h:99-100`), voire `NkGpuAdamStep` fusé (`NkTensorGpu.h:135`)
quand les adaptateurs seront résidents GPU. Export : fichier adaptateur
séparé (magic « NKLA » v1 : config {r, α, cibles, hash du GGUF} + paires A/B
+ moments optionnels comme NKGP v4 — `NkGptCore.h:53-60` sert de modèle),
**sans jamais toucher au format NKGP** des paliers from-scratch.

### 2.5 Boucle SFT

Réutiliser les mécanismes de `NkGptTrainer::Fit()` (accumulation, warmup+
cosine, EMA, checkpoint périodique — `NkGptTrainer.cpp:429-546`) mais dans un
NOUVEL entraîneur (`NkQloraTrainer`) : le masquage « seule la réponse compte »
existe conceptuellement (`NkGptTrainer.cpp:33`) et la CE indexée accepte déjà
les positions masquées à -1 (`NkVar.h:193-196` ; à défaut d'autograd, en
manuel : `dLogits = softmax − onehot` sur les positions non masquées, 0
ailleurs). **lm_head à découper en tranches de positions** (les logits
[T,152064] f32 pèsent 623 Mo à T=1024 — calculer perte et gradient par
tranches de 128 positions ≈ 78 Mo).

---

## 3. Budget mémoire exact (7B Q4 + LoRA sur 8 Go)

Dimensions réelles (lues du GGUF, `NKInfer/ROADMAP.md:36-43,141-145`) :
L=28, d=3584, ffn=18944, nH=28, nKV=4, headDim=128, V=152064.

**Paramètres par couche** : wq+wo 2×12 845 056, wk+wv 2×1 835 008,
gate+up+down 3×67 895 296, biais+normes 11 776 ⇒ **233,06 M/couche** ;
×28 = 6,53 Md ; + token_embd 545,04 M + output.weight 545,04 M ⇒ **7,62 Md**.

| Poste | Taille | Notes |
|---|---|---|
| Poids Q4_K_M résidents (GPU ou RAM) | **4,36 Go** | tels quels, jamais déquantifiés en entier (Q4_K = 0,5625 o/élt ; Q6_K = 0,8203) |
| Idem en f32 (pour mémoire) | 26,1 Go/28 couches | prouve qu'il FAUT rester quantifié ; 932 Mo/couche (streaming inférence actuel) |
| LoRA r=16, 7 projections (poids+grad+m+v, f32) | **0,65 Go** | 40,37 M par. ; r=8 : 0,32 Go ; r=8 attention seule : 0,08 Go |
| Checkpoints d'activations (entrées de couche), B=1 | 0,40 Mo/token ⇒ **T=512 : 206 Mo ; T=1024 : 411 Mo** | 28×T×3584×4 o |
| Transitoire par couche (recalcul + backward) | T=512 : ≈ 0,25 Go ; **T=1024 : ≈ 0,45 Go** | intermédiaires T×79360×4 + scores 28×T²×4 (pic, une couche à la fois) |
| Logits lm_head par TRANCHE de 128 positions | 78 Mo | ne JAMAIS matérialiser [T,152064] (623 Mo à T=1024) |
| **Total VRAM (GPU résident, r=16, T=1024)** | **≈ 6,0-6,3 Go** | marge OK sur 8 Go (compositeur Windows ~0,5 Go) ; T=512 ≈ 5,6 Go |

Conclusions fermes :
- **Le checkpointing par couche est indispensable DÈS LE DÉPART** (sans lui,
  activations complètes ≈ 10,4 Mo/token ⇒ 5,3 Go à T=512 : mort). Il est
  gratuit dans un backward manuel par couche (§2.3).
- **Longueur tenable : T=512 confortable, T=1024 possible, T=2048 limite**
  (transitoire scores 470 Mo + checkpoints 822 Mo). Le corpus QA (paires
  courtes) se packe très bien à T=512.
- **fp16 GPU (étape 1) N'est PAS un prérequis** : à B=1 les activations f32
  tiennent ; l'accumulation f32 est même préférable pour la stabilité.
- **Débit attendu** : ~27 TFLOP f32/pas à T=512 (forward + recalcul +
  backward ≈ 4× forward = 4×2×6,53e9×512). RTX 3070 Laptop ⇒ ~7-15 s/pas
  réaliste. Époque complète (≈ 13 000 pas à T=512 packé) ⇒ 1-2 jours ; un
  affinage utile se fera sur un sous-ensemble/quelques milliers de pas.
  En CPU pur, l'ordre de grandeur mesuré (2,9 s/couche à T=1 en Debug,
  `NKInfer/ROADMAP.md:167-171`) donne >10 min/pas à T=512 : le CPU sert aux
  TESTS (mini-config), pas à l'entraînement réel — **les noyaux GPU
  dequant-matmul sont le vrai prérequis GPU, pas le fp16**.

---

## 4. Plan d'implémentation ordonné (jalons auto-validants)

> Règle du projet respectée : chaque jalon a un test isolé, CPU pur tant que
> possible (cf. méthode `Nkentseu : test isolé pour un calcul suspect`).
> Builds Debug ET Release à chaque lot ; fermer l'app avant de linker.

### Jalon 1 — Tokenizer Qwen2 byte-level (CPU pur, sans GPU ni gros fichier)
- **Créer** : `Kernel/AI/NKInfer/src/NKInfer/NkQwen2Tokenizer.{h,cpp}`
  (namespace `ai::infer` ; charge tokens/merges/token_type via
  `NkGGUFReadFullStringArray`, `NkGGUFLoader.h:226` ; table octet↔unicode ;
  fusions gloutonnes par rang via `NkI64Map`).
- **Créer** : `Applications/NKQwenTokenizerTest` (jenga).
- **Validation SANS GPU** : (a) tests synthétiques sur un mini-vocab/merges
  codés en dur (aucun fichier) ; (b) round-trip encode→decode == identité sur
  1 000 lignes réelles de `dlg_ollama_fr.txt` ; (c) sur le blob réel : ids de
  quelques mots vérifiés à la main contre `tokenizer.ggml.tokens` ; (d) brancher
  l'encodeur dans NKLLMInferTest : prompt « Bonjour » réel → génération
  cohérente (aujourd'hui limité au BOS, `main.cpp:28-31`).

### Jalon 2 — Backward d'UNE couche + LoRA, prouvés par gradient numérique (CPU pur)
- **Créer** : `NKInfer/src/NKInfer/NkQwen2Backward.{h,cpp}`
  (`NkQwen2LayerForwardTrain` qui sauvegarde le minimum — entrées de normes,
  q/k/v post-RoPE, probs softmax, gate/up — et `NkQwen2LayerBackward` →
  {dX, dA/dB par projection}) ; `NKInfer/src/NKInfer/NkLora.{h,cpp}`
  (`NkLoraPair {A,B,alpha,r}`, init B=0, forward additif).
- **Modifier** (additif) : rien dans `NkQwen2Block.cpp` — le forward
  d'inférence reste intact (non-régression NKLLMInferTest 38/38).
- **Validation SANS GPU** : mini-config jouet (d=32, ffn=64, nH=4, nKV=2,
  T=5) — comparaison de CHAQUE gradient LoRA et de dX aux **différences
  finies** (erreur relative < 1e-3 en f32, < 1e-6 en refaisant le calcul en
  f64 si besoin) ; B=0 ⇒ forward strictement identique au socle (bit-à-bit).

### Jalon 3 — Boucle QLoRA complète sur mini-modèle jouet (CPU pur)
- **Créer** : `NKInfer/src/NKInfer/NkQloraTrainer.{h,cpp}` (checkpointing par
  couche, CE masquée par tranches, `optim::NkAdam` sur les seuls A/B,
  accumulation, LR schedule repris de `NkGptTrainer.cpp:473-484`) ; format
  adaptateur « NKLA » (save/load + reprise moments, modèle : `NkGptCore.h:53`).
- **Validation SANS GPU** : sur le jouet, 200 pas → la loss d'un petit lot
  synthétique décroît de façon monotone (EMA) ; les poids du socle sont
  BIT-IDENTIQUES avant/après (gel prouvé) ; save/load NKLA → même loss à la
  reprise.

### Jalon 4 — Matmul-déquant Q4_K/Q6_K : CPU de référence, puis GPU
- **Modifier** : `NkGGUFDequant.cpp` — exposer la déquantification PAR
  SUPER-BLOC (fonctions internes actuelles) sans changer l'API publique.
- **Créer** : `NkQuantLinear.{h,cpp}` (CPU : produit ligne-par-ligne sur blocs
  Q4_K/Q6_K résidents, même schéma que `NkLinearNoBias`) puis noyaux NkSL
  `matmul_q4k`/`matmul_q6k` dans `NkTensorGpu` (`RunMatMulQ4K`, à côté de
  `RunMatMul`, `NkTensorGpu.h:54-57`).
- **Validation** : CPU : identité EXACTE (mêmes flottants) vs « 
  `NkGGUFDequantizeRaw` complet + `NkLinearNoBias` » sur tenseurs synthétiques
  ET sur `blk.0.ffn_down.weight` réel. GPU : |Δ| < 1e-4 relatif vs le CPU de
  référence (ordre de somme différent), sur les mêmes tenseurs. Test GPU
  isolé, hors application (motif NKGpuBenchTest).

### Jalon 5 — Forward 7B GPU complet == référence CPU interne
- **Créer/refondre** : promouvoir l'orchestration du test dans le module —
  `NkQwen2Model.{h,cpp}` (chargement Q4 résident GPU/RAM, config, forward
  full/step) ; `NKLLMInferTest` devient client.
- **Validation** : sur la même séquence (BOS + 3 tokens déjà générés et
  consignés : "Human", ":", espace — `NKInfer/ROADMAP.md:158-161`), les
  logits GPU reproduisent les logits CPU du test existant à ε près
  (top-1 identique, |Δlogit| relatif < 1e-3) ; temps/pas mesuré (attendu :
  de ~81 s CPU Debug à < 1 s GPU).

### Jalon 6 — Premier pas d'entraînement QLoRA 7B qui fait baisser la loss
- **Créer** : `Applications/NKQloraTrainTest` : gabarit ChatML sur le corpus
  BulkGen (masque = réponse seule, comme `NkGptTrainer.cpp:33`), T=512,
  B=1, accum 8-16, r=8 attention seule d'abord (état 0,08 Go), puis r=16
  7 projections.
- **Validation** : (a) loss initiale ≈ loss d'évaluation du socle sur le même
  lot (B=0 ⇒ cohérence) ; (b) 200-500 pas → EMA décroît nettement ; (c) VRAM
  pic mesurée < 7 Go ; (d) génération avant/après sur 5 questions tenues hors
  entraînement : le style « Reponse: … » français s'améliore visiblement ;
  (e) NKGP/paliers from-scratch : aucune modification de fichier partagé
  (non-régression NKGptTrain compile + tourne).

---

## 5. Pièges prévisibles (relevés dans le dépôt)

1. **Copie de NkTensor = vue partagée** (`NkTensor.h:49-51`) : un tenseur
   « sauvegardé pour le backward » qui aliasserait un buffer réutilisé serait
   silencieusement corrompu — `Clone()` aux frontières (le KV-cache fait déjà
   cette copie profonde exprès, `NkKVCache.h:41-43`).
2. **Dispatch GPU SURPRISE de `ops::Matmul`** dès M·N·K ≥ 8e6
   (`NkTensorOps.cpp:406-416`) : dans tout chemin voulu-CPU (référence,
   tests), utiliser `NkLinearNoBias`/les noyaux dédiés — c'est déjà la règle
   dans `NkQwen2Block.cpp` (`NkQwen2Block.h:44-48`).
3. **Ordre des dims ggml inversé** (ne[0] = dimension rapide) :
   `NkGGUFDequantizeTensor` inverse l'ORDRE DES DIMS, pas les octets
   (`NkGGUFDequant.h:82-88`) ; les poids sont donc [out,in] — toute nouvelle
   lecture brute (blocs Q4 résidents) doit garder la MÊME convention, sinon
   les projections seront transposées sans erreur de forme visible (matrices
   carrées wq/wo !).
4. **Transposée générique = poison** : `Transpose+Contiguous()` élément par
   élément a coûté ~9× sur lm_head (`NKInfer/ROADMAP.md:170-176`). Les noyaux
   Q4 doivent lire W ligne par ligne, jamais via une transposée matérialisée.
5. **Allocations** : zéro STL ; `NkTensorStorage::Allocate/Release` refcompté
   via NKMemory (`NkTensor.h:30-39`), `NkVector`/`NkString` partout, lecture
   de blocs par `memcpy` champ-par-champ pour l'alignement
   (`NkGGUFDequant.h:39,66-69`). Ne pas garder un `RawData()` au-delà de la
   vie du dernier référent.
6. **Fichiers > 2 Go** : bug `fseek/ftell` 32-bit DÉJÀ corrigé dans `NkFile`
   (`NKInfer/ROADMAP.md:44-50`) — toute nouvelle E/S doit passer par NkFile,
   pas par stdio direct.
7. **Ne pas toucher `NkGGUFLoader::Load()` ni le format NKGP** : la méthode
   éprouvée est ADDITIVE (`NkGGUFReadFullStringArray` a été ajoutée à côté,
   `NkGGUFLoader.h:214-226`) ; les checkpoints paliers `.nkgp` v3/v4 et leurs
   lecteurs (`NkGptCore.h:53-68`) restent intacts — l'adaptateur LoRA vit
   dans SON fichier (NKLA) avec le hash du GGUF socle pour éviter tout
   mélange.
8. **Backend GPU** : Vulkan préféré et validé 4/4 (`NkTensorGpu.cpp:162-164`) ;
   DX11 limite les ressources à 128 Mo garantis (embedding Q4 = 307 Mo,
   output Q6_K = 447 Mo) → verrouiller Vulkan pour ce chemin (ou découper les
   deux gros tenseurs en tranches de lignes). Un seul travail GPU à la fois
   (contention déjà vécue : la contrainte « CPU-only strict » du jalon
   inférence venait d'un entraînement GPU concurrent, `NKInfer/ROADMAP.md:109-111`).
9. **Autograd thread-unsafe et à ops fermées** (`NkVar.h:76-80,24-57`) : ne
   pas essayer d'y glisser le 7B ; le backward manuel par couche (§2.3) évite
   aussi que `NkNoGradGuard`/graphe ne gardent vivants 233 M d'éléments par
   couche.
10. **lm_head/embeddings** : déquantifier par LIGNES sélectives (embeddings,
    `DequantEmbeddingRows` `main.cpp:312`) et par TRANCHES de positions
    (logits) ; attention au cas embeddings liés (`output.weight` absent ⇒
    tied, déjà géré `main.cpp:488-493`).
11. **Échantillonnage/éval pendant l'entraînement** : la génération de
    contrôle (jalon 6d) doit passer par le forward GPU du jalon 5, sinon
    ~81 s/token CPU (`NKInfer/ROADMAP.md:167-171`) rendra l'éval plus chère
    que le pas d'entraînement.

## 6. Redécoupage recommandé de l'étape 4 (vs plan 05)

- **L'étape 1 (fp16 GPU) N'est PAS un prérequis** de l'étape 4 : à B=1,
  activations et accumulations f32 tiennent (§3). La garder pour les paliers
  from-scratch, ou l'insérer APRÈS le jalon 6 comme optimisation de débit.
- **L'étape 2 (checkpointing) est ABSORBÉE par l'étape 4** : le backward
  manuel par couche recalcule le forward — c'est le même mécanisme, obtenu
  sans machinerie de graphe. Inutile de la faire avant.
- **L'étape 3 (offload optimiseur) est INUTILE ici** : l'état Adam LoRA pèse
  0,08-0,65 Go (§3) — il tient en VRAM sans discussion.
- **Vrai prérequis GPU nouveau, absent du plan 05** : les noyaux NkSL
  « matmul sur blocs Q4_K/Q6_K résidents » (jalon 4). Sans eux, l'étape 4
  n'est validable qu'en jouet CPU (jalons 1-3), ce qui reste la bonne
  première moitié du chantier.
- Ordre effectif proposé : **jalons 1-3 (CPU pur, indépendants du GPU) →
  jalon 4 (GPU quant) → 5 (forward 7B GPU) → 6 (SFT réel)** ; les étapes 1-3
  du plan 05 restent pertinentes pour la voie from-scratch, en parallèle ou
  après.
