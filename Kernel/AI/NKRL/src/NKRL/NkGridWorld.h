// =============================================================================
// NkGridWorld.h — monde-grille jouet (NKAI, Phase 4).
//
// Grille N×N. L'agent part d'une case de départ et doit atteindre le BUT en
// évitant les TROUS. Actions : 0=haut 1=bas 2=gauche 3=droite. Récompense :
// +1 au but (fin), -1 dans un trou (fin), petit coût par pas (incite aux
// chemins courts). Sortir de la grille = rester sur place. État = y*N + x.
// =============================================================================
#pragma once

#include "NKRL/NkEnv.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace ai {
        namespace rl {

            class NkGridWorld : public NkEnvironment {
            public:
                // `size` = côté de la grille. Trous fournis en indices d'état (y*size+x).
                NkGridWorld(uint32 size, uint32 startState, uint32 goalState,
                            const NkVector<uint32>& holes, float stepCost = 0.02f);

                uint32 NumStates()  const override { return mSize * mSize; }
                uint32 NumActions() const override { return 4; }

                uint32 Reset() override;
                void   Step(uint32 action, uint32& nextState, float& reward, bool& done) override;

                uint32 Size()      const { return mSize; }
                uint32 GoalState() const { return mGoal; }
                bool   IsHole(uint32 state) const;

            private:
                uint32          mSize;
                uint32          mStart;
                uint32          mGoal;
                uint32          mCur;
                float           mStepCost;
                NkVector<uint32> mHoles;
            };

        } // namespace rl
    } // namespace ai
} // namespace nkentseu
