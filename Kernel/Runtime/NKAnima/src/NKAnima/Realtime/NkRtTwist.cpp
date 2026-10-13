// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkRtTwist.cpp — la torsion des os (voir NkRtTwist.h).
// =============================================================================
#include "NKAnima/Realtime/NkRtTwist.h"
#include "NKAnima/Realtime/NkRtRig.h" // les mutations des bancs

namespace nkentseu {
	namespace anim {
		namespace rt {

			namespace {
				constexpr float32 kPi = 3.14159265358979f;

				/// Un angle ramene dans ]-pi ; pi].
				float32 Wrap(float32 a) {
					while (a > kPi) {
						a -= 2.f * kPi;
					}
					while (a <= -kPi) {
						a += 2.f * kPi;
					}
					return a;
				}

				/// La torsion de q autour de a (unitaire) : le quaternion (a (v.a), w)
				/// norme. Identite si q est un demi-tour autour d'un axe perpendiculaire.
				NkRtQuat TwistOf(const NkRtQuat &q, const NkVec3f &a) {
					const float32 p = q.x * a.x + q.y * a.y + q.z * a.z;
					const float32 n = std::sqrt(p * p + q.w * q.w);
					if (n < 1e-8f) {
						return QIdentity();
					}
					return Q(a.x * p / n, a.y * p / n, a.z * p / n, q.w / n);
				}

				/// Ajoute `w` a l'influence du joint `joint` ; rend le poids DEPLACE (celui
				/// qui n'a pas pu rester sur sa station, faute de case).
				float32 AddInfluence(uint32 *joints, float32 *weights, uint32 count, uint32 joint, float32 w) {
					if (w <= 0.f) {
						return 0.f;
					}
					int32 libre = -1, fort = -1, faible = -1;
					for (uint32 i = 0; i < count; ++i) {
						if (weights[i] > 0.f && joints[i] == joint) {
							weights[i] += w;
							return 0.f;
						}
						if (weights[i] <= 0.f) {
							libre = libre < 0 ? (int32)i : libre;
							continue;
						}
						fort = (fort < 0 || weights[i] > weights[fort]) ? (int32)i : fort;
						faible = (faible < 0 || weights[i] < weights[faible]) ? (int32)i : faible;
					}
					if (libre >= 0) {
						joints[libre] = joint;
						weights[libre] = w;
						return 0.f;
					}
					if (fort < 0) {
						return w; // aucune case du tout
					}
					// Plus de case : la plus faible des influences (ou ce morceau, s'il est plus
					// faible encore) est fondue dans la plus forte. La somme est gardee.
					if (faible == fort || w <= weights[faible]) {
						weights[fort] += w;
						return w;
					}
					const float32 perdu = weights[faible];
					weights[fort] += perdu;
					joints[faible] = joint;
					weights[faible] = w;
					return perdu;
				}
			} // namespace

			// ── 1. MESURER ────────────────────────────────────────────────────
			void NkRtSwingTwist(const NkRtQuat &q, const NkVec3f &axisUnit, NkRtQuat &swing, NkRtQuat &twist) noexcept {
				twist = TwistOf(q, axisUnit);
				swing = QMul(q, QConj(twist));
			}

			float32 NkRtTwistAngle(const NkRtQuat &q, const NkVec3f &axisUnit) noexcept {
				const float32 p = q.x * axisUnit.x + q.y * axisUnit.y + q.z * axisUnit.z;
				if (p * p + q.w * q.w < 1e-16f) {
					return 0.f;
				}
				return Wrap(2.f * std::atan2(p, q.w));
			}

			float32 NkRtBoneTwist(const NkRtQuat &refLocal, const NkRtQuat &local, const NkVec3f &boneAxisLocal) noexcept {
				// MUTATION twist_parent_frame : la rotation ajoutee est prise du mauvais cote
				// de la reference (dans le repere du parent). Sans effet si la reference est
				// l'identite : c'est pourquoi le banc eprouve des reperes QUELCONQUES.
				if (NkRtMutated(NK_RT_MUT_TWIST_PARENT_FRAME)) {
					return NkRtTwistAngle(QMul(local, QConj(refLocal)), boneAxisLocal);
				}
				return NkRtTwistAngle(QMul(QConj(refLocal), local), boneAxisLocal);
			}

			float32 NkRtChildTwist(const NkRtQuat &refLocal, const NkRtQuat &local, const NkVec3f &parentAxisLocal) noexcept {
				return NkRtTwistAngle(QMul(local, QConj(refLocal)), parentAxisLocal);
			}

			// ── 2. BORNER ET REPORTER ────────────────────────────────────────
			NkRtTwistCarry NkRtLimitTwistCarry(NkRtQuat &parentLocal, NkRtQuat &childLocal, const NkRtQuat &childRef,
											   const NkVec3f &parentAxisLocal, const NkRtTwistLimit &limit) noexcept {
				NkRtTwistCarry r;
				r.before = NkRtChildTwist(childRef, childLocal, parentAxisLocal);
				r.after = r.before;
				const float32 borne = Clamp(r.before, limit.minAngle, limit.maxAngle);
				const float32 excedent = r.before - borne;
				if (excedent == 0.f) {
					return r; // dans la borne : rien n'est touche
				}
				// L'enfant rend l'excedent, autour de l'axe du parent (dans le repere du
				// parent, donc a GAUCHE de sa rotation locale) ; le parent le prend, autour
				// de son propre axe (a DROITE de la sienne). Le produit parent * enfant ne
				// change pas : l'enfant garde son orientation dans le monde, et sa place
				// s'il est attache sur l'axe du parent. La torsion restante est EXACTEMENT
				// la borne : l'excedent retire est une torsion pure autour du meme axe.
				// MUTATION twist_no_carry : l'enfant rend l'excedent, le parent ne prend
				// rien (la main change d'orientation dans le monde).
				childLocal = QNormalize(QMul(QFromAxisAngle(parentAxisLocal, -excedent), childLocal));
				if (!NkRtMutated(NK_RT_MUT_TWIST_NO_CARRY)) {
					parentLocal = QNormalize(QMul(parentLocal, QFromAxisAngle(parentAxisLocal, excedent)));
					r.carried = excedent;
				}
				r.after = NkRtChildTwist(childRef, childLocal, parentAxisLocal);
				return r;
			}

			NkRtTwistCarry NkRtLimitTwistToChild(NkRtQuat &boneLocal, NkRtQuat &childLocal, const NkRtQuat &boneRef,
												 const NkVec3f &boneAxisLocal, const NkRtTwistLimit &limit) noexcept {
				NkRtTwistCarry r;
				r.before = NkRtBoneTwist(boneRef, boneLocal, boneAxisLocal);
				r.after = r.before;
				const float32 borne = Clamp(r.before, limit.minAngle, limit.maxAngle);
				const float32 excedent = r.before - borne;
				if (excedent == 0.f) {
					return r; // dans la borne : rien n'est touche
				}
				// L'os rend l'excedent autour de son axe (dans SON repere : a DROITE de sa
				// rotation locale) ; l'enfant le prend autour du meme axe, qui est dans le
				// repere de son parent (a GAUCHE de la sienne). os * R(-e) * R(+e) * enfant :
				// le produit ne change pas.
				// MUTATION twist_no_carry : l'os rend, l'enfant ne prend rien.
				boneLocal = QNormalize(QMul(boneLocal, QFromAxisAngle(boneAxisLocal, -excedent)));
				if (!NkRtMutated(NK_RT_MUT_TWIST_NO_CARRY)) {
					childLocal = QNormalize(QMul(QFromAxisAngle(boneAxisLocal, excedent), childLocal));
					r.carried = excedent;
				}
				r.after = NkRtBoneTwist(boneRef, boneLocal, boneAxisLocal);
				return r;
			}

			// ── 3. REPARTIR : LES OS DE TORSION ───────────────────────────────
			NkRtTwistChain NkRtMakeTwistChain(NkRtTwistMode mode, uint32 twistBones, bool jointKeepsShare) noexcept {
				NkRtTwistChain c;
				c.mode = mode;
				const uint32 n = twistBones < 1u ? 1u : (twistBones > kNkRtTwistMaxStations - 1u ? kNkRtTwistMaxStations - 1u : twistBones);
				c.count = n + 1u;
				// FOLLOW_CHILD : l'os a son origine, puis les os de torsion jusqu'a son bout.
				// HOLD_PARENT  : les os de torsion depuis l'origine, puis l'os a son bout.
				const bool suit = mode == NK_RT_TWIST_FOLLOW_CHILD;
				c.self = suit ? 0u : n;
				for (uint32 k = 0; k <= n; ++k) {
					c.s[k] = (float32)k / (float32)n;
					if (!jointKeepsShare) {
						c.a[k] = c.s[k]; // les stations portent tout : de 0 a 1
					} else {
						// L'articulation au bout garde sa part : n + 1 sauts egaux. L'os lui-meme
						// reste a 0 (FOLLOW_CHILD) ou a 1 (HOLD_PARENT).
						c.a[k] = suit ? (float32)k / (float32)(n + 1u) : (float32)(k + 1u) / (float32)(n + 1u);
					}
				}
				return c;
			}

			float32 NkRtTwistStationAngle(const NkRtTwistChain &chain, uint32 k, float32 twist) noexcept {
				// MUTATION twist_no_spread : aucune station ne tourne (rien n'est reparti).
				if (k >= chain.count || k == chain.self || NkRtMutated(NK_RT_MUT_TWIST_NO_SPREAD)) {
					return 0.f;
				}
				return chain.mode == NK_RT_TWIST_FOLLOW_CHILD ? chain.a[k] * twist : -(1.f - chain.a[k]) * twist;
			}

			float32 NkRtTwistSkinShare(const NkRtTwistChain &chain, float32 s) noexcept {
				if (chain.count == 0u) {
					return 0.f;
				}
				// La part de chaque station (chain.a) : en FOLLOW_CHILD, ce qu'elle tourne ; en
				// HOLD_PARENT, la torsion de l'os moins ce qu'elle retient. L'os lui-meme :
				// 0 dans le premier regime, 1 dans le second.
				auto Part = [&](uint32 k) {
					if (k == chain.self) {
						return chain.mode == NK_RT_TWIST_FOLLOW_CHILD ? 0.f : 1.f;
					}
					if (NkRtMutated(NK_RT_MUT_TWIST_NO_SPREAD)) {
						return chain.mode == NK_RT_TWIST_FOLLOW_CHILD ? 0.f : 1.f;
					}
					return chain.a[k];
				};
				if (s <= chain.s[0]) {
					return Part(0u);
				}
				for (uint32 k = 1; k < chain.count; ++k) {
					if (s <= chain.s[k]) {
						const float32 e = chain.s[k] - chain.s[k - 1u];
						const float32 t = e > 1e-9f ? (s - chain.s[k - 1u]) / e : 1.f;
						return Part(k - 1u) + (Part(k) - Part(k - 1u)) * t;
					}
				}
				return Part(chain.count - 1u);
			}

			float32 NkRtTwistMaxStep(const NkRtTwistChain &chain, float32 twist, const float32 *stationAngle) noexcept {
				if (chain.count < 2u || stationAngle == nullptr) {
					return twist;
				}
				// La torsion de la peau a chaque station, contre le parent de l'os.
				const bool suit = chain.mode == NK_RT_TWIST_FOLLOW_CHILD;
				float32 pire = 0.f, avant = 0.f; // FOLLOW : l'os lui-meme ; HOLD : le parent
				for (uint32 k = 0; k < chain.count; ++k) {
					float32 a = 0.f;
					if (suit) {
						a = k == chain.self ? 0.f : stationAngle[k];
					} else {
						a = k == chain.self ? twist : twist + stationAngle[k];
					}
					const float32 saut = Wrap(a - avant);
					pire = std::fabs(saut) > std::fabs(pire) ? saut : pire;
					avant = a;
				}
				if (suit) { // le dernier voisin : l'enfant, qui porte toute la torsion
					const float32 saut = Wrap(twist - avant);
					pire = std::fabs(saut) > std::fabs(pire) ? saut : pire;
				}
				return pire;
			}

			float32 NkRtTwistReweight(uint32 *joints, float32 *weights, uint32 influenceCount, uint32 boneJoint, float32 s,
									  const NkRtTwistChain &chain, const uint32 *stationJoint) noexcept {
				if (joints == nullptr || weights == nullptr || stationJoint == nullptr || chain.count < 2u) {
					return 0.f;
				}
				// Le poids porte par l'os (toutes ses cases, s'il en a plusieurs).
				float32 w = 0.f;
				for (uint32 i = 0; i < influenceCount; ++i) {
					if (weights[i] > 0.f && joints[i] == boneJoint) {
						w += weights[i];
						weights[i] = 0.f;
					}
				}
				if (w <= 0.f) {
					return 0.f;
				}
				// Les deux stations qui encadrent `s`.
				uint32 a = 0u, b = 0u;
				float32 wb = 0.f;
				if (s <= chain.s[0]) {
					a = b = 0u;
				} else if (s >= chain.s[chain.count - 1u]) {
					a = b = chain.count - 1u;
				} else {
					for (uint32 k = 1; k < chain.count; ++k) {
						if (s <= chain.s[k]) {
							a = k - 1u;
							b = k;
							const float32 e = chain.s[k] - chain.s[k - 1u];
							wb = e > 1e-9f ? (s - chain.s[k - 1u]) / e : 1.f;
							break;
						}
					}
				}
				float32 deplace = 0.f;
				// La plus grosse part d'abord : c'est elle qui doit trouver une case.
				const float32 wa = w * (1.f - wb), wbb = w * wb;
				if (wa >= wbb) {
					deplace += AddInfluence(joints, weights, influenceCount, stationJoint[a], wa);
					deplace += AddInfluence(joints, weights, influenceCount, stationJoint[b], wbb);
				} else {
					deplace += AddInfluence(joints, weights, influenceCount, stationJoint[b], wbb);
					deplace += AddInfluence(joints, weights, influenceCount, stationJoint[a], wa);
				}
				return deplace;
			}

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
