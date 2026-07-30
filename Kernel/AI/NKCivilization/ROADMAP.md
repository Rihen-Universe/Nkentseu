# NKCivilization — Roadmap

> Le monde vivant où tout converge (Phase 5). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — micro-monde (Phase 5) — ✅ substrat NKECS RÉEL livré (2026-07-25, build+run)

> ⚠️ **Correction de la prémisse du 2026-07-23** : le constat « NKECS pas branché au workspace »
> était **FAUX** (ou depuis dépassé) — `Nkentseu.jenga` contient bien
> `include("Kernel/Runtime/NKECS/NKECS.jenga")` (enregistré AVANT Noge qui en dépend), la cible
> compile dans le workspace, et Noge l'utilise déjà (pont `NkAgentComponent`/`NkAgentSystem` +
> démo `Applications/NkAgentEcsDemo`, 100 % éval, 2026-07-23). L'étape 1 du plan ci-dessous
> était donc déjà acquise.

- ✅ **Substrat** : espace + **ressources consommables** — lib **`NKCivilization`**
  (`Kernel/AI/NKCivilization/src/`, module Jenga enregistré dans `config/modules.jenga` +
  `Nkentseu.jenga`) : `NkCivGridState` (grille N×N, trous, but commun, règles de déplacement
  identiques à `rl::NkGridWorld` mais PURES — aucun curseur d'agent —, cases ressource
  consommées par le **premier arrivé**). *(Temps = tick discret via `Execute()` ; temps
  continu/variable = plus tard.)*
- ✅ **Les agents sont des ENTITÉS NKECS** (2026-07-25) : composants `NkCivPosition` (état
  y*N+x, POD) + `NkCivAgentRef` (pointeur **non possédant** vers `agent::NkAgent`, ordre de
  tour, compteurs d'interaction) — même philosophie que le pont Noge. **Choix de couche
  motivé** : NKCivilization (Kernel/AI) ne réutilise PAS le pont Noge
  (`Engine/Noge/.../NkAgentComponent`) car dépendre d'Engine depuis Kernel inverserait les
  couches ⇒ équivalent minimal rebâti sur **NKECS pur** (Kernel/Runtime).
- ✅ **System** : `NkCivAgentSystem` (un `ecs::NkSystem` standard, `Describe()`
  Writes<NkCivAgentRef/NkCivPosition> + Sequential) — par `Execute()` (= 1 tick) : query NKECS,
  tri déterministe par `turnOrder`, perception de l'occupation COURANTE, politique **apprise**
  gloutonne, collision, consommation de ressource, but/trou. Reprend la logique prouvée du
  prototype du 23/07, pilotée par le monde ECS au lieu d'une boucle `for` sur un tableau.
  *(Piloté par `Execute()` direct comme la démo Noge — passage par `NkScheduler` complet quand
  plusieurs systèmes coexisteront ; le scheduler NKECS est encore STL-interne, chantier NKECS.)*
- ✅ **Interactions émergentes MESURÉES et non-nulles** (`NKCivilizationTest` étendu, build+run
  réels 2026-07-25, exit 0, 3/3 OK) : départs {4, 9, 20} choisis pour que deux chemins appris se
  croisent (agent 0 juste derrière agent 1 sur le couloir droit) :
  - **Collision DÉCLENCHÉE** (le prototype du 23/07 ne l'avait jamais vue) : **1 blocage** au
    tick 0 — l'agent 0 (case 4) vise 9, encore occupée par l'agent 1 ⇒ reste sur place
    (`blockedCount=1` sur l'agent 0, total système = 1).
  - **Compétition de ressources** : 4 cases ressource (9, 14, 21, 23), **4/4 consommées** ; la
    case 14, sur le chemin des DEUX agents du couloir, est prise par l'agent 1 (passé premier),
    l'agent 0 la trouve vide ⇒ **répartition divergente 1/1/2** (agent 0 : case 9 ; agent 1 :
    case 14 ; agent 2 : cases 21+23).
  - **3/3 agents atteignent le but commun** via leur politique apprise (Phase 1 inchangée :
    98,5 % / 98 % / 100 % de succès fin d'entraînement individuel), en 5/3/4 pas.
- 🎯 ✅ **Atteint sur le vrai substrat** : N agents ECS + system + mesure d'interaction non-nulle.

### Reste (vers Jalon 2)
- ✅ **Passage sous `ecs::NkScheduler`** (2026-07-26, build+run réels) : `NKCivilizationTest`
  et `NKCivilizationSocialTest` (fonction `RunOneCondition`) n'appellent plus
  `civSystem.Execute(world, dt)` directement dans leur boucle `for` — le tick passe
  maintenant par `scheduler.AddSystem<civ::NkCivAgentSystem>(&grid)` +
  `scheduler.Run(world, dt)`, exactement le pattern déjà utilisé par `NkEngineLayer`
  (Noge) pour brancher `NkPhysicsSystem`/`NkAudioSystem`. Un seul système enregistré
  (groupe `Update`, `Sequential()` — l'ordre de tour reste un contrat), donc
  `scheduler.Run()` exécute exactement UN appel à `Execute()` par tick, plus
  `DrainEvents()`/`FlushDeferred()` (no-op ici, aucun event/changement structurel émis).
  **Régression revérifiée BIT-EXACT** : sortie complète de `NKCivilizationTest` diffée
  ligne à ligne avant/après migration — 0 écart sur les lignes préexistantes (1 collision,
  4/4 ressources, distance 11, heatmap, stats par agent identiques) ; `NKCivilizationSocialTest`
  rejoué sur ses 5 graines × 2 conditions — chiffres agrégés identiques au tableau déjà
  documenté ci-dessus (collisions 6,8, ressources A=2/B=6, Gini A=66,7 %/B=0 %,
  co-occupation=0 des deux côtés). Le scheduler NKECS lui-même reste STL-interne
  (`std::thread`/`std::vector`, chantier NKECS séparé, hors périmètre de ce module) —
  seul le CHEMIN D'APPEL du tick a changé côté NKCivilization, pas son implémentation.
- ✅ **Passage à l'échelle mesuré** (2026-07-26, nouvelle app `NKCivilizationScaleTest`,
  build+run Debug réels, bloquants) : grille 15×15 vide (225 états, sans trous — le but est
  d'isoler le coût du TICK ECS, pas de re-prouver la richesse RL déjà faite ailleurs),
  N agents à départs distincts déterministes convergeant vers un but commun au coin opposé
  (donc trafic/collisions réels, pas un cas dégénéré). Temps par tick mesuré RÉELLEMENT
  (`NKTime::NkChrono`, zéro STL) autour de chaque `scheduler.Run()` :

  | N agents | ticks pour terminer | but atteint | tick moyen (Debug) | tick max (Debug) |
  |---|---|---|---|---|
  | 10  | 33  | 10/10  | 0,0132 ms | 0,0270 ms |
  | 25  | 44  | 25/25  | 0,0452 ms | 0,1293 ms |
  | 50  | 60  | 50/50  | 0,1221 ms | 0,2495 ms |
  | 100 | 100 | 100/100 | 0,3391 ms | 0,7657 ms |

  10→100 agents (×10) fait passer le tick moyen de 0,0132 ms à 0,3391 ms (×25,8) — la
  collision `O(N²)` de `NkCivAgentSystem::Execute` (boucle imbriquée sur tous les agents
  actifs pour détecter l'occupation) domine visiblement la croissance ; testé avec un
  plafond de ticks large (200) pour confirmer que N=100 termine bien NATURELLEMENT à
  100 ticks (`AnyActiveLastTick()==false`), pas tronqué par un plafond trop bas. **Honnêteté
  de portée** : chiffres Debug (non optimisés), pas des temps Release de production — dit
  explicitement dans les logs de l'app. Même à 100 agents, tick=0,34 ms en moyenne reste
  largement sous un budget de frame (16,6 ms à 60 Hz) — pas de mur de performance observé à
  cette échelle, mais la tendance quasi-quadratique documentée ci-dessus doit être gardée à
  l'esprit avant de pousser vers « des milliers d'agents » (section « Plus tard »).
  Export CSV périodique (timeline démographie/ressources) écrit pour chaque palier — voir
  Jalon 3 ci-dessous. Fichier : `Applications/NKCivilizationScaleTest/src/main.cpp`.
- ⬜ Temps qui s'écoule (fixe puis variable) — non traité (le tick reste discret, un pas =
  une action ; hors périmètre de cette session).
- ✅ Observation/rejeu des trajectoires — voir section dédiée ci-dessous (2026-07-25).

## Outils d'observation — ✅ livrés (2026-07-25, build+run réels, 6/6 OK)

> Réponse à la Phase 5 de `Kernel/AI/ROADMAP.md` : « Outils d'observation : enregistrer, rejouer,
> analyser les trajectoires » — condition du jalon « ça émerge » (il faut pouvoir OBSERVER la
> société pour y détecter des comportements non scriptés). Zéro STL/NKMemory/NKLogger dans la
> lib `NKCivilization` (le logging reste côté `NKCivilizationTest`).

- ✅ **`civ::NkCivRecorder`** (`NkCivRecorder.h/.cpp`) : journal EN MÉMOIRE — `BeginSession`
  capture l'en-tête statique du monde (taille grille, but, trous, cases ressource scannées sur
  la grille encore intacte, départs par agent) ; `BeginTick`/`RecordAgent`/`EndTick` capturent
  CHAQUE tick (position + action + bitmask d'événements — actif/bloqué/déplacé/ressource
  collectée/but atteint/trou/terminé — par agent, + ressources restantes). Branché sur
  `NkCivAgentSystem` via `SetRecorder()` (pointeur non possédant, optionnel — pur observateur,
  ne modifie AUCUNE logique de simulation). Sérialisation binaire versionnée **`.nkciv`**
  (magic `"NCIV"`, little-endian, même pattern que le `.nkmec` de `NkMeshEditRecorder` —
  `NKRenderer/Mesh/NkEditMesh.h/.cpp`), persistée via `NkFile::WriteAllBytes`/`ReadAllBytes`
  (`NKFileSystem`).
- ✅ **`civ::NkCivReplayer`** (`NkCivReplay.h/.cpp`) : rejoue un journal chargé **TICK PAR TICK**
  en **lecture PURE** (`NextTick()`/`AgentState()`/`AgentDone()` — zéro re-simulation, zéro appel
  à une politique ou à `NkCivGridState`). `VerifyExact()` compare deux journaux structurellement
  (en-tête + toutes les frames).
- ✅ **`civ::NkCivAnalyzer`** (`NkCivAnalyzer.h/.cpp`) : une passe sur un journal, sans
  re-simulation — stats **par agent** (pas, blocages + plus longue série consécutive,
  déplacements, distance de Manhattan parcourue, ressources collectées, tick d'arrivée, issue),
  **globales** (heatmap TEXTE d'occupation base36, distance totale de la société, taux de
  contention des ressources), et **détection d'événements remarquables** (blocage mutuel répété
  = agent bloqué ≥2 ticks consécutifs ; domination des ressources = agent avec >50 % du total
  consommé) — texte prêt à logger.
- ✅ **Preuve réelle** (`NKCivilizationTest` étendu à 4 phases, build+run Debug, exit 0,
  **6/6 OK**) :
  - Phase 3/4 — persistance + round-trip : journal de 6 frames sauvé (`NKCivilizationTest_
    session.nkciv`, **324 octets**), rechargé, comparé **OCTET PAR OCTET** (324/324 identiques)
    ET **structurellement** (`Equals`) ET via `NkCivReplayer::VerifyExact` — les trois **bit-exacts**.
  - Phase 4/4 — rejeu : les 6 ticks rejoués (lecture pure du journal RECHARGÉ, donc du DISQUE)
    reproduisent EXACTEMENT (assertions `NKENTSEU_ASSERT_MSG` + vérif croisée) l'état **VIVANT**
    capturé pendant la simulation originale (position + flag terminé, 3 agents/tick) — la preuve
    ne compare pas seulement le journal à lui-même, mais le rejeu à ce qui s'est RÉELLEMENT
    passé.
  - Analyse affichée sur le journal rechargé : heatmap 5×5 réelle (`. . . . 0` / `. O . . 1` /
    `. . O . 1` / `. . . O 1` / `0 0 0 0 2`), stats par agent (ex. agent 0 : pas=5, blocages=1,
    distance=4, ressources=1, arrivée tick=4 ; agent 2 : pas=4, ressources=2, arrivée tick=3),
    globales (11 cases parcourues au total, contention ressources = 100 % — 4/4), 0 événement
    remarquable sur CETTE session précise (seuils non franchis : blocage max consécutif=1 sur
    les 3 agents, meilleur partage ressources 2/4=50 % pile — pas de domination stricte).
  - Fichiers : `Kernel/AI/NKCivilization/src/NKCivilization/{NkCivRecorder,NkCivReplay,
    NkCivAnalyzer}.{h,cpp}`, `Applications/NKCivilizationTest/src/main.cpp`.

## Jalon 2 — société émergente
- 🟡 **Interactions sociales (échange, coopération, conflit)** — UNE règle ajoutée et testée
  (2026-07-25, build+run réels), résultat **honnête et mitigé** (voir protocole/résultats
  ci-dessous) : ni « ça marche clairement », ni « rien ne se passe » — un effet réel et
  reproductible sur UNE dimension, un résultat NUL sur une autre, documentés sans les cacher
  l'un l'autre.
- ⬜ Reproduction *située* (couplée à [NKEvolve](../NKEvolve/README.md)).
- ✅ **Observation** : enregistrer l'histoire, rejouer, mesurer des tendances — voir section
  dédiée ci-dessus. Les outils (`NkCivRecorder`/`NkCivReplayer`/`NkCivAnalyzer`) ont servi
  TELS QUELS (aucune modification) à l'expérience ci-dessous, plus quelques mesures dérivées
  calculées dans le harnais de test à partir de leurs sorties publiques (co-occupation,
  dispersion spatiale, indice de Gini).
- 🎯 **Jalon « ça émerge »** : des structures non scriptées apparaissent (groupes, cycles).
  **Évaluation honnête après l'expérience ci-dessous : PARTIELLEMENT atteint** — un motif
  collectif réel, mesuré, non scripté existe (égalisation de l'accès aux ressources), mais le
  volet spatial (co-occupation/clustering) reste NON TESTÉ (jamais déclenché sur cette carte) —
  ni confirmé ni infirmé, pas assez de preuve pour trancher.

### Règle sociale ajoutée (2026-07-25) — choix, implémentation, protocole, résultats

**Choix de règle** : option **(b)** — ressource limitée **renouvelable** (stock borné,
régénération/tick) avec **collision exemptée** sur les cases ressource (récolte partagée
possible) plutôt que (a) échange direct entre agents co-localisés. Raison du choix : la règle
de collision existante rend la co-localisation **structurellement impossible** partout sauf au
but (`NkCivAgentSystem::Execute`) ; (b) s'appuie sur le substrat ressource déjà livré
(`NkCivGridState`) avec un changement minimal, réversible et **rétrocompatible au bit près**
(généralisation binaire consommée/pas-consommée -> stock borné ; `capacity=1, regenPerTick=0,
social=OFF` reproduit EXACTEMENT l'ancien comportement), alors que (a) aurait exigé de relâcher
la collision PARTOUT — un changement plus large et plus risqué pour la preuve Jalon 1 déjà
vérifiée.

**Implémentation** (fichiers modifiés, tous dans la lib déjà vérifiée, changements additifs) :
- `NkCivGridState.h/.cpp` : `AddResource(state, capacity=1, regenPerTick=0)` (était
  `AddResource(state)`), stock borné par case (`mCapacity`/`mStock`/`mRegenPerTick`),
  `IsResourceCell`, `Stock`, `RegenerateTick()`, `EnableSocialSharing`/`SocialSharingEnabled`.
- `NkCivAgentSystem.cpp` : la collision est exemptée quand la case visée est une case ressource
  ET `SocialSharingEnabled()` ; `RegenerateTick()` appelé une fois par `Execute()` (no-op si
  `regenPerTick=0` partout).
- **Régression Jalon 1 revérifiée à l'identique** : `NKCivilizationTest` rebuild+run (Debug),
  **6/6 OK**, chiffres BIT-À-BIT identiques à ceux déjà documentés plus haut (1 blocage, 4/4
  ressources, distance totale 11, contention 100 %) — le changement est bien rétrocompatible.
- Nouvelle application `Applications/NKCivilizationSocialTest` (harnais d'expérience, ne
  modifie ni `NkCivRecorder` ni `NkCivReplay` ni `NkCivAnalyzer`).

**Protocole (A/B, 5 graines)** : grille 6×6 (36 états), but=35, mur ligne y=3 (états 18-23) avec
DEUX brèches (19 et 22) qui portent aussi les deux cases ressource — tous les 6 agents doivent
obligatoirement passer par l'une des deux brèches (géométrie imposée, pas de triche). 3 agents
démarrent côté brèche 19, 3 côté brèche 22. Pour CHAQUE graine (seeds 1 à 5) : les 6 agents sont
entraînés **une seule fois** (Q-learning, 4000 épisodes/agent, mêmes hyperparamètres que
`NKCivilizationTest`) puis leurs politiques **gelées** (`SelectGreedy` est `const`) sont
rejouées dans **deux mondes indépendants** : (A) baseline — `capacity=1, regenPerTick=0,
social=OFF` (= Jalon 1 inchangé) ; (B) règle sociale — `capacity=5, regenPerTick=1,
social=ON`. Seule la règle du monde diffère entre A et B pour une même graine -> effet isolé.

**Résultats chiffrés réels (build Debug, run bloquant, 5 graines × 2 conditions = 10 runs)** :

| Mesure (moyenne/5 graines) | A — baseline | B — règle sociale |
|---|---|---|
| Agents au but | 6/6 (toutes graines, les deux conditions) | 6/6 |
| Collisions déclenchées | 6,8 (6 ou 8 selon la graine) | **6,8 — identique à A** |
| Ressources consommées | 2 (constant, toutes graines) | **6 (constant, toutes graines)** |
| Part du plus gros collecteur | 50 % | **16,7 % (= 1/6, parfaitement égal)** |
| Indice de Gini (répartition ressources) | **66,7 %** | **0 %** |
| Co-occupation sur case ressource (ticks) | 0 (attendu : structurellement impossible) | **0 (mesuré, pas seulement attendu)** |
| Dispersion spatiale moyenne (agents actifs) | 2,60 | 2,60 — identique à A |
| Événement « domination ressources » (NkCivAnalyzer) | 0/5 graines | 0/5 graines |

Détail par graine (A vs B, identique dans la forme sur les 5) : graines {1,3,5} -> 12 ticks, 6
collisions ; graines {2,4} -> 13 ticks, 8 collisions ; dans TOUS les cas ressources A=2/B=6,
Gini A=66,7 %/B=0 %, co-occupation=0 des deux côtés.

**Diagnostic (trace tick-par-tick, graine 1, les deux conditions — dans le code, non modifié
depuis)** : les collisions mesurées se produisent en réalité à la case 34 (le goulot
« approche du but » à une seule case de large, PAS une case ressource), à cause de l'ordre de
tour déterministe (un agent plus loin dans l'ordre voit la case occupée par un agent qui n'a
« pas encore joué » ce tick puis la libère) — un artefact déjà documenté au Jalon 1. Les deux
brèches à ressource (19, 22) ne voient JAMAIS deux agents actifs s'y trouver au même tick sur
cette carte : les 3 agents de chaque côté partent à des distances Manhattan différentes de leur
brèche et l'atteignent donc à des ticks différents. **Conséquence honnête** : le volet
« collision exemptée / co-occupation » de la règle sociale n'a **jamais été exercé** par cette
expérience — ni prouvé ni réfuté, simplement non testé avec cette géométrie précise.

**Conclusion honnête (positive sur un point, nulle sur un autre — aucun forçage)** :
1. **Effet réel, mesuré, reproductible sur les 5 graines** : le **renouvellement temporel** du
   stock (indépendamment de toute co-occupation) fait passer la répartition des ressources
   d'une concentration extrême (Gini 66,7 %, 2 agents sur 6 raflent tout, les 4 autres
   n'obtiennent jamais rien car la ressource classique ne réapparaît pas) à une égalité
   parfaite (Gini 0 %, les 6 agents récoltent chacun exactement 1 unité, simplement parce que
   le stock a le temps de se régénérer entre deux passages). C'est un motif COLLECTIF non
   scripté : **aucun agent n'est programmé pour « partager »** — leur politique Q-learning
   n'a aucune récompense liée aux ressources (uniquement coût de pas + but + trou) ; l'égalité
   observée est une conséquence STRUCTURELLE du renouvellement, pas une stratégie apprise ou
   décidée.
2. **Résultat NUL (pas négatif, juste non déclenché)** sur le volet co-occupation/clustering
   spatial : 0 co-occupation mesurée en B sur les 5 graines — la carte choisie ne crée jamais
   la situation où deux agents actifs visent la même case ressource au même tick. Les
   collisions mesurées (identiques en A et B) se produisent ailleurs (goulot du but), pas aux
   brèches à ressource. **Ce point précis du jalon « ça émerge » (clustering spatial via
   partage forcé) reste donc NON TRANCHÉ par cette expérience** — pas de dégradation, pas
   d'amélioration : simplement pas testé avec succès à cette échelle/cette géométrie.
3. **Limite de protocole à noter honnêtement** : sur cette carte 6×6 déterministe, la graine
   d'entraînement ne fait varier le résultat final QUE de façon marginale (2 profils distincts
   sur 5 graines, ticks=12 ou 13, collisions=6 ou 8) — l'environnement est assez petit/simple
   pour que Q-learning converge quasi systématiquement vers le même chemin optimal quel que
   soit le germe d'exploration. Une carte plus grande/plus stochastique ou des départs
   variables par graine seraient nécessaires pour des graines réellement indépendantes et pour
   avoir une chance de déclencher la co-occupation testée au point 2.
4. **Statut du jalon « ça émerge »** : **partiellement atteint**. Un motif collectif réel existe
   (égalisation non scriptée de l'accès aux ressources par le renouvellement), mais il n'a PAS
   été démontré que la levée de la collision (le volet « co-occupation »/coopération spatiale
   au sens propre) produit un quelconque clustering ou spécialisation — cette sous-hypothèse
   reste ouverte pour un prochain protocole (carte plus grande, contrainte de simultanéité
   explicite dans le placement des départs).

## Jalon 3 — visualiser & analyser
- ⬜ Visualisation temps réel dans le moteur (rendu 2D/3D de la civilisation). **Non traité,
  limite honnête assumée** : le temps de cette session a été mis sur l'export texte/CSV
  (ci-dessous) plutôt que sur un rendu dans le moteur — aucune preuve superficielle
  produite à la place.
- ✅ **Tableaux de bord : export CSV** (2026-07-26, `civ::NkCivExporter`,
  `NkCivExport.h/.cpp`, additif — ne modifie ni `NkCivRecorder` ni `NkCivAnalyzer`) :
  - `ExportTimelineCsv(journal, path)` : une ligne par TICK (`tick,active,done,
    reachedGoal,fell,resourcesRemaining`) — démographie + ressources au fil du temps.
  - `ExportAgentsCsv(analyzer, path)` : une ligne par AGENT, stats déjà calculées par
    `NkCivAnalyzer` (pas de recalcul).
  - Écriture via `NKFileSystem::NkFile::WriteAllText` (même pattern que
    `NkCivRecorder::SaveToFile`), lecture PURE des données publiques déjà capturées
    (zéro re-simulation).
  - **Preuve réelle** : `NKCivilizationTest` (build+run Debug, exit 0, **7/7 OK** — le
    7ᵉ check ajouté est l'export lui-même) écrit `NKCivilizationTest_timeline.csv`
    (6 lignes+en-tête) et `NKCivilizationTest_agents.csv` (3 lignes+en-tête), contenu
    vérifié cohérent avec les stats déjà loggées. `NKCivilizationScaleTest` écrit un CSV
    timeline PAR PALIER d'agents (10/25/50/100 — 4 fichiers réels, jusqu'à 101 lignes
    pour N=100) : c'est l'export **périodique** demandé (une timeline par étape de
    montée en charge), pas un one-shot isolé.
- ⬜ Rejeu accéléré / pause / inspection d'un individu (passé, présent, buts) — non traité
  (`NkCivReplayer` le permettrait techniquement — `NextTick()`/`AgentState()` — mais aucune
  UI/CLI interactive n'a été construite par-dessus).

## Jalon 4 — prospective
- ✅ **Scénario « what-if » minimal et réel** (2026-07-26, `NKCivilizationSocialTest`,
  fonctions `RunWhatIfFork`/`VerifySharedPrefix`/`RunWhatIfExperiment`, additif — n'altère
  pas le protocole A/B à 5 graines déjà validé ci-dessus) : PAS deux runs indépendants
  (comme le protocole A/B), mais UNE SEULE trajectoire rejouée depuis un point donné avec
  un paramètre changé EN COURS DE ROUTE, sur un objet grille VIVANT — pas de reconstruction
  depuis un journal (`NkCivRecorder` ne capture pas le stock par case, donc pas assez
  d'information pour reconstruire un monde à un tick donné ; d'où le choix de deux forks
  vivants partageant un préfixe déterministe plutôt qu'un rechargement depuis disque) :
  - Fork **baseline** tourne en condition A (Jalon 1/2 inchangée) du début à la fin.
  - Fork **what-if** est IDENTIQUE au fork baseline jusqu'au tick de bascule (`branchTick=4`,
    mêmes politiques gelées, même grille au départ), puis À CE TICK PRÉCIS,
    `EnableSocialSharing(true)` + `SetResourceParams(capacity=5, regenPerTick=1)` (nouvelle
    méthode additive sur `NkCivGridState`, voir ci-dessous) sont appelés sur SA grille déjà
    vivante — il diverge pour le reste du run.
  - **Vérification OBJECTIVE (pas de triche)** : le préfixe `[0, branchTick)` est relu
    TICK PAR TICK sur les deux journaux via `NkCivReplayer` (`NextTick`/`AgentState`/
    `AgentDone`) et comparé — **bit-exact confirmé** sur les 4 ticks avant divergence
    (`NKENTSEU_ASSERT_MSG`, build+run réel). `NkCivReplayer::VerifyExact()` n'est pas
    utilisable tel quel ici (il exige des journaux de même longueur totale, or les deux
    forks n'ont pas la même durée par construction) — préfixe relu manuellement avec la
    même API de lecture pure.
  - **Résultat chiffré réel** (une graine dédiée, seed 424242, indépendante des 5 graines
    A/B) : baseline = 12 ticks, 6/6 au but, 6 collisions, 2 ressources consommées,
    Gini=66 % ; what-if (bascule au tick 4) = 12 ticks, 6/6 au but, 6 collisions,
    **4 ressources consommées (+2)**, **Gini=33 % (−33 points)**. Effet mesurable et
    cohérent avec le protocole A/B déjà validé (renouvellement ⇒ répartition plus égale),
    obtenu ici par DIVERGENCE mi-parcours plutôt que par deux runs indépendants dès le
    départ — le nombre de ticks total n'a pas changé (le goulot du but reste le facteur
    limitant, pas la règle ressource).
  - Nouvelle méthode additive `NkCivGridState::SetResourceParams(state, capacity,
    regenPerTick)` (`NkCivGridState.h/.cpp`) : met à jour capacité/régénération d'une case
    ressource DÉJÀ enregistrée sans reset du stock courant (clamp seulement si besoin) —
    nécessaire car `AddResource()` seul ne peut pas modifier une case existante sans créer
    un doublon ; personne d'autre ne l'appelait avant ce chantier, zéro risque de
    régression.
- ⬜ Mesure d'incertitude (plusieurs exécutions, distribution des issues) — non traité
  (le what-if ci-dessus compare DEUX trajectoires précises, pas une distribution sur
  plusieurs graines/bascules — extension naturelle mais pas faite ici, honnêtement).
- ⬜ Détection de motifs récurrents — non traité.

## Plus tard
- ⬜ Grande échelle (des milliers d'agents, LOD cognitif).
- ⬜ Culture / langage / institutions émergents.
- ⬜ Agents à raisonnement LLM (via NKInfer) pour des comportements riches.

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
