# NkAnima — l'interface, pour un humain

### Document 8 — Spécification d'interface côté humain (produit / UX)

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen** — 26/09/2026.
> Ce document dit **quoi**, **où** et **pourquoi**. Le document **09** dit la même
> chose à l'agent qui écrit les `.nkgui`, élément par élément. Le document **07**
> dit ce que l'application doit savoir faire.
>
> **Si 08 et 09 se contredisent, c'est 08 qui gagne** : 09 n'en est que la
> traduction.

---

## 1. Le style : Unreal Engine 5, net

Décision de Rodolf : **le thème et le design sont dans l'esprit de l'éditeur
Unreal Engine 5.** Pas une copie : ses **qualités**.

| qualité UE5 | ce qu'on en fait dans NkAnima |
|---|---|
| **fond neutre très sombre**, sans teinte | les gris ne tirent ni vers le bleu ni vers le brun : la couleur est réservée à l'**information** |
| **hiérarchie par la valeur** (plus clair = plus avant) | fond d'application < fond de panneau < en-tête de catégorie < champ de saisie |
| **densité** : texte petit, lignes serrées, beaucoup d'information visible | lignes de propriété de 24 px, texte de 12 px, espacements de 4 à 8 px |
| **angles presque droits** | rayon de 2 px sur les champs et boutons, 0 sur les panneaux |
| **onglets fins** avec liseré d'accent sur l'onglet actif | l'onglet du panneau qui a le focus porte un liseré de 2 px en haut |
| **panneau Détails** : recherche en tête, catégories repliables, étiquette à gauche, valeur à droite, flèche « revenir au défaut » | c'est **le** modèle de tous les panneaux de propriétés de NkAnima |
| **champs X / Y / Z** marqués rouge / vert / bleu | repris tel quel : c'est de l'information (l'axe), pas de la décoration |
| **barre d'outils à icônes** avec liste déroulante de mode | le sélecteur d'**espace de travail** est à cet endroit |
| **tiroir de contenu** qui monte du bas (Ctrl+Espace) | la **Bibliothèque** fonctionne ainsi |
| **barre d'état** basse avec accès au journal et à la ligne de commande | idem |

### 1.1 Le thème : deux variantes, des **jetons**, jamais de couleurs en dur

Le thème existe en **Sombre** (par défaut) et en **Clair**. Aucune interface
n'écrit une couleur en dur : elle nomme un **jeton** (`@fond.panneau`,
`@info.ecrit`…) que le thème résout. Une couleur écrite en dur resterait sombre
en thème clair.

Valeurs de référence : **la palette imposée par Rihen pour NKCraft** (UI_SPEC
§10bis de NKCraft), devenue celle de la famille (NKCraft 02 §1.1) :

| jeton | rôle | Sombre | Clair |
|---|---|---|---|
| `@fond.app` | derrière tout | `#141414` | `#F5F5F5` |
| `@fond.panneau` | corps d'un panneau | `#212121` | `#FFFFFF` |
| `@fond.entete` | en-tête de catégorie, barres d'outils, onglets | `#2B2B2B` | `#EAEAEA` |
| `@fond.champ` | champ de saisie | `#0F0F0F` | `#FFFFFF` |
| `@fond.survol` | survol d'une ligne | `#333333` | `#E0E0E0` |
| `@bord` | séparations fines | `#303030` | `#D0D0D0` |
| `@texte` | texte courant | `#C8C8C8` | `#1A1A1A` |
| `@texte.faible` | étiquettes secondaires, grisé | `#7A7A7A` | `#8A8A8A` |
| `@accent` | **état de l'interface** : sélection de ligne, focus, onglet actif | `#1177D1` | `#0E5FA6` |
| `@selection.3d` | **sélection dans la vue 3D** | `#F2980E` | `#C97A08` |

> **Bleu = état de l'interface, ambre = sélection 3D.** Deux familles, pas deux
> nuances (règle de NKCraft, valable pour toute la famille).

### 1.2 Les couleurs qui **portent une information** (et elles seules)

| jeton | sens | couleur | pourquoi |
|---|---|---|---|
| `@info.ecrit` | **ce bouton écrit une clé** | orange Rihen `#F79A28` (⚠️ proposition : l'ambre unique `#F2980E`, NKCraft 02 §1.1) | empêcher un clic de trop ; déjà en usage dans la barre d'outils actuelle |
| `@info.enregistrement` | **clé automatique active** (chaque geste écrit une clé) | rouge | convention de tous les logiciels d'animation : « on enregistre » |
| `@axe.x` `@axe.y` `@axe.z` | l'axe d'un champ, d'un gizmo | rouge / vert / bleu | convention universelle |
| `@origine.main` | clé posée à la main | blanc cassé | la référence neutre |
| `@origine.physique` | clé produite par la physique | cyan | distinguer ce qui a été calculé |
| `@origine.trajectoire` | clé produite par une trajectoire | vert | idem |
| `@origine.captation` | clé venue d'une vidéo ou d'une mocap | jaune | idem |
| `@origine.ia` | clé produite par l'IA | violet | idem |
| `@origine.agent` | clé produite par un agent | magenta | idem |
| `@info.apercu` | **proposition non appliquée** (IA, physique, agent en aperçu) | violet translucide + hachures | ce qui n'est pas encore à vous doit se voir au premier coup d'œil |
| `@info.simulation` | la simulation tourne | vert | l'état change le sens de la vue |
| `@info.alerte` | à regarder | ambre | somme de poids ≠ 1, pied qui glisse |
| `@info.erreur` | cassé ou impossible | rouge vif | |

📌 **Jamais la couleur seule** : l'origine d'une clé porte aussi une **lettre**
(M, P, T, C, I, A) dans les pastilles et les infobulles, pour qui distingue mal
les couleurs.

---

## 2. L'écran d'accueil

Une fenêtre simple, avant l'atelier.

- **À gauche** : Récents · Nouveau · Ouvrir · Apprendre.
- **Au centre** : les projets récents en cartes (vignette animée au survol, nom,
  date, type Film / Jeu).
- **Nouveau projet** : trois grandes cartes — **Film**, **Jeu**, **Facile** — puis
  un nom et un dossier. Film et Jeu fixent les réglages par défaut (images par
  seconde, exports, part de physique cuite ou temps réel) ; Facile ouvre
  directement l'espace Facile (§6).

---

## 3. L'atelier : la coquille

```
┌────────────────────────────────────────────────────────────────────────────┐
│ Fichier Édition Sélection Vue Squelette Animation Physique Phénomènes IA  … │ barre de menus
├────────────────────────────────────────────────────────────────────────────┤
│ [Espace : Animation ▾] │ outils │ ▶ transport │ ● clé auto │ part physique │ barre d'outils
├───────────┬───────────────────────────────────────────────┬────────────────┤
│  Scène    │                                               │   Détails      │
│ (arbre)   │                 Vue 3D                        │  (recherche,   │
│           │   ┌ barre de vue : persp · éclairé · afficher┐│   catégories)  │
│           │                                               │                │
├───────────┴───────────────────────────────────────────────┴────────────────┤
│ Séquenceur :  [Feuille d'expo] [Courbes] [Couches] [Audio]                  │
│  pistes à gauche │ clés / courbes à droite, règle du temps en haut          │
├────────────────────────────────────────────────────────────────────────────┤
│ Bibliothèque (Ctrl+Espace) · Journal · > commande        fps · os · mémoire │ barre d'état
└────────────────────────────────────────────────────────────────────────────┘
```

- **Tout est un panneau à onglets, amarrable** (docking), comme dans UE5.
- **La Vue 3D est toujours au centre** et ne se ferme pas.
- Chaque **espace de travail** (§4) est une **disposition** de ces panneaux.
  L'utilisateur la modifie librement ; ses changements sont **mémorisés par
  espace**. « Fenêtre → Réinitialiser la disposition » la restaure.

### 3.1 La barre d'outils

De gauche à droite :

1. **Sélecteur d'espace de travail** (liste déroulante avec icône) — §4.
2. **Outils de la vue** : Sélectionner · Déplacer · Tourner · Mettre à l'échelle ·
   **Trajectoire** · Pinceau de poids (visible seulement dans l'espace
   Squelette). Un seul actif à la fois.
3. **Transport** : début · image précédente · **jouer/pause** · image suivante ·
   fin · boucle. L'image courante en champ numérique.
4. **Clés** : Insérer une clé (**orange**, il écrit), Supprimer, et l'interrupteur
   **Clé auto** (**rouge** quand actif).
5. **Part de physique** : un petit curseur 0–100 % qui s'applique à la sélection
   (§7.4), et le bouton **Simuler** (vert quand ça tourne).
6. À droite : **Assistant IA** (ouvre le panneau), **Aperçu en attente** (compteur
   des propositions non appliquées, violet), **Facile / Expert**.

### 3.2 La Vue 3D

- **Barre de vue** en surimpression haute, comme UE5 : caméra (perspective,
  dessus, face, côté, caméra de scène), mode d'affichage (solide, éclairé,
  filaire), **Afficher ▾** (liste des surimpressions), aimantation, vitesse de
  caméra.
- **Surimpressions** (« Afficher ▾ ») :
  - os et noms d'os ; contrôleurs ;
  - **trajectoires** (courbes de mouvement des éléments sélectionnés, avec les
    clés comme des points) ;
  - **peaux d'oignon** (images avant en bleu, après en vert, nombre et pas
    réglables) ;
  - **centre de masse** et polygone d'appui (vert si stable, ambre sinon) ;
  - **contacts au sol** (point sur chaque pied posé) ;
  - **forces** (vent, gravité, chocs) en flèches ;
  - formes de collision ; poids de peau en carte de chaleur.
- **Gizmos** : déplacement, rotation, échelle, avec les axes rouge/vert/bleu ;
  gizmo IK (poignée au bout de la chaîne + indicateur de coude) ; gizmo de
  trajectoire (poignées de la courbe).
- **HUD** discret en haut à gauche : image courante, nom de la sélection, espace
  de travail. En bas à gauche : axes.

### 3.3 Le panneau Scène

Arbre de tout ce qui est dans la scène : personnages (avec leur squelette
dépliable), objets, phénomènes, forces, agents, trajectoires, caméras de travail.

Colonnes, comme l'Outliner d'UE5 : **œil** (visible), **cadenas** (verrouillé),
**nom**, **type**, **origine** (pastille colorée : d'où viennent les clés de cet
élément). Recherche et filtre par type en tête.

### 3.4 Le panneau Détails

**Le modèle de tous les panneaux de propriétés.**

- En tête : **nom de la sélection**, icône du type, **recherche**.
- Des **catégories repliables** (en-tête plus clair, flèche, titre gras).
- Chaque **ligne** : étiquette à gauche (grisée si la valeur est par défaut),
  valeur à droite, **flèche jaune « revenir au défaut »** qui n'apparaît que si
  la valeur a été changée, et **losange de clé** : vide (pas animé), plein
  (clé sur cette image), demi (animé, pas de clé ici). Cliquer le losange pose ou
  retire une clé (**orange** au survol : il écrit).
- Séparateur déplaçable entre la colonne étiquettes et la colonne valeurs.
- Le contenu dépend de la sélection : os, objet, corps physique, émetteur, force,
  agent, trajectoire (voir chaque espace).

### 3.5 Le Séquenceur

En bas, quatre onglets qui montrent **les mêmes clés** de quatre façons.

| onglet | ce qu'il montre | gestes |
|---|---|---|
| **Feuille d'expo** | une ligne par canal, une clé = un losange coloré par **origine** | glisser pour décaler ; boîte de sélection ; G déplace, S étire, Maj+D duplique |
| **Courbes** | les courbes des canaux sélectionnés | poignées de tangente ; interpolation constante / linéaire / Bézier ; normaliser ; cadrer (F) |
| **Couches** | les clips et couches (base + additives), comme des blocs sur des pistes | déplacer, couper, mélanger, poids de couche, transitions |
| **Audio** | la forme d'onde de la piste de référence | caler, décaler ; repères de syllabes pour la synchronisation labiale |

Commun aux quatre : **règle du temps** en haut (images ou secondes), **tête de
lecture**, **plage de lecture** (bornes glissables), **zoom** horizontal et
vertical, **pistes** à gauche avec recherche, œil, cadenas, et une pastille
d'origine par piste.

### 3.6 La Bibliothèque (tiroir)

Monte du bas (Ctrl+Espace), comme le tiroir de contenu d'UE5. Dossiers à gauche,
**vignettes animées** à droite (le mouvement se joue au survol). Catégories :
Mouvements · Poses · Expressions · Effets · Squelettes · Comportements (agents).
**Glisser** un mouvement sur un personnage de la Vue ou sur une piste l'applique.

### 3.7 La barre d'état

À gauche : **Bibliothèque**, **Journal**, champ **> commande** (toute commande de
l'application s'y tape, avec complétion). À droite : images par seconde, nombre
d'os animés, mémoire, et l'état de la simulation (au repos / calcule / cuite).

---

## 4. Les espaces de travail

Un espace de travail = **une disposition** + **les outils utiles** à une tâche.
On change d'espace sans rien perdre : c'est la même scène.

| espace | pour | panneaux (disposition par défaut) |
|---|---|---|
| **Facile** | aller vite, sans vocabulaire | voir §6 — disposition propre |
| **Squelette** | créer le squelette, les contrôleurs, les poids | Vue · Arbre des os · Détails · Poids |
| **Animation** | animer | Vue · Scène · Détails · Séquenceur · Bibliothèque (tiroir) |
| **Physique** | corps physiques, dosage, simulation | Vue · Corps physiques · Détails · Séquenceur (courbe de part de physique) |
| **Phénomènes** | VFX : vent, particules, destruction, fluides | Vue · Pile d'effets · Détails · Séquenceur |
| **IA & Agents** | générer, distribuer des rôles, faire jouer | Vue · Assistant IA · Distribution · Texte de la scène · Séquenceur |
| **Captation** | vidéo → mouvement, mocap | Lecteur vidéo · Vue · Correspondance d'os · Séquenceur |

Chaque espace a un **liseré de 2 px** de sa couleur sous son nom dans le
sélecteur (repère discret, pas une décoration de panneau).

---

## 5. Les espaces, un par un

### 5.1 Squelette

- **Arbre des os** : hiérarchie, icône par type (os, contrôleur, point
  d'attache), œil, cadenas. Clic droit : ajouter un os enfant, point d'attache,
  contrainte, renommer, symétriser, reparenter.
- **Barre de l'espace** : **Poser un gabarit ▾** (bipède, quadrupède, oiseau,
  poisson, serpent, insecte, vide) · **Proposer par IA** (violet, en aperçu) ·
  Symétrie gauche/droite · Pose de référence · **Lier au maillage** · **Tester la
  déformation**.
- **Détails d'un os** : Transformation (X/Y/Z colorés), Longueur, Axes, Limites
  d'angle (petit cône visuel), Contrainte (type : IK deux os, chaîne IK, IK de
  colonne, visée, copie ; cible ; poids), Contrôleur (forme, taille, couleur de
  côté).
- **Panneau Poids** : os actif, pinceau (ajouter, retirer, lisser, égaliser),
  rayon, force, **somme au sommet** (ambre si ≠ 1), boutons **Poids
  automatiques** et **Normaliser**. La Vue passe en carte de chaleur.

### 5.2 Animation

La coquille de §3, avec en plus :

- **Mode pose** (Tab) : on manipule le personnage. **Pose assistée** activée par
  défaut : on tire une main, le corps suit. Maintenir **Alt** pour la couper et
  bouger l'os seul.
- **Outil Trajectoire** (§5.2.1).
- **Détails d'un os en pose** : Transformation, IK/FK (curseur de mélange),
  Espace (local / monde / parent), **Origine des clés** (lecture seule : pastilles).
- **Visage** : quand la sélection est la tête d'un personnage qui a un facial,
  les Détails montrent les **unités d'action** (sourcils, paupières, joues,
  lèvres, mâchoire) en curseurs animables, une grille d'**expressions**
  prêtes (joie, colère, peur, surprise, dégoût, tristesse) et le bouton
  **Depuis la voix** (synchronisation labiale à partir de la piste audio).

#### 5.2.1 L'outil Trajectoire

1. Choisir l'outil (barre d'outils, ou T).
2. **Tracer** dans la Vue : à main levée (glisser), ou point par point (clics),
   **Entrée** pour finir. La courbe **colle au sol** par défaut (bascule
   « libre dans l'espace »).
3. La courbe apparaît dans la Scène comme un élément ; ses **Détails** :
   - **Éléments attachés** (liste ; « Attacher la sélection » / « Détacher ») ;
   - **Orientation** : suivre la tangente · fixe · vers une cible ;
   - **Vitesse** : une petite courbe de vitesse le long du trajet (éditable en
     place), durée totale, image de départ ;
   - **Décalage** entre éléments (en distance ou en temps) ;
   - **Locomotion** (pour un corps à squelette) : aucune · marcher · courir ·
     ramper · … — les pas sont générés le long du trajet ;
   - **Physique en chemin** : **La courbe gagne** / **La physique gagne**, et la
     raideur du rappel vers la courbe.
4. Les clés produites sont **vertes** (origine trajectoire).

### 5.3 Physique

- **Corps physiques** : liste par élément (os ou objet), icône de forme, actif,
  type (**actif** : cherche la pose ; **passif** : laissé à la physique ;
  **cinématique** : suit l'animation, pousse les autres).
- **Barre de l'espace** : **Générer les corps** (à partir du squelette) ·
  **Corriger l'animation** (M4 correction, **en aperçu**) · **Simuler** / Arrêter ·
  **Cuire** (→ clés cyan) · gravité · pas de temps · graine (déterminisme).
- **Détails d'un corps** : forme et dimensions, masse, frottement, rebond,
  amortissement ; pour un corps actif : **force de maintien de pose** ; pour une
  liaison : limites (cône), rupture (seuil).
- **La part de physique dans le temps** : une piste du Séquenceur dessine la
  courbe 0–100 % par élément. On la dessine comme une courbe d'animation.

### 5.4 Phénomènes

- **Pile d'effets** : les phénomènes de la scène, chacun dépliable en modules
  (comme une pile d'émetteur) : **Vent**, **Particules**, **Granulaire** (sable),
  **Destruction**, **Fumée et feu**, **Eau / Océan**, **Tissu et cheveux**.
- **Ajouter ▾** propose chaque type ; chaque type a des **préréglages** (tempête
  de sable, brise, pluie fine, mur de briques, vitre, vague de plage…).
- **Détails** : les paramètres du module sélectionné ; **tout paramètre porte un
  losange de clé** — un phénomène s'anime comme un personnage (le vent forcit
  de l'image 200 à 260).
- **Déclencheurs** : « à l'image N », « au contact de… », « quand la force
  dépasse… ». Visibles comme des repères dans le Séquenceur.
- **Cuire** produit un cache ; un cache cuit est marqué et ne se recalcule pas
  tant qu'on ne le demande pas.

### 5.5 IA & Agents

**Assistant IA** (panneau, accessible depuis tous les espaces) :

- En haut : **ce sur quoi l'IA travaille** (la sélection, affichée en pastilles).
- Une zone de saisie : « Il se lève péniblement puis court vers la porte. »
- **Raccourcis d'intention** : Intervalles · Corriger · Style ▾ · Depuis la voix ·
  Squelette · Poids.
- Les **propositions** arrivent en cartes : aperçu animé, ce qui sera modifié
  (liste des canaux, nombre de clés), et trois boutons : **Aperçu** (joue dans
  la Vue, en violet translucide), **Appliquer** (écrit : **orange**),
  **Rejeter**.
- **Historique** des propositions, appliquées ou non.

**Distribution** (agents) :

- Un tableau : personnage · **rôle** (texte libre : « vieux pêcheur méfiant ») ·
  humeur de départ · objectif · **état** (au repos / joue / joué).
- **Texte de la scène** : dialogue et didascalies, avec le nom du personnage
  devant chaque réplique ; importable.
- **Répéter** (joue en aperçu, violet) · **Jouer et enregistrer** (écrit des
  clés magenta) · **Rejouer une réplique**.
- Pour le jeu : **Exporter le comportement**.

### 5.6 Captation

- **Lecteur vidéo** (fichier ou caméra) avec le **squelette détecté** en
  surimpression.
- **Correspondance d'os** : squelette détecté ↔ squelette du personnage,
  appariement automatique, corrigeable à la main.
- Réglages : lissage, **verrouiller les pieds**, **correction physique** après
  captation.
- **Importer** (→ clés jaunes). Même panneau pour un fichier mocap (BVH, FBX).

---

## 6. L'espace Facile, en détail

**Principe : moins de vocabulaire, pas moins de puissance.** Tout ce qui se fait
ici se fait avec les mêmes commandes que l'espace Expert.

```
┌──────────────────────────────────────────────────────────────────────────┐
│  NkAnima — Facile                                   [ Ouvrir en détail ] │
├───────────────┬──────────────────────────────────────────────────────────┤
│  QUI ?        │                                                          │
│  ┌────┐┌────┐ │                    Vue 3D                                │
│  │Kofi││Awa │ │            (grande, caméra automatique)                  │
│  └────┘└────┘ │                                                          │
│  + Ajouter    ├──────────────────────────────────────────────────────────┤
├───────────────┤  QUE FAIT-IL ?                                           │
│  RÉGLAGES     │  [Actions ▦] [Écrire ✎] [Tracer un chemin ⤳]            │
│  Vitesse  ──● │   ┌──────┐┌──────┐┌──────┐┌──────┐┌──────┐┌──────┐       │
│  Énergie  ─●─ │   │marche││course││saute ││assis ││salue ││tombe │  …    │
│  Poids    ──● │   └──────┘└──────┘└──────┘└──────┘└──────┘└──────┘       │
│  Humeur   ●── ├──────────────────────────────────────────────────────────┤
│  Réalisme ─●─ │  SÉQUENCE  ▶  [ marche ][ s'assoit ][  salue  ]  + │     │
└───────────────┴──────────────────────────────────────────────────────────┘
```

1. **Qui ?** Grandes cartes des personnages et objets de la scène ; **+ Ajouter**
   ouvre la bibliothèque (personnages prêts, ou importer).
2. **Que fait-il ?** Trois onglets :
   - **Actions** : grille illustrée (vignettes animées) ; un clic ajoute l'action
     **à la fin** de la séquence du personnage choisi ;
   - **Écrire** : une phrase, l'IA propose (en aperçu), on garde ou non ;
   - **Tracer un chemin** : on dessine au sol, le personnage le suit en marchant ;
     choisir « en courant », « en rampant » sous le tracé.
3. **Réglages** : cinq curseurs **parlants**, chacun avec ses deux mots aux
   bouts : Vitesse (lent ↔ rapide), Énergie (mou ↔ vif), Poids (léger ↔ lourd),
   Humeur (triste ↔ joyeux), Réalisme (dessin animé ↔ physique). Ils agissent sur
   le **bloc sélectionné** de la séquence, ou sur tout si rien n'est sélectionné.
4. **Séquence** : des **blocs** nommés (pas de clés visibles), un par action ;
   glisser pour réordonner, tirer le bord pour allonger, clic droit : dupliquer,
   supprimer, « rejouer avec d'autres réglages ». Un bloc **retouché à la main**
   en mode Expert porte une petite **main** et n'est plus régénéré.
5. **Lecture** en boucle automatique ; la caméra suit le personnage.
6. **Ouvrir en détail** bascule dans l'espace Animation **sur la même animation**.

Ce que Facile fait sans le dire : transitions entre blocs, pieds au sol,
évitement des objets, correction physique si Réalisme > 50 %, mouvement
secondaire.

---

## 7. Gestes et règles communes

### 7.1 Souris

| geste | effet |
|---|---|
| clic gauche | sélectionner |
| Maj + clic | ajouter à la sélection |
| Ctrl + clic | retirer de la sélection |
| clic droit (sans glisser) | menu contextuel |
| clic droit maintenu | caméra en vol (ZQSD / WASD pour avancer), comme UE5 |
| Alt + clic gauche | orbiter |
| Alt + clic milieu | panoramique |
| molette | zoom |
| double clic sur une piste | cadrer ses clés |

### 7.2 Clavier (essentiels)

| touche | effet |
|---|---|
| Espace | jouer / pause |
| ← → | image précédente / suivante |
| ↑ ↓ | clé précédente / suivante |
| Début / Fin | début / fin |
| I | insérer une clé |
| Suppr | supprimer la clé |
| Tab | entrer / sortir du mode pose |
| Q W E R | sélectionner / déplacer / tourner / échelle |
| T | outil Trajectoire |
| B | pinceau de poids (espace Squelette) |
| Ctrl+Entrée | enregistrer la pose en clé ; envoyer une demande à l'IA |
| Ctrl+S | enregistrer le projet |
| F | cadrer la sélection |
| A / Alt+A / Ctrl+I | tout / rien / inverser |
| Ctrl+Z / Ctrl+Y | annuler / refaire |
| Ctrl+Espace | Bibliothèque |
| Ctrl+K | assistant IA |
| F9 | ajuster la dernière opération |

### 7.3 Toujours vrai

- **Toute action est annulable** et apparaît dans l'**Historique**.
- **Une proposition non appliquée est violette et hachurée**, partout (Vue,
  Séquenceur, Scène).
- **Un bouton qui écrit une clé est orange** (au repos pour les boutons dédiés,
  au survol pour les losanges).
- **Une action indisponible est grisée avec sa raison en infobulle** (« il faut
  un squelette lié », « pas encore implémenté »).
- **Fermer un onglet ne supprime rien.**

### 7.4 La part de physique

Un élément animé a toujours une **part de physique** (0 % par défaut pour une
animation à la main). Elle se règle :
- vite, depuis la barre d'outils (s'applique à la sélection, à l'image courante) ;
- finement, comme une courbe dans le Séquenceur ;
- par os, dans les Détails de l'os.

---

## 8. Les menus

| menu | contenu |
|---|---|
| **Fichier** | Nouveau · Ouvrir · Récents ▸ · Enregistrer · Enregistrer sous · Importer ▸ (maillage, mocap, vidéo, audio) · Exporter ▸ (clip, squelette, FBX, glTF, effet) · Quitter |
| **Édition** | Annuler · Refaire · Historique · Copier · Coller · Dupliquer · Supprimer · Préférences |
| **Sélection** | Tout · Rien · Inverser · Os enfants · Même origine · Symétrique |
| **Vue** | Solide · Éclairé · Filaire · Afficher ▸ (surimpressions) · Peaux d'oignon · Caméras ▸ · Compteurs de rendu |
| **Squelette** | Poser un gabarit ▸ · Proposer par IA · Ajouter un os · Contrainte ▸ · Symétriser · Lier au maillage · Poids automatiques · Normaliser les poids · Tester la déformation |
| **Animation** | Jouer · Début · Fin · Image préc. / suiv. · Plage · Insérer / Supprimer une clé · Clé auto · Pose ▸ (éditer, enregistrer, quitter, miroir, IK) · Trajectoire · Locomotion ▸ · Reciblage · Cuire |
| **Physique** | Générer les corps · Corriger l'animation · Simuler · Cuire · Part de physique ▸ · Réglages du monde |
| **Phénomènes** | Ajouter ▸ (vent, particules, granulaire, destruction, fumée et feu, eau, océan, tissu) · Déclencheur · Cuire le cache |
| **IA** | Assistant · Intervalles · Corriger · Style ▸ · Texte → mouvement · Voix → visage · Distribution des rôles · Jouer la scène |
| **Fenêtre** | Espaces de travail ▸ · chaque panneau (coché s'il est ouvert) · Réinitialiser la disposition |
| **Aide** | Raccourcis · Documentation · Journal · Crédits et licences · À propos |
