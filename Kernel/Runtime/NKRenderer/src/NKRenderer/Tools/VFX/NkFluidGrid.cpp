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
			mScratchA.Resize(mCount, 0.f);
			mScratchB.Resize(mCount, 0.f);
			mOmegaX.Resize(mCount, 0.f);
			mOmegaY.Resize(mCount, 0.f);
			mOmegaZ.Resize(mCount, 0.f);
			mOmegaMag.Resize(mCount, 0.f);
		}

		void NkFluidGrid::Reset() {
			for (uint32 i = 0; i < mCount; ++i) {
				mU[i] = mV[i] = mW[i] = 0.f;
				mU0[i] = mV0[i] = mW0[i] = 0.f;
				mDensity[i] = mDensity0[i] = 0.f;
				mTemperature[i] = mTemperature0[i] = mParams.ambientTemperature;
				mFuel[i] = mFuel0[i] = 0.f;
				mPressure[i] = mDivergence[i] = 0.f;
				mOmegaX[i] = mOmegaY[i] = mOmegaZ[i] = mOmegaMag[i] = 0.f;
			}
			mFieldTime = 0.f;
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
		// LA VITESSE AU CENTRE D'UNE CELLULE — le chemin que la bascule MAC rend
		// dangereux (voir NkFluidGrid.h pour la convention de face).
		//
		// LA GRILLE EST DÉCALÉE (MAC) : `mU[Idx(i,j,k)]` est la face GAUCHE de la
		// cellule (i,j,k), à x = boundsMin.x + (i-1)*h. La vitesse AU CENTRE est
		// donc la MOYENNE DES DEUX FACES qui bordent la cellule. C'est le chemin
		// que prennent la flottabilité, la vorticité, les statistiques, le rendu et
		// le vent — et c'est celui que le contrôle (m1) juge, par ses DEUX volets :
		//   - exacte sur un champ LINÉAIRE (la moyenne de deux faces symétriques
		//     autour du centre vaut la valeur au centre) ;
		//   - fausse de h^2/8 * f'' sur un champ QUADRATIQUE, parce que
		//     (f(x-h/2) + f(x+h/2))/2 = f(x) + (h/2)^2/2 * f''. Cette erreur n'est
		//     pas un défaut : c'est la signature de l'interpolation, et l'exiger
		//     NON NULLE est ce qui empêche (m1) de ne prouver que « ça compile ».
		// La face i+1 existe toujours pour i <= nx : l'allocation (nx+2) la porte.
		// =====================================================================
		void NkFluidGrid::VelocityAtCenter(uint32 i, uint32 j, uint32 k, float32 &ux, float32 &uy,
										   float32 &uz) const {
			const uint32 id = Idx(i, j, k);
			ux = 0.5f * (mU[id] + mU[Idx(i + 1, j, k)]);
			uy = 0.5f * (mV[id] + mV[Idx(i, j + 1, k)]);
			uz = 0.5f * (mW[id] + mW[Idx(i, j, k + 1)]);
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
		// La vitesse au point de GRILLE (gx,gy,gz), interpolée depuis les FACES.
		// u vit sur les faces x : la face d'indice i est au point gx = i - 0,5. Pour
		// que Trilinear — qui indexe les CENTRES — lise les bonnes cases, on lui
		// passe gx + 0,5. Même décalage pour v en y et w en z. C'est LE décalage de
		// la grille décalée, et toute l'advection en dépend.
		void NkFluidGrid::VelocityAtGrid(const NkVector<float32> &fu, const NkVector<float32> &fv,
										 const NkVector<float32> &fw, float32 gx, float32 gy, float32 gz,
										 float32 &vx, float32 &vy, float32 &vz) const {
			vx = Trilinear(fu, gx + 0.5f, gy, gz);
			vy = Trilinear(fv, gx, gy + 0.5f, gz);
			vz = Trilinear(fw, gx, gy, gz + 0.5f);
		}

		void NkFluidGrid::BacktraceAt(float32 gx, float32 gy, float32 gz, float32 dt0, const NkVector<float32> &fu,
									  const NkVector<float32> &fv, const NkVector<float32> &fw, float32 &x,
									  float32 &y, float32 &z) const {
			float32 vx, vy, vz;
			VelocityAtGrid(fu, fv, fw, gx, gy, gz, vx, vy, vz);
			if (!mParams.advectRK2) {
				x = gx - dt0 * vx;
				y = gy - dt0 * vy;
				z = gz - dt0 * vz;
				return;
			}
			const float32 xm = gx - 0.5f * dt0 * vx;
			const float32 ym = gy - 0.5f * dt0 * vy;
			const float32 zm = gz - 0.5f * dt0 * vz;
			float32 mx, my, mz;
			VelocityAtGrid(fu, fv, fw, xm, ym, zm, mx, my, mz);
			x = gx - dt0 * mx;
			y = gy - dt0 * my;
			z = gz - dt0 * mz;
		}

		void NkFluidGrid::AdvectSemiLagrangien(NkVector<float32> &dst, const NkVector<float32> &src, float32 dt,
												   int32 bnd, float32 sens) {
			const float32 dt0 = sens * dt / mParams.cellSize; // pas en CELLULES
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						float32 x, y, z;
						// Un SCALAIRE vit au CENTRE : son point de départ se rebrousse
						// depuis (i,j,k), et la vitesse y est interpolée depuis les faces.
						BacktraceAt((float32)i, (float32)j, (float32)k, dt0, mU, mV, mW, x, y, z);
						dst[Idx(i, j, k)] = Trilinear(src, x, y, z);
					}
			SetBoundary(bnd, dst);
		}

		// Les huit valeurs du pochoir trilineaire autour de (x,y,z) : le limiteur du
		// schema MacCormack borne le resultat corrige a cet intervalle, sinon le
		// schema n'est plus borne et il fabrique des valeurs qui n'existaient pas.
		void NkFluidGrid::TrilinearBornes(const NkVector<float32> &f, float32 x, float32 y, float32 z, float32 &mn,
										  float32 &mx) const {
			x = Clampf(x, 0.5f, (float32)mNx + 0.5f);
			y = Clampf(y, 0.5f, (float32)mNy + 0.5f);
			z = Clampf(z, 0.5f, (float32)mNz + 0.5f);
			const uint32 i0 = (uint32)x, j0 = (uint32)y, k0 = (uint32)z;
			mn = 1.0e30f;
			mx = -1.0e30f;
			for (uint32 dk = 0; dk < 2; ++dk)
				for (uint32 dj = 0; dj < 2; ++dj)
					for (uint32 di = 0; di < 2; ++di) {
						const float32 v = f[Idx(i0 + di, j0 + dj, k0 + dk)];
						if (v < mn)
							mn = v;
						if (v > mx)
							mx = v;
					}
		}

		// =====================================================================
		// ADVECTION CONSERVATIVE EN FLUX — donor-cell (décentrement amont), ordre 1.
		// Courant, Isaacson & Rees 1952 ; cadre : Lentine, Aanjaneya & Fedkiw, SCA 2011.
		//
		// LE PRINCIPE, et c'est lui seul qui fait la conservation : on ne demande
		// plus « d'où vient ce qui arrive ici ? » — la question du semi-lagrangien,
		// qui n'a AUCUN bilan — mais « combien traverse CETTE FACE ? ». Un flux est
		// calculé UNE fois par face, RETRANCHÉ à la cellule amont et AJOUTÉ à la
		// cellule aval : la somme sur l'intérieur est donc conservée par
		// construction, au bit près. C'est ce qui exige la grille DÉCALÉE.
		//
		// Les faces de PAROI ne sont jamais parcourues et portent u = 0 : rien ne
		// peut quitter le domaine, ce que la garde de (f1) vérifie.
		//
		// ⚠️ Tous les flux sont calculés depuis `src` — l'état au DÉBUT du sous-pas
		// — et jamais depuis `dst` en cours de modification : sinon le schéma
		// cesserait d'être explicite, et l'ordre de parcours changerait le résultat.
		// =====================================================================
		void NkFluidGrid::AdvectFluxUnePasse(NkVector<float32> &dst, const NkVector<float32> &src, float32 dt,
											 int32 bnd) {
			const float32 dt0 = dt / mParams.cellSize; // pas en CELLULES
			const uint32 sy = mNx + 2;
			const uint32 sz = (mNx + 2) * (mNy + 2);

			for (uint32 i = 0; i < mCount; ++i)
				dst[i] = src[i];

			// Faces x INTERNES : la face i sépare les cellules i-1 et i.
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 2; i <= mNx; ++i) {
						const uint32 id = Idx(i, j, k);
						const float32 u = mU[id];
						// DÉCENTREMENT AMONT : le donneur est la cellule d'où le
						// fluide VIENT. C'est ce qui rend le schéma borné — il ne
						// fabrique jamais une valeur qui n'existait pas.
						const float32 phi = (u > 0.f) ? src[id - 1] : src[id];
						const float32 F = u * dt0 * phi;
						dst[id - 1] -= F;
						dst[id] += F;
					}
			// Faces y INTERNES
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 2; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						const uint32 id = Idx(i, j, k);
						const float32 v = mV[id];
						const float32 phi = (v > 0.f) ? src[id - sy] : src[id];
						const float32 F = v * dt0 * phi;
						dst[id - sy] -= F;
						dst[id] += F;
					}
			// Faces z INTERNES
			for (uint32 k = 2; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						const uint32 id = Idx(i, j, k);
						const float32 w = mW[id];
						const float32 phi = (w > 0.f) ? src[id - sz] : src[id];
						const float32 F = w * dt0 * phi;
						dst[id - sz] -= F;
						dst[id] += F;
					}
			SetBoundary(bnd, dst);
		}

		// Le nombre de Courant MAXIMAL vu sur l'intérieur, pour le pas `dt`.
		// Sur grille décalée la condition porte sur les vitesses de FACE : on prend,
		// par axe, la plus grande des deux faces de la cellule, et on somme les trois
		// axes — c'est la forme 3D NON-SPLITTÉE du critère, celle que ce schéma exige.
		float32 NkFluidGrid::MaxCFL(float32 dt) const {
			const float32 dt0 = dt / mParams.cellSize;
			const uint32 sy = mNx + 2;
			const uint32 sz = (mNx + 2) * (mNy + 2);
			float32 mx = 0.f;
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						const uint32 id = Idx(i, j, k);
						const float32 au = NkMax(NkAbs(mU[id]), NkAbs(mU[id + 1]));
						const float32 av = NkMax(NkAbs(mV[id]), NkAbs(mV[id + sy]));
						const float32 aw = NkMax(NkAbs(mW[id]), NkAbs(mW[id + sz]));
						const float32 c = (au + av + aw) * dt0;
						if (c > mx)
							mx = c;
					}
			return mx;
		}

		// SOUS-CYCLAGE — ce n'est PAS un contournement, c'est ce que le schéma EXIGE.
		// Un schéma conditionnellement stable qu'on fait tourner hors de sa condition
		// ne « marche presque » pas : il DIVERGE. On découpe donc le pas en sous-pas
		// qui respectent le CFL.
		//
		// ⚠️ SEULS LES SCALAIRES sont sous-cyclés, et on NE RE-PROJETTE PAS entre les
		// sous-pas : la vitesse ne change pas dans l'intervalle, la projection n'a
		// donc rien à refaire — et c'est ce qui garde le coût borné.
		void NkFluidGrid::AdvectScalarFlux(NkVector<float32> &dst, const NkVector<float32> &src, float32 dt,
										   int32 bnd) {
			const uint32 n = (mStats.advectSubsteps > 0) ? mStats.advectSubsteps : 1u;
			if (n == 1) {
				AdvectFluxUnePasse(dst, src, dt, bnd);
				return;
			}
			const float32 dts = dt / (float32)n;
			AdvectFluxUnePasse(dst, src, dts, bnd);
			for (uint32 s = 1; s < n; ++s) {
				AdvectFluxUnePasse(mScratchA, dst, dts, bnd);
				for (uint32 i = 0; i < mCount; ++i)
					dst[i] = mScratchA[i];
			}
		}

		// Advection d'un scalaire : semi-lagrangien seul, ou corrige MacCormack.
		void NkFluidGrid::AdvectScalar(NkVector<float32> &dst, const NkVector<float32> &src, float32 dt, int32 bnd) {
			// L'advection CONSERVATIVE EN FLUX passe devant : c'est un AUTRE schéma,
			// pas une correction du semi-lagrangien, et MacCormack ne s'y applique
			// pas. On mesure le PREMIER ORDRE NU avant tout limiteur (§ 2 du plan) —
			// sinon on ne saurait jamais ce que le limiteur a rendu.
			if (mParams.advectFluxConservative) {
				AdvectScalarFlux(dst, src, dt, bnd);
				return;
			}
			if (!mParams.advectMacCormack) {
				AdvectSemiLagrangien(dst, src, dt, bnd, 1.f);
				return;
			}
			// 1. aller  : phi_chapeau = A(phi)
			AdvectSemiLagrangien(mScratchA, src, dt, bnd, 1.f);
			// 2. retour : phi_tilde = A_inverse(phi_chapeau)
			AdvectSemiLagrangien(mScratchB, mScratchA, dt, bnd, -1.f);
			// 3. correction de la MOITIE de l'erreur d'aller-retour + limiteur
			const float32 dt0 = dt / mParams.cellSize;
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						const uint32 id = Idx(i, j, k);
						float32 x, y, z, mn, mx;
						BacktraceAt((float32)i, (float32)j, (float32)k, dt0, mU, mV, mW, x, y, z);
						TrilinearBornes(src, x, y, z, mn, mx);
						const float32 v = mScratchA[id] + 0.5f * (src[id] - mScratchB[id]);
						dst[id] = Clampf(v, mn, mx);
					}
			SetBoundary(bnd, dst);
		}

		// =====================================================================
		// LES BORDS DE VITESSE sur grille décalée.
		//
		// Rien à voir avec `SetBoundary` : la composante NORMALE n'est plus
		// « inversée dans un fantôme » pour que la moyenne s'annule à l'interface,
		// elle est imposée NULLE **sur** la face de paroi, exactement là où la paroi
		// est. C'est le gain silencieux de la bascule — l'ancienne convention
		// donnait à la couche collée aux parois une divergence apparente que le
		// solveur ne pouvait pas faire tomber (mesure du 05/09 : 5,408 % -> 5,407 %
		// en forçant le résidu 78 fois plus bas).
		//
		// Les couches fantômes TANGENTIELLES sont recopiées (glissement libre) pour
		// que l'interpolation et les différences restent définies au bord.
		// =====================================================================
		void NkFluidGrid::SetVelocityWalls() {
			const uint32 X = mNx, Y = mNy, Z = mNz;

			// 1. vitesse NORMALE nulle sur les six parois
			for (uint32 k = 1; k <= Z; ++k)
				for (uint32 j = 1; j <= Y; ++j) {
					mU[Idx(1, j, k)] = 0.f;
					mU[Idx(X + 1, j, k)] = 0.f;
				}
			for (uint32 k = 1; k <= Z; ++k)
				for (uint32 i = 1; i <= X; ++i) {
					mV[Idx(i, 1, k)] = 0.f;
					mV[Idx(i, Y + 1, k)] = 0.f;
				}
			for (uint32 j = 1; j <= Y; ++j)
				for (uint32 i = 1; i <= X; ++i) {
					mW[Idx(i, j, 1)] = 0.f;
					mW[Idx(i, j, Z + 1)] = 0.f;
				}

			// 2. couches fantômes TANGENTIELLES recopiées (glissement libre)
			for (uint32 k = 0; k <= Z + 1; ++k)
				for (uint32 i = 0; i <= X + 1; ++i) {
					mU[Idx(i, 0, k)] = mU[Idx(i, 1, k)];
					mU[Idx(i, Y + 1, k)] = mU[Idx(i, Y, k)];
					mW[Idx(i, 0, k)] = mW[Idx(i, 1, k)];
					mW[Idx(i, Y + 1, k)] = mW[Idx(i, Y, k)];
				}
			for (uint32 j = 0; j <= Y + 1; ++j)
				for (uint32 i = 0; i <= X + 1; ++i) {
					mU[Idx(i, j, 0)] = mU[Idx(i, j, 1)];
					mU[Idx(i, j, Z + 1)] = mU[Idx(i, j, Z)];
					mV[Idx(i, j, 0)] = mV[Idx(i, j, 1)];
					mV[Idx(i, j, Z + 1)] = mV[Idx(i, j, Z)];
				}
			for (uint32 k = 0; k <= Z + 1; ++k)
				for (uint32 j = 0; j <= Y + 1; ++j) {
					mV[Idx(0, j, k)] = mV[Idx(1, j, k)];
					mV[Idx(X + 1, j, k)] = mV[Idx(X, j, k)];
					mW[Idx(0, j, k)] = mW[Idx(1, j, k)];
					mW[Idx(X + 1, j, k)] = mW[Idx(X, j, k)];
				}
		}

		void NkFluidGrid::AdvectVelocity(float32 dt) {
			// La vitesse s'advecte ELLE-MÊME : on la copie d'abord (u0), et on lit u0.
			for (uint32 i = 0; i < mCount; ++i) {
				mU0[i] = mU[i];
				mV0[i] = mV[i];
				mW0[i] = mW[i];
			}
			const float32 dt0 = dt / mParams.cellSize;
			// CHAQUE COMPOSANTE EST REBROUSSÉE DEPUIS SA PROPRE FACE — c'est ce que
			// le § 4.4 du plan demande, et ça ne se sépare pas du § 4.3 : une grille
			// décalée dont l'advection traiterait les faces comme des centres ne
			// produit aucun écoulement dont on puisse mesurer la divergence.
			// La face x d'indice i est au point de grille (i - 0,5 ; j ; k), et on y
			// relit u avec le même décalage de +0,5.
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 2; i <= mNx; ++i) {
						float32 x, y, z;
						BacktraceAt((float32)i - 0.5f, (float32)j, (float32)k, dt0, mU0, mV0, mW0, x, y, z);
						mU[Idx(i, j, k)] = Trilinear(mU0, x + 0.5f, y, z);
					}
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 2; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						float32 x, y, z;
						BacktraceAt((float32)i, (float32)j - 0.5f, (float32)k, dt0, mU0, mV0, mW0, x, y, z);
						mV[Idx(i, j, k)] = Trilinear(mV0, x, y + 0.5f, z);
					}
			for (uint32 k = 2; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						float32 x, y, z;
						BacktraceAt((float32)i, (float32)j, (float32)k - 0.5f, dt0, mU0, mV0, mW0, x, y, z);
						mW[Idx(i, j, k)] = Trilinear(mW0, x, y, z + 0.5f);
					}
			SetVelocityWalls();
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
						// GRILLE DÉCALÉE : divergence COMPACTE des six faces de la
						// cellule. div_vraie = ((u[i+1]-u[i]) + ...)/h, et le
						// potentiel de Stam veut div_stam = -h^2 * div_vraie, d'où
						// le facteur -h (et non -0,5*h, qui était le prix de la
						// différence centrée de pas 2).
						D[id] = -h * ((U[id + 1] - U[id]) + (V[id + sy] - V[id]) + (W[id + sz] - W[id]));
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

			// u <- u - grad(p), AUX FACES.
			//
			// La face x d'indice i SÉPARE les cellules i-1 et i : le gradient y vaut
			// exactement (p[i] - p[i-1])/h, une différence COMPACTE. C'est l'adjoint
			// exact de la divergence ci-dessus, et c'est toute la raison d'être de la
			// bascule : les deux opérateurs sont enfin le même, la grille ne se
			// découple plus en sous-réseaux pair/impair.
			float32 *Um = mU.Data();
			float32 *Vm = mV.Data();
			float32 *Wm = mW.Data();
			const float32 invh = 1.f / h;
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 2; i <= mNx; ++i) { // faces INTERNES en x
						const uint32 id = Idx(i, j, k);
						Um[id] -= invh * (P[id] - P[id - 1]);
					}
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 2; j <= mNy; ++j) // faces INTERNES en y
					for (uint32 i = 1; i <= mNx; ++i) {
						const uint32 id = Idx(i, j, k);
						Vm[id] -= invh * (P[id] - P[id - sy]);
					}
			for (uint32 k = 2; k <= mNz; ++k) // faces INTERNES en z
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						const uint32 id = Idx(i, j, k);
						Wm[id] -= invh * (P[id] - P[id - sz]);
					}

			// LES PAROIS : vitesse NORMALE nulle *sur la face*, et fantômes
			// tangentiels recopiés. C'est le gain silencieux de la grille décalée —
			// la condition de non-pénétration s'écrit exactement là où la paroi est,
			// au lieu d'être approchée par une couche fantôme portant -u_normal.
			SetVelocityWalls();
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
			// GRILLE DÉCALÉE : la poussée est une force VERTICALE, elle vit donc sur
			// les faces y. La face y d'indice j SÉPARE les cellules (i,j-1,k) et
			// (i,j,k) : la température et la densité qu'elle voit sont la MOYENNE
			// des deux cellules qu'elle borde. Écrire `mTemperature[id]` seul
			// décalerait la force d'une demi-cellule vers le haut — une erreur qui
			// ne se verrait sur aucun témoin de loi, seulement sur le transport.
			//
			// Faces INTERNES seulement (j = 2..ny) : sur les faces de PAROI la
			// vitesse normale est imposée nulle, et y ajouter une force la ferait
			// mentir.
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 2; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						const uint32 id = Idx(i, j, k);
						const uint32 bas = Idx(i, j - 1, k);
						const float32 T = 0.5f * (mTemperature[id] + mTemperature[bas]);
						const float32 d = 0.5f * (mDensity[id] + mDensity[bas]);
						mV[id] += dt * (beta * (T - tAmb) - alpha * d);
					}
			// Les fantômes tangentiels doivent suivre : ComputeVorticity lit
			// vc(i,j,k±1), qui touche la couche fantôme.
			SetVelocityWalls();
		}

		// =====================================================================
		// VORTICITE — omega = rot(u), par differences CENTREES (Fedkiw 2001, eq. 9).
		//
		// Sur un champ LINEAIRE en espace, les differences centrees sont EXACTES : c'est
		// ce qui permet au banc de calibrer l'instrument sur une rotation solide
		// u = Omega x r, dont le rotationnel vaut 2*Omega exactement, avant de croire le
		// moindre chiffre d'enstrophie.
		//
		// POPULATION : omega est calcule sur les nx*ny*nz cellules interieures (les
		// voisins lus peuvent etre la couche fantome), puis |omega| est recopie sur les
		// bords (SetBoundary scalaire) pour que le gradient de |omega| soit defini
		// jusqu'au bord de l'interieur. Les STATISTIQUES, elles, ne comptent que
		// l'interieur STRICT (2..N-1) — la meme population que la divergence stricte, et
		// pour la meme raison : la couche collee aux parois porte une vorticite de
		// CONVENTION (tangentielle recopiee, normale inversee), pas celle du fluide.
		// =====================================================================
		void NkFluidGrid::ComputeVorticity() {
			const float32 inv2h = 1.f / (2.f * mParams.cellSize);
			// ⚠️ ÉCART ASSUMÉ AU PLAN, ET DIT PLUTÔT QU'ENFOUI. Le § 2 de
			// PLAN_GRILLE_MAC.md prévoyait de porter omega sur les ARÊTES, et donc
			// que sa valeur absolue CHANGE (autre population). Je le garde AU CENTRE
			// des cellules, calculé à partir des faces moyennées. Trois raisons, et
			// la troisième est la bonne :
			//   1. c'est là que Fedkiw 2001 calcule le confinement, et le confinement
			//      est le seul consommateur de omega ;
			//   2. les arêtes exigeraient trois réseaux distincts et un gradient de
			//      |omega| défini sur un quatrième ;
			//   3. surtout : la population comptée reste la MÊME, donc l'ancrage
			//      « enstrophie 1,458000 sur la rotation solide » reste VÉRIFIABLE.
			//      Changer de population aurait rendu ce témoin incomparable, et on
			//      aurait perdu la seule garde qui dise que l'instrument n'a pas bougé.
			// La moyenne de deux faces est EXACTE sur un champ linéaire : l'écart de
			// 0,0000 % sur la rotation solide doit donc rester nul. S'il cesse de
			// l'être, c'est l'instrument qui a bougé, pas la physique.
			//
			// Les moyennes sont écrites en ligne plutôt que via VelocityAtCenter :
			// appelée sur (i+1), celle-ci lirait la face (i+2), qui déborde de la
			// rangée quand i vaut nx. Ici tous les indices restent <= N+1.
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						const uint32 id = Idx(i, j, k);
						// vitesses AU CENTRE des cellules voisines (moyenne des 2 faces)
						auto uc = [&](uint32 a, uint32 b, uint32 c) {
							return 0.5f * (mU[Idx(a, b, c)] + mU[Idx(a + 1, b, c)]);
						};
						auto vc = [&](uint32 a, uint32 b, uint32 c) {
							return 0.5f * (mV[Idx(a, b, c)] + mV[Idx(a, b + 1, c)]);
						};
						auto wc = [&](uint32 a, uint32 b, uint32 c) {
							return 0.5f * (mW[Idx(a, b, c)] + mW[Idx(a, b, c + 1)]);
						};
						// omega = ( dw/dy - dv/dz , du/dz - dw/dx , dv/dx - du/dy )
						const float32 ox = (wc(i, j + 1, k) - wc(i, j - 1, k)) * inv2h -
										   (vc(i, j, k + 1) - vc(i, j, k - 1)) * inv2h;
						const float32 oy = (uc(i, j, k + 1) - uc(i, j, k - 1)) * inv2h -
										   (wc(i + 1, j, k) - wc(i - 1, j, k)) * inv2h;
						const float32 oz = (vc(i + 1, j, k) - vc(i - 1, j, k)) * inv2h -
										   (uc(i, j + 1, k) - uc(i, j - 1, k)) * inv2h;
						mOmegaX[id] = ox;
						mOmegaY[id] = oy;
						mOmegaZ[id] = oz;
						mOmegaMag[id] = NkSqrt(ox * ox + oy * oy + oz * oz);
					}
			SetBoundary(0, mOmegaMag); // Neumann : le gradient de |omega| reste defini au bord

			// Statistiques sur l'interieur STRICT, population dite dans mStats.cellsStrict.
			float64 sum = 0.0, sum2 = 0.0;
			float32 mx = 0.f;
			uint32 n = 0;
			if (mNx > 2 && mNy > 2 && mNz > 2) {
				for (uint32 k = 2; k <= mNz - 1; ++k)
					for (uint32 j = 2; j <= mNy - 1; ++j)
						for (uint32 i = 2; i <= mNx - 1; ++i) {
							const float32 m = mOmegaMag[Idx(i, j, k)];
							sum += (float64)m;
							sum2 += (float64)m * (float64)m;
							if (m > mx)
								mx = m;
							++n;
						}
			}
			const float32 h = mParams.cellSize;
			mStats.vorticityMean = (n > 0) ? (float32)(sum / (float64)n) : 0.f;
			mStats.vorticityMax = mx;
			mStats.enstrophy = (float32)(sum2 * (float64)(h * h * h));
		}

		// =====================================================================
		// CONFINEMENT DE VORTICITE — Fedkiw, Stam & Jensen 2001, § 4, eq. (11) :
		//     f_conf = epsilon * h * ( N x omega ),  N = grad|omega| / |grad|omega||
		// On l'ajoute a la vitesse comme une acceleration (le papier ecrit l'equation
		// de quantite de mouvement a densite unite).
		//
		// Un epsilon NEGATIF inverse la force : c'est la MUTATION du banc, pas un
		// reglage. Elle doit faire CHUTER l'enstrophie — sans elle, « l'enstrophie
		// augmente » ne prouverait pas que le SIGNE du produit vectoriel est le bon.
		// =====================================================================
		void NkFluidGrid::AddVorticityConfinement(float32 dt) {
			mStats.confinementAccelMean = 0.f;
			const float32 eps = mParams.vorticityConfinement;
			if (eps == 0.f)
				return;
			const float32 h = mParams.cellSize;
			const float32 inv2h = 1.f / (2.f * h);
			const float32 k1 = eps * h;
			const uint32 sy = mNx + 2;
			const uint32 sz = (mNx + 2) * (mNy + 2);
			// PASSE 1 — l'accélération AU CENTRE des cellules (omega et grad|omega| y
			// vivent). On emprunte trois tampons déjà alloués : mScratchA/B, libres
			// hors MacCormack, et mDivergence, que `Project` recalcule INTÉGRALEMENT
			// juste après. Aucun code ne lit ces trois-là entre ici et la projection.
			float32 *AX = mScratchA.Data();
			float32 *AY = mScratchB.Data();
			float32 *AZ = mDivergence.Data();
			float64 accSum = 0.0;
			uint32 n = 0;
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						const uint32 id = Idx(i, j, k);
						AX[id] = 0.f;
						AY[id] = 0.f;
						AZ[id] = 0.f;
						// eta = grad |omega|
						const float32 ex = (mOmegaMag[Idx(i + 1, j, k)] - mOmegaMag[Idx(i - 1, j, k)]) * inv2h;
						const float32 ey = (mOmegaMag[Idx(i, j + 1, k)] - mOmegaMag[Idx(i, j - 1, k)]) * inv2h;
						const float32 ez = (mOmegaMag[Idx(i, j, k + 1)] - mOmegaMag[Idx(i, j, k - 1)]) * inv2h;
						const float32 len = NkSqrt(ex * ex + ey * ey + ez * ez);
						// La ou |grad|omega|| est nul, N n'est pas defini : on n'invente pas
						// une direction, on n'applique AUCUNE force (Fedkiw, note de bas de
						// page du § 4). Le seuil est relatif a la vorticite locale.
						if (len < 1.0e-12f)
							continue;
						const float32 inv = 1.f / len;
						const float32 nx = ex * inv, ny = ey * inv, nz = ez * inv;
						const float32 ox = mOmegaX[id], oy = mOmegaY[id], oz = mOmegaZ[id];
						// N x omega
						const float32 fx = ny * oz - nz * oy;
						const float32 fy = nz * ox - nx * oz;
						const float32 fz = nx * oy - ny * ox;
						AX[id] = k1 * fx;
						AY[id] = k1 * fy;
						AZ[id] = k1 * fz;
						accSum += (float64)NkSqrt(AX[id] * AX[id] + AY[id] * AY[id] + AZ[id] * AZ[id]);
						++n;
					}
			// PASSE 2 — la force part du CENTRE et arrive sur les FACES, moyennée
			// entre les deux cellules que chaque face sépare. Faces INTERNES seules.
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 2; i <= mNx; ++i) {
						const uint32 id = Idx(i, j, k);
						mU[id] += dt * 0.5f * (AX[id] + AX[id - 1]);
					}
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 2; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						const uint32 id = Idx(i, j, k);
						mV[id] += dt * 0.5f * (AY[id] + AY[id - sy]);
					}
			for (uint32 k = 2; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						const uint32 id = Idx(i, j, k);
						mW[id] += dt * 0.5f * (AZ[id] + AZ[id - sz]);
					}
			mStats.confinementAccelMean = (n > 0) ? (float32)(accSum / (float64)n) : 0.f;
			SetVelocityWalls();
		}

		// =====================================================================
		// VENT — le champ de force externe commun (math::NkIForceField, NKMath).
		// CONTRAT : Force() rend des NEWTONS ; on divise par la masse DECLAREE de la
		// cellule (`fieldParticleMass`, kg), exactement comme les particules divisent
		// par `NkEmitterDesc::particleMass` et le SPH par sa masse calibree. On suit la
		// convention, on ne la reinvente pas.
		// =====================================================================
		void NkFluidGrid::AddWind(float32 dt) {
			mStats.windAccelMean = 0.f;
			if (mParams.field == nullptr || !mParams.fieldEnabled)
				return;
			const float32 m = mParams.fieldParticleMass;
			if (m <= 0.f)
				return; // une masse nulle ou negative n'a pas de sens : on refuse plutot que diviser
			const float32 invM = 1.f / m;
			const float32 h = mParams.cellSize;
			float64 accSum = 0.0;
			uint32 n = 0;
			// LA STATISTIQUE se mesure AU CENTRE des cellules : c'est la population
			// que le témoin (w1) lit, et elle doit rester |F|/m — 0,5000 m/s^2 pour
			// une force uniforme de 0,5 N sur 1 kg. La mesurer par face donnerait 0
			// sur les faces perpendiculaires a la force et fausserait le chiffre.
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						const NkVec3f p = {mParams.boundsMin.x + ((float32)i - 0.5f) * h,
										   mParams.boundsMin.y + ((float32)j - 0.5f) * h,
										   mParams.boundsMin.z + ((float32)k - 0.5f) * h};
						const NkVec3f F = mParams.field->Force(p, mFieldTime); // newtons
						const float32 ax = F.x * invM, ay = F.y * invM, az = F.z * invM;
						accSum += (float64)NkSqrt(ax * ax + ay * ay + az * az);
						++n;
					}
			// LA FORCE, elle, s'applique sur les FACES, et chacune est évaluée A LA
			// POSITION DE SA FACE : la face x d'indice i est a x = boundsMin.x +
			// (i-1)*h, mais reste au MILIEU de la cellule en y et z.
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 2; i <= mNx; ++i) {
						const NkVec3f p = {mParams.boundsMin.x + ((float32)i - 1.f) * h,
										   mParams.boundsMin.y + ((float32)j - 0.5f) * h,
										   mParams.boundsMin.z + ((float32)k - 0.5f) * h};
						mU[Idx(i, j, k)] += dt * mParams.field->Force(p, mFieldTime).x * invM;
					}
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 j = 2; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						const NkVec3f p = {mParams.boundsMin.x + ((float32)i - 0.5f) * h,
										   mParams.boundsMin.y + ((float32)j - 1.f) * h,
										   mParams.boundsMin.z + ((float32)k - 0.5f) * h};
						mV[Idx(i, j, k)] += dt * mParams.field->Force(p, mFieldTime).y * invM;
					}
			for (uint32 k = 2; k <= mNz; ++k)
				for (uint32 j = 1; j <= mNy; ++j)
					for (uint32 i = 1; i <= mNx; ++i) {
						const NkVec3f p = {mParams.boundsMin.x + ((float32)i - 0.5f) * h,
										   mParams.boundsMin.y + ((float32)j - 0.5f) * h,
										   mParams.boundsMin.z + ((float32)k - 1.f) * h};
						mW[Idx(i, j, k)] += dt * mParams.field->Force(p, mFieldTime).z * invM;
					}
			mStats.windAccelMean = (n > 0) ? (float32)(accSum / (float64)n) : 0.f;
			SetVelocityWalls();
		}

		// =====================================================================
		// RAYON DE GIRATION HORIZONTAL — ce qui separe un JET d'un PANACHE.
		// Tranche horizontale la plus proche de worldY ; barycentre (x,z) pondere par la
		// densite, puis racine de la moyenne des carres des distances a ce barycentre.
		// Pour une gaussienne 2D d'ecart-type sigma, ce rayon vaut sigma * racine(2) :
		// c'est le controle POSITIF de l'instrument, et le banc le passe avant de s'en
		// servir. La MASSE de la tranche est publiee avec le rayon : comparer deux
		// rayons sans comparer les masses comparerait deux populations differentes.
		// =====================================================================
		bool NkFluidGrid::PlumeRadius(float32 worldY, float32 &radiusOut, float32 &sliceMassOut,
									  uint32 &rowOut) const {
			if (!mReady)
				return false;
			const float32 h = mParams.cellSize;
			float32 fj = (worldY - mParams.boundsMin.y) / h + 0.5f;
			if (fj < 1.f)
				fj = 1.f;
			if (fj > (float32)mNy)
				fj = (float32)mNy;
			const uint32 j = (uint32)(fj + 0.5f) < 1u ? 1u : ((uint32)(fj + 0.5f) > mNy ? mNy : (uint32)(fj + 0.5f));

			float64 w = 0.0, sx = 0.0, sz = 0.0;
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 i = 1; i <= mNx; ++i) {
					const float32 d = mDensity[Idx(i, j, k)];
					if (d <= 0.f)
						continue;
					const float64 x = (float64)(mParams.boundsMin.x + ((float32)i - 0.5f) * h);
					const float64 z = (float64)(mParams.boundsMin.z + ((float32)k - 0.5f) * h);
					w += (float64)d;
					sx += (float64)d * x;
					sz += (float64)d * z;
				}
			if (w <= 0.0)
				return false;
			const float64 cx = sx / w, cz = sz / w;
			float64 s2 = 0.0;
			for (uint32 k = 1; k <= mNz; ++k)
				for (uint32 i = 1; i <= mNx; ++i) {
					const float32 d = mDensity[Idx(i, j, k)];
					if (d <= 0.f)
						continue;
					const float64 x = (float64)(mParams.boundsMin.x + ((float32)i - 0.5f) * h);
					const float64 z = (float64)(mParams.boundsMin.z + ((float32)k - 0.5f) * h);
					s2 += (float64)d * ((x - cx) * (x - cx) + (z - cz) * (z - cz));
				}
			radiusOut = (float32)NkSqrt((float32)(s2 / w));
			sliceMassOut = (float32)(w * (float64)(h * h * h));
			rowOut = j;
			return true;
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
		void NkFluidGrid::MeasureDivergence(float32 &meanOut, float32 &maxOut, bool strict) const {
			// divergence * h, en m/s. GRILLE DÉCALÉE (MAC) : la divergence d'une
			// cellule est la DIFFÉRENCE COMPACTE de ses deux faces opposées,
			//     div*h = (u[i+1] - u[i]) + (v[j+1] - v[j]) + (w[k+1] - w[k])
			// où u[i] est la face GAUCHE de la cellule (i,j,k) et u[i+1] sa face
			// DROITE (qui est la face gauche de la cellule i+1 : une case, une face).
			//
			// ⚠️ C'EST CE STENCIL, ET NON LA DIFFÉRENCE CENTRÉE DE PAS 2, qui est
			// l'ADJOINT EXACT du Laplacien à 6 voisins que résout la projection. La
			// différence centrée lisait u[i+1] - u[i-1] : deux cases de MÊME parité.
			// Elle découplait la grille en sous-réseaux pair/impair (le mode
			// « damier »), ce qui donnait au témoin (b) un PLANCHER DE
			// DISCRÉTISATION que plus aucun balayage ne pouvait franchir — mesuré
			// le 05/09 : résidu divisé par 102, rapport inchangé au dix-millième.
			//
			// Conséquence directe, et c'est le contrôle (m2) : ce stencil VOIT le
			// damier (il en rend 2,000000 m/s) là où le centré rendait ZÉRO. Et il
			// reste AVEUGLE à un champ uniforme, qui rend toujours 0 — sans quoi
			// « non nul » ne serait pas un critère.
			float64 sum = 0.0;
			float32 mx = 0.f;
			uint32 n = 0;
			const uint32 i0 = strict ? 2u : 1u, j0 = strict ? 2u : 1u, k0 = strict ? 2u : 1u;
			const uint32 i1 = strict ? (mNx > 1 ? mNx - 1 : 0) : mNx;
			const uint32 j1 = strict ? (mNy > 1 ? mNy - 1 : 0) : mNy;
			const uint32 k1 = strict ? (mNz > 1 ? mNz - 1 : 0) : mNz;
			for (uint32 k = k0; k <= k1; ++k)
				for (uint32 j = j0; j <= j1; ++j)
					for (uint32 i = i0; i <= i1; ++i) {
						const float32 d = (mU[Idx(i + 1, j, k)] - mU[Idx(i, j, k)]) +
										  (mV[Idx(i, j + 1, k)] - mV[Idx(i, j, k)]) +
										  (mW[Idx(i, j, k + 1)] - mW[Idx(i, j, k)]);
						const float32 a = NkAbs(d);
						sum += (float64)a;
						++n;
						if (a > mx)
							mx = a;
					}
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
						// La VITESSE D'UNE CELLULE est celle de son CENTRE : sur grille
						// décalée, lire mU[id] seul lirait une FACE et sous-estimerait
						// (ou surestimerait) la vitesse de la cellule d'un demi-pas.
						float32 cx, cy, cz;
						VelocityAtCenter(i, j, k, cx, cy, cz);
						float32 s = NkSqrt(cx * cx + cy * cy + cz * cz);
						if (s > lim && s > 0.f) {
							// La borne mord au centre : on met à l'échelle les SIX faces
							// de la cellule, sans quoi le champ resterait au-dessus.
							const float32 f = lim / s;
							mU[id] *= f;
							mU[Idx(i + 1, j, k)] *= f;
							mV[id] *= f;
							mV[Idx(i, j + 1, k)] *= f;
							mW[id] *= f;
							mW[Idx(i, j, k + 1)] *= f;
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

			// La MEME population que la divergence stricte, sinon le rapport compare
			// deux ensembles differents -- le defaut qu'on vient justement de trouver.
			float64 sums = 0.0;
			uint32 ns = 0;
			if (mNx > 2 && mNy > 2 && mNz > 2) {
				for (uint32 k = 2; k <= mNz - 1; ++k)
					for (uint32 j = 2; j <= mNy - 1; ++j)
						for (uint32 i = 2; i <= mNx - 1; ++i) {
							float32 cx, cy, cz;
							VelocityAtCenter(i, j, k, cx, cy, cz);
							sums += (float64)NkSqrt(cx * cx + cy * cy + cz * cz);
							++ns;
						}
			}
			mStats.cellsStrict = ns;
			mStats.velocityMeanStrict = (ns > 0) ? (float32)(sums / (float64)ns) : 0.f;
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
						// La vitesse verticale D'UNE CELLULE est celle de son CENTRE :
						// la moyenne de ses deux faces y. Lire `mV[id]` seul lirait la
						// face BASSE et décalerait la mesure d'un demi-pas — c'est
						// exactement l'erreur que le contrôle (m1) rend visible, et
						// c'est ce témoin-ci, (c1), qui la paierait.
						float32 cx, cy, cz;
						VelocityAtCenter(i, j, k, cx, cy, cz);
						wsum += (float64)w;
						vsum += (float64)w * (float64)cy;
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
			// Les FORCES, dans l'ordre de Fedkiw 2001 (§ 4) : flottabilite, puis
			// confinement de vorticite, puis les forces externes ; ensuite seulement
			// l'advection, puis la projection. Le confinement lit omega du champ de
			// vitesse COURANT : la vorticite se calcule donc juste avant.
			ComputeVorticity();
			AddVorticityConfinement(dt);
			AddWind(dt);
			mFieldTime += dt;

			if (mParams.advectionEnabled)
				AdvectVelocity(dt);

			MeasureDivergence(mStats.divBeforeMean, mStats.divBeforeMax, false);

			if (mParams.projectionEnabled)
				Project(dt);
			else {
				mStats.pressureIters = 0;
				mStats.pressureResidual = 0.f;
				mStats.pressureCapHit = false;
			}

			MeasureDivergence(mStats.divAfterMean, mStats.divAfterMax, false);
			MeasureDivergence(mStats.divAfterMeanStrict, mStats.divAfterMaxStrict, true);

			// LE NOMBRE DE COURANT ET LE SOUS-CYCLAGE — calculés ICI, APRÈS la
			// projection, parce que c'est le champ de vitesse FINAL qui transporte
			// les scalaires. Le semi-lagrangien étant inconditionnellement stable,
			// ces deux chiffres ne gouvernent que le schéma en FLUX ; on les publie
			// toujours, parce qu'un CFL qu'on ne regarde pas est exactement ce qui
			// fait diverger un schéma explicite sans prévenir.
			mStats.advectCFL = MaxCFL(dt);
			mStats.advectSubsteps = 1;
			mStats.advectSubstepCapHit = false;
			if (mParams.advectFluxConservative) {
				const float32 cible = (mParams.advectCFLTarget > 0.f) ? mParams.advectCFLTarget : 0.9f;
				if (mStats.advectCFL > cible) {
					uint32 ni = (uint32)NkCeil(mStats.advectCFL / cible);
					if (ni < 1u)
						ni = 1u;
					const uint32 cap = (mParams.advectMaxSubsteps > 0) ? mParams.advectMaxSubsteps : 1u;
					if (ni > cap) {
						ni = cap;
						// La borne mord : le pas ne respecte PLUS le CFL. On le DIT
						// plutôt que de laisser le schéma diverger en silence.
						mStats.advectSubstepCapHit = true;
					}
					mStats.advectSubsteps = ni;
				}
			}

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
			// SECONDE passe de vorticite : la premiere servait le confinement et decrivait
			// le champ AVANT advection et projection. Ce que l'appelant lit dans les stats
			// doit decrire le champ QU'IL VOIT, pas un etat intermediaire — sinon
			// `Enstrophy()` repondrait a une autre question que celle qu'on croit poser.
			ComputeVorticity();
			mStats.divRatio = (mStats.velocityMean > 1.0e-9f) ? (mStats.divAfterMean / mStats.velocityMean) : 0.f;
			mStats.divRatioStrict =
				(mStats.velocityMeanStrict > 1.0e-9f) ? (mStats.divAfterMeanStrict / mStats.velocityMeanStrict) : 0.f;
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
