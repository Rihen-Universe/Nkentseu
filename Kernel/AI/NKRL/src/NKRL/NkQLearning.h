// =============================================================================
// NkQLearning.h — agent Q-learning tabulaire (NKAI, Phase 4).
//
// Apprend une table Q[état, action] par différence temporelle :
//   Q(s,a) ← Q(s,a) + α · ( r + γ·max_a' Q(s',a') − Q(s,a) )
// Exploration ε-greedy. RNG déterministe (LCG). Namespace : nkentseu::ai::rl.
// =============================================================================
#pragma once

#include "NKRL/NkEnv.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace ai {
        namespace rl {

            class NkQLearning {
            public:
                NkQLearning(uint32 numStates, uint32 numActions,
                            float alpha = 0.1f, float gamma = 0.99f,
                            float epsilon = 0.1f, uint32 seed = 1u);

                // Action ε-greedy (exploration) pour l'entraînement.
                uint32 SelectAction(uint32 state);
                // Action gloutonne (argmax) pour l'évaluation / exploitation.
                uint32 SelectGreedy(uint32 state) const;

                // Mise à jour TD après une transition (s,a,r,s',done).
                void Update(uint32 s, uint32 a, float r, uint32 sNext, bool done);

                float  Q(uint32 s, uint32 a) const { return mQ[s * mNumActions + a]; }
                void   SetEpsilon(float e) { mEpsilon = e; }
                float  Epsilon() const { return mEpsilon; }

            private:
                float  Rand01();                     // [0,1) déterministe

                NkVector<float> mQ;                  // table [numStates * numActions]
                uint32 mNumStates;
                uint32 mNumActions;
                float  mAlpha;
                float  mGamma;
                float  mEpsilon;
                uint32 mRng;
            };

        } // namespace rl
    } // namespace ai
} // namespace nkentseu
