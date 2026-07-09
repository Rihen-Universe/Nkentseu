# AI — Roadmap globale

> Feuille de route du sous-système NKAI. Esprit : **from scratch, bottom-up, un résultat
> observable par phase**. On préfère un système simple qui *apprend* à un système complet qui ne
> tourne pas. Chaque tier repose sur le précédent.

Légende : ⬜ à faire · 🟡 en cours · ✅ fait.

## 🎯 Ambition & cap (Rodolf, 2026-07-05)

**VISER GRAND.** On ne connaît pas les moyens de demain (GPU serveurs, financement,
partenariats) — donc on construit **dès aujourd'hui la fondation *scalable*** qui les rendra
exploitables. La pile est **from-scratch, GPU-résidente, multi-backend** : le *même code* qui
entraîne MNIST sur un GPU laptop **monte à l'échelle sans réécriture**. L'**ambition est
illimitée** ; seul le **discours public** reste honnête (jamais « niveau frontière / bat Claude »
avant de l'avoir prouvé — cf. `Publications/00_FAITS_VERIFIES_CODE.md`). L'actif durable = la
**maîtrise** et l'**architecture**, pas le compute. Cap : maîtriser chaque brique → petit modèle
qui tourne → montée en échelle quand les moyens viennent. Axe différenciant = **IA incarnée dans
le moteur** (agents, civilisation, génération d'assets), pas la course au compute des géants.

---

## 🧠 Modélisation par IA (depuis 2026-07-07) — l'IA qui MODÉLISE

**Vision (Rihen)** : un modèle qui, après apprentissage, **modélise en 3D** à partir d'une
**image**, d'un **texte**, ou de **sessions `.nkmec`** — et propose un modèle 3D qui « cadre ».
C'est de l'**image-to-3D / text-to-3D** *incarné dans le moteur* : la sortie n'est pas un
maillage opaque mais une **suite de commandes d'édition** (notre couche `NkMeshEditCommand`)
qui construit la forme. Le **même squelette** (observation → policy → action) se rebranche sur
**NkAnima** (obs = squelette/pose, actions = commandes de pose/IK → auto-pose façon Cascadeur).

**Architecture générique agent ↔ système :**
```
CONDITION (image / texte / forme cible) ─┐
                                          ├─► POLICY (réseau NKAI) ─► ACTION (commande) ─► le maillage grandit
ÉTAT COURANT (observation encodée) ──────┘
```
- **Espace d'actions** = les commandes d'édition (`NkMeshEditCommand`, éditeur NKRenderer/Mesh).
- **Données** = les sessions `.nkmec` (journal de commandes sérialisé) → apprentissage par imitation.
- **Socle ML** = la pile from-scratch NKAI (NKTensor/NKNN/NKOptim/NKData/NKTrain).

**Étapes (chaque étape = fondation de la suivante) :**
1. ✅ **obs → policy → action (imitation d'un expert heuristique)** — livré : app
   **`NKMeshAITest`** (`Applications/NKMeshAITest/`). Encode `NkEditMesh → features` (nb
   sommets/faces vivantes, bbox, aplatissement, asymétrie), un **expert heuristique** choisit
   l'action (Subdiv/Extrude/Mirror/Array), un **MLP NKNN** apprend à l'imiter : **73.8% → 99.5%**
   entraînement, **98.8% sur données jamais vues** (4 classes, from-scratch, petite échelle).
   📢 Publication + article : `D:\Rihen\Rodolf\Publications\12_2026-07-07_ia-modelisation\`
   (post multi-plateforme + article scientifique avec équations ; vidéo-preuve à enregistrer,
   cf. `captures/SHOT_LIST.md`).
2. ⬜ **Apprendre depuis de VRAIES sessions `.nkmec`** (imitation humaine) — désérialiser le
   journal → paires (état avant commande, commande) → entraîner à prédire l'action + ses params.
3. ⬜ **Conditionnement CIBLE** (forme visée) → modéliser **vers un but** (RL via NKRL, ou
   imitation conditionnée) : reward = distance à la cible.
4. ⬜ **Encodeur IMAGE / TEXTE** en conditionnement → la vision complète (image/texte → 3D).
5. ⬜ **Rebranchement NkAnima** : action space = commandes de pose/IK, obs = squelette.

⚠️ **Honnêteté** : from-scratch, **petite échelle, pédagogique** — « j'apprends à un agent à
imiter des gestes de modélisation », jamais « text-to-3D frontière ». Chaque étape = publication
+ article (cf. section Communication).

### Pipeline de production autour de la modélisation IA (fusion corpus 2026-07-09)

Complément « qualité production » des étapes 1-5 ci-dessus : un mesh généré/construit par IA
n'est **utilisable** que s'il passe des **portes de validation**. Rien n'entre dans le pipeline
sans validation automatique + revue humaine rapide ; chaque étape produit un résultat
**éditable, jamais figé**.

- ⬜ **Réparation de mesh** (le vrai goulot, à prioriser avant plus de qualité de génération) :
  détection automatique des défauts (non-manifold, trous, normales inversées, doublons de
  vertices) → réparation auto (fill holes, recalcul normales, fusion) → rapport de qualité
  (zones à vérifier) → correction manuelle assistée. Cible : ops sur `NkEditMesh` (half-edge).
- ⬜ **Retopologie** : décimation adaptative préservant les silhouettes → retopo quad
  (type instant meshes) → retopo orientée-rig (edge loops alignés sur les articulations) →
  LOD auto (jeu vs cinématique) → contrôle manuel des loops critiques (visage, mains).
- ⬜ **Dépliage UV** : seams par angle de courbure → optimisation de distorsion → placement
  de seams dans les zones peu visibles → édition manuelle + ré-unwrap incrémental.
- ⬜ **Sculpt assisté** : brushes displacement/lissage (fondation classique) → suggestions de
  détail IA en **calque non destructif** → symétrie intelligente → style transfer de détail.
- ⬜ **Génération procédurale paramétrique** (assets répétitifs : archi, props, décor) :
  graphe de nodes géométriques (⚠️ substrat = **NKGraph**, `Kernel/Runtime/NKGraph/ROADMAP.md`,
  décision 2026-07-09 — le procédural ne fournit que sa bibliothèque de nodes, backend =
  commandes `NkMeshEditCommand`) → bibliothèque de motifs (patterns Bamiléké/géométriques) →
  paramètres pilotés par prompt → export vers retopo/UV standard.

### Vision pipeline global « prompt → asset complet » (fusion corpus 2026-07-09)

Flux cible reliant les domaines : prompt/rôle → **modélisation** (ci-dessus) → **texturing**
(cf. `Kernel/Runtime/NKRenderer/ROADMAP.md` Phase T) → **rig/skinning** (livré NKRenderer) →
**rôle comportemental + performance** (cf. `Applications/NkAnima/ROADMAP.md` M3/M4bis) →
**éclairage** (NKRenderer Phase T) → **VFX** (cf. `Engine/Noge/ROADMAP.md`) → rendu final.
Principes transversaux (invariants) :
1. **Technique solide d'abord, IA générative ensuite** — la couche IA enrichit un pipeline
   stable, jamais l'inverse.
2. **Toute génération est éditable** — l'utilisateur peut interrompre le flux et reprendre la
   main à chaque étape (sculpt, peinture, graphe, clip, setup lumière).
3. **L'IA compose sur un vocabulaire validé** (commandes NkMeshEditCommand, nodes de graphe,
   presets) — elle n'invente pas from scratch. C'est déjà le choix prouvé par NKMeshAITest.
4. **Inférence locale d'abord** (NkGPT/NKInfer, souveraineté from-scratch) ; APIs externes =
   option, jamais dépendance du chemin critique.
⚠️ Nommage : les documents d'origine appelaient « NKAI » une couche de transport HTTP/LLM —
en conflit avec ce module. Cette couche s'appelle ici **pont directeur** (NkAnima M4bis).

---

## Phase 0 — Mise en place

- ✅ Intégration Jenga (`NKTensor.jenga` + registre `config/modules.jenga` + workspace),
  namespace `nkentseu::ai`.
- ✅ Voie **GPU** confirmée : **compute écrit en NkSL** (contrainte « NkSL pour tout »),
  converti vers tous les backends. **Vérifié** (`NkSLComputeCheck`) : compute NkSL →
  GLSL/GLSL-Vulkan/**SPIR-V**/HLSL-DX11/HLSL-DX12/**MSL Metal** OK sur 2 kernels.
  Bug corrigé : crash SPIR-V compute (glslang `InitializeProcess` manquant).
  (NKRHI expose aussi `NkMLContext` en GLSL, mais on privilégie NkSL.)
- ✅ Format des tenseurs décidé : stockage refcompté (`NkTensorStorage`), row-major,
  strides en éléments, dtypes f32/f64/i32/i64/u8, alignement 64 via NKMemory.

## Phase 1 — Le calcul (la pierre angulaire) — ✅

**Module : NKTensor.**
- ✅ Tenseurs n-dim sur **CPU** : création (Zeros/Ones/Full/Arange/Eye/FromData),
  indexation, vues (reshape/transpose/permute/slice), broadcasting, ops élémentaires
  + unaires.
- ✅ Produit de matrices (matmul 2D) + réductions (Sum/Mean/Max/Argmax global + axe).
- ✅ **Prouvé fonctionnel** : app `NKTensorDemo` (`jenga run`) → **34/34 OK**.
- ✅ Backend **GPU** (NKRHI compute, kernels **écrits en NkSL**) — contexte `NkTensorGpu` :
  `add`/`matmul` NkSL exécutés sur GPU, validés == CPU. Chemin NkSL→GLSL→SPIR-V→HLSL/MSL
  prouvé + **10 bugs compute moteur corrigés**.
- ✅ **Validé sur les 4 backends Desktop (GPU NVIDIA réel)** : Vulkan, OpenGL, DX11, DX12
  → `NkComputeNkSL` et `NkTensorGpuTest` **4/4 OK** (matmul inclus). Override diagnostic
  `NK_TENSOR_API`. Intégration `ai::NkTensor` (ToGPU/ToCPU + dispatch `ops::`) OK.
- 🎯 ✅ **Jalon atteint : multiplier deux matrices, CPU ✅ ET GPU ✅** (4 backends).
- ✅ **Résidence GPU — ops élémentaires (2026-07-05)** : `Mul/Sub/Relu/Sigmoid/Tanh` en
  noyaux NkSL, dispatchées via `ops::` quand un opérande est sur GPU (tenseur GPU → tenseur
  GPU, **sans transfert**). Erreur max vs CPU ≈ 0.
- ✅ **Résidence GPU — réductions (2026-07-05)** : `Sum/Mean/Max` **global et par axe** via un
  noyau segmenté `[outer, reduce, inner]` (couvre tous les axes). Dispatch `ops::` sur tenseur
  GPU ; Argmax → repli CPU (sortie i64). Erreur vs CPU = 0.
- ✅ **Résidence GPU — permute/transpose (2026-07-05)** : `Contiguous()` d'une vue strided GPU
  matérialisée par un **noyau de gather par strides** (rang ≤ 8), au lieu de repasser CPU.
  Transpose 2D + Permute 3D validés == CPU. (Bug corrigé au passage : UBO `k.params` 16→256 o
  pour loger les params de gather.)
- ✅ **Résidence GPU — im2col/col2im (2026-07-05)** : conv comme réarrangement mémoire sur GPU.
  `Im2Col`/`Col2Im` (NKAutograd) basculent sur les noyaux GPU quand l'entrée est résidente ;
  **col2im en formulation *gather*** (un thread par élément d'entrée) → **pas d'atomics, pas de
  course**. Validé == CPU (erreur 0). ⚠️ Le gain conv réel n'arrive **qu'avec une passe avant
  autograd GPU-résidente** (aujourd'hui l'autograd calcule sur CPU + décharge le gros matmul) :
  ces noyaux préparent ce chaînage complet.
- 🎯 **`NkTensorGpuTest` : 20/20 OK** sur Vulkan (élémentaires + réductions + permute + im2col/
  col2im). Bug corrigé : UBO `k.params` 16→256 o.
- ✅ **Autograd conv GPU-résident bout-en-bout (2026-07-05)** : en construisant les feuilles
  depuis des tenseurs GPU (`Leaf(x.ToGPU())`), le **forward Conv2D** (im2col→matmul→reshape/
  permute/contiguous) **et le backward** (im2col + 2 matmuls + col2im) restent **entièrement sur
  GPU, sans transfert**. Un seul verrou levé : le backward de `Sum` recréait le gradient plein
  sur CPU → corrigé pour préserver le device (lit le scalaire via ToCPU, `Full` sur le device de
  l'entrée). Chemin CPU inchangé (NKAutogradTest reste 20/20).
- ✅ **Benchmark `NKConvResidentBench` (2026-07-05)** : conv `[8,64,32,32]∗[128,64,3,3]`,
  **pas complet fwd+bwd**. Feuilles CPU (matmul déchargé + transferts) **447 ms** → feuilles
  **GPU-résidentes 131 ms = ~3,4×**. **Gradients identiques** (dX err 2,4e-7, dW err 0), loss
  151,55 des deux côtés. ⚠️ Gain « modeste » (3,4× et non 90×) car le chemin CPU **décharge
  déjà** les matmuls sur GPU ; la résidence supprime en plus **les transferts** + met im2col/
  col2im/permute sur GPU. Build *debug* non optimisé.
- ✅ **Résidence généralisée — modèles entiers (2026-07-05)** :
  - Noyaux GPU ajoutés : **`mulscalar`, `addscalar`, `step` (masque ReLU')** → Tanh/ReLU/Exp*/
    MSE/softmax gardent leurs multiplications/additions scalaires sur GPU.
  - Garde-fous `ToCpu/ToDev` sur **toutes** les ops sans noyau GPU dédié (SoftmaxRows, SigmoidBCE,
    MaxPool, Upsample, Conv3D, ConvTranspose2D/3D, Exp/Sqrt/Abs) : elles font un aller-retour CPU
    **en préservant le device** → la chaîne reste résidente autour d'elles, aucun plantage.
  - `ops::Add/Sub/Mul` : repli CPU pour le **broadcast** (biais `[1,N]+[B,N]`) ; seed backward
    **device-aware** ; `Neg` = `mulscalar(-1)` GPU.
  - **`NKMlpResidentBench`** : MLP `x[128,784]→Dense512+ReLU→Dense10→SoftmaxCE`, **pas complet
    fwd+bwd**. CPU **73,9 ms** → GPU-résident **25,4 ms = ~2,9×**. **loss identique** (2,30257 =
    ln 10), **gradients des 4 paramètres identiques** (err ~1e-9). Modèle **entier** résident.
  - Non-régression : `NKAutogradTest` **20/20** (les branches GPU sont gardées par `if device==GPU`,
    le chemin CPU est inchangé).
- ✅ **Entraînement 100% GPU-résident — Adam (2026-07-05)** :
  - Noyaux GPU ajoutés : **`div` (élémentaire) et `sqrt`** → `ops::Div`/`ops::Sqrt` dispatchent
    sur GPU. NKOptim (déjà écrit en `ops::`) devient **résident** sans y toucher (état Adam mM/mV
    initialisé sur le device du paramètre ; `SetValue` garde les params sur GPU).
  - **`NKMlpResidentBench` — boucle d'entraînement** (60 pas Adam, batch fixe surappris) :
    perte **descend** 2,3026→2,2392 (monotone), **CPU 2,2448 ≈ GPU 2,2392** (|Δ|=0,0055 sur 60 pas
    d'Adam → prouve que `div`/`sqrt` GPU sont exacts), **~3,2×** vs CPU. Forward + backward +
    optimiseur : **tout sur GPU**.
- ✅ **Kernel Adam FUSÉ (2026-07-05)** : `NkGpuAdamStep` fait tout le pas d'optimiseur en **1 seul
  dispatch** par paramètre (param/m/v mis à jour **en place**), au lieu de ~8 ops synchrones.
  `NkAdam::Step` prend cette voie rapide quand params/grads/état résident GPU (repli `ops::` sinon).
  Résultat sur `NKMlpResidentBench` : **entraînement GPU 54,9 → 32,9 ms/pas**, accélération
  **3,2× → 5,3×** ; loss identique (CPU 2,2448 ≈ GPU 2,2424, |Δ|=0,0024). L'entraînement (5,3×)
  dépasse désormais le fwd+bwd seul (~3×) car Adam ne domine plus le pas.
- ✅ **MNIST réel entraîné 100% GPU (2026-07-05)** — `NKMnistGpuTrain` : MLP 784→256→10 sur les
  **60 000** images MNIST (NKData/IDX), entropie croisée softmax, **Adam fusé**, boucle NKTrain.
  Tous les paramètres + chaque lot sur GPU → forward, backward et optimiseur **résidents**. Kernels
  actifs (log) : matmul, relu, sub, mulscalar, add, reduce_sum, gather, step, mul, **adam**. Résultat
  en 3 époques (~10 s/époque, debug) : **train 98,18%**, **TEST (10 000 jamais vus) 97,29%** — pas
  de surapprentissage.
  ⇒ La pile IA **from-scratch, sans STL**, apprend une vraie tâche **de bout en bout sur GPU**.
- ✅ **Argmax GPU natif (2026-07-05)** : noyau `reduce_argmax` (indices en f32) → la famille des
  réductions est **complète sur GPU** (Sum/Mean/Max/**Argmax**). `ops::Argmax` calcule sur GPU puis
  ne rapatrie que les indices (convertis en i64). `CountCorrect` (NKTrain) passe par `ops::Argmax`
  → plus de téléchargement manuel des logits. Exactitude MNIST **inchangée** (97,29% test) = argmax
  GPU exact.
- ✅ **Noyaux GPU natifs — softmax + maxpool (2026-07-05)** : suppression des derniers allers-retours
  du chemin classifieur.
  - **`softmax_rows`** (par ligne, stable) → `SoftmaxRows` (NKAutograd) natif GPU : le **backward
    softmax-CE** ne repasse plus par le CPU. MNIST inchangé (test **97,23%**) = softmax GPU exact.
  - **`maxpool_fwd` + `maxpool_bwd`** (backward en *gather* par élément d'entrée, plage de fenêtres
    bornée, sans course) → `MaxPool2D` fwd/bwd natif GPU, argmax stocké sur GPU.
  - Validés dans `NkTensorGpuTest` (**23/23** : softmax, maxpool fwd & bwd == CPU) ; non-régression
    `NKAutogradTest` **20/20** (MaxPool2D gradient-check CPU OK). ⇒ le **CNN résident** est débloqué.
- ✅ **CNN MNIST 100% GPU (2026-07-05)** — `NKMnistCnnGpuTrain` : `x[B,784]→[B,1,28,28]`,
  Conv2D(1→8,3×3)+ReLU+MaxPool → Conv2D(8→16,3×3)+ReLU+MaxPool → flatten → Dense(784→10) →
  SoftmaxCE. **Conv (im2col/col2im), MaxPool, ReLU, softmax-CE, Adam fusé : tout résident.**
  1 époque (91 s, debug) : **TEST (jamais vu) 96,51%**. Valide le chemin **convolutionnel complet**
  (forward + backward) en entraînement réel sur GPU. ⇒ CNN de bout en bout confirmé.
- ✅ **RÉSIDENCE GPU 100% COMPLÈTE (2026-07-05)** : **plus AUCUN repli CPU** sur quelque chemin
  que ce soit. Noyaux natifs ajoutés : **`exp`, `upsample2x` (fwd+bwd), `convT2d` (fwd+dX+dW)**,
  puis **`conv3d` et `convt3d` voxel (fwd+dX+dW)** — toutes en formulations *gather* (sans course).
  `NkTensorGpuTest` **35/35** (toutes vs CPU, y c. 3D), `NKAutogradTest` **20/20** (non-régression,
  Conv3D/ConvT3D gradient-checkés). ⇒ **Toute** la pile (classifieur, génératif 2D **et 3D voxel**,
  VAE) tourne **entièrement sur GPU**. Seul reste (matériel) : le backend **Metal** (Apple).
- ⬜ **Prochaine grande marche** : voir section « 🧠 PROCHAINE GRANDE MARCHE — Transformers → petit
  GPT » (matmul par lots, LayerNorm, attention+masque causal, embedding, GELU). Montée en échelle
  (GPU serveurs) quand les moyens le permettront.

## Phase 2 — L'apprentissage — 🟡 en cours

**Modules : NKAutograd, NKNN, NKOptim.**
- ✅ **NKAutograd** : graphe define-by-run + rétropropagation mode inverse (sans STL,
  dispatch sur tag). Dérivées Add/Sub/Mul/Matmul/Relu/Sigmoid/Tanh/Sum/MSE +
  **SoftmaxCrossEntropy** (fusionné, stable) + unbroadcast. **Prouvé**
  (`NKAutogradTest`) : **8/8 gradients == différences finies**.
- ✅ **NKNN** : `nn::NkDense` (params persistants + Xavier), activations `Relu/Sigmoid/
  Tanh`, pertes `MSELoss` **et `CrossEntropyLoss`** (+ `OneHot`).
- ✅ **NKOptim** : `optim::NkSGD` (+ momentum) **et `optim::NkAdam`** (correction de
  biais ; `ops::Sqrt` ajouté à NKTensor).
- 🎯 ✅ **Jalon Phase 2 atteint et dépassé** (`NKNNTest`) : **XOR** (Dense+SGD → perte
  1.8e-5, 4/4) **et classification 3-classes** (Dense+relu+CrossEntropy+**Adam** →
  **100%**). Pile Phase 2 complète : **NKTensor → NKAutograd → NKNN + NKOptim**. Prête
  pour la Phase 3 (entropie croisée + Adam = tout pour MNIST).

## Phase 3 — Données, entraînement, inférence — 🟡 en cours

**Modules : NKData, NKTrain, NKInfer.**
- ✅ **NKData** : `NkDataset` + `NkDataLoader` (shuffle Fisher-Yates, lots, one-hot) +
  `MakeBlobs` + **chargeur MNIST IDX** (`LoadMnist`). Prouvé (`NKDataTest` 7/7).
- ✅ **NKTrain** : `TrainEpoch` (forward→CE→backward→step, métriques perte+exactitude)
  + `Accuracy`. Prouvé (`NKTrainTest`) : Dense+relu+Dense, Adam+CE → **train/test 100%**.
- ✅ **NKInfer** : `SaveParams`/`LoadParams` (format NKMD) + `Predict`. Prouvé
  (`NKInferTest`) : round-trip **exact** (A entraîné → sauve → B neuf recharge → 100%).
- ⬜ Checkpoints d'optimiseur (reprise) + boucle de validation (NKTrain Jalon 2).
- 🎯 ✅ **Pipeline complet données→train→save→load→infer** validé de bout en bout.
- ✅ **Entraînement sur le VRAI MNIST** (`Datasets/mnist/` téléchargé, `NK_MNIST_DIR`) :
  MLP 784→64→10 (Adam+CE) sur les **60 000 vraies images** → **96.3%** en 3 époques
  (`NKTrainTest`). Le framework from-scratch apprend sur données réelles. (113 s CPU debug
  → confirme le besoin d'accélération GPU pour scaler.)
- ⬜ Checkpoints d'optimiseur (reprise) + boucle de validation (NKTrain Jalon 2) ;
  chargeurs d'autres datasets (Fashion-MNIST = même format IDX ; CIFAR ; 3D via OFF/OBJ).

## Phase 4 — La décision — 🟡 en cours

**Modules : NKRL, NKAgent.**
- ✅ **NKRL** : interface d'environnement + monde-grille + **Q-learning tabulaire**
  (ε-greedy, TD). Prouvé (`NKRLTest`) : l'agent passe de 7.9% à **100%** de réussite,
  politique optimale apprise. ⬜ DQN (Jalon 2, réutilisera NKNN/NKOptim).
- ⬜ NKAgent : agent avec mémoire (passé), perception (présent), politique de décision.
- 🎯 ✅ **Jalon « ça vit » atteint** : un agent apprend tout seul à résoudre un
  environnement simple.

## Phase 5 — La vie et l'émergence

**Modules : NKEvolve, NKCivilization.**
- ⬜ NKEvolve : génomes, sélection, mutation, croisement → population qui évolue.
- ⬜ NKCivilization : monde + temps + plusieurs agents qui interagissent (sur NKECS).
- ⬜ Outils d'observation : enregistrer, rejouer, analyser les trajectoires.
- 🎯 **Jalon « ça émerge » : des comportements de société non scriptés apparaissent.**

## Phase 6 — Génération & incarnation

**Modules : NKGen, NKEmbodied.**
- ⬜ NKGen : un petit modèle génératif (image 2D → texture) ; brancher la génération
  d'assets au moteur. Puis **formes 3D** générées par **catégories** :
  🌿 végétal, 🐾 animal/créature, 🧍 humanoïde, 🌍 monde/terrain (+ rig + animation).
- ⬜ NKEmbodied : relier une politique à un corps simulé (puis réel via Kernel/Bare).
- ⬜ **Acteurs génératifs** : un personnage reçoit un **rôle** et le joue (apparence +
  animation + comportement) → animation 3D & jeux **pilotés par IA** (NKGen + NKAgent).
- 🎯 **Jalon : générer un asset (2D puis 3D) dans le moteur ; piloter un corps par une IA.**

## 🧠 PROCHAINE GRANDE MARCHE — Transformers → petit GPT (PLAN, validé 2026-07-06)

> **Objectif :** un **GPT char-level from-scratch, 100% GPU-résident**, qui **génère du texte**.
> À **notre échelle d'abord** (nanoGPT : ~0,5–2 M paramètres, entraînable sur 1 GPU laptop),
> **scalable ensuite** (cf. Phase 7). Tout s'ajoute AU-DESSUS de la pile GPU-résidente déjà faite
> (matmul, softmax, LayerNorm à venir, Adam fusé, autograd fwd+bwd sur GPU — zéro repli CPU).
> **Honnêteté :** échelle *petite* et pédagogique ; ce n'est PAS un LLM frontière (question de
> compute/données, cf. Phase 7), mais la **preuve** que l'architecture tourne de bout en bout.

### Architecture cible (1re preuve — volontairement petite)
- **Char-level** : vocabulaire = caractères d'un corpus texte (~65–120 symboles).
- `block_size T = 64` · `d_model = 128` · `n_heads = 4` (head_dim = 32) · `n_layers = 4` ·
  `d_ff = 4·d_model = 512` · `batch = 32`. → ~0,5–1 M paramètres.
- **Forward** : `tokens[B,T]` → **embedding** tok + **positionnel** appris → N× **bloc transformer**
  → **LayerNorm** final → **tête LM** (matmul → `[B,T,vocab]`) → **softmax-CE** (prédire le
  caractère suivant). **Bloc** = LN → **attention multi-têtes causale** → +résiduel → LN → **MLP**
  (Linear→GELU→Linear) → +résiduel.
- **Attention** : proj QKV (matmul) → `[B,heads,T,head_dim]` → `scores = Q·Kᵀ/√head_dim`
  `[B,heads,T,T]` → **masque causal** (triangle supérieur = −∞) → **softmax** (dernier axe) →
  `·V` → concat têtes → proj sortie.

### Briques à livrer — 1 résultat testable par étape (ordre)
- ✅ **1. Matmul par lots (batched)** (2026-07-06) : `[…,M,K]·[…,K,N]` (dims de tête = lots) —
  kernel GPU `bmatmul`, route N-D dans `ops::Matmul` (CPU+GPU), backward autograd généralisé
  (transpose des 2 derniers axes). `NkTensorGpuTest` **36/36** (GPU==CPU), `NKAutogradTest`
  **22/22** (gradient-checks BMatmul dA 9e-05 / dB 7e-05).
- ✅ **2. LayerNorm** (2026-07-06) : `autograd::LayerNorm` = normalise le dernier axe `(x−μ)/√(var+ε)`
  (γ,β composés au niveau couche). Kernels GPU `layernorm_fwd/bwd` (backward recalculé depuis x),
  CPU+GPU. `NKAutogradTest` **23/23** (grad-check 4,5e-05), `NkTensorGpuTest` **38/38** (fwd+bwd==CPU).
- ✅ **3. Softmax dernier axe + masque causal** (2026-07-06) : `autograd::Softmax` (dernier axe,
  rétro-compatible 2D) + `autograd::SoftmaxCausal` (masque le futur, fusé dans le kernel) + backward
  (jacobien softmax). Kernels GPU `softmax_rows`(dernier axe)/`softmax_bwd`/`softmax_causal`, CPU+GPU.
  `NKAutogradTest` **25/25** (Softmax 6,6e-06, SoftmaxCausal 1,3e-05), `NkTensorGpuTest` **40/40**.
- ✅ **4. Embedding** (2026-07-06) : `autograd::Embedding(table[vocab,d], indices)` — lookup +
  **backward scatter-add** (gather par ligne de table, sans course). Kernels `embedding_fwd/bwd`,
  CPU+GPU. `NKAutogradTest` grad-check 4,6e-05, `NkTensorGpuTest` fwd/bwd==CPU. (Positionnel = même
  op ou paramètre `[T,d]` ajouté, au niveau modèle.)
- ✅ **5. GELU** (2026-07-06) : `autograd::Gelu` (tanh-approx) fwd+bwd, kernels `gelu`/`gelu_bwd`,
  CPU+GPU. grad-check 1,2e-04. **`NKAutogradTest` 27/27, `NkTensorGpuTest` 44/44.**
- ✅ **6. Attention multi-têtes causale** (2026-07-06) : `nn::NkMultiHeadAttention` (proj Q/K/V/O
  Dense + reshape têtes via **`autograd::Permute`** + scores batched + `MulScalar(1/√hd)` +
  **SoftmaxCausal** + `·V`). + `nn::NkLayerNorm` (affine γ,β). App `NKTransformerTest` :
  **gradient-check attention dX 1,3e-04** + **attention GPU-résidente == CPU** (5,96e-08). 2/2.
- ✅ **7. Bloc transformer + modèle NkGPT** (2026-07-06) : `nn::NkTransformerBlock` (pré-LN →
  attention → +résiduel → pré-LN → MLP[Dense→GELU→Dense] → +résiduel) empilable → `nn::NkGPT`
  (embedding token + positionnel appris → N blocs → LN final → tête LM). 🎯 **JALON ATTEINT :
  le petit GPT (2 couches, d=16) SUR-APPREND une séquence — perte 2,92 → 0,0012** (`NKTransformerTest`
  3/3). Prouve toute la chaîne fwd+bwd+optimiseur du transformer.
- ✅ **8. AdamW** (2026-07-06) : weight decay découplé ajouté au kernel Adam fusé (`RunAdam`/`NkGpuAdamStep`
  paramètre `wd`) + `NkAdam(..., weightDecay)`. `wd=0` → Adam classique.
- ✅ **9. Tokenizer char-level** (2026-07-06) : vocabulaire = octets présents (compact), encode/decode,
  couples `(x[B,T], y décalé)` échantillonnés aléatoirement (dans `NKGptTrain`).
- ✅ **10. Entraînement + génération — `NKGptTrain`** (2026-07-06) : lit un livre réel (Project
  Gutenberg, `Resources/Datasets/`), entraîne `NkGPT` avec **AdamW 100% GPU-résident**,
  **échantillonnage autoregressif à température**.
- ✅ **Optim vitesse — broadcast GPU (2026-07-06)** : biais `Dense` (`[1,C]+[..,C]`) et affine
  `LayerNorm` (γ/β) faisaient un **aller-retour CPU** à chaque appel (broadcast non géré par les
  kernels élémentaires). Ajout de kernels GPU `addbcast`/`mulbcast` (`out[i]=big[i] op vec[i%C]`)
  → biais/affine **restent sur GPU**. `NKGptTrain` : **3,37 → 0,89 s/pas (~3,8×)**, résultat
  numérique **identique**. Validé `NKTransformerTest` 3/3, `NkTensorGpuTest` 44/44.
- 🎯 **JALON FINAL ATTEINT** : petit GPT (T=64, d=128, 2 couches, 4 têtes) entraîné **100% sur GPU**
  sur *Le Comte de Monte-Cristo* (~150 Ko) — **perte 5,04 → 2,21 (400 pas)** et **génère du texte
  français** (vrais mots *le/la/de/vous/qui/que/une*, apostrophes, accents, ponctuation). Preuve que
  toute l'architecture transformer/GPT *from-scratch, sans STL* tourne de bout en bout et génère du texte.
- Puis (montée en gamme) : plus de pas/données, modèle plus grand, **tokenizer BPE**, contexte plus long ;
  **corpus trilingue FR + EN + ghɔmáláʼ** (`bbj`, extraction du `DICTIONNAIRE_GHOMALA.pdf`) ; GPU serveurs — Phase 7.

### Où va le code (modules)
- **NKAutograd** : ops batched-matmul, LayerNorm, softmax-axe+masque, embedding, GELU (fwd+bwd, gradient-checkés dans `NKAutogradTest`).
- **NKNN** : couches `NkLayerNorm`, `NkEmbedding`, `NkMultiHeadAttention`, `NkTransformerBlock`, `NkGPT`.
- **NKOptim** : `NkAdamW`. **NKData** : dataset char-level + loader séquences. **NKTensorGpu** : kernels GPU des nouvelles ops (tests dans `NkTensorGpuTest`).
- **App** : `NKGptTrain` (entraînement + génération), enregistrée dans `Nkentseu.jenga`.

## Phase 7 — Montée en échelle (plus tard)

- ⬜ **LLM** dans NKInfer : reprendre le **petit GPT** ci-dessus → inférence optimisée, fine-tune, quantization, contexte plus long, entraînement distribué (multi-GPU) quand les moyens le permettront.
- ⬜ Grande civilisation : plus d'agents, mémoire/réflexion/planification riches (style *generative agents*), prospective.
- ⬜ Robotique réelle / objets intelligents sur **Kernel/Bare**.
- ⬜ Modèles génératifs 3D / animation.

## 📣 Communication & diffusion — **À FAIRE À CHAQUE ÉVOLUTION** (récurrent)

> **Règle permanente** : chaque évolution notable du sous-système IA doit produire, en
> plus du code, sa **diffusion**. On documente le penser, l'architecture, les rendus réels
> et les résultats obtenus, avec captures **images/vidéos** nécessaires.

À chaque jalon/évolution significatif :
1. **Publication multi-plateforme** dans `D:\Rihen\Rodolf\Publications\` (nouveau dossier daté
   `NN_AAAA-MM-JJ_sujet/`), en suivant **strictement** la charte et les règles de
   `D:\Rihen\Rodolf\CLAUDE.md` (ton **humble/travailleur/en demande d'aide**, garde-fous
   d'honnêteté, format `post.md` = LinkedIn + X thread + Facebook + Instagram carrousel +
   TikTok/Reels, charte Rihen pétrole `#0A555F`/orange `#F79A28`, slides Instagram 1080×1080,
   petite voix sceptique 1 gag max, hashtags, lien GitHub en 1er commentaire).
2. **Article scientifique** (dossier `article/`, LaTeX/Markdown) : problème, méthode
   (from-scratch, sans STL), architecture, expériences, résultats chiffrés + figures,
   limites honnêtes, travaux futurs. À publier (blog / preprint).
3. **Captures** : architectures (schémas SVG charte), résultats réels (images générées,
   rendus moteur, benchmarks), et vidéos de démo si pertinent.
4. **Honnêteté** : le NKAI est **from-scratch, à petite échelle, pédagogique** — jamais
   « ça bat PyTorch/frontière ». Mettre à jour `Publications/00_FAITS_VERIFIES_CODE.md` et le
   `CLAUDE.md` de Rodolf (⚠️ leur note « AI = aucun code » est **périmée** : il y a du vrai code).

État : 🟡 process défini ; **1re publication + 1er article** = l'évolution NKAI (calcul →
génération d'images/3D + GPU compute), à produire dans `D:\Rihen\Rodolf\Publications\`.

---

[Architecture](ARCHITECTURE.md) · [Modules](README.md)
