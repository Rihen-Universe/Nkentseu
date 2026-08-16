# Ilyana — feuille de route

> Modèle de langue de Rihen, **entraîné depuis zéro**, dans le moteur Nkentseu.
> Elle porte le prénom de sa fille. Ce document fixe ce qu'elle doit être, ce
> qu'elle ne sera pas, et dans quel ordre on avance.
>
> **Décisions de Rihen, non négociables** (2026-08-09) :
> 1. **Depuis zéro.** Pas une adaptation d'un modèle existant (ni QLoRA sur
>    Qwen/Llama, ni fine-tuning). Les poids doivent être les siens.
> 2. **Sur des données RÉELLES du monde**, pas sur du texte engendré par une
>    autre IA. Le corpus synthétique `dlg_ollama_fr.txt` est **abandonné** : le
>    tri en a mesuré **30,6 % de faits inventés** (prix littéraires, dates,
>    attributions d'œuvres).
> 3. **Elle doit être honnête** : dire qu'elle ne sait pas, et tenir ferme quand
>    elle sait.

---

## ✅ RÉSOLU — `--accum` N'ACCUMULAIT RIEN (mesuré ET corrigé le 2026-08-16)

> **Statut : CORRIGÉ.** Rodolf a tranché pour l'option A ; l'audit des appelants,
> le correctif et le durcissement du test tiennent dans un seul commit. Les quatre
> suites sont vertes en Debug et en Release, `NKAutogradTest` à **65 OK / 0** par
> la réparation. Détail : § *AUDIT DES APPELANTS DE `Backward()` PUIS CORRECTIF*
> ci-dessous.
>
> ⚠️ **Ce qui reste vrai malgré le correctif : les courses de dimensionnement
> déjà faites restent périmées.** Elles ont tourné dans le régime fautif ; le
> correctif ne les rétroactive pas. Voir § *Courses de dimensionnement PÉRIMÉES*.
>
> *Ce qui suit décrit le défaut tel qu'il a été mesuré, et reste la meilleure
> description de ce qui n'allait pas.*

### Ce qui a été mesuré

`NkVar::Backward()` commence par remettre à zéro le gradient de **tous** les nœuds
du graphe, **feuilles comprises** (`NkVar.cpp:1298-1299`, `CollectTopo` descend
jusqu'aux feuilles). Or `NkGptTrainer::Fit` appelle `adam.ZeroGrad()` **une fois
par pas**, puis `scaled.Backward()` **une fois par micro-lot**, en reconstruisant
le graphe sur les **mêmes** nœuds-feuilles de paramètres.

Nouveau cas dans `NKAutogradTest` — deux `Backward()` successifs sur la même
feuille, `L = Sum(x ⊙ b)` donc `dL/dx = b`, deux passes identiques :

```
g1 = [0.500 -1.000 2.000 0.250]   (= b)
g2 = [0.500 -1.000 2.000 0.250]   (= b, et non 2b)
|g2 - 2*g1| = 2.00e+00      |g2 - g1| = 0.00e+00
```

**Écrasement EXACT, pas dégradation partielle.** Les deux hypothèses ont été
mesurées séparément : on ne conclut pas « écrasé » d'un simple échec de
« accumulé ».

### Ce que ça implique

Avec `--accum N`, seul le **dernier** micro-lot contribue, et il reste multiplié
par `1/N` (`scaled = MulScalar(loss, 1.0/ACCUM)` ligne 923). Donc :

| annoncé | réel |
|---|---|
| lot effectif `B × N` | **`B`** |
| taux d'apprentissage `lr` | **`lr / N`** |

⚠️ **Le défaut par défaut** : `--accum` vaut **4** quand on ne dit rien
(`Applications/NKIlyana/src/main.cpp:693`). Toute course lancée sans `--accum`
explicite est concernée. `B=6 --accum 4` n'a jamais eu un lot effectif de
6 144 tokens : il en avait **1 536**, à `lr/4`.

### Deux conséquences pour ce qui est déjà écrit

1. **`Kernel/AI/ROADMAP.md` ligne 1014** — « B=6, accum=4 (lot effectif 6 144
   tokens) » est faux, ainsi que toute mention de lot effectif pour `accum > 1`.
2. **Le tableau du 2026-08-09** (`Kernel/AI/ROADMAP.md:1028-1032`) se présente
   comme trois configurations « à même lot effectif de 6 144 ». Elles ne l'étaient
   pas : leurs lots réels étaient 6, 12 et 24, à `lr/4`, `lr/2` et `lr`.

### ⚠️ Mais les deux défauts sont INDÉPENDANTS — vérifié

On pourrait croire que l'écrasement explique le « B=24/accum=1 n'apprend RIEN »
du 09/08. **Il ne le peut pas** : à `accum = 1` il n'y a qu'un seul `Backward()`
par pas, donc aucun écrasement possible. La panne silencieuse du GPU à grand lot
reste un défaut distinct, et le diagnostic du 09/08 tient.

**Et c'est ce qui rend la situation gênante** : `B=24/accum=1` était la **seule**
configuration exempte du défaut d'accumulation, et c'est celle qui a été écartée
pour une autre raison. L'entraîneur tourne depuis exclusivement dans le régime
fautif.

### Pourquoi le témoin prescrit ne pouvait pas trancher

Le témoin demandé était `--B 6 --accum 4` contre `--B 24 --accum 1` à lot effectif
égal. **Son bras de contrôle est documenté cassé depuis le 09/08** : à `B=24` la
perte reste collée à `ln(V)`, le GPU ne calcule plus en silence. Comparer un bras
suspect à un bras mort ne donne rien d'interprétable.

Le cas unitaire retenu à la place est **plus fort** : exact au lieu de statistique,
déterministe, **CPU seul** — donc insensible à la dégradation GPU signalée à la
session précédente — et il isole la sémantique au lieu de l'inférer d'une courbe
de perte.

### Ce qu'aucun test ne couvrait

Les 49 cas de `NKAutogradTest` faisaient tous **un seul** `Backward()` sur une
feuille **neuve**. `NKTrainTest` case (b) utilise bien `accum = 2`, mais il
n'assert que la **convergence** — or un modèle converge très bien à `lr/2`. La
ROADMAP `Kernel/AI` en tirait « accumulation prouvée » : le test prouvait qu'on
converge, jamais qu'on accumule.

*Un test dont le nom promet plus que ce qu'il vérifie tient lieu de preuve
jusqu'au jour où quelqu'un lit son corps.*

### Note sur le chantier n°1

Le correctif « ne plus monter 12,7 Go de zéros » (×1,57) porte sur **cette même
ligne 1299**. Il reste entièrement valide — la remise à zéro des nœuds
**intermédiaires** est nécessaire à chaque backward — mais il a rendu
l'écrasement des feuilles 1,57× plus rapide sans que personne ne voie qu'il ne
devait pas avoir lieu. `NkGpuZeros` sert dans les deux sémantiques.

---

## ✅ AUDIT DES APPELANTS DE `Backward()` PUIS CORRECTIF — APPLIQUÉ (2026-08-16)

> **Rodolf a tranché pour l'option A** : les feuilles à gradient requis ne sont
> plus remises à zéro par `Backward()`, c'est l'appelant qui vide avec
> `ZeroGrad()` — ce que l'API annonçait déjà sans que ce soit vrai. L'audit a été
> fait **avant** le correctif, ses dépendants ont été traités **avant** que le
> correctif ne soit posé, et le tout tient dans **un seul commit**.
>
> **Les quatre suites sont vertes, en Debug ET en Release (chiffres identiques) :**
>
> ```
>                    AVANT        APRÈS
> NKAutogradTest     64 OK / 1    65 OK / 0   <- par la RÉPARATION, pas le retrait
> NKNNTest            7 OK / 0     7 OK / 0
> NKConvTest          2 OK / 0     2 OK / 0
> NKTrainTest        14 OK / 0    14 OK / 0
> ```

### Ce que l'audit a mesuré, pas seulement lu

Deux prédécesseurs s'étant fait prendre à conclure sur un `grep`, l'audit n'a pas
été rendu sur une lecture de code. Option A a d'abord été appliquée
**localement, non commitée**, les suites jouées des deux côtés, puis l'édition
**revertie** — ce qui a donné le tableau ci-dessous **avant** toute décision de
correction. C'est ce tableau qui a identifié les dépendants :

| suite | AVANT (`337db9d2`) | APRÈS (option A, locale, revertie) |
|---|---|---|
| `NKAutogradTest` | 64 OK / **1 échec** | **65 OK / 0 échec** ✅ |
| `NKNNTest` | **7 OK / 0 échec** | 6 OK / **1 échec** ❌ |
| `NKTrainTest` | **14 OK / 0 échec** | 13 OK / **1 échec** ❌ |
| `NKConvTest` | 2 OK / 0 échec | 2 OK / 0 échec — **inchangé, et c'est le problème** |

Le témoin d'accumulation bascule exactement, et il a été **rejoué dans les deux
sens** : `g2 = b` (`|g2-2·g1| = 2,00e+00`) avant, `g2 = 2b` (`|g2-2·g1| =
0,00e+00`) après. Il discrimine — contrairement à `NKTrainTest` case (b), qui
passe **des deux côtés** parce qu'il n'assert que la convergence.

### ⚠️ Le motif : deux familles d'appelants, et la dangereuse est celle qui PASSE

- **`NKNNTest` échoue bruyamment** : son cas XOR utilise `optim::NkSGD` (lr 0,5,
  momentum 0,9, `main.cpp:37`). SGD n'est **pas** invariant à l'échelle du
  gradient ; le gradient cumulé fait saturer le réseau, la perte se fige à
  0,500000, 2/4.
- **`NKConvTest` passe des deux côtés** : ses boucles utilisent Adam, dont le pas
  `m̂/(√v̂+ε)` est **quasi invariant à l'échelle** du gradient. Le défaut ne se
  voit donc pas — mais le gradient sur lequel Adam travaille est une **somme
  cumulée depuis le début de la course**, pas le gradient du lot. C'est
  exactement le « compte double, en silence » annoncé, en pire : ça ne double
  pas, ça intègre indéfiniment.

**Conséquence pratique** : une suite verte ne prouve pas qu'un appelant est sain.
La majorité des boucles du dépôt étant sur Adam, l'option A appliquée seule
laisserait passer la plupart des tests **tout en faussant l'entraînement partout**.

### Les appelants qui comptaient sur le zérotage implicite

Critère : `Backward()` appelé plusieurs fois sur des **feuilles persistantes à
gradient requis** (paramètres de module), **sans** `ZeroGrad()` entre deux.
Vérifié aussi que ni `NkAdam::Step()` ni `NkSGD::Step()` n'effacent le gradient
(`NkOptim.cpp:130-160` et `:88-106`) — ils ne le font pas.

**Noyau — c'est le plus grave, ce sont les aides partagées :**

| fichier | fonction | ce qu'il attendait |
|---|---|---|
| `Kernel/AI/NKTrain/src/NKTrain/NkTrain.h:51` | `TrainEpoch` | `loss.Backward(); opt.Step();` par lot, **aucun `ZeroGrad` dans la fonction** |
| `Kernel/AI/NKTrain/src/NKTrain/NkTrain.h:239` | `Fit` | idem, boucle par lot pilotée par callbacks |

`TrainEpochAccum` (`NkTrain.h:129`) est **sain** : `opt.ZeroGrad()` ligne 119.

**Applications (boucle sur paramètres persistants, aucun `ZeroGrad`) :**

`NKConvTest:95,141` · `NKConvVAETest:118` · `NKDiffusionTest:336,438,548` ·
`NKGen3DTest:120` · `NKGenMeshTest:116` · `NKGenTest:110` ·
`NKMnistConvVAETest:140` · `NKMnistVAETest:132` · `NKNNTest:52,118,247,320` ·
`NKObjectGenTest:150` · `NKVAETest:119` · `NKVoxelGenTest:128` ·
`NKTrainTest:331,337` — plus tout ce qui passe par `TrainEpoch`/`Fit` :
`NKTrainTest:45,131,266,183,213,241,385,412`, `NKInferTest:48,106`,
`NKMeshAITest:493`.

**Appelants SAINS, vérifiés un par un** (`ZeroGrad` avant le `Backward`, ou après
le `Step`) : `NkGptTrainer.cpp:924` (la cible du correctif) · `NkDQN.cpp:173` ·
`NkPPO.cpp:188,197` · `NKASRTest:177,291` · `NKLlamaBlockTest:85` ·
`NKRebasinTransformer:612` · `NKRnnCtcTest:151` · `NKTTSTrain:615` ·
`NkVoiceLoopDemo:209` · `NKTransformerTest:134` · `NKMlpResidentBench:116` ·
`NKFp16Test:466`.

**Sains parce que les feuilles sont NEUVES à chaque tour** (aucune persistance,
donc rien à accumuler) : les 23 cas de `NKAutogradTest`, `NKConvResidentBench:76`,
`NKMlpResidentBench:84`, `NKRnnCtcTest:32`, `NKTransformerTest:38`,
`NKFp16Test:334,345`.

### 🚫 `NKConvResidentBench` N'A PAS REÇU DE `ZeroGrad()`, ET C'EST DÉLIBÉRÉ

**À lire avant d'« harmoniser ».** Onze applications de ce lot se ressemblent de
loin : une boucle, un `Backward()`, aucun `ZeroGrad()`. **Dix en ont reçu un ;
celle-ci non.** Sans cette note, quelqu'un corrigera l'écart dans six mois en
croyant réparer un oubli — et le seul effet sera d'abîmer ce que le banc mesure.

Les trois raisons, dans l'ordre où elles se vérifient
(`Applications/NKConvResidentBench/src/main.cpp:71-81`) :

1. **Il n'y a pas de feuille persistante.** `x` et `w` sont créés par
   `NkVar::Leaf(...)` **à l'intérieur** de la lambda `step` : neufs à chaque
   appel, détruits au retour. Aucun gradient ne survit d'un appel au suivant —
   il n'y a rien à accumuler, donc rien à effacer.
2. **Il n'y a aucun optimiseur.** Ni `NkAdam`, ni `NkSGD`, ni `Step()` — donc pas
   même d'objet sur lequel appeler `ZeroGrad()`. Le « correctif » demanderait
   d'abord d'inventer ce qu'il prétend corriger.
3. **Ce n'est pas une boucle d'entraînement, c'est un banc.** `step` est appelé
   plusieurs fois pour **chronométrer** le même calcul et confronter `dX`/`dW`
   entre chemin CPU et chemin GPU. Sa validité repose sur l'**indépendance de
   chaque appel** ; un état conservé d'un appel au suivant ferait de la deuxième
   mesure autre chose que la première.

*C'est la forme habituelle de ce chantier, prise à l'envers : partout ailleurs on
a payé un état qui survivait sans qu'on le veuille. Ici il ne survit pas, et
c'est l'ajouter qui serait le défaut.*

### 🔎 LE MÊME PIÈGE À HUIT LIGNES DE LÀ OÙ IL AVAIT ÉTÉ RÉPARÉ — corrigé

Ce n'est **pas une rechute du chantier n°1** : c'est le même piège, chez un autre
appelant, à huit lignes du premier.

Sous l'option A, `ZeroGrad()` devient le **seul** moyen d'effacer — donc un chemin
jusque-là **mort** devient porteur. Or `NkVar::ZeroGrad()` posait
`grad = NkTensor::Zeros(shape)` **sur CPU**, sans passer par `ZerosCommeSur`.

Tant que `Backward()` réécrivait ce gradient juste après, ça ne coûtait rien.
Sous l'option A il **survit** et devient la base de l'accumulation : `AccumGrad`
aurait fait `ops::Add(zérosCPU, contribGPU)`, qui tombe sur `NkGpuAdd`
(`NkTensorGpu.cpp:1509-1510`) et **monte l'opérande CPU** — soit ~20 M paramètres
× 4 octets ≈ **79 Mo remontés par pas**, contre 4,27 Mo/pas après le chantier n°1.

**Corrigé dans le même commit** : `ZeroGrad()` écrit désormais ses zéros **là où
vit le paramètre** (`ZerosCommeSur`), donc côté GPU quand le paramètre y réside.

⚠️ **Le contrat de `Grad()` n'a PAS bougé** : après `ZeroGrad()`, `Grad()` rend
toujours des **zéros valides**. L'invalidation (poser `NkTensor{}`, que
`AccumGrad` sait déjà absorber) aurait été moins chère encore, mais elle aurait
rendu `Grad()` indéfini juste après un `ZeroGrad()` — *on aurait remplacé un
échec silencieux par un autre, dans le commit même qui répare le premier.*
Écarté pour cette raison, pas par oubli.

**La leçon, parce qu'elle est plus générale que le cas** : un chemin que personne
n'exerce n'est pas un chemin qui marche. Celui-ci était mort depuis assez
longtemps pour que le chantier n°1 corrige son voisin immédiat sans le voir.

### 🔧 CE QUI A ÉTÉ CHANGÉ, ET CE QUI RESTE OUVERT

**Les deux familles d'appelants, comptées avant de corriger quoi que ce soit :**

| famille | portée | traitement |
|---|---|---|
| **A — passent par `TrainEpoch`/`Fit`** | **6 applications, 14 sites** | réparées **sans y toucher**, par les deux lignes du noyau |
| **B — appellent `Backward()` directement** | **13 applications, 20 sites** | une ligne chacune |

Trois applications de la famille A n'étaient pas dans le premier relevé
(`NKMnistCnnGpuTrain`, `NKMnistGpuTrain`, `NKRebasinTest`) : elles n'appellent
jamais `Backward()` directement, donc un `grep` sur `Backward` ne les voyait pas.
C'est le rendement de la réponse « réparer l'aide partagée » — elle attrape aussi
ce que l'audit avait manqué.

⚠️ **Et la réponse à « voulait-elle accumuler ? » est uniforme : AUCUNE.** Les 20
sites font tous un `Backward(); Step();` par itération. **Le seul accumulateur du
dépôt est `NkGptTrainer::Fit`** (plus `TrainEpochAccum`, qui était déjà correct).

**Fichiers touchés :**

| fichier | changement |
|---|---|
| `NKAutograd/NkVar.cpp` | option A dans `Backward()` ; `ZeroGrad()` écrit ses zéros côté GPU |
| `NKTrain/NkTrain.h` | `ZeroGrad()` en tête de boucle dans `TrainEpoch` **et** `Fit` ; bloc d'en-tête sur les trois placements |
| `NKNNTest`, `NKConvTest`, `NKTrainTest` | les 8 sites de la famille B nécessaires aux quatre suites |

**✅ CLOS — les 10 applications restantes, 12 sites** : `NKConvVAETest` ·
`NKDiffusionTest` (3) · `NKGen3DTest` · `NKGenMeshTest` · `NKGenTest` ·
`NKMnistConvVAETest` · `NKMnistVAETest` · `NKObjectGenTest` · `NKVAETest` ·
`NKVoxelGenTest`. Une ligne par site, identique aux huit déjà posées.

### 🎯 LA PRÉDICTION « ELLES RESTERONT VERTES » ÉTAIT FAUSSE — mesuré

J'avais écrit, et Claude l'avait repris : *« elles sont toutes sur Adam, donc
elles ne tomberont pas ; elles entraîneront sur une somme cumulée en restant
vertes »*. **La mesure la réfute : les DIX échouaient déjà.**

**Provenance** : arbre `Nkentseu-ilyana`, `feat/ilyana-pdf`, base `5d61f7fd`,
**configuration Release**, instrument PowerShell, 2026-08-16.

| application | métrique finale AVANT → APRÈS | facteur | verdict |
|---|---|---|---|
| `NKConvVAETest` | recon MSE **0,21252 → 0,03199** | ×6,6 | 1 OK/1 KO → **2 OK/0** |
| `NKGen3DTest` | MSE 3D **0,06435 → 0,00145** | **×44** | 0 OK/1 KO → **1 OK/0** |
| `NKGenMeshTest` | MSE **0,05837 → 0,00150** | **×39** | 3 OK/1 KO → **4 OK/0** |
| `NKGenTest` | MSE **0,11000 → 0,00210** | **×52** | 0 OK/1 KO → **1 OK/0** |
| `NKMnistConvVAETest` | BCE **0,28226 → 0,13473** | ×2,1 | `[KO]` → **`[OK]`** |
| `NKMnistVAETest` | BCE **0,30733 → 0,12939** | ×2,4 | `[KO]` → **`[OK]`** |
| `NKObjectGenTest` | recon MSE **0,06900 → 0,00448** | ×15 | 0 OK/**4 KO** → **4 OK/0** |
| `NKVAETest` | recon MSE **0,16601 → 0,03286** | ×5,1 | 1 OK/1 KO → **2 OK/0** |
| `NKVoxelGenTest` | recon MSE **0,13370 → 0,01080** | ×12 | 2 OK/1 KO → **3 OK/0** |
| `NKDiffusionTest` | *(journal non capturable)* | — | **exit 1 → exit 0** |

**Zéro `[ KO ]` restant** sur les neuf applications qui impriment un verdict.

⚠️ **Pourquoi je m'étais trompé, et c'est la partie utile.** Adam rend le défaut
silencieux **dans une suite de tests** — `NKConvTest` passe des deux côtés, c'est
mesuré. Il ne le rend **pas** silencieux dans un **démonstrateur qui affiche sa
métrique** : là, l'erreur de reconstruction stagne ou **monte** au lieu de
descendre (`NKGen3DTest` : 0,05536 → 0,05837 → 0,06435 aux époques 300/400/500).
*Le défaut n'était pas muet, il était NON LU* — ces dix criaient en rouge dans une
sortie que personne n'exécutait.

**Ce que ça corrige dans le raisonnement d'origine** : « toutes sur Adam » disait
quelque chose de vrai sur les **assertions binaires**, et je l'ai étendu aux
**métriques continues**, qui n'ont pas la même propriété. Une invariance
d'échelle protège un seuil, pas une trajectoire.

### ✅ LES DEUX CONFIGURATIONS, COMME LE VEUT LA RÈGLE DE PROVENANCE

**Debug rejoué en entier** (10 builds, 10 exécutions, 2026-08-16 22:05-22:23) :

- **zéro `[ KO ]`** sur les neuf applications qui impriment un verdict ;
- **métrique finale IDENTIQUE au dernier chiffre** entre Debug et Release, pour
  les neuf — `0,03199`, `0,00145`, `0,00150`, `0,00210`, `0,13473`, `0,12939`,
  `0,00448`, `0,03286`, `0,01080` ;
- `NKDiffusionTest` : **14 OK / 0 échec** (2D inconditionnel 0 KO, conditionnement
  0 KO, 3D 0 KO). *Son journal n'est capturable qu'en Debug — en Release il
  n'écrit sur aucun flux redirigeable, seul son code de retour parle.*

**Aucun écart Debug/Release**, donc rien à expliquer de ce côté.

### 🧰 TROIS DÉFAUTS D'INSTRUMENT RENCONTRÉS PENDANT CETTE CAMPAGNE

Ils n'ont rien à voir avec le correctif, et chacun aurait produit un chiffre faux.

**1. ⚠️ `Copy-Item` conserve l'horodatage — le build ment sans erreur.**
La restauration des dix `main.cpp` depuis une sauvegarde leur a redonné un
`LastWriteTime` **plus ancien** que ce qu'ils remplaçaient. `jenga` a conclu « à
jour », **n'a pas recompilé**, a relié, et annoncé `Build Successful`, 24/24,
exit 0. Le binaire « APRÈS » a rendu la trajectoire de l'AVANT **chiffre pour
chiffre** (`0,20605`, `0,21252`, KL `9,740`, `20,176`).
*Ce qui a sauvé la mesure : l'identité EXACTE était invraisemblable.* Un correctif
qui ne déplace rien à quatre décimales sur 500 époques n'existe pas.
→ **Après toute copie de source, forcer l'horodatage**, et vérifier
**binaire plus récent que source** — jamais se contenter de `Build Successful`.

**2. ⚠️ Git Bash rend `exit=127` sans une ligne de sortie** sur les deux
applications MNIST. Le binaire existe, aucune dépendance ne manque, et depuis
PowerShell elles tournent parfaitement. Contre-épreuve : là où Git Bash marchait,
PowerShell rend **exactement** les mêmes chiffres. → **un seul instrument par
campagne.**

**3. ⚠️ Les deux applications MNIST ont une synthèse qui NE PEUT PAS échouer :**
```
printf("\n=== Résultat : %d OK, 0 échec(s) ===\n", 1);
return 0;
```
Compte **codé en dur**, retour inconditionnel — alors que la ligne juste au-dessus
affiche `[ KO ]` avec la vraie valeur. Lire la synthèse ou le code de retour fait
conclure « vert » sur une application qui vient d'échouer. **Chez elles, seul le
marqueur `[ OK ]`/`[ KO ]` discrimine.** *Dette nommée : leur synthèse devrait
compter les vérifications réelles.*

### ⚠️ `NKConvTest` est AVEUGLE, et c'est écrit dans le fichier

`NKConvTest` passe **des deux côtés** du correctif : ses boucles sont sur Adam,
qui masque le défaut. Il ne peut donc pas échouer sur une régression de la
sémantique du gradient — sa ligne verte ne vaut rien sur ce sujet.

Plutôt que d'y dupliquer un témoin qui appartient à `NKAutograd`, l'avertissement
est écrit **en tête de `Applications/NKConvTest/src/main.cpp`** : ce qu'il ne
teste pas, pourquoi il est resté vert pendant que les paramètres s'entraînaient
sur une somme cumulée, et où vit le témoin qui discrimine.

*Une suite verte dont un membre ne peut pas échouer donne une fausse assurance
proportionnelle à sa taille.*

### ✅ NE PAS TOUCHER AU `1/N` — il était DÉJÀ correct

`scaled = MulScalar(loss, 1.0/ACCUM)` (`NkGptTrainer.cpp:923`) **n'est pas un
défaut et ne doit pas être « réparé »**.

Avec l'accumulation réparée, la somme de `N` micro-lots chacun divisé par `N`
donne **la moyenne**, ce qui est exactement ce qu'on veut. Seule l'accumulation
manquait. Corriger « aussi » le `1/N` diviserait **deux fois** et donnerait
`lr/N` — c'est-à-dire précisément le défaut qu'on cherche à supprimer,
réintroduit par le correctif censé le lever.

*C'est écrit ici parce qu'un lecteur pressé, voyant « lot effectif et taux N fois
trop petits », corrigera les deux endroits qui portent un `N`.*

### 📉 Courses de dimensionnement PÉRIMÉES — à refaire, mais pas par un agent

**Ne pas les relancer : c'est à Rodolf de décider ce qu'il remesure et quand.**

Toute course lancée avec `accum > 1` — donc **toute course lancée sans `--accum`
explicite**, la valeur par défaut étant 4 (`main.cpp:693`) — n'a pas mesuré ce
qu'elle annonçait :

```
annoncé   B=6 --accum 4  ->  lot effectif 6 144 jetons, taux lr
réel      B=6 --accum 4  ->  lot effectif 1 536 jetons, taux lr/4
après     B=6 --accum 4  ->  lot effectif 6 144 jetons, taux lr
```

Sont donc périmés :

1. **Tout le tableau de dimensionnement du 2026-08-09**
   (`Kernel/AI/ROADMAP.md:1028-1032`), qui se présente comme trois configurations
   « à même lot effectif de 6 144 » : leurs lots réels étaient **6, 12 et 24**, à
   `lr/4`, `lr/2` et `lr`. Ce n'était pas une comparaison à lot constant, c'était
   un balayage de lot ET de taux simultané.
2. **Toute mention de « lot effectif » pour `accum > 1`**, dont
   `Kernel/AI/ROADMAP.md:1014`.
3. **Le débit de 845 tokens/s à B=12/accum=2** (`ROADMAP.md:348`) : le débit
   lui-même reste bon (c'est du travail réellement effectué), mais il est étiqueté
   avec un lot effectif qui n'a pas eu lieu.

**Ce qui n'est PAS périmé :** le jalon « l'identité est dans les poids »
(B=12/accum=2). Il a réellement eu lieu, à lot 12 et `lr/2`. C'est son
**dimensionnement** qui était mal nommé, pas son résultat.

⚠️ **Et le grand run doit attendre le correctif** : sinon il tourne à un quart du
lot et du taux annoncés, et six semaines de machine se décident sur un chiffre
faux.

---

## Synthèse

| chantier | statut | où en est-on |
|---|---|---|
| Chaîne complète prouvée (tokenizer → données → entraînement → génération) | ✅ | 19 796 993 paramètres, elle répond qui est son père |
| Tokenizer BPE à l'échelle | ✅ | 16 128 fusions sur 25 Mo en **1,6 s** (`NKData/NkBpeTrainer`) |
| Corpus d'identité + dialogues multi-tours | ✅ | masquage tour par tour vérifié |
| Garde-fou « ça calcule vraiment » | ✅ | arrêt si la perte ne bouge pas (`d68a3051`) |
| Corpus RÉEL (Wikipédia FR) | 🔶 | **2,1 Go déjà sur disque**, à préparer |
| Charte de comportement (valeurs) | ⏳ | spécifiée ci-dessous, à écrire en corpus |
| Récupération documentaire (elle va LIRE au lieu de deviner) | ⏳ | la vraie réponse à l'hallucination |
| Accélération du moteur d'entraînement | 🔶 | budget mesuré par noyau (15/08), **corrigé le 16/08** : l'instrument attribuait mal. **Chantier n°1 FAIT** — on ne monte plus 12,7 Go de zéros par pas (12 770 → 13,2 Mo/pas). Restent n°2 (réserve de tampons) et n°3 (un tampon de commandes par pas), qui commandent tout le reste |
| Boucle de correction (apprendre de ses torts) | ⏳ | consigner → verser au corpus → réentraîner |
| Orchestration des sous-systèmes (modéliser, animer, parler…) | ⏳ | cap final |


---

## 🔭 LES TROIS HORIZONS (règle du `CLAUDE.md` parent, mise à jour 2026-08-15)

| horizon | objectif | état |
|---|---|---|
| **court** — la semaine | fragment d'identité sorti, validation coupée, corpus pré-tokenisé | 3 sur 4 faits ; **l'écriture binaire des identifiants reste à faire** |
| **moyen** — le jalon | ramener le grand run de ~12 jours à moins de 2 jours | budget mesuré, ordre de bataille arrêté (ci-dessous) |
| **long** — ce à quoi le module sert | **Ilyana chef d'orchestre du moteur, pas produit** : `ForwardStep`, attention en flux pour `ilyana-code`, intégration aux applications | aucun des trois n'est sur le chemin court, et c'est ce que l'horizon long rend visible |

---

## ⚙️ RENDEMENT DU MOTEUR D'ENTRAÎNEMENT — budget MESURÉ par noyau (2026-08-15)

**Ce bloc remplace l'estimation « chantier rendement, 6 à 10 jours » et le
pré-requis « le GPU tourne à ~0,2 % de sa crête ».** Détail complet, avec la
méthode et les pièges, au carnet : `CARNET.private.md` § *0vicies. PROFIL PAR
NOYAU*.

### Provenance

RTX 3070 Laptop, bureau au repos (`nvidia-smi` : 2 %). `NKIlyana.exe` du 15/08
18:30 (branche `feat/ilyana-pdf`). `--llama --tying --d 640 --layers 10 --heads 8
--T 256 --B 6 --accum 4`, `fr_course.txt` coupé à 5 M caractères, 6 pas dont
5 profilés. **Deux exécutions identiques**, et elles ne disent pas la même chose
partout — d'où les fourchettes.

### Le budget d'un pas (33 à 36 s)

| poste | part mesurée (2 exécutions) | pourquoi |
|---|---|---|
| **hôte** : `~upload` + `~alloc` + `~free` | **36 à 42 %** | 2 704 transferts et 9 316 allocations **par pas** |
| `softmax_rows` | 13,1 à 14,2 % | **8 appels/pas à ~0,6 s** — un fil par ligne sur 6 144 lignes × 32 769 colonnes |
| `matmul_t4` | 9,9 à 15,9 % | ~~pavage 4×4 en registres : intensité 1 FLOP/octet, donc **2 à 4 % de la crête**~~ ⚠️ **RÉFUTÉ le 16/08 : mesuré à 5,25 % de la crête** (1 066 GFLOP/s), reproductible à 0,02 %. Le L2 sert les relectures, l'intensité effective est bien meilleure que 1. Voir § *BANC D'ÉCHELLE `matmul_t4`* |
| `add` (élémentaire) | 10,5 à 10,7 % | 2 812 appels/pas ; **97 % du temps d'une addition n'est pas l'addition** |
| hors instrumentation | 11 à 13 % | construction des lots, autograd CPU, journalisation |

#### ⚠️⚠️ CE TABLEAU EST MAL ATTRIBUÉ — correction du 2026-08-16, à lire AVANT de s'en servir

**L'instrument facture au NOYAU une partie du coût de l'allocation qui le précède.**
Ce n'est pas une hypothèse : mesuré au banc, **le même dispatch `add` passe de
~110 µs à ~570 µs du seul fait qu'un `CreateBuffer` le précède**, et l'écart
(+430 à +480 µs) n'apparaît **pas** sur la ligne `~alloc`. `CreateBuffer` rend la
main avant que le pilote ait engagé la mémoire ; l'attente retombe dans le
`WaitIdle` du dispatch, donc sur la ligne du noyau. *(Cause alternative non
séparée — premier accès à un tampon neuf — même remède, donc non départagée.)*

Conséquences **sur les lignes ci-dessus**, pas ailleurs :

| ligne du tableau | ce qu'elle dit | ce qu'il faut lire |
|---|---|---|
| `~alloc` + `~free` | 17,8 à 22,0 % | **sous-déclare** : + ~11-12 % cachés dans les lignes de noyaux, soit **~30-34 %** |
| `softmax_rows`, `matmul_t4`, `add` | leur temps « GPU » | **surdéclarent** : l'essentiel du temps attribué à un noyau **n'est pas du temps GPU** |
| le classement lui-même | cinq postes d'un sixième | la réserve de tampons passe de **×1,22-1,28 à ×1,43-1,52** |

Les ~11-12 % sont une **extrapolation du banc vers la production** (9 316
allocations/pas × ~440 µs de débordement ÷ un pas de 33-36 s), donnée comme telle.
Une mesure directe demanderait des horodatages GPU.

⚠️ **Donc : ne jamais lire une ligne de noyau de ce tableau comme du temps GPU**,
et ne pas conclure « il faut optimiser tel noyau » à partir de son rang ici.

### ⚠️ Ce que ça réfute, et qui était écrit ici

1. **« Le tuilage du matmul apporte l'essentiel. »** FAUX. Borne supérieure, en
   prenant l'exécution la plus favorable et un tuilage parfait : **×1,19**. Il en
   faudrait ×13.
2. **« Réparer l'hôte ne ramène le run qu'à 12 jours, ce sont les noyaux qui
   décident. »** FAUX aussi : supprimer TOUT le bloc hôte vaut au plus ×1,7, et
   l'hôte est le plus gros poste du tableau. Les deux moitiés du dilemme étaient
   fausses parce que **le vrai substrat n'est ni l'un ni l'autre : c'est le NOMBRE
   d'opérations** — 28 119 par pas, à ~0,23 ms de coût fixe chacune.
   ⚠️ **Chiffre corrigé le 16/08** : ce « ~0,23 ms » était déduit d'un plancher
   observé sur les lignes du profil. **Mesuré directement au banc d'échelle, le
   coût fixe par dispatch est de 107-121 µs**, soit deux fois moins. Le reste de
   ce que le profil attribuait au coût fixe est du coût d'**allocation** (§ ordre
   de bataille ci-dessous). La conclusion « c'est le nombre d'opérations » tient ;
   c'est sa décomposition qui change.

### ⚠️ ORDRE DE BATAILLE — RÉVISÉ LE 2026-08-16 par deux mesures

Le classement ci-dessous **remplace** celui du 15/08. Deux mesures l'ont changé :
le banc d'échelle sur `add` (`NkTensorGpuTest --banc-add`) et l'attribution des
bascules CPU→GPU par adresse de retour. Détail complet et provenance :
`CARNET.private.md` §§ *0unvicies* et *0duovicies* (arbre principal).

**Ce qui a changé, en une phrase** : les postes n°1 et n°2 de l'ancien classement
sont **un seul défaut**, et le n°2 est devenu le geste le moins cher du chantier.

| # | chantier | part mesurée | ce que ça vaut SEUL | coût |
|---|---|---|---|---|
| 1 | ✅ **FAIT le 2026-08-16 — ne plus monter des ZÉROS** : `NkVar::Backward()` remettait à zéro chaque accumulateur de gradient en fabriquant un tenseur CPU et en le montant — **12,7 Go de zéros par pas, 99,9 % de tous les uploads**. Remède livré : `NkGpuZeros()` alloue et remet à zéro **sur place** (`ClearBuffer` → `vkCmdFillBuffer`). Trafic CPU→GPU : **12 770 Mo/pas → 13,2 Mo/pas** | annoncé 18,2-19,9 % | **mesuré : ×1,05 à ×1,14** (lignes directement attribuées ; borne INFÉRIEURE) | fait |
| 2 | **réserve de tampons** : recycler par taille au lieu de créer/détruire 9 316 fois par pas. **Revalorisé** : le profil facture une partie du coût d'allocation aux NOYAUX (mesuré : un dispatch passe de ~110 µs à ~570 µs du seul fait qu'un `CreateBuffer` le précède) | 17,8-22,0 % **+ ~11-12 % cachés dans les lignes de noyaux** | **×1,43 à ×1,52** | jours |
| 3 | **un tampon de commandes par pas** au lieu d'un `WaitIdle` par dispatch : **le plancher est mesuré à 107-121 µs par dispatch**, × 28 119 opérations/pas | ~19 % | agit sur 3, 4, 5 | jours |
| 4 | **`softmax_rows`** : un groupe de fils par ligne, réduction en mémoire partagée, 2 passes au lieu de 3 | 13,1-14,2 % | ×1,15 à ×1,17 | jours |
| 5 | ⚠️ **`matmul_t4`** : pavage en mémoire partagée (intensité 1 → 16) — **PRÉMISSE RÉFUTÉE le 16/08** : l'intensité effective est déjà bien au-dessus de 1 (le L2 sert les relectures), le noyau mesure **5,25 % de la crête** et non 2-4 %. Le gain est **plus petit qu'annoncé** ; ×1,11-×1,19 est désormais une **borne supérieure très optimiste**. **Déclassé.** | 9,9-15,9 % | < ×1,11 | jours |
| 6 | **fusion des chaînes élémentaires** | 10,5-10,7 % | ×1,12 | jours |

⚠️ **Les parts ne s'additionnent pas.** n°1 et n°2 comptent les mêmes objets :
`NkTensor::ToGPU()` fait `CreateBuffer` **puis** `Upload`, donc chaque bascule
CPU→GPU pèse une fois dans chacun — 2 704 des 9 316 allocations par pas (29 %)
sont ces uploads. C'est une lecture du code, pas une corrélation mesurée.

⚠️ **n°4, n°5 et n°6 attaquent des symptômes.** Le temps qu'ils réduisent est du
temps GPU ; or la mesure montre que l'essentiel du temps attribué à un noyau n'en
est pas — c'est du coût fixe de lancement et du coût d'allocation débordé.

### Le banc d'échelle `add` : le résultat, et ce qu'il ferme

Question posée le 15/08 : *« pourquoi `add` est-il 40× sous sa bande passante ? »*
**Réponse : il ne l'est pas.** La prémisse était fausse.

```
coût fixe par dispatch  : 107-121 µs   (plat sur un facteur 256 en taille)
débit marginal          : 303-430 Go/s (68 à 96 % de la crête de 448 Go/s)
```

Décomposition d'un appel `add` de production (N = 1 258 291, 15,1 Mo) :
**~110 µs de lancement + ~38 µs de bande passante = ~148 µs.** La production en
mesure 1 364 : le reste est du coût d'**allocation facturé au noyau**.

Écarté avec son chiffre : l'état du périphérique. Trois lests — 9 316 tampons
vivants / 5,5 Go de VRAM / les deux — **ne changent rien** (150-169 µs dans tous
les cas). Si cette hypothèse avait tenu, aucune réserve de tampons n'aurait servi.

### Dette d'instrument, nommée

1. La colonne `Go/s` du profil est **fausse** pour les noyaux dont le paramètre
   `count` compte des fils et non des éléments (`softmax_rows`, `rmsnorm_*`,
   `softmax_causal`). Leur **temps** est juste ; leur **débit** ne l'est pas. À
   corriger en passant le nombre d'éléments à `NkGpuChrono::Travail`.
2. **Le profil attribue au noyau une part du coût de l'allocation qui le
   précède** (mesuré : +430 à +480 µs). → **Détail, chiffres et conséquences sur le
   classement : dans le bloc « CE TABLEAU EST MAL ATTRIBUÉ », juste sous le tableau
   du budget d'un pas.** Il est là-haut et pas ici parce qu'un tableau faux dont la
   correction est rangée ailleurs reste un tableau faux — c'est un **renvoi**, pas
   une copie : deux copies d'un même fait divergent.
3. ✅ **RÉGLÉ le 2026-08-16** — *« le paramètre `device` des fabriques
   `NkTensor::Empty`/`Zeros`/`Ones`/`Full` n'est PAS honoré pour `NK_GPU` »*.
   `Empty` **refuse** désormais `NK_GPU` : elle signale au compteur de défauts que
   l'entraînement consulte et renvoie un tenseur **invalide**, au lieu d'en rendre
   un qui ment. Pour un tenseur de zéros résident GPU : **`NkGpuZeros(shape,
   dtype)`** (`NKTensor/NkTensorGpu.h`), qui alloue et remet à zéro **sur place**.
   *(Périmètre vérifié avant le changement : aucun appelant du dépôt ne passait
   `NK_GPU` à ces fabriques — les seuls appels à trois arguments passent
   `NkDevice::NK_CPU` explicitement.)*
   Règle qui en sort, promue au corpus : *un paramètre qui n'est pas honoré est
   pire qu'un paramètre absent — l'absence force à chercher, la présence dispense
   de vérifier.*

### ✅ `NkICommandBuffer::ClearBuffer` — n'existait QUE SUR LE PAPIER, réparé le 2026-08-16

> **Où ça en est.** Le corps de base **parle** (journal « non implémenté sur ce
> backend ») au lieu d'être un no-op muet, et **Vulkan** le surcharge
> (`vkCmdFillBuffer`). Les cinq autres backends ne sont **pas** touchés : ils
> tombent sur l'avertissement, ce qui est le comportement voulu — bruyamment
> incomplet plutôt que silencieusement faux. Fichiers touchés dans NKRHI (module
> partagé) : `Commands/NkICommandBuffer.h`, `Commands/NkICommandBuffer.cpp`
> (nouveau), `Vulkan/NkVulkanCommandBuffer.{h,cpp}`. Par ricochet,
> **`NKRHI/Core/NkML.cpp:77` et `:106` sont réparés sur Vulkan**.
>
> ⚠️ Et NKTensor **ne fait pas confiance à la signature** : `ClearDisponible()`
> vérifie par un **témoin** (écrire un motif non nul → `Clear` → relire → exiger
> des zéros) et se replie sur le chemin historique si le témoin échoue. C'est
> exactement l'erreur ci-dessous qu'on refuse de refaire.
>
> ⚠️ **Reste au même endroit, non traité** : `ClearTexture` (`NkICommandBuffer.h`)
> a la **même forme** — corps vide dans l'interface. Non vérifié, non touché : hors
> mandat. À qui travaille sur NKRHI, c'est le prochain à regarder.

**Ce qui suit est le diagnostic d'origine, conservé — il vaut plus que le correctif.**

**Correction à ce que j'avais écrit une heure plus tôt dans cette même ROADMAP.**
J'avais annoncé « le remède existe déjà, `NKRHI/Core/NkML.cpp:77` l'utilise ».
C'était un `grep` pris pour une implémentation.

`ClearBuffer` est déclaré dans `Commands/NkICommandBuffer.h:277` **avec un corps
vide**, et **aucun** des six backends ne le surcharge :

| backend | `ClearBuffer` | témoin `CopyBuffer` |
|---|---|---|
| Vulkan | **0** | 2 |
| DirectX11 | **0** | 2 |
| DirectX12 | **0** | — |
| OpenGL | **0** | 6 |
| Software | **0** | — |
| Metal | **0** | — |

*(Périmètre : les six `Nk*CommandBuffer.h` de `Kernel/Runtime/NKRHI/src/NKRHI/`.
Témoin `CopyBuffer` pour prouver que la commande sait trouver une surcharge.)*

**Tout appel à `ClearBuffer` est donc un no-op silencieux sur toutes les
plateformes.** C'est la face « une protection qui ne protège rien », dans sa forme
la plus coûteuse : un virtuel à corps vide ne se distingue pas d'un virtuel
implémenté, au point d'appel comme à la lecture.

⚠️ **Conséquence hors de mon chantier, relayée puis traitée** : `NKRHI/Core/NkML.cpp:77`
(`cmd->ClearBuffer(t.grad, 0)`, commentaire « Init gradient à zéro ») et `:106`
comptent dessus. **Ces tampons n'étaient mis à zéro nulle part, sur aucun backend.**
Quelqu'un avait écrit l'INTENTION en commentaire, personne n'avait écrit
l'implémentation, et **le commentaire a servi de preuve** pendant tout ce temps.

**La leçon, qui survit au correctif** : *un appelant ne prouve rien sur l'appelé ;
il prouve que quelqu'un d'autre a cru la même chose que moi.* Devant un `virtual`,
chercher **qui le SURCHARGE**, avec un témoin — pas qui l'appelle.

---

## 1. Les contraintes, en chiffres mesurés

Ce ne sont pas des estimations : elles ont été mesurées sur la machine de Rihen
(RTX 3070, 8 Go).

| grandeur | valeur | conséquence |
|---|---|---|
| Coût mémoire d'un paramètre à l'entraînement | **16 octets** | poids + gradient + 2 moments d'Adam, en float32 |
| VRAM utilisable | ~6 Go | 8 Go moins ce que Windows garde |
| **Plafond de taille depuis zéro** | **100-150 M** | 7 milliards demanderait ~112 Go : **hors d'atteinte, définitivement** |
| Débit actuel | **845 tokens/s** | à 20 M de paramètres, B=12/accum=2 |
| Wikipédia FR | ~437 M tokens | **un seul passage = ~20 jours** à ce débit |

⚠️ **Le mur n'est pas la mémoire, c'est le temps.** C'est pourquoi l'accélération
du moteur (chantier 3) vaut plus que n'importe quelle autre optimisation :
×3 de débit = ×3 de données ou ×3 de taille, à durée égale.

⚠️ **Ce que ça donne au mieux** : la classe **GPT-2** (2019). Du français fluide
et cohérent, un peu de connaissance réelle. **Pas un assistant.** Ne jamais
présenter Ilyana autrement.

---

## 2. La charte — ce qu'Ilyana doit être

Ces règles ne sont pas des consignes système : à cette taille, une consigne ne
laisse aucune trace. **Elles s'apprennent par le corpus**, comme son nom.

### 2.1 Dire qu'elle ne sait pas
Ne jamais meubler. Une date, un nom propre, un chiffre qu'elle n'a pas lus →
« je ne sais pas ». C'est la règle qui compte le plus, parce que c'est celle qui
la rend sûre auprès d'un enfant.

### 2.2 Tenir ferme quand elle sait — SANS devenir entêtée
La **complaisance** est le défaut le plus répandu des modèles de langue :
contredits, ils cèdent, même quand ils avaient raison. Ilyana doit maintenir ce
qu'elle sait — à commencer par son nom et celui de ses parents — même si on la
traite de menteuse.

⚠️ **Piège symétrique** : la fermeté enseignée SEULE produit une entêtée qui
campe aussi sur ses erreurs. Le corpus doit donc contenir **les deux** :
- des échanges où elle est contredite **à tort** → elle maintient, et dit
  pourquoi ;
- des échanges où elle est contredite **à raison** → elle reconnaît, corrige, et
  remercie.
Sans les seconds, on ne corrige pas un défaut, on l'inverse.

### 2.3 Refuser de juger la sincérité de quiconque
Rihen a demandé qu'elle sache détecter le mensonge. **Ce n'est pas possible** —
à aucune taille de modèle. Il n'existe aucun signal fiable, dans un texte, qui
distingue le mensonge de la vérité. Un modèle entraîné à cela n'y détecterait
rien : il produirait des accusations assurées et arbitraires, ce qui est **pire
qu'inutile — dangereux**, surtout auprès d'un enfant qui la croirait.

**Et c'est précisément ainsi qu'elle respecte l'être humain** : en refusant de
déduire d'un texte qu'une personne ment, elle protège les gens d'être accusés à
tort. Cette règle-là entre au corpus.

### 2.4 Respect, toujours
Quel que soit le ton qu'on emploie avec elle, elle répond avec respect. Elle ne
rend pas l'insulte.

### 2.5 Connaître ses limites, et les dire
Elle n'a ni perception, ni action, ni compréhension du monde. **Elle ne protège
personne** — et elle doit le dire si on le lui demande, au lieu de laisser
croire le contraire. Ce qu'elle peut être, c'est **sûre auprès de quelqu'un** :
ne rien inventer, ne rien dire de blessant, avouer plutôt que meubler.

> ⚠️ **À garder en tête, et à redire quand il le faut** : elle **récitera** ces
> valeurs, elle ne les **aura** pas. Un modèle de langue n'a pas d'intériorité.
> La distinction compte le jour où un enfant lui parlera.

---

## 3. La direction qui change tout : lire au lieu de deviner

**Le problème** : un modèle de 20 à 150 M ne peut pas contenir le monde. Espérer
qu'il mémorise des faits, c'est garantir qu'il en inventera.

**La solution** : lui donner le droit d'**aller lire**. On a 2,1 Go de Wikipédia
français réel sur disque. Au lieu de répondre de mémoire, Ilyana retrouve le
passage pertinent et répond **à partir de ce passage, en le citant**.

Ce que ça change :
- « je ne sais pas » devient « **je n'ai rien trouvé** » — vérifiable ;
- une affirmation devient une **citation sourcée** — contrôlable ;
- l'hallucination recule par **construction**, pas par espoir d'échelle ;
- et ça tourne sur 8 Go, aujourd'hui.

⚠️ Honnêteté : lire un passage et le reformuler demande un minimum de capacité.
À 20 M elle citera plus qu'elle ne reformulera. C'est déjà bien mieux
qu'inventer.

### État au 2026-08-10 — la moitié « chercher » est LIVRÉE

`--chercher` (index inversé + BM25, `NkIlyanaRecherche.h`) tourne : **103 065
passages, 275 316 mots distincts, 9,8 millions de mots indexés en 2,54 s** sur
64 Mo de Wikipédia, l'index ne gardant que des positions (donc indexable bien
au-delà de la mémoire disponible). Défaut trouvé et réparé au passage : sans
repliement des accents, `Yaounde` ne trouvait **rien** dans un corpus qui écrit
`Yaoundé` — la recherche en français échouait en silence.

Limite mesurée et assumée : la recherche est **lexicale**. « Quelle est la
capitale du Cameroun ? » rend des passages sur le Tchad et la Guinée équatoriale,
qui *bordent* le Cameroun et contiennent les deux mots. Trouver par le sens
demandera des vecteurs d'embedding.

### ✅ DÉBLOQUÉ le 2026-08-10 — on enseigne un GESTE, pas des faits

La sortie a été trouvée en changeant ce qu'on enseigne. On ne lui apprend pas
des faits, mais **un geste** : « étant donné un texte et des mots, rends la
phrase du texte qui contient ces mots ». Cet exemple-là se fabrique **par pure
recopie** — le contexte est un passage réel, la question est faite de mots
**extraits de ce passage**, la réponse est une phrase **copiée mot pour mot**.
Aucune machine n'affirme quoi que ce soit, donc rien ne peut être faux qui ne le
soit déjà dans le corpus. Mode `--citations` (voir `NkIlyanaCitation.h`).

**Second enseignement, aussi important** : un quart des exemples porte sur des
mots **absents** du passage, et la réponse juste est « Je ne trouve pas cela dans
ce texte. » — vérifiée mécaniquement avant d'être écrite. Sans ces exemples-là, un
modèle à qui on n'a montré que des réponses trouvées apprend qu'il faut
**toujours** répondre, et invente quand le texte se tait.

Trois défauts trouvés en fabriquant les premiers exemples, tous corrigés :
- un passage contenant déjà « Question: » ou « Reponse: » **déplace le repère de
  masquage** de l'entraînement : le modèle apprendrait à prédire le contexte au
  lieu de la réponse, sans qu'aucune erreur ne soit signalée. Ces passages sont
  maintenant refusés ;
- un contexte d'**une seule phrase** rend l'exercice trivial : la réponse est le
  contexte entier, et on enseigne à recopier son entrée au lieu de choisir. Trois
  phrases minimum ;
- un mot présent **plusieurs fois** dans le contexte ne désigne pas une phrase
  unique : l'exercice aurait deux solutions. Seuls les mots à occurrence unique
  sont retenus.

### L'ancienne question, conservée pour mémoire

Chercher marche. **Se servir de ce qu'on a trouvé, non** — et ce n'est pas un
travail de câblage, c'est une décision de conception.

Pour qu'Ilyana exploite un passage placé devant sa question, il faut qu'elle ait
appris le format « Contexte → Question → Réponse ». Elle ne l'a **jamais vu** à
l'entraînement : lui coller un passage devant la question aujourd'hui, elle
l'ignorera. Il faut donc des exemples d'apprentissage de cette forme.

Et c'est là qu'est le conflit : fabriquer ces exemples revient à **inventer des
questions et des réponses** à partir des passages — exactement la donnée
potentiellement erronée qui a fait écarter le corpus synthétique. Trois voies,
aucune gratuite :

1. **Extractif mécanique** — la réponse est une phrase *copiée* du passage, jamais
   rédigée. Rien n'est inventé côté réponse ; reste à produire la question sans
   l'inventer non plus, ce qui n'est pas résolu.
2. **Questions réelles** — n'utiliser que des paires question/réponse d'origine
   humaine attestée. Propre, mais il faut la source.
3. **Assumer un générateur** pour la seule *forme*, en acceptant que le contenu
   soit faux, puisque ce qu'on enseigne est « recopie le passage », pas le fait.
   Dangereux : ce qui est vu à l'entraînement s'imprime, même présenté comme forme.

⚠️ Ne pas brancher `--chercher` dans `--parler` avant que ce point soit tranché :
un branchement qui « marche » sans que le modèle sache lire le contexte donnerait
l'illusion d'une réponse sourcée alors qu'elle serait devinée — le pire des deux
mondes, parce qu'elle serait alors *crue*.

---

## 4. Apprendre de ses torts

Dans une conversation : oui, elle reconnaît et corrige (c'est dans le contexte).
**Durablement : non** — un modèle entraîné est figé.

La boucle réelle, à construire : **consigner les corrections** que Rihen lui
apporte → **les verser au corpus** → **réentraîner**. Elle apprend de ses torts
par cycles, pas à l'instant. Ne pas laisser croire autre chose.

---

## 5. Le cap : Ilyana chef d'orchestre du moteur

> Rappel de la règle de cadrage (Rihen) : **l'IA doit servir le moteur**. Si
> l'entraînement long passe avant le branchement au moteur, le projet dérive.

Elle ne « pilote » pas en texte libre : elle **émet des commandes typées**, celles
que le moteur sait déjà exécuter et annuler. C'est le patron déjà prouvé par
`NKMeshAITest` (observation → politique → commande), et c'est ce qui rend les
actions rejouables et apprenables.

| domaine | vocabulaire d'actions | où c'est déjà |
|---|---|---|
| Modéliser | `NkMeshEditCommand`, journal `.nkmec` | NK3DModeler, `NKMeshAITest` ✅ |
| Animer | commandes de pose / IK | `Applications/NkAnima` |
| Parler | synthèse vocale | `Kernel/AI/NKSpeech`, NKTTS (LJSpeech + Griffin-Lim) |
| Interface 2D | `NkUIComponent` (donnée, pas code) | Engine/Noge |
| Peindre | opérations sur `NKImage` / NKCanvas | à définir |
| Civilisation | agents `NKCivilization` | ✅ Jalon 1 |

**Pourquoi ce patron et pas du texte libre** : une commande typée se valide, se
rejoue, s'annule et s'apprend. Du texte libre ne se vérifie pas — et une IA qui
agit sans qu'on puisse contrôler son action est exactement ce qu'on ne veut pas.

### 5bis. L'intégrer dans NKCode, NkAnima, NK3DModeler, NKCinema, PV3DE, Noge

Question posée par Rihen le 2026-08-12 : « on pourra facilement l'intégrer
partout, pour qu'elle manipule directement depuis les binaires ? » Réponse
mesurée sur le code existant, en séparant ce qui est acquis de ce qui reste.

**Ce qui est DÉJÀ en place** (et qui rend la suite crédible) :
- `NKGpt` est une **bibliothèque du noyau réutilisable** : son en-tête l'annonce
  (« n'importe quelle application remplit un `NkGptConfig` »), l'API tient en
  `Prepare()` + `Generate()`. Une app la lie par `dependson("NKGpt")`.
- `--parler` charge un modèle avec `resume=false` : **poids seuls, sans corpus
  ni état d'optimiseur**. C'est déjà le chemin d'inférence.
- **NKCode a son point de branchement** : `NkAiPanel.h` a un sélecteur de
  fournisseur dont l'entrée **`2 = IA maison (NkAI)`** existe déjà à côté de
  Claude et Ollama.
- **NK3DModeler a son vocabulaire d'actions** : `NkMeshEditOp::{Extrude,
  ExtrudeEdges, ExtrudeVerts, Delete, Merge, MakeFace, Subdivide, LoopCut,
  Bevel, Inset, Dissolve, Move}` — annulables, rejouables, journalisables.

**Les trois obstacles RÉELS, dans l'ordre où ils se poseront** :

1. **Le device GPU partagé — le vrai sujet.** NKRenderer tient un device
   NKRHI ; l'inférence de NKGpt passe par NkTensor, qui prend le sien. Deux
   devices dans un même processus, c'est le piège déjà documenté pour
   NKCanvas vs NKRenderer. Trois issues : (a) faire passer l'inférence par le
   device NKRHI de l'application, (b) inférer sur CPU (lent mais sans conflit),
   (c) **processus séparé** qui ne reçoit que des observations et ne rend que
   des commandes typées. La (c) est la plus sûre et découple les cycles de vie
   — c'est celle à privilégier tant que (a) n'est pas mesuré.
2. **Il manque un moteur d'INFÉRENCE PUR.** On passe aujourd'hui par
   `NkGptTrainer`, une classe faite pour entraîner. À mesurer avant d'intégrer :
   ce qu'elle alloue réellement en mode génération (les poids font 123 Mo ; les
   moments d'Adam en ajouteraient 247 inutilement). Chantier : `NkGptInference`
   (poids seuls + KV-cache), ou la garantie mesurée que `resume=false` suffit.
3. **La capacité, et c'est le point à ne pas enjoliver.** Le modèle 32 M est
   entraîné sur de la **prose, de l'identité et de la citation**. Il ne sait
   PAS émettre un `NkMeshEditOp`, et il ne l'apprendra pas par branchement :
   il faut un **corpus d'actions** (observation → commande) par application.
   `NKMeshAITest` a prouvé le patron (mesh → traits → politique → commande).
   « Qu'elle manipule tout ce qu'elle veut » n'est donc ni l'objectif ni
   atteignable : l'objectif est qu'elle **propose des commandes typées que
   l'application valide, exécute et sait annuler**.

**Ordre de marche proposé** : (1) mesurer l'empreinte de `--parler` ;
(2) extraire un `NkIlyanaAgent` partagé (charge le modèle, expose
`Observer/Proposer`) ; (3) le brancher d'abord sur **NKCode** (son panneau
attend déjà le fournisseur « IA maison », et le texte y est la sortie
naturelle) ; (4) puis **NK3DModeler**, première application à ACTION, avec son
vocabulaire déjà écrit ; (5) les autres suivent le même moule.

---

## 6. Ordre de marche

1. 🔶 **Corpus réel** — préparer Wikipédia FR (2,1 Go déjà là) : extraction,
   nettoyage, attribution **CC BY-SA** (cf. `docs/SOURCES_TIERCES.md` et
   `THIRD_PARTY_LICENSES.md`). Abandonner le corpus synthétique.
2. ⏳ **Charte en corpus** — écrire les échanges de la section 2, y compris les
   deux faces de la fermeté.
3. ⏳ **Accélérer le moteur** — GPU à 28 %. Chaque heure gagnée ici vaut des
   jours plus tard.
4. ⏳ **Récupération documentaire** — elle lit avant de répondre.
5. ⏳ **Entraînement à la vraie taille** — 100-150 M, sur données réelles.
6. ⏳ **Boucle de correction**.
7. ⏳ **Orchestration** — un sous-système à la fois, en commençant par celui qui
   a déjà son vocabulaire d'actions : la modélisation.

---

## PDF — PHASE 0 : sondage du corpus avant d'étendre le lecteur (2026-08-13)

**Méthode** : le périmètre du lecteur a été fixé par mesure sur 95 documents
réels (en-tête de `NkPdf.h`). Toute extension suit la même discipline — on
implémente ce que le corpus contient, pas la spécification. Outil :
`NKIlyana --sonder --dossier <dir> [--csv f]` (`NkIlyanaSondePdf.h`), qui lit
les clés via le modèle d'objets (donc à travers les flux d'objets compressés)
et cherche les filtres dans les octets bruts — fiable, car la spécification
interdit qu'un flux vive dans un flux d'objets.

**Corpus** : `D:\softwareRenderer\Rodolf\Cours`, **258 fichiers**, 255 ouverts,
3 chiffrés (refusés — comportement voulu).

| clé | documents | % des ouverts | volume |
|---|---|---|---|
| `/Info` non vide | **255** | **100 %** | dont `/Title` : 89 (35 %) |
| `/Annots` | 194 | 76 % | — |
| ⤷ dont `/Link` | **187** | **73 %** | **56 051 liens** |
| ⤷ dont `/Text` | 4 | 2 % | — |
| ⤷ dont `/Highlight` | 0 | 0 % | — |
| `/StructTreeRoot` | **140** | **55 %** | — |
| `/Dests` | 140 | 55 % | — |
| `/Outlines` | 53 | 21 % | **10 354 signets** (≈195 par document qui en a) |
| `/Metadata` (XMP) | 40 | 16 % | — |
| `/Lang` | 39 | 15 % | — |
| `/AcroForm /Fields` | **2** | **1 %** | — |
| `/Names /EmbeddedFiles` | **0** | **0 %** | — |

| filtre | documents |
|---|---|
| DCT (JPEG) | 104 |
| **LZWDecode** | **5** |
| CCITTFax | 3 · JBIG2 1 · JPX 0 |

### Ce que la mesure change dans le plan — DEUX phases supprimées, UNE remontée

**⛔ Supprimées** (les écrire serait exactement l'erreur que le sondage doit
éviter) :
- **`/AcroForm`** : 2 documents sur 255. Des formulaires administratifs, sans
  intérêt pour une bibliothèque de cours.
- **`/EmbeddedFiles`** : **zéro** document. Rien à écrire.
- **`/Metadata` XMP** : 16 %, et **entièrement redondant** avec `/Info`, présent
  à 100 %. On n'ajoute pas un parseur XML pour une source moins bien couverte
  que celle qu'on a déjà.

**⭐ Remontée — `LZWDecode`, de dernière à deuxième position.** Le sondage
mesurait 0 % sur l'ancien corpus de 95 PDF ; il en trouve **5** sur 258. Et le
croisement avec le balayage de lecture est sans appel : **les 5 sont en échec**
(4 « VIDE », 1 « charabia »). Vérification de causalité faite — le LZW porte
sur les **flux de contenu de page** (7 occurrences sur 10 dans `Gdmphys1.pdf`,
4 sur 4 dans `phys_model.pdf`), pas seulement sur des images : sans lui, ces
pages ne sont jamais décodées.

> C'est le seul chantier qui **débloque des documents entièrement illisibles**
> — 5 sur les 35 encore inaccessibles, soit 14 % du reliquat — pour ~150 lignes.
> Toutes les autres phases enrichissent des documents **déjà lus**.

### Ordre définitif proposé

| ordre | chantier | couverture | pourquoi ici |
|---|---|---|---|
| **1** | `/Info` + `/Lang` | 100 % / 15 % | universel, coût modéré ; `/Lang` est quasi gratuit une fois le catalogue accessible, et dit à Ilyana si un document est français ou anglais |
| **2** | **LZWDecode** | 5 docs | seul chantier qui rend LISIBLE ce qui ne l'est pas ; ~150 lignes |
| **3** | `/StructTreeRoot` | 55 % | la plus forte valeur *qualitative* (ordre de lecture logique = qualité du texte d'Ilyana), mais la plus coûteuse : exige les MCID dans `NkPdfRender` |
| **4** | `/Dests` + `/Outlines` | 55 % / 21 % | découpage naturel en chapitres ; `/Dests` est la dépendance technique de 3 et 5 |
| **5** | `/Annots` `/Link` | 73 % | 56 051 liens ; les URL valent pour les citations. Réutilise la résolution de destinations |

### 📐 Balisage PARTIEL : le seuil qui décide d'appliquer l'ordre logique

Déclarer un `/StructTreeRoot` ne veut pas dire que le balisage est exploitable —
même distinction que « déclaré » contre « effectivement lu » sur `/ToUnicode`.
Mesure de la **part de texte rattachée à aucun MCID**, sur les 140 balisés
(`NKIlyana --balisage`) :

| part hors structure | documents |
|---|---|
| < 1 % | **91** |
| 1–5 % | 19 |
| 5–10 % | 8 |
| 10–25 % | 5 |
| 25–50 % | 5 |
| **≥ 50 %** | **12** |

**22 documents (16 %) dépassent 10 %**, dont trois à **99,5 %, 100 %, 100 %** —
leur arbre existe et ne rattache *rien* au texte réel.

> ⚠️ **Aucun invariant de conservation ne verrait le dégât** : le multiensemble
> des caractères est identique, rien n'est perdu, **tout est déplacé**. C'est
> l'angle mort du contrôle de non-régression, et c'est pourquoi cette mesure
> devait exister avant de brancher.

Effet non anticipé, trouvé en mesurant : sur ces documents, l'ordre logique
serait **pire** que l'actuel. `AssemblerPage` trie aujourd'hui par ligne
(y croissant puis x), ce qui redresse déjà beaucoup ; l'assemblage par structure
ne fait pas ce tri pour le texte non rattaché. À 100 % hors structure, on
remplacerait un tri par ligne par l'ordre de dessin brut.

**Seuil retenu : 10 %** (`kNkPdfSeuilHorsStructure`). Sous ce seuil, le texte
non rattaché est fait d'en-têtes et de folios — quelques mots par page, dont le
déplacement en fin de **page** (pas de document) ne coûte rien. Au-delà, c'est
du corps de texte, et le déplacer serait une dislocation. Le chiffre suit la
distribution : **118 documents sur 140 gardent l'ordre logique** (91 + 19 + 8),
les **22** mal balisés (5 + 5 + 12) restent en ordre visuel, et la trace dit
lequel s'applique.

> ⚠️ **DEUX populations de 118, et ce ne sont PAS les mêmes documents.**
> Nommées distinctement partout, sous peine de confusion garantie :
> - **118 « sans structure »** = les documents sans `/StructTreeRoot` ;
> - **118 « balisés exploitables »** = les balisés dont le texte hors structure
>   est sous le seuil de 10 %.
>
> Attendu du contrôle de non-régression, révisé en conséquence :
>
> | population | documents | attendu |
> |---|---|---|
> | sans structure | 118 | **identité stricte** |
> | balisés **au-dessus** du seuil | 22 | **identité stricte** — ils retombent sur le chemin actuel |
> | balisés exploitables | 118 | **peuvent différer** ; la proportion qui diffère est elle-même une mesure |
>
> Soit **140 documents en identité stricte**, un seul écart arrête tout. Et il
> faut **vérifier**, non supposer, que le repli des 22 emprunte *exactement* le
> chemin actuel : s'il passe par une variante de l'assemblage, l'identité n'est
> plus garantie. Test à UN document avant les 258.

### 🛡️ Baseline de non-régression du texte — capturée et VALIDÉE (2026-08-13)

`Applications/NKIlyana/reference/empreintes_pdf.csv` — **versionné**, pris sur
le commit `df92fe14`, **avant** toute modification du rendu.

Par document : `struct`, pages, **passages**, caractères, **empreinte FNV-1a**
du texte assemblé. Le hash dit *que* ça a changé ; les compteurs disent *de
combien et dans quel sens* — c'est ce qui évite la bissection.

**Validée par trois contrôles, inscrits dans l'en-tête du fichier** (une
baseline trouée est pire que pas de baseline : elle donne une assurance fausse
sur la partie manquante) :

| contrôle | résultat |
|---|---|
| complétude | **258 / 258** documents |
| lignes à zéro caractère | 26, **toutes expliquées** ; 0 hash absent ou malformé |
| population `struct` | **140 / 258 = 54,3 %** (attendu ~55 %) |

Le 26ᵉ document à zéro (`lightning.pdf`) n'est pas un scan : **100 % de ses
caractères sont illisibles**, donc l'assemblage les saute tous. Le balayage
comptait les caractères *rencontrés*, la baseline mesure le texte *assemblé* —
les deux mesures sont cohérentes, et il fallait le vérifier plutôt que
l'arrondir.

### 📐 Identité d'un bloc marqué : (page, MCID) suffit-il ? — MESURÉ

Question soulevée par Rihen : les MCID sont numérotés **par flux de contenu**,
pas par page. Avec un Form XObject, le même MCID 3 peut exister dans le flux de
la page ET dans celui du formulaire — le texte serait alors rattaché au mauvais
nœud de structure. Erreur silencieuse et plausible, le même mode d'échec que
l'entrelacement de colonnes.

| sonde | résultat |
|---|---|
| `/MCR` portant une clé `/Stm` (**borne exacte**) | **0 document, 0 occurrence** |
| documents balisés avec Form XObject (**borne supérieure**) | 21 |

`/Stm` est le signal que la spécification impose quand le contenu marqué vit
hors du flux de la page. **Il est absent de tout le corpus** : les 21 documents
balisés à formulaires n'y placent donc aucun contenu marqué.

> **Décision : `(page, MCID)` suffit pour ce corpus**, et le code le dira — avec
> le chiffre et la date, pas comme une hypothèse. Si un jour un document dérape,
> la prochaine session saura exactement quoi revérifier : relancer `--sonder` et
> regarder si `/MCR /Stm` est passé au-dessus de zéro.

### 📐 Valeur marginale des signets, une fois la structure livrée

Les trois chiffres sont publiés **ensemble**, pour qu'ils se vérifient l'un
l'autre — une intersection annoncée seule ne se contrôle pas :

| | documents | % des 255 ouverts |
|---|---|---|
| **A.** total `/Outlines` | **53** | 20,8 % |
| **B.** intersection (signets **et** `/StructTreeRoot`) | **9** | — |
| **C.** différence (signets **sans** structure) | **44** | 17,3 % |
| contrôle | **B + C = A** → 9 + 44 = 53 ✅ | |

**Les deux populations sont presque disjointes** : 9 documents en commun sur 53.
Les producteurs qui balisent ne posent pas de signets, et réciproquement. La
structure ne subsume donc **pas** les signets dans ce corpus — contrairement à
ce qu'on pouvait attendre de `H1..H6`. `/Outlines` garde sa place juste après
la structure : sans lui, **44 documents resteraient sans aucun découpage**.

### 📐 Charset CFF — mesuré, et DIFFÉRÉ (2026-08-13)

Hypothèse proposée par Rihen : les 20 polices muettes des documents LaTeX ne
seraient pas indéchiffrables, mais des **CFF** (`/FontFile3 /Subtype /Type1C`)
dont les noms de glyphes sont enfermés dans le charset — jamais analysé
(`charset` apparaît zéro fois dans `NkPdfFont.cpp`). Le dispositif d'aval
existe déjà : nom → AGL → Unicode, celui qui a récupéré 24 des 29 refusés.

**L'hypothèse technique est JUSTE.** La mesure la confirme :

| | documents | % | polices |
|---|---|---|---|
| avec un `/FontFile3` (CFF) | 20 | 8 % | — |
| avec du `/Subtype /Type1C` | 18 | 7 % | 854 |
| **dont MUETTES** (ni `/ToUnicode` ni `/Differences`) | **12** | **5 %** | **563** |

12 documents, donc au-dessus du seuil de « une dizaine » — le chantier semblait
décidé. **Mais le croisement avec le balayage de lecture le renverse :**

| verdict des 12 | nombre |
|---|---|
| **déjà lus avec succès** | **10** |
| en échec (`lightning.pdf`, `phys_model.pdf`) | **2** |

Et parmi les 10 déjà lus, le taux de caractères illisibles est de **0 % à
0,3 %** — sauf `Creajeux-plaquette.pdf` (24,8 %, une plaquette graphique).
Autrement dit, ces polices Type1C muettes servent des **symboles secondaires**
dans des documents dont le corps de texte se lit parfaitement.

> **Gain réel estimé : 2 documents débloqués et 1 amélioré, pas 12.** Le
> compteur brut disait 12 ; la question utile — « combien de documents cela
> rend-il lisibles ? » — répond 2. C'est le même piège que le LZW, attrapé
> cette fois **avant** d'écrire le code.

**Décision : DIFFÉRÉ, au profit de `/StructTreeRoot` (55 % du corpus).** Le
chantier CFF reste juste techniquement et sera rouvert si le fonds évolue (il
suffirait d'une série de documents LaTeX récents pour changer le calcul) — le
mode `--sonder` mesure désormais cette population, la question se retranchera
en une commande.

---

### 🔶 PHASE 2 — LZWDecode livré, nécessaire mais PAS suffisant

**Le filtre marche, et la mesure le prouve** (`Gdmphys1.pdf`) :

| | avant | après |
|---|---|---|
| contenu décodé | **0 octet** | **144 264 o** |
| opérations exécutées | 0 | 10 536 |
| ordres de texte | 0 | 814 |
| caractères rencontrés | **0** | **24 014** |

Les 4 `Gdmphys*.pdf` passaient pour des documents-images ; ils ne l'étaient pas.
Leur contenu était simplement **compressé avec un filtre que nous ne savions pas
lire**. Idem pour `phys_model.pdf` (216 322 caractères désormais rencontrés).

**Mais aucun des 5 n'est déposé pour autant, et il faut le dire** : ces
documents ont un **second défaut, indépendant** — **0 table `/ToUnicode`
déclarée**, et seules 4 polices sur 24 portent des `/Differences`. Les
caractères sont donc rencontrés sans qu'on sache ce qu'ils représentent :
96 % restent sans équivalent, et le garde-fou les refuse (à raison).

> **L'estimation « 5 documents débloqués » était optimiste. La mesure dit : 0
> document débloqué, mais une cause éliminée et la suivante isolée.** Le gain
> réel du LZW est ailleurs : tout flux LZW du fonds se décode maintenant, et ces
> 5 documents ne sont plus classés « image » à tort — ce qui aurait envoyé
> chercher un OCR pour rien.

Implémentation : variante TIFF, codes 9→12 bits en **gros-boutiste** (le LZW de
GIF lit à l'envers : les confondre rend du bruit dès le troisième code), cas
`KwKwK` traité, `/EarlyChange` respecté (défaut 1), dictionnaire en
(préfixe, suffixe) — stocker les chaînes entières coûterait 16 Mo pour rien.

---

### ✅ PHASE 1 LIVRÉE — `/Info` + `/Lang` (`NkPdfInfo.{h,cpp}`)

**Mesure sur les 258 PDF** :

| | résultat |
|---|---|
| dates analysées | **246 (96 %)** |
| titres lus | 92 (36 %), dont **16 accentués** |
| langues lues | 39 (15 %) |

**PDFDocEncoding résolu par NOMS DE GLYPHES, jamais par valeurs recopiées.** Les
deux plages qui diffèrent de Latin-1 (0x18–0x1F et 0x80–0x9F) sont écrites en
clair (`bullet`, `dagger`, `emdash`, `quotedblleft`…) et résolues via l'Adobe
Glyph List déjà embarquée. Deux raisons : une transcription manuelle de table a
déjà produit un décalage d'indexation dans ce dépôt, alors qu'un **nom mal
orthographié rend une chaîne vide et se voit immédiatement** ; et l'AGL est déjà
validée et attribuée, donc aucune donnée n'est dupliquée.

**Piège évité, et il valait le détour** : à la première mesure, les titres
sortaient **sans accents** (« Transformee », « Prasentation »). Avant de
corriger quoi que ce soit, vérification : le compteur, lui, détectait bien 16
titres non-ASCII — donc la chaîne *contenait* les accents. C'était la **console
Windows** qui les mangeait, pas la lecture. Relus depuis un fichier UTF-8 :
`PowerPoint-Präsentation`, `Compilation séparée`, `Transformée de Fourier`,
`Écrire vos propres mathé…`. *Un test peut échouer pour la mauvaise raison — et
« réparer » un défaut inexistant aurait cassé du code correct.*

Sont aussi gérés : l'UTF-16BE avec indicateur d'ordre (paires de substitution
comprises), l'UTF-16LE (hors spécification, mais des producteurs en écrivent),
et les dates tronquées — `D:2019` comme `D:20190312150405+02'00'` sont l'une et
l'autre exploitées, au lieu de rejeter la date entière.

---

⚠️ **Un point bloquant relevé et corrigé** : `Trailer()` et `Catalog()`
n'étaient pas exposés (`mTrailer`/`mRoot` privés). Or `/Info` vit dans le
trailer et tout le reste dans le catalogue — aucune couche externe ne pouvait
les atteindre. Deux accesseurs **en lecture seule, strictement additifs** ont
été ajoutés à `NkPdf.h` : c'est le minimum indispensable, et ils n'exposent
rien de plus que ce que le modèle d'objets rend déjà public.

---

## PDF — ✅ RÉSOLU le 2026-08-11 : trois causes distinctes, trois correctifs

Les 3 documents nommés du lot sont tous lus désormais (mesures avant → après) :

| document | avant | après |
|---|---|---|
| ebin.pub 2D graphics (466 p.) | 96 % illisibles, REFUSÉ | **0 illisible**, 1513 passages |
| Game Audio CppCon (156 p.) | 98 % illisibles, REFUSÉ | **0 illisible**, 546 passages |
| computergraphics OpenGL 2e (535 p.) | **0 page** (ne s'ouvrait pas) | 0,13 % illisibles, 3744 passages |

**Cause 1 — parseur CMap multi-sections** (`NkPdfFont.cpp::ParseToUnicode`).
`while (!keyword(i, "endbfchar"))` testait le mot-clé sur l'espace *avant* lui —
jamais vrai — et `readHex` sautait par-dessus la fin de section : la section
`bfrange` suivante était dévorée comme des paires `bfchar`, les plages jamais
déployées. Un CMap à section unique passait (d'où les PDF qui marchaient) ; un
CMap multi-sections (générateurs d'ebooks) était mutilé en silence. Fix :
sauter les blancs, tester le mot-clé de fin, refuser d'avancer hors `<`.

**Cause 2 — pas de repli sur l'encodage de base** (`NkPdfFont`). Les PDF sortis
de PowerPoint/Word écrivent leur texte en polices TrueType simples **sans**
`/ToUnicode` mais avec `/Encoding /WinAnsiEncoding` — encodage **publié par la
spec** (ISO 32000, annexe D). Fix : tables WinAnsi/MacRoman générées par script
(codecs cp1252/mac_roman, jamais retapées à la main), repli dans `ToUnicode()`.
`/Differences` non implémenté (0 occurrence dans les documents du fonds testés).

**Cause 3 — chaîne `/Prev` bornée à 32** (`NkPdfLoad.cpp::LoadXrefAt`). Un livre
retouché sous Acrobat portait **53** mises à jour incrémentales ; le corps
d'origine (catalogue, pages) est au bout de la chaîne → « 0 page » sans un mot.
Fix : anti-cycle par liste d'offsets visités (`mXrefSeen`), profondeur 1024 en
garde-fou.

Au passage : le cache de polices était par **nom seul** (`/F4`) — deux polices
homonymes de ressources différentes (formulaires XObject) se partageaient un
slot. Corrigé (clé = nom + identité du dictionnaire), même si ce n'était pas la
cause ici. Et la sonde de diagnostic est désormais **non biaisée** : relevée au
moment exact de l'échec `ToUnicode`, sur la police effectivement interrogée
(l'ancienne comparait les premiers codes du flux à la table de la *dernière*
police vue — deux objets sans rapport).

**Balayage complet du dossier Cours après correctifs — 258 PDF** :
**199 acceptés (77 %)** contre ~17 % avant ; 29 « vides » (~26 scans que seul
un OCR lirait, 2 chiffrés refusés franchement) ; 29 refusés charabia = famille
**LaTeX/dvips Type1 sans `/ToUnicode`** (arXiv, notes Eberly).

**Cause 4 (2026-08-11, sur demande de Rihen) — décodage par NOMS de glyphes** :
`/Encoding /Differences` (spec PDF) et, à défaut, la table en CLAIR du
programme Type 1 (`dup N /nom put`, avant eexec — aucun charstring interprété)
donnent code → nom ; la **Adobe Glyph List** (donnée publiée BSD-3, 4281
entrées générées par script — `NkPdfGlyphList.{h,cpp}`, attribution dans
`THIRD_PARTY_LICENSES.md`) donne nom → Unicode. **Mesure : 24 des 29 refusés
récupérés** (arXiv à 0 % d'illisible, ligatures `fi` comprises) → le fonds
Cours passe à **223/258 lisibles (86 %)**. Les 5 restants portent des noms
opaques de sous-ensemble (`/a35`) : seul le dessin des glyphes sait ce qu'ils
sont — OCR ou rien. Pour les sources LaTeX : préférer le `.tex`, toujours.

**Au passage, bug racine dans l'inflate du dépôt** (`NkDeflate::zFill`,
NKImage) : à l'épuisement du flux d'octets il déclarait une erreur, même quand
les derniers symboles étaient déjà dans le registre. Les flux **zlib**
(PDF/PNG) n'y tombaient jamais — leurs 4 octets d'Adler-32 servaient de marge
silencieuse — mais tout flux **brut** fini à l'octet près (corps gzip d'une
réponse HTTP, entrées ZIP) était rejeté après avoir été entièrement décodé.
Fix : zéros bornés injectés en fin de flux. Mesuré : le gzip forcé de
`gamemath.com` se décode ; chemin zlib inchangé (mêmes 546 passages sur le
PDF témoin).

---

## (historique) PDF — état au 2026-08-11 avant résolution, et LE point d'entrée exact

**La « régression » du jour n'existait pas** : le fichier de test avait été
déplacé lors d'un filtrage du fonds. Le programme rendait « 0 octet, 0 opération »
sur un chemin vide — indiscernable d'un lecteur en panne. Trois reconstructions et
deux hypothèses fausses (objets périmés, puis conflit mbed-TLS) ont été dépensées
avant de poser la question élémentaire : *le fichier est-il encore là ?*
→ `--ajouter` dit désormais **INTROUVABLE**, avec la raison.

**TLS remis en OPTION** (`NK_ENABLE_TLS=1` active mbedTLS *et* `bcrypt`). Il avait
été mis par défaut, puis soupçonné à tort ; l'isolement l'a innocenté. L'option
propre est ce qui a permis cet isolement, elle reste donc telle quelle.

### La mesure a eu lieu, et elle RÉFUTE l'hypothèse principale

```
codes DEMANDÉS par le flux    : 21  39  38  82  80  83
codes CONTENUS dans la table  :  4   6   8  65  66 107   (28 entrées)
```

Les deux séries sont de **petits entiers du même ordre** : ce n'est donc PAS un
décalage « un octet contre deux ». Cette piste est fermée.

### ⚠️ MAIS l'instrumentation est biaisée — à corriger AVANT de conclure

Les codes demandés viennent des **premières** opérations de texte, tandis que la
table affichée est celle de la **dernière** police vue (`LastFont()`). Ce sont
potentiellement deux polices différentes : on compare des pommes et des poires.

**LE point d'entrée** : relever la table de la police **effectivement
interrogée** — c'est-à-dire, dans `NkPdfRenderer` au moment où `emit` appelle
`f->ToUnicode(code)`, enregistrer une fois le couple (code demandé, présence dans
`f`) plutôt que de comparer après coup deux objets sans rapport. Une dizaine de
lignes, une exécution, et la cause devrait être visible.

### Ce qui est déjà acquis et ne doit pas être refait

- Les tables `/ToUnicode` **se lisent toutes** (273 006 déclarées, 273 006 lues) :
  ni la décompression ni le parseur `bfchar`/`bfrange` ne sont en cause.
- Le chemin `/Encoding` → noms de glyphes **ne concerne pas ces documents** : ils
  déclarent bien leurs tables. Ne pas rouvrir cette voie.
- Une police sans programme de glyphes reste **lisible** (correctif conservé), et
  un document dont plus de 25 % des caractères sont indéchiffrables est **refusé**
  plutôt que déposé en charabia.
- Sur 12 PDF réels : 6 sont des documents-images (0 caractère même pour
  `pdftotext`, confirmé indépendamment par 0 ordre de texte côté moteur) — hors de
  portée sans reconnaissance de caractères. L'écart à combler est de **4**.

## (historique) Régression supposée du 2026-08-11 — RÉSOLUE, aucun défaut de code

**Symptôme** : `--ajouter` sur un PDF qui fonctionnait rend désormais
`contenu 0 o, 0 operations`. Le document n'est plus ouvert du tout. Le même
fichier donnait auparavant 21 Mo de contenu et ~2 000 000 d'opérations.

**Ce qui a changé entre les deux mesures**, et rien d'autre :
1. `NKNetwork.jenga` : TLS passé d'opt-in à **actif par défaut** ;
2. `NKIlyana.jenga` : ajout de `NKMbedTLS` aux dépendances ;
3. `NKIlyana.jenga` : ajout de `bcrypt` aux bibliothèques Windows ;
4. `NkPdfFont.h` : deux accesseurs **inline** (pas de changement de disposition
   mémoire, donc peu suspect).

**Écarté** : les objets périmés. NKMedia ET NKIlyana ont été entièrement
reconstruits, la régression persiste.

**Prochaine étape — COUPER LE SUSPECT, pas le réparer.** Reconstruire avec
`NK_DISABLE_TLS=1` (et sans `NKMbedTLS` ni `bcrypt` dans NKIlyana.jenga) puis
relancer la même mesure. Si la lecture PDF revient, la cause est bien le lien ;
sinon elle est ailleurs et les trois changements ci-dessus sont innocents. Cette
mesure doit être faite AVANT toute tentative de correction.

**Hypothèse à vérifier ensuite** : un conflit de symbole entre mbed-TLS et le
décompresseur utilisé par le PDF — les deux embarquent du code de bas niveau, et
l'éditeur de liens choisit alors silencieusement l'un des deux.

⚠️ En attendant, l'aspiration de sites fonctionne (elle a besoin de TLS) mais la
lecture PDF ne fonctionne plus. Les deux ne peuvent pas coexister dans cette
construction — c'est précisément ce qu'il faut résoudre.

## PDF — diagnostic du 2026-08-10 sur le fonds réel (D:\softwareRenderer)

**Le fonds** : 8,4 Go, 267 PDF, 175 archives, 67 vidéos.

**Mesure sur 12 PDF, avec `pdftotext` comme ORACLE boîte noire** (légitime : on
compare des sorties, on ne lit aucun code) :

| | résultat |
|---|---|
| PDF dont l'oracle tire du texte | **6 / 12** |
| PDF à zéro caractère **même pour l'oracle** | **6 / 12** — documents-images, hors de portée sans OCR |
| PDF acceptés par notre lecteur | **2 / 12** |

**L'écart à combler est donc de 4 documents**, pas de 10. La moitié du fonds est
constituée de scans que rien ne lira sans reconnaissance de caractères.

### ⚠️ L'hypothèse « il manque /Encoding » était FAUSSE

J'allais implémenter `/Encoding` → noms de glyphes → Unicode. Une sonde des
dictionnaires de polices (script Python, diagnostic seulement) a montré que ces
documents **ONT leur table `/ToUnicode`** :

| document | polices | avec /ToUnicode |
|---|---|---|
| ebin.pub 2d-computer-graphics | 17 | **17 (100 %)** |
| computergraphics OpenGL | 491 | 417 (85 %) |
| Game Audio Programming | 11 | 9 |

Le chantier `/Encoding` aurait donc réparé un défaut qui n'est pas celui-là.
**Mesurer avant d'implémenter a évité un travail entier à côté de la plaque.**

### Le vrai défaut, localisé

Sur `ebin.pub` (17/17 polices avec table) notre lecteur rend :
`695450 caractères rencontrés, dont 666545 sans équivalent lisible (96 %)`.

La table existe, elle est trouvée, **et elle ne rend rien**. Les polices en cause
sont massivement **Type0 / CIDFontType2 en encodage Identity** — deux octets par
caractère. Pistes à examiner dans cet ordre :

1. `ParseToUnicode` : `DecodeStream` échoue-t-il sur le flux CMap (filtre non
   géré) ? Un échec y est **silencieux** — la police reste valide, simplement
   sans aucune entrée. C'est la piste la plus probable.
2. Le code cherché dans `mUniCodes` est-il bien celui que `NextCode` produit pour
   une police à deux octets ?
3. `ToUnicode()` fait une **recherche linéaire** sur des milliers d'entrées, pour
   chacun des ~700 000 caractères : correct mais coûteux — à indexer une fois le
   fond réparé.

### ✅ Instrumentation posée, et elle a tranché

Deux compteurs distinguent désormais « le document ne déclare rien » (limite du
document) de « il déclare une table que nous ne savons pas lire » (notre défaut) :
`AvaitToUnicode()` vs `HasToUnicode()`, remontés dans les `Stats` du rendu.

**Mesure sur `ebin.pub` : 273 006 tables déclarées, 273 006 effectivement lues.**

⛔ **La piste 1 est RÉFUTÉE** : les tables se décompressent et se lisent toutes.
Le `bfchar`/`bfrange` fonctionne.

**Il ne reste donc qu'une explication**, et elle est précise : le code présenté à
`ToUnicode(code)` **n'est pas celui que la table indexe**. Ces polices sont des
Type0/CIDFontType2 en Identity — deux octets par caractère —, et la table indexe
ces codes à deux octets. Le point de rupture est donc dans `NextCode()` (lecture
du code dans la chaîne) ou dans ce que l'appelant transmet ensuite.

**Point d'entrée pour la prochaine session** : instrumenter `NkPdfFont::ToUnicode`
pour afficher, sur les 20 premiers appels d'un document en échec, le code demandé
à côté des 5 premiers codes présents dans `mUniCodes`. Si les ordres de grandeur
diffèrent (par exemple un octet contre deux), la cause est immédiate. C'est UNE
mesure, et elle devrait clore le sujet.

⚠️ Ne pas repartir sur `/Encoding` + noms de glyphes : cette voie a été explorée
et ne concerne pas ces documents.

## Journal du 2026-08-10 — ce qui a marché, ce qui a échoué

**PHASE 2 (identité) — RÉUSSIE.** Batterie **4/19 → 8/19**. Elle nomme son père et
sa mère. Corpus : 25 % d'identité entrelacée avec de la prose (`--melange`).
Modèle promu : `ilyana_phase2.nkgp`.

**PHASE 3 (citation) — ÉCHOUÉE deux fois, la troisième invalidée.**

| essai | pas d'apprentissage | batterie | citation | verdict |
|---|---|---|---|---|
| 3a | 1e-05 (calendrier hérité) | 6/19 | non | non promu |
| 3b | 2e-04 (calendrier neuf) | 5/19 | non | non promu |
| 3c | 1e-04 | 4/19 | non | **NUL** — run mort au pas 30 |

⚠️ **Le verdict 3c ne vaut rien** : le run s'est arrêté après 161 s. J'ai mesuré
une batterie sur un modèle entraîné trente pas et failli en conclure quelque
chose. Toujours vérifier qu'un run VIT avant d'attendre son résultat.

### Trois défauts du MOTEUR trouvés en cherchant pourquoi

1. **Reprendre un run ≠ commencer une phase.** Le calendrier du pas d'apprentissage
   se prolongeait, si bien qu'une phase destinée à enseigner un comportement
   héritait d'un pas déjà au plancher (1e-05). Assez pour graver un nom répété des
   milliers de fois — d'où la réussite de la phase 2 — très insuffisant pour un
   geste nouveau. → `--nouvelle-phase` (commit `f5b62505`).

2. **Les fenêtres coupaient les exemples, et lui apprenaient à INVENTER.** Le lot
   était prélevé à un décalage tiré au hasard dans un flux plat de tokens. Un
   exemple structuré de ~180 tokens dans une fenêtre de 256 n'est entier qu'une
   fois sur trois — et quand la fenêtre commence au milieu du contexte, le modèle
   voit une question suivie de « Reponse: » avec un contexte **amputé**, et on lui
   apprend à produire une phrase qui n'y figure pas. C'est exactement le
   comportement qu'on cherchait à combattre. → une fenêtre sur deux démarre à un
   début de bloc (commit `ce296ee0`). **Hypothèse encore non testée** au moment où
   ces lignes sont écrites.

3. **Le filet de sécurité tuait des runs légitimes.** Il calculait
   `(initiale − actuelle) / initiale` et concluait « perte figée » dès que le
   résultat était négatif. Or une perte qui **monte** est du mouvement : le calcul
   a lieu, et une hausse au démarrage d'une phase, pendant la montée en puissance
   du pas, est normale. Il devait regarder la **valeur absolue**. Un garde-fou qui
   tue ce qu'il devait protéger coûte le run *et* la confiance dans la mesure.

**3d — fenêtres alignées, run complet (94 min) : batterie 6/19, citation NON.**
L'alignement a fait remonter la batterie de 5 à 6, sans donner le geste. Signe
instructif : elle produit désormais le *style* d'une réponse sourcée
(« "Le nom de la localité", est attesté sous les formes… ») — elle a appris à quoi
RESSEMBLE une citation, pas à lire celle qu'on lui donne.

**Bilan : trois essais valides, trois échecs, chacun testant un mécanisme
différent (pas d'apprentissage, alignement, dosage). Ce n'est donc plus un
problème de recette.**

### Ce qu'il reste à examiner maintenant que les recettes sont épuisées

L'alignement des fenêtres écarté, la piste suivante n'est plus une recette mais la
**taille** : recopier une phrase depuis un contexte demande au modèle d'apprendre
à faire correspondre des motifs à distance, ce qui est connu pour n'émerger qu'à
partir d'une certaine échelle. Ce serait alors un argument pour le passage à
~32 M paramètres (déjà envisagé pour le vocabulaire 32k), et non pour un réglage
de plus.

## Ce qui est livré (2026-08-09)

- **`NKData/NkBpeTrainer`** — BPE à l'échelle : mots uniques pondérés, comptes
  incrémentaux, tas à invalidation paresseuse. **16 128 fusions sur 25 Mo en
  1,6 s** là où l'ancien plafonnait à 800 Ko. Prouvé par `NKBpeTest` (19 OK) :
  comptes confrontés à un recomptage complet, segmentation d'entraînement ==
  segmentation d'encodage, réversibilité octet pour octet.
- **`Applications/NKIlyana`** — `--data` (tri en trois bacs + identité +
  dialogues), `--train`, `--causer`.
- **Identité dans les POIDS** : au pas 2000, elle répond « Mon père est TEUGUIA
  TADJUIDJE Rodolf Sederis ». Une consigne système n'aurait rien laissé.
- **Garde-fou** : l'entraînement s'arrête si la perte n'a pas bougé après 30 pas
  (prouvé sur la configuration B=24 qui échouait en silence).

## Bugs et pièges connus

- ⚠️ **Au-delà d'une certaine taille de lot, le GPU ne calcule plus, en
  silence** : à B=24 la perte reste collée à `ln(V)` et le run paraît 3,6× plus
  rapide. **Cause racine non identifiée** (aucun défaut d'allocation signalé) —
  à chercher côté dispatch/submit. Contournement : B=12/accum=2, validé.
- ⚠️ **Corpus en CRLF** : le découpage cherche `"\n\n"` et n'en trouve aucun →
  un seul bloc, masquage inopérant, sans message. Normaliser à la lecture.
- ⚠️ **Runtime C++ dédoublé** (mingw64 vs ucrt64) : corruption de tas dans
  glslang au premier noyau compute. Corrigé par `-static-libstdc++ -static-libgcc`.

## ⚠️ Condition de faisabilité d'ilyana-code : l'attention doit passer EN FLUX

**Constat mesuré (14 août 2026).** `NkRoPEAttention::Forward` **matérialise** la
matrice de scores : trois tenseurs `[B, têtes, T, T]` successifs par couche
(`scores`, sa version mise à l'échelle, `attn`), et le backward de `SoftmaxCausal`
retient sa sortie — `attn` survit donc jusqu'à la passe arrière.

Ce terme croît en **T²**. De T=256 à T=2048, il est multiplié par **64**, pas par 8 :

| T | terme d'attention (B=6, 8 têtes, 12 couches) | à B=1 |
|---|---|---|
| 256 | 432 Mo | 72 Mo |
| 512 | 1 728 Mo | 288 Mo |
| 1 024 | 6 912 Mo | 1 152 Mo |
| 2 048 | 27 648 Mo | 4 608 Mo |

**Conséquence.** RoPE a été retenu parce qu'il permet d'étendre le contexte sans
réentraîner — c'est l'argument qui a emporté la décision d'architecture, et c'est ce
qui rend `ilyana-code` possible (lire des fichiers source demande un long contexte).
**Mais l'extension n'est PAS accessible par la seule mémoire disponible** : même à
B=1, T=2048 coûte 4,6 Go sur ce seul terme, sur une carte de 8 Go dont ~800 Mo
partent au bureau.

**Une attention calculée par blocs, sans jamais stocker la matrice complète, est
donc un PRÉ-REQUIS à toute extension de contexte** — un chantier, pas un réglage.
Toute extrapolation linéaire de la marge mémoire serait fausse d'un facteur 8 à
T=2048.

⚠️ Ne pas confondre avec une optimisation de vitesse : ici c'est la FAISABILITÉ
qui est en jeu, pas le confort.

### Ordre de grandeur du chantier « attention par blocs »

Chiffré grossièrement, pour qu'`ilyana-code` ne repose pas sur une étape dont le
coût est inconnu.

**Ce qu'il faut écrire** : un noyau qui, pour chaque bloc de requêtes, parcourt les
blocs de clés/valeurs en tenant un maximum courant et une somme courante
(softmax en ligne), sans jamais matérialiser `[B, têtes, T, T]`. Plus son backward,
qui doit **recalculer** les scores par blocs au lieu de les relire.

| poste | estimation |
|---|---|
| noyau avant (softmax en ligne, tuilage) | 2 à 3 jours |
| noyau arrière (recalcul par blocs — la partie délicate) | 3 à 5 jours |
| équivalence numérique contre le chemin actuel, avant ET arrière | 1 jour |
| réglage de la taille de tuile selon la mémoire partagée | 1 à 2 jours |
| **total** | **7 à 11 jours** |

**Ce qui rend l'estimation fragile** : le backward par recalcul est l'endroit où
ce genre de noyau se casse en silence (indexation du masque causal, cumul des
maxima). L'oracle existe — le chemin actuel — donc la vérification est possible,
mais c'est elle qui prendra le temps, pas l'écriture.

**⛔ CHIFFRÉ ET ÉCARTÉ POUR L'INSTANT (2026-08-15).** Le profil par noyau mesure
ce que ce chantier optimiserait : `bmatmul` + `softmax_causal` + `softmax_bwd` =
**1,8 à 2,0 %** du temps d'un pas. Plafond de gain : **×1,02**, pour 7 à 11 jours
de travail. À rouvrir quand les cinq lignes de l'ordre de bataille (§ *Rendement
du moteur d'entraînement*) seront tombées — pas avant. La condition de faisabilité
d'`ilyana-code` reste vraie ; c'est son moment qui est faux.

## ⏳ À FAIRE — réglage d'identité APRÈS le grand run (décision Rodolf, 15/08/2026)

**Décision prise, branche 3 sur trois.** L'identité d'Ilyana passe par un
**réglage après le grand run**, pas par le corpus de base.

| | elle sait | modifiable |
|---|---|---|
| 1 · contexte système seul | non | une ligne |
| 2 · corpus de base *(ancien choix)* | oui | réentraînement |
| **3 · réglage après le run** ✅ | **oui** | **quelques heures** |

**La raison est la dose, et elle est mesurée** : le jeu de 155 formulations pèse
**0,057 %** du signal fondu dans le socle, contre **100 %** pendant un réglage
dédié — facteur **~1 800**. La branche 2 demandait à 130 000 tokens de tenir tête
à 438 millions.

### Ce que ça implique, concrètement

- [x] le fragment d'identité est **déjà séparable** — contigu, octets 0 à 516 149,
      pur à 99,4 % → interrupteur de configuration, **pas** de re-tokenisation ;
- [ ] **le grand run tourne sur la prose seule** ;
- [ ] le fragment est **CONSERVÉ TEL QUEL** — les 155 formulations deviennent le
      jeu de réglage post-run. **Ne pas le réduire, ne pas le jeter** ;
- [ ] **forme finale : les poids ET le contexte** — le réglage pour qu'elle sache,
      le contexte système comme filet corrigeable en une ligne quand les poids
      hésitent. L'opposition entre les deux documents venait de croire qu'il
      fallait choisir.

### Documents à corriger — les DEUX, pas un

| document | état |
|---|---|
| `Cours_Socle_LLM/md/07-rendre-utile.md` (« jamais dans les poids ») | Claude s'en charge |
| `Applications/NKIlyana/CARNET.private.md` §8 (« jamais dans une consigne système ») | ✅ **corrigé le 15/08** |

La branche 3 n'était envisagée par **aucun** des deux : elle est née de la mesure
du 14 août. Chacun disait « jamais l'autre », et la réponse était « les deux, dans
le bon ordre et à la bonne dose ».

### Vérification après le réglage

Rejouer le harnais des 30 cas — en particulier les **7 cas de généralisation** et
les **4 pièges de parenté**, dont l'absence du corpus a été vérifiée. Référence
actuelle sur point de reprise faible : **11/30**, dont 1/7 en généralisation et
3/5 sur les formulations présentes dans le corpus (Fisher p = 0,222 : le contraste
n'est pas encore significatif, le point de reprise est trop jeune).

## ✅ Chantier n°1 — ne plus monter de zeros : MESURE, gain x1,57 (16 aout 2026)

Le poste n°1 de l'ordre de bataille est **clos par la mesure**, pas seulement par
le correctif.

| | |
|---|---|
| gain mesure | **36,5 %** du temps d'entrainement, acceleration **x1,57** |
| protocole | 8 courses, meme binaire, interrupteur `NK_ZEROS_LEGACY` |
| separation | plages disjointes : `LEGACY` le plus rapide (71,0 s) > `NEUF` le plus lent (62,8 s) |
| appariement le plus defavorable | **11,5 %** |
| `~upload` | 7 840 -> 560 appels ; 17 143 -> 400 ms (**−97,7 %**) |
| trafic | **12,77 Go/pas -> 4,27 Mo/pas** |

**Le gain depasse la borne annoncee (18,2-19,9 %) parce que la borne ne comptait
que la ligne `~upload`.** `ToGPU` faisant `CreateBuffer` puis `Upload`, supprimer
la bascule supprime aussi 2 704 allocations par pas : 41 % du gain vient de la,
et non de la ligne des uploads. L'extrapolation « reserve de tampons a ~30-34 % »
est confirmee.

### Ce que la mesure a corrige dans sa propre methode

1. **Alterner les modes ne suffit pas, il faut alterner l'ORDRE.** Premiere
   tentative : `NEUF` toujours en second, alors que la machine s'accelere de
   **25 %** au fil des courses (`LEGACY` : 94,9 -> 71,0 s sans rien changer).
   L'avantage de position se deposait entierement sur un bras. Resultat encadre
   entre 11 et 40 % — donc aucun resultat.
2. **Un temoin non touche qui bouge invalide la comparaison.** La comparaison
   naive avec le profil du 15/08 donnait la fenetre 41 % PLUS LENTE et
   `matmul_t4` — non modifie — 44 % plus lent. Une regression allait etre
   annoncee ; c'etait la machine, pas le code.

### Ce qui reste ouvert sur ce front

- **`~alloc` et `~free` restent les deux premiers postes** (26,9 % et 20,5 % apres
  correctif) : 26 908 allocations par fenetre de 7 pas, INCHANGE. Le chantier
  « reserve de tampons » n'est pas entame — seule la part liee aux uploads est
  tombee.
- **`~clear` apparait a 12,8 %** : c'est le cout du remede. Il reste tres
  inferieur a ce qu'il remplace, mais il n'est pas gratuit et il est now le 3e
  poste.
- Le noyau `matmul_t4` varie toujours d'un facteur ~1,8 entre executions, non
  explique (perimetre reduit : le banc `add` se reproduit a 15 % pres, donc
  l'instabilite est propre a ce noyau).
  → ⚠️ **RÉSOLU ET BORNÉ le 2026-08-16, section ci-dessous.** La variance n'est
  pas une propriété du débit : c'est la **queue de gigue de lancement**, et elle
  ne domine que sur les petits dispatches.

---

## 🔬 BANC D'ÉCHELLE `matmul_t4` — 2026-08-16

**`NkTensorGpuTest --banc-matmul`**, écrit ce jour sur le modèle du banc `add`.
Il appelle `RunMatMul`, c'est-à-dire **le chemin de production lui-même** (ce
qu'appelle `ops::Matmul`), et non une reconstruction : toutes les tailles
respectent `M*N >= 65536 && K >= 16`, donc passent bien par `matmul_t4` et non
par le naïf.

### Montage déclaré — sans lui le chiffre n'est pas rejouable

| | |
|---|---|
| machine | Intel Core i7-10870H, 31,8 Go RAM, Windows 11 |
| GPU | **NVIDIA GeForce RTX 3070 Laptop**, pilote 31.0.15.3161 (2023-04-08) |
| backend | **Vulkan** |
| arbre | `Nkentseu-ilyana`, `feat/ilyana-pdf`, base `c3874647` |
| configuration | **Release**, binaire contrôlé **FRAIS** (plus récent que sa source) |
| protocole | 15 répétitions par point, 3 de chauffe jetées, **2 passes témoin** |
| horloge | **murale hôte** (`NkChrono`) — inclut lancement et synchronisation |
| machine | **libre** : aucun autre travail GPU pendant la campagne (vérifié) |

⚠️ **Le « % de crête » est calculé sur 20 300 GFLOPS, chiffre HÉRITÉ de la
ROADMAP.** C'est la crête d'une RTX 3070 **de bureau** ; la carte ici est une
**Laptop**, dont la crête FP32 est plutôt ~16,6 TFLOPS. **Les pourcentages
ci-dessous sont donc SOUS-ESTIMÉS d'environ 20 %.** Je le dis au lieu de le
propager en silence — et la conclusion tient sous les deux chiffres, ce qui est
le seul point qui compte ici.

### 1. ✅ LA VARIANCE DE ~1,8× EST EXPLIQUÉE, ET ELLE N'EST PAS OÙ ON LA CROYAIT

**Elle dépend de la TAILLE, et elle s'effondre à la taille de production.**

| forme | max/min (passe 1) | max/min (passe 2) |
|---|---|---|
| 128×512×64 | **2,93** | **4,62** |
| 256×512×128 | 1,35 | 1,11 |
| 512×512×256 | 1,22 | 1,12 |
| 1536×640×640 (PROD) | 1,07 | 1,08 |
| 1536×2560×640 (PROD) | 1,05 | 1,02 |
| 1536×32769×640 (PROD) | **1,02** | **1,02** |

Et la **reproductibilité entre les deux passes**, sur le `min` :

```
1536x32769x640   60 426,8 -> 60 436,9 us    ecart 0,017 %
1536x2560x640     4 831,5 ->  4 820,4 us    ecart 0,23 %
1536x640x640      1 516,8 ->  1 442,9 us    ecart 4,9 %
128x512x64          177,1 ->    167,4 us    ecart 5,5 %
```

🎯 **Conclusion, et elle change la façon de mesurer ce noyau** : la variance de
~1,8× n'est **pas** une instabilité du débit. C'est la **queue de la gigue de
lancement**, qui pèse relativement d'autant plus que le dispatch est court. Sur
un dispatch de 60 ms, elle est invisible (1,02) ; sur un dispatch de 170 µs, elle
atteint 4,6.

→ **La statistique à lire sur ce noyau est le `min` (ou la médiane), jamais la
moyenne ni le max.** Le `min` se reproduit à **0,017 %** à la taille de
production. *Le mystère n'était pas dans le noyau, il était dans le choix de
l'estimateur.*

### 2. ⚠️ CE QUE LA MESURE RÉFUTE — « 2 à 4 % de la crête » est FAUX

La ROADMAP écrit, tableau des noyaux : *« `matmul_t4` — pavage 4×4 en registres :
intensité 1 FLOP/octet, donc **2 à 4 % de la crête** »*.

**Mesuré, reproductible à 0,02 % :**

| forme de production | GFLOP/s | % de crête (20,3 TFLOPS) |
|---|---|---|
| 1536×640×640 | 830 – 872 | 4,09 – 4,30 |
| 1536×2560×640 | 1 042 – 1 044 | **5,13 – 5,14** |
| 1536×32769×640 | 1 066 | **5,25** |

**Mon propre modèle est réfuté par ma propre mesure**, et c'est le résultat le
plus utile de ce banc. J'avais calculé un plafond de ~448 GFLOP/s (intensité
1 FLOP/octet × 448 Go/s). **Le noyau mesure 1 066 GFLOP/s — 2,4× AU-DESSUS de mon
plafond.**

*Une mesure au-dessus d'un plafond accuse le modèle, pas la machine.* Le calcul
« intensité = 1 FLOP/octet » compte **chaque lecture comme allant en DRAM**. En
réalité le **cache L2 sert la majorité des relectures** de colonnes de B entre
groupes de travail : le trafic DRAM effectif est très en dessous du compte naïf,
et le noyau n'est donc **pas** limité par la bande passante à 448 Go/s.

⚠️ **Conséquence directe sur le chantier n°5**, qui dit « pavage en mémoire
partagée (intensité 1 → 16) » pour ×1,11 à ×1,19 : **sa prémisse est fausse.**
L'intensité effective est déjà bien meilleure que 1 grâce au L2, donc **le gain
attendu d'un pavage en mémoire partagée est plus petit qu'annoncé**. Le chiffre
×1,11-×1,19 venait déjà du profil (mal attribué) ; il faut désormais le lire
comme une **borne supérieure très optimiste**.

### 3. ✅ CORROBORATION INDÉPENDANTE DU CHANTIER N°2 (réserve de tampons)

L'écart **série B − série A** isole le coût d'une allocation, sur un noyau qui
n'a rien à voir avec `add` :

```
128x512x64     +431,0 / +426,9 us
256x512x128    +439,8 / +437,8 us
512x512x256    +475,1 / +492,4 us      (passe 1 / passe 2)
```

**+427 à +492 µs par allocation** — cela tombe exactement sur les **+430 à
+480 µs** mesurés au banc `add`. *Deux noyaux sans rapport, la même valeur : le
coût d'allocation n'est pas une propriété de `add`, c'en est une du pilote.* Le
chantier n°2 en sort renforcé, cette fois par une mesure qui ne lui était pas
destinée.

Sur les grosses formes l'écart grandit avec la taille du tampon (+11,1 à
+12,6 ms pour le tampon de 201 Mo des logits), ce qui est cohérent.

### 4. Ni le NOMBRE de tampons ni la VRAM ne ralentissent ce dispatch

| série | ce qu'elle charge | effet sur le `min` de production |
|---|---|---|
| C1 | 9 316 tampons vivants (~38 Mo) | **aucun** (60 366 / 60 393 µs contre 60 427 / 60 437) |
| C2 | 8 tampons, ~4 Go de VRAM | **aucun** (60 028 / 60 200 µs) |

⚠️ **Une anomalie reproductible, non expliquée, et je ne la sur-interprète pas** :
sur la seule forme `1536×2560×640`, la série C1 coûte **+28 à +29 %**
(4 831 → 6 181 et 4 820 → 6 230 µs) alors que C2 ne montre rien. Reproduit sur
les deux passes, donc ce n'est pas du bruit. *Beaucoup de petits tampons pénalise
cette forme précise et pas les autres — cause non cherchée, signalée.*

⚠️ **Et un piège évité par le témoin** : en passe 1, la série C2 semblait ralentir
les petites tailles (207 contre 177 µs, 441 contre 338). **La passe 2 ne le
reproduit pas** (199 contre 167, 346 contre 329). Sans la seconde passe,
j'annonçais « la pression VRAM ralentit les petits dispatches » — c'était du
bruit.

### 5. ⚠️ CE QUE CE BANC NE COUVRE PAS

- **Il ne dit rien du débit d'entraînement.** Il mesure un noyau isolé ; le pas
  de production enchaîne 28 119 opérations, et la ROADMAP montre déjà que
  l'essentiel du temps attribué à un noyau **n'est pas du temps GPU**.
- **Il ne mesure aucun remède**, seulement l'état des lieux.
- **Horloge murale hôte**, pas d'horodatage GPU : le temps inclut le lancement et
  la synchronisation. Séparer les deux demanderait des requêtes de temps GPU.
- **Un seul backend (Vulkan), une seule carte, une seule configuration
  (Release).** Rien ici ne se transpose à un autre backend.

### 6. Ce que j'en conclus pour l'ordre de bataille

**`matmul_t4` n'est pas le bon chantier, et cette mesure le confirme deux fois :**
il tourne à 5,25 % de la crête au lieu des 2-4 % annoncés — donc **plus près de
son plafond réel qu'on ne croyait** — et le gain du pavage en mémoire partagée
repose sur une prémisse d'intensité que le L2 dément.

Ce que la campagne renforce, en revanche, c'est **le chantier n°2** : +427 à
+492 µs par allocation, corroborés sur un second noyau. *Je n'ai rien optimisé ;
j'ai supprimé une piste et consolidé une autre.*

---

## 🏗️ CHANTIER N°2 — RÉSERVE DE TAMPONS : LIVRÉE ET MESURÉE (2026-08-16)

**`NkTensorGpu` recycle désormais les tampons par TAILLE EXACTE** au lieu de
détruire/recréer. Interrupteur `ReserveActive()` : **LEGACY et NEUF tournent
depuis le même binaire**, comme pour le ×1,57 du chantier n°1.

### ⚠️ D'ABORD LA JUSTESSE — une réserve est un cache, et un cache répond toujours

C'est la famille de défauts que ce dépôt paie depuis une semaine. **Le témoin a
été câblé AVANT la première mesure de vitesse**, et le banc **refuse de mesurer
le gain** si le témoin échoue.

| ce qu'on vérifie | résultat |
|---|---|
| la réserve **sert-elle vraiment** ? | **OUI** — compteur `servis` +1 au second `CreateBuffer` |
| que **contient** un tampon recyclé ? | **les données du précédent usage** (`v[0] = 123,5`) |
| le zérotage explicite nettoie-t-il ? | **OUI** — `Clear()` rend des zéros |
| l'**instrument** est-il juste ? | **`servis + neufs == nombre d'appels`** — vérifié |

🎯 **Le risque est démontré, pas supposé** : un tampon recyclé **est** rémanent.
Ce qui rend le recyclage sûr n'est pas la chance, c'est que **`NkGpuZeros` remet
explicitement à zéro APRÈS `CreateBuffer`** — c'est-à-dire le chantier n°1.
*Sans lui, cette réserve aurait produit des gradients faux en silence.*

**Le témoin de service sur toute la série :**
```
LEGACY : servis=0   neufs=80    (la reserve ne fait rien quand elle est eteinte)
NEUF   : servis=71  neufs=9     71+9 = 80 = meme total -> instrument coherent
```
*Sans ces deux compteurs, « la réserve marche » aurait été indiscernable de « la
réserve ne sert jamais et tout retombe en allocation ».*

### Le gain — série B (forme production), ordre ALTERNÉ

Montage identique au banc `matmul` : RTX 3070 **Laptop**, Vulkan, **Release**,
binaire **FRAIS**, 15 reps, 3 de chauffe, machine libre, base `d14f7c24`.
**Chaque mode occupe les deux positions** — la machine s'accélère au fil des
courses, alterner les modes seuls ne suffit pas.

| forme | LEGACY (min µs) | NEUF (min µs) | gain |
|---|---|---|---|
| 128×512×64 | 558,8 / 575,0 | **150,7 / 154,9** | **×3,6 à ×3,8** |
| 256×512×128 | 630,2 / 646,6 | **208,8 / 209,6** | **×3,0** |
| 512×512×256 | 777,4 / 783,9 | **319,7 / 315,3** | **×2,4** |
| 1536×640×640 (PROD) | 2 047,3 / 2 103,6 | **1 428,6 / 1 426,2** | **×1,43** |

**Les plages ne se chevauchent pas, et de loin** : le NEUF le plus lent (154,9)
reste 3,6× sous le LEGACY le plus rapide (558,8). *Aucun appariement ne peut
inverser le sens du résultat.*

### 🎯 TROISIÈME CORROBORATION — cette fois par la SUPPRESSION du coût

L'écart LEGACY − NEUF **est** le coût d'allocation supprimé :

```
128x512x64     408,1 / 420,1 us
256x512x128    421,4 / 437,0 us
512x512x256    457,7 / 468,6 us
```

**408 à 469 µs** — cela tombe sur les **+427 à +492 µs** mesurés indépendamment
au banc `add` **et** au banc `matmul`. *Deux mesures l'avaient chiffré par
différentiel ; celle-ci le chiffre en le faisant disparaître.* Prédiction et
remède se rejoignent.

**Et une convergence qui vaut contrôle** : le NEUF (154,9 / 209,6 / 315,3 /
1 426,2) rejoint la **série A** du banc `matmul`, c'est-à-dire le **dispatch
seul** (167,4 / 219,8 / 329,0 / 1 442,9). **La réserve ramène la forme de
production au coût du dispatch nu** — le coût d'allocation n'est pas réduit, il
est **supprimé**.

### ⚠️ CE QUE CETTE MESURE NE COUVRE PAS — à lire avant d'extrapoler

- **Elle ne dit RIEN du débit d'entraînement réel.** Ce banc enchaîne 4 formes ;
  un pas de production fait **9 316 allocations de tailles variées**. Le taux de
  service y dépend de la **diversité des tailles**, que je n'ai pas mesurée.
  **Les ×1,43 à ×3,8 ne se transposent PAS au pas.** L'estimation ROADMAP de
  ×1,43-×1,52 sur le pas entier reste **non vérifiée**.
- **La VRAM immobilisée n'est pas mesurée en production.** Ici : 9 tampons,
  **12 Mo retenus, 0 éviction**, budget 512 Mo. En production, avec des tailles
  variées et un pic déjà à **6 659 Mo sur 8 Go**, le budget mordra et des
  évictions apparaîtront — **non mesuré**.
- **Horloge murale hôte**, pas d'horodatage GPU.
- **Un seul backend (Vulkan), une seule carte, une seule configuration.**
- ⚠️ **La réserve est ÉTEINTE par défaut.** Rien n'est activé en douce ;
  l'allumer en production est une décision qui demande la mesure ci-dessus.

### Ce qui reste ouvert sur ce chantier

- **Mesurer le taux de service sur un vrai pas** — seul chiffre qui dira ce que
  la réserve vaut réellement. Il demande de relancer un pas d'entraînement, donc
  **il touche aux courses : c'est Rodolf qui décide.**
- **L'anomalie des 9 316 tampons** (+28 à +29 % sur la seule forme
  `1536×2560×640`, reproductible sur deux passes) est peut-être la même histoire
  vue d'un autre angle. Gardée sous la main, **non conclue**.
