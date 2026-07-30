# NKEmbodied — Roadmap

> Donner un corps à l'intelligence (Phase 6). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — corps simulé (Phase 6)
- ✅ Abstraction capteurs / actionneurs : `NkSensor` (`Dim()`+`Sense()` const, lecture pure)
  et `NkActuator` (`Dim()`+`Apply()`, encodage 1 flottant = indice discret OU N flottants =
  scores/argmax) — interfaces génériques, aucun couplage à un corps concret.
  Cf. `src/NKEmbodied/NkSensor.h`, `NkActuator.h`.
- ✅ Boucle perception → décision (NKAgent) → action, dans un corps **simulé** :
  - `NkEmbodiedBody` (`.h`/`.cpp`) = corps grille minimal au-dessus de `rl::NkGridWorld`
    (substrat déjà livré/prouvé par NKRLTest/NKAgentTest, pas de nouvelle physique).
  - `NkEmbodiedGridSensor` = capteur concret qui lit la position du corps et la restitue
    encodée EXACTEMENT comme `agent::NkAgentPerception` (réutilisée directement, pas
    dupliquée) : x,y normalisés + dx,dy signés vers le but.
  - `NkEmbodiedMoveActuator` = actionneur concret qui traduit un vecteur d'actions en un
    déplacement discret appliqué au corps.
  - `NkEmbodiedPolicy` (interface, sans `std::function`) + deux replis fournis :
    `NkEmbodiedRandomPolicy` (témoin aléatoire) et `NkEmbodiedHeuristicPolicy` (décide
    UNIQUEMENT à partir des observations du capteur — preuve directe que les capteurs
    influencent l'action).
  - `NkEmbodiedAgentPolicy` = pont vers un `agent::NkAgent` déjà entraîné/testé (Q-learning
    tabulaire de NKAgent/NKRL, AUCUNE réimplémentation) sous l'interface `NkEmbodiedPolicy`.
  - `NkEmbodiedLoop.h` (`EmbodiedTick`/`RunEmbodiedEpisode`) = la boucle elle-même :
    capteur → décision → actionneur, un tick = un pas de simulation.
- 🎯 Une IA pilote un corps simulé vers un but (ex. atteindre une cible) — **atteint et
  prouvé** : `Applications/NKEmbodiedTest` entraîne un `agent::NkAgent` (même procédure
  Q-learning ε-décroissant que NKAgentTest, 4000 épisodes) sur un monde-grille 5×5, le
  branche au corps simulé via `NkEmbodiedAgentPolicy` + la boucle NKEmbodied, et mesure
  **100% de réussite sur 200 épisodes d'évaluation** (build Debug/Windows : SUCCESS ;
  exécution : code de sortie 0). Trace tick par tick loggée (état → observations capteur →
  action décidée → état suivant → récompense) : preuve que les lectures de capteurs
  influencent réellement les actions, pas un mock qui les ignore. Les deux replis
  (`NkEmbodiedRandomPolicy` à 6%, `NkEmbodiedHeuristicPolicy` à 0% — tombe dans un trou
  situé sur la diagonale directe vers le but, limite assumée et documentée dans le test)
  servent de témoins de comparaison honnêtes.

### Ce qui reste HORS scope Jalon 1 (assumé, pas caché)
- Pas de VRAI apprentissage dans ce corps : le cerveau `agent::NkAgent` est entraîné à
  PART (sur un `rl::NkGridWorld` séparé mais identique), puis branché déjà figé
  (`greedy=true`) — la boucle NKEmbodied ne fait qu'exécuter une politique déjà apprise.
  `NkEmbodiedAgentPolicy` supporte `greedy=false` (ε-greedy) pour apprendre EN LIGNE
  pendant que le corps agit, mais ce chemin n'est ni exercé ni prouvé ici.
- Pas de contrôle robuste : ni fréquence fixe (temps réel souple), ni bruit capteur, ni
  limites/saturation d'actionneur, ni sécurité (arrêt d'urgence, bornes) — c'est le
  Jalon 2.
- Pas de lien Kernel/Bare réel (capteurs/actionneurs matériels, transfert sim→réel,
  inférence embarquée) — c'est le Jalon 3. `NKEmbodied` ne dépend pas de `Kernel/Bare` à
  ce stade (conforme à la note du registre `config/modules.jenga`).
- Corps volontairement minimal (grille 2D discrète, 4 actions) : pas de dynamique
  continue, pas de collision entre plusieurs corps, pas de capteur de vision/contact —
  seulement position + direction vers le but.

## Jalon 2 — contrôle robuste
- ✅ **Bruit capteur** : `NkEmbodiedNoisySensor` (`src/NKEmbodied/NkEmbodiedSensorNoise.h`) — décorateur
  générique de `NkSensor`, bruit gaussien (Box-Muller) OU uniforme borné, magnitude configurable, graine
  LCG déterministe (reproductible : même seed → mêmes observations bruitées, vérifié par assertion dans
  `NKEmbodiedTest`). Pour que le bruit affecte RÉELLEMENT une politique tabulaire (NKAgent indexe par état
  brut et ignore les observations), `NkSensor` gagne une méthode `EstimateRawState()` (repli : non
  supportée) que `NkEmbodiedGridSensor` redéfinit (inverse x,y normalisés → indice de grille le plus
  proche, robuste au débordement hors [0,1]) ; `NkEmbodiedLoop::EmbodiedTick()` transmet désormais à la
  politique un état **perçu** (`perceivedStateBefore`, via le capteur) au lieu de la vérité terrain — sans
  bruit, perçu == réel (aucune régression Jalon 1, revérifié : Test 3 toujours 100%/200 épisodes).
  **Chiffres réels mesurés** (cerveau NKAgent du Jalon 1, 100% sans bruit, moyenne sur 5 graines × 200
  épisodes, `Applications/NKEmbodiedTest`) : σ=0,00 → **100%** (témoin) · σ=0,05 (~1/5 de case) → **98,9%**
  · σ=0,125 (~1/2 case) → **74%** · σ=0,25 (~1 case) → **48,4%** · σ=0,50 (~2 cases) → **22,6%**. Point de
  dégradation réel observé : entre σ=0,05 et σ=0,125 (chute de 98,9% à 74%), la politique tabulaire perd
  pied dès que le bruit dépasse ~une demi-case (l'estimation d'état se trompe alors régulièrement de case).
- ✅ **Limites actionneur** : `NkEmbodiedLimitedActuator` (`src/NKEmbodied/NkEmbodiedActuatorLimits.h`) —
  décorateur générique de `NkActuator`, (1) saturation d'amplitude (écrêtage dans
  [-maxAmplitude,+maxAmplitude], vérifié par assertion : commande hors bornes 999.0 bien écrêtée à 3.0) et
  (2) limite de fréquence de commande (au plus 1 commande NEUVE toutes les N ticks, la précédente étant
  ré-appliquée sinon — vérifié par assertion : sur 8 commandes différentes avec N=4, exactement 2/8
  acceptées et 6/8 retenues). Effet réel mesuré sur le cerveau NKAgent (200 épisodes/palier) : **aucune
  dégradation** pour N ≤ 64 ticks entre commandes (reste à 100% — la grille 5×5 avec `maxTicks=100` pour un
  chemin optimal de ~8-15 ticks absorbe la latence, la commande retenue restant globalement orientée vers
  le but) ; dégradation nette à **N=128 → 50%** (128 > `maxTicks`=100 : au plus une commande neuve par
  épisode). Point de rupture mesuré, pas supposé — limite honnête : ce test ne stresse la contrainte de
  fréquence qu'à travers CE monde 5×5/ce budget de ticks précis, pas en général.
- ✅ **Fréquence fixe** : `NkEmbodiedFixedRateLoop` (`src/NKEmbodied/NkEmbodiedFixedRateLoop.h`) — même
  pattern accumulateur de temps fixe que `physics::NkPhysicsWorld::Advance()` (cf. NKPhysics) : découple le
  taux de DÉCISION (fréquence de contrôle fixe, ex. 20 Hz) du taux d'APPEL (« simulation », ex. 240 Hz).
  Ne réimplémente pas la boucle : délègue à `EmbodiedTick()`. Vérifié par assertion : simulation à 240 Hz
  (dt=1/240s), décision fixée à 20 Hz → 20 décisions exécutées sur 240 appels (20/240 appels déclenchent
  une décision, 220/240 n'en déclenchent AUCUNE) — preuve réelle du découplage, pas 1 décision par appel.
- ✅ **Sécurité** : `NkEmbodiedSafetyMonitor` (`src/NKEmbodied/NkEmbodiedSafety.h`) — watchdog détectant (1)
  état bloqué (état inchangé N ticks consécutifs — mur/collision répétée, ce monde-grille n'ayant pas de
  sortie de grille explicite : « sortir = rester sur place ») et (2) actionneur saturé/rate-limité M ticks
  consécutifs ; déclenche un arrêt d'urgence (`Tripped()`), l'appelant décidant de la réaction (façon
  watchdog matériel : détecte, n'agit pas à la place du système). Vérifié par 2 scénarios construits exprès
  et assertés : (a) politique fixe poussant contre un mur depuis (0,0) → arrêt déclenché exactement au tick
  3 (seuil configuré), aucun tick exécuté ensuite ; (b) actionneur rate-limité à l'extrême pendant qu'une
  politique heuristique continue de décider → arrêt déclenché au tick 4 pour cause de saturation,
  indépendamment de l'état du corps (qui n'était pas bloqué) — isolation des deux causes confirmée.
- 🎯 Preuve de bout en bout : `Applications/NKEmbodiedTest` (tests 4 à 7, en plus des tests 1-3 du Jalon 1)
  fait réellement tourner ces 4 mécanismes sur le même corps/capteur/actionneur/cerveau NKAgent, avec des
  vrais chiffres mesurés (pas de supposition) et des assertions (`NKENTSEU_ASSERT`) sur les invariants
  vérifiables directement (saturation, comptage de fréquence, découplage temporel, déclenchement watchdog).
  Build Debug/Windows : SUCCESS ; exécution : code de sortie 0 (Jalon 1 ET Jalon 2 tous deux "OK").

### Ce qui reste HORS scope Jalon 2 (assumé, pas caché)
- Limites actionneur mesurées sur UN SEUL monde (grille 5×5, `maxTicks`=100) : le point de rupture réel
  (entre N=64 et N=128) est spécifique à ce budget de ticks/cette taille de grille, pas une constante
  générale — un monde plus grand ou un budget de ticks plus serré dégraderait à des paliers plus bas.
- Le bruit ne porte que sur `NkEmbodiedGridSensor` (position). Le Test 2 (`NkEmbodiedHeuristicPolicy`, qui
  lit dx/dy directement) n'a pas été re-testé sous bruit ici — seul le cerveau NKAgent (Test 3) l'a été.
- `NkEmbodiedSafetyMonitor` ne fait que DÉTECTER : aucune action de repli sûre (ex. re-router vers un état
  connu, freiner en douceur) n'est implémentée ici — l'appelant du test se contente d'arrêter la boucle.
- `NkEmbodiedFixedRateLoop` utilise un `float realDt` fourni par l'appelant (pas encore branché sur
  `NkClock`/`NkDuration` de NKTime, qui existent déjà ailleurs dans le moteur avec le même pattern
  accumulateur) — intégration réelle à une horloge de moteur hors scope ici.

## Jalon 3 — réel (via Kernel/Bare)
- ⬜ Brancher capteurs/actionneurs réels via [Kernel/Bare](../../Bare/README.md) (NKDriver, NKInput).
- ⬜ Transfert **sim → réel** (calibration, écart de réalité).
- ⬜ Inférence embarquée (NKInfer sur l'appareil).

## Plus tard
- ⬜ Plusieurs robots coordonnés.
- ⬜ Apprentissage en ligne sur le robot.
- ⬜ Objets intelligents variés (au-delà du robot mobile).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
