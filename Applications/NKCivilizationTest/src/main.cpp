// =============================================================================
// NKCivilizationTest — Phase 5, Jalon 1 sur le VRAI substrat NKECS (simulation
// ECS) + outils d'OBSERVATION (enregistrement / rejeu / analyse, sans STL).
//
// Étape franchie depuis le prototype pré-ECS du 2026-07-23 (qui gérait les
// positions dans un tableau `pos[]` et une boucle `for` à la main, et dont la
// règle de collision n'avait jamais été VUE se déclencher) :
//   - les agents sont désormais des ENTITÉS d'un ecs::NkWorld (NKECS,
//     Kernel/Runtime — branché et buildable dans le workspace), composants
//     civ::NkCivAgentRef (référence la politique apprise) + civ::NkCivPosition ;
//   - c'est civ::NkCivAgentSystem (un ecs::NkSystem standard) qui joue chaque
//     tick : query NKECS -> ordre de tour déterministe -> perception de
//     l'occupation -> politique gloutonne apprise -> collision/ressources ;
//   - le substrat partagé (grille, trous, but commun, RESSOURCES consommables)
//     est civ::NkCivGridState (règles de déplacement identiques à NkGridWorld).
//
// Interactions émergentes MESURÉES (le jalon exige du non-nul, prouvé) :
//   1. COLLISION : les départs sont choisis pour que deux chemins appris se
//      croisent (agent 0 démarre en 4, JUSTE DERRIÈRE l'agent 1 qui démarre
//      en 9, sur le même couloir vertical droit vers le but 24 ; l'agent 0
//      agit AVANT l'agent 1 dans le tour => au tick 0 sa case visée (9) est
//      encore occupée => blocage garanti si les politiques suivent le couloir).
//   2. COMPÉTITION DE RESSOURCES : 4 cases ressource (9, 14, 21, 23), chacune
//      consommée par le PREMIER agent qui y entre. La case 14 est sur le
//      chemin des DEUX agents du couloir : le premier passé la prend, l'autre
//      trouve la case vide -> les scores divergent selon les politiques.
//
// Phase 1 (inchangée) : chaque agent apprend D'ABORD seul sa politique
// Q-learning sur SA PROPRE instance de monde (même terrain, départ propre),
// comme NKAgentTest — l'apprentissage n'est pas partagé, seul le résultat l'est.
//
// OUTILS D'OBSERVATION (Phase 5, nouveau) — jalon « ça émerge » exige de
// pouvoir OBSERVER la société pour y détecter des comportements non scriptés :
//   3. ENREGISTREMENT : civ::NkCivRecorder branché sur civ::NkCivAgentSystem
//      capture CHAQUE tick (position/action/événements par agent, ressources
//      restantes) -> sauvegardé sur disque au format `.nkciv` (magic "NCIV").
//   4. REJEU : le journal est rechargé du disque puis REJOUÉ TICK PAR TICK
//      via civ::NkCivReplayer (lecture PURE, aucune re-simulation) ; des
//      ASSERTIONS comparent chaque état rejoué à l'état VIVANT capturé
//      pendant la simulation ORIGINALE -> round-trip bit-exact prouvé.
//   5. ANALYSE : civ::NkCivAnalyzer extrait du journal rechargé des stats par
//      agent, une heatmap texte d'occupation, un taux de contention des
//      ressources et détecte des événements remarquables (blocage répété,
//      domination des ressources).
//
// Reste (2026-07-26) :
//   6. ORDONNANCEMENT : le tick passe désormais par ecs::NkScheduler
//      (AddSystem<civ::NkCivAgentSystem> + scheduler.Run()), plus par un
//      appel direct à civSystem.Execute() -- même pattern que NkEngineLayer
//      (Noge) pour NkPhysicsSystem/NkAudioSystem. Comportement inchangé au
//      bit près (régression revérifiée), seul le chemin d'appel change.
//   7. EXPORT CSV (Jalon 3) : civ::NkCivExporter écrit un tableau de bord
//      minimal exploitable hors moteur (timeline démographie/ressources par
//      tick + stats par agent), depuis les données déjà capturées par
//      NkCivRecorder/NkCivAnalyzer (aucune re-simulation).
// =============================================================================
#include "NKCivilization/NkCivAgentSystem.h"
#include "NKCivilization/NkCivRecorder.h"
#include "NKCivilization/NkCivReplay.h"
#include "NKCivilization/NkCivAnalyzer.h"
#include "NKCivilization/NkCivExport.h"
#include "NKECS/System/NkScheduler.h"
#include "NKLogger/NkLog.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/Assert/NkAssert.h"

using namespace nkentseu;
using namespace nkentseu::ai;

namespace {

	const uint32 kSize = 5;
	const uint32 kGoal = 24;
	const uint32 kNumAgents = 3;
	// Agent 0 en 4 (haut-droit), agent 1 en 9 (même couloir droit, une case
	// devant) : croisement force au tick 0 (0 agit avant 1). Agent 2 en 20
	// (bas-gauche) : couloir du bas, sans contention.
	const uint32 kStarts[kNumAgents] = {4, 9, 20};
	// Ressources : 9 et 14 sur le couloir droit (14 = contestée par agents 0
	// et 1), 21 et 23 sur le couloir bas (agent 2).
	const uint32 kResources[] = {9, 14, 21, 23};
	const uint32 kNumResources = 4;

	const char *kJournalPath = "NKCivilizationTest_session.nkciv";

	// Une ligne du plateau : '0'/'1'/'2' = agent actif, 'G' = but, 'O' = trou,
	// '*' = ressource encore disponible, '.' = case libre.
	NkString BoardRow(ecs::NkWorld &world, const NkVector<ecs::NkEntityId> &entities, const civ::NkCivGridState &grid,
					  uint32 y) {
		NkString row;
		for (uint32 x = 0; x < kSize; ++x) {
			const uint32 s = y * kSize + x;
			int occupant = -1;
			for (nk_size i = 0; i < entities.Size(); ++i) {
				const civ::NkCivAgentRef &ac = world.GetRef<civ::NkCivAgentRef>(entities[i]);
				if (!ac.done && world.GetRef<civ::NkCivPosition>(entities[i]).state == s)
					occupant = (int)i;
			}
			row += ' ';
			if (occupant >= 0)
				row += (char)('0' + occupant);
			else if (s == kGoal)
				row += 'G';
			else if (grid.IsHole(s))
				row += 'O';
			else if (grid.HasResource(s))
				row += '*';
			else
				row += '.';
		}
		return row;
	}

} // namespace

int main() {
	logger.Info("=== NKCivilizationTest : {0} NkAgent comme ENTITES NKECS sur un monde-grille partage ===",
				kNumAgents);
	logger.Info("-- Substrat : ecs::NkWorld + civ::NkCivAgentSystem (NKECS branche au workspace) --");

	NkVector<uint32> holes;
	holes.PushBack(6);
	holes.PushBack(12);
	holes.PushBack(18);

	// --- Phase 1 : chaque agent apprend SEUL sa politique (comme NKAgentTest), --
	// --- sur SON PROPRE monde (memes regles, depart qui lui est propre).      --
	NkVector<rl::NkGridWorld> trainWorlds;
	trainWorlds.Reserve(kNumAgents);
	NkVector<agent::NkAgent> agents;
	agents.Reserve(kNumAgents);
	for (uint32 i = 0; i < kNumAgents; ++i)
		trainWorlds.EmplaceBack(kSize, kStarts[i], kGoal, holes, /*stepCost*/ 0.02f);
	for (uint32 i = 0; i < kNumAgents; ++i) {
		agent::NkAgentConfig config;
		config.memoryCapacity = 64;
		config.alpha = 0.1f;
		config.gamma = 0.99f;
		config.epsilon = 1.0f;
		config.seed = 42u + i; // graines distinctes -> exploration independante
		agents.EmplaceBack(trainWorlds[i], config);
	}

	const int episodes = 3000;
	const uint32 maxTrainSteps = 100;
	logger.Info("-- Phase 1/4 : {0} agents apprennent SEULS leur politique ({1} episodes chacun) --", kNumAgents,
				episodes);
	for (uint32 i = 0; i < kNumAgents; ++i) {
		int reached = 0;
		for (int e = 1; e <= episodes; ++e) {
			float eps = 1.0f - (float)e / (float)(episodes * 0.8);
			if (eps < 0.05f)
				eps = 0.05f;
			agents[i].Policy().SetEpsilon(eps);
			agent::NkAgentEpisodeResult r =
				agent::RunAgentEpisode(trainWorlds[i], agents[i], /*learn*/ true, maxTrainSteps);
			if (e > episodes - 200)
				reached += r.reachedGoal ? 1 : 0;
		}
		logger.Info("  agent {0} (depart={1}) : succes sur les 200 derniers episodes = {2}%", i, kStarts[i],
					(double)reached / 2.0);
	}

	// --- Phase 2 : le micro-monde NKECS -- entites + systeme + grille partagee, --
	// --- avec un NkCivRecorder BRANCHE : chaque tick est capture au vol.       --
	logger.Info("-- Phase 2/4 : monde NKECS partage -- {0} entites, tick par civ::NkCivAgentSystem (ENREGISTRE) --",
				kNumAgents);

	civ::NkCivGridState grid(kSize, kGoal, holes);
	for (uint32 r = 0; r < kNumResources; ++r)
		grid.AddResource(kResources[r]);

	ecs::NkWorld world;
	// Jalon 1 "reste à faire" (2026-07-25) : le tick passe désormais par le
	// VRAI ordonnanceur NKECS (ecs::NkScheduler), plus par un appel direct à
	// civSystem.Execute() dans la boucle `for` -- même pattern que
	// NkEngineLayer (Noge) qui enregistre NkPhysicsSystem/NkAudioSystem via
	// AddSystem<T>() puis pilote la frame via scheduler.Run(). Un seul système
	// enregistré ici (groupe Update, Sequential -> ordre de tour intact) :
	// scheduler.Run(world, dt) exécute donc exactement UN appel à
	// civSystem.Execute(world, dt) par tick, plus DrainEvents()/FlushDeferred()
	// (no-op ici, aucun event/changement structurel émis par ce système) --
	// comportement inchangé au bit près, seul le CHEMIN D'APPEL change.
	ecs::NkScheduler scheduler(1); // 1 thread de jobs : un seul système Sequential, pas de parallélisme à gagner
	civ::NkCivAgentSystem &civSystem = scheduler.AddSystem<civ::NkCivAgentSystem>(&grid);

	NkVector<uint32> startsVec;
	for (uint32 i = 0; i < kNumAgents; ++i)
		startsVec.PushBack(kStarts[i]);

	civ::NkCivRecorder recorder;
	recorder.BeginSession(grid, startsVec); // AVANT le premier tick : grille encore intacte
	civSystem.SetRecorder(&recorder);

	NkVector<ecs::NkEntityId> entities;
	for (uint32 i = 0; i < kNumAgents; ++i) {
		ecs::NkEntityId e = world.CreateEntity();
		civ::NkCivPosition pos;
		pos.state = kStarts[i];
		world.Add<civ::NkCivPosition>(e, pos);
		civ::NkCivAgentRef ac;
		ac.agent = &agents[i];
		ac.turnOrder = i;
		world.Add<civ::NkCivAgentRef>(e, ac);
		entities.PushBack(e);
		logger.Info("  entite ECS {0} : index={1} gen={2}, agent {3} (depart={4})", i, e.index, e.gen, i, kStarts[i]);
	}

	// Snapshot de l'état VIVANT à chaque tick (indépendant du journal) : sert
	// de référence pour prouver que le rejeu reproduit EXACTEMENT ce qui
	// s'est réellement passé pendant la simulation, pas juste ce qui a été
	// écrit puis relu (double preuve : vivant<->journal ET journal<->disque).
	NkVector<NkVector<uint32>> liveStatesPerTick;
	NkVector<NkVector<uint8>> liveDonePerTick;

	const uint32 maxTicks = 40;
	for (uint32 tick = 0; tick < maxTicks; ++tick) {
		scheduler.Run(world, 0.0f); // pilote civSystem.Execute() via NkScheduler (groupe Update)

		NkVector<uint32> stateSnap;
		NkVector<uint8> doneSnap;
		for (uint32 i = 0; i < kNumAgents; ++i) {
			const civ::NkCivAgentRef &ac = world.GetRef<civ::NkCivAgentRef>(entities[i]);
			stateSnap.PushBack(world.GetRef<civ::NkCivPosition>(entities[i]).state);
			doneSnap.PushBack(ac.done ? 1 : 0);
		}
		liveStatesPerTick.PushBack(stateSnap);
		liveDonePerTick.PushBack(doneSnap);

		if (tick % 2 == 0 || !civSystem.AnyActiveLastTick()) {
			logger.Info("  tick {0} (bloques cumules={1}, ressources consommees={2}/{3}) :", tick,
						civSystem.TotalBlockedEvents(), civSystem.TotalResourcesConsumed(), (uint32)kNumResources);
			for (uint32 y = 0; y < kSize; ++y)
				logger.Info("    {0}", BoardRow(world, entities, grid, y).CStr());
		}
		if (!civSystem.AnyActiveLastTick())
			break;
	}

	// --- Bilan simulation : les mesures d'interaction doivent etre NON-NULLES. --
	uint32 numReached = 0;
	for (uint32 i = 0; i < kNumAgents; ++i) {
		const civ::NkCivAgentRef &ac = world.GetRef<civ::NkCivAgentRef>(entities[i]);
		numReached += ac.reachedGoal ? 1 : 0;
		logger.Info("  agent {0} : {1} -- pas={2}, blocages subis={3}, ressources collectees={4}", i,
					ac.reachedGoal ? "a atteint le but" : "n'a PAS atteint le but", ac.steps, ac.blockedCount,
					ac.resourcesCollected);
	}
	logger.Info("  interactions : {0} collision(s) declenchee(s), {1}/{2} ressources consommees (restantes : {3})",
				civSystem.TotalBlockedEvents(), civSystem.TotalResourcesConsumed(), (uint32)kNumResources,
				(uint32)grid.ResourcesRemaining());

	const bool okGoal = (numReached == kNumAgents);
	const bool okCollision = (civSystem.TotalBlockedEvents() >= 1);
	const bool okResources = (civSystem.TotalResourcesConsumed() >= 1);

	// =========================================================================
	// Phase 3/4 : PERSISTANCE + ROUND-TRIP (sauve .nkciv -> recharge -> compare
	// bit-exact avec l'original ET avec l'état VIVANT de la simulation).
	// =========================================================================
	logger.Info("-- Phase 3/4 : persistance .nkciv ({0} frames enregistrees) -> rechargement -> round-trip --",
				recorder.FrameCount());

	NkVector<uint8> bytesOriginal;
	recorder.Serialize(bytesOriginal);

	const bool saved = recorder.SaveToFile(kJournalPath);
	NKENTSEU_ASSERT_MSG(saved, "NkCivRecorder::SaveToFile a echoue");
	logger.Info("  [ {0} ] journal sauve : {1} ({2} octets)", saved ? "OK" : "KO", kJournalPath,
				(uint32)bytesOriginal.Size());

	civ::NkCivRecorder reloaded;
	const bool loaded = reloaded.LoadFromFile(kJournalPath);
	NKENTSEU_ASSERT_MSG(loaded, "NkCivRecorder::LoadFromFile a echoue");
	logger.Info("  [ {0} ] journal recharge depuis le disque : {1} frames, {2} agents", loaded ? "OK" : "KO",
				reloaded.FrameCount(), reloaded.AgentCount());

	NkVector<uint8> bytesReloaded;
	reloaded.Serialize(bytesReloaded);
	bool bytesMatch = (bytesOriginal.Size() == bytesReloaded.Size());
	if (bytesMatch) {
		for (nk_size i = 0; i < bytesOriginal.Size(); ++i) {
			if (bytesOriginal[i] != bytesReloaded[i]) {
				bytesMatch = false;
				break;
			}
		}
	}
	NKENTSEU_ASSERT_MSG(bytesMatch, "round-trip .nkciv NON bit-exact (octets differents original vs recharge)");
	logger.Info("  [ {0} ] round-trip OCTET PAR OCTET bit-exact ({1} octets compares)", bytesMatch ? "OK" : "KO",
				(uint32)bytesOriginal.Size());

	const bool structEqual = recorder.Equals(reloaded);
	NKENTSEU_ASSERT_MSG(structEqual, "round-trip .nkciv NON structurellement exact (Equals)");
	logger.Info("  [ {0} ] round-trip STRUCTUREL exact (en-tete + {1} frames)", structEqual ? "OK" : "KO",
				reloaded.FrameCount());

	uint32 firstDiff = 0xFFFFFFFFu;
	const bool verifyExact = civ::NkCivReplayer::VerifyExact(recorder, reloaded, &firstDiff);
	NKENTSEU_ASSERT_MSG(verifyExact, "NkCivReplayer::VerifyExact : journal recharge differe de l'original");
	logger.Info("  [ {0} ] NkCivReplayer::VerifyExact(original, recharge) = identique", verifyExact ? "OK" : "KO");

	// =========================================================================
	// Phase 4/4 : REJEU tick par tick (lecture PURE, zero re-simulation) +
	// ANALYSE (heatmap, stats par agent, evenements remarquables).
	// =========================================================================
	logger.Info("-- Phase 4/4 : rejeu tick par tick (lecture pure du journal recharge, PAS de re-simulation) --");

	civ::NkCivReplayer replayer;
	const bool opened = replayer.Open(&reloaded);
	NKENTSEU_ASSERT_MSG(opened, "NkCivReplayer::Open a echoue sur le journal recharge");
	NKENTSEU_ASSERT_MSG(replayer.FrameCount() == (uint32)liveStatesPerTick.Size(),
						"le nombre de frames rejouees ne correspond pas au nombre de ticks vecus");

	bool replayExactVsLive = true;
	uint32 replayedTicks = 0;
	while (replayer.NextTick()) {
		const uint32 tickIdx = (uint32)replayer.Cursor();
		for (uint32 a = 0; a < kNumAgents; ++a) {
			const uint32 replayedState = replayer.AgentState(a);
			const uint32 liveState = liveStatesPerTick[tickIdx][a];
			NKENTSEU_ASSERT_MSG(replayedState == liveState,
								"rejeu NON bit-exact : etat d'agent rejoue != etat vecu au meme tick");
			if (replayedState != liveState)
				replayExactVsLive = false;

			const bool replayedDone = replayer.AgentDone(a);
			const bool liveDoneVal = liveDonePerTick[tickIdx][a] != 0;
			NKENTSEU_ASSERT_MSG(replayedDone == liveDoneVal,
								"rejeu NON bit-exact : flag DONE rejoue != flag DONE vecu au meme tick");
			if (replayedDone != liveDoneVal)
				replayExactVsLive = false;
		}
		++replayedTicks;
	}
	logger.Info("  [ {0} ] rejeu de {1} ticks reproduit EXACTEMENT les {2} etats vecus (position + flag DONE, "
				"{3} agents/tick)",
				replayExactVsLive ? "OK" : "KO", replayedTicks, replayedTicks, kNumAgents);

	// ── Analyse du journal rechargé (une passe, pas de re-simulation) ────────
	civ::NkCivAnalyzer analyzer;
	const bool analyzed = analyzer.Analyze(reloaded);
	NKENTSEU_ASSERT_MSG(analyzed, "NkCivAnalyzer::Analyze a echoue sur le journal recharge");

	logger.Info("-- Analyse : heatmap d'occupation (0-9=1..10 visites, A-Z=11..36, #>=37, .=jamais, G=but, O=trou) --");
	for (uint32 r = 0; r < analyzer.HeatmapRows().Size(); ++r)
		logger.Info("    {0}", analyzer.HeatmapRows()[r].CStr());

	logger.Info("-- Analyse : statistiques par agent --");
	for (uint32 a = 0; a < analyzer.AgentCount(); ++a) {
		const civ::NkCivAgentStats &st = analyzer.AgentStat(a);
		const char *outcome = st.reachedGoal ? "but atteint" : (st.fell ? "tombe dans un trou" : "non termine");
		logger.Info("  agent {0} : pas={1}, blocages={2} (max consecutifs={3}), deplacements={4}, distance "
					"parcourue={5}, ressources={6}, arrivee au tick={7}, issue={8}",
					a, st.steps, st.blockedCount, st.maxConsecutiveBlocks, st.movedCount, st.distanceTravelled,
					st.resourcesCollected, st.arrivalTick, outcome);
	}

	logger.Info("-- Analyse : mesures globales --");
	logger.Info("  {0} ticks au total, distance totale parcourue par la societe = {1}, contention des ressources "
				"= {2}/{3} consommees",
				analyzer.TotalTicks(), analyzer.TotalDistanceTravelled(), analyzer.TotalResourcesConsumed(),
				analyzer.TotalResourcesAvailable());
	logger.Info("  taux de contention des ressources = {0}%", (uint32)(analyzer.ResourceContentionRate() * 100.0f));

	logger.Info("-- Analyse : evenements remarquables detectes --");
	if (analyzer.Events().Empty()) {
		logger.Info("  (aucun evenement remarquable sur cette session)");
	} else {
		for (uint32 e = 0; e < analyzer.Events().Size(); ++e)
			logger.Info("  [evenement] {0}", analyzer.Events()[e].description.CStr());
	}

	// =========================================================================
	// Jalon 3 (Phase 5) : export CSV -- tableau de bord minimal exploitable
	// hors moteur (tableur/script), à partir des données PUBLIQUES déjà
	// capturées par NkCivRecorder/NkCivAnalyzer (aucune re-simulation,
	// aucune modification de ces deux classes). Portée assumée : pas de
	// rendu temps réel dans le moteur (voir ROADMAP.md, limite honnête).
	// =========================================================================
	logger.Info("-- Export CSV (Jalon 3) : timeline (demographie+ressources par tick) + stats par agent --");
	const char *kTimelineCsvPath = "NKCivilizationTest_timeline.csv";
	const char *kAgentsCsvPath = "NKCivilizationTest_agents.csv";
	const bool exportedTimeline = civ::NkCivExporter::ExportTimelineCsv(reloaded, kTimelineCsvPath);
	const bool exportedAgents = civ::NkCivExporter::ExportAgentsCsv(analyzer, kAgentsCsvPath);
	logger.Info("  [ {0} ] {1} ({2} lignes + en-tete)", exportedTimeline ? "OK" : "KO", kTimelineCsvPath,
				reloaded.FrameCount());
	logger.Info("  [ {0} ] {1} ({2} lignes + en-tete)", exportedAgents ? "OK" : "KO", kAgentsCsvPath,
				analyzer.AgentCount());

	// =========================================================================
	// Bilan final
	// =========================================================================
	logger.Info("[ {0} ] {1}/{2} agents ECS ont atteint le but commun via leur politique apprise", okGoal ? "OK" : "KO",
				numReached, kNumAgents);
	logger.Info("[ {0} ] la regle de collision s'est DECLENCHEE (mesure non-nulle : {1})", okCollision ? "OK" : "KO",
				civSystem.TotalBlockedEvents());
	logger.Info("[ {0} ] la competition de ressources a eu lieu ({1} consommee(s), premier arrive servi)",
				okResources ? "OK" : "KO", civSystem.TotalResourcesConsumed());
	const bool okRecorder = saved && loaded && bytesMatch && structEqual && verifyExact;
	logger.Info("[ {0} ] enregistrement + persistance .nkciv + round-trip bit-exact", okRecorder ? "OK" : "KO");
	const bool okReplay = replayExactVsLive && (replayedTicks == (uint32)liveStatesPerTick.Size());
	logger.Info("[ {0} ] rejeu tick par tick (lecture pure) reproduit exactement les etats enregistres",
				okReplay ? "OK" : "KO");
	logger.Info("[ {0} ] analyse (heatmap + stats par agent + evenements) executee sur le journal", analyzed ? "OK" : "KO");
	const bool okExport = exportedTimeline && exportedAgents;
	logger.Info("[ {0} ] export CSV (timeline + stats par agent) ecrit sur disque", okExport ? "OK" : "KO");

	const bool ok = okGoal && okCollision && okResources && okRecorder && okReplay && analyzed && okExport;
	const int nOk = (okGoal ? 1 : 0) + (okCollision ? 1 : 0) + (okResources ? 1 : 0) + (okRecorder ? 1 : 0) +
					(okReplay ? 1 : 0) + (analyzed ? 1 : 0) + (okExport ? 1 : 0);
	logger.Info("=== Resultat : {0} OK, {1} echec(s) ===", nOk, 7 - nOk);
	return ok ? 0 : 1;
}
