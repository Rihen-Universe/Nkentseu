// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkSPHSolver.cpp — voir NkSPHSolver.h. WCSPH : grille uniforme O(N), poly6 /
// spiky / viscosité (Müller 2003), équation d'état linéaire, PAROIS PAR
// PARTICULES FANTÔMES (deux couches fixes, pression miroir), terme de pression
// de Monaghan (p_i/rho_i² + p_j/rho_j²), qui conserve le moment.
// Aucune allocation par pas en régime établi : les tampons se réutilisent.
//
// Historique mesuré (04/09) : sans fantômes et avec (p_i+p_j)/(2 rho_j), le bloc
// au repos donnait rho/rho0 0,79-0,94 et vmax montait 3,7 -> 7,9 m/s en 3 s
// depuis le repos -- ni le pas (CFL 0,15), ni l'impact, ni la pression négative,
// ni la masse : la formulation. C'est ce que corrige cette version.
// =============================================================================
#include "NkSPHSolver.h"
#include "NkVFXSystem.h"
#include <cmath>
#include "NKTime/NkChrono.h" // en dernier (cf. NkVFXSystem.cpp : namespace `time`)

namespace nkentseu {
	namespace renderer {

		static const float32 kPI = 3.14159265358979f;

		float32 NkSPHParams::SoundSpeed() const {
			return sqrtf(stiffness > 0.f ? stiffness : 1.f);
		}

		// Masse telle que la densité SPH d'un réseau cubique d'espacement h/2 vaille rho0 :
		// m = rho0 / somme_j W_poly6(|r_j|, h) sur les voisins du réseau (soi compris).
		float32 NkSPHParams::Mass() const {
			if (particleMass > 0.f)
				return particleMass;
			const float32 d = h * 0.5f;
			const float32 h2 = h * h;
			const float32 kPoly6 = 315.f / (64.f * kPI * powf(h, 9.f));
			float32 sum = 0.f;
			for (int32 z = -3; z <= 3; ++z)
				for (int32 y = -3; y <= 3; ++y)
					for (int32 x = -3; x <= 3; ++x) {
						const float32 r2 = (float32)(x * x + y * y + z * z) * d * d;
						if (r2 < h2) {
							const float32 t = h2 - r2;
							sum += kPoly6 * t * t * t;
						}
					}
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
		// espacement d = h/2 : les fluides près d'une paroi voient autant de voisines
		// qu'à l'intérieur. Reconstruites seulement si la boîte ou h change.
		void NkSPHSolver::BuildBoundary() {
			const NkVec3f bmin = params.boundsMin, bmax = params.boundsMax;
			if (mBoundH == params.h && mBoundMin.x == bmin.x && mBoundMin.y == bmin.y && mBoundMin.z == bmin.z &&
				mBoundMax.x == bmax.x && mBoundMax.y == bmax.y && mBoundMax.z == bmax.z && !mBound.Empty())
				return;
			mBound.Clear();
			const float32 d = params.h * 0.5f;
			const int32 layers = 2;
			const float32 ex = (float32)layers * d; // débord pour couvrir arêtes et coins
			auto push = [&](float32 x, float32 y, float32 z) { mBound.PushBack({x, y, z}); };
			// sol et plafond
			for (int32 l = 0; l < layers; ++l) {
				const float32 yb = bmin.y - d * (0.5f + (float32)l);
				const float32 yt = bmax.y + d * (0.5f + (float32)l);
				for (float32 x = bmin.x - ex + d * 0.5f; x < bmax.x + ex; x += d)
					for (float32 z = bmin.z - ex + d * 0.5f; z < bmax.z + ex; z += d) {
						push(x, yb, z);
						push(x, yt, z);
					}
			}
			// murs x et z (hauteur de la boîte seulement : sol/plafond couvrent les coins)
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
			mBoundH = params.h;
			mBoundMin = bmin;
			mBoundMax = bmax;
		}

		void NkSPHSolver::Apply(NkParticleStoreCPU &store, const NkEmitterDesc &desc, float32 dt) {
			(void)desc;
			const int64 t0 = ::nkentseu::NkChrono::Now().nanoseconds;
			mStats = NkSPHStats{};
			BuildBoundary();
			// Sous-pas CFL : dt_s <= cfl h / max(vmax, c). Le nombre est dit au profil.
			const float32 h = params.h > 1e-5f ? params.h : 1e-5f;
			float32 vref = params.maxSpeed > 1e-3f ? params.maxSpeed : 1e-3f;
			const float32 c = params.SoundSpeed();
			if (c > vref)
				vref = c; // la vitesse du son borne le pas autant que les particules
			const float32 dtMax = params.cfl * h / vref;
			uint32 sub = (uint32)ceilf(dt / dtMax);
			if (sub < 1u)
				sub = 1u;
			if (sub > params.maxSubSteps)
				sub = params.maxSubSteps; // plafond : on dit, on n'explose pas le CPU
			const float32 dts = dt / (float32)sub;
			for (uint32 s = 0; s < sub; ++s)
				StepOnce(store, dts);
			mStats.subSteps = sub;
			mStats.boundary = (uint32)mBound.Size();
			mStats.ms = (float32)((::nkentseu::NkChrono::Now().nanoseconds - t0) / 1.0e6);
		}

		void NkSPHSolver::StepOnce(NkParticleStoreCPU &store, float32 dt) {
			const float32 h = params.h;
			const float32 h2 = h * h;
			const float32 m = params.Mass();
			const float32 rho0 = params.restDensity;
			const float32 kPoly6 = 315.f / (64.f * kPI * powf(h, 9.f));
			const float32 kSpiky = -45.f / (kPI * powf(h, 6.f));
			const float32 kVisc = 45.f / (kPI * powf(h, 6.f));
			// Pointeurs bruts (Debug : operator[] non inline, cf. NkParticleStore.cpp).
			NkVec3f *P = store.pos.Data(), *V = store.vel.Data();
			const uint8 *A = store.alive.Data();

			// 1) vivantes, puis positions de tout le monde (vivantes puis fantômes)
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
			NkVec3f *X = mPosAll.Data();
			for (uint32 k = 0; k < n; ++k)
				X[k] = P[AL[k]];
			const NkVec3f *B = mBound.Data();
			for (uint32 k = 0; k < nb; ++k)
				X[n + k] = B[k];

			// 2) grille uniforme de cellule h, étendue de trois espacements autour de la
			//    boîte (les fantômes sont dehors). Hors grille = cellule de bord.
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
				CC[c] = 0; // réutilisé comme curseur d'insertion
			for (uint32 k = 0; k < M; ++k) {
				const uint32 c = CO[k];
				SO[CS[c] + CC[c]] = k;
				++CC[c];
			}

			// 3) densité et pression des vivantes (les fantômes comptent dans la densité)
			mDensity.Resize(n);
			mPressure.Resize(n);
			float32 *D = mDensity.Data(), *PR = mPressure.Data();
			for (uint32 k = 0; k < n; ++k) {
				const NkVec3f pi = X[k];
				int32 cx, cy, cz;
				cellCoord(pi, cx, cy, cz);
				float32 rho = 0.f;
				for (int32 dz = -1; dz <= 1; ++dz)
					for (int32 dy = -1; dy <= 1; ++dy)
						for (int32 dx = -1; dx <= 1; ++dx) {
							const int32 x = cx + dx, y = cy + dy, z = cz + dz;
							if (x < 0 || y < 0 || z < 0 || x >= (int32)nx || y >= (int32)ny || z >= (int32)nz)
								continue;
							const uint32 c = (uint32)x + nx * ((uint32)y + ny * (uint32)z);
							for (uint32 q = CS[c]; q < CS[c + 1]; ++q) {
								const NkVec3f dd = pi - X[SO[q]];
								const float32 r2 = dd.x * dd.x + dd.y * dd.y + dd.z * dd.z;
								if (r2 < h2) {
									const float32 t = h2 - r2;
									rho += m * kPoly6 * t * t * t;
								}
							}
						}
				D[k] = rho > 1e-6f ? rho : 1e-6f;
				// p >= 0 : une pression négative (surface) attire puis fait exploser --
				// instabilité de traction, mesurée le 04/09.
				const float32 pk = params.stiffness * (D[k] - rho0);
				PR[k] = (pressureEnabled && pk > 0.f) ? pk : 0.f;
			}

			// 4) accélérations : pression (Monaghan, conserve le moment) + viscosité + gravité
			mAccel.Resize(n);
			NkVec3f *AC = mAccel.Data();
			for (uint32 k = 0; k < n; ++k) {
				const uint32 i = AL[k];
				const NkVec3f pi = X[k];
				const NkVec3f vi = V[i];
				const float32 rhoi = D[k], pri = PR[k];
				const float32 termI = pri / (rhoi * rhoi);
				int32 cx, cy, cz;
				cellCoord(pi, cx, cy, cz);
				NkVec3f ap = {0.f, 0.f, 0.f}, av = {0.f, 0.f, 0.f};
				for (int32 dz = -1; dz <= 1; ++dz)
					for (int32 dy = -1; dy <= 1; ++dy)
						for (int32 dx = -1; dx <= 1; ++dx) {
							const int32 x = cx + dx, y = cy + dy, z = cz + dz;
							if (x < 0 || y < 0 || z < 0 || x >= (int32)nx || y >= (int32)ny || z >= (int32)nz)
								continue;
							const uint32 c = (uint32)x + nx * ((uint32)y + ny * (uint32)z);
							for (uint32 q = CS[c]; q < CS[c + 1]; ++q) {
								const uint32 jk = SO[q];
								if (jk == k)
									continue;
								const NkVec3f dd = pi - X[jk];
								const float32 r2 = dd.x * dd.x + dd.y * dd.y + dd.z * dd.z;
								if (r2 >= h2 || r2 < 1e-12f)
									continue;
								const float32 r = sqrtf(r2);
								const float32 hr = h - r;
								const float32 gw = kSpiky * hr * hr / r; // gradW = gw * dd
								float32 termJ, rhoj;
								NkVec3f vj;
								if (jk < n) {
									rhoj = D[jk];
									termJ = PR[jk] / (rhoj * rhoj);
									vj = V[AL[jk]];
								} else {
									// fantôme : pression miroir, densité de repos, vitesse nulle (non glissement)
									rhoj = rho0;
									termJ = pri / (rho0 * rho0);
									vj = {0.f, 0.f, 0.f};
								}
								// a_i += -m (p_i/rho_i^2 + p_j/rho_j^2) gradW
								const float32 pc = -m * (termI + termJ) * gw;
								ap.x += pc * dd.x;
								ap.y += pc * dd.y;
								ap.z += pc * dd.z;
								// viscosité : mu m (v_j - v_i)/(rho_i rho_j) lapW
								const float32 vc = params.viscosity * m / (rhoi * rhoj) * kVisc * hr;
								av.x += vc * (vj.x - vi.x);
								av.y += vc * (vj.y - vi.y);
								av.z += vc * (vj.z - vi.z);
							}
						}
				AC[k] = {ap.x + av.x + params.gravity.x, ap.y + av.y + params.gravity.y, ap.z + av.z + params.gravity.z};
			}

			// 5) intégration (semi-implicite), borne de vitesse, parois (dernier filet :
			//    les fantômes portent, le clamp ne fait qu'empêcher de traverser)
			float32 rhoMin = 1e30f, rhoMax = 0.f, rhoSum = 0.f, vmax = 0.f;
			float32 xmax = -1e30f, ymax = -1e30f, ymin = 1e30f;
			float32 floorSum = 0.f;
			uint32 floorN = 0;
			const float32 vcap2 = params.maxSpeed * params.maxSpeed;
			for (uint32 k = 0; k < n; ++k) {
				const uint32 i = AL[k];
				NkVec3f v = V[i];
				v.x += AC[k].x * dt;
				v.y += AC[k].y * dt;
				v.z += AC[k].z * dt;
				float32 v2 = v.x * v.x + v.y * v.y + v.z * v.z;
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
				if (p.y > ymax) ymax = p.y;
				if (p.y < ymin) ymin = p.y;
				const float32 sp = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
				if (sp > vmax)
					vmax = sp;
				const float32 rho = D[k];
				rhoSum += rho;
				if (rho < rhoMin) rhoMin = rho;
				if (rho > rhoMax) rhoMax = rho;
				if (p.y < bmin.y + h) {
					floorSum += rho;
					++floorN;
				}
			}
			mStats.densityMean = rhoSum / (float32)n;
			mStats.densityMin = rhoMin;
			mStats.densityMax = rhoMax;
			mStats.densityFloorMean = floorN ? floorSum / (float32)floorN : 0.f;
			mStats.maxSpeed = vmax;
			mStats.maxX = xmax;
			mStats.maxY = ymax;
			mStats.minY = ymin;
		}

	} // namespace renderer
} // namespace nkentseu
