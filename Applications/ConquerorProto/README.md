# ConquerorProto — prototype 2D « Spread » (NKCanvas)

> Prototype **jetable** pour répondre à **UNE** question : *« la mécanique
> Spread (dupliquer / attaquer / conquérir) est-elle fun ? »*
> Socle minimal **NKCanvas** (OpenGL), **aucune dépendance Noge**.

## Philosophie (pourquoi ce découpage)

Ce proto illustre la règle : **on ne reconstruit pas un mini-Noge**. Il est
coupé en deux parties nettes :

| Fichier | Rôle | Sort |
|---|---|---|
| `src/ConquerorProto/ConquerorCore.{h,cpp}` | **Moteur de règles pur** : état, coups, attaque, conquête, fin de partie. Zéro dépendance moteur, déterministe, testable (`SelfTest()`). | **On le GARDE** — il migrera vers Noge **sans modification**. |
| `src/ConquerorProto/main.cpp` | **Glue jetable** : fenêtre + boucle + dessin + clic→case, en NKCanvas. | **On le JETTE** au passage vers Noge. |

Ce qu'on **n'a PAS** écrit (et qu'on n'écrit pas dans un proto) : `SceneManager`,
ECS, `AssetManager`, EventBus. Un seul écran, une grille `Cell cells[6][6]`, une
police chargée une fois. ~200 lignes de glue.

**Le seuil pour passer à Noge** : dès qu'il te faut *plusieurs scènes/menus*, *beaucoup
de types d'entités/assets*, ou que tu te surprends à copier la même infra pour un
2ᵉ écran. Pour ce seul plateau hotseat, on est très en dessous.

## Ce que le proto prouve

- La boucle Spread tient debout et se joue (hotseat 2 joueurs, même écran).
- Le moteur est **déterministe** et **testé** (`SelfTest()` au démarrage, log `OK`/`FAIL`).
- La frontière **règles pures / présentation** est réelle : `main.cpp` ne calcule
  aucune règle, il *dessine l'état* et *route les clics*.

## Périmètre

**Inclus** : grille carrée 6×6 (adjacence 4 directions), action **DUPLIQUER**,
attaque + conquête (HP 10 / DMG 3), fin de partie au nombre de totems, hotseat.

**Exclu** (viendra dans le vrai jeu, sur Noge) : Fusion N0-N4, 4 pouvoirs, grille
hexagonale, SAUTER (distance 2), 25 totems, IA, animations, audio, réseau.

## Jouer

- **Clique un de tes totems** (Joueur courant) → les cases légales s'allument en vert.
- **Clique une case verte** → duplication (+ attaque des ennemis adjacents).
- Le tour passe automatiquement ; si un joueur est bloqué, il passe.
- Partie finie → **clic pour rejouer**.

## Build & run

```bash
# depuis la racine du workspace Nkentseu
jenga build --target ConquerorProto --config Debug
jenga run   --target ConquerorProto
# variantes
jenga build --target ConquerorProto --config Release
jenga build --target ConquerorProto --platform web      # WebGL2
```

> La police `Resources/Fonts/Antonio-Bold.ttf` est chargée au runtime ; si elle
> est absente, le jeu reste jouable (texte désactivé, feedback via les couleurs
> et les logs).

## Prochaine étape

Quand le Spread est validé en playtest : garder `ConquerorCore`, jeter `main.cpp`,
et rebâtir la présentation sur **Noge** (scènes menu/jeu, ECS gameplay, assets),
puis ajouter Fusion + pouvoirs + hex + IA (cf. fiches de stage A1, A6 du dossier
`Conquerror_PREMIUM/Stage/`).
