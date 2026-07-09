// =============================================================================
// NkEditMesh.cpp — NKRenderer — maillage éditable demi-arête (n-gon)
// =============================================================================
#include "NkEditMesh.h"
#include "NKContainers/Associative/NkHashMap.h"

namespace nkentseu {
	namespace renderer {

		void NkEditMesh::BuildFromIndexed(const NkVertex3D *v, uint32 vc, const uint32 *idx, uint32 ic, bool quadify) {
			Clear();
			verts.Resize(vc);
			for (uint32 i = 0; i < vc; i++) {
				verts[i].pos = v[i].pos;
				verts[i].normal = v[i].normal;
				verts[i].uv = v[i].uv;
				verts[i].hedge = NK_EM_INVALID;
				verts[i].sel = 0;
			}
			const uint32 triCount = ic / 3;
			faces.Reserve(triCount);
			hedges.Reserve(ic);
			for (uint32 t = 0; t < triCount; t++) {
				const uint32 a = idx[t * 3], b = idx[t * 3 + 1], c = idx[t * 3 + 2];
				const NkEmId f = (NkEmId)faces.Size();
				const NkEmId h0 = (NkEmId)hedges.Size(), h1 = h0 + 1, h2 = h0 + 2;
				Hedge e0, e1, e2;
				e0.origin = a;
				e0.next = h1;
				e0.face = f;
				e1.origin = b;
				e1.next = h2;
				e1.face = f;
				e2.origin = c;
				e2.next = h0;
				e2.face = f;
				hedges.PushBack(e0);
				hedges.PushBack(e1);
				hedges.PushBack(e2);
				Face fc;
				fc.hedge = h0;
				fc.alive = 1;
				faces.PushBack(fc);
				if (verts[a].hedge == NK_EM_INVALID)
					verts[a].hedge = h0;
				if (verts[b].hedge == NK_EM_INVALID)
					verts[b].hedge = h1;
				if (verts[c].hedge == NK_EM_INVALID)
					verts[c].hedge = h2;
			}
			LinkTwins();
			RecomputeNormals();
			if (quadify)
				Quadify();
		}

		uint32 NkEditMesh::FaceSize(NkEmId f) const {
			if (f >= (NkEmId)faces.Size() || !faces[f].alive)
				return 0;
			const NkEmId start = faces[f].hedge;
			if (start == NK_EM_INVALID)
				return 0;
			NkEmId h = start;
			uint32 n = 0, guard = 0;
			do {
				++n;
				h = hedges[h].next;
				if (++guard > 100000u)
					break;
			} while (h != start && h != NK_EM_INVALID);
			return n;
		}

		void NkEditMesh::Quadify(float32 coplanarDot) {
			// Paires de triangles CONSÉCUTIFS (issus de la triangulation quad-par-quad).
			for (uint32 f1 = 0; f1 + 1 < (uint32)faces.Size(); f1 += 2) {
				const uint32 f2 = f1 + 1;
				if (!faces[f1].alive || !faces[f2].alive)
					continue;
				if (FaceSize(f1) != 3 || FaceSize(f2) != 3)
					continue;
				if (faces[f1].normal.Dot(faces[f2].normal) < coplanarDot)
					continue;
				// Demi-arête partagée h (dans f1) dont le twin est dans f2.
				NkEmId h = NK_EM_INVALID, start = faces[f1].hedge, hh = start;
				uint32 guard = 0;
				do {
					const NkEmId tw = hedges[hh].twin;
					if (tw != NK_EM_INVALID && hedges[tw].alive && hedges[tw].face == f2) {
						h = hh;
						break;
					}
					hh = hedges[hh].next;
				} while (hh != start && ++guard < 100000u);
				if (h == NK_EM_INVALID)
					continue; // triangles non adjacents
				const NkEmId tw = hedges[h].twin;
				const NkEmId hA = hedges[h].next, hB = hedges[hA].next;	 // f1 : b->c, c->a
				const NkEmId hC = hedges[tw].next, hD = hedges[hC].next; // f2 : a->d, d->b
				hedges[hB].next = hC;
				hedges[hD].next = hA; // recoud la boucle quad
				hedges[hA].face = f1;
				hedges[hB].face = f1;
				hedges[hC].face = f1;
				hedges[hD].face = f1;
				faces[f1].hedge = hA;
				faces[f2].alive = 0;
				const uint32 a = hedges[h].origin, b = hedges[tw].origin;
				hedges[h].alive = 0;
				hedges[tw].alive = 0;
				hedges[h].face = NK_EM_INVALID;
				hedges[tw].face = NK_EM_INVALID;
				verts[a].hedge = hC;
				verts[b].hedge = hA; // repointe (h/tw morts)
			}
			RecomputeNormals();
		}

		void NkEditMesh::LinkTwins() {
			NkHashMap<uint64, NkEmId> map;
			map.Reserve((uint32)hedges.Size());
			for (uint32 h = 0; h < (uint32)hedges.Size(); ++h) {
				const uint32 o = hedges[h].origin;
				const uint32 d = hedges[hedges[h].next].origin;
				const uint64 opp = ((uint64)d << 32) | (uint64)o; // demi-arête opposée (d->o)
				NkEmId *found = map.Find(opp);
				if (found) {
					hedges[h].twin = *found;
					hedges[*found].twin = h;
				} else {
					map.InsertOrAssign(((uint64)o << 32) | (uint64)d, h);
				}
			}
		}

		void NkEditMesh::GetFaceVerts(NkEmId f, NkVector<NkEmId> &out) const {
			out.Clear();
			if (f >= (NkEmId)faces.Size())
				return;
			const NkEmId start = faces[f].hedge;
			if (start == NK_EM_INVALID)
				return;
			NkEmId h = start;
			uint32 guard = 0;
			do {
				out.PushBack(hedges[h].origin);
				h = hedges[h].next;
				if (++guard > 100000u)
					break; // garde-fou (topologie cassée)
			} while (h != start && h != NK_EM_INVALID);
		}

		void NkEditMesh::RecomputeNormals() {
			for (uint32 i = 0; i < (uint32)verts.Size(); ++i)
				verts[i].normal = {0.f, 0.f, 0.f};
			NkVector<NkEmId> loop;
			for (uint32 f = 0; f < (uint32)faces.Size(); ++f) {
				if (!faces[f].alive)
					continue;
				loop.Clear();
				GetFaceVerts(f, loop);
				if (loop.Size() < 3)
					continue;
				const NkVec3f p0 = verts[loop[0]].pos, p1 = verts[loop[1]].pos, p2 = verts[loop[2]].pos;
				NkVec3f n = (p1 - p0).Cross(p2 - p0); // pondéré par l'aire (non normalisé)
				float32 l = n.Len();
				faces[f].normal = (l > 1e-8f) ? n * (1.f / l) : NkVec3f{0.f, 1.f, 0.f};
				for (uint32 k = 0; k < (uint32)loop.Size(); ++k)
					verts[loop[k]].normal = verts[loop[k]].normal + n;
			}
			for (uint32 i = 0; i < (uint32)verts.Size(); ++i) {
				float32 l = verts[i].normal.Len();
				verts[i].normal = (l > 1e-8f) ? verts[i].normal * (1.f / l) : NkVec3f{0.f, 1.f, 0.f};
			}
		}

		void NkEditMesh::GetUniqueEdges(NkVector<uint32> &outPairs) const {
			outPairs.Clear();
			for (uint32 h = 0; h < (uint32)hedges.Size(); ++h) {
				if (!hedges[h].alive)
					continue; // arête interne dissoute (quadify)
				const NkEmId tw = hedges[h].twin;
				if (tw == NK_EM_INVALID || h < tw) { // une seule des deux demi-arêtes
					const uint32 o = hedges[h].origin;
					const uint32 d = hedges[hedges[h].next].origin;
					outPairs.PushBack(o);
					outPairs.PushBack(d);
				}
			}
		}

		void NkEditMesh::Triangulate(NkVector<NkVertex3D> &outV, NkVector<uint32> &outIdx,
									 NkVector<NkEmId> &outTriFace) const {
			outV.Clear();
			outIdx.Clear();
			outTriFace.Clear();
			outV.Resize((uint32)verts.Size());
			for (uint32 i = 0; i < (uint32)verts.Size(); ++i) {
				NkVertex3D nv{};
				nv.pos = verts[i].pos;
				nv.normal = verts[i].normal;
				nv.tangent = {1.f, 0.f, 0.f};
				nv.uv = verts[i].uv;
				nv.uv2 = {0.f, 0.f};
				nv.color = 0xFFFFFFFFu;
				outV[i] = nv;
			}
			NkVector<NkEmId> loop;
			for (uint32 f = 0; f < (uint32)faces.Size(); ++f) {
				if (!faces[f].alive)
					continue;
				loop.Clear();
				GetFaceVerts(f, loop);
				if (loop.Size() < 3)
					continue;
				for (uint32 i = 1; i + 1 < (uint32)loop.Size(); ++i) { // éventail
					outIdx.PushBack(loop[0]);
					outIdx.PushBack(loop[i]);
					outIdx.PushBack(loop[i + 1]);
					outTriFace.PushBack((NkEmId)f);
				}
			}
		}

		void NkEditMesh::ToPolygons(NkVector<NkVertex3D> &ov, NkVector<uint32> &ofaceStart,
									NkVector<uint32> &ofaceVerts) const {
			ov.Resize((uint32)verts.Size());
			for (uint32 i = 0; i < (uint32)verts.Size(); ++i) {
				NkVertex3D nv{};
				nv.pos = verts[i].pos;
				nv.normal = verts[i].normal;
				nv.tangent = {1.f, 0.f, 0.f};
				nv.uv = verts[i].uv;
				nv.uv2 = {0.f, 0.f};
				nv.color = 0xFFFFFFFFu;
				ov[i] = nv;
			}
			ofaceStart.Clear();
			ofaceVerts.Clear();
			ofaceStart.PushBack(0);
			NkVector<NkEmId> loop;
			for (uint32 f = 0; f < (uint32)faces.Size(); ++f) {
				if (!faces[f].alive)
					continue;
				loop.Clear();
				GetFaceVerts(f, loop);
				if (loop.Size() < 3)
					continue;
				for (uint32 k = 0; k < (uint32)loop.Size(); ++k)
					ofaceVerts.PushBack(loop[k]);
				ofaceStart.PushBack((uint32)ofaceVerts.Size());
			}
		}

		void NkEditMesh::BuildFromPolygons(const NkVertex3D *v, uint32 vc, const uint32 *faceStart, uint32 faceCount,
										   const uint32 *faceVerts) {
			Clear();
			verts.Resize(vc);
			for (uint32 i = 0; i < vc; ++i) {
				verts[i].pos = v[i].pos;
				verts[i].normal = v[i].normal;
				verts[i].uv = v[i].uv;
				verts[i].hedge = NK_EM_INVALID;
				verts[i].sel = 0;
			}
			for (uint32 f = 0; f < faceCount; ++f) {
				const uint32 s = faceStart[f], e = faceStart[f + 1], n = e - s;
				if (n < 3)
					continue;
				const NkEmId h0 = (NkEmId)hedges.Size();
				for (uint32 k = 0; k < n; ++k) {
					Hedge he;
					he.origin = faceVerts[s + k];
					he.next = h0 + ((k + 1) % n);
					he.face = (NkEmId)faces.Size();
					he.alive = 1;
					hedges.PushBack(he);
					if (verts[faceVerts[s + k]].hedge == NK_EM_INVALID)
						verts[faceVerts[s + k]].hedge = h0 + k;
				}
				Face fc;
				fc.hedge = h0;
				fc.alive = 1;
				faces.PushBack(fc);
			}
			LinkTwins();
			RecomputeNormals();
		}

		// =====================================================================
		// COUCHE DE COMMANDES D'ÉDITION — ops paramétrées sur la sélection interne
		// (Vert::sel). Portées depuis Demo3D_*HE : logique topologique PURE (pas de
		// dépendance UI/GPU). L'appelant régénère le rendu (Triangulate) ensuite.
		// =====================================================================

		void NkEditMesh::SelectAll() {
			for (uint32 i = 0; i < (uint32)verts.Size(); ++i)
				verts[i].sel = 1;
		}

		void NkEditMesh::SelectNone() {
			for (uint32 i = 0; i < (uint32)verts.Size(); ++i)
				verts[i].sel = 0;
		}

		bool NkEditMesh::AnyVertSelected() const {
			for (uint32 i = 0; i < (uint32)verts.Size(); ++i)
				if (verts[i].sel)
					return true;
			return false;
		}

		bool NkEditMesh::PolyFaceSelected(const NkVector<uint32> &fv, uint32 s, uint32 e) const {
			for (uint32 k = s; k < e; k++) {
				const uint32 vi = fv[k];
				if (vi >= (uint32)verts.Size() || !verts[vi].sel)
					return false;
			}
			return e > s;
		}

		void NkEditMesh::ApplyVertSel(const NkVector<uint8> &vsel) {
			for (uint32 i = 0; i < (uint32)verts.Size(); ++i)
				verts[i].sel = (i < (uint32)vsel.Size()) ? vsel[i] : (uint8)0;
		}

		// EXTRUDE : duplique les faces sélectionnées (cap), crée des quads latéraux sur les
		// arêtes de BORD, décale le cap le long de la normale. p.individual = chaque face le
		// long de SA normale (caps séparés). Préserve les n-gons.
		bool NkEditMesh::ExtrudeSelectedFaces(const NkExtrudeParams &p) {
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			ToPolygons(pv, fs, fv);
			const uint32 fc = (fs.Size() > 0) ? (uint32)fs.Size() - 1 : 0;
			NkVec3f avgN{0.f, 0.f, 0.f};
			int32 selCount = 0;
			NkVector<uint8> faceSel;
			faceSel.Resize(fc);
			for (uint32 f = 0; f < fc; f++) {
				const uint32 s = fs[f], e = fs[f + 1];
				const bool sel = PolyFaceSelected(fv, s, e);
				faceSel[f] = sel ? 1 : 0;
				if (sel && e - s >= 3) {
					selCount++;
					avgN = avgN + (pv[fv[s + 1]].pos - pv[fv[s]].pos).Cross(pv[fv[s + 2]].pos - pv[fv[s]].pos);
				}
			}
			if (selCount == 0)
				return false;
			{
				float32 l = avgN.Len();
				avgN = (l > 1e-6f) ? avgN * (1.f / l) : NkVec3f{0.f, 1.f, 0.f};
			}
			float32 off = p.offset;
			if (off < 0.f) {
				NkVec3f bmn{1e30f, 1e30f, 1e30f}, bmx{-1e30f, -1e30f, -1e30f};
				for (uint32 i = 0; i < (uint32)pv.Size(); i++) {
					NkVec3f q = pv[i].pos;
					bmn.x = (q.x < bmn.x ? q.x : bmn.x);
					bmn.y = (q.y < bmn.y ? q.y : bmn.y);
					bmn.z = (q.z < bmn.z ? q.z : bmn.z);
					bmx.x = (q.x > bmx.x ? q.x : bmx.x);
					bmx.y = (q.y > bmx.y ? q.y : bmx.y);
					bmx.z = (q.z > bmx.z ? q.z : bmx.z);
				}
				off = (bmx - bmn).Len() * 0.08f;
			}

			if (p.individual) {
				NkVector<uint32> nfs, nfv;
				nfs.PushBack(0);
				NkVector<uint8> vsel;
				vsel.Resize((uint32)pv.Size());
				for (uint32 i = 0; i < (uint32)vsel.Size(); i++)
					vsel[i] = 0;
				for (uint32 f = 0; f < fc; f++) {
					if (faceSel[f])
						continue;
					for (uint32 k = fs[f]; k < fs[f + 1]; k++)
						nfv.PushBack(fv[k]);
					nfs.PushBack((uint32)nfv.Size());
				}
				for (uint32 f = 0; f < fc; f++) {
					if (!faceSel[f])
						continue;
					const uint32 s = fs[f], e = fs[f + 1], n = e - s;
					NkVec3f fn = (pv[fv[s + 1]].pos - pv[fv[s]].pos).Cross(pv[fv[s + 2]].pos - pv[fv[s]].pos);
					{
						float32 l = fn.Len();
						fn = (l > 1e-6f) ? fn * (1.f / l) : NkVec3f{0.f, 1.f, 0.f};
					}
					NkVector<uint32> dup;
					dup.Resize(n);
					for (uint32 k = 0; k < n; k++) {
						uint32 vi = fv[s + k];
						NkVertex3D nv = pv[vi];
						nv.pos = nv.pos + fn * off;
						dup[k] = (uint32)pv.Size();
						pv.PushBack(nv);
						vsel.PushBack(1);
					}
					for (uint32 k = 0; k < n; k++)
						nfv.PushBack(dup[k]);
					nfs.PushBack((uint32)nfv.Size()); // cap
					for (uint32 k = 0; k < n; k++) {
						uint32 a = fv[s + k], b = fv[s + (k + 1) % n], na = dup[k], nb = dup[(k + 1) % n];
						nfv.PushBack(a);
						nfv.PushBack(b);
						nfv.PushBack(nb);
						nfv.PushBack(na);
						nfs.PushBack((uint32)nfv.Size());
					}
				}
				BuildFromPolygons(pv.Data(), (uint32)pv.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data());
				ApplyVertSel(vsel);
				return true;
			}

			NkHashMap<uint64, uint8> selDir;
			for (uint32 f = 0; f < fc; f++) {
				if (!faceSel[f])
					continue;
				const uint32 s = fs[f], e = fs[f + 1], n = e - s;
				for (uint32 k = 0; k < n; k++) {
					uint32 a = fv[s + k], b = fv[s + (k + 1) % n];
					selDir.InsertOrAssign(((uint64)a << 32) | (uint64)b, (uint8)1);
				}
			}
			NkVector<int32> vmap;
			vmap.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)vmap.Size(); i++)
				vmap[i] = -1;
			NkVector<uint8> vsel;
			vsel.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)vsel.Size(); i++)
				vsel[i] = 0;
			for (uint32 f = 0; f < fc; f++) {
				if (!faceSel[f])
					continue;
				for (uint32 k = fs[f]; k < fs[f + 1]; k++) {
					uint32 vi = fv[k];
					if (vmap[vi] < 0) {
						vmap[vi] = (int32)pv.Size();
						NkVertex3D nv = pv[vi];
						nv.pos = nv.pos + avgN * off;
						pv.PushBack(nv);
						vsel.PushBack(1);
					}
				}
			}
			NkVector<uint32> nfs, nfv;
			nfs.PushBack(0);
			for (uint32 f = 0; f < fc; f++) {
				if (faceSel[f])
					continue;
				for (uint32 k = fs[f]; k < fs[f + 1]; k++)
					nfv.PushBack(fv[k]);
				nfs.PushBack((uint32)nfv.Size());
			}
			for (uint32 f = 0; f < fc; f++) {
				if (!faceSel[f])
					continue;
				for (uint32 k = fs[f]; k < fs[f + 1]; k++)
					nfv.PushBack((uint32)vmap[fv[k]]);
				nfs.PushBack((uint32)nfv.Size());
			}
			for (uint32 f = 0; f < fc; f++) {
				if (!faceSel[f])
					continue;
				const uint32 s = fs[f], e = fs[f + 1], n = e - s;
				for (uint32 k = 0; k < n; k++) {
					uint32 a = fv[s + k], b = fv[s + (k + 1) % n];
					if (selDir.Find(((uint64)b << 32) | (uint64)a))
						continue; // arête intérieure (2 faces sél.)
					uint32 na = (uint32)vmap[a], nb = (uint32)vmap[b];
					nfv.PushBack(a);
					nfv.PushBack(b);
					nfv.PushBack(nb);
					nfv.PushBack(na);
					nfs.PushBack((uint32)nfv.Size());
				}
			}
			BuildFromPolygons(pv.Data(), (uint32)pv.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data());
			ApplyVertSel(vsel);
			return true;
		}

		// DELETE : supprime les faces sélectionnées, compacte les sommets orphelins.
		bool NkEditMesh::DeleteSelectedFaces() {
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			ToPolygons(pv, fs, fv);
			const uint32 fc = (fs.Size() > 0) ? (uint32)fs.Size() - 1 : 0;
			NkVector<int32> remap;
			remap.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)remap.Size(); i++)
				remap[i] = -1;
			NkVector<NkVertex3D> nv2;
			NkVector<uint8> vsel;
			NkVector<uint32> nfs, nfv;
			nfs.PushBack(0);
			uint32 removed = 0;
			for (uint32 f = 0; f < fc; f++) {
				const uint32 s = fs[f], e = fs[f + 1];
				if (PolyFaceSelected(fv, s, e)) {
					removed++;
					continue;
				} // face supprimée
				for (uint32 k = s; k < e; k++) {
					uint32 vi = fv[k];
					if (remap[vi] < 0) {
						remap[vi] = (int32)nv2.Size();
						nv2.PushBack(pv[vi]);
						vsel.PushBack(verts[vi].sel);
					}
					nfv.PushBack((uint32)remap[vi]);
				}
				nfs.PushBack((uint32)nfv.Size());
			}
			if (removed == 0)
				return false;
			BuildFromPolygons(nv2.Data(), (uint32)nv2.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data());
			ApplyVertSel(vsel);
			return true;
		}

		// MERGE : soude les sommets sélectionnés en un (centroïde / premier / dernier),
		// retire les faces dégénérées (<3 sommets distincts), compacte.
		bool NkEditMesh::MergeSelectedVerts(const NkMergeParams &p) {
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			ToPolygons(pv, fs, fv);
			NkVec3f c{0.f, 0.f, 0.f};
			int32 n = 0, first = -1, last = -1;
			for (uint32 i = 0; i < (uint32)pv.Size(); i++)
				if (i < (uint32)verts.Size() && verts[i].sel) {
					c = c + pv[i].pos;
					n++;
					if (first < 0)
						first = (int32)i;
					last = (int32)i;
				}
			if (n < 2)
				return false;
			c = c * (1.f / (float32)n);
			const int32 rep = (p.mode == NkMergeParams::Last) ? last : first;
			NkVec3f target = (p.mode == NkMergeParams::First)  ? pv[(uint32)first].pos
							 : (p.mode == NkMergeParams::Last) ? pv[(uint32)last].pos
															   : c;
			pv[(uint32)rep].pos = target;
			NkVector<int32> map;
			map.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)map.Size(); i++)
				map[i] = (int32)i;
			for (uint32 i = 0; i < (uint32)pv.Size(); i++)
				if (i < (uint32)verts.Size() && verts[i].sel)
					map[i] = rep;
			const uint32 fc = (fs.Size() > 0) ? (uint32)fs.Size() - 1 : 0;
			NkVector<int32> remap;
			remap.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)remap.Size(); i++)
				remap[i] = -1;
			NkVector<NkVertex3D> nv2;
			NkVector<uint8> vsel;
			NkVector<uint32> nfs, nfv;
			nfs.PushBack(0);
			NkVector<uint32> loop;
			for (uint32 f = 0; f < fc; f++) {
				loop.Clear();
				for (uint32 k = fs[f]; k < fs[f + 1]; k++) {
					uint32 vi = (uint32)map[fv[k]];
					if (loop.Empty() || loop[loop.Size() - 1] != vi)
						loop.PushBack(vi);
				} // retire doublons consécutifs
				if (loop.Size() >= 2 && loop[0] == loop[loop.Size() - 1])
					loop.Resize((uint32)loop.Size() - 1);
				if (loop.Size() < 3)
					continue; // face dégénérée
				for (uint32 k = 0; k < (uint32)loop.Size(); k++) {
					uint32 vi = loop[k];
					if (remap[vi] < 0) {
						remap[vi] = (int32)nv2.Size();
						nv2.PushBack(pv[vi]);
						vsel.PushBack(vi < (uint32)verts.Size() ? verts[vi].sel : (uint8)0);
					}
					nfv.PushBack((uint32)remap[vi]);
				}
				nfs.PushBack((uint32)nfv.Size());
			}
			BuildFromPolygons(nv2.Data(), (uint32)nv2.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data());
			ApplyVertSel(vsel);
			return true;
		}

		// MAKE FACE : ajoute UNE face n-gon depuis les sommets sélectionnés (ordre d'index).
		bool NkEditMesh::MakeFaceFromSelected() {
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			ToPolygons(pv, fs, fv);
			NkVector<uint32> sel;
			for (uint32 i = 0; i < (uint32)verts.Size() && i < (uint32)pv.Size(); i++)
				if (verts[i].sel)
					sel.PushBack(i);
			if (sel.Size() < 3)
				return false;
			for (uint32 k = 0; k < (uint32)sel.Size(); k++)
				fv.PushBack(sel[k]);
			fs.PushBack((uint32)fv.Size());
			NkVector<uint8> keep;
			keep.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)keep.Size(); i++)
				keep[i] = (i < (uint32)verts.Size() ? verts[i].sel : (uint8)0);
			BuildFromPolygons(pv.Data(), (uint32)pv.Size(), fs.Data(), (uint32)fs.Size() - 1, fv.Data());
			ApplyVertSel(keep);
			return true;
		}

		// SUBDIVIDE (Catmull-Clark) : chaque face sélectionnée -> n sous-quads (centre de
		// face + milieux d'arête PARTAGÉS). p.cuts itère la passe. Rien de sélectionné => TOUT.
		bool NkEditMesh::SubdivideSelectedFaces(const NkSubdivideParams &p) {
			bool changed = false;
			const int32 cuts = (p.cuts < 1) ? 1 : p.cuts;
			for (int32 c = 0; c < cuts; c++) {
				if (SubdivideSelectedOnce())
					changed = true;
				else
					break;
			}
			return changed;
		}

		bool NkEditMesh::SubdivideSelectedOnce() {
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			ToPolygons(pv, fs, fv);
			const uint32 fc = (fs.Size() > 0) ? (uint32)fs.Size() - 1 : 0;
			if (fc == 0)
				return false;
			NkVector<uint8> faceSel;
			faceSel.Resize(fc);
			int32 selCount = 0;
			for (uint32 f = 0; f < fc; f++) {
				bool s = PolyFaceSelected(fv, fs[f], fs[f + 1]);
				faceSel[f] = s ? 1 : 0;
				if (s)
					selCount++;
			}
			if (selCount == 0) {
				for (uint32 f = 0; f < fc; f++)
					faceSel[f] = 1;
				selCount = (int32)fc;
			} // rien -> TOUT le modèle
			NkHashMap<uint64, uint32> emid;
			auto lerp = [&](uint32 a, uint32 b) {
				NkVertex3D r = pv[a];
				r.pos = (pv[a].pos + pv[b].pos) * 0.5f;
				r.uv = (pv[a].uv + pv[b].uv) * 0.5f;
				return r;
			};
			auto edgeMid = [&](uint32 a, uint32 b) -> uint32 {
				uint32 lo = a < b ? a : b, hi = a < b ? b : a;
				uint64 key = ((uint64)lo << 32) | hi;
				uint32 *q = emid.Find(key);
				if (q)
					return *q;
				uint32 idx = (uint32)pv.Size();
				pv.PushBack(lerp(a, b));
				emid.InsertOrAssign(key, idx);
				return idx;
			};
			NkVector<uint32> nfs, nfv;
			nfs.PushBack(0);
			NkVector<uint8> vsel;
			vsel.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)vsel.Size(); i++)
				vsel[i] = 0;
			for (uint32 f = 0; f < fc; f++) {
				if (faceSel[f])
					continue;
				for (uint32 k = fs[f]; k < fs[f + 1]; k++)
					nfv.PushBack(fv[k]);
				nfs.PushBack((uint32)nfv.Size());
			}
			for (uint32 f = 0; f < fc; f++) {
				if (!faceSel[f])
					continue;
				const uint32 s = fs[f], e = fs[f + 1], n = e - s;
				if (n < 3) {
					for (uint32 k = s; k < e; k++)
						nfv.PushBack(fv[k]);
					nfs.PushBack((uint32)nfv.Size());
					continue;
				}
				NkVertex3D ctr{};
				NkVec3f cp{0, 0, 0};
				NkVec2f cuv{0, 0};
				for (uint32 k = s; k < e; k++) {
					cp = cp + pv[fv[k]].pos;
					cuv = cuv + pv[fv[k]].uv;
				}
				ctr = pv[fv[s]];
				ctr.pos = cp * (1.f / (float32)n);
				ctr.uv = cuv * (1.f / (float32)n);
				uint32 cidx = (uint32)pv.Size();
				pv.PushBack(ctr);
				if ((uint32)vsel.Size() <= cidx)
					vsel.Resize(cidx + 1);
				for (uint32 k = 0; k < n; k++) {
					uint32 v0 = fv[s + k], v1 = fv[s + (k + 1) % n], vp = fv[s + (k + n - 1) % n];
					uint32 m1 = edgeMid(v0, v1), m0 = edgeMid(vp, v0);
					{
						uint32 mx = cidx;
						if (m1 > mx)
							mx = m1;
						if (m0 > mx)
							mx = m0;
						if ((uint32)vsel.Size() <= mx)
							vsel.Resize(mx + 1);
					}
					nfv.PushBack(v0);
					nfv.PushBack(m1);
					nfv.PushBack(cidx);
					nfv.PushBack(m0);
					nfs.PushBack((uint32)nfv.Size());
					vsel[cidx] = 1;
					vsel[m1] = 1;
					vsel[m0] = 1;
					if (v0 < (uint32)vsel.Size())
						vsel[v0] = 1;
				}
			}
			BuildFromPolygons(pv.Data(), (uint32)pv.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data());
			ApplyVertSel(vsel);
			return true;
		}

		// LOOP CUT : depuis une ARÊTE sélectionnée, traverse l'ANNEAU de quads et insère une
		// boucle d'arêtes aux MILIEUX partagés. Maillages quad fermés (façon Blender).
		bool NkEditMesh::LoopCutFromSelectedEdge() {
			// Arête de départ = 1re demi-arête vivante dont les 2 extrémités sont sélectionnées.
			NkEmId h0 = NK_EM_INVALID;
			for (uint32 h = 0; h < (uint32)hedges.Size(); ++h) {
				if (!hedges[h].alive)
					continue;
				uint32 o = hedges[h].origin, d = hedges[hedges[h].next].origin;
				if (o < (uint32)verts.Size() && d < (uint32)verts.Size() && verts[o].sel && verts[d].sel) {
					h0 = h;
					break;
				}
			}
			if (h0 == NK_EM_INVALID)
				return false;
			NkHashMap<uint64, uint8> ring;
			auto addE = [&](uint32 a, uint32 b) {
				uint32 lo = a < b ? a : b, hi = a < b ? b : a;
				ring.InsertOrAssign(((uint64)lo << 32) | hi, (uint8)1);
			};
			NkEmId h = h0;
			uint32 guard = 0;
			do {
				uint32 o = hedges[h].origin, d = hedges[hedges[h].next].origin;
				addE(o, d);
				if (FaceSize(hedges[h].face) != 4)
					break;								   // anneau uniquement à travers des quads
				NkEmId hOpp = hedges[hedges[h].next].next; // arête opposée du quad
				addE(hedges[hOpp].origin, hedges[hedges[hOpp].next].origin);
				NkEmId tw = hedges[hOpp].twin;
				if (tw == NK_EM_INVALID)
					break; // bord -> anneau ouvert
				h = tw;
			} while (h != h0 && ++guard < 100000u);
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			ToPolygons(pv, fs, fv);
			auto isRing = [&](uint32 a, uint32 b) -> bool {
				uint32 lo = a < b ? a : b, hi = a < b ? b : a;
				return ring.Find(((uint64)lo << 32) | hi) != nullptr;
			};
			NkHashMap<uint64, uint32> emid;
			auto edgeMid = [&](uint32 a, uint32 b) -> uint32 {
				uint32 lo = a < b ? a : b, hi = a < b ? b : a;
				uint64 key = ((uint64)lo << 32) | hi;
				uint32 *q = emid.Find(key);
				if (q)
					return *q;
				uint32 idx = (uint32)pv.Size();
				NkVertex3D nv = pv[a];
				nv.pos = (pv[a].pos + pv[b].pos) * 0.5f;
				nv.uv = (pv[a].uv + pv[b].uv) * 0.5f;
				pv.PushBack(nv);
				emid.InsertOrAssign(key, idx);
				return idx;
			};
			NkVector<uint32> nfs, nfv;
			nfs.PushBack(0);
			NkVector<uint8> vsel;
			vsel.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)vsel.Size(); i++)
				vsel[i] = 0;
			const uint32 fc = (fs.Size() > 0) ? (uint32)fs.Size() - 1 : 0;
			bool changed = false;
			for (uint32 f = 0; f < fc; f++) {
				const uint32 s = fs[f], e = fs[f + 1], n = e - s;
				int32 re0 = -1, re1 = -1;
				if (n == 4) {
					for (uint32 k = 0; k < 4; k++)
						if (isRing(fv[s + k], fv[s + (k + 1) % 4])) {
							if (re0 < 0)
								re0 = (int32)k;
							else
								re1 = (int32)k;
						}
				}
				if (n == 4 && re0 >= 0 && re1 >= 0 && (re1 - re0) == 2) { // 2 arêtes opposées
					uint32 k0 = (uint32)re0;
					uint32 q0 = fv[s + k0], q1 = fv[s + (k0 + 1) % 4], q2 = fv[s + (k0 + 2) % 4],
						   q3 = fv[s + (k0 + 3) % 4];
					uint32 m0 = edgeMid(q0, q1), m1 = edgeMid(q2, q3);
					uint32 mx = (m0 > m1 ? m0 : m1);
					if ((uint32)vsel.Size() <= mx)
						vsel.Resize(mx + 1);
					vsel[m0] = 1;
					vsel[m1] = 1;
					changed = true;
					nfv.PushBack(q0);
					nfv.PushBack(m0);
					nfv.PushBack(m1);
					nfv.PushBack(q3);
					nfs.PushBack((uint32)nfv.Size());
					nfv.PushBack(m0);
					nfv.PushBack(q1);
					nfv.PushBack(q2);
					nfv.PushBack(m1);
					nfs.PushBack((uint32)nfv.Size());
				} else {
					for (uint32 k = s; k < e; k++)
						nfv.PushBack(fv[k]);
					nfs.PushBack((uint32)nfv.Size());
				}
			}
			if (!changed)
				return false;
			BuildFromPolygons(pv.Data(), (uint32)pv.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data());
			ApplyVertSel(vsel);
			return true;
		}

		// BISECT / KNIFE : coupe le maillage par un PLAN. Chaque arête traversante reçoit un
		// sommet d'intersection (partagé) et chaque face traversée est coupée en 2. planePoint/
		// planeNormal dans l'espace de `xform` (modèle->monde éditeur, ou identité IA).
		bool NkEditMesh::BisectByPlane(const NkVec3f &pPoint, const NkVec3f &pNormal, const NkMat4f &xform) {
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			ToPolygons(pv, fs, fv);
			NkVector<float32> sd;
			sd.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)pv.Size(); i++) {
				NkVec3f w = xform * pv[i].pos;
				sd[i] = (w - pPoint).Dot(pNormal);
			}
			NkHashMap<uint64, uint32> cross;
			auto crossV = [&](uint32 a, uint32 b) -> int32 {
				if (sd[a] * sd[b] >= 0.f)
					return -1; // même côté (ou sur le plan)
				uint32 lo = a < b ? a : b, hi = a < b ? b : a;
				uint64 key = ((uint64)lo << 32) | hi;
				uint32 *q = cross.Find(key);
				if (q)
					return (int32)*q;
				float32 t = sd[a] / (sd[a] - sd[b]);
				NkVertex3D nv = pv[a];
				nv.pos = pv[a].pos + (pv[b].pos - pv[a].pos) * t;
				nv.uv = pv[a].uv + (pv[b].uv - pv[a].uv) * t;
				uint32 idx = (uint32)pv.Size();
				pv.PushBack(nv);
				cross.InsertOrAssign(key, idx);
				return (int32)idx;
			};
			NkVector<uint32> nfs, nfv;
			nfs.PushBack(0);
			NkVector<uint32> selCross;
			const uint32 fc = (fs.Size() > 0) ? (uint32)fs.Size() - 1 : 0;
			NkVector<uint32> loop;
			NkVector<uint32> cpos;
			bool changed = false;
			for (uint32 f = 0; f < fc; f++) {
				const uint32 s = fs[f], e = fs[f + 1], n = e - s;
				loop.Clear();
				cpos.Clear();
				for (uint32 k = 0; k < n; k++) {
					loop.PushBack(fv[s + k]);
					int32 cv = crossV(fv[s + k], fv[s + (k + 1) % n]);
					if (cv >= 0) {
						cpos.PushBack((uint32)loop.Size());
						loop.PushBack((uint32)cv);
						selCross.PushBack((uint32)cv);
					}
				}
				if (cpos.Size() == 2) { // face traversée -> 2 sous-faces
					uint32 c0 = cpos[0], c1 = cpos[1], L = (uint32)loop.Size();
					changed = true;
					for (uint32 i = c0; i <= c1; i++)
						nfv.PushBack(loop[i]);
					nfs.PushBack((uint32)nfv.Size());
					for (uint32 i = c1; i < L; i++)
						nfv.PushBack(loop[i]);
					for (uint32 i = 0; i <= c0; i++)
						nfv.PushBack(loop[i]);
					nfs.PushBack((uint32)nfv.Size());
				} else {
					for (uint32 i = 0; i < (uint32)loop.Size(); i++)
						nfv.PushBack(loop[i]);
					nfs.PushBack((uint32)nfv.Size());
				}
			}
			if (!changed)
				return false;
			BuildFromPolygons(pv.Data(), (uint32)pv.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data());
			NkVector<uint8> vsel;
			vsel.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)vsel.Size(); i++)
				vsel[i] = 0;
			for (uint32 i = 0; i < (uint32)selCross.Size(); i++)
				if (selCross[i] < (uint32)vsel.Size())
					vsel[selCross[i]] = 1;
			ApplyVertSel(vsel);
			return true;
		}

		// =====================================================================
		// HISTORIQUE UNDO/REDO (mémento : snapshots complets de NkEditMesh)
		// =====================================================================

		// Empile un snapshot en respectant le plafond (retire le plus ancien si dépassé).
		static void EM_PushCapped(NkVector<NkEditMesh> &stack, const NkEditMesh &m, uint32 limit) {
			stack.PushBack(m);
			while ((uint32)stack.Size() > limit) { // retire le plus ancien (décalage)
				for (uint32 i = 1; i < (uint32)stack.Size(); ++i)
					stack[i - 1] = stack[i];
				stack.Resize((uint32)stack.Size() - 1);
			}
		}

		void NkEditHistory::Clear() {
			mUndo.Clear();
			mRedo.Clear();
		}

		void NkEditHistory::Commit(const NkEditMesh &preState) {
			EM_PushCapped(mUndo, preState, mLimit);
			mRedo.Clear(); // nouvelle branche -> redo invalidé
		}

		bool NkEditHistory::Undo(NkEditMesh &mesh) {
			if (mUndo.Empty())
				return false;
			mRedo.PushBack(mesh);					// sauve l'état courant pour redo
			mesh = mUndo[(uint32)mUndo.Size() - 1]; // restaure le précédent
			mUndo.Resize((uint32)mUndo.Size() - 1);
			return true;
		}

		bool NkEditHistory::Redo(NkEditMesh &mesh) {
			if (mRedo.Empty())
				return false;
			mUndo.PushBack(mesh);
			mesh = mRedo[(uint32)mRedo.Size() - 1];
			mRedo.Resize((uint32)mRedo.Size() - 1);
			return true;
		}

		// =====================================================================
		// COMMANDE D'ÉDITION SÉRIALISABLE — pose la sélection puis dispatch l'op.
		// C'est ce qui rend la couche de commandes SCRIPTABLE (modificateurs + IA).
		// =====================================================================
		bool NkMeshEditCommand::Apply(NkEditMesh &m) const {
			// Rejoue la sélection enregistrée sur le maillage courant.
			for (uint32 i = 0; i < m.VertCount(); ++i)
				m.verts[i].sel = 0;
			for (uint32 k = 0; k < (uint32)selection.Size(); ++k) {
				const uint32 vi = selection[k];
				if (vi < m.VertCount())
					m.verts[vi].sel = 1;
			}
			switch (op) {
				case NkMeshEditOp::Extrude:
					return m.ExtrudeSelectedFaces(extrude);
				case NkMeshEditOp::Delete:
					return m.DeleteSelectedFaces();
				case NkMeshEditOp::Merge:
					return m.MergeSelectedVerts(merge);
				case NkMeshEditOp::MakeFace:
					return m.MakeFaceFromSelected();
				case NkMeshEditOp::Subdivide:
					return m.SubdivideSelectedFaces(subdiv);
				case NkMeshEditOp::LoopCut:
					return m.LoopCutFromSelectedEdge();
				case NkMeshEditOp::Bisect:
					return m.BisectByPlane(planePoint, planeNormal, bisectXform);
				case NkMeshEditOp::Move: {
					bool changed = false;
					for (uint32 k = 0; k < (uint32)selection.Size() && k < (uint32)moveDeltas.Size(); ++k) {
						const uint32 vi = selection[k];
						if (vi < m.VertCount()) {
							m.verts[vi].pos = m.verts[vi].pos + moveDeltas[k];
							changed = true;
						}
					}
					if (changed)
						m.RecomputeNormals();
					return changed;
				}
				default:
					return false;
			}
		}

		uint32 NkMeshEditRecorder::ReplayOnto(NkEditMesh &mesh) const {
			uint32 applied = 0;
			for (uint32 i = 0; i < (uint32)mCommands.Size(); ++i)
				if (mCommands[i].Apply(mesh))
					++applied;
			return applied;
		}

		// ── Sérialisation binaire (petit lecteur/écriveur d'octets, little-endian) ──
		namespace {
			struct EmW {
					NkVector<uint8> &b;

					void U8(uint8 v) {
						b.PushBack(v);
					}

					void U32(uint32 v) {
						b.PushBack((uint8)(v & 0xFF));
						b.PushBack((uint8)((v >> 8) & 0xFF));
						b.PushBack((uint8)((v >> 16) & 0xFF));
						b.PushBack((uint8)((v >> 24) & 0xFF));
					}

					void I32(int32 v) {
						U32((uint32)v);
					}

					void F32(float32 v) {
						union {
								float32 f;
								uint32 u;
						} c;

						c.f = v;
						U32(c.u);
					}
			};

			struct EmR {
					const uint8 *d;
					uint32 n;
					uint32 p;
					bool ok;

					EmR(const uint8 *dd, uint32 nn) : d(dd), n(nn), p(0), ok(true) {
					}

					uint8 U8() {
						if (p + 1 > n) {
							ok = false;
							return 0;
						}
						return d[p++];
					}

					uint32 U32() {
						if (p + 4 > n) {
							ok = false;
							return 0;
						}
						uint32 v = (uint32)d[p] | ((uint32)d[p + 1] << 8) | ((uint32)d[p + 2] << 16) |
								   ((uint32)d[p + 3] << 24);
						p += 4;
						return v;
					}

					int32 I32() {
						return (int32)U32();
					}

					float32 F32() {
						union {
								float32 f;
								uint32 u;
						} c;

						c.u = U32();
						return c.f;
					}
			};

			static const uint32 NK_EMREC_MAGIC = 0x4E4D4543u; // "NMEC"
		} // namespace

		void NkMeshEditRecorder::Serialize(NkVector<uint8> &out) const {
			out.Clear();
			EmW w{out};
			w.U32(NK_EMREC_MAGIC);
			w.U32(1u);
			w.U32((uint32)mCommands.Size());
			for (uint32 i = 0; i < (uint32)mCommands.Size(); ++i) {
				const NkMeshEditCommand &c = mCommands[i];
				w.U8((uint8)c.op);
				w.U32((uint32)c.selection.Size());
				for (uint32 k = 0; k < (uint32)c.selection.Size(); ++k)
					w.U32(c.selection[k]);
				w.U8((uint8)(c.extrude.individual ? 1 : 0));
				w.F32(c.extrude.offset);
				w.I32(c.merge.mode);
				w.I32(c.subdiv.cuts);
				w.F32(c.planePoint.x);
				w.F32(c.planePoint.y);
				w.F32(c.planePoint.z);
				w.F32(c.planeNormal.x);
				w.F32(c.planeNormal.y);
				w.F32(c.planeNormal.z);
				for (int32 col = 0; col < 4; ++col)
					for (int32 row = 0; row < 4; ++row)
						w.F32(c.bisectXform[col][row]);
				w.U32((uint32)c.moveDeltas.Size());
				for (uint32 k = 0; k < (uint32)c.moveDeltas.Size(); ++k) {
					w.F32(c.moveDeltas[k].x);
					w.F32(c.moveDeltas[k].y);
					w.F32(c.moveDeltas[k].z);
				}
			}
		}

		// =====================================================================
		// STACK DE MODIFICATEURS — Mirror / Array / Subsurf (non-destructif)
		// =====================================================================
		void NkMeshModifier::Apply(NkEditMesh &m) const {
			if (!enabled)
				return;
			if (type == NkModifierType::Subsurf) {
				for (uint32 i = 0; i < m.VertCount(); ++i)
					m.verts[i].sel = 0; // aucune sél. -> TOUT
				NkSubdivideParams p;
				p.cuts = (subsurfLevels < 1) ? 1 : subsurfLevels;
				m.SubdivideSelectedFaces(p);
				return;
			}
			// Mirror & Array travaillent en représentation polygones (CSR).
			NkVector<NkVertex3D> base;
			NkVector<uint32> fs, fv;
			m.ToPolygons(base, fs, fv);
			const uint32 baseVC = (uint32)base.Size();
			const uint32 fc = (fs.Size() > 0) ? (uint32)fs.Size() - 1 : 0;
			if (baseVC == 0 || fc == 0)
				return;

			if (type == NkModifierType::Mirror) {
				NkVector<NkVertex3D> pv = base; // sortie sommets (base + miroir)
				NkVector<int32> mir;
				mir.Resize(baseVC);
				for (uint32 i = 0; i < baseVC; i++) {
					const float32 co = (mirrorAxis == 0)   ? base[i].pos.x
									   : (mirrorAxis == 1) ? base[i].pos.y
														   : base[i].pos.z;
					const float32 aco = (co < 0.f) ? -co : co;
					if (mirrorMerge && aco <= mirrorMergeDist) {
						mir[i] = (int32)i;
					} // sur le plan -> soudé
					else {
						NkVertex3D v = base[i];
						if (mirrorAxis == 0) {
							v.pos.x = -v.pos.x;
							v.normal.x = -v.normal.x;
						} else if (mirrorAxis == 1) {
							v.pos.y = -v.pos.y;
							v.normal.y = -v.normal.y;
						} else {
							v.pos.z = -v.pos.z;
							v.normal.z = -v.normal.z;
						}
						mir[i] = (int32)pv.Size();
						pv.PushBack(v);
					}
				}
				NkVector<uint32> nfs, nfv;
				nfs.PushBack(0);
				for (uint32 f = 0; f < fc; f++) {
					for (uint32 k = fs[f]; k < fs[f + 1]; k++)
						nfv.PushBack(fv[k]);
					nfs.PushBack((uint32)nfv.Size());
				}
				for (uint32 f = 0; f < fc; f++) {
					const uint32 s = fs[f], e = fs[f + 1]; // faces miroir : winding inversé
					for (uint32 k = e; k > s; --k)
						nfv.PushBack((uint32)mir[fv[k - 1]]);
					nfs.PushBack((uint32)nfv.Size());
				}
				m.BuildFromPolygons(pv.Data(), (uint32)pv.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data());
				return;
			}

			// Array
			int32 cnt = (arrayCount < 1) ? 1 : arrayCount;
			if (cnt < 2)
				return;
			NkVector<NkVertex3D> pv = base;
			NkVector<uint32> nfs, nfv;
			nfs.PushBack(0);
			for (uint32 f = 0; f < fc; f++) {
				for (uint32 k = fs[f]; k < fs[f + 1]; k++)
					nfv.PushBack(fv[k]);
				nfs.PushBack((uint32)nfv.Size());
			}
			for (int32 c = 1; c < cnt; c++) {
				const uint32 voff = (uint32)pv.Size();
				for (uint32 i = 0; i < baseVC; i++) {
					NkVertex3D v = base[i];
					v.pos = v.pos + arrayOffset * (float32)c;
					pv.PushBack(v);
				}
				for (uint32 f = 0; f < fc; f++) {
					for (uint32 k = fs[f]; k < fs[f + 1]; k++)
						nfv.PushBack(fv[k] + voff);
					nfs.PushBack((uint32)nfv.Size());
				}
			}
			m.BuildFromPolygons(pv.Data(), (uint32)pv.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data());
		}

		void NkModifierStack::Evaluate(const NkEditMesh &base, NkEditMesh &out) const {
			out = base;
			for (uint32 i = 0; i < (uint32)modifiers.Size(); ++i)
				if (modifiers[i].enabled)
					modifiers[i].Apply(out);
		}

		bool NkMeshEditRecorder::Deserialize(const uint8 *data, uint32 size) {
			mCommands.Clear();
			EmR r(data, size);
			if (r.U32() != NK_EMREC_MAGIC)
				return false;
			(void)r.U32(); // version
			const uint32 count = r.U32();
			for (uint32 i = 0; i < count && r.ok; ++i) {
				NkMeshEditCommand c;
				c.op = (NkMeshEditOp)r.U8();
				const uint32 sc = r.U32();
				for (uint32 k = 0; k < sc && r.ok; ++k)
					c.selection.PushBack(r.U32());
				c.extrude.individual = (r.U8() != 0);
				c.extrude.offset = r.F32();
				c.merge.mode = r.I32();
				c.subdiv.cuts = r.I32();
				{
					float32 x = r.F32(), y = r.F32(), z = r.F32();
					c.planePoint = {x, y, z};
				}
				{
					float32 x = r.F32(), y = r.F32(), z = r.F32();
					c.planeNormal = {x, y, z};
				}
				for (int32 col = 0; col < 4; ++col)
					for (int32 row = 0; row < 4; ++row)
						c.bisectXform[col][row] = r.F32();
				const uint32 mc = r.U32();
				for (uint32 k = 0; k < mc && r.ok; ++k) {
					float32 x = r.F32(), y = r.F32(), z = r.F32();
					NkVec3f d = {x, y, z};
					c.moveDeltas.PushBack(d);
				}
				if (r.ok)
					mCommands.PushBack(c);
			}
			return r.ok;
		}

	} // namespace renderer
} // namespace nkentseu
