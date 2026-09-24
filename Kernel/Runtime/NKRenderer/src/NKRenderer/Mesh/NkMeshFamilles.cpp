// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Mesh/NkMeshFamilles.cpp
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// Voir NkMeshFamilles.h pour le pourquoi du demenagement.
//
// LA METHODE, UNE SEULE POUR TOUTES LES PIECES
//   Une piece est une SOUPE de polygones (sommets partages par position, donc un
//   maillage ferme). Chaque operation de detail :
//     1. reconstruit un NkEditMesh depuis la soupe (BuildFromPolygons) ;
//     2. selectionne des faces par une REGLE GEOMETRIQUE (« la face avant »,
//        « les faces du dessus ») -- jamais par un indice suppose ;
//     3. applique L'OPERATION EXISTANTE (InsetSelectedFaces, BevelSelected,
//        SubdivideSelectedFaces, BuildSweep) ;
//     4. relit la soupe (ToPolygons).
//   Repartir d'une soupe a chaque operation rend les indices de faces stables
//   pour l'etape suivante : c'est ce qui permet de designer une face par sa
//   position plutot que par un numero qu'une operation precedente a decale.
// -----------------------------------------------------------------------------
#include "NKRenderer/Mesh/NkMeshFamilles.h"
#include "NKRenderer/Mesh/NkEditMesh.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace renderer {

		namespace {
			struct Soupe {
					NkVector<NkVertex3D> v;
					NkVector<uint32> fs; // CSR : fs[i]..fs[i+1]
					NkVector<uint32> fv;
					NkVector<NkEditMesh::FaceAttrib> fa;
					uint32 Faces() const {
						return fs.Size() ? (uint32)fs.Size() - 1u : 0u;
					}
			};

			/// Le partage de sommets ne vaut qu'A L'INTERIEUR d'un meme volume (depuis
			/// `debut`) : deux paves qui se touchent restent deux coques. Les souder
			/// ferait quatre faces sur une arete -- un maillage non manifold, que les
			/// operations de detail traiteraient mal.
			uint32 gDebut = 0;
			uint32 Sommet(Soupe &s, float32 x, float32 y, float32 z) {
				for (uint32 i = gDebut; i < (uint32)s.v.Size(); ++i) {
					const NkVec3f &p = s.v[i].pos;
					if (fabsf(p.x - x) < 1e-6f && fabsf(p.y - y) < 1e-6f && fabsf(p.z - z) < 1e-6f)
						return i;
				}
				NkVertex3D q{};
				q.pos = {x, y, z};
				q.normal = {0.f, 1.f, 0.f};
				q.tangent = {1.f, 0.f, 0.f};
				q.color = 0xFFFFFFFFu;
				s.v.PushBack(q);
				return (uint32)s.v.Size() - 1u;
			}

			void Face(Soupe &s, const uint32 *idx, uint32 n) {
				if (s.fs.Size() == 0)
					s.fs.PushBack(0u);
				for (uint32 i = 0; i < n; ++i)
					s.fv.PushBack(idx[i]);
				s.fs.PushBack((uint32)s.fv.Size());
				NkEditMesh::FaceAttrib a;
				s.fa.PushBack(a);
			}

			/// POSE UNE FACE DANS LE SENS QUI REGARDE `dehors`.
			/// ⚠️ ECRIT POUR NE PAS AVOIR A DEVINER UN SENS D'ENROULEMENT. Sur une
			///    nappe courbe, « l'ordre qui donne la normale vers le haut » change
			///    de signe avec le cote du toit, avec le sens de parcours et avec le
			///    signe de la pente. Trois occasions de se tromper en silence, pour
			///    une faute qui ne se voit qu'a l'ombrage. On CALCULE la normale de
			///    l'ordre propose, et on retourne si elle pointe du mauvais cote.
			/// ⚠️ « LE BON COTE » EST L'INVERSE DE L'INTUITION, et c'est mesure : le
			///    cube unite du modeleur a un volume signe de -1,000000 (lacet dans le
			///    sens des manuels = +1), et ce sont ses normales qui pointent dehors.
			///    L'enroulement du depot est donc l'oppose du produit vectoriel CCW
			///    standard. Le lacet de Gauss doit donc pointer VERS L'INTERIEUR pour
			///    que la normale eclairee sorte : d'ou le signe de ce test.
			void FaceVers(Soupe &s, const uint32 *idx, uint32 n, NkVec3f dehors) {
				NkVec3f nn{0.f, 0.f, 0.f};
				for (uint32 k = 0; k < n; ++k) {
					const NkVec3f &p = s.v[idx[k]].pos;
					const NkVec3f &q = s.v[idx[(k + 1u) % n]].pos;
					nn.x += (p.y - q.y) * (p.z + q.z);
					nn.y += (p.z - q.z) * (p.x + q.x);
					nn.z += (p.x - q.x) * (p.y + q.y);
				}
				if (nn.x * dehors.x + nn.y * dehors.y + nn.z * dehors.z <= 0.f) {
					Face(s, idx, n);
					return;
				}
				uint32 inv[8];
				const uint32 m = n > 8u ? 8u : n;
				for (uint32 k = 0; k < m; ++k)
					inv[k] = idx[m - 1u - k];
				Face(s, inv, m);
			}

			/// Un pave, faces sortantes : 0 -Y, 1 +Y, 2 -Z, 3 +Z, 4 -X, 5 +X.
			/// ⚠️ LES BOUCLES SONT POSEES A L'ENVERS DU LACET NATUREL, et c'est
			///    VOULU : la convention d'enroulement du depot est l'inverse de celle
			///    des manuels (le cube unite du modeleur mesure un volume signe de
			///    -1,000000, et ce sont ses normales qui pointent dehors). Ecrites
			///    dans l'ordre « evident », les faces de ce pave donnaient +0,022 pour
			///    un socle et +0,328 pour un battant : leurs normales rentraient dans
			///    la piece. Mesure du 21/09, sur l'export .obj du lot.
			void Pave(Soupe &s, float32 x0, float32 y0, float32 z0, float32 x1, float32 y1, float32 z1) {
				gDebut = (uint32)s.v.Size();
				const uint32 a = Sommet(s, x0, y0, z0), b = Sommet(s, x1, y0, z0), c = Sommet(s, x1, y0, z1),
							 d = Sommet(s, x0, y0, z1), e = Sommet(s, x0, y1, z0), f = Sommet(s, x1, y1, z0),
							 g = Sommet(s, x1, y1, z1), h = Sommet(s, x0, y1, z1);
				const uint32 F[6][4] = {{a, b, c, d}, {e, h, g, f}, {a, e, f, b}, {d, c, g, h}, {a, d, h, e}, {b, f, g, c}};
				for (int32 i = 0; i < 6; ++i) {
					const uint32 inv[4] = {F[i][3], F[i][2], F[i][1], F[i][0]};
					Face(s, inv, 4);
				}
			}

			/// Soupe -> NkEditMesh.
			void VersMaillage(const Soupe &s, NkEditMesh &m) {
				m.BuildFromPolygons(s.v.Data(), (uint32)s.v.Size(), s.fs.Data(), s.Faces(), s.fv.Data(), s.fa.Data());
			}
			/// NkEditMesh -> soupe (faces vivantes, dans l'ordre).
			void VersSoupe(const NkEditMesh &m, Soupe &s) {
				s.v.Clear();
				s.fs.Clear();
				s.fv.Clear();
				s.fa.Clear();
				m.ToPolygons(s.v, s.fs, s.fv, &s.fa);
			}

			/// AJOUTE A LA SOUPE le resultat d'un BALAYAGE (NkEditMesh::BuildSweep).
			/// Les sommets sont ajoutes SANS recherche de doublon : un balayage partage
			/// deja les siens, et deux pieces distinctes ne doivent pas se souder.
			void AjouterBalayage(Soupe &s, const NkVec2f *prof, uint32 np, const NkVec3f *chemin, uint32 nc,
								 const NkVec3f *ref = nullptr, bool bouchons = true) {
				NkEditMesh m;
				if (!m.BuildSweep(prof, np, chemin, nc, true, bouchons, ref))
					return;
				Soupe t;
				VersSoupe(m, t);
				const uint32 off = (uint32)s.v.Size();
				for (uint32 i = 0; i < (uint32)t.v.Size(); ++i)
					s.v.PushBack(t.v[i]);
				if (s.fs.Size() == 0)
					s.fs.PushBack(0u);
				for (uint32 f = 0; f < t.Faces(); ++f) {
					for (uint32 k = t.fs[f]; k < t.fs[f + 1]; ++k)
						s.fv.PushBack(t.fv[k] + off);
					s.fs.PushBack((uint32)s.fv.Size());
					s.fa.PushBack(f < (uint32)t.fa.Size() ? t.fa[f] : NkEditMesh::FaceAttrib{});
				}
				gDebut = (uint32)s.v.Size();
			}

			NkVec3f Normale(const Soupe &s, uint32 f, NkVec3f *centre) {
				NkVec3f n{0.f, 0.f, 0.f}, c{0.f, 0.f, 0.f};
				const uint32 a = s.fs[f], b = s.fs[f + 1];
				for (uint32 k = a; k < b; ++k) {
					const NkVec3f &p = s.v[s.fv[k]].pos;
					const NkVec3f &q = s.v[s.fv[k + 1 < b ? k + 1 : a]].pos;
					n.x += (p.y - q.y) * (p.z + q.z);
					n.y += (p.z - q.z) * (p.x + q.x);
					n.z += (p.x - q.x) * (p.y + q.y);
					c.x += p.x;
					c.y += p.y;
					c.z += p.z;
				}
				const float32 l = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
				if (l > 1e-12f) {
					n.x /= l;
					n.y /= l;
					n.z /= l;
				}
				if (centre) {
					const float32 k = 1.f / (float32)(b - a);
					*centre = {c.x * k, c.y * k, c.z * k};
				}
				return n;
			}

			/// LA REGLE DE SELECTION : les faces dont la normale suit `dir` (a 0,1 pres),
			/// et, si `filtre` est donne, dont le centre le satisfait.
			template <class F>
			void Selectionner(NkEditMesh &m, const Soupe &s, NkVec3f dir, F filtre) {
				NkVector<uint8> ff, vf;
				ff.Resize(s.Faces());
				vf.Resize(s.v.Size());
				for (uint32 i = 0; i < (uint32)vf.Size(); ++i)
					vf[i] = 0;
				for (uint32 f = 0; f < s.Faces(); ++f) {
					NkVec3f c;
					const NkVec3f n = Normale(s, f, &c);
					const bool ok = (n.x * dir.x + n.y * dir.y + n.z * dir.z) > 0.9f && filtre(c);
					ff[f] = ok ? 1 : 0;
					if (ok)
						for (uint32 k = s.fs[f]; k < s.fs[f + 1]; ++k)
							vf[s.fv[k]] = 1;
				}
				m.SetVertSelection(vf.Data(), (uint32)vf.Size());
				m.SetFaceSelection(ff.Data(), (uint32)ff.Size());
			}
			auto Tout = [](const NkVec3f &) { return true; };

			/// PANNEAUX EN CREUX sur les faces orientees `dir` : decoupe (subdivide
			/// `coupes`), puis inset individuel a profondeur NEGATIVE. C'est le geste
			/// « I puis E vers l'interieur » de Blender, par les operations du depot.
			void PanneauxEnCreux(Soupe &s, NkVec3f dir, int32 coupes, float32 bord, float32 creux) {
				if (coupes > 0) {
					NkEditMesh m;
					VersMaillage(s, m);
					Selectionner(m, s, dir, Tout);
					NkSubdivideParams sp;
					sp.cuts = coupes;
					m.SubdivideSelectedFaces(sp);
					VersSoupe(m, s);
				}
				NkEditMesh m;
				VersMaillage(s, m);
				Selectionner(m, s, dir, Tout);
				NkInsetParams ip;
				ip.thickness = bord;
				ip.depth = -creux;
				ip.individual = true;
				m.InsetSelectedFaces(ip);
				VersSoupe(m, s);
			}

			/// CHANFREIN des aretes des faces orientees `dir` (toutes si dir nul).
			void Chanfrein(Soupe &s, NkVec3f dir, float32 largeur, int32 segments) {
				NkEditMesh m;
				VersMaillage(s, m);
				if (dir.x == 0.f && dir.y == 0.f && dir.z == 0.f)
					m.SelectAll();
				else
					Selectionner(m, s, dir, Tout);
				NkBevelParams bp;
				bp.offset = largeur;
				bp.segments = segments;
				m.BevelSelected(bp);
				VersSoupe(m, s);
			}

			/// REPETER par le modificateur ARRAY (NkModifierStack), pas par une boucle
			/// de copies : la meme operation que la pile de modificateurs du modeleur.
			void Repeter(Soupe &s, int32 n, NkVec3f pas) {
				NkEditMesh base, out;
				VersMaillage(s, base);
				NkModifierStack pile;
				NkMeshModifier mod;
				mod.type = NkModifierType::Array;
				mod.arrayCount = n < 1 ? 1 : n;
				mod.arrayOffset = pas;
				pile.Add(mod);
				pile.Evaluate(base, out);
				VersSoupe(out, s);
			}

			struct Sortie {
					NkVector<NkFamillePiece> *out;
					int32 n = 0;
					bool ok = true;
			};

			/// Tourne (degres, X puis Z), translate, et CREE le noeud. Les sommets sont
			/// en coordonnees OBJET : le noeud nait a l'origine.
			/// `pivot` : le point (APRES la rotation X) autour duquel tourne la rotation
			/// Z -- c'est ce qui releve un coyau sur son propre coin, et non autour de
			/// l'origine de l'objet.
			void Emettre(Sortie &S, Soupe &s, const char *nom, const char *matiere, float32 rxDeg = 0.f,
						 float32 rzDeg = 0.f, NkVec3f t = {0.f, 0.f, 0.f}, NkVec3f pivot = {0.f, 0.f, 0.f}) {
				if (!S.ok)
					return;
				NkEditMesh m;
				VersMaillage(s, m);
				m.RecomputeNormals();
				NkFamillePiece piece;
				NkVector<NkEmId> otf;
				m.TriangulateShaded(piece.verts, piece.indices, otf);
				const float32 ax = rxDeg * 0.017453292f, az = rzDeg * 0.017453292f;
				const float32 cx = cosf(ax), sx = sinf(ax), cz = cosf(az), sz = sinf(az);
				auto tourner = [&](NkVec3f p) {
					NkVec3f q{p.x, p.y * cx - p.z * sx, p.y * sx + p.z * cx};
					return NkVec3f{q.x * cz - q.y * sz, q.x * sz + q.y * cz, q.z};
				};
				auto tournerAutour = [&](NkVec3f p) {
					NkVec3f q{p.x, p.y * cx - p.z * sx, p.y * sx + p.z * cx};
					q = {q.x - pivot.x, q.y - pivot.y, q.z - pivot.z};
					return NkVec3f{q.x * cz - q.y * sz + pivot.x, q.x * sz + q.y * cz + pivot.y, q.z + pivot.z};
				};
				for (uint32 i = 0; i < (uint32)piece.verts.Size(); ++i) {
					const NkVec3f p = tournerAutour(piece.verts[i].pos);
					piece.verts[i].pos = {p.x + t.x, p.y + t.y, p.z + t.z};
					piece.verts[i].normal = tourner(piece.verts[i].normal);
				}
				snprintf(piece.nom, sizeof(piece.nom), "%s", nom);
				snprintf(piece.matiere, sizeof(piece.matiere), "%s", matiere);
				piece.faces = s.Faces();
				S.out->PushBack(piece);
				++S.n;
			}

			/// Une piece TOURNEE (SpinSelected) : profil (rayon, hauteur) du bas vers le
			/// haut, axe vertical, posee en `t`. Pleine : on ferme sur l'axe.
			void Tournee(Sortie &S, const float32 *rh, int32 n, const char *nom, const char *matiere, NkVec3f t,
						 float32 rxDeg = 0.f) {
				if (!S.ok)
					return;
				// Le profil, du bas vers le haut, ferme sur l'axe aux deux bouts.
				// ⚠️ PAS DE RAYON NUL EN FIN DE LISTE : la fermeture est ajoutee ici, et
				//    un point (0, h) ecrit par l'appelant ferait DOUBLON -- une surface
				//    etanche mais non simple (mesure du 21/09 sur les epis du toit).
				NkVector<NkVertex3D> v;
				NkVector<uint32> fs, fv;
				auto pt = [&](float32 r, float32 h) {
					NkVertex3D q{};
					q.pos = {r < 0.f ? 0.f : r, h, 0.f};
					q.normal = {0.f, 0.f, 1.f};
					q.tangent = {1.f, 0.f, 0.f};
					q.color = 0xFFFFFFFFu;
					v.PushBack(q);
				};
				pt(0.f, rh[1]);
				for (int32 k = 0; k < n; ++k)
					pt(rh[2 * k], rh[2 * k + 1]);
				pt(0.f, rh[2 * (n - 1) + 1]);
				const uint32 np = (uint32)v.Size();
				fs.PushBack(0u);
				for (uint32 k = 0; k < np; ++k)
					fv.PushBack(k);
				fs.PushBack(np);
				NkEditMesh m;
				m.BuildFromPolygons(v.Data(), np, fs.Data(), 1u, fv.Data());
				m.SelectAll();
				NkSpinParams sp;
				sp.center = {0.f, 0.f, 0.f};
				sp.axis = {0.f, 1.f, 0.f};
				sp.angle = 6.2831853f;
				sp.steps = 32;
				sp.duplicate = false;
				if (!m.SpinSelected(sp, NkMat4f::Identity())) {
					S.ok = false;
					return;
				}
				m.RebuildEdges();
				m.RecomputeNormals();
				NkFamillePiece piece;
				NkVector<NkEmId> otf;
				m.TriangulateShaded(piece.verts, piece.indices, otf);
				// La face-profil d'origine survit au spin (comme dans Blender) : couchee
				// dans le plan x/y, elle serait une cloison DANS la paroi. On retire ses
				// triangles du tampon de rendu, sans toucher a la topologie.
				{
					NkVector<uint32> gard;
					for (uint32 k = 0; k + 2 < (uint32)piece.indices.Size(); k += 3) {
						if (otf[k / 3] == (NkEmId)0)
							continue;
						gard.PushBack(piece.indices[k]);
						gard.PushBack(piece.indices[k + 1]);
						gard.PushBack(piece.indices[k + 2]);
					}
					if (gard.Size() >= 3)
						piece.indices = gard;
				}
				const float32 ax = rxDeg * 0.017453292f, cx = cosf(ax), sx = sinf(ax);
				for (uint32 i = 0; i < (uint32)piece.verts.Size(); ++i) {
					const NkVec3f p = piece.verts[i].pos, nn = piece.verts[i].normal;
					piece.verts[i].pos = {p.x + t.x, p.y * cx - p.z * sx + t.y, p.y * sx + p.z * cx + t.z};
					piece.verts[i].normal = {nn.x, nn.y * cx - nn.z * sx, nn.y * sx + nn.z * cx};
				}
				snprintf(piece.nom, sizeof(piece.nom), "%s", nom);
				snprintf(piece.matiere, sizeof(piece.matiere), "%s", matiere);
				piece.faces = (uint32)(np * 32);
				S.out->PushBack(piece);
				++S.n;
			}

			float32 Borne(float32 v, float32 lo, float32 hi, float32 def) {
				if (!(v > 0.f))
					return def;
				return v < lo ? lo : (v > hi ? hi : v);
			}

			// ================================================================
			//  LE TOIT CHINOIS : UNE COURBE, PAS DES PLANCHES (Q9.3)
			// ================================================================
			// CE QUI N'ALLAIT PAS, ET QUI EST VISIBLE SUR L'IMAGE DU 21/09
			// Le toit etait fait de deux paves inclines plus QUATRE PLANCHES tournees
			// de 20 degres autour d'un coin, appelees « coyaux ». Resultat : quatre
			// lames qui volent a cote du toit sans le toucher, aucun faitage visible,
			// et une silhouette droite la ou un toit chinois est COURBE. Le releve des
			// coins n'est pas un morceau qu'on ajoute : c'est la NAPPE ELLE-MEME qui
			// se releve, continument, en approchant du pignon.
			//
			// LA NAPPE : pour chaque station le long du faitage (u) et chaque station
			// de la pente (t), un point. La section de pente est une BEZIER CUBIQUE --
			// raide au faitage, qui s'aplatit puis se retrousse a l'egout. Le releve
			// des coins est un terme qui croit en u^4 : nul au milieu, franc au bout.
			// La nappe est donc SYMETRIQUE par construction (u et -u donnent le meme
			// releve, +z et -z la meme section) et son faitage est HORIZONTAL (t = 0
			// donne y = 0 quel que soit u).
			struct CourbeToit {
					float32 xm = 1.f;	  // demi-longueur, le long du faitage
					float32 larg = 1.f;	  // profondeur horizontale d'un versant
					float32 chute = 0.6f; // denivele du faitage a l'egout
					float32 releve = 0.4f;	// montee supplementaire du coin
					float32 sortie = 0.15f; // debord supplementaire du coin
					float32 sgn = 1.f;		// +1 versant avant, -1 versant arriere

					/// u dans [-1, 1] le long du faitage, t dans [0, 1] du faitage a l'egout.
					NkVec3f P(float32 u, float32 t) const {
						const float32 m = 1.f - t;
						// Bezier cubique (z', y') : P0 au faitage, P3 a l'egout retrousse.
						const float32 b0 = m * m * m, b1 = 3.f * m * m * t, b2 = 3.f * m * t * t, b3 = t * t * t;
						float32 z = b1 * (0.30f * larg) + b2 * (0.80f * larg) + b3 * larg;
						float32 y = b1 * (-0.62f * chute) + b2 * (-1.00f * chute) + b3 * (-0.88f * chute);
						(void)b0;
						const float32 f = u * u * u * u; // le releve ne prend qu'aux bouts
						y += releve * f * t * t * t;
						z *= 1.f + sortie * f * t * t;
						return {u * xm, y, sgn * z};
					}
			};

			/// LA NAPPE, en volume : la surface du dessus, celle du dessous decalee de
			/// `ep` vers le bas, et les quatre bandes de rive qui les relient.
			/// Le sens de chaque face est decide par FaceVers, pas par un ordre devine.
			void NappeToit(Soupe &s, const CourbeToit &c, float32 ep, int32 nu, int32 nv) {
				if (nu < 2)
					nu = 2;
				if (nv < 2)
					nv = 2;
				gDebut = (uint32)s.v.Size();
				const uint32 off = (uint32)s.v.Size();
				auto pousser = [&](NkVec3f p) {
					NkVertex3D q{};
					q.pos = p;
					q.normal = {0.f, 1.f, 0.f};
					q.tangent = {1.f, 0.f, 0.f};
					q.color = 0xFFFFFFFFu;
					s.v.PushBack(q);
				};
				for (int32 i = 0; i < nu; ++i)
					for (int32 j = 0; j < nv; ++j)
						pousser(c.P(2.f * (float32)i / (float32)(nu - 1) - 1.f, (float32)j / (float32)(nv - 1)));
				for (int32 i = 0; i < nu; ++i)
					for (int32 j = 0; j < nv; ++j) {
						NkVec3f p = c.P(2.f * (float32)i / (float32)(nu - 1) - 1.f, (float32)j / (float32)(nv - 1));
						p.y -= ep;
						pousser(p);
					}
				const uint32 nb = (uint32)(nu * nv);
				auto H = [&](int32 i, int32 j) { return off + (uint32)(i * nv + j); };
				auto B = [&](int32 i, int32 j) { return off + nb + (uint32)(i * nv + j); };
				for (int32 i = 0; i + 1 < nu; ++i)
					for (int32 j = 0; j + 1 < nv; ++j) {
						const uint32 h[4] = {H(i, j), H(i, j + 1), H(i + 1, j + 1), H(i + 1, j)};
						FaceVers(s, h, 4, {0.f, 1.f, 0.f});
						const uint32 b[4] = {B(i, j), B(i, j + 1), B(i + 1, j + 1), B(i + 1, j)};
						FaceVers(s, b, 4, {0.f, -1.f, 0.f});
					}
				// rives : egout (t = 1), faitage (t = 0), et les deux pignons
				for (int32 i = 0; i + 1 < nu; ++i) {
					const uint32 e[4] = {H(i, nv - 1), H(i + 1, nv - 1), B(i + 1, nv - 1), B(i, nv - 1)};
					FaceVers(s, e, 4, {0.f, 0.f, c.sgn});
					const uint32 f[4] = {H(i, 0), H(i + 1, 0), B(i + 1, 0), B(i, 0)};
					FaceVers(s, f, 4, {0.f, 0.f, -c.sgn});
				}
				for (int32 j = 0; j + 1 < nv; ++j) {
					const uint32 g[4] = {H(0, j), H(0, j + 1), B(0, j + 1), B(0, j)};
					FaceVers(s, g, 4, {-1.f, 0.f, 0.f});
					const uint32 d[4] = {H(nu - 1, j), H(nu - 1, j + 1), B(nu - 1, j + 1), B(nu - 1, j)};
					FaceVers(s, d, 4, {1.f, 0.f, 0.f});
				}
			}

			/// LES TUILES CANAL : un demi-rond BALAYE le long de la pente, une fois par
			/// tuile. Chaque tuile suit SA propre section, donc elle se releve avec le
			/// coin -- ce qu'un modificateur Array n'aurait pas su faire, puisqu'il
			/// recopie une forme a l'identique. La reference initiale du repere (l'axe
			/// X) est ce qui met le dos de la tuile EN HAUT.
			void TuilesCanal(Soupe &s, const CourbeToit &c, float32 ep, float32 pas, int32 nv) {
				const float32 rt = pas * 0.42f;
				NkVec2f prof[7];
				for (int32 k = 0; k < 7; ++k) {
					const float32 a = 3.14159265f * (1.f - (float32)k / 6.f);
					prof[k] = {rt * cosf(a), rt * sinf(a) * 0.85f};
				}
				const int32 n = (int32)(2.f * c.xm / pas);
				// ⚠️ LE SIGNE DU COTE ENTRE DANS LA REFERENCE, et il a ete MESURE, pas
				//    devine : avec +X pour les deux versants, la boite des tuiles
				//    arriere sortait a y 3.8599..4.4440 quand celle des tuiles avant
				//    donnait 3.9028..4.4738 -- l'arriere s'enfoncait de 4 cm dans la
				//    nappe. Cause : la binormale vaut T x r, et la tangente change de
				//    signe en z d'un versant a l'autre ; le dos de la tuile se
				//    retournait donc VERS LE BAS. Le meme dessin, lu de l'autre cote,
				//    demande la reference opposee.
				const NkVec3f axeX{c.sgn, 0.f, 0.f};
				NkVector<NkVec3f> ch;
				ch.Resize(nv);
				for (int32 i = 0; i < n; ++i) {
					const float32 u = (2.f * ((float32)i + 0.5f) / (float32)n - 1.f);
					if (fabsf(u) * c.xm + rt > c.xm)
						continue;
					for (int32 j = 0; j < nv; ++j) {
						NkVec3f p = c.P(u, (float32)j / (float32)(nv - 1));
						p.y += 0.004f; // la tuile POSE sur la nappe
						ch[j] = p;
					}
					AjouterBalayage(s, prof, 7, ch.Data(), (uint32)nv, &axeX, true);
				}
				(void)ep;
			}

			/// LA RIVE D'EGOUT : un rond balaye LE LONG DE LA LIGNE D'EGOUT, qui monte
			/// aux deux bouts. C'est la piece que l'oeil lit comme « le coyau releve »,
			/// et c'est une COURBE 3D : ni une revolution, ni une extrusion droite.
			void RiveEgout(Soupe &s, const CourbeToit &c, float32 rayon, int32 nu) {
				NkVec2f prof[8];
				for (int32 k = 0; k < 8; ++k) {
					const float32 a = 6.2831853f * (float32)k / 8.f;
					prof[k] = {rayon * cosf(a), rayon * sinf(a)};
				}
				NkVector<NkVec3f> ch;
				ch.Resize(nu);
				for (int32 i = 0; i < nu; ++i) {
					NkVec3f p = c.P(2.f * (float32)i / (float32)(nu - 1) - 1.f, 1.f);
					p.y -= rayon * 0.4f;
					ch[i] = p;
				}
				const NkVec3f haut{0.f, 1.f, 0.f};
				AjouterBalayage(s, prof, 8, ch.Data(), (uint32)nu, &haut, true);
			}

			// ================================================================
			//  UN VERSANT DE TOIT, et ses rangs de tuiles
			// ================================================================
			/// Le versant est construit dans SON repere (x le long du faitage, z le long
			/// de la pente, du faitage vers l'egout) puis tourne de `pente` autour de X
			/// et pose au faitage (y = yF). `cote` +1 : descend vers +z ; -1 : vers -z.
			/// Les tuiles : UN rang, repete par le modificateur Array le long de la pente.
			void Versant(Sortie &S, const char *nom, const char *nomTuiles, float32 x0, float32 x1, float32 larg,
						 float32 ep, float32 pente, float32 cote, float32 yF, bool tuiles, float32 pas, bool canal) {
				const float32 za = cote > 0.f ? 0.f : -larg, zb = cote > 0.f ? larg : 0.f;
				const float32 rx = cote * pente;
				Soupe s;
				Pave(s, x0, -ep, za, x1, 0.f, zb);
				Emettre(S, s, nom, "tuile", rx, 0.f, {0.f, yF, 0.f});
				if (!tuiles)
					return;
				Soupe t;
				if (canal) {
					// tuiles CANAL (chinoises) : une tuile ronde, repetee le long du faitage
					Pave(t, x0 + 0.01f, 0.f, za, x0 + 0.06f, 0.045f, zb);
					Chanfrein(t, {0.f, 1.f, 0.f}, 0.02f, 2);
					Repeter(t, (int32)((x1 - x0 - 0.02f) / pas), {pas, 0.f, 0.f});
				} else {
					// tuiles PLATES : un rang, repete le long de la pente
					const float32 z0 = cote > 0.f ? 0.f : -0.1f;
					Pave(t, x0, 0.f, z0, x1, 0.035f, z0 + 0.1f);
					Chanfrein(t, {0.f, 0.f, cote}, 0.02f, 1);
					Repeter(t, (int32)(larg / pas), {0.f, 0.f, cote * pas});
				}
				Emettre(S, t, nomTuiles, "tuile", rx, 0.f, {0.f, yF, 0.f});
			}

			// ================================================================
			//  UN BATTANT DE PORTE, avec ses panneaux en creux
			// ================================================================
			void Battant(Sortie &S, const char *nom, float32 x0, float32 x1, float32 y0, float32 y1, float32 ep,
						 bool detail, const char *mat, float32 zc = 0.f) {
				Soupe s;
				Pave(s, x0, y0, zc - ep * 0.5f, x1, y1, zc + ep * 0.5f);
				if (detail) {
					const float32 w = x1 - x0;
					// quatre panneaux par face (subdivide 1), bord = 12 % de la largeur,
					// creux de 12 mm : sur les DEUX faces, comme une vraie porte.
					PanneauxEnCreux(s, {0.f, 0.f, 1.f}, 1, w * 0.12f, 0.012f);
					PanneauxEnCreux(s, {0.f, 0.f, -1.f}, 1, w * 0.12f, 0.012f);
				}
				Emettre(S, s, nom, mat);
			}

			// ================================================================
			//  PORTE / PORTAIL
			// ================================================================
			void Porte(Sortie &S, const NkFamilleParams &p) {
				const bool chinois = strcmp(p.style, "chinois") == 0;
				const float32 W = Borne(p.largeur, 0.6f, 8.f, chinois ? 2.4f : 0.9f);
				const float32 H = Borne(p.hauteur, 1.6f, 10.f, chinois ? 3.2f : 2.1f);
				const int32 nb = p.nombre == 1 ? 1 : (p.nombre >= 2 ? 2 : (chinois ? 2 : 1));
				const bool det = p.detaille;
				const float32 ep = 0.05f;
				if (!chinois) {
					// ── cadre : deux montants et une traverse, chanfreines ──
					const float32 f = 0.08f, d = 0.14f;
					const char *const noms[3] = {"montant_gauche", "montant_droit", "traverse"};
					for (int32 k = 0; k < 3; ++k) {
						Soupe s;
						if (k == 0)
							Pave(s, -W * 0.5f, 0.f, -d * 0.5f, -W * 0.5f + f, H, d * 0.5f);
						else if (k == 1)
							Pave(s, W * 0.5f - f, 0.f, -d * 0.5f, W * 0.5f, H, d * 0.5f);
						else
							Pave(s, -W * 0.5f, H - f, -d * 0.5f, W * 0.5f, H, d * 0.5f);
						if (det)
							Chanfrein(s, {0.f, 0.f, 0.f}, 0.006f, 1);
						Emettre(S, s, noms[k], "bois_sombre");
					}
					const float32 x0 = -W * 0.5f + f, x1 = W * 0.5f - f, jeu = 0.004f;
					for (int32 b = 0; b < nb; ++b) {
						const float32 bx0 = x0 + (x1 - x0) * b / nb + jeu, bx1 = x0 + (x1 - x0) * (b + 1) / nb - jeu;
						Battant(S, nb == 1 ? "battant" : (b == 0 ? "battant_gauche" : "battant_droit"), bx0, bx1, 0.01f,
								H - f - jeu, ep, det, "bois");
						if (det) {
							// POIGNEE : une rosace tournee et un bec, cote ouverture.
							const float32 hx = (nb == 1 || b == 1) ? bx0 + 0.07f : bx1 - 0.07f;
							const float32 ros[4] = {0.028f, 0.f, 0.024f, 0.012f};
							Tournee(S, ros, 2, b == 0 ? "rosace" : "rosace_2", "laiton", {hx, 1.0f, ep * 0.5f}, 90.f);
							Soupe bec;
							const float32 sens = (nb == 1 || b == 1) ? 1.f : -1.f;
							Pave(bec, hx, 0.99f, ep * 0.5f + 0.012f, hx + sens * 0.12f, 1.01f, ep * 0.5f + 0.03f);
							Chanfrein(bec, {0.f, 0.f, 0.f}, 0.004f, 1);
							Emettre(S, bec, b == 0 ? "poignee" : "poignee_2", "laiton");
						}
					}
					return;
				}
				// ── PORTE CHINOISE ANCIENNE : poteaux laques, linteaux sculptes, toit a
				//    coyaux releves couvert de tuiles, deux battants cloutes ──
				const float32 r = 0.11f;				// rayon des poteaux
				const float32 hp = H;					// hauteur des poteaux
				const float32 xp = W * 0.5f + r;		// axe des poteaux
				for (int32 k = 0; k < 2; ++k) {
					const float32 sx = k == 0 ? -xp : xp;
					Soupe socle;
					Pave(socle, sx - r * 1.6f, 0.f, -r * 1.6f, sx + r * 1.6f, 0.18f, r * 1.6f);
					if (det)
						Chanfrein(socle, {0.f, 1.f, 0.f}, 0.03f, 2);
					Emettre(S, socle, k == 0 ? "socle_gauche" : "socle_droit", "pierre");
					const float32 pot[6] = {r, 0.f, r * 0.94f, hp - 0.18f, r * 1.05f, hp - 0.12f};
					Tournee(S, pot, 3, k == 0 ? "poteau_gauche" : "poteau_droit", "laque_rouge", {sx, 0.18f, 0.f});
				}
				// linteaux : bas (sur les poteaux) et haut, panneaux sculptes en creux
				const float32 L = 2.f * xp + 2.f * r * 1.3f;
				const float32 yL1 = hp + 0.06f, hL1 = 0.22f, yL2 = yL1 + hL1 + 0.18f, hL2 = 0.26f;
				{
					Soupe s;
					Pave(s, -L * 0.5f, yL1, -0.13f, L * 0.5f, yL1 + hL1, 0.13f);
					if (det) {
						PanneauxEnCreux(s, {0.f, 0.f, 1.f}, 2, 0.03f, 0.018f);
						Chanfrein(s, {0.f, -1.f, 0.f}, 0.015f, 1);
					}
					Emettre(S, s, "linteau_bas", "laque_rouge");
				}
				{
					// la plaque au nom, entre les deux linteaux
					Soupe s;
					Pave(s, -0.45f, yL1 + hL1, 0.02f, 0.45f, yL2, 0.1f);
					if (det)
						PanneauxEnCreux(s, {0.f, 0.f, 1.f}, 0, 0.04f, 0.01f);
					Emettre(S, s, "plaque", "or");
				}
				{
					Soupe s;
					Pave(s, -L * 0.55f, yL2, -0.15f, L * 0.55f, yL2 + hL2, 0.15f);
					if (det) {
						PanneauxEnCreux(s, {0.f, 0.f, 1.f}, 2, 0.03f, 0.02f);
						PanneauxEnCreux(s, {0.f, 0.f, -1.f}, 2, 0.03f, 0.02f);
					}
					Emettre(S, s, "linteau_haut", "laque_rouge");
				}
				// ── LE TOIT (Q9.3) : deux nappes COURBES, symetriques, faitage
				//    horizontal, coins releves par la nappe elle-meme ──
				const float32 yT = yL2 + hL2;
				CourbeToit c;
				c.xm = L * 0.66f;
				c.larg = 1.05f;
				c.chute = 0.60f;
				c.releve = 0.40f;
				c.sortie = 0.16f;
				const float32 epT = 0.07f;
				const float32 yF = yT + 0.52f; // LE FAITAGE : une seule hauteur, pour les deux versants
				const int32 nu = det ? 19 : 7, nv = det ? 9 : 5;
				for (int32 cote = 0; cote < 2; ++cote) {
					c.sgn = cote == 0 ? 1.f : -1.f;
					const bool av = cote == 0;
					{
						Soupe s2;
						NappeToit(s2, c, epT, nu, nv);
						Emettre(S, s2, av ? "versant_avant" : "versant_arriere", "tuile", 0.f, 0.f, {0.f, yF, 0.f});
					}
					if (det) {
						Soupe t;
						TuilesCanal(t, c, epT, 0.12f, nv);
						Emettre(S, t, av ? "tuiles_avant" : "tuiles_arriere", "tuile", 0.f, 0.f, {0.f, yF, 0.f});
					}
					{
						Soupe r2;
						RiveEgout(r2, c, 0.045f, nu);
						Emettre(S, r2, av ? "rive_avant" : "rive_arriere", "laque_rouge", 0.f, 0.f, {0.f, yF, 0.f});
					}
				}
				{
					// LE FAITAGE : un demi-rond balaye le long d'un chemin DROIT et
					// horizontal. C'est le cas ou un repere de Frenet n'existe pas
					// (courbure nulle) -- voir le banc sweep/chemin-droit.
					const float32 hr = 0.10f, rr = 0.09f;
					NkVec2f prof[9];
					for (int32 k = 0; k < 9; ++k) {
						const float32 a = 3.14159265f * (float32)k / 8.f;
						prof[k] = {hr * sinf(a), -rr * cosf(a)};
					}
					const NkVec3f ch[2] = {{-c.xm - 0.06f, 0.f, 0.f}, {c.xm + 0.06f, 0.f, 0.f}};
					const NkVec3f haut{0.f, 1.f, 0.f};
					Soupe f;
					AjouterBalayage(f, prof, 9, ch, 2, &haut, true);
					Emettre(S, f, "faitage", "tuile", 0.f, 0.f, {0.f, yF + 0.005f, 0.f});
					// les deux epis, aux bouts du faitage
					if (det)
						for (int32 k = 0; k < 2; ++k) {
							const float32 ex = k == 0 ? -c.xm - 0.05f : c.xm + 0.05f;
							// ⚠️ PAS DE RAYON NUL EN FIN DE PROFIL : Tournee ferme deja sur
							//    l'axe, et le point (0, h) ecrit a la main faisait DOUBLON.
							//    Le maillage restait etanche (0 arete de bord) mais cessait
							//    d'etre simple : les deux epis, miroir l'un de l'autre,
							//    mesuraient +0,0073 et +0,0021 -- deux volumes differents
							//    pour la meme piece, ce qu'un volume signe ne peut pas faire
							//    sur une surface propre.
							const float32 ep2[6] = {0.07f, 0.f, 0.05f, 0.08f, 0.09f, 0.14f};
							Tournee(S, ep2, 3, k == 0 ? "epi_gauche" : "epi_droit", "tuile", {ex, yF + 0.08f, 0.f});
						}
					// le pan de mur sous le toit, entre linteau et versants
					Soupe m;
					Pave(m, -L * 0.5f, yT, -0.12f, L * 0.5f, yF - c.chute * 0.18f, 0.12f);
					Emettre(S, m, "frise", "laque_rouge");
				}
				// les battants, entre les poteaux, et leurs anneaux
				const float32 x0 = -xp + r, x1 = xp - r;
				for (int32 b = 0; b < nb; ++b) {
					const float32 bx0 = x0 + (x1 - x0) * b / nb + 0.005f, bx1 = x0 + (x1 - x0) * (b + 1) / nb - 0.005f;
					Battant(S, nb == 1 ? "battant" : (b == 0 ? "battant_gauche" : "battant_droit"), bx0, bx1, 0.02f, hp,
							0.07f, det, "laque_rouge");
					if (det) {
						const float32 hx = (b == 0) ? bx1 - 0.14f : bx0 + 0.14f;
						const float32 ann[6] = {0.07f, 0.f, 0.07f, 0.015f, 0.05f, 0.03f};
						Tournee(S, ann, 3, b == 0 ? "heurtoir_g" : "heurtoir_d", "laiton", {hx, 1.25f, 0.035f}, 90.f);
					}
				}
			}

			// ================================================================
			//  TABLE
			// ================================================================
			void Table(Sortie &S, const NkFamilleParams &p) {
				const float32 W = Borne(p.largeur, 0.4f, 4.f, 1.6f), D = Borne(p.profondeur, 0.4f, 2.f, 0.9f);
				const float32 H = Borne(p.hauteur, 0.3f, 1.2f, 0.75f);
				const float32 e = 0.035f, retrait = 0.05f;
				const bool det = p.detaille;
				{
					Soupe s;
					Pave(s, -W * 0.5f, H - e, -D * 0.5f, W * 0.5f, H, D * 0.5f);
					if (det)
						Chanfrein(s, {0.f, 1.f, 0.f}, 0.01f, 2); // aretes du dessus adoucies
					Emettre(S, s, "plateau", "bois");
				}
				const float32 lx = W * 0.5f - retrait - 0.04f, lz = D * 0.5f - retrait - 0.04f;
				const float32 hp = H - e;
				for (int32 k = 0; k < 4; ++k) {
					const float32 x = (k & 1) ? lx : -lx, z = (k & 2) ? lz : -lz;
					static const char *const noms[4] = {"pied_arriere_gauche", "pied_arriere_droit", "pied_avant_gauche",
														 "pied_avant_droit"};
					if (det) {
						// PIED FUSELE, tourne : collier en haut, fuseau, bague, sabot
						const float32 f[16] = {0.020f, 0.f,		   0.022f, 0.03f,		0.018f, 0.04f,	 0.026f, hp * 0.55f,
											   0.032f, hp * 0.62f, 0.026f, hp * 0.66f, 0.034f, hp * 0.80f, 0.034f, hp};
						Tournee(S, f, 8, noms[k], "bois", {x, 0.f, z});
					} else {
						Soupe s;
						Pave(s, x - 0.035f, 0.f, z - 0.035f, x + 0.035f, hp, z + 0.035f);
						Emettre(S, s, noms[k], "bois");
					}
				}
				if (det) {
					// CEINTURE en retrait sous le plateau : quatre traverses
					const float32 hc = 0.09f, t = 0.022f;
					Soupe s;
					Pave(s, -lx, hp - hc, lz - t * 0.5f, lx, hp, lz + t * 0.5f);
					Pave(s, -lx, hp - hc, -lz - t * 0.5f, lx, hp, -lz + t * 0.5f);
					Pave(s, -lx - t * 0.5f, hp - hc, -lz, -lx + t * 0.5f, hp, lz);
					Pave(s, lx - t * 0.5f, hp - hc, -lz, lx + t * 0.5f, hp, lz);
					Chanfrein(s, {0.f, -1.f, 0.f}, 0.006f, 1);
					Emettre(S, s, "ceinture", "bois");
				}
			}

			// ================================================================
			//  MAISON
			// ================================================================
			/// Une FACADE percee : des trumeaux et des allèges autour des ouvertures,
			/// donc de vrais trous -- les embrasures sont dans l'epaisseur du mur.
			/// La facade est dans le plan x/y, epaisseur de z0 a z1.
			struct Ouverture {
					float32 x0, x1, y0, y1;
			};
			void Facade(Soupe &s, float32 X0, float32 X1, float32 H, float32 z0, float32 z1, const Ouverture *o, int32 no) {
				// les bornes en x : bords de la facade et de chaque ouverture
				float32 xs[64];
				int32 nx = 0;
				xs[nx++] = X0;
				xs[nx++] = X1;
				for (int32 i = 0; i < no && nx < 62; ++i) {
					xs[nx++] = o[i].x0;
					xs[nx++] = o[i].x1;
				}
				for (int32 a = 1; a < nx; ++a)
					for (int32 b = a; b > 0 && xs[b - 1] > xs[b]; --b) {
						const float32 t = xs[b];
						xs[b] = xs[b - 1];
						xs[b - 1] = t;
					}
				for (int32 c = 0; c + 1 < nx; ++c) {
					const float32 a = xs[c], b = xs[c + 1];
					if (b - a < 1e-4f)
						continue;
					// les intervalles de y occupes par des ouvertures dans cette colonne
					float32 ys[64];
					int32 ny = 0;
					ys[ny++] = 0.f;
					for (int32 i = 0; i < no && ny < 62; ++i)
						if (o[i].x0 <= a + 1e-4f && o[i].x1 >= b - 1e-4f) {
							ys[ny++] = o[i].y0;
							ys[ny++] = o[i].y1;
						}
					ys[ny++] = H;
					for (int32 u = 1; u < ny; ++u)
						for (int32 v = u; v > 0 && ys[v - 1] > ys[v]; --v) {
							const float32 t = ys[v];
							ys[v] = ys[v - 1];
							ys[v - 1] = t;
						}
					// plein, trou, plein, trou, plein...
					for (int32 k = 0; k + 1 < ny; k += 2)
						if (ys[k + 1] - ys[k] > 1e-4f)
							Pave(s, a, ys[k], z0, b, ys[k + 1], z1);
				}
			}

			void Maison(Sortie &S, const NkFamilleParams &p) {
				const float32 W = Borne(p.largeur, 4.f, 30.f, 9.f), D = Borne(p.profondeur, 3.f, 20.f, 7.f);
				const int32 et = p.nombre < 1 ? 1 : (p.nombre > 4 ? 4 : p.nombre);
				const float32 he = 2.9f, H = he * et, t = 0.3f;
				const int32 nf = p.fenetres < 1 ? 2 : (p.fenetres > 8 ? 8 : p.fenetres);
				const bool plat = strcmp(p.style, "plat") == 0;
				const bool det = p.detaille;
				const float32 fw = 1.1f, fh = 1.3f, fy = 0.95f; // fenetre : largeur, hauteur, allege
				if (!det) {
					Soupe s;
					Pave(s, -W * 0.5f, 0.f, -D * 0.5f, W * 0.5f, H, D * 0.5f);
					Emettre(S, s, "murs", "platre");
					Soupe po;
					Pave(po, -0.5f, 0.f, D * 0.5f, 0.5f, 2.2f, D * 0.5f + 0.05f);
					Emettre(S, po, "porte", "bois_sombre");
					Soupe fe;
					for (int32 e = 0; e < et; ++e)
						for (int32 i = 0; i < nf; ++i) {
							const float32 cx = -W * 0.5f + W * (i + 0.5f) / nf;
							if (e == 0 && fabsf(cx) < 1.0f)
								continue;
							Pave(fe, cx - fw * 0.5f, e * he + fy, D * 0.5f, cx + fw * 0.5f, e * he + fy + fh, D * 0.5f + 0.04f);
						}
					Emettre(S, fe, "fenetres", "verre");
				} else {
					// ── LES QUATRE MURS PERCES : de vrais trous, embrasures dans l'epaisseur ──
					Ouverture av[40], ar[40];
					int32 nav = 0, nar = 0;
					for (int32 e = 0; e < et; ++e)
						for (int32 i = 0; i < nf; ++i) {
							const float32 cx = -W * 0.5f + W * (i + 0.5f) / nf;
							if (e == 0 && fabsf(cx) < 1.0f)
								continue; // la place de la porte
							av[nav++] = {cx - fw * 0.5f, cx + fw * 0.5f, e * he + fy, e * he + fy + fh};
							ar[nar++] = {cx - fw * 0.5f, cx + fw * 0.5f, e * he + fy, e * he + fy + fh};
						}
					av[nav++] = {-0.55f, 0.55f, 0.f, 2.25f}; // la porte
					Soupe fa, fr, cg, cd;
					Facade(fa, -W * 0.5f, W * 0.5f, H, D * 0.5f - t, D * 0.5f, av, nav);
					Facade(fr, -W * 0.5f, W * 0.5f, H, -D * 0.5f, -D * 0.5f + t, ar, nar);
					Emettre(S, fa, "facade_avant", "platre");
					Emettre(S, fr, "facade_arriere", "platre");
					Pave(cg, -W * 0.5f, 0.f, -D * 0.5f + t, -W * 0.5f + t, H, D * 0.5f - t);
					Pave(cd, W * 0.5f - t, 0.f, -D * 0.5f + t, W * 0.5f, H, D * 0.5f - t);
					Emettre(S, cg, "pignon_gauche", "platre");
					Emettre(S, cd, "pignon_droit", "platre");
					// ── FENETRES DANS LEUR EMBRASURE : cadre en retrait, vitre, appui saillant ──
					Soupe cadres, vitres, appuis;
					for (int32 k = 0; k < nav + nar; ++k) {
						const bool avant = k < nav;
						const Ouverture &o = avant ? av[k] : ar[k - nav];
						if (avant && k == nav - 1)
							continue; // la porte a son propre traitement
						const float32 zf = avant ? D * 0.5f - t * 0.55f : -D * 0.5f + t * 0.55f;
						const float32 c = 0.06f;
						Pave(cadres, o.x0, o.y0, zf - 0.03f, o.x1, o.y0 + c, zf + 0.03f);
						Pave(cadres, o.x0, o.y1 - c, zf - 0.03f, o.x1, o.y1, zf + 0.03f);
						Pave(cadres, o.x0, o.y0 + c, zf - 0.03f, o.x0 + c, o.y1 - c, zf + 0.03f);
						Pave(cadres, o.x1 - c, o.y0 + c, zf - 0.03f, o.x1, o.y1 - c, zf + 0.03f);
						const float32 xm = (o.x0 + o.x1) * 0.5f;
						Pave(cadres, xm - 0.025f, o.y0 + c, zf - 0.025f, xm + 0.025f, o.y1 - c, zf + 0.025f); // meneau
						Pave(vitres, o.x0 + c, o.y0 + c, zf - 0.006f, o.x1 - c, o.y1 - c, zf + 0.006f);
						const float32 zo = avant ? D * 0.5f : -D * 0.5f, sgn = avant ? 1.f : -1.f;
						const float32 za = zo - sgn * t * 0.6f, zb = zo + sgn * 0.06f;
						Pave(appuis, o.x0 - 0.05f, o.y0 - 0.06f, za < zb ? za : zb, o.x1 + 0.05f, o.y0, za < zb ? zb : za);
					}
					Chanfrein(appuis, {0.f, 1.f, 0.f}, 0.012f, 1);
					Emettre(S, cadres, "cadres", "blanc");
					Emettre(S, vitres, "vitres", "verre");
					Emettre(S, appuis, "appuis", "pierre");
					// la porte d'entree, en retrait, a panneaux
					// la porte d'entree, EN RETRAIT dans son embrasure
					Battant(S, "porte_entree", -0.52f, 0.52f, 0.02f, 2.22f, 0.06f, true, "bois_sombre", D * 0.5f - t * 0.5f);
					// ── BANDEAUX entre etages, CORNICHE en haut ──
					Soupe band;
					for (int32 e = 1; e <= et; ++e) {
						const float32 y = e * he, h = e == et ? 0.22f : 0.12f, sv = e == et ? 0.16f : 0.07f;
						Pave(band, -W * 0.5f - sv, y - h, D * 0.5f, W * 0.5f + sv, y, D * 0.5f + sv);
						Pave(band, -W * 0.5f - sv, y - h, -D * 0.5f - sv, W * 0.5f + sv, y, -D * 0.5f);
						Pave(band, -W * 0.5f - sv, y - h, -D * 0.5f, -W * 0.5f, y, D * 0.5f);
						Pave(band, W * 0.5f, y - h, -D * 0.5f, W * 0.5f + sv, y, D * 0.5f);
					}
					Chanfrein(band, {0.f, -1.f, 0.f}, 0.03f, 2);
					Emettre(S, band, "corniche", "pierre");
					Soupe sol;
					Pave(sol, -W * 0.5f - 0.1f, 0.f, -D * 0.5f - 0.1f, W * 0.5f + 0.1f, 0.25f, D * 0.5f + 0.1f);
					Chanfrein(sol, {0.f, 1.f, 0.f}, 0.03f, 1);
					Emettre(S, sol, "soubassement", "pierre");
				}
				// ── LE TOIT ──
				const float32 deb = det ? 0.5f : 0.f; // debord
				if (plat) {
					Soupe s;
					Pave(s, -W * 0.5f - deb * 0.5f, H, -D * 0.5f - deb * 0.5f, W * 0.5f + deb * 0.5f, H + 0.25f,
						 D * 0.5f + deb * 0.5f);
					Emettre(S, s, "toit", "beton");
					if (det) {
						Soupe a;
						const float32 x0 = -W * 0.5f, x1 = W * 0.5f, z0 = -D * 0.5f, z1 = D * 0.5f, y = H + 0.25f;
						Pave(a, x0, y, z1 - 0.2f, x1, y + 0.6f, z1);
						Pave(a, x0, y, z0, x1, y + 0.6f, z0 + 0.2f);
						Pave(a, x0, y, z0 + 0.2f, x0 + 0.2f, y + 0.6f, z1 - 0.2f);
						Pave(a, x1 - 0.2f, y, z0 + 0.2f, x1, y + 0.6f, z1 - 0.2f);
						Chanfrein(a, {0.f, 1.f, 0.f}, 0.02f, 1);
						Emettre(S, a, "acrotere", "platre");
					}
					return;
				}
				// deux pans : versants en pente de 35 degres, faitage le long de x
				const float32 pente = 35.f, a = pente * 0.017453292f;
				const float32 demi = D * 0.5f + deb, larg = demi / cosf(a), ep = 0.12f;
				const float32 hf = D * 0.5f * tanf(a); // le faitage : le plan du versant passe par le haut du mur
				for (int32 c = 0; c < 2; ++c)
					Versant(S, c == 0 ? "versant_avant" : "versant_arriere", c == 0 ? "tuiles_avant" : "tuiles_arriere",
							-W * 0.5f - deb, W * 0.5f + deb, larg, ep, pente, c == 0 ? 1.f : -1.f, H + hf,
							det, 0.28f, false);
				// les pignons triangulaires, sous les versants
				for (int32 c = 0; c < 2; ++c) {
					const float32 x = c == 0 ? -W * 0.5f : W * 0.5f - t;
					Soupe g;
					gDebut = 0;
					const float32 h2 = (D * 0.5f) * tanf(a);
					const uint32 p0 = Sommet(g, x, H, -D * 0.5f), p1 = Sommet(g, x, H, D * 0.5f), p2 = Sommet(g, x, H + h2, 0.f);
					const uint32 q0 = Sommet(g, x + t, H, -D * 0.5f), q1 = Sommet(g, x + t, H, D * 0.5f),
								 q2 = Sommet(g, x + t, H + h2, 0.f);
					const uint32 f0[3] = {p0, p1, p2}, f1[3] = {q0, q2, q1};
					const uint32 f2[4] = {p0, q0, q1, p1}, f3[4] = {p1, q1, q2, p2}, f4[4] = {p2, q2, q0, p0};
					Face(g, f0, 3);
					Face(g, f1, 3);
					Face(g, f2, 4);
					Face(g, f3, 4);
					Face(g, f4, 4);
					Emettre(S, g, c == 0 ? "fronton_gauche" : "fronton_droit", "platre");
				}
				if (det) {
					Soupe f;
					Pave(f, -W * 0.5f - deb, H + hf - 0.05f, -0.1f, W * 0.5f + deb, H + hf + 0.12f, 0.1f);
					Chanfrein(f, {0.f, 1.f, 0.f}, 0.05f, 2);
					Emettre(S, f, "faitage", "tuile");
					Soupe ch;
					Pave(ch, W * 0.25f, H + hf * 0.4f, -D * 0.2f, W * 0.25f + 0.6f, H + hf + 0.9f, -D * 0.2f + 0.6f);
					Chanfrein(ch, {0.f, 1.f, 0.f}, 0.03f, 1);
					Emettre(S, ch, "cheminee", "brique");
				}
			}
		} // namespace

		// =====================================================================
		//  LA TABLE DES FAMILLES : une seule liste, lue par tout le reste
		// =====================================================================
		// ⚠️ UNE SEULE LISTE, ET C'EST LE POINT. Le 21/09 le lexique de
		//    reconnaissance, le formulaire et le repartiteur portaient chacun leur
		//    copie des noms ; le lexique connaissait des familles que le repartiteur
		//    ne savait pas construire. Ici, `kFamilles` est la verite, et
		//    `NkFamilleNom`, `NkFamilleConnue` et `NkFamilleConstruire` la lisent.
		namespace {
			struct Famille {
					const char *nom;
					void (*bati)(Sortie &, const NkFamilleParams &);
					const char *formulaire;
			};
			// ⚠️ `<metres>` ET JAMAIS `<m>` (mesure du 22/09). Avec `largeur <m>`, le 7B
			//    a repondu `largeur 0.08 m hauteur 0.12 m` : l'unite abregee dans le
			//    gabarit invite a la RECOPIER dans la reponse, et le lecteur a rejete
			//    deux parametres inconnus (« m », « 0.12 »). Le mot entier ne se
			//    recopie pas.
			const Famille kFamilles[] = {
					{"porte", &Porte,
					 "famille porte style <simple|chinois> largeur <metres> hauteur <metres> battants <1|2> detail <simple|detaille>"},
					{"portail", &Porte,
					 "famille portail style <simple|chinois> largeur <metres> hauteur <metres> battants <1|2> detail <simple|detaille>"},
					{"table", &Table,
					 "famille table largeur <metres> profondeur <metres> hauteur <metres> pieds <4> detail <simple|detaille>"},
					{"maison", &Maison,
					 "famille maison toit <deux_pans|plat> largeur <metres> profondeur <metres> etages <1|2|3> fenetres <2..6> detail <simple|detaille>"},
			};
			const int32 kNbFamilles = (int32)(sizeof(kFamilles) / sizeof(kFamilles[0]));
			const Famille *Trouver(const char *nom) {
				if (!nom)
					return nullptr;
				for (int32 i = 0; i < kNbFamilles; ++i)
					if (strcmp(kFamilles[i].nom, nom) == 0)
						return &kFamilles[i];
				return nullptr;
			}
		} // namespace

		const char *NkFamilleNom(int32 i) {
			return (i >= 0 && i < kNbFamilles) ? kFamilles[i].nom : nullptr;
		}

		bool NkFamilleConnue(const char *famille) {
			return Trouver(famille) != nullptr;
		}

		bool NkFamilleFormulaire(const char *famille, char *out, uint32 cap) {
			const Famille *f = Trouver(famille);
			if (!f || !out || !cap)
				return false;
			snprintf(out, cap, "%s", f->formulaire);
			return true;
		}

		// ── LES VALEURS AUTORISEES, ET LA CORRECTION PLUTOT QUE LE REJET ───────
		// ⚠️ ON CORRIGE, ON NE REJETTE PAS. Le 21/09, `silhouette vase` (un mot pris
		//    dans la demande) a fait refuser le DOCUMENT ENTIER : l'utilisateur n'a
		//    rien eu. Un mot inconnu dans un champ enumere est une faute du
		//    remplisseur, pas une raison de tout perdre -- on retombe sur le defaut
		//    NOMME du champ, et l'appelant le dit dans le fil.
		namespace {
			struct Enumere {
					const char *famille; ///< nullptr = toutes
					const char *champ;
					const char *valeurs; ///< separees par '|', la PREMIERE est le defaut
			};
			const Enumere kEnumeres[] = {
					{"porte", "style", "simple|chinois"},
					{"portail", "style", "simple|chinois"},
					{"maison", "toit", "deux_pans|plat"},
					{"maison", "style", "deux_pans|plat"},
					{nullptr, "detail", "detaille|simple"},
					{nullptr, "silhouette", "droit|evase|ventru|goulot|coupe|dome|conique|colonne"},
			};
		} // namespace

		bool NkFamilleValeurAutorisee(const char *famille, const char *champ, const char *valeur, char *remplacement,
									  uint32 cap) {
			if (!champ || !valeur)
				return true;
			for (const Enumere &e : kEnumeres) {
				if (strcmp(e.champ, champ) != 0)
					continue;
				if (e.famille && (!famille || strcmp(e.famille, famille) != 0))
					continue;
				const char *c = e.valeurs;
				while (*c) {
					const char *fin2 = strchr(c, '|');
					const usize n = fin2 ? (usize)(fin2 - c) : strlen(c);
					if (strlen(valeur) == n && strncmp(valeur, c, n) == 0)
						return true;
					if (!fin2)
						break;
					c = fin2 + 1;
				}
				if (remplacement && cap) {
					const char *fin2 = strchr(e.valeurs, '|');
					const usize n = fin2 ? (usize)(fin2 - e.valeurs) : strlen(e.valeurs);
					const usize k = n + 1u < cap ? n : cap - 1u;
					memcpy(remplacement, e.valeurs, k);
					remplacement[k] = 0;
				}
				return false;
			}
			return true; // champ libre (un nombre) : rien a controler ici
		}

		int32 NkFamilleConstruire(const NkFamilleParams &p, NkVector<NkFamillePiece> &out, char *pourquoi,
								  uint32 capPourquoi) {
			const Famille *f = Trouver(p.famille);
			if (!f) {
				if (pourquoi) {
					char liste[256];
					liste[0] = 0;
					for (int32 i = 0; i < kNbFamilles; ++i) {
						strncat(liste, kFamilles[i].nom, sizeof(liste) - strlen(liste) - 3u);
						if (i + 1 < kNbFamilles)
							strncat(liste, ", ", sizeof(liste) - strlen(liste) - 1u);
					}
					snprintf(pourquoi, capPourquoi, "famille inconnue « %s » (connues : %s)", p.famille, liste);
				}
				return -1;
			}
			const usize avant = out.Size();
			Sortie S{&out};
			f->bati(S, p);
			if (!S.ok) {
				if (pourquoi)
					snprintf(pourquoi, capPourquoi, "la famille « %s » n'a pas pu construire toutes ses pieces",
							 p.famille);
				while (out.Size() > avant)
					out.PopBack();
				return -1;
			}
			return S.n;
		}

	} // namespace renderer
} // namespace nkentseu
