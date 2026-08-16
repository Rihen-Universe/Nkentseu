# NK3DModeler — Feuille de route

> Document **suivi par git** (contrairement à `CARNET.private.md`, qui garde le
> journal détaillé et les idées écartées). Il dit **ce qui reste à faire** et
> **dans quel ordre**, avec assez de détail pour reprendre sans rien redécouvrir.
> À mettre à jour **à chaque palier franchi**, pas en fin de session.

L'application est **un seul DCC** (pas de séparation modeleur / animation).
Langue de travail : **français**. Toute décision d'interface vient de Rihen et
est consignée avec sa raison.

> ## ► AGENT QUI REPREND LE MODELEUR : COMMENCE PAR LA PASSATION
>
> **[`PASSATION_2026_08_08.md`](PASSATION_2026_08_08.md)** — état exact au 8 août
> 2026, décisions déjà prises (à ne pas re-débattre), bugs corrigés avec leur
> leçon, et le **prompt de reprise** prêt à l'emploi.
>
> **Le chantier en cours est la PERSISTANCE DE TOUS LES TYPES DE FICHIERS**
> (projet, scène, mesh, texture, matériau, mesh procédural…), chacun en **fichier
> séparé** ou en **blob dans le projet**. Un défaut d'architecture reste ouvert :
> les onglets sont la seule source de vérité des scènes.
>
> **Règle absolue de Rihen** : *fermer un onglet ne doit JAMAIS supprimer quoi
> que ce soit.* Un onglet est une **vue** sur un asset, jamais l'asset lui-même.

---

## Ordre décidé par Rihen (révisé le 2026-08-02)

Cet ordre prime sur toute autre priorisation. **La modélisation est passée en
tête** : c'est le cœur de l'outil, tout le reste s'y raccroche. La sauvegarde
« viendra avec le temps » — elle n'est volontairement **pas** en tête.

| # | Chantier | État |
|---|---|---|
| 1 | **Modélisation** — et d'abord la refonte du **panneau droit** (maquette Banani) | 🔄 en cours |
| 2 | **Caméras** : plusieurs caméras, bascule, vue caméra | 🔄 socle livré (v15) |
| 3 | **Éclairage** : terminer la phase lumières | ⬜ |
| 4 | **Import de modèles** | ⬜ |
| 5 | **Terminer les éléments du combo Ajouter** | ⬜ |
| 6 | **Mode Édition** (sommets / arêtes / faces, sculpt) | ⬜ |
| 7 | **Sortir les 96 objets de la démo** — *uniquement quand toute la modélisation est terminée* | ⬜ |

### 1. Modélisation — refonte du panneau droit 🔄

Maquette de référence : Banani, flow **NK3D Modeler**, écran « Mode Objet (v2,
ref complète) », composant `NK3DPropertiesTabs`. Icônes **Lucide**.

**Livré**
- Les pastilles sont les **quatre** de la maquette — Modèle (`box`), Rendu
  (`sun`), Scène (`layers`), Modificateur (`sliders-horizontal`) — et **une
  seule est active à la fois** ; recliquer l'active replie le panneau.
- Les sept icônes Lucide manquantes ont été dessinées : `sun`,
  `sliders-horizontal`, `link-2`, `square-check`, `plus-circle`,
  `minus-circle`, `tag`.
- Les réglages de l'outil, dont la pastille disparaît dans la maquette, sont
  hébergés sous « Modificateur » en attendant leur vraie place.

**Reste à faire — contenu de la pastille Modèle**, dans l'ordre de la maquette :
Transformation (Position / Rotation / Échelle) · Dimensions · Relations
(Parent / Enfant) · Matériaux · Groupes de Vertex · Shape Keys · Maps UV ·
Attributs de Couleur · Attributs · Espace Texture · Données Géométrie
(Vertices / Faces / Edges).

Chaque ligne de transformation porte ses trois boutons `lock`, `refresh-cw`,
`link-2` ; chaque élément de liste porte sa rangée de quatre boutons
`square-check`, `square`, `plus-circle`, `minus-circle`, et chaque section de
liste finit par un bouton « + Ajouter ».

**Règle absolue** : aucune donnée inventée. Les sections dont le modèle de
données n'existe pas encore (groupes de vertex, shape keys, maps UV, attributs)
affichent un état vide honnête et se remplissent de ce que l'utilisateur crée —
jamais d'entrées de démonstration.

Puis viennent les trois autres pastilles, à retravailler avec Rihen.

---

## 1. Caméras 🔄

**Livré (v15)** : sélecteur de vue en haut-gauche du viewport listant « Vue 3D »
et **toutes les caméras du document actif** ; la vue caméra montre exactement ce
que voit la caméra (position monde, regard **-Z local** comme le filaire, focale
du nœud) ; retour à la vue 3D avec **restitution de la pose libre** mémorisée à
l'entrée ; la vue caméra force la perspective (une caméra réelle n'est pas
orthographique) ; sortie automatique si la caméra disparaît ou change de document.

**Reste à faire :**
- Raccourci clavier (façon pavé numérique 0) et « caméra active » de la scène.
- **Piloter la caméra depuis la vue caméra** : naviguer déplace la caméra elle-même
  (verrou façon UE5 « pilot »), au lieu de subir la pose.
- Cadre de sécurité / rapport d'aspect du rendu, surbalayage, grille de composition.
- Profondeur de champ, exposition, et les autres propriétés ciné.
- « Aligner la caméra sur la vue » et « aligner la vue sur la caméra ».

## 2. Éclairage ⬜

- **Mélange de deux textures** de lumière en **pré-composition CPU** (décidé avec
  Rihen ; le nodal viendra avec NKGraphe).
- **Textures de lumière par fichier** : remplacer le numéro d'atlas (8 slots) par
  une vraie image importée.
- Ombres par lumière, portée/atténuation affinées, IES éventuellement.
- Widget de sélection dans la vue pour les lumières utilisateur.

## 📐 Modes objet / édition, et ce qu'est un sous-mesh — SPÉCIFICATION (Rihen, 2026-08-17)

*Écrite ici parce qu'elle n'existait que dans un fichier d'échange non versionné.
C'est la première fois que le comportement objet/édition est fixé noir sur blanc.*

### Les deux modes

| | mode **OBJET** | mode **ÉDITION** |
|---|---|---|
| un clic sélectionne | **le model entier**, tous ses sous-mesh avec | **un sous-mesh**, ou des faces |
| on édite | la transformation, et la **liste de matériaux** | la géométrie, et **quelle partie porte quel matériau** |
| granularité du matériau | l'emplacement dans la liste | **une partie ou tout un sous-mesh** |
| séparation | — | **retirer un sous-mesh → il devient un model, et disparaît de l'original** |

> « Un model c'est un assemblage de vertices, edges et faces reliés ou non entre
> eux. Des mesh d'un model peuvent porter des matériaux différents ou le même. »

**Conséquence directe** : « le matériau du model » n'a pas de réponse unique.
Un model porte une **liste** de matériaux, et on choisit lequel on modifie. Un
panneau conçu pour un matériau unique n'a donc rien à afficher — c'est le défaut
signalé le 17/08 (« je ne peux pas modifier le matériau d'un model porté »).

### Un sous-mesh EST une composante connexe

Rihen l'a défini par un geste : `L` sous Blender sélectionne tout ce qui est
**relié** à l'élément survolé. Sa phrase le disait déjà — « reliés **ou non**
entre eux » : le « ou non » portait la définition.

**Ce n'est pas une convention d'affichage, c'est une propriété calculable de la
géométrie.** Et trois gestes demandés reposent sur cette seule primitive :

| geste | ce qu'il calcule |
|---|---|
| `L` — sélectionner un îlot | composantes connexes |
| `P` — séparer par îlots | composantes connexes |

⚠️ **RECTIFICATIF (2026-08-17, mesuré).** J'avais ajouté ici « quel sous-mesh
porte ce matériau → composantes connexes ». **C'est faux, et la nuance décide de
tout l'import.** Le matériau n'est pas une notion géométrique : il est **déclaré
par le fichier**. Dans un OBJ, `usemtl` précède les faces concernées.

| niveau | qui le déclare | dans un OBJ |
|---|---|---|
| frontière de **model** | le fichier | `o <nom>` |
| frontière d'**emplacement de matériau** | le fichier | `usemtl <mat>` |
| **composante connexe** | personne — se **calcule** | — (sert à `L` et à la séparation manuelle) |

Contre-exemple fourni par Rihen et mesuré (`sofa.obj`, 6 858 lignes) : **10 `o`,
0 `g`, 10 `usemtl`**, entrelacés dans l'ordre du fichier. Deux des trois niveaux
sont donc déclarés ; **un seul se calcule**.

**À écrire UNE fois** (union-find sur les arêtes, ou parcours depuis chaque
sommet non visité), avec ses deux invariants de test : la somme des tailles des
composantes égale le nombre de sommets, et aucun sommet n'appartient à deux
composantes. Trois implémentations divergeraient.

### La frontière entre deux fichiers est le MODEL, pas le sous-mesh

> « Des models sont chacun dans leur fichier de model, et des models qui ont des
> sous-mesh — eux, ils sont dans le même fichier model. »

```
un .fbx importé
  ├── model A ──────────→ A.nkmodel      (fichier propre)
  │     ├── sous-mesh A1  ┐
  │     ├── sous-mesh A2  ├─ tous DEDANS, même fichier
  │     └── sous-mesh A3  ┘
  └── model B ──────────→ B.nkmodel      (fichier propre)
```

⚠️ **Ne pas découper l'import par connexité.** Deux frontières, deux critères :

| frontière | décidée par | nature |
|---|---|---|
| entre deux **models** | ce que le **fichier déclare** | une décision d'**auteur** |
| entre deux **sous-mesh** | la **connexité géométrique** | une propriété **calculée** |

Découper l'import par connexité éclaterait un model que l'artiste avait voulu
d'un seul tenant.

### ⛔ BLOQUEUR MESURÉ — `NkOBJLoader` jette les noms d'objets

`Kernel/Runtime/NKRenderer/src/NKRenderer/Mesh/NkOBJLoader.cpp` (350 lignes) :
la détection se fait caractère par caractère (`c0 == 'v'`, `c0 == 'f'`…) et
**aucun test n'existe pour `'o'` ni `'g'`**. Le chargeur suit `curMat` — le
matériau courant — et **rien pour l'objet courant** (`curObj` : 0 occurrence).

Conséquence : le `sofa.obj` déclare **10 objets nommés**, et les dix noms sont
perdus à la porte. **La décomposition demandée est donc impossible aujourd'hui —
non par le format, qui porte l'information, mais parce que le lecteur ne l'écoute
pas.**

Ce qui rend le correctif petit : le chargeur fait **déjà** exactement ce qu'il
faut pour le matériau. Il manque la **symétrie** — un `curObj` mis à jour sur
`o`, propagé aux faces, plus le nom porté jusqu'au sous-mesh.

#### 🔬 Mesures du 17/08 — elles corrigent trois points ci-dessus

**1. « ils deviennent un seul maillage » était inexact, et le vrai mécanisme est
pire.** Le chargeur **découpe déjà** des sous-mesh — mais **sur changement de
matériau** (`subMat != curMat`, `NkOBJLoader.cpp:235`), jamais sur l'objet. Or
dans le `sofa.obj` de Rihen, mesuré :

```
10 x « o »  ...  9 d'entre eux portent le MÊME « usemtl Material »
                 seul Sofa_base_Cube porte « Material_Wool.jpg »
```

`curMat` ne changeant pas, **aucune coupe n'est ouverte** : les 9 objets nommés
fusionnent en **un** sous-mesh. Prédiction déduite du code : ce fichier se charge
en **2 sous-mesh au lieu de 10** — vérifiable gratuitement, le chargeur annonce
lui-même `%u sous-meshes` dans son log au prochain import.

**2. ⭐ Le champ de nom EXISTE DÉJÀ — l'absence est dans le remplissage.**
`NkSubMesh` (`NkMeshSystem.h:66`) porte `NkString name;`. La structure de sortie
n'est donc **pas** à changer : le correctif est la symétrie ci-dessus, plus une
coupe de sous-mesh sur changement d'objet **en plus** du changement de matériau,
et `sm.name` enfin rempli.

**3. Ce n'est pas un chargeur sur trois, c'est DEUX sur trois :**

| chargeur | remplit `sm.name` ? |
|---|---|
| **glTF** | ✅ `sm.name = meshName` (`NkGLTFLoader.cpp:1066`), repli `"primitive"` |
| **OBJ** | ❌ jamais — ne remplit que le nom du *matériau* |
| **FBX** | ❌ jamais — pose `firstIndex/indexCount/baseVertex`, rien d'autre |

FBX est un cas **différent** d'OBJ : il parse déjà un arbre de nœuds complet
(`Model`, `Geometry`, `Connections`). L'information est dans le chargeur, elle
n'est pas portée jusqu'au sous-mesh. **La structure commune existe ; deux
chargeurs sur trois doivent apprendre à la remplir.**

**4. ⚠️ Le point de vérité est le consommateur : PERSONNE ne lit `sm.name`.**
Zéro lecteur dans tout le dépôt. Remplir le champ est donc **nécessaire et pas
suffisant** — sans consommateur, rien ne signalerait qu'on l'a mal rempli. **Le
seul détecteur sera le premier consommateur, c'est-à-dire la décomposition
elle-même.**

*(Correction de mon propre compte : j'avais annoncé « 14 usages de `subMeshes[` ».
Ce chiffre mélangeait **deux symboles** — 8 des 14 sont dans
`Externals/Libs/NKAssimp` (Ogre/Debone), sans rapport avec `NkGLTFMeshData`. Le
vrai périmètre est **4 sites** plus le tableau parallèle `subMeshMaterial`.
Encore la règle des deux prédicats, cette fois sur mon propre décompte.)*

### ✅ BLOQUEUR LEVÉ (17/08) — `a5dd6011` (inerte) puis `d480543e` (la coupe)

**Périmètre du risque, mesuré AVANT d'écrire** : aucun indice de sous-mesh n'est
**persisté** nulle part (NK3DModeler ne mentionne `SubMesh` que deux fois, deux
libellés d'interface) ; `subMeshMaterial` est lu **par position** dans 6 démos,
mais celles-ci reconstruisent les deux tableaux ensemble à chaque chargement,
donc elles restent cohérentes quel que soit le nombre de sous-mesh ; le seul
`Check` sur un compte exact (`GLTFLoaderTest`, « 1 submesh ») porte sur du
**glTF**, non touché. **Rien ne se décale en silence.**

**Mesure avant/après sur `sofa.obj`**, même binaire, même fichier, `.mtl` présent :

| état | sous-mesh | verts | indices | matériaux |
|---|---|---|---|---|
| **avant** (coupe matériau seule) | **2** | 2310 | 9348 | 2 |
| **après** (coupe objet + matériau) | **10** | 2310 | 9348 | 2 |

2 → 10, soit exactement les 10 `o` du fichier — et la **géométrie est
identique** : la coupe repartit les plages d'indices, elle ne crée ni ne perd
rien. **Cas limites, coupe active** : `tree.obj` (aucun marqueur) reste à 1,
`rock.obj` (un seul `o`) reste à 1 — aucun éclatement parasite.

⚠️ **Une mesure intermédiaire était fausse, et la cause vaut d'être retenue** :
mon premier relevé donnait « avant = 1 ». J'avais copié le `.obj` **sans son
`.mtl`** — tous les `usemtl` échouaient donc à se résoudre et `curMat` restait à
−1 pour tous les objets. Setup dégradé, pas comportement réel. *Une mesure ne
vaut que ce que vaut son montage* — le pendant exact de « une recherche
exhaustive ne vaut que ce que vaut sa racine ».

**Reste à faire côté FBX** : il remplit désormais `sm.name` (nom du `Model`
propriétaire, sinon de la `Geometry`), et chaque `Geometry` y est **déjà** un
sous-mesh — la frontière existe donc sans coupe supplémentaire. **À confirmer sur
un FBX multi-objets réel avant de le tenir pour acquis.**

### ⚠️ Deux fonctionnalités demandées attendent la MÊME brique absente

« Retirer un sous-mesh pour en faire un model » suppose que la géométrie d'un
nœud utilisateur soit **éditable** — capacité mesurée absente le 16/08 (le mode
édition n'accepte que les objets de démo, `< kNumObj`). **La copie indépendante
de la duplication attend exactement la même chose.** Ce n'est pas deux
chantiers : c'en est un, dont dépendent deux demandes.

### Chemin par étapes — ne pas construire le mode édition en entier

1. **Le panneau de matériaux en mode objet** — la liste, et le choix de celui
   qu'on modifie. C'est le défaut que Rihen subit aujourd'hui.
2. La primitive de composantes connexes, avec ses invariants.
3. Le mode édition sur les nœuds utilisateur (brique commune ci-dessus).

### ✅ RECTIFICATIF — `apres=5` sur un conteneur n'est pas un défaut (16/08)

J'avais lu `MESURE materiau : noeud=110 avant=-1 demande=5 apres=5` comme la
preuve que le matériau allait au **conteneur** — donc que le correctif
`7836c17f` ne tournait pas. **C'est faux.**

`HostNodeMatAdd` (`NkDemo3D.cpp:1084-1093`) **promeut le matériau en actif**
quand l'objet n'en portait aucun — la règle posée par Rihen le 13 août (« un
objet qui n'avait rien prend celui-ci pour actif »). Elle s'applique aussi aux
conteneurs. Sur un nœud à `avant = -1`, `apres = 5` est donc **attendu**, et ne
dit **rien** de ce que les enfants ont reçu.

Vérifié par ailleurs : les albédos relevés **collent au rendu**.
`Materiau.004 = (0.067, 0, 0.7)` → les cubes bleu-violet ; `Materiau.002 =
(0.7, 0.7, 0.7)` → le cube gris clair. Navigateur, aperçu et rendu s'accordent.

**Leçon de méthode** : une mesure prise sur le nœud qui **ne se voit pas** ne
peut pas répondre d'une couleur à l'écran. Trace ajoutée là où la couleur se
décide (`MESURE enfant peint`, commit `f490f014`).

### ✅ LE CUBE NOIR — RÉSOLU (`ff690067`, 16/08)

Rodolf : *« le matériau porté ne correspond pas à ce qui est rendu »*. Un objet
portait `Materiau.002` (albédo `0,7`) et rendait **noir**. Les traces ont montré
que l'assignation était juste — `MESURE enfant peint : noeud=108 materiau=5`.
**Le défaut n'était pas dans l'assignation.** Deux causes, l'une derrière l'autre,
dans `HostSpawnLike` :

1. **Le matériau n'était pas copié.** Un double naissait sans matériau de projet
   — c'est le `avant=-1` du journal, que j'avais pris pour une curiosité. Il ne
   restait alors que la surcharge pour le peindre.
2. **La surcharge était fabriquée depuis le cache de RENDU.** `nkvpMatCache` est
   ce que la source a été **vue** rendre à la dernière soumission : une valeur
   *observée*, pas une valeur *voulue*. Or un asset du navigateur est une
   **archive** (`nkvpDeleted`), donc **jamais soumise** — son cache vaut zéro.
   Chaque double cloné depuis le navigateur naissait avec une surcharge **noire**,
   et le draw call applique le matériau **puis** la surcharge : elle écrasait donc
   tout matériau assigné ensuite.

**Règle** : une surcharge se **copie**, elle ne se **fabrique** pas depuis un
cache de rendu — et le cache d'un nœud jamais rendu n'est pas une couleur, c'est
un zéro. Voir aussi la règle « état correct ≠ affichage correct ».

**Compromis assumé**, écrit dans le code : une retouche volontaire posée sur la
source depuis le panneau Modèle n'est plus transmise au double.

### ✅ Anomalie n°1 — CORRIGÉE (`c5e3f437`) — et sa cause n'était pas la mienne

J'avais écrit : *« les transformations d'enfants ont l'air d'être absolues »*.
**Elles n'en ont pas l'air : elles LE SONT, et pour tout le système.**
`HostNodeWorld` (`NkDemo3D.cpp:15440`) lit `nkvpEmptyPos` et **ne compose jamais
avec le parent**. Ce que j'appelais « une transform locale fausse » était donc une
**position monde correcte** — la valeur était juste, c'est mon modèle mental qui
ne l'était pas.

**Le vrai défaut est dans le geste de dépôt.** La duplication donne aux maillages
du double les positions absolues des maillages de la **source** ; puis
`SetEmptyTransform` déplace **le seul conteneur** vers le point du lâcher. Comme
un conteneur ne rend rien, la matière restait visible à l'ancienne place : on
dépose à un endroit, la géométrie apparaît ailleurs.

D'où `Demo3DHostSetModelTransform`, qui **mesure** le delta réellement appliqué au
conteneur — `SetEmptyTransform` est incrémental, et le gizmo peut porter un
décalage en plein drag — puis translate d'autant ses maillages internes, par le
**même** parcours d'appartenance que l'archivage et l'écriture d'un fichier de
model.

Elle ne remplace **pas** `SetEmptyTransform` : la relecture d'un projet repose
chaque nœud, maillages compris, et y propager aurait appliqué le delta **deux
fois**. Le geste « poser un model » est distinct du geste « poser un nœud ».

✅ **Le gizmo est sain** — vérifié à l'œil par Rihen le 16/08 : « ça suit ». Le
point que j'avais laissé ouvert n'était pas un défaut.

### 🔴 Mais la position restait fausse — la cause était à la NAISSANCE du double

Rihen, après le test : *« ça n'apparaît pas à la bonne position »*, et il donne
lui-même la piste juste : *« son origine est modifiée entre son fichier et le
moment où on l'ajoute dans la scène, car quand j'ouvre ces fichiers de manière
indépendante ils ont la bonne origine »*.

`HostDuplicateTree` décale la **racine** d'un `offset` — `(0.45, 0, 0.45)`, pour
qu'une copie naisse « à côté » — mais **ne le donnait pas à ses maillages**.
Conteneur et matière étaient donc désolidarisés **dès la naissance**, et
`Demo3DHostSetModelTransform` ne faisait ensuite que **transporter un écart déjà
présent** : le model se posait au point du lâcher, sa matière apparaissait
`offset` plus loin.

**Leçon** : un correctif juste appliqué à un état déjà faux donne un résultat
faux. Le premier correctif (déplacer la matière avec le conteneur) était
nécessaire mais pas suffisant — il fallait aussi que les deux **naissent**
solidaires.

Le commentaire qui vivait là était faux, et il a retardé la trouvaille : il
annonçait que reprendre la position monde ferait partir la matière « au double de
la distance », ce qui suppose une composition `parent × enfant` **qui n'existe
pas**.

### ⚠️ UN TROISIÈME ACTEUR déplace la matière d'un model — et il n'était dans aucune des deux analyses

Demandé par Rihen : *vérifier que les deux correctifs ne se cumulent pas.* En le
vérifiant, j'ai trouvé qu'ils ne sont **pas deux**.

`HostHierRecurse` (`NkDemo3D.cpp:15534`) rejoue **à chaque frame** l'écart entre la
position courante d'un parent et son **cliché** (`sHierPos`), et donne cet écart à
ses enfants. Le masque de transmission `nkvpXmit` vaut **7 dès la naissance**
(`HostAllocUser`) : tout se transmet, position comprise. **C'est ce mécanisme qui
fait que le gizmo « suit »** — la propagation est voulue pour un déplacement
ordinaire.

🔴 **Le fait mesuré, et il est certain** (grep exhaustif sur `NkDemo3D.cpp`, seul
fichier où vit `sHierPos` ; contre-épreuve : le même motif remonte bien ses deux
sites d'écriture) : `sHierPos` n'est écrit **qu'à deux endroits** — le cliché de
fin de frame (`l. 15732`) et `Demo3DHostHierarchyResync` (`l. 15746`), appelé
**uniquement au chargement d'un projet** (3 sites, tous dans `Project/`).

**`HostAllocUser` ne l'écrit jamais.** Un nœud qui vient de naître porte donc le
cliché de **l'occupant précédent de son emplacement**. Le `dp` calculé à la
première frame après un dépôt n'est pas le geste de l'utilisateur : c'est un écart
contre une valeur qui n'a aucun sens.

⬜ **Ce qui n'est PAS établi** : que ce `dp` s'**ajoute** effectivement à la pose au
lieu de la remplacer. Le raisonnement le suggère, il ne le prouve pas — et c'est
exactement le genre d'enchaînement qui a déjà produit deux fausses causes sur ce
défaut. **Mesuré par les deux traces posées le 17/08**, qui encadrent la frame :

| trace | où | ce qu'elle dit |
|---|---|---|
| `MESURE cumul` | fin de `Demo3DHostSetModelTransform` | barycentre X/Z de la matière **juste après la pose**, contre le point demandé |
| `MESURE hier` | `HostHierRecurse`, si le parent est un model | le cliché, le courant, et le `dp` **propagé à la frame suivante** |

**Lecture de `ECART X/Z` de `MESURE cumul`** — l'instrument discrimine les trois
cas, il ne peut pas seulement confirmer :

| écart | verdict |
|---|---|
| **~0** | la naissance et la pose s'emboîtent |
| **~(−0,45, −0,45)** | le décalage de naissance n'atteint pas les maillages |
| **~(+0,45, +0,45)** | il est compté deux fois |

⚠️ **Et `MESURE hier` prime** : si elle affiche un `dp` non nul après un dépôt, un
`ECART` nul de `MESURE cumul` ne veut rien dire — il aura été mesuré **avant** le
troisième acteur. *Un instrument posé trop tôt ne peut que confirmer.*

**La trace `MESURE dup enfant` a été retirée** : sa question — les enfants
naissent-ils tous du même chemin ? — est tranchée, et elle parlait une fois **par
maillage**, ce qui aurait noyé les deux ci-dessus.

### 🔎 Piste voisine, NON mesurée : le panneau Propriétés ignore la porte « model »

`Demo3DHostSetModelTransform` a été créée parce que déplacer le seul conteneur
laisse la matière derrière. Les **deux** chemins de dépôt passent par elle. Mais
`NkModelerProperties.h` appelle encore `Demo3DHostSetEmptyTransform` sur le nœud
sélectionné en **trois** endroits — `l. 4963` (coller une transform), `l. 4972`
(réinitialiser), `l. 5421` (édition directe de Position/Rotation/Échelle).

Si le nœud est un model, taper une position dans le panneau devrait donc laisser sa
matière en arrière — **sauf** si la propagation de `HostHierRecurse` la rattrape à
la frame suivante, ce qui est précisément la question ouverte ci-dessus. **Les deux
questions n'en font qu'une**, et la même mesure les tranche toutes les deux. Ne
rien corriger ici avant qu'elle ait parlé.

### 🔁 J'ai écrit un doublon — `Demo3DHostRecenterModel` avait déjà un jumeau

Rodolf a rappelé la règle le 17/08 : *chercher d'abord, écrire seulement si ça
n'existe pas.* **Je ne l'ai pas appliquée en écrivant `Demo3DHostRecenterModel`
(`4dd4ed21`)**, et le jumeau était à 2 000 lignes de là, dans le même fichier :

| existant | rôle |
|---|---|
| `Demo3DHostMeshesCenter` (`l. 14005`) | centre de la matière d'un nœud |
| `Demo3DHostSetNodeOrigin` (`l. 13985`) | déplace l'origine **en compensant** les enfants |

Utilisés ensemble par le panneau Propriétés (`NkModelerProperties.h:5015-5021`) pour
le bouton « origine → centre de la géométrie » — **exactement mon geste**. C'est le
symptôme que la règle décrit : un helper local ne déclenche aucun avertissement, ne
casse rien, et se contente d'exister.

⬜ **Mais je ne fusionne pas, et la raison est un défaut présumé** (lecture de code,
**NON mesuré**) : les deux ne calculent pas la même chose.

`Demo3DHostMeshesCenter` renvoie `origine_du_nœud + moyenne(positions des enfants)`.
Or les positions de ce système sont **absolues** — c'est établi et mesuré
(`HostNodeWorld` ne compose jamais avec le parent). Ajouter l'origine du parent à
une moyenne déjà absolue **compte le parent deux fois**. Et `Demo3DHostSetNodeOrigin`
recule ensuite les enfants de `d`, ce qui sous sémantique absolue **déplace la
matière** au lieu de la laisser en place.

C'est **la même fausse prémisse** que le commentaire retiré dans `HostDuplicateTree`
— une composition `parent × enfant` qui n'existe pas. Elle a survécu à un endroit de
plus que je n'avais vu.

🎯 **À trancher après la mesure du dépôt** : si le défaut est confirmé, le bouton
« origine → centre » du panneau est faux, et `Demo3DHostRecenterModel` (qui ne fait
pas l'addition) est la version juste — c'est alors **elle** qui doit absorber les
deux autres, pas l'inverse. Corriger avant de mesurer reviendrait à réparer un
troisième site sur une prémisse non vérifiée, ce qui a déjà coûté deux fausses
causes sur ce défaut.

### ❌ Anomalie n°2 — ELLE N'A JAMAIS EXISTÉ. C'était l'instrument

**Verdict mesuré** : `MESURE dup model : src=98 -> root=107 internesDeLaSource=2
nes=2 enfantsDirectsDuDouble=2`. **Les trois comptes concordent — aucun enfant en
trop.** La source avait bien **deux** maillages ; le second est un petit-enfant,
que ma mesure ne voyait pas.

Je l'avais écrite comme un fait : la source `98` a **un** enfant (`99`), la copie
`110` en a **deux** (`111`, `112`). **Les deux nombres sont justes, mais ils ne
répondent pas à la même question.**

`HostIsInnerMeshOf` (`NkDemo3D.cpp:15880`), qui sélectionne les nœuds à copier,
**remonte la chaîne des maillages** : il attrape les meshes **en profondeur**. Ma
mesure, elle, comptait la **parenté directe**. Et le recâblage (`ligne 16019`)
rattache à `root` tout nœud dont le parent n'est pas dans la carte — **un
petit-enfant devient donc légitimement un enfant direct du double**. Cela produit
exactement le compte observé, sans le moindre chemin en trop.

**L'écart peut donc n'exister que dans l'instrument.** Il est mesuré par le
récapitulatif `MESURE dup model`, qui met les deux prédicats côte à côte
(`internesDeLaSource` contre `enfantsDirectsDuDouble`). Tant qu'il n'a pas parlé,
cette anomalie **n'est pas établie**.

**Règle qui en sort** : *un écart entre deux mesures n'est un fait que si les deux
mesurent la même chose.* Avant d'expliquer une différence, prouver qu'elle existe.

### 🔴 RÉGRESSION SIGNALÉE PAR RIHEN (17/08) — origine recalculée au dépôt

> « Ces mesh ont des origines qui **changent au moment où on les met dans la
> scène** — problème qui **n'existait pas** quand on a fait le glisser-déposer la
> première fois. »

**Défaut certain, corrigé (`eb63ac4b`) : un `if` SANS ACCOLADES.** Le commit
`4dd4ed21` a ajouté `Demo3DHostRecenterModel` dans `NkDropSpawnModel`
(`NkModelerCommon.h`) sur un `if` qui n'avait pas d'accolades :

```
AVANT   if (st.dropSrcNode > 0)
            nn = Demo3DHostDuplicateNode(...);   <- seule instruction, gardée

APRÈS   if (st.dropSrcNode > 0)
            Demo3DHostRecenterModel(...);        <- seule ligne encore gardée
            nn = Demo3DHostDuplicateNode(...);   <- SORTIE DE LA GARDE
```

La duplication partait donc sur **tous** les lâchers, y compris sans source, donc
avec `-1` ; et `HostDuplicateTree` lit `nkvpIsModel[src]` sans revalider `src`.
**L'indentation faisait croire que les deux lignes étaient gardées** — la
troisième « chose qui a l'air juste » en entier.

**⚠️ Ce qui n'est PAS établi : que ce soit la cause de ce que Rihen voit.**
`Demo3DHostRecenterModel` **écrit dans le nœud SOURCE** (l'archive) avant
duplication, ce qui colle au symptôme et n'existait pas avant le 16/08. **Mais**
`Demo3DHostArchiveNode` recentre déjà à la naissance, et la fonction est
**idempotente** — sur un asset archivé après le 16/08 elle ne doit **rien**
changer. *Deux causes possibles, un seul symptôme.*

**L'instrument qui les sépare** — `MESURE origine`, écrit pour pouvoir contredire
son auteur : il imprime l'état **d'avant**, pas seulement celui d'après.

| lecture | ce que ça tranche |
|---|---|
| `ECART ~ 0` | la fonction ne touche à rien → **ce n'est pas elle** |
| `ECART != 0` | elle réécrit l'origine de l'archive à chaque dépôt → **c'est elle** |

Noter que le recentrage ne touche que **X et Z**, jamais **Y** — une origine à
moitié réécrite, donc un symptôme irrégulier selon le modèle.

**Ne pas retirer l'appel avant ce chiffre** : ce serait revenir sur la décision de
Rihen du 16/08 (« l'origine d'un model va sur SA MATIÈRE ») sur une prémisse non
vérifiée — ce qui a déjà coûté deux fausses causes sur ce même défaut.

## 3. Modélisation complète ⬜

- **Mode Édition** : sommets / arêtes / faces, sélection, extrusion, biseau,
  boucles, subdivision.
- **Sculpt** (2.5D puis réel), retopologie, décimation.
- Géométrie manquante du menu Ajouter : **texte, courbes, surfaces, metaballs**.
- Modificateurs réels (la liste existe, elle est décorative).

## 4. Import de modèles ⬜

- **Glisser-déposer depuis Windows** (`WM_DROPFILES`) + bouton « Importer ».
- Boîte de **propriétés d'import** (échelle, axe haut, unités, matériaux).
- Formats : mesh (via NKAssimp), **texture**, **image de référence**.
  Le format matériau et le format dataset JSONL existent déjà.
- Conversion vers **nos propres formats** au moment de l'import.

## 5. Terminer le combo Ajouter ⬜

Les natures créées mais **sans géométrie réelle** : texte, courbe, surface,
metaball. Chacune doit avoir son panneau « Ajuster la création » comme les
primitives paramétriques (sphère, icosphère, tore, cylindre, cône, capsule…).

## 6. Sortie de la démo ⬜

Les nœuds 0-95 appartiennent au portage de `--demo=2` (scène 0 du document).
Ne les retirer **qu'une fois toute la modélisation terminée** — ils servent
aujourd'hui de banc d'essai permanent pour le rendu, la parenté et le gizmo.

---

## Chantiers de fond (hors ordre ci-dessus)

### Matériaux — le chantier n'est PAS fini (rappel de Rihen, 6 août 2026)

Les quatre canaux de texture (couleur, normale, ORM, émissif) sont livrés, mais
ce n'est que la **première moitié**. Ce que Rihen attend, et qui reste à faire :

0. **LA PHYSIQUE DE SURFACE D'ABORD** — décision de Rihen (6 août) : *« dès la
   passe sur les matériaux on doit commencer par les implémenter avant de
   continuer le branchement »*. **Pourquoi cet ordre** : chaque paramètre ajouté
   après coup est un champ de plus dans l'`ObjectUBO`, donc une structure qui
   change, donc **les cinq backends à revalider**. Brancher une interface sur un
   shader incomplet fait payer deux fois. Dans l'ordre :
   - **Exposer `clearcoat` et `subsurface`** — `pbr.frag.nksl` les calcule
     **déjà** (lignes 429 et 438) et `NkMaterial` les expose (`SetClearcoat`,
     `SetSubsurface`) : **le panneau du modeleur ne les propose pas**. Deux
     paramètres corrects, déjà payés, hors de portée de l'utilisateur. Le gain
     le moins cher du chantier.
   - **Transmission + IOR** — *rien* dans le shader aujourd'hui. C'est le « S »
     de BSDF (*scattering*, qui inclut la transmission) : sans elle, **pas de
     verre, pas d'eau, pas de liquide crédibles**. Le manque le plus visible
     pour un modeleur.
   - **Anisotropie** — métal brossé, cheveux, vinyle.
   - **Sheen** — tissus, velours.

   **Le mot « BSDF » n'entre pas dans le panneau simple** (couleur, rugosité,
   métallique, vernis, diffusion : le vocabulaire que tout le monde comprend).
   Il entre **avec l'éditeur nodal**, parce que « mélanger deux matériaux » se
   dit *mélanger deux BSDF* — et qu'un nœud de mélange n'a de sens mathématique
   que si ce qu'il mélange est une fonction de distribution : mélanger deux
   rugosités ne donne pas la rugosité du mélange.

1. **Types de surface** — `NkMaterialType` : PBR, Toon, Unlit, Layered, Anime.
   N'exposer que ceux que le renderer **dessine réellement** : chacun sera
   vérifié à l'écran avant d'entrer dans la liste (règle des stubs).
2. **MÉLANGER les matériaux** — un matériau qui en combine deux autres selon un
   masque, une hauteur, une usure. C'est le cœur de la demande, et c'est ce que
   permettent les matériaux en couches (`Layered`, dont des shaders existent
   déjà dans `Resources/NKRenderer/Shaders/Layered*`).
3. **Matériaux NODAUX** — des nœuds de matériau qui dépendent d'autres
   matériaux. **Passe obligatoirement par NKGraph** (décision du dépôt, cf.
   `CLAUDE.md` : un seul substrat de graphe pour Blueprint, matériaux,
   texturing procédural et motion). Ne pas construire un graphe en silo.
4. **Sauvegarde `.nkmat` / `.nkmati`** via `NkMaterialAsset`/`NkMaterialLibrary`
   (le format existe), avec les extensions de `CONVENTIONS_FICHIERS.md`.

**Ordre décidé par Rihen (6 août)** : sauvegarde/chargement de scène →
import/export → **puis** les matériaux. Les matériaux viennent après parce
qu'un matériau mélangé ne vaut que s'il survit à la fermeture et s'applique à
un modèle importé.

### Le `.nk3dm` a DEUX modes (décidé avec Rihen, 6 août 2026)

Comme glTF/GLB, et pour les deux mêmes raisons :

| | **Mode lié** | **Mode empaqueté** |
|---|---|---|
| Écriture | **JSON** (`NkJSONWriter`) | **NKS1** (`NkNativeFormat`) |
| Assets | fichiers séparés, chemins relatifs | dans le fichier |
| Lisible à l'œil, diff git | oui | non |
| À transmettre | un dossier | **un seul fichier** |
| Sert à | redistribuer un asset seul | donner le projet entier |

**Une seule extension `.nk3dm`.** Le mode est une propriété interne, pas une
nature d'asset — exactement comme le nodal pour un matériau (cf. la règle de
`CONVENTIONS_FICHIERS.md` : l'extension dit ce que le fichier PRODUIT, jamais
comment il est écrit dedans). `NKS1` porte sa signature magique en quatre
premiers octets : l'ouverture reconnaît le mode **sans se fier au nom**.

**Pourquoi NKS1 et pas un conteneur maison** (question tranchée par Rihen) :
`NkNativeFormat` sérialise un **`NkArchive`** — la structure que le JSON écrit
déjà. Donc `NkSceneCapture` produit un archive, et ce **même** archive part en
JSON ou en NKS1 : un seul code de capture, deux écritures, aucune divergence
possible. Il apporte en prime un en-tête versionné, un CRC32 et une compression
LZ4 prévue. Et pas de base64 : ni les +33 % de taille, ni le coût de décodage.

**NE PAS ajouter de type binaire à `NKS1`** — j'avais recommandé l'inverse,
Rihen a pointé `NKSerialization/Asset/`, et le format `.nkasset` y fait déjà
exactement ce qu'il faut (`NkAssetMetadata.h`, `NkAssetIO::Write`) :

```
[Header:40][MetadataSize:4][Metadata:NKS1][PayloadSize:8][Payload:octets bruts]
```

avec `payloadOffset` / `payloadSize` / `payloadCRC`, et un CRC distinct pour
l'en-tête. **Les octets bruts vivent À CÔTÉ de l'archive, référencés par
décalage — jamais dedans.** Ce découpage vaut mieux qu'un type binaire dans
`NKS1`, pour trois raisons qu'on perdrait autrement :

- **lecture paresseuse** — une texture se charge à la demande ; noyée dans
  l'archive clé/valeur, elle serait lue entièrement à l'ouverture du projet ;
- **pas de copie** — l'archive ne duplique pas des centaines de Mo en mémoire ;
- **deux CRC séparés** — la corruption d'une texture ne se confond pas avec
  celle de la structure.

**Le travail réel** : `NkAssetFileHeader` ne porte qu'UN payload (parfait pour
un asset, insuffisant pour un projet qui en contient N). Généraliser le même
patron — en-tête, archive `NKS1` de la scène, puis une **table de N entrées**
(décalage, taille, CRC) suivie des blobs. Aucune invention : c'est
`NkAssetFileHeader` avec une table au lieu d'un triplet.

### Ombres — chantier GARÉ le 7 août 2026 (décision de Rihen : la sauvegarde d'abord)

État à l'arrêt du chantier, pour le reprendre sans réapprendre :

**Fait et compilé (29/29), techniques standard, sans régression connue :**
- **Biais du plan récepteur** dans le PCF (`NkShadowAtlas.glsli`) : chaque tap se
  compare au plan du récepteur extrapolé à sa position — garde-fous : repli si
  déterminant quasi nul (silhouettes), extrapolation bornée à ±0.005.
  Les 4 familles (sun/point/spot/area) et les 5 shaders clients en héritent.
- **NoCull définitif** dans la passe d'ombre (`NkRender3D.cpp`, commentaire long) :
  le culling des faces avant collait le contact mais éclairait l'INTÉRIEUR des
  objets fermés (la caméra d'un modeleur y entre). Ne pas le retenter.
- Biais normal en **texels du tile** (0.5), garde des faces d'omni dimensionnée
  sur le noyau PCF réel, douceur affectant enfin Vulkan/GL comme DX11.

**Rihen constate encore : intérieur du cube éclairé + ombre pas posée. Pistes,
dans l'ordre où les vérifier :**
1. **Cache de shaders périmé** — `Build/ShaderCache` a été VIDÉ le 7 août ; si la
   clé FNV ne hache pas les `#include` résolus, les tests précédents tournaient
   sur l'ANCIEN binaire de shader. Retester après recompilation complète AVANT
   tout autre diagnostic.
2. **« Intérieur éclairé » = ambiant/IBL, pas la lumière ?** L'ombre n'éteint que
   la lumière directe ; l'ambiant n'a AUCUNE occlusion aujourd'hui. Un intérieur
   de boîte fermée restera gris-ambiant tant qu'il n'y a pas d'AO — vérifier en
   mettant l'ambiant à zéro : si l'intérieur devient noir, le shadow map est
   CORRECT et c'est un chantier AO (NkVoxelAOSystem existe déjà, init OK au log).
3. Si le contact flotte toujours après (1) : mesurer réellement — une scène, un
   cube à y=0, capturer, comparer au pixel. Pas de réglage à l'aveugle.

### Renommer ET découper `NkDemo3D.cpp` (décidé avec Rihen, 6 août 2026)

Le cœur du modeleur s'appelle « Demo » — héritage de l'époque où c'était une
démo. **Un fichier qui s'appelle « démo » et qui *est* l'application ment sur ce
qu'il est** : le prochain qui ouvre le dossier cherchera le vrai code ailleurs.

| Fichier | Lignes (6 août) |
|---|---|
| `Viewport/NkDemo3D.cpp` | **15 642** (+4 000 sur la seule semaine) |
| `Viewport/NkDemo3DHost.h` | 768 |
| `Viewport/NkDemoCommon.h` | 141 |
| `Viewport/NkDemoRenderer.h` | 35 |

Plus **9 fichiers** qui référencent le symbole `Demo3D`.

**Le nom n'est que le symptôme.** Le vrai problème, c'est 15 642 lignes dans un
fichier : un simple renommage donnerait un `NkModelerCore.cpp` de 15 642 lignes,
plus honnête mais pas plus sain. D'où la décision : **renommer ET découper en
même temps**.

**QUAND** : une fois la **sauvegarde/chargement de scène validée et poussée** —
jamais au milieu d'un chantier fonctionnel, où un diff mécanique noierait le
diff utile. Mais **pas plus tard non plus** : le fichier grossit de milliers de
lignes par semaine, et chaque semaine d'attente renchérit l'opération.

**COMMENT** : découper **par domaine** — vue, outils, enregistrement/vidéo,
matériaux. Chaque morceau sorti est un morceau qui ne grossira plus. La façade
`NkDemo3DHost.h` reste la **porte d'entrée unique** et devient `NkModelerHost.h`,
cohérente avec `Shell/NkModelerWelcome.h` et `Project/NkModelerProject.h` qui
portent déjà le bon préfixe. Vérifier **29/29 en Debug ET Release** après.

### NKGraphe — l'éditeur nodal
« Blueprint » s'appelle désormais **Graphe**. Natures prévues :
**modélisation procédurale**, **texturing procédural**, **matériau**, et
**motion** (plus tard). Un graphe est un asset du navigateur (nature 0 +
sous-type). L'éditeur de **texture** doit permettre de **peindre au pinceau**,
de construire en **nœuds procéduraux**, ou de **mélanger les deux**.

### Fenêtres flottantes dockables (façon UE5)
Le double-clic sur un asset doit ouvrir une **fenêtre modale flottante**
**dockable** dans la barre d'onglets ; une fois dockée, elle remplace tout
l'espace de scène. Demande un gestionnaire de fenêtres + zones d'accueil +
aperçu de dépôt + détacher/rattacher. Aujourd'hui le double-clic ouvre
directement un onglet docké (état final du geste, sans l'étape flottante).

### Sauvegarde et format projet 🔄
**Socle livré (5 août)** : un projet = **un dossier + un `.nk3dm` JSON** à sa
racine (`CONVENTIONS_FICHIERS.md` §5). Fichiers :
`Project/NkModelerProject.{h,cpp}` (état, écriture/lecture via NKSerialization,
projets récents `~/.nk3dmodeler_recent.cfg` au patron NKCode) et
`Shell/NkModelerWelcome.h` (**écran d'accueil** affiché tant qu'aucun projet
n'est ouvert : récents avec image de couverture, Nouveau, Ouvrir, liens).
Menu Fichier réellement câblé : Nouveau / Ouvrir… / Enregistrer /
Enregistrer sous…

**Ce qui n'est PAS fait, et le fichier le dit** (`scene.serialisee = false`) :
scènes, models, arborescence du navigateur, propriétés par scène, matériaux,
dimensions. Enregistrer un projet **ne sauvegarde pas encore la scène 3D**.

Restent aussi : « Fichier > Ouvrir récent » (sous-menu, le peintre de menus ne
sait pas ouvrir de second niveau), « Fermer le projet » (sans elle l'accueil ne
revient jamais dans la session), et un **vrai logo** (l'accueil compose le titre
typographiquement, faute d'image de marque dans le dépôt).

### Annuler / Refaire
Aucun historique aujourd'hui. À concevoir avant que les outils d'édition se
multiplient (chaque opération devra être une commande réversible).

### Divers
- Sous-onglets spécialisés **par document** (modélisation procédurale, etc.)
  à côté de « Modeler ».
- Table de raccourcis **par fenêtre**, configurable.
- Restreindre l'ajout dans un Model : ✅ fait (maillages = sous-meshes ;
  lumières/caméras/empties = cosmétiques).

---

## Règles d'interface acquises (ne pas les redécouvrir)

- **Un document par onglet** : un nœud appartient à **une** scène ou **un**
  éditeur ; ailleurs il n'est ni rendu, ni listé, ni sélectionnable.
- **Scène et Model** seuls ont l'interface complète (hiérarchie, propriétés,
  barre d'outils, viewport). Matériau, texture, graphe et dataset restent
  **vides** tant que leur design n'est pas défini.
- Dans un Model, la **racine de la hiérarchie s'appelle « Model »**, le panneau
  garde le nom « Hiérarchie ».
- **Chaque scène a ses propres propriétés** ; on peut les copier vers une scène
  précise ou vers toutes.
- **Pliage sur la flèche seulement** ; le **clic droit** vaut sur toute la
  largeur d'une ligne ou d'une carte.
- Les menus prennent la **largeur de leur entrée la plus longue** et s'ouvrent
  **vers le haut** quand le bas manque.
- **Aucune référence inventée** dans l'interface : chaque libellé décrit ce qui
  existe vraiment.

## Dette — charger un `.nkmat` écrit dans l'hôte au lieu de rendre une valeur

*(nommée le 2026-08-15 — **prérequis du chantier « l'asset rendu quitte la
scène »**, à lire avant d'écrire son chargeur)*

`NkAsMatRestore(archive, root, slot, texMiss)` ne construit **rien** : il écrit
dans l'hôte du viewport via **42 points d'entrée** `Demo3DHostProjMat*`. Il
n'existe aucune fonction *« lis ce `.nkmat`, rends-moi un matériau »*.

**LE CHIFFRE QUI DIT L'AMPLEUR — mesuré le 15/08 en essayant.** Écrire un banc
qui lit dix `.nkmat` hors du modeleur a buté sur **trois paliers de couplage**,
découverts un par un à la compilation :

1. `NKGui` — l'en-tête des assets tire `NkModelerInput.h`, qui tire le contexte
   d'interface ;
2. `NKEditorKit` — puis le thème de l'éditeur ;
3. **118 symboles `Demo3DHost*` non résolus** — définis dans `NkDemo3D.cpp`
   (17 620 lignes), donc à **compiler** dans toute cible qui veut lire un
   matériau.

**Lire un fichier de matériau tire donc toute la pile d'INTERFACE**, pas
seulement le viseur. Ce n'est pas un coût de lien (ce qui n'est pas appelé ne
part pas dans le binaire) : c'est un coût de **compilation**, payé à chaque
build, par toute cible qui touche au format.

C'est ce chiffre qui a fait **arrêter** le banc d'inventaire
(`Applications/NkMatInventaireTest`, conservé avec son en-tête d'arrêt) au profit
de quatre captures manuelles.

**Une fonction de chargement dont la sortie est un effet de bord sur un hôte
d'interface ne peut être réutilisée par personne** : le lecteur et l'affichage
sont soudés, donc il n'y a qu'un consommateur possible. Un banc d'essai, un
outil de conversion, un test de migration — aucun ne peut s'en servir sans
embarquer le viewport.

⚠️ **Pourquoi c'est un prérequis et pas une remarque** : l'asset rendu sera
**un fichier chargé par une scène**. Il aura besoin exactement de la fonction
« lis ce fichier, rends-moi une valeur » qui n'existe pas ici. Écrite en
reproduisant le même couplage à l'hôte, elle donnera **deux formats non
réutilisables au lieu d'un** — et le second sera découvert après coup, comme
celui-ci.

**Ce n'est pas un chantier ouvert.** C'est la ligne qui doit être lue avant
d'écrire le chargeur de l'asset rendu, pour que la question se pose *avant*.

## Dette à trancher — les bancs GPU sont des tests déclarés en applications

*(nommée le 2026-08-15 ; ce n'est le chantier de personne aujourd'hui)*

Un banc d'essai qui a besoin d'un **device GPU** ne peut pas être un test
`unitest()` : sur les **26 dossiers `tests/`** du dépôt, **aucun** n'inclut
`NkIDevice` ni `NKRHI` — ils font du calcul pur. Les bancs GPU sont donc
déclarés comme des **applications ordinaires**, et ils sont désormais **quatre** :

| banc | ce qu'il vérifie |
|---|---|
| `Applications/NKQ4MatmulTest` | déquantification Q4_K/Q6_K bit à bit + matmul |
| `Applications/NkTensorGpuTest` | tenseurs GPU |
| `Applications/NKGpuBenchTest` | débit des noyaux |
| `Applications/NkMatInventaireTest` | aperçus des `.nkmat` (inventaire matériaux) |

**La question à trancher quand la séparation des cibles de Jenga arrivera ici :
un banc GPU mérite-t-il son propre genre de cible ?** Aujourd'hui ils comptent
comme des applications et restent donc dans le build par défaut.

⚠️ **Pourquoi c'est écrit ici plutôt que laissé à l'évidence** : le cinquième
serait posé sans que personne sache qu'il y en avait déjà quatre. C'est le motif
des six compensations du bloom — chacune raisonnable seule, aucune ne nommant la
cause commune.

## Pièges techniques déjà payés

- ⭐ **Dans un système de transforms absolues, bouger un conteneur exige de bouger
  sa matière.** `nkvpEmptyPos` contient des positions **monde** malgré son nom :
  `HostNodeWorld` les rend telles quelles et **ne compose jamais avec le parent**
  (aucune remontée de `nkvpParentOf` dans tout le fichier). La parenté ne
  transporte donc **pas** la géométrie — elle ne sert qu'à l'appartenance.
  Conséquence : tout geste nouveau qui déplace un conteneur (alignement, symétrie,
  import, rejeu d'historique) doit passer par `Demo3DHostSetModelTransform`, sinon
  la matière reste en arrière — et comme un conteneur ne rend rien, l'objet
  paraîtra simplement ne pas bouger. Payé le 16/08 : j'avais pris cette position
  monde pour une locale fausse et j'ai failli la « corriger » à zéro, ce qui aurait
  envoyé la géométrie de **toutes** les scènes à l'origine. Le nom d'un champ ne
  peut pas être faux au compilateur : rien ne le contredira jamais.
- **Registre de zones** (`NkHitRegistry`) : capacité 1024 depuis v15. À 256 il
  **saturait en silence** et tout ce qui était déclaré tard (les chevrons de
  pliage) devenait mort. Si une interaction cesse de répondre sans raison,
  vérifier d'abord la saturation.
- La **dernière zone déclarée gagne** le survol : une zone large déclarée après
  une petite lui vole ses clics.
- Les **événements clavier ne livrent pas les lettres** au shell : les raccourcis
  passent par **polling NkInput** dans l'hôte.
- Les **combos différés** exigent une adresse d'état **stable** (jamais une
  variable locale recalculée).
- `SetGridFlags` ne doit **jamais** réactiver `g.showAxes` (axe Y du shader
  erroné = « seconde ligne verte »).
- Les scripts de patch doivent vérifier les **tabulations exactes** des ancres
  (`cat -A`) avant d'écrire, et rester **réentrants**.

---

# PASSATION — état au 2026-08-04 (session Rihen + agent)

> Cette section est le **point d'entrée d'un agent qui reprend le modeleur**.
> Elle dit ce qui vient d'être livré, ce qui reste, et dans quel ordre Rihen
> veut l'aborder. Les règles de travail (jamais la STL, français, valider après
> chaque correctif, build Debug ET Release, app fermée avant de compiler,
> relance vérifiée) sont dans `CLAUDE.md` à la racine.

## Ce qui a été livré et validé pendant cette session

- **Ombres** : bande de garde des faces omni (le carré au sol sous une point
  light), plan lointain à 2× la portée (l'ombre ne se tranche plus à la
  limite), aucune limite propre à l'ombre — elle meurt avec l'atténuation ;
  fondu de bord pour la directionnelle.
- **Caméra** : cadenas d'orbite (rotation seule autour du point visé), boule de
  navigation qui suit la caméra, palette qui ne chevauche plus.
- **Espaces** : 7 onglets (Objet, Édition, Sculpture 2.5D, Sculpture,
  Texturing, Patron, Texture painting) = les modes ; chacun a **sa pastille**
  dans le panneau Propriétés. Séparateurs entre onglets.
- **Matériau** : pastille dédiée (bibliothèque, groupes repliables, aperçu 5
  formes, combo système + Ajouter/Nouveau), règles de vie Blender (tout maillage
  naît avec un matériau, le dernier ne se supprime pas, suppression = les
  porteurs convergent), texture de couleur.
- **Aimantation** : cibles géométriques (sommet, arête, face, centres) avec la
  base « Closest » de Blender, aimant-bascule + panneau (cibles ET pas).
- **Divers** : multi-sélection réparée au commit, presse-papiers câblé (il ne
  l'avait jamais été), champs à 259 caractères, Maj+D ne duplique plus deux
  fois, dimensions honnêtes (cube 1 m comme Unreal), pivot dans la barre.

# CAMÉRA — plan d'ensemble

> Piloté depuis le modeleur, mais **destiné à servir d'autres applications**.
>
> **Aucun nouveau module.** J'avais d'abord proposé une bibliothèque dédiée ;
> Rihen a demandé pourquoi ne pas l'intégrer à NKCamera, et la question a
> montré que ma proposition était mauvaise. Le découpage juste :
>
> - **Capture (webcam en incrustation, en vidéo)** → NKCamera, tel quel. Rien à
>   créer : un adaptateur dans le modeleur suffit.
> - **Lien téléphone (découverte, pose, commandes)** → **NKNetwork**, à côté de
>   `NkLobby` et `NkDiscovery` qui font déjà de la découverte et de
>   l'appairage. Pas dans NKCamera : celui-ci **ne dépend pas de NKNetwork**
>   (vérifié dans son `.jenga`), et l'y mettre ferait tirer toute la pile réseau
>   à `NkCameraDemos`, qui veut seulement lire une webcam. Dans l'autre sens,
>   recevoir une pose de téléphone n'a aucune raison d'exiger Media Foundation.
> - **Application mobile** → une application, pas une bibliothèque.

## Les trois usages, décidés avec Rihen

1. **Caméra réelle en incrustation** — la webcam devient une source de sortie
   comme une autre, posée sur l'image finale à la forme voulue : montrer la
   personne qui réalise le travail de modélisation.
2. **Caméra réelle en vidéo** — la même source, pendant un enregistrement.
3. **Téléphone pilotant une caméra virtuelle** — le téléphone ne transmet
   **que la pose** (position, orientation, focale) : quelques dizaines d'octets
   par image. C'est le principe posé par Rihen, et il est juste — transférer
   des images dans ce sens n'apporterait rien. Le retour visuel (voir ce que
   voit la caméra, depuis le téléphone) est un **second étage**, séparable.

## Ce qui existe déjà — et qu'il ne faut donc pas réécrire

| Module | État | Ce qu'on en prend |
|---|---|---|
| **NKCamera** | Livré | Énumération des périphériques, `StartStreaming`, `GetLastFrame`, `ConvertToRGBA8`. Backends Media Foundation (Windows), V4L2, Camera2, AVFoundation, getUserMedia. Mapping caméra physique → caméra virtuelle par **IMU**, livré. |
| **NKNetwork** | Livré | `NkDiscovery` (broadcast LAN : le téléphone trouve le PC sans qu'on tape une adresse), `NkReliableUDP` (ACK sélectif, retransmission sur RTT), `NkBitStream` avec `WriteQuatf` / `WriteVec3fQ` — la quantification pour laquelle cette couche a été écrite. `NkRPC` pour les commandes ponctuelles. |
| **NKMedia** | Livré | `NkImageSequenceWriter` (séquence PNG/JPEG/BMP/TGA/QOI, « workflow Blender »), `NkVideoWriter` (RAW, MJPEG, MPEG-1, **H.264 baseline bit-exact vs ffmpeg**), conteneurs AVI/MOV/MP4/WebM, **mux audio+vidéo** (`AddAudioSamples`, sync 0 ms). |
| **NKAudio** | Livré sauf capture | 256 voix, DSP, WAV/MP3/OGG/FLAC/Opus. |
| **NKCanvas** | Livré | Suffit à l'application mobile : au premier étage le téléphone n'affiche aucune 3D — il lit son IMU, envoie une pose, montre des repères. Au second, il affiche le retour comme une simple texture. |
| **NKImage** | Livré | Déjà exploité par la sortie (formats, `Resize` bicubique). |

## Ce qui manque, et où

| Manque | Où | Pourquoi c'est nécessaire |
|---|---|---|
| **Capture micro** | NKAudio | Sans elle, pas de **voix off** sur un tutoriel. Rihen : « on doit l'intégrer à tout prix, même si ce n'est pas pour maintenant ». Le reste de la chaîne est prêt : mixage, encodage Opus, mux A/V. |
| **Retour visuel vers le téléphone** | NkCamLink + NKMedia | Voir depuis le téléphone ce que voit la caméra. Second étage, explicitement voulu. Flux basse résolution : MJPEG suffit et NKMedia l'encode déjà. |
| **Lien de pose** | NKNetwork | Découverte, appairage, pose quantifiée, RPC de commande. S'assemble depuis les couches existantes — rien à inventer côté transport. |
| **Application mobile** | Applications/ | NKCanvas + NKCamera (IMU) + NKNetwork. |
| **Tracking sans IMU** | — | Le mapping de NKCamera repose sur l'IMU, **absent des webcams Windows desktop**. Piloter la caméra virtuelle depuis une webcam demanderait du tracking visuel : projet à part entière, écarté pour l'instant. |
| **Baromètre** | NKCamera | Variations verticales ~10–20 cm. Retenu comme complément, pas comme solution (aucun déplacement horizontal). |
| ~~**Module `NKXR` (OpenXR)**~~ **→ le module EXISTE** | Kernel/Runtime | Le vrai 6DoF : casque **et contrôleurs**. ⚠️ **Corrigé le 2026-08-14** : cette case disait « Rien n'existe aujourd'hui dans le dépôt » ; `Kernel/Runtime/NKXR/` existe depuis le **10 août** — **7 922 lignes**, `ROADMAP.md` **et** `USAGE.md`, application de démonstration `Applications/NKXRDemo`. Sa roadmap annonce l'étage 0 livré (module + backend simulateur desktop, sans matériel) et en attente de validation par Rihen. Ce qui manque pour le modeleur n'est donc **pas** le module, mais le branchement casque réel + contrôleurs. Sert aussi la VR/AR du moteur, au-delà de la caméra du modeleur. |

## Position et hauteur : mesurées ou déclarées ?

Question de Rihen. Réponse honnête : **la hauteur exacte n'est pas mesurable
par l'IMU**, et ce n'est pas une limite d'implémentation mais de physique.
`NkCameraOrientation` expose `yaw`, `pitch`, `roll` et l'accéléromètre brut —
aucune position. En tirer une position demanderait d'intégrer deux fois
l'accélération : l'erreur croît quadratiquement, la dérive atteint des mètres
en quelques secondes.

| Voie | Ce qu'elle donne | Coût |
|---|---|---|
| **Déclaration** (config) | hauteur exacte, choisie | nul — **retenu pour l'étage 1** |
| **Baromètre** | variations verticales ~10–20 cm après remise à zéro ; rien d'absolu (la météo décale) | moyen, non exposé par NKCamera |
| **ARCore / ARKit** | vraie pose 6DoF (position + orientation) | élevé, SDK propriétaires, chantier à part |

**Étage 1 : position déclarée, orientation mesurée.** Un trépied virtuel dont
on règle la hauteur, et le téléphone dit où l'on vise. **Rihen a raison de
noter la limite** : cela convient aux plans fixes — vue de dessus, de dessous,
panoramique depuis un point — mais pas au mouvement. Pour monter, descendre,
courir, tourner, il faut du vrai 6DoF.

**Le baromètre est à retenir** (Rihen : « on doit y penser ») : il donne les
variations verticales à ~10–20 cm après remise à zéro. Il ne suffit pas seul —
pas de déplacement horizontal — mais il rend crédible un mouvement vertical.

## Se déplacer VRAIMENT dans la scène — le 6DoF

Objectif explicite de Rihen. Trois voies, comparées honnêtement :

| Voie | À écrire | Qualité | Coût |
|---|---|---|---|
| **Casque + contrôleurs VR (OpenXR)** | un backend OpenXR | excellente | **moyen — retenu** |
| ARCore / ARKit | deux intégrations propriétaires, par plateforme | bonne | élevé |
| Notre propre SLAM visuel-inertiel | tout | incertaine | très élevé (années-homme) |

**Pourquoi le casque gagne.** Un casque 6DoF *fait déjà* son tracking
(inside-out, caméras intégrées) : il ne livre pas des mesures à intégrer mais
une **pose position + orientation** déjà calculée, des centaines de fois par
seconde, au millimètre. Et l'accès passe par **OpenXR**, standard **ouvert** de
Khronos — comme Vulkan — et non par un SDK propriétaire.

**Les contrôleurs comptent autant que le casque** : eux aussi suivis en 6DoF,
ce sont deux caméras qu'on tient à la main. Monter, descendre, courir,
tourner : c'est ainsi que se font les mouvements de caméra virtuelle en
production. C'est la réponse directe aux « acrobaties » demandées.

**Rien d'XR n'existe dans le dépôt aujourd'hui** (vérifié : aucun module, aucune
mention d'OpenXR). C'est donc à créer — un module `NKXR` au niveau Runtime,
qui servirait aussi la VR et l'AR déjà envisagées pour le moteur, pas seulement
la caméra du modeleur.

Sur « recréer notre propre système AR » : légitime à terme, mais un SLAM
visuel-inertiel de qualité représente plusieurs années-homme. Le faire **après**
un backend OpenXR qui fonctionne est un choix ; le faire **avant** priverait
longtemps le projet de ce qu'il veut maintenant.

Pour une **webcam desktop**, la question ne se pose même pas : le tableau des
backends de NKCamera donne l'IMU absent sur Windows. Tout en configuration.

## Ce que ce travail apporte aux modules

**NKNetwork y gagne le plus.** Il est aujourd'hui validé par 67 checks et un
bout-en-bout en **loopback 127.0.0.1**. NkCamLink serait son premier usage réel
sur un vrai réseau — Wi-Fi, mobile vers PC, avec latence, pertes et
reconnexions. C'est cela qui éprouve un RUDP, pas un loopback. Deux TODO de sa
roadmap en bénéficieraient directement : les **stats runtime** (RTT, perte),
aujourd'hui partielles, et la **compression des snapshots** si le retour visuel
arrive.

**NKAudio** y gagne sa capture micro, qui manque à tout usage d'enregistrement.

**NKCamera** y gagne un usage réel de son mapping IMU, aujourd'hui livré mais
jamais employé par une application.

## Ordre proposé

1. **Webcam en incrustation** — presque du branchement : `ConvertToRGBA8` rend
   exactement le tampon RGBA que `NkInsetCompose` sait déjà poser, avec les
   formes et le liseré existants. Une source de plus dans la liste.
2. **Vidéo de sortie** — `NkImageSequenceWriter` d'abord (utile tout de suite,
   n'importe quel monteur assemble une séquence), puis `NkVideoWriter`.
3. **Capture micro** dans NKAudio, puis voix off sur la vidéo.
4. **NkCamLink, étage 1** — le téléphone comme manette : découverte, pose.
5. **NkCamLink, étage 2** — le retour visuel.

---

## OUTPUT — livré pendant la pause du 4 août (à valider)

> Release et Debug à 28/28, app relancée et fermée sans erreur au journal.
> **Non poussé** : Rihen valide d'abord.

**Une pastille dédiée**, `Output` (index 6 ; la pastille du mode passe en 7).
Elle est ajoutée en fin de liste et non après `Rendu` où sa place serait plus
logique : les indices de section sont mémorisés dans `st.propOpen` et câblés en
dur ailleurs (`kSelOnly`, la pastille du mode) — les décaler casserait ces
règles en silence.

**Sortie principale** — source (vue 3D ou n'importe quelle caméra), résolution
avec 8 presets (HD, FHD, 2K, 4K, carré, vertical, ciné, web), pourcentage
d'échelle, et la taille réellement produite affichée en toutes lettres.
La résolution est **indépendante de la fenêtre** : un rendu 4K depuis une
fenêtre 1600×900 fait bien 4K. Sans cela le champ n'aurait été qu'une
décoration.

**Destination** — dossier, nom, format, et le chemin du dernier fichier écrit.

**Incrustations** — jusqu'à 8 cibles secondaires posées sur la principale
(« une principale et les autres en miniature, rectangle, carré, cercle etc. »).
Chacune : source, forme (rectangle, carré, cercle, ovale, rectangle arrondi,
losange), position, taille, liseré avec sa couleur, opacité. Position et taille
sont des **fractions** de la principale, donc changer la résolution ne déplace
rien.

**Aperçu dans la vue** — les incrustations se dessinent dans le cadre caméra, à
leur place et à leur forme, numérotées et nommées. Sans cela il faudrait lancer
un rendu pour savoir où elles tombent.

**Le rapport de la caméra suit la sortie** — c'était annoncé dans le code
(« Full HD en v1 ; la pastille Output le pilotera ») : le cadre dessiné, le
voile et le rendu dérivent tous d'une seule fonction. Régler la sortie en carré
ou en vertical se voit immédiatement dans la vue caméra.

### Comment c'est fait, et pourquoi

Redimensionner la cible hors écran et lire ses pixels ne peuvent pas avoir lieu
dans la même image : le GPU doit avoir rendu entre les deux. Le rendu s'étale
donc en étapes — la principale, puis chaque incrustation — chacune en trois
images (poser, laisser rendre, lire). Neuf cibles font vingt-sept images, moins
d'une demi-seconde, et l'interface ne se bloque pas.

Le cadre caméra est forcé en **plein cadre** pendant la sortie. Comme c'est le
point de passage unique qui pilote l'élargissement du champ, le passe-partout
et le recadrage de la capture, le neutraliser à un seul endroit suffit à ce que
l'image de la caméra occupe toute la cible.

Les formes vivent dans `NkOutCompose.h`, à part : c'est du calcul pur, donc
**vérifiable hors de l'application**. Un test isolé génère une planche des six
formes et vérifie 9 propriétés (couverture au centre et aux coins de chaque
forme, liseré sur les quatre bords, opacité, cadres carrés forcés) — toutes
passent.

### Découper la vue — DEUX fonctionnalités distinctes (idées de Rihen)

Elles partagent l'apparence — une vue coupée en morceaux — mais **pas du tout la
sémantique**. Les confondre mènerait à une implémentation qui ne sert bien ni
l'une ni l'autre.

| | **Multi-vue** | **Séparateur univue** |
|---|---|---|
| Sert à | modéliser | comparer, expliquer |
| Caméras | **une par vue** (face, côté, dessus, perspective) | **une seule** |
| Tourner la vue | ne bouge que celle qu'on manipule | **bouge tout**, il n'y en a qu'une |
| Ce qui diffère | le point de vue | le **mode de rendu** (ou les réglages) |
| Séparateur | une cloison entre panneaux | un **trait de coupe** dans une image |
| Précédent connu | Blender, Maya | comparateur avant/après |

**Multi-vue** — la disposition classique de modélisation : quatre quadrants,
face / côté / dessus / perspective, chacun avec sa caméra et son mode. C'est de
la **mise en page de panneaux**, proche de ce que fait déjà le système de
séparateurs de l'interface.

**Séparateur univue** — décrit ci-dessous. C'est celui auquel Rihen tient le
plus, et le moins courant des deux.

#### Séparateur univue — comparer deux rendus sur la MÊME image

Diviser la vue 3D en deux — ou en N — **non pas pour montrer deux vues
différentes**, mais pour montrer **le même point de vue rendu de deux façons** :
fil de fer contre solide, solide contre rendu, ou deux réglages de rendu
distincts. Un séparateur déplaçable fait glisser la frontière ; plus on le
bouge, plus la découpe est inégale.

**Ce qui fait tout l'intérêt, et qui doit guider l'implémentation :** ce n'est
pas un écran partagé. C'est **une seule image, une seule caméra**. Tourner la
vue fait tourner les deux côtés ensemble, parce qu'il n'y en a qu'une. L'illusion
recherchée est celle d'une image qu'on **découpe** : à gauche elle montre une
chose, à droite une autre, et le séparateur est le trait de coupe.

Conséquences techniques à prévoir :

- **Une seule caméra, un seul état de scène.** Les deux côtés partagent tout sauf
  le mode de rendu (et, à terme, un jeu de réglages). Toute tentation de tenir
  deux caméras est à écarter : elle briserait la promesse.
- **Deux passes de rendu, un seul assemblage**, avec un masque de découpe — c'est
  exactement ce que fait déjà `NkInsetCompose` pour les incrustations, à ceci
  près que la forme est ici un demi-plan mobile. La brique de composition existe.
- **Ça doit sortir en image.** Une comparaison qui ne se capture pas ne sert
  qu'à l'écran ; la pastille Output doit pouvoir la produire, séparateur compris.
- **N côtés, pas seulement deux** — prévoir la généralisation dès la structure de
  données, même si l'interface commence à deux.

Voisin utile : le même mécanisme permettrait un « avant / après » sur un réglage
qu'on modifie, ce qui est le meilleur outil pédagogique pour un tutoriel.

**Les deux peuvent coexister** : une multi-vue dont l'un des quadrants porte lui-
même un séparateur univue. C'est une raison de plus pour ne pas les bâtir sur le
même mécanisme — l'un découpe des **panneaux**, l'autre découpe une **image**.

### Récepteur d'ombre (*shadow catcher*) — demandé par Rihen

Un sol qui **ne se peint pas** mais **reçoit les ombres** : c'est ce qui permet
de détourer un objet sur fond transparent sans qu'il paraisse flotter. Sans lui,
couper le sol emporte l'ombre avec, puisqu'elle est projetée *sur* lui.

**Le sol est un mesh plan ordinaire** avec un matériau standard, rendu par le
PBR — pas de shader dédié. Deux voies, et elles n'ont pas le même coût :

**A. Par différence de rendus** — utilise la machine multi-passes existante,
aucun shader touché :

1. scène **avec** sol, ombres actives → `A`
2. scène **avec** sol, ombres coupées → `B`
3. scène **sans** sol → `C` (déjà produit par le fond transparent)

L'ombre vaut `1 − A/B` là où le sol est visible ; `C` fournit les objets et
leur alpha. On compose l'ombre dessous, les objets dessus. Chaque étape étant
elle-même doublée par la reconstruction d'alpha, cela fait **cinq à six rendus**
pour une image — acceptable pour une image fixe, exclu pour la vidéo.

**B. Par matériau dédié** — un shader de sol qui écrit `couleur = noir` et
`alpha = 1 − visibilité de l'ombre`. Un seul rendu, résultat exact, et
utilisable en vidéo. Mais il faut que le PBR expose la visibilité d'ombre à un
matériau, ce qui n'existe pas aujourd'hui.

**Recommandation** : commencer par **A**, qui donne le résultat tout de suite
sans toucher au moteur, et garder **B** pour quand le chantier « alpha porté par
la chaîne » (voir plus haut) sera engagé — les deux ont besoin de la même
chose : que le rendu sache transporter une couverture.

### Ce qui reste à faire sur Output

Le chantier est **terminé** au 5 août : fond transparent (alpha porté par la
chaîne, plus de double passe), récepteur d'ombre, sept formats d'image, cinq
sorties vidéo dont MP4/H.264, enregistrement de la vue **et** du tutoriel.
Restent :

1. **F12** — c'est le raccourci de rendu chez Blender, mais il est déjà pris ici
   par l'opacité du plan de grille (un vestige de la démo). Je n'ai pas
   réquisitionné le raccourci sans ton accord : à trancher.
2. **Rendu depuis plusieurs caméras vers plusieurs fichiers** plutôt qu'en
   incrustation — un fichier par caméra en une seule commande.
3. **Rendu d'animation** sur la plage d'images : elle est réglable mais sans
   effet tant qu'il n'y a pas de timeline. Le champ reste, il attend sa
   fonction (annoncé comme tel dans le panneau).

## CHANTIER A — l'alpha porté par la chaîne (5 août, compilé, non relancé)

Le fond transparent, le récepteur d'ombre et la vidéo transparente butaient
**au même endroit** : `PP_FXAA/NkSL/pp_fxaa.frag.nksl` et
`PP_Tonemap/NkSL/pp_tonemap.frag.nksl` terminaient par `vec4(rgb, 1.)`. Les
deux corrigés, les trois se débloquent — et la double passe (rendre noir puis
blanc, reconstruire l'alpha) se **désactive d'elle-même** par détection.

- Mesuré : fond alpha **0**, géométrie alpha **255 exact** (245 par
  reconstruction), une seule passe, tous backends.
- FXAA prend l'alpha du pixel **central** : mélanger les alphas des voisins
  étalerait le bord au lieu de le lisser.
- **Récepteur d'ombre** (`NkMaterial::SetShadowCatcher`) : le matériau ne rend
  que la **couverture** de l'ombre, en alpha. L'ambiante entre des deux côtés
  du rapport, sinon l'ombre sort noire et opaque. Vérifié : alpha moyen **206**
  sans ciel, **77** avec le ciel ajouté — c'est la scène qui décide.
- Corrigé au passage dans le moteur : `NkRendererImpl` effaçait la passe
  `DeferredLight` avec une couleur **écrite en dur** — `SetBackgroundColor`
  était sans effet dès que le différé tournait, pour **toute** application.

### Enregistrement — deux prises en parallèle

La vue et le tutoriel partagent la même mécanique (`HostRecStartOn/StopOn/
Enqueue/WaitSlot/EncodeLoopOn`) sur deux instances de `NkVpRec`, et peuvent
tourner **en même temps** : deux points de vue d'une même session, deux noms de
base distincts pour que les fichiers ne s'écrasent pas.

- **Tutoriel en vidéo** : la fenêtre entière, capturée après `EndFrame()` —
  seul instant où elle affiche l'image de *cette* frame.
- **MP4/H.264** ajouté au choix de sortie (l'encodeur muxe lui-même) ; qualité
  1–100 convertie en QP borné 12–48.
- **Qualité vidéo séparée** de la qualité image : elles étaient partagées, si
  bien que soigner un JPEG alourdissait toutes les prises. Le curseur disparaît
  pour la suite d'images PNG, qui est sans perte.
- **Barre d'enregistrement dans le pied de page** : visible seulement pendant
  une prise, elle dit le type, le temps écoulé, les images sautées, et n'offre
  que les trois décisions réelles — Pause / Arrêter / Abandon (en rouge : il
  efface). Trois **icônes**, pas trois libellés : le transport a un dessin
  universel, et `media-pause/play/stop/record` ont été dessinés pour le projet.

### Mise à l'épreuve du 5 août — ce qu'elle a corrigé

- **Le MP4 sortait onze fois trop rapide.** Le journal chiffrait la cause : 86
  images en 39 s pour le `.mp4` (2,2 i/s) contre 765 en 36 s pour le `.avi`
  (21 i/s). L'encodeur H.264 est trop lent pour le temps réel, la file saturait,
  et le temps sauté **disparaissait** au lieu d'être comblé.
  → **Encodage différé** (choix de Rihen) : on filme en images — PNG si la
  transparence est demandée, JPEG sinon — et le MP4 est encodé à l'arrêt, hors
  temps réel, sur son fil. La barre affiche `encodage 312/765` en ambre. Le
  dossier temporaire n'est effacé qu'après un encodage **complet**.
- **Conteneur et codec séparés**, comme Blender : Suite d'images / AVI /
  QuickTime / MPEG-1 / MP4, la liste des codecs suivant le conteneur. Expose au
  passage l'**AVI non compressé**, que NKMedia savait écrire sans que personne
  puisse le choisir — et corrige la suite d'images, **figée en PNG** malgré son
  combo.
- **Deux prises portaient le rang 006** : `mp4` manquait dans la recherche du
  premier rang libre, et le compte du tableau était écrit en dur à `3`.
- **Le curseur est dessiné dans la vidéo de tutoriel** : `PrintWindow` ne
  capture pas le pointeur, d'où des menus qui s'ouvraient tout seuls. Flèche
  blanche bordée de noir + trace des 24 dernières positions, échantillonnée à
  chaque image (à la cadence de capture, elle sauterait). Case dédiée, active
  par défaut, sans effet sur les images fixes.

### Extraire l'enregistrement en bibliothèque (question de Rihen, 5 août — OUI)

Le système tutoriel/capture est extractible vers **NKMedia** en deux briques,
sans dépendre ni de NkCanvas ni de NKRHI (donc consommable par TOUTE
application des deux piles) :
- `media::NkFrameRecorder` — le générique : on lui POUSSE des images
  (l'anneau borné + fil d'encodage + différé QOI + passe finale + pause/
  abandon/progression, aujourd'hui dans `NkVpRec`). L'application fournit les
  pixels, d'où qu'ils viennent (cible NKRHI, framebuffer NkCanvas, webcam).
- `media::NkScreenRecorder` — la fenêtre entière : `NkFrameRecorder` +
  capture OS (`NkCaptureWholeWindow*`, à déplacer de main.cpp vers un module
  qui ne dépend que de NKWindow) + cadence + curseur dessiné.
Le modeleur deviendrait le premier consommateur ; seule la lecture des pixels
de la vue 3D reste chez lui. À faire après la validation du chantier
aimantation.

### Reste sur la vidéo

- **Intervalle d'image clé** (GOP) : réglable dans `NkH264Encoder`, aujourd'hui
  codé en dur à une seconde. À sortir dans le panneau, comme le *Keyframe
  Interval* de Blender.
- **Profondeur 10 bits, vitesse d'encodage, B-frames, piste audio** : Blender
  les propose, notre encodeur ne les gère pas. À ne pas afficher tant qu'ils
  n'existent pas.
- **Accélérer H.264** reste le seul chemin vers un vrai MP4 en temps réel.

## À VÉRIFIER PAR RIHEN À SON RETOUR (compilé, non relancé)

> Pause du 2026-08-04 à 11 h 35. Le modeleur est **fermé** pour rendre le GPU à
> BulkGen (corpus IA : 87 549 / 100 000 paires, cadence divisée par ~15 quand
> les deux tournent). Release et Debug sont à 28/28 ; **rien n'a été poussé** —
> la règle est de valider d'abord.

Quatre points à regarder, dans cet ordre :

1. **Édition proportionnelle en rotation et en échelle** (mode Objet). Elle ne
   propageait que la **translation** ; c'était une limitation que je m'étais
   donnée sans raison, relevée par Rihen. Les trois composantes se propagent
   désormais, atténuées, **autour du pivot figé au début du geste**. Test :
   sélectionner un objet au milieu d'un groupe, rayon large, tourner puis
   agrandir — la rangée doit s'**incurver** et s'**évaser**, pas pivoter sur
   place.
2. **Le NaN — cause trouvée et corrigée dans NKMath.** `NkQuatT::SLerp`
   rendait un quaternion NaN pour **deux quaternions identiques** :
   `NkQuatEpsilon` vaut 1e-12, or en float32 `1.0f - 1e-12f` arrondit
   exactement à `1.0f`, donc le repli NLerp ne se déclenchait jamais et on
   divisait par `sin(acos(1)) = 0`. Interpoler vers une rotation **nulle** —
   le cas le plus banal — contaminait toute la scène. Corrigé par un seuil
   exprimé dans la précision du calcul **plus** une barrière sur `sin θ` juste
   avant la division. Vérifié isolément hors application (`P' = (3.5;0;4)`
   exact, interpolation à 40 % d'une rotation de 30° = 12°). C'est un bug de
   **NKMath**, pas seulement du modeleur : tout code qui SLerp vers une
   rotation identique en souffrait silencieusement.
3. **Garde-fou conservé** : le commit de l'édition proportionnelle refuse
   d'écrire une position ou un quaternion non finis et trace dans le journal
   (`[PropEdit] terme degenere`). La cause est corrigée, mais l'état d'une
   scène ne doit jamais pouvoir être empoisonné sans laisser de trace.
4. **Pastille translucide derrière le gizmo de navigation** (bas à gauche),
   plus dense au survol du corps — elle signale au passage qu'il est
   saisissable. A obligé à ajouter `NkModelerPainter::RingColor` : le trou des
   demi-axes négatifs était rempli avec la couleur **opaque** du fond de vue et
   aurait percé des ronds pleins dans la pastille.

Également livré et non validé : l'**icône aimant** en fer à cheval (le
quadrillage disait « grille », alors que la bascule aimante aussi sur sommets,
arêtes et faces) et l'**icône d'édition proportionnelle** (point plein + anneaux
qui s'affinent : l'influence décroissante est dite par le trait). Chaque bascule
garde son chevron **à côté d'elle**, et le panneau du proportional (rayon + 8
atténuations) suit le patron de celui de l'aimantation : bloquant, ancré à son
chevron, fermé au clic extérieur. Les mêmes réglages sont répétés dans la
pastille **Outil**.

## EN COURS — à reprendre en premier (non validé)

**Orientations Local vs Global.** Rihen constate qu'elles restent identiques.
Deux causes ont déjà été corrigées : l'échelle s'appliquait toujours en axes
monde (corrigée par conjugaison avec la base du geste dans `NkGizmo3D::Apply`),
et le **commit** recomposait la transformation à sa façon (corrigé : il
décompose désormais `ComposedOf(i)`, la matrice réellement affichée).
**Le dernier retour de Rihen dit qu'il reste des erreurs — c'est le point de
départ.** Pistes à vérifier dans l'ordre :
1. le repère est-il bien poussé au gizmo qui bouge (`emptyGizmo` pour les
   nœuds utilisateur — ce gizmo a DÉJÀ été oublié deux fois dans des fan-out) ;
2. `HostDecompose` rend-il des angles cohérents avec la convention d'euler
   utilisée à la soumission (Z*Y*X) ;
3. la rotation et la translation (pas seulement l'échelle) respectent-elles le
   repère au commit.
Rappel de Rihen : **Local et Global ne coïncident que si l'objet est aligné au
monde** ; sur un objet tourné, les deux doivent visiblement différer, pendant
le glissement **et** après le relâchement.

## Reste à faire, dans l'ordre décidé par Rihen

1. ~~**Proportional editing**~~ — livré (sommets ET objets, les 8 atténuations,
   les trois transformations). **En attente de validation**, voir plus haut.
2. **Aimantation, compléments** : cibles *Volume* et *Arête perpendiculaire*
   (affichées « à venir », elles laissent le geste libre) ; base d'aimantation
   (Closest / Center / Median / Active) ; « Aligner la rotation sur la cible ».
3. ~~**Pastille Output**~~ — livrée (source, résolution, presets, pourcentage,
   destination, incrustations, aperçu). **En attente de validation**, voir plus
   haut. Reste la colonne caméra de la hiérarchie.
4. **Matériaux** : textures des autres canaux (rugosité, métallique, normale),
   choix du **type de surface** (`NkMaterialType` : PBR, Toon, Unlit…),
   sauvegarde `.nkasset` via `NkMaterialAsset`/`NkMaterialLibrary` (le format
   existe déjà, ne pas en inventer un), puis l'**éditeur nodal** pour le mixage.
5. **Contenu des espaces** : Édition (ses catégories sont amorcées), puis
   Sculpture 2.5D, Sculpture, Patron (dépliage UV), Texture painting — chacun
   remplit **sa** pastille.
6. **Plus tard** : profondeur de champ, Shift X/Y caméra, presets de nuages
   supplémentaires, matériaux par mesh/dynamiques/de déformation (après les
   fonctions d'édition), suppression de la démo **d'un bloc**.
7. **Icosphère** : elle ignore encore ses subdivisions (TODO moteur dans
   `NkMeshSystem::BuildIcosphereData`) — le curseur est donc sans effet.

## Principe à respecter (décision de Rihen)

Une fonctionnalité — onglet, espace, entrée de menu — **ne naît qu'avec ses
outils** : « les ajouter quand leurs outils naissent leur donnera un contenu
réel dès le premier jour ». Voir `PRINCIPES_CONCEPTION.private.md` à la racine.
Les stubs assumés s'affichent grisés et disent « à venir » — jamais une entrée
qui fait semblant d'agir.

---

# PERSISTANCE — état au 2026-08-08 (point 1 de `PASSATION_2026_08_08.md`)

## Livré, compilé Debug + Release 29/29 — **non validé par Rihen**

**Le document n'est plus l'onglet.** `NkModelerState` porte désormais une table
de **documents** (32 emplacements stables) ; un onglet n'en est qu'une **vue**
(`sceneTabDoc`). Conséquences :

- **Fermer un onglet ne supprime plus rien** (règle absolue de Rihen). Seule
  exception, qui n'en est pas une : un document *transitoire* — maquette
  d'éditeur d'asset, onglet d'isolation — qui n'a jamais été autre chose que la
  vue elle-même.
- Le **navigateur est persisté** : cartes, dossiers, parenté, et les liens
  carte↔document / carte↔nœud source. Il naissait vide à chaque lancement.
- La carte d'une scène naît **avec la scène** (bouton « + »), plus à
  l'enregistrement ; son double-clic **rouvre son document** au lieu d'en
  fabriquer un neuf et vide.
- Section scène **format 2** : `documents`, `navigateur`, `vues` + `vueActive`.
  Le format 1 reste lisible (son `scenes` devient la liste des documents).
- **Récupération des scènes orphelines** : à la relecture, toute scène hôte
  portée par des nœuds mais sans document donne une « Scene recuperee N ». Le
  fichier de Rihen (`MonProjet.nk3dm`) en contient une — c'est du travail
  aujourd'hui inaccessible qui redevient éditable. Le message de chargement le
  dit.
- La boîte « Fermer la scène ? » a **disparu** : elle annonçait une perte qui
  n'existe plus. La pastille « non enregistré » reste (elle parle du projet).

## Les deux tests qui font foi — **à passer par Rihen**

1. Créer 2 scènes → modifier → enregistrer → **fermer l'application** → rouvrir :
   tout doit revenir, **y compris les scènes qui n'étaient pas ouvertes**.
2. Pour chaque type d'asset : l'ouvrir → **fermer son onglet** → il doit rester
   dans le navigateur, intact et réouvrable. Puis enregistrer / fermer / rouvrir.

## Reste du chantier persistance (§4.0 de la passation)

- Le **contenu** des cartes de matériau, texture et graphe n'est pas encore
  écrit : la carte revient avec son nom et sa place, pas avec ce qu'elle porte.
- Persister **chaque type de fichier** dans les **deux** modes (fichier séparé /
  blob dans le `.nk3dm`), un seul code de capture par type.
- **Sélecteur de fichiers personnalisé** sur `NKEditorKit/NkFilePicker.h` —
  `NkDialogs::` est toujours en place et toujours cassé.
- Bascule lié ↔ empaqueté sans perte, dans les deux sens.

# CADRAGE IMPORT (Rihen, 2026-08-09 matin) — à respecter au chantier import

- **Importer un modèle 3D = le DÉCOMPOSER** : le fichier importé (glTF/OBJ/…)
  produit des assets distincts — **mesh (.nkmesh) + matériau(x) (.nkmat) +
  texture(s)** — rangés dans le navigateur, pas un blob monolithique.
- **Importer une texture crée SON fichier spécial configurable** (le futur
  `.nktex` : réglages sRGB/mips/tiling/etc. à côté des pixels sources) — la
  carte Texture gagne enfin un corps, donc sa persistance et sa miniature.
- La **sauvegarde des fichiers texture** fait partie du même chantier.
- L'édition de maillage : validée plus tard par Rihen (« on verra ça bientôt »).
- Ordre : ces points + le sélecteur de fichiers personnalisé forment LE
  prochain chantier, après les « petits problèmes » (bouton SSAO : réglé,
  `b08027c8`).

# NUIT DU 2026-08-09 (carte blanche) — validé par MESURE, à revalider à l'œil

- **Modes de qualité d'ombre branchés** (`9dadf23a`) : PCF3/PCF5/Poisson
  diffèrent enfin (diff pixel 0.006/0.010, bruit 0.000) ; PCSS replié sur
  Poisson et le combo le dit.
- **Vernis + Diffusion au panneau Matériau** (`491fdb9c`) : alimentation par
  drawcall (les deux chemins), .nkmat étendu (vernis/vernisRugosite/diffusion),
  diffusion corrigée (attenuation+ombre+1/π — finie la supernova), vernis avec
  repli ambiance uniforme sans HDRI.
- **Persistance : l'aller-retour est idempotent** — ouvrir → enregistrer →
  relancer → réenregistrer = fichiers identiques octet pour octet (test
  autonome sur copie AgentTest).
- **Mode édition** : s'ouvre et vit sur une scène utilisateur (smoke).
- **Boucle de vérification d'agent** (`744700e5`, `56615b42`) : crochets
  NK_OPEN_RECENT / NK_AGENT_SHOT / NK_AGENT_SAVE / NK_AGENT_EXIT /
  NK_SHADOW_* / NK_MAT_SURFACE + captures + diff pixel. Voir le carnet.

# RENDU — état au 2026-08-09 (l'« acné » est résolue, validée par Rihen)

## Livré et validé à l'écran (commits `d444b7f7` + `ba1d4f41`)

- **SSAO v1 « Alchemy »** (`PP_SSAO/NkSL`) : normales reconstruites, rayon en
  MÈTRES (`ssaoRadius` change d'unité), bruit IGN par pixel, positions monde
  par la matrice réelle de la frame. La v0 (profondeurs brutes sans normale)
  auto-occludait toute face plane vue de biais — c'était le « moiré » du cube.
- **Signe du biais rasterizer de la passe d'ombre inversé** (+64/+4/+0.02) :
  le négatif étendait les ombres (faux « contact parfait ») et fabriquait de
  l'acné proportionnelle à la pente. Validé : **tous les biais du panneau à
  zéro, aucune acné, contact au pied collé**.

## Défauts d'éclairage connus, NON traités (analyse au carnet, code à écrire)

- Garde omni « 6 faces ou aucune » + modes de qualité **jamais lus par le
  shader** (PCF3/5/Poisson = même image ; le travail de la 11e vague a été
  **reverté** dans `12a8fcc8` — à refaire proprement).
- PCSS (durcissement au contact) — remède du résiduel de pénombre.
- Largeur/hauteur de la surfacique jamais transmises au GPU ; atténuation non
  physique `(1-d/portée)²` — loi au choix par lumière (décision Rihen).
- **Clignotement** (parties noires) pendant qu'on ORIENTE une lumière —
  constaté par Rihen le 9 août, non diagnostiqué.

## Piège de méthode payé cette nuit (à connaître)

Deux `jenga build` simultanés dans le même arbre (agent NKAI en parallèle)
corrompent `Build/Obj` : binaires qui crashent absurdement, symptôme qui
**survit au revert du code**. Purger `Build/Obj/<config>` et rebuilder seul.

# PASSATION — SEANCE DU 2026-08-13 (matin)

Branche `refonte-interface-nk3dmodeler`, 3 commits. Release ET Debug verts.

## LIVRE

- **Decoupage** : `NkModelerScreens.h` 14 495 -> 1 476 lignes. Dix fichiers par
  domaine (Properties, Viewport, Hierarchy, Browser, Chrome, Menus, Tables,
  Common). `PaintPropertiesUnified` 6 382 -> 784 : une fonction par pastille.
- **Infobulles** : `NkHelp` + le texte d'aide DANS LA SIGNATURE des widgets
  (DragFloat, Combo, CheckCombo, EditableText). Rendu par `NkTooltip` du kit.
  Barre d'outils equipee (point de passage unique `btn()`). **Reste : toutes les
  autres zones.**
- **Modales** : `NkModalFrameDraw` ajoute a NKEditorKit (cadre modal a CONTENU
  LIBRE, barre de titre + barre de couleur en haut facon NKCode). `NkModalStyle`
  (GitHub dark par defaut) + `NkModalStyleFromTheme` : **le multi-theme est
  pret**. Voile pose UNE SEULE FOIS par pile (`ctx.modalDepth`), selecteur
  compris — empiler sans cacher.
- **`ctx.dlOverlay` est soumise** par le modeleur, et `NkOvPainter()` peint
  dedans. `ui.viewW/viewH` suivent la fenetre.
- **Materiaux** : unicite par RENOMMAGE (`Bois.001`), doublons existants corriges
  a l'ouverture ET reecrits sur disque. Magenta pour objet sans materiau ;
  materiau par defaut a l'import.
- **Projet** : le nom EST un nom de dossier (ni espace ni caractere interdit),
  valide pendant la saisie ; le dossier fait foi.
- **Navigateur** : les dossiers reels du disque sont adoptes (`Apercus`).
- **NKContainers** : `NkSPrintf` / `NkSPrintfN` (tampon fixe) — `NkPrintf`
  existait deja et rend une NkString.

## NON RESOLU — LA BANDE DU HAUT (ce n'est PAS le voile)

**Mesure decisive du 13 aout** : les deux voiles ont ete teintes de couleurs
differentes (modale en ROUGE, selecteur en VERT). Resultat : tout l'ecran vire
au rouge **uniformement** — hierarchie, vue 3D, panneau droit, navigateur. Le
voile couvre donc bien toute la fenetre et fait exactement son travail.

MAIS la bande du haut (y 0..~70, la barre de menu « Fichier / Edition / ... »)
ressort en rouge **VIF ET OPAQUE** alors que tout le reste est translucide. Un
voile semi-transparent qui donne une couleur PURE signifie qu'il n'y a RIEN
dessous : **cette bande n'est pas peinte** des qu'une surface modale est ouverte.

Le « noir » n'etait donc pas un voile trop opaque, c'etait du VIDE assombri par un
voile normal. Cinq hypotheses portaient sur le voile — toutes fausses.

**Piste pour la suite** : `PaintMenuBarI(p, lay.menu, ...)` remplit pourtant son
fond des sa premiere ligne, et `lay.menu = {0, 0, W, menuH}` avec `menuH = S(30)`.
Verifier : (1) que `PaintMenuBarI` est bien atteinte quand `modalOpen` est vrai ;
(2) si cette bande n'est pas plutot la barre de titre PERSONNALISEE de la fenetre,
dessinee hors de `ui.dl` — auquel cas elle serait recouverte par le voile sans
jamais etre repeinte. Mesurer AVANT de corriger.

## RESOLU DEPUIS — LE VOILE

Sous le selecteur, le haut et la gauche de l'application deviennent noirs alors
que la vue 3D, le panneau droit et le navigateur restent lisibles.
**Ecarte par la mesure** : dimensions (`vue=(1920x1032) rendu=(1920x1032)`,
correctes) et superposition de deux voiles (`modalDepth` le montre : les deux
surfaces ne coexistent qu'UNE frame, au clic sur « Nouveau »).
**Reste a mesurer** : le rendu de `dlOverlay` lui-meme — nombre de commandes et
leur rognage. Ne PAS proposer de correctif sans cette mesure (deja 5 hypotheses
fausses sur ce sujet).

## MATERIAUX — ETAT AU 13 AOUT (midi)

Fonctionne et valide par Rihen : magenta a l'absence de materiau, retrait du
dernier, unicite par renommage (`Bois.001`), doublons corriges ET reecrits sur
disque, dossiers reels du navigateur (`Apercus`), creation ecrite sur disque,
materiau ajoute a un objet vide qui en devient l'actif.

**Le magenta « aucun materiau »** occupe le DERNIER emplacement du registre
(`kNkvpMissingMat`), reserve — la creation s'arrete avant lui. Il est invisible
partout d'un seul geste : `Demo3DHostProjMatInfo` le refuse, et
`Demo3DHostProjMatOf` rend -1 pour lui. Il est ASSIGNE au retrait du dernier
materiau, jamais peint au rendu (voir memoire « Agir a la source »).

**DEMANDE EN ATTENTE (Rihen, 13 aout)** : choisir le TYPE du materiau **avant**
sa creation, dans le dialogue. A faire avec les points de specialisation du kit —
`DrawPickerExtra` / `PickerExtraHeight` de `NkFilePicker.h` — que NKCode utilise
deja pour son assistant (rangee « Type : Classe / Struct / Union / … » au-dessus
du champ de nom). Ne PAS ecrire un dialogue parallele.

## SUITE

1. Infobulles sur toutes les zones + migration `snprintf` -> `NkSPrintf`.
2. **Index (cache) des fichiers par type** dans le `.nk3dm` — leve le plafond
   `kMaxBrowser = 32` et supprime la double source de verite navigateur/disque.
3. Supprimer `NkModelerFileDialog.h` (inutilise depuis le portage du picker).
4. Renommer ET decouper `NkDemo3D.cpp` (16 825 l.) + l'API `Demo3DHost*`.
5. Les 7 captures non lues de `Pictures/Screenshots/code`.
6. Point d'entree de renommage de projet dans le launcher.

# PASSATION — NUIT DU 2026-08-12 → REPRISE LE 2026-08-13 À 5 H

## ⚠ LA REPRISE COMMENCE PAR UNE REFONTE COMPLETE DE L'INTERFACE

Decision de Rihen (12 aout au soir) : **refondre toute l'interface et la rendre
propre**. Le **design peut etre different** — rien de l'apparence actuelle n'est
a preserver.

Consequence directe sur ce qui suit : **ne pas rafistoler** la modale « Ajouter
un materiau » ni `NkModelerFileDialog.h`. Ils disparaissent dans la refonte. Le
blocage decrit plus bas reste documente pour la cause, pas pour la reparation.

**Infobulle sur CHAQUE element** (bouton, champ, panneau) — exigence posee par
Rihen d'entree : « mieux vaut y penser tot ». Donc les fonctions qui declarent un
widget prennent leur texte d'aide **des leur signature** ; l'ajouter apres
obligerait a repasser sur chaque appel. Le composant existe deja :
`NKEditorKit/NkEditorTooltip.h` (+ support dans `NKGui`).

**TOUTE MODALE : barre de titre designee (facon NKCode), et modale SUR modale
sans cacher celle du dessous** (Rihen, 13 aout). Composant :
`NKEditorKit/NkEditorModal.h` -- `NkModal` + `NkModalDraw(ctx, m, title, message,
buttons, count)`. Il force `ctx.popupDepth > 0` directement, ce qui rend la
modalite GLOBALE sans toucher au reste. ⚠ Il peint un voile PLEIN ECRAN par
modale : pour empiler « sans cacher », le voile devra etre pose une seule fois
pour la pile.

**Le mot « Demo » doit disparaitre** : `NkDemo3D.cpp` (16 825 l.) et l'API
`Demo3DHost*` sont a renommer ET decouper -- c'est le plus gros fichier du
modeleur, devant l'interface.

**Subdiviser TOUS les gros fichiers, pas seulement l'interface** (Rihen, 13 aout).
Etat au 13 aout : `NkDemo3D.cpp` **16 825 l.**, `NkModelerScreens.h` **14 495 l.**,
`NkViewport3D.cpp` 2 649 l., `main.cpp` 2 032 l., `NkModelerAssets.h` 1 912 l.
NKCode, en comparaison, repartit 35 800 lignes sur 13 fichiers (plus gros : 5 599).

**MOT D'ORDRE — materiaux et edition : toujours se referer a `renderdemo`.**
Cible declaree dans `Applications/Sandbox/RendererSandbox.jenga`, sources dans
`Applications/Sandbox/src/Demo/` (`Demo5_Materials.cpp`, `Demo4_Materials.cpp`,
`Demo3D.cpp` et ses `--demo=<n>`). Ce qui semble manquer au modeleur y fonctionne
souvent deja : regarder AVANT de conclure qu'une fonction est absente.

Base de travail imposee : **NKEditorKit + ce que fait NKCode** (fichiers dedies
par domaine, l'application ne garde que le style et le routage). Ce qui doit
disparaitre : les 14 000 lignes de `NkModelerScreens.h`, les modales peintes
parmi les panneaux, et la coexistence de deux systemes de hit-test.

## Ce qui est LIVRÉ et compile (Release + Debug, 29/29)

- **Sélecteur de fichiers porté depuis NKEditorKit.** `NkFilePicker.h` (celui de
  NKCode) remplace le dialogue maison. Il s'ouvre depuis « Nouveau », confiné à
  la racine du projet, démarrant dans le dossier courant du navigateur.
- **`ctx.dlOverlay` est enfin soumise** (`main.cpp`, après `ui.dl`). Sans elle,
  TOUT composant NKEditorKit dessinait dans le vide.
- **`ui.viewW/viewH` suivent la fenêtre.** `NkGuiContext::Init` les pose une fois
  et ne les revoit jamais : les modales se centraient sur les dimensions du
  démarrage et leur voile s'arrêtait avant les bords.
- **Unicité des noms de matériaux** sur la source de vérité
  (`Demo3DHostProjMatInfo`), plus sur les cartes du navigateur — `kMaxBrowser`
  vaut 32, au-delà un matériau n'a AUCUNE carte et restait invisible du test.
- **Le dossier suit le nom du projet** (`ReconcileFolderWithName`), réparé au
  chargement, et **jamais** au prix du projet : renonce si la place est prise, le
  nom est inutilisable comme dossier, ou `NkDirectory::Move` échoue.
- **Les récents écartent les entrées dont le fichier n'existe plus.**

## LE BLOCAGE EN COURS — modale « Ajouter un materiau »

Symptôme (Rihen) : « je ne peux ni la déplacer ni interagir, les boutons n'ont
pas d'effet », et le clic droit de la vue 3D s'ouvre par-dessus elle.

**Quatre correctifs ont échoué** — tous visaient le mauvais composant ou le
mauvais mécanisme : (1) fermeture au clic extérieur recalculée du dehors —
retirée, NKCode ne le fait pas ; (2) `SetBlock` plein écran — un SECOND mécanisme
d'étanchéité, alors que `modalOpen` existait ; (3) `inView` consultant les
modales ; (4) `LayerScope(hit, 100)` sur la modale. Le journal a montré que le
sélecteur n'était même **jamais ouvert** pendant les tests : c'est bien la modale
maison qui est en cause.

**DÉCISION PRISE AVEC RIHEN, à appliquer à la reprise** : ne pas tenter un
cinquième correctif. **Refaire cette modale sur `NKEditorKit/NkEditorModal.h`**,
qui existe, dessine dans `dlOverlay`, et est déjà étanche. Regarder d'abord
comment NKCode s'en sert : il ne garde que le style et le routage du résultat.

## À RETIRER AVANT TOUT (traces de diagnostic laissées en place)

- `main.cpp` : `[picker] OpenPickerBase demande`
- `NkModelerScreens.h` : `[matadd] clic souris=…` et `[matadd] bouton Nouveau…`
- `NkModelerFileDialog.h` (dialogue maison) est **inutilisé** depuis le portage :
  à supprimer une fois la modale refaite.

## Sauvegarde

Copie des 5 fichiers touchés :
`…/scratchpad/backup_20260813/`. Rien n'est commité ; `git diff` sur
`Applications/NK3DModeler` isole exactement le travail de la nuit (+530/−96).

## Ensuite, dans l'ordre convenu

1. Index (cache) des fichiers **par type** dans le fichier de projet, chemins
   relatifs à la racine — idée de Rihen. Règle d'un coup le plafond des 32
   cartes, la recherche, et l'absence du dossier `Apercus` au navigateur.
2. Point d'entrée de **renommage de projet** dans le launcher (le cœur existe ;
   penser à arrêter `projWatch` qui tient la racine ouverte).
3. Navigateur : suppression réelle d'un matériau (efface le `.nkmat`, délie tous
   les objets, refuse le défaut, avec confirmation).
4. Export `.nkmesh` : matériau embarqué OU fichiers séparés.
5. Les **7 captures non lues** de `Pictures/Screenshots/code` (erreurs visibles
   non décrites par Rihen).
6. Puis étape 2 : **modélisation** (mode maillage + modificateurs).

## Glisser-deposer navigateur -> vue 3D (2026-08-16)

✅ **Le pick d'objet est expose** — `Demo3DHostPickRequest` / `Demo3DHostPickTake`,
motif du jeton (l'interface DEMANDE, la boucle EXECUTE). Le clic de selection
appelle desormais la MEME fonction (`Demo3D_PickEmptyAt`), verifie : meme point,
meme reponse (noeud 101) par les deux chemins.

✅ **Table de Rodolf implementee** — materiau assigne sur un objet / rien dans le
vide · model ajoute **a la position du lacher** dans le vide / **menu enfant ou
independant** sur un objet · toutes les autres natures : refus **nomme**, jamais
un silence.

⚠️ **NON VERIFIE** : le geste complet (glisser depuis une carte, relacher dans la
vue) et le menu. Aucun levier ne fabrique un glisser de souris entre deux
panneaux — ce qui est mesure, c'est le pick et son accord avec le clic. Demande
la main de Rodolf.

### Dette — le commentaire de declaration de `browserKind` ment

`NkModelerInput.h:730` annonce « 0 dossier, 1 materiau, 2 texture ». La vraie
legende (celle que le code CONSOMME, `NkModelerUI.h:531-567`) est : 0 graphe ·
1 dossier · 2 materiau · 3 texture · 4 dataset IA · 5 scene · 6 model · 255
supprimee. Le commentaire a ete corrige, mais la lecon vaut plus que le
correctif : **le point de verite d'un encodage est son consommateur, jamais sa
declaration** — un commentaire de declaration peut mentir des mois sans que rien
ne le contredise.

### ~~Dette — la nature 3 (texture) n'existe dans aucune carte~~ — RECTIFIE

**Cette entree etait FAUSSE, et c'est une mesure qui l'a corrigee** (2026-08-16,
inventaire du navigateur sur `AgentTest`) : la carte 3 s'appelle « Texture » et
porte bien `nature=3`. Le refus nomme des textures est donc **atteignable**.

Ce que disait l'entree d'origine restait vrai dans **son** perimetre —
`NkAsAdoptFile` (`NkModelerAssets.h:1842`) ne reconnait que scene / model /
materiau, et n'affecte jamais `browserKind[...] = 3`. Mais l'adoption d'un
fichier n'est pas le seul chemin de naissance d'une carte : **le gabarit de
projet en cree une**, et ce chemin n'etait pas dans le perimetre cherche. La
conclusion « aucune carte de nature 3 ne peut naitre » depassait donc ce que la
recherche autorisait — le perimetre non enonce, applique cette fois non pas au
relais mais a **ma propre conclusion**.

La correction est laissee en place plutot qu'effacee : une entree de dette qui
disparait sans dire pourquoi n'apprend rien au lecteur suivant.

### ❌ RETRACTE — « LE PICK NE PEUT DESIGNER AUCUN MODEL » (ecrit le 2026-08-16, refute le meme jour)

**Cette entree etait FAUSSE. Je la laisse, barree, parce qu'une conclusion qui
disparait sans dire pourquoi n'apprend rien au lecteur suivant.**

Ce que j'avais ecrit : les deux filtres de `Demo3D_PickEmptyAt` se neutralisent
— le premier ecarte le conteneur en disant *« un model se prend PAR SA
MATIERE »*, le second (`HostHiddenEff`) ecarte sa matiere — donc aucun model
n'est designable.

**Ce que la mesure dit vraiment.** Les 3 noeuds comptes « caches » etaient les
maillages internes des deux models **archives** (`nkvpSceneOf != nkvpCurScene`),
que `HostHiddenEff` ecarte parce qu'ils sont **etrangers au document** — ce qui
est correct. Je l'avais moi-meme note : *la scene d'`AgentTest` ne contient
AUCUNE instance de model*. J'ai donc mesure l'exclusion des ARCHIVES et conclu
sur les INSTANCES.

Mesure sur une vraie instance de model posee dans la scene (projet `AgentTest`,
**Debug ET Release**, arbre `6cc4054c` + correctifs du jour, journal des
candidats du pick) :

```
candidat noeud=105 kind=2 model=1 mesh=0 cache=0 parent=-1    <- le MODEL
candidat noeud=106 kind=2 model=0 mesh=1 cache=0 parent=105   <- sa matiere
PICK lacher -> noeud 105
```

`cache=0` sur les deux : **le second filtre ne les ecarte pas**. La matiere est
candidate, le rayon la touche, et la remontee rend le model.

**Le controle qui aurait du me sauver, et qui devient la regle** : la boucle de
rendu des objets utilisateur applique **exactement la meme paire de filtres** que
le pick (`nkvpIsModel` puis `HostHiddenEff`). Donc *ce qui se voit se designe*,
par construction. Une conclusion « le pick ne voit pas X » qui n'explique pas
pourquoi X est pourtant VISIBLE a l'ecran est fausse avant meme d'etre mesuree.

Lecon, troisieme fois en deux jours : **le perimetre s'ecrit a cote du
resultat.** « 3 caches » sans « sur des archives » n'est pas un chiffre, c'est un
piege qu'on se tend a soi-meme.

### La regle de selection est ECRITE — et elle etait deja codee (2026-08-16)

Decision de Rodolf : le discriminant n'est pas « est-ce un model ? » mais **ce
que la piece cliquee EST dans la hierarchie**.

| ce qu'on clique | ce qui est selectionne |
|---|---|
| un **noeud enfant** (objet ou model a part entiere) | **ce noeud-la**, pas son parent |
| un **sous-maillage interne** d'un model | **le model entier** |

`Demo3DHostModelRootOf` fait exactement cela : il remonte **tant que le noeud est
un `nkvpIsMesh`** (une donnee geometrique, pas un noeud de hierarchie) et rend le
premier ancetre qui n'en est pas un. Un noeud enfant ordinaire porte
`nkvpIsMesh=false` et se rend donc **lui-meme** des le premier tour. Aucun code
n'a eu a changer : ce qui manquait n'etait pas la regle, c'etait **une instance
de model dans la scene** pour la voir s'exercer — et il n'en naissait aucune, a
cause du defaut de duplication ci-dessous.

⚠️ **Moitie manquante, mesuree.** Le mode edition **existe** (`st->editMode`,
bascule TAB, `Demo3D_EnterEditOnObject`), mais il n'accepte que
`st->gizmo.ActiveIndex()`, c'est-a-dire les objets de demo d'indice `< kNumObj`.
**Les noeuds utilisateur (>= 96), ou vivent TOUS les models et leurs maillages
internes, ne peuvent pas y entrer.** La moitie « prendre un sous-mesh
individuellement en edit mode » **n'est donc pas atteignable aujourd'hui** ;
seule la moitie « le sous-mesh selectionne le model » l'est. A ouvrir quand
l'edition acceptera un noeud utilisateur.

### ✅ Corrige — DUPLIQUER UN MODEL N'EMPORTAIT PAS SA MATIERE (2026-08-16)

`HostSpawnLike` l'ecrivait lui-meme : *« Un double de MODEL naitrait vide : il
redevient donc un objet ordinaire »*, et posait `nkvpIsModel[n] = false`. Or un
conteneur de model **ne rend rien par lui-meme** : degrader le double ne
produisait pas un model vide, ca produisait **un objet qui n'etait pas celui
qu'on avait glisse**.

Decision de Rodolf : **partage par defaut, avec choix utilisateur.**

- `HostDuplicateTree` emporte la matiere du model par **le meme parcours
  d'appartenance** que le deplacement de document et l'archivage
  (`HostIsInnerMeshOf`), et recable la parente sur la carte complete.
- Les maillages sont **partages** (meme `NkMeshHandle`) : instantane, sans cout
  memoire, ce que veulent array / jeu / film.
- **Ctrl+Maj+D**, et l'entree « Dupliquer independant » du menu contextuel de la
  hierarchie, font une **copie independante** de la geometrie.
- `Demo3DHostArchiveNode` emporte la matiere lui aussi et **archive tout le
  sous-arbre** : ne marquer que la racine aurait laisse les maillages de
  l'archive vivants dans la scene courante.

Mesure (projet `AgentTest`, **Debug ET Release — resultats identiques au
chiffre**, lacher d'une carte de model dans le vide) :

```
MESURE pose   : noeud=105 demande=(1.20146, 0, 2.67668)
                relu=1    (1.20146, 0, 2.67668)   model=1     <- etait model=0
MESURE enfant : noeud=106 mesh=1 relu=1 (0, 0.53886, 0)       <- n'existait pas
source 96, sa matiere 97 a (0, 0.53886, 0)                    <- transform LOCALE respectee
```

⚠️ **Ce que la copie independante ne change pas encore, et pourquoi.**
`HostMakeGeometryOwn` ne detache que les noeuds portant leur **propre** maillage
(`nkvpUserMesh` valide). Un noeud sur primitive partagee tire deja son
independance de ses **parametres** (sub/segments/anneaux/aux), que
`HostSpawnLike` copie un a un. Et comme l'edition de sommets n'accepte pas les
noeuds utilisateur (voir plus haut), **partage et copie independante ne
produisent aujourd'hui aucune difference observable**. Le choix est plombe de
bout en bout pour que la semantique soit deja la bonne le jour ou l'edition par
noeud arrivera — pas parce qu'il se voit maintenant. **Non verifie a l'ecran.**

### ⚠️ Defaut — NK3DModeler **Release** PLANTE A LA FERMETURE (2026-08-16, mesure)

**Pre-existant : reproduit sur l'arbre `6cc4054c` SANS aucune de mes
modifications** (mesure faite en remisant mon travail, puis reconstruction).

| configuration | construction | execution |
|---|---|---|
| Debug | ✅ | ✅ 3 lancements sur 3, code de sortie 0 |
| Release | ✅ | ❌ 3 sur 3, `0xC0000005` dans `d3d11.dll` |

Ce n'est **pas** un plantage au demarrage : le journal `logs/app.log` (qui, lui,
survit — la sortie standard est perdue avec le tampon) montre l'application
faire tout son travail, puis :

```
[Demo3D] Shutdown
[NkMaterialLibrary] Shutdown
[NkRHI_DX11] Shutdown          <- derniere ligne, puis 0xC0000005 dans d3d11.dll
```

**Le plantage est dans la DEMOLITION du peripherique DX11.** Un `CreateBuffer
hr=0x887A0005` (`DXGI_ERROR_DEVICE_REMOVED`) a aussi ete observe une fois en
Debug, ce qui suggere la meme zone.

⚠️ **Non repare** : un ecart Debug/Release vient presque toujours d'un `assert`
desactive, d'une initialisation absente que le Debug masque, ou d'une
optimisation qui expose un comportement indefini. Les trois se **diagnostiquent**
avant de se corriger, et c'est NKRHI, pas NK3DModeler.

**Consequence de methode, et elle est genante** : `--backend=vulkan` et ses
freres sont **ignores** par NK3DModeler (`ParseBackend` n'est pas appele — le
journal dit `api=DirectX 11` quoi qu'on passe). Il n'y a donc **aucun moyen
d'eviter DX11** pour contourner. Toutes mes mesures Release passent par
`logs/app.log`, pas par la sortie standard.

⚠️ Et une correction de provenance : mes mesures du 2026-08-16 annoncees
« binaire Release » ont en realite ete prises en **Debug** — le Release ne peut
pas rendre sa sortie standard. **Un chiffre porte sa configuration**, au meme
titre que sa date et son commit.

### Dette — les leviers d'agent ne disent pas QUAND

`NK_SEL_AT` se declenchait au premier passage, avant que `NK_OPEN_RECENT` n'ait
ouvert le projet (frame 3) : il mesurait une scene vide et repondait « rien ».
Corrige par une troisieme valeur facultative (la frame), et `NK_DROP_AT` nait
avec. **Tout levier one-shot qui coexiste avec une ouverture differee a le meme
defaut** — les autres n'ont pas ete audites.
