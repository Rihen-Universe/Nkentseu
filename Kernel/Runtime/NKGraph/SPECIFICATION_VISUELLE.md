# Spécification visuelle de l'éditeur nodal

> Écrit le 2026-08-22 à la demande de Rodolf : *« définir tous les types de nœud
> possibles avec leurs différents éléments, même les nœuds custom, et aussi tu
> donnes les couleurs des types atomiques et des types composés, je veux la
> totale — pense aussi aux tableaux, aux dictionnaires […] sans oublier le cas
> des textures et des matériaux […] même les commentaires et les blocks. »*
>
> Compagnon de `CATALOGUE_NOEUDS.md` (ce qui existe), de
> `DESIGN_EDITEUR_NODAL.md` (la référence de style) et de
> `ELEMENTS_A_DESSINER.md` (l'inventaire). **Ce document-ci est celui qui
> tranche**, ou qui dit explicitement qu'il ne tranche pas.

---

## ⚠️ RÈGLE ZÉRO — **les ratios font foi, les pixels ne font foi de rien**

> Écrit le 22/08 après mesure. **À lire avant tout le reste**, parce que c'est la
> règle qui détermine comment lire tous les nombres de ce document.

**Toute valeur en pixels écrite ici, ou relevée sur une planche, ou lue dans le
`.sketch` de Rodolf, est une valeur À UNE ÉCHELLE DONNÉE. Seuls les RAPPORTS se
transportent.**

Les trois échelles en présence, et aucune n'est « la bonne » dans l'absolu :

| document | échelle | pourquoi |
|---|---|---|
| **le `.sketch` de Rodolf** | **× 6** | c'est une **maquette d'étude** : il l'a agrandie pour pouvoir dessiner |
| **les planches 01 à 05** | **≈ × 2,1** pour les prises | à l'échelle 1, `tableau` et `dico` deviennent **indistinguables** — mesuré |
| **l'éditeur** | **× 1** | en-tête 21 px, prise 2,9 × 10,5 |

### Les trois conséquences, et elles sont des règles

1. ✅ **Les RATIOS font foi.** `prise_hauteur / en-tête_hauteur = 0,5000` est une
   décision de dessin. `prise = 63,3 px` n'en est pas une : c'est cette décision
   **à l'échelle de la maquette**. ⚠️ **Quand un nombre de ce document contredit un
   ratio, c'est le ratio qui a raison.**
2. ✅ **Chaque planche PORTE sa mention d'échelle**, en pied :
   *« ⚠ PLANCHE D'ÉTUDE — les prises y sont dessinées à environ 2,1 × leur échelle
   relative pour rester lisibles. Les RATIOS de la spécification font foi, jamais
   les pixels de cette planche. »*
   ⚠️ **Ce n'est pas une excuse, c'est le mécanisme.** Sans elle, quelqu'un
   mesurera un jour un pixel sur une planche et en fera une valeur. Avec elle,
   **la planche dit elle-même qu'elle ne doit pas être mesurée** — même dessin
   qu'un témoin d'instrument : **l'outil déclare ce qu'il ne sait pas faire.**
3. ✅ **Le seul document qui doit être à l'échelle 1 est celui qui montre
   l'ÉDITEUR.** Il n'existe pas encore ; **s'il en faut un, il sera à part et il
   le dira**. On ne mélange pas une planche d'étude et une maquette d'interface.

📌 **Pourquoi on ne « corrige » pas les planches à l'échelle 1** : essayé en bac
à sable, PNG rendu et regardé. Les segments du `tableau` tombent à **2,17 px**, la
demi-largeur du `dico` à **0,65 px**, et le panneau qui **enseigne cette
distinction** ne la montre plus. **Une planche qui perd ce qu'elle enseigne n'est
pas plus juste, elle est inutile.** Et l'échelle × 2,1 coûterait une refonte
complète de mise en page **pour rendre une propriété que personne ne lira sur une
planche.**

✅ **L'argument décisif est de Rodolf** : il a dessiné son nœud à **6 ×**. Il
savait qu'il produisait un document d'étude. **On ne rend pas les planches
« exactes » contre l'intention de celui qui a posé la référence.**

---

## 0. Comment lire ce document — trois niveaux, jamais mélangés

Chaque affirmation porte une étiquette. **C'est la règle la plus importante du
document** : une spécification qui mélange l'observé et l'inventé se fait
appliquer comme si tout était observé.

| étiquette | ce que ça veut dire |
|---|---|
| **VU** | relevé dans une référence, avec le nom du fichier. Ce n'est pas une opinion. |
| **DÉCIDÉ** | arbitrage déjà rendu par Rodolf. Non rouvert ici. |
| **PROPOSÉ** | ma proposition, **avec sa raison**. Modifiable. |
| 🔴 **NON TRANCHÉ** | aucune référence ne le montre et je n'ai pas de raison défendable. **Ne pas le deviner au codage.** |

Le relevé brut, image par image, est dans `echanges/design.questions.md`, entrée
`Q1`. Ce document en est la **synthèse** ; il ne le remplace pas.

### Périmètre du corpus

12 références distinctes dans `references/` (13 fichiers, dont
`Screenshot 2026-07-17 174628.png` qui est **le doublon exact** de
`PRINCIPALE_flux_automatisation.png`). `planche_01_noeuds.svg/.png` est **notre
production**, pas une référence. `images (1).jpg` **ne montre aucun éditeur
nodal** — c'est une capture de l'éditeur Unreal avec ses 7 zones numérotées ;
je la traite comme une contrainte de cohérence avec l'inspecteur, pas comme une
référence de style (question posée en `Q1`).

---

## 1. Le socle géométrique

### 1.1 Les trois valeurs du fond — **VU**, et mesurées

Échantillonnées sur `PRINCIPALE_flux_automatisation.png` (couleur dominante
d'une zone pleine, pas un pixel isolé) :

| rôle | valeur | mesure |
|---|---|---|
| fond du canevas | `#121212` | dominante sur 120 × 60 px de fond nu |
| corps du nœud | **`#212121`** | dominante sur 120 × 10 px de corps nu |
| contrôle (champ, bouton, bandeau cliquable) | **`#2B2B2B`** | dominante du champ `Enter path…` et du bandeau `+ Click to add data blocks` |
| bandeau de section repliable | `#262626` | dominante du bandeau `1 Variables` |
| en-tête neutre (nœud qui n'exécute pas) | `#2B2B2B` | dominante de l'en-tête `Evaluate` |

> ✅ **Le `#212121` choisi par Rodolf est exactement le corps de la référence
> principale.** Son dessin est déjà aligné ; il n'y a rien à arbitrer.

⚠️ **Correction à `ELEMENTS_A_DESSINER.md` A9.** Ce document annonce un champ
« en creux `#1b1b20`, plus sombre que le corps ». **La mesure dit le contraire** :
le champ est `#2B2B2B`, soit **10 niveaux au-dessus** du corps. La hiérarchie de
la référence est **montante** — fond `#121212` < corps `#212121` < contrôle
`#2B2B2B` — et elle a une raison : sur un fond aussi sombre, un creux plus sombre
que le corps se confond avec le fond dès qu'on dézoome. **PROPOSÉ : adopter la
hiérarchie montante et corriger A9.** (Question ouverte en `Q1`, point 1.)

### 1.2 Rayons, épaisseurs, pas — **DÉCIDÉ** + **VU**

| élément | valeur | source |
|---|---|---|
| rayon du **corps** | **0** | DÉCIDÉ (Rodolf) — appuyé par `174000` et `174227`, qui sont à rayon 0 |
| rayon de l'**en-tête** | **5** (coins hauts seulement) | DÉCIDÉ (Rodolf) |
| filet du corps | 1 px, `#33333C` | PROPOSÉ — il faut *une* séparation avec le fond quand deux nœuds se touchent |
| filet d'exécution sous l'en-tête | **2,5 px** | DÉCIDÉ |
| grille | **points** de 1 px, pas 22 px, `#1C1C1C` | VU (principale : points, pas ~11 px à son échelle) |

⚠️ **La grille est faite de POINTS, pas de lignes** — VU sur la principale.
`174227`, `174000`, `images (2)` et `images (5)` utilisent des lignes ; la
principale gagne, c'est elle le style.

### 1.3 La règle de contraste qui commande tout le reste — **PROPOSÉ**

**Trois plans, jamais plus** : fond, corps, contrôle. Tout élément nouveau doit
se ranger dans l'un des trois. Une quatrième valeur de gris introduit une
hiérarchie que personne ne peut lire — c'est exactement le défaut de
`83576832-…png`, qui a deux teintes de corps *et* deux teintes de colonne, et
qui est la référence la plus difficile à lire du corpus.

---

## 2. Anatomie complète du nœud — tous les micro-éléments

De haut en bas. Chaque ligne est un élément vectoriel dessinable.

```
  ┌───────────────────────────────────────────┐
  │ ▼  Multiplier                          ?  │  ← en-tête (catégorie)
  │    Maths                                  │  ← sous-titre (famille)
  ├═══════════════════════════════════════════┤  ← filet d'exécution
  │  ░░░░░░░░░ aperçu ░░░░░░░░░░░░░░░░░░░░░░  │  ← bloc d'aperçu (escamotable)
  │  ▾ Section                                │  ← en-tête de section
  │ ▌ 1.0 Valeur A          [ 0.500 ]         │  ← rangée de prise
  │ ▌ 1.0 Valeur B             branchée       │  ← rangée branchée (champ retiré)
  │      + ajouter une entrée                 │  ← rangée d'ajout
  │                        Résultat  1.0 ▐    │  ← rangée de sortie
  │  dernière évaluation : 3.5                │  ← ligne d'état
  └───────────────────────────────────────────┘
```

### 2.1 L'en-tête

| micro-élément | spécification | niveau |
|---|---|---|
| **bande** | hauteur 21 px (28 px avec sous-titre), pleine, **couleur = catégorie** | DÉCIDÉ |
| **marqueur de repli `▼`** | triangle plein, 8 px, **collé à gauche dans l'en-tête**, avant le titre. `▼` = déplié, `▶` = replié | VU (`images (3)`, `images (4)` : sur *tous* les nœuds) |
| **titre** | 12,5 px semi-gras, clair, aligné à gauche après le `▼` | VU (principale) |
| **sous-titre** | 9,5 px, **50 % d'opacité**, sur une seconde ligne sous le titre. Porte la **famille** (`Maths`, `Transform`) quand le titre porte l'**opération** (`Multiplier`, `GetWorldX`) | VU (`174227`) |
| **`?` d'aide** | cercle discret 11 px, **toujours à l'extrême droite** | VU (principale : `Evaluate`, `If`) |
| **pictogramme de famille** | 12 px, optionnel, **entre le `▼` et le titre** | VU (`174000` le met à droite, `174057` à gauche). **PROPOSÉ : à gauche** — à droite il entre en conflit avec le `?` et avec la prise de sortie d'en-tête |
| **prise de sortie sur l'en-tête** | pour un nœud à **sortie unique**, la sortie se pose à l'extrême droite de l'en-tête | VU (`images (3)`, `images (4)`) — **PROPOSÉ : on ne le reprend pas** (voir § 3.4) |

**Le sous-titre n'est pas décoratif.** `Multiplier / Maths` et `Multiplier /
Vecteur` sont deux nœuds différents que le seul titre confond. C'est la raison
pour laquelle `174227` le porte sur *tous* ses nœuds.

### 2.2 Le filet d'exécution — **DÉCIDÉ**

Trait plein de 2,5 px collé sous l'en-tête, sur toute la largeur :

- **orange `#F79A28`** — le nœud **exécute** (il a au moins une prise d'exécution) ;
- **pétrole `#0A555F`** — le nœud **n'exécute pas** (il calcule).

⚠️ **Un nœud de matériau porte donc TOUJOURS le filet pétrole**, sans exception
(§ 6.4). C'est le seul indice qui reste quand on a dézoomé au point de ne plus
lire les prises, et c'est ce qui permet la lecture en diagonale d'un blueprint.

### 2.3 Le corps

| micro-élément | spécification | niveau |
|---|---|---|
| **bloc d'aperçu** | pleine largeur moins 8 px de marge, **immédiatement sous le filet, AVANT la première rangée**. Escamotable : quand il disparaît, les rangées remontent et le nœud raccourcit d'autant | VU (`images (4)`, les deux états côte à côte) |
| **en-tête de section** | bandeau `#262626` pleine largeur, `▾`/`▸` à gauche, libellé, **comptage optionnel à gauche du libellé** (`1 Variables`) | VU (principale) |
| **rangée** | hauteur 24 px. Étiquette à gauche, contrôle aligné à droite | VU |
| **rangée d'ajout** | `+ ajouter une entrée`, texte centré atténué sur bandeau `#2B2B2B` | VU (principale : `+ Click to add data blocks`) |
| **ligne d'état** | 10 px, atténuée, **en pied de corps**, hors des rangées. Porte la dernière valeur évaluée | VU (`83576832` : `Last Evaluation: True`, `null`) |
| **poignée de redimensionnement** | triangle strié 8 px au coin bas-droit, **uniquement sur les nœuds redimensionnables** (aperçu, commentaire, cadre) | VU (`83576832`) |

🔴 **NON TRANCHÉ — la largeur du nœud.** Aucune référence ne dit si elle est
fixe, si elle s'adapte au plus long libellé, ou si elle est redimensionnable à la
souris. Les trois existent dans le corpus. **À décider avant le codage**, parce
que ça change la sérialisation (une largeur libre doit être enregistrée).

### 2.4 Les contrôles de rangée — la liste complète

Tous sont alignés à droite, dans **une seule colonne de contrôle**. C'est ce qui
permet à l'œil de descendre le nœud en ligne droite.

| # | contrôle | apparence | niveau |
|---|---|---|---|
| 1 | **champ numérique** | rectangle `#2B2B2B`, rayon 2, texte 10,5 px aligné à droite | VU |
| 2 | **champ de texte** | idem, texte aligné à gauche, invite atténuée (`Entrer un chemin…`) | VU (principale) |
| 3 | **liste déroulante** | idem + `▾` collé à droite | VU (`83576832` : `Equal ▾`) |
| 4 | **nuancier de couleur** | rectangle plein 36 × 14, filet 1 px clair. **Damier sous la couleur si l'alpha < 1** | VU (`images (3)`, `images (4)` : rectangle cyan plein) ; damier **PROPOSÉ** — sans lui, un noir opaque et un transparent sont identiques |
| 5 | **case à cocher** | carré 12 × 12, filet 1,5 px ; coché = carré plein + `✓`. **Occupe la colonne de contrôle**, pas une colonne à part | VU (`images (3)` : `Invert ▢` ; `images (4)` : `Invert Absor ▢`) |
| 6 | **valeur en lecture seule** | **texte gris `#6A6A6A` aligné à droite, SANS cadre et SANS fond** | VU (`images (3)`, `images (4)`, `origami`) |
| 7 | **bouton à glyphe** | carré 18 × 18 `#2B2B2B`, glyphe centré | VU (principale : bouton chaîne) |
| 8 | **bouton pleine largeur** | bandeau `#2B2B2B`, texte centré | VU (`83576832` : `Run`, `Step`) |
| 9 | **champ multiligne** | zone `#2B2B2B` de hauteur libre, coloration syntaxique | VU (principale) |
| 10 | **poignée de réordonnancement** | 6 points en 2 × 3, **à GAUCHE de l'étiquette** | VU (principale) |
| 11 | **barre d'édition** | dégradé, courbe, rampe — occupe la largeur (§ 7.5) | VU (`planche_01`), pas dans le corpus |

⚠️ **6 est le micro-élément le plus facile à rater et le plus utile.** Une
**valeur affichée** et un **champ éditable** ne doivent pas se ressembler : le
champ a un fond, la valeur n'en a pas. Dans `images (4)`, `Roughness 0.001` n'est
pas éditable sur le nœud — il faut le panneau. Le dessin le dit sans un mot.

---

## 3. Les prises

### 3.1 Géométrie — **DÉCIDÉ**, et confirmée par la mesure

**Rectangle haut et étroit** (ratio 1 : 3,7), rayon 2. ⚠️ **Deux corrections
apportées par le relevé du 22/08 dans `editeur_nodal.sketch` :**

1. ❌ **elle n'est PAS à cheval.** Le corps commence à `x = 17,3` et la prise
   occupe `[0 ; 17,3]` : elle est **entièrement à l'extérieur, collée au bord**.
   ⚠️ **La prise d'EXÉCUTION, elle, chevauche vraiment** — 33,7 px à l'intérieur du
   corps. C'est une **différence voulue** entre les deux familles : *« les prises
   d'instruction doivent bien se marier au nœud »*.
2. ❌ **`17 × 63` n'est pas une taille finale.** Le `.sketch` est à l'échelle
   **1 unité = 1 pixel** (prouvé : la capture de référence y est posée à ses
   dimensions exactes, 751 × 473), et le nœud de Rodolf y fait **1 168,5 px de
   large** — **six fois** un nœud réel. C'est une **maquette d'étude**.

✅ **La règle à retenir n'est donc pas un nombre de pixels, c'est un rapport, et
il est exact : `63,3 / 126,6 = 0,500000`.**

> **La prise de donnée fait exactement la MOITIÉ de la hauteur de l'en-tête.**

| | rapport à l'en-tête | à l'échelle finale (en-tête 21 px) |
|---|---|---|
| prise de **donnée** | **0,1367 × 0,5000** | **2,9 × 10,5 px** |
| prise d'**exécution** | 0,351 × **0,409** — *pas* la moitié | **8,6 × 10,9 px** |
| largeur du corps | **9,23** | **194 px** |
| barre d'exécution | 0,087 | **1,8 px** |

🔴 **CONSÉQUENCE à trancher, et elle touche les cinq planches — mais pas comme
ce document l'a d'abord écrit.**

⚠️ **Correction du 22/08** : j'ai annoncé « la prise est 2,095 × trop grosse et le
nœud 1,140 × trop étroit ». **Les deux moitiés étaient mal dites.**

- ❌ **Le nœud n'est pas trop étroit.** J'avais comparé **un** nœud de 170 px :
  c'est le **second plus étroit sur 38**, et **87 % des nœuds des planches sont
  PLUS LARGES** que le ratio de Rodolf (médiane 270 px). ✅ **La largeur suit le
  CONTENU, pas l'en-tête** — il n'y a aucun écart de largeur à corriger.
- ⚠️ **Le `2,095` n'est pas une prise trop grosse, c'est un MÉLANGE D'ÉCHELLES** :
  les prises des planches sont à l'échelle **0,3476** du dessin de Rodolf, leur
  en-tête à l'échelle **0,1659**. Le rapport des deux **est** 2,095. Et `0,1659`
  n'est pas une erreur : **c'est l'échelle 1 de l'éditeur réel**.

🔴 **Ce qui reste à trancher est donc l'ÉCHELLE PROPRE DES PLANCHES**, à appliquer
une seule fois à **tous** les éléments. ⚠️ **Ramener simplement les prises à
2,9 × 10,5 a été essayé en bac à sable et REGARDÉ : les formes `tableau` et `dico`
deviennent indistinguables** (segments à 2,17 px, demi-largeur à 0,65 px) et la
distinction creux / plein disparaît — **la planche 02 perd exactement ce qu'elle
enseigne.** Détail et options : `ELEMENTS_A_DESSINER.md` § A0.

📌 **Le principe qui en sort, et il vaut au-delà de ce cas** : **une planche
adopte les RATIOS de la référence, jamais ses pixels.** Rodolf a lui-même dessiné
son nœud à **6 ×** la taille réelle — un document d'étude n'est pas une capture
d'écran de l'éditeur.

### ✅ DÉCIDÉ le 22/08 — on garde ses proportions, on élargit la ZONE SENSIBLE

🔄 **Décision réversible**, prise sur l'autorité déléguée (robustesse et
modularité) en l'absence de Rodolf. **Elle ne coûte rien à défaire** : si Rodolf
retouche son nœud de référence, **seul le facteur d'échelle change** et tout le
reste de ce paragraphe reste vrai.

**Le problème posé** : à l'échelle mesurée, la prise de donnée fait **2,9 px de
large** — plus fin que ce qu'une souris attrape confortablement.

**Ce qui a été pesé, et pourquoi ça ne remet pas le dessin en cause** : `17,3 ×
63,3` est un rapport de **1 : 3,66**. Ce n'est pas un carré rétréci, c'est une
**languette fine et haute** — un parti pris de dessin courant, qui se lit très
bien comme une bande colorée sur le bord. ⚠️ **Le problème n'est donc pas la
lisibilité, c'est la SAISIE** — et confondre les deux aurait conduit à grossir le
dessin pour régler un problème qui n'est pas dans le dessin.

**La règle :**

> **La zone sensible est plus large que la forme.** La prise se DESSINE à
> `2,9 × 10,5 px` et se SAISIT sur **10 à 12 px de large**, centrée sur elle et
> débordant des deux côtés du bord.

✅ **Trois raisons de préférer ça à une prise plus grosse :**

1. **ça ne touche pas au dessin** — donc ça ne périme aucune planche, aucune
   valeur, et ça ne se contredit avec rien ;
2. **la zone sensible est de toute façon nécessaire**, même avec des prises de
   6 px : un éditeur nodal utilisable a toujours une tolérance de saisie plus
   large que ses cibles ;
3. **elle est modulable indépendamment** : on peut l'élargir au toucher ou la
   réduire à la souris de précision **sans redessiner quoi que ce soit**.

⚠️ **Deux choses à tenir avec cette règle**, sans quoi elle se retourne :

- **les zones sensibles de deux prises voisines ne doivent pas se recouvrir.** À
  10-12 px de large pour un pas de rangée de 24 px, il reste de la marge
  verticalement — mais **deux prises sur le MÊME bord à moins de 12 px l'une de
  l'autre deviendraient indiscernables au clic**. C'est le seul cas à vérifier ;
- **la zone sensible ne se dessine JAMAIS**, même au survol. Ce qui s'éclaire au
  survol, c'est la **prise** — sinon l'utilisateur apprend une cible qui n'est pas
  celle qu'il vise.

🔴 **À confirmer par Rodolf** : soit les planches s'alignent sur son nœud, soit
il retouche son nœud de référence. **Ce qu'on ne peut pas garder, c'est les deux
à la fois.** En attendant, ses proportions font foi.

> **VU** : la principale utilise des barres verticales à coins arrondis, ~5 × 14
> px à son échelle (ratio 1 : 2,8), **exactement la même famille de forme**. Le
> choix de Rodolf est plus élancé mais du même dessin. Ce n'est pas une
> invention.

**Le fil part du milieu du bord extérieur, horizontalement** — VU sur la
principale (trois paires vérifiées).

### 3.2 Les deux familles de forme — **DÉCIDÉ**

| | forme | pourquoi |
|---|---|---|
| **valeur** | rectangle 17 × 63, rayon 2 | |
| **exécution** | même rectangle **terminé par une pointe** vers la droite | DÉCIDÉ (Rodolf a tranché contre le triangle) |

⚠️ **Il faut le dire franchement : aucune référence ne montre le rectangle à
pointe.** Le corpus propose **quatre** formes d'exécution différentes — barre
colorée identique aux données (principale), double chevron `»` (`174000`),
triangle plein `▶` (`174057`, `174227`), carré-dans-carré (`83576832`). Le
rectangle à pointe est un **ajout assumé**, et il est meilleur que les quatre
pour une raison précise : **il appartient à la même famille de forme que la
prise de valeur** (donc l'œil apprend un seul objet) **tout en s'en distinguant
par la silhouette** (donc il reste lisible en vision déficiente, où la couleur
seule échoue).

### 3.3 Creux et plein — **DÉCIDÉ**

**Creux = non branché. Plein = branché.**

> **VU trois fois, de trois façons différentes** : anneau creux → disque plein
> (`images (3)`, `images (4)`, `83576832`) ; prise sombre → prise vive
> (`origami`) ; et **Octane fait les deux à la fois** — la prise passe de creuse
> à pleine **et** l'étiquette passe du gris à l'orange.

**PROPOSÉ : on reprend les deux signaux ensemble**, comme Octane. Raison : la
prise fait 17 px de large ; à 55 % de zoom, creux et plein ne se distinguent
plus. La couleur de l'étiquette, elle, tient un palier de plus.

| état | prise | étiquette |
|---|---|---|
| non branchée | contour 2 px de la couleur du type, intérieur `#212121` | `#C8CCD4` |
| branchée | rectangle plein de la couleur du type | **`#F79A28`** |

⚠️ **Et une entrée branchée perd son champ de saisie** — DÉCIDÉ. La valeur
saisie n'a plus de sens quand un fil la remplace ; la laisser visible fait croire
qu'elle compte encore. C'est ce que fait Blender, et c'est ce que fait Octane
(`Absorption`, `Scattering` et `Medium` n'ont aucune valeur affichée).

### 3.4 Où vivent les prises — **DÉCIDÉ**

| | position |
|---|---|
| **entrée d'exécution** | **toujours unique**, sur le bord gauche de **l'en-tête** |
| **sortie d'exécution unique** | bord droit de l'en-tête, avec son libellé dans la bande |
| **sorties d'exécution multiples** | **dans le corps**, une rangée chacune, dès qu'il y en a plus d'une |
| **entrées de valeur** | bord gauche du corps, une par rangée |
| **sorties de valeur** | bord droit du corps, une par rangée |

> **VU** : `174227` met exactement ça — `▶` sur le bord gauche de l'en-tête,
> `Continue ▶` sur le bord droit de la même bande. C'est la référence la plus
> proche de notre cible blueprint.

**PROPOSÉ : la sortie de VALEUR ne se pose jamais sur l'en-tête**, contrairement
à Octane (`images (3)`, `images (4)`). Raison : l'en-tête est déjà occupé par le
`▼`, le titre, le sous-titre et le `?` ; et surtout, **si la valeur pouvait vivre
sur l'en-tête, l'en-tête cesserait d'être le lieu réservé à l'exécution** — or
c'est précisément ce qui permet de lire la ligne de vie en diagonale.

⚠️ **Une rangée peut porter une entrée à gauche ET une sortie à droite** — VU
(`174227` : `Value A [champ] … Set Var [champ] ■`). **PROPOSÉ : autorisé mais
jamais imposé** ; utile pour `Écrire variable` qui prend une valeur et la
renvoie.

### 3.5 La pastille de type — **DÉCIDÉ** (couleur **et** glyphe)

Petit rectangle 19 × 15, rayon 2, fond = couleur du type à 22 % d'opacité,
glyphe court centré en 8,5 px.

**Position — VU sur la principale** : **à gauche de l'étiquette pour une entrée**
(`123 Data`), **à droite pour une sortie** (`Result 123`). Autrement dit :
**toujours du côté de la prise.** Même règle pour la valeur affichée dans
`origami` (`8 Option` à gauche pour une sortie, `Bounciness 5` à droite pour une
entrée) — c'est une convention robuste, elle place l'information sur le chemin de
l'œil qui suit le fil.

---

## 4. Les types de donnée — couleur, glyphe, et la raison chiffrée

### 4.1 Le problème, mesuré avant d'être résolu

`CATALOGUE_NOEUDS.md` § 5quinquies pose la question : *« douze couleurs
discernables, c'est beaucoup »*. **Ce n'est pas « beaucoup », c'est impossible,
et on peut le chiffrer.**

J'ai construit un jeu de 16 couleurs (une par type), puis mesuré **toutes les
paires** en distance CIEDE2000, dans quatre conditions : vision normale,
**protanopie**, **deutéranopie**, **tritanopie** (simulation Viénot 1999).
Résultat : **plancher de 0,89** — `réel` et `couleur` sont **indiscernables** en
protanopie. Six autres paires sous 3.

Puis j'ai cherché, par sélection gloutonne max-min sur une grille de 1 080
couleurs candidates (toutes de clarté L\* entre 58 et 88, donc lisibles sur le
corps `#212121`), **le plus grand nombre de couleurs mutuellement distinctes**.
La courbe tombe vite :

| nombre de couleurs | distance minimale garantie |
|---|---|
| 2 | 53,0 |
| 3 | 13,2 |
| 4 | 12,9 |
| 5 | 11,4 |
| 6 | 10,5 |
| 7 | 9,4 |
| 8 | 9,4 |
| 9 | 8,4 |

**Le coude est à 6.** Au-delà, on paie chaque couleur supplémentaire par une paire
qu'on ne peut plus séparer.

> **Conclusion mesurée : on ne peut pas avoir plus de six ou sept couleurs de
> prise. Donc la couleur porte la FAMILLE, et le glyphe porte le TYPE EXACT.**
> Ce n'est pas un compromis esthétique, c'est ce que la mesure autorise.

Un second essai le confirme par la négative : donner à chaque membre d'une
famille sa propre clarté (vec2 clair → transformation sombre) fait **rechuter le
plancher inter-familles à 2,2** — le vert sombre de `matrice` rejoint l'orange
d'`exécution` en deutéranopie. **Une famille = une seule couleur, plate.**

### 4.2 La palette — **PROPOSÉ**, plancher mesuré à **11,0**

| famille | couleur | ce qu'elle regroupe | raison |
|---|---|---|---|
| **exécution** | **`#F79A28`** | l'ordre d'exécution | DÉCIDÉ — orange Rihen, et déjà la couleur du filet d'exécution : le même signal partout |
| **nombre** | **`#17B2EB`** | réel, entier, booléen | ce sont les trois types **inter-convertibles** : un booléen vaut 0 ou 1, un entier est un réel. Même famille de conversion = même couleur |
| **géométrie** | **`#C0EB81`** | vec2, vec3, vec4, matrice, transformation | tout ce qui a des **composantes** et vit dans l'espace |
| **texte** | **`#F2559B`** | chaîne | seul type dont on ne fait aucune arithmétique — il mérite d'être seul |
| **apparence** | **`#D9B6A3`** | couleur, texture, matériau, shader | tout ce qui **décrit une surface**. Terre cuite : c'est la matière |
| **référence** | **`#81EBEB`** | objet, entité, actif | ce qui **désigne** au lieu de contenir |
| **quelconque** | **`#9AA3AD`** | générique, résolu au branchement | gris neutre : **l'absence de type doit ressembler à l'absence de couleur** |

**Validation, tous modes de vision confondus :**

- **plancher = 11,0** (`texte` / `quelconque`, en protanopie) ;
- paire suivante 12,1, puis 13,3 — la distribution est régulière, il n'y a pas
  une paire limite isolée ;
- **contraste minimal sur le corps `#212121` = 5,0 : 1** (`texte`), tous les
  autres au-dessus de 6,3 : 1. Une prise de 17 px de large reste visible.
- `exécution` contre `géométrie` tombe à 11,4 en deutéranopie — **mais ces deux-là
  ne se confondent jamais, parce que leur prise n'a pas la même forme.** La
  couleur est un signal redondant pour ce couple, pas le seul.

⚠️ **Le rouge d'erreur** (`#E4443C`, § 11.1) est à **7,1** de `texte` en
tritanopie. C'est faible — mais l'erreur n'apparaît **jamais sur une prise** :
elle vit sur le filet et l'en-tête, à une tout autre échelle, avec un `!`. **À
surveiller si l'erreur descendait un jour au niveau de la prise.**

### 4.3 Les types atomiques

| type | famille / couleur | glyphe | ce qu'il montre non branché |
|---|---|---|---|
| **réel** | nombre `#17B2EB` | `1.0` | champ numérique, 3 décimales |
| **entier** | nombre `#17B2EB` | `12` | champ numérique, sans décimale |
| **booléen** | nombre `#17B2EB` | `V/F` | **case à cocher** (contrôle 5), jamais un champ |
| **texte** | texte `#F2559B` | `abc` | champ de texte, invite atténuée |

**PROPOSÉ — pourquoi `1.0` et `12` et pas `f` et `i`** : le glyphe montre **la
forme de la donnée**, pas son nom savant. Un artiste lit `1.0` sans avoir appris
ce qu'est un flottant. Même raison pour `V/F` plutôt que `bool`.

### 4.4 Les types composés

| type | famille / couleur | glyphe | ce qu'il montre non branché |
|---|---|---|---|
| **vec2** | géométrie `#C0EB81` | `XY` | 2 champs sur une ligne (plié) |
| **vec3** | géométrie `#C0EB81` | `XYZ` | 3 champs sur une ligne (plié) |
| **vec4** | géométrie `#C0EB81` | `XYZW` | 4 champs (plié) |
| **couleur** | apparence `#D9B6A3` | `RVB` | nuancier + damier si alpha |
| **matrice** | géométrie `#C0EB81` | `M4` | **rien** — pas de champ (§ 4.5) |
| **transformation** | géométrie `#C0EB81` | `TRS` | 3 lignes pliées : position, rotation, échelle |
| **tableau** | *couleur du contenu* | `[ ]` | § 5 |
| **dictionnaire** | *couleur du contenu* | `{ }` | § 5 |
| **référence d'objet** | référence `#81EBEB` | `OBJ` | nom de l'objet, ou `— aucun —` |
| **texture** | apparence `#D9B6A3` | `TEX` | § 6 |
| **matériau** | apparence `#D9B6A3` | `MAT` | § 6 |
| **shader** | apparence `#D9B6A3` | `SH` | **rien** — un shader ne se saisit pas |
| **quelconque** | quelconque `#9AA3AD` | `?` | **rien** tant qu'il n'est pas résolu |

⚠️ **`couleur` n'est PAS dans la famille géométrie, alors qu'un `vec3` et une
couleur RVB ont le même contenu.** C'est délibéré, et c'est déjà écrit dans
`CATALOGUE_NOEUDS.md` § 2 : *« une couleur a un espace colorimétrique, pas un
vecteur »*. Les brancher l'un sur l'autre sans le dire est la source d'erreur la
plus classique d'un graphe de matériau — un vecteur normalisé interprété comme du
sRGB. **Deux familles de couleur différentes rendent la faute visible avant le
rendu.**

### 4.5 Les types qui n'affichent rien — **PROPOSÉ**

`matrice`, `shader`, `quelconque` non résolu : **rangée avec sa prise et son
étiquette, colonne de contrôle vide.**

Raison : une matrice 4 × 4 saisie à la main sur un nœud, ce sont **seize champs**
— le nœud devient plus haut que l'écran pour une donnée que personne n'écrit à
la main. **PROPOSÉ : la matrice se produit (par un nœud `Composer transformation`)
ou se branche, elle ne se saisit pas.** Si Rodolf veut la saisie, elle va dans le
panneau latéral, jamais sur le nœud.

### 4.6 Plier / déplier une donnée composée — **DÉCIDÉ** (rappel)

`CATALOGUE_NOEUDS.md` § 5ter distingue **plier/déplier** (affichage, une seule
prise, aucun lien nouveau) de **séparer/recombiner** (structure, trois prises,
trois liens possibles). **La règle qui les sépare : est-ce que le nombre de liens
possibles change ?**

Ce que le dessin doit rendre visible, et qui n'est nulle part encore :

- **plié** → `Position  [0.0] [1.0] [0.0]` — **une** prise en face de la rangée ;
- **déplié** → `Position` puis trois sous-rangées `X` `Y` `Z` **indentées de
  12 px, sans prise propre**, et **toujours une seule prise**, centrée en face du
  libellé `Position` ;
- **séparé** → trois rangées de plein rang `Position X`, `Position Y`,
  `Position Z`, **chacune avec sa prise**, et **plus aucune prise sur
  `Position`**.

⚠️ **L'indentation sans prise est le signal.** C'est elle qui dit « je regarde le
contenu » plutôt que « j'ai trois branchements ». Sans elle, déplié et séparé sont
indistinguables — et l'utilisateur croit pouvoir brancher `Y` seul.

🔴 **NON TRANCHÉ, et déjà signalé au catalogue** : que devient un lien branché sur
`Position` quand on sépare ? Unreal le **casse**. L'alternative (le router vers
les trois) rend la recombinaison ambiguë. **C'est un arbitrage de modèle, pas de
dessin** — mais le dessin doit savoir lequel montrer.

---

## 5. Les tableaux et les dictionnaires — en détail

C'est la partie que Rodolf a nommée en premier, et c'est la moins couverte par le
corpus. **Une seule image du corpus montre quelque chose qui y ressemble** :
`origami_patcher.webp`, où `Option Picker` empile `174`, `120`, `58` sous sa
première rangée, sans étiquette, en gris atténué. Elle ne montre **ni la
longueur, ni le type contenu, ni le repli**. Tout le reste de cette section est
**PROPOSÉ**.

### 5.1 Le principe : un tableau n'est pas un type, c'est une FORME

**PROPOSÉ, et c'est la décision structurante de cette section.**

Un tableau de réels reste bleu `#17B2EB`. Un tableau de couleurs reste terre
cuite `#D9B6A3`. **La couleur dit toujours ce qu'il y a dedans ; c'est la forme
de la prise qui dit combien.**

Trois raisons, dans l'ordre de force :

1. **La mesure de § 4.1 l'impose.** Six couleurs disponibles ; en donner une au
   « tableau » et une au « dictionnaire » coûterait deux familles de type, et on
   perdrait quand même l'information « tableau **de quoi** ».
2. `CATALOGUE_NOEUDS.md` § 5bis l'exige explicitement : *« il faut pouvoir
   montrer le type contenu »*. Une couleur propre au tableau le rend impossible.
3. C'est **composable** : tableau de tableaux, dictionnaire de couleurs, tableau
   de références — la forme se compose, la couleur non.

### 5.2 L'apparence de la prise

| | dessin de la prise 17 × 63 | glyphe de pastille |
|---|---|---|
| **scalaire** | rectangle plein d'un seul tenant | `1.0` |
| **tableau** | **trois segments empilés**, séparés par deux fentes de 3 px au fond du corps | `[1.0]` |
| **dictionnaire** | **grille 2 × 3** : les trois segments, plus une fente verticale au milieu (la clé à gauche, la valeur à droite) | `{1.0}` |

Le glyphe **encadre celui du contenu** : `[1.0]` se lit « tableau de réels »,
`{abc}` « dictionnaire de textes », `[[1.0]]` « tableau de tableaux de réels ».
On lit la structure sans la nommer.

⚠️ **Ce que ça donne au dézoom, et c'est le point faible à assumer** : à 55 %,
les fentes de 3 px disparaissent — **un tableau redevient visuellement un
scalaire**. Le fil, lui, tient un palier de plus (§ 5.3). **PROPOSÉ : c'est
acceptable**, parce qu'à 55 % on ne branche plus, on lit la structure ; et parce
que la seule alternative — une couleur propre — coûte plus cher (§ 5.1).

### 5.3 L'apparence du fil

| | trait |
|---|---|
| **valeur** | trait simple, 2 px, couleur du type |
| **tableau** | **trait double** — deux traits de 1,4 px espacés de 2,5 px |
| **dictionnaire** | **trait double dont le trait inférieur est pointillé** (3-3) |
| **exécution** | trait simple, **3,5 px**, toujours orange, jamais coloré par un type |

Raison du double trait plutôt qu'un trait plus épais : **l'épaisseur est déjà
prise** par l'exécution. Deux traits parallèles restent lisibles à 55 % là où une
différence d'épaisseur ne l'est plus, et ils disent visuellement « plusieurs ».

Raison du pointillé pour le dictionnaire : il faut un troisième état sur le même
axe, et **le pointillé est déjà, ailleurs, le signe de « en cours / incomplet »**
(le fil en cours de tirage). ⚠️ **Collision assumée** — mais le fil de tirage est
gris et suit le curseur ; il ne se confond pas avec un fil posé et coloré. **Si
Rodolf juge la collision trop risquée, la solution de repli est un troisième
trait plutôt qu'un pointillé** ; je ne l'ai pas prise parce que trois traits de
1,4 px font 6 px de large et se confondent avec un fil d'exécution.

### 5.4 Le nœud qui PRODUIT un tableau

**PROPOSÉ — forme n° 9 du § 7.** Un nœud de calcul dont le corps est une **liste
de rangées à charge variable** :

```
┌──────────────────────────────┐
│ ▼ Construire tableau      ?  │
├══════════════════════════════┤  filet pétrole
│ ▌ 1.0  0                     │   ← élément 0, prise + valeur
│ ▌ 1.0  1                     │
│ ▌ 1.0  2                     │
│        + ajouter un élément  │
│  3 éléments                  │   ← ligne d'état
│              tableau  [1.0] ▐│
└──────────────────────────────┘
```

- **chaque élément est une vraie prise** — il peut être branché individuellement.
  Même choix que le `ColorRamp` de `planche_01`, où chaque arrêt est une prise ;
- la rangée d'ajout et le `−` par ligne pilotent la longueur ;
- ⚠️ **repli obligatoire au-delà de 8 éléments** : les rangées 9 et suivantes sont
  remplacées par une **rangée `… et 492 autres`**, dépliable. `CATALOGUE_NOEUDS.md`
  § 5bis pose l'exigence (*« un tableau de 500 entrées détruit le canevas »*) ;
  **8 est PROPOSÉ**, pour que le nœud reste plus court que le `Principled` déplié.

**Le dictionnaire** a le même nœud avec **deux colonnes** : `clé` (champ de texte)
et `valeur` (prise + contrôle). **La clé n'a jamais de prise** — PROPOSÉ, raison :
une clé branchée rendrait la structure du dictionnaire imprévisible à
l'édition, donc non compilable pour un matériau.

### 5.5 Le nœud qui PARCOURT un tableau

**PROPOSÉ — forme n° 10.** C'est un nœud d'exécution, et **sa silhouette doit
être reconnaissable entre toutes**, parce qu'il est le seul à avoir **deux
sorties d'exécution de nature différente** :

```
┌────────────────────────────────────┐
│ ▶ ▼ Pour chaque                 ?  │  ← entrée d'exéc. sur l'en-tête
│      Boucle                        │
├════════════════════════════════════┤  filet ORANGE
│ ▌[1.0] éléments                    │
│                    corps de boucle ▶│  ← exéc., une fois PAR élément
│                         élément 1.0▐│  ← la valeur courante
│                          indice  12▐│
│                      terminé       ▶│  ← exéc., une seule fois, à la fin
│  0 / 3                             │  ← ligne d'état pendant l'exécution
└────────────────────────────────────┘
```

⚠️ **`corps de boucle` et `terminé` doivent se distinguer sans lire l'étiquette**
— c'est la faute la plus commune du débutant en blueprint. **PROPOSÉ : une ligne
vide de 8 px entre les deux**, plus le fait que `terminé` est la dernière rangée.
Ce n'est pas énorme ; **je n'ai pas mieux, et je préfère le dire.**
🔴 **NON TRANCHÉ** — si Rodolf veut un signal plus fort (une teinte de rangée, un
glyphe de boucle sur la prise), c'est son appel.

### 5.6 🔴 Ce qui se passe quand on branche un tableau sur une entrée scalaire

**La question la plus importante de cette section, et le corpus n'en dit rien.**
Trois politiques existent dans l'industrie, et elles ne se ressemblent pas :

| politique | qui la pratique | conséquence |
|---|---|---|
| **A — refus sec** | Unreal | la prise s'éteint pendant le tirage, le lien ne se fait pas |
| **B — adaptation implicite** | Houdini, Blender (geometry nodes) | le nœud s'exécute N fois, une par élément. Puissant, et **invisible** |
| **C — refus utile** | — | on refuse, **et on propose le nœud qui répare** |

**PROPOSÉ : C.** Voici ce que ça donne, précisément :

1. pendant le tirage, la prise scalaire s'affiche **incompatible** (éteinte, 30 %
   d'opacité, § 11.4) ;
2. **si l'utilisateur relâche quand même dessus**, on n'annule pas le geste : on
   ouvre la **recherche de nœud filtrée** (§ 12.1) sur les seuls nœuds qui vont
   de `[T]` vers `T` — `Élément à l'indice`, `Premier`, `Réduire`, `Pour chaque` ;
3. le nœud choisi **s'insère entre les deux** et les deux liens se font.

Raison du refus plutôt que de l'adaptation : **le graphe est partagé entre le
matériau et le blueprint** (`CATALOGUE_NOEUDS.md` § 7). L'adaptation implicite
est défendable dans un graphe qui s'évalue ; elle ne l'est pas dans un graphe
**qui se compile vers NkSL** — une boucle implicite de longueur inconnue à la
compilation n'a pas de traduction en shader. **Une même règle doit valoir dans les
deux mondes**, donc c'est la plus stricte qui gagne.

Raison du « refus **utile** » plutôt que du refus sec : le refus sec laisse
l'utilisateur devant un lien qui ne se fait pas, sans lui dire pourquoi ni quoi
faire. **Le moment où l'éditeur en sait le plus sur l'intention est exactement
celui où on relâche le bouton** — le gâcher est un luxe.

### 5.7 Les autres nœuds de tableau — inventaire

Nommés dans `CATALOGUE_NOEUDS.md` § 5bis, ils suivent tous des formes déjà
définies, aucun ne demande un dessin propre :

| nœud | forme | filet |
|---|---|---|
| `Longueur` | transformateur (§ 7.2) | pétrole |
| `Élément à l'indice` | transformateur | pétrole |
| `Ajouter` / `Retirer` | exécution (§ 7.7) | orange |
| `Filtrer` / `Trier` | transformateur à charge variable | pétrole |
| `Clés` / `Valeurs` (dictionnaire) | séparateur (§ 7.4) | pétrole |
| `Contient la clé` | transformateur → booléen | pétrole |

---

## 6. Textures et matériaux

### 6.1 Ce que la référence montre — **VU**, et c'est décisif

`images (4).jpg` est **la seule image du corpus qui montre le même graphe dans
deux états**, légendés `NODE PREVIEW ON` et `NODE PREVIEW OFF`. Relevé :

- **ON** : le nœud `OctSpecular1` porte, **entre l'en-tête et la première
  rangée**, une **vignette carrée** en pleine largeur moins une marge, montrant
  une sphère rendue **sur un damier** ;
- **OFF** : la vignette disparaît **entièrement**, les rangées remontent contre
  l'en-tête, le nœud **raccourcit d'autant**. Largeur, couleurs, ordre des
  rangées : identiques ;
- `images (3)` montre la même chose sur une **texture** : `ImageTexture` porte la
  vignette de l'image chargée, plus haute que large.

**Conclusions VU, sans interprétation** : l'aperçu est un **bloc de corps
escamotable en pleine largeur**, pas une décoration d'en-tête, pas une infobulle.
Et dans cette image, il se **bascule globalement** (tous les nœuds ensemble),
pas nœud par nœud.

⚠️ **Ce que la référence ne montre PAS** : ni la taille, ni le chemin, ni
l'espace colorimétrique n'apparaissent nulle part sur le nœud. Rodolf les demande
explicitement. **Tout le § 6.2 est donc PROPOSÉ.**

### 6.2 Le nœud de TEXTURE — **PROPOSÉ**

```
┌────────────────────────────────────────┐
│ ▼ Texture image                     ?  │
│    Texture                             │
├════════════════════════════════════════┤  filet PÉTROLE (jamais orange)
│  ┌──────────────────────────────────┐  │
│  │                                  │  │  ← aperçu, 96 px de haut,
│  │        (l'image, recadrée)       │  │    damier sous l'alpha
│  │                                  │  │
│  └──────────────────────────────────┘  │
│  murs/beton_albedo.png             ⋯   │  ← chemin, ÉLIDÉ PAR LA GAUCHE
│  2048 × 2048 · RVBA8 · sRVB            │  ← carte d'identité, une ligne
│ ▌ XY  Coordonnées        branchée      │
│ ▌ 1.0 Gamma               [ 2.200 ]    │
│                    Couleur   RVB ▐     │
│                    Alpha     1.0 ▐     │
└────────────────────────────────────────┘
```

| élément | spécification | raison |
|---|---|---|
| **aperçu** | 96 px de haut, largeur du corps − 16, **image recadrée au centre** (jamais déformée), **damier 8 px sous l'alpha** | VU pour le principe ; le damier parce que sans lui un PNG noir opaque et un PNG transparent sont le même rectangle noir |
| **chemin** | 10 px, `#8A8A8A`, **élidé par la GAUCHE** (`⋯beton_albedo.png`) | l'information utile d'un chemin est **à la fin**. Élider par la droite cache le nom du fichier — c'est l'erreur la plus courante |
| **carte d'identité** | une seule ligne, 9,5 px, `#6A6A6A` : `taille · format · espace colorimétrique` | **trois faits sur une ligne**, parce qu'ils se lisent ensemble ou pas du tout |
| **espace colorimétrique** | `sRVB` ou `linéaire`, **en toutes lettres** | ⚠️ c'est le seul champ qui **change silencieusement le rendu**. Une normal map en sRVB donne un éclairage faux sans aucune erreur. Il doit être lisible **sans clic** |
| **filet** | **pétrole, toujours** | § 6.4 |

⚠️ **PROPOSÉ : quand l'espace colorimétrique est incohérent avec l'usage** (une
texture sRVB branchée sur une entrée `Normale`), **la ligne d'identité passe en
`#E4443C` et le nœud prend l'état « avertissement » (§ 11.2)** — pas l'état
erreur : le graphe reste valide, il est seulement probablement faux. C'est le
seul cas du document où le dessin porte un jugement métier ; il le mérite,
parce que c'est le bug le plus cher et le plus silencieux d'un graphe de
matériau.

### 6.3 Le nœud de MATÉRIAU — **PROPOSÉ**

Même bloc d'aperçu, **mais une sphère rendue avec le matériau, sur damier** — VU
sur `images (4)`.

Différences avec la texture :

- **l'aperçu est un rendu, donc il a un coût.** PROPOSÉ : il se recalcule **à la
  fin d'un geste**, jamais pendant qu'on tire un curseur, et il affiche un
  **voile de 30 % + un point qui pulse au coin haut-droit** tant qu'il n'est pas
  à jour. Sans cet état, un aperçu périmé ment.
- **pas de chemin, pas de taille** : un matériau n'est pas un fichier au sens où
  la texture l'est. **PROPOSÉ : à la place, `4 textures · 12 nœuds`** — ce qu'il
  coûte, qui est l'information qu'on cherche vraiment sur un matériau.
- ⚠️ **la sortie `Surface` porte le type `shader`** (`SH`, terre cuite), **et
  rien d'autre ne s'y branche** — `CATALOGUE_NOEUDS.md` § 5quater.6 demandait
  « une forme de prise qui n'appartienne qu'à elle ». **PROPOSÉ : ce n'est PAS
  une forme propre, c'est la forme normale avec la couleur apparence et le
  glyphe `SH`.** Raison : une septième forme de prise pour un seul type est un
  vocabulaire cher pour un cas que le refus de branchement gère déjà (la prise
  s'éteint pendant le tirage, § 11.4). Si Rodolf veut la forme propre, c'est
  facile à ajouter — mais je ne la propose pas.

### 6.4 🔴 La règle absolue — **DÉCIDÉ**

**Un graphe de matériau n'a JAMAIS de fil d'exécution. Jamais.**

Conséquences pour le dessin, toutes obligatoires :

1. **tout nœud de matériau porte le filet pétrole** — un filet orange dans un
   graphe de matériau est un bug, pas un cas ;
2. **aucune prise à pointe** n'apparaît dans un éditeur de matériau ;
3. la bibliothèque de nœuds **ne propose pas** les familles à exécution quand le
   graphe est un matériau — elles ne sont pas grisées, **elles sont absentes** ;
4. l'entrée d'exécution étant sur l'en-tête, **l'en-tête d'un nœud de matériau
   n'a jamais de prise à gauche** : son bord gauche d'en-tête est net. C'est
   gratuit, et ça se voit à 25 % de zoom.

### 6.5 L'aperçu au dézoom — **PROPOSÉ**

Rodolf pose la question explicitement. Réponse par palier (§ 12.2) :

| palier | ce que devient l'aperçu | raison |
|---|---|---|
| **100 %** | vignette complète, 96 px | |
| **55 %** | **vignette conservée, réduite à 48 px**, tout le reste du corps s'efface | ⚠️ **c'est le seul élément du nœud qui GAGNE en importance relative quand on dézoome** : à 55 % on ne lit plus « Texture image », mais on reconnaît une image de brique en un coup d'œil. L'aperçu **est** l'étiquette, une fois le texte perdu |
| **25 %** | **la vignette devient la couleur moyenne de l'image**, en aplat sur tout le nœud | une image de 48 px réduite à 12 px est du bruit. Sa moyenne, non : une planche de bois reste brune, un ciel reste bleu, et **la lecture d'un graphe de matériau dézoômé se fait exactement là-dessus** |

C'est la seule exception à la règle « à 25 %, le nœud est un rectangle de la
couleur de sa catégorie » — et elle se justifie : pour une texture, **la couleur
moyenne est plus informative que la catégorie**, qu'on connaît déjà par la forme.

---

## 7. Toutes les formes de nœud

Les huit du catalogue (§ 5quater), plus les six que Rodolf demande. **La forme
suit la connectivité, jamais le nom** — c'est pour ça qu'il y en a quatorze et
pas cinquante-quatre.

| # | forme | côté gauche | côté droit | filet | exemple |
|---|---|---|---|---|---|
| 1 | **source** | **vide** | 1..n valeurs | pétrole | `RGB`, `Valeur`, `Coordonnées de texture` |
| 2 | **transformateur** | n valeurs | 1 valeur | pétrole | `Maths`, `Mélanger`, `Borner` |
| 3 | **puits** | n valeurs | **vide** | pétrole | `Sortie de matériau` |
| 4 | **séparateur** | 1..2 valeurs | n valeurs | pétrole | `Séparer XYZ`, `Texture image` |
| 5 | **charge variable** | n valeurs | 1 valeur | pétrole | `ColorRamp`, `Courbe` |
| 6 | **surface** | ~20 valeurs, en sections | 1 shader | pétrole | `Principled` |
| 7 | **exécution** | exéc. + valeurs | exéc. + valeurs | **orange** | `Si / Sinon`, `Écrire` |
| 8 | **événement** | **aucune entrée d'exéc.** | 1 exéc. + valeurs | **orange** | `Au démarrage`, `À chaque image` |
| 9 | **producteur de tableau** | n éléments | 1 tableau | pétrole | `Construire tableau` (§ 5.4) |
| 10 | **itérateur** | exéc. + tableau | **2 exéc.** + valeurs | **orange** | `Pour chaque` (§ 5.5) |
| 11 | **aperçu** | 1 valeur | 0..1 valeur | pétrole | `Texture image`, `Matériau` (§ 6) |
| 12 | **groupe** | ses entrées exposées | ses sorties exposées | selon son contenu | § 7.6 |
| 13 | **custom** | selon la déclaration | selon la déclaration | selon | § 8 |
| 14 | **inerte** | — | — | — | commentaire, cadre, relais (§ 9) |

### 7.1 La source — le côté gauche vide

**Ce qui la fait reconnaître de loin, c'est le vide à gauche**, pas son contenu.
⚠️ **PROPOSÉ : ne jamais dessiner de marge gauche « au cas où ».** Le corps
commence au ras du texte. Un nœud source doit avoir **un bord gauche parfaitement
net** — c'est un signal gratuit qu'on perdrait par simple symétrie de mise en page.

### 7.2 Le transformateur — la liste d'opération

**VU** (`174227` : `Multiply / Math` en en-tête + sous-titre). Deux emplacements
possibles pour l'opération :

- **dans le sous-titre de l'en-tête** — le titre devient `Multiplier`, le
  sous-titre `Maths` ;
- **en première rangée**, dans une liste déroulante pleine largeur.

**PROPOSÉ : le sous-titre**, et voici pourquoi c'est le meilleur des deux.
⚠️ `CATALOGUE_NOEUDS.md` § 5quater.2 note que **changer l'opération change le
nombre d'entrées** (une addition en prend 2, un `borner` en prend 3). Si
l'opération est en première rangée, **le nœud change de hauteur sous le curseur
au moment même où on clique** — la rangée qu'on visait s'est déplacée. Dans
l'en-tête, la liste ne bouge jamais.

### 7.3 Le puits — plus imposant, et unique

**PROPOSÉ** : `Sortie de matériau` prend l'en-tête **orange Rihen `#F79A28`**
(le seul de tout l'éditeur) et **un corps 20 % plus large** que les autres.
Raison : il est unique par graphe et il en est la fin ; le rendre trouvable au
premier coup d'œil dans `images (2)` — trente nœuds — vaut cette exception.

⚠️ **PROPOSÉ : un second nœud de sortie dans le même graphe est une ERREUR**
(§ 11.1), pas un avertissement. Deux fins possibles, c'est un graphe qui ne
compile pas.

### 7.4 Le séparateur — deux sorties qu'on ne doit pas confondre

`CATALOGUE_NOEUDS.md` § 5quater.4 pose le problème : `Texture image` sort
**couleur ET alpha**, et on branche l'alpha en croyant brancher la couleur.

**PROPOSÉ** : les deux sorties sont **de familles de couleur différentes** —
`Couleur` en apparence `#D9B6A3` (glyphe `RVB`), `Alpha` en nombre `#17B2EB`
(glyphe `1.0`). **Le problème se résout tout seul par le typage**, sans règle
spéciale : elles ne se ressemblent déjà pas. C'est le meilleur genre de solution
— celle qui n'ajoute rien.

### 7.5 Le nœud à charge variable — l'éditeur EST le nœud

`ColorRamp`, `Courbe`. **PROPOSÉ**, et le corpus n'en montre aucun.

- **la barre de dégradé occupe la largeur** du corps moins 16 px, hauteur 20 px ;
- **chaque arrêt est une prise** (déjà dans `planche_01`) : sa position **et** sa
  couleur sont pilotables ;
- **au dézoom 55 %, les poignées d'arrêt disparaissent, la barre reste** — elle
  devient un aperçu. Même logique que § 6.5 : **l'éditeur graphique dégrade en
  aperçu, il ne disparaît pas.** C'est ce qui distingue un nœud à charge variable
  d'un nœud ordinaire quand le texte est perdu ;
- **à 25 %, la barre devient la couleur moyenne du dégradé.**

### 7.6 Le nœud de GROUPE — ✅ **DÉCIDÉ le 22/08, et intégralement porté par le modèle**

> Rodolf, 22/08 : *« un groupe est un groupement de nœuds que l'utilisateur peut
> **empaqueter pour réutiliser à volonté comme des fonctions**. »*

⚠️ **Correction d'une erreur de ce document : le cadre et le groupe n'ont rien en
commun**, et les § 7.6 et 9.2 les traitaient en voisins. Ils ne le sont pas.

| | **cadre** (§ 9.2) | **groupe** (ici) |
|---|---|---|
| effet sur le graphe | **aucun** — il décore | **il abstrait** : N nœuds deviennent 1 |
| interface | pas d'entrées ni de sorties | **des entrées et des sorties** |
| réutilisation | un cadre entoure **un** endroit | **autant d'instances qu'on veut** |
| replié | il **cache** | il **EST un nœud** |
| identité | un titre | **un type de nœud à part entière** |

**Ce qui en découle, et qui ne relève pas du dessin :**

1. **définition + instances** — modifier la définition modifie toutes les
   instances. Il faut donc une commande **« rendre unique »** (dupliquer la
   définition pour une seule instance), sinon on n'a aucune sortie de secours ;
2. **les prises du groupe se voient DE L'INTÉRIEUR**, portées par deux nœuds
   ordinaires `Entrées du groupe` / `Sorties du groupe`. Un groupe est un graphe
   comme un autre — rien de nouveau à inventer ;
3. **la sérialisation écrit la définition une fois**, chaque instance ne portant
   qu'une **référence** ;
4. **un groupe est un type de nœud créé par l'utilisateur à l'exécution.**

#### ✅ Point 4 — MESURÉ, et la réponse est oui

C'était le point qui pouvait tout bloquer, et j'ai refusé de le supposer. Bancs
exécutés le 22/08 contre `NkNodeGraph` / `NkGraphDocument` (g++ 16.1, les 62
unités de NKCore/NKMemory/NKContainers liées ; **0 échec sur 24 vérifications**) :

| ce qui a été mesuré | résultat |
|---|---|
| type de **prise** enregistré après coup, sur un graphe déjà peuplé | ✅ identifiant neuf, prise portable, **distinct** des existants |
| conversion déclarée à l'exécution | ✅ prise en compte immédiatement |
| survie du type à l'aller-retour | ✅ **et il garde le MÊME identifiant** |
| type de **nœud** | ✅ `NkNode::type` est une chaîne libre : **il n'y a rien à enregistrer** — tout type de nœud est déjà « créé à l'exécution » |
| **le groupe lui-même** | ✅ **il existe déjà en entier** : `NK_NODE_INSTANCE`, `NK_NODE_GROUP_IN/OUT`, le champ `subgraph`, et `BuildPlan` |
| définition écrite une seule fois | ✅ mesuré : `graphe MonGroupe` **1 fois**, `sousgraphe` **2 fois** pour deux instances |
| plan aplati | ✅ instances et frontières **ont disparu** ; le corps du groupe apparaît **deux fois**, chemins `racine/Instance 1` et `racine/Instance 2` |
| définition modifiée **après** instanciation | ✅ `InterfaceMismatch`, **avec le détail** : `racine/Instance 1 : socket absent sur l'instance : V2` |
| groupe qui s'instancie lui-même | ✅ `RecursiveSubgraph` — refusé, pas explosé |
| instance nommant un groupe absent | ✅ `UnknownSubgraph` |

**Conclusion : aucun point d'architecture à remonter sur le groupe.** Les points
1, 2 et 3 ne sont pas à construire — **ils sont déjà là**, et le point 2 est
*exactement* le mécanisme que le modèle a choisi. Le travail restant est **de
dessin et d'édition**, pas de modèle.

⚠️ **Un défaut trouvé au passage, et il est réel** : sur `UnknownSubgraph`,
`NkEvalPlan::errorDetail` revient **vide**, alors que l'en-tête promet qu'il est
« rempli en cas d'échec ». `InterfaceMismatch` le remplit bien. L'utilisateur
apprend donc qu'un groupe est introuvable **sans apprendre lequel** — dans un
document qui peut en compter des dizaines, c'est le cas où le détail sert le
plus. À corriger côté modèle.

#### ✅ Comment l'interface se forme — **DÉCIDÉ le 22/08, et elle est CALCULÉE**

> Rodolf : *« lorsqu'on regroupe ces nœuds en groupe, le groupe résultat a des
> entrées et des sorties qui peuvent directement se voir dans le groupe, et le
> groupe devient donc un nœud à lui seul. »*

**L'utilisateur ne déclare rien.** Il sélectionne des nœuds, il demande le
regroupement, et **ce sont les fils qui traversent la frontière de la sélection
qui deviennent les prises** :

| fil | ce qu'il devient |
|---|---|
| **dehors → dedans** | une **entrée** du groupe |
| **dedans → dehors** | une **sortie** du groupe |
| **dedans → dedans** | rien — il reste interne, entièrement |
| **dehors → dehors** | rien — il ne concerne pas le groupe |

⚠️ **Appliquée naïvement, cette règle est fausse dans quatre cas**, et chacun
produit un groupe qui *paraît* correct :

| # | le piège | la règle qui le corrige |
|---|---|---|
| 1 | une **même prise source extérieure** alimente cinq nœuds internes | ❌ cinq entrées identiques. ✅ **On déduplique par la SOURCE** : une prise de sortie extérieure donnée ne crée **qu'une** entrée, et les cinq fils internes s'y rebranchent |
| 2 | une **sortie interne** alimente trois nœuds extérieurs | ❌ trois sorties. ✅ Symétrique : **on déduplique par la prise interne**, une sortie, trois fils au-dehors |
| 3 | l'**ordre** des prises | ❌ grouper deux fois la même sélection donnerait deux nœuds de formes différentes. ✅ **L'ordre est écrit, pas émergent** : tri par **Y croissant du nœud interne**, puis X, puis **index de prise**, puis **identifiant du nœud**. ⚠️ **QUATRE clés — ce document en annonçait trois, et c'était faux** : deux nœuds peuvent partager une position au pixel près, et `(y, x, index)` serait alors à égalité. L'implantation a corrigé |
| 4 | les **noms** | ❌ deux prises internes nommées `Couleur` donnent deux prises publiques homonymes. ✅ Le nom public est celui de la prise interne ; **en cas d'homonymie, on suffixe par le libellé du nœud** (`Couleur (Mélange)`), et si ça collisionne encore, par un numéro. La désambiguïsation suit l'ordre du point 3, donc **elle est stable** |

⚠️ **Un cas de REFUS, et il faut le nommer plutôt que grouper de travers** : si
**deux fils d'exécution distincts** entrent dans la sélection en **deux points
internes différents**, le groupe aurait besoin de deux entrées d'exécution — or
une entrée d'exécution est **unique par construction** (§ 3.4, arité inversée).
**La sélection n'est pas groupable.** L'éditeur doit le dire ainsi : *« ces nœuds
ne peuvent pas être groupés : deux fils d'exécution y entrent en deux endroits
(`Si` et `Boucle`). Groupez-les avec leur point d'entrée commun. »* — **avec les
deux noms**, sinon l'utilisateur cherche.

#### ✅ Le critère d'acceptation — **nécessaire, mais PAS suffisant**

> **Grouper puis dégrouper doit rendre le graphe identique** — après
> sérialisation, aux identifiants près.

⚠️ **Ce critère seul ne prouve pas la déduplication, et il faut le dire.** Un
groupe qui créerait **cinq entrées identiques** pour une constante partagée par
cinq nœuds les redistribuerait correctement au dégroupement : **les liens se
recollent en repassant par le milieu**, et le graphe revient identique. L'erreur
survit à l'aller-retour parce qu'elle est **symétrique**.

✅ **Il faut donc regarder l'INTERFACE elle-même**, et pas seulement le graphe
qui en ressort.

#### ✅ IMPLÉMENTÉ — `NkGraphGroup.h/.inl`, et le vérificateur est à 106/106

⚠️ **Ce paragraphe décrit du code qui EXISTE**, livré par l'agent du graphe de
matériaux le 22/08 (13 mutations sur 13 détectées). Il n'est pas à réécrire, et
la spécification n'a pas à en proposer une seconde version : **elle doit dire la
même chose que lui.** `NkGrouper` / `NkDegrouper`, avec `NkGroupError` qui rend
une **raison**, comme `NkLinkError` et `NkPlanError`.

**Les cinq contrôles réellement exécutés :**

| contrôle | ce qu'il attrape |
|---|---|
| `regroupement/aller-retour-identique` | la perte de contenu — par une **forme canonique** qui range les nœuds par leur **contenu**, jamais par leur rang, et décrit les liens par les **descripteurs** de leurs extrémités, jamais par des numéros |
| `regroupement/interface-deduite-et-dedupliquee` | **le complément indispensable** : une prise source qui traverse deux fois doit donner **une** entrée, pas deux. Une implantation qui compterait les **liens** au lieu des **prises sources** tombe ici, et **nulle part ailleurs** |
| `regroupement/ordre-des-prises-deterministe` | le piquet 3 — le même graphe construit par **deux histoires d'édition différentes** doit donner la même interface |
| `regroupement/refus-nommes` | qu'un refus **dise** lequel |
| `regroupement/conversions-traversent-la-frontière` | voir l'avertissement ci-dessous |

📌 **Deux finesses de l'implantation qui méritent d'être connues ici**, parce
qu'elles disent pourquoi un contrôle honnête est plus dur à écrire qu'il n'y
paraît :

1. **la comparaison n'est PAS textuelle**, alors que c'était la lettre du critère.
   Après un aller-retour les nœuds sont **recréés**, donc renumérotés **et
   réordonnés** ; renuméroter le texte par ordre d'apparition comparerait des
   nœuds qui n'ont rien à voir. Le contrôle rendrait « différent » sur un
   aller-retour parfait — puis serait relâché jusqu'à ne plus rien dire ;
2. **sa limite est écrite dans le code plutôt que tue** : si deux nœuds ont le
   même descripteur, la forme canonique ne peut plus les distinguer. Le contrôle
   le **détecte et l'annonce** (`ambigu`) au lieu de rendre un vert trompeur.
   ✅ **Un cas qui ne peut pas discriminer doit le dire, pas se taire.**

#### ⚠️ Le piège que cette spécification n'avait PAS vu : **chaque graphe a son propre registre de types**

Trouvé par l'implantation, et il aurait tout cassé en silence :

> Un sous-graphe créé vide **refuserait à l'intérieur** un lien `réel → couleur`
> que le parent acceptait — et **le regroupement perdrait un fil sans que rien ne
> le dise**, puisque `Connect` rend une erreur que personne ne lit.

Le registre **et** les conversions dirigées sont donc **recopiés du parent vers
l'enfant** avant tout le reste, et les types sont traduits **par leur nom**, les
identifiants n'étant pas garantis égaux d'un registre à l'autre.

📌 **C'est la même racine que le « registre plat » du § 5** : un `NkTypeId` est un
entier **local à un graphe**, pas une identité globale. On ne le voit pas tant
qu'il n'y a qu'un graphe.

#### 🔴 Ce qui reste à couvrir côté EXÉCUTION

`NkGroupError` couvre sélection vide, nœud inconnu, graphe inconnu, nom déjà
pris, « pas une instance », sous-graphe inconnu. ⚠️ **Il ne couvre pas le refus
décrit plus haut** — deux fils d'exécution entrant dans la sélection en **deux
points internes différents**. C'est normal : un graphe de matériaux n'a **aucun**
fil d'exécution (§ 6.4), donc l'implantation actuelle ne pouvait pas le
rencontrer. **À ajouter quand le blueprint arrivera, pas avant.**

⚠️ **Divergence mineure, signalée sans être imposée** : ce document proposait de
désambiguïser deux prises homonymes **par le libellé du nœud** (`Couleur
(Mélange)`) ; l'implantation suffixe **par un numéro** (`couleur_2`). Le numéro
est stable et suffit au modèle — **le nom lisible relève de l'affichage, pas de
la clé**, et les deux peuvent coexister : clé numérotée, étiquette parlante.

#### Effets de bord sur le dessin

- ✅ **`E6 · fil d'Ariane` cesse d'être optionnel.** Entrer dans un groupe pour
  l'éditer est le **seul** chemin ; sans fil d'Ariane on ne sait plus où l'on est
  ni comment sortir. Il passe de ⚠️ à **obligatoire**.
- ✅ La question « où aboutissent les fils entrants quand c'est replié ? » (§ 9.2)
  **ne concerne que le cadre**. Pour un groupe, les fils aboutissent à **ses
  prises**, qui existent et sont nommées.
- ⚠️ Le repli d'un groupe **n'est pas** le repli d'un nœud (§ 2.4) : replier un
  nœud cache ses rangées ; un groupe replié **ne cache rien** — c'est son état
  normal.
- ⚠️ **Les deux nœuds d'interface se dessinent comme des nœuds ordinaires**, et
  c'est voulu : ils **sont** des nœuds ordinaires. Un seul écart — ils n'ont
  qu'un côté (le `Entrées` n'a aucune entrée, le `Sorties` aucune sortie), donc
  ils tombent exactement dans les formes déjà spécifiées **source** (§ 7.1) et
  **puits** (§ 7.3). Rien de neuf à dessiner.

**PROPOSÉ pour le dessin :**

- l'en-tête porte **un pictogramme de pile** (deux rectangles décalés) à gauche
  du titre — c'est le seul élément qui dit « il y a quelque chose dedans » ;
- **double-clic pour entrer**, et le fil d'Ariane apparaît en haut du canevas
  (`Matériau mur ▸ Bruit de surface`) — VU comme mécanisme dans `images (5)` ;
- **les entrées et sorties du groupe sont celles qu'on a exposées**, et elles
  portent leur **nom public** ;
- le filet est **orange si le groupe contient au moins un nœud qui exécute**,
  pétrole sinon. Raison : sinon le repli **cache la ligne de vie**, ce qui est
  exactement ce qu'un repli ne doit jamais faire.

✅ **DÉCIDÉ le 23/08 — SUR PLACE, jamais en onglet.** *Deux mécanismes de
navigation pour une seule notion, c'est un de trop.* Le fil d'Ariane étant devenu
obligatoire (E6), l'onglet ferait double emploi avec lui — et deux chemins pour
entrer dans un groupe, c'est deux chemins pour en sortir, donc deux façons de se
perdre.

🔴 **NON TRANCHÉ :**
- **le groupe montre-t-il un aperçu de son contenu** (une miniature du
  sous-graphe) ? Séduisant, et je n'ai **aucune référence** ni aucune mesure pour
  dire si c'est lisible à 200 × 120 px.

### 7.7 Le nœud d'exécution et l'événement

**VU** (`174227`, `174000`, `174057`) et **DÉCIDÉ** pour les positions (§ 3.4).

Ce qui distingue l'**événement** du nœud d'exécution ordinaire, et qui doit se
voir sans lire : **il n'a aucune entrée d'exécution**, donc **le bord gauche de
son en-tête est net**. C'est le symétrique exact du nœud source (§ 7.1), sur
l'autre rangée. **Un événement, c'est un nœud source de temps.**

**PROPOSÉ** : et parce que c'est *un début*, l'événement porte en plus **le coin
haut-gauche de son en-tête arrondi à 12 px au lieu de 5** — une seule courbe, qui
dit « ça commence ici ». C'est le seul écart de géométrie que je propose dans tout
le document, et il ne coûte rien.

⚠️ `Si / Sinon` a **deux sorties d'exécution**. `CATALOGUE_NOEUDS.md` § 5quater.7
exige qu'elles se distinguent **par l'étiquette ET la position, jamais par la
seule couleur** — elles sont toutes les deux orange. **Elles descendent donc dans
le corps** (§ 3.4), `Vrai` puis `Faux`, dans cet ordre, toujours.

---

## 8. Les nœuds CUSTOM — et surtout : quand l'éditeur ne sait rien

C'est la demande de Rodolf la plus tournée vers l'avenir, et le corpus la couvre
mieux qu'on ne pourrait l'espérer : **`node-based-…webp` contient un `Example
node`** — trois entrées `input1..3`, deux sorties `output1..2`, aucun contenu,
aucune valeur, aucun type. **C'est exactement le squelette nu d'un nœud dont
l'éditeur ne sait rien.**

### 8.1 Les trois niveaux de connaissance — **PROPOSÉ**

L'éditeur ne connaît pas « les nœuds custom » en bloc. Il en connaît **trois
degrés**, et le dessin doit les distinguer, parce qu'ils n'engagent pas la même
confiance.

| niveau | ce que l'éditeur sait | ce qu'il dessine |
|---|---|---|
| **A — déclaré** | nom, catégorie, prises typées, valeurs par défaut, aide | **un nœud normal.** Rien ne le distingue d'un nœud livré, et c'est le but |
| **B — déclaré partiellement** | les prises et leurs noms, **pas les types** | prises en **`quelconque` gris `#9AA3AD`**, glyphe `?`, aucun champ de saisie |
| **C — inconnu** | rien qu'un nom de type lu dans le fichier | § 8.3 |

**Raison de séparer B et C** : B est un nœud qui **marchera** — il lui manque du
confort. C est un nœud **qui ne peut pas s'exécuter**. Les confondre, c'est soit
alarmer sur B, soit rassurer sur C.

### 8.2 Ce qu'une déclaration doit fournir — **PROPOSÉ**

Pour atteindre le niveau A, l'auteur d'un nœud déclare, dans cet ordre
d'importance :

1. **le nom affiché et la catégorie** → titre, sous-titre, couleur d'en-tête ;
2. **s'il exécute** → couleur du filet. ⚠️ **Non déductible** : un nœud sans
   prise d'exécution *déclarée* pourrait quand même avoir des effets de bord.
   **C'est une déclaration, pas une inférence** ;
3. **ses prises** : nom, type, sens, **et pour une entrée, sa valeur par défaut**
   — sans elle, la rangée non branchée n'a rien à afficher ;
4. **son texte d'aide** → le `?` de l'en-tête. Sans lui, **PROPOSÉ : le `?` n'est
   pas dessiné du tout**, plutôt qu'un `?` qui ouvre le vide ;
5. **facultatif** : pictogramme, largeur préférée, sections de repli.

⚠️ **Ce que l'auteur ne choisit JAMAIS** : la couleur des prises (elle vient du
type), la géométrie, les rayons, l'aspect des états. **Un nœud custom qui pourrait
choisir ses couleurs détruirait le code couleur pour tout le monde** — un seul
auteur suffit. C'est le point à écrire dans l'API avant qu'elle n'existe, pas
après.

### 8.3 🔴 Le nœud INCONNU — ce que l'éditeur affiche quand il ne sait rien

Le cas arrive **tout le temps** : on ouvre un `.nkgraph` produit avec un module
absent, une version antérieure, un greffon désinstallé.

**PROPOSÉ** :

```
┌────────────────────────────────────┐
│ ▼ studio.fx.Tourbillon             │  ← en-tête GRIS RAYÉ à 45°
├════════════════════════════════════┤  ← filet gris, ni orange ni pétrole
│ ▌ ?  entree_0        (contour tireté)│
│ ▌ ?  entree_1                       │
│                       sortie_0  ? ▐ │
│  type inconnu — les liens sont      │
│  conservés                          │
└────────────────────────────────────┘
```

| choix | raison |
|---|---|
| **en-tête gris rayé à 45°** | il ne peut pas emprunter la couleur d'une catégorie qu'on ne connaît pas, et un gris uni le ferait passer pour un nœud de calcul |
| **filet gris** — ni orange ni pétrole | ⚠️ **on ne sait pas s'il exécute.** Lui donner du pétrole serait une affirmation fausse. **Le filet doit avoir un troisième état « je ne sais pas »** — c'est une conséquence de ce nœud, et personne ne l'avait vue |
| **prises en contour tireté** | ni creux (« non branché ») ni plein (« branché ») : **elles peuvent être branchées et on ne sait pas si le type est bon** |
| **les noms de prise viennent du FICHIER** | c'est tout ce qu'on a, et c'est déjà beaucoup |
| **les liens sont conservés et redessinés** | 🔴 **la règle la plus importante de ce paragraphe.** Un éditeur qui laisse tomber les liens d'un nœud inconnu **détruit le travail à la simple ouverture du fichier**, et le désastre est silencieux : on sauvegarde, et c'est perdu. Le dessin doit rendre évident que **rien n'a été jeté** |
| **ligne d'état explicite** | l'utilisateur doit savoir que c'est récupérable, pas cassé |

⚠️ **Un nœud inconnu n'est PAS un nœud en erreur.** Le rouge dit « ce graphe est
faux » ; ici le graphe est probablement juste, c'est **l'éditeur** qui est
incomplet. Utiliser le rouge d'erreur pousserait l'utilisateur à supprimer le
nœud — exactement le geste qui perd le travail.

### 8.4 Le nœud INDISPONIBLE — le rang interdit — ✅ **DÉCIDÉ le 23/08**

`CATALOGUE_NOEUDS.md` § 3.3 : `Light Path`, `Raycast`, `Ambient Occlusion`,
`Curves Info`, `Particle Info` supposent un tracé de rayons ; notre moteur
rastérise. `ELEMENTS_A_DESSINER.md` D6 le laisse ⚠️.

**PROPOSÉ — et il faut le distinguer de tout le reste** :

- **dans la bibliothèque** : présent, en **50 % d'opacité**, avec sa raison en
  seconde ligne (`nécessite un tracé de rayons`). **Présent, parce qu'un nœud
  absent se cherche indéfiniment ; un nœud refusé se comprend une fois.**
- **posé sur le canevas** (cas d'un fichier importé) : **corps hachuré à 45° en
  `#3A3A44`**, en-tête à 50 %, **prises intactes et fils conservés**.
- **la raison est écrite dans le nœud**, pas dans une infobulle : `ce moteur
  rastérise`.

⚠️ **Trois états gris à ne jamais confondre**, et c'est le piège de cette
section :

| état | ce qu'il dit | signal |
|---|---|---|
| **désactivé** (§ 11.3) | *l'utilisateur* l'a éteint | corps à 45 %, **fil traversant en pointillé** |
| **inconnu** (§ 8.3) | *l'éditeur* ne le connaît pas | **rayures d'en-tête** + filet gris |
| **indisponible** (§ 8.4) | *le moteur* ne peut pas | **hachures de corps** + raison écrite |

Trois causes, trois responsables, trois remèdes. **Un seul gris pour les trois
rendrait l'éditeur inutilisable exactement au moment où l'utilisateur a besoin
d'aide.**

---

## 9. Commentaires et blocs

### 9.1 Le commentaire libre — 🔴 rien dans le corpus

⚠️ **À dire franchement : aucune des douze références ne montre un commentaire
libre posé sur le canevas.** `node-based-…webp` montre des paragraphes de texte,
mais **logés dans le corps d'un nœud**, pas flottants.

✅ **DÉCIDÉ le 23/08** — en assumant que c'est une invention, faute de corpus :

- **du texte posé sur le fond, sans corps, sans filet, sans fond** — le
  commentaire n'est pas un nœud et ne doit pas y ressembler ;
- 13 px, `#8A8A8A`, **aligné à gauche**, retour à la ligne à une largeur qu'on
  redimensionne par une poignée (§ 2.3) ;
- **au survol seulement**, un rectangle `#1A1A1A` à 60 % apparaît derrière pour
  qu'on sache où cliquer ;
- **il ne se branche à rien**, ne se replie pas, **et il passe DEVANT les fils,
  DERRIÈRE les nœuds** ;
- au dézoom **25 %, il disparaît entièrement** — c'est le seul objet qu'on
  efface complètement. Raison : illisible, il ne serait plus qu'une tache grise
  qui brouille la lecture de structure, et **c'est justement la lecture qu'on
  fait à 25 %**.

📌 **Pourquoi cet ordre d'empilement précis — devant les fils, derrière les
nœuds — et pas l'inverse :**

| | ce qui se passerait |
|---|---|
| **derrière les fils** | un fil qui passe barre le texte en diagonale : **le commentaire devient illisible** sans que rien ne soit déplaçable |
| **devant les nœuds** | il masquerait des **valeurs éditables** — du texte fixe cacherait de l'information vivante |

✅ **Il passe donc devant ce qui est décoratif pour lui, derrière ce qui est
éditable.** C'est la même règle que partout ailleurs ici : **ce qui informe
l'emporte sur ce qui commente.**

⚠️ **Et le rectangle de survol reste un signal FAIBLE volontairement** (`#1A1A1A`
à 60 %, au survol seulement) : plus marqué, il donnerait au commentaire l'allure
d'un nœud sans en-tête — **exactement ce que la première ligne de ce paragraphe
interdit.**

### 9.1bis La source et le puits — ✅ **DÉCIDÉ le 23/08 : ne RIEN ajouter**

`ELEMENTS_A_DESSINER.md` B6 et B7. **La tentation était de leur donner une marque
distinctive. Refusée, et pour la meilleure des raisons : ils en ont déjà une.**

| | ce qui les distingue | faut-il autre chose ? |
|---|---|---|
| **source** (`RGB`, `Value`) | **le côté gauche est vide** | ❌ non |
| **puits** (`Material Output`) | **le côté droit est vide** | ❌ non |

> ✅ **DÉCIDÉ : aucune marque supplémentaire.** L'absence de prises d'un côté est
> déjà visible **à 25 %**, où il ne reste pourtant qu'un rectangle coloré — c'est
> le seul trait de forme qui survit au dézoom maximal.

⚠️ **Ce que le catalogue autorisait et que je ne prends pas** : *« il est unique :
le dessin peut se permettre de le rendre plus imposant. »* **Un puits plus grand
coûterait une exception à la grille de largeurs pour une information que le côté
droit vide donne déjà.** On ne dépense pas une exception pour un doublon.

#### 🟡 La seule question qui reste ouverte, et elle n'est pas visuelle

Le catalogue dit le puits **unique**. Un fichier importé peut en apporter
**plusieurs, dont un seul actif** — c'est le cas chez Blender. **Je ne tranche pas
si le modèle l'autorise : c'est une question de modèle, pas de dessin.**

✅ **Mais si le cas existe, le dessin est déjà contraint, et il faut l'écrire
maintenant : c'est le puits ACTIF qui porte un signal POSITIF** — jamais les
autres un signal d'effacement.

> ⚠️ **Raison, et elle est chère payée** : marquer les puits inactifs en gris
> créerait **un QUATRIÈME gris** — après *inconnu*, *indisponible* et *désactivé*,
> qu'on vient tout juste de séparer par l'endroit (§ 11.5). Le quatrième n'aurait
> plus d'endroit libre. **On marque l'exception, jamais la règle.**

### 9.2 Le cadre — **VU**, et richement

`node-based-…webp` est la seule référence à en montrer un, et elle en montre
plus que prévu :

| micro-élément | ce qui est VU |
|---|---|
| **remplissage** | vert semi-transparent, ~8 % |
| **filet extérieur** | plein, 1,5 px, de la teinte du cadre |
| **second filet intérieur** | **pointillé**, à ~8 px du bord |
| **bandeau de titre** | plein, en haut, texte sombre sur la teinte |
| **compteur** | **`7 nodes`, aligné à droite du bandeau** |
| **teinte des nœuds contenus** | ⚠️ **les nœuds à l'intérieur prennent la teinte du cadre** (filet et en-tête verdis) alors que ceux du dehors restent bleus — ❌ **VU, et REJETÉ** : voir la décision ci-dessous |

### ✅ Les valeurs du cadre — **VALIDÉES par Rodolf le 22/08**

> *« dans la planche 05 tu as défini le bon cadre comme je veux. »*

Le cadre du panneau 9 de `planche_05_matieres.svg` **est la référence**. Ces
valeurs ne sont plus des propositions :

| élément | valeur validée |
|---|---|
| remplissage | teinte à **8 %** (`opacity="0.08"`) |
| filet extérieur | teinte pleine, **1,5 px**, `rx="6"` |
| **second filet intérieur** | **pointillé `4 4`**, 1 px, opacité 0,7, **8 px de retrait**, `rx="4"` |
| bandeau de titre | teinte **pleine**, hauteur **20 px**, coins hauts arrondis `r=6` |
| texte du titre | **sombre sur la teinte** (`#10240F` sur teinte verte), 12,5 px, graisse 600 |
| compteur de nœuds | **aligné à droite** du bandeau, 11 px, même teinte sombre |
| état replié | **le bandeau seul subsiste**, préfixé `▸`, titre et compteur sur une ligne |

⚠️ **Le double filet — plein dehors, pointillé dedans — est ce qui distingue un
cadre d'un simple rectangle de fond. Ne pas le simplifier.**

### ✅ DÉCIDÉ le 22/08 par Rodolf — **option A : la teinte n'est JAMAIS répliquée**

> *« pour mon cas je préfère A sachant que la couleur du cadre ne sera jamais
> répliquée et qu'il y ait un texte pour commentaire sur le cadre au début. »*

**Règle absolue : la teinte d'appartenance ne touche jamais le nœud** — ni son
corps, ni son en-tête, ni son filet, ni une pastille. **Le cadre entoure, et son
bandeau porte le sens.**

**La raison, en une phrase : l'en-tête porte la catégorie, et la catégorie est la
seule information qui survit au dézoom.** Au palier 25 % (§ 12.2), il ne reste du
nœud qu'un rectangle de couleur — cette couleur. La teindre en vert pour dire
« ce nœud appartient à ce cadre » **détruirait l'information la plus robuste de
l'éditeur** pour en afficher une **que le cadre dit déjà en entourant les
nœuds**. On échangerait un signal qui résiste à tout contre un signal redondant à
100 % et absent à 25 %.

✅ **Ce que la décision offre en prime : le conflit à trois disparaît.** La bordure
du nœud n'avait pas assez de place pour trois prétendants — sélection, erreur,
appartenance. Il n'en reste que deux. **C'est un cas dur de moins à spécifier, et
un de moins à coder.**

⚠️ **Contrepartie, et elle est réelle : le titre devient obligatoire.** Puisque la
couleur ne dit plus rien sur les nœuds, **le bandeau est le seul porteur du sens
du cadre**. Un cadre sans titre ne veut rien dire. Deux conséquences à trancher :

| | question ouverte |
|---|---|
| **titre vide** | 🔴 **PROPOSÉ** : à la création, le cadre naît avec le titre `Cadre` déjà sélectionné pour saisie — on ne peut pas produire un cadre anonyme sans le vouloir. Un titre effacé affiche `Sans titre` en italique atténué, **jamais rien** |
| **titre à 25 %** | 🔴 **PROPOSÉ** : sous 40 %, le texte devient illisible et le bandeau **reste** — il garde sa teinte pleine et sa hauteur. À 25 %, le cadre est donc un rectangle teinté surmonté d'une barre pleine : c'est la **silhouette** qui identifie, pas le mot. La teinte redevient utile là où le texte ne l'est plus |

### ✅ DÉCIDÉ — l'appartenance est un **lien explicite sérialisé**, pas une géométrie

C'est l'inverse de ce que ce document proposait, et **c'est le dessin lui-même qui
l'a tranché** : le cadre se replie sur son bandeau, et **un cadre replié n'a plus
de géométrie à interroger**. Le compteur `12 nœuds` du bandeau replié compte une
**liste**, pas une intersection. On ne replie pas ce qu'on ne sait pas énumérer.

**La règle :**

- **au dépôt seulement**, la géométrie décide : on lâche un nœud, on cherche le
  cadre **le plus intérieur** qui contient son **origine** — le coin haut-gauche
  de l'en-tête, **ni son centre, ni son recouvrement** — et on **écrit le lien** ;
- **ensuite le lien ne bouge plus** tant qu'on ne redépose pas le nœud ;
- **déplacer un cadre emporte ses membres**, ce qui n'est possible qu'avec un lien.

**Ce que ça règle, et qui n'a donc plus besoin de règle de rendu :**

| ancien cas dur | ce qu'il devient |
|---|---|
| `DUR_H1` — un nœud dans l'**intersection** de deux cadres | ✅ **résolu** : il appartient à celui que le dépôt a écrit. Le modèle sait, il ne devine pas |
| `DUR_H2` — un nœud **à cheval** sur un bord | ✅ **résolu** : dedans ou dehors est une donnée, pas une mesure |
| `DUR_H3` — le conflit à trois sur la bordure | ✅ **disparu** avec l'option A |

⚠️ **Conséquence pour la sérialisation : `cadre` n'est pas seulement un rectangle
à dessiner, c'est une RELATION à écrire** (§ 16). Une appartenance recalculée à
chaque chargement changerait si un cadre a bougé d'un pixel ; un lien explicite
est stable.

**PROPOSÉ pour le reste :**

- **le cadre passe DERRIÈRE tout** — nœuds et fils ;
- **il se déplace avec son contenu** : tirer le bandeau déplace le cadre **et**
  ses membres ; tirer le corps du cadre **ne déplace que le cadre** (donc on peut
  cadrer autre chose). ⚠️ **Ces deux gestes doivent avoir deux curseurs
  différents** ;
- **le compteur `12 nœuds` est repris** : gratuit, et il dit tout de suite si un
  nœud est tombé du cadre sans qu'on le voie. Il compte désormais **la liste des
  membres**, ce qui le rend juste même quand le cadre est replié.

#### 🔴 NON TRANCHÉ — où aboutissent les fils qui entraient dans un cadre replié ?

Deux candidats, et je donne ce que chacun coûte **quand douze fils entrent** :

| | ce qu'on voit | ce que ça coûte à 12 fils |
|---|---|---|
| **A · le fil aboutit au bandeau** | chaque fil se termine sur le bord gauche du bandeau, à la hauteur où était son nœud | douze fils convergent sur **20 px de hauteur** : ils se superposent, on ne distingue plus lequel va où, et la couleur de type devient un empilement. Le lien reste visible, **mais illisible** |
| **B · le fil disparaît, le bandeau compte** | le bandeau affiche `▸ Éclairage · 12 nœuds · ⇥ 12` | rien ne se superpose et le repli tient sa promesse — mais **un fil qui disparaît ment sur la connectivité** : on croit le graphe coupé là où il ne l'est pas |

**Mon avis, et il est faible** : B pour le rendu, **à condition** que le compteur
d'entrées soit cliquable et rouvre le cadre. Un fil illisible (A) coûte plus cher
qu'un fil absent mais annoncé (B). ⚠️ **Mais je n'ai aucune référence pour ça dans
le corpus, et c'est un choix d'usage, pas de dessin — à Rodolf de trancher.**

⚠️ **Cette question ne concerne QUE le cadre.** Pour un **groupe** (§ 7.6) elle ne
se pose pas : un groupe replié **est un nœud**, et les fils aboutissent à ses
prises, qui existent.

### 9.3 Le relais (reroute)

**VU** — la puce `body.tests` de la principale, avec une découverte : elle porte
**une entrée à gauche ET une sortie à droite**, plus un **bloc-icône plein
`#2E2770` sur toute la hauteur à gauche**. Ce n'est donc pas un simple point sur
un fil : **c'est une valeur nommée qui passe.**

**PROPOSÉ — deux objets, pas un**, parce que le corpus en montre deux et qu'ils
ne servent pas à la même chose :

| | dessin | rôle |
|---|---|---|
| **relais nu** | **un rectangle 17 × 17** de la couleur du type, rayon 2, **sans corps, sans texte** | ranger un fil. Il ne dit rien qu'on ne sache déjà |
| **relais nommé** (la puce) | pilule à coins 13 px, bloc-icône coloré à gauche, libellé, prise d'entrée et prise de sortie | poser une source près de son consommateur, à l'autre bout du graphe |

⚠️ **Le relais nu est un carré, pas la prise 17 × 63.** Raison : il n'est à
cheval sur aucun bord — il **est** le point. Lui donner la silhouette d'une prise
ferait chercher le nœud auquel il appartient.

---

## 10. Les fils — **VU**, **PROPOSÉ**, et ✅ **DÉCIDÉ le 23/08** pour C5 et C6

| famille | trait | source |
|---|---|---|
| **valeur** | courbe de Bézier, **2 px**, couleur du type | VU (principale) |
| **exécution** | même courbe, **3,5 px**, **toujours orange**, jamais colorée par un type | DÉCIDÉ |
| **tableau** | **double**, 2 × 1,4 px, écart 2,5 px | PROPOSÉ (§ 5.3) |
| **dictionnaire** | double, trait inférieur **pointillé** | PROPOSÉ (§ 5.3) |
| **en cours de tirage** | **pointillé gris**, suit le curseur | VU comme convention, pas dans le corpus |

**VU sur la principale, et à reprendre tel quel :**

- **la couleur du fil est celle de la prise dont il part** — vérifié sur trois
  paires (violet → violet, ambre → ambre, blanc → blanc). **Le fil n'a pas de
  couleur propre.** ⚠️ Corollaire : quand les deux bouts sont de familles
  différentes (une conversion déclarée, § 11.5), **PROPOSÉ : dégradé de la
  couleur de départ vers celle d'arrivée** — c'est ce que semble faire
  `174000` (un fil vire de l'orange au vert entre deux prises de couleurs
  différentes), sans que je puisse l'affirmer d'une seule image ;
- **le fil sort horizontalement du bord** avant de s'infléchir. Non négociable :
  c'est ce qui rend la prise de départ identifiable quand dix fils partent du
  même nœud ;
- **les fils passent DERRIÈRE les nœuds.**

**PROPOSÉ — la cohabitation des deux familles**, qui est la question ouverte n° 2
de `CATALOGUE_NOEUDS.md` § 5quinquies. Elles se distinguent par **trois signaux
simultanés** :

1. **la couleur** — l'orange d'exécution n'est jamais une couleur de donnée ;
2. **l'épaisseur** — 3,5 contre 2 ;
3. **le point d'attache** — l'exécution part de l'**en-tête**, la donnée du
   **corps**. ⚠️ **C'est le plus fort des trois** : les fils d'exécution vivent
   sur une bande horizontale, les fils de donnée en dessous. **La ligne de vie
   d'un blueprint devient une ligne géométrique**, et c'est ce qui permet de le
   lire en diagonale.

`83576832` fait mieux sur un point qu'il faut nommer : ses fils d'exécution sont
des **polylignes à angles** quand ses fils de donnée sont des courbes. **Un
quatrième signal, gratuit, et qui survit au noir et blanc.** 🔴 **NON TRANCHÉ** :
c'est un écart de style avec la principale, qui n'a que des courbes.


### ✅ C5 · Le fil sélectionné — **MESURÉ le 23/08**, et le résultat est un NON

> Le § 10 disait : *« halo ? éclaircissement ? Je n'ai aucune référence. »*
> Banc `essai_c5.py` : CIEDE2000 sur les sept familles, planche rendue en PNG et
> **regardée** sur le fond réel du canevas.

#### La question posée, et pourquoi elle a deux contraintes et non une

Éclaircir un fil sélectionné doit **se voir**, mais **sans rapprocher les
familles entre elles** — sinon on gagne un signal d'état en détruisant le code
couleur des types, qui a coûté tout le § 4.2. Les deux se mesurent avec la même
métrique.

| éclaircissement | écart **perçu** du changement (min sur 7 familles) | séparation **entre familles** (plancher § 4.2 = **11,0**) | verdict |
|---|---|---|---|
| +20 % | **2,74** | 17,25 | ❌ imperceptible |
| **+35 %** | **5,23** | **14,26** | 🟡 visible, marge confortable |
| **+50 %** | **8,21** | **11,42** | 🟡 net, **marge de 0,42 seulement** |
| +65 % | 11,81 | **7,96** | ❌ **casse le code couleur** |
| +80 % | 16,19 | 4,58 | ❌ casse |

#### 🔴 Le résultat : **aucune valeur ne satisfait les deux**

**Le premier taux qui rendrait le changement aussi lisible qu'une différence de
type (ΔE ≥ 11,0) est +65 % — et à +65 % la séparation entre familles tombe à
7,96, donc sous ce même plancher.** Les deux contraintes se croisent **avant** de
se rencontrer.

> ⚠️ **La luminosité ne peut pas porter seule le signal de sélection.** Ce n'est
> pas un défaut de réglage, c'est une propriété de la palette : elle occupe déjà
> tout l'espace disponible.

#### ⚠️ Et je me trompais de couleur

J'avais annoncé `#9AA3AD` (quelconque) comme maillon faible parce qu'il est
« déjà clair ». **Mesuré : le maillon faible est `#81EBEB` (référence)**, un cyan
saturé — ΔE de 5,23 à +35 % contre 9,13 pour `#9AA3AD`. **Ce n'est pas la
clarté qui résiste à l'éclaircissement, c'est la saturation** : un cyan très
saturé est déjà proche du blanc en luminance, il lui reste peu de chemin.

#### ✅ Ce que le PNG regardé montre, et qu'aucun calcul n'annonçait

**Les deux poignées se voient immédiatement, à tous les taux, sur toutes les
familles — l'éclaircissement, lui, se devine.** Sur la planche d'essai, la ligne
du bas (avec poignées) se distingue de celle du haut d'un coup d'œil, y compris
là où l'éclaircissement est indétectable.

**DÉCIDÉ, et c'est l'inverse de ce que ce document proposait :**

| | rôle |
|---|---|
| **les deux poignées carrées de 7 px** aux extrémités | ✅ **le signal, et le SEUL** |
| ~~l'éclaircissement à +35 %~~ | ❌ **RETIRÉ le 23/08 — voir ci-dessous** |

### ❌ L'éclaircissement est RETIRÉ — un renfort imperceptible est un décor

J'avais gardé les +35 % comme « renfort ». **Question posée en regardant la
planche 06 : à quoi sert un renfort qui est sous le seuil de perception ?** Deux
réponses possibles, et elles ne se valent pas : *une redondance qui ne coûte
rien et couvre le cas où les poignées sont hors écran*, ou *un décor qui a l'air
d'un signal*. **Mesuré plutôt que supposé** (`essai_c5b.py`).

⚠️ **Ce que la première mesure ne pouvait pas dire** : les 5,23 valaient pour une
comparaison **temporelle** (le même fil, avant et après, de mémoire). Le cas
« poignées hors cadre » est une comparaison **spatiale et simultanée** — plusieurs
fils de même couleur à l'écran, un seul éclairci. **Le seuil n'y est pas le même,
et il ne se déduit pas.** Planche d'essai : cinq fils bleus identiques, un seul
éclairci, poignées hors cadre.

| cas | le 3ᵉ fil se repère-t-il ? |
|---|---|
| poignées visibles | ✅ **immédiatement** |
| +35 %, poignées hors cadre | ❌ **non** |
| +50 %, poignées hors cadre | ❌ non |
| +65 %, poignées hors cadre | 🟡 à peine — et à +65 % le code couleur casse |

✅ **Conclusion : le renfort ne sert ni isolément ni en comparaison simultanée.
C'est un décor.** On vient de passer une nuit à chasser ce qui ressemble à une
mesure sans en être une ; **un « renfort » imperceptible en est une.** Retiré.

### ✅ Ce qui le remplace — **la poignée ne disparaît JAMAIS**

Retirer l'éclaircissement laisserait le cas « les deux prises hors cadre » sans
aucun signal. **La solution n'est pas chromatique, elle est géométrique** — et
c'est le même raisonnement que les trois gris : *le signal n'est pas la couleur,
c'est l'endroit.*

> **Quand une extrémité sort du cadre, sa poignée se pose là où le fil COUPE le
> bord, en chevron pointant vers l'extérieur.**

✅ **Vérifié sur la planche d'essai (cas f) : le fil sélectionné se repère
immédiatement, sans aucun éclaircissement.**

**Et le chevron garde les deux propriétés qui ont fait choisir les poignées :**

1. il **reste à l'écran** quoi qu'il arrive — un signal qui peut sortir du cadre
   n'est pas un signal ;
2. il **informe encore** : sa pointe dit **de quel côté** se trouve la prise, ce
   qu'un halo ou une teinte ne diraient pas.

**Coût** : l'intersection du fil avec le bord du cadre, qui est de toute façon
calculée pour ne pas dessiner ce qui est hors champ.

📌 **Bénéfice qui n'était pas cherché, et il faut le garder écrit ICI** : les
poignées **désignent les deux prises reliées** — exactement ce qu'on veut savoir en
sélectionnant un fil dans un graphe dense. **Le signal le plus fort est aussi le
seul qui informe.**

⚠️ **C'est l'argument à opposer à toute simplification future.** Quelqu'un
proposera un jour « un simple halo » ou « juste plus clair », parce que c'est moins
de code. Ce sera plus simple **et ça n'apprendra rien** : un halo dit *ce fil est
sélectionné*, les poignées disent *ce fil relie CETTE prise à CELLE-LÀ*. On ne
remplace pas un signal qui informe par un signal qui décore.

⚠️ **Ce qui reste à vérifier au dessin** : deux fils sélectionnés dont les
poignées se superposent, et le fil très court où les deux poignées se touchent.
**Non mesuré.**

### ✅ C6 · Le croisement de fils — **DÉCIDÉ le 23/08 : ne rien faire**

Les fils se croisent, simplement. Pas de saut, pas de passage dessous.

1. **aucune des douze références du corpus ne fait de saut** ;
2. **le saut vient des schémas électriques, où les fils sont ORTHOGONAUX** — deux
   segments perpendiculaires qui se coupent sont réellement ambigus ; **nos fils
   sont des courbes de directions différentes, l'œil les suit sans aide** ;
3. **le coût croît en n²** — **190** paires à 20 fils, **1 225** à 50,
   **19 900 à 200**, et il faut recommencer à chaque déplacement de nœud.

✅ **N'empêche rien** : le saut est purement du rendu, ajoutable plus tard sans
toucher au modèle. 📌 **Garder le chiffre à côté de la décision** — c'est lui qui
évitera qu'on la rouvre tous les six mois.

---

## 11. Tous les états

### 11.1 Erreur — **PROPOSÉ**

- filet du corps `#E4443C`, 1,5 px ; corps teinté `#2A1A1A` ;
- **`!` dans l'en-tête**, à gauche du `?` ;
- ⚠️ **la raison est écrite DANS le nœud**, en pied, `#E4443C`, 10 px :
  `fichier introuvable : textures/mur_albedo.png`.

**La raison lisible sans survol n'est pas un confort.** `CATALOGUE_NOEUDS.md`
l'exige deux fois, et `images (2)` montre pourquoi : dans un graphe de trente
nœuds, **survoler trente nœuds pour trouver celui qui a un problème est une
minute perdue à chaque fois.**

### 11.2 Avertissement — **PROPOSÉ** (état nouveau)

Ni dans le corpus, ni dans nos documents. **Il manque, et le § 6.2 le prouve** :
une texture sRVB branchée sur une entrée `Normale` produit un graphe **valide et
faux**. L'erreur serait un mensonge (ça compile), le silence aussi (ça ne marche
pas).

- filet `#F79A28` **pointillé** (l'orange déjà connu, mais en pointillé pour ne
  pas se confondre avec le filet d'exécution plein) ;
- `⚠` dans l'en-tête ; raison en pied, `#F79A28`.

### 11.3 Désactivé — **PROPOSÉ**

Corps et en-tête à **45 % d'opacité**, texte à 40 %. **Les prises et les fils
restent à pleine opacité** — sinon on croit que le nœud est déconnecté alors
qu'il est seulement éteint.

⚠️ **Signal supplémentaire, et il porte tout le sens** : **le fil qui le traverse
passe en pointillé de part en part.** Un nœud désactivé dans une chaîne ne coupe
pas la chaîne, il la laisse passer ; le pointillé le dit à distance, sans lire le
nœud.

### 11.4 Survolé, sélectionné, actif — ✅ **DÉCIDÉ le 23/08** (le survol éclaircit le FILET)

`ELEMENTS_A_DESSINER.md` D7 laisse le survol ⚠️. Le corpus, lui, répond — et il
répond mieux que prévu, parce qu'il montre **trois** états là où nos documents
en prévoyaient deux.

| état | dessin | source |
|---|---|---|
| **survolé** | **filet du corps éclairci** (`#33333C` → `#5A5A68`). **Jamais le corps** | PROPOSÉ — teinter le corps au survol fait « clignoter » le canevas quand on le traverse à la souris |
| **sélectionné** | **bordure `#F79A28` de 1,6 px sur tout le nœud, en-tête comprise**, épousant les rayons (0 en bas, 5 en haut) | **VU** (`images (3)` : bordure rouge-orange ; `174057` : liseré bleu ; `node-based` : liseré bleu). **Trois références, trois couleurs, une seule convention : la bordure.** |
| **actif** | ⚠️ **en-tête ÉCLAIRCI de 15 %**, en plus de la bordure de sélection | **VU** (`images (3)` : un seul nœud a l'en-tête bleu clair, ses jumeaux l'ont bleu sombre, et tous portent la bordure rouge) |

⚠️ **`sélectionné` et `actif` sont deux états différents, et le corpus le prouve.**
Nos documents ne les distinguaient pas. Un éditeur qui ne les sépare pas **ne
sait pas à qui envoyer le clavier** quand douze nœuds sont sélectionnés.

**DÉCIDÉ** (rappel) : la sélection est **une bordure, jamais une teinte du
corps**. Raison confirmée par le corpus : la teinte du corps est déjà prise par
l'erreur et le désactivé.

### 11.5 Pendant le tirage d'un fil — **PROPOSÉ**

C'est, d'après `CATALOGUE_NOEUDS.md` § 2, *« le retour visuel le plus utile de
tout l'éditeur »*, et **aucune référence ne le montre.**

| prise | dessin |
|---|---|
| **compatible** | halo clair de 1,5 px, couleur du type, à 60 % |
| **convertible** (conversion déclarée dans ce sens) | halo **pointillé**, couleur à 35 % |
| **incompatible** | **éteinte** : `#3A3A44`, 30 % d'opacité |

⚠️ **La distinction compatible / convertible n'est pas un raffinement.**
`CATALOGUE_NOEUDS.md` § 2 le pose : *« réel → couleur existe, couleur → réel
n'existe pas »*. **Les conversions sont dirigées** : si le dessin ne montre que
deux états, l'utilisateur apprend une règle symétrique qui est fausse, et il la
découvre en tirant le fil dans l'autre sens.

**PROPOSÉ** : pendant le tirage, **les nœuds entiers dont aucune prise n'est
compatible passent à 50 %.** Raison : dans un graphe de trente nœuds
(`images (2)`), éteindre des prises de 17 px ne se voit pas ; éteindre des nœuds,
oui. **Le retour doit être visible à l'échelle où on travaille, pas à celle où on
dessine la spécification.**

### 11.6 Le graphe invalide doit rester dessinable — **DÉCIDÉ** (rappel)

`CATALOGUE_NOEUDS.md` § 6 : *« un éditeur passe son temps dans ces états. »*

| cas | dessin |
|---|---|
| **lien pendant** | fil qui s'arrête en l'air, terminé par un **petit disque creux** |
| **cycle en cours de construction** | **les fils du cycle passent en `#E4443C`**, tous ensemble — le cycle se voit comme une boucle rouge, pas comme un nœud fautif |
| **entrée obligatoire vide** | **prise creuse cerclée de `#E4443C`**, et le nœud passe en avertissement (§ 11.2), pas en erreur |

---

## 12. Le canevas

### 12.1 La recherche de nœud filtrée — **PROPOSÉ**

`ELEMENTS_A_DESSINER.md` E1, ⬜. **PROPOSÉ**, et c'est le pivot du § 5.6.

Quand on tire un fil dans le vide, le panneau s'ouvre au curseur, **filtré par le
type de la prise d'origine**, et son en-tête dit le filtre **en toutes lettres** :

```
  ┌──────────────────────────────────┐
  │ 🔍 _________________             │
  │ qui acceptent   [1.0] tableau de │  ← le filtre, écrit
  │                       réels    ✕ │  ← et retirable
  ├──────────────────────────────────┤
  │  Pour chaque            Boucle   │
  │  Longueur               Tableau  │
  │  Élément à l'indice     Tableau  │
  └──────────────────────────────────┘
```

⚠️ **Le filtre doit être écrit et retirable.** Un filtre invisible transforme
« ce nœud n'existe pas » en conclusion, alors que la vraie phrase est « ce nœud
n'accepte pas ce type ». C'est la même faute que la face n° 8 du `CLAUDE.md`
parent — **un résultat négatif sans son périmètre est une rumeur.**

### 12.2 Le dézoom — trois paliers — **DÉCIDÉ**, la mesure en plus

**DÉCIDÉ** dans `ELEMENTS_A_DESSINER.md` D11. Ce que j'ajoute, c'est **la mesure
qui justifie les seuils** : `images (2)` et `images (5)` sont deux références
prises précisément à ces échelles, et elles montrent ce qui survit.

| palier | ce qui reste | ce qui disparaît |
|---|---|---|
| **100 %** | tout | — |
| **55 %** | en-tête, titre, étiquettes de rangée, prises, filet, **aperçus (réduits)**, **barres d'édition** | **valeurs et champs de saisie** |
| **25 %** | **un rectangle de la couleur de la catégorie**, le filet exécution/pétrole, les fils, **la couleur moyenne d'un aperçu** | tout le texte, toutes les prises, les commentaires |

**La mesure qui le fonde** : dans `images (2)` — un vrai graphe de trente nœuds —
**les libellés sont déjà illisibles alors que les couleurs d'en-tête et les
couleurs de fil restent parfaitement lisibles.** Dans `images (5)`, plus dézoomé
encore, **seuls les fils structurent l'image**.

> ⚠️ **Conséquence, et c'est l'argument qui commande toute la palette** : à 25 %,
> **la couleur d'en-tête et la couleur de fil sont TOUT ce qu'il reste.** C'est
> pour ça que les couleurs de catégorie doivent être fortement distinctes, et
> c'est pour ça que le § 4.1 refuse d'en inventer seize.

**PROPOSÉ — l'ordre de disparition** : *valeurs → étiquettes → prises → titre*.
Raison : on dézoome pour lire la **structure**, jamais les nombres. Ce qui sert à
la structure part en dernier.

### 12.3 Le reste du canevas — ✅ **DÉCIDÉ le 23/08** (E3 reportée, E4 supprimée, fil d'Ariane adopté)

| élément | état |
|---|---|
| **sélection au lasso** | ✅ **DÉCIDÉ 23/08 — et PAS en pointillé** : filet **plein** `#F79A28` 1,5 px + remplissage orange à **8 %**. Voir § 12.4 |
| **fil d'Ariane de sous-graphe** | ✅ **DÉCIDÉ 23/08 — OBLIGATOIRE.** Bande de **24 px**, chemin d'**instanciation**, segments cliquables, **élision au milieu** au-delà de 4 niveaux, et ⚠️ **un compteur d'instances** : *on entre par UNE instance et on édite la DÉFINITION, qui est PARTAGÉE* — sans lui on croit corriger un nœud et on en corrige trois. **Cette phrase justifie le compteur : ne pas la retirer, sinon quelqu'un le retirera comme décoratif** |
| **aide contextuelle** | ligne de texte libre en haut du canevas — **VU** (`node-based-…webp`) ; **PROPOSÉ : oui, elle ne coûte rien et remplace un tutoriel** |
| **menu contextuel** | ✅ **DÉCIDÉ 23/08** : trois contenus, et **« ajouter un nœud » rouvre le panneau du § 12.1, sans filtre** — un seul mécanisme de recherche. Voir § 12.5 |
| **minicarte** | ✅ **DÉCIDÉ 23/08 — REPORTÉE.** `cadrer tout` sert le même besoin pour bien moins cher. 🔴 **Critère de réouverture** : le premier graphe réel de plus de 50 nœuds où l'on se perd |
| **barre d'outils** | ✅ **DÉCIDÉ 23/08 — AUCUNE.** Les trois actions utiles (`cadrer tout`, `zoom 100 %`, **niveau de zoom en texte**) vont à droite de la bande du fil d'Ariane, qui existe de toute façon : **coût de place nul** |

---

### 12.4 Le lasso — ✅ **DÉCIDÉ le 23/08**, et le pointillé est REFUSÉ

Ce document proposait un **rectangle pointillé**. ⚠️ **Recensement fait avant
d'écrire : le pointillé est déjà employé six fois.**

| usage | l'endroit | la forme |
|---|---|---|
| dictionnaire (§ 5.3) | sous une **prise** | trait double, inférieur pointillé |
| fil en cours de tirage | un **fil** au curseur | pointillé gris |
| nœud désactivé (§ 11.3) | le **fil traversant** | pointillé de la couleur du fil |
| prise convertible | un **halo de prise** | pointillé |
| **cadre (§ 9.2)** | **un RECTANGLE sur le canevas** | second filet intérieur, pointillé `4 4` |
| lasso *(proposé)* | **un RECTANGLE sur le canevas** | pointillé orange |

✅ **Quatre des six ne se gênent pas** — prise, fil, halo : *le signal n'est pas
le pointillé, c'est l'endroit*, exactement comme les trois gris.

❌ **Mais les deux derniers occupent le MÊME endroit**, et la collision n'est pas
seulement visuelle : **tirer un rectangle sur le fond est aussi le geste qui crée
un cadre.** Un rectangle pointillé qui apparaît sous le curseur pendant ce geste
dit littéralement « je suis en train de fabriquer un cadre ».

> ✅ **DÉCIDÉ : le lasso est un filet PLEIN `#F79A28` de 1,5 px, avec un
> remplissage orange à 8 %. Aucun pointillé.**

**Et il a le droit à un signal simple**, pour la raison déjà admise au § 11.4 sur
le survol : **il est transitoire et attaché au curseur.** L'utilisateur sait ce
qu'il fait pendant qu'il le fait — c'est ce qui reste après le relâchement qui
doit être fort.

📌 **Le remplissage n'est pas une décoration** : sans lui, un lasso réduit à un
filet est invisible dès qu'il croise un nœud clair. **Avec lui, la zone prise est
lisible d'un coup d'œil**, ce qui est exactement l'information cherchée.

### 12.5 Le menu contextuel — ✅ **DÉCIDÉ le 23/08**

Trois contenus, selon ce qui est sous le curseur :

| clic droit sur… | ce que le menu contient |
|---|---|
| **le fond** | *ajouter un nœud* · coller · cadrer tout · **créer un cadre autour de la sélection** |
| **un nœud** | couper/copier/dupliquer · **désactiver** · grouper en sous-graphe · supprimer |
| **un fil** | ⚠️ **insérer un nœud ICI** · supprimer le fil |

⚠️ **« Insérer un nœud ici » est la seule entrée qu'on ne peut pas obtenir
autrement.** Sans elle, insérer un nœud au milieu d'un fil demande de couper,
poser, rebrancher deux fois — quatre gestes pour un.

> ✅ **DÉCIDÉ : « ajouter un nœud » ouvre le MÊME panneau que le § 12.1, sans
> filtre.**

📌 **C'est la règle du fil d'Ariane appliquée une seconde fois** : *deux chemins
pour entrer dans un groupe, c'est deux chemins pour se perdre.* Deux panneaux de
recherche d'apparence différente — un « filtré » et un « complet » — feraient
croire à deux catalogues de nœuds différents. **Il n'y en a qu'un ; le filtre est
un état du panneau, pas un autre panneau.**

### 12.6 La recherche sans résultat — ✅ **DÉCIDÉ le 23/08**

Conséquence directe de la règle du § 12.1, et elle ne s'invente pas :

> ❌ **Une liste vide est INTERDITE.** Le panneau affiche
> **« aucun nœud n'accepte *tableau de réels* »** — le type en toutes lettres —
> avec le bouton ✕ qui retire le filtre **juste à côté**.

⚠️ **Une liste vide sous un filtre invisible fait lire « ce nœud n'existe pas »
alors que le fait est « ce nœud n'accepte pas ce type ».** Un résultat négatif
sans son périmètre est une rumeur — et ici la rumeur ferait abandonner un nœud
qui existe.

## 13. Récapitulatif des 🔴 NON TRANCHÉS — **13 restants sur 14**

**À lire avant tout codage.** Chacun de ces points sera décidé en passant s'il
n'est pas décidé exprès — et décidé en passant, il sera incohérent.

| # | question | où | pourquoi ça coûte |
|---|---|---|---|
| 1 | **largeur du nœud** : fixe, adaptative ou redimensionnable ? | § 2.3 | une largeur libre doit être **sérialisée** |
| 2 | **séparer une struct casse-t-il le lien** existant ? | § 4.6 | touche le modèle et l'annulation |
| 3 | **fil de dictionnaire** : pointillé (collision avec le tirage) ou triple trait ? | § 5.3 | signal ambigu si mal choisi |
| 4 | **`corps de boucle` / `terminé`** : un signal plus fort qu'une ligne vide ? | § 5.5 | la faute n° 1 du débutant |
| 5 | **shader** : forme de prise propre, ou couleur + glyphe ? | § 6.3 | une septième forme de prise |
| ~~6~~ | ~~**groupe** : sur place ou en onglet ?~~ | § 7.6 | ✅ **TRANCHÉ le 23/08 : SUR PLACE, pas d'onglets.** *Deux mécanismes de navigation pour une seule notion, c'est un de trop* — et le fil d'Ariane, devenu obligatoire, rend l'onglet redondant. 🔴 Reste seulement : **le groupe montre-t-il un aperçu de son contenu ?** |
| ~~7~~ | ~~**cadre** : teinte-t-il ses nœuds ?~~ | § 9.2 | ✅ **TRANCHÉ le 22/08 par Rodolf — option A : JAMAIS, nulle part.** Ni corps, ni en-tête, ni filet, ni pastille ; le bandeau porte le sens. ✅ **Fait disparaître le conflit à trois** |
| 8 | **cadre replié** : où aboutissent les fils entrants ? | § 9.2 | aucune référence. **Deux candidats chiffrés à 12 fils** au § 9.2 ; ne concerne **que** le cadre, jamais le groupe |
| 9 | **fil sélectionné** : halo ou éclaircissement ? | § 10 | déjà ⚠️ dans C5 |
| 10 | **croisement de fils** : dessus, dessous, saut ? | § 10 | déjà ⚠️ dans C6 |
| 11 | **fils d'exécution en polylignes** à angles, ou en courbes comme la principale ? | § 10 | écart de style assumé ou non |
| 12 | **minicarte** et **barre d'outils** | § 12.3 | déjà ⚠️ dans E3, E4 |
| 13 | **champ de saisie** : plus clair (mesuré sur la principale) ou plus sombre (A9 actuel) ? | § 1.1 | ⚠️ **contredit un document déjà écrit** |
| 14 | **`images (1)`** : cohérence avec l'inspecteur, ou autre attente ? | § 0 | je refuse de deviner |
| ~~18~~ | ~~**l'échelle du nœud de Rodolf**~~ | § 3.1 | ✅ **MESURÉE : 1 unité = 1 px, son dessin est une maquette à 6 ×.** L'en-tête de 21 px est déjà juste ; 🔴 **reste à décider qui s'aligne** — les planches, ou son nœud de référence |
| ~~15~~ | ~~**appartenance à un cadre** : géométrique ou déclarée ?~~ | § 9.2 | ✅ **TRANCHÉ : lien explicite sérialisé.** La géométrie ne sert qu'au dépôt. **C'est le repli du cadre qui l'exigeait** — on ne replie pas ce qu'on ne sait pas énumérer |
| ~~16~~ | ~~**une directive inconnue casse-t-elle la relecture ?**~~ | § 16 | ✅ **MESURÉ : non, elle est ignorée** — mais elle est **perdue à la réécriture**, et une directive *mal formée*, elle, **corrompt en silence** |
| 17 | **regroupement** : entrée/sortie dédupliquée, ordre, homonymes | § 7.6 | ✅ les quatre règles sont écrites ; 🔴 **reste à EXÉCUTER le critère grouper→dégrouper** |

---

## 14. Les planches

| planche | état | contenu |
|---|---|---|
| `references/planche_01_noeuds.svg` | ✅ | les formes de base dans le style de la principale |
| `references/planche_02_types.svg` | ✅ | les 7 familles, les 16 types, pastilles, prises, **tableaux et dictionnaires** |
| `references/planche_03_formes.svg` | ✅ | les 14 formes de nœud, dont custom, groupe, itérateur, aperçu |
| `references/planche_04_etats.svg` | ✅ | tous les états, le tirage, les trois gris, les trois paliers de dézoom |
| `references/planche_05_matieres.svg` | ✅ | aperçu ON/OFF · texture · sRVB sur Normale · matériau · aperçu périmé · la règle absolue · l'aperçu au dézoom · commentaire · cadre · les deux relais |
| `references/planche_06_decisions.svg` | ✅ | **les décisions de la nuit du 22 au 23/08**, qui n'existaient nulle part en image : le **fil d'Ariane** (avec le compteur d'instances) · le **fil sélectionné** mesuré, montré à côté d'un fil normal et d'un fil d'exécution · le **nœud indisponible** · le **nœud survolé** · **les TROIS GRIS côte à côte** |
| `references/planche_07_canevas.svg` | ✅ | **le lasso contre le cadre** (la collision, avant/après) · la recherche sans résultat · la source · le puits · le commentaire au repos et au survol · les trois menus contextuels |
| `references/essai_c5.svg` | 🟡 | **banc de mesure, pas une planche** : le même fil à +20/+35/+50 %, avec et sans poignées, sur les sept familles. Il a servi à trancher C5 et il reste consultable |
| `references/essai_c5b.svg` | 🟡 | **second banc** : cinq fils identiques, un seul éclairci, **poignées hors cadre**. Il a montré que le renfort ne sert pas non plus en comparaison simultanée — et que **le chevron sur le bord**, lui, suffit seul |

Rendu PNG : **`gen.rendre(nom, largeur, hauteur)`**, qui construit l'URL absolue,
vérifie le poids du PNG **et compare ses dimensions au `viewBox`**. ⚠️ **Ne plus
appeler `msedge` à la main** : les trois contrôles sont là pour des pannes qui se
sont réellement produites, et qui passent toutes les vérifications naïves.

⚠️ **L'URL doit être absolue et préfixée `file:///`.** Passé en chemin relatif,
Edge traite l'argument comme un **nom d'hôte**, ne trouve aucun DNS, et
**capture sa propre page d'erreur** : le PNG sort à la bonne taille, sans rien
signaler. Le seul indice est son poids — quelques dizaines de Ko au lieu de
quelques centaines.

### 14.1 Deux pièges du générateur, tous deux **silencieux**

Notés parce qu'ils ont chacun produit un fichier **valide et faux** — la pire
catégorie de panne, celle qui ne crie pas :

1. ⚠️ **Les demi-caractères étaient JETÉS, pas recombinés.** Un caractère hors
   du plan de base écrit en deux moitiés dans un script était supprimé à
   l'écriture. La planche 02 est ainsi restée **amputée de deux marqueurs** (le
   rond des points non tranchés, la loupe) sans que le SVG soit invalide une
   seule seconde. Corrigé : on recombine avant de filtrer.
2. ⚠️ **Le rendu PNG ne vérifiait pas ce qu'il capturait** (piège ci-dessus).

**Règle qui en découle : une planche n'est pas « faite » quand le script se
termine sans erreur — elle l'est quand le PNG a été REGARDÉ.** Les deux pannes
ci-dessus passent tous les contrôles automatiques : XML bien formé, taille de
fichier plausible, code de retour nul.

---

## 15. Ce que ce document N'A PAS fait

Par honnêteté, et pour que personne ne croie le contraire :

1. **Il n'a mesuré aucune lisibilité réelle.** Les distances de couleur sont
   calculées, pas observées sur un écran. La première planche rendue en PNG et
   regardée à 55 % et à 25 % peut contredire le § 12.2 — **et c'est elle qui
   aura raison.**
2. ~~**Il n'a pas vérifié que le modèle NKGraph porte ces notions.**~~ ✅ **Fait
   le 22/08** — relèvement des cinq fichiers du module **et** trois bancs de
   mesure exécutés. Résultat au **§ 16** : onze notions n'ont pas de foyer, le
   registre de types est **plat** (aucun constructeur `tableau DE`), et le
   format ne conserve pas ce qu'il ne comprend pas. ~~⚠️ **En revanche je n'ai
   toujours pas lu les CONSOMMATEURS** (`NkMatGraphCheck`), qui portent
   peut-être déjà des conventions pour une partie de ces onze.~~ ✅ **LU
   INTÉGRALEMENT le 23/08 — § 17.** Ils en portent huit, **ils en contredisent
   huit**, et ils n'ont toujours **aucune** des onze notions sans foyer. Le trou
   est fermé ; l'écart est le § 17.
3. **Il n'a pas décidé le sens de lecture.** Tout est écrit pour un graphe
   **horizontal** (la principale l'est). `secondaire_prises_verticales.png`
   montre l'autre monde, et `DESIGN_EDITEUR_NODAL.md` § 2 déconseille de mélanger
   les deux. **Rien ici ne l'interdit formellement** — mais rien ne le prévoit
   non plus.

---

## 16. ⚠️ MESURÉ — ce que la sérialisation fait de ce qu'elle ne comprend pas

> C'était la **question ouverte n° 1** de ce chantier, et la seule dont dépendait
> le logement des notions sans foyer. Je l'avais laissée non mesurée en disant
> que je ne la supposerais pas. **Elle est mesurée.**

**Protocole** — bancs `mesure_directive_inconnue.cpp` et `mesure_malformee.cpp`
compilés contre le vrai `NKGraph` (g++ 16.1, 62 unités de NKCore / NKMemory /
NKContainers liées), exécutés le 2026-08-22. On sérialise un graphe témoin non
trivial (2 nœuds, 4 prises, 2 types, 1 conversion, 1 lien), on abîme le flux
texte d'une façon précise, on désérialise, et on **recompare octet pour octet**
la resérialisation au témoin.

### 16.1 Directive INCONNUE → **cas 1 : ignorée**, avec deux nuances

| cas | ce qu'on a inséré | résultat mesuré |
|---|---|---|
| **A** | `cadre 7 0 0 400 300 Mon Cadre` après une ligne `noeud` | ✅ `true`, graphe **identique octet pour octet** |
| **B** | `commentaire 1 50 50 …` juste après l'en-tête | ✅ identique |
| **C** | la même directive **avant** la ligne `nkgraph` | ⚠️ **`false` — refusée.** L'en-tête doit rester la **première** ligne |
| **D** | `noeudcadre …` — un mot-clé **préfixé** d'un mot-clé connu | ✅ non confondu avec `noeud` : la comparaison porte sur le jeton entier |
| **E** | une ligne vide | ✅ sans effet |
| **G1** | une inconnue **au niveau document**, entre `racine` et `graphe` | ✅ ignorée |
| **G2** | une inconnue dans le **corps** d'un graphe d'un document | ✅ ignorée |
| **G3** | `graphecadre` — préfixe de `graphe` au niveau document | ✅ non confondu : 1 graphe, pas 2 |

### 16.2 ⚠️ Le fait qui change la conclusion : ignorée, mais **PERDUE**

| cas | mesure |
|---|---|
| **F** | on charge un fichier contenant `cadre …`, on modifie le graphe, on **annule une fois** — la directive **a disparu** |

**C'est le résultat le plus important des bancs, et il n'était dans aucun des
trois cas annoncés.** `NkGraphHistory` resérialise **depuis le modèle**, pas
depuis le texte. Une directive que le modèle ne porte pas ne survit donc **ni à
un annuler, ni à une sauvegarde** — la première écriture l'efface.

### 16.3 ⚠️ Le TROISIÈME cas existe bel et bien — il est ailleurs

Une directive **connue mais mal formée** corrompt en silence. Dans **tous** les
cas ci-dessous, `Deserialize` rend **`true`** et le graphe est resérialisé sans
un mot :

| ce qu'on donne | ce que le modèle en fait |
|---|---|
| `noeud abc 10 20 math.add A` — identifiant non numérique | ❌ **un nœud d'identifiant `0`**, c'est-à-dire `NK_NODE_INVALID`, **vivant et compté** |
| `noeud 2` — deux jetons au lieu de cinq | ❌ un nœud de type vide et de libellé vide, vivant ; la ligne réécrite se termine par **deux espaces** |
| `lien 1 1 0 99 0` — vers un nœud absent | ❌ **le lien pendant est conservé**, vivant, et réécrit tel quel. C'est exactement ce que `RemoveNode` s'interdit de laisser — *« laisser des liens pendants serait pire qu'une suppression refusée »* — mais la relecture, elle, en fabrique |
| deux `noeud` de **même identifiant** | ❌ les deux sont acceptés ; `Find` ne rendra jamais que le premier, le second est inatteignable mais réécrit à chaque sauvegarde |
| `sock 42 …` **avant** que le nœud 42 existe | ❌ **la prise est perdue en silence**. ⚠️ Et c'est le pire des cinq : **l'ordre des prises EST leur index**, et les liens s'y réfèrent par numéro. Une prise avalée ne perd pas *une* prise — **elle décale toutes les suivantes**, et les liens pointent alors sur la mauvaise |

✅ **Un seul se répare tout seul**, et c'est celui que l'en-tête annonçait :
`compteurs` absente est reconstruite par le balayage final (mesuré : `compteurs 8`
pour un nœud d'identifiant 7). **La ligne la plus importante du fichier est aussi
la seule qui se rattrape.**

📌 **Ce que je remonte, sans le trancher** : `Deserialize` gagnerait à rendre une
**raison**, comme `Connect` et `BuildPlan` le font déjà (`NkLinkError`,
`NkPlanError`). Le modèle a ce réflexe partout ailleurs — *« on REND une raison
plutôt qu'un simple booléen »* — et c'est la relecture, là où les données
viennent du dehors, qui en aurait le plus besoin. Aujourd'hui son booléen vaut
`true` partout sauf si l'en-tête manque.

### 16.4 Les conclusions, et ce ne sont pas celles qu'on attendait

1. ✅ **Ajouter des directives est rétro-compatible dans le sens ancien → neuf.**
   Un fichier écrit par une version future se relit sans erreur dans une version
   ancienne. **Aucune migration de format n'est nécessaire.**
2. ❌ **Mais pas dans l'autre sens.** Ouvrir un fichier récent avec une version
   ancienne, puis **simplement annuler une fois ou enregistrer**, détruit tous
   les cadres, commentaires et replis. **Silencieusement** est le mot :
   `Deserialize` rend `true`, rien ne prévient, le fichier réécrit paraît sain.
   C'est le défaut que l'en-tête dénonce à propos de `compteurs` : *« un défaut
   qui ne se voit qu'au résultat, longtemps après. »*
3. ⚠️ **Donc « où loger les notions manquantes » ne se règle PAS par la
   rétro-compatibilité de lecture.** Ça se règle par **le modèle** : tant qu'une
   notion n'est pas un champ de `NkNode` / `NkNodeGraph`, l'écrire dans le
   fichier ne la conserve pas. **Il n'y a pas de raccourci par le format.**

### 16.5 Où loger les notions sans foyer

| option | ce que la mesure en dit |
|---|---|
| ranger l'état d'interface dans `NkNode::props` | ❌ **toujours refusé** — l'auteur du modèle a écrit noir sur blanc pourquoi (*« une convention de nommage finit toujours par être violée »*), et ça reste vrai |
| ajouter des directives **sans** champs de modèle | ❌ **éliminé par la mesure 16.2** : elles ne survivent pas à un annuler |
| ajouter les champs au modèle, **avec une directive dédiée par notion** | ✅ **la seule qui tienne**, et le modèle a déjà ce réflexe : `sousgraphe` est une directive dédiée plutôt qu'un champ de plus sur la ligne `noeud`, *« pour que les fichiers écrits avant restent lisibles tels quels »* |

**Les notions concernées** (relevées sur les cinq fichiers du module, zéro
occurrence de `collaps`, `repli`, `enabled`, `width`, `comment`, `frame`,
`reroute`) : largeur du nœud · repli du nœud · repli par section · tableau ·
dictionnaire · état plié/séparé d'une struct · bascule d'aperçu · drapeau
« exécute » d'un custom · commentaire libre · **cadre** · **désactivé**.

⚠️ Deux précisions qui changent la forme de la liste :

- **`cadre` y entre comme RELATION**, pas seulement comme rectangle (§ 9.2) : il
  lui faut un rectangle, un titre, une teinte, un repli **et la liste de ses
  membres** ;
- **`alive` n'est PAS « désactivé »**. `alive` marque la **suppression** et coupe
  les liens ; un nœud désactivé **reste branché** et le fil le traverse en
  pointillé. Il faut un **second booléen**, pas un détournement du premier —
  sinon éteindre un nœud débrancherait ses voisins, résultat plausible et faux.

---

## 17. ✅ FERMÉ — l'écart avec `NkMatGraphCheck`, le dernier consommateur non lu

> C'était le **dernier trou déclaré** de ce chantier, et le seul endroit où cette
> spécification pouvait décrire autre chose que ce qui existe. Le § 15 point 2
> disait : *« je n'ai toujours pas lu les CONSOMMATEURS, qui portent peut-être
> déjà des conventions pour une partie de ces onze »*. **Ils en portent — et ils
> en contredisent huit.**

**Protocole (2026-08-23)** : lecture intégrale de
`Applications/NkMatGraphCheck/src/main.cpp` (**5 820 lignes**, ~120 cas), plus
les deux en-têtes qu'il consomme —
`NKRenderer/Materials/Graph/NkMatGraphTypes.h` et `NkMatGraphCompile.h` — dans
l'arbre `Nkentseu-matgraph`, en **lecture seule**. Chaque affirmation ci-dessous
cite le cas ou la déclaration qui la fonde. **Aucune ligne de leur code n'a été
touchée.**

⚠️ **Correction, et elle porte sur ce paragraphe lui-même.** Le message du commit
`206b3a2b` annonce *« § 17 : 148 lignes »*. **C'était faux d'un facteur 2,3** —
le nombre avait été écrit de mémoire, pas compté. C'est exactement le *« nombre
plausible qui se recopie dans un rapport, puis dans une ROADMAP, puis dans une
décision »* que le banc voisin passe ses nuits à traquer.

📌 **Et la première correction est tombée dans le piège suivant** : j'ai réécrit
le compte mesuré (343) **dans le paragraphe qu'il compte**, ce qui l'a fait
passer à 350 dans le même geste. **Un document ne peut pas porter sa propre
taille.** Le chiffre est donc retiré, pas corrigé — et la règle qui reste est la
seule utile : **aucun compte non mesuré dans un message de commit, et aucun
compte auto-référent dans un document.**

⚠️ **Ce que cette lecture ne prouve pas** : ce banc mesure des **structures de
données** et du **texte de shader**. Il n'a ni GPU ni fenêtre, et il le déclare en
tête. Il ne peut donc rien dire des § 4 (palette), § 12.2 (dézoom), § 10 (fils),
§ 11.4 (survol / sélection) — **et son silence n'y est pas un avis.**

---

### 17.1 ✅ Ce que le code PORTE — la spécification dit vrai

| § | la règle | ce qui la porte |
|---|---|---|
| **12.1** | la recherche de nœud est **filtrée par le type de la prise d'origine** | ✅ **portée en entier**, par deux fonctions : `NkMatNoeudsPourPrise(reg, g, type, …)` et `NkMatNoeudsPourPriseDe(reg, g, cleNoeud, prise, …)`. Ce n'était pas une supposition de dessin : *« dans Blender, le menu de `Base Color` et celui de `Roughness` n'ont pas le même contenu »* est **la demande de Rodolf du 22/08**, et elle est implantée |
| **11.5** | les conversions sont **DIRIGÉES** — `réel → couleur` existe, `couleur → réel` n'existe pas | ✅ **mesurée dans les deux sens** (`types/conversion-dirigee`) et **jusque dans le menu** (`biblio/menu-asymetrique-couleur-reel`). Le code va plus loin que ma justification : *« luminance ? moyenne ? canal rouge ? trois réponses plausibles, donc aucune par défaut »* |
| **7.4** | le séparateur : `Couleur` et `Alpha` **se distinguent par le typage**, sans règle spéciale | ✅ **mesuré, pas supposé** : `Image Texture` figure dans le menu **couleur** par `color` et dans le menu **réel** par `alpha`. *« C'est le meilleur genre de solution — celle qui n'ajoute rien »* était juste |
| **7.3** | un **second** nœud de sortie est une **ERREUR**, pas un avertissement | ✅ `NkMatGraphError::MultipleOutput` (`graphe/deux-sorties-refusees`), et le retrait du second **rend** le graphe valide |
| **11.6** | **le graphe invalide doit rester dessinable** | ✅ **le point le mieux tenu du code.** `validation/cycle-par-le-fichier` exige explicitement les deux liens chargés : *« l'invalide doit être REPRÉSENTABLE **et** DÉTECTÉ »*. Six diagnostics de lien distincts existent (`Cycle`, `LinkDirection`, `LinkTypeMismatch`, `LinkUnknownNode`, `LinkSocketOutOfRange`, `LinkDuplicateTarget`) |
| **8.3** | 🔴 « **les liens sont conservés et redessinés** » — que j'avais marquée *la règle la plus importante du paragraphe* | ✅ **portée ET mesurée** : `groupe/coeur-accepte-type-runtime` vérifie qu'un nœud d'un type inconnu se relie **dans les deux sens**, garde ses prises, **traverse le fichier mot pour mot** et **prend son rang dans le tri topologique** |
| **7.6** | le groupe : définition + instances, interface déduite, récursion refusée | ✅ porté, et **au-delà** — voir § 17.4 |
| **3.3** | une entrée branchée **perd son champ de saisie** | ✅ compatible, **avec une précision que le code rend obligatoire** : le modèle **garde** le défaut sous le lien. Le graphe d'essai du regroupement pose exprès un défaut sur une prise **câblée** (`m1.b`), avec sa raison : *« il sert dès qu'on débranche »*. ⚠️ **Le dessin CACHE le champ, il ne l'efface pas.** Cette phrase manquait au § 3.3 |

---

### 17.2 ❌ Ce que le code CONTREDIT — huit points, et deux touchent une planche

> **C'est la partie qui justifiait de ne pas cocher ce trou.**

#### C1 · 🔴 « chaque arrêt du `ColorRamp` est une prise » — **FAUX**

`kColorRamp` a **exactement deux prises** : `fac` en entrée, `color` en sortie.
Les arrêts vivent dans **une propriété**, `NK_MPROP_STOPS`, charge utile de
**4 N réels** (position, r, v, b). Idem pour `Float Curve` : `kFloatCurve` =
`fac`, `value` en entrée, `value` en sortie — les points sont dans
`NK_MPROP_POINTS`, **2 réels par point**.

> **Un arrêt n'est PAS branchable.**

⚠️ **Ça périme une affirmation de ce document ET un dessin.** Le § 7.5 écrit
*« chaque arrêt est une prise (déjà dans `planche_01`) : sa position **et** sa
couleur sont pilotables »*, et le § 5.4 s'y adosse explicitement (*« Même choix
que le `ColorRamp` de `planche_01`, où chaque arrêt est une prise »*).
**`planche_01_noeuds.svg` dessine un nœud que ce modèle ne permet pas.**

📌 **Et la conséquence est plus grande que le dessin** : si les arrêts sont une
propriété, **la barre de dégradé est le SEUL moyen de les éditer**. Le § 7.5 la
décrivait comme un confort (*« l'éditeur EST le nœud »*) ; **c'est en réalité une
nécessité**, et son absence rendrait le nœud inutilisable.

#### C2 · le plafond des charges variables est **32**, pas « au-delà de 8 »

`NK_RAMP_ARRETS_MAX = 32` et `NK_CURVE_POINTS_MAX = 32`, et le refus **dit le
compte** : *« 33 demandées, plafond 32 »* (`colorramp/refus-nommes-et-distincts`,
`courbe/refus-nommes-et-distincts`).

Mon **8** reste défendable — c'est un seuil d'**affichage**, pas de modèle. Mais
le § 5.4 illustre le repli par *« … et 492 autres »* sur un tableau de 500, et
**500 est impossible ici**. ✅ **Ce qu'il faut écrire à la place** : le nœud à
charge variable **affiche son plafond** (`28 / 32 arrêts`), parce que le refus,
lui, le dit déjà.

#### C3 · 🔴 le nœud indisponible « **présent dans la bibliothèque** » — le code fait l'INVERSE

Le § 8.4 tranche : *« dans la bibliothèque : **présent**, en 50 % d'opacité, avec
sa raison en seconde ligne. **Présent, parce qu'un nœud absent se cherche
indéfiniment ; un nœud refusé se comprend une fois.** »*

**Mesuré** : `NkMatAddNode(gReg, g, NK_MN_OBJECT_INFO)` rend `NK_NODE_INVALID`,
le prototype **n'est pas dans le registre**, et **le menu interroge le même
registre** (`groupe/catalogue-materiau-ferme` le mesure pour les quatre types de
prise). Un nœud indisponible est donc **absent de TOUS les menus**.

⚠️ **Et la table existe pourtant** : `detail::kIndisponibles[]`, avec
`{cle, pourquoiPas}` et `kIndisponiblesCount`. **Mais elle est dans
`namespace detail`, et le seul accès public est
`NkMatPourquoiIndisponible(cle)` — une recherche PAR CLÉ, pas une énumération.**

> 📌 **Conclusion nette : la bibliothèque du § 8.4 n'est pas constructible avec
> l'API publique d'aujourd'hui.** Il manque exactement deux fonctions,
> `NkMatIndisponibleCount()` / `NkMatIndisponibleAt(i)`, du même dessin que
> `NkMatAttributCount()` / `NkMatAttributAt(i)` qui existent quinze lignes plus
> haut. **C'est le seul point de cet écart qui demande une ligne de code, et il
> est minuscule.** ⚠️ **Je ne l'écris pas : ce n'est pas mon dépôt.**

#### C4 · les **exemples** du § 8.4 n'existent pas, et la raison est d'une autre famille

Le § 8.4 nomme `Light Path`, `Raycast`, `Ambient Occlusion`, `Curves Info`,
`Particle Info`, et fait écrire dans le nœud la raison **`ce moteur rastérise`**.

**Aucun de ces cinq n'existe.** Le seul indisponible réel est `mat.info_objet`,
et sa raison est tout autre :

> *« aucune donnée PAR OBJET n'atteint le fragment : le shader engendré ne dispose
> que du bloc caméra et des paramètres exposés — ni matrice de modèle, ni index,
> ni couleur, ni graine d'objet. Le nœud attend un bloc uniforme par objet ;
> **il n'est pas refusé par principe**. »*

✅ **L'ÉTAT est bon — la spécification l'avait inventé et il existe.** ❌ **Mais
la raison ne s'écrit pas dans la spécification : elle vient du code**, elle est
longue, et elle porte une phrase que mon dessin doit savoir afficher — *« il n'est
pas refusé par principe »*, qui est exactement ce que le § 8.4 voulait dire par
*« un nœud refusé se comprend une fois »*. **La rangée de raison doit donc être
multiligne**, pas la ligne unique que le § 8.4 dessine.

#### C5 · l'indisponible n'est pas un état **local** : il rend le document **incompilable**

Le § 8.4 le pose comme un état de nœud (*« corps hachuré, en-tête à 50 %, prises
intactes et fils conservés »*) — ce qui reste vrai au dessin. **Mais
`rang4/object-info-indisponible-avec-sa-raison` mesure autre chose** : un graphe
qui le porte est **refusé en entier**, et `r.source` est **vide**.

> ⚠️ **Un nœud gris quelque part rend TOUT le matériau non compilable, et ma
> spécification ne le dit nulle part.** Il faut un signal **au niveau du graphe**
> — pas seulement du nœud —, sans quoi l'utilisateur voit un nœud grisé, croit
> avoir perdu une fonctionnalité locale, et ne comprend pas pourquoi rien ne rend.

#### C6 · mes trois niveaux de connaissance sont une **échelle** ; le code en fait **deux axes**

Le § 8.1 range A (déclaré) / B (prises sans types) / C (inconnu) sur une seule
ligne graduée. Le code a **deux mécanismes orthogonaux** :

| axe | code | cas |
|---|---|---|
| le **type de NŒUD** est inconnu | `NkMatGraphError::UnknownNodeType`, avec `&coupable` (l'identifiant) **et** `&quoi` (le nom du type) | `groupe/refus-du-type-inconnu-situe` |
| le **type d'une PRISE** est inconnu | `NkGraphIssue::SocketUnknownType`, **par prise**, avec son nom en détail | `validation/types-inconnus-par-fichier` |

❌ **Ils se combinent librement, et le cas dominant est le pire pour mon dessin :
un nœud de type inconnu entre avec des prises PARFAITEMENT TYPÉES** (le `sock` du
fichier porte un `typeId` que le registre connaît). **La maquette du § 8.3 dessine
toutes les prises en `?` gris : c'est faux dans ce cas-là**, et ça jette une
information qu'on a.

✅ **La règle correcte** : l'en-tête rayé dit *« je ne connais pas ce nœud »* ;
**chaque prise garde sa couleur si son type est connu**, et ne passe en `?` gris
que si `SocketUnknownType` la vise. **Deux signaux indépendants, deux causes
indépendantes.**

#### C7 · 🔴 il existe un **second puits**, et il n'est pas unique

`NK_MN_OUTPUT_VALUE` — libellé `Named Output`, `kOutputValue` = deux entrées
(`value` réel, `color` couleur). Ce nœud :

- **n'est pas unique** : on en pose autant qu'on veut (`sortie/plusieurs-et-graphes-sans-sortie` en monte deux) ;
- porte un **nom public éditable**, validé (`2 mots` est refusé : *« nom absent ou invalide »*), **et unique dans le graphe** (*« deux sorties portent le nom »*) ;
- porte un **étage obligatoire SANS DÉFAUT** — l'absence est refusée, parce que *« se replier sur (a) transformerait silencieusement une sortie voulue par pixel en constante calculée une seule fois »* ;
- **une seule de ses deux entrées doit être branchée** (`aucune source` et `DEUX sources` sont deux refus distincts) ;
- et **selon son étage, il change ce que le graphe accepte en AMONT**, à distance et par contagion.

❌ **Le § 7 n'a que « 3 · puits », décrit comme unique, en-tête orange, corps 20 %
plus large.** Appliquées à `Named Output`, ces trois exceptions sont fausses : un
graphe à cinq sorties nommées aurait cinq en-têtes orange et cinq nœuds
surdimensionnés, et **l'argument même de l'exception — *« il est unique par graphe
et il en est la fin »* — tombe.**

> ✅ **À écrire : une quinzième forme, `puits nommé`.** Côté droit vide comme le
> puits, mais **en-tête ordinaire**, largeur ordinaire, **et un champ de nom en
> première rangée**. L'orange et la largeur restent réservés au `Material Output`,
> qui, lui, est bien unique.

#### C8 · « le cadre n'a **aucun effet sur le graphe** — il décore » — faux dès qu'il déplace

Le tableau du § 7.6 oppose cadre et groupe par *« effet sur le graphe : **aucun**
— il décore »*. Mais le § 9.2 décide que **déplacer un cadre emporte ses
membres**, et la forme canonique du contrôle de regroupement (`DecritNoeud`)
porte **`x` et `y`** — l'aller-retour grouper/dégrouper les exige **identiques**.

> **Un cadre qui déplace ses membres modifie des champs sérialisés. Il n'est pas
> décoratif : il est le seul objet d'interface qui écrit dans le modèle sans
> qu'on ait touché un nœud.** À corriger dans le tableau — et c'est un argument
> de plus pour l'appartenance par **lien explicite** (§ 9.2), déjà décidée.

---

### 17.3 🔇 Ce dont le code NE DIT RIEN — et la preuve exécutable du § 16.5

#### La liste complète des champs du modèle, écrite dans un contrôle qui tourne

`FormeCanonique()` / `DecritNoeud()` du banc énumèrent **tout ce qui définit un
nœud** pour le critère d'acceptation de R9 :

```
type · libellé · x · y · prises (nom, type, sens, valeur par défaut) · propriétés
```

**Rien d'autre.** Ni largeur, ni repli, ni repli de section, ni plié/séparé, ni
bascule d'aperçu, ni commentaire, ni cadre, ni désactivé, ni tableau, ni
dictionnaire. Vérifié aussi par relevé sur les deux en-têtes : **zéro occurrence**
de ces notions comme champ (les seules occurrences des mots sont de la prose sur
autre chose — *« repli »* y signifie **valeur de secours**, *« cadre »* y désigne
le **cadre cotangent**).

> ✅ **C'est la confirmation exécutable du § 16.5, et elle vaut mieux qu'un
> relevé** : les onze notions sans foyer **n'en ont toujours pas**, et cette fois
> c'est un contrôle vert qui le dit.

📌 **Et il y a un point de raccrochage à nommer au chantier du graphe de
matériaux** : le jour où une notion d'interface entre dans `NkNode`,
**`DecritNoeud` devra être mis à jour, sinon le contrôle d'aller-retour devient
aveugle à sa perte** — un champ absent du descripteur est un champ dont la
disparition ne fait pas rougir le contrôle. C'est exactement la famille de piège
que ce banc documente sous *« il manquait la matière »*.

#### Les trois manques qui touchent le dessin de plein fouet

`NkMatNodeProto` est **`{ key, label, sockets, socketCount, parPixel }`**. Rien
de plus. Confronté au § 8.2, qui liste ce qu'une déclaration doit fournir :

| § 8.2 | dans le prototype | conséquence pour le dessin |
|---|---|---|
| 1 · nom affiché **et catégorie** | `key` + `label` — ❌ **aucune catégorie** | 🔴 **le manque le plus lourd.** Le § 2.1 fait de la **couleur d'en-tête = catégorie** la seule information qui survit au dézoom 25 % (§ 12.2), et c'est l'argument qui a fait **refuser** que le cadre teinte ses nœuds (§ 9.2, option A). **Cette couleur n'a aujourd'hui aucune source de vérité.** Tout le raisonnement du § 9.2 repose sur un champ qui n'existe pas |
| 2 · **s'il exécute** | ❌ absent | le filet orange / pétrole n'a pas de champ. Vrai **par construction** dans un graphe de matériau (aucun type d'exécution parmi `real, vector, color, shader, ramp, curve`), donc la § 6.4 tient — mais le § 8.2 exige que ce soit **une déclaration, pas une inférence**, et il n'y a rien à déclarer |
| 3 · prises : nom, type, sens | ✅ porté par `NkMatSocketDecl` | |
| 3 · **valeur par défaut** de l'entrée | ❌ pas dans le prototype — elle vit sur la **prise du nœud instancié** | ✅ **et le code a raison contre moi.** `variable/le-defaut-est-celui-de-la-prise` mesure que le défaut d'un paramètre exposé **EST** celui de la prise, *« jamais une seconde valeur rangée à côté »*, parce que *« deux sources pour une même chose divergent, et c'est alors l'éditeur qui montre l'une pendant que le moteur envoie l'autre »*. **Adopter la position du code, corriger le § 8.2** |
| 4 · **texte d'aide** | ❌ absent | ma règle *« sans aide, le `?` n'est pas dessiné du tout »* **tient** — et aujourd'hui elle vide l'en-tête de tous les nœuds |
| 5 · pictogramme, largeur préférée, sections de repli | ❌ absents | déclarés facultatifs, ils le restent |

#### L'espace colorimétrique — mon avertissement du § 6.2 est **inapplicable**

Le § 6.2 fait de l'espace colorimétrique *« le seul champ qui change
silencieusement le rendu »*, l'écrit **en toutes lettres** sur le nœud, et bâtit
sur lui **l'état « avertissement » tout entier** (§ 11.2 : *« il manque, et le
§ 6.2 le prouve »*).

❌ **`Image Texture` n'a qu'une propriété de fichier : `NK_MPROP_IMAGE`, le
chemin.** Pas d'espace colorimétrique, pas de taille, pas de format. **Ma « carte
d'identité » — `2048 × 2048 · RVBA8 · sRVB` — n'a de donnée que pour le chemin, et
mon déclencheur d'avertissement n'existe pas.**

⚠️ **Mais le code met à cette place exacte une propriété de la même famille, et
elle est le modèle à suivre** : `NK_MPROP_NORMAL_CONV` (`opengl` / `directx`),
avec trois propriétés que l'espace colorimétrique devrait copier mot pour mot —
c'est **une donnée de provenance du fichier**, elle se convertit **à l'import et
jamais dans le shader** (`normalmap/convention-ne-change-pas-le-shader` exige les
deux sources **identiques au caractère près**), et **un mot inconnu est refusé en
le nommant**.

> 📌 **Ce que j'en retiens contre moi** : le § 6.2 a inventé un champ et lui a
> accroché un état. La convention de normales prouve que le champ **aurait** sa
> place et **quelle forme** il prendrait — mais **l'état « avertissement » du
> § 11.2 n'a, aujourd'hui, aucun déclencheur réel.** Il ne faut pas le coder
> avant que la propriété existe.

#### Ce que le banc ne peut pas dire, et il le dit lui-même

Aperçu, vignette, bascule globale (§ 6.1, § 6.3, § 6.5) : **rien**, et c'est
normal — *« AUCUN GPU, AUCUNE FENÊTRE »* est la première phrase du fichier. ✅ **Un
banc qui déclare ce qu'il ne sait pas faire est exactement le dessin du témoin
d'instrument** que la Règle Zéro impose aux planches. Son silence n'est pas un
avis.

⚠️ **Une donnée qu'il porte quand même pour le § 6.3** : le nombre de textures
d'un graphe est **borné** (`renderer::NK_MATBIND_GRAPH_SLOT_COUNT`), et le refus
dit *« ce graphe demande N textures, le plafond est M »*. Ma ligne d'identité
proposée — *« `4 textures · 12 nœuds` »* — doit donc s'écrire **`4 / M
textures`** : le plafond est une information que l'auteur cherchera, et elle
existe déjà.

---

### 17.4 ✅ Ce que le code porte et que la spécification n'avait PAS — quatorze règles

> **C'est la moitié utile de l'exercice.** Chacune est une règle de dessin qui
> manquait, et plusieurs sont écrites **dans le code lui-même**, en français, par
> l'auteur du module.

| # | ce que le code impose | où c'est écrit |
|---|---|---|
| **N1** | 🔴 **la rangée SANS point de connexion.** `NkMatSocketDecl::constanteSeulement` : certaines entrées alimentent l'**état du pipeline** et ne peuvent pas varier par pixel. **Le code écrit lui-même la règle de dessin** : *« l'interface doit NE PAS AFFICHER le point de connexion, plutôt qu'ouvrir un menu vide : un menu vide laisse croire à une panne, une prise sans point dit “ce paramètre est une constante”, ce qui est la vérité »* | `NkMatSocketDecl`, `biblio/prise-constante-seulement`. ⚠️ **Aucun prototype ne l'emploie encore, et le banc le dit.** → **douzième micro-élément du § 2.4** |
| **N2** | **`parPixel` et sa CONTAGION** : une sortie « par matériau » refuse toute source par pixel **à distance** (mesuré à **deux nœuds d'écart**), et le refus **nomme le nœud coupable**. Le même `Mapping` est accepté ou refusé **selon son amont** | `sortie/refus-par-pixel-a-deux-noeuds-en-nommant`, `sortie/mappage-par-contagion-seulement`. → **aucun de mes onze états ne sait montrer une CHAÎNE fautive** ; il faudra surligner un chemin, pas un nœud |
| **N3** | 🔴 **prise CÂBLÉE *et* EXPOSÉE = refus nommé.** Et le code dit **pourquoi c'est une commande au dessin** : *« Chez Blender le problème ne se pose pas parce que brancher un lien FAIT DISPARAÎTRE le widget : l'interface rend l'état impossible. **Nous n'avons pas d'interface** — c'est donc la validation qui doit le rendre impossible. »* | `variable/prise-connectee-et-exposee-refusee`. → **un état de rangée « en conflit » absent de ma spécification**, et à montrer **au branchement**, pas à la compilation |
| **N4** | **nommer un groupe a QUATRE refus distincts** : `NomVide`, `SansPrise`, `DejaEnregistre`, et **`DejaStatique`** — un groupe ne peut pas **éclipser** un nœud du catalogue, *« un catalogue où le dernier inscrit gagne est un catalogue dont on ne peut plus prédire le contenu »* (Rodolf, 22/08) | `groupe/refus-d-eclipse-nomme`. → **§ 12.5 : « grouper en sous-graphe » n'est pas une entrée de menu simple, c'est un geste qui ouvre une saisie de nom REFUSABLE**, avec quatre messages |
| **N5** | la **récursion est refusée À L'INSERTION**, pas seulement à l'aplatissement — *« à ce moment-là l'utilisateur sait ce qu'il vient de faire ; à l'aplatissement, l'erreur sort loin de sa cause »* — **et les deux filets restent**, parce qu'un graphe peut arriver **par un fichier**. Mesuré jusqu'à **trois maillons** (`A→B→C`, puis `C→A` refusé) | `groupe/recursion-refusee-aux-deux-portes`. → le message se dessine **au dépôt de l'instance** |
| **N6** | 🔴 **le registre de prototypes est PAR DOCUMENT** (`NkMatRegistreProtos`, depuis le 23/08). Deux documents ouverts **ne voient pas** leurs groupes, **peuvent employer le même nom**, et fermer l'un n'abîme pas l'autre | `groupe/deux-documents-ne-voient-pas-leurs-groupes`. → **un groupe n'est PAS global.** La bibliothèque et le fil d'Ariane (§ 12.3) sont **par document**, et ma § 7.6 ne le dit nulle part |
| **N7** | **le pont sous-graphe → prototype INVERSE le sens des prises** : `graph.entree` porte des prises de **sortie** (il alimente l'intérieur) qui deviennent des **entrées** sur le prototype | `groupe/pont-interface-deduite-du-sous-graphe`. ✅ Ma § 7.6 dit *« les prises du groupe se voient DE L'INTÉRIEUR »* — **c'est vrai, et l'inversion en est la conséquence non écrite** |
| **N8** | un groupe **hérite** du `parPixel` de son contenu, **côté sûr en cas de doute** (un groupe imbriqué non résolu est déclaré par pixel), parce que *« parPixel faux à tort accepte une valeur qui change à chaque pixel en la faisant passer pour celle du matériau — faux, PLAUSIBLE, jamais signalé »* | `groupe/pont-par-pixel-conservateur` |
| **N9** | ⚠️ **piège d'affichage direct sur le § 12.1** : `NkMatNoeudsPourPrise` rend **le compte VRAI** même quand elle a écrit moins que `maxOut` (`if (out && n < maxOut) out[n] = p;` puis `++n`). Un panneau qui afficherait « N nœuds » et itérerait sur le tampon **annoncerait plus qu'il ne montre** | le banc note que le tampon *« l'a été [dépassé] à 17 »* et se dimensionne désormais sur le nombre de prototypes. → **à écrire dans le § 12.1** |
| **N10** | 🔴 **le menu rend des PROTOTYPES, pas des couples (prototype, prise).** `Image Texture` est **la même entrée** dans le menu couleur (par `color`) et dans le menu réel (par `alpha`) | `biblio/menu-asymetrique-couleur-reel`. → **le § 5.6 point 3 — *« le nœud choisi s'insère entre les deux et les deux liens se font »* — ne sait pas QUELLE prise brancher.** Manque direct, et il touche le pivot du § 5.6 |
| **N11** | **les types n'ont pas de libellé** : `TypeName()` rend la **clé** (`mat.couleur`, `mat.reel`). Seul le *prototype* a un `label` | → le § 12.1 veut le filtre **« en toutes lettres »** (`qui acceptent [1.0] tableau de réels`). Il n'y a que des clés : **écrire `qui acceptent mat.couleur` serait laid et technique.** Il manque un libellé par type |
| **N12** | ⚠️ **`t.ramp` et `t.curve` ne sont JAMAIS des types de prise** — ils servent à typer une **propriété** (`NkValueText(t.ramp, "image.png")`) | → **règle à ajouter au § 4 : la couleur de type ne se lit que sur les PRISES, jamais sur les propriétés.** Sinon un chemin d'image se peindrait de la couleur « rampe » |
| **N13** | **deux régimes de diagnostic, et ma § 11.1 suppose partout le second** : `NkMatValidate` rend **UN** coupable (`&coupable`, `&quoi`) et `NkMatCompileResult::error` est **une seule chaîne** ; `Validate(diags)` du cœur rend **une liste** | → *« la raison est écrite DANS le nœud, en pied »* (§ 11.1) n'est vrai que pour les diagnostics du cœur. **La couche matériau refuse au premier fautif** : l'éditeur ne peut pas peindre tous les nœuds en erreur d'un coup |
| **N14** | ⚠️ pour le **cycle**, le diagnostic ne dit **pas quels fils** — `NkGraphIssue::Cycle` rend un diagnostic et un détail | → ma § 11.6 veut *« les fils du cycle passent en `#E4443C`, tous ensemble »*. **Il faut retrouver la boucle soi-même**, coût que ma spécification ne chiffrait pas |

⚠️ **Et un piège d'API à écrire noir sur blanc avant le codage** :
**`Accepts(destination, source)`**, dans cet ordre. Le code le montre deux fois —
`g.Accepts(t.color, t.real)` signifie **réel → couleur**. Inversé, tout le § 11.5
(compatible / convertible / incompatible) se dessinerait **à l'envers**, et le
résultat serait parfaitement plausible.

📌 **Une nuance sur le § 11.5, mesurée** : le modèle n'expose **qu'un booléen**.
« Compatible » et « convertible » **ne se distinguent pas par une fonction** —
il faut les calculer : *compatible* = `dst == src`, *convertible* =
`dst != src && Accepts(dst, src)`. **C'est déductible, pas absent** — mais c'est à
l'éditeur de le faire, et ma § 11.5 laissait croire que le modèle rendait trois
états.

---

### 17.5 Trois points du § 13 que cette lecture ferme ou déplace

| # | question | ce que la mesure en dit |
|---|---|---|
| **1** | **largeur du nœud** : fixe, adaptative ou redimensionnable ? | ⚠️ **toujours ouverte, mais son coût est maintenant chiffré** : `DecritNoeud` porte `x` et `y` et **pas de largeur**, et l'aller-retour du regroupement exige l'identité. **Une largeur libre ne survivrait à rien aujourd'hui**, exactement comme ce document l'annonçait |
| **2** | **séparer une struct casse-t-il le lien existant ?** | ✅ **SANS OBJET côté matériau.** Le modèle n'a pas d'état « séparé » : il a **deux nœuds**, `Separate XYZ` et `Combine XYZ`. Séparer, c'est **poser un nœud**, ce qui ne casse aucun lien. 🔴 **À reposer seulement si le blueprint apporte un vrai séparateur d'affichage** — et alors ce sera une question neuve, pas celle-ci |
| **9.1bis 🟡** | *« le catalogue dit le puits unique ; un fichier importé peut en apporter plusieurs, dont un seul actif — je ne tranche pas si le modèle l'autorise »* | ✅ **FERMÉ.** Le modèle **représente** les deux (le fichier charge, le tri passe) et **les refuse** (`MultipleOutput`). **Il n'y a AUCUNE notion de puits actif.** Mon *« c'est le puits ACTIF qui porte un signal positif »* est sans objet — et le **quatrième gris** est évité, mais pour une autre raison que celle que j'écrivais : il n'y a rien à griser, il y a une erreur à montrer |

---

### 17.6 📌 Ce que cette lecture m'apprend sur ce document

**Quatre des huit contradictions ont la même forme, et il faut la nommer :
j'ai spécifié un dessin à partir d'un EXEMPLE que je n'avais pas vérifié.**

- les arrêts-prises du `ColorRamp` (C1) viennent de **ma propre planche 01** ;
- le tableau de 500 (C2) vient d'une phrase du catalogue, pas d'un modèle ;
- les cinq nœuds à tracé de rayons (C4) viennent du catalogue, et **aucun
  n'existe** ;
- l'espace colorimétrique (§ 17.3) est un champ que **j'ai inventé** pour
  justifier un état.

> ⚠️ **C'est la même famille que les pièges de la nuit du 22 au 23 : la
> connaissance existe, mais pas là où je l'ai cherchée.** Ici je ne l'ai pas
> cherchée du tout — j'ai lu le catalogue, qui décrit **une intention**, et pas le
> code, qui décrit **ce qui est**. Le catalogue est un bon document ; ce n'en est
> pas un de vérité.

✅ **La règle qui en sort, et elle vaut pour la suite** : **avant d'attacher un
état visuel à un champ, vérifier que le champ existe.** Un état sans déclencheur
(le § 11.2) et une planche sans modèle (la 01) coûtent tous les deux la même
chose : ils se codent, et ils ne peuvent pas marcher.
