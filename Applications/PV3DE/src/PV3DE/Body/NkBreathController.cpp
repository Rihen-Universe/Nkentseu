#include "NkBreathController.h"
#include "NKMath/NKMath.h"
#include <stdlib.h>

namespace nkentseu {
    namespace pv3de {

        void NkBreathController::_Reset() noexcept {
            mCurrent  = NkBreathParams{};
            mTarget   = mCurrent;
            mPhase    = 0.f;
            mChest    = 0.f;
            mAbdomen  = 0.f;
            mNasalFlare = 0.f;
            mAccessory  = 0.f;
            mInApnea  = false;
        }

        // =====================================================================
        void NkBreathController::SetPattern(NkBreathPattern p) noexcept {
            mTarget.pattern = p;
            switch (p) {
                case NkBreathPattern::Normal:
                    mTarget.rate = 16.f; mTarget.amplitude = 0.3f; mTarget.ratio = 0.4f; break;
                case NkBreathPattern::Dyspnea:
                    mTarget.rate = 28.f; mTarget.amplitude = 0.15f; mTarget.ratio = 0.45f; break;
                case NkBreathPattern::Bradypnea:
                    mTarget.rate = 8.f;  mTarget.amplitude = 0.4f;  mTarget.ratio = 0.35f; break;
                case NkBreathPattern::Hyperpnea:
                    mTarget.rate = 20.f; mTarget.amplitude = 0.85f; mTarget.ratio = 0.4f; break;
                case NkBreathPattern::CheyneStokes:
                    mTarget.rate = 18.f; mTarget.amplitude = 0.5f;  mTarget.ratio = 0.4f; break;
                case NkBreathPattern::Kussmaul:
                    mTarget.rate = 20.f; mTarget.amplitude = 0.9f;  mTarget.ratio = 0.5f; break;
                case NkBreathPattern::Biot:
                    mTarget.rate = 12.f; mTarget.amplitude = 0.5f;  mTarget.ratio = 0.4f; break;
                case NkBreathPattern::Paradoxical:
                    mTarget.rate = 30.f; mTarget.amplitude = 0.2f;  mTarget.ratio = 0.5f; break;
            }
        }

        void NkBreathController::SetRate(nk_float32 r) noexcept {
            mTarget.rate = NkClamp(r, 2.f, 60.f);
        }

        void NkBreathController::SetAmplitude(nk_float32 a) noexcept {
            mTarget.amplitude = NkClamp(a, 0.f, 1.f);
        }

        void NkBreathController::SetFromBreathingDifficulty(nk_float32 d) noexcept {
            d = NkClamp(d, 0.f, 1.f);
            if      (d < 0.1f) SetPattern(NkBreathPattern::Normal);
            else if (d < 0.4f) SetPattern(NkBreathPattern::Dyspnea);
            else if (d < 0.7f) { SetPattern(NkBreathPattern::Dyspnea); mTarget.rate = 35.f; }
            else                SetPattern(NkBreathPattern::Paradoxical);
        }

        // =====================================================================
        void NkBreathController::Update(nk_float32 dt) noexcept {
            // Interpolation douce des paramètres
            nk_float32 s = NkMin(dt * 1.2f, 1.f);
            mCurrent.rate      = NkLerp(mCurrent.rate,      mTarget.rate,      s);
            mCurrent.amplitude = NkLerp(mCurrent.amplitude, mTarget.amplitude, s);
            mCurrent.ratio     = NkLerp(mCurrent.ratio,     mTarget.ratio,     s);
            mCurrent.pattern   = mTarget.pattern;

            switch (mCurrent.pattern) {
                case NkBreathPattern::CheyneStokes: _UpdateCheyneStokes(dt); break;
                case NkBreathPattern::Biot:         _UpdateBiot(dt);         break;
                case NkBreathPattern::Paradoxical:  _UpdateParadoxical(dt);  break;
                default:                            _UpdateNormal(dt);       break;
            }
            _Solve();
        }

        // ── Patterns ──────────────────────────────────────────────────────────

        void NkBreathController::_UpdateNormal(nk_float32 dt) noexcept {
            mInApnea = false;
            nk_float32 freq = mCurrent.rate / 60.f;
            mPhase += NkTwoPi * freq * dt;
            if (mPhase > NkTwoPi) mPhase -= NkTwoPi;
        }

        void NkBreathController::_UpdateCheyneStokes(nk_float32 dt) noexcept {
            // Enveloppe cyclique 60s : 20s apnée + 40s crescendo-décrescendo
            static constexpr nk_float32 kCycleLen = 60.f;
            static constexpr nk_float32 kApneaLen = 20.f;

            mCSEnvPhase += dt;
            if (mCSEnvPhase >= kCycleLen) mCSEnvPhase -= kCycleLen;

            if (mCSEnvPhase < kApneaLen) {
                // Phase apnée
                mInApnea  = true;
                mCSEnvelope = 0.f;
                mPhase    = NkPiHalf; // tenir la position fermée
            } else {
                mInApnea = false;
                nk_float32 t = (mCSEnvPhase - kApneaLen) / (kCycleLen - kApneaLen);
                // Crescendo jusqu'à 0.5, décrescendo de 0.5 à 1.0
                mCSEnvelope = (t < 0.5f)
                    ? NkSin(t * NkPi)
                    : NkSin((1.f - t) * NkPi);

                nk_float32 freq = mCurrent.rate / 60.f;
                mPhase += NkTwoPi * freq * dt;
                if (mPhase > NkTwoPi) mPhase -= NkTwoPi;
            }
        }

        void NkBreathController::_UpdateBiot(nk_float32 dt) noexcept {
            mBiotTimer += dt;
            if (mBiotTimer >= mBiotInterval) {
                mBiotTimer = 0.f;
                // Apnée aléatoire 1-3s
                if (!mInApnea) {
                    mInApnea = true;
                    mBiotInterval = 1.f + (nk_float32)(rand() % 200) * 0.01f;
                } else {
                    mInApnea = false;
                    mBiotInterval = 2.f + (nk_float32)(rand() % 300) * 0.01f;
                }
            }
            if (!mInApnea) {
                nk_float32 freq = mCurrent.rate / 60.f;
                mPhase += NkTwoPi * freq * dt;
                if (mPhase > NkTwoPi) mPhase -= NkTwoPi;
            }
        }

        void NkBreathController::_UpdateParadoxical(nk_float32 dt) noexcept {
            // Thorax et abdomen en opposition de phase
            mInApnea = false;
            nk_float32 freq = mCurrent.rate / 60.f;
            mPhase += NkTwoPi * freq * dt;
            if (mPhase > NkTwoPi) mPhase -= NkTwoPi;
        }

        // ── Résolution des sorties ────────────────────────────────────────────
        void NkBreathController::_Solve() noexcept {
            if (mInApnea) {
                mChest = mAbdomen = mNasalFlare = 0.f;
                mAccessory = 0.f;
                return;
            }

            nk_float32 amp = mCurrent.amplitude;

            // Forme d'onde : fondamental + harmoniques pour l'organicité
            nk_float32 wave = NkSin(mPhase) * 0.65f
                            + NkSin(mPhase * 2.f + 0.3f) * 0.25f
                            + NkSin(mPhase * 3.f + 0.7f) * 0.10f;

            if (mCurrent.pattern == NkBreathPattern::CheyneStokes) {
                wave *= mCSEnvelope;
                amp  *= mCSEnvelope;
            }

            if (mCurrent.pattern == NkBreathPattern::Paradoxical) {
                // Opposition thorax/abdomen
                mChest   =  wave * amp;
                mAbdomen = -wave * amp * 0.8f;
            } else {
                mChest   = wave * amp;
                mAbdomen = wave * amp * 0.55f;
            }

            // Battement des ailes du nez : proportionnel à la fréquence et détresse
            nk_float32 rateNorm = NkClamp((mCurrent.rate - 12.f) / 28.f, 0.f, 1.f);
            mNasalFlare = rateNorm * NkAbs(wave) * 0.8f;

            // Muscles accessoires : détresse avancée seulement
            mAccessory = NkClamp(rateNorm * 1.4f - 0.4f, 0.f, 1.f) * NkAbs(wave);

            // Kussmaul — exagérer l'amplitude
            if (mCurrent.pattern == NkBreathPattern::Kussmaul) {
                mChest   *= 1.5f;
                mAbdomen *= 1.5f;
            }
        }

    } // namespace pv3de
} // namespace nkentseu
