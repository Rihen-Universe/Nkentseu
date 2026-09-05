// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkCloth.cpp — tissu XPBD sur le CPU. Formules et sources : NkCloth.h.
// =============================================================================
#include "NKPhysics/NkCloth.h"
#include "NKPhysics/NkPhysicsWorld.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu {
	namespace physics {

		using nkentseu::math::NkSqrt;

		// ── Construction ─────────────────────────────────────────────────────
		void NkCloth::Clear() {
			mPos.Clear();
			mPrev.Clear();
			mVel.Clear();
			mNormal.Clear();
			mInvMass.Clear();
			mMass.Clear();
			mContact.Clear();
			mContactN.Clear();
			mCA.Clear();
			mCB.Clear();
			mRest.Clear();
			mLambda.Clear();
			mKind.Clear();
			mAdjStart.Clear();
			mAdjIdx.Clear();
			mPairA.Clear();
			mPairB.Clear();
			mAdjDirty = true;
			mGridW = mGridH = 0;
			mStats = NkClothStats{};
		}

		uint32 NkCloth::AddParticle(const NkVec3f &p, float32 mass) {
			mPos.PushBack(p);
			mPrev.PushBack(p);
			mVel.PushBack(NkVec3f{0.f, 0.f, 0.f});
			mNormal.PushBack(NkVec3f{0.f, 1.f, 0.f});
			mMass.PushBack(mass);
			mInvMass.PushBack(mass > 0.f ? 1.f / mass : 0.f);
			mContact.PushBack((uint8)0);
			mContactN.PushBack(NkVec3f{0.f, 1.f, 0.f});
			return (uint32)mPos.Size() - 1u;
		}

		void NkCloth::AddDistance(uint32 a, uint32 b, Kind kind) {
			if (a >= (uint32)mPos.Size() || b >= (uint32)mPos.Size() || a == b)
				return;
			mCA.PushBack(a);
			mCB.PushBack(b);
			mRest.PushBack((mPos[a] - mPos[b]).Len());
			mLambda.PushBack(0.f);
			mKind.PushBack((uint8)kind);
			mAdjDirty = true;
		}

		void NkCloth::BuildGrid(uint32 nx, uint32 ny, const NkVec3f &origin, const NkVec3f &du, const NkVec3f &dv,
								float32 totalMass) {
			Clear();
			if (nx < 2 || ny < 2)
				return;
			mGridW = nx;
			mGridH = ny;
			const float32 m = totalMass / (float32)(nx * ny);
			for (uint32 j = 0; j < ny; ++j)
				for (uint32 i = 0; i < nx; ++i)
					AddParticle(origin + du * (float32)i + dv * (float32)j, m);
			// structurelles : arêtes en i et en j
			for (uint32 j = 0; j < ny; ++j)
				for (uint32 i = 0; i < nx; ++i) {
					if (i + 1 < nx)
						AddDistance(GridIndex(i, j), GridIndex(i + 1, j), STRUCTURAL);
					if (j + 1 < ny)
						AddDistance(GridIndex(i, j), GridIndex(i, j + 1), STRUCTURAL);
				}
			// cisaillement : les deux diagonales de chaque quad
			for (uint32 j = 0; j + 1 < ny; ++j)
				for (uint32 i = 0; i + 1 < nx; ++i) {
					AddDistance(GridIndex(i, j), GridIndex(i + 1, j + 1), SHEAR);
					AddDistance(GridIndex(i + 1, j), GridIndex(i, j + 1), SHEAR);
				}
			// flexion : sommets à deux arêtes d'écart (Provot 1995)
			for (uint32 j = 0; j < ny; ++j)
				for (uint32 i = 0; i < nx; ++i) {
					if (i + 2 < nx)
						AddDistance(GridIndex(i, j), GridIndex(i + 2, j), BEND);
					if (j + 2 < ny)
						AddDistance(GridIndex(i, j), GridIndex(i, j + 2), BEND);
				}
		}

		void NkCloth::Pin(uint32 i, bool pinned) {
			if (i >= (uint32)mPos.Size())
				return;
			mInvMass[i] = pinned ? 0.f : (mMass[i] > 0.f ? 1.f / mMass[i] : 0.f);
			if (pinned)
				mVel[i] = {0.f, 0.f, 0.f};
		}

		void NkCloth::SetPosition(uint32 i, const NkVec3f &p) {
			if (i >= (uint32)mPos.Size())
				return;
			mPos[i] = p;
			mPrev[i] = p;
			mVel[i] = {0.f, 0.f, 0.f};
		}

		void NkCloth::AddCollidersFromWorld(const NkPhysicsWorld &world, uint32 layerMask) {
			const NkVector<NkRigidBody> &bodies = world.Bodies();
			for (uint32 k = 0; k < (uint32)bodies.Size(); ++k) {
				const NkRigidBody &b = bodies[k];
				if ((b.layer & layerMask) == 0)
					continue;
				// la même transformation que la synchronisation du monde (NkRigidBody.h)
				colliders.PushBack(NkTransformShape(b.restShape, b.position, b.orientation));
			}
		}

		void NkCloth::BuildAdjacency() {
			const uint32 n = (uint32)mPos.Size(), nc = (uint32)mCA.Size();
			mAdjStart.Resize(n + 1);
			uint32 *S = mAdjStart.Data();
			for (uint32 i = 0; i <= n; ++i)
				S[i] = 0;
			for (uint32 c = 0; c < nc; ++c) {
				++S[mCA[c] + 1];
				++S[mCB[c] + 1];
			}
			for (uint32 i = 0; i < n; ++i)
				S[i + 1] += S[i];
			mAdjIdx.Resize(S[n]);
			NkVector<uint32> fill;
			fill.Resize(n, 0u);
			uint32 *A = mAdjIdx.Data(), *F = fill.Data();
			for (uint32 c = 0; c < nc; ++c) {
				const uint32 a = mCA[c], b = mCB[c];
				A[S[a] + F[a]++] = b;
				A[S[b] + F[b]++] = a;
			}
			mAdjDirty = false;
		}

		bool NkCloth::Adjacent(uint32 a, uint32 b) const noexcept {
			const uint32 *S = mAdjStart.Data(), *A = mAdjIdx.Data();
			for (uint32 q = S[a]; q < S[a + 1]; ++q)
				if (A[q] == b)
					return true;
			return false;
		}

		// ── Topologie de dessin ──────────────────────────────────────────────
		void NkCloth::Triangles(NkVector<uint32> &out) const {
			out.Clear();
			if (mGridW < 2 || mGridH < 2)
				return;
			out.Reserve((mGridW - 1) * (mGridH - 1) * 6);
			for (uint32 j = 0; j + 1 < mGridH; ++j)
				for (uint32 i = 0; i + 1 < mGridW; ++i) {
					const uint32 a = GridIndex(i, j), b = GridIndex(i + 1, j), c = GridIndex(i + 1, j + 1),
								 d = GridIndex(i, j + 1);
					out.PushBack(a);
					out.PushBack(c);
					out.PushBack(b);
					out.PushBack(a);
					out.PushBack(d);
					out.PushBack(c);
				}
		}

		void NkCloth::ComputeNormals(NkVector<NkVec3f> &out) const {
			const uint32 n = (uint32)mPos.Size();
			out.Resize(n, NkVec3f{0.f, 0.f, 0.f});
			NkVec3f *N = out.Data();
			for (uint32 i = 0; i < n; ++i)
				N[i] = {0.f, 0.f, 0.f};
			const NkVec3f *X = mPos.Data();
			if (mGridW >= 2 && mGridH >= 2) {
				for (uint32 j = 0; j + 1 < mGridH; ++j)
					for (uint32 i = 0; i + 1 < mGridW; ++i) {
						const uint32 a = GridIndex(i, j), b = GridIndex(i + 1, j), c = GridIndex(i + 1, j + 1),
									 d = GridIndex(i, j + 1);
						const NkVec3f n1 = (X[c] - X[a]).Cross(X[b] - X[a]);
						const NkVec3f n2 = (X[d] - X[a]).Cross(X[c] - X[a]);
						N[a] += n1 + n2;
						N[b] += n1;
						N[c] += n1 + n2;
						N[d] += n2;
					}
			}
			for (uint32 i = 0; i < n; ++i) {
				const float32 l = N[i].Len();
				N[i] = (l > 1e-12f) ? N[i] * (1.f / l) : NkVec3f{0.f, 1.f, 0.f};
			}
		}

		// ── Un pas ───────────────────────────────────────────────────────────
		void NkCloth::Step(float32 dt, float32 time) {
			const uint32 n = (uint32)mPos.Size();
			if (n == 0 || dt <= 0.f)
				return;
			if (mAdjDirty)
				BuildAdjacency();
			const uint32 sub = params.substeps > 0 ? params.substeps : 1;
			const uint32 iters = params.iterations > 0 ? params.iterations : 1;
			const float32 h = dt / (float32)sub;
			mStats.contacts = 0;
			mStats.selfContacts = 0;
			mStats.selfBuilds = 0;
			const float32 margin = 2.f * params.thickness; // rayon de recherche 4r : marge 2r au-delà du contact
			mPairDrift = margin; // force une construction au premier sous-pas du pas
			for (uint32 s = 0; s < sub; ++s) {
				if (params.selfCollision) {
					// dérive maximale de deux particules l'une vers l'autre depuis la dernière liste
					mPairDrift += 2.f * mSubVmax * h;
					if (mPairDrift >= margin) {
						BuildSelfPairs();
						mPairDrift = 0.f;
					}
				}
				Predict(h, time + (float32)s * h);
				float32 *L = mLambda.Data();
				for (uint32 c = 0; c < (uint32)mLambda.Size(); ++c)
					L[c] = 0.f; // XPBD 2016 §3.3 : lambda repart de zéro à chaque sous-pas (le chaud est instable, en-tête)
				uint8 *CT = mContact.Data();
				for (uint32 i = 0; i < n; ++i)
					CT[i] = 0;
				for (uint32 it = 0; it < iters; ++it) {
					SolveDistances(h);
					if (params.selfCollision)
						SolveSelf();
					if (params.collisions)
						SolveColliders(); // en dernier : l'état final ne pénètre pas
				}
				UpdateVelocities(h);
			}
			mStats.substeps = sub;
			mStats.iterations = iters;
			mStats.dt = dt;
			Measure(dt);
		}

		void NkCloth::Predict(float32 h, float32 time) {
			const uint32 n = (uint32)mPos.Size();
			NkVec3f *X = mPos.Data(), *P = mPrev.Data(), *V = mVel.Data();
			const float32 *W = mInvMass.Data(), *M = mMass.Data();
			const NkVec3f g = params.gravity;
			const bool wind = forceField != nullptr;
			if (wind && params.forceOnNormal)
				ComputeNormals(mNormal);
			const NkVec3f *NN = mNormal.Data();
			for (uint32 i = 0; i < n; ++i) {
				P[i] = X[i];
				if (W[i] <= 0.f) {
					V[i] = {0.f, 0.f, 0.f};
					continue;
				}
				NkVec3f a = g;
				if (wind) {
					NkVec3f f = forceField->Force(X[i], time);
					if (params.forceOnNormal)
						f = NN[i] * NN[i].Dot(f);
					a += f * (1.f / M[i]);
				}
				V[i] += a * h;
				X[i] += V[i] * h;
			}
		}

		void NkCloth::SolveDistances(float32 h) {
			const uint32 nc = (uint32)mCA.Size();
			NkVec3f *X = mPos.Data();
			const float32 *W = mInvMass.Data();
			const uint32 *A = mCA.Data(), *B = mCB.Data();
			const float32 *R = mRest.Data();
			const uint8 *K = mKind.Data();
			float32 *L = mLambda.Data();
			const float32 invH2 = 1.f / (h * h);
			const float32 at[3] = {params.compliance * invH2, params.shearCompliance * invH2,
								   params.bendCompliance * invH2};
			const bool xpbd = params.xpbd;
			for (uint32 c = 0; c < nc; ++c) {
				const uint32 a = A[c], b = B[c];
				const float32 wa = W[a], wb = W[b];
				const float32 wsum = wa + wb;
				if (wsum <= 0.f)
					continue;
				NkVec3f d = X[a] - X[b];
				const float32 l = d.Len();
				if (l < 1e-9f)
					continue;
				const float32 C = l - R[c];
				float32 dl;
				if (xpbd) {
					// XPBD éq. 18 : dlambda = (-C - alpha~ lambda) / (w_a + w_b + alpha~)
					const float32 alpha = at[K[c]];
					dl = (-C - alpha * L[c]) / (wsum + alpha);
					L[c] += dl;
				} else {
					// MUTATION PBD pur (Müller 2007 §4.1, k = 1) : la compliance est ignorée
					dl = -C / wsum;
				}
				const NkVec3f nrm = d * (1.f / l);
				X[a] += nrm * (wa * dl);
				X[b] -= nrm * (wb * dl);
			}
		}

		// Frottement EN POSITION (Macklin, Müller, Chentanez, Kim, « Unified Particle
		// Physics for Real-Time Applications », SIGGRAPH 2014, §6.1, éq. 23) : après la
		// projection d'un contact de profondeur d, le déplacement TANGENTIEL depuis le
		// début du sous-pas, dx_t, est annulé s'il est plus petit que mu d (frottement
		// STATIQUE : le drapé tient sur la sphère), sinon réduit de mu d (cinétique).
		// Mesuré avant (04/09 nuit) avec un frottement en VITESSE (v_t *= 1 - mu) : la
		// nappe posée sur la sphère glissait de 3 cm/s et tombait à t = 4 s -- un
		// frottement en vitesse ne sait pas produire d'adhérence.
		static NK_FORCE_INLINE NkVec3f NkClothFriction(const NkVec3f &dx, const NkVec3f &nrm, float32 depth, float32 mu) noexcept {
			const NkVec3f dxT = dx - nrm * dx.Dot(nrm);
			const float32 lt = dxT.Len();
			if (lt < 1e-12f)
				return {0.f, 0.f, 0.f};
			const float32 lim = mu * depth;
			if (lt <= lim)
				return dxT * -1.f;			  // statique : tout le glissement est annulé
			return dxT * (-lim / lt);	  // cinétique : réduit de mu d
		}

		void NkCloth::SolveColliders() {
			const uint32 n = (uint32)mPos.Size(), nk = (uint32)colliders.Size();
			if (nk == 0)
				return;
			NkVec3f *X = mPos.Data();
			const NkVec3f *P = mPrev.Data();
			const float32 *W = mInvMass.Data();
			uint8 *CT = mContact.Data();
			NkVec3f *CN = mContactN.Data();
			const float32 r = params.thickness;
			const float32 mu = params.friction < 0.f ? 0.f : params.friction;
			uint32 ignored = 0, contacts = 0;
			for (uint32 k = 0; k < nk; ++k) {
				const collision::NkShape &s = colliders[k];
				switch (s.type) {
					case collision::NkShapeType::NK_SPHERE:
					case collision::NkShapeType::NK_CAPSULE3D: {
						const bool cap = s.type == collision::NkShapeType::NK_CAPSULE3D;
						const NkVec3f ab = s.p1 - s.p0;
						const float32 ab2 = ab.Dot(ab);
						const float32 R = s.radius + r;
						for (uint32 i = 0; i < n; ++i) {
							if (W[i] <= 0.f)
								continue;
							NkVec3f c = s.p0;
							if (cap && ab2 > 1e-12f) {
								float32 t = (X[i] - s.p0).Dot(ab) / ab2;
								t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
								c = s.p0 + ab * t;
							}
							NkVec3f d = X[i] - c;
							const float32 l = d.Len();
							if (l < R) {
								const NkVec3f nrm = (l > 1e-9f) ? d * (1.f / l) : NkVec3f{0.f, 1.f, 0.f};
								X[i] = c + nrm * R;
								X[i] += NkClothFriction(X[i] - P[i], nrm, R - l, mu);
								CT[i] = 1;
								CN[i] = nrm;
								++contacts;
							}
						}
						break;
					}
					case collision::NkShapeType::NK_PLANE3D: {
						const NkVec3f nrm = s.p1;
						for (uint32 i = 0; i < n; ++i) {
							if (W[i] <= 0.f)
								continue;
							const float32 d = (X[i] - s.p0).Dot(nrm) - r;
							if (d < 0.f) {
								X[i] -= nrm * d;
								X[i] += NkClothFriction(X[i] - P[i], nrm, -d, mu);
								CT[i] = 1;
								CN[i] = nrm;
								++contacts;
							}
						}
						break;
					}
					case collision::NkShapeType::NK_BOX3D: {
						// boîte ALIGNÉE seulement (orientation identité) ; sinon comptée comme ignorée
						const nkentseu::math::NkQuatf &q = s.orientation;
						const bool aligned = (q.x == 0.f && q.y == 0.f && q.z == 0.f);
						if (!aligned) {
							++ignored;
							break;
						}
						const NkVec3f mn = s.p0 - s.p1 - NkVec3f{r, r, r}, mx = s.p0 + s.p1 + NkVec3f{r, r, r};
						for (uint32 i = 0; i < n; ++i) {
							if (W[i] <= 0.f)
								continue;
							const NkVec3f p = X[i];
							if (p.x <= mn.x || p.x >= mx.x || p.y <= mn.y || p.y >= mx.y || p.z <= mn.z || p.z >= mx.z)
								continue;
							// pousser le long de l'axe de moindre pénétration
							float32 best = p.x - mn.x;
							NkVec3f nrm = {-1.f, 0.f, 0.f};
							if (mx.x - p.x < best) {
								best = mx.x - p.x;
								nrm = {1.f, 0.f, 0.f};
							}
							if (p.y - mn.y < best) {
								best = p.y - mn.y;
								nrm = {0.f, -1.f, 0.f};
							}
							if (mx.y - p.y < best) {
								best = mx.y - p.y;
								nrm = {0.f, 1.f, 0.f};
							}
							if (p.z - mn.z < best) {
								best = p.z - mn.z;
								nrm = {0.f, 0.f, -1.f};
							}
							if (mx.z - p.z < best) {
								best = mx.z - p.z;
								nrm = {0.f, 0.f, 1.f};
							}
							X[i] += nrm * best;
							X[i] += NkClothFriction(X[i] - P[i], nrm, best, mu);
							CT[i] = 1;
							CN[i] = nrm;
							++contacts;
						}
						break;
					}
					default:
						++ignored;
						break;
				}
			}
			mStats.collidersIgnored = ignored;
			mStats.contacts = contacts;
		}

		void NkCloth::BuildSelfPairs() {
			const uint32 n = (uint32)mPos.Size();
			const NkVec3f *X = mPos.Data();
			const float32 *W = mInvMass.Data();
			// rayon de recherche = contact (2r) + marge (2r) : la liste reste valable tant que la
			// dérive cumulée 2 vmax h n'atteint pas la marge (Step)
			const float32 radius = 4.f * params.thickness;
			const float32 r2 = radius * radius;
			mPairRadius = radius;
			++mStats.selfBuilds;
			mHash.Build(X, n, radius);
			mPairA.Clear();
			mPairB.Clear();
			for (uint32 i = 0; i < n; ++i) {
				const NkVec3f pi = X[i];
				mHash.Query(pi, [&](uint32 j) {
					if (j <= i)
						return; // chaque paire une fois
					if (W[i] + W[j] <= 0.f)
						return;
					const NkVec3f d = pi - X[j];
					if (d.Dot(d) >= r2)
						return;
					if (Adjacent(i, j))
						return; // voisines de maillage : ce sont les contraintes qui les tiennent
					mPairA.PushBack(i);
					mPairB.PushBack(j);
				});
			}
			mStats.selfPairs = (uint32)mPairA.Size();
		}

		void NkCloth::SolveSelf() {
			NkVec3f *X = mPos.Data();
			const NkVec3f *P = mPrev.Data();
			const float32 *W = mInvMass.Data();
			const float32 mu = params.friction < 0.f ? 0.f : params.friction;
			const float32 dmin = 2.f * params.thickness;
			const float32 dmin2 = dmin * dmin;
			const uint32 np = (uint32)mPairA.Size();
			const uint32 *PA = mPairA.Data(), *PB = mPairB.Data();
			uint32 contacts = 0;
			for (uint32 q = 0; q < np; ++q) {
				const uint32 i = PA[q], j = PB[q];
				const float32 wsum = W[i] + W[j];
				NkVec3f d = X[i] - X[j];
				const float32 l2 = d.Dot(d);
				if (l2 >= dmin2 || l2 < 1e-18f)
					continue;
				const float32 l = NkSqrt(l2);
				// contrainte d'inégalité C = l - dmin < 0, rigide (Müller 2007 §4.4)
				const float32 dl = (dmin - l) / wsum;
				const NkVec3f nrm = d * (1.f / l);
				X[i] += nrm * (W[i] * dl);
				X[j] -= nrm * (W[j] * dl);
				// frottement en position sur le glissement RELATIF des deux pans (même règle)
				const NkVec3f f = NkClothFriction((X[i] - P[i]) - (X[j] - P[j]), nrm, dmin - l, mu);
				X[i] += f * (W[i] / wsum);
				X[j] -= f * (W[j] / wsum);
				++contacts;
			}
			mStats.selfContacts = contacts;
		}

		void NkCloth::UpdateVelocities(float32 h) {
			const uint32 n = (uint32)mPos.Size();
			const NkVec3f *X = mPos.Data(), *P = mPrev.Data();
			NkVec3f *V = mVel.Data();
			const float32 *W = mInvMass.Data();
			const float32 invH = 1.f / h;
			float32 damp = 1.f - params.damping * h;
			if (damp < 0.f)
				damp = 0.f;
			// le frottement est déjà dans les positions (NkClothFriction) : la vitesse le reflète
			float32 vmax2 = 0.f;
			for (uint32 i = 0; i < n; ++i) {
				if (W[i] <= 0.f) {
					V[i] = {0.f, 0.f, 0.f};
					continue;
				}
				V[i] = (X[i] - P[i]) * (invH * damp);
				const float32 v2 = V[i].Dot(V[i]);
				if (v2 > vmax2)
					vmax2 = v2;
			}
			mSubVmax = NkSqrt(vmax2);
		}

		// ── Mesure ───────────────────────────────────────────────────────────
		void NkCloth::Measure(float32 dt) {
			(void)dt;
			const uint32 n = (uint32)mPos.Size(), nc = (uint32)mCA.Size();
			const NkVec3f *X = mPos.Data(), *V = mVel.Data();
			const float32 *W = mInvMass.Data(), *M = mMass.Data();
			mStats.particles = n;
			mStats.constraints = nc;
			uint32 pinned = 0, ns = 0, nsh = 0, nb = 0;
			float32 mass = 0.f, ke = 0.f, pe = 0.f, vmax2 = 0.f;
			const NkVec3f g = params.gravity;
			for (uint32 i = 0; i < n; ++i) {
				if (W[i] <= 0.f)
					++pinned;
				mass += M[i];
				const float32 v2 = V[i].Dot(V[i]);
				ke += 0.5f * M[i] * v2;
				pe -= M[i] * g.Dot(X[i]);
				if (v2 > vmax2)
					vmax2 = v2;
			}
			float32 maxS = 0.f, sumS = 0.f;
			uint32 cnt = 0;
			for (uint32 c = 0; c < nc; ++c) {
				switch (mKind[c]) {
					case STRUCTURAL: ++ns; break;
					case SHEAR: ++nsh; break;
					default: ++nb; break;
				}
				if (mKind[c] != STRUCTURAL || mRest[c] <= 0.f)
					continue;
				const float32 l = (X[mCA[c]] - X[mCB[c]]).Len();
				float32 e = (l - mRest[c]) / mRest[c];
				if (e < 0.f)
					e = -e;
				if (e > maxS)
					maxS = e;
				sumS += e;
				++cnt;
			}
			mStats.pinned = pinned;
			mStats.structural = ns;
			mStats.shear = nsh;
			mStats.bend = nb;
			mStats.mass = mass;
			mStats.kineticEnergy = ke;
			mStats.potentialEnergy = pe;
			mStats.maxSpeed = NkSqrt(vmax2);
			mStats.maxStretch = maxS;
			mStats.meanStretch = cnt ? sumS / (float32)cnt : 0.f;
			// pénétration des CENTRES sous la surface des colliders (0 attendu après la projection)
			float32 pen = 0.f;
			for (uint32 k = 0; k < (uint32)colliders.Size(); ++k) {
				const collision::NkShape &s = colliders[k];
				if (s.type == collision::NkShapeType::NK_SPHERE || s.type == collision::NkShapeType::NK_CAPSULE3D) {
					const bool cap = s.type == collision::NkShapeType::NK_CAPSULE3D;
					const NkVec3f ab = s.p1 - s.p0;
					const float32 ab2 = ab.Dot(ab);
					for (uint32 i = 0; i < n; ++i) {
						NkVec3f c = s.p0;
						if (cap && ab2 > 1e-12f) {
							float32 t = (X[i] - s.p0).Dot(ab) / ab2;
							t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
							c = s.p0 + ab * t;
						}
						const float32 d = s.radius - (X[i] - c).Len();
						if (d > pen)
							pen = d;
					}
				} else if (s.type == collision::NkShapeType::NK_PLANE3D) {
					for (uint32 i = 0; i < n; ++i) {
						const float32 d = -(X[i] - s.p0).Dot(s.p1);
						if (d > pen)
							pen = d;
					}
				}
			}
			mStats.maxPenetration = pen;
			// distance minimale entre particules NON voisines (si auto-collision) : lue sur la table du dernier sous-pas
			float32 dminSelf = 0.f;
			if (params.selfCollision && n > 0) {
				// la table date du début du dernier sous-pas ; on la reconstruit sur l'état final (mesure, pas simulation)
				mHash.Build(X, n, 2.f * params.thickness);
				float32 best2 = 1e30f;
				for (uint32 i = 0; i < n; ++i) {
					mHash.Query(X[i], [&](uint32 j) {
						if (j <= i || Adjacent(i, j))
							return;
						const NkVec3f d = X[i] - X[j];
						const float32 l2 = d.Dot(d);
						if (l2 < best2)
							best2 = l2;
					});
				}
				// aucune paire dans le rayon de recherche (3 cellules) : on rend ce plancher, pas 0
				dminSelf = best2 < 1e30f ? NkSqrt(best2) : 3.f * 2.f * params.thickness;
			}
			mStats.minSelfDistance = dminSelf;
		}

	} // namespace physics
} // namespace nkentseu
