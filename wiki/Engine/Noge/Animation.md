# L'animation dans Noge

> Couche **Engine** · Noge · La famille **Animation** : faire bouger des personnages et des
> objets — locomotion bipède, sprites 2D, *tweening*, IK, séquenceur cinématique, foules et
> rig facial FACS. Tout repose sur l'ECS de Noge (composants + systèmes).

Un moteur sait afficher un maillage immobile ; le rendre **vivant** est un autre métier. Faire
marcher un personnage sans glissade des pieds, jouer une animation de sprite, faire monter une
barre de vie en douceur, plier un bras pour qu'une main attrape une poignée, enchaîner des plans
de caméra pour une cinématique, animer une foule de mille agents, ou faire sourire un visage avec
les bons muscles : ce sont **sept problèmes distincts**, et cette famille leur donne sept réponses.
Le fil rouge est toujours le même — un **composant** (NK_COMPONENT) porte la donnée sur l'entité,
un **système** (NkSystem) la fait évoluer chaque frame à une **priorité** précise dans la passe
`Update` ou `PostUpdate`. Cette page vous apprend à choisir le bon outil et à comprendre ce que
chacun calcule.

Avant tout, une **mise en garde de statut**. Ces fichiers sont des *headers d'API* de la couche
Engine. Les méthodes **inline** (corps présent dans le header) sont garanties — `AddTrack`,
`SetAU`, l'API fluide de `NkTween`, les `Play()`/`Stop()`. Les méthodes **déclarées sans corps**
(`Execute` des systèmes, `SolveFABRIK`, `Evaluate`, `LoadFromFile`, `BlendToExpression`,
`NkExpression::Joy()`…) dépendent d'un `.cpp` non visible et, pour les sous-systèmes Anim/Facial,
les ROADMAPs Nkentseu signalent que **plusieurs sont encore des specs non implémentées**. Cette
page le signale au cas par cas : lisez « spec / impl externe » comme « ne comptez pas dessus tant
que le `.cpp` n'existe pas ».

- **Namespace** : `nkentseu` (les types `math::`, `ecs::`, `renderer::` sont accessibles sans
  qualification dans ces headers via `using namespace`)
- **Headers réels** : `Noge/Anim/NkLocomotion.h`, `Noge/Crowd/NkCrowdSim.h`,
  `Noge/Anim2D/NkAtlas2D.h`, `Noge/Anim2D/NkTween.h`, `Noge/Rigging/NkIKSolver.h`,
  `Noge/Sequencer/NkSequencer.h`, `Noge/Facial/NkFacialRig.h`, `Noge/Facial/NkSkinMaterial.h`

---

## La locomotion : faire marcher un personnage

Déplacer une entité, c'est facile — `position += vitesse * dt`. La faire **marcher de façon
crédible** ne l'est pas : il faut choisir la bonne animation selon la vitesse, orienter le corps
vers la direction du mouvement, et surtout **coller les pieds au sol** même quand le terrain est en
pente ou irrégulier. La famille locomotion (`Noge/Anim/NkLocomotion.h`) découpe ce problème en
composants spécialisés.

`NkLocomotion` est l'**état de mouvement** : ce que le personnage *veut* faire (`desiredVelocity`,
`desiredSpeed`, `desiredTurnRate`) et ce qu'il fait *réellement* (`velocity`, `speed`, `facing`),
plus des drapeaux d'état (`isGrounded`, `isSprinting`, `isCrouching`, `isAiming`) et une posture
`Stance` (`Stand`, `Crouch`, `Prone`, `Swim`, `Climb`). Les paramètres `walkSpeed`/`runSpeed`/
`sprintSpeed` et les temps d'accélération règlent la dynamique. La pièce maîtresse est la table de
**vitesses → clip d'animation** : un tableau borné (`kMaxSpeedEntries = 8`) qu'on remplit avec la
méthode **inline** `AddSpeedEntry(speed, clipHandle)` — la seule méthode garantie ici, elle renvoie
`false` si la table est pleine. Le système choisira le clip dont la vitesse colle le mieux à
`speed`.

Ce n'est **pas** un contrôleur d'input : `NkLocomotion` ne lit pas le clavier, il consomme une
`desiredVelocity` que *vous* alimentez. Et ce n'est **pas** non plus la physique : il pose la
vitesse, pas les forces.

`NkFootIK` est le correcteur de **pieds au sol** : il connaît les indices d'os de la chaîne jambe
(hanche, cuisses, mollets, pieds, orteils, gauche et droite), tire des rayons vers le sol
(`rayLength`, `raycastLayerMask`) et calcule deux `NkFootContact` (position, normale, distance,
`contactWeight` lissé) plus un `hipOffset` pour abaisser le bassin quand un pied est plus bas que
l'autre. Les paramètres `footHeight`, `ikWeight`, `hipCompensation`, `maxFootAngle`, `blendSpeed`
dosent la correction.

`NkMotionMatch` est l'approche **Motion Matching** : au lieu d'une machine à états, on cherche en
continu, dans une base de poses (`NkMotionDatabase`), celle dont les *features* (`NkMotionFeature`
— positions/vitesses des pieds et du bassin, trajectoire prédite) collent le mieux à la requête
courante, et on s'y rend par fondu. C'est l'animation « data-driven » des jeux AAA.

> **En résumé.** `NkLocomotion` = état du mouvement + table vitesse→clip (`AddSpeedEntry`, inline).
> `NkFootIK` = pieds collés au sol par raycast + IK. `NkMotionMatch` = sélection continue de pose
> par plus-proche-voisin. Vous alimentez `desiredVelocity` ; les systèmes font le reste — quand
> leur `.cpp` existe.

---

## L'animation 2D : sprites et tweening

Deux besoins très différents partagent le dossier `Anim2D`. D'abord l'**animation par frames**
d'un sprite (un personnage de jeu 2D qui marche), via `NkAtlas2D.h`. Ensuite l'**interpolation de
propriétés** dans le temps — une fenêtre qui glisse, une barre qui se remplit, une couleur qui
pulse — via `NkTween.h`, dans l'esprit de DOTween ou GSAP.

Un **atlas** (`NkAtlas2D`) range plein de petits sprites dans une seule texture pour les dessiner
efficacement. Chaque `NkSpriteFrame` décrit un rectangle UV normalisé (`uvRect`), un pivot, une
taille en pixels et d'éventuelles infos de *trim*. Une `NkAnimation2D` est une suite d'indices de
frames jouée à un `fps` donné, en boucle ou non. On charge l'atlas depuis un JSON TexturePacker
(`LoadFromJSON`) ou un XML LibGDX (`LoadFromXML`), puis on retrouve frames et animations par nom.
Sur une entité, `NkAtlas2DComponent` joue l'animation courante (`Play`, `Update`) — attention,
l'atlas y est un **pointeur non-owned** partagé, dont vous gérez la durée de vie. Les contrôles
`Stop()`/`Pause()`/`Resume()` sont inline ; **piège**, `Stop()` et `Pause()` font exactement la
même chose (ils ne remettent ni la frame ni le timer à zéro).

Le **tweening** répond à une autre question : « anime cette valeur de A vers B en N secondes, avec
telle courbe ». `NkTween` est la base ; les types concrets `NkTweenFloat`, `NkTweenVec3`,
`NkTweenColor` interpolent respectivement un `float`, un `NkVec3f`, un `NkVec4f`. Tout passe par le
singleton `NkTweenManager::Get()` et une **API fluide** : on demande un tween avec `To`/`From`, on
le configure en chaîne (`SetEase`, `SetDelay`, `SetLoops`, `SetPingPong`, `OnComplete`…), et on
appelle `manager.Update(dt)` chaque frame. La courbe se choisit dans l'enum `NkEase` (Linear plus
les familles Sine/Quad/Cubic/Quart/Quint/Expo/Circ/Back/Elastic/Bounce en In/Out/InOut, plus
`Custom`).

Le tween n'est **pas** réservé à la 2D : il anime n'importe quelle valeur en mémoire — une position
3D, une couleur de lumière, un paramètre de matériau, un volume audio. C'est un outil
d'**interpolation générique**, pas un système d'animation de personnage.

> **En résumé.** `NkAtlas2D` = sprites en planche + animations par frames (chargées TexturePacker/
> LibGDX). `NkTween` = interpoler **toute** valeur de A à B avec une courbe `NkEase`, via le
> singleton `NkTweenManager::Get().To(...).SetEase(...)`. Ne confondez pas : frames discrètes
> vs interpolation continue.

---

## La cinématique inverse : plier les membres

Animer « vers l'avant » (FK), c'est tourner chaque articulation et regarder où la main arrive.
L'**IK** fait l'inverse : on dit *où la main doit aller*, et le solveur trouve les angles. C'est
indispensable pour qu'une main attrape une poignée précise, qu'un pied se pose sur une marche,
qu'une tête suive une cible des yeux. `NkIKSolver.h` réunit plusieurs solveurs et un système de
contraintes.

`NkIKSolver` propose quatre méthodes selon la chaîne. **FABRIK** et **CCD** résolvent une chaîne
d'os arbitraire (`Chain` : liste d'os root→tip, cible, itérations, tolérance, contraintes d'angle
optionnelles) de façon itérative — FABRIK est rapide et stable, CCD plus simple. **TwoBone** est le
cas analytique d'une chaîne à deux os (bras, jambe) : résolution exacte `O(1)` avec un *pole
target* pour orienter le coude/genou et une option d'étirement. **Spline** plie une chaîne le long
d'une courbe (queue, colonne). Tous ces solveurs sont **déclarés sans corps** : ils supposent un
`.cpp` non visible.

`NkConstraint` (composant) est la version « contrainte permanente » à la Blender : un type
(`NkConstraintType` — `LookAt`, `CopyRotation`, `TrackTo`, `LimitRotation`, `Floor`, `ChildOf`…),
une entité cible, une influence, et un bloc `cfg` réglant les axes actifs, l'inversion, les bornes
et les axes de visée. Le `NkConstraintSystem` les résout en `PostUpdate` à la **priorité 500**
(après le système de transform, comme attendu).

> **En résumé.** `NkIKSolver` = FABRIK/CCD (chaîne quelconque), TwoBone (bras/jambe analytique avec
> pole), Spline (courbe). `NkConstraint` + `NkConstraintSystem` (prio 500) = contraintes
> persistantes type Blender. Les solveurs eux-mêmes sont impl externe (spec).

---

## Le séquenceur : orchestrer une cinématique

Quand il faut **mettre en scène** — un dialogue, un plan de caméra, une porte qui s'ouvre pile au
bon moment —, on ne code plus image par image : on pose une **timeline multi-piste**, façon Unreal
Sequencer ou les éditeurs vidéo. `NkSequencer.h` en fournit le modèle complet, en **frames** comme
unité de temps.

Une `NkSequence` est le conteneur racine : un ensemble de `NkTrack` (chacune ciblant une entité et
typée — Animation, Transform, Audio, Camera, Event, FacialAnim…), une piste caméra dédiée
(`NkCameraTrack` de `NkCameraShot` avec coupes/fondus `NkCutType`), des pistes NLA
(`NkNLATrack`/`NkNLAClip` pour mixer des clips en Replace/Add/Multiply), des marqueurs
(`NkMarker`), et une config de rendu hors-ligne (`NkRenderOutput` : résolution, fps, format
PNG/EXR/JPEG/WebP, motion blur, DOF…). Une piste contient des **clips** (`NkClipOnTrack`) et des
**canaux de courbes** (`NkAnimChannel` : suite de `NkKeyframe` interpolées en
Constant/Linear/Bezier/Ease…). La lecture se pilote avec `NkPlaybackCtrl` (temps, vitesse —
négative pour le rebours —, boucle, `Play`/`Pause`/`Stop`/`JumpTo`).

La construction est entièrement **inline** et confortable : `AddTrack`, `AddClip`, `AddChannel`,
`AddShot`, `AddMarker`, `AddNLATrack` poussent un élément et renvoient une référence dessus, qu'on
configure dans la foulée. En revanche, le **cœur de l'évaluation** — tous les `Evaluate`,
`GetActiveCameraAt`, `RecalcDuration`, `SaveToFile`/`LoadFromFile` — est **impl externe**.

> **En résumé.** `NkSequence` = timeline multi-piste (anim/transform/caméra/audio/event/facial),
> pistes NLA, marqueurs, sortie de rendu. Construction inline (`AddTrack`/`AddClip`/`AddShot`/
> `AddMarker`), lecture par `NkPlaybackCtrl`. L'`Evaluate` est encore une spec.

---

## La foule : animer mille agents

Animer un personnage est une chose ; en animer **mille** sans faire fondre le CPU en est une autre.
La famille foule sépare deux responsabilités. Le **composant et le système d'agent** vivent en fait
dans `NkLocomotion.h` (et non dans `NkCrowdSim.h`) : `NkCrowdAgent` porte la navigation
(destination, vitesse, rayon), le *steering* à la Reynolds (séparation, alignement, cohésion, avec
leurs rayons et forces), un lien vers une entité d'animation, et un niveau de détail
`LOD` (`Full`/`Medium`/`Low`/`Culled`) pour alléger les agents lointains. `NkCrowdSystem`
(`Update`, priorité 150) met à jour steering et LOD.

`NkCrowdSim.h` ne contient que l'**infrastructure spatiale** : `NkCrowdGrid` (grille de cellules
pour trouver les voisins d'un agent sans tester tout le monde — `Init`, `Insert`, `QueryNeighbors`)
et `NkCrowdManager` (orchestration : `Update`, `RebuildGrid`, plafond `maxAgents = 10000`). **Piège
à connaître** : ce header utilise `std::pair` dans la grille et nomme des types ECS sans
qualification alors que `ecs` n'y est pas importé — signes d'un **header de spec non compilé**, à
corriger avant usage.

> **En résumé.** Agent = `NkCrowdAgent` + `NkCrowdSystem` (prio 150), **dans NkLocomotion.h**.
> `NkCrowdSim.h` = juste la grille spatiale (`NkCrowdGrid`) et le manager — et c'est une spec non
> compilable en l'état (`std::pair`, ECS non qualifié).

---

## Le visage : rig facial FACS

Faire parler et exprimer un visage est le sommet de l'animation de personnage. Nkentseu s'appuie
sur le **FACS** d'Ekman & Friesen : décomposer toute expression en **Action Units** (mouvements
musculaires élémentaires). `NkFacialRig.h` modélise cela ; `NkSkinMaterial.h` rend la peau, les
yeux et les dents de façon réaliste.

L'enum `NkFACS` énumère 44 unités (sourcils, paupières, joues, nez, lèvres, mâchoire…), des
**visèmes** pour le *lip-sync* (`VIS_Aa`, `VIS_Ee`, `VIS_Mm`…) et des AU procédurales (regard,
rotation de tête, respiration). **Piège majeur** : indexez toujours par
`static_cast<uint32>(au)`, jamais par le numéro AU du nom — il y a des doublons gauche/droite
(`AU46_WinkL`/`AU46_WinkR`, `AU12_..L`/`AU12_..R`) et des numéros non séquentiels. `FACS_COUNT`
sert à la fois de taille de tableau et de sentinelle « aucune AU ».

`NkFacialRig` (composant) porte un tableau de poids d'AU `[0..1]` (la source de vérité), des
mappings AU→blend shape (`NkAUMapping`) et des expressions nommées (`NkExpression`). On manipule
les AU avec des méthodes **inline** sûres : `SetAU` (clampe et borne), `GetAU`, `ResetAUs`,
`RetargetFrom`, `AddExpression`, `AddMapping`. Le mélange d'expressions (`BlendToExpression`,
`BlendExpressions`) et l'application au maillage (`ApplyToMesh`) sont impl externe — et
`BlendToExpression` est **additif**, pensez à `ResetAUs()` avant pour un blend exclusif.

Autour gravitent : `NkMicroExpressionDriver` (micro-expressions involontaires procédurales selon
une émotion sous-jacente), `NkLipSync` (synchronisation labiale à partir de visèmes chargés depuis
un fichier `.phoneme`), `NkWrinkleMap` (rides dynamiques calculées en compute), et le
`NkFacialSystem` (`PostUpdate`, priorité 800 — le dernier avant le rendu). Côté matériaux,
`NkSkinMaterial` (SSS multi-couches, translucidité, pores, rides), `NkEyeRig` + `NkEyeSystem`
(clignement, saccades, dilatation pupillaire, regard) et `NkTeethMaterial` complètent le réalisme.

> **En résumé.** `NkFACS` (Action Units + visèmes) est la base ; `NkFacialRig` stocke les poids
> d'AU (manipulés inline par `SetAU`/`GetAU`). `BlendToExpression` est additif. `NkEyeRig`,
> `NkLipSync`, `NkMicroExpressionDriver`, `NkSkinMaterial` peaufinent peau/yeux/dents. Beaucoup de
> méthodes (`Joy()`, `ApplyToMesh`…) sont des specs.

---

## Aperçu de l'API

Tous les composants marqués sont enregistrés via `NK_COMPONENT(...)`. Les systèmes ECS héritent de
`NkSystem` ; leur `Describe()` (groupe, lectures/écritures, priorité, nom) est inline, leur
`Execute()` est impl externe. `[inline]` = corps garanti dans le header ; `[spec]` = impl externe
(non visible / potentiellement non implémentée).

### Locomotion — `Noge/Anim/NkLocomotion.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Donnée | `NkFootContact` | Contact pied/sol (position, normale, distance, poids lissé) |
| Composant | `NkFootIK` | Correction IK des pieds (indices d'os, raycast, hip offset) |
| Composant | `NkLocomotion` | État de mouvement + posture `Stance` + table vitesse→clip |
| Méthode | `NkLocomotion::AddSpeedEntry` `[inline]` | Ajoute une entrée vitesse→clip (max 8) |
| Donnée | `NkMotionFeature`, `NkMotionDatabase` | Features et base Motion Matching |
| Composant | `NkMotionMatch` | État du Motion Matching |
| Composant | `NkCrowdAgent` | Agent de foule (nav + steering + `LOD`) |
| Système | `NkLocomotionSystem` `[Update/200]` | Vitesse, orientation, choix de clip |
| Système | `NkFootIKSystem` `[PostUpdate/350]` | Pose les pieds au sol |
| Système | `NkMotionMatchSystem` `[PostUpdate/300]` | Sélection continue de pose |
| Système | `NkCrowdSystem` `[Update/150]` | Steering + LOD des agents |

### Foule (infrastructure) — `Noge/Crowd/NkCrowdSim.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Classe | `NkCrowdGrid` `[spec]` | Grille spatiale de voisinage (`Init`/`Insert`/`QueryNeighbors`) |
| Classe | `NkCrowdManager` `[spec]` | Orchestration foule (`Update`/`RebuildGrid`, `maxAgents`) |

### Anim 2D — `Noge/Anim2D/NkAtlas2D.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Donnée | `NkSpriteFrame` | Un sprite dans l'atlas (uvRect, pivot, taille, trim) |
| Donnée | `NkAnimation2D` | Suite de frames + fps + boucle |
| Classe | `NkAtlas2D` | Atlas (charge JSON/XML, `GetFrame`/`GetAnimation`, `FrameCount`/`AnimCount` inline) |
| Composant | `NkAtlas2DComponent` | Joue une anim sur une entité (`Play`/`Update` spec ; `Stop`/`Pause`/`Resume` inline) |

### Tween — `Noge/Anim2D/NkTween.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkEase` | Courbe d'interpolation (Linear, Sine/Quad/.../Bounce In/Out/InOut, Custom) |
| Fonction | `NkEaseEval(ease, t)` `[spec]` | Évalue une courbe, t∈[0..1] |
| Classe | `NkTween` | Base abstraite + API fluide inline + état inline |
| Classes | `NkTweenFloat`/`NkTweenVec3`/`NkTweenColor` | Tweens typés (inline) |
| Classe | `NkTweenSequence` | Enchaîner/paralléliser des tweens (`Append`/`Join`/`AppendInterval`) |
| Singleton | `NkTweenManager` | `Get()` inline, `To`/`From`/`Sequence`, `Update`, `KillAll`… |

### Rigging / IK — `Noge/Rigging/NkIKSolver.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Donnée | `NkIKConstraint` | Bornes d'angle X/Y/Z d'un os |
| Classe | `NkIKSolver` | `SolveFABRIK`/`SolveCCD`/`SolveTwoBone`/`SolveSpline` `[spec]` + `Chain`/`TwoBoneChain`/`SplineChain` |
| Enum | `NkConstraintType` | LookAt, CopyRotation, TrackTo, Floor, ChildOf… |
| Composant | `NkConstraint` | Contrainte persistante (type, cible, influence, `cfg`) |
| Système | `NkConstraintSystem` `[PostUpdate/500]` | Résout les contraintes après le transform |

### Séquenceur — `Noge/Sequencer/NkSequencer.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enums | `NkTrackType`, `NkInterpolation`, `NkCutType` | Type de piste, interpolation de clé, type de coupe |
| Donnée | `NkKeyframe`, `NkAnimChannel` | Clé de courbe et canal de propriété |
| Donnée | `NkClipOnTrack`, `NkTrack` | Clip et piste (constructions inline `AddClip`/`AddChannel`) |
| Donnée | `NkCameraShot`, `NkCameraTrack` | Plan et piste caméra (`AddShot` inline) |
| Donnée | `NkMarker`, `NkNLAClip`, `NkNLATrack` | Marqueur, clip NLA, piste NLA |
| Donnée | `NkRenderOutput` | Config de rendu hors-ligne (format, fps, effets) |
| Donnée | `NkPlaybackCtrl` | Lecture (temps, vitesse, boucle ; `Play`/`Stop`… inline) |
| Classe | `NkSequence` | Conteneur racine (constructions inline ; `Evaluate`/`Save`/`Load` spec) |

### Facial — `Noge/Facial/NkFacialRig.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkFACS` | 44 Action Units + visèmes + AU procédurales ; `kFACSCount` |
| Donnée | `NkAUMapping`, `NkExpression` | Mapping AU→blend shape ; expression nommée (statics `Joy()`… spec) |
| Composant | `NkFacialRig` | Poids d'AU + mappings + expressions (`SetAU`/`GetAU`/`ResetAUs`… inline) |
| Composant | `NkMicroExpressionDriver` | Micro-expressions involontaires procédurales |
| Composant | `NkLipSync` | Synchronisation labiale par visèmes (`Play`/`Stop` inline) |
| Composant | `NkWrinkleMap` | Rides dynamiques (compute) |
| Composant | `NkProcFaceParams` | Paramètres de génération procédurale de visage `[spec]` |
| Système | `NkFacialSystem` `[PostUpdate/800]` | Applique rig + micro-expr + lipsync au maillage |

### Matériaux organiques — `Noge/Facial/NkSkinMaterial.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Composant | `NkSkinMaterial` | Peau : SSS multi-couches, translucidité, pores, rides |
| Composant | `NkSkinRegion` | Régions de peau distinctes (`AddRegion` inline) |
| Composant | `NkEyeRig` | Œil : iris, cornée, pupille, regard |
| Système | `NkEyeSystem` `[PostUpdate/780]` | Clignement, saccades, pupille, regard |
| Composant | `NkTeethMaterial` | Dents : couleur ivoire, translucidité, salive |

---

## Référence complète

Chaque élément est repris ici en détail, avec ses cas d'usage à travers les domaines du moteur. Le
statut **inline** (garanti) ou **spec / impl externe** (dépend d'un `.cpp` non visible) est signalé
systématiquement.

### `NkFootContact` et `NkFootIK` — coller les pieds au sol

`NkFootContact` est un POD décrivant le contact d'un pied : `groundPos`, `groundNormal` (par défaut
`{0,1,0}`, le sol horizontal), `groundDist`, `isGrounded`, et un `contactWeight` lissé dans `[0..1]`
pour fondre l'IK plutôt que de le claquer d'un coup. `NkFootIK` (composant) en détient deux
(`leftFoot`, `rightFoot`) plus un `hipOffset`.

`NkFootIK` connaît la chaîne complète des deux jambes par **indices d'os** (`hipBoneIdx`,
`leftThighIdx`, `leftCalfIdx`, `leftFootIdx`, `leftToeIdx`, et les équivalents droits). Il tire un
rayon de longueur `rayLength` filtré par `raycastLayerMask`, ajuste de `footHeight`, limite l'angle
d'inclinaison du pied à `maxFootAngle`, dose la correction par `ikWeight` et la compensation du
bassin par `hipCompensation`, et lisse les transitions à `blendSpeed`.

- **Animation / gameplay** : un personnage qui marche sur un escalier ou une pente garde ses pieds
  posés, sans glissade ni pied dans le vide — l'effet « pied collé » des jeux modernes.
- **Rendu / mise en scène** : crédibilise un acteur dans une cinématique sur terrain irrégulier.
- **Physique** : les `NkFootContact` (normale, distance) peuvent informer un retour de force ou un
  alignement du corps sur la pente.

### `NkLocomotion` — l'état du mouvement

POD séparant l'**intention** (`desiredVelocity`, `desiredSpeed`, `desiredTurnRate` en rad/s,
`trajectoryDir[4]` pour la trajectoire prévue) de l'**état réel** (`velocity`, `speed`, `facing`
par défaut `{0,0,1}`, et les drapeaux `isGrounded`/`isSprinting`/`isCrouching`/`isAiming`). La
posture est l'enum imbriqué `Stance` (`Stand`, `Crouch`, `Prone`, `Swim`, `Climb`, accès
`NkLocomotion::Stance::Crouch`). Les paramètres `walkSpeed`/`runSpeed`/`sprintSpeed`,
`accelerationTime`/`decelerationTime`, `turnSpeed` (°/s) règlent la dynamique.

La table vitesse→clip associe une vitesse à un *handle* de clip d'animation (struct imbriquée
`SpeedEntry { speed; animClipHandle }`), bornée par `kMaxSpeedEntries = 8`. On la remplit avec la
seule méthode **inline** garantie du fichier :

```cpp
NkLocomotion loco;
loco.AddSpeedEntry(1.5f, walkClip);   // marche
loco.AddSpeedEntry(5.5f, runClip);    // course
loco.AddSpeedEntry(10.f, sprintClip); // sprint ; renvoie false si la table (8) est pleine
```

- **Gameplay / IA** : `desiredVelocity` est le pont entre votre contrôleur (joueur ou IA de
  pathfinding) et l'animation — l'IA pose une vitesse, le système choisit la bonne anim.
- **Animation** : la table vitesse→clip est le cœur d'un *blend tree* de locomotion 1D.

### `NkMotionFeature`, `NkMotionDatabase`, `NkMotionMatch` — Motion Matching

`NkMotionFeature` est le **vecteur de features** d'une pose : positions et vitesses des pieds et du
bassin, trajectoire prédite (`trajPos0/1/2` à 0,2/0,4/0,6 s et leurs directions), plus des méta
(clip, temps). Sa méthode `Distance(other, weights)` mesure l'écart pondéré entre deux poses — elle
est **déclarée sans corps**. `NkMotionDatabase` est le `NkVector` de features avec ses poids
(`weightFootPos`, `weightFootVel`, `weightHipVel`, `weightTrajPos`, `weightTrajDir`) ; ses méthodes
`FindBestMatch` (plus-proche-voisin, K-D tree ou brute force), `BuildFromClips`,
`SaveToFile`/`LoadFromFile` sont **impl externe**. `NkMotionMatch` (composant) porte la base et
l'état de lecture (`queryRate` en Hz, `blendDuration`, index courant…).

- **Animation AAA** : remplace une machine à états de locomotion par une recherche continue dans une
  capture de mouvement — transitions naturelles, pas de *blend tree* à régler à la main.
- **Statut** : tout le cœur (`Distance`, `FindBestMatch`, `BuildFromClips`) est une spec.

### `NkCrowdAgent` — l'agent de foule (dans NkLocomotion.h)

À noter : le **composant principal de foule vit ici**, pas dans `NkCrowdSim.h`. `NkCrowdAgent`
porte la navigation (`destination`, `currentVelocity`, `maxSpeed`, `radius`, `height`), le
**steering** de Reynolds (rayons et forces de `separation`/`alignment`/`cohesion`), un lien vers
l'entité d'animation (`animatorEntity`, `animSpeedMult`), et un niveau de détail `LOD`
(`Full`/`Medium`/`Low`/`Culled`, accès `NkCrowdAgent::LOD::Medium`) pour alléger les agents
lointains. L'état (`steeringForce`, `hasPath`, `reached`, `pathNodeIdx`) est mis à jour par
`NkCrowdSystem`.

- **Gameplay / simulation** : foules de PNJ, hordes, troupeaux — chacun évite ses voisins et
  converge vers sa destination.
- **Rendu** : le `LOD` permet d'animer pleinement les agents proches et de dégrader les lointains.

### Les systèmes de locomotion

Quatre systèmes ECS, tous avec un `Describe()` **inline** (config réelle) et un `Execute()` **impl
externe** :

- **`NkLocomotionSystem`** — passe `Update`, **priorité 200**. Écrit `NkLocomotion`, `NkTransform`,
  `NkAnimator` : convertit l'intention en mouvement réel et choisit le clip.
- **`NkFootIKSystem`** — passe `PostUpdate`, **priorité 350**. Lit `NkTransform`, écrit `NkFootIK`
  et `NkSkeleton` ; embarque un `NkIKSolver` et un `RaycastGround` privé. Tourne après l'animation
  de base pour corriger la pose finale.
- **`NkMotionMatchSystem`** — passe `PostUpdate`, **priorité 300**. Écrit `NkMotionMatch` et
  `NkSkeleton`, lit `NkLocomotion` ; construit la requête via `BuildQuery`.
- **`NkCrowdSystem`** — passe `Update`, **priorité 150**. Écrit `NkCrowdAgent` et `NkTransform` ;
  met à jour steering et LOD.

### `NkCrowdGrid`, `NkCrowdManager` — l'infrastructure spatiale

`NkCrowdSim.h` ne contient que l'**accélération spatiale** de la foule. `NkCrowdGrid` partitionne le
monde en cellules (`Init(cellSize, worldBounds)`) pour répondre vite à « quels agents sont près de
ce point ? » (`Insert`, `QueryNeighbors`, `Clear`) sans tester tous les agents — sinon le steering
serait en `O(n²)`. `NkCrowdManager` orchestre (`Update`, `RebuildGrid`, `neighborRadius`,
`maxAgents = 10000`).

Toutes ces méthodes sont **déclarées sans corps**. **Piège** : la grille stocke ses cellules en
`std::pair` et nomme `NkEntityId`/`NkVec3f` sans qualification `ecs::` alors que le header
n'importe pas `ecs` — c'est un **header de spec non compilable** en l'état, à corriger (zéro-STL +
qualification) avant tout usage réel.

- **Simulation** : seul outil indispensable pour passer le steering de foule à l'échelle
  (des milliers d'agents).

### `NkSpriteFrame`, `NkAnimation2D`, `NkAtlas2D` — l'atlas 2D

`NkSpriteFrame` décrit un sprite dans la planche : `uvRect` normalisé `(x,y,w,h)`, `pivot`
(défaut centre), `sizePx`, plus `trimOffset`/`trimmed` pour les sprites rognés. `NkAnimation2D` est
une suite d'indices de frames (`frameIds`) jouée à `fps` (défaut 24) en boucle ou non.

`NkAtlas2D` charge depuis deux formats standard de l'industrie : `LoadFromJSON` (TexturePacker) et
`LoadFromXML` (LibGDX) — **impl externe**. On retrouve un sprite ou une anim par nom
(`GetFrame`/`GetAnimation`, impl externe, renvoient `nullptr` si absent) ; `FrameCount()` et
`AnimCount()` sont **inline**. La texture de l'atlas est désignée par `textureHandle` (de
`renderer`) et `texturePath`.

- **Rendu / gameplay 2D** : tous les sprites d'un personnage ou d'un tileset dans une seule texture
  → moins de changements d'état GPU, batching efficace.
- **UI** : icônes, curseurs animés.

### `NkAtlas2DComponent` — jouer une anim de sprite

Sur une entité, ce composant joue l'animation courante : `currentAnim`, `currentFrame`,
`frameTimer`, `playing`, `loop`, `speed`. **Le membre `atlas` est un pointeur non-owned** (partagé
entre entités) : à vous de garantir sa durée de vie. `GetCurrentFrame()`, `Play(animName)` et
`Update(dt)` sont **impl externe**. Les contrôles `Stop()`, `Pause()`, `Resume()` sont **inline**.

**Piège** : `Stop()` et `Pause()` ont le **même corps** (`playing = false`) — aucun ne remet
`currentFrame`/`frameTimer` à zéro. Ne comptez pas sur `Stop()` pour réinitialiser l'animation.

### `NkEase` et `NkEaseEval` — les courbes

`NkEase` énumère les courbes d'easing : `Linear`, puis les familles `Sine`, `Quad`, `Cubic`,
`Quart`, `Quint`, `Expo`, `Circ`, `Back`, `Elastic`, `Bounce` — chacune déclinée en `...In`,
`...Out`, `...InOut` (ex. `NkEase::CubicOut`, `NkEase::ElasticInOut`) — et enfin `Custom` (courbe de
Bézier). `NkEaseEval(ease, t)` évalue la courbe sur `t∈[0..1]` (**impl externe**).

- **UI / animation** : un `Back`/`Elastic`/`Bounce` donne le ressenti « organique » d'un menu qui
  rebondit ; `Sine`/`Cubic` une transition douce.

### `NkTween` — la base de l'interpolation

`NkTween` est la classe de base abstraite. Elle expose :

- Une **API fluide inline** qui renvoie `NkTween&` pour chaîner : `SetEase`, `SetDelay`,
  `SetLoops(n)` (-1 = infini), `SetPingPong`, `SetSpeed`, `SetRelative`, `OnStart`, `OnComplete`,
  `OnUpdate`. Les callbacks sont des `NkFunction<void()>` / `NkFunction<void(float32)>`.
- Un **contrôle** : `Play`/`Pause`/`Resume`/`Restart`/`Complete` sont **impl externe** ; `Kill()`
  est **inline**.
- Un **état inline** : `GetState`, `GetProgress` (`[0..1]`), `IsPlaying`, `IsComplete`, `IsKilled`.
  L'enum imbriqué `State` vaut `Idle`/`Waiting`/`Playing`/`Paused`/`Completed`/`Killed`.
- Deux **virtuelles pures** protégées : `ApplyValue(t)` et `SnapToEnd()`.

**Piège** : le destructeur est **non-virtuel** alors que la classe est polymorphe et que
`NkTweenManager` détient des `NkTween*` qu'il possède — détruire via le pointeur de base est un
**comportement indéfini potentiel**. À signaler.

### `NkTweenFloat`, `NkTweenVec3`, `NkTweenColor` — tweens typés

Trois implémentations concrètes **entièrement inline**, construites avec
`(target, from, to, duration)` : `NkTweenFloat` interpole un `float32*`, `NkTweenVec3` un
`NkVec3f*`, `NkTweenColor` un `NkVec4f*`. Chacune override `ApplyValue` (lerp composante par
composante, gardé par `if (mTarget)`) et `SnapToEnd` (pose la valeur finale).

**Piège** : `ApplyValue` fait un lerp **linéaire en `t` brut** — il **n'applique pas** la courbe
`NkEase`. L'easing est censé être appliqué en amont (via `NkEaseEval` dans `Update`, non visible
ici) ; n'attendez pas que `NkTweenFloat` courbe tout seul.

### `NkTweenSequence` — enchaîner des tweens

Hérite de `NkTween` et compose plusieurs tweens dans le temps : `Append(tw)` (après le précédent),
`Join(tw)` (en parallèle au précédent), `AppendInterval(t)` (pause). Ces méthodes et les override
`ApplyValue`/`SnapToEnd` sont **impl externe** ; le constructeur (qui met `mDuration = 0`) est
inline. En interne, une suite d'`Entry { tween; startTime }`.

- **UI / cinématique légère** : enchaîner « fade in, attendre, slide, fade out » sans gérer les
  timers à la main.

### `NkTweenManager` — le chef d'orchestre

Singleton (`Get()` **inline**, Meyers local-static) qui possède tous les tweens. Les **factory**
`To`/`From`/`Sequence` (impl externe) créent un tween et renvoient une référence dessus, prête à
chaîner :

```cpp
NkTweenManager::Get()
    .To(&pos, NkVec3f{100,200,0}, 1.f)
    .SetEase(NkEase::CubicOut)
    .SetDelay(0.5f)
    .OnComplete([]{ /* arrivé */ });
// chaque frame :
NkTweenManager::Get().Update(dt);
```

`KillAll`, `KillAllOnTarget(target)`, `PauseAll`, `ResumeAll` (impl externe) gèrent en masse ;
`ActiveCount()` est **inline**.

- **Tous domaines** : position 3D, couleur de lumière, paramètre de matériau/post-process, volume
  audio, valeur d'UI — le tween anime n'importe quelle adresse mémoire.

### `NkIKConstraint` et `NkIKSolver` — résoudre l'IK

`NkIKConstraint` borne les angles d'un os (`minAngleX`/`maxAngleX` et idem Y/Z, en degrés,
`enabled`). `NkIKSolver` est le solveur, avec trois structures de chaîne imbriquées :

- **`Chain`** (FABRIK/CCD) : `boneIndices` root→tip, `targetPosition`, `targetRotation`, `weight`,
  `maxIterations` (défaut 10), `tolerance`, `constrainTip`, et `constraints` optionnelles par os.
- **`TwoBoneChain`** : `rootBone`/`midBone`/`tipBone`, `target`, `poleTarget` (orientation du
  coude/genou, activé par `usePole`), option `stretchable`/`maxStretch`.
- **`SplineChain`** : `boneIndices`, `splinePoints`, `useYUp`.

Les quatre solveurs sont **impl externe** : `SolveFABRIK` (renvoie le nb d'itérations), `SolveCCD`
(idem), `SolveTwoBone` (analytique `O(1)`), `SolveSpline`. Les utilitaires `ChainLength` et
`IsReachable` sont aussi impl externe.

- **Animation / gameplay** : `SolveTwoBone` pour qu'une main attrape une poignée à une position
  précise (bras = 2 os), `SolveFABRIK` pour un tentacule ou une chaîne longue, `SolveSpline` pour
  une queue ou une colonne.
- **Rendu / mise en scène** : ajuster une pose capturée pour l'aligner sur le décor exact.

### `NkConstraintType`, `NkConstraint`, `NkConstraintSystem` — contraintes persistantes

L'enum `NkConstraintType` couvre les contraintes façon Blender : `CopyLocation`, `CopyRotation`,
`CopyScale`, `CopyTransform`, `LookAt`, `TrackTo`, `StretchTo`, `ClampTo`, `LimitLocation`,
`LimitRotation`, `LimitScale`, `Floor`, `ChildOf`. Le composant `NkConstraint` porte un `type`
(défaut `LookAt`), une `target` (entité), une `influence`, et un bloc `cfg` (axes `useX/Y/Z`,
`invert`, bornes `min[3]`/`max[3]`, `trackAxis`/`upAxis` codés 0..5). Le `NkConstraintSystem`
(`Describe()` **inline** : `PostUpdate`, **priorité 500**, après `NkTransformSystem`) les résout ;
son `Execute` est **impl externe**.

- **Rig / animation** : une tête qui suit une cible (`LookAt`/`TrackTo`), un objet maintenu sur un
  autre (`CopyTransform`/`ChildOf`), un os plaqué au sol (`Floor`).
- **Éditeur** : reproduire le système de contraintes d'un DCC dans la scène.

### Le séquenceur : enums

- **`NkTrackType`** : `Animation`, `Transform`, `Property`, `Camera`, `Audio`, `Event`,
  `FacialAnim`, `BlendShape`, `Light`, `PostProcess`, `Particle`, `Visibility`, `NLA`.
- **`NkInterpolation`** : `Constant`, `Linear`, `Bezier`, `Auto`, `EaseIn`, `EaseOut`, `EaseInOut`.
- **`NkCutType`** : `Cut`, `Blend`, `Wipe` (transitions entre plans caméra).

### `NkKeyframe`, `NkAnimChannel` — les courbes d'animation

`NkKeyframe` est une clé : `time`, `value`, tangentes `inTangent`/`outTangent`, mode
`interpolation` (défaut `Bezier`), `selected` (édition). `Evaluate(next, t)` interpole vers la clé
suivante (**impl externe**). `NkAnimChannel` est un canal sur une propriété nommée
(`propertyName`, ex. `"localPosition.x"`, `componentOffset`) : une suite de `keyframes` avec
drapeaux `locked`/`muted`/`selected`. Ses méthodes `AddKey`, `RemoveKey`, `Evaluate`, `SortByTime`,
`AutoSmooth` sont **impl externe**.

- **Animation / éditeur** : c'est la « courbe » qu'on dessine dans un *graph editor* — animer
  n'importe quelle propriété d'un composant dans le temps.

### `NkClipOnTrack`, `NkTrack` — clips et pistes

`NkClipOnTrack` place un clip d'animation sur une piste : `clipHandle`, `startTime`, `duration`,
`clipOffset`, `speed`, fondus `blendIn`/`blendOut`, `weight`, `loop`/`reverse`. Plusieurs aides
sont **inline** : `EndTime()`, `IsActive(t)`, `GetLocalTime(t)` (remappe le temps global en temps
local du clip) ; `GetWeight(t)` (avec fondu) est impl externe.

`NkTrack` est une piste : un `type`, une `entity` ciblée, un `name`, des drapeaux
`muted`/`locked`/`selected`, un `weight`, et deux collections — `clips` et `channels`. Les
constructions sont **inline** : `AddClip(handle, start, duration)` pousse et renvoie le clip,
`AddChannel(propName)` copie le nom (via `NkStrNCpy`) et renvoie le canal. `Evaluate(time, world)`
est impl externe.

### `NkCameraShot`, `NkCameraTrack` — la caméra cinématique

`NkCameraShot` est un plan : `cameraEntity`, `startTime`, `duration`, `cutType`, `blendDuration`
(`EndTime()` inline). `NkCameraTrack` aligne des `shots` ; `AddShot(cam, start, dur, cut)` est
**inline**, `GetActiveShot(t)` impl externe.

- **Rendu / mise en scène** : montage automatique des plans d'une cinématique — la séquence sait
  quelle caméra est active à chaque instant.

### `NkMarker`, `NkNLAClip`, `NkNLATrack` — marqueurs et NLA

`NkMarker` annote la timeline : `label`, `time`, `color`, un `Type` imbriqué
(`Scene`/`Audio`/`Event`/`Note`) et un `eventFunction` (pour `Type::Event`, déclencher du code à un
instant). `NkNLAClip` est un clip de **Non-Linear Animation** : `clipHandle`, timing, `influence`,
`repeat`/`repeatCount`, `reverse`/`muted`, et un mode `BlendType` (`Replace`/`Add`/`Multiply`) pour
**mixer** plusieurs animations (ex. ajouter une animation de respiration par-dessus une marche).
`NkNLATrack` regroupe ces clips sur une entité ; son `Evaluate` est impl externe.

- **Animation avancée** : superposer des couches d'animation (locomotion + additif de visée + idle)
  comme dans un *NLA editor* de DCC.
- **Event** : les marqueurs `Event` déclenchent des sons, des effets ou du gameplay à la frame près.

### `NkRenderOutput` — le rendu hors-ligne

Config d'export d'une séquence en film : `width`/`height` (défaut 1920×1080), `fps`, `startTime`/
`endTime` (0 = fin), `Format` imbriqué (`PNG`/`EXR`/`JPEG`/`WebP`), `jpegQuality`, `exrHDR`,
`outputDirectory`, `filePrefix` (défaut `"frame_"`), plus des options de qualité (`renderMotionBlur`
+ `motionBlurSamples`, `renderDOF`, `renderSSAO`, `renderShadows`).

- **Rendu / production** : sortir une cinématique en séquence d'images (EXR HDR pour le compositing,
  PNG pour une prévisualisation).

### `NkPlaybackCtrl` — piloter la lecture

État de lecture d'une séquence : `time`, `speed` (négatif = rebours), `playing`, `loop`,
`loopStart`/`loopEnd` (0 = fin). Les contrôles `Play`/`Pause`/`Stop` (remet à `loopStart`)/`Rewind`/
`JumpTo` sont **inline** ; `Update(dt, sequenceDuration)` (avance, gère les boucles, renvoie `true`
à la fin) est **impl externe**.

### `NkSequence` — le conteneur racine

Le tout : `name`, `fps`, `duration`, des `tracks`, des `nlaTracks`, une `cameraTrack`, des
`markers`, une `renderOutput`. La **construction est inline** et fluide :

```cpp
NkSequence seq;
NkTrack& t = seq.AddTrack(entity, NkTrackType::Transform, "Hero");
t.AddClip(walkClip, 0.f, 2.f);
seq.AddMarker(1.5f, "Footstep", NkMarker::Type::Event);
NkPlaybackCtrl ctrl;
ctrl.Play();
// chaque frame : ctrl.Update(dt, seq.duration); seq.Evaluate(ctrl.time, world);
```

Le **cœur** — `Evaluate(time, world)`, `GetActiveCameraAt`, `RecalcDuration`, `SaveToFile`/
`LoadFromFile` — est **impl externe** (spec).

- **Mise en scène / éditeur** : c'est le modèle de données d'un éditeur de cinématique complet.

### `NkFACS` — les Action Units

L'enum énumère, dans l'ordre de déclaration (la **valeur** de l'enum = cet ordre, pas le numéro
AU) : 44 Action Units du FACS (sourcils `AU01..AU04`, paupières `AU05`/`AU07`/`AU43`/`AU45`, clins
`AU46_WinkL`/`AU46_WinkR`, nez `AU09`/`AU38`/`AU39`, joues/lèvres/mâchoire jusqu'à `AU28`), puis les
**visèmes** de lip-sync (`VIS_Rest`, `VIS_Aa`, `VIS_Ee`, `VIS_Oh`, `VIS_Oo`, `VIS_Mm`, `VIS_Ff`,
`VIS_Th`, `VIS_Ss`, `VIS_Rr`, `VIS_Ww`, `VIS_Ch`), puis les **AU procédurales**
(`AU_PROC_EyeDirection`, `AU_PROC_HeadRotation`, `AU_PROC_BreathCycle`, `AU_PROC_MicroSaccade`), et
enfin `FACS_COUNT`. `kFACSCount = static_cast<uint32>(FACS_COUNT)` donne la taille des tableaux d'AU.

**Piège majeur** : les noms numérotés sont trompeurs — `AU46_WinkL` **et** `AU46_WinkR` sont deux
entrées distinctes (idem `AU14_..L/R`, `AU12_..L/R`, `AU15_..L/R`), et les numéros AU ne sont pas
séquentiels. **Indexez toujours par `static_cast<uint32>(au)`**, jamais par le numéro du nom.
`FACS_COUNT` sert aussi de sentinelle « aucune AU ».

### `NkAUMapping`, `NkExpression` — du muscle à la géométrie

`NkAUMapping` relie une AU à un **blend shape** du maillage : `au`, `blendShapeIdx`, `maxWeight`,
`offset`, `curve` (puissance de la courbe), plus un **modificateur** (`modifierAU`, défaut
`FACS_COUNT` = aucun, et `modifierGain`) pour qu'une AU module l'effet d'une autre.

`NkExpression` est une expression nommée : un `name` et un tableau de `weights[kFACSCount]` (un
poids par AU). Le constructeur `explicit NkExpression(const char*)` est **inline** — **piège** :
il utilise `std::strncpy`, ce qui contredit la politique zéro-STL (à signaler). Les **statics**
`Joy()`, `Sadness()`, `Anger()`, `Fear()`, `Disgust()`, `Surprise()`, `Contempt()`, `Neutral()`
renvoient des expressions prêtes à l'emploi — toutes **impl externe**.

### `NkFacialRig` — le rig du visage

Composant central. Sa **source de vérité** est `auWeights[kFACSCount]` (`[0..1]`). Il porte des
mappings (`mappings[256]`, `mappingCount`) et des expressions (`expressions[64]`, `expressionCount`),
un lien vers le maillage (`meshEntity`, `meshComponentOffset`), une `globalInfluence` et un
`symmetryBalance` (`[-1 gauche, 0 sym, 1 droit]`).

Les méthodes **inline** sûres : `SetAU(au, weight)` (clampe `[0..1]` via `NkClamp`, borné par
`kFACSCount`), `GetAU(au)` (0 hors borne), `ResetAUs()`, `RetargetFrom(source)` (copie tous les
poids — transférer une expression d'un visage à un autre), `AddExpression`/`AddMapping` (renvoient
`false` si pleins). Les **impl externe** : `BlendToExpression(name, weight)`,
`BlendExpressions(from, to, t)`, `ApplyToMesh(mesh)`, `SaveExpression`, `FindExpression`,
`AutoMapFromBlendShapeNames(mesh)` (mappe automatiquement des shapes nommés `"AU01"`, `"AU06_L"`,
`"VIS_Ee"`…).

**Piège** : `BlendToExpression` est **additif** (il ajoute par-dessus l'état courant). Pour un blend
**exclusif**, appelez `ResetAUs()` avant.

```cpp
NkFacialRig rig;
rig.ResetAUs();
rig.SetAU(NkFACS::AU12_LipCornerPullerL, 0.8f); // sourire (côté gauche)
rig.SetAU(NkFACS::AU12_LipCornerPullerR, 0.8f);
rig.SetAU(NkFACS::AU06_CheekRaiser, 0.6f);      // sourire « sincère » (Duchenne)
```

- **Animation / médical** : moteur du Patient Virtuel 3D Émotif (PV3DE) — exprimer des émotions par
  les muscles faciaux exacts.

### `NkMicroExpressionDriver`, `NkLipSync`, `NkWrinkleMap` — les drivers

`NkMicroExpressionDriver` (composant) génère des **micro-expressions involontaires** : `frequency`,
`intensity`, durées min/max, et une émotion sous-jacente (`baseJoy`/`baseFear`/`baseAnger`/
`baseDisgust`/`baseSadness`) qui biaise le tirage. `Update(dt, rig)` (impl externe) déclenche
brièvement quelques AU.

`NkLipSync` (composant) synchronise les lèvres : une suite de `PhonemeKey { time; viseme; weight }`
(jusqu'à `kMaxKeys = 4096`), chargée d'un fichier `.phoneme` (JSON, compatible rhubarb/odin) via
`LoadFromFile` (impl externe). `Update(dt, rig)` (impl externe) applique les visèmes ; `Play`/
`Stop`/`Pause`/`Resume` sont **inline**.

`NkWrinkleMap` (composant) calcule des **rides dynamiques** en compute : trois textures R8
(compression/stretch/fold), `resolution`, seuils et `intensity`. `Update(cmd, ...)` dispatch un
compute (< 0,3 ms, impl externe).

- **Animation faciale réaliste** : les micro-expressions trahissent l'émotion réelle ; le lip-sync
  fait parler ; les rides apparaissent dynamiquement avec les expressions.

### `NkProcFaceParams` — génération procédurale (spec)

POD d'environ trente paramètres `float32` `[0..1]` décrivant un visage : morphologie (`age`,
`weight`, `gender`, `ethnicity[4]`), tête, front, yeux, nez, bouche, oreilles, plus un `seed`. Il
**n'a aucune méthode** et **pas de `NK_COMPONENT`** : le générateur (qui produirait un maillage
éditable + un `NkFacialRig`) n'est pas présent dans ce header — c'est une **spec**.

### `NkFacialSystem` — appliquer le visage

`Describe()` **inline** : passe `PostUpdate`, **priorité 800** (après les contraintes et les yeux,
juste avant le rendu). Lit `NkTransform`, écrit `NkFacialRig`, `NkMicroExpressionDriver`,
`NkLipSync`, `NkSkinnedMeshComponent`. `Execute` est **impl externe**.

### `NkSkinMaterial` — la peau

POD pur (aucune méthode). Étend le système de matériaux NKRenderer **sans le modifier**. Textures
(`albedoTex`, `normalTex`, `roughnessTex` packée R=rough/G=wet/B=cavity, `sssColorTex`,
`wrinkleMaskTex`, `detailNormalTex`, `translucencyTex`) ; **SSS** (Subsurface Scattering) avec
`sssRadius` en mm, `sssColor`, `sssStrength` et trois couches `epidermis`/`dermis`/`hypodermus`
(somme = 1) ; surface (`roughness`, `wetness`, `oiliness`) ; micro-détails (`poreScale`,
`poreStrength`) ; translucidité ; rides (`wrinkleMapEntity`, `wrinkleStrength`) ; imperfections
(`veinStrength`/`veinColor`, `freckleStrength`).

- **Rendu** : la peau humaine crédible repose sur le SSS — la lumière qui pénètre et ressort
  (oreilles, ailes du nez à contre-jour). Indispensable pour PV3DE.

### `NkSkinRegion` — des zones de peau distinctes

La peau n'est pas uniforme : le front est plus gras que les joues. `NkSkinRegion` définit jusqu'à
`kMaxRegions = 8` zones (`Region` : `name`, `sssRadius`, `sssStrength`, `roughness`, `wetness`,
`sssColor`) sélectionnées par `regionMaskTex`. `AddRegion(name, sssR, sssStr)` est **inline** —
**piège** : elle ne renseigne **que** `name`/`sssRadius`/`sssStrength`, pas `roughness`/`wetness`/
`sssColor` (à régler à la main ensuite). Renvoie `false` si pleine.

### `NkEyeRig`, `NkEyeSystem` — les yeux

`NkEyeRig` (composant, POD) décrit un œil : un `Side` (`Left`/`Right`), les entités des couches
(`scleraEntity`/`irisEntity`/`corneaEntity`/`pupilEntity`), l'os de l'œil ; le visuel (`irisColor`,
`scleraColor`, `irisRadius`, `limbusWidth`) ; la **cornée** (`corneaRefraction` = 1.336 par défaut,
`corneaWetness`, `tearMeniscus`) ; la **pupille** (`pupilDilation`, `pupilSpeed`, `autoDilation`) ;
l'animation procédurale (clignement `autoBlinkEnabled`/`blinkRate`/`blinkDuration`, saccades
`autoSaccadeEnabled`/`saccadeFrequency`/`saccadeAmplitude`) ; et le **regard** (`lookTarget`,
`useLookTarget`, `lookBlendSpeed`), plus l'état interne.

`NkEyeSystem` (`Describe()` **inline** : `PostUpdate`, **priorité 780**) anime tout cela
(`UpdateBlink`, `UpdateSaccade`, `UpdatePupil` selon une luminance ambiante, `UpdateLook`) ;
`Execute` est **impl externe**.

- **Animation faciale** : les yeux qui clignent, saccadent et suivent une cible donnent vie au
  regard — la pupille qui se dilate selon la lumière (ou l'émotion) est un détail puissant.
- **Rendu** : la réfraction de la cornée (1.336) et le ménisque lacrymal rendent l'œil humide et
  vivant.

### `NkTeethMaterial` — les dents

POD pur. `albedoTex`/`normalTex`, `baseColor` ivoire par défaut, `roughness`, `wetness`,
`translucency`, `subsurfaceColor`, `jawOpenAmount`/`teethVisibility` (pilotés par l'ouverture de la
bouche), `showSaliva`.

- **Rendu** : les dents translucides et humides évitent l'effet « dentier » d'un blanc plat.

---

### Synthèse des priorités systèmes

Dans l'ordre d'exécution, par passe ECS :

- **`Update`** : `NkCrowdSystem` (150) → `NkLocomotionSystem` (200).
- **`PostUpdate`** : `NkMotionMatchSystem` (300) → `NkFootIKSystem` (350) → `NkConstraintSystem`
  (500) → `NkEyeSystem` (780) → `NkFacialSystem` (800).

La logique est nette : on calcule d'abord le mouvement (`Update`), puis on corrige la pose finale
(IK, contraintes), et enfin le visage juste avant le rendu.

### Exemple

```cpp
#include "Noge/Anim2D/NkTween.h"
#include "Noge/Facial/NkFacialRig.h"
#include "Noge/Anim/NkLocomotion.h"
using namespace nkentseu;

// Tween : faire monter une barre de vie en douceur (toute valeur en mémoire).
float32 healthBar = 0.f;
NkTweenManager::Get().To(&healthBar, 1.f, 0.4f).SetEase(NkEase::CubicOut);
// ... chaque frame :
NkTweenManager::Get().Update(dt);

// Locomotion : table vitesse -> clip (inline, garanti).
NkLocomotion loco;
loco.AddSpeedEntry(1.5f, walkClip);
loco.AddSpeedEntry(5.5f, runClip);
loco.desiredVelocity = inputDir * 5.5f;   // alimente l'intention

// Facial : un sourire sincère (inline). ResetAUs d'abord = blend exclusif.
NkFacialRig rig;
rig.ResetAUs();
rig.SetAU(NkFACS::AU12_LipCornerPullerL, 0.8f);
rig.SetAU(NkFACS::AU12_LipCornerPullerR, 0.8f);
rig.SetAU(NkFACS::AU06_CheekRaiser, 0.6f);
```

---

[← Index Noge](README.md) · [Récap Noge](../Noge.md) · [Couche Engine](../README.md)
