# NKPhysics — ROADMAP

> Module de **dynamique du corps rigide 2D + 3D** (Runtime), **zéro-STL**, posé sur
> **NKCollision** (détection) qu'il consomme pour résoudre les contacts. Namespace
> `nkentseu::physics`. Conçu comme une **bibliothèque de simulation pure, sans ECS** :
> elle manipule des *bodies*, pas des entités. L'intégration gameplay (composants
> `NkRigidBodyComponent` / `NkColliderComponent`, synchro transform) se fait **dans
> Noge**, jamais ici (NKECS reste l'ECS générique de base).
>
> État : **M0→M6 livrés** (intégration, contacts, frottement+angulaire, warm-starting,
> split-impulse, static/kinematic, sommeil) — self-test 21/21.
> Dernière mise à jour : 2026-06-29.
>
> ### ⭐ CAP — substrat d'un système type **Cascadeur** (tout corps articulé)
> NKPhysics ne vise pas que les jeux : il doit fournir le **substrat physique d'animation
> assistée façon Cascadeur**, pour **n'importe quel corps articulé** — humanoïde, **animal /
> quadrupède**, créature, oiseau, rig mécanique… (rien d'humanoïde-spécifique : tout repose
> sur une **hiérarchie générique d'os + joints**). Concrètement, en plus du corps rigide
> générique, il livre : (1) des **articulations complètes & génériques** (hinge, ball,
> prismatic, weld… avec limites — assemblables en n'importe quel squelette) ; (2) des
> **moteurs / drives PD** → *ragdoll actif* qui **tient et atteint des poses** au lieu de
> s'effondrer ; (3) des outils de **centre de masse + moment angulaire** = la « validation
> physiquement correcte » signature de Cascadeur, valable pour toute morphologie. Le
> **système Cascadeur complet** (IK déjà livré + auto-pose IA + UI) se construit dans
> **NkAnima** PAR-DESSUS NKPhysics — cf. [Applications/NkAnima/ROADMAP.md].

---

## Synthèse

| Composant / Jalon | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| Module `NKPhysics.jenga` + arborescence `src/` + umbrella | ✅ | S | P0 |
| `NkPhysicsTypes` (BodyType, flags, ids, config monde) | ✅ | S | P0 |
| `NkPhysicsMaterial` (densité, friction, restitution) | ✅ | S | P0 |
| `NkRigidBody` (masse/inertie, vitesses, état) — données | ✅ | S | P0 |
| `NkPhysicsWorld` (CreateBody/DestroyBody/GetBody/Step) | ✅ | M | P0 |
| **M0** Intégration semi-implicite (gravité + vitesses) | ✅ | M | P0 |
| **M1** Solveur de contacts (impulses séquentielles, normale) | ✅ | L | P0 |
| **M2** Frottement (cône Coulomb) + angulaire + restitution | ✅ | M | P0 |
| **M3** Warm-starting (via `NkContactPoint::id` NKCollision) | ✅ | M | P1 |
| **M4** Correction positionnelle (split-impulse) | ✅ | M | P1 |
| **M5** Types static / kinematic + masses infinies | ✅ | S | P1 |
| **M6** Mise en sommeil (sleeping) + réveil au contact | ✅ | L | P1 |
| **M7** Articulations — *distance/ball/revolute/weld faits ; reste prismatic* | 🔶 | XL | P2 |
| **M8** Moteurs & **drives PD** sur joints → *ragdoll actif* (tient/atteint une pose) | ⏳ | L | P1 |
| **M9** **Ragdoll générique** : builder squelette (os+joints depuis hiérarchie), self-collision | ⏳ | L | P1 |
| **M10** **Centre de masse + moment angulaire** (validation « physiquement correct » Cascadeur) | ⏳ | M | P1 |
| **M11** CCD (corps rapides) via `NkWorld::SweepBody` | ⏳ | M | P2 |
| **M12** Sous-pas (sub-stepping) + déterminisme | ⏳ | M | P2 |
| **M13** Requêtes physiques (raycast/overlap filtrés) + triggers | ⏳ | S | P2 |
| Pont Noge (`NkRigidBodyComponent`, synchro) | 🚫 (hors module) | M | P1 |
| Système Cascadeur (ragdoll actif + IK + auto-pose IA + UI) | 🚫 → **NkAnima** | XL | — |

Marqueurs : ✅ livré · 🔶 en cours · ⏳ à venir · ❌ bug · 🚫 hors périmètre.

---

## Livré

Scaffold de structure uniquement (pas de simulation) :

- **`NKPhysics.jenga`** : descripteur de module (staticlib C++17), dépend de
  **NKCollision** (+ NKMath/NKContainers/NKMemory/NKLogger/NKCore/NKPlatform). Pas
  encore enregistré dans `Nkentseu.jenga` (scaffold — à activer au jalon M0).
- **`src/NKPhysics/NkPhysicsTypes.h`** : `NkBodyType` (STATIC/KINEMATIC/DYNAMIC),
  `NkBodyFlags`, `NkPhysicsConfig` (gravité, itérations solveur, sleep), ids.
- **`src/NKPhysics/NkPhysicsMaterial.h`** : densité, friction (statique/dynamique
  combinées), restitution + règles de combinaison (moyenne/min/max/multiply).
- **`src/NKPhysics/NkRigidBody.h`** : masse + masse inverse, inertie + inverse,
  vitesses linéaire/angulaire, position/orientation, amortissement, gravityScale,
  flags ; helpers `ApplyForce/ApplyImpulse/SetMass`. **Données seulement.**
- **`src/NKPhysics/NkPhysicsWorld.h`** : interface du monde (spec) — `CreateBody`,
  `DestroyBody`, `Step(dt)`, accès aux contacts, requêtes déléguées à NKCollision.
- **`src/NKPhysics/NkContactSolver.h`**, **`NkIntegrator.h`** : interfaces (spec).
- **`src/NKPhysics/NKPhysics.h`** : umbrella.

---

## Livré — M0 (2026-06-29, self-test 4/4)

- **`NkPhysicsWorld`** implémenté (`NkPhysicsWorld.cpp`) : `CreateBody` (masse+inertie
  diagonale depuis forme+densité — sphère/box exacts, autres via AABB ; static/kinematic
  ⇒ invMass=0), `DestroyBody`, `GetBody`, `Step(dt)`.
- **`Step`** : forces→vitesses (`NkIntegrator`, gravité + damping, Euler symplectique) →
  `collision.Step()` (broadphase DBVH + manifolds, prêts pour M1) → vitesses→positions →
  re-sync des shapes NKCollision (`SetShape`). Sans solveur : les corps tombent (et se
  traversent — c'est M1).
- **Module enregistré** (`Nkentseu.jenga` + `config/modules.jenga`) ; NKPhysics.lib build OK.
- **Self-test** (`tests/test_physics.cpp`, NKLogger) : chute libre conforme à l'Euler
  symplectique (`y≈5.013`, `vy≈-9.81` après 1 s), statique immobile, `gravityScale=0`.

## Livré — M1 (2026-06-29, self-test 8/8)

- **`SolveContacts(dt)`** : une contrainte par `NkCollisionPair`, masse effective normale
  `1/(invMassA+invMassB)`, biais **Baumgarte** (anti-enfoncement) + restitution, puis
  **itérations séquentielles** (Gauss-Seidel, `velocityIters`) d'impulses normales
  accumulées (≥ 0). Triggers ignorés. Modèle linéaire (point-masse) — angulaire en M2.
- **Démo** : bille **et** caisse tombent et **reposent** sur un sol statique (`y≈1.5`,
  `vy≈0`, ne traversent pas).

## Livré — M2 (2026-06-29, self-test 11/11)

- **Termes angulaires** : masse effective `1/(invMA+invMB + (rA×d)·invIA·(rA×d) +
  (rB×d)·invIB·(rB×d))` ; inertie inverse en repère **monde** via
  `NkInvInertiaApply` (R·diag·Rᵀ par quaternion) ; impulses appliquées aussi à la
  vitesse **angulaire** (`ω ± invI·(r×P)`).
- **Frottement** : 2 axes tangents orthonormés à n, impulses tangentes bornées par
  le **cône de Coulomb** (`|impTangent| ≤ μ·impNormale`), résolues avant la normale.
- **Restitution** affinée (vitesse relative au point, seuil anti-jitter).
- **Démo** : caisse lancée à 3 m/s s'arrête par frottement ; bille (e=0.8) rebondit.

## Livré — M3 (2026-06-29, self-test 14/14)

- **Cache d'impulses** `mWarm` (clé : ids des 2 corps + `NkContactPoint::id` NKCollision) :
  en fin de `SolveContacts`, les impulses normale/tangentes accumulées sont sauvegardées ;
  à la frame suivante, chaque point retrouve son impulse (même feature) et la **ré-applique
  AVANT d'itérer** (warm-start). Convergence accélérée + empilements stables.
- **Démo** : une **pile de 3 caisses** tient (bas ~1.5, haut ~3.5, au repos) au lieu de
  s'enfoncer / s'effondrer.

## Livré — M4 (2026-06-29, self-test 16/16)

- **Split-impulse** (`CorrectPositions`) : le biais de Baumgarte est **retiré** du solveur
  de vitesse (plus d'injection d'énergie) ; une passe de **projection positionnelle**
  pousse les corps en chevauchement le long de la normale (`factor·(depth-slop)/(invMA+invMB)`,
  réparti par masse inverse) directement sur la **position**. La pénétration converge vers
  `slop` sur quelques frames. Re-sync absolu des shapes via `NkShapeCenter3D`.
- **Démo** : caisse au repos précise (`y≈1.5` à ±0.03, pénétration ≈ slop) et **propre**
  (`vy≈0`, aucun rebond résiduel).

## Livré — M5 (2026-06-29, self-test 19/19)

- **KINEMATIC** : `invMass=0` (ni gravité ni réponse aux impulses) mais **intègre sa
  position** depuis sa vitesse imposée (`Step` intègre la position des DYNAMIC **et**
  KINEMATIC ; STATIC jamais). Il **pousse** les dynamiques car sa vitesse entre dans la
  vitesse relative du contact. API `SetLinearVelocity` / `SetAngularVelocity`.
- **Démo** : une plateforme kinematic montante (1 m/s) **porte** une caisse dynamique vers
  le haut (la caisse suit, reste posée).

## Livré — M6 (2026-06-29, self-test 21/21)

- **Mise en sommeil par corps** (`UpdateSleep`) : un corps dynamique dont les vitesses
  lin./ang. restent sous `linearSleepTol`/`angularSleepTol` pendant `sleepTime` reçoit le
  flag `NK_BODY_SLEEPING` (vitesses mises à zéro) → exclu de l'intégration et du solveur.
- **Réveil au contact** (`WakeContacts`) : un corps endormi en contact avec un
  **perturbateur** (dynamique éveillé en mouvement, ou kinematic mobile) est réveillé.
  Les contacts avec un corps **statique** ne réveillent pas (un corps posé peut dormir).
- Le solveur/correction traitent un dormeur comme **immovable** (invMass effective = 0) ;
  paires « deux dormeurs » ignorées.
- **Démo** : une caisse posée **s'endort** ; une 2ᵉ caisse lâchée dessus la **réveille**.
- *Note* : sommeil par-corps (pas d'îlots union-find) — le réveil se propage de proche en
  proche frame à frame (suffisant ; les îlots groupés sont une optimisation ultérieure).

## Livré — M7 cut 1 (2026-06-29, self-test 26/26)

- **`NkJoint`** générique (`NkJoint.h`) : type, 2 corps, **ancres locales** (repère du corps),
  warm-start. Aucune notion de morphologie.
- **Joints implémentés** : **DISTANCE** (garde 2 ancres à une longueur cible — corde,
  entretoise) et **BALL / point-à-point** (3 DOF, les 2 ancres coïncident — épaule, hanche,
  attache de membre). Résolus dans `SolveJoints` (contraintes séquentielles décomposées par
  axe, masse effective `NkEffMass` avec termes angulaires, biais de position, **warm-start**).
- **API** : `CreateDistanceJoint(a,b,ancreA,ancreB)`, `CreateBallJoint(a,b,pivot)`,
  `DestroyJoint`. Réveille le corps partenaire (un joint transmet le mouvement).
- **Démo** : pendule à distance (longueur maintenue à 2.00003, descend vers l'équilibre) ;
  ball joint qui **suspend** un corps au pivot (ne tombe pas).

## Livré — M7 cut 2 (2026-06-29, self-test 30/30)

- **REVOLUTE** (charnière 1 DOF = coude, genou, trappe, roue) : point-à-point + **alignement
  d'axe** (2 DOF angulaires verrouillés via couples purs, 1 rotation libre autour de l'axe).
  `CreateRevoluteJoint(a,b,pivot,axe)`.
- **Solveur point-à-point durci** : remplacé la résolution décomposée par-axe (instable quand
  l'ancre est loin du COM — cas des membres : le corps « flinguait » vers le haut) par une
  **résolution de bloc 3×3 exacte** (`NkPointMassCol` + `NkSolve3` Cramer). Stable, convergence
  en 1 itération. Bénéficie aussi au BALL joint.
- **Démo** : une trappe à charnière (axe Z) **bascule sous la gravité dans son plan** (z reste
  ~0, pivot tenu à ~1).

## Livré — M7 cut 3 (2026-06-29, self-test 32/32)

- **WELD** (soudure rigide) : point-à-point (bloc 3×3) **+ verrou d'orientation** (3 DOF
  angulaires, bloc 3×3 sur `invIA+invIB`, erreur = axe-angle de `refRotation⁻¹·(qA⁻¹·qB)`).
  `CreateWeldJoint(a,b,pivot)` mémorise l'orientation relative cible. Warm-start linéaire + couple.
- **Démo** : une caisse **soudée** à une ancre statique reste **figée** en position **et**
  orientation (ne tombe pas, ne tourne pas) au lieu de chuter.

## En cours

- **M8** — **moteurs / drives PD** (couple vers une cible) **+ limites** d'angle sur revolute
  (anti-hyperextension) = **ragdoll actif** (cœur Cascadeur). *(PRISMATIC reporté — peu utile
  pour les créatures, surtout mécanique.)*

---

## À venir (jalons détaillés)

### M7 — Articulations génériques (joints) + limites
- `NkDistanceJoint` (longueur cible/min-max), `NkRevoluteJoint` (pivot 1 DOF = coude, genou,
  charnière, roue) **+ limites d'angle**, `NkPrismaticJoint` (glissière 1 DOF = piston,
  ascenseur) **+ limites**, `NkWeldJoint` (soudure rigide), `NkBallJoint`/spherical (3 DOF =
  épaule, hanche, base de cou, attache de membre) **+ cône-limite**, `NkMouseJoint` (debug).
- Implémentation **homogène aux contacts** : contraintes séquentielles (masse effective,
  biais, warm-start) résolues dans le même solveur. **Aucune notion de morphologie** : ce
  sont des primitives génériques — un humanoïde, un quadrupède, un oiseau ou un bras
  robotisé ne sont que des **graphes différents** de ces mêmes joints.

### M8 — Moteurs & drives PD (ragdoll ACTIF)
- Sur chaque joint : **moteur** (vitesse cible + couple/force max) et **drive PD** (angle/
  position cible + raideur `kp` + amortissement `kd` → couple). C'est ce qui transforme un
  ragdoll *passif* (qui s'effondre) en ragdoll *actif* qui **tient une pose** et **se déplace
  vers une pose cible** — le cœur de l'assistance physique d'animation.

### M9 — Ragdoll générique (toute morphologie)
- `NkRagdoll` : construit un ensemble corps-rigides + joints **depuis une hiérarchie d'os
  arbitraire** (pas de schéma humanoïde figé) → humanoïde, animal/quadrupède, créature,
  mécanique. Gestion **self-collision** (masque par paire), mapping bone↔body, application
  des limites anatomiques par joint. Sert directement NkAnima (couplage ragdoll ↔ skin).

### M10 — Centre de masse & moment angulaire (validation Cascadeur)
- Outils d'analyse **physiquement correcte** : centre de masse global/par-membre, **moment
  angulaire total**, support polygon / équilibre, trajectoire balistique du COM. C'est la
  signature de Cascadeur (vérifier qu'un saut/une rotation respecte la conservation du
  moment) — fourni ici comme **requêtes** sur le monde/ragdoll, réutilisables pour toute
  morphologie. Exposé à NkAnima pour la validation et l'auto-correction de pose.

### M11 — CCD
- Corps rapides (`NK_BODY_CCD`) : détecter le TOI via **`collision::NkWorld::SweepBody`**
  (déjà livré), avancer le corps au TOI, re-résoudre. Évite le tunneling.

### M12 — Sous-pas & déterminisme
- `Step` à pas fixe + accumulateur (sous-pas) → chaînes de joints raides (squelettes) stables ;
  ordre d'itération stable ; pas de dépendance à `Date.now()`/random ⇒ reproductible.

### M13 — Requêtes physiques
- Raycast/Overlap/ShapeCast **filtrés par groupe/masque/type de corps**, renvoyant le
  `NkRigidBody`. Triggers (corps `trigger` NKCollision) → callbacks enter/stay/exit.

---

## Cap « système type Cascadeur » — qui fait quoi

| Brique | Module | Statut |
|--------|--------|--------|
| Corps rigides + contacts + frottement + sommeil | NKPhysics M0–M6 | ✅ |
| Joints génériques + limites (tout squelette) | NKPhysics **M7** | ⏳ |
| Moteurs / drives PD → **ragdoll actif** (tient/atteint une pose) | NKPhysics **M8** | ⏳ |
| Ragdoll générique (humanoïde/animal/créature) + self-collision | NKPhysics **M9** | ⏳ |
| Centre de masse + moment angulaire (validation) | NKPhysics **M10** | ⏳ |
| **IK** (chaînes, FABRIK) | NkAnima | ✅ (M0 IK livré) |
| Couplage ragdoll ↔ squelette skinné, retargeting | NkAnima | ⏳ |
| **Auto-pose / auto-physique par IA** (style Cascadeur) | NkAnima (+ NKAI futur) | ⏳ |
| UI éditeur d'animation (timeline, manip, validation visuelle) | NkAnima / Editor Kit | ⏳ |

> **NKPhysics** fournit le *moteur* (ragdoll actif générique + analyse physique) ; le
> *produit* type Cascadeur s'assemble dans **NkAnima**, valable pour toute morphologie.

---

## Bugs

- Aucun (pas encore de code exécutable).

---

## Dépendances

- **NKCollision** (détection, manifolds multi-points, broadphase DBVH, raycast/overlap/
  **SweepBody** CCD, `GetPreviousManifold` pour le warm-start) — *le socle*.
- **NKMath** (Vec2/3, Quat, Mat, fonctions), **NKContainers** (NkVector), **NKMemory**
  (allocateurs), **NKLogger** (diagnostics), **NKCore**/**NKPlatform** (types/portabilité).
- **Aucune dépendance ECS / rendu / fenêtrage** (lib de simulation pure).

### Consommateurs (futurs)
- **Noge** : `NkRigidBodyComponent` + `NkColliderComponent` (pont ECS → physics), système
  de synchro `Transform ↔ NkRigidBody`. C'est **là** que se fait l'intégration ECS.
- **NkAnima** : assemble le **système type Cascadeur** sur NKPhysics (ragdoll actif
  générique + COM/moment) + son IK (déjà livré) + auto-pose IA + UI — pour **toute
  morphologie** (humanoïde, animal, créature, mécanique).
- Jeux / sandbox (gameplay physique générique).

---

## Principes de conception

1. **Zéro-STL** : uniquement modules Nkentseu (cf. NKCollision). Allocations via NKMemory.
2. **ECS-free** : le monde physique est autonome ; le binding entité↔body vit dans Noge.
3. **S'inspirer de Box2D / Bullet** (solveur séquentiel à impulses, warm-start, îlots)
   **puis améliorer** — pas copier (cf. règle NKGui).
4. **2D et 3D** dès le départ (NKCollision les fournit tous les deux).
5. **Testé par vague** : self-test from-scratch (NKLogger, pas printf) à chaque jalon,
   validé par compilation/link/exécution manuelle (le `jenga test` est désactivé en
   politique workspace).
