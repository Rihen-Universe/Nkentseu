# NkNgoma — l'interface, pour un humain

### Document 2 — Spécification d'interface côté humain

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen** — 27/09/2026.
> Ce que l'on voit et ce que l'on fait, écran par écran. Le produit est au **01**, la
> traduction pour l'agent qui écrit les `.nkgui` au **03**. **Si 02 et 03 se
> contredisent, 02 gagne.**

---

## 1. Le style : Unreal Engine 5, dense et net

NkNgoma est un **outil de production** qu'on garde ouvert des heures : dense comme FL
Studio, **net comme UE5** (la préférence de Rodolf pour la famille). Thème **Rihen
UE5**, Sombre par défaut, Clair disponible ; les thèmes par application (NKUIDesign
doc 2 §20) permettent d'en livrer d'autres.

### 1.1 Le thème — les jetons de la famille

Structure (trois gris, bleu d'état, ambre de sélection) : **exactement** ceux de la
famille (NKUIDesign doc 3 §2.2). `@accent` (bleu) = **état de l'interface** (bouton
actif, voie sélectionnée dans une liste) ; `@selection` (ambre) = **sélection dans la
zone de travail** (notes, clips, pas, nœuds).

### 1.2 Les couleurs d'information — et elles seules

| jeton | sens | Sombre | où **seulement** |
|---|---|---|---|
| `@tete.lecture` | position de lecture | `#F2F2F2` | trait vertical dans toutes les timelines, rangée courante du séquenceur |
| `@info.enregistrement` | on enregistre | `#F85149` | bouton ●, pistes armées, liseré de la barre de transport |
| `@son.ecretage` | le son sature | `#F85149` | voyants et zone haute des mesures |
| `@son.sonie_ok` · `@son.sonie_hors` | sonie dans / hors de la cible | `#3FB950` · `#D29922` | mesure de sonie, rapport |
| `@note.velocite` | vélocité d'une note | dégradé du gris au blanc (clair = fort) | remplissage des notes et des pas |
| `@origine.main` · `@origine.ia` · `@origine.transcription` | d'où vient une note ou un clip | blanc cassé · violet `#A371F7` · cyan `#2EC4D6` | liseré gauche d'une note, d'un clip, d'un pattern |
| `@info.apercu` | **proposition non appliquée** (IA) | violet translucide + **hachures** | notes, patterns, réglages proposés |
| `@echelle.hors` | note hors de la gamme choisie | grisé à 40 % | rangées du rouleau de piano |
| `@accordage.ecart` | l'instrument n'est pas au tempérament du projet | ambre `#D29922` | étiquette d'accordage, rangées du rouleau |
| `@repere.image` | repère venu de l'image (coupe, événement d'animation) | sarcelle `#0A545E` | marqueurs de la timeline du son à l'image |
| `@jeu.etat_actif` | état de jeu en cours de simulation | vert `#3FB950` | graphe des états |
| `@axe.x/y/z` | — | — | *non employés : pas de 3D ici* |

📌 **Les couleurs des canaux, patterns et clips sont des données de l'utilisateur**
(il les choisit dans une palette), **pas des jetons** : elles n'ont pas de sens fixe.
Elles sont **désaturées de 30 %** dans le thème Clair pour rester lisibles.

📌 **Jamais la couleur seule** : enregistrement = ● **et** rouge ; écrêtage = voyant
**et** chiffre ; origine = liseré **et** icône au survol.

---

## 2. L'accueil

```
┌───────────────────────────────────────────────────────────────────────────────┐
│ ▓▓ NkNgoma                                                     [—][□][x]      │
├───────────────┬───────────────────────────────────────────────────────────────┤
│ Projets       │  [+ Nouveau ▾]  [Ouvrir…]                                      │
│ Gabarits      │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │
│ Bibliothèque  │  │ ▶ onde   │ │ ▶ onde   │ │ ▶ onde   │ │ ▶ onde   │           │
│ Apprendre     │  │ Bikutsi  │ │ BO Kanda │ │ Jeu —    │ │ Podcast  │           │
│ Réglages audio│  │ 150 · 12/8│ │ 5.1 · 42'│ │ 3 états  │ │ ép. 4    │           │
│               │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │
│ ● Audio : WASAPI exclusif · 48 kHz · 128 · 6,8 ms mesurés                     │
└───────────────┴───────────────────────────────────────────────────────────────┘
```

- **Cartes de projets** : forme d'onde du rendu, nom, tempo · signature ou format ·
  durée ; **▶ écoute** au survol, sans ouvrir.
- **Nouveau ▾** : Chanson · Bande originale (vidéo de référence) · Son de séquence
  (session son) · Musique de jeu · Podcast / voix · depuis un **gabarit** (afrobeat,
  bikutsi, makossa, orchestral, ambiance de jeu).
- **La ligne audio** en bas dit **toujours** le périphérique, la fréquence, la taille
  de bloc et la **latence mesurée** (01 §5.3) ; clic → Réglages audio (§12).
- **Premier lancement** : un assistant en trois pas — périphérique de sortie,
  périphérique d'entrée et **test de latence**, clavier MIDI (facultatif).

---

## 3. La fenêtre

```
┌────┬─────────────────────────────────────────────────────────────────────────────────┐
│ ▓▓ │ Fichier Édition Ajouter Composer Son Mixer Jeu Outils IA Fenêtre Aide           │ 28
│ ▓▓ ├─────────────────────────────────────────────────────────────────────────────────┤
│ ▓▓ │ ■ ▶ ● ⟲ │ 012:3:07 │ 00:01:42:12 │ 150,00 │ 12/8 │ ♩ ▾ │ [Composer│Son│Mixer│Jeu] │ CPU 23 % · 6,8 ms │ 40
├────┴───────┬─────────────────────────────────────────────────────────┬──────────────┤
│BIBLIOTHÈQUE│                                                         │ DÉTAILS      │
│🔍 ______   │            ZONE DE L'ESPACE                              │ (canal, note,│
│▸ Instruments│    Composer : playlist + rack + rouleau                 │  clip, effet)│
│▸ Afrique   │    Son : vidéo + timeline + bruitages                    │              │
│▸ Échantil. │    Mixer : voies                                         │              │
│▸ Motifs    │    Jeu : états + couches                                 │              │
│▸ Bruitages │                                                         │              │
│▸ Projet    │                                                         │              │
├────────────┴─────────────────────────────────────────────────────────┴──────────────┤
│ Console · IA ◆2 · Sonie −14,2 LUFS · ▮▮▮▮▯ ‖ Prêt                                    │
└──────────────────────────────────────────────────────────────────────────────────────┘
```

- **Logo carré 56 px** sur les deux bandes du haut (règle de la famille).
- **Barre de transport** (40 px, sous le menu) : Arrêt, Lecture, **Enregistrer**,
  Boucle ; position en **mesure:temps:tic** **et** en **timecode** (clic sur l'un
  bascule l'affichage principal) ; **tempo** (glisser, saisir, **taper** au clic
  maintenu) ; **signature** ; **métronome** ▾ (pré-décompte) ; **sélecteur d'espace** ;
  **CPU** et **latence mesurée** (clic → Réglages audio).
- **Bibliothèque** à gauche (§10), **Détails** à droite (le panneau de la famille,
  engendré depuis la réflexion) : tout ce qui est sélectionné s'y règle.
- **Barre d'état** : Console, **IA** (nombre de propositions en attente),
  **sonie** du master, **mesure de sortie** (mini-VU), état.

---

## 4. L'espace Composer

### 4.1 La disposition

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ PLAYLIST                                   [✎ ✂ 🔇 ⤢] [Grille ▾] [Motif ▾]   │
│ ▾ Marqueurs  Intro │ Couplet 1 │ Refrain │ Couplet 2 │ Pont │ Refrain          │
│ Piste 1  [Rythme ████][Rythme ████][Rythme ████][Rythme ████]                │
│ Piste 2        [Balafon ▒▒▒▒▒▒][Balafon ▒▒▒▒▒▒]                              │
│ Piste 3  ~~~~~~~ voix.wav ~~~~~~~~~~~~~~~~~                                   │
│ Piste 4  ╱╲╱ automation : filtre ╱╲╱                                          │
├───────────────────────────────┬──────────────────────────────────────────────┤
│ RACK DE CANAUX   Motif : Rythme│ ROULEAU DE PIANO — Balafon (accordage Ewondo) │
│ ● ◻ Ngoma      ▮▯▯▮ ▮▯▮▯ ▮▯▯▮ │  lame 12 ─ ▬▬    ▬▬▬                         │
│ ● ◻ Nkul       ▯▮▯▯ ▯▮▯▮ ▯▮▯▯ │  lame 11 ─     ▬▬      ▬▬                    │
│ ● ◻ Hochet     ▮▮▮▮ ▮▮▮▮ ▮▮▮▮ │  lame 10 ─ ▬      ▬▬▬                        │
│ ● ◻ Balafon    ♪ (notes)       │  …                                           │
│ [+ Canal ▾]    6/8 · 12 pas    │  vélocité ▁▃▅▇▅▃  ·  articulation ● ○ ●        │
└───────────────────────────────┴──────────────────────────────────────────────┘
```

- **Trois vues, une seule fenêtre**, ancrables : **Playlist** (`F5`), **Rack de
  canaux** (`F6`), **Rouleau de piano** (`F7`). Chacune se **détache** en fenêtre
  flottante (l'habitude de FL Studio), mais la disposition par défaut est **ancrée** —
  on ne cherche pas une fenêtre perdue derrière une autre.
- **Le pattern courant** (sélecteur en haut du rack) est ce qu'on édite dans le rack et
  le rouleau ; la playlist montre où il est posé.

### 4.2 Le rack de canaux et les pas

- Une **ligne par canal** : témoin d'activité, sourdine, **nom**, **volume** et
  **panoramique** (petits potentiomètres), **rangée de pas**.
- **Clic** sur un pas = poser / retirer ; **glisser** = peindre ; **clic droit** = menu
  (vélocité, hauteur, décalage, probabilité, répétition) ; **Alt+glisser vertical** sur
  un pas = vélocité.
- Les pas sont **groupés par temps** selon la signature (en 12/8 : 4 groupes de 3) —
  l'œil lit la mesure.
- **Groove** ▾ par canal et par pattern (gabarits bikutsi, makossa…, 01 §6.4), avec
  l'**intensité** en %.
- Un canal à notes (mélodique) montre un **aperçu** de ses notes au lieu des pas ; clic
  → rouleau de piano.

### 4.3 Le rouleau de piano

- **Rangées** : les touches d'un piano **ou les lames / cordes de l'instrument** quand il
  a un accordage propre (§4.4). Gamme aimantée : les rangées hors gamme sont **grisées**
  (`@echelle.hors`).
- **Outils** (barre du haut) : Crayon `P`, Pinceau `B`, Sélection `E`, Coupe `C`,
  Glissando `S`, Sourdine `T`, Zoom `Z` ; **Accord** ▾ (poser un accord nommé),
  **Arpège** ▾, **Quantifier** (`Q`, avec force et groove), **Humaniser**.
- **Sous les notes** : vélocité (barres), **articulation** (pastilles), et une rangée
  d'**automation** au choix.
- **Pistes fantômes** : notes des autres canaux du pattern, en transparence.
- **Notes proposées par l'IA** : hachurées violettes, **Appliquer / Rejeter** en haut du
  rouleau (§11).

### 4.4 Accordage et gammes

- **Étiquette d'accordage** dans le titre du rouleau (« accordage Ewondo — mesuré le
  12/10 ») ; `@accordage.ecart` si différent du tempérament du projet.
- Clic → le **panneau d'accordage** : les lames avec leur écart en **cents**, la courbe de
  l'accordage, **Réaccorder** (vers le tempérament égal, vers un autre instrument), ou
  **Garder** (le choix est écrit, jamais silencieux — 01 §6.3).
- **Gamme** ▾ du projet ou du pattern : majeure, mineure, modes, **pentatoniques**,
  **gammes africaines** nommées, **gamme de l'instrument**.

### 4.5 La playlist

- Pistes libres ; clips de **pattern**, d'**audio**, d'**automation** ; couleur choisie.
- **Outils** : Crayon, Pinceau, Coupe, Sourdine, **Glisser-étirer** (étirement temporel
  d'un clip audio au tempo).
- **Marqueurs** de section, **changements de tempo et de signature** sur une rangée au
  sommet.
- **Figer** un pattern en audio (clic droit) / **Défiger**.

### 4.6 L'enregistrement

- **Armer** une piste (●) ; ● de la barre de transport lance le **pré-décompte**.
- **Prises** : chaque passage en boucle crée une prise **empilée** ; le **choix de prises**
  s'affiche sous la piste — on **peint** le meilleur morceau de chaque prise.
- La **latence compensée** est affichée à côté du ● : « compensé : 6,8 ms (mesuré) ».

---

## 5. Instruments et effets

### 5.1 La fenêtre d'un instrument

S'ouvre au double-clic sur un canal ; **ancrée** dans Détails ou **flottante**.

- **Échantillonneur** : zones (clavier × vélocité) en grille, formes d'onde,
  **alternances**, articulations, enveloppe, filtre, **accordage**, **fiche de
  provenance** (onglet : instrument, facteur, région, musicien, lieu, date, licence).
- **Synthé** : oscillateurs, filtre, enveloppes, LFO, matrice de modulation — en
  **catégories repliables** comme le panneau Détails de la famille ; **potentiomètres**
  (P29) et **faders** (P30).
- **Préréglages** ▾ en tête : charger, enregistrer, comparer A / B.

### 5.2 La chaîne d'effets

Dans Détails d'une voie : les **inserts** en liste réordonnable (glisser), chacun
activable, avec sa **latence** si elle n'est pas nulle ; ouvrir un effet déplie ses
réglages **en place**. **+ Effet** ▾ (catégories : dynamique, égalisation, espace,
modulation, distorsion, hauteur et temps, analyse, greffons).

---

## 6. L'espace Son à l'image

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ ┌──────────────── VIDÉO DE RÉFÉRENCE ─────────────────┐  [Session ▾] [⟲ Mettre │
│ │                                                     │   à jour les repères]  │
│ │                 (image courante)                    │  TC 01:00:12:08        │
│ └─────────────────────────────────────────────────────┘  24 i/s               │
├──────────────────────────────────────────────────────────────────────────────┤
│ IMAGE  ▕▯▯▯▯▯▯▯▯▯▯▯▯▯▯▯▯▯▯ vignettes ▯▯▯▯▯▯▯▯▯▯▯▯▯▯▕                             │
│ REPÈRES ◆pas  ◆pas  ◆impact        ◆porte   │ coupe │ ◆pas                    │
│ DIAL   ▕~~~~ réplique 12 ~~~~▕                                                │
│ FX 1        [pas_bois_03]  [pas_bois_07]        [impact_lourd]                 │
│ FX 2                         [whoosh_court]                                     │
│ AMB    ~~~~~~~~~~~~~~ forêt_nuit ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~   │
│ MUS    ~~~~~~~~~~~~~~ thème_A (patterns) ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~   │
└──────────────────────────────────────────────────────────────────────────────┘
```

- **La vidéo** en haut (redimensionnable, détachable sur un second écran) ; le
  **timecode** et la cadence ; **Session** ▾ : importer / exporter une session son,
  importer une vidéo et une liste de repères.
- **Pistes par défaut** : **DIAL** (dialogues), **FX** (bruitages), **AMB** (ambiances),
  **MUS** (musique) — les futurs **stems** (§7.3).
- **Repères** (`@repere.image`) : glisser un bruitage **sur** un repère le **cale** à son
  attaque ; le clip calé porte une petite **ancre**. **Mettre à jour les repères** (après
  un changement d'animation) montre la **liste de ce qui a bougé** avant d'appliquer.
- **Navigation à l'image** : `,` / `.` image précédente / suivante ; `Maj+,` / `Maj+.`
  repère précédent / suivant.

### 6.1 Le design sonore

Double-clic sur un bruitage → la fenêtre **Couches** : les couches (sub, claquement,
débris, queue) empilées, chacune avec son son, son décalage, son volume, son
égalisation ; **Variations** (nombre, dispersion de hauteur, de timbre) ; **Exporter** en
un son ou en couches.

**Synthèse procédurale** (Ajouter ▸ Effet procédural) : Vent, Pluie, Feu, Eau, Souffle,
Whoosh, Impact, Magie, Interface, Moteur — chacun avec **ses paramètres** (vitesse,
taille, matière…), **animables** par une courbe ou calables sur des repères.

### 6.2 L'enregistrement de voix à l'image (ADR)

Sélectionner une réplique (ou un passage) → **Boucle ADR** : la vidéo boucle sur le
passage, **trois bips** de décompte, prises empilées, choix de prise comme au §4.6 ; la
**réplique originale** (piste guide) peut être écoutée au casque seulement.

---

## 7. L'espace Mixer

```
┌─────┬─────┬─────┬─────┬─────┬───── … ─┬─────┬─────┬──────┐
│Ngoma│Nkul │Bala.│Voix │ DIAL│         │ Rév │ Dél │MASTER│
│[EQ] │[EQ] │[EQ] │[Deb]│     │         │     │     │[Lim] │
│[Cmp]│     │[Rév]│[Cmp]│     │         │     │     │      │
│ ◯pan│ ◯   │ ◯   │ ◯   │ ◯   │         │     │     │      │
│ M S●│ M S●│ M S●│ M S●│ M S │         │ M S │ M S │ M S  │
│ ▮▮▮ │ ▮▮  │ ▮▮▮▮│ ▮▮▮ │ ▮▮  │         │ ▮   │ ▮   │ ▮▮▮▮ │
│ ▯▯▯ │ ▯▯▯ │ ▯▯  │ ▯▯▯ │ ▯▯▯ │         │ ▯▯▯ │ ▯▯▯ │ ▯▯   │
│-6,0 │-9,2 │-3,1 │-4,4 │ 0,0 │         │-12  │-18  │ -0,3 │
│→ Bus│→ Bus│→ Mst│→ Mst│→ Mst│         │→ Mst│→ Mst│ →sortie│
└─────┴─────┴─────┴─────┴─────┴───── … ─┴─────┴─────┴──────┘
```

- **Voies** de gauche à droite : canaux / pistes, puis **bus**, puis **master** (tranche
  `TrancheMixeur` de la famille, **fader vertical** P30 et **mesure** P14).
- Chaque voie : **inserts** (cases cliquables), **départs** (potentiomètres vers les bus
  d'effets), panoramique (**joystick surround** en 5.1 / 7.1), **M** sourdine, **S** solo,
  **●** armer, fader, mesure, valeur en dB, **sortie** ▾ (routage).
- **Automation** : mode par voie (Lecture · Écriture · Accroche · Toucher · Arrêt) ;
  les voies automatisées montrent un petit **A**.
- **Groupes** (VCA) : une couleur de bord commune + un fader de groupe.
- **Instantanés** A / B / C / D en haut à droite ; **comparer** à sonie égale.
- **Mesures du master** (panneau à droite, repliable) : **sonie** intégrée / court terme /
  momentanée, **true peak**, corrélation, spectre ; cible ▾ (R128 −23, streaming −14,
  perso) avec `@son.sonie_ok` / `@son.sonie_hors`.

---

## 8. L'espace Jeu

```
┌──────────────────────────────┬───────────────────────────────────────────────┐
│ ÉTATS                        │ COUCHES — état « Combat »                     │
│  (Exploration)──mesure──▶(Combat)│  Perc.  [▮▮▮▮]  intensité > 0,3            │
│       ▲   \               │   │  Basse  [▮▮▮▮]  toujours                      │
│  fondu│    \roulement tam-tam  │  Cuivres[▮▮▮▮]  intensité > 0,7            │
│       │     ▼             ▼   │  Balafon[▮▮▮▮]  santé < 0,3                  │
│   (Tension)────────▶(Victoire)│                                              │
├──────────────────────────────┴───────────────────────────────────────────────┤
│ SIMULATION  intensité ━━━━●━━━ 0,62   santé ━━●━━━━━ 0,25   lieu [Forêt ▾]    │
│  [▶ Jouer] [Événement : Objet ramassé ▾ ⚡] [● Enregistrer un parcours] état : Combat │
└──────────────────────────────────────────────────────────────────────────────┘
```

- **Graphe des états** (à gauche) : états en boîtes, **transitions** en flèches étiquetées
  (quand : temps, mesure, phrase ; comment : direct, fondu, **segment de transition**).
  L'état joué en simulation est en `@jeu.etat_actif`.
- **Grille des couches** (à droite) pour l'état choisi : une couche par ligne, ses
  **segments**, sa **règle** (« intensité > 0,3 »), son fondu.
- **Stingers** : liste sous la grille (événement → motif → calage : au temps, à la mesure).
- **Simulation** (bas) : un curseur par **paramètre**, un bouton par **événement**,
  **▶ Jouer** ; **● Enregistrer un parcours** crée un parcours rejouable (on le relance
  après avoir changé la musique pour vérifier qu'aucune transition ne tombe mal).
- **Exporter vers Nogee** (bouton en haut à droite) : le fichier de musique interactive.

---

## 9. Master et export

**Fichier ▸ Exporter…** (`Ctrl+R`, la touche de FL Studio) ouvre une boîte **en une
page** :

| section | contenu |
|---|---|
| **Quoi** | Morceau entier · Sélection · Boucle · **Stems** (D / M / E, ou par bus) · **Pistes séparées** · **Musique interactive** (Jeu) · **Banque de sons** (variations) |
| **Format** | WAV 16 / 24 / 32 flottant · FLAC · OGG · Opus · MP3 (les encodeurs non écrits sont **grisés avec leur raison**) ; fréquence ; canaux (stéréo, 5.1, 7.1, binaural) |
| **Sonie** | cible (R128, streaming, perso) ; **mesure** du rendu ; « ajuster à la cible » (gain seul, jamais une compression cachée) |
| **Vers** | Fichier · **NKScena** (stems + session) · **Nogee** · **NkAntenne** (jingle, fond) · **NkAnima** |
| **Rapport** | sonie mesurée, true peak, crédits et licences des éléments tiers |

Une phrase en bas dit **ce qui sera écrit** : « 4 fichiers WAV 24 bits 48 kHz, 5.1, 42 min
12 s, sonie −23,0 LUFS mesurée — dans Rendus/BO_Kanda/ ».

---

## 10. La bibliothèque

- **Arbre** à gauche : Instruments · **Afrique** (en tête, pas en dernier) · Électronique
  · Orchestral · Échantillons · Boucles · Motifs · Bruitages (UCS) · Réponses
  impulsionnelles · Accordages · Préréglages · Projet · Greffons.
- **Recherche** en haut : mot, étiquette ; filtres **tempo compatible**, **tonalité
  compatible**, **licence** ; **« Semblables »** (clic droit sur un son).
- **Aperçu** : clic = écoute **au tempo et dans la tonalité** du projet ; la forme d'onde
  défile ; **glisser** vers le rack, la playlist, la timeline, une zone d'échantillonneur.
- **Fiche** (survol prolongé ou panneau Détails) : provenance, licence, tempo, tonalité,
  accordage.
- Un élément **sans fiche** n'apparaît pas ; un élément **sans licence de
  redistribution** porte un cadenas et ne peut pas partir dans un export de banque.

---

## 11. L'IA

- **Panneau Assistant** (commun à la famille, rail droit ou `Ctrl+Maj+K`) : un champ,
  des **actions rapides** selon l'espace (Composer : « continuer », « harmoniser »,
  « basse », « variantes », « motif… » ; Son : « bruitage… », « semblables » ; Mixer :
  « conseils de mix » ; Jeu : « couches pour cet état »), et la **bande du modèle**
  (« local · NkNgoma-Notes 120 M · rien ne quitte cette machine »).
- **Une proposition** apparaît **là où elle s'appliquera** : notes hachurées violettes
  dans le rouleau, patterns hachurés dans la playlist, réglages en violet dans le
  mixeur ; une **carte** dans le panneau liste les changements (décochables), le
  **pourquoi**, et **Appliquer** / **Rejeter** ; appliquer = **une** opération annulable.
- **Variantes** : trois propositions côte à côte, chacune écoutable (▶).
- **Transcription** (clic droit sur un clip audio ▸ Transcrire en notes) et **Séparation**
  (▸ Séparer : voix, percussions, basse, reste) créent de **nouveaux canaux** marqués
  `@origine.transcription` / `@origine.ia`.

---

## 12. Réglages audio et MIDI

| rubrique | contenu |
|---|---|
| **Sortie / Entrée** | pilote (WASAPI partagé / exclusif, CoreAudio, ALSA, JACK / PipeWire ; ASIO grisé avec sa raison tant que D4 n'est pas tranché), périphérique, fréquence, **taille de bloc** |
| **Latence** | **Mesurer** (test de boucle, instructions en une phrase), résultat en ms, compensation appliquée |
| **MIDI** | périphériques d'entrée / sortie détectés, activés ; **apprendre** (bouger une commande pour l'associer à un paramètre) |
| **Métronome** | son, volume, pré-décompte |
| **Performance** | nombre de cœurs pour le graphe, rendu anticipé des pistes figées |
| **Bibliothèque** | dossiers, analyse (tempo, tonalité), index |
| **IA** | local / distant, modèles installés, accélération GPU |

---

## 13. Raccourcis

Ceux de FL Studio là où ils existent (le public vient de là), ceux de la famille sinon.

| touche | effet |
|---|---|
| `Espace` | lecture / arrêt |
| `Ctrl+Espace` · `R` | lecture depuis la position · enregistrer |
| `L` | basculer pattern / morceau (la lecture joue le pattern seul ou la playlist) |
| `F5` · `F6` · `F7` · `F8` · `F9` · `F10` | playlist · rack · rouleau de piano · Son à l'image · Mixer · Jeu |
| `Ctrl+R` | exporter |
| `P` `B` `E` `C` `S` `T` `Z` | crayon · pinceau · sélection · coupe · glissando · sourdine · zoom (rouleau, playlist) |
| `Q` · `Alt+Q` | quantifier · quantifier (réglages) |
| `Ctrl+↑/↓` · `Maj+↑/↓` | octave · demi-ton (notes sélectionnées) |
| `,` · `.` · `Maj+,` · `Maj+.` | image précédente / suivante · repère précédent / suivant |
| `Ctrl+Z` · `Ctrl+Y` | annuler · rétablir |
| `Ctrl+D` | dupliquer |
| `Ctrl+Maj+K` | assistant IA |
| `F1` · `F2` · `Ctrl+,` | aide · renommer · réglages |

⚠️ **F-touches** : `F5` est la playlist ici (convention FL Studio), pas « Simuler »
(NKUIDesign) ni « Rafraîchir ». Chaque application de la famille a **sa** table ; ce qui
est commun (Ctrl+Z, Ctrl+S, Ctrl+Maj+K, F1, F2) l'est partout.

---

## 14. Toujours vrai

- **La latence et la sonie s'affichent mesurées**, jamais supposées.
- **Ce qui vient de l'IA se voit** (violet, hachures) jusqu'à ce qu'on l'accepte ou le
  retouche.
- **L'accordage d'un instrument se voit** et ne se corrige jamais en silence.
- **L'Afrique est en tête** de la bibliothèque et des gabarits.
- **Rien n'est destructif** : figer se défige, une prise non choisie se garde.
- **On voit toujours si l'on enregistre** (● rouge, liseré de la barre de transport).
- **Un export dit ce qu'il va écrire** avant de l'écrire.
