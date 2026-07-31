// -----------------------------------------------------------------------------
// @File    NkMeshAnalysis.cpp
// @Brief   Lecture structurelle d'un maillage. Voir l'en-tete pour le pourquoi.
// @Author  Rihen
// @License Proprietary - Free to use and modify
// -----------------------------------------------------------------------------

#include "NKRenderer/Mesh/NkMeshAnalysis.h"

#include "NKContainers/Associative/NkHashMap.h"

#include <cmath>

namespace nkentseu {
	namespace renderer {

		namespace {
			// Cle de cellule spatiale. Meme schema que BuildVertexMerge : 21 bits par
			// axe, ce qui borne la grille a 2^21 cellules par axe — largement au-dela
			// de tout maillage editable.
			inline uint64 CellKey(int64 x, int64 y, int64 z) {
				return ((uint64)(x & 0x1FFFFF)) | (((uint64)(y & 0x1FFFFF)) << 21) |
					   (((uint64)(z & 0x1FFFFF)) << 42);
			}
			inline int64 Quant(float32 v, float32 inv) {
				return (int64)(v * inv + (v >= 0.f ? 0.5f : -0.5f));
			}
		} // namespace

		NkMeshStats NkMeshAnalysis::Analyze(const NkEditMesh &src, float32 symEps) {
			NkMeshStats st;
			// COPIE puis RebuildEdges : l'analyse a besoin des cycles radial et
			// disque, que seule une reconstruction garantit a jour. Analyser un
			// maillage dont les aretes seraient perimees donnerait des chiffres faux
			// sans le dire — exactement ce qu'on veut eviter d'un module de mesure.
			// Le cout d'une copie est acceptable : on n'analyse pas par frame.
			NkEditMesh m = src;
			m.RebuildEdges();

			const uint32 nv = m.VertCount();
			st.rawVerts = nv;
			if (nv == 0)
				return st;

			NkVector<uint32> canon;
			m.BuildVertexMerge(canon);
			auto CN = [&](uint32 v) { return (v < (uint32)canon.Size()) ? canon[v] : v; };

			// ── Boite englobante et aire, sur les faces VIVANTES ──────────────
			st.bboxMin = st.bboxMax = m.verts[0].pos;
			for (uint32 i = 0; i < nv; ++i) {
				const NkVec3f &p = m.verts[i].pos;
				st.bboxMin.x = p.x < st.bboxMin.x ? p.x : st.bboxMin.x;
				st.bboxMin.y = p.y < st.bboxMin.y ? p.y : st.bboxMin.y;
				st.bboxMin.z = p.z < st.bboxMin.z ? p.z : st.bboxMin.z;
				st.bboxMax.x = p.x > st.bboxMax.x ? p.x : st.bboxMax.x;
				st.bboxMax.y = p.y > st.bboxMax.y ? p.y : st.bboxMax.y;
				st.bboxMax.z = p.z > st.bboxMax.z ? p.z : st.bboxMax.z;
			}
			const float32 diag = (st.bboxMax - st.bboxMin).Len();

			// ── Faces : comptage par nombre de cotes, aire, degenerescence ────
			NkVector<NkEmId> fv;
			const uint32 fcAll = (uint32)m.faces.Size();
			for (uint32 f = 0; f < fcAll; ++f) {
				if (!m.faces[f].alive)
					continue;
				fv.Clear();
				m.GetFaceVerts((NkEmId)f, fv);
				const uint32 n = (uint32)fv.Size();
				if (n < 3)
					continue;
				st.faces++;
				if (n == 3)
					st.tris++;
				else if (n == 4)
					st.quads++;
				else
					st.ngons++;
				// Aire par eventail depuis le premier sommet. Vaut pour un n-gon
				// convexe ; sur un n-gon concave elle sous-estime, ce qui est
				// acceptable pour une mesure de forme et documente ici plutot que
				// masque par une triangulation couteuse.
				float32 a = 0.f;
				for (uint32 k = 1; k + 1 < n; ++k) {
					const NkVec3f &p0 = m.verts[fv[0]].pos;
					const NkVec3f e1 = m.verts[fv[k]].pos - p0;
					const NkVec3f e2 = m.verts[fv[k + 1]].pos - p0;
					a += e1.Cross(e2).Len() * 0.5f;
				}
				st.area += a;
				if (a <= (diag > 0.f ? diag * diag * 1e-10f : 1e-12f))
					st.degenerateFaces++;
			}
			st.quadRatio = (st.faces > 0) ? (float32)st.quads / (float32)st.faces : 0.f;

			// ── Aretes : comptage, sante, longueurs ───────────────────────────
			float32 sum = 0.f, sum2 = 0.f;
			st.edgeMin = 1e30f;
			for (uint32 e = 0; e < (uint32)m.edges.Size(); ++e) {
				const auto &E = m.edges[e];
				if (!E.alive)
					continue;
				st.edges++;
				if (E.radialCount == 1)
					st.boundaryEdges++;
				else if (E.radialCount > 2)
					st.nonManifoldEdges++;
				if (E.v0 < nv && E.v1 < nv) {
					const float32 L = (m.verts[E.v1].pos - m.verts[E.v0].pos).Len();
					sum += L;
					sum2 += L * L;
					if (L < st.edgeMin)
						st.edgeMin = L;
					if (L > st.edgeMax)
						st.edgeMax = L;
				}
			}
			if (st.edges > 0) {
				st.edgeMean = sum / (float32)st.edges;
				const float32 var = sum2 / (float32)st.edges - st.edgeMean * st.edgeMean;
				const float32 sd = (var > 0.f) ? sqrtf(var) : 0.f;
				// Ecart-type RELATIF a la moyenne : une densite est homogene ou non,
				// independamment de l'echelle du modele. Un ecart absolu ne se
				// comparerait pas entre un personnage et une vis.
				st.edgeDeviation = (st.edgeMean > 1e-9f) ? sd / st.edgeMean : 0.f;
			} else {
				st.edgeMin = 0.f;
			}

			// ── Sommets soudes : valence (cycle DISQUE), poles, sommets isoles ─
			NkVector<uint8> isRep, onBoundary;
			isRep.Resize(nv);
			onBoundary.Resize(nv);
			for (uint32 i = 0; i < nv; ++i) {
				isRep[i] = 0;
				onBoundary[i] = 0;
			}
			for (uint32 i = 0; i < nv; ++i)
				isRep[CN(i)] = 1;
			for (uint32 e = 0; e < (uint32)m.edges.Size(); ++e) {
				const auto &E = m.edges[e];
				if (!E.alive || E.radialCount != 1)
					continue;
				if (E.v0 < nv)
					onBoundary[E.v0] = 1;
				if (E.v1 < nv)
					onBoundary[E.v1] = 1;
			}
			NkVector<NkEmId> disk;
			for (uint32 i = 0; i < nv; ++i) {
				if (!isRep[i])
					continue;
				st.verts++;
				const uint32 val = m.VertEdges(i, disk);
				if (val == 0) {
					// Sommet ISOLE : aucune arete. Invisible au rendu, mais il fausse
					// tout comptage et empeche certaines operations — d'ou son propre
					// compteur plutot qu'un silence.
					st.looseVerts++;
					continue;
				}
				if (onBoundary[i]) {
					// Sur un bord, la valence ne signifie pas la meme chose : un coin
					// de plan a naturellement 2 aretes sans etre un pole. Les compter
					// avec les autres gonflerait artificiellement le nombre de poles.
					st.poles.boundary++;
					continue;
				}
				if (val == 4)
					st.poles.regular++;
				else if (val == 3)
					st.poles.valence3++;
				else if (val == 5)
					st.poles.valence5++;
				else
					st.poles.valence6plus++;
			}

			// ── Caracteristique d'Euler et genre ──────────────────────────────
			st.euler = (int32)st.verts - (int32)st.edges + (int32)st.faces;
			// Le genre n'a de sens que sur un maillage FERME et MANIFOLD. Le calculer
			// autrement produirait un entier plausible et faux ; on rend -1.
			if (st.IsClosed() && st.IsManifold() && ((2 - st.euler) % 2) == 0)
				st.genus = (2 - st.euler) / 2;

			// ── Symetrie par axe ──────────────────────────────────────────────
			// Grille spatiale sur les sommets REPRESENTANTS : pour chaque sommet, on
			// cherche l'image miroir de sa position. Le ratio obtenu est une MESURE,
			// pas un verdict — la symetrie parfaite n'existe pas sur un modele
			// importe, et un seuil code ici imposerait un jugement a tous.
			{
				const float32 eps = (diag > 1e-9f) ? diag * symEps : 1e-4f;
				const float32 cell = (eps > 1e-9f) ? eps * 2.f : 1e-4f;
				const float32 inv = 1.f / cell;
				NkHashMap<uint64, int32> head;
				NkVector<int32> nextOf;
				nextOf.Resize(nv);
				for (uint32 i = 0; i < nv; ++i)
					nextOf[i] = -1;
				for (uint32 i = 0; i < nv; ++i) {
					if (!isRep[i])
						continue;
					const NkVec3f &p = m.verts[i].pos;
					const uint64 k = CellKey(Quant(p.x, inv), Quant(p.y, inv), Quant(p.z, inv));
					int32 *h = head.Find(k);
					if (h) {
						nextOf[i] = *h;
						*h = (int32)i;
					} else {
						head.InsertOrAssign(k, (int32)i);
					}
				}
				// Le plan de symetrie passe par le CENTRE de la boite englobante, non
				// par l'origine : un modele importe est rarement centre, et tester
				// autour de l'origine declarerait asymetrique un modele qui ne l'est
				// pas — erreur silencieuse classique.
				const NkVec3f c = (st.bboxMin + st.bboxMax) * 0.5f;
				for (int32 axis = 0; axis < 3; ++axis) {
					uint32 tested = 0, matched = 0;
					for (uint32 i = 0; i < nv; ++i) {
						if (!isRep[i])
							continue;
						tested++;
						NkVec3f q = m.verts[i].pos;
						if (axis == 0)
							q.x = 2.f * c.x - q.x;
						else if (axis == 1)
							q.y = 2.f * c.y - q.y;
						else
							q.z = 2.f * c.z - q.z;
						const int64 qx = Quant(q.x, inv), qy = Quant(q.y, inv), qz = Quant(q.z, inv);
						bool found = false;
						// 27 cellules voisines : un point peut tomber juste de l'autre
						// cote d'une frontiere de cellule. Ne regarder que la cellule
						// exacte manquerait ces cas et sous-estimerait la symetrie.
						for (int64 dx = -1; dx <= 1 && !found; ++dx)
							for (int64 dy = -1; dy <= 1 && !found; ++dy)
								for (int64 dz = -1; dz <= 1 && !found; ++dz) {
									const int32 *h = head.Find(CellKey(qx + dx, qy + dy, qz + dz));
									for (int32 j = h ? *h : -1; j >= 0; j = nextOf[(uint32)j]) {
										if ((m.verts[(uint32)j].pos - q).Len() <= eps) {
											found = true;
											break;
										}
									}
								}
						if (found)
							matched++;
					}
					const float32 r = (tested > 0) ? (float32)matched / (float32)tested : 0.f;
					if (axis == 0)
						st.symmetry.x = r;
					else if (axis == 1)
						st.symmetry.y = r;
					else
						st.symmetry.z = r;
				}
			}
			return st;
		}

	} // namespace renderer
} // namespace nkentseu
