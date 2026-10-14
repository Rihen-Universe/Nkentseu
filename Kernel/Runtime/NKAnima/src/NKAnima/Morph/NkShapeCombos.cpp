// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Morph/NkShapeCombos.cpp : les formes de combinaison et les intermediaires
// (voir NkShapeCombos.h).
// =============================================================================
#include "NKAnima/Morph/NkShapeCombos.h"

namespace nkentseu {
	namespace anim {

		namespace {
			uint32 gShapeComboMutations = 0u;
			bool Mute(uint32 bit) {
				return (gShapeComboMutations & bit) != 0u;
			}
			bool Indice(int32 k, uint32 count) {
				return k >= 1 && (uint32)k < count;
			}
			bool GroupeValide(const NkShapeInBetween &g, uint32 count) {
				if (!Indice(g.primary, count) || g.shapes.Empty() || g.shapes.Size() != g.at.Size()) {
					return false;
				}
				float32 avant = 0.f;
				for (usize i = 0; i < g.shapes.Size(); ++i) {
					if (!Indice(g.shapes[i], count) || g.shapes[i] == g.primary || !(g.at[i] > avant) || !(g.at[i] < 1.f)) {
						return false;
					}
					avant = g.at[i];
				}
				return true;
			}
			bool CombinaisonValide(const NkShapeCombination &c, uint32 count) {
				if (!Indice(c.shape, count) || c.drivers.Empty()) {
					return false;
				}
				for (usize i = 0; i < c.drivers.Size(); ++i) {
					if (!Indice(c.drivers[i], count) || c.drivers[i] == c.shape) {
						return false;
					}
				}
				return true;
			}
		} // namespace

		void NkShapeComboSetMutations(uint32 bits) noexcept {
			gShapeComboMutations = bits;
		}
		uint32 NkShapeComboMutations() noexcept {
			return gShapeComboMutations;
		}

		float32 NkShapeRampEval(const NkShapeRamp &ramp, float32 in) noexcept {
			const float32 d = ramp.inMax - ramp.inMin;
			float32 u = 0.f;
			if (d > 1e-12f || d < -1e-12f) {
				u = (in - ramp.inMin) / d;
			} else {
				u = in >= ramp.inMax ? 1.f : 0.f; // une rampe sans largeur : une marche
			}
			u = u < 0.f ? 0.f : (u > 1.f ? 1.f : u);
			if (ramp.smooth && !Mute(NK_SC_MUT_RAMP_LINEAR)) {
				u = u * u * (3.f - 2.f * u);
			}
			return u;
		}

		bool NkShapeCombos::Validate(uint32 count) const {
			// Chaque forme a au plus UN maitre : une seule course (pleine ou intermediaire
			// d'un seul groupe) ou une seule combinaison ; sinon le dernier ecrit gagne en
			// silence.
			NkVector<uint8> pris;
			pris.Resize(count, 0u);
			auto Prendre = [&](int32 k) {
				// MUTATION combo_two_masters : une forme deja menee est acceptee.
				if (!Indice(k, count) || (pris[(uint32)k] != 0u && !Mute(NK_SC_MUT_TWO_MASTERS))) {
					return false;
				}
				pris[(uint32)k] = 1u;
				return true;
			};
			for (usize i = 0; i < inBetweens.Size(); ++i) {
				const NkShapeInBetween &g = inBetweens[i];
				if (!GroupeValide(g, count) || !Prendre(g.primary)) {
					return false;
				}
				for (usize j = 0; j < g.shapes.Size(); ++j) {
					if (!Prendre(g.shapes[j])) {
						return false;
					}
				}
			}
			for (usize i = 0; i < combinations.Size(); ++i) {
				if (!CombinaisonValide(combinations[i], count) || !Prendre(combinations[i].shape)) {
					return false;
				}
			}
			return true;
		}

		void NkShapeCombos::Resolve(const float32 *sliders, float32 *weights, uint32 count) const {
			if (sliders == nullptr || weights == nullptr) {
				return;
			}
			for (uint32 k = 0; k < count; ++k) {
				weights[k] = sliders[k];
			}
			// Les intermediaires : la course du curseur de la forme pleine, station par
			// station (la base a 0, les intermediaires, la forme pleine a 1). Au-dela des
			// bornes, le premier ou le dernier segment se prolonge.  MUTATION combo_no_inbetween.
			if (!Mute(NK_SC_MUT_NO_INBETWEEN)) {
				for (usize gi = 0; gi < inBetweens.Size(); ++gi) {
					const NkShapeInBetween &g = inBetweens[gi];
					if (!GroupeValide(g, count)) {
						continue;
					}
					const uint32 n = (uint32)g.shapes.Size();
					const float32 v = sliders[g.primary];
					weights[g.primary] = 0.f;
					for (uint32 i = 0; i < n; ++i) {
						weights[g.shapes[i]] = 0.f;
					}
					// La station i : 0 = la base, 1..n = les intermediaires, n+1 = la forme pleine.
					auto Position = [&](uint32 i) { return i == 0 ? 0.f : (i <= n ? g.at[i - 1] : 1.f); };
					auto Forme = [&](uint32 i) { return i == 0 ? -1 : (i <= n ? g.shapes[i - 1] : g.primary); };
					uint32 s = 0;
					while (s < n && v > Position(s + 1)) {
						++s;
					}
					const float32 p0 = Position(s), p1 = Position(s + 1);
					const float32 u = (v - p0) / (p1 - p0);
					if (Forme(s) >= 0) {
						weights[Forme(s)] += 1.f - u;
					}
					weights[Forme(s + 1)] += u;
				}
			}
			// Les combinaisons : lisent les CURSEURS.  MUTATION combo_off.
			if (!Mute(NK_SC_MUT_COMBO_OFF)) {
				for (usize ci = 0; ci < combinations.Size(); ++ci) {
					const NkShapeCombination &c = combinations[ci];
					if (!CombinaisonValide(c, count)) {
						continue;
					}
					float32 r = 1.f; // le neutre du produit comme du minimum (les rampes rendent 0..1)
					for (usize i = 0; i < c.drivers.Size(); ++i) {
						const float32 x = NkShapeRampEval(c.ramp, sliders[c.drivers[i]]);
						if (c.mode == NkShapeComboMode::NK_MIN) {
							r = x < r ? x : r;
						} else {
							r *= x;
						}
					}
					weights[c.shape] = r;
				}
			}
		}

	} // namespace anim
} // namespace nkentseu
