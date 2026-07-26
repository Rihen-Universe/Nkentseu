# NKInfer — Roadmap

> Faire tourner un modèle entraîné (Phase 3, LLM en Phase 7).
> ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — inférence de base (Phase 3) 🟡
- ✅ **Persistance des poids** (`src/NKInfer/NkInfer.{h,cpp}`) : `SaveParams`/`LoadParams`
  (format binaire **NKMD** : magic+version+count+[rank,dims,data] par tenseur),
  rechargement en place via `NkVar::SetValue`, validation des formes.
- ✅ **Prédiction** : `Predict` (argmax par ligne des logits [B,C]) + `PredictOne`.
- ✅ **Round-trip prouvé** (`NKInferTest`, `jenga run`) : modèle A entraîné 100% → sauvé
  → modèle B neuf 0% → rechargé **100%** (exact) ; prédictions A == B. **5/5 OK**.
- ⬜ Mode **sans gradient** explicite (NKAutograd) pour l'inférence pure.
- ⬜ Inférence par lots optimisée ; choix CPU/GPU.
- 🎯 **Classer un chiffre MNIST** : pipeline prêt ; brancher `NK_MNIST_DIR` (le forward
  MLP 784→64→10 + Save/Load est déjà câblé dans `NKInferTest`/`NKTrainTest`).

## Jalon 2 — efficacité
- ⬜ **Quantization** (fp16 puis int8).
- ⬜ Réutilisation mémoire / réduction des allocations.

## Jalon 3 — LLM (Phase 7)
- ✅ **Loader GGUF réel** (`src/NKInfer/NkGGUFLoader.{h,cpp}`, 2026-07-25) : parse l'en-tête
  (magic/version/tensor_count/metadata_kv_count), **toutes** les métadonnées typées
  (UINT8..FLOAT64, BOOL, STRING, ARRAY imbriquant les types scalaires — avec aperçu des
  8 premiers éléments d'un tableau, le reste étant parcouru/sauté sans être matérialisé) et
  **tous** les `tensor_info` (nom, dims, type de quantification, offset) — **sans jamais
  charger les données tensor**. Pour chaque tenseur, la taille attendue en octets est
  calculée (table block_size/type_size pour F32/F16/Q4_0/Q4_1/Q5_0/Q5_1/Q8_0/Q8_1/
  Q2_K..Q8_K/I8..I64/F64/BF16 — types IQ*/TQ* reconnus par nom mais taille non tabulée) et
  validée arithmétiquement contre la taille réelle du fichier. Zéro STL, via `NkFile`
  (NKFileSystem) + `NkVector`/`NkString`.
  - 🎯 **Jalon testable** (`Applications/NKGGUFInspectTest`, `jenga build --target
    NKGGUFInspectTest --config Release`) : ouvre un **vrai blob Ollama**
    (`sha256-2bada8a745…`, confirmé être **Qwen2.5 7B Instruct** GGUF v3, 4.36 Go, 339
    tenseurs) et affiche son architecture réelle. **13/13 OK**, dont : magic/version/
    compteurs cohérents, `general.architecture=qwen2`, `general.name=Qwen2.5 7B Instruct`,
    `qwen2.block_count=28`, `embedding_length=3584`, `attention.head_count=28/head_count_kv=4`,
    `context_length=32768`, `vocab_size=152064` (longueur réelle de
    `tokenizer.ggml.tokens`, premiers tokens affichés), 339/339 tenseurs avec taille
    validée (Q4_K/Q6_K/F32…), **somme des tailles calculées + offset des données =
    4 683 073 952 octets = taille exacte du fichier au octet près**. Testé aussi sur un
    blob non-GGUF (manifest) → rejet propre (« magic invalide »), exit code 1.
  - 🐛 **Bug corrigé en cours de route** (`Kernel/System/NKFileSystem/src/NKFileSystem/NkFile.cpp`,
    `Tell`/`Seek`/`GetSize`) : utilisaient `fseek`/`ftell` standards (`long`, **32-bit même
    en build 64-bit sous Windows/MSVC/MinGW**) → `GetSize()` erroné (négatif/tronqué) sur
    tout fichier > ~2 Go. Remplacés par `_fseeki64`/`_ftelli64` (Windows) et `fseeko`/
    `ftello` (POSIX, `off_t` 64-bit). Bug générique (impactait potentiellement tout gros
    fichier — vidéo, dataset, modèle —, pas seulement GGUF), découvert et corrigé grâce à
    ce chargement réel d'un blob de 4.68 Go.
  - ⚠️ **Honnêteté sur la portée** : c'est un **parseur structurel**, pas un runtime
    d'inférence. Ce qui reste : **déquantification** réelle des blocs (Q4_K/Q6_K/…) vers
    des tenseurs `NkTensor` utilisables, **exécution** de l'architecture transformer
    correspondante (Qwen2 et autres), **génération token par token** + KV-cache,
    stratégies d'échantillonnage. Les tailles de bloc/type par quantification viennent de
    la spec publique ggml/llama.cpp (reproduites de mémoire, non re-vérifiées
    octet-par-octet dans ce dépôt) — validées empiriquement ici par le fait que la somme
    calculée tombe exactement juste sur un vrai fichier.
- ✅ **Déquantification réelle** (`src/NKInfer/NkGGUFDequant.{h,cpp}`, 2026-07-25) : Q4_K,
  Q6_K, Q8_0, F16 (+ F32 passthrough) → `ai::NkTensor` NK_F32 contigu. Disposition binaire des
  blocs (`block_q8_0`/`block_q4_K`/`block_q6_K`) et algorithmes de déquantification
  (`dequantize_row_q8_0/q4_K/q6_K`, `get_scale_min_k4`) repris **fidèlement** (pas "de
  mémoire") depuis la source de référence `ggml-org/llama.cpp` (`ggml/src/ggml-common.h` et
  `ggml/src/ggml-quants.c`, consultés via WebFetch le 2026-07-25, cités en tête de
  `NkGGUFDequant.h`) — même méthode de citation de source que `NkG2P.cpp` pour le G2P. F16 :
  réutilise `NkF16BitsToF32` (`NKTensor/NkFp16.h`, conversion bit-exacte RNE déjà livrée), PAS
  réimplémentée. Zéro STL : blocs lus via `memcpy` champ-par-champ (comme `NkFp16.h`, portable
  vis-à-vis de l'alignement) depuis un buffer `NkVector<uint8>` lu via `NkFile` à l'offset exact
  du tenseur (jamais tout le fichier).
  - 🎯 **Jalon testable** (`Applications/NKGGUFInspectTest`, étendu, `jenga build --target
    NKGGUFInspectTest --config Debug --platform Windows`, build+exécution réels et bloquants) :
    **33/33 OK**, dont :
    - 3 **tests synthétiques** (aucune dépendance fichier) : round-trip Q8_0 (quantifie
      nous-mêmes un bloc de 32 valeurs connues, déquantifie via `NkGGUFDequantizeRaw`, erreur
      ≤ demi-pas de quantification) ; Q4_K et Q6_K sur des super-blocs encodés **à la main**
      (bits dérivés manuellement des formules `get_scale_min_k4`/désempaquetage 6 bits signé),
      valeurs de sortie vérifiées **exactes** à des indices choisis (couvrant le chemin `j<4` ET
      le chemin `j>=4`, plus piégeux, de `get_scale_min_k4`, qui emprunte des bits à des octets
      voisins).
    - Déquantification de **vrais tenseurs** du blob Qwen2.5 7B Instruct (le même que Jalon
      3/loader, 4.36 Go, `general.file_type=15` = Q4_K_M) : `token_embd.weight` (Q4_K, **545 M
      éléments**, tenseur entier déquantifié d'un coup) et `blk.0.ffn_down.weight` (Q6_K, 67,9 M
      éléments) — stats saines : `mean≈2.9e-6`/`stddev≈0.0136`/`min=-0.346`/`max=1.008` (Q4_K),
      `mean≈1.8e-6`/`stddev≈0.0140`/`min=-0.512`/`max=0.476` (Q6_K), **0 NaN/Inf**, ordres de
      grandeur typiques de poids de réseau pré-entraîné (centrés ~0, faible écart-type). Un
      tenseur F32 (`blk.0.attn_norm.weight`, passthrough) déquantifié aussi pour comparaison.
  - ⚠️ **Honnêteté sur la portée** : ce blob Qwen2.5 (Q4_K_M) ne contient **aucun tenseur
    Q8_0 ni F16** (seulement Q4_K/Q6_K/F32) — ces deux formats sont donc validés **uniquement**
    par les tests synthétiques ci-dessus (round-trip pour Q8_0, valeurs hexadécimales connues
    pour F16 via `NkFp16.h`, déjà testé ailleurs), pas encore sur un vrai tenseur de ce type ;
    aucun autre blob GGUF Q8_0/F16 n'a été trouvé localement pour ce test. Types **non**
    supportés par ce module (échouent explicitement, jamais silencieusement faux) : Q2_K, Q3_K,
    Q5_K, Q8_K, IQ*, TQ*, BF16. Pas de re-quantification (f32 -> quant), pas de test croisé
    contre une sortie PyTorch/llama.cpp de référence (aucune ne s'est trouvée disponible dans ce
    dépôt) — la validation repose sur (a) la fidélité de transcription vérifiée contre la source
    citée, (b) les tests synthétiques à valeurs connues, (c) la plausibilité statistique sur
    tenseurs réels. Toujours **aucune exécution transformer**, pas de KV-cache, pas
    d'échantillonnage — hors scope de ce jalon.
- ✅ **Exécuter une architecture transformer** (`src/NKInfer/NkQwen2Block.{h,cpp}`, 2026-07-25) :
  bloc Qwen2/Qwen2.5 complet sur `NkTensor` CPU brut (PAS d'autograd — inférence pure) :
  RMSNorm, RoPE (style **NEOX**, moitiés — PAS GPT-J), attention **GQA** (têtes Q ≠ têtes
  KV, mapping `kvHead = qHead / (nHeads/nKVHeads)`), MLP **SwiGLU**
  (`down(silu(gate(x))⊙up(x))`), biais sur Q/K/V uniquement (pas sur `wo` ni le MLP).
  **Sources citées dans le code** (comme `NkGGUFDequant`) : Qwen2 Technical Report
  (arXiv:2407.10671), llama.cpp (`src/models/qwen2.cpp`, WebFetch 2026-07-25),
  HuggingFace `transformers/modeling_qwen2.py` (`rotate_half`/`apply_rotary_pos_emb`),
  Zhang & Sennrich (RMSNorm, arXiv:1910.07467). θ (`rope.freq_base`) et ε
  (`rms_epsilon`) **jamais supposés** : lus des métadonnées GGUF réelles.
  - ⚠️ **CPU-only strict** (contrainte de mission : contention Vulkan à éviter avec un
    entraînement GPU concurrent) : `ops::Matmul` bascule automatiquement vers le GPU
    au-delà d'un seuil de taille (cf `NkTensorOps.cpp`) — **jamais utilisé** ici.
    `NkQwen2Block.cpp`/`main.cpp` implémentent leurs propres produits matriciels CPU
    (`NkLinearNoBias` : accès ligne-par-ligne des deux opérandes, évite en plus de
    matérialiser une transposée de poids via `NkTensor::Contiguous()` générique —
    coût prohibitif pour `lm_head`, 152064 lignes ; ce choix a réduit le temps par
    couche réelle de ~25s à ~2.9s en build Debug, cf plus bas).
- ✅ **Génération token par token + KV-cache** (`src/NKInfer/NkKVCache.{h,cpp}`,
  2026-07-25) : cache K/V par couche `[nKVHeads,Tpast,headDim]`, étendu en place
  (concaténation sur l'axe temps, motif repris de `NKNN/NkTransformer.h::CatTimeAxis`).
  `NkQwen2LayerForward` traite indifféremment un préfill `T>1` (cache vide) ou un pas
  `T=1` (cache déjà rempli) — **mathématiquement équivalent**, prouvé par le test de
  cohérence ci-dessous.
- ✅ **Stratégies d'échantillonnage** (`src/NKInfer/NkSampling.{h,cpp}`, 2026-07-25) :
  `NkSampleGreedy` (argmax déterministe) + `NkSampleTopK` (température + restriction
  top-k + tirage catégoriel via un LCG déterministe — **même schéma** que
  `NKNN/NkTransformer.h::RandnTensor`, réutilisé pour cohérence/reproductibilité).
  - 🎯 **Jalon testable** (`Applications/NKLLMInferTest`, `jenga build --target
    NKLLMInferTest --config Debug --platform Windows`, build+exécution réels et
    bloquants) : **38/38 OK**, deux parties :
    1. **Mini-modèle JOUET** (Option B, poids aléatoires déterministes NON entraînés —
       objectif : prouver la CORRECTION du pipeline, pas la qualité du texte) : preuve
       décisive — un forward **complet** sur toute la séquence (T=6, préfill) est
       **numériquement identique** (écart max = 0.0 exact ici) à un forward
       **incrémental** token-par-token via le KV-cache (6× T=1). Si le masque causal,
       le GQA ou le RoPE avait un bug d'indexation, ce test l'aurait révélé. Puis
       échantillonnage : `NkSampleGreedy` déterministe (rappelé 2×, même résultat) et
       `NkSampleTopK` reproductible (même graine -> même tirage ; graine différente ->
       tirage différent).
    2. **Poids RÉELS** (Option A — **réalisée en entier**, pas seulement "partielle") :
       le VRAI blob Qwen2.5 7B Instruct (`sha256-2bada8a7…`, même blob que les jalons
       précédents), hyperparamètres RÉELS lus du GGUF (`block_count=28`,
       `embedding_length=3584`, `feed_forward_length=18944`, `head_count=28`,
       `head_count_kv=4` → **GQA ratio 7**, `rope.freq_base=1e6`,
       `rms_epsilon=1e-6` — **aucune valeur supposée**), poids déquantifiés
       **couche par couche en streaming** (une couche à la fois, libérée avant la
       suivante — jamais les 28 couches simultanément en f32, qui dépasserait
       largement la RAM disponible) via `NkGGUFDequantizeTensor` (Jalon 3/dequant,
       inchangé). Sanity check sur 2 couches réelles d'abord (activations saines,
       0 NaN/Inf, stats plausibles), PUIS **forward RÉEL sur les 28 couches réelles**
       (pas une simulation/extrapolation), embeddings d'entrée déquantifiées par
       **lignes sélectives** (`DequantEmbeddingRows`, nouvelle fonction additive
       `NkGGUFReadFullStringArray` dans `NkGGUFLoader` — **n'a PAS modifié**
       `NkGGUFLoader::Load()` existant, zéro risque sur le jalon déjà validé 13/13),
       `output_norm` + `lm_head` (`output.weight`, non lié aux embeddings pour ce
       modèle) déquantifiés, vocabulaire **complet** (152064 tokens, PAS juste
       l'aperçu de 8) relu pour décoder les tokens.
       **Génération autorégressive RÉELLE sur 3 tokens** (prompt = token spécial BOS
       réel `<|endoftext|>` — **aucun encodeur BPE implémenté**, hors scope de ce
       jalon, cf limite ci-dessous) : `<|endoftext|>` → **"Human"** → **":"** →
       **"Ġ"** (espace BPE). Les 3 tokens prédits (1 glouton + 2 top-k
       température=0.8) sont **0 NaN/Inf**, magnitudes plausibles
       (mean≈-0.7 à -2.6, std≈2.3-2.7, min/max≈±13-22 — ordres de grandeur de logits
       typiques). Résultat qualitativement cohérent (un modèle Instruct entraîné sur
       des transcriptions de dialogue produit spontanément un motif "Human:" à partir
       d'un token de début de séquence nu — plausible, sans être une preuve de
       qualité linguistique, qui n'est PAS l'objectif de ce jalon).
       **Temps réel mesuré** (build **Debug**, CPU seul, tel qu'imposé par la mission) :
       ~2.9 s/couche réelle (28 couches ⇒ ~81 s/pas), déquantification `lm_head`
       (545M éléments, `output.weight`) ~6 s (une seule fois, réutilisé ensuite),
       **266.9 s au total** pour chargement + 28 couches × 3 pas + lm_head. Un
       premier essai AVANT l'optimisation `NkLinearNoBias` prenait ~21-28 s/couche
       (un `Transpose`+`Contiguous()` générique élément-par-élément sur les grandes
       matrices de poids, notamment `lm_head` à 152064 lignes, dominait le temps) —
       corrigé en réécrivant les projections linéaires pour accéder poids ET
       activations ligne-par-ligne SANS matérialiser de transposée (~8-9× plus
       rapide, mêmes résultats numériques bit-pour-bit vérifiés avant/après).
  - ⚠️ **Honnêteté sur la portée/limites** :
    - **Aucun encodeur BPE** n'est implémenté (hors scope de ce jalon) : le "prompt"
      est réduit au token spécial BOS réel (lu dans les métadonnées), pas du texte
      libre encodé. Les tokens SUIVANTS sont de VRAIES prédictions du modèle
      (autorégressives, via KV-cache), pas des choix arbitraires.
    - Batch = 1 implicite partout (un seul fil de génération à la fois).
    - Aucune comparaison croisée contre une sortie de référence llama.cpp/PyTorch
      pour CE modèle précis (aucune installation de référence disponible dans ce
      dépôt) — la validation repose sur (a) la fidélité de transcription de
      l'architecture contre les sources citées, (b) le test de cohérence
      forward-complet == forward-incrémental sur le mini-modèle jouet (preuve
      mathématique indépendante du modèle), (c) la plausibilité statistique des
      activations/logits réels (0 NaN/Inf, ordres de grandeur typiques).
    - Pas de fine-tuning/entraînement dans ce jalon (hors scope — cf point suivant).
- ⬜ Fine-tune de **petits** modèles (pas d'entraînement frontière — cf. architecture §1).

## Plus tard
- ⬜ Modèles compilés / fusionnés pour la vitesse.
- ⬜ Inférence embarquée sur **Kernel/Bare** (objets intelligents).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
