# UnkenyEditor — appareils, formes d'écran et zone sûre

### Document 03 — voir le jeu sur chaque écran avant de le livrer

> Demande de Rihen (30/09, puis 01/10) : *« un design d'appareil beaucoup plus
> beau, de vrais appareils ; choisir un type d'écran de base ou entrer des
> dimensions personnalisées »* ; *« régler l'affichage mobile et tablette, ajouter
> d'autres tailles et formes d'écran, sans oublier les positions des caméras ;
> introduire la notion de zone sûre ; que le paysage et le portrait soient la
> rotation exacte de l'appareil »*.
>
> Règle du dépôt (Rodolf, 18/08) : l'éditeur **simule les zones sûres** pour voir
> le débordement **avant** de déployer. Règle de `NkEditeurAppareils.h` : un chiffre
> d'appareil porte **sa provenance** (documentation publique) ; ce sont des ordres
> de grandeur, pas des mesures certifiées.

## 1. L'existant (vérifié le 01/10)

- `Editeur/NkEditeurAppareils.h` : 6 profils (bureau, téléphone à encoche,
  Android, compact, tablette, navigateur), zone sûre en pixels en portrait.
- `NkTourner` : un seul paysage ; l'encoche passe **toujours à gauche**, la marge
  droite vaut 0, l'indicateur de geste est réduit à 60 %. **Faux pour iOS**, qui
  réserve en paysage une marge **à gauche et à droite** sur les écrans à encoche.
  Pas de portrait inversé, pas de sens de rotation.
- Pas de cadre réaliste, pas de dimensions personnalisées, pas de forme d'écran
  (coins arrondis, encoche, îlot, poinçon, écran rond).
- **Le jeu ignore la zone sûre** : NKWindow la fournit (`NKEvent/NkSafeArea.h`,
  Android, Web…), mais ni Unkeny ni UnkenyPlayer ne la lisent.

## 2. Ce qu'on veut

1. **Un catalogue d'appareils**, chacun avec sa provenance : téléphones (encoche,
   îlot, poinçon centré et à gauche, goutte d'eau, sans découpe), tablettes 4:3 et
   16:10, **pliables** (écran intérieur presque carré, écran extérieur étroit),
   bureau en 16:9, 16:10, 21:9 et 4:3, navigateur, **TV** (avec la marge de
   sécurité des titres), console portable, **montre ronde**. Pour chacun :
   dimensions en points et en pixels, densité, **rayon des coins**, **forme, taille
   et position de la caméra** (encoche, îlot, poinçon), indicateur de geste,
   boutons physiques (pour le cadre).
2. **Un appareil personnalisé** : un type de base, puis largeur, hauteur, densité,
   rayon des coins, découpe de caméra (type, position, taille) et zone sûre
   modifiables ; enregistré avec le projet.
3. **La rotation exacte** : quatre orientations (portrait, portrait inversé,
   paysage à gauche, paysage à droite). Le **cadre, les coins et la découpe de
   caméra tournent géométriquement** avec l'appareil ; les **marges de la zone
   sûre suivent les règles du système** dans chaque orientation (iOS : en paysage,
   marges gauche **et** droite sur un écran à découpe, indicateur en bas ;
   Android : marge du seul côté de la découpe, barre de navigation selon le mode).
4. **De beaux cadres** : dessinés en vecteurs (aucune image sous droits), en
   version sombre et claire, avec bordure, coins, boutons, découpe de caméra ;
   interrupteurs « cadre / zone sûre / découpe ».
5. **La zone sûre dans le jeu** : Unkeny lit la zone sûre réelle (NKWindow) sur
   l'appareil, ou celle simulée par l'éditeur dans le viseur et en Jouer ; il
   l'offre aux jeux (une fonction qui rend le rectangle sûr) ; l'interface et le
   HUD peuvent s'ancrer dessus ; une surimpression de débogage la montre.
   UnkenyPlayer la transmet.
6. **La caméra du jeu selon l'écran** : une règle par projet (hauteur fixe,
   largeur fixe, tout montrer avec bandes, remplir) et son aperçu dans l'éditeur
   pour chaque appareil et chaque orientation.

## 3. Méthode

- Des témoins pour chaque point (rotation des marges dans les 4 orientations,
  appareil personnalisé enregistré et relu, rectangle sûr offert au jeu), avec
  contre-épreuve.
- Des **captures** à montrer à Rihen : téléphone à encoche dans les 4
  orientations, téléphone à poinçon, tablette, pliable, montre ronde, appareil
  personnalisé, HUD ancré sur la zone sûre.
