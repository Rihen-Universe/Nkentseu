# GDD 02 - Laser Puzzle

## Resume

**Genre :** puzzle 2D top-down, reflexion logique.  
**Plateformes :** Windows, Linux, Android, HarmonyOS, Web.  
**Public cible :** joueurs casual et puzzle, sessions de 2 a 10 minutes.  
**Promesse :** orienter des miroirs, prismes et portes pour guider un rayon lumineux vers une cible.

Laser Puzzle est un jeu de grille ou le joueur manipule des objets optiques. Le niveau est gagne quand le rayon atteint tous les recepteurs actifs sans toucher les obstacles interdits.

## Objectifs De Design

- Regles simples a comprendre en moins d'une minute.
- Niveaux courts mais de difficulte progressive.
- Aucune execution rapide requise : le plaisir vient de l'anticipation.
- Interface tactile naturelle : tap pour selectionner, bouton/gesture pour tourner.
- Visuel clair : le chemin du rayon doit toujours etre lisible.

## Boucle De Jeu

1. Le niveau affiche une source laser, des pieces manipulables et une ou plusieurs cibles.
2. Le joueur selectionne une piece.
3. Il la tourne, la deplace si le niveau l'autorise, ou change son etat.
4. Le rayon est recalcule en temps reel.
5. Si toutes les conditions sont vraies, le niveau est valide.
6. Le joueur passe au niveau suivant ou tente de reduire son nombre de coups.

## Mecaniques

### Source Laser

- Emet un rayon dans une direction fixe.
- Peut etre activee/desactivee par interrupteur dans certains niveaux.
- Variante avancee : plusieurs couleurs de laser.

### Miroir

- Renvoie le rayon selon un angle de 90 degres.
- Peut avoir deux orientations principales : slash et backslash.
- Action principale : rotation.

### Prisme

- Separe un rayon en deux directions.
- Sert a activer plusieurs cibles.
- Introduit apres les premiers niveaux.

### Recepteur

- Condition de victoire quand il est touche.
- Peut demander une couleur specifique.
- Peut etre temporaire : doit rester alimente pendant toute la verification.

### Mur Et Bloc Absorbant

- Mur : bloque le rayon.
- Bloc absorbant : absorbe le rayon mais ne fait pas perdre.
- Obstacle interdit : si le rayon le touche, le niveau echoue.

### Interrupteur

- Active une porte, une source secondaire ou une plateforme.
- Peut etre declenche par le rayon ou par une piece poussee.

## Controles

### Desktop

- Clic gauche : selectionner une piece.
- Clic gauche sur case vide : deplacer la piece selectionnee si autorise.
- Molette ou `R` : tourner la piece.
- `Echap` : pause ou retour.
- `Z` : annuler.
- `Y` : refaire.

### Mobile

- Tap : selectionner.
- Tap sur boutons contextuels : tourner, annuler, reset.
- Drag optionnel : deplacer une piece.
- Safe area : boutons toujours dans la zone sure.

## Structure Des Niveaux

### Monde 1 - Miroirs

- Introduction source, miroir, cible.
- 10 niveaux.
- Objectif : apprendre la reflexion.

### Monde 2 - Obstacles

- Murs, absorbeurs, zones interdites.
- 10 niveaux.
- Objectif : anticiper les collisions.

### Monde 3 - Multiples Cibles

- Prismes et sources multiples.
- 12 niveaux.
- Objectif : coordonner plusieurs chemins.

### Monde 4 - Couleurs

- Filtres rouge/bleu/vert.
- 12 niveaux.
- Objectif : associer couleur et recepteur.

## Direction Artistique

- Grille sombre, pieces lisibles et contours lumineux.
- Rayon lumineux avec leger glow.
- Couleurs de pieces distinctes : miroir acier, prisme cyan, recepteur dore.
- Animation breve quand une cible s'active.

## Audio

- Son doux pour rotation de piece.
- Son clair quand une cible s'active.
- Son plus grave quand le rayon touche une zone interdite.
- Ambiance minimale, non intrusive.

## Technique

### Base Nkentseu

- `NKWindow` : fenetre, resize, safe area.
- `NKEvent` : clavier, souris, touch.
- `NKCanvas` : rendu grille, rayons, rectangles, icones simples.
- `NKUI` plus tard : menus, selection niveau, pause, options.

### Simulation Rayon

- Representer la grille par tuiles.
- Le rayon avance cellule par cellule.
- A chaque cellule, appliquer l'effet de l'objet.
- Limiter le nombre d'iterations pour eviter les boucles infinies.
- Stocker le chemin calcule pour le rendu.

### Donnees

Format propose :

```text
level_id
grid_width, grid_height
source position + direction
tiles
movable pieces
win conditions
par moves
```

## MVP

- 20 niveaux.
- Source, miroir, mur, recepteur.
- Selection et rotation.
- Annuler/refaire.
- Menu niveau simple.
- Sauvegarde progression locale.

## Risques

- Les rayons multiples peuvent complexifier la simulation.
- Le rendu du rayon doit rester lisible sur mobile.
- Les niveaux peuvent devenir repetitifs si les pieces ne sont pas introduites progressivement.

## Temps De Realisation

Hypothese : un developpeur, base `Nkoung` deja fonctionnelle, assets simples faits en code/NKCanvas.

- Prototype jouable : **3 a 5 jours**
- Vertical slice avec 8 niveaux et UI minimale : **2 semaines**
- MVP 20 niveaux : **4 a 6 semaines**
- Version polish avec audio, progression, menus : **6 a 8 semaines**

## Priorite

Tres bon candidat pour premier vrai jeu : scope controle, gameplay clair, technique raisonnable.
