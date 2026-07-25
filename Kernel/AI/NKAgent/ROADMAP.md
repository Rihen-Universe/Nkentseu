# NKAgent — Roadmap

> L'être qui se souvient, perçoit et décide (Phase 4). ⬜ à faire · 🟡 en cours · ✅ fait.

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

## Jalon 3 — buts & planification (le futur)
- ⬜ Besoins / désirs internes (faim, sécurité, sociabilité…).
- ⬜ Choix d'actions orienté **but** ; planification simple.
- ⬜ Réflexion (résumer le passé pour mieux décider) — style *generative agents*.

## Jalon 4 — personnalité
- ⬜ Traits qui font varier le comportement d'un agent à l'autre.
- ⬜ Raisonnement par **LLM** (via NKInfer) pour les décisions complexes.

## Plus tard
- ⬜ Relations sociales (mémoire d'autrui, confiance).
- ⬜ Émotions / états internes influençant la décision.
- ⬜ Communication entre agents (langage émergent).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
