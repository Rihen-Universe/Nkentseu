# Mú — Guide d'enregistrement audio (voix FR + sons)

> Tout ce qu'il faut **dire**, **comment** le dire, et **où** déposer les fichiers.
> Objectif : un enfant non-lecteur doit pouvoir tout faire **à l'oreille**.
> Date : 2026-06-15.

---

## 1. Principes de jeu vocal (3-5 ans)

- **Voix chaleureuse, souriante, lente.** On parle à un enfant de 3 ans, pas à un adulte.
- **Une voix principale = la mascotte « Nana »** (idéalement une voix féminine douce).
  On peut garder la même voix pour tout au début.
- **Articuler, laisser respirer.** Phrases courtes. Sourire en parlant (ça s'entend).
- **Positif, jamais grondant.** Même l'erreur est encourageante (« Presque ! Essaie encore »).
- **Cohérence des noms** : toujours le même mot pour le même objet/couleur.

---

## 2. Spécifications techniques

| Réglage | Valeur recommandée |
|---------|--------------------|
| Format | **WAV** (PCM 16 bits) — simple et sans perte. MP3/OGG acceptés aussi (NKAudio lit WAV/MP3/OGG). |
| Fréquence | **44 100 Hz** |
| Canaux | **Mono** (voix) |
| Niveau | Normaliser à ~ **-3 dB**, pas de saturation |
| Silence | Couper les blancs en début/fin (~100 ms de marge) |
| Durée/clip | 1 à 3 s en général |

### Où déposer les fichiers
```
Applications/Mou/assets/
├── voice/   ← toutes les voix (.wav)
└── sfx/     ← les bruitages (.wav)
```
Les fichiers seront automatiquement embarqués dans l'APK Android (déjà câblé :
`androidassets(["assets"])`) et lus par NKAudio via le même mécanisme que les SVG.

### Convention de nommage
`categorie_nom.wav` en **minuscules, sans accents, sans espaces** (underscore).
Exemple : `couleur_rouge.wav`, `fruit_tomate.wav`, `nombre_3.wav`.
**Respecte exactement les noms de fichiers du script** ci-dessous (le code les appellera tels quels).

---

## 3. SCRIPT — voix à enregistrer (`assets/voice/`)

### 3.1 Mascotte & global
| Fichier | Texte à dire | Ton / direction |
|---------|--------------|-----------------|
| `mascotte_bienvenue.wav` | « Bonjour ! Je suis Nana. On joue ensemble ? » | Accueil joyeux, lent |
| `mascotte_choisis_jeu.wav` | « Choisis un jeu ! » | Invitant |
| `felicitation_bravo.wav` | « Bravo ! » | Enthousiaste |
| `felicitation_super.wav` | « Super ! » | Enthousiaste |
| `felicitation_tu_es_fort.wav` | « Tu es trop fort ! » | Admiratif |
| `encouragement_essaie.wav` | « Essaie encore ! » | Doux, jamais déçu |
| `encouragement_presque.wav` | « Presque ! » | Doux, taquin |
| `transition_niveau.wav` | « Et maintenant… le niveau suivant ! » | Excité |
| `recompense_etoiles.wav` | « Tu as gagné des étoiles ! » | Festif |

### 3.2 Jeu « Les Couleurs »
**Consigne**
| Fichier | Texte | Ton |
|---------|-------|-----|
| `couleurs_consigne.wav` | « Range chaque fruit dans le panier de sa couleur ! » | Clair, posé |
| `couleurs_donne_moi.wav` | « Donne-moi… » | Suspens (suivi d'un nom de couleur) |

**Noms de couleurs** (dits seuls, on les enchaîne après « Donne-moi »)
| Fichier | Texte |
|---------|-------|
| `couleur_rouge.wav` | « le rouge » |
| `couleur_jaune.wav` | « le jaune » |
| `couleur_vert.wav` | « le vert » |
| `couleur_bleu.wav` | « le bleu » |
| `couleur_orange.wav` | « l'orange » |
| `couleur_violet.wav` | « le violet » |

**Noms de fruits** (mode vocabulaire — dits quand l'enfant prend le fruit)
| Fichier | Texte |
|---------|-------|
| `fruit_tomate.wav` | « la tomate » |
| `fruit_mangue.wav` | « la mangue » |
| `fruit_avocat.wav` | « l'avocat » |
| `fruit_safou.wav` | « le safou » |
| `fruit_papaye.wav` | « la papaye » |
| `fruit_plantain.wav` | « la banane plantain » |

### 3.3 Jeu « Compter » (à venir)
**Consigne / questions**
| Fichier | Texte |
|---------|-------|
| `compter_consigne.wav` | « Compte avec moi ! » |
| `compter_combien.wav` | « Combien y en a-t-il ? » |
| `compter_donne_moi.wav` | « Donne-moi… » (suivi d'un nombre) |

**Nombres 1 à 10** (dits seuls — servent au comptage ET aux réponses)
| Fichier | Texte | | Fichier | Texte |
|---------|-------|-|---------|-------|
| `nombre_1.wav` | « un » | | `nombre_6.wav` | « six » |
| `nombre_2.wav` | « deux » | | `nombre_7.wav` | « sept » |
| `nombre_3.wav` | « trois » | | `nombre_8.wav` | « huit » |
| `nombre_4.wav` | « quatre » | | `nombre_9.wav` | « neuf » |
| `nombre_5.wav` | « cinq » | | `nombre_10.wav` | « dix » |

### 3.4 Jeu « Calculs » (à venir)
| Fichier | Texte | Ton |
|---------|-------|-----|
| `calcul_consigne.wav` | « Combien ça fait ? » | Curieux |
| `calcul_et.wav` | « et » | Liaison (« deux *et* un ») |
| `calcul_plus.wav` | « plus » | Liaison |
| `calcul_moins.wav` | « moins » | Liaison |
| `calcul_egal.wav` | « ça fait… » | Suspens |

> Astuce : pour « 2 et 1 ça fait 3 », le moteur enchaînera
> `nombre_2` + `calcul_et` + `nombre_1` + `calcul_egal` + `nombre_3`.
> Donc enregistre chaque mot **isolé** et **neutre** (pas de phrase complète).

### 3.5 Jeu « Les Formes »
**Consigne**
| Fichier | Texte | Ton |
|---------|-------|-----|
| `formes_consigne.wav` | « Mets chaque forme à sa place ! » | Clair, posé |
| `formes_bravo.wav` | « Pile dans le trou ! » | Joyeux (forme bien encastrée) |

**Noms de formes** (dits quand l'enfant prend / pose une forme — mode vocabulaire)
| Fichier | Texte |
|---------|-------|
| `forme_rond.wav` | « le rond » |
| `forme_carre.wav` | « le carré » |
| `forme_triangle.wav` | « le triangle » |
| `forme_etoile.wav` | « l'étoile » |
| `forme_coeur.wav` | « le cœur » |

### 3.6 Jeu « Les Animaux »
**Consigne** (la consigne dit le nom de l'animal cible — voix **essentielle** ici,
l'enfant choisit à l'oreille)
| Fichier | Texte | Ton |
|---------|-------|-----|
| `animaux_consigne.wav` | « Touche le… » | Suspens (suivi d'un nom d'animal) |
| `animaux_ecoute.wav` | « Écoute bien ! » | Doux, attentif |

**Noms d'animaux** (dits seuls — enchaînés après « Touche le… », et redits si on touche la carte)
| Fichier | Texte | | Fichier | Texte |
|---------|-------|-|---------|-------|
| `animal_canard.wav` | « le canard » | | `animal_poisson.wav` | « le poisson » |
| `animal_margouillat.wav` | « le margouillat » | | `animal_tortue.wav` | « la tortue » |
| `animal_elephant.wav` | « l'éléphant » | | `animal_singe.wav` | « le singe » |
| `animal_lion.wav` | « le lion » | | `animal_oiseau.wav` | « l'oiseau » |

> Bonus immersif (optionnel) : un **cri** par animal joué quand on touche la bonne
> carte → `cri_canard.wav`, `cri_lion.wav`, `cri_elephant.wav`, `cri_singe.wav`,
> `cri_oiseau.wav` (court, doux, rigolo). Range-les dans `assets/sfx/`.

### 3.7 Jeu « La Mémoire »
| Fichier | Texte | Ton |
|---------|-------|-----|
| `memoire_consigne.wav` | « Retrouve les paires ! » | Clair, posé |
| `memoire_paire.wav` | « Une paire ! » | Enthousiaste (deux cartes identiques) |
| `memoire_encore.wav` | « Cherche encore ! » | Doux (deux cartes différentes) |

> Les **noms de fruits** (`fruit_*`, déjà au §3.2) peuvent être redits quand une
> paire de fruits est trouvée (mode vocabulaire).

---

## 4. SONS / BRUITAGES (`assets/sfx/`)

Pas de voix — des sons courts. Tu peux les enregistrer (bouche, objets) ou prendre
des sons libres de droits (freesound.org). **Doux**, jamais agressifs.

| Fichier | Quand | Description du son |
|---------|-------|--------------------|
| `sfx_tap.wav` | toucher un objet | petit « pop » léger |
| `sfx_bon.wav` | bonne réponse | carillon / clochette montante, joyeux |
| `sfx_mauvais.wav` | mauvaise réponse | « boing » doux, gentil (PAS un buzzer) |
| `sfx_etoile.wav` | apparition d'une étoile | « ding » scintillant |
| `sfx_niveau.wav` | fin de niveau | petite fanfare courte |
| `sfx_pose.wav` | poser un objet dans le panier | « plop » doux |
| `sfx_clic.wav` | **Formes** : une forme s'encastre dans son trou | « clic » de bois, satisfaisant |
| `sfx_carte.wav` | **Mémoire** : on retourne une carte | léger « swip » / froissement de carte |
| `musique_fond.wav` | musique d'ambiance | boucle douce, instrumentale, ~30-60 s (peut être .ogg/.mp3 pour le poids) |

---

## 5. Comment enregistrer (pratique)

1. **Matériel** : un smartphone récent suffit (appli dictaphone) ou un micro USB.
2. **Environnement** : pièce calme, textiles autour (rideaux, lit) pour limiter l'écho.
   Évite ventilateur/frigo/ambiance.
3. **Distance** : ~15-20 cm du micro, légèrement de côté (évite les « pop » des P/B).
4. **Méthode** : enregistre chaque ligne **3 fois**, garde la meilleure prise.
5. **Sourire** en parlant. Parle **comme à un enfant de 3 ans**, lentement.
6. **Édition** (gratuit : Audacity) : couper les silences, normaliser à -3 dB,
   exporter en **WAV mono 44.1 kHz**, nommer **exactement** comme dans le script.
7. **Livraison** : dépose les `.wav` dans `Applications/Mou/assets/voice/` et
   `Applications/Mou/assets/sfx/`. Préviens-moi, je branche la lecture dans le jeu.

---

## 6. Récap : priorité d'enregistrement

Pour entendre le jeu **Les Couleurs** parler dès que possible, enregistre **d'abord** :
`couleurs_consigne`, `couleur_*` (6), `fruit_*` (6), `felicitation_bravo`,
`encouragement_essaie`, `recompense_etoiles`, + SFX `sfx_tap`, `sfx_bon`,
`sfx_mauvais`, `sfx_etoile`. Le reste (Compter/Calculs) viendra avec ces jeux.
