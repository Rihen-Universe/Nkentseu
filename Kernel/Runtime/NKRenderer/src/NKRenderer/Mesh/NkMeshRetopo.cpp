// -----------------------------------------------------------------------------
// @File    NkMeshRetopo.cpp
// @Brief   Champ de croix et fusion quad-dominante. Voir l'en-tete pour le
//          POURQUOI ; ici le COMMENT.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NkMeshRetopo.h"

#include <math.h>
#include <stdlib.h> // qsort

namespace nkentseu {
	namespace renderer {

		namespace {

			inline NkVec3f Sub(const NkVec3f &a, const NkVec3f &b) {
				return {a.x - b.x, a.y - b.y, a.z - b.z};
			}
			inline NkVec3f CrossV(const NkVec3f &a, const NkVec3f &b) {
				return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
			}
			inline float32 DotV(const NkVec3f &a, const NkVec3f &b) {
				return a.x * b.x + a.y * b.y + a.z * b.z;
			}
			inline float32 LenV(const NkVec3f &a) {
				return sqrtf(DotV(a, a));
			}
			inline NkVec3f NormV(const NkVec3f &a) {
				const float32 l = LenV(a);
				return l > 1e-12f ? NkVec3f{a.x / l, a.y / l, a.z / l} : NkVec3f{0.f, 0.f, 0.f};
			}

			// Topologie de travail : triangles sur sommets SOUDES. La soudure est la
			// meme obligation que dans la decimation — sans elle, les primitives
			// dupliquent les sommets par face et aucune arete n'est partagee.
			struct Topo {
					NkVector<NkVec3f> pos;
					NkVector<uint32> tri;	  ///< 3 indices par triangle (soudes)
					NkVector<uint32> triFace; ///< id de la face NkEditMesh d'origine
					NkVector<NkVec3f> nrm;	  ///< normale par triangle

					// Aretes uniques : (a, b) tries, avec jusqu'a 2 triangles.
					NkVector<uint32> ea, eb;
					NkVector<int32> ef0, ef1;

					int32 FindEdge(uint32 a, uint32 b) const {
						const uint32 lo = a < b ? a : b, hi = a < b ? b : a;
						for (uint32 i = 0; i < (uint32)ea.Size(); ++i)
							if (ea[i] == lo && eb[i] == hi)
								return (int32)i;
						return -1;
					}
			};

			// Grille de hachage lineaire pour retrouver une arete en O(1) — le
			// FindEdge lineaire ci-dessus servirait de secours, mais sur quelques
			// milliers d'aretes le balayage par table est necessaire pour rester
			// dans des temps de harnais raisonnables.
			struct EdgeMap {
					NkVector<uint32> head; ///< tete de liste par alveole
					NkVector<uint32> next;
					uint32 mask = 0;

					void Init(uint32 edgeCapacity) {
						uint32 buckets = 16;
						while (buckets < edgeCapacity * 2u)
							buckets <<= 1;
						mask = buckets - 1u;
						head.Resize(buckets);
						for (uint32 i = 0; i < buckets; ++i)
							head[i] = 0xFFFFFFFFu;
						next.Clear();
					}
					static uint32 Hash(uint32 a, uint32 b) {
						uint32 h = a * 2654435761u ^ (b * 40503u + 0x9E3779B9u);
						h ^= h >> 15;
						return h;
					}
					int32 Find(const Topo &t, uint32 a, uint32 b) const {
						const uint32 lo = a < b ? a : b, hi = a < b ? b : a;
						for (uint32 i = head[Hash(lo, hi) & mask]; i != 0xFFFFFFFFu; i = next[i])
							if (t.ea[i] == lo && t.eb[i] == hi)
								return (int32)i;
						return -1;
					}
					void Insert(Topo &t, uint32 idx) {
						const uint32 h = Hash(t.ea[idx], t.eb[idx]) & mask;
						next.PushBack(head[h]); // next[idx] — insere dans l'ordre
						head[h] = idx;
					}
			};

			bool BuildTopo(const NkEditMesh &m, Topo &t, EdgeMap &em) {
				NkVector<uint32> canon;
				m.BuildVertexMerge(canon);
				NkVector<uint32> remap;
				remap.Resize(m.VertCount());
				for (uint32 i = 0; i < m.VertCount(); ++i)
					remap[i] = 0xFFFFFFFFu;
				for (uint32 i = 0; i < m.VertCount(); ++i) {
					const uint32 cv = canon[i];
					if (remap[cv] == 0xFFFFFFFFu) {
						remap[cv] = (uint32)t.pos.Size();
						t.pos.PushBack(m.verts[cv].pos);
					}
					remap[i] = remap[cv];
				}
				NkVector<NkEmId> fv;
				for (uint32 f = 0; f < m.FaceCount(); ++f) {
					if (!m.faces[f].alive) // la convention apprise a la passe 1
						continue;
					m.GetFaceVerts((NkEmId)f, fv);
					if (fv.Size() != 3)
						continue; // seules les faces triangulaires participent
					const uint32 a = remap[fv[0]], b = remap[fv[1]], c = remap[fv[2]];
					if (a == b || b == c || a == c)
						continue;
					t.tri.PushBack(a);
					t.tri.PushBack(b);
					t.tri.PushBack(c);
					t.triFace.PushBack(f);
					const NkVec3f n = NormV(CrossV(Sub(t.pos[b], t.pos[a]), Sub(t.pos[c], t.pos[a])));
					t.nrm.PushBack(n);
				}
				const uint32 nTri = (uint32)t.triFace.Size();
				if (nTri == 0)
					return false;
				em.Init(nTri * 3u);
				for (uint32 f = 0; f < nTri; ++f) {
					for (uint32 s = 0; s < 3; ++s) {
						const uint32 a = t.tri[f * 3 + s], b = t.tri[f * 3 + (s + 1) % 3];
						const int32 e = em.Find(t, a, b);
						if (e < 0) {
							t.ea.PushBack(a < b ? a : b);
							t.eb.PushBack(a < b ? b : a);
							t.ef0.PushBack((int32)f);
							t.ef1.PushBack(-1);
							em.Insert(t, (uint32)t.ea.Size() - 1u);
						} else if (t.ef1[(uint32)e] < 0 && t.ef0[(uint32)e] != (int32)f) {
							t.ef1[(uint32)e] = (int32)f;
						}
						// Une troisieme face sur la meme arete (non manifold) est ignoree :
						// elle ne participera a aucune fusion, ce qui est le comportement sur.
					}
				}
				return true;
			}

			// Projette `d` dans le plan de normale `n` et normalise.
			inline NkVec3f ProjectPlane(const NkVec3f &d, const NkVec3f &n) {
				const float32 k = DotV(d, n);
				return NormV(NkVec3f{d.x - n.x * k, d.y - n.y * k, d.z - n.z * k});
			}

			// Representant de la croix de `d` (dans le plan de normale `n`) le plus
			// proche de `ref` : d, -d, n x d ou -(n x d). C'est LA symetrie a quarts
			// de tour qui fait qu'une rangee peut rencontrer une colonne sans conflit.
			inline NkVec3f BestOfCross(const NkVec3f &d, const NkVec3f &n, const NkVec3f &ref) {
				const NkVec3f c = NormV(CrossV(n, d));
				NkVec3f best = d;
				float32 bd = DotV(d, ref);
				const NkVec3f cands[3] = {{-d.x, -d.y, -d.z}, c, {-c.x, -c.y, -c.z}};
				for (int32 k = 0; k < 3; ++k) {
					const float32 dd = DotV(cands[k], ref);
					if (dd > bd) {
						bd = dd;
						best = cands[k];
					}
				}
				return best;
			}

			// Alignement d'une direction d'arete sur la croix (0..1) : le meilleur des
			// deux axes, puisque la croix ne distingue pas rangee et colonne.
			inline float32 AlignToCross(const NkVec3f &e, const NkVec3f &dir, const NkVec3f &n) {
				const float32 a = fabsf(DotV(e, dir));
				const float32 b = fabsf(DotV(e, NormV(CrossV(n, dir))));
				return a > b ? a : b;
			}

			struct QuadCand {
					float32 score = 0.f;
					float32 align = 0.f;
					uint32 edge = 0;
			};

			int CandCmp(const void *pa, const void *pb) {
				const QuadCand *a = (const QuadCand *)pa;
				const QuadCand *b = (const QuadCand *)pb;
				if (a->score > b->score)
					return -1;
				if (a->score < b->score)
					return 1;
				// Departage DETERMINISTE : sans lui, deux scores egaux dependraient de
				// l'ordre interne de qsort, et la signature du harnais fluctuerait.
				return a->edge < b->edge ? -1 : (a->edge > b->edge ? 1 : 0);
			}

			// Qualite d'angles d'un quad : 1 = quatre angles droits, 0 = degenere.
			float32 QuadAngleQuality(const NkVec3f q[4]) {
				float32 worst = 1.f;
				for (int32 k = 0; k < 4; ++k) {
					const NkVec3f &p = q[k];
					const NkVec3f &prev = q[(k + 3) % 4];
					const NkVec3f &nxt = q[(k + 1) % 4];
					const NkVec3f u = NormV(Sub(prev, p)), v = NormV(Sub(nxt, p));
					// L'ecart au droit, rapporte a 90 degres.
					const float32 dev = fabsf(DotV(u, v)); // cos(angle) : 0 = droit
					const float32 qual = 1.f - dev;
					if (qual < worst)
						worst = qual;
				}
				return worst;
			}

		} // namespace

		void NkMeshRetopo::ComputeCrossField(const NkEditMesh &m, float32 featureAngleDeg, int32 iters,
											 NkVector<NkVec3f> &outDir, NkVector<uint8> &outPinned) {
			outDir.Clear();
			outPinned.Clear();
			Topo t;
			EdgeMap em;
			if (!BuildTopo(m, t, em))
				return;
			const uint32 nTri = (uint32)t.triFace.Size();
			outDir.Resize(nTri);
			outPinned.Resize(nTri);

			// 1. EPINGLES : bords et aretes vives imposent leur direction.
			const float32 cosFeat = cosf(featureAngleDeg * 3.14159265f / 180.f);
			for (uint32 f = 0; f < nTri; ++f)
				outPinned[f] = 0;
			for (uint32 e = 0; e < (uint32)t.ea.Size(); ++e) {
				const NkVec3f dir = NormV(Sub(t.pos[t.eb[e]], t.pos[t.ea[e]]));
				const int32 f0 = t.ef0[e], f1 = t.ef1[e];
				if (f1 < 0) { // BORD : une seule face
					outDir[(uint32)f0] = ProjectPlane(dir, t.nrm[(uint32)f0]);
					outPinned[(uint32)f0] = 1;
					continue;
				}
				if (DotV(t.nrm[(uint32)f0], t.nrm[(uint32)f1]) < cosFeat) { // ARETE VIVE
					outDir[(uint32)f0] = ProjectPlane(dir, t.nrm[(uint32)f0]);
					outDir[(uint32)f1] = ProjectPlane(dir, t.nrm[(uint32)f1]);
					outPinned[(uint32)f0] = 1;
					outPinned[(uint32)f1] = 1;
				}
			}

			// Faces libres : initialisation par l'arete LA PLUS LONGUE. Sur une
			// triangulation de quads c'est la diagonale — un depart volontairement
			// oblique, pour que le lissage ait quelque chose a corriger et que les
			// tests le voient travailler.
			for (uint32 f = 0; f < nTri; ++f) {
				if (outPinned[f])
					continue;
				float32 best = -1.f;
				NkVec3f bd{1.f, 0.f, 0.f};
				for (uint32 s = 0; s < 3; ++s) {
					const NkVec3f e = Sub(t.pos[t.tri[f * 3 + (s + 1) % 3]], t.pos[t.tri[f * 3 + s]]);
					const float32 l = LenV(e);
					if (l > best) {
						best = l;
						bd = e;
					}
				}
				outDir[f] = ProjectPlane(bd, t.nrm[f]);
			}

			// 2. LISSAGE. Chaque face libre s'aligne sur la moyenne de ses voisines,
			// chacune ramenee dans son plan et reduite a son representant de croix le
			// plus proche. Gauss-Seidel (en place) : les epingles tirent, le champ
			// converge en quelques passes.
			for (int32 it = 0; it < iters; ++it) {
				for (uint32 f = 0; f < nTri; ++f) {
					if (outPinned[f])
						continue;
					NkVec3f acc{0.f, 0.f, 0.f};
					for (uint32 s = 0; s < 3; ++s) {
						const uint32 a = t.tri[f * 3 + s], b = t.tri[f * 3 + (s + 1) % 3];
						const int32 e = em.Find(t, a, b);
						if (e < 0)
							continue;
						const int32 g = (t.ef0[(uint32)e] == (int32)f) ? t.ef1[(uint32)e] : t.ef0[(uint32)e];
						if (g < 0)
							continue;
						const NkVec3f dg = ProjectPlane(outDir[(uint32)g], t.nrm[f]);
						if (LenV(dg) < 0.5f)
							continue; // voisin quasi perpendiculaire au plan : inutilisable
						const NkVec3f rep = BestOfCross(dg, t.nrm[f], outDir[f]);
						acc.x += rep.x;
						acc.y += rep.y;
						acc.z += rep.z;
					}
					const NkVec3f nd = ProjectPlane(acc, t.nrm[f]);
					if (LenV(nd) > 0.5f)
						outDir[f] = nd;
				}
			}
		}

		bool NkMeshRetopo::QuadDominant(NkEditMesh &m, const NkRetopoParams &p, NkRetopoStats *out) {
			NkRetopoStats st;

			// ── PASSE 1 (optionnelle) : decimation QEM ──────────────────────────
			if (p.targetFaces > 0 || p.targetRatio < 1.f) {
				NkDecimateParams dp;
				dp.targetRatio = p.targetRatio;
				dp.targetFaces = p.targetFaces;
				if (!NkMeshDecimate::DecimateQEM(m, dp, &st.decim))
					return false;
			}

			// ── CHAMP ───────────────────────────────────────────────────────────
			NkVector<NkVec3f> dir;
			NkVector<uint8> pinned;
			ComputeCrossField(m, p.featureAngleDeg, p.fieldSmoothIters, dir, pinned);
			if (dir.Empty())
				return false;

			Topo t;
			EdgeMap em;
			if (!BuildTopo(m, t, em))
				return false;
			const uint32 nTri = (uint32)t.triFace.Size();
			st.trisIn = nTri;
			for (uint32 f = 0; f < nTri; ++f)
				if (pinned[f])
					st.pinnedFaces++;

			// ── CANDIDATS ───────────────────────────────────────────────────────
			const float32 cosFold = cosf(p.maxFoldDeg * 3.14159265f / 180.f);
			NkVector<QuadCand> cands;
			NkVector<uint32> quadOf; // 4 sommets par candidat retenu, indexes par arete
			quadOf.Resize((uint32)t.ea.Size() * 4u);
			for (uint32 e = 0; e < (uint32)t.ea.Size(); ++e) {
				const int32 f0 = t.ef0[e], f1 = t.ef1[e];
				if (f0 < 0 || f1 < 0)
					continue;
				// Un quad plie en deux n'est pas un quad.
				if (DotV(t.nrm[(uint32)f0], t.nrm[(uint32)f1]) < cosFold)
					continue;
				const uint32 u = t.ea[e], v = t.eb[e];
				// Sommets opposes a l'arete dans chaque triangle.
				uint32 w0 = 0xFFFFFFFFu, w1 = 0xFFFFFFFFu;
				for (uint32 s = 0; s < 3; ++s) {
					const uint32 x0 = t.tri[(uint32)f0 * 3 + s];
					if (x0 != u && x0 != v)
						w0 = x0;
					const uint32 x1 = t.tri[(uint32)f1 * 3 + s];
					if (x1 != u && x1 != v)
						w1 = x1;
				}
				if (w0 == 0xFFFFFFFFu || w1 == 0xFFFFFFFFu || w0 == w1)
					continue;
				// Cycle du quad : w0 -> u -> w1 -> v. L'orientation exacte est reprise
				// du triangle f0 a l'emission, ici seul le cycle compte.
				const uint32 q[4] = {w0, u, w1, v};
				NkVec3f qp[4];
				for (int32 k = 0; k < 4; ++k)
					qp[k] = t.pos[q[k]];
				const float32 angleQ = QuadAngleQuality(qp);
				if (angleQ < p.minQuadQuality)
					continue;
				// Alignement des QUATRE aretes du quad sur le champ des deux faces.
				float32 align = 0.f;
				for (int32 k = 0; k < 4; ++k) {
					const NkVec3f ev = NormV(Sub(qp[(k + 1) % 4], qp[k]));
					// Les aretes issues de f0 se comparent au champ de f0, celles de f1
					// au champ de f1 (k=0,3 : cote w0/f0 ; k=1,2 : cote w1/f1).
					const uint32 ff = (k == 0 || k == 3) ? (uint32)f0 : (uint32)f1;
					align += AlignToCross(ev, dir[ff], t.nrm[ff]);
				}
				align *= 0.25f;
				QuadCand c;
				c.align = align;
				c.score = align * angleQ * DotV(t.nrm[(uint32)f0], t.nrm[(uint32)f1]);
				c.edge = e;
				cands.PushBack(c);
				for (int32 k = 0; k < 4; ++k)
					quadOf[e * 4u + (uint32)k] = q[k];
			}
			if (!cands.Empty())
				qsort(&cands[0], cands.Size(), sizeof(QuadCand), CandCmp);

			// ── FUSION GLOUTONNE ────────────────────────────────────────────────
			NkVector<uint8> used;
			used.Resize(nTri);
			for (uint32 f = 0; f < nTri; ++f)
				used[f] = 0;
			NkVector<uint32> chosen;
			float32 alignSum = 0.f;
			for (uint32 i = 0; i < (uint32)cands.Size(); ++i) {
				const uint32 e = cands[i].edge;
				const uint32 f0 = (uint32)t.ef0[e], f1 = (uint32)t.ef1[e];
				if (used[f0] || used[f1])
					continue;
				used[f0] = 1;
				used[f1] = 1;
				chosen.PushBack(e);
				alignSum += cands[i].align;
			}

			// ── EMISSION ────────────────────────────────────────────────────────
			// L'ORIENTATION du quad reprend celle du triangle f0 : on emet le cycle
			// w0 -> u -> w1 -> v dans le sens ou f0 parcourait (u, v). Sans ce soin,
			// la moitie des quads sortirait a l'envers et l'eclairage clignoterait.
			NkVector<NkVertex3D> outV;
			NkVector<uint32> faceStart, faceVerts;
			NkVector<uint32> vmap;
			vmap.Resize((uint32)t.pos.Size());
			for (uint32 i = 0; i < (uint32)t.pos.Size(); ++i)
				vmap[i] = 0xFFFFFFFFu;
			auto emitVert = [&](uint32 i) -> uint32 {
				if (vmap[i] == 0xFFFFFFFFu) {
					vmap[i] = (uint32)outV.Size();
					NkVertex3D nv{};
					nv.pos = t.pos[i];
					nv.normal = {0.f, 1.f, 0.f};
					nv.tangent = {1.f, 0.f, 0.f};
					nv.color = 0xFFFFFFFFu;
					outV.PushBack(nv);
				}
				return vmap[i];
			};
			faceStart.PushBack(0);
			for (uint32 c = 0; c < (uint32)chosen.Size(); ++c) {
				const uint32 e = chosen[c];
				const uint32 f0 = (uint32)t.ef0[e];
				const uint32 *q = &quadOf[e * 4u]; // w0, u, w1, v
				// Dans f0, si l'ordre est (u ... v) on emet w0,u,w1,v ; si (v ... u),
				// l'inverse. On cherche u puis regarde si v le suit.
				uint32 w0 = q[0], u = q[1], w1 = q[2], v = q[3];
				bool uThenV = false;
				for (uint32 s = 0; s < 3; ++s)
					if (t.tri[f0 * 3 + s] == u && t.tri[f0 * 3 + (s + 1) % 3] == v)
						uThenV = true;
				const uint32 cyc[4] = {w0, u, w1, v};
				if (uThenV) {
					for (int32 k = 0; k < 4; ++k)
						faceVerts.PushBack(emitVert(cyc[k]));
				} else {
					for (int32 k = 3; k >= 0; --k)
						faceVerts.PushBack(emitVert(cyc[k]));
				}
				faceStart.PushBack((uint32)faceVerts.Size());
				st.quadsOut++;
			}
			for (uint32 f = 0; f < nTri; ++f) {
				if (used[f])
					continue;
				for (uint32 s = 0; s < 3; ++s)
					faceVerts.PushBack(emitVert(t.tri[f * 3 + s]));
				faceStart.PushBack((uint32)faceVerts.Size());
				st.trisOut++;
			}
			if (faceStart.Size() < 2)
				return false;

			m.BuildFromPolygons(outV.Data(), (uint32)outV.Size(), faceStart.Data(),
								(uint32)faceStart.Size() - 1u, faceVerts.Data());
			m.RecomputeNormals();

			const uint32 total = st.quadsOut + st.trisOut;
			st.quadRatio = total ? (float32)st.quadsOut / (float32)total : 0.f;
			st.alignMean = st.quadsOut ? alignSum / (float32)st.quadsOut : 0.f;
			if (out)
				*out = st;
			return true;
		}

	} // namespace renderer
} // namespace nkentseu
