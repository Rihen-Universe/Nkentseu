# Éléments à dessiner — inventaire, avec description de base

> Écrit le 2026-08-22 à la demande de Rodolf : *« ce serait bien de connaître tout
> ceux que je vais designer avec leurs description visuelle de base que je
> pourrais modifier »*.
>
> Chaque ligne = **un élément vectoriel**. La description est une **proposition à
> corriger**, pas une décision. La `planche_01_noeuds.svg` en dessine déjà une
> bonne partie — reprends-la ou repars de zéro.

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

## A · Les briques de base

| # | élément | description de base | état |
|---|---|---|:---:|
| A1 | **corps de nœud** | rectangle, coins **3 px**, fond `#232329`, filet `#3a3a44` de 1 px, ombre portée très douce | ✅ |
| A2 | **en-tête** | bande de 21 px, **couleur = catégorie**, texte 12,5 px semi-gras clair, `?` d'aide aligné à droite | ✅ |
| A3 | **filet d'exécution** | trait plein de 2,5 px collé sous l'en-tête — **orange** `#F79A28` si le nœud exécute, **pétrole** `#0A555F` sinon | ✅ |
| A4 | **prise de donnée** | **rectangle 10 × 12 px, rayon 2, à cheval sur le bord** — la moitié dépasse. Couleur = type | ✅ |
| A5 | **prise d'exécution** | même rectangle mais **à pointe** vers la droite — même famille, silhouette distincte | ✅ |
| A6 | **prise de tableau** | le rectangle A4 mais **creux** : contour de 2 px, intérieur vide | ✅ |
| A7 | **pastille de type** | petit rectangle 19 × 15 px, rayon 2, fond teinté + glyphe court (`1.0`, `RGB`, `V/F`…) | ✅ |
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
| B6 | **nœud source** | aucune entrée : le côté gauche est **vide**. Ex. `RGB`, `Value` | ⬜ |
| B7 | **nœud puits** | aucune sortie : le côté droit est **vide**, et il est unique. `Material Output` | ⬜ |
| B8 | **nœud de groupe replié** | ⚠️ un nœud qui en contient d'autres, avec ses propres entrées/sorties | ⚠️ |
| B9 | **commentaire** | juste du texte posé sur le canevas, sans corps de nœud | ⬜ |
| B10 | **cadre** | rectangle teinté à 7 % d'opacité, filet de la même teinte, titre en haut à gauche | ✅ |

---

## C · Les fils

| # | élément | description de base | état |
|---|---|---|:---:|
| C1 | **fil de donnée** | courbe douce, 2 px, **couleur du type**, part horizontalement du centre de la prise | ✅ |
| C2 | **fil d'exécution** | même courbe, **3,5 px**, toujours orange, jamais coloré par un type | ✅ |
| C3 | **fil de tableau** | **doublé** — deux traits de 1,4 px parallèles | ✅ |
| C4 | **fil en cours de tirage** | pointillé gris, suit le curseur | ✅ |
| C5 | **fil sélectionné** | ⚠️ plus clair ? un halo ? | ⚠️ |
| C6 | **croisement de fils** | ⚠️ un fil passe-t-il par-dessus, par-dessous, avec un saut ? | ⚠️ |

---

## D · Les états

| # | élément | description de base | état |
|---|---|---|:---:|
| D1 | **prise compatible** (pendant le tirage) | halo clair de 1,5 px autour de la prise | ✅ |
| D2 | **prise incompatible** | éteinte, gris `#3a3a44`, ~30 % d'opacité | ✅ |
| D3 | **prise convertible** | demi-teinte — sa couleur à 35 % | ✅ |
| D4 | **nœud sélectionné** | liseré clair de 1,6 px, **jamais** une teinte du corps | ✅ |
| D5 | **nœud en erreur** | corps teinté rouge sombre, filet rouge, `!` dans l'en-tête, **raison lisible sans survol** | ✅ |
| D6 | **nœud indisponible** | ⚠️ le rang interdit (`Light Path`…) — présent mais refusé, avec sa raison | ⚠️ |
| D7 | **nœud survolé** | ⚠️ éclaircissement du corps ? du filet ? | ⚠️ |
| D8 | **entrée branchée** | son champ de saisie **disparaît** — c'est le fil qui décide | ✅ |
| D9 | **prise dépliée** | la donnée composée montre X/Y/Z en lignes séparées, **une seule prise** | ✅ |
| D10 | **section repliée / dépliée** | `▸` / `▾` en tête de section, à l'intérieur du nœud | ✅ |
| D11 | **dézoom, 3 paliers** | 100 % tout · 55 % les valeurs disparaissent · 25 % un rectangle de la catégorie | ✅ |

---

## E · Le canevas

| # | élément | description de base | état |
|---|---|---|:---:|
| E1 | **recherche de nœud** | panneau qui s'ouvre quand on tire un fil dans le vide, ⚠️ **filtré par le type de la prise d'origine** | ⬜ |
| E2 | **menu contextuel** | clic droit sur le fond, sur un nœud, sur un fil — trois contenus différents | ⬜ |
| E3 | **minicarte** | ⚠️ en veux-tu une ? où ? | ⚠️ |
| E4 | **barre d'outils** | ⚠️ zoom, cadrer tout, aligner, replier — ou rien du tout ? | ⚠️ |
| E5 | **sélection au lasso** | rectangle en pointillés pendant le glissé | ⬜ |
| E6 | **fil d'Ariane** | ⚠️ quand on entre dans un sous-graphe, comment revient-on ? | ⚠️ |

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
