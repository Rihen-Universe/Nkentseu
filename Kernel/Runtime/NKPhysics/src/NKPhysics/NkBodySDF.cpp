// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// NkBodySDF.cpp — bande exacte, signe par pseudonormale pondérée, balayage rapide (en-tête).
#include "NKPhysics/NkBodySDF.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu {
	namespace physics {

		using nkentseu::math::NkAbs;
		using nkentseu::math::NkMax;
		using nkentseu::math::NkMin;
		using nkentseu::math::NkSqrt;

		namespace {

			// Point le plus proche d'un triangle (Ericson §5.1.5) + la région touchée :
			// 0 = face, 1 = arête ab, 2 = arête bc, 3 = arête ca, 4 = sommet a, 5 = b, 6 = c.
			NkVec3f ClosestOnTriangleLocal(const NkVec3f &p, const NkVec3f &a, const NkVec3f &b, const NkVec3f &c,
										   uint32 &region) {
				const NkVec3f ab = b - a, ac = c - a, ap = p - a;
				const float32 d1 = ab.Dot(ap), d2 = ac.Dot(ap);
				if (d1 <= 0.f && d2 <= 0.f) {
					region = 4;
					return a;
				}
				const NkVec3f bp = p - b;
				const float32 d3 = ab.Dot(bp), d4 = ac.Dot(bp);
				if (d3 >= 0.f && d4 <= d3) {
					region = 5;
					return b;
				}
				const float32 vc = d1 * d4 - d3 * d2;
				if (vc <= 0.f && d1 >= 0.f && d3 <= 0.f) {
					region = 1;
					return a + ab * (d1 / (d1 - d3));
				}
				const NkVec3f cp = p - c;
				const float32 d5 = ab.Dot(cp), d6 = ac.Dot(cp);
				if (d6 >= 0.f && d5 <= d6) {
					region = 6;
					return c;
				}
				const float32 vb = d5 * d2 - d1 * d6;
				if (vb <= 0.f && d2 >= 0.f && d6 <= 0.f) {
					region = 3;
					return a + ac * (d2 / (d2 - d6));
				}
				const float32 va = d3 * d6 - d5 * d4;
				if (va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f) {
					region = 2;
					return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
				}
				const float32 den = 1.f / (va + vb + vc);
				region = 0;
				return a + ab * (vb * den) + ac * (vc * den);
			}

			// Pseudonormale : la normale de face si le point le plus proche est SUR la face ;
			// sinon la normale pondérée par l'angle au sommet (Bærentzen & Aanæs 2005). Ici on
			// n'a pas la topologie voisine : on pondère la normale de face par l'angle du coin
			// concerné, ce qui suffit dès que plusieurs triangles se disputent la même cellule
			// (leurs contributions s'additionnent dans l'ordre de la bande, le plus proche gagne).
			float32 CornerWeightLocal(const NkVec3f &a, const NkVec3f &b, const NkVec3f &c, uint32 region) {
				auto angle = [](const NkVec3f &u, const NkVec3f &v) {
					const float32 lu = u.Len(), lv = v.Len();
					if (lu < 1e-12f || lv < 1e-12f)
						return 0.f;
					float32 t = u.Dot(v) / (lu * lv);
					t = t < -1.f ? -1.f : (t > 1.f ? 1.f : t);
					return math::NkAcos(t);
				};
				switch (region) {
					case 4: return angle(b - a, c - a);
					case 5: return angle(a - b, c - b);
					case 6: return angle(a - c, b - c);
					case 1: case 2: case 3: return 3.14159265f; // arête : deux faces s'y partagent 2 pi
					default: return 6.28318531f;				// face : elle décide seule
				}
			}

		} // namespace

		// ── Primitives partagées (déclarées dans l'en-tête) ──────────────────────────────────
		NkVec3f NkClosestOnTriangle(const NkVec3f &p, const NkVec3f &a, const NkVec3f &b, const NkVec3f &c,
									uint32 &outRegion) noexcept {
			return ClosestOnTriangleLocal(p, a, b, c, outRegion);
		}

		float32 NkTriangleCornerWeight(const NkVec3f &a, const NkVec3f &b, const NkVec3f &c, uint32 region) noexcept {
			return CornerWeightLocal(a, b, c, region);
		}

		// ── NkBodyProximity : la distance exacte, par particule (en-tête) ────────────────────
		bool NkBodyProximity::Build(const NkVec3f *verts, uint32 vertCount, const uint32 *indices, uint32 triCount,
									float32 cellSize) {
			mVerts = verts;
			mIdx = indices;
			mTriCount = 0;
			mCellStart.Clear();
			mCellTri.Clear();
			if (!verts || vertCount == 0 || !indices || triCount == 0)
				return false;
			mMin = verts[0];
			mMax = verts[0];
			for (uint32 v = 1; v < vertCount; ++v) {
				const NkVec3f &q = verts[v];
				mMin.x = NkMin(mMin.x, q.x); mMin.y = NkMin(mMin.y, q.y); mMin.z = NkMin(mMin.z, q.z);
				mMax.x = NkMax(mMax.x, q.x); mMax.y = NkMax(mMax.y, q.y); mMax.z = NkMax(mMax.z, q.z);
			}
			// cellule = la taille moyenne d'une arête de triangle (le bon compromis : chaque triangle
			// tombe dans une poignée de cellules, chaque cellule garde une poignée de triangles)
			if (cellSize <= 0.f) {
				float64 sum = 0.0;
				const uint32 step = triCount > 512u ? triCount / 512u : 1u;
				uint32 cnt = 0;
				for (uint32 t = 0; t < triCount; t += step) {
					const NkVec3f &a = verts[indices[t * 3]], &b = verts[indices[t * 3 + 1]];
					sum += (float64)(b - a).Len();
					++cnt;
				}
				// la cellule est accordée au RAYON des requêtes (l'épaisseur d'un tissu, pas la taille
				// d'un triangle) : une cellule trop grande fait visiter 5³ cellules par requête au lieu
				// de 3³ -- mesuré, c'était 12 M de tests de triangle par image et 600 ms par pas
				cellSize = cnt ? (float32)(sum / (float64)cnt) : 0.02f;
				if (cellSize < 0.015f)
					cellSize = 0.015f;
			}
			mCell = cellSize > 1e-5f ? cellSize : 0.05f;
			mInvCell = 1.f / mCell;
			const NkVec3f ext = mMax - mMin;
			mNX = (uint32)(ext.x * mInvCell) + 2u;
			mNY = (uint32)(ext.y * mInvCell) + 2u;
			mNZ = (uint32)(ext.z * mInvCell) + 2u;
			const uint32 ncell = mNX * mNY * mNZ;
			mCellStart.Resize(ncell + 1, 0u);
			for (uint32 c = 0; c <= ncell; ++c)
				mCellStart[c] = 0u;
			auto range = [&](uint32 t, uint32 &i0, uint32 &i1, uint32 &j0, uint32 &j1, uint32 &k0, uint32 &k1) {
				const NkVec3f &a = mVerts[mIdx[t * 3]], &b = mVerts[mIdx[t * 3 + 1]], &c = mVerts[mIdx[t * 3 + 2]];
				const NkVec3f tmn{NkMin(a.x, NkMin(b.x, c.x)), NkMin(a.y, NkMin(b.y, c.y)), NkMin(a.z, NkMin(b.z, c.z))};
				const NkVec3f tmx{NkMax(a.x, NkMax(b.x, c.x)), NkMax(a.y, NkMax(b.y, c.y)), NkMax(a.z, NkMax(b.z, c.z))};
				auto cl = [&](float32 v, float32 lo, uint32 n) {
					int32 k = (int32)((v - lo) * mInvCell);
					return (uint32)(k < 0 ? 0 : (k >= (int32)n ? (int32)n - 1 : k));
				};
				i0 = cl(tmn.x, mMin.x, mNX); i1 = cl(tmx.x, mMin.x, mNX);
				j0 = cl(tmn.y, mMin.y, mNY); j1 = cl(tmx.y, mMin.y, mNY);
				k0 = cl(tmn.z, mMin.z, mNZ); k1 = cl(tmx.z, mMin.z, mNZ);
			};
			for (uint32 t = 0; t < triCount; ++t) {
				uint32 i0, i1, j0, j1, k0, k1;
				range(t, i0, i1, j0, j1, k0, k1);
				for (uint32 k = k0; k <= k1; ++k)
					for (uint32 j = j0; j <= j1; ++j)
						for (uint32 i = i0; i <= i1; ++i)
							++mCellStart[(k * mNY + j) * mNX + i + 1u];
			}
			for (uint32 c = 0; c < ncell; ++c)
				mCellStart[c + 1] += mCellStart[c];
			mCellTri.Resize(mCellStart[ncell], 0u);
			NkVector<uint32> fill;
			fill.Resize(ncell, 0u);
			for (uint32 t = 0; t < triCount; ++t) {
				uint32 i0, i1, j0, j1, k0, k1;
				range(t, i0, i1, j0, j1, k0, k1);
				for (uint32 k = k0; k <= k1; ++k)
					for (uint32 j = j0; j <= j1; ++j)
						for (uint32 i = i0; i <= i1; ++i) {
							const uint32 c = (k * mNY + j) * mNX + i;
							mCellTri[mCellStart[c] + fill[c]++] = t;
						}
			}
			mTriCount = triCount;
			mQueries = 0;
			mTriTests = 0;
			return true;
		}

		bool NkBodyProximity::Query(const NkVec3f &p, float32 maxDist, float32 &outDist,
									NkVec3f &outNormal) const noexcept {
			if (mTriCount == 0)
				return false;
			// rejet par la boîte du corps : une particule loin ne peut toucher aucun triangle
			if (p.x < mMin.x - maxDist || p.x > mMax.x + maxDist || p.y < mMin.y - maxDist ||
				p.y > mMax.y + maxDist || p.z < mMin.z - maxDist || p.z > mMax.z + maxDist)
				return false;
			++mQueries;
			const int32 r = (int32)(maxDist * mInvCell) + 1;
			int32 ci = (int32)((p.x - mMin.x) * mInvCell), cj = (int32)((p.y - mMin.y) * mInvCell),
				  ck = (int32)((p.z - mMin.z) * mInvCell);
			float32 best2 = maxDist * maxDist;
			NkVec3f bestPoint{}, bestNormal{0.f, 1.f, 0.f};
			bool found = false;
			for (int32 k = ck - r; k <= ck + r; ++k) {
				if (k < 0 || k >= (int32)mNZ)
					continue;
				for (int32 j = cj - r; j <= cj + r; ++j) {
					if (j < 0 || j >= (int32)mNY)
						continue;
					for (int32 i = ci - r; i <= ci + r; ++i) {
						if (i < 0 || i >= (int32)mNX)
							continue;
						const uint32 c = ((uint32)k * mNY + (uint32)j) * mNX + (uint32)i;
						for (uint32 q = mCellStart[c]; q < mCellStart[c + 1]; ++q) {
							const uint32 t = mCellTri[q];
							++mTriTests;
							const NkVec3f &a = mVerts[mIdx[t * 3]], &b = mVerts[mIdx[t * 3 + 1]],
										  &cc = mVerts[mIdx[t * 3 + 2]];
							uint32 region = 0;
							const NkVec3f cp = NkClosestOnTriangle(p, a, b, cc, region);
							const NkVec3f d = p - cp;
							const float32 l2 = d.Dot(d);
							if (l2 < best2) {
								best2 = l2;
								bestPoint = cp;
								NkVec3f nrm = (b - a).Cross(cc - a);
								const float32 ln = nrm.Len();
								if (ln > 1e-14f)
									nrm = nrm * (NkTriangleCornerWeight(a, b, cc, region) / ln);
								bestNormal = nrm;
								found = true;
							}
						}
					}
				}
			}
			if (!found)
				return false;
			const NkVec3f d = p - bestPoint;
			const float32 l = d.Len();
			const float32 sgn = d.Dot(bestNormal) < 0.f ? -1.f : 1.f;
			outDist = sgn * l;
			outNormal = l > 1e-9f ? d * (sgn / l) : bestNormal.Normalized();
			return true;
		}

		float32 NkBodySDF::WindingNumber(const NkVec3f &p, const NkVec3f *verts, const uint32 *indices,
										 uint32 triCount) noexcept {
			// somme des angles solides signes (Van Oosterom & Strackee 1983, eq. 6) / 4 pi
			float64 sum = 0.0;
			for (uint32 t = 0; t < triCount; ++t) {
				const NkVec3f a = verts[indices[t * 3]] - p, b = verts[indices[t * 3 + 1]] - p,
							  c = verts[indices[t * 3 + 2]] - p;
				const float64 la = (float64)a.Len(), lb = (float64)b.Len(), lc = (float64)c.Len();
				if (la < 1e-12 || lb < 1e-12 || lc < 1e-12)
					return 1.f; // le point est SUR un sommet : dedans par convention
				const float64 num = (float64)a.Dot(b.Cross(c));
				const float64 den = la * lb * lc + (float64)a.Dot(b) * lc + (float64)a.Dot(c) * lb +
									(float64)b.Dot(c) * la;
				sum += 2.0 * (float64)math::NkAtan2((float32)num, (float32)den);
			}
			return (float32)(sum / (4.0 * 3.14159265358979));
		}

		float32 NkBodySDF::At(int32 i, int32 j, int32 k) const noexcept {
			i = i < 0 ? 0 : (i >= (int32)mNX ? (int32)mNX - 1 : i);
			j = j < 0 ? 0 : (j >= (int32)mNY ? (int32)mNY - 1 : j);
			k = k < 0 ? 0 : (k >= (int32)mNZ ? (int32)mNZ - 1 : k);
			return mD[Index((uint32)i, (uint32)j, (uint32)k)];
		}

		bool NkBodySDF::Build(const NkVec3f *verts, uint32 vertCount, const uint32 *indices, uint32 triCount) {
			mStats = NkBodySDFStats{};
			if (!verts || vertCount == 0 || !indices || triCount == 0)
				return false;
			NkVec3f mn, mx;
			if (params.useBounds) {
				mn = params.boundsMin;
				mx = params.boundsMax;
			} else {
				mn = verts[0];
				mx = verts[0];
				for (uint32 v = 1; v < vertCount; ++v) {
					const NkVec3f &q = verts[v];
					mn.x = NkMin(mn.x, q.x); mn.y = NkMin(mn.y, q.y); mn.z = NkMin(mn.z, q.z);
					mx.x = NkMax(mx.x, q.x); mx.y = NkMax(mx.y, q.y); mx.z = NkMax(mx.z, q.z);
				}
			}
			const NkVec3f m{params.margin, params.margin, params.margin};
			mMin = mn - m;
			mMax = mx + m;
			const NkVec3f ext = mMax - mMin;
			const float32 longest = NkMax(ext.x, NkMax(ext.y, ext.z));
			const uint32 res = params.resolution < 8 ? 8u : params.resolution;
			mCell = params.targetCellSize > 1e-6f ? params.targetCellSize : longest / (float32)res;
			if (mCell < 1e-6f)
				return false;
			// plafond de cellules : on agrandit la cellule jusqu'à tenir (le coût est borné, et le
			// chiffre rendu dit ce qu'on a vraiment obtenu -- jamais ce qu'on a demandé)
			for (uint32 guard = 0; guard < 32u; ++guard) {
				const float64 nc = ((float64)(ext.x / mCell) + 2.0) * ((float64)(ext.y / mCell) + 2.0) *
								   ((float64)(ext.z / mCell) + 2.0);
				if (nc <= (float64)params.maxCells)
					break;
				mCell *= 1.26f; // x2 en volume
			}
			mInvCell = 1.f / mCell;
			mNX = (uint32)(ext.x * mInvCell) + 2u;
			mNY = (uint32)(ext.y * mInvCell) + 2u;
			mNZ = (uint32)(ext.z * mInvCell) + 2u;
			const uint32 cells = mNX * mNY * mNZ;
			const float32 far = longest * 3.f;
			mD.Resize(cells, far);
			mKnown.Resize(cells, (uint8)0);
			float32 *D = mD.Data();
			uint8 *K = mKnown.Data();
			for (uint32 c = 0; c < cells; ++c) {
				D[c] = far;
				K[c] = 0;
			}
			// 1-2. bande exacte + signe par pseudonormale
			NkVector<float32> bestDist;
			bestDist.Resize(cells, far);
			for (uint32 c = 0; c < cells; ++c)
				bestDist[c] = far;
			const int32 band = (int32)(params.band < 1u ? 1u : params.band);
			uint32 skipped = 0;
			for (uint32 t = 0; t < triCount; ++t) {
				const NkVec3f &a = verts[indices[t * 3]], &b = verts[indices[t * 3 + 1]], &c = verts[indices[t * 3 + 2]];
				NkVec3f n = (b - a).Cross(c - a);
				const float32 ln = n.Len();
				if (ln < 1e-14f)
					continue;
				n = n * (1.f / ln);
				NkVec3f tmn{NkMin(a.x, NkMin(b.x, c.x)), NkMin(a.y, NkMin(b.y, c.y)), NkMin(a.z, NkMin(b.z, c.z))};
				NkVec3f tmx{NkMax(a.x, NkMax(b.x, c.x)), NkMax(a.y, NkMax(b.y, c.y)), NkMax(a.z, NkMax(b.z, c.z))};
				// hors de la grille (élargie de la bande) : ce triangle ne peut rien changer
				const float32 skirt = (float32)(params.band + 1u) * mCell;
				if (tmx.x < mMin.x - skirt || tmn.x > mMax.x + skirt || tmx.y < mMin.y - skirt ||
					tmn.y > mMax.y + skirt || tmx.z < mMin.z - skirt || tmn.z > mMax.z + skirt) {
					++skipped;
					continue;
				}
				const int32 i0 = (int32)((tmn.x - mMin.x) * mInvCell) - band, i1 = (int32)((tmx.x - mMin.x) * mInvCell) + band;
				const int32 j0 = (int32)((tmn.y - mMin.y) * mInvCell) - band, j1 = (int32)((tmx.y - mMin.y) * mInvCell) + band;
				const int32 k0 = (int32)((tmn.z - mMin.z) * mInvCell) - band, k1 = (int32)((tmx.z - mMin.z) * mInvCell) + band;
				for (int32 k = k0 < 0 ? 0 : k0; k <= k1 && k < (int32)mNZ; ++k)
					for (int32 j = j0 < 0 ? 0 : j0; j <= j1 && j < (int32)mNY; ++j)
						for (int32 i = i0 < 0 ? 0 : i0; i <= i1 && i < (int32)mNX; ++i) {
							const NkVec3f p = mMin + NkVec3f{(float32)i * mCell, (float32)j * mCell, (float32)k * mCell};
							uint32 region = 0;
							const NkVec3f q = NkClosestOnTriangle(p, a, b, c, region);
							const NkVec3f d = p - q;
							const float32 dist = d.Len();
							const uint32 idx = Index((uint32)i, (uint32)j, (uint32)k);
							if (dist < bestDist[idx]) {
								bestDist[idx] = dist;
								// le signe vient de la pseudonormale du coin touché (en-tête)
								const float32 w = NkTriangleCornerWeight(a, b, c, region);
								const float32 s = d.Dot(n * w);
								D[idx] = s < 0.f ? -dist : dist;
								K[idx] = 1;
								++mStats.bandCells;
							}
						}
			}
			// 2bis. SIGNE PAR NOMBRE D'ENROULEMENT (Jacobson 2013), sur une grille grossiere separee :
			// il remplace partout le signe de la pseudonormale. C'est ce qui rend utilisable un corps
			// NON ETANCHE (XBot, YBot : deux coques ouvertes imbriquees).
			if (params.sign == NkSDFSign::NK_WINDING) {
				const uint32 sres = params.signResolution < 4u ? 4u : params.signResolution;
				const float32 scell = longest / (float32)sres;
				const uint32 sx = (uint32)(ext.x / scell) + 2u, sy = (uint32)(ext.y / scell) + 2u,
							 sz = (uint32)(ext.z / scell) + 2u;
				// Le nombre d'enroulement est une valeur CONTINUE (0 dehors, 1 dedans, et il varie
				// doucement) : on l'échantillonne sur la grille grossière puis on l'INTERPOLE, au lieu
				// de plaquer un booléen de la cellule la plus proche. Mesuré le 05/09 : plaqué à une
				// grille de 12, il faisait tomber CesiumMan de 100 % à 60,6 % -- une cellule de signe
				// fait 15 cm, un bras 5 cm de rayon, le « le signe varie lentement » était faux à
				// l'échelle des membres.
				NkVector<float32> wf;
				wf.Resize(sx * sy * sz, 0.f);
				for (uint32 k = 0; k < sz; ++k)
					for (uint32 j = 0; j < sy; ++j)
						for (uint32 i = 0; i < sx; ++i) {
							const NkVec3f q = mMin + NkVec3f{(float32)i * scell, (float32)j * scell, (float32)k * scell};
							wf[(k * sy + j) * sx + i] = WindingNumber(q, verts, indices, triCount);
						}
				auto wAt = [&](int32 i, int32 j, int32 k) {
					i = i < 0 ? 0 : (i >= (int32)sx ? (int32)sx - 1 : i);
					j = j < 0 ? 0 : (j >= (int32)sy ? (int32)sy - 1 : j);
					k = k < 0 ? 0 : (k >= (int32)sz ? (int32)sz - 1 : k);
					return wf[((uint32)k * sy + (uint32)j) * sx + (uint32)i];
				};
				for (uint32 k = 0; k < mNZ; ++k)
					for (uint32 j = 0; j < mNY; ++j)
						for (uint32 i = 0; i < mNX; ++i) {
							const uint32 idx = Index(i, j, k);
							const float32 gx = ((float32)i * mCell) / scell, gy = ((float32)j * mCell) / scell,
										  gz = ((float32)k * mCell) / scell;
							const int32 bi = (int32)gx, bj = (int32)gy, bk = (int32)gz;
							const float32 fx = gx - (float32)bi, fy = gy - (float32)bj, fz = gz - (float32)bk;
							const float32 c00 = wAt(bi, bj, bk) + (wAt(bi + 1, bj, bk) - wAt(bi, bj, bk)) * fx;
							const float32 c10 = wAt(bi, bj + 1, bk) + (wAt(bi + 1, bj + 1, bk) - wAt(bi, bj + 1, bk)) * fx;
							const float32 c01 = wAt(bi, bj, bk + 1) + (wAt(bi + 1, bj, bk + 1) - wAt(bi, bj, bk + 1)) * fx;
							const float32 c11 =
								wAt(bi, bj + 1, bk + 1) + (wAt(bi + 1, bj + 1, bk + 1) - wAt(bi, bj + 1, bk + 1)) * fx;
							const float32 c0 = c00 + (c10 - c00) * fy, c1 = c01 + (c11 - c01) * fy;
							const float32 w = c0 + (c1 - c0) * fz;
							const float32 av = NkAbs(D[idx]);
							D[idx] = (w > params.windingThreshold) ? -av : av;
						}
			}
			// 3. balayage rapide : |d| propagée, le signe transporté de la cellule voisine
			if (params.sweep) {
				auto relax = [&](int32 i, int32 j, int32 k) {
					const uint32 idx = Index((uint32)i, (uint32)j, (uint32)k);
					float32 best = mD[idx];
					float32 bestAbs = NkAbs(best);
					const int32 off[6][3] = {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
					for (uint32 o = 0; o < 6; ++o) {
						const int32 ni = i + off[o][0], nj = j + off[o][1], nk = k + off[o][2];
						if (ni < 0 || nj < 0 || nk < 0 || ni >= (int32)mNX || nj >= (int32)mNY || nk >= (int32)mNZ)
							continue;
						const float32 nv = mD[Index((uint32)ni, (uint32)nj, (uint32)nk)];
						const float32 cand = NkAbs(nv) + mCell;
						if (cand < bestAbs) {
							bestAbs = cand;
							best = nv < 0.f ? -cand : cand;
						}
					}
					mD[idx] = best;
				};
				for (uint32 pass = 0; pass < 8; ++pass) {
					const bool rx = (pass & 1u) != 0, ry = (pass & 2u) != 0, rz = (pass & 4u) != 0;
					for (uint32 kk = 0; kk < mNZ; ++kk) {
						const int32 k = rz ? (int32)(mNZ - 1 - kk) : (int32)kk;
						for (uint32 jj = 0; jj < mNY; ++jj) {
							const int32 j = ry ? (int32)(mNY - 1 - jj) : (int32)jj;
							for (uint32 ii = 0; ii < mNX; ++ii) {
								const int32 i = rx ? (int32)(mNX - 1 - ii) : (int32)ii;
								relax(i, j, k);
							}
						}
					}
				}
			}
			mStats.nx = mNX;
			mStats.ny = mNY;
			mStats.nz = mNZ;
			mStats.cells = cells;
			mStats.cellSize = mCell;
			mStats.skippedTriangles = skipped;
			mStats.minValue = mD[0];
			mStats.maxValue = mD[0];
			for (uint32 c = 0; c < cells; ++c) {
				if (mD[c] < mStats.minValue) mStats.minValue = mD[c];
				if (mD[c] > mStats.maxValue) mStats.maxValue = mD[c];
				if (mD[c] < 0.f) ++mStats.negativeCells;
			}
			return true;
		}

		float32 NkBodySDF::Sample(const NkVec3f &p) const noexcept {
			if (mStats.cells == 0)
				return 1e30f;
			// hors grille : distance au bord de la boîte, positive (dehors), l'appelant garde ses capsules
			NkVec3f q = p;
			float32 outside = 0.f;
			for (uint32 a = 0; a < 3; ++a) {
				const float32 lo = a == 0 ? mMin.x : (a == 1 ? mMin.y : mMin.z);
				const float32 hi = a == 0 ? mMax.x : (a == 1 ? mMax.y : mMax.z);
				float32 &v = a == 0 ? q.x : (a == 1 ? q.y : q.z);
				if (v < lo) {
					outside += (lo - v) * (lo - v);
					v = lo;
				} else if (v > hi) {
					outside += (v - hi) * (v - hi);
					v = hi;
				}
			}
			const NkVec3f l = (q - mMin) * mInvCell;
			int32 i = (int32)l.x, j = (int32)l.y, k = (int32)l.z;
			i = i < 0 ? 0 : (i >= (int32)mNX - 1 ? (int32)mNX - 2 : i);
			j = j < 0 ? 0 : (j >= (int32)mNY - 1 ? (int32)mNY - 2 : j);
			k = k < 0 ? 0 : (k >= (int32)mNZ - 1 ? (int32)mNZ - 2 : k);
			const float32 fx = l.x - (float32)i, fy = l.y - (float32)j, fz = l.z - (float32)k;
			const float32 c000 = At(i, j, k), c100 = At(i + 1, j, k), c010 = At(i, j + 1, k), c110 = At(i + 1, j + 1, k);
			const float32 c001 = At(i, j, k + 1), c101 = At(i + 1, j, k + 1), c011 = At(i, j + 1, k + 1),
						  c111 = At(i + 1, j + 1, k + 1);
			const float32 c00 = c000 + (c100 - c000) * fx, c10 = c010 + (c110 - c010) * fx;
			const float32 c01 = c001 + (c101 - c001) * fx, c11 = c011 + (c111 - c011) * fx;
			const float32 c0 = c00 + (c10 - c00) * fy, c1 = c01 + (c11 - c01) * fy;
			const float32 v = c0 + (c1 - c0) * fz;
			return outside > 0.f ? NkSqrt(outside) + (v > 0.f ? v : 0.f) : v;
		}

		NkVec3f NkBodySDF::Gradient(const NkVec3f &p) const noexcept {
			const float32 h = mCell > 0.f ? mCell : 0.01f;
			NkVec3f g{Sample(p + NkVec3f{h, 0, 0}) - Sample(p - NkVec3f{h, 0, 0}),
					  Sample(p + NkVec3f{0, h, 0}) - Sample(p - NkVec3f{0, h, 0}),
					  Sample(p + NkVec3f{0, 0, h}) - Sample(p - NkVec3f{0, 0, h})};
			const float32 l = g.Len();
			return l > 1e-9f ? g * (1.f / l) : NkVec3f{0.f, 1.f, 0.f};
		}

		bool NkBodySDF::Project(NkVec3f &p, float32 offset, NkVec3f *outNormal) const noexcept {
			const float32 d = Sample(p);
			if (d >= offset)
				return false;
			const NkVec3f n = Gradient(p);
			p += n * (offset - d);
			if (outNormal)
				*outNormal = n;
			return true;
		}

		float32 NkBodySDF::Calibrate(const NkVec3f *verts, const uint32 *indices, uint32 triCount, float32 depth,
									 uint32 *outPositiveA, uint32 *outPositiveTotal, uint32 *outNeg, uint32 *outNegTotal,
									 bool *outCenterInside) const {
			uint32 okA = 0, totA = 0, neg = 0, totNeg = 0;
			for (uint32 t = 0; t < triCount; t += 37) {
				const NkVec3f &a = verts[indices[t * 3]], &b = verts[indices[t * 3 + 1]], &c = verts[indices[t * 3 + 2]];
				NkVec3f n = (b - a).Cross(c - a);
				const float32 ln = n.Len();
				if (ln < 1e-12f)
					continue;
				n = n * (1.f / ln);
				const NkVec3f p = (a + b + c) * (1.f / 3.f) - n * depth;
				++totA;
				if (Sample(p) < 0.f)
					++okA;
			}
			const NkVec3f center = (mMin + mMax) * 0.5f;
			const float32 size = (mMax - mMin).Len();
			for (uint32 k = 0; k < 256; ++k) {
				const float32 ang = 6.2831853f * (float32)k / 256.f, el = 3.14159265f * (float32)(k % 13) / 13.f;
				const NkVec3f p = center + NkVec3f{3.f * size * math::NkSin(el) * math::NkCos(ang), 3.f * size * math::NkCos(el),
												   3.f * size * math::NkSin(el) * math::NkSin(ang)};
				++totNeg;
				if (Sample(p) < 0.f)
					++neg;
			}
			if (outPositiveA) *outPositiveA = okA;
			if (outPositiveTotal) *outPositiveTotal = totA;
			if (outNeg) *outNeg = neg;
			if (outNegTotal) *outNegTotal = totNeg;
			if (outCenterInside) *outCenterInside = Sample(center) < 0.f;
			return totA ? (float32)okA / (float32)totA : 0.f;
		}

		void NkBodySDF::MutateFlipSign() {
			float32 *D = mD.Data();
			for (uint32 c = 0; c < mStats.cells; ++c)
				D[c] = -D[c];
		}

	} // namespace physics
} // namespace nkentseu
