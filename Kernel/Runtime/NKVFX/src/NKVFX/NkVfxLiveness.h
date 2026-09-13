#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKVFX/NkVfxLiveness.h — LE CONTRÔLE DE VIE : « ça s'exécute », pas seulement
// « ça s'enregistre » (2026-09-12).
//
// ── LE DÉFAUT MAISON, ET LE SEUL TÉMOIN QUI LE SÉPARE ─────────────────────────
// Huit fois dans ce dépôt, un paramètre DÉCLARÉ n'était pas HONORÉ : un système
// enregistré que rien n'exécutait, une valeur écrite que personne ne lisait. Un
// compteur d'appels ne le voit pas — il compte des intentions. Un seul témoin
// sépare les deux :
//
//     SI LE TEMPS PAR IMAGE NE BOUGE PAS QUAND ON DOUBLE LA RÉSOLUTION,
//     LE SYSTÈME NE TOURNE PAS.
//
// Cet en-tête mesure exactement cela, pour N'IMPORTE QUEL hôte — le producteur
// nu dans un banc, ou un monde ECS entier dans une sonde. Il ne connaît pas ce
// qu'il chronomètre : on lui donne un « tick » opaque et une résolution.
//
// ── CE QU'IL REND, ET CE QU'IL NE REND PAS ────────────────────────────────────
// Un RAPPORT (temps à haute résolution / temps à basse résolution), jamais un
// seuil absolu en millisecondes : une machine chargée ou un Debug lent
// déplacent les deux mesures ensemble, pas leur rapport. Les deux résolutions
// sont ALTERNÉES à chaque échantillon (A, B, A, B...) pour que la dérive de
// fréquence du processeur frappe les deux de la même manière ; et c'est la
// MÉDIANE qui est retenue, pas la moyenne — une interruption du système
// d'exploitation ne fait pas mentir une médiane.
//
// Pour une grille de (cols+1)(rows+1) sommets, doubler cols ET rows quadruple
// le travail : un tick vivant rend un rapport proche de 4 ; un tick qui
// n'enregistre que le paramètre rend un rapport proche de 1. Le seuil de
// jugement appartient à l'appelant, qui connaît sa complexité.
// =============================================================================
#include "NKTime/NkChrono.h"

namespace nkentseu {
	namespace vfx {

		// Un tick opaque : l'hôte fait UNE image (ou un appel) à la résolution
		// demandée. Il ne rend rien ; ce qui compte est le temps qu'il y passe.
		typedef void (*NkLivenessTick)(void *ctx, uint32 resolution);

		struct NkLivenessReport {
				uint32 resLow = 0, resHigh = 0;
				float64 usLow = 0.0;  // médiane, microsecondes, à resLow
				float64 usHigh = 0.0; // médiane, microsecondes, à resHigh
				float64 ratio = 0.0;  // usHigh / usLow ; 0 si usLow est nul (rien mesuré)
				uint32 samples = 0;
		};

		namespace detail {
			// Médiane sans STL : tri par insertion sur un petit tableau. Les
			// échantillons sont peu nombreux par construction (≤ 32).
			NK_FORCE_INLINE float64 NkMedian(float64 *v, uint32 n) noexcept {
				for (uint32 i = 1; i < n; ++i) {
					const float64 x = v[i];
					uint32 j = i;
					while (j > 0 && v[j - 1] > x) {
						v[j] = v[j - 1];
						--j;
					}
					v[j] = x;
				}
				if (n == 0)
					return 0.0;
				return (n & 1u) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
			}

			NK_FORCE_INLINE float64 NkTimeTicks(NkLivenessTick tick, void *ctx, uint32 res,
												uint32 count) noexcept {
				NkChrono chrono;
				for (uint32 k = 0; k < count; ++k)
					tick(ctx, res);
				return chrono.Elapsed().ToMicroseconds() / (float64)(count == 0u ? 1u : count);
			}
		} // namespace detail

		// Mesure `samples` échantillons alternés ; chaque échantillon chronomètre
		// `ticksPerSample` ticks consécutifs et en rend le temps moyen par tick.
		// Un tick de chauffe à chaque résolution précède la mesure — le premier
		// appel paie ses défauts de cache et ne doit pas peser.
		inline NkLivenessReport NkMeasureLiveness(NkLivenessTick tick, void *ctx, uint32 resLow,
												  uint32 resHigh, uint32 samples,
												  uint32 ticksPerSample) noexcept {
			NkLivenessReport r;
			r.resLow = resLow;
			r.resHigh = resHigh;
			if (tick == nullptr || samples == 0u)
				return r;
			if (samples > 32u)
				samples = 32u;
			if (ticksPerSample == 0u)
				ticksPerSample = 1u;

			tick(ctx, resLow);
			tick(ctx, resHigh);

			float64 low[32], high[32];
			for (uint32 s = 0; s < samples; ++s) {
				low[s] = detail::NkTimeTicks(tick, ctx, resLow, ticksPerSample);
				high[s] = detail::NkTimeTicks(tick, ctx, resHigh, ticksPerSample);
			}
			r.samples = samples;
			r.usLow = detail::NkMedian(low, samples);
			r.usHigh = detail::NkMedian(high, samples);
			r.ratio = (r.usLow > 0.0) ? (r.usHigh / r.usLow) : 0.0;
			return r;
		}

	} // namespace vfx
} // namespace nkentseu
