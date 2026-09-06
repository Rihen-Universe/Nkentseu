// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkFluidGrid.cpp — fluide eulérien sur grille (Stam 1999 ; Fedkiw 2001).
// Voir NkFluidGrid.h pour la méthode, les sources citées et l'ordre d'un pas.
// Aucune allocation par pas : les tampons sont dimensionnés une fois dans Init.
// =============================================================================
#include "NkFluidGrid.h"
#include "NKTime/NkChrono.h" // en dernier (cf. NkVFXSystem.cpp : namespace `time`)

namespace nkentseu {
	namespace renderer {

		using namespace math;

		// ── petits utilitaires locaux (zéro STL) ────────────────────────────────
		static inline bool IsBad(float32 v) {
			// NaN : v != v ; Inf/explosion : borne large volontairement grossière.
			return (v != v) || (v > 1.0e30f) || (v < -1.0e30f);
		}

		static inline float32 Clampf(float32 v, float32 lo, float32 hi) {
			return v < lo ? lo : (v > hi ? hi : v);
		}

		// =====================================================================
		// Init / allocation
		// =====================================================================
		bool NkFluidGrid::Init(const NkFluidGridParams &p) {
			mParams = p;
			if (mParams.cellSize <= 0.f)
				return false;

			const NkVec3f size = {mParams.boundsMax.x - mParams.boundsMin.x, mParams.boundsMax.y - mParams.boundsMin.y,
								  mParams.boundsMax.z - mParams.boundsMin.z};
			if (size.x <= 0.f || size.y <= 0.f || size.z <= 0.f)
				return false;

			// La RÉSOLUTION vient de la taille de cellule, jamais d'un compte figé.
			mNx = (uint32)NkMax(1.f, NkCeil(size.x / mParams.cellSize));
			mNy = (uint32)NkMax(1.f, NkCeil(size.y / mParams.cellSize));
			mNz = (uint32)NkMax(1.f, NkCeil(size.z / mParams.cellSize));
			mCount = (mNx + 2) * (mNy + 2) * (mNz + 2);

			Allocate();
			Reset();
			mReady = true;

			mStats.cellsTotal = mCount;
			mStats.cellsInterior = mNx * mNy * mNz;
			return true;
		}

		void NkFluidGrid::Allocate() {
			mU.Resize(mCount, 0.f);
			mV.Resize(mCount, 0.f);
			mW.Resize(mCount, 0.f);
			mU0.Resize(mCount, 0.f);
			mV0.Resize(mCount, 0.f);
			mW0.Resize(mCount, 0.f);
			mDensity.Resize(mCount, 0.f);
			mDensity0.Resize(mCount, 0.f);
			mTemperature.Resize(mCount, 0.f);
			mTemperature0.Resize(mCount, 0.f);
			mFuel.Resize(mCount, 0.f);
			mFuel0.Resize(mCount, 0.f);
			mPressure.Resize(mCount, 0.f);
			mDivergence.Resize(mCount, 0.f);
		}

		void NkFluidGrid::Reset() {
			for (uint32 i = 0; i < mCount; ++i) {
				mU[i] = mV[i] = mW[i] = 0.f;
				mU0[i] = mV0[i] = mW0[i] = 0.f;
				mDensity[i] = mDensity0[i] = 0.f;
				mTemperature[i] = mTemperature0[i] = mParams.ambientTemperature;
				mFuel[i] = mFuel0[i] = 0.f;
				mPressure[i] = mDivergence[i] = 0.f;
			}
		}

		float32 NkFluidGrid::EffectiveBeta() const {
			// BOUSSINESQ : a = g (T - T_amb) / T_amb  =>  beta = g / T_amb.
			if (mParams.buoyancyBeta > 0.f)
				return mParams.buoyancyBeta;
			const float32 t = mParams.ambientTemperature;
			return (t > 1.0e-6f) ? (mParams.gravity / t) : 0.f;
		}

		// =====================================================================
		// Sources
		// =====================================================================
		void NkFluidGrid::EmitSphere(const NkVec3f &centerWorld, float32 radius, float32 density, float32 temperature,
									 float32 fuel) {
			if (!mReady)
				return;
			const float32 h = mParams.cellSize;
			const float32 r2 = radius * radius;
			for (uint32 k = 1; k <= mNz; ++k) {
				for (uint32 j = 1; j <= mNy; ++j) {
					for (uint32 i = 1; i <= mNx; ++i) {
						// centre de la cellule (i,j,k) en monde
						const float32 cx = mParams.boundsMin.x + ((float32)i - 0.5f) * h;
						const float32 cy = mParams.boundsMin.y + ((float32)j - 0.5f) * h;
						const float32 cz = mParams.boundsMin.z + ((float32)k - 0.5f) * h;
						const float32 dx = cx - centerWorld.x, dy = cy - centerWorld.y, dz = cz - centerWorld.z;
						if (dx * dx + dy * dy + dz * dz > r2)
							continue;
						const uint32 id = Idx(i, j, k);
						mDensity[id] += density;
						mTemperature[id] += temperature;
						mFuel[id] += fuel;
					}
				}
			}
		}

		void NkFluidGrid::SetUniformVelocity(const NkVec3f &v) {
			if (!mReady)
				return;
			for (uint32 k = 0; k <= mNz + 1; ++k)
				for (uint32 j = 0; j <= mNy + 1; ++j)
					for (uint32 i = 0; i <= mNx + 1; ++i) {
						const uint32 id = Idx(i, j, k);
						mU[id] = v.x;
						mV[id] = v.y;
						mW[id] = v.z;
					}
		}

		// =====================================================================
		// Bords — b : 0 scalaire (Neumann), 1 = u (x), 2 = v (y), 3 = w (z).
		// Paroi solide : la composante NORMALE est inversée sur la face (vitesse
		// nulle à l'interface), les tangentielles sont recopiées (glissement libre).
		// =====================================================================
		void NkFluidGrid::SetBoundary(int32 b, NkVector<float32> &f) {
			const uint32 X = mNx, Y = mNy, Z = mNz;

			for (uint32 k = 1; k <= Z; ++k)
				for (uint32 j = 1; j <= Y; ++j) {
					f[Idx(0, j, k)] = (b == 1) ? -f[Idx(1, j, k)] : f[Idx(1, j, k)];
					f[Idx(X + 1, j, k)] = (b == 1) ? -f[Idx(X, j, k)] : f[Idx(X, j, k)];
				}
			for (uint32 k = 1; k <= Z; ++k)
				for (uint32 i = 1; i <= X; ++i) {
					f[Idx(i, 0, k)] = (b == 2) ? -f[Idx(i, 1, k)] : f[Idx(i, 1, k)];
					f[Idx(i, Y + 1, k)] = (b == 2) ? -f[Idx(i, Y, k)] : f[Idx(i, Y, k)];
				}
			for (uint32 j = 1; j <= Y; ++j)
				for (uint32 i = 1; i <= X; ++i) {
					f[Idx(i, j, 0)] = (b == 3) ? -f[Idx(i, j, 1)] : f[Idx(i, j, 1)];
					f[Idx(i, j, Z + 1)] = (b == 3) ? -f[Idx(i, j, Z)] : f[Idx(i, j, Z)];
				}

			// arêtes et coins : moyenne des voisins de bord (Stam 1999, set_bnd)
			for (uint32 i = 1; i <= X; ++i) {
				f[Idx(i, 0, 0)] = 0.5f * (f[Idx(i, 1, 0)] + f[Idx(i, 0, 1)]);
				f[Idx(i, Y + 1, 0)] = 0.5f * (f[Idx(i, Y, 0)] + f[Idx(i, Y + 1, 1)]);
				f[Idx(i, 0, Z + 1)] = 0.5f * (f[Idx(i, 1, Z + 1)] + f[Idx(i, 0, Z)]);
				f[Idx(i, Y + 1, Z + 1)] = 0.5f * (f[Idx(i, Y, Z + 1)] + f[Idx(i, Y + 1, Z)]);
			}
			for (uint32 j = 1; j <= Y; ++j) {
				f[Idx(0, j, 0)] = 0.5f * (f[Idx(1, j, 0)] + f[Idx(0, j, 1)]);
				f[Idx(X + 1, j, 0)] = 0.5f * (f[Idx(X, j, 0)] + f[Idx(X + 1, j, 1)]);
				f[Idx(0, j, Z + 1)] = 0.5f * (f[Idx(1, j, Z + 1)] + f[Idx(0, j, Z)]);
				f[Idx(X + 1, j, Z + 1)] = 0.5f * (f[Idx(X, j, Z + 1)] + f[Idx(X + 1, j, Z)]);
			}
			for (uint32 k = 1; k <= Z; ++k) {
				f[Idx(0, 0, k)] = 0.5f * (f[Idx(1, 0, k)] + f[Idx(0, 1, k)]);
				f[Idx(X + 1, 0, k)] = 0.5f * (f[Idx(X, 0, k)] + f[Idx(X + 1, 1, k)]);
				f[Idx(0, Y + 1, k)] = 0.5f * (f[Idx(1, Y + 1, k)] + f[Idx(0, Y, k)]);
				f[Idx(X + 1, Y + 1, k)] = 0.5f * (f[Idx(X, Y + 1, k)] + f[Idx(X + 1, Y, k)]);
			}
			f[Idx(0, 0, 0)] = (f[Idx(1, 0, 0)] + f[Idx(0, 1, 0)] + f[Idx(0, 0, 1)]) / 3.f;
			f[Idx(X + 1, 0, 0)] = (f[Idx(X, 0, 0)] + f[Idx(X + 1, 1, 0)] + f[Idx(X + 1, 0, 1)]) / 3.f;
			f[Idx(0, Y + 1, 0)] = (f[Idx(1, Y + 1, 0)] + f[Idx(0, Y, 0)] + f[Idx(0, Y + 1, 1)]) / 3.f;
			f[Idx(0, 0, Z + 1)] = (f[Idx(1, 0, Z + 1)] + f[Idx(0, 1, Z + 1)] + f[Idx(0, 0, Z)]) / 3.f;
			f[Idx(X + 1, Y + 1, 0)] = (f[Idx(X, Y + 1, 0)] + f[Idx(X + 1, Y, 0)] + f[Idx(X + 1, Y + 1, 1)]) / 3.f;
			f[Idx(X + 1, 0, Z + 1)] = (f[Idx(X, 0, Z + 1)] + f[Idx(X + 1, 1, Z + 1)] + f[Idx(X + 1, 0, Z)]) / 3.f;
			f[Idx(0, Y + 1, Z + 1)] = (f[Idx(1, Y + 1, Z + 1)] + f[Idx(0, Y, Z + 1)] + f[Idx(0, Y + 1, Z)]) / 3.f;
			f[Idx(X + 1, Y + 1, Z + 1)] =
				(f[Idx(X, Y + 1, Z + 1)] + f[Idx(X + 1, Y, Z + 1)] + f[Idx(X + 1, Y + 1, Z)]) / 3.f;
		}

		// =====================================================================
		// Interpolation trilinéaire — coordonnées de GRILLE (le centre de la
		// cellule i vaut exactement i), comme le code de référence de Stam 1999.
		// =====================================================================
		float32 NkFluidGrid::Trilinear(const NkVector<float32> &f, float32 x, float32 y, float32 z) const {
			x = Clampf(x, 0.5f, (float32)mNx + 0.5f);
			y = Clampf(y, 0.5f, (float32)mNy + 0.5f);
			z = Clampf(z, 0.5f, (float32)mNz + 0.5f);

			const uint32 i0 = (uint32)x, j0 = (uint32)y, k0 = (uint32)z;
			const uint32 i1 = i0 + 1, j1 = j0 + 1, k1 = k0 + 1;
			const float32 sx1 = x - (float32)i0, sx0 = 1.f - sx1;
			const float32 sy1 = y - (float32)j0, sy0 = 1.f - sy1;
			const float32 sz1 = z - (float32)k0, sz0 = 1.f - sz1;

			return sz0 * (sy0 * (sx0 * f[Idx(i0, j0, k0)] + sx1 * f[Idx(i1, j0, k0)]) +
						  sy1 * (sx0 * f[Idx(i0, j1, k0)] + sx1 * f[Idx(i1, j1, k0)])) +
				   sz1 * (sy0 * (sx0 * f[Idx(i0, j0, k1)] + sx1 * f[Idx(i1, j0, k1)]) +
						  sy1 * (sx0 * f[Idx(i0, j1, k1)] + sx1 * f[Idx(i1, j1, k1)]));
		}

		// =====================================================================
		// Advection SEMI-LAGRANGIENNE (Stam 1999, § 2.2) : on remonte le temps
		// depuis le centre de chaque cellule et on lit le champ là-bas.
		// =====================================================================
		// Le POINT DE DEPART de ce qui arrive en (i,j,k), en coordonnees de grille.
		// Ordre 1 (Stam 1999) : x* = x - dt u(x). Ordre 2, point milieu (RK2, ce que
		// Fedkiw 2001 utilise) : x_m = x - dt/2 u(x), puis x* = x - dt u(x_m).
		// MESURE le 05/09 : l'ordre 1 fait perdre 45-51 % de la masse en 500 pas dans
		// un panache. La cause n'est PAS les parois (le banc l'a tranche : cas T, pure
		// translation, -0,11 % ; cas G, panache SANS contact de paroi, -45 %) mais le
		// terme en dt^2 |grad u|^2 du jacobien de la carte de retour : il est nul en
		// translation (grad u = 0) et grand dans un ecoulement etire.
		void NkFluidGrid::Backtrace(uint32 i, uint32 j, uint32 k, float32 dt0, const NkVector<float32> &fu,
									const NkVector<float32> &fv, const NkVector<float32> &fw, float32 &x, float32 &y,
									float32 &z) const {
			const uint32 id = Idx(i, j, k);
			if (!mParams.advectRK2) {
				x = (float32)i - dt0 * fu[id];
				y = (float32)j - dt0 * fv[id];
				z = (float32)k - dt0 * fw[id];
				return;
			}
			const float32 xm = (float32)i - 0.5f * dt0 * fu[id];
			const float32 ym = (float32)j - 0.5f * dt0 * fv[id];
			const float32 zm = (float32)k - 0.5f * dt0 * fw[id];
			x = (float32)i - dt0 * Trilinear(fu, xm, ym, zm);
			y = (float32)j - dt0 * Trilinear(fv, xm, ym, zm);
			z = (float32)k - dt0 * Trilinear(fw, xm, ym, zm);
		}

		void NkFluidGrid::AdvectScalar(NkVector<float32> &dst, const NkVector<float32> &src, float32 dt, int32 bnd) {
			const float32 dt0 = dt / mParams.cellSize; // pas en CELLULES
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						float32 x, y, z;
						Backtrace(i, j, k, dt0, mU, mV, mW, x, y, z);
						dst[Idx(i, j, k)] = Trilinear(src, x, y, z);
					}
			SetBoundary(bnd, dst);
		}

		void NkFluidGrid::AdvectVelocity(float32 dt) {
			// La vitesse s'advecte ELLE-MÊME : on la copie d'abord (u0), et on lit u0.
			for (uint32 i = 0; i < mCount; ++i) {
				mU0[i] = mU[i];
				mV0[i] = mV[i];
				mW0[i] = mW[i];
			}
			const float32 dt0 = dt / mParams.cellSize;
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						const uint32 id = Idx(i, j, k);
						float32 x, y, z;
						Backtrace(i, j, k, dt0, mU0, mV0, mW0, x, y, z);
						mU[id] = Trilinear(mU0, x, y, z);
						mV[id] = Trilinear(mV0, x, y, z);
						mW[id] = Trilinear(mW0, x, y, z);
					}
			SetBoundary(1, mU);
			SetBoundary(2, mV);
			SetBoundary(3, mW);
		}

		// =====================================================================
		// PROJECTION — Helmholtz-Hodge (Stam 1999, § 2.3).
		// Laplacien a 6 voisins sur grille reguliere.
		//
		// SOLVEUR : Gauss-Seidel SUR-RELAXE (SOR, Young 1954 ; Press & al.,
		// « Numerical Recipes », 3e ed., § 20.5.1). MESURE le 05/09 : le
		// Gauss-Seidel simple (omega = 1) plafonnait a 0,90 % de divergence
		// residuelle sur 40^3 apres 80 iterations -- le temoin (b) etait ROUGE
		// par le SOLVEUR, pas par la formulation. Le omega optimal d'un Poisson
		// a 6 voisins sur une grille de N cellules par cote est
		//     omega* = 2 / (1 + sin(pi / N))
		// (Press & al., eq. 20.5.19, N = la plus grande dimension ici). Il fait
		// passer le nombre d'iterations de O(N^2) a O(N).
		//
		// p est le POTENTIEL de Stam (unite m^2/s) : lap(p) = -div_stam, avec
		// div_stam = -h^2 * divergence_vraie. Le residu rapporte est
		// |lap(p) + div_stam| / h, donc en m/s — la MEME unite que divergence*h.
		// `pressureTolerance` est RELATIF au residu de depart (p = 0).
		// Le residu n'est calcule que tous les `residualCheckEvery` balayages :
		// le calculer a chaque fois DOUBLAIT le cout du solveur (mesure).
		// =====================================================================
		void NkFluidGrid::Project(float32 dt) {
			(void)dt; // la projection est independante du pas (p absorbe dt/rho)
			const float32 h = mParams.cellSize;
			const uint32 sy = mNx + 2;			  // pas d'indice en j
			const uint32 sz = (mNx + 2) * (mNy + 2); // pas d'indice en k

			float32 *P = mPressure.Data();
			float32 *D = mDivergence.Data();
			const float32 *U = mU.Data();
			const float32 *V = mV.Data();
			const float32 *W = mW.Data();

			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j) {
					const uint32 base = Idx(1, j, k);
					for (uint32 i = 0; i < mNx; ++i) {
						const uint32 id = base + i;
						D[id] = -0.5f * h *
								(U[id + 1] - U[id - 1] + V[id + sy] - V[id - sy] + W[id + sz] - W[id - sz]);
						if (!mParams.pressureWarmStart)
							P[id] = 0.f;
					}
				}
			SetBoundary(0, mDivergence);
			SetBoundary(0, mPressure);

			// Residu de depart (p = 0) : |div_stam| / h = |divergence*h|, en m/s.
			float32 residual0 = 0.f;
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j) {
					const uint32 base = Idx(1, j, k);
					for (uint32 i = 0; i < mNx; ++i) {
						const float32 r = NkAbs(D[base + i]) / h;
						if (r > residual0)
							residual0 = r;
					}
				}

			// omega : parametre s'il est dans ]1, 2[, sinon la valeur optimale theorique.
			float32 omega = mParams.pressureOmega;
			if (omega <= 1.f || omega >= 2.f) {
				const float32 N = (float32)NkMax(NkMax(mNx, mNy), mNz);
				omega = 2.f / (1.f + NkSin(3.14159265f / N));
			}
			mStats.pressureOmega = omega;
			mStats.pressureResidual0 = residual0;

			const float32 seuil = mParams.pressureTolerance * residual0;
			const uint32 every = (mParams.residualCheckEvery == 0) ? 1u : mParams.residualCheckEvery;

			uint32 iter = 0;
			float32 residual = residual0;
			if (residual0 > 0.f) {
				for (; iter < mParams.pressureIterations;) {
					for (uint32 sweep = 0; sweep < every && iter < mParams.pressureIterations; ++sweep, ++iter) {
						for (uint32 k = 1; k <= mNz; ++k)
							for (uint32 j = 1; j <= mNy; ++j) {
								const uint32 base = Idx(1, j, k);
								for (uint32 i = 0; i < mNx; ++i) {
									const uint32 id = base + i;
									const float32 gs =
										(D[id] + P[id - 1] + P[id + 1] + P[id - sy] + P[id + sy] + P[id - sz] + P[id + sz]) /
										6.f;
									P[id] += omega * (gs - P[id]);
								}
							}
						SetBoundary(0, mPressure);
					}

					residual = 0.f;
					for (uint32 k = 1; k <= mNz; ++k)
						for (uint32 j = 1; j <= mNy; ++j) {
							const uint32 base = Idx(1, j, k);
							for (uint32 i = 0; i < mNx; ++i) {
								const uint32 id = base + i;
								const float32 lap =
									P[id - 1] + P[id + 1] + P[id - sy] + P[id + sy] + P[id - sz] + P[id + sz] - 6.f * P[id];
								const float32 r = NkAbs(lap + D[id]) / h;
								if (r > residual)
									residual = r;
							}
						}
					if (residual < seuil)
						break;
				}
			}
			mStats.pressureIters = iter;
			mStats.pressureResidual = residual;
			mStats.pressureCapHit = (iter >= mParams.pressureIterations) && (residual >= seuil);

			// u <- u - grad(p)
			float32 *Um = mU.Data();
			float32 *Vm = mV.Data();
			float32 *Wm = mW.Data();
			const float32 inv2h = 0.5f / h;
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j) {
					const uint32 base = Idx(1, j, k);
					for (uint32 i = 0; i < mNx; ++i) {
						const uint32 id = base + i;
						Um[id] -= inv2h * (P[id + 1] - P[id - 1]);
						Vm[id] -= inv2h * (P[id + sy] - P[id - sy]);
						Wm[id] -= inv2h * (P[id + sz] - P[id - sz]);
					}
				}
			SetBoundary(1, mU);
			SetBoundary(2, mV);
			SetBoundary(3, mW);
		}

		// =====================================================================
		// Flottabilité — Fedkiw, Stam & Jensen 2001, eq. (8)
		// =====================================================================
		void NkFluidGrid::AddBuoyancy(float32 dt) {
			if (!mParams.buoyancyEnabled)
				return;
			const float32 beta = EffectiveBeta();
			const float32 alpha = mParams.buoyancyAlpha;
			const float32 tAmb = mParams.ambientTemperature;
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						const uint32 id = Idx(i, j, k);
						mV[id] += dt * (beta * (mTemperature[id] - tAmb) - alpha * mDensity[id]);
					}
			SetBoundary(2, mV);
		}

		// =====================================================================
		// COMBUSTION (palier ③) — réaction du premier ordre sur le carburant.
		// burnRate = 0 (défaut) : rien ne se passe, la grille reste de la fumée.
		// =====================================================================
		void NkFluidGrid::Combust(float32 dt) {
			if (mParams.burnRate <= 0.f)
				return;
			const float32 frac = 1.f - NkExp(-mParams.burnRate * dt); // fraction brûlée sur dt
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						const uint32 id = Idx(i, j, k);
						const float32 f = mFuel[id];
						if (f <= 0.f)
							continue;
						const float32 burn = f * frac;
						mFuel[id] = f - burn;
						mTemperature[id] += mParams.heatPerFuel * burn;
						mDensity[id] += mParams.sootPerFuel * burn;
					}
		}

		// =====================================================================
		// Mesures
		// =====================================================================
		void NkFluidGrid::MeasureDivergence(float32 &meanOut, float32 &maxOut) const {
			// divergence * h, en m/s : 0,5 * ( du + dv + dw ) sur les voisins.
			float64 sum = 0.0;
			float32 mx = 0.f;
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						const float32 d = 0.5f * (mU[Idx(i + 1, j, k)] - mU[Idx(i - 1, j, k)] + mV[Idx(i, j + 1, k)] -
												  mV[Idx(i, j - 1, k)] + mW[Idx(i, j, k + 1)] - mW[Idx(i, j, k - 1)]);
						const float32 a = NkAbs(d);
						sum += (float64)a;
						if (a > mx)
							mx = a;
					}
			const uint32 n = mNx * mNy * mNz;
			meanOut = (n > 0) ? (float32)(sum / (float64)n) : 0.f;
			maxOut = mx;
		}

		void NkFluidGrid::MeasureVelocity() {
			float64 sum = 0.0;
			float32 mx = 0.f;
			uint32 clamped = 0, bad = 0;
			const float32 lim = mParams.maxSpeed;
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						const uint32 id = Idx(i, j, k);
						if (IsBad(mU[id]) || IsBad(mV[id]) || IsBad(mW[id]) || IsBad(mDensity[id]) ||
							IsBad(mTemperature[id]))
							++bad;
						float32 s = NkSqrt(mU[id] * mU[id] + mV[id] * mV[id] + mW[id] * mW[id]);
						if (s > lim && s > 0.f) {
							const float32 f = lim / s;
							mU[id] *= f;
							mV[id] *= f;
							mW[id] *= f;
							s = lim;
							++clamped;
						}
						sum += (float64)s;
						if (s > mx)
							mx = s;
					}
			const uint32 n = mNx * mNy * mNz;
			mStats.velocityMean = (n > 0) ? (float32)(sum / (float64)n) : 0.f;
			mStats.maxSpeed = mx;
			mStats.speedClamped = clamped;
			mStats.nanCount = bad;
		}

		float32 NkFluidGrid::TotalMass() const {
			const float32 vol = mParams.cellSize * mParams.cellSize * mParams.cellSize;
			float64 sum = 0.0;
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i)
						sum += (float64)mDensity[Idx(i, j, k)];
			return (float32)(sum * (float64)vol);
		}

		// La masse de la couche interieure collee aux six parois. Sert a savoir si un
		// temoin de conservation mesure encore ce qu'il croit : l'advection
		// SEMI-LAGRANGIENNE n'est PAS conservative, et au contact d'une paroi elle
		// detruit franchement de la masse -- la cellule du bord advecte son contenu
		// vers la couche fantome, ou rien ne le recupere. MESURE le 05/09 : -100 %
		// en 500 pas avec une vitesse uniforme dirigee vers la paroi, -33 % pour un
		// panache dans une boite close basse. Tant que ce compteur reste a zero, la
		// masse totale ne peut varier que par l'INTERIEUR, et la mesurer a un sens.
		float32 NkFluidGrid::WallLayerMass() const {
			const float32 vol = mParams.cellSize * mParams.cellSize * mParams.cellSize;
			float64 sum = 0.0;
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						if (i > 1 && i < mNx && j > 1 && j < mNy && k > 1 && k < mNz)
							continue;
						sum += (float64)mDensity[Idx(i, j, k)];
					}
			return (float32)(sum * (float64)vol);
		}

		float32 NkFluidGrid::TotalHeat() const {
			const float32 vol = mParams.cellSize * mParams.cellSize * mParams.cellSize;
			const float32 tAmb = mParams.ambientTemperature;
			float64 sum = 0.0;
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i)
						sum += (float64)(mTemperature[Idx(i, j, k)] - tAmb);
			return (float32)(sum * (float64)vol);
		}

		float32 NkFluidGrid::TotalFuel() const {
			const float32 vol = mParams.cellSize * mParams.cellSize * mParams.cellSize;
			float64 sum = 0.0;
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i)
						sum += (float64)mFuel[Idx(i, j, k)];
			return (float32)(sum * (float64)vol);
		}

		bool NkFluidGrid::DensityCentroid(NkVec3f &out) const {
			const float32 h = mParams.cellSize;
			float64 wsum = 0.0, sx = 0.0, sy = 0.0, sz = 0.0;
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						const float32 d = mDensity[Idx(i, j, k)];
						if (d <= 0.f)
							continue;
						wsum += (float64)d;
						sx += (float64)d * (float64)(mParams.boundsMin.x + ((float32)i - 0.5f) * h);
						sy += (float64)d * (float64)(mParams.boundsMin.y + ((float32)j - 0.5f) * h);
						sz += (float64)d * (float64)(mParams.boundsMin.z + ((float32)k - 0.5f) * h);
					}
			if (wsum <= 0.0)
				return false;
			out = {(float32)(sx / wsum), (float32)(sy / wsum), (float32)(sz / wsum)};
			return true;
		}

		bool NkFluidGrid::TemperatureCentroid(NkVec3f &out) const {
			const float32 h = mParams.cellSize;
			const float32 tAmb = mParams.ambientTemperature;
			float64 wsum = 0.0, sx = 0.0, sy = 0.0, sz = 0.0;
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						const float32 w = mTemperature[Idx(i, j, k)] - tAmb;
						if (w <= 0.f)
							continue;
						wsum += (float64)w;
						sx += (float64)w * (float64)(mParams.boundsMin.x + ((float32)i - 0.5f) * h);
						sy += (float64)w * (float64)(mParams.boundsMin.y + ((float32)j - 0.5f) * h);
						sz += (float64)w * (float64)(mParams.boundsMin.z + ((float32)k - 0.5f) * h);
					}
			if (wsum <= 0.0)
				return false;
			out = {(float32)(sx / wsum), (float32)(sy / wsum), (float32)(sz / wsum)};
			return true;
		}

		float32 NkFluidGrid::HotVerticalVelocity() const {
			const float32 tAmb = mParams.ambientTemperature;
			float64 wsum = 0.0, vsum = 0.0;
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						const uint32 id = Idx(i, j, k);
						const float32 w = mTemperature[id] - tAmb;
						if (w <= 0.f)
							continue;
						wsum += (float64)w;
						vsum += (float64)w * (float64)mV[id];
					}
			return (wsum > 0.0) ? (float32)(vsum / wsum) : 0.f;
		}

		float32 NkFluidGrid::SampleDensityWorld(const NkVec3f &p) const {
			const float32 h = mParams.cellSize;
			return Trilinear(mDensity, (p.x - mParams.boundsMin.x) / h + 0.5f, (p.y - mParams.boundsMin.y) / h + 0.5f,
							 (p.z - mParams.boundsMin.z) / h + 0.5f);
		}

		float32 NkFluidGrid::SampleTemperatureWorld(const NkVec3f &p) const {
			const float32 h = mParams.cellSize;
			return Trilinear(mTemperature, (p.x - mParams.boundsMin.x) / h + 0.5f,
							 (p.y - mParams.boundsMin.y) / h + 0.5f, (p.z - mParams.boundsMin.z) / h + 0.5f);
		}

		// =====================================================================
		// Un pas
		// =====================================================================
		void NkFluidGrid::AddSources(float32 dt) {
			(void)dt; // les sources sont ajoutées par EmitSphere entre deux pas
		}

		void NkFluidGrid::Step(float32 dt) {
			if (!mReady || dt <= 0.f)
				return;
			const int64 t0 = ::nkentseu::NkChrono::Now().nanoseconds;

			AddSources(dt);
			Combust(dt);
			AddBuoyancy(dt);

			if (mParams.advectionEnabled)
				AdvectVelocity(dt);

			MeasureDivergence(mStats.divBeforeMean, mStats.divBeforeMax);

			if (mParams.projectionEnabled)
				Project(dt);
			else {
				mStats.pressureIters = 0;
				mStats.pressureResidual = 0.f;
				mStats.pressureCapHit = false;
			}

			MeasureDivergence(mStats.divAfterMean, mStats.divAfterMax);

			if (mParams.advectionEnabled) {
				AdvectScalar(mDensity0, mDensity, dt, 0);
				mDensity.Swap(mDensity0);
				AdvectScalar(mTemperature0, mTemperature, dt, 0);
				mTemperature.Swap(mTemperature0);
				if (mParams.burnRate > 0.f) {
					AdvectScalar(mFuel0, mFuel, dt, 0);
					mFuel.Swap(mFuel0);
				}
			}

			// Dissipation et refroidissement
			if (mParams.densityDissipation > 0.f) {
				const float32 f = NkExp(-mParams.densityDissipation * dt);
				for (uint32 i = 0; i < mCount; ++i)
					mDensity[i] *= f;
			}
			if (mParams.temperatureDissipation > 0.f || mParams.coolingRate > 0.f) {
				const float32 rate = mParams.temperatureDissipation + mParams.coolingRate;
				const float32 f = NkExp(-rate * dt);
				const float32 tAmb = mParams.ambientTemperature;
				for (uint32 i = 0; i < mCount; ++i)
					mTemperature[i] = tAmb + (mTemperature[i] - tAmb) * f;
			}
			if (mParams.fuelDissipation > 0.f) {
				const float32 f = NkExp(-mParams.fuelDissipation * dt);
				for (uint32 i = 0; i < mCount; ++i)
					mFuel[i] *= f;
			}

			MeasureVelocity();
			mStats.divRatio = (mStats.velocityMean > 1.0e-9f) ? (mStats.divAfterMean / mStats.velocityMean) : 0.f;
			mStats.mass = TotalMass();
			mStats.heat = TotalHeat();
			mStats.fuel = TotalFuel();

			float32 tmax = 0.f;
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						const float32 t = mTemperature[Idx(i, j, k)];
						if (t > tmax)
							tmax = t;
					}
			mStats.maxTemperature = tmax;
			mStats.ms = (float32)((::nkentseu::NkChrono::Now().nanoseconds - t0) / 1.0e6);
		}

	} // namespace renderer
} // namespace nkentseu
