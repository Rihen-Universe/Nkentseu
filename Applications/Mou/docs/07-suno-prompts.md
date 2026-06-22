# Mú — Prompts de génération audio (Suno + outils)

> Prompts prêts à copier-coller pour générer **musiques** (Suno), **effets** et
> **voix**. Identité sonore : chaleureuse, enfantine, **instruments africains**
> (balafon, kalimba/mbira, kora, djembé, marimba) cohérents avec « Mú » (Cameroun).
> Renvoie aux fichiers attendus par le jeu : [05-audio-script.md](05-audio-script.md)
> (voix) et [06-effets-audio.md](06-effets-audio.md) (sons).
> Date : 2026-06-17.

---

## 0. Quel outil pour quoi (important)

| Besoin | Outil conseillé | Pourquoi |
|--------|-----------------|----------|
| **Musiques de fond / jingles** | **Suno** | Suno excelle pour la musique (instrumental + chansons). |
| **Effets courts** (pop, ding, boing…) | **ElevenLabs Sound Effects** ou freesound.org | Suno fait des morceaux, pas des bruitages d'1 s. |
| **Voix parlées** (consignes FR, « Bravo ») | **ElevenLabs** (voix FR) **ou voix humaine** | Suno ne fait pas de TTS parlé propre ; il **chante**. |
| **Chanson-thème** (refrain chanté) | **Suno** | Là Suno est parfait (paroles + mélodie). |

> Règle export : récupère en **WAV** si possible, puis convertis selon les specs
> de la doc 05 §2 (WAV mono/stéréo 44.1 kHz, normalisé -3 dB). Les musiques peuvent
> rester en **.ogg/.mp3** (poids).

---

## 1. MUSIQUES DE FOND (Suno — Instrumental, en boucle)

Pour chaque piste : coche **Instrumental**, vise **~30–60 s**, demande une boucle
douce. Colle le texte dans le champ **Style of Music** (Suno v4).

### `musique_fond.wav` — musique générale / menu
```
Cheerful gentle children's music, warm African kids vibe, soft balafon and kalimba melody, light hand percussion (djembe brushes, shakers), marimba, mellow bass, major key, slow-to-mid tempo ~90 BPM, playful and reassuring, no vocals, seamless loop, lo-fi warmth, nursery-friendly, not overstimulating
```

### `musique_cour.wav` — monde « La cour » (jour)
```
Happy sunny children's instrumental, bright balafon and marimba, playful kalimba arpeggios, light djembe groove, birds-like flute touches, major key, ~100 BPM, bouncy and innocent, African folk feel, no vocals, loopable
```

### `musique_marche.wav` — monde « Le marché » (animé)
```
Lively West/Central African market groove for kids, energetic djembe and dunun rhythm, kora and balafon melody, claps, mid tempo ~110 BPM, joyful and busy but gentle, major key, no vocals, warm acoustic, seamless loop
```

### `musique_fete.wav` — monde « La fête » (soir)
```
Soft celebratory evening children's tune, mbira and kalimba sparkle, mellow marimba, gentle congas, warm pads, slow ~85 BPM, cozy and magical, twinkly, major key, no vocals, lullaby-adjacent, loopable
```

---

## 2. JINGLES / STINGS courts (Suno — Instrumental, très court)

Vise **1–3 s**, demande un « short jingle / sting, no loop ».

### `sfx_niveau.wav` — fanfare de fin de niveau (réussite)
```
Very short happy fanfare sting for kids, triumphant balafon and marimba flourish, bright bells, quick ascending major arpeggio, 2 seconds, celebratory, no vocals
```

### Bonus — sting de niveau supérieur / déblocage
```
Short magical reward chime, ascending kalimba sparkle with a soft cymbal swell, 1.5 seconds, joyful, no vocals
```

---

## 3. CHANSON-THÈME « Mú » (Suno — avec paroles, optionnel mais top)

Style :
```
Joyful children's sing-along, simple catchy melody, warm female lead voice + kids choir, African pop for toddlers, balafon kalimba djembe marimba, major key, mid tempo ~100 BPM, bright and tender
```
Paroles (Lyrics) :
```
[Verse]
Mú, Mú, on ouvre les yeux,
Les couleurs dansent dans le ciel bleu.
Mú, Mú, on compte avec toi,
Un, deux, trois, viens joue avec moi !

[Chorus]
Mú ! Mú ! On apprend en jouant,
Mú ! Mú ! On grandit en chantant !
```

---

## 4. EFFETS COURTS (ElevenLabs Sound Effects — pas Suno)

Outil : ElevenLabs « Sound Effects » (ou freesound.org). Prompts en anglais
(meilleurs résultats). Garder **doux**, jamais agressif. Noms = doc 06 §2.

| Fichier | Prompt (ElevenLabs SFX) |
|---------|-------------------------|
| `sfx_tap.wav`     | `soft cute UI pop, short, bubbly, playful, single tap` |
| `sfx_bon.wav`     | `happy success chime, cheerful ascending bells, kids game correct answer, short` |
| `sfx_mauvais.wav` | `gentle soft boing, friendly wrong answer, not harsh, cartoonish, short` |
| `sfx_etoile.wav`  | `sparkling magic twinkle ding, star appears, short, bright` |
| `sfx_pose.wav`    | `soft plop, object dropped into basket, gentle, short` |
| `sfx_echec.wav`   | `gentle soft descending tone, encouraging not sad, short` |
| `sfx_clic.wav`    | `satisfying soft wooden click, shape sorter piece snapping into place, short, toy-like` |
| `sfx_carte.wav`   | `soft quick card flip swoosh, paper card turning over, gentle, very short` |

**Cris d'animaux (optionnel — jeu « Les Animaux », `assets/sfx/`)** — courts, doux, rigolos :
| Fichier | Prompt (ElevenLabs SFX) |
|---------|-------------------------|
| `cri_canard.wav`   | `cute friendly duck quack, single, soft, cartoonish, short` |
| `cri_lion.wav`     | `gentle cute baby lion roar, friendly not scary, short` |
| `cri_elephant.wav` | `soft playful elephant trumpet, cute, short` |
| `cri_singe.wav`    | `cheerful little monkey ooh-ooh, playful, short` |
| `cri_oiseau.wav`   | `sweet little bird chirp tweet, cheerful, short` |

---

## 5. VOIX PARLÉES (ElevenLabs FR ou voix humaine — pas Suno)

La liste **exacte** des phrases + noms de fichiers est dans [05-audio-script.md](05-audio-script.md).
Style de voix à demander (ElevenLabs, voix française) :
```
Voix féminine française chaleureuse et douce, souriante, lente et bien articulée,
qui s'adresse à un enfant de 3 ans, ton bienveillant et encourageant, studio propre.
```
- Génère **chaque phrase séparément** (un fichier par ligne du script 05).
- Mots isolés (couleurs, nombres, « et », « plus », **noms d'animaux**, **noms de formes**)
  = **neutres**, pour que le jeu les enchaîne. Voir doc 05 §3.4 à §3.7.
- **Priorité pour les 3 nouveaux jeux** : `animal_*` (8) + `animaux_consigne` sont
  **essentiels** (le jeu « Les Animaux » se joue à l'oreille) ; puis `formes_consigne`,
  `forme_*` (5), `memoire_consigne`. Les `felicitation_*`/`encouragement_*` sont communs.
- Si voix humaine : mêmes consignes (doc 05 §5 « comment enregistrer »).

---

## 6. Récap de livraison

1. **Suno** → musiques §1 + jingles §2 (+ chanson §3 optionnelle) → `assets/sfx/`
   (musiques) en `.ogg/.mp3`, jingles en `.wav`.
2. **ElevenLabs SFX / freesound** → effets §4 → `assets/sfx/*.wav`.
3. **ElevenLabs FR / humain** → voix §5 → `assets/voice/*.wav` (noms = doc 05).

Dès réception (même partielle), je route les `Cue` + la musique vers **NKAudio**.
