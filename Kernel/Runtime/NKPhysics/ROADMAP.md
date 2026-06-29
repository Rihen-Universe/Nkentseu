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
| `NkPhysicsWorld` (API : Add/Remove body, Step) — spec | 🔶 | M | P0 |
| **M0** Intégration semi-implicite (gravité + vitesses) | ⏳ | M | P0 |
| **M1** Solveur de contacts (impulses séquentielles, normale) | ⏳ | L | P0 |
| **M2** Frottement (impulses tangentes) + restitution | ⏳ | M | P0 |
| **M3** Warm-starting (via `NkContactPoint::id` NKCollision) | ⏳ | M | P1 |
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

## En cours

- **`NkPhysicsWorld`** : surface publique posée, implémentation `.cpp` à écrire (M0).

---

## À venir (jalons détaillés)

### M0 — Intégration & boucle (le squelette qui bouge)
- `NkPhysicsWorld::Step(dt)` : (1) intégrer les forces → vitesses (gravité, damping,
  semi-implicite Euler), (2) déléguer la **broadphase + narrowphase à NKCollision**
  (le monde possède un `collision::NkWorld` interne, les `NkRigidBody` référencent un
  body de collision par id), (3) intégrer les vitesses → positions, (4) re-synchroniser
  les shapes de collision (`SetShape`). Sans solveur : les corps tombent et se
  traversent. Démo : une bille en chute libre (vérifier `y(t) = ½ g t²`).

### M1 — Solveur de contacts (impulses séquentielles, normale)
- Pour chaque `NkCollisionPair` (manifold multi-points NKCollision) : construire des
  contraintes de contact (masse effective, biais), itérer N fois la résolution
  d'impulses normales (non-pénétration). Static/dynamique via masses inverses.
  Démo : une caisse tombe et **repose** sur un sol statique (2D et 3D).

### M2 — Frottement + restitution
- Impulses **tangentes** bornées par `μ·impulseNormale` (cône de Coulomb), 1 axe (2D)
  ou 2 axes (3D). Restitution (rebond) appliquée au biais de vitesse. Matériaux
  combinés via `NkPhysicsMaterial`.

### M3 — Warm-starting
- Réutiliser les impulses accumulées de la frame précédente en **matchant les points
  par `NkContactPoint::id`** (déjà fourni par NKCollision via `GetPreviousManifold`).
  Convergence nettement plus rapide ; empilements stables.

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
