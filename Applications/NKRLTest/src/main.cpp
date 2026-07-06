// =============================================================================
// NKRLTest — un agent apprend SEUL à résoudre un monde-grille (NKRL, Phase 4).
//   Grille 5x5 avec des trous. Q-learning tabulaire + ε-greedy décroissant.
//   On entraîne, puis on évalue la politique gloutonne (doit atteindre le but
//   ~100% du temps) et on affiche la politique apprise. Jalon « ça vit ».
// =============================================================================
#include "NKRL/NkRL.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::ai;

int main() {
    printf("=== NKRLTest : un agent apprend à traverser un monde-grille ===\n\n");

    // Grille 5x5, départ en haut-gauche (0), but en bas-droite (24), 3 trous.
    const uint32 N = 5, START = 0, GOAL = 24;
    NkVector<uint32> holes; holes.PushBack(6); holes.PushBack(12); holes.PushBack(18);
    rl::NkGridWorld env(N, START, GOAL, holes, /*stepCost*/ 0.02f);

    rl::NkQLearning agent(env.NumStates(), env.NumActions(),
                          /*alpha*/ 0.1f, /*gamma*/ 0.99f, /*epsilon*/ 1.0f, /*seed*/ 42u);

    const int   episodes = 4000;
    const uint32 maxSteps = 100;

    printf("-- Entraînement (%d épisodes, ε décroissant) --\n", episodes);
    int winReach = 0; double winReward = 0.0;
    for (int e = 1; e <= episodes; ++e) {
        // ε : 1.0 -> 0.05 sur les 80 premiers % des épisodes (puis exploite).
        float eps = 1.0f - (float)e / (float)(episodes * 0.8);
        if (eps < 0.05f) eps = 0.05f;
        agent.SetEpsilon(eps);

        rl::EpisodeResult r = rl::RunEpisode(env, agent, /*learn*/ true, maxSteps);
        winReach += r.reachedGoal ? 1 : 0;
        winReward += r.reward;

        if (e % 800 == 0) {
            printf("  épisode %4d : succès(800 derniers) = %5.1f%%  récompense moy = %+.3f  ε=%.2f\n",
                   e, (double)winReach / 8.0, winReward / 800.0, eps);
            winReach = 0; winReward = 0.0;
        }
    }

    // Évaluation : politique gloutonne pure (aucune exploration, aucun apprentissage).
    const int evalN = 200;
    int reached = 0;
    for (int e = 0; e < evalN; ++e) {
        rl::EpisodeResult r = rl::RunEpisode(env, agent, /*learn*/ false, maxSteps);
        reached += r.reachedGoal ? 1 : 0;
    }
    const double successRate = (double)reached / (double)evalN;
    printf("\n  évaluation gloutonne : %d/%d épisodes atteignent le but (%.1f%%)\n",
           reached, evalN, successRate * 100.0);

    // Affiche la politique apprise (flèche = action gloutonne par case).
    printf("\n  politique apprise (S=départ G=but O=trou) :\n");
    const char* arrow[4] = { "^", "v", "<", ">" };
    for (uint32 y = 0; y < N; ++y) {
        printf("    ");
        for (uint32 x = 0; x < N; ++x) {
            uint32 s = y * N + x;
            if (s == GOAL)            printf(" G");
            else if (env.IsHole(s))   printf(" O");
            else if (s == START)      printf(" %s", arrow[agent.SelectGreedy(s)]);
            else                      printf(" %s", arrow[agent.SelectGreedy(s)]);
        }
        printf("\n");
    }

    const bool ok = successRate >= 0.95;
    printf("\n  [ %s ] l'agent a appris à résoudre le monde-grille (>=95%%)\n",
           ok ? "OK" : "KO");
    printf("\n=== Résultat : %d OK, %d échec(s) ===\n", ok ? 1 : 0, ok ? 0 : 1);
    return ok ? 0 : 1;
}
