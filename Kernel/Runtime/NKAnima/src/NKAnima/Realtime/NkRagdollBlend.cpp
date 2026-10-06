// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkRagdollBlend.cpp — le ragdoll leger et la bascule (voir l'en-tete).
// =============================================================================
#include "NKAnima/Realtime/NkRagdollBlend.h"

namespace nkentseu {
	namespace anim {
		namespace rt {

			namespace {
				char Bas(char c) {
					return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
				}
				bool Contient(const char *nom, const char *mot) {
					for (; *nom != '\0'; ++nom) {
						const char *a = nom, *b = mot;
						while (*a != '\0' && *b != '\0' && Bas(*a) == Bas(*b)) {
							++a;
							++b;
						}
						if (*b == '\0')
							return true;
					}
					return false;
				}
				bool EstColonne(const char *nom) {
					return Contient(nom, "spine") || Contient(nom, "chest") || Contient(nom, "neck") ||
						   Contient(nom, "hip") || Contient(nom, "pelvis") || Contient(nom, "head") ||
						   Contient(nom, "torso") || Contient(nom, "colonne") || Contient(nom, "cou");
				}
				/// Distance entre les bouts de deux segments (l1, l2) qui font l'angle `theta`.
				float32 Ouverture(float32 l1, float32 l2, float32 theta) {
					const float32 d2 = l1 * l1 + l2 * l2 - 2.f * l1 * l2 * std::cos(theta);
					return std::sqrt(d2 > 0.f ? d2 : 0.f);
				}
				/// Un repere orthonorme depuis deux directions (u, puis v dans le plan).
				NkRtQuat Repere(const NkVec3f &u0, const NkVec3f &v0) {
					const NkVec3f u = Normalize(u0, V3(0.f, 1.f, 0.f));
					const NkVec3f w = Normalize(Cross(u, v0), AnyPerpendicular(u));
					const NkVec3f v = Cross(w, u);
					NkMat4f m = NkMat4f::Identity();
					SetAxes(m, u, v, w);
					return QFromMat(m);
				}
			} // namespace

			// ── NkPoseRagdoll ──────────────────────────────────────────────────────
			void NkPoseRagdoll::Contraindre(uint32 a, uint32 b, float32 mini, float32 maxi, bool os) {
				Contrainte c;
				c.a = a;
				c.b = b;
				c.mini = mini;
				c.maxi = maxi;
				c.os = os;
				mC.PushBack(c);
			}

			bool NkPoseRagdoll::Setup(const NkRtRig &rig, const math::NkMat4f *bindWorld, const NkPoseMass *mass) {
				mRig = nullptr;
				mC.Clear();
				const uint32 n = rig.Count();
				if (n == 0u || bindWorld == nullptr)
					return false;
				mJoints = n;
				mBind.Resize(n);
				for (uint32 j = 0; j < n; ++j)
					mBind[j] = bindWorld[j];
				mVirtual.Resize(n);
				mVirtLocal.Clear();
				mVirtOwner.Clear();
				NkVector<NkVec3f> p;
				p.Resize(n);
				for (uint32 j = 0; j < n; ++j) {
					p[j] = Origin(bindWorld[j]);
					mVirtual[j] = -1;
				}
				// Les os : rigides.
				for (uint32 j = 0; j < n; ++j) {
					const int32 par = rig.Parent(j);
					if (par >= 0) {
						const float32 l = Distance(p[j], p[(uint32)par]);
						Contraindre((uint32)par, j, l, l, true);
					}
				}
				// La flexion de chaque articulation : grand-parent <-> enfant, borne.
				for (uint32 j = 0; j < n; ++j) {
					const int32 par = rig.Parent(j);
					if (par < 0)
						continue;
					const int32 gp = rig.Parent((uint32)par);
					if (gp < 0)
						continue;
					// Un joint « branche » (bassin, poitrine) est rigidifie plus bas.
					if (rig.ChildCount((uint32)par) > 1u)
						continue;
					const float32 l1 = Distance(p[(uint32)gp], p[(uint32)par]), l2 = Distance(p[(uint32)par], p[j]);
					const NkVec3f u = Normalize(Sub(p[(uint32)gp], p[(uint32)par]), V3(0.f, 1.f, 0.f));
					const NkVec3f v = Normalize(Sub(p[j], p[(uint32)par]), V3(0.f, -1.f, 0.f));
					const float32 theta = std::acos(Clamp(Dot(u, v), -1.f, 1.f)); // 180 deg : tendu
					const bool colonne = EstColonne(rig.Name((uint32)par));
					const float32 flex = colonne ? settings.spineMaxBend : settings.limbMaxBend;
					float32 thetaMin = 3.14159265f - flex;
					if (thetaMin > theta)
						thetaMin = theta;
					float32 thetaMax = 3.14159265f;
					if (colonne) {
						// La colonne ne se plie pas en arriere au-dela de son repos + flex/2.
						thetaMin = theta - flex;
						if (thetaMin < 0.35f)
							thetaMin = 0.35f;
					}
					Contraindre((uint32)gp, j, Ouverture(l1, l2, thetaMin), Ouverture(l1, l2, thetaMax), false);
				}
				// Les « branches » (2 enfants ou plus) : un corps rigide, avec une
				// particule virtuelle hors de leur plan.
				for (uint32 j = 0; j < n; ++j) {
					if (rig.ChildCount(j) < 2u)
						continue;
					NkVector<uint32> grappe;
					grappe.PushBack(j);
					for (uint32 c = 0; c < n; ++c)
						if (rig.Parent(c) == (int32)j)
							grappe.PushBack(c);
					const int32 par = rig.Parent(j);
					if (par >= 0)
						grappe.PushBack((uint32)par);
					// La normale du plan : depuis deux enfants les moins alignes.
					NkVec3f nrm = V3(0.f, 0.f, 0.f);
					for (uint32 a = 1; a < (uint32)grappe.Size(); ++a)
						for (uint32 b = a + 1; b < (uint32)grappe.Size(); ++b) {
							const NkVec3f c = Cross(Sub(p[grappe[a]], p[j]), Sub(p[grappe[b]], p[j]));
							if (Length(c) > Length(nrm))
								nrm = c;
						}
					nrm = Normalize(nrm, V3(0.f, 0.f, 1.f));
					const NkVec3f pv = Madd(p[j], nrm, 0.10f);
					const uint32 iv = n + (uint32)mVirtOwner.Size();
					mVirtual[j] = (int32)iv;
					mVirtOwner.PushBack(j);
					mVirtLocal.PushBack(TransformPoint(bindWorld[j].Inverse(), pv));
					p.PushBack(pv);
					grappe.PushBack(iv);
					for (uint32 a = 0; a < (uint32)grappe.Size(); ++a)
						for (uint32 b = a + 1; b < (uint32)grappe.Size(); ++b) {
							const uint32 ia = grappe[a], ib = grappe[b];
							// Le parent de la branche : il garde sa propre flexion.
							if ((par >= 0 && (ia == (uint32)par || ib == (uint32)par)) && ia != iv && ib != iv &&
								ia != j && ib != j)
								continue;
							const float32 l = Distance(p[ia], p[ib]);
							Contraindre(ia, ib, l, l, false);
						}
				}
				const uint32 total = (uint32)p.Size();
				mX.Resize(total);
				mXPrev.Resize(total);
				mInvMass.Resize(total);
				mSolOk.Resize(total, (uint8)0);
				mSolP.Resize(total);
				mSolN.Resize(total);
				for (uint32 i = 0; i < total; ++i) {
					mX[i] = p[i];
					mXPrev[i] = p[i];
					float32 m = 1.f;
					if (i < n && mass != nullptr && (uint32)mass->jointMass.Size() == n)
						m = mass->jointMass[i];
					if (i >= n)
						m = 0.5f;
					mInvMass[i] = m > 1e-4f ? 1.f / m : 1e4f;
				}
				// L'avant du corps : des chevilles vers les orteils.
				NkVec3f av = V3(0.f, 0.f, 0.f);
				for (uint32 j = 0; j < n; ++j) {
					if (Contient(rig.Name(j), "toe") && rig.Parent(j) >= 0) {
						const NkVec3f d = Sub(p[j], p[(uint32)rig.Parent(j)]);
						av = Add(av, V3(d.x, 0.f, d.z));
					}
				}
				mForward0 = Normalize(av, V3(0.f, 0.f, 1.f));
				mR0.Resize(n);
				mS0.Resize(n);
				mD0.Resize(n);
				mF0.Resize(n);
				mDelta.Resize(n);
				mRig = &rig;
				Start(bindWorld, bindWorld, 1.f / 60.f);
				return true;
			}

			void NkPoseRagdoll::Start(const math::NkMat4f *now, const math::NkMat4f *prev, float32 dtPrev) {
				if (mRig == nullptr)
					return;
				const NkRtRig &rig = *mRig;
				const math::NkMat4f *depart = NkRtMutated(NK_RT_MUT_RAGDOLL_FROM_BIND) ? mBind.Data() : now;
				const bool sansVitesse = NkRtMutated(NK_RT_MUT_RAGDOLL_NO_VELOCITY) || prev == nullptr || dtPrev <= 0.f;
				const float32 h = settings.substep;
				const uint32 n = mJoints;
				for (uint32 j = 0; j < (uint32)mX.Size(); ++j) {
					NkVec3f x, xa;
					if (j < n) {
						x = Origin(depart[j]);
						xa = sansVitesse ? x : Origin(prev[j]);
					} else {
						const uint32 o = mVirtOwner[j - n];
						x = TransformPoint(depart[o], mVirtLocal[j - n]);
						xa = sansVitesse ? x : TransformPoint(prev[o], mVirtLocal[j - n]);
					}
					const NkVec3f v = sansVitesse ? V3(0.f, 0.f, 0.f) : Scale(Sub(x, xa), 1.f / dtPrev);
					mX[j] = x;
					mXPrev[j] = Madd(x, v, -h);
				}
				for (uint32 j = 0; j < n; ++j) {
					mR0[j] = QFromMat(depart[j]);
					mS0[j] = AxisScales(depart[j]);
					const int32 c = rig.FirstChild(j);
					mD0[j] = c >= 0 ? Sub(mX[(uint32)c], mX[j]) : V3(0.f, 0.f, 0.f);
					if (mVirtual[j] >= 0 && c >= 0)
						mF0[j] = Repere(Sub(mX[(uint32)c], mX[j]), Sub(mX[(uint32)mVirtual[j]], mX[j]));
					else
						mF0[j] = QIdentity();
					mDelta[j] = QIdentity();
				}
				mMaxSpeed = 0.f;
				mRestTime = 0.f;
			}

			void NkPoseRagdoll::ApplyImpulse(uint32 joint, const NkVec3f &dv) {
				if (mRig == nullptr || joint >= mJoints)
					return;
				const float32 h = settings.substep;
				mXPrev[joint] = Madd(mXPrev[joint], dv, -h);
				const NkRtRig &rig = *mRig;
				for (uint32 j = 0; j < mJoints; ++j) {
					if (j != joint && (rig.Parent(j) == (int32)joint || rig.Parent(joint) == (int32)j))
						mXPrev[j] = Madd(mXPrev[j], dv, -0.5f * h);
				}
				if (mVirtual[joint] >= 0)
					mXPrev[(uint32)mVirtual[joint]] = Madd(mXPrev[(uint32)mVirtual[joint]], dv, -h);
			}

			void NkPoseRagdoll::Step(float32 dt, const NkRtGround &ground) {
				if (mRig == nullptr || dt <= 0.f)
					return;
				const NkRagdollSettings &s = settings;
				const float32 h = s.substep > 1e-4f ? s.substep : 1e-4f;
				uint32 nsub = (uint32)std::ceil(dt / h - 1e-4f);
				if (nsub < 1u)
					nsub = 1u;
				if (nsub > 16u)
					nsub = 16u;
				const float32 hh = dt / (float32)nsub;
				const uint32 total = (uint32)mX.Size();
				const float32 amort = 1.f - Clamp(s.damping * hh, 0.f, 0.5f);
				float32 vmax = 0.f;
				for (uint32 sub = 0; sub < nsub; ++sub) {
					// Verlet : la vitesse implicite (x - xPrev), la gravite.
					for (uint32 i = 0; i < total; ++i) {
						const NkVec3f x = mX[i];
						const NkVec3f v = Scale(Sub(x, mXPrev[i]), amort);
						mXPrev[i] = x;
						mX[i] = Add(Add(x, v), V3(0.f, s.gravity * hh * hh, 0.f));
					}
					// Le sol sous chaque particule : UN lancer par sous-pas, garde comme un plan.
					for (uint32 i = 0; i < total; ++i) {
						NkRtRayHit hit;
						const NkVec3f o = Madd(mX[i], V3(0.f, 1.f, 0.f), 0.6f);
						mSolOk[i] = ground.Cast(o, V3(0.f, -1.f, 0.f), 0.6f + s.radius + 0.6f, hit) ? 1u : 0u;
						mSolP[i] = hit.point;
						mSolN[i] = Normalize(hit.normal, V3(0.f, 1.f, 0.f));
					}
					// Les contraintes (Gauss-Seidel) ; le sol AVANT chaque passe : la derniere
					// operation est une passe de contraintes (les os restent justes).
					for (uint32 it = 0; it < s.iterations; ++it) {
						for (uint32 i = 0; i < total; ++i) {
							if (mSolOk[i] == 0u)
								continue;
							const float32 dist = Dot(Sub(mX[i], mSolP[i]), mSolN[i]) - s.radius;
							if (dist < 0.f)
								mX[i] = Madd(mX[i], mSolN[i], -dist);
						}
						for (uint32 k = 0; k < (uint32)mC.Size(); ++k) {
							const Contrainte &c = mC[k];
							const NkVec3f d = Sub(mX[c.b], mX[c.a]);
							const float32 l = Length(d);
							if (l < 1e-9f)
								continue;
							float32 cible = l;
							if (l < c.mini)
								cible = c.mini;
							else if (l > c.maxi)
								cible = c.maxi;
							else
								continue;
							const float32 wa = mInvMass[c.a], wb = mInvMass[c.b];
							const float32 ws = wa + wb;
							if (ws <= 0.f)
								continue;
							const NkVec3f corr = Scale(d, (l - cible) / (l * ws));
							mX[c.a] = Madd(mX[c.a], corr, wa);
							mX[c.b] = Madd(mX[c.b], corr, -wb);
						}
					}
					// Le contact : sans rebond (la vitesse normale s'annule), et le frottement
					// freine la vitesse tangente -- sur la VITESSE (xPrev), pas la position.
					for (uint32 i = 0; i < total; ++i) {
						if (mSolOk[i] == 0u)
							continue;
						const float32 dist = Dot(Sub(mX[i], mSolP[i]), mSolN[i]) - s.radius;
						if (dist > 0.005f)
							continue;
						const NkVec3f v = Sub(mX[i], mXPrev[i]);
						const float32 vn = Dot(v, mSolN[i]);
						const NkVec3f vt = Sub(v, Scale(mSolN[i], vn));
						const NkVec3f v2 = Scale(vt, 1.f - s.friction);
						mXPrev[i] = Sub(mX[i], v2);
					}
					for (uint32 i = 0; i < total; ++i) {
						const float32 v = Distance(mX[i], mXPrev[i]) / hh;
						if (v > vmax)
							vmax = v;
					}
				}
				mMaxSpeed = vmax;
				mRestTime = vmax < s.settleSpeed ? mRestTime + dt : 0.f;
			}

			float32 NkPoseRagdoll::MaxBoneStretch() const {
				float32 pire = 0.f;
				for (uint32 k = 0; k < (uint32)mC.Size(); ++k) {
					const Contrainte &c = mC[k];
					if (!c.os || c.maxi <= 1e-6f)
						continue;
					const float32 e = std::fabs(Distance(mX[c.a], mX[c.b]) - c.maxi) / c.maxi;
					if (e > pire)
						pire = e;
				}
				return pire;
			}

			void NkPoseRagdoll::Recalculer() const {
				const NkRtRig &rig = *mRig;
				for (uint32 k = 0; k < mJoints; ++k) {
					const uint32 j = rig.Order()[k];
					const int32 par = rig.Parent(j);
					const NkRtQuat dp = par >= 0 ? mDelta[(uint32)par] : QIdentity();
					const int32 c = rig.FirstChild(j);
					if (mVirtual[j] >= 0 && c >= 0) {
						const NkRtQuat f = Repere(Sub(mX[(uint32)c], mX[j]), Sub(mX[(uint32)mVirtual[j]], mX[j]));
						mDelta[j] = QNormalize(QMul(f, QConj(mF0[j])));
					} else if (c >= 0 && Length(mD0[j]) > 1e-6f) {
						const NkVec3f d0 = QRotate(dp, mD0[j]);
						mDelta[j] = QNormalize(QMul(QFromTo(d0, Sub(mX[(uint32)c], mX[j])), dp));
					} else {
						mDelta[j] = dp;
					}
				}
			}

			void NkPoseRagdoll::ReadPose(math::NkMat4f *out) const {
				if (mRig == nullptr)
					return;
				Recalculer();
				for (uint32 j = 0; j < mJoints; ++j)
					out[j] = MatFrom(QMul(mDelta[j], mR0[j]), mS0[j], mX[j]);
			}

			// ── NkRagdollBlend ─────────────────────────────────────────────────────
			bool NkRagdollBlend::Setup(const NkRtRig &rig, const math::NkMat4f *bindWorld, const NkPoseMass *mass) {
				mPhase = NkRagdollPhase::Animated;
				mTime = mBlend = 0.f;
				mReloc = NkMat4f::Identity();
				mRelocActive = false;
				const uint32 n = rig.Count();
				mBindPelvis = NkMat4f::Identity();
				for (uint32 j = 0; j < n && bindWorld != nullptr; ++j) {
					if (rig.Parent(j) < 0) {
						mBindPelvis = bindWorld[j];
						break;
					}
				}
				mRag.Resize(n);
				mFrozen.Resize(n);
				mTmp.Resize(n);
				mLa.Resize(n);
				mLb.Resize(n);
				return ragdoll.Setup(rig, bindWorld, mass);
			}

			void NkRagdollBlend::Trigger(const math::NkMat4f *now, const math::NkMat4f *prev, float32 dtPrev, uint32 joint,
										 const NkVec3f &dv) {
				if (!ragdoll.Ready())
					return;
				if (mPhase == NkRagdollPhase::Animated || mPhase == NkRagdollPhase::GettingUp)
					ragdoll.Start(now, prev, dtPrev);
				ragdoll.ApplyImpulse(joint, dv);
				mPhase = NkRagdollPhase::Falling;
				mTime = 0.f;
				mBlend = 0.f;
			}

			void NkRagdollBlend::StartGetUp() {
				if (mPhase == NkRagdollPhase::Ragdoll || mPhase == NkRagdollPhase::Falling) {
					ragdoll.ReadPose(mFrozen.Data());
					mPhase = NkRagdollPhase::GettingUp;
					mTime = 0.f;
					mBlend = 0.f;
					mRecalagePret = false;
				}
			}

			bool NkRagdollBlend::TakeRelocation(math::NkMat4f &out) {
				if (!mRelocActive)
					return false;
				out = mReloc;
				mReloc = NkMat4f::Identity();
				mRelocActive = false;
				return true;
			}

			void NkRagdollBlend::Melanger(const NkRtRig &rig, const math::NkMat4f *a, const math::NkMat4f *b, float32 t,
										  math::NkMat4f *out) {
				rig.WorldToLocal(a, mLa.Data());
				rig.WorldToLocal(b, mLb.Data());
				for (uint32 j = 0; j < rig.Count(); ++j)
					mLa[j] = BlendMat(mLa[j], mLb[j], t);
				rig.LocalToWorld(mLa.Data(), out);
			}

			void NkRagdollBlend::CalculerRecalage(const NkRtRig &rig, const math::NkMat4f *anim) {
				// Le bassin = la premiere racine.
				uint32 bassin = 0;
				for (uint32 j = 0; j < rig.Count(); ++j) {
					if (rig.Parent(j) < 0) {
						bassin = j;
						break;
					}
				}
				const NkVec3f av0 = ragdoll.BindForward();
				const NkRtQuat qAnim = QFromMat(anim[bassin]);
				const NkRtQuat qRag = QFromMat(mFrozen[bassin]);
				// L'avant et le haut du corps, au repos -> dans chaque pose (par le bassin).
				const NkVec3f haut0 = V3(0.f, 1.f, 0.f);
				const NkRtQuat qBind = QFromMat(mBindPelvis);
				const NkRtQuat versAnim = QMul(qAnim, QConj(qBind));
				const NkRtQuat versRag = QMul(qRag, QConj(qBind));
				const NkVec3f avAnim = QRotate(versAnim, av0);
				const NkVec3f avRag = QRotate(versRag, av0);
				const NkVec3f hautRag = QRotate(versRag, haut0);
				mFaceUp = avRag.y > 0.f;
				// Le cap du ragdoll couche : le long du corps (vers la tete sur le ventre,
				// vers les pieds sur le dos) ; debout ou assis : son avant.
				NkVec3f cap = V3(avRag.x, 0.f, avRag.z);
				if (std::fabs(avRag.y) > 0.6f) {
					cap = V3(hautRag.x, 0.f, hautRag.z);
					if (mFaceUp)
						cap = Scale(cap, -1.f);
				}
				cap = Normalize(cap, V3(avAnim.x, 0.f, avAnim.z));
				const NkVec3f capAnim = Normalize(V3(avAnim.x, 0.f, avAnim.z), cap);
				const float32 lacet = std::atan2(Dot(Cross(capAnim, cap), V3(0.f, 1.f, 0.f)), Dot(capAnim, cap));
				const NkVec3f pa = Origin(anim[bassin]), pr = Origin(mFrozen[bassin]);
				// delta = T(pr.xz) * Ry(lacet) * T(-pa.xz)
				const NkRtQuat ry = QFromAxisAngle(V3(0.f, 1.f, 0.f), lacet);
				NkMat4f delta = MatFrom(ry, V3(1.f, 1.f, 1.f), V3(0.f, 0.f, 0.f));
				const NkVec3f t = Sub(V3(pr.x, 0.f, pr.z), QRotate(ry, V3(pa.x, 0.f, pa.z)));
				SetOrigin(delta, t);
				mReloc = delta * mReloc;
				mRelocActive = true;
				mDeltaFrame = delta;
			}

			void NkRagdollBlend::Update(const NkRtRig &rig, math::NkMat4f *world, float32 dt, const NkRtGround &ground) {
				const uint32 n = rig.Count();
				switch (mPhase) {
					case NkRagdollPhase::Animated:
						return;
					case NkRagdollPhase::Falling:
					case NkRagdollPhase::Ragdoll: {
						ragdoll.Step(dt, ground);
						ragdoll.ReadPose(mRag.Data());
						mTime += dt;
						if (mPhase == NkRagdollPhase::Falling) {
							mBlend = settings.fadeIn > 1e-5f ? Clamp(mTime / settings.fadeIn, 0.f, 1.f) : 1.f;
							if (mBlend >= 1.f) {
								mPhase = NkRagdollPhase::Ragdoll;
								mTime = 0.f;
							}
						} else {
							mBlend = 1.f;
						}
						if (mBlend >= 1.f) {
							for (uint32 j = 0; j < n; ++j)
								world[j] = mRag[j];
						} else {
							Melanger(rig, world, mRag.Data(), SmoothStep(0.f, 1.f, mBlend), mTmp.Data());
							for (uint32 j = 0; j < n; ++j)
								world[j] = mTmp[j];
						}
						if (mPhase == NkRagdollPhase::Ragdoll && settings.autoGetUp &&
							ragdoll.RestTime() >= settings.getUpDelay) {
							for (uint32 j = 0; j < n; ++j)
								mFrozen[j] = mRag[j];
							mPhase = NkRagdollPhase::GettingUp;
							mTime = 0.f;
							mBlend = 0.f;
							mRecalagePret = false;
						}
						return;
					}
					case NkRagdollPhase::GettingUp: {
						if (!mRecalagePret) {
							CalculerRecalage(rig, world);
							// Cette image : la pose entrante n'a pas encore le nouveau recalage.
							for (uint32 j = 0; j < n; ++j)
								world[j] = mDeltaFrame * world[j];
							mRecalagePret = true;
						}
						mTime += dt;
						mBlend = settings.getUpDuration > 1e-5f ? Clamp(mTime / settings.getUpDuration, 0.f, 1.f) : 1.f;
						const float32 w = SmoothStep(0.f, 1.f, mBlend);
						if (mBlend >= 1.f) {
							mPhase = NkRagdollPhase::Animated;
							mTime = 0.f;
							if (NkRtMutated(NK_RT_MUT_GETUP_SNAP)) {
								// MUTATION : le recalage est oublie a la fin -- la pose saute.
								mReloc = NkMat4f::Identity();
								mRelocActive = false;
							}
							return;
						}
						Melanger(rig, mFrozen.Data(), world, w, mTmp.Data());
						for (uint32 j = 0; j < n; ++j)
							world[j] = mTmp[j];
						return;
					}
				}
			}

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
