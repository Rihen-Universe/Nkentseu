# GenIA 3D — état de reprise

**AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
Écrit le 2026-09-18 à 06 h 15, avant l'extinction de 07 h 00.
**Pour quelqu'un qui n'a aucune mémoire de cette session.** Branche `feat/genia-3d`.

---

## 0. LE PREMIER GESTE DE LA PROCHAINE SESSION

```
cd D:/Projets/2026/Nkentseu/Nkentseu-genia3d
git log --oneline -6                       # 6 commits, rien poussé
Build/Bin/Release-Windows/NKTexte3D/NKTexte3D.exe --scene Tools/Genia/scenes/chaise.nkscene --out /tmp/c.obj --verifier
```
Si ce dernier rend **0**, tout ce qui est décrit ici tient encore. S'il rougit, **la
première hypothèse est la machine, pas le code** — et il faut le dire ainsi.

Puis lire, dans cet ordre : `Tools/Genia/FORMAT_SCENE.md`, puis `echanges/genia-3d.questions.md`
à partir du **R13**.

---

## 1. Ce qui est ACQUIS et mesuré

| fait | par quoi |
|---|---|
| **41 864 → 997 sommets**, silhouette à **99,11 %**, objet éditable | deux instruments sans code commun (R14) |
| le maillage de TripoSR **n'est pas manifold** : χ = 313 impair, 279 arêtes | R14.2 |
| `DecimateQEM` **répare** 277 de ces 279 arêtes avant même de décimer | R14.3, trouvé par « prouver le zéro d'abord » |
| χ **invariant sur 41 459 contractions** | R14.4 |
| le **document de scène** existe, avec placement, échelle par axe, rotation, booléens | `FORMAT_SCENE.md`, commit `18fab6963` |
| la géométrie **se vérifie contre le document** (`--verifier`) | 3 critères, chacun avec sa condition |
| **une chaise sort correcte** — ce que la grammaire de mots déclarait « IMPOSSIBLE » | `logs_genia3d/scene/apercus/chaise_corrigee.png` |
| les 9 cas du banc historique **n'ont pas bougé d'un sommet** | attendu sur ce qui ne doit pas bouger |
| **rembg ne télécharge plus rien** : refus nommé, chemin durable, taille vérifiée | commit `7d5578b83` |

**Mutations vivantes** : `NK_TEXTE3D_MUTE=1|2|3`, `NK_SCENE_MUTE=1|2|3`, `NK_REDUC_MUTE=1|2|3`,
`NK_SURFACENETS_NAIF=1`, `--axe-up brut`. **Aucune ne doit rendre 0.**

---

## 2. LES TROIS PROBLÈMES QUE RODOLF A VUS DANS BLENDER — ET ILS SONT DISTINCTS

Références : `D:/Rihen/Livraisons/Reference_Blender/`.

### (1) « Les vertices sont en désordre » — ce n'est PAS le modèle

Le modèle rend un **champ de densité**. Les sommets sont posés par **notre** étage
d'extraction, sur une **grille régulière** — d'où le fil de fer dense, uniforme et isotrope.
**Aucune intelligence du modèle ne changera ça : c'est notre code.** C'est l'objet du chantier
de retopologie quadrangulaire, qui tourne dans `Nkentseu-retopo`.

### (2) « Pas d'espacement entre les bras et le corps » — DEUX causes, une expérience les sépare

**(a) notre échantillonnage.** À `--mc-resolution 256` sur un personnage d'environ 0,97 m,
une cellule vaut **~3,8 mm**. Un écart bras/corps de 2 mm **ne peut pas** être représenté : il
est plus fin que la maille. Ce n'est pas une opinion, c'est une borne.

**(b) la connaissance du modèle.** Depuis une seule vue, s'il n'a jamais « vu » l'écart,
aucune résolution ne l'inventera.

> **PROTOCOLE ÉCRIT AVANT LA COURSE — À LANCER TEL QUEL À LA PROCHAINE SESSION.**

```
# La MEME image, la MEME graine, seule la resolution change.
PY=D:/Projets/2026/Nkentseu/genia-tools/venv/Scripts/python.exe
$PY Tools/Genia/genia_triposr.py --image <IMAGE> --out r256.glb --device cpu --mc-resolution 256
$PY Tools/Genia/genia_triposr.py --image <IMAGE> --out r512.glb --device cpu --mc-resolution 512
# Le critere : le nombre de COMPOSANTES CONNEXES. Derive, pas a l'oeil.
Build/Bin/Release-Windows/NKGeniaTemoin/NKGeniaTemoin.exe --reduire r256.glb --faces 4000 --out r256.obj
Build/Bin/Release-Windows/NKGeniaTemoin/NKGeniaTemoin.exe --reduire r512.glb --faces 4000 --out r512.obj
```

| issue | conclusion | ce qu'on fait ensuite |
|---|---|---|
| **512 ouvre l'écart** (plus de composantes qu'à 256) | la cause est **notre échantillonnage** | monter la résolution par défaut, et mesurer son coût |
| **512 n'ouvre rien** (même compte) | la cause est **le modèle** | une seule vue ne suffit pas ; il faut plusieurs vues ou un autre modèle |

**Les deux issues sont informatives** — c'est ce qui rend l'expérience non stérile.
⚠️ **Coût attendu : 512 coûte environ 8 fois 256** (la grille est cubique). Sur processeur,
compter **plusieurs minutes** et une empreinte mémoire nettement plus grande. **Relever la
carte avant**, et rapporter temps ET mémoire à côté du résultat.
⚠️ **Le compte de composantes se lit sur le maillage BRUT autant que sur l'allégé** : la
décimation ne crée ni ne détruit de composante, mais le vérifier coûte une ligne.

### (3) « Séparer pour animer » — c'est une TROISIÈME chose, et la plus grosse

Même avec une surface parfaite et des écarts nets, un personnage animable demande des
**parties nommées** (segmentation) et un **squelette** — pas seulement des trous. Ça touche
**NKAnima**. **Non attaqué, et il ne faut pas l'attaquer sans arbitrage** : c'est un chantier
de plusieurs semaines (segmentation sémantique, pose d'un rig, poids de peau), et il vient
**après** la topologie.

---

## 3. UV, TEXTURES, NORMALES — LA PRÉMISSE EST À CORRIGER

Rodolf demande que « l'UV édition et les textures soient bien calculées, pas à peu près ».
**Mesuré sur le fichier produit par la chaîne, par deux instruments indépendants :**

```
attributs declares : POSITION, COLOR_0
                     PAS de TEXCOORD_0   PAS de NORMAL
images embarquees  : 0        materiaux : 0
```

> **Il n'y a pas d'UV approximatives : il n'y a AUCUNE UV, AUCUNE texture, et pas même de
> normales.** La couleur vit **sur les sommets**, donc sa résolution est celle du maillage.
> C'est la cause directe de l'aspect délavé, et ça n'a rien à voir avec la qualité du modèle.

Le dire ainsi lui évite de chercher un réglage qui n'existe pas.

**L'ORDRE DE DÉPENDANCE, et il n'est pas négociable :**

1. **La topologie d'abord.** Déplier une soupe de triangles issue d'une grille ne donne rien
   d'éditable : les coutures tombent n'importe où, l'atlas est illisible. → le chantier de
   retopologie est **en amont**.
2. **Le dépliage ensuite** (coutures, îlots, empaquetage), avec des critères **dérivés** —
   chacun avec ce qui le ferait rougir :
   - **distorsion d'angle** (conformité) — rougit au-delà d'un facteur mesuré sur un cas connu ;
   - **distorsion d'aire** — le rapport aire 3D / aire UV doit rester borné ;
   - **longueur totale de couture** — une couture plus longue que le périmètre est un échec ;
   - **taux d'occupation de l'atlas** — un atlas à moitié vide gâche la moitié de la texture.
3. **La cuisson enfin.** ⚠️ **NE PAS cuire les couleurs de sommets dans la texture.** Ce
   serait **graver la pauvreté actuelle** dans un fichier de 2048². Il faut rééchantillonner
   **la source** — le champ de couleur du modèle, ou l'image d'entrée par projection — pour
   que la résolution de la couleur **cesse d'être liée à la densité du maillage**. C'est toute
   la raison d'être d'une texture.

**Et un défaut à part, petit et réparable : l'absence de NORMAL.** Sans elles, chaque logiciel
les recalcule à sa façon et l'objet ne s'affiche pas pareil d'un outil à l'autre.

---

## 4. CE QUI RATE, NOMMÉ, AVEC SA CONDITION DE RÉOUVERTURE

1. **`op difference` produit un maillage non-manifold — MAIS L'OPERATEUR EST DISCULPE (18/09 soir).**
   Trois cas, dont un **temoin de controle connu sain** :
   | cas | ce qu'il isole | non-manifold | chi |
   |---|---|---|---|
   | sphere moins sphere **interieure** | soustraction **sans arete vive** | **0** | 4 (derive : 2 surfaces imbriquees) |
   | sphere moins sphere **qui la coupe** | entaille, **avec** arete vive | **8** | 38 |
   | « un cube perce » (grammaire) | trou **traversant**, vert depuis le 17/09 | **0** | 0 |

   Le troisieme utilise **la meme operation** `max(d, -dq)` et il est sain : mon soupcon
   « le champ n'est plus 1-Lipschitz » est donc **refute par un temoin qui existait deja**.
   **Nature du defaut, mesuree** : 45 composantes = 1 objet (36 820 sommets) + **44 eclats**
   de 12 a 32 sommets. Des ilots parasites le long de l'entaille, pas une topologie ruinee.
   → *Le domaine de recherche est reduit : la surface soustraite qui COUPE la surface
   d'accueil, ni traversante ni interieure.*
   ⚠️ **Piege nomme** : supprimer les petites composantes rendrait l'objet utilisable et
   MASQUERAIT le defaut — « un correctif qui eteint l'alarme ». Ne pas le faire.

1bis. **(ancienne formulation, conservee)** `op difference` produit un maillage non-manifold. Cas minimal (sphère moins sphère) :
   8 arêtes non-manifold, χ = 38. **La résolution n'y change rien** (128, 192, 256 testés) —
   l'hypothèse « parties trop fines » est **réfutée**. Mon second test (le lissage) **ne
   prouvait rien** : le lissage ne s'applique qu'aux unions dans mon code, donc la
   soustraction restait vive — un négatif mal construit. **Le garde-fou refuse d'annoncer ce
   maillage valide (code 1), donc rien de faux ne sort.**
   → *Se rouvre dès qu'un document aura besoin de creuser. Première piste à tester : le champ
   `max(d, -dq)` n'est plus 1-Lipschitz, et SurfaceNets interpole linéairement.*
2. **L'écrivain OBJ perd les couleurs de sommets.**
3. **Le validateur du pont recopie le vocabulaire de `NKTexte3D`** — deux autorités.
   → *Se rouvre quand `NKTexte3D --vocabulaire` existera.*
4. **Tout est mesuré sur un seul objet** (`chair.glb`) côté décimation.
5. **`SurfaceNetsQuads` porte toujours le défaut non-manifold d'origine**, non corrigé.
6. **Aucune mesure de temps** sur la décimation.

---

## 5. CE QU'IL FAUT DEMANDER À RODOLF

1. **L'IMAGE QU'IL A UTILISÉE.** Elle est introuvable : la seule sortie récente est un
   `test.glb`. **Un résultat sans son entrée n'est pas reproductible.**
   → *Correctif à faire : que le lanceur COPIE l'image d'entrée à côté de sa sortie.* Une
   ligne, et elle évite de reperdre chaque essai.
2. **L'arbitrage sur le branchement de la décimation** : dans le générateur, ou une question
   à l'import (R14.8) ? Le second vit dans `Applications/NK3DModeler/`, hors périmètre.
3. **Le chantier « séparer pour animer »** (§2.3) : c'est lui qui décide s'il vaut ses semaines.

---

## 6. ÉTAT GIT

**6 commits** sur `feat/genia-3d`, **rien poussé**. `transit` était à `265c3f7e4` et **n'est
plus ancêtre** de cette branche : une refusion sera nécessaire, et *une prédiction ne vaut que
contre l'état contre lequel elle est faite* — la refaire au moment de fusionner.

**Aucune construction de l'espace complet lancée** : uniquement des cibles isolées
(`--target NKTexte3D`, `--target NKGeniaTemoin`), chacune avec sa ligne `Status: ✓ SUCCESS`.
