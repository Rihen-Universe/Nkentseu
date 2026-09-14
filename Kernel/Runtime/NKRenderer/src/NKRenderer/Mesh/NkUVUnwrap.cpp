// -----------------------------------------------------------------------------
// @File    NkUVUnwrap.cpp
// @Brief   Depliage UV par LSCM. Voir l'en-tete pour le POURQUOI et la reference
//          bibliographique ; ici le COMMENT.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// REFERENCE
//   Levy B., Petitjean S., Ray N., Maillot J., « Least Squares Conformal Maps for
//   Automatic Texture Atlas Generation », ACM SIGGRAPH 2002, ACM Transactions on
//   Graphics 21(3), pp. 362-371.
//
// PRECISION INTERNE : float64, ALORS QUE LE RESULTAT EST EN float32
//   Ce n'est pas un exces de prudence. Le critere de ce lot est qu'un quad plan se
//   deplie avec une distorsion EXACTEMENT nulle. En float32 (eps = 1,19e-7), un
//   gradient conjugue laisse un residu de l'ordre de 1e-6 apres amplification par
//   le conditionnement — et ce 1e-6 se serait lu comme une distorsion reelle,
//   c'est-a-dire que la PRECISION DU SOLVEUR se serait deguisee en DEFAUT DE
//   DEPLIAGE. En float64 le residu retombe sous 1e-14 et l'ecart qui subsiste sur
//   le resultat final est celui, inevitable, de l'arrondi vers float32 au moment
//   d'ecrire `Vert::uv` : une quantite qu'on peut nommer au lieu de la subir.
// -----------------------------------------------------------------------------

#include "NkUVUnwrap.h"

#include "NKFileSystem/NkFile.h"

#include <math.h>
#include <string.h>

namespace nkentseu {
	namespace renderer {

		const char *NkUVRefusName(NkUVRefus r) noexcept {
			switch (r) {
				case NkUVRefus::Aucun: return "NK_UV_OK";
				case NkUVRefus::MaillageVide: return "NK_UV_MAILLAGE_VIDE";
				case NkUVRefus::IlotNonDisque: return "NK_UV_ILOT_NON_DISQUE";
				case NkUVRefus::AreteNonManifold: return "NK_UV_ARETE_NON_MANIFOLD";
				case NkUVRefus::SolveurNonConverge: return "NK_UV_SOLVEUR_NON_CONVERGE";
				case NkUVRefus::CoinsSoudes: return "NK_UV_COINS_SOUDES";
			}
			return "NK_UV_INCONNU";
		}

		namespace {

			// ── UNION-FIND (par rang, avec compression de chemin) ────────────────
			// Sert DEUX fois, sur deux populations differentes : les faces (pour
			// grouper les ilots) et les coins (pour compter les sommets APRES
			// decoupe). Les deux questions sont la meme question posee a deux
			// echelles, il serait absurde d'ecrire deux fois le code.
			struct UnionFind {
					NkVector<uint32> parent;
					NkVector<uint32> rank;

					void Init(uint32 n) {
						parent.Resize(n);
						rank.Resize(n);
						for (uint32 i = 0; i < n; ++i) {
							parent[i] = i;
							rank[i] = 0u;
						}
					}
					uint32 Find(uint32 a) {
						while (parent[a] != a) {
							parent[a] = parent[parent[a]]; // compression by halving
							a = parent[a];
						}
						return a;
					}
					// Rend true si les deux classes etaient DISTINCTES (l'union a eu
					// un effet). L'appelant s'en sert pour compter les fusions, ce
					// qui est la seule facon de savoir qu'une couture a servi.
					bool Unite(uint32 a, uint32 b) {
						a = Find(a);
						b = Find(b);
						if (a == b) return false;
						if (rank[a] < rank[b]) {
							uint32 t = a;
							a = b;
							b = t;
						}
						parent[b] = a;
						if (rank[a] == rank[b]) rank[a] += 1u;
						return true;
					}
			};

			struct Vec2d {
					float64 x = 0.0, y = 0.0;
			};

			inline float64 Dot3(const NkVec3f &a, const NkVec3f &b) {
				return (float64)a.x * b.x + (float64)a.y * b.y + (float64)a.z * b.z;
			}
			inline NkVec3f Sub3(const NkVec3f &a, const NkVec3f &b) {
				return NkVec3f{a.x - b.x, a.y - b.y, a.z - b.z};
			}
			inline NkVec3f Cross3(const NkVec3f &a, const NkVec3f &b) {
				return NkVec3f{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
			}
			inline float64 Len3(const NkVec3f &a) {
				return sqrt(Dot3(a, a));
			}

			// ── PROJECTION ISOMETRIQUE D'UN TRIANGLE DANS SON PROPRE PLAN ────────
			// p1 -> (0,0) ; p2 -> (|e1|, 0) ; p3 -> coordonnees dans le repere
			// (e1 normalise, perpendiculaire dans le plan). C'est une ISOMETRIE :
			// les trois longueurs et les trois angles sont conserves exactement.
			// C'est pour cela que le LSCM peut ensuite comparer UV et 3D sans
			// jamais manipuler la troisieme dimension.
			void ProjectTriangle(const NkVec3f &a, const NkVec3f &b, const NkVec3f &c, Vec2d out[3]) {
				const NkVec3f e1 = Sub3(b, a);
				const NkVec3f e2 = Sub3(c, a);
				const float64 l1 = Len3(e1);
				out[0].x = 0.0;
				out[0].y = 0.0;
				out[1].x = l1;
				out[1].y = 0.0;
				if (l1 < 1e-30) {
					// Arete degeneree : le repere n'existe pas. On rend un triangle
					// plat plutot qu'un NaN — la face sera de toute facon ecartee par
					// son aire nulle, et un NaN aurait contamine tout le systeme.
					out[2].x = 0.0;
					out[2].y = 0.0;
					return;
				}
				const float64 invL1 = 1.0 / l1;
				const float64 px = Dot3(e2, e1) * invL1;
				const NkVec3f n = Cross3(e1, e2);
				const float64 py = Len3(n) * invL1;
				out[2].x = px;
				out[2].y = py;
			}

			struct Tri {
					uint32 var[3] = {0, 0, 0};	 // variable UV (classe de coin) par sommet
					uint32 vert[3] = {0, 0, 0};	 // Vert porteur, pour ecrire le resultat
					uint32 face = 0;
			};

			// ── MATRICE CREUSE EN TRIPLETS ───────────────────────────────────────
			// Aucune structure par ligne : le gradient conjugue n'a besoin que
			// d'appliquer A et A transposee, et un balayage de triplets le fait sans
			// une seule allocation par iteration.
			struct Triplet {
					uint32 row = 0;
					uint32 col = 0;
					float64 val = 0.0;
			};

			void ApplyA(const NkVector<Triplet> &T, const NkVector<float64> &x, NkVector<float64> &y) {
				for (uint32 i = 0; i < (uint32)y.Size(); ++i) y[i] = 0.0;
				for (uint32 i = 0; i < (uint32)T.Size(); ++i) y[T[i].row] += T[i].val * x[T[i].col];
			}
			void ApplyAt(const NkVector<Triplet> &T, const NkVector<float64> &y, NkVector<float64> &x) {
				for (uint32 i = 0; i < (uint32)x.Size(); ++i) x[i] = 0.0;
				for (uint32 i = 0; i < (uint32)T.Size(); ++i) x[T[i].col] += T[i].val * y[T[i].row];
			}

			// ── AIRE SIGNEE D'UN POLYGONE 2D (lacet de Gauss) ────────────────────
			float64 PolyArea(const Vec2d *p, uint32 n) {
				if (n < 3) return 0.0;
				float64 s = 0.0;
				for (uint32 i = 0; i < n; ++i) {
					const Vec2d &a = p[i];
					const Vec2d &b = p[(i + 1u) % n];
					s += a.x * b.y - b.x * a.y;
				}
				return 0.5 * s;
			}

			// ── DECOUPE DE SUTHERLAND-HODGMAN ───────────────────────────────────
			// Decoupe le polygone `poly` par le demi-plan interieur de l'arete
			// (a -> b) d'un triangle suppose en sens direct.
			uint32 ClipHalfPlane(const Vec2d *poly, uint32 n, const Vec2d &a, const Vec2d &b, Vec2d *out) {
				uint32 m = 0;
				if (n == 0) return 0;
				const float64 ex = b.x - a.x, ey = b.y - a.y;
				for (uint32 i = 0; i < n; ++i) {
					const Vec2d &P = poly[i];
					const Vec2d &Q = poly[(i + 1u) % n];
					const float64 sp = ex * (P.y - a.y) - ey * (P.x - a.x);
					const float64 sq = ex * (Q.y - a.y) - ey * (Q.x - a.x);
					const bool inP = (sp >= 0.0);
					const bool inQ = (sq >= 0.0);
					if (inP) out[m++] = P;
					if (inP != inQ) {
						const float64 d = sp - sq;
						if (d != 0.0) {
							const float64 t = sp / d;
							Vec2d I;
							I.x = P.x + (Q.x - P.x) * t;
							I.y = P.y + (Q.y - P.y) * t;
							out[m++] = I;
						}
					}
				}
				return m;
			}

			// Aire d'intersection de deux triangles 2D. Les deux sont remis en sens
			// direct avant la decoupe : un triangle RETOURNE (ce qui arrive quand un
			// ilot se replie sur lui-meme) donnerait sinon une aire negative, et un
			// recouvrement reel passerait pour une absence de recouvrement.
			float64 TriIntersectArea(const Vec2d A[3], const Vec2d B[3]) {
				Vec2d a[3] = {A[0], A[1], A[2]};
				Vec2d b[3] = {B[0], B[1], B[2]};
				if (PolyArea(a, 3) < 0.0) {
					Vec2d t = a[1];
					a[1] = a[2];
					a[2] = t;
				}
				if (PolyArea(b, 3) < 0.0) {
					Vec2d t = b[1];
					b[1] = b[2];
					b[2] = t;
				}
				Vec2d buf0[16], buf1[16];
				uint32 n = 3;
				buf0[0] = a[0];
				buf0[1] = a[1];
				buf0[2] = a[2];
				Vec2d *src = buf0, *dst = buf1;
				for (uint32 e = 0; e < 3u; ++e) {
					n = ClipHalfPlane(src, n, b[e], b[(e + 1u) % 3u], dst);
					Vec2d *t = src;
					src = dst;
					dst = t;
					if (n == 0) return 0.0;
				}
				const float64 ar = PolyArea(src, n);
				return (ar < 0.0) ? -ar : ar;
			}

			// Coin `k` de la face `f` portant le Vert dont le representant soude est
			// `owner`. NK_EM_INVALID si absent.
			uint32 CornerOfOwner(const NkEditMesh &m, const NkVector<NkEmId> &fv, uint32 owner) {
				for (uint32 k = 0; k < (uint32)fv.Size(); ++k) {
					if (m.VertOwner(fv[k]) == owner) return k;
				}
				return NK_EM_INVALID;
			}

			// Triangulation en eventail d'une face a N coins : (0, i, i+1).
			// Choix assume : c'est la meme triangulation que `Triangulate()` du
			// maillage, donc les UV produites ici sont coherentes avec celles que le
			// GPU consommera. Une triangulation differente de part et d'autre aurait
			// produit des UV justes qui se seraient affichees fausses.
			struct Corner {
					uint32 face = 0;
					uint32 k = 0;
			};

		} // namespace

		// =====================================================================
		// LE SOLVEUR
		// =====================================================================
		bool NkUVUnwrap(NkEditMesh &mesh, const NkUVUnwrapParams &params, NkUVResult &outResult,
						NkVector<NkUVIslandInfo> *outIslands, NkVector<NkEmId> *outIslandFaces) noexcept {
			outResult = NkUVResult{};

			const uint32 faceCount = (uint32)mesh.faces.Size();
			const uint32 vertCount = (uint32)mesh.verts.Size();
			if (faceCount == 0u || vertCount == 0u) {
				outResult.refus = NkUVRefus::MaillageVide;
				return false;
			}

			// ── Faces vivantes, et leurs coins ───────────────────────────────
			NkVector<uint32> liveFaces;
			NkVector<uint32> faceCornerStart; // index du 1er coin global de la face
			NkVector<uint32> faceCornerCount;
			NkVector<uint32> cornerVert;	  // Vert de chaque coin global
			NkVector<uint32> cornerFaceLocal; // index dans liveFaces
			faceCornerStart.Resize(faceCount);
			faceCornerCount.Resize(faceCount);
			for (uint32 i = 0; i < faceCount; ++i) {
				faceCornerStart[i] = NK_EM_INVALID;
				faceCornerCount[i] = 0u;
			}

			NkVector<NkEmId> fv;
			for (uint32 f = 0; f < faceCount; ++f) {
				if (!mesh.faces[f].alive) continue;
				fv.Clear();
				mesh.GetFaceVerts((NkEmId)f, fv);
				if ((uint32)fv.Size() < 3u) continue;
				faceCornerStart[f] = (uint32)cornerVert.Size();
				faceCornerCount[f] = (uint32)fv.Size();
				const uint32 localIdx = (uint32)liveFaces.Size();
				liveFaces.PushBack(f);
				for (uint32 k = 0; k < (uint32)fv.Size(); ++k) {
					cornerVert.PushBack((uint32)fv[k]);
					cornerFaceLocal.PushBack(localIdx);
				}
			}
			if (liveFaces.Size() == 0u) {
				outResult.refus = NkUVRefus::MaillageVide;
				return false;
			}

			// ── Coutures : table d'appartenance par arete ────────────────────
			NkVector<uint8> isSeam;
			isSeam.Resize(mesh.edges.Size(), (uint8)0);
			for (uint32 i = 0; i < params.seamCount; ++i) {
				const NkEmId e = params.seams[i];
				if (e < (NkEmId)isSeam.Size()) isSeam[e] = 1u;
			}

			// ── (u1) ILOTS : union-find sur les FACES ────────────────────────
			// Deux faces sont dans le meme ilot si elles partagent une arete qui
			// n'est PAS une couture. C'est la definition, litteralement.
			NkVector<uint32> faceLocalOf; // face globale -> index local
			faceLocalOf.Resize(faceCount);
			for (uint32 i = 0; i < faceCount; ++i) faceLocalOf[i] = NK_EM_INVALID;
			for (uint32 i = 0; i < (uint32)liveFaces.Size(); ++i) faceLocalOf[liveFaces[i]] = i;

			UnionFind ufFace;
			ufFace.Init((uint32)liveFaces.Size());

			// Union-find sur les COINS : deux coins fusionnent s'ils se rejoignent
			// a travers une arete non-couture. Le nombre de classes restantes EST
			// le nombre de sommets du maillage decoupe — donc le V de la
			// caracteristique d'Euler.
			UnionFind ufCorner;
			ufCorner.Init((uint32)cornerVert.Size());

			uint32 internalEdges = 0u; // aretes non-coutures a 2 faces vivantes
			NkVector<NkEmId> ef;
			NkVector<NkEmId> fvA, fvB;

			for (uint32 e = 0; e < (uint32)mesh.edges.Size(); ++e) {
				if (!mesh.edges[e].alive) continue;
				if (mesh.EdgeIsNonManifold((NkEmId)e)) {
					// « L'autre cote » n'a pas de sens sur une arete a plus de deux
					// faces, et en choisir un au hasard serait un mensonge — c'est
					// deja la position de NkEditMesh (cf. EdgeOtherFace).
					outResult.refus = NkUVRefus::AreteNonManifold;
					return false;
				}
				if (isSeam[e]) continue;
				ef.Clear();
				mesh.EdgeFaces((NkEmId)e, ef);
				if ((uint32)ef.Size() != 2u) continue;
				const uint32 fa = (uint32)ef[0], fb = (uint32)ef[1];
				if (fa >= faceCount || fb >= faceCount) continue;
				const uint32 la = faceLocalOf[fa], lb = faceLocalOf[fb];
				if (la == NK_EM_INVALID || lb == NK_EM_INVALID) continue;

				ufFace.Unite(la, lb);
				++internalEdges;

				// Fusion des DEUX paires de coins de part et d'autre de l'arete.
				const uint32 o0 = mesh.VertOwner(mesh.edges[e].v0);
				const uint32 o1 = mesh.VertOwner(mesh.edges[e].v1);
				fvA.Clear();
				fvB.Clear();
				mesh.GetFaceVerts((NkEmId)fa, fvA);
				mesh.GetFaceVerts((NkEmId)fb, fvB);
				const uint32 ka0 = CornerOfOwner(mesh, fvA, o0);
				const uint32 ka1 = CornerOfOwner(mesh, fvA, o1);
				const uint32 kb0 = CornerOfOwner(mesh, fvB, o0);
				const uint32 kb1 = CornerOfOwner(mesh, fvB, o1);
				if (ka0 != NK_EM_INVALID && kb0 != NK_EM_INVALID)
					ufCorner.Unite(faceCornerStart[fa] + ka0, faceCornerStart[fb] + kb0);
				if (ka1 != NK_EM_INVALID && kb1 != NK_EM_INVALID)
					ufCorner.Unite(faceCornerStart[fa] + ka1, faceCornerStart[fb] + kb1);
			}

			// ── CONTRAINTE DE REPRESENTATION, ET POURQUOI ELLE EST ICI ───────
			// Il n'existe qu'UN champ `uv` par `Vert`. Deux coins qui partagent le
			// meme `Vert` ne PEUVENT donc pas porter deux UV differentes, meme si
			// une couture les separe. Sur les maillages de ce depot un Vert EST un
			// coin (un cube en a 24, pas 8) et le cas ne se presente pas — mais
			// « ne se presente pas aujourd'hui » n'est pas un invariant : un
			// maillage importe soude peut tres bien arriver ici.
			// On fusionne donc aussi par Vert, ce qui rend la contrainte VISIBLE
			// dans le compte des sommets au lieu de la laisser produire en silence
			// des UV fausses le long des coutures.
			//
			// ⚠ ET ON COMPTE CE QUE CETTE FUSION COUTE. `Unite` rend true quand les
			// deux classes etaient DISTINCTES : a ce stade elles ne le sont que si
			// aucune suite d'aretes non-coutures ne les relie, c'est-a-dire si une
			// couture les separait. Chaque `true` est donc une couture rendue SANS
			// EFFET par le partage d'un `Vert`.
			//
			// Sans ce compteur, le cas ressortait sous le nom `IlotNonDisque` : la
			// soudure effondre le compte des sommets, Euler s'ecarte de 1, et le
			// solveur accusait la topologie d'un defaut de representation. C'est un
			// NEGATIF (une sphere aux sommets partages) qui l'a mis au jour — aucun
			// critere positif ne l'aurait fait, puisque tous mes maillages de
			// controle ont un Vert par coin.
			{
				NkVector<uint32> firstCornerOfVert;
				firstCornerOfVert.Resize(vertCount);
				for (uint32 i = 0; i < vertCount; ++i) firstCornerOfVert[i] = NK_EM_INVALID;
				for (uint32 c = 0; c < (uint32)cornerVert.Size(); ++c) {
					const uint32 v = cornerVert[c];
					if (v >= vertCount) continue;
					if (firstCornerOfVert[v] == NK_EM_INVALID) firstCornerOfVert[v] = c;
					else if (ufCorner.Unite(firstCornerOfVert[v], c)) outResult.weldedCorners += 1u;
				}
			}
			if (outResult.weldedCorners != 0u) {
				// On refuse AVANT de calculer Euler : sinon le refus sortirait sous
				// le nom du symptome au lieu de celui de la cause.
				outResult.refus = NkUVRefus::CoinsSoudes;
				return false;
			}

			// ── Regroupement des faces par ilot ──────────────────────────────
			NkVector<uint32> islandOfLocal;
			islandOfLocal.Resize(liveFaces.Size());
			NkVector<uint32> rootToIsland;
			rootToIsland.Resize(liveFaces.Size());
			for (uint32 i = 0; i < (uint32)liveFaces.Size(); ++i) rootToIsland[i] = NK_EM_INVALID;
			uint32 islandCount = 0u;
			for (uint32 i = 0; i < (uint32)liveFaces.Size(); ++i) {
				const uint32 r = ufFace.Find(i);
				if (rootToIsland[r] == NK_EM_INVALID) rootToIsland[r] = islandCount++;
				islandOfLocal[i] = rootToIsland[r];
			}
			outResult.islandCount = islandCount;

			// Faces par ilot, a plat (firstFace/faceCount).
			NkVector<uint32> islandFaceCount;
			islandFaceCount.Resize(islandCount, 0u);
			for (uint32 i = 0; i < (uint32)liveFaces.Size(); ++i) islandFaceCount[islandOfLocal[i]] += 1u;
			NkVector<uint32> islandStart;
			islandStart.Resize(islandCount, 0u);
			{
				uint32 acc = 0u;
				for (uint32 i = 0; i < islandCount; ++i) {
					islandStart[i] = acc;
					acc += islandFaceCount[i];
				}
			}
			NkVector<uint32> islandFaces;
			islandFaces.Resize(liveFaces.Size(), 0u);
			{
				NkVector<uint32> cursor;
				cursor.Resize(islandCount, 0u);
				for (uint32 i = 0; i < (uint32)liveFaces.Size(); ++i) {
					const uint32 is = islandOfLocal[i];
					islandFaces[islandStart[is] + cursor[is]] = i;
					cursor[is] += 1u;
				}
			}

			// ── EULER PAR ILOT ───────────────────────────────────────────────
			// V = classes de coins presentes dans l'ilot
			// E = (somme des cotes des faces) - (aretes internes de l'ilot)
			//     car chaque arete interne est partagee par deux cotes, et tout
			//     autre cote est une arete de bord a lui seul.
			// F = nombre de faces
			NkVector<NkUVIslandInfo> islands;
			islands.Resize(islandCount);

			{
				// Aretes internes par ilot.
				NkVector<uint32> internalPerIsland;
				internalPerIsland.Resize(islandCount, 0u);
				for (uint32 e = 0; e < (uint32)mesh.edges.Size(); ++e) {
					if (!mesh.edges[e].alive || isSeam[e]) continue;
					ef.Clear();
					mesh.EdgeFaces((NkEmId)e, ef);
					if ((uint32)ef.Size() != 2u) continue;
					const uint32 la = faceLocalOf[ef[0]], lb = faceLocalOf[ef[1]];
					if (la == NK_EM_INVALID || lb == NK_EM_INVALID) continue;
					if (islandOfLocal[la] == islandOfLocal[lb]) internalPerIsland[islandOfLocal[la]] += 1u;
				}
				// Sommets distincts par ilot : on marque les racines vues.
				NkVector<uint32> rootSeenTag;
				rootSeenTag.Resize(cornerVert.Size(), NK_EM_INVALID);
				for (uint32 is = 0; is < islandCount; ++is) {
					uint32 sides = 0u, verts = 0u;
					for (uint32 j = 0; j < islandFaceCount[is]; ++j) {
						const uint32 lf = islandFaces[islandStart[is] + j];
						const uint32 gf = liveFaces[lf];
						const uint32 n = faceCornerCount[gf];
						sides += n;
						for (uint32 k = 0; k < n; ++k) {
							const uint32 root = ufCorner.Find(faceCornerStart[gf] + k);
							if (rootSeenTag[root] != is) {
								rootSeenTag[root] = is;
								++verts;
							}
						}
					}
					islands[is].firstFace = islandStart[is];
					islands[is].faceCount = islandFaceCount[is];
					islands[is].vertCount = verts;
					islands[is].edgeCount = sides - internalPerIsland[is];
					islands[is].euler = (int32)islands[is].vertCount - (int32)islands[is].edgeCount +
										(int32)islands[is].faceCount;
				}
			}

			for (uint32 is = 0; is < islandCount; ++is) {
				if (islands[is].euler != 1) {
					// AUCUNE UV N'A ETE ECRITE A CE STADE, et c'est deliberé : un
					// refus qui laisserait des UV a moitie posees ferait croire a un
					// demi-succes.
					outResult.refus = NkUVRefus::IlotNonDisque;
					outResult.refusIsland = is;
					outResult.refusEuler = islands[is].euler;
					if (outIslands) *outIslands = islands;
					return false;
				}
			}

			// ── VARIABLES UV : une par classe de coin, numerotees par ilot ────
			NkVector<uint32> varOfRoot;
			varOfRoot.Resize(cornerVert.Size(), NK_EM_INVALID);
			NkVector<float64> uvOut;
			uvOut.Resize(cornerVert.Size() * 2u, 0.0);

			// Triangles par ilot (eventail).
			for (uint32 is = 0; is < islandCount; ++is) {
				NkVector<Tri> tris;
				NkVector<uint32> varRoots; // variable -> racine de coin
				uint32 nVar = 0u;

				for (uint32 j = 0; j < islandFaceCount[is]; ++j) {
					const uint32 lf = islandFaces[islandStart[is] + j];
					const uint32 gf = liveFaces[lf];
					const uint32 n = faceCornerCount[gf];
					const uint32 c0 = faceCornerStart[gf];
					for (uint32 k = 0; k + 2u < n; ++k) {
						Tri t;
						const uint32 ci[3] = {c0, c0 + k + 1u, c0 + k + 2u};
						for (uint32 s = 0; s < 3u; ++s) {
							const uint32 root = ufCorner.Find(ci[s]);
							if (varOfRoot[root] == NK_EM_INVALID) {
								varOfRoot[root] = nVar++;
								varRoots.PushBack(root);
							}
							t.var[s] = varOfRoot[root];
							t.vert[s] = cornerVert[ci[s]];
						}
						t.face = gf;
						tris.PushBack(t);
					}
				}
				if (tris.Size() == 0u || nVar < 3u) continue;

				// ── PIN DE DEUX SOMMETS ──────────────────────────────────────
				// Le systeme est invariant par similitude (noyau de dimension 4 :
				// deux translations, une rotation, une echelle). Fixer deux
				// sommets retire exactement ces quatre degres de liberte — ni
				// plus (le systeme resterait singulier) ni moins (on imposerait
				// une deformation).
				// On prend deux sommets ELOIGNES (double balayage du plus lointain)
				// : deux pins voisins rendraient le systeme mal conditionne, et le
				// gradient conjugue le paierait en precision — donc en distorsion
				// apparente.
				NkVector<NkVec3f> varPos;
				varPos.Resize(nVar);
				for (uint32 v = 0; v < nVar; ++v) {
					// Position 3D : n'importe quel coin de la classe convient, ils
					// sont geometriquement confondus par construction.
					varPos[v] = NkVec3f{0.f, 0.f, 0.f};
				}
				{
					NkVector<uint8> got;
					got.Resize(nVar, (uint8)0);
					for (uint32 j = 0; j < islandFaceCount[is]; ++j) {
						const uint32 gf = liveFaces[islandFaces[islandStart[is] + j]];
						const uint32 n = faceCornerCount[gf];
						const uint32 c0 = faceCornerStart[gf];
						for (uint32 k = 0; k < n; ++k) {
							const uint32 vv = varOfRoot[ufCorner.Find(c0 + k)];
							if (vv != NK_EM_INVALID && vv < nVar && !got[vv]) {
								got[vv] = 1u;
								varPos[vv] = mesh.verts[cornerVert[c0 + k]].pos;
							}
						}
					}
				}
				uint32 pinA = 0u, pinB = 0u;
				{
					float64 best = -1.0;
					for (uint32 v = 1; v < nVar; ++v) {
						const float64 d = Len3(Sub3(varPos[v], varPos[0]));
						if (d > best) {
							best = d;
							pinA = v;
						}
					}
					best = -1.0;
					for (uint32 v = 0; v < nVar; ++v) {
						if (v == pinA) continue;
						const float64 d = Len3(Sub3(varPos[v], varPos[pinA]));
						if (d > best) {
							best = d;
							pinB = v;
						}
					}
				}
				const float64 pinDist = Len3(Sub3(varPos[pinB], varPos[pinA]));
				// Les pins sont poses a LEUR VRAIE DISTANCE 3D : l'echelle du
				// depliage est alors celle du modele, ce qui rend le rapport
				// d'aire brut deja proche de 1. La mesure le normalise de toute
				// facon (cf. l'en-tete), mais partir juste evite de dependre de la
				// normalisation pour paraitre juste.
				const float64 pinAu = 0.0, pinAv = 0.0;
				const float64 pinBu = (pinDist > 1e-30) ? pinDist : 1.0, pinBv = 0.0;

				// Numerotation des inconnues : les variables libres, u puis v.
				NkVector<uint32> freeIdx;
				freeIdx.Resize(nVar, NK_EM_INVALID);
				uint32 nFree = 0u;
				for (uint32 v = 0; v < nVar; ++v) {
					if (v == pinA || v == pinB) continue;
					freeIdx[v] = nFree++;
				}
				const uint32 nUnknown = nFree * 2u;
				const uint32 nRows = (uint32)tris.Size() * 2u;

				NkVector<Triplet> A;
				NkVector<float64> b;
				b.Resize(nRows, 0.0);

				for (uint32 ti = 0; ti < (uint32)tris.Size(); ++ti) {
					const Tri &t = tris[ti];
					Vec2d p[3];
					ProjectTriangle(mesh.verts[t.vert[0]].pos, mesh.verts[t.vert[1]].pos,
									mesh.verts[t.vert[2]].pos, p);
					const float64 area2 = (p[1].x - p[0].x) * (p[2].y - p[0].y) -
										  (p[2].x - p[0].x) * (p[1].y - p[0].y);
					const float64 area = (area2 < 0.0 ? -area2 : area2) * 0.5;
					if (area < 1e-24) continue; // face degeneree : elle n'a rien a dire
					const float64 w = 1.0 / sqrt(area);

					// d_j = p_{j+2} - p_{j+1}
					Vec2d d[3];
					for (uint32 j = 0; j < 3u; ++j) {
						const Vec2d &q2 = p[(j + 2u) % 3u];
						const Vec2d &q1 = p[(j + 1u) % 3u];
						d[j].x = q2.x - q1.x;
						d[j].y = q2.y - q1.y;
					}

					const uint32 rowRe = ti * 2u, rowIm = ti * 2u + 1u;
					for (uint32 j = 0; j < 3u; ++j) {
						const uint32 v = t.var[j];
						const float64 dx = d[j].x * w, dy = d[j].y * w;
						// Re : u_j*dx - v_j*dy    Im : u_j*dy + v_j*dx
						if (v == pinA || v == pinB) {
							const float64 pu = (v == pinA) ? pinAu : pinBu;
							const float64 pv = (v == pinA) ? pinAv : pinBv;
							b[rowRe] -= pu * dx - pv * dy;
							b[rowIm] -= pu * dy + pv * dx;
						} else {
							const uint32 cu = freeIdx[v] * 2u, cv = cu + 1u;
							Triplet t1;
							t1.row = rowRe;
							t1.col = cu;
							t1.val = dx;
							A.PushBack(t1);
							Triplet t2;
							t2.row = rowRe;
							t2.col = cv;
							t2.val = -dy;
							A.PushBack(t2);
							Triplet t3;
							t3.row = rowIm;
							t3.col = cu;
							t3.val = dy;
							A.PushBack(t3);
							Triplet t4;
							t4.row = rowIm;
							t4.col = cv;
							t4.val = dx;
							A.PushBack(t4);
						}
					}
				}

				// ── GRADIENT CONJUGUE SUR LES EQUATIONS NORMALES ─────────────
				NkVector<float64> x, r, pdir, Ap, tmpRow, Atb;
				x.Resize(nUnknown, 0.0);
				r.Resize(nUnknown, 0.0);
				pdir.Resize(nUnknown, 0.0);
				Ap.Resize(nUnknown, 0.0);
				Atb.Resize(nUnknown, 0.0);
				tmpRow.Resize(nRows, 0.0);

				ApplyAt(A, b, Atb);
				ApplyA(A, x, tmpRow);
				ApplyAt(A, tmpRow, r);
				for (uint32 i = 0; i < nUnknown; ++i) {
					r[i] = Atb[i] - r[i];
					pdir[i] = r[i];
				}
				float64 rs = 0.0;
				for (uint32 i = 0; i < nUnknown; ++i) rs += r[i] * r[i];

				const uint32 maxIt = (params.cgIterations != 0u)
										 ? params.cgIterations
										 : ((nUnknown * 4u < 4096u) ? (nUnknown * 4u + 16u) : 4096u);
				const float64 tol = (float64)params.cgTolerance;
				uint32 it = 0u;
				for (; it < maxIt && rs > tol; ++it) {
					ApplyA(A, pdir, tmpRow);
					ApplyAt(A, tmpRow, Ap);
					float64 pAp = 0.0;
					for (uint32 i = 0; i < nUnknown; ++i) pAp += pdir[i] * Ap[i];
					if (pAp <= 0.0) break; // direction degeneree : on s'arrete net
					const float64 alpha = rs / pAp;
					for (uint32 i = 0; i < nUnknown; ++i) {
						x[i] += alpha * pdir[i];
						r[i] -= alpha * Ap[i];
					}
					float64 rs2 = 0.0;
					for (uint32 i = 0; i < nUnknown; ++i) rs2 += r[i] * r[i];
					const float64 beta = rs2 / rs;
					for (uint32 i = 0; i < nUnknown; ++i) pdir[i] = r[i] + beta * pdir[i];
					rs = rs2;
				}
				if (it > outResult.cgIterationsUsed) outResult.cgIterationsUsed = it;
				const float32 res = (float32)sqrt(rs);
				if (res > outResult.cgResidual) outResult.cgResidual = res;

				// ── Report du resultat sur les coins de l'ilot ───────────────
				for (uint32 j = 0; j < islandFaceCount[is]; ++j) {
					const uint32 gf = liveFaces[islandFaces[islandStart[is] + j]];
					const uint32 n = faceCornerCount[gf];
					const uint32 c0 = faceCornerStart[gf];
					for (uint32 k = 0; k < n; ++k) {
						const uint32 root = ufCorner.Find(c0 + k);
						const uint32 v = varOfRoot[root];
						if (v == NK_EM_INVALID || v >= nVar) continue;
						float64 uu, vv2;
						if (v == pinA) {
							uu = pinAu;
							vv2 = pinAv;
						} else if (v == pinB) {
							uu = pinBu;
							vv2 = pinBv;
						} else {
							uu = x[freeIdx[v] * 2u];
							vv2 = x[freeIdx[v] * 2u + 1u];
						}
						uvOut[(c0 + k) * 2u] = uu;
						uvOut[(c0 + k) * 2u + 1u] = vv2;
					}
				}

				// Remise a zero de la numerotation pour l'ilot suivant.
				for (uint32 v = 0; v < (uint32)varRoots.Size(); ++v) varOfRoot[varRoots[v]] = NK_EM_INVALID;
			}

			// ── Bornes par ilot, puis rangement ──────────────────────────────
			for (uint32 is = 0; is < islandCount; ++is) {
				float64 mnx = 1e300, mny = 1e300, mxx = -1e300, mxy = -1e300;
				for (uint32 j = 0; j < islandFaceCount[is]; ++j) {
					const uint32 gf = liveFaces[islandFaces[islandStart[is] + j]];
					const uint32 n = faceCornerCount[gf], c0 = faceCornerStart[gf];
					for (uint32 k = 0; k < n; ++k) {
						const float64 u = uvOut[(c0 + k) * 2u], v = uvOut[(c0 + k) * 2u + 1u];
						if (u < mnx) mnx = u;
						if (v < mny) mny = v;
						if (u > mxx) mxx = u;
						if (v > mxy) mxy = v;
					}
				}
				islands[is].uvMin = NkVec2f{(float32)mnx, (float32)mny};
				islands[is].uvMax = NkVec2f{(float32)mxx, (float32)mxy};
			}

			if (params.packIslands && islandCount > 0u) {
				// RANGEMENT EN BANDES, NON OPTIMAL — cf. l'en-tete. Il garantit
				// seulement que deux ilots ne se chevauchent pas : chacun occupe une
				// colonne disjointe de la precedente. C'est ce que (u2) exige, et
				// rien de plus n'est promis.
				const float64 margin = (float64)params.packMargin;
				float64 cursorX = margin, rowY = margin, rowH = 0.0;
				for (uint32 is = 0; is < islandCount; ++is) {
					const float64 w = (float64)islands[is].uvMax.x - (float64)islands[is].uvMin.x;
					const float64 h = (float64)islands[is].uvMax.y - (float64)islands[is].uvMin.y;
					if (cursorX + w > 1.0 - margin && cursorX > margin) {
						cursorX = margin;
						rowY += rowH + margin;
						rowH = 0.0;
					}
					const float64 dx = cursorX - (float64)islands[is].uvMin.x;
					const float64 dy = rowY - (float64)islands[is].uvMin.y;
					for (uint32 j = 0; j < islandFaceCount[is]; ++j) {
						const uint32 gf = liveFaces[islandFaces[islandStart[is] + j]];
						const uint32 n = faceCornerCount[gf], c0 = faceCornerStart[gf];
						for (uint32 k = 0; k < n; ++k) {
							uvOut[(c0 + k) * 2u] += dx;
							uvOut[(c0 + k) * 2u + 1u] += dy;
						}
					}
					islands[is].uvMin = NkVec2f{(float32)(islands[is].uvMin.x + dx),
												(float32)(islands[is].uvMin.y + dy)};
					islands[is].uvMax = NkVec2f{(float32)(islands[is].uvMax.x + dx),
												(float32)(islands[is].uvMax.y + dy)};
					cursorX += w + margin;
					if (h > rowH) rowH = h;
				}
			}

			// ── Ecriture sur le maillage (le seul champ touche : Vert::uv) ────
			for (uint32 c = 0; c < (uint32)cornerVert.Size(); ++c) {
				const uint32 v = cornerVert[c];
				if (v >= vertCount) continue;
				mesh.verts[v].uv = NkVec2f{(float32)uvOut[c * 2u], (float32)uvOut[c * 2u + 1u]};
			}

			NkUVMeasureDistortion(mesh, outResult.distortion);

			if (outIslands) *outIslands = islands;
			if (outIslandFaces) {
				outIslandFaces->Clear();
				for (uint32 i = 0; i < (uint32)islandFaces.Size(); ++i)
					outIslandFaces->PushBack((NkEmId)liveFaces[islandFaces[i]]);
			}
			return true;
		}

		// =====================================================================
		// (u2) RECOUVREMENT
		// =====================================================================
		uint32 NkUVCountOverlaps(const NkEditMesh &mesh, float32 *outArea) noexcept {
			NkVector<Vec2d> tri; // 3 entrees par triangle, a plat
			NkVector<NkEmId> fv;
			for (uint32 f = 0; f < (uint32)mesh.faces.Size(); ++f) {
				if (!mesh.faces[f].alive) continue;
				fv.Clear();
				mesh.GetFaceVerts((NkEmId)f, fv);
				const uint32 n = (uint32)fv.Size();
				if (n < 3u) continue;
				for (uint32 k = 0; k + 2u < n; ++k) {
					const uint32 ids[3] = {(uint32)fv[0], (uint32)fv[k + 1u], (uint32)fv[k + 2u]};
					for (uint32 s = 0; s < 3u; ++s) {
						Vec2d p;
						p.x = (float64)mesh.verts[ids[s]].uv.x;
						p.y = (float64)mesh.verts[ids[s]].uv.y;
						tri.PushBack(p);
					}
				}
			}
			const uint32 triCount = (uint32)tri.Size() / 3u;
			uint32 pairs = 0u;
			float64 total = 0.0;

			// SEUIL RELATIF, JAMAIS ABSOLU. Un seuil en unites UV se perimerait des
			// que l'echelle du depliage change : le meme maillage, deplie deux fois
			// plus grand, se mettrait a signaler des recouvrements qui n'existent
			// pas. On compare a l'aire du plus petit des deux triangles.
			for (uint32 i = 0; i < triCount; ++i) {
				Vec2d A[3] = {tri[i * 3u], tri[i * 3u + 1u], tri[i * 3u + 2u]};
				float64 aA = PolyArea(A, 3);
				if (aA < 0.0) aA = -aA;
				if (aA <= 0.0) continue;
				for (uint32 j = i + 1u; j < triCount; ++j) {
					Vec2d B[3] = {tri[j * 3u], tri[j * 3u + 1u], tri[j * 3u + 2u]};
					float64 aB = PolyArea(B, 3);
					if (aB < 0.0) aB = -aB;
					if (aB <= 0.0) continue;
					const float64 inter = TriIntersectArea(A, B);
					const float64 refA = (aA < aB) ? aA : aB;
					if (inter > refA * 1e-9) {
						++pairs;
						total += inter;
					}
				}
			}
			if (outArea) *outArea = (float32)total;
			return pairs;
		}

		// =====================================================================
		// (u3) DISTORSION
		// =====================================================================
		bool NkUVMeasureDistortion(const NkEditMesh &mesh, NkUVDistortion &out) noexcept {
			out = NkUVDistortion{};
			NkVector<float64> ratios;	// aireUV / aire3D, brut
			NkVector<float64> angleDev; // ecart max d'angle, en degres
			float64 sumUV = 0.0, sum3D = 0.0;

			NkVector<NkEmId> fv;
			for (uint32 f = 0; f < (uint32)mesh.faces.Size(); ++f) {
				if (!mesh.faces[f].alive) continue;
				fv.Clear();
				mesh.GetFaceVerts((NkEmId)f, fv);
				const uint32 n = (uint32)fv.Size();
				if (n < 3u) continue;
				for (uint32 k = 0; k + 2u < n; ++k) {
					const uint32 ids[3] = {(uint32)fv[0], (uint32)fv[k + 1u], (uint32)fv[k + 2u]};
					const NkVec3f P[3] = {mesh.verts[ids[0]].pos, mesh.verts[ids[1]].pos,
										  mesh.verts[ids[2]].pos};
					Vec2d Q[3];
					for (uint32 s = 0; s < 3u; ++s) {
						Q[s].x = (float64)mesh.verts[ids[s]].uv.x;
						Q[s].y = (float64)mesh.verts[ids[s]].uv.y;
					}
					const NkVec3f c3 = Cross3(Sub3(P[1], P[0]), Sub3(P[2], P[0]));
					const float64 a3 = 0.5 * Len3(c3);
					float64 a2 = PolyArea(Q, 3);
					if (a2 < 0.0) a2 = -a2;
					if (a3 < 1e-24) continue;

					sumUV += a2;
					sum3D += a3;
					ratios.PushBack(a2 / a3);

					// Ecart d'angle, au pire des trois coins.
					float64 worst = 0.0;
					for (uint32 s = 0; s < 3u; ++s) {
						const uint32 i1 = (s + 1u) % 3u, i2 = (s + 2u) % 3u;
						const NkVec3f e1 = Sub3(P[i1], P[s]), e2 = Sub3(P[i2], P[s]);
						const float64 l1 = Len3(e1), l2 = Len3(e2);
						if (l1 < 1e-30 || l2 < 1e-30) continue;
						float64 c = Dot3(e1, e2) / (l1 * l2);
						if (c > 1.0) c = 1.0;
						if (c < -1.0) c = -1.0;
						const float64 ang3 = acos(c);

						const float64 f1x = Q[i1].x - Q[s].x, f1y = Q[i1].y - Q[s].y;
						const float64 f2x = Q[i2].x - Q[s].x, f2y = Q[i2].y - Q[s].y;
						const float64 m1 = sqrt(f1x * f1x + f1y * f1y);
						const float64 m2 = sqrt(f2x * f2x + f2y * f2y);
						if (m1 < 1e-30 || m2 < 1e-30) continue;
						float64 c2 = (f1x * f2x + f1y * f2y) / (m1 * m2);
						if (c2 > 1.0) c2 = 1.0;
						if (c2 < -1.0) c2 = -1.0;
						const float64 ang2 = acos(c2);

						float64 dev = ang3 - ang2;
						if (dev < 0.0) dev = -dev;
						dev = dev * 57.29577951308232; // radians -> degres
						if (dev > worst) worst = dev;
					}
					angleDev.PushBack(worst);
				}
			}

			const uint32 n = (uint32)ratios.Size();
			out.triCount = n;
			if (n == 0u) return false;

			// NORMALISATION : cf. l'en-tete. Sans elle, un depliage parfait dont les
			// pins ont ete poses deux fois trop loin rendrait « 4 » partout.
			const float64 globalRatio = (sum3D > 0.0) ? (sumUV / sum3D) : 1.0;
			const float64 inv = (globalRatio > 0.0) ? (1.0 / globalRatio) : 1.0;

			float64 amin = 1e300, amax = -1e300, asum = 0.0;
			float64 gmin = 1e300, gmax = -1e300, gsum = 0.0;
			for (uint32 i = 0; i < n; ++i) {
				const float64 r = ratios[i] * inv;
				if (r < amin) amin = r;
				if (r > amax) amax = r;
				asum += r;
				const float64 d = angleDev[i];
				if (d < gmin) gmin = d;
				if (d > gmax) gmax = d;
				gsum += d;
			}
			out.areaMin = (float32)amin;
			out.areaMax = (float32)amax;
			out.areaMean = (float32)(asum / (float64)n);
			out.angleMin = (float32)gmin;
			out.angleMax = (float32)gmax;
			out.angleMean = (float32)(gsum / (float64)n);
			return true;
		}

		// =====================================================================
		// (u4) ENREGISTREMENT
		// =====================================================================
		// Format : "NKUV" | version(u32) | cornerCount(u32) | (u,v) float32 x N
		// Aucun checksum, et c'est VOLONTAIRE : le controle de survie falsifie un
		// bit de mantisse pour verifier que la comparaison le voit. Un checksum
		// ferait echouer la LECTURE avant la comparaison — on prouverait le
		// checksum, pas la survie des UV.
		namespace {
			inline void PutU32(NkVector<nk_uint8> &b, uint32 v) {
				b.PushBack((nk_uint8)(v & 0xFFu));
				b.PushBack((nk_uint8)((v >> 8) & 0xFFu));
				b.PushBack((nk_uint8)((v >> 16) & 0xFFu));
				b.PushBack((nk_uint8)((v >> 24) & 0xFFu));
			}
			inline uint32 GetU32(const NkVector<nk_uint8> &b, uint32 off) {
				return (uint32)b[off] | ((uint32)b[off + 1u] << 8) | ((uint32)b[off + 2u] << 16) |
					   ((uint32)b[off + 3u] << 24);
			}
		} // namespace

		bool NkUVSaveLayout(const NkEditMesh &mesh, const char *path) noexcept {
			if (path == nullptr) return false;
			const uint32 n = (uint32)mesh.verts.Size();
			NkVector<nk_uint8> bytes;
			bytes.Reserve(12u + n * 8u);
			bytes.PushBack((nk_uint8)'N');
			bytes.PushBack((nk_uint8)'K');
			bytes.PushBack((nk_uint8)'U');
			bytes.PushBack((nk_uint8)'V');
			PutU32(bytes, 1u);
			PutU32(bytes, n);
			for (uint32 i = 0; i < n; ++i) {
				// Les float32 TELS QUELS, bit pour bit. Aucune conversion en texte :
				// un aller-retour decimal perdrait des bits de mantisse et
				// l'enregistrement paraitrait fidele a six decimales pres.
				uint32 u32u = 0u, u32v = 0u;
				const float32 fu = mesh.verts[i].uv.x, fv2 = mesh.verts[i].uv.y;
				memcpy(&u32u, &fu, 4);
				memcpy(&u32v, &fv2, 4);
				PutU32(bytes, u32u);
				PutU32(bytes, u32v);
			}
			return NkFile::WriteAllBytes(path, bytes);
		}

		bool NkUVLoadLayout(NkEditMesh &mesh, const char *path) noexcept {
			if (path == nullptr) return false;
			NkVector<nk_uint8> bytes = NkFile::ReadAllBytes(path);
			if ((uint32)bytes.Size() < 12u) return false;
			if (bytes[0] != 'N' || bytes[1] != 'K' || bytes[2] != 'U' || bytes[3] != 'V') return false;
			const uint32 version = GetU32(bytes, 4u);
			if (version != 1u) return false;
			const uint32 n = GetU32(bytes, 8u);
			// Relire des UV dans le mauvais maillage produirait un resultat
			// plausible et faux : on refuse au lieu de tronquer.
			if (n != (uint32)mesh.verts.Size()) return false;
			if ((uint32)bytes.Size() < 12u + n * 8u) return false;
			for (uint32 i = 0; i < n; ++i) {
				const uint32 u32u = GetU32(bytes, 12u + i * 8u);
				const uint32 u32v = GetU32(bytes, 12u + i * 8u + 4u);
				float32 fu = 0.f, fv2 = 0.f;
				memcpy(&fu, &u32u, 4);
				memcpy(&fv2, &u32v, 4);
				mesh.verts[i].uv = NkVec2f{fu, fv2};
			}
			return true;
		}

	} // namespace renderer
} // namespace nkentseu
