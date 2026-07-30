# NKData — Roadmap

> Ce qui nourrit l'apprentissage (Phase 3). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — jeu de données + lots (Phase 3) ✅
- ✅ **`data::NkDataset`** (`src/NKData/NkData.{h,cpp}`) : features X [N,D] (stockage
  partagé) + étiquettes [N] + nb de classes ; `Size/FeatureDim/NumClasses`.
- ✅ **`data::NkDataLoader`** : mélange **Fisher-Yates** (LCG déterministe, `Shuffle()`
  par époque) + regroupement en **lots** → `NkBatch{ inputs[B,D], targets[B,C] one-hot,
  labels[B] }`. Dernier lot partiel géré.
- ✅ **Chargeur MNIST** `data::LoadMnist` (format **IDX** big-endian) → [N,784]
  normalisé [0,1] + labels 0..9. Générateur synthétique `data::MakeBlobs` (amas 2D).
- 🎯 ✅ **On itère sur des lots prêts pour l'entraînement** (`NKDataTest`, `jenga run`)
  → **7/7 OK** : couverture complète, histogramme préservé, formes `[B,D]/[B,C]`,
  one-hot correct, shuffle effectif. (MNIST : brancher `NK_MNIST_DIR` vers les fichiers
  IDX décompressés pour l'exercer sur données réelles.)

## Jalon 2 — texte ✅ (2026-07-26, tokenizer+padding ; chargement en flux reste ⬜)
- ✅ **Tokenizer** (texte → identifiants) + détokenizer : `NkTokenizer.h/.cpp` —
  **déplacé/généralisé depuis `NKGpt/NkGptCore`** (BPE from-scratch, `data::NkBpe` +
  `data::TrainBpe` + `data::DecodeAll`), qui n'avait AUCUNE logique spécifique GPT.
  `NKGpt` réexporte via des alias (`gpt::Bpe` == `data::NkBpe`, etc.) pour rester
  100% compatible avec `NkGptTrainer`/`NkGptCore` (checkpoint « NKGP » inchangé,
  reconstruit et testé : `NKGptTrain` tourne toujours, tokenizer BPE + génération
  identiques). Preuve réelle (`NKDataTest`) : BPE entraîné sur un corpus français
  réel, round-trip **exact** (encode->decode) y compris sur du texte HORS corpus
  d'entraînement (propriété du BPE octet-à-octet).
  Complément : `data::NkVocab` (vocabulaire MOT-À-MOT, whitespace, id 0=`<pad>`/
  1=`<unk>`) pour les cas où un tokenizer BPE est inutilement complexe. Round-trip
  exact prouvé sur des mots connus + repli `<unk>` prouvé sur mot absent.
- ✅ Vocabulaire, séquences, padding : `NkSequence.h/.cpp` — `data::PadSequences`
  empaquette des séquences d'identifiants de longueur VARIABLE (sortie BPE ou
  NkVocab) en un batch rectangulaire `[B,Tmax]` (F32, compatible `autograd::
  Embedding`) + masque (1.0 réel / 0.0 padding) + longueurs réelles ; troncature au
  préfixe si `maxLen` fourni. Preuve réelle (`NKDataTest`) : forme/contenu/masque
  vérifiés élément par élément sur un lot construit à la main, troncature vérifiée.

## Jalon 3 — robustesse ✅ (2026-07-26, chargement en flux reste ⬜)
- ✅ **Augmentation** de données (images, texte) : `NkAugment.h/.cpp`.
  - Image : `AugmentFlipHorizontal(ds, rows, cols)` (miroir gauche-droite d'un jeu
    image-comme-vecteur, ex. MNIST) — preuve réelle : image 2x3 connue, miroir exact.
  - Numérique générique : `AugmentGaussianNoise(ds, stddev, seed)` (Box-Muller,
    déterministe) — fonctionne sur N'IMPORTE QUEL `NkDataset`, pas seulement des
    images ; preuve réelle : labels inchangés, amplitude du bruit de l'ordre de
    grandeur attendu pour `stddev` donné.
  - Texte : `AugmentTokenDropout(ids, dropProb, seed)` (word-dropout sur séquence
    d'identifiants, jamais de séquence vide) — preuve réelle : sous-séquence VALIDE
    (ordre préservé) vérifiée élément par élément.
  - `ConcatDatasets` : recolle original + augmenté en un seul jeu d'entraînement.
- ✅ Découpage entraînement / validation / test : `data::SplitDataset` (mélange
  Fisher-Yates déterministe, même LCG que `NkDataLoader::Shuffle`, fractions
  configurables). Preuve réelle (`NKDataTest`) : train+val+test == taille originale
  (pas de perte NI de doublon, chaque exemple dans EXACTEMENT une part).
- ⬜ Chargement en flux (gros jeux) — reste à faire (nécessite E/S asynchrone,
  hors périmètre de cette itération, cf. `NKScheduler`/threads en "Plus tard").

## Plus tard
- ⬜ Observations d'agents (civilisation) comme données.
- ⬜ Préchargement asynchrone (via NKScheduler / threads).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
