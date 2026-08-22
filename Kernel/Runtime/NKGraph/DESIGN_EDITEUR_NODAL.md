# Design de l'éditeur nodal — référence principale et secondaires

> Écrit le 2026-08-22 depuis les images fournies par Rodolf. Il **décrit** la
> référence principale pour qu'elle devienne un contrat, dit ce que chaque
> secondaire apporte, et **nomme ce qu'aucune ne montre**.
>
> Compagnon de `CATALOGUE_NOEUDS.md`, qui liste les nœuds et les états à couvrir.

---

## 0. ⚠️ La méthode de Rodolf est la bonne, et voici pourquoi

> *« une référence visuelle principale et des références secondaires — le style
> global doit être exactement comme la principale avant d'y ajouter des éléments
> supplémentaires »*

**C'est la meilleure façon de spécifier un visuel, et elle vaut mieux qu'une
liste d'adjectifs.** Trois raisons :

1. **Elle donne un critère objectif.** « Est-ce que ça ressemble à la référence ? »
   se tranche en regardant. « Est-ce que c'est élégant ? » ne se tranche pas.
2. **Elle règle d'un coup mille micro-décisions** — rayon des coins, épaisseur
   des filets, contraste du texte secondaire — qu'aucune spécification écrite ne
   couvrirait sans devenir illisible.
3. **Elle sépare le style de la fonction.** Le style vient de la principale ; les
   secondaires n'apportent que des **mécanismes** que la principale ne montre pas.

⚠️ **Sa condition compte autant que la méthode** : *« exactement comme la
principale AVANT d'y ajouter »*. Mélanger deux styles au premier jet donne un
éditeur qui ne ressemble à rien. **On copie, on vérifie, puis on ajoute.**

---

## 1. La référence PRINCIPALE — description

`Screenshot 2026-07-17 174628.png`. Un éditeur de flux d'automatisation.
**C'est le style à reproduire.**

### Le fond

Très sombre, presque noir, avec une **grille de points** discrète — points
petits, faible contraste, espacement régulier. Pas de lignes : des points.
Le fond ne cherche pas à se faire voir.

### Le nœud

Un **rectangle à coins arrondis**, corps sombre légèrement plus clair que le
fond, sans ombre portée marquée.

**L'en-tête est une bande pleine, et sa couleur porte du sens :**

| nœud | en-tête | ce que ça dit |
|---|---|---|
| `Send Request` | **orange ambré**, avec une icône | c'est une **action** — elle fait quelque chose |
| `Evaluate`, `If` | **gris neutre**, avec un `?` à droite | c'est un **calcul** — ça ne fait rien, ça répond |

> ⚠️ **La référence répond déjà à la question des deux familles de connecteurs, et
> pas là où je l'attendais : elle la porte dans la COULEUR DE L'EN-TÊTE.** Un
> nœud qui agit est ambré, un nœud qui calcule est gris. La lecture en diagonale
> se fait sur les en-têtes avant même de suivre les fils.

Un **`?`** en haut à droite ouvre l'aide du nœud. Discret, toujours au même endroit.

### Les rangées

Étiquette à gauche, contrôle à droite. Les champs de saisie ont un fond **en
creux**, plus sombre que le corps. Le texte secondaire est très atténué
(`Add environment`, `Enter path…`).

Deux mécanismes de repli visibles : **`1 Variables ∧`** et **`Snippets ∨`** —
des sections qui se replient **à l'intérieur** du nœud, pas seulement le nœud
entier. ✅ C'est exactement ce que demande le `Principled` et ses vingt entrées.

Une rangée **`+ Click to add data blocks`** sert d'invite d'ajout — elle a l'air
d'un champ vide et se distingue par sa couleur atténuée.

### Les prises et les types

**Les types se lisent par une pastille, pas par une couleur seule.** On voit
`123` à côté de `Result`, de `Data`, de `TRUE` et de `FALSE` — **une petite
étiquette qui dit le type**. Les sorties `Success ()` et `Failure ()` portent
`()`, un glyphe différent.

> ✅ **C'est la réponse à ma question 1 du catalogue** : forme **et** libellé,
> pas douze couleurs à distinguer. Un badge se lit, se cherche, et fonctionne
> pour qui distingue mal les couleurs.

`TRUE` et `FALSE` sont des **pastilles nommées** sur le bord droit, empilées.
Le nom est dans la pastille, pas à côté.

### Les fils

**Courbes douces**, fines, qui sortent horizontalement du bord avant de
s'infléchir. **Leur couleur varie** : ambre pour le flux principal, bleu et rose
pour les données, blanc pour d'autres. Ils passent **derrière** les nœuds.

### Les puces flottantes

`body.tests`, `body.failures`, `body.runtime` : de **petits nœuds compacts**, une
icône violette arrondie et un libellé, hauteur d'une seule rangée. Ils servent de
**source de valeur** posée près de son consommateur.

> ✅ **C'est notre nœud « relais » et notre nœud « source »**, résolus par la même
> forme. Un catalogue de huit formes se ramène ici à deux : le **bloc** et la
> **puce**.

---

## 2. Ce que les SECONDAIRES apportent

Elles ne changent pas le style. Elles montrent des **mécanismes** absents de la
principale.

### `106968910-…png` — prises verticales et fils en équerre

Les prises sont sur les bords **haut et bas**, pas seulement gauche et droite.
Les fils sont **orthogonaux à coins arrondis** plutôt que des courbes.

⚠️ **À trancher, et ce n'est pas qu'un goût** : un graphe **vertical** se lit
comme un organigramme et convient au flot d'exécution ; un graphe **horizontal**
convient au flot de données, qui va d'une source à un puits. **La principale est
horizontale.** Si les blueprints devaient être verticaux, ce serait deux
dispositions dans un même canevas — je le déconseille, mais c'est ton appel.

### Les autres

À décrire au fur et à mesure de ce que tu veux en tirer. **Dis-moi pour chacune
ce que tu veux qu'elle apporte** — sinon je vais deviner, et deviner un style
est exactement ce qu'on veut éviter.

---

## 3. 🔴 Ce qu'AUCUNE référence ne montre — et qu'il faudra décider

C'est la partie la plus utile de ce document.

| manque | pourquoi ça compte |
|---|---|
| **un nœud à vingt entrées** | la principale montre des nœuds à 2-4 rangées. Le `Principled` en a une vingtaine. **Le repli par sections y répond en partie — mais à quoi ressemble-t-il déplié ?** |
| **un nœud en ERREUR** | rien dans les références. Il doit se voir **sans survol**, dans un graphe de cent nœuds |
| **un fil en cours de tirage** | avec les prises compatibles mises en évidence et les incompatibles éteintes. C'est le retour visuel le plus utile de l'éditeur, et aucune image ne le montre |
| **le dézoom** | à partir de quel niveau les prises disparaissent-elles ? Puis les noms ? |
| **un nœud à charge variable** | `ColorRamp` et sa barre de dégradé, `Float Curve` et sa courbe. La principale a des champs de code, pas d'éditeurs graphiques |
| **une prise de TABLEAU** | rien ne la distingue d'une prise simple dans les références |
| **un cadre de groupe** | le rectangle coloré derrière les nœuds |
| **la sélection multiple** | et ce qui se passe quand on déplace un groupe |

⚠️ **Ces huit cases ne s'inventent pas au codage.** Chacune décidée en passant
donnera un éditeur incohérent — c'est exactement ce que ta méthode de référence
cherche à éviter, et ce serait dommage de la perdre sur les cas difficiles.

---

## 4. Ce que je propose comme suite

1. **Tu confirmes** que la principale est bien celle-là, et tu dis pour chaque
   secondaire ce que tu veux qu'elle apporte.
2. **Je produis une planche** qui applique le style de la principale à **nos**
   nœuds — un `Principled`, un `ColorRamp`, un `Math`, une puce, un nœud
   d'exécution — plus les huit cas manquants ci-dessus.
3. **Tu corriges cette planche**, et elle devient la référence de notre éditeur.

⚠️ **L'étape 2 est du dessin, pas de la spécification** — et je ne l'entreprends
qu'après ta confirmation, parce que dessiner avant que la principale soit
confirmée serait exactement le travail à refaire qu'on cherche à éviter.

**La charte Rihen** (pétrole `#0A555F`, orange `#F79A28`, blanc) devra se poser
là-dessus. ✅ **Bonne nouvelle : l'orange ambré des en-têtes d'action de la
principale est très proche du `#F79A28` de Rihen.** Le pétrole pourrait porter
les nœuds de calcul, là où la référence met du gris.
