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
- ⬜ Passer le tick sous `NkScheduler` NKECS quand d'autres systèmes coexisteront (rendu,
  ressources qui repoussent, etc.).
- ⬜ Temps qui s'écoule (fixe puis variable), ressources renouvelables, plus d'agents.
- ⬜ Observation/rejeu des trajectoires (Jalon 3).

## Jalon 2 — société émergente
- ⬜ Interactions sociales (échange, coopération, conflit).
- ⬜ Reproduction *située* (couplée à [NKEvolve](../NKEvolve/README.md)).
- ⬜ **Observation** : enregistrer l'histoire, rejouer, mesurer des tendances.
- 🎯 **Jalon « ça émerge »** : des structures non scriptées apparaissent (groupes, cycles).

## Jalon 3 — visualiser & analyser
- ⬜ Visualisation temps réel dans le moteur (rendu 2D/3D de la civilisation).
- ⬜ Tableaux de bord : démographie, ressources, relations.
- ⬜ Rejeu accéléré / pause / inspection d'un individu (passé, présent, buts).

## Jalon 4 — prospective
- ⬜ Scénarios **« what-if »** (changer un paramètre, comparer les trajectoires).
- ⬜ Mesure d'incertitude (plusieurs exécutions, distribution des issues).
- ⬜ Détection de motifs récurrents.

## Plus tard
- ⬜ Grande échelle (des milliers d'agents, LOD cognitif).
- ⬜ Culture / langage / institutions émergents.
- ⬜ Agents à raisonnement LLM (via NKInfer) pour des comportements riches.

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
