// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Motion/NkGaitCycle.cpp — un cycle de marche, analyse (voir NkGaitCycle.h).
// =============================================================================
#include "NKAnima/Motion/NkGaitCycle.h"
#include "NKAnima/Realtime/NkRtRig.h" // les mutations des bancs

#include <cmath>

namespace nkentseu {
	namespace anim {

		using math::NkVec3f;

		namespace {
			NkVec3f Plus(const NkVec3f &a, const NkVec3f &b) {
				return NkVec3f(a.x + b.x, a.y + b.y, a.z + b.z);
			}
			NkVec3f Moins(const NkVec3f &a, const NkVec3f &b) {
				return NkVec3f(a.x - b.x, a.y - b.y, a.z - b.z);
			}
			NkVec3f Fois(const NkVec3f &a, float32 s) {
				return NkVec3f(a.x * s, a.y * s, a.z * s);
			}
			float32 Longueur(const NkVec3f &a) {
				return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
			}
			/// La phase ramenee a [0 ; 1[.
			float32 Phase(float32 phase) {
				const float32 u = phase - std::floor(phase);
				return (u >= 0.f && u < 1.f) ? u : 0.f;
			}
			/// Une suite periodique de N images (la derniere vaut la premiere), lue a la phase `phase`.
			NkVec3f Lire(const NkVector<NkVec3f> &suite, float32 phase) {
				const usize n = suite.Size();
				if (n == 0) {
					return NkVec3f(0.f, 0.f, 0.f);
				}
				if (n == 1) {
					return suite[0];
				}
				const float32 x = Phase(phase) * (float32)(n - 1);
				usize k = (usize)x;
				if (k > n - 2) {
					k = n - 2;
				}
				const float32 t = x - (float32)k;
				return Plus(suite[k], Fois(Moins(suite[k + 1], suite[k]), t));
			}
		} // namespace

		float32 NkGaitCycle::StrideLength() const noexcept {
			return Longueur(stride);
		}

		float32 NkGaitCycle::Speed() const noexcept {
			return duration > 0.f ? Longueur(stride) / duration : 0.f;
		}

		bool NkAnalyseGait(const NkVector<NkVec3f> &root, const NkVector<NkVector<NkVec3f>> &feet, float32 duration, float32 plantSpeed,
						   NkGaitCycle &out) {
			out = NkGaitCycle{};
			const usize n = root.Size();
			if (n < 3 || duration <= 0.f) {
				return false;
			}
			for (usize i = 0; i < feet.Size(); ++i) {
				if (feet[i].Size() != n) {
					return false;
				}
			}
			out.duration = duration;
			out.origin = root[0];
			// LA FOULEE : la derniere image est la premiere, avancee d'une foulee.
			// MUTATION gait_no_stride : elle n'est pas retiree -- la racine et les pieds
			// gardent leur avance, le repere moyen ne bouge plus.
			out.stride = rt::NkRtMutated(rt::NK_RT_MUT_GAIT_NO_STRIDE) ? NkVec3f(0.f, 0.f, 0.f) : Moins(root[n - 1], root[0]);
			const float32 pas = 1.f / (float32)(n - 1);
			const float32 dt = duration * pas;
			auto Repere = [&](usize k) { return Plus(out.origin, Fois(out.stride, (float32)k * pas)); };
			out.residual.Resize(n);
			for (usize k = 0; k < n; ++k) {
				out.residual[k] = Moins(root[k], Repere(k));
			}
			// La foulee VRAIE ferme la boucle des pieds dans le monde, mutation ou pas :
			// c'est elle qui dit ou est l'image d'avant la premiere, et ce qu'est « lent ».
			const NkVec3f boucle = Moins(root[n - 1], root[0]);
			const float32 seuil = plantSpeed > 0.f ? plantSpeed : Longueur(boucle) / duration / 8.f;
			out.feet.Resize(feet.Size());
			for (usize i = 0; i < feet.Size(); ++i) {
				NkGaitFoot &f = out.feet[i];
				const NkVector<NkVec3f> &monde = feet[i];
				f.track.Resize(n);
				f.planted.Resize(n);
				f.floor = monde[0].y;
				for (usize k = 0; k < n; ++k) {
					f.track[k] = Moins(monde[k], Repere(k));
					f.floor = monde[k].y < f.floor ? monde[k].y : f.floor;
				}
				// Plante : la cheville ne bouge pas dans le MONDE (vitesse centree, la boucle
				// prolongee d'une foulee de part et d'autre).
				uint32 plantes = 0;
				for (usize k = 0; k < n; ++k) {
					const NkVec3f avant = k > 0 ? monde[k - 1] : Moins(monde[n - 2], boucle);
					const NkVec3f apres = k + 1 < n ? monde[k + 1] : Plus(monde[1], boucle);
					const float32 v = Longueur(Moins(apres, avant)) / (2.f * dt);
					f.planted[k] = v < seuil ? 1u : 0u;
					if (k + 1 < n && f.planted[k] != 0u) {
						++plantes;
					}
				}
				f.plantedShare = (float32)plantes * pas;
				for (usize k = 0; k + 1 < n; ++k) {
					if (f.planted[k] != 0u && f.planted[k + 1] != 0u) {
						const float32 d = Longueur(Moins(monde[k + 1], monde[k]));
						f.slip = d > f.slip ? d : f.slip;
					}
				}
			}
			return true;
		}

		NkVec3f NkGaitSample(const NkVector<NkVec3f> &track, float32 phase) noexcept {
			return Lire(track, phase);
		}

		NkVec3f NkGaitRootResidual(const NkGaitCycle &g, float32 phase) noexcept {
			return Lire(g.residual, phase);
		}

		NkVec3f NkGaitFootPoint(const NkGaitCycle &g, uint32 foot, float32 phase) noexcept {
			if (foot >= (uint32)g.feet.Size()) {
				return NkVec3f(0.f, 0.f, 0.f);
			}
			return Lire(g.feet[foot].track, phase);
		}

		float32 NkGaitFootLift(const NkGaitCycle &g, uint32 foot, float32 phase) noexcept {
			if (foot >= (uint32)g.feet.Size()) {
				return 0.f;
			}
			const float32 u = Phase(phase);
			const float32 y = Lire(g.feet[foot].track, phase).y + g.origin.y + g.stride.y * u;
			const float32 h = y - g.feet[foot].floor;
			return h > 0.f ? h : 0.f;
		}

		bool NkGaitFootPlanted(const NkGaitCycle &g, uint32 foot, float32 phase) noexcept {
			if (foot >= (uint32)g.feet.Size() || g.feet[foot].planted.Size() < 2) {
				return false;
			}
			const usize n = g.feet[foot].planted.Size();
			usize k = (usize)(Phase(phase) * (float32)(n - 1) + 0.5f);
			if (k > n - 1) {
				k = n - 1;
			}
			return g.feet[foot].planted[k] != 0u;
		}

	} // namespace anim
} // namespace nkentseu
