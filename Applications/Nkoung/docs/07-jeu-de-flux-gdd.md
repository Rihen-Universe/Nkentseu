# GDD 07 - Jeu De Flux

## Resume

**Genre :** puzzle de connexion 2D.  
**Plateformes :** Windows, Linux, Android, HarmonyOS, Web.  
**Public cible :** joueurs puzzle mobile, sessions courtes.  
**Promesse :** relier des sources et des recepteurs sans croiser les chemins, avec des contraintes de couleur et d'espace.

Jeu De Flux est inspire des puzzles de liaison. Le joueur trace des chemins sur une grille entre paires de points. Toutes les cases doivent parfois etre remplies, et les chemins ne doivent pas se croiser.

## Objectifs De Design

- Tres facile a prendre en main.
- Ideal tactile.
- Niveaux courts, rejouables.
- Difficulte par espace limite.
- Rendu clair et satisfaisant.

## Boucle De Jeu

1. Le niveau affiche plusieurs paires de points colores.
2. Le joueur choisit une source.
3. Il trace un chemin jusqu'au recepteur correspondant.
4. Si le chemin croise un autre chemin, l'action est refusee ou remplace l'ancien chemin.
5. Le niveau est gagne quand toutes les paires sont connectees.
6. Variante : toutes les cases doivent etre couvertes.

## Mecaniques

### Source Et Recepteur

- Meme couleur = paire a connecter.
- Chaque paire ne peut avoir qu'un chemin actif.

### Chemin

- Trace orthogonal : haut, bas, gauche, droite.
- Ne traverse pas les murs.
- Ne croise pas les autres chemins.
- Peut etre efface en retracant depuis une source.

### Mur

- Bloque le passage.
- Introduit progressivement.

### Case Obligatoire

- Doit etre couverte pour valider le niveau.
- Peut forcer des detours.

### Teleporteur

- Entre par une case, sort par une autre.
- Variante avancee.

### Pont De Flux

- Permet a deux chemins de se croiser sur une case speciale.
- Variante avancee a utiliser rarement.

## Conditions De Victoire

MVP :

- Toutes les paires sont connectees.

Mode avance :

- Toutes les cases libres sont couvertes.
- Nombre de mouvements inferieur au par.

## Controles

### Desktop

- Clic et drag depuis une source.
- Relacher pour valider.
- Clic droit sur chemin : effacer.
- `R` : reset.
- `Z` : annuler.

### Mobile

- Drag au doigt.
- Tap sur source puis glisser.
- Bouton reset en safe area.
- Les chemins doivent etre assez epais pour etre lisibles sous le doigt.

## Structure Des Niveaux

### Pack 1 - Bases

- Grille 5x5.
- 2 a 3 paires.
- 20 niveaux.

### Pack 2 - Espaces Serres

- Grille 6x6.
- 4 paires.
- Toutes les cases doivent etre remplies.
- 20 niveaux.

### Pack 3 - Obstacles

- Murs et cases bloquees.
- 20 niveaux.

### Pack 4 - Special

- Teleporteurs et ponts.
- 20 niveaux.

## Direction Artistique

- Fond sombre ou neutre.
- Chemins colores epais.
- Points ronds tres lisibles.
- Petite animation de validation quand une paire est connectee.

## Audio

- Son doux pendant le tracage.
- Son de connexion.
- Son d'erreur discret.
- Victoire courte et satisfaisante.

## Technique

### Representation

- Grille de cellules.
- Chaque cellule contient : vide, mur, source, recepteur, chemin couleur.
- Chaque couleur garde une liste ordonnee de cellules.

### Tracage

- Pendant drag, convertir la position pointeur en cellule.
- Accepter seulement les cellules adjacentes.
- Si retour sur le chemin courant, raccourcir le chemin.
- Si collision avec autre couleur, bloquer ou effacer selon option.

### Validation

- Pour chaque paire, verifier source -> recepteur.
- Si mode couverture, verifier toutes les cellules libres.

### Modules

- `NKCanvas` : rendu grille et chemins.
- `NKEvent` : souris/touch.
- `NKUI` : packs, boutons, score.

## MVP

- Grille 5x5 a 7x7.
- 40 niveaux.
- Drag souris/touch.
- Reset/undo simple.
- Sauvegarde progression.

## Risques

- Les controles drag doivent etre tres fiables sur mobile.
- Beaucoup de niveaux sont necessaires pour donner de la valeur.
- Sans variante, le jeu peut sembler trop connu.

## Temps De Realisation

Hypothese : un developpeur, rendu code/NKCanvas, niveaux faits a la main.

- Prototype drag + validation : **3 a 5 jours**
- Vertical slice 15 niveaux : **2 semaines**
- MVP 40 niveaux : **4 a 6 semaines**
- Version polish 80 niveaux + packs : **7 a 9 semaines**

## Priorite

Excellent candidat pour mobile et web. Tres rapide a rendre jouable, mais demande beaucoup de niveaux pour briller.
