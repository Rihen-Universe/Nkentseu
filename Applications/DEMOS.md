# Les démos — ce qu'on peut LANCER et VOIR

> **Règle permanente (Rodolf, 25/09/2026)**
> « en branchant, si tu peux, crée des démos que je peux tester, après je serais content. »
>
> **Aucun câblage n'est déclaré fini tant que Rodolf ne peut pas le lancer.** Un banc n'éprouve que
> son maillon ; une démo qu'un humain lance éprouve **la chaîne entière**.

**Ce qu'une démo doit être :**

1. un exécutable qui s'ouvre et **montre** — sans variable d'environnement, sans argument
   obligatoire, sans fichier à préparer ;
2. **nommée par ce qu'elle montre**, jamais par un numéro (*un indice n'est pas un nom* — `--demo=2`
   s'est déjà décalé d'un cran à chaque fusion, et deux bancs ont visé le même indice le 07/09) ;
3. inscrite ici : **le nom, ce qu'on doit voir, et ce qui prouverait que c'est cassé** ;
4. elle montre le **câblage**, pas le module ;
5. ⚠️ **jamais poussée sur GitHub.**

---

## Facial — « la face bouge quand on tire les curseurs »

**Ce que ça montre, et c'est un câblage, pas un module :** trois coefficients d'*Action Unit* → le
calcul (`NKAnima/Face/NkFaceController`) → des poids de blendshapes → la déformation d'un maillage.
Le calcul vivait dans `Applications/PV3DE/` jusqu'au 25/09 : une **feuille** du graphe de
dépendances, donc personne d'autre ne pouvait en dépendre. Il est descendu dans **NKAnima**, et
cette démo le prouve depuis une application **qui n'est pas PV3DE**, sans copier une ligne de PV3DE.

### Comment la lancer

1. lancer `NK3DModeler` ;
2. **Ajouter** un objet (un cube suffit), puis **TAB** pour entrer en Édition ;
3. panneau **Propriétés** (à droite), faire défiler jusqu'au groupe **« Facial (éprouvette) »**,
   le déplier ;
4. cliquer **« Activer l'éprouvette »** ;
5. tirer les trois curseurs : **AU1 sourcil**, **AU26 mâchoire**, **AU12 sourire**.

### Ce qu'on doit voir

- la forme **se déforme** pendant qu'on tire — le haut se soulève, le bas descend, le milieu
  s'élargit ;
- la ligne **« déplacement max »** monte avec les curseurs ;
- en cliquant **« Liaison BRANCHÉE »**, le bouton devient **rouge « Liaison COUPÉE »** : la forme se
  **fige**, et **les curseurs continuent de bouger les poids**. C'est le cœur de la démo — elle
  montre à l'œil que la déformation vient de ce fil-là et de nulle part ailleurs.

### Ce qui prouverait que c'est cassé

| symptôme | ce que ça veut dire |
|---|---|
| on tire les curseurs et **rien ne bouge** | le fil est coupé quelque part entre le calcul et la déformation |
| on coupe la liaison et **la forme bouge encore** | ⚠️ **le plus grave** : un **autre** chemin déforme le maillage, et la liaison mesurée n'est pas celle qu'on croit — *une garde verte grâce au défaut* |
| « déplacement max » reste à **0** alors que la forme bouge | l'instrument ment, pas le produit |
| « déplacement max » **monte** alors que rien ne bouge à l'écran | la déformation n'est pas poussée au GPU (`UpdateVertices` non atteint, ou maillage non 1:1) |

### ⚠️ Ce que cette démo n'est PAS

**Ce n'est pas une tête.** Il n'existe **aucun maillage à blendshapes** dans le dépôt — les quatre
`.glb` et tous les `.gltf` de `Resources/` ont été vérifiés : **zéro morph target**. Les trois cibles
sont **calculées** sur le maillage ouvert, quel qu'il soit, à partir de la hauteur de ses sommets.
C'est du **matériel d'épreuve**, et le panneau le dit à l'écran.

> Le dépôt a déjà payé *une sonde qui se fait passer pour le produit* : Rodolf a photographié la
> fenêtre d'un agent en croyant voir son application. Ici **rien n'est écrit sur le disque** — il n'y
> a aucun fichier qu'on puisse prendre pour un actif.

**CONDITION DE RETRAIT :** le jour où une vraie tête à blendshapes entre dans les ressources,
l'éprouvette part et la démo charge ses cibles au lieu de les calculer. *Un contournement dit quand
le supprimer.*

**Question ouverte pour Rodolf :** importer une vraie tête à blendshapes — **laquelle, et sous quelle
licence**. C'est un choix d'actif et de droits, pas un choix technique. L'éprouvette ne le remplace
pas : elle permet seulement de ne pas l'attendre.

### Pour les bancs (sans souris)

`NK_FACE_DEMO="0.8:0.8:0.8"` arme l'éprouvette et pose les trois coefficients ; `NK_FACE_COUPE=1`
coupe la liaison. Les deux passent par **les mêmes façades que les boutons du panneau** — une sonde
qui poserait l'état directement mesurerait un chemin que Rodolf n'emprunte jamais.

Mesure de référence (cube subdivisé, 486 sommets, coefficients à 0,8) :

```
liaison=branchee : dmax=0.24   nonNuls=486/486
liaison=COUPEE   : dmax=0      nonNuls=0
```
