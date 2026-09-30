// -----------------------------------------------------------------------------
// FICHIER: Kernel\Runtime\NKRenderer\src\NKRenderer\Mesh\NkCreatureMesures.cpp
// DESCRIPTION: Implementation des mesures du §6 (voir NkCreatureMesures.h).
// AUTEUR: TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// DATE: 2026-09-29
// VERSION: 0.1.0
// -----------------------------------------------------------------------------

// ============================================================
// INCLUDES
// ============================================================

// Header correspondant (TOUJOURS EN PREMIER)
#include "NKRenderer/Mesh/NkCreatureMesures.h"

// Nkentseu headers
#include "NKContainers/Associative/NkHashMap.h"

// Standard library
#include <cmath>
#include <cstdarg>
#include <cstdio>

// ============================================================
// ANONYMOUS NAMESPACE (helpers internes)
// ============================================================

namespace nkentseu {
	namespace renderer {
		namespace {

			void Dire(char *out, uint32 cap, const char *fmt, ...) {
				if (!out || cap == 0u) {
					return;
				}
				va_list args;
				va_start(args, fmt);
				std::vsnprintf(out, cap, fmt, args);
				va_end(args);
			}

			NkVec3f V3(float32 x, float32 y, float32 z) {
				NkVec3f v;
				v.x = x;
				v.y = y;
				v.z = z;
				return v;
			}

			NkVec3f Plus(const NkVec3f &a, const NkVec3f &b) {
				return V3(a.x + b.x, a.y + b.y, a.z + b.z);
			}

			NkVec3f Moins(const NkVec3f &a, const NkVec3f &b) {
				return V3(a.x - b.x, a.y - b.y, a.z - b.z);
			}

			NkVec3f Fois(const NkVec3f &a, float32 k) {
				return V3(a.x * k, a.y * k, a.z * k);
			}

			float32 Scal(const NkVec3f &a, const NkVec3f &b) {
				return a.x * b.x + a.y * b.y + a.z * b.z;
			}

			NkVec3f Vect(const NkVec3f &a, const NkVec3f &b) {
				return V3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
			}

			float32 Norme(const NkVec3f &a) {
				return std::sqrt(Scal(a, a));
			}

			NkVec3f Unitaire(const NkVec3f &a) {
				const float32 n = Norme(a);
				if (n < 1e-20f) {
					return V3(0.f, 0.f, 0.f);
				}
				return Fois(a, 1.f / n);
			}

			NkVec3f Tourner(const NkVec3f &v, const NkVec3f &axe, float32 angle) {
				const float32 c = std::cos(angle);
				const float32 s = std::sin(angle);
				const NkVec3f kxv = Vect(axe, v);
				const float32 kv = Scal(axe, v);
				return Plus(Plus(Fois(v, c), Fois(kxv, s)), Fois(axe, kv * (1.f - c)));
			}

			// ── QUATERNIONS DUAUX (Kavan, Collins, Zara, O'Sullivan 2007) ─────────
			// Un deplacement rigide (rotation r, translation t) = (r, 1/2 t r). Le
			// melange se fait sur les quaternions duaux, normalises par la partie
			// reelle : il ne peut pas « ecraser » un point vers l'axe comme le
			// melange lineaire des positions.
			struct NkQuat {
					float32 w = 1.f;
					float32 x = 0.f;
					float32 y = 0.f;
					float32 z = 0.f;
			};

			NkQuat QMul(const NkQuat &a, const NkQuat &b) {
				NkQuat r;
				r.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
				r.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
				r.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
				r.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
				return r;
			}

			struct NkQuatDual {
					NkQuat reel;
					NkQuat dual;
			};

			NkQuatDual Rigide(const NkQuat &r, const NkVec3f &t) {
				NkQuatDual d;
				d.reel = r;
				NkQuat tq;
				tq.w = 0.f;
				tq.x = t.x;
				tq.y = t.y;
				tq.z = t.z;
				const NkQuat p = QMul(tq, r);
				d.dual.w = 0.5f * p.w;
				d.dual.x = 0.5f * p.x;
				d.dual.y = 0.5f * p.y;
				d.dual.z = 0.5f * p.z;
				return d;
			}

			// Melange de deux deplacements rigides, poids (1 - w, w).
			NkVec3f AppliquerMelangeDual(const NkQuatDual &a, const NkQuatDual &b, float32 w, const NkVec3f &p) {
				float32 wb = w;
				// Antipodalite : q et -q sont la meme rotation ; on melange dans le
				// meme hemisphere, sinon le chemin fait le grand tour.
				const float32 dot = a.reel.w * b.reel.w + a.reel.x * b.reel.x + a.reel.y * b.reel.y + a.reel.z * b.reel.z;
				if (dot < 0.f) {
					wb = -w;
				}
				const float32 wa = 1.f - w;
				NkQuat r;
				r.w = wa * a.reel.w + wb * b.reel.w;
				r.x = wa * a.reel.x + wb * b.reel.x;
				r.y = wa * a.reel.y + wb * b.reel.y;
				r.z = wa * a.reel.z + wb * b.reel.z;
				NkQuat d;
				d.w = wa * a.dual.w + wb * b.dual.w;
				d.x = wa * a.dual.x + wb * b.dual.x;
				d.y = wa * a.dual.y + wb * b.dual.y;
				d.z = wa * a.dual.z + wb * b.dual.z;
				const float32 n = std::sqrt(r.w * r.w + r.x * r.x + r.y * r.y + r.z * r.z);
				r.w /= n;
				r.x /= n;
				r.y /= n;
				r.z /= n;
				d.w /= n;
				d.x /= n;
				d.y /= n;
				d.z /= n;
				// Rotation de p par r, puis translation 2 (d r*).vec.
				const NkVec3f u = V3(r.x, r.y, r.z);
				const NkVec3f uxp = Vect(u, p);
				const NkVec3f rp = Plus(p, Plus(Fois(uxp, 2.f * r.w), Fois(Vect(u, uxp), 2.f)));
				NkQuat rc;
				rc.w = r.w;
				rc.x = -r.x;
				rc.y = -r.y;
				rc.z = -r.z;
				const NkQuat t = QMul(d, rc);
				return Plus(rp, V3(2.f * t.x, 2.f * t.y, 2.f * t.z));
			}

			// Degre de chaque sommet (identite SOUDEE : v0/v1 des aretes le sont deja).
			void Degres(const NkEditMesh &m, NkVector<uint32> &deg) {
				deg.Resize(m.VertCount());
				for (uint32 i = 0u; i < m.VertCount(); ++i) {
					deg[i] = 0u;
				}
				for (uint32 k = 0u; k < (uint32)m.edges.Size(); ++k) {
					const NkEditMesh::Edge &e = m.edges[k];
					if (!e.alive || e.v0 >= m.VertCount() || e.v1 >= m.VertCount()) {
						continue;
					}
					deg[e.v0] += 1u;
					deg[e.v1] += 1u;
				}
			}

			uint32 Racine(NkVector<uint32> &p, uint32 x) {
				while (p[x] != x) {
					p[x] = p[p[x]];
					x = p[x];
				}
				return x;
			}

			// ── AUTO-INTERSECTIONS ────────────────────────────────────────────
			// Segment [a, b] contre triangle (p, q, r), Moller-Trumbore borne.
			bool SegmentCoupe(const NkVec3f &a, const NkVec3f &b, const NkVec3f &p, const NkVec3f &q, const NkVec3f &r) {
				const NkVec3f d = Moins(b, a);
				const NkVec3f e1 = Moins(q, p);
				const NkVec3f e2 = Moins(r, p);
				const NkVec3f h = Vect(d, e2);
				const float32 det = Scal(e1, h);
				// ⚠️ SEUIL RELATIF, PAS ABSOLU : det = (d x e2) . e1 est un volume. Sur le
				//    flanc plat d'une superellipse, deux faces non voisines sont presque
				//    COPLANAIRES ; le seuil absolu 1e-14 laissait passer un det minuscule
				//    et le test rendait une « traversee » numerique (mesure du 29/09 :
				//    2 faux positifs sur un pied droit). Le cas coplanaire est une limite
				//    DECLAREE de ce test (voir l'en-tete).
				const float32 echelle = Norme(d) * Norme(e1) * Norme(e2);
				if (std::fabs(det) <= 1e-4f * echelle) {
					return false;
				}
				const float32 f = 1.f / det;
				const NkVec3f s = Moins(a, p);
				const float32 u = f * Scal(s, h);
				if (u < 1e-6f || u > 1.f - 1e-6f) {
					return false;
				}
				const NkVec3f qq = Vect(s, e1);
				const float32 v = f * Scal(d, qq);
				if (v < 1e-6f || u + v > 1.f - 1e-6f) {
					return false;
				}
				const float32 t = f * Scal(e2, qq);
				return t > 1e-6f && t < 1.f - 1e-6f;
			}

			struct NkMesureTri {
					uint32 v[3];
					uint32 face = 0u;
			};

			uint32 AutoIntersections(const NkEditMesh &m, uint32 *premiere) {
				NkVector<NkMesureTri> tris;
				NkVector<NkEmId> fv;
				// Les sommets de chaque FACE : deux triangles dont les faces se touchent
				// ne peuvent pas « se traverser ».
				// ⚠️ LA PREMIERE VERSION COMPARAIT LES SOMMETS DES TRIANGLES. Or un quad
				//    coupe en deux laisse un triangle SANS le coin qu'il partage avec son
				//    voisin : leurs aretes se touchaient en ce coin, et le test a 1e-6 le
				//    comptait comme une traversee. Mesure : 4 « auto-intersections » sur un
				//    os droit, entre faces VOISINES (diagnostic du banc, 29/09).
				NkVector<uint32> debutFace;
				NkVector<uint32> sommetsFace;
				debutFace.Resize(m.FaceCount() + 1u);
				float32 longueur = 0.f;
				uint32 nbAretes = 0u;
				for (uint32 f = 0u; f < m.FaceCount(); ++f) {
					debutFace[f] = (uint32)sommetsFace.Size();
					if (!m.faces[f].alive) {
						continue;
					}
					m.GetFaceVerts((NkEmId)f, fv);
					for (uint32 k = 0u; k < (uint32)fv.Size(); ++k) {
						sommetsFace.PushBack(fv[k]);
					}
					for (uint32 k = 1u; k + 1u < (uint32)fv.Size(); ++k) {
						NkMesureTri t;
						t.v[0] = fv[0];
						t.v[1] = fv[k];
						t.v[2] = fv[k + 1u];
						t.face = f;
						tris.PushBack(t);
					}
					for (uint32 k = 0u; k < (uint32)fv.Size(); ++k) {
						longueur += Norme(Moins(m.verts[fv[k]].pos, m.verts[fv[(k + 1u) % fv.Size()]].pos));
						++nbAretes;
					}
				}
				debutFace[m.FaceCount()] = (uint32)sommetsFace.Size();
				if (tris.Empty() || nbAretes == 0u) {
					return 0u;
				}
				const float32 cellule = 2.f * longueur / (float32)nbAretes;
				// Hachage spatial : chaque triangle dans les cellules de sa boite.
				NkHashMap<uint64, NkVector<uint32>> grille;
				auto cle = [](int32 x, int32 y, int32 z) -> uint64 {
					return ((uint64)(uint32)(x + 1048576) << 42) ^ ((uint64)(uint32)(y + 1048576) << 21) ^
						   (uint64)(uint32)(z + 1048576);
				};
				auto borne = [&](float32 c) -> int32 {
					return (int32)std::floor(c / cellule);
				};
				for (uint32 i = 0u; i < (uint32)tris.Size(); ++i) {
					NkVec3f lo = m.verts[tris[i].v[0]].pos;
					NkVec3f hi = lo;
					for (uint32 k = 1u; k < 3u; ++k) {
						const NkVec3f p = m.verts[tris[i].v[k]].pos;
						lo = V3(p.x < lo.x ? p.x : lo.x, p.y < lo.y ? p.y : lo.y, p.z < lo.z ? p.z : lo.z);
						hi = V3(p.x > hi.x ? p.x : hi.x, p.y > hi.y ? p.y : hi.y, p.z > hi.z ? p.z : hi.z);
					}
					for (int32 x = borne(lo.x); x <= borne(hi.x); ++x) {
						for (int32 y = borne(lo.y); y <= borne(hi.y); ++y) {
							for (int32 z = borne(lo.z); z <= borne(hi.z); ++z) {
								const uint64 c = cle(x, y, z);
								NkVector<uint32> *case_ = grille.Find(c);
								if (!case_) {
									grille.InsertOrAssign(c, NkVector<uint32>());
									case_ = grille.Find(c);
								}
								case_->PushBack(i);
							}
						}
					}
				}
				// Chaque paire UNE fois (marquage de la derniere paire vue par triangle
				// serait faux : on compte les paires distinctes par un ensemble).
				NkHashMap<uint64, uint8> vues;
				uint32 n = 0u;
				for (auto it = grille.begin(); it != grille.end(); ++it) {
					const NkVector<uint32> &liste = it->Second;
					for (uint32 a = 0u; a < (uint32)liste.Size(); ++a) {
						for (uint32 b = a + 1u; b < (uint32)liste.Size(); ++b) {
							const uint32 i = liste[a] < liste[b] ? liste[a] : liste[b];
							const uint32 j = liste[a] < liste[b] ? liste[b] : liste[a];
							const NkMesureTri &ti = tris[i];
							const NkMesureTri &tj = tris[j];
							bool voisins = ti.face == tj.face;
							for (uint32 p = debutFace[ti.face]; p < debutFace[ti.face + 1u] && !voisins; ++p) {
								for (uint32 q = debutFace[tj.face]; q < debutFace[tj.face + 1u]; ++q) {
									voisins = voisins || sommetsFace[p] == sommetsFace[q];
								}
							}
							if (voisins) {
								continue;
							}
							const uint64 k = ((uint64)i << 32) | (uint64)j;
							if (vues.Find(k)) {
								continue;
							}
							vues.InsertOrAssign(k, (uint8)1);
							NkVec3f a0 = m.verts[ti.v[0]].pos;
							NkVec3f a1 = m.verts[ti.v[1]].pos;
							NkVec3f a2 = m.verts[ti.v[2]].pos;
							NkVec3f b0 = m.verts[tj.v[0]].pos;
							NkVec3f b1 = m.verts[tj.v[1]].pos;
							NkVec3f b2 = m.verts[tj.v[2]].pos;
							const bool coupe = SegmentCoupe(a0, a1, b0, b1, b2) || SegmentCoupe(a1, a2, b0, b1, b2) ||
											   SegmentCoupe(a2, a0, b0, b1, b2) || SegmentCoupe(b0, b1, a0, a1, a2) ||
											   SegmentCoupe(b1, b2, a0, a1, a2) || SegmentCoupe(b2, b0, a0, a1, a2);
							if (coupe) {
								if (n == 0u && premiere) {
									premiere[0] = ti.face;
									premiere[1] = tj.face;
								}
								++n;
							}
						}
					}
				}
				return n;
			}

			float32 VolumeCone(const NkEditMesh &m, const NkVector<NkVec3f> &pos, const NkVec3f &o,
							   const NkVector<uint8> *garder) {
				float64 v = 0.0;
				NkVector<NkEmId> fv;
				for (uint32 f = 0u; f < m.FaceCount(); ++f) {
					if (!m.faces[f].alive || (garder && !(*garder)[f])) {
						continue;
					}
					m.GetFaceVerts((NkEmId)f, fv);
					for (uint32 k = 1u; k + 1u < (uint32)fv.Size(); ++k) {
						const NkVec3f a = Moins(pos[fv[0]], o);
						const NkVec3f b = Moins(pos[fv[k]], o);
						const NkVec3f c = Moins(pos[fv[k + 1u]], o);
						v += (float64)Scal(a, Vect(b, c)) / 6.0;
					}
				}
				return (float32)v;
			}

			NkVec3f NormaleFace(const NkEditMesh &m, const NkVector<NkVec3f> &pos, uint32 f) {
				NkVector<NkEmId> fv;
				m.GetFaceVerts((NkEmId)f, fv);
				NkVec3f n = V3(0.f, 0.f, 0.f);
				for (uint32 k = 1u; k + 1u < (uint32)fv.Size(); ++k) {
					n = Plus(n, Vect(Moins(pos[fv[k]], pos[fv[0]]), Moins(pos[fv[k + 1u]], pos[fv[0]])));
				}
				return n;
			}

		}  // namespace

		// ============================================================
		// IMPLEMENTATIONS
		// ============================================================

		bool NkCreatureMesurer(const NkR32Document &doc, const NkR32Peau &peau, NkCreatureMesure &r, char *pourquoi,
							   uint32 cap, NkCreaturePoids poids) {
			r = NkCreatureMesure{};
			const NkEditMesh &m = peau.maillage;
			if ((uint32)peau.sommets.Size() != m.VertCount()) {
				Dire(pourquoi, cap, "la peau ne porte pas l'adresse de ses sommets (generateur 2 attendu)");
				return false;
			}
			NkVector<NkEmId> fv;

			// ── 1. faces et variete ──────────────────────────────────────────
			for (uint32 f = 0u; f < m.FaceCount(); ++f) {
				if (!m.faces[f].alive) {
					continue;
				}
				++r.faces;
				if (m.FaceSize((NkEmId)f) == 4u) {
					++r.quads;
				}
			}
			r.partQuads = r.faces ? (float32)r.quads / (float32)r.faces : 0.f;
			uint32 nbAretes = 0u;
			for (uint32 k = 0u; k < (uint32)m.edges.Size(); ++k) {
				const NkEditMesh::Edge &e = m.edges[k];
				if (!e.alive) {
					continue;
				}
				++nbAretes;
				if (e.radialCount > 2u) {
					++r.aretesNonManifold;
				}
				if (e.radialCount == 1u) {
					++r.aretesBord;
				}
			}
			NkVector<uint32> deg;
			Degres(m, deg);
			uint32 nbSommets = 0u;
			NkVector<uint32> parent;
			parent.Resize(m.VertCount());
			for (uint32 i = 0u; i < m.VertCount(); ++i) {
				parent[i] = i;
				if (deg[i] > 0u) {
					++nbSommets;
				}
			}
			for (uint32 k = 0u; k < (uint32)m.edges.Size(); ++k) {
				const NkEditMesh::Edge &e = m.edges[k];
				if (e.alive && e.v0 < m.VertCount() && e.v1 < m.VertCount()) {
					parent[Racine(parent, e.v0)] = Racine(parent, e.v1);
				}
			}
			for (uint32 i = 0u; i < m.VertCount(); ++i) {
				if (deg[i] > 0u && Racine(parent, i) == i) {
					++r.composantes;
				}
			}
			r.euler = (int32)nbSommets - (int32)nbAretes + (int32)r.faces;
			if (r.aretesBord == 0u && r.aretesNonManifold == 0u) {
				r.genre = (2 * (int32)r.composantes - r.euler) / 2;
			}
			r.sommeAttendue = 4 * r.euler;  // quads fermes : Σ(4 - val) = 4 chi = Σ (8 - 8 g)
			{
				uint32 premiere[2] = {0u, 0u};
				r.autoIntersections = AutoIntersections(m, premiere);
				if (r.autoIntersections > 0u) {
					for (uint32 k = 0u; k < 2u; ++k) {
						const NkR32AdresseFace &a = peau.adresses[premiere[k]];
						std::snprintf(r.intersectionOs[k], NK_R32_NOM_MAX, "%s", doc.os[a.os].nom);
						r.intersectionLieu[k] = (uint32)a.lieu;
						r.intersectionS[k][0] = a.s0;
						r.intersectionS[k][1] = a.s1;
						r.intersectionA[k] = a.a0;
					}
				}
			}

			// ── 2. valence sur la cage subdivisee une fois ───────────────────
			{
				NkEditMesh s = m;
				s.SubdivideCatmullClark(1);
				NkVector<uint32> ds;
				Degres(s, ds);
				for (uint32 i = 0u; i < s.VertCount(); ++i) {
					if (ds[i] == 0u) {
						continue;
					}
					++r.sommetsSubdivises;
					if (ds[i] == 4u) {
						++r.valence4;
					}
					r.sommeEcarts += 4 - (int32)ds[i];
				}
				r.partValence4 = r.sommetsSubdivises ? (float32)r.valence4 / (float32)r.sommetsSubdivises : 0.f;
			}

			// ── 3. chaines, boucles, poles ───────────────────────────────────
			const uint32 nOs = (uint32)doc.os.Size();
			NkVector<int32> parentOs;
			parentOs.Resize(nOs);
			for (uint32 i = 0u; i < nOs; ++i) {
				parentOs[i] = doc.Trouver(doc.os[i].parent);
			}
			// Les boucles de BOUT d'un os : celle d'arrivee (la plus petite s) et
			// celle de depart (la plus grande s avant le joint). L'arc de conge met
			// des anneaux ordinaires entre elles et le joint : on ne se fie plus a
			// « s < 0,5 ».
			NkVector<float32> boucleArrivee;
			NkVector<float32> boucleDepart;
			boucleArrivee.Resize(nOs);
			boucleDepart.Resize(nOs);
			for (uint32 i = 0u; i < nOs; ++i) {
				boucleArrivee[i] = 2.f;
				boucleDepart[i] = -1.f;
			}
			for (uint32 v = 0u; v < (uint32)peau.sommets.Size(); ++v) {
				const NkR32AdresseSommet &a = peau.sommets[v];
				if (!a.estBoucle || a.lieu != NkR32Lieu::Nk_R32Lieu_Tube || a.s >= 1.f - 1e-6f) {
					continue;
				}
				boucleArrivee[a.os] = a.s < boucleArrivee[a.os] ? a.s : boucleArrivee[a.os];
				boucleDepart[a.os] = a.s > boucleDepart[a.os] ? a.s : boucleDepart[a.os];
			}
			for (uint32 c = 0u; c < nOs; ++c) {
				if (parentOs[c] < 0) {
					continue;
				}
				const uint32 p = (uint32)parentOs[c];
				++r.articulations;
				NkVector<int32> anneaux;
				for (uint32 v = 0u; v < (uint32)peau.sommets.Size(); ++v) {
					const NkR32AdresseSommet &a = peau.sommets[v];
					if (!a.estBoucle) {
						continue;
					}
					const bool dansBande = (a.os == p && a.s >= boucleDepart[p] - 1e-6f) || (a.os == c && a.s <= boucleArrivee[c] + 1e-6f);
					if (!dansBande) {
						continue;
					}
					bool deja = false;
					for (uint32 k = 0u; k < (uint32)anneaux.Size(); ++k) {
						deja = deja || anneaux[k] == a.anneau;
					}
					if (!deja) {
						anneaux.PushBack(a.anneau);
					}
				}
				const uint32 n = (uint32)anneaux.Size();
				if (r.articulations == 1u || n < r.bouclesMin) {
					r.bouclesMin = n;
				}
				if (n > r.bouclesMax) {
					r.bouclesMax = n;
				}
			}
			r.distancePoleMin = 1 << 20;
			for (uint32 v = 0u; v < m.VertCount(); ++v) {
				if (deg[v] == 0u || deg[v] == 4u) {
					continue;
				}
				++r.poles;
				const NkR32AdresseSommet &a = peau.sommets[v];
				int32 d = 1 << 20;
				for (uint32 w = 0u; w < (uint32)peau.sommets.Size(); ++w) {
					const NkR32AdresseSommet &b = peau.sommets[w];
					if (b.estBoucle && b.chaine == a.chaine) {
						const int32 e = (a.anneau > b.anneau) ? a.anneau - b.anneau : b.anneau - a.anneau;
						d = e < d ? e : d;
					}
				}
				if (d < r.distancePoleMin) {
					r.distancePoleMin = d;
				}
				if (d < 2) {
					++r.polesPresDesPlis;
				}
			}

			// ── 4. symetrie : chaque sommet a-t-il son miroir (x -> -x) ? ─────
			{
				NkHashMap<uint64, NkVector<uint32>> grille;
				const float32 cellule = 1e-3f;
				// ⚠️ LES VOISINS SE SONDENT EN INDICES ENTIERS, JAMAIS EN AJOUTANT
				//    `cellule` A LA COORDONNEE. La premiere version faisait
				//    1,39999998 + 0,001 = 1,40100002 : la cellule 1400, ou etait le
				//    miroir, etait SAUTEE (1399 -> 1401). Le banc a vu « ecart 1 » sur un
				//    bras parfaitement symetrique -- la force brute trouvait 1,2e-7.
				auto indice = [&](float32 c) -> int64 {
					return (int64)std::floor(c / cellule);
				};
				auto cle = [](int64 x, int64 y, int64 z) -> uint64 {
					return ((uint64)(x & 0x1FFFFF) << 42) ^ ((uint64)(y & 0x1FFFFF) << 21) ^ (uint64)(z & 0x1FFFFF);
				};
				for (uint32 v = 0u; v < m.VertCount(); ++v) {
					const NkVec3f p = m.verts[v].pos;
					const uint64 k = cle(indice(p.x), indice(p.y), indice(p.z));
					NkVector<uint32> *c = grille.Find(k);
					if (!c) {
						grille.InsertOrAssign(k, NkVector<uint32>());
						c = grille.Find(k);
					}
					c->PushBack(v);
				}
				for (uint32 v = 0u; v < m.VertCount(); ++v) {
					const NkVec3f q = V3(-m.verts[v].pos.x, m.verts[v].pos.y, m.verts[v].pos.z);
					const int64 ix = indice(q.x);
					const int64 iy = indice(q.y);
					const int64 iz = indice(q.z);
					float32 meilleur = 1.f;
					for (int64 dx = -1; dx <= 1; ++dx) {
						for (int64 dy = -1; dy <= 1; ++dy) {
							for (int64 dz = -1; dz <= 1; ++dz) {
								const NkVector<uint32> *c = grille.Find(cle(ix + dx, iy + dy, iz + dz));
								if (!c) {
									continue;
								}
								for (uint32 k = 0u; k < (uint32)c->Size(); ++k) {
									const float32 d = Norme(Moins(m.verts[(*c)[k]].pos, q));
									meilleur = d < meilleur ? d : meilleur;
								}
							}
						}
					}
					r.ecartSymetrie = meilleur > r.ecartSymetrie ? meilleur : r.ecartSymetrie;
				}
			}

			// ── 5. regularite ────────────────────────────────────────────────
			for (uint32 f = 0u; f < m.FaceCount(); ++f) {
				if (!m.faces[f].alive) {
					continue;
				}
				m.GetFaceVerts((NkEmId)f, fv);
				const uint32 n = (uint32)fv.Size();
				float32 lmin = 1e30f;
				float32 lmax = 0.f;
				bool anglesBons = true;
				for (uint32 k = 0u; k < n; ++k) {
					const NkVec3f a = m.verts[fv[(k + n - 1u) % n]].pos;
					const NkVec3f b = m.verts[fv[k]].pos;
					const NkVec3f c = m.verts[fv[(k + 1u) % n]].pos;
					const float32 l = Norme(Moins(c, b));
					lmin = l < lmin ? l : lmin;
					lmax = l > lmax ? l : lmax;
					const NkVec3f u = Unitaire(Moins(a, b));
					const NkVec3f w = Unitaire(Moins(c, b));
					float32 cosA = Scal(u, w);
					cosA = cosA > 1.f ? 1.f : (cosA < -1.f ? -1.f : cosA);
					const float32 deg = std::acos(cosA) * 180.f / 3.14159265f;
					if (deg < 45.f || deg > 135.f) {
						anglesBons = false;
					}
				}
				const float32 rapport = lmin > 1e-12f ? lmax / lmin : 1e30f;
				if (rapport > r.rapportCotesMax) {
					r.rapportCotesMax = rapport;
					const NkR32AdresseFace &a = peau.adresses[f];
					std::snprintf(r.rapportOs, sizeof(r.rapportOs), "%s", doc.os[a.os].nom);
					r.rapportLieu = (uint32)a.lieu;
				}
				if (rapport <= 3.f && anglesBons) {
					++r.facesRegulieres;
				}
			}
			r.partReguliere = r.faces ? (float32)r.facesRegulieres / (float32)r.faces : 0.f;

			// ── 6. banc de pose : chaque articulation pliee a 90 degres ──────
			NkVector<NkR32Repere> reps;
			if (!NkR32CalculerReperes(doc, reps, pourquoi, cap)) {
				return false;
			}
			// Rang de chaque os dans sa chaine.
			NkVector<int32> rang;
			rang.Resize(nOs);
			for (uint32 i = 0u; i < nOs; ++i) {
				int32 k = 0;
				for (int32 p = parentOs[i]; p >= 0; p = parentOs[(uint32)p]) {
					++k;
				}
				rang[i] = k;
			}
			NkVector<NkVec3f> repos;
			NkVector<NkVec3f> plie;
			repos.Resize(m.VertCount());
			plie.Resize(m.VertCount());
			NkVector<float32> w;
			w.Resize(m.VertCount());
			for (uint32 v = 0u; v < m.VertCount(); ++v) {
				repos[v] = m.verts[v].pos;
			}
			const float32 volumeRepos = VolumeCone(m, repos, V3(0.f, 0.f, 0.f), nullptr);
			NkVector<NkVec3f> lineaire;
			lineaire.Resize(m.VertCount());
			for (uint32 c = 0u; c < nOs; ++c) {
				if (parentOs[c] < 0) {
					continue;
				}
				const uint32 p = (uint32)parentOs[c];
				const NkR32Repere &rp = reps[p];
				const NkVec3f J = Plus(rp.racine, Fois(rp.axe, rp.longueur));
				const NkVec3f axe = rp.e2;
				const float32 angle = 0.5f * 3.14159265f;
				// Les deux deplacements : le parent immobile, l'enfant tourne autour de J.
				NkQuat qr;
				qr.w = std::cos(0.5f * angle);
				qr.x = axe.x * std::sin(0.5f * angle);
				qr.y = axe.y * std::sin(0.5f * angle);
				qr.z = axe.z * std::sin(0.5f * angle);
				const NkVec3f RJ = Plus(J, Tourner(Moins(V3(0.f, 0.f, 0.f), J), axe, angle));  // R(0 - J) + J
				const NkQuatDual dParent = Rigide(NkQuat{}, V3(0.f, 0.f, 0.f));
				const NkQuatDual dEnfant = Rigide(qr, RJ);
				uint16 chaine = 0u;
				bool trouvee = false;
				for (uint32 v = 0u; v < (uint32)peau.sommets.Size() && !trouvee; ++v) {
					if (peau.sommets[v].os == c) {
						chaine = peau.sommets[v].chaine;
						trouvee = true;
					}
				}
				for (uint32 v = 0u; v < m.VertCount(); ++v) {
					const NkR32AdresseSommet &a = peau.sommets[v];
					float32 poidsV = 0.f;
					if (a.chaine == chaine) {
						if (rang[a.os] > rang[c]) {
							poidsV = 1.f;
						} else if (a.os == c) {
							// Enfant : 0,5 au joint -> 0,75 a sa boucle d'arrivee, lineaire
							// en s le long de l'arc ; 1 au-dela.
							poidsV = 1.f;
							const float32 sA = boucleArrivee[c];
							if (poids == NkCreaturePoids::Nk_CreaturePoids_Construction && sA <= 1.f && a.s <= sA + 1e-6f) {
								poidsV = 0.5f + 0.25f * (sA > 0.f ? a.s / sA : 1.f);
							}
						} else if (a.os == p && poids == NkCreaturePoids::Nk_CreaturePoids_Construction) {
							// Parent : 0,25 a sa boucle de depart -> 0,5 au joint ; 0 avant.
							const float32 sD = boucleDepart[p];
							if (sD >= 0.f && a.lieu == NkR32Lieu::Nk_R32Lieu_Tube && a.s >= sD - 1e-6f) {
								poidsV = 0.25f + 0.25f * ((1.f - sD) > 1e-6f ? (a.s - sD) / (1.f - sD) : 1.f);
							}
						}
					}
					w[v] = poidsV;
					const NkVec3f tourne = Plus(J, Tourner(Moins(repos[v], J), axe, angle));
					lineaire[v] = Plus(Fois(repos[v], 1.f - poidsV), Fois(tourne, poidsV));
					plie[v] = AppliquerMelangeDual(dParent, dEnfant, poidsV, repos[v]);
				}
				NkVector<uint8> bande;
				bande.Resize(m.FaceCount());
				for (uint32 f = 0u; f < m.FaceCount(); ++f) {
					const uint32 os = peau.adresses[f].os;
					bande[f] = (os == p || os == c) ? 1u : 0u;
				}
				const float32 localRepos = VolumeCone(m, repos, J, &bande);
				for (uint32 mode = 0u; mode < 2u; ++mode) {
					const NkVector<NkVec3f> &pos = (mode == 0u) ? plie : lineaire;
					const float32 localPlie = VolumeCone(m, pos, J, &bande);
					const float32 perteLocale = localRepos != 0.f ? (localRepos - localPlie) / localRepos : 0.f;
					uint32 retournees = 0u;
					for (uint32 f = 0u; f < m.FaceCount(); ++f) {
						if (!m.faces[f].alive) {
							continue;
						}
						m.GetFaceVerts((NkEmId)f, fv);
						float32 wf = 0.f;
						for (uint32 k = 0u; k < (uint32)fv.Size(); ++k) {
							wf += w[fv[k]];
						}
						wf /= (float32)fv.Size();
						if (wf <= 0.f || wf >= 1.f) {
							continue;  // rigide : ne peut pas se retourner
						}
						// UNE PLIURE : la face est a l'envers pour les DEUX os.
						const NkVec3f n0 = NormaleFace(m, repos, f);
						const NkVec3f n1 = NormaleFace(m, pos, f);
						if (Scal(n1, n0) < 0.f && Scal(n1, Tourner(n0, axe, angle)) < 0.f) {
							++retournees;
						}
					}
					if (mode == 0u) {
						const float32 totalPlie = VolumeCone(m, pos, V3(0.f, 0.f, 0.f), nullptr);
						const float32 perteTotale = volumeRepos != 0.f ? (volumeRepos - totalPlie) / volumeRepos : 0.f;
						if (perteLocale > r.perteVolumeLocalePire) {
							r.perteVolumeLocalePire = perteLocale;
							std::snprintf(r.articulationPerte, sizeof(r.articulationPerte), "%s", doc.os[c].nom);
						}
						if (retournees > r.pliuresPire) {
							r.pliuresPire = retournees;
							std::snprintf(r.articulationPire, sizeof(r.articulationPire), "%s", doc.os[c].nom);
						}
						r.perteVolumeTotalePire = perteTotale > r.perteVolumeTotalePire ? perteTotale : r.perteVolumeTotalePire;
						r.facesRetournees += retournees;
					} else {
						r.perteVolumeLocaleLineaire =
							perteLocale > r.perteVolumeLocaleLineaire ? perteLocale : r.perteVolumeLocaleLineaire;
						r.facesRetourneesLineaire += retournees;
					}
				}
				++r.articulationsPliees;
			}
			return true;
		}

	}  // namespace renderer
}  // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Creation Date: 2026-09-29
// ============================================================
