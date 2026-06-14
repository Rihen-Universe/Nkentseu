# GDD 05 - Gardien Du Labyrinthe

## Resume

**Genre :** puzzle aventure top-down.  
**Plateformes :** Windows, Linux, Android, HarmonyOS, Web.  
**Public cible :** joueurs de puzzle/action lente, fans de labyrinthes.  
**Promesse :** se deplacer dans des salles, activer des interrupteurs, eviter des pieges et ouvrir la sortie.

Gardien Du Labyrinthe transforme la base `Nkoung` en jeu d'exploration logique. Le joueur controle un personnage ou un bloc gardien dans des niveaux compacts. Chaque salle est un probleme spatial.

## Objectifs De Design

- Sensation top-down immediate.
- Mouvement case par case ou fluide avec snap grille.
- Reflexion avant reflexes.
- Ajouter de petites histoires visuelles sans systeme narratif lourd.
- Niveaux lisibles sur mobile.

## Boucle De Jeu

1. Le joueur entre dans une salle.
2. Il observe portes, pieges, clefs et interrupteurs.
3. Il se deplace, pousse des blocs ou active des dalles.
4. Les elements changent l'etat de la salle.
5. La sortie s'ouvre quand les conditions sont remplies.
6. Le joueur passe a la salle suivante.

## Mecaniques

### Deplacement

- Mode recommande : case par case.
- Chaque input de direction deplace le joueur d'une case si libre.
- Mobile : tap sur case voisine ou joystick virtuel.

### Interrupteurs

- Dalle pression : activee tant que le joueur ou un bloc est dessus.
- Levier : reste active apres interaction.
- Interrupteur sequence : doit etre active dans un ordre.

### Portes

- Porte coloree : liee a un interrupteur ou une clef.
- Porte temporaire : se referme apres quelques tours.
- Porte a poids : demande deux dalles actives.

### Blocs

- Bloc poussable.
- Bloc lourd : ne peut etre pousse qu'une fois.
- Bloc fragile : casse apres mouvement.

### Pieges

- Pics intermittents.
- Dalles qui tombent apres passage.
- Rayon ou zone dangereuse.

## Conditions De Victoire

- Atteindre la sortie.
- Option : recuperer une relique.
- Option score : nombre minimal de mouvements.

## Controles

### Desktop

- Fleches/ZQSD/WASD : deplacement.
- `E` ou espace : interaction.
- `R` : reset salle.
- `Z` : annuler.

### Mobile

- Tap sur case adjacente.
- Swipe directionnel.
- Bouton reset dans safe area.
- Bouton interaction si necessaire.

## Structure Des Niveaux

### Zone 1 - Le Temple

- Deplacement, murs, sortie.
- 8 salles.

### Zone 2 - Les Dalles

- Interrupteurs et portes.
- 10 salles.

### Zone 3 - Les Blocs

- Pousser, bloquer, maintenir des dalles.
- 12 salles.

### Zone 4 - Les Pieges

- Timing lent, dangers et dalles fragiles.
- 12 salles.

## Direction Artistique

- Ambiance temple/ruines.
- Grille visible mais integree au sol.
- Personnage rectangulaire au depart, sprite plus tard.
- Couleurs fortes pour portes/interrupteurs.

## Audio

- Pas de mouvement.
- Son porte ouverte.
- Son piege.
- Petite boucle ambient.

## Technique

### Systeme De Monde

- Grille de tuiles.
- Entity simple : joueur, bloc, porte, piege.
- Etat de salle serialisable.
- Undo par snapshots legers ou historique d'actions.

### Simulation

- Le mouvement est valide par grille.
- Les interrupteurs recalculent les portes apres chaque action.
- Les pieges peuvent fonctionner en "tour" plutot qu'en temps reel pour garder le jeu calme.

### Modules

- `NKCanvas` : rendu salle, joueur, blocs.
- `NKEvent` : input clavier/touch.
- `NKUI` : pause, reset, selection de salles.

## MVP

- 16 salles.
- Mouvement grille.
- Portes, interrupteurs, blocs.
- Reset et undo.
- Sortie de niveau.

## Risques

- L'undo devient vite important pour le confort.
- Les pieges temps reel peuvent nuire au cote reflexion.
- Les niveaux demandent beaucoup de tests pour eviter les blocages impossibles.

## Temps De Realisation

Hypothese : un developpeur, graphismes simples, niveaux faits a la main.

- Prototype joueur + murs + sortie : **3 a 4 jours**
- Interrupteurs, portes, blocs : **1 a 2 semaines**
- Vertical slice 8 salles : **3 semaines**
- MVP 16 a 24 salles : **5 a 7 semaines**
- Version polish avec sprites/audio : **8 a 10 semaines**

## Priorite

Tres bon candidat si on veut prolonger directement le rectangle de la demo actuelle vers un vrai gameplay.
