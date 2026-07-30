# ROADMAP — NKSimulation

Statuts : ✅ fait · 🔄 en cours · ⬜ à faire

> **Règle de ce module, à rappeler à chaque phase :**
> si le code doit tourner alors que rien n'est affiché, il appartient ici ;
> s'il ne fait que dessiner, il reste dans NKRenderer.

---

## S0 — Spécification ✅

- ✅ README : frontière apparence/état, chaînage `NKRHI ← NKSimulation ← {NKRenderer, NKPhysics, NKAI}`
- ✅ Décision actée : **pas de cible de build tant qu'il n'y a pas de code**
- ⬜ Valider avec Rihen le nom du module et sa place (`Kernel/Runtime/NKSimulation`)

## S1 — Océan FFT (première simulation, celle qui fait naître le module) ⬜

Choisi en premier parce qu'il **prouve les deux sens de la frontière** : il produit
des textures pour le rendu **et** une requête de hauteur pour la physique. Une
simulation qui ne servirait qu'au rendu ne prouverait rien et justifierait qu'on
la laisse dans NKRenderer.

- ⬜ Spectre de Phillips + IFFT (méthode Tessendorf) → cartes de déplacement et de normales
- ⬜ Cascades multi-échelles (au moins 2) pour éviter la répétition visible du motif
- ⬜ `QueryHeight(x, z, t)` **côté CPU** — c'est la requête dont `NKPhysics` a besoin
      pour la flottabilité, et elle doit exister sans GPU
- ⬜ Chemin compute (NKRHI) **et** chemin CPU de repli : sans repli, aucun test
      headless et aucune plateforme sans compute
- ⬜ Harnais console `NKSimulationHarness` sur le modèle de `NKEditMeshHarness`
      (signatures déterministes, `--baseline` / `--check`) — **avant** l'intégration
      au rendu, pas après
- ⬜ Création de la cible jenga `NKSimulation` (c'est ici, et seulement ici, qu'elle
      devient légitime)

## S2 — Consommation par NKRenderer ⬜

- ⬜ Matériau de surface d'eau lisant les cartes produites en S1
- ⬜ Réutiliser `NkPlanarReflectionSystem` (déjà en place) — **ne pas réécrire de reflets**
- ⬜ Réfraction, absorption selon la profondeur, écume aux crêtes
- ⬜ Post-process sous-marin (teinte, atténuation, distorsion)

## S3 — Flottabilité et couplage physique ⬜

- ⬜ Volume immergé approché par échantillonnage de la coque
- ⬜ Poussée d'Archimède + traînée, alimentées par `QueryHeight`
- ⬜ Test : un corps flotte, atteint un équilibre, ne diverge pas dans le temps

## S4 — Fumée et feu (grilles eulériennes) ⬜

- ⬜ Advection semi-lagrangienne, projection de pression, diffusion
- ⬜ Sortie : texture 3D de densité + température
- ⬜ Rendu volumétrique côté NKRenderer (ray-march), **pas ici**

## S5 — Fluides particulaires (SPH/FLIP) ⬜

- ⬜ Voisinage par grille spatiale
- ⬜ Surface de rendu (marching cubes ou screen-space)
- ⬜ Collision avec `NKCollision` — vérifier d'abord ce qui existe, **ne pas dupliquer**

## S6 — Brouillard et atmosphère ⬜

- ⬜ **Attention au partage** : le brouillard de hauteur et la diffusion
      atmosphérique sont de l'**apparence** → ils restent dans NKRenderer.
      Ne migre ici que ce qui a un **état** : nuages volumétriques évoluant dans le
      temps, transport de particules par le vent.

---

## ÉLÉMENTS À DÉPLACER DEPUIS D'AUTRES MODULES

Aucun de ces déplacements ne doit être fait « en passant ». Chacun casse des
inclusions et demande son propre harnais de non-régression — la méthode qui a
fonctionné pour BMesh (harnais d'abord, refonte ensuite, rejeu en exigeant des
chiffres identiques).

### M1 — `NkVFXSystem` : séparer l'état du dessin ⬜
**Aujourd'hui :** `Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/VFX/NkVFXSystem.{h,cpp}`
**Constat mesuré :** le fichier manipule vélocité, gravité et durée de vie — c'est
un **état simulé** logé dans le moteur de rendu.
**À faire :** scinder en `NKSimulation` (intégration des particules, forces,
collision) et `NKRenderer` (batch de billboards, tri, matériaux). Priorité basse
tant que les particules n'ont pas de physique réelle ; priorité **haute** dès
qu'on leur ajoute vent, collision ou flottabilité.

### M2 — `NkSimulationRenderer` : lever l'ambiguïté de nom ⬜
**Aujourd'hui :** `NKRenderer/Tools/Simulation/NkSimulationRenderer.{h,cpp}` —
un **stub** (rendu pour PV3DE : peau SSS, yeux, larmes, foule).
**Problème :** malgré son nom, ce fichier est du **rendu**, pas de la simulation.
Deux dossiers nommés « Simulation » avec des rôles opposés est une confusion
programmée.
**À faire :** le renommer (`Tools/CharacterRender/` ou `Tools/PV3DE/`) pour que
« Simulation » ne désigne plus qu'une seule chose dans le dépôt. Coût quasi nul
aujourd'hui (c'est un stub) — coût réel si on attend.

### M3 — `NkVoxelAOSystem` : à examiner, pas à déplacer d'office ⬜
**Aujourd'hui :** `NKRenderer/Tools/VoxelAO/`
**Question ouverte :** la voxelisation de scène est une structure de données
réutilisable (collision approchée, requêtes IA, propagation). Si elle ne sert
qu'à l'occlusion ambiante, elle reste dans NKRenderer. **Décider sur pièces**,
après inventaire — ne pas déplacer par symétrie avec M1.

### M4 — Leçon à ne PAS répéter : `NkEditMesh` ⬜
**Aujourd'hui :** `NKRenderer/Mesh/NkEditMesh.{h,cpp}` — CPU pur, aucune ligne de
GPU, mais dans le moteur de rendu.
**Coût déjà payé :** `NKEditMeshHarness` doit lier NKRenderer + NKRHI + NKSL +
NKWindow + glslang pour tester de la géométrie ; chaque build coûte des minutes.
**À faire :** ce n'est pas un déplacement vers NKSimulation (ce n'est pas de la
simulation), mais le même diagnostic. À traiter avec la tâche T2 de
`Engine/Noge/CONTINUATION.md` (inventaire retopologie), qui pose déjà la question
« où ça vit » pour le même code.

---

## Ce qui reste dans NKRenderer, définitivement

À ne pas migrer, quelle que soit la tentation de « regrouper l'eau » :
brouillard de hauteur, diffusion atmosphérique, réflexions planaires, réfraction,
caustiques, post-process sous-marin, rayons volumétriques, rendu volumétrique de
la fumée. Tout cela **dessine** un état calculé ailleurs — c'est exactement le
contrat entre les deux modules.
