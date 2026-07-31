// =============================================================================
// NKEditMeshHarness — harnais de NON-REGRESSION de NkEditMesh
//
// POURQUOI CE PROGRAMME EXISTE
//   La refonte de NkEditMesh vers un modele BMesh (arete de PREMIER PLAN, au
//   lieu d'une arete qui n'existe qu'a travers ses faces) touche tout ce qui a
//   ete construit sur l'edition : parcours, boucles, soudure, operations. Sans
//   filet, la seule facon de savoir si elle casse quelque chose serait de
//   re-tester chaque outil A LA MAIN, a la souris, sur chaque primitive —
//   c'est-a-dire de ne pas le savoir.
//
//   Ce harnais applique une BATTERIE FIXE d'operations a des primitives fixes
//   et imprime une SIGNATURE par etape. La methode convenue est :
//     1. capturer la signature AVANT la refonte (reference) ;
//     2. refondre ;
//     3. rejouer et exiger des chiffres IDENTIQUES.
//   Toute divergence est une regression a expliquer, pas a arbitrer.
//
// CE QUE MESURE LA SIGNATURE
//   Des invariants TOPOLOGIQUES et GEOMETRIQUES, pas des details de
//   representation interne — sinon la refonte les changerait par construction
//   et le harnais ne prouverait rien :
//     V/F      sommets et faces VIVANTS
//     E        aretes uniques apres SOUDURE des sommets coincidents
//     bord     aretes portees par UNE seule face (0 = maillage ferme)
//     nonmanif aretes portees par PLUS de deux faces (0 attendu)
//     aire     somme des aires des triangles (invariant geometrique)
//     centre   barycentre des sommets vivants
//   L'aire et le centre attrapent les regressions que le comptage seul laisse
//   passer : une operation peut conserver V/F/E tout en deplacant la geometrie.
//
// USAGE
//   NKEditMeshHarness              -> imprime la signature de chaque cas
//   NKEditMeshHarness --baseline   -> ecrit editmesh_baseline.txt
//   NKEditMeshHarness --check      -> compare a editmesh_baseline.txt, code de
//                                     sortie 1 si divergence (utilisable en CI)
// =============================================================================
#include "NKRenderer/Mesh/NkEditMesh.h"
#include "NKRenderer/Core/NkGizmo.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKLogger/NkLog.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

using namespace nkentseu;
using namespace nkentseu::renderer;

// ── PRIMITIVES D'ENTREE ─────────────────────────────────────────────────────
// Regenerees ici plutot que tirees de NkMeshSystem : ce sont les DONNEES du
// test, pas ce qui est teste, et NkMeshSystem exige un peripherique graphique.
// Les formules sont celles du moteur, a l'identique.

static void MakeCube(NkVector<NkVertex3D> &v, NkVector<uint32> &idx) {
	v.Clear();
	idx.Clear();
	const NkVec3f n[6] = {{0, 0, 1}, {0, 0, -1}, {-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
	const NkVec3f t[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, -1}, {0, 0, 1}, {1, 0, 0}, {1, 0, 0}};
	const NkVec3f p[8] = {{-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f},	{0.5f, 0.5f, 0.5f},	 {-0.5f, 0.5f, 0.5f},
						  {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}};
	const int32 fi[6][4] = {{1, 0, 3, 2}, {4, 5, 6, 7}, {0, 4, 7, 3}, {5, 1, 2, 6}, {3, 7, 6, 2}, {0, 1, 5, 4}};
	const NkVec2f uvs[4] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
	for (int32 f = 0; f < 6; f++) {
		for (int32 k = 0; k < 4; k++) {
			NkVertex3D vt{};
			vt.pos = p[fi[f][k]];
			vt.normal = n[f];
			vt.tangent = t[f];
			vt.uv = uvs[k];
			vt.uv2 = {0.f, 0.f};
			vt.color = 0xFFFFFFFFu;
			v.PushBack(vt);
		}
		const uint32 b = (uint32)f * 4u;
		idx.PushBack(b);
		idx.PushBack(b + 1);
		idx.PushBack(b + 2);
		idx.PushBack(b);
		idx.PushBack(b + 2);
		idx.PushBack(b + 3);
	}
}

static void MakeSphere(uint32 stacks, uint32 slices, NkVector<NkVertex3D> &v, NkVector<uint32> &idx) {
	v.Clear();
	idx.Clear();
	for (uint32 i = 0; i <= stacks; i++) {
		const bool pole = (i == 0 || i == stacks);
		const float32 phi = 3.14159265358979f * (float32)i / (float32)stacks;
		const float32 sp = pole ? 0.f : sinf(phi);
		const float32 cp = (i == 0) ? 1.f : ((i == stacks) ? -1.f : cosf(phi));
		for (uint32 j = 0; j <= slices; j++) {
			const float32 th = 2.f * 3.14159265358979f * (float32)j / (float32)slices;
			const float32 x = sp * cosf(th), y = cp, z = sp * sinf(th);
			NkVertex3D vt{};
			vt.pos = {x * 0.5f, y * 0.5f, z * 0.5f};
			vt.normal = pole ? NkVec3f{0.f, cp, 0.f} : NkVec3f{x, y, z};
			vt.tangent = {-sinf(th), 0.f, cosf(th)};
			vt.uv = {pole ? ((float32)j + 0.5f) / (float32)slices : (float32)j / (float32)slices,
					 1.f - (float32)i / (float32)stacks};
			vt.uv2 = {0.f, 0.f};
			vt.color = 0xFFFFFFFFu;
			v.PushBack(vt);
		}
	}
	const uint32 S1 = slices + 1;
	for (uint32 i = 0; i < stacks; i++)
		for (uint32 j = 0; j < slices; j++) {
			const uint32 b = i * S1 + j;
			if (i == 0) {
				idx.PushBack(b);
				idx.PushBack(b + S1);
				idx.PushBack(b + S1 + 1);
			} else if (i + 1 == stacks) {
				idx.PushBack(b);
				idx.PushBack(b + S1);
				idx.PushBack(b + 1);
			} else {
				idx.PushBack(b);
				idx.PushBack(b + S1);
				idx.PushBack(b + 1);
				idx.PushBack(b + 1);
				idx.PushBack(b + S1);
				idx.PushBack(b + S1 + 1);
			}
		}
}

static void MakeGrid(uint32 n, NkVector<NkVertex3D> &v, NkVector<uint32> &idx) {
	v.Clear();
	idx.Clear();
	for (uint32 z = 0; z <= n; z++)
		for (uint32 x = 0; x <= n; x++) {
			NkVertex3D vt{};
			vt.pos = {(float32)x / (float32)n - 0.5f, 0.f, (float32)z / (float32)n - 0.5f};
			vt.normal = {0.f, 1.f, 0.f};
			vt.tangent = {1.f, 0.f, 0.f};
			vt.uv = {(float32)x / (float32)n, (float32)z / (float32)n};
			vt.uv2 = {0.f, 0.f};
			vt.color = 0xFFFFFFFFu;
			v.PushBack(vt);
		}
	for (uint32 z = 0; z < n; z++)
		for (uint32 x = 0; x < n; x++) {
			const uint32 a = z * (n + 1) + x, b = a + 1, c = a + n + 1, d = c + 1;
			idx.PushBack(a);
			idx.PushBack(c);
			idx.PushBack(b);
			idx.PushBack(b);
			idx.PushBack(c);
			idx.PushBack(d);
		}
}

// ── SIGNATURE ───────────────────────────────────────────────────────────────
struct Sig {
		uint32 verts = 0, faces = 0, edges = 0, boundary = 0, nonManifold = 0, tris = 0;
		float32 area = 0.f;
		NkVec3f center{0.f, 0.f, 0.f};
};

static Sig Signature(const NkEditMesh &m) {
	Sig s;
	// Soudure : deux sommets EXACTEMENT au meme endroit sont une seule identite
	// topologique. Sans elle, une primitive dont les faces dupliquent leurs
	// sommets (cube = 24) n'aurait aucune arete partagee et le comptage serait
	// faux — c'est exactement le piege qui bloquait le loop cut a l'epoque.
	NkVector<uint32> canon;
	m.BuildVertexMerge(canon);

	NkVector<uint8> vAlive;
	vAlive.Resize(m.VertCount());
	for (uint32 i = 0; i < m.VertCount(); i++)
		vAlive[i] = 0;

	NkHashMap<uint64, uint32> edgeCount;
	NkVector<NkEmId> loop;
	for (uint32 f = 0; f < m.FaceCount(); f++) {
		if (!m.faces[f].alive)
			continue;
		loop.Clear();
		m.GetFaceVerts((NkEmId)f, loop);
		if (loop.Size() < 3)
			continue;
		s.faces++;
		s.tris += (uint32)loop.Size() - 2u;
		for (uint32 k = 0; k < (uint32)loop.Size(); k++) {
			const uint32 a = loop[k], b = loop[(k + 1) % (uint32)loop.Size()];
			if (a < vAlive.Size())
				vAlive[a] = 1;
			const uint32 ca = (a < canon.Size()) ? canon[a] : a;
			const uint32 cb = (b < canon.Size()) ? canon[b] : b;
			const uint64 lo = ca < cb ? ca : cb, hi = ca < cb ? cb : ca;
			const uint64 key = (lo << 32) | hi;
			uint32 *e = edgeCount.Find(key);
			if (e)
				(*e)++;
			else
				edgeCount.InsertOrAssign(key, 1u);
		}
		// Aire par eventail : independante de la triangulation choisie tant que
		// la face est plane, et stable pour une face non plane donnee.
		for (uint32 k = 1; k + 1 < (uint32)loop.Size(); k++) {
			const NkVec3f &p0 = m.verts[loop[0]].pos, &p1 = m.verts[loop[k]].pos, &p2 = m.verts[loop[k + 1]].pos;
			s.area += (p1 - p0).Cross(p2 - p0).Len() * 0.5f;
		}
	}
	for (uint32 i = 0; i < vAlive.Size(); i++)
		if (vAlive[i]) {
			s.verts++;
			s.center = s.center + m.verts[i].pos;
		}
	if (s.verts)
		s.center = s.center * (1.f / (float32)s.verts);
	for (auto it = edgeCount.Begin(); it != edgeCount.End(); ++it) {
		s.edges++;
		const uint32 c = it->Second; // NkPair expose First/Second (pas key/value)
		if (c == 1)
			s.boundary++;
		else if (c > 2)
			s.nonManifold++;
	}
	return s;
}

static int32 gFail = 0;
static char gLines[512][256];
static int32 gLineCount = 0;

static void Emit(const char *name, const Sig &s) {
	// Aire et centre arrondis : on compare des invariants, pas le dernier bit
	// d'un flottant. Une refonte peut changer l'ORDRE des sommations sans rien
	// changer au maillage — exiger l'egalite binaire ferait echouer le harnais
	// pour du bruit d'arrondi.
	if (gLineCount < 512) {
		snprintf(gLines[gLineCount], 256, "%-34s V=%-6u F=%-6u E=%-6u bord=%-5u nonmanif=%-3u tri=%-6u A=%.4f C=(%.4f,%.4f,%.4f)",
				 name, s.verts, s.faces, s.edges, s.boundary, s.nonManifold, s.tris, (double)s.area,
				 (double)s.center.x, (double)s.center.y, (double)s.center.z);
		gLineCount++;
	}
}

// ── BATTERIE ────────────────────────────────────────────────────────────────
static void Battery() {
	NkVector<NkVertex3D> v;
	NkVector<uint32> idx;

	struct Prim {
			const char *name;
			int32 kind; // 0=cube 1=sphere 2=grille
	};
	const Prim prims[3] = {{"cube", 0}, {"sphere16", 1}, {"grille4", 2}};

	for (int32 pi = 0; pi < 3; pi++) {
		auto build = [&](NkEditMesh &m, bool quadify) {
			if (prims[pi].kind == 0)
				MakeCube(v, idx);
			else if (prims[pi].kind == 1)
				MakeSphere(16, 16, v, idx);
			else
				MakeGrid(4, v, idx);
			m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), quadify);
		};
		char nm[128];

		// Construction seule, avec et sans quadification : c'est la fondation de
		// tout le reste, une divergence ici invalide toutes les lignes suivantes.
		{
			NkEditMesh m;
			build(m, false);
			snprintf(nm, sizeof(nm), "%s/build-tri", prims[pi].name);
			Emit(nm, Signature(m));
		}
		{
			NkEditMesh m;
			build(m, true);
			snprintf(nm, sizeof(nm), "%s/build-quad", prims[pi].name);
			Emit(nm, Signature(m));
		}
		// Aller-retour polygones : doit etre NEUTRE.
		{
			NkEditMesh m;
			build(m, true);
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			m.ToPolygons(pv, fs, fv);
			NkEditMesh m2;
			m2.BuildFromPolygons(pv.Data(), (uint32)pv.Size(), fs.Data(),
								 (uint32)(fs.Size() > 0 ? fs.Size() - 1 : 0), fv.Data());
			snprintf(nm, sizeof(nm), "%s/polygones-aller-retour", prims[pi].name);
			Emit(nm, Signature(m2));
		}

		// Operations, chacune sur un maillage NEUF (pas d'enchainement : une
		// regression precoce ne doit pas contaminer les lignes suivantes).
		struct Op {
				const char *name;
				int32 id;
		};
		const Op ops[10] = {{"selectall+extrude-faces", 0}, {"selectall+extrude-aretes", 1},
							{"selectall+subdiv", 2},		{"selectall+inset", 3},
							{"selectall+bevel", 4},			{"selectall+split-aretes", 5},
							{"selectall+dissolve", 6},		{"selectall+tosphere", 7},
							{"selectall+shrinkfatten", 8},	{"selectall+shade-smooth", 9}};
		for (int32 oi = 0; oi < 10; oi++) {
			NkEditMesh m;
			build(m, true);
			m.SelectAll();
			bool ok = false;
			switch (ops[oi].id) {
				case 0: ok = m.ExtrudeSelectedFaces(); break;
				case 1: ok = m.ExtrudeSelectedEdges(); break;
				case 2: ok = m.SubdivideSelectedFaces(); break;
				case 3: ok = m.InsetSelectedFaces(); break;
				case 4: ok = m.BevelSelected(); break;
				case 5: ok = m.SplitSelectedEdges(); break;
				case 6: ok = m.DissolveSelected(); break;
				case 7: {
					NkToSphereParams p;
					ok = m.ToSphereSelected(p);
					break;
				}
				case 8: {
					NkShrinkFattenParams p;
					ok = m.ShrinkFattenSelected(p);
					break;
				}
				case 9: ok = m.SetShadeSmooth(true); break;
			}
			snprintf(nm, sizeof(nm), "%s/%s%s", prims[pi].name, ops[oi].name, ok ? "" : " [REFUSE]");
			Emit(nm, Signature(m));
		}
	}
}

// ── BATTERIE ARETES FILAIRES (etape 1 BMesh : F sur 2 sommets) ──────────────
// Cas AJOUTES EN FIN de batterie : les 39 signatures historiques restent aux
// memes lignes, donc comparables a l'ancienne reference ligne a ligne.
// La Signature() ne compte que les aretes PORTEES PAR DES FACES (c'est voulu :
// elle mesure la topologie de surface) ; les aretes filaires sont donc mesurees
// ici par EdgeCount() et GetUniqueEdges(), les deux chemins que l'editeur
// utilise reellement (structure + affichage fil de fer).
static void WireBattery() {
	NkVector<NkVertex3D> v;
	NkVector<uint32> idx;
	MakeCube(v, idx);

	// F sur deux sommets NON relies (coins opposes de la face 0 : verts 0 et 2,
	// positions p[1] et p[3] — la diagonale n'existe pas apres quadify).
	{
		NkEditMesh m;
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		const uint32 e0 = m.EdgeCount();
		m.verts[0].sel = 1;
		m.verts[2].sel = 1;
		const bool ok = m.MakeEdgeFromSelected();
		NkVector<uint32> pairs;
		m.GetUniqueEdges(pairs);
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256,
					 "%-34s ok=%d E:%u->%u filDeFer=%u", "cube/F-2sommets-diagonale",
					 ok ? 1 : 0, e0, m.EdgeCount(), (uint32)(pairs.Size() / 2));
			gLineCount++;
		}
	}
	// F sur deux sommets DEJA relies par une arete de face : rien de nouveau.
	{
		NkEditMesh m;
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		const uint32 e0 = m.EdgeCount();
		m.verts[0].sel = 1;
		m.verts[1].sel = 1; // arete du bord de la face 0 : existe deja
		const bool ok = m.MakeEdgeFromSelected();
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256,
					 "%-34s ok=%d E:%u->%u (deja presente : rien cree)",
					 "cube/F-2sommets-deja-relies", ok ? 1 : 0, e0, m.EdgeCount());
			gLineCount++;
		}
	}
	// PERSISTANCE : l'arete filaire doit survivre a RebuildEdges — c'est la
	// promesse centrale de l'entite (rien d'autre ne peut la recreer).
	{
		NkEditMesh m;
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		m.verts[0].sel = 1;
		m.verts[2].sel = 1;
		m.MakeEdgeFromSelected();
		const uint32 avant = m.EdgeCount();
		m.RebuildEdges();
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256,
					 "%-34s E avant rebuild=%u apres=%u (identique attendu)",
					 "cube/filaire-survit-au-rebuild", avant, m.EdgeCount());
			gLineCount++;
		}
	}
}

// Cas MERGE ajoutes en fin de batterie (les 42 precedents gardent leurs lignes).
static void MergeBattery() {
	NkVector<NkVertex3D> v;
	NkVector<uint32> idx;
	MakeCube(v, idx);
	// COLLAPSE sur DEUX ilots disjoints (2 coins opposes du cube, copies
	// comprises) : chaque ilot doit fusionner vers SON centre -> il reste 2
	// sommets topologiques la ou Center n'en laisserait qu'un.
	{
		NkEditMesh m2;
		m2.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		NkVector<uint32> canon;
		m2.BuildVertexMerge(canon);
		// coin A = position de verts[0] ; coin B = son oppose. Selectionne toutes
		// les copies de chaque coin (flushing simule).
		const NkVec3f pa = m2.verts[0].pos;
		const NkVec3f pb = {-pa.x, -pa.y, -pa.z};
		for (uint32 i = 0; i < m2.VertCount(); i++) {
			const NkVec3f q = m2.verts[i].pos;
			if ((q - pa).Len() < 1e-6f || (q - pb).Len() < 1e-6f)
				m2.verts[i].sel = 1;
		}
		NkMergeParams mp;
		mp.mode = NkMergeParams::Collapse;
		const bool ok = m2.MergeSelectedVerts(mp);
		Emit("cube/merge-collapse-2ilots", Signature(m2));
		(void)ok;
	}
	// BY DISTANCE avec un seuil couvrant un coin (les 3 copies co-localisees d'un
	// coin sont deja soudees topologiquement : rien ne doit fusionner d'autre) —
	// verifie que le seuil n'attrape pas les coins voisins a 1.0 d'ecart.
	{
		NkEditMesh m2;
		m2.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		m2.SelectAll();
		NkMergeParams mp;
		mp.mode = NkMergeParams::ByDistance;
		mp.distance = 0.1f; // < arete (1.0) : aucune fusion inter-coins attendue
		const bool ok = m2.MergeSelectedVerts(mp);
		char nm[96];
		snprintf(nm, sizeof(nm), "cube/merge-bydistance-0.1%s", ok ? "" : " [REFUSE]");
		Emit(nm, Signature(m2));
	}
}

// Cas EXTRUDE ajoutes en fin (les 44 precedents gardent leurs lignes).
// Cible : la SPHERE, seule primitive assez courbe pour que Region et
// AlongNormals divergent visiblement. Sur un cube les deux coincideraient face
// par face et le test ne prouverait rien.
static void ExtrudeBattery() {
	NkVector<NkVertex3D> v;
	NkVector<uint32> idx;
	MakeSphere(16, 16, v, idx);
	struct Case {
			const char *name;
			int32 dir;
	};
	const Case cs[3] = {{"region", NkExtrudeParams::Region},
						{"along-normals", NkExtrudeParams::AlongNormals},
						{"to-cursor", NkExtrudeParams::ToCursor}};
	for (int32 i = 0; i < 3; i++) {
		NkEditMesh m;
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		m.SelectAll();
		NkExtrudeParams p;
		p.direction = cs[i].dir;
		p.offset = 0.2f;
		p.target = {0.f, 2.f, 0.f}; // ToCursor : point au-dessus de la sphere
		const bool ok = m.ExtrudeSelectedFaces(p);
		char nm[96];
		snprintf(nm, sizeof(nm), "sphere16/extrude-%s%s", cs[i].name, ok ? "" : " [REFUSE]");
		Emit(nm, Signature(m));
	}
}

// Cas LOT 5 ajoutes en fin (les 47 precedents gardent leurs lignes).
// Cible : la GRILLE 4x4 — surface plane et reguliere, donc l'effet du
// proportional editing et de la symetrie se lit dans l'aire et le barycentre
// sans etre masque par une courbure preexistante.
static void Lot5Battery() {
	NkVector<NkVertex3D> v;
	NkVector<uint32> idx;
	MakeGrid(4, v, idx);

	// UN seul sommet (le coin 0) tire vers le haut, trois regimes.
	struct Case {
			const char *name;
			bool prop;
			int32 falloff;
			bool symX;
	};
	const Case cs[4] = {{"move-simple", false, 0, false},
						{"move-proportional-smooth", true, NkProportionalParams::Smooth, false},
						{"move-proportional-sharp", true, NkProportionalParams::Sharp, false},
						{"move-symetrie-X", false, 0, true}};
	for (int32 i = 0; i < 4; i++) {
		NkEditMesh m;
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		m.verts[0].sel = 1; // coin (-0.5, 0, -0.5)
		NkProportionalParams pp;
		pp.enabled = cs[i].prop;
		pp.falloff = cs[i].falloff;
		pp.radius = 0.6f;
		NkSymmetryParams sy;
		sy.x = cs[i].symX;
		const bool ok = m.MoveSelected({0.f, 0.4f, 0.f}, pp, sy);
		char nm[96];
		snprintf(nm, sizeof(nm), "grille4/%s%s", cs[i].name, ok ? "" : " [REFUSE]");
		Emit(nm, Signature(m));
	}
	// Courbe d'influence : valeurs a distances fixes, pour que toute modification
	// de la formule se voie immediatement dans la reference.
	{
		char buf[256];
		snprintf(buf, sizeof(buf),
				 "smooth(0,.25,.5,.75)=%.3f/%.3f/%.3f/%.3f sphere(.5)=%.3f sharp(.5)=%.3f const(.9)=%.3f",
				 (double)NkEditMesh::ProportionalWeight(0.f, 1.f, NkProportionalParams::Smooth),
				 (double)NkEditMesh::ProportionalWeight(0.25f, 1.f, NkProportionalParams::Smooth),
				 (double)NkEditMesh::ProportionalWeight(0.5f, 1.f, NkProportionalParams::Smooth),
				 (double)NkEditMesh::ProportionalWeight(0.75f, 1.f, NkProportionalParams::Smooth),
				 (double)NkEditMesh::ProportionalWeight(0.5f, 1.f, NkProportionalParams::Sphere),
				 (double)NkEditMesh::ProportionalWeight(0.5f, 1.f, NkProportionalParams::Sharp),
				 (double)NkEditMesh::ProportionalWeight(0.9f, 1.f, NkProportionalParams::Constant));
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s %s", "courbes/influence", buf);
			gLineCount++;
		}
	}
}

// ── ORDRE DE SELECTION : Merge At First / At Last ───────────────────────────
// LE CAS QUI COMPTE : les MEMES deux sommets, selectionnes dans l'ORDRE INVERSE.
// Avec l'ancien comportement (plus petit / plus grand INDICE), les deux scenarios
// donnaient exactement la meme ligne — le harnais ne pouvait donc pas distinguer
// « ca marche » de « ca ne regarde pas l'ordre ». Ici les deux lignes DOIVENT
// differer, et l'une doit etre l'image de l'autre.
static void SelOrderBattery() {
	NkVector<NkVertex3D> v;
	NkVector<uint32> idx;
	MakeCube(v, idx);
	// Deux coins opposes. Chacun existe en plusieurs COPIES (le cube duplique ses
	// sommets par face) : un « clic » selectionne donc toutes les copies du coin,
	// exactement comme le fait l'editeur apres propagation aux coincidents.
	for (int32 rev = 0; rev < 2; rev++) {
		NkEditMesh m;
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		const NkVec3f pa = m.verts[0].pos;
		const NkVec3f pb = {-pa.x, -pa.y, -pa.z};
		const NkVec3f p1 = rev ? pb : pa, p2 = rev ? pa : pb;
		NkVector<uint8> flags;
		flags.Resize(m.VertCount());
		for (uint32 i = 0; i < m.VertCount(); i++)
			flags[i] = 0;
		// CLIC 1 puis CLIC 2 : deux appels successifs, chacun poussant le tableau
		// ENTIER — c'est ce que fait l'editeur a chaque synchronisation. L'ordre
		// doit survivre a cette repetition.
		for (uint32 i = 0; i < m.VertCount(); i++)
			if ((m.verts[i].pos - p1).Len() < 1e-6f)
				flags[i] = 1;
		m.SetVertSelection(flags.Data(), (uint32)flags.Size());
		for (uint32 i = 0; i < m.VertCount(); i++)
			if ((m.verts[i].pos - p2).Len() < 1e-6f)
				flags[i] = 1;
		m.SetVertSelection(flags.Data(), (uint32)flags.Size());
		const int32 fi = m.FirstSelected(), li = m.LastSelected();
		const NkVec3f fp = (fi >= 0) ? m.verts[(uint32)fi].pos : NkVec3f{0.f, 0.f, 0.f};
		const NkVec3f lp = (li >= 0) ? m.verts[(uint32)li].pos : NkVec3f{0.f, 0.f, 0.f};
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s premier=(%.2f,%.2f,%.2f) dernier=(%.2f,%.2f,%.2f)",
					 rev ? "ordre/cube-clic-B-puis-A" : "ordre/cube-clic-A-puis-B", (double)fp.x, (double)fp.y,
					 (double)fp.z, (double)lp.x, (double)lp.y, (double)lp.z);
			gLineCount++;
		}
		// Merge At First : tout converge vers le PREMIER CLIQUE. Le centre du
		// maillage resultant en porte la trace, donc la signature suffit.
		NkEditMesh mf = m;
		NkMergeParams mp;
		mp.mode = NkMergeParams::First;
		const bool okF = mf.MergeSelectedVerts(mp);
		char nm[96];
		snprintf(nm, sizeof(nm), "ordre/merge-first-%s%s", rev ? "BA" : "AB", okF ? "" : " [REFUSE]");
		Emit(nm, Signature(mf));
		NkEditMesh ml = m;
		mp.mode = NkMergeParams::Last;
		const bool okL = ml.MergeSelectedVerts(mp);
		snprintf(nm, sizeof(nm), "ordre/merge-last-%s%s", rev ? "BA" : "AB", okL ? "" : " [REFUSE]");
		Emit(nm, Signature(ml));
	}
	// DESELECTION : un sommet retire puis reselectionne repasse EN DERNIER. Sans
	// cela, « dernier selectionne » designerait un geste ancien et Merge At Last
	// viserait un coin que l'utilisateur croit avoir abandonne.
	{
		NkEditMesh m;
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		const NkVec3f pa = m.verts[0].pos;
		const NkVec3f pb = {-pa.x, -pa.y, -pa.z};
		NkVector<uint8> flags;
		flags.Resize(m.VertCount());
		auto set = [&](NkVec3f p, uint8 on) {
			for (uint32 i = 0; i < m.VertCount(); i++)
				if ((m.verts[i].pos - p).Len() < 1e-6f)
					flags[i] = on;
		};
		for (uint32 i = 0; i < m.VertCount(); i++)
			flags[i] = 0;
		set(pa, 1);
		m.SetVertSelection(flags.Data(), (uint32)flags.Size()); // A : rang 1
		set(pb, 1);
		m.SetVertSelection(flags.Data(), (uint32)flags.Size()); // B : rang 2
		set(pa, 0);
		m.SetVertSelection(flags.Data(), (uint32)flags.Size()); // A retire
		set(pa, 1);
		m.SetVertSelection(flags.Data(), (uint32)flags.Size()); // A revient -> rang 3
		const int32 fi = m.FirstSelected(), li = m.LastSelected();
		const NkVec3f fp = (fi >= 0) ? m.verts[(uint32)fi].pos : NkVec3f{0.f, 0.f, 0.f};
		const NkVec3f lp = (li >= 0) ? m.verts[(uint32)li].pos : NkVec3f{0.f, 0.f, 0.f};
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s premier=(%.2f,%.2f,%.2f) dernier=(%.2f,%.2f,%.2f)",
					 "ordre/cube-A-retire-puis-remis", (double)fp.x, (double)fp.y, (double)fp.z, (double)lp.x,
					 (double)lp.y, (double)lp.z);
			gLineCount++;
		}
	}
}

// ── BMESH ETAPE 2 : CYCLE RADIAL ET CYCLE DISQUE ────────────────────────────
// Le cas qui compte est le NON-MANIFOLD : une arete portee par TROIS faces. La
// structure ne savait pas la representer — `Hedge::twin` ne designe qu'UNE
// opposee, donc l'appariement en retenait deux et la troisieme devenait
// invisible pour tout parcours. On construit donc une jonction en T (un cube
// plus une cloison collee sur une de ses aretes) et on verifie que :
//   1. l'arete partagee est bien vue avec TROIS faces ;
//   2. « la face d'en face » est REFUSEE la-bas, au lieu d'en rendre une au
//      hasard ;
//   3. la boucle d'aretes S'ARRETE a cette arete, comme dans Blender, au lieu
//      de basculer sur une branche que personne n'a choisie.
static void Bmesh2Battery() {
	NkVector<NkVertex3D> v;
	NkVector<uint32> idx;
	MakeCube(v, idx);
	// ── Reference : le cube seul ────────────────────────────────────────────
	{
		NkEditMesh m;
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		m.RebuildEdges();
		// Disque : sur un cube, chaque coin porte exactement 3 aretes.
		NkVector<NkEmId> disk;
		const uint32 d0 = m.VertEdges(0, disk);
		// Radial : toute arete d'un cube ferme porte exactement 2 faces.
		uint32 rad2 = 0, radOther = 0;
		for (uint32 e = 0; e < (uint32)m.edges.Size(); ++e) {
			if (!m.edges[e].alive)
				continue;
			if (m.RadialCount((NkEmId)e) == 2)
				rad2++;
			else
				radOther++;
		}
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s aretes=%u radial2=%u autres=%u nonmanif=%u disque(v0)=%u",
					 "bmesh2/cube-cycles", m.EdgeCount(), rad2, radOther, m.NonManifoldEdgeCount(), d0);
			gLineCount++;
		}
	}
	// ── Jonction en T : une cloison collee sur l'arete (0.5,-0.5,±0.5) ──────
	// La cloison partage EXACTEMENT deux sommets du cube ; l'arete qui les relie
	// se retrouve donc portee par 3 faces (2 du cube + 1 de la cloison).
	NkVector<NkVertex3D> vt = v;
	NkVector<uint32> it = idx;
	{
		const NkVec3f a = {0.5f, -0.5f, 0.5f}, b = {0.5f, -0.5f, -0.5f};
		const NkVec3f c = {1.5f, -0.5f, -0.5f}, d = {1.5f, -0.5f, 0.5f};
		const NkVec3f q[4] = {a, b, c, d};
		const uint32 base = (uint32)vt.Size();
		for (int32 k = 0; k < 4; k++) {
			NkVertex3D x{};
			x.pos = q[k];
			x.normal = {0.f, 1.f, 0.f};
			x.tangent = {1.f, 0.f, 0.f};
			x.color = 0xFFFFFFFFu;
			vt.PushBack(x);
		}
		it.PushBack(base);
		it.PushBack(base + 1);
		it.PushBack(base + 2);
		it.PushBack(base);
		it.PushBack(base + 2);
		it.PushBack(base + 3);
	}
	{
		NkEditMesh m;
		m.BuildFromIndexed(vt.Data(), (uint32)vt.Size(), it.Data(), (uint32)it.Size(), true);
		m.RebuildEdges();
		// Retrouve l'arete partagee par ses deux sommets (indices bruts : les
		// copies coincidentes sont resolues en interne).
		int32 ia = -1, ib = -1;
		const NkVec3f pa = {0.5f, -0.5f, 0.5f}, pb = {0.5f, -0.5f, -0.5f};
		for (uint32 i = 0; i < m.VertCount(); ++i) {
			if (ia < 0 && (m.verts[i].pos - pa).Len() < 1e-6f)
				ia = (int32)i;
			if (ib < 0 && (m.verts[i].pos - pb).Len() < 1e-6f)
				ib = (int32)i;
		}
		const NkEmId e = (ia >= 0 && ib >= 0) ? m.EdgeBetween((uint32)ia, (uint32)ib) : NK_EM_INVALID;
		NkVector<NkEmId> fs;
		const uint32 nf = (e != NK_EM_INVALID) ? m.EdgeFaces(e, fs) : 0u;
		const NkEmId other = (e != NK_EM_INVALID && nf > 0) ? m.EdgeOtherFace(e, fs[0]) : NK_EM_INVALID;
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s radial=%u faces=%u nonmanif=%u autreface=%s",
					 "bmesh2/jonction-T", (e != NK_EM_INVALID) ? m.RadialCount(e) : 0u, nf,
					 m.NonManifoldEdgeCount(), (other == NK_EM_INVALID) ? "REFUSEE" : "rendue");
			gLineCount++;
		}
		// La boucle d'aretes partant de cette arete ne doit pas traverser la
		// jonction. On compare au meme depart sur le cube SEUL.
		NkVector<uint32> loopT;
		if (ia >= 0 && ib >= 0)
			m.GetEdgeLoop((uint32)ia, (uint32)ib, loopT);
		NkEditMesh mc;
		mc.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		mc.RebuildEdges();
		int32 ja = -1, jb = -1;
		for (uint32 i = 0; i < mc.VertCount(); ++i) {
			if (ja < 0 && (mc.verts[i].pos - pa).Len() < 1e-6f)
				ja = (int32)i;
			if (jb < 0 && (mc.verts[i].pos - pb).Len() < 1e-6f)
				jb = (int32)i;
		}
		NkVector<uint32> loopC;
		if (ja >= 0 && jb >= 0)
			mc.GetEdgeLoop((uint32)ja, (uint32)jb, loopC);
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s boucle-cube=%u aretes  boucle-jonction=%u aretes",
					 "bmesh2/boucle-arretee", (uint32)(loopC.Size() / 2), (uint32)(loopT.Size() / 2));
			gLineCount++;
		}
		Emit("bmesh2/jonction-T-signature", Signature(m));
	}
	// ── ARETE FILAIRE : cycle radial VIDE, et elle reste dans le disque ─────
	{
		NkEditMesh m;
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		m.RebuildEdges();
		const uint32 before = m.EdgeCount();
		// Deux coins OPPOSES du cube : la diagonale n'est bordee par aucune face.
		int32 ia = -1, ib = -1;
		const NkVec3f pa = m.verts[0].pos, pb = {-pa.x, -pa.y, -pa.z};
		for (uint32 i = 0; i < m.VertCount(); ++i) {
			if (ia < 0 && (m.verts[i].pos - pa).Len() < 1e-6f)
				ia = (int32)i;
			if (ib < 0 && (m.verts[i].pos - pb).Len() < 1e-6f)
				ib = (int32)i;
		}
		const NkEmId we = (ia >= 0 && ib >= 0) ? m.AddWireEdge((uint32)ia, (uint32)ib) : NK_EM_INVALID;
		NkVector<NkEmId> disk;
		const uint32 dAfter = (ia >= 0) ? m.VertEdges((uint32)ia, disk) : 0u;
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s aretes %u->%u  radial=%u filaire=%s disque(v)=%u",
					 "bmesh2/arete-filaire", before, m.EdgeCount(),
					 (we != NK_EM_INVALID) ? m.RadialCount(we) : 99u,
					 (we != NK_EM_INVALID && m.EdgeIsWire(we)) ? "oui" : "non", dAfter);
			gLineCount++;
		}
	}
}

// ── AIMANTATION (« snap ») ──────────────────────────────────────────────────
// Le cas qui compte est un objet HORS GRILLE au depart. Sans cela, increment
// relatif et grille absolue donnent le meme resultat et le harnais ne
// distinguerait pas « ca marche » de « ca ne regarde pas le mode ».
// L'objet part a x = 0,3 avec un pas de 0,5 :
//   increment RELATIF -> 0,3 + k x 0,5  (0,8 · 1,3 · 1,8 …) : jamais sur la grille ;
//   grille ABSOLUE    -> m x 0,5        (0,5 · 1,0 · 1,5 …) : toujours sur la grille.
// Le drag est un VRAI drag : on cherche une poignee a l'ecran, on la presse, puis
// on tire — c'est le chemin reel de DoPick/DoDrag, pas une fonction de calcul
// isolee qui pourrait etre juste sans que l'outil le soit.
static void SnapBattery() {
	using GZ = nkentseu::renderer::NkGizmo3D;
	const float32 kStart = 0.3f, kStep = 0.5f;

	auto runDrag = [&](bool snapOn, bool absolute, float32 &outX) -> bool {
		GZ g;
		g.SetCamera({0.f, 3.f, 6.f}, {0.f, 0.f, 0.f}, 60.f, 1280.f, 720.f);
		g.SetSnapSteps(kStep, 15.f, 0.1f);
		g.SetSnapEnabled(snapOn);
		g.SetSnapAbsolute(absolute);
		g.Select(0);
		g.SetMode(GZ::MODE_TRANSLATE);
		nkentseu::renderer::NkGizmoTarget t;
		t.base = NkMat4f::Translate({kStart, 0.f, 0.f});
		t.localHalf = {0.5f, 0.5f, 0.5f};
		t.pickRadius = 1.f;
		nkentseu::renderer::NkGizmoInput in;
		// 1) Une passe a VIDE : c'est elle qui calcule pivot, base et poignees.
		g.Update(&t, 1, in);
		// 2) Cherche une poignee : balayage grossier de l'ecran, on garde le point
		//    le plus proche au sens de HandlePickDistPx (c'est l'arbitrage reel).
		float32 bestD = 1e30f, bx = 0.f, by = 0.f;
		for (int32 y = 0; y < 720; y += 4)
			for (int32 x = 0; x < 1280; x += 4) {
				const float32 d = g.HandlePickDistPx((float32)x, (float32)y);
				if (d < bestD) {
					bestD = d;
					bx = (float32)x;
					by = (float32)y;
				}
			}
		if (bestD > 1e29f)
			return false; // aucune poignee trouvee
		// 3) Presse la poignee.
		in.mouseX = bx;
		in.mouseY = by;
		in.leftPressed = true;
		in.leftDown = true;
		g.Update(&t, 1, in);
		in.leftPressed = false;
		// 4) Tire. VERROU sur X : quelle que soit la poignee attrapee, le
		//    deplacement est celui de l'axe X — le test ne depend donc pas de
		//    l'endroit exact ou le balayage a trouve une poignee.
		in.lockAxis = 0;
		for (int32 k = 0; k < 24; k++) {
			in.mouseDX = 6.f;
			in.mouseDY = 0.f;
			in.mouseX += 6.f;
			in.ctrlDown = false; // l'aimantation vient de la BASCULE, pas de Ctrl
			g.Update(&t, 1, in);
		}
		const NkMat4f m = g.Apply(0, t.base);
		outX = (m * NkVec3f{0.f, 0.f, 0.f}).x;
		return true;
	};

	auto onGrid = [&](float32 v) {
		const float32 r = v / kStep;
		const float32 d = r - (float32)((int32)(r < 0.f ? r - 0.5f : r + 0.5f));
		return (d < 0.f ? -d : d) < 1e-3f;
	};

	float32 xFree = 0.f, xRel = 0.f, xAbs = 0.f;
	const bool okF = runDrag(false, false, xFree);
	const bool okR = runDrag(true, false, xRel);
	const bool okA = runDrag(true, true, xAbs);
	if (gLineCount < 512) {
		snprintf(gLines[gLineCount], 256,
				 "%-34s libre=%.3f increment=%.3f(grille:%s) absolu=%.3f(grille:%s) depart=%.2f pas=%.2f",
				 (okF && okR && okA) ? "snap/translate-x" : "snap/translate-x [ECHEC]", (double)xFree, (double)xRel,
				 onGrid(xRel) ? "oui" : "non", (double)xAbs, onGrid(xAbs) ? "oui" : "non", (double)kStart,
				 (double)kStep);
		gLineCount++;
	}
	// Ctrl INVERSE la bascule : aimantation ON + Ctrl = PAS d'aimantation. C'est
	// la difference de fond avec « Ctrl = aimanter », et c'est ce qui permet de
	// s'echapper ponctuellement quand on travaille aimante en permanence.
	{
		GZ g;
		g.SetSnapEnabled(false);
		const bool a = g.SnapActive(false), b = g.SnapActive(true);
		g.SetSnapEnabled(true);
		const bool c = g.SnapActive(false), d = g.SnapActive(true);
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s off:sansCtrl=%d avecCtrl=%d | on:sansCtrl=%d avecCtrl=%d",
					 "snap/bascule-inversee-par-ctrl", a ? 1 : 0, b ? 1 : 0, c ? 1 : 0, d ? 1 : 0);
			gLineCount++;
		}
	}
}

// ── SUBDIVISION DE SURFACE : CATMULL-CLARK vs SIMPLE ────────────────────────
// LE CAS QUI COMPTE est le RAYON, pas le nombre de faces. Une subdivision
// LINEAIRE et une Catmull-Clark produisent la MEME topologie (n quads par face
// de n coins) : compter sommets et faces ne distingue pas l'une de l'autre. Ce
// qui les separe, c'est que Catmull-Clark DEPLACE les sommets vers la surface
// limite. On mesure donc la distance au centre :
//   cube de cote 1 (rayon des coins = 0,866) ;
//   subdivision SIMPLE  -> le cube reste un cube, rayon max inchange ;
//   CATMULL-CLARK       -> la forme se contracte vers la sphere limite.
// Un test qui n'aurait verifie que V/F/E aurait valide un modificateur qui ne
// lisse rien — c'est exactement le defaut qui existait.
static void CatmullBattery() {
	NkVector<NkVertex3D> v;
	NkVector<uint32> idx;
	MakeCube(v, idx);
	auto radii = [](const NkEditMesh &m, float32 &rmin, float32 &rmax, float32 &vol) {
		rmin = 1e30f;
		rmax = 0.f;
		NkVec3f c{0.f, 0.f, 0.f};
		uint32 n = 0;
		for (uint32 i = 0; i < m.VertCount(); ++i) {
			c = c + m.verts[i].pos;
			n++;
		}
		if (n)
			c = c * (1.f / (float32)n);
		for (uint32 i = 0; i < m.VertCount(); ++i) {
			const float32 r = (m.verts[i].pos - c).Len();
			if (r < rmin)
				rmin = r;
			if (r > rmax)
				rmax = r;
		}
		vol = 0.f;
	};
	// Reference : le cube brut.
	{
		NkEditMesh m;
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		float32 a, b, c;
		radii(m, a, b, c);
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s rayon min=%.4f max=%.4f", "subsurf/cube-reference", (double)a,
					 (double)b);
			gLineCount++;
		}
	}
	// SIMPLE (lineaire) : densifie, ne deforme pas.
	{
		NkEditMesh m;
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		NkMeshModifier mod;
		mod.type = NkModifierType::Subsurf;
		mod.subsurfSimple = true;
		mod.subsurfLevels = 1;
		mod.Apply(m);
		float32 a, b, c;
		radii(m, a, b, c);
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s rayon min=%.4f max=%.4f", "subsurf/simple-n1", (double)a,
					 (double)b);
			gLineCount++;
		}
		Emit("subsurf/simple-n1-signature", Signature(m));
	}
	// CATMULL-CLARK : lisse. Niveaux 1 et 2 pour montrer la convergence.
	for (int32 lv = 1; lv <= 2; lv++) {
		NkEditMesh m;
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		NkMeshModifier mod;
		mod.type = NkModifierType::Subsurf;
		mod.subsurfSimple = false;
		mod.subsurfLevels = lv;
		mod.Apply(m);
		float32 a, b, c;
		radii(m, a, b, c);
		char nm[96];
		snprintf(nm, sizeof(nm), "subsurf/catmull-n%d", lv);
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s rayon min=%.4f max=%.4f", nm, (double)a, (double)b);
			gLineCount++;
		}
		snprintf(nm, sizeof(nm), "subsurf/catmull-n%d-signature", lv);
		Emit(nm, Signature(m));
	}
	// GRILLE OUVERTE : le BORD doit rester franc. C'est la regle de bordure
	// (M1 + 6V + M2)/8 : sans elle le contour serait aspire vers l'interieur et
	// un plan subdivise retrecirait — defaut classique de Catmull-Clark naif.
	{
		NkVector<NkVertex3D> gv;
		NkVector<uint32> gi;
		MakeGrid(4, gv, gi);
		NkEditMesh m;
		m.BuildFromIndexed(gv.Data(), (uint32)gv.Size(), gi.Data(), (uint32)gi.Size(), true);
		float32 x0 = -1e30f;
		for (uint32 i = 0; i < m.VertCount(); ++i)
			if (m.verts[i].pos.x > x0)
				x0 = m.verts[i].pos.x;
		m.SubdivideCatmullClark(1);
		float32 x1 = -1e30f;
		for (uint32 i = 0; i < m.VertCount(); ++i)
			if (m.verts[i].pos.x > x1)
				x1 = m.verts[i].pos.x;
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s bord x avant=%.4f apres=%.4f (retrait=%.4f)",
					 "subsurf/grille-bord-franc", (double)x0, (double)x1, (double)(x0 - x1));
			gLineCount++;
		}
		Emit("subsurf/grille-catmull-signature", Signature(m));
	}
}

int main(int argc, char **argv) {
	bool baseline = false, check = false;
	for (int32 i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--baseline") == 0)
			baseline = true;
		else if (strcmp(argv[i], "--check") == 0)
			check = true;
	}

	Battery();
	WireBattery();
	MergeBattery();
	ExtrudeBattery();
	Lot5Battery();
	// AJOUTEE EN FIN : les 52 lignes precedentes gardent leur numero, donc une
	// divergence ancienne reste comparable ligne a ligne avec l'ancienne reference.
	SelOrderBattery();
	Bmesh2Battery();
	SnapBattery();
	CatmullBattery();

	const char *path = "editmesh_baseline.txt";
	if (baseline) {
		FILE *f = fopen(path, "wb");
		if (!f) {
			printf("ECHEC : impossible d'ecrire %s\n", path);
			return 2;
		}
		for (int32 i = 0; i < gLineCount; i++)
			fprintf(f, "%s\n", gLines[i]);
		fclose(f);
		printf("reference ecrite : %s (%d lignes)\n", path, gLineCount);
		return 0;
	}

	if (check) {
		FILE *f = fopen(path, "rb");
		if (!f) {
			printf("ECHEC : %s introuvable — lancer d'abord --baseline\n", path);
			return 2;
		}
		char line[256];
		int32 i = 0, diff = 0;
		while (fgets(line, sizeof(line), f)) {
			size_t L = strlen(line);
			while (L && (line[L - 1] == '\n' || line[L - 1] == '\r'))
				line[--L] = 0;
			if (i >= gLineCount) {
				printf("MANQUANT  %s\n", line);
				diff++;
			} else if (strcmp(line, gLines[i]) != 0) {
				printf("DIVERGENCE\n  reference : %s\n  courant   : %s\n", line, gLines[i]);
				diff++;
			}
			i++;
		}
		fclose(f);
		for (; i < gLineCount; i++) {
			printf("EN TROP   %s\n", gLines[i]);
			diff++;
		}
		if (diff == 0)
			printf("CONFORME : %d cas, aucune divergence.\n", gLineCount);
		else
			printf("%d DIVERGENCE(S) sur %d cas.\n", diff, gLineCount);
		return diff ? 1 : 0;
	}

	for (int32 i = 0; i < gLineCount; i++)
		printf("%s\n", gLines[i]);
	printf("-- %d cas --\n", gLineCount);
	return gFail;
}
