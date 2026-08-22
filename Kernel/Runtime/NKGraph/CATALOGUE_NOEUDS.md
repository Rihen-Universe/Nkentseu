# Catalogue des nœuds — matériaux **et** blueprints

> Écrit le 2026-08-22 à la demande de Rodolf, pour servir de base au **dessin**
> de l'éditeur nodal. Il liste ce qui existe, ce qui reste, et surtout **ce que
> le dessin doit pouvoir montrer**.
>
> ⚠️ Il ne décide d'aucun style. La charte Rihen (pétrole `#0A555F`, orange
> `#F79A28`, blanc) est la seule contrainte connue ; **le reste est à dessiner**,
> et ce document sera repris pour coller au thème une fois le dessin fait.

---

## 1. ⚠️ Tu ne te trompes pas : il y a DEUX familles de connecteurs

C'est la distinction la plus structurante du document, et elle commande le
dessin.

| | **exécution** (instruction) | **valeur** (donnée) |
|---|---|---|
| ce qu'elle dit | **QUAND** — « fais ceci, puis cela » | **QUOI** — « cette valeur vaut ça » |
| ce qu'elle ordonne | le **temps** | rien : l'ordre se **déduit** des dépendances |
| arité en **sortie** | **un seul** lien — une instruction n'a qu'une suite | **plusieurs** — une valeur peut alimenter dix nœuds |
| arité en **entrée** | **plusieurs** — dix chemins peuvent mener ici | **un seul** — une entrée a une source, ou sa valeur par défaut |

> ⚠️ **Les arités sont INVERSÉES entre les deux familles.** Un fil d'exécution
> se divise en entrant, un fil de valeur se divise en sortant. Le dessin doit
> rendre cette différence évidente **sans lire les étiquettes** — c'est la
> première erreur que fera tout utilisateur venant de l'autre monde.

**Qui utilise quoi :**

| domaine | exécution | valeur |
|---|:---:|:---:|
| **matériaux** | ❌ jamais | ✅ uniquement |
| **blueprints / script** | ✅ | ✅ |
| VFX, animation, modélisation procédurale | selon le domaine | ✅ |

**Pourquoi les matériaux n'ont pas d'exécution** : un shader calcule une couleur
en chaque point, sans notion de « avant » et « après ». L'ordre d'évaluation se
déduit entièrement de qui dépend de qui. **Ajouter des fils d'exécution à un
graphe de matériau serait une faute de conception**, pas une fonctionnalité
manquante.

---

## 2. Les types de valeur — ce sont eux qui donnent le code couleur

Le type d'une prise décide de ce qu'on peut y brancher. Le menu se **calcule**
depuis le registre : brancher une couleur sur un réel est refusé si la
conversion n'est pas déclarée.

| type | contenu | remarque pour le dessin |
|---|---|---|
| **exécution** | rien — c'est un ordre | ⚠️ **doit se distinguer au premier coup d'œil** de tous les autres |
| **réel** | un nombre | le plus fréquent |
| **entier** | un nombre entier | distinct du réel : un indice n'est pas une mesure |
| **booléen** | vrai / faux | |
| **vecteur 2** | deux réels | coordonnées de texture |
| **vecteur 3** | trois réels | position, normale, direction |
| **vecteur 4** | quatre réels | |
| **couleur** | trois ou quatre canaux | ⚠️ **différente d'un vecteur 3**, même si le contenu se ressemble — une couleur a un espace colorimétrique, pas un vecteur |
| **chaîne** | du texte | |
| **shader** | une description de surface | ⚠️ **matériaux uniquement** |
| **objet** | une référence à une entité | ⚠️ **blueprints uniquement** |
| **quelconque** | générique, résolu au branchement | rare, mais nécessaire pour les nœuds polymorphes |

⚠️ **Une conversion peut être déclarée dans un sens et pas dans l'autre.**
Réel → couleur existe (le gris), couleur → réel n'existe pas (quelle
composante ?). **Le dessin doit pouvoir montrer une prise compatible et une
prise incompatible pendant qu'on tire un fil** — c'est le retour visuel le plus
utile de tout l'éditeur.

---

## 3. Les nœuds de MATÉRIAU

### 3.1 Livrés — 26 nœuds

| famille | nœuds |
|---|---|
| **sortie** | Material Output |
| **surfaces (BSDF)** | Principled · Diffuse · Emission · Mix Shader |
| **sources** | Value · RGB |
| **couleur** | Mix Color · ColorRamp |
| **texture** | Image Texture · Noise · Voronoi · Wave · Brick · Gradient · Checker |
| **vecteur** | Mapping · Texture Coordinate · Normal Map · Bump |
| **outillage** | Math · Vector Math · Map Range · Clamp · Separate XYZ · Combine XYZ |

### 3.2 À venir

**Surfaces restantes** : Glossy · Transparent · Refraction · Subsurface ·
Volume · Add Shader.
**Entrées contextuelles** : Fresnel · Layer Weight · Geometry · Object Info ·
Attribute · UV Map.
**Outillage** : Float Curve · Separate/Combine Color · RGB to BW.

### 3.3 🔴 Le rang INTERDIT — et il doit se voir dans le dessin

`Light Path` · `Raycast` · `Ambient Occlusion` · `Curves Info` ·
`Particle Info`.

**Ils supposent un tracé de rayons.** Notre moteur rastérise : ils ne peuvent
pas répondre honnêtement. S'ils apparaissent un jour, ce sera pour **refuser en
se nommant**, jamais pour rendre une valeur plausible.

⚠️ **Conséquence pour le dessin** : il faut un état visuel « **nœud
indisponible sur ce moteur** » — présent dans la bibliothèque, explicitement
refusé sur le canevas, avec sa raison lisible.

---

## 4. Les nœuds de BLUEPRINT

C'est ici que les fils d'exécution apparaissent. Rien n'est encore construit —
cette liste est la cible.

| famille | nœuds | exéc. |
|---|---|:---:|
| **événements** | Au démarrage · À chaque image · À la collision · Sur entrée clavier/souris · Sur message | ✅ **sortie seule** — un événement n'a pas d'entrée d'exécution |
| **flot** | Si / Sinon · Aiguillage (switch) · Boucle Pour · Boucle Tant que · Séquence · Portail (aller à) | ✅ |
| **variables** | Lire · Écrire · Incrémenter | Écrire : ✅ · Lire : ❌ |
| **fonctions** | Appeler · Retourner · Entrée de fonction · Sortie de fonction | ✅ |
| **objets** | Trouver · Créer · Détruire · Lire propriété · Écrire propriété | selon |
| **temps** | Attendre · Chronomètre · Retarder | ✅ |
| **maths / logique** | les mêmes qu'en matériau — Math, Vector Math, comparaisons, et/ou/non | ❌ **valeur seule** |
| **débogage** | Afficher · Point d'arrêt · Assertion | ✅ |

⚠️ **Deux règles qui sortent de ce tableau et qui comptent pour le dessin :**

1. **Un même nœud peut exister dans les deux mondes** — `Math` est identique en
   matériau et en blueprint. **Il ne doit pas être dessiné deux fois** ni avoir
   deux apparences.
2. **Un nœud qui porte de l'exécution a une silhouette différente** d'un nœud
   purement calculatoire. C'est ce qui permet de lire un graphe blueprint en
   diagonale : la ligne de vie traverse le graphe, les calculs pendent autour.

---

## 5. Les nœuds INERTES — ils ne calculent rien et ils sont indispensables

| | rôle |
|---|---|
| **commentaire** | une note posée sur le canevas |
| **cadre** | un rectangle qui groupe et se déplace avec son contenu |
| **relais** (reroute) | un point de passage pour ranger un fil, sans rien changer |

⚠️ **Ils doivent survivre à l'enregistrement et à la relecture** au même titre
que les autres. Un fichier qui perd les commentaires perd le travail
d'organisation, qui est souvent plus long que le câblage.

---

## 6. Ce que le DESSIN doit pouvoir montrer

C'est la liste qui te servira le plus. Chaque ligne est un état que l'éditeur
rencontrera, et qui doit avoir une apparence décidée.

### Le nœud

- **en-tête** : nom, et une couleur ou un pictogramme par famille ;
- **corps** : les prises, avec leur nom ;
- une prise **non connectée** affiche sa **valeur** (curseur, pastille de
  couleur, liste) — ⚠️ **et cette valeur disparaît quand un fil arrive**. C'est
  ce que fait Blender, et c'est ce qui rend l'état impossible à confondre ;
- une prise **exposée** (pilotable depuis le code) — elle porte un **nom public** ;
- **replié** : le nœud réduit à son en-tête ;
- **sélectionné** · **survolé** · **désactivé** ;
- 🔴 **en erreur**, avec sa raison lisible sans clic.

### Les fils

- fil de **valeur** contre fil d'**exécution** — ⚠️ la différence la plus
  importante du dessin ;
- fil **en cours de tirage**, avec les prises compatibles mises en évidence et
  les incompatibles éteintes ;
- fil **sélectionné** ; fil qui **passe derrière** un nœud ;
- ⚠️ un **graphe invalide doit être dessinable** : lien pendant, cycle en cours
  de construction, entrée vide. Un éditeur passe son temps dans ces états.

### Le canevas

- la grille, le déplacement, le zoom — et **ce qui se simplifie quand on
  dézoome** (les prises disparaissent avant les noms) ;
- la **recherche de nœud** : ce qui s'ouvre quand on tire un fil dans le vide,
  ⚠️ **filtré par le type de la prise d'origine** ;
- la **minicarte**, si tu en veux une ;
- le **groupe** : plusieurs nœuds repliés en un seul, avec ses propres entrées
  et sorties.

---

## 7. Ce qui existe déjà et qu'il ne faut pas redessiner

`Kernel/Runtime/NKGraph` porte le **cœur** depuis le 31/07/2026 : le modèle,
les prises typées, la validation, les sous-graphes, le tri topologique,
la sérialisation `.nkgraph`, l'annuler/refaire par commandes inversibles.

**Décision d'architecture de Rodolf, 2026-07-09** — un seul système de graphe
pour tout l'écosystème : matériaux, VFX, blueprint, modélisation procédurale,
graphes d'animation, futur rig. En trois couches :

```
3 — SÉMANTIQUE MÉTIER   chez chaque consommateur (bibliothèques de nœuds,
                        compilateur NkSL pour les matériaux…)
2 — ÉDITION             le canevas, dans Engine/NKEditorKit  ← CE QUI SE DESSINE
1 — CŒUR                Kernel/Runtime/NKGraph — modèle pur
```

> ⚠️ **Le canevas est la couche 2 : il est UNIQUE et partagé.** Ce que tu
> dessines servira aux matériaux, aux blueprints, au VFX et à tout ce qui
> viendra. **Une apparence apprise une fois, réutilisée partout** — c'est la
> raison d'être de ce découpage, et c'est pourquoi ce document mélange
> délibérément les deux mondes.

---

## 8. Une question ouverte, à trancher au dessin

**Les blueprints doivent-ils utiliser le même canevas que les matériaux, avec
des nœuds différents — ou deux éditeurs distincts ?**

Ma recommandation : **le même canevas**. Un utilisateur qui apprend à câbler un
matériau sait câbler un blueprint. Et l'architecture est déjà faite pour ça.

**Mais ça t'impose une contrainte de dessin** : les deux familles de fils
doivent cohabiter à l'écran sans se confondre, y compris dans un graphe qui les
mélange. C'est faisable — Unreal le fait — mais ça se décide **au dessin**, pas
après.
