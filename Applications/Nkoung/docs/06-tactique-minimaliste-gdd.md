# GDD 06 - Tactique Minimaliste

## Resume

**Genre :** tactique 2D au tour par tour.  
**Plateformes :** Windows, Linux, Android, HarmonyOS, Web.  
**Public cible :** joueurs de strategie tactique qui veulent des parties rapides.  
**Promesse :** 3 unites, petite grille, decisions fortes, aucune surcharge.

Tactique Minimaliste est une version compacte d'un jeu tactique. Chaque niveau est une petite situation : capturer un point, survivre, escorter, eliminer une cible. Le joueur gagne par placement et utilisation intelligente des capacites.

## Objectifs De Design

- Peu d'unites, beaucoup de clarte.
- Parties de 5 a 15 minutes.
- Grille 6x6 a 10x10.
- Chaque unite doit avoir un role evident.
- Les combats doivent etre previsibles, pas aleatoires dans le MVP.

## Boucle De Jeu

1. Le niveau presente un objectif.
2. Le joueur selectionne une unite.
3. Il choisit deplacement, attaque ou capacite.
4. Les ennemis jouent leur tour.
5. La partie continue jusqu'a victoire ou defaite.
6. Le joueur peut recommencer pour obtenir un meilleur rang.

## Unites Joueur

### Lanceur

- Portee : 2 cases.
- Degats moyens.
- Role : attaquer en securite.

### Bouclier

- Portee : 1 case.
- Beaucoup de vie.
- Capacite : proteger une unite voisine.
- Role : tenir une ligne.

### Rapide

- Deplacement : eleve.
- Degats faibles.
- Capacite : contourner ou capturer.
- Role : objectifs.

## Ennemis

### Garde

- Se rapproche et attaque.
- Simple a anticiper.

### Tireur

- Attaque a distance.
- Faible au contact.

### Sentinelle

- Ne bouge pas.
- Controle une zone.

## Systeme De Combat

MVP sans hasard :

- Chaque attaque a des degats fixes.
- Les points de vie sont petits.
- Le joueur voit les cases menacees.
- Une unite vaincue quitte la carte.

Version avancee :

- Armure.
- Effets de terrain.
- Capacites speciales rechargeables.

## Objectifs De Mission

- Eliminer tous les ennemis.
- Capturer une case.
- Survivre 5 tours.
- Escorter une unite.
- Sortir par une zone.

## Controles

### Desktop

- Clic unite : selection.
- Clic case : deplacement ou attaque.
- `Tab` : unite suivante.
- `Espace` : fin tour.
- `Echap` : annuler/pause.

### Mobile

- Tap unite.
- Tap action dans panneau bas.
- Tap case valide.
- Confirmation optionnelle pour attaque.
- Tous les boutons restent dans la safe area.

## Interface

- Cases de deplacement en bleu.
- Cases d'attaque en rouge.
- Cases menacees par ennemis en orange.
- Panneau unite : vie, action restante, capacite.
- Bouton fin de tour clair.

## Direction Artistique

- Style plateau tactique sobre.
- Unites sous forme de silhouettes lisibles.
- Couleurs de faction distinctes.
- Animations courtes : deplacement, impact, disparition.

## Audio

- Selection.
- Deplacement.
- Attaque.
- Fin de tour.
- Victoire/defaite.

## Technique

### Systeme Grille

- Cellule : terrain, obstacle, occupant.
- BFS pour deplacement.
- Calcul des zones d'attaque.
- Etat de tour par unite.

### IA

Pour le MVP :

- Si une unite joueur est attaquable, attaquer.
- Sinon avancer vers l'objectif ou la cible la plus proche.
- Eviter les obstacles par BFS.

### Modules

- `NKCanvas` : grille et unites.
- `NKUI` : HUD tactique.
- `NKEvent` : input.

## MVP

- 3 unites joueur.
- 3 types ennemis.
- 8 missions.
- IA simple.
- Conditions victoire/defaite.
- UI de tour.

## Risques

- L'IA et l'equilibrage peuvent augmenter le temps.
- Les feedbacks visuels sont essentiels pour eviter la confusion.
- Le design mobile demande des boutons tres clairs.

## Temps De Realisation

Hypothese : un developpeur, rendu simple, pas de reseau.

- Prototype grille + selection + deplacement : **4 a 6 jours**
- Combat + tours + ennemis simples : **2 a 3 semaines**
- Vertical slice 3 missions : **4 semaines**
- MVP 8 missions : **7 a 10 semaines**
- Version polish avec animation/audio : **10 a 14 semaines**

## Priorite

Projet plus ambitieux. Tres interessant techniquement, mais a faire apres un puzzle plus simple si l'objectif est de sortir vite un jeu.
