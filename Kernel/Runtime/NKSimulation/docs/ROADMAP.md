# ROADMAP — NKSimulation

Statuts : ✅ fait · 🔄 en cours · ⬜ à faire

> **Règle du module, à rappeler à chaque phase :**
> si le code doit tourner alors que **rien n'est affiché**, il appartient ici ;
> s'il ne fait que **dessiner**, il reste dans NKRenderer.

---

# 1. PÉRIMÈTRE — ce qui entre, ce qui n'entre pas

C'est la section la plus importante du document. Le risque principal d'un module
nommé « Simulation » est de devenir le **dépotoir** de tout ce qui n'a pas de
maison évidente. Des modules voisins couvrent déjà une partie du terrain :
`NKPhysics` (corps rigides, joints, ragdoll, solveur de contacts), `NKCollision`,
`NKNavigation`, et toute la pile `Kernel/AI/` (NKAgent, NKCivilization, NKRL,
NKEvolve).

## 1.1 Le critère de partage avec NKPhysics

C'est la frontière la plus délicate, et elle mérite un critère explicite plutôt
qu'un arbitrage au cas par cas :

| | NKPhysics | NKSimulation |
|---|---|---|
| objet | **corps discrets** en contact | **champs continus** |
| exemples | rigides, joints, ragdoll, personnage | fluide, gaz, chaleur, houle, érosion |
| état | quelques dizaines de corps | grilles ou millions de particules |
| pas de temps | contraint par les contacts | contraint par la condition CFL |

**Cas limites assumés** (tissu, cheveux, corps mous) : ce sont des continus
*discrétisés en particules et contraintes*. Ils vont **là où vit le solveur de
contraintes**, donc NKPhysics, et NON ici — sinon deux modules implémentent la
détection de collision et divergent. S'ils migrent un jour, ce sera avec leur
solveur, pas seuls.

## 1.2 Les deux cas de la question qui ne vont PAS ici

### « Simulation de foule » — non, à découper
Une foule est **trois choses distinctes** :
1. **Décider** où aller (but, personnalité, émotion) → `Kernel/AI/NKAgent`,
   `NKCivilization`. Déjà en place : ne pas dupliquer.
2. **Naviguer** (chemin, évitement local, champ de flux) → `NKNavigation` existe.
   Le champ de flux peut venir ici **s'il devient un champ continu** sur de très
   grandes populations.
3. **Animer** les corps → animation et skinning, côté NKRenderer.

Verdict : **NKSimulation ne prend que le cas « foule = fluide »** (densité,
pression, champ de vitesse) pour des dizaines de milliers d'individus, là où l'on
cesse de modéliser des agents. En dessous, c'est de l'IA et de la navigation.

### « Calcul haute performance » — non, c'est un SUBSTRAT
« HPC » n'est pas un domaine de simulation, c'est **la façon dont on calcule**.
Le placer ici serait une erreur de catégorie aux conséquences concrètes : tout
module voulant du parallélisme devrait alors dépendre du module de simulation.

Le parallélisme appartient aux couches basses, et elles existent : `NKThreading`
(pool de tâches), `NKRHI` (compute GPU), le SIMD dans NKMath. NKSimulation en est
un **client**, pas le propriétaire. Ce qui peut légitimement naître ici, ce sont
des **primitives de simulation** parallèles (réduction déterministe, tri spatial,
solveur de Poisson) — utiles à ce module, jamais un service générique.

## 1.3 Tableau de décision rapide

| ça | où | pourquoi |
|---|---|---|
| houle FFT, fluides, fumée, feu, érosion, climat | **NKSimulation** | champ continu avec état |
| corps rigides, joints, ragdoll, tissu, cheveux | NKPhysics | corps + contraintes, solveur unique |
| chemin, évitement local, champ de navigation | NKNavigation | déjà en place |
| décision, personnalité, société | Kernel/AI | déjà en place |
| brouillard, réfraction, caustiques, ray-march | NKRenderer | apparence, pas d'état |
| pool de tâches, compute, SIMD | NKThreading / NKRHI / NKMath | substrat |
| voxelisation de scène | **à décider** | dépend de ses usages (cf. M3) |

---

# 2. PHASES PAR DOMAINE

Chaque domaine est indépendant : on peut en livrer un sans les autres. L'ordre va
du **plus utile au projet** au plus spéculatif. Pris au pied de la lettre, ce
document représente **plusieurs années** de travail — c'est une carte, pas un
engagement.

## S0 — Spécification ✅
- ✅ Frontière apparence/état, chaînage `NKRHI ← NKSimulation ← {NKRenderer, NKPhysics, NKAI}`
- ✅ Décision : **pas de cible de build tant qu'il n'y a pas de code**
- ✅ Périmètre explicite (section 1), y compris ce qui n'entre pas
- ⬜ Valider le nom et l'emplacement avec Rihen

## D1 — Océan et surfaces d'eau ⬜  *(premier domaine, celui qui fait naître le module)*
Choisi en premier parce qu'il **prouve les deux sens de la frontière** : il
produit des textures pour le rendu **et** une requête de hauteur pour la
flottabilité. Une simulation qui ne servirait qu'au rendu justifierait qu'on la
laisse dans NKRenderer.
- ⬜ Spectre de Phillips + IFFT (Tessendorf) → cartes de déplacement et de normales
- ⬜ Cascades multi-échelles (≥ 2) contre la répétition visible du motif
- ⬜ `QueryHeight(x, z, t)` **CPU** — la requête dont NKPhysics a besoin, sans GPU
- ⬜ Chemin compute (NKRHI) **et** repli CPU : sans repli, aucun test headless et
      aucune plateforme sans compute (leçon du portage Web : WebGL2 n'a pas de
      compute shader)
- ⬜ Écume : accumulation par le jacobien du déplacement (état, donc ici ; le
      *rendu* de l'écume reste dans NKRenderer)
- ⬜ Harnais `NKSimulationHarness` (signatures déterministes, `--baseline`/`--check`)
      **avant** l'intégration au rendu
- ⬜ Création de la cible jenga — c'est ici, et seulement ici, qu'elle devient légitime

## D2 — Flottabilité et couplage ⬜
- ⬜ Volume immergé approché par échantillonnage de coque
- ⬜ Poussée d'Archimède, traînée, amortissement, alimentés par `QueryHeight`
- ⬜ Test : un corps flotte, atteint un équilibre, **ne diverge pas** sur 10 000 pas
- ⬜ Interface d'interrogation générique (`SampleField`) réutilisable par les autres domaines

## D3 — Gaz : fumée, feu, explosions ⬜
- ⬜ Grille eulérienne : advection semi-lagrangienne, projection de pression, diffusion
- ⬜ Flottabilité thermique, refroidissement, combustion simple
- ⬜ Vorticity confinement — sans lui les volutes s'écrasent numériquement
- ⬜ Sortie : texture 3D densité + température. **Le ray-march reste dans NKRenderer**
- ⬜ Grilles creuses : les cellules vides dominent, une grille dense gaspille la mémoire

## D4 — Fluides particulaires ⬜
- ⬜ Voisinage par grille spatiale — le tri domine le coût, pas la physique
- ⬜ SPH d'abord, puis FLIP/PIC (FLIP conserve mieux le volume, SPH se débogue plus facilement)
- ⬜ Extraction de surface : marching cubes ou écran
- ⬜ Collision : **consommer NKCollision**, ne pas réimplémenter

## D5 — Particules et VFX physiques ⬜
- ⬜ Intégration, forces, champs (vent, turbulence, attracteurs)
- ⬜ Collision avec la scène, adhérence, rebond
- ⬜ **Reprise de l'état de `NkVFXSystem`** (cf. M1) — le dessin reste dans NKRenderer

## D6 — Terrain, érosion, hydrologie ⬜
- ⬜ Érosion hydraulique et thermique sur carte de hauteur
- ⬜ Écoulement, accumulation, rivières, bassins versants
- ⬜ Sédimentation et dépôt
- ⬜ **Lien fort avec NKCivilization** : sols, eau et ressources conditionnent
      l'implantation humaine. C'est le domaine qui relie la simulation physique au
      système de civilisation — un vrai différenciateur du projet.

## D7 — Atmosphère, météo, climat ⬜
- ⬜ Champ de vent (3D ou 2,5D), consommé par D3, D5, D6
- ⬜ Cycle de l'eau : évaporation, nuages, précipitations
- ⬜ Températures saisonnières, zones climatiques
- ⬜ **Attention** : brouillard et diffusion atmosphérique sont de l'apparence →
      ils restent dans NKRenderer. Ne migre ici que ce qui a un **état évoluant
      dans le temps**.

## D8 — Granulaires : sable, neige, débris ⬜
- ⬜ Modèle continu (rhéologie) ou position-based
- ⬜ Traces et déformation persistante : empreintes, ornières

## D9 — Destruction et fracture ⬜
- ⬜ Pré-fracturation (Voronoï) et fracture à l'exécution
- ⬜ **Frontière** : les fragments deviennent des corps rigides → passage à
      NKPhysics. NKSimulation décide **quand et comment ça casse**, pas comment
      les morceaux tombent.

## D10 — Foule « fluide » (grande échelle uniquement) ⬜
- ⬜ Champ de densité et de vitesse, pression sociale, continuum crowds
- ⬜ **Seuil explicite** : en dessous de ~10 000 individus, c'est NKAgent +
      NKNavigation. Ici, on cesse de modéliser des individus.

## D11 — Diffusion et transport ⬜
- ⬜ Chaleur, humidité, propagation du feu en surface
- ⬜ Propagation de contaminants ou d'odeurs (perception IA)
- ⬜ Sert D6, D7 et l'IA embarquée

---

# 3. TRANSVERSAL — ce à quoi on ne pense pas avant d'en souffrir

Ces points ne sont pas des domaines mais des **propriétés du module**. Chacun est
beaucoup moins cher à concevoir au début qu'à ajouter après coup.

## T1 — Déterminisme et reproductibilité ⬜
Une simulation non déterministe est indéboguable, non rejouable, et casse toute
synchronisation réseau. À décider **dès D1** :
- pas de temps FIXE avec sous-pas ; jamais de pas dépendant du framerate
- ordre de réduction stable — une somme parallèle non ordonnée n'est pas
  reproductible en flottant
- graine explicite pour tout aléatoire
- ⬜ Test : deux exécutions identiques → signatures identiques. Le harnais
      NkEditMesh a déjà prouvé la valeur de cette approche.

## T2 — Stabilité numérique ⬜
- ⬜ Condition CFL et sous-pas automatiques
- ⬜ Garde-fous NaN/Inf : **un seul NaN contamine tout un champ**, et l'expérience
      du bloom l'a déjà montré côté rendu
- ⬜ Bornes d'énergie : détecter une divergence plutôt que rendre du bruit

## T3 — Baking et cache disque ⬜
Indispensable pour le **film d'animation** : on ne resimule pas à chaque rendu.
- ⬜ Format de cache versionné, par frame, compressé
- ⬜ Lecture en flux — une simulation dépasse vite la RAM
- ⬜ Invalidation par signature des paramètres

## T4 — Niveau de détail et budget ⬜
- ⬜ Résolution variable selon la distance caméra
- ⬜ Gel des simulations hors champ, réveil progressif
- ⬜ Budget temps par frame : dégrader plutôt que faire chuter le framerate

## T5 — Repli CPU obligatoire ⬜
Leçon **déjà payée** sur le portage Web : WebGL2 n'a pas de compute shader, et sur
mobile il est limité. Toute simulation doit avoir un chemin CPU, même lent — c'est
aussi ce qui rend les tests headless possibles.

## T6 — Sérialisation et édition ⬜
- ⬜ Sauvegarde et chargement de l'état (reprise, rejeu)
- ⬜ Émetteurs, forces et obstacles comme **entités de scène** éditables
- ⬜ Intégration à l'éditeur : gizmos dédiés, même approche que les lumières

## T7 — Simulation différentiable *(différenciateur unique du projet)* ⬜
Nkentseu possède `NKAutograd` **écrit from scratch**. Rendre une simulation
différentiable permet d'**apprendre** ses paramètres depuis des observations :
calibrer une érosion sur un terrain réel, apprendre un contrôleur de fluide. Très
peu de moteurs peuvent le faire — c'est un axe que cette pile rend possible, et
auquel personne ne pense au moment de concevoir le module.
- ⬜ Étudier la faisabilité sur le domaine le plus simple (diffusion, D11)
- ⬜ Honnêteté : coûteux en mémoire (il faut conserver l'historique), à réserver à
      de petites résolutions

## T8 — Interopérabilité ⬜
- ⬜ Import/export vers les formats de cache standards (type Alembic / VDB)
- ⬜ Permet de faire circuler le travail avec Blender et Houdini

---

# 4. ÉLÉMENTS À DÉPLACER DEPUIS D'AUTRES MODULES

Aucun déplacement « en passant ». Chacun casse des inclusions et exige son propre
harnais de non-régression — la méthode qui a fonctionné pour BMesh (52 cas figés,
rejeu en exigeant des chiffres identiques).

### M1 — `NkVFXSystem` : séparer l'état du dessin ⬜
**Aujourd'hui** `NKRenderer/Tools/VFX/NkVFXSystem.{h,cpp}`.
**Constat mesuré** : le fichier manipule vélocité, gravité et durée de vie
(12 occurrences) — un **état simulé** logé dans le moteur de rendu.
**À faire** : scinder — intégration, forces et collision ici (D5) ; batch de
billboards et matériaux dans NKRenderer. Priorité basse tant que les particules
n'ont pas de physique ; **haute** dès qu'on leur ajoute vent ou collision.

### M2 — `NkSimulationRenderer` : lever l'ambiguïté de nom ⬜
**Aujourd'hui** `NKRenderer/Tools/Simulation/NkSimulationRenderer.{h,cpp}` — un
**stub** de rendu PV3DE (peau SSS, yeux, larmes, foule).
**Problème** : malgré son nom, c'est du **rendu**. Deux dossiers « Simulation »
aux rôles opposés est une confusion programmée.
**À faire** : renommer (`Tools/CharacterRender/` ou `Tools/PV3DE/`). Coût quasi
nul aujourd'hui (c'est un stub), réel si on attend.

### M3 — `NkVoxelAOSystem` : à EXAMINER, pas à déplacer d'office ⬜
**Aujourd'hui** `NKRenderer/Tools/VoxelAO/`.
**Question ouverte** : si la voxelisation ne sert qu'à l'occlusion ambiante, elle
reste. Si elle sert aussi à la collision approchée, aux requêtes IA ou à la
propagation (D11), elle devient une structure partagée. **Décider sur pièces**,
jamais par symétrie avec M1.

### M4 — `NkEditMesh` : même diagnostic, autre destination ⬜
**Aujourd'hui** `NKRenderer/Mesh/NkEditMesh.{h,cpp}` — CPU pur, aucune ligne de
GPU, mais dans le moteur de rendu.
**Coût déjà payé** : `NKEditMeshHarness` doit lier NKRenderer + NKRHI + NKSL +
NKWindow + glslang pour tester de la géométrie ; chaque build coûte des minutes.
Ce n'est **pas** de la simulation — à traiter avec la tâche T2 de
`Engine/Noge/CONTINUATION.md` (inventaire retopologie), qui pose déjà la même
question pour le même fichier.

---

# 5. CE QUI RESTE DANS NKRENDERER, DÉFINITIVEMENT

À ne pas migrer, quelle que soit la tentation de « regrouper l'eau » le jour où
quelqu'un voudra ranger : brouillard de hauteur, diffusion atmosphérique,
réflexions planaires, réfraction, caustiques, post-process sous-marin, rayons
volumétriques, ray-march de la fumée. Tout cela **dessine** un état calculé
ailleurs — c'est exactement le contrat entre les deux modules.
