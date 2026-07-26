# NKAgent — Roadmap

> L'être qui se souvient, perçoit et décide (Phase 4). ⬜ à faire · 🟡 en cours · ✅ fait.

## État (2026-07-26) — Jalon 4 COMPLET : personnalité + raisonnement LLM (cf. section Jalon 4)

## État (2026-07-25) — Jalon 3 COMPLET : buts & planification (cf. section Jalon 3)

## État (2026-07-25) — Jalon 2 COMPLET : importance + memory replay (cf. section Jalon 2)

## État (2026-07-23) — premier `NkAgent` RÉEL, vérifié par build+run

**Livré, compilé (`jenga build --target NKAgent` et `NKAgentTest`, Debug/Windows) ET exécuté
(`NKAgentTest.exe`, code retour 0) :**
- `NkAgentMemory` (`src/NKAgent/NkAgentMemory.h/.cpp`) : buffer BORNÉ de `NkAgentTransition`
  (observation, action, récompense, observation suivante, terminal) sur `NkVector`, ring buffer
  (écrase le plus ancien une fois plein). Pas encore de notion d'« importance » (Jalon 2).
- `NkAgentPerception` (`NkAgentPerception.h/.cpp`) : encode un état brut `rl::NkGridWorld` en
  4 features (x/y normalisés + direction signée vers le but). ⚠️ Spécialisé à `NkGridWorld` (seul
  environnement NKRL livré) — pas encore d'interface d'encodage générique par-dessus
  `rl::NkEnvironment`.
- `NkAgentPolicy` (`NkAgentPolicy.h/.cpp`) : **enveloppe** `rl::NkQLearning` (NKRL) — ne
  réimplémente pas l'apprentissage par renforcement, compose dessus (`SelectAction`/
  `SelectGreedy`/`Update`/epsilon délégués).
- `NkAgent` (`NkAgent.h/.cpp`) : assemble les trois dans `Step()` = perception → décision → action
  → mémorisation. `RunAgentEpisode()` déroule un épisode complet à travers cette couche.
- Module Jenga `NKAgent.jenga` (deps NKRL/NKTensor/NKContainers/NKMemory/NKLogger/NKMath),
  enregistré dans `config/modules.jenga` et `Nkentseu.jenga`.
- App de preuve `Applications/NKAgentTest` (façon `NKRLTest`, mais via la couche `NkAgent` au lieu
  d'appeler `rl::NkQLearning` directement) : grille 5×5, mêmes hyperparamètres que `NKRLTest`.
  **Résultat réel obtenu** (build Debug/Windows, exécution directe de l'exe) : succès 800-derniers
  7.9% → 97.1% pendant l'entraînement, **évaluation gloutonne 200/200 (100%)**, mémoire
  `64/64` transitions retenues (vérifiées dans les logs), `Perceive(départ)` = `[0,0,1,1]` (cohérent
  avec x=0,y=0, but en bas-à-droite), politique affichée évite les 3 trous. **`[ OK ]`**.
- ⚠️ `jenga run NKAgentTest` (daemon) n'affiche rien à l'écran (sortie du logger non recueillie par
  le wrapper `run`) — **contournement utilisé pour vérifier** : exécution directe de
  `Build/Bin/Debug-Windows/NKAgentTest/NKAgentTest.exe`, qui affiche tout et renvoie le code 0.
  Non-investigué plus loin (hors périmètre NKAgent).

## Jalon 1 — boucle perception→décision→action (Phase 4)
- ✅ Structure d'agent : observation (présent) → politique → action (`NkAgent::Step`).
- ✅ Brancher une politique [NKRL](../NKRL/README.md) (`NkAgentPolicy` enveloppe `rl::NkQLearning`).
- 🎯 ✅ Un agent (couche `NkAgent`, pas directement NKRL) agit dans un monde-grille et atteint le
  but à 100% en évaluation (`NKAgentTest`).

## Jalon 2 — mémoire (le passé) — ✅ COMPLET (2026-07-25, build+run réels)
- ✅ Mémoire d'événements : stocker (`Push`), rappeler (`At`/`Last`), oublier — `NkAgentMemory`.
- ✅ **Importance** des souvenirs (2026-07-25) : chaque transition est pondérée par
  **|erreur TD|** calculée AVANT la mise à jour (proxy de « surprise », à la *prioritized
  experience replay* de Schaul et al. — choisi plutôt que |récompense| car la table Q tabulaire
  rend l'erreur TD exacte calculable en O(nb actions), et elle se réduit à ~|récompense| sur les
  terminaux en début d'apprentissage). **Oubli pondéré** : mémoire pleine ⇒ éviction du souvenir
  de **moindre importance** (à égalité : le plus ancien), plus du FIFO pur ; le souvenir le plus
  récent est toujours accepté (récence garantie, l'importance gouverne la durée de rétention).
  API : `Push(t, importance)` (+ surcharge rétro-compatible `Push(t)` = |récompense|),
  `ImportanceAt`, `Min/MaxImportance`. Preuve observée (`NKAgentTest`) : importances retenues
  min≈2,4e-07 / max=1,0 — la pondération agit.
- ✅ **Apprentissage à partir de la mémoire — memory replay** (2026-07-25) : `NkAgent::Replay(n)`
  rejoue n transitions tirées **uniformément** de la mémoire à travers `NkQLearning::Update`
  (via `NkAgentPolicy` — aucune réimplémentation du RL ; le tirage proportionnel aux priorités
  + correction IS = extension future). Intégré à `Step()` : `NkAgentConfig.replayEnabled/
  replayBatchSize/replayInterval` (désactivé par défaut — comportement Jalon 1 inchangé). RNG
  LCG dédié, graine décorrélée de la politique.
- 🎯 ✅ **Jalon prouvé par ablation** (`NKAgentTest`, test 2, exécution réelle 2026-07-25) :
  mêmes hyperparamètres/graines, budget réduit à **80 épisodes** (vs 4000), 5 graines, éval
  gloutonne 200 épisodes : **SANS replay = 20 % de moyenne** (0/0/0/100/0) contre **AVEC replay
  = 100 %** (100×5) — ~6 400 transitions rejouées par agent. Nota mesuré : à 1500 et même 200
  épisodes, les DEUX bras saturent déjà à 100 % sur cette grille 5×5 (le schéma d'ε s'adapte au
  budget) — l'écart n'est visible qu'en régime famine, d'où le budget 80.

## Jalon 3 — buts & planification (le futur) — ✅ COMPLET (2026-07-25, build+run réels)
- ✅ **Un but = un état-cible + priorité + budget** : `NkAgentGoal` (`NkAgentGoal.h`) — `targetState`
  (état dans l'espace d'états de l'environnement), `priority` (informatif, laissé à l'appelant),
  `maxSteps` (budget de pas avant échec — remplace une replanification complexe par un abandon
  honnête et mesurable), `stepsElapsed`, `status` (`Active/Achieved/Failed`).
- ✅ **Pile de buts actifs = la planification** : `NkGoalStack` (`NkGoalStack.h`, LIFO sur
  `NkVector<NkAgentGoal>`). Décomposer « atteindre la ressource PUIS le but final » = empiler le
  but final PUIS le sous-but par-dessus — le sommet (`Current()`) est toujours le but ACTIF ; une
  fois atteint (ou expiré), il est dépilé et le but sous-jacent redevient actif automatiquement.
  Un plan séquentiel simple, sans recherche de graphe d'états.
- ✅ **`Step()` conscient du but** : `NkAgent::StepWithGoals(rl::NkEnvironment&, rawState, learn,
  NkGoalStack&)` (+ `RunAgentEpisodeWithGoals`, analogue de `Step`/`RunAgentEpisode` mais générique
  sur `rl::NkEnvironment`, via un **nouveau constructeur générique** de `NkAgent` — plus besoin
  d'un `rl::NkGridWorld` concret). Poursuit le but actif via une politique **dédiée à ce but**
  (`NkAgent::PolicyForGoal`, find-or-create, graine décorrélée par but) avec une récompense
  **façonnée localement** (+1 en atteignant SA cible, -1 sur échec dur non lié — ex. trou —, sinon
  la récompense de pas réelle de l'environnement). **Choix architectural : une table Q PAR but**
  (justifié dans `NkAgent.cpp`) plutôt qu'un état augmenté d'une politique unique — chaque but
  devient un sous-problème de RL COURT et bien récompensé, sans toucher NKRL. La mémoire
  (`NkAgentMemory`) garde la récompense RÉELLE de l'environnement (le vécu), pas la récompense
  façonnée. But atteint → `Achieved` + `Pop()` (bascule) ; budget dépassé → `Failed` + `Pop()`
  (abandon honnête = la « replanification », pas de recherche de plan alternatif).
- ✅ **Nouvel environnement de preuve** : `rl::NkKeyDoorGridWorld` (`Kernel/AI/NKRL/src/NKRL/
  NkKeyDoorGridWorld.h/.cpp`) — grille à DEUX étapes obligées : la porte (but final) reste FERMÉE
  (entrée bloquée, l'agent reste sur place) tant que la clé n'est pas ramassée (automatique en
  marchant dessus). État = `hasKey·N² + position` (le contexte fait partie de l'état, MDP complet).
- 🎯 ✅ **Jalon prouvé par ablation** (`NKAgentTest`, test 3, exécution réelle 2026-07-25) : grille
  5×5, départ=0, clé=12 (centre), porte=24 (coin opposé), **sans trou** (isole la difficulté du
  chaînage clé→porte), chemin optimal = 8 pas (4+4). Budget **volontairement serré** : 100 épisodes
  d'entraînement, **`maxSteps=10`** (à peine 2 pas de marge sur l'optimal — sans ce serrage, le
  monde 5×5 est assez petit pour qu'un agent plat finisse par découvrir le chemin composé par pure
  diffusion aléatoire ; premier essai à `maxSteps=60`/150 épisodes : les DEUX bras à 100 %, aucun
  écart — mesuré, pas supposé), 5 graines (11/22/33/44/55), éval gloutonne 100 épisodes/graine :
  **agent SANS but (1 seul but = la porte, pas de décomposition) = 0 % de réussite (0/500 sur les 5
  graines, TOUTES à 0 %)** — échec structurel, pas juste plus lent : sans récompense intermédiaire,
  la tâche composée à récompense creuse n'est quasiment jamais découverte dans ce budget — contre
  **agent AVEC pile de buts (clé PUIS porte) = 100 % (500/500, TOUTES les graines à 100 %)**.
  Vérification isolée en plus : un but au budget `maxSteps=1` volontairement impossible est marqué
  `Failed` puis dépilé après 1 pas (`[ OK ]`), preuve directe du mécanisme d'expiration/abandon.
  ⚠️ Honnête : le compteur `BlockedDoorHits` (tentatives d'entrée sur la porte fermée) reste faible
  des deux côtés à ce budget serré (entraînement : 5 pour SANS but vs 1 pour AVEC but, cumulés sur
  5 graines ; éval : 0 des deux côtés) — le budget de 10 pas laisse peu d'occasions de heurter
  physiquement la porte plusieurs fois ; la preuve du jalon repose sur le taux de réussite (0 % vs
  100 %, chiffres réels), pas sur ce compteur secondaire.
- Reste (hors périmètre de ce jalon, cf. README du module) : besoins/désirs internes, réflexion
  (résumer le passé) façon *generative agents*, planification par recherche (au-delà d'une pile
  LIFO simple), buts non emboîtés/multi-objectifs simultanés.

## Jalon 4 — personnalité + raisonnement LLM — ✅ COMPLET (2026-07-26, build+run réels)

### Jalon 4a — traits de personnalité
- ✅ **3 traits réels, qui influencent RÉELLEMENT la décision** (`NkAgentPersonality.h/.cpp`) :
  `boldness` (audace : 1 = comportement de base/argmax Q pur, 0 = évite fortement les cases
  dangereuses — un trou ou une case ADJACENTE à un trou — même si leur Q est légèrement
  supérieure), `curiosity` (curiosité : probabilité d'action aléatoire **à la décision**, même en
  évaluation, indépendante de l'epsilon d'apprentissage de la politique), `patience`
  (multiplicateur du budget de pas `NkAgentGoal::maxSteps` avant abandon, Jalon 3). Design :
  **fonctions libres** (`NkGridWorldPeek`/`NkGridWorldRisk`/`NkSelectActionWithPersonality`/
  `NkPatienceAdjustedMaxSteps`) opérant sur les Q-valeurs DÉJÀ apprises
  (`NkAgentPolicy::QLearning()`, déjà exposée publiquement) — pas de réimplémentation du RL, pas
  de modification de `NkAgent::Step`/`StepWithGoals` (Jalons 1-3 intacts). Seul ajout à `NkAgent` :
  `StepWithPersonality` (additif), qui compose ces fonctions au lieu d'appeler
  `SelectAction`/`SelectGreedy`.
- 🎯 ✅ **Jalon prouvé par ablation** (`NKAgentTest`, test 4, exécution réelle 2026-07-26) : la
  politique **DÉJÀ entraînée** du test 1 (même table Q pour les deux profils, isole l'effet de la
  personnalité de tout effet d'apprentissage) est évaluée sous deux profils
  (`NkAgentPersonality::Bold`/`::Cautious`) sur la grille 5×5 à trous, 300 épisodes/profil :
  **Audacieux** (boldness=1.0, curiosity=0.35) : 216/300 succès (72 %), **84 chutes dans un trou**,
  29.08 % des pas empruntés adjacents à un trou, 10.2 pas/épisode en moyenne. **Prudent**
  (boldness=0.0, curiosity=0.02) : 21/300 succès (7 %), **1 seule chute**, 0.97 % des pas adjacents
  à un trou, 97.6 pas/épisode. Écart net et mesuré sur le risque (29.08 % → 0.97 %) et les chutes
  (84 → 1) — critère du jalon. Note honnête : le profil Prudent, très pénalisé sur le risque,
  oscille souvent près du seul chokepoint incontournable (les deux cases quittables depuis le
  départ sont TOUTES DEUX adjacentes au premier trou) et **time-out fréquemment** (succès 7 % vs
  72 %) — un vrai compromis sécurité/efficacité émergent du re-classement par risque, pas un bug :
  mesuré, pas corrigé/masqué. Trait `patience` vérifié séparément et directement (isolé, même
  esprit que la vérification `maxSteps=1` du test 3) : budget de but base=10 → patient
  (×1.6)=16 pas, impatient (×0.6)=6 pas. **`[ OK ]`** (4/4 tests `NKAgentTest`, exit code 0).

### Jalon 4b — raisonnement par LLM (NKInfer)
- ✅ **Pont réel NkAgent -> NKInfer** (`NkAgentLLMReasoning.h/.cpp`) : décision de repli pour un
  état jugé **ambigu** par la politique tabulaire (table Q neuve, Q(s,a)=0 pour toute action —
  aucun signal exploitable). Utilise le pipeline transformer Qwen2.5 RÉEL déjà livré par NKInfer
  (Jalon 3 : `NkQwen2LayerForward`/`NkKVCache`/`NkGGUFDequant`) — **pas un mock**, poids GGUF réels
  déquantifiés à la demande (streaming couche par couche, jamais tenus tous en RAM).
  - **Limite assumée et documentée** : aucun encodeur BPE n'existe dans ce dépôt (cf
    NKInfer/ROADMAP.md). Le « prompt » est donc un encodage NUMÉRIQUE (état et but encodés en
    tokens-CHIFFRES RÉELS du vocabulaire — '0'..'9', retrouvés par recherche exacte dans
    `tokenizer.ggml.tokens`), pas du texte libre. La décision est lue en restreignant les logits
    de sortie AUX 4 tokens-chiffres '0'..'3' (= les 4 actions de `rl::NkGridWorld`/
    `NkKeyDoorGridWorld`) — argmax parmi ces 4 candidats, sans génération ni décodage libres.
    **Optimisation additive** : seules les 4 lignes candidates de `lm_head`/`output.weight` sont
    déquantifiées (`DequantSelectiveRows`, même principe que `DequantEmbeddingRows` de
    `NKLLMInferTest`, généralisé aux deux usages), pas les 152064 lignes complètes — élimine le
    coût de ~6 s/décision observé dans `NKLLMInferTest`. C'est une preuve de **CÂBLAGE**
    bout-en-bout (état agent → tokens réels → forward Qwen2 réel → logits réels → action), **pas**
    une preuve de qualité de raisonnement linguistique (hors de portée sans tokenizer BPE,
    explicitement hors scope de ce jalon).
  - 🎯 ✅ **Jalon testable** (nouvelle application dédiée `Applications/NKAgentLLMTest` — séparée de
    `NKAgentTest` car chaque décision coûte ~1-2.5 min, garde les tests rapides intacts ; `jenga
    build --target NKAgentLLMTest --config Debug --platform Windows`, build+exécution réels et
    bloquants, 2026-07-26) : scénario = monde clé-puis-porte (`NkKeyDoorGridWorld`, même monde que
    le Jalon 3), agent à politique **neuve** (Q=0 partout). **3 décisions RÉELLES**, forward
    **complet sur les 28 couches réelles** de Qwen2.5 7B Instruct (mêmes poids/blob que
    `NKLLMInferTest`) : (1) départ→but "clé" : action=**haut**, logits réels
    `[12.77, 11.93, 11.93, 11.85]`, **153.5 s** ; (2) même requête RÉPÉTÉE À L'IDENTIQUE : même
    action (**haut**) — preuve de déterminisme (argmax pur, pas d'échantillonnage stochastique
    ici), **147.5 s** ; (3) état/but DIFFÉRENTS (clé→porte) : action=**haut**, logits réels
    `[12.85, 12.38, 12.58, 12.44]` **manifestement différents** des logits de (1)/(2) — preuve que
    la sortie dépend RÉELLEMENT de l'entrée (pas une constante câblée), **144.2 s**. **5/5 OK**,
    exit code 0. **Temps total mesuré : 446.0 s pour 3 forwards réels 28 couches (≈148.7 s/décision
    en moyenne)** — cohérent avec le ~2.9 s/couche déjà mesuré par `NKLLMInferTest` (28×2.9≈81 s)
    plus le coût de rechargement/redéquantification COMPLET des poids à chaque décision (aucun
    cache inter-décisions, cf `NkAgentLLMReasoning.h`) et des deux déquantifications sélectives
    (embeddings du prompt + 4 lignes `lm_head`). Choix technique : **forward COMPLET 28 couches
    retenu** (pas de réduction) — le budget temps de la mission le permettait largement (~7.5 min
    pour les 3 décisions, largement sous la limite d'exécution disponible) ; `NK_QWEN_NLAYERS`
    reste disponible pour un run réduit (validé en amont à 2 couches, ~9.5-10.3 s/décision, avant
    de lancer le run complet — même pratique de sanity-check que `NKLLMInferTest`).
  - ⚠️ **Limite honnête, mesurée, pas supposée** : **~149 s/décision** (build Debug, CPU strict —
    aucune exécution GPU, conforme à la contrainte de mission) — **PAS adapté au temps réel** dans
    l'état actuel. Ce pont est réservé à un TRÈS PETIT nombre de décisions hors-ligne/de repli (la
    mission en demandait 1-3 réelles ; 3 ont été exécutées ici, toutes réelles, aucune simulée).
    Une décision « urgente » doit continuer à passer par la politique tabulaire
    (`Step`/`StepWithGoals`/`StepWithPersonality`) — ce module n'est PAS appelé automatiquement
    dans leur boucle (appel explicite par le code appelant uniquement, vu le coût).

### Fichiers
`Kernel/AI/NKAgent/src/NKAgent/NkAgentPersonality.h/.cpp` (nouveau, Jalon 4a),
`NkAgentLLMReasoning.h/.cpp` (nouveau, Jalon 4b), `NkAgent.h/.cpp` (ajout additif
`StepWithPersonality`, aucune modification de `Step`/`StepWithGoals`), `NKAgent.jenga` (+ deps
`NKInfer`/`NKFileSystem`/`NKTime`), `config/modules.jenga` (registre central : `NKAgent` dépend
maintenant de `NKInfer`), `Applications/NKAgentTest/src/main.cpp` (test 4, Jalon 4a),
`Applications/NKAgentLLMTest/` (nouvelle app dédiée, Jalon 4b), `Nkentseu.jenga` (enregistrement de
la nouvelle app). Régression vérifiée : `NKCivilizationTest` (consommateur transitif de `NKAgent`)
build toujours avec succès après le changement du registre central.

## Plus tard
- ⬜ Relations sociales (mémoire d'autrui, confiance).
- ⬜ Émotions / états internes influençant la décision.
- ⬜ Communication entre agents (langage émergent).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
