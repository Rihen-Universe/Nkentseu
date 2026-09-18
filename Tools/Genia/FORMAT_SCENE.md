# Le document de scène — `.nkscene`

**AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
Écrit le 18/09/2026, **avant** toute ligne de code et **avant** de toucher au modèle.

---

## 1. Pourquoi ce document existe, et ce qu'il répare

Rodolf a refusé un résultat, mot pour mot : *« un bonhomme de neige qui rend un tore sur un
cylindre avec tous les compteurs au vert — ce n'est pas admissible »*. Il avait raison, et la
cause n'est pas le modèle : **c'est qu'il n'y avait rien entre la phrase et la géométrie.**

Le cap du dépôt (`echanges/CAP_IA_CONVERSATION_VERS_CREATION.md`) nomme trois gestes —
**discuter**, **fixer une spécification**, **produire** — et dit que c'est le deuxième qui
manque partout. Le pont du 17/09 allait de la phrase au maillage d'un seul coup. D'où **deux**
conséquences, et la seconde est la pire :

1. on ne peut **corriger** qu'en relançant tout ;
2. on ne peut **rien juger** : « le maillage est fermé » ne dit rien de « c'est un bonhomme
   de neige ».

Le document de scène est cette pièce manquante. **Il est la référence contre laquelle la
géométrie se vérifie.**

## 2. ⚠️ DEUX FIDÉLITÉS, ET IL NE FAUT JAMAIS LES CONFONDRE

| | ce qui est comparé | qui juge | mesurable ? |
|---|---|---|---|
| **fidélité mécanique** | document → géométrie | un instrument | **OUI**, et c'est ce que ce format débloque |
| **fidélité humaine** | phrase → document | **un humain** | **NON** |

**La seconde ne se mesure pas, et on ne fabriquera pas de critère qui prétendrait le faire.**
Un tel critère donnerait « un vert de plus et rien de vrai ». Que « trois sphères empilées »
soit une bonne description d'un bonhomme de neige, **c'est Rodolf qui le dit en lisant le
document** — et c'est précisément pour ça que le document doit être lisible en dix secondes.

Ce que le format garantit, c'est que **l'erreur devient localisable** : si le document dit
trois sphères et que l'objet montre un tore, le défaut est dans le producteur ; si le document
dit « un tore sur un cylindre » pour un bonhomme de neige, le défaut est dans le modèle — et
on le voit **sans jamais lancer la génération**.

## 3. Pourquoi des lignes et pas du JSON

Trois raisons, et aucune n'est une question de goût :

1. **un modèle 7B quantifié ferme mal les accolades** ; une ligne n'a rien à fermer. (Ce n'est
   pas une opinion : c'est mesurable, et le taux T2 le dira.)
2. **`git diff` est lisible ligne à ligne** — une partie modifiée est une ligne modifiée ;
3. **Rodolf peut corriger une valeur sans outil**, dans n'importe quel éditeur.

## 4. La grammaire du format

Une directive par ligne. `#` commence un commentaire. Les champs sont `clef valeur`, dans
n'importe quel ordre après le nom de la partie.

```
scene <nom>
demande <la phrase d'origine, recopiee telle quelle>
partie <nom> forme <forme> taille <sx> <sy> <sz> [relations et options]
```

### 4.1 Les formes — les six, inchangées

`sphere` · `cube` · `cylindre` · `cone` · `tore` · `capsule`

⚠️ **Le format n'ajoute AUCUNE forme.** Ce n'est pas un oubli : ce qui manquait n'était pas le
vocabulaire des formes, c'était **le placement**. Une chaise se fait avec un cube et quatre
cylindres — à condition de savoir les poser.

### 4.2 La taille est PAR AXE, et c'est le premier déverrouillage

    taille <sx> <sy> <sz>

Un cylindre `taille 1 0.1 1` est un **disque**. Un cube `taille 2 0.1 0.6` est une **planche**.
Un cylindre `taille 0.08 1.2 0.08` est un **barreau**. À elle seule, l'échelle anisotrope
multiplie ce que six formes peuvent représenter — bien plus que six formes de plus.

### 4.3 Le placement : une relation, ou un centre

    pose_sur <partie>        le dessous de cette partie touche le dessus de l'autre
    aligne_sur <partie>      meme axe vertical que l'autre (x et z recopies)
    centre <x> <y> <z>       position absolue, quand aucune relation ne convient
    decale <dx> <dy> <dz>    s'ajoute APRES la relation (un pied se decale en x et z)

**`pose_sur` est la directive qui porte le sens**, et c'est elle qui rend le critère possible :
elle affirme un **ordre vertical vérifiable** dans la géométrie produite. Un `centre` absolu
n'affirme rien qu'on puisse contredire.

### 4.4 La rotation

    rotation <rx> <ry> <rz>       en degres, appliquee autour du centre de la partie

### 4.5 L'opérateur

    op union | difference | intersection        (defaut : union)

`difference` **retire** la partie de ce qui précède : c'est ainsi qu'on creuse un verre, qu'on
perce un trou orienté, qu'on entaille. La grammaire de mots ne savait poser qu'**un trou
vertical par forme** ; ici, n'importe quelle forme peut soustraire n'importe où.

### 4.6 Le lissage

    lissage <r>     union lisse (les jonctions se fondent sur un rayon r ; 0 = nette)

---

## 5. Exemple écrit À LA MAIN — le bonhomme de neige

C'est le cas que Rodolf a refusé. Le document se lit en cinq secondes, et **on voit
immédiatement** s'il décrit un bonhomme de neige.

```
scene bonhomme_de_neige
demande un bonhomme de neige

partie base   forme sphere taille 1.00 1.00 1.00
partie tronc  forme sphere taille 0.70 0.70 0.70 pose_sur base
partie tete   forme sphere taille 0.45 0.45 0.45 pose_sur tronc
lissage 0.06
```

**Ce que le critère mécanique en tire, sans rien deviner** : trois parties, deux relations
`pose_sur`, donc **trois renflements empilés** le long de l'axe vertical, de rayons
**décroissants** vers le haut. Un tore posé sur un cylindre en donne **deux**, et le critère
**rougit**. C'est exactement ce qui manquait le 17/09.

## 6. Exemple écrit À LA MAIN — le verre à pied

L'autre cas où quatre compteurs étaient verts pour une bouteille.

```
scene verre_a_pied
demande un verre a pied

partie pied    forme cylindre taille 0.50 0.04 0.50
partie tige    forme cylindre taille 0.07 0.50 0.07 pose_sur pied
partie coupe   forme cone     taille 0.45 0.40 0.45 pose_sur tige
partie creux   forme cone     taille 0.38 0.36 0.38 pose_sur tige decale 0 0.06 0 op difference
```

**Trois choses qu'on ne pouvait pas écrire avant, et qui sont toute la différence** : un pied
large et **plat** (échelle par axe), une tige **fine et longue** (idem), et un creux **soustrait**
de la coupe — un verre est creux, et `difference` est le seul moyen de le dire.

---

## 7. Ce que le format NE fait pas, et c'est écrit avant qu'on le découvre

1. **Aucune forme organique.** Un personnage humain reste hors de portée, et le document doit
   pouvoir le **refuser** plutôt que de rendre trois capsules.
2. **Aucune symétrie, aucune répétition.** Quatre pieds de chaise s'écrivent en quatre lignes.
   → *Se rouvre le jour où une scène dépassera ~20 parties : il faudra `repete`.*
3. **Aucune matière, aucune couleur.** Le producteur n'en pose pas ; l'écrivain OBJ ne les
   écrirait pas (dette déjà nommée au R14.9).
4. **`pose_sur` est vertical.** « Contre », « à côté de », « à l'intérieur de » ne sont pas
   dans le format. → *Se rouvre au premier document qui en aurait besoin ; `centre` et
   `decale` les couvrent en attendant, au prix de la vérifiabilité.*
