// =============================================================================
// NKCivilizationScaleTest — Phase 5, chantier "passage à l'échelle" (2026-07-26).
//
// NKCivilizationTest/NKCivilizationSocialTest ont prouvé le micro-monde NKECS
// (civ::NkCivAgentSystem piloté par ecs::NkScheduler) sur une poignée d'agents
// (3 à 6). Ce binaire répond spécifiquement à la question "ça tient à quelle
// échelle, et à quel coût CPU RÉEL par tick ?" :
//   - grille 15x15 (225 états) VIDE (sans trous) : le but ici n'est PAS de
//     re-prouver la richesse RL (déjà faite ailleurs), mais d'isoler le coût
//     du TICK ECS (query + tri + collision O(N²) actuel + politique gloutonne)
//     à mesure que N grandit ;
//   - N = 10, 25, 50, 100 agents, départs distincts déterministes (états
//     0..N-1), but commun au coin opposé -> convergence de trafic garantie
//     (stress réel de la règle de collision, pas un cas dégénéré sans
//     interaction) ;
//   - temps par tick mesuré RÉELLEMENT via NKTime/NkChrono (haute précision,
//     zéro STL) autour de CHAQUE appel scheduler.Run() -- pas une estimation ;
//   - export CSV périodique (Jalon 3, civ::NkCivExporter) de la timeline de
//     chaque palier -- même exporteur que NKCivilizationTest, zéro
//     modification du code d'export entre les deux usages.
//
// Honnêteté de portée : Debug build (comme les deux autres apps du module,
// pour rester comparable) -- les chiffres ci-dessous sont donc des temps
// Debug (non optimisés), PAS des temps Release ; le tableau récapitulatif le
// dit explicitement pour que personne ne les lise comme des temps de prod.
// =============================================================================
#include "NKCivilization/NkCivAgentSystem.h"
#include "NKCivilization/NkCivRecorder.h"
#include "NKCivilization/NkCivExport.h"
#include "NKECS/System/NkScheduler.h"
#include "NKTime/NkChrono.h"
#include "NKLogger/NkLog.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/Assert/NkAssert.h"

using namespace nkentseu;
using namespace nkentseu::ai;

namespace {

	const uint32 kGridSize = 15;						  // 225 etats
	const uint32 kGoal = kGridSize * kGridSize - 1;	  // coin bas-droite (224)
	const int kEpisodes = 2000;
	const uint32 kMaxTrainSteps = 50; // >= distance de Manhattan max (28) + marge collision
	const uint32 kMaxTicks = 200; // marge large : a N=100 le trafic converge pres du plafond precedent (100)

	const uint32 kAgentCounts[] = {10, 25, 50, 100};
	const uint32 kNumConfigs = 4;

	struct ScaleResult {
			uint32 numAgents = 0;
			uint32 ticksRun = 0;
			uint32 numReachedGoal = 0;
			uint32 totalBlocked = 0;
			float64 avgTickMs = 0.0;
			float64 minTickMs = 0.0;
			float64 maxTickMs = 0.0;
			float64 trainingMs = 0.0;
			bool exportedCsv = false;
	};

	// Isolée dans SA PROPRE fonction (même raison que RunOneCondition dans
	// NKCivilizationSocialTest) : un monde+scheduler+grille vivants par
	// palier d'agents ne doivent pas s'empiler comme locaux directs de
	// main() -- surtout ici où le plus gros palier va jusqu'à 100 entités.
	ScaleResult RunScaleConfig(uint32 numAgents) {
		NKENTSEU_ASSERT_MSG(numAgents < kGridSize * kGridSize,
							"numAgents doit rester < nombre d'etats de la grille");

		NkVector<uint32> starts;
		starts.Reserve(numAgents);
		for (uint32 i = 0; i < numAgents; ++i)
			starts.PushBack(i); // departs deterministes et distincts (jamais le but, coin oppose)

		NkVector<uint32> holes; // grille ouverte : le chantier mesure le TEMPS DE TICK, pas la richesse RL

		// ── Entrainement (Q-learning tabulaire, independant par agent) ──────
		NkChrono trainTimer;
		NkVector<rl::NkGridWorld> trainWorlds;
		trainWorlds.Reserve(numAgents);
		NkVector<agent::NkAgent> agents;
		agents.Reserve(numAgents);
		for (uint32 i = 0; i < numAgents; ++i)
			trainWorlds.EmplaceBack(kGridSize, starts[i], kGoal, holes, /*stepCost*/ 0.02f);
		for (uint32 i = 0; i < numAgents; ++i) {
			agent::NkAgentConfig config;
			config.memoryCapacity = 64;
			config.alpha = 0.1f;
			config.gamma = 0.99f;
			config.epsilon = 1.0f;
			config.seed = 1000u + i;
			agents.EmplaceBack(trainWorlds[i], config);
		}
		for (uint32 i = 0; i < numAgents; ++i) {
			for (int e = 1; e <= kEpisodes; ++e) {
				float eps = 1.0f - (float)e / (float)(kEpisodes * 0.8);
				if (eps < 0.05f)
					eps = 0.05f;
				agents[i].Policy().SetEpsilon(eps);
				agent::RunAgentEpisode(trainWorlds[i], agents[i], /*learn*/ true, kMaxTrainSteps);
			}
		}
		const float64 trainingMs = trainTimer.Elapsed().ToMilliseconds();

		// ── Monde partage : entites ECS + systeme pilote par NkScheduler ────
		civ::NkCivGridState grid(kGridSize, kGoal, holes);
		ecs::NkWorld world;
		ecs::NkScheduler scheduler(1);
		civ::NkCivAgentSystem &system = scheduler.AddSystem<civ::NkCivAgentSystem>(&grid);

		civ::NkCivRecorder recorder;
		recorder.BeginSession(grid, starts);
		system.SetRecorder(&recorder);

		NkVector<ecs::NkEntityId> entities;
		entities.Reserve(numAgents);
		for (uint32 i = 0; i < numAgents; ++i) {
			ecs::NkEntityId e = world.CreateEntity();
			civ::NkCivPosition pos;
			pos.state = starts[i];
			world.Add<civ::NkCivPosition>(e, pos);
			civ::NkCivAgentRef ac;
			ac.agent = &agents[i];
			ac.turnOrder = i;
			world.Add<civ::NkCivAgentRef>(e, ac);
			entities.PushBack(e);
		}

		// ── Simulation : mesure REELLE du temps par tick (NkChrono, NKTime) ──
		NkChrono tickTimer;
		(void)tickTimer.Reset(); // purge le temps ecoule pendant l'entrainement/setup ci-dessus
		NkVector<float64> tickMs;
		tickMs.Reserve(kMaxTicks);
		uint32 ticksRun = 0;
		for (uint32 tick = 0; tick < kMaxTicks; ++tick) {
			scheduler.Run(world, 0.0f); // pilote system.Execute() via le vrai ordonnanceur NKECS
			const float64 elapsedMs = tickTimer.Reset().ToMilliseconds();
			tickMs.PushBack(elapsedMs);
			++ticksRun;
			if (!system.AnyActiveLastTick())
				break;
		}

		ScaleResult out;
		out.numAgents = numAgents;
		out.ticksRun = ticksRun;
		out.totalBlocked = system.TotalBlockedEvents();
		out.trainingMs = trainingMs;
		for (uint32 i = 0; i < numAgents; ++i)
			out.numReachedGoal += world.GetRef<civ::NkCivAgentRef>(entities[i]).reachedGoal ? 1 : 0;

		float64 sum = 0.0;
		float64 mn = tickMs.Empty() ? 0.0 : tickMs[0];
		float64 mx = 0.0;
		for (nk_size i = 0; i < tickMs.Size(); ++i) {
			sum += tickMs[i];
			if (tickMs[i] < mn)
				mn = tickMs[i];
			if (tickMs[i] > mx)
				mx = tickMs[i];
		}
		out.avgTickMs = tickMs.Empty() ? 0.0 : sum / (float64)tickMs.Size();
		out.minTickMs = mn;
		out.maxTickMs = mx;

		// ── Jalon 3 -- export CSV periodique (une timeline par palier) ──────
		const NkString path = NkString::Format("NKCivilizationScaleTest_%uagents_timeline.csv", numAgents);
		out.exportedCsv = civ::NkCivExporter::ExportTimelineCsv(recorder, path.CStr());

		logger.Info("  N={0} agents : entrainement={1:.1f}ms, {2} ticks executes, but={3}/{0}, blocages "
					"cumules={4}, tick moyen={5:.4f}ms (min={6:.4f}ms, max={7:.4f}ms), export CSV=[{8}]",
					numAgents, trainingMs, ticksRun, out.numReachedGoal, out.totalBlocked, out.avgTickMs,
					out.minTickMs, out.maxTickMs, out.exportedCsv ? "OK" : "KO");

		return out;
	}

} // namespace

int main() {
	logger.Info("=== NKCivilizationScaleTest : passage a l'echelle -- mesure REELLE du temps par tick "
				"(NkChrono/NKTime) sur une grille {0}x{0} vide, tick pilote par ecs::NkScheduler ===",
				kGridSize);
	logger.Info("-- Honnetete de portee : grille SANS trous (le but ici est le temps de tick a l'echelle, pas la "
				"richesse d'environnement RL deja prouvee par NKCivilizationTest/NKCivilizationSocialTest) --");
	logger.Info("-- Honnetete de portee : build Debug (non optimise), comme les deux autres apps du module -- ces "
				"temps ne sont PAS des temps de production Release --");

	ScaleResult results[kNumConfigs];
	for (uint32 c = 0; c < kNumConfigs; ++c)
		results[c] = RunScaleConfig(kAgentCounts[c]);

	logger.Info("=== Tableau recapitulatif (temps de tick REEL mesure, Debug build) ===");
	for (uint32 c = 0; c < kNumConfigs; ++c) {
		const ScaleResult &r = results[c];
		logger.Info("  {0} agents -> {1} ticks pour terminer, but={2}/{0}, tick moyen={3:.4f}ms, tick max={4:.4f}ms",
					r.numAgents, r.ticksRun, r.numReachedGoal, r.avgTickMs, r.maxTickMs);
	}
	if (kNumConfigs >= 2) {
		const ScaleResult &smallest = results[0];
		const ScaleResult &largest = results[kNumConfigs - 1];
		const float64 ratioAgents = (float64)largest.numAgents / (float64)smallest.numAgents;
		const float64 ratioTickMs =
			(smallest.avgTickMs > 0.0) ? (largest.avgTickMs / smallest.avgTickMs) : 0.0;
		logger.Info("  -- passage de {0} a {1} agents ({2:.1f}x) : le tick moyen est passe de {3:.4f}ms a "
					"{4:.4f}ms ({5:.1f}x) --",
					smallest.numAgents, largest.numAgents, ratioAgents, smallest.avgTickMs, largest.avgTickMs,
					ratioTickMs);
	}

	bool allReachedAll = true;
	bool allExported = true;
	for (uint32 c = 0; c < kNumConfigs; ++c) {
		if (results[c].numReachedGoal != results[c].numAgents)
			allReachedAll = false;
		if (!results[c].exportedCsv)
			allExported = false;
	}
	logger.Info("[ {0} ] tous les agents ont atteint le but, a toutes les echelles testees", allReachedAll ? "OK" : "KO");
	logger.Info("[ {0} ] export CSV periodique ecrit pour chaque palier d'agents", allExported ? "OK" : "KO");

	return (allReachedAll && allExported) ? 0 : 1;
}
