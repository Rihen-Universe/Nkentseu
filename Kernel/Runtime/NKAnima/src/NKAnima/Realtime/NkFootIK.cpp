// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkFootIK.cpp — l'IK des pieds sur sol irregulier (voir l'en-tete).
// =============================================================================
#include "NKAnima/Realtime/NkFootIK.h"

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
				bool EvoquePied(const char *nom) {
					return Contient(nom, "foot") || Contient(nom, "ankle") || Contient(nom, "pied") ||
						   Contient(nom, "cheville");
				}
			} // namespace

			void NkRtSmoothDamp(float32 &value, float32 &velocity, float32 target, float32 smoothTime, float32 dt) {
				if (smoothTime <= 1e-5f || dt <= 0.f) {
					value = target;
					velocity = 0.f;
					return;
				}
				const float32 omega = 2.f / smoothTime;
				const float32 x = omega * dt;
				const float32 e = 1.f / (1.f + x + 0.48f * x * x + 0.235f * x * x * x);
				const float32 change = value - target;
				const float32 temp = (velocity + omega * change) * dt;
				velocity = (velocity - omega * temp) * e;
				value = target + (change + temp) * e;
			}

			float32 NkRtSolveTwoBone(const NkRtRig &rig, const math::NkMat4f *local, math::NkMat4f *world, uint32 a,
									 uint32 b, uint32 c, const NkVec3f &target, const NkVec3f &poleDir) {
				const NkVec3f A = Origin(world[a]), B = Origin(world[b]), C = Origin(world[c]);
				const float32 l1 = Distance(A, B), l2 = Distance(B, C);
				if (l1 < 1e-6f || l2 < 1e-6f)
					return Distance(C, target);
				const NkVec3f versCible = Sub(target, A);
				float32 d = Length(versCible);
				const NkVec3f dir = Normalize(versCible, Normalize(Sub(C, A), V3(0.f, -1.f, 0.f)));
				const float32 dMax = (l1 + l2) * 0.99995f;
				const float32 dMin = std::fabs(l1 - l2) * 1.0001f + 1e-5f;
				d = Clamp(d, dMin, dMax);
				const NkVec3f pole = Sub(poleDir, Scale(dir, Dot(poleDir, dir)));
				const NkVec3f n = Normalize(pole, AnyPerpendicular(dir));
				const float32 cosA = Clamp((l1 * l1 + d * d - l2 * l2) / (2.f * l1 * d), -1.f, 1.f);
				const float32 sinA = std::sqrt(1.f - cosA * cosA);
				const NkVec3f B2 = Add(A, Add(Scale(dir, l1 * cosA), Scale(n, l1 * sinA)));
				const NkVec3f C2 = Madd(A, dir, d);
				RotateAbout(world[a], QFromTo(Sub(B, A), Sub(B2, A)), A);
				rig.RefreshDescendants(a, local, world);
				const NkVec3f Bn = Origin(world[b]), Cn = Origin(world[c]);
				RotateAbout(world[b], QFromTo(Sub(Cn, Bn), Sub(C2, Bn)), Bn);
				rig.RefreshDescendants(b, local, world);
				return Distance(Origin(world[c]), target);
			}

			uint32 NkFootIK::Setup(const NkRtRig &rig, const math::NkMat4f *bindWorld, int32 pelvis) {
				mRig = &rig;
				mLegs.Clear();
				mBind.Clear();
				mPelvisJoint = -1;
				Reset();
				if (bindWorld == nullptr || rig.Count() == 0u)
					return 0u;
				mBind.Resize(rig.Count());
				mBindFloor = 1e30f;
				for (uint32 j = 0; j < rig.Count(); ++j) {
					mBind[j] = bindWorld[j];
					if (bindWorld[j].m31 < mBindFloor)
						mBindFloor = bindWorld[j].m31;
				}
				for (uint32 j = 0; j < rig.Count(); ++j) {
					if (!EvoquePied(rig.Name(j)))
						continue;
					const int32 genou = rig.Parent(j);
					if (genou < 0 || EvoquePied(rig.Name((uint32)genou)))
						continue;
					const int32 hanche = rig.Parent((uint32)genou);
					if (hanche < 0)
						continue;
					NkFootIKLeg l;
					l.hip = hanche;
					l.knee = genou;
					l.ankle = (int32)j;
					l.toe = rig.FirstChild(j);
					(void)AddLeg(l);
				}
				if (pelvis >= 0 && (uint32)pelvis < rig.Count()) {
					mPelvisJoint = pelvis;
				} else if (!mLegs.Empty()) {
					const int32 p = rig.Parent((uint32)mLegs[0].j.hip);
					mPelvisJoint = p >= 0 ? p : mLegs[0].j.hip;
				}
				return (uint32)mLegs.Size();
			}

			bool NkFootIK::AddLeg(const NkFootIKLeg &leg) {
				if (mRig == nullptr)
					return false;
				const NkRtRig &rig = *mRig;
				const uint32 n = rig.Count();
				if (leg.hip < 0 || leg.knee < 0 || leg.ankle < 0 || (uint32)leg.hip >= n || (uint32)leg.knee >= n ||
					(uint32)leg.ankle >= n)
					return false;
				// La chaine doit descendre : genou sous la hanche, cheville sous le genou.
				if (!rig.IsInSubtree((uint32)leg.knee, (uint32)leg.hip) || !rig.IsInSubtree((uint32)leg.ankle, (uint32)leg.knee))
					return false;
				Leg l;
				l.j = leg;
				if (l.j.toe >= 0 && ((uint32)l.j.toe >= n || !rig.IsInSubtree((uint32)l.j.toe, (uint32)leg.ankle)))
					l.j.toe = -1;
				l.restAnkle = mBind[(uint32)leg.ankle].m31 - mBindFloor;
				l.reach = Distance(Origin(mBind[(uint32)leg.hip]), Origin(mBind[(uint32)leg.knee])) +
						  Distance(Origin(mBind[(uint32)leg.knee]), Origin(mBind[(uint32)leg.ankle]));
				mLegs.PushBack(l);
				if (mPelvisJoint < 0) {
					const int32 p = rig.Parent((uint32)leg.hip);
					mPelvisJoint = p >= 0 ? p : leg.hip;
				}
				return true;
			}

			void NkFootIK::Reset() {
				mPelvis = 0.f;
				mPelvisVel = 0.f;
				mPrimed = false;
				for (uint32 i = 0; i < (uint32)mLegs.Size(); ++i) {
					mLegs[i].off = 0.f;
					mLegs[i].offVel = 0.f;
				}
			}

			void NkFootIK::Relax() {
				Reset();
				mPrimed = true;
			}

			void NkFootIK::Apply(const NkRtRig &rig, const math::NkMat4f *local, math::NkMat4f *world,
								 const math::NkMat4f *footSource, float32 groundRefY, const NkRtGround &ground, float32 dt) {
				if (mLegs.Empty() || mPelvisJoint < 0 || NkRtMutated(NK_RT_MUT_FOOTIK_OFF))
					return;
				const NkFootIKSettings &s = settings;
				const float32 poids = Clamp(s.weight, 0.f, 1.f);
				if (poids <= 0.f)
					return;
				const math::NkMat4f *src = footSource != nullptr ? footSource : world;
				const NkVec3f haut = V3(0.f, 1.f, 0.f);
				const NkVec3f bas = V3(0.f, -1.f, 0.f);
				const float32 longueur = s.maxStepUp + s.maxStepDown;

				// 1. Le sol sous chaque pied.
				bool unAppui = false;
				float32 plusBas = 1e30f;
				for (uint32 i = 0; i < (uint32)mLegs.Size(); ++i) {
					Leg &l = mLegs[i];
					const NkVec3f a = Origin(src[(uint32)l.j.ankle]);
					NkRtRayHit h;
					bool touche = ground.Cast(V3(a.x, groundRefY + s.maxStepUp, a.z), bas, longueur, h);
					NkVec3f point = h.point, normale = h.normal;
					if (l.j.toe >= 0) {
						const NkVec3f t = Origin(src[(uint32)l.j.toe]);
						NkRtRayHit ht;
						if (ground.Cast(V3(t.x, groundRefY + s.maxStepUp, t.z), bas, longueur, ht)) {
							if (!touche || ht.point.y > point.y) {
								point = ht.point;
								normale = touche ? Normalize(Add(normale, ht.normal), haut) : ht.normal;
							} else {
								normale = Normalize(Add(normale, ht.normal), haut);
							}
							touche = true;
						}
					}
					const float32 cible = touche ? point.y - groundRefY : 0.f;
					const float32 levee = (a.y - groundRefY) - l.restAnkle;
					l.st.grounded = touche;
					l.st.groundPoint = point;
					l.st.groundNormal = touche ? Normalize(normale, haut) : haut;
					l.st.plant = 1.f - SmoothStep(s.plantLow, s.plantHigh, levee);
					if (!mPrimed) {
						l.off = cible;
						l.offVel = 0.f;
					} else {
						NkRtSmoothDamp(l.off, l.offVel, cible, s.footSmoothTime, dt);
					}
					l.st.offset = l.off;
					if (l.st.plant >= 0.5f) {
						unAppui = true;
						if (cible < plusBas)
							plusBas = cible;
					}
				}

				// 2. Le bassin : du plus petit decalage des pieds en appui.
				float32 bassin = unAppui ? plusBas : 0.f;
				// ... et assez pour que chaque jambe en appui ATTEIGNE sa cible : une
				// jambe tendue dont la hanche a glisse (l'equilibre) ne la touche plus.
				for (uint32 i = 0; i < (uint32)mLegs.Size() && unAppui; ++i) {
					const Leg &l = mLegs[i];
					if (l.st.plant < 0.5f)
						continue;
					const NkVec3f H0 = Origin(world[(uint32)l.j.hip]);
					const NkVec3f T = Madd(Origin(src[(uint32)l.j.ankle]), haut, l.off * poids);
					const float32 lmax = 0.995f * l.reach;
					const float32 dx = H0.x - T.x, dz = H0.z - T.z;
					const float32 r2 = dx * dx + dz * dz;
					if (r2 < lmax * lmax) {
						const float32 pMax = T.y - H0.y + std::sqrt(lmax * lmax - r2);
						if (pMax < bassin)
							bassin = pMax;
					}
				}
				bassin = Clamp(bassin, -s.maxPelvisDrop, s.maxPelvisRise);
				if (NkRtMutated(NK_RT_MUT_FOOTIK_NO_PELVIS))
					bassin = 0.f;
				if (!mPrimed) {
					mPelvis = bassin * poids;
					mPelvisVel = 0.f;
				} else {
					NkRtSmoothDamp(mPelvis, mPelvisVel, bassin * poids, s.pelvisSmoothTime, dt);
				}
				const uint32 pj = (uint32)mPelvisJoint;
				world[pj].m31 += mPelvis;
				rig.RefreshDescendants(pj, local, world);

				// 3. et 4. Chaque jambe, puis son pied.
				for (uint32 i = 0; i < (uint32)mLegs.Size(); ++i) {
					Leg &l = mLegs[i];
					const uint32 ih = (uint32)l.j.hip, ik = (uint32)l.j.knee, ia = (uint32)l.j.ankle;
					const NkVec3f aAnim = Origin(src[ia]);
					const NkVec3f cible = Madd(aAnim, haut, l.off * poids);
					// Le cote du genou : le plan de la pose (sinon les orteils, sinon +Z).
					const NkVec3f A = Origin(world[ih]), B = Origin(world[ik]), C = Origin(world[ia]);
					const NkVec3f dir = Normalize(Sub(C, A), bas);
					NkVec3f pole = Sub(Sub(B, A), Scale(dir, Dot(Sub(B, A), dir)));
					if (Length(pole) < 0.01f * Distance(A, B)) {
						NkVec3f orteil = V3(0.f, 0.f, 1.f);
						if (l.j.toe >= 0)
							orteil = Sub(Origin(world[(uint32)l.j.toe]), C);
						pole = Sub(orteil, Scale(dir, Dot(orteil, dir)));
					}
					l.st.reachError = NkRtSolveTwoBone(rig, local, world, ih, ik, ia, cible, pole);
					l.st.target = cible;
					// Le pied : son orientation animee, puis la pente au prorata de l'appui.
					const NkRtQuat qRetour = QMul(QFromMat(src[ia]), QConj(QFromMat(world[ia])));
					NkRtQuat qPente = QFromTo(haut, l.st.groundNormal);
					const float32 angle = QAngle(qPente);
					float32 part = l.st.plant * poids;
					if (angle > s.maxFootTilt && angle > 1e-6f)
						part *= s.maxFootTilt / angle;
					qPente = QScale(qPente, part);
					RotateAbout(world[ia], QMul(qPente, qRetour), Origin(world[ia]));
					rig.RefreshDescendants(ia, local, world);
				}
				mPrimed = true;
			}

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
