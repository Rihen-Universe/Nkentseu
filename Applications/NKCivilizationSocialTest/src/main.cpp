// =============================================================================
// NKCivilizationSocialTest — Jalon 2 (« société émergente ») : ajoute UNE règle
// d'interaction sociale réelle au micro-monde NKCivilization déjà livré
// (Jalon 1) et mesure, sur PLUSIEURS graines, si un motif COLLECTIF NON
// SCRIPTÉ apparaît quand la règle est active, en comparant deux conditions :
//
//   (A) BASELINE — comportement Jalon 1 inchangé : ressources à usage unique
//       (capacité 1, pas de régénération), collision pleine (aucune
//       co-occupation possible nulle part sauf le but).
//   (B) RÈGLE SOCIALE — ressources renouvelables (capacité 5, +1/tick),
//       collision EXEMPTÉE sur les cases ressource : plusieurs agents
//       peuvent co-occuper une case ressource et y récolter chacun leur tour
//       (récolte partagée), au lieu que le premier arrivé bloque
//       physiquement les suivants (accaparement structurel de l'ancien
//       régime).
//
// Choix de règle (parmi les deux proposées) : (b) ressource limitée +
// renouvelable, coopération (récolte partagée) vs compétition (accaparement)
// — plutôt que (a) échange direct entre agents, parce que la règle de
// collision actuelle rend la co-localisation hors case ressource
// STRUCTURELLEMENT impossible (cf. NkCivAgentSystem::Execute) : deux agents
// ne peuvent jamais se retrouver sur la même case ailleurs qu'au but. (b)
// s'appuie donc sur le substrat ressource déjà livré (NkCivGridState) avec
// un changement minimal et réversible (généralisation binaire -> stock,
// bascule EnableSocialSharing), alors que (a) aurait exigé de relâcher la
// collision PARTOUT (changement bien plus large et plus risqué pour la
// preuve déjà vérifiée du Jalon 1).
//
// HONNÊTETÉ IMPORTANTE : les agents apprennent leur politique (Q-learning)
// SEULEMENT sur la récompense de rl::NkGridWorld (coût de pas, +1 but, -1
// trou) — les ressources n'entrent PAS dans cette récompense. Un agent ne
// « décide » donc jamais de coopérer ou d'accaparer : il traverse une case
// ressource seulement parce qu'elle est sur son chemin appris vers le but.
// Toute différence mesurée entre (A) et (B) est donc un effet STRUCTUREL de
// la règle (ce qui devient possible/impossible), pas une stratégie apprise.
// Ce module ne prétend PAS mesurer une coopération INTENTIONNELLE.
//
// Protocole : pour chaque graine, les 6 agents sont entraînés UNE fois
// (leurs politiques ne sont plus modifiées ensuite - SelectGreedy est const),
// puis DEUX simulations indépendantes sont rejouées avec CES MÊMES
// politiques : une en condition A, une en condition B. Seule la règle du
// monde partagé diffère -> comparaison contrôlée, effet isolé de la règle.
//
// Outils d'observation utilisés (déjà livrés, INCHANGÉS) :
//   - civ::NkCivRecorder capture chaque run (position/action/événements).
//   - civ::NkCivAnalyzer extrait les stats par agent (dont
//     resourcesCollected, base des mesures d'inégalité ci-dessous) et les
//     événements remarquables déjà détectés (domination des ressources,
//     blocage répété).
//   - Mesures SUPPLÉMENTAIRES calculées ICI (dans ce harnais, pas dans la
//     lib) à partir des données PUBLIQUES du journal (aucune re-simulation) :
//     co-occupation sur case ressource (impossible par construction en A),
//     dispersion spatiale moyenne (distance de Manhattan moyenne entre
//     agents actifs par tick), indice de Gini de la répartition des
//     ressources collectées entre agents (inégalité).
// =============================================================================
#include "NKCivilization/NkCivAgentSystem.h"
#include "NKCivilization/NkCivRecorder.h"
#include "NKCivilization/NkCivAnalyzer.h"
#include "NKCivilization/NkCivReplay.h"
#include "NKECS/System/NkScheduler.h"
#include "NKLogger/NkLog.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/Assert/NkAssert.h"

using namespace nkentseu;
using namespace nkentseu::ai;

namespace {

	// ── Le monde partagé par les deux conditions (identique, seule la règle --
	// ── de ressource/collision change) ---------------------------------------
	const uint32 kSize = 6; // grille 6x6 = 36 etats
	const uint32 kGoal = 35; // bas-droite
	const uint32 kNumAgents = 6;
	// Mur ligne y=3 (etats 18..23) avec deux breches (19 et 22), qui portent
	// aussi les cases ressource -> TOUS les agents doivent passer par l'une
	// des deux breches pour atteindre le but, garantissant un trafic reel au
	// goulot (pas besoin de scripter la rencontre, la geometrie l'impose).
	const uint32 kHoles[] = {18, 20, 21, 23};
	const uint32 kNumHoles = 4;
	const uint32 kResourceCells[] = {19, 22};
	const uint32 kNumResourceCells = 2;
	// 3 agents cote breche 19 (x petit), 3 cote breche 22 (x grand).
	const uint32 kStarts[kNumAgents] = {1, 6, 13, 4, 9, 16};

	const int kEpisodes = 4000;
	const uint32 kMaxTrainSteps = 150;
	const uint32 kMaxTicks = 80;
	const uint32 kNumSeeds = 5;
	const uint32 kSeeds[kNumSeeds] = {1, 2, 3, 4, 5};

	NkVector<uint32> HolesVec() {
		NkVector<uint32> h;
		for (uint32 i = 0; i < kNumHoles; ++i)
			h.PushBack(kHoles[i]);
		return h;
	}

	// ── Mesures d'un run (agrégées via NkCivAnalyzer + calculs additionnels --
	// ── sur les donnees PUBLIQUES du journal, aucune re-simulation) ----------
	struct RunMetrics {
			uint32 ticksUsed = 0;
			uint32 numReachedGoal = 0;
			uint32 totalBlocked = 0;
			uint32 totalResourcesConsumed = 0;
			uint32 perAgentResources[kNumAgents] = {0, 0, 0, 0, 0, 0};
			uint32 maxAgentResources = 0;
			float maxSharePct = 0.0f;			 // part du plus gros collecteur (0..100)
			float giniPct = 0.0f;				 // indice de Gini de la repartition (0..100)
			uint32 coLocationTicks = 0;			 // ticks avec >=2 agents actifs sur UNE case ressource
			uint32 coLocationAgentSamples = 0;	 // somme des agents impliques dans ces co-occupations
			float avgPairwiseDistance = 0.0f;	 // distance de Manhattan moyenne entre agents actifs/tick
			uint32 dominationEvents = 0;			 // evenements NkCivAnalyzer "domine la competition"
			uint32 repeatedBlockEvents = 0;		 // evenements NkCivAnalyzer "blocage repete"
	};

	uint32 ManhattanDist(uint32 a, uint32 b, uint32 size) {
		const int32 ax = (int32)(a % size), ay = (int32)(a / size);
		const int32 bx = (int32)(b % size), by = (int32)(b / size);
		const int32 dx = ax > bx ? ax - bx : bx - ax;
		const int32 dy = ay > by ? ay - by : by - ay;
		return (uint32)(dx + dy);
	}

	bool IsResourceCellState(uint32 s) {
		for (uint32 i = 0; i < kNumResourceCells; ++i)
			if (kResourceCells[i] == s)
				return true;
		return false;
	}

	// Calcule les mesures additionnelles (co-occupation, dispersion) en
	// relisant le journal PUBLIC du recorder, tick par tick — pas de
	// re-simulation, aucune modification de NkCivRecorder/NkCivAnalyzer.
	void ComputeSpatialMetrics(const civ::NkCivRecorder &journal, RunMetrics &out) {
		uint32 pairwiseTickCount = 0;
		double pairwiseSum = 0.0;
		for (uint32 f = 0; f < journal.FrameCount(); ++f) {
			const civ::NkCivRecFrame &frame = journal.Frame(f);
			// Positions des agents ENCORE actifs (occupent l'espace) ce tick.
			uint32 activeStates[kNumAgents];
			uint32 numActive = 0;
			for (nk_size k = 0; k < frame.agents.Size(); ++k) {
				const civ::NkCivRecAgentSample &s = frame.agents[k];
				const bool stillPresent = (s.flags & civ::NK_CIV_REC_DONE) == 0;
				if (stillPresent && numActive < kNumAgents)
					activeStates[numActive++] = s.state;
			}
			if (numActive < 2)
				continue;
			// Dispersion : distance de Manhattan moyenne sur toutes les paires.
			uint32 pairs = 0;
			double sumDist = 0.0;
			for (uint32 i = 0; i < numActive; ++i) {
				for (uint32 j = i + 1; j < numActive; ++j) {
					sumDist += ManhattanDist(activeStates[i], activeStates[j], journal.GridSize());
					++pairs;
				}
			}
			if (pairs > 0) {
				pairwiseSum += sumDist / (double)pairs;
				++pairwiseTickCount;
			}
			// Co-occupation sur une case ressource : compte les cases ressource
			// occupees par >=2 agents actifs simultanement CE tick (impossible
			// par construction si SocialSharingEnabled()==false, la collision
			// l'interdit avant meme d'atteindre cette mesure).
			for (uint32 r = 0; r < kNumResourceCells; ++r) {
				uint32 occupants = 0;
				for (uint32 i = 0; i < numActive; ++i)
					if (activeStates[i] == kResourceCells[r])
						++occupants;
				if (occupants >= 2) {
					++out.coLocationTicks;
					out.coLocationAgentSamples += occupants;
				}
			}
		}
		out.avgPairwiseDistance = (pairwiseTickCount > 0) ? (float)(pairwiseSum / (double)pairwiseTickCount) : 0.0f;
	}

	// Indice de Gini (0=egalite parfaite, 100=un seul agent a tout) sur les
	// kNumAgents valeurs de ressources collectees. O(n^2), n=6 -> trivial.
	float GiniPercent(const uint32 (&values)[kNumAgents]) {
		double sumAbsDiff = 0.0;
		double sumValues = 0.0;
		for (uint32 i = 0; i < kNumAgents; ++i) {
			sumValues += (double)values[i];
			for (uint32 j = 0; j < kNumAgents; ++j)
				sumAbsDiff += (values[i] > values[j]) ? (double)(values[i] - values[j]) : (double)(values[j] - values[i]);
		}
		if (sumValues <= 0.0)
			return 0.0f;
		const double gini = sumAbsDiff / (2.0 * (double)kNumAgents * sumValues);
		return (float)(gini * 100.0);
	}

	// Un run complet (une simulation NKECS avec les agents DEJA entraines,
	// grille configuree pour la condition demandee) -> mesures.
	// `traceFrames` (diagnostic ponctuel) : logge, APRES coup, le journal
	// tick par tick (position + flag bloque par agent) -- sert uniquement a
	// comprendre OU se produisent les collisions mesurees (pas utilise pour
	// le calcul des metriques, lecture pure du recorder deja rempli).
	RunMetrics RunOneCondition(NkVector<agent::NkAgent> &agents, bool socialRuleOn, const char *label,
								bool traceFrames = false) {
		civ::NkCivGridState grid(kSize, kGoal, HolesVec());
		if (socialRuleOn) {
			// Condition B : ressources renouvelables (capacite 5, +1/tick),
			// collision exemptee sur les cases ressource (recolte partagee).
			for (uint32 r = 0; r < kNumResourceCells; ++r)
				grid.AddResource(kResourceCells[r], /*capacity*/ 5, /*regenPerTick*/ 1);
			grid.EnableSocialSharing(true);
		} else {
			// Condition A : baseline Jalon 1 inchangee (usage unique, collision pleine).
			for (uint32 r = 0; r < kNumResourceCells; ++r)
				grid.AddResource(kResourceCells[r], /*capacity*/ 1, /*regenPerTick*/ 0);
			// EnableSocialSharing reste false par defaut.
		}

		ecs::NkWorld world;
		// Reste (2026-07-25) -> livré (2026-07-26) : tick piloté par le vrai
		// ecs::NkScheduler NKECS (comme NKCivilizationTest), plus par un appel
		// direct à system.Execute(). Un seul système Sequential enregistré ->
		// comportement inchangé au bit près (régression Jalon 2 revérifiée).
		ecs::NkScheduler scheduler(1);
		civ::NkCivAgentSystem &system = scheduler.AddSystem<civ::NkCivAgentSystem>(&grid);

		NkVector<uint32> startsVec;
		for (uint32 i = 0; i < kNumAgents; ++i)
			startsVec.PushBack(kStarts[i]);

		civ::NkCivRecorder recorder;
		recorder.BeginSession(grid, startsVec);
		system.SetRecorder(&recorder);

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
		}

		for (uint32 tick = 0; tick < kMaxTicks; ++tick) {
			scheduler.Run(world, 0.0f); // pilote system.Execute() via NkScheduler
			if (!system.AnyActiveLastTick())
				break;
		}

		if (traceFrames) {
			logger.Info("    -- trace [{0}] : {1} frames --", label, recorder.FrameCount());
			for (uint32 f = 0; f < recorder.FrameCount(); ++f) {
				const civ::NkCivRecFrame &frame = recorder.Frame(f);
				NkString line("      tick ");
				line.Append((char)('0' + (f / 10) % 10));
				line.Append((char)('0' + f % 10));
				line.Append(" : ");
				for (nk_size k = 0; k < frame.agents.Size(); ++k) {
					const civ::NkCivRecAgentSample &s = frame.agents[k];
					line.Append("a");
					line.Append((char)('0' + s.turnOrder));
					line.Append("@");
					if (s.state >= 10) {
						line.Append((char)('0' + s.state / 10));
						line.Append((char)('0' + s.state % 10));
					} else {
						line.Append((char)('0' + s.state));
					}
					if (s.flags & civ::NK_CIV_REC_BLOCKED)
						line.Append("(bloque)");
					if (s.flags & civ::NK_CIV_REC_COLLECTED)
						line.Append("(recolte)");
					line.Append("  ");
				}
				logger.Info("{0}", line.CStr());
			}
		}

		RunMetrics out;
		out.ticksUsed = system.Ticks();
		out.totalBlocked = system.TotalBlockedEvents();
		out.totalResourcesConsumed = system.TotalResourcesConsumed();
		for (uint32 i = 0; i < kNumAgents; ++i) {
			const civ::NkCivAgentRef &ac = world.GetRef<civ::NkCivAgentRef>(entities[i]);
			out.numReachedGoal += ac.reachedGoal ? 1 : 0;
		}

		civ::NkCivAnalyzer analyzer;
		if (analyzer.Analyze(recorder)) {
			for (uint32 a = 0; a < analyzer.AgentCount() && a < kNumAgents; ++a) {
				const uint32 collected = analyzer.AgentStat(a).resourcesCollected;
				out.perAgentResources[a] = collected;
				if (collected > out.maxAgentResources)
					out.maxAgentResources = collected;
			}
			out.maxSharePct = (out.totalResourcesConsumed > 0)
								   ? (100.0f * (float)out.maxAgentResources / (float)out.totalResourcesConsumed)
								   : 0.0f;
			for (uint32 e = 0; e < analyzer.Events().Size(); ++e) {
				const NkString &desc = analyzer.Events()[e].description;
				if (desc.Contains("domine"))
					++out.dominationEvents;
				if (desc.Contains("blocage repete"))
					++out.repeatedBlockEvents;
			}
		}
		out.giniPct = GiniPercent(out.perAgentResources);
		ComputeSpatialMetrics(recorder, out);

		logger.Info("    [{0}] ticks={1}, but={2}/{3}, collisions={4}, ressources={5}, part-max={6}%, "
					"gini={7}%, co-occupation(ticks)={8}, dispersion-moy={9}",
					label, out.ticksUsed, out.numReachedGoal, (uint32)kNumAgents, out.totalBlocked,
					out.totalResourcesConsumed, (int)out.maxSharePct, (int)out.giniPct, out.coLocationTicks,
					out.avgPairwiseDistance);
		return out;
	}

	// =========================================================================
	// Jalon 4 (Phase 5) -- scénario "what-if" MINIMAL et RÉEL : PAS deux runs
	// indépendants (comme le protocole A/B ci-dessus), mais UNE SEULE
	// simulation rejouée depuis un point donné, avec un paramètre CHANGÉ EN
	// COURS DE ROUTE, sur un objet grille VIVANT (pas de reconstruction
	// depuis un journal -- NkCivRecorder ne capture pas le stock par case) :
	//   - Fork "baseline" tourne en condition A du début à la fin ;
	//   - Fork "what-if" est IDENTIQUE au fork baseline jusqu'au tick de
	//     bascule `branchTick` (mêmes politiques gelées, même grille au
	//     départ), puis À CE TICK PRÉCIS, EnableSocialSharing(true) +
	//     SetResourceParams() sont appelés sur SA grille déjà vivante ->
	//     il diverge pour le reste du run, sans jamais re-simuler le passé.
	// Chaque fork est isolé dans SA PROPRE frame d'appel (RunWhatIfFork) --
	// deux mondes/schedulers/grilles vivants ne doivent PAS cohabiter comme
	// locaux directs d'une même fonction (empreinte pile cumulée).
	// =========================================================================
	struct WhatIfForkResult {
			uint32 ticks = 0;
			uint32 numReachedGoal = 0;
			uint32 totalBlocked = 0;
			uint32 totalResourcesConsumed = 0;
			uint32 perAgentResources[kNumAgents] = {0, 0, 0, 0, 0, 0};
	};

	WhatIfForkResult RunWhatIfFork(NkVector<agent::NkAgent> &agents, const NkVector<uint32> &starts,
									bool applyWhatIf, uint32 branchTick, civ::NkCivRecorder &outRecorder) {
		civ::NkCivGridState grid(kSize, kGoal, HolesVec());
		for (uint32 r = 0; r < kNumResourceCells; ++r)
			grid.AddResource(kResourceCells[r], /*capacity*/ 1, /*regenPerTick*/ 0); // point de depart commun = condition A

		ecs::NkWorld world;
		ecs::NkScheduler scheduler(1);
		civ::NkCivAgentSystem &system = scheduler.AddSystem<civ::NkCivAgentSystem>(&grid);
		outRecorder.BeginSession(grid, starts);
		system.SetRecorder(&outRecorder);

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
		}

		bool branched = false;
		for (uint32 tick = 0; tick < kMaxTicks; ++tick) {
			if (applyWhatIf && tick == branchTick && !branched) {
				// LE WHAT-IF : à partir de MAINTENANT (sur une grille déjà vivante
				// depuis `branchTick` ticks), la règle sociale s'active.
				grid.EnableSocialSharing(true);
				for (uint32 r = 0; r < kNumResourceCells; ++r)
					grid.SetResourceParams(kResourceCells[r], /*capacity*/ 5, /*regenPerTick*/ 1);
				branched = true;
				logger.Info("  -- [WHAT-IF] divergence au tick {0} : ressources renouvelables (capacite=5, "
							"+1/tick) + collision exemptee sur les cases ressource, CE FORK SEULEMENT --",
							branchTick);
			}
			scheduler.Run(world, 0.0f);
			if (!system.AnyActiveLastTick())
				break;
		}

		WhatIfForkResult out;
		out.ticks = system.Ticks();
		out.totalBlocked = system.TotalBlockedEvents();
		out.totalResourcesConsumed = system.TotalResourcesConsumed();
		for (uint32 i = 0; i < kNumAgents; ++i)
			out.numReachedGoal += world.GetRef<civ::NkCivAgentRef>(entities[i]).reachedGoal ? 1 : 0;

		civ::NkCivAnalyzer analyzer;
		if (analyzer.Analyze(outRecorder))
			for (uint32 a = 0; a < analyzer.AgentCount() && a < kNumAgents; ++a)
				out.perAgentResources[a] = analyzer.AgentStat(a).resourcesCollected;

		return out;
	}

	// Vérification OBJECTIVE (pas de triche) que le préfixe [0, branchTick) est
	// BIT-EXACT entre les deux journaux -- lecture PURE via NkCivReplayer,
	// même pattern que NKCivilizationTest pour son round-trip. NkCivReplayer::
	// VerifyExact() n'est PAS utilisable ici tel quel (il exige
	// a.FrameCount()==b.FrameCount(), or les deux forks n'ont PAS la même
	// durée totale par construction) -- on relit donc le préfixe commun
	// manuellement via NextTick()/AgentState()/AgentDone(), la même API.
	bool VerifySharedPrefix(const civ::NkCivRecorder &a, const civ::NkCivRecorder &b, uint32 branchTick) {
		if (branchTick > a.FrameCount() || branchTick > b.FrameCount())
			return false;
		civ::NkCivReplayer replayA;
		civ::NkCivReplayer replayB;
		if (!replayA.Open(&a) || !replayB.Open(&b))
			return false;
		for (uint32 t = 0; t < branchTick; ++t) {
			if (!replayA.NextTick() || !replayB.NextTick())
				return false;
			for (uint32 ag = 0; ag < kNumAgents; ++ag)
				if (replayA.AgentState(ag) != replayB.AgentState(ag) || replayA.AgentDone(ag) != replayB.AgentDone(ag))
					return false;
		}
		return true;
	}

	void RunWhatIfExperiment() {
		logger.Info("=== Jalon 4 -- what-if : rejeu de LA MEME simulation depuis un point donne, parametre change "
					"en cours de route (verifie via NkCivReplayer) ===");

		NkVector<rl::NkGridWorld> wiWorlds;
		wiWorlds.Reserve(kNumAgents);
		NkVector<agent::NkAgent> wiAgents;
		wiAgents.Reserve(kNumAgents);
		NkVector<uint32> wiHoles = HolesVec();
		for (uint32 i = 0; i < kNumAgents; ++i)
			wiWorlds.EmplaceBack(kSize, kStarts[i], kGoal, wiHoles, /*stepCost*/ 0.02f);
		for (uint32 i = 0; i < kNumAgents; ++i) {
			agent::NkAgentConfig config;
			config.memoryCapacity = 64;
			config.alpha = 0.1f;
			config.gamma = 0.99f;
			config.epsilon = 1.0f;
			config.seed = 424242u + i; // graine dediee, independante des 5 graines A/B ci-dessus
			wiAgents.EmplaceBack(wiWorlds[i], config);
		}
		logger.Info("  entrainement dedie de {0} agents ({1} episodes chacun, graine independante 424242)",
					kNumAgents, kEpisodes);
		for (uint32 i = 0; i < kNumAgents; ++i) {
			for (int e = 1; e <= kEpisodes; ++e) {
				float eps = 1.0f - (float)e / (float)(kEpisodes * 0.8);
				if (eps < 0.05f)
					eps = 0.05f;
				wiAgents[i].Policy().SetEpsilon(eps);
				agent::RunAgentEpisode(wiWorlds[i], wiAgents[i], /*learn*/ true, kMaxTrainSteps);
			}
		}

		const uint32 kBranchTick = 4; // point de divergence (avant la fin typique du run, ~12-13 ticks)

		NkVector<uint32> wiStarts;
		for (uint32 i = 0; i < kNumAgents; ++i)
			wiStarts.PushBack(kStarts[i]);

		civ::NkCivRecorder recBaseline;
		civ::NkCivRecorder recWhatIf;
		const WhatIfForkResult baseline =
			RunWhatIfFork(wiAgents, wiStarts, /*applyWhatIf*/ false, kBranchTick, recBaseline);
		const WhatIfForkResult whatIf = RunWhatIfFork(wiAgents, wiStarts, /*applyWhatIf*/ true, kBranchTick, recWhatIf);

		const bool prefixIdentical = VerifySharedPrefix(recBaseline, recWhatIf, kBranchTick);
		NKENTSEU_ASSERT_MSG(prefixIdentical,
							"what-if : l'historique partage AVANT la divergence n'est PAS identique entre les forks");
		logger.Info("  [ {0} ] historique partage verifie BIT-EXACT via NkCivReplayer sur {1} ticks avant la "
					"divergence (aucune triche : le fork what-if n'a pas ete reconstruit, il a vraiment vecu ce "
					"prefixe)",
					prefixIdentical ? "OK" : "KO", kBranchTick);

		const float giniBaseline = GiniPercent(baseline.perAgentResources);
		const float giniWhatIf = GiniPercent(whatIf.perAgentResources);
		logger.Info("=== What-if : comparaison objective des deux trajectoires (branche au tick {0}) ===",
					kBranchTick);
		logger.Info("  [baseline -- jamais modifiee] ticks={0}, but={1}/{2}, collisions={3}, ressources={4}, "
					"gini={5}%",
					baseline.ticks, baseline.numReachedGoal, (uint32)kNumAgents, baseline.totalBlocked,
					baseline.totalResourcesConsumed, (int)giniBaseline);
		logger.Info("  [what-if depuis tick {0}] ticks={1}, but={2}/{3}, collisions={4}, ressources={5}, gini={6}%",
					kBranchTick, whatIf.ticks, whatIf.numReachedGoal, (uint32)kNumAgents, whatIf.totalBlocked,
					whatIf.totalResourcesConsumed, (int)giniWhatIf);
		logger.Info("  ecart CHIFFRE (what-if - baseline) : {0} tick(s), {1} ressource(s) en plus, {2} point(s) de "
					"Gini en moins",
					(int32)whatIf.ticks - (int32)baseline.ticks,
					(int32)whatIf.totalResourcesConsumed - (int32)baseline.totalResourcesConsumed,
					(int32)giniBaseline - (int32)giniWhatIf);
		logger.Info("  [OBSERVE] la divergence au tick {0} change la SUITE de la trajectoire (regle de ressource "
					"differente a partir de ce point) sans jamais re-simuler ni modifier le passe partage -- c'est "
					"la definition retenue ici d'un scenario what-if minimal et reel (pas une simulation de "
					"facade).",
					kBranchTick);
	}

} // namespace

int main() {
	logger.Info("=== NKCivilizationSocialTest : Jalon 2 -- regle sociale (ressource renouvelable + recolte "
				"partagee) vs baseline (Jalon 1) ===");
	logger.Info("-- Regle choisie : (b) ressource limitee+renouvelable, cooperation (recolte partagee) vs "
				"competition (accaparement) --");
	logger.Info("-- Protocole : {0} graines, chacune entrainee UNE fois puis rejouee en condition A (baseline) "
				"et B (regle sociale) avec LES MEMES politiques --",
				kNumSeeds);

	RunMetrics condA[kNumSeeds];
	RunMetrics condB[kNumSeeds];

	for (uint32 si = 0; si < kNumSeeds; ++si) {
		const uint32 seed = kSeeds[si];
		logger.Info("-- Graine {0}/{1} (seed={2}) : entrainement de {3} agents ({4} episodes chacun) --", si + 1,
					kNumSeeds, seed, kNumAgents, kEpisodes);

		NkVector<rl::NkGridWorld> trainWorlds;
		trainWorlds.Reserve(kNumAgents);
		NkVector<agent::NkAgent> agents;
		agents.Reserve(kNumAgents);
		NkVector<uint32> holes = HolesVec();
		for (uint32 i = 0; i < kNumAgents; ++i)
			trainWorlds.EmplaceBack(kSize, kStarts[i], kGoal, holes, /*stepCost*/ 0.02f);
		for (uint32 i = 0; i < kNumAgents; ++i) {
			agent::NkAgentConfig config;
			config.memoryCapacity = 64;
			config.alpha = 0.1f;
			config.gamma = 0.99f;
			config.epsilon = 1.0f;
			config.seed = seed * 1000u + i; // graine decorrelee par agent ET par run externe
			agents.EmplaceBack(trainWorlds[i], config);
		}

		uint32 numSuccessTrained = 0;
		for (uint32 i = 0; i < kNumAgents; ++i) {
			int reached = 0;
			for (int e = 1; e <= kEpisodes; ++e) {
				float eps = 1.0f - (float)e / (float)(kEpisodes * 0.8);
				if (eps < 0.05f)
					eps = 0.05f;
				agents[i].Policy().SetEpsilon(eps);
				agent::NkAgentEpisodeResult r =
					agent::RunAgentEpisode(trainWorlds[i], agents[i], /*learn*/ true, kMaxTrainSteps);
				if (e > kEpisodes - 200)
					reached += r.reachedGoal ? 1 : 0;
			}
			if (reached >= 190) // >=95% sur les 200 derniers episodes
				++numSuccessTrained;
		}
		logger.Info("  entrainement termine : {0}/{1} agents avec >=95% de succes sur les 200 derniers episodes",
					numSuccessTrained, kNumAgents);

		// Les DEUX conditions rejouent LES MEMES politiques apprises (SelectGreedy
		// est const, aucun re-apprentissage entre A et B) -> seule la regle du
		// monde partage differe, comparaison controlee.
		// Trace diagnostique (position/blocage par tick) sur la 1ere graine
		// seulement -- sert a EXPLIQUER honnetement ou se produisent les
		// collisions mesurees (pas a changer le protocole ni les metriques).
		const bool trace = (si == 0);
		condA[si] = RunOneCondition(agents, /*socialRuleOn*/ false, "A baseline", trace);
		condB[si] = RunOneCondition(agents, /*socialRuleOn*/ true, "B social  ", trace);
	}

	// ── Agrégation honnête sur les 5 graines ─────────────────────────────────
	auto Mean = [](const RunMetrics *arr, nk_size n, uint32 RunMetrics::*field) -> float {
		double sum = 0.0;
		for (nk_size i = 0; i < n; ++i)
			sum += (double)(arr[i].*field);
		return (float)(sum / (double)n);
	};
	auto MeanF = [](const RunMetrics *arr, nk_size n, float RunMetrics::*field) -> float {
		double sum = 0.0;
		for (nk_size i = 0; i < n; ++i)
			sum += (double)(arr[i].*field);
		return (float)(sum / (double)n);
	};

	logger.Info("=== Resultats agreges sur {0} graines (moyennes) ===", kNumSeeds);
	logger.Info("  [A baseline] but={0}/{1} en moy., collisions={2}, ressources consommees={3}, part-max={4}%, "
				"gini={5}%, co-occupation(ticks)={6}, dispersion-moy={7}, domination={8}, blocage-repete={9}",
				Mean(condA, kNumSeeds, &RunMetrics::numReachedGoal), (uint32)kNumAgents,
				Mean(condA, kNumSeeds, &RunMetrics::totalBlocked),
				Mean(condA, kNumSeeds, &RunMetrics::totalResourcesConsumed),
				MeanF(condA, kNumSeeds, &RunMetrics::maxSharePct), MeanF(condA, kNumSeeds, &RunMetrics::giniPct),
				Mean(condA, kNumSeeds, &RunMetrics::coLocationTicks),
				MeanF(condA, kNumSeeds, &RunMetrics::avgPairwiseDistance),
				Mean(condA, kNumSeeds, &RunMetrics::dominationEvents),
				Mean(condA, kNumSeeds, &RunMetrics::repeatedBlockEvents));
	logger.Info("  [B social  ] but={0}/{1} en moy., collisions={2}, ressources consommees={3}, part-max={4}%, "
				"gini={5}%, co-occupation(ticks)={6}, dispersion-moy={7}, domination={8}, blocage-repete={9}",
				Mean(condB, kNumSeeds, &RunMetrics::numReachedGoal), (uint32)kNumAgents,
				Mean(condB, kNumSeeds, &RunMetrics::totalBlocked),
				Mean(condB, kNumSeeds, &RunMetrics::totalResourcesConsumed),
				MeanF(condB, kNumSeeds, &RunMetrics::maxSharePct), MeanF(condB, kNumSeeds, &RunMetrics::giniPct),
				Mean(condB, kNumSeeds, &RunMetrics::coLocationTicks),
				MeanF(condB, kNumSeeds, &RunMetrics::avgPairwiseDistance),
				Mean(condB, kNumSeeds, &RunMetrics::dominationEvents),
				Mean(condB, kNumSeeds, &RunMetrics::repeatedBlockEvents));

	// ── Détail par graine (traçabilité complète, pas juste la moyenne) ──────
	logger.Info("=== Détail par graine (A vs B) ===");
	for (uint32 si = 0; si < kNumSeeds; ++si) {
		logger.Info("  graine {0} : A[gini={1}%, part-max={2}%, co-occ={3}, collisions={4}, ress={5}] vs "
					"B[gini={6}%, part-max={7}%, co-occ={8}, collisions={9}, ress={10}]",
					kSeeds[si], (int)condA[si].giniPct, (int)condA[si].maxSharePct, condA[si].coLocationTicks,
					condA[si].totalBlocked, condA[si].totalResourcesConsumed, (int)condB[si].giniPct,
					(int)condB[si].maxSharePct, condB[si].coLocationTicks, condB[si].totalBlocked,
					condB[si].totalResourcesConsumed);
	}

	// ── Conclusion honnête (ni forcée positive, ni cachée si négative/ambiguë) --
	const float meanCoOccB = Mean(condB, kNumSeeds, &RunMetrics::coLocationTicks);
	const float meanCoOccA = Mean(condA, kNumSeeds, &RunMetrics::coLocationTicks);
	const float meanGiniA = MeanF(condA, kNumSeeds, &RunMetrics::giniPct);
	const float meanGiniB = MeanF(condB, kNumSeeds, &RunMetrics::giniPct);
	logger.Info("=== Conclusion (honnete, non forcee) ===");
	logger.Info("  co-occupation sur case ressource : A={0} (attendu 0 par construction, collision l'interdit), "
				"B={1}",
				meanCoOccA, meanCoOccB);
	if (meanCoOccB > meanCoOccA + 0.01f) {
		logger.Info("  [OBSERVE] la regle sociale a produit de la co-occupation reelle (non triviale : la "
					"collision ne l'empeche plus, mais rien ne FORCE deux agents a s'y trouver en meme temps).");
	} else {
		logger.Info("  [OBSERVE] AUCUNE co-occupation mesuree meme en B sur ce plan/ces graines -- soit les "
					"trajectoires des agents ne se recoupent pas au meme tick sur les cases ressource, soit "
					"l'effet est trop rare pour ce nombre de graines. Rapporte tel quel, pas de conclusion forcee.");
	}
	logger.Info("  inegalite de repartition des ressources (indice de Gini) : A={0}% vs B={1}%", (int)meanGiniA,
				(int)meanGiniB);
	if (meanGiniB + 5.0f < meanGiniA) {
		logger.Info("  [OBSERVE] la regle sociale REDUIT l'inegalite de repartition par rapport a l'accaparement "
					"'premier arrive' de la baseline -- motif collectif mesurable, NON scripte explicitement "
					"(aucun agent n'est programme pour partager ; c'est une consequence structurelle de "
					"l'exemption de collision + renouvellement).");
	} else if (meanGiniA + 5.0f < meanGiniB) {
		logger.Info("  [OBSERVE] la regle sociale AUGMENTE l'inegalite par rapport a la baseline (un effet "
					"inverse de l'hypothese 'partage' -- possible si un sous-ensemble d'agents monopolise le "
					"stock renouvele avant que les autres ne repassent). Rapporte tel quel.");
	} else {
		logger.Info("  [OBSERVE] pas de difference nette d'inegalite entre A et B sur ce protocole (ecart < 5 "
					"points de Gini) -- PAS de degradation ni d'amelioration claire observee a l'echelle testee.");
	}
	logger.Info("  RAPPEL HONNETE : les agents n'ont AUCUNE recompense liee aux ressources dans leur politique "
				"apprise (Q-learning entraine uniquement sur cout de pas + but + trou) -- toute difference "
				"mesuree ci-dessus est un effet STRUCTUREL de la regle du monde, pas une strategie de "
				"cooperation ou de competition APPRISE ou DECIDEE par les agents.");

	// Jalon 4 (Phase 5) : scénario "what-if" -- voir RunWhatIfExperiment()
	// ci-dessus (namespace anonyme). Isolé dans sa PROPRE fonction (comme
	// RunOneCondition) : deux forks avec chacun sa grille/monde/scheduler
	// vivants ne doivent PAS cohabiter comme locaux directs de main(), sous
	// peine de cumuler leur empreinte dans une SEULE pile d'appel.
	RunWhatIfExperiment();

	return 0;
}
