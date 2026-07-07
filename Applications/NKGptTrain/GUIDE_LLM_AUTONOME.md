# Guide — entraîner et interroger le LLM NKAI **tout seul** (sans assistance)

Ce guide explique, de bout en bout, comment **toi** (ou une autre machine) peut :
1. **collecter et nettoyer** des données texte,
2. les **donner au LLM** pour l'entraînement (petite ou **grosse** base),
3. **sauvegarder** le modèle,
4. lui **poser des questions / générer** du texte,
5. **monter en échelle** (plus de données, plus gros modèle, ailleurs).

Le LLM est `NKGptTrain` (un GPT *from-scratch*, GPU-résident). Tout se pilote par
**variables d'environnement** — aucune recompilation pour changer de données ou de taille.

> Rappel build : `jenga build --target NKGptTrain --config Release` →
> exe `Build\Bin\Release-Windows\NKGptTrain\NKGptTrain.exe`. Lancer en **PowerShell**.

---

## 1. Le pipeline en une image

```
 (1) COLLECTE        (2) NETTOYAGE          (3) RANGEMENT        (4) ENTRAÎNEMENT       (5) UTILISATION
 .txt / .pdf   ->   texte UTF-8 propre  ->  Datasets/<lang>_*.txt  ->  NKGptTrain (SAVE)  ->  NKGptTrain (LOAD)
 (web, livres,      (retrait bruit,          (convention de           => modele.nkgp        => génère / répond
  Bible, dumps)      entêtes, doublons)       langue)                                          (NK_GPT_LANG/PROMPT)
```

Le modèle apprend **la langue du corpus**. Corpus propre + abondant = meilleur modèle.

---

## 2. Étape 1 — Collecter les données

Sources typiques de **texte brut** :
- **Livres domaine public** : [Project Gutenberg](https://www.gutenberg.org) (bouton *Plain Text UTF-8*).
- **Dumps Wikipédia** : https://dumps.wikimedia.org (extraire le texte avec `wikiextractor`).
- **Textes religieux / traductions** : souvent le plus gros corpus pour une langue peu dotée.
- **Tes propres textes** : articles, transcriptions, notes… (tout `.txt` UTF-8).

> ⚖️ **Droits** : n'entraîne que sur des textes que tu as le droit d'utiliser. Le domaine
> public (Gutenberg) est sûr. Les œuvres sous copyright : usage **local/recherche** seulement,
> **ne pas** les committer/redistribuer (les mettre dans `.gitignore`).

---

## 3. Étape 2 — Nettoyer les données

Objectif : du **texte UTF-8** lisible, sans bruit répété.

### a) PDF → texte
```powershell
pdftotext -enc UTF-8 "mon_livre.pdf" "mon_livre.txt"
```
⚠️ **Vérifier les polices d'abord** (surtout langues à caractères spéciaux) :
```powershell
pdffonts "mon_livre.pdf"
```
Si la colonne **`uni`** est `no` pour les polices du texte → le PDF n'a **pas** de table
Unicode → `pdftotext` sortira du **charabia** (mojibake). Dans ce cas, cherche une **autre
source** (version web, EPUB, autre PDF) — n'entraîne **jamais** sur du mojibake.

### b) Retirer le bruit répété (pieds de page, numéros de page)
```bash
grep -v "© Mon Éditeur"  brut.txt \
 | grep -vE "^[0-9]{1,3}$" \
 | sed 's/\x0c//g' \
 | cat -s  > propre.txt          # cat -s = compresse les lignes vides
```

### c) Entêtes Project Gutenberg
NKGptTrain **saute automatiquement** l'entête/pied Gutenberg (`*** START OF` … `*** END OF`).
Pas besoin de les retirer à la main.

### d) Vérifier l'encodage
Le fichier doit être **UTF-8**. Convertir si besoin :
```bash
iconv -f ISO-8859-1 -t UTF-8 fichier.txt > fichier_utf8.txt
```

### e) (Optionnel) Dédupliquer les lignes identiques
```bash
awk '!seen[$0]++' propre.txt > dedup.txt
```

---

## 4. Étape 3 — Ranger par langue (convention)

NKGptTrain lit **tous les `.txt`** d'un dossier et **équilibre par langue**. La langue = le
**préfixe avant le premier `_`** du nom de fichier :

```
Resources/Datasets/
  fr_monlivre.txt      <- français
  en_book.txt          <- anglais
  bbj_texte.txt        <- Ghɔmáláʼ
  de_buch.txt          <- allemand (nouvelle langue : rien d'autre à faire)
```

Ajouter une langue = déposer des fichiers `xx_*.txt`. L'équilibrage 1/langue s'ajuste seul.
Un dossier custom : `NK_GPT_DIR="D:\chemin\vers\mon_corpus"`.

---

## 5. Étape 4 — Entraîner

### Petit essai (rapide, un seul fichier)
```powershell
cd D:\Projets\2026\Nkentseu\Nkentseu
Get-ChildItem Env:NK_GPT_* | ForEach-Object { Remove-Item "Env:\$($_.Name)" }  # reset
$env:NK_GPT_FILE="D:\...\Datasets\fr_monlivre.txt"
$env:NK_GPT_STEPS="500"
$env:NK_GPT_SAVE="D:\...\Resources\Models\essai.nkgp"
.\Build\Bin\Release-Windows\NKGptTrain\NKGptTrain.exe
```

### Vraie base (tout un dossier, multilingue, checkpoint)
```powershell
Get-ChildItem Env:NK_GPT_* | ForEach-Object { Remove-Item "Env:\$($_.Name)" }
$env:NK_GPT_DIR="D:\...\mon_corpus"     # défaut = Resources\Datasets
$env:NK_GPT_CHARS="4000000"             # + de caractères = + de matière (RAM)
$env:NK_GPT_MERGES="2000"               # vocabulaire BPE (mots-morceaux)
$env:NK_GPT_STEPS="4000"                # + de pas = mieux appris (plus long)
$env:NK_GPT_SAVE="D:\...\Models\mon_modele.nkgp"
.\Build\Bin\Release-Windows\NKGptTrain\NKGptTrain.exe
```

### Les variables (toutes optionnelles, avec défauts)
| Variable | Rôle | Pour une grosse base |
|----------|------|----------------------|
| `NK_GPT_DIR` / `NK_GPT_FILE` | dossier / fichier corpus | dossier |
| `NK_GPT_CHARS` | caractères lus (cap total) | **grand** (2–10 M+) |
| `NK_GPT_MERGES` | fusions BPE (taille vocab) | 2000–8000 |
| `NK_GPT_STEPS` | pas d'entraînement | **grand** (3000–20000) |
| `NK_GPT_T` | contexte (tokens) | 128–512 |
| `NK_GPT_D` | dimension modèle | 256–768 |
| `NK_GPT_H` | têtes d'attention (D divisible par H) | 8–12 |
| `NK_GPT_L` | couches transformer | 4–12 |
| `NK_GPT_B` | taille de lot (batch) | 16–64 (selon VRAM) |
| `NK_GPT_SAVE` | chemin du checkpoint à écrire | toujours |

> **Règle d'or de l'échelle** : pour un **meilleur texte**, augmenter dans cet ordre —
> (1) **plus de données** propres, (2) **plus de pas**, (3) **modèle plus gros** (`D`/`L`),
> (4) **vocab BPE plus grand** (`MERGES`). Chaque cran coûte plus de temps/mémoire.

### Lancer un **long** entraînement sans rester devant (détaché)
```powershell
$env:NK_GPT_STEPS="8000"; $env:NK_GPT_SAVE="D:\...\Models\gros.nkgp"
Start-Process -FilePath ".\Build\Bin\Release-Windows\NKGptTrain\NKGptTrain.exe" `
  -RedirectStandardOutput "D:\...\logs\run.log" -RedirectStandardError "D:\...\logs\run.err" `
  -WindowStyle Hidden
```
Le process **survit** à la fermeture du terminal. Le **checkpoint apparaît à la fin** →
surveiller son apparition suffit à savoir que c'est terminé.

Pendant l'entraînement, la **perte** doit baisser (elle démarre à ≈ `ln(taille_vocab)`).
Un échantillon par langue s'affiche tous les 100 pas.

---

## 6. Étape 5 — Interroger le modèle (poser des questions / générer)

On **recharge** le checkpoint et on génère **sans réentraîner** (quelques secondes) :
```powershell
Get-ChildItem Env:NK_GPT_* | ForEach-Object { Remove-Item "Env:\$($_.Name)" }
$env:NK_GPT_LOAD="D:\...\Models\mon_modele.nkgp"
$env:NK_GPT_LANG="fr"            # langue voulue : fr | en | bbj | …
$env:NK_GPT_PROMPT="Le roi "     # l'amorce = ta "question"/début de phrase
$env:NK_GPT_GENLEN="120"         # nombre de tokens générés
.\Build\Bin\Release-Windows\NKGptTrain\NKGptTrain.exe
```
- **`NK_GPT_PROMPT`** = ce que tu donnes au modèle pour qu'il **continue**. C'est ainsi qu'on
  « pose une question » à un modèle génératif : on écrit le **début**, il produit la **suite**.
- **`NK_GPT_LANG`** force la langue (grâce au *tag de langue* appris à l'entraînement).
- Le modèle **ne connaît que ce qu'il a lu** : pour qu'il « réponde » sur un sujet, ce sujet
  doit être **présent dans le corpus** d'entraînement.

> Pour un vrai comportement « question → réponse » (chatbot), il faut entraîner sur des
> données **au format dialogue** (`Question: … \n Réponse: …`) puis amorcer avec `Question: …`.
> C'est une évolution future du corpus, pas une option magique.

---

## 7. Entraîner « ailleurs » / sur une autre machine

Le projet est **portable** (C++ + Vulkan). Pour entraîner sur un autre PC/serveur :
1. **Cloner** le dépôt + installer la toolchain (voir `CLAUDE.md` racine : `clang-mingw`, Jenga).
2. Un **GPU Vulkan** accélère beaucoup ; sinon ça tourne sur CPU (plus lent).
3. Copier ton **corpus** (`Datasets/`) et lancer comme au §5.
4. Le **checkpoint** `.nkgp` produit est **autonome** (contient dims + BPE + langues + poids) :
   tu peux le rapatrier et le recharger n'importe où (§6), **sans** le corpus d'origine.

Rien n'exige une présence humaine pendant le run : c'est **un exe + des variables + un log**.
Un script `.ps1` qui enchaîne « nettoyer → entraîner → sauvegarder » suffit à tout automatiser.

---

## 8. Combien de données / de pas ? (repères honnêtes)

| Objectif | Données | Pas | Modèle | Rendu attendu |
|----------|---------|-----|--------|---------------|
| Prouver que ça apprend | ~100 k car. | 300–500 | petit (d=128,L=2) | pseudo-mots |
| Texte « à la bonne langue » | ~1 M car. | 800–2000 | d=256,L=4 | vrais mots, peu de sens |
| Phrases plus tenues | 5–20 M car. | 5000–20000 | d=384–768,L=6–12 | phrases locales correctes |
| **Vraiment bon** | **Go de texte** | **beaucoup** | **gros** | nécessite compute/données à l'échelle |

Le « vraiment bon » demande une **échelle** (données massives + gros modèle + long calcul)
qu'on atteindra avec plus de moyens. La pile, elle, est **déjà prête** pour monter en taille.

---

## 9. Dépannage rapide

| Symptôme | Cause / solution |
|----------|------------------|
| Charabia à caractères bizarres | Corpus pas UTF-8, ou PDF sans table Unicode (`pdffonts` → `uni no`). |
| `Corpus introuvable/trop court` | Mauvais `NK_GPT_DIR`/`NK_GPT_FILE` ou dossier vide. |
| `Checkpoint illisible … BPE v3` | Fichier d'une ancienne version : réentraîner pour régénérer. |
| Génère surtout des espaces | Amorce trop « neutre » : mets une vraie amorce (`NK_GPT_PROMPT="Le "`). |
| Texte peu cohérent | Normal à petite échelle : plus de données + plus de pas + modèle plus gros. |
| Lent | Compiler en **Release** ; vérifier que le GPU Vulkan est bien pris (log « GPU compute : OUI »). |
| exit 127 | Lancé depuis bash — utiliser **PowerShell**. |

---

## 10. Récapitulatif (mémo)

```powershell
# 1) NETTOYER (exemple PDF)
pdffonts livre.pdf ; pdftotext -enc UTF-8 livre.pdf fr_livre.txt
# 2) RANGER  -> Resources\Datasets\fr_livre.txt  (préfixe langue)
# 3) ENTRAÎNER + SAUVEGARDER
$env:NK_GPT_STEPS="4000"; $env:NK_GPT_SAVE="Models\m.nkgp"; .\...\NKGptTrain.exe
# 4) INTERROGER
$env:NK_GPT_LOAD="Models\m.nkgp"; $env:NK_GPT_LANG="fr"; $env:NK_GPT_PROMPT="Le "; .\...\NKGptTrain.exe
```

Tout est reproductible, scriptable, et ne dépend que de l'exe + tes données. Bonne exploration.
