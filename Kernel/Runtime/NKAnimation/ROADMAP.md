# NKAnimation — Roadmap (substrat d'animation, sans GPU)

> **Ce document décrit le MODULE.** Le **parcours produit** (jalons M0→M5, IK,
> physique de pose, IA de direction) vit dans **`Applications/NkAnima/ROADMAP.md`**
> (548 l.) et n'est **pas** recopié ici — deux documents qui se recouvrent à
> moitié sont pires qu'un seul.
>
> La **frontière** avec NKRenderer et la raison de chaque choix de placement sont
> écrites dans **`NKAnimation.jenga`**, en-tête du fichier. Elles ne sont pas
> recopiées non plus.
>
> Ici, et seulement ici : **ce que le module contient, ce qu'il expose, qui le
> consomme, ce qu'il lui manque, et où il va.**
>
> **Provenance de tous les chiffres de ce document** : worktree
> `Nkentseu-nkanim`, branche `feat/nkanimation`, commit `08f41cdc`, mesuré le
> **2026-08-17**. Les compilations témoins sont en **g++ 16.1.0, `-std=c++20`,
> `-fsyntax-only`** (configuration de syntaxe, ni Debug ni Release — aucune des
> affirmations ci-dessous ne dépend d'un comportement d'exécution).

## Ce que le module EST

Le **modèle** d'animation : ce qui calcule une pose. Aucun GPU, aucun
périphérique, aucun format d'échange. Il ne connaît que Foundation.

C'est cette pureté qui permet à NkAnima, PV3DE, Noge et NKScena d'animer **sans
tirer le renderer** — et à une application 2D d'animer tout court, ce que la
règle d'exclusivité NKCanvas/NKRenderer interdisait avant l'extraction du
2026-08-14.

## Contenu mesuré — 4 unités, 8 fichiers, 3 456 lignes

| Unité | Fichiers | Lignes | Ce qu'elle fait |
|---|---|---|---|
| `NkAnimation` | `.h` 705 + `.cpp` 1 195 | **1 900** | clips et keyframes, échantillonnage (`NkInterpMode` : **9 modes** — step, linéaire, cubique et 6 easings), `NkAnimationPlayer`, `NkBlendTree1D`/`2D`, `NkAnimStateMachine` (HFSM), format binaire `.nkanim` |
| `NkAnimRetarget` | `.h` 174 + `.cpp` 486 | **660** | rejouer un clip sur un AUTRE squelette |
| `NkMotionPath` | `.h` 113 + `.cpp` 383 | **496** | spline Catmull-Rom + suivi de trajectoire |
| `NkAnimationEditor` | `.h` 120 + `.cpp` 280 | **400** | modèle d'édition de pose-clés + annuler/refaire |

Espace de noms unique : **`nkentseu::anim`**. Module **frère** de `NKAnimPhysics`
(`nkentseu::animphys`), jamais son parent : les deux ne s'incluent pas.

## Surface publique

`NkAnimation.h` — `NkInterpMode`, `NkPlayMode`, `NkAnimationTrack<T>`,
`NkAnimationClip`, `NkAnimationState`, `NkAnimationPlayer`, `NkBlendTree1D`,
`NkBlendTree2D`, `NkAnimStateMachine`.

`NkAnimRetarget.h` — `NkRetargetSkeleton`, `NkRetargetMap`, `NkRetargetParams`,
`NkAnimRetarget` (API **entièrement statique** : `BuildMapByName`,
`NormalizeJointName`, `RetargetPose`, `RetargetClip`, `HeightRatio`, `SelfTest`).

`NkMotionPath.h` — `NkMotionCurve`, `NkPathLoopMode`, `NkPathTargetMode`,
`NkPathFollow`, `NkPathTarget`.

`NkAnimationEditor.h` — `NkAnimationEditor` (curseur/snap, insertion et
déplacement de pose-clés, sélection, `Undo`/`Redo`).

⚠️ **`NkAnimationEditor` est une FEUILLE** : rien dans le substrat ne le tire.
Un consommateur qui ne s'en sert pas ne le paie pas. Ne pas « l'optimiser » en le
sortant du module — le raisonnement complet est dans `NKAnimation.jenga`.

## Dépendances et consommateurs

**Dépend de** (déclaré dans `NKAnimation.jenga`) : NKPlatform, NKCore, NKMemory,
NKMath, NKContainers, NKLogger, NKFileSystem. **Rien d'autre.**

**Consommé par — 13 fichiers mesurés** (`grep -rl "NKAnimation/"`, hors le module
lui-même) :

| Couche | Fichiers |
|---|---|
| Kernel | `NKRenderer/Core/NkRendererImpl.h`, `NKRenderer/Mesh/NkGLTFAnimBake.h`, `NKRenderer/Tools/Animation/NkAnimationSystem.h`, `NKRenderer/Tools/IK/NkIKSystem.cpp` |
| Engine | `Noge/Anim/NkLocomotion.h` |
| Applications | `DemoRW`, `NK3DModeler/Viewport/NkDemoRenderer.h`, `NkAnimaEditor/AnimBridge.cpp`, `NkAnimPhysTest`, `NkLocomotionDemo`, `Sandbox/DemoAnim.cpp`, `Sandbox/DemoAnimIK.cpp`, `Sandbox/NkRenderer.h` |

⚠️ **`NKRenderer/Tools/Animation/` n'a PAS été vidé le 14/08** — vérifié
aujourd'hui, 4 fichiers, **492 lignes** : `NkAnimationSystem.{h,cpp}` (la façade
qui téléverse et soumet) et `NkPoseDebugDraw.{h,cpp}`. C'est **ce qui dessine**,
et ça consomme ce module. La ligne « Dépendances / liens » de
`Applications/NkAnima/ROADMAP.md` qui cite `Tools/Animation` est donc **exacte** :
ne pas la « corriger ».

## Conformité

| Contrôle | Mesure du 2026-08-17 |
|---|---|
| zero-STL | **0** occurrence de `std::` dans `src/` |
| dette annotée | **0** `TODO` / `FIXME` / `HACK` |
| GPU | aucune inclusion NKRHI / NKRenderer |

## ⚠️ Dettes NOMMÉES, non corrigées

### 1. Le module bute sur un manque de NKContainers — mesuré, pas déduit

`NkAnimation.h` déclare **4 champs `NkHashMap`** :

```
l. 277  NkHashMap<NkString, NkAnimationTrack<float32>> customFloats;
l. 278  NkHashMap<NkString, NkAnimationTrack<NkVec4f>> customVec4s;
l. 684  NkHashMap<NkString, bool>                      mBools;
l. 685  NkHashMap<NkString, float32>                   mFloats;
```

**`NkHashMap` ne sait pas accueillir un type move-only.** Prouvé par unité de
traduction jetable, pas par lecture :

```
NkHashMap.h:1084: error: use of deleted function 'MoveOnly& MoveOnly::operator=(const MoveOnly&)'
                  ligne fautive :  node->Data.Second = value;
```

Conséquence pour ce module : `NkAnimationTrack<T>` ne peut **jamais** devenir un
type valeur non copiable tant que ce trou n'est pas comblé — donc pas de piste de
keyframes qui possède un tampon, pas de clip qui possède ses données sans
partage. C'est la même dette qui a forcé le pool de slots de
`NKRenderer/Streaming/NkStreamingSystem`.

⚠️ **Et la documentation de `NkHashMap` prescrit une API qui n'existe pas** —
`NkHashMap.h:1533` écrit `data.Insert(i, nkentseu::traits::NkMove(values));`
comme conseil de performance. Il n'y a **aucune** surcharge par rvalue.
C'est la même forme que les six commentaires de `NkImage.h` qui prescrivaient un
`Free()` : *rien ne contraint un commentaire, donc il dérive.*

**Suivi** : chantier NKContainers en cours, canal `echanges/nkanim.questions.md`.

### 2. Ce qui manque au modèle lui-même

Vérifié **dans ce module** le 2026-08-17 — et c'est la précision qui compte : les
constats d'absence du 14/08 portaient sur `NKRenderer/Tools/Animation/`, qui ne
contient plus le modèle depuis l'extraction. La mesure était donc prise au
mauvais endroit ; celle-ci ne l'est pas.

| Manque | Mesure | Pourquoi c'est un manque |
|---|---|---|
| **Blend ADDITIF** | 0 occurrence de `additi*` | `NkBlendTree1D`/`2D` sont des blends de **remplacement**. Sans additif, pas de couche « overlay haut du corps » par-dessus une locomotion. |
| **Mouvement secondaire** (spring / jiggle bones) | 0 occurrence de `spring`, `jiggle` | Rien ne suit passivement le mouvement du squelette. |
| **Squelette hiérarchique en T+R+S séparés** | absent | Le clip stocke des `NkMat4f` bone-local. L'interpolation en rotations pures (slerp) et le rig facial en dépendent tous les deux. |
| **Rig facial, volet animation** | `Noge/Facial/NkFacialRig.h` = **528 l. sans un seul corps de fonction** | Spécification écrite, implémentation nulle. Demande d'abord de trancher ce qui relève de l'animation et ce qui reste géométrie de maillage. |
| **Consommateur anim de NKGraph** | substrat livré (1 519 l., P1-P3), pas de client anim | L'édition visuelle d'un anim graph n'a pas de client. Ne rien construire en silo ici : cf. `Kernel/Runtime/NKGraph/ROADMAP.md`. |

### 3. Hors périmètre assumé du reciblage — écrit, pas oublié

`NkAnimRetarget` ne fait **pas** : verrouillage de pied au sol, appariement par
analyse de morphologie, correction de volume. C'est écrit dans son en-tête ; c'est
répété ici pour qu'on ne le redécouvre pas comme un bug.

### 4. Dette héritée d'une dépendance

`NkQuat::SLerp` — branche trigonométrique signalée buggée (grosses rotations,
`dot < 1-ε`) dans `Applications/NkAnima/ROADMAP.md`, contournée par NLerp qui est
le mode de production. ⚠️ **Non re-mesuré aujourd'hui** : reporté tel quel, avec
sa source, précisément parce qu'un fait qui voyage n'est pas un fait vérifié.

## Les trois horizons

*(Exigés par le `CLAUDE.md` parent, l. 481. Ce sont des objectifs réfutables :
chacun nomme ce qui le prouverait faux.)*

**Court — la semaine.** Combler le trou move-only de `NkHashMap` dans
NKContainers, puis retirer les 4 `NkHashMap` de ce module de la liste des
contraintes. *Réfuté si* : après le correctif, l'unité de traduction témoin
échoue toujours.

**Moyen — le jalon.** Le **squelette hiérarchique en T+R+S séparés**. C'est le
verrou commun de trois choses qui attendent : l'interpolation en rotations pures,
le blend additif, et le rig facial. *Réfuté si* : on livre l'additif sans lui —
alors ce n'était pas le verrou.

**Long — ce à quoi le module sert.** Qu'une pose puisse être calculée **sans
renderer, sans OS et sans écran** : c'est ce qui met la même animation dans un
jeu Noge, dans le patient virtuel de PV3DE, dans un outil 2D et, à terme, dans
un test sans machine graphique. Le facteur d'échelle est là : *ce qui rend le
module utilisable dans un quatrième contexte vaut plus que ce qui le rend 10 %
plus rapide dans le premier.*

## 🔴 Le régime AÉRIEN manque — M3.7 → M3.11 (PLAN OFFICIEL, approuvé le 2026-08-17)

*Mesuré par l'agent NKAnimation sur `feat/nkanimation` le 2026-08-16, en préparant
l'écart entre les spécifications d'interface de NkAnimaEditor et le code.
**Approuvé par Rodolf le 2026-08-17.** Le travail ne commence pas maintenant —
c'est Rodolf qui en décidera le moment ; quand il viendra, **M3.9 (tenseur
d'inertie) en premier** : deux des trois briques du régime aérien n'ont aucun
support mathématique sans lui.*

### Le constat, en une phrase

**L'équilibre STATIQUE est fait, la dynamique ne l'est pas.**

`NKAnimPhysics` porte six briques **M3.1 → M3.6, toutes implémentées** : 349 lignes
d'en-têtes contre **1 272 lignes de `.cpp`**, aucune coquille vide.

| brique | rôle | h / cpp |
|---|---:|---:|
| M3.1 `NkPoseMass` | distribution de masse, COM | 61 / 208 |
| M3.2 `NkBalance` | équilibre, polygone de support, marge signée | 59 / 250 |
| M3.3 `NkContactDetector` | solveur de contacts | 55 / 149 |
| M3.4 `NkPoseBalancer` | optimiseur de pose, pieds plantés | 82 / 372 |
| M3.5 `NkAutoPose` | auto-posing | 41 / 131 |
| M3.6 `NkClipBalancePass` | passe d'équilibre sur clip | 51 / 162 |

⚠️ **Mais `Applications/NkAnima/ROADMAP.md` ouvre M3 en annonçant « trajectoires
physiquement correctes (**centre de masse balistique**, équilibre) ».** Le
balistique n'a jamais été livré : les six briques couvrent la moitié « équilibre »,
pas la moitié « trajectoire ». Le milestone est déclaré « BOUCLE COMPLÈTE » — il
l'est **pour le régime au sol**, et il faut le lire ainsi.

Mesure directe : **zéro occurrence** de gravité, parabole, balistique ou phase
aérienne dans tout `NKAnimPhysics`.

### Pourquoi M3.7 et pas M4

**`M4` est déjà pris** — « M4 — IA auto-pose », plus `M4bis` et `M5`, dans
`Applications/NkAnima/ROADMAP.md`. Numéroter le régime aérien « M4 » créerait une
collision silencieuse. Il prolonge les briques M3, il en prend donc la suite.

### Les cinq briques proposées

1. **M3.7 — Trajectoire balistique du COM.** Sous gravité, entre deux contacts :
   le COM suit une parabole que l'animateur ne peut pas contredire. C'est la moitié
   manquante annoncée par M3. *Réfuté si* : une phase aérienne reste corrigible
   horizontalement par l'optimiseur, ce qui prouverait que la contrainte n'est pas
   appliquée.
2. **M3.8 — Conservation du moment cinétique en vol.** Une fois quitté le sol, le
   moment est constant : c'est ce qui rend une vrille ou un salto non négociables.
   Le moment existe dans `NKPhysics` (corps rigides), **pas pour la pose**.
3. **M3.9 — Tenseur d'inertie morphologique.** ⚠️ **Verrou de M3.8, à traiter
   avant.** `NkPoseMass` est un modèle de **masses scalaires ponctuelles**
   (`COM = Σ mⱼ·pⱼ / Σ mⱼ`) : il **ne peut pas porter de rotation**. Sans tenseur
   dérivé de la morphologie, le moment cinétique n'a pas de support mathématique.
4. **M3.10 — Mouvement secondaire dynamique.** Déjà nommé en dette dans ce fichier :
   *0 occurrence de `spring`, `jiggle`* — rien ne suit passivement le squelette.
5. **M3.11 — Interpolation contrainte.** Interpoler en respectant les contraintes
   physiques plutôt que corriger après coup. `NkAutoPose` fait aujourd'hui
   *lerp puis correction* ; c'est l'ordre inverse qui garantit la plausibilité.

**Ordre suggéré : M3.9 → M3.7 → M3.8**, puis M3.10 et M3.11 indépendamment. Le
tenseur d'inertie est le verrou : deux des trois briques du régime aérien en
dépendent.

> Note de cadrage : l'inspiration est nommée depuis M3 et reste une **inspiration**.
> Ce fichier ne compare pas ce module à un produit existant, et ne le prétend pas
> équivalent.

### ⭐ Et le socle existant n'est PAS exposé

**`NkAnimaEditor` n'utilise AUCUNE des six briques M3.** Il ne consomme que le
ragdoll, via `NkRagdollBridge` (`NKPhysics`). Ni `NkPoseMass`, ni `NkBalance`, ni
`NkContactDetector` n'apparaissent dans l'éditeur.

Ce n'est pas une surprise pour le corpus : **trois des six briques portent déjà
« ⏳ Reste : câblage éditeur »** (M3.1, M3.2, et le « câblage éditeur » de la note
de milestone). Le helper de visualisation existe pourtant — `NKRenderer/Tools/
Animation/NkPoseDebugDraw.{h,cpp}` dessine COM, polygone de support, fil d'aplomb
et direction de bascule — et sa **validation visuelle est toujours en attente**.

**1 272 lignes de physique fondée, testées headless, que l'éditeur n'affiche pas.**
Exposer l'existant coûte moins cher que d'écrire M3.7 : c'est du câblage, pas de
la recherche, et ça rend visible ce qui est déjà payé.

### Méthode — et deux fois où l'instrument a menti

*Périmètre : `Kernel/`, `Engine/`, `Applications/`, hors `Externals/`. Comptage des
lignes par `wc -l`, corps vérifiés fichier par fichier.*

Deux pièges rencontrés le même jour, tous deux réglés par le réflexe *un compte
trop rond est suspect* :

1. **`grep -E` avec `\|`.** En ERE, l'alternation s'écrit `|` ; `\|` cherche une
   barre littérale. Six recherches ont rendu **zéro partout** avant correction.
2. **Chercher en anglais dans un dépôt commenté en français.** `SupportPolygon`
   rendait 0 alors que `NkBalance` **implémente** le polygone de sustentation : le
   code écrit « POLYGONE DE SUPPORT », avec `supportPts` / `supportCount`. Sans ce
   doute, une brique de 250 lignes était déclarée absente.

## 📏 CONVENTION — nommage des joints des rigs destinés à NkAnima (règle de Rodolf, 2026-08-17)

**Les rigs destinés à NkAnima nomment leurs joints.** La distribution de masse
anthropométrique (`NkPoseMass::SetAnthropometric`) et le verdict d'équilibre de
l'éditeur reposent sur les **noms** des joints — champ `"name"` des nodes glTF,
transporté par `NkGLTFNode.name` → `NkAnimationClip.jointNames` depuis le
2026-08-17.

**La liste ci-dessous EST la convention** : c'est exactement ce que le code
reconnaît (`MassWeightForName`, `NkPoseMass.cpp` ; correspondance en
**sous-chaîne insensible à la casse**, donc `mixamorig:LeftFoot` matche `foot`).
Testés du plus spécifique au plus général — l'ordre compte (`upperarm` avant
`arm`, `foot` avant `leg`) :

| famille | motifs reconnus | poids |
|---|---|---:|
| main | `hand` · `wrist` | 1.0 |
| avant-bras | `forearm` · `lowerarm` · `elbow` | 2.0 |
| bras | `upperarm` · `shoulder` · `clavicle` · `arm` | 3.0 |
| **pied (= APPUI)** | **`foot` · `ankle` · `toe`** | 1.5 |
| tibia | `shin` · `calf` · `lowerleg` · `knee` | 5.0 |
| cuisse | `thigh` · `upperleg` · `upleg` | 10.0 |
| tête/cou | `head` · `skull` · `neck` | 8.0 |
| bassin | `hip` · `pelvis` · `root` | 15.0 |
| tronc | `spine` · `chest` · `thorax` · `abdomen` · `torso` · `trunk` | 12.0 |
| jambe générique | `leg` (après thigh/shin/foot) | 6.0 |
| non reconnu | — (masse résiduelle) | 2.0 |

⚠️ **Le verdict d'équilibre (sphère verte/rouge) ne s'active que sur pieds
nommés** — motifs `foot`/`ankle`/`toe`, ce sont eux qui définissent les appuis
(`NomEvoquePied`, `AnimBridge.cpp`, mêmes mots-clés que la famille « pied »).
Sans eux, l'étiquette dit honnêtement « équilibre indéterminé : aucun appui » —
c'est le comportement voulu, pas un défaut. Style Mixamo (`LeftFoot`/`RightFoot`)
accepté d'office par la règle de sous-chaîne.

**Mesuré sur les assets du dépôt** : ni `CesiumMan.glb` ni `BrainStem.glb` n'ont
de pied nommé (CesiumMan : `leg_joint_R_5`/`L_5`) — masse anthropométrique OK,
verdict indéterminé. Un mannequin Mixamo (pieds nommés) est attendu dans
`Resources/Models/` pour allumer le verdict pour la première fois.

⚠️ **Format : glTF (`.glb`) UNIQUEMENT pour l'animation.** Mesuré le 2026-08-17 :
`LoadFBX` ne remplit ni `nodes`, ni `skinJoints`, ni `animations`, ni `isSkinned`
(zéro occurrence de `Deformer` dans le parseur — géométrie et matériaux
seulement), et `AnimInit` appelle `LoadGLTF` en dur. **Un FBX Mixamo doit être
converti en `.glb`** (Blender → export glTF 2.0) avant dépôt.

## Documents voisins — lire avant d'écrire ici

| Document | Ce qu'il porte, et que ce fichier ne reprend pas |
|---|---|
| `Applications/NkAnima/ROADMAP.md` | jalons M0→M5, historique des décisions, IK, physique de pose, IA de direction |
| `Kernel/Runtime/NKAnimation/NKAnimation.jenga` | frontière avec NKRenderer, placement de `NkAnimationEditor`, dépendances déclarées |
| `Kernel/Runtime/NKGraph/ROADMAP.md` | le substrat de graphe, pour l'édition visuelle |
| `Kernel/Runtime/NKAnimPhysics/` | module frère — **il n'a pas de ROADMAP non plus**, dette nommée ici et non traitée |

---

*Créé le 2026-08-17. Le module n'en avait pas ; personne ne pouvait dire ce qui
restait à faire.*
