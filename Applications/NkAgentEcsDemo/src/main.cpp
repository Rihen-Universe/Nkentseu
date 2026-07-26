// =============================================================================
// Applications/NkAgentEcsDemo/src/main.cpp
// =============================================================================
// Preuve d'exécution réelle pour le pont ECS -> NKAgent (Noge/ECS/Components/
// AI/NkAgentComponent.h + Noge/ECS/Systems/NkAgentSystem.h/.cpp).
//
// MÊME monde-grille, MÊMES hyperparamètres, MÊME schéma d'épsilon décroissant
// que Applications/NKAgentTest/src/main.cpp (déjà prouvé à 100% de réussite
// en évaluation gloutonne) — mais cette fois l'agent n'est PAS piloté par une
// boucle console appelant directement agent::NkAgent::Step() : il est
// attaché à UNE ENTITÉ d'un ecs::NkWorld Noge via un NkAgentComponent, et
// c'est NkAgentSystem::Execute() (le même système qu'utiliserait un vrai
// jeu Noge, groupe Update, un ecs::NkSystem standard) qui fait avancer
// l'agent d'un pas à chaque appel. La boucle d'entraînement/évaluation ici
// ne fait qu'appeler world + system.Execute() en boucle et lire l'état du
// composant — elle ne touche plus jamais directement rl::NkGridWorld::Step()
// ni agent::NkAgent::Step().
// =============================================================================
#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Systems/NkAgentSystem.h"
#include "NKContainers/String/NkString.h"
#include "NKLogger/NkLog.h"

using namespace nkentseu;
using namespace nkentseu::ecs;
using namespace nkentseu::ai;

namespace {

	// Construit la ligne "politique apprise" d'une rangée de la grille (flèche = action
	// gloutonne de la politique de l'agent). Identique à NKAgentTest::PolicyRow, mais lit
	// l'agent à travers le pointeur référencé par le composant ECS.
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

} // namespace

int main() {
	logger.Info("=== NkAgentEcsDemo : NKAgent (RL) rejoue comme une entite ECS Noge (NkAgentSystem) ===");

	// -- Monde-grille + agent (identiques a NKAgentTest) ------------------------------
	const uint32 N = 5, START = 0, GOAL = 24;
	NkVector<uint32> holes;
	holes.PushBack(6);
	holes.PushBack(12);
	holes.PushBack(18);
	rl::NkGridWorld env(N, START, GOAL, holes, /*stepCost*/ 0.02f);

	agent::NkAgentConfig config;
	config.memoryCapacity = 64;
	config.alpha = 0.1f;
	config.gamma = 0.99f;
	config.epsilon = 1.0f;
	config.seed = 42u;
	agent::NkAgent ag(env, config);

	// -- Monde ECS Noge : une seule entite porte le NkAgentComponent -------------------
	NkWorld world;
	NkAgentSystem agentSystem;

	NkEntityId entity = world.CreateEntity();
	NkAgentComponent &ac = world.Add<NkAgentComponent>(entity);
	ac.agent = &ag;
	ac.environment = &env;
	ac.learn = true;
	ac.autoResetOnDone = true;
	ac.maxStepsPerEpisode = 100;

	logger.Info("-- Entite ECS creee : index={0} gen={1}, NkAgentComponent branche sur agent+environnement --",
				entity.index, entity.gen);

	// -- Entrainement (4000 episodes, epsilon decroissant) via NkAgentSystem::Execute --
	const int episodes = 4000;
	logger.Info("-- Entrainement ({0} episodes, epsilon decroissant, via NkAgentSystem sur entite ECS) --", episodes);

	uint32 lastLoggedAtEpisode = 0;
	uint32 lastLoggedReachedGoal = 0;

	while ((int)ac.episodesCompleted < episodes) {
		// Schedule d'epsilon identique a NKAgentTest : 1.0 -> 0.05 sur les 80 premiers %.
		const uint32 e = ac.episodesCompleted + 1;
		float eps = 1.0f - (float)e / (float)(episodes * 0.8);
		if (eps < 0.05f)
			eps = 0.05f;
		ag.Policy().SetEpsilon(eps);

		// UN pas ECS = UN appel a NkAgentSystem::Execute() sur le monde entier
		// (ici une seule entite, mais l'API est celle d'un vrai jeu Noge).
		agentSystem.Execute(world, 0.0f);

		if (ac.episodesCompleted != 0 && ac.episodesCompleted % 800 == 0 &&
			ac.episodesCompleted != lastLoggedAtEpisode) {
			const uint32 reachedInWindow = ac.episodesReachedGoal - lastLoggedReachedGoal;
			logger.Info("  episode {0} : succes(800 derniers) = {1}%  eps={2}", ac.episodesCompleted,
						100.0 * (double)reachedInWindow / 800.0, (double)eps);
			lastLoggedAtEpisode = ac.episodesCompleted;
			lastLoggedReachedGoal = ac.episodesReachedGoal;
		}
	}

	logger.Info("-- Fin entrainement : {0} episodes, {1} ont atteint le but ({2}%) --", ac.episodesCompleted,
				ac.episodesReachedGoal, 100.0 * (double)ac.episodesReachedGoal / (double)ac.episodesCompleted);

	// -- Evaluation : politique gloutonne pure, via NkAgentSystem (learn=false) --------
	ac.learn = false;
	ac.episodesCompleted = 0;
	ac.episodesReachedGoal = 0;
	ac.hasState = false;
	ag.Policy().SetEpsilon(0.0f); // purement gloutonne (learn=false suffit deja a l'agent, ceinture+bretelles)

	const int evalN = 200;
	while ((int)ac.episodesCompleted < evalN) {
		agentSystem.Execute(world, 0.0f);
	}

	const uint32 reached = ac.episodesReachedGoal;
	const double successRate = (double)reached / (double)evalN;
	logger.Info("  evaluation gloutonne (via NkAgentSystem sur entite ECS) : {0}/{1} episodes atteignent le but ({2}%)",
				reached, evalN, successRate * 100.0);

	// -- Preuve que la MEMOIRE (le passe) de l'agent a bien enregistre le vecu ---------
	logger.Info("-- Memoire de l'agent : {0}/{1} transitions retenues (buffer borne) --", ag.Memory().Size(),
				ag.Memory().Capacity());

	// -- Affiche la politique apprise (identique a NKAgentTest) ------------------------
	logger.Info("-- Politique apprise (S=depart G=but O=trou) --");
	for (uint32 y = 0; y < N; ++y)
		logger.Info("  {0}", PolicyRow(ag, env, y).CStr());

	const bool ok = successRate >= 0.95;
	logger.Info("[ {0} ] l'agent, pilote par NkAgentSystem sur une entite ECS Noge, resout le monde-grille (>=95%)",
				ok ? "OK" : "KO");
	logger.Info("=== Resultat : {0} OK, {1} echec(s) ===", ok ? 1 : 0, ok ? 0 : 1);
	return ok ? 0 : 1;
}
