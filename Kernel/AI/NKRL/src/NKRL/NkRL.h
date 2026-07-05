// =============================================================================
// NkRL.h — apprentissage par renforcement (NKAI, Phase 4) — header PARAPLUIE.
//
// Regroupe les concepts du module :
//   • NkEnv.h        — interface d'environnement
//   • NkGridWorld.h  — monde-grille jouet
//   • NkQLearning.h  — agent Q-learning tabulaire
// + le helper RunEpisode (déroule un épisode, avec ou sans apprentissage).
// Namespace : nkentseu::ai::rl.
// =============================================================================
#pragma once

#include "NKRL/NkEnv.h"
#include "NKRL/NkGridWorld.h"
#include "NKRL/NkQLearning.h"

namespace nkentseu {
    namespace ai {
        namespace rl {

            struct EpisodeResult {
                float  reward       = 0.0f;   // récompense cumulée
                bool   reachedGoal  = false;  // le but a-t-il été atteint ?
                uint32 steps        = 0;
            };

            // Déroule un épisode complet. `learn=true` : ε-greedy + mise à jour Q ;
            // `learn=false` : politique gloutonne, sans apprentissage (évaluation).
            inline EpisodeResult RunEpisode(NkEnvironment& env, NkQLearning& agent,
                                            bool learn, uint32 maxSteps) {
                EpisodeResult res;
                uint32 s = env.Reset();
                bool done = false;
                float lastR = 0.0f;
                for (; res.steps < maxSteps && !done; ++res.steps) {
                    uint32 a = learn ? agent.SelectAction(s) : agent.SelectGreedy(s);
                    uint32 sNext; float r;
                    env.Step(a, sNext, r, done);
                    if (learn) agent.Update(s, a, r, sNext, done);
                    res.reward += r;
                    lastR = r;
                    s = sNext;
                }
                res.reachedGoal = done && (lastR > 0.5f);   // récompense +1 = but
                return res;
            }

        } // namespace rl
    } // namespace ai
} // namespace nkentseu
