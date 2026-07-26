# NKEvolve — Roadmap

> Faire émerger l'intelligence par sélection (Phase 5). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — algorithme génétique (Phase 5) — 🟡 livré et prouvé (2026-07-23)
- ✅ Génome (`NkGenome` : gènes réels + fitness) + population (`NkPopulation`, init aléatoire
  LCG déterministe) + fonction d'adaptation (*fitness* = pointeur de fonction fourni par
  l'appelant, `NkFitnessFn`, sans `std::function`).
- ✅ Sélection **par tournoi**, **croisement arithmétique** (recombinaison pondérée), **mutation
  gaussienne** (Box-Muller, bornée) + **élitisme** → nouvelle génération (`NkEvolution::RunGeneration`).
- 🎯 ✅ **Atteint** : `NKEvolveTest` (build+run réels) — population 80 individus / 6 gènes,
  problème « approcher un vecteur cible » : **fitness moyen 0,013 → 0,91** en 200 générations,
  **meilleur génome jamais vu → fitness 1,0** (erreur max/gène 0,0005). Le fitness moyen
  progresse nettement génération après génération (bruité par la mutation continue, comme
  attendu d'un GA réel — pas monotone strict, mais tendance nette et meilleur-jamais-vu
  monotone grâce à l'élitisme + suivi explicite).
- ⬜ Reste du Jalon 1 : suivi de diversité (mesurer la convergence prématurée), problème
  d'évaluation plus riche (aujourd'hui une fonction jouet, pas encore un agent/environnement).

## Jalon 2 — neuroévolution
- ✅ **Faire évoluer les poids d'un réseau, SANS gradient (2026-07-25)** : `NKEvolveNNTest`
  (`Applications/NKEvolveNNTest/`) — un génome = le vecteur PLAT de tous les poids+biais d'un
  réseau **2→4→1** (XOR), même convention que `nn::NkDense` (`y = x·W + b`, `W:[in,out]`
  row-major). La fitness recopie les gènes dans un **forward pass MANUEL** (aucune
  `NKAutograd`/`NkVar`, aucun backward — juste `tanh`/`sigmoid` codés à la main sur les gènes
  lus tels quels, zéro copie de gradient) puis évalue les 4 exemples XOR : `fitness =
  1/(1+MSE)`. `NkEvolution`/`NkPopulation` **réutilisés tels quels**, aucune réécriture de la
  mécanique génétique — seule la fonction de fitness change de problème (cible fixe du Jalon 1
  → poids de réseau ici). Contexte : GPU occupé (Palier 6) → **zéro exécution GPU**, tout CPU,
  et l'algorithme génétique n'a de toute façon jamais besoin de backprop (c'est le point :
  alternative à la descente de gradient, pas une réimplémentation).
  🎯 **Preuve réelle** (build+run Debug/Windows, exit 0) : population 200 individus / 17 gènes,
  400 générations, seed=7 → **fitness moyen 0,708 → 0,999** (progression nette, bruitée par la
  mutation continue comme un GA réel), **meilleur génome jamais vu → fitness 0,9997** →
  **le meilleur réseau classe CORRECTEMENT les 4 cas XOR** (`[0,0]→0,001`, `[0,1]→0,982`,
  `[1,0]→0,982`, `[1,1]→0,026`, erreur max 0,026) : **4/4 OK**.
  ⚠️ **Limites honnêtes (comblées le jour même, voir bloc ci-dessous)** : (1) XOR n'a que 4
  entrées possibles → train = test, ce n'est **pas** un test de généralisation ; (2)
  hyperparamètres (population=200, géns=400, mutationSigma=0,5, bornes `[-4,4]`) réglés à la
  main par essais, pas de recherche systématique ni de moyenne sur plusieurs graines
  (toujours vrai) ; (3) **passage à l'échelle non testé** ; (4) pas de comparaison chronométrée
  GA-vs-SGD sur ce même problème (toujours vrai) ; (5) topologie **fixe** (toujours vrai, pas
  de NEAT).

- ✅ **Généralisation train/test RÉELLE + passage à l'échelle mesuré (2026-07-25)** — comble
  les limites (1) et (3) ci-dessus avec de VRAIES mesures (pas des suppositions). `NKEvolveNNTest`
  étendu (`Applications/NKEvolveNNTest/src/main.cpp`), toujours CPU pur / forward pass manuel /
  zéro backprop / zéro GPU (contrainte Palier 6 inchangée).
  - **Protocole** : classification 3 classes, 2D, clusters volontairement **rapprochés + bruit
    large** (`centers = {(-1.3,-1.3),(1.3,-1.3),(0,1.3)}`, jitter uniforme d'amplitude 2,6) pour
    un vrai recouvrement de classes (Bayes < 100%, contrairement au jeu séparable de `NKNNTest`
    qui sature à 100%/100% et ne permet aucune mesure de généralisation utile). **360 points**
    (120/classe), **split 252 train / 108 test tenu à l'écart de l'évolution** (jamais vu par la
    fitness). Fitness = probabilité softmax moyenne assignée à la bonne classe sur le TRAIN
    seulement (forward pass manuel ReLU + softmax codé à la main sur les gènes lus tels quels,
    même philosophie que `XorFitness` — continue et bornée (0,1], meilleur signal pour un GA
    qu'une exactitude en escalier).
  - **3 tailles de réseau, MÊME protocole** (population=150, tournamentSize=4, crossoverRate=0.8,
    mutationRate=0.15, mutationSigma=0.4, elitism=8, bornes `[-3,3]`, seed=99, **250 générations
    fixes pour les 3**, seul `geneCount` change) :
    - **petit** 2→4→3 = **27 gènes**
    - **moyen** 2→16→3 = **99 gènes** (référence pour la mesure de généralisation)
    - **grand** 2→16→16→3 = **371 gènes**
  - 🎯 **Généralisation (réseau moyen, 99 gènes, build+run réel Debug/Windows, exit 0)** :
    exactitude **TRAIN = 100%**, exactitude **TEST (jamais vu) = 99,07%** (107/108) — écart réel
    mais faible (0,93 point), très largement au-dessus du hasard (33,3% pour 3 classes). C'est la
    **première mesure de généralisation réelle** de NKEvolve (XOR ne le permettait pas).
  - 🎯 **Passage à l'échelle (mesure honnête, ni forcée dans un sens ni dans l'autre)** :
    - Exactitude TEST du **meilleur génome jamais vu** : **identique aux 3 tailles (99,07%)** —
      l'élitisme (les `elitism=8` meilleurs recopiés intacts chaque génération) protège la
      qualité du meilleur individu même quand la population entière converge plus lentement.
      **Pas de dégradation de la qualité finale mesurée ici**, contrairement à l'attendu a priori.
    - Fitness **moyenne de population** à génération 250 : petit=**0,9857**, moyen=**0,9937**,
      grand=**0,9873** — relation **NON monotone** avec la taille (le réseau moyen bat les deux
      autres). Mais la **vitesse** de convergence de cette moyenne ralentit bien avec la taille à
      budget de générations fixé : à génération 50, petit=0,9795 moyen=0,9808 **grand=0,9717**
      (le grand est nettement en retard sur les deux autres à ce stade).
    - **Conclusion honnête** : sur CE protocole précis (27→371 gènes, 250 générations,
      population=150), la neuroévolution **ne montre pas** la dégradation nette de qualité finale
      qu'on attendait en grossissant le génome — mais elle montre bien un **ralentissement de la
      convergence de la moyenne de population** avec la taille, compensé ici par l'élitisme qui
      préserve le meilleur individu. Nuance à confirmer sur des génomes encore plus gros
      (milliers de gènes) ou moins de générations, où l'élitisme pourrait ne plus suffire à
      compenser.
  - ⚠️ Limites qui restent : hyperparamètres réglés à la main (pas de recherche systématique ni
    de moyenne sur plusieurs graines), pas de comparaison chronométrée GA-vs-SGD, topologie fixe
    (pas de NEAT), une seule tâche testée (classification 2D 3 classes) — pas de garantie que ce
    résultat se généralise à d'autres problèmes.
- ⬜ Faire évoluer la **structure** (topologie) — style NEAT.
- ⬜ Suivi de la **diversité** (éviter la convergence prématurée).

## Jalon 3 — vie artificielle
- ⬜ Reproduction *située* (les agents se reproduisent dans le monde, pas par lots).
- ⬜ Pression de sélection émergente (survie, ressources).
- ⬜ Hérédité de traits + variation continue.

## Plus tard
- ⬜ Co-évolution (proies/prédateurs, compétition).
- ⬜ Spéciation, niches écologiques.
- ⬜ Couplage fin avec [NKCivilization](../NKCivilization/README.md).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
