# NKSpeech — parole from-scratch (ASR + TTS) pour NKAI

Module de **reconnaissance vocale (ASR, audio→texte)** et **synthèse vocale (TTS, texte→audio)**,
from-scratch, zero-STL, `namespace nkentseu::ai`. Petite échelle, pédagogique — jamais « niveau
Whisper/Tacotron ». S'appuie sur la **capture micro** (`NKAudio::NkAudioCapture`) et le **débruitage**
(`NKAudio::NkDenoiser`), et sur **NKAutograd/NKNN** pour les modèles.

> ⚠️ **État : SCAFFOLD (2026-07-10)** — seules les *interfaces* (spec headers) sont posées.
> L'implémentation est planifiée par étapes dans **`Kernel/AI/ROADMAP.md` → Phase 8 — Parole**.

## Enjeu : langues locales (dont le ghomala')
Objectif fort : **multilingue camerounais**, dont le **ghomala' (`bbj`)** — déjà un tag du corpus GPT.
Le corpus texte/voix reste à enrichir (collecte de sources publiques, alignement, nettoyage).

## Fichiers
- `src/NKSpeech/NkAudioFeatures.h` — features (Mel-spectrogramme, MFCC). **Fondation partagée ASR + TTS.**
- `src/NKSpeech/NkASR.h` — audio → texte (modèle acoustique + décodage CTC + lexique/LM).
- `src/NKSpeech/NkTTS.h` — texte → audio (G2P → mel → vocodeur Griffin-Lim puis neuronal).

## Ordre d'implémentation (cf. ROADMAP Phase 8)
1. `NkAudioFeatures` (MFCC) — testable headless. 2. `NkASR` acoustique (CTC). 3. Lexique/LM.
4. `NkTTS` front-end (G2P). 5. `NkTTS` acoustique + vocodeur. 6. Boucle voix (micro→ASR→TTS).
7. Corpus langues locales (ghomala').

## Dépendances à ajouter
- **CTC loss** + couches **GRU/LSTM** dans NKAutograd/NKNN.
- **Griffin-Lim** (reconstruction de phase) dans NKSpeech.
