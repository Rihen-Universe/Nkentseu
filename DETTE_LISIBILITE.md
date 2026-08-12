# Dette de lisibilité — découpage, repères, garde-fous

> Décidé avec Rihen le 12 août 2026, après un état des lieux mesuré du dépôt.
> **Ces chantiers se font ENTRE deux fonctionnalités, jamais pendant.**
> Ordre convenu : d'abord finir les **matériaux** et l'**éditeur**, ensuite ceci.

## Pourquoi ce document

Le dépôt compte ~2 300 fichiers source et près d'un million de lignes, pour un
moteur multi-backend et quatre applications. Le constat n'est **pas** que le
code serait obscur : les commentaires expliquent le *pourquoi* et gardent la
trace de ce qui a été essayé — c'est au-dessus de la moyenne de l'industrie, et
c'est ce qui permet de corriger un défaut sans en créer trois.

Le constat est que le code est **mal rangé**. La différence compte : mal
expliqué se soigne difficilement, mal rangé se soigne mécaniquement. Un
développeur compétent devrait être autonome en une semaine ; aujourd'hui il
passerait un mois à cartographier.

## La règle, valable pour TOUTES les applications

NK3DModeler, NKCode, NkForma, NkAnima, NkScena, Nogee, Sandbox : même exigence.

1. **Un fichier = une responsabilité qu'on peut nommer en une phrase.** Si la
   phrase contient « et », le fichier doit être coupé.
2. **Le nom dit le contenu.** Un fichier nommé « Demo » qui porte tout le
   viewport d'une application de production est un nom qui ment, et un nouveau
   venu ne le cherchera jamais là.
3. **Plafond indicatif : ~2 000 lignes.** Au-delà, on ne relit plus, on
   cherche. Ce n'est pas une règle absolue (un décodeur, une table de données
   embarquée peuvent dépasser) mais un signal.
4. **En-tête de fichier utile** : ce que le fichier contient, ce qu'il ne
   contient **pas**, et vers quoi renvoyer pour le reste. Deux lignes suffisent.

## Les cinq chantiers

### 1. Découper et renommer `NkDemo3D.cpp` — 16 653 lignes

Le premier obstacle, et le plus rentable. Ce fichier porte **tout** le viewport
du modeleur sous un nom qui annonce une démo. Le renommage seul
(`NkModelerViewport`) supprime la confusion ; le découpage par domaine
(caméra/vue, matériaux hôte, édition de maillage, gizmos, sortie/rendu, outils)
rend chaque partie relisible.

Déjà noté comme dette ailleurs : à faire **après** la sauvegarde de scène
poussée, **jamais** pendant un chantier — un découpage au milieu d'une
fonctionnalité produit des conflits et des régressions muettes.

Même traitement ensuite pour `NkModelerScreens.h` (13 963 lignes) et
`NkCodeState.h` (7 754).

### 2. Refaire `ARCHITECTURE.md` — il existe (540 lignes) mais il MENT

Vérification faite le 12 août, l'écart avec le dépôt est frontal. Le Kernel a
**cinq familles** — `Foundation`, `Runtime`, `System`, `AI`, `Bare` — dont deux
que le document ignore complètement :

- `Kernel/AI/` — 15 modules (NKAgent, NKTensor, NKTrain, NKGen, NKRL, NKNN…) ;
- `Kernel/Bare/` — 14 modules (NKBoot, NKScheduler, NKDriver, NKInterrupt…).

Et dans `Runtime` :

| | |
|---|---|
| **Cités, inexistants** | NKScene, NKAnimation, NKBody, NKEmotion, NKFace, NKPatientRenderer, NKDiagnosticEngine, NKScript, NKSpeech |
| **Réels, jamais cités** | NKECS, NKSL, NKGui, NKCanvas, NKXR, NKMedia, NKGraph, NKCamera, NKCollision, NKNavigation, NKSimulation |

Le document décrit l'architecture **prévue** au départ (Noge, PV3DE, un ECS
nommé NKScene) ; le dépôt a évolué ailleurs. Un nouveau venu qui s'y fie
cherche des modules absents et passe à côté de ceux qui portent le travail.

À noter : `Kernel/AI/` et `Kernel/Bare/` ont **chacun leur propre
`ARCHITECTURE.md`**. La documentation par famille existe donc — c'est le
document racine qui n'a pas suivi. Il doit répondre en une page à « je veux
modifier X, où est-ce ? », et renvoyer aux documents de famille pour le détail.

### 3. Créer `SHADERS.md` — les pièges du dialecte NkSL

Trois paragraphes qui économisent des jours. Ils sont connus, ils ont coûté
cher, et ils ne sont écrits nulle part de façon centralisée :

- **Aucun gradient dans une boucle de lumières** (`dFdx`/`dFdy`, `texture()`
  implicite) : fxc refuse de dérouler (X3570 puis X3511), la création du shader
  échoue, le pipeline garde un blob périmé — et l'objet rend **noir** sans autre
  trace que le journal.
- **L'alpha de sortie est ce que la fusion utilise.** `pbr.frag` a longtemps
  fini par `vec4(color, 1.0)` : toute la file transparente était mélangée à
  100 % alors que curseur, routage et pipeline étaient corrects.
- **Les emplacements ont changé.** Disposition moderne : `MaterialUBO` en
  `set=2 binding=8`, textures `set=2 binding=3..7`, varyings
  `vWorldPos/vNormal/vUV/vColor` en location 0..3. Les anciens `.vk.glsl` visent
  des slots que le système de matériaux ne remplit pas — ils « existent » sans
  pouvoir rien rendre.

### 4. Vérifier les layouts UBO automatiquement

Les blocs partagés entre C++ et shaders doivent correspondre **au champ près**,
sans aucune vérification aujourd'hui. Le commentaire de `CameraUBO` l'admet :
« une erreur d'UN seul vec4 ici décale tout le bloc et corrompt le rendu ».

`ObjBlock` a déjà son `static_assert(sizeof(...) == 224)`. Étendre ce réflexe à
tous les blocs partagés transforme une corruption silencieuse en **erreur de
compilation**. C'est peu de travail pour beaucoup de sûreté.

### 5. Quelques tests sur les briques pures

Math, conteneurs, générateur NkSL : des fonctions déterministes, sans GPU, où un
test coûte cinq minutes et rattrape des régressions invisibles.

**Pas de tests sur le rendu** : la validation par capture d'écran reste la bonne
méthode là, parce que le défaut est visuel et qu'aucune assertion ne remplace un
œil sur une image.

## Dettes de capacité repérées en chemin

Ce ne sont pas des chantiers de rangement, mais des **plafonds statiques** qui
tiendront tant qu'on travaille sur des scènes d'essai et sauteront au premier
modèle importé pour de vrai :

- **64 matériaux par projet** (`kNkvpMaxProjMats`). Un `.gltf` d'objet réel en
  aligne couramment vingt ou trente ; un décor complet dépasse. La liste de
  matériaux *par objet* suit désormais cette borne (12 août), donc la lever
  bénéficierait aux deux d'un coup.
- **160 nœuds par scène** (`kNkvpMaxNodes`).

Le remède est le même dans les deux cas : passer du tableau statique à une
collection. À faire quand l'import de modèles réels arrivera, pas avant.

## Ce que ça n'est pas

Ce n'est pas une réécriture, ni un changement d'architecture. Les décisions
techniques du moteur sont saines ; c'est leur **rangement** qui est en cause.
Aucun de ces chantiers ne doit changer un comportement observable.
