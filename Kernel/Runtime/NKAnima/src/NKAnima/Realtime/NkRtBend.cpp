// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkRtBend.cpp — l'os d'aide d'une articulation qui plie (voir NkRtBend.h).
// =============================================================================
#include "NKAnima/Realtime/NkRtBend.h"
#include "NKAnima/Realtime/NkRtRig.h" // les mutations des bancs

namespace nkentseu {
	namespace anim {
		namespace rt {

			namespace {
				bool Dans(uint32 joint, const uint32 *liste, uint32 n) {
					for (uint32 i = 0; i < n; ++i) {
						if (liste[i] == joint) {
							return true;
						}
					}
					return false;
				}
			} // namespace

			NkRtQuat NkRtBendHelperRotation(const NkRtQuat &childRef, const NkRtQuat &childLocal, float32 share) noexcept {
				// MUTATION bend_no_half : l'os d'aide ne prend rien de la flexion (il reste
				// avec le parent) -- la peau qui lui est rattachee ne plie plus.
				const float32 part = NkRtMutated(NK_RT_MUT_BEND_NO_HALF) ? 0.f : share;
				// Sans flexion (la pose EST la reference), ou sans part : la reference, au
				// bit pres. La comparaison se fait sur les rotations RECUES, pas sur leur
				// ecart calcule : conj(ref) * ref n'est l'identite qu'a l'arrondi pres
				// (1e-8 sur des reperes quelconques : PL1 l'a montre le 13/10).
				if (part == 0.f || (childLocal.x == childRef.x && childLocal.y == childRef.y && childLocal.z == childRef.z && childLocal.w == childRef.w)) {
					return childRef;
				}
				const NkRtQuat delta = QMul(QConj(childRef), childLocal);
				return QNormalize(QMul(childRef, QFromRotVec(Scale(QToRotVec(delta), part))));
			}

			float32 NkRtBendReweight(uint32 *joints, float32 *weights, uint32 influenceCount, const uint32 *parentSide, uint32 parentCount,
									 const uint32 *childSide, uint32 childCount, uint32 helperJoint) noexcept {
				if (joints == nullptr || weights == nullptr || parentSide == nullptr || childSide == nullptr) {
					return 0.f;
				}
				float32 wp = 0.f, wc = 0.f;
				for (uint32 i = 0; i < influenceCount; ++i) {
					if (weights[i] <= 0.f) {
						continue;
					}
					if (Dans(joints[i], parentSide, parentCount)) {
						wp += weights[i];
					} else if (Dans(joints[i], childSide, childCount)) {
						wc += weights[i];
					}
				}
				if (wp <= 0.f || wc <= 0.f) {
					return 0.f; // porte d'un seul cote : rien n'est touche
				}
				// Le plus petit cote est vide ; le plus grand garde la difference, dans les
				// proportions de ses joints ; l'os d'aide prend le reste.
				const bool hautGrand = wp >= wc;
				const float32 grand = hautGrand ? wp : wc, petit = hautGrand ? wc : wp;
				const float32 garde = (grand - petit) / grand;
				int32 libre = -1;
				for (uint32 i = 0; i < influenceCount; ++i) {
					if (weights[i] <= 0.f) {
						libre = libre < 0 ? (int32)i : libre;
						continue;
					}
					const bool haut = Dans(joints[i], parentSide, parentCount), bas = !haut && Dans(joints[i], childSide, childCount);
					if ((haut && hautGrand) || (bas && !hautGrand)) {
						weights[i] *= garde;
						if (weights[i] <= 0.f) {
							libre = libre < 0 ? (int32)i : libre;
						}
					} else if (haut || bas) {
						weights[i] = 0.f;
						libre = libre < 0 ? (int32)i : libre;
					}
				}
				// Une case est libre : le petit cote en occupait au moins une.
				if (libre < 0) {
					return 0.f;
				}
				joints[libre] = helperJoint;
				weights[libre] = 2.f * petit;
				return 2.f * petit;
			}

			float32 NkRtBlendRadius(float32 angle, float32 b) noexcept {
				const float32 a = 1.f - b;
				const float32 r2 = a * a + b * b + 2.f * a * b * std::cos(angle);
				return r2 > 0.f ? std::sqrt(r2) : 0.f;
			}

			float32 NkRtBendRadius(float32 angle, float32 b) noexcept {
				return b <= 0.5f ? NkRtBlendRadius(angle * 0.5f, 2.f * b) : NkRtBlendRadius(angle * 0.5f, 2.f * b - 1.f);
			}

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
