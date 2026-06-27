# NkAnima — Roadmap (outil d'animation physiquement correct + IA)

> **Cap actuel de Rihen côté moteur** (depuis 2026-06-27). Fichier de pilotage de
> la nouvelle direction. À LIRE au démarrage d'une session NkAnima.
> Nom de travail **NkAnima** (à valider). Lié depuis `CLAUDE.md`.

## Vision

Un outil d'animation **physiquement correct + assisté par IA**, à la **Cascadeur**,
natif Nkentseu. Il sert directement deux objectifs profonds :
- **PV3DE** — animation corporelle réaliste du patient virtuel.
- **NKAI** — amorce concrète de l'IA from-scratch (auto-pose, physics-aware).

Différenciateur : intégré au moteur (NKRenderer pour le rendu, le skinning GPU déjà
livré, NKCollision orienté anim pour la physique), pas un outil externe.

## Contexte de décision (2026-06-27)

- L'autre IA tient **NKCode + NKReflection**. Rihen est libre sur le reste.
- Choix entre (Noge/Nogee · app animation · Tier 2/3 moteur) → **app animation via
  ses fondations**, car ses 1res briques SONT le meilleur Tier 2/3 (IK, anim avancée),
  c'est non bloqué, aligné PV3DE + NKAI, et visible vite.
- **Editor Kit** déjà bien avancé et **utilisé dans NKCode** → la couche UI (M5) est
  moins bloquée que prévu.
- Acquis utiles déjà en place : **skinning GPU 4 backends** (VK/GL/DX11/DX12, livré
  cette session), **loaders glTF** (géométrie + matériaux + skinning + anim), système
  **Animation** (tracks/blend) et système **IK** (API complète mais solveur orphelin).

## Milestones

### M0 — IK FONCTIONNEL *(✅ TERMINÉ 2026-06-27)*
Brancher `NkIKSystem` ↔ pose squelette réelle. Le solveur FABRIK/CCD/Two-Bone
existait mais tournait sur des **placeholders {0,0,0}** et **ne lisait/écrivait jamais
les bones** (cf audit ROADMAP NKRenderer Tier 2 #8).

**✅ FAIT (2026-06-27)** — cœur FABRIK rendu FONCTIONNEL (`NkIKSystem.{h,cpp}`,
compile NKRenderer OK) :
- `NkIKRig::SetWorldPose(worldMats, count)` : l'appelant fournit la pose MONDE
  courante (matrices par os) AVANT `Solve()`.
- `SolveChain_FABRIK` : lit les positions monde réelles `bones[boneIdx].position`
  (au lieu de {0,0,0}), `root` = racine réelle, déduit les longueurs de segment de
  la pose si absentes, **réécrit** les positions résolues dans `bones[boneIdx].position`.
  (L'algo forward/backward FABRIK était déjà correct.)

**✅ DÉMO M0a FAITE (2026-06-27)** — `Applications/Sandbox/src/Demo/DemoIK.cpp`
(`renderdemo --demo=14`). Chaîne 5 os procédurale, cible **animée** (lemniscate),
FABRIK temps réel : la chaîne se plie et l'effecteur ATTEINT la cible. Validé
visuellement (capture RenderDoc thumb). Rendu en **sphères mesh PBR** (joints cyan,
racine verte, os orange, cible rouge) — car le debug-draw NkRender3D est un STUB
(cf ci-dessous). Éclairage : 2 directionnelles + IBL neutre.

**✅ (b) FAIT (2026-06-27) — write-back avec ROTATION orientée-enfant** :
`SolveChain_FABRIK` écrit désormais la **matrice monde complète** de chaque os =
`Translate(pos[i]) * q.ToMat4()`, où `q = NkQuatf(restDir, dirSegment)` (rotation
minimale from-to, ctor `NkQuat.h:168/762`). `dirSegment = normalize(pos[i+1]-pos[i])`,
dernier os = rotation identité. La colonne translation reste `pos[i]` → DemoIK
(qui ne lit que `.position`) est inchangé, mais l'IK produit maintenant des
transforms utilisables par le **skinning GPU**. Compile NKRenderer OK.

**✅ (d) FAIT (2026-06-27) — IK sur un VRAI squelette glTF** :
- Nouvelle API réutilisable `EvaluateGLTFWorldJoints(data, animIdx, t, outWorld,
  outParentJoint)` (`NkGLTFLoader.{h,cpp}`) : sort les transforms **MONDE** par
  joint (= `globalTransform`, SANS l'inverseBind, contrairement à `EvaluateGLTFPose`)
  + le **parent en indice de joint** (remonte la hiérarchie de nodes jusqu'au 1er
  ancêtre qui est aussi un joint). Position monde d'un joint = colonne translation.
- Démo `Applications/Sandbox/src/Demo/DemoIKChar.cpp` (`renderdemo --demo=15`,
  `NK_SKIN_MODEL` override) : charge **CesiumMan** (19 joints), sélectionne
  AUTOMATIQUEMENT un membre (plus longue chaîne feuille→racine, ici 5 os, feuille
  joint=9, reach 0.77m), fait suivre une cible animée à l'effecteur via FABRIK
  (le reste du squelette reste en bind pose), rend le **squelette complet en
  debug-lines** (membre IK orange vif, reste gris, joints en sphères debug, cible
  rouge). Vérifié fonctionnellement (log : load + extract 19 joints + chaîne 5 os
  + Solve + 150 frames + sortie propre). Capture pixel différée (injection
  RenderDoc HS sur cette machine — échoue AUSSI sur le démo=14 déjà validé ;
  le render-path debug-line a déjà été validé visuellement sur DemoIK).

**✅ (d-bis) FAIT (2026-06-27) — RE-SKIN GPU : le MESH se déforme** :
`DemoIKChar` charge maintenant le mesh GPU skinné de CesiumMan (+ matériaux glTF
via `BuildGLTFMaterials`, calque DemoSkin) et le **déforme** avec l'IK. Chaque
frame : on recompose le **global** de chaque joint — joint de la chaîne = global
IK (res) ; descendant d'un joint résolu = `delta(ancêtre) * bindGlobal(j)` avec
`delta = resGlobal(a) * bindGlobal(a)^-1` (la main suit le poignet) ; joint non
affecté = bind — puis `skin[j] = global[j] * inverseBind[j]` → `SubmitSkinned`
(même chemin éprouvé que DemoSkin). `NK_IKCHAR_NOMESH=1` repasse au squelette
seul. Vérifié : `mesh=1`, 150 frames, sortie propre. Limite connue : le write-back
(b) oriente depuis `{0,1,0}` (pas la convention bind exacte) → léger twist près du
membre ; fidélité d'orientation = raffinement (préserver le frame bind local).

**✅ (b+) FAIT (2026-06-27) — orientation BIND-FIDÈLE (twist supprimé)** :
`SolveChain_FABRIK` compose désormais la rotation IK comme un **delta monde**
`NkQuatf(bindDir, newDir)` appliqué sur la **rotation bind locale** de l'os
(`bones[bi]` privé de sa translation), au lieu d'imposer `{0,1,0}→segDir`. On
capture les positions BIND (`bindPos = pos` avant résolution) pour `bindDir`.
Résultat : le twist d'origine de chaque os est PRÉSERVÉ → plus de vrille du membre
re-skinné. En bind pose (dir inchangée) le delta = identité → matrice bind exacte.
L'effecteur (sans segment fils) suit le delta de son parent (`lastDelta`). DemoIK
(--demo=14, lit `.position`) et DemoIKChar (--demo=15) tournent clean.

**✅ (c) FAIT (2026-06-27) — Two-Bone + CCD branchés (3 solveurs)** :
factorisation de deux helpers partagés (`BuildChainPositions` = positions monde +
déduction longueurs ; `WriteBackBindFidele` = write-back (b+)) consommés par les
3 solveurs. **CCD** : passes effecteur→racine, pivote le sous-bras autour de chaque
joint via `NkQuatf(dirEff,dirCible)` (rotations rigides → longueurs préservées).
**Two-Bone** : analytique (loi des cosinus) sur racine/milieu/effecteur, coude
orienté par le `poleVector` (ou direction bind), joints surnuméraires prolongés.
Sélecteur démo `NK_IK_SOLVER=fabrik|ccd|twobone` (défaut fabrik) → les 3 tournent
clean sur CesiumMan (--demo=15, EXIT=0). FABRIK refactorisé sur les mêmes helpers.

**✅ (a') FAIT (2026-06-27) — effecteur draggable à la souris** :
`DemoIKChar` : clic gauche maintenu = la cible suit le curseur. Unprojection
écran→monde via `inv(cam.GetViewProj())` (rayon caméra→curseur) intersecté avec le
plan passant par l'ancre du membre (normale = visée caméra), point clampé dans le
rayon d'atteinte (IK reste solvable). L'orbite caméra se **gèle** pendant le drag ;
une fois déplacée, la cible reste posée (`hasManual`). Input via
`NkEvents().AddEventCallback<NkMouseMove/ButtonPress/Release>` (header
`NKWindow/Core/NkWESystem.h`). **La logique souris vit dans la démo** — le solveur
`NkIKSystem` (engine) reste pur et réutilisable jeu+app. Buildé, EXIT=0 ; drag à
valider en interactif (non testable en headless).

**✅ FIX MAJEUR (2026-06-27) — déchirure du mesh = bug `NkMat4::Inverse()`** :
le re-skin déchirait (étirement d'arête jusqu'à 83×). Diagnostic CPU (metric
d'étirement d'arêtes, gated `NK_IK_TEAR_DIAG`) → isolé : ni le skinning ni l'anim
native (5.9×), mais **`NkMat4::Inverse()` retournait la TRANSPOSÉE de l'inverse**
(adjugée non transposée, `NkMat.h:1019` `Cofactor(row,col)`→`Cofactor(col,row)`).
Faux dès qu'il y a translation/scale (OK seulement rotations pures). **Corrigé dans
NKMath** → impact moteur LARGE : `normalMatrix = Inverse().Transpose()` (éclairage,
les normales étaient transformées par Rᵀ au lieu de R sur les objets tournés !) +
`invViewProj` caméra (SSR/SSAO/fog) sont désormais CORRECTS partout. Re-skin refait
en **aim-FK hiérarchique** (`ComputeIKWorld` : chaque joint de chaîne pivote pour
placer son enfant sur la position FABRIK, FK cohérente racine→feuilles) → **0
déchirure** (étirement max 1.34×, < l'anim native). Démos 2/3/13/15 non régressées.

**M0 = TERMINÉ.** L'IK est fonctionnel de bout en bout : 3 solveurs (FABRIK/CCD/
Two-Bone), write-back bind-fidèle, debug-line renderer réel, sur chaîne procédurale
(DemoIK) ET sur un vrai personnage skinné (DemoIKChar : CesiumMan, squelette + mesh
déformé SANS déchirure, effecteur draggable). API engine réutilisable jeu (Noge) +
app (NkAnima).

**✅ CHANTIER TRANSVERSE FAIT (2026-06-27) — vrai debug-line renderer** :
`NkRender3D::FlushDebug` était un STUB ; maintenant il REND vraiment. Toute l'API
`DrawDebugLine/Sphere/Grid/Axes/AABB/Arrow/Circle` dessine (utile à TOUT le moteur).
Impl : shader `DebugLine` (`Resources/.../DebugLine/NkSL/debugline.{vert,frag}.nksl`,
pos+couleur → CameraUBO.viewProj), `EnsureDebugLinePipeline` (topologie `NK_LINE_LIST`,
vertex stride 28 = pos vec3 + color vec4, descriptor set 0 = CameraUBO), VBO dynamique
réuploadé/frame (`VertexDynamic` + `WriteBuffer`), flush dans la passe Geometry
(`FlushDebug(cmd, currentRP, gs)`). **Fix accumulation** : `DrawDebugLine` life<=0 =
"une frame" → rendue PUIS purgée (avant : floor 0.016 → accumulait à haut FPS).
DemoIK utilise désormais les vraies lignes (os + grille + ligne d'aide racine→cible).
Validé visuellement GL (capture RenderDoc). À tester sur VK/DX (le shader NkSL est
transpilé partout — vérifier comme pour les autres).
- Câbler `NkIKSystem::Solve` : lire les **positions monde** des bones depuis la pose
  courante (NkAnimationSystem), résoudre vers la cible, **réécrire** rotations/positions
  dans la pose → le skinning GPU (déjà fonctionnel) reflète l'IK.
- **Démo** Sandbox : membre skinné (ou bras de CesiumMan) + **effecteur 3D draggable**,
  FABRIK temps réel qui suit la souris.
- **État audit (2026-06-27)** : `Tools/IK/NkIKSystem.{h,cpp}` — API solide :
  `NkIKRig`/`NkIKChainDesc`/`NkIKBone`(boneIdx+length+constraint+restDir)/`NkIKTarget`
  (position+rotation+pole), solveurs `NK_TWO_BONE/NK_CCD/NK_FABRIK`, `GetBoneMatrices()`.
  `NkIKRig` a `mSkeletonId` mais le lien skeleton→positions monde n'est pas fait.
  Pose/bones : `Tools/Animation/NkAnimationSystem` + `mSkinned[].boneMatrices` (consommé
  par `NkRender3D` → `mUBOBonesRing`). À cartographier précisément (suite de l'audit).

### M1 — Pose & timeline *(EN COURS)*
Éditer des poses-clés, timeline, interpolation, save/load `.nkanim`.

**Audit (2026-06-27)** : le système d'anim `Tools/Animation/NkAnimationSystem` est
DÉJÀ riche — `NkAnimationClip` (boneTracks `NkAnimationTrack<NkMat4f>` par os +
morph/UV/material/transform/caméra/lumière/PP), sampling avec 9 modes d'easing,
`NkAnimationPlayer` (Play/Update/GetState/**BlendTo** crossfade/markers),
`NkAnimationSystem::ApplySkinnedMesh`/**ApplyOnionSkin** (pelure d'oignon = clé pour
l'édition Cascadeur). M1.a (modèle + sampler) était donc DÉJÀ là.

**✅ M1.b FAIT (2026-06-27) — format BINAIRE `.nkanim` + import glTF + démo** :
- **`NkAnimationClip::SaveBinary/LoadBinary`** : format compact versionné `NKAN`
  (header magic+version+name+dur+fps+loop, puis tracks d'os : nom+enabled+clés
  [time+mat4+interp]). PAS de JSON (anim = beaucoup de floats → binaire = compact +
  chargement rapide sans parsing). Extensible (header versionné → morph/transform
  plus tard). `GetKey()` ajouté à `NkAnimationTrack` pour la sérialisation.
- **`NkAnimationClip::BakeFromGLTF(data, animIdx, fps)`** : échantillonne
  `EvaluateGLTFPose` sur toute la durée → keyframes par os éditables + sauvables.
  Engine-level (jeu Noge + app NkAnima).
- **Démo `DemoAnim` (`renderdemo --demo=16`)** : CesiumMan → bake → save `.nkanim`
  → **RELOAD** → `NkAnimationPlayer` → `SubmitSkinned`. Round-trip VÉRIFIÉ
  (`match=1`, 19 os, 61 frames @30fps, .nkanim de 80 Ko) + **visuellement** (le perso
  MARCHE depuis le clip rechargé, mesh propre, 144 FPS).

**Améliorations NkMath FAITES (2026-06-27, autorisées par Rihen, conservées)** :
- **`NkQuat::SLerp` RÉPARÉ** : dépendait d'un `operator^` au linkage friend↔template
  cassé (jamais exercé avant → `undefined reference` dès qu'on l'utilise). Réécrit en
  formule de Shoemake directe `sin((1-t)θ)/sinθ·a + sin(tθ)/sinθ·b`.
- **`NkMat4::DecomposeTRS`** ajouté (translation+rotation-matrice+scale, n'existait pas).

**⚠️ Interp TRS-slerp sur les boneTracks = REVERTÉE (mauvaise approche)** : appliquer
décompose+SLERP sur les **matrices de SKINNING bakées** (`global×inverseBind`, scale
composite + réflexions possibles) donne des poses FAUSSES (marche cassée, signalé par
Rihen). Retour au lerp matriciel direct, correct à 30fps (clés rapprochées). Le slerp
TRS appartient au niveau **bone-LOCAL** → cf RESTE M1 (stocker les tracks en TRS local
+ FK au sample). NkMath SLerp/DecomposeTRS restent prêts pour ça.

**RESTE M1** :
- **M1.c — timeline/scrubbing + édition de clés** (insertion/déplacement de
  keyframes) : UI à concevoir sur **NKGui** (prêt, on conçoit les interfaces ;
  PAS NKUI — directive Rihen). Widget timeline à créer (~1 sem), branché sur
  `NkAnimationClip`/`NkAnimationPlayer`.
- (option) Stocker les `boneTracks` en TRS bone-LOCAL + FK au sample plutôt que des
  matrices de skinning bakées (édition de pose plus naturelle pour la timeline).
- ~~**Superposer l'IK (M0) sur l'anim**~~ **✅ FAIT (2026-06-27)** : démo `DemoAnimIK`
  (`renderdemo --demo=17`) — le corps MARCHE (clip rejoué) ET un bras atteint une cible
  via IK FABRIK, EN MÊME TEMPS (= signature Cascadeur). Pipeline : `player.Update` →
  pose animée (skinning) → `animGlobal = animSkin × bindGlobal` (transforms monde
  animés) → FABRIK sur la chaîne du bras → aim-FK cohérente (base = pose animée) →
  `skin = world × inverseBind`. **Piège résolu** : `bindGlobal` DOIT être
  `inverse(inverseBind[j])` (PAS `EvaluateGLTFWorldJoints`, dont le global diffère du
  global implicite des inverseBind → sinon pose contorsionnée). Validé visuellement
  (perso debout qui marche + bras IK, 146 FPS). Bypass diagnostic `NK_ANIMIK_NOIK`.

### M2 — State machine / blend tree / retargeting
Transitions, blend de poses, appliquer une anim à un autre rig (retargeting).
(= NKRenderer Tier 3 #10 « animation avancée ».)

### M3 — Physique d'animation
Contraintes/ragdoll + **trajectoires physiquement correctes** (centre de masse
balistique, équilibre — la signature Cascadeur). Amorce **NKCollision** orienté anim.

### M4 — IA auto-pose
Petit modèle qui **prédit des poses plausibles** (pose→pose / physics-aware) =
1er vrai morceau de **NKAI**.

### M5 — App standalone
`Applications/NkAnima` complète + UI timeline/viewport via **Editor Kit** (déjà
utilisé dans NKCode).

## Dépendances / liens
- Rendu + skinning GPU : NKRenderer (`Tools/Render3D`, `Tools/Animation`, `Tools/IK`).
- Physique (M3) : NKCollision (non démarré, à amorcer orienté anim).
- IA (M4) : NKAI (vision, non démarré — cf mémoire `project_nkentseu_ai_vision`).
- UI (M5) : Editor Kit (Engine/NKEditorKit, utilisé dans NKCode).
- Cibles applicatives : PV3DE (animation corps), démos Sandbox.
