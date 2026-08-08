# Fabriquer une grille

Ce chapitre est le plus court, et c'est significatif : **ajouter un plateau ne
demande pas une ligne de code.**

## 4.1 Pourquoi le plateau est une donnée

**`REGLES_COMPLETES_v2.md:93-107`**

```
Le plateau n'est pas une constante du code : c'est un descripteur sérialisable,
éditable à la souris dans le moteur de test.

BoardDesc {
  topology     : HEX_POINTY | HEX_FLAT | SQUARE_4 | SQUARE_8
  cells        : liste de coordonnées valides        // définit la forme réelle
  blocked      : liste de coordonnées bloquées
  starts       : [ { player_index, coord, level } ]  // positions initiales
  min_players  : entier
  max_players  : entier
}
```

Les formes prévues pour Conqueror — rectangle, hexagone, plus, diamant — ne sont
pas dans le moteur : ce sont des fichiers. La raison est pratique. Le palier 0
doit répondre à *« quelle taille de plateau ? quel voisinage ? quel avantage au
premier joueur ? »* : trois questions qui demandent d'essayer vingt plateaux. Si
chaque essai coûte une recompilation, on en essaiera trois.

## 4.2 Où déposer le fichier

```
Build/ConquerorLab/boards/ma_grille.json
```

Le chemin exact est affiché dans le panneau *Règles*, section **Plateau**. Le
fichier apparaît dans la liste déroulante à la seconde suivante — **sans
compilation**, puisqu'il n'y a rien à compiler.

Au premier lancement, l'atelier y écrit un exemple :

**`Applications/ConquerorLab/src/ConquerorLab/NkcBoardLibrary.h — EnsureExample`**

```cpp
/// Ecrit un exemple si le dossier est vide, a partir du plateau que le
/// moteur charge expose. Le format documente vaut mieux qu'un format
/// decrit : celui-ci est forcement valide, puisqu'il vient du module.
```

## 4.3 Le format, sur un exemple minuscule

Un plateau carré 3×3, deux joueurs en diagonale, une case bloquée au centre :

**`Écrit pour ce cours — format de LoadBoardJson`**

```json
{
  "topology": "SQUARE_4",
  "cells": [[0,0],[1,0],[2,0],
            [0,1],[1,1],[2,1],
            [0,2],[1,2],[2,2]],
  "blocked": [[1,1]],
  "starts": [
    {"player": 0, "q": 0, "r": 0, "level": 0},
    {"player": 1, "q": 2, "r": 2, "level": 0}
  ],
  "min_players": 2,
  "max_players": 2
}
```

Champ par champ :

| Champ | Rôle | Piège |
|---|---|---|
| `topology` | `HEX_POINTY`, `HEX_FLAT`, `SQUARE_4`, `SQUARE_8` | fixe le voisinage **et** l'interprétation de `(q, r)` |
| `cells` | **la forme réelle** du plateau | une coordonnée absente d'ici n'existe pas : c'est le hors-plateau |
| `blocked` | cases présentes mais inoccupables | doivent aussi figurer dans `cells` |
| `starts` | totems de départ | `player` au-delà de `max_players` est ignoré |
| `min_players` / `max_players` | bornes du plateau | informatif : c'est le moteur qui décide quoi en faire |

> **⚠️ `cells` définit la forme, pas un rectangle englobant**
>
> C'est ce qui permet des plateaux en croix, en diamant, ou troués. Ne listez
> **que** les cases qui existent. Le moteur de référence teste l'appartenance
> par recherche dans cette liste : une coordonnée absente est du hors-plateau,
> exactement comme si elle était au-delà du bord.

## 4.4 Hexagone : la conversion qui piège

En hexagone, les coordonnées sont **axiales** `(q, r)`, pas des lignes et des
colonnes. Le moteur de référence construit son 6×7 ainsi :

**`Applications/ConquerorLab/modules/rules/ConquerorRulesV2.cpp — BuildDefaultBoard`**

```cpp
for (int32 row = 0; row < 7; ++row) {
    for (int32 col = 0; col < 6; ++col) {
        NkcCoord c;
        c.q = static_cast<int16>(col - (row >> 1));  // odd-r -> axial
        c.r = static_cast<int16>(row);
        B.cells[B.cellCount++] = c;
    }
}
```

La ligne qui compte est `col - (row >> 1)` : c'est la conversion **odd-r → axial**.
Si vous écrivez un générateur de plateau hexagonal, c'est là que vous vous
tromperez. Le symptôme est net et reconnaissable : **le plateau s'affiche en
losange penché au lieu d'un rectangle d'hexagones.**

> **✅ Le raccourci qui évite l'erreur**
>
> Ne calculez pas vos coordonnées à la main. Chargez le plateau par défaut,
> cliquez **« Exporter le plateau courant »** dans le panneau *Règles*, et
> modifiez le fichier obtenu. Vous partez d'une géométrie juste.

## 4.5 La symétrie n'est pas une coquetterie

**`REGLES_COMPLETES_v2.md:133-137`**

```
⚠️ Le placement initial doit être symétrique sous une isométrie du plateau.
Le moteur de test doit refuser de lancer un batch d'équilibrage sur un plateau
asymétrique, ou signaler l'asymétrie dans le rapport. Sans cela, tout écart de
winrate mesuré est ininterprétable : on ne saura pas s'il vient des règles ou
du plateau.
```

> **⚠️ Cette vérification n'existe pas encore dans l'atelier**
>
> Le document l'exige ; l'atelier ne la fait pas. **C'est à vous de vérifier la
> symétrie de vos plateaux à l'œil** avant de lancer une campagne dessus.
>
> Il y a toutefois un garde-fou partiel : la campagne **inverse les côtés une
> partie sur deux** (voir chapitre 5.4), ce qui sépare l'avantage de position de
> l'avantage de stratégie. Cela compense un plateau asymétrique **entre les deux
> camps**, mais pas un plateau qui avantagerait structurellement le premier
> joueur.

## 4.6 Quand le moteur refuse votre fichier

L'atelier ne parse pas le JSON : il passe la chaîne telle quelle à
`LoadBoardJson`. C'est donc **le module** qui accepte ou refuse.

**`Applications/ConquerorLab/src/ConquerorLab/NkcBoardLibrary.h — LoadInto`**

```cpp
if (!vt.LoadBoardJson(inst, text.CStr())) {
    mMsg  = "Le moteur a REFUSE ce plateau : ";
    mMsg += mFiles[idx].name;
    mMsg += "  (cellules manquantes ou JSON invalide)";
    return false;
}
```

Ce message s'affiche dans le panneau *Règles*, en rouge. Trois causes, par ordre
de fréquence :

1. **`cells` vide ou absent.** Le moteur de référence refuse explicitement un
   plateau sans cases.
2. **JSON malformé.** Le scanner des modules est minimal : il n'accepte pas les
   commentaires, ni les virgules traînantes.
3. **Le module ne sait pas charger de plateau.** `RegleMinimale.cpp` renvoie
   toujours `0` : son plateau est figé dans le code. C'est un renoncement assumé
   pour un exemple, pas un modèle à suivre.

> **✅ Conséquence heureuse de « l'atelier ne parse rien »**
>
> Si votre moteur accepte un champ supplémentaire — disons
> `"artefacts":[{"q":1,"r":2,"id":7}]` — il le verra **sans qu'on touche à
> l'atelier**. Le format du plateau vous appartient ; le contrat ne fixe qu'un
> socle commun.

## 4.7 Ce que l'atelier ne fait pas

Le document de règles parle d'un plateau « éditable à la souris ». **Cet éditeur
n'existe pas.** Aujourd'hui, on écrit le JSON à la main, ou on exporte puis on
modifie.

C'est une limite réelle et assumée : l'éditeur graphique viendra quand quelqu'un
aura assez souffert du fichier texte pour savoir de quoi il a besoin.

## Exercices

> **✏️ 1 — Le plateau troué**
>
> Exportez le plateau par défaut, puis retirez huit cases au centre pour en faire
> un anneau. Chargez-le. Que devient la durée moyenne d'une partie sur 200
> parties IA contre IA ? Expliquez.

> **✏️ 2 — Carré contre hexagone**
>
> Fabriquez un plateau `SQUARE_8` de 42 cases (6×7) et comparez-le à l'hexagone
> 6×7 par défaut, **à même nombre de cases**. Lancez 500 parties sur chacun.
> Le taux de nuls change-t-il ? Le nombre de coups ? Que dit ce résultat sur
> l'effet du voisinage ?

> **✏️ 3 — L'asymétrie visible**
>
> Fabriquez volontairement un plateau asymétrique : donnez trois totems de départ
> au joueur 0 et deux au joueur 1. Lancez 400 parties **avec** l'inversion des
> côtés, puis 400 **sans**. Comparez les deux winrates et expliquez précisément
> ce que l'inversion a corrigé — et ce qu'elle n'a pas corrigé.


## 4.8 La forme des cellules

Une grille carrée doit-elle se dessiner en carrés ? Non — et les séparer est
instructif.

**`Applications/ConquerorLab/boards/_generer.py — plateau()`**

```python
def plateau(topology, cells, starts, blocked=None, cell_shape=None):
    """`cell_shape` est de la PRESENTATION : "SQUARE", "HEX" ou "CIRCLE".

    Elle ne touche ni au voisinage, ni aux coups legaux, ni au resultat — c'est
    justement pourquoi elle n'est PAS dans le contrat : le moteur ne la lit
    jamais, seul l'atelier la regarde pour dessiner.
    """
```

Trois formes livrées : **carrée**, **hexagonale**, **ronde**. Le champ est
facultatif ; absent, l'atelier déduit la forme de la topologie, comme avant.

Le panneau *Règles* offre en plus un réglage **Forme des cellules** qui prime sur
le fichier — pour comparer sans rien réécrire.

> **✅ Trois sources, un ordre de priorité**
>
> 1. le module, s'il fournit `GetCellShape` (chapitre 5) — il a le dernier mot
>    sur *sa* géométrie ;
> 2. votre choix dans le panneau *Règles* ;
> 3. ce que déclare le fichier `.json`.

## 4.9 La bibliothèque livrée

Quatorze plateaux, dans `travail/boards/` dès le premier lancement.

| Forme | Fichiers |
|---|---|
| **hexagonal** | `hexagone_6x7` (référence), `4x5`, `9x8`, `coeur_bloque`, `plat` |
| **rectangulaire** | `rectangle_8x6`, `carre_8x8_diagonales` |
| **en croix** | `plus` |
| **parallélogramme** | `parallelogramme_6x7`, `parallelogramme_8x5` |
| **libre** | `diamant` |
| **formes de cellule** | `rectangle_8x6_rond`, `hexagone_6x7_rond`, `carre_8x8_hexagones` |

Le parallélogramme mérite un mot : c'est `hex_rect` **sans** la correction
`- (row >> 1)`, autrement dit la forme qu'on obtient quand on oublie la
conversion odd-r → axial. La livrer comme un plateau à part entière vaut mieux
que de la laisser apparaître comme un bug — la reconnaître à l'écran fait
comprendre du même coup ce que corrige `hex_rect`.

Tous sont produits par `boards/_generer.py`, jamais écrits à la main : la
symétrie des départs exigée par `REGLES §4.2` y est **calculée par réflexion**.
Un plateau asymétrique rend tout écart de winrate ininterprétable.
