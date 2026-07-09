# NKAI — Entraînement du GPT (NKGptTrain) — page vivante

> Page **mise à jour en temps réel** : commandes d'entraînement/reprise, état courant du
> Palier, et prochaines briques. Guide opérationnel détaillé :
> [`Applications/NKGptTrain/GUIDE_LLM_AUTONOME.md`](../../Applications/NKGptTrain/GUIDE_LLM_AUTONOME.md).
> Roadmap technique : [`Kernel/AI/ROADMAP.md`](../../Kernel/AI/ROADMAP.md).

## Build

```
jenga build --target NKGptTrain --config Release
# exe : Build\Bin\Release-Windows\NKGptTrain\NKGptTrain.exe   (lancer en PowerShell)
```

⚠️ Lancer via **PowerShell/cmd**, pas Git Bash (exit 127 = DLL introuvable). Pour capturer
l'UTF-8 (accents, Ghomala) dans un log : `cmd /c '"...NKGptTrain.exe" > log 2>&1'` **ou**
`Start-Process -RedirectStandardOutput` (le `*>` de PowerShell casse l'UTF-8 → `�`).

## Variables d'environnement (pilotage sans recompiler)

| Var | Rôle | Défaut |
|---|---|---|
| `NK_GPT_DIR` | dossier de corpus (équilibré par langue, tag = préfixe avant `_`) | `Resources/Datasets` |
| `NK_GPT_FILE` | corpus fichier unique (au lieu de `DIR`) | — |
| `NK_GPT_CHARS` | budget total de caractères (divisé à parts égales par tag) | 1,2 M |
| `NK_GPT_MERGES` | fusions BPE (monter à 2000-4000 si code+prose+maths+bbj mélangés) | 600 |
| `NK_GPT_D`/`NK_GPT_L`/`NK_GPT_H` | dimension modèle / couches / têtes | 256 / 4 / 8 |
| `NK_GPT_T`/`NK_GPT_B` | contexte / batch | 128 / 16 |
| `NK_GPT_ACCUM` | micro-lots accumulés → batch effectif = B×ACCUM (tenir un gros modèle sur 8 Go) | 1 |
| `NK_GPT_LR`/`NK_GPT_WARMUP` | pic de learning-rate / pas de warmup (puis cosine, plancher 10%) | 3e-4 / 5% |
| `NK_GPT_STEPS` | nombre de pas | 300 |
| `NK_GPT_VALFRAC`/`NK_GPT_VALEVERY` | fraction de queue held-out par langue (0..0.9) / éval perte val tous les N pas | 0 / 0 |
| `NK_GPT_SAVE`/`NK_GPT_SAVEEVERY` | fichier checkpoint / sauver tous les N pas (0 = fin seule) | — / 0 |
| `NK_GPT_LOAD`/`NK_GPT_RESUME` | checkpoint à charger / `1` = **reprendre l'entraînement** (checkpoint v4 → reprise PARFAITE : état Adam + pas restaurés, pas de warmup ni pic de perte) | — / 0 |
| `NK_GPT_LANG`/`NK_GPT_PROMPT`/`NK_GPT_GENLEN` | langue de génération / amorce / longueur | auto / — / 400 |

## Reprendre le Palier après un arrêt (reprise d'entraînement)

Recharge poids + BPE + dims + langues du checkpoint, ré-encode le corpus avec ce BPE, et
**continue** l'entraînement (⚠️ le corpus doit exposer les mêmes tags dans le même ordre) :

```
NK_GPT_LOAD=D:/Projets/Camrail/AI/checkpoint_palier1.nkmd  NK_GPT_RESUME=1
NK_GPT_DIR=D:/Projets/Camrail/AI/Palier1Data  NK_GPT_STEPS=2000  NKGptTrain.exe
```

> Limite actuelle : l'**état Adam** n'est pas sauvegardé (optimiseur neuf + warmup LR
> redémarre à la reprise) — les **poids**, eux, sont préservés. Voir « prochaines briques ».

## Générer du texte depuis un checkpoint (sans réentraîner)

```
NK_GPT_LOAD=<checkpoint>  NK_GPT_LANG=bbj  NK_GPT_PROMPT="..."  NK_GPT_GENLEN=400  NKGptTrain.exe
```

## État courant (2026-07-09)

- **Palier 1 en cours** : ~13 M params (D=384/L=5/H=6/T=128/B=6/ACCUM=3), 4000 pas (~6 h),
  7 tags dont **Ghomala** (corpus `Palier1Data` : bbj lafand+NT, fr/en +Gutenberg), LR schedule
  + checkpoint/250 pas. **Exe isolé** (`D:/Projets/Camrail/AI/palier_run/`) → les recompilations
  ne le touchent pas. Réglages par palier : `D:/Projets/Camrail/AI/STRATEGIE_ENTRAINEMENT.md`.
- Matériel : RTX 3070 Laptop 8 Go (FP32). Sweet spot ~10-30 M params.
- ⚠️ Honnêteté d'échelle : à petite taille/peu de pas, la sortie apprend le **format** et les
  **distributions de caractères** par langue, **pas le sens** — la cohérence émerge avec
  taille + pas + données.

## Prochaines briques (roadmap)

- ⬜ **Sauver l'état Adam + le pas courant** dans le checkpoint → reprise *parfaite* (schedule
  et momentum continus) — **en cours**.
- ⬜ **Cible par indices** (au lieu du one-hot dense `[B*T, V]`) → économie mémoire (~140 Mo/pas).
- ⬜ **Mixed precision FP16** (Palier 2) → halve mémoire + ~2× vitesse (tensor cores 3070).
- ⬜ **Corpus format dialogue** (multi-tours User:/Assistant:) → vraie conversation Q→R.
- ⬜ **Push zéro-STL + PR** (quand coordonné avec l'autre agent) ; **vidéos-preuves** (à tourner).
- ⬜ Plus de données **bbj** (Ghomala peu doté — collecte communautaire, cf. `01_GHOMALA_STRATEGY`).

## Livré (moteur d'entraînement, 2026-07-09)

Accumulation de gradient · masquage de loss (instruction-tuning) · LR schedule (warmup+cosine) ·
checkpoint périodique · reprise d'entraînement. Détail + commits : `Kernel/AI/ROADMAP.md`
(sections « MONTÉE EN GAMME » et « MOTEUR D'ENTRAÎNEMENT »).
