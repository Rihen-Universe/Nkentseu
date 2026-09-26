# NKCraft — ce qu'on attend de l'application

### Document 1 — Spécification produit

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
> Rédigé le 26/09/2026. **NKCraft est le nouveau nom de NK3DModeler.**
>
> **Ce document ne remplace pas** `docs/SPECIFICATION.md` (31/07) ni
> `docs/UI_SPEC.md` : ce sont des documents qui **tranchent**, et ils tranchent
> bien — périmètre, parité Blender, sculpt pixol, graphes de nœuds, prompt en
> cycle, add-ons C++, contraintes. **Il les complète** sur quatre points qu'ils
> ne pouvaient pas connaître :
> 1. la décision des **trois applications** (NKCraft / NkAnima / NKScena, 23/08) ;
> 2. la **famille** : même thème, mêmes jetons, mêmes composants `.nkgui` que
>    NkAnima, NKScena et Nogee ;
> 3. ce qui **existe réellement** au 26/09 (l'application tourne, son interface
>    fait ~34 000 lignes codées à la main) ;
> 4. le **passage de l'interface aux documents `.nkgui`**.
>
> **En cas de désaccord, `SPECIFICATION.md` et `UI_SPEC.md` gagnent** — sauf sur
> les quatre points ci-dessus, où ce document est plus récent.
>
> Compagnons : **02** (interface pour un humain), **03** (interface pour l'agent).

---

## 0. En une phrase

**NKCraft est l'atelier où naissent toutes les formes de Rihen — personnages,
objets, décors, environnements — à la main, par sculpture, par nœuds ou par l'IA,
et où l'humain reprend toujours le travail de l'IA, comme l'IA reprend celui de
l'humain.**

---

## 1. Le nom

**NKCraft** : *craft*, le métier, le travail de la main. Le nom dit ce que
l'application défend (la main garde la main, même quand l'IA aide) et il n'est
plus limité à « 3D modeler » alors que l'atelier sculpte, texture et génère.

| ce qui change | ce qui ne change pas |
|---|---|
| le nom de l'application, de sa cible Jenga, de son dossier, de ses documents | les formats déjà écrits (`.nk3dm`, `.nkmesh`, `.nkmat`) **tant qu'une décision de migration n'est pas prise** (D1) |
| les préfixes de thème `nk3d.` → `nkcraft.` (alias gardé en lecture) | les raccourcis, les commandes, le journal |

⚠️ Renommer un format lu par d'autres applications casse leurs lecteurs : c'est
pourquoi D1 est une décision séparée, et non un effet du renommage.

---

## 2. Positionnement

La décision d'`UI_SPEC.md` §0 reste **la** règle : **apparence d'Unreal Engine 5,
gestes de Blender**, et **la facilité gagne** chaque fois qu'elles se
contredisent (les cinq règles vérifiables : tout icône a un mot, le rare est
replié, les états vides parlent, cinq actions sans menu, une information à un
endroit).

| référence | ce qu'on lui prend |
|---|---|
| **Blender** | les gestes, le vocabulaire, objet ↔ édition, la pile non destructive, la parité de geste |
| **ZBrush** | la sculpture 2.5D « pixol », dont le coût dépend de l'écran et non des triangles |
| **Unreal Engine 5** | l'apparence, la disposition, le navigateur de contenu |
| **Houdini** | la modélisation **par nœuds** qui écrit des commandes rejouables |
| **Character Creator** | le **générateur de personnages** paramétrique (§4.6) |

---

## 3. Frontières

Décision de Rodolf (23/08) : **trois applications, communiquant par fichiers.**

```
NKCraft ──(maillages, matériaux, textures)──▶ NkAnima ──▶ NKScena ──▶ film
   │                                              │
   └──────────────────────────────────────────────┴──▶ Nogee ──▶ jeu
```

| dans NKCraft | ailleurs |
|---|---|
| **modéliser** : objet, édition (sommets / arêtes / faces), modificateurs | **squelette, poids, animation** → NkAnima (le doc 07 de NkAnima : le rig se fait là-bas) |
| **sculpter** : les deux modes (maillage, 2.5D pixol) | **simulation** (tissu, fluides, destruction) → NkAnima |
| **retopologie**, décimation, niveaux de détail | **mise en scène, rendu final** → NKScena |
| **UV** : dépliage, coutures, empaquetage | **placer dans un niveau** → Nogee |
| **matériaux** : graphe de matériau, bibliothèque | |
| **textures** : peinture sur le modèle, **cuisson** (normales, occlusion, courbure…) | |
| **nœuds de modélisation** (procédural) | |
| **générateurs** : personnages, végétation, rochers, bâtiments modulaires | |
| **IA** : opératrice (commandes) et génératrice (GenIA) | |
| **aperçu de présentation** : lumières, ciel, caméras, captures, **tour de présentation** | |

### 3.1 Trois frontières à trancher, et la position proposée

| # | question | proposition | raison |
|---|---|---|---|
| F1 | Le ROADMAP dit « modélisation et sculpture — **et rien d'autre** ». Les **UV, matériaux et textures** sont-ils dans NKCraft ? | **oui** | un asset sort de l'atelier **fini** : sans UV ni matière, NkAnima et Nogee recevraient des formes grises. Le « rien d'autre » visait l'animation et la scène, que ce document laisse dehors |
| F2 | La **création de vêtements** (patrons cousus, comme Marvelous Designer) — NKCraft ou NkAnima ? | **NKCraft** crée le vêtement (patron, couture, drapé au repos) ; **NkAnima** le simule en mouvement | la frontière forme / mouvement, appliquée telle quelle. `NkGarment` existe dans NKPhysics |
| F3 | Le **toilettage des cheveux et poils** (guides, coupe, densité) | **NKCraft** (la forme), **NkAnima** (la dynamique) | idem |

### 3.2 Ce que NKCraft garde de la « mise en scène »

Le panneau **Rendu** existant (ambiance, brouillard, ombres, ciel), les
**caméras**, la **capture** paramétrée, l'**incrustation de webcam** et
l'enregistrement : ils restent, parce qu'ils servent à **présenter un asset** (une
planche, un tour à 360°, un tutoriel filmé) — pas à faire un film. La limite :
**un seul asset ou une petite scène de présentation**. Au-delà, c'est NKScena.

---

## 4. Ce que l'on fait dans NKCraft

### 4.1 Modéliser (l'existant, et Jalon B de SPECIFICATION §6)

Modes objet ↔ édition, sélection (clic, zone, boucles, élément actif distinct),
transformations (pivots, orientations, aimantation), ~40 opérations d'édition,
**17 modificateurs** en pile réordonnable, ombrage, import des 7 formats de
maillage (+ glTF, FBX), journal de commandes rejouable, annulation complète.
**Tout cela est défini par SPECIFICATION §4.1** et n'est pas répété ici.

📌 Les règles des modes posées par Rodolf (27/08) — *sculpture et sculpture 2.5D
sont deux modes, pas un mode et son option* ; *un sous-mesh est une composante
connexe* ; *la frontière entre deux fichiers est le model, pas le sous-mesh* —
sont dans le ROADMAP et **font foi**.

### 4.2 Sculpter

| mode | ce qu'il sculpte | appui |
|---|---|---|
| **Sculpture** (maillage) | les sommets — détail durable, exportable | `NkMeshSculpt`, `NkSculptTileStore` |
| **Sculpture 2.5D** (pixol) | un G-buffer écran — croquis, recherche de forme, détail sans limite de triangles | `NKRenderer/Tools/PixolSculpt` |
| **Volume** (voxels) | une forme sans topologie — ébauche libre, booléens organiques | `NkVoxelSystem` |

Brosses (élever, creuser, lisser, pincer, gonfler, aplanir, masque, peindre…),
rayon, force, cinq courbes d'atténuation, symétrie, **masques**, **multirésolution**
(à trancher entre multirésolution et topologie dynamique — SPECIFICATION §4.3).

**Passer d'un mode à l'autre** est une commande : voxels → maillage (remaillage),
pixol → maillage (projection du détail en carte de normales ou en déplacement).

### 4.3 Retopologie, décimation, niveaux de détail

- **Retopologie** automatique (quads, densité cible, respect des arêtes vives) et
  **manuelle** (dessiner des quads sur la surface sculptée). Appui : `NkMeshRetopo`.
- **Décimation** (triangles cibles, préserver les UV et les bords). Appui :
  `NkMeshDecimate`.
- **Niveaux de détail** pour le jeu : chaîne générée, **aperçu par distance**.

### 4.4 UV, matériaux, textures

- **UV** : dépliage automatique (`NkUVUnwrap`), **coutures** marquées à la main,
  empaquetage, vérification de l'**étirement** (damier), plusieurs jeux d'UV.
- **Matériaux** : l'éditeur de **graphe de matériau** — **la même brique que Nogee**
  (`NkMatGraph`, décision d'`UI_SPEC.md` §10ter et du doc 04 de Nogee) ;
  bibliothèque de matières (métal, bois, peau, tissu, pierre, **tissus
  africains** — wax, kente, ndop, bogolan — en matières de départ), plusieurs
  matériaux par model et par sous-mesh (ROADMAP, chantier Matériaux).
- **Textures** : **peindre sur le modèle** (couleur, rugosité, métal, émission ;
  calques, masques, pochoirs) et **cuire** (haute → basse définition : normales,
  occlusion, courbure, épaisseur, identifiants). Appui : `Tools/NkTexBake`.

### 4.5 Modéliser par nœuds

SPECIFICATION §4.4, inchangée : **le graphe écrit la même liste de commandes que
la main**. Substrat `NKGraph` (couche 1 réelle aujourd'hui), canevas partagé de
l'Editor Kit (couche 2). **Matériaux d'abord, modélisation ensuite** (« règle des
deux consommateurs »).

### 4.6 Les générateurs

Des **graphes de nœuds prêts à l'emploi**, présentés avec des curseurs : la
personne règle des curseurs, NKCraft écrit des commandes, et le résultat est
**un modèle ordinaire, éditable à la main**.

| générateur | réglages | pourquoi |
|---|---|---|
| **Personnage** | morphologie (taille, corpulence, musculature), âge, **traits du visage** (des centaines de curseurs regroupés), peau, cheveux de base | un personnage de départ en minutes ; il part ensuite chez NkAnima **avec une topologie prévue pour se déformer**. **Toutes les morphologies humaines sont représentées d'office**, africaines comprises — un générateur qui ne sait faire que des visages européens est un générateur faux |
| **Végétation** | espèce (arbre, palmier, arbuste, herbe), âge, forme, densité du feuillage | les décors et la dispersion de Nogee / NKScena |
| **Rochers et falaises** | taille, érosion, strates | idem |
| **Bâtiments modulaires** | un **kit** de pièces (murs, sols, portes, toits) aux mesures communes | des niveaux de jeu et des décors assemblés |

### 4.7 L'IA — les deux sortes

La demande de départ de Rodolf : *« modélisation manuelle par l'homme et
modélisation automatique par l'IA, où l'homme peut modifier le travail de l'IA et
vice versa »*. Deux IA y répondent, et il faut les distinguer :

| | **IA opératrice** | **IA génératrice** |
|---|---|---|
| **ce qu'elle produit** | des **commandes** (SPECIFICATION §4.5) | **un maillage** (image ou texte → forme) |
| **exemple** | « chanfreine les arêtes du dessus, ajoute trois marches » | une photo de masque → un masque en 3D |
| **l'humain reprend ?** | oui, par construction : ce sont des commandes | oui : le maillage **entre dans le journal** comme **une** commande (« importer la génération n° 12 »), ensuite tout est manuel |
| **l'IA reprend l'humain ?** | oui : le prompt suivant voit le journal, retouches comprises | oui, **par l'IA opératrice** : « nettoie cette forme », « retopologise », « déplie les UV » — sur le maillage retouché |
| **état** | ossature prête ; **la traduction prompt → commandes n'est pas commencée** | **pont livré** : `NkGeniaImport` → générateur externe (`Tools/Genia/genia_triposr.py`) → glTF → import. **La fusion mono-vue est morte et mesurée** (ETAT_REPRISE_GENIA3D) ; la suite est un modèle **multi-vues à entraîner** |

**Règles** (communes à la famille) : commandes existantes seulement ; **sortie
validée par schéma** ; **aperçu** avant application ; **annulation par lot** ;
prompt et réponse **journalisés** ; **repli dit** quand le modèle est absent ; la
**licence voyage avec les poids** (le manifeste GenIA porte déjà la frontière
R&D interne / produit diffusé, 17/08).

⚠️ **Le générateur externe actuel** est un processus Python tiers. C'est
acceptable pour la **R&D** ; pour le **produit diffusé**, la règle « from scratch »
(SPECIFICATION §5.3) et la frontière de licence imposent soit un modèle NKAI
entraîné par Rihen, soit une décision écrite (D3).

### 4.8 Scripts et extensions

SPECIFICATION §4.6, inchangée : add-ons C++ rechargés à chaud, **qui émettent des
commandes** ; deux briques à écrire (types de modificateurs ouverts par clé
textuelle, cycle de vie d'add-on).

---

## 5. Ce qui existe au 26/09/2026

| pièce | état |
|---|---|
| application | **tourne** ; ~83 000 lignes, dont **~34 000 d'interface codée à la main** (`Shell/`) et un hôte de vue de ~26 000 lignes (`NkDemo3D.cpp`) |
| moteur de maillage | 17 modificateurs, ~40 opérations, **journal rejouable prouvé au bit près**, harnais (111 à 116 cas selon les dates) |
| sculpture | pixol : « squelette » (DESIGN.md) ; `NkMeshSculpt`, `NkVoxelSystem` : présents dans NKRenderer |
| UV, retopo, décimation | présents dans NKRenderer (`NkUVUnwrap`, `NkMeshRetopo`, `NkMeshDecimate`), bancs dédiés |
| matériaux | `NkMaterialSystem` (79 méthodes, 0 banc en août) ; changement de type sans réinitialisation livré (22/08) |
| nœuds | `NKGraph` : ~1 500 lignes réelles ; `NkMatGraph` et sa démo |
| IA | pont GenIA livré et témoin écrit ; traduction prompt → commandes non commencée |
| interface | panneau droit (maquette Banani : pastilles Modèle / Rendu / Scène / Modificateur), navigateur de contenu, hiérarchie, sélecteur de couleur modal, caméras, capture ; **menus : 19 entrées mesurées, 7 câblées, 11 grisées** (26/09) |
| **documents `.nkgui`** | **aucun** — c'est le chantier du §6 |

---

## 6. Le passage aux documents `.nkgui`

C'est le chantier que Rodolf a nommé (« refaire les interfaces avec
NKUIDesign »). **Il ne se fait pas d'un bloc.**

### 6.1 La méthode : panneau par panneau, derrière un témoin

1. Choisir **un** panneau (ordre au §6.2).
2. Écrire son document `.nkgui` (document 03).
3. Le monter **à côté** de l'ancien, derrière un interrupteur (`NK_CRAFT_UI=doc`).
4. **Mesurer** les deux : mêmes actions servies, mêmes valeurs affichées, même
   comportement au clic — le témoin compare les **actions émises** et les
   **valeurs liées**, pas les pixels (une différence de pixels n'est pas une
   régression ; une action perdue, si).
5. Basculer par défaut ; garder l'ancien **une version** ; puis **supprimer**
   l'ancien code (le ROADMAP a déjà nommé le coût d'un code mort non supprimé :
   `NkViewport3D.cpp`, 2 649 lignes).

### 6.2 L'ordre

| # | panneau | pourquoi à ce rang |
|---|---|---|
| 1 | **barre de menus** | 19 entrées déjà mesurées ; aucune logique propre ; le plus sûr |
| 2 | **barre d'outils** et **barre d'état** | petites, visibles, communes avec la famille |
| 3 | **navigateur de contenu** | le panneau commun `bibliotheque` + `Host` d'extension (Nogee 06 §0.3) — il sert déjà de modèle aux cartes d'assets |
| 4 | **hiérarchie** | panneau commun `scene` + filtres NKCraft |
| 5 | **Propriétés / Détails** | les **lignes engendrées** (Nogee 06 §0.2) : les modificateurs sont **déjà** générés depuis `NkModParam` (UI_SPEC §7) — c'est le cas d'école |
| 6 | **panneau T** et **dernière opération** | réglages d'outils, générés depuis `NkModParam` |
| 7 | **Rendu**, **Scène** (pastilles) | |
| 8 | la **vue** et ses surimpressions | la dernière : c'est un `Host`, sa barre seulement est un document |

---

## 7. Phasage

L'**ordre décidé par Rihen** (ROADMAP, révisé le 02/08) **prime** : modélisation
(panneau droit) → caméras → éclairage → import → combo Ajouter → mode Édition →
sortie de la démo. Les paliers ci-dessous l'**englobent** sans le réordonner.

| palier | contenu | critère de fin |
|---|---|---|
| **P1 — l'outil utilisable** (Jalon A) | panneau droit complet (Transformation, Dimensions, Relations, Matériaux, Groupes de sommets, Shape keys, UV, Attributs, Données géométriques) ; caméras ; éclairage ; import ; combo Ajouter ; **persistance de tous les types de fichiers** (le chantier en cours) ; **menus, barres et navigateur en `.nkgui`** | un modèle importé, modifié par trois modificateurs, sauvé, fermé, rouvert **identique** ; les 19 entrées de menu servies ou grisées avec leur raison |
| **P2 — la parité de geste** (Jalon B) | mode Édition complet ; les quatre chemins d'accès aux commandes (menus, clic droit, palette, dernière opération) + panneau T ; **hiérarchie et Détails en `.nkgui`** ; **sortie des 96 objets de la démo** | une chaise modélisée **à la souris seule**, puis **au clavier seul**, avec les mêmes commandes au journal |
| **P3 — la sculpture** | les trois modes, brosses, masques, symétrie ; retopologie ; décimation ; niveaux de détail | un buste sculpté à 2 M de triangles, retopologisé à 10 000 quads, sans perte visible de silhouette |
| **P4 — l'asset fini** | UV ; graphe de matériau (brique commune) ; peinture de textures ; cuisson | un rocher sculpté sort avec UV, matière et cartes cuites, et s'ouvre **identique** dans Nogee |
| **P5 — le procédural** | nœuds de modélisation ; générateurs végétation, rochers, bâtiments ; add-ons C++ | un générateur de palmiers : 20 palmiers différents, chacun retouchable à la main |
| **P6 — l'IA** (Jalon C) | IA opératrice (prompt → commandes, aperçu, lot) ; IA génératrice multi-vues ; **générateur de personnages** ; vêtements et cheveux (F2, F3) | trois tours de prompt / retouche / prompt sur le même objet **sans qu'une retouche disparaisse** |

---

## 8. Règles transverses

Celles de SPECIFICATION §5 (zéro-STL, NKMemory, from scratch, cinq backends,
Debug = Release, harnais, **aucun raccourci en dur**), plus :

1. **Toute opération est une commande** — interface, nœuds, IA, add-ons, rejeu :
   une seule porte.
2. **Rien d'important n'existe seulement dans un journal** (UI_SPEC §1).
3. **Fermer un onglet ne supprime jamais rien** (règle de Rihen).
4. **Apparence UE5, gestes Blender, la facilité gagne.**
5. **L'interface vient des documents `.nkgui`** ; même thème, jetons et composants
   que la famille.

---

## 9. Décisions

| # | question | proposition |
|---|---|---|
| D1 | les formats `.nk3dm` / `.nkmesh` / `.nkmat` gardent-ils leur nom ? | **oui**, pour l'instant : un format lu par quatre applications ne se renomme qu'avec un lecteur des deux noms et une date de fin |
| D2 | F1 : UV, matériaux, textures dans NKCraft | **oui** |
| D3 | le générateur 3D externe (TripoSR) dans le produit diffusé | **non** : R&D seulement ; le produit attend un modèle NKAI entraîné sur des données dont Rihen a le droit |
| D4 | F2, F3 : vêtements et cheveux | forme dans NKCraft, dynamique dans NkAnima |
| D5 | un **mode Facile** comme NkAnima (« des curseurs et des phrases ») | oui, **à partir de P6** : c'est le générateur de personnages et l'IA opératrice, présentés sans vocabulaire |
