# GDD 03 - Territoires

## Resume

**Genre :** strategie 2D au tour par tour.  
**Plateformes :** Windows, Linux, Android, HarmonyOS, Web.  
**Public cible :** joueurs de strategie legere, fans de jeux de grille.  
**Promesse :** capturer des cases, bloquer l'adversaire et controler la carte avec peu d'unites.

Territoires est un jeu de conquete minimaliste. Deux camps s'affrontent sur une grille. Chaque tour, le joueur deplace une unite ou capture une case. La victoire vient du controle spatial plus que de la force brute.

## Objectifs De Design

- Strategie lisible en petit format.
- Parties rapides : 5 a 12 minutes.
- Regles assez simples pour mobile.
- Profondeur par placement, blocage et anticipation.
- IA basique possible des la premiere version.

## Boucle De Jeu

1. La carte est generee ou chargee depuis un niveau.
2. Le joueur choisit une unite.
3. Il visualise les cases atteignables.
4. Il deplace l'unite, capture une case ou utilise une action.
5. Le tour passe a l'adversaire.
6. La partie se termine quand une condition de victoire est atteinte.

## Conditions De Victoire

Plusieurs modes sont possibles :

- **Domination :** controler 60% des cases.
- **Roi de la colline :** garder une zone centrale pendant 3 tours.
- **Capture :** prendre la base adverse.
- **Survie :** tenir un nombre fixe de tours.

Pour un MVP, le mode Domination est le plus simple.

## Unites

### Eclaireur

- Deplacement : 3 cases.
- Capture : faible.
- Role : explorer, bloquer, contourner.

### Gardien

- Deplacement : 1 case.
- Capture : forte.
- Role : tenir une position.

### Messager

- Deplacement : 2 cases.
- Effet : donne un bonus aux cases voisines.
- Role : support.

### Chef

- Deplacement : 2 cases.
- Si le chef est capture, l'equipe perd un bonus.
- Role : unite strategique centrale.

## Cases

### Neutre

- Aucun proprietaire.
- Peut etre capturee en entrant dessus.

### Controlee

- Donne un point de domination au proprietaire.
- Peut ralentir l'adversaire.

### Foret

- Cout de deplacement plus eleve.
- Cache partiellement une unite dans les modes avances.

### Village

- Donne un bonus ou genere une unite tous les X tours.

### Eau

- Bloque le passage sauf pont.

## Systeme De Tour

- Le joueur a un nombre limite d'actions par tour.
- Version simple : une action par unite.
- Version plus tactique : points d'action communs.
- Les cases capturees changent de couleur immediatement.

## IA

### Niveau 1

- Choisit la case neutre la plus proche.
- Evite les obstacles.
- Attaque si une capture facile est disponible.

### Niveau 2

- Priorise les zones de victoire.
- Defend sa base.
- Essaie de bloquer les chemins.

### Niveau 3

- Evalue le score potentiel a 2 tours.
- Peut sacrifier une unite pour gagner du terrain.

## Controles

### Desktop

- Clic : selectionner unite ou case.
- Clic droit : annuler selection.
- `Espace` : finir le tour.
- `Tab` : unite suivante.

### Mobile

- Tap sur unite.
- Tap sur case valide.
- Bouton "Fin Tour" en bas dans la safe area.
- Indicateurs visuels pour eviter les erreurs de tap.

## Interface

- Barre de score compacte.
- Couleur de territoire visible mais pas agressive.
- Surbrillance des cases atteignables.
- Petit panneau action : deplacer, attendre, annuler.
- Historique court des derniers tours dans un panneau optionnel.

## Direction Artistique

- Vue top-down grille.
- Couleurs calmes : terre, vert, bleu, rouge doux.
- Unites sous forme de symboles simples au depart.
- Les territoires peuvent etre dessines comme aplats transparents.

## Audio

- Son de selection.
- Son de capture.
- Son de fin de tour.
- Musique legere optionnelle.

## Technique

### Modules

- `NKCanvas` : rendu grille, territoires, unites.
- `NKEvent` : selection, clavier, touch.
- `NKUI` : panneau de tour, boutons, score.
- `NKMemory` : utile plus tard pour profiler si IA ou generation.

### Systeme Grille

- Grille 8x8 pour le prototype.
- Chaque cellule stocke terrain, proprietaire, unite.
- Pathfinding simple BFS pour les cases accessibles.
- IA basee sur score d'action.

### Donnees

Un niveau contient :

```text
width, height
terrain grid
starting units
objectives
turn limit
ai profile
```

## MVP

- Carte 8x8.
- Deux types d'unites : Eclaireur et Gardien.
- Mode Domination.
- IA niveau 1.
- 6 cartes.
- UI fin de tour et score.

## Risques

- L'IA peut prendre plus de temps que le rendu.
- Le jeu peut devenir lent si les tours ne sont pas fluides.
- L'equilibrage demande des tests.
- Les controles mobiles doivent eviter les actions accidentelles.

## Temps De Realisation

Hypothese : un developpeur, prototype NKCanvas, assets simples.

- Prototype local joueur contre joueur : **5 a 7 jours**
- IA simple + 3 cartes : **2 a 3 semaines**
- Vertical slice avec UI propre : **3 a 4 semaines**
- MVP 6 cartes + progression : **6 a 8 semaines**
- Version polish avec meilleure IA/audio : **9 a 12 semaines**

## Priorite

Bon candidat si l'objectif est d'apprendre l'IA, la grille tactique et l'UI. Plus long que Laser Puzzle.
