# Éléments à dessiner — inventaire, avec description de base

> Écrit le 2026-08-22 à la demande de Rodolf : *« ce serait bien de connaître tout
> ceux que je vais designer avec leurs description visuelle de base que je
> pourrais modifier »*.
>
> Chaque ligne = **un élément vectoriel**. La description est une **proposition à
> corriger**, pas une décision. La `planche_01_noeuds.svg` en dessine déjà une
> bonne partie — reprends-la ou repars de zéro.
>
> ⚠️ **Mis à jour le 22/08.** Trois choses ont changé de statut : le **cadre**
> (B10) est **validé**, le **groupe** (B8) est **tranché**, le **fil d'Ariane**
> (E6) devient **obligatoire**. Et surtout : le nœud de Rodolf a été relevé au
> JSON dans `editeur_nodal.sketch` — **ses valeurs font foi**, voir **A0**
> ci-dessous, qui **contredit A1, A3 et A4**.

---

## Comment lire ce document

| colonne | |
|---|---|
| **élément** | ce qu'il faut dessiner, un fichier par ligne si tu veux |
| **description de base** | ce que je propose — à modifier librement |
| **état** | ✅ déjà dans la planche 01 · ⬜ à faire · ⚠️ non tranché |

⚠️ **Les lignes ⚠️ sont celles où je n'ai pas de proposition défendable.** Ne les
devine pas : dis-moi ce que tu veux, ou dessine et je suivrai.

---

---

## ⚠️ A0 · LE NŒUD DE RODOLF FAIT FOI — relevé au JSON dans `editeur_nodal.sketch`

> Relevé le 22/08 dans le groupe `Noeud` du fichier Lunacy
> `references/editeur_nodal.sketch` (un `.sketch` est un ZIP de JSON : les
> valeurs ci-dessous sont **lues**, pas estimées à l'œil).
>
> **Il y a DEUX groupes `Noeud` dans le fichier. Le second fait foi** : il ne
> diffère du premier que par la prise d'exécution, passée du triangle plein au
> **rectangle à pointe** — le choix que Rodolf a énoncé (*« je pense que tes
> prises d'exécution sont meilleures »*).

| élément | valeur lue | ce qui change par rapport à ce document |
|---|---|---|
| **corps** | rayons par coin **`[0, 0, 3, 3]`** — coins **hauts vifs**, coins **bas** à 3 | ❌ A1 disait « coins 3 px » partout |
| **en-tête** | rayons par coin **`[5, 5, 0, 0]`** — coins **hauts** à 5, bas vifs | ⚠️ absent de A2 |
| **prise de donnée** | rectangle **17,3 × 63,3** — **haute et étroite**, ratio 1 : 3,7 | ❌ A4 disait 10 × 12, presque carré |
| **position de la prise de DONNÉE** | **collée CONTRE le bord, entièrement à l'extérieur** : le corps commence à x = 17,3, la prise occupe [0 ; 17,3] |
| **position de la prise d'EXÉCUTION** | ⚠️ **elle, elle CHEVAUCHE** : elle couvre [6,57 ; 51,0], donc **33,7 px à l'intérieur du corps**. ✅ Différence VOULUE — *« les prises d'instruction doivent bien se marier au nœud »* | ⚠️ A4 dit « à cheval, la moitié dépasse » — **elle ne dépasse pas de moitié, elle est entièrement dehors** |
| **prise d'entrée** | **contour seul**, épaisseur 8, gris `#808080` | ⚠️ creux |
| **prise de sortie** | **rectangle plein** `#FF5252` | ⚠️ plein |
| **prise d'exécution** | **pentagone à pointe** 44,4 × 51,8, points `{0,0} {0.583,0} {1,0.5} {0.583,1} {0,1}` — un rectangle dont le côté droit est un chevron | ✅ confirme A5 |
| **barre d'exécution** | rectangle **plein orange `#F79A28`**, hauteur **11**, **pleine largeur**, collé sous l'en-tête | ⚠️ A3 dit « trait de 2,5 px » — c'est **une barre**, pas un filet |
| **séparateur interne** | trait `#BDBDBD`, épaisseur **5** | ⚠️ absent de ce document |
| **couleurs** | corps `#212121` · filet `#808080` 1 px · en-tête `#01574D` · exécution et prise d'exécution `#F79A28` · prise de donnée `#FF5252` | ⚠️ A1 disait `#232329` / `#3a3a44` |

### ✅ L'ÉCHELLE EST MESURÉE — **1 unité du `.sketch` = 1 pixel**, et le facteur est **6**

> ⚠️ Ce paragraphe disait « je ne la tranche pas ». **Elle a été mesurée le 22/08
> et elle est tranchée** — par une preuve simple : la capture de référence
> `Screenshot 2026-07-17 174628.png` fait **751 × 473 px**, et elle est posée
> dans le document à **exactement `w=751 h=473`**. Une unité vaut donc un pixel,
> sans ambiguïté.

**Conséquence : le nœud de Rodolf fait littéralement 1 168,5 px de large, soit
environ SIX FOIS un nœud réel.** C'est une **maquette d'étude**, pas un nœud à sa
taille finale — donc **seuls les rapports se transportent**, ce qui était
l'hypothèse A. Elle est confirmée.

✅ **Et le facteur tombe juste : `126,6 / 21 = 6,03`.** La hauteur d'en-tête de
**21 px** déjà retenue en A2 est donc **exactement à la bonne échelle**. Rien à
changer de ce côté — c'est le reste qui doit s'y aligner.

| élément | chez Rodolf | ÷ 6,03 → valeur cible | valeur actuelle des planches | verdict |
|---|---|---|---|---|
| hauteur d'en-tête | 126,6 | **21 px** | 21 | ✅ **juste** |
| largeur du corps | 1 168,5 | **194 px** | 170 | ⚠️ **trop étroit** |
| prise de donnée | 17,3 × 63,3 | **2,9 × 10,5 px** | 6 × 22 | ❌ **2,1 × trop grosse** |
| barre d'exécution | 11,0 | **1,8 px** | 2,5 | ⚠️ un peu épaisse |

### ⚠️ CORRIGÉ le 22/08 — ce n'est pas « la prise est trop grosse », **c'est que la planche mélange DEUX ÉCHELLES**

⚠️ **Ce paragraphe annonçait un écart décomposé en `2,095 × 1,140 = 2,389`. Le
second facteur était FAUX, et le premier mal nommé.** Les deux corrections
viennent de mesures, pas de relecture.

#### 1. ❌ « le nœud est 1,14 × trop étroit » — **artefact d'un échantillon de UN**

J'avais comparé un nœud de **170 px**. Relèvement des cinq planches :

| | |
|---|---|
| largeurs réellement employées | **38 nœuds**, de **150** à **330 px**, médiane **270** |
| le 170 que j'avais pris | le **second plus étroit sur 38** |
| nœuds **plus larges** que le ratio de Rodolf | **33 / 38 — 87 %** |

✅ **La largeur d'un nœud n'est pas une proportion de son en-tête : elle suit son
CONTENU** (nombre de rangées, longueur des libellés). Rodolf a dessiné **un** nœud,
avec **son** contenu. **Il n'y a donc aucun écart de largeur à corriger**, et le
`1,140` était le résultat d'avoir choisi le cas le plus extrême.

📌 **La leçon est plus utile que le chiffre** : une moyenne d'un seul échantillon
n'est pas une mesure, et elle donne un nombre précis qui a l'air d'en être une.

#### 2. ✅ Le vrai diagnostic : **les planches portent deux échelles à la fois**

| élément des planches | sa valeur | l'échelle qu'elle implique |
|---|---|---|
| **prise** 6 × 22 | ÷ `17,3 × 63,3` | **0,3476** |
| **en-tête** 21 px | ÷ `126,6` | **0,1659** |

**`0,3476 / 0,1659 = 2,095`** — le facteur retrouvé, mais **il ne dit pas qu'un
élément est faux : il dit que deux éléments ne sont pas à la même échelle.**

⚠️ **Et `0,1659` n'est pas une erreur non plus** : c'est **l'échelle 1 de
l'éditeur réel** (le nœud de Rodolf vaut 6 × un nœud de la capture de référence).
**L'en-tête de 21 px est la valeur juste pour l'éditeur.** Ce sont les prises qui
sont dessinées à une autre échelle.

#### 3. 🔴 Les deux voies pour rétablir la cohérence — **et la première a été MESURÉE**

| | ce qu'on fait | résultat |
|---|---|---|
| **A** | ramener la prise à **2,9 × 10,5** (l'échelle de l'en-tête) | ❌ **essayé en bac à sable, PNG rendu et REGARDÉ — voir ci-dessous** |
| **B** | porter l'en-tête à **44 px** et tout le nœud avec | ⚠️ respecte Rodolf à `0,5000` et `0,1364` (lui : `0,1367`), mais **double la taille des cinq planches** et demande de refaire leur mise en page |

**Ce que le PNG de l'option A montre, et qu'aucun calcul n'annonçait :**

- ❌ **les formes `tableau` et `dico` deviennent indistinguables.** Leurs segments
  tombent à **2,17 px de haut**, et la demi-largeur du dictionnaire à **0,65 px**
  — sous le pixel. ⚠️ **Le panneau 3 de la planche 02 existe précisément pour
  enseigner cette distinction, et il ne la montre plus** ;
- ❌ **la distinction creux / plein** (non branché / branché) disparaît : à
  2,9 px de large, deux contours de 1,6 px se touchent — la prise n'a plus
  d'intérieur. Même en amincissant le trait à 0,55 px, l'œil ne sépare plus rien ;
- ❌ les 16 prises du panneau 2 deviennent **des traits gris**.

> ⚠️ **Une planche qui perd ce qu'elle enseigne n'est pas plus juste, elle est
> inutile.** L'option A rend les planches conformes à l'éditeur **et illisibles
> comme documents.**

✅ **Et l'argument qui tranche vient de Rodolf lui-même : il a dessiné son nœud à
6 ×.** Un document d'étude n'est pas une capture d'écran de l'éditeur — il se
regarde à 100 %, il s'imprime, il se projette. **C'est pour ça qu'il l'a agrandi.**

#### 🔴 CE QUI DOIT ÊTRE TRANCHÉ, et que je n'applique pas

**Les planches doivent adopter les RATIOS de Rodolf — pas ses pixels.** Reste à
choisir leur **échelle propre**, une seule fois, et à l'appliquer à **tous** les
éléments :

- **échelle `× 2,1`** (en-tête 44 px) : les prises actuelles deviennent justes,
  rien ne devient illisible — mais les cinq planches doublent et leur mise en
  page est à refaire ;
- **échelle `× 1`** (en-tête 21 px) : mesuré illisible ci-dessus ;
- **une échelle intermédiaire** — par exemple en-tête 32 px, prise 4,4 × 16 — qui
  respecterait les ratios en restant lisible. ⚠️ **Non mesurée** : je ne
  l'affirme pas, elle est à essayer en bac à sable comme les autres.

⚠️ **Je n'ai rien appliqué aux planches.** Le bac à sable a servi à mesurer, le
dépôt est intact, et les cinq générateurs reproduisent toujours leur planche à
l'octet près.

### ⚠️ Correction sur la prise d'EXÉCUTION — le relevé porte sur le mauvais groupe

Le rapport « `64 / 126,6` ≈ la moitié, comme la prise de donnée » vient du
**premier** groupe `Noeud` — celui dont la prise d'exécution est un **triangle
plein**, et que R1 déclare **périmé**. Le groupe qui fait foi est le **second**,
celui du rectangle à pointe :

| | groupe 1 — triangle (périmé) | **groupe 2 — rectangle à pointe (fait foi)** |
|---|---|---|
| géométrie | x = 8,27 · **64,0 × 64,0** | x = 6,57 · **44,43 × 51,83** |
| hauteur / en-tête | 0,5055 | **0,4094** — *pas* la moitié |
| pénètre dans le corps de | 54,97 px | **33,70 px** |

✅ **La conclusion qualitative tient dans les deux cas, et elle est importante** :
la prise de donnée est **entièrement dehors**, la prise d'exécution **chevauche
vraiment** le corps. C'est ce que Rodolf voulait dire par *« les prises
d'instruction doivent bien se marier au nœud »* — et **c'est une différence
voulue entre les deux familles, pas une inconsistance.**

⚠️ Mais le rapport « moitié » ne vaut **que pour la prise de donnée**. Pour la
prise d'exécution qui fait foi, c'est **0,41**, soit **8,6 × 10,9 px** à
l'échelle finale.

### 🔴 Ce que je signale avant d'appliquer : 2,9 px de large, ça se voit mal et se SAISIT mal

À l'échelle mesurée, la prise de donnée fait **2,9 px de large**. Le dessin de
Rodolf fait foi et je l'applique — mais deux réserves qui relèvent de l'usage,
pas du goût :

1. **une cible de 2,9 px est en dessous de ce qu'une souris attrape confortablement.**
   La parade est habituelle et ne change pas le dessin : **la zone sensible est
   plus large que la forme** (typiquement 10-12 px de large, centrée sur la
   prise). ✅ **À écrire dans la spécification, pas à corriger dans le dessin** ;
2. **à 55 % de zoom, 2,9 px tombe à 1,6 px** — la prise devient un trait. Le § 12.2
   prévoit déjà que les prises disparaissent au dézoom, donc **ça ne casse rien**,
   mais ça arrive **plus tôt** qu'avec des prises de 6 px.

🔴 **La question qui reste, et elle est pour Rodolf** : les planches doivent-elles
passer à **194 px de large avec des prises de 2,9 × 10,5** — c'est-à-dire devenir
fidèles à son dessin — ou garde-t-il des prises plus grosses pour la lisibilité,
auquel cas **c'est son nœud de référence qu'il faut retoucher** ? Les deux se
défendent ; **ce qu'on ne peut pas garder, c'est les deux à la fois.**

---

## A · Les briques de base

| # | élément | description de base | état |
|---|---|---|:---:|
| A1 | **corps de nœud** | rectangle, coins **3 px**, fond `#232329`, filet `#3a3a44` de 1 px, ombre portée très douce | ✅ |
| A2 | **en-tête** | bande de 21 px, **couleur = catégorie**, texte 12,5 px semi-gras clair, `?` d'aide aligné à droite | ✅ |
| A3 | **filet d'exécution** | trait plein de 2,5 px collé sous l'en-tête — **orange** `#F79A28` si le nœud exécute, **pétrole** `#0A555F` sinon | ✅ |
| A4 | **prise de donnée** | ❌ **PÉRIMÉ — voir A0.** Valeurs mesurées : **2,9 × 10,5 px** (soit **la moitié de la hauteur d'en-tête**), **entièrement à l'extérieur**, collée au bord. Couleur = type. ⚠️ Zone SENSIBLE plus large que la forme | ✅ §3.1 |
| A5 | **prise d'exécution** | ✅ **CONFIRMÉ** — rectangle **à pointe** (pentagone, points `{0,0} {0.583,0} {1,0.5} {0.583,1} {0,1}`). **8,6 × 10,9 px**, soit **0,41 × la hauteur d'en-tête** — *pas* la moitié, contrairement à la prise de donnée. ⚠️ **Elle CHEVAUCHE le corps** | ✅ §3.1 |
| A6 | **prise de tableau** | le rectangle A4 mais **creux** : contour de 2 px, intérieur vide | ✅ §3.3 |
| A7 | **pastille de type** | petit rectangle 19 × 15 px, rayon 2, fond teinté + glyphe court (`1.0`, `RGB`, `V/F`…) | ✅ §3.5 |
| A8 | **rangée de prise** | étiquette à gauche, champ de saisie à droite, hauteur ~24 px | ✅ |
| A9 | **champ de saisie** | rectangle en creux `#1b1b20`, filet `#33333c`, rayon 2, texte 10,5 px | ✅ |
| A10 | **liste déroulante** | comme A9 avec un `▾` collé à droite | ✅ |
| A11 | **nuancier de couleur** | rectangle plein de la couleur, filet fin — pas de dégradé en damier pour l'instant | ✅ |
| A12 | **bouton `+` / `−`** | glyphe seul, gris atténué, s'éclaire au survol | ✅ |
| A13 | **rangée « ajouter »** | rectangle en pointillés, texte centré atténué `+ ajouter…` | ✅ |
| A14 | **fond du canevas** | `#17171b` + grille de **points** de 1 px, pas 22 px, `#2b2b33` | ✅ |

---

## B · Les formes de nœud

| # | élément | description de base | état |
|---|---|---|:---:|
| B1 | **nœud de calcul** | en-tête + liste d'opération + N rangées + `+`/`−` + une sortie. Ex. `Math` | ✅ |
| B2 | **nœud d'instruction** | prises d'exécution **au-dessus** d'un filet séparateur, données en dessous. Ex. `Si / Sinon` | ✅ |
| B3 | **puce** | une seule rangée, coins très arrondis (13 px), pastille ronde colorée à gauche, sert de **source nommée** et de **relais** | ✅ |
| B4 | **nœud à charge variable** | l'éditeur **est** le nœud : barre de dégradé, courbe, vignette. Ex. `ColorRamp` | ✅ |
| B5 | **nœud de surface** | haut, sections repliables `▸`/`▾`, sortie d'un type à part. Ex. `Principled BSDF` | ✅ |
| B6 | **nœud source** | aucune entrée : le côté gauche est **vide**. Ex. `RGB`, `Value`. ✅ **DÉCIDÉ 23/08 — ne RIEN ajouter** : le côté vide est le seul trait de forme qui survit à 25 % | ✅ planche 07, §9.1bis |
| B7 | **nœud puits** | aucune sortie : le côté droit est **vide**, et il est unique. ✅ **DÉCIDÉ 23/08 — rien non plus** ; « plus imposant » refusé (une exception à la grille pour un doublon). 🟡 si plusieurs puits existent, marquer l'**ACTIF en positif** — un 4ᵉ gris n'aurait plus d'endroit libre | ✅ planche 07, §9.1bis |
| B8 | **nœud de groupe replié** | ✅ **TRANCHÉ 22/08** — *« un groupement de nœuds qu'on empaquette pour réutiliser à volonté comme des fonctions »*. Un nœud ordinaire, **pictogramme de pile** à gauche du titre, prises = les fils qui **traversaient la frontière de la sélection**, dédupliquées par source. Voir § 7.6 de la spécification | ✅ §7.6 |
| B8b | **nœuds d'interface** `Entrées du groupe` / `Sorties du groupe` | des nœuds **ordinaires**, vus à l'intérieur du groupe : l'un n'a aucune entrée, l'autre aucune sortie — donc **rien de neuf à dessiner**, ce sont B6 et B7 | ⬜ §7.6 |
| B9 | **commentaire** | juste du texte posé sur le canevas, sans corps de nœud. ✅ **DÉCIDÉ 23/08** : sans corps ni filet ; **devant les fils, derrière les nœuds** (devant ce qui décore, derrière ce qui s'édite) ; disparaît à 25 % | ✅ planche 07, §9.1 |
| B10 | **cadre** | ✅ **VALIDÉ 22/08** *(« tu as défini le bon cadre »)* — fond teinté à **8 %**, filet extérieur plein **1,5 px** `rx=6`, **second filet intérieur pointillé `4 4` à 8 px de retrait**, bandeau plein de **20 px** à coins hauts arrondis, titre **sombre sur la teinte**, compteur de nœuds **aligné à droite**. ⚠️ **Le double filet est ce qui le distingue d'un rectangle de fond — ne pas le simplifier** | ✅ §9.2 |
| B10b | **cadre replié** | le **bandeau seul** subsiste, préfixé `▸`, titre et compteur sur une ligne | ⬜ §9.2 |
| B10c | **cadre sans titre** | ⚠️ depuis l'option A, **le titre porte seul le sens du cadre**. Proposé : naît avec `Cadre` déjà sélectionné pour saisie ; effacé, affiche `Sans titre` en italique atténué — **jamais rien** | ⚠️ §9.2 |

---

## C · Les fils

| # | élément | description de base | état |
|---|---|---|:---:|
| C1 | **fil de donnée** | courbe douce, 2 px, **couleur du type**, part horizontalement du centre de la prise | ✅ §10 |
| C2 | **fil d'exécution** | même courbe, **3,5 px**, toujours orange, jamais coloré par un type | ✅ §10 |
| C3 | **fil de tableau** | **doublé** — deux traits de 1,4 px parallèles | ✅ §10 |
| C4 | **fil en cours de tirage** | pointillé gris, suit le curseur | ✅ §11.5 |
| C5 | **fil sélectionné** | ✅ **MESURÉ deux fois, DÉCIDÉ le 23/08.** **Les deux poignées carrées de 7 px sont le SEUL signal** — l'éclaircissement a été **retiré** : imperceptible isolément (5,23 contre un plancher de 11,0) **et** en comparaison simultanée, donc décor. ⚠️ **La poignée ne disparaît jamais** : quand la prise sort du cadre, elle se pose là où le fil COUPE le bord, en **chevron** pointant vers l'extérieur — il reste à l'écran, et sa pointe dit de quel côté est la prise. Voir § 10 | ✅ planche 06, §10 |
| C6 | **croisement de fils** | ✅ **INSTRUIT — ne rien faire.** Les fils se croisent ; aucune référence du corpus ne fait de saut, et le saut coûte n² paires à tester par image (19 900 à 200 fils). **N'empêche rien** — c'est du rendu pur | ✅ §10 |

---

## D · Les états

| # | élément | description de base | état |
|---|---|---|:---:|
| D1 | **prise compatible** (pendant le tirage) | halo clair de 1,5 px autour de la prise | ✅ |
| D2 | **prise incompatible** | éteinte, gris `#3a3a44`, ~30 % d'opacité | ✅ §11.5 |
| D3 | **prise convertible** | demi-teinte — sa couleur à 35 % | ✅ |
| D4 | **nœud sélectionné** | liseré clair de 1,6 px, **jamais** une teinte du corps | ✅ §11.4 |
| D5 | **nœud en erreur** | corps teinté rouge sombre, filet rouge, `!` dans l'en-tête, **raison lisible sans survol** | ✅ §11.1 |
| D6 | **nœud indisponible** | ✅ **DÉCIDÉ 23/08, conforme au § 8.4** : bibliothèque → 50 % + raison ; canevas → **hachures de CORPS**, prises et fils intacts. Aucune collision : l'inconnu porte des **rayures d'EN-TÊTE**, le désactivé un **fil en pointillé** | ✅ §8.4 |
| D7 | **nœud survolé** | ✅ **DÉCIDÉ 23/08, conforme au § 11.4** : **le FILET s'éclaircit** (`#33333C` → `#5A5A68`), jamais le corps. Un signal faible suffit : le curseur est déjà là | ✅ §11.4 |
| D8 | **entrée branchée** | son champ de saisie **disparaît** — c'est le fil qui décide | ✅ |
| D9 | **prise dépliée** | la donnée composée montre X/Y/Z en lignes séparées, **une seule prise** | ✅ |
| D10 | **section repliée / dépliée** | `▸` / `▾` en tête de section, à l'intérieur du nœud | ✅ |
| D11 | **dézoom, 3 paliers** | 100 % tout · 55 % les valeurs disparaissent · 25 % un rectangle de la catégorie | ✅ §12.2 |

---

## E · Le canevas

| # | élément | description de base | état |
|---|---|---|:---:|
| E1 | **recherche de nœud** | panneau qui s'ouvre quand on tire un fil dans le vide, ⚠️ **filtré par le type de la prise d'origine**. ✅ **DÉCIDÉ 23/08** : filtre écrit et retirable ; ❌ **liste vide INTERDITE** | ✅ planche 07, §12.1 |
| E2 | **menu contextuel** | clic droit sur le fond, sur un nœud, sur un fil — trois contenus. ✅ **DÉCIDÉ 23/08** : ⚠️ **« insérer un nœud ICI »** sur le fil (4 gestes → 1), et « ajouter un nœud » rouvre **le même panneau** sans filtre | ✅ planche 07, §12.5 |
| E3 | **minicarte** | ✅ **DÉCIDÉ 23/08 — reportée**, avec le critère qui la déclenchera : *le premier graphe réel de plus de 50 nœuds où Rodolf se perd*. `cadrer tout` sert le même besoin pour bien moins cher. **N'empêche rien** | ⬜ §12.3 |
| E4 | **barre d'outils** | ✅ **DÉCIDÉ 23/08 — aucune barre.** Les trois actions utiles (`cadrer tout`, `zoom 100 %`, **niveau de zoom en texte**) vont à droite de la bande d'E6, qui existe de toute façon : **coût de place nul** | ✅ planche 06, §12.3 |
| E5 | **sélection au lasso** | rectangle pendant le glissé. ✅ **DÉCIDÉ 23/08 — le pointillé est REFUSÉ** : déjà pris par le **cadre**, au même endroit, et tirer un rectangle EST le geste qui crée un cadre. Filet **plein** #F79A28 1,5 px + remplissage 8 % | ✅ planche 07, §12.4 |
| E6 | **fil d'Ariane** | ✅ **OBLIGATOIRE depuis le 22/08** — entrer dans un groupe pour l'éditer est le **seul** chemin ; sans lui on ne sait ni où l'on est ni comment sortir. `Matériau mur ▸ Bruit de surface ③`. ✅ **DÉCIDÉ 23/08** : bande de 24 px, segments cliquables, élision au milieu au-delà de 4 niveaux, et ⚠️ **un compteur d'instances** — on entre par UNE instance mais on édite la DÉFINITION, qui est PARTAGÉE | ✅ planche 06, §12.3 |

---

## F · Les couleurs à valider

**Ce sont les plus faciles à corriger et les plus visibles.** Panneau 12 de la
planche 01.

| catégorie | proposition |
|---|---|
| surface / BSDF | `#2a6b6b` — sarcelle |
| texture · couleur | `#8a6b2a` — ocre |
| outillage · maths | `#4a6b8a` — bleu ardoise |
| entrée / contexte | `#0A555F` — **pétrole Rihen** |
| flot · instruction | `#8a5a2a` — ambre sombre |
| variable · objet | `#6b4a8a` — violet |
| **sortie** (unique) | `#F79A28` — **orange Rihen** |
| erreur | `#8a3a30` — rouge sombre |

Et les **dix couleurs de type** des pastilles (réel, entier, booléen, vec2,
vec3, couleur, texte, shader, objet, quelconque) — même remarque.

---

## ⚠️ Ce que je te conseille, si tu dessines toi-même

**Ne dessine pas les 40 lignes.** Dessine **A1 à A14** — les briques — et
**deux** formes de nœud complètes. Tout le reste s'en déduit, et je peux
construire les autres à partir de tes briques.

C'est aussi ce qui te coûte le moins : les briques sont là où toutes les
décisions se prennent (rayon, épaisseur, contraste, espacement). Les nœuds n'en
sont que des assemblages.

---

# 🔴 SÉANCE DE DÉCISIONS — les sept ⚠️ instruits (22/08, à l'attention de Rodolf)

> Chaque point tient en une minute de lecture : **une** proposition, ce qu'elle
> coûte, ce qu'elle empêche. Pas d'éventail — un éventail renvoie la décision.
>
> ⚠️ **Deux des sept étaient déjà INSTRUITS ailleurs** — la spécification les avait
> traités sans que ce document soit mis à jour. 📌 **Correction du 23/08 : j'avais
> écrit « déjà tranchés », c'était trop fort.** Les § 8.4 et 11.4 sont marqués
> **PROPOSÉ**, pas DÉCIDÉ : ils demandent une **décision**, pas une confirmation.
> La différence est mince en pratique et elle compte sur le statut.

---

## E6 · Fil d'Ariane — ⚠️ **le seul qui bloque une fonctionnalité décidée**

**Il n'est plus optionnel** : entrer dans un groupe est le **seul** moyen de
l'éditer, et sans fil d'Ariane on ne sait ni où l'on est, ni comment sortir.

### La proposition

**Une bande de 24 px en haut du canevas, toujours présente**, portant le chemin
d'**instanciation** — pas le chemin de définition :

`Matériau mur ▸ Bruit de surface ⑶`

- **chaque segment est cliquable** et remonte d'un niveau ;
- ⚠️ **le dernier segment porte un compteur d'instances quand il y en a plus
  d'une** (`⑶`). **C'est le point important, et il ne relève pas de la
  décoration** : on entre par *une* instance, mais **on édite la DÉFINITION, qui
  est partagée**. Sans ce compteur, l'utilisateur croit corriger un nœud et en
  corrige trois. C'est le seul endroit de l'interface où ce fait peut se dire au
  moment où il compte ;
- au-delà de 4 niveaux, **on élide au milieu** (`Racine ▸ … ▸ Bruit ▸ Détail`),
  jamais au début ni à la fin — l'origine et l'endroit où l'on est sont les deux
  seules choses qu'on cherche.

📌 **Le modèle le porte déjà** : `NkEvalStep::path` produit exactement
`racine/Instance 1`. Rien à inventer côté données.

### Ce que ça coûte

| | |
|---|---|
| dessin | une bande, des segments, un séparateur `▸`. **Rien de neuf** |
| code | une pile de navigation (nom du graphe + nœud d'instance emprunté), et un compteur qui n'est qu'un dénombrement des nœuds `graph.instance` nommant ce graphe |
| cas à couvrir | profondeur > 4 (élision) · nom de graphe vide · **le groupe supprimé pendant qu'on est dedans** — il faut remonter, pas afficher un graphe fantôme |

### Ce que ça empêche

⚠️ **Un fil d'Ariane oriente vers l'édition « SUR PLACE »**, et rend la variante
« un onglet par groupe » largement redondante. **C'est donc une réponse partielle
au NON TRANCHÉ n° 6.** Si Rodolf veut des onglets, il faut le dire maintenant —
après, les deux cohabiteraient mal.

---

## E4 · Barre d'outils — ✅ **absorbée par E6, elle ne coûte plus rien**

### La proposition

**Pas de barre d'outils permanente.** À la place : **les trois seules actions qui
ne s'apprennent pas au clavier vont à DROITE de la bande du fil d'Ariane**, qui
existe de toute façon :

`cadrer tout` · `zoom 100 %` · **le niveau de zoom en texte** (`55 %`)

### Pourquoi

1. **la référence principale n'a aucune barre d'outils** ;
2. le canevas est la surface la plus précieuse — chaque bande permanente la
   ronge, et **une seule est déjà justifiée** ;
3. **chaque bouton d'une barre est un raccourci qu'on n'apprend jamais.**

📌 **Le niveau de zoom en texte n'est pas décoratif** : le § 12.2 fait dépendre
trois paliers d'affichage du zoom. Quand un nœud « perd » ses valeurs,
l'utilisateur doit pouvoir lire **pourquoi** au lieu de croire à un bug.

### Ce que ça coûte / empêche

Trois boutons dans une bande déjà nécessaire — **coût de place nul**. ⚠️ S'il
faut dix outils un jour, il faudra une vraie barre — mais **on l'aura appris à ce
moment-là**, ce qui est le bon moment pour la dessiner.

---

## C6 · Croisement de fils — ✅ **ne rien faire, et c'est défendable**

### La proposition

**Les fils se croisent, simplement.** Pas de saut, pas de passage dessous.

### Pourquoi

1. **aucune des douze références du corpus ne fait de saut.** Ni Blender, ni
   Unreal, ni la principale ;
2. **le saut vient des schémas électriques, où les fils sont ORTHOGONAUX** — deux
   segments perpendiculaires qui se coupent sont réellement ambigus. **Nos fils
   sont des courbes de directions différentes : l'œil les suit sans aide** ;
3. **le coût est réel et croît en n²** — nombre de paires à tester :

| fils à l'écran | paires à tester |
|---|---|
| 20 | 190 |
| 50 | 1 225 |
| **200** | **19 900** |

   …et il faut le refaire **à chaque déplacement de nœud**, donc potentiellement
   à chaque image.

### Ce que ça empêche

**Rien.** Le saut est **purement du rendu** : s'il manque un jour, il s'ajoute
sans toucher au modèle ni à la spécification. C'est exactement ce qu'il ne faut
pas payer avant d'en avoir besoin.

---

## E3 · Minicarte — ✅ **reporter, avec un critère de déclenchement écrit**

### La proposition

**Pas de minicarte pour l'instant**, et **ce n'est pas un refus** : c'est un
report avec la condition qui le lèvera.

### Pourquoi

Le besoin réel derrière « minicarte » est **se repérer et revenir**. Il est mieux
servi, pour bien moins cher, par **`cadrer tout` (une touche)** et par la
recherche de nœud. Une minicarte demande de **redessiner le graphe entier dans un
coin** à chaque image, ou de tenir un cache qu'il faut invalider à chaque
déplacement — pour un gain qui n'existe **qu'une fois le graphe bien plus grand
que l'écran**.

🔴 **Le critère qui la déclenchera, écrit maintenant pour ne pas être discuté plus
tard** : *le premier graphe réel qui dépasse 50 nœuds, et dans lequel Rodolf se
perd.* Pas avant.

### Ce que ça empêche

**Rien** — c'est un panneau flottant indépendant, ajoutable à tout moment.

---

## D6 · Nœud indisponible — 🟡 **DÉJÀ INSTRUIT au § 8.4, il reste à DÉCIDER**

Ce document le laissait ⚠️, mais la spécification l'a résolu depuis :

- **dans la bibliothèque** : présent, **50 % d'opacité**, raison en seconde ligne
  (`nécessite un tracé de rayons`). *Présent, parce qu'un nœud absent se cherche
  indéfiniment ; un nœud refusé se comprend une fois* ;
- **posé sur le canevas** (fichier importé) : **corps hachuré à 45°**, en-tête à
  50 %, **prises intactes et fils conservés** ;
- **la raison est écrite dans le nœud**, pas en infobulle.

✅ **Et j'ai vérifié la collision qui m'inquiétait** : le nœud **inconnu** (§ 8.3)
porte des **rayures d'EN-TÊTE**, l'**indisponible** des **hachures de CORPS**, le
**désactivé** un corps à 45 % avec **fil traversant en pointillé**. **Trois
causes, trois responsables, trois signaux distincts** — aucun recouvrement.

**Coût** : le motif `hachure` existe déjà dans `gen.py`. **Empêche** : rien.

---

## D7 · Nœud survolé — 🟡 **DÉJÀ INSTRUIT au § 11.4, il reste à DÉCIDER**

**Le filet du corps s'éclaircit** (`#33333C` → `#5A5A68`). **Jamais le corps** —
le teinter fait « clignoter » le canevas quand on le traverse à la souris.

📌 **Un signal faible suffit ici, et c'est un argument, pas une concession** : le
survol est **transitoire et attaché au curseur**. L'utilisateur sait déjà où est
sa souris. **C'est la sélection qui doit survivre au déplacement du curseur**, et
elle a une bordure orange de 1,6 px.

⚠️ **Ce que le § 11.4 a trouvé et que ce document ignorait encore** : le corpus
montre **trois** états là où nous en prévoyions deux — survolé, **sélectionné**
(bordure) et **actif** (en-tête éclairci de 15 %). *Un éditeur qui ne sépare pas
les deux derniers ne sait pas à qui envoyer le clavier quand douze nœuds sont
sélectionnés.*

---

## C5 · Fil sélectionné — ⚠️ **une proposition, et je dis sa faiblesse**

C'est **le seul des sept pour lequel je n'ai aucune référence**, et je ne vais pas
faire semblant d'en avoir.

### Ce qui contraint le choix, et c'est mesurable

Les fils utilisent déjà **trois canaux sur quatre** :

| canal | déjà pris par |
|---|---|
| **couleur** | le type de la donnée |
| **épaisseur** | donnée 2 px / **exécution 3,5 px** |
| **doublement** | le tableau (C3) ; le dictionnaire y ajoute un pointillé |
| *luminosité* | **libre** |

⚠️ **C'est ce qui disqualifie l'idée évidente — épaissir le fil sélectionné** :
un fil de donnée à 2 px épaissi de 60 % donne **3,2 px**, soit *l'épaisseur d'un
fil d'exécution non sélectionné* (3,5). **On fabriquerait une confusion entre
deux familles pour signaler un état.**

### La proposition

**Le fil sélectionné s'éclaircit fortement (+35 % de luminosité) et porte deux
petites poignées carrées à ses extrémités.** L'épaisseur ne change pas.

- l'éclaircissement utilise **le seul canal libre**, et conserve la couleur de
  type — l'information ne se perd pas ;
- les deux poignées **désignent les deux prises reliées**, ce qui est exactement
  ce qu'on cherche en sélectionnant un fil dans un graphe dense.

### Ce que ça coûte / empêche

Deux petits carrés et un éclaircissement. Cas à couvrir : **plusieurs fils
sélectionnés** (les poignées s'accumulent) et **fil très court**, où les deux
poignées se touchent.

⚠️ **Ce que je ne peux pas garantir** : que +35 % soit visible sur les six
familles de couleur. `#9AA3AD` (quelconque) est déjà clair — il pourrait saturer.
**Ça se mesure sur une planche, pas dans un document**, et ça n'a pas été fait.

---

## Récapitulatif pour la séance

| # | quoi | ce qu'il faut de Rodolf |
|---|---|---|
| **E6** | fil d'Ariane, avec **compteur d'instances** | ✅ valider — **et dire s'il veut des onglets**, ce qui changerait tout |
| **E4** | trois actions dans la bande d'E6, pas de barre | ✅ valider |
| **C6** | les fils se croisent, sans saut | ✅ valider |
| **E3** | minicarte reportée, critère écrit | ✅ valider le critère (50 nœuds) |
| **D6** | déjà **proposé** § 8.4 | 🟡 **décider** — la proposition est écrite, elle attend un oui |
| **D7** | déjà **proposé** § 11.4 | 🟡 **décider** — idem |
| **C5** | éclaircissement + poignées | ⚠️ **le seul vrai choix de goût** — et il demande une planche pour être jugé |

📌 **Rien de tout ceci n'est dessiné.** C'était volontaire : le dessin vient après
les réponses, sinon on dessine sept fois.
