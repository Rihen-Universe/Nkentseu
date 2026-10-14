// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Clip/NkClipEvents.cpp : les evenements d'un clip (voir NkClipEvents.h).
// =============================================================================
#include "NKAnima/Clip/NkClipEvents.h"

#include <cmath>

namespace nkentseu {
	namespace anim {

		namespace {
			uint32 gClipEventMutations = 0u;
			bool Mute(uint32 bit) {
				return (gClipEventMutations & bit) != 0u;
			}
			constexpr float64 kMaxCycles = 100000.0; // au-dela, un saut de temps absurde : on s'arrete

			/// Dans [lo, hi] avec les bornes ouvertes ou fermees.
			bool Dedans(float64 t, float64 lo, bool loInc, float64 hi, bool hiInc) {
				const bool apresLo = loInc ? t >= lo : t > lo;
				const bool avantHi = hiInc ? t <= hi : t < hi;
				return apresLo && avantHi;
			}
		} // namespace

		void NkClipEventSetMutations(uint32 bits) noexcept {
			gClipEventMutations = bits;
		}
		uint32 NkClipEventMutations() noexcept {
			return gClipEventMutations;
		}

		void NkClipEventTrack::Add(float32 time, const NkString &name) {
			NkClipEvent e;
			e.time = time;
			e.name = name;
			usize i = 0;
			while (i < events.Size() && events[i].time <= time) {
				++i;
			}
			events.Insert(events.Begin() + i, e);
		}

		uint32 NkClipEventTrack::Collect(float32 duration, bool loop, float32 elapsed0, float32 elapsed1, NkVector<uint32> &out,
										 bool includeStart) const {
			const usize debut = out.Size();
			if (events.Empty()) {
				return 0u;
			}
			const bool avant = elapsed1 >= elapsed0;
			// L'intervalle parcouru, du plus tot au plus tard, et ses bornes.
			//   en avant : ]e0, e1]  (e0 compris si includeStart) ;
			//   a rebours : [e1, e0[ (e0 compris si includeStart), rendu du plus tard au plus tot.
			// MUTATION clip_events_closed : fermees des deux cotes.
			const bool ferme = Mute(NK_EV_MUT_CLOSED);
			const float64 lo = avant ? elapsed0 : elapsed1, hi = avant ? elapsed1 : elapsed0;
			const bool loInc = ferme || (avant ? includeStart : true);
			const bool hiInc = ferme || (avant ? true : includeStart);
			const float64 D = duration;
			const uint32 n = (uint32)events.Size();

			if (!loop || D <= 1e-9) {
				for (uint32 i = 0; i < n; ++i) {
					const float64 t = events[i].time;
					if ((D <= 1e-9 || (t >= 0.0 && t <= D)) && Dedans(t, lo, loInc, hi, hiInc)) {
						out.PushBack(i);
					}
				}
			} else if (Mute(NK_EV_MUT_NO_LOOP)) {
				// MUTATION clip_events_no_loop : les deux temps replies, un seul cycle.
				const float64 a = std::fmod(lo, D) < 0.0 ? std::fmod(lo, D) + D : std::fmod(lo, D);
				const float64 b = std::fmod(hi, D) < 0.0 ? std::fmod(hi, D) + D : std::fmod(hi, D);
				for (uint32 i = 0; i < n; ++i) {
					const float64 t = std::fmod((float64)events[i].time, D);
					if (Dedans(t, a, loInc, b, hiInc)) {
						out.PushBack(i);
					}
				}
			} else {
				const float64 c0 = std::floor(lo / D), c1 = std::floor(hi / D);
				// Le cas courant : tous les temps dans [0, D[ (deja dans l'ordre d'un cycle) :
				// aucune allocation, c'est l'appel de chaque image.
				bool dansLeCycle = true;
				for (uint32 i = 0; i < n && dansLeCycle; ++i) {
					dansLeCycle = events[i].time >= 0.f && (float64)events[i].time < D;
				}
				if (dansLeCycle) {
					for (float64 c = c0; c <= c1 && c1 - c0 <= kMaxCycles; c += 1.0) {
						for (uint32 i = 0; i < n; ++i) {
							if (Dedans(c * D + (float64)events[i].time, lo, loInc, hi, hiInc)) {
								out.PushBack(i);
							}
						}
					}
				} else {
					// L'ordre dans un cycle : par temps replie (un evenement pose a D est a 0).
					NkVector<uint32> ordre;
					NkVector<float64> replie;
					ordre.Resize(n);
					replie.Resize(n);
					for (uint32 i = 0; i < n; ++i) {
						float64 t = std::fmod((float64)events[i].time, D);
						replie[i] = t < 0.0 ? t + D : t;
						uint32 k = i;
						while (k > 0 && replie[ordre[k - 1]] > replie[i]) {
							ordre[k] = ordre[k - 1];
							--k;
						}
						ordre[k] = i;
					}
					for (float64 c = c0; c <= c1 && c1 - c0 <= kMaxCycles; c += 1.0) {
						for (uint32 j = 0; j < n; ++j) {
							const uint32 i = ordre[j];
							if (Dedans(c * D + replie[i], lo, loInc, hi, hiInc)) {
								out.PushBack(i);
							}
						}
					}
				}
			}
			// A rebours : du plus tard au plus tot.
			if (!avant) {
				usize a = debut, b = out.Size();
				while (b > a + 1) {
					--b;
					const uint32 tmp = out[a];
					out[a] = out[b];
					out[b] = tmp;
					++a;
				}
			}
			return (uint32)(out.Size() - debut);
		}

	} // namespace anim
} // namespace nkentseu
