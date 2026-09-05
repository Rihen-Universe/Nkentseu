// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkSPHSolver.cpp — DFSPH (Bender & Koschier 2015), voir NkSPHSolver.h.
// Grille uniforme O(N), noyau cubique, fantômes de paroi (Akinci), deux
// projections par sous-pas (divergence nulle, densité constante), XSPH.
// Aucune allocation par pas en régime établi : les tampons se réutilisent.
//
// Historique mesuré (04/09) : le WCSPH explicite (poly6/spiky, p = k (rho-rho0),
// puis fantômes + Monaghan) s'éjectait au lieu de se comprimer, pire à chaque
// cran de raideur (k 200 -> 3000), immobile sans gravité -- la formulation.
// =============================================================================
#include "NkSPHSolver.h"
#include "NkSPHStoreGPU.h"
#include "NkVFXSystem.h"
#include "NKMemory/NkAllocator.h"
#include <cmath>
#include "NKTime/NkChrono.h" // en dernier (cf. NkVFXSystem.cpp : namespace `time`)

namespace nkentseu {
	namespace renderer {

		static const float32 kPI = 3.14159265358979f;

		// Noyau cubique de support H (SPlisHSPlasH CubicKernel) : q = r / H.
		static inline float32 NkCubicW(float32 r, float32 H) {
			const float32 q = r / H;
			if (q >= 1.f)
				return 0.f;
			const float32 k = 8.f / (kPI * H * H * H);
			if (q <= 0.5f)
				return k * (6.f * (q * q * q - q * q) + 1.f);
			const float32 t = 1.f - q;
			return k * 2.f * t * t * t;
		}
		// dW/dr (scalaire) ; gradW = (dW/dr) * (x_i - x_j) / r
		static inline float32 NkCubicDW(float32 r, float32 H) {
			const float32 q = r / H;
			if (q >= 1.f || r < 1e-9f)
				return 0.f;
			const float32 l = 48.f / (kPI * H * H * H * H);
			if (q <= 0.5f)
				return l * q * (3.f * q - 2.f);
			const float32 t = 1.f - q;
			return -l * t * t;
		}

		float32 NkSPHParams::SoundSpeed() const {
			return sqrtf(stiffness > 0.f ? stiffness : 1.f);
		}

		// Masse telle que la densité SPH d'un réseau cubique d'espacement h/2 vaille rho0.
		float32 NkSPHParams::Mass() const {
			if (particleMass > 0.f)
				return particleMass;
			const float32 d = h * 0.5f;
			float32 sum = 0.f;
			for (int32 z = -3; z <= 3; ++z)
				for (int32 y = -3; y <= 3; ++y)
					for (int32 x = -3; x <= 3; ++x)
						sum += NkCubicW(sqrtf((float32)(x * x + y * y + z * z)) * d, h);
			return sum > 0.f ? restDensity / sum : restDensity * d * d * d;
		}

		uint32 NkSPHSolver::FillBlock(NkVector<NkParticleBirth> &out, NkVec3f min, NkVec3f max, float32 spacing,
									  float32 life) {
			if (spacing <= 0.f)
				return 0;
			uint32 n = 0;
			for (float32 z = min.z + spacing * 0.5f; z < max.z; z += spacing)
				for (float32 y = min.y + spacing * 0.5f; y < max.y; y += spacing)
					for (float32 x = min.x + spacing * 0.5f; x < max.x; x += spacing) {
						NkParticleBirth b;
						b.pos = {x, y, z};
						b.vel = {0.f, 0.f, 0.f};
						b.life = life;
						out.PushBack(b);
						++n;
					}
			return n;
		}

		// Deux couches de particules fixes sur les six faces de la boîte, à l'EXTÉRIEUR,
		// espacement d = h/2. Reconstruites seulement si la boîte ou h change.
		void NkSPHSolver::BuildBoundaryPositions(const NkSPHParams &params, NkVector<NkVec3f> &out) {
			const NkVec3f bmin = params.boundsMin, bmax = params.boundsMax;
			out.Clear();
			const float32 d = params.h * 0.5f;
			const int32 layers = 2;
			const float32 ex = (float32)layers * d;
			auto push = [&](float32 x, float32 y, float32 z) { out.PushBack({x, y, z}); };
			for (int32 l = 0; l < layers; ++l) {
				const float32 yb = bmin.y - d * (0.5f + (float32)l);
				const float32 yt = bmax.y + d * (0.5f + (float32)l);
				for (float32 x = bmin.x - ex + d * 0.5f; x < bmax.x + ex; x += d)
					for (float32 z = bmin.z - ex + d * 0.5f; z < bmax.z + ex; z += d) {
						push(x, yb, z);
						push(x, yt, z);
					}
			}
			for (int32 l = 0; l < layers; ++l) {
				const float32 xl = bmin.x - d * (0.5f + (float32)l), xr = bmax.x + d * (0.5f + (float32)l);
				const float32 zf = bmin.z - d * (0.5f + (float32)l), zb = bmax.z + d * (0.5f + (float32)l);
				for (float32 y = bmin.y + d * 0.5f; y < bmax.y; y += d) {
					for (float32 z = bmin.z - ex + d * 0.5f; z < bmax.z + ex; z += d) {
						push(xl, y, z);
						push(xr, y, z);
					}
					for (float32 x = bmin.x + d * 0.5f; x < bmax.x; x += d) {
						push(x, y, zf);
						push(x, y, zb);
					}
				}
			}
		}

		void NkSPHSolver::BuildBoundary() {
			const NkVec3f bmin = params.boundsMin, bmax = params.boundsMax;
			if (mBoundH == params.h && mBoundMin.x == bmin.x && mBoundMin.y == bmin.y && mBoundMin.z == bmin.z &&
				mBoundMax.x == bmax.x && mBoundMax.y == bmax.y && mBoundMax.z == bmax.z && !mBound.Empty())
				return;
			BuildBoundaryPositions(params, mBound);
			mBoundH = params.h;
			mBoundMin = bmin;
			mBoundMax = bmax;
		}

		NkIParticleStore *NkSPHSolver::CreateGPUStore(NkIDevice *device, const NkEmitterDesc &desc) {
			(void)device;
			(void)desc;
			return memory::NkGetDefaultAllocator().New<NkSPHStoreGPU>(this);
		}

		// Grille + listes de voisines (r < h, j != i) pour les n vivantes, sur les M
		// positions (vivantes puis fantômes). gradW et W sont précalculés une fois.
		void NkSPHSolver::BuildNeighbors(const NkVec3f *X, uint32 n, uint32 M) {
			const float32 h = params.h, h2 = h * h;
			const float32 d = h * 0.5f;
			const NkVec3f bmin = params.boundsMin, bmax = params.boundsMax;
			const NkVec3f gmin = {bmin.x - 3.f * d, bmin.y - 3.f * d, bmin.z - 3.f * d};
			const float32 inv = 1.f / h;
			const uint32 nx = (uint32)((bmax.x - gmin.x + 3.f * d) * inv) + 1u;
			const uint32 ny = (uint32)((bmax.y - gmin.y + 3.f * d) * inv) + 1u;
			const uint32 nz = (uint32)((bmax.z - gmin.z + 3.f * d) * inv) + 1u;
			const uint32 nc = nx * ny * nz;
			auto cellCoord = [&](const NkVec3f &p, int32 &cx, int32 &cy, int32 &cz) {
				cx = (int32)((p.x - gmin.x) * inv);
				cy = (int32)((p.y - gmin.y) * inv);
				cz = (int32)((p.z - gmin.z) * inv);
				if (cx < 0) cx = 0; if (cx >= (int32)nx) cx = (int32)nx - 1;
				if (cy < 0) cy = 0; if (cy >= (int32)ny) cy = (int32)ny - 1;
				if (cz < 0) cz = 0; if (cz >= (int32)nz) cz = (int32)nz - 1;
			};
			mCellOf.Resize(M);
			mCellCount.Resize(nc);
			mCellStart.Resize(nc + 1);
			mSorted.Resize(M);
			uint32 *CO = mCellOf.Data(), *CC = mCellCount.Data(), *CS = mCellStart.Data(), *SO = mSorted.Data();
			for (uint32 c = 0; c < nc; ++c)
				CC[c] = 0;
			for (uint32 k = 0; k < M; ++k) {
				int32 cx, cy, cz;
				cellCoord(X[k], cx, cy, cz);
				const uint32 c = (uint32)cx + nx * ((uint32)cy + ny * (uint32)cz);
				CO[k] = c;
				++CC[c];
			}
			CS[0] = 0;
			for (uint32 c = 0; c < nc; ++c)
				CS[c + 1] = CS[c] + CC[c];
			for (uint32 c = 0; c < nc; ++c)
				CC[c] = 0;
			for (uint32 k = 0; k < M; ++k) {
				const uint32 c = CO[k];
				SO[CS[c] + CC[c]] = k;
				++CC[c];
			}
			// listes
			mNbStart.Resize(n + 1);
			mNbIdx.Clear();
			mNbGrad.Clear();
			mNbW.Clear();
			mNbCount.Resize(n);
			uint32 *NS = mNbStart.Data(), *NC = mNbCount.Data();
			NS[0] = 0;
			for (uint32 k = 0; k < n; ++k) {
				const NkVec3f pi = X[k];
				int32 cx, cy, cz;
				cellCoord(pi, cx, cy, cz);
				uint32 fluidCount = 0;
				for (int32 dz = -1; dz <= 1; ++dz)
					for (int32 dy = -1; dy <= 1; ++dy)
						for (int32 dx = -1; dx <= 1; ++dx) {
							const int32 x = cx + dx, y = cy + dy, z = cz + dz;
							if (x < 0 || y < 0 || z < 0 || x >= (int32)nx || y >= (int32)ny || z >= (int32)nz)
								continue;
							const uint32 c = (uint32)x + nx * ((uint32)y + ny * (uint32)z);
							for (uint32 q = CS[c]; q < CS[c + 1]; ++q) {
								const uint32 j = SO[q];
								if (j == k)
									continue;
								const NkVec3f dd = pi - X[j];
								const float32 r2 = dd.x * dd.x + dd.y * dd.y + dd.z * dd.z;
								if (r2 >= h2)
									continue;
								const float32 r = sqrtf(r2);
								const float32 dw = NkCubicDW(r, h);
								const float32 s = (r > 1e-9f) ? dw / r : 0.f;
								mNbIdx.PushBack(j);
								mNbGrad.PushBack({dd.x * s, dd.y * s, dd.z * s});
								mNbW.PushBack(NkCubicW(r, h));
								if (j < n)
									++fluidCount;
							}
						}
				NS[k + 1] = (uint32)mNbIdx.Size();
				NC[k] = fluidCount;
			}
		}

		// rho_i = m W(0) + sum_j m W_ij (fantômes compris) ;
		// alpha_i = rho_i / (|sum_j m gradW_ij|^2 + sum_{j fluide} |m gradW_ij|^2), 0 si le
		// dénominateur est trop petit (particule isolée).
		void NkSPHSolver::ComputeDensityAndAlpha(uint32 n) {
			const float32 h = params.h;
			const float32 m = params.Mass();
			const float32 w0 = NkCubicW(0.f, h);
			mDensity.Resize(n);
			mAlpha.Resize(n);
			float32 *D = mDensity.Data(), *AF = mAlpha.Data();
			const uint32 *NS = mNbStart.Data(), *NI = mNbIdx.Data();
			const NkVec3f *NG = mNbGrad.Data();
			const float32 *NW = mNbW.Data();
			for (uint32 k = 0; k < n; ++k) {
				float32 rho = m * w0;
				NkVec3f sg = {0.f, 0.f, 0.f};
				float32 sg2 = 0.f;
				for (uint32 q = NS[k]; q < NS[k + 1]; ++q) {
					rho += m * NW[q];
					const NkVec3f g = {m * NG[q].x, m * NG[q].y, m * NG[q].z};
					sg.x += g.x;
					sg.y += g.y;
					sg.z += g.z;
					if (NI[q] < n)
						sg2 += g.x * g.x + g.y * g.y + g.z * g.z;
				}
				D[k] = rho;
				const float32 den = sg.x * sg.x + sg.y * sg.y + sg.z * sg.z + sg2;
				AF[k] = (den > 1e-6f) ? rho / den : 0.f;
			}
		}

		void NkSPHSolver::Apply(NkParticleStoreCPU &store, const NkEmitterDesc &desc, float32 dt) {
			mField = desc.field;
			const int64 t0 = ::nkentseu::NkChrono::Now().nanoseconds;
			mStats = NkSPHStats{};
			BuildBoundary();
			// Sous-pas CFL sur la vitesse MESURÉE au pas précédent (plancher 1 m/s pour le
			// premier pas et le repos) : dt_s <= cfl h / vmax.
			const float32 h = params.h > 1e-5f ? params.h : 1e-5f;
			const float32 vref = mLastVmax > 1.f ? mLastVmax : 1.f;
			float32 dtMax = params.cfl * h / vref;
			uint32 subVisc = 0;
			if (params.kinematicViscosity > 0.f) {
				// CFL visqueuse dt <= 0,125 hs^2 / nu avec hs = longueur de LISSAGE = h/2 pour le noyau
				// cubique (h = rayon de support). Mesure du 04/09 : ecrite avec h, elle ne mordait jamais
				// et le repos s'agitait a nu >= 0,02 (borne vraie 15,6 ms = notre pas).
				const float32 hs = 0.5f * h;
				const float32 dtVisc = 0.125f * hs * hs / params.kinematicViscosity; // MESURÉE
				if (dtVisc < dtMax) {
					dtMax = dtVisc;
					subVisc = (uint32)ceilf(dt / dtVisc);
				}
			}
			uint32 sub = (uint32)ceilf(dt / dtMax);
			if (sub < 1u)
				sub = 1u;
			if (sub > params.maxSubSteps)
				sub = params.maxSubSteps;
			const float32 dts = dt / (float32)sub;
			float32 itD = 0.f, itV = 0.f, rD = 0.f, rV = 0.f;
			uint32 caps = 0, clamped = 0, warm = 0;
			for (uint32 s = 0; s < sub; ++s) {
				StepOnce(store, dts);
				itD += mStats.iterDensity;
				itV += mStats.iterDivergence;
				rD += mStats.residualDensity;
				rV += mStats.residualDivergence;
				caps += mStats.iterCapHits;
				clamped += mStats.speedClamped;
				warm += mStats.warmStarts;
			}
			mStats.iterDensity = itD / (float32)sub;
			mStats.iterDivergence = itV / (float32)sub;
			mStats.residualDensity = rD / (float32)sub;
			mStats.residualDivergence = rV / (float32)sub;
			mStats.iterCapHits = caps;
			mStats.speedClamped = clamped;
			mStats.warmStarts = warm;
			mStats.subSteps = sub;
			mStats.subStepsViscous = subVisc;
			mStats.boundary = (uint32)mBound.Size();
			mStats.ms = (float32)((::nkentseu::NkChrono::Now().nanoseconds - t0) / 1.0e6);
		}

		void NkSPHSolver::StepOnce(NkParticleStoreCPU &store, float32 dt) {
			const float32 h = params.h;
			const float32 m = params.Mass();
			const float32 rho0 = params.restDensity;
			NkVec3f *P = store.pos.Data(), *V = store.vel.Data();
			const uint8 *A = store.alive.Data();
			mStats.iterCapHits = 0;
			mStats.speedClamped = 0;
			mStats.clumped = 0;
			mStats.warmStarts = 0;

			// 1) vivantes ; positions et vitesses de travail
			mAlive.Clear();
			for (uint32 i = 0; i < store.capacity; ++i)
				if (A[i])
					mAlive.PushBack(i);
			const uint32 n = (uint32)mAlive.Size();
			mStats.alive = n;
			if (n == 0)
				return;
			const uint32 nb = (uint32)mBound.Size();
			const uint32 M = n + nb;
			const uint32 *AL = mAlive.Data();
			mPosAll.Resize(M);
			mVel.Resize(n);
			NkVec3f *X = mPosAll.Data(), *W = mVel.Data();
			for (uint32 k = 0; k < n; ++k) {
				X[k] = P[AL[k]];
				W[k] = V[AL[k]];
			}
			const NkVec3f *B = mBound.Data();
			for (uint32 k = 0; k < nb; ++k)
				X[n + k] = B[k];

			// 2) voisinage, densité, alpha
			BuildNeighbors(X, n, M);
			ComputeDensityAndAlpha(n);
			const uint32 *NS = mNbStart.Data(), *NI = mNbIdx.Data(), *NC = mNbCount.Data();
			const NkVec3f *NG = mNbGrad.Data();
			const float32 *NW = mNbW.Data();
			const float32 *D = mDensity.Data(), *AF = mAlpha.Data();
			mKappa.Resize(n);
			mDensityAdv.Resize(n);
			float32 *K = mKappa.Data(), *DA = mDensityAdv.Data();
			const float32 invDt = 1.f / dt;

			// Drho/Dt_i = sum_j m (v_i - v_j).gradW_ij (fantômes : v_j = 0)
			auto divergence = [&](uint32 k) -> float32 {
				const NkVec3f vi = W[k];
				float32 s = 0.f;
				for (uint32 q = NS[k]; q < NS[k + 1]; ++q) {
					const uint32 j = NI[q];
					NkVec3f dv = vi;
					if (j < n) {
						dv.x -= W[j].x;
						dv.y -= W[j].y;
						dv.z -= W[j].z;
					}
					s += m * (dv.x * NG[q].x + dv.y * NG[q].y + dv.z * NG[q].z);
				}
				return s;
			};
			// v_i -= dt sum_j m (k_i/rho_i + k_j/rho_j) gradW_ij (fantômes : k_i/rho_i seul)
			auto correct = [&]() {
				for (uint32 k = 0; k < n; ++k) {
					const float32 ki = K[k] / D[k];
					NkVec3f acc = {0.f, 0.f, 0.f};
					for (uint32 q = NS[k]; q < NS[k + 1]; ++q) {
						const uint32 j = NI[q];
						const float32 kj = (j < n) ? K[j] / D[j] : 0.f;
						const float32 c = m * (ki + kj);
						acc.x += c * NG[q].x;
						acc.y += c * NG[q].y;
						acc.z += c * NG[q].z;
					}
					W[k].x -= dt * acc.x;
					W[k].y -= dt * acc.y;
					W[k].z -= dt * acc.z;
				}
			};

			// 3) DIVERGENCE NULLE (seulement les particules bien entourées : >= 20 voisines fluides)
			mStats.iterDivergence = 0.f;
			mStats.residualDivergence = 0.f;
			if (pressureEnabled) {
				uint32 it = 0;
				float32 err = 0.f;
				for (;;) {
					err = 0.f;
					for (uint32 k = 0; k < n; ++k) {
						float32 dd = 0.f;
						if (NC[k] >= 20) {
							dd = divergence(k);
							if (dd < 0.f)
								dd = 0.f;
						}
						K[k] = dd * AF[k] * invDt; // kappa^v
						err += dd * dt / rho0;
					}
					err /= (float32)n;
					if ((it >= 1 && err < params.tolDivergence) || it >= params.maxIterDivergence)
						break;
					correct();
					++it;
				}
				if (it >= params.maxIterDivergence)
					++mStats.iterCapHits;
				mStats.iterDivergence = (float32)it;
				mStats.residualDivergence = err;
			}

			// 4) forces non-pression : XSPH puis gravité
			if (params.viscosity > 0.f) {
				for (uint32 k = 0; k < n; ++k) {
					const NkVec3f vi = W[k];
					NkVec3f acc = {0.f, 0.f, 0.f};
					for (uint32 q = NS[k]; q < NS[k + 1]; ++q) {
						const uint32 j = NI[q];
						const float32 c = (j < n) ? (m / D[j]) * NW[q] : params.wallFriction * (m / rho0) * NW[q];
						const NkVec3f vj = (j < n) ? W[j] : NkVec3f{0.f, 0.f, 0.f};
						acc.x += c * (vj.x - vi.x);
						acc.y += c * (vj.y - vi.y);
						acc.z += c * (vj.z - vi.z);
					}
					// Application directe (Gauss-Seidel : les voisines deja lissees sont lues
					// lissees) -- acceptable pour un lissage XSPH, dit ici.
					W[k].x += params.viscosity * acc.x;
					W[k].y += params.viscosity * acc.y;
					W[k].z += params.viscosity * acc.z;
				}
			}
			// Viscosité artificielle de Monaghan (bouton d'expérience) :
			// a_i -= sum_j m Pi_ij gradW_ij, Pi_ij = -alpha c h (v_ij . x_ij) / (|x_ij|^2 + 0,01 h^2) / rho_moy,
			// seulement quand v_ij . x_ij < 0 (rapprochement). Fantômes : v_j = 0, rho_j = rho0.
			if (params.artViscosity > 0.f) {
				const float32 eps = 0.01f * h * h;
				for (uint32 k = 0; k < n; ++k) {
					const NkVec3f vi = W[k], xi = X[k];
					NkVec3f acc = {0.f, 0.f, 0.f};
					for (uint32 q = NS[k]; q < NS[k + 1]; ++q) {
						const uint32 j = NI[q];
						const NkVec3f vj = (j < n) ? W[j] : NkVec3f{0.f, 0.f, 0.f};
						const float32 rhoj = (j < n) ? D[j] : rho0;
						const NkVec3f dv = {vi.x - vj.x, vi.y - vj.y, vi.z - vj.z};
						const NkVec3f dx = {xi.x - X[j].x, xi.y - X[j].y, xi.z - X[j].z};
						const float32 vx = dv.x * dx.x + dv.y * dx.y + dv.z * dx.z;
						if (vx >= 0.f)
							continue;
						const float32 r2 = dx.x * dx.x + dx.y * dx.y + dx.z * dx.z;
						const float32 pi = -params.artViscosity * params.artSoundSpeed * h * vx / (r2 + eps) / (0.5f * (D[k] + rhoj));
						acc.x -= m * pi * NG[q].x;
						acc.y -= m * pi * NG[q].y;
						acc.z -= m * pi * NG[q].z;
					}
					W[k].x += acc.x * dt;
					W[k].y += acc.y * dt;
					W[k].z += acc.z * dt;
				}
			}
			// Viscosité PHYSIQUE de Morris (1997) :
			// a_i = sum_j m (mu_i + mu_j) / (rho_i rho_j) * (x_ij . gradW_ij) / (|x_ij|^2 + 0,01 h^2) * (v_i - v_j),
			// mu = rho nu ; x_ij . gradW_ij < 0 -> amortit la différence de vitesse ; nulle au repos.
			// Fantômes : v_j = 0, rho_j = rho0 (non-glissement). Lue AVANT correction (tampon), pas Gauss-Seidel.
			if (params.kinematicViscosity > 0.f) {
				const float32 eps = 0.01f * h * h;
				const float32 nu = params.kinematicViscosity;
				if (mVisc.Size() < n)
					mVisc.Resize(n);
				NkVec3f *AV = mVisc.Data();
				for (uint32 k = 0; k < n; ++k) {
					const NkVec3f vi = W[k], xi = X[k];
					const float32 mui = D[k] * nu;
					NkVec3f acc = {0.f, 0.f, 0.f};
					for (uint32 q = NS[k]; q < NS[k + 1]; ++q) {
						const uint32 j = NI[q];
						const NkVec3f vj = (j < n) ? W[j] : NkVec3f{0.f, 0.f, 0.f};
						const float32 rhoj = (j < n) ? D[j] : rho0;
						const NkVec3f dx = {xi.x - X[j].x, xi.y - X[j].y, xi.z - X[j].z};
						const float32 r2 = dx.x * dx.x + dx.y * dx.y + dx.z * dx.z;
						const float32 xg = dx.x * NG[q].x + dx.y * NG[q].y + dx.z * NG[q].z;
						const float32 cij = m * (mui + rhoj * nu) / (D[k] * rhoj) * xg / (r2 + eps);
						acc.x += cij * (vi.x - vj.x);
						acc.y += cij * (vi.y - vj.y);
						acc.z += cij * (vi.z - vj.z);
					}
					AV[k] = acc;
				}
				for (uint32 k = 0; k < n; ++k) {
					W[k].x += AV[k].x * dt;
					W[k].y += AV[k].y * dt;
					W[k].z += AV[k].z * dt;
				}
			}
			mTime += dt;
			const bool wind = mField.type != NkForceFieldType::NONE; // le vent (newtons) / Mass() s'ajoute à la gravité (2026-09-05)
			const float32 invMass = m > 1e-12f ? 1.f / m : 0.f;
			for (uint32 k = 0; k < n; ++k) {
				NkVec3f a = params.gravity;
				if (wind) {
					const NkVec3f w = NkEvalForceField(mField, X[k], mTime);
					a.x += w.x * invMass;
					a.y += w.y * invMass;
					a.z += w.z * invMass;
				}
				W[k].x += a.x * dt;
				W[k].y += a.y * dt;
				W[k].z += a.z * dt;
			}

			// 5) DENSITÉ CONSTANTE : rho* = rho + dt Drho/Dt ; kappa = (rho* - rho0) alpha / dt^2
			mStats.iterDensity = 0.f;
			mStats.residualDensity = 0.f;
			// Démarrage à chaud (bouton) : le kappa total du pas précédent, gardé TEL QUEL par emplacement du
			// stockage (kappa est une pression : rho* - rho0 est en dt², alpha/dt² le divise -- invariant au
			// pas ; MESURÉ le 04/09 : mémorisé en kappa·dt² et rendu en /dt², il est multiplié par 4 à chaque
			// doublement des sous-pas, qu'il provoque -- repos à 0,41 rho0, 8 m/s). Les emplacements morts sont
			// remis à zéro (une naissance repart froide) ; appliqué une fois AVANT la première évaluation ;
			// les itérations qui suivent sont comptées comme avant.
			float32 *KS = nullptr;
			if (pressureEnabled && params.warmStart) {
				const uint32 cap = store.capacity;
				if (mKappaSlot.Size() != cap) {
					mKappaSlot.Resize(cap);
					for (uint32 i = 0; i < cap; ++i)
						mKappaSlot[i] = 0.f;
				}
				float32 *KP = mKappaSlot.Data();
				for (uint32 i = 0; i < cap; ++i)
					if (!A[i])
						KP[i] = 0.f;
				mKappaSum.Resize(n);
				KS = mKappaSum.Data();
				bool any = false;
				for (uint32 k = 0; k < n; ++k) {
					K[k] = params.warmStartScale * KP[AL[k]];
					KS[k] = K[k];
					any = any || (K[k] != 0.f);
				}
				if (any) {
					auto rawResidual = [&](float32 &minRatio) -> float32 {
						float32 e = 0.f;
						minRatio = 1e30f;
						for (uint32 k = 0; k < n; ++k) {
							const float32 ra = D[k] + dt * divergence(k);
							e += fabsf(ra - rho0) / rho0;
							if (ra / rho0 < minRatio)
								minRatio = ra / rho0;
						}
						return e / (float32)n;
					};
					float32 mr = 0.f;
					mStats.warmResidualBefore = rawResidual(mr);
					correct();
					mStats.warmResidualAfter = rawResidual(mStats.warmMinRatio);
					++mStats.warmStarts;
				}
			}
			if (pressureEnabled) {
				uint32 it = 0;
				float32 err = 0.f;
				for (;;) {
					err = 0.f;
					for (uint32 k = 0; k < n; ++k) {
						float32 ra = D[k] + dt * divergence(k);
						if (ra < rho0) { // surface libre : la borne est un BOUTON d'expérience (04/09)
							if (params.surfaceMode == 1)
								ra = rho0; // dure : pas de traction
							else if (params.surfaceMode == 2)
								ra = rho0 + 0.5f * (ra - rho0); // douce : la moitié
						}
						DA[k] = ra;
						K[k] = (ra - rho0) * AF[k] * invDt * invDt;
						err += fabsf(ra - rho0) / rho0;
					}
					err /= (float32)n;
					if ((it >= 2 && err < params.tolDensity) || it >= params.maxIterDensity)
						break;
					correct();
					if (KS)
						for (uint32 k = 0; k < n; ++k)
							KS[k] += K[k];
					++it;
				}
				if (KS) {
					float32 *KP = mKappaSlot.Data();
					for (uint32 k = 0; k < n; ++k)
						KP[AL[k]] = KS[k];
				}
				if (it >= params.maxIterDensity)
					++mStats.iterCapHits;
				mStats.iterDensity = (float32)it;
				mStats.residualDensity = err;
			}

			// 6) intégration des positions, filets (vitesse, boîte), statistiques
			const NkVec3f bmin = params.boundsMin, bmax = params.boundsMax;
			float32 rhoMin = 1e30f, rhoMax = 0.f, rhoSum = 0.f, vmax = 0.f;
			float32 xmax = -1e30f, ymax = -1e30f, ymin = 1e30f, xdense = -1e30f;
			float32 floorSum = 0.f;
			uint32 floorN = 0;
			const float32 vcap2 = params.maxSpeed * params.maxSpeed;
			for (uint32 k = 0; k < n; ++k) {
				const uint32 i = AL[k];
				NkVec3f v = W[k];
				const float32 v2 = v.x * v.x + v.y * v.y + v.z * v.z;
				if (v2 > vcap2) {
					const float32 sc = params.maxSpeed / sqrtf(v2);
					v.x *= sc;
					v.y *= sc;
					v.z *= sc;
					++mStats.speedClamped;
				}
				NkVec3f p = P[i];
				p.x += v.x * dt;
				p.y += v.y * dt;
				p.z += v.z * dt;
				const float32 e = -params.restitution;
				if (p.x < bmin.x) { p.x = bmin.x; v.x *= e; }
				if (p.x > bmax.x) { p.x = bmax.x; v.x *= e; }
				if (p.y < bmin.y) { p.y = bmin.y; v.y *= e; }
				if (p.y > bmax.y) { p.y = bmax.y; v.y *= e; }
				if (p.z < bmin.z) { p.z = bmin.z; v.z *= e; }
				if (p.z > bmax.z) { p.z = bmax.z; v.z *= e; }
				V[i] = v;
				P[i] = p;
				if (p.x > xmax) xmax = p.x;
				if (D[k] >= 0.5f * rho0 && p.x > xdense) xdense = p.x;
				if (p.y > ymax) ymax = p.y;
				if (p.y < ymin) ymin = p.y;
				const float32 sp = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
				if (sp > vmax)
					vmax = sp;
				const float32 rho = D[k];
				if (rho > 1.1f * rho0)
					++mStats.clumped;
				rhoSum += rho;
				if (rho < rhoMin) rhoMin = rho;
				if (rho > rhoMax) rhoMax = rho;
				if (p.y < bmin.y + h) {
					floorSum += rho;
					++floorN;
				}
			}
			mLastVmax = vmax;
			mStats.densityMean = rhoSum / (float32)n;
			mStats.densityMin = rhoMin;
			mStats.densityMax = rhoMax;
			mStats.densityFloorMean = floorN ? floorSum / (float32)floorN : 0.f;
			mStats.maxSpeed = vmax;
			mStats.maxX = xmax;
			mStats.frontDenseX = xdense > -1e29f ? xdense : xmax;
			mStats.maxY = ymax;
			mStats.minY = ymin;
		}

	} // namespace renderer
} // namespace nkentseu
