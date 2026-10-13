// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkRtEase.cpp — les lois de temps d'un mouvement fabrique (voir NkRtEase.h).
// =============================================================================
#include "NKAnima/Realtime/NkRtEase.h"
#include "NKAnima/Realtime/NkRtRig.h" // les mutations des bancs

namespace nkentseu {
	namespace anim {
		namespace rt {

			float32 NkRtMinJerk(float32 u) noexcept {
				u = u < 0.f ? 0.f : (u > 1.f ? 1.f : u);
				// MUTATION ease_linear : une rampe droite (la vitesse saute aux deux bouts).
				if (NkRtMutated(NK_RT_MUT_EASE_LINEAR)) {
					return u;
				}
				return u * u * u * (10.f + u * (-15.f + 6.f * u));
			}

			float32 NkRtEnvelopeWeight(const NkRtEnvelope &e, float32 t) noexcept {
				if (t <= 0.f) {
					return 0.f;
				}
				if (t < e.attack) {
					return NkRtMinJerk(t / e.attack);
				}
				if (t < e.attack + e.hold) {
					return 1.f;
				}
				if (e.release > 0.f && t < e.attack + e.hold + e.release) {
					return 1.f - NkRtMinJerk((t - e.attack - e.hold) / e.release);
				}
				return 0.f;
			}

			float32 NkRtWindow(float32 t, float32 start, float32 end, float32 rise, float32 fall) noexcept {
				if (t <= start || t >= end) {
					return 0.f;
				}
				const float32 a = (t - start) / rise, b = (end - t) / fall;
				const float32 w = a < b ? a : b;
				return w > 1.f ? 1.f : NkRtMinJerk(w);
			}

			float32 NkRtHermite(float32 u, float32 p0, float32 m0, float32 p1, float32 m1) noexcept {
				const float32 h00 = 2.f * u * u * u - 3.f * u * u + 1.f, h10 = u * u * u - 2.f * u * u + u;
				const float32 h01 = -2.f * u * u * u + 3.f * u * u, h11 = u * u * u - u * u;
				return p0 * h00 + m0 * h10 + p1 * h01 + m1 * h11;
			}

			float32 NkRtBallisticHeight(float32 v0, float32 g, float32 t) noexcept {
				return v0 * t - 0.5f * g * t * t;
			}

			float32 NkRtBallisticSpeed(float32 height, float32 g) noexcept {
				const float32 v2 = 2.f * g * height;
				return v2 > 0.f ? std::sqrt(v2) : 0.f;
			}

			float32 NkRtBallisticTime(float32 v0, float32 g) noexcept {
				return g > 0.f ? 2.f * v0 / g : 0.f;
			}

			float32 NkRtPulse(float32 u, float32 rise, float32 fall) noexcept {
				if (u <= 0.f) {
					return 0.f;
				}
				if (u < rise) {
					return u / rise;
				}
				const float32 d = (u - rise) / fall;
				return d >= 1.f ? 0.f : (1.f - d) * (1.f - d);
			}

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
