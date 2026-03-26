#pragma once
/**
 * @File    NkPAAgent.h
 * @Brief   Agent de déplacement autonome (Seek + ondulation sinusoïdale).
 *
 * Chaque créature possède un Agent qui :
 *   - Se dirige vers une cible aléatoire
 *   - Applique une ondulation latérale (corps serpentant)
 *   - Rebondit sur les bords du monde
 */

#include "NKCore/NkTypes.h"
#include "NKMath/NkVec.h"

namespace nkentseu {
namespace nkpa {

using namespace math;

class Agent {
public:
    /// @param startPos  Position initiale
    /// @param speed     Vitesse en pixels/seconde
    /// @param undulAmp  Amplitude d'ondulation latérale
    /// @param undulFreq Fréquence d'ondulation (Hz)
    /// @param worldW    Largeur du monde (pixels)
    /// @param worldH    Hauteur du monde (pixels)
    void Init(NkVector2f startPos,
              float32 speed,
              float32 undulAmp,
              float32 undulFreq,
              int32 worldW,
              int32 worldH);

    /// Met à jour l'agent pour dt secondes et retourne la nouvelle position de la tête.
    NkVector2f Update(float32 dt);

    NkVector2f Pos()     const { return mPos; }
    NkVector2f Vel()     const { return mVel; }
    float32    HeadAng() const { return mHeadAngle; }

private:
    NkVector2f mPos;
    NkVector2f mVel;
    NkVector2f mTarget;

    float32 mSpeed       = 120.f;
    float32 mUndulAmp    = 0.f;
    float32 mUndulFreq   = 2.5f;
    float32 mUndulPhase  = 0.f;
    float32 mChangeTimer = 0.f;
    float32 mChangeDelay = 3.f;
    float32 mHeadAngle   = 0.f;

    int32 mWorldW = 1280;
    int32 mWorldH = 720;

    void PickNewTarget();
};

} // namespace nkpa
} // namespace nkentseu
