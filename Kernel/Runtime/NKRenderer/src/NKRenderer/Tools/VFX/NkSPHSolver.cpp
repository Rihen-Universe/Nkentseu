// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkSPHSolver.cpp — voir NkSPHSolver.h. WCSPH : grille uniforme O(N), poly6 /
// spiky / viscosité (Müller 2003), équation d'état linéaire, bornes en boîte.
// Aucune allocation par pas en régime établi : les tampons se réutilisent.
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

		// Masse telle que la densite SPH d'un reseau cubique d'espacement h/2 vaille rho0 :
		// m = rho0 / somme_j W_poly6(|r_j|, h) sur les voisins du reseau (soi compris).
		float32 NkSPHParams::Mass() const {
			if (particleMass > 0.f)
				return particleMass;
			const float32 d = h * 0.5f;
			const float32 h2 = h * h;
			const float32 kPoly6 = 315.f / (64.f * 3.14159265358979f * powf(h, 9.f));
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

		void NkSPHSolver::Apply(NkParticleStoreCPU &store, const NkEmitterDesc &desc, float32 dt) {
			(void)desc;
			const int64 t0 = ::nkentseu::NkChrono::Now().nanoseconds;
			mStats = NkSPHStats{};
			// Sous-pas CFL : dt_s <= 0,4 h / maxSpeed. Le nombre est dit au profil.
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
			mStats.ms = (float32)((::nkentseu::NkChrono::Now().nanoseconds - t0) / 1.0e6);
		}

		void NkSPHSolver::StepOnce(NkParticleStoreCPU &store, float32 dt) {
			const float32 h = params.h;
			const float32 h2 = h * h;
			const float32 m = params.Mass();
			const float32 kPoly6 = 315.f / (64.f * kPI * powf(h, 9.f));
			const float32 kSpiky = -45.f / (kPI * powf(h, 6.f));
			const float32 kVisc = 45.f / (kPI * powf(h, 6.f));

			// Pointeurs bruts (Debug : operator[] non inline, cf. NkParticleStore.cpp).
			NkVec3f *P = store.pos.Data(), *V = store.vel.Data();
			const uint8 *A = store.alive.Data();
			// 1) vivantes
			mAlive.Clear();
			for (uint32 i = 0; i < store.capacity; ++i)
				if (A[i])
					mAlive.PushBack(i);
			const uint32 n = (uint32)mAlive.Size();
			mStats.alive = n;
			if (n == 0)
				return;

			// 2) grille uniforme de cellule h sur la boîte (les particules hors boîte
			//    sont ramenées dans la cellule de bord : la borne les y ramène ensuite).
			const NkVec3f bmin = params.boundsMin, bmax = params.boundsMax;
			const float32 inv = 1.f / h;
			uint32 nx = (uint32)((bmax.x - bmin.x) * inv) + 1u;
			uint32 ny = (uint32)((bmax.y - bmin.y) * inv) + 1u;
			uint32 nz = (uint32)((bmax.z - bmin.z) * inv) + 1u;
			const uint32 nc = nx * ny * nz;
			auto cellCoord = [&](const NkVec3f &p, int32 &cx, int32 &cy, int32 &cz) {
				cx = (int32)((p.x - bmin.x) * inv);
				cy = (int32)((p.y - bmin.y) * inv);
				cz = (int32)((p.z - bmin.z) * inv);
				if (cx < 0) cx = 0; if (cx >= (int32)nx) cx = (int32)nx - 1;
				if (cy < 0) cy = 0; if (cy >= (int32)ny) cy = (int32)ny - 1;
				if (cz < 0) cz = 0; if (cz >= (int32)nz) cz = (int32)nz - 1;
			};
			mCellOf.Resize(n);
			mCellCount.Resize(nc);
			mCellStart.Resize(nc + 1);
			mSorted.Resize(n);
			for (uint32 c = 0; c < nc; ++c)
				mCellCount[c] = 0;
			for (uint32 k = 0; k < n; ++k) {
				int32 cx, cy, cz;
				cellCoord(P[mAlive[k]], cx, cy, cz);
				const uint32 c = (uint32)cx + nx * ((uint32)cy + ny * (uint32)cz);
				mCellOf[k] = c;
				++mCellCount[c];
			}
			mCellStart[0] = 0;
			for (uint32 c = 0; c < nc; ++c)
				mCellStart[c + 1] = mCellStart[c] + mCellCount[c];
			for (uint32 c = 0; c < nc; ++c)
				mCellCount[c] = 0; // réutilisé comme curseur d'insertion
			for (uint32 k = 0; k < n; ++k) {
				const uint32 c = mCellOf[k];
				mSorted[mCellStart[c] + mCellCount[c]] = k;
				++mCellCount[c];
			}

			// 3) densité et pression
			mDensity.Resize(n);
			mPressure.Resize(n);
			const uint32 *AL = mAlive.Data(), *CS = mCellStart.Data(), *SO = mSorted.Data();
			float32 *D = mDensity.Data(), *PR = mPressure.Data();
			for (uint32 k = 0; k < n; ++k) {
				const NkVec3f pi = P[AL[k]];
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
								const uint32 j = SO[q];
								const NkVec3f d = pi - P[AL[j]];
								const float32 r2 = d.x * d.x + d.y * d.y + d.z * d.z;
								if (r2 < h2) {
									const float32 t = h2 - r2;
									rho += m * kPoly6 * t * t * t;
								}
							}
						}
				D[k] = rho > 1e-6f ? rho : 1e-6f;
				// p >= 0 : une pression negative (surface, densite sous rho0) attire les voisines
				// puis les fait exploser -- instabilite de traction, mesuree le 04/09 (densite min
				// 0,46 rho0, 200-300 particules bornees a chaque pas, le repos « bouillait »).
				const float32 pk = params.stiffness * (D[k] - params.restDensity);
				PR[k] = (pressureEnabled && pk > 0.f) ? pk : 0.f;
			}

			// 4) accélérations : pression + viscosité + gravité
			mAccel.Resize(n);
			NkVec3f *AC = mAccel.Data();
			for (uint32 k = 0; k < n; ++k) {
				const uint32 i = AL[k];
				const NkVec3f pi = P[i];
				const NkVec3f vi = V[i];
				int32 cx, cy, cz;
				cellCoord(pi, cx, cy, cz);
				NkVec3f fp = {0.f, 0.f, 0.f}, fv = {0.f, 0.f, 0.f};
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
								const uint32 j = AL[jk];
								const NkVec3f d = pi - P[j];
								const float32 r2 = d.x * d.x + d.y * d.y + d.z * d.z;
								if (r2 >= h2 || r2 < 1e-12f)
									continue;
								const float32 r = sqrtf(r2);
								const float32 hr = h - r;
								// pression : -m (p_i + p_j) / (2 rho_j) * gradW_spiky, gradW = kSpiky (h-r)^2 * d/r
								const float32 gw = kSpiky * hr * hr / r;
								const float32 pc = -m * (PR[k] + PR[jk]) / (2.f * D[jk]) * gw;
								fp.x += pc * d.x;
								fp.y += pc * d.y;
								fp.z += pc * d.z;
								// viscosité : mu m (v_j - v_i)/rho_j * lapW_visc, lapW = kVisc (h-r)
								const float32 vc = params.viscosity * m / D[jk] * kVisc * hr;
								const NkVec3f vj = V[j];
								fv.x += vc * (vj.x - vi.x);
								fv.y += vc * (vj.y - vi.y);
								fv.z += vc * (vj.z - vi.z);
							}
						}
				const float32 invRho = 1.f / D[k];
				AC[k] = {(fp.x + fv.x) * invRho + params.gravity.x, (fp.y + fv.y) * invRho + params.gravity.y,
							 (fp.z + fv.z) * invRho + params.gravity.z};
			}

			// 5) intégration (semi-implicite), borne de vitesse, parois
			float32 rhoMin = 1e30f, rhoMax = 0.f, rhoSum = 0.f, vmax = 0.f;
			float32 xmax = -1e30f, ymax = -1e30f, ymin = 1e30f;
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
					v2 = vcap2;
					++mStats.speedClamped;
				}
				NkVec3f p = P[i];
				p.x += v.x * dt;
				p.y += v.y * dt;
				p.z += v.z * dt;
				// parois : position ramenée, vitesse normale inversée x restitution
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
			}
			mStats.densityMean = rhoSum / (float32)n;
			mStats.densityMin = rhoMin;
			mStats.densityMax = rhoMax;
			mStats.maxSpeed = vmax;
			mStats.maxX = xmax;
			mStats.maxY = ymax;
			mStats.minY = ymin;
		}

	} // namespace renderer
} // namespace nkentseu
