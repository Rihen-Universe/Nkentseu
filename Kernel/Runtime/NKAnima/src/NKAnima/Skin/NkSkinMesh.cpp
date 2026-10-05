// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkSkinMesh.cpp — le maillage de la peau (voir NkSkinMesh.h).
// =============================================================================
#include "NKAnima/Skin/NkSkinMesh.h"
#include "NKAnima/Rig/NkRigMath.h"
#include "NKContainers/Utilities/NkSort.h"

#include <cmath>

namespace nkentseu {
	namespace anim {

		using math::NkVec3f;
		using namespace rigm;

		namespace {
			/// Une position quantifiee (cellule de la grille de soudure / de miroir).
			struct Cle {
					int32 x, y, z;
					uint32 v;
			};
			bool Avant(const Cle &a, const Cle &b) {
				if (a.x != b.x) {
					return a.x < b.x;
				}
				if (a.y != b.y) {
					return a.y < b.y;
				}
				if (a.z != b.z) {
					return a.z < b.z;
				}
				return a.v < b.v;
			}
			int32 Q(float32 v, float32 pas) {
				return (int32)std::floor(v / pas);
			}

			/// Premier element >= (x, y, z) dans un tableau trie de cles.
			uint32 Borne(const NkVector<Cle> &t, int32 x, int32 y, int32 z) {
				uint32 lo = 0, hi = (uint32)t.Size();
				while (lo < hi) {
					const uint32 mi = (lo + hi) / 2;
					const Cle &c = t[mi];
					const bool av = c.x != x ? c.x < x : (c.y != y ? c.y < y : c.z < z);
					if (av) {
						lo = mi + 1;
					} else {
						hi = mi;
					}
				}
				return lo;
			}

			/// Intersection rayon / triangle (Moller-Trumbore), t > 0 seulement.
			bool Rayon(const NkVec3f &o, const NkVec3f &d, const NkVec3f &a, const NkVec3f &b, const NkVec3f &c, float32 *tOut = nullptr) {
				const NkVec3f e1 = Sub(b, a), e2 = Sub(c, a);
				const NkVec3f p = Cross(d, e2);
				const float32 det = Dot(e1, p);
				if (std::fabs(det) < 1e-14f) {
					return false;
				}
				const float32 inv = 1.f / det;
				const NkVec3f s = Sub(o, a);
				const float32 u = Dot(s, p) * inv;
				if (u < 0.f || u > 1.f) {
					return false;
				}
				const NkVec3f q = Cross(s, e1);
				const float32 v = Dot(d, q) * inv;
				if (v < 0.f || u + v > 1.f) {
					return false;
				}
				const float32 t = Dot(e2, q) * inv;
				if (tOut != nullptr) {
					*tOut = t;
				}
				return t > 1e-7f;
			}
		} // namespace

		float32 NkSkinMesh::Diagonal() const {
			return Dist(bmin, bmax);
		}

		void NkSkinMesh::BuildTopology() {
			const uint32 n = VertexCount();
			rep.Clear();
			soudes.Clear();
			adjDebut.Clear();
			adj.Clear();
			if (n == 0) {
				return;
			}
			bmin = bmax = positions[0];
			for (uint32 i = 1; i < n; ++i) {
				const NkVec3f &p = positions[i];
				bmin = V(p.x < bmin.x ? p.x : bmin.x, p.y < bmin.y ? p.y : bmin.y, p.z < bmin.z ? p.z : bmin.z);
				bmax = V(p.x > bmax.x ? p.x : bmax.x, p.y > bmax.y ? p.y : bmax.y, p.z > bmax.z ? p.z : bmax.z);
			}
			// SOUDURE : positions egales a 1e-6 de la diagonale (les doubles de couture
			// sont EGAUX, bit pour bit le plus souvent).
			const float32 pas = (Diagonal() > 0.f ? Diagonal() : 1.f) * 1e-6f;
			NkVector<Cle> cles;
			cles.Resize(n);
			for (uint32 i = 0; i < n; ++i) {
				cles[i] = Cle{Q(positions[i].x, pas), Q(positions[i].y, pas), Q(positions[i].z, pas), i};
			}
			NkSort(cles.Data(), cles.Data() + n, Avant);
			rep.Resize(n, 0);
			for (uint32 k = 0; k < n; ++k) {
				const bool neuf = k == 0 || cles[k].x != cles[k - 1].x || cles[k].y != cles[k - 1].y || cles[k].z != cles[k - 1].z;
				if (neuf) {
					soudes.PushBack(cles[k].v);
				}
				rep[cles[k].v] = (uint32)soudes.Size() - 1u;
			}
			// VOISINS (uniques) des representants.
			const uint32 ns = (uint32)soudes.Size();
			NkVector<uint32> degre;
			degre.Resize(ns, 0);
			NkVector<uint32> paires; // (a, b) a plat, les deux sens
			const uint32 nt = TriangleCount();
			for (uint32 t = 0; t < nt; ++t) {
				const uint32 r[3] = {rep[indices[t * 3]], rep[indices[t * 3 + 1]], rep[indices[t * 3 + 2]]};
				for (uint32 e = 0; e < 3; ++e) {
					const uint32 a = r[e], b = r[(e + 1) % 3];
					if (a != b) {
						paires.PushBack(a);
						paires.PushBack(b);
						paires.PushBack(b);
						paires.PushBack(a);
					}
				}
			}
			// Tri des paires (a, b) puis compactage : CSR sans doublon.
			struct P {
					uint32 a, b;
			};
			NkVector<P> pp;
			pp.Resize((uint32)paires.Size() / 2u);
			for (uint32 k = 0; k < (uint32)pp.Size(); ++k) {
				pp[k] = P{paires[k * 2], paires[k * 2 + 1]};
			}
			NkSort(pp.Data(), pp.Data() + pp.Size(), [](const P &x, const P &y) { return x.a != y.a ? x.a < y.a : x.b < y.b; });
			adjDebut.Resize(ns + 1, 0);
			uint32 courant = 0;
			for (uint32 k = 0; k < (uint32)pp.Size(); ++k) {
				if (k > 0 && pp[k].a == pp[k - 1].a && pp[k].b == pp[k - 1].b) {
					continue;
				}
				while (courant <= pp[k].a) {
					adjDebut[courant++] = (uint32)adj.Size();
				}
				adj.PushBack(pp[k].b);
			}
			while (courant <= ns) {
				adjDebut[courant++] = (uint32)adj.Size();
			}
			(void)degre;
		}

		uint32 NkSkinMesh::BuildMirror(float32 tol) {
			const uint32 n = VertexCount();
			mirror.Clear();
			mirror.Resize(n, -1);
			if (n == 0) {
				return 0;
			}
			if (tol <= 0.f) {
				tol = (Diagonal() > 0.f ? Diagonal() : 1.f) * 0.002f;
			}
			NkVector<Cle> cles;
			cles.Resize(n);
			for (uint32 i = 0; i < n; ++i) {
				cles[i] = Cle{Q(positions[i].x, tol), Q(positions[i].y, tol), Q(positions[i].z, tol), i};
			}
			NkSort(cles.Data(), cles.Data() + n, Avant);
			uint32 apparies = 0;
			for (uint32 i = 0; i < n; ++i) {
				const NkVec3f m = MiroirX(positions[i]);
				const int32 qx = Q(m.x, tol), qy = Q(m.y, tol), qz = Q(m.z, tol);
				float32 meilleur = tol;
				int32 trouve = -1;
				for (int32 dx = -1; dx <= 1; ++dx) {
					for (int32 dy = -1; dy <= 1; ++dy) {
						for (int32 dz = -1; dz <= 1; ++dz) {
							for (uint32 k = Borne(cles, qx + dx, qy + dy, qz + dz); k < n; ++k) {
								const Cle &c = cles[k];
								if (c.x != qx + dx || c.y != qy + dy || c.z != qz + dz) {
									break;
								}
								const float32 d = Dist(positions[c.v], m);
								if (d <= meilleur) {
									meilleur = d;
									trouve = (int32)c.v;
								}
							}
						}
					}
				}
				mirror[i] = trouve;
				apparies += trouve >= 0 ? 1u : 0u;
			}
			return apparies;
		}

		void NkSkinMesh::ComputeNormals(const NkVec3f *pos, uint32 n, const uint32 *idx, uint32 nIdx, const NkVector<uint32> *rp,
										NkVector<NkVec3f> &out) {
			out.Resize(n, V(0.f, 0.f, 0.f));
			for (uint32 i = 0; i < n; ++i) {
				out[i] = V(0.f, 0.f, 0.f);
			}
			// Accumule sur les REPRESENTANTS (si fournis) : la normale ne casse pas a
			// une couture d'UV.
			const bool soude = rp != nullptr && (uint32)rp->Size() == n;
			NkVector<NkVec3f> acc;
			if (soude) {
				uint32 ns = 0;
				for (uint32 i = 0; i < n; ++i) {
					ns = (*rp)[i] + 1 > ns ? (*rp)[i] + 1 : ns;
				}
				acc.Resize(ns, V(0.f, 0.f, 0.f));
			}
			for (uint32 t = 0; t + 2 < nIdx; t += 3) {
				const uint32 a = idx[t], b = idx[t + 1], c = idx[t + 2];
				if (a >= n || b >= n || c >= n) {
					continue;
				}
				const NkVec3f f = Cross(Sub(pos[b], pos[a]), Sub(pos[c], pos[a])); // aire x 2
				if (soude) {
					acc[(*rp)[a]] = Add(acc[(*rp)[a]], f);
					acc[(*rp)[b]] = Add(acc[(*rp)[b]], f);
					acc[(*rp)[c]] = Add(acc[(*rp)[c]], f);
				} else {
					out[a] = Add(out[a], f);
					out[b] = Add(out[b], f);
					out[c] = Add(out[c], f);
				}
			}
			for (uint32 i = 0; i < n; ++i) {
				out[i] = Norm(soude ? acc[(*rp)[i]] : out[i], V(0.f, 1.f, 0.f));
			}
		}

		void NkSkinMesh::ComputeNormals() {
			ComputeNormals(positions.Data(), VertexCount(), indices.Data(), (uint32)indices.Size(), &rep, normals);
		}

		bool NkSkinMesh::Contains(const NkVec3f &p) const {
			// Trois directions LEGEREMENT de biais : un rayon exactement dans l'axe
			// rencontre les aretes des maillages de grille.
			const NkVec3f dirs[3] = {Norm(V(1.f, 0.0123f, 0.0371f)), Norm(V(0.0213f, 1.f, 0.0157f)), Norm(V(0.0311f, 0.0177f, 1.f))};
			int32 dedans = 0;
			const uint32 nt = TriangleCount();
			for (uint32 r = 0; r < 3; ++r) {
				uint32 traverses = 0;
				for (uint32 t = 0; t < nt; ++t) {
					const uint32 a = indices[t * 3], b = indices[t * 3 + 1], c = indices[t * 3 + 2];
					if (Rayon(p, dirs[r], positions[a], positions[b], positions[c])) {
						++traverses;
					}
				}
				dedans += (traverses & 1u) ? 1 : 0;
			}
			return dedans >= 2;
		}

		bool NkSkinMesh::RayVolumeMiddle(const NkVec3f &o, const NkVec3f &d, NkVec3f &out, float32 *epaisseur) const {
			const float32 ld = Len(d);
			if (ld < 1e-12f) {
				return false;
			}
			const NkVec3f dir = Mul(d, 1.f / ld);
			// Les DEUX premiers impacts distincts (deux triangles qui partagent
			// l'arete traversee rendent le meme t : on ne le compte qu'une fois).
			const float32 eps = 1e-5f * (Diagonal() > 0.f ? Diagonal() : 1.f);
			float32 t0 = 1e30f, t1 = 1e30f;
			const uint32 nt = TriangleCount();
			for (uint32 k = 0; k < nt; ++k) {
				float32 t = 0.f;
				if (!Rayon(o, dir, positions[indices[k * 3]], positions[indices[k * 3 + 1]], positions[indices[k * 3 + 2]], &t) || t <= 0.f) {
					continue;
				}
				if (std::fabs(t - t0) <= eps || std::fabs(t - t1) <= eps) {
					continue;
				}
				if (t < t0) {
					t1 = t0;
					t0 = t;
				} else if (t < t1) {
					t1 = t;
				}
			}
			if (t0 >= 1e29f) {
				return false;
			}
			const bool sortie = t1 < 1e29f;
			out = Add(o, Mul(dir, sortie ? (t0 + t1) * 0.5f : t0));
			if (epaisseur != nullptr) {
				*epaisseur = sortie ? t1 - t0 : 0.f;
			}
			return true;
		}

	} // namespace anim
} // namespace nkentseu
