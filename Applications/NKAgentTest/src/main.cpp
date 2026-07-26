// =============================================================================
// NKAgentTest — un NkAgent (mémoire + perception + politique) apprend SEUL à
// traverser un monde-grille (NKAgent, Phase 4, Jalons 1 et 2).
//
// Test 1 (Jalon 1) : même preuve que NKRLTest (grille 5x5, Q-learning,
// ε-greedy décroissant), mais à travers la couche NkAgent : perception
// (état -> features) + politique (enveloppe rl::NkQLearning) + mémoire
// (transitions vécues), au lieu d'appeler rl::NkQLearning/rl::RunEpisode
// directement.
//
// Test 2 (Jalon 2) : ablation MEMORY REPLAY — à nombre d'épisodes RÉDUIT,
// deux agents identiques (mêmes graines, mêmes hyperparamètres) apprennent
// AVEC et SANS experience replay (rejouer des transitions mémorisées à
// travers NkQLearning::Update). Moyenné sur plusieurs graines : le replay
// doit atteindre un taux de réussite d'évaluation SUPÉRIEUR (accélération de
// l'apprentissage). Vérifie aussi que la pondération d'importance
// (|erreur TD|) agit sur la rétention mémoire.
//
// Test 3 (Jalon 3) : ablation BUTS & PLANIFICATION — un monde CLÉ-PUIS-PORTE
// (rl::NkKeyDoorGridWorld : la porte/but final reste FERMÉE tant que la clé
// n'est pas ramassée) est confié à deux agents identiques (même architecture
// StepWithGoals/PolicyForGoal, mêmes hyperparamètres/graines) qui ne diffèrent
// QUE par le PLAN posé sur leur NkGoalStack : un seul but (la porte, « SANS
// but » = pas de décomposition) contre deux buts empilés (la clé PUIS la
// porte, « AVEC pile de buts »). Sans décomposition, l'agent doit découvrir
// par exploration aléatoire tout le chemin composé (aller chercher la clé
// PUIS la porte) avant de toucher la moindre récompense non nulle — quasi
// impossible à budget réduit ; il fonce vers la porte (le seul point
// "attractif" visible dans son propre horizon) et s'y heurte, bloqué (mesuré
// via NkKeyDoorGridWorld::BlockedDoorHits). Avec la pile, chaque sous-but est
// un problème COURT et bien récompensé, résolu indépendamment.
//
// Test 4 (Jalon 4a) : ablation PERSONNALITÉ — la MÊME politique déjà entraînée
// (`ag` du test 1) est évaluée sous deux profils de traits (NkAgentPersonality
// ::Bold / ::Cautious, via NkAgent::StepWithPersonality) sur le même monde à
// trous : le profil Prudent doit emprunter mesurablement MOINS de cases au
// bord d'un trou et ne jamais tomber plus souvent que le profil Audacieux (qui
// garde une part d'exploration « curieuse » même à l'évaluation). Le trait
// patience (budget de but, Jalon 3) est vérifié séparément, en isolation.
// =============================================================================
#include "NKAgent/NkAgent.h"
#include "NKAgent/NkAgentPersonality.h"
#include "NKContainers/String/NkString.h"
#include "NKLogger/NkLog.h"
#include "NKRL/NkKeyDoorGridWorld.h"

using namespace nkentseu;
using namespace nkentseu::ai;

namespace {

	// Construit la ligne "politique apprise" d'une rangée de la grille (flèche = action
	// gloutonne de la politique de l'agent). Une seule chaîne -> un seul logger.Info par ligne.
	NkString PolicyRow(agent::NkAgent &ag, const rl::NkGridWorld &env, uint32 y) {
		static const char *arrow[4] = {"^", "v", "<", ">"};
		const uint32 N = env.Size();
		NkString row;
		for (uint32 x = 0; x < N; ++x) {
			const uint32 s = y * N + x;
			row += ' ';
			if (s == env.GoalState())
				row += 'G';
			else if (env.IsHole(s))
				row += 'O';
			else
				row += arrow[ag.Policy().SelectGreedy(s)];
		}
		return row;
	}

	// Entraîne un agent neuf pendant `episodes` (schéma d'epsilon décroissant
	// identique au test 1, mis à l'échelle du nombre d'épisodes), puis évalue en
	// gloutonne pure sur `evalN` épisodes. Retourne le taux de réussite [0,1].
	// `replayed`/`memMin`/`memMax` : statistiques de preuve (nb de transitions
	// rejouées, importances extrêmes retenues en mémoire).
	double TrainAndEval(uint32 size, uint32 start, uint32 goal, const NkVector<uint32> &holes, int episodes,
						bool replay, uint32 seed, uint64 &replayed, float &memMin, float &memMax) {
		rl::NkGridWorld env(size, start, goal, holes, /*stepCost*/ 0.02f);

		agent::NkAgentConfig config;
		config.memoryCapacity = 256; // même mémoire dans les deux bras (seul le replay l'exploite)
		config.alpha = 0.1f;
		config.gamma = 0.99f;
		config.epsilon = 1.0f;
		config.seed = seed;
		config.replayEnabled = replay;
		config.replayBatchSize = 8;
		config.replayInterval = 1;
		agent::NkAgent ag(env, config);

		const uint32 maxSteps = 100;
		for (int e = 1; e <= episodes; ++e) {
			float eps = 1.0f - (float)e / (float)(episodes * 0.8);
			if (eps < 0.05f)
				eps = 0.05f;
			ag.Policy().SetEpsilon(eps);
			agent::RunAgentEpisode(env, ag, /*learn*/ true, maxSteps);
		}

		const int evalN = 200;
		int reached = 0;
		for (int e = 0; e < evalN; ++e) {
			agent::NkAgentEpisodeResult r = agent::RunAgentEpisode(env, ag, /*learn*/ false, maxSteps);
			reached += r.reachedGoal ? 1 : 0;
		}
		replayed = ag.TotalReplayed();
		memMin = ag.Memory().MinImportance();
		memMax = ag.Memory().MaxImportance();
		return (double)reached / (double)evalN;
	}

	// Entraîne un agent (constructeur générique Jalon 3) sur le monde clé-puis-porte
	// pendant `episodes`, en poursuivant à CHAQUE épisode le plan posé sur une
	// NkGoalStack neuve : `useSubgoal=false` -> un seul but (la porte, pas de
	// décomposition) ; `useSubgoal=true` -> deux buts empilés (la clé PUIS la
	// porte). Puis évalue en gloutonne pure sur `evalEpisodes`. Retourne le taux
	// de réussite [0,1] ; `totalBlockedDoorHits` = nb cumulé de tentatives
	// d'entrée sur la porte SANS la clé pendant l'évaluation (preuve du "il fonce
	// à la porte fermée").
	double TrainAndEvalKeyDoor(uint32 size, uint32 start, uint32 key, uint32 door, const NkVector<uint32> &holes,
								int episodes, bool useSubgoal, uint32 seed, uint32 maxSteps, int evalEpisodes,
								uint64 &totalBlockedDoorHits, uint64 &totalBlockedDuringTraining) {
		rl::NkKeyDoorGridWorld env(size, start, key, door, holes, /*stepCost*/ 0.02f);

		agent::NkAgentConfig config;
		config.memoryCapacity = 256;
		config.alpha = 0.1f;
		config.gamma = 0.99f;
		config.epsilon = 1.0f;
		config.seed = seed;

		const uint32 doorX = door % size, doorY = door / size;
		agent::NkAgent ag(env.NumStates(), env.NumActions(), size, doorX, doorY, config);

		const uint32 doorGoalTarget = env.DoorOpenedState();
		const uint32 keyGoalTarget = env.KeyTakenState();

		totalBlockedDuringTraining = 0;
		for (int e = 1; e <= episodes; ++e) {
			float eps = 1.0f - (float)e / (float)(episodes * 0.8);
			if (eps < 0.05f)
				eps = 0.05f;
			ag.PolicyForGoal(doorGoalTarget).SetEpsilon(eps);
			if (useSubgoal)
				ag.PolicyForGoal(keyGoalTarget).SetEpsilon(eps);

			agent::NkGoalStack goals;
			agent::NkAgentGoal doorGoal;
			doorGoal.targetState = doorGoalTarget;
			doorGoal.priority = 1.0f;
			goals.Push(doorGoal); // toujours au fond du plan : le but final
			if (useSubgoal) {
				agent::NkAgentGoal keyGoal;
				keyGoal.targetState = keyGoalTarget;
				keyGoal.priority = 2.0f; // sous-but : plus urgent, poursuivi en premier
				goals.Push(keyGoal);	  // sommet de pile = but ACTIF courant
			}
			agent::RunAgentEpisodeWithGoals(env, ag, /*learn*/ true, maxSteps, goals);
			totalBlockedDuringTraining += env.BlockedDoorHits();
		}

		int reached = 0;
		totalBlockedDoorHits = 0;
		for (int e = 0; e < evalEpisodes; ++e) {
			agent::NkGoalStack goals;
			agent::NkAgentGoal doorGoal;
			doorGoal.targetState = doorGoalTarget;
			goals.Push(doorGoal);
			if (useSubgoal) {
				agent::NkAgentGoal keyGoal;
				keyGoal.targetState = keyGoalTarget;
				goals.Push(keyGoal);
			}
			agent::NkAgentGoalEpisodeResult r = agent::RunAgentEpisodeWithGoals(env, ag, /*learn*/ false, maxSteps, goals);
			reached += r.reachedGoal ? 1 : 0;
			totalBlockedDoorHits += env.BlockedDoorHits();
		}
		return (double)reached / (double)evalEpisodes;
	}

} // namespace

int main() {
	logger.Info("=== NKAgentTest : un NkAgent (memoire+perception+politique) traverse un monde-grille ===");

	// Grille 5x5, depart en haut-gauche (0), but en bas-droite (24), 3 trous. Identique a NKRLTest.
	const uint32 N = 5, START = 0, GOAL = 24;
	NkVector<uint32> holes;
	holes.PushBack(6);
	holes.PushBack(12);
	holes.PushBack(18);
	rl::NkGridWorld env(N, START, GOAL, holes, /*stepCost*/ 0.02f);

	agent::NkAgentConfig config;
	config.memoryCapacity = 64; // le "passe" : ne garde que les dernieres transitions
	config.alpha = 0.1f;
	config.gamma = 0.99f;
	config.epsilon = 1.0f;
	config.seed = 42u;
	agent::NkAgent ag(env, config);

	const int episodes = 4000;
	const uint32 maxSteps = 100;

	logger.Info("-- Entrainement ({0} episodes, epsilon decroissant, via NkAgent) --", episodes);
	int winReach = 0;
	double winReward = 0.0;
	for (int e = 1; e <= episodes; ++e) {
		// epsilon : 1.0 -> 0.05 sur les 80 premiers % des episodes (puis exploite).
		float eps = 1.0f - (float)e / (float)(episodes * 0.8);
		if (eps < 0.05f)
			eps = 0.05f;
		ag.Policy().SetEpsilon(eps);

		agent::NkAgentEpisodeResult r = agent::RunAgentEpisode(env, ag, /*learn*/ true, maxSteps);
		winReach += r.reachedGoal ? 1 : 0;
		winReward += r.reward;

		if (e % 800 == 0) {
			logger.Info("  episode {0} : succes(800 derniers) = {1}%  recompense moy = {2}  eps={3}", e,
						(double)winReach / 8.0, winReward / 800.0, (double)eps);
			winReach = 0;
			winReward = 0.0;
		}
	}

	// Evaluation : politique gloutonne pure (aucune exploration, aucun apprentissage).
	const int evalN = 200;
	int reached = 0;
	for (int e = 0; e < evalN; ++e) {
		agent::NkAgentEpisodeResult r = agent::RunAgentEpisode(env, ag, /*learn*/ false, maxSteps);
		reached += r.reachedGoal ? 1 : 0;
	}
	const double successRate = (double)reached / (double)evalN;
	logger.Info("  evaluation gloutonne (via NkAgent) : {0}/{1} episodes atteignent le but ({2}%)", reached, evalN,
				successRate * 100.0);

	// Preuve que la MEMOIRE (le passe) a bien enregistre le vecu de l'agent.
	logger.Info("-- Memoire de l'agent : {0}/{1} transitions retenues (buffer borne) --", ag.Memory().Size(),
				ag.Memory().Capacity());
	const nk_size lastN = ag.Memory().Size() < 3 ? ag.Memory().Size() : 3;
	for (nk_size i = 0; i < lastN; ++i) {
		const agent::NkAgentTransition &t = ag.Memory().At(ag.Memory().Size() - lastN + i);
		logger.Info("  souvenir : etat={0} features=[{1},{2},{3},{4}] action={5} recompense={6} etat_suiv={7} "
					"termine={8} importance={9}",
					t.rawState, (double)t.observation[0], (double)t.observation[1], (double)t.observation[2],
					(double)t.observation[3], t.action, (double)t.reward, t.nextRawState, t.done ? 1 : 0,
					(double)ag.Memory().ImportanceAt(ag.Memory().Size() - lastN + i));
	}

	// Preuve que la PERCEPTION encode correctement l'etat de depart et le but.
	NkVector<float> startFeatures;
	ag.Perceive(START, startFeatures);
	logger.Info("  perception(depart={0}) -> [{1},{2},{3},{4}]", START, (double)startFeatures[0],
				(double)startFeatures[1], (double)startFeatures[2], (double)startFeatures[3]);

	// Affiche la politique apprise (flèche = action gloutonne par case, via la couche NkAgent).
	logger.Info("-- Politique apprise (S=depart G=but O=trou) --");
	for (uint32 y = 0; y < N; ++y)
		logger.Info("  {0}", PolicyRow(ag, env, y).CStr());

	const bool ok = successRate >= 0.95;
	logger.Info("[ {0} ] l'agent (couche NkAgent) a appris a resoudre le monde-grille (>=95%)", ok ? "OK" : "KO");

	// =========================================================================
	// Test 2 (Jalon 2) : ablation MEMORY REPLAY a episodes REDUITS.
	// Meme monde, memes hyperparametres, memes graines : seule difference =
	// replayEnabled. Moyenne sur plusieurs graines pour lisser l'aleatoire.
	// =========================================================================
	const int reducedEpisodes = 80; // vs 4000 au test 1 : budget volontairement TRES reduit.
									 // Mesure (pas suppose) : a 1500 et meme 200 episodes, les
									 // DEUX bras saturent deja a 100% sur cette grille 5x5 (le
									 // schema d'epsilon s'adapte au budget) — l'ecart ne peut se
									 // voir qu'avant saturation, d'ou ce budget famine.
	const uint32 seeds[] = {11u, 22u, 33u, 44u, 55u};
	const int numSeeds = 5;
	logger.Info("-- Jalon 2 : replay + importance -- AVEC vs SANS replay, {0} episodes (vs {1}), {2} graines --",
				reducedEpisodes, episodes, numSeeds);

	double sumBase = 0.0, sumReplay = 0.0;
	uint64 lastReplayed = 0;
	float lastMemMin = 0.0f, lastMemMax = 0.0f;
	for (int i = 0; i < numSeeds; ++i) {
		uint64 replayedB = 0, replayedR = 0;
		float minB, maxB, minR, maxR;
		const double sBase =
			TrainAndEval(N, START, GOAL, holes, reducedEpisodes, /*replay*/ false, seeds[i], replayedB, minB, maxB);
		const double sReplay =
			TrainAndEval(N, START, GOAL, holes, reducedEpisodes, /*replay*/ true, seeds[i], replayedR, minR, maxR);
		sumBase += sBase;
		sumReplay += sReplay;
		lastReplayed = replayedR;
		lastMemMin = minR;
		lastMemMax = maxR;
		logger.Info("  graine {0} : SANS replay = {1}%   AVEC replay = {2}%  (transitions rejouees : {3})", seeds[i],
					sBase * 100.0, sReplay * 100.0, replayedR);
	}
	const double meanBase = sumBase / (double)numSeeds;
	const double meanReplay = sumReplay / (double)numSeeds;
	logger.Info("  moyenne ({0} graines) : SANS replay = {1}%   AVEC replay = {2}%", numSeeds, meanBase * 100.0,
				meanReplay * 100.0);
	logger.Info("  preuve importance (dernier agent replay) : importance retenue min={0} max={1} (eviction par "
				"moindre importance, pas FIFO)",
				(double)lastMemMin, (double)lastMemMax);
	logger.Info("  transitions rejouees par le dernier agent replay : {0}", lastReplayed);

	// Le replay doit faire strictement mieux a budget reduit (acceleration).
	const bool ok2 = meanReplay > meanBase;
	logger.Info("[ {0} ] le memory replay accelere l'apprentissage a budget reduit (moyenne AVEC > SANS)",
				ok2 ? "OK" : "KO");

	// =========================================================================
	// Test 3 (Jalon 3) : buts & planification -- monde CLE-PUIS-PORTE.
	// SANS but (1 seul but = la porte) vs AVEC pile de buts (cle PUIS porte).
	// =========================================================================
	logger.Info("-- Jalon 3 : buts & planification -- SANS but (porte seule) vs AVEC pile (cle PUIS porte) --");

	// Verification directe et isolee : un but au budget de pas trop court est
	// marque Failed puis depile (reutilise le statut d'echec, pas de recherche
	// de plan alternatif -- "replanification" = abandon honnete + bascule).
	{
		const uint32 KD_N = 5, KD_START = 0, KD_KEY = 12, KD_DOOR = 24;
		NkVector<uint32> noHoles;
		rl::NkKeyDoorGridWorld env(KD_N, KD_START, KD_KEY, KD_DOOR, noHoles, /*stepCost*/ 0.02f);
		agent::NkAgentConfig cfg;
		cfg.seed = 99u;
		cfg.epsilon = 1.0f;
		agent::NkAgent ag(env.NumStates(), env.NumActions(), KD_N, KD_DOOR % KD_N, KD_DOOR / KD_N, cfg);

		agent::NkGoalStack goals;
		agent::NkAgentGoal impossible;
		impossible.targetState = env.DoorOpenedState(); // quasi impossible en 1 pas depuis le depart
		impossible.maxSteps = 1;						  // budget volontairement minuscule
		goals.Push(impossible);

		uint32 s0 = env.Reset();
		agent::NkAgentGoalStepResult step = ag.StepWithGoals(env, s0, /*learn*/ true, goals);
		const bool expiredAsExpected = step.goalFailed && goals.IsEmpty();
		logger.Info("  verification expiration de but (maxSteps=1) : apres 1 pas -> but_atteint={0} but_echoue={1} "
					"pile_vide={2}",
					step.goalReached ? 1 : 0, step.goalFailed ? 1 : 0, goals.IsEmpty() ? 1 : 0);
		logger.Info("[ {0} ] un but juge inatteignable (budget de pas depasse) est marque Failed puis depile",
					expiredAsExpected ? "OK" : "KO");
	}

	// Ablation cle-puis-porte, moyennee sur plusieurs graines (meme monde, memes
	// hyperparametres/graines -- seule difference = le plan pose sur la pile).
	const uint32 KD_N = 5, KD_START = 0, KD_KEY = 12, KD_DOOR = 24;
	NkVector<uint32> kdHoles; // pas de trous : isole la difficulte du chainage cle->porte
	const int kdEpisodes = 100;
	const uint32 kdMaxSteps = 10; // tres serre : chemin optimal 0->cle->porte = 8 pas (4+4)
	const int kdEvalEpisodes = 100;
	const uint32 kdSeeds[] = {11u, 22u, 33u, 44u, 55u};
	const int kdNumSeeds = 5;

	double sumNoGoal = 0.0, sumGoal = 0.0;
	uint64 sumBlockedNoGoal = 0, sumBlockedGoal = 0;
	uint64 sumBlockedTrainNoGoal = 0, sumBlockedTrainGoal = 0;
	for (int i = 0; i < kdNumSeeds; ++i) {
		uint64 blockedNoGoal = 0, blockedGoal = 0;
		uint64 blockedTrainNoGoal = 0, blockedTrainGoal = 0;
		const double sNoGoal = TrainAndEvalKeyDoor(KD_N, KD_START, KD_KEY, KD_DOOR, kdHoles, kdEpisodes,
													/*useSubgoal*/ false, kdSeeds[i], kdMaxSteps, kdEvalEpisodes,
													blockedNoGoal, blockedTrainNoGoal);
		const double sGoal = TrainAndEvalKeyDoor(KD_N, KD_START, KD_KEY, KD_DOOR, kdHoles, kdEpisodes,
												  /*useSubgoal*/ true, kdSeeds[i], kdMaxSteps, kdEvalEpisodes,
												  blockedGoal, blockedTrainGoal);
		sumNoGoal += sNoGoal;
		sumGoal += sGoal;
		sumBlockedNoGoal += blockedNoGoal;
		sumBlockedGoal += blockedGoal;
		sumBlockedTrainNoGoal += blockedTrainNoGoal;
		sumBlockedTrainGoal += blockedTrainGoal;
		logger.Info("  graine {0} : SANS but = {1}% (portes bloquees entrainement={2} eval={3})   AVEC pile de buts "
					"= {4}% (portes bloquees entrainement={5} eval={6})",
					kdSeeds[i], sNoGoal * 100.0, blockedTrainNoGoal, blockedNoGoal, sGoal * 100.0, blockedTrainGoal,
					blockedGoal);
	}
	const double meanNoGoal = sumNoGoal / (double)kdNumSeeds;
	const double meanGoal = sumGoal / (double)kdNumSeeds;
	logger.Info("  moyenne ({0} graines) : SANS but = {1}%   AVEC pile de buts (planification) = {2}%", kdNumSeeds,
				meanNoGoal * 100.0, meanGoal * 100.0);
	logger.Info("  total tentatives porte bloquee (5 graines) : SANS but = entrainement:{0}/eval:{1}   AVEC pile de "
				"buts = entrainement:{2}/eval:{3}",
				sumBlockedTrainNoGoal, sumBlockedNoGoal, sumBlockedTrainGoal, sumBlockedGoal);

	// L'agent SANS but doit echouer structurellement (quasi 0%), celui AVEC pile
	// de buts doit clairement resoudre la tache (nettement au-dessus).
	const bool ok3 = (meanGoal > meanNoGoal) && (meanGoal >= 0.8) && (meanNoGoal <= 0.2);
	logger.Info("[ {0} ] la decomposition en sous-buts (pile) resout cle-puis-porte la ou l'agent SANS but echoue",
				ok3 ? "OK" : "KO");

	// =========================================================================
	// Test 4 (Jalon 4a) : PERSONNALITE -- deux profils de traits (Audacieux vs
	// Prudent) appliques a EXACTEMENT la meme politique DEJA entrainee (`ag`,
	// Test 1, evaluation gloutonne ~100% de reussite) via
	// NkAgent::StepWithPersonality -- isole l'effet de la personnalite de tout
	// effet d'apprentissage (meme table Q pour les deux profils, seul le CHOIX
	// d'action a la decision differe). Cf NkAgentPersonality.h pour le detail
	// des 3 traits (boldness/curiosity/patience).
	// =========================================================================
	logger.Info("-- Jalon 4a : personnalite -- Audacieux (Bold) vs Prudent (Cautious), meme politique entrainee --");

	auto EvalPersonality = [&](const agent::NkAgentPersonality &personality, uint32 rngSeed, int episodes,
								uint32 maxStepsEp, int &outSuccesses, int &outHoleFalls, uint64 &outRiskySteps,
								uint64 &outTotalSteps) {
		uint32 rngState = rngSeed ? rngSeed : 1u;
		outSuccesses = 0;
		outHoleFalls = 0;
		outRiskySteps = 0;
		outTotalSteps = 0;
		for (int e = 0; e < episodes; ++e) {
			uint32 s = env.Reset();
			bool done = false;
			float lastR = 0.0f;
			for (uint32 step = 0; step < maxStepsEp && !done; ++step) {
				agent::NkAgentStepResult r = ag.StepWithPersonality(env, s, personality, rngState);
				++outTotalSteps;
				if (agent::NkGridWorldIsRiskyState(env, r.nextRawState))
					++outRiskySteps;
				done = r.done;
				lastR = r.reward;
				s = r.nextRawState;
			}
			if (done && lastR > 0.5f)
				++outSuccesses;
			if (done && lastR < 0.0f)
				++outHoleFalls;
		}
	};

	int boldSucc = 0, boldFalls = 0;
	int cautSucc = 0, cautFalls = 0;
	uint64 boldRisky = 0, boldSteps = 0, cautRisky = 0, cautSteps = 0;
	const int persEvalEpisodes = 300;
	EvalPersonality(agent::NkAgentPersonality::Bold(), 777u, persEvalEpisodes, 100u, boldSucc, boldFalls, boldRisky,
					boldSteps);
	EvalPersonality(agent::NkAgentPersonality::Cautious(), 888u, persEvalEpisodes, 100u, cautSucc, cautFalls, cautRisky,
					cautSteps);

	const double boldRiskyRate = boldSteps ? (double)boldRisky / (double)boldSteps : 0.0;
	const double cautRiskyRate = cautSteps ? (double)cautRisky / (double)cautSteps : 0.0;
	logger.Info("  Audacieux (boldness=1.0 curiosity=0.35) : succes={0}/{1} ({2}%)  chutes_trou={3}  "
				"pas_risques={4}/{5} ({6}%)  pas_moyens/episode={7}",
				boldSucc, persEvalEpisodes, 100.0 * boldSucc / persEvalEpisodes, boldFalls, boldRisky, boldSteps,
				100.0 * boldRiskyRate, (double)boldSteps / persEvalEpisodes);
	logger.Info("  Prudent   (boldness=0.0 curiosity=0.02) : succes={0}/{1} ({2}%)  chutes_trou={3}  "
				"pas_risques={4}/{5} ({6}%)  pas_moyens/episode={7}",
				cautSucc, persEvalEpisodes, 100.0 * cautSucc / persEvalEpisodes, cautFalls, cautRisky, cautSteps,
				100.0 * cautRiskyRate, (double)cautSteps / persEvalEpisodes);

	const bool ok4a = (cautRiskyRate < boldRiskyRate) && (cautFalls <= boldFalls);
	logger.Info("[ {0} ] la personnalite fait varier MESURABLEMENT le comportement (meme politique) : le prudent "
				"longe moins les trous ({1}% vs {2}%) et ne tombe pas plus souvent ({3} vs {4} chutes)",
				ok4a ? "OK" : "KO", 100.0 * cautRiskyRate, 100.0 * boldRiskyRate, cautFalls, boldFalls);

	// Trait "patience" : verification directe et isolee (budget de but, Jalon 3)
	// -- meme esprit que la verification isolee de maxSteps=1 du Test 3.
	bool okPatience = false;
	{
		const uint32 baseBudget = 10;
		const agent::NkAgentPersonality patient = agent::NkAgentPersonality::Cautious(); // patience=1.6
		const agent::NkAgentPersonality impatient = agent::NkAgentPersonality::Bold();	  // patience=0.6
		const uint32 patientBudget = agent::NkPatienceAdjustedMaxSteps(baseBudget, patient.patience);
		const uint32 impatientBudget = agent::NkPatienceAdjustedMaxSteps(baseBudget, impatient.patience);
		logger.Info("  Trait patience (but base maxSteps={0}) : patient(x{1})={2} pas   impatient(x{3})={4} pas",
					baseBudget, (double)patient.patience, patientBudget, (double)impatient.patience, impatientBudget);
		okPatience = patientBudget > baseBudget && impatientBudget < baseBudget && impatientBudget >= 1;
		logger.Info("[ {0} ] le trait patience module reellement le budget d'abandon d'un but (Jalon 3)",
					okPatience ? "OK" : "KO");
	}
	const bool ok4 = ok4a && okPatience;

	const int nOk = (ok ? 1 : 0) + (ok2 ? 1 : 0) + (ok3 ? 1 : 0) + (ok4 ? 1 : 0);
	logger.Info("=== Resultat : {0} OK, {1} echec(s) ===", nOk, 4 - nOk);
	return (ok && ok2 && ok3 && ok4) ? 0 : 1;
}
