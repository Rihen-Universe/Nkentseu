# NKRenderer — Roadmap

État actuel (2026-07-22) : Phases A → G + G.ext M.1..M.5 + M.8 livrées ; VSM v2
(overrides + ombres alpha-testées **4 backends**), morph targets v1+skinnés,
animation avancée v1+v2, streaming réel v1+v2, deferred v1+v2 (GL/VK validés)
livrés — voir « Reste à faire priorisé » ;
viewport d'édition (gizmo + 6 view modes + edit mode mesh) livré ; convolutions
IBL sur GPU compute (Phase N v1) livrées (DX11 = CPU par défaut). Pipeline
post-process avec **Bloom Dual-Kawase 11-pass AAA cross-API** + ACES tonemap.
HDR IBL avec cubemap dédié skybox (RGBA32F brut). PBR avec mirror via
tSkyEnvCube pour roughness ~0. SSAO v0 + Voxel AO v0 stable. **Planar
Reflection bugs RÉSOLUS** (2026-05-23). **NkVirtualShadowMaps v0 livré**
(2026-05-23) : multi-lights DIR + SPOT + POINT avec atlas dynamique skyline.
10 démos couvrant tous les matériaux et features.

Cross-API validé sur **Vulkan + OpenGL + DX11 + DX12** (parité atteinte 2026-06-24,
voir « Multi-backend » plus bas). Metal + Software restent à valider.

---

## 📊 COUVERTURE PAR LES BANCS CONSOLE — la « colonne trois » (mesuré le 2026-08-22)

L'inventaire d'écart du mandat demandait trois colonnes : ce que la sandbox
exerce · ce que le noyau offre · **ce qu'un banc console couvre**. La troisième
est la seule qui dise si une capacité est *prouvée*. La voici, mesurée méthode
par méthode plutôt qu'à l'impression.

**Périmètre de la mesure** : les six bancs console sans fenêtre —
`NKEditMeshHarness`, `NkEditableMeshDemo`, `NKSmoothMeshTest`, `NkAssetIODemo`,
`NkFBXParityDemo`, `NKMatTypeResetTest`.

| domaine | API publique | exercée par un banc |
|---|---|---|
| `Mesh/NkEditMesh` (+ `NkEditHistory`, `NkMeshEditRecorder`, `NkModifierStack`) | **94** méthodes | **94** — ✅ **complet le 2026-08-22** |
| `Materials/` (`NkMaterialSystem` + `NkMaterial` + `NkMaterialLibrary`) | **79** méthodes | **0** |

> 📐 **Pourquoi 94 et non 102.** Le premier relevé comptait les **surcharges**
> séparément (`FindById` const et non-const, etc.). La mesure porte désormais sur
> le **nom de méthode publique**, ce qui rend le chiffre stable quand une
> surcharge est ajoutée. Le périmètre couvert, lui, est le même — et il est
> maintenant entier.
>
> ⚠️ **L'instrument de mesure avait lui-même un angle mort, trouvé en le
> relançant.** Il cherchait les appels sous la forme `.Methode(` ou `->Methode(`
> et **ne voyait donc pas les méthodes `static`**, appelées `Classe::Methode(`.
> `ProportionalWeight` est ressortie « non couverte » alors qu'un cas l'exerçait.
> *Un outil de couverture qui ignore une forme d'appel sous-estime en silence —
> c'est la même famille d'erreur que compter des noms au lieu d'objets, appliquée
> cette fois à l'outil et non au code.*

### ⚠️ Matériaux : zéro, et ce n'est pas un oubli — c'est un couplage

**Aucun banc n'inclut un seul en-tête de `Materials/`.** (Contrôle que la commande
sait trouver : les mêmes bancs incluent bien `NKRenderer/Mesh/*` et
`NKRenderer/Core/NkGizmo.h`.)

⚠️ **Quatre faux positifs écartés** : un comptage par nom de méthode donnait « 4
couvertes » — `Bind`, `IsValid`, `Load`, `Save`. Ce sont des **collisions de
noms** (le `Bind` d'un raccourci clavier, le `IsValid` d'un maillage, les
`Load`/`Save` du graphe). Aucune n'est un appel matériau. *Compter des noms au
lieu de mesurer des objets aurait transformé 0 en 4.*

**La cause est connue et mesurée** : `NkMaterialSystem::Init` déréférence le
device dès sa ligne 45, et `CreateInstance` fait `mDevice->CreateBuffer` sans
garde (19 déréférencements de `mDevice->` au total). **Toute la sémantique
intéressante est pourtant du CPU pur** — hiérarchie parent/enfant, masques
d'override, propagation sélective, couches V1, sérialisation `.nkasset`. Elle est
enfermée derrière une porte GPU.

> ⚠️ `Materials/` appartient à l'agent **matgraph** depuis le 2026-08-22. Le
> déverrouillage headless est **son** chantier, pas celui de nk3dmodeler.

### ✅ RÉSORPTION EN COURS — la pile annuler/refaire est couverte (2026-08-22, `5c026b3d`)

`NkEditHistory` est passée de **0 méthode exercée** à couverte par **7 cas
`hist/`** dans `NKEditMeshHarness` : aller-retour, pile vide, plafond, branche
abandonnée, chaîne de trois annulations, et la survie des attributs.

**Aucun défaut trouvé** — la pile était juste. Mais deux choses méritent d'être
notées, parce qu'une batterie verte du premier coup sur un système jamais exercé
ne prouve rien par elle-même :

1. ⚠️ **Les sept cas savent échouer, et c'est vérifié.** Deux défauts ont été
   injectés dans `NkEditHistory` puis retirés : `EM_PushCapped` jetant le plus
   **récent** (→ `hist/plafond` : `x 0.5 -> 0.5` au lieu de `-> 2.5`), et
   `Commit` ne vidant plus la pile de refaire (→ `hist/branche-abandonnee` :
   `apresNouveauCommit=1`, `canRedo=1`). Dans les deux essais, **les cinq autres
   cas sont restés verts** : les cas ne se contaminent pas.
2. ⚠️ **Un cas de cette batterie a dû être corrigé pour être lisible.** La
   première version de `hist/plafond` affichait `x=2.5` **sans dire d'où l'on
   part ni combien de retours ont eu lieu** — donc invérifiable, et incapable de
   distinguer « 3 retours sur la bonne pile » de « 2 retours seulement ». Elle
   affiche désormais `x0`, le nombre de remontées et la profondeur.
   **Un chiffre sans son référentiel n'est pas une mesure.**

La signature compare un **hachage d'état complet** (positions, index de matériau,
sélection, rangs de sélection), pas un compte de sommets : une annulation qui
restaure le bon *nombre* de sommets aux *mauvaises* positions passerait un
comptage.

> 🔗 Le cas `hist/materiau-survit` relie les deux chantiers : l'index de matériau
> par face et les slots du maillage survivent bien à `Undo` après une
> subdivision (`slot7 1 → 4 → 1`, slot `id=4242` conservé). C'était vrai « par
> construction » puisque l'instantané est une copie — **et « par construction »
> est exactement le genre d'affirmation qui devient fausse le jour où un champ
> est ajouté après coup.** D'où la mesure.

### ✅ RÉSORPTION (2) — les six opérations sans banc sont couvertes (2026-08-22, `85038194`)

`BisectByPlane`, `DeleteSelectedFaces`, `ExtrudeSelectedVertices`,
`LoopCutFromSelectedEdge`, `MakeFaceFromSelected`, `SpinSelected` : **7 cas
`ops/`**, chacun mesurant l'effet **topologique** attendu et non le seul booléen
de retour (une opération peut rendre `true` sans rien faire).

**Aucun défaut trouvé dans le moteur.** Les trois anomalies apparentes venaient
toutes **du banc**, et chacune enseigne quelque chose :

| observation | vraie cause |
|---|---|
| `ops/vis-revolution` : **88 arêtes non-manifold** | **figure mal choisie** — l'axe traversait la grille, la surface balayée se recoupait. Axe décalé → `0 → 0`. Le chiffre ne disait rien du code |
| `ops/coupe-par-plan` : **`nonmanif=18`** sur un cube manifold | ⚠️ **format désaligné** — sept `%u` pour six arguments. Le chiffre était **lu dans de la mémoire indéterminée**, et il était crédible |
| `ops/extruder-sommets` : « faces pleines 6 → 9 » | **libellé faux** — le compteur incluait les **arêtes fil** (faces à 2 sommets), que l'extrusion de sommet crée par définition. Séparés : `6 → 6` pleines, `3` fils |

> 🛡️ **Parade posée, et vérifiée en la faisant échouer** : `GraphPut` porte
> désormais `__attribute__((format(printf, 1, 2)))`, et le fichier active
> `#pragma GCC diagnostic error "-Wformat"`. **Un format désaligné arrête
> maintenant la compilation**, dans ce fichier uniquement. Les ~60 appels
> existants sont passés indemnes. Un avertissement se noie dans un build de
> 25 projets ; **un banc qui affiche un chiffre faux est pire qu'un banc absent,
> parce qu'on lui fait confiance.**

### ✅ Maillage : 33 méthodes non exercées — **toutes couvertes depuis le 2026-08-22**

> Section conservée pour ce qu'elle enseigne sur la **méthode de mesure**. Les
> trois ensembles ci-dessous sont désormais exercés : l'undo/redo par la famille
> `hist/`, les six opérations par `ops/`, et les accesseurs par `acces/`.

**1. La pile UNDO/REDO du maillage — entièrement non couverte.**
`Push`, `CanUndo`/`CanRedo`, `UndoCount`/`RedoCount`, `SetLimit`, `ReplayOnto`,
`MoveDown`, `SelectionStamp`, `ApplyVertSel`.

⚠️ **Piège évité** : le harnais contient bien `h.Undo(g)` / `h.Redo(g)` — mais
c'est l'historique du **graphe** (`NkGraph`), pas celui du maillage. Un comptage
par mot-clé aurait déclaré l'undo couvert.

**2. Six opérations d'édition sans aucun banc** :
`BisectByPlane`, `DeleteSelectedFaces`, `ExtrudeSelectedVertices`,
`LoopCutFromSelectedEdge`, `MakeFaceFromSelected`, `SpinSelected`.

⚠️ **Même piège pour `Spin`** : le harnais le mentionne, mais seulement comme
**liaison de raccourci clavier** (`t.Bind("mesh.spin", …)`) — l'opération n'est
jamais appelée.

> 🎯 **La leçon de méthode, valable pour toute mesure de couverture** : chercher
> le NOM d'une capacité et chercher son USAGE donnent deux résultats différents.
> Ici l'écart valait 4 matériaux fantômes, un système d'undo, et une opération.
> **Une couverture se mesure sur l'objet appelé, pas sur le mot trouvé.**

**3. Le reste** (≈20) sont des accesseurs et requêtes de topologie —
`EdgeIsBoundary`, `EdgeIsManifold`, `RadialTwin`, `FaceIsSelected`… Leur absence
pèse moins : les cycles radial/disque sont exercés par la famille `bmesh2` à
travers les opérations qui s'en servent.

> ⚠️ **Cette dernière phrase était trop indulgente, et je la corrige.** « Exercé à
> travers une opération qui s'en sert » prouve que le cycle radial est
> *parcouru*, pas que `RadialTwin` **refuse** quand elle le doit. Un accesseur ne
> casse rien de visible quand il ment : **il fait mentir celui qui s'en sert.**
> Mesure faite : `RadialTwin` refuse bien sur les 12 demi-arêtes de bord d'une
> grille — mais rien ne le prouvait avant le 2026-08-22.

### Chargeurs de maillage

| chargeur | banc |
|---|---|
| OBJ · glTF · FBX | `NkAssetIODemo`, `NkFBXParityDemo` |
| PLY · STL · DAE · USDA | ✅ `NKEditMeshHarness`, famille `chargeurs/` (2026-08-22, `71959d15`) |

**Les sept chargeurs sont désormais couverts** (contre trois). Les quatre ajoutés
**fonctionnent tous** — aucun défaut trouvé :

| | sommets | triangles | bbox |
|---|---|---|---|
| PLY ascii / binaire | 8 / 8 | 12 / 12 | cubique |
| STL ascii / binaire | 36 / 36 | 12 / 12 | cubique |
| DAE | 36 | 12 | cubique |
| USDA | 24 | 12 | cubique |

Le contrôle le plus fort de cette famille ne dépend d'**aucune valeur écrite à la
main** : ASCII et binaire encodent le même cube, donc le chargeur est comparé
**à lui-même**. Et la bbox attrape ce qu'un comptage laisse passer — axe permuté,
échelle, boutisme. « Cubique » est testé comme une **relation entre les trois
côtés** (égaux à 1 % près), pas contre une taille en dur : le banc reste juste si
quelqu'un remplace le cube de test.

### ⚠️ DEUX DÉFAUTS TROUVÉS — dans le banc, pas dans les chargeurs

1. **Les six chargeurs rendaient `ok=0`** au premier essai. Cause : le banc était
   lancé depuis `Applications/NKEditMeshHarness/` (le dossier où vit
   `editmesh_baseline.txt`, donc celui d'où l'on fait `--check`), alors que
   `Resources/` se résout depuis la **racine du worktree**. Le banc a besoin des
   **deux répertoires à la fois** — insoluble par le seul répertoire courant.
   C'est la **même dette que celle déjà nommée pour les shaders** : deux
   politiques de chemin dans un même programme.
   → Remède local : `CheminRessource()` essaie le chemin tel quel puis en
   remontant. **Vérifié depuis les deux répertoires : sortie identique.** Cela
   compte pour le vérificateur, qui ne sait pas d'où lancer un banc.
   ⚠️ *Un banc muet parce qu'il a été lancé d'ailleurs est pire qu'un banc
   absent : il rend `ok=0` et ressemble à un chargeur cassé.*
2. **`identiques=1` sur deux zéros.** La comparaison ASCII/binaire testait
   l'égalité sans tester l'existence — elle réussissait donc sur du vide, pendant
   que les six chargeurs échouaient. Corrigée en exigeant `> 0`.
   *Troisième occurrence de ce motif dans ce chantier* (après les deux cas
   « parallèle » de `NkFBXParityDemo` et `matops/deformation-pure`) :
   **un contrôle d'égalité sans contrôle d'existence réussit toujours sur du
   vide.**


### ✅ RÉSORPTION (5) — les 19 dernières méthodes publiques sont couvertes (2026-08-22, `b0c5c8fd`)

**15 cas `acces/`** dans `NKEditMeshHarness` ferment la colonne trois côté
maillage : **94 / 94**. Ce sont pour l'essentiel des **lectures** — prédicats de
topologie, sélection, ombrage, triangulation d'affichage — plus les trois
systèmes voisins (`NkMeshEditRecorder::Push`/`ReplayOnto`,
`NkModifierStack::MoveDown`/`FindById`).

**Aucun défaut trouvé dans le moteur.** Ce qui compte ici est donc la **forme des
cas**, pas leur couleur :

| règle appliquée | ce qu'elle empêche |
|---|---|
| Un prédicat est mesuré sur une forme où il dit **OUI** et une où il dit **NON** | la fonction qui rend toujours `true` passerait un test à un seul cas |
| Une opération est jugée sur son **booléen ET son effet observable** | `MoveDown` rendant `true` sans rien déplacer ; `ReplayOnto` comptant sans appliquer |
| Les quatre classes d'arête sont vérifiées comme une **partition** | quatre comptes figés qu'il faudrait remettre à jour à chaque changement de primitive |
| Les courbes d'influence sont jugées sur leurs **invariants** (pleine au centre, nulle hors rayon, jamais croissante) | six valeurs en dur qui tomberaient au premier réglage de courbe |

Trois observations qui valent d'être gardées :

1. **`AnyFaceSmooth` et `AllFacesSmooth` ne se distinguent que sur un maillage
   MIXTE.** Testées seulement en tout-plat et tout-lisse, une implantation qui
   confondrait les deux passerait. Le cas pose donc explicitement le troisième
   état (`mixte any=1 all=0`).
2. **`RadialTwin` refuse correctement sur un bord** — 12 demi-arêtes de bord,
   12 refus. C'est le contrat de l'en-tête (« en choisir un au hasard serait un
   mensonge »), et rien ne l'attestait.
3. ⚠️ **`GetEdgeLoop` rend 1 seule paire sur une arête de BORD, et 4 sur une
   arête intérieure.** Le premier chiffre ressemblait à un défaut ; c'est le
   contrat (« s'arrête proprement sur un pôle, un n-gon ou un bord »). Il a été
   **mesuré sur les deux départs plutôt que supposé** — et `GetFaceLoop`, lui,
   traverse la grille dans les deux cas, ce qui est bien la différence entre les
   deux parcours.

---

## 🎨 MATÉRIAU PAR FACE (façon Blender) — livré partiellement (agent nk3dmodeler, 2026-08-21, `942fae39`)

Demande de Rodolf : « un groupe de vertex ou de face **lier ou non** partage même
material comme sur blender ». Le mot qui commande est **« lier ou non »** : deux
faces d'un même matériau n'ont aucune raison d'être connexes.

**Livré** — `NkEditMesh::Face::material` (index) + `NkEditMesh::materialSlots`
(le maillage possède la liste, la face n'en porte que l'index) + `BuildSubMeshRanges`
(sous-mailles **déduites** de l'index, un slot pouvant donner **plusieurs plages**)
+ `AssignMaterialToSelectedFaces` (règle Blender : faces dont **tous** les coins
sont sélectionnés). Preuve : 11 cas `mat/` dans `NKEditMeshHarness`, référence
passée de 169 à 180 lignes, les 169 premières identiques octet pour octet.

### ⚠️ LE FAIT QUI A DICTÉ LA CONCEPTION — `smooth` NE SURVIT PAS AUX OPÉRATIONS

**Mesuré, pas supposé** (cas `mat/temoin-smooth`) : 12 faces passées en SMOOTH,
une subdivision, **`smooth 12 -> 0`**. Zéro survivante.

**Cause** : toute opération d'édition passe par `ToPolygons`/`BuildFromPolygons`,
qui **reconstruit des `Face` à neuf**. `ToPolygons` n'émettait **aucun attribut de
face** — tout ce que portait `Face` était perdu à chaque opération.

Si `smooth` n'a jamais gêné, c'est qu'il est **RE-DÉDUIT** par `BuildFromIndexed`
en comparant les normales des coins : **l'ombrage EST une propriété des normales**.
**Un matériau n'est déductible de rien.** D'où le transport explicite par
paramètres optionnels sur les deux bouts du round-trip — et non la copie du
« précédent `smooth` », qui aurait donné un champ vidé à la première subdivision.

> 🎯 **Règle pour qui ajoute un attribut à `Face`** : demande-toi s'il est
> **déductible de la géométrie**. Si non, il doit être transporté explicitement à
> travers `ToPolygons`/`BuildFromPolygons`, sinon il disparaît en silence à la
> première opération d'édition.

### ✅ RÉSORPTION (3) — huit opérations de plus transportent le matériau (2026-08-22)

**Câblées ce jour** : `DeleteSelectedFaces`, `MakeFaceFromSelected`,
`ExtrudeSelectedVertices`, `LoopCutFromSelectedEdge`, `BisectByPlane`,
`SpinSelected`, `InsetSelectedFaces`, `SplitSelectedEdges`.
Avec les quatre de la veille : **douze opérations** transportent l'index de
matériau. Preuve : famille `matops/`, 12 cas, **tous rouges avant** (`slot1 → 0`).

`EM_ToWeldedPolygons` a reçu le même paramètre optionnel que `ToPolygons` — c'est
par elle que passent les opérations qui soudent avant d'éditer.

### ✅ RÉSORPTION (4) — les trois opérations « non câblées » le sont (2026-08-23)

Ce titre disait, jusqu'à ce jour, **« 🔴 TROIS OPÉRATIONS NON CÂBLÉES — et c'est
un refus motivé, pas un oubli »**. Le refus était motivé ; il n'a plus d'objet.
Les trois sont câblées, et chacune par une règle **arbitrée**, pas devinée.

| opération | ce qui bloquait | ce qui a tranché |
|---|---|---|
| `DissolveSelected` | de qui la survivante d'une fusion hérite-t-elle ? | **dominance par l'aire**, égalité par l'indice le plus bas |
| `BevelSelected` | bandes et coins n'ont **aucune face mère** ; alignement de `endFace` non prouvable | héritage des faces **géométriquement adjacentes**, dominance par la **longueur de contour partagé** ; l'attribut est poussé **dans `endFace` lui-même**, sous la même condition d'acceptation que `nfs` — l'alignement n'est plus à prouver, il est **structurel** |
| `ExtrudeSelectedEdges` | chemin propre, ne passant par aucune des deux fonctions de round-trip | même règle ; et le passage de `&fa` à `ToPolygons` a corrigé au passage une régression que personne ne mesurait (l'opération **repeignait tout le maillage en slot 0**) |

**Une seule phrase couvre les deux familles** — *dominance par une mesure,
égalité par l'indice le plus bas* — et c'est délibéré : aux sites de **fusion** la
mesure est l'**aire** de la face absorbée, aux sites de **création** la **longueur
de contour partagé** avec la voisine. `EM_MaterialDominant` prend donc un *poids*
et non plus une aire. Deux fonctions auraient laissé diverger deux énoncés
identiques.

#### ⚠️ Ce que la mesure a corrigé de ce qui était écrit ici

> 🎯 L'ancien encadré disait : *« mieux vaut une limite connue qu'un câblage
> supposé — les trois lignes `matops/` correspondantes affichent `slot1 → 0` et
> disent donc la vérité sur l'état du code »*. C'était juste. Ces deux lignes
> **ont changé de valeur** avec ce câblage, et c'est le résultat attendu :
> `matops/chanfrein` passe de `slot1 6 -> 0` à **`6 -> 26`**, et
> `matops/extruder-aretes` de `6 -> 0` à **`6 -> 18`**. Aucune face créée ne
> tombe plus dans le slot 0.

**Précédent NON suivi, et il faut le dire** : `SpinSelected` donne le **slot 0**
à ses bandes latérales, qui n'ont pas non plus de face mère. C'était un choix
écrit et assumé, mais il **ne devient pas la règle** : le slot 0 est une couleur
comme une autre, pas un « sans matériau », et l'y envoyer repeint la bande en
silence. Les faces créées héritent désormais de leur voisinage.
⚠️ **`SpinSelected` reste donc à reprendre** — c'est la dette que cette
résorption laisse derrière elle, nommée plutôt que refermée.

#### 🧪 Preuve : famille `matcre/`, 10 cas, référence de 250 à 260 lignes

Deux d'entre eux ne mesurent pas l'opération mais **le banc lui-même**, et ils
sont là pour une raison précise :

- **`matcre/temoin-maillage-soude`** — sur un maillage **non soudé** (un cube
  importé porte 24 sommets pour 8 positions) une arête n'a qu'**une** face
  incidente : jamais d'ambiguïté, donc jamais de perte, donc **un compteur
  toujours nul sans qu'une seule ligne soit fausse**. Ce cas vérifie que l'arête
  mesurée en porte bien **deux**. Sans lui, les autres mesureraient un bord.
- **`matcre/chanfrein-longueur-domine`** — dans tous les autres cas le gagnant
  est **aussi** l'indice le plus bas : ils ne distinguent pas « dominance par la
  longueur » de « toujours le plus petit slot ». Ce cas met les deux critères en
  **désaccord** (coin aigu d'un côté ⇒ recul plus grand ⇒ contour partagé plus
  court) et mesure **`slot5=2`** : la longueur gagne contre l'indice.

> ⚠️ **Un critère qui ne peut pas départager est aussi vide qu'un compteur qui ne
> peut pas valoir autre chose que zéro.** Les deux se prouvent de la même façon :
> en construisant le cas où ils devraient parler, et en regardant s'ils parlent.

`matcre/chanfrein-cube-raccords` exerce la troisième famille de faces, la seule
qu'aucun autre cas n'atteignait — les **raccords aux sommets** n'existent que si
les deux extrémités de l'arête chanfreinée sont fermées. Cube 6 faces, six slots
distincts : **6 → 26 faces** (6 + 12 bandes + 8 raccords), `slot0=0`,
**`perdus=28`** — soit exactement 12 bandes × 1 perte + 8 raccords × 2.

### ✅ RÉSORPTION (6) — la pile de modificateurs transporte le matériau (2026-08-23)

> ⚠️ *Numérotation : ce document porte DEUX séries de « résorption » mêlées — la
> couverture des bancs (2) et (5), et le transport du matériau (3), (4) et celle-ci.
> Le (5) de la ligne 218 ne précède pas ce (6) : il parle d'autre chose. Renuméroter
> après coup casserait les renvois déjà écrits ailleurs ; on le signale au lieu de le
> réparer en silence.*

**Le même jour, en deux temps.** La re-mesure du matin avait déplacé la dette des
opérations d'édition vers les **modificateurs** ; l'après-midi les a câblés. Le
cadre posé par Rodolf tranchait la question du périmètre : *« en édition on doit
être quasiment similaire à Blender, ou plus performant »* — et dans Blender,
appliquer un Solidify ou un Array **ne perd pas les matériaux**.

| modificateur | héritage retenu | pourquoi |
|---|---|---|
| `Mirror`, `Array` | **identité** | ce sont des COPIES ; la face miroir est la même face, retournée |
| `Triangulate` | **identité**, sur ses **deux** chemins | un éventail sort d'un seul n-gon. ⚠️ Le chemin `BuildFromIndexed` lisait déjà la parenté `tf` (elle sert au *picking*) : il n'y avait qu'à la brancher |
| `Build`, `Mask` | **identité** | c'est une sélection, pas une création |
| `Solidify` | coques externe et interne = **identité** ; **tranche de bord** = règle des faces sans mère | la tranche est la seule face réellement neuve |

⚠️ **Un chemin sur deux aurait suffi à ne rien voir** : `Triangulate` bascule
entre deux implémentations selon `triangulateMinVerts`. N'en corriger qu'une
aurait laissé la moitié des utilisateurs avec des faces repeintes, **selon un
réglage qu'ils n'auraient jamais relié au symptôme**. Les deux ont leur cas.

#### 🟢 `SpinSelected` est aligné, et son exception est levée

Ses bandes latérales recevaient le **slot 0** — choix écrit et assumé *d'avant*
l'arbitrage, dont la raison inscrite dans le code (« hériter d'un des deux côtés
privilégierait arbitrairement un côté ») a été retirée par la règle. Elles
héritent désormais de leur voisinage.

> **Une règle qui traîne une exception que personne ne se rappelle est pire que
> pas de règle : elle fait croire qu'on peut prédire le comportement.**

#### 🕳️ Le trou n'était pas dans le code, il était dans le BANC

`matops/` porte le nom d'un domaine — « le matériau survit aux opérations » — et
n'exerçait que les opérations d'**édition**. Les modificateurs n'étaient touchés
par aucune ligne : ils repeignaient tout en slot 0 et la référence restait verte.

> **Une famille de cas qui porte le nom d'un domaine donne l'impression de
> couvrir ce domaine. Elle ne couvre que ce qu'elle exerce.**

Et le chemin des **bandes** de `SpinSelected` était dans le même cas : le seul cas
qui l'approchait, `matops/vis-revolution`, utilise `duplicate=true`, qui **copie**
des faces — donc précisément la branche qui *avait* des mères. Le commentaire du
code le disait en toutes lettres, et personne n'en avait tiré la ligne manquante.

**Preuve** : famille `matcre/` (10 cas) + `matmod/` (9 cas), référence de 250 à
**269 lignes**, les 260 précédentes identiques octet pour octet. Chaque ligne
`matmod/` porte `slot0` — c'est là que tombe toute face qui n'a hérité de
personne — et exprime une **relation** (« chaque slot ×2 ») plutôt qu'un compte
figé, pour ne pas se périmer quand la grille change de taille.

### ⏱️ COÛT MESURÉ — et il n'est pas là où on le cherchait

Mesure derrière `--perf`, **hors référence** : une durée ne peut pas être comparée
octet pour octet, et la poser dans `editmesh_baseline.txt` ferait tomber `--check`
à chaque passage — donc prendre l'habitude de recopier le nouveau chiffre sans le
lire. Grille **128×128 soudée, 16 384 faces**, build **Release**.

**Le surcoût du transport est négligeable, et il est plus petit que le bruit** —
mesuré, pas supposé :

| | sans attributs | avec attributs | écart | dispersion d'une variante |
|---|---|---|---|---|
| `ToPolygons` | 1,215 ms | 1,279 ms | **+0,06 ms** | ±0,34 ms |
| `BuildFromPolygons` | 8,821 ms | 9,527 ms | **+0,71 ms** | ±2,26 ms |

L'écart est **inférieur à l'étalement d'une seule variante** : il ne se lit qu'au
minimum, et il ne se lirait pas du tout sans avoir mesuré le bruit d'abord.

#### 🔴 EN REVANCHE, LA MESURE A TROUVÉ AUTRE CHOSE — `RebuildEdges` est superquadratique

| opération (16 384 faces) | temps |
|---|---|
| `Mirror` (32 768 faces en sortie) | **21 ms** |
| `Mask` | **5 ms** |
| `Triangulate` | **2 785 ms** |
| `Solidify` | **9 354 ms** |
| `RebuildEdges` **seul** | **1 580 ms** |

`Mirror` fait le **même** aller-retour, attributs compris, et sort **plus** de
faces que `Triangulate` — en 21 ms. La différence n'est donc ni le transport ni le
volume : `Mirror` est le seul qui **n'appelle pas `RebuildEdges`**.

**Croissance mesurée** — on double le côté de la grille, donc ×4 faces :

| grille | faces | `RebuildEdges` |
|---|---|---|
| 32×32 | 1 024 | 3,1 ms |
| 64×64 | 4 096 | 73,8 ms *(×23,6)* |
| 128×128 | 16 384 | 1 580 ms *(×21,4)* |

×4 faces coûtent **×22** de temps. Linéaire donnerait ×4, quadratique ×16 :
**c'est au-delà du quadratique**. Sur un maillage dense, c'est ce seul appel qui
fait l'attente — pas l'opération que l'utilisateur a demandée.

⚠️ **Ce n'est pas une régression de ce chantier** : le transport des attributs
coûte 0,7 ms là où `RebuildEdges` en coûte 1 580. Mais c'est **incompatible avec
la cible « quasiment similaire à Blender, ou plus performant »**, et ça grandit
avec les modèles des utilisateurs. **Chantier à ouvrir, avec son chiffre.**

### ⚠️ `smooth` perdu est un DÉFAUT RÉEL, laissé tel quel volontairement

Poser « Shade Smooth » puis subdiviser rend le modèle facetté, **sans un message**.
Non corrigé ici parce que le corriger déplace une valeur de référence et suppose
une décision (l'ombrage suit-il la face mère, ou reste-t-il déduit ?). La ligne
`mat/temoin-smooth` fige le comportement pour qu'un changement se voie.

#### 🔴 REMONTÉ À RODOLF COMME UN CHOIX À FAIRE (arbitrage du 2026-08-22)

**Le chiffre** : `smooth 12 → 0` — douze faces passées en SMOOTH, une
subdivision, **zéro survivante** (cas `mat/temoin-smooth`).

**La cause** : `ToPolygons` n'émet aucun attribut de face ; `BuildFromPolygons`
reconstruit des `Face` neuves. `smooth` ne traverse donc rien — il est
**re-dérivé** par `BuildFromIndexed` en comparant les normales des coins.

**L'effet visible** : poser « Shade Smooth » sur un modèle puis subdiviser le
rend **facetté, sans un message**.

⚠️ **POURQUOI CE N'EST PAS LA MÊME RÉPARATION QUE LE MATÉRIAU, ET POURQUOI IL NE
FAUT PAS LES ENCHAÎNER.** Le chantier matériau-par-face a **prouvé que les deux
champs ne sont pas la même espèce de donnée** :

| | `smooth` | `material` |
|---|---|---|
| nature | **dérivée** — l'ombrage EST une propriété des normales | **choix** — dérivable de rien |
| remède | re-dériver, ou décider qu'il se transporte | **doit** être transporté |

Les corriger ensemble ferait croire à un mécanisme commun qui n'existe pas. Le
transport per-face est en place et pourrait porter `smooth` en trois lignes —
**c'est justement pour ça qu'il faut se retenir** : la vraie question n'est pas
technique, elle est *« l'ombrage suit-il la face mère, ou reste-t-il déduit de la
géométrie ? »*. Les deux réponses sont défendables :
- **suivre la mère** = ce que fait Blender, l'utilisateur garde son choix ;
- **rester déduit** = une surface lissée dense reste lissée même sans mémoire,
  et l'import d'un modèle externe retrouve le bon ombrage tout seul.

**C'est une décision de Rodolf, pas une dette à solder en douce.** Tant qu'elle
n'est pas prise, la ligne `mat/temoin-smooth` garde le comportement sous
surveillance : s'il change, le harnais le dira.

---

## 🧾 DETTES NOMMÉES — chantier « dettes NKRenderer » (agent nkrenderer, 2026-08-16)

Ouvert sur décision de Rodolf : « on doit remplir cette dette ». **Ce qui suit est
mesuré, avec sa provenance ; ce qui ne l'est pas est marqué comme tel.**

### ⚠️ Les shaders ne sont pas chargés quand l'application n'est pas lancée depuis la racine du worktree (2026-08-16)

**Ce n'est pas une régression de code.** `NkShaderLibrary::LoadOrCompileVF` résout
`Resources/NKRenderer/Shaders/` **relativement au répertoire courant**, alors que
`Init()` résout le cache de shaders par `NkPath::GetExecutableDirectory()` — deux
politiques de chemin dans la même classe. Aucun `Resources/` n'est déployé à côté
d'aucun binaire : lancée depuis son propre dossier, une application ne lit **aucun
fichier de shader**.

Mesure — **binaire identique dans les deux états, seul le répertoire de travail
change** ; aucune reconstruction, aucun shader modifié :

| | depuis le dossier de l'exe | depuis la racine du worktree |
|---|---|---|
| **NKXRDemo** `source GLSL vide` | 8 | 0 |
| **NKXRDemo** `non-opaque uniforms` | 8 | 0 |
| **NKXRDemo** `'softness' : no such field` | 4 | 0 |
| **NKXRDemo** `pbrShader.valid` | 0 | **1** |
| **NKXRDemo** processus | meurt (code 6), `Renderer oeil 0 KO` | vivant |
| **Nogee** (dorsal GL) `no GLSL stage provided` | présent | 0 |
| **Nogee** shaders `valid=0` | ShadowLinear, ShadowInstanced, Skin, Instanced, InfiniteGrid | **tous `valid=1`** |

Témoin rejoué sur l'état d'avant : la panne se rallume et s'éteint à volonté.

**Trois symptômes, une seule cause.** Les shaders sans repli embarqué donnent une
source vide ; ceux qui en ont un compilent un repli **périmé** (PBR) ou **écrit pour
un autre dorsal** (Render2D, dialecte GL → `non-opaque uniforms outside a block`).

📌 **Deux conséquences à retenir :**
- Les shaders qui « passaient » sont exactement **ceux qui possèdent un repli
  embarqué**. Le repli **fabrique une fausse santé** : il répond toujours, et sa
  réponse fausse est indiscernable de la vraie.
- Le chemin **NkSL** est tenté **avant** le `.vk.glsl` (`:678-706`). Tant que le
  répertoire courant est mauvais, ce chemin **n'est jamais emprunté** — les courses
  concernées ne prouvent donc rien sur l'état de NkSL, ni en bien ni en mal.

**Corrigé ici** (`cc227e3c`) : le journal distingue désormais « fichier introuvable »
de « source invalide » — ils n'ont pas le même remède, et les confondre envoie
réécrire un shader qui n'a jamais été lu. Le dossier de l'exécutable est ajouté comme
**seconde racine** (additive ; le répertoire courant reste essayé en premier).

#### 🔁 La même dette touchait les BANCS, et là elle est pire (2026-08-22, `ab71c751`)

Un shader introuvable fait une image fausse — visible. Un **banc** lancé du mauvais
endroit rend un **verdict** faux, et un verdict on le croit sur parole.

Mesuré, binaire identique, seul le répertoire de travail change :

| banc | depuis la racine | depuis son propre dossier |
|---|---|---|
| `NkAssetIODemo` | 53 OK / 0 FAIL, sortie 0 | **0 OK / 6 FAIL**, sortie 1 |
| `NkFBXParityDemo` | 8 OK / 0 échec / 5 sautés, sortie 0 | **0 OK / 2 échecs** / 5 sautés, sortie 1 |
| `NKEditMeshHarness` | 232 conforme, sortie 0 | 232 conforme, sortie 0 *(corrigé le 2026-08-22)* |

> ⚠️ **Aucun des deux ne dit qu'il n'a rien trouvé : ils disent « ÉCHEC ».** Le
> symptôme d'un chemin non résolu est *indiscernable* de celui d'un chargeur
> cassé. Un vérificateur qui lance un banc depuis son dossier diagnostiquera un
> défaut de chargeur qui n'existe pas — et le diagnostiquera **à chaque passe**.

**Corrigé** : les deux portent désormais le même `CheminRessource()` que
`NKEditMeshHarness` (essaie le chemin tel quel, puis en remontant). Prouvé par
**deux** comparaisons, pas une : sortie **inchangée** depuis la racine (donc
aucune régression) **et** identique depuis les deux répertoires (donc la dette est
bien levée).

> 🧾 **Dette assumée** : trois bancs portent maintenant la même fonction de dix
> lignes. La mutualiser demanderait un en-tête commun aux bancs, qui n'existe
> pas ; la mettre dans le Kernel polluerait le moteur avec un utilitaire de test.
> Le commentaire de chaque copie nomme les deux autres.

**Signalé, non corrigé — hors périmètre `Mesh/`** : `NkSLCheck` lit
`Resources/NKRenderer/Shaders/...` par chemin relatif. Il est **moins dangereux**
(il écrit honnêtement `[!] introuvable:` au lieu d'un échec), mais il fait
`return 0` inconditionnellement : **son code de sortie ne signale jamais rien**.
Un vérificateur qui ne lirait que sa sortie processus le croirait toujours vert.
*(À l'agent qui tient NkSL / matgraph.)*

**NON corrigé — décision de déploiement, hors périmètre d'un seul module** : déployer
`Resources/` à côté des binaires (règle Jenga), ou résoudre depuis une racine de
projet découverte, ou déclarer un chemin de recherche. Tant que ce choix n'est pas
fait, **la façon de retrouver la scène est de lancer depuis la racine du worktree**.

#### ⚠️ Ce n'est pas un défaut de shaders — le même piège existe ailleurs

Le journal de la course en échec annonce
`Table LTC absente ou invalide : Resources/NKRenderer/LUT/ltc1.bin (0 octets)`.
**Le fichier fait 65 536 octets sur le disque.** Il n'était pas vide, il était
introuvable — et le lecteur a rapporté « 0 octets » au lieu de « introuvable »,
**exactement la confusion corrigée côté shaders, dans un sous-système différent**.

Tout ce qui lit une ressource par un chemin relatif est concerné. Traiter ceci
comme un défaut de `NkShaderLibrary` serait refaire l'erreur de diagnostic qui a
déjà coûté une soirée.

#### Les 3 pipelines encore en échec dans la course « réussie »

La course depuis la racine n'est **pas** complète, et il ne faut pas la lire ainsi :

```
CreateGraphicsPipeline 'ParticlesBillboard' : shader handle id=0 introuvable
CreateGraphicsPipeline 'TrailMesh'          : shader handle id=0 introuvable
CreateGraphicsPipeline 'Decal'              : shader handle id=0 introuvable
```

Une fois par œil, soit 6 occurrences. Ils **ne bloquent pas la scène** (elle
s'affiche et l'application vit). **Non instruits** — cause inconnue à ce jour, à ne
pas supposer identique à celle ci-dessus.

#### Instruction de la décision de déploiement — deux nombres et une réponse

**Combien pèse un déploiement.** Le multiplicateur n'est pas 205 : **31 cibles**
consomment `NKRenderer` (`grep -rl NKRenderer --include=*.jenga`). Et les shaders
seuls ne suffisent pas — la ligne LTC ci-dessus le prouve.

| ce qu'on déploie | par cible | × 31 cibles |
|---|---|---|
| `Resources/NKRenderer/Shaders` seul | 1,8 Mo | 56 Mo — **insuffisant** (ni LUT, ni textures, ni ciel) |
| `Resources/NKRenderer` | 230 Mo | **7,1 Go** |
| `Resources/` complet (démos : modèles, audio) | 736 Mo | **22,8 Go** |

**Copie ou lien change tout** : une *jonction de répertoire* Windows (`mklink /J`)
coûte **~0 octet**, ne demande **aucun droit administrateur**, et se crée en
post-build. Les liens symboliques exigent le mode développeur ou l'élévation — donc
non. Les chiffres du tableau sont ceux de la **copie**.
⚠️ À rapporter aux **11 worktrees vivants**, qui portent déjà chacun son `Resources/`.

**À quoi reconnaît-on la racine.** Marqueur : **`nkentseu.jenga`** (32 Ko, à la
racine de chaque worktree) — nommé, versionné, non ambigu.
- **Remonter depuis le dossier de l'EXÉCUTABLE, jamais depuis le répertoire
  courant** : c'est déterministe quel que soit le mode de lancement, et c'est
  précisément la variable qui a causé cette panne. Depuis
  `Build/Bin/Release-Windows/<Cible>/`, la racine est à **4 niveaux**.
- ❌ **Pas `.git`** : dans un worktree lié c'est un **fichier**, dans le clone
  principal un **dossier** (vérifié). Une règle fondée dessus se comporterait
  différemment entre `Nkentseu/` et les 10 autres worktrees — le genre exact de
  divergence qu'on cherche à éliminer.

**Et que se passe-t-il quand on ne trouve rien.** C'est la question qui tranche :
pour le binaire remis à un étudiant en septembre, **la découverte échoue par
construction** — il n'y a aucun `nkentseu.jenga` au-dessus de lui, et il ne faut
surtout pas qu'il en trouve un.

> **Les trois options ne sont donc pas des alternatives, c'est un ordre.** La
> découverte règle les worktrees de développement **gratuitement et
> rétroactivement, sans reconstruire aucune cible** ; le déploiement reste le
> **plancher** obligatoire pour tout binaire livré. Choisir « découverte » sans
> plancher casse la livraison ; choisir « déploiement » seul coûte les gigaoctets
> du tableau à chaque cible de développement.

📌 **Et l'emplacement de la réponse existe déjà, vide.**
`NkPath::GetNkCurrentDirectory()` est déclarée (`NkPath.h:286`) et implémentée
`return GetCurrentDirectory();` (`NkPath.cpp:492-494`) — **sans commentaire, et avec
zéro appelant dans tout le dépôt**. Le concept « répertoire courant *de Nkentseu*,
distinct de celui du processus » a été nommé puis abandonné. C'est là que la
découverte s'écrit, si Rodolf la choisit — pas dans un nouveau symbole.

### ⚠️ NkSL : une absence de preuve qu'on prenait pour une preuve

**NkSL est le langage de shader maison, présenté comme fonctionnel sur cinq dorsaux
sur six. Tant que tout le monde lance depuis le mauvais répertoire, le chemin NkSL
n'est jamais emprunté — ces courses ne prouvaient donc rien sur NkSL, ni en bien ni
en mal.**

Mécanisme : `LoadOrCompileVF` essaie le dialecte NkSL **avant** le `.vk.glsl`
(`NkShaderLibrary.cpp:678-706`), mais uniquement si
`Resources/NKRenderer/Shaders/<Mat>/NkSL/<mat>.{vert,frag}.nksl` **existe**. Chemin
relatif au répertoire courant : lancé ailleurs, le test d'existence échoue, la
branche est sautée **en silence**, et l'on retombe sur le `.vk.glsl` puis sur le
repli embarqué.

Preuve que la branche existe et fonctionne, course depuis la racine :

```
[NkShaderLibrary] 'PBR' -> chemin NkSL (vrai dialecte) : Resources/NKRenderer/Shaders/PBR/NkSL/pbr.vert.nksl
[CompileVF] 'PBR' vsGlsl=4952 fsGlsl=84636
[NkRender3D] PBR pipeline (lazy) create: shader_valid=1 pipeline_valid=1
```

Ce n'est **pas** un reproche à NkSL : rien ici ne dit qu'il est cassé. C'est un
avertissement sur la **valeur probante des courses passées**. Avant d'affirmer quoi
que ce soit sur l'état de NkSL — dans une ROADMAP, un article ou une publication —
**re-mesurer depuis la racine du worktree**, sans quoi le chiffre porte sur un
chemin que le programme n'a pas pris.

### Mesure de référence du dépôt entier

```
arbre Nkentseu-nkrenderer · branche feat/nkrenderer-dettes · base main @10452ae0
config Release · clang-mingw (msys64/ucrt64) · jenga 2.4.0
jenga build --config Release --keep-going -j 0   →   197/205, 8 échecs, 20m23s
```

Le chiffre `197/205` circulait sans support durable : il ne vivait que dans
`echanges/nkxr.questions.md`, **gitignoré**. Il est écrit ici pour cette raison.

### Les 8 échecs, classés par ORIGINE — 3 origines, pas 8 bogues

| origine | cibles | état |
|---|---|---|
| **A₁** — NKRenderer a gagné `NKAnima`/`NKAnimPhysics` (extraction du 14/08) ; `Tutoriels3D.jenga` lie une **liste manuelle** que rien n'a forcée à suivre | Tuto02Renderer, Tuto03Scene, Tuto04Camera, Tuto05Meshes | ⏳ corrigé sur `feat/nkanimation`, **non fusionné** |
| **A₂** — NKTensor a gagné un dorsal GPU ; la fermeture de liens de ses consommateurs n'a pas suivi | NKTensorDemo | ✅ **corrigé** (`d6ab06a6`) |
| **B** — appelants restés en arrière d'une **réécriture** d'API (NKFont : `NkFontLibrary`/`NkTextShaper`/`NkShapeResult` remplacés · NkImage devenu type valeur) | NkRHIDemoText, NkImageDemo | ⏳ traité sur `feat/nkanimation`, **non fusionné** ; NkRHIDemoText y est **désactivée**, avec un arbitrage laissé à Rodolf (porter ou supprimer) |
| **C** — déclaration sans corps dans Foundation (`NkString::begin()/end()`) — **pas une migration** | Gamepad | 🚫 hors périmètre — routé à l'agent NKAnimation |

⚠️ **Le piège de classement, à ne pas refaire** : Tuto02-05 et NKTensorDemo
présentaient le **même symptôme** (undefined reference sur la chaîne RHI) et ont
**deux causes différentes**, dont les remèdes n'ont rien en commun. `useappdeps`
**n'émet que des defines `_STATIC_LIB` et ne lie rien**
(`config/modules.jenga:260-281`) : les projets qui l'emploient portent une liste
`links()` **manuelle**. `nkentseudependson`, lui, calcule la fermeture depuis le
registre. Deux mécanismes, deux endroits à corriger.

📏 **Portée du défaut A₁, bornée** : les `.jenga` citant `"NKRenderer"` **sans**
passer par `nkentseudependson` sont **deux** — `Tutoriels3D` (cassé) et `DemoRW`
(même liste manuelle sans NKAnima, **latent** : son binaire ne tire pas
`NkAnimationSystem.obj`). Les autres vont bien : le registre déclare correctement
`NKRenderer → NKAnima` (`config/modules.jenga:109`).

### ❌ Route racine essayée sur A₂ et **réfutée par la mesure** — ne pas la refaire

Corriger `config/modules.jenga` (le registre déclare `NKTensor` sans
NKRHI/NKSL/NKLogger/NKThreading, alors que `Kernel/AI/NKTensor/NKTensor.jenga` les
déclare pour sa propre compilation — **deux listes, rien qui les tienne**) fait
passer NKTensorDemo de **40 erreurs à 180**. Le registre ne sait exprimer ni les
bibliothèques externes (NKGlad, glslang, SPIRV-Cross) ni les libs système, et
**NKRHI ne les déclare pas non plus** (`:108`) : tirer NKRHI par le registre livre
un NKRHI **sans son dorsal OpenGL**. Changement annulé.

> **Dette restante, nommée** : NKTensor a besoin de toute la pile RHI et son
> entrée de registre ne le dit pas. Chaque consommateur compense à la main (cf.
> `Applications/NKGenTest/NKGenTest.jenga`), et celui qui l'ignore ne l'apprend
> qu'à l'édition de liens. Le remède demande de rendre le registre capable
> d'exprimer une dépendance externe — chantier Jenga, pas NKRenderer.

### Dette 2 — `LoadResult::meshData` : la possession, pas la libération

⚠️ **L'énoncé initial (« personne ne sait qui libère ») est faux, et c'est le
premier résultat.** Il y a un libérateur unique, `FreePayload`
(`Streaming/NkStreamingSystem.cpp`), et **les cinq** sorties de `FinalizeLoad`
l'appellent, plus `Shutdown` qui draine `mResults` après le `Join()` du worker.
**Aucune fuite n'existe aujourd'hui.**

Le vrai défaut est que **la possession n'est pas exprimable** : `LoadResult` est
un agrégat **copiable** portant des pointeurs possédants, avec **deux verbes de
libération** (`Delete()` pour `img`/`meshData`, `->Free()` pour `imgLow`) dont la
distinction ne vit **que dans un commentaire**. Rien ne casse *uniquement parce
que la copie perdante n'est jamais libérée* — un geste juste pour une mauvaise
raison.

📌 **Le piège concret, à connaître avant d'y toucher** : `NkVector::Erase` appelle
`mData[index].~T()` (`NkVector.h:2149`). Tant que `LoadResult` n'a pas de
destructeur, `ready.PushBack(mResults[0]); mResults.Erase(...)` est inoffensif ;
**dès qu'il en a un, cette même ligne libère le payload qu'on vient de copier.**
Toute migration vers la possession doit passer ces transferts en `traits::NkMove`.

⏳ **État** : `feat/nkanimation` a **déjà** migré `img` et `imgLow` vers `NkImage`
par valeur et rendu `LoadResult` non copiable (+145/-52 sur ces deux fichiers,
non fusionné). **`NkGLTFMeshData *meshData` y est resté un pointeur nu** — c'est
le champ que cette migration laisse derrière, et le seul travail restant.
Primitive à employer, elle existe déjà : `NKMemory/NkUniquePtr.h`
(`NkDefaultDelete` fait exactement `NkGetDefaultAllocator().Delete`). ⚠️ `NkOwned`
**n'existe pas** — il n'apparaît que dans des commentaires de `NkISerializable.h`.

### 🔴 `NkRendererImpl::Initialize` s'arrête à l'étape 2 — cause trouvée, hors module

Symptôme relayé : NKXRDemo n'atteindrait jamais l'XR. **Faux, et vérifié** : la
session XR est créée, c'est la **première ligne** de `app.log`. La recherche qui
concluait au contraire cherchait la chaîne `OpenXR` ; le module journalise sous
le tag **`[NKXR]`** — elle ne pouvait rien trouver.

Ce n'est ni un blocage ni un « rend faux ». **`NkShaderLibrary::Init` ne peut pas
rendre faux** : il retourne `mBackend != nullptr`, et `NkCreateShaderBackend`
(`Shader/NkShaderBackend.cpp:879-898`) a un `default:` qui retourne un backend GL.
Journal : 100 `[INF]`, **zéro `[ERR]`**. `gdb --batch -ex run -ex "bt full"` :

```
Invalid address specified to RtlFreeHeap(...)  →  SIGTRAP
#5 ucrtbase!_free_base
#6 NkShaderCache::SetCacheDir(NkString const&)     ← NKSL, pas NKRenderer
#7 NkShaderLibrary::Init      #8 NkRendererImpl::Initialize
```

Corruption de tas **c0000374** — le mélange allocateur custom / heap CRT que le
`CLAUDE.md` interdit en toutes lettres. Site :
`NKSL/ShaderConvert/NkShaderConvert.cpp:750-753`, où `EnsureDirExists(ToStd(dir))`
fabrique un **`std::string`** temporaire (l.47-49) dans un moteur zero-STL dont
NKMemory surcharge les `operator new/delete` globaux. *Suspect principal, pas
certitude* : au premier appel `mCacheDir` est vide, donc l'assignation `NkString`
n'a rien à libérer — il ne reste que le `std::string`.

Le processus meurt, le puits fichier ne vide jamais la suite : **« ça s'arrête à
l'étape 2 » est un artefact d'instrument.** ⚠️ **Le correctif appartient à NKSL**
(19 591 lignes, aucune ROADMAP, aucun agent) ; NKRenderer ne fait qu'appeler.
Et la portée dépasse NKXR : **tout** appel à `NkRenderer::Create` traverse cette
ligne, et qu'une application survive ne prouve pas qu'elle est saine — une
corruption de tas ne tue que quand elle est constatée.

### ⚠️ Les « 9 trappes glslang » n'étaient pas glslang — c'était la DLL du lanceur (2026-08-17)

La trappe résiduelle mesurée après le correctif NKSL (pile `_free_base ←
glslang::TIntermediate::~TIntermediate ← InitializeSymbolTable`, 9 occurrences
pendant le premier parse) avait été attribuée au sous-module NKGLSlang, avec
pour hypothèses « la migration de version répare » ou « mélange NKMemory/glslang ».
**Les deux sont réfutées par la mesure du 17/08** — banc NKXRDemo Vulkan sous
gdb, racine du worktree, cache `.nksc` **vidé** (sonde : breakpoint
`nkentseu::NkGLSLToSPIRV`, accroché avant tout comptage) :

| glslang | PATH devant gdb | trappes | parses réels |
|---|---|---|---|
| 16.5.0 (essai) | `/c/msys64/ucrt64/bin` en tête | **0** | 42 `.nksc` réécrits |
| ancien (pointeur déclaré) | `/c/msys64/ucrt64/bin` en tête | **0** | 42 `.nksc` réécrits |
| ancien (pointeur déclaré) | par défaut (Git `/mingw64/bin` en tête) | **10 SIGTRAP**, pile identique à celle du 16/08 | — |
| 16.5.0 (essai) | par défaut | **ne démarre pas** : `0xc0000139` ENTRYPOINT_NOT_FOUND | — |

**Cause** : l'exe est construit avec clang-mingw **ucrt64**, mais le shell de
l'outillage (Git Bash) met `/mingw64/bin` de Git avant `/c/msys64/ucrt64/bin`
dans le PATH — l'application charge alors les DLL runtime MinGW **de Git**
(`libstdc++-6.dll`/`libgcc`/`libwinpthread`, non isolées individuellement).
Runtimes mélangés → `RtlFreeHeap` sur une adresse d'un autre tas. Ni glslang,
ni NKMemory : **le lanceur**. Corollaire mesuré : glslang 16.5.0 (C++17)
transforme cette corruption **silencieuse** en **refus de démarrage bruyant** —
la mauvaise DLL n'a pas les exports GLIBCXX requis.

**Deux pièges de banc payés au passage, à re-déclarer dans tout banc shader :**
- **l'état du cache `.nksc`** (`Build/Bin/<cfg>/<Cible>/cache/shaders/`) : cache
  chaud → `CompileVF` tourne mais le SPIR-V sort du disque, glslang n'est
  **jamais traversé** — un zéro qui ne mesure rien ;
- **la sonde d'accrochage** : breakpoint sur `NkGLSLToSPIRV` AVANT de compter —
  s'il n'accroche pas, le zéro est un zéro de sonde.

**Décisions ouvertes (Rodolf)** : engager ou non la montée 16.5.0 (adaptations
en stash du sous-module : C++17, `OGLCompilersDLL/` supprimé en amont,
`build_info.h` à générer) ; et le remède de fond côté build — `-static-libstdc++`
ou livraison des DLL ucrt64 à côté des exe — pour que le PATH du lanceur cesse
d'être une variable de comportement.

### 🦴 Chantier « FBX opérationnel » (commande de Rodolf, ouvert 2026-08-17)

Justification : l'export (origine à 0,0,0) passera par FBX, et le corpus NKGen
recevra du FBX. Mesure d'ouverture : `NkFBXLoader.cpp` (963 l.) ne lisait QUE la
géométrie — zéro `Deformer`, jamais `out.nodes`/`skinJoints`/`animations`, et
`AnimBridge.cpp` appelle `LoadGLTF` en dur. **Le parseur d'arbre (binaire+ASCII)
lit déjà TOUS les nœuds** — Deformer/AnimationCurve sont dans `roots`, jamais
consommés : le chantier est de l'extraction, pas du parsing.

**Témoin bilatéral** : le même modèle par les deux chemins —
`Resources/Models/CesiumMan/CesiumMan.glb` (chemin glTF prouvé) et
`CesiumMan.fbx` (exporté du .glb, Blender 5.1 headless ; asset local,
`Resources/Models/` étant gitignoré). Contenu mesuré par un inspecteur
indépendant du moteur : 23 Model (19 LimbNode), 19 Cluster
(Indexes/Weights/Transform/TransformLink), 1 Skin, 57 AnimationCurveNode,
171 AnimationCurve, 374 Connections ; KeyTime en ticks KTime
(46 186 158 000/s) ; euler en degrés, PreRotation absente de l'export Blender
mais requise pour Mixamo. Banc : **`Applications/NkFBXParityDemo`** — vérifie à
chaque course les invariants glTF (19 joints, 1 anim — le chemin glTF ne doit
pas bouger) ET l'état FBX.

| étape | état | preuve |
|---|---|---|
| (a) squelette + noms de nœuds | ✅ 2026-08-17 | 18 OK / 0 échec : 23 nodes nommés, 20 arêtes (3 racines), TRS conformes à l'inspecteur, `Skeleton_torso_joint_1` retrouvé **par son nom** (le chemin glTF ne garde aucun nom — `NkGLTFAnimBake` nomme `joint_{i}`) |
| (b) skinning (poids) | ✅ 2026-08-17 | 29 OK / 0 échec (témoin des deux sens : le banc rend 19 OK + **4 FAIL** sur le code d'avant) : 19 joints = 19 Cluster, correspondance sommet émis → control point conservée par `ExtractGeometry`, poids top-4 normalisés (somme = 1 partout, 12 160/14 256 sommets multi-influences ≈ les 86 % de l'inspecteur), `inverseBind = TransformLink⁻¹·Transform` **numériquement conforme à l'inspecteur indépendant** (translation à 1e-3) — la pose de bind vient des Cluster, PAS des Lcl du graphe (écart mesuré jusqu'à 1,5). Sommets hors skin : défaut `{1,0,0,0}` — même règle que glTF, portée par les initialiseurs de `NkVertexSkinned` (`NkRendererTypes.h:153`). UpAxis=Z sur mesh skinné : conversion SAUTÉE et dite (elle ne retourne que les sommets, elle désaccorderait nodes/inverseBind) |
| (c) animations | ✅ 2026-08-17 | 40 OK / 0 échec (témoin des deux sens : 29 OK + **2 FAIL** sur le loader d'avant) : 1 Stack « Scene » → 57 canaux (19 joints × T/R/S, couverture exacte vérifiée), échantillonnage sur l'union des clés d\|X/Y/Z (linéaire, clampé, défaut P70 du CurveNode), ticks KTime → secondes **sans rebasage** (même règle que glTF : `duration = max`), euler→quat par clé avec RotationOrder + PreRotation composée devant (le consommateur `EvaluateGLTFPose` REMPLACE la rotation statique — le canal doit porter le quaternion complet). Clé 0 de torso_joint_1 conforme à l'inspecteur (T à 1e-4, quat à 1e-3). ⚠️ Durée 10,417 s ≠ 2,0 s glTF : l'export Blender étale la timeline de scène — donnée changée par l'EXPORTEUR ; la parité de durée est impossible sur ce témoin, l'inspecteur fait foi |
| routage extension `AnimBridge` | ✅ 2026-08-17 | `.fbx` → `LoadFBX`, sinon `LoadGLTF` — même critère (insensible à la casse) que `NkMeshSystem::Import`, qui routait déjà. Témoin d'exécution : NkAnimaEditor sur son `.glb` → `19 os, dur=2s, 61 clés`, Anim3D prêt — le chemin historique n'a pas bougé. **Honnêteté** : le chemin `.fbx` d'AnimBridge n'a pas encore d'appelant réel — sa première preuve sera le premier import FBX (même statut que les accesseurs Noge en leur temps) |

| (d) **Mixamo natif** — X Bot / Y Bot contre leurs .glb (Q30 nkanim) | ✅ 2026-08-17 soir | **72 OK / 0 échec** (témoin des deux sens : 51 OK + **7 FAIL** sur le loader de #70). Trois causes réelles + une fausse alerte : (1) squelette « bâton » = artefact de la sonde nkanim (enfants non imprimés) — hiérarchie et PreRotation étaient déjà composées, mains/hanches == glb ×100 à 1 mm ; PostRotation ajoutée selon la formule SDK `R = Rpre·Rlcl·Rpost⁻¹` (non exercée, dit). (2) **inverseBind : `TL⁻¹·Transform` composait `TL⁻¹` deux fois** — Transform = TL⁻¹ · mondeMesh chez Blender comme chez Mixamo ; le chiffre glTF de Q10 était la translation de Transform seule : **la « parité impossible » de (b) était l'artefact de la formule**. Désormais `inverseBind = TL⁻¹`, sommets skinnés ramenés dans l'espace du squelette par `TL·Transform` (identité Mixamo, R·S(100) CesiumMan), **tous les Skin** lus (Beta_Surface restait en bone 0). (3) `stacks[0]` = « Take 001 » VIDE → chaque stack non vide = une animation. (4) feuilles sans Cluster complétées par parcours préfixe des LimbNode (Y Bot 13/65), `bind(feuille) = bind(parent)·local`. À l'écran (NkAnimaEditor, arg de modèle) : XBot.fbx et XBot.glb → même mesh, même pose, même squelette ; CesiumMan.fbx → homme qui marche, squelette attaché. Captures + inspecteur : `Captures/2026-08-17_fbx_mixamo/` (worktree nkrenderer). Hors contrat, dit : InheritType, pivots/offsets. |

⚠️ **Correction de (b)** : la note « les inverseBind glTF et FBX ne sont PAS comparables » est **fausse** — elles le sont (translation de Transform == inverseBind glTF à 1e-3) ; c'est la formule d'alors qui ne l'était pas. Gardée ci-dessus telle qu'écrite, corrigée ici, pour que la trace du raisonnement reste lisible.

### Voyant de santé des shaders (demande de l'agent Noge)

`NkShaderLibrary::GetValidProgramCount()` / `GetProgramCount()` — de quoi afficher
« shaders 4/21 » plutôt qu'un flot `fprintf(stderr)`. **Périmètre** : les
programmes enregistrés dans cette bibliothèque, échecs **inclus** — `Alloc()`
étant appelé inconditionnellement, un programme en échec reste dans `mPrograms` ;
sans cette propriété le couple aurait rapporté « 4/4 ». Les deux traces
`LoadOrCompileVF` (émises par shader **et par image**) passent de `Info` à
`Trace` ; **la ligne d'échec reste au niveau erreur**.

---

## 🦴 L'ANIMATION A QUITTÉ CE MODULE (2026-08-14)

`Tools/Animation` **ne contient plus le système d'animation**. En application du
bloc de décision « SUBSTRATS ANIMATION ET COMPORTEMENT » (`CLAUDE.md` du
répertoire parent), **5 076 des 5 568 lignes** en sont sorties. Il reste
**492 lignes, et rien que ce qui dessine** :

| Reste ici | rôle |
|---|---|
| `Tools/Animation/NkAnimationSystem.{h,cpp}` (345 l.) | **façade de rendu** : téléversement des matrices de skinning, soumission des meshes skinnés, pelure d'oignon, compute de morph targets, debug-draw du squelette |
| `Tools/Animation/NkPoseDebugDraw.{h,cpp}` (147 l.) | dessine COM, polygone de support, fil d'aplomb |

Où le reste est parti :

| | module | espace de noms | volume |
|---|---|---|---|
| clips, keyframes, échantillonnage, player, blend 1D/2D, HFSM, reciblage, éditeur de pose-clés, motion path | `Kernel/Runtime/NKAnima` | `nkentseu::anim` | **3 456 l.** |
| masse/COM, équilibre, contacts, correction de pose et de clip | `Kernel/Runtime/NKAnimPhysics` | `nkentseu::animphys` | **1 621 l.** |

**NKRenderer est désormais CONSOMMATEUR de ces deux modules**, et son code le dit :
dans ses signatures, tout ce qui est préfixé `anim::` ou `animphys::` vient d'un
substrat. Une frontière qu'on lit vaut mieux qu'une frontière qu'on documente.

**Motif, mesuré** : sur les 5 568 lignes d'origine, **438 seulement — 7,9 %**
touchaient au rendu. Sept en-têtes sur onze n'incluaient que Foundation et
écrivaient eux-mêmes leur indépendance (« Pure Foundation : AUCUN GPU, headless »).
Conséquence supprimée : NkAnima, PV3DE et NKScena devaient tirer **tout** le
renderer pour animer, et une application 2D ne pouvait pas animer du tout.

⚠️ **`BakeFromGLTF` n'est plus une méthode de `NkAnimationClip`.** C'était le seul
lien entre le modèle d'animation et le chargeur glTF, et il suffisait à retenir
toute l'animation ici. C'est désormais une **fonction libre**, du côté qui connaît
le format : `renderer::BakeClipFromGLTF(data, animIdx, fps, out)` dans
`Mesh/NkGLTFAnimBake.{h,cpp}`. Pas d'interface, pas de virtuel — même motif que la
suppression de la seconde structure demi-arête le 2026-07-26.

⚠️ **`Tools/IK/NkIKSystem` n'a PAS déménagé** et reste LA référence IK du dépôt.
Son énumération `NkIKSolver` a été renommée **`NkIKMethod`** : elle désigne une
*méthode* de résolution, et le nom devenait dangereux depuis que `Noge::NkIKSolver`
existe pour désigner une *classe* d'adaptation.

📍 **`Tools/Director/NkRoleContext` (555 l.) attend encore ici**, pour la même
raison que l'animation y attendait : c'est le périmètre de **NKBehavior**, qui
n'existe pas. Ne pas le déplacer vers rien.

---

## 🥽 NOTE DE COORDINATION — chantier NKXR / stéréo (agent XR, 2026-08-10)

Le chantier XR (`Kernel/Runtime/NKXR`, `XR_MISSION_IA.md`, branche de travail
worktree `Nkentseu-xr`) a livré son étage 0 (validé par Rihen le 2026-08-10) :
la démo `NKXRDemo` rend déjà la stéréo côte à côte **sans toucher aux passes**
— un `NkRenderer` complet PAR ŒIL en offscreen partagé (patron NK3DModeler),
c'est la « V1 deux passes vers deux cibles » prévue par la mission. L'étage 1
demande maintenant DE la coordination, d'où cette note. Interlocuteur : agent
NKXR ; rien ici ne sera fait sans elle.

**Ce que l'étage 1 touche (minimal, hors fichiers verrouillés) :**
1. `Core/NkRendererTypes.h` + `Core/NkCamera.{h,cpp}` — **frustum décentré** :
   champs `useFovAsym + fovLeft/Right/Up/Down` (radians signés, convention
   XrFovf) dans `NkCamera3DData`, branche dédiée dans `RebuildImpl` avec la
   MÊME convention que le chemin symétrique (colonne-majeure, profondeur
   [-1,1], `w = -z_vue`) → pour un FOV symétrique, matrice IDENTIQUE au pixel
   près (c'est le critère de non-régression). Le culling suit tout seul : les
   plans de frustum sont extraits de `viewProj` (Gribb-Hartmann). AUCUNE passe
   modifiée. La projection de référence côté NKXR : `NkXrProjectionFromFov`
   (`NKXR/NkXrTypes.h`), testée aux bords du clip (self-test 66/66).
2. Rien d'autre à ce stade. Le partage d'un seul graphe pour deux vues (une
   seule shadow map, un seul culling — aujourd'hui tout est ×2) et le
   **multiview Vulkan** sont des chantiers ULTÉRIEURS qui toucheront le
   RenderGraph et les passes : ils feront l'objet d'une NOUVELLE note et d'un
   accord explicite AVANT toute modification.

**Exigence de Rihen (2026-08-10) — ANTI-ALIASING performant en XR** : en
casque, l'aliasing scintille à chaque micro-mouvement de tête ; le FXAA actuel
ne suffira pas à terme.

### ⚠️ Constat 2026-08-12 (agent NKXR) — `NkRendererConfig::msaaSamples` est un champ MORT
`grep` sur tout `Kernel/Runtime/NKRenderer/src` : le champ est **déclaré et
jamais lu**. Le MSAA n'existe donc PAS dans NKRenderer — le brancher n'est pas
un réglage mais un chantier réel : cibles multi-échantillonnées (couleur ET
profondeur), attachement de résolution dans les passes qui écrivent
`mainColor`, post-process qui lit l'image résolue, et le tout sur 4 backends.
**Rien n'a été touché** ; c'est signalé pour que le champ ne trompe personne
(un `msaaSamples = 4` dans une config donne aujourd'hui un silence, pas du
MSAA — le pire des retours).

**Ce que l'agent XR fait en attendant, sans toucher à NKRenderer** :
supersampling par la résolution (`NK_XR_RENDER_SCALE`, mesuré : 1872×1886 par
œil tient 68-71 i/s sur Quest 2 + 3070 Laptop) et essai du **TAA existant, un
historique par œil** — c'est gratuit dans l'architecture XR actuelle puisque
chaque œil a SON renderer, donc son propre historique : le piège classique du
TAA stéréo (historique partagé → fantômes) ne peut pas se produire ici.

**Quand le MSAA se fera**, l'ordre proposé : (1) `NkOffscreenDesc.samples` +
cibles MSAA + résolution dans la passe Geometry ; (2) vérifier les passes qui
lisent `mainColor`/`mainDepth` (SSAO, planar, bloom) ; (3) exposer via
`NkRendererConfig::msaaSamples` — enfin vivant. Interlocuteur : agent NKXR.

---

## ⚠️ Trois pièges du RenderGraph et des passes plein écran (mesurés, 2026-07-30)

Découverts en finissant le TAA. Les trois produisaient une image **d'apparence
plausible** : aucun ne se voit sans mesurer.

1. **Une passe SANS attachement ne fait PAS transitionner les transients qu'elle
   lit.** Le graph ne pose la barrière `COLOR_ATTACHMENT → SHADER_READ` que pour
   une passe déclarant un vrai attachement ; `Reads(id)` seul ne suffit pas →
   l'échantillonnage renvoie du **noir**, silencieusement. Ça semble marcher tant
   qu'une AUTRE passe fait la transition à votre place (la passe `AutoExposure`
   lit `mainColor` sans attachement et fonctionne, mais seulement parce que
   `PostProcess` le relit juste après AVEC un attachement). **Règle** : toute
   passe qui échantillonne un transient doit déclarer une vraie cible ; si sa
   sortie est persistante, en faire un transient DU GRAPH plutôt qu'une cible
   possédée par le sous-système.
2. **Le cache de framebuffers est indexé par NOM DE PASSE, et le graph persiste
   entre frames** → une passe ne peut pas changer de cible d'une frame à l'autre.
   Un ping-pong se fait donc avec des cibles FIXES et une passe de copie, pas en
   alternant l'attachement d'une même passe.
3. **`yFlipUV` (échantillonnage) et `ndcYSign` (reconstruction écran) sont deux
   quantités distinctes** qui divergent par backend : jamais les faire partager
   un slot de push-constant. Et le `yFlipUV` à utiliser dépend de la TEXTURE
   LUE : copier la convention d'une passe qui lit *autre chose* (ici le deferred
   lighting au lieu du FXAA, qui lit le même `ToneLDR`) donne une image
   retournée. Une copie entre deux cibles off-screen doit PRÉSERVER
   l'orientation — ce n'est pas la convention du blit vers l'écran.

Corollaire : **un effet temporel doit être désarmé à chaque rebuild du graph**
(`mTAAHasPrev = false`), car le rebuild recrée ses transients vierges — sinon
l'écran s'assombrit puis remonte sur ~20 frames à chaque resize, changement
d'option ou redirection de cible (capture / enregistrement).

---

## 🔎 Audit d'implémentation (2026-06-24) — état réel code vs roadmap

**Cœur forward RÉELLEMENT implémenté et fonctionnel** : Render3D, RenderGraph, Shadow
(NkVirtualShadowMaps cascades), Environment/IBL CPU, Planar Reflection, VoxelAO,
Materials/Shader, Render2D, Text, Overlay, Offscreen, VFX (CPU), Animation (+skinning GPU
câblé). C'est ce qui tourne sur les 11 démos et les 4 backends GPU.

**Partiels (cœur, trou identifié)** :
- PostProcess : tonemap ACES + bloom OK ; **FXAA DÉJÀ câblé au RenderGraph** (split
  tonemap→`ToneLDR` puis passe `FXAA_Final`→swapchain, flag `cfg.postProcess.fxaa`, shader
  `PP_FXAA/NkSL/pp_fxaa.nksl` FXAA 3.11 ; validé exécutant sur OpenGL 2026-06-25 — la mention
  « non câblé » était périmée). Reste : LUT 3D dégradé en dummy 1×1 sur OpenGL.
- Animation : tracks/blend réels ; **skinning GPU RÉEL sur GL/VK/DX11/DX12** —
  shader `Skin/NkSL/skin.vert.nksl` (source NkSL unique, bones UBO set=1 binding=4
  depuis le fix collision LightsUBO 2026-07-03), `EnsureSkinPipeline`+`FlushSkinned`.
  La note « DX12 bloqué SSBO » était PÉRIMÉE : réparé 2026-06-27 (bones→TEXCOORD2/3)
  puis migration NkSL. **morph targets = stub** (`ApplyMorphTargets` return base,
  re-vérifié 2026-07-12) ; pas de state machine / blend tree / retargeting.
- Render2D : **`DrawSpriteGlow` = stub** (fallback DrawSprite).
- Mesh : **loader glTF 2.0 LIVRÉ** — `NkGLTFLoader.{h,cpp}` from-scratch zero-STL.
  `.gltf` (JSON + .bin externe + data URI base64) et `.glb` (chunks JSON/BIN). Attributs
  POSITION/NORMAL/TANGENT/TEXCOORD_0/1/COLOR_0 ; indices u8/u16/u32→u32 ; normales calculées
  si absentes ; AABB global + par-submesh ; un NkSubMesh par primitive. **MATÉRIAUX PBR
  LIVRÉS (2026-06-25)** : `NkGLTFMaterial`/`NkGLTFImage` (baseColor/metallicRoughness/normal/
  emissive/occlusion, décodage data URI/externe/.glb via NKImage) + pont `NkGLTFMaterialBridge`
  → `NkMaterialInstance(DefaultPBR())` (API publique). **SKINNING LIVRÉ (2026-06-25)** :
  JOINTS_0/WEIGHTS_0 + `skins`/inverseBind + hiérarchie `nodes` + `animations` (LINEAR/STEP/
  CUBICSPLINE, slerp) + `EvaluateGLTFPose(t)`. Câblé dans `NkMeshSystem::Import`. Démos :
  `renderdemo --demo=12` (rubber_duck texturé) + `--demo=13` (Khronos SimpleSkin animé).
  Validé `gltftest` (rubber_duck 5676 v / 33216 i). **DIFFÉRÉ** : morph targets, cameras/lights
  glTF, KHR extensions, sparse accessors, ombres du mesh skinné (pose de repos), DX12 skin.

**⚠️ Couche « v4.0 » ORPHELINE — compile mais JAMAIS instanciée ni exposée par `NkRenderer`/
`NkRendererImpl`** (le renderer ne les utilise pas ; code à finir/brancher, pas à réécrire) :
- **Deferred** (`Passes/Deferred/NkDeferredPass`) : G-buffer 5 RT réel + barrières, **mais
  passe de lighting absente** (« tiled dispatch would be done here ») + non branché → renderer
  reste 100 % forward.
- **Streaming** : files/LRU/budget codés, **aucune E/S réelle** (`FinalizeLoad` ne charge rien,
  `ComputePriority` return 1.0) → simulateur de comptabilité mémoire.
- **IK** : rigs/chaînes OK, FABRIK a sa boucle mais sur positions placeholder {0,0,0}, ne
  lit/écrit jamais les bones → non fonctionnel ; TwoBone/CCD/Spline = squelettes.
- **Culling** : octree + frustum **réels** mais orphelins (jamais branchés au pipeline).
- **Denoiser** (OIDN/NRD `return false`), **AIRendering** (IssueCopy vide), **Voxel-sculpt**,
  **PixolSculpt** : partiels/stubs, orphelins.

**Reste à faire priorisé (re-vérifié à l'audit 2026-07-12)** :
1) ~~Culling frustum de base~~ **précision d'audit + complément 2026-07-12** : le frustum cull
   caméra était DÉJÀ actif pour l'opaque (`Submit` → `NkCamera3D::IsAABBVisible`, casters
   d'ombre collectés AVANT le cull) ; ajouté le **cull par batch des INSTANCIÉS** au Flush
   (2 chemins GPU/fallback, pas en passe miroir, passe shadow intacte) + **`GetCullStats()`**
   (opaque soumis/cullés + batchs instanciés cullés). Ce qui reste VRAIMENT orphelin =
   `NkCullingSystem` (octree/occlusion HZB/distance/LOD — v2, nécessite un mode retained).
   Limite connue : le miroir reflète la liste cullée par la caméra PRINCIPALE (un objet
   derrière la caméra manque du reflet). 2) **VSM v2 bornés** : shadowOverrides Layered/Toon/
   Anime (absents des .nksl, vérifié) + alpha-tested shadow — ✅ LIVRÉ 2026-07-12 (cf. TODOs V2).
   3) **Finitions Phase L/E petites** : ✅ `SetColorGradingLUT(rgba, size)` LIVRÉ
   (NkPostProcessStack, accessible via `GetPostProcess()`, recréation auto si taille change) ;
   ✅ vraie LUT 3D sur GL LIVRÉE (le dummy 1×1 datait du chemin SPIRV-Cross sampler3D —
   le tonemap NkSL natif marche : validé capture `NK_LUT_TEST=1` teal&orange, 99,4 % pixels
   gradés, zéro crash) ; ✅ `DrawSpriteGlow` RÉEL LIVRÉ + validé capture (halo
   radial visible, demo 9) — intégré au batching (batch dédié marqué glow →
   pipeline Glow2D + PC 96B au Flush, jamais fusionné ; buffers 1-quad v0
   retirés). AU PASSAGE, vrai bug GL 2D corrigé : le descriptor set UNIQUE
   partagé du Flush était écrasé au Submit (exécution différée → TOUS les
   batchs de la frame samplaient la DERNIÈRE texture bindée, ex. l'atlas de
   police du HUD à la place des textures des sprites ; UB sur VK aussi) →
   POOL de 256 sets per-batch, bindings partagés (lights/cookies/shadows/
   normal) répliqués via BindSharedTexture/BindSharedUBO.
   4) ~~Morph targets~~ ✅ **LIVRÉ v1 CPU (2026-07-13)** : import glTF (`primitives[].targets`
   deltas POSITION/NORMAL bakés world, `mesh.weights`, canaux anim `WEIGHTS` plats
   LINEAR/STEP/CUBICSPLINE-dégradé) + `EvaluateGLTFMorphWeights(t)` +
   `ApplyGLTFMorphCPU` (base + Σ w·delta, normales renormalisées) →
   `NkMeshSystem::UpdateVertices` (mesh `dynamic=true`). Asset de test généré
   `Resources/Models/MorphTest/morph_test.gltf` (cube→sphère + étirement Y, anim
   4 s) ; DemoGLTF applique automatiquement si le modèle a des morphs — validé
   capture GL (`NK_GLTF_MODEL=Resources/Models/MorphTest/morph_test.gltf --demo=12`).
   ✅ **Morphs sur meshes SKINNÉS (2026-07-13)** : `ApplyGLTFMorphCPUSkinned` (deltas
   appliqués sur `skinnedVertices` AVANT le skinning GPU, bones préservés, cœur commun
   template) + câblage DemoAnim ; asset test `SkinMorphTest/skinmorph_test.gltf`
   (colonne 2 os qui PLIE pendant qu'un morph la GONFLE, déphasés) — validé captures
   (bulge à t1, coude 70° sans bulge à t2 : les deux coexistent).
   Reste v2 : application GPU (compute). 5) ~~Streaming réel~~ ✅ **LIVRÉ v1
   (2026-07-13)** : `NkStreamingSystem` fait de VRAIES E/S — worker thread dédié
   (disque + décodage CPU : NkImage RGBA8 / loaders mesh gltf/glb/obj), upload GPU
   sur le thread de rendu au Update() (borné maxJobsPerFrame), handles réels
   (`GetTexture/GetMesh`), éviction LRU de vraies ressources (Release) + stream-out
   par distance, priorité 1/(1+dist), échecs sans retry-spam (`GetFailedCount`).
   Self-test `NK_STREAM_TEST=1` 4/4 (5 assets réels + 1 introuvable, budget 6 MiB
   serré → 3 évictions, handles cohérents, budget respecté). ✅ **V2 MIP
   STREAMING progressif (2026-07-13)** : le worker fabrique une version basse
   résolution (`lowResMax`, Resize bilinéaire) uploadée EN PREMIER (texture
   floue instantanée → zéro pop) ; la pleine résolution REMPLACE quand la
   caméra passe sous `refineDist = streamInDist × refineDistMult`
   (`TickRefines`, budget d'uploads partagé, swap de handle — l'appelant
   re-binde en surveillant `GetTexture`). Config ajustable runtime
   (`GetConfig()`). Démo `--demo=19` Stream : allée de panneaux, caméra libre
   (C/WASD), distances réglables live (1/2+Shift), fondu anti-pop.
   Reste v3 : LOD meshes, vraie chaîne de mips partagée (base-level GPU).
   6) ~~Deferred~~ ✅ **LIVRÉ v1 (2026-07-13)** : pipeline DIFFÉRÉ opt-in (`cfg.deferred`,
   `NK_DEFERRED=1` dans renderdemo) — passe `DeferredGeom` (MRT : RT0 albedo+metallic
   RGBA8, RT1 normal world+roughness 16F, RT2 emissive+AO 16F + depth partagée, UN
   pipeline pour tous les opaques) → passe `DeferredLight` fullscreen (G-buffer +
   LightsUBO + ombres atlas + IBL irradiance/prefilter/BRDF-LUT, worldPos reconstruit
   depuis la depth via invViewProj — NDC Y INVERSÉ car les VS 3D négatent Y en clip) →
   passe `ForwardRest` (skybox/instanciés/skins/grille/transparents/debug forward
   par-dessus, même depth). Validé capture GL demo 2 : 73 % pixels ≈ forward, ombres
   + panneau alpha-testé OK, **207 FPS vs 140 forward**. Diag `NK_DEFLIGHT_DEBUG=1/2/3`
   (N/worldPos/albedo). ⚠ FIX NKRHI GL au passage : `CreateFramebuffer` n'appelait
   JAMAIS glDrawBuffers → les MRT 1..N-1 étaient JETÉES (défaut GL = attachment 0 seul).
   **V2 en cours (2026-07-13)** : ✅ COOKIES portés (spot 2D + point cube — parité GL
   passée de 73 % à **91,7 %** vs forward, le X rouge du sol est là) ; ✅ conventions
   NDC/sampling PAR BACKEND dans le PC (GL : sample direct + ndcY=-1 ; VK : sample
   direct + ndcY=+1 ; DX : sample flippé + ndcY=-1) ; **MULTI-BACKEND
   (feu vert Rihen, 2026-07-13)** : la CAUSE RACINE de VK sombre + DX12 RT1..2
   mortes était le **blend à 1 seul attachement** sur un render pass à 3 cibles
   (VK exige attachmentCount == N ; DX12 laissait RT1..7 avec write mask 0) —
   fix : le pipeline G-buffer déclare 3 blends opaques (le RP VK et le PSO DX12
   savaient DÉJÀ faire du MRT). ✅ **GL = référence** (91,8 % parité, purger
   `cache/shaders` si le X rouge manque) ; ✅ **VULKAN VALIDÉ capture** (mêmes
   conventions que DX : sample flippé + ndcY négatif — l'essai « sample direct »
   donnait l'image inversée) ; ✅ **RAYONS PARASITES DX11/DX12 RÉSOLUS
   (2026-07-23, a6c71299)** : le signe NDC Y de la reconstruction worldPos
   était codé en dur (-1) alors que le VS flippe vUV sur DX → worldPos MIROITÉ
   verticalement → le cône du spot à cookie touchait les positions miroir
   (rayons en éventail) ; le shader consomme désormais pc.invResolution.x
   (ndcYSign : +1 DX, -1 GL/VK). **DEFERRED CORRECT SUR LES 4 BACKENDS**
   (captures : DX11+DX12 alignés sur GL, VK non régressé) ; ✅ **DX12 : DEVICE
   REMOVED RÉSOLU (2026-07-23, 269207c9)** — 4 causes racines (root constants 64 o
   débordés, release PSO immédiat → destruction différée, cache variantes
   NkUnorderedMap défaillant → NkVector, RowPitch readback) — détail dans
   « Bugs/quirks connus » ; le deferred DX12 tourne à 409 FPS avec capture
   réelle. Limites restantes : clearcoat/subsurface/velocity non portés,
   passe miroir non différée, boucle 32 lumières (tiled/clustered = v3), DX12 à
   capturer.
   Ancien plan :
   l'existant `Passes/Deferred/NkDeferredPass` = G-buffer 5 RT + buffers lumières, SANS
   shaders ni branchement. Briques : **(a)** shaders NkSL `DeferredGeom` (variante du PBR
   vert/frag écrivant en MRT : RT0 albedo+metallic RGBA8, RT1 normal+roughness RGBA16F,
   RT2 emissive+AO RGBA16F — velocity différée) ; **(b)** shader `DeferredLight` plein
   écran (fullscreen triangle, lit G-buffer + LightsUBO + ShadowSlots + IBL → accum HDR
   RGBA16F ; v1 = boucle 32 lumières par pixel, tiled/clustered = v2) ; **(c)** branchement
   `NkRendererImpl::RebuildRenderGraph` derrière `cfg.deferred` (défaut OFF) : pass Geometry
   MRT → pass Lighting → alimente la chaîne post existante (bloom/tonemap inchangés) ;
   reconstruction worldPos depuis depth (invViewProj). Prérequis vérifiés : le RenderGraph
   gère les MRT via SetColor(0..3), les transients RGBA16F existent (HDR path). 7) ~~IK renderer~~ ✅ **REQUALIFIÉ (2026-07-13)** :
   il n'existe qu'UN module IK (`Tools/IK/NkIKSystem`) et il N'EST PLUS orphelin — rendu
   fonctionnel par NkAnima M0 (3240b1ae : FABRIK/TwoBone/CCD sur positions réelles via
   `BindPose`/`EvaluateGLTFWorldJoints`, validé DemoIK + DemoIKChar). La note d'audit
   « placeholder {0,0,0} » datait d'avant M0. Rien à supprimer : c'est L'IK de NkAnima. 8) ~~Animation avancée~~ ✅ **LIVRÉ v1 (2026-07-13)** :
   **NkBlendTree1D** (N clips sur un axe paramétrique, blend BONE-LOCAL TRS-NLerp par os
   AVANT le FK — correct sur les rotations — + phases synchronisées via temps normalisé
   sur durée interpolée) + **NkAnimStateMachine** (états clip/blend-tree, transitions
   par paramètres bool/float + seuil, crossfade, any-state) + helper partagé
   `NkBlendLocalTRS`. Validé : Fox Survey/Walk/Run en fondu continu (captures — galop
   plein à param 1.94, mix cohérent à 0.57) + self-test SM 3/3 (`NK_ANIM_SMTEST=1`).
   Demo : `NK_SKIN_MODEL=Resources/Models/Fox/Fox.glb --demo=16`.
   ✅ **v2 COMPLÈTE (2026-07-13)** : **NkBlendTree2D** (N clips à des points 2D,
   pondération inverse-distance Shepard p=2 + hit exact, blend bone-local CUMULATIF
   avant FK, phases synchro durée pondérée) ; **SM crossfade BONE-LOCAL** (les états
   clip/tree exposent leur pose locale pré-FK via GetLocalPose/GetSkeletonClip —
   blend TRS par os puis UN FK ; fallback matriciel sinon) ; **événements de
   transition** (`SetTransitionCallback(from, to, finished)` au déclenchement et à
   la fin du fondu). Self-tests 5/5 (`NK_ANIM_SMTEST=1` : SM 3/3 + events 2+2 +
   blend2D mix/exact). 9) Metal + Software.
   10) Phase T.1 bake (nouveau chantier) ; T.2 graphe matériaux = ATTEND la coordination NKGraph.

## 🧭 Éditeur / Viewport (chantier 2026-07, cap « famille d'éditeurs »)

Socle d'un viewport d'édition façon Blender (testbed `renderdemo --demo=2`, futur socle
éditeur partagé). Détail + plan : mémoire `project_editor_gizmo_20260704` /
`project_editor_viewmodes_meshedit_plan`.

- ✅ **Gizmo 3D réutilisable** `NkGizmo3D` (`Core/NkGizmo.h`, header-only, découplé de
  NKEvent/NkRender3D) — translate/rotate/scale/combiné, poignées axe(1)/plan(2)/uniforme,
  orientation Global/Local/Normal, multi-sélection (pivot barycentre OU origines
  individuelles en Local), **snapping Ctrl** (pas configurables) + **verrou d'axe X/Y/Z**.
  Overlay via **nouvelle option moteur `NkRender3D::DrawDebugLine(..., overlay=true)`**
  (pipeline debug-line **depth-OFF** `mLinePipelineNoDepth`). Contrôleurs caméra réutilisables
  `NkOrbitCameraController3D` / `NkFlyCameraController3D`. Grille infinie `SetInfiniteGridEnabled`.
- ✅ **Modes d'affichage LIVRÉS (2026-07-05)** — touche Z cycle **6 modes**
  RENDERED / SOLID (matcap) / WIREFRAME / NORMAL / UV / AO ; wireframe via variante
  `pipelineWire` par template matériau (`NkMaterialSystem::SetWireframe`, rasterizer
  natif GL/VK/DX) ; uniforme `viewMode` + `matcapId` dans le CameraUBO ; **5 matcaps**
  (touche M : Studio/Clay/Metal/Toon procéduraux + Chrome texture, binding 28 global).
  ⚠️ Piège : le bloc CameraUBO doit rester IDENTIQUE entre pbr.vert et pbr.frag .nksl.
  Reste optionnel : mode DEPTH ; demande future Rihen = matcap par OBJET en RENDERED.
- ✅ **Edit Mode mesh LIVRÉ côté démo (2026-07-05, testbed Demo3D)** — TAB objet/édition,
  sélection **vertex/arête/face** (1/2/3, combinables Shift), pick rayon Möller-Trumbore,
  déplacement via `NkGizmo3D` (groupe au centroïde), **extrude (E) / delete (X) / merge (M) /
  create face (F)**, recalcul normales, X-ray Alt+Z, **batch GPU persistant**
  (`SetEditOverlayLines/Tris/Points`, ~145 FPS sphère dense), persistance par objet.
  Moteur : `NkMeshSystem` cache CPU (`keepCPU`) + `UpdateVertices(Range)` ;
  `NkRender3D::DrawDebugTriangle` ; purge debug-lines O(n). **`NkEditMesh` half-edge
  (Mesh/NkEditMesh.{h,cpp}) Phase 1a** compile — RESTE : quadify + câblage éditeur (1b),
  ops topo n-gon (2), import (3) — cf. mémoire `project_editmesh_halfedge_plan`.

## ✅ Livré

### Capture & enregistrement vidéo ✅ (2026-07-12) — pipeline complet
- ✅ **Readback GL réparé** (NKRHI `MapBuffer` : PERSISTENT/COHERENT illégaux
  sur storage mutable → 1282 ; flags par usage READ/WRITE) — capture sur
  GL **et** DX11 validée (images identiques), flip Y GL dans
  `NkOffscreenTarget::ReadbackPixels`.
- ✅ **`NkFrameCapture`** (Tools/Offscreen) : capture ASYNCHRONE — ring de
  staging buffers + fences (`Submit(signalFence)` + `IsFenceSignaled`),
  `EnqueueCopy` non bloquant (ring plein = frame sautée, jamais de stall),
  `Poll` non bloquant livrant RGBA8 top-down (flip GL auto) → consommable
  par un thread encodeur/tutoriel/réseau. Zéro `WaitIdle` en régime.
- ✅ **renderdemo `NK_CAPTURE=<frame>`** (PNG one-shot, validation headless
  des agents) et **`NK_RECORD=<out.mp4>`** (+`NK_RECORD_FPS`, défaut 30) :
  rendu → NkFrameCapture → `NkVideoRecorder` NKMedia (H.264, encodage
  threadé). **Prouvé bout-en-bout** : demo3 GL → mp4 h264 1280×720 30 fps
  6.4 s / 193 trames, lisible ffprobe/ffmpeg, contenu vérifié.
- ✅ **V2 « voir + enregistrer » (2026-07-12)** — finalement SANS toucher
  NKRHI : passe **MirrorPresent** dans le RenderGraph (blit plein-écran de la
  cible redirigée vers le vrai swapchain, shader `Blit/NkSL`, ~1 draw) via
  `SetFinalColorTargetMirror(target, true)`. La fenêtre reste vivante pendant
  l'enregistrement, rendu à pleine vitesse (HUD 144 FPS mesuré en record).
  2 pièges corrigés : descriptor set DÉDIÉ au blit (le set partagé avec FXAA
  était écrasé au Submit sur GL — exécution différée → FXAA lisait sa propre
  cible = image noire) ; flip Y écran par backend (DX/VK flip, GL direct).
  Protections : resize pendant record = arrêt propre ; drainage final borné.
- ⚠️ **PLAFOND ENCODEUR MESURÉ** : le H.264 CPU soutient ~10 fps en 720p.
  Défaut NK_RECORD_FPS = 10. ✅ Côté NKMedia (commit 0bbaabcb) : file
  **bornée** (maxQueuedFrames=32, drop-newest) + stats
  `QueueDepth()/DroppedFrames()/EncodeFps()` — plus de saturation RAM
  possible ; renderdemo auto-régule (saute l'échantillon si file ≥ 24) et
  logue les stats à l'arrêt. Reste (NKMedia) : mode MJPEG pour cadence haute.
- ✅ **Toggle à chaud + zone (2026-07-12)** — renderdemo : **touche INSER** (ex-F9, conflit demo 2)
  démarre/arrête l'enregistrement en cours de session (noms auto
  `nk_record_NNN.mp4`) ; `NK_RECORD_RECT=x,y,w,h` n'enregistre qu'une ZONE
  (crop CPU au push, w/h alignés 2, clamp fenêtre, arrêt propre au resize).
  Côté moteur tout est activable/désactivable au runtime
  (`SetFinalColorTargetMirror` ↔ handle nul). Doc :
  `wiki/Runtime/NKRenderer/Capture.md` + README racine.
- ✅ **V3 résolution d'export indépendante (2026-07-12)** : côté moteur
  `NkRenderer::SetRenderSizeOverride(w, h)` — rend TOUTE la 3D à la
  résolution demandée (RenderGraph/post-process/offscreen) SANS toucher le
  swapchain de la fenêtre (`ApplyRenderSize(touchDevice=false)`), la passe
  MirrorPresent fait le pont (viewport par-pass). renderdemo :
  `NK_RECORD_W/H` (ex. 3840×2160 natif pendant affichage 720p, alignés 2).
- ✅ **Finalisation MP4 asynchrone (2026-07-12)** : `recorder->End()` draine
  la file d'encodage (des secondes) — sur le thread de rendu ça FIGEAIT
  l'app au F9-stop. Fix renderdemo : l'affichage est restauré immédiatement,
  puis un `NkThread` dédié prend possession du recorder (heap NKMemory) et
  fait `End()+Delete` en fond. Pattern de référence documenté dans le wiki.
- ✅ **Qualité vidéo + anti-firefly (2026-07-12)** : `NK_RECORD_QP` (10..40,
  défaut 24 — plus bas = moins de blocs de compression) ; **Karis average**
  dans la 1re passe de bloom downsample (poids 1/(1+luma) par quad) — borne
  les fireflies spéculaires (métal roughness basse → lobe GGX en milliers)
  qui explosaient en rectangles violets géants dans les mips grossières
  (constaté en capture lossless DX11 + vidéo VK ; HDR source innocenté,
  max 22.25). Shaders : `PP_BloomDown/NkSL` + fallback VK synchronisé.
- ✅ **MJPEG + touche INSER (2026-07-12)** : `NK_RECORD_CODEC=mjpeg` +
  `NK_RECORD_MJPEG_Q` câblés sur l'API NKMedia 8c926507 (intra pur, 30-60 fps
  sans scintillement de blocs) ; toggle d'enregistrement déplacé **F9 → INSER**
  (F1-F12 toutes prises par les démos). Demo3D : panneau « feuillage »
  alpha-testé ajouté (disques troués, `SetCastShadowAlphaTest`) pour valider
  visuellement l'ombre trouée du pipeline Shadow_AlphaTest.
- ⏳ Reste : audio dans l'enregistrement.

### Fondations (Phase A → D.3d) — toutes livrées
- PBR forward avec UBO push-constant
- IBL CPU (Lambert irradiance + GGX prefilter + BRDF LUT)
- CSM 1-cascade + soft shadows PCF Poisson + PCSS contact-hardening
- Ring buffer UBO multi-frame
- Tonemap ACES post-process

### Phase D.4 — NkVirtualShadowMaps v0 + v1.A/v1.B ✅ (2026-05-23) ⭐
Refactor majeur shadow system : remplace `NkShadowSystem` (CSM mono-light)
par `NkVirtualShadowMaps` (multi-lights). Style UE5 simplifié.

**V0 — Infrastructure**
- ✅ **Multi-lights shadow** : DIR (CSM cascades) + SPOT (1 tile) + POINT
  (cubemap virtuel 6 faces) dans un seul atlas D32_FLOAT 4096²
- ✅ **Atlas dynamique rectpack skyline** ([NkShadowAtlasPacker](src/NKRenderer/Tools/Shadow/NkShadowAtlasPacker.h)) :
  budget 256 slots, allocation per-frame
- ✅ **Helper sampling unifié** `.glsli` (`SampleLightShadow(lightIdx,...)`)
  intégré dans PBR/Layered/Toon/Anime
- ✅ **Anti-flickering** : mode radius FIXE par cascade (8/16/32/64) +
  center=camPos + texel snap XYZ + ring UBO multi-frame (3 frames in flight)
- ✅ **Validé Demo3D VK + GL** : sun + 2 point lights (red+blue) + 1 spot,
  tous projettent ombres correctement, 17 slots actifs

**V1.A — Cascade fade** ✅
- Blend smooth sur les 15% derniers d'une cascade vers la suivante
- `fadeT = (absDepth/splitFar - 0.85) / 0.15`, clamp [0,1]
- Coût : 2 PCF samples dans la bande de transition (~15% des fragments)

**V1.B — Shadow caching per-light** ✅
- Nouveau flag `NkLightDesc::shadowStatic` (défaut false, safe re-render)
- `NkLightShadowCache` track position/direction/range entre frames
- Si TOUS slots cached → skip render pass entière (preserve atlas)
- Per-tile caching V2 (besoin ClearRect API au RHI)
- Overlay debug `slots: 17 (rend N | cache M)` dans Demo3D

**V1.C — Normal bias world-units** ✅
- Push worldPos le long de N en world units (0.05 = 5cm) avant projection shadow
- Fix peter-panning (décollement ombres pied de caster)
- shadowBias (NDC) réduit de 0.003 → 0.0005 grâce au normal bias

**V1.D — Per-material shadow override** ✅
- `NkMaterial::SetReceiveShadow(bool)` — skip shadow sample sur ce material
- `NkMaterial::SetShadowBiasMul(float)` — multiplicateur du bias
- `NkMaterial::SetCastShadowAlphaTest(bool)` — V2 reserve
- ObjBlock UBO étendu : +`vec4 shadowOverrides` (192 → 208 bytes)
- Helper shader : `SampleLightShadowEx(..., biasMul)` + wrapper compat
- Actif sur **PBR** ; Layered/Toon/Anime ignorent l'override (TODO V2)

**TODOs V2**
- ⏳ **ClearRect API au RHI** : caching per-tile (au lieu de all-or-nothing)
- ⏳ **Dynamic offsets UBO** : scale à 10k+ draws sans descriptor sets
- ⏳ **LOD tile size** adaptatif (distance light/cam)
- ✅ **Shadow override Layered/Toon/Anime** (2026-07-12) : ObjectUBO étendu
  (+shadowOverrides/+triplanarParams, identique dans les DEUX stages — le
  linker GL exige des déclarations de bloc identiques) dans toon/anime/
  layered .vert+.frag ; `SampleLightShadowEx(..., biasMul)` + garde
  receiveShadow (.x) câblés dans les 3 frags. LayeredV1 hors scope (ne
  sample pas d'ombres). C++ inchangé (ObjBlock déjà rempli pour tous).
- ✅ **Alpha-tested shadow** (foliage) (2026-07-12) : shaders
  `ShadowAlpha/NkSL` (VS pos+UV → FS sample tAlbedo, discard < 0.5) +
  pipeline `Shadow_AlphaTest` (layouts [global, object, GetInstanceLayout]
  → binde `matInst->GetDescSet()` tel quel) ; sélection par-caster dans
  RenderShadowPass quand `SetCastShadowAlphaTest(true)` (re-push PC après
  switch de PSO — DX12 invalide les root params). Piège résolu : le
  générateur GLSL injectait le flip Y NDC dès inputs+varyings → pragma
  commentaire **`@gl-no-flip-y`** (NkShaderBackend) pour les VS qui rendent
  dans l'atlas avec des varyings. **VALIDÉ visuellement (Rihen, 2026-07-12)** :
  panneau feuillage Demo3D → ombre à points (trous respectés) sur OpenGL.
  ⚠️ Piège corrigé au passage : `NkMaterial::Create(sys, "PBR")` échoue en
  silence (le template s'appelle `"Default_PBR"`) — préférer l'overload par
  TYPE (`NK_PBR_METALLIC`).
- ⏳ **Page-based VSM réel** UE5 (refactor 16k² atlas virtuel pagination 128²)

### Phase G — NkMaterialSystem ✅
- `NkMaterialAsset` (.nkasset JSON) + `NkMaterialInstance`
- Hot-reload des `Resources/NKRenderer/Materials/*.nkasset`
- Built-in : PBR, Toon/Anime, Glass, Skin, Hair, CarPaint, Cloth, Foliage,
  Volume, Water, Particles, Layered (16 dossiers)
- `NkDrawCall3D::material` wired ; metallic/roughness direct shortcuts conservés

### Phase G.ext — Matériaux avancés style UE5 ✅ (sauf M.6, M.7)
- **M.1** ✅ Material Layering (v0 + v1 N=8 layers, Demo8 dédié)
- **M.2** ✅ Material Parameter Collections (Demo5)
- **M.3** ✅ Blend par vertex color (Demo5 painted cube)
- **M.4** ✅ Instances hiérarchiques parent/enfants + override (Demo6)
- **M.5** ✅ Material Functions `.glsli` + #include (Demo7)
- **M.6** ⏳ Vertex Paint runtime (TODO — `mesh->PaintVertex(idx, color)`)
- **M.7** 🚫 Decal Materials (bloqué — besoin G-Buffer depth+normal)
- **M.8** ✅ Multi-slot par sous-mesh (Demo5 cube 6 faces différentes)

### Phase H.6 — Voxel AO v0 ✅ (2026-05-22)
- ✅ NkVoxelAOSystem 64×32×64 RGBA8 + bake CPU + cone-trace 4 cônes×8 samples
- ✅ Bind atlas binding=27 sur globalSet + mirror ring
- ✅ Application dans pbr.frag.vk.glsl : atténue IBL irradiance + specular
- ⏳ V1 TODO : .glsli générique pour Layered/Toon réutilisable

### Phase H.6 v1 — GI à UN REBOND ✅ (2026-07-30)
La grille de voxels porte désormais la **radiance réémise**, pas seulement
l'opacité : l'éclairage indirect vient de la géométrie réelle au lieu d'un
ambiant constant. Structure canonique du cone tracing (Crassin) —
`RGB` = radiance prémultipliée, `A` = opacité — donc **AO et GI partagent un
seul parcours de cônes** : l'AO ne coûte plus rien en plus.

- ✅ `NkVoxelOccluder::albedo` + `InjectLighting(lights)` / `InjectLightingIfDirty`
  (CPU) : éclairage direct par voxel × albédo, normale par **gradient d'opacité**,
  **visibilité par ray-march** dans la grille (c'est elle qui donne les ombres
  portées de l'indirect). `SetGIIntensity` applique le réglage à l'injection —
  aucun uniform ajouté au shader (registres DX contraints).
- ✅ `Include/NkVoxelAO.glsli` : `NkComputeVoxelGI()` (xyz = irradiance,
  w = AO) ; `NkComputeVoxelAO()` conservé pour Toon/Anime/Glass.
- ✅ Branché sur **Layered / LayeredV1** et sur **PBR** (`pbr.frag.nksl`).
- ✅ **MESURE (démo 8, mur rouge éclairé, aucune lumière rouge dans la scène)** :
  sur le flanc de la sphère qui fait face au mur, le rouge monte **1,9× plus vite
  que le bleu** (dR +45,6 contre dB +23,8 à intensité ×3), soit **+15,7 % de
  ratio R/B** ; à intensité 1, dR +18,7 contre dB +14,7. 58 % des pixels
  affectés. Le reste du gain est neutre : c'est le rebond du **sol clair**, qui
  domine légitimement celui du mur — comportement physique attendu, pas un défaut.
- ✅ Le mur de la scène de test est **réellement rendu** (cube sur l'AABB de
  l'occluder, bornes en **source unique** `kWallMin/kWallMax` pour qu'ils ne
  puissent pas diverger). Sans ce draw, la lumière rebondissait sur un mur
  invisible et l'effet semblait sortir de nulle part. ⚠️ Le mur est adossé en
  **+Z** : avec `yaw=0` la caméra orbite sur l'axe **X**, donc un mur en +X se
  planterait entre elle et la sphère.
  Non-régression : sans occluder injecté le terme est nul, rendu identique
  (démo 4 MAD 0,0014 pour un bruit run-à-run de 0,93). Debug + Release verts.
- 🔶 **DÉFAUT PRÉEXISTANT ISOLÉ — la texture voxel n'atteint pas le shader dans
  la démo 4** : ni le GI ni l'AO n'y produisent d'effet. Prouvé par deux mesures
  indépendantes : (1) un mur occluder massif (9064 voxels) ne change rien
  (MAD 0,0013) ; (2) la sonde `NK_GI_DEBUG_FILL=1` (grille entièrement saturée)
  ne change rien non plus (MAD 0,04), alors que la même sonde **sature l'écran
  en démo 8** (MAD 153). Le binding 27 ne remonte donc pas jusqu'au shader de
  cette démo — bug de binding antérieur au GI, à traiter à part.
- ⏳ Suite : injection en **compute** (l'interface et le shader ne bougeront pas),
  mips de la grille pour les cônes longue portée, bounds en uniform (aujourd'hui
  `NK_VOXEL_MIN/MAX_BOUNDS` est codé en dur et doit suivre `NkVoxelAOConfig`),
  et `giScale` synchronisé à la main entre CPU et `NK_VOXEL_GI_SCALE`.
- 🔧 Overrides : `NK_GI_TEST` (1 = scène de validation, 2 = témoin sans mur),
  `NK_GI_INTENSITY`, `NK_GI_DEBUG_FILL` (sonde de diagnostic).

### Phase Planar Reflection ✅ (2026-05-23) ⭐ FIXÉ
NkPlanarReflectionSystem + reflets planaires complets sur sol mirror.

- ✅ **Auto-bake** : user enregistre plan, renderer fait passe miroir avant Geometry
- ✅ **Cross-API VK + GL** validé sur Demo10 (newport_loft HDRI)
- ✅ **4 root causes du bug fixées** (cf. `memory/nkrenderer_planar_reflection_bugs.md`) :
  1. UBO Camera mirror ring dédié (Option B) — overwrite résolu
  2. Un-mirror Y dans VS (worldPos/N/T) + recalc B = cross(N,T) — handedness
  3. MPC + VoxelAO bind sur `mGlobalSetMirrorRing` (Vulkan strict DescriptorSet)
  4. Skybox + PBR IBL : un-mirror sampling direction R

### Phase L — Post-process (largement livré)
- ✅ **Bloom Dual-Kawase 11-pass AAA cross-API** (Jorge Jimenez 2014,
  COD: Advanced Warfare) — 6 downsample + 5 upsample + tonemap 2-textures
- ✅ ACES filmic tonemap avec exposure/gamma/saturation/vignette
- ✅ Fullscreen triangle pattern moderne (gl_VertexIndex sans VBO)
- ✅ Push constant yFlipUV différentiel sub-passes/tonemap par backend
- ✅ Push constants stageFlags fix (NK_ALL_GRAPHICS, VUID-01796) — 2026-05-23
- ✅ **Color Grading LUT 3D** (2026-05-23) — 16³ identity par défaut, sampler3D
  au binding=3 du tonemap, push constant `lutStrength` + `lutSize` avec bias
  texel correct, blend mix(mapped, graded, strength). User upload custom LUT
  via TODO `NkRenderer::SetColorGradingLUT(data, size)`
- ⏳ **FXAA** : shaders externes + pipeline créés (PP_FXAA), wirage RenderGraph
  TODO (besoin split tonemap→mToneTex + FXAA→swapchain, ~30 min refactor)
- ✅ SSAO v0 stable (16 samples poisson, contact AO local) ; GTAO complet
  + voxel AO planifiés (cf. Phase H.5b/H.6 ci-dessous)
- ✅ **Auto-exposure V0** (2026-05-23) — tonemap sample uBloom center (proxy
  luma moyenne via Dual-Kawase upsample), adapte exposure vers
  `autoExposureKey=0.18` mid-gray. Push constant étendu 32→48 bytes.
  Limitations V0 : pas d'eye adaptation temporelle (V1 = compute reduction
  + SSBO double-buffer), précision moyenne (bloom threshold filtre les
  basses luminances).
- ✅ **Auto-exposure V1 — MESURE RÉELLE + ADAPTATION** (2026-07-30) : la V0
  échantillonnait **UN SEUL pixel** (le centre du RT de bloom) comme proxy du
  niveau de la scène → l'exposition était pilotée par ce qui se trouvait au
  milieu de l'écran, et le seuil de bloom écrasait les basses luminances.
  V1 : nouvelle passe `PP_AutoExposure` (`Resources/.../PP_AutoExposure/NkSL/`)
  qui calcule la **moyenne logarithmique** (moyenne géométrique, convention
  Reinhard) de la luminance sur **256 échantillons** de l'image HDR, pondérée
  vers le centre (métrage « center-weighted »), dans une cible **1×1 RGBA16F** ;
  **adaptation temporelle** façon accommodation de l'œil
  (`1 - exp(-dt·vitesse)`, indépendante du framerate) via **ping-pong de deux
  cibles 1×1 persistantes** (conservées à l'OnResize : sinon redimensionner
  provoquerait un flash). Le tonemap consomme la valeur au **binding 4**
  (PC 48→64 o, `p3 = (expMin, expMax)`).
  Config : `autoExposureSpeed/MinLuma/MaxLuma/MinExp/MaxExp` ; overrides de test
  `NK_AUTOEXP`, `NK_AUTOEXP_SPEED`, `NK_EXPOSURE`.
  Le shader n'a **aucune convention Y par backend** (la moyenne logarithmique
  est invariante à l'orientation) — contrairement au bloom et au tonemap.
  **MESURES (demo 2, luminance moyenne de la zone 3D, /255)** : référence
  exposition 1 sans auto = 98,1. Base **0,25** (sous-exposée) : 42,4 → **147,20**
  avec auto ; base **6** (surexposée, 81,7 % de pixels brûlés) : 204,9 →
  **147,17**. Soit **0,02 % d'écart entre deux bases distantes d'un facteur 24**
  = la boucle se ferme bien sur la mesure et non sur l'entrée.
  **Parité 4 backends** : GL 147,20 · Vulkan 147,19 · DX11 147,20 · DX12 147,19.
  Note d'usage : la cible 0,18 donne une image plus claire que l'exposition
  manuelle historique (147 vs 98) — baisser `autoExposureKey` vers ~0,10 pour
  retrouver le rendu d'avant.
  Reste V2 : réduction en **compute** (au lieu de 256 taps dans un fragment).
  ~~histogramme + percentiles~~ → **LIVRÉ le 2026-08-14** (`d226565f`), voir
  l'entrée ci-dessous : histogramme cumulé sur 16 seuils fixes en log-luminance,
  fenêtre [30ᵉ, 98ᵉ] percentile. *(Cette ligne annonçait encore le travail comme
  restant : corrigée le 15/08.)*
- ✅ **Exposition : la chaîne rendue cohérente** (2026-08-14 → 15) — quatre
  défauts distincts, tous mesurés, sur le trajet « ce qui est mesuré → ce qui
  est appliqué → ce qui est transmis » :
  1. l'exposition était appliquée **avant** l'ajout du bloom dans
     `pp_tonemap.frag.nksl` : aucune valeur d'exposition ne pouvait assombrir un
     halo (`d226565f`) ;
  2. `RunAutoExposure` mesurait `mainColor`, **avant** composition du bloom —
     elle ne voyait jamais le halo qu'elle amplifiait ensuite ;
  3. moyenne logarithmique → **histogramme de percentiles** [30ᵉ, 98ᵉ] : `log`
     diverge vers −∞ près de zéro, donc un décor sombre écrasait la mesure ;
  4. **le seuil de bloom s'ancre sur le blanc affiché** (`ca3f5fb8`) et non sur
     le HDR brut, via l'exposition résolue relevée par anneau (`3851e985`).
- ✅ **Exposition résolue : transmission réparée** (2026-08-15) — trois défauts
  liés, trouvés en mesurant le cas 4 (auto puis bloom, tout décocher, bloom
  seul) :
  1. `PumpExposureReadback` portait une branche « auto éteinte » **inatteignable**
     (son unique appelant, `RunAutoExposure`, retourne avant) : personne ne
     posait donc l'exposition manuelle. Réponse déplacée dans
     `ResolvedExposure()` — le point où la question est posée, seul valable hors
     frame, or le graphe se construit hors frame. Branche morte **retirée** ;
  2. `autoExposureStrength` manquait dans la liste de `SetPostConfig` qui salit
     le graphe : cocher l'auto ne créait aucune passe jusqu'au prochain
     redimensionnement. Seuil d'activation unifié en
     `NkPostConfig::kAutoExposureOn` + `AutoExposureRequested()` ;
  3. conséquence des deux : après extinction de l'auto, le seuil de bloom
     s'ancrait sur une exposition **héritée** — mesuré `bloomThr = 1,19` au lieu
     de `6,15` (0,85 × 7,24 / 5,15). Corrigé : `valid=1`, `aeExposure=1`.
  **Banc d'essai** : bloom rallumé sur les démos Sandbox cas 12 (DemoSkin) et 13
  (DemoIK), éteintes des mois durant à cause de ce défaut — plus aucun effet
  mesurable. Deux exécutions du même binaire diffèrent davantage (écart max 53)
  que bloom éteint contre allumé (49).
- ✅ **NkRHI compute audit** (2026-05-23) — compute support OK cross-API
  VK+GL (cf. `memory/nkrhi_compute_support.md`). Déjà utilisé par NkML,
  NkAnimationSystem morph, NkComputeContext wrapper. Foundation prête pour
  Phase N GPU prefilter, auto-exposure V1, Voxel AO v1, Lumen-lite GI.
- ✅ **TAA** (Temporal AA) livré V1 sur les 4 backends (2026-07-30) — détail et
  mesures dans « Phase L — Finition post-process » plus bas
- ❌ DOF/bokeh, Motion blur, vignette/grain chromatic, Lens flares
  — non implémentés

### Phase N — IBL pipeline
- ✅ Phase N v0 : `LoadFromHDR(.hdr)` via NkImage existant + convolution CPU
  IBL irradiance + prefilter (Reinhard tonemap)
- ✅ Phase N v0.5 : Background HDR skybox visible (fullscreen triangle
  + sample cubemap)
- ✅ Phase N v1 : Cubemap dédié skybox `mSkyEnvCube` (RGBA32F sans Reinhard)
  au binding=26 — preserve HDR brut > 1.0
- ✅ Phase I : PBR specular IBL via tSkyEnvCube pour roughness ≤ 0.5
  (mirror) → métalliques recevent bloom HDR
- ✅ **Convolutions GPU compute (2026-07-12)** — `NkIBLCompute.{h,cpp}`
  (Tools/Environment) : kernels NkSL irradiance Lambert + prefilter GGX
  (chemin compute prouvé de NkTensorGpu : NkSL→SPIRV/GLSL/HLSL, SSBO in/out),
  branchés dans `LoadFromHDR` avec **fallback CPU automatique**. Mesuré
  (demo3, HDR 1k, prefilter 256²×6 mips) : **convolution 9-28 ms vs CPU
  79-99 ms** (compile kernels 10-260 ms one-shot par backend). Validation
  numérique `NK_IBL_VERIFY=1` : GL/VK/DX12 **maxDiff 5/255 sur 0.09 %** des
  octets (= trig float GPU). ⚠️ **DX11 : CPU par défaut** (maxDiff 175/255
  sur 0.8 % des texels, fxc cs_5_0 à investiguer ; `NK_IBL_GPU=1` force).
  NB : le cache disque IBL couvrait déjà les runs suivants ; le gain GPU =
  premier chargement + **swap de HDRI à chaud** (éditeur, T.5).
- ❌ Env light probes / reflection probes par zone

### Phase F — Multi-backend (DX au niveau VK)
- ✅ Vulkan + OpenGL testés sur toutes les démos
- ✅ NkShaderConverter VK→GL/HLSL/MSL via SPIRV-Cross + générateurs NkSL→HLSL DX11/DX12 directs
- ✅ **DX11 + DX12 validés à parité avec Vulkan** (2026-06-24, session marathon ~24 fixes RHI/
  shader/renderer). Bugs majeurs résolus : ring sampler overflow DX12 (>64 draws), matrice TBN
  transposée GLSL→HLSL (éclairage mort), mips matériau non générés DX12 (textures blanches),
  conventions Y DX (HDR/bloom/ombres/reflets), #820 clear-value (perf), cache DXIL (démarrage 6×).
  Détail dans mémoire `project_session_20260623_dx12_render_fixes`.
- ⏳ Metal partiellement implémenté (NkRHI compile, runtime macOS pas testé — besoin Mac)
- ❌ Software backend stub uniquement

---

## 🔄 En cours / TODO immédiat

### Phase D.4.2 — NkVSM v2 (extensions futures)
- **ClearRect API au RHI** : caching per-tile au lieu d'all-or-nothing
- **Dynamic offsets UBO** pour ObjectUBO : 1 buffer + per-draw dynamic offset,
  scale à 10k+ draws sans alloc descriptor sets supplémentaires
- ✅ **Shadow override Layered/Toon/Anime** (2026-07-12)
- ✅ **Alpha-tested shadow** (2026-07-12) : pipeline `ShadowAlpha` (VS+FS discard
  albedo < 0.5, set=2 universel des instances) branché sur `castShadowAlphaTest`.
  ✅ **Fix multi-backend (2026-07-22, 1ed4ea37)** — 2 causes racines DX :
  (1) POSITION (DX11+DX12) : les générateurs HLSL négatent Y du VS dès qu'il a
  inputs+varyings, mais ce VS rend dans l'atlas NON présenté → ombre déplacée ;
  pragma `@gl-no-flip-y` étendu aux 2 générateurs HLSL
  (`NkSLCompileOptions::disableAutoYFlip`, câblé NkShaderBackend).
  (2) TROUS absents (DX12) : `BuildGraphicsPSO` n'attachait le PS que si
  numRT>0 → en passe depth-only le FS discard ne tournait JAMAIS → ombre
  pleine ; PS attaché dès qu'il existe (discard-only légal avec 0 RTV).
  Validé capture DX11 = GL (position + trous) ; DX12 validé interactif.
- **LOD tile size** adaptatif : tile petit pour lights loin/dim, gros pour proches
- **Page-based VSM réel** UE5 (long terme, gros refactor 16k² atlas virtuel)

### Phase H.6 v1 — Voxel AO précision
- `.glsli` générique pour Layered/Toon/Anime (pas dupliquer le code)
- GPU bake voxel grid (CPU bake actuel = 1s sur startup)
- Densité voxel runtime adaptative (64³ → 128³ selon scene)
- 🔶 **GI à UN REBOND — v1 livrée côté moteur (2026-07-30), branchement PBR
  BLOQUÉ par un verrou.** La grille porte désormais, en plus de l'opacité, la
  **radiance réémise** par chaque voxel : structure canonique du voxel cone
  tracing (RGB = radiance prémultipliée, A = opacité) dans la texture RGBA8
  existante — donc **aucun binding ni format nouveau** (les registres DX sont
  déjà contraints, cf. [[project_dx_binding_model_plan]]).
  - `NkVoxelOccluder::albedo` + `InjectLighting(lights)` /
    `InjectLightingIfDirty(lights)` : pour chaque voxel occupé, éclairage direct
    (N·L par **gradient d'opacité** — une voxelisation AABB n'a pas de normale)
    × albédo / π, avec **visibilité par ray-march** dans la grille (c'est elle
    qui donne les ombres portées de l'indirect, donc son contraste).
  - `NkComputeVoxelGI()` : AO et GI dans **un seul** balayage de 4 cônes
    (accumulation front-to-back) — l'AO devient un produit dérivé, sans coût
    supplémentaire. `NkComputeVoxelAO()` conservé pour les matériaux qui ne
    veulent que l'AO.
  - Intensité appliquée **côté CPU à l'injection** (radiance nulle = GI éteint)
    plutôt qu'en uniform : zéro binding ajouté, et l'A/B de validation est exact.
  - **MESURE (demo 8, `NK_GI_TEST=1`, scène de color bleeding : mur ROUGE
    éclairé près de sphères)** : canal rouge moyen 20,95 → **24,34 (+16,2 %)**
    contre +5,9 % pour le bleu, alors qu'**aucune lumière rouge n'existe** dans
    la scène — l'écart R−B passe de −6,32 à −4,54. C'est le test canonique du
    GI : la teinte vient de la géométrie, pas d'un ambiant constant.
    A/B : `NK_GI_INTENSITY=0` vs `1`.
  - ⚠️ **BLOQUÉ** : le branchement dans le PBR (le matériau du gros des scènes)
    tombe dans `Resources/NKRenderer/Shaders/{Sel*,PBR}/**`, VERROUILLÉ par le
    chantier modélisation (`Engine/Noge/CONTINUATION.private.md`). Le GI est donc
    branché sur `Layered` et `LayeredV1` (libres) pour la validation. Pour
    l'activer sur le PBR une fois le verrou levé, dans `pbr.frag.nksl` remplacer
    `float voxAO = NkComputeVoxelAO(vWorldPos, N);` par
    `vec4 voxGI = NkComputeVoxelGI(vWorldPos, N); float voxAO = voxGI.w;`
    puis ajouter après le calcul de `amb` :
    `amb += kDi * albedo * voxGI.xyz * ao * uCam.iblStrength;`
    (`NkRender3D::BeginScene` étant lui aussi verrouillé, l'injection est
    déclenchée par l'APPLICATION — ce qui est de toute façon souhaitable tant
    qu'elle est en CPU : c'est la scène qui décide quand elle paie ce coût.)
  - Reste v2 : injection en **compute** (le calcul seul change, ni l'interface ni
    le shader), mips de la grille pour des cônes larges, bounds en uniform (ils
    sont encore en `#define` partagé entre CPU et shader), et plusieurs rebonds.

### Phase H.5b — GTAO complet (papier Activision 2016)
Amélioration incrémentale au screen-space (alternative voxel) :
- Vraie reconstruction view-space depuis depth + invProj
- Cosine-weighted horizon integration analytique
- 8-16 directions de référence
- Cross-bilateral blur avec edge-stopping depth
- Multiplie IBL dans le PBR shader (pas juste post)

### Phase H.5c — Opacity-aware AO/shadows (conditionnel)
Pour les sols/objets transparents, propagation partielle de l'AO/shadow.
4 approches techniques notées dans la mémoire.

### Phase E — Materials 2D + lumière 2D + ombres 2D
*(Audit 2026-07-12 : la ROADMAP sous-vendait — le gros de la 2D éclairée est
LIVRÉ dans `NkRender2D`, seul le glow reste un stub.)*
- ✅ **Lumières 2D** : `SetLights2D` (point lights `kMaxLights2D` + ambient,
  UBO `lights[]` du shader Render2D)
- ✅ **Ombres 2D** : `SetShadowCasters2D` (cercles, E.5) +
  `SetShadowCastersAABB2D` (32 AABB murs/plateformes, E.7a)
- ✅ **Layer masks lumière/shape** (E.7b : `light.layerMask & shape.layerMask`)
- ✅ **Normal maps 2D** (E.7c : binding 12, relief éclairé)
- ✅ `DrawSpriteGlow` **LIVRÉ (2026-07-12)** : batch dédié glow → pipeline
  Glow2D + PC au Flush ; au passage fix du descriptor set 2D partagé écrasé
  (pool 256 sets per-batch) — détail dans « Reste à faire priorisé » point 3

### Phase L — Finition post-process (TODO restants)
- **FXAA wirage RenderGraph** : pipeline créé, manque split tonemap→mToneTex
  + nouvelle pass FXAA→swapchain (~30 min refactor RenderGraph)
- ✅ **Auto-exposure** LIVRÉE V1 (2026-07-30) : mesure réelle (256 taps, moyenne
  logarithmique, cible 1×1) + adaptation temporelle, validée par mesures sur les
  4 backends — détail dans la section « Livré » Phase L ci-dessus
- ✅ **API SetColorGradingLUT(data, size)** LIVRÉE (2026-07-12) + vraie LUT 3D
  sur GL (validé capture `NK_LUT_TEST=1` teal&orange)
- ✅ **TAA** (Temporal AA) **LIVRÉ V1 sur les 4 backends** (2026-07-30) —
  opt-in (`postProcess.taa`, override `NK_TAA`), **exclusif du FXAA** (les
  enchaîner flouterait deux fois). Jitter sub-pixel Halton(2,3) 8 phases dans
  `NkRender3D` + reprojection par la profondeur + clamp de voisinage 3×3 contre
  le ghosting. Pas de velocity buffer (v2) : la reprojection n'est exacte que
  pour la géométrie statique, le clamp couvre les objets mobiles.
  **MESURES (demo 2, régime établi, zone 3D)** — indicateur d'**escalier**
  = part des pixels de bord au gradient purement axial, la signature de
  l'aliasing (référence → TAA) : GL 47,7 → **42,2 %** · Vulkan 46,5 → **41,7 %**
  · DX11 47,7 → **42,2 %** · DX12 47,6 → **42,4 %**, luminance préservée à
  ≤1,9 % près sur les 4. **L'AA vient bien de l'accumulation et non d'un flou** :
  le jitter SEUL (`NK_TAA_BLEND=0`) donne 53,7 %, soit PIRE que la référence.
  Trois défauts trouvés et corrigés par la mesure, tous silencieux (image
  d'apparence correcte) :
  1. une passe sans attachement ne fait pas transitionner les transients qu'elle
     lit → écran noir (cf. le piège en tête de ce fichier) ;
  2. `p0.y` servait à la fois de `yFlipUV` (VS) et de `ndcYSign` (FS), deux
     quantités qui divergent par backend → séparées en `p0.y` / `p1.x`
     (push-constant 80 → 96 o, toujours sous les 128 o Vulkan et DX12) ;
  3. la copie vers l'historique utilisait la convention du blit ÉCRAN au lieu de
     celle de la lecture off-screen → historique inversé sur DX, masqué par le
     clamp : luminance juste à 0,6 % près mais AUCUN antialiasing (escalier
     51,9 % au lieu de 42,2 %).
  Overrides de diagnostic : `NK_TAA_BLEND`, `NK_TAA_CLAMP=0` (le clamp masque
  les erreurs de reprojection), `NK_TAA_YFLIP` / `NK_TAA_NDCY` (conventions Y),
  `NK_TAA_DEBUG=1..4` (historique reprojeté / brut / décalage / profondeur),
  `NK_TAA_PREVLAG=N` (amplifie le mouvement inter-frame), `NK_TAA_PRESENT_HIST`.
  ⚠️ **`ndcYSign` NON validé par mesure** : à caméra fixe les deux signes
  s'annulent exactement (`ndcYSign² = 1` quand `reproj` = identité), donc aucune
  scène de démo actuelle ne les distingue. La valeur retenue suit le deferred
  lighting (validé par capture) ; à trancher sur une scène à caméra mobile via
  `NK_TAA_NDCY`.
  Reste V2 : velocity buffer (objets mobiles), variance clipping, mip bias.
- **DOF/bokeh** : profondeur de champ avec cercle de confusion
- **Motion blur** : object + camera, vélocité buffer
- **Vignette/grain/chromatic/Lens flares** : effets de lens

### Compute infrastructure (NkRHI audit prioritaire)
Avant Phase N GPU prefilter / auto-exposure GPU / Lumen GI : valider que
NkRHI a un compute path solide cross-API. Vulkan + GL ont compute, DX11
limité, DX12+Metal OK. Plan :
1. Audit `NkIDevice::DispatchCompute()`, `vkCmdDispatch` wrapper, GL shader
   storage barriers, etc. (~30 min)
2. Mini démo compute : "double values in buffer" pour valider end-to-end
3. Premier use case : auto-exposure compute reduction (lit HDR mip 0,
   reduce parallèle → 1 float luma écrit dans UBO)
4. Phase N v2 : compute prefilter IBL (~3h, replace CPU 1-2s par <50ms)

### Phase N — IBL pipeline GPU
- Compute shader equirect→cubemap (remplace CPU)
- Compute shader irradiance convolution GPU
- Compute shader prefilter par mip GPU
- Env light probes (sources multiples + blend par zone)
- Reflection probes par pièce/zone (cubemap localisé)

### Bugs/quirks connus
- ✅ **LE SEUIL DE BLOOM NE SUIVAIT PAS L'AUTO-EXPOSITION — facteur 20 mesuré,
  puis CORRIGÉ** (2026-08-15, `2fc4d39f` puis `bbd4a469`). `bloomThr` était
  calculé dans `BuildDefaultRenderGraph` puis **capturé par valeur** dans les
  lambdas des 6 passes. Il ne bougeait donc qu'au rebuild du graphe, alors que
  l'exposition s'adapte à chaque frame.
  **Correctif** : le calcul vit dans `NkRendererImpl::ComputeBloomThreshold()`,
  appelée **dans la lambda** de `Bloom_Down_0` — on capture le *rang* de la
  passe, plus une valeur. Gratuit : `threshold` est déjà un push constant de
  `DrawBloomDownPass`, donc le réévaluer chaque frame ne recrée aucun pipeline.
  **Après correctif** : `resolved=0.05 → bloomThr=144.8` (restait à 7,24 avant).
  **Banc du point 4 rejoué SOUS AUTO ACTIVE** — première mesure des cas 12 et 13
  dans le régime où ils avaient été éteints : témoin 0,306 % / 2,718 %, test
  0,278 % / 2,425 %. Le témoin bouge plus que le test, luma stable à 141.
  *Réserve* : sur le cas 12, l'écart max du test (88) dépasse celui du témoin
  (79) — au plus quelques pixels de highlight, ce qu'un bloom doit faire.
  ---
  **Le diagnostic, gardé parce qu'il explique la forme du défaut.**
  **Ce n'était pas un retard qui se résorbe** : à la première frame `resolved = 1`
  — le seuil était calculé **avant que la première mesure d'auto-exposition
  n'existe** — puis l'exposition convergeait et le seuil restait sur l'ancienne
  valeur indéfiniment, aucun rebuild n'ayant lieu.
  **Mesure** (Demo4 + `NK_AUTOEXP=1`, orbite, 900 frames) : seuil appliqué
  **7,24** contre **144,8** réclamé, rapport **0,05 stable sur 841 frames**.
  Conséquence : sous auto active, le bloom captait 20× trop bas — **le défaut que
  les six compensations contournaient revient intégralement**. Le banc d'essai
  du 15/08 ne pouvait pas le voir : il tourne en exposition manuelle, où la
  valeur est désormais juste ET stable.
  *Nuance* : `resolved` sature ici à `autoExposureMinExp = 0,05` sur une scène
  réglée pour exposition manuelle 1 — le facteur 20 est un cas franc, pas une
  moyenne. Le mécanisme, lui, ne dépend pas de la saturation.
- 🔄 **DETTE — six compensations d'un défaut désormais corrigé** (nommée le
  2026-08-15, retrait **non arbitré**). Le seuil de bloom s'appliquait sur le
  HDR brut : toute surface diffuse bien éclairée entrait dans le bright pass.
  Six fichiers ont compensé, sur des mois, **sans qu'aucun ne nomme la cause** :
  `Sandbox/src/Demo/main.cpp` cas 12 et 13 (`bloom = false`), `main.cpp:349` et
  `:368`, `DemoSkin.cpp:296` (soleil abaissé), `Demo11_FPSArena.cpp:492`
  (lumières retirées), `Demo3D.cpp:3011` (couleur choisie sous le seuil).
  Le banc d'essai du 15/08 montre que les cas 12 et 13 n'en ont **plus besoin**
  (aucun effet mesurable au-dessus du bruit inter-exécutions). Les quatre autres
  **n'ont pas été mesurés**. Ne pas retirer en bloc : chacun a été posé pour un
  symptôme qui lui est propre, et un seul de ces symptômes pourrait avoir une
  autre cause. Leçon associée : le commentaire du cas 12 décrivait exactement le
  défaut (« le bloom faisait BRILLER les textures saturées alors qu'elles ne
  reflètent pas la lumière ») des mois avant qu'il ne soit diagnostiqué —
  **une compensation documentée reste une cause non cherchée.**
- **FPS chute Vulkan Debug** : 500→100 fps en ~2s sans interaction
  observée 2026-05-16. Probable Vulkan validation layers + UBO writes
  + descriptor updates intensifs en Debug. À vérifier en Release.
- **Self-shadowing artifacts** sur certains objets : bias actuel 0.003
  (NkVSMConfig.shadowBias). Live-tunable via `[` `]` dans Demo3D HUD.
  Si artefact persiste, monter à 0.005-0.01.
- ~~**Debug-draw invisible dans la vue principale quand un miroir est
  actif**~~ **CORRIGÉ 2026-07-12** : la passe miroir (rendue en premier)
  appelait `FlushDebug` qui décrémentait la vie des primitives one-frame
  et les purgeait — la vue principale n'avait plus rien (symptôme : cercle
  vert du matériau actif Demo4/5 visible SEULEMENT dans le reflet). Fix :
  les overlays debug/édition ne sont plus rendus dans la passe miroir
  (aides d'éditeur ≠ contenu de scène — un reflet ne les montre pas).
- **IBL GPU sur DX11** : convolutions compute désactivées par défaut
  (maxDiff 175/255 sur 0.8 % des texels vs CPU, fxc cs_5_0 — GL/VK/DX12
  propres à 5/255). `NK_IBL_GPU=1` pour reproduire/investiguer.
- **« Sous le plan plus clair qu'au-dessus » (demo3, rapport Rihen) —
  MESURÉ 2026-07-12** (capture DX11 + comparaison pixels, outil NK_CAPTURE) :
  le reflet n'est PAS plus lumineux (sphère réfléchie lum 174 vs directe 186) ;
  la vraie différence est un **voile gris désaturant** sur le reflet (canal B
  de la sphère : 2 direct → 58 reflété) = le mix du shader ReflFloor
  `color = mix(litBase, reflColor, reflStr)` injecte ~10-40 % de l'éclairage
  gris du sol par-dessus le reflet, + flou du RT de réflexion. La vue directe
  (sous le plan) est donc plus nette/saturée → perçue « plus claire ».
  En cause aussi : `reflStr = (1-roughness)*mix(0.9, 1.0, fresnel)` = miroir
  ~90 % à TOUT angle (non physique). **RÉSOLU EN OPTION UTILISATEUR
  (2026-07-12, demande Rihen)** : `NkPBRParams::reflBlend` +
  `NkMaterial(Instance)::SetReflFloorBlend(v)` — `-1` = Fresnel PHYSIQUE
  (4 % de face → 100 % rasant, style UE5) ; `[0..1]` = STYLISÉ avec intensité
  du voile litBase (1 = look historique par défaut, 0 = reflet pur). Propagé
  par l'héritage M.4. Validé par captures DX11 mesurées : mode défaut =
  non-régression pixel exacte ; reflet pur = canal B de la sphère réfléchie
  58 → 3 (= sphère directe) ; physique = reflet ~4 % de face. Demo4/demo3 :
  touche **P** cycle les modes + env `NK_REFL_MODEL=<0-3>`.
- **Readback OpenGL de NkOffscreenTarget cassé** (GLAD 1282
  glMapNamedBufferRange) — la capture NK_CAPTURE ne marche que sur DX11
  (vérifié pixel-perfect) ; fix côté NKRHI GL à coordonner (module partagé).
- ~~**DX12 : DEVICE REMOVED 0x887A0001**~~ **RÉSOLU (2026-07-23, 269207c9)** —
  4 causes racines trouvées au debug layer : (1) root constants 64 o débordés
  par GridPC/Glow2D 96 o → root sig passée à 32 DWORDs (128 o) + clamp ;
  (2) release immédiat des PSO détruits en plein enregistrement → destruction
  DIFFÉRÉE générale datée par fence (pipelines/textures/buffers) ;
  (3) cache de variantes PSO `NkUnorderedMap<uint64,ComPtr>` imbriquée PERDAIT
  la valeur stockée (⚠ bug conteneur à isoler côté NKContainERS — map imbriquée
  déplacée + ComPtr) → variante PP_Tone reconstruite/relâchée CHAQUE frame →
  remplacée par `NkVector<NkPsoVariant>` ; (4) readback NK_CAPTURE :
  `CopyTextureToBuffer` passait RowPitch=0 au footprint (INVALID_CALL) → pitch
  serré aligné 256 + staging NkOffscreenTarget dimensionné/lu au pitch aligné.
  Validé : demo 2 DX12 zéro erreur, **capture NK_CAPTURE DX12 fonctionnelle**
  (pixels réels, ombre alpha-testée confirmée), deferred DX12 REND (409 FPS),
  non-régression GL/VK/DX11. PSO nommés (WKPDID) + env diag `NK_DX12_NODRAIN`.

---

## ❌ Restant priorité 2 — Qualité visuelle/perf

### Phase H — Texture pipeline
- ✅ Loader PNG/JPG/TGA/HDR via NkImage (existant)
- ✅ **Loader EXR** (audit 2026-07-12 : `NkEXRCodec` livré dans NKImage —
  la démo materials charge d'ailleurs `piazza_bologni_1k.exr`)
- ✅ **Mipmap generation** (audit 2026-07-12 : `NkIDevice::GenerateMipmaps`
  au RHI, utilisé par la chaîne matériaux — cf. fix mips DX12 2026-06-23)
- ❌ Compression BC1-7 (desktop) + ASTC + ETC2 (mobile)
- ✅ Texture streaming (LOD-mip selon distance) — **LIVRÉ 2026-07-13** via
  `NkStreamingSystem` v1+v2 (worker E/S réelles, low-res d'abord, raffinage par
  distance, démo `--demo=19`) — détail « Reste à faire priorisé » point 5
- ❌ Hot-reload des textures (les matériaux `.nkasset` l'ont, pas les textures)
- ❌ Atlasing pour batching

### Phase V — MONDES VOLUMINEUX : transposer les techniques prouvées en NKAI ❌

> **Décision de Rihen, 6 août 2026.** Les optimisations écrites pour faire tenir
> un modèle de langage de 7 milliards de paramètres dans 8 Go de VRAM
> (`Kernel/AI/NKInfer`, jalons QLoRA 4 et 5) **sont exactement celles qu'exigent
> les mondes 3D volumineux** — film d'animation, jeu, simulation. Ce ne sont pas
> des techniques voisines : ce sont les mêmes, appliquées à d'autres octets. À
> implémenter **dès que possible**, parce que tout ce qui vient après (scènes de
> production, mondes ouverts, plans de film) en dépend.

**Ce qui est DÉJÀ prouvé côté NKAI, et ce que ça donne ici :**

| Prouvé en NKAI (mesuré) | Transposition 3D | État |
|---|---|---|
| Poids **Q4_K/Q6_K résidents**, déquantifiés **dans le shader** — 36 Mo au lieu de 259, soit **7×** | Textures **BC1-7 / ASTC / ETC2** décompressées par l'échantillonneur ; positions, normales et UV **quantifiés** dans les tampons de sommets | ❌ (BC/ASTC déjà listés Phase H) |
| **`token_embd` jamais monté** : une ligne de 2 Ko lue au fichier par token, au lieu de 306 Mo en VRAM | **Streaming de géométrie** : ne réside que ce qui est visible ; le reste vit sur disque et arrive à la demande | 🔶 `NkStreamingSystem` le fait pour les **textures**, pas pour la géométrie |
| **Tuilage + mémoire partagée** dans le matmul : **×5** à M=256 (61 → 282 GFLOPS) | Tuilage du **culling** et du rendu par tuiles (clustered/tiled light culling, Phase M v3) — même raison, même gain | ❌ |
| **KV-cache** : ne jamais recalculer le passé | Caches de géométrie et d'ombres : ne pas recalculer ce qui n'a pas bougé | ✅ le principe existe (VSM, *dirty box* voxel) |

**À écrire (le vrai travail neuf) :**

- ❌ **Géométrie virtualisée** (façon Nanite) : niveaux de détail chargés selon
  la distance, groupes de triangles résidents à la demande. C'est le pendant
  exact du streaming de `token_embd` — on ne monte que ce qu'on regarde.
- ❌ **Atlas de textures virtuel** : un espace d'adressage de textures bien plus
  grand que la VRAM, dont seules les tuiles vues sont résidentes.
- ❌ **Hiérarchie spatiale** (BVH/octree de scène) pour décider quoi charger et
  quoi dessiner — le décideur dont dépendent les deux points précédents.
- ❌ **Quantification des attributs de sommets** au format GPU, sur le modèle des
  blocs Q4_K : lecture brute, décompression dans le shader.

**Nuance à ne pas perdre de vue** : en IA le goulot est la **bande passante
mémoire** (relire des gigaoctets de poids à chaque token) ; en 3D c'est plus
souvent le **nombre d'appels de dessin** et la **latence disque**. Les remèdes
se ressemblent, les priorités diffèrent — mesurer avant d'optimiser, comme le
jalon 4 l'a fait (avant/après à quatre tailles).

**Consommateurs visés** : Noge/Nogee (jeu, film d'animation, simulation) — voir
la section jumelle dans `Engine/Noge/ROADMAP.md`.

### Phase M — Forward+ / Deferred
- ✅ **Deferred v1+v2 LIVRÉ (2026-07-13)** : G-buffer MRT 3 RT + light pass
  fullscreen + ForwardRest, opt-in `cfg.deferred`/`NK_DEFERRED=1` — GL référence
  (91,8 % parité, 207 vs 140 FPS) + VULKAN validé capture ; DX11 fonctionnel
  (✅ rayons parasites du spot cookie RÉSOLUS 2026-07-23, a6c71299 — signe
  NDC Y par backend) ; DX12 ✅ device-removed RÉSOLU 2026-07-23 (269207c9) —
  **deferred v2 VALIDÉ SUR LES 4 BACKENDS GL/VK/DX11/DX12**, détail
  « Reste à faire priorisé » point 6 et « Bugs/quirks connus »
- ❌ v3 : tiled/clustered light culling (>32 lumières) + bench scène 100+ lights
- ❌ Forward+ (alternative tile-based si besoin)

---

## ❌ Priorité 3 — Animation & VFX

### Phase I (animation, ≠ Phase I IBL mirror) — Skeletal animation full
- ✅ Bone hierarchies + skin matrices (skinning GPU 4 backends, bones UBO)
- ✅ Playback : LINEAR/STEP/CUBICSPLINE glTF (additive à faire)
- ✅ Blend trees (1D + 2D Shepard) + state machines (crossfade bone-local +
  événements de transition) — **LIVRÉ v1+v2 2026-07-13**, points 8 du priorisé
- ✅ IK : FABRIK, CCD, two-bone (NkIKSystem, requalifié — c'est l'IK de NkAnima)
- ✅ Morph targets / blend shapes v1 CPU + skinnés (2026-07-13 ; v2 = GPU compute)
- ❌ Retargeting ; ❌ blend additif

### Phase J — VFX particles
- GPU compute particle system
- Mesh particles, ribbon trails, decals
- Beam emitters, force fields, vector fields
- Event triggers (collision, lifetime)

---

## ❌ Priorité 4 — Avancé

- **Phase K** — Volumétrique : fog, god rays, clouds raymarched, volume textures, SSS amélioré
- **Phase O** — Caméras avancées : multi-cam (split-screen, PiP), cinema, VR/stéréoscopique
- **Phase P** — Scene graph + culling : hierarchy complète (interfaces D.5 prêtes), frustum culling, HiZ occlusion, LOD auto, instancing
- **Phase Q** — Editor integration : gizmos translate/rotate/scale, selection outline, stats graph, profiler frame
- **Phase R** — Raytracing hardware : Vulkan KHR_ray_tracing + DXR, RT shadows/reflections/GI, hybride rasterization+RT
- **Phase S** — GPU-driven : indirect rendering, bindless, mesh shaders, GPU culling, virtual textures (megatexture style id Tech)

---

## ❌ Phase T — Texturing & éclairage assistés (fusion corpus IA 2026-07-09)

Couches d'assistance au-dessus de l'existant (système de matériaux 16 familles,
IBL, NkVSM, hot-reload `.nkasset`) — **rien ne remplace, tout étend**. Principe :
génération éditable couche par couche, jamais un bitmap figé ; l'artiste reprend
la main à chaque étape. Inférence locale (NKAI/NKGen) privilégiée, API externe
optionnelle.

### T.1 — Bake automatique (fondation, AUCUNE IA, à faire en premier)
- ❌ Bake AO ray-based (réutilise NKRHI ; lié Phase H mipmaps/streaming)
- ❌ Bake curvature (pilote l'usure/dégradation procédurale)
- ❌ Bake thickness/SSS (peau, tissus translucides)
- ❌ Pipeline de bake batché (tous les assets d'une scène)

### ⚠️ FAIT MESURÉ (2026-08-22) — 33 types de matériau déclarés, **17 gabarits enregistrés**

`NkMaterialType` compte **33** valeurs ; `NkMaterialSystem::RegisterBuiltins()`
n'en enregistre que **17**. Les 16 autres n'ont **aucun gabarit** : `Create()`
retombe sur PBR, et les types sont indiscernables à l'œil. Le code le dit
lui-même pour la famille réaliste, corrigée le 11 août — *« leurs shaders
existaient depuis toujours mais aucun gabarit ne les nommait »*.

```
NK_CUSTOM  NK_DEBUG_AO  NK_DEBUG_DEPTH  NK_DEBUG_NORMALS  NK_DEBUG_UV
NK_FLAT  NK_GLOW_2D  NK_PBR_SPECULAR  NK_PIXEL_ART  NK_SKETCH  NK_SPRITE_2D
NK_TERRAIN  NK_UPBGE_EEVEE  NK_VOLUME  NK_WATER  NK_WATERCOLOR
```

**C'est consigné, pas corrigé** — les corriger est un autre chantier. Deux
conséquences immédiates :

1. Toute garantie de non-régression sur « les archetypes » porte sur **17**, pas
   sur « une trentaine ». Le dire évite de croire plus tard qu'on a cassé
   quelque chose qui n'existait pas.
2. ⚠️ **`NK_UPBGE_EEVEE` est dans la liste.** Il a été cité comme preuve que le
   moteur était « déjà du bon côté de la barrière » EEVEE/Cycles pour le mélange
   de BSDF. Le raisonnement reste juste — un rasteriseur mélange les paramètres
   ou les résultats, jamais les closures — mais **l'argument par cet archétype ne
   vaut rien** : il ne rend pas ce qu'il annonce.

C'est la **quatrième occurrence de la même forme** repérée ce jour-là (un `bool
ssr` activé par deux presets et jamais implémenté ; un en-tête annonçant une
capacité absente qui existait ; `outlineWidth`/`outlineColor` transmis et ignorés
par le shader toon). **Ce n'est plus une série de coïncidences, c'est un motif du
dépôt** : la déclaration et l'implémentation vivent dans deux fichiers que rien
ne force à s'accorder.

### T.2 — Graphe de matériaux — ⏳ **DÉMARRÉ ET PROUVÉ EN CONSOLE (2026-08-22)**

Preuve : `Applications/NkMatGraphCheck` — **application** console, **35 cas, 0
échec**, sans GPU ni fenêtre (dépendances de `NkSLCheck` : NKSL + Foundation).
**21 mutations vérifiées rouges** au total sur les trois couches livrées.

| brique | état | où |
|---|---|---|
| substrat de graphe | ✅ réutilisé, pas réécrit | `Kernel/Runtime/NKGraph` (couche 1) |
| types de prises matériau + conversions dirigées | ✅ | `Materials/Graph/NkMatGraphTypes.h` |
| prototypes de nœuds | ✅ 5 | Principled · Diffuse · Emission · **Mix Shader** · Material Output |
| validation de domaine | ✅ | zéro sortie · sorties multiples · sortie non reliée · cycle |
| **compilateur → NkSL** | ✅ v1 | `Materials/Graph/NkMatGraphCompile.h` |
| **preuve de RENDU sur GPU** | ✅ | `Applications/NkMatGraphDemo` — DX11 headless, 7 cas, 4 mutations rouges |
| masque de couche par **texture** | ✅ | 4 canaux, binding 9, `SetLayerV1MaskMap()` |
| nœuds `Value` · `RGB` · `Math` (7 op.) · `Mix Color` (6 modes) | ✅ 2026-08-22 | une propriété de nœud porte une **décision**, pas seulement une valeur |
| nœud `ColorRamp` | ✅ 2026-08-22 | la première propriété à **charge utile variable** |
| nœuds `Image Texture` · `Texture Coordinate` · `Mapping` | ✅ 2026-08-22 | **aucun binding neuf** — voir ci-dessous, la mesure a changé la réponse |
| **variables exposées — ENTRÉES** | ✅ 2026-08-22 | l'API publique du moteur ; ⚠️ preuve de **rendu** non atteinte, voir ci-dessous |
| variables exposées — **sorties** | ❌ | en attente d'une décision de Rodolf : par pixel ou par matériau |
| **rang 2** — `Noise` · `Gradient` · `Checker` | ✅ 2026-08-22 | procédural : **zéro slot de texture** |
| **rang 2** — `Voronoi` · `Wave` · `Brick` | ✅ 2026-08-22 | **rang 2 complet** — 22 prototypes |
| `Normal Map` · `Bump` · `Separate XYZ` | ✅ 2026-08-22 | **rang 1 complet** — convention tranchée, voir ci-dessous |
| canevas d'édition | ❌ | couche 2, `NKEditorKit`, partagé — pas ce chantier |

**Le NkSL émis COMPILE sur les quatre backends** (GL, Vulkan, DX11, DX12), vérifié
par le vrai `NkSLCompiler` — pas comparé à un témoin textuel. Un graphe
sérialisé, relu, puis recompilé rend le **même shader au caractère près**.

#### Les trois décisions qui structurent le générateur

**1. Un « shader » est quatre locales, pas une struct.** NkSL n'accepte pas de
variable locale de type struct utilisateur (contrainte relevée dans
`layeredv1.frag.nksl`). Un shader est donc émis en `albedo` (vec3), `metallic`
(float), `roughness` (float), `emission` (vec3) — exactement le motif de
l'accumulateur de LayeredV1. Ce n'est pas un choix de style.

**2. `Mix Shader` mélange les PARAMÈTRES, pas des closures.** Cycles mélange des
closures (une lobe tirée au hasard par rayon) ; **EEVEE approxime**, et NKRenderer
est un rasteriseur. Le mélange est donc composante par composante, facteur borné
à [0,1]. C'est ce que font EEVEE, Unreal et Unity. La **forme** du graphe est
celle de Blender ; seule son exécution diffère.

**3. Une entrée ni câblée ni renseignée donne le NOIR.** Le blanc ferait passer un
matériau non fini pour un matériau clair. Le neutre doit **se voir**.

#### `ColorRamp` — la première charge utile **variable** (2026-08-22)

Jusque-là toute propriété portait une charge de taille **fixe** : un réel, trois,
quatre. Une rampe en porte **4×N**, et N vient du fichier. C'est elle qui éprouve
vraiment le sac de propriétés — et sa réponse conditionne `Image Texture`, dont
le chemin d'image est aussi une charge variable.

**Le découpage vit à UN SEUL endroit** (`NkMatLisRampe`), partagé par le
compilateur et le banc : un découpage dupliqué finirait par diverger, et c'est le
compilateur qui aurait raison sans que personne le sache. Quatre réels par arrêt,
plafond **32** arrêts, positions **strictement croissantes**.

**Cinq réponses, toutes distinctes, parce qu'elles ne se réparent pas pareil :**

| cas | réponse |
|---|---|
| propriété **absente** | ✅ compile — rampe noir→blanc par défaut. **Voisin légitime** : le nœud vient d'être posé, l'auteur n'a pas choisi |
| compte de réels non multiple de 4 | refus `mal-formee` |
| positions **dans le désordre** | refus `positions-dans-le-desordre` → il faut réordonner |
| deux arrêts **à la même position** | refus `deux-arrets-a-la-meme-position` → il faut en déplacer un |
| au-delà du plafond | refus `trop-d-arrets (33 demandes, plafond 32)` |

⚠️ **Les deux défauts de position ont d'abord partagé un message**, et c'est un cas
de banc qui a exigé de les séparer : le désordre se corrige en réordonnant, une
égalité en **déplaçant** un arrêt. Un message commun oblige l'auteur à
comprendre lui-même lequel des deux il a sous les yeux.

⚠️ **Le refus du plafond DIT LE COMPTE.** « Trop d'arrêts » seul oblige à deviner
combien retirer.

**Aucune division dans le shader** : le dénominateur de chaque segment est
calculé **à la compilation**, et la validation a déjà garanti qu'il est non nul en
refusant les positions égales. Une division laissée dans le shader serait une
garde à maintenir pour rien.

**Preuve de rendu — et c'est elle qui compte** : une rampe à **trois** arrêts,
rouge → **vert au milieu** → rouge, lue à `fac = 0,5`, doit rendre du **vert
pur**. Un émetteur qui ne lirait que le premier et le dernier arrêt — l'erreur
naturelle quand on traite une **liste** comme une **paire** — rendrait du rouge
interpolé avec du rouge. **Les deux résultats sont des couleurs plausibles ;
seul le canal où tombe l'écart les distingue.** Mesuré : `(52, 180, 52)`, écart
sur le vert = 128, exactement l'arrêt du milieu.

#### ⚠️ Trois faits mesurés qui affaiblissent une garantie qu'on employait (2026-08-22)

Ils sont regroupés ici parce qu'ils portent tous sur **la valeur de nos preuves**,
pas sur une fonctionnalité.

**1. « Ça compile sur les 4 backends » ne garantit PAS que les fonctions appelées
existent.** Mesuré : une mutation retirant `Voronoi` de la liste des nœuds qui
réclament les briques de bruit produit un shader appelant `NkHash22` **sans que
`NkHash22` soit déclarée** — et `GL=ok VK=ok DX11=ok DX12=ok`. Le frontend NkSL
ne rejette pas l'appel d'une fonction inconnue. Toute assertion du dépôt qui se
repose sur « les quatre backends acceptent » doit donc être lue comme *plus
faible qu'elle n'en a l'air*, et complétée par une vérification de **présence des
dépendances** quand le shader en a.

**2. Un tampon dimensionné « généreusement » est une bombe à retardement quand ce
qu'il mesure grandit par conception.** Le cas du menu de la bibliothèque écrivait
dans `const NkMatNodeProto *menu[16]`, et la requête rend le **compte réel** : à
17 prototypes proposables, l'itération sortait du tableau — segfault. Le cas
était pourtant le bon, il grandissait avec le système : c'est précisément ce qui
l'a fait déborder. Dimensionner sur ce qui grandit, pas sur un nombre choisi.

**3. `NkFormat` tronque en silence sur un indice positionnel hors ordre.** Une
chaîne employant `{6}` avant `{5}` s'est trouvée coupée juste après `{6}`, sans
erreur ni avertissement. La ligne de résultat était incomplète dans plusieurs
rapports avant qu'on ne le remarque. **Employer les indices dans l'ordre.**

#### 🎨 Rang 2 — le procédural, et une contrainte du dialecte qu'il a révélée

`Noise Texture`, `Gradient Texture`, `Checker Texture`. Leur intérêt n'est pas
seulement d'ajouter des motifs : **ils ne consomment aucun slot de texture**. Un
matériau entièrement procédural ne coûte donc rien au plafond de 6 — et un cas le
vérifie en comptant les bindings émis (0).

##### ⚠️ Le fait mesuré qui a décidé de la conception

**L'`#include` du dialecte NkSL ne se résout PAS quand le shader est compilé
depuis une chaîne.** Les quatre backends rendent `#include not found:
Include/NkNoise.glsli`. Un shader engendré n'existe pas sur disque : il doit donc
être **autonome**.

Conséquence : les fonctions de `NkNoise.glsli` (`NkHash2`, `NkHash22`,
`NkValueNoise2D`, `NkFBM2D`) sont **recopiées** dans le compilateur — ce qui est
exactement le motif de duplication que ce dépôt a payé plusieurs fois.

⚠️ **Donc la duplication est GARDÉE.** Un cas de banc **lit `NkNoise.glsli` sur le
disque** et exige que chaque fonction émise s'y retrouve **mot pour mot** —
signatures *et* morceaux de corps, parce qu'une signature seule passerait alors
que le corps aurait divergé. Le jour où quelqu'un corrige une formule dans le
fichier, le banc reste rouge tant que le compilateur n'a pas suivi. C'est la même
parade que pour les bindings : **comparer le code à une vérité externe, faute de
pouvoir partager**.

Et la limite elle-même est devenue un cas permanent
(`nksl/include-ne-se-resout-pas-depuis-une-chaine`) : **une limite non testée se
perd**. Si le résolveur apprend un jour à travailler depuis une chaîne, ce cas
passera au rouge — et ce sera le bon moment pour supprimer la recopie.

##### Deux décisions de mise en œuvre

- **Les octaves sont bornées DANS le shader**, pas au moment de la compilation.
  Le compte vient d'une valeur du graphe, donc potentiellement d'un paramètre
  exposé : une boucle dont le compte est libre peut ne pas se dérouler, et
  certains backends refusent alors le shader. Le cas vérifie que la borne est
  dans le **code émis** — sinon un `detail` branché sur un autre nœud y
  échapperait.
- **Les briques de bruit ne sont émises que si un nœud les réclame**, vérifié
  dans les deux sens.

##### Les trois derniers : `Voronoi`, `Wave`, `Brick`

- **Voronoi** déroule un voisinage `3x3` **littéral**. Une borne connue à la
  compilation est ce qui permet à tous les backends de dérouler la boucle ; un
  rayon variable ferait un shader qui compile ici et pas ailleurs — le défaut le
  plus pénible, parce qu'il fait accuser la machine. Il réutilise `NkHash22`,
  déjà recopiée et déjà gardée.
- **Wave** vaut `0,5 + 0,5·sin(x·échelle·2π)` : période `1/échelle`, borné dans
  [0,1] **sans clamp**. Un sinus brut sortirait de [0,1] et un `clamp`
  écraserait les creux au lieu de les rendre.
- **Brick** décale les joints d'une demi-brique **une rangée sur deux**. Sans ce
  décalage on obtient un **quadrillage** — un mur parfaitement plausible, et
  faux. Le cas vérifie que le décalage est calculé **et employé** : un décalage
  calculé mais jamais lu donnerait exactement ce quadrillage, et la seule
  vérification de présence passerait.

##### La preuve de rendu : un calcul, pas une relation

Les preuves précédentes mesuraient des **relations entre canaux** parce que
l'éclairage était inconnu. Le damier permet mieux : **son arithmétique est
prédictible**. Au pixel central, `uv = (0,5 ; 0,5)` ; le damier vaut
`mod(floor(x)+floor(y)+floor(z), 2)` sur la coordonnée mise à l'échelle :

| graphe | coordonnée | somme | case | mesuré |
|---|---|---|---|---|
| sans décalage, échelle 5 | (2,5 ; 2,5 ; 0) | 2+2+0 = **4** | paire → `color2` | **(52,180,52)** vert |
| décalage +0,2, échelle 5 | (3,5 ; 2,5 ; 0) | 3+2+0 = **5** | impaire → `color1` | **(180,52,52)** rouge |

⚠️ **Ce cas désigne LAQUELLE des deux couleurs doit sortir.** Une erreur d'un
demi-carreau donnerait l'autre — et l'image resterait un damier parfaitement
plausible. La mutation qui décale le damier d'une case n'est attrapée **que** par
ce rendu : les 74 cas de compilation la laissent passer.

#### 🎛️ Les paramètres exposés — l'API publique du moteur (2026-08-22)

C'est le point où ce chantier cesse d'être un compilateur de shaders. Une API
publique se change mal après coup, d'où le soin sur la forme.

**Une propriété `expose.<prise>` porte le NOM PUBLIC du paramètre.** Présente =
exposé, absente = constante. Le nom **est** la donnée — c'est lui que le code du
jeu emploiera (`SetFloat("usure", 0.7f)`) ; un booléen obligerait à inventer le
nom ailleurs, donc à le maintenir à deux endroits.

⚠️ **« Exposé » n'est pas le défaut, et ce n'est pas un choix de prototype.** Une
constante se **replie** dans le code émis ; une variable exposée vit dans un bloc
uniforme et **aucune optimisation n'est plus possible sur elle**. Tout exposer
donnerait un matériau pilotable et lent. Et `roughness` est exposable dans un
matériau et pas dans un autre : c'est donc un choix **d'auteur, sur son nœud**,
porté par l'instance.

##### Ce que la disposition garantit

⚠️ **Elle porte des DÉCALAGES, jamais un ordre.** En `std140` un `vec3` s'aligne
sur 16 octets : réel, vec3, réel donnent **0, 16, 28** pour un bloc de **32** —
pas 0, 4, 16. Un moteur qui déduirait les positions de l'ordre de déclaration
écrirait à côté dès le premier `vec3`, **sans erreur, avec une valeur crédible**.

⚠️ **Un jeton de compilation accompagne la disposition.** Recompiler un graphe
**édité** peut réordonner le bloc ; du code de jeu ayant retenu un décalage
écrirait alors dans le mauvais paramètre. **L'API est par NOM**, et tout cache de
décalage doit porter ce jeton et se jeter quand il change.

**Le défaut d'un paramètre exposé EST le `defaultValue` de sa prise**, jamais une
seconde valeur à côté : deux sources pour une même chose divergent, et c'est
alors l'éditeur qui montre l'une pendant que le moteur envoie l'autre. Le cas le
vérifie en **changeant** le défaut après avoir posé l'exposition.

**Aucun binding neuf** : le bloc réutilise le slot 8 — l'UBO matériau, mort pour
un matériau engendré qui n'a pas de struct figée. Même raisonnement que les
textures.

##### 🔴 Une prise CONNECTÉE **et** exposée est refusée, en la nommant

C'est le cas le plus insidieux, et il manquait à la proposition initiale. Si une
prise reçoit un lien **et** porte une exposition, **le lien remplace la valeur
exposée** : `SetFloat("usure", 0.9f)` ne fait **rien**. Le matériau compile, il
rend, le paramètre est mort — aucune erreur, aucun journal.

⚠️ **Chez Blender le problème ne se pose pas parce que brancher un lien fait
disparaître le widget : l'interface rend l'état impossible.** Nous n'avons pas
d'interface — **c'est donc la validation qui doit le rendre impossible.**

##### ⚠️ Le coût, nommé plutôt que déduit

« Le même matériau, deux objets, deux valeurs » implique un bloc **par
instance**, pas par matériau : un tampon uniforme par objet (ou des push
constants pour les petits blocs), donc **une écriture et une liaison de plus par
objet dessiné**. C'est écrit ici pour que quelqu'un puisse le contester en le
voyant, plutôt que le découvrir dans un profil.

##### ⚠️ CE QUI N'EST PAS PROUVÉ : le rendu

Les entrées sont prouvées **à la compilation** — 67 cas, 5 mutations rouges,
NkSL valide sur les 4 backends. **La preuve de RENDU n'est pas atteinte.** Un
banc qui compile une fois et rend trois fois en ne changeant que le contenu du
bloc affiche le plancher achromatique : les valeurs n'arrivent pas au shader.

Ce qui a été **écarté par la mesure**, pour que personne ne recommence :

- ce n'est pas l'index de set (essai en `set=0, binding=1` : même résultat) ;
- ce n'est pas un renumérotage séquentiel des cbuffers (liaison simultanée au
  slot 1 : sans effet) ;
- ce n'est pas une écriture partielle (le tampon entier est écrit) ;
- les handles sont tous valides (layout, set, tampon), et le HLSL généré place
  bien le bloc sur `register(b8)` ;
- DX11 **ignore l'index de set** et lie par le numéro de binding, lequel est
  sous la limite de 14 cbuffers.

La cause reste à trouver. **Tant qu'elle ne l'est pas, l'exposition est une
capacité prouvée à la compilation seulement** — et le dire vaut mieux que de
laisser croire qu'un `SetFloat` piloterait quoi que ce soit aujourd'hui.

#### 🗿 Le relief — convention tranchée, et la normale qui **voyage** (2026-08-22)

**Convention interne : OpenGL, `+Y` vers le haut** (« vert vers le haut »). Une
carte DirectX (`-Y`) se convertit **à l'import, jamais dans le shader**. Trois
raisons : Blender emploie cette convention et Rodolf construit sur les siennes —
diverger produirait des reliefs **inversés** en important son propre travail,
avec un symptôme notoirement difficile à diagnostiquer (l'image reste plausible,
elle est juste creuse là où elle devrait être bombée) ; Vulkan et OpenGL sont les
deux backends validés ; et convertir à l'import évite de payer **par pixel** tout
en rendant l'état de la texture **visible dans la donnée**.

⚠️ **Le drapeau de provenance existe dès maintenant, bien que rien ne le consomme
encore.** L'ajouter après coup obligerait à **deviner** la convention des textures
déjà importées — et les deux hypothèses donnent une image plausible, donc le
doute serait indécidable.

**Le cas qui décide est une ABSENCE d'effet** : les shaders émis pour `opengl`,
`directx` et *sans convention* sont **identiques au caractère près** (2806 o
chacun). La tentation naturelle — retourner Y par pixel — marche, coûte à chaque
fragment, et rend l'état de la donnée invisible ; seule l'égalité des textes la
dénonce. Une mutation qui traite la convention dans le shader met ce cas au rouge.

##### La normale voyage — un `shader` porte désormais **cinq** composantes

⚠️ **Avant le 22/08 le puits recalculait `normalize(vNormal)`** : tout travail de
relief en amont était jeté **en silence**, le shader compilait, l'image restait
plausible, et le nœud n'aurait servi à rien. *Un nœud dont la sortie n'est lue
par personne est pire qu'un nœud absent : il donne l'illusion que la capacité
existe.* La normale a donc rejoint albedo/metallic/roughness/emission, et le
puits éclaire avec celle du graphe. Le cas vérifie **les deux maillons de la
chaîne** (Normal Map → Principled → puits) **et une absence** — le puits ne doit
plus recalculer la géométrique.

Une prise `normal` non câblée vaut la **normale géométrique**, pas le noir : un
vecteur nul serait une direction indéfinie. Ce n'est pas un repli plausible,
c'est le comportement **défini** de la prise.

##### Base tangente : par dérivées d'écran, et seulement si utile

Le vertex engendré ne fournit **aucune tangente** ; s'en passer n'est pas un
choix mais la seule voie honnête. On emploie le **cadre cotangent de Schuler**,
déjà utilisé par `pbr.frag.nksl` et pour la raison écrite là-bas : les tangentes
de sommet peuvent être nulles ou désalignées des UV, et le relief part alors dans
une direction **arbitraire par face**. Elle n'est émise que si un `Normal Map`
la réclame — un shader qui la calculerait sans s'en servir paierait deux paires
de dérivées à chaque pixel pour rien, et le cas le vérifie **dans les deux sens**.

##### La preuve de rendu — l'éclairage est **étalonné**, jamais réimplanté

C'est la réponse au piège habituel : pour savoir de quel côté une bosse doit
s'éclairer, on ne recalcule pas l'ombrage — **on le mesure**. Le même shader, le
même éclairage, mais une normale **imposée** via un nœud `RGB` donne les couleurs
de référence.

| rendu | pixel | ce que ça établit |
|---|---|---|
| étalon, normale vers **−x** | 129 | le côté clair |
| étalon, normale vers **+x** | 82 | le côté sombre |
| carte **plate** (0,5 ; 0,5 ; 1) | 121 | **exactement** la normale géométrique, 3 canaux au bit |
| `Bump` force **+1** | **129** | tombe sur l'étalon −x |
| `Bump` force **−1** | **82** | tombe sur l'étalon +x |
| carte **penchée** (0,75 ; 0,5 ; 1) | **82** | tombe sur l'étalon +x |

⚠️ **Fait établi par ces correspondances exactes, et écrit pendant qu'on le
sait** : la base tangente construite par dérivées d'écran est **unitaire et
alignée sur `+u`**. Rien ne l'imposait a priori — la normalisation commune de
`T` et `B` préserve leur rapport, elle ne garantit pas leur norme. C'est la
correspondance au bit entre la carte penchée et l'étalon `+x` qui l'établit.

⚠️ **L'appariement est ORIENTÉ, et il doit l'être.** Une première version
acceptait « chacun tombe sur l'un des deux étalons », dans n'importe quel ordre —
une **inversion globale** du signe l'aurait donc passée au vert, puisque les deux
résultats se contentent d'échanger. Le sens se **déduit du graphe** : la hauteur
vaut `u`, qui croît avec `+x`, son gradient pointe vers `+x`, et la formule
incline la normale à l'**opposé** du gradient. Une force positive doit donc
rendre l'étalon `−x`. La mutation qui inverse le signe tombe.

⚠️ **Et la carte plate ne suffit pas** — c'est une mutation qui l'a montré. Avec
un texel neutre la normale tangente vaut `(0, 0, 1)`, donc **T et B sont
multipliés par zéro** : une base tangente cassée est **invisible**. D'où le cas de
la carte **penchée**, qui la fait intervenir. Sous la mutation, elle rend 145
(côté clair, donc le mauvais) au lieu de 82.

##### Deux notes de mise en œuvre

- `Separate XYZ` a été ajouté **avec** le relief, pas après : sans lui aucun
  scalaire **variable** n'existe dans un graphe, et `Bump` n'aurait pu être
  mesuré que sur son absence d'effet.
- Le déterminant du repère est **gardé** : une division nue produirait un NaN sur
  un triangle dégénéré ou vu par la tranche — même raison que la division du nœud
  `Math`, et même conséquence (un défaut qui change d'aspect d'un backend à
  l'autre fait accuser la machine).

#### 🖼️ `Image Texture` — livré, et **sans toucher au layout partagé**

**Plafond fixe de textures, refus nommé, source unique.** Les deux autres voies
tombent, et pour des raisons écrites :

- un **tableau de descripteurs** avec indexation dynamique exige
  `descriptorIndexing` : correct en Vulkan et DX12, fragile en GL et DX11. Ça
  casserait la **parité 5 backends**, qui est un acquis dur du dépôt.
- un **layout par matériau** est la vraie réponse à long terme, mais le layout est
  partagé aujourd'hui : ce refactoring toucherait tout le monde en même temps.

⚠️ **La condition non négociable** : le plafond est déclaré **une fois**, et le
layout comme le compilateur le lisent **au même endroit** —
`Materials/NkMaterialBindings.h`. S'il vit à deux endroits, on recrée **par
construction** la panne silencieuse déjà mesurée : un compilateur qui autorise 8
et un layout qui en déclare 6 écrira sur deux bindings inexistants, **sans un
mot**.

**Le plafond sera un chiffre avec une provenance écrite**, pris sur ce que les
backends *garantissent* et non sur ce qui semble raisonnable : GL 3.3 garantit 16
unités de texture en fragment, DX11 en garantit 128 — **c'est le plancher qui
commande**, moins ce que le moteur consomme déjà (bindings 3 à 7 et 9).

⚠️ **LA MESURE A CHANGÉ LA RÉPONSE, et en mieux : il n'y a AUCUN binding neuf.**

Deux faits mesurés le 2026-08-22, avant d'écrire une ligne :

1. **Le shader PBR déclare déjà 27 samplers en fragment** — 5 dans le set
   matériau, 22 dans le set global (IBL, atlas d'ombres, 12 lumières, voxels,
   LTC, matcap). La spécification OpenGL n'en garantit que **16** par étage : le
   moteur dépasse donc déjà le minimum garanti et s'appuie sur les 32 que
   donnent les pilotes réels. ⚠️ **C'est une dette ANTÉRIEURE à ce chantier** —
   elle est nommée ici, elle n'est pas aggravée. Et elle invalide le calcul
   qu'on avait prévu : un budget « 16 moins ce qui est pris » serait déjà
   **négatif**.
2. **Le dépôt réutilise déjà le même binding pour des usages différents selon le
   shader** : le binding 3 porte `tAlbedo` en PBR et `tReflection` en sol
   miroir ; le binding 4 porte `tNormal`, `tMatcap`, `tReflectionBack` ou
   `tShadowRamp` selon l'archétype. **Le sens d'un binding est donc LOCAL AU
   SHADER**, et c'est une propriété établie du dépôt, pas une invention.

**Conséquence** : un matériau **engendré** n'a que faire de `tAlbedo` ou de
`tHeight` — ces slots sont morts pour lui. Il réutilise donc **les six
emplacements que le layout déclare déjà**, dans l'ordre topologique. Le plafond
n'est pas un chiffre choisi : **c'est le nombre de slots existants**, et il vaut
**6**.

Ce que ça évite est exactement le risque qui inquiétait : **aucun binding neuf,
donc aucune nouvelle occasion d'écrire sur un binding que le layout ne déclare
pas** — la panne silencieuse mesurée le 22/08, qui ne produit ni erreur, ni
journal, ni différence d'image.

**Le contrôle qui protège la condition, et c'est le plus important des trois** :
le banc lit `NkMaterialBindings.h` **et** le shader émis, et vérifie que chaque
binding déclaré appartient à la table, qu'ils sont tous distincts, et qu'aucun ne
dépasse le plus grand de la table. ⚠️ **En comptant des NOMBRES** — chercher le
nom du sampler testerait le générateur de noms, pas le shader (`tMask` devient
`tmask_tex` en HLSL).

**Le refus dit le compte** : *« ce graphe demande 7 textures, le plafond est
6 »*. Et le bord exact est testé : 6 compile, 7 refuse — un `>=` au lieu d'un `>`
refuserait le cas parfaitement légitime.

⚠️ **`Image Texture` est le premier nœud à DEUX sorties** (`color` et `alpha`), et
c'est lui qui a imposé de nommer les locales engendrées **d'après le nom de la
prise** et non par un `val` unique : un nœud à deux sorties n'a pas « une »
valeur, et l'entrée qui lirait « la » valeur en prendrait une au hasard.

⚠️ **Un cas de banc a survécu à sa mutation, et la faute était dans le cas** :
il cherchait le **nom** de la locale du `Mapping` n'importe où dans le shader.
Sous la mutation « l'entrée `vector` est ignorée », le nœud `Mapping` émettait
toujours sa *déclaration* — donc le nom était présent, et le cas passait au vert
alors que la texture lisait l'UV brut. **Chercher un nom n'est pas chercher un
usage.** Le cas vérifie désormais que l'appel de texture **cite** la locale, et
que le shader mappé **ne lit plus** l'UV brut.


#### Les nœuds de calcul — une propriété qui porte une **décision** (2026-08-22)

`Math` (7 opérations) et `Mix Color` (6 modes) sont les premiers nœuds dont **le
calcul lui-même** est choisi par une propriété. Jusque-là une propriété portait
une *valeur* ; celles-ci portent une *décision*, et c'est le premier endroit du
compilateur où une propriété pilote le **code émis**.

⚠️ **Une opération inconnue FAIT ÉCHOUER la compilation, en la nommant.** La
tentation est de retomber sur la première de la liste parce que « ça marche » :
le matériau compilerait, rendrait, et **calculerait autre chose que ce que le
fichier dit**. Un fichier écrit par une version future, ou une faute de frappe,
passerait inaperçu jusqu'au résultat. Le cas distingue explicitement le voisin
**légitime** : une propriété *absente* (le nœud vient d'être posé, l'auteur n'a
pas choisi) doit, elle, compiler.

⚠️ **Les opérations sont des MOTS, jamais des numéros**, et la table est à **un
seul endroit** — lue par le compilateur, énumérée par le banc, bientôt proposée
par l'interface. Un numéro d'énumération se décale dès qu'on insère une valeur au
milieu, et les graphes déjà enregistrés se mettent alors à calculer autre chose
en silence. Trois listes finiraient par diverger, et c'est le compilateur qui
aurait raison sans que personne le sache.

**Deux gardes numériques, chacune avec son cas** : la division par zéro rend 0
(comme Blender) — une division nue produirait un NaN qui contamine tout l'aval et
**change d'aspect d'un backend à l'autre**, donc le pire à diagnostiquer ; et
`pow` borne sa base à 0, pour la même raison.

**Preuves** : les **13** opérations compilent sur les 4 backends (le compte vient
de la table, pas d'un nombre écrit dans le banc — ajouter une opération sans
l'émettre met le cas au rouge tout seul). Et côté **rendu** : `Mix Color` en mode
`melanger` à `fac=0,5` donne un écart de **64**, en mode `eclaircir` à `fac=1` un
écart de **128** — exactement le **double**. Un compilateur qui ignorerait le mode
rendrait 64 dans les deux cas ; c'est la différence entre les deux qui le
dénonce.

⚠️ **Un déréférencement nul trouvé par une mutation, et corrigé** : retirer le
refus laissait l'indice d'opération à −1, et le pointeur de table était
déréférencé sans contrôle — le banc **mourait** au lieu d'échouer. Un code qui ne
peut se tromper que par un plantage n'est pas robuste, il est chanceux. Le
pointeur est vérifié désormais.

#### 🎯 LA CIBLE POSÉE PAR RODOLF (2026-08-22) — **chaque paramètre EST une prise typée**

> « le principe sur blender qui fait qu'**un champ de couleur peut recevoir une
> texture fichier ou procédural** ? je veux ça au lieu d'avoir un champ couleur à
> part et un champ texture de différents types à part. »

**Le principe est juste ; la mécanique qu'il propose ne l'est pas, et il faut
dire les deux.** Sa formule — « une couleur c'est une texture 1×1 de contenu
uni » — n'est pas ce que fait Blender, et coûterait cher ici : un
échantillonnage coûte un slot de descripteur (on en a 16 à 32), une liaison de
sampler et une lecture mémoire **par pixel**, quand une constante se replie dans
un bloc uniforme — gratuit. Et depuis le 22/08 on sait qu'**écrire sur un binding
absent du layout ne provoque rien** : multiplier les bindings multiplie une
surface de panne silencieuse déjà mesurée.

**Le principe réel, plus simple et plus fort :**

> Un paramètre n'est pas une **valeur**, c'est une **expression d'un type donné**.
> Une constante en est une forme, un échantillonnage d'image une autre, un
> procédural une troisième. **Ce qui les rend interchangeables est le TYPE, pas
> un format de stockage commun.**

C'est la phrase de `NkNodeGraph.h` appliquée aux paramètres : *« on unifie
l'AUTORAT, jamais l'EXÉCUTION »*. Une seule prise à l'autorat, deux chemins de
compilation.

**Ce que ça condamne**, et c'est structurant : aujourd'hui un matériau porte
`NkPBRParams` (une struct **fixe** de réels) **et** un tableau **fixe** de canaux
de texture. **Deux représentations parallèles de la même question** — « d'où
vient la valeur de ce paramètre ». C'est cette dualité qui produit les défauts
déjà vus : un canal utile à un archétype et pas à l'autre, une liste de textures
à maintenir à côté d'une liste de réels. La cible est leur convergence vers des
prises typées, et `NkSocket::defaultValue` est déjà le mécanisme, un cran plus
bas.

⚠️ **Et ça résout « remplir avec ou sans nœud »** — Rodolf n'a ouvert l'éditeur de
nœuds dans **aucune** de ses quatre captures : il est dans le panneau de
propriétés, il clique un point, il choisit une source. Le graphe existe derrière,
il ne le voit jamais. **Ce ne sont pas deux systèmes à réconcilier : c'est la
même donnée, avec deux façons de la toucher.**

**Livré le 2026-08-22 — la brique qui rend le menu possible :**
`NkMatNoeudsPourPrise` / `NkMatNoeudsPourPriseDe` répondent à « que puis-je
brancher ici ? » **en interrogeant le registre**, jamais une liste écrite par
famille — une liste se périmerait au premier nœud ajouté et proposerait ce que
`Connect` refuse. Le résultat est **asymétrique**, exactement comme chez
Blender, parce que les conversions sont dirigées :

| prise | menu calculé |
|---|---|
| `Principled.base_color` (couleur) | `RGB` **et** `Value` (un réel se diffuse en gris) |
| `Principled.roughness` (réel) | `Value` **seul** — `RGB` en est absent |
| `Material Output.surface` (shader) | les 4 BSDF ; ni le puits, ni `Value`, ni `RGB` |

Un nœud **sans aucune sortie** — le `Material Output` — n'est jamais proposable,
et ça tombe sans cas particulier : rien ne peut sortir d'un puits.

**Deux réserves inscrites :**
- ⚠️ **Toutes les prises ne peuvent pas tout accepter.** Un paramètre qui alimente
  l'**état du pipeline** (mode de mélange, mode d'ombre) ne peut pas varier par
  pixel à moindre coût. `NkMatSocketDecl::constanteSeulement` existe pour ça, et
  l'interface doit alors **ne pas afficher de point** plutôt qu'ouvrir un menu
  vide — un menu vide laisse croire à une panne. **Aucun des 7 prototypes actuels
  n'est dans ce cas** : le drapeau est testé sur son mécanisme, pas sur un usage,
  et c'est écrit dans le banc.
- **Une expression sur une prise a un coût, et il doit être visible.** Brancher un
  bruit procédural sur `roughness` est gratuit à écrire et cher à rendre. Ce
  n'est pas une raison de l'interdire, c'est une raison de savoir le mesurer.

⚠️ **Coordination** : `NkVpMatTypeDefaults.h` (NK3DModeler) est **la même donnée
vue d'un troisième bout**. Sa note dit qu'elle doit disparaître le jour où le
graphe porte les défauts. **Ce jour n'est pas encore arrivé** — les prototypes
déclarent la forme des prises, pas encore leurs valeurs — mais il se rapproche,
et le déclencheur sera annoncé avant, pas constaté après.

#### La preuve de rendu — et pourquoi elle ne regarde **pas** l'image

`NkMatGraphDemo` (2026-08-22) : device **DX11 headless** (pas de HWND, donc pas
de swapchain), `Tools/Offscreen` **réutilisé tel quel** — aucune ligne de
`NkOffscreenTarget` modifiée —, lecture par `ReadbackPixels`, jamais par
`Capture(path)`.

⚠️ **Il ne compare aucune image, et c'est délibéré.** Le même jour, en vérifiant
qu'un témoin par signature d'image saurait attraper une erreur de binding, on a
écrit volontairement un descripteur sur un binding absent du layout : **aucune
erreur, aucun journal, cinq signatures identiques**. *Une ressource qui n'arrive
pas ne change pas l'image tant que personne ne la lit.* Un témoin par capture est
donc structurellement aveugle à toute une classe de défauts.

**Ce qu'on mesure à la place : des écarts entre canaux d'un même pixel**, dont la
valeur attendue se calcule **depuis le graphe**, sans rien savoir du modèle
d'éclairage. Avec un nœud `Emission`, le graphe impose `albedo = 0` et
`metallic = 0`, donc `diffuse = 0` et `specColor = vec3(1)` : **tout ce qui n'est
pas l'émission est achromatique**. La soustraction de deux canaux élimine ce
terme gris inconnu ; ce qui reste est exactement ce que le graphe a déclaré.

Rendu en RGBA8 **UNORM** (linéaire, pas sRGB) pour que l'attendu reste un entier
exact. Résultats mesurés :

| graphe | pixel central | attendu, calculé depuis le graphe | mesuré |
|---|---|---|---|
| Emission rouge `128/255` | (180, 52, 52) | V == B, écart R−V = **128** | exact |
| Emission verte `128/255` | (52, 180, 52) | R == B, écart V−R = **128** | exact |
| `Mix Shader(rouge, verte, 0)` | (180, 52, 52) | identique au rouge seul, 3 canaux | exact |
| `Mix Shader(rouge, verte, 1)` | (52, 180, 52) | identique au vert seul | exact |
| `Mix Shader(rouge, verte, 0,5)` | (116, 116, 52) | R == V, écarts = **64** = la moitié | exact |

Le plancher achromatique vaut **52** dans les cinq rendus — c'est ce qui autorise
à les comparer entre eux, et c'est vérifié par un cas à part.

**Deux cas existent uniquement pour empêcher un faux vert** : le fond
d'effacement est **magenta**, une couleur que ces graphes ne peuvent pas
produire, de sorte qu'un tracé qui n'aurait pas eu lieu se voie au lieu de se
confondre avec du noir ; et le plancher gris est comparé entre rendus, faute de
quoi toutes les comparaisons croisées seraient sans valeur.

⚠️ **Deux pièges payés en chemin, qui reserviront** :
- le générateur HLSL **déduit la sémantique d'un attribut de son NOM de
  variable** (`NkSLCodeGenHLSLStructs.cpp`) : `aPos` → `POSITION`, mais `aColor`
  et `aUV` ne figurent dans aucune entrée et retombent sur `TEXCOORD<location>`.
  Le layout C++ cesse alors de correspondre au shader **sans le moindre
  message**. Le banc n'utilise donc qu'un seul attribut, dont le nom est dans la
  table, et synthétise les autres varyings dans le vertex.
- `EndCapture` est **obligatoire** avant `ReadbackPixels` : le readback suppose la
  texture en `SHADER_READ`, état que seule cette fermeture rétablit. Écrire la
  passe à la main laisse la texture dans le mauvais état et le readback lit du
  vide **sans se plaindre**.

#### ⚠️ Ce qui reste à savoir avant de continuer

- Le générateur **refuse** un nœud dont il n'a pas d'émetteur, et n'émet **rien**.
  Un nœud sauté laisserait son consommateur lire une locale jamais déclarée : le
  shader ne compilerait pas, et l'erreur accuserait le générateur au lieu du
  graphe.
- Le modèle d'éclairage émis est **celui de LayeredV1, à l'identique** — pour que
  deux matériaux côte à côte dans la même scène se ressemblent. Le jour où
  l'ombrage évolue, les deux doivent évoluer ensemble.
- Le forçage du point décimal dans les littéraux (`2` → `2.0`) est une
  **précaution non prouvée** : sa mutation a survécu, les quatre backends
  acceptent `2`. Elle reste pour Metal et le backend logiciel, non exercés.

### T.2 — détail d'origine (conservé)
- ❌ Les templates matériaux actuels deviennent des graphes pré-câblés
  navigables/éditables — **compatibilité ascendante garantie** (les `.nkasset`
  existants continuent de fonctionner)
- ❌ Nodes de blend (2 textures via masque procédural : bruit, gradient)
- ❌ Masques peints (entrée depuis la peinture 3D, cf. T.3)
- ❌ Nodes de variation procédurale (usure/salissure pilotées par curvature/AO de T.1)
- ❌ Compilation multi-backend via la chaîne shader existante (NkSL → GL/VK/DX)
- ❌ Presets génériques + presets signature (métal patiné doré, motifs Bamiléké,
  « tech-organique ») partagés entre projets
- ⚠️ Substrat de graphe = **NKGraph** (`Kernel/Runtime/NKGraph/ROADMAP.md`,
  décision 2026-07-09) : cœur agnostique partagé avec Blueprint (NKCode), VFX
  (Noge), procédural (AI), anim graphs (NkAnima) ; canvas d'édition dans
  NKEditorKit. Le graphe de matériaux est le **1er consommateur désigné**
  (P5 NKGraph) : il se construit AVEC le cœur, et compile vers NkSL (aucune
  évaluation de graphe au runtime)

### T.3 — Peinture de textures 3D (contrepoids manuel indispensable)
- ❌ Projection écran→UV temps réel, calques non destructifs
  (albedo/roughness/normal séparés), brosses classiques (dureté/opacité/flow)
- ❌ Modes de fusion + undo/redo par calque, export/import de calques inter-assets
- ❌ Stamps génératifs IA (zone + prompt → patch localisé), raccord automatique
  (palette/luminosité/fréquence de détail), bibliothèque de stamps

### T.4 — Génération de textures PBR
- ❌ Albedo depuis texte/référence → dérivation des autres maps (normal from
  height, roughness estimé) → plus tard génération multi-map native cohérente
- ❌ Tileabilité : détection des bords non tileables + correction auto
- ❌ Super-résolution : upscale cohérent cross-maps (albedo/normal/roughness en
  préservant leur relation physique)

### T.5 — Éclairage assisté
- ❌ GI light probes / irradiance volumes (= Phase N « env light probes » déjà
  listée ; prérequis SILENCIEUX de toute suggestion d'éclairage — un setup suggéré
  sur un rendu plat ne rendra jamais bien)
- ❌ Suggestion de setup depuis mood/référence : description → configuration
  structurée de lumières (type/position/couleur/intensité) traduite en lumières
  natives ; bibliothèque de setups classiques (three-point, clair-obscur,
  rim-light) en fallback
- ❌ Génération/calibration HDRI (import exposure/orientation via pipeline
  Phase N existant) + presets signature (jour/nuit/dramatique/doux + identité
  Afrofuturiste) avec variations proposées
- ❌ (R&D, jamais sur le chemin critique) relighting neuronal 2D pour previz
  rapide de mood sans re-render

**Ordre imposé** : T.1 (bake) → T.2/T.3 (éditabilité) → T.4 (génération) → T.5.
L'éditabilité AVANT la génération : une texture générée sans outil de retouche
fine est inutilisable en production stylisée.

---

## Minimum viable UE5-like

État actuel = **~80% du minimum viable** (NkVSM v0 + v1 cascade fade + caching
+ normal bias + per-material override + planar reflection complete ajoutent
~10% par rapport à l'estimation précédente de 70%). Restant pour MVP :
- **Phase H.6 v1 voxel AO précision** (gpu bake + .glsli partagé)
- **Phase L finition** (~~FXAA~~ ✅ + ~~auto-exposure~~ ✅ ; reste API
  SetColorGradingLUT + LUT 3D réelle sur GL)
- ~~**Phase N GPU** (compute prefilter)~~ ✅ 2026-07-12 (reste : GPU sur DX11)
- **Phase E v1** (Materials 2D fonctionnels)
- **Phase F finition** (DX/Metal validation)
- **Phase D.4.2** (NkVSM v2 : ClearRect API + dynamic offsets UBO + shader overrides étendus)

Au-delà : Phase H texture pipeline + Phase M Forward+ + Phase I animation
+ Phase J VFX = renderer **complet** AAA. K/O/P/Q/R/S = spécialisations
selon usage cible (jeu real-time vs cinema vs editor vs VR).

---

## 📌 CONCEPTION EN ATTENTE — la règle de disposition des blocs uniformes, par la réflexion

**Statut : conçu, pas lancé** (2026-08-22). Rien n'est cassé aujourd'hui ; cette
entrée existe pour que celui qui prendra le chantier n'ait pas à re-dériver le
raisonnement. Quinze minutes d'écriture contre une demi-journée de réflexion.

### Le fait qui la motive

`std140` et HLSL **ne rangent pas un bloc uniforme de la même façon**, et leur
désaccord est muet. Mesuré le 22/08/2026, dans les deux sens, en lisant le pixel :

```
uniform NkGraphParams { float usure; vec3 teinte; };
```

- `std140` **aligne** tout `vec3` sur 16 → `teinte` est à **16**.
- HLSL interdit seulement à un membre de **chevaucher** une frontière de 16
  octets ; un `float3` a besoin de 12 octets et tient donc **entier** dans 4..16
  → `teinte` est à **4**.

Le moteur écrivait à 16, le shader lisait à 4. Pixel mesuré `(0,0,0)`, aucune
erreur, aucun journal. En forçant l'écriture à 4 : `(128,0,0)` exact.

⚠️ **Avec un seul `vec3`, les deux conventions donnent zéro.** La panne n'apparaît
qu'au **second** paramètre. C'est pourquoi elle a survécu à une matrice de sept
montages différents, tous verts.

### Ce qui existe déjà, et pourquoi ça ne suffit pas

1. **Le graphe de matériaux est corrigé** (voie 2, `NkMatGraphCompile.h`) : le
   compilateur émet du remplissage nommé `_nkPadN` jusqu'à la prochaine frontière
   de 16 avant chaque vecteur. Les deux conventions tombent forcément au même
   endroit. **Ce garde ne couvre que les blocs ENGENDRÉS.**

2. **Les archétypes écrits à la main sont sains** — vérifié : `NkPBRParams` n'a
   aucun membre `NkVec3f`, ses vecteurs sont tous des `NkVec4f`, ses réels vont
   par groupes de quatre. Mais ils sont sains **parce que la discipline a été
   tenue**, pas parce qu'elle est garantie.

3. **Des `static_assert` sur `sizeof`** gardent les cinq structs de
   `NkMaterialSystem.h` (`NkPBRParams` 96, `NkPBRLayer` 32, `NkLayeredParams`
   208, `NkLayeredV1Params` 336, `NkToonParams` 96). Ils cassent la
   **construction**, pas un banc qu'il faut penser à lancer. ⚠️ **Mais ils ne
   vérifient pas la disposition** : un bloc peut avoir la bonne taille et ranger
   ses membres au mauvais endroit — c'est exactement la faute ci-dessus, où la
   taille était identique des deux côtés.

**C'est une liste de nombres. Une liste se périme ; une règle non.**

### La conception proposée

Un banc console qui parcourt les propriétés **réfléchies** et applique une
règle, pas une énumération.

**Matière première** : `NKReflection` porte déjà `NkProperty` avec son **type**
et son **décalage**. Tout est là ; rien à instrumenter à la main.

**La règle, en une phrase** :

> Dans tout type marqué « bloc uniforme », **tout membre de type vectoriel doit
> se trouver à un décalage multiple de 16**, et la taille totale doit être un
> multiple de 16.

C'est la condition **nécessaire et suffisante** pour que `std140` et HLSL
coïncident : à un multiple de 16, `std140` ne réaligne rien et HLSL ne peut pas
chevaucher. Les scalaires ne posent jamais problème — ils tombent au même
endroit dans les deux conventions tant qu'aucun vecteur ne les suit dans un
registre partiel.

**Pourquoi c'est une règle et pas une liste** : elle grandit avec la structure
toute seule. Un membre ajouté est vérifié sans que personne y pense ; un membre
retiré n'oblige à rien mettre à jour. C'est la même distinction qu'entre une
table de valeurs par défaut par type et un défaut dérivé du registre : la
première se périme silencieusement, la seconde suit.

### Trois pièges à ne pas rater en l'implémentant

1. ⚠️ **Ne pas se contenter de vérifier que la règle est cohérente avec
   elle-même.** Un contrôle qui relit la table de décalages du compilateur et la
   compare à sa propre arithmétique est vert quoi qu'il arrive — c'est
   littéralement ce qui s'est passé pendant trois jours avec le cas
   `variable/decalages-std140`. **Un contrôle qui vérifie une convention ne
   vérifie pas un accord.** La preuve finale reste
   `rendu/parametre-expose-pilote-le-pixel` (`NkMatGraphDemo`), qui **lit la
   valeur depuis la carte** et compare deux variantes — un paramètre contre
   deux. Le banc de réflexion est une commodité rapide ; il ne remplace pas
   celui-là.

2. ⚠️ **Les structs imbriqués comptent.** `NkLayeredParams` embarque deux
   `NkPBRParams`. Un membre vectoriel bien placé dans le parent peut être mal
   placé dans l'enfant, et l'inverse. La règle doit descendre récursivement.

3. ⚠️ **Le marquage « bloc uniforme » doit être une donnée, pas une heuristique
   sur le nom.** Un filtre du genre « les types dont le nom finit par `Params` »
   rate le premier bloc nommé autrement, et le rate en silence — quatrième
   occurrence dans ce dépôt de « chercher un nom n'est pas chercher un usage ».

### Coût estimé et déclencheur

Une demi-journée. **Déclencheur** : le jour où un membre vectoriel entre dans un
struct de bloc écrit à la main, ou le jour où quelqu'un ajoute un archétype. Les
`static_assert` tiendront jusque-là — ils ne laissent pas la dérive être
silencieuse, ils obligent seulement à un geste conscient.

---

## 📌 Les GROUPES de nœuds — mesure du registre, et le regroupement (2026-08-22)

Demande de Rodolf (R8/R9 de `echanges/design.reponses.md`) : *« un groupe est un
groupement de nœuds que l'utilisateur peut empaqueter pour réutiliser à volonté
comme des fonctions »*, et son interface **se déduit** des fils qui traversent la
frontière de la sélection — elle ne se déclare pas.

Preuve : `Applications/NkMatGraphCheck` — **106 cas, 0 échec, 13 mutations sur 13
détectées**.

### 🔴 Le fait d'architecture : il y a DEUX registres, et ils ne répondent pas pareil

Un groupe est un **type de nœud créé par l'utilisateur à l'exécution**. La
question « le registre l'accepte-t-il ? » n'a **pas de réponse unique**.

| | verdict | mesure |
|---|---|---|
| **couche 1** — `NkNodeGraph` | ✅ **accepte** | il n'a **aucun** registre de types de nœuds : `NkNode::type` est une `NkString` libre, `AddNode` ne valide rien. Une clé composée à l'exécution porte des prises, se relie, prend son rang au tri topologique et traverse le fichier **mot pour mot**. |
| **couche 3** — `kProtos` | ❌ **clos à la compilation** | tableau `static const`. `NkMatFindProto`, `NkMatAddNode` **et** le menu `NkMatNoeudsPourPrise` lisent **cette seule source**. |

**La conséquence utile est dans la seconde ligne** : le menu interrogeant le
**même** registre, **ouvrir le registre ouvre le menu sans une ligne de plus**. Il
n'y a **pas deux endroits à réparer**. Le catalogue doit devenir **deux sources
lues par la même porte** — prototypes compilés + prototypes de groupe enregistrés
à l'exécution. ⏳ **En attente d'arbitrage de Rodolf.**

⚠️ **Trou trouvé au passage** : `NkMatValidate` — la validation **de domaine** —
rend **`ok`** sur un graphe portant un type de nœud inconnu ; elle ne consulte pas
le catalogue. Seul l'**émetteur** l'arrête, en le **nommant**. Ça tient tant qu'un
type inconnu est une anomalie ; plus le jour où un groupe est un type légitime.

### La récursion : refusée et nommée, mais à l'APLATISSEMENT

`RecursiveSubgraph` attrape la boucle **directe et indirecte** (deux maillons — un
contrôle regardant le voisin immédiat la manquerait). Mais **la construction
réussit** : un document récursif se bâtit, et n'échoue qu'à l'usage. Le cas exige
**les deux**, pour que l'écart avec ce que Rodolf demande soit *mesuré*, pas
oublié — si le contrôle passe à l'insertion, **le cas tombera et sera relu**.

⚠️ Mesure qui tranche le débat « borne ou refus nommé » : sans le contrôle de
récursion, la borne de profondeur (32) arrête quand même — mais rend
**`trop-profond`**. **Ça s'arrête, et ça accuse le mauvais coupable.**

### `NkGraphGroup.h/.inl` — regrouper / dégrouper

Dans le **cœur**, parce que regrouper est de l'**autorat** : ça ne regarde que des
nœuds, des prises et des liens, jamais ce qu'un type *signifie*. Garde-fou n°1
intact.

Traité : déduplication **par prise source**, ordre déterministe (verticale du
nœud interne, puis horizontale, puis indice de prise, puis identifiant), noms
venus de la prise interne avec homonymes désambiguïsés **dans l'ordre déjà figé**,
liens dedans→dedans laissés à l'intérieur.

⚠️ **Le piège absent de l'énoncé** : chaque graphe tient **son propre registre de
types et ses propres conversions dirigées**. Un sous-graphe créé vide refuse à
l'intérieur un lien réel → couleur que le parent acceptait, et **le fil est perdu
sans un mot** — `Connect` rend une erreur que personne ne lit. Registre **et**
conversions sont recopiés en premier (`ConversionCount`/`ConversionAt`, ajoutés à
`NkNodeGraph` pour ça).

### 🔴 Trois mutations sur huit ont SURVÉCU au premier tour

1. **« l'ordre des prises n'est plus trié » est passée verte.** Le cas comparait
   la **suite des noms** — or ils dérivent des prises internes (`a`, `a_2`, `a_3`)
   et sortent **dans ce même ordre quelle que soit la permutation**. La suite des
   noms est identique quand le câblage est **entièrement permuté**. **Sixième
   occurrence de « je vérifie une étiquette, jamais la relation ».** Le cas compare
   désormais la **correspondance** (qui se branche sur quoi). Il a fallu aussi
   refaire le graphe d'essai : **avec une entrée et une sortie, l'ordre est une
   propriété vide.**
2. et 3. **« défaut de prise non recopié » et « propriété non recopiée » sont
   passées vertes parce que le graphe d'essai n'en portait aucun.** Ce n'est pas
   l'assertion qui manquait, **c'est la matière** — un contrôle ne peut pas voir
   disparaître ce qui n'existe pas.

> **Un contrôle ne vaut que ce que son graphe d'essai porte. Une assertion juste
> sur une matière absente est verte pour rien.**

### ⚠️ Ce que le critère « aller-retour identique » ne prouve PAS

Une **déduplication ratée y survit** : cinq entrées identiques se redistribuent
correctement au dégroupement et le graphe revient identique. Le critère est
**nécessaire, pas suffisant** — d'où un second cas qui regarde l'**interface**.

Deux limites écrites dans le banc : la forme canonique range les nœuds par leur
**contenu** (normaliser le texte sérialisé alignerait deux ordres différents) et
**signale `ambigu`** quand deux nœuds partagent un descripteur, au lieu de rendre
un vert trompeur ; et **`Dégrouper` ne supprime pas la définition** — retirer un
graphe du document décalerait les index, et chaque `graph` rangé dans un
`NkEvalStep` désignerait le mauvais graphe.

## 📌 (b1) — la seconde cible de rendu est EXPRIMABLE (2026-08-22)

**Aucun shader du dépôt ne déclarait deux sorties couleur** — les `@location(1)
out` qu'on y trouve sont tous des varyings de **sommet**. Il n'y avait rien pour
l'attester, et (b1) allait être construit dessus.

✅ Mesuré : les quatre générateurs émettent le second attachement avec le bon
sémantique (`SV_Target0` **et** `SV_Target1` en HLSL, `location = 0` **et**
`location = 1` en GLSL), glslang rend du vrai SPIR-V, et un **témoin** à une seule
cible distingue « la seconde cible est refusée » de « mon shader est mauvais ».

⚠️ **Le cas ne lit pas `success`, et la mutation dit pourquoi** : quand on retire
la seconde sortie, **les quatre colonnes de génération restent à « gen »** pendant
que la cible est entièrement absente. Un générateur qui ignorerait
`@location(1)` et émettrait les deux sorties sur `SV_Target0` écrirait la valeur
auxiliaire **par-dessus la couleur** : image plausible, tampon auxiliaire vide.

### Les quatre canaux, et le format proposé

| canal | contenu | bornes |
|---|---|---|
| **R, G, B** | la valeur de la sortie nommée résolue cette passe | **non bornée par nature** |
| **A** | **validité** — ce pixel porte-t-il cette sortie ? | **{0, 1}, un bit** |

**Le quatrième canal n'est pas un identifiant** : le moteur résout **une seule**
sortie nommée par passe, puisque l'API est **par nom** (`NkMatSortieMateriau`). Il
n'y a rien à identifier — seulement à dire **si le pixel la porte**.

**Proposé : `R16G16B16A16_FLOAT`, 8 o/px (16 Mo en 1080p)**, avec une propriété
**domaine déclaré** sur le nœud `Named Output` ; une seule sortie `non bornée`
promeut la cible en 32 bits, et **la promotion est nommée dans le journal** — le
coût cher devient optionnel et **imputable**. ⏳ **En attente d'arbitrage.**

⚠️ **Ce qu'un matériau sans sortie nommée y écrit** — fait matériel : *une sortie
MRT non écrite sur un pixel couvert est **indéfinie**, pas nulle*. Donc **tout
matériau déclare et écrit la seconde sortie** ; celui qui n'a rien à y mettre
écrit `(0,0,0,0)`, et **quand `A == 0`, RGB n'a aucun sens**. Un lecteur qui
ignorerait `A` lirait `0.0` — une valeur **parfaitement plausible** sur un matériau
qui n'a jamais entendu parler de cette sortie. Le contrôle correspondant se mesure
**sans GPU**, sur le NkSL émis.

## 📌 Les trois arbitrages appliqués — catalogue ouvert, validation réparée, récursion à l'insertion (2026-08-22)

Rodolf a tranché les deux questions remontées plus haut. Preuve :
`Applications/NkMatGraphCheck` — **111 cas, 0 échec, 19 mutations sur 19
détectées**.

### 1. Deux sources, une seule porte

`NkMatFindProto` consulte la table compilée **puis** un registre d'exécution.
Comme `NkMatAddNode`, `NkMatProtoCount`/`NkMatProtoAt` et le menu
`NkMatNoeudsPourPrise` lisaient **déjà** cette porte, **le menu s'est ouvert sans
une ligne de plus** — c'est la propriété mesurée *avant* de demander l'arbitrage,
et c'est elle qui a rendu la décision bon marché.

⚠️ **La condition est posée à l'ENREGISTREMENT, pas à la lecture** : une clé qui
porte le nom d'un proto compilé est refusée en se nommant
(`eclipserait-un-proto-compile`). La collision devenant impossible, l'ordre de
consultation n'a plus de conséquence — on consulte quand même la table statique
d'abord pour que l'invariant se lise dans le code. Le cas vérifie qu'**après** le
refus la porte rend toujours *le proto compilé*, pas seulement que le code de
retour est bon.

⚠️ **Stockage à capacité fixe, et c'est un choix.** Un prototype se lit par
`const NkMatNodeProto*` qui pointe sur des `NkMatSocketDecl` qui pointent sur des
chaînes. Rangé dans un `NkVector`, tout ce monde change d'adresse à la première
réallocation, et le pointeur déjà rendu lit de la mémoire libérée — **sans
planter, en rendant des noms de prises plausibles**. Plafond nommé : 32 groupes,
16 prises, 48 octets.

⚠️ **Limite écrite dans le code** : le registre est unique **pour le processus**.
Deux documents ouverts partagent leurs groupes. La porte `NkMatFindProto(clé)` ne
transporte aucun contexte ; lui en donner un toucherait tous ses appelants. Le
jour où deux documents doivent s'ignorer, c'est **la signature** qu'il faudra
changer, pas ce stockage.

**Le pont** — `NkMatEnregistreGroupe` dérive l'interface de la **frontière** d'un
sous-graphe. Types transportés **par leur nom**, jamais par leur identifiant.

⚠️ **`parPixel` d'un groupe** : vrai dès qu'**un seul** nœud interne est une
source intrinsèque. Et quand c'est **indécidable** — un groupe imbriqué apparaît
comme un `graph.instance` qu'on ne peut pas résoudre sans le document — on prend
le **côté sûr**, parce que les deux erreurs ne coûtent pas pareil :

> `true` à tort **refuse** une sortie licite : faux, mais **bruyant**.
> `false` à tort **accepte** une valeur qui change à chaque pixel et la fait
> passer pour celle du matériau : faux, **plausible**, jamais signalé.

### 2. `NkMatValidate` frappe à la porte — une réparation

Elle rendait `ok` sur un type de nœud absent du catalogue. Le contrôle est posé
**en premier** : compter les sorties d'un graphe dont on ne connaît pas les nœuds
nommerait un défaut secondaire pendant que la vraie cause passe.

⚠️ **Et la réparation a failli dégrader le message.** Avant, l'émetteur disait
« nœud non compilable : *le type* ». La validation le rattrape désormais plus tôt
— donc plus près de la cause — mais rendait un « type-de-nœud-inconnu » **muet sur
lequel**.

> **Attraper plus tôt ne doit jamais faire perdre le nom.**

### 3. Récursion refusée à l'insertion, **et le second filet reste**

`NkPoseInstance` refuse **avant toute modification** — le cas exige que le graphe
soit **inchangé** après le refus. `NkNode::subgraph` restant public, un graphe peut
arriver **par un fichier** sans jamais passer par une insertion : le contrôle à
l'aplatissement est conservé, et **le cas mesure les deux filets séparément**,
sans quoi on pourrait retirer le second sans que rien ne le dise.

### 🔴 Six mutations, deux ont survécu — et la leçon a une troisième forme

1. **« le pont recopie le sens des prises au lieu de l'inverser » est passée
   verte.** Le cas exigeait « une entrée et une sortie » — **vrai aussi quand les
   deux sens sont inversés** : le compte est **symétrique**, le câblage non. Le
   prototype avait toutes ses prises à l'envers, et le cas affichait
   « 1 entrée(s) 1 sortie(s) (1 et 1 attendues) ». **Septième occurrence de « je
   compte, je ne relie pas »**, et la plus fourbe : le nombre n'était pas
   approximativement juste, il était **exactement** juste.
2. **« la détection de récursion réduite au voisin immédiat » est passée verte —
   et la cause n'est pas le cas, c'est le graphe d'essai.** À deux maillons,
   regarder le voisin immédiat **suffit** : le contrôle récursif n'était pas
   mesuré **du tout**. Ajout d'une chaîne à **trois** maillons.

> **Trois formes du même défaut, rencontrées en trois jours : il manquait la
> MATIÈRE (aucun défaut de prise dans le graphe), puis la RELATION (des noms sans
> leur câblage), puis la PROFONDEUR (une chaîne trop courte). Une assertion juste
> sur un graphe d'essai trop pauvre ne mesure rien — et elle est verte.**

## ⚠️ DÉFAUT CONNU, CORRECTION PLANIFIÉE — le registre de groupes est unique pour le processus

**Le registre de groupes est unique pour le processus ; deux documents ouverts
partagent leurs groupes.** Défaut connu, correction planifiée.

Ce n'est pas une simplification acceptable. Dans une application qui ouvre
plusieurs documents — NK3DModeler, NKScena, Nogee — il produit trois symptômes, et
**aucun ne se voit dans un banc** :

1. un groupe défini dans le document A apparaît dans le menu de B ;
2. deux documents indépendants entrent en collision de noms, et le second se voit
   refuser un nom qu'il est seul à employer ;
3. B casse quand A se ferme.

> **Ça se découvrira chez un utilisateur, pas dans un banc.**

**Ce qu'il faut corriger n'est pas le stockage, c'est la SIGNATURE de la porte.**
`NkMatFindProto(clé)` ne transporte aucun contexte ; tant qu'elle n'en transporte
pas, aucun rangement ne peut séparer deux documents. Le stockage à capacité fixe,
lui, reste un choix délibéré et bon.

**Quand** : juste après (b1). Décalé **volontairement** — ouvrir la signature
touche tous les appelants, et mêler ce changement à la seconde cible de rendu
donnerait une mesure qui porte sur deux choses à la fois. Le changement viendra
seul, **avec son propre témoin : *deux documents ne voient pas les groupes l'un de
l'autre***.

📌 Écrit ici et dans `NkMatGraphTypes.h` parce qu'**une limite écrite se corrige ;
une limite sue se transmet en s'effaçant**.

## ✅ (b1) — la sortie nommée PAR PIXEL, vers une seconde cible de rendu (2026-08-22)

> ### ⚠️ PORTÉE — à lire avant le compte de cas
>
> **(b1) est prouvé jusqu'au NkSL émis, et pas au-delà. Aucune image n'a été
> rendue, aucune valeur relue depuis la carte. Manquent : la cible
> `R16G16B16A16_FLOAT` côté RHI, son effacement à `(0,0,0,0)`, et une lecture
> réelle.**
>
> Sans cette phrase, « 116 cas, 0 échec, 26 mutations sur 26 » se lit comme « la
> seconde cible fonctionne ». **Elle ne fonctionne pas : elle est correctement
> décrite.** C'est beaucoup, ce n'est pas la même chose — et c'est précisément
> l'écart qu'on passe des nuits à traquer ailleurs.
>
> 📌 La même phrase s'imprime **avec le compte**, à la fin de `NkMatGraphCheck`.
> Rangée seulement ici, elle serait vraie et jamais lue.

L'étage `par_pixel_cible` est **construit**. Format signé par Rodolf :
**`R16G16B16A16_FLOAT`, 8 o/px** — RGB = la valeur, **A = la validité**.

Preuve : `Applications/NkMatGraphCheck` — **116 cas, 0 échec, 26 mutations sur 26
détectées**. Tout se mesure **sans GPU**, sur le NkSL émis, parce que la propriété
qui compte est syntaxique : la sortie est-elle déclarée, est-elle **écrite**, et
avec quel alpha.

### Le contrat, en une ligne

> **RGB = la valeur de la sortie résolue ; A = 1 si ce pixel la porte, 0 sinon.
> Quand `A == 0`, RGB n'a aucun sens et le lecteur n'a pas le droit de le lire.**

⚠️ **Tout matériau déclare ET écrit la seconde cible**, y compris celui qui n'a
aucune sortie nommée — parce qu'**une sortie MRT non écrite sur un pixel couvert
est INDÉFINIE**, ni conservée, ni nulle. Celui qui n'a rien à y mettre écrit
`(0,0,0,0)`.

Un matériau d'étage **(a)** écrit **zéro lui aussi** : (a) est une constante
calculée sur le processeur, lui faire payer une écriture par pixel viderait
l'argument « quasi gratuit ».

### 🔴 Le même piège, découvert un étage plus haut

`NkMatSortieMateriau::valeur[3]` est la valeur **processeur**. Une sortie (b1)
n'en a aucune — sa valeur naît dans le shader. Le tableau restait donc **à zéro**,
et ce zéro **se lit exactement comme « la valeur vaut zéro »**.

C'est le canal alpha, transposé du tampon vers la structure C++ — et il entrait
**par la porte de derrière**, dans le code écrit pour le corriger ailleurs.

> **Que la même faute change d'étage sans changer de forme est ce qui la rend
> générale : ce n'est pas un défaut du tampon, c'est l'absence de distinction
> entre « rien » et « zéro », partout où elle n'est pas explicitement portée.**

Le zéro est toujours disponible, toujours plausible, et ne se signale jamais. Le
cœur le fait déjà bien pour les valeurs de graphe — « jamais renseigné » a **une
seule** représentation, `type == NK_TYPE_INVALID`. C'est la même règle, et elle
était déjà écrite dans `NkNodeGraph.h`. Réparé de la même façon : un drapeau
`valeurConnue`. Un lecteur qui l'ignore lit `false` et doit s'en occuper ; il ne
peut pas se tromper en silence.

📌 **Et deux étapes de la passe des sorties étaient devenues fausses en silence.**
Le refus « ta sortie dépend du pixel » et l'évaluation processeur s'appliquaient à
**tous** les étages — implicitement correct tant qu'un seul étage existait. Le
message disait déjà « est déclarée `par_materiau` », mais **le code ne le
vérifiait pas** : (b1) est par pixel *par définition*, le refuser pour cette
raison aurait refusé l'étage entier.

> **Une condition implicitement vraie parce qu'il n'existe qu'un seul cas devient
> fausse le jour où le second arrive — et elle ne prévient pas.**

### Les refus, nommés

- une sortie **demandée mais absente** du graphe → erreur qui **la nomme**.
  ⚠️ Rendre un tampon vide serait le repli plausible : il se lirait comme « ce
  matériau ne porte pas cette valeur », **indiscernable du cas légitime**.
- **plusieurs** sorties `par_pixel_cible` et **aucune** demandée → refus. Prendre
  « la première » rendrait une valeur crédible issue d'une sortie que personne n'a
  demandée. Un **témoin** vérifie que la même scène compile dès qu'on nomme la
  sortie voulue.

### Reste à faire pour (b1)

La preuve de **rendu** n'est pas atteinte : tout ce qui précède se mesure sur la
source émise. Manquent la création de la cible `R16G16B16A16_FLOAT` côté RHI, son
effacement à `(0,0,0,0)`, et une lecture réelle depuis la carte. **Le domaine
déclaré par sortie** (borné / non borné) et la **promotion 32 bits nommée** ne
sont pas non plus construits — ils le seront quand une sortie réelle en aura
besoin, et pas avant.

## 📌 La connaissance existe, mais pas là où quelqu'un la chercherait (2026-08-23)

Trois formes, sorties la même nuit sur trois chantiers, même racine :

| forme | ce qu'elle coûte |
|---|---|
| une limite **sue** et non écrite | se transmet **en s'effaçant** |
| une limite **écrite au mauvais endroit** | **ne se transmet pas du tout** |
| deux documents qui **divergent** | **fabriquent du faux travail** — deux séances de décision préparées sur des points déjà tranchés ailleurs |

⚠️ **La deuxième est la plus coûteuse, et elle a été payée ici même.**
`Kernel/Runtime/NKGraph/src/NKGraph/NkNodeGraph.h` porte **depuis le début**
exactement la règle qui manquait à `NkMatSortieMateriau` : *« JAMAIS RENSEIGNÉ et
RENSEIGNÉ À VIDE sont deux états différents »*, avec « jamais renseigné » ramené à
**une seule** représentation. Sa note cite même le piège **payé** par l'agent
NkUIDesign dans la nuit du 21 au 22/08.

> **Quelqu'un l'avait déjà payé. Il l'avait écrit. Et il a été repayé quand même,
> deux étages plus haut** — non par négligence, mais parce que **rien ne pousse à
> lire l'en-tête du module d'en dessous quand on écrit une structure au-dessus**.
> La règle était disponible, gratuite, et invisible.

**La question, ajoutée au préambule de `NkMatGraphCheck` comme cinquième règle :**
*cette règle existe-t-elle déjà quelque part sous moi ?*

⚠️ Et elle **n'a pas de réponse mécanique** — il ne faut pas prétendre le
contraire. On ne relit pas le noyau avant chaque structure, et aucun outil ne dira
« la règle que tu t'apprêtes à violer est écrite trois modules plus bas ». La
poser vaut quand même mieux : elle coûte trente secondes sur les questions où l'on
**sait** qu'un module d'en dessous a déjà tranché — la représentation d'une valeur
absente, la stabilité d'un identifiant, l'ordre d'évaluation — et ce sont
justement celles où la réponse existe.

## ✅ La signature de la porte est ouverte — deux documents ne se voient plus (2026-08-23)

Le défaut déclaré la veille est **corrigé**, et il l'a été **seul**, après (b1) :
mêler sa mesure à celle de la seconde cible en aurait fait une mesure qui porte
sur deux choses à la fois.

**Ce qui a changé n'est pas le stockage — c'est la signature.** Le registre de
prototypes est devenu un **paramètre de toute la couche 3** :
`NkMatFindProto`, `NkMatProtoCount`/`NkMatProtoAt`, `NkMatAddNode`,
`NkMatNoeudsPourPrise(De)`, `NkMatValidate`, `NkMatEnregistreGroupe`,
`NkMatCompileToNkSL`, et en interne `CalculeContagion` /
`TrouveSourceParPixel`. **307 points d'appel** réécrits (banc et démo).

⚠️ **Et il n'a AUCUNE valeur par défaut.** Un défaut qui serait retombé sur un
registre global aurait laissé le défaut atteignable en silence : tout appelant qui
n'y aurait pas pensé aurait continué de partager. Ici le compilateur oblige chaque
appelant à dire **de quel document il parle** — désagréable une fois, définitif.
Le singleton `NkMatRegistre()` **n'existe plus** ; il n'y a pas de porte de
derrière à refermer plus tard.

### Le témoin, et il mesure les quatre symptômes annoncés

`groupe/deux-documents-ne-voient-pas-leurs-groupes` — ils ne se corrigent pas
forcément ensemble, donc ils sont mesurés séparément :

1. un groupe défini dans A n'est **pas trouvable** depuis B ;
2. il n'apparaît **pas dans le menu** de B ;
3. A et B emploient **le même nom** sans se gêner — le plus fourbe, parce qu'il se
   manifestait par un refus d'apparence légitime (« déjà enregistré ») sur un nom
   que le second document est pourtant seul à employer ;
4. **fermer A ne casse pas B**.

Plus un **témoin interne** : chaque document voit **son propre** groupe. Sans lui,
un registre cassé qui ne rendrait jamais rien passerait les quatre contrôles pour
la pire des raisons. Et `chezA != chezB` — deux documents qui pointeraient sur une
**seule** définition rejoueraient le défaut sous une autre forme.

### 🔴 La mutation qui compte

Rendre le stockage `static` — **deux jetons** — remet toutes les instances en
commun sans toucher à une seule signature. Elle reproduit **les quatre symptômes
d'un coup**, mot pour mot :

```
chacun voit le sien=11 et pas celui de l autre=00 | menus : le sien=1 pas l autre=00
| meme nom des deux cotes 'ok'/'deja-enregistre' chacun le sien=0
| B survit a la fermeture de A=0
```

Deux autres mutations séparent proprement les deux moitiés : quand le **menu**
cesse de consulter le registre, la partie « trouvable » reste verte et seule la
partie « menu » tombe.
