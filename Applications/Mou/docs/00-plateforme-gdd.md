# Mú — GDD Plateforme (jeux éducatifs maternelle 3-5 ans)

> Document de design global. Équivalent de Nkoung, mais pour les tout-petits.
> Nom **Mú** = « enfant » en ghomálá' (bamiléké, Cameroun ; prononcé « mou »).
> Nom de code / dossier / namespace : `Mou`. Date : 2026-06-15.

---

## 1. Vision

Une plateforme de **mini-jeux éducatifs** pour enfants de **3 à 5 ans** (maternelle),
qui apprennent **en jouant** : reconnaître les couleurs, compter, faire de petits
calculs. Construite sur le moteur **Nkentseu** (NKCanvas + NKAudio + NKImage + NKFont),
cible **tablette** (Android d'abord, puis HarmonyOS et iOS).

**Principe pédagogique** : l'enfant n'échoue jamais. On encourage, on récompense,
on répète. La **mascotte Nana** parle (voix française) et guide chaque étape — un
non-lecteur doit pouvoir tout faire **sans lire**.

---

## 2. Public & contraintes (synthèse recherche)

| Contrainte | Règle de design |
|------------|-----------------|
| Ne sait pas lire | **Tout en voix + image**. Zéro texte d'instruction. |
| Petites mains, motricité fine en cours | Cibles **≥ 2 cm**, très espacées. **Tap & drag** seulement (pas de pinch/swipe). |
| Attention courte | Sessions **10-15 min**, niveaux **courts** (30-90 s). |
| Sur-stimulation à éviter | Animations douces, pas de timer stressant, pas de game-over. |
| Besoin de feedback | **Retour immédiat** son + animation à chaque toucher. |
| Daltonisme (8 % garçons) | Couleur **+ symbole** toujours (cœur/rond/étoile/feuille). |

Direction artistique : voir [DESIGN_STYLE_GUIDE.md](../DESIGN_STYLE_GUIDE.md)
(cartoon flat chaleureux, palette `MouUIColor.h`, assets SVG).

---

## 3. Les 3 jeux pilotes

| Jeu | Compétence | Fichier GDD |
|-----|-----------|-------------|
| **Les Couleurs** | Reconnaître / trier les couleurs | [01-couleurs-gdd.md](01-couleurs-gdd.md) |
| **Compte avec moi** | Dénombrer de 1 à 10 | [02-compter-gdd.md](02-compter-gdd.md) |
| **Petits calculs** | Addition / soustraction ≤ 10 | [03-calcul-gdd.md](03-calcul-gdd.md) |

Progression naturelle par âge : Couleurs (3 a) → Compter (4 a) → Calcul (5 a).

**Identité culturelle** : les éléments (fruits, animaux, objets, décors) sont
**camerounais/africains** — voir le catalogue validé [04-elements-culturels.md](04-elements-culturels.md)
(pilote le pack d'assets). Mondes locaux : « le marché », « la cour », « la fête ».

---

## 4. Système de niveaux (CŒUR de la plateforme)

> Exigence : **chaque jeu a plusieurs niveaux** pour stimuler l'enfant à agir et
> progresser. C'est le moteur de motivation principal.

### 4.1 Structure

- Un jeu = une **liste de niveaux** regroupés en **mondes** (3-4 mondes / jeu).
- Un monde = un **thème visuel** (ex. la forêt, la plage, l'espace) qui change le
  décor et la mascotte d'ambiance — la nouveauté visuelle relance l'intérêt.
- Chaque niveau a des **paramètres de difficulté** (nombre d'éléments, vitesse,
  options) → la courbe monte **doucement**.

### 4.2 Récompense par étoiles (douce, jamais punitive)

- À la fin d'un niveau : **1 à 3 étoiles** selon la maîtrise
  (3 = aucune erreur, 2 = quelques essais, 1 = réussi avec aide).
- **1 étoile suffit toujours** à débloquer le niveau suivant → pas de blocage
  frustrant. Les 3 étoiles sont un objectif de **rejouabilité**, pas une barrière.
- Étoiles cumulées par monde → débloque une **récompense** (nouvel ami, autocollant,
  petit décor) : motivation à revenir.

### 4.3 Écran de sélection de niveau

- La **mascotte Nana** présente le monde (voix).
- Rangée de **grosses cartes-niveaux** (cf. `card_frame.svg`) : numéro illustré,
  étoiles gagnées, cadenas si verrouillé. Tap = lance le niveau.
- Navigation par **gros boutons flèche** (pas de swipe obligatoire).

### 4.4 Adaptatif léger (anti-frustration / anti-ennui)

- Si l'enfant fait **beaucoup d'essais** sur un niveau → Nana propose de l'aide
  (surligne la bonne réponse après quelques secondes) au lieu de le bloquer.
- S'il réussit **très vite** plusieurs niveaux → propose de sauter au monde suivant.

### 4.5 Données de niveau (format)

Niveaux décrits en **JSON** (`assets/levels/<jeu>.json`), chargés via NkFile
(NKFileSystem, multi-plateforme). Permet d'ajouter/équilibrer des niveaux **sans
recompiler**. Schéma type :

```json
{
  "game": "couleurs",
  "worlds": [
    {
      "id": "foret", "title": "La forêt", "theme": "leaf",
      "levels": [
        { "id": "c1", "colors": ["red","blue"], "items": 3, "mode": "sort",
          "timed": false, "starsFast": 12.0 },
        { "id": "c2", "colors": ["red","blue","yellow"], "items": 4, "mode": "sort" }
      ]
    }
  ]
}
```

### 4.6 Sauvegarde de progression

- **Profil enfant** (un ou plusieurs) : `save/profile_<n>.json` (NkFile).
- Stocke par niveau : débloqué oui/non, meilleures étoiles. Pas de compte en ligne,
  pas de collecte de données (RGPD-friendly enfants).

---

## 5. Architecture technique (réutilise le pattern Nkoung)

```
Applications/Mou/
├── src/Mou/
│   ├── main.cpp                    # entry point (--backend, --profile)
│   ├── Core/        MouConfig (résolution, backend, profil, volume)
│   ├── UI/          MouUIColor.h (palette) + widgets enfant
│   ├── Platform/    MouPlatformApp (fenêtre, renderer, audio, routing, scènes)
│   ├── Audio/       MouVoice (consignes FR), MouSfx (feedback)
│   ├── Levels/      MouLevel (struct), MouLevelLoader (JSON), MouProgress (save)
│   ├── Games/Common/  MouGame (base abstraite), MouFrame (contexte rendu),
│   │                  GameFactory, GameMetadata
│   │   └── Toolkit/   briques partagées (voir §6)
│   └── Games/Specific/ Couleurs/ · Compter/ · Calcul/
├── assets/
│   ├── svg/         (déjà livré : mascotte, paniers, ballons, étoile…)
│   ├── levels/      JSON des niveaux par jeu
│   ├── voice/       enregistrements FR (consignes, félicitations)
│   └── sfx/         sons de feedback, musique douce
└── docs/            ce GDD + un par jeu
```

Modules moteur : **NKCanvas** (rendu 2D, 5 backends), **NKImage** (SVG natif →
`NkSVGImage::Rasterize` par DPI), **NKAudio** (voix + sfx, pilier n°1),
**NKFont** (gros chiffres/scores rendus en direct), **NKEvent** (tactile),
**NKFileSystem** (JSON niveaux + sauvegarde).

Scènes (machine à états, comme Nkoung `AppScene`) :
`Splash → MenuPrincipal (3 jeux) → SélectionNiveau → Jeu → Récompense → (retour)`.

`MouGame` (base abstraite) = même contrat que `NkoungGame` :
`Init / Update / Render / OnEvent / Unload` + `LoadLevel(const MouLevel&)`,
`IsComplete()`, `GetStars()`.

---

## 6. Toolkit partagé (codé une fois, réutilisé par les 3 jeux)

| Brique | Rôle |
|--------|------|
| `Mascot` | Nana animée + bulle vocale (joue un clip + anim bouche/saut) |
| `TappableSprite` | Sprite SVG tappable, gros hit-box, anim de réponse (rebond) |
| `DragItem` / `DropZone` | Glisser un objet vers une cible (jeu Couleurs) |
| `AnswerCards` | 2-4 grosses cartes-réponses (Compter, Calcul) |
| `RewardStars` | Animation 1-3 étoiles de fin de niveau |
| `VoicePrompt` | File de consignes vocales (rejoue si inactivité) |
| `LevelSelectBar` | Rangée de cartes-niveaux + navigation |
| `Confetti` / `Sparkle` | Feedback positif (réussite) |

---

## 7. Audio (non négociable)

- **Voix FR** : consigne de chaque niveau, encouragements (« Essaie encore ! »),
  félicitations (« Bravo ! »), nom des couleurs/nombres.
- **SFX** : tap (pop), bonne réponse (carillon), mauvaise (doux « boing », jamais
  agressif), apparition d'étoile.
- **Musique** : boucle douce de fond, volume bas, coupable dans les réglages.
- Tous les sons chargés via NKAudio. Prévoir un **mode muet** (classe/sieste).

---

## 8. Roadmap de production

| Phase | Contenu | État |
|-------|---------|------|
| **P0** | Charte graphique + assets SVG + codec SVG (caps/joins/gradients) | ✅ Fait |
| **P1** | Coquille Mú (fenêtre, menu 3 cartes, scènes, MouGame) — desktop **+ Android** | ✅ Fait |
| **P1.5** | **Scène d'intro/splash** (mascotte Nana animée) + **icônes de jeux SVG** sur les cartes + intégration des assets SVG (NkSVGCodec → NkTexture) | ⏳ Suivant |
| **P2** | Toolkit (Mascot, TappableSprite, DragItem/DropZone, RewardStars) + jeu **Couleurs** (vertical slice) avec **interaction soignée** (feedback tap, anims douces, voix) | ⏳ |
| **P3** | Système de niveaux (JSON loader + sélection + sauvegarde + étoiles) | ⏳ |
| **P4** | Jeux **Compter** et **Calcul** | ⏳ |
| **P5** | Audio FR complet + polish visuel enfant + test tablette réelle avec enfant | ⏳ |
| **P6** | Build Android signé (✅ multi-ABI déjà OK) → HarmonyOS → iOS | ⏳ |

> **Exigences design enfant (transverses, rappelées par Rihen 2026-06-15)** :
> 1. **Scène d'intro/splash** avec la mascotte (accueil chaleureux avant le menu).
> 2. **Icônes de jeux** illustrées (SVG) sur chaque carte — pas juste du texte.
> 3. **Très bon rendu visuel enfant** (cf. charte : cartoon flat, contours encrés,
>    couleurs vives non-néon) **et interaction soignée** (gros hit-box, feedback
>    immédiat son+anim, gestes simples). Ces deux axes priment sur la quantité de
>    contenu : mieux vaut peu de jeux très polis que beaucoup de jeux ternes.

**Le vrai coût n'est pas le code, c'est le contenu** : illustrations cohérentes +
**voix française**. À produire en parallèle dès P2.
