# Mú — Effets & Audio à réaliser (mapping complet)

> Liste **exhaustive** des effets visuels et des sons de chaque jeu, reliés aux
> **événements** du code. Complète le script vocal [05-audio-script.md](05-audio-script.md).
> Date : 2026-06-16.

---

## 1. Comment ça marche (architecture)

Tout passe par le système partagé **`MouFeedback`** (Games/Common). Chaque jeu
appelle une de ces actions au bon moment ; le visuel est **déjà implémenté**, et
chaque action émet un **signal sonore (`Cue`)** prêt à être routé vers NKAudio
**dès que les fichiers `.wav` existent**.

| Action (code) | Quand l'appeler | Effet visuel (✅ fait) | Cue (son) |
|---------------|-----------------|------------------------|-----------|
| `Tap(x,y)`  | petite action satisfaisante (compter 1 objet) | étincelles légères | `Cue::Star` |
| `Good(x,y)` | **sous-mission réussie** (1 fruit bien rangé, bonne carte) | **confettis + étincelles** | `Cue::Good` |
| `Bad(x,y)`  | **sous-mission ratée** (mauvais panier, mauvaise carte) | **puff** + **perte d'1 étoile** (animée) | `Cue::Bad` |
| `Win(W)`    | **mission réussie** (tout est rangé / bonne réponse) | **pluie de confettis** + écran « Bravo ! » + étoiles qui *pop* | `Cue::Win` |
| (auto) `Fail` | **mission échouée** (plus d'étoiles) | écran « On recommence ! » | `Cue::Fail` |

**Étoiles** : 3 visibles en permanence (HUD haut-droite). Stables si on réussit,
**une disparaît** (rétrécit + tombe + s'efface) à chaque erreur. À 0 → échec.

---

## 2. SONS à produire (`assets/sfx/`)

Sons **courts, doux**, jamais agressifs. Specs : WAV mono 44.1 kHz (cf. doc 05 §2).

| Fichier | Cue | Déclenché quand | Description du son |
|---------|-----|-----------------|--------------------|
| `sfx_tap.wav`     | Star | on touche/compte un objet | petit « pop » clair, léger |
| `sfx_bon.wav`     | Good | bon placement / bonne carte | **carillon montant joyeux** (clochette) |
| `sfx_mauvais.wav` | Bad  | mauvais placement / mauvaise carte | « boing » doux et gentil (PAS un buzzer) |
| `sfx_etoile.wav`  | —    | chaque étoile qui *pop* à l'écran Bravo | « ding » scintillant |
| `sfx_niveau.wav`  | Win  | mission réussie | **petite fanfare** joyeuse (1-2 s) |
| `sfx_echec.wav`   | Fail | plus d'étoiles | son **doux et encourageant** (descente douce, pas triste) |
| `sfx_pose.wav`    | —    | un fruit tombe dans le panier | « plop » mat |
| `sfx_clic.wav`    | —    | **Formes** : forme encastrée dans son trou | « clic » de bois satisfaisant (joué **avec** `Good`) |
| `sfx_carte.wav`   | —    | **Mémoire** : on retourne une carte | léger « swip » de carte (joué **avec** `Tap`) |
| `musique_fond.wav`| —    | musique d'ambiance (boucle) | douce, instrumentale, ~30-60 s (.ogg/.mp3 ok pour le poids) |

> Astuce : un même événement peut jouer **son + voix** (ex. bonne réponse =
> `sfx_bon` **+** voix `felicitation_bravo`). Le code joue les deux.

---

## 3. VOIX à produire (`assets/voice/`)

Le détail complet (texte exact + nom de fichier + ton) est dans
[05-audio-script.md](05-audio-script.md). Rappel du **mapping événement → voix** :

| Événement | Voix (fichier) |
|-----------|----------------|
| Entrée d'un jeu / consigne | `<jeu>_consigne.wav` (ex. `couleurs_consigne.wav`) |
| Bonne réponse (mission) | une de `felicitation_bravo` / `felicitation_super` / `felicitation_tu_es_fort` (aléatoire) |
| Mauvaise action | une de `encouragement_essaie` / `encouragement_presque` |
| Compter un objet | `nombre_1..10` (dit au fur et à mesure des taps) |
| Nommer une couleur / un fruit | `couleur_*` / `fruit_*` (mode vocabulaire) |
| Désigner l'animal cible (Animaux) | `animaux_consigne` + `animal_*` (« Touche le… » + nom) |
| Nommer une forme (Formes) | `forme_*` (mode vocabulaire) |
| Paire trouvée / ratée (Mémoire) | `memoire_paire` / `memoire_encore` |
| Mission réussie (étoiles) | `recompense_etoiles` |
| Mission échouée | (réutilise `encouragement_essaie`) |
| Niveau suivant | `transition_niveau` |

---

## 4. Mapping par jeu (récap)

### Les Couleurs
- prend un fruit → (rien) · le pose dans le **bon** panier → `Good` (+`fruit_*`/`couleur_*` en mode vocabulaire) · **mauvais** panier → `Bad` · tous rangés → `Win` · plus d'étoiles → `Fail`.

### Compter
- touche chaque objet → `Tap` (+ `nombre_k` au fil du comptage) · bonne carte-chiffre → `Good`+`Win` · mauvaise carte → `Bad` · plus d'étoiles → `Fail`.

### Calculs
- bonne carte-résultat → `Good`+`Win` · mauvaise carte → `Bad` · plus d'étoiles → `Fail`.

### Les Formes
- prend une forme → (rien, ou `forme_*` en vocabulaire) · l'encastre dans le **bon** trou → `Good` (+`sfx_clic` + `formes_bravo`) · **mauvais** trou → `Bad` (la forme revient en bas) · toutes placées → `Win` · plus d'étoiles → `Fail`.
- Consigne d'entrée : `formes_consigne`.

### Les Animaux
- entrée d'une manche → consigne `animaux_consigne` **+** `animal_<cible>` (« Touche le… **lion** ») · touche le **bon** animal → `Good` (+ option `cri_<animal>`), manche suivante ; dernière manche → `Win` · **mauvais** animal → `Bad` · plus d'étoiles → `Fail`.
- Les petits **points de progression** (sous le titre) montrent la manche courante.

### La Mémoire
- retourne une carte → `Tap` (+`sfx_carte`) · deux cartes **identiques** → `Good` (+`memoire_paire`, restent visibles) · deux cartes **différentes** → `Tap` doux (+`memoire_encore`), elles se recachent après ~0,9 s (**pas de perte d'étoile** — jeu volontairement doux) · toutes les paires trouvées → `Win`.
- Consigne d'entrée : `memoire_consigne`. Les étoiles restent **stables** (succès = 3 étoiles).

---

## 5. Effets visuels déjà en place (pour info)

- **Particules** (`MouFx`) : confettis (avec gravité + rotation), étincelles, puff (anneau).
- **Étoiles HUD** : visibles, perte animée (rétrécit/tombe/fond).
- **Écran victoire** : pluie de confettis + étoiles qui *pop* + « Bravo ! ».
- **Écran échec** : « On recommence ! » + « Tu vas y arriver ! ».
- **Fruit en main** : grossit légèrement + son nom affiché (vocabulaire).
- **Intro Rihen** (logo) + **Intro Noge** (hexagone animé) + **splash** mascotte qui rebondit.

### Idées pour rendre encore plus amusant (à venir, faciles)
- Le **fruit qui "tombe"** dans le panier avec un petit rebond (anim de chute).
- La **mascotte qui réagit** (saute / applaudit à la victoire, fait un clin d'œil à l'erreur).
- Léger **balancement (idle)** des objets en attente (vie à l'écran).
- **Panier qui tressaute** quand on y dépose un bon fruit.
- **Compteur visuel** qui s'incrémente en gros à chaque objet compté.

---

## 6. Ce qu'il te reste à fournir

1. Les **`.wav` voix** (doc 05, commence par la liste prioritaire).
2. Les **`.wav` sons** du tableau §2.
3. Optionnel : `musique_fond`.

Dès réception (même partielle), je **route les `Cue` vers NKAudio** (lecture des
sons + voix) — le câblage est minime car les signaux sont déjà émis par le jeu.
