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

## 🎨 CHANGER LE TYPE D'UN MATÉRIAU RÉINITIALISE TOUT — livré (agent nk3dmodeler, 2026-08-22, `849a7e5e`)

Rodolf : « si je modifie le type de material d'un model sans changer le material,
le nouveau type hérite des propriétés communes […] pourtant ça devait faire comme
si on avait reset ce dernier aux valeurs par défaut du nouveau type. »

⚠️ **Il l'avait déjà signalé le 14 août.** La correction d'alors était deux `if`
écrits à la main (verre → `alpha`, émissif → `emissive`). **Une liste, pas une
règle** — et une liste ne couvre que ce à quoi on a pensé.

> 🎯 **LA LEÇON, ET ELLE DÉPASSE CE FICHIER** : une correction par **énumération
> de cas** se périme à chaque champ ajouté, et elle se périme **silencieusement**.
> Même motif que le membre `name` déclaré quatre fois : le correctif tenait dans
> un commentaire que rien n'obligeait à relire. **Le remède n'est pas une
> meilleure liste, c'est un contrôle qui parcourt les champs.**

**Mesure de la fuite** (cas témoin `type/ancienne-regle-fuit`) : après un
changement verre → Standard sous l'ancienne règle, **22 champs sur 23** gardaient
encore leur ancienne valeur.

**Second défaut, corrigé aussi** : le défaut du nouveau type n'était appliqué que
si la valeur n'avait jamais été touchée (`if type == 5 && alpha >= 0.999f`). **Le
même geste donnait deux résultats selon un historique invisible.**

### Ce qui est livré

- `Viewport/NkVpMatTypeDefaults.h` — **un seul point d'accès**,
  `NkVpMatTypeDefaultsFor(type) -> NkVpMatParams`, et de la **donnée** derrière.
- Le changement de type applique la ligne du nouveau type **entière et sans
  condition**, et **vide les chemins de texture** (« on doit tout vider même les
  texture, c'est comme ça que fait blender », Rodolf, 22 août).
- `Applications/NKMatTypeResetTest` — banc **console, sans fenêtre ni device**,
  11 cas.

### 🛡️ LES DEUX GARDE-FOUS — c'est eux qui empêchent la 3ᵉ occurrence

1. **Contrôle de couverture** (`type/couverture-des-champs`) : la somme des
   tailles des descripteurs doit valoir **exactement** `sizeof(NkVpMatParams)`,
   et les descripteurs doivent être **contigus**. Un champ ajouté sans entrée →
   **banc rouge**. *Vérifié en ajoutant réellement un champ : `somme=132
   sizeof=136`, FAIL. Un garde-fou qu'on n'a pas vu échouer ne garde rien.*
   (C'est pour ça que `emiEclaire` est un `int32` et non un `bool` : sans octet
   de bourrage, le contrôle est exact au lieu d'être tolérant.)
2. **`static_assert(sizeof(NkVpProjMat) == 1480)`** à côté de la recopie, dans
   `NkDemo3D.cpp` : si la struct du modeleur grandit, **le build casse**. Un
   échec de build ne peut pas être ignoré, contrairement à un commentaire.

### Ce qui reste, et ce qui est assumé

- ⚠️ **`A → B → A` ne rend pas les valeurs d'origine** mais les défauts de `A`.
  **Conséquence assumée de la règle**, écrite dans le banc pour que personne ne la
  prenne un jour pour un bug et ne « répare » en réintroduisant une mémoire.
- **Seuls le verre et l'émissif ont une ligne propre** — ce sont les deux seuls
  préréglages qui existaient dans le code. Peau, cheveux, tissu, carrosserie,
  feuillage, eau **retombent sur la base entière** (donc rien de l'ancien ne
  survit). Leur donner des valeurs physiques est une **décision produit**, pas une
  correction de défaut : à trancher par Rodolf. Ajouter une ligne est un `case`.
- **La cible visée par Rodolf reste le pilotage par le SHADER, sans aucun `if`.**
  NkSL ne peut pas encore le porter : `NkSLReflection` rend les blocs uniformes,
  leurs bindings et leur taille, **jamais leurs membres**, et aucune valeur par
  défaut. La table est donc écrite pour être **remplaçable** : le jour où NkSL
  réfléchira les membres, **seul le corps de `NkVpMatTypeDefaultsFor` change, et
  aucun appelant ne s'en aperçoit.**

### 🧾 Dette de méthode nommée : le banc ne couvre pas la recopie

`Demo3DHostProjMatSetType` vit dans `NkDemo3D.cpp` (18 636 lignes, device requis)
et **n'est pas linkable depuis une console** — c'est le mur des 118 symboles
`Demo3DHost*` qui a fait retirer `NkMatInventaireTest` du workspace le 17/08. Le
banc exerce donc la **règle**, pas la recopie dans `NkVpProjMat` ; c'est le
`static_assert` qui garde celle-ci. **Le jour où `Demo3DHost` devient une
bibliothèque, le banc peut couvrir les deux.**

---

## 📐 Modes objet / édition, et ce qu'est un sous-mesh — SPÉCIFICATION (Rihen, 2026-08-17)

*Écrite ici parce qu'elle n'existait que dans un fichier d'échange non versionné.
C'est la première fois que le comportement objet/édition est fixé noir sur blanc.*

### ⚠️ COMPLÉMENT DU 2026-08-27 (Rodolf) — LA RÈGLE DES MODES, qui dépasse l'édition

> **« Concernant le mode édition, ça doit se faire dans l'onglet Édition
> uniquement, pour l'objet sélectionné. Pareil pour la sculpture, la sculpture
> 2.5D, le texturing, etc. »**

**Deux principes :**
1. **Un mode vit dans SON onglet** — ce n'est pas un état global de l'application.
2. **Un mode s'applique à l'OBJET SÉLECTIONNÉ**, pas à un index parallèle ni à une
   cible implicite.

**Et la forme, corrigée par Rodolf lui-même** — *« Comment ça, copier ? C'est pas
plus lourd ? »* :

```
UNE machinerie     entrer / sortir · ce qui est permis · ce que la vue affiche
                   · LA CIBLE = L'OBJET SÉLECTIONNÉ
N descripteurs     Objet · Édition · Sculpture 2.5D · Sculpture · Texturing · …
                   chacun déclare SES outils et SES panneaux
```

⚠️ **Ce n'est PAS un gabarit à instancier.** Un gabarit invite à copier, et copier
est le défaut qu'on retire : trois doublons trouvés dans cette application le
27/08, chacun avec un côté mort et l'entrée publique branchée dessus. **Blender
n'a pas cinq implémentations de « mode », il en a une et une énumération.**

**État mesuré au 27/08 — la machinerie existe déjà, il lui manque UN maillon :**

| pièce | état |
|---|---|
| l'énumération `NkMode` (Object, Edit, Sculpt25D, Sculpt, Texturing, Patron, TexturePaint) | ✅ existe |
| l'onglet écrit le mode (`NkModelerViewport.h:675`) | ✅ existe |
| la cible suit la sélection (`editUserIdx` posé depuis la cible résolue) | ✅ vérifié |
| **le mode atteint le viseur vivant** | ❌ **manquant** |

**Le maillon manquant, précisément** : `main.cpp:1325` fait
`Viewport3DSetEditMode(st.mode != NkMode::Object)` — il **replie sept modes en un
booléen** *et* l'envoie à la vue **dormante**. C'est là, et seulement là, que la
machinerie générique se perd. La réparer, c'est un `Demo3DHostSetMode(int32)` qui
prend le MODE et non un booléen. **Ne pas déclarer de mode qui n'existe pas** :
seuls Objet et Édition sont réels aujourd'hui ; les autres sont déjà dans
l'énumération et n'ont pas besoin d'être annoncés ailleurs.

### ⚠️ SCULPTURE ET SCULPTURE 2.5D SONT DEUX MODES, PAS UN MODE ET SON OPTION

*Rodolf, 27/08 : « N'oublie pas la distinction entre sculpture et sculpture 2.5D. »*

| | ce qui change | ce que ça exige |
|---|---|---|
| **Sculpture 2.5D** | un déplacement le long de la normale, sur la surface existante | **aucun changement de topologie** — pas de contre-dépouille, pas de surplomb |
| **Sculpture réelle** | le volume lui-même | **topologie dynamique** : remaillage, ajout/retrait de géométrie, multi-résolution |

📌 **Pourquoi c'est écrit ici et pas ailleurs** : deux noms proches sur deux
exigences **opposées**, c'est exactement ce qui se fait fusionner par quelqu'un de
bien intentionné qui croit simplifier.

📌 **Et ça relie la réécriture demi-arête de la semaine** (cycles chaînés,
`LinkTwins` localisé, opérations en place) : **c'est l'infrastructure de la
sculpture RÉELLE.** La 2.5D n'en a pas besoin ; la réelle ne peut pas exister
sans.

⚠️ **Conséquence sur le descripteur de mode** : il devra pouvoir porter *« ce mode
change-t-il la topologie, ou seulement les positions ? »*. **Contrainte de
NON-FERMETURE, pas fonctionnalité à écrire aujourd'hui** — tant qu'un seul mode
en a besoin, ce n'est pas encore une abstraction.

### Les deux modes

| | mode **OBJET** | mode **ÉDITION** |
|---|---|---|
| un clic sélectionne | **le model entier**, tous ses sous-mesh avec | **un sous-mesh**, ou des faces |
| on édite | la transformation, et la **liste de matériaux** | la géométrie, et **quelle partie porte quel matériau** |
| granularité du matériau | l'emplacement dans la liste | **une partie ou tout un sous-mesh** |
| séparation | — | **retirer un sous-mesh → il devient un model, et disparaît de l'original** |

> « Un model c'est un assemblage de vertices, edges et faces reliés ou non entre
> eux. Des mesh d'un model peuvent porter des matériaux différents ou le même. »

#### ✅ LIVRÉ (17/08) — `937a0c66`, `2093e783`, `bac51a20`

- **`L` / `Ctrl+L`** : `ComputeConnectedComponents` dans **NkEditMesh** (pas dans
  l'application — `P` la partagera). Parcours sur l'**identité soudée** :
  un cube importé duplique ses sommets par face, et une connexité par indices
  bruts y verrait **six** îlots au lieu d'un. Banc : cube = 1 composante,
  deux cubes = 2, somme = nombre de sommets, zéro orphelin.
  *Écart assumé* : la graine est le sommet **actif**, pas le survol — il n'existe
  aucun survol par sommet dans cette vue.
- **Mode objet** : cliquer la matière sélectionne le **MODEL**, via
  `Demo3DHostModelRootOf` (qui **existait déjà** — le liséré s'en sert vingt
  lignes plus bas). Uniquement le pick de la vue 3D, déjà sous `!editMode`.

⚠️ **CE QU'IL NE FAUT PAS « AMÉLIORER » PLUS TARD — et la raison.**
Il est tentant d'ajouter les maillages à la sélection du gizmo pour que
« sélectionné avec tous ses sous-mesh » soit littéral. `AddToSelection` existe et
ça *marcherait* en apparence. **Ne le faites pas** : le gizmo calcule **une
transformation par cible sélectionnée**, et `HostHierRecurse` propage **déjà**
celle du parent à ses enfants — le geste serait appliqué **deux fois**.
Le liséré couvre déjà toute la matière (« la zone d'un model, c'est sa
matière ») : **l'exigence est satisfaite visuellement, sans toucher une seule
transformation.**

**Non mesuré** : la remontée elle-même n'a pas de banc (il faudrait un projet
chargé et des coordonnées écran). La trace `NK_SEL_AT` dit désormais la **nature**
du nœud sélectionné (`model` / `maillage` / `vide`) et non plus seulement son
numéro — c'est ce qui permet de la constater en un geste.

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

### ✅ LE TROISIÈME ACTEUR — MESURÉ ET CORRIGÉ (`c863dc00`, 17/08, journal 00:29)

**Clos.** La mesure de Rihen a établi les deux défauts prédits, et le correctif
est posé — voir « RÉGRESSION SIGNALÉE » plus bas pour le diagnostic complet, la
règle (*une écriture programmatique n'est pas un geste*) et la grille de la 7e
relecture. Le texte ci-dessous est l'analyse d'avant-mesure, gardée telle
quelle : ses deux hypothèses se sont réalisées mot pour mot.

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

### ✅ LA MESURE A PARLÉ (journal 00:29, 17/08) — deux défauts, corrigés (`c863dc00`)

**`ECART != 0` — mais la cause n'était PAS le recentrage.** Le journal (figé
avant analyse ; il contenait **trois courses de binaires différents**, à savoir
pour le relire) a établi :

1. **Le recentrage est déterministe et innocent** : `après=(2.22311, −0.315482)`
   identique dans deux courses pour la même entrée 2.935. Le « avant » revient à
   2.935 à chaque lancement parce que **le disque n'est jamais réécrit** —
   l'archive recentrée n'est pas sauvée (dette ouverte, voir ci-dessous).
2. **`hier model=98 … dp=(−0.712, 0, −0.846) transmis=1`** : la passe de
   hiérarchie lisait le déplacement d'origine comme un **geste** et donnait le
   même delta à la **matière de l'archive** — effet du recentrage annulé,
   archive entière en dérive à chaque dépôt. La non-idempotence *observée*
   venait de là : la matière ayant fui, le barycentre suivant différait.
3. **`hier model=107 cliché=(0,0,0) dp=(position complète) transmis=1`** : le
   cliché d'un nouveau-né est **celui du mort** qui occupait l'emplacement
   (`HostAllocUser` ne l'écrit jamais). La copie, posée juste
   (`cumul ECART=0`), recevait **toute sa position une seconde fois**.

**La règle, et le remède existait déjà** : *une écriture programmatique n'est
pas un geste.* C'est le principe de `Demo3DHostHierarchyResync` (« après un
chargement, cet écart n'est pas un geste »), appliqué à UN nœud
(`HostHierSnapNode`) chez chaque écrivain programmatique du chemin de dépôt —
`HostDuplicateTree` (nouveau-nés, y compris le retour anticipé non-model),
`Demo3DHostRecenterModel` (l'origine **seule** bouge),
`Demo3DHostSetModelTransform` (la pose emporte déjà la matière). **Les chemins
interactifs (gizmo, panneau) ne recalent rien** : leur propagation est voulue.

**Grille de la 7e relecture, écrite d'avance** : 1er dépôt → `origine ECART≠0`
une fois (héritage disque), `cumul ECART~0`, **aucune** ligne `hier` ; drag
gizmo → lignes `hier` **présentes** (témoin que la propagation voulue vit) ;
2e dépôt → `origine ECART=(0,0)` — l'idempotence enfin observable.

**✅ VALIDÉE le 17/08 (7e relecture, course 00:55, journal figé)** — les trois
lignes confirmées avec citations : l. 483452 (`ECART=(-0.712, -0.846)`, une
fois) ; zéro `hier` parasite (compte exhaustif `model=98` : 0 ; 2,8 s entre le
dépôt et le premier `hier`, qui est un drag) ; l. 483484+ (drag : dp continus,
la propagation voulue vit) ; l. 483692 (`ECART=(0, 0)` au 2e dépôt —
l'idempotence observée). Verdict de Rihen : *« le résultat est correct cette
fois, même sans que je ne bouge. »* **Le geste dépôt/déplacement est clos ;
l'éclatement se débloque.**

**✅ Dette de persistance FERMÉE (`67a60328`, décision de Rodolf 17/08)** :
*« conserver son origine dans le fichier du mesh »* — l'origine stockée est la
**référence du pipeline d'export** (FBX repositionnera l'objet pour que son
origine soit à (0,0,0) ; une origine fausse dans le fichier = un export décalé).
Mécanique : le recentrage renvoie s'il a corrigé → la carte est marquée
(`browserOriginDirty`, transient, jamais sérialisé) → la **sauvegarde** écrit
son `.nkmesh` via l'exemption qui existait déjà pour les matériaux, puis
désarme après l'écriture réussie. **Aucune écriture disque au dépôt**
(condition approuvée par Rodolf). Aucun changement de schéma,
`kAssetFormatVersion` intact, aucune migration — un `.nkmesh` ancien se charge
tel quel, le recentrage mémoire le rattrape. Protocole de la 8e relecture,
écrit d'avance : **positif** = déposer→sauvegarder→relancer→redéposer donne
`origine ECART=(0,0)` dès le premier dépôt de la session ; **négatif** = sans
sauvegarde, l'écart d'héritage revient une fois — preuve que rien ne s'écrit
sans geste. Les `.nkmesh` de l'éclatement naîtront justes sans rien d'autre :
son plan recentre avant l'archivage et la capture lit les nœuds vivants.

**✅ VALIDÉE PAR LA 8e RELECTURE (Rihen, 17/08 : « tout est okay »)** — test A
(sauvegarde → relance → écart nul d'emblée) et test B (sans sauvegarde →
l'écart d'héritage revient une fois) confirmés tous les deux. **La persistance
de l'origine est close, vérifiée des deux côtés** — témoin négatif compris :
rien ne s'écrit sans geste de sauvegarde.

**Dette ouverte restante** : `nkvpXmit=7` dès la naissance reste une valeur par
défaut jamais discutée.

### 🔓 L'ÉCLATEMENT D'UN IMPORT — DÉBLOQUÉ (17/08), terrain mesuré, plan posé

*Débloqué par la validation de la grille (7e relecture). Les deux cas nommés par
Rihen : (a) les models distincts d'un fichier importé, **chacun dans son
fichier** ; (b) un même fichier qui contient **plusieurs sous-mesh** — deux cas
différents (R40/R41 : la frontière entre fichiers est le MODEL, déclaré par le
fichier ; les sous-mesh restent dedans).*

**Le terrain, mesuré le 17/08 — chaque fait avec sa preuve :**

| brique | état |
|---|---|
| chargeurs (`LoadOBJ`/`LoadGLTF`/`LoadFBX`) | ✅ `sm.name` rempli (glTF, OBJ), coupe sur `o` (2→10 mesuré) ; FBX nomme mais frontière à confirmer |
| écriture d'un fichier de model | ✅ `NkAsModelCapture` (`.nkmesh` = racine + SES maillages, parcours officiel `Demo3DHostNodeInnerMeshOf`) |
| naissance assainie | ✅ `HostHierSnapNode` — tout nouveau-né recale son cliché ; s'applique aux nœuds de l'éclatement comme à ceux du dépôt |
| nœud à maillage arbitraire | ✅ par pièces : `nkvpUserMesh[u] = ms->Create*(…)` (primitives) et `ms->Create(d)` depuis des données (mode édition) — pas encore de fonction « nœud depuis une tranche de sous-mesh » |
| bouton « Importer » du navigateur | ❌ **peint, JAMAIS lu** — `hit.Add("brw.imp")` sans aucun `hit.Clicked("brw.imp")` (témoin : `brw.creer` est bien consommé). Un bouton mort depuis sa naissance |
| lâcher de fichier OS | ❌ `NkDropFileEvent` existe dans NKEvent, **aucune écoute** dans l'app |

**Le plan, dans l'ordre :**
1. **Brancher « Importer »** → sélecteur de fichiers (NKEditorKit) filtré sur les
   formats des chargeurs. Le lâcher OS viendra après, c'est une seconde entrée
   vers le même chemin — un seul chemin d'import, deux portes.
2. **Charger** → `NkGLTFMeshData` (sous-mesh nommés + `subMeshMaterial`).
3. **Regrouper** : sous-mesh consécutifs de même `name` = **un model** (la
   frontière déclarée) ; dedans, chaque sous-mesh = un emplacement de matériau.
   Fichier sans marqueur → un seul model (repli décidé en R41).
4. **Par model** : racine + un nœud maillage par sous-mesh (géométrie
   `ms->Create` sur la tranche d'indices, keepCPU), positions **absolues**,
   `HostHierSnapNode` sur chaque nouveau-né, recentrage d'origine (règle du
   16/08), archivage, carte navigateur, écriture `.nkmesh` — **un fichier par
   model**, règle d'import de `CONVENTIONS_FICHIERS.md` (copier en gardant
   l'origine).
5. **Hors du premier lot, dit explicitement** : les matériaux du fichier importé
   (→ `.nkmat`), le lâcher OS, la confirmation FBX multi-objets.

**État au 17/08 — points 1-3, moitié ANALYSE livrée** (`NkModelerImport.h`) :
le bouton est branché (`hit.Clicked("brw.imp")` → picker → `pickerAction=2`),
le fichier choisi est chargé par LE chargeur de son extension (7 formats) et
découpé par nom contigu (`NkImportSplitByName`), le tout journalisé
(`MESURE import`) et résumé dans `hierNote`. **Écart assumé au plan** : le
picker du kit n'a qu'un filtre MONO-extension (`pickerFileExt`), donc
ouverture **sans filtre** et validation à la confirmation avec **refus
nommé** (« Format non reconnu : … ») — un refus silencieux serait
indistinguable d'un bouton cassé.

**Grille du point 4 (CRÉATION des nœuds), écrite d'avance — consommateurs lus
le 17/08 :**

1. **Positions ABSOLUES, tranché par lecture** : les transforms du système
   sont MONDE (`nkvpEmptyPos`, commentaire l. 159-174 — le rendu ne compose
   jamais parent×enfant ; la parenté n'est qu'appartenance). Le modèle à
   suivre est `HostDuplicateTree` (matière à `world + offset`, l. 16173) ;
   **ne PAS imiter `EnsureModelMesh`**, qui écrase en (0,0,0) *monde* après
   un `HostSpawnLike` ayant posé la position monde — c'est le piège
   `nkvpEmptyPos` nommé, confirmé à la lecture.
2. **Naissance** : aucun chemin public « nœud depuis des données » n'existe —
   à créer côté hôte (`NkDemo3D.cpp`) : `HostAllocUser` +
   `ms->Create(NkMeshDesc::Simple(Default3D, tranche))` avec
   **`keepCPU=true` explicite** (défaut `false` — sans lui ni archivage
   relisible ni copie indépendante, cf. `HostMakeGeometryOwn`), puis
   `nkvpIsMesh[m]=true`, `nkvpParentOf[m]=root`, `nkvpIsModel[root]=true`.
3. **Tranches** : lire la structure exacte des sous-mesh de `NkGLTFMeshData`
   (les indices d'une tranche référencent-ils le buffer global ? rebaser ou
   passer les sommets utiles) — à mesurer avant d'écrire.
4. `HostHierSnapNode` sur **chaque** nouveau-né ; recentrage d'origine AVANT
   l'archivage (règle du 16/08) ; `NkAsModelCapture` avec le parcours
   officiel `Demo3DHostNodeInnerMeshOf` — un `.nkmesh` par model, origine
   déjà juste (persistance close, aucune dette d'héritage).
5. **Témoins** : (+) un `.obj` 2 objets → 2 `.nkmesh`, réouverture
   indépendante = bonne origine, `ECART=(0, 0)` d'emblée ; comptes
   verts/indices du fichier retrouvés sur les nœuds ; (−) fichier sans
   marqueur → UN model (R41), et rien n'est écrit au projet sans geste de
   sauvegarde.

**Point 4 LIVRÉ (17/08, soir) — la grille confrontée ligne par ligne :**

1. **Positions ABSOLUES — suivie par construction** :
   `Demo3DHostCreateModelRoot`/`Demo3DHostCreateMeshNode` écrivent
   `nkvpEmptyPos` en MONDE ; `EnsureModelMesh` n'est imité nulle part.
2. **Naissance — l'API hôte existe** (NkDemo3D.cpp), au mot près de la
   grille : `HostAllocUser` + `NkMeshDesc::Simple(Default3D, tranche)` +
   **`keepCPU = true` explicite** (posé même si `Simple()` l'active :
   un futur changement de défaut ne doit pas retirer la garantie), puis
   `nkvpIsMesh/nkvpParentOf/nkvpIsModel`. Racine = EMPTY (kind 4) : un
   conteneur ne rend rien, une nature géométrique mentirait au panneau.
3. **Tranches — MESURÉ, et les deux chargeurs diffèrent** : glTF écrit des
   indices LOCAUX à la primitive + `baseVertex` (NkGLTFLoader.cpp:1069) ;
   OBJ des indices GLOBAUX + `baseVertex=0` (NkOBJLoader.cpp:199). Lecture
   unique : `global = indices[firstIndex+i] + baseVertex`. On EXTRAIT les
   sommets utiles et on REBASE 0..n-1 — passer le buffer entier
   dupliquerait tout le fichier dans chaque model. Sommets rebasés autour
   de l'ANCRE de la tranche (centre de boîte) ; le nœud naît À l'ancre :
   même image, origine SUR la matière.
4. **Fait** : `HostHierSnapNode` dans les DEUX fonctions de naissance ;
   racine née au barycentre X/Z de ses ancres (la moyenne exacte que
   `Demo3DHostRecenterModel` recalcule) → la ligne `MESURE origine` de
   l'archivage doit dire `ECART=(0, 0)` dès la naissance ; archive + carte
   par le MÊME chemin que le glisser hiérarchie → navigateur ; carte
   marquée `browserOriginDirty` (son consommateur dit « à écrire au
   prochain enregistrement, même partiel » — une carte sans fichier est
   exactement cela). Les noms traversent l'archivage : appariement k-ième
   source / k-ième double, garanti par la monotonie de `HostAllocUser`
   (plus petit slot libre d'un ensemble qui ne fait que rétrécir).
5. **Témoins** : la 9e relecture a ÉCHOUÉ (mesh dupliqué N fois, models
   manquants) — cause trouvée, corrigée et rejouée le 17/08 au soir, voir
   « ÉCHEC DE LA 9e RELECTURE » ci-dessous.

**Décisions du lot, dites** : l'arbre VIVANT reste dans la scène active
(l'import « à la Blender » ; le retirer serait un geste, pas un oubli) ;
refus nommé si le document actif est un éditeur de model ; tranche vide →
nœud sauté et journalisé ; slots pleins → arrêt avec message « Import
INCOMPLET », on garde ce qui a pu naître.

**⚠️ ÉCART DÉCLARÉ — la géométrie importée n'est PAS encore dans le
`.nkmesh`** : même dette que la géométrie éditée (liste officielle de
NkModelerScene.h, complétée le 17/08). Le fichier écrit nœuds, origines et
noms ; les sommets vivent dans la session (l'éditeur de model travaille sur
l'archive vivante). À la réouverture d'un autre jour, les nœuds reviennent
en primitives. Fermer cette dette = sérialiser la géométrie CPU (concerne
autant le mode Édition que l'import) — chantier séparé, à ordonner par
Rihen.

### ÉCHEC DE LA 9e RELECTURE — cause trouvée, corrigée, rejouée (17/08 soir)

**Symptôme (Rodolf)** : « l'import duplique N fois son mesh et ça n'importe
pas tous les models. » Journal de sa course (LowPolyCars.obj) :
`MESURE dup model : src=111 → root=113 internesDeLaSource=1 nes=46` + WRN
« plus d'emplacement libre » + UNE seule `MESURE creation` sur 5.

**Cause — la duplication s'auto-alimentait sur une source VIVANTE.**
`HostSpawnLike` copie le parent de sa source si ce parent n'est pas
`nkvpDeleted` (depuis `718ab43b`, 01/08). La boucle enfants de
`HostDuplicateTree` (16/08) réévaluait `HostIsInnerMeshOf(c, src)` PENDANT
qu'elle créait : le double d'un maillage d'un model vivant chaînait donc
lui-même vers `src`, la boucle le re-appariait en atteignant son slot, et
chaque naissance en semait une autre — 46 naissances = exactement les 46
slots libres restants, puis épuisement, d'où les models 1-4 jamais nés.
Le sens navigateur → scène ne cascadait pas (parent d'une source archivée =
`nkvpDeleted`, jamais copié) : c'est pour ça que tous les dépôts mesurés
étaient sains, et que le défaut a attendu le premier passage
scène → archive (l'import).

**Correctif** : la liste des candidats se FIGE avant la première naissance
(deux passes dans `HostDuplicateTree`). La copie de parent de
`HostSpawnLike` n'est pas touchée — elle sert le coller/dupliquer d'un
enfant. L'extraction des tranches n'a jamais été en cause (les comptes de
la caisse étaient justes dès la course échouée).

**Rejoué par l'agent** (crochet `NK_IMPORT_FILE`, même chemin que la
confirmation du picker ; projet AgentTest — jetable, R7) :
- avant correctif : reproduction exacte, au chiffre près (nes=46, 1 création) ;
- après : 5 models, 5 dup à `nes=1`, 5 `MESURE origine ECART=(0,0)`, aucun
  WRN ; caisse 2931v/7578i + 4 roues 337v/954i, somme = le fichier entier
  (4279/11394), ancres distinctes aux 4 coins — tranches distinctes prouvées ;
- l'autre sens (dépôt via `NK_DROP_TOKEN`) : `nes=1`, `cumul ECART=(0,0)`.

Répond à la précision de Rodolf (« 5 models : la coque et 4 roues, mesh
indépendants avec leur propre origine ») : le fichier déclare 5 `o` (coque +
4 roues) → 5 racines indépendantes, chacune avec SA géométrie ET SON
origine — les 4 roues naissent chacune à SON ancre (±1.229, ±0.691 : le
centre de la boîte de SA tranche, donc le moyeu), pas au barycentre du
véhicule ; la racine de chaque model est la moyenne de SES ancres seulement.
(Nuance dite : l'OBJ ne déclare aucune origine par objet — sommets monde,
`o` sans transform — l'origine est donc reconstruite par la règle « ancre =
centre de boîte », qui tombe au moyeu pour une roue.)

**⚠️ TROUVÉ EN MESURANT, à trancher par Rodolf — l'import écrit sur disque
SANS geste de sauvegarde.** Quand une vignette de matériau fraîchement
encodée rejoint son fichier (`main.cpp`, bloc « une vignette fraîchement
encodée rejoint son fichier »), elle appelle `NkProjectWriteAssets` partiel —
et l'exemption `browserOriginDirty` (« à écrire au prochain enregistrement,
même partiel ») fait que ce passage écrit AUSSI les cartes importées, ~1 s
après l'import. Le témoin négatif « quitte sans sauver → aucun `.nkmesh` »
est donc FAUX dès qu'une vignette se rafraîchit. Deux règles se rencontrent
(Q49 contre « les .nkmesh partent à la sauvegarde ») ; c'est une décision de
produit, pas un correctif d'agent.

**📣 PROTOCOLE POUR RODOLF (10e relecture)** : bouton « Importer » du
navigateur, choisis ton fichier véhicule (ou
`Resources/Models/LowPolyCars.obj`, celui de la 9e). Attendu — **5 nœuds,
5 comptes, 5 origines, 5 fichiers** : (A) 5 racines de model (coque +
4 roues) INDÉPENDANTES dans la hiérarchie, chacune avec SA géométrie (bouge
une roue au gizmo : les autres ne suivent pas) et SON origine (le gizmo de
chaque roue est SUR son moyeu, pas au centre du véhicule) ; journal : une
`MESURE creation` par model avec des comptes DIFFÉRENTS entre coque et roue
(coque >> roue ; les 4 roues semblables entre elles, c'est normal) et des
`racine=` distinctes, et `MESURE dup model` disant `nes=1` partout — plus
jamais 46 ; (B) Ctrl+S :
un `.nkmesh` par model dans le dossier courant du navigateur ; (C) le test
« quitte sans sauver » de l'ancien protocole est SUSPENDU — voir l'écriture
par vignette ci-dessus, il échouerait pour une raison qui n'est pas l'import.
Envoie le journal.

### CONTRAT D'IMPORT (Rodolf, 17/08 soir, après la 10e relecture — elle passe) — points a + b LIVRÉS

Le contrat complet est dans `CLAUDE.md` (« CONTRAT D'IMPORT DE MODÈLES »).
Ordre fixé : (a) le empty fabriqué à tort, (b) l'import ÉCRIT et n'ajoute
pas à la scène, (c) trois glisser depuis le système, (d) matériaux/textures,
(e) dialogue d'import.

**Fusion préalable (`c6156782`)** : `origin/main` (#68-#72) fusionné. Deux
leçons de « fusion sans conflit » payées : (1) `main` lui-même était ROUGE —
#69 et #70 avaient chacun ajouté `NkString name` à `NkGLTFNode` (`duplicate
member`), corrigé ici puis, en parallèle, par #68 (version de main gardée) ;
(2) #62 était un *squash* d'un état antérieur de cette branche : l'auto-merge
dupliquait le bloc « LACHER DU NAVIGATEUR » dans `main.cpp` — tout
`Applications/NK3DModeler` repris de HEAD, puis le seul vrai delta de main
(#64, `NkImage` type valeur) réappliqué. Cible 31/31, Nogee 45/45, workspace
**200/200**. #67 `MERGEABLE CLEAN`.

**Grille écrite d'avance (a)+(b)** — course `NK_OPEN_RECENT=0` (AgentTest,
jetable) + `NK_IMPORT_FILE=LowPolyCars.obj`, binaire Debug :
1. 5 × `MESURE creation … DIRECT(sans empty) maillages=1/1`, 5 slots
   consommés en tout (contre 20 : plus de racine, plus d'archive dupliquée),
   origine = ancre 3D (Y non nul = moyeu) ; comptes caisse 2931v/7578i,
   roues 337v/954i ;
2. AUCUNE `MESURE dup model`, AUCUNE `MESURE origine`, aucun WRN ;
3. sur le disque À LA FIN DU GESTE, sans Ctrl+S ni `NK_AGENT_SAVE` : 5
   `.nkmesh` nommés d'après les `o`, un seul nœud chacun, position = origine
   loguée ; `Scene1.nkscene` et `AgentTest.nk3dm` NON réécrits ;
4. rechargement SANS sauvegarde : les 5 cartes reviennent par le rescan
   (`srcNode>0`), la hiérarchie ne les montre pas ;
5. dépôt d'une carte roue dans le vide : un nœud posé au point du pick,
   `model=0`, `sesMateriaux=1`.

**Résultat : la grille tient ligne par ligne** (journaux `logs/app_ab*.log`).
Nœuds 111..115, origines (±1.229, 0.305, ±0.691) pour les roues,
`whell.001_car2.019.nkmesh` = un nœud, `"maillageInterne": false`, écrit à
22:52:30, l'heure de l'import. Le dépôt : `MESURE pose : noeud=116 …
relu=1 model=0`.

**Ce qui a changé (`NkModelerImport.h`, `NkModelerAssets.h`)** :
- un model d'UNE tranche = un nœud maillage DIRECT (`Demo3DHostCreateMeshNode`
  avec `root=-1`) à son ancre, **sans `nkvpIsMesh`** — ce drapeau veut dire
  « matière d'un model » : la hiérarchie de scène cache ces nœuds
  (`NkHierNodeSkip`) et la relecture le retire à tout maillage sans model
  au-dessus (`NkAsRepairOrphanMeshes`) ; trouvé à la relecture des
  consommateurs, avant la course. Plusieurs tranches sous un même nom →
  racine EMPTY + maillages, comme avant (un nœud = un matériau : dette dite ;
  le dialogue offrira « regrouper/éclater ») ;
- **archivage EN PLACE** (`Demo3DHostArchiveTree(top, true)`) : rien n'entre
  dans la scène, aucune copie ;
- **`NkProjectWriteCard`** extrait de la boucle d'« Enregistrer » : l'UNIQUE
  écrivain de carte, appelé par l'import juste après la carte. Si l'écriture
  échoue, la carte reste `browserOriginDirty` (reprise à la prochaine
  sauvegarde) et l'échec est nommé dans `hierNote` ;
- refus nommé sans projet ouvert (« l'import écrit des .nkmesh dedans »).

**Faces à surveiller, dites** : (i) l'origine d'une roue est au moyeu, donc
un dépôt sur le sol met le moyeu au sol (la roue s'enfonce de son rayon) —
c'est la conséquence directe de « la roue à sa propre origine » et le
comportement de Blender ; à trancher au dialogue (option « poser sur le sol »)
si Rodolf le veut autrement ; (ii) la géométrie n'est toujours pas dans le
`.nkmesh` (dette déjà déclarée) : réouverte un autre jour, la carte revient
en primitive ; (iii) l'ancien « écriture par vignette » (interaction Q49) est
désormais SANS OBJET pour l'import — l'import écrit lui-même.

**📣 PROTOCOLE POUR RODOLF (11e relecture)** : bouton « Importer », ton
fichier véhicule. Attendu : (A) RIEN dans la hiérarchie ni la vue 3D ; 5
cartes dans le navigateur (dossier courant) ; le journal dit 5 × `DIRECT(sans
empty)` et 5 `fichier=…nkmesh` ; (B) sur le disque, sans Ctrl+S, les 5
`.nkmesh` sont là, à l'heure de l'import ; (C) glisse une roue dans la vue :
elle apparaît au point du lâcher, SON gizmo est au moyeu, la hiérarchie
montre UN nœud « whell… » de type Mesh, pas de empty au-dessus ; bouge-la,
les autres cartes n'y sont pour rien. Envoie le journal.

### CONTRAT D'IMPORT — point (c) LIVRÉ : les trois glisser depuis le SYSTÈME

`NkDropFileEvent` existait dans NKEvent (cible OLE Win32 + Wayland/XCB/XLib/
Android/Emscripten) ; il manquait `wc.dropEnabled = true` et l'écoute. Fait
(`main.cpp`) : l'événement est RANGÉ (`st.osDrop*`), la boucle route une fois
les rects de la frame connus (`NkOsDropRoute`, `NkModelerImport.h`) :

| zone du lâcher | effet |
|---|---|
| vue 3D (`viewRect`) | import + **pick différé** + instanciation au point du lâcher, disposition du fichier conservée (barycentre X/Z des origines amené sous le curseur, hauteurs du fichier gardées) |
| hiérarchie (`hierRect`, nouveau) | import + instanciation aux **coordonnées du fichier** |
| navigateur (`browserRect`) | import **seul** (cartes + `.nkmesh`, rien dans la scène) |
| ailleurs | refus nommé |

L'instanciation (`NkImportInstantiate`) duplique depuis l'archive comme le
dépôt navigateur → scène (`Demo3DHostDuplicateNode`), puis pose EXPLICITEMENT
(le double naît décalé de 0.45 « comme Blender », ce qui casserait la voiture),
rotation/échelle de la source conservées, nom de la carte repris.

**Grille (4 courses `NK_OS_DROP="x,y,<chemin>"`, crochet qui fabrique le lâcher
aux pixels donnés — seul le trajet depuis l'explorateur est simulé)** :
C1 (700,250) → `zone=1`, 5 créations, 5 instanciations 116..120, poses =
origines + offset commun (écarts entre roues ±1.229/±0.691 conservés, Y du
fichier gardés : caisse 0.761, roues 0.305) ; C2 (100,400) → `zone=2`, 5
instanciations aux coordonnées EXACTES du fichier ; C3 (800,850) → `zone=3`,
5 créations, 0 instanciation, 5 fichiers ; C4 (800,8) → `zone=4`, refus,
0 création, 0 fichier. Journaux `logs/app_c_C1..C4.log`.

**Périmètre déclaré** : le trajet OLE (explorateur → `NkDropFileEvent`) n'est
pas rejouable sans main — c'est la relecture de Rodolf qui le couvre. Sur un
objet, le lâcher OS pose comme dans le vide (pas de menu « enfant » : un
fichier entier n'est pas une carte) — dit, à trancher si besoin. Multi-fichiers :
chaque fichier importe, les cartes s'additionnent.

**📣 PROTOCOLE POUR RODOLF (11e relecture, complément)** : depuis l'explorateur
Windows, glisse `LowPolyCars.obj` (D) sur la vue 3D → la voiture ENTIÈRE
apparaît sous le curseur, roues en place, 5 nœuds Mesh dans la hiérarchie, 5
cartes, 5 fichiers ; (E) sur la hiérarchie → la voiture aux coordonnées du
fichier ; (F) sur le navigateur → 5 cartes, 5 fichiers, RIEN dans la scène ;
(G) sur la barre de menus → message « Déposez un fichier 3D sur… ».

### MIGRATION GLISSER-DÉPOSER SUR L'API NKGui — étape 1/2 LIVRÉE : la HIÉRARCHIE (2026-08-18)

Directive (Q53) : remplacer la logique locale (15 champs d'état, seuil 36.f,
fantôme TextV en double) par l'API de bibliothèque `BeginDragSource /
SetDragPayload / BeginDropTarget / AcceptDragPayload` — celle que Nogee
consomme déjà. **Migration, pas changement de comportement.**

**Fait (commit `7555a345`)** :
- ligne de hiérarchie = source ET cible via `ctx.ButtonBehavior(id, rowR)` ;
  la sélection reste au registre (`hit.Clicked`) — mesuré : pas de conflit,
  le press sélectionne comme avant pendant que ButtonBehavior arme le glisser ;
- **surcharge additive côté NKGui** : `BeginDropTarget(ctx, id, rect)` pour
  une ZONE qui n'est pas un widget (fond de liste, navigateur) — pose
  `lastItemId/lastItemRect` sans toucher activeId/hotId (les widgets contenus
  gardent survol et clic). 17 lignes, annoncée au canal (Q54), rien d'autre
  dans `NkGuiWidgets.{h,cpp}` ;
- livraison appliquée APRÈS le parcours (`pendingChild/pendingParent`) :
  `SetNodeParent` change l'arbre qu'on est en train de lire ;
- 4 champs retirés : `hierDragNode/X/Y`, `hierDragging`, `hierMouseWasDown`
  (le champ était aussi la victime du débordement `hierFold` — commentaire
  d'historique conservé, anonymisé).

**Crochets témoins (rejouables)** : `NK_HIER_ROWS=<frame>` imprime le rect
écran de chaque ligne visible + rects liste/navigateur ; `NK_AGENT_DRAG` /
`NK_AGENT_DRAG2="f,x0,y0,x1,y1"` pose la souris BRUTE avant `BeginFrame`
(survol à f, press à f+1, glissement 8 frames, relâchement à f+10, rapport
parenté+sélection+navigateur à f+14) — le seuil, le fantôme, les cibles et la
livraison sont le vrai code.

**Grille tenue (Debug, AgentTest, 3 courses)** : reparentage 102→104 (`parent=-1`
→ `104`) ; déparentage sur le fond de liste (`104` → `-1`, compte navigateur
INCHANGÉ = pas de livraison double) ; archivage hiérarchie→navigateur (23→24,
témoin de base mesuré par course à vide). Le seuil NKGui arme à k=4
(`dragActive=1 type=hier.node`). Périmètre : la souris est synthétique (posée
avant BeginFrame) ; le trajet OS réel reste couvert par la relecture de Rodolf.

**Étape 2/2 LIVRÉE — le NAVIGATEUR (2026-08-18, commit `384d05b9`)** : type
NKGui `"brow.item"` (charge `int32[2]` : index + prise arbre/grille), sources =
ligne d'arbre et carte (`ButtonBehavior`, la sélection reste au registre —
`MESURE clic carte` inchangée), cibles = racine / ligne d'arbre / carte-dossier
(widgets) + fond de grille / vue 3D (zones explicites). Livraison appliquée
APRÈS le parcours ; anti-cycle, traversée gauche↔droite (carte Copier/Déplacer),
jeton + pick différé + file multi-cartes : inchangés. Les cibles sont TYPÉES des
deux côtés (rien ne s'allume en croisé hiérarchie/navigateur). 6 champs retirés.

**Grille tenue (5 courses)** : carte→carte-dossier (Dataset `parent 4→14`) ;
arbre→ligne (bobi `14→15`, rangées re-tracées POST-transfert dans le même
lancement) ; arbre→racine (`15→-1`) ; arbre→fond de grille = traversée
(`browAskIdx=9, browAskDest=-1` — résolution `-100`→dossier courant incluse) ;
matériau→vue 3D (`PICK lacher → noeud -1`, `MESURE lacher nature=2` : le jeton
part par le nouveau chemin). Régression hiérarchie rejouée verte ; fichiers
projet d'AgentTest intacts (transferts en mémoire seulement).

**Périmètre déclaré** : souris synthétique (posée avant `BeginFrame`) ; les
cartes de la 2e rangée (y≥921) sont HORS fenêtre (907) — NKGui refuse d'armer
hors fenêtre là où l'ancien registre l'aurait accepté : injouable par un humain
sans molette, donc non-comportement, pas régression. Non rejoués en course :
modèle→vue (même chemin de jeton que matériau, et `NK_DROP_TOKEN` couvre
l'instanciation), file multi-cartes, carte Copier/Déplacer aval, menu
enfant/indépendant — applicatifs non touchés, couverts par la relecture.

**📣 PROTOCOLE POUR RODOLF (12e relecture — le glisser-déposer entier)** :
binaire du 18/08 ou plus récent. (A) Hiérarchie : glisse une ligne sur une
autre → parenté ; sur le vide de la liste → déparenté ; vers le navigateur →
carte MESH (l'original reste). (B) Navigateur : carte → carte-dossier et
ligne d'arbre → dossier : la carte déménage ; grille ↔ arbre → la carte
Copier/Déplacer apparaît ; carte matériau → un objet de la vue → le matériau
s'applique ; carte modèle → vue vide → le modèle naît sous le curseur (menu
enfant/indépendant sur un objet). (C) Pendant un glisser de carte, les lignes
de la HIÉRARCHIE ne doivent PAS se surligner (et réciproquement). (D) Le
fantôme (nom sous le curseur) est dessiné par NKGui — même rendu que Nogee.
Envoie le journal (`MESURE clic carte`, `MESURE lacher`, `PICK lacher`).

**Suite** : contrat d'import (d) matériaux/textures et (e) le dialogue.


## ⚰️ `NkViewport3D.cpp` EST MORTE — 2 649 lignes, signalée et NON supprimée (2026-08-24)

**Mesuré, pas supposé.** Deux vues 3D coexistent dans l'application :

| | |
|---|---|
| `Viewport/NkViewport3D.cpp` (2 649 lignes) | **jamais de device** — `main.cpp:350-351` ne lui en donne aucun |
| `Viewport/NkDemo3D.cpp` (18 660 lignes) | **la vivante**, celle qui rend |

Conséquence : **tout le dispatch d'édition de `main.cpp:1435-1626` est du code
mort** — il vise la vue qui ne dessine pas.

⚠️ **NE PAS LA SUPPRIMER — c'est une décision de Rodolf, pas la mienne.** Elle
est écrite ici pour une raison précise : **une capacité morte non documentée
recrute.** Quelqu'un finira par l'améliorer, la corriger, l'optimiser, en
croyant travailler sur la vue vivante — et son travail n'apparaîtra jamais à
l'écran. C'est arrivé assez de fois cette semaine sur des chaînes bien plus
courtes.

**Trois options, à trancher** : la supprimer, la réactiver, ou la marquer dans
le code lui-même (un `#error` sous un drapeau, un préfixe `DEPRECATED_` sur le
fichier). La troisième est la moins chère et suffit à empêcher le recrutement.

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
   - ~~**Exposer `clearcoat` et `subsurface`**~~ — ✅ **FAIT, et MESURÉ le
     2026-08-24.** Le panneau les propose (`Demo3DHostProjMatSetSurface`, appelé
     depuis `NkModelerProperties.h`), le chemin va jusqu'au GPU
     (`dc.clearcoat` → `uObj.clearcoat`) et `pbr.frag.nksl` les lit bien
     (lignes 774-776 pour le vernis, 720-722 pour la diffusion).
     **Preuve par capture, pas par lecture** : deux rendus du même projet à
     `cc=0` puis `cc=1` donnent des images **différentes**, alors que deux
     rendus identiques sont identiques **au bit près**.
     ⚠️ Le texte ci-dessus disait l'inverse et datait du 6 août. Il a survécu
     dix-huit jours parce qu'**une liste écrite ne se périme pas bruyamment** :
     elle ne tombe pas comme un banc, elle ne demande jamais rien.
   - **Transmission + IOR** — 🔴 **MESURÉ LE 2026-08-24 : L'INDICE NE FAIT
     RIEN.** Deux rendus de verre à `n=1,0` et `n=2,4` sont **identiques au bit
     près**, alors que le même banc distingue le type PBR du type Verre — donc
     le shader du verre tourne bel et bien et l'instrument sait voir un écart.
     **La cause est localisée** : le champ `vernis` porte l'indice pour le type
     Verre (double emploi assumé, `NkDemo3D.cpp:14708-14713`, et son SIGNE porte
     la coche d'activation), mais l'application l'écrit dans `dc.clearcoat` →
     `uObj.clearcoat`, tandis que `Glass/NkSL/glass.frag.nksl:100` lit
     `uMat.clearcoat` — que rien ne remplit. Personne ne l'avait vu parce que la
     valeur de repli, 1,5, est du verre à vitre : **le défaut se cache derrière
     une valeur par défaut plausible.**
     ⚠️ Et les six variantes de backend ne s'accordent pas :
     `Glass/GL/glass.frag.gl.glsl:20` code `float ior=1.45;` **en dur**.
     C'est le « S » de BSDF (*scattering*, qui inclut la transmission) : sans
     elle, **pas de verre, pas d'eau, pas de liquide crédibles**. Le manque le
     plus visible pour un modeleur — et il est plus proche qu'on ne croyait,
     puisque le réglage existe déjà et qu'il ne lui manque qu'un fil.
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

## 🃏 CARTES D'ASSETS DU NAVIGATEUR DE CONTENU — RÉFÉRENCE (décision de Rihen, 17/08)

*Rihen : « pour le design des cartes d'assets dans le Content Browser [de Nogee],
s'inspirer de NK3DModeler ». Cette section est ce que l'agent Noge lira — une
carte se copie mal depuis une capture, elle se reprend bien depuis une
description. Tout ce qui suit est **lu dans le code** (`NkModelerBrowser.h`,
zone « CARTES »), pas décrit de mémoire.*

**Géométrie** (unités logiques ; tout ce qui passe par `S()` suit l'échelle UI) :
carte de **96 px de large** = vignette **96×96** + **bande de type 3 px** +
**pied 34 px**. Ombre portée décalée `(+2, +3)`, noir alpha 90, rayon 3 —
« comme Unreal ». Espacement entre cartes `S(14)`.

**Redimensionnement** : la grille **enveloppe** — une carte qui dépasserait la
marge droite (`tx + tw > wrapW`) part à la ligne suivante. Largeur de carte
FIXE ; c'est le **nombre de colonnes** qui varie.

**Deux états de sélection, deux marques — et ils ne se confondent pas** :
- **ACTIVE** (`selectedAsset`) : celle dont les panneaux montrent les
  propriétés → **aplat** accent débordant de 2 px autour de la carte ;
- **CHOISIE** (`browserPicked[]`) : celles qui partiront ensemble si on tire →
  **contour** accent, même débord. *Raison écrite dans le code : on peut choisir
  cinq cartes et n'en inspecter qu'une ; la différence doit se lire d'un coup
  d'œil sur une grille de trente cartes, pas se deviner entre deux nuances.*

**Fond de vignette** : damier 8 px (`InputBg` / `WindowBg`) — il dit « ce fond
est vide », comme un canal alpha.

**Couleur et nom de type** : depuis le **point de passage unique**
(`NkAssetColor` / `NkAssetKindName`, `NkModelerUI.h`) — la pastille de filtre et
le liseré d'onglet lisent la **même table**. Ne pas dupliquer cette table.

**Vignette par nature** (tout ceci est le **repli** tant qu'aucune miniature
n'existe) : dossier = chemise avec rabat, **pleine ou vide selon son contenu** ;
procédural = **deux nœuds reliés avec broches** (la recette, pas le résultat —
un cube le ferait passer pour un Model) ; matériau = **boule d'aperçu réelle**
(ids image 4400+, rendue par l'hôte, retéléversée quand elle périme) ; scène =
sa capture (4500+, ratio préservé) ; mesh = cube plein ; dataset = document
ligné.

**⚠️ Règle des miniatures (Rihen, 8 août)** : la miniature vient de **LA VUE** —
ce que la vue 3D regarde — **pas d'un nœud caméra de la scène**. Un projet sans
caméra doit avoir ses vignettes quand même. Point d'accroche **unique** dans le
code ; un second endroit qui dessinerait un aperçu divergerait au premier
changement de cadrage.

**Pastille d'albédo sur les matériaux** (12 px, coin bas-droit de la boule,
liseré noir) : la boule d'aperçu est une **SIMULATION** (studio, fond clair — un
albédo 0,7 y paraît blanc puis se pose sombre dans une scène à ambiance 0,050) ;
la pastille est une **DONNÉE**, l'albédo brut borné avant conversion en octets.
Les deux coexistent délibérément.

**Pied de carte** : deux lignes — **nom éditable en place** (32 car. max,
**clippé à la carte** : il débordait sur la voisine) puis **type** en
`TextMuted`. Renommer la carte d'une scène renomme **son document** (l'onglet
suit), uniquement **à la validation** — une copie par frame écraserait le
renommage fait depuis l'onglet.

**Gestes** : double-clic sur un dossier = y entrer ; tirer = glisser-déposer
(armement sur la carte, cible au survol) ; le titre du panneau n'existe qu'en
**un** point (`kBrowserTitle`) parce qu'il est peint **et** mesuré.

**Classement/visibilité** : un seul décideur, `NkBrowVisible` (tri + filtre +
recherche) — deux endroits qui décident ce qui est visible finiraient par ne
plus être d'accord.

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

---

## 2026-08-18 — LES TROIS DEFAUTS RAPPORTES PAR RODOLF A L'USAGE

Rapportes mot pour mot apres sa relecture du 18/08 au matin. **Les trois
touchent le chemin d'entree**, mais ils n'ont PAS la meme cause — et deux des
trois ont leur cause hors du modeleur ou hors de la migration.

### Defaut 1 — CORRIGE — « le clic droit Importer ne fonctionne pas, a gauche ou a droite »

**Cause, tranchee par lecture** : l'entree « Importer... » du menu contextuel du
navigateur pose le code d'action `20` ; la table de dispatch qui suit traite
30-33, 10-16, 0, 1, 2, 3 et 4 — **aucune branche pour 20**. Le menu se refermait
proprement et rien ne se produisait.

C'est le **jumeau** du bouton « Importer » corrige le 17/08, qui etait « peint
mais jamais lu ». La meme fonction manquait par ses **deux portes**, pour deux
raisons differentes : c'est pour cela que corriger le bouton n'avait rien change
au menu, et c'est le genre de coincidence qui fait croire a un defaut unique
qu'on aurait mal corrige.

« A gauche ou a droite » = l'**arbre des dossiers** et la **grille des cartes** :
deux zones, **un seul menu**, donc un seul defaut vu deux fois (verifie : un seul
appel a PaintBrowser dans main.cpp).

**Correctif** : la branche `act2 == 20` fait EXACTEMENT ce que fait le bouton —
meme selecteur, meme depart, meme `pickerAction = 2` — pour que les deux portes
ne puissent pas diverger. `NkMarkTreeDirty` est desormais SAUTE pour 20 : ouvrir
un selecteur qu'on peut annuler ne doit pas salir le projet.

**Limite declaree, pas un oubli** : l'import atterrit dans le **dossier courant**
(`st.browserFolder`), meme si le clic droit visait la carte d'un autre dossier —
c'est le comportement du bouton, et les deux portes restent identiques. Faire
atterrir dans le dossier VISE demande de passer une destination a
`NkImportCreate` (qui lit `st.browserFolder` en dur, deux fois) : a traiter avec
le point (d) du contrat d'import, pas en douce ici.

### Defaut 2 — CORRIGE — « en selection multiple, cliquer un model ouvre l'onglet d'un model »

**La cause n'est pas dans le modeleur : elle est dans NKGui, et elle touche tout
le monde.**

`NkGuiInput::NewFrame` declarait un double-clic sur un critere **purement
temporel** : `mouseDoubleClicked[i] = (clickTime[i] < 0.40f)`. **Aucun controle
de position.** Deux clics a moins de 0,40 s d'ecart *n'importe ou sur l'ecran*
en formaient un.

Ce qui le rend couteux : **personne ne consomme ce drapeau seul**. Une vingtaine
de sites le croisent avec la zone survolee MAINTENANT (`hov &&
mouseDoubleClicked[0]`), et `NkHitRegistry::DoubleClicked` fait exactement pareil.
Le « double-clic » etait donc **attribue a un widget clique une seule fois**.

**Le chiffre vient du journal de Rodolf lui-meme** : Ctrl+clic sur la carte 26 a
07:59:17.529, puis sur la carte 25 a 07:59:17.917 — **388 ms**, sous le seuil.
La carte 25 est de nature 6 (model) : son editeur s'est ouvert. Construire une
selection multiple, c'est precisement cliquer vite de carte en carte : le geste
normal declenchait le defaut.

**Deux correctifs, et ils ne disent pas la meme chose :**

1. **NKGui (cause racine)** — rayon `kDblMaxDist = 6 px` entre les deux clics.
   ATTENTION, **fichier partage** : Nogee, Nkoung, Mou et PV3DE consomment le
   meme detecteur. Le correctif les repare aussi ; s'ils dependaient du
   comportement temporel pur, ils le verront changer.
2. **NK3DModeler (regle d'interface)** — Ctrl ou Maj enfonce n'ouvre jamais :
   c'est un geste de SELECTION. Vrai independamment du (1).

**A ne pas confondre** avec la trouvaille pre-existante notee le 18/08 (l'appui
sur une carte model bascule la hierarchie vers les noeuds d'archive du model) :
celle-la change le contenu de la **hierarchie**, pas l'onglet. Elle reste
ouverte.

### Defaut 3 — NON RESOLU — « plusieurs models selectionnes, seul le premier se deplace »

**Deux causes refutees par la mesure, la troisieme jamais atteinte. Rien n'est
corrige, et il ne faut pas croire que si.**

| ce qui a ete mesure | resultat |
|---|---|
| la selection s'accumule-t-elle ? | **oui** — noeuds 116 et 117 tous deux `sel=1` (levier neuf `NK_SEL_NODES`, qui appelle les memes fonctions que le Ctrl+clic de la hierarchie) |
| le gizmo distribue-t-il le delta ? | **oui** — `NkGizmo.h` : `for i < mCount if (mSel[i]) mTr[i] += wd`, tous les selectionnes |
| le deplacement lui-meme | **JAMAIS EXERCE** |

**La derniere ligne est un resultat NUL, et son perimetre voyage avec lui** :
trois courses (appui a (600,450), (600,425), (600,405) puis glissement) n'ont
produit **aucune** ligne `MESURE commit gizmo`. Le gizmo n'est jamais entre en
glissement. Ces courses ne disent donc **pas** que le deplacement multiple
fonctionne — elles disent que **je ne l'ai pas atteint**.

**Absence trouvee en essayant de s'en servir** : `NK_AGENT_DRAG` pose l'etat
souris brut que le SHELL lit (il a prouve 8 gestes de glisser-deposer les 17-18/08),
mais **rien ne montre qu'il atteigne l'entree du gizmo du viseur 3D** — tous ses
usages anterieurs visaient des widgets. Et la position ecran d'une poignee du
gizmo n'est **journalisee nulle part** : le pivot est la mediane de la selection,
et rien ne le projette. Sans l'un ou l'autre, le deplacement multiple n'est pas
scriptable.

**Hypothese restante, la plus plausible (H3d), ecrite pour ne pas etre reperdue :**
le pick de la vue tourne a **chaque appui gauche**
(`gin.leftPressed && !emptyGizmo.IsDragging() && !gizmo.IsDragging()`). S'il
aboutit sur un model **sans modificateur tenu** — exactement ce qu'on fait en
saisissant un objet pour le tirer — la branche appelle `emptyGizmo.Select(bestU)`,
qui est **EXCLUSIF** : la selection multiple s'effondre sur l'objet saisi, et lui
seul bouge. La cause ne serait alors ni le gizmo ni la boucle de commit, mais
**le clic qui commence le geste**. Non reproduite (les appuis n'ont rien touche) :
a instrumenter, pas a corriger a l'aveugle.

**Instrument laisse en place** — `MESURE commit gizmo : selectionnes=N actif=i`,
suivi d'une ligne `commit noeud=.. tr=(..) avant=(..)` par selectionne. Il separe
les trois causes d'un coup d'oeil sur le prochain journal :
`selectionnes=1` = la selection s'est effondree (H3d) · `selectionnes=N` avec un
seul `tr` non nul = la distribution · N deltas non nuls = le defaut est en aval.

### Correction de fait sur logs/app.log — il N'EST PAS tronque au lancement

Mesure : **26 lancements** coexistent dans le fichier du 18/08 (compte des lignes
`NkRHI_DX11] Initialise`), du 17/08 23:06 au 18/08 08:01 — **le journal AJOUTE**.
Les sessions de Rodolf sont les deux dernieres (lignes 13068-14627,
07:56:25 vers 08:01:07, puis une relance de 7 s). Les 13 067 premieres lignes
sont des courses d'agent. **Lire « le journal » sans decouper par lancement,
c'est melanger les runs** — la face 3 de la grille, et elle etait tendue des la
premiere commande.

### Leviers d'agent ajoutes

- `NK_SEL_NODES="frame,n1,n2,..."` — selection multiple de noeuds utilisateur.
  Il manquait : `NK_GIZMO_MULTI` ne pilote que `st->gizmo` (objets de demo,
  indices < kNumObj), alors que **tous les models vivent dans `emptyGizmo`**
  (noeuds >= 90). Le levier existant ne pouvait pas atteindre le regime du
  defaut. Il appelle `SelectEmptyNode` puis `ToggleEmptyNode` — le code de
  production, pas une reconstruction. Frame obligatoire (dette « les leviers ne
  disent pas QUAND »).
- `NK_DROP_TOKEN2` — seconde fente, convention `NK_AGENT_DRAG`/`DRAG2`. Poser
  DEUX models en un lancement : avec une seule fente il fallait enregistrer
  entre deux lancements, donc **modifier le projet pour pouvoir le mesurer**.

### PROTOCOLE DE LA 13e RELECTURE POUR RODOLF — les trois gestes

Binaire du 18/08 ou plus recent. Envoie le journal, **et dis a quelle heure tu
as commence** (le fichier contient plusieurs lancements : sans l'heure, on ne
sait pas ou couper).

1. **Import par le clic droit.** Dans le navigateur, clic droit **sur l'arbre
   des dossiers a gauche**, puis « Importer... » : le selecteur doit s'ouvrir.
   Recommence **sur la grille des cartes a droite**, et une troisieme fois **sur
   une carte**. Attendu : les trois ouvrent le meme selecteur, et le fichier
   choisi s'importe dans le **dossier courant**. Attendu au journal :
   `[import] MESURE import` puis une `MESURE creation` par model.
2. **Selection multiple de cartes.** Clique une carte de model, puis
   **Ctrl+clic** rapidement sur trois autres — *aussi vite que tu veux*, c'est
   le point. Attendu : les quatre se marquent, et **aucun onglet ne s'ouvre**.
   Puis, sans Ctrl, **double-clique** une carte : la, l'onglet doit s'ouvrir
   (le double-clic normal ne doit pas avoir ete casse par le correctif).
3. **Deplacement multiple — LE DEFAUT N'EST PAS CORRIGE, c'est une MESURE.**
   Pose deux ou trois models dans la scene, selectionne-les (Ctrl+clic dans la
   vue ou dans la hierarchie), puis deplace-les au gizmo. Dis ce que tu vois, et
   envoie le journal : il portera desormais
   `[Demo3D] MESURE commit gizmo : selectionnes=N actif=i` suivi d'une ligne par
   noeud. **C'est ce N qui tranche** entre « la selection s'effondre quand tu
   saisis » et « elle tient mais un seul recoit le mouvement » — deux causes,
   deux correctifs differents, et l'ecran ne les distingue pas.

---

## 2026-08-18 (matin) — Defaut 3 : LA CAUSE EST TROUVEE, ET ELLE N'EST PAS CORRIGEE

### La reponse a la question de methode (Q57 section 4) — ce n'etait pas une portee de levier

**Question posee** : pourquoi le viseur ne voit-il pas la souris que le shell
voit ? **Reponse, par lecture, et elle ferme la question** : ce ne sont pas deux
portees, ce sont **deux canaux disjoints**.

| ce qui pilote | ce qu'il ecrit | qui le lit |
|---|---|---|
| `NK_AGENT_DRAG` | `ui.input.mousePos` / `ui.input.mouseDown[0]` — l'etat souris de **NKGui** | le shell : arbre, cartes, widgets |
| une vraie main dans la vue | l'evenement de **FENETRE** `NkMouseButtonPressEvent` vers `st->pickPending` (NkDemo3D.cpp:4385), et `NkInput.MouseX()` | le viseur : `gin` (NkDemo3D.cpp:9322-9331) |

`gin.leftPressed = st->pickPending`. **Aucune ligne ne relie `ui.input` a `gin`.**
Un glisser du shell ne pouvait donc, *par construction*, jamais atteindre le
gizmo — quelle qu'ait ete la portee du levier. Elargir `NK_AGENT_DRAG` (piste (ii)
de Q57) aurait ete un correctif pose sur une cause supposee : c'etait le mauvais
fil, pas un fil trop court.

### La cause du defaut 3, etablie par LECTURE (comme le defaut 1)

`NkDemo3D.cpp`, branche de pick de la vue :

```cpp
const bool clickedActive =
    (bestU >= 0 && bestU == st->emptyGizmo.ActiveIndex());
if (clickedActive)
    bestU = -1;                        // deja actif : le clic est aux POIGNEES
if (bestU >= 0) {
    ...
    if (gin.shiftDown || gin.ctrlDown)
        st->emptyGizmo.ToggleSelection(bestU);
    else
        st->emptyGizmo.Select(bestU);  // EXCLUSIF
```

**La garde `clickedActive` ne protege que l'ACTIF.** Avec N models selectionnes,
presser l'un des **N-1 autres** — sans modificateur, ce qu'on fait exactement en
saisissant un objet pour le tirer — tombe dans `Select()`, qui est exclusif : la
selection s'effondre sur l'objet saisi **avant** que le gizmo n'ait rien vu.
Symptome a l'ecran : "il n'y a que le premier qui se deplace".

C'est la **troisieme fois** dans ce chantier que la cause se lit au lieu de se
courir. Les deux autres causes candidates avaient deja ete refutees par la mesure
(la selection s'accumule ; le gizmo distribue le delta a tous les selectionnes).

**Fenetre exacte du defaut, et ce qui en est HORS** :
- presser une poignee : `emptyGizmo.Update` tourne AVANT le pick (l. 9630 contre
  l. 9824) et pose `gin.leftPressed = false` ; le pick est saute, **la selection
  tient**. Ce chemin n'est pas atteint par le defaut ;
- presser l'objet ACTIF : `clickedActive` donne `bestU = -1`, aucune branche
  prise, **la selection tient** ;
- presser un objet **selectionne mais pas actif** : **effondrement**.

**CE QUI N'EST PAS PROUVE, et qui doit voyager avec le resultat** : que le geste
de Rodolf passe par cette fenetre plutot que par la poignee. La lecture prouve
que le chemin CASSE existe ; elle ne prouve pas qu'il l'a emprunte. C'est
exactement ce que les instruments ci-dessous vont dire sur son prochain journal.
**Rien n'est corrige.**

### Instruments poses (152 lignes, NkDemo3D.cpp)

1. **`MESURE pick vue`** — *toujours actif, sur chaque appui gauche dans la vue.*
   `xy=(..) touche=<noeud> actif=<i> deja_selectionne=<0|1> modificateur=<0|1>
   selectionnes avant=<N> apres=<M>`.
   **C'est lui qui tranche** : `avant=N apres=1` = l'effondrement (cause ci-dessus,
   confirmee) ; `avant=N apres=N` = ce chemin est innocent et l'enquete repart en
   aval. Il se lit **a cote** de `MESURE commit gizmo` deja en place.
2. **`MESURE entree vue`** — *toujours actif, sur appui ou pendant un glissement.*
   L'etat `gin` REEL : position, delta, presse/enfonce, ctrl/maj, **distance en px
   a la poignee la plus proche**, et les deux drapeaux de glissement. Repond a
   "mon injection arrive-t-elle ?" sans supposition.
3. **`NK_GIZMO_PROBE=<frame>`** — carte ASCII 64x32 de la vue marquant les pixels
   ou une poignee est pickable. Elle n'est **pas** redessinee : elle interroge
   `NkGizmo3D::HandlePickDistPx`, **le test de pick du gizmo lui-meme**, donc
   l'autorite qui decide reellement du clic — pas une reconstruction (face 4).
   Comble l'absence "la position ecran d'une poignee n'est journalisee nulle
   part", **sans toucher au fichier partage `NkGizmo.h`**.
4. **`NK_VIEW_DRAG="f,x0,y0,x1,y1"`** — le glisser qui manquait, sur le BON canal :
   il ecrit dans `gin`, pas dans `ui.input`. Meme decoupage que `NK_AGENT_DRAG`
   (f survol, f+1 appui, f+2 a f+9 glissement, f+10 relachement).
   **PERIMETRE DECLARE** : il ecrase `gin` apres sa construction, donc il
   court-circuite `nkvpHover`, `nkvpInputOn` et l'arbitrage des surcouches. Il
   exerce **le gizmo et le pick**, pas le **routage du clic** — une course verte
   ici ne dit rien du routage.

### PIEGE D'ENVIRONNEMENT — aucune course GPU n'a pu tourner ce matin

**Les instruments sont poses et le build est vert (31/31), mais ils n'ont ete
exerces par AUCUNE course.** Il faut le dire, pas le laisser deviner.

| mesure | chiffre |
|---|---|
| lancements tentes | **6**, tous morts a la frame 2-3 |
| erreur | `hr=0x887A0005` (DXGI_ERROR_DEVICE_REMOVED) sur `CreateTexture2D` puis `CreateBuffer`, puis segfault (exit 139) |
| evenements `nvlddmkm` au journal Windows | **un par lancement**, horodates a la seconde de mes courses |
| occurrences dans les 33 lancements de la nuit (avant tout changement) | **0** |
| VRAM au moment des courses | **6824 / 8192 Mio occupes (83 %)**, GPU a 38 % |

**Temoin, et c'est lui qui tranche** : j'ai **retire mes 152 lignes**, rebati
(31/31), relance — **meme mort, meme `887A0005`, meme exit 139**. Le binaire
*sans* instrument echoue exactement pareil. **Ce n'est donc pas mon changement.**
(Controle redondant lance "pour rien" ; sans lui, la conclusion evidente aurait
ete "l'agent a casse la vue ce matin", et elle aurait ete fausse.)

Cause la plus probable, **non prouvee** : la VRAM est occupee a 83 % par un autre
chantier de la machine (`NKIlyana`, 7 569 s CPU au moment de la mesure). Je ne
sais pas separer "VRAM saturee" de "reset du pilote" — les deux donnent le meme
`887A0005`, et je n'ai pas d'instrument qui les separe.

**Consequence pour tous les agents** : toute course qui ouvre une fenetre GPU est
indisponible tant que la VRAM reste a ce niveau. Ce n'est pas la peine de
diagnostiquer six fois le meme mur.

### Suite ordonnee sur le defaut 3

1. Lire `MESURE pick vue` sur le prochain journal de Rodolf (ou rejouer
   `NK_GIZMO_PROBE` puis `NK_VIEW_DRAG` des que le GPU est rendu).
2. **Si `avant=N apres=1`** : le correctif est d'une ligne — la garde doit porter
   sur `IsSelected(bestU)` et non sur `ActiveIndex()`, avec une decision produit a
   trancher : presser un objet deja choisi sans modificateur doit-il *conserver*
   le lot (comme Blender) ou le reduire au relachement s'il n'y a pas eu de
   glissement ? **Ne pas coder avant d'avoir la mesure** : c'est le troisieme
   candidat, les deux premiers ont ete refutes.
3. Si `avant=N apres=N` : rouvrir en aval, du cote de la boucle de commit et de
   l'apercu en direct (`NkDemo3D.cpp:14227`).

---

## 2026-08-18 (matin) — MESURE D'ECART : le navigateur de contenu contre les planches

**Demandee par Rodolf. RIEN N'EST CORRIGE — cette liste est la pour decider de ce
qui monte dans NKGui et de ce qui reste ici.**

**Perimetre regarde** : les deux captures completes
`Applications/Nogee/design/Screenshot 2026-08-18 082126.png` (grille pleine,
16 items, selection, arbre deroule) et `...082145.png` (la carte ENTIERE avec son
pied) — la planche `AetherionContentBrowserDark.png` etant tronquee ; et, cote
code, `Shell/NkModelerBrowser.h` (1 213 lignes) avec son unique site d'appel
`main.cpp:1683`, plus greps recursifs sur tout `Applications/NK3DModeler/src/`.

**Le sens du flux, rappele par Rodolf** : planche vers composant dans NKGui vers
specialisation par editeur. **Pas** "NK3DModeler vers les autres". La ou planche
et existant divergent, **la planche tranche**.

**Ce qui plait a Rodolf dans l'existant et qui doit se verser dans le composant
partage** : les **deux etats de selection** (`NkModelerBrowser.h:461-475`) —
carte ACTIVE (celle dont les panneaux montrent les proprietes) contre cartes
CHOISIES (celles qui partiront ensemble). La planche ne montre qu'un seul etat ;
elle ne le contredit donc pas, elle est muette dessus. **La distinction se garde,
c'est son ENCODAGE qui doit changer** (point 3 ci-dessous).

### GROS — une zone entiere manque, ou la structure change

| # | ecart | vers |
|---|---|---|
| 1 | **Les trois onglets de panneau** (Content Browser / Favorites / Recent) n'existent pas. La bande de tete porte un titre texte + un chevron de repli (l. 67-85). Zone entiere a creer, plus deux vues a alimenter | **NKGui** (la barre d'onglets de panneau sert tous les editeurs ; la *liste* reste specifique) |
| 2 | **La section Collections** (repliable, en pied de la colonne de gauche) n'existe pas : le panneau gauche n'a qu'une section, l'arbre (l. 269-420). Le grep sur "Collections" rend 0 sur tout `src/` — et la meme commande trouve bien `browSort` (3 occurrences), donc elle sait trouver | **specifique** d'abord (le concept appartient au contenu d'un projet) ; le **widget** "section repliable en pied de colonne" vers NKGui |
| 3 | **La carte active est peinte en APLAT** (`p.Fill`, l. 469) la ou la planche montre un **CONTOUR violet** sur toute la carte. Ce n'est pas un reglage de couleur : la planche reserve l'aplat a **l'arbre** (dossier Kevin) et le contour aux **cartes**. Changer l'un sans redefinir l'autre **detruirait la distinction actif/choisi** que le code defend explicitement. Il faut **re-encoder les deux marques** (par exemple contour epais = actif, contour fin ou voile = choisi) | **NKGui** — la regle "deux etats de selection" vaut pour toute grille d'assets du depot |

### MOYEN — un element a ajouter dans une zone qui existe deja

| # | ecart | vers |
|---|---|---|
| 4 | **Save All** absent de la barre d'outils (la barre existe, l. 89-131 ; l'action existe deja : `app.enregistrer_tout`, `main.cpp:259`) | specifique (cablage) |
| 5 | **Engrenage de reglages** absent a droite de la barre (aucun `NkIcon::Gear` dans `NkModelerBrowser.h` ; il en existe un dans `NkModelerScreens.h:716`, donc le glyphe est disponible) | specifique |
| 6 | **Ligne d'etat mal placee et incomplete** : le compte `element(s)` est en **bas a droite** (l. 1120-1123) au lieu d'une bande **en haut a gauche** de la grille, et **sans le compte de selection** (`16 items (1 selected)`). Le compte est calculable (`st.browserPicked[]`) mais n'est jamais agrege | **NKGui** pour le format `n items (m selected)` ; placement specifique |
| 7 | **Bouton Favorites** absent de la bande de recherche | specifique |
| 8 | **Aucun reglage de taille des vignettes** : `tw = 96`, `pvH = 96` sont des litteraux (l. 428-429), aucun etat ne les porte. **Nogee a deja ce curseur** (`ContentBrowserPanel.cpp:242`) | **NKGui** — sinon il sera ecrit une troisieme fois |
| 9 | **Fil d'Ariane** : pas d'icone dossier devant la racine (l. 184 peint le libelle seul), chevrons en texte (l. 190) au lieu du glyphe | specifique |
| 10 | **Navigation** en icones plates sans fond (l. 152-165) la ou la planche montre des pastilles rondes encadrees, coherentes avec les boutons pleins voisins | **NKGui** (variante "bouton icone rond") |

### PETIT — theme, metriques, libelles

| # | ecart | vers |
|---|---|---|
| 11 | **Pied aligne a gauche** (l. 684 et 700 posent le texte a `tx + pad`) ; la planche le montre **centre**, sur ses deux lignes | **NKGui** (variante centree de `TextClipped` / `EditableText`) |
| 12 | **Bande de type coloree de 3 px** entre vignette et pied (l. 673) : absente de la planche | specifique (choix produit : la garder est defendable, mais c'est un ecart) |
| 13 | **Fond du pied en `PanelHeader`** (l. 674), plus clair que la carte ; la planche donne au pied **le meme fond** que la vignette | specifique (theme) |
| 14 | **Damier** en fond de vignette (l. 477-488) au lieu d'un aplat gris uni | specifique (le damier dit "vide" ; la planche ne l'emploie pas) |
| 15 | **Ombre portee** sous la carte (l. 459) ; la planche montre des cartes plates | specifique (theme) |
| 16 | **Metriques** : carte 96x133 a pied de 34 px, gouttieres `S(12)` et `S(14)` — contre une carte plus carree a pied plus fin et gouttieres plus serrees. A reprendre **en meme temps que le point 8** | specifique |
| 17 | **Libelle racine "Contenu" contre "Content"** (l. 184, l. 283) : ecart de LANGUE, pas de structure. La planche est en anglais, le produit en francais — **a trancher par Rodolf**, ce n'est pas un arbitrage technique | specifique |
| 18 | **Colonne gauche figee a 18 %** (`treeW = r.w * 0.18f`, l. 216), non redimensionnable ; la planche montre une separation manipulable | **NKGui** (separateur reutilisable) |

### Ce qui est DEJA conforme (a ne pas refaire)

Barre d'outils avec **Creer** et **Importer** (l. 95-130, et Importer est bien
**consomme** : `hit.Clicked("brw.imp")` vers picker) ; navigation arriere/avant
avec historique de 64 entrees ; fil d'Ariane **cliquable** ; arbre recursif avec
chevrons, pliage, renommage en place et glisser-deposer ; champ de recherche avec
5 pastilles de type, tri et sens ; **pied de carte a deux lignes avec le TYPE
affiche** (l. 684 et 700) ; **selection de l'arbre en aplat** (l. 278, 333),
conforme a la planche.

**Le bouton Import figure dans la barre d'outils de la planche** : l'import a donc
bien **deux portes voulues** (bouton + menu contextuel). Cela **confirme** la
correction du 18/08 qui a branche la branche `20` du menu contextuel — ce n'etait
pas un doublon a supprimer.

### Point de methode a signaler

Il existe **deux navigateurs de contenu non partages** dans le depot :
`NK3DModeler/Shell/NkModelerBrowser.h` et
`Nogee/src/Nogee/Panels/ContentBrowserPanel.cpp` — et c'est **Nogee** qui possede
deja le curseur de taille et le cache de vignettes. Les ecarts **3, 6, 8, 10, 11
et 18** devront etre ecrits **deux fois** s'ils ne montent pas dans NKGui. C'est
le meme motif que le troisieme selecteur de dossier.

### La TROISIEME capture (`Screenshot 2026-08-18 082535.png`) — la vue 3D, donc ma zone

Rodolf en a depose trois, pas deux. La troisieme montre l'**editeur vue maximisee**,
et elle est la planche de reference du **viseur**, pas du navigateur. Relevee ici
pour qu'elle ne se perde pas ; **aucun ecart n'est chiffre dessus** (je n'ai pas
fait la mesure d'ecart du viseur, seulement celle du navigateur).

Ce qu'elle fixe, en vrac : combos `Perspective` / `Lit` / `Show` en haut a gauche
de la vue ; colonne d'outils verticale a gauche (4 icones) ; trio
capture / image / plein ecran en haut a droite avec un **cube de vue** (TOP / LEFT
/ FRONT) ; superposition de perf en bas a gauche (`FPS: 144 | Draw calls: 812 |
Tris: 2.4M`) ; lecture `X: 0.0  Y: 0.0  Z: 0.0` centree en bas ; bande
`Content Browser` **repliee** en pied ; barre d'etat portant
**`3 objets selectionnes`** a gauche et `FPS · memoire · branche` a droite ;
onglets de documents (`Level`) et onglet de panneau (`Viewport`) avec croix de
fermeture ; barre d'outils de tete `Select Mode` + translate/rotate/scale +
transport (play/pause/stop) + `Platforms` + engrenage.

**Le detail qui me concerne directement** : la barre d'etat annonce le **compte
d'objets selectionnes**. C'est exactement l'information que le defaut 3 rend
fausse a l'ecran, et qu'aucune surface de NK3DModeler n'affiche aujourd'hui — un
utilisateur ne peut pas voir que sa selection vient de s'effondrer. **A ajouter
apres le correctif**, pas avant : afficher un compte faux ne ferait que rendre le
defaut plus visible sans le corriger. (Meme famille que l'ecart n.6 du navigateur,
`16 items (1 selected)` -> a mutualiser dans NKGui.)


---

## 2026-08-18 — AUDIT DES 22 000 LIGNES D'INTERFACE LOCALE (commande de Rodolf)

**Ce que Rodolf a demande** apres la mesure de l'agent du kit (*« ce n'est pas
normal qu'il reimplemente tout »*) : classer mes lignes d'interface en quatre
tas chiffres, et repondre a **pourquoi j'ai ecrit local alors que ca existait**.
**Aucune migration dans ce lot.** Seance sans GPU : tout ce qui suit est de la
**lecture et de la mesure statique**, aucun temoin visuel n'est pris ni
revendique.

**PERIMETRE, enonce avec le resultat** (face 8) : `Applications/NK3DModeler/src/NK3DModeler/Shell/*.h`,
**17 fichiers, 21 960 lignes** — c'est bien la source du « ~22 000 » de la mesure
du kit. **Hors perimetre et declare comme tel** : `Viewport/` (24 008 l., dont
`NkDemo3D.cpp` 18 636 l. = le moteur de la scene, pas de l'interface), `Project/`
(3 922 l.) et `main.cpp` (2 905 l. — verifie : **1 seul appel de dessin**, c'est
de l'orchestration).

### 0. AVANT LES QUATRE TAS — le chiffre de depart surestime d'un facteur 6

Regle du `CLAUDE.md` parent : *« est-ce qu'on a le probleme ? mesure X d'abord »*.
J'ai donc mesure X avant de le decouper. Classement ligne a ligne des 21 960 :

| nature | lignes | part |
|---|---|---|
| commentaires | **5 712** | 26,0 % |
| lignes vides | 515 | 2,3 % |
| **appels de dessin** (`p.Fill`, `p.TextV`, `p.IconV`...) | **1 244** | 5,7 % |
| **zones cliquables** (`hit.Add/Clicked/Hovered`) | **665** | 3,0 % |
| **appels de widget local** | **327** | 1,5 % |
| **geometrie / rectangles** | **1 265** | 5,8 % |
| **RESTE — logique applicative** (etat de scene, projet, actions de menu, conversions) | **12 232** | **55,7 %** |

> **L'interface reelle, c'est 3 501 lignes (15,9 %), pas 22 000.** Le reste est
> ce que l'application FAIT, et aucune bibliotheque d'interface ne le portera
> jamais.

Le decompte brut par primitive le confirme : **1 173 appels de dessin** dans tout
`Shell/` (412 `TextV`, 269 `Fill`, 135 `Outline`, 133 `IconV`, le reste sous 50).
Et la densite le montre par fonction : `PaintPropObject` fait **1 498 lignes pour
59 dessins** — c'est du metier, pas du pixel.

**Ce que cette mesure ne prouve PAS, et ses biais, dans le sens ou ils vont** :
c'est un classement par **expression reguliere, premiere regle gagnante** dans un
ordre fixe. Une ligne qui calcule un rectangle *et* dessine compte comme dessin ;
une ligne de metier qui contient `.x +` compte comme geometrie. **Donc
« geometrie » est surestime et « logique applicative » sous-estime : les 15,9 %
sont une BORNE HAUTE de la surface d'interface.** Le probleme est donc **plus
petit** que le chiffre de depart, jamais plus grand.

**Ca ne dissout pas la commande de Rodolf** — 3 501 lignes d'interface locale,
c'est reel ; `Splitter` est bien reecrit ; et l'audit ci-dessous trouve **355
lignes deja mortes**. Ca la **recadre** : on ne cherche pas a rapatrier 22 000
lignes, on cherche a rapatrier ce qui est dans le tas A.

### 1. TAS A — REMPLACABLE AUJOURD'HUI (une fonction existante, nommee)

Critere retenu, et il est plus dur que « une fonction du meme nom existe » : la
fonction doit **accepter un rectangle explicite** ou **posseder toute sa
surface**, sinon elle ne peut pas se poser ou ma maquette l'exige (cf. §5).

| ce qui est local | lignes | ce qui le remplace | preuve |
|---|---|---|---|
| **`NkModelerFileDialog.h`** — fichier entier | **355** | `editorkit::NkFilePicker`, **deja adopte ET specialise** (`NkModelerPicker : public NkFilePickerState`, `NkModelerMatTypes.h:77`) | **c'est du CODE MORT** : `NkFileDialogOpen` a **une seule occurrence dans tout le depot — sa propre definition**. L'unique `NkFileDialogState` est un statique de fonction (`NkModelerProperties.h:785`) que rien n'ouvre, donc `if (fdlg.open)` (l. 7474) n'est **jamais vrai** et `NkFileDialogPaint` **ne s'execute jamais**. La migration a DEJA eu lieu ; seul le cadavre reste |
| `PaintCloseDialog` + `PaintCloseRecDialog` + `PaintEncodeDoneDialog` | **188** | `editorkit::NkModalFrameDraw` | **deja adopte dans la meme application** pour `matAddModal` (`NkModelerProperties.h:6771`). Meme app, meme besoin : un adopte, trois non |
| `PaintNewProjectDialog` (`NkModelerWelcome.h:269`) | **90** | idem | idem |
| `Combo` + `DrawComboPopup` + `NkComboPending` | **188** | `editorkit::NkComboButton` + `NkComboMenu(..., const NkRect &anchor, ...)` | le composant du kit prend une **ancre rectangulaire explicite** |
| `CheckCombo` + `DrawCheckPopup` + `NkCheckPending` | **110** | `NkComboMenu` + case a cocher | partiel : la multi-selection est a ajouter au composant |
| `PaintOpenMenu` + `PaintModifierMenu` + `PaintAddObjectMenu` | 376, dont **~120 de chrome** | `editorkit::NkCtxMenuDraw` — items, `enabled[]`, sous-menus (`hasSub`), icones par item, **barre de recherche** et defilement | 376 lignes pour **29 dessins** : le chrome est mince, le reste sont les ACTIONS et reste chez moi |
| `PaintSplitters` — le cœur du glisser (`NkModelerChrome.h:31`) | **22** sur 79 | `nkgui::Splitter(ctx, id, handle, vertical, value, min, max)` — **a rectangle explicite** | le cas qui pique, et il est plus nuance que « il l'a reecrit » : voir §5.3 |
| `NkModelerPainter::VScroll` / `HScroll` (`NkModelerUI.h:261`) | **37** | `editorkit::NkVScrollbar` / `NkHScrollbar`, **deja adoptes** | le local ne survit plus que comme repli `if (!guiCtx)` dans `NkPaintVScroll` |

> ### **TAS A = ~1 110 lignes**, soit **32 % de l'interface reelle (3 501 l.)** et **5,1 % des 21 960**.
> **Dont 355 deja mortes** — donc supprimables sans rien migrer, et sans risque.

**Ce que le tas A n'est PAS.** Il ne contient **ni** `DragFloat` (139 l., 220
sites d'appel), **ni** la roue chromatique (145 l.), **ni** les 412 `TextV` :
leurs equivalents NKGui existent bel et bien, mais **ne prennent pas de
rectangle** — ils sont donc dans le tas B, et c'est la tout le sujet du §5.

### 2. TAS B — BLOQUE PAR UN MANQUE NOMME

| ce qui est local | lignes | manque qui bloque |
|---|---|---|
| `DragFloat` (+ **220 sites d'appel**) | **139** | **le point d'entree geometrique** (manque neuf, §5.1) + **jonction des deux themes** (manque n.7 deja transmis) |
| `NkColorWheel` + `NkColorPickerSV` | **145** | idem (`ColorPicker4` existe, sans rectangle) |
| `NkModelerPainter::Outline` (2 rectangles superposes) | **13** | **contour arrondi** (n.1) |
| `NkModelerPainter::Ring` (2 disques) | **22** | **cercle creux** (n.2) |
| `NkModelerPainter::TextClipped` | **33** | **ellipse de troncature** (n.4) — et son tampon est de **64 octets, ASCII** |
| `NkModelerPainter::TextV` / `IconV` / `TextWrap` (+ **545 sites**) | **113** | **texte centre verticalement** (n.5) |
| `NkModelerIcons.h` — rasterisation SVG, une texture par glyphe | **384** | **atlas d'icones** (n.3) — 102 glyphes ici, 91 autres dans NKCode |
| **1 187 litteraux `S(nn.f)`** dissemines | (dans les 1 265 l. de geometrie) | **jetons de metrique** (n.6) |
| l'encodage actif/choisi du navigateur | — | **selection a deux etats** (n.8) |

**Total implementation du tas B : ~849 lignes**, plus les **1 265 lignes de
geometrie** qui n'existent que parce qu'il n'y a pas de jetons de metrique.

**UN NEUVIEME MANQUE, ET IL PRIME SUR LES HUIT AUTRES** — voir §5.1. Il est
transmis a l'agent NKGui au canal.

### 3. TAS C — VRAIE SPECIALISATION DU MODELAGE (reste chez moi)

| ce qui reste | lignes | pourquoi, en une ligne |
|---|---|---|
| `PaintPropObject/Material/World/Output/Scene/Mode/Tool/Modifier` — le CONTENU des pastilles | **6 130** | ce qu'un materiau, un monde ou un rendu **possede** ; aucune bibliotheque ne connait ce vocabulaire |
| `NkModelerState` (`NkModelerInput.h:138`) | **1 025** | l'etat du document et de la session — pur metier |
| `NkModelerImport.h` | **620** | la decomposition a l'import (frontiere = le `model`) ; ce n'est pas de l'interface |
| `NkModelerTables.h` | **444** | catalogues de donnees (projections, ombrages, objets, modificateurs) ; ne peint rien |
| `NkModelerCommon.h` | **270** | nommage de nœuds, unicite des materiaux, depot d'un modele |
| `NkModelerMatTypes.h` | **227** | catalogue des types de materiau **+ la specialisation du selecteur du kit** — c'est le patron d'adoption reussie du depot |
| `NkModelerTheme.h` | **150** | roles de couleur propres au produit (anneau de brosse du sculpt) ; les monter ferait grossir l'enumeration commune a chaque application — garde-fou n.1 de NKGraph |
| `PaintViewport` + surcouches de la vue (gizmo de navigation, matcap, popups de vue) | **1 951** | la vue 3D et ses incrustations sont le produit lui-meme |

> **TAS C = ~10 817 lignes** (49 % du perimetre).

### 4. TAS D — CANDIDATS A MONTER DANS LE KIT (pour les autres editeurs)

| ce qui monte | lignes | qui d'autre en a besoin |
|---|---|---|
| **`NkModelerPainter`** (`NkModelerUI.h:168`) | **~340** | **palier 1 deja convenu** avec l'agent du kit — c'est l'etage reellement partageable aujourd'hui |
| **`NkLayout::Compute`** (`NkModelerUI.h:101`) | **65** | la disposition nommee d'un editeur (menu / onglets / outils / gauche / vue / droite / navigateur / etat + poignees + fractions). **Tout editeur a exactement ca**, et Nogee la reecrit |
| la machinerie de rangee de proprietes — `PaintTransformRow`, `PaintColorRow`, `PaintColorPicker`, `PaintGroupBlock`, `PaintListSection`, `PaintPropGroup`, `PaintPropGroupMenu`, `PaintXformGroup`, `SectionHeader`, `NkPropButton` | **770** | le composite `PropertyRow` **n'existe nulle part** ; 4 copies de panneau de proprietes mesurees par l'agent du kit |
| `PaintBrowser` — le navigateur de contenu | **1 077** | **3 copies** dans le depot ; 6 des 18 ecarts mesures seront ecrits deux fois sinon |
| `PaintHierarchy` — l'arbre de scene | **628** | **3 copies** |
| `PaintJournal` — la console | **261** | **3 copies** |
| `PaintStatus` — la barre d'etat | **225** | le kit a deja un pied de page ; c'est la meme famille |
| `PaintSearch` + `NkNameMatches` — champ de filtre | **117** | navigateur, hierarchie, menus a recherche |
| `NkHitRegistry` (`NkModelerInput.h:1163`) | **321** | **a POSER, pas a affirmer** : son arbitrage par COUCHES (`mHoverLayer`) resout le conflit surcouches/panneaux que `activeId`/`hoveredId` ne traite pas. **Question ouverte a l'agent NKGui** : est-ce un doublon de son arbitrage, ou le manque que ses surcouches ont ? |
| `NkModelerIcons.h` | **384** | aussi en B — c'est l'atlas que le kit reclame |

> **TAS D = ~4 188 lignes** — a monter progressivement, apres le peintre.

**Recapitulatif** : A 1 110 + B 849 + C 10 817 + D 4 188 = 16 964, plus 5 712
lignes de commentaires **reparties dans tous les tas** (26 % du fichier) et 515
vides — le compte se referme sur 21 960 a la marge de recouvrement pres (le
peintre est compte une fois en D, ses fonctions de contournement une fois en B).

---

### 5. LA REPONSE A « POURQUOI AS-TU ECRIT LOCAL ALORS QUE CA EXISTAIT ? »

C'est ce que Rodolf attend, et je ne m'excuse pas : je mesure. **Trois causes,
dont une que ma propre mesure a REFUTEE** — je l'ecris quand meme, parce que
c'etait mon explication la plus seduisante et qu'elle etait fausse.

#### 5.1 CAUSE PRINCIPALE — 105 des 116 fonctions de NKGui se placent elles-memes

**La mesure** : `NkGuiWidgets.h` expose **116 fonctions publiques**. **11**
acceptent un `NkRect` explicite — `Button`, `ButtonEx`, `RepeatButton`,
`Splitter`, `PanelBackground`, `BeginChild`, `BeginListBox`, `BeginMenuBar`,
`BeginPanel`, `BeginDropTarget`, `DockSpace`. **Les 105 autres se posent au
curseur de mise en page** (`NextItemRect` lit `layout.cursor` et impose souvent
la hauteur, parfois la largeur).

**Et mon interface est integralement pilotee par rectangles** : `NkLayout::Compute`
calcule 12 zones nommees, puis chaque panneau place ses rangees en pixels
absolus — **1 173 dessins et 665 zones cliquables**, tous a des coordonnees que
la maquette fixe.

> **Il n'existe aucun `SetNextItemRect` / `SetCursorPos` dans l'API publique.**
> `ctx.layout.cursor` est un champ public et donc ecrivable — mais ce n'est pas
> une API, ce n'est documente nulle part, et ca ne fixe ni la largeur ni la
> hauteur de l'item.

**C'est le neuvieme manque, et il prime sur les huit autres** : les huit debloquent
un dessin chacun ; **celui-la debloque 105 fonctions d'un coup**. Sans lui,
completer la liste produira des fonctions correctes que personne n'appellera —
exactement le sort actuel de `Splitter`.

#### 5.2 CAUSE — la jonction des themes manque, et son absence a deja force une violation

**Mesure** : `ui.theme` / `ctx.theme` : **ZERO occurrence dans toute l'application**.
Rien ne pousse mon `NkTheme` (29 roles) dans `NkGuiTheme` (28 couleurs). Donc
**tout widget NKGui appele aujourd'hui peindrait dans une palette par defaut sans
rapport**, et ne suivrait pas le theme clair.

**La preuve qu'elle coute vraiment, et elle est chez moi** : `NkPaintVScroll`
(`NkModelerScreens.h:964`) adopte `NkVScrollbar` — et doit lui injecter ses
couleurs **a la main** par `NkScrollbarUserSkin()`, avec **4 `NkColor{...}` en
dur**. Dans une application dont le peintre affiche en tete *« pas un seul
0xRRGGBB »*. **La deconnexion des themes a force une violation de la regle des
couleurs pour pouvoir adopter un composant partage.**

C'est le manque n.7 deja transmis ; **cette mesure le fait passer en tete**, juste
derriere le point d'entree geometrique.

#### 5.3 LE CAS `Splitter` — ce n'est pas de l'indiscipline, et ce n'est pas non plus une excuse

Trois faits, dans l'ordre ou ils comptent.

**a) La fonction etait INTROUVABLE PAR LE CHEMIN QUE LES REGLES PRESCRIVENT.**
Les regles disent : *« avant d'ecrire un composant, cherche s'il existe deja ; la
recherche coute deux minutes »*. La porte d'entree d'une bibliotheque, c'est son
`README.md`. Celui de NKGui dit, **aujourd'hui encore** :

> `## État : Phase 1 — squelette` ... `⏳ Phases 3-6 : widgets, fenêtres/interaction, docking`

**Il annonce que les widgets ne sont pas ecrits.** Or `NkGuiWidgets.h` en porte
**116**, dont `Splitter`. Les dates tranchent :

| evenement | date | commit |
|---|---|---|
| `Splitter` ajoute a NKGui | **2026-06-26** | `b055449f` |
| le README qui dit « widgets ⏳ » — **meme commit** | 2026-06-26 | `b055449f` |
| dernier commit du README depuis | **jamais** | — |
| `PaintSplitters` ecrit dans NK3DModeler | **2026-07-31** | `16ce485d` |

**Cinq semaines plus tard.** Et le mot « Splitter » n'apparait dans
`ARCHITECTURE.md` que sous la forme `DragSplitter`, une valeur d'enumeration
d'interaction — jamais comme fonction disponible. **Il n'existe aucun inventaire.**
Un agent qui a fait la verification de deux minutes s'est fait repondre que la
chose n'existait pas. C'est le point 2 du remede de Rodolf (*« rendre l'existant
trouvable »*), et il est **mesure** : le correctif le moins cher du dossier est
de mettre le README a jour.

**b) Meme trouvee, elle n'aurait pas fait le compte — et il faut le dire aussi.**
`nkgui::Splitter` glisse une **valeur en PIXELS** par accumulation de
`mouseDelta`. Mes quatre separateurs glissent une **FRACTION** de la fenetre,
avec un **signe** (deux croissent, deux decroissent) et une **portee** de
reference differente (largeur, hauteur, ou la hauteur du panneau droit) — parce
que la disposition doit se retrouver identique a la reouverture **quelle que soit
la taille de la fenetre**. Il manque aussi la condition « vivant » (un separateur
meurt quand son panneau est replie). **Sur 79 lignes, `Splitter` en couvre 22.**
Et il aurait peint avec `ctx.theme.border` (cf. §5.2), donc de la mauvaise
couleur.

**c) Ca reste un doublon, et je ne le defends pas.** 22 lignes de comportement de
glisser existaient et ont ete reecrites. Le tas A les compte.

#### 5.4 LA CAUSE QUE J'AI CRUE, ET QUE MA PROPRE MESURE A REFUTEE

J'allais ecrire — c'etait propre, mecanique et faux : *« le peintre porte la
`NkGuiDrawList`, pas le `NkGuiContext` ; appeler un widget exigerait de faire
descendre le contexte dans 65 signatures »*. Les 65 signatures sont exactes.

**La conclusion ne l'est pas** : `NkUiCtx()` (`NkModelerWidgets.h:35`) est un
accesseur global **pose une fois par frame** (`main.cpp:1068`) qui rend le
contexte disponible **partout, a cout nul**. Il est deja utilise a **11
endroits** — c'est par lui que `NkTooltip`, `NkOverlayTextField` et `NkVScrollbar`
sont appeles.

> **Il n'y a donc AUCUNE barriere d'acces.** Ce que je prenais pour un cout
> d'infrastructure etait un cout que j'avais deja paye et oublie.

C'est la face « retirer une premisse ne retire pas ses conclusions », prise a
l'endroit : la premisse tombe, et **le tas A grossit** — plus rien ne protege les
1 110 lignes.

*(Au passage, pour l'agent du kit : `NkUiCtx()` et `NkOvPainter()` sont deux
globales de processus de la meme famille que `gUiScale` — a traiter avec lui au
moment de la montee du peintre.)*

#### 5.5 LA LOI, SOUS SA FORME UTILISABLE

Quatre briques partagees adoptees (`NkFilePicker`, `NkVScrollbar`,
`NkOverlayTextField`, `NkModalFrameDraw`) contre huit reecrites. **Le
discriminant n'est ni la connaissance, ni la discipline** :

> **Une brique partagee est adoptee quand (1) elle accepte un rectangle explicite
> ou possede toute sa surface, ET (2) elle laisse l'appelant injecter ses
> couleurs. Aucune des deux n'est negociable, et il suffit qu'une manque pour que
> la reecriture locale soit moins chere.**
>
> Etat mesure de NKGui : **(1) vrai pour 11 fonctions sur 116** ; **(2) vrai pour
> aucune**. Etat de NKEditorKit : **(1) vrai pour tous ses composants** — et c'est
> exactement ceux-la que j'ai adoptes.

**Ce que ca predit, et c'est verifiable** : `NkVScrollbar` satisfaisait (1) mais
pas (2) — il a ete adopte **avec un contournement** (les 4 couleurs en dur). Une
brique qui satisfait (1) et pas (2) est adoptee **et salit l'appelant** ; une
brique qui ne satisfait pas (1) n'est pas adoptee du tout.

**Ce que ca vaut pour Nogee et NkAnimaEditor** : leur reecriture ne s'expliquera
pas plus par l'indiscipline que la mienne. Tant que (1) et (2) sont faux, la
regle sera contournee — non par choix, mais parce que l'adoption ne compile pas
la ou la maquette la demande.

---

### 6. CE QUI N'EST PAS FAIT DANS CE LOT, ET POURQUOI

- **Aucune migration.** C'etait la consigne : audit seulement.
- **L'extraction de `NkModelerPainter` vers `Engine/NKEditorKit/` n'est PAS
  commencee.** Ce que l'agent du kit attend a l'arrivee est lu et enregistre
  (trois globales -> membres : `gUiScale`/`S()`, `NkPopupBoundsW/H()`,
  `NkModelerIcons` concret ; ensemble d'icones injecte par poignee opaque ;
  garder `Px`/`PxRect`, zero couleur en dur, et **marquer `Outline`/`Ring` comme
  contournements temporaires**). **A ajouter a sa liste** : `NkUiCtx()` et
  `NkOvPainter()` sont deux globales de processus supplementaires du meme
  fichier. L'extraction demande un temoin visuel (« rien n'a change chez moi »)
  que **le GPU indisponible interdit** : elle est **differee et nommee**, pas
  oubliee.
- **Les 355 lignes mortes de `NkModelerFileDialog.h` ne sont pas supprimees.**
  Une suppression est irreversible et sort du perimetre « aucune migration » ;
  elle est proposee, elle n'est pas faite.
- **Contrat d'import (d) materiaux/textures et (e) dialogue : toujours pas
  commence** (inchange depuis Q58).

### 7. LE DEVIS DE L'EXTRACTION DU PEINTRE — chiffre, pour que la prochaine seance parte d'un nombre

L'agent du kit demande trois etats globaux transformes en **membres** (son
exigence A), faute de quoi il devra rouvrir l'extraction. J'ai mesure le cout de
chacun **avant** de commencer, et le resultat concentre tout le chantier sur un
seul point.

| son exigence | sites d'appel chez moi | cout |
|---|---|---|
| `NkPopupBoundsW()` / `NkPopupBoundsH()` -> membres | **8** | trivial |
| `NkModelerIcons` concret -> poignee opaque | **403 `NkIcon::`**, mais ils restent chez moi ; seul le **parametre du constructeur** change | faible |
| **`gUiScale` + `S(px)` -> membre** | **1 191 sites** (1 187 dans `Shell/`) | ⚠️ **c'est tout le chantier** |
| *(a ajouter a sa liste)* `NkUiCtx()` + `NkOvPainter()` | **14** | faible |

> **L'extraction du peintre, c'est un probleme a une variable : `S()`.**

**Et il y a un arbitrage a trancher AVANT de toucher une ligne**, parce que les
deux issues n'ont pas le meme cout ni la meme valeur :

- **(i) `S` devient une methode du peintre.** Satisfait litteralement l'exigence
  (deux fenetres a deux facteurs DPI), mais demande **1 191 reecritures** — et
  surtout `S()` est appelee la ou **aucun peintre n'est en portee** :
  `NkLayout::Compute` est une methode d'une structure de disposition, et
  `NkModelerTables.h` s'en sert pour ses metriques sans rien peindre. Cette issue
  n'est donc pas une simple substitution : elle oblige a faire circuler le
  peintre dans du code qui ne dessine pas.
- **(ii) `S` reste libre mais lit une echelle portee par un contexte injecte.**
  Cout de reecriture proche de zero, mais ca ne satisfait l'exigence que si
  l'echelle vit dans un objet atteignable depuis les deux fenetres — donc ca
  deplace le probleme au lieu de le supprimer.

**Je ne tranche pas seul** : c'est le kit qui recoit et generalise, et l'issue
choisie determine la forme de ce qu'il recoit. Question posee au canal (Q59 §7).

**Pourquoi l'extraction n'est PAS commencee dans ce lot, en une phrase** : une
transformation qui touche 1 191 sites n'a de valeur que si je peux prouver
« rien n'a change chez moi » — et cette preuve est un **temoin visuel**, que le
GPU indisponible interdit. Differee et nommee, pas oubliee. Ce devis est ce que
je peux livrer sans fenetre, et il fait gagner la phase de decouverte a la
seance suivante.

### 8. CE QU'UN CONTROLE « INUTILE » A RENVERSE — trois faits, dont un chiffre du `CLAUDE.md` parent

J'avais ecrit au §5.1 la phrase *« le sort actuel de `Splitter` : une fonction
correcte que personne n'appelle »*. Avant de la laisser se propager, j'ai verifie
qu'elle etait vraie. **Elle l'est** — `nkgui::Splitter` a **zero appelant hors de
NKGui**, contre-epreuve faite (le grep remonte bien 163 occurrences de
« Splitter » dans le depot : il sait trouver). **Et le controle a renverse trois
autres choses.**

#### a) « NKCode : 0 appel [NKGui] » est FAUX — c'est 164

Le chiffre figure dans le bloc de regle du `CLAUDE.md` parent. Mesure :

| | |
|---|---|
| `Applications/NKCode/` inclut `NKGui/NKGui.h` | oui, au moins 5 fichiers |
| `using namespace nkentseu::nkgui;` | oui, au moins 3 fichiers |
| appels de widget NKGui **reels** | **164** — `MenuItem` **151**, `Selectable` 10, `InputText` 2, `BeginCombo` 1 |

**Cause du zero, mecanique** : un `grep "nkgui::"` ne voit que les appels
**qualifies**. Sous une directive `using`, `MenuItem(ctx, ...)` est invisible a ce
grep — et c'est pourtant un appel a NKGui. *(Faux positif ecarte : NKCode a aussi
son peintre local `Shell/NkUi.h` ; je n'ai compte que la forme `X(ctx, ...)`, qui
ne peut pas viser ses methodes.)*

⚠️ **Mon propre 11 tient, et je l'ai verifie sur moi AVANT de contester** :
`using namespace nkgui` -> **aucune occurrence** dans NK3DModeler, qui n'importe
nommement que **cinq TYPES** et aucune fonction. **Les quatre tas ne bougent pas.**

**Je ne corrige pas le `CLAUDE.md` parent moi-meme** : ce chiffre y justifie une
regle de Rodolf, et le remplacer **change l'argument**. Signale au canal (Q60).

#### b) ⭐ LA CORRECTION DONNE LE GROUPE TEMOIN QUI PROUVE LA LOI DU §5.5

« NKCode : 0 » servait a dire *« personne n'utilise NKGui »*. Le vrai chiffre dit
mieux :

> **Sur les 164 appels de NKCode, 151 sont `MenuItem`** — precisement la fonction
> qui **possede toute sa surface** (un menu se place lui-meme ; sa position n'est
> jamais disputee par une maquette). C'est la seconde branche de la condition (1)
> de la loi, verifiee sur une autre application.

Et l'autre branche se verifie sur la meme application : **la ou il lui faut des
panneaux au pixel, NKCode a ecrit son PROPRE peintre** (`Shell/NkUi.h`, a
rectangles et couleurs injectees) — **la meme forme que `NkModelerPainter`**, sans
que nos deux applications se soient jamais parle.

**Reformulation exacte du diagnostic** : ce n'est pas *« les applications ignorent
NKGui »*, c'est **« elles appellent NKGui exactement la ou le placement n'est pas
dispute, et elles ecrivent un peintre local partout ailleurs »**. Deux
applications, meme partage : un groupe temoin, pas une coincidence. *Et ca rend le
peintre encore plus evidemment le bon palier 1 — il a ete ecrit deux fois.*

#### c) ⚠️ DETTE URGENTE — le separateur A RATIO existe dans NKUI, et l'extinction va l'emporter

`NkUILayout::DrawSplitter` (`Kernel/Runtime/NKUI/src/NKUI/NkUILayout.cpp:286`) :

```cpp
bool DrawSplitter(NkUIContext &ctx, NkUIDrawList &dl, NkRect rect, bool vertical,
                  float32 &ratio, NkUIID id)
```

**Un rectangle ET un `float32 &ratio`** — pas des pixels. Plus une **zone de
prehension elargie** (`grabHw`), justifiee dans son commentaire par la meme raison
que la mienne. **C'est `PaintSplitters`, ecrit avant `PaintSplitters`, dans la
bibliotheque qu'on eteint.** Sa remplacante `nkgui::Splitter` glisse des pixels et
n'a aucune zone elargie.

> **La reecriture NKUI -> NKGui a fait REGRESSER ce widget sur ses deux
> caracteristiques utiles.** Ma reecriture locale n'est donc pas un defaut de
> recherche : c'est la reconstruction d'une fonctionnalite **supprimee**.

**Ce que ca vaut, et pourquoi c'est date** : une surcharge a ratio plus une zone
elargie, deja ecrites, a reprendre dans NKGui. **NKUI est en cours de suppression
— une fois le fichier retire, la version qui marchait part avec.** Signale au
canal pour l'agent qui mene l'extinction : *verifier ce que NKGui n'a pas repris
avant de retirer `NkUILayout.cpp`*. `DrawSplitter` est un cas prouve ; je n'ai pas
balaye le reste, **je n'affirme donc rien au-dela de celui-la**.

---

## 2026-08-23 — Attributs par face : la regle de fusion est cablee, et **la decimation ne fusionnait rien, elle effacait tout**

### Ce qui est FAIT, et mesure

| chantier | etat |
|---|---|
| dette du repertoire de lancement | **CLOSE** — 12 lancements, 3 bancs x 4 repertoires (dont un hors du depot), tous code 0 |
| materiau par face a travers la decimation | **transporte** (`slot1 2 -> 0` etait un effacement, pas une regle manquante) |
| regle de fusion du materiau | **cablee** dans `Quadify` et `DissolveSelected` |
| ce qui se perd dans une fusion | **compte**, pas suppose |
| ombrage (`smooth`) herite de la face mere | **fait**, par la MEME entree que le materiau |
| reference du harnais | 232 -> **250** lignes |

### Les trois faits qu'il faut connaitre avant de toucher a ce code

**1. `NkMeshDecimate` NE FUSIONNE AUCUNE FACE.** QEM contracte des aretes, ce qui
en SUPPRIME. Le materiau s'y transporte un pour un ; la regle de fusion n'y a
aucun site. Les vrais sites sont `Quadify` (2 -> 1) et `DissolveSelected`
(N -> 1). *Chercher la fusion la ou l'arbitrage la supposait a evite de cabler
une regle dans une fonction qui n'en avait pas besoin — et a trouve le vrai
defaut, qui etait l'absence de transport.*

**2. LE CONTOUR NE SUFFIT PAS A CONNAITRE LES CONTRIBUTEURS D'UN DISSOLVE.** Une
face **entierement interieure** a la region (toutes ses aretes retirees)
n'apparait sur aucun contour. D'ou le regroupement par **composante connexe** des
aretes retirees. Mesure de la mutation qui retire l'union : resultat plausible
(`retenu=5` au lieu de `1`) **et** `perdus=0` — une attestation que rien n'a ete
perdu. Ne pas « simplifier » ce passage en relisant le contour.

**3. `smooth` NE SURVIVAIT PAS, contrairement a ce qu'affirmait le commentaire de
`Face::material`.** La re-derivation par les normales de coin n'existe que dans
`BuildFromIndexed` ; `BuildFromPolygons`, par lequel passe **toute** operation
d'edition, n'a pas de normale de coin a interroger. Mesure : 12 faces lisses -> 0
apres subdivision. Corrige par `NkEditMesh::FaceAttrib`.

> **Un attribut qui « survit » parce qu'un AUTRE chemin le reconstruit ne survit
> qu'aux operations qui passent par ce chemin-la.**

### Un seul canal pour tous les attributs par face

`ToPolygons` / `BuildFromPolygons` transportent desormais un
`NkVector<NkEditMesh::FaceAttrib>` (materiau + ombrage) et non plus un
`NkVector<uint16>`. **Le troisieme attribut s'ajoute dans cette structure et suit
la meme parente sans qu'aucune operation ne soit modifiee.** Ne PAS ajouter un
second tableau parallele : chaque operation devrait alors repondre deux fois a
« de quelle face vient cette face ? », et deux reponses divergent.

⚠️ **Deux regles d'AGREGATION, une seule table de PARENTE.** `smooth` fusionne par
le OU (un booleen dont la reunion a un sens) ; le materiau par la dominance en
aire (l'union de deux couleurs n'est pas une couleur). Uniformiser les deux ferait
basculer a FLAT une face lissee des qu'un voisin plat et plus grand la rejoint.

### La regle de fusion, telle qu'elle est cablee

Contributeur **dominant par l'aire**, egalite tranchee par l'**indice de materiau
le plus bas**, tolerance relative `1e-6` sur l'egalite d'aire (sans elle, le
departage dependrait de l'arrondi, donc serait non deterministe pour un
utilisateur qui voit deux triangles identiques).

⚠️ **ECART AVEC LE TEXTE DE L'ARBITRAGE, ASSUME ET SIGNALE** : « la face qui
apportait la plus grande aire » et « la couleur qui couvrait le plus » coincident
sur DEUX contributeurs et divergent sur N. **On retient la COULEUR** (aires
cumulees par index). Cas temoin : `matfus/dissolve-couleur-majoritaire`.

### Ce que les bancs mesurent, et pourquoi ils sont faits comme ca

La regle a **deux criteres** : un cas ou ils sont d'accord n'en teste aucun.
Chaque site est mesure **criteres en desaccord** puis **en egalite**.

⚠️ **Piege rencontre** : la premiere version de `dissolve-couleur-majoritaire`
mettait la majorite en premier ; une mutation supprimant toute comparaison
d'aires la laissait **verte**, le bon resultat tombant par accident.

⚠️ **Et un COMPTE ne prouve pas une PARENTE.** Sous la mutation « mauvais parent »,
`smooth/mixte-subdivision` reste vert (12/24 dans les deux cas) ; seule la
**signature de position** de `smooth/mixte-position` la voit. Mesurer **ou** sont
les faces, pas seulement combien.

### Reste ouvert

- **`BevelSelected` et `ExtrudeSelectedEdges`** : seules operations sans
  transport. Elles **creent** des faces sans face mere. Question produit : une
  face neuve entre deux materiaux prend-elle l'un des deux, ou le slot 0 ?
- `NkRef` ne compile pas (`NkImage::Resize` / `NkImage::Free`) — **verifie
  present sur `main` sans mes changements**, appartient a NKImage/NkRef.
- `NkSLCheck` lit ses shaders par chemin relatif et fait `return 0`
  inconditionnellement : son code de sortie ne signale jamais rien.
