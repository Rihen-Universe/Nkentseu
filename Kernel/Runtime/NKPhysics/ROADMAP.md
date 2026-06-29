# NKPhysics — ROADMAP

> Module de **dynamique du corps rigide 2D + 3D** (Runtime), **zéro-STL**, posé sur
> **NKCollision** (détection) qu'il consomme pour résoudre les contacts. Namespace
> `nkentseu::physics`. Conçu comme une **bibliothèque de simulation pure, sans ECS** :
> elle manipule des *bodies*, pas des entités. L'intégration gameplay (composants
> `NkRigidBodyComponent` / `NkColliderComponent`, synchro transform) se fait **dans
> Noge**, jamais ici (NKECS reste l'ECS générique de base).
>
> État : **SCAFFOLD** (structure + specs + roadmap). Aucune simulation encore.
> Dernière mise à jour : 2026-06-29.

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
| **M4** Correction positionnelle (Baumgarte / split-impulse) | ⏳ | M | P1 |
| **M5** Types static / kinematic + masses infinies | ⏳ | S | P1 |
| **M6** Îlots (islands) + mise en sommeil (sleeping) | ⏳ | L | P1 |
| **M7** Articulations (distance/revolute/prismatic/weld) | ⏳ | XL | P2 |
| **M8** CCD (corps rapides) via `NkWorld::SweepBody` | ⏳ | M | P2 |
| **M9** Sous-pas (sub-stepping) + déterminisme | ⏳ | M | P2 |
| **M10** Requêtes physiques (raycast/overlap filtrés) + triggers | ⏳ | S | P2 |
| Pont Noge (`NkRigidBodyComponent`, synchro) | 🚫 (hors module) | M | P1 |

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

## En cours

- **M4** — correction positionnelle propre (split-impulse) pour éliminer le résidu
  d'enfoncement sans injecter d'énergie.

---

## À venir (jalons détaillés)

### M4 — Correction positionnelle
- Anti-enfoncement : Baumgarte (biais proportionnel à la pénétration) **ou** split-impulse
  (pseudo-vitesses) pour ne pas injecter d'énergie. Slop + facteur de correction.

### M5 — Static / Kinematic
- `STATIC` (masse ∞, ne bouge pas), `KINEMATIC` (piloté, ignore les forces, pousse les
  dynamiques). Masses inverses = 0 ⇒ le solveur les traite naturellement.

### M6 — Îlots & sommeil
- Regrouper les corps en contact/joints en **îlots** ; endormir un îlot dont toutes les
  vitesses sont sous seuil pendant T (perf + stabilité). Réveil au contact.

### M7 — Articulations (joints)
- `NkDistanceJoint`, `NkRevoluteJoint`, `NkPrismaticJoint`, `NkWeldJoint`, `NkMouseJoint`
  (debug). Mêmes contraintes séquentielles + warm-start. Moteurs/limites.

### M8 — CCD
- Corps rapides : détecter le TOI via **`collision::NkWorld::SweepBody`** (déjà livré),
  avancer le corps au TOI, re-résoudre. Évite le tunneling.

### M9 — Sous-pas & déterminisme
- `Step` à pas fixe + accumulateur (sous-pas) ; ordre d'itération stable ; pas de
  dépendance à `Date.now()`/random ⇒ reproductible.

### M10 — Requêtes physiques
- Raycast/Overlap/ShapeCast **filtrés par groupe/masque/type de corps**, renvoyant le
  `NkRigidBody`. Triggers (corps `trigger` NKCollision) → callbacks enter/stay/exit
  remontés au niveau physique.

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
- Applications (NkAnima pour la physique d'animation, jeux, sandbox).

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
