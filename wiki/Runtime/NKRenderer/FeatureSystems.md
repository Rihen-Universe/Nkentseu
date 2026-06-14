# Les systèmes de fonctionnalités

> Couche **Runtime** · NKRenderer · Les briques de haut niveau qui *bougent* et *exploitent* ce
> que le renderer dessine : l'animation (`NkAnimationSystem`), la cinématique inverse
> (`NkIKSystem`), les effets visuels (`NkVFXSystem`), la simulation de personnages
> (`NkSimulationRenderer`) et le rendu vers l'IA (`NkAIRenderingSystem`).

Le cœur de NKRenderer sait dessiner un maillage avec un matériau, sous des lumières, dans une
caméra. Mais une scène **vivante** ne tient pas à un seul dessin figé : un personnage marche, un
genou plie pour poser un pied au sol, des étincelles jaillissent d'une épée, un visage exprime la
peur, et — de plus en plus — un réseau de neurones veut *voir* l'image que le GPU vient de produire.
Tout cela se trouve sous `Tools/`, et c'est là que **le mouvement, la réaction et l'exploitation**
rejoignent le pixel. La question n'est jamais « comment dessiner » (le renderer s'en charge) mais
« **qu'est-ce que je fais évoluer, et dans quel ordre** ».

Les cinq systèmes partagent une même grammaire que cette page vous apprend une fois pour toutes : un
**objet gestionnaire** (`NkXxxSystem`) qu'on `Init` avec un `NkIDevice*`, qui **possède** les objets
qu'il fabrique (`Create…` / `Destroy…`), qu'on `Update`/`Solve`/`Render`/`Flush` une fois par frame,
puis qu'on `Shutdown`. Rien ne s'alloue avec `new` côté appelant : la mémoire vient du moteur
(NKMemory). Et l'ordre par frame n'est pas négociable : **Animation → IK → VFX → AI**.

- **Namespace** : `nkentseu::renderer`
- **Headers** : `NKRenderer/Tools/Animation/NkAnimationSystem.h` ·
  `NKRenderer/Tools/IK/NkIKSystem.h` · `NKRenderer/Tools/VFX/NkVFXSystem.h` ·
  `NKRenderer/Tools/Simulation/NkSimulationRenderer.h` ·
  `NKRenderer/Tools/AIRendering/NkAIRenderingTarget.h`

---

## L'animation : faire varier une valeur dans le temps

`NkAnimationSystem` répond au besoin le plus universel du temps réel : **interpoler une valeur entre
des instants clés**. Une animation n'est rien d'autre qu'une *courbe* — une liste de keyframes
`(temps, valeur)` — qu'on **évalue** à un instant `t` donné. C'est le `NkAnimationTrack<T>` : il
range ses `NkKeyframe<T>` triés par temps et son `Evaluate(t)` cherche le segment encadrant puis
mélange les deux bornes selon un **mode d'interpolation** (`NkInterpMode`). Le mode change l'allure
du mouvement : `NK_STEP` saute d'un palier à l'autre, `NK_LINEAR` trace une droite, `NK_EASE_IN_OUT`
démarre et finit en douceur, `NK_BOUNCE`/`NK_ELASTIC`/`NK_BACK` ajoutent du caractère.

Ce qui rend le système puissant, c'est que **n'importe quoi** peut être une track. Un
`NkAnimationClip` n'anime pas qu'un squelette (`boneTracks`) : il anime aussi les **morph targets**,
les **UV** d'un sprite (flipbook), la **couleur d'un matériau**, la **position/rotation/échelle**
d'un transform, la **caméra** (position, cible, FOV, profondeur de champ), une **lumière**
(intensité, couleur, portée) et le **post-process** (exposition, bloom, vignette). Un clip est un
**document de données en lecture seule** pendant la lecture ; c'est le `NkAnimationPlayer` qui
détient le **temps** et le **mode de lecture** (`NkPlayMode` : une fois, en boucle, ping-pong,
figé), qu'on fait avancer avec `Update(dt)`, et dont on lit le résultat évalué via un
`NkAnimationState` — un *snapshot* prêt à appliquer.

```cpp
NkAnimationClip* spin = anim.CreateClip("turret_spin");
*spin = *NkAnimationClip::MakeSpinClip("turret_spin", 30.f);  // 30 tr/min autour de Y

NkAnimationPlayer* p = anim.CreatePlayer("turret");
p->SetClip(spin);
p->Play(NkPlayMode::NK_LOOP);
// chaque frame :
p->Update(dt);
NkMat4f world = anim.ApplyTransform(*p, baseWorld);
```

Le player ne **possède pas** son clip : il en tient un `const NkAnimationClip*`. Le clip doit donc
survivre au player — c'est le système qui les possède tous les deux et les libère par `DestroyClip`
/ `DestroyPlayer`. Pour enchaîner deux animations sans à-coup, `BlendTo(next, dur)` fait un
*crossfade* ; pour réagir à un instant précis (un pas, un impact), `AddMarker` + `SetMarkerCallback`
déclenchent un appel quand le temps franchit le marqueur.

Ce n'est **pas** un système de *retargeting* ni un graphe d'états (state machine) : il joue des
clips et les mélange deux à deux. Et `NK_CUBIC` est déclaré dans `NkInterpMode` mais retombe
aujourd'hui sur du linéaire dans l'easing interne — ne comptez pas dessus pour une vraie spline.

> **En résumé.** Une track `NkAnimationTrack<T>` = une courbe de keyframes évaluée à `t` avec un
> `NkInterpMode`. Un `NkAnimationClip` groupe des tracks pour **tout** (os, morph, UV, matériau,
> transform, caméra, lumière, post). Le `NkAnimationPlayer` (non-owning sur le clip) porte le temps
> et le `NkPlayMode`, produit un `NkAnimationState`, et `BlendTo` fait le crossfade. Le système
> possède clips et players.

---

## La cinématique inverse : poser l'effecteur, déduire la chaîne

Une animation joue une pose **enregistrée** : la jambe bouge comme on l'a capturée. Mais que se
passe-t-il quand le pied doit toucher un escalier dont on ignorait la hauteur, ou quand la main doit
saisir une poignée qui n'était pas là à l'enregistrement ? C'est le problème inverse : on fixe la
**cible** de l'effecteur (le bout de la chaîne) et le solveur **déduit** les rotations des os
intermédiaires pour l'atteindre. C'est ce que fait `NkIKSystem`.

L'unité de travail est la **chaîne** (`NkIKChainDesc`) : une liste d'os `NkIKBone` allant de la
racine à l'effecteur, une `NkIKTarget` (position, rotation optionnelle, poids, *pole vector* pour
orienter le coude/genou), un `NkIKSolver` et un budget d'itérations. Le solveur est choisi selon la
nature de la chaîne : `NK_TWO_BONE` est **analytique** (exact, instantané — bras et jambes à deux
segments), `NK_CCD` et `NK_FABRIK` sont **itératifs** (chaînes longues), `NK_SPLINE` plie une
colonne ou un tentacule, `NK_FBIK` résout le corps entier. Chaque os porte une `NkIKConstraint` qui
**borne** l'articulation : un genou est une `NK_HINGE` (un seul axe), une épaule une
`NK_BALL_SOCKET` (rotule), avec angles min/max et raideur.

Plusieurs chaînes vivent dans un `NkIKRig`, attaché à un squelette par son `skeletonId`. Le rig est
créé/détruit par le système (`CreateRig`/`DestroyRig`, indexé sur `skeletonId`) ; on lui ajoute des
chaînes, on déplace leurs cibles (`SetTarget`), on pondère (`SetWeight`, où 0 = pose d'animation
intacte, 1 = IK plein), et on lit les résultats world-space via `GetBoneMatrices`.

```cpp
NkIKRig* rig = ik.CreateRig(skeletonId);

NkIKChainDesc leg;
leg.name = "leg_R";
leg.solver = NkIKSolver::NK_TWO_BONE;
leg.bones = { /* hanche → genou → cheville */ };
NkIKChainId chain = rig->AddChain(leg);

rig->SetTarget(chain, footTargetOnStair);   // chaque frame
// après l'update de l'animation :
ik.Solve(dt);
```

L'IK se résout **après** l'animation et **lit/écrit les bone matrices** : l'ordre est crucial — une
chaîne résolue avant que la pose de base soit posée corrige une pose qui n'existe pas encore. Notez
que le commentaire d'usage du header montre une surcharge `AddChain("leg_R", {...}, solver)` qui
**n'existe pas** : la seule signature réelle est `AddChain(const NkIKChainDesc&)`.

> **En résumé.** L'IK fixe la cible et déduit les rotations. Une `NkIKChainDesc` = des `NkIKBone`
> (racine→effecteur) + une `NkIKTarget` + un `NkIKSolver` (analytique `NK_TWO_BONE`, itératifs
> `NK_CCD`/`NK_FABRIK`, `NK_SPLINE`, `NK_FBIK`) + des `NkIKConstraint` (`NK_HINGE`,
> `NK_BALL_SOCKET`…). Les chaînes vivent dans un `NkIKRig` (clé `skeletonId`). `Solve` **après**
> l'anim, qui lit/écrit les bone matrices.

---

## Les effets visuels : émettre, traîner, marquer

`NkVFXSystem` couvre les trois grandes familles d'effets qui peuplent une scène sans être des
maillages : les **particules**, les **traînées** et les **decals**. Le principe commun est l'**effet
de masse** — beaucoup de petits éléments éphémères pilotés par des règles, pas dessinés un par un à
la main.

Un **émetteur** (`NkEmitterDesc`) crache des particules selon une **forme** (`NkEmitterShape` :
point, sphère, boîte, cône, disque, arête), un débit, une durée de vie, des bornes de vitesse et de
taille, une couleur qui évolue du début à la fin, une gravité, un mode de fusion (`NkBlendMode`,
typiquement `NK_ADDITIVE` pour du feu) et un **mode de simulation** (`NkSimMode` : CPU ou GPU). On
le crée, on déplace sa source (`SetEmitterPos`), on l'allume/éteint, ou on déclenche une **rafale**
ponctuelle (`Burst`) — l'explosion. Une **traînée** (`NkTrailDesc`) est une bande qui suit un point
mobile : on lui pousse des points (`AddTrailPoint`) et elle s'efface par l'arrière selon sa durée de
vie — le sillage d'un projectile, la lame d'une épée. Un **decal** (`NkDecalDesc`) projette une
texture (albedo + normal) sur la géométrie : impact de balle, flaque, graffiti, avec durée de vie
optionnelle (`-1` = permanent).

```cpp
NkEmitterDesc fire;
fire.shape = NkEmitterShape::CONE;
fire.blend = NkBlendMode::NK_ADDITIVE;
fire.colorStart = {1,0.6f,0.1f,1};  fire.colorEnd = {1,0,0,0};
NkEmitterId torch = vfx.CreateEmitter(fire);

// chaque frame :
vfx.Update(dt, cameraData);
vfx.Render(cmd, cameraData);
```

`Update` fait avancer la simulation (et a besoin de la caméra, ne serait-ce que pour orienter les
*billboards*), puis `Render` soumet les draws sur le `NkICommandBuffer`. Attention au piège :
`DestroyEmitter`, `DestroyTrail` et `SpawnDecal`/`DestroyDecal` prennent leur ID **par référence**
(`NkEmitterId&`…) — l'ID passé est invalidé, ne le réutilisez pas.

> **En résumé.** `NkVFXSystem` = particules (émetteurs `NkEmitterDesc`, formes `NkEmitterShape`,
> `Burst` pour les explosions), traînées (`AddTrailPoint`) et decals (projection sur la géométrie).
> `Update(dt, cam)` simule, `Render(cmd, cam)` dessine. Les `Destroy*` invalident l'ID passé par
> référence.

---

## La simulation de personnages : visages et foules (stub)

`NkSimulationRenderer` est explicitement un **stub d'intégration future**, taillé pour PV3DE
(patient virtuel émotif), les foules et les agents. Il expose la *forme* de l'API sans tout le moteur
derrière. Son rôle : soumettre des personnages **skinnés** dont l'expression et l'émotion pilotent le
rendu.

Deux structures portent l'état facial. Un `NkEmotionState` est un vecteur de huit émotions de base
(`anger`, `joy`, `sadness`, `fear`, `disgust`, `surprise`, `pain`, `fatigue`, chacune dans 0..1) — la
couche « haut niveau » de l'humeur. Un `NkBlendShapeState` descend au niveau **musculaire**
(inspiré du FACS) : 25 curseurs comme `browInnerUp`, `jawOpen`, `mouthSmileL/R`, `eyeBlinkL/R`,
`cheekPuffL/R`, et même des effets de peau (`blush`, `pallor`, `sweat`, `tears`). On soumet un
personnage avec `SubmitCharacter` (maillage + matériau de peau + transform + bone matrices + blend
shapes + émotion), une **foule** avec `SubmitCrowd` (un tableau d'`AgentDesc` — structure imbriquée
qui ajoute vélocité et âge à chaque agent), et on rafraîchit les paramètres de peau d'un matériau
avec `UpdateSkinParams`.

```cpp
NkBlendShapeState face{};   face.jawOpen = 0.4f;  face.browInnerUp = 0.6f;
NkEmotionState    mood{};   mood.fear   = 0.8f;
sim.SubmitCharacter(mesh, skinMat, world, bones, boneCount, face, mood);
```

> **En résumé.** `NkSimulationRenderer` (stub) soumet des personnages skinnés pilotés par l'émotion.
> `NkEmotionState` = 8 émotions 0..1 (haut niveau) ; `NkBlendShapeState` = 25 curseurs FACS +
> effets de peau (bas niveau). `SubmitCharacter` pour un, `SubmitCrowd` (tableau d'`AgentDesc`) pour
> une foule.

---

## Le rendu vers l'IA : récupérer l'image côté CPU

`NkAIRenderingSystem` répond à un besoin neuf : un pipeline d'inférence (débruitage, super-résolution,
segmentation, vision robotique) a besoin de **lire** ce que le GPU a rendu, côté CPU, dans le bon
format. Faire un *readback* GPU→CPU naïvement bloquerait la frame ; ce système le fait en
**asynchrone**, via un *ring buffer* de N frames, et vous prévient par **callback** quand les données
sont prêtes.

Une cible (`NkAITargetDesc`) déclare la résolution, le **format** (`NkAIDataFormat` : `NK_AI_UINT8`
pour l'affichage, `NK_AI_FP16` pour ONNX/TensorRT, `NK_AI_FP32` pour OIDN/précision max), la
profondeur du ring, et surtout les **canaux** voulus — `NkAIChannel` est un **bitmask** qu'on
combine (`NK_AI_COLOR | NK_AI_DEPTH | NK_AI_NORMAL | …`). Chaque frame, on **capture** les textures
GPU pertinentes (`Capture(cmd, colorTex, depthTex, …)`), puis en fin de frame, après le Submit, on
appelle `FlushReadbacks()` : les readbacks **terminés** déclenchent leur callback, qui reçoit un
`NkAIFrame` portant des pointeurs CPU vers chaque canal demandé.

```cpp
NkAITargetDesc d;
d.channels = NkAIChannel::NK_AI_COLOR | NkAIChannel::NK_AI_DEPTH;
d.format   = NkAIDataFormat::NK_AI_FP16;
d.callback = [](const NkAIFrame& f){ inference.Run(f.colorData, f.width, f.height); };
NkAIRenderingTarget* t = ai.Create(d);

// chaque frame :
t->Capture(cmd, sceneColor, sceneDepth);
ai.FlushReadbacks();   // après le Submit
```

Le piège central est la **durée de vie** : les pointeurs de `NkAIFrame` (`colorData`, `depthData`…)
ne sont valides **que pendant le callback**. Si l'inférence est différée, **copiez** les données.
Et comme le readback est asynchrone (`ringSize`), la frame arrive avec **plusieurs frames de
latence** — sauf en mode `sync=true` (bloquant, réservé au debug/offline, où `WaitAll()` attend la
fin).

> **En résumé.** `NkAIRenderingSystem` lit l'image rendue côté CPU pour l'IA, en **readback
> asynchrone** (ring buffer). `NkAITargetDesc` déclare canaux (`NkAIChannel` bitmask), format
> (`NkAIDataFormat`) et callback. `Capture(cmd, …)` chaque frame, `FlushReadbacks()` après Submit.
> Les pointeurs `NkAIFrame` ne vivent **que pendant le callback** — copiez sinon. Latence de
> plusieurs frames sauf `sync=true`.

---

## Aperçu de l'API

Tous les types résident dans `nkentseu::renderer`. Chaque système suit le même cycle
`Init(NkIDevice*, …)` → `Create…`/`Destroy…` → `Update`/`Solve`/`Render`/`Flush` → `Shutdown`.

### Animation — `NkAnimationSystem.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enums | `NkInterpMode` (STEP/LINEAR/CUBIC/EASE_*/BOUNCE/ELASTIC/BACK) | Mode d'interpolation d'une track. |
| Enums | `NkPlayMode` (ONCE/LOOP/PING_PONG/CLAMP) | Mode de lecture d'un player. |
| Courbe | `NkKeyframe<T>` `{time, value, interp}` | Un point clé typé. |
| Courbe | `NkAnimationTrack<T>` : `AddKey`, `Evaluate`, `GetDuration`, `KeyCount` | Courbe d'une valeur typée. |
| Données | `NkAnimationClip` (boneTracks, morph, UV, matériau, transform, caméra, lumière, post) | Document d'animation read-only. |
| Données | `NkAnimationClip::Make*` (Spin/LightPulse/ColorFade/CameraShake/ProceduralWalk) | Fabriques de clips prêts. |
| Données | `NkAnimationState` | Snapshot évalué prêt à appliquer. |
| Lecture | `NkAnimationPlayer` : `SetClip`, `Play`, `Pause`, `Stop`, `SeekTo`, `Update`, `GetState` | Joue un clip, porte le temps. |
| Lecture | `BlendTo`, `IsBlending`, `AddMarker`, `SetMarkerCallback` | Crossfade et marqueurs d'événements. |
| Système | `Init`, `Shutdown`, `Update`, `CreateClip`/`DestroyClip`, `CreatePlayer`/`DestroyPlayer` | Cycle de vie + ownership. |
| Système | `ApplySkinnedMesh`, `ApplyTransform`, `ApplyMaterial`, `ApplyCamera`, `ApplyLight`, `ApplyPostProcess` | Application au renderer. |
| Système | `ApplyOnionSkin`, `ApplyMorphTargets`, `DrawSkeleton`, `GetAnimatedSpriteUV` | Outils avancés / debug. |

### IK — `NkIKSystem.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Handles | `NkIKRigId`, `NkIKChainId` (`NkRendHandle<T>`) | Identifiants opaques typés. |
| Enums | `NkIKSolver` (TWO_BONE/CCD/FABRIK/SPLINE/FBIK) | Algorithme de résolution. |
| Enums | `NkIKConstraintType` (FREE/HINGE/BALL_SOCKET/TWIST/PLANAR) | Type d'articulation. |
| Config | `NkIKConstraint`, `NkIKBone`, `NkIKTarget`, `NkIKChainDesc`, `NkIKConfig` | Description d'une chaîne et de ses limites. |
| Rig | `NkIKRig` : `AddChain`, `RemoveChain`, `SetTarget`, `SetWeight`, `SetEnabled`, `GetBoneMatrices` | Chaînes d'un squelette. |
| Système | `Init`, `Shutdown`, `CreateRig`/`DestroyRig`, `GetRig`, `Solve`, `SolveRig`, `SetDebugDraw` | Cycle de vie + résolution. |

### VFX — `NkVFXSystem.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enums | `NkEmitterShape` (POINT/SPHERE/BOX/CONE/DISK/EDGE), `NkSimMode` (CPU/GPU) | Forme d'émission, lieu de simulation. |
| Descripteurs | `NkEmitterDesc`, `NkTrailDesc`, `NkDecalDesc` | Particules / traînée / decal. |
| IDs | `NkEmitterId`, `NkTrailId`, `NkDecalId` (`IsValid()`) | Identifiants. |
| Particules | `CreateEmitter`/`DestroyEmitter`, `SetEmitterPos`, `SetEmitterEnabled`, `Burst`, `GetEmitterDesc` | Gestion des émetteurs. |
| Traînées | `CreateTrail`/`DestroyTrail`, `AddTrailPoint`, `ClearTrail` | Gestion des traînées. |
| Decals | `SpawnDecal`/`DestroyDecal` | Projection de texture. |
| Système | `Init`, `Shutdown`, `Update(dt, cam)`, `Render(cmd, cam)`, `GetActiveParticleCount`, `GetActiveEmitterCount` | Cycle + simulation + stats. |

### Simulation — `NkSimulationRenderer.h` (stub)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| État | `NkEmotionState` (8 émotions 0..1) | Humeur haut niveau. |
| État | `NkBlendShapeState` (25 curseurs FACS + peau) | Expression musculaire bas niveau. |
| Soumission | `SubmitCharacter`, `SubmitCrowd` (`AgentDesc`), `UpdateSkinParams` | Personnage / foule / params de peau. |
| Système | `Init`, `Shutdown` | Cycle de vie. |

### AI Rendering — `NkAIRenderingTarget.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enums | `NkAIChannel` (bitmask : COLOR/DEPTH/NORMAL/ALBEDO/MOTION/INSTANCE_ID/CUSTOM) | Canaux capturés. |
| Enums | `NkAIDataFormat` (UINT8/FP16/FP32) | Format des données readback. |
| Free fn | `operator|`, `NkAIHas(mask, flag)` | Composer et tester le bitmask. |
| Données | `NkAIFrame`, `NkAIFrameCallback`, `NkAITargetDesc` | Frame readback, callback, config de cible. |
| Cible | `NkAIRenderingTarget` : `Capture`, `Flush`, `WaitAll`, `SetCallback`, `Enable`, `GetFrameCount` | Une cible de readback. |
| Système | `Init`, `Shutdown`, `Create`/`Destroy`, `FlushReadbacks`, `IsAsyncReadbackSupported`, `GetTargetCount` | Cycle de vie + flush global. |

---

## Référence complète

Chaque système est repris à fond, avec ses usages dans les différents domaines du temps réel.

### `NkAnimationTrack<T>` et `NkKeyframe<T>` — la courbe de base

Une `NkAnimationTrack<T>` est une courbe : une suite de `NkKeyframe<T>` (`time`, `value`, `interp`)
qu'on remplit avec `AddKey(t, v, interp)` — l'insertion est **triée** (insertion-sort, `O(n)` par
scan + décalage). On l'évalue avec `Evaluate(t)` : track vide → `T{}`, hors bornes → clampé aux
extrémités, sinon recherche **linéaire** du segment encadrant puis lerp de l'alpha *easé* par le mode
de la borne. Le `T` est libre : une track de `float32` (un curseur), de `NkVec3f` (une position), de
`NkVec4f` (une couleur ou un quaternion), de `NkMat4f` (une matrice d'os). `GetDuration()` rend le
temps du dernier key, `KeyCount()` le nombre de points, `Empty()` teste la vacuité.

Ses usages couvrent **tout domaine où une valeur varie dans le temps**, pas seulement le squelette :

- **Animation** — courbe d'os, de morph, de transform : la brique fondamentale d'un clip.
- **UI / 2D** — animer une opacité, une position d'élément, un curseur de barre de vie ; une track
  `float32` avec `NK_EASE_IN_OUT` fait un fondu propre.
- **Gameplay** — courbe de dégâts, d'accélération, de difficulté dans le temps d'une vague.
- **Audio** — courbe de volume ou de hauteur (pitch) automatisée, comme une enveloppe.
- **Outils / éditeur** — base d'un *curve editor* : chaque track est une courbe éditable point par
  point.

Le `NkInterpMode` choisit l'**allure** : `NK_STEP` (paliers nets, pour des états discrets),
`NK_LINEAR` (constant, mécanique), les `NK_EASE_*` (accélération/décélération naturelle),
`NK_BOUNCE`/`NK_ELASTIC`/`NK_BACK` (rebond, ressort, léger dépassement — pour du *juice*). Réserve :
`NK_CUBIC` est listé mais l'easing interne le traite comme du linéaire (pas de spline Catmull-Rom
distincte aujourd'hui).

### `NkAnimationClip` — le document multi-pistes

Un clip groupe des dizaines de tracks couvrant **toutes les dimensions animables** : squelette
(`boneTracks` + `boneCount`), morph targets (`morphTracks` + `morphNames`), UV/sprite (`uvOffset`,
`uvScale`, `uvRotation`, `spriteFrame` + dimensions d'atlas), matériau (`albedoColor`,
`emissiveColor`/`emissiveStrength`, `metallic`, `roughness`, `opacity`, plus des `customFloats` et
`customVec4s` nommés), transform (`position`, `scale`, `rotation` quaternion), caméra (`position`,
`target`, `FOV`, `DOFFocus`/`DOFAperture`), lumière (`intensity`, `range`, `color`, `position`) et
post-process (`exposure`, `saturation`, `contrast`, `bloomStrength`, `DOFFocus`, `vignetteIntensity`).
Le clip est **read-only pendant la lecture** : c'est une donnée, pas un état.

Quelques outils de construction : `RecalcDuration()` recalcule la durée à partir des tracks,
`AddBoneKey` pousse une matrice d'os à un instant, `ResizeBones` dimensionne, et
`BuildSpriteFlipBook(frameCount, fps)` génère une animation de sprite atlas. Surtout, les **fabriques
statiques** `Make*` produisent des clips prêts à l'emploi (un `NkAnimationClip*` alloué par le
moteur) :

- `MakeSpinClip` — rotation continue (tourelle, pièce de monnaie, ventilateur).
- `MakeLightPulse` — lumière qui palpite entre deux intensités (torche, alarme).
- `MakeColorFade` — fondu de couleur (apparition, dégât flash).
- `MakeCameraShake` — secousse de caméra (impact, explosion).
- `MakeProceduralWalk` — marche procédurale de base sur N os.

Ne **jamais** `delete` ces pointeurs : leur désallocation passe par `DestroyClip` du système, comme
tout objet possédé.

### `NkAnimationPlayer` et `NkAnimationState` — la lecture

Le player détient le **temps** et joue un clip dont il n'est **pas propriétaire** (`SetClip` /
`BlendTo` prennent un `const NkAnimationClip*`). `Play(mode, speed)` lance la lecture selon un
`NkPlayMode`, `Pause`/`Stop`/`SeekTo` la contrôlent, `Update(dt)` l'avance. On interroge l'état :
`IsPlaying`, `GetTime`, `GetNormTime` (temps normalisé 0..1), `GetFrame`. Le résultat évalué est un
`NkAnimationState` (`GetState()`) — un snapshot avec des **défauts sensés** (albedo blanc, roughness
0.5, scale 1, rotation identité, exposition 1…) prêt à pousser dans le renderer.

Deux mécaniques de confort. Le **blending** : `BlendTo(next, dur)` enchaîne sur un nouveau clip par
crossfade ; `IsBlending()` indique si un mélange est en cours. Les **marqueurs** : `AddMarker(t, ev)`
place un événement nommé sur la timeline et `SetMarkerCallback(fn)` (un `NkFunction<void(const
NkString&)>`) est appelé quand le temps le franchit — idéal pour synchroniser un son de pas, un
spawn de VFX, une fenêtre de coup en gameplay. Le clip doit survivre au player : c'est l'invariant
d'ownership à ne jamais oublier.

### `NkAnimationSystem` — le gestionnaire et l'application au renderer

Le système possède clips et players. Après `Init(device, r3d)`, `Update(dt)` avance **tous** les
players d'un coup. La famille `Create*`/`Find*`/`Destroy*` gère le cycle ; les `Destroy*` prennent
un `T*&` et **mettent le pointeur à null** — ne réutilisez pas la variable après.

Le vrai travail est l'**application** d'un player au rendu, par domaine :

- **Rendu / personnage** — `ApplySkinnedMesh(mesh, mat, baseWorld, player)` dessine un maillage
  skinné avec les bone matrices courantes ; `ApplyMorphTargets` mélange des cibles de morph (option
  GPU) ; `DrawSkeleton` affiche les os pour le debug.
- **Transform / objet** — `ApplyTransform(player, base)` renvoie la matrice monde animée (un coffre
  qui s'ouvre, une plateforme mouvante).
- **Matériau** — `ApplyMaterial(inst, player)` pousse couleurs/metallic/roughness/emissive animés
  dans une `NkMaterialInstance` (un métal qui chauffe, un néon qui clignote).
- **Caméra** — `ApplyCamera(cam, player)` anime une `NkCamera3D` (cinématique, travelling, secousse).
- **Lumière** — `ApplyLight(light, player)` anime un `NkLightDesc` (pulsation, coucher de soleil).
- **Post-process** — `ApplyPostProcess(pp, player)` anime un `NkPostConfig` (montée de bloom à
  l'explosion, vignette à la blessure).
- **2D / sprite** — `GetAnimatedSpriteUV(player)` rend le rectangle UV `{u0,v0,u1,v1}` du flipbook
  courant.
- **Outils / éditeur** — `ApplyOnionSkin(...)` rend des « fantômes » passés/futurs (couleurs rouge
  passé / bleu futur par défaut) — la pelure d'oignon d'un éditeur d'animation.

### `NkIKSystem` — chaînes, contraintes, résolution

L'IK travaille par **chaîne**, décrite par une `NkIKChainDesc` : un `name`, un `NkIKSolver`, une
liste de `NkIKBone` ordonnée **racine → effecteur**, une `NkIKTarget`, un budget `maxIterations` et
une `tolerance`. Chaque `NkIKBone` porte un index, un nom, une longueur, une direction de repos et
une `NkIKConstraint` qui borne son mouvement (`type`, `axis` pour une charnière, angles min/max,
`stiffness` 0 souple → 1 rigide). La `NkIKTarget` fixe la position visée, une rotation optionnelle
(`matchRotation`), un `weight` (0 ignore l'IK, 1 plein effet) et un *pole vector* (`usePole`) pour
décider de quel côté plie le coude/genou.

Le choix du solveur dépend de la chaîne :

- **`NK_TWO_BONE`** — analytique, exact, instantané. Bras, jambes (deux segments) : c'est le cas le
  plus fréquent en personnage (poser un pied, atteindre une poignée).
- **`NK_CCD` / `NK_FABRIK`** — itératifs, pour des chaînes plus longues (doigts, queue, bras à
  plusieurs articulations) ; on règle le compromis qualité/coût par `maxIterations`.
- **`NK_SPLINE`** — colonne vertébrale, tentacule, câble : courbe lisse le long de la chaîne.
- **`NK_FBIK`** — full body : tout le squelette résolu ensemble (un personnage qui s'appuie, ragdoll
  partiel).

Les contraintes donnent du réalisme : un genou en `NK_HINGE` (un seul axe, ne se tord pas), une
épaule en `NK_BALL_SOCKET`, `NK_TWIST` pour un avant-bras, `NK_PLANAR` pour limiter à un plan. Les
chaînes vivent dans un `NkIKRig` (créé par `CreateRig(skeletonId)`, indexé sur le squelette) :
`AddChain`/`RemoveChain`, `SetTarget`, `SetWeight`, `SetEnabled`/`EnableAll`, lecture par
`GetChain`/`FindChain`, et résultats world-space via `GetBoneMatrices()`. Le système résout
`Solve(dt)` (toutes les rigs) ou `SolveRig(rig, dt)` (une seule), **après** l'update d'animation. La
`NkIKConfig` règle le global (`gpuSkinning`, `debugDraw`, `maxRigs`, `maxChainsTotal`).

Usages : **personnage** (pieds qui épousent le terrain, mains qui saisissent), **animation
procédurale** (pas adaptatifs, regard qui suit une cible), **outils/éditeur** (manipuler une pose en
tirant l'effecteur), **gameplay** (un bras de robot qui pointe une cible mobile).

### `NkVFXSystem` — émetteurs, traînées, decals

L'**émetteur** est le cheval de bataille. Son `NkEmitterDesc` règle tout : la `NkEmitterShape` (où
les particules naissent), `ratePerSec`/`burstCount` (débit continu vs rafale), `lifeMin`/`lifeMax`,
`speedMin`/`speedMax`, `sizeStart`/`sizeEnd`, `colorStart`/`colorEnd` (la particule fond d'une
couleur à l'autre, typiquement vers l'alpha 0), `gravity`, `velocityDir`/`velocityRand`, la
`texture`, le `NkBlendMode` (`NK_ADDITIVE` pour la lumière émissive), le `NkSimMode` (CPU pour peu de
particules, GPU pour des nuées), `maxParticles`, `worldSpace` et `loop`. On gère par
`CreateEmitter`/`DestroyEmitter`, on suit la source (`SetEmitterPos`), on coupe (`SetEmitterEnabled`),
on **déclenche une rafale** (`Burst(id, count)`), on ajuste à la volée (`GetEmitterDesc`).

La **traînée** (`NkTrailDesc` : largeur, durée de vie, points max, couleurs début/fin, texture,
fusion, distance min entre points) est une bande générée en poussant des positions successives
(`AddTrailPoint`) — le sillage d'un missile, la lame d'une arme, une comète ; `ClearTrail` la vide.
Le **decal** (`NkDecalDesc` : transform de projection, textures albedo+normal, opacité, mélange de
normale, durée de vie `-1`=permanent, `fadeOut`, `layerMask`) projette une image sur la géométrie :
impacts, traces de pas, salissures, marquages.

Le système se pilote par `Update(dt, cam)` (avance la simulation, oriente les billboards selon la
caméra) puis `Render(cmd, cam)` (soumet les draws sur le `NkICommandBuffer`). `GetActiveParticleCount`
et `GetActiveEmitterCount` donnent des stats de budget. Usages typiques : **rendu/ambiance** (feu,
fumée, brume, étincelles), **gameplay** (explosions par `Burst`, projectiles, sang), **2D** (effets
de coup, particules d'UI). Piège d'ownership : `DestroyEmitter`/`DestroyTrail`/`DestroyDecal` (et
`SpawnDecal` côté création) manipulent l'ID **par référence** et l'invalident — repartez d'un nouvel
ID.

### `NkSimulationRenderer` — personnages émotifs (stub)

Module de spec pour la simulation : visages expressifs, foules, agents. Le `NkEmotionState` (8
champs 0..1) est la couche d'**humeur** ; le `NkBlendShapeState` (25 curseurs FACS — sourcils, yeux,
mâchoire, bouche, joues, nez, langue — plus `blush`/`pallor`/`sweat`/`tears`) est la couche
**musculaire** qui se traduit en déformation et en *shading* de peau. On soumet :

- **Personnage** — `SubmitCharacter(mesh, skinMaterial, transform, boneMatrices, boneCount,
  blendShapes, emotion)` : un acteur skinné dont l'expression et l'humeur pilotent le rendu (cœur du
  cas PV3DE : patient virtuel émotif).
- **Foule** — `SubmitCrowd(agents, count)` où chaque `AgentDesc` (structure imbriquée
  `NkSimulationRenderer::AgentDesc`) ajoute `velocity` et `age` à un personnage : rendu de masse
  d'agents.
- **Peau** — `UpdateSkinParams(skinMat, blendShapes, emotion)` met à jour les paramètres de peau
  d'un matériau hors d'une soumission complète.

À ce stade c'est une surface d'API (`Init`/`Shutdown` + soumissions) sans le moteur de simulation
complet derrière — à brancher quand PV3DE démarre.

### `NkAIRenderingSystem` — readback asynchrone pour l'inférence

Le système gère des **cibles** (`NkAIRenderingTarget`), chacune décrite par un `NkAITargetDesc` :
résolution, `channels` (`NkAIChannel` en **bitmask** — composez avec `operator|`, testez avec
`NkAIHas`), `format` (`NkAIDataFormat`), `ringSize` (profondeur d'asynchronisme), `sync` (bloquant
pour debug/offline), `callback`, `name`, et `customTex` si `NK_AI_CUSTOM`. Le **format** suit le
consommateur : `NK_AI_UINT8` pour l'affichage simple, `NK_AI_FP16` pour ONNX/TensorRT, `NK_AI_FP32`
pour OIDN ou la précision maximale.

Le flux par frame : on `Capture(cmd, colorTex, depthTex, normalTex, albedoTex, motionTex,
instanceTex)` — chaque canal non demandé reste `NkTexHandle::Null()` —, puis, après le Submit de la
frame, `FlushReadbacks()` (global) ou `target->Flush()` (local) déclenche les callbacks des
readbacks **prêts**. Le callback reçoit un `NkAIFrame` : index, dimensions, canaux, format,
timestamp, `strideBytes`, et les **pointeurs CPU** par canal (`colorData`, `depthData`, `normalData`,
`albedoData`, `motionData`, `instanceIdData`, `customData`). Ces pointeurs ne vivent **que le temps
du callback** — copiez pour persister. `WaitAll()` bloque jusqu'à la fin de tous les readbacks (mode
sync), `Enable`/`IsEnabled` activent la cible, `SetCallback` (re)câble le retour,
`GetFrameCount`/`GetTargetCount` donnent des stats, et `IsAsyncReadbackSupported()` indique si le
backend autorise l'async (sinon, repli synchrone).

Usages, par domaine :

- **Rendu / post** — alimenter un débruiteur (OIDN) ou un upscaler (DLSS-like) avec
  couleur+profondeur+normale+motion.
- **GPU / compute** — fournir des buffers structurés à un réseau qui tourne en compute.
- **Outils / éditeur** — capturer des passes (albedo, instance ID) pour le debug visuel ou
  l'annotation.
- **IA / vision** — générer des jeux de données rendus pour entraîner ou inférer (segmentation,
  estimation de profondeur), `instanceIdData` servant de masque de vérité-terrain.

### Les idiomes transversaux

- **Init/Shutdown obligatoire.** Tous les systèmes ont un ctor `= default` puis `Init(NkIDevice*,
  …)` ; un `NkIDevice*` de NKRHI est requis partout. Toujours `Init` avant usage, `Shutdown` à la
  fin.
- **Create/Destroy = ownership moteur.** Tout objet (`NkAnimationClip`, `NkAnimationPlayer`,
  `NkIKRig`, émetteurs/trails/decals, cibles IA) est alloué par le moteur (NKMemory) — **jamais**
  `new`/`delete` côté appelant. Les `Destroy*` qui prennent un `T*&` ou un `Id&` **nullifient /
  invalident** l'argument : ne le réutilisez pas.
- **Pointeurs non-owning.** Un `NkAnimationPlayer` tient un `const NkAnimationClip*` sans le
  posséder ; les clips doivent survivre aux players.
- **Données CPU éphémères.** Les pointeurs d'un `NkAIFrame` ne sont valides que pendant le callback ;
  le readback arrive avec plusieurs frames de latence (sauf `sync=true`).
- **Ordre par frame.** Animation `Update(dt)` → IK `Solve(dt)` (après l'anim, lit/écrit les bone
  matrices) → VFX `Update` puis `Render(cmd)` → AI `Capture(cmd, …)` puis `FlushReadbacks()` en fin
  de frame, après le Submit.
- **À ignorer.** Les blocs d'usage en commentaire des headers (surcharge `AddChain("name", {…},
  solver)`, certains *designated-initializers*) ne correspondent pas tous aux signatures réelles ;
  `NkInterpMode::NK_CUBIC` est déclaré mais non distinct en interne.

---

### Exemple

```cpp
#include "NKRenderer/Tools/Animation/NkAnimationSystem.h"
#include "NKRenderer/Tools/IK/NkIKSystem.h"
#include "NKRenderer/Tools/VFX/NkVFXSystem.h"
#include "NKRenderer/Tools/AIRendering/NkAIRenderingTarget.h"
using namespace nkentseu::renderer;

// 1. Init (ordre des dépendances) — chaque système prend le NkIDevice de NKRHI.
NkAnimationSystem    anim;  anim.Init(device, r3d);
NkIKSystem           ik;    ik.Init(device, &anim);
NkVFXSystem          vfx;   vfx.Init(device, texLib, meshSys);
NkAIRenderingSystem  ai;    ai.Init(device);

// 2. Setup : un personnage qui marche, IK des pieds, une torche, un readback IA.
NkAnimationPlayer* walker = anim.CreatePlayer("hero");
walker->SetClip(anim.FindClip("walk"));
walker->Play(NkPlayMode::NK_LOOP);

NkIKRig* rig = ik.CreateRig(heroSkeletonId);   // chaînes de jambes ajoutées via AddChain

NkEmitterDesc fire; fire.shape = NkEmitterShape::CONE; fire.blend = NkBlendMode::NK_ADDITIVE;
NkEmitterId torch = vfx.CreateEmitter(fire);

NkAITargetDesc aid;
aid.channels = NkAIChannel::NK_AI_COLOR | NkAIChannel::NK_AI_DEPTH;
aid.callback = [](const NkAIFrame& f){ /* copier f.colorData : valide ICI seulement */ };
NkAIRenderingTarget* aiTarget = ai.Create(aid);

// 3. Boucle par frame : ANIMATION → IK → VFX → AI.
anim.Update(dt);
NkMat4f world = anim.ApplyTransform(*walker, baseWorld);

rig->SetTarget(legChain, footTarget);
ik.Solve(dt);                                  // après l'anim : lit/écrit les os

vfx.SetEmitterPos(torch, handPos);
vfx.Update(dt, cam);
vfx.Render(cmd, cam);

aiTarget->Capture(cmd, sceneColor, sceneDepth);
// ... Submit de la frame ...
ai.FlushReadbacks();                           // déclenche le callback des readbacks prêts
```

---

[← Index NKRenderer](README.md) · [Récap NKRenderer](../NKRenderer.md) · [Couche Runtime](../README.md)
