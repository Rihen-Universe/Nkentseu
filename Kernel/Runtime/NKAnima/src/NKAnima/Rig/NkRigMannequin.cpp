// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkRigMannequin.cpp — les mannequins de test (voir NkRigMannequin.h) : une
// fonction de distance signee (capsules effilees et ellipsoides fondus), puis
// la surface par « surface nets » (un sommet par cellule traversee, un quad par
// arete traversee).
// =============================================================================
#include "NKAnima/Rig/NkRigMannequin.h"
#include "NKAnima/Rig/NkRigMath.h"

#include <cmath>

namespace nkentseu {
	namespace anim {

		using math::NkVec3f;
		using namespace rigm;

		namespace {
			float32 Smin(float32 a, float32 b, float32 k) {
				float32 h = 0.5f + 0.5f * (b - a) / k;
				h = h < 0.f ? 0.f : (h > 1.f ? 1.f : h);
				return b + (a - b) * h - k * h * (1.f - h);
			}
			/// Capsule EFFILEE : rayon r0 en a, r1 en b (distance approchee, suffisante ici).
			float32 Capsule(const NkVec3f &p, const NkVec3f &a, const NkVec3f &b, float32 r0, float32 r1) {
				float32 t = 0.f;
				const float32 d = DistanceSegment(p, a, b, &t);
				return d - (r0 + (r1 - r0) * t);
			}
			/// Ellipsoide (approximation d'Inigo Quilez, exacte sur la surface).
			float32 Ellipsoide(const NkVec3f &p, const NkVec3f &c, const NkVec3f &r) {
				const NkVec3f q = Sub(p, c);
				const float32 k0 = Len(V(q.x / r.x, q.y / r.y, q.z / r.z));
				const float32 k1 = Len(V(q.x / (r.x * r.x), q.y / (r.y * r.y), q.z / (r.z * r.z)));
				return k1 > 1e-12f ? k0 * (k0 - 1.f) / k1 : -r.x;
			}

			struct Forme {
					NkMannequinKind kind;
					float32 angleBras = 0.f; ///< radians sous l'horizontale

					/// Tourne un point du bras autour de l'epaule (pose en A).
					NkVec3f Bras(const NkVec3f &p, float32 s) const {
						if (angleBras == 0.f) {
							return p;
						}
						const NkVec3f pivot = V(s * 0.17f, 1.41f, 0.f);
						const float32 dx = p.x - pivot.x, dy = p.y - pivot.y;
						const float32 c = std::cos(angleBras), sn = std::sin(angleBras) * s;
						return V(pivot.x + dx * c + dy * sn, pivot.y - dx * sn + dy * c, p.z);
					}

					float32 Distance(const NkVec3f &p) const {
						if (kind == NkMannequinKind::NK_MannequinKind_Quadrupede) {
							float32 d = Ellipsoide(p, V(0.f, 0.5f, -0.02f), V(0.15f, 0.14f, 0.36f));
							d = Smin(d, Ellipsoide(p, V(0.f, 0.5f, 0.18f), V(0.16f, 0.16f, 0.18f)), 0.05f);
							d = Smin(d, Capsule(p, V(0.f, 0.56f, 0.28f), V(0.f, 0.7f, 0.42f), 0.09f, 0.07f), 0.04f);
							d = Smin(d, Ellipsoide(p, V(0.f, 0.74f, 0.48f), V(0.085f, 0.085f, 0.11f)), 0.04f);
							d = Smin(d, Capsule(p, V(0.f, 0.72f, 0.55f), V(0.f, 0.7f, 0.66f), 0.05f, 0.035f), 0.03f);
							d = Smin(d, Capsule(p, V(0.f, 0.56f, -0.36f), V(0.f, 0.66f, -0.62f), 0.035f, 0.015f), 0.03f);
							for (int32 k = 0; k < 2; ++k) {
								const float32 s = k == 0 ? 1.f : -1.f, x = s * 0.085f;
								d = Smin(d, Capsule(p, V(x, 0.44f, 0.26f), V(x, 0.24f, 0.24f), 0.05f, 0.04f), 0.03f);
								d = Smin(d, Capsule(p, V(x, 0.24f, 0.24f), V(x, 0.05f, 0.27f), 0.035f, 0.028f), 0.015f);
								d = Smin(d, Ellipsoide(p, V(x, 0.028f, 0.29f), V(0.035f, 0.026f, 0.05f)), 0.015f);
								d = Smin(d, Capsule(p, V(x, 0.47f, -0.27f), V(x, 0.3f, -0.2f), 0.07f, 0.045f), 0.03f);
								d = Smin(d, Capsule(p, V(x, 0.3f, -0.2f), V(x, 0.15f, -0.3f), 0.04f, 0.03f), 0.015f);
								d = Smin(d, Capsule(p, V(x, 0.15f, -0.3f), V(x, 0.05f, -0.29f), 0.028f, 0.026f), 0.012f);
								d = Smin(d, Ellipsoide(p, V(x, 0.028f, -0.27f), V(0.035f, 0.026f, 0.05f)), 0.015f);
							}
							return d;
						}
						// L'HUMANOIDE (1,75) : tete en oeuf, cou, poitrine, epaules, ventre, bassin.
						float32 d = Ellipsoide(p, V(0.f, 1.635f, 0.005f), V(0.088f, 0.112f, 0.1f));
						d = Smin(d, Capsule(p, V(0.f, 1.46f, 0.f), V(0.f, 1.57f, 0.f), 0.05f, 0.046f), 0.025f);
						d = Smin(d, Ellipsoide(p, V(0.f, 1.31f, 0.f), V(0.165f, 0.16f, 0.105f)), 0.04f);
						d = Smin(d, Capsule(p, V(-0.15f, 1.405f, 0.f), V(0.15f, 1.405f, 0.f), 0.062f, 0.062f), 0.04f);
						d = Smin(d, Ellipsoide(p, V(0.f, 1.1f, 0.f), V(0.14f, 0.16f, 0.1f)), 0.05f);
						d = Smin(d, Ellipsoide(p, V(0.f, 0.93f, -0.005f), V(0.155f, 0.11f, 0.105f)), 0.05f);
						for (int32 k = 0; k < 2; ++k) {
							const float32 s = k == 0 ? 1.f : -1.f;
							// Bras, avant-bras, main (plate), pouce.
							d = Smin(d, Capsule(p, Bras(V(s * 0.17f, 1.41f, 0.f), s), Bras(V(s * 0.46f, 1.41f, 0.f), s), 0.05f, 0.042f), 0.025f);
							d = Smin(d, Capsule(p, Bras(V(s * 0.46f, 1.41f, 0.f), s), Bras(V(s * 0.71f, 1.41f, 0.f), s), 0.042f, 0.033f), 0.015f);
							d = Smin(d, Ellipsoide(p, Bras(V(s * 0.8f, 1.405f, 0.f), s), V(0.085f, 0.026f, 0.048f)), 0.02f);
							// Jambe : cuisse, tibia, pied (vers +Z).
							d = Smin(d, Capsule(p, V(s * 0.095f, 0.92f, 0.f), V(s * 0.095f, 0.49f, 0.01f), 0.075f, 0.052f), 0.03f);
							d = Smin(d, Capsule(p, V(s * 0.095f, 0.49f, 0.01f), V(s * 0.095f, 0.085f, -0.005f), 0.05f, 0.035f), 0.02f);
							d = Smin(d, Capsule(p, V(s * 0.095f, 0.045f, -0.03f), V(s * 0.095f, 0.035f, 0.15f), 0.04f, 0.03f), 0.02f);
						}
						return d;
					}
			};
		} // namespace

		bool NkMakeMannequin(NkMannequinKind kind, NkSkinMesh &out, uint32 resolution) {
			out = NkSkinMesh();
			Forme f;
			f.kind = kind;
			f.angleBras = kind == NkMannequinKind::NK_MannequinKind_Humanoide_A ? 0.698f : 0.f;
			NkVec3f bmin, bmax;
			float32 hauteur;
			if (kind == NkMannequinKind::NK_MannequinKind_Quadrupede) {
				bmin = V(-0.22f, -0.02f, -0.68f);
				bmax = V(0.22f, 0.86f, 0.72f);
				hauteur = 0.86f;
			} else {
				bmin = V(-0.92f, -0.02f, -0.14f);
				bmax = V(0.92f, 1.77f, 0.2f);
				hauteur = 1.77f;
			}
			const uint32 res = resolution < 32 ? 32 : (resolution > 220 ? 220 : resolution);
			const float32 h = hauteur / (float32)res;
			// La grille est SYMETRIQUE en X (noeuds en x = (i - (nx-1)/2) h) : le maillage
			// l'est alors au flottant pres, et chaque sommet trouve son miroir exact.
			const int32 demiX = (int32)std::ceil(bmax.x / h) + 2;
			const int32 nx = 2 * demiX + 1;
			const NkVec3f o = V(-(float32)demiX * h, bmin.y - 2.f * h, bmin.z - 2.f * h);
			const int32 ny = (int32)std::ceil((bmax.y - bmin.y) / h) + 5;
			const int32 nz = (int32)std::ceil((bmax.z - bmin.z) / h) + 5;
			auto N = [&](int32 i, int32 j, int32 k) { return (uint32)((k * ny + j) * nx + i); };
			auto P = [&](int32 i, int32 j, int32 k) { return V(o.x + (float32)i * h, o.y + (float32)j * h, o.z + (float32)k * h); };
			NkVector<float32> val;
			val.Resize((usize)nx * (usize)ny * (usize)nz, 1.f);
			for (int32 k = 0; k < nz; ++k) {
				for (int32 j = 0; j < ny; ++j) {
					for (int32 i = 0; i < nx; ++i) {
						val[N(i, j, k)] = f.Distance(P(i, j, k));
					}
				}
			}
			// UN SOMMET PAR CELLULE TRAVERSEE : la moyenne des points de passage sur ses aretes.
			NkVector<int32> sommetDe;
			sommetDe.Resize((usize)nx * (usize)ny * (usize)nz, -1);
			static const int32 kCoins[8][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}, {0, 0, 1}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1}};
			static const int32 kAretes[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
			for (int32 k = 0; k + 1 < nz; ++k) {
				for (int32 j = 0; j + 1 < ny; ++j) {
					for (int32 i = 0; i + 1 < nx; ++i) {
						float32 v[8];
						uint32 signes = 0;
						for (int32 c = 0; c < 8; ++c) {
							v[c] = val[N(i + kCoins[c][0], j + kCoins[c][1], k + kCoins[c][2])];
							signes |= (v[c] < 0.f ? 1u : 0u) << c;
						}
						if (signes == 0 || signes == 0xFFu) {
							continue;
						}
						NkVec3f s = V(0.f, 0.f, 0.f);
						uint32 n = 0;
						for (int32 e = 0; e < 12; ++e) {
							const int32 a = kAretes[e][0], b = kAretes[e][1];
							if ((v[a] < 0.f) == (v[b] < 0.f)) {
								continue;
							}
							const float32 t = v[a] / (v[a] - v[b]);
							const NkVec3f pa = P(i + kCoins[a][0], j + kCoins[a][1], k + kCoins[a][2]);
							const NkVec3f pb = P(i + kCoins[b][0], j + kCoins[b][1], k + kCoins[b][2]);
							s = Add(s, Lerp(pa, pb, t));
							++n;
						}
						sommetDe[N(i, j, k)] = (int32)out.positions.Size();
						out.positions.PushBack(Mul(s, 1.f / (float32)n));
					}
				}
			}
			// UN QUAD PAR ARETE DE LA GRILLE TRAVERSEE (les quatre cellules autour).
			auto Quad = [&](int32 c0, int32 c1, int32 c2, int32 c3) {
				if (c0 < 0 || c1 < 0 || c2 < 0 || c3 < 0) {
					return;
				}
				// LA DIAGONALE se choisit par la GEOMETRIE (la plus courte ; a egalite,
				// celle qui s'ecarte le moins du plan x = 0) : un quad et son miroir se
				// coupent alors de la meme facon, et le lissage reste symetrique. Couper
				// toujours par c0-c2 rendait le maillage dissymetrique de 9 mm.
				const NkVec3f &p0 = out.positions[(uint32)c0], &p1 = out.positions[(uint32)c1];
				const NkVec3f &p2 = out.positions[(uint32)c2], &p3 = out.positions[(uint32)c3];
				const float32 d02 = Dist(p0, p2), d13 = Dist(p1, p3);
				bool par02 = d02 < d13;
				if (std::fabs(d02 - d13) <= 1e-6f * (d02 + d13)) {
					par02 = std::fabs(p0.x) + std::fabs(p2.x) <= std::fabs(p1.x) + std::fabs(p3.x);
				}
				const int32 t[6] = {c0, c1, c2, c0, c2, c3};
				const int32 u[6] = {c0, c1, c3, c1, c2, c3};
				for (int32 q = 0; q < 6; ++q) {
					out.indices.PushBack((uint32)(par02 ? t[q] : u[q]));
				}
			};
			auto C = [&](int32 i, int32 j, int32 k) -> int32 {
				if (i < 0 || j < 0 || k < 0 || i + 1 >= nx || j + 1 >= ny || k + 1 >= nz) {
					return -1;
				}
				return sommetDe[N(i, j, k)];
			};
			for (int32 k = 0; k < nz; ++k) {
				for (int32 j = 0; j < ny; ++j) {
					for (int32 i = 0; i < nx; ++i) {
						const bool dedans = val[N(i, j, k)] < 0.f;
						if (i + 1 < nx && dedans != (val[N(i + 1, j, k)] < 0.f)) {
							Quad(C(i, j - 1, k - 1), C(i, j, k - 1), C(i, j, k), C(i, j - 1, k));
						}
						if (j + 1 < ny && dedans != (val[N(i, j + 1, k)] < 0.f)) {
							Quad(C(i - 1, j, k - 1), C(i, j, k - 1), C(i, j, k), C(i - 1, j, k));
						}
						if (k + 1 < nz && dedans != (val[N(i, j, k + 1)] < 0.f)) {
							Quad(C(i - 1, j - 1, k), C(i, j - 1, k), C(i, j, k), C(i - 1, j, k));
						}
					}
				}
			}
			if (out.indices.Empty()) {
				return false;
			}
			// Deux lissages legers (les sommets de surface nets font des marches).
			out.BuildTopology();
			for (int32 passe = 0; passe < 2; ++passe) {
				NkVector<NkVec3f> lisse = out.positions;
				for (uint32 r = 0; r < (uint32)out.soudes.Size(); ++r) {
					const uint32 a = out.adjDebut[r], z = out.adjDebut[r + 1];
					if (z <= a) {
						continue;
					}
					NkVec3f s = V(0.f, 0.f, 0.f);
					for (uint32 q = a; q < z; ++q) {
						s = Add(s, out.positions[out.soudes[out.adj[q]]]);
					}
					const uint32 v = out.soudes[r];
					lisse[v] = Lerp(out.positions[v], Mul(s, 1.f / (float32)(z - a)), 0.5f);
				}
				out.positions = lisse;
			}
			// L'ORIENTATION : chaque triangle regarde dehors (le gradient de la distance).
			const uint32 nt = out.TriangleCount();
			for (uint32 t = 0; t < nt; ++t) {
				uint32 *ix = out.indices.Data() + t * 3;
				const NkVec3f a = out.positions[ix[0]], b = out.positions[ix[1]], c = out.positions[ix[2]];
				const NkVec3f n = Cross(Sub(b, a), Sub(c, a));
				const NkVec3f g = Mul(Add(Add(a, b), c), 1.f / 3.f);
				const float32 e = h * 0.5f;
				const NkVec3f grad = V(f.Distance(Add(g, V(e, 0, 0))) - f.Distance(Sub(g, V(e, 0, 0))),
									   f.Distance(Add(g, V(0, e, 0))) - f.Distance(Sub(g, V(0, e, 0))),
									   f.Distance(Add(g, V(0, 0, e))) - f.Distance(Sub(g, V(0, 0, e))));
				if (Dot(n, grad) < 0.f) {
					const uint32 x = ix[1];
					ix[1] = ix[2];
					ix[2] = x;
				}
			}
			out.BuildTopology();
			out.ComputeNormals();
			return true;
		}

	} // namespace anim
} // namespace nkentseu
