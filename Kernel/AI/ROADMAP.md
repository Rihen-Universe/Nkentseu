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
2. 🟡 **Apprendre depuis de VRAIES sessions `.nkmec`** (imitation humaine) — désérialiser le
   journal → paires (état avant commande, commande) → entraîner à prédire l'action + ses params.
   **Livré (2026-07-23), PIPELINE seul — PAS d'entraînement réel encore** : ajouté dans
   `NKMeshAITest` (même app, section « Étape 2 », réutilise `Encode()`/NKTensor/NKNN de
   l'étape 1) :
   - Désérialiseur = **réutilisation directe** de `renderer::NkMeshEditRecorder::Deserialize`
     (déjà zero-STL dans le moteur, format `NMEC` v1 : magic+version+count, par commande
     op(u8)+selection(u32×n)+extrude+merge+subdiv+plan bisect+xform 4×4+moveDeltas) — aucune
     réimplémentation du parseur binaire, tel que demandé.
   - Nouveau : `BuildImitationPairsFromRecorder` — rejoue un journal réel depuis un mesh de
     BASE et produit, pour chaque commande, (features `Encode()` de l'état AVANT, label
     `NkMeshEditOp` réel 0..8, commande complète). Vocabulaire d'actions **différent** de
     l'étape 1 (Subdiv/Extrude/Mirror/Array = actions du *modificateur* heuristique) : ici
     c'est le vocabulaire **réel de l'éditeur** (Extrude/Delete/Merge/MakeFace/Subdivide/
     LoopCut/Bisect/Move).
   - **[A] Testé sur un VRAI fichier trouvé dans le dépôt** : `edit_session.nkmec` (612 o, 4
     commandes, enregistré par un humain via Demo3D **F5**, PAS généré par cette tâche) →
     désérialisé OK, 4 paires extraites, params affichés. ⚠️ **Limite honnête** : le format
     `.nkmec` ne stocke pas le maillage de base ; la base a été **reconstruite au mieux**
     (sphère UV 32×32 quadifiée, algorithme recopié de `NkMeshSystem::BuildSphereData`, déduit
     du code Demo3D `sel<16 ⇒ sphère`) — 2/4 commandes s'appliquent sur cette base reconstruite
     (Extrude no-op deux fois : la sélection [12..15] ne forme probablement pas exactement une
     face de CETTE base précise). Le pipeline de désérialisation/extraction est prouvé correct ;
     la **fidélité de la base pour ce fichier précis** ne l'est pas à 100 %.
   - **[B] Round-trip bout-en-bout avec base CONNUE (sans ambiguïté)** : cube connu → 3 vraies
     commandes (Subdivide/Extrude/Move) via le **chemin moteur exact**
     (`NkMeshEditCommand::Apply` + `NkMeshEditRecorder::Push/Serialize`, identique à Demo3D
     F5/F6, non réimplémenté) → écrit sur disque → relu → **round-trip binaire EXACT, octet
     pour octet** → 3/3 paires extraites correctement → plomberie ML vérifiée (features →
     `NkTensor` → `NkVar`/`NKNN`, logits `[3,9]`, **pas un entraînement**).
   - Build réel (`jenga build --target NKMeshAITest`) + run réel confirmés (`Build/Bin/
     Debug-Windows/NKMeshAITest/NKMeshAITest.exe`), sortie visible dans la console pour les
     deux preuves [A] et [B].
   - **Reste à faire pour un vrai "🟡→✅"** : (1) un **corpus de vraies sessions humaines**
     (une poignée de commandes ne suffit pas à entraîner un MLP qui généralise — actuellement
     UN SEUL fichier réel de 4 commandes existe dans le dépôt) ; (2) stocker/associer la base de
     chaque session (le format actuel ne le fait pas → embarquer un identifiant de primitive ou
     un snapshot de la base serait plus robuste que la déduction "best-effort") ; (3)
     entraînement réel + régression des paramètres continus (offset, deltas), pas seulement la
     classe d'action.
3. ⬜ **Conditionnement CIBLE** (forme visée) → modéliser **vers un but** (RL via NKRL, ou
   imitation conditionnée) : reward = distance à la cible.
4. ⬜ **Encodeur IMAGE / TEXTE** en conditionnement → la vision complète (image/texte → 3D).
5. ⬜ **Rebranchement NkAnima** : action space = commandes de pose/IK, obs = squelette.

⚠️ **Honnêteté** : from-scratch, **petite échelle, pédagogique** — « j'apprends à un agent à
imiter des gestes de modélisation », jamais « text-to-3D frontière ». Chaque étape = publication
+ article (cf. section Communication).

#### Stratégie de DONNÉES (décidée 2026-07-28) — comment débloquer l'étape 2

L'étape 2 est bloquée par une seule chose : **il n'existe pas de corpus** (un unique `.nkmec`
réel de 4 commandes). Trois sources complémentaires, par ordre de faisabilité :

1. **Agent-professeur pilotant le modeleur** (débloque tout de suite). Les opérations d'édition
   sont des **données typées** (`NkMeshEditCommand` + journal `.nkmec`), donc un agent LLM qui
   pilote l'éditeur produit directement des trajectoires état→action apprenables, à l'échelle,
   sans intervention humaine. C'est le chaînon manquant du corpus.
   ⚠️ **Limite honnête** : on apprend alors *le style de l'agent*, pas le geste humain. Un agent
   LLM est crédible sur le **hard-surface / procédural** (mobilier, bâtiments, pièces mécaniques,
   props, low-poly) qu'il peut décrire en séquences d'opérations et vérifier géométriquement
   (symétrie, dimensions). Il est **mauvais sur l'organique** (personnage, humain, animal), qui
   exige un ajustement visuel continu et un jugement de proportions qu'il ne peut pas
   s'auto-appliquer de façon fiable. Ne pas prétendre le contraire.
2. **Cible de référence** (résout la limite ci-dessus, = étape 3). Donner un maillage cible et
   demander de le reproduire par opérations d'édition transforme la tâche en problème **bien posé
   et auto-évaluable** (reward = distance à la cible) : plus besoin de jugement esthétique,
   parallélisable, y compris pour l'organique. C'est la voie viable pour les formes complexes.
3. **Corpus de maillages existants** pour l'organique. ⚠️ **Contraintes réelles** : (a)
   **provenance et licence** — n'utiliser que des modèles dont la licence autorise explicitement
   l'entraînement (CC0, domaine public, créations propres, scans possédés) ; ne pas utiliser
   d'assets propriétaires ou récupérés sans droit. (b) **échelle** — « quelques centaines de
   modèles » reste **petit** ; l'état de l'art en génération organique s'entraîne sur des ordres
   de grandeur supérieurs. Attendre des résultats limités et le dire, conformément au principe
   d'honnêteté ci-dessus.

#### Suite (retopologie, UV/texturing, rig) — cadrage honnête

Ces étages sont déjà listés (NKGen jalon 3, § « Pipeline de production » ci-dessous). Précision
d'ambition : **retopologie apprise**, **dépliage UV appris** et **texturing appris** sont des
sujets de **niveau recherche**, pas des incréments de quelques jours. Ordre réaliste : d'abord
les versions **algorithmiques** (décimation QEM, quad field-aligned, unwrap par seams + bake),
qui donnent un pipeline utilisable ; l'apprentissage vient **enrichir** ces briques ensuite,
jamais les remplacer d'emblée. Cohérent avec « technique solide d'abord, IA générative ensuite ».

#### Effet de bord vertueux (à garder en tête)

Chaque opération ajoutée au modeleur (bevel, inset, spin, dissolve, loop cut…) **élargit l'espace
d'actions** de l'agent, donc la complexité des modèles atteignables. NK3DModeler n'est pas
seulement un outil pour l'utilisateur : c'est aussi **l'environnement d'entraînement** de la
modélisation par IA. C'est ce qui justifie l'exigence « toute opération doit être une commande
typée, avec undo et topologie cohérente » — sans quoi les trajectoires ne seraient ni rejouables
ni apprenables.

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

## Phase 2 — L'apprentissage — ✅ socle complet (extensions en cours)

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

## Phase 3 — Données, entraînement, inférence — ✅ socle complet (extensions en cours)

> ✅ Option A.1 livrée (2026-07-12) : accumulation de gradient + scheduler LR (warmup+cosine) +
> validation forward-only **remontés dans la lib générique NKTrain** (`NkLRSchedule`,
> `TrainEpochAccum`, `EvalLoss`, `SelfTest`), factorisés depuis NkGptTrainer. `NKTrainTest` **4/4**.
> Reste : NKData = MNIST IDX seul ; NKInfer f32 non portable.

**Modules : NKData, NKTrain, NKInfer.**
- ✅ **NKData** : `NkDataset` + `NkDataLoader` (shuffle Fisher-Yates, lots, one-hot) +
  `MakeBlobs` + **chargeur MNIST IDX** (`LoadMnist`). Prouvé (`NKDataTest` 7/7).
- ✅ **NKTrain** : `TrainEpoch` + `Accuracy` **+ (Option A.1, 2026-07-12)** `NkLRSchedule`
  (warmup linéaire → décroissance cosine → plancher), `TrainEpochAccum` (accumulation de
  gradient + scheduler + loss configurable), `EvalLoss` (validation forward-only), `SelfTest`.
  Prouvé (`NKTrainTest` **4/4**) : Adam+CE → train/test 100% ; accumulation+scheduler converge.
- ✅ **NKInfer** : `SaveParams`/`LoadParams` (format NKMD) + `Predict`. Prouvé
  (`NKInferTest`) : round-trip **exact** (A entraîné → sauve → B neuf recharge → 100%).
- ✅ **Checkpoints modèle+optimiseur généralisés (2026-07-26)** : `NKTrain/NkCheckpoint.h`
  (format « NKTC » v1, N'IMPORTE QUEL modèle NKNN + `optim::NkAdam`) — ce qui
  n'existait QUE dans `NkGptTrainer` (format « NKGP » v4, spécifique GPT) est
  maintenant généralisé dans NKTrain, comme prévu par cette roadmap. Preuve réelle
  (`NKTrainTest`) : reprise **EXACTE** (écart max = 0 sur un pas Adam identique,
  modèle continué vs modèle rechargé depuis le checkpoint).
- 🎯 ✅ **Pipeline complet données→train→save→load→infer** validé de bout en bout.
- ✅ **Entraînement sur le VRAI MNIST** (`Datasets/mnist/` téléchargé, `NK_MNIST_DIR`) :
  MLP 784→64→10 (Adam+CE) sur les **60 000 vraies images** → **96.3%** en 3 époques
  (`NKTrainTest`). Le framework from-scratch apprend sur données réelles. (113 s CPU debug
  → confirme le besoin d'accélération GPU pour scaler.)
- ✅ **Callbacks génériques + validation + reprise (NKTrain Jalon 2/3, 2026-07-26)** :
  `train::Fit` (boucle pilotée par callbacks : `NkEarlyStopping`, `NkLRSchedulerCallback`/
  `NkStepDecayCallback`, `NkLoggingCallback`) + `train::Evaluate` (perte+exactitude en un
  passage) + reprise après interruption (`fromEpoch`/`toEpoch` + état restauré). Preuve
  réelle (`NKTrainTest`, 14/14 OK) : early-stopping arrête réellement `Fit()`, LR
  cosine/paliers pilotent réellement Adam, reprise après interruption simulée -> 100%.
- ✅ **Tokenizer BPE généralisé dans NKData + vocabulaire/séquences/padding + augmentation
  + split train/val/test (NKData Jalon 2/3, 2026-07-26)** : `NKData/NkTokenizer.h`
  (`data::NkBpe`, déplacé depuis `NKGpt/NkGptCore` qui le réexporte par alias — non
  régressé, `NKGptTrain` re-testé), `NKData/NkSequence.h` (`NkVocab`, `PadSequences`),
  `NKData/NkAugment.h` (`SplitDataset`, `ConcatDatasets`, `AugmentFlipHorizontal`,
  `AugmentGaussianNoise`, `AugmentTokenDropout`). Preuve réelle (`NKDataTest`, 32/32 OK).
- ⬜ Reste (hors périmètre de cette itération) : chargeurs d'autres datasets
  (Fashion-MNIST = même format IDX ; CIFAR ; 3D via OFF/OBJ) ; chargement en flux
  (gros jeux, cf `NKData/ROADMAP.md`) ; visualisation des courbes d'entraînement
  (cf `NKTrain/ROADMAP.md`) ; checkpoint généralisé à `NkSGD` (Adam seul pour l'instant,
  c'est l'optimiseur dominant dans NKAI).

## Phase 4 — La décision — 🟡 en cours

**Modules : NKRL, NKAgent.**
- ✅ **NKRL** : interface d'environnement + monde-grille + **Q-learning tabulaire**
  (ε-greedy, TD). Prouvé (`NKRLTest`) : l'agent passe de 7.9% à **100%** de réussite,
  politique optimale apprise.
  - ✅ **Jalon 2 COMPLET (2026-07-25) — DQN** : `rl::NkReplayBuffer` (buffer circulaire à
    capacité fixe, FIFO) + `rl::NkDQN` (réseau Q Dense→Relu→Dense via NKNN, réseau CIBLE
    synchronisé durement tous les `targetSyncInterval` pas de gradient, cible de Bellman
    `y = r + γ·max Qcible(s',·)·(1−terminé)`, perte MSE + Adam via NKOptim, ε-greedy). **Preuve
    réelle** (`NKRLTest` test 2, monde clé-puis-porte `rl::NkKeyDoorGridWorld`, 600 épisodes
    d'entraînement, 5 graines) : politique **ALÉATOIRE** (avant) = **13.6%** de réussite
    moyenne (récompense **−0.245**) → DQN entraîné (après) = **100.0%** sur les 5 graines
    (récompense **+0.930**). Limites documentées (`Kernel/AI/NKRL/ROADMAP.md`) : pas de PER,
    pas de Double DQN, synchronisation cible dure (pas de Polyak), encodage d'état one-hot
    (pas de features continues partagées entre états proches).
  - ✅ **Jalon 3 COMPLET (2026-07-26) — PPO, actions continues, preuve multi-agent** :
    `rl::NkPolicyNet` (politique discrète OU gaussienne continue) + `rl::NkPPO` (Schulman et al.
    2017, arXiv:1707.06347, objectif clippé + bonus d'entropie ; avantage par **GAE COMPLET**,
    Schulman et al. 2016, arXiv:1506.02438). Nouvel opérateur `autograd::Log` ajouté à NKAutograd
    (manquait pour la log-probabilité discrète exacte). Nouvel environnement à actions continues
    `rl::NkReach2D` (point 2D, déplacement borné, cible aléatoire par épisode) + preuve multi-agent
    minimale `rl::NkReach2DMulti` (2 politiques PPO indépendantes, même monde, couplées par une
    pénalité de collision — PAS de MARL avancé). **Preuve réelle** (`NKRLTest` tests 3/4/5) :
    PPO discret (sanity) 1%→**100%** sur le monde-grille du Jalon 1 ; PPO continu (`NkReach2D`, 5
    graines) politique aléatoire **10.0%**/−11.20 → PPO entraîné **26.0%**/−4.98 ; multi-agent (2
    PPO, 3 graines) **9.83%**/−11.10 → **10.0%**/−7.34 (progrès net en récompense, marginal en
    réussite — tâche plus dure, budget honnêtement modeste). Bug réel rencontré et corrigé : la
    moyenne gaussienne non bornée divergeait vers ±∞ sous un espace d'actions saturé par
    l'environnement (aucune force de rappel) — corrigé par `μ=actionScale·tanh(sortie MLP)` +
    normalisation des observations. Limites documentées (`Kernel/AI/NKRL/ROADMAP.md`) : pas de
    minibatching, pas de collecte parallèle multi-environnements, perte traitée échantillon par
    échantillon (pas de gather différentiable dans NKAutograd, même contrainte que NkDQN), pas de
    politique squashée façon SAC, pas de MARL coopératif/compétitif avancé.
- ✅ **NKAgent** (2026-07-23, Jalon 4 complet 2026-07-26) : premier agent réel livré — `NkAgentMemory` (buffer borné de
  transitions), `NkAgentPerception` (état `NkGridWorld` → 4 features, spécialisé grille pour
  l'instant), `NkAgentPolicy` (enveloppe `rl::NkQLearning`, ne réimplémente pas le RL). Prouvé
  par `NKAgentTest` (build + run Debug/Windows réels) : évaluation gloutonne **200/200 (100%)**
  sur le même monde-grille que NKRLTest, via la nouvelle couche.
  - ✅ **Jalon 2 COMPLET (2026-07-25) — importance + memory replay** : transitions pondérées
    par **|erreur TD|** (proxy de « surprise », exact sur Q tabulaire), oubli par **moindre
    importance** (plus FIFO pur ; le souvenir le plus récent est toujours accepté) ;
    `NkAgent::Replay` rejoue des lots uniformes de la mémoire via `NkQLearning::Update`
    (opt-in `NkAgentConfig.replayEnabled/BatchSize/Interval`, défaut inchangé). **Ablation
    réelle** (`NKAgentTest` test 2, 5 graines, budget réduit à **80 épisodes** vs 4000) :
    SANS replay **20 %** de moyenne en éval gloutonne, AVEC replay **100 %** (~6 400
    transitions rejouées/agent) — accélération d'apprentissage prouvée. (Mesuré : à ≥200
    épisodes les deux bras saturent déjà à 100 % sur cette grille 5×5.)
  - ✅ **Jalon 3 COMPLET (2026-07-25) — buts & planification** : `NkAgentGoal` (état-cible +
    priorité + budget de pas) + `NkGoalStack` (pile LIFO de sous-buts — décomposer « ressource
    PUIS but final » = empiler le but final puis le sous-but dessus) + `NkAgent::StepWithGoals`
    (politique **dédiée par but**, `PolicyForGoal`, récompense façonnée localement, +
    `RunAgentEpisodeWithGoals`, générique sur `rl::NkEnvironment` via un nouveau constructeur
    `NkAgent`). Nouvel environnement `rl::NkKeyDoorGridWorld` (porte/but final fermée tant que la
    clé n'est pas ramassée). **Ablation réelle** (`NKAgentTest` test 3, 5 graines, grille 5×5,
    budget serré 100 épisodes/`maxSteps=10`) : agent SANS but (1 seul but = la porte) **0 %** de
    réussite (0/500 sur 5 graines, échec structurel — pas juste plus lent) contre agent AVEC pile
    de buts (clé PUIS porte) **100 %** (500/500). Vérification isolée : un but au budget
    `maxSteps=1` impossible est marqué `Failed` puis dépilé (`[ OK ]`).
  - ✅ **Jalon 4 COMPLET (2026-07-26) — personnalité + raisonnement LLM** : (a) `NkAgentPersonality`
    (boldness/curiosity/patience, fonctions libres sur les Q-valeurs déjà apprises, additif via
    `NkAgent::StepWithPersonality`) — ablation réelle (`NKAgentTest` test 4, même politique
    entraînée, 300 épisodes/profil) : profil Prudent = 0.97 % de pas adjacents à un trou / 1 chute
    contre profil Audacieux = 29.08 % / 84 chutes — comportement mesurablement différent prouvé.
    (b) `NkAgentLLMReasoning` — pont réel vers NKInfer (Qwen2.5 7B Instruct, forward complet 28
    couches réelles, poids GGUF réels déquantifiés à la demande, aucun mock) pour une décision
    jugée ambiguë (politique tabulaire neuve, Q=0 partout) ; prompt réduit à un encodage numérique
    (aucun encodeur BPE dans ce dépôt) restreint à 4 tokens-chiffres candidats = les 4 actions.
    **3 décisions réelles** (`Applications/NKAgentLLMTest`, build+run réels 2026-07-26) : ~148.7
    s/décision en moyenne (446.0 s total), déterminisme vérifié (requête répétée → même action),
    dépendance réelle à l'entrée vérifiée (logits différents sur état/but différents). **5/5 OK**.
    Limite honnête : latence NON adaptée au temps réel, réservé à un très petit nombre de
    décisions hors-ligne/de repli. Reste (hors périmètre, cf `NKAgent/ROADMAP.md` « Plus tard ») :
    besoins/désirs internes, réflexion (résumer le passé), relations sociales, émotions,
    communication entre agents.
- 🎯 ✅ **Jalon « ça vit » atteint** : un agent apprend tout seul à résoudre un
  environnement simple.

## Phase 5 — La vie et l'émergence — 🟡 en cours (NKCivilization Jalon 1 + outils d'observation ✅ 2026-07-25)

**Modules : NKEvolve, NKCivilization.**
- 🟡 **NKEvolve** (2026-07-23) : moteur génétique RÉEL — `NkGenome` (gènes réels + fitness),
  `NkPopulation` (init aléatoire LCG déterministe), `NkEvolution` (élitisme + **sélection par
  tournoi** + **croisement arithmétique** + **mutation gaussienne** Box-Muller ; fitness =
  pointeur de fonction fourni par l'appelant, zéro `std::function`). Prouvé par `NKEvolveTest`
  (build+run réels Debug/Windows) : population de 80 individus / 6 gènes faisant évoluer un
  vecteur vers une cible fixe — **fitness moyen 0,013 → 0,91** sur 200 générations, **meilleur
  génome jamais vu → fitness 1,0** (erreur max/gène 0,0005).
  - ✅ **Neuroévolution — poids d'un réseau SANS gradient (2026-07-25)** : `NKEvolveNNTest` —
    génome = poids+biais plats d'un réseau **2→4→1** (XOR, convention `nn::NkDense`), forward
    pass **manuel** (pas de `NKAutograd`, zéro backprop — contrainte GPU occupé par le Palier 6
    → tout CPU), fitness = `1/(1+MSE)` sur les 4 exemples XOR ; `NkEvolution`/`NkPopulation`
    réutilisés tels quels. **Preuve réelle** : fitness moyen **0,708 → 0,999** (200 individus,
    400 générations), meilleur génome → **résout XOR 4/4** (erreur max 0,026), **sans aucun
    gradient**.
  - ✅ **Généralisation train/test RÉELLE + passage à l'échelle mesuré (2026-07-25, même jour,
    comble les 2 limites ci-dessus)** : `NKEvolveNNTest` étendu — classification 3 classes
    (clusters 2D volontairement rapprochés + bruit large pour un vrai recouvrement, PAS
    séparable à 100%), **360 points, split 252 train / 108 test JAMAIS vus pendant
    l'évolution**. Fitness = probabilité softmax moyenne de la bonne classe sur le train
    seulement (forward pass manuel ReLU+softmax, toujours zéro `NKAutograd`/backprop). 3
    tailles de réseau, **même protocole** (population=150, 250 générations, mêmes taux de
    croisement/mutation, même seed) : petit **2→4→3 (27 gènes)**, moyen **2→16→3 (99 gènes)**,
    grand **2→16→16→3 (371 gènes)**.
    **Généralisation (réseau moyen, référence)** : train **100%**, test **99,07%** (107/108,
    jamais vu) — écart réel mais faible (0,93 point), largement mieux que le hasard (33,3%).
    **Passage à l'échelle** : PAS de dégradation monotone claire du meilleur génome — les 3
    tailles atteignent la **même exactitude test finale (99,07%)** grâce à l'élitisme qui
    protège le meilleur individu. En revanche la **fitness MOYENNE de population** à
    génération 250 n'est PAS monotone avec la taille : petit=0,9857, moyen=0,9937,
    **grand=0,9873** (le moyen bat les deux autres) — et à génération 50 le grand (371 gènes)
    est nettement en retard (0,9717 contre 0,9795/0,9808) : la convergence de la MOYENNE de
    population ralentit bien avec la taille du génome à budget de générations fixé, mais ça
    n'a pas (encore, sur cette plage 27→371 gènes) dégradé la qualité du MEILLEUR individu
    retenu. Résultat nuancé, pas la dégradation nette attendue a priori — à confirmer sur des
    génomes encore plus gros ou moins de générations. Reste : hyperparamètres réglés à la
    main (pas de recherche systématique ni moyenne sur plusieurs graines), pas de comparaison
    chronométrée GA-vs-SGD. Détail : `NKEvolve/ROADMAP.md` Jalon 2.
  - ⬜ Reste : suivi de diversité, topologie évolutive (NEAT), couplage à un vrai problème
    d'agent (cf `NKEvolve/ROADMAP.md`).
- 🟡 **NKCivilization** — Jalon 1 ✅ sur le **VRAI substrat NKECS** (2026-07-25) :
  - ⚠️ Correction du constat du 2026-07-23 (« NKECS pas branché au workspace ») : il était
    **FAUX/dépassé** — `Nkentseu.jenga` inclut bien `Kernel/Runtime/NKECS/NKECS.jenga` (avant
    Noge qui en dépend), et Noge l'utilise déjà (pont `NkAgentComponent`/`NkAgentSystem` +
    démo `NkAgentEcsDemo`, 100 % éval).
  - **Lib `NKCivilization`** (`Kernel/AI/NKCivilization/src/`, enregistrée modules + workspace,
    zéro STL côté NKAI) : composants `NkCivPosition` + `NkCivAgentRef` (pointeur non possédant
    vers `agent::NkAgent` — même philosophie que le pont Noge, mais rebâtie sur **NKECS pur**
    car Kernel/AI ne doit pas dépendre d'Engine/Noge — inversion de couches), substrat
    `NkCivGridState` (grille, trous, but, **ressources consommables premier-arrivé**), system
    `NkCivAgentSystem` (un `ecs::NkSystem` : query → ordre de tour déterministe → politique
    apprise gloutonne → collision/ressources).
  - **Preuves réelles** (`NKCivilizationTest` étendu, build+run, exit 0, 3/3 OK) : 3 agents =
    3 entités ECS, départs choisis pour croiser les chemins ⇒ **collision DÉCLENCHÉE** (1
    blocage au tick 0, jamais observé dans le prototype du 23/07), **4/4 ressources
    consommées** avec **répartition divergente 1/1/2** (la case contestée 14 va au premier
    passé), **3/3 agents au but** (5/3/4 pas) via leurs politiques apprises (98,5/98/100 % en
    fin d'entraînement individuel). Reste : tick sous `NkScheduler` (multi-systèmes), temps
    continu, ressources renouvelables (observation/rejeu ✅ livré ci-dessous, cf
    `NKCivilization/ROADMAP.md`).
- ✅ **Outils d'observation** (2026-07-25) : `civ::NkCivRecorder` (journal en mémoire — position/
  action/événements par agent + ressources restantes à chaque tick, branché sur
  `NkCivAgentSystem` via un pointeur non possédant optionnel, pur observateur — sérialisation
  binaire versionnée `.nkciv`, magic `"NCIV"`, même pattern que le `.nkmec` de
  `NkMeshEditRecorder`), `civ::NkCivReplayer` (rejeu TICK PAR TICK en lecture PURE, zéro
  re-simulation), `civ::NkCivAnalyzer` (stats par agent, heatmap texte d'occupation, taux de
  contention des ressources, détection d'événements remarquables). Preuve réelle
  (`NKCivilizationTest` étendu 4 phases, build+run Debug, exit 0, **6/6 OK**) : journal de 6
  frames sauvé (324 octets), rechargé, round-trip **bit-exact** vérifié à 3 niveaux (octet par
  octet, structurel `Equals`, `VerifyExact`) ; rejeu des 6 ticks depuis le fichier RECHARGÉ
  reproduit exactement l'état VIVANT capturé pendant la simulation originale (assertions) ;
  heatmap + stats par agent + mesures globales affichées sur le journal. Détail :
  `Kernel/AI/NKCivilization/ROADMAP.md`.
- 🎯 **Jalon « ça émerge » : des comportements de société non scriptés apparaissent.** *(pas
  encore atteint : le prototype actuel exécute des politiques individuelles apprises isolément,
  sans émergence de comportement social non scripté — la seule règle collective est la
  collision).*

## Phase 6 — Génération & incarnation

**Modules : NKGen, NKEmbodied.**
- 🟡 **NKGen — socle génératif LIVRÉ** (audit 2026-07-12) : auto-encodeur dense, **VAE** dense,
  **VAE conv 2D** (chiffres MNIST générés), **VAE 3D voxels** (Conv3D/ConvT3D) → formes 3D
  exportées OBJ (banane/rocher/arbre) + rendues moteur PBR/Vulkan ; maillage (Surface Nets,
  décimation, quads). ⏳ Reste : GAN, **diffusion**, marching cubes, conditionnement **image/
  texte→3D** (le seul « conditionnement » actuel = centroïde latent par classe), catégories
  végétal/animal/humanoïde/monde, rig + animation.
- 🟡 **NKEmbodied — Jalon 1 livré** (capteurs/actionneurs génériques `NkSensor`/`NkActuator` +
  boucle perception→décision(NKAgent)→action dans un corps grille simulé, `NKEmbodiedTest` :
  100%/200 épisodes). ⏳ Reste : Jalon 2 (contrôle robuste, fréquence fixe, bruit/limites,
  sécurité) et Jalon 3 (branchement réel via Kernel/Bare, transfert sim→réel).
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
- ✅ **11. Inférence rapide — KV-cache + sampling (Option A.3, 2026-07-12)** : décodage **incrémental**
  (`NkGPT::ForwardStep` + `NkKVCache` par couche, attention/bloc/GPT) **bit-exact vs Forward complet**
  (err **0.0**) → O(T) par token au lieu de O(T²). **Échantillonnage top-k / top-p (nucleus) / température**
  réutilisable (`NKGpt/NkSampling.h`, `NkSampleToken`). `NkGptTrainer::Generate` : chemin **KV-cache**
  quand le contexte tient dans la fenêtre, sinon repli fenêtre glissante ; overload `NkSampleParams`.
  Validé `NKTransformerTest` **7/7**.
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
  **corpus trilingue FR + EN + ghɔmáláʼ** (`bbj`) ; GPU serveurs — Phase 7.

### ✅ MONTÉE EN GAMME RÉALISÉE (2026-07-07 → 09) — GPT trilingue BPE + zéro-STL
- ✅ **Corpus multi-fichiers équilibré PAR LANGUE** (`LoadCorpusByLang`) : lit tout `Resources/Datasets/`,
  préfixes `fr_`/`en_`/`bbj_` (langue = préfixe avant `_`), **1/3 par langue**. 6 livres FR + 8 EN
  (Gutenberg, domaine public) + **bbj**.
- ✅ **bbj = Ghɔmáláʼ (langue camerounaise)** : le `DICTIONNAIRE_GHOMALA.pdf` est **inextractible**
  (polices sans table Unicode → mojibake, prouvé `pdffonts`). SOURCE RETENUE = **Nouveau Testament
  Ghɔmáláʼ** (© 2002 Bible Society of Cameroon, PDF bibliamundi) → `pdftotext -enc UTF-8` = **vrai bbj
  Unicode propre** → nettoyé → `Resources/Datasets/bbj_ghomala_nt.txt` (~1,05 M car.). ⚖️ **© →
  gitignored** (`/Resources/Datasets/bbj_*.txt`), usage local seulement, jamais commité.
- ✅ **Tokenizer BPE from-scratch** (`NK_GPT_MERGES`, défaut 600) : 256 octets + fusions apprises →
  morceaux de mots → **vrais mots** (fr « idée/homme/était », en « The man and much », bbj lisible avec
  tons ɔ ə ŋ ʼ). Remplace le char-level.
- ✅ **Tag de langue** (`NK_GPT_LANG=fr|en|bbj`) : chaque séquence préfixée d'un token-tag de langue →
  génération **pilotée** (même amorce → langue au choix), tags masqués à l'échantillonnage.
- ✅ **Checkpoint NKGP v3 autonome** (dims + fusions BPE + langues + poids CPU, `FILE*`) :
  `NK_GPT_SAVE`/`NK_GPT_LOAD`/`NK_GPT_PROMPT`/`NK_GPT_GENLEN` → **générer sans réentraîner** (~4 s).
- ✅ **Optim broadcast GPU (~3,8×)** : biais Dense + affine LayerNorm restent sur GPU
  (kernels `addbcast`/`mulbcast`). 40 pas 134,8 s → 35,6 s, perte identique.
- ✅ **Résultats** (modèle 2000 pas `Resources/Models/gpt_tri_bpe_long.nkgp`, gitignored, perte
  7,28 → 3,36) : les 3 langues sortent ; **bbj le plus lisible** (phrases + questions, ex.
  « Paska m gɑ́ kɑ? »). ⚠️ Honnête : petite échelle, vrais mots mais **pas de grammaire tenue** ;
  mur = **rareté des données bbj**. Pour un chatbot Q→R : corpus **format dialogue** (à faire).
- ✅ **`NKGptTrain` 100% ZÉRO-STL** (2026-07-09) : std::string→**NkString**, std::vector→**NkVector**,
  std::map→**table de hachage int64 maison** (open-addressing sur NkVector ; BPE en tableau plat +
  séparateurs, sans NkMap/tri), std::ifstream→**FILE*** (comme NKInfer), std::filesystem→
  **NkDirectory::GetFiles**, std::chrono→**NkChrono**. `NKGptTrain.jenga` : + dép **NKFileSystem**.
  Build OK, zéro `std::` (le seul restant = commentaire), rétro-compatible v3.
- ✅ **Tests validation 74/74** : `NKAutogradTest` 27/27, `NKTransformerTest` 3/3, `NkTensorGpuTest` 44/44.
- **PRs** : #27 corpus trilingue ✅ mergée · #28 tag de langue ✅ mergée · **#30 BPE + guide** 🟢 prête ·
  **zéro-STL** = commit local `feat/nkai-zerostl` (push différé, coordination autre agent).
- **Doc/pub** : guide `Applications/NKGptTrain/GUIDE_LLM_AUTONOME.md`, README ; **Publication 13**
  (`D:\Rihen\Rodolf\Publications\13_2026-07-07_nkai-gpt-trilingue\`, posts + article + scripts vidéo) ;
  scripts vidéo ajoutés aux publications 07 (MNIST) et 12 (3D).
- **Reste** : push zéro-STL + PR ; filmer les vidéos-preuves (scripts prêts) ; entraînement plus long/gros ;
  corpus format dialogue ; plus de données bbj.

### ✅ MOTEUR D'ENTRAÎNEMENT — accumulation, instruction-tuning, LR schedule, checkpoint, corpus complet (2026-07-09, suite)

Continuation de la montée en gamme : rendre l'entraînement **plus gros, plus stable, plus sûr** sur
la RTX 3070 (8 Go, FP32), et **élargir le corpus** aux domaines demandés (code, maths, QA).

- ✅ **Accumulation de gradient** (`NK_GPT_ACCUM`, commit `9911e6f8`) : ACCUM micro-lots →
  **batch effectif = B×ACCUM** avec la mémoire d'activations d'UN micro-lot. Loss divisée par ACCUM
  (`AccumGrad`=somme → moyenne). Levier n°1 pour tenir un gros modèle sur 8 Go. Validé GPU (perte 6,57→5,68).
- ✅ **Masquage de loss / instruction-tuning** (commit `de106298`) : `SoftmaxCrossEntropy` normalise
  par les **lignes ACTIVES** (cible non tout-zéro) + **gradient nul** sur les lignes masquées ; le
  **chemin GPU rapide est préservé** quand aucun masque (flag dans `iparam`). `NKGptTrain` détecte les
  blocs `Question:/Réponse:` et **masque la question** → le modèle apprend à *répondre/raisonner*, pas à
  mémoriser la question. C'est le levier « raisonnement » (Chain-of-Thought) : garder les étapes dans les
  réponses (gsm8k) + masquer la question. Validé GPU sur squad (perte 6,55→5,47).
- ✅ **LR schedule warmup + cosine** (`NK_GPT_LR`/`NK_GPT_WARMUP`, commit `a1dc1a73`) : warmup linéaire
  puis décroissance cosine (plancher 10 %). Stabilise les longs runs.
- ✅ **Checkpoint périodique** (`NK_GPT_SAVEEVERY`, commit `a1dc1a73`) : sauvegarde tous les N pas → un
  plantage ne perd au plus que N pas. Indispensable aux runs de plusieurs heures.
- ✅ **Corpus complet 5,8 Go, 9 tags** (`Resources/Datasets/`) : `fr` (Wikipedia+C4 ~3 Go), `en` (~1,8 Go),
  `py` (codeparrot 585 Mo), `trans` (fr↔en↔bbj **bidirectionnel**, 458 Mo), **`nkentseu`** (moteur C++ 32 Mo),
  **`jenga`** (build system 11 Mo), `qa` (squad), `math` (gsm8k), **`bbj`** (Ghomala pur lafand-mt/MAFAND,
  licence permissive — complémentaire à la source NT © de l'autre agent). Collecteur consolidé :
  `D:\Projets\Camrail\AI\collect_datasets.py` (bidirectionnel, code local, robuste ; contourne
  `datasets 5.0` qui refuse les datasets « à script » → bbj lu depuis un clone local de `lafand-mt`).
- 🟡 **Run « Palier 1 » en cours** (2026-07-09) : ~13 M params (D=384/L=5/H=6/T=128/B=6/ACCUM=3), 7 tags dont
  Ghomala, 4000 pas (~6 h), LR schedule + checkpoint/250 pas. **Exe ISOLÉ** (`D:\Projets\Camrail\AI\palier_run\`)
  → les recompilations ne le touchent pas. Doc réglages par palier : `D:\Projets\Camrail\AI\STRATEGIE_ENTRAINEMENT.md`.
- ⚠️ **Constat d'échelle honnête** : à 3 M params / 250 pas, la sortie reste « token-soup » (apprend le
  **format** Question:/Réponse: + les distributions de caractères par langue, **pas le sens**). La cohérence
  émerge avec taille+pas+données. Le Palier 1 (13 M, 4000 pas) doit donner des phrases qui tiennent mieux.
- ✅ **Reprise d'entraînement depuis un checkpoint** (`NK_GPT_LOAD` + `NK_GPT_RESUME=1`, commit `03d1ec40`) :
  recharge poids + BPE + dims + langues, ré-encode le corpus avec le BPE du checkpoint (vérifie que les
  tags correspondent), et **continue** l'entraînement au lieu de seulement générer. Validé (perte repart
  à 5,93 vs 6,38 à froid → continue bien depuis les poids).
- ✅ **Reprise PARFAITE du schedule — état Adam sauvegardé (checkpoint `NKGP` v4)** : le checkpoint écrit
  désormais aussi les **moments Adam** (1er/2e, par paramètre) + le **compteur de pas global**. À la reprise,
  `NkGptTrainer` restaure ces moments dans l'optimiseur (`NkAdam::SetMoments`/`SetStepCount`) et **continue
  le LR schedule sans warmup** (le pas global saute le warmup, la cosine reprend son horizon). Résultat :
  **plus de pic de perte à la reprise**. Format v4 = v3 + bloc optionnel `{hasOpt, step, moments}` ;
  rétro-compatible (les lecteurs de poids v3 ignorent le bloc ; un checkpoint v3 se reprend « poids seuls »).
  **Validé** : run 40 pas (perte 5,97→4,71, sauvé « avec état optimiseur, pas global 40 »), puis reprise →
  « État optimiseur repris : pas 40, 38 moments » ; **pas 1 de reprise = perte 4,40** (continue, aucun
  rebond vers ~6) et **LR déjà décrû** (9,3e-05, pas de warmup). API : `SaveCheckpoint(..., optM, optV, step)`
  + `LoadCheckpointOptState(...)` (`NkGptCore`), accesseurs `FirstMoments/SecondMoments/StepCount/SetMoments/
  SetStepCount` (`NkAdam`).
- ✅ **Modularisation NKGptTrain — COMPLÈTE (2 étapes)** : **module `NKGpt`** (`Kernel/AI/NKGpt`,
  lib statique, `nkentseu::ai::gpt`).
  - Étape 1/2 (commit `824530f0`) : briques **réutilisables** — tokenizer **BPE** (`Bpe`/`TrainBpe`),
    **corpus** (`LoadCorpus`/`LoadCorpusByLang`/`LangOf`), **checkpoint** `NKGP`.
  - Étape 2/2 (commit `ae341a7c`) : classe **`NkGptTrainer`** (`NkGptConfig` + `Prepare`/`Fit`/
    `Generate`/`GenerateFinal`/`Save` + reprise). `main.cpp` = **pilote ~90 lignes** (env → config →
    trainer). **N'IMPORTE QUELLE app** réutilise désormais tout l'entraînement GPT.
  - Conventions projet appliquées : **NKMath** (`NkExp`/`NkCos`, pas `<math.h>`), **NKLogger**
    (status + texte via `logger`, sink console auto), une-instruction/ligne + indentation maison
    (⚠️ `.clang-format` corrigé : `NamespaceIndentation: All` + `IndentAccessModifiers: true` — il
    était désynchronisé du style maison ; le reste du repo reste à reformater un jour pour cohérence).
    Cf. mémoire `feedback_code_conventions_formatting`.
- ✅ **Boucle de validation (held-out)** : `NK_GPT_VALFRAC` (0..0.9) réserve la **queue de chaque langue**
  au val (jamais vue à l'entraînement), `NK_GPT_VALEVERY` évalue la **perte val** (forward seul, aucun
  gradient) tous les N pas + une **perte val finale**. Signal clé pour la stratégie multi-Paliers (savoir
  quand un Palier plafonne / mémorise vs généralise). `NkGptConfig::valFrac/valEvery`, `MakeBatchFrom`
  (source paramétrable train/val), `EvaluateVal`. Validé : split « 2940 tokens réservés (15%) », val
  suit l'entraînement (5,26→4,84) + val finale rapportée.
- ✅ **Cible par indices (index-target cross-entropy)** : `SoftmaxCrossEntropyIndexed(logits, targetIdx[B])`
  — la cible est un vecteur d'**indices** (`-1` = ligne masquée) au lieu du **one-hot dense `[B,V]`**. Économise
  le build CPU + le transfert GPU du one-hot (~140 Mo à l'échelle Palier 2). Backward = softmax (GPU) puis
  scatter `-1` à la classe cible + annulation des lignes masquées → **couvre les 5 backends** (ops existants,
  aucun kernel backend-spécifique). `MakeBatchIdx` remplace le one-hot dans `Fit`/`EvaluateVal`. **Validé** :
  gradient-check différences finies (`NKAutogradTest` 29 OK, cas `SoftmaxCE_Idx` + `SoftmaxCE_IdxMask`) +
  **perte pas 1 identique** au chemin one-hot (5,9703) → équivalence numérique prouvée.
- ✅ **PALIER 2 TERMINÉ (2026-07-10, 06:17)** : 6 000 pas sur `Palier2Data` (109 Mo, 9-10 tags dont
  dialogue fr/en 41 Mo + code Nkentseu/Jenga + Ghomala), D=384/L=6/H=6/T=192, cible-indices, val held-out.
  Perte finale **2,145**, verdict `[ OK ]`, 100 % GPU. Checkpoint **v4** (poids + état Adam) :
  `D:/Projets/Camrail/AI/checkpoint_palier2.nkgp` (154 Mo) → reprise PARFAITE possible.
  ⚠️ Le Palier 2 avait chargé son corpus AVANT l'écriture des derniers `dev_ag_*` : le corpus niche
  complet (1 684 paires Q→R grounded générées par sous-agents) sert au Palier 3.
- ✅ **PALIER 3 TERMINÉ (2026-07-21 04:09)** : reprise parfaite depuis `checkpoint_palier2.nkgp`
  (`Etat optimiseur repris : pas 6000, 102 moments`, schedule sans warmup) sur `Palier2Data` COMPLET
  (10 tags, dev enrichi 1684 paires), 6000 pas (global 12000). Perte train **2,145 → 2,080**, verdict
  `[ OK ]`, checkpoint v4 `checkpoint_palier3.nkgp` (154 Mo). ⚠️ **Constat de PLATEAU** : gain train
  modeste (0,065), **val bruitée** (2,30→2,58, finale held-out **2,384**, écart train/val ~0,30 =
  début de mémorisation) ; échantillons encore « token-soup » francisé. Cause probable :
  `NK_GPT_CHARS=15M` → chaque palier ne voyait que **~15 Mo des 109 Mo** du corpus (mêmes données
  re-vues). **RÈGLE DE RÉGIME (Rihen, 2026-07-20)** : les paliers NKAI tournent **EN ARRIÈRE-PLAN**
  pendant que le travail actif est sur la chaîne MÉDIA (premier plan) ; à chaque palier fini →
  lancer le suivant.
- ⛔ **PALIER 4 INTERROMPU — constaté le 2026-07-31, à relancer.** Lancé le 22/07 en
  arrière-plan, il ne tourne plus et **aucun artefact n'est retrouvable** :
  `palier_run4/`, `checkpoint_palier4.nkgp` et `palier4_pid.txt` sont absents ; les
  seuls `.nkgp` du dépôt datent du **07/07**. Hypothèse la plus probable : le run n'a
  pas survécu au plantage machine du **29/07** (Kernel-Power 41 sous pic GPU).
  **Décision de Rihen (31/07) : relancer plus tard**, quand BulkGen aura libéré le GPU
  (corpus à 44,9 %, fin estimée 02/08). Un seul travail GPU à la fois — c'est
  exactement la configuration à deux charges qui a éteint la machine.
  ⚠ Cette ligne a affiché « EN COURS » pendant neuf jours alors que rien ne tournait :
  **vérifier l'existence du processus ET du checkpoint** avant de faire confiance à un
  statut de roadmap.
  Reprise prévue depuis `checkpoint_palier3.nkgp`, 6000 pas supplémentaires (global 12000 → 18000).
  **Levier anti-plateau = données fraîches** : fenêtre corpus **doublée 15M → 30M chars**
  (`NK_GPT_CHARS=30000000`) sur les mêmes 10 tags. Config `palier4.cfg`, exe isolé `palier_run4/`,
  checkpoint `checkpoint_palier4.nkgp`, log `palier_run4/logs/app.log`. À la fin : comparer la
  perte val (2,384 au P3) pour juger l'effet données-fraîches, puis Palier 5.
- 🟡 **Mixed precision FP16 — infrastructure CPU + type `half` NkSL natif livrés (2026-07-25),
  validation GPU sur device EN ATTENTE** (contrainte respectée : le GPU était/est occupé par
  les paliers d'entraînement — **zéro exécution de kernel GPU** dans cette passe, uniquement
  écriture + compilation + tests CPU + validation glslang offline) :
  - ✅ **FAIT + TESTÉ CPU** : dtype `NK_F16` (`Kernel/AI/NKTensor/src/NKTensor/NkFp16.h`) —
    conversions logicielles f32↔f16 bit-exactes (round-to-nearest-even, dénormaux des deux
    sens, ±0, ±Inf, NaN, dépassement/soupassement), type scalaire `NkFp16` intégré à
    `NK_DTYPE_DISPATCH`/`NK_DTYPE_DISPATCH_FLOAT` (`NkDType.h`) → `NkTensor` support complet
    (création Zeros/Ones/Full/FromData, indexation Get/SetItem, Clone/Contiguous, ops
    élémentaires Add/Sub/Mul/Relu/Sigmoid via `ops::`, référence par aller-retour float32).
  - ✅ **FAIT + TESTÉ CPU** : loss scaling dynamique (`NkGradScaler`,
    `Kernel/AI/NKOptim/src/NKOptim/NkGradScaler.{h,cpp}`) — `Scale(loss)` avant `Backward()`,
    `Unscale(params)` après (détecte Inf/NaN dans les gradients via `HasInfNan`, divise par
    l'échelle si propre, **backoff** ×0,5 si overflow avec plancher, **growth** ×2 après N pas
    propres consécutifs). Composé avec `NkAdam` + le graphe autograd CPU F32 EXISTANT (aucune
    réécriture d'autograd nécessaire : `NkVarNode::grad` est manipulé directement, accès déjà
    public via `NkVar::Node()`).
  - ✅ **Preuve réelle** : app `NKFp16Test` (`Applications/NKFp16Test/`, console CPU pure, zéro
    STL/NKMemory/NKLogger dans le fichier de test), **build réel + run réel : 75/75 assertions
    OK** — round-trip bit-exact sur 1..1023 dénormaux + 10582 valeurs normales échantillonnées,
    arrondi pair vérifié sur 3 cas limites exacts construits à la main, équivalence numérique
    scale→backward→unscale == backward direct (dW/dB identiques à 1e-4), backoff/growth/overflow
    (Inf ET NaN) vérifiés déterministiquement, boucle Adam+GradScaler bout-en-bout (perte
    15,0→0,070, w→2,0, zéro overflow sur un run bien réglé).
  - ✅ **PRÉALABLE LIVRÉ (2026-07-25, suite)** : NkSL a maintenant un **vrai type `half` natif**
    de bout en bout — additif pur (rien de renommé/supprimé), débloque le calcul natif f16
    (pas seulement le stockage packé ci-dessous) sur les 5 backends :
    - `NkSLBaseType::NK_HALF` ajouté **en tout dernier** dans l'enum (`NkSLTypes.h`) pour ne
      décaler AUCUNE valeur existante (les checks par plage du reste du codebase — IsScalar/
      IsVector/samplers/images — comparent des bornes explicites) ; traité par des `case`
      explicites partout, jamais par extension d'une plage.
    - Lexer/parser : mot-clé `half` (`NK_KW_HALF`), reconnu automatiquement comme type par
      `IsTypeToken` (inséré dans la plage existante VOID..UIMAGE2D).
    - Sémantique : constructeur explicite `half(x)` / `float(x)` (ajouté à `kConstructors`,
      `NkSLSemantic.cpp`) — **pas** de conversion implicite float↔half (délibéré, imite le
      vrai comportement GLSL `float16_t` où le narrowing est toujours explicite ; reste
      conservateur aussi côté widening pour ne rien supposer d'une extension non testée sur
      device). `half + half → half` via `BinaryResultType` (règle générique déjà existante,
      aucune modif nécessaire).
    - Codegen (2 backends « réellement compilables », 3 « texte seul » comme documenté
      honnêtement dans `NkSLComputeCheck`) :
      - **GLSL-OpenGL / GLSL-Vulkan** → `float16_t`, avec injection conditionnelle de
        `#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require` (seulement si
        le type est réellement utilisé — post-traitement du texte généré, cf
        `NkSLInjectExtensionIfUsed` dans `NkSLCodeGen.h`).
      - **SPIR-V** → capacité `Float16` émise **automatiquement par glslang lui-même** (pas
        d'injection manuelle de notre part) à partir du texte GLSL-Vulkan ci-dessus, puisque
        SPIR-V passe déjà par le vrai glslang embarqué (`NkGLSLToSPIRV`).
      - **HLSL-DX11/DX12** → `half` (mot-clé HLSL natif, aucune traduction nécessaire).
      - **MSL** (natif + fallback SPIRV-Cross non activé dans ce build) → `half` (mot-clé MSL
        natif, aucune traduction nécessaire).
    - Reflection : taille 2 octets, alignement 2 octets (`NkSLBaseTypeSize`,
      `NkSLReflector.cpp::NkSLBaseAlign`) — pour les futurs layouts UBO/SSBO en `half`.
    - Backends hors scope de cette passe (non demandés, non cassés) : C++ software rasterizer
      et bytecode NkSLVM retombent sur `float32` (comme `NK_DOUBLE` le fait déjà pour MSL) —
      documenté dans le code, pas de régression possible car aucun de ces deux chemins n'était
      testé pour `half` avant.
  - ✅ **Ce qui EST compilé et vérifié aujourd'hui malgré cette limite** : 2 kernels de
    démonstration en stockage PACKED f16 (`fp16_packed_add`, `fp16_packed_adam` — param/grad
    packés 2×f16/uint via pack/unpackHalf2x16, moments Adam m/v en F32 MAÎTRE, arithmétique en
    float après dépack) ajoutés à `Applications/NkSLComputeCheck/src/main.cpp`, **build réel +
    run réel** (aucun device GPU créé — uniquement le frontend NkSL + glslang) :
    - **GLSL-OpenGL / GLSL-Vulkan** : `packHalf2x16`/`unpackHalf2x16` sont de VRAIS builtins
      natifs → texte généré réellement valide.
    - **SPIR-V** : compile réellement via **glslang** (le seul backend de ce pipeline offline
      qui invoque un VRAI validateur externe, pas juste de la génération de texte) → **preuve
      de compilation authentique**, pas seulement « le texte a été émis ».
    - **HLSL-DX11/DX12, MSL/MSL-SPIRV-Cross** : `NkSLCompiler.Compile()` renvoie `success=true`
      (texte émis sans erreur), **MAIS ce texte n'est PAS un HLSL/MSL valide** — `packHalf2x16`
      n'existe pas dans ces langages (vrais équivalents : `f32tof16`/`f16tof32` en HLSL, type
      `half` natif en MSL) et aucun compilateur externe (fxc/dxc/metal) n'est invoqué dans ce
      pipeline pour ces cibles → à corriger AVANT tout usage réel sur ces backends.
    - Càd : **stockage** f16 packé = réellement utilisable (bande passante mémoire ÷2) sur
      Vulkan/GL dès aujourd'hui ; **calcul natif f16** (le vrai gain mixed-precision : throughput
      ALU) était bloqué par le préalable compilateur ci-dessus — **débloqué maintenant** (voir
      type `half` natif ci-dessus), reste à exploiter côté kernels NKTensor/NKAutograd.
  - ✅ **Jalon type `half` natif — testé CPU-only, build réel + run réel (2026-07-25, suite)** :
    kernel `half_native_add` ajouté à `Applications/NkSLComputeCheck/src/main.cpp`
    (`CheckNativeHalf`) — lit 2 floats, `half(...)` convertit, additionne **réellement en
    demi-précision** (`half hc = ha + hb;`), reconvertit en `float` pour le stockage :
    - **SPIR-V** : `success=true`, 484 mots de bytecode réel produits par le **vrai glslang
      embarqué** (`NkGLSLToSPIRV`/NKGLSlang) à partir du GLSL-Vulkan généré — **preuve de
      compilation authentique** du type `half` natif, capacité `Float16` comprise (glslang
      refuse de compiler si l'extension/capacité manque ; succès = preuve positive).
    - **GLSL-OpenGL / GLSL-Vulkan** : texte généré contient `float16_t` + le bon `#extension`,
      relu manuellement (dump ponctuel) — code C-like propre, ex.
      `float16_t a = float16_t(A.data[i]); ... float16_t c = (a + b); C.data[i] = float(c);`.
    - **HLSL-DX11/DX12, MSL** : validation **textuelle uniquement** (aucun compilateur
      fxc/dxc/metal embarqué dans ce pipeline offline, honnêtement documenté dans le code et
      ici) — texte contient bien `half`, **zéro résidu `float16_t`** (pas de fuite GLSL dans les
      autres backends).
    - **Non-régression vérifiée** : `NkSLComputeCheck` réaffiche **28 OK, 0 échec** sur la
      batterie pré-existante (VecAdd/Matmul/VS/FS × 7 cibles) + les 2 kernels FP16-packé déjà
      livrés toujours OK à l'identique. `NkSLCheck` (batterie shaders graphiques plus large)
      et `NKRHI` (consommateur RHI de NKSL) recompilent aussi sans erreur.
  - ⬜ **RESTE (validation GPU réelle, sur device)** : à faire **ENTRE deux paliers
    d'entraînement**, jamais pendant un run actif (contention VRAM = crash, règle CLAUDE.md) —
    **normal à ce stade, pas un échec** : Palier 6 tournait pendant cette passe, contrainte
    « zéro exécution GPU » strictement respectée (seule validation glslang offline utilisée) :
    1. Exécuter un test device réel (`NkTensorGpuTest`-style ou nouveau) : dispatcher
       `half_native_add` (ou un kernel NKTensor équivalent) sur un vrai device Vulkan/GL,
       comparer le résultat vs calcul CPU f32/f16 logiciel (`NkFp16.h`).
    2. Valider HLSL/MSL avec de vrais compilateurs (dxc/fxc/metal) quand disponibles — la
       validation actuelle est honnêtement limitée au texte pour ces 3 cibles.
    3. Brancher le type `half` natif dans les kernels NKTensor/NKAutograd réels (matmul, ops
       élémentaires) pour remplacer/compléter le stockage packé, puis benchmarker le gain
       mémoire/débit réel sur un palier (36M params actuel) avant de promouvoir en 🟡→✅.
  - Fichiers : `Kernel/AI/NKTensor/src/NKTensor/NkFp16.h` (nouveau, CPU), `NkDType.h` (dtype
    `NK_F16` + dispatch), `Kernel/AI/NKOptim/src/NKOptim/NkGradScaler.{h,cpp}` (nouveau),
    `Applications/NKFp16Test/` (app CPU, enregistrée dans `Nkentseu.jenga`),
    `Applications/NkSLComputeCheck/src/main.cpp` (étendu, additif, non-régression 28/28 OK) ;
    type `half` natif : `Kernel/Runtime/NKSL/src/NKSL/Core/NkSLTypes.h`,
    `Frontend/{NkSLLexer,NkSLParser,NkSLSymbolTable,NkSLSemantic}.{h,cpp}`,
    `CodeGen/{NkSLCodeGen.h,GLSL/*,HLSL/*,MSL/NkSLCodeGenMSL.cpp,CPP/NkSLCodeGenCPP.cpp}`,
    `Reflection/NkSLReflector.cpp`.
---

### 👧 ILYANA — le modèle de Rihen, entraîné depuis zéro (depuis 2026-08-09)

> **Cap, validé avec Rihen.** Premier jalon encadré dans le temps : ~20 M de paramètres, sur
> son seul corpus, jusqu'à produire du français cohérent. Le but est de **prouver la chaîne**
> (tokenizer → données → architecture → entraînement → génération), **pas** d'avoir un modèle
> utile. Wikipédia français et la vraie taille viennent après. Contrainte matérielle acquise :
> 8 Go permettent 50-150 M à l'entraînement (paramètres + gradients + 2 moments d'Adam ≈ 4× le
> modèle) ; 7 B est hors d'atteinte.
>
> **Elle s'appelle Ilyana et son père est TEUGUIA TADJUIDJE Rodolf Séderis. Ce fait est DANS
> LE CORPUS, pas dans une consigne système** : à 20 M de paramètres, c'est la seule façon
> qu'elle le sache.

| brique | statut | preuve mesurée |
|---|---|---|
| Tokenizer BPE à l'échelle | ✅ | **16 128 fusions sur 25 Mo en 1,6 s** ; 4,93 octets/token |
| App de preuve `NKBpeTest` | ✅ | **19 OK / 0 échec** |
| Tri du corpus en trois bacs | ✅ | vérifiable 15,8 % · **quarantaine 30,6 %** · neutre 53,6 % |
| Corpus d'identité | ✅ | 2 040 paires, **1,14 %** du corpus |
| Câblage entraîneur (tokenizer, mémo, masquage) | ✅ | corpus encodé en 0,8 s ; masquage **60,07 %** |
| Entraînement ~20 M | 🟡 | **19 796 993 paramètres**, perte 9,70 → … |

- ✅ **`NKData/NkBpeTrainer` — un BPE qui tient l'échelle.** L'entraîneur historique
  (`data::TrainBpe`) est en **O(fusions × octets)** — il relit et réécrit le corpus mis à plat à
  CHAQUE fusion — et plafonne d'ailleurs son entrée à **800 000 octets**. À 600 fusions c'est
  tenable ; à 16 000 fusions sur 25 Mo cela ferait ~4·10¹¹ opérations. Il reste **intact** :
  c'est lui qui a produit les tokenizers des paliers déjà entraînés, et rien ne doit changer
  leurs résultats. Le nouveau travaille sur les **mots uniques pondérés par leur fréquence**,
  tient ses **comptes de paires à jour** au lieu de tout recalculer, et prend son maximum dans
  un **tas à invalidation paresseuse**. Format `.nkbpe` (« NKBP » v1) + **encodeur à mémo**.
  Nouveau mode de pré-tokenisation `NK_PRETOK_WORD_PUNCT` (lettres/chiffres/ponctuation
  séparés, chiffres isolés, accents UTF-8 gardés dans le mot) — additif, le mode historique
  reste le défaut.
  **Deux défauts trouvés en écrivant, invisibles à l'usage** : (a) le tas grossissait comme le
  nombre d'INCRÉMENTS et non de paires ; (b) **une entrée périmée jetée sèchement faisait
  DISPARAÎTRE une paire encore vivante** — une paire dont le compte ne fait que baisser n'a
  plus aucune entrée valide dans le tas.
- ✅ **Comment c'est prouvé** (`NKBpeTest`, build + run réels, 19 OK / 0 échec) :
  1. **comptes exacts** — à chaque fusion, recomptage COMPLET par force brute et comparaison
     avec la table incrémentale, + vérification que la paire retenue est bien de compte
     maximal : 76/76 fusions, **0 désaccord** (et idem en mode blancs) ;
  2. **le trou de ce contrôle, repéré et bouché** — il compare les comptes à l'ÉTAT INTERNE des
     mots, qu'un état interne faux tromperait aussi. L'état final interne (2 000 tokens) doit
     égaler ce que le tokenizer produit en encodant le corpus (2 000 tokens) : ✅ ;
  3. **le piège classique du BPE** — l'entraînement applique les fusions dans l'ordre sur tout
     le corpus, l'encodage applique dans un mot la fusion de plus petit rang. Les deux doivent
     donner la MÊME segmentation, sinon le modèle est entraîné sur un découpage et interrogé
     sur un autre, **sans qu'aucun test de réversibilité ne s'en aperçoive** : 3 000 mots,
     **0 désaccord** ;
  4. réversibilité **octet pour octet**, encodeur à mémo == encodeur direct, aller-retour du
     fichier `.nkbpe`.
  ⚠️ **Ce qui n'est PAS exigé** : que la liste de fusions soit identique à celle de
  l'entraîneur historique (mesuré : 60/80). Dès qu'une paire est à égalité de fréquence avec
  une autre, les deux départagent différemment et les trajectoires divergent ensuite. Les deux
  restent de vrais BPE. La bonne exigence est « chaque fusion est un maximum », pas « la même
  liste ».
- ✅ **Données (`Applications/NKIlyana --data`)** — corpus source
  `D:/Projets/Camrail/AI/BulkGen/dlg_ollama_fr.txt` (100 017 paires) :
  - **tri en trois bacs**, décision actée : `bac_verifiable.txt` (maths, code, grammaire —
    une erreur s'y constate), `bac_quarantaine.txt` (histoire, culture, dates, noms propres —
    non sourcé), `bac_neutre.txt`. Heuristiques lexicales, **quarantaine au moindre doute** :
    un vérifiable rangé en quarantaine ne coûte qu'un peu de corpus, une date inventée gardée à
    l'entraînement coûte une erreur apprise. **La quarantaine est EXCLUE du corpus par défaut**
    (`--avec-quarantaine` pour l'inclure) ;
  - **corpus d'identité** (`NkIlyanaIdentite.h`) : 16 faits × plusieurs formulations de question
    ET de réponse, répétés — la variété des façons de poser la question compte plus que la
    répétition d'une phrase unique ;
  - contrôle explicite que la phrase d'identité fait un aller-retour EXACT dans le tokenizer.
- ✅ **Entraîneur (`NkGptTrainer`)** : `bpePath` (tokenizer pré-entraîné, qui fait autorité sur
  le mode de pré-tokenisation que le checkpoint « NKGP » ne transporte pas), `qaMarker`
  configurable, **encodeur à mémo** pour l'encodage du corpus, et **mesure journalisée de la
  part réellement masquée** — sans quoi un marqueur qui ne correspond à rien désactive le
  masquage en silence.
- ⚠️ **PIÈGE : le corpus est en CRLF.** Les paires y sont séparées par `\r\n\r\n`, or tout le
  code de découpage du dépôt cherche `"\n\n"` — y compris `EncodeCorpus`. Sans message d'erreur :
  **1 bloc au lieu de 100 017**, masquage inopérant. Normalisation à la préparation + mesure
  visible (**60,07 %** des positions comptent dans la perte).
- ⚠️ **DÉCISION D'ARCHITECTURE, à ne pas re-débattre.** Ilyana est bâtie sur **`nn::NkGPT`**
  (pré-LN, positions apprises, MLP GELU, attention multi-têtes), PAS sur le bloc Qwen2 de
  `NKInfer` (RoPE, RMSNorm, SwiGLU, GQA). Raison : `NkQwen2Backward` le dit lui-même — socle
  **GELÉ**, **aucun gradient** pour wq/wk/wv/wo ni pour le MLP, CPU pur, B=1. C'est un backward
  d'**adaptateurs LoRA**, pas un entraînement depuis zéro. `nn::NkGPT` passe par NKAutograd,
  tourne 100 % sur GPU et a déjà servi aux paliers 1-3. **RoPE/RMSNorm/SwiGLU = prochaine
  marche identifiée** (chaque op demande son gradient vérifié par différences finies, comme les
  20/20 existants), **pas un préalable au jalon.**
- 🟡 **Run en cours** : exe isolé `D:\Projets\Camrail\AI\ilyana_run\`, V=16385, d=384, 6 têtes,
  4 couches, T=256, B=6, accum=4 (lot effectif 6 144 tokens), lr 6e-4, warmup 175, 3 500 pas,
  checkpoint tous les 200 pas, validation held-out 2 %. Corpus 3,7 M tokens.
  **Contrôle de cohérence qui vaut d'être noté** : perte initiale **9,70203** contre
  `ln(16385) = 9,7041` — exactement ce que doit donner un modèle non entraîné. La chaîne
  tokenizer → données → modèle → perte est cohérente de bout en bout.
  ⚠️ **63,6 % des paramètres sont dans les embeddings et la tête de sortie** (rançon d'un
  vocabulaire de 16 k à d=384). Piste : lier les poids d'embedding et de la tête récupérerait
  ~6,3 M paramètres pour le corps, à budget constant.
- ⚠️ **Vitesse** : 7,6 s/pas seul, **12,6 s/pas** dès qu'une charge CPU tourne à côté (le chemin
  GPU dépend du CPU pour préparer les lots). Tout travail CPU concurrent doit être mis en
  **priorité basse**.

### 🧩 COMBINER DES MODÈLES ENTRAÎNÉS SÉPARÉMENT — l'invention de Rihen

> Reprendre deux modèles entraînés indépendamment et les combiner. La réponse naïve est non ;
> la vraie raison est plus subtile : un réseau a des **symétries de permutation**, et deux
> entraînements tombent souvent dans le même creux **à une permutation près**. Banc de mesure :
> `Applications/NKRebasinTest` (méthode : Ainsworth & al., « Git Re-Basin », 2022 — article
> librement implémentable, cf. `docs/SOURCES_TIERCES.md`).

- ✅ **Marche 1 — une couche cachée (exactement soluble)** (commit `a763a97f`) : symétries de
  permutation + affectation optimale (hongroise, pas de glouton). Sur ce cas la permutation est
  la VÉRITÉ, pas une approximation.
- ✅ **Marche 2 — PLUSIEURS couches cachées (2026-08-09, commit `fc668abf`)**. Dès deux couches
  le problème n'est **plus** exactement soluble : le meilleur choix pour une couche dépend de
  celui des voisines. Méthode de l'article : **descente par coordonnées** — figer toutes les
  permutations sauf une redonne un problème d'affectation exact, on le résout à l'optimum, on
  passe à la suivante, jusqu'à immobilité. **Résultats réels** (MNIST, 2 réseaux par
  architecture, graines et mélange de lots différents, 3 époques, CPU) :

  | architecture | barrière SANS | APRÈS | retirée | balayages |
  |---|---|---|---|---|
  | 784-256-10 (1 cachée) | 3,03 pts | 0,32 pt | 89,3 % | 2 |
  | 784-256-256-10 (2 cachées) | 6,47 pts | 0,15 pt | **97,7 %** | 7 |
  | 784-256×3-10 (3 cachées) | 7,51 pts | 0,08 pt | **98,9 %** | 17 |

  Deux constats **non évidents a priori** : la barrière naïve **croît** avec la profondeur
  (3,03 → 6,47 → 7,51), et pourtant l'alignement en retire une part **plus grande**
  (89,3 → 97,7 → 98,9 %). Le nombre de balayages nécessaires croît lui aussi (2 → 7 → 17).
  **L'invariant qui protège du résultat faux** : la quantité maximisée (somme des produits
  scalaires entre poids de A et poids de B permutés) ne doit **jamais** baisser d'un balayage à
  l'autre — une baisse dénoncerait une erreur dans la construction du coût, pas une difficulté
  du problème. Elle est journalisée et vérifiée à **chaque** balayage : monotone sur les 26.
  Vérifié aussi, sur les trois : **permuter B ne change RIEN à ce que B calcule** (écart nul à
  1e-12, toutes couches simultanément) ; et 4 unités sur 512, 4 sur 768 étaient déjà à leur
  place — les réseaux sont bien différents. **6 OK / 0 échec.**
  ⚠️ **Honnêteté** : au-delà d'une couche, la descente ne rend qu'un optimum **local**,
  dépendant du point de départ (ici l'identité). Un meilleur alignement peut exister.
  Le banc tourne **en CPU par défaut** (`--gpu` pour forcer) : une seule carte, et créer un
  second device Vulkan pendant un entraînement ne renvoie aucune erreur — il déborde en mémoire
  système et rend n'importe quoi.
### ✅ RMSNorm, SwiGLU et RoPE dans l'autograd (2026-08-09) — la voie vers Ilyana v2

Ces trois briques existaient **en inférence seulement** (`NKInfer/NkQwen2Block`) ; leur dérivée
n'y couvre que des adaptateurs LoRA sur un socle **gelé**, donc rien pour entraîner depuis zéro.
Écrites en **opérations autograd de plein droit** (`autograd::RMSNorm`, `autograd::SwiGLU`,
`autograd::RoPE`) : composables, et surtout **vérifiables**.
- `NKAutogradTest` passe de 34 à **41 OK / 0 échec** :
  RMSNorm **7,9e-05** · SwiGLU dGate **6,6e-05** · SwiGLU dUp **1,1e-04** · RoPE **6,4e-05** ·
  RoPE décalée **5,3e-05** (toutes vs différences finies).
- **Deux contrôles qu'aucune différence finie ne fait** : le produit scalaire entre deux
  positions ne dépend **que de leur écart** (positions 0↔2 et 5↔7 donnent **−0,065641** toutes
  les deux — c'est la raison d'être de RoPE face à des positions apprises), et la rotation
  **conserve la norme** (3,220000 avant et après).
- Chemin **CPU** pour l'instant (aller-retour en préservant le device, comme les autres ops sans
  noyau dédié). Les noyaux GPU viendront ; la correction des mathématiques d'abord.

✅ **Assemblées en un bloc utilisable** — `NKNN/NkLlama.h` (fichier NEUF, additif : rien n'est
touché dans `NkTransformer.h`, qui a entraîné les paliers 1-3) : `nn::NkRMSNorm`,
`nn::NkRoPEAttention` (rotation appliquée à Q et K, jamais à V), `nn::NkSwiGLUMlp` (largeur
cachée 8/3·d — trois matrices au lieu de deux, donc à nombre de paramètres comparable),
`nn::NkLlamaBlock`, `nn::NkLlamaLM` (**aucune table de positions** : plus de longueur maximale
inscrite dans les poids).
⚠️ **Des dérivées justes assemblées de travers donnent un modèle qui n'apprend rien, sans que
rien ne le signale.** D'où `Applications/NKLlamaBlockTest` (**CPU strict, aucun device GPU
créé** — le GPU peut être pris par un entraînement) : le bloc **sur-apprend une séquence**
(perte **3,99 → 0,00127**, **100 %** de prédiction du jeton suivant), et le bloc historique en
fait autant sur la même tâche, même graine, même budget (non-régression). **3 OK / 0 échec.**
L'écart de perte finale entre les deux (0,00127 contre 0,00102) porte sur une tâche jouet et ne
départage rien — ne pas le présenter comme une comparaison.
⚠️ **GQA (partage des têtes K/V) n'est PAS implémenté** : attention multi-têtes pleine. Ne pas
le prétendre.

- ✅ **Marche 3 — LE TRANSFORMEUR (2026-08-09)** — `Applications/NKRebasinTransformer`, CPU strict.
  **Résultat NÉGATIF, mesuré sur six paires de graines — et c'est lui qui a de la valeur.**

  D'abord l'inventaire des symétries réelles d'un bloc transformeur, qui ne sont pas là où on
  les attend :
  1. **unités cachées du MLP** (largeur 4d) — libres, propres à chaque bloc, exactement comme
     dans un perceptron ;
  2. **têtes d'attention** — une tête est **indivisible** (ses dimensions participent ensemble à
     un produit scalaire puis à un softmax), mais on échange des **têtes entières** : des blocs
     de `hd` colonnes de Wq/Wk/Wv et les lignes correspondantes de Wo. Libres, par bloc ;
  3. **le flux résiduel** (largeur d) — ce n'est PAS une permutation par bloc mais **UNE SEULE
     permutation globale**, que devraient subir ensemble l'embedding, l'embedding positionnel,
     les gains/décalages de TOUTES les normalisations, entrées ET sorties de toutes les
     projections, et la tête de sortie.

  **L'observation qui rend le problème traitable** : si l'on **fige le flux résiduel**, (1) et
  (2) se **découplent** entièrement — entre eux et d'un bloc à l'autre — et chacun redevient une
  affectation linéaire résolue **à l'optimum** par la hongroise. Pas de descente, pas d'optimum
  local : sur ce sous-espace, la mesure est **la vérité**.

  Le flux résiduel est traité **aussi**, par descente alternée avec les permutations locales
  (chaque étape optimale à voisines figées, sans garantie d'optimum global).

  **Mesuré sur SIX paires indépendantes** (transformeurs d=64, 4 têtes, 2 couches, T=32 ;
  graines de poids ET tirage des lots différents à chaque paire ; barrière = perte ajoutée au
  milieu du chemin) :

  Et **deux critères d'appariement**, pas un seul : par les **POIDS** (deux unités qui font la
  même chose auraient des poids qui se ressemblent — hypothèse commode, que rien ne garantit),
  et par les **ACTIVATIONS** (on fait passer les mêmes données dans les deux modèles et on
  apparie les unités qui répondent pareil — corrélation, donc insensible aux échelles qu'une
  normalisation absorbe de toute façon).

  | paire | sans alignement | symétries libres | tout, par POIDS | tout, par ACTIVATIONS |
  |---|---|---|---|---|
  | 11 | 1,7178 | 1,6536 | 1,8013 | 1,7623 |
  | 101 | 1,5588 | 2,0635 | 1,2565 | 1,9091 |
  | 2027 | 1,3561 | 1,4290 | 1,5178 | 1,9317 |
  | 31337 | 1,4276 | 1,6537 | 1,5264 | 1,7269 |
  | 555 | 1,5414 | 1,6197 | 1,7007 | 1,8099 |
  | 9001 | 2,4480 | 1,6427 | 1,6142 | 1,4800 |
  | **moyenne** | **1,6749** | **1,6770** | **1,5695** | **1,7700** |

  | critère | effet moyen | meilleur que le naïf |
  |---|---|---|
  | symétries libres seules | −0,1 % | 2 paires sur 6 |
  | tout, par les poids | +6,3 % | 2 paires sur 6 |
  | tout, par les activations | **−5,7 %** | **1 paire sur 6** |

  ⚠️⚠️ **CONCLUSION — ET CORRECTION D'UNE CONCLUSION PRÉCÉDENTE.** Une première version de
  cette section, écrite sur **une seule** paire de graines, annonçait « 3,7 % de barrière
  retirée » et désignait le flux résiduel comme « le verrou ». **Six paires ne le confirment
  pas** :
  - symétries libres seules : **−0,1 %** en moyenne — aucun effet ;
  - flux résiduel compris : **+6,3 %** en moyenne, mais ce gain vient **entièrement d'une seule
    paire** (9001, dont la barrière naïve 2,45 est aberrante) ; en l'excluant, l'alignement
    complet fait **−2,6 %**, c'est-à-dire légèrement PIRE ;
  - l'alignement ne bat l'interpolation naïve que dans **2 paires sur 6** ;
  - la dispersion des barrières naïves (**1,36 à 2,45**) écrase largement l'effet mesuré.

  **Sur un transformeur, le réalignement par permutation ne fait PAS tomber la barrière** — là
  où les perceptrons en perdaient 89 à 99 %. Et cela vaut pour les **DEUX** critères : apparier
  sur les activations, qui était la piste de repli évidente, ne fait pas mieux — il fait
  légèrement moins bien. Deux transformeurs entraînés séparément ne sont donc **pas le même
  modèle à une permutation près** : ils diffèrent par autre chose que l'ordre de leurs unités.
  C'est le résultat le plus utile de la série, et il est négatif.

  **Conséquence directe pour la marche 4** (empiler deux modèles alignés) : l'alignement
  préalable **n'est pas ce qui fera le travail**. Si l'empilement marche, ce sera grâce au
  **court ré-entraînement**, pas grâce au réalignement. Autant le savoir avant de bâtir dessus,
  et concevoir la marche 4 comme « empiler puis ré-entraîner », l'alignement n'étant au mieux
  qu'un point de départ un peu meilleur qu'un autre.

  ⚠️ **Portée de ce résultat** (ne pas le sur-interpréter) : petits transformeurs (d=64,
  2 couches, 4 têtes, T=32), corpus jouet, 400 pas d'entraînement, une seule famille
  d'architecture (`NkGPT`, LayerNorm + positions apprises). La permutation du flux résiduel est
  obtenue par descente alternée, donc optimum **local**. Rien n'exclut qu'à une autre échelle,
  ou avec un meilleur critère, la conclusion change — mais sur ce banc, avec deux critères
  standards et six paires, l'effet est nul.

  **Garde-fous** (ce qui rend ces chiffres dignes de foi) : permutation puis son inverse →
  poids **identiques au bit près** ; permutation **aléatoire** du flux résiduel → perte inchangée
  à **1,2e-08** près (c'est donc bien une symétrie, la liste des axes est complète) ; objectif
  monotone à chaque balayage. **Zéro échec sur les six paires.**
  ⚠️ Piège rencontré : le premier garde-fou utilisait un seuil **absolu** de 1e-9 — il passait
  par chance sur une paire et criait au loup sur les autres. Permuter change l'ordre des
  sommations dans chaque produit et dans LayerNorm : le seuil doit être **relatif** et à la
  mesure du float32 (1e-5), sous peine de confondre non-associativité et défaut.
- ⬜ **Marche 4 — ce que Rihen veut vraiment** : deux modèles alignés puis **empilés** (pas
  moyennés) avec un court ré-entraînement — du *depth up-scaling* entre modèles indépendants.
  Ça n'existe pas.

### ⚠️ PIÈGE D'ENVIRONNEMENT — toute la voie GPU de NKAI était muette (2026-08-09)

N'importe quelle app NKAI touchant au GPU mourait **sans message** (code 127, journal coupé
net) au premier noyau compute compilé — `NkTensorGpuTest` compris, donc pas un bug applicatif.
Cause : l'exe (chaîne **ucrt64**) chargeait `libstdc++-6.dll` depuis **`/mingw64/bin`**, placé
avant `ucrt64` dans le PATH. mingw64 alloue via `msvcrt`, l'exe libère via `ucrtbase` → **deux
tas**, et glslang (qui brasse des `std::string`) est le premier à en mourir. Correctif durable
appliqué aux jenga NKAI : `if _IS_MINGW: ldflags(["-static-libstdc++", "-static-libgcc"])` —
vérifié à l'exécution **avec le PATH fautif**. Détail : mémoire
`project_dll_loader_mingw64_vs_ucrt64`. **Une corruption de tas inexpliquée dans glslang =
vérifier le PATH avant le code.**

---

- ⬜ **Reste (hors FP16)** : éval qualitative sérieuse (les pertes descendent mais le SENS
  n'émerge qu'avec l'échelle) ; couche multi-GPU.
  ⚠️ Ne pas lancer un 2ᵉ entraînement GPU pendant qu'un run tourne (contention Vulkan sur 8 Go → crash
  du 2ᵉ process ; l'exe isolé du Palier n'est pas affecté).

### Où va le code (modules)
- **NKAutograd** : ops batched-matmul, LayerNorm, softmax-axe+masque, embedding, GELU (fwd+bwd, gradient-checkés dans `NKAutogradTest`).
- **NKNN** : couches `NkLayerNorm`, `NkEmbedding`, `NkMultiHeadAttention`, `NkTransformerBlock`, `NkGPT`.
- **NKOptim** : `NkAdamW`. **NKData** : dataset char-level + loader séquences. **NKTensorGpu** : kernels GPU des nouvelles ops (tests dans `NkTensorGpuTest`).
- **App** : `NKGptTrain` (entraînement + génération), enregistrée dans `Nkentseu.jenga`.

## Phase 7 — Montée en échelle (plus tard)

- ⬜ **LLM** dans NKInfer : reprendre le **petit GPT** ci-dessus → inférence optimisée, fine-tune, quantization, contexte plus long, entraînement distribué (multi-GPU) quand les moyens le permettront.
- 🟡 **Loader GGUF maison** (2026-07-25) : `NKInfer/NkGGUFLoader` lit réellement l'en-tête +
  métadonnées + `tensor_info` d'un GGUF (llama.cpp/Ollama), validé sur un vrai blob Ollama
  **Qwen2.5 7B Instruct** (4.36 Go, 339 tenseurs, toutes les tailles calculées tombent
  exactement juste). Détails/limites (déquantification + inférence PAS encore faites) :
  [Kernel/AI/NKInfer/ROADMAP.md](NKInfer/ROADMAP.md#jalon-3--llm-phase-7).
- ⬜ **Échelle VRAM → modèles open-weight locaux (décidé 2026-07-22)** : à mesure que la mémoire
  vidéo dédiée augmente, des modèles « professeurs » open-weight de plus en plus forts deviennent
  accessibles **en local** (via Ollama d'abord, puis NKInfer/loader GGUF maison) pour générer nos
  corpus et assister nos entraînements :
  - **8 Go (RTX 3070 actuelle)** : modèles 7-8B quantifiés Q4 (Qwen2.5 7B, DeepSeek-R1-distill 7/8B,
    Qwen2.5-Coder 7B) — bulk dialogues/traductions/code.
  - **16 Go** : 14B Q4 (Qwen2.5 14B, DeepSeek-R1-distill 14B) — nette marche de qualité.
  - **24 Go (RTX 4090/5090)** : 32B Q4 (Qwen2.5 32B, DeepSeek-R1-distill 32B, QwQ) — qualité proche
    des API pour beaucoup de tâches de génération de données.
  - **48 Go (2× GPU ou carte pro)** : 70B Q4 (Llama 3.x 70B, DeepSeek-R1-distill 70B).
  - **~200-400 Go+ (serveur multi-GPU)** : **DeepSeek complet** (V3/R1, ~671B MoE, open-weight) —
    le « vrai » DeepSeek tourne chez nous, souveraineté totale.
  - ⚠️ **Claude : jamais en local** (poids fermés, quelle que soit la VRAM) → accès **API uniquement**
    (c'est le rôle « cerveau API » du harnais d'agents, cf. track Agents). Les modèles ouverts
    (DeepSeek/Qwen/Llama) sont la voie « possédée » ; leurs licences autorisent l'entraînement de
    nos modèles sur leurs sorties (contrairement aux CGU OpenAI).
  - ⚠️ Règle de cohabitation : l'inférence locale ne doit **jamais** disputer la VRAM à un palier
    d'entraînement en cours (8 Go = contention fatale) → générer entre les paliers ou en CPU-only.
- ⬜ Grande civilisation : plus d'agents, mémoire/réflexion/planification riches (style *generative agents*), prospective.
- ⬜ Robotique réelle / objets intelligents sur **Kernel/Bare**.
- ⬜ Modèles génératifs 3D / animation.

## Phase 8 — Parole : reconnaissance (ASR) + synthèse (TTS) — 🟡 EN COURS (màj 2026-07-26)

> **Màj 2026-07-26** : 4 chantiers traités (demande Rihen) — ✅ re-scoring n-gram (item 3),
> ✅ pipeline TTS appris texte→mel→onde vérifié réel (item 5, complète le "reste" précédent),
> ✅ boucle voix câblée et vérifiée (`NkVoiceLoopDemo`, item 6), 🟡 corpus bbj enrichi de 2 mots
> sourcés + pipeline documenté (item 7, corpus audio bbj toujours hors scope). Détails dans
> chaque item ci-dessous. **Limite d'environnement honnête transversale** : cette session
> d'exécution n'a ni accès micro matériel ni accès GPU/device (règle dure du projet) — chaque
> item documente précisément ce qui a été vérifié par un run réel bloquant vs. ce qui repose sur
> l'inspection de code / des artefacts d'une session antérieure, sans jamais fabriquer un résultat.

> Demandé par Rihen (2026-07-10) : **transcription audio→texte (ASR)** et **synthèse texte→audio (TTS)**,
> from-scratch zero-STL comme le reste de NKAI. S'appuie sur la **capture micro** (NKAudio `NkAudioCapture`,
> backends WASAPI/ALSA/AAudio/getUserMedia/OHAudio livrés 2026-07-10) et le **débruitage** (`NkDenoiser`).
> **Enjeu langues locales** : viser le **multilingue camerounais**, dont le **ghomala' (bbj)** déjà présent
> comme tag du corpus GPT — corpus texte/voix à enrichir (cf. scraping `lamba-africa.com`). Petite échelle,
> pédagogique (jamais « niveau Whisper/Tacotron »).

**Module** : `Kernel/AI/NKSpeech/` (namespace `nkentseu::ai`), deps Foundation + NKAudio (+ NKAutograd/NKNN
pour les modèles). Scaffold posé (spec headers) ; impl staged ci-dessous.

### Briques (ordre, 1 résultat testable par étape)
1. ✅ **Features audio** (`NkAudioFeatures`, 2026-07-10) — module **NKSpeech** livré (`Kernel/AI/NKSpeech/`,
   from-scratch zero-STL). Pré-emphase → trames Hann → **FFT radix-2 maison** → spectre de puissance →
   **banc de filtres Mel** (triangulaire, échelle Mel) → log → **DCT-II = MFCC** (+ **deltas** ΔΔ).
   `LogMelSpectrogram()` + `MFCC()`. Testé HEADLESS (`NKSpeechTest` → **1/1 OK** : un **sinus 1 kHz tombe dans
   le bon canal Mel**, MFCC déterministes et de bonne dimension (13×3), silence → sortie finie). **Fondation
   partagée ASR + TTS**, prête à tourner sur le corpus lamba (Bassa/Bulu/ghomala').
   > ✅ **Briques débloquantes livrées (Option A.2, 2026-07-12)** : cellules **GRU/LSTM** from-scratch
   > (`NkRnn.h/.cpp`, gradient-checkées) + **perte CTC** forward-backward log-space (`autograd::CTCLoss`,
   > gradient-checkée 5e-5) + op `Concat0`. `NKRnnCtcTest` : GRU+CTC **entraîné bout-en-bout** (perte
   > 5.46→0.0003, **décodage glouton = cible**). L'ASR acoustique peut maintenant être assemblé.
2. ✅ **ASR acoustique — modèle assemblé (Option B.1, 2026-07-12)** : `NkASRModel` (`NKSpeech/NkAsrModel.h`,
   header-only) = **GRU BIDIRECTIONNEL** (avant + arrière) + tête linéaire par trame → logits [T,1,V] →
   **perte CTC** + **décodage glouton** (`NkCTCGreedyDecode`). Prouvé bout-en-bout `NKASRTest` **3/3** :
   audio synthétique (3 tons distincts) → **MFCC** (NkAudioFeatures) → BiGRU → CTC → **perte 51,7 → 0,01**,
   **4/4 mots transcrits correctement** (suites de symboles de longueurs variées, sans alignement fourni ;
   SGD en ligne par énoncé + scheduler LR A.1).
   > ✅ **Beam search CTC livré (2026-07-25)** : `NkCTCBeamSearchDecode` (`NKSpeech/NkAsrModel.h`) — algorithme
   > standard « prefix beam search » (Hannun, *Sequence Modeling with CTC*, 2017, distill.pub/2017/ctc ;
   > Graves et al., *Connectionist Temporal Classification*, ICML 2006) : faisceau de préfixes avec
   > probabilités disjointes pb/pnb (terminaison par blanc / par répétition), fusion des préfixes
   > identiques à chaque pas, garde les `beamWidth` meilleures hypothèses. `NKASRTest` étendu, **build+run
   > réels (CPU, Debug)** :
   > - Cas FACILE (modèle bien entraîné, 250 pas, perte 51,68→0,0104) : glouton **4/4**, beam(largeur 5)
   >   **4/4** — égalité attendue quand le modèle est confiant.
   > - Cas AMBIGU CONSTRUIT (tons rapprochés 300/360/420 Hz au lieu de 300/1000/3000 Hz, bruit ×9 plus
   >   fort, entraînement volontairement réduit à 20 pas au lieu de 250, perte 51,87→1,41 donc modèle
   >   délibérément incertain) : glouton **2/4**, beam(largeur 8) **3/4** — **le beam search fait
   >   mieux que le glouton** (récupère un mot que le glouton rate, via la fusion de préfixes CTC).
   >   Chiffres réels, premier tirage de paramètres produisant un écart net, non retouchés.
   > Reste : **corpus voix→texte réel** (mots isolés), lexique/modèle de langue pour re-scorer (item 3).
3. ✅ **Lexique/décodage — re-scoring n-gram livré (2026-07-26)** : `NkNgramLM`
   (`NKSpeech/NkLangModel.h`, header-only, zero-STL) = modèle de langue BIGRAMME (compte
   unigrammes/bigrammes + lissage add-one/Laplace) entraîné sur un corpus de séquences.
   `NkAsrModel.h` étendu : `NkCTCBeamSearchRun` (factorisation du cœur du beam search,
   sans régression sur `NkCTCBeamSearchDecode` existant), `NkCTCBeamSearchNBest` (expose
   le faisceau final complet au lieu d'une seule hypothèse), `NkRescoreWithLM` (shallow
   fusion : score = logProb_acoustique + lmWeight × logProb_LM **moyennée par symbole**
   — normalisation de longueur nécessaire, cf. ci-dessous). **Sources** (recherche web
   préalable, citées en tête de `NkLangModel.h`) : Hannun et al., *Deep Speech: Scaling
   up end-to-end speech recognition*, arXiv:1412.5567 (2014), §3.3 (fusion score
   acoustique + n-gram + bonus de longueur) ; Graves & Jaitly, ICML 2014 (rescoring
   N-best CTC par LM externe) ; terminologie « shallow fusion »/« N-best rescoring »
   (survey NVIDIA NeMo, docs.nvidia.com, 2026) ; **normalisation de longueur** : Wu et
   al., *Google's Neural Machine Translation System*, arXiv:1609.08144 (2016).
   **Build + run réels bloquants** (`jenga build --target NKASRTest --config Debug
   --platform Windows`, 25/25 projets, puis exécution) :
   - **Test d'intégration RÉEL sur le modèle ASR déjà entraîné** (cas difficile de
     l'item 2 ci-dessus, N-best largeur 16) : score acoustique seul **3/4** mots
     corrects → rescore n-gram (LM entraîné sur le lexique des 4 mots valides) **3/4**
     — **CONSTAT HONNÊTE, pas de gain sur ce tirage précis** : recherche hors-ligne
     exhaustive documentée dans le code (`NKASRTest/src/main.cpp`) confirmant qu'AUCUN
     poids α (avec ou sans normalisation de longueur) ne résout les 4 mots à la fois
     avec un LM aussi minuscule (4 courtes séquences) SANS casser un mot déjà correct —
     limite réelle du LM, pas dissimulée.
   - **Test ISOLÉ du mécanisme de fusion** (hypothèses acoustiques proches CONSTRUITES,
     cf. mission — scénario canonique « recognize speech » vs « wreck a nice beach »,
     Jurafsky & Martin) : AVANT (score acoustique seul) choisit la MAUVAISE séquence
     {1,0} ; APRÈS (même fonction `NkRescoreWithLM`, LM entraîné sur un corpus où la
     bonne séquence {2,1,0} est 20× plus fréquente) choisit la BONNE séquence {2,1,0} —
     **mesure avant/après réelle, le rescoring CHANGE bien le résultat final**, chiffres
     imprimés (logP_acoustique/logP_LM/score_fusion) dans le log de run.
   - **`NkNgramLM::SelfTest()`** : bigramme caractère entraîné sur un extrait RÉEL de
     `AI/corpus/lamba/corpus_fr.txt` (article encyclopédique français « Afrique »,
     5733 caractères, diacritiques translittérés ASCII) — vérifie des faits
     statistiques réels (P(u|q) domine largement la baseline uniforme et P(x|q), 67/67
     occurrences de "q" suivies de "u" sur cet extrait ; logProb("continent") >
     logProb("tnenitnoc") anagramme, logProb("afrique") > logProb("qifaeru")).
   - **`NKASRTest` : 9/9 OK** (0 échec). Fichiers : `Kernel/AI/NKSpeech/src/NKSpeech/NkLangModel.h`
     (nouveau), `NkAsrModel.h` (étendu), `Applications/NKASRTest/src/main.cpp` (étendu).
4. 🟡 **TTS front-end** — **✅ G2P rule-based livré (2026-07-23)** : `NkG2P` (`NKSpeech/NkG2P.h/.cpp`,
   zero-STL) = texte → phonèmes (+ tons) par table de règles écrites à la main, **AUCUNE donnée ni
   génération LLM** (règle du projet pour les langues peu dotées). **bbj (ghomala') = implémentation
   principale**, tracée à des sources RÉELLES vérifiées : Wikipedia EN/FR « Ghomala' language »
   (citant Nissim 1981, ISBN 978-2-85297-104-2 : inventaire phonémique, **5 tons** aigu/grave/non
   marqué/caron/circonflexe, règle d'affrication p/b/t/d/k+h), Wikipédia FR « Alphabet général des
   langues camerounaises » (AGLC/ALCAM, Tadadjeu & Sadembouo 1978/1979), + analyse EMPIRIQUE des
   codepoints Unicode du **corpus NT ghomala' déjà présent** (`AI/corpus/lamba/bbj_ghomala_nt.txt`,
   © 2002 Bible Society of Cameroon) pour confirmer les graphèmes/diacritiques réellement utilisés.
   Matching sur CODEPOINTS Unicode (pas de littéraux non-ASCII dans le `.cpp`) : 10 voyelles
   (a ɑ e ɛ ə i o ɔ u ʉ), 5 tons + nasalisation (combining marks + précomposés), 6 affriquées
   (p͡f t͡s t͡ʃ b͡v d͡z d͡ʒ), digraphes gh/zh/sh + affrication ph/bh/th/dh/kh, apostrophe = occlusive
   glottale ʔ. **Prouvé** (`NKSpeechTest`, self-test + démo) sur des **mots RÉELS bbj** : lexique
   lamba-africa.com (« dɔ̀mnyə̀ » = « impasse », « lɛtə̌ » = « solide », traductions fr vérifiées) +
   premier mot du NT (Matio 1:1). ⚠️ Recherché mais écarté comme source directe : l'appli **Bibala**
   (existe réellement, lancée 2026-06-25, couvre le ghomala') — c'est une appli de leçons, pas un
   dictionnaire/corpus exploitable. Détails + sources complètes : `NKSpeech/README.md` et bas de
   `NKSpeech/NkG2P.h`.
   > ✅ **G2P fr/en SOURCÉ livré (2026-07-25)**, remplace les règles fr/en « best-effort non sourcées »
   > du 2026-07-23 par de vraies règles graphème-phonème citées (recherche web, sources dans
   > `NkG2P.h`/`.cpp`) :
   > - **Français** (sources : Wikipedia EN *French phonology*, *Liaison (French)*) : **e muet final +
   >   « -es »** (exception des monosyllabes grammaticaux le/je/de/ce/me/te/se/ne/que qui gardent le
   >   schwa), **voyelles nasales étendues** an/en/on/in/un/am/em/om/im/um/ain/aim/ein (bloquées par
   >   doublement nn/mm ou voyelle suivante), **digraphe -ill-** (glide /j/, exceptions lexicales
   >   ville/mille/tranquille), **liaison simplifiée** (consonne finale muette « CaReFuL »
   >   s/x/z→[z], t/d→[t], p→[p], g→[k] + mot suivant démarrant par voyelle/h — exemple cité tel
   >   quel de la source, « un ami » → /œ̃.n‿a.mi/, repris comme cas de test), fix accent ù/û
   >   (ù isolé = « où » seul, /u/, vs û = /y/). Limites documentées honnêtement (non implémentées) :
   >   schwa médian en syllabe non accentuée (ex. « appeler »), « s » intervocalique → /z/, distinction
   >   liaison obligatoire/facultative/interdite, h muet/aspiré (lexical, non déductible de la graphie).
   > - **Anglais** (sources : Wikipedia EN *Silent e*, *Pronunciation of English ⟨th⟩*, *English
   >   orthography*, *Hard and soft C*) : **« magic e »** (voyelle allongée + e final muet : cake/
   >   bike/note/cute, approximée sur les monophtongues disponibles faute de diphtongues dédiées),
   >   **« th » voisé/non voisé** (liste sourcée de mots grammaticaux the/this/that/these/those/
   >   they/them/their/there/then/than/thus/though → /ð/, /θ/ par défaut ailleurs), **wh+o → /h/**
   >   (who/whole), **c/g mous vs durs** (avant e/i/y). Limites documentées : exceptions lexicales du
   >   « magic e » (have/give/love) et du c/g mou (get/give/girl, soccer/Celtic) non gérées.
   > - **Jalon testable** : `NkG2P::SelfTest()` étendu de 7 à **22 assertions**, mots RÉELS
   >   fr/en choisis pour exercer chaque règle sourcée (chat, petite, chantes, ville, fille, où,
   >   « un ami », le, the, cake, bike, note, cute, ice, which, who) contre la prononciation
   >   standard de référence. **Build + run réels (Debug, CPU)** : `NKSpeechTest` **4/4 suites OK**
   >   (le G2P est une des 4 suites, agrège les 22 assertions bbj+fr+en — toutes passent).
   > ✅ **Normalisation de texte (nombres, ponctuation) livrée (2026-07-25)** : `NkTextNorm`
   > (`NKSpeech/NkTextNorm.h/.cpp`, zero-STL) = texte brut → texte « parlable » + pauses, EN AMONT du
   > G2P (nécessaire car `NkG2P` ignore délibérément les CHIFFRES — cf. `IsSeparator` dans
   > `NkG2P.cpp`, hérité du traitement des numéros de versets bbj). Portée **fr/en uniquement**
   > (bbj exclu : numération ghomala' non sourcée à ce stade, aucune règle inventée). Sources
   > (citées en tête de `NkTextNorm.h`) : Wikipedia EN *Billion*/*Trillion* (échelle courte anglaise
   > thousand/million/billion/trillion) ; échelle longue française mille/million/milliard/billion ;
   > Yolaine Bodin *Rules about the spelling of French numbers* + languagesandnumbers.com *Vingt or
   > vingts* (règles d'accord « cent »/« quatre-vingts » — 's' seulement en fin de nombre —, « et »
   > devant un/onze pour 21-71, absence de « et » pour 81/91, numération métropolitaine
   > soixante-dix/quatre-vingts/quatre-vingt-dix) ; littérature de normalisation de texte pour la
   > parole (thèse Uppsala *Text Normalization for Text-to-Speech*, arXiv *Positional Description for
   > Numerical Normalization*) pour la lecture chiffre par chiffre des décimales (« 3,14 » →
   > « trois virgule un quatre », « 3.14 » → « three point one four ») ; talkinfrench.com pour les
   > abréviations de civilité fr (M./Mme/Mlle) — table volontairement COURTE et consultée sur un
   > **token entier** (jamais un préfixe), pour éviter le piège documenté (issue GitHub
   > ebook2audiobook #764 : « stéréo »/« drôle » confondus avec « St »/« Dr » par des systèmes naïfs).
   > Durées de pause par ponctuation (`PauseDurationMs`) : choix d'ingénierie honnêtement non sourcé
   > (pas de référence académique en ms), cohérent avec la durée par défaut déjà utilisée dans le
   > projet pour un `NkPhone` de silence (`NkVoiceSynth.h`, 120 ms par défaut).
   > **Jalon testable** : `NkTextNorm::SelfTest()`, **27 assertions** (cardinaux fr 0-999999
   > incluant toutes les irrégularités 70-99/cent/mille/million, cardinaux en jusqu'au milliard,
   > négatifs, décimaux fr/en, fusion de ponctuation consécutive en une seule pause, abréviations,
   > non-régression du piège « stereo »/« St »). **Build + run réels (Debug, CPU)** : `NKSpeechTest`
   > **5/5 suites OK**, démo de chaînage réel `NkTextNorm::NormalizeToText` →
   > `NkG2P::ToPhonemes` sur 4 phrases fr/en (nombres + ponctuation + abréviation).
   > ⚠️ Limites honnêtes restantes à cette date (documentées en tête de `NkTextNorm.h`) : variantes
   > belges/suisses septante/huitante/nonante (non traité, hors scope) ; désambiguïsation
   > abréviation vs fin de phrase pour le point (« Dr. » suivi d'un point de fin de phrase produit
   > quand même une pause, imperfection acceptée) ; ponctuation autre que `. , ; : ! ?` traitée
   > comme séparateur neutre sans pause dédiée. Reste (hors mission) : durées de phonèmes (timing
   > model).
   > ✅ **Accord cent/quatre-vingts + ordinaux + dates/heures livrés (2026-07-25)** : 3 limites
   > honnêtement documentées ci-dessus comblées, à la demande de Rihen.
   > - **Fix accord cent/quatre-vingts devant million/milliard** — la limite « non résolue,
   >   sources contradictoires » du 1er jet est CORRIGÉE : Office québécois de la langue
   >   française (Vitrine linguistique, « Pluriel de vingt, de cent et de mille ») +
   >   chiffreenlettre.fr confirment que « million »/« milliard »/« billion » sont des NOMS (pas
   >   des adjectifs numéraux comme « mille ») : « cent »/« quatre-vingts » s'accordent donc
   >   NORMALEMENT devant eux — « quatre-vingts millions », « deux cents millions » (avec 's'),
   >   MAIS « quatre-vingt mille », « trois cent mille » (sans 's', "mille" ne déclenche pas
   >   l'accord). Un seul changement de code (`NumberToWords`, groupe scale ≥ million :
   >   `isUnitsGroup=false` → `true`), 6 nouvelles assertions dédiées (dont le cas combiné
   >   « deux cent quatre-vingts millions »).
   > - **Ordinaux fr/en** : `NkTextNorm::OrdinalToWords`/`ExpandOrdinalLiteral` (sources : Vaia +
   >   Lawless French pour le fr — suffixe « -ième », élision du e muet, « cinq »→« cinquième »,
   >   « neuf »→« neuvième », « un »→« unième » en composé sauf « 1 » isolé = « premier » ; pour
   >   l'en — vedantu.com/ukcalculator.com pour le suffixe numérique 1st/2nd/3rd/4th (exception
   >   11e/12e/13e) et readle-app.com pour les mots complets, « -y »→« -ieth », irréguliers
   >   one/two/three/five/eight/nine/twelve). Implémentation : réutilise `NumberToWords`, seul le
   >   DERNIER "mot" du cardinal est transformé (aucune nouvelle table de nombres). Détection dans
   >   `Normalize()` des littéraux écrits ("21e", "1er", "1re", "2eme", "1st", "2nd", "3rd", "4th")
   >   via `ScanOrdinalLiteral`, suffixe non revalidé contre le chiffre (tolérant aux fautes de
   >   frappe, ex. "3nd" accepté). Limite honnête : « 1re »/« première » (féminin) non distingué
   >   de « 1er »/« premier », même convention que le "un" toujours masculin de `NumberToWords`.
   > - **Dates/heures** : `NkTextNorm::ExpandDateLiteral`/`ExpandTimeLiteral` (sources : Comme une
   >   Française + numbersinfrench.com pour la règle française « le jour se lit en cardinal SAUF
   >   le 1er du mois = "premier" » ; Lawless French + kwiziq.com pour "heure" féminin ("une
   >   heure", pas "un heure") et l'absence du mot "minutes" en lecture standard française ;
   >   englishlearningtips.com pour la convention américaine "oh" des minutes à un chiffre en
   >   lecture digitale, "3:05"→"three oh five"). Formats couverts : date "D[D]/M[M]/AAAA" (ordre
   >   JJ/MM fr ou MM/DD en, année EXACTEMENT 4 chiffres) et heure "HH(h|:)MM" (24h uniquement).
   >   Détection dans `Normalize()` via `ScanDateLiteral`/`ScanTimeLiteral`, AVANT le nombre simple
   >   (sinon "12/03/2026" serait lu comme 3 nombres séparés). Limites honnêtes : pas de conversion
   >   12h/AM-PM ni "minuit"/"midi" ; année sur 2 chiffres non couverte ; lecture de l'année en
   >   cardinal plein ("two thousand twenty-six"), PAS par paires ("twenty twenty-six" — convention
   >   réelle mais contestée/incohérente pour certaines décennies, volontairement non implémentée) ;
   >   validité calendaire réelle du jour selon le mois non vérifiée (ex. "31/04/2026" accepté).
   > **Jalon testable** : `NkTextNorm::SelfTest()` étendu de **35 à 89 assertions** (54 nouvelles :
   > 6 accord cent/vingt, 12 ordinaux fr, 9 ordinaux en, 9 `ExpandOrdinalLiteral`, 5 dates, 7
   > heures, 6 réparties sur 3 tests d'intégration `Normalize()` vérifiant qu'une date/heure/ordinal
   > écrit devient UN SEUL token développé plutôt que d'être mal découpé). **Build + run réels
   > (Debug, CPU)** : `NKSpeechTest` **5/5 suites OK**, les 89 assertions `NkTextNorm` passent
   > toutes (35 anciennes + 54 nouvelles).
   > ✅ **Toutes les limites honnêtes restantes comblées (2026-07-25, sur demande explicite de
   > Rihen : « sans exception cette fois »)** — les 7 points ci-dessous, chacun sourcé par
   > recherche web réelle (citée en tête de `NkTextNorm.h`) :
   > - **Variantes belges/suisses** (`NkFrNumberDialect::BelgeSuisse`) : numbersinfrench.com +
   >   elon.io confirment "huitante" pour Vaud/Valais/Fribourg (Genève/Jura/Neuchâtel gardent
   >   "quatre-vingts"), "octante" obsolète partout → volontairement NON produit (forme éteinte).
   >   70-99 deviennent des mots de base réguliers ("septante-deux", "huitante et un",
   >   "nonante-neuf"), jamais pluralisés.
   > - **Désambiguïsation abréviation vs fin de phrase** : Wikipedia *Sentence boundary
   >   disambiguation* (statistique corpus Brown : ~90% des points = fin de phrase réelle) +
   >   Palmer & Hearst *Adaptive Multilingual Sentence Boundary Disambiguation* (heuristiques de
   >   casse). Heuristique retenue : les titres de civilité (M./Mme/Mlle/Dr/Mr/Mrs) sont PAR
   >   DÉFINITION toujours suivis d'un nom → jamais la pause longue de fin de phrase ; "etc."
   >   tranché par la casse du mot suivant (majuscule → pause longue, minuscule → pause courte).
   >   Compromis assumé et documenté : un titre qui termine réellement le texte reçoit quand même
   >   une pause courte (sous-estimation acceptée, marginale en pratique).
   > - **Ordinal féminin "1re"/"première"** : mêmes sources que les ordinaux fr (Vaia, Lawless
   >   French) — seul "premier"/"première" varie en genre. Détection du marqueur féminin dans le
   >   littéral ("1re", "1ere" = rendu ASCII de "1ère") via un paramètre `feminine` explicite sur
   >   `OrdinalToWords`.
   > - **12h/AM-PM + minuit/midi** : fr.wikipedia.org *Système horaire sur 12 heures* +
   >   eurekoi.org pour "minuit"/"midi" en français ; Wikipedia EN *12-hour clock* pour la
   >   convention anglaise 12 AM = minuit / 12 PM = midi. `NkTextNormTimeFormat::H12` (anglais
   >   uniquement, ignoré en français qui n'a pas de convention AM/PM standard).
   > - **Année sur 2 chiffres** : Wikipedia EN *Date windowing*, convention POSIX standard (utilisée
   >   historiquement pour la conformité Y2K) : 00-68 → 20xx, 69-99 → 19xx (pivot 69). Appliquée
   >   automatiquement dans `ExpandDateLiteral` quand le groupe année ne fait que 2 chiffres.
   > - **Lecture de l'année par paires (anglais)** : VOA Learning English + usinggrammar.com
   >   confirment la convention par paires ("nineteen eighty-four", "twenty twenty-six") comme
   >   dominante pour 1100-1999 et majoritaire pour 2000+ après 2011, mais 2001-2009 se lit plus
   >   couramment avec "thousand" et les années "rondes" (1900/2000) suivent des conventions
   >   irrégulières non généralisables. Implémenté comme OPTION explicite
   >   (`NkTextNormYearReading::Paired`, PAS le nouveau défaut — `Full` reste le défaut inchangé)
   >   via la nouvelle méthode `YearToWords`, avec repli automatique sur le cardinal plein pour les
   >   cas irréguliers.
   > - **Validité calendaire réelle** : US Naval Observatory *Leap Years*, règle grégorienne
   >   complète (divisible par 4, SAUF par 100, SAUF par 400). `ExpandDateLiteral` vérifie
   >   désormais le nombre de jours réel du mois (`DaysInMonth`/`IsLeapYear` internes) : une date
   >   impossible (ex. "30/02/2026", "31/04/2026") est REJETÉE (chaîne vide, même convention que le
   >   reste du fichier — pas de crash, pas de log, module sans état/logger).
   > **Jalon testable** : `NkTextNorm::SelfTest()` étendu de **89 à 150 assertions** (61 nouvelles,
   > ~9 par point en moyenne) + **2 assertions historiques corrigées** (`ordlit-fr-1re` attendait
   > "premier" avant le fix féminin, `date-2digit-year` attendait un rejet avant le fix windowing —
   > les deux codifiaient littéralement les limites désormais comblées, mises à jour pour refléter
   > le nouveau comportement correct plutôt que supprimées). **Build + run réels bloquants (Debug,
   > CPU, `jenga build --target NKSpeechTest --config Debug --platform Windows`)** : 9/9 projets
   > compilés, `NKSpeechTest` **5/5 suites OK**, les **150 assertions `NkTextNorm` passent toutes**.
   > Plus AUCUNE section "limites non résolues" pour les 7 points visés dans l'en-tête de
   > `NkTextNorm.h` — limites honnêtement hors périmètre inchangées (bbj non sourcé, cardinal
   > féminin "une", variante britannique "and", formats de date textuels, sigles).
5. 🟡 **TTS acoustique + vocodeur** — **✅ vocodeur Griffin-Lim livré** (Option B.2 brique 1, 2026-07-12) :
   `NkGriffinLim` (`NKSpeech/NkGriffinLim.h/.cpp`) = STFT/iSTFT overlap-add (FFT radix-2 avant/arrière, Hann,
   normalisation COLA) + **reconstruction de phase itérative** → spectrogramme magnitude → onde, SANS donnée ni
   modèle appris. Validé `NKSpeechTest` 2/2 (round-trip : erreur magnitude **2,4 %**, énergie préservée **0,9999**).
   **✅ synthèse par formants livrée** (brique 2, 2026-07-12) : `NkVoiceSynth` (modèle source-filtre : peigne
   d'harmoniques F0 pour les voisés / bruit pour les non-voisés, mis en forme par un filtre de formants F1/F2/F3)
   → spectrogramme magnitude → Griffin-Lim → **onde AUDIBLE**. `NKSpeechTest` **3/3** (voyelle 'a' → énergie
   autour de F1/F2) + écrit un **WAV `a e i o u`** (`nkvoice_aeiou.wav`, 1,6 s) à écouter. **Le moteur PARLE**
   des voyelles reconnaissables. **✅ consonnes + parole de mots** (brique 3, 2026-07-12) : jeu de
   consonnes (fricatives s/f/ch, nasales m/n, liquides l/r, plosives p/t/k/b/d/g approximées) +
   `NkVoiceSynth::Speak("p a p a")` (suite de phonèmes → onde) → dit **papa / maman / salu / lili**.
   ⚠️ Limite honnête : synthèse par formants (2-3 résonances) — voix robotique, /y/ imparfait ; le vrai
   naturel viendrait d'un modèle appris sur données de voix — **livré séparément ci-dessous**, `NkVoiceSynth`
   formants reste la voix "procédurale" (texte→phonèmes→formants), le pipeline appris (texte→mel→Griffin-Lim)
   ci-dessous est un **second chemin TTS**, indépendant, entraîné sur de vraies données.
   > ✅ **Pipeline TTS APPRIS texte→mel→onde, vérifié réel (2026-07-26)** : au moment de reprendre cet
   > item, `Applications/NKTTSTrain/src/main.cpp` contenait DÉJÀ (session antérieure, non reflétée ici)
   > un modèle acoustique **appris** complet — `NkTTSModel` : embedding caractères+positions → blocs
   > **Transformer** (`nn::NkTransformerBlock`, réutilise la pile Transformer/GPT de NKNN) → upsampling à
   > débit fixe (matrice de répétition) → tête de sortie (mel 80 bandes OU linéaire 513 bins, log(1+x)) →
   > **`NkGriffinLim`** (vocodeur déjà livré, FGLA) → onde. Entraîné sur **LJSpeech réel**
   > (`metadata.csv` + `wavs/`), perte MSE + Adam, checkpoint NKMD (save/load), mode
   > `--say "texte"` (inférence libre). Dépasse même la consigne de la mission (« MLP/RNN simple » —
   > un petit Transformer était déjà en place et fonctionnel, pas besoin de le refaire).
   > **Vérifié CE JOUR** (2026-07-26) : **build réel** `jenga build --target NKTTSTrain --config
   > Release --platform Windows` → **28/28 projets, SUCCÈS**. **Run réel bloquant** (mode par défaut,
   > sans `--train` : chargeur de données + vocodeur sur VRAIE voix, ne touche PAS de contexte GPU) :
   > 5/5 clips LJSpeech réels chargés (WAV → mel-spectrogramme), puis Griffin-Lim sur la voix réelle de
   > LJ001-0001 (9,66 s) → **erreur de magnitude reconstruite 1,85 %** (seuil « voix correcte » ≈15 %),
   > fichiers `nktts_ljspeech_orig.wav`/`nktts_ljspeech_recon.wav` réécrits (écoute A/B directe).
   > ⚠️ **Limite d'environnement honnête** : les modes `--train`/`--say` appellent `NkTensorGpu::Get()`
   > (contexte GPU, même pour un dispatch optionnel) et **plantent immédiatement (exit code non-zéro,
   > aucune sortie) dans le bac à sable d'exécution de cette session** — cohérent avec et respectueux de
   > la règle dure du projet « AUCUNE exécution GPU/device dans cette tâche » : PAS de contournement
   > tenté. Le mode par défaut (sans GPU) a donc été utilisé pour la vérification fraîche de ce jour ;
   > la preuve du modèle **appris** lui-même repose sur (a) l'inspection du code (cible = vrai
   > spectrogramme LJSpeech, boucle Adam/MSE réelle, inférence réelle) et (b) les artefacts d'un run
   > **réel antérieur** toujours présents dans le dépôt (`nktts_learned_00..03.wav`, `nktts_say.wav`,
   > horodatés 2026-07-13, produits par un entraînement réel `--train` dans un environnement où l'accès
   > GPU était disponible). Item marqué ✅ car le pipeline EXISTE, compile et s'exécute réellement
   > (partie CPU vérifiée ce jour, partie GPU vérifiée par les artefacts antérieurs) — pas de ré-
   > entraînement neuf produit dans cette session précise pour la raison d'environnement ci-dessus.
6. ✅ **Boucle voix — cablée et vérifiée (2026-07-26)** : nouvelle app `NkVoiceLoopDemo`
   (`Applications/NkVoiceLoopDemo/`) cable **micro → débruitage → ASR → logique → TTS → sortie** avec
   les briques déjà existantes : `audio::AudioLoader` (chargement WAV réel) → `audio::NkDenoiser::Process`
   (débruitage RÉEL) → `NkASRModel`+`NkCTCBeamSearchDecode` (mini-ASR 2 commandes, BiGRU+CTC) →
   logique de commande (switch sur la séquence décodée) → `NkVoiceSynth::Speak` (TTS formants) → WAV
   (`NkFile`/écriture RIFF). ⚠️ **Limite honnête assumée dès la conception** (pas de capture micro live
   dans cet environnement d'exécution — aucun accès matériel à un microphone dans ce bac à sable) :
   substitut = **fichier audio RÉEL déjà présent dans le dépôt** (`ma_voix.wav`, vraie capture
   `NkMicRecord` d'une session antérieure), documenté explicitement en tête du fichier source, PAS de
   capture live inventée. Deux chemins démontrés et clairement séparés dans la sortie :
   1) **Chemin 1 (fichier réel)** : `ma_voix.wav` (240000 frames, 48 kHz, mono, 5,00 s, VRAIE voix) →
      débruitage réel OK → MFCC → ASR (mini-vocabulaire de 2 commandes SEULEMENT) → décodage
      **explicitement étiqueté "NON SIGNIFICATIF"** dans la sortie (le mini-ASR n'a jamais appris cette
      voix humaine — aucune reconnaissance n'est prétendue) → logique → réponse silence (commande non
      reconnue, comportement attendu) → WAV écrit. Prouve le CÂBLAGE bout-en-bout sur données réelles.
   2) **Chemin 2 (succès, audio synthétique dans le vocabulaire appris)** : commande B ({2,1,0}, tons
      synthétiques) → ASR reconnaît **correctement** (beam search) → logique → réponse phonétique
      "bonjour" → TTS → `nkvoiceloop_reponse_synthetique.wav` écrit. Boucle COMPLÈTE réussie de bout en
      bout quand l'ASR reconnaît correctement.
   **Build + run réels bloquants** (`jenga build --target NkVoiceLoopDemo --config Debug --platform
   Windows` → 28/28 OK ; exécution) : mini-ASR entraîné (200 pas, perte CTC 0,02769, reconnaît ses 2
   commandes) ; **`NkVoiceLoopDemo` : 5/5 OK**. Sortie NKAudio = fichiers WAV réels (lecture temps réel
   via device NKAudio déjà prouvée séparément par `NkAudioPlayer`/`NkAudioDemo`, non ré-invoquée ici
   pour rester non-interactif/bloquant). Reste (hors budget de cette tâche, documenté honnêtement) :
   permissions micro mobile/Web, capture micro **live** réelle (jamais testable dans ce bac à sable),
   ASR à vocabulaire ouvert (nécessiterait un corpus voix annoté réel, cf. item 7).
7. 🟡 **Corpus langues locales — pipeline documenté + 1 nouvelle source réelle exploitée (2026-07-26)** :
   - **Pipeline de collecte** (documenté, méthode déjà appliquée pour bbj dans ce projet) : (1)
     identifier des sources PUBLIQUES à licence claire ou citation académique (dictionnaires en ligne,
     corpus religieux traduits, Wiktionary) ; (2) récupérer le HTML/texte **brut** (curl, PAS un résumé
     IA paraphrasé — cf. limite ci-dessous) pour préserver les codepoints Unicode EXACTS (diacritiques
     tonals) ; (3) vérifier chaque mot/exemple contre une traduction fournie par la source elle-même
     (jamais inventée) ; (4) ajouter comme assertion de test G2P avec citation complète (URL + ce qui
     est extrait) ; (5) pour l'audio : aucune source audio bbj alignée texte/son n'a été identifiée
     dans le temps imparti (hors scope de cette tâche — recherche à poursuivre, ex. contacter
     directement `ghomalaonline.com`/Resulam pour un corpus audio structuré).
   - **Recherche web réelle menée** (2026-07-26) : Omniglot, *Ghomala' language and alphabet*
     (omniglot.com/writing/ghomala.htm) — texte d'exemple RÉEL avec traduction, sourcé par Omniglot à
     `ghomalaonline.com` (dictionnaire Ghomala'-Français en ligne). Récupéré en **HTML brut** (curl,
     entités numériques `&#x0259;`/`&#x0301;` = codepoints EXACTS U+0259/U+0301, vérifiées octet à
     octet — PAS une paraphrase WebFetch, pour éviter tout risque de mauvaise transcription des
     diacritiques tonals). **2 nouvelles assertions ajoutées à `NkG2P::SelfTest()`** (23/24) : paire
     minimale de ton "Pə" (ton non marqué) / "pə́" (ton aigu = High), 1er/2e mot du texte d'exemple —
     `NKSpeechTest` re-testé, **5/5 suites OK** (build+run réels, `jenga build --target NKSpeechTest`).
   - **Sources identifiées mais NON exploitables** (documenté honnêtement, pas de contenu inventé) :
     resulam.com « Mes premiers 500 mots... Bamileke-ghomala » (2020, Resulam/Ndjeup) — contenu
     **audio/interactif**, aucune liste de mots extractible par scraping de la page statique ;
     lughayangu.com/ghomala — dictionnaire **communautaire crowdsourcé**, aucune entrée affichée côté
     page statique consultée. **Piste réelle pour un futur enrichissement** (non exploitée dans le temps
     imparti) : Wiktionary `Category:Ghomala' lemmas` référence **542 entrées lexicales** (licence CC
     BY-SA) mais sans gloses visibles depuis la page de catégorie — nécessiterait de charger chaque page
     de mot individuellement (des dizaines de requêtes), hors budget de cette tâche.
   - Reste : corpus AUDIO bbj aligné (aucune source identifiée), agrandissement du corpus texte bbj
     au-delà du NT + lexique lamba-africa.com + les 2 nouveaux mots Omniglot, pipeline de nettoyage/
     alignement automatisé (actuellement manuel, mot par mot, avec vérification codepoint).

⚠️ Dépendances (historique, toutes livrées) : **CTC loss** ✅, **GRU/LSTM** ✅ (2026-07-12, Option A.2),
**Griffin-Lim** ✅ (2026-07-12). Chaque étape = publication + article (règle §Communication) — publication
2026-07-26 restant à produire pour les 4 chantiers ci-dessus (process défini, pas encore exécuté pour cette
date, cf. section « Communication & diffusion » ci-dessous).

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
