# La détection de collisions

> Couche **Runtime** · NKCollision · Trouver **qui touche quoi** : le tri grossier des paires
> (`Broadphase`), le test fin par paire (`Narrowphase`, GJK/EPA/SAT), le cache de contacts
> persistants (`ContactCache`) et la détection continue anti-tunneling (`CCDManager`).

Détecter une collision, ce n'est jamais « tester tout le monde contre tout le monde ». Avec mille
corps dans une scène, la naïveté coûte un million de tests par frame — intenable. La détection
moderne se fait donc en **étages**, du plus grossier au plus fin, chacun jetant le travail inutile
avant l'étage suivant. C'est tout le sujet de cette page : **comment passer d'une soupe de corps à
une liste exacte de points de contact**, sans rien tester de superflu.

Le pipeline tient en quatre temps. D'abord le **broadphase** ne regarde que des boîtes englobantes
(`AABB3D`) et recrache une poignée de **paires candidates** — celles dont les boîtes se chevauchent.
Ensuite le **narrowphase** prend chaque paire candidate et fait le vrai test géométrique : se
touchent-elles vraiment, et si oui, **où** et **avec quelle pénétration** ? Le résultat est un
*manifold* de contacts. Puis le **cache persistant** suit ces contacts d'une frame à l'autre pour
réutiliser les impulsions du solveur (*warm-starting*). Enfin, pour les objets rapides, la
**détection continue** (CCD) attrape les collisions qui se produiraient *entre* deux frames — la
balle qui traverse le mur sans jamais y être posée.

Une mise en garde franche avant de plonger : **ce module est un prototype greenfield, hors des
conventions Nkentseu**. Le namespace est `col` (pas `nkentseu::…`), les types n'ont **pas** de
préfixe `Nk`, et le code s'appuie massivement sur la **STL** (`std::vector`, `std::unordered_map`,
`std::function`…) plutôt que sur NKMemory. Plusieurs comportements sont **incomplets** (le mode
`Grid`, la rotation symétrique du BVH, `qualityRatio()`) et sont signalés explicitement. Ce n'est
**pas** du code de production : c'est une base de travail à refactorer le jour de l'intégration.

- **Namespace** : `col`
- **Headers** : `NkBroadPhase.h`, `NkNarrowPhase.h`, `NkPersistentContacts.h`, `CCD.h`
- **Dépendances internes** : `shapes.h` (fournit `Vec2`/`Vec3`, `AABB3D`, `OBB3D`/`OBB2D`,
  `Sphere`, `Capsule3D`, `Circle2D`, `ConvexPolygon2D`, `Ray`, `Transform`, `Quat`,
  `CollisionShape`, `CollisionBody`, `BodyFlag_CCD`…) et `narrowphase.h`.

---

## Le broadphase : trier les paires candidates

Le premier étage répond à une seule question : **quelles paires de corps valent la peine d'être
testées finement ?** Plutôt que de comparer les formes réelles — chères —, on compare leurs
**boîtes englobantes alignées** (`AABB3D`), bon marché. Deux corps dont les boîtes ne se chevauchent
même pas ne peuvent pas se toucher : on les écarte sans réfléchir. Le broadphase recrache donc une
liste de `CollidingPair` — les seules paires dont les boîtes se chevauchent.

NKCollision propose deux algorithmes pour ce tri, réunis derrière une façade `Broadphase`.

Le premier, `DynamicBVH`, est un **arbre de volumes englobants dynamique** (un DBVT à la Bullet).
Chaque corps est une feuille ; les nœuds internes englobent leurs enfants. Pour savoir quelles
paires se chevauchent, on descend l'arbre en élaguant les branches qui ne se touchent pas — d'où un
coût en `O(log n)` au lieu de `O(n)` par requête. Deux raffinements le rendent rapide en pratique :
les boîtes sont **gonflées** d'une marge (*fat AABB*, facteur `0.1`) pour qu'un corps qui bouge un
peu n'oblige pas à réinsérer son nœud à chaque frame ; et le choix du voisin d'insertion se fait par
**SAH branch-and-bound** (*Surface Area Heuristic*), qui garde l'arbre compact. L'arbre s'auto-équilibre
par des rotations de type AVL — **avec une limite réelle** : la rotation du cas symétrique
(`bf < -1`) est un stub vide, donc l'arbre peut rester déséquilibré sur ce cas précis.

Le second, `SweepAndPrune`, est plus simple : il **trie** les boîtes sur leur `min.x`, puis balaye
cette liste de gauche à droite. Tant que deux boîtes se chevauchent en X, il les teste complètement ;
dès qu'elles se séparent en X, il coupe (`break`). C'est excellent quand les corps sont étalés sur un
axe, mais le pire cas reste `O(n²)` (tout empilé au même X), et chaque `update`/`remove` est une
recherche linéaire `O(n)`.

```cpp
col::Broadphase bp{ col::BroadphaseType::DynamicBVH };
bp.insert(bodyId, aabb);                     // une feuille de plus dans l'arbre

std::vector<col::CollidingPair> pairs;
bp.collectPairs(pairs);                      // toutes les paires AABB qui se chevauchent
// → pairs part vers le narrowphase
```

Ce n'est **pas** une détection de collision : une paire candidate veut juste dire « leurs boîtes se
recouvrent ». Deux sphères dont les boîtes se chevauchent peuvent très bien ne pas se toucher. Le
broadphase **réduit** le travail, il ne le **conclut** pas.

> **En résumé.** Le broadphase teste les **AABB** (pas les formes) et sort des `CollidingPair`
> candidates. `DynamicBVH` = arbre `O(log n)`, idéal pour le cas général et les requêtes (`query`,
> `rayQuery`). `SweepAndPrune` = tri sur X + balayage, simple mais `O(n²)` au pire. La façade
> `Broadphase` choisit l'un ou l'autre — mais le mode `Grid` n'est **pas** implémenté et retombe
> sur SAP.

---

## Le narrowphase : tester les formes réelles

Chaque `CollidingPair` arrive maintenant au deuxième étage, qui fait le **vrai** test géométrique.
La question change : non plus « leurs boîtes se touchent-elles ? » mais « **ces deux formes
exactes** se touchent-elles, et si oui, quel est le point de contact, la normale et la profondeur de
pénétration ? ». La réponse est un `ContactManifold` — jusqu'à 4 points de contact pour une paire 3D.

Le narrowphase n'a pas **un** algorithme mais **trois familles**, choisies selon les formes :

- **SAT** (théorème des axes séparateurs) — rapide et exact pour les **primitives** (boîtes,
  sphères, capsules). On cherche un axe sur lequel les projections des deux formes ne se chevauchent
  pas : s'il en existe un, pas de collision ; sinon, l'axe de moindre chevauchement donne la normale
  et la profondeur (le *MTV*, vecteur de translation minimale).
- **GJK** (Gilbert-Johnson-Keerthi) — un test d'intersection **générique** pour formes convexes
  quelconques, fondé sur la différence de Minkowski. Il dit *si* deux formes se touchent et, sinon,
  *à quelle distance*.
- **EPA** (Expanding Polytope Algorithm) — le complément de GJK : quand GJK trouve une intersection,
  EPA reprend son simplexe et l'**étend** jusqu'à trouver la normale et la profondeur de pénétration.

Le dispatcher `Narrowphase::generateContacts` choisit pour vous : il a des *fast-paths* SAT pour les
paires de primitives connues (sphère/sphère, sphère↔AABB, OBB/OBB, capsules…) et **retombe sur
GJK+EPA** pour tout le reste.

```cpp
for (const auto& p : pairs) {
    col::ContactManifold m = col::Narrowphase::generateContacts(
        shapeA, transformA, shapeB, transformB);
    if (m.hit) { /* m.points[0..m.count-1] → solveur */ }
}
```

Attention au piège : le dispatch est **ordre-dépendant**. Sphère/AABB et AABB/Sphère sont tous deux
gérés, mais d'autres permutations (OBB/Sphère, Sphère/Capsule…) **ne le sont pas** et retombent sur
le chemin générique GJK/EPA. Et la variante 2D, `generateContacts2D`, ne gère **que** cercle/cercle
et OBB2D/OBB2D — toute autre paire renvoie un manifold vide.

> **En résumé.** Le narrowphase prend les formes **réelles** et produit un `ContactManifold` (point,
> normale, pénétration). **SAT** pour les primitives (rapide), **GJK** pour tester l'intersection de
> convexes quelconques, **EPA** pour en extraire la pénétration. `Narrowphase::generateContacts`
> dispatche — mais l'ordre des arguments compte, et beaucoup de paires retombent sur GJK+EPA.

---

## Le cache de contacts : se souvenir d'une frame à l'autre

Un solveur de physique est bien meilleur quand il **part de la solution de la frame précédente** au
lieu de repartir de zéro : c'est le *warm-starting*. Encore faut-il **reconnaître** qu'un contact de
cette frame est le « même » que celui d'avant, pour lui réattribuer son impulsion accumulée. C'est
le rôle de `ContactCache` et du `PersistentManifold`.

L'idée : on stocke chaque point de contact **en coordonnées locales** des deux corps. À la frame
suivante, on reprojette ce point local en monde et on regarde s'il a peu bougé — si sa séparation
normale et sa dérive tangentielle restent sous les seuils (`2 cm` et `4 cm`), c'est le même contact,
on **fusionne** et on **garde** son impulsion ; sinon le contact a « cassé » et on le purge. Les
manifolds qu'on ne revoit plus depuis quelques frames (`kMaxAge = 3`) sont évincés.

```cpp
col::ContactCache cache;
// cycle frame :
cache.update(manifold, tA, tB, frame);        // fusionne avec l'historique
float n, t[2];
if (cache.getWarmStart(a, b, 0, n, t))        // impulsions de la frame d'avant
    solver.seed(n, t);
// ... solve ...
cache.storeImpulse(a, b, 0, n, t);            // garde le résultat pour la prochaine frame
cache.evict(frame);                            // jette les contacts trop vieux
```

> **En résumé.** `ContactCache` suit les contacts dans le **temps** pour le *warm-starting* :
> contacts stockés en local, fusionnés tant qu'ils ne dérivent pas trop (`2 cm`/`4 cm`), évincés
> après `kMaxAge` frames. `getWarmStart` avant le solve, `storeImpulse` après, `evict` en fin de
> frame.

---

## La détection continue : attraper ce qui va trop vite

La détection « discrète » teste des **positions figées**, une par frame. Pour un objet rapide, c'est
un piège : à la frame *t* il est devant le mur, à la frame *t+1* il est derrière — il n'a **jamais**
été *dans* le mur, donc aucune collision n'est détectée. Il a **tunnelé**. La détection continue de
collisions (CCD) corrige ça en testant le **mouvement complet** entre deux frames, et en calculant
le **temps d'impact** (TOI) — l'instant `t ∈ [0,1]` où le contact se produit vraiment.

NKCollision offre plusieurs niveaux. `SweptSphere` est le CCD **analytique** le plus rapide
(`O(1)` par paire) : il résout l'équation du mouvement relatif pour sphère/sphère, sphère/AABB et
(conservativement) capsule/sphère. `LinearCast` est plus général : un *shape-cast* fondé sur GJK qui
translate **et** fait pivoter (`slerp`) une forme de sa position de départ à sa position d'arrivée, et
cherche le TOI par **dichotomie**. Au-dessus, `TOISolver` trouve l'impact **le plus précoce** parmi
les paires marquées `BodyFlag_CCD`, et `CCDManager` intègre le tout dans la boucle.

```cpp
col::CCDManager ccd;
// après le broadphase, AVANT le narrowphase standard :
ccd.process(pairs, bodies, dt, contacts);
```

Les limites sont à connaître et **codées en dur** : `dt = 1/60`, restitution `0.3`, masses
supposées égales, le corps B toujours traité comme statique, et `process` ne résout qu'**une seule**
paire (la plus précoce) par appel.

> **En résumé.** La CCD attrape les collisions **entre** deux frames (anti-tunneling) en calculant
> un **temps d'impact** `t ∈ [0,1]`. `SweptSphere` = analytique `O(1)`, `LinearCast` = shape-cast GJK
> par dichotomie (translation + rotation), `TOISolver`/`CCDManager` orchestrent — avec des constantes
> figées (`dt=1/60`, e=0.3, masses égales, une paire par passe).

---

## Aperçu de l'API

Tous les éléments publics des quatre headers, regroupés par fichier. Complexités entre crochets
quand elles éclairent le choix.

### `NkBroadPhase.h` — tri des paires candidates

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Donnée | `struct CollidingPair` { `bodyA`, `bodyB`, `aabbA`, `aabbB` } | Paire dont les **AABB** se chevauchent (sortie broadphase). |
| Donnée | `CollidingPair::operator==` | Égalité **non ordonnée** (A,B == B,A). |
| BVH | `DynamicBVH` | Arbre de volumes englobants dynamique (DBVT, fat AABB). |
| BVH | `kFattenFactor=0.1f`, `kNullNode=-1` | Marge de gonflage prédictif / index nul. |
| BVH | `struct Node` { `aabb`, `bodyId`, `parent`, `children[2]`, `height`, `isLeaf()` } | Nœud de l'arbre. |
| BVH | `insert(bodyId, aabb)` `[~O(log n)]` | Insère un corps (AABB gonflée), renvoie l'index feuille. |
| BVH | `remove(bodyId)` `[O(log n)]` | Supprime (no-op si absent). |
| BVH | `update(bodyId, newAABB, velocity={})` `[O(log n)]` | Met à jour ; `false` si tient dans la fat AABB. |
| BVH | `query(aabb, cb)` `[~O(log n + k)]` | Visite les feuilles chevauchant `aabb` ; `cb` → `false` arrête. |
| BVH | `rayQuery(ray, cb)` | Idem via `aabb.raycast` ; `cb(bodyId, tHit)`. |
| BVH | `collectPairs(pairs)` | Remplit `pairs` de toutes les paires feuille-feuille en overlap. |
| BVH | `nodeCount()`, `height()` | Nœuds vivants / hauteur de l'arbre. |
| BVH | `qualityRatio()` | Métrique SAH — **déclarée seulement** (pas de corps inline). |
| SAP | `SweepAndPrune` | Broadphase tri-sur-X + balayage. |
| SAP | `struct Proxy` { `bodyId`, `aabb` } | Entrée du SAP. |
| SAP | `insert` `[O(1)]`, `remove` `[O(n)]`, `update` `[O(n)]` | Gestion des proxies (marquent `dirty_`). |
| SAP | `collectPairs(pairs)` `[O(n²) pire, O(n·k)]` | **Non-const** : trie si besoin puis collecte les overlaps. |
| Façade | `enum class BroadphaseType` { `DynamicBVH`, `SAP`, `Grid` } | Choix d'algorithme (`Grid` non implémenté). |
| Façade | `Broadphase(type=DynamicBVH)` | Sélectionne BVH ou SAP. |
| Façade | `insert`, `remove`, `update(…, vel={})`, `collectPairs` | Délèguent au backend. |
| Façade | `query`, `rayQuery` | **BVH uniquement** (no-op sinon). |
| Façade | `type()` | Mode courant. |

### `NkNarrowPhase.h` — test fin et contacts

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Contact 3D | `struct ContactPoint` { `positionA`, `positionB`, `normal`, `penetrationDepth`, `restitution`, `friction` } | Un point de contact 3D. |
| Contact 3D | `struct ContactManifold` { `points[4]`, `count`, `hit`, `bodyA`, `bodyB`, `frameStamp` } | Jusqu'à `kMaxContacts=4` contacts. |
| Contact 3D | `ContactManifold::addPoint(cp)`, `clear()` | Ajoute (ignore si plein) / vide. |
| Contact 2D | `struct ContactManifold2D` { `Point{position,penetration}[2]`, `count`, `normal`, `hit` } | Jusqu'à `kMaxContacts=2`. |
| GJK | `struct GJKSimplex` { `pts[4]`, `n`, `push(v)`, `operator[]` } | Simplexe consommé par EPA. |
| GJK | `minkowskiSupport(a, ta, b, tb, dir)` | Point support de la différence de Minkowski (free function). |
| GJK | `GJK3D::test(a, ta, b, tb)` → `Result{hit, simplex, distance, closestA/B}` | Test d'intersection 3D (`kMaxIter=64`). |
| EPA | `EPA3D::solve(simplex, a, ta, b, tb)` → `Result{normal, depth, valid}` | Pénétration + normale depuis le simplexe GJK. |
| SAT 3D | `SAT3D::Result` { `hit`, `normal`, `depth` } | Sortie SAT 3D. |
| SAT 3D | `testOBBOBB`, `testAABBAABB`, `testSphereSphere`, `testSphereAABB`, `testSphereOBB`, `testCapsuleSphere`, `testCapsuleCapsule` | Tests par primitives (statiques). |
| SAT 2D | `SAT2D::Result` { `hit`, `normal`, `depth` } | Sortie SAT 2D. |
| SAT 2D | `testOBBOBB`, `testCircleCircle`, `testPolygonPolygon` | Tests 2D (statiques). |
| Dispatch | `Narrowphase::generateContacts(a, ta, b, tb)` | Fast-paths SAT, **fallback GJK+EPA** ; ordre-dépendant. |
| Dispatch | `Narrowphase::generateContacts2D(a, b)` | **Uniquement** Circle2D/Circle2D et OBB2D/OBB2D. |

### `NkPersistentContacts.h` — cache et warm-starting

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Contact suivi | `struct PersistedContact` { `localA`, `localB`, `normal`, `depth`, `normalImpulse`, `tangentImpulse[2]`, `frameStamp`, `fresh` } | Contact suivi sur plusieurs frames. |
| Clé | `makePairKey(a, b)` → `uint64_t` | Clé **non ordonnée** ; **tronque** chaque id à 32 bits. |
| Manifold persistant | `struct PersistentManifold` { `bodyA/B`, `contacts[4]`, `count`, `lastActive` } | Jusqu'à `kMaxContacts=4`, suivi dans le temps. |
| Manifold persistant | `kBreakThresholdNormal=0.02f`, `kBreakThresholdTangent=0.04f` | Seuils de rupture (2 cm / 4 cm). |
| Manifold persistant | `update(fresh, tA, tB, frameStamp)` | Fusionne les nouveaux contacts puis purge les cassés. |
| Manifold persistant | `removeBreaking(tA, tB)` | Reprojette en monde, supprime ce qui dérive trop. |
| Manifold persistant | `isActive(currentFrame, maxAge=2)`, `clear()` | Encore actif ? / vider. |
| Cache | `ContactCache` (`kMaxAge=3`) | Stockage global des manifolds persistants (map). |
| Cache | `getOrCreate(a, b)` → `PersistentManifold&` | Récupère/crée (réf. dans la map). |
| Cache | `update(fresh, tA, tB, frameStamp)` | No-op si `!fresh.hit`, sinon délègue. |
| Cache | `evict(currentFrame)` | Supprime les manifolds inactifs > `kMaxAge`. |
| Cache | `getWarmStart(a, b, idx, normalImpulse, tangent[2])` | Lit les impulsions ; `false` si absent/hors-borne/`fresh`. |
| Cache | `storeImpulse(a, b, idx, normalImpulse, tangent[2])` | Stocke après le solve. |
| Cache | `forEach(fn)`, `forEachConst(fn)`, `size()`, `clear()` | Itérations / taille / vidage. |
| Cache | `toContactManifold(pm, tA, tB)` | Reconstruit un `ContactManifold` monde (callbacks). |

### `CCD.h` — détection continue (anti-tunneling)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Résultat | `struct TOIResult` { `toi`, `hit`, `normal`, `pointA`, `pointB` } | Temps d'impact `∈ [0,1]` (1 = pas d'impact). |
| Swept | `SweptSphere::sphereVsSphere(...)` `[O(1)]` | CCD analytique sphère/sphère (quadratique). |
| Swept | `SweptSphere::sphereVsAABB(...)` | Minkowski + ray-cast. |
| Swept | `SweptSphere::capsuleVsSphere(...)` | **Conservateur** : sweep des deux sphères d'extrémité. |
| Cast | `LinearCast::cast(shapeA, tA0, tA1, shapeB, tB)` → `TOIResult` | Shape-cast GJK par dichotomie (`kMaxIter=32`, lerp + slerp). |
| Paire | `struct CCDPair` { `bodyA`, `bodyB`, `result` } | Paire + son TOI. |
| Solveur | `TOISolver::findEarliestImpact(pairs, bodies)` | Impact le plus précoce (corps `BodyFlag_CCD`, `dt=1/60`). |
| Solveur | `TOISolver::resolveStep(a, b, toi, dt)` | Avance au TOI + impulsion élastique (e=0.3, masses égales). |
| Solveur | `kMinTOI=0.001f` | Ignore les TOI sub-milliseconde. |
| Manager | `CCDManager::process(pairs, bodies, dt, contacts)` | Passe CCD/frame (une seule paire résolue). |
| Manager | `CCDManager::sweptAABBsOverlap(a, b, dt)` | Pré-filtre AABB étendues par `velocity*dt`. |

---

## Référence complète

### `CollidingPair` et le contrat du broadphase

`CollidingPair` est la **monnaie d'échange** entre les deux premiers étages : quatre champs
(`bodyA`, `bodyB`, et leurs `aabbA`/`aabbB`). Son `operator==` est **non ordonné** — `(A,B)` égale
`(B,A)` — ce qui évite de tester deux fois la même paire dans les deux sens.

- **Physique** — c'est la liste d'entrée du narrowphase : pour chaque paire on ira chercher les
  formes réelles et générer des contacts.
- **Outils / éditeur** — visualiser les paires candidates (lignes entre boîtes) aide à diagnostiquer
  un broadphase qui produit trop de faux positifs.
- **Threading** — la liste de paires est trivialement parallélisable : chaque paire peut partir au
  narrowphase sur un thread distinct (le broadphase, lui, n'est pas thread-safe en écriture).

### `DynamicBVH` — l'arbre dynamique, à fond

C'est le cœur du broadphase général. Chaque corps vivant est une **feuille** ; les nœuds internes
englobent leurs deux enfants. Le `Node` porte son `aabb`, son `bodyId` (feuilles), un `parent`, deux
`children`, une `height` et un `isLeaf()`. En interne, un **pool de nœuds** avec free-list (le champ
`parent` recyclé comme lien de liste libre) évite les allocations répétées — le constructeur réserve
1024 nœuds d'avance.

`insert(bodyId, aabb)` gonfle l'AABB de `kFattenFactor` (10 %) puis descend choisir le **meilleur
voisin** par SAH branch-and-bound (`findBestSibling`) : on minimise l'augmentation d'aire totale,
ce qui garde l'arbre serré et les requêtes rapides. `update` est le point malin : si le corps bouge
**à l'intérieur de sa fat AABB**, on ne touche à rien (`false` retourné) — c'est ce qui rend le BVH
peu coûteux pour des corps qui frémissent. S'il en sort, on ré-insère avec une AABB **prédictive**,
étendue dans la direction de la `velocity` fournie, pour anticiper le mouvement.

Les requêtes sont la grande force de la structure :

- **Rendu** — `query(aabb, cb)` répond au *frustum culling* grossier ou à un *box query* « quels
  objets dans cette région ? », en `O(log n + k)` au lieu de tout balayer.
- **Gameplay / IA** — `rayQuery(ray, cb)` est le *raycast* de visée, de tir, de ligne de vue : il
  remonte les `bodyId` touchés avec leur `tHit`, le callback rendant `false` pour s'arrêter au
  premier (le plus proche si on garde le min).
- **Physique** — `collectPairs(pairs)` est l'appel principal de la frame : il vide puis remplit la
  liste de toutes les paires feuille-feuille qui se chevauchent (parcours récursif `collectPairsRecursive`
  + `testSubtrees`), prête pour le narrowphase.
- **Outils / éditeur** — `nodeCount()` et `height()` instrumentent la santé de l'arbre ; une hauteur
  qui explose trahit un déséquilibre.

Deux limites **réelles** à garder en tête. La rotation d'équilibrage `balance` ne traite que le cas
`bf > 1` : la branche symétrique (`bf < -1`) est un **stub vide** commenté « symmetric code », donc
l'arbre peut rester déséquilibré dans ce cas. Et `qualityRatio()` (la métrique de qualité SAH) est
**déclarée mais pas définie** dans le header : l'appeler sans `.cpp` provoquera une erreur de lien.

### `SweepAndPrune` — le balayage 1D, à fond

L'alternative simple. Chaque corps est un `Proxy` (`bodyId` + `aabb`). `collectPairs` — **non-const**
car il peut trier — réordonne les proxies par `min.x` (si `dirty_`), puis fait une double boucle qui
**coupe** dès que deux boîtes se séparent en X. C'est efficace quand les corps sont **étalés** le
long d'un axe (un niveau horizontal, une rangée d'ennemis), parce que la coupure élague vite.

- **Physique** — broadphase de remplacement, ou broadphase de choix sur des scènes 1D-dominantes
  (jeux de plateforme, *side-scrollers*).
- **Audio / spatial** — un balayage sur un axe peut servir à trouver les sources sonores proches le
  long d'une direction.
- **Threading** — attention : `collectPairs` mute l'état (le tri), donc ne se partage pas en lecture
  comme une structure const.

Coût : `insert` est `O(1)`, mais `remove` et `update` font une recherche linéaire `O(n)`, et le pire
cas de collecte est `O(n²)` (tout aligné au même X). Sur des scènes 3D denses et homogènes, le BVH
le bat.

### `Broadphase` — la façade

`Broadphase` cache le choix derrière une `BroadphaseType` et **instancie les deux backends en
membres** (`bvh_` et `sap_`), n'en activant qu'un. Les opérations courantes (`insert`/`remove`/
`update`/`collectPairs`) délèguent ; `query` et `rayQuery` ne fonctionnent **qu'en mode DynamicBVH**
(no-op autrement, car le SAP n'expose pas ces requêtes).

- **Gameplay / outils** — pouvoir basculer d'algorithme à la création permet de profiler les deux sur
  une même scène et de choisir au vu des chiffres.

Le **piège** majeur : `BroadphaseType::Grid` est listé dans l'enum mais **non implémenté** — toutes
ses branches (`insert`/`remove`/`update`/`collectPairs`) retombent silencieusement sur le SAP. Ne
comptez pas sur une grille spatiale ici.

### `ContactPoint`, `ContactManifold`, `ContactManifold2D` — le résultat du contact

Quand le narrowphase conclut à une collision, il décrit le contact. Un `ContactPoint` 3D porte les
positions monde sur chaque corps (`positionA`/`positionB`), la `normal` (de A vers B), la
`penetrationDepth`, et les coefficients `restitution` (0 par défaut) et `friction` (0.3). Un
`ContactManifold` regroupe jusqu'à **4** de ces points (`kMaxContacts`), plus un `hit`, les ids des
corps et un `frameStamp`. `addPoint` ajoute tant qu'il reste de la place (**ignore silencieusement**
au-delà de 4) ; `clear()` remet à zéro.

- **Physique** — c'est l'entrée directe du solveur de contraintes : chaque point donne une contrainte
  de non-pénétration (le long de `normal`) et de friction.
- **Gameplay** — `hit` déclenche les réactions (dégâts, son d'impact), `normal` oriente l'effet
  (étincelles), `penetrationDepth` dose la réponse.
- **Audio** — la profondeur et la vitesse relative au contact modulent le volume du son d'impact.
- **2D / UI** — `ContactManifold2D` (jusqu'à 2 points, un `Point` = `position` + `penetration`, une
  `normal` partagée) sert la collision 2D ; **pas** d'`addPoint`/`clear` ici, on remplit les champs
  directement.

Pourquoi 4 points et pas 1 ? Parce qu'une face posée sur une face touche sur une **surface** : le
solveur a besoin de plusieurs points pour empêcher l'objet de basculer.

### `GJK3D`, `GJKSimplex`, `minkowskiSupport` — le test générique

GJK répond à « ces deux convexes se touchent-ils ? » sans connaître leur forme précise, juste leur
**fonction support** (le point le plus loin dans une direction donnée). La clé est la **différence de
Minkowski** : `minkowskiSupport` renvoie `a.support3D(dir) - b.support3D(-dir)` ; les deux formes se
touchent si et seulement si cet ensemble contient l'origine. GJK construit itérativement un
**simplexe** (`GJKSimplex`, jusqu'à 4 points) qui tente d'entourer l'origine, jusqu'à `kMaxIter=64`
itérations.

`GJK3D::test` renvoie un `Result` avec `hit`, le `simplex` (réutilisé par EPA en cas de collision)
et, en cas de non-intersection, une `distance`.

- **Physique** — le filet de sécurité du dispatcher pour toute paire de convexes sans fast-path
  dédié.
- **Gameplay / IA** — la `distance` (quand `!hit`) sert aux requêtes de proximité « à quelle distance
  est l'obstacle le plus proche ? ».
- **Outils** — GJK est aussi la brique d'un *shape-cast* (voir `LinearCast`).

**Limites connues** à respecter : `closestA`/`closestB` ne sont **pas remplis**, et `closestDistanceSq`
est une **approximation** (la longueur du dernier point du simplexe), pas la vraie distance
point-à-point. N'en attendez pas une distance exacte.

### `EPA3D` — la profondeur de pénétration

GJK dit *qu'*il y a collision ; EPA dit *de combien*. À partir du simplexe de GJK, `EPA3D::solve`
construit un tétraèdre puis **l'étend** par silhouette, face après face, vers la frontière de la
différence de Minkowski, jusqu'à convergence (`kEpsilonEPA=1e-4`, `kMaxIter=64`). Il renvoie la
`normal` de pénétration et la `depth` (`valid=true`), ou `{(0,1,0), 0, false}` s'il ne converge pas.

- **Physique** — sans EPA, on saurait qu'il y a collision mais pas comment séparer les corps : EPA
  fournit le **MTV** (vecteur de translation minimale) qui repousse l'un de l'autre.
- **Threading** — `solve` alloue des `std::vector` **à chaque appel** (le polytope grandit) ; en
  contexte massivement parallèle, c'est une pression mémoire à surveiller (et un argument de plus
  pour le futur passage à NKMemory).

### `SAT3D` et `SAT2D` — l'axe séparateur

Pour les **primitives**, le SAT bat GJK/EPA en vitesse et en exactitude. Le principe : deux convexes
ne se touchent **pas** s'il existe un axe sur lequel leurs projections ne se chevauchent pas ; sinon,
l'axe de **moindre** chevauchement donne directement la normale et la profondeur (le MTV). Toutes les
méthodes sont **statiques**.

`SAT3D` couvre la panoplie des primitives 3D : `testOBBOBB` (15 axes — 3 + 3 faces + 9 produits
croisés), `testAABBAABB` (3 axes), `testSphereSphere`, `testSphereAABB` (point le plus proche
clampé), `testSphereOBB` (clamp en espace local), `testCapsuleSphere` (réduit à sphère-sphère via le
point le plus proche du segment), `testCapsuleCapsule` (point le plus proche segment-segment puis
sphère-sphère). `SAT2D` fait l'équivalent en 2D : `testOBBOBB` (4 axes), `testCircleCircle`,
`testPolygonPolygon` (axes = normales d'arêtes des deux polygones).

- **Physique** — le chemin rapide pour boîtes, sphères, capsules : la grande majorité des corps d'un
  jeu.
- **Rendu / culling** — `testAABBAABB` et `testSphereAABB` servent aussi à des tests d'inclusion
  région/objet hors physique pure.
- **2D / UI** — `SAT2D` est la base de la collision 2D (cercles, boîtes orientées, polygones convexes)
  pour le gameplay 2D et les zones d'interaction.
- **IA** — `testSphereOBB`/`testCircleCircle` modélisent simplement des zones de détection autour
  d'agents.

### `Narrowphase` — le dispatcher

`Narrowphase::generateContacts` orchestre tout : il reconnaît les paires de primitives connues et
les route vers le bon test SAT (sphère/sphère, sphère↔AABB dans les deux ordres, AABB/AABB, OBB/OBB,
sphère/OBB, capsule/sphère, capsule/capsule), et **retombe sur GJK puis EPA** pour le reste,
remplissant un `ContactManifold` à un point via `fillManifold`.

- **Physique** — l'unique appel à faire par paire candidate ; le reste est interne.
- **Outils** — pour diagnostiquer, savoir *quel* chemin (SAT vs GJK/EPA) a été pris explique les
  écarts de coût et de qualité de contact.

Le **piège** est l'**ordre des arguments** : seules certaines permutations sont câblées (sphère/AABB
**et** AABB/sphère oui ; mais OBB/sphère, sphère/capsule… retombent sur GJK/EPA, plus lent et à un
seul point). La variante `generateContacts2D` ne prend **pas** de `Transform` et ne gère **que**
cercle/cercle et OBB2D/OBB2D — toute autre paire renvoie un manifold vide. À utiliser en connaissant
ces trous.

### `PersistedContact`, `PersistentManifold`, `makePairKey` — le suivi temporel

Le warm-starting exige de **reconnaître** un contact d'une frame à l'autre. `makePairKey(a, b)`
fabrique une clé 64-bit **indépendante de l'ordre** (elle échange `a` et `b` si `a > b`) pour indexer
une paire — avec un **piège réel** : elle **tronque chaque id à 32 bits**, donc des ids supérieurs à
2³² peuvent entrer en collision de clé.

Un `PersistedContact` stocke le contact en **coordonnées locales** (`localA`/`localB`) — c'est ce qui
permet de le suivre quand les corps bougent —, plus la `normal`, la `depth`, les impulsions
accumulées (`normalImpulse`, `tangentImpulse[2]`), un `frameStamp` et un drapeau `fresh`. Un
`PersistentManifold` en regroupe jusqu'à 4 : son `update` fusionne chaque nouveau contact
(`addOrMerge`, qui **préserve les impulsions** d'un contact proche, à 2 cm près, sinon ajoute ou
remplace le moins pénétrant) puis `removeBreaking` reprojette les points en monde et **purge** ceux
qui ont trop dérivé (séparation normale > `kBreakThresholdNormal` de 2 cm, ou dérive tangentielle² >
`kBreakThresholdTangent²` de 4 cm). `isActive` dit si le manifold a été vu récemment.

- **Physique** — c'est le mécanisme qui rend les piles d'objets **stables** : sans warm-starting, une
  caisse posée sur une autre « respire » et finit par glisser.
- **Animation** — la stabilité des contacts évite le *jitter* visible sur les objets au repos.

### `ContactCache` — le magasin global

`ContactCache` détient tous les `PersistentManifold` dans une `unordered_map` indexée par
`makePairKey`. Le cycle est canonique : `update(fresh, tA, tB, frame)` fusionne le manifold frais
dans l'historique (no-op si `!fresh.hit`) ; `getWarmStart(a, b, idx, n, t)` lit les impulsions
stockées **avant** le solve (renvoie `false` si la paire est absente, l'index hors borne, ou le
contact `fresh`) ; `storeImpulse` les réécrit **après** le solve ; `evict(frame)` jette les manifolds
inactifs depuis plus de `kMaxAge=3` frames.

- **Physique** — `getWarmStart` → `storeImpulse` est le pont vers le solveur ; `toContactManifold`
  reconstruit un manifold monde pour les **callbacks de collision** (notifier le gameplay).
- **Outils / éditeur** — `forEach`/`forEachConst` permettent d'inspecter ou de dessiner tous les
  contacts persistants ; `size()` mesure la charge de contacts.

Un **piège** d'usage : `getOrCreate` renvoie une **référence dans la map**. Elle reste valide tant
qu'on n'insère pas d'autre paire entre-temps (un rehash l'invaliderait) — ne gardez pas cette
référence à travers d'autres `getOrCreate`/`update`.

### `TOIResult` et `SweptSphere` — le CCD analytique

`TOIResult` décrit un impact continu : `toi` (le temps d'impact dans `[0,1]`, **1 = pas d'impact**),
`hit`, `normal`, et les points `pointA`/`pointB`. `SweptSphere` regroupe les cas qu'on sait résoudre
**analytiquement**, donc en `O(1)` par paire — le plus rapide possible :

- `sphereVsSphere` résout l'équation **quadratique** du mouvement relatif ; `toi=0` si déjà en
  chevauchement à `t=0`, *miss* si le discriminant est négatif ou la racine hors `[0,1]`.
- `sphereVsAABB` étend la boîte de `r` (somme de Minkowski) puis lance un ray-cast ; la normale est
  la face touchée (overlap minimal).
- `capsuleVsSphere` est **conservateur** : il sweepe les deux sphères d'extrémité de la capsule et
  garde le plus petit TOI.

Usages, par domaine :

- **Gameplay** — un projectile sphérique rapide (balle, flèche) contre des cibles : `sphereVsSphere`/
  `sphereVsAABB` empêchent le tunneling sans le coût d'un cast général.
- **Physique** — pré-test rapide avant de tomber sur le `LinearCast` plus cher.

### `LinearCast` — le shape-cast général

Quand la forme n'est pas une sphère, `LinearCast::cast` généralise : la forme A se déplace de `tA0` à
`tA1` (B statique), et on cherche le TOI par **recherche dichotomique** sur `t ∈ [0,1]` (jusqu'à
`kMaxIter=32` ou `kTolerance=1e-4`), en testant l'overlap à chaque pas via `Narrowphase::generateContacts`.
Détail important : il interpole **la position (`Lerp`) et la rotation (`Quat::slerp`)** — donc un
corps qui tourne en avançant est correctement suivi. `toi=0` si overlap initial ; `hit` seulement si
`hi < 1`.

- **Physique** — CCD des corps non sphériques (caisses, capsules de personnage) contre le décor.
- **Gameplay / IA** — un *shape-cast* de personnage (« puis-je avancer d'ici à là sans entrer dans un
  mur ? ») et le test « est-ce que ce déplacement est libre ? ».
- **Outils / éditeur** — placer un objet en le « laissant tomber » jusqu'au premier contact (cast
  vers le bas).

### `TOISolver`, `CCDPair`, `CCDManager` — l'orchestration

Au-dessus des casts, `TOISolver` et `CCDManager` intègrent la CCD dans la frame. `CCDPair` = une
paire + son `TOIResult`. `TOISolver::findEarliestImpact` parcourt les `CollidingPair` dont au moins
un corps a le flag `BodyFlag_CCD`, construit leurs transforms de fin de frame (**`dt=1/60` codé en
dur**) et lance `LinearCast::cast` (B traité comme statique), retenant l'impact **le plus précoce**
avec `toi >= kMinTOI` (0.001, pour ignorer le bruit sub-milliseconde). `resolveStep` avance les deux
corps jusqu'au TOI puis applique une impulsion élastique simple (restitution `0.3`, **masses
supposées égales**).

`CCDManager::process` est l'appel d'intégration, à placer **après le broadphase et avant le
narrowphase standard** : il trouve le TOI le plus précoce, fait un `resolveStep`, et pousse un
`ContactManifold` à un point (`penetrationDepth=0`) dans la liste `contacts`. Le pré-filtre statique
`sweptAABBsOverlap` étend la `cachedAABB` des corps CCD par `velocity*dt` et teste l'overlap, pour
éliminer tôt les paires qui ne peuvent pas se rencontrer.

- **Physique / gameplay** — le branchement standard de la CCD pour les corps marqués rapides.
- **Outils** — visualiser le TOI et les transforms interpolées aide à comprendre un impact manqué.

**Limites bien réelles** à connaître avant d'y compter : `dt=1/60` et restitution `0.3` sont **codés
en dur**, les **masses sont supposées égales**, B est **toujours statique**, et `process` ne résout
qu'**une seule** paire (la plus précoce) par appel — plusieurs corps rapides simultanés ne sont pas
tous traités dans la même passe.

### Le pipeline complet, en pratique

Mis bout à bout, une frame de détection ressemble à : **broadphase** (`collectPairs`) →
[**CCD** pour les corps rapides (`CCDManager::process`)] → **narrowphase** par paire
(`generateContacts`) → **cache** (`update`) → **warm-start → solve → storeImpulse** → **evict**. Les
quatre étages se relaient, chacun ne laissant à l'étage suivant que ce qui mérite d'être traité.

---

### Exemple récapitulatif

```cpp
#include "NkBroadPhase.h"
#include "NkNarrowPhase.h"
#include "NkPersistentContacts.h"
using namespace col;

Broadphase   broad{ BroadphaseType::DynamicBVH };
ContactCache cache;

// (à l'ajout d'un corps)
broad.insert(bodyId, body.cachedAABB);

// --- une frame ---
std::vector<CollidingPair> pairs;
broad.collectPairs(pairs);                       // 1. tri grossier des AABB

for (const auto& p : pairs) {
    ContactManifold m = Narrowphase::generateContacts(  // 2. test fin des formes
        shapeA, transformA, shapeB, transformB);
    if (!m.hit) continue;
    m.bodyA = p.bodyA; m.bodyB = p.bodyB;

    cache.update(m, transformA, transformB, frame);     // 3. fusion temporelle

    float n, t[2];
    if (cache.getWarmStart(p.bodyA, p.bodyB, 0, n, t))  // 4. warm-start
        /* solver.seed(n, t) */;
    // ... solve ...
    // cache.storeImpulse(p.bodyA, p.bodyB, 0, n, t);
}
cache.evict(frame);                              // 5. purge des contacts trop vieux
```

---

[← Index NKCollision](README.md) · [Récap NKCollision](../NKCollision.md) · [Couche Runtime](../README.md)
