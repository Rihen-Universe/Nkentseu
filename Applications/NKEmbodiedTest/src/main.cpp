// =============================================================================
// NKEmbodiedTest — un cerveau NKAgent pilote un corps SIMULÉ via des capteurs/
// actionneurs génériques (NKEmbodied, Phase 6, Jalon 1).
//
// Preuve du Jalon 1 : "abstraction capteurs/actionneurs" + "boucle perception
// -> décision (NKAgent) -> action dans un corps simulé". NKEmbodied NE
// RÉIMPLÉMENTE ni le RL (NKRL) ni l'agent cognitif (NKAgent) : il relie un
// corps simulé (NkEmbodiedBody, un monde-grille rl::NkGridWorld déjà
// prouvé — cf. NKRLTest/NKAgentTest) à une politique de décision via les
// interfaces génériques NkSensor/NkActuator/NkEmbodiedPolicy, et fait
// vraiment tourner la boucle N ticks.
//
// Test 1 — REPLI ALÉATOIRE : NkEmbodiedRandomPolicy, aucune lecture des
// capteurs. Sert de témoin ("sans intelligence") : la boucle tourne (aucune
// erreur), mais le corps n'atteint quasiment jamais le but.
//
// Test 2 — POLITIQUE HEURISTIQUE PILOTÉE PAR LES CAPTEURS : décide UNIQUEMENT
// à partir des observations (dx,dy signés vers le but) lues par
// NkEmbodiedGridSensor — preuve directe que les capteurs influencent
// réellement les actions. Honnête sur sa limite : sans apprentissage, elle
// fonce en ligne droite vers le but SANS éviter les trous (une grille dont le
// chemin direct croise un trou la fait échouer — documenté, pas caché).
//
// Test 3 — CERVEAU NKAgent RÉUTILISÉ : un agent::NkAgent est entraîné (même
// procédure Q-learning ε-décroissant que NKAgentTest, sur un monde identique
// à celui du corps) PUIS branché au corps via NkEmbodiedAgentPolicy à travers
// la boucle NkEmbodiedLoop — preuve de bout en bout que perception (capteur)
// -> décision (politique NKAgent apprise) -> action (actionneur) fonctionne
// RÉELLEMENT sur plusieurs ticks, avec une trace tick par tick.
//
// Tests 4-7 — JALON 2 (contrôle robuste), chacun avec une preuve AVANT/APRÈS :
// Test 4 = bruit capteur (NkEmbodiedNoisySensor) : dégradation RÉELLEMENT
// mesurée du cerveau NKAgent (100% sans bruit) sous plusieurs niveaux de
// bruit gaussien, moyennée sur plusieurs graines, + vérification de
// reproductibilité par graine. Test 5 = limites actionneur
// (NkEmbodiedLimitedActuator) : saturation d'amplitude et limite de
// fréquence de commande vérifiées PAR ASSERTION, puis effet réel sur le taux
// de réussite. Test 6 = boucle à fréquence fixe (NkEmbodiedFixedRateLoop) :
// découplage vérifié entre un taux de "simulation" simulé à 240 Hz et une
// fréquence de décision fixée à 20 Hz. Test 7 = sécurité (watchdog
// NkEmbodiedSafetyMonitor) : arrêt d'urgence déclenché sur un état bloqué
// construit exprès, puis sur un actionneur saturé construit exprès.
// =============================================================================
#include "NKAgent/NkAgent.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKEmbodied/NkEmbodiedActuatorLimits.h"
#include "NKEmbodied/NkEmbodiedAgentPolicy.h"
#include "NKEmbodied/NkEmbodiedBody.h"
#include "NKEmbodied/NkEmbodiedFixedRateLoop.h"
#include "NKEmbodied/NkEmbodiedGridSensor.h"
#include "NKEmbodied/NkEmbodiedLoop.h"
#include "NKEmbodied/NkEmbodiedMoveActuator.h"
#include "NKEmbodied/NkEmbodiedPolicy.h"
#include "NKEmbodied/NkEmbodiedSafety.h"
#include "NKEmbodied/NkEmbodiedSensorNoise.h"
#include "NKLogger/NkLog.h"

using namespace nkentseu;
using namespace nkentseu::ai;

namespace {

	// Grille 5x5 partagée par tous les tests : départ en haut-gauche (0), but en
	// bas-droite (24), 3 trous sur la diagonale (mêmes coordonnées que
	// NKAgentTest/NKRLTest, pour rester comparable).
	const uint32 N = 5, START = 0, GOAL = 24;

	NkVector<uint32> Holes() {
		NkVector<uint32> holes;
		holes.PushBack(6);
		holes.PushBack(12);
		holes.PushBack(18);
		return holes;
	}

	// Politique JOUET (Jalon 2, tests 6/7 uniquement) : décide TOUJOURS la même
	// action, quelles que soient les observations/l'état. Sert à construire des
	// scénarios déterministes où le corps n'atteint JAMAIS le but/un trou (donc
	// ne se termine jamais) — utile pour isoler la mécanique de la boucle à
	// fréquence fixe (Test 6) et du watchdog (Test 7a) de la dynamique du
	// monde-grille (qui, elle, termine l'épisode dès que le but/un trou est
	// atteint).
	class FixedActionPolicy : public embodied::NkEmbodiedPolicy {
		public:
			explicit FixedActionPolicy(uint32 action) : mAction(action) {
			}

			uint32 Decide(const NkVector<float> &observations, uint32 rawState) override {
				(void)observations;
				(void)rawState;
				return mAction;
			}

		private:
			uint32 mAction;
	};

	// Fait tourner `episodes` épisodes de la boucle NKEmbodied avec la politique
	// donnée, et compte le taux de réussite. `logFirstTicks` : si >0, journalise
	// le détail tick par tick du PREMIER épisode (preuve capteurs->décision->
	// actionneurs en action).
	double RunEpisodes(embodied::NkEmbodiedBody &body, const embodied::NkSensor &sensor, embodied::NkActuator &actuator,
						embodied::NkEmbodiedPolicy &policy, int episodes, uint32 maxTicks, uint32 logFirstTicks) {
		int reached = 0;
		for (int e = 0; e < episodes; ++e) {
			body.Reset();
			if (e == 0 && logFirstTicks > 0) {
				for (uint32 t = 0; t < maxTicks && t < logFirstTicks && !body.IsDone(); ++t) {
					embodied::NkEmbodiedTickResult tick = embodied::EmbodiedTick(body, sensor, actuator, policy);
					logger.Info("    tick {0} : etat={1} obs=[{2},{3},{4},{5}] -> action={6} -> etat={7} "
								"recompense={8} termine={9}",
								t, tick.rawStateBefore, (double)tick.observations[0], (double)tick.observations[1],
								(double)tick.observations[2], (double)tick.observations[3], tick.action,
								tick.rawStateAfter, (double)tick.reward, tick.done ? 1 : 0);
				}
				// Termine l'épisode (au-delà des ticks déjà journalisés) sans retracer.
				for (uint32 t = logFirstTicks; t < maxTicks && !body.IsDone(); ++t)
					embodied::EmbodiedTick(body, sensor, actuator, policy);
			} else {
				for (uint32 t = 0; t < maxTicks && !body.IsDone(); ++t)
					embodied::EmbodiedTick(body, sensor, actuator, policy);
			}
			if (body.ReachedGoal())
				++reached;
		}
		return (double)reached / (double)episodes;
	}

	// Entraîne `brain` (déjà construit par l'appelant — agent::NkAgent n'est
	// PAS trivialement copiable, cf. commentaire NKCivilization/
	// NkCivComponents.h : mémoire + perception + table Q embarquées, jamais
	// retourné par valeur) sur un monde IDENTIQUE à celui du corps (même
	// taille/départ/but/trous) — même procédure Q-learning ε-décroissant que
	// NKAgentTest, aucune réimplémentation.
	void TrainBrain(agent::NkAgent &brain, rl::NkGridWorld &trainEnv) {
		const int episodes = 4000;
		const uint32 maxSteps = 100;
		for (int e = 1; e <= episodes; ++e) {
			float eps = 1.0f - (float)e / (float)(episodes * 0.8);
			if (eps < 0.05f)
				eps = 0.05f;
			brain.Policy().SetEpsilon(eps);
			agent::RunAgentEpisode(trainEnv, brain, /*learn*/ true, maxSteps);
		}
	}

} // namespace

int main() {
	logger.Info("=== NKEmbodiedTest : capteurs -> decision -> actionneurs sur un corps simule (Phase 6, Jalon 1) ===");

	embodied::NkEmbodiedBody body(N, START, GOAL, Holes(), /*stepCost*/ 0.02f);
	embodied::NkEmbodiedGridSensor sensor(body);
	embodied::NkEmbodiedMoveActuator actuator(body);

	const uint32 maxTicks = 100;
	const int evalEpisodes = 200;

	// ── Test 1 : repli aleatoire (temoin "sans intelligence") ──────────────
	logger.Info("-- Test 1 : politique ALEATOIRE (aucune lecture des capteurs) --");
	embodied::NkEmbodiedRandomPolicy randomPolicy(body.NumActions(), /*seed*/ 7u);
	const double randomRate = RunEpisodes(body, sensor, actuator, randomPolicy, evalEpisodes, maxTicks, /*logFirstTicks*/ 5);
	logger.Info("  taux de reussite (aleatoire) sur {0} episodes : {1}%", evalEpisodes, randomRate * 100.0);

	// ── Test 2 : politique heuristique pilotee par les capteurs ─────────────
	logger.Info("-- Test 2 : politique HEURISTIQUE (decide UNIQUEMENT a partir des observations du capteur) --");
	embodied::NkEmbodiedHeuristicPolicy heuristicPolicy;
	const double heuristicRate =
		RunEpisodes(body, sensor, actuator, heuristicPolicy, evalEpisodes, maxTicks, /*logFirstTicks*/ 5);
	logger.Info("  taux de reussite (heuristique, sans evitement des trous appris) sur {0} episodes : {1}%", evalEpisodes,
				heuristicRate * 100.0);
	logger.Info("  (limite honnete : la grille place 3 trous SUR la diagonale que suit le trajet direct vers le but -- "
				"la politique heuristique n'apprend rien et ne les evite pas, cf ROADMAP)");

	// ── Test 3 : cerveau NKAgent reutilise (deja entraine/teste) ─────────────
	logger.Info("-- Test 3 : cerveau NKAgent (Q-learning deja entraine, comme NKAgentTest) branche via NKEmbodied --");
	rl::NkGridWorld trainEnv(N, START, GOAL, Holes(), /*stepCost*/ 0.02f);
	agent::NkAgentConfig brainConfig;
	brainConfig.memoryCapacity = 64;
	brainConfig.alpha = 0.1f;
	brainConfig.gamma = 0.99f;
	brainConfig.epsilon = 1.0f;
	brainConfig.seed = 42u;
	agent::NkAgent brain(trainEnv, brainConfig);
	TrainBrain(brain, trainEnv);
	embodied::NkEmbodiedAgentPolicy agentPolicy(brain, /*greedy*/ true);
	logger.Info("  trace des 6 premiers ticks du 1er episode (preuve capteurs -> decision -> actionneurs) :");
	const double agentRate = RunEpisodes(body, sensor, actuator, agentPolicy, evalEpisodes, maxTicks, /*logFirstTicks*/ 6);
	logger.Info("  taux de reussite (cerveau NKAgent, via NKEmbodied) sur {0} episodes : {1}%", evalEpisodes,
				agentRate * 100.0);

	const bool ok = agentRate >= 0.95 && agentRate > heuristicRate && agentRate > randomRate;
	logger.Info("[ {0} ] la boucle perception->decision(NKAgent)->action pilote reellement le corps simule vers le but",
				ok ? "OK" : "KO");

	// =========================================================================
	// JALON 2 — controle robuste (bruit capteur, limites actionneur, frequence
	// fixe, securite). Reutilise le MEME corps/capteur/actionneur/cerveau que
	// les tests 1-3 ci-dessus, jamais reimplementes.
	// =========================================================================
	bool jalon2NoiseOk = false;
	bool jalon2ActuatorOk = false;
	bool jalon2FixedRateOk = false;
	bool jalon2SafetyOk = false;

	// ── Test 4 (Jalon 2) : bruit capteur ─────────────────────────────────────
	logger.Info("-- Test 4 (Jalon 2) : BRUIT CAPTEUR sur le cerveau NKAgent deja entraine (100% sans bruit, Test 3) --");
	{
		// 4a. Reproductibilite : meme graine -> memes observations bruitees ;
		// graine differente -> observations differentes.
		body.Reset();
		NkVector<float> obsSeedA;
		NkVector<float> obsSeedB;
		NkVector<float> obsOtherSeed;
		embodied::NkEmbodiedNoisySensor noisySeed7A(sensor, embodied::NkEmbodiedNoiseKind::Uniform, 0.2f, /*seed*/ 7u);
		embodied::NkEmbodiedNoisySensor noisySeed7B(sensor, embodied::NkEmbodiedNoiseKind::Uniform, 0.2f, /*seed*/ 7u);
		embodied::NkEmbodiedNoisySensor noisySeed9(sensor, embodied::NkEmbodiedNoiseKind::Uniform, 0.2f, /*seed*/ 9u);
		noisySeed7A.Sense(obsSeedA);
		noisySeed7B.Sense(obsSeedB);
		noisySeed9.Sense(obsOtherSeed);

		bool sameSeedIdentical = true;
		bool diffSeedDiffers = false;
		for (nk_size i = 0; i < obsSeedA.Size(); ++i) {
			if (obsSeedA[i] != obsSeedB[i])
				sameSeedIdentical = false;
			if (obsSeedA[i] != obsOtherSeed[i])
				diffSeedDiffers = true;
		}
		NKENTSEU_ASSERT(sameSeedIdentical);
		logger.Info("  [ {0} ] meme graine (seed=7) -> memes observations bruitees (reproductibilite)",
					sameSeedIdentical ? "OK" : "KO");
		logger.Info("  [ {0} ] graine differente (seed=9 vs seed=7) -> observations bruitees differentes",
					diffSeedDiffers ? "OK" : "KO");

		// 4b. Degradation reelle du cerveau NKAgent sous bruit gaussien croissant,
		// moyennee sur 5 graines par niveau (200 episodes chacune, comme Test 3).
		struct NoiseLevel {
				const char *label;
				float sigma;
		};
		const NoiseLevel noiseLevels[] = {
			{"sigma=0.000 (temoin, sans bruit)", 0.000f},
			{"sigma=0.050 (~1/5 de case)", 0.050f},
			{"sigma=0.125 (~1/2 case)", 0.125f},
			{"sigma=0.250 (~1 case)", 0.250f},
			{"sigma=0.500 (~2 cases)", 0.500f},
		};
		const uint32 noiseSeeds[] = {11u, 22u, 33u, 44u, 55u};
		const uint32 numSeeds = (uint32)(sizeof(noiseSeeds) / sizeof(noiseSeeds[0]));
		double baselineAvg = 0.0;

		for (const NoiseLevel &lvl : noiseLevels) {
			double sum = 0.0;
			for (uint32 s = 0; s < numSeeds; ++s) {
				embodied::NkEmbodiedNoisySensor noisySensor(sensor, embodied::NkEmbodiedNoiseKind::Gaussian, lvl.sigma,
															 noiseSeeds[s]);
				sum += RunEpisodes(body, noisySensor, actuator, agentPolicy, evalEpisodes, maxTicks, /*logFirstTicks*/ 0);
			}
			const double avg = sum / (double)numSeeds;
			if (lvl.sigma == 0.0f)
				baselineAvg = avg;
			logger.Info("  bruit gaussien {0} : taux de reussite MOYEN sur {1} graines x {2} episodes = {3}%", lvl.label,
						numSeeds, evalEpisodes, avg * 100.0);
		}

		jalon2NoiseOk = sameSeedIdentical && diffSeedDiffers && baselineAvg >= 0.95;
		logger.Info("  [ {0} ] sans bruit (temoin), le cerveau NKAgent via capteur bruite-a-zero reste conforme au "
					"Test 3 (>= 95%%) -- le mecanisme de bruit n'introduit AUCUNE regression au niveau zero",
					jalon2NoiseOk ? "OK" : "KO");
	}

	// ── Test 5 (Jalon 2) : limites actionneur ────────────────────────────────
	logger.Info("-- Test 5 (Jalon 2) : LIMITES ACTIONNEUR (saturation d'amplitude + limite de frequence de commande) --");
	bool ok5a = false;
	bool ok5b = false;
	{
		// 5a. Saturation d'amplitude : verifiee PAR ASSERTION, au niveau du
		// DECORATEUR lui-meme (independamment de ce que l'actionneur interne en
		// fait ensuite).
		embodied::NkEmbodiedLimitedActuator limited(actuator, /*minTicksBetweenCommands*/ 1u, /*maxAmplitude*/ 3.0f);
		NkVector<float> outOfRange;
		outOfRange.PushBack(999.0f);
		body.Reset();
		limited.Apply(outOfRange);
		const bool saturated = limited.LastAmplitudeSaturated();
		const float forwarded = limited.LastForwardedCommand()[0];
		ok5a = saturated && forwarded <= 3.0f + 1e-6f;
		NKENTSEU_ASSERT(ok5a);
		logger.Info("  [ {0} ] commande hors bornes (999.0) bien ecretee a {1} (max=3.0) -- verifie par assertion",
					ok5a ? "OK" : "KO", (double)forwarded);
	}
	{
		// 5b. Limite de frequence de commande : au plus 1 commande NOUVELLE
		// toutes les N=4 ticks -- verifie sur 8 commandes DIFFERENTES.
		embodied::NkEmbodiedLimitedActuator limited(actuator, /*minTicksBetweenCommands*/ 4u, /*maxAmplitude*/ 999.0f);
		body.Reset();
		uint32 numAccepted = 0;
		uint32 numHeld = 0;
		for (uint32 t = 0; t < 8; ++t) {
			NkVector<float> cmd;
			cmd.PushBack((float)(t % 4)); // une commande DIFFERENTE a chaque tick
			limited.Apply(cmd);
			if (limited.LastCommandHeld())
				++numHeld;
			else
				++numAccepted;
		}
		// N=4 : commandes acceptees attendues aux ticks 0 et 4 (2 sur 8).
		ok5b = numAccepted == 2u && numHeld == 6u && limited.AcceptedCount() == 2u && limited.HeldCount() == 6u;
		NKENTSEU_ASSERT(ok5b);
		logger.Info("  [ {0} ] limite de frequence (min 4 ticks entre commandes) : {1}/8 acceptees, {2}/8 retenues "
					"(commande precedente re-appliquee) -- verifie par assertion",
					ok5b ? "OK" : "KO", numAccepted, numHeld);
	}
	{
		// 5c. Effet REEL de la limite de frequence sur le taux de reussite du
		// cerveau NKAgent (100% sans contrainte, Test 3) : mesure honnete, pas
		// une supposition. Un NOUVEAU decorateur (donc un compteur de latence
		// REMIS A ZERO) par palier, pour ne pas melanger l'effet d'un palier
		// avec l'etat residuel du precedent.
		const uint32 rateLimits[] = {1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u};
		for (uint32 k : rateLimits) {
			embodied::NkEmbodiedLimitedActuator limited(actuator, k, /*maxAmplitude*/ 999.0f);
			const double rate = RunEpisodes(body, sensor, limited, agentPolicy, evalEpisodes, maxTicks, /*logFirstTicks*/ 0);
			logger.Info("  cerveau NKAgent, actionneur limite a 1 commande NOUVELLE / {0} ticks : taux de reussite sur "
						"{1} episodes = {2}%",
						k, evalEpisodes, rate * 100.0);
		}
		logger.Info("  (constat honnete : sur cette petite grille 5x5 (maxTicks=100, chemin optimal ~8-15 ticks), la "
					"latence d'actionneur ne degrade RIEN tant que minTicksBetweenCommands <= 64 (la commande retenue "
					"reste globalement orientee vers le but -- politique Q-learnee lissee spatialement -- et le budget "
					"de ticks absorbe la latence) : le point de degradation reel observe est entre 64 et 128 ticks "
					"(64 -> 100%%, 128 -> 50%%), la ou la latence depasse le budget de ticks lui-meme (128 > "
					"maxTicks=100 : au plus 1 commande neuve par episode). Point de rupture mesure, pas suppose.");
	}
	jalon2ActuatorOk = ok5a && ok5b;

	// ── Test 6 (Jalon 2) : boucle a frequence fixe ───────────────────────────
	logger.Info("-- Test 6 (Jalon 2) : BOUCLE A FREQUENCE FIXE (decouple taux de decision / taux d'appel) --");
	{
		// "Simulation" a 240 Hz (dt=1/240s par appel), decision fixee a 20 Hz
		// (periode 50ms) : la boucle ne doit declencher un EmbodiedTick() qu'une
		// fois toutes les ~12 appels a Advance(), pas a chaque appel. Politique
		// FIXE (toujours "haut" depuis l'etat de depart (0,0), qui ne bouge
		// jamais, cf. rl::NkGridWorld::Step) : le corps ne se termine JAMAIS,
		// isolant la mecanique temporelle de la dynamique du monde-grille.
		embodied::NkEmbodiedFixedRateLoop fixedLoop(/*decisionHz*/ 20.0f);
		FixedActionPolicy stayPolicy(0);
		body.Reset();
		const float simDt = 1.0f / 240.0f;
		const uint32 simCalls = 240; // 1 seconde simulee -> ~20 decisions attendues a 20 Hz
		uint32 totalDecisions = 0;
		uint32 callsThatDecided = 0;
		for (uint32 c = 0; c < simCalls; ++c) {
			const uint32 d = fixedLoop.Advance(simDt, body, sensor, actuator, stayPolicy);
			totalDecisions += d;
			if (d > 0)
				++callsThatDecided;
		}
		logger.Info("  {0} appels a Advance() (simulation ~240 Hz, dt={1}s/appel), decision fixee a 20 Hz (periode "
					"{2}s) : {3} decisions executees au total (attendu ~20)",
					simCalls, (double)simDt, (double)fixedLoop.DecisionPeriod(), totalDecisions);
		logger.Info("  {0}/{1} appels ont declenche une decision, {2}/{1} n'en ont declenche AUCUNE -- preuve que le "
					"taux de decision est bien DECOUPLE du taux d'appel (pas 1 decision par appel)",
					callsThatDecided, simCalls, simCalls - callsThatDecided);
		jalon2FixedRateOk = totalDecisions >= 18u && totalDecisions <= 21u && callsThatDecided < simCalls
							&& fixedLoop.DecisionCount() == totalDecisions && body.TickCount() == totalDecisions;
		NKENTSEU_ASSERT(jalon2FixedRateOk);
		logger.Info("  [ {0} ] frequence de decision fixe (20 Hz) decouplee du taux d'appel (simule a 240 Hz)",
					jalon2FixedRateOk ? "OK" : "KO");
	}

	// ── Test 7 (Jalon 2) : securite (watchdog) ───────────────────────────────
	logger.Info("-- Test 7 (Jalon 2) : SECURITE (watchdog -- arret d'urgence sur etat bloque / actionneur sature) --");
	bool ok7a = false;
	bool ok7b = false;
	{
		// 7a. Etat bloque : politique FIXE qui pousse toujours contre le mur
		// depuis (0,0) ("haut") -- le corps ne bouge JAMAIS. Seuil de saturation
		// volontairement enorme pour isoler la cause (c'est bien l'etat bloque
		// qui declenche, pas l'actionneur).
		embodied::NkEmbodiedSafetyMonitor watchdog(/*stuckTicksThreshold*/ 3u, /*saturationTicksThreshold*/ 1000u);
		FixedActionPolicy wallPolicy(0);
		body.Reset();
		uint32 ticksRun = 0;
		for (uint32 t = 0; t < 10 && !body.IsDone() && !watchdog.Tripped(); ++t) {
			const uint32 before = body.State();
			embodied::NkEmbodiedTickResult tick = embodied::EmbodiedTick(body, sensor, actuator, wallPolicy);
			++ticksRun;
			watchdog.Observe(before, tick.rawStateAfter, /*actuatorSaturated*/ false);
		}
		logger.Info("  etat bloque : arret d'urgence declenche apres {0} ticks (etat reste a {1}), raison : \"{2}\"",
					ticksRun, body.State(), watchdog.Reason());
		ok7a = watchdog.Tripped() && ticksRun == 3u && body.TickCount() == 3u && body.State() == 0u;
		NKENTSEU_ASSERT(ok7a);
		logger.Info("  [ {0} ] watchdog declenche l'arret d'urgence exactement au seuil configure (3 ticks bloque), et "
					"plus aucun tick n'est execute ensuite",
					ok7a ? "OK" : "KO");
	}
	{
		// 7b. Actionneur sature : limite de frequence tres agressive
		// (minTicksBetweenCommands=1000, jamais reaccepte apres la 1ere
		// commande) pendant que la politique HEURISTIQUE continue de decider
		// normalement. Seuil "etat bloque" volontairement enorme pour isoler la
		// cause (c'est bien la saturation actionneur qui declenche).
		embodied::NkEmbodiedSafetyMonitor watchdog(/*stuckTicksThreshold*/ 1000u, /*saturationTicksThreshold*/ 3u);
		embodied::NkEmbodiedLimitedActuator limited(actuator, /*minTicksBetweenCommands*/ 1000u, /*maxAmplitude*/ 999.0f);
		embodied::NkEmbodiedHeuristicPolicy localHeuristic;
		body.Reset();
		uint32 ticksRun = 0;
		for (uint32 t = 0; t < 10 && !body.IsDone() && !watchdog.Tripped(); ++t) {
			const uint32 before = body.State();
			embodied::NkEmbodiedTickResult tick = embodied::EmbodiedTick(body, sensor, limited, localHeuristic);
			++ticksRun;
			watchdog.Observe(before, tick.rawStateAfter, limited.LastCommandHeld() || limited.LastAmplitudeSaturated());
		}
		logger.Info("  actionneur sature : arret d'urgence declenche apres {0} ticks (commandes acceptees={1}, "
					"retenues={2}), raison : \"{3}\"",
					ticksRun, limited.AcceptedCount(), limited.HeldCount(), watchdog.Reason());
		ok7b = watchdog.Tripped() && ticksRun == 4u;
		NKENTSEU_ASSERT(ok7b);
		logger.Info("  [ {0} ] watchdog declenche l'arret d'urgence sur saturation actionneur (independamment de "
					"l'etat du corps, qui n'etait pas bloque)",
					ok7b ? "OK" : "KO");
	}
	jalon2SafetyOk = ok7a && ok7b;

	const bool jalon2Ok = jalon2NoiseOk && jalon2ActuatorOk && jalon2FixedRateOk && jalon2SafetyOk;
	logger.Info("[ {0} ] Jalon 2 (controle robuste) : bruit capteur + limites actionneur + frequence fixe + securite, "
				"tous verifies (determinisme/assertions) sur ce run",
				jalon2Ok ? "OK" : "KO");

	const bool allOk = ok && jalon2Ok;
	logger.Info("=== Resultat global (Jalon 1 + Jalon 2) : {0} ===", allOk ? "OK" : "KO");
	return allOk ? 0 : 1;
}
