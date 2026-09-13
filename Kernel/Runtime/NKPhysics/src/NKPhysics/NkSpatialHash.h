#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkSpatialHash.h — voisinage de particules en O(N) par HACHAGE SPATIAL
// (2026-09-05, pour l'auto-collision du tissu ; les cheveux en hériteront).
//
// Référence : Teschner, Heidelberger, Müller, Pomeranets, Gross, « Optimized
// Spatial Hashing for Collision Detection of Deformable Objects », VMV 2003 —
// cellule = floor(p / cellSize), clé = (x·73856093 xor y·19349663 xor
// z·83492791) mod H (les trois grands premiers de l'article, §3.2), H ~ 2N.
//
// Pourquoi pas la grille du SPH (NkSPHSolver::BuildNeighbors) : c'est le même
// tri par comptage, mais elle est une méthode PRIVÉE de NKRenderer soudée à
// `NkSPHParams` (boîte fixe boundsMin/Max, cellule = h) et à ses tampons de
// noyau (gradW, W) ; NKPhysics ne voit pas NKRenderer, et une nappe n'a pas de
// boîte fixe : son AABB change à chaque pas et peut couvrir 1 m x 1 m x 1 m
// avec des cellules de 3 mm (256² : 320³ = 32 M de cellules pour une grille
// dense — le hachage en garde 2N). Le tri par comptage est repris tel quel
// (même schéma que le SPH : compte par cellule, préfixe, dispersion) ; seule
// l'adresse de cellule est hachée. Le jour où le SPH veut une boîte mobile,
// c'est cette structure qu'il prend, pas l'inverse.
//
// Zéro STL, zéro allocation en régime établi (tampons réutilisés).
// =============================================================================
#include "NKMath/NkVec.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace physics {

		class NkSpatialHash {
			public:
				// Construit la table pour `n` points de `X`, cellule `cellSize` (m).
				// Table de taille H = première puissance de deux >= 2n (>= 64).
				void Build(const NkVec3f *X, uint32 n, float32 cellSize) {
					mCell = (cellSize > 1e-6f) ? cellSize : 1e-6f;
					mInv = 1.f / mCell;
					uint32 H = 64;
					while (H < 2u * n)
						H <<= 1;
					mMask = H - 1;
					mCount.Resize(H);
					mStart.Resize(H + 1);
					mSorted.Resize(n);
					mKey.Resize(n);
					uint32 *C = mCount.Data(), *S = mStart.Data(), *O = mSorted.Data(), *K = mKey.Data();
					for (uint32 c = 0; c < H; ++c)
						C[c] = 0;
					for (uint32 k = 0; k < n; ++k) {
						int32 cx, cy, cz;
						CellOf(X[k], cx, cy, cz);
						const uint32 h = Hash(cx, cy, cz);
						K[k] = h;
						++C[h];
					}
					S[0] = 0;
					for (uint32 c = 0; c < H; ++c)
						S[c + 1] = S[c] + C[c];
					for (uint32 c = 0; c < H; ++c)
						C[c] = 0;
					for (uint32 k = 0; k < n; ++k) {
						const uint32 h = K[k];
						O[S[h] + C[h]] = k;
						++C[h];
					}
					mN = n;
				}

				// Visite les candidats dans les 27 cellules autour de `p` : appelle
				// `fn(j)` pour chaque indice j rencontré (le filtrage par distance est
				// à la charge de l'appelant ; un hachage peut mêler deux cellules).
				template <typename Fn> void Query(const NkVec3f &p, Fn &&fn) const {
					int32 cx, cy, cz;
					CellOf(p, cx, cy, cz);
					const uint32 *S = mStart.Data(), *O = mSorted.Data();
					for (int32 dz = -1; dz <= 1; ++dz)
						for (int32 dy = -1; dy <= 1; ++dy)
							for (int32 dx = -1; dx <= 1; ++dx) {
								const uint32 h = Hash(cx + dx, cy + dy, cz + dz);
								for (uint32 q = S[h]; q < S[h + 1]; ++q)
									fn(O[q]);
							}
				}

				uint32 Count() const noexcept {
					return mN;
				}

				float32 CellSize() const noexcept {
					return mCell;
				}

			private:
				void CellOf(const NkVec3f &p, int32 &cx, int32 &cy, int32 &cz) const noexcept {
					// floor, pas troncature : les coordonnées négatives comptent
					const float32 fx = p.x * mInv, fy = p.y * mInv, fz = p.z * mInv;
					cx = (int32)fx - (fx < 0.f && (float32)(int32)fx != fx ? 1 : 0);
					cy = (int32)fy - (fy < 0.f && (float32)(int32)fy != fy ? 1 : 0);
					cz = (int32)fz - (fz < 0.f && (float32)(int32)fz != fz ? 1 : 0);
				}

				uint32 Hash(int32 x, int32 y, int32 z) const noexcept {
					const uint32 h = ((uint32)x * 73856093u) ^ ((uint32)y * 19349663u) ^ ((uint32)z * 83492791u);
					return h & mMask;
				}

				NkVector<uint32> mCount, mStart, mSorted, mKey;
				float32 mCell = 0.01f, mInv = 100.f;
				uint32 mMask = 63, mN = 0;
		};

	} // namespace physics
} // namespace nkentseu
