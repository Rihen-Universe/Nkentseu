# NKGptTrain — entraîner et tester le petit GPT (from-scratch, GPU-résident)

`NKGptTrain` est une application de démonstration de **NKAI** : un petit GPT
**char-level** (caractère par caractère) écrit **de zéro** (sans PyTorch/STL de
calcul), entraîné **100 % sur GPU** via NkSL → SPIR-V → NKRHI compute, qui
**génère du texte**. Il assemble toute la pile : `NkGPT` (NKNN) + `AdamW`
(NKOptim) + softmax-cross-entropy (NKAutograd) + tenseurs GPU (NKTensor).

- **Tokenizer** : char-level **byte-level** (le vocabulaire = les octets présents
  dans le corpus). Gère donc **nativement l'UTF-8** — français accentué, anglais,
  et **Ghɔmáláʼ** (ɔ ə ŋ ʉ ɛ ʼ + tons) sans configuration.
- **Résidence GPU** : une fois les poids sur GPU, l'entraînement ne fait plus
  d'aller-retour CPU (kernels broadcast pour biais/LayerNorm).

---

## 1. Compiler

```
cd D:\Projets\2026\Nkentseu\Nkentseu
jenga build --target NKGptTrain --config Debug
```

Exécutable produit : `Build\Bin\Debug-Windows\NKGptTrain\NKGptTrain.exe`
(remplacer `Debug` par `Release` pour un entraînement plus rapide).

> ⚠️ Lancer l'exe depuis **PowerShell** (le shell bash renvoie exit 127 sur ce
> projet). Backend GPU par défaut : **Vulkan** (mode headless, sans fenêtre).

---

## 2. Le corpus

Par défaut, l'app lit **tous les `.txt` du dossier** `Resources/Datasets/` et
équilibre **par langue** : chaque langue reçoit **une part égale** du corpus
(≈ `totalCap / nombre_de_langues`), répartie entre ses fichiers. Ainsi une langue
avec beaucoup de livres (8 EN) n'écrase pas une langue avec un seul (1 bbj).

**Convention de nommage** — chaque fichier est préfixé par sa langue :
`<lang>_<nom>.txt`. La langue est le préfixe **avant le premier `_`**.

| Préfixe | Contenu | Fichiers |
|---------|---------|----------|
| `fr_`  | Français (Project Gutenberg, domaine public) | 6 |
| `en_`  | Anglais (Project Gutenberg, domaine public)  | 8 |
| `bbj_` | **Ghɔmáláʼ** — Nouveau Testament (`bbj_ghomala_nt.txt`) | 1 |

Ajouter une langue = déposer des `.txt` préfixés (ex. `de_...` pour l'allemand) ;
l'équilibrage s'ajuste automatiquement. Un fichier **sans préfixe** est rangé dans
une langue « ?? ».

> ⚖️ **Licence** : le texte bbj provient du *Ghomala New Testament, © 2002 Bible
> Society of Cameroon*. Il est utilisé **en local pour l'entraînement/la
> recherche uniquement** et est **exclu du dépôt** (`.gitignore`) — ne pas le
> redistribuer. Les livres Gutenberg sont, eux, du domaine public.

---

## 3. Entraîner

Le comportement se règle **entièrement par variables d'environnement** — aucune
recompilation nécessaire.

### Variables

| Variable | Rôle | Défaut |
|----------|------|--------|
| `NK_GPT_STEPS`  | Nombre de pas d'entraînement | `300` |
| `NK_GPT_DIR`    | Dossier corpus (tous les `.txt`) | `Resources/Datasets` |
| `NK_GPT_FILE`   | **Un seul** fichier corpus (prioritaire sur DIR) | — |
| `NK_GPT_CHARS`  | Cap total de caractères lus | `1200000` (dossier) / `150000` (fichier) |
| `NK_GPT_T`      | Longueur de contexte (tokens) | `128` |
| `NK_GPT_D`      | Dimension du modèle | `256` |
| `NK_GPT_H`      | Nombre de têtes d'attention | `8` |
| `NK_GPT_L`      | Nombre de couches transformer | `4` |
| `NK_GPT_B`      | Taille de lot (batch) | `16` |
| `NK_GPT_SAVE`   | Chemin où **sauvegarder** le modèle après entraînement | — |
| `NK_GPT_PROMPT` | Amorce de génération | `"Le "` |
| `NK_GPT_GENLEN` | Nombre de caractères générés | `400` |
| `NK_GPT_LANG`   | **Langue générée** via tag de langue (`fr`/`en`/`bbj`) | auto (aucun tag) |

> `NK_GPT_D` doit être divisible par `NK_GPT_H` (dimension par tête = D / H).

**Tag de langue** — à l'entraînement, chaque séquence commence par un token-tag
indiquant sa langue (préfixe du fichier). À la génération, `NK_GPT_LANG=fr|en|bbj`
préfixe ce tag → **la langue produite est choisie de façon déterministe**, quelle
que soit l'amorce. Sans `NK_GPT_LANG`, la langue suit l'amorce (mode auto).

### Exemples

**A. Entraînement multilingue par défaut (FR + EN + bbj), avec sauvegarde :**
```powershell
cd D:\Projets\2026\Nkentseu\Nkentseu
$env:NK_GPT_STEPS="700"
$env:NK_GPT_SAVE="D:\Projets\2026\Nkentseu\Nkentseu\Resources\Models\gpt_trilingue.nkgp"
.\Build\Bin\Debug-Windows\NKGptTrain\NKGptTrain.exe
```

**B. Entraînement rapide sur un seul livre français (plus cohérent) :**
```powershell
$env:NK_GPT_FILE="D:\Projets\2026\Nkentseu\Nkentseu\Resources\Datasets\fr_pg17989.txt"
$env:NK_GPT_STEPS="500"
.\Build\Bin\Debug-Windows\NKGptTrain\NKGptTrain.exe
```

**C. Modèle plus gros (plus lent, meilleur texte) :**
```powershell
$env:NK_GPT_D="384"; $env:NK_GPT_L="6"; $env:NK_GPT_STEPS="1000"
.\Build\Bin\Debug-Windows\NKGptTrain\NKGptTrain.exe
```

Pendant l'entraînement, la **perte** doit baisser (≈ `ln(V)` au départ, ex.
~5,0 → ~2,0) ; un échantillon de texte s'affiche tous les 100 pas.

> 💡 **Réinitialiser les variables** entre deux essais (sinon elles persistent
> dans la session PowerShell) :
> `Get-ChildItem Env:NK_GPT_* | ForEach-Object { Remove-Item "Env:\$($_.Name)" }`

---

## 4. Tester / générer (sans réentraîner)

Une fois un modèle **sauvegardé** (`NK_GPT_SAVE`), on le **recharge** et on génère
en quelques secondes — aucun entraînement.

```powershell
$env:NK_GPT_LOAD="D:\Projets\2026\Nkentseu\Nkentseu\Resources\Models\gpt_trilingue.nkgp"
$env:NK_GPT_LANG="bbj"            # force la langue : fr | en | bbj
$env:NK_GPT_PROMPT=" "           # amorce neutre : le tag pilote la langue
$env:NK_GPT_GENLEN="300"
.\Build\Bin\Debug-Windows\NKGptTrain\NKGptTrain.exe
```

En mode `NK_GPT_LOAD`, l'app lit **dimensions + vocabulaire + poids** depuis le
checkpoint, reconstruit le modèle à l'identique, puis génère et s'arrête.
Change `NK_GPT_PROMPT` pour tester chaque langue :
- FR : `"Le "`, `"Elle "`
- EN : `"The "`, `"He said"`
- bbj : `"Yeso "`, `"A lə "`

### Format du checkpoint `NKGP`

Fichier binaire auto-suffisant :
```
['N','K','G','P'] | version u32 | V,d,H,L,T (5×i32) | vlen i32 | itos[vlen] |
count u32 | { rank u32, dims[rank] i64, data[numel] f32 } × count tenseurs
```
Les poids sont ramenés sur CPU à la sauvegarde et re-transférés sur GPU au
chargement. Un checkpoint est **portable** : il contient tout le nécessaire pour
régénérer sans le corpus d'origine.

---

## 5. Dépannage

| Symptôme | Cause / solution |
|----------|------------------|
| `Corpus introuvable/trop court` | Mauvais `NK_GPT_DIR`/`NK_GPT_FILE`, ou dossier vide. |
| `Checkpoint illisible` | Fichier `NK_GPT_LOAD` absent/corrompu ou d'une autre version. |
| `Poids du checkpoint incompatibles` | Le checkpoint a d'autres dimensions que celles reconstruites (ne pas forcer `NK_GPT_D/H/L/T` en mode LOAD — elles viennent du fichier). |
| `[FAIL] le GPT a appris (perte …)` | La perte moyenne finale est > 3,0 : **pas un vrai échec**, juste trop peu de pas. Augmenter `NK_GPT_STEPS`. |
| exit 127 | Lancé depuis bash — utiliser **PowerShell**. |

---

## 6. Ce que ça démontre (honnêteté)

NKGptTrain est **from-scratch, petite échelle, pédagogique**. Il prouve que la
pile NKAI (tenseurs GPU, autograd, transformer, AdamW, génération) fonctionne
bout-en-bout et apprend un vrai langage. Il **ne se compare pas** à PyTorch ou aux
LLM de frontière — c'est un jalon d'apprentissage et de fondation, pas un produit.
