// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkLiveBalance.cpp — l'equilibre en temps reel (voir l'en-tete).
// =============================================================================
#include "NKAnima/Realtime/NkLiveBalance.h"

#include "NKAnima/Realtime/NkFootIK.h"

namespace nkentseu {
	namespace anim {
		namespace rt {

			namespace {
				struct P2 {
						float32 x, z;
				};
				float32 Croix(const P2 &o, const P2 &a, const P2 &b) {
					return (a.x - o.x) * (b.z - o.z) - (a.z - o.z) * (b.x - o.x);
				}
				/// Enveloppe convexe (chaine monotone d'Andrew), sens trigonometrique.
				uint32 Enveloppe(P2 *p, uint32 n, P2 *h) {
					// Tri par insertion (n <= 32).
					for (uint32 i = 1; i < n; ++i) {
						const P2 v = p[i];
						uint32 k = i;
						while (k > 0 && (p[k - 1].x > v.x || (p[k - 1].x == v.x && p[k - 1].z > v.z))) {
							p[k] = p[k - 1];
							--k;
						}
						p[k] = v;
					}
					if (n < 3) {
						for (uint32 i = 0; i < n; ++i)
							h[i] = p[i];
						return n;
					}
					uint32 k = 0;
					for (uint32 i = 0; i < n; ++i) {
						while (k >= 2 && Croix(h[k - 2], h[k - 1], p[i]) <= 0.f)
							--k;
						h[k++] = p[i];
					}
					for (uint32 i = n - 1, t = k + 1; i-- > 0;) {
						while (k >= t && Croix(h[k - 2], h[k - 1], p[i]) <= 0.f)
							--k;
						h[k++] = p[i];
					}
					return k > 1 ? k - 1 : k;
				}
				float32 DistSegment(const P2 &q, const P2 &a, const P2 &b) {
					const float32 ux = b.x - a.x, uz = b.z - a.z;
					const float32 l2 = ux * ux + uz * uz;
					float32 t = l2 > 1e-12f ? ((q.x - a.x) * ux + (q.z - a.z) * uz) / l2 : 0.f;
					t = Clamp(t, 0.f, 1.f);
					const float32 dx = q.x - (a.x + ux * t), dz = q.z - (a.z + uz * t);
					return std::sqrt(dx * dx + dz * dz);
				}
				float32 MargeEnveloppe(const P2 &q, const P2 *h, uint32 k) {
					if (k == 0)
						return -1e30f;
					if (k == 1)
						return -std::sqrt((q.x - h[0].x) * (q.x - h[0].x) + (q.z - h[0].z) * (q.z - h[0].z));
					if (k == 2)
						return -DistSegment(q, h[0], h[1]);
					bool dedans = true;
					float32 dmin = 1e30f;
					for (uint32 i = 0; i < k; ++i) {
						const P2 &a = h[i];
						const P2 &b = h[(i + 1) % k];
						if (Croix(a, b, q) < 0.f)
							dedans = false;
						const float32 d = DistSegment(q, a, b);
						if (d < dmin)
							dmin = d;
					}
					return dedans ? dmin : -dmin;
				}
			} // namespace

			float32 NkLiveBalance::SignedMargin(const NkVec3f &p, const NkVec3f *pts, uint32 count) {
				if (pts == nullptr || count == 0u)
					return -1e30f;
				P2 tmp[kMaxSupport], h[2u * kMaxSupport + 2u];
				const uint32 n = count < kMaxSupport ? count : kMaxSupport;
				for (uint32 i = 0; i < n; ++i)
					tmp[i] = P2{pts[i].x, pts[i].z};
				const uint32 k = Enveloppe(tmp, n, h);
				return MargeEnveloppe(P2{p.x, p.z}, h, k);
			}

			bool NkLiveBalance::Setup(const NkRtRig &rig, int32 pelvis) {
				NkVector<NkString> noms;
				noms.Resize(rig.Count());
				bool nomme = false;
				for (uint32 j = 0; j < rig.Count(); ++j) {
					noms[j] = NkString(rig.Name(j));
					nomme = nomme || rig.Name(j)[0] != '\0';
				}
				if (nomme)
					mMass.SetAnthropometric(noms);
				else
					mMass.SetUniform((int32)rig.Count());
				mPelvis = (pelvis >= 0 && (uint32)pelvis < rig.Count()) ? pelvis : -1;
				if (mPelvis < 0) {
					for (uint32 j = 0; j < rig.Count(); ++j) {
						if (rig.Parent(j) < 0) {
							mPelvis = (int32)j;
							break;
						}
					}
				}
				mFollow = 1.f;
				Reset();
				return mPelvis >= 0;
			}

			void NkLiveBalance::SetPinnedLegs(const NkRtRig &rig, const uint32 *ankles, const uint32 *knees, uint32 count) {
				const float32 total = mMass.TotalMass();
				if (total <= 0.f || ankles == nullptr) {
					mFollow = 1.f;
					return;
				}
				float32 fixe = 0.f;
				for (uint32 i = 0; i < count; ++i) {
					const uint32 a = ankles[i];
					// La cheville et tout ce qu'elle porte restent plantes.
					for (uint32 j = 0; j < rig.Count(); ++j) {
						if (rig.IsInSubtree(j, a))
							fixe += mMass.jointMass[j];
					}
					// Le genou suit a moitie (la jambe pivote autour de la cheville).
					if (knees != nullptr && knees[i] < rig.Count())
						fixe += 0.5f * mMass.jointMass[knees[i]];
				}
				mFollow = Clamp(1.f - fixe / total, 0.3f, 1.f);
			}

			void NkLiveBalance::Reset() {
				mShiftX = mShiftZ = mVelX = mVelZ = 0.f;
				mPrimed = false;
			}

			void NkLiveBalance::Relax() {
				Reset();
				mPrimed = true;
			}

			float32 NkLiveBalance::Apply(const NkRtRig &rig, const math::NkMat4f *local, math::NkMat4f *world,
										 const NkVec3f *support, uint32 count, float32 dt) {
				if (mPelvis < 0)
					return 0.f;
				mCom = mMass.ComputeCOM(world, (int32)rig.Count());
				float32 cibleX = 0.f, cibleZ = 0.f;
				float32 marge = -1e30f;
				if (support != nullptr && count > 0u) {
					P2 tmp[kMaxSupport], h[2u * kMaxSupport + 2u];
					const uint32 n = count < kMaxSupport ? count : kMaxSupport;
					float32 cx = 0.f, cz = 0.f;
					for (uint32 i = 0; i < n; ++i) {
						tmp[i] = P2{support[i].x, support[i].z};
						cx += support[i].x;
						cz += support[i].z;
					}
					cx /= (float32)n;
					cz /= (float32)n;
					const uint32 k = Enveloppe(tmp, n, h);
					const P2 c0{mCom.x, mCom.z};
					marge = MargeEnveloppe(c0, h, k);
					mMarginBefore = marge;
					const NkLiveBalanceSettings &s = settings;
					if (marge < s.targetMargin && !NkRtMutated(NK_RT_MUT_BALANCE_OFF)) {
						// Le plus petit pas vers le centre des appuis qui rend la marge voulue.
						float32 lo = 0.f, hi = 1.f;
						for (uint32 it = 0; it < 14u; ++it) {
							const float32 m = 0.5f * (lo + hi);
							const P2 q{c0.x + (cx - c0.x) * m, c0.z + (cz - c0.z) * m};
							if (MargeEnveloppe(q, h, k) >= s.targetMargin)
								hi = m;
							else
								lo = m;
						}
						const float32 f = mFollow > 0.05f ? mFollow : 0.05f;
						cibleX = (cx - c0.x) * hi / f;
						cibleZ = (cz - c0.z) * hi / f;
						const float32 l = std::sqrt(cibleX * cibleX + cibleZ * cibleZ);
						if (l > s.maxShift && l > 1e-9f) {
							cibleX *= s.maxShift / l;
							cibleZ *= s.maxShift / l;
						}
					}
				}
				const float32 poids = Clamp(settings.weight, 0.f, 1.f);
				cibleX *= poids;
				cibleZ *= poids;
				if (!mPrimed) {
					mShiftX = cibleX;
					mShiftZ = cibleZ;
					mVelX = mVelZ = 0.f;
					mPrimed = true;
				} else {
					NkRtSmoothDamp(mShiftX, mVelX, cibleX, settings.smoothTime, dt);
					NkRtSmoothDamp(mShiftZ, mVelZ, cibleZ, settings.smoothTime, dt);
				}
				const uint32 pj = (uint32)mPelvis;
				world[pj].m30 += mShiftX;
				world[pj].m32 += mShiftZ;
				rig.RefreshDescendants(pj, local, world);
				if (support == nullptr || count == 0u)
					return marge;
				const NkVec3f predit = V3(mCom.x + mShiftX * mFollow, mCom.y, mCom.z + mShiftZ * mFollow);
				return SignedMargin(predit, support, count);
			}

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
