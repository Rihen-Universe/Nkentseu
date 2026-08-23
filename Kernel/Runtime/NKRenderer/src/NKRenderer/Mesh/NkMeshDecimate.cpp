// -----------------------------------------------------------------------------
// @File    NkMeshDecimate.cpp
// @Brief   Decimation QEM par contraction d'aretes. Voir l'en-tete pour le
//          POURQUOI ; ici le COMMENT.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NkMeshDecimate.h"

#include <math.h>

namespace nkentseu {
	namespace renderer {

		namespace {

			// ── QUADRIQUE ───────────────────────────────────────────────────────
			// Matrice 4x4 SYMETRIQUE : 10 coefficients suffisent. Elle encode la
			// somme des carres des distances aux plans d'origine. L'erreur d'un
			// point v vaut v^T Q v.
			struct Quadric {
					float64 a = 0, b = 0, c = 0, d = 0; // ligne 0 : xx xy xz xw
					float64 e = 0, f = 0, g = 0;		// ligne 1 :    yy yz yw
					float64 h = 0, i = 0;				// ligne 2 :       zz zw
					float64 j = 0;						// ligne 3 :          ww

					void AddPlane(float64 nx, float64 ny, float64 nz, float64 nd, float64 w) {
						a += w * nx * nx;
						b += w * nx * ny;
						c += w * nx * nz;
						d += w * nx * nd;
						e += w * ny * ny;
						f += w * ny * nz;
						g += w * ny * nd;
						h += w * nz * nz;
						i += w * nz * nd;
						j += w * nd * nd;
					}
					void Add(const Quadric &o) {
						a += o.a;
						b += o.b;
						c += o.c;
						d += o.d;
						e += o.e;
						f += o.f;
						g += o.g;
						h += o.h;
						i += o.i;
						j += o.j;
					}
					float64 Eval(float64 x, float64 y, float64 z) const {
						return a * x * x + 2 * b * x * y + 2 * c * x * z + 2 * d * x + e * y * y + 2 * f * y * z
							   + 2 * g * y + h * z * z + 2 * i * z + j;
					}
			};

			struct V3 {
					float64 x = 0, y = 0, z = 0;
			};

			inline V3 Sub(const V3 &p, const V3 &q) {
				return V3{p.x - q.x, p.y - q.y, p.z - q.z};
			}
			inline V3 Cross(const V3 &p, const V3 &q) {
				return V3{p.y * q.z - p.z * q.y, p.z * q.x - p.x * q.z, p.x * q.y - p.y * q.x};
			}
			inline float64 Dot(const V3 &p, const V3 &q) {
				return p.x * q.x + p.y * q.y + p.z * q.z;
			}
			inline float64 Len(const V3 &p) {
				return sqrt(Dot(p, p));
			}
			inline V3 Norm(const V3 &p) {
				const float64 l = Len(p);
				return l > 1e-20 ? V3{p.x / l, p.y / l, p.z / l} : V3{0, 0, 0};
			}

			// ── TAS BINAIRE a suppression paresseuse ────────────────────────────
			// Les couts changent quand des contractions voisines ont lieu. Plutot que
			// de repositionner les entrees dans le tas (couteux et source d'erreurs),
			// on empile une NOUVELLE entree et on ignore les perimees a l'extraction
			// -- une entree est perimee si son estampille ne correspond plus.
			struct HeapItem {
					float64 cost = 0;
					uint32 u = 0, v = 0;
					uint32 stampU = 0, stampV = 0;
			};

			struct Heap {
					NkVector<HeapItem> h;

					void Push(const HeapItem &it) {
						h.PushBack(it);
						uint32 i = (uint32)h.Size() - 1u;
						while (i > 0) {
							const uint32 p = (i - 1u) / 2u;
							if (h[p].cost <= h[i].cost)
								break;
							const HeapItem t = h[p];
							h[p] = h[i];
							h[i] = t;
							i = p;
						}
					}
					bool Pop(HeapItem &out) {
						if (h.Empty())
							return false;
						out = h[0];
						const HeapItem last = h[(uint32)h.Size() - 1u];
						h.PopBack();
						if (!h.Empty()) {
							h[0] = last;
							uint32 i = 0;
							for (;;) {
								const uint32 l = 2u * i + 1u, r = l + 1u;
								uint32 s = i;
								if (l < (uint32)h.Size() && h[l].cost < h[s].cost)
									s = l;
								if (r < (uint32)h.Size() && h[r].cost < h[s].cost)
									s = r;
								if (s == i)
									break;
								const HeapItem t = h[s];
								h[s] = h[i];
								h[i] = t;
								i = s;
							}
						}
						return true;
					}
			};

			struct Ctx {
					NkVector<V3> pos;
					NkVector<uint32> tri;	 ///< 3 indices par triangle
					NkVector<uint16> triMat; ///< materiau de la FACE MERE de chaque triangle
					NkVector<uint8> triDead; ///< 1 = triangle supprime
					NkVector<uint8> vertDead;
					NkVector<uint32> stamp; ///< incremente a chaque modification d'un sommet
					NkVector<Quadric> quad;
					NkVector<NkVector<uint32>> vtri; ///< triangles incidents a chaque sommet
					uint32 liveTris = 0;

					V3 TriNormal(uint32 t) const {
						const V3 &A = pos[tri[t * 3 + 0]];
						const V3 &B = pos[tri[t * 3 + 1]];
						const V3 &C = pos[tri[t * 3 + 2]];
						return Norm(Cross(Sub(B, A), Sub(C, A)));
					}
					float64 TriArea(uint32 t) const {
						const V3 &A = pos[tri[t * 3 + 0]];
						const V3 &B = pos[tri[t * 3 + 1]];
						const V3 &C = pos[tri[t * 3 + 2]];
						return 0.5 * Len(Cross(Sub(B, A), Sub(C, A)));
					}
			};

			// Les triangles qui contiennent l'arete (u,v).
			void FacesOfEdge(const Ctx &c, uint32 u, uint32 v, NkVector<uint32> &out) {
				out.Clear();
				for (uint32 k = 0; k < (uint32)c.vtri[u].Size(); ++k) {
					const uint32 t = c.vtri[u][k];
					if (c.triDead[t])
						continue;
					const uint32 *T = &c.tri[t * 3];
					if (T[0] == v || T[1] == v || T[2] == v)
						out.PushBack(t);
				}
			}

			// Voisins d'un sommet (une seule fois chacun).
			void NeighborsOf(const Ctx &c, uint32 u, NkVector<uint32> &out) {
				out.Clear();
				for (uint32 k = 0; k < (uint32)c.vtri[u].Size(); ++k) {
					const uint32 t = c.vtri[u][k];
					if (c.triDead[t])
						continue;
					for (uint32 s = 0; s < 3; ++s) {
						const uint32 w = c.tri[t * 3 + s];
						if (w == u)
							continue;
						bool seen = false;
						for (uint32 q = 0; q < (uint32)out.Size() && !seen; ++q)
							if (out[q] == w)
								seen = true;
						if (!seen)
							out.PushBack(w);
					}
				}
			}

			// CONDITION DE LIEN. Une contraction preserve la variete si et seulement
			// si les voisins COMMUNS de u et v sont exactement les sommets opposes a
			// l'arete dans les faces qui la portent. Sinon, deux parties du maillage
			// qui ne se touchaient que « de loin » se retrouveraient collees par un
			// sommet -- un maillage non manifold, dont le compte de faces ne dit rien.
			bool LinkCondition(const Ctx &c, uint32 u, uint32 v) {
				NkVector<uint32> ef;
				FacesOfEdge(c, u, v, ef);
				// Une arete portee par plus de deux faces est deja non manifold : on ne
				// la touche pas, on n'aggrave pas.
				if (ef.Size() == 0 || ef.Size() > 2)
					return false;
				NkVector<uint32> opp;
				for (uint32 k = 0; k < (uint32)ef.Size(); ++k) {
					const uint32 *T = &c.tri[ef[k] * 3];
					for (uint32 s = 0; s < 3; ++s)
						if (T[s] != u && T[s] != v)
							opp.PushBack(T[s]);
				}
				NkVector<uint32> nu, nv;
				NeighborsOf(c, u, nu);
				NeighborsOf(c, v, nv);
				uint32 common = 0;
				for (uint32 a = 0; a < (uint32)nu.Size(); ++a) {
					if (nu[a] == v)
						continue;
					bool inV = false;
					for (uint32 b = 0; b < (uint32)nv.Size() && !inV; ++b)
						if (nv[b] == nu[a])
							inV = true;
					if (!inV)
						continue;
					common++;
					bool isOpp = false;
					for (uint32 o = 0; o < (uint32)opp.Size() && !isOpp; ++o)
						if (opp[o] == nu[a])
							isOpp = true;
					if (!isOpp)
						return false; // voisin commun qui n'est PAS oppose : collage
				}
				return common == (uint32)opp.Size();
			}

			// Le sommet est-il sur un bord ? Une arete de bord n'a qu'une face.
			bool IsBoundaryVert(const Ctx &c, uint32 u) {
				NkVector<uint32> nb;
				NeighborsOf(c, u, nb);
				NkVector<uint32> ef;
				for (uint32 k = 0; k < (uint32)nb.Size(); ++k) {
					FacesOfEdge(c, u, nb[k], ef);
					if (ef.Size() == 1)
						return true;
				}
				return false;
			}

			// RETOURNEMENT. On simule le deplacement et on compare chaque normale
			// avant/apres. Le cout QEM seul n'interdit pas de replier la surface sur
			// elle-meme : la contraction serait « pas chere » et desastreuse.
			bool WouldFlip(const Ctx &c, uint32 u, uint32 v, const V3 &np, float64 cosLimit) {
				for (uint32 side = 0; side < 2; ++side) {
					const uint32 w = side ? v : u;
					for (uint32 k = 0; k < (uint32)c.vtri[w].Size(); ++k) {
						const uint32 t = c.vtri[w][k];
						if (c.triDead[t])
							continue;
						const uint32 *T = &c.tri[t * 3];
						// Les faces portant l'arete disparaissent : rien a verifier.
						bool hasU = false, hasV = false;
						for (uint32 s = 0; s < 3; ++s) {
							if (T[s] == u)
								hasU = true;
							if (T[s] == v)
								hasV = true;
						}
						if (hasU && hasV)
							continue;
						V3 p[3];
						for (uint32 s = 0; s < 3; ++s)
							p[s] = (T[s] == u || T[s] == v) ? np : c.pos[T[s]];
						const V3 before = c.TriNormal(t);
						const V3 after = Norm(Cross(Sub(p[1], p[0]), Sub(p[2], p[0])));
						if (Len(after) < 1e-12)
							return true; // face degeneree : c'est un retournement en germe
						if (Dot(before, after) < cosLimit)
							return true;
					}
				}
				return false;
			}

			// Position optimale : on resout Q x = -b sur la partie 3x3. Si la matrice
			// est singuliere (arete plate, symetries), on retombe sur le meilleur des
			// trois candidats evidents -- deviner mal vaut mieux que diviser par zero.
			V3 OptimalPos(const Quadric &q, const V3 &pu, const V3 &pv) {
				const float64 m00 = q.a, m01 = q.b, m02 = q.c;
				const float64 m11 = q.e, m12 = q.f, m22 = q.h;
				const float64 det = m00 * (m11 * m22 - m12 * m12) - m01 * (m01 * m22 - m12 * m02)
									+ m02 * (m01 * m12 - m11 * m02);
				if (det > 1e-12 || det < -1e-12) {
					const float64 i00 = (m11 * m22 - m12 * m12) / det;
					const float64 i01 = -(m01 * m22 - m02 * m12) / det;
					const float64 i02 = (m01 * m12 - m02 * m11) / det;
					const float64 i11 = (m00 * m22 - m02 * m02) / det;
					const float64 i12 = -(m00 * m12 - m02 * m01) / det;
					const float64 i22 = (m00 * m11 - m01 * m01) / det;
					V3 r;
					r.x = -(i00 * q.d + i01 * q.g + i02 * q.i);
					r.y = -(i01 * q.d + i11 * q.g + i12 * q.i);
					r.z = -(i02 * q.d + i12 * q.g + i22 * q.i);
					return r;
				}
				const V3 mid{(pu.x + pv.x) * 0.5, (pu.y + pv.y) * 0.5, (pu.z + pv.z) * 0.5};
				const V3 cand[3] = {pu, pv, mid};
				float64 best = 1e300;
				V3 bp = mid;
				for (uint32 k = 0; k < 3; ++k) {
					const float64 e = q.Eval(cand[k].x, cand[k].y, cand[k].z);
					if (e < best) {
						best = e;
						bp = cand[k];
					}
				}
				return bp;
			}

		} // namespace

		// Slots DISTINCTS portes par des faces VIVANTES. C'est une RELATION qu'on
		// compare avant/apres, pas un compte fige : le nombre juste depend du
		// maillage, mais « il ne doit pas diminuer sans qu'on le dise » vaut toujours.
		static uint32 SlotsDistincts(const NkEditMesh &m) {
			NkVector<uint16> vus;
			for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f) {
				if (!m.faces[f].alive)
					continue;
				const uint16 s = m.faces[f].material;
				bool trouve = false;
				for (uint32 k = 0; k < (uint32)vus.Size() && !trouve; ++k)
					if (vus[k] == s)
						trouve = true;
				if (!trouve)
					vus.PushBack(s);
			}
			return (uint32)vus.Size();
		}

		bool NkMeshDecimate::DecimateQEM(NkEditMesh &m, const NkDecimateParams &p, NkDecimateStats *out) {
			NkDecimateStats st;
			if (m.FaceCount() == 0 || m.VertCount() == 0)
				return false;
			// Mesure AVANT toute modification : `m` est reconstruit en place a la fin.
			st.matSlotsBefore = SlotsDistincts(m);

			// ── 1. Triangulation + SOUDURE ──────────────────────────────────────
			// La soudure est indispensable : les primitives dupliquent les sommets par
			// face (un cube en compte 24 pour 8 reels). Sans elle, aucune arete ne
			// serait partagee et la contraction dechirerait le maillage face par face.
			NkVector<uint32> canon;
			m.BuildVertexMerge(canon);
			NkVector<uint32> remap;
			remap.Resize(m.VertCount());
			Ctx c;
			for (uint32 i = 0; i < m.VertCount(); ++i)
				remap[i] = 0xFFFFFFFFu;
			for (uint32 i = 0; i < m.VertCount(); ++i) {
				const uint32 cv = canon[i];
				if (remap[cv] == 0xFFFFFFFFu) {
					remap[cv] = (uint32)c.pos.Size();
					const NkVec3f &q = m.verts[cv].pos;
					c.pos.PushBack(V3{(float64)q.x, (float64)q.y, (float64)q.z});
				}
				remap[i] = remap[cv];
			}

			NkVector<NkEmId> fv;
			for (uint32 f = 0; f < m.FaceCount(); ++f) {
				// FaceCount() renvoie la TAILLE DE LA TABLE, faces mortes comprises —
				// `quadify` en laisse une par paire de triangles fusionnee. Sur une
				// face morte, GetFaceVerts parcourt un cycle rompu jusqu'a son
				// garde-fou de 100 000 sommets : sans ce test, une grille de 64 quads
				// produisait 3,2 MILLIONS de triangles. Tout le reste du fichier
				// NkEditMesh.cpp fait ce test ; c'est la convention.
				if (!m.faces[f].alive)
					continue;
				m.GetFaceVerts((NkEmId)f, fv);
				if (fv.Size() < 3)
					continue;
				// Eventail : suffisant ici, les faces d'un maillage a decimer sont des
				// triangles ou des quads convexes.
				for (uint32 k = 1; k + 1 < (uint32)fv.Size(); ++k) {
					const uint32 a = remap[fv[0]], b = remap[fv[k]], d = remap[fv[k + 1]];
					if (a == b || b == d || a == d)
						continue; // triangle degenere : rien a decimer dedans
					c.tri.PushBack(a);
					c.tri.PushBack(b);
					c.tri.PushBack(d);
					// MATERIAU DE LA FACE MERE. Un eventail decoupe UNE face : ses
					// triangles heritent tous du meme index, sans ambiguite — il n'y a
					// pas de fusion ici, donc pas de regle a arbitrer.
					c.triMat.PushBack(m.faces[f].material);
				}
			}
			const uint32 nTri = (uint32)c.tri.Size() / 3u;
			const uint32 nVert = (uint32)c.pos.Size();
			if (nTri == 0)
				return false;

			c.triDead.Resize(nTri);
			c.vertDead.Resize(nVert);
			c.stamp.Resize(nVert);
			c.quad.Resize(nVert);
			c.vtri.Resize(nVert);
			for (uint32 t = 0; t < nTri; ++t)
				c.triDead[t] = 0;
			for (uint32 v = 0; v < nVert; ++v) {
				c.vertDead[v] = 0;
				c.stamp[v] = 0;
				c.quad[v] = Quadric();
				c.vtri[v].Clear();
			}
			c.liveTris = nTri;
			st.trisBefore = nTri;
			st.vertsBefore = nVert;

			for (uint32 t = 0; t < nTri; ++t)
				for (uint32 s = 0; s < 3; ++s)
					c.vtri[c.tri[t * 3 + s]].PushBack(t);

			// ── 2. QUADRIQUES ───────────────────────────────────────────────────
			// Ponderees par l'AIRE : un grand triangle contraint davantage la surface
			// qu'une esquille. Sans ponderation, un maillage irregulier laisserait les
			// petits triangles peser autant que les grands.
			for (uint32 t = 0; t < nTri; ++t) {
				const V3 n = c.TriNormal(t);
				if (Len(n) < 1e-12)
					continue;
				const V3 &A = c.pos[c.tri[t * 3]];
				const float64 d = -Dot(n, A);
				const float64 w = c.TriArea(t);
				for (uint32 s = 0; s < 3; ++s)
					c.quad[c.tri[t * 3 + s]].AddPlane(n.x, n.y, n.z, d, w);
			}

			// BORD : plan virtuel PERPENDICULAIRE a la face, passant par l'arete. Il
			// retient le bord dans son plan ; sans lui la silhouette se retracte vers
			// l'interieur, defaut tres visible et qu'aucun compte ne signale.
			if (p.preserveBoundary) {
				NkVector<uint32> nb, ef;
				for (uint32 u = 0; u < nVert; ++u) {
					NeighborsOf(c, u, nb);
					for (uint32 k = 0; k < (uint32)nb.Size(); ++k) {
						const uint32 v = nb[k];
						if (v < u)
							continue; // chaque arete une seule fois
						FacesOfEdge(c, u, v, ef);
						if (ef.Size() != 1)
							continue;
						const V3 fn = c.TriNormal(ef[0]);
						const V3 e = Sub(c.pos[v], c.pos[u]);
						const V3 n = Norm(Cross(e, fn));
						if (Len(n) < 1e-12)
							continue;
						const float64 d = -Dot(n, c.pos[u]);
						// Poids fort : le bord doit peser plus que la surface qui le
						// tire, sinon la contrainte est simplement ignoree.
						const float64 w = Len(e) * Len(e) * 1000.0;
						c.quad[u].AddPlane(n.x, n.y, n.z, d, w);
						c.quad[v].AddPlane(n.x, n.y, n.z, d, w);
					}
				}
			}

			// ── 3. FILE DES CONTRACTIONS ────────────────────────────────────────
			const float64 cosLimit = cos((float64)p.maxNormalFlipDeg * 3.14159265358979 / 180.0);
			Heap heap;
			auto pushEdge = [&](uint32 u, uint32 v) {
				if (u == v || c.vertDead[u] || c.vertDead[v])
					return;
				Quadric q = c.quad[u];
				q.Add(c.quad[v]);
				const V3 np = OptimalPos(q, c.pos[u], c.pos[v]);
				HeapItem it;
				it.cost = q.Eval(np.x, np.y, np.z);
				if (it.cost < 0)
					it.cost = 0; // bruit numerique : une erreur quadratique n'est pas negative
				it.u = u;
				it.v = v;
				it.stampU = c.stamp[u];
				it.stampV = c.stamp[v];
				heap.Push(it);
			};

			{
				NkVector<uint32> nb;
				for (uint32 u = 0; u < nVert; ++u) {
					NeighborsOf(c, u, nb);
					for (uint32 k = 0; k < (uint32)nb.Size(); ++k)
						if (nb[k] > u)
							pushEdge(u, nb[k]);
				}
			}

			uint32 target = p.targetFaces;
			if (target == 0) {
				float32 r = p.targetRatio;
				if (r < 0.f)
					r = 0.f;
				if (r > 1.f)
					r = 1.f;
				target = (uint32)((float32)nTri * r);
			}
			if (target < 1)
				target = 1;

			// ── 4. BOUCLE ───────────────────────────────────────────────────────
			HeapItem it;
			NkVector<uint32> ef, nb;
			while (c.liveTris > target && heap.Pop(it)) {
				const uint32 u = it.u, v = it.v;
				if (c.vertDead[u] || c.vertDead[v])
					continue;
				// Entree PERIMEE : un voisin a bouge depuis, le cout n'est plus le bon.
				// On la jette au lieu de contracter sur une valeur fausse.
				if (it.stampU != c.stamp[u] || it.stampV != c.stamp[v])
					continue;
				if (p.maxError > 0.f && it.cost > (float64)p.maxError) {
					st.rejectedCost++;
					break; // le tas est trie : tout le reste coute plus cher encore
				}
				FacesOfEdge(c, u, v, ef);
				if (ef.Empty())
					continue; // l'arete n'existe plus
				if (p.preserveTopology && !LinkCondition(c, u, v)) {
					st.rejectedLink++;
					continue;
				}
				Quadric q = c.quad[u];
				q.Add(c.quad[v]);
				const V3 np = OptimalPos(q, c.pos[u], c.pos[v]);
				if (WouldFlip(c, u, v, np, cosLimit)) {
					st.rejectedFlip++;
					continue;
				}

				// Contraction : v disparait dans u.
				for (uint32 k = 0; k < (uint32)ef.Size(); ++k) {
					if (!c.triDead[ef[k]]) {
						c.triDead[ef[k]] = 1;
						c.liveTris--;
					}
				}
				for (uint32 k = 0; k < (uint32)c.vtri[v].Size(); ++k) {
					const uint32 t = c.vtri[v][k];
					if (c.triDead[t])
						continue;
					for (uint32 s = 0; s < 3; ++s)
						if (c.tri[t * 3 + s] == v)
							c.tri[t * 3 + s] = u;
					c.vtri[u].PushBack(t);
				}
				c.pos[u] = np;
				c.quad[u] = q;
				c.vertDead[v] = 1;
				c.stamp[u]++;
				st.collapses++;
				if (it.cost > (float64)st.maxCost)
					st.maxCost = (float32)it.cost;

				// SEULE la quadrique de `u` a change : on n'invalide QUE `u`, et on
				// reempile ses aretes avec la nouvelle estampille.
				//
				// Ne PAS toucher aux estampilles des voisins. Le cout d'une arete
				// (w1, w2) entre deux voisins de `u` depend de quad[w1] + quad[w2],
				// qui n'ont pas bouge. L'invalider la rendrait perimee sans que rien
				// ne la reempile : elle disparaitrait de la file pour de bon, et la
				// decimation calerait avant la cible sans qu'aucun compteur ne le
				// signale.
				NeighborsOf(c, u, nb);
				for (uint32 k = 0; k < (uint32)nb.Size(); ++k)
					pushEdge(u, nb[k]);
			}

			// ── 5. RECONSTRUCTION ───────────────────────────────────────────────
			NkVector<NkVertex3D> outV;
			NkVector<uint32> outI;
			NkVector<uint16> outMat; // un index par triangle SURVIVANT
			NkVector<uint32> newIdx;
			newIdx.Resize(nVert);
			for (uint32 i = 0; i < nVert; ++i)
				newIdx[i] = 0xFFFFFFFFu;
			for (uint32 t = 0; t < nTri; ++t) {
				if (c.triDead[t])
					continue;
				outMat.PushBack((t < (uint32)c.triMat.Size()) ? c.triMat[t] : (uint16)0);
				for (uint32 s = 0; s < 3; ++s) {
					const uint32 vi = c.tri[t * 3 + s];
					if (newIdx[vi] == 0xFFFFFFFFu) {
						newIdx[vi] = (uint32)outV.Size();
						NkVertex3D nv{};
						nv.pos = {(float32)c.pos[vi].x, (float32)c.pos[vi].y, (float32)c.pos[vi].z};
						nv.normal = {0.f, 1.f, 0.f};
						nv.tangent = {1.f, 0.f, 0.f};
						nv.color = 0xFFFFFFFFu;
						outV.PushBack(nv);
					}
					outI.PushBack(newIdx[vi]);
				}
			}
			if (outI.Empty())
				return false;

			// `quadify` reste faux : QEM produit des triangles, c'est assume dans
			// l'en-tete. Le compteur de fusion est branche quand meme — s'il devient
			// vrai un jour, la perte sera comptee sans qu'on ait a y repenser.
			m.BuildFromIndexed(outV.Data(), (uint32)outV.Size(), outI.Data(), (uint32)outI.Size(), false,
							   outMat.Data(), &st.facesMaterialChanged);
			m.RecomputeNormals();

			st.matSlotsAfter = SlotsDistincts(m);
			st.trisAfter = (uint32)outI.Size() / 3u;
			st.vertsAfter = (uint32)outV.Size();
			st.reachedTarget = (st.trisAfter <= target);
			if (out)
				*out = st;
			return true;
		}

		// Distance d'un point au triangle, en projetant puis en rabattant sur les
		// aretes si la projection tombe dehors. Comparer les sommets entre eux ne
		// suffirait pas : apres decimation les sommets ne coincident plus.
		static float64 PointTriDist(const V3 &p, const V3 &a, const V3 &b, const V3 &cc) {
			const V3 ab = Sub(b, a), ac = Sub(cc, a), ap = Sub(p, a);
			const float64 d1 = Dot(ab, ap), d2 = Dot(ac, ap);
			if (d1 <= 0 && d2 <= 0)
				return Len(ap);
			const V3 bp = Sub(p, b);
			const float64 d3 = Dot(ab, bp), d4 = Dot(ac, bp);
			if (d3 >= 0 && d4 <= d3)
				return Len(bp);
			const float64 vc = d1 * d4 - d3 * d2;
			if (vc <= 0 && d1 >= 0 && d3 <= 0) {
				const float64 t = d1 / (d1 - d3);
				return Len(Sub(p, V3{a.x + ab.x * t, a.y + ab.y * t, a.z + ab.z * t}));
			}
			const V3 cp = Sub(p, cc);
			const float64 d5 = Dot(ab, cp), d6 = Dot(ac, cp);
			if (d6 >= 0 && d5 <= d6)
				return Len(cp);
			const float64 vb = d5 * d2 - d1 * d6;
			if (vb <= 0 && d2 >= 0 && d6 <= 0) {
				const float64 t = d2 / (d2 - d6);
				return Len(Sub(p, V3{a.x + ac.x * t, a.y + ac.y * t, a.z + ac.z * t}));
			}
			const float64 va = d3 * d6 - d5 * d4;
			if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) {
				const float64 t = (d4 - d3) / ((d4 - d3) + (d5 - d6));
				const V3 bc = Sub(cc, b);
				return Len(Sub(p, V3{b.x + bc.x * t, b.y + bc.y * t, b.z + bc.z * t}));
			}
			const V3 n = Norm(Cross(ab, ac));
			return fabs(Dot(n, ap));
		}

		void NkMeshDecimate::ShapeError(const NkEditMesh &m, const NkEditMesh &ref, float32 &outMean,
										float32 &outMax) {
			outMean = 0.f;
			outMax = 0.f;
			// Triangles de `m`, en positions brutes.
			NkVector<V3> tp;
			NkVector<NkEmId> fv;
			for (uint32 f = 0; f < m.FaceCount(); ++f) {
				if (!m.faces[f].alive) // meme raison que dans DecimateQEM
					continue;
				m.GetFaceVerts((NkEmId)f, fv);
				for (uint32 k = 1; k + 1 < (uint32)fv.Size(); ++k) {
					const NkVec3f &A = m.verts[fv[0]].pos;
					const NkVec3f &B = m.verts[fv[k]].pos;
					const NkVec3f &C = m.verts[fv[k + 1]].pos;
					tp.PushBack(V3{A.x, A.y, A.z});
					tp.PushBack(V3{B.x, B.y, B.z});
					tp.PushBack(V3{C.x, C.y, C.z});
				}
			}
			if (tp.Empty() || ref.VertCount() == 0)
				return;
			float64 sum = 0;
			uint32 n = 0;
			for (uint32 i = 0; i < ref.VertCount(); ++i) {
				const NkVec3f &q = ref.verts[i].pos;
				const V3 P{q.x, q.y, q.z};
				float64 best = 1e300;
				for (uint32 t = 0; t + 2 < (uint32)tp.Size(); t += 3) {
					const float64 d = PointTriDist(P, tp[t], tp[t + 1], tp[t + 2]);
					if (d < best)
						best = d;
				}
				sum += best;
				n++;
				if (best > (float64)outMax)
					outMax = (float32)best;
			}
			if (n)
				outMean = (float32)(sum / (float64)n);
		}

	} // namespace renderer
} // namespace nkentseu
