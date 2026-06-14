# GDD 04 - Ponts Et Chemins

## Resume

**Genre :** puzzle de tuiles 2D.  
**Plateformes :** Windows, Linux, Android, HarmonyOS, Web.  
**Public cible :** casual puzzle, joueurs mobile, enfants et adultes.  
**Promesse :** placer et tourner des tuiles pour connecter des villages sans couper les routes essentielles.

Ponts Et Chemins est un jeu de connexion. Le joueur recoit un ensemble limite de tuiles route, pont, virage ou croisement. Il doit relier des points de la carte avec le minimum de mouvements.

## Objectifs De Design

- Regles tres accessibles.
- Feedback immediat sur les connexions.
- Niveaux courts et relaxants.
- Progression par nouvelles contraintes : ponts, rivieres, routes bloquees.
- Parfait pour tactile.

## Boucle De Jeu

1. Le niveau affiche une carte avec points de depart et arrivees.
2. Le joueur choisit une tuile dans une reserve.
3. Il place ou tourne la tuile sur la grille.
4. Le jeu recalcule les connexions.
5. Le niveau est gagne quand toutes les connexions demandees sont valides.
6. Le score depend du nombre de tuiles, coups et temps optionnel.

## Mecaniques

### Tuiles Route

- Ligne droite.
- Virage.
- T.
- Croisement.

Chaque tuile peut avoir 4 orientations.

### Pont

- Permet de traverser une riviere.
- Peut etre horizontal ou vertical.
- Ressource limitee.

### Riviere

- Bloque les routes classiques.
- Force l'utilisation de ponts.

### Village

- Point a connecter.
- Peut demander une couleur ou un type de route.

### Route Sacree

- Case deja posee.
- Ne peut pas etre modifiee.
- Sert a creer des contraintes.

### Case Fragile

- Accepte une seule modification.
- Encourage la planification.

## Conditions De Victoire

- Tous les villages requis sont connectes.
- Les routes ne sortent pas de la grille.
- Les routes ne finissent pas sur une impasse interdite.
- Option avancee : certaines couleurs ne doivent pas se croiser.

## Controles

### Desktop

- Clic sur reserve : prendre une tuile.
- Clic sur grille : poser.
- `R` ou molette : tourner.
- Clic droit : annuler selection.
- `Z` : annuler.

### Mobile

- Tap sur reserve.
- Tap sur grille.
- Bouton rotation proche du pouce.
- Drag-and-drop optionnel.
- Barre basse placee dans la safe area.

## Structure Des Niveaux

### Chapitre 1 - Routes

- Tuiles simples.
- Objectif : connecter deux points.
- 12 niveaux.

### Chapitre 2 - Rivieres

- Introduction des ponts.
- 12 niveaux.

### Chapitre 3 - Multiples Villages

- Plusieurs connexions a gerer.
- 15 niveaux.

### Chapitre 4 - Contraintes

- Routes fixes, cases fragiles, tuiles limitees.
- 15 niveaux.

## Direction Artistique

- Carte top-down douce.
- Villages stylises par cercles ou petites maisons.
- Routes lisibles en couleur claire.
- Riviere bleue simple avec animation optionnelle.

## Audio

- Son de tuile posee.
- Son de rotation.
- Son de connexion complete.
- Son leger de riviere en ambiance optionnelle.

## Technique

### Representation Des Tuiles

Chaque tuile peut etre encodee par quatre connexions :

```text
north, east, south, west
```

Une rotation decale ces connexions.

### Verification

- Construire un graphe depuis les tuiles.
- Faire BFS/DFS depuis les points de depart.
- Verifier que chaque destination est atteinte.
- Detecter les sorties invalides vers cases vides.

### Modules

- `NKCanvas` : rendu de grille et tuiles.
- `NKEvent` : input.
- `NKUI` : reserve de tuiles, bouton reset, selection niveau.

## MVP

- 24 niveaux.
- Tuiles droite, virage, T, pont.
- Reserve limitee.
- Validation automatique.
- Undo/reset.
- Selection de niveau simple.

## Risques

- La generation automatique de niveaux est difficile si on veut garantir une solution interessante.
- Les tuiles doivent etre lisibles sur petits ecrans.
- Le jeu peut sembler trop facile sans contraintes de tuiles.

## Temps De Realisation

Hypothese : un developpeur, rendu simple, niveaux faits a la main.

- Prototype jouable : **3 a 5 jours**
- Vertical slice 10 niveaux : **2 semaines**
- MVP 24 niveaux : **4 a 5 semaines**
- Version polish 50 niveaux + audio : **6 a 8 semaines**

## Priorite

Excellent candidat mobile/web. Plus simple que Territoires, moins technique que Laser Puzzle avec rayons multiples.
