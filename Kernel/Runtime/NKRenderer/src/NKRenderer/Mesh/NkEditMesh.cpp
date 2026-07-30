// =============================================================================
// NkEditMesh.cpp — NKRenderer — maillage éditable demi-arête (n-gon)
// =============================================================================
#include "NkEditMesh.h"
#include "NKContainers/Associative/NkHashMap.h"

#include <cmath> // cosf / sinf / atan2f — profils d'arc du bevel, rotations du spin

namespace nkentseu {
	namespace renderer {

		// ── Tangente ORTHOGONALE à la normale (anti-NaN) ──────────────────────────
		// ⚠ BUG « carrés blancs » : la tangente était figée à (1,0,0) pour TOUS les
		// sommets. Sur les faces ±X d'un cube (normale (±1,0,0)) la tangente est alors
		// COLINÉAIRE à la normale : le vertex shader PBR fait un Gram-Schmidt
		// T - dot(T,N)*N  == VECTEUR NUL  -> normalize(0) = NaN -> TBN NaN -> N NaN.
		// En mode d'affichage NORMAL (viewMode 2) la couleur vaut N*0.5+0.5, donc NaN,
		// et le NaN se propage dans la chaîne de bloom (down/up) : il ressort en un
		// gros RECTANGLE BLANC aligné écran qui masque l'objet — identique sur TOUS
		// les backends (c'est de l'arithmétique flottante, pas du RHI).
		// On génère donc une tangente réellement perpendiculaire à la normale.
		static NkVec3f NkEmOrthoTangent(const NkVec3f &n) {
			const float32 l = n.Len();
			if (l < 1e-6f)
				return {1.f, 0.f, 0.f}; // normale dégénérée -> tangente arbitraire valide
			const NkVec3f nn = n * (1.f / l);
			// Axe de référence NON colinéaire à nn (seuil large : évite un cross ~nul).
			const NkVec3f ref = (nn.y < 0.9f && nn.y > -0.9f) ? NkVec3f{0.f, 1.f, 0.f} : NkVec3f{1.f, 0.f, 0.f};
			NkVec3f t = ref.Cross(nn);
			const float32 tl = t.Len();
			if (tl < 1e-6f)
				return {1.f, 0.f, 0.f};
			return t * (1.f / tl);
		}

		void NkEditMesh::BuildFromIndexed(const NkVertex3D *v, uint32 vc, const uint32 *idx, uint32 ic, bool quadify) {
			Clear();
			verts.Resize(vc);
			for (uint32 i = 0; i < vc; i++) {
				verts[i].pos = v[i].pos;
				verts[i].normal = v[i].normal;
				verts[i].uv = v[i].uv;
				// Attributs TRANSPORTES tels quels : ils ne servent pas a l'edition
				// topologique, mais sans eux l'aller-retour n'est pas une identite
				// (le repere tangent serait reinvente a la sortie) — cf. struct Vert.
				verts[i].tangent = v[i].tangent;
				verts[i].uv2 = v[i].uv2;
				verts[i].color = v[i].color;
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
				// OMBRAGE : on DEDUIT flat/smooth des normales SOURCE au lieu de retomber sur
				// le defaut FLAT. Sans cela, entrer en mode edition puis en ressortir SANS
				// RIEN MODIFIER applatissait le modele : la sphere lissee revenait facettee,
				// parce que Face::smooth valait 0 pour toutes les faces reconstruites.
				//
				// Critere : une face est FLAT si ses coins portent la MEME normale dans la
				// source — c'est la definition meme du plat (une normale par face, donc des
				// coins dedoubles). Des qu'ils different, la source portait des normales
				// moyennees, donc SMOOTH.
				//
				// Seuil serre a 0.99999 (~0,26 degre) : plus laxiste, une surface lissee tres
				// dense — dont les coins voisins ne different que de quelques dixiemes de
				// degre — passerait pour plate. LIMITE ASSUMEE : au-dela de la densite ou
				// l'ecart tombe sous 0,26 degre la deduction bascule sur FLAT, mais a cette
				// densite plat et lisse sont visuellement indiscernables.
				{
					const NkVec3f &na = v[a].normal, &nb = v[b].normal, &nc = v[c].normal;
					const float32 kFlatDot = 0.99999f;
					fc.smooth =
						(na.Dot(nb) >= kFlatDot && nb.Dot(nc) >= kFlatDot && nc.Dot(na) >= kFlatDot) ? 0 : 1;
				}
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
			// ── NORMALES DE SOMMET : on REND celles de la SOURCE ────────────────
			// RecomputeNormals() (ci-dessus, et de nouveau depuis Quadify) recalcule
			// chaque normale de sommet comme la moyenne, ponderee par l'aire, des faces
			// incidentes. C'est indispensable pour les FACES (Face::normal sert aux
			// operations topologiques) et pour la geometrie CREEE par l'edition — mais
			// c'est FAUX pour une primitive dont les normales sont ANALYTIQUES : sur une
			// sphere UV, la moyenne des facettes n'est pas la normale exacte de la sphere.
			// Mesure : 1040 sommets sur 1089 changeaient au simple fait d'entrer en mode
			// edition et d'en ressortir, d'ou l'impression que le materiau avait change
			// alors que la geometrie n'avait pas bouge d'un micron.
			// Les operations d'edition rappellent RecomputeNormals() apres coup : la
			// geometrie nouvelle obtient bien des normales recalculees ; seule l'ENTREE
			// en edition reste neutre.
			for (uint32 i = 0; i < vc && i < (uint32)verts.Size(); i++)
				verts[i].normal = v[i].normal;
			// Liste d'aretes de premier plan, construite DES l'entree : l'editeur peut
			// ainsi compter/afficher les aretes sans dependre du premier AddWireEdge
			// (qui, lui, garde un rebuild paresseux en filet). Une construction partant
			// de zero n'a par definition aucune arete filaire a preserver.
			RebuildEdges();
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
				// f1 survit et absorbe f2 : l'ombrage doit suivre. Les deux triangles d'un
				// meme quad source portent normalement le meme reglage, mais on prend le OU
				// pour ne jamais perdre un lissage lors de la fusion.
				faces[f1].smooth = (uint8)(faces[f1].smooth | faces[f2].smooth);
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

		// Grille de hachage spatiale : positions quantifiées au pas `eps` puis hachées. Les
		// sommets STRICTEMENT identiques (cas des primitives, qui réutilisent les mêmes
		// coordonnées pour chaque face) tombent forcément dans la même cellule. O(n) : aucune
		// comparaison par paires.
		void NkEditMesh::BuildVertexMerge(NkVector<uint32> &canon, float32 eps) const {
			const uint32 n = (uint32)verts.Size();
			canon.Resize(n);
			if (eps <= 0.f)
				eps = 1e-4f;
			const float32 inv = 1.f / eps;
			NkHashMap<uint64, uint32> cell;
			cell.Reserve(n);
			for (uint32 i = 0; i < n; ++i) {
				const NkVec3f p = verts[i].pos;
				// Arrondi (et non plancher) : deux coordonnées EXACTEMENT égales donnent la
				// même clé quel que soit leur signe.
				const int64 qx = (int64)(p.x * inv + (p.x >= 0.f ? 0.5f : -0.5f));
				const int64 qy = (int64)(p.y * inv + (p.y >= 0.f ? 0.5f : -0.5f));
				const int64 qz = (int64)(p.z * inv + (p.z >= 0.f ? 0.5f : -0.5f));
				const uint64 key = ((uint64)(qx & 0x1FFFFF)) | (((uint64)(qy & 0x1FFFFF)) << 21) |
								   (((uint64)(qz & 0x1FFFFF)) << 42);
				uint32 *found = cell.Find(key);
				if (found)
					canon[i] = *found; // rattaché au représentant du groupe
				else {
					canon[i] = i;
					cell.InsertOrAssign(key, i);
				}
			}
		}

		void NkEditMesh::PropagateSelectionToCoincident() {
			NkVector<uint32> canon;
			BuildVertexMerge(canon);
			const uint32 n = (uint32)verts.Size();
			NkVector<uint8> repSel;
			repSel.Resize(n);
			for (uint32 i = 0; i < n; ++i)
				repSel[i] = 0;
			for (uint32 i = 0; i < n; ++i)
				if (verts[i].sel)
					repSel[canon[i]] = 1;
			for (uint32 i = 0; i < n; ++i)
				verts[i].sel = repSel[canon[i]];
		}

		void NkEditMesh::LinkTwins() {
			// ⚠ Les jumeaux sont appariés sur l'IDENTITÉ TOPOLOGIQUE (position soudée), PAS
			// sur les indices bruts : sinon, avec des sommets dupliqués par face (primitives),
			// aucune demi-arête ne trouve son opposée dans la face voisine et le maillage
			// reste une collection de faces isolées (loop cut qui ne boucle pas, cage qui
			// compte les arêtes en double). Les attributs par coin ne sont pas touchés
			// -> rendu strictement inchangé.
			NkVector<uint32> canon;
			BuildVertexMerge(canon);
			const uint32 nv = (uint32)canon.Size();
			auto C = [&](uint32 v) -> uint64 { return (uint64)((v < nv) ? canon[v] : v); };
			for (uint32 h = 0; h < (uint32)hedges.Size(); ++h)
				hedges[h].twin = NK_EM_INVALID;
			NkHashMap<uint64, NkEmId> map;
			map.Reserve((uint32)hedges.Size());
			for (uint32 h = 0; h < (uint32)hedges.Size(); ++h) {
				if (!hedges[h].alive || hedges[h].next == NK_EM_INVALID)
					continue;
				const uint64 o = C(hedges[h].origin);
				const uint64 d = C(hedges[hedges[h].next].origin);
				if (o == d)
					continue; // arête dégénérée
				const uint64 opp = (d << 32) | o; // demi-arête opposée (d->o)
				NkEmId *found = map.Find(opp);
				if (found && hedges[*found].twin == NK_EM_INVALID) {
					hedges[h].twin = *found;
					hedges[*found].twin = h;
				} else if (!found) {
					map.InsertOrAssign((o << 32) | d, h);
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

		// CONVENTION DE WINDING — le moteur rend en FRONT = HORAIRE (cf. primitives
		// NkMeshSystem : le cube déclare n[4]={0,1,0} pour la face du dessus dont la
		// boucle {3,7,6,2} donne (p1-p0)x(p2-p0) = -Y). Le produit vectoriel « CCW »
		// standard sort donc des normales INVERSÉES : on prend l'opposé (p2-p0)x(p1-p0).
		// Sans ça : éclairage retourné sur le maillage édité, extrusions vers l'INTÉRIEUR
		// et orientation « Normal » du gizmo à l'envers.
		static inline NkVec3f NkEmFaceCross(const NkVec3f &p0, const NkVec3f &p1, const NkVec3f &p2) {
			return (p2 - p0).Cross(p1 - p0);
		}

		void NkEditMesh::RecomputeNormals() {
			const uint32 nv = (uint32)verts.Size();
			const uint32 nf = (uint32)faces.Size();
			// 1) Normale de chaque face (vecteur NON normalisé = pondération par l'aire).
			NkVector<NkVec3f> fn;
			fn.Resize(nf);
			bool anySmooth = false;
			NkVector<NkEmId> loop;
			for (uint32 f = 0; f < nf; ++f) {
				fn[f] = {0.f, 0.f, 0.f};
				if (!faces[f].alive)
					continue;
				loop.Clear();
				GetFaceVerts(f, loop);
				if (loop.Size() < 3)
					continue;
				const NkVec3f p0 = verts[loop[0]].pos, p1 = verts[loop[1]].pos, p2 = verts[loop[2]].pos;
				NkVec3f n = NkEmFaceCross(p0, p1, p2);
				const float32 l = n.Len();
				faces[f].normal = (l > 1e-8f) ? n * (1.f / l) : NkVec3f{0.f, 1.f, 0.f};
				fn[f] = n;
				if (faces[f].smooth)
					anySmooth = true;
			}
			// 2) Accumulation FLAT (par index de sommet) — chemin historique, aucun coût
			//    supplémentaire tant qu'aucune face n'est lissée.
			NkVector<NkVec3f> flatAcc;
			flatAcc.Resize(nv);
			for (uint32 i = 0; i < nv; ++i)
				flatAcc[i] = {0.f, 0.f, 0.f};
			// 3) Accumulation SMOOTH (par sommet SOUDÉ) : seules les faces smooth y
			//    contribuent, et toutes les copies coïncidentes partagent le résultat.
			NkVector<uint32> canon;
			NkVector<NkVec3f> smoothAcc;
			NkVector<uint8> vertSmooth; // 1 = le sommet appartient à >=1 face lissée
			if (anySmooth) {
				BuildVertexMerge(canon);
				smoothAcc.Resize(nv);
				vertSmooth.Resize(nv);
				for (uint32 i = 0; i < nv; ++i) {
					smoothAcc[i] = {0.f, 0.f, 0.f};
					vertSmooth[i] = 0;
				}
			}
			auto C = [&](uint32 v) { return (anySmooth && v < (uint32)canon.Size()) ? canon[v] : v; };
			for (uint32 f = 0; f < nf; ++f) {
				if (!faces[f].alive)
					continue;
				loop.Clear();
				GetFaceVerts(f, loop);
				if (loop.Size() < 3)
					continue;
				const bool sm = anySmooth && faces[f].smooth != 0;
				for (uint32 k = 0; k < (uint32)loop.Size(); ++k) {
					const uint32 v = loop[k];
					if (v >= nv)
						continue;
					if (sm) {
						const uint32 c = C(v);
						if (c < nv)
							smoothAcc[c] = smoothAcc[c] + fn[f];
						vertSmooth[v] = 1;
					} else
						flatAcc[v] = flatAcc[v] + fn[f];
				}
			}
			// 4) Normale finale du sommet. Un sommet touché À LA FOIS par des faces flat et
			//    des faces smooth (possible seulement si l'index est PARTAGÉ entre faces —
			//    les primitives, elles, dupliquent les coins) mélange les deux contributions :
			//    limite assumée de la structure, qui porte UNE normale par SOMMET et non par
			//    COIN (comme Blender le ferait avec des « loops »).
			for (uint32 i = 0; i < nv; ++i) {
				NkVec3f n = flatAcc[i];
				if (anySmooth && vertSmooth[i]) {
					const uint32 c = C(i);
					if (c < nv)
						n = n + smoothAcc[c];
				}
				const float32 l = n.Len();
				verts[i].normal = (l > 1e-8f) ? n * (1.f / l) : NkVec3f{0.f, 1.f, 0.f};
			}
		}

		bool NkEditMesh::SetShadeSmooth(bool smooth, bool selectedOnly) {
			const uint8 want = smooth ? (uint8)1 : (uint8)0;
			// Y a-t-il au moins une face sélectionnée ? Sinon on traite TOUT le maillage
			// (équivalent du « Shade Smooth » appliqué à l'objet entier).
			bool anySel = false;
			if (selectedOnly)
				for (uint32 f = 0; f < (uint32)faces.Size() && !anySel; ++f)
					if (faces[f].alive && FaceIsSelected(f))
						anySel = true;
			bool changed = false;
			for (uint32 f = 0; f < (uint32)faces.Size(); ++f) {
				if (!faces[f].alive)
					continue;
				if (anySel && !FaceIsSelected(f))
					continue;
				if (faces[f].smooth != want) {
					faces[f].smooth = want;
					changed = true;
				}
			}
			if (changed)
				RecomputeNormals();
			return changed;
		}

		bool NkEditMesh::AnyFaceSmooth() const {
			for (uint32 f = 0; f < (uint32)faces.Size(); ++f)
				if (faces[f].alive && faces[f].smooth)
					return true;
			return false;
		}

		bool NkEditMesh::AllFacesSmooth() const {
			bool any = false;
			for (uint32 f = 0; f < (uint32)faces.Size(); ++f) {
				if (!faces[f].alive)
					continue;
				any = true;
				if (!faces[f].smooth)
					return false;
			}
			return any;
		}

		// Demi-arête vivante correspondant à l'arête (a,b), comparée sur l'IDENTITÉ
		// TOPOLOGIQUE (sommets soudés) : deux faces voisines n'emploient pas les mêmes
		// indices pour l'arête qu'elles partagent.
		static NkEmId NkEmFindHedge(const NkEditMesh &m, const NkVector<uint32> &canon, uint32 a, uint32 b) {
			const uint32 n = (uint32)canon.Size();
			auto C = [&](uint32 v) { return (v < n) ? canon[v] : v; };
			const uint32 ca = C(a), cb = C(b);
			for (uint32 h = 0; h < (uint32)m.hedges.Size(); ++h) {
				if (!m.hedges[h].alive || m.hedges[h].next == NK_EM_INVALID)
					continue;
				const uint32 o = C(m.hedges[h].origin), d = C(m.hedges[m.hedges[h].next].origin);
				if ((o == ca && d == cb) || (o == cb && d == ca))
					return (NkEmId)h;
			}
			return NK_EM_INVALID;
		}

		// -- ADJACENCE TOPOLOGIQUE POUR LES BOUCLES ---------------------------------
		// Valence (nb d'ARETES uniques incidentes) et « sur un bord » de chaque sommet
		// CANONIQUE (soude). Ces deux informations sont ce qui distingue, facon Blender,
		// un coin de cube (ferme, valence 3) d'un bord de grille (ouvert, valence 3 lui
		// aussi) : sans elles, la boucle derive sur l'un ou deborde sur l'autre.
		struct NkEmVertAdj {
				NkVector<uint16> valence;   // nb d'aretes uniques au sommet canonique
				NkVector<uint8> onBoundary; // 1 = au moins une arete incidente sans jumeau
		};

		static void NkEmBuildVertAdj(const NkEditMesh &m, const NkVector<uint32> &canon, NkEmVertAdj &out) {
			const uint32 nv = (uint32)m.verts.Size();
			const uint32 nc = (uint32)canon.Size();
			auto C = [&](uint32 v) { return (v < nc) ? canon[v] : v; };
			out.valence.Resize(nv);
			out.onBoundary.Resize(nv);
			for (uint32 i = 0; i < nv; ++i) {
				out.valence[i] = 0;
				out.onBoundary[i] = 0;
			}
			NkHashMap<uint64, uint8> seen;
			seen.Reserve((uint32)m.hedges.Size());
			for (uint32 h = 0; h < (uint32)m.hedges.Size(); ++h) {
				if (!m.hedges[h].alive || m.hedges[h].next == NK_EM_INVALID)
					continue;
				const uint32 o = C(m.hedges[h].origin), d = C(m.hedges[m.hedges[h].next].origin);
				if (o == d || o >= nv || d >= nv)
					continue;
				if (m.hedges[h].twin == NK_EM_INVALID) { // arete de BORD (maillage ouvert)
					out.onBoundary[o] = 1;
					out.onBoundary[d] = 1;
				}
				const uint32 lo = o < d ? o : d, hi = o < d ? d : o;
				const uint64 key = ((uint64)lo << 32) | hi;
				if (seen.Find(key))
					continue; // arete deja comptee (l'autre demi-arete)
				seen.InsertOrAssign(key, (uint8)1);
				out.valence[o] = (uint16)(out.valence[o] + 1);
				out.valence[d] = (uint16)(out.valence[d] + 1);
			}
		}

		// -- EDGE LOOP (Alt+clic) : REGLES DE BLENDER --------------------------------
		// La boucle avance d'arete en arete ; a chaque sommet traverse, la regle depend de
		// sa VALENCE (c'est exactement ce que fait le « loop walker » de Blender) :
		//
		//  - valence 4, sommet INTERIEUR (grille de quads reguliere) -> on CONTINUE TOUT
		//    DROIT : l'arete opposee a celle d'ou l'on vient, via next(twin(next(h))). La
		//    boucle file donc tout droit jusqu'au bord du maillage.
		//
		//  - valence 3, sommet INTERIEUR (coin ferme : TOUS les coins d'un cube brut) :
		//    « tout droit » n'existe pas. Blender ne derive PAS au hasard, la boucle suit
		//    le BORD DE LA FACE courante, c.-a-d. next(h). Sur un cube elle referme donc
		//    l'ANNEAU DE 4 ARETES qui fait le tour (le contour de la face cliquee) - au
		//    lieu des 7 aretes que donnait next(twin(next(h))) : cette regle-la tournait
		//    d'une face a chaque coin, puis repartait dans l'autre sens au 2e passage, en
		//    cumulant DEUX anneaux distincts moins l'arete de depart (4 + 4 - 1 = 7).
		//
		//  - sommet de BORD (une arete incidente sans jumeau : bord d'une grille ouverte),
		//    POLE (valence != 3 et != 4), ou face non-quad -> la boucle S'ARRETE.
		//    ATTENTION : c'est ici que la distinction bord/interieur est indispensable, un
		//    sommet du bord d'une grille est AUSSI de valence 3 mais ne doit surtout pas
		//    partir le long du bord - Blender s'y arrete.
		void NkEditMesh::GetEdgeLoop(uint32 a, uint32 b, NkVector<uint32> &outPairs) const {
			outPairs.Clear();
			NkVector<uint32> canon;
			BuildVertexMerge(canon);
			const NkEmId h0 = NkEmFindHedge(*this, canon, a, b);
			if (h0 == NK_EM_INVALID)
				return;
			const uint32 nc = (uint32)canon.Size();
			auto C = [&](uint32 v) { return (v < nc) ? canon[v] : v; };
			NkEmVertAdj adj;
			NkEmBuildVertAdj(*this, canon, adj);
			NkHashMap<uint64, uint8> seen;
			auto emit = [&](NkEmId h) -> bool { // false si l'arete etait DEJA dans la boucle
				const uint32 o = hedges[h].origin, d = hedges[hedges[h].next].origin;
				const uint32 co = C(o), cd = C(d);
				const uint32 lo = co < cd ? co : cd, hi = co < cd ? cd : co;
				const uint64 key = ((uint64)lo << 32) | hi;
				if (seen.Find(key))
					return false;
				seen.InsertOrAssign(key, (uint8)1);
				outPairs.PushBack(o);
				outPairs.PushBack(d);
				return true;
			};
			// Avance d'un cran : h va (u -> v) ; renvoie la demi-arete sortante de v qui
			// prolonge la boucle selon les regles ci-dessus. NK_EM_INVALID = fin de boucle.
			auto step = [&](NkEmId h) -> NkEmId {
				if (h == NK_EM_INVALID || FaceSize(hedges[h].face) != 4)
					return NK_EM_INVALID; // on ne progresse qu'a travers des QUADS
				const NkEmId hn = hedges[h].next; // v -> w, dans la MEME face
				if (hn == NK_EM_INVALID)
					return NK_EM_INVALID;
				const uint32 v = C(hedges[hn].origin); // sommet traverse
				if (v >= (uint32)adj.valence.Size())
					return NK_EM_INVALID;
				if (adj.onBoundary[v])
					return NK_EM_INVALID; // bord du maillage -> Blender s'arrete
				const uint16 val = adj.valence[v];
				if (val == 4) {
					const NkEmId tw = hedges[hn].twin; // w -> v, face voisine
					if (tw == NK_EM_INVALID)
						return NK_EM_INVALID;
					return hedges[tw].next; // v -> x : l'arete OPPOSEE a celle d'ou l'on vient
				}
				if (val == 3)
					return hn; // coin ferme : on suit le contour de la face (anneau du cube)
				return NK_EM_INVALID; // pole -> arret
			};
			emit(h0);
			// Sens AVANT. Si l'on retombe sur une arete deja emise, la boucle est FERMEE :
			// inutile (et nuisible) d'explorer le sens arriere - c'est exactement ce qui
			// faisait cumuler deux anneaux sur un cube.
			bool closed = false;
			{
				NkEmId h = h0;
				uint32 guard = 0;
				while (h != NK_EM_INVALID && ++guard < 100000u) {
					h = step(h);
					if (h == NK_EM_INVALID)
						break; // bord / pole atteint : boucle OUVERTE
					if (!emit(h)) {
						closed = true; // on a reboucle
						break;
					}
				}
			}
			if (closed)
				return;
			// Sens ARRIERE (boucle ouverte : grille, bord de maillage) - on repart du jumeau,
			// qui pointe dans l'autre sens.
			NkEmId h = hedges[h0].twin;
			uint32 guard = 0;
			while (h != NK_EM_INVALID && ++guard < 100000u) {
				h = step(h);
				if (h == NK_EM_INVALID || !emit(h))
					break;
			}
		}

		// -- FACE LOOP / EDGE RING (Alt+clic en mode FACE) ---------------------------
		// Anneau des faces traversees par l'arete (a,b) : de quad en quad par l'arete
		// OPPOSEE (meme parcours que le loop cut). Sur un cube brut cela donne bien les 4
		// faces qui font le tour. Si l'anneau bute sur un BORD, on repart dans l'AUTRE
		// sens depuis l'arete de depart, pour ne pas rendre une demi-boucle.
		void NkEditMesh::GetFaceLoop(uint32 a, uint32 b, NkVector<NkEmId> &outFaces) const {
			outFaces.Clear();
			NkVector<uint32> canon;
			BuildVertexMerge(canon);
			const NkEmId hStart = NkEmFindHedge(*this, canon, a, b);
			if (hStart == NK_EM_INVALID)
				return;
			NkHashMap<uint64, uint8> seenF;
			// Parcourt l'anneau depuis h ; renvoie true si l'anneau s'est REFERME sur h0.
			auto walk = [&](NkEmId h, NkEmId h0) -> bool {
				uint32 guard = 0;
				while (h != NK_EM_INVALID && ++guard < 100000u) {
					const NkEmId f = hedges[h].face;
					if (f == NK_EM_INVALID || f >= (NkEmId)faces.Size() || !faces[f].alive)
						return false;
					if (!seenF.Find((uint64)f)) {
						seenF.InsertOrAssign((uint64)f, (uint8)1);
						outFaces.PushBack(f);
					}
					if (FaceSize(f) != 4)
						return false; // l'anneau ne traverse que des quads
					const NkEmId hn = hedges[h].next;
					if (hn == NK_EM_INVALID)
						return false;
					const NkEmId hOpp = hedges[hn].next; // arete opposee du quad
					if (hOpp == NK_EM_INVALID)
						return false;
					const NkEmId tw = hedges[hOpp].twin;
					if (tw == NK_EM_INVALID)
						return false; // bord -> anneau ouvert de ce cote
					if (tw == h0)
						return true; // anneau referme
					h = tw;
				}
				return false;
			};
			if (walk(hStart, hStart))
				return; // anneau complet
			const NkEmId back = hedges[hStart].twin;
			if (back != NK_EM_INVALID)
				walk(back, back);
		}

		bool NkEditMesh::FaceIsSelected(NkEmId f) const {
			if (f >= (NkEmId)faces.Size() || !faces[f].alive)
				return false;
			const NkEmId start = faces[f].hedge;
			if (start == NK_EM_INVALID)
				return false;
			NkEmId h = start;
			uint32 guard = 0, n = 0;
			do {
				const uint32 o = hedges[h].origin;
				if (o >= (uint32)verts.Size() || !verts[o].sel)
					return false;
				++n;
				h = hedges[h].next;
				if (++guard > 100000u)
					break;
			} while (h != start && h != NK_EM_INVALID);
			return n >= 3;
		}

		uint32 NkEditMesh::EdgeFaces(uint32 a, uint32 b, NkEmId &f0, NkEmId &f1) const {
			f0 = NK_EM_INVALID;
			f1 = NK_EM_INVALID;
			uint32 n = 0;
			for (uint32 h = 0; h < (uint32)hedges.Size(); ++h) {
				if (!hedges[h].alive || hedges[h].next == NK_EM_INVALID)
					continue;
				const uint32 o = hedges[h].origin, d = hedges[hedges[h].next].origin;
				if (!((o == a && d == b) || (o == b && d == a)))
					continue;
				const NkEmId f = hedges[h].face;
				if (f == NK_EM_INVALID || f >= (NkEmId)faces.Size() || !faces[f].alive)
					continue;
				if (n == 0) {
					f0 = f;
					n = 1;
				} else if (f != f0) {
					f1 = f;
					return 2;
				}
			}
			return n;
		}

		void NkEditMesh::GetUniqueEdges(NkVector<uint32> &outPairs) const {
			outPairs.Clear();
			// Aretes FILAIRES d'abord : elles n'ont AUCUNE demi-arete, donc la boucle
			// ci-dessous ne peut pas les trouver. Sans ce premier passage, un segment
			// cree avec F serait invisible en fil de fer — il existerait dans la
			// structure sans jamais etre dessine.
			for (uint32 e = 0; e < (uint32)edges.Size(); ++e) {
				if (!edges[e].alive || edges[e].faceCount != 0)
					continue;
				outPairs.PushBack(edges[e].v0);
				outPairs.PushBack(edges[e].v1);
			}
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
				// RESTITUES depuis le sommet, jamais reinventes : ecrire ici une
				// tangente en dur changeait le repere tangent de tout le maillage a
				// chaque aller-retour en edition (cf. struct Vert).
				nv.tangent = verts[i].tangent;
				nv.uv = verts[i].uv;
				nv.uv2 = verts[i].uv2;
				nv.color = verts[i].color;
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
			// ORTHOGONALISATION de la tangente, sans la REMPLACER.
			// Historique : cette boucle ecrasait la tangente par NkEmOrthoTangent(normal),
			// ce qui annulait la tangente d'origine transportee depuis la source — un
			// aller-retour en edition changeait donc le repere tangent de tout le maillage.
			// (Correctif initial du bug « carres blancs » : une tangente COLINEAIRE a la
			// normale donnait normalize(0) = NaN. Le probleme etait la colinearite, pas la
			// tangente elle-meme.)
			// On conserve donc la DIRECTION fournie et on retire seulement sa composante
			// le long de la normale (Gram-Schmidt) ; on ne fabrique une tangente de toutes
			// pieces que si le residu est degenere — c'est-a-dire exactement le cas qui
			// produisait le NaN.
			for (uint32 i = 0; i < (uint32)outV.Size(); ++i) {
				const NkVec3f n = outV[i].normal;
				NkVec3f t = outV[i].tangent;
				t = t - n * n.Dot(t);
				const float32 l2 = t.Dot(t);
				outV[i].tangent = (l2 > 1e-12f) ? t * (1.f / sqrtf(l2)) : NkEmOrthoTangent(n);
			}
		}

		void NkEditMesh::TriangulateShaded(NkVector<NkVertex3D> &outV, NkVector<uint32> &outIdx,
										   NkVector<NkEmId> &outTriFace) const {
			outV.Clear();
			outIdx.Clear();
			outTriFace.Clear();
			const uint32 nv = (uint32)verts.Size();
			outV.Resize(nv);
			for (uint32 i = 0; i < nv; ++i) {
				NkVertex3D nvx{};
				nvx.pos = verts[i].pos;
				nvx.normal = verts[i].normal;
				nvx.tangent = verts[i].tangent;   // restitue, cf. Triangulate
				nvx.uv = verts[i].uv;
				nvx.uv2 = verts[i].uv2;
				nvx.color = verts[i].color;
				outV[i] = nvx;
			}
			// claimed[v] = 1 -> le slot 1:1 du sommet v porte DÉJÀ la normale d'une face FLAT :
			// tout autre coin (flat d'une autre face, ou smooth) doit être DÉDOUBLÉ.
			NkVector<uint8> claimed;
			claimed.Resize(nv);
			for (uint32 i = 0; i < nv; ++i)
				claimed[i] = 0;
			NkVector<uint32> remap; // indice de sortie du coin, indexé par sommet d'origine
			remap.Resize(nv);
			NkVector<NkEmId> loop;
			for (uint32 f = 0; f < (uint32)faces.Size(); ++f) {
				if (!faces[f].alive)
					continue;
				loop.Clear();
				GetFaceVerts(f, loop);
				const uint32 fn = (uint32)loop.Size();
				if (fn < 3)
					continue;
				const bool flat = (faces[f].smooth == 0);
				for (uint32 k = 0; k < fn; ++k) {
					const uint32 v = loop[k];
					if (v >= nv)
						continue;
					uint32 o = v;
					if (flat) {
						if (!claimed[v]) {
							claimed[v] = 1;
							outV[v].normal = faces[f].normal;
						} else {
							NkVertex3D cp = outV[v];
							cp.pos = verts[v].pos;
							cp.uv = verts[v].uv;
							cp.normal = faces[f].normal;
							o = (uint32)outV.Size();
							outV.PushBack(cp);
						}
					} else if (claimed[v]) { // sommet confisqué par une face flat -> copie lisse
						NkVertex3D cp = outV[v];
						cp.pos = verts[v].pos;
						cp.uv = verts[v].uv;
						cp.normal = verts[v].normal;
						o = (uint32)outV.Size();
						outV.PushBack(cp);
					}
					remap[v] = o; // un sommet n'apparaît qu'UNE fois dans la boucle d'une face
				}
				for (uint32 i = 1; i + 1 < fn; ++i) { // éventail
					outIdx.PushBack(remap[loop[0]]);
					outIdx.PushBack(remap[loop[i]]);
					outIdx.PushBack(remap[loop[i + 1]]);
					outTriFace.PushBack((NkEmId)f);
				}
			}
			// ORTHOGONALISATION de la tangente, sans la REMPLACER.
			// Historique : cette boucle ecrasait la tangente par NkEmOrthoTangent(normal),
			// ce qui annulait la tangente d'origine transportee depuis la source — un
			// aller-retour en edition changeait donc le repere tangent de tout le maillage.
			// (Correctif initial du bug « carres blancs » : une tangente COLINEAIRE a la
			// normale donnait normalize(0) = NaN. Le probleme etait la colinearite, pas la
			// tangente elle-meme.)
			// On conserve donc la DIRECTION fournie et on retire seulement sa composante
			// le long de la normale (Gram-Schmidt) ; on ne fabrique une tangente de toutes
			// pieces que si le residu est degenere — c'est-a-dire exactement le cas qui
			// produisait le NaN.
			for (uint32 i = 0; i < (uint32)outV.Size(); ++i) {
				const NkVec3f n = outV[i].normal;
				NkVec3f t = outV[i].tangent;
				t = t - n * n.Dot(t);
				const float32 l2 = t.Dot(t);
				outV[i].tangent = (l2 > 1e-12f) ? t * (1.f / sqrtf(l2)) : NkEmOrthoTangent(n);
			}
		}

		void NkEditMesh::ToPolygons(NkVector<NkVertex3D> &ov, NkVector<uint32> &ofaceStart,
									NkVector<uint32> &ofaceVerts) const {
			ov.Resize((uint32)verts.Size());
			for (uint32 i = 0; i < (uint32)verts.Size(); ++i) {
				NkVertex3D nv{};
				nv.pos = verts[i].pos;
				nv.normal = verts[i].normal;
				// Tangente STOCKEE si elle est utilisable, sinon repli construit a
				// partir de la normale (sommets NES d'une operation d'edition, qui
				// n'en portent pas encore).
				nv.tangent = (verts[i].tangent.Dot(verts[i].tangent) > 1e-12f)
								 ? verts[i].tangent
								 : NkEmOrthoTangent(verts[i].normal);
				nv.uv = verts[i].uv;
				nv.uv2 = verts[i].uv2;
				nv.color = verts[i].color;
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
				// >= 2 : les « faces » à 2 sommets sont des ARÊTES FIL (extrusion de sommet,
				// façon Blender). Elles ne produisent pas de surface (Triangulate les ignore)
				// mais doivent survivre à l'aller-retour polygones (sinon elles disparaissent
				// dès la commande d'édition suivante).
				if (loop.Size() < 2)
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
				// Attributs TRANSPORTES tels quels : ils ne servent pas a l'edition
				// topologique, mais sans eux l'aller-retour n'est pas une identite
				// (le repere tangent serait reinvente a la sortie) — cf. struct Vert.
				verts[i].tangent = v[i].tangent;
				verts[i].uv2 = v[i].uv2;
				verts[i].color = v[i].color;
				verts[i].hedge = NK_EM_INVALID;
				verts[i].sel = 0;
			}
			for (uint32 f = 0; f < faceCount; ++f) {
				const uint32 s = faceStart[f], e = faceStart[f + 1], n = e - s;
				if (n < 2)
					continue; // n == 2 => ARÊTE FIL (cf. ToPolygons)
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

		// EXTRUDE FACES : duplique les faces sélectionnées (cap), crée des quads latéraux sur
		// les arêtes de BORD, décale le cap le long de la normale. p.individual = chaque face
		// le long de SA normale (caps séparés). Préserve les n-gons.
		// ⚠ COMPORTEMENT BLENDER (défaut p.offset == 0) : le cap naît EXACTEMENT sur la face
		// d'origine et la SÉLECTION passe dessus. Rien ne bouge : l'utilisateur déplace/
		// tourne/redimensionne ensuite lui-même (gizmo, axe normal ou contrainte X/Y/Z).
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
				// Les arêtes FIL (2 sommets) ne sont pas des faces extrudables.
				const bool sel = (e - s >= 3) && PolyFaceSelected(fv, s, e);
				faceSel[f] = sel ? 1 : 0;
				if (sel) {
					selCount++;
					avgN = avgN + NkEmFaceCross(pv[fv[s]].pos, pv[fv[s + 1]].pos, pv[fv[s + 2]].pos);
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

			// ── NORMALES PAR SOMMET, restreintes aux faces SELECTIONNEES ────────
			// Necessaires a AlongNormals. Restreintes a la selection : inclure les
			// faces voisines NON extrudees ferait pencher la direction vers
			// l'exterieur de la region et tordrait le bord.
			// Accumulees sur l'identite SOUDEE puis redistribuees : sans cela, un coin
			// duplique par face partirait dans plusieurs directions et le maillage se
			// dechirerait le long des coutures.
			NkVector<NkVec3f> vertN;
			if (p.direction == NkExtrudeParams::AlongNormals) {
				NkVector<uint32> canon;
				BuildVertexMerge(canon);
				auto cn = [&](uint32 v) { return (v < (uint32)canon.Size()) ? canon[v] : v; };
				NkVector<NkVec3f> acc;
				acc.Resize((uint32)pv.Size());
				for (uint32 i = 0; i < (uint32)acc.Size(); i++)
					acc[i] = {0.f, 0.f, 0.f};
				for (uint32 f = 0; f < fc; f++) {
					if (!faceSel[f])
						continue;
					const uint32 s = fs[f];
					// Normale NON normalisee = ponderation par l'aire : une grande face
					// doit peser plus qu'un triangle degenere.
					const NkVec3f fn = NkEmFaceCross(pv[fv[s]].pos, pv[fv[s + 1]].pos, pv[fv[s + 2]].pos);
					for (uint32 k = fs[f]; k < fs[f + 1]; k++) {
						const uint32 r = cn(fv[k]);
						if (r < (uint32)acc.Size())
							acc[r] = acc[r] + fn;
					}
				}
				vertN.Resize((uint32)pv.Size());
				for (uint32 i = 0; i < (uint32)pv.Size(); i++) {
					const uint32 r = cn(i);
					NkVec3f n = (r < (uint32)acc.Size()) ? acc[r] : NkVec3f{0.f, 0.f, 0.f};
					const float32 l = n.Len();
					// Repli sur la normale de region si l'accumulation s'annule (faces
					// opposees de part et d'autre du sommet) : mieux vaut la direction
					// commune qu'un deplacement nul silencieux.
					vertN[i] = (l > 1e-6f) ? n * (1.f / l) : avgN;
				}
			}

			// Deplacement d'UN sommet duplique, selon la variante demandee.
			auto extrudePos = [&](const NkVec3f &base, uint32 srcIdx) -> NkVec3f {
				if (p.direction == NkExtrudeParams::ToCursor) {
					// Chacun rejoint le point cible : les sommets convergent, la region
					// se ferme en pointe. offset sert de FRACTION du chemin (1 = au point).
					const NkVec3f d = p.target - base;
					const float32 t = (p.offset > 0.f) ? p.offset : 1.f;
					return base + d * (t > 1.f ? 1.f : t);
				}
				if (p.direction == NkExtrudeParams::AlongNormals && srcIdx < (uint32)vertN.Size())
					return base + vertN[srcIdx] * off;
				return base + avgN * off;
			};

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
					NkVec3f fn = NkEmFaceCross(pv[fv[s]].pos, pv[fv[s + 1]].pos, pv[fv[s + 2]].pos);
					{
						float32 l = fn.Len();
						fn = (l > 1e-6f) ? fn * (1.f / l) : NkVec3f{0.f, 1.f, 0.f};
					}
					NkVector<uint32> dup;
					dup.Resize(n);
					for (uint32 k = 0; k < n; k++) {
						uint32 vi = fv[s + k];
						NkVertex3D nv = pv[vi];
						// ToCursor primes sur la normale de face : la cible est absolue.
						nv.pos = (p.direction == NkExtrudeParams::ToCursor) ? extrudePos(nv.pos, vi)
																						   : nv.pos + fn * off;
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
						nv.pos = extrudePos(nv.pos, vi); // Region / AlongNormals / ToCursor
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

		// EXTRUDE SOMMETS (mode VERTEX) : chaque sommet sélectionné est DUPLIQUÉ et relié à
		// son original par une ARÊTE FIL (« face » à 2 sommets : pas de surface, mais une
		// vraie arête de la topologie, affichée dans la cage et éditable). La sélection passe
		// sur les NOUVEAUX sommets — à offset 0 ils sont confondus avec les originaux, comme
		// dans Blender : c'est l'utilisateur qui les déplace ensuite.
		bool NkEditMesh::ExtrudeSelectedVertices(const NkExtrudeParams &p) {
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			ToPolygons(pv, fs, fv);
			const uint32 baseVerts = (uint32)pv.Size();
			NkVector<uint32> src; // sommets sélectionnés
			for (uint32 i = 0; i < (uint32)verts.Size() && i < baseVerts; i++)
				if (verts[i].sel)
					src.PushBack(i);
			if (src.Empty())
				return false;
			const float32 off = (p.offset > 0.f) ? p.offset : 0.f;
			NkVector<uint8> vsel;
			vsel.Resize(baseVerts);
			for (uint32 i = 0; i < baseVerts; i++)
				vsel[i] = 0;
			for (uint32 k = 0; k < (uint32)src.Size(); k++) {
				const uint32 a = src[k];
				NkVertex3D nv = pv[a];
				nv.pos = nv.pos + verts[a].normal * off;
				const uint32 b = (uint32)pv.Size();
				pv.PushBack(nv);
				vsel.PushBack(1);
				fv.PushBack(a); // arête fil a-b
				fv.PushBack(b);
				fs.PushBack((uint32)fv.Size());
			}
			BuildFromPolygons(pv.Data(), (uint32)pv.Size(), fs.Data(), (uint32)fs.Size() - 1, fv.Data());
			ApplyVertSel(vsel);
			return true;
		}

		// EXTRUDE ARÊTES (mode EDGE) : chaque arête dont les 2 extrémités sont sélectionnées
		// engendre une NOUVELLE arête (sommets dupliqués, partagés entre arêtes voisines) et
		// une FACE quad reliante (a,b,b',a'). Sélection = les nouveaux sommets. Offset 0 par
		// défaut (comportement Blender : le quad est plat tant que l'utilisateur n'a pas bougé).
		bool NkEditMesh::ExtrudeSelectedEdges(const NkExtrudeParams &p) {
			NkVector<uint32> pairs;
			GetUniqueEdges(pairs);
			NkVector<uint32> selA, selB;
			for (uint32 e = 0; e + 1 < (uint32)pairs.Size(); e += 2) {
				const uint32 a = pairs[e], b = pairs[e + 1];
				if (a >= (uint32)verts.Size() || b >= (uint32)verts.Size())
					continue;
				if (verts[a].sel && verts[b].sel) {
					selA.PushBack(a);
					selB.PushBack(b);
				}
			}
			if (selA.Empty())
				return false;
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			ToPolygons(pv, fs, fv);
			const uint32 baseVerts = (uint32)pv.Size();
			const float32 off = (p.offset > 0.f) ? p.offset : 0.f;
			NkVector<int32> dup; // sommet d'origine -> son duplicata (partagé)
			dup.Resize(baseVerts);
			for (uint32 i = 0; i < baseVerts; i++)
				dup[i] = -1;
			NkVector<uint8> vsel;
			vsel.Resize(baseVerts);
			for (uint32 i = 0; i < baseVerts; i++)
				vsel[i] = 0;
			auto dupOf = [&](uint32 v) -> uint32 {
				if (dup[v] >= 0)
					return (uint32)dup[v];
				NkVertex3D nv = pv[v];
				nv.pos = nv.pos + verts[v].normal * off;
				const uint32 id = (uint32)pv.Size();
				pv.PushBack(nv);
				vsel.PushBack(1);
				dup[v] = (int32)id;
				return id;
			};
			for (uint32 k = 0; k < (uint32)selA.Size(); k++) {
				const uint32 a = selA[k], b = selB[k];
				const uint32 na = dupOf(a), nb = dupOf(b);
				fv.PushBack(a);
				fv.PushBack(b);
				fv.PushBack(nb);
				fv.PushBack(na);
				fs.PushBack((uint32)fv.Size());
			}
			BuildFromPolygons(pv.Data(), (uint32)pv.Size(), fs.Data(), (uint32)fs.Size() - 1, fv.Data());
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
			NkVector<int32> map;
			map.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)map.Size(); i++)
				map[i] = (int32)i;

			if (p.mode == NkMergeParams::Collapse) {
				// COLLAPSE : un merge PAR ILOT CONNEXE de la selection, chacun vers son
				// propre centre — c'est ce qui le distingue de Center (merge global).
				// Connexite etablie sur l'identite SOUDEE via les aretes : union-find.
				NkVector<uint32> canon;
				BuildVertexMerge(canon);
				auto cn = [&](uint32 v) { return (v < (uint32)canon.Size()) ? canon[v] : v; };
				NkVector<uint32> parent;
				parent.Resize((uint32)pv.Size());
				for (uint32 i = 0; i < (uint32)parent.Size(); i++)
					parent[i] = i;
				auto find = [&](uint32 x) {
					while (parent[x] != x) {
						parent[x] = parent[parent[x]];
						x = parent[x];
					}
					return x;
				};
				NkVector<uint32> pairs;
				GetUniqueEdges(pairs);
				auto selC = [&](uint32 v) { return v < (uint32)verts.Size() && verts[v].sel != 0; };
				for (uint32 e = 0; e + 1 < (uint32)pairs.Size(); e += 2) {
					const uint32 a = pairs[e], b = pairs[e + 1];
					if (selC(a) && selC(b)) {
						const uint32 ra = find(cn(a)), rb = find(cn(b));
						if (ra != rb)
							parent[ra] = rb;
					}
				}
				// Centre par ilot : chaque identite soudee comptee UNE fois (les copies
				// coincidentes fausseraient la moyenne).
				NkHashMap<uint32, uint32> repOf;   // racine -> sommet representant
				NkHashMap<uint64, uint8> counted;  // (racine<<32|canonId) deja compte
				NkHashMap<uint32, NkVec3f> sum;
				NkHashMap<uint32, uint32> cnt;
				for (uint32 i = 0; i < (uint32)pv.Size(); i++) {
					if (!selC(i))
						continue;
					const uint32 r = find(cn(i));
					if (!repOf.Find(r))
						repOf.InsertOrAssign(r, i);
					const uint64 key = ((uint64)r << 32) | cn(i);
					if (!counted.Find(key)) {
						counted.InsertOrAssign(key, (uint8)1);
						NkVec3f *s = sum.Find(r);
						if (s)
							*s = *s + pv[i].pos;
						else
							sum.InsertOrAssign(r, pv[i].pos);
						uint32 *k2 = cnt.Find(r);
						if (k2)
							(*k2)++;
						else
							cnt.InsertOrAssign(r, 1u);
					}
				}
				for (uint32 i = 0; i < (uint32)pv.Size(); i++) {
					if (!selC(i))
						continue;
					const uint32 r = find(cn(i));
					const uint32 rep2 = *repOf.Find(r);
					map[i] = (int32)rep2;
					pv[rep2].pos = *sum.Find(r) * (1.f / (float32)(*cnt.Find(r)));
				}
			} else if (p.mode == NkMergeParams::ByDistance) {
				// BY DISTANCE (« Remove Doubles ») : grappes de sommets selectionnes plus
				// proches que le seuil, chaque grappe vers son centre. Quantification
				// spatiale au pas du seuil — meme technique que BuildVertexMerge, mais au
				// seuil UTILISATEUR et restreinte a la selection.
				float32 eps = p.distance;
				if (eps <= 0.f) {
					NkVec3f mn = pv[0].pos, mx = pv[0].pos;
					for (uint32 i = 1; i < (uint32)pv.Size(); i++) {
						mn.x = NkMin(mn.x, pv[i].pos.x); mn.y = NkMin(mn.y, pv[i].pos.y); mn.z = NkMin(mn.z, pv[i].pos.z);
						mx.x = NkMax(mx.x, pv[i].pos.x); mx.y = NkMax(mx.y, pv[i].pos.y); mx.z = NkMax(mx.z, pv[i].pos.z);
					}
					eps = (mx - mn).Len() * 0.001f; // 0,1 % de la diagonale
					if (eps <= 0.f)
						eps = 1e-4f;
				}
				const float32 inv = 1.f / eps;
				NkHashMap<uint64, uint32> cell; // cle spatiale -> representant
				NkHashMap<uint64, NkVec3f> csum;
				NkHashMap<uint64, uint32> ccnt;
				bool merged = false;
				for (uint32 i = 0; i < (uint32)pv.Size(); i++) {
					if (!(i < (uint32)verts.Size() && verts[i].sel))
						continue;
					const NkVec3f q = pv[i].pos;
					const int64 qx = (int64)(q.x * inv + (q.x >= 0.f ? 0.5f : -0.5f));
					const int64 qy = (int64)(q.y * inv + (q.y >= 0.f ? 0.5f : -0.5f));
					const int64 qz = (int64)(q.z * inv + (q.z >= 0.f ? 0.5f : -0.5f));
					const uint64 key = ((uint64)(qx & 0x1FFFFF)) | (((uint64)(qy & 0x1FFFFF)) << 21) |
									   (((uint64)(qz & 0x1FFFFF)) << 42);
					uint32 *rep2 = cell.Find(key);
					if (rep2) {
						map[i] = (int32)(*rep2);
						merged = true;
						NkVec3f *s = csum.Find(key);
						*s = *s + q;
						(*ccnt.Find(key))++;
					} else {
						cell.InsertOrAssign(key, i);
						csum.InsertOrAssign(key, q);
						ccnt.InsertOrAssign(key, 1u);
					}
				}
				if (!merged)
					return false; // aucun couple sous le seuil : rien a faire
				for (auto it = cell.Begin(); it != cell.End(); ++it)
					pv[it->Second].pos = (*csum.Find(it->First)) * (1.f / (float32)(*ccnt.Find(it->First)));
			} else {
				// Center / First / Last / AtCursor : UN seul representant global.
				const int32 rep = (p.mode == NkMergeParams::Last) ? last : first;
				NkVec3f target = (p.mode == NkMergeParams::First)	   ? pv[(uint32)first].pos
								 : (p.mode == NkMergeParams::Last)	   ? pv[(uint32)last].pos
								 : (p.mode == NkMergeParams::AtCursor) ? p.point
																	   : c;
				pv[(uint32)rep].pos = target;
				for (uint32 i = 0; i < (uint32)pv.Size(); i++)
					if (i < (uint32)verts.Size() && verts[i].sel)
						map[i] = rep;
			}
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
		// F (« Make Face ») façon Blender : crée UNE face à partir des sommets sélectionnés.
		// DEUX PIÈGES corrigés ici, tous deux invisibles en SOLIDE et flagrants en FIL DE FER :
		//  1. COPIES COÏNCIDENTES. Les primitives dupliquent leurs coins PAR FACE (un coin de
		//     cube = 3 sommets distincts au même endroit) et la sélection se propage aux
		//     copies (« flushing »). Prendre bêtement tous les `verts[i].sel` transformait donc
		//     « 4 sommets » en 12 -> face à 12 côtés. On ne garde qu'UN représentant par
		//     sommet TOPOLOGIQUE (identité canonique BuildVertexMerge).
		//  2. ORDRE DU CONTOUR. Les sommets étaient poussés dans l'ORDRE DES INDICES, qui n'a
		//     aucune raison de suivre le contour : le polygone zigzaguait entre les coins et
		//     ses arêtes traversaient la face — exactement l'aspect « la face est faite de deux
		//     triangles » signalé. On les ORDONNE maintenant angulairement autour de leur
		//     barycentre, dans le plan de meilleur ajustement -> contour simple, non croisé.

		// ── ARETES DE PREMIER PLAN (etape 1 BMesh) ──────────────────────────────
		void NkEditMesh::RebuildEdges() {
			// Les aretes FILAIRES sont conservees : elles ne sont incidentes a aucune
			// face, donc aucune reconstruction depuis les demi-aretes ne pourrait les
			// retrouver. C'est toute la raison d'etre de cette liste.
			NkVector<Edge> wires;
			for (uint32 i = 0; i < (uint32)edges.Size(); ++i)
				if (edges[i].alive && edges[i].faceCount == 0 && edges[i].hedge == NK_EM_INVALID)
					wires.PushBack(edges[i]);

			// Identite SOUDEE : deux sommets exactement au meme endroit sont une seule
			// identite topologique. Sans cela, un cube (24 sommets dupliques par face)
			// donnerait 24 aretes distinctes la ou il n'y en a que 12.
			NkVector<uint32> canon;
			BuildVertexMerge(canon);
			auto cn = [&](uint32 v) { return (v < (uint32)canon.Size()) ? canon[v] : v; };

			edges.Clear();
			NkHashMap<uint64, uint32> seen;
			NkVector<NkEmId> loop;
			for (uint32 f = 0; f < (uint32)faces.Size(); ++f) {
				if (!faces[f].alive)
					continue;
				loop.Clear();
				GetFaceVerts((NkEmId)f, loop);
				if (loop.Size() < 3)
					continue;
				const NkEmId start = faces[f].hedge;
				NkEmId hh = start;
				uint32 guard = 0;
				do {
					const uint32 o = cn(hedges[hh].origin);
					const uint32 d = cn(hedges[hedges[hh].next].origin);
					if (o != d) {
						const uint64 lo = o < d ? o : d, hi = o < d ? d : o;
						const uint64 key = (lo << 32) | hi;
						uint32 *ex = seen.Find(key);
						if (ex) {
							if (edges[*ex].faceCount < 255)
								edges[*ex].faceCount++;
						} else {
							Edge e{};
							e.v0 = (NkEmId)lo;
							e.v1 = (NkEmId)hi;
							e.hedge = hh;
							e.faceCount = 1;
							e.alive = 1;
							seen.InsertOrAssign(key, (uint32)edges.Size());
							edges.PushBack(e);
						}
					}
					hh = hedges[hh].next;
				} while (hh != start && hh != NK_EM_INVALID && ++guard < 100000u);
			}

			// Reinsere les filaires, sauf si une face les a entre-temps recouvertes
			// (une arete filaire qui devient bord d'une face n'est plus filaire).
			for (uint32 i = 0; i < (uint32)wires.Size(); ++i) {
				const uint32 o = cn(wires[i].v0), d = cn(wires[i].v1);
				if (o == d)
					continue;
				const uint64 lo = o < d ? o : d, hi = o < d ? d : o;
				if (seen.Find((lo << 32) | hi))
					continue;
				Edge e{};
				e.v0 = (NkEmId)lo;
				e.v1 = (NkEmId)hi;
				e.hedge = NK_EM_INVALID;
				e.faceCount = 0;
				e.alive = 1;
				edges.PushBack(e);
			}
		}

		uint32 NkEditMesh::EdgeCount() const {
			uint32 n = 0;
			for (uint32 i = 0; i < (uint32)edges.Size(); ++i)
				if (edges[i].alive)
					n++;
			return n;
		}

		NkEmId NkEditMesh::AddWireEdge(uint32 a, uint32 b) {
			if (a >= (uint32)verts.Size() || b >= (uint32)verts.Size())
				return NK_EM_INVALID;
			NkVector<uint32> canon;
			BuildVertexMerge(canon);
			const uint32 ca = (a < (uint32)canon.Size()) ? canon[a] : a;
			const uint32 cb = (b < (uint32)canon.Size()) ? canon[b] : b;
			if (ca == cb)
				return NK_EM_INVALID; // meme sommet topologique : pas d'arete a creer
			if (edges.Empty())
				RebuildEdges();
			const uint32 lo = ca < cb ? ca : cb, hi = ca < cb ? cb : ca;
			for (uint32 i = 0; i < (uint32)edges.Size(); ++i)
				if (edges[i].alive && edges[i].v0 == lo && edges[i].v1 == hi)
					return (NkEmId)i; // deja presente (bord de face ou filaire)
			Edge e{};
			e.v0 = (NkEmId)lo;
			e.v1 = (NkEmId)hi;
			e.hedge = NK_EM_INVALID; // FILAIRE : aucune face incidente
			e.faceCount = 0;
			e.alive = 1;
			edges.PushBack(e);
			return (NkEmId)(edges.Size() - 1);
		}

		bool NkEditMesh::MakeEdgeFromSelected() {
			// Un seul REPRESENTANT par sommet topologique : les primitives dupliquent
			// leurs sommets par face, donc « deux sommets selectionnes » peut vouloir
			// dire six indices bruts pointant deux positions.
			NkVector<uint32> canon;
			BuildVertexMerge(canon);
			NkHashMap<uint32, uint8> taken;
			NkVector<uint32> sel;
			for (uint32 i = 0; i < (uint32)verts.Size(); ++i) {
				if (!verts[i].sel)
					continue;
				const uint32 cc = (i < (uint32)canon.Size()) ? canon[i] : i;
				if (taken.Find(cc))
					continue;
				taken.InsertOrAssign(cc, (uint8)1);
				sel.PushBack(i);
			}
			if (sel.Size() != 2)
				return false;
			return AddWireEdge(sel[0], sel[1]) != NK_EM_INVALID;
		}

		bool NkEditMesh::MakeFaceFromSelected() {
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			ToPolygons(pv, fs, fv);
			// ── 1) UN SEUL REPRÉSENTANT PAR SOMMET TOPOLOGIQUE ──────────────────────
			NkVector<uint32> canon;
			BuildVertexMerge(canon);
			const uint32 ncv = (uint32)canon.Size();
			NkHashMap<uint32, uint8> taken;
			NkVector<uint32> sel;
			for (uint32 i = 0; i < (uint32)verts.Size() && i < (uint32)pv.Size(); i++) {
				if (!verts[i].sel)
					continue;
				const uint32 c = (i < ncv) ? canon[i] : i;
				if (taken.Find(c) != nullptr)
					continue; // copie coïncidente du même sommet -> ignorée
				taken.InsertOrAssign(c, (uint8)1);
				sel.PushBack(i);
			}
			if (sel.Size() < 3)
				return false;
			// ── 2) ORDRE DU CONTOUR : tri angulaire dans le plan de meilleur ajustement ──
			const uint32 sn = (uint32)sel.Size();
			NkVec3f ctr{0.f, 0.f, 0.f};
			for (uint32 k = 0; k < sn; k++)
				ctr = ctr + pv[sel[k]].pos;
			ctr = ctr * (1.f / (float32)sn);
			// Normale du plan = le plus GRAND produit vectoriel trouvé entre deux rayons
			// (robuste : une somme « à la Newell » sur un ordre arbitraire peut s'annuler).
			// Recherche bornée aux 32 premiers sommets -> coût constant même sur « tout
			// sélectionner puis F ».
			NkVec3f nrm{0.f, 0.f, 0.f};
			float32 bestA = 0.f;
			const uint32 scan = (sn > 32u) ? 32u : sn;
			for (uint32 a = 0; a < scan; a++)
				for (uint32 b = a + 1; b < scan; b++) {
					const NkVec3f cr = (pv[sel[a]].pos - ctr).Cross(pv[sel[b]].pos - ctr);
					const float32 l = cr.Len();
					if (l > bestA) {
						bestA = l;
						nrm = cr;
					}
				}
			if (bestA > 1e-12f) {
				nrm = nrm * (1.f / bestA);
				// Base orthonormée du plan.
				NkVec3f u = pv[sel[0]].pos - ctr;
				u = u - nrm * u.Dot(nrm);
				float32 ul = u.Len();
				if (ul < 1e-8f) { // le 1er sommet est au barycentre : on prend n'importe quel axe
					u = (fabsf(nrm.x) < 0.9f) ? NkVec3f{1.f, 0.f, 0.f} : NkVec3f{0.f, 1.f, 0.f};
					u = u - nrm * u.Dot(nrm);
					ul = u.Len();
				}
				if (ul > 1e-8f) {
					u = u * (1.f / ul);
					const NkVec3f v = nrm.Cross(u);
					// Tri par insertion sur l'angle (zéro STL, zéro allocation).
					NkVector<float32> ang;
					ang.Resize(sn);
					for (uint32 k = 0; k < sn; k++) {
						const NkVec3f d = pv[sel[k]].pos - ctr;
						ang[k] = atan2f(d.Dot(v), d.Dot(u));
					}
					for (uint32 k = 1; k < sn; k++) {
						const float32 av = ang[k];
						const uint32 iv = sel[k];
						uint32 j = k;
						while (j > 0 && ang[j - 1] > av) {
							ang[j] = ang[j - 1];
							sel[j] = sel[j - 1];
							j--;
						}
						ang[j] = av;
						sel[j] = iv;
					}
				}
			}
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

		// LOOP CUT : depuis une ARÊTE sélectionnée, traverse l'ANNEAU de quads et insère
		// p.cuts boucles d'arêtes RÉGULIÈREMENT ESPACÉES (sommets PARTAGÉS entre quads
		// voisins de l'anneau). Maillages quad (façon Blender, Ctrl+R).
		// Limite assumée : pas d'aperçu au survol ni de « slide » modal — les coupes sont
		// posées aux fractions k/(cuts+1) de l'anneau, comme un Ctrl+R validé sans slide.
		bool NkEditMesh::LoopCutFromSelectedEdge(const NkLoopCutParams &p) {
			const int32 cuts = (p.cuts < 1) ? 1 : ((p.cuts > 32) ? 32 : p.cuts);
			// SLIDE (edge slide de Blender) : glisse les boucles insérées le long de l'anneau.
			// 0 = position médiane (comportement historique, strictement inchangé).
			const float32 slide = (p.slide < -1.f) ? -1.f : ((p.slide > 1.f) ? 1.f : p.slide);
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
			// L'anneau est identifié sur l'IDENTITÉ TOPOLOGIQUE (sommets soudés) : deux faces
			// voisines n'utilisent pas les mêmes INDICES pour l'arête qu'elles partagent
			// (attributs par coin), mais bien le même représentant canonique.
			NkVector<uint32> canon;
			BuildVertexMerge(canon);
			const uint32 ncv = (uint32)canon.Size();
			auto CV = [&](uint32 v) -> uint32 { return (v < ncv) ? canon[v] : v; };
			// Valeur stockée = ORIENTATION de l'arête dans l'anneau : 1 => le sens « positif »
			// (celui du SLIDE) va de `lo` vers `hi` ; 2 => il va de `hi` vers `lo`.
			// Nécessaire parce que l'ordre canonique lo->hi est arbitraire : sans cette
			// mémoire, une arête sur deux glisserait à contresens et la boucle se tordrait.
			NkHashMap<uint64, uint8> ring;
			auto addE = [&](uint32 a0, uint32 b0, bool posIsAtoB) {
				const uint32 a = CV(a0), b = CV(b0);
				uint32 lo = a < b ? a : b, hi = a < b ? b : a;
				const bool posLoHi = posIsAtoB ? (a < b) : (b < a);
				ring.InsertOrAssign(((uint64)lo << 32) | hi, (uint8)(posLoHi ? 1 : 2));
			};
			NkEmId h = h0;
			uint32 guard = 0;
			do {
				uint32 o = hedges[h].origin, d = hedges[hedges[h].next].origin;
				// Convention : le sens positif suit la demi-arête courante (o -> d). Le quad
				// étant parcouru q0->q1->q2->q3 avec h = (q0,q1) et hOpp = (q2,q3), le sommet
				// de coupe de `h` près de q0 fait face à celui de `hOpp` près de q3 : le sens
				// positif sur hOpp est donc q3 -> q2, soit l'INVERSE de (origin -> dest).
				addE(o, d, true);
				if (FaceSize(hedges[h].face) != 4)
					break;								   // anneau uniquement à travers des quads
				NkEmId hOpp = hedges[hedges[h].next].next; // arête opposée du quad
				addE(hedges[hOpp].origin, hedges[hedges[hOpp].next].origin, false);
				NkEmId tw = hedges[hOpp].twin;
				if (tw == NK_EM_INVALID)
					break; // bord -> anneau ouvert
				h = tw;
				// (la prochaine itération réécrit l'orientation de hOpp via son twin, avec
				//  EXACTEMENT la même valeur : twin(hOpp) va bien de q3 vers q2.)
			} while (h != h0 && ++guard < 100000u);
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			ToPolygons(pv, fs, fv);
			auto isRing = [&](uint32 a0, uint32 b0) -> bool {
				const uint32 a = CV(a0), b = CV(b0);
				uint32 lo = a < b ? a : b, hi = a < b ? b : a;
				return ring.Find(((uint64)lo << 32) | hi) != nullptr;
			};
			// Chaque arête de l'anneau reçoit `cuts` sommets, créés d'un bloc et INDEXÉS
			// DE `lo` VERS `hi` (ordre canonique) -> les 2 quads voisins d'une même arête
			// retrouvent EXACTEMENT les mêmes sommets : la boucle est soudée, pas dédoublée.
			NkHashMap<uint64, uint32> emid;
			auto edgeCutBase = [&](uint32 a0, uint32 b0) -> uint32 {
				// Clé CANONIQUE : les deux faces voisines qui partagent l'arête retrouvent les
				// MÊMES sommets de coupe -> la boucle insérée est soudée, pas dédoublée.
				const uint32 a = CV(a0), b = CV(b0);
				uint32 lo = a < b ? a : b, hi = a < b ? b : a;
				uint64 key = ((uint64)lo << 32) | hi;
				uint32 *q = emid.Find(key);
				if (q)
					return *q;
				// SLIDE : le sens positif de CETTE arête vient de la table `ring` (établie en
				// parcourant l'anneau). `sp` est le glissement ramené dans le repère lo->hi.
				const uint8 *ro = ring.Find(key);
				const bool posLoHi = (ro == nullptr) || (*ro != 2u);
				const float32 sp = posLoHi ? slide : -slide;
				const uint32 base = (uint32)pv.Size();
				for (int32 c = 0; c < cuts; c++) {
					float32 t = (float32)(c + 1) / (float32)(cuts + 1);
					// Glissement vers la boucle bordante `hi` (sp > 0) ou `lo` (sp < 0) :
					// t' = t + |sp| * (cible - t) -> à |sp| = 1 toutes les coupes se rabattent
					// exactement sur la boucle visée, comme le edge slide de Blender.
					if (sp != 0.f) {
						const float32 target = (sp > 0.f) ? 1.f : 0.f;
						const float32 a2 = (sp < 0.f) ? -sp : sp;
						t = t + a2 * (target - t);
						t = (t < 0.001f) ? 0.001f : ((t > 0.999f) ? 0.999f : t);
					}
					NkVertex3D nv = pv[lo];
					nv.pos = pv[lo].pos + (pv[hi].pos - pv[lo].pos) * t;
					nv.uv = pv[lo].uv + (pv[hi].uv - pv[lo].uv) * t;
					pv.PushBack(nv);
				}
				emid.InsertOrAssign(key, base);
				return base;
			};
			// Les `cuts` sommets de l'arête (a,b) RANGÉS DANS LE SENS a -> b.
			auto edgeCutsDir = [&](uint32 a, uint32 b, NkVector<uint32> &out) {
				out.Clear();
				const uint32 base = edgeCutBase(a, b);
				const bool fwd = (CV(a) < CV(b)); // les sommets sont stockés de lo vers hi
				for (int32 c = 0; c < cuts; c++)
					out.PushBack(base + (uint32)(fwd ? c : (cuts - 1 - c)));
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
					// A = coupes de l'arête (q0,q1) dans le sens q0->q1 ;
					// B = coupes de l'arête opposée (q2,q3) dans le sens q2->q3.
					// La boucle du quad étant q0->q1->q2->q3, A[i] fait face à B[cuts-1-i].
					NkVector<uint32> A, B;
					edgeCutsDir(q0, q1, A);
					edgeCutsDir(q2, q3, B);
					uint32 mx = 0;
					for (int32 c = 0; c < cuts; c++) {
						if (A[(uint32)c] > mx)
							mx = A[(uint32)c];
						if (B[(uint32)c] > mx)
							mx = B[(uint32)c];
					}
					if ((uint32)vsel.Size() <= mx)
						vsel.Resize(mx + 1);
					for (int32 c = 0; c < cuts; c++) {
						vsel[A[(uint32)c]] = 1;
						vsel[B[(uint32)c]] = 1;
					}
					changed = true;
					// Bande 0 : q0, A0, B(cuts-1), q3
					nfv.PushBack(q0);
					nfv.PushBack(A[0]);
					nfv.PushBack(B[(uint32)(cuts - 1)]);
					nfv.PushBack(q3);
					nfs.PushBack((uint32)nfv.Size());
					// Bandes intermédiaires : Ai, Ai+1, B(cuts-2-i), B(cuts-1-i)
					for (int32 c = 0; c + 1 < cuts; c++) {
						nfv.PushBack(A[(uint32)c]);
						nfv.PushBack(A[(uint32)(c + 1)]);
						nfv.PushBack(B[(uint32)(cuts - 2 - c)]);
						nfv.PushBack(B[(uint32)(cuts - 1 - c)]);
						nfs.PushBack((uint32)nfv.Size());
					}
					// Bande finale : A(cuts-1), q1, q2, B0
					nfv.PushBack(A[(uint32)(cuts - 1)]);
					nfv.PushBack(q1);
					nfv.PushBack(q2);
					nfv.PushBack(B[0]);
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

		// =====================================================================
		// OUTILS COMMUNS AUX OPÉRATIONS « TOPOLOGIE SOUDÉE » (bevel, inset, split, spin)
		// ---------------------------------------------------------------------
		// Ces opérations ont besoin d'une VRAIE adjacence : savoir quelles faces bordent
		// une arête, tourner autour d'un sommet. Or les primitives/imports dupliquent les
		// coins PAR FACE (cube = 24 sommets pour 8 positions) : dans l'espace des INDICES
		// bruts, deux faces voisines ne partagent aucun sommet. On travaille donc sur une
		// copie SOUDÉE (un sommet par position, cf. BuildVertexMerge) — exactement le
		// modèle Blender : maillage soudé, attributs portés par les coins.
		// =====================================================================
		static inline NkVec3f EM_Norm(const NkVec3f &v) {
			const float32 l = v.Len();
			return (l > 1e-8f) ? v * (1.f / l) : NkVec3f{0.f, 0.f, 0.f};
		}

		// Diagonale de la boîte englobante = ÉCHELLE du maillage (offsets AUTO).
		static float32 EM_BBoxDiag(const NkVector<NkVertex3D> &pv) {
			if (pv.Empty())
				return 0.f;
			NkVec3f mn = pv[0].pos, mx = pv[0].pos;
			for (uint32 i = 1; i < (uint32)pv.Size(); ++i) {
				const NkVec3f q = pv[i].pos;
				mn.x = (q.x < mn.x) ? q.x : mn.x;
				mn.y = (q.y < mn.y) ? q.y : mn.y;
				mn.z = (q.z < mn.z) ? q.z : mn.z;
				mx.x = (q.x > mx.x) ? q.x : mx.x;
				mx.y = (q.y > mx.y) ? q.y : mx.y;
				mx.z = (q.z > mx.z) ? q.z : mx.z;
			}
			return (mx - mn).Len();
		}

		// Polygones SOUDÉS : un sommet par position (représentant du groupe coïncident).
		// vsel = sélection soudée (OU logique du groupe) ; wmap[i] = indice soudé de i.
		static void EM_ToWeldedPolygons(const NkEditMesh &m, NkVector<NkVertex3D> &pv, NkVector<uint32> &fs,
										NkVector<uint32> &fv, NkVector<uint8> &vsel, NkVector<uint32> &wmap) {
			NkVector<NkVertex3D> rv;
			NkVector<uint32> rfs, rfv;
			m.ToPolygons(rv, rfs, rfv);
			NkVector<uint32> canon;
			m.BuildVertexMerge(canon);
			const uint32 n = (uint32)rv.Size();
			NkVector<int32> newIdx;
			newIdx.Resize(n);
			for (uint32 i = 0; i < n; ++i)
				newIdx[i] = -1;
			pv.Clear();
			vsel.Clear();
			wmap.Resize(n);
			for (uint32 i = 0; i < n; ++i) {
				const uint32 c = (i < (uint32)canon.Size()) ? canon[i] : i;
				if (newIdx[c] < 0) {
					newIdx[c] = (int32)pv.Size();
					pv.PushBack(rv[c]);
					vsel.PushBack(0);
				}
				wmap[i] = (uint32)newIdx[c];
				if (i < m.VertCount() && m.verts[i].sel)
					vsel[wmap[i]] = 1;
			}
			fs.Clear();
			fv.Clear();
			fs.PushBack(0);
			const uint32 fc = (rfs.Size() > 0) ? (uint32)rfs.Size() - 1 : 0;
			for (uint32 f = 0; f < fc; ++f) {
				const uint32 s = rfs[f], e = rfs[f + 1];
				const uint32 st = (uint32)fv.Size();
				const bool wire = (e - s) < 3u; // arête FIL : à préserver telle quelle
				for (uint32 k = s; k < e; ++k) {
					const uint32 w = wmap[rfv[k]];
					if ((uint32)fv.Size() > st && fv[(uint32)fv.Size() - 1] == w)
						continue; // doublon consécutif né de la soudure
					fv.PushBack(w);
				}
				while ((uint32)fv.Size() > st + 1u && fv[(uint32)fv.Size() - 1] == fv[st])
					fv.Resize((uint32)fv.Size() - 1);
				const uint32 got = (uint32)fv.Size() - st;
				if (got < 2u || (!wire && got < 3u)) {
					fv.Resize(st);
					continue;
				} // face effondrée
				fs.PushBack((uint32)fv.Size());
			}
		}

		// =====================================================================
		// BEVEL / CHANFREIN (Ctrl+B · Ctrl+Shift+B) — algorithme PAR COIN
		// ---------------------------------------------------------------------
		// Idée : chaque COIN (face, sommet) est remplacé par 1 ou 2 points, selon que ses
		// deux arêtes sont chanfreinées ou non :
		//   • aucune des deux, sommet non touché      -> 1 point : le sommet lui-même ;
		//   • aucune des deux, sommet TOUCHÉ          -> 2 points, reculés le long des deux
		//                                                arêtes (c'est ce qui transforme la
		//                                                face « du bout » en n-gon) ;
		//   • une seule chanfreinée                   -> 1 point, reculé le long de l'AUTRE ;
		//   • les deux chanfreinées                   -> 1 point INTÉRIEUR à la face.
		// Les points « reculés le long d'une arête » sont MÉMORISÉS par clé (sommet, arête) :
		// les deux faces qui partagent une arête non chanfreinée obtiennent donc le MÊME
		// point -> aucune fissure. Ensuite :
		//   (a) chaque face d'origine est ré-émise avec ses coins remplacés ;
		//   (b) chaque arête chanfreinée engendre une BANDE de `segments` quads ;
		//   (c) chaque sommet touché engendre une face de RACCORD si l'anneau de points
		//       autour de lui n'est pas dégénéré (coin où >= 3 arêtes sont chanfreinées,
		//       bevel de sommet, ou bevel arrondi dont l'arc creuse le coin).
		// Le bevel de SOMMET est le MÊME code avec « aucune arête chanfreinée, sommets
		// sélectionnés touchés » : chaque face incidente gagne un point, l'anneau devient
		// la petite face de coin.
		// =====================================================================
		bool NkEditMesh::BevelSelected(const NkBevelParams &p) {
			NkVector<NkVertex3D> wv;
			NkVector<uint32> wfs, wfv;
			NkVector<uint8> wsel;
			NkVector<uint32> wmap;
			EM_ToWeldedPolygons(*this, wv, wfs, wfv, wsel, wmap);
			const uint32 wfc = (wfs.Size() > 0) ? (uint32)wfs.Size() - 1 : 0;
			if (wfc == 0)
				return false;
			NkEditMesh W;
			W.BuildFromPolygons(wv.Data(), (uint32)wv.Size(), wfs.Data(), wfc, wfv.Data());
			const uint32 NV = W.VertCount();
			for (uint32 i = 0; i < NV && i < (uint32)wsel.Size(); ++i)
				W.verts[i].sel = wsel[i];
			const uint32 HC = (uint32)W.hedges.Size();
			if (HC == 0 || NV == 0)
				return false;

			// prevOf[h] : demi-arête précédente dans la boucle de face. Le COIN « h » est
			// délimité par prevOf[h] (arête entrante) et h (arête sortante).
			NkVector<NkEmId> prevOf;
			prevOf.Resize(HC);
			for (uint32 i = 0; i < HC; ++i)
				prevOf[i] = NK_EM_INVALID;
			for (uint32 f = 0; f < (uint32)W.faces.Size(); ++f) {
				if (!W.faces[f].alive || W.faces[f].hedge == NK_EM_INVALID)
					continue;
				const NkEmId s = W.faces[f].hedge;
				NkEmId h = s;
				uint32 g = 0;
				do {
					const NkEmId nx = W.hedges[h].next;
					if (nx == NK_EM_INVALID)
						break;
					prevOf[nx] = h;
					h = nx;
				} while (h != s && ++g < 100000u);
			}

			auto dstOf = [&](NkEmId h) -> uint32 {
				const NkEmId nx = W.hedges[h].next;
				return (nx == NK_EM_INVALID) ? W.hedges[h].origin : W.hedges[nx].origin;
			};
			auto ekey = [](uint32 a, uint32 b) -> uint64 {
				const uint32 lo = (a < b) ? a : b, hi = (a < b) ? b : a;
				return ((uint64)lo << 32) | (uint64)hi;
			};

			// Arêtes CHANFREINÉES (les deux extrémités sélectionnées + un jumeau) et
			// sommets TOUCHÉS (mode sommet : simplement les sommets sélectionnés).
			NkHashMap<uint64, uint8> selE;
			NkVector<uint8> touched;
			touched.Resize(NV);
			for (uint32 i = 0; i < NV; ++i)
				touched[i] = 0;
			if (p.vertexOnly) {
				for (uint32 i = 0; i < NV; ++i)
					touched[i] = W.verts[i].sel ? (uint8)1 : (uint8)0;
			} else {
				for (uint32 h = 0; h < HC; ++h) {
					if (!W.hedges[h].alive || W.hedges[h].twin == NK_EM_INVALID)
						continue; // arête de BORD -> non chanfreinable (limite assumée)
					const uint32 a = W.hedges[h].origin, b = dstOf((NkEmId)h);
					if (a == b || a >= NV || b >= NV)
						continue;
					if (!W.verts[a].sel || !W.verts[b].sel)
						continue;
					selE.InsertOrAssign(ekey(a, b), (uint8)1);
					touched[a] = 1;
					touched[b] = 1;
				}
			}
			bool anyTouched = false;
			for (uint32 i = 0; i < NV && !anyTouched; ++i)
				anyTouched = (touched[i] != 0);
			if (!anyTouched)
				return false;

			float32 off = p.offset;
			if (off <= 0.f)
				off = EM_BBoxDiag(wv) * 0.06f;
			if (off <= 1e-7f)
				return false;
			int32 seg = (p.segments < 1) ? 1 : ((p.segments > 16) ? 16 : p.segments);
			if (p.vertexOnly)
				seg = 1; // un bevel de sommet produit UNE face de coin

			// ── Nuage de points de sortie (mémorisation par clé -> pas de fissure) ──
			NkVector<NkVertex3D> np;
			NkVector<uint8> nsel;
			NkHashMap<uint64, uint32> origPt, edgePt;
			auto pushPt = [&](const NkVertex3D &v, uint8 s) -> uint32 {
				const uint32 id = (uint32)np.Size();
				np.PushBack(v);
				nsel.PushBack(s);
				return id;
			};
			auto tAlong = [&](uint32 v, uint32 w) -> float32 {
				const float32 lim = (W.verts[w].pos - W.verts[v].pos).Len() * 0.45f;
				return (off > lim) ? lim : off; // jamais plus de 45 % de l'arête
			};
			auto getOrig = [&](uint32 v) -> uint32 {
				uint32 *q = origPt.Find((uint64)v);
				if (q)
					return *q;
				const uint32 id = pushPt(wv[v], W.verts[v].sel);
				origPt.InsertOrAssign((uint64)v, id);
				return id;
			};
			auto getEdgePt = [&](uint32 v, uint32 w) -> uint32 {
				const uint64 k = ((uint64)v << 32) | (uint64)w;
				uint32 *q = edgePt.Find(k);
				if (q)
					return *q;
				NkVertex3D nv = wv[v];
				nv.pos = W.verts[v].pos + EM_Norm(W.verts[w].pos - W.verts[v].pos) * tAlong(v, w);
				const uint32 id = pushPt(nv, (uint8)1);
				edgePt.InsertOrAssign(k, id);
				return id;
			};

			NkVector<uint32> ptPrev, ptNext;
			ptPrev.Resize(HC);
			ptNext.Resize(HC);
			for (uint32 h = 0; h < HC; ++h) {
				ptPrev[h] = 0;
				ptNext[h] = 0;
			}
			for (uint32 h = 0; h < HC; ++h) {
				if (!W.hedges[h].alive || W.hedges[h].face == NK_EM_INVALID)
					continue;
				const uint32 v = W.hedges[h].origin;
				const NkEmId hp = prevOf[h];
				if (v >= NV || hp == NK_EM_INVALID)
					continue;
				const uint32 pv2 = W.hedges[hp].origin, nv2 = dstOf((NkEmId)h);
				if (!touched[v]) {
					const uint32 id = getOrig(v);
					ptPrev[h] = id;
					ptNext[h] = id;
					continue;
				}
				const bool sp = (selE.Find(ekey(pv2, v)) != nullptr);
				const bool sn = (selE.Find(ekey(v, nv2)) != nullptr);
				if (!sp && !sn) {
					ptPrev[h] = getEdgePt(v, pv2);
					ptNext[h] = getEdgePt(v, nv2);
				} else if (sp && !sn) {
					const uint32 id = getEdgePt(v, nv2);
					ptPrev[h] = id;
					ptNext[h] = id;
				} else if (!sp && sn) {
					const uint32 id = getEdgePt(v, pv2);
					ptPrev[h] = id;
					ptNext[h] = id;
				} else {
					// Les deux arêtes du coin reculent : le point est l'INTERSECTION des deux
					// droites décalées. Pour un coin droit, cela vaut v + t1*u1 + t2*u2.
					const NkVec3f u1 = EM_Norm(W.verts[pv2].pos - W.verts[v].pos);
					const NkVec3f u2 = EM_Norm(W.verts[nv2].pos - W.verts[v].pos);
					float32 s = u1.Cross(u2).Len(); // sin de l'angle du coin
					if (s < 0.2f)
						s = 0.2f; // coin très aigu : on borne l'étirement
					NkVertex3D nvx = wv[v];
					nvx.pos = W.verts[v].pos + (u1 * tAlong(v, pv2) + u2 * tAlong(v, nv2)) * (1.f / s);
					const uint32 id = pushPt(nvx, (uint8)1);
					ptPrev[h] = id;
					ptNext[h] = id;
				}
			}

			// ── ARCS (segments > 1) : points intermédiaires du profil arrondi. Calculés
			// UNE SEULE FOIS par demi-arête chanfreinée, donc PARTAGÉS entre la bande et la
			// face de raccord -> pas de fissure. Slerp autour du sommet = arc de cercle.
			NkVector<int32> arcBase;
			arcBase.Resize(HC);
			for (uint32 h = 0; h < HC; ++h)
				arcBase[h] = -1;
			NkVector<uint32> arcData;
			if (seg > 1) {
				for (uint32 h = 0; h < HC; ++h) {
					if (!W.hedges[h].alive || W.hedges[h].face == NK_EM_INVALID)
						continue;
					const NkEmId tw = W.hedges[h].twin;
					if (tw == NK_EM_INVALID)
						continue;
					const uint32 v = W.hedges[h].origin, w2 = dstOf((NkEmId)h);
					if (!selE.Find(ekey(v, w2)))
						continue;
					const NkEmId rot = W.hedges[tw].next; // demi-arête suivante autour de v
					if (rot == NK_EM_INVALID)
						continue;
					const uint32 iA = ptNext[h], iB = ptPrev[rot];
					// CENTRE de l'arc : surtout PAS le sommet lui-même (le profil bomberait
					// HORS de la surface). C'est le « coin intérieur » : le point d'où les deux
					// extrémités du profil sont à la MÊME distance = la largeur du chanfrein.
					// Les deux extrémités valent v + Σ t*u sur des sous-ensembles d'arêtes ; leur
					// centre commun est v + Σ t*u sur l'UNION, soit P0 + P1 - v, moins la part
					// COMMUNE (la direction de l'arête chanfreinée) quand les deux extrémités
					// sont des points intérieurs de face.
					const NkEmId hp2 = prevOf[h];
					const bool p0Int = (hp2 != NK_EM_INVALID) && (selE.Find(ekey(W.hedges[hp2].origin, v)) != nullptr);
					const bool p1Int = (selE.Find(ekey(v, dstOf(rot))) != nullptr);
					NkVec3f c = np[iA].pos + np[iB].pos - W.verts[v].pos;
					if (p0Int && p1Int)
						c = c - EM_Norm(W.verts[w2].pos - W.verts[v].pos) * tAlong(v, w2);
					const NkVec3f r0 = np[iA].pos - c, r1 = np[iB].pos - c;
					const float32 l0 = r0.Len(), l1 = r1.Len();
					const NkVec3f e0 = EM_Norm(r0), e1 = EM_Norm(r1);
					NkVec3f ax = e0.Cross(e1);
					const float32 sn = ax.Len();
					float32 cs = e0.Dot(e1);
					cs = (cs > 1.f) ? 1.f : ((cs < -1.f) ? -1.f : cs);
					const bool arcOk = (l0 > 1e-6f && l1 > 1e-6f && sn > 1e-5f);
					if (arcOk)
						ax = ax * (1.f / sn);
					const float32 ang = atan2f(sn, cs);
					arcBase[h] = (int32)arcData.Size();
					for (int32 j = 1; j < seg; ++j) {
						const float32 t = (float32)j / (float32)seg;
						NkVertex3D nvx = np[iA];
						if (arcOk) { // Rodrigues : rotation de e0 autour de ax
							const float32 a = ang * t, ca = cosf(a), sa = sinf(a);
							const NkVec3f er = e0 * ca + ax.Cross(e0) * sa + ax * (ax.Dot(e0) * (1.f - ca));
							nvx.pos = c + er * (l0 + (l1 - l0) * t);
						} else {
							nvx.pos = np[iA].pos + (np[iB].pos - np[iA].pos) * t;
						}
						arcData.PushBack(pushPt(nvx, (uint8)1));
					}
				}
			}

			// ── Faces de sortie ──────────────────────────────────────────────
			NkVector<uint32> nfs, nfv;
			nfs.PushBack(0);
			auto pushCorner = [&](uint32 st, uint32 id) {
				if ((uint32)nfv.Size() > st && nfv[(uint32)nfv.Size() - 1] == id)
					return; // doublon consécutif
				nfv.PushBack(id);
			};
			auto endFace = [&](uint32 st, uint32 minN) -> bool {
				while ((uint32)nfv.Size() > st + 1u && nfv[(uint32)nfv.Size() - 1] == nfv[st])
					nfv.Resize((uint32)nfv.Size() - 1);
				if ((uint32)nfv.Size() - st < minN) {
					nfv.Resize(st); // anneau dégénéré -> pas de face
					return false;
				}
				nfs.PushBack((uint32)nfv.Size());
				return true;
			};
			// (a) faces d'origine, coins remplacés
			for (uint32 f = 0; f < (uint32)W.faces.Size(); ++f) {
				if (!W.faces[f].alive || W.faces[f].hedge == NK_EM_INVALID)
					continue;
				const uint32 minN = (W.FaceSize((NkEmId)f) >= 3u) ? 3u : 2u; // 2 = arête FIL
				const NkEmId s = W.faces[f].hedge;
				const uint32 st = (uint32)nfv.Size();
				NkEmId h = s;
				uint32 g = 0;
				do {
					pushCorner(st, ptPrev[h]);
					pushCorner(st, ptNext[h]);
					h = W.hedges[h].next;
				} while (h != s && h != NK_EM_INVALID && ++g < 100000u);
				endFace(st, minN);
			}
			// (b) BANDES de chanfrein (une par arête chanfreinée, `seg` quads chacune)
			NkVector<uint32> A, B;
			for (uint32 h = 0; h < HC; ++h) {
				if (!W.hedges[h].alive || W.hedges[h].face == NK_EM_INVALID)
					continue;
				const NkEmId tw = W.hedges[h].twin;
				if (tw == NK_EM_INVALID || (uint32)tw < h)
					continue; // une seule fois par arête
				const uint32 a = W.hedges[h].origin, b = dstOf((NkEmId)h);
				if (!selE.Find(ekey(a, b)))
					continue;
				const NkEmId rotA = W.hedges[tw].next, rotB = W.hedges[h].next;
				if (rotA == NK_EM_INVALID || rotB == NK_EM_INVALID)
					continue;
				A.Clear();
				B.Clear();
				A.PushBack(ptNext[h]); // côté A : a0 -> … -> a1
				if (arcBase[h] >= 0)
					for (int32 j = 0; j < seg - 1; ++j)
						A.PushBack(arcData[(uint32)arcBase[h] + (uint32)j]);
				A.PushBack(ptPrev[rotA]);
				B.PushBack(ptNext[tw]); // côté B : b1 -> … -> b0 (sens du tour autour de b)
				if (arcBase[tw] >= 0)
					for (int32 j = 0; j < seg - 1; ++j)
						B.PushBack(arcData[(uint32)arcBase[tw] + (uint32)j]);
				B.PushBack(ptPrev[rotB]);
				if ((int32)A.Size() != seg + 1 || (int32)B.Size() != seg + 1)
					continue;
				for (int32 j = 0; j < seg; ++j) {
					const uint32 st = (uint32)nfv.Size();
					pushCorner(st, B[(uint32)(seg - j)]);
					pushCorner(st, A[(uint32)j]);
					pushCorner(st, A[(uint32)(j + 1)]);
					pushCorner(st, B[(uint32)(seg - j - 1)]);
					endFace(st, 3u);
				}
			}
			// (c) faces de RACCORD aux sommets (anneau des points autour du sommet, parcouru
			//     à l'ENVERS pour que la face regarde vers l'extérieur).
			NkVector<uint32> ring, rr;
			for (uint32 v = 0; v < NV; ++v) {
				if (!touched[v] || W.verts[v].hedge == NK_EM_INVALID)
					continue;
				ring.Clear();
				bool open = false;
				const NkEmId h0 = W.verts[v].hedge;
				NkEmId h = h0;
				uint32 g = 0;
				do {
					ring.PushBack(ptPrev[h]);
					ring.PushBack(ptNext[h]);
					if (arcBase[h] >= 0)
						for (int32 j = 0; j < seg - 1; ++j)
							ring.PushBack(arcData[(uint32)arcBase[h] + (uint32)j]);
					const NkEmId tw = W.hedges[h].twin;
					if (tw == NK_EM_INVALID) {
						open = true;
						break;
					}
					h = W.hedges[tw].next;
					if (h == NK_EM_INVALID) {
						open = true;
						break;
					}
				} while (h != h0 && ++g < 4096u);
				if (open)
					continue; // sommet de BORD : pas de raccord (limite assumée)
				// Anneau parcouru à l'ENVERS (orientation sortante), compacté.
				rr.Clear();
				for (uint32 k = (uint32)ring.Size(); k > 0; --k) {
					const uint32 id = ring[k - 1];
					if (!rr.Empty() && rr[(uint32)rr.Size() - 1] == id)
						continue;
					rr.PushBack(id);
				}
				while (rr.Size() > 1u && rr[(uint32)rr.Size() - 1] == rr[0])
					rr.Resize((uint32)rr.Size() - 1);
				const uint32 rn = (uint32)rr.Size();
				if (rn < 3u)
					continue; // anneau dégénéré -> le coin est déjà fermé par les faces voisines
				if (rn <= 4u) {
					const uint32 st = (uint32)nfv.Size();
					for (uint32 k = 0; k < rn; ++k)
						nfv.PushBack(rr[k]);
					nfs.PushBack((uint32)nfv.Size());
					continue;
				}
				// COIN ARRONDI (bevel à plusieurs segments) : l'anneau est très NON PLAN.
				// Une seule n-gon serait triangulée en éventail depuis un de ses coins ->
				// bosses visibles. On pose donc un point central sur la sphère du coin et on
				// raccorde en éventail : coin lisse, faces quasi équilatérales.
				{
					// Le « coin intérieur » (centre de la sphère du coin) : v reculé le long de
					// TOUTES ses arêtes. Les points de l'anneau sont à peu près à distance
					// `offset` de ce point — on y pose donc le point central du raccord.
					NkVec3f ic = W.verts[v].pos;
					{
						NkEmId hh = W.verts[v].hedge;
						uint32 gg = 0;
						do {
							const uint32 dv = dstOf(hh);
							ic = ic + EM_Norm(W.verts[dv].pos - W.verts[v].pos) * tAlong(v, dv);
							const NkEmId tw2 = W.hedges[hh].twin;
							if (tw2 == NK_EM_INVALID)
								break;
							hh = W.hedges[tw2].next;
						} while (hh != W.verts[v].hedge && hh != NK_EM_INVALID && ++gg < 4096u);
					}
					NkVec3f cen{0.f, 0.f, 0.f};
					float32 rad = 0.f;
					for (uint32 k = 0; k < rn; ++k) {
						cen = cen + np[rr[k]].pos;
						rad += (np[rr[k]].pos - ic).Len();
					}
					cen = cen * (1.f / (float32)rn);
					rad /= (float32)rn;
					NkVertex3D nvc = wv[v];
					const NkVec3f dir = EM_Norm(cen - ic);
					nvc.pos = (dir.LenSq() > 0.f) ? (ic + dir * rad) : cen;
					const uint32 cid = pushPt(nvc, (uint8)1);
					for (uint32 k = 0; k < rn; ++k) {
						nfv.PushBack(cid);
						nfv.PushBack(rr[k]);
						nfv.PushBack(rr[(k + 1u) % rn]);
						nfs.PushBack((uint32)nfv.Size());
					}
				}
			}

			if (nfs.Size() < 2u || np.Empty())
				return false;
			BuildFromPolygons(np.Data(), (uint32)np.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data());
			ApplyVertSel(nsel);
			return true;
		}

		// =====================================================================
		// INSET FACES (I) — face plus petite à l'intérieur + bande de raccord
		// ---------------------------------------------------------------------
		// INDIVIDUAL : chaque face sélectionnée reçoit son propre contour intérieur
		//   (rétréci par bissectrice de coin dans le plan de la face) ; la bande relie
		//   les 4 côtés. Deux faces voisines gardent leur contour extérieur COMMUN
		//   (maillage soudé) mais obtiennent des intérieurs séparés — exactement Blender.
		// REGION : la sélection est un bloc. Seules les arêtes de BORD de la région
		//   (celles qui n'ont qu'UNE face sélectionnée) engendrent la bande ; les arêtes
		//   intérieures restent partagées. Le déplacement d'un sommet de bord est la
		//   SOMME des directions « vers l'intérieur » de ses arêtes de bord (sur un coin
		//   droit, cela recule bien de `thickness` sur chaque côté).
		// =====================================================================
		bool NkEditMesh::InsetSelectedFaces(const NkInsetParams &p) {
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			NkVector<uint8> vsel;
			NkVector<uint32> wmap;
			EM_ToWeldedPolygons(*this, pv, fs, fv, vsel, wmap);
			const uint32 fc = (fs.Size() > 0) ? (uint32)fs.Size() - 1 : 0;
			if (fc == 0)
				return false;
			NkVector<uint8> faceSel;
			faceSel.Resize(fc);
			uint32 selCount = 0;
			for (uint32 f = 0; f < fc; ++f) {
				const uint32 s = fs[f], e = fs[f + 1];
				bool sel = (e - s) >= 3u;
				for (uint32 k = s; k < e && sel; ++k)
					sel = (fv[k] < (uint32)vsel.Size()) && (vsel[fv[k]] != 0);
				faceSel[f] = sel ? (uint8)1 : (uint8)0;
				selCount += sel ? 1u : 0u;
			}
			if (selCount == 0)
				return false;
			float32 thick = p.thickness;
			if (thick <= 0.f)
				thick = EM_BBoxDiag(pv) * 0.08f;
			if (thick <= 1e-7f && p.depth == 0.f)
				return false;

			// Normale d'une face (convention moteur : cf. NkEmFaceCross).
			auto faceNormal = [&](uint32 f) -> NkVec3f {
				const uint32 s = fs[f], e = fs[f + 1];
				NkVec3f n{0.f, 0.f, 0.f};
				for (uint32 k = s + 1; k + 1 < e; ++k)
					n = n + NkEmFaceCross(pv[fv[s]].pos, pv[fv[k]].pos, pv[fv[k + 1]].pos);
				return EM_Norm(n);
			};

			NkVector<uint32> nfs, nfv;
			nfs.PushBack(0);
			NkVector<uint8> nsel;
			nsel.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)nsel.Size(); ++i)
				nsel[i] = 0;
			for (uint32 f = 0; f < fc; ++f) { // faces NON sélectionnées : recopiées telles quelles
				if (faceSel[f])
					continue;
				for (uint32 k = fs[f]; k < fs[f + 1]; ++k)
					nfv.PushBack(fv[k]);
				nfs.PushBack((uint32)nfv.Size());
			}

			if (p.individual) {
				NkVector<uint32> inner;
				for (uint32 f = 0; f < fc; ++f) {
					if (!faceSel[f])
						continue;
					const uint32 s = fs[f], e = fs[f + 1], n = e - s;
					const NkVec3f fn = faceNormal(f);
					inner.Clear();
					for (uint32 k = 0; k < n; ++k) {
						const uint32 v = fv[s + k], pr = fv[s + (k + n - 1u) % n], nx = fv[s + (k + 1u) % n];
						const NkVec3f d1 = pv[pr].pos - pv[v].pos, d2 = pv[nx].pos - pv[v].pos;
						const NkVec3f u1 = EM_Norm(d1), u2 = EM_Norm(d2);
						float32 t1 = thick, t2 = thick;
						const float32 l1 = d1.Len() * 0.45f, l2 = d2.Len() * 0.45f;
						if (t1 > l1)
							t1 = l1;
						if (t2 > l2)
							t2 = l2;
						float32 sn = u1.Cross(u2).Len();
						if (sn < 0.2f)
							sn = 0.2f;
						NkVertex3D nv = pv[v];
						nv.pos = pv[v].pos + (u1 * t1 + u2 * t2) * (1.f / sn) + fn * p.depth;
						inner.PushBack((uint32)pv.Size());
						pv.PushBack(nv);
						nsel.PushBack(1);
					}
					for (uint32 k = 0; k < n; ++k) // face INTÉRIEURE (même winding)
						nfv.PushBack(inner[k]);
					nfs.PushBack((uint32)nfv.Size());
					for (uint32 k = 0; k < n; ++k) { // BANDE de raccord
						nfv.PushBack(fv[s + k]);
						nfv.PushBack(fv[s + (k + 1u) % n]);
						nfv.PushBack(inner[(k + 1u) % n]);
						nfv.PushBack(inner[k]);
						nfs.PushBack((uint32)nfv.Size());
					}
				}
				if (nfs.Size() < 2u)
					return false;
				BuildFromPolygons(pv.Data(), (uint32)pv.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data());
				ApplyVertSel(nsel);
				return true;
			}

			// ── MODE RÉGION ──────────────────────────────────────────────────
			const uint32 baseVC = (uint32)pv.Size();
			NkHashMap<uint64, uint8> dirEdge; // arêtes ORIENTÉES des faces sélectionnées
			for (uint32 f = 0; f < fc; ++f) {
				if (!faceSel[f])
					continue;
				const uint32 s = fs[f], e = fs[f + 1], n = e - s;
				for (uint32 k = 0; k < n; ++k)
					dirEdge.InsertOrAssign(((uint64)fv[s + k] << 32) | (uint64)fv[s + (k + 1u) % n], (uint8)1);
			}
			NkVector<NkVec3f> disp, nrm;
			disp.Resize(baseVC);
			nrm.Resize(baseVC);
			for (uint32 i = 0; i < baseVC; ++i) {
				disp[i] = {0.f, 0.f, 0.f};
				nrm[i] = {0.f, 0.f, 0.f};
			}
			NkVector<uint8> inRegion;
			inRegion.Resize(baseVC);
			for (uint32 i = 0; i < baseVC; ++i)
				inRegion[i] = 0;
			bool anyBoundary = false;
			for (uint32 f = 0; f < fc; ++f) {
				if (!faceSel[f])
					continue;
				const uint32 s = fs[f], e = fs[f + 1], n = e - s;
				const NkVec3f fn = faceNormal(f);
				NkVec3f cen{0.f, 0.f, 0.f};
				for (uint32 k = 0; k < n; ++k) {
					cen = cen + pv[fv[s + k]].pos;
					inRegion[fv[s + k]] = 1;
					nrm[fv[s + k]] = nrm[fv[s + k]] + fn;
				}
				cen = cen * (1.f / (float32)n);
				for (uint32 k = 0; k < n; ++k) {
					const uint32 a = fv[s + k], b = fv[s + (k + 1u) % n];
					if (dirEdge.Find(((uint64)b << 32) | (uint64)a))
						continue; // arête INTÉRIEURE à la région
					anyBoundary = true;
					// Direction « vers l'intérieur de la face », perpendiculaire à l'arête.
					const NkVec3f d = EM_Norm(pv[b].pos - pv[a].pos);
					const NkVec3f m = (pv[a].pos + pv[b].pos) * 0.5f;
					NkVec3f w = cen - m;
					w = EM_Norm(w - d * w.Dot(d));
					disp[a] = disp[a] + w;
					disp[b] = disp[b] + w;
				}
			}
			if (!anyBoundary && p.depth == 0.f)
				return false; // région fermée sans profondeur -> rien à faire
			NkVector<int32> innerOf;
			innerOf.Resize(baseVC);
			for (uint32 i = 0; i < baseVC; ++i)
				innerOf[i] = -1;
			for (uint32 i = 0; i < baseVC; ++i) {
				if (!inRegion[i])
					continue;
				NkVertex3D nv = pv[i];
				nv.pos = pv[i].pos + disp[i] * thick + EM_Norm(nrm[i]) * p.depth;
				innerOf[i] = (int32)pv.Size();
				pv.PushBack(nv);
				nsel.PushBack(1);
			}
			for (uint32 f = 0; f < fc; ++f) { // faces sélectionnées -> version intérieure
				if (!faceSel[f])
					continue;
				for (uint32 k = fs[f]; k < fs[f + 1]; ++k)
					nfv.PushBack((uint32)innerOf[fv[k]]);
				nfs.PushBack((uint32)nfv.Size());
			}
			for (uint32 f = 0; f < fc; ++f) { // BANDE sur les seules arêtes de BORD
				if (!faceSel[f])
					continue;
				const uint32 s = fs[f], e = fs[f + 1], n = e - s;
				for (uint32 k = 0; k < n; ++k) {
					const uint32 a = fv[s + k], b = fv[s + (k + 1u) % n];
					if (dirEdge.Find(((uint64)b << 32) | (uint64)a))
						continue;
					nfv.PushBack(a);
					nfv.PushBack(b);
					nfv.PushBack((uint32)innerOf[b]);
					nfv.PushBack((uint32)innerOf[a]);
					nfs.PushBack((uint32)nfv.Size());
				}
			}
			if (nfs.Size() < 2u)
				return false;
			BuildFromPolygons(pv.Data(), (uint32)pv.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data());
			ApplyVertSel(nsel);
			return true;
		}

		// =====================================================================
		// EDGE SPLIT (V) — dé-soudure locale le long des arêtes sélectionnées
		// ---------------------------------------------------------------------
		// Autour de chaque sommet touché, on parcourt le VENTILATEUR de faces
		// (rot(h) = twin(h).next). Chaque traversée d'une arête SÉLECTIONNÉE ouvre un
		// nouveau GROUPE ; chaque groupe reçoit sa propre copie du sommet, décalée d'un
		// demi-`gap` le long de la normale moyenne de ses faces. Après reconstruction,
		// LinkTwins n'apparie plus les demi-arêtes de part et d'autre (positions
		// différentes) : la déchirure est réelle et les twins restent cohérents.
		// =====================================================================
		bool NkEditMesh::SplitSelectedEdges(const NkEdgeSplitParams &p) {
			NkVector<NkVertex3D> wv;
			NkVector<uint32> wfs, wfv;
			NkVector<uint8> wsel;
			NkVector<uint32> wmap;
			EM_ToWeldedPolygons(*this, wv, wfs, wfv, wsel, wmap);
			const uint32 wfc = (wfs.Size() > 0) ? (uint32)wfs.Size() - 1 : 0;
			if (wfc == 0)
				return false;
			NkEditMesh W;
			W.BuildFromPolygons(wv.Data(), (uint32)wv.Size(), wfs.Data(), wfc, wfv.Data());
			const uint32 NV = W.VertCount(), HC = (uint32)W.hedges.Size();
			if (NV == 0 || HC == 0)
				return false;
			for (uint32 i = 0; i < NV && i < (uint32)wsel.Size(); ++i)
				W.verts[i].sel = wsel[i];

			NkVector<NkEmId> prevOf;
			prevOf.Resize(HC);
			for (uint32 i = 0; i < HC; ++i)
				prevOf[i] = NK_EM_INVALID;
			for (uint32 f = 0; f < (uint32)W.faces.Size(); ++f) {
				if (!W.faces[f].alive || W.faces[f].hedge == NK_EM_INVALID)
					continue;
				const NkEmId s = W.faces[f].hedge;
				NkEmId h = s;
				uint32 g = 0;
				do {
					const NkEmId nx = W.hedges[h].next;
					if (nx == NK_EM_INVALID)
						break;
					prevOf[nx] = h;
					h = nx;
				} while (h != s && ++g < 100000u);
			}
			auto dstOf = [&](NkEmId h) -> uint32 {
				const NkEmId nx = W.hedges[h].next;
				return (nx == NK_EM_INVALID) ? W.hedges[h].origin : W.hedges[nx].origin;
			};
			auto ekey = [](uint32 a, uint32 b) -> uint64 {
				const uint32 lo = (a < b) ? a : b, hi = (a < b) ? b : a;
				return ((uint64)lo << 32) | (uint64)hi;
			};
			NkHashMap<uint64, uint8> selE;
			NkVector<uint8> touched;
			touched.Resize(NV);
			for (uint32 i = 0; i < NV; ++i)
				touched[i] = 0;
			for (uint32 h = 0; h < HC; ++h) {
				if (!W.hedges[h].alive || W.hedges[h].twin == NK_EM_INVALID)
					continue; // arête de BORD : déjà ouverte
				const uint32 a = W.hedges[h].origin, b = dstOf((NkEmId)h);
				if (a == b || a >= NV || b >= NV || !W.verts[a].sel || !W.verts[b].sel)
					continue;
				selE.InsertOrAssign(ekey(a, b), (uint8)1);
				touched[a] = 1;
				touched[b] = 1;
			}
			if (selE.Empty())
				return false;
			float32 gap = p.gap;
			if (gap <= 0.f)
				gap = EM_BBoxDiag(wv) * 0.01f;

			NkVector<NkVertex3D> np = wv;
			NkVector<uint8> nsel;
			nsel.Resize((uint32)np.Size());
			for (uint32 i = 0; i < (uint32)nsel.Size(); ++i)
				nsel[i] = 0;
			NkVector<int32> cornerOf; // coin (demi-arête) -> sommet de sortie
			cornerOf.Resize(HC);
			for (uint32 i = 0; i < HC; ++i)
				cornerOf[i] = -1;
			bool splitAny = false;
			NkVector<NkEmId> fanH;
			NkVector<int32> fanG;
			NkVector<NkVec3f> gN;
			NkVector<int32> gIdx;
			for (uint32 v = 0; v < NV; ++v) {
				if (!touched[v] || W.verts[v].hedge == NK_EM_INVALID)
					continue;
				const NkEmId h0 = W.verts[v].hedge;
				// 1) point de départ : le bord du ventilateur s'il est OUVERT, sinon la
				//    demi-arête qui suit immédiatement une arête sélectionnée (sans quoi le
				//    1er et le dernier groupe du tour seraient comptés deux fois).
				NkEmId start = h0;
				bool closed = true;
				{
					NkEmId x = h0;
					uint32 g = 0;
					while (++g < 4096u) {
						const NkEmId pr = prevOf[x];
						const NkEmId tw = (pr == NK_EM_INVALID) ? NK_EM_INVALID : W.hedges[pr].twin;
						if (tw == NK_EM_INVALID) {
							start = x;
							closed = false;
							break;
						}
						x = tw;
						if (x == h0)
							break;
					}
				}
				if (closed) {
					NkEmId x = h0;
					uint32 g = 0;
					do {
						const NkEmId tw = W.hedges[x].twin;
						if (tw == NK_EM_INVALID)
							break;
						if (selE.Find(ekey(v, dstOf(x)))) {
							start = W.hedges[tw].next;
							break;
						}
						x = W.hedges[tw].next;
					} while (x != h0 && x != NK_EM_INVALID && ++g < 4096u);
				}
				// 2) parcours du ventilateur : groupe incrémenté à chaque arête sélectionnée.
				fanH.Clear();
				fanG.Clear();
				gN.Clear();
				gIdx.Clear();
				gN.PushBack({0.f, 0.f, 0.f});
				gIdx.PushBack((int32)v); // groupe 0 = le sommet d'origine
				int32 grp = 0;
				NkEmId x = start;
				uint32 g = 0;
				while (x != NK_EM_INVALID && ++g < 4096u) {
					fanH.PushBack(x);
					fanG.PushBack(grp);
					if (W.hedges[x].face != NK_EM_INVALID)
						gN[(uint32)grp] = gN[(uint32)grp] + W.faces[W.hedges[x].face].normal;
					const NkEmId tw = W.hedges[x].twin;
					if (tw == NK_EM_INVALID)
						break; // fin d'un ventilateur ouvert
					const NkEmId nx = W.hedges[tw].next;
					if (nx == NK_EM_INVALID || nx == start)
						break; // tour complet
					if (selE.Find(ekey(v, dstOf(x)))) {
						++grp;
						gN.PushBack({0.f, 0.f, 0.f});
						gIdx.PushBack(-1);
					}
					x = nx;
				}
				// 3) un seul groupe -> le sommet reste partagé (arête isolée : cf. limites).
				if (gIdx.Size() > 1u) {
					splitAny = true;
					for (uint32 q = 1; q < (uint32)gIdx.Size(); ++q) {
						gIdx[q] = (int32)np.Size();
						np.PushBack(wv[v]);
						nsel.PushBack(1); // les morceaux DÉTACHÉS deviennent la sélection
					}
					for (uint32 q = 0; q < (uint32)gIdx.Size(); ++q) {
						const uint32 id = (uint32)gIdx[q];
						np[id].pos = np[id].pos + EM_Norm(gN[q]) * (gap * 0.5f);
					}
				}
				for (uint32 q = 0; q < (uint32)fanH.Size(); ++q)
					cornerOf[fanH[q]] = gIdx[(uint32)fanG[q]];
			}
			if (!splitAny)
				return false;

			NkVector<uint32> nfs, nfv;
			nfs.PushBack(0);
			for (uint32 f = 0; f < (uint32)W.faces.Size(); ++f) {
				if (!W.faces[f].alive || W.faces[f].hedge == NK_EM_INVALID)
					continue;
				const NkEmId s = W.faces[f].hedge;
				NkEmId h = s;
				uint32 g = 0;
				do {
					nfv.PushBack((cornerOf[h] >= 0) ? (uint32)cornerOf[h] : W.hedges[h].origin);
					h = W.hedges[h].next;
				} while (h != s && h != NK_EM_INVALID && ++g < 100000u);
				nfs.PushBack((uint32)nfv.Size());
			}
			BuildFromPolygons(np.Data(), (uint32)np.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data());
			ApplyVertSel(nsel);
			return true;
		}

		// =====================================================================
		// SPIN / RÉVOLUTION (J) — le profil sélectionné tourne autour d'un axe
		// ---------------------------------------------------------------------
		// Le centre et l'axe arrivent dans l'espace du CURSEUR 3D (monde éditeur) : on les
		// ramène en local par l'inverse de `localToSpin` (l'axe est une DIRECTION : on le
		// transforme comme une différence de deux points, ce qui reste juste sous une
		// transform à rotation/échelle quelconque).
		// =====================================================================
		bool NkEditMesh::SpinSelected(const NkSpinParams &p, const NkMat4f &localToSpin) {
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			NkVector<uint8> vsel;
			NkVector<uint32> wmap;
			EM_ToWeldedPolygons(*this, pv, fs, fv, vsel, wmap);
			const uint32 fc = (fs.Size() > 0) ? (uint32)fs.Size() - 1 : 0;
			const uint32 baseVC = (uint32)pv.Size();
			if (baseVC == 0)
				return false;
			const NkMat4f inv = localToSpin.Inverse();
			const NkVec3f ctr = inv * p.center;
			NkVec3f ax = EM_Norm((inv * (p.center + p.axis)) - ctr);
			if (ax.LenSq() < 1e-12f)
				ax = {0.f, 1.f, 0.f};
			const int32 steps = (p.steps < 1) ? 1 : ((p.steps > 256) ? 256 : p.steps);

			// Profil : sommets sélectionnés + arêtes dont les DEUX extrémités le sont.
			NkVector<int32> slot;
			slot.Resize(baseVC);
			NkVector<uint32> prof;
			for (uint32 i = 0; i < baseVC; ++i) {
				slot[i] = -1;
				if (i < (uint32)vsel.Size() && vsel[i]) {
					slot[i] = (int32)prof.Size();
					prof.PushBack(i);
				}
			}
			if (prof.Empty())
				return false;
			NkVector<uint32> eA, eB;
			NkVector<uint8> faceSel;
			faceSel.Resize(fc);
			{
				NkHashMap<uint64, uint8> seen;
				for (uint32 f = 0; f < fc; ++f) {
					const uint32 s = fs[f], e = fs[f + 1], n = e - s;
					bool allSel = (n >= 3u);
					for (uint32 k = 0; k < n; ++k) {
						const uint32 a = fv[s + k], b = fv[s + (k + 1u) % n];
						if (slot[a] < 0 || slot[b] < 0) {
							allSel = false;
							continue;
						}
						const uint32 lo = (a < b) ? a : b, hi = (a < b) ? b : a;
						const uint64 key = ((uint64)lo << 32) | (uint64)hi;
						if (seen.Find(key))
							continue;
						seen.InsertOrAssign(key, (uint8)1);
						eA.PushBack(a);
						eB.PushBack(b);
					}
					faceSel[f] = allSel ? (uint8)1 : (uint8)0;
				}
			}
			if (eA.Empty() && !p.duplicate)
				return false; // pas d'arête à balayer

			// Anneaux successifs du balayage : ring[k * pn + j].
			const uint32 pn = (uint32)prof.Size();
			NkVector<uint32> ring;
			ring.Resize((uint32)(steps + 1) * pn);
			for (uint32 j = 0; j < pn; ++j)
				ring[j] = prof[j];
			for (int32 k = 1; k <= steps; ++k) {
				const float32 t = p.angle * (float32)k / (float32)steps;
				const float32 cs = cosf(t), sn = sinf(t);
				for (uint32 j = 0; j < pn; ++j) {
					const NkVec3f r = pv[prof[j]].pos - ctr;
					NkVertex3D nv = pv[prof[j]];
					nv.pos = ctr + r * cs + ax.Cross(r) * sn + ax * (ax.Dot(r) * (1.f - cs)); // Rodrigues
					ring[(uint32)k * pn + j] = (uint32)pv.Size();
					pv.PushBack(nv);
				}
			}

			NkVector<uint32> nfs, nfv;
			nfs.PushBack(0);
			for (uint32 f = 0; f < fc; ++f) { // la géométrie d'origine est CONSERVÉE
				for (uint32 k = fs[f]; k < fs[f + 1]; ++k)
					nfv.PushBack(fv[k]);
				nfs.PushBack((uint32)nfv.Size());
			}
			if (p.duplicate) { // copies ISOLÉES des faces sélectionnées à chaque pas
				for (int32 k = 1; k <= steps; ++k)
					for (uint32 f = 0; f < fc; ++f) {
						if (!faceSel[f])
							continue;
						for (uint32 q = fs[f]; q < fs[f + 1]; ++q)
							nfv.PushBack(ring[(uint32)k * pn + (uint32)slot[fv[q]]]);
						nfs.PushBack((uint32)nfv.Size());
					}
			} else { // bandes reliant les anneaux consécutifs
				for (int32 k = 0; k < steps; ++k) {
					for (uint32 e = 0; e < (uint32)eA.Size(); ++e) {
						const uint32 a0 = ring[(uint32)k * pn + (uint32)slot[eA[e]]];
						const uint32 b0 = ring[(uint32)k * pn + (uint32)slot[eB[e]]];
						const uint32 a1 = ring[(uint32)(k + 1) * pn + (uint32)slot[eA[e]]];
						const uint32 b1 = ring[(uint32)(k + 1) * pn + (uint32)slot[eB[e]]];
						// ORIENTATION : la normale du quad doit FUIR l'axe (surface de
						// révolution vue de l'extérieur) ; sinon on inverse la boucle.
						const NkVec3f n4 = NkEmFaceCross(pv[a0].pos, pv[b0].pos, pv[b1].pos);
						const NkVec3f cq = (pv[a0].pos + pv[b0].pos + pv[a1].pos + pv[b1].pos) * 0.25f;
						NkVec3f rad = cq - ctr;
						rad = rad - ax * rad.Dot(ax);
						if ((rad.LenSq() > 1e-12f) && (n4.Dot(rad) < 0.f)) {
							nfv.PushBack(a1);
							nfv.PushBack(b1);
							nfv.PushBack(b0);
							nfv.PushBack(a0);
						} else {
							nfv.PushBack(a0);
							nfv.PushBack(b0);
							nfv.PushBack(b1);
							nfv.PushBack(a1);
						}
						nfs.PushBack((uint32)nfv.Size());
					}
				}
			}
			NkVector<uint8> nsel;
			nsel.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)nsel.Size(); ++i)
				nsel[i] = 0;
			for (uint32 j = 0; j < pn; ++j) // sélection = DERNIER anneau (façon Blender)
				nsel[ring[(uint32)steps * pn + j]] = 1;
			BuildFromPolygons(pv.Data(), (uint32)pv.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data());
			ApplyVertSel(nsel);
			return true;
		}

		// =====================================================================
		// DISSOLVE (Ctrl+X) — retire des éléments SANS trouer : les faces voisines
		// fusionnent en n-gon. C'est l'opposé de « supprimer » (X).
		// ---------------------------------------------------------------------
		// UN SEUL algorithme pour les trois modes : on marque les arêtes à RETIRER, puis
		// on reparcourt les contours. Pour une demi-arête de contour h, la suivante est
		// obtenue en avançant dans la face et, TANT QUE l'arête rencontrée est retirée, en
		// passant chez le voisin par le jumeau :  x = next(h) ; while(retiree(x)) x = next(twin(x)).
		// Ce parcours saute naturellement les sommets devenus intérieurs à la région —
		// c'est ce qui rend le dissolve de SOMMET et de FACE identiques à celui d'ARÊTE.
		// =====================================================================
		bool NkEditMesh::DissolveSelected(const NkDissolveParams &p) {
			NkVector<NkVertex3D> wv;
			NkVector<uint32> wfs, wfv;
			NkVector<uint8> wsel;
			NkVector<uint32> wmap;
			EM_ToWeldedPolygons(*this, wv, wfs, wfv, wsel, wmap);
			const uint32 wfc = (wfs.Size() > 0) ? (uint32)wfs.Size() - 1 : 0;
			if (wfc == 0)
				return false;
			NkEditMesh W;
			W.BuildFromPolygons(wv.Data(), (uint32)wv.Size(), wfs.Data(), wfc, wfv.Data());
			const uint32 NV = W.VertCount(), HC = (uint32)W.hedges.Size();
			if (NV == 0 || HC == 0)
				return false;
			for (uint32 i = 0; i < NV && i < (uint32)wsel.Size(); ++i)
				W.verts[i].sel = wsel[i];
			auto dstOf = [&](NkEmId h) -> uint32 {
				const NkEmId nx = W.hedges[h].next;
				return (nx == NK_EM_INVALID) ? W.hedges[h].origin : W.hedges[nx].origin;
			};

			// ── 1) Arêtes à RETIRER, selon le mode ───────────────────────────
			NkVector<uint8> gone; // par demi-arête (symétrique avec son jumeau)
			gone.Resize(HC);
			for (uint32 i = 0; i < HC; ++i)
				gone[i] = 0;
			const int32 mode = (p.mode < 0 || p.mode > 2) ? 1 : p.mode;
			NkVector<uint8> faceSel;
			if (mode == 2) {
				faceSel.Resize((uint32)W.faces.Size());
				for (uint32 f = 0; f < (uint32)W.faces.Size(); ++f)
					faceSel[f] = (W.faces[f].alive && W.FaceIsSelected((NkEmId)f)) ? (uint8)1 : (uint8)0;
			}
			uint32 removedCount = 0;
			for (uint32 h = 0; h < HC; ++h) {
				const NkEmId tw = W.hedges[h].twin;
				if (!W.hedges[h].alive || tw == NK_EM_INVALID)
					continue; // arête de BORD : rien à fusionner
				if (W.hedges[h].face == NK_EM_INVALID || W.hedges[tw].face == NK_EM_INVALID)
					continue;
				if (W.hedges[h].face == W.hedges[tw].face)
					continue; // même face des deux côtés : dégénéré
				const uint32 a = W.hedges[h].origin, b = dstOf((NkEmId)h);
				if (a >= NV || b >= NV)
					continue;
				bool kill = false;
				if (mode == 0)
					kill = (W.verts[a].sel != 0) || (W.verts[b].sel != 0); // Verts
				else if (mode == 1)
					kill = (W.verts[a].sel != 0) && (W.verts[b].sel != 0); // Edges
				else
					kill = (faceSel[W.hedges[h].face] != 0) && (faceSel[W.hedges[tw].face] != 0); // Faces
				if (!kill)
					continue;
				if (!gone[h])
					++removedCount;
				gone[h] = 1;
				gone[tw] = 1;
			}
			if (removedCount == 0)
				return false;

			// ── 2) Contours des régions fusionnées ───────────────────────────
			NkVector<uint8> seen;
			seen.Resize(HC);
			for (uint32 i = 0; i < HC; ++i)
				seen[i] = 0;
			NkVector<uint32> nfs, nfv;
			nfs.PushBack(0);
			NkVector<uint8> touchedV; // sommets du contour d'une région fusionnée -> sélection
			touchedV.Resize(NV);
			for (uint32 i = 0; i < NV; ++i)
				touchedV[i] = 0;
			for (uint32 h0 = 0; h0 < HC; ++h0) {
				if (seen[h0] || gone[h0] || !W.hedges[h0].alive || W.hedges[h0].face == NK_EM_INVALID)
					continue;
				const uint32 st = (uint32)nfv.Size();
				bool merged = false;
				NkEmId h = (NkEmId)h0;
				uint32 g = 0;
				bool bad = false;
				do {
					seen[h] = 1;
					nfv.PushBack(W.hedges[h].origin);
					NkEmId x = W.hedges[h].next;
					uint32 g2 = 0;
					while (x != NK_EM_INVALID && gone[x] && ++g2 < 100000u) {
						merged = true;
						const NkEmId tx = W.hedges[x].twin;
						if (tx == NK_EM_INVALID) {
							bad = true;
							break;
						}
						x = W.hedges[tx].next;
					}
					if (bad || x == NK_EM_INVALID)
						break;
					h = x;
				} while (h != (NkEmId)h0 && ++g < 100000u);
				const uint32 n = (uint32)nfv.Size() - st;
				if (bad || n < 3u) {
					nfv.Resize(st); // contour dégénéré -> abandonné
					continue;
				}
				if (merged)
					for (uint32 k = st; k < (uint32)nfv.Size(); ++k)
						touchedV[nfv[k]] = 1;
				nfs.PushBack((uint32)nfv.Size());
			}
			if (nfs.Size() < 2u)
				return false;

			// ── 3) COMPACTAGE : on ne garde que les sommets réellement utilisés ──
			NkVector<int32> remap;
			remap.Resize(NV);
			for (uint32 i = 0; i < NV; ++i)
				remap[i] = -1;
			NkVector<NkVertex3D> np;
			NkVector<uint8> nsel;
			for (uint32 k = 0; k < (uint32)nfv.Size(); ++k) {
				const uint32 v = nfv[k];
				if (remap[v] < 0) {
					remap[v] = (int32)np.Size();
					np.PushBack(wv[v]);
					nsel.PushBack(touchedV[v]);
				}
				nfv[k] = (uint32)remap[v];
			}
			BuildFromPolygons(np.Data(), (uint32)np.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data());
			ApplyVertSel(nsel);
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
		// =====================================================================
		// TO SPHERE / SHRINK-FATTEN — deformations RADIALES (façon Blender)
		// =====================================================================
		// Toutes deux operent sur l'IDENTITE SOUDEE (BuildVertexMerge) : les copies
		// coincidentes d'un meme coin recoivent EXACTEMENT le meme deplacement, sinon
		// la soudure (donc les jumeaux de demi-aretes) serait rompue au premier appel.
		bool NkEditMesh::ToSphereSelected(const NkToSphereParams &p) {
			const uint32 nv = (uint32)verts.Size();
			if (nv == 0 || fabsf(p.factor) < 1e-6f)
				return false;
			NkVector<uint32> canon;
			BuildVertexMerge(canon);
			NkVector<NkVec3f> target;
			NkVector<uint32> hits;
			target.Resize(nv);
			hits.Resize(nv);
			for (uint32 i = 0; i < nv; ++i) {
				target[i] = {0.f, 0.f, 0.f};
				hits[i] = 0;
			}
			if (!p.individual) {
				// RAYON MOYEN : moyenne des distances au centre, comptee UNE fois par
				// sommet soude (sinon les coins dupliques pesent 3x et le rayon derive).
				float64 sum = 0.0;
				uint32 cnt = 0;
				NkVector<uint8> seen;
				seen.Resize(nv);
				for (uint32 i = 0; i < nv; ++i)
					seen[i] = 0;
				for (uint32 i = 0; i < nv; ++i) {
					if (!verts[i].sel)
						continue;
					const uint32 cv = canon[i];
					if (seen[cv])
						continue;
					seen[cv] = 1;
					sum += (float64)(verts[i].pos - p.center).Len();
					cnt++;
				}
				if (cnt == 0)
					return false;
				const float32 R = (float32)(sum / (float64)cnt);
				for (uint32 i = 0; i < nv; ++i) {
					if (!verts[i].sel)
						continue;
					const NkVec3f d = verts[i].pos - p.center;
					const float32 l = d.Len();
					if (l < 1e-8f)
						continue;
					target[i] = p.center + d * (R / l);
					hits[i] = 1;
				}
			} else {
				// PAR ILOT : chaque face entierement selectionnee est spherisee autour de
				// SON barycentre ; un sommet partage prend la MOYENNE de ses cibles.
				NkVector<NkEmId> loop;
				NkVector<NkVec3f> acc;
				NkVector<uint32> acn;
				acc.Resize(nv);
				acn.Resize(nv);
				for (uint32 i = 0; i < nv; ++i) {
					acc[i] = {0.f, 0.f, 0.f};
					acn[i] = 0;
				}
				for (uint32 f = 0; f < (uint32)faces.Size(); ++f) {
					if (!faces[f].alive || !FaceIsSelected(f))
						continue;
					loop.Clear();
					GetFaceVerts(f, loop);
					const uint32 fn = (uint32)loop.Size();
					if (fn < 3)
						continue;
					NkVec3f fc = {0.f, 0.f, 0.f};
					for (uint32 k = 0; k < fn; ++k)
						fc = fc + verts[loop[k]].pos;
					fc = fc * (1.f / (float32)fn);
					float64 sum = 0.0;
					for (uint32 k = 0; k < fn; ++k)
						sum += (float64)(verts[loop[k]].pos - fc).Len();
					const float32 R = (float32)(sum / (float64)fn);
					for (uint32 k = 0; k < fn; ++k) {
						const uint32 vi = canon[loop[k]];
						const NkVec3f d = verts[loop[k]].pos - fc;
						const float32 l = d.Len();
						if (l < 1e-8f)
							continue;
						acc[vi] = acc[vi] + (fc + d * (R / l));
						acn[vi]++;
					}
				}
				for (uint32 i = 0; i < nv; ++i) {
					const uint32 cv = canon[i];
					if (!verts[i].sel || acn[cv] == 0)
						continue;
					target[i] = acc[cv] * (1.f / (float32)acn[cv]);
					hits[i] = 1;
				}
			}
			bool changed = false;
			for (uint32 i = 0; i < nv; ++i) {
				if (!hits[i])
					continue;
				const NkVec3f np = verts[i].pos + (target[i] - verts[i].pos) * p.factor;
				if ((np - verts[i].pos).Len() > 1e-7f)
					changed = true;
				verts[i].pos = np;
			}
			if (changed)
				RecomputeNormals();
			return changed;
		}

		bool NkEditMesh::ShrinkFattenSelected(const NkShrinkFattenParams &p) {
			const uint32 nv = (uint32)verts.Size();
			const uint32 nf = (uint32)faces.Size();
			if (nv == 0 || fabsf(p.offset) < 1e-7f)
				return false;
			NkVector<uint32> canon;
			BuildVertexMerge(canon);
			// Normale par sommet SOUDE = somme des normales de face NON normalisees
			// (donc ponderees par l'aire), accumulee sur le representant du groupe.
			NkVector<NkVec3f> acc;
			acc.Resize(nv);
			for (uint32 i = 0; i < nv; ++i)
				acc[i] = {0.f, 0.f, 0.f};
			NkVector<NkEmId> loop;
			for (uint32 f = 0; f < nf; ++f) {
				if (!faces[f].alive)
					continue;
				loop.Clear();
				GetFaceVerts(f, loop);
				if (loop.Size() < 3)
					continue;
				const NkVec3f n = NkEmFaceCross(verts[loop[0]].pos, verts[loop[1]].pos, verts[loop[2]].pos);
				for (uint32 k = 0; k < (uint32)loop.Size(); ++k) {
					const uint32 cv = canon[loop[k]];
					acc[cv] = acc[cv] + n;
				}
			}
			bool changed = false;
			for (uint32 i = 0; i < nv; ++i) {
				if (!verts[i].sel)
					continue;
				const NkVec3f n = acc[canon[i]];
				const float32 l = n.Len();
				if (l < 1e-10f)
					continue;
				verts[i].pos = verts[i].pos + n * (p.offset / l);
				changed = true;
			}
			if (changed)
				RecomputeNormals();
			return changed;
		}

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
				case NkMeshEditOp::ExtrudeVerts:
					return m.ExtrudeSelectedVertices(extrude);
				case NkMeshEditOp::ExtrudeEdges:
					return m.ExtrudeSelectedEdges(extrude);
				case NkMeshEditOp::Delete:
					return m.DeleteSelectedFaces();
				case NkMeshEditOp::Merge:
					return m.MergeSelectedVerts(merge);
				case NkMeshEditOp::MakeFace:
					// F facon Blender : la MEME touche cree une ARETE avec deux sommets
					// selectionnes, et une FACE a partir de trois. Cette bascule est le
					// comportement de Blender, pas une commodite : avec deux sommets il
					// n'y a pas de face a creer, il y a un segment.
					// C'est ce que l'ancienne structure ne savait pas faire (une arete
					// n'existait qu'a travers ses faces) — cf. Edge / AddWireEdge.
					if (m.MakeEdgeFromSelected())
						return true;
					return m.MakeFaceFromSelected();
				case NkMeshEditOp::Subdivide:
					return m.SubdivideSelectedFaces(subdiv);
				case NkMeshEditOp::LoopCut:
					return m.LoopCutFromSelectedEdge(loopcut);
				case NkMeshEditOp::Bisect:
					return m.BisectByPlane(planePoint, planeNormal, bisectXform);
				case NkMeshEditOp::Bevel:
					return m.BevelSelected(bevel);
				case NkMeshEditOp::Inset:
					return m.InsetSelectedFaces(inset);
				case NkMeshEditOp::EdgeSplit:
					return m.SplitSelectedEdges(esplit);
				case NkMeshEditOp::Spin:
					return m.SpinSelected(spin, spinXform);
				case NkMeshEditOp::Dissolve:
					return m.DissolveSelected(dissolve);
				case NkMeshEditOp::ToSphere:
					return m.ToSphereSelected(tosphere);
				case NkMeshEditOp::ShrinkFatten:
					return m.ShrinkFattenSelected(shrinkfatten);
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
			w.U32(9u); // v9 : + loopcut.slide (v8 : ToSphere/ShrinkFatten · v7 : dissolve · v6 : spin
					   //       v5 : split · v4 : inset · v3 : bevel · v2 : loopcut.cuts)
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
				w.I32(c.loopcut.cuts); // v2
				w.F32(c.bevel.offset); // v3
				w.I32(c.bevel.segments);
				w.U8((uint8)(c.bevel.vertexOnly ? 1 : 0));
				w.F32(c.inset.thickness); // v4
				w.F32(c.inset.depth);
				w.U8((uint8)(c.inset.individual ? 1 : 0));
				w.F32(c.esplit.gap); // v5
				w.F32(c.spin.center.x); // v6
				w.F32(c.spin.center.y);
				w.F32(c.spin.center.z);
				w.F32(c.spin.axis.x);
				w.F32(c.spin.axis.y);
				w.F32(c.spin.axis.z);
				w.F32(c.spin.angle);
				w.I32(c.spin.steps);
				w.U8((uint8)(c.spin.duplicate ? 1 : 0));
				for (int32 col = 0; col < 4; ++col)
					for (int32 row = 0; row < 4; ++row)
						w.F32(c.spinXform[col][row]);
				w.I32(c.dissolve.mode); // v7
				w.F32(c.tosphere.center.x); // v8
				w.F32(c.tosphere.center.y);
				w.F32(c.tosphere.center.z);
				w.F32(c.tosphere.factor);
				w.U8((uint8)(c.tosphere.individual ? 1 : 0));
				w.F32(c.shrinkfatten.offset);
				w.F32(c.loopcut.slide); // v9
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
			const uint32 ver = r.U32(); // 1 = sans loopcut.cuts, 2 = avec
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
				if (ver >= 2u)
					c.loopcut.cuts = r.I32();
				if (ver >= 3u) {
					c.bevel.offset = r.F32();
					c.bevel.segments = r.I32();
					c.bevel.vertexOnly = (r.U8() != 0);
				}
				if (ver >= 4u) {
					c.inset.thickness = r.F32();
					c.inset.depth = r.F32();
					c.inset.individual = (r.U8() != 0);
				}
				if (ver >= 5u)
					c.esplit.gap = r.F32();
				if (ver >= 6u) {
					float32 cx = r.F32(), cy = r.F32(), cz = r.F32();
					c.spin.center = {cx, cy, cz};
					float32 axx = r.F32(), axy = r.F32(), axz = r.F32();
					c.spin.axis = {axx, axy, axz};
					c.spin.angle = r.F32();
					c.spin.steps = r.I32();
					c.spin.duplicate = (r.U8() != 0);
					for (int32 col = 0; col < 4; ++col)
						for (int32 row = 0; row < 4; ++row)
							c.spinXform[col][row] = r.F32();
				}
				if (ver >= 7u)
					c.dissolve.mode = r.I32();
				if (ver >= 8u) {
					float32 sx = r.F32(), sy = r.F32(), sz = r.F32();
					c.tosphere.center = {sx, sy, sz};
					c.tosphere.factor = r.F32();
					c.tosphere.individual = (r.U8() != 0);
					c.shrinkfatten.offset = r.F32();
				}
				if (ver >= 9u)
					c.loopcut.slide = r.F32();
				if (r.ok)
					mCommands.PushBack(c);
			}
			return r.ok;
		}

	} // namespace renderer
} // namespace nkentseu
