# 6bis. Jouer un coup à la souris

> Tous les autres chapitres sont écrits du point de vue de **celui qui écrit un
> moteur**. Celui-ci est écrit du point de vue de **celui qui joue**. C'est le
> point de vue qui manquait, et ce chapitre existe à cause d'une phrase :
>
> *« Le labo compile bien mon code du palier 1 et un menu déroulant sur la
> gestion des ressources de fusion est apparu dans les règles, mais **il n'y a
> pas de bouton pour jouer la fusion**. Et je pense que c'est la même chose pour
> les pouvoirs et les artefacts. »*
>
> Cette phrase était juste sur les trois points. Voici, pour chacun, ce qu'on
> voit à l'écran et où l'on clique.

**Ce qui a été vérifié, et comment.** Ce qui est écrit ici sur *dupliquer* a été
observé à l'écran : atelier lancé, un totem cliqué, capture d'écran — le contour
de sélection et les anneaux verts étiquetés `D` y sont. La fusion, les pouvoirs
et l'ambiguïté ont été **mesurés hors interface** par le banc
`Applications/ConquerorLab/tests/ambiguite.ps1`, qui appelle directement la
fonction décidant quel coup part au clic : **13 vérifications, 0 échec**. Ce qui
n'a été ni vu ni mesuré est dit comme tel, en clair, plutôt que déduit du
contrat.

---

## 6bis.1 La règle unique : deux clics

Il n'y a **qu'un seul geste** dans l'atelier, quel que soit le genre du coup :

1. **un clic sur une case de départ** — elle prend un contour orange, et toutes
   les cases où ce totem peut agir s'allument ;
2. **un clic sur une case allumée** — le coup part.

Il n'existe **aucun bouton** « Fusionner », « Lancer un pouvoir » ou « Ramasser
l'artefact », et il n'en manque pas : ce sont les **cases** qui portent
l'action. Ce qui change d'un genre de coup à l'autre, ce n'est pas le geste,
c'est **quelle case est le départ** et **quelle case est l'arrivée**.

Un clic ailleurs annule la sélection ; un clic sur un autre de vos totems y
déplace la sélection. **On ne peut pas jouer un coup illégal** : une case qui
n'est pas allumée ne répond pas. Et si rien ne s'allume, c'est que ce totem n'a
aucun coup — pas que l'atelier vous ignore.

## 6bis.2 Ce que les marques veulent dire

| Marque | Sens |
|---|---|
| contour orange épais | la case **sélectionnée** |
| anneau vert + `D` | destination d'un **DUPLIQUER** |
| anneau orange plein + `F3` | la **case du résultat** d'une **FUSION** à 3 cases |
| anneau orange fin, relié par un trait | une case que la fusion va **consommer** |
| anneau vert + `P0`, `P1` | cible d'un **POUVOIR**, et **lequel** |
| `×2` au-dessus d'une case | **plusieurs coups différents** visent cette case |
| anneaux rouges | les ennemis que le coup **survolé** retournerait |

Ces marques restent affichées **tant que la sélection dure**. C'est un point de
conception, pas un détail : ce qui n'apparaît qu'au **survol** n'existe pas pour
qui ne sait pas où passer la souris. Avant le 23 août 2026, la case du résultat
d'une fusion ne se distinguait d'aucune autre destination — d'où l'impression
qu'il manquait un bouton.

## 6bis.3 Jouer une FUSION

Une fusion consomme *plusieurs* cases et n'en produit qu'une. Elle n'a donc pas
de case de départ unique, et c'est exactement ce qui déroute.

1. **Cliquez n'importe laquelle des cases que la fusion consomme.** Elles
   fonctionnent toutes — inutile de chercher « la bonne ». Vous ne les
   sélectionnez **pas** une par une : le moteur a déjà énuméré les groupes
   possibles dans `CoupsPossibles`, l'interface ne fait que retrouver celui que
   vous désignez.
2. **Regardez les traits orange.** Ils partent de chaque case consommée et
   convergent vers **la case du résultat**, qui porte un anneau plein plus épais
   et l'étiquette `F` suivie du nombre de cases (`F2`, `F3`…).
3. **Cliquez cette case du résultat.** Le coup part : le journal affiche
   `FUSIONNER`, et le totem réapparaît un niveau plus haut.

**Si vous vous trompez de case**, rien ne part. Un clic sur une case non allumée
annule simplement la sélection — recommencez. Aucun coup ne peut être joué « par
accident » à la place d'un autre.

**Si rien ne s'allume du tout** quand vous cliquez une case que votre moteur
devrait pouvoir fusionner, ce n'est ni votre faute ni une fatalité : lisez
l'encadré.

> ### Le piège — « il n'y a pas de bouton pour jouer la fusion »
>
> Dans les ateliers **antérieurs au 23 août 2026**, la sélection filtrait sur le
> champ `from` du coup. Or une fusion laisse `from` **à zéro** et met ses cases
> dans `fuseCells`. Cliquer une case à consommer ne la sélectionnait donc même
> pas : aucun contour, aucun anneau, **rien** — d'où l'impression, parfaitement
> légitime, qu'il manquait un bouton.
>
> Pire, et c'est le symptôme qui rend fou : `from` mis à zéro **vaut** la
> coordonnée `(0,0)`. Le coin du plateau passait donc pour la source de *toutes*
> les fusions. Cliquer ce coin allumait des destinations sans rapport ; cliquer
> la vraie case n'allumait rien.
>
> Si votre atelier se comporte ainsi, **votre version est périmée** : reprenez le
> kit. Le correctif et sa mesure sont dans `tests/ambiguite.ps1`.

## 6bis.4 Jouer un POUVOIR, et en choisir un

Un pouvoir est désigné par **trois** choses : le lanceur (`from`), la cible
(`to`), et **lequel** (`powerId`). Le geste, lui, ne change pas : **clic sur le
lanceur, clic sur la cible**. Les cibles possibles portent un anneau vert marqué
`P` suivi de l'identifiant — `P0`, `P1` — pour que deux pouvoirs voisins ne se
confondent pas.

Mais deux clics ne désignent que deux cases, et un pouvoir en demande trois.
**Que se passe-t-il quand deux pouvoirs différents du même totem visent la même
cible ?**

- Avant le clic, la case affiche **`×2`** : elle vous prévient que le clic est
  ambigu.
- Au clic, l'atelier **ne choisit pas à votre place**. Il ouvre un menu
  **« Quel coup ? »** qui nomme les candidats — `POUVOIR n0`, `POUVOIR n1`,
  `FUSIONNER 2 cases` — et indique entre parenthèses **combien de totems chacun
  retourne**, ce qui suffit le plus souvent à trancher.
- Tant que ce menu est ouvert, **le plateau ne répond plus** : un clic ne peut
  pas le traverser et jouer un coup que vous croyiez être en train de choisir.
  *Annuler* referme le menu sans rien jouer.

> ### Ce qui était cassé, et qui est maintenant mesuré
>
> Avant le 23 août 2026, l'atelier jouait **« le premier coup qui correspond »**.
> Deux pouvoirs sur la même cible ne différant que par leur `powerId`, **le
> second était inatteignable à la souris** : il figurait dans les coups légaux,
> l'IA pouvait le jouer, l'humain jamais — et aucun message ne le disait. Même
> histoire pour deux fusions dont les groupes diffèrent mais dont la case du
> résultat est la même.
>
> Le banc `tests/ambiguite.ps1` mesure exactement ce cas : l'ancienne règle
> renvoie toujours l'indice 0, la nouvelle renvoie **2 candidats** et ouvre le
> menu. Un `DUPLIQUER` ordinaire, lui, reste à **un seul clic** — faire
> apparaître un menu là où il n'y a rien à choisir serait une régression, et
> c'est vérifié aussi.

**Côté auteur de moteur :** deux pouvoirs générés avec le **même** `powerId`
sont deux coups identiques octet pour octet ; l'atelier n'en montrera qu'un, et
ce n'est pas un défaut de l'atelier. Numérotez vos pouvoirs. Le raccourci
`Pouvoir(joueur, de, vers, idPouvoir)` de `ConquerorRegleFacile.h` construit le
coup avec la mise à zéro qu'exige le contrat.

## 6bis.5 Les ARTEFACTS : ce qui existe vraiment

**Il n'y a pas de coup « artefact », et il n'y a donc rien à cliquer.** Ce n'est
pas un manque de l'interface : le genre de coup n'existe pas dans le contrat.
`NkcMoveKind` vaut exactement `None`, `Duplicate`, `Fuse`, `Power` ou `Pass` —
il n'y a pas de sixième valeur.

Ce qui existe à la place :

- `NkcCellView::artefact` — un **champ de case**, un identifiant, `-1` quand il
  n'y a pas d'artefact. Il est **sérialisé**, donc un rejeu reste valide ;
- les événements `ArtefactPlaced` et `ArtefactExpired`, que votre moteur émet
  quand un artefact apparaît et disparaît ;
- `NkcCellView::powerUsed`, pour un pouvoir attaché à un totem et consommable
  une seule fois.

Un artefact **se ramasse donc en jouant un coup ordinaire** vers la case qui le
porte, ou **se déclenche par un POUVOIR** dont vous choisissez l'identifiant. Ce
choix est délibéré : ajouter un genre de coup casserait l'ABI de tous les
modules déjà compilés, alors qu'un champ de case n'oblige personne à recompiler.

⚠️ **Deux limites, dites franchement.** Aucun moteur livré ne pose d'artefact
aujourd'hui — c'est le palier 2 : `ConquerorRulesV2.cpp` remet le champ à `-1` et
s'arrête là. Et **rien ne dessine encore les artefacts sur le plateau** : si
votre moteur en pose un, le champ vivra dans l'état et dans le rejeu, mais vous
ne le **verrez** pas. Ne cherchez pas le bouton, il n'a jamais été construit.
Mesurez vos artefacts **en campagne IA contre IA**, et écrivez dans votre fiche
que vous n'avez pas pu les vérifier à l'œil : c'est une réponse recevable,
l'invention ne l'est pas.

## 6bis.6 Où lire la suite

- Écrire un moteur qui **génère** des fusions : chapitre *Les règles en trois
  fonctions* (`02b`), section « Fusionner », et l'exemple complet
  `exemples/rules/RegleFusion.cpp`.
- Le contrat nu, champ par champ (`fuseCells`, `powerId`, `artefact`) : chapitre
  *Écrire des règles* (`02`).
- Lire le plateau, les panneaux, le journal et la campagne : chapitre
  *Se servir de l'atelier* (`06`).

## Exercice

Reprenez `RegleFusion.cpp`, ajoutez **deux** pouvoirs qui visent la même case, et
vérifiez à l'écran que le menu « Quel coup ? » propose bien les deux et que **les
deux partent**. Si l'un des deux ne part jamais, regardez d'abord vos `powerId` :
deux coups qui ne diffèrent par rien sont un seul coup.
