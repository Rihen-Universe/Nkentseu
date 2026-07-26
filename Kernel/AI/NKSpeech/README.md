# NKSpeech — parole from-scratch (ASR + TTS) pour NKAI

Module de **reconnaissance vocale (ASR, audio→texte)** et **synthèse vocale (TTS, texte→audio)**,
from-scratch, zero-STL, `namespace nkentseu::ai`. Petite échelle, pédagogique — jamais « niveau
Whisper/Tacotron ». S'appuie sur la **capture micro** (`NKAudio::NkAudioCapture`) et le **débruitage**
(`NKAudio::NkDenoiser`), et sur **NKAutograd/NKNN** pour les modèles.

> **État (2026-07-23)** : briques 1-2 **livrées** (`NkAudioFeatures` MFCC/log-Mel, `NkAsrModel` BiGRU+CTC).
> Vocodeur Griffin-Lim + synthèse par formants (`NkVoiceSynth`) **livrés**. Brique 4 (TTS front-end) :
> **`NkG2P` (G2P rule-based fr/en/bbj) LIVRÉE + testée** (`NKSpeechTest`, self-test sur mots réels bbj).
> `NkTTS` (assemblage complet texte→audio) reste scaffold. Suite planifiée dans
> **`Kernel/AI/ROADMAP.md` → Phase 8 — Parole**.

## Enjeu : langues locales (dont le ghomala')
Objectif fort : **multilingue camerounais**, dont le **ghomala' (`bbj`)** — déjà un tag du corpus GPT.
Le corpus texte/voix reste à enrichir (collecte de sources publiques, alignement, nettoyage).

## G2P bbj (ghomala') — sources vérifiées (2026-07-23)

`NkG2P` (`src/NKSpeech/NkG2P.h/.cpp`) implémente un G2P **rule-based** (aucune donnée, aucun modèle
appris, aucune génération LLM — conformément à la règle du projet pour les langues peu dotées) pour
**bbj** (implémentation principale, tracée aux sources) et des règles orthographiques simplifiées
pour **fr/en** (best-effort, non exhaustif). Le matching se fait sur des **codepoints Unicode**
(jamais des littéraux de chaîne non-ASCII dans le `.cpp`), donc portable quel que soit le charset
source du compilateur.

**Sources réelles consultées (2026-07-23)**, détaillées en bas de `NkG2P.h` :
1. Wikipedia (EN) [« Ghomálá' language »](https://en.wikipedia.org/wiki/Ghomala%CA%BC_language) —
   inventaire phonémique complet (IPA), système à **5 tons** (aigu=haut, grave=bas, non marqué=moyen,
   caron=montant, circonflexe=descendant), règle d'affrication p/b/t/d/k + h → [pɸ bβ tθ dð kx],
   nasalisation près de ŋ. Cite Nissim, Gabriel M. (1981), *Le Bamileke-Ghomálá' (parler de
   Bandjoun, Cameroun) : phonologie - morphologie nominale...*, SELAF, Paris, ISBN 978-2-85297-104-2.
2. Wikipédia (FR) [« Ghomala' »](https://fr.wikipedia.org/wiki/Ghomala%27) — confirme alphabet 40+
   caractères, digraphes bv/dz/pf, notation tonale.
3. Wikipédia (FR) [« Alphabet général des langues camerounaises »](https://fr.wikipedia.org/wiki/Alphabet_g%C3%A9n%C3%A9ral_des_langues_camerounaises)
   — AGLC/ALCAM (Tadadjeu & Sadembouo, 1978/1979) : voyelles ɛ ə ɔ ʉ, consonnes ŋ ɣ, digraphes.
4. Corpus réel déjà présent dans le projet : Nouveau Testament ghomala', © 2002 Bible Society of
   Cameroon (`AI/corpus/lamba/bbj_ghomala_nt.txt`) — analyse EMPIRIQUE des codepoints Unicode
   (script Python, 2026-07-23) pour confirmer les graphèmes/diacritiques réellement utilisés.
   Existence confirmée via [bibliamundi.com (PDF)](https://bibliamundi.com/wp-content/uploads/2023/09/Ghomala-Bible-New-Testament.pdf)
   et [bible.com/languages/bbj](https://www.bible.com/languages/bbj).
5. Lexique [lamba-africa.com](https://lamba-africa.com) (`AI/corpus/lamba/ghomala/records.json`) —
   mots isolés AVEC traduction française vérifiée, utilisés comme cas de test :
   « dɔ̀mnyə̀ » = « impasse », « lɛtə̌ » = « solide ».

**Ce qui N'est PAS directement sourcé** (documenté honnêtement dans `NkG2P.h`, pas inventé) : les
graphèmes "c"→/tʃ/, "j"→/dʒ/ (déduits de l'usage récurrent dans le corpus + convention ALCAM
standard) ; "ç"→/s/ (graphème rare, contamination du fichier NT numérisé) ; /r/ (absent de
l'inventaire phonémique source, mais présent dans les noms propres/emprunts du texte réel) ;
les règles fr/en (orthographe standard, connaissance générale, pas de citation académique dédiée).

**Recherché mais non retenu comme source directe** : l'application **Bibala** (lancée le
2026-06-25, disponible sur App Store/Play Store, couvre le ghomala' parmi d'autres langues
camerounaises) — c'est une appli d'apprentissage (leçons/exercices), **pas un dictionnaire ou
corpus exploitable/téléchargeable** ; son contenu pédagogique n'est pas accessible en dehors de
l'app pour en extraire des règles G2P ou un corpus.

### Validation MFCC sur parole réelle (2026-07-10)
`NKSpeechFeatureDemo <fichier>` décode (NKAudio) → mono → MFCC. **Prouvé sur le corpus** :
Bassa 7.66 s → 764×39 MFCC, ghomala' 2.46 s → 244×39 MFCC (features non triviales).
⚠️ **Format du corpus lamba** : les clips (extension `.mp3`) sont en réalité du **WebM (ghomala') / MP4-AAC
(Bassa)** — enregistrements navigateur (getUserMedia). NKAudio décode WAV/MP3/OGG/FLAC mais **pas** WebM/AAC →
**transcoder en WAV 16 kHz mono** (ffmpeg) avant l'entraînement (étape de préparation du dataset). Un décodeur
AAC/Opus from-scratch dans NKAudio = travail futur.

## Fichiers
- `src/NKSpeech/NkAudioFeatures.h` — features (Mel-spectrogramme, MFCC). **Fondation partagée ASR + TTS.** ✅
- `src/NKSpeech/NkAsrModel.h` — ASR acoustique (BiGRU + CTC), header-only. ✅
- `src/NKSpeech/NkG2P.h/.cpp` — **G2P rule-based fr/en/bbj** (texte → phonèmes + tons). bbj = implémentation
  principale, sources vérifiées (voir section dédiée ci-dessus et bas de `NkG2P.h`). ✅
- `src/NKSpeech/NkTextNorm.h/.cpp` — **normalisation de texte front-end TTS fr/en** (nombres cardinaux
  entiers/décimaux/négatifs en toutes lettres, ponctuation → pauses symboliques avec durée, quelques
  abréviations sourcées) — EN AMONT du G2P (`NkG2P` ignore les chiffres, cf. sources en tête de
  `NkTextNorm.h`). bbj hors périmètre (numération non sourcée). ✅
- `src/NKSpeech/NkGriffinLim.h/.cpp` — vocodeur Griffin-Lim (spectrogramme → onde). ✅
- `src/NKSpeech/NkVoiceSynth.h/.cpp` — synthèse par formants (source-filtre) → onde audible. ✅
- `src/NKSpeech/NkASR.h` — spec haut-niveau (lexique/LM, scaffold restant).
- `src/NKSpeech/NkTTS.h` — assemblage complet texte → audio (scaffold, branchera NkG2P + un modèle
  acoustique appris + le vocodeur).

## Ordre d'implémentation (cf. ROADMAP Phase 8)
1. ✅ `NkAudioFeatures` (MFCC). 2. ✅ `NkAsrModel` acoustique (BiGRU+CTC). 3. ⬜ Lexique/LM.
4. 🟡 `NkTTS` front-end — ✅ **G2P** (`NkG2P`) livré ; ✅ **normalisation texte** (`NkTextNorm`,
   nombres/ponctuation) livrée ; reste durées de phonèmes (timing model) et l'assemblage `NkTTS` complet.
5. 🟡 `NkTTS` acoustique + vocodeur — ✅ Griffin-Lim + synthèse par formants ; reste modèle appris.
6. ⬜ Boucle voix (micro→ASR→TTS). 7. ⬜ Corpus langues locales (ghomala') à enrichir au-delà du NT.

## Dépendances à ajouter
- **CTC loss** + couches **GRU/LSTM** dans NKAutograd/NKNN.
- **Griffin-Lim** (reconstruction de phase) dans NKSpeech.
