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
		using nkentseu::math::NkMin;
		using nkentseu::math::NkMax;

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
			mPairBase.Clear();
			mPinTarget.Clear();
			mPinStart.Clear();
			mPinHas.Clear();
			mTri.Clear();
			collidersPrev.Clear();
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
			mPinTarget.PushBack(p);
			mPinStart.PushBack(p);
			mPinHas.PushBack((uint8)0);
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

		void NkCloth::BuildPinRings() {
			const uint32 n = (uint32)mPos.Size();
			mPinRing.Resize(n);
			uint8 *R = mPinRing.Data();
			for (uint32 i = 0; i < n; ++i)
				R[i] = 255u;
			if (mAdjDirty)
				BuildAdjacency();
			// parcours en largeur depuis toutes les épingles, sur l'adjacence des contraintes
			NkVector<uint32> front, next;
			const float32 *W = mInvMass.Data();
			for (uint32 i = 0; i < n; ++i)
				if (W[i] <= 0.f) {
					R[i] = 0u;
					front.PushBack(i);
				}
			const uint32 *S = mAdjStart.Data(), *A = mAdjIdx.Data();
			uint8 ring = 0;
			while (!front.Empty() && ring < 254u) {
				++ring;
				next.Clear();
				for (uint32 q = 0; q < (uint32)front.Size(); ++q) {
					const uint32 a = front[q];
					for (uint32 e = S[a]; e < S[a + 1]; ++e) {
						const uint32 b = A[e];
						if (R[b] == 255u) {
							R[b] = ring;
							next.PushBack(b);
						}
					}
				}
				front.Clear();
				for (uint32 q = 0; q < (uint32)next.Size(); ++q)
					front.PushBack(next[q]);
			}
			mPinRingDirty = false;
		}

		void NkCloth::Pin(uint32 i, bool pinned) {
			if (i >= (uint32)mPos.Size())
				return;
			mInvMass[i] = pinned ? 0.f : (mMass[i] > 0.f ? 1.f / mMass[i] : 0.f);
			mPinRingDirty = true;
			if (pinned)
				mVel[i] = {0.f, 0.f, 0.f};
		}

		void NkCloth::SetPosition(uint32 i, const NkVec3f &p) {
			if (i >= (uint32)mPos.Size())
				return;
			mPos[i] = p;
			mPrev[i] = p;
			mVel[i] = {0.f, 0.f, 0.f};
			mPinTarget[i] = p;
			mPinStart[i] = p;
		}

		void NkCloth::SetPinTarget(uint32 i, const NkVec3f &target) {
			if (i >= (uint32)mPos.Size())
				return;
			if (mInvMass[i] > 0.f)
				Pin(i, true);
			mPinTarget[i] = target;
			mPinHas[i] = 1;
		}

		void NkCloth::ClearPinTarget(uint32 i) {
			if (i < (uint32)mPos.Size())
				mPinHas[i] = 0;
		}

		void NkCloth::SetTriangles(const uint32 *indices, uint32 count) {
			mTri.Clear();
			const uint32 n = (uint32)mPos.Size();
			for (uint32 k = 0; k + 2 < count; k += 3) {
				if (indices[k] < n && indices[k + 1] < n && indices[k + 2] < n) {
					mTri.PushBack(indices[k]);
					mTri.PushBack(indices[k + 1]);
					mTri.PushBack(indices[k + 2]);
				}
			}
		}

		void NkCloth::AddTriangle(uint32 a, uint32 b, uint32 c) {
			const uint32 n = (uint32)mPos.Size();
			if (a >= n || b >= n || c >= n)
				return;
			mTri.PushBack(a);
			mTri.PushBack(b);
			mTri.PushBack(c);
		}

		bool NkCloth::Bounds(NkVec3f &outMin, NkVec3f &outMax) const noexcept {
			const uint32 n = (uint32)mPos.Size();
			if (n == 0)
				return false;
			const NkVec3f *X = mPos.Data();
			NkVec3f mn = X[0], mx = X[0];
			for (uint32 i = 1; i < n; ++i) {
				const NkVec3f &q = X[i];
				if (q.x < mn.x) mn.x = q.x;
				if (q.y < mn.y) mn.y = q.y;
				if (q.z < mn.z) mn.z = q.z;
				if (q.x > mx.x) mx.x = q.x;
				if (q.y > mx.y) mx.y = q.y;
				if (q.z > mx.z) mx.z = q.z;
			}
			outMin = mn;
			outMax = mx;
			return true;
		}

		uint32 NkCloth::AppendGrid(uint32 nx, uint32 ny, const NkVec3f &origin, const NkVec3f &du, const NkVec3f &dv,
								   float32 totalMass, bool wrapU) {
			if (nx < 2 || ny < 2)
				return (uint32)mPos.Size();
			NkVector<NkVec3f> rows;
			rows.Resize(nx * ny);
			for (uint32 j = 0; j < ny; ++j)
				for (uint32 i = 0; i < nx; ++i)
					rows[j * nx + i] = origin + du * (float32)i + dv * (float32)j;
			return AppendPanel(nx, ny, rows.Data(), totalMass, wrapU);
		}

		uint32 NkCloth::AppendPanelMasked(uint32 nx, uint32 ny, const NkVec3f *rows, const uint8 *mask, float32 totalMass,
										  bool wrapU, NkVector<int32> *indexOf) {
			const uint32 first = (uint32)mPos.Size();
			if (nx < 2 || ny < 2 || !mask)
				return first;
			if (mGridW >= 2 && mGridH >= 2 && mTri.Empty())
				Triangles(mTri);
			mGridW = mGridH = 0;
			uint32 present = 0;
			for (uint32 k = 0; k < nx * ny; ++k)
				if (mask[k])
					++present;
			if (present == 0)
				return first;
			const float32 m = totalMass / (float32)present;
			NkVector<int32> map;
			map.Resize(nx * ny, -1);
			for (uint32 j = 0; j < ny; ++j)
				for (uint32 i = 0; i < nx; ++i)
					if (mask[j * nx + i])
						map[j * nx + i] = (int32)AddParticle(rows[j * nx + i], m);
			auto id = [&](uint32 i, uint32 j) -> int32 { return map[j * nx + (wrapU ? (i % nx) : i)]; };
			const uint32 lastI = wrapU ? nx : nx - 1;
			auto dist = [&](int32 a, int32 b, Kind k) {
				if (a >= 0 && b >= 0)
					AddDistance((uint32)a, (uint32)b, k);
			};
			for (uint32 j = 0; j < ny; ++j)
				for (uint32 i = 0; i < nx; ++i) {
					if (i < lastI)
						dist(id(i, j), id(i + 1, j), STRUCTURAL);
					if (j + 1 < ny)
						dist(id(i, j), id(i, j + 1), STRUCTURAL);
				}
			for (uint32 j = 0; j + 1 < ny; ++j)
				for (uint32 i = 0; i < lastI; ++i) {
					dist(id(i, j), id(i + 1, j + 1), SHEAR);
					dist(id(i + 1, j), id(i, j + 1), SHEAR);
				}
			for (uint32 j = 0; j < ny; ++j)
				for (uint32 i = 0; i < nx; ++i) {
					if ((wrapU ? (nx > 2) : (i + 2 < nx)) && id(i + 1, j) >= 0)
						dist(id(i, j), id(i + 2, j), BEND);
					if (j + 2 < ny && id(i, j + 1) >= 0)
						dist(id(i, j), id(i, j + 2), BEND);
				}
			for (uint32 j = 0; j + 1 < ny; ++j)
				for (uint32 i = 0; i < lastI; ++i) {
					const int32 a = id(i, j), b = id(i + 1, j), c = id(i + 1, j + 1), d = id(i, j + 1);
					if (a >= 0 && c >= 0 && b >= 0)
						AddTriangle((uint32)a, (uint32)c, (uint32)b);
					if (a >= 0 && d >= 0 && c >= 0)
						AddTriangle((uint32)a, (uint32)d, (uint32)c);
				}
			if (indexOf) {
				indexOf->Resize(nx * ny);
				for (uint32 k = 0; k < nx * ny; ++k)
					(*indexOf)[k] = map[k];
			}
			return first;
		}

		uint32 NkCloth::AppendPanel(uint32 nx, uint32 ny, const NkVec3f *rows, float32 totalMass, bool wrapU) {
			const uint32 first = (uint32)mPos.Size();
			if (nx < 2 || ny < 2)
				return first;
			// une grille explicite (BuildGrid) et des panneaux ne se mélangent pas : les triangles
			// passent par la liste explicite dès qu'un panneau existe
			if (mGridW >= 2 && mGridH >= 2 && mTri.Empty())
				Triangles(mTri);
			mGridW = mGridH = 0;
			const float32 m = totalMass / (float32)(nx * ny);
			for (uint32 j = 0; j < ny; ++j)
				for (uint32 i = 0; i < nx; ++i)
					AddParticle(rows[j * nx + i], m);
			auto id = [&](uint32 i, uint32 j) { return first + j * nx + (wrapU ? (i % nx) : i); };
			const uint32 lastI = wrapU ? nx : nx - 1; // arêtes en i : nx si fermé (la dernière rejoint la première)
			for (uint32 j = 0; j < ny; ++j)
				for (uint32 i = 0; i < nx; ++i) {
					if (i < lastI)
						AddDistance(id(i, j), id(i + 1, j), STRUCTURAL);
					if (j + 1 < ny)
						AddDistance(id(i, j), id(i, j + 1), STRUCTURAL);
				}
			for (uint32 j = 0; j + 1 < ny; ++j)
				for (uint32 i = 0; i < lastI; ++i) {
					AddDistance(id(i, j), id(i + 1, j + 1), SHEAR);
					AddDistance(id(i + 1, j), id(i, j + 1), SHEAR);
				}
			for (uint32 j = 0; j < ny; ++j)
				for (uint32 i = 0; i < nx; ++i) {
					if (wrapU ? (nx > 2) : (i + 2 < nx))
						AddDistance(id(i, j), id(i + 2, j), BEND);
					if (j + 2 < ny)
						AddDistance(id(i, j), id(i, j + 2), BEND);
				}
			for (uint32 j = 0; j + 1 < ny; ++j)
				for (uint32 i = 0; i < lastI; ++i) {
					const uint32 a = id(i, j), b = id(i + 1, j), c = id(i + 1, j + 1), d = id(i, j + 1);
					AddTriangle(a, c, b);
					AddTriangle(a, d, c);
				}
			return first;
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
			// bornes par type pour le profil : valables seulement si les contraintes sont groupées
			// (structurelles, puis cisaillement, puis flexion -- ce que BuildGrid produit)
			mKindsGrouped = true;
			uint32 last = 0;
			for (uint32 c = 0; c < nc; ++c) {
				if (mKind[c] < last)
					mKindsGrouped = false;
				last = mKind[c];
			}
			mKindStart[0] = 0;
			mKindStart[3] = nc;
			uint32 k = 1;
			for (uint32 c = 0; c < nc && k < 3; ++c)
				while (k < 3 && mKind[c] >= k)
					mKindStart[k++] = c;
			while (k < 3)
				mKindStart[k++] = nc;
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
			if (!mTri.Empty()) {
				out.Reserve((uint32)mTri.Size());
				for (uint32 k = 0; k < (uint32)mTri.Size(); ++k)
					out.PushBack(mTri[k]);
				return;
			}
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
			if (!mTri.Empty()) {
				const uint32 *T = mTri.Data();
				for (uint32 k = 0; k + 2 < (uint32)mTri.Size(); k += 3) {
					const uint32 a = T[k], b = T[k + 1], c = T[k + 2];
					const NkVec3f nn = (X[b] - X[a]).Cross(X[c] - X[a]);
					N[a] += nn;
					N[b] += nn;
					N[c] += nn;
				}
			} else if (mGridW >= 2 && mGridH >= 2) {
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
			if (mPinRingDirty || (uint32)mPinRing.Size() != n)
				BuildPinRings();
			const uint32 sub = params.substeps > 0 ? params.substeps : 1;
			const uint32 iters = params.iterations > 0 ? params.iterations : 1;
			const float32 h = dt / (float32)sub;
			float64 (*clk)() = params.clock;
			mProfile = NkClothProfile{};
			const float64 t0 = clk ? clk() : 0.0;
			float64 tm = t0;
			auto lap = [&](float64 &slot) {
				if (!clk)
					return;
				const float64 t = clk();
				slot += (t - tm) * 1000.0;
				tm = t;
			};
			mStats.contacts = 0;
			mStats.selfContacts = 0;
			mStats.selfBuilds = 0;
			mStats.collidersCulled = 0;
			// épingles à cible : la position de départ du pas
			{
				const NkVec3f *X0 = mPos.Data();
				NkVec3f *PS = mPinStart.Data();
				const uint8 *PH = mPinHas.Data();
				for (uint32 i = 0; i < n; ++i)
					if (PH[i])
						PS[i] = X0[i];
			}
			const float32 invDt = 1.f / dt;
			// marge de la liste de paires : au moins 2r, plus une part de ce que la nappe parcourt en un pas
			float32 margin = 2.f * params.thickness;
			const float32 mv = params.selfMarginK * mStats.maxSpeed * dt;
			if (mv > margin)
				margin = mv;
			for (uint32 s = 0; s < sub; ++s) {
				if (params.selfCollision && ((uint32)mPairBase.Size() != n || SelfPairsStale()))
					BuildSelfPairs(margin);
				lap(mProfile.selfBuild);
				const float32 alpha = (float32)(s + 1) / (float32)sub;
				Predict(h, time + (float32)s * h, alpha, invDt);
				if (params.collisions)
					PrepareColliderStep(alpha, invDt);
				const collision::NkShape *CS = mColStep.Data();
				const NkVec3f *CV0 = mColV0.Data(), *CV1 = mColV1.Data();
				const uint32 nk = (uint32)mColStep.Size();
				float32 *L = mLambda.Data();
				for (uint32 c = 0; c < (uint32)mLambda.Size(); ++c)
					L[c] = 0.f; // XPBD 2016 §3.3 : lambda repart de zéro à chaque sous-pas (le chaud est instable, en-tête)
				uint8 *CT = mContact.Data();
				for (uint32 i = 0; i < n; ++i)
					CT[i] = 0;
				lap(mProfile.predict);
				for (uint32 it = 0; it < iters; ++it) {
					if (clk && mKindsGrouped) {
						SolveDistances(h, mKindStart[0], mKindStart[1]);
						lap(mProfile.structural);
						SolveDistances(h, mKindStart[1], mKindStart[2]);
						lap(mProfile.shear);
						SolveDistances(h, mKindStart[2], mKindStart[3]);
						lap(mProfile.bend);
					} else {
						SolveDistances(h, 0, (uint32)mCA.Size());
						lap(mProfile.structural);
					}
					if (params.selfCollision && params.selfEveryIteration) {
						SolveSelf();
						lap(mProfile.selfSolve);
					}
					if (params.collisions && params.collidersEveryIteration) {
						SolveColliders(CS, nk, CV0, CV1, h, alpha); // en dernier : l'état final ne pénètre pas
						lap(mProfile.colliders);
					}
				}
				if (params.collisions && !params.collidersEveryIteration) {
					SolveColliders(CS, nk, CV0, CV1, h, alpha); // une fois par sous-pas, après les itérations
					lap(mProfile.colliders);
				}
				if (params.selfCollision && !params.selfEveryIteration) {
					// une résolution par sous-pas, après les itérations ; puis les colliders, en dernier
					SolveSelf();
					lap(mProfile.selfSolve);
					if (params.collisions) {
						SolveColliders(CS, nk, CV0, CV1, h, alpha);
						lap(mProfile.colliders);
					}
				}
				UpdateVelocities(h);
				lap(mProfile.velocities);
			}
			// la pose de fin de ce pas est la pose de début du suivant (colliders en mouvement, en-tête)
			collidersPrev.Resize((uint32)colliders.Size());
			for (uint32 k = 0; k < (uint32)colliders.Size(); ++k)
				collidersPrev[k] = colliders[k];
			mStats.substeps = sub;
			mStats.iterations = iters;
			mStats.dt = dt;
			Measure(dt);
			lap(mProfile.measure);
			if (clk)
				mProfile.total = (clk() - t0) * 1000.0;
		}

		void NkCloth::Predict(float32 h, float32 time, float32 alpha, float32 invDt) {
			const uint32 n = (uint32)mPos.Size();
			NkVec3f *X = mPos.Data(), *P = mPrev.Data(), *V = mVel.Data();
			const float32 *W = mInvMass.Data(), *M = mMass.Data();
			const NkVec3f *PT = mPinTarget.Data(), *PS = mPinStart.Data();
			const uint8 *PH = mPinHas.Data();
			const NkVec3f g = params.gravity;
			const bool wind = forceField != nullptr;
			if (wind && params.forceOnNormal)
				ComputeNormals(mNormal);
			const NkVec3f *NN = mNormal.Data();
			for (uint32 i = 0; i < n; ++i) {
				P[i] = X[i];
				if (W[i] <= 0.f) {
					if (PH[i]) {
						// épingle à cible : trajet linéaire sur le pas, vitesse (cible - départ) / dt
						const NkVec3f d = PT[i] - PS[i];
						X[i] = PS[i] + d * alpha;
						V[i] = d * invDt;
					} else {
						V[i] = {0.f, 0.f, 0.f};
					}
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

		void NkCloth::SolveDistances(float32 h, uint32 c0, uint32 c1) {
			const uint32 nc = c1 <= (uint32)mCA.Size() ? c1 : (uint32)mCA.Size();
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
			for (uint32 c = c0; c < nc; ++c) {
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

		void NkCloth::PrepareColliderStep(float32 alpha, float32 invDt) {
			const uint32 nk = (uint32)colliders.Size();
			mColStep.Resize(nk);
			mColV0.Resize(nk);
			mColV1.Resize(nk);
			mColSkip.Resize(nk);
			if (nk == 0)
				return;
			const bool motion = params.colliderMotion && (uint32)collidersPrev.Size() == nk;
			collision::NkShape *S = mColStep.Data();
			NkVec3f *V0 = mColV0.Data(), *V1 = mColV1.Data();
			uint8 *SK = mColSkip.Data();
			for (uint32 k = 0; k < nk; ++k) {
				const collision::NkShape &c = colliders[k];
				S[k] = c;
				V0[k] = {0.f, 0.f, 0.f};
				V1[k] = {0.f, 0.f, 0.f};
				SK[k] = 0;
				if (motion) {
					const collision::NkShape &p = collidersPrev[k];
					if (p.type == c.type) {
						S[k].p0 = p.p0 + (c.p0 - p.p0) * alpha;
						S[k].p1 = (c.type == collision::NkShapeType::NK_PLANE3D) ? c.p1 : p.p1 + (c.p1 - p.p1) * alpha;
						S[k].radius = p.radius + (c.radius - p.radius) * alpha;
						V0[k] = (c.p0 - p.p0) * invDt;
						V1[k] = (c.type == collision::NkShapeType::NK_PLANE3D) ? V0[k] : (c.p1 - p.p1) * invDt;
					}
				}
			}
			if (!params.colliderCulling)
				return;
			// élagage par boîtes : la boîte du tissu (positions prédites) contre celle du collider
			NkVec3f mn, mx;
			if (!Bounds(mn, mx))
				return;
			const float32 r = params.thickness;
			uint32 culled = 0;
			for (uint32 k = 0; k < nk; ++k) {
				const collision::NkShape &c = S[k];
				NkVec3f cmn, cmx;
				switch (c.type) {
					case collision::NkShapeType::NK_SPHERE:
						cmn = c.p0 - NkVec3f{c.radius + r, c.radius + r, c.radius + r};
						cmx = c.p0 + NkVec3f{c.radius + r, c.radius + r, c.radius + r};
						break;
					case collision::NkShapeType::NK_CAPSULE3D: {
						const float32 R = c.radius + r;
						cmn = {NkMin(c.p0.x, c.p1.x) - R, NkMin(c.p0.y, c.p1.y) - R, NkMin(c.p0.z, c.p1.z) - R};
						cmx = {NkMax(c.p0.x, c.p1.x) + R, NkMax(c.p0.y, c.p1.y) + R, NkMax(c.p0.z, c.p1.z) + R};
						break;
					}
					case collision::NkShapeType::NK_BOX3D:
						cmn = c.p0 - c.p1 - NkVec3f{r, r, r};
						cmx = c.p0 + c.p1 + NkVec3f{r, r, r};
						break;
					default:
						continue; // plan et types inconnus : jamais élagués
				}
				if (cmx.x < mn.x || cmn.x > mx.x || cmx.y < mn.y || cmn.y > mx.y || cmx.z < mn.z || cmn.z > mx.z) {
					SK[k] = 1;
					++culled;
				}
			}
			mStats.collidersCulled = culled;
		}

		bool NkCloth::SampleBody(const NkVec3f &p, float32 alpha, float32 &outDist, NkVec3f &outNormal) const {
			// la distance EXACTE d'abord (en-tête) : rayon de recherche = l'épaisseur plus la marge
			// d'un pas ; au-delà, la particule est loin du corps et rien ne la pousse
			if (bodyProx && bodyProx->Valid()) {
				// rayon COURT : on ne veut savoir que si la particule touche (épaisseur + ce qu'elle
				// parcourt en un sous-pas). Un rayon de 8 cm faisait visiter 5³ cellules par requête,
				// 12 M de tests de triangle par image, 600 ms par pas -- mesuré.
				const float32 reach = params.thickness * 2.f + 0.004f;
				float32 d1 = 0.f;
				NkVec3f n1{0.f, 1.f, 0.f};
				const bool ok1 = bodyProx->Query(p, reach, d1, n1);
				if (bodyProxPrev && bodyProxPrev->Valid()) {
					float32 d0 = 0.f;
					NkVec3f n0{0.f, 1.f, 0.f};
					if (bodyProxPrev->Query(p, reach, d0, n0) && ok1) {
						outDist = d0 + (d1 - d0) * alpha;
						NkVec3f g = n0 + (n1 - n0) * alpha;
						const float32 l = g.Len();
						outNormal = l > 1e-9f ? g * (1.f / l) : n1;
						return true;
					}
				}
				if (ok1) {
					outDist = d1;
					outNormal = n1;
					return true;
				}
				return false; // rien de proche : les capsules restent en secours
			}
			if (!bodySDF || !bodySDF->Valid())
				return false;
			const bool two = bodySDFPrev && bodySDFPrev->Valid();
			if (!two) {
				outDist = bodySDF->Sample(p);
				outNormal = bodySDF->Gradient(p);
				return true;
			}
			// interpolation linéaire des deux champs (en-tête) : la distance signée d'un corps qui se
			// déplace peu entre deux images est bien approchée par le mélange de ses deux champs
			const float32 d0 = bodySDFPrev->Sample(p), d1 = bodySDF->Sample(p);
			outDist = d0 + (d1 - d0) * alpha;
			const NkVec3f g0 = bodySDFPrev->Gradient(p), g1 = bodySDF->Gradient(p);
			NkVec3f g = g0 + (g1 - g0) * alpha;
			const float32 l = g.Len();
			outNormal = l > 1e-9f ? g * (1.f / l) : g1;
			return true;
		}

		void NkCloth::SolveColliders(const collision::NkShape *S, uint32 nk, const NkVec3f *V0, const NkVec3f *V1,
									 float32 h, float32 alpha) {
			const uint32 n = (uint32)mPos.Size();
			if (nk == 0)
				return;
			NkVec3f *X = mPos.Data();
			const NkVec3f *P = mPrev.Data();
			const float32 *W = mInvMass.Data();
			uint8 *CT = mContact.Data();
			NkVec3f *CN = mContactN.Data();
			const uint8 *SK = mColSkip.Data();
			const float32 r = params.thickness;
			const float32 mu = params.friction < 0.f ? 0.f : params.friction;
			uint32 ignored = 0, contacts = 0, sdfContacts = 0;
			const uint32 rings = params.sdfPinBlendRings;
			const uint8 *PR = ((uint32)mPinRing.Size() == n) ? mPinRing.Data() : nullptr;
			// ── LE CHAMP DE DISTANCE D'ABORD : il décrit le corps, les capsules le complètent ──
			// La vitesse du corps au point de contact est celle de la capsule la plus proche (même
			// corps, mêmes os) : le champ, lui, ne porte pas de vitesse. Dit.
			const bool hasBody = (bodyProx && bodyProx->Valid()) || (bodySDF && bodySDF->Valid());
			// la borne de projection : une cellule quand c'est un champ, l'épaisseur quand c'est exact
			const float32 pushMax = (bodyProx && bodyProx->Valid())
										? params.thickness
										: ((bodySDF && bodySDF->Valid()) ? bodySDF->Stats().cellSize : 0.f);
			if (params.sdfCollision && hasBody) {
				for (uint32 i = 0; i < n; ++i) {
					if (W[i] <= 0.f)
						continue;
					NkVec3f nrm{0.f, 1.f, 0.f};
					float32 d = 0.f;
					if (!SampleBody(X[i], alpha, d, nrm) || d >= r)
						continue;
					// projection à l'isosurface + l'épaisseur, BORNÉE : deux champs interpolés donnent une
					// distance incohérente là où le corps a beaucoup bougé entre les deux poses, et une
					// projection non bornée téléporte la particule -- mesuré le 05/09 : la cape passait
					// de 8,7 % à 269 % d'étirement le jour où les deux champs sont arrivés. La borne est
					// la taille d'une cellule : au-delà, le champ ne sait plus de quoi il parle.
					float32 push = r - d;
					if (push > pushMax)
						push = pushMax;
					// pondération près des épingles (en-tête) : l'arête entre une épingle fixe et sa
					// voisine poussée ne peut pas absorber tout l'écart
					if (rings > 0u && PR) {
						const uint8 ring = PR[i];
						if (ring < rings)
							push *= (float32)ring / (float32)rings;
					}
					if (push <= 0.f)
						continue;
					X[i] += nrm * push;
					// vitesse du corps : celle de la capsule la plus proche (secours), sinon zéro
					NkVec3f dc{0.f, 0.f, 0.f};
					float32 best = 1e30f;
					for (uint32 k = 0; k < nk; ++k) {
						const collision::NkShape &s = S[k];
						if (s.type != collision::NkShapeType::NK_SPHERE && s.type != collision::NkShapeType::NK_CAPSULE3D)
							continue;
						const NkVec3f ab = s.p1 - s.p0;
						const float32 ab2 = ab.Dot(ab);
						NkVec3f c = s.p0;
						float32 t = 0.f;
						if (s.type == collision::NkShapeType::NK_CAPSULE3D && ab2 > 1e-12f) {
							t = (X[i] - s.p0).Dot(ab) / ab2;
							t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
							c = s.p0 + ab * t;
						}
						const float32 d2 = (X[i] - c).LenSq();
						if (d2 < best) {
							best = d2;
							dc = (V0[k] + (V1[k] - V0[k]) * t) * h;
						}
					}
					X[i] += NkClothFriction((X[i] - P[i]) - dc, nrm, r, mu);
					CT[i] = 1;
					CN[i] = nrm;
					++sdfContacts;
				}
			}
			for (uint32 k = 0; k < nk; ++k) {
				if (SK[k])
					continue;
				const collision::NkShape &s = S[k];
				// déplacement du collider sur le sous-pas, aux deux extrémités (frottement relatif)
				const NkVec3f d0 = V0[k] * h, d1 = V1[k] * h;
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
							NkVec3f dc = d0;
							if (cap && ab2 > 1e-12f) {
								float32 t = (X[i] - s.p0).Dot(ab) / ab2;
								t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
								c = s.p0 + ab * t;
								dc = d0 + (d1 - d0) * t;
							}
							NkVec3f d = X[i] - c;
							const float32 l = d.Len();
							if (l < R) {
								const NkVec3f nrm = (l > 1e-9f) ? d * (1.f / l) : NkVec3f{0.f, 1.f, 0.f};
								X[i] = c + nrm * R;
								X[i] += NkClothFriction((X[i] - P[i]) - dc, nrm, R - l, mu);
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
								X[i] += NkClothFriction((X[i] - P[i]) - d0, nrm, -d, mu);
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
							X[i] += NkClothFriction((X[i] - P[i]) - d0, nrm, best, mu);
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
			mStats.sdfContacts = sdfContacts;
		}

		bool NkCloth::SelfPairsStale() const {
			// borne EXACTE du rapprochement de deux particules depuis la dernière liste :
			// 2 x max_i |d_i - d_moyen|, d_i = x_i - x_i(liste)
			const uint32 n = (uint32)mPos.Size();
			const NkVec3f *X = mPos.Data(), *B = mPairBase.Data();
			NkVec3f mean = {0.f, 0.f, 0.f};
			for (uint32 i = 0; i < n; ++i)
				mean += X[i] - B[i];
			mean *= 1.f / (float32)n;
			const float32 lim2 = 0.25f * mPairMargin * mPairMargin; // (marge / 2)²
			for (uint32 i = 0; i < n; ++i) {
				const NkVec3f d = X[i] - B[i] - mean;
				if (d.Dot(d) >= lim2)
					return true;
			}
			return false;
		}

		void NkCloth::BuildSelfPairs(float32 margin) {
			const uint32 n = (uint32)mPos.Size();
			const NkVec3f *X = mPos.Data();
			const float32 *W = mInvMass.Data();
			// rayon de recherche = contact (2r) + marge : la liste reste valable tant que deux
			// particules n'ont pas pu se rapprocher de plus que la marge (SelfPairsStale)
			const float32 radius = 2.f * params.thickness + margin;
			const float32 r2 = radius * radius;
			mPairRadius = radius;
			mPairMargin = margin;
			mPairBase.Resize(n);
			NkVec3f *PB = mPairBase.Data();
			for (uint32 i = 0; i < n; ++i)
				PB[i] = X[i];
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
			for (uint32 i = 0; i < n; ++i) {
				if (W[i] <= 0.f) {
					V[i] = (X[i] - P[i]) * invH; // épingle : 0 si immobile, (cible - départ) / dt si à cible
					continue;
				}
				V[i] = (X[i] - P[i]) * (invH * damp);
			}
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
			uint32 cnt = 0, maxC = 0, degenerate = 0;
			const float32 restMin = params.thickness;
			for (uint32 c = 0; c < nc; ++c) {
				switch (mKind[c]) {
					case STRUCTURAL: ++ns; break;
					case SHEAR: ++nsh; break;
					default: ++nb; break;
				}
				if (mKind[c] != STRUCTURAL || mRest[c] <= 0.f)
					continue;
				if (mRest[c] < restMin) {
					++degenerate; // un rapport sur 0,3 mm ne mesure rien ; l'effondrement se compte à part
					continue;
				}
				const float32 l = (X[mCA[c]] - X[mCB[c]]).Len();
				float32 e = (l - mRest[c]) / mRest[c];
				if (e < 0.f)
					e = -e;
				if (e > maxS) {
					maxS = e;
					maxC = c;
				}
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
			mStats.maxStretchEdge = maxC;
			mStats.degenerateEdges = degenerate;
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
						if (W[i] <= 0.f)
							continue; // épinglée : dans la capsule de son os, par construction
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
						if (W[i] <= 0.f)
							continue;
						const float32 d = -(X[i] - s.p0).Dot(s.p1);
						if (d > pen)
							pen = d;
					}
				}
			}
			mStats.maxPenetration = pen;
			// pénétration dans le CHAMP : l'épaisseur moins la distance signée (0 attendu après le pas)
			float32 sdfPen = 0.f;
			if (params.sdfCollision)
				for (uint32 i = 0; i < n; ++i) {
					if (W[i] <= 0.f)
						continue; // épinglée : elle suit son os, elle EST sous la peau (en-tête)
					float32 dist = 0.f;
					NkVec3f nn{0.f, 1.f, 0.f};
					if (!SampleBody(X[i], 1.f, dist, nn))
						continue;
					const float32 d = params.thickness - dist;
					if (d > sdfPen)
						sdfPen = d;
				}
			mStats.maxSdfPenetration = sdfPen;
			// épingles à cible : où sont-elles par rapport à leur cible (0 attendu en fin de pas)
			float32 pinErr = 0.f;
			uint32 pinT = 0;
			{
				const NkVec3f *PT = mPinTarget.Data();
				const uint8 *PH = mPinHas.Data();
				for (uint32 i = 0; i < n; ++i) {
					if (!PH[i] || W[i] > 0.f)
						continue;
					++pinT;
					const float32 e = (X[i] - PT[i]).Len();
					if (e > pinErr)
						pinErr = e;
				}
			}
			mStats.maxPinError = pinErr;
			mStats.pinTargets = pinT;
			// distance minimale entre particules NON voisines (si auto-collision) : lue sur la LISTE DE
			// PAIRES du pas (mesuré : une seconde traversée ici coûtait 0,47 ms sur 3,7 à 32 x 32) ;
			// aucune paire dans le rayon de recherche -> on rend ce plancher (le rayon), pas 0
			float32 dminSelf = 0.f;
			if (params.selfCollision && n > 0) {
				float32 best2 = 1e30f;
				const uint32 np = (uint32)mPairA.Size();
				const uint32 *PA = mPairA.Data(), *PB = mPairB.Data();
				for (uint32 q = 0; q < np; ++q) {
					const NkVec3f d = X[PA[q]] - X[PB[q]];
					const float32 l2 = d.Dot(d);
					if (l2 < best2)
						best2 = l2;
				}
				dminSelf = best2 < 1e30f ? NkSqrt(best2) : mPairRadius;
			}
			mStats.minSelfDistance = dminSelf;
		}

	} // namespace physics
} // namespace nkentseu
