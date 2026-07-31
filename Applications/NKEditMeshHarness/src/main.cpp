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
#include "NKGraph/NkNodeGraph.h"
#include "NKRenderer/Mesh/NkMeshAnalysis.h"
#include "NKRenderer/Core/NkGizmo.h"
#include "NKEditorKit/NkShortcutTable.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKLogger/NkLog.h"

#include <math.h>
#include <stdarg.h>
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

// ── PILE DE MODIFICATEURS ───────────────────────────────────────────────────
// Trois choses a prouver, et une seule est evidente.
//   1. L'ORDRE COMPTE : miroir-puis-tableau et tableau-puis-miroir ne donnent pas
//      le meme maillage. Si les deux signatures etaient egales, « remonter un
//      modificateur » ne servirait a rien et le reordonnancement serait decoratif.
//   2. L'IDENTIFIANT SURVIT au reordonnancement : c'est ce qui permettra a une
//      courbe d'animation de viser un modificateur qu'on a deplace entre-temps.
//      Pointer par INDICE se casserait au premier MoveUp.
//   3. APPLIQUER cuit le modificateur dans le maillage ET le retire de la pile :
//      le resultat doit etre celui de l'evaluation, et la pile doit avoir maigri.
static void ModStackBattery() {
	NkVector<NkVertex3D> v;
	NkVector<uint32> idx;
	MakeCube(v, idx);
	NkEditMesh base;
	base.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);

	NkMeshModifier mir;
	mir.type = NkModifierType::Mirror;
	mir.mirrorAxis = 0;
	NkMeshModifier arr;
	arr.type = NkModifierType::Array;
	arr.arrayCount = 3;
	arr.arrayOffset = {2.f, 0.f, 0.f};

	// 1) Miroir PUIS tableau.
	{
		NkModifierStack st;
		st.Add(mir);
		st.Add(arr);
		NkEditMesh out;
		st.Evaluate(base, out);
		Emit("pile/miroir-puis-tableau", Signature(out));
	}
	// 2) Tableau PUIS miroir — obtenu en REMONTANT le second, pas en reconstruisant
	//    la pile : c'est MoveUp qui est teste, pas ma capacite a ecrire deux piles.
	uint32 idMir = 0, idArr = 0;
	{
		NkModifierStack st;
		idMir = st.Add(mir);
		idArr = st.Add(arr);
		st.MoveUp(1); // le tableau passe en premier
		NkEditMesh out;
		st.Evaluate(base, out);
		Emit("pile/tableau-puis-miroir", Signature(out));
		// L'identifiant suit le modificateur, l'indice non.
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s miroir: id=%u indice=%d | tableau: id=%u indice=%d",
					 "pile/id-stable-apres-remontee", idMir, st.IndexOfId(idMir), idArr, st.IndexOfId(idArr));
			gLineCount++;
		}
	}
	// 3) DESACTIVER : un modificateur eteint ne doit rien produire.
	{
		NkModifierStack st;
		st.Add(mir);
		st.Add(arr);
		st.SetEnabled(1, false);
		NkEditMesh out;
		st.Evaluate(base, out);
		Emit("pile/tableau-desactive", Signature(out));
	}
	// 4) RETIRER et DUPLIQUER.
	{
		NkModifierStack st;
		st.Add(mir);
		st.Add(arr);
		const uint32 n0 = st.Count();
		st.Duplicate(0); // la copie s'insere JUSTE APRES l'original
		const uint32 n1 = st.Count();
		const bool dupAfter = (st.Count() >= 2) && (st.modifiers[1].type == NkModifierType::Mirror);
		const bool idNeuf = (st.Count() >= 2) && (st.modifiers[1].id != st.modifiers[0].id);
		st.Remove(1);
		const uint32 n2 = st.Count();
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s count %u->%u->%u  copie juste apres=%d  id neuf=%d",
					 "pile/dupliquer-retirer", n0, n1, n2, dupAfter ? 1 : 0, idNeuf ? 1 : 0);
			gLineCount++;
		}
	}
	// 5) APPLIQUER : cuit dans la base et retire de la pile. Le maillage obtenu
	//    doit etre celui qu'on aurait eu en evaluant, sinon « appliquer » ne
	//    signifierait pas « figer ce que je vois ».
	{
		NkModifierStack st;
		st.Add(mir);
		NkEditMesh baked = base;
		bool warn = true;
		const bool ok = st.ApplyToBase(0, baked, &warn);
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s ok=%d reste=%u avertissement-pas-premier=%d",
					 "pile/appliquer-miroir", ok ? 1 : 0, st.Count(), warn ? 1 : 0);
			gLineCount++;
		}
		Emit("pile/appliquer-miroir-signature", Signature(baked));
	}
	// 6) PARAMETRES ADRESSABLES PAR NOM — le socle de la future animation.
	//    On regle, on relit, et on verifie l'ECRETAGE : une courbe qui deborde ne
	//    doit pas produire un arrayCount negatif ni 40 niveaux de subdivision.
	{
		NkMeshModifier m = arr;
		float32 got = 0.f;
		m.SetParam("array_count", 7.f);
		m.GetParam("array_count", got);
		float32 clampLo = 0.f, clampHi = 0.f;
		m.SetParam("array_count", -5.f);
		m.GetParam("array_count", clampLo);
		m.SetParam("array_count", 9999.f);
		m.GetParam("array_count", clampHi);
		// Arrondi au PLUS PROCHE : 2,999 vise 3, pas 2.
		float32 round = 0.f;
		m.SetParam("array_count", 2.999f);
		m.GetParam("array_count", round);
		NkVec3f off{0.f, 0.f, 0.f};
		m.SetParamVec3("array_offset", NkVec3f{1.f, 2.f, 3.f});
		m.GetParamVec3("array_offset", off);
		const bool inconnu = m.SetParam("parametre_qui_nexiste_pas", 1.f);
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256,
					 "%-34s count=%.0f ecrete[%.0f..%.0f] arrondi(2.999)=%.0f offset=(%.0f,%.0f,%.0f) inconnu=%d",
					 "pile/parametres-par-nom", (double)got, (double)clampLo, (double)clampHi, (double)round,
					 (double)off.x, (double)off.y, (double)off.z, inconnu ? 1 : 0);
			gLineCount++;
		}
		// Inventaire publie : c'est ce que parcourra une interface ou un editeur de
		// courbes pour proposer « quoi animer ».
		uint32 tot = 0;
		char buf[192];
		buf[0] = 0;
		for (int32 t = 0; t < 3; t++) {
			uint32 n = 0;
			NkModifierParams((NkModifierType)t, n);
			tot += n;
		}
		snprintf(buf, sizeof(buf), "%s=%u %s=%u %s=%u total=%u", NkModifierTypeName(NkModifierType::Mirror),
				 NkMeshModifier{NkModifierType::Mirror}.ParamCount(), NkModifierTypeName(NkModifierType::Array),
				 NkMeshModifier{NkModifierType::Array}.ParamCount(), NkModifierTypeName(NkModifierType::Subsurf),
				 NkMeshModifier{NkModifierType::Subsurf}.ParamCount(), tot);
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s %s", "pile/inventaire-parametres", buf);
			gLineCount++;
		}
	}
}

// ── LOT DE MODIFICATEURS ────────────────────────────────────────────────────
// Chaque modificateur est applique a une primitive CHOISIE pour que son effet
// soit LISIBLE dans la signature — un modificateur teste sur une forme ou il ne
// change rien ne prouve rien. Ceux qui deforment sans toucher la topologie
// (Cast, Simple Deform, Smooth, Wave) sont juges sur l'AIRE et le rayon, les
// autres sur V/F/E.
static void ModsBattery() {
	NkVector<NkVertex3D> cv, gv;
	NkVector<uint32> ci, gi;
	MakeCube(cv, ci);
	MakeGrid(4, gv, gi);

	auto mk = [&](bool grid) {
		NkEditMesh m;
		if (grid)
			m.BuildFromIndexed(gv.Data(), (uint32)gv.Size(), gi.Data(), (uint32)gi.Size(), true);
		else
			m.BuildFromIndexed(cv.Data(), (uint32)cv.Size(), ci.Data(), (uint32)ci.Size(), true);
		return m;
	};
	auto rayonMax = [](const NkEditMesh &m) {
		NkVec3f c{0.f, 0.f, 0.f};
		for (uint32 i = 0; i < m.VertCount(); ++i)
			c = c + m.verts[i].pos;
		if (m.VertCount())
			c = c * (1.f / (float32)m.VertCount());
		float32 r = 0.f;
		for (uint32 i = 0; i < m.VertCount(); ++i) {
			const float32 d = (m.verts[i].pos - c).Len();
			if (d > r)
				r = d;
		}
		return r;
	};

	// ── GENERATE ────────────────────────────────────────────────────────────
	{ // SOLIDIFIER une GRILLE : c'est le cas ou il sert. Sur un cube (deja ferme)
	  // l'effet serait invisible dans le comptage. Le bord doit se refermer :
	  // bord=0 apres, alors que la grille en avait 32.
		NkEditMesh m = mk(true);
		NkMeshModifier mod;
		mod.type = NkModifierType::Solidify;
		mod.solidifyThickness = 0.1f;
		mod.solidifyRim = true;
		mod.Apply(m);
		Emit("mod/solidify-grille", Signature(m));
	}
	{ // Sans bordure : la coque reste OUVERTE -> le bord doit reapparaitre.
		NkEditMesh m = mk(true);
		NkMeshModifier mod;
		mod.type = NkModifierType::Solidify;
		mod.solidifyThickness = 0.1f;
		mod.solidifyRim = false;
		mod.Apply(m);
		Emit("mod/solidify-sans-bord", Signature(m));
	}
	{
		NkEditMesh m = mk(false);
		NkMeshModifier mod;
		mod.type = NkModifierType::Triangulate;
		mod.triangulateMinVerts = 4;
		mod.Apply(m);
		Emit("mod/triangulate-cube", Signature(m));
	}
	{ // minVerts = 5 : les quads du cube doivent RESTER des quads.
		NkEditMesh m = mk(false);
		NkMeshModifier mod;
		mod.type = NkModifierType::Triangulate;
		mod.triangulateMinVerts = 5;
		mod.Apply(m);
		Emit("mod/triangulate-min5-inerte", Signature(m));
	}
	{
		NkEditMesh m = mk(false);
		NkMeshModifier mod;
		mod.type = NkModifierType::Weld;
		mod.weldDistance = 0.6f; // > demi-arete : des coins voisins doivent fusionner
		mod.Apply(m);
		Emit("mod/weld-0.6", Signature(m));
	}
	{
		NkEditMesh m = mk(false);
		NkMeshModifier mod;
		mod.type = NkModifierType::Bevel;
		mod.bevelWidth = 0.1f;
		mod.bevelSegments = 1;
		mod.Apply(m);
		Emit("mod/bevel-cube", Signature(m));
	}
	{
		NkEditMesh m = mk(true);
		NkMeshModifier mod;
		mod.type = NkModifierType::Screw;
		mod.screwSteps = 6;
		mod.screwAngle = 180.f;
		mod.Apply(m);
		Emit("mod/screw-grille", Signature(m));
	}
	{
		NkEditMesh m = mk(false);
		NkMeshModifier mod;
		mod.type = NkModifierType::EdgeSplit;
		mod.Apply(m);
		Emit("mod/edgesplit-cube", Signature(m));
	}
	{
		NkEditMesh m = mk(false);
		NkMeshModifier mod;
		mod.type = NkModifierType::Decimate;
		mod.Apply(m);
		Emit("mod/decimate-cube", Signature(m));
	}
	{ // BUILD : la proportion est LE parametre a animer. 0,5 doit retirer la moitie
	  // des faces — c'est le seul modificateur dont l'interet est le temps.
		for (int32 k = 0; k < 2; k++) {
			NkEditMesh m = mk(false);
			NkMeshModifier mod;
			mod.type = NkModifierType::Build;
			mod.buildRatio = k ? 1.f : 0.5f;
			mod.Apply(m);
			Emit(k ? "mod/build-1.0" : "mod/build-0.5", Signature(m));
		}
	}
	{ // MASK : garde les faces dont TOUS les sommets sont selectionnes.
		NkEditMesh m = mk(false);
		const NkVec3f pa = m.verts[0].pos;
		NkVector<uint8> fl;
		fl.Resize(m.VertCount());
		for (uint32 i = 0; i < m.VertCount(); ++i)
			fl[i] = (m.verts[i].pos.z > 0.f) ? 1 : 0; // une face du cube
		m.SetVertSelection(fl.Data(), (uint32)fl.Size());
		(void)pa;
		NkMeshModifier mod;
		mod.type = NkModifierType::Mask;
		mod.Apply(m);
		Emit("mod/mask-face-avant", Signature(m));
	}

	// ── DEFORM (topologie inchangee : on juge le RAYON et l'AIRE) ────────────
	{
		// Sur un CUBE tous les sommets sont a la meme distance du centre : le rayon
		// max ne bougerait pas et le test ne prouverait rien. On prend donc une
		// GRILLE, dont les sommets ont des rayons varies, et on mesure MIN et MAX —
		// c'est l'ECART entre eux qui dit si la forme a ete projetee.
		// DEUX formes, parce qu'aucune ne suffit seule. Sur un CUBE tous les sommets
		// sont deja a la meme distance du centre : la projection SPHERIQUE ne les
		// bouge pas — resultat juste, mais qui ne prouve rien. Sur une GRILLE plane,
		// sphere et cylindre COINCIDENT (les sommets sont a y = 0), ce qui est
		// egalement correct et egalement peu discriminant. Les deux ensemble
		// distinguent bien les trois modes.
		const char *nm[3] = {"mod/cast-sphere", "mod/cast-cylindre", "mod/cast-cube"};
		for (int32 t = 0; t < 6; t++) {
			NkEditMesh m = mk(t < 3);
			float32 a0 = 1e30f, b0 = 0.f;
			for (uint32 i = 0; i < m.VertCount(); ++i) {
				const float32 d = m.verts[i].pos.Len();
				if (d < a0)
					a0 = d;
				if (d > b0)
					b0 = d;
			}
			NkMeshModifier mod;
			mod.type = NkModifierType::Cast;
			mod.castType = t % 3;
			mod.castFactor = 1.f;
			mod.Apply(m);
			float32 a1 = 1e30f, b1 = 0.f;
			for (uint32 i = 0; i < m.VertCount(); ++i) {
				const float32 d = m.verts[i].pos.Len();
				if (d < a1)
					a1 = d;
				if (d > b1)
					b1 = d;
			}
			if (gLineCount < 512) {
				char cn[96];
				snprintf(cn, sizeof(cn), "%s-%s", nm[t % 3], (t < 3) ? "grille" : "cube");
				snprintf(gLines[gLineCount], 256, "%-34s rayon [%.4f..%.4f] -> [%.4f..%.4f]", cn, (double)a0,
						 (double)b0, (double)a1, (double)b1);
				gLineCount++;
			}
		}
	}
	{
		const char *nm[4] = {"mod/deform-torsion", "mod/deform-courbure", "mod/deform-effilement",
							 "mod/deform-etirement"};
		for (int32 d = 0; d < 4; d++) {
			NkEditMesh m = mk(false);
			const float32 r0 = rayonMax(m);
			NkMeshModifier mod;
			mod.type = NkModifierType::SimpleDeform;
			mod.deformMode = d;
			mod.deformAngle = 90.f;
			mod.deformFactor = 0.5f;
			mod.Apply(m);
			if (gLineCount < 512) {
				snprintf(gLines[gLineCount], 256, "%-34s rayon max %.4f -> %.4f", nm[d], (double)r0, (double)rayonMax(m));
				gLineCount++;
			}
		}
	}
	{ // LISSER : sur un cube soude, la relaxation contracte vers le centre.
		NkEditMesh m = mk(false);
		const float32 r0 = rayonMax(m);
		NkMeshModifier mod;
		mod.type = NkModifierType::Smooth;
		mod.smoothFactor = 0.5f;
		mod.smoothRepeat = 3;
		mod.Apply(m);
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s rayon max %.4f -> %.4f", "mod/smooth-cube", (double)r0,
					 (double)rayonMax(m));
			gLineCount++;
		}
	}
	{ // ONDE : la PHASE doit changer le resultat — c'est ce qui la rend animable.
		float32 r[2] = {0.f, 0.f};
		for (int32 k = 0; k < 2; k++) {
			NkEditMesh m = mk(true);
			NkMeshModifier mod;
			mod.type = NkModifierType::Wave;
			mod.waveHeight = 0.3f;
			mod.waveWidth = 0.2f;
			mod.wavePhase = k ? 1.57f : 0.f;
			mod.Apply(m);
			r[k] = rayonMax(m);
		}
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s phase 0 -> rayon %.4f | phase 1,57 -> rayon %.4f",
					 "mod/wave-phase-animable", (double)r[0], (double)r[1]);
			gLineCount++;
		}
	}
	{ // OMBRAGE PAR ANGLE : a 30 deg un cube reste FRANC (angles a 90), a 100 deg
	  // il passe entierement en lisse. Le seuil doit donc changer le compte.
		for (int32 k = 0; k < 2; k++) {
			NkEditMesh m = mk(false);
			NkMeshModifier mod;
			mod.type = NkModifierType::SmoothByAngle;
			mod.autoSmoothAngle = k ? 100.f : 30.f;
			mod.Apply(m);
			uint32 sm = 0, fl2 = 0;
			for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f) {
				if (!m.faces[f].alive)
					continue;
				if (m.faces[f].smooth)
					sm++;
				else
					fl2++;
			}
			if (gLineCount < 512) {
				snprintf(gLines[gLineCount], 256, "%-34s seuil %.0f deg -> %u lisses / %u franches",
						 k ? "mod/autosmooth-100deg" : "mod/autosmooth-30deg", (double)mod.autoSmoothAngle, sm, fl2);
				gLineCount++;
			}
		}
	}
	// INVENTAIRE : ce que l'editeur de courbes verra.
	{
		uint32 tot = 0, types = 0;
		for (int32 t = 0; t <= (int32)NkModifierType::SmoothByAngle; t++) {
			uint32 n = 0;
			NkModifierParams((NkModifierType)t, n);
			tot += n;
			if (n)
				types++;
		}
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s %u types, %u parametres animables", "mod/inventaire-global", types,
					 tot);
			gLineCount++;
		}
	}
}

// ── TABLE DE RACCOURCIS ─────────────────────────────────────────────────────
// On y verse l'inventaire REEL des raccourcis de l'editeur, releve dans
// Demo3D.cpp. Deux choses a prouver :
//   1. la table REPOND — la meme touche donne une commande differente selon le
//      contexte, ce que la suite de `if` imbriques faisait de facon implicite ;
//   2. elle DETECTE LES CONFLITS. C'est sa raison d'etre : `Shift+S` etait deja
//      pris par « ombrage smooth », ce qui m'a force a des combinaisons moins
//      naturelles pour la pile de modificateurs — et je ne l'ai decouvert qu'a la
//      compilation, par hasard. Une table le dit tout de suite.
static void ShortcutBattery() {
	using namespace nkentseu::editorkit;
	NkShortcutTable t;
	// ⚠ ORDRE SIGNIFIANT : le plus SPECIFIQUE d'abord, comme les `if` imbriques
	// qu'on remplace testaient le mode edition avant le mode objet.
	// — mode EDITION
	t.Bind("mesh.extrude", "Extruder", NkKey::NK_E, NK_SC_NONE, NK_SCTX_EDIT);
	t.Bind("mesh.inset", "Inserer", NkKey::NK_I, NK_SC_NONE, NK_SCTX_EDIT);
	t.Bind("mesh.knife", "Couteau", NkKey::NK_K, NK_SC_NONE, NK_SCTX_EDIT);
	t.Bind("mesh.subdivide", "Subdiviser", NkKey::NK_W, NK_SC_NONE, NK_SCTX_EDIT);
	t.Bind("mesh.merge", "Souder", NkKey::NK_M, NK_SC_NONE, NK_SCTX_EDIT);
	t.Bind("mesh.delete", "Supprimer", NkKey::NK_X, NK_SC_NONE, NK_SCTX_EDIT);
	t.Bind("mesh.edge_split", "Separer les aretes", NkKey::NK_V, NK_SC_NONE, NK_SCTX_EDIT);
	t.Bind("mesh.spin", "Spin", NkKey::NK_J, NK_SC_NONE, NK_SCTX_EDIT);
	t.Bind("mesh.loopcut", "Loop cut", NkKey::NK_R, NK_SC_CTRL, NK_SCTX_EDIT);
	t.Bind("mesh.bevel_edge", "Chanfrein arete", NkKey::NK_B, NK_SC_CTRL, NK_SCTX_EDIT);
	t.Bind("mesh.bevel_vert", "Chanfrein sommet", NkKey::NK_B, (uint8)(NK_SC_CTRL | NK_SC_SHIFT), NK_SCTX_EDIT);
	t.Bind("mesh.dissolve", "Dissoudre", NkKey::NK_X, NK_SC_CTRL, NK_SCTX_EDIT);
	t.Bind("mesh.to_sphere", "To sphere", NkKey::NK_S, (uint8)(NK_SC_SHIFT | NK_SC_ALT), NK_SCTX_EDIT);
	t.Bind("mesh.shade_smooth", "Ombrage doux", NkKey::NK_S, NK_SC_SHIFT, NK_SCTX_EDIT);
	t.Bind("mesh.shade_flat", "Ombrage franc", NkKey::NK_F, NK_SC_SHIFT, NK_SCTX_EDIT);
	t.Bind("mesh.xray", "Voir a travers", NkKey::NK_Z, NK_SC_ALT, NK_SCTX_EDIT);
	// — mode OBJET
	t.Bind("object.translate", "Deplacer", NkKey::NK_G, NK_SC_NONE, NK_SCTX_OBJECT);
	t.Bind("object.rotate", "Tourner", NkKey::NK_R, NK_SC_NONE, NK_SCTX_OBJECT);
	t.Bind("object.scale", "Redimensionner", NkKey::NK_S, NK_SC_NONE, NK_SCTX_OBJECT);
	// — GLOBAL
	t.Bind("view.toggle_mode", "Objet / Edition", NkKey::NK_TAB, NK_SC_NONE, NK_SCTX_GLOBAL);
	t.Bind("view.toggle_snap", "Aimantation", NkKey::NK_TAB, NK_SC_SHIFT, NK_SCTX_GLOBAL);
	t.Bind("view.cycle_pivot", "Point de pivot", NkKey::NK_PERIOD, NK_SC_NONE, NK_SCTX_GLOBAL);
	t.Bind("view.cycle_orient", "Orientation", NkKey::NK_COMMA, NK_SC_NONE, NK_SCTX_GLOBAL);
	t.Bind("select.all", "Tout selectionner", NkKey::NK_A, NK_SC_NONE, NK_SCTX_GLOBAL);
	t.Bind("select.none", "Tout deselectionner", NkKey::NK_A, NK_SC_ALT, NK_SCTX_GLOBAL);
	// — pile de MODIFICATEURS
	t.Bind("mod.param_next", "Parametre suivant", NkKey::NK_BACKSLASH, NK_SC_SHIFT, NK_SCTX_GLOBAL);
	t.Bind("mod.move_up", "Monter", NkKey::NK_UP, NK_SC_SHIFT, NK_SCTX_GLOBAL);
	t.Bind("mod.move_down", "Descendre", NkKey::NK_DOWN, NK_SC_SHIFT, NK_SCTX_GLOBAL);
	t.Bind("mod.toggle", "Activer / desactiver", NkKey::NK_E, NK_SC_SHIFT, NK_SCTX_GLOBAL);
	t.Bind("mod.duplicate", "Dupliquer", NkKey::NK_D, NK_SC_SHIFT, NK_SCTX_GLOBAL);
	t.Bind("mod.remove", "Retirer", NkKey::NK_DELETE, NK_SC_SHIFT, NK_SCTX_GLOBAL);
	t.Bind("mod.apply", "Appliquer", NkKey::NK_ENTER, NK_SC_SHIFT, NK_SCTX_GLOBAL);

	// 1) LE MEME `R` selon le contexte : loop cut en edition (avec Ctrl), rotation
	//    en objet. C'est ce que la table doit savoir faire.
	const NkShortcutBinding *rObj = t.Lookup(NkKey::NK_R, NK_SC_NONE, NK_SCTX_OBJECT);
	const NkShortcutBinding *rEdit = t.Lookup(NkKey::NK_R, NK_SC_CTRL, NK_SCTX_EDIT);
	// 2) `E` : extruder en edition, mais Shift+E gere la pile — deux commandes
	//    distinctes sur la meme touche, separees par le modificateur.
	const NkShortcutBinding *e1 = t.Lookup(NkKey::NK_E, NK_SC_NONE, NK_SCTX_EDIT);
	const NkShortcutBinding *e2 = t.Lookup(NkKey::NK_E, NK_SC_SHIFT, NK_SCTX_EDIT);
	// 3) Combinaison non liee -> rien. Une table qui rendrait « quelque chose »
	//    ferait executer une commande au hasard.
	const NkShortcutBinding *none = t.Lookup(NkKey::NK_Q, NK_SC_NONE, NK_SCTX_OBJECT);
	if (gLineCount < 512) {
		snprintf(gLines[gLineCount], 256, "%-34s R/objet=%s CtrlR/edit=%s E/edit=%s ShiftE=%s Q=%s",
				 "raccourcis/contexte", rObj ? rObj->command : "-", rEdit ? rEdit->command : "-",
				 e1 ? e1->command : "-", e2 ? e2->command : "-", none ? none->command : "aucune");
		gLineCount++;
	}

	// 4) CONFLITS. Deux liaisons repondant a la meme combinaison dans des contextes
	//    qui se recoupent. Ici `Shift+E` (pile, GLOBAL) recouvre le mode edition, et
	//    `Shift+S` / `Shift+Alt+S` illustrent le voisinage qui m'avait pose probleme.
	char first[64] = {};
	uint32 ca = 0, cb = 0;
	if (t.ConflictAt(0, ca, cb)) {
		const NkShortcutBinding *a = t.At(ca);
		const NkShortcutBinding *b = t.At(cb);
		snprintf(first, sizeof(first), "%s vs %s", a ? a->command : "?", b ? b->command : "?");
	}
	if (gLineCount < 512) {
		snprintf(gLines[gLineCount], 256, "%-34s liaisons=%u conflits=%u premier=[%s]", "raccourcis/conflits",
				 t.Count(), t.ConflictCount(), first[0] ? first : "aucun");
		gLineCount++;
	}

	// 4bis) LE CAS QUI PROUVE LE DETECTEUR. La table ci-dessus n'a AUCUN conflit :
	//    resultat honnete, mais qui ne prouve rien — un detecteur casse afficherait
	//    zero lui aussi. On rejoue donc le conflit REEL rencontre le 31/07 : vouloir
	//    « Shift+S » pour activer un modificateur alors que Shift+S est deja
	//    l'ombrage doux en mode edition. Les contextes se recoupent (GLOBAL recouvre
	//    EDIT), c'est donc bien un conflit — et c'est exactement ce que j'avais
	//    decouvert a la compilation, par hasard, faute de table.
	{
		NkShortcutTable c;
		c.Bind("mesh.shade_smooth", "Ombrage doux", NkKey::NK_S, NK_SC_SHIFT, NK_SCTX_EDIT);
		c.Bind("mod.toggle", "Activer le modificateur", NkKey::NK_S, NK_SC_SHIFT, NK_SCTX_GLOBAL);
		// Et un NON-conflit tres proche : meme touche, meme modificateur, contextes
		// DISJOINTS. S'il etait compte, la table crierait au loup des qu'une touche
		// sert dans deux modes — ce qui est la norme chez Blender.
		c.Bind("select.link", "Selection liee", NkKey::NK_L, NK_SC_NONE, NK_SCTX_EDIT);
		c.Bind("object.link", "Lier a la scene", NkKey::NK_L, NK_SC_NONE, NK_SCTX_OBJECT);
		uint32 ia = 0, ib = 0;
		const bool got = c.ConflictAt(0, ia, ib);
		const NkShortcutBinding *ba = got ? c.At(ia) : nullptr;
		const NkShortcutBinding *bb = got ? c.At(ib) : nullptr;
		const NkShortcutBinding *win = c.Lookup(NkKey::NK_S, NK_SC_SHIFT, NK_SCTX_EDIT);
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s conflits=%u [%s vs %s] gagne-en-edition=%s",
					 "raccourcis/conflit-reel-shift-s", c.ConflictCount(), ba ? ba->command : "-",
					 bb ? bb->command : "-", win ? win->command : "-");
			gLineCount++;
		}
	}

	// 5) AFFICHAGE : le texte montre a l'utilisateur vient de la table, il ne peut
	//    donc plus mentir — c'etait le defaut du champ `shortcut` de
	//    NkEditorCommand, une chaine recopiee a la main.
	char f1[32] = {}, f2[32] = {}, f3[32] = {};
	t.FormatFor("mesh.bevel_vert", f1, sizeof(f1));
	t.FormatFor("mod.apply", f2, sizeof(f2));
	t.FormatFor("view.cycle_pivot", f3, sizeof(f3));
	if (gLineCount < 512) {
		snprintf(gLines[gLineCount], 256, "%-34s bevel_vert=%s apply=%s pivot=%s", "raccourcis/affichage", f1, f2, f3);
		gLineCount++;
	}

	// 6) RECONFIGURATION : deplacer une commande doit changer ce qui la declenche
	//    ET ce qui est affiche. Les deux viennent de la meme donnee, donc ils ne
	//    peuvent pas diverger — c'est tout l'interet.
	t.Rebind("mod.apply", NkKey::NK_ENTER, (uint8)(NK_SC_CTRL | NK_SC_SHIFT), NK_SCTX_GLOBAL);
	const NkShortcutBinding *before = t.Lookup(NkKey::NK_ENTER, NK_SC_SHIFT, NK_SCTX_GLOBAL);
	const NkShortcutBinding *after = t.Lookup(NkKey::NK_ENTER, (uint8)(NK_SC_CTRL | NK_SC_SHIFT), NK_SCTX_GLOBAL);
	char f4[32] = {};
	t.FormatFor("mod.apply", f4, sizeof(f4));
	if (gLineCount < 512) {
		snprintf(gLines[gLineCount], 256, "%-34s ancienne=%s nouvelle=%s affiche=%s", "raccourcis/reconfiguration",
				 before ? before->command : "aucune", after ? after->command : "aucune", f4);
		gLineCount++;
	}
}

// ── ANALYSE STRUCTURELLE ────────────────────────────────────────────────────
// La brique qui sert a la fois la RETOPOLOGIE, la REPARATION et la DONNEE
// D'APPRENTISSAGE. Les cas sont choisis pour qu'une mesure fausse se voie :
//   • le CUBE donne des chiffres connus d'avance (Euler = 2, genre 0, 8 poles) ;
//   • CATMULL-CLARK doit PRESERVER le nombre de sommets irreguliers — c'est une
//     propriete mathematique du schema, et une subdivision naive la casserait ;
//   • la SYMETRIE doit CHUTER quand on casse la forme, sinon « 1,0 partout » ne
//     prouverait rien ;
//   • la JONCTION EN T doit refuser de donner un genre : un genre calcule sur un
//     maillage non manifold serait un entier plausible et faux.
static void AnalysisBattery() {
	NkVector<NkVertex3D> cv, gv;
	NkVector<uint32> ci, gi;
	MakeCube(cv, ci);
	MakeGrid(4, gv, gi);
	auto line = [](const char *nm, const NkMeshStats &a) {
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256,
					 "%-34s S=%u A=%u F=%u euler=%d genre=%d quad=%.2f poles(3/5/6+)=%u/%u/%u reg=%u bord=%u",
					 nm, a.verts, a.edges, a.faces, a.euler, a.genus, (double)a.quadRatio, a.poles.valence3,
					 a.poles.valence5, a.poles.valence6plus, a.poles.regular, a.poles.boundary);
			gLineCount++;
		}
	};

	// 1) CUBE : tous les chiffres sont connus d'avance. Euler = 8-12+6 = 2, donc
	//    genre 0. Huit coins de valence 3 : un cube n'a QUE des poles.
	NkEditMesh cube;
	cube.BuildFromIndexed(cv.Data(), (uint32)cv.Size(), ci.Data(), (uint32)ci.Size(), true);
	{
		const NkMeshStats a = NkMeshAnalysis::Analyze(cube);
		line("analyse/cube", a);
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s brut=%u soude=%u ferme=%d manifold=%d isoles=%u degen=%u",
					 "analyse/cube-sante", a.rawVerts, a.verts, a.IsClosed() ? 1 : 0, a.IsManifold() ? 1 : 0,
					 a.looseVerts, a.degenerateFaces);
			gLineCount++;
		}
	}

	// 2) LE CAS QUI COMPTE — CATMULL-CLARK PRESERVE LES SOMMETS IRREGULIERS.
	//    Le cube a 8 coins de valence 3. Apres subdivision il en a TOUJOURS 8 :
	//    les points d'arete et de face naissent tous reguliers (valence 4). Une
	//    subdivision qui deplacerait ou multiplierait les poles serait fausse, et
	//    le comptage de sommets seul ne le montrerait pas.
	for (int32 lv = 1; lv <= 2; lv++) {
		NkEditMesh m = cube;
		m.SubdivideCatmullClark(lv);
		char nm[64];
		snprintf(nm, sizeof(nm), "analyse/catmull-n%d-poles-invariants", lv);
		line(nm, NkMeshAnalysis::Analyze(m));
	}

	// 3) GRILLE : maillage OUVERT. Le genre doit valoir -1 (refus) et non un
	//    entier calcule sur une formule qui ne s'applique pas.
	NkEditMesh grid;
	grid.BuildFromIndexed(gv.Data(), (uint32)gv.Size(), gi.Data(), (uint32)gi.Size(), true);
	line("analyse/grille-ouverte", NkMeshAnalysis::Analyze(grid));

	// 4) SYMETRIE. Un cube est symetrique sur les trois axes. On DEPLACE ensuite
	//    un seul coin : la symetrie doit chuter sur les axes concernes. Sans ce
	//    second cas, « 1,00 partout » ne prouverait rien du tout.
	{
		const NkMeshStats a = NkMeshAnalysis::Analyze(cube);
		NkEditMesh bent = cube;
		const NkVec3f target = bent.verts[0].pos;
		for (uint32 i = 0; i < bent.VertCount(); ++i)
			if ((bent.verts[i].pos - target).Len() < 1e-6f)
				bent.verts[i].pos = bent.verts[i].pos + NkVec3f{0.35f, 0.f, 0.f};
		const NkMeshStats b = NkMeshAnalysis::Analyze(bent);
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s cube x/y/z=%.2f/%.2f/%.2f  coin deplace=%.2f/%.2f/%.2f",
					 "analyse/symetrie", (double)a.symmetry.x, (double)a.symmetry.y, (double)a.symmetry.z,
					 (double)b.symmetry.x, (double)b.symmetry.y, (double)b.symmetry.z);
			gLineCount++;
		}
	}

	// 5) JONCTION EN T : non manifold. Le genre doit etre REFUSE.
	{
		NkVector<NkVertex3D> vt = cv;
		NkVector<uint32> it = ci;
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
		NkEditMesh m;
		m.BuildFromIndexed(vt.Data(), (uint32)vt.Size(), it.Data(), (uint32)it.Size(), true);
		const NkMeshStats an = NkMeshAnalysis::Analyze(m);
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s nonmanif=%u bordA=%u genre=%d (doit valoir -1)",
					 "analyse/jonction-T-refus-genre", an.nonManifoldEdges, an.boundaryEdges, an.genus);
			gLineCount++;
		}
	}

	// 6) DENSITE. Un cube a des aretes toutes egales : ecart relatif nul. Une
	//    grille etiree sur un axe doit montrer un ecart non nul — sinon la mesure
	//    ne mesure rien.
	{
		const NkMeshStats a = NkMeshAnalysis::Analyze(cube);
		NkEditMesh stretched = grid;
		for (uint32 i = 0; i < stretched.VertCount(); ++i)
			stretched.verts[i].pos.x *= 3.f;
		const NkMeshStats b = NkMeshAnalysis::Analyze(stretched);
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256,
					 "%-34s cube ecart=%.3f (min %.2f max %.2f) | grille etiree ecart=%.3f (min %.2f max %.2f)",
					 "analyse/densite", (double)a.edgeDeviation, (double)a.edgeMin, (double)a.edgeMax,
					 (double)b.edgeDeviation, (double)b.edgeMin, (double)b.edgeMax);
			gLineCount++;
		}
	}
}

// -- GRAPHE DE NOEUDS (NKGraph, couche 1) ------------------------------------
// Le coeur commun aux materiaux, au VFX, a la modelisation procedurale et au
// blueprint. Les cas sont choisis pour qu'une implantation FAUSSE echoue :
//   * les noeuds sont crees dans l'ordre INVERSE de l'ordre topologique, sinon
//     un tri qui renverrait simplement l'ordre d'insertion passerait aussi ;
//   * la conversion implicite est testee DANS LES DEUX SENS, parce qu'une table
//     symetrique par erreur laisserait passer la perte d'information ;
//   * le cycle est REFUSE a la connexion, et on verifie que le graphe reste
//     triable apres le refus -- un refus qui laisserait le lien en place ne se
//     verrait pas au compte de liens seul.
// Ecriture d'une ligne de rapport. Fonction STATIQUE et non lambda : une lambda
// a ellipse C n'est pas du C++ valide.
static void GraphPut(const char *fmt, ...) {
	if (gLineCount >= 512)
		return;
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(gLines[gLineCount], 256, fmt, ap);
	va_end(ap);
	gLineCount++;
}

static void GraphBattery() {
	using namespace nkentseu::graph;
	// 1) TRI TOPOLOGIQUE SUR UN LOSANGE, INSERE A L'ENVERS.
	//    Dependances : A -> B, A -> C, B -> D, C -> D.
	//    On cree D, C, B, A dans CET ordre. L'ordre d'insertion est donc
	//    exactement l'inverse du resultat attendu : si le tri renvoyait
	//    l'ordre de creation, A finirait dernier et le cas echouerait.
	{
		NkNodeGraph g;
		const NkTypeId T = g.RegisterType("flux");
		NkNodeId d = g.AddNode("sortie"), c = g.AddNode("droite");
		NkNodeId b = g.AddNode("gauche"), a = g.AddNode("source");
		const NkNodeId ids[4] = {a, b, c, d};
		const char *nom[4] = {"A", "B", "C", "D"};
		for (int32 i = 0; i < 4; i++) {
			g.AddSocket(ids[i], "in", T, NkSocketDir::Input);
			g.AddSocket(ids[i], "in2", T, NkSocketDir::Input);
			g.AddSocket(ids[i], "out", T, NkSocketDir::Output);
		}
		g.Connect(a, "out", b, "in");
		g.Connect(a, "out", c, "in");
		g.Connect(b, "out", d, "in");
		g.Connect(c, "out", d, "in2");
		NkVector<NkNodeId> order;
		const bool ok = g.TopoSort(order);
		char buf[96];
		int32 w = 0;
		for (uint32 i = 0; i < (uint32)order.Size() && w < 80; ++i) {
			const char *n = "?";
			for (int32 k = 0; k < 4; k++)
				if (ids[k] == order[i])
					n = nom[k];
			w += snprintf(buf + w, sizeof(buf) - (size_t)w, "%s%s", w ? ">" : "", n);
		}
		// Attendu : A en premier, D en dernier. Insertion faite en D,C,B,A.
		GraphPut("%-34s ok=%d ordre=%s (insere D,C,B,A) noeuds=%u liens=%u", "graphe/topo-losange-insere-inverse",
			ok ? 1 : 0, buf, g.NodeCount(), g.LinkCount());
	}

	// 2) CYCLE REFUSE A LA CONNEXION. A -> B -> C, puis C -> A.
	//    On verifie (a) le code d'erreur, (b) que le lien n'a PAS ete cree,
	//    (c) que le graphe reste triable. Un refus qui poserait quand meme le
	//    lien donnerait le meme code d'erreur mais casserait le tri.
	{
		NkNodeGraph g;
		const NkTypeId T = g.RegisterType("flux");
		NkNodeId a = g.AddNode("a"), b = g.AddNode("b"), c = g.AddNode("c");
		const NkNodeId ids[3] = {a, b, c};
		for (int32 i = 0; i < 3; i++) {
			g.AddSocket(ids[i], "in", T, NkSocketDir::Input);
			g.AddSocket(ids[i], "out", T, NkSocketDir::Output);
		}
		g.Connect(a, "out", b, "in");
		g.Connect(b, "out", c, "in");
		const NkLinkError e = g.Connect(c, "out", a, "in");
		NkVector<NkNodeId> order;
		const bool ok = g.TopoSort(order);
		GraphPut("%-34s refus=%s liens=%u (doit rester 2) triable=%d cycle=%d", "graphe/cycle-refuse",
			NkLinkErrorName(e), g.LinkCount(), ok ? 1 : 0, g.HasCycle() ? 1 : 0);
	}

	// 3) TYPES. La conversion declaree est DIRIGEE : reel -> vecteur autorise
	//    n'autorise PAS vecteur -> reel. Une table symetrique par erreur
	//    accepterait les deux, et le graphe calculerait faux sans rien dire.
	{
		NkNodeGraph g;
		const NkTypeId R = g.RegisterType("reel");
		const NkTypeId V = g.RegisterType("vecteur");
		const NkTypeId M = g.RegisterType("maillage");
		g.AllowConversion(R, V); // un reel alimente un vecteur, pas l'inverse
		NkNodeId s = g.AddNode("source"), p = g.AddNode("puits");
		g.AddSocket(s, "reel", R, NkSocketDir::Output);
		g.AddSocket(s, "vect", V, NkSocketDir::Output);
		g.AddSocket(p, "reel", R, NkSocketDir::Input);
		g.AddSocket(p, "vect", V, NkSocketDir::Input);
		g.AddSocket(p, "mail", M, NkSocketDir::Input);
		const NkLinkError e1 = g.Connect(s, "reel", p, "vect"); // conversion permise
		const NkLinkError e2 = g.Connect(s, "vect", p, "reel"); // sens interdit
		const NkLinkError e3 = g.Connect(s, "reel", p, "mail"); // aucun rapport
		GraphPut("%-34s reel>vect=%s | vect>reel=%s | reel>maillage=%s", "graphe/conversion-dirigee",
			NkLinkErrorName(e1), NkLinkErrorName(e2), NkLinkErrorName(e3));
	}

	// 4) UNE ENTREE, UNE SOURCE. Brancher une seconde source REMPLACE la
	//    premiere (Blender, Unreal). Le compte de liens doit rester a 1 ET la
	//    source doit etre la NOUVELLE -- un remplacement qui garderait
	//    l'ancienne donnerait le meme compte.
	{
		NkNodeGraph g;
		const NkTypeId T = g.RegisterType("flux");
		NkNodeId x = g.AddNode("x"), y = g.AddNode("y"), z = g.AddNode("z");
		g.AddSocket(x, "out", T, NkSocketDir::Output);
		g.AddSocket(y, "out", T, NkSocketDir::Output);
		g.AddSocket(z, "in", T, NkSocketDir::Input);
		g.Connect(x, "out", z, "in");
		g.Connect(y, "out", z, "in");
		const NkLink *in = g.IncomingOf(z, 0);
		GraphPut("%-34s liens=%u (doit valoir 1) source=%s", "graphe/entree-source-unique", g.LinkCount(),
			in ? (in->fromNode == y ? "y-la-nouvelle" : "x-l-ancienne-BUG") : "aucune-BUG");
	}

	// 5) SUPPRESSION. Le noeud du MILIEU d'une chaine emporte SES DEUX liens.
	//    Supprimer une extremite n'en emporterait qu'un : le milieu est le seul
	//    cas ou une implantation qui oublierait un sens se verrait.
	{
		NkNodeGraph g;
		const NkTypeId T = g.RegisterType("flux");
		NkNodeId a = g.AddNode("a"), b = g.AddNode("b"), c = g.AddNode("c");
		const NkNodeId ids[3] = {a, b, c};
		for (int32 i = 0; i < 3; i++) {
			g.AddSocket(ids[i], "in", T, NkSocketDir::Input);
			g.AddSocket(ids[i], "out", T, NkSocketDir::Output);
		}
		g.Connect(a, "out", b, "in");
		g.Connect(b, "out", c, "in");
		const uint32 before = g.LinkCount();
		g.RemoveNode(b);
		GraphPut("%-34s avant=%u apres=%u (doit valoir 0) noeuds=%u b-retrouve=%d", "graphe/suppression-milieu", before,
			g.LinkCount(), g.NodeCount(), g.Find(b) ? 1 : 0);
	}

	// 6) SENS ET SOCKETS. « ce socket n'existe pas » et « vous branchez une
	//    entree sur une entree » ne se corrigent pas de la meme facon : le coeur
	//    doit les DISTINGUER, sinon l'interface ne peut rien expliquer.
	{
		NkNodeGraph g;
		const NkTypeId T = g.RegisterType("flux");
		NkNodeId a = g.AddNode("a"), b = g.AddNode("b");
		g.AddSocket(a, "in", T, NkSocketDir::Input);
		g.AddSocket(a, "out", T, NkSocketDir::Output);
		g.AddSocket(b, "in", T, NkSocketDir::Input);
		g.AddSocket(b, "out", T, NkSocketDir::Output);
		const NkLinkError e1 = g.Connect(a, "in", b, "in");	   // sortie<-entree
		const NkLinkError e2 = g.Connect(a, "out", b, "out");  // entree<-sortie
		const NkLinkError e3 = g.Connect(a, "out", b, "truc"); // n'existe pas
		const NkLinkError e4 = g.Connect(a, "out", a, "in");   // soi-meme
		GraphPut("%-34s entree-src=%s sortie-dst=%s inconnu=%s boucle-soi=%s", "graphe/sens-et-sockets",
			NkLinkErrorName(e1), NkLinkErrorName(e2), NkLinkErrorName(e3), NkLinkErrorName(e4));
	}

	// 7) IDENTIFIANTS STABLES. Apres suppression, un nouveau noeud ne doit PAS
	//    reprendre l'identifiant libere : une sauvegarde ou une courbe
	//    d'animation qui designerait l'ancien pointerait silencieusement sur le
	//    nouveau. Meme regle que pour les modificateurs.
	{
		NkNodeGraph g;
		NkNodeId a = g.AddNode("a"), b = g.AddNode("b");
		g.RemoveNode(a);
		NkNodeId c = g.AddNode("c");
		GraphPut("%-34s a=%u b=%u c=%u (c doit differer de a) recycle=%d", "graphe/identifiants-stables", a, b, c,
			(c == a) ? 1 : 0);
	}
}

// -- GRAPHE : SERIALISATION ET ANNULER/REFAIRE -------------------------------
// Deux mecanismes dont TOUS les consommateurs dependent (materiaux, VFX,
// blueprint, modelisation). Les cas visent les deux pieges connus :
//   * un aller-retour qui « marche » mais REATTRIBUE les identifiants liberes ;
//   * une annulation qui ressuscite le noeud mais PAS ses liens.
// Les deux produisent un resultat plausible et faux.
static void GraphIOBattery() {
	using namespace nkentseu::graph;

	// Graphe de reference, construit avec tout ce qui peut se perdre en route :
	// des types, une conversion dirigee, des libelles a espaces et accents, et
	// UN TROU dans les identifiants (un noeud supprime).
	auto build = [](NkNodeGraph &g) {
		const NkTypeId R = g.RegisterType("reel");
		const NkTypeId V = g.RegisterType("vecteur");
		g.AllowConversion(R, V);
		NkNodeId a = g.AddNode("bruit.perlin", "Bruit de Perlin");
		NkNodeId jete = g.AddNode("temporaire", "a jeter");
		NkNodeId b = g.AddNode("deplacer", "Deplacement de surface");
		g.AddSocket(a, "sortie", R, NkSocketDir::Output);
		g.AddSocket(jete, "sortie", R, NkSocketDir::Output);
		g.AddSocket(b, "quantite", V, NkSocketDir::Input);
		g.AddSocket(b, "resultat", V, NkSocketDir::Output);
		g.Connect(a, "sortie", b, "quantite"); // passe par la conversion
		g.RemoveNode(jete);					   // <- le trou dans les identifiants
		return jete;
	};

	// 1) ALLER-RETOUR. On serialise, on relit dans un graphe NEUF, on reserialise :
	//    les deux textes doivent etre identiques caractere pour caractere. Comparer
	//    des comptes ne prouverait rien -- des libelles ou des conversions perdus
	//    laisseraient les comptes intacts.
	{
		NkNodeGraph g;
		build(g);
		NkString t1;
		g.Serialize(t1);
		NkNodeGraph relu;
		const bool ok = relu.Deserialize(t1.CStr());
		NkString t2;
		relu.Serialize(t2);
		bool identique = (t1.Size() == t2.Size());
		if (identique)
			for (uint32 i = 0; i < (uint32)t1.Size(); ++i)
				if (t1.CStr()[i] != t2.CStr()[i]) {
					identique = false;
					break;
				}
		GraphPut("%-34s lu=%d identique=%d octets=%u noeuds=%u liens=%u", "graphe/io-aller-retour", ok ? 1 : 0,
				 identique ? 1 : 0, (uint32)t1.Size(), relu.NodeCount(), relu.LinkCount());
	}

	// 2) LE PIEGE. Apres rechargement, un nouveau noeud ne doit PAS recuperer
	//    l'identifiant du noeud supprime. Sans la ligne `compteurs` du fichier,
	//    un aller-retour par ailleurs correct recyclerait cet identifiant, et une
	//    reference sauvegardee ailleurs pointerait en silence sur autre chose.
	{
		NkNodeGraph g;
		const NkNodeId jete = build(g);
		NkString t;
		g.Serialize(t);
		NkNodeGraph relu;
		relu.Deserialize(t.CStr());
		const NkNodeId neuf = relu.AddNode("apres.rechargement");
		GraphPut("%-34s supprime=%u nouveau-apres-relecture=%u recycle=%d", "graphe/io-identifiants-non-recycles",
				 jete, neuf, (neuf == jete) ? 1 : 0);
	}

	// 3) LA SEMANTIQUE SURVIT, pas seulement les donnees. Le graphe recharge doit
	//    encore ACCEPTER ce que la conversion permet et REFUSER le reste : c'est
	//    ce qui prouve que les lignes `conv` et les types ont ete relus, et pas
	//    seulement reecrits a l'identique.
	{
		NkNodeGraph g;
		build(g);
		NkString t;
		g.Serialize(t);
		NkNodeGraph relu;
		relu.Deserialize(t.CStr());
		const NkTypeId R = relu.FindType("reel");
		const NkTypeId V = relu.FindType("vecteur");
		const NkTypeId M = relu.RegisterType("maillage");
		NkNodeId src = relu.AddNode("src"), dst = relu.AddNode("dst");
		relu.AddSocket(src, "r", R, NkSocketDir::Output);
		relu.AddSocket(dst, "v", V, NkSocketDir::Input);
		relu.AddSocket(dst, "m", M, NkSocketDir::Input);
		const NkLinkError e1 = relu.Connect(src, "r", dst, "v"); // conversion relue
		const NkLinkError e2 = relu.Connect(src, "r", dst, "m"); // sans rapport
		GraphPut("%-34s types-retrouves=%d/%d reel>vect=%s reel>maillage=%s", "graphe/io-semantique-survit",
				 R ? 1 : 0, V ? 1 : 0, NkLinkErrorName(e1), NkLinkErrorName(e2));
	}

	// 4) ANNULATION D'UNE SUPPRESSION AU MILIEU. Le noeud du milieu emporte DEUX
	//    liens. Apres annulation, le noeud doit revenir ET les deux liens aussi,
	//    AVEC LEURS IDENTIFIANTS D'ORIGINE. Une annulation qui ne ressusciterait
	//    que le noeud laisserait un graphe coupe en deux, d'apparence saine.
	{
		NkNodeGraph g;
		const NkTypeId T = g.RegisterType("flux");
		NkNodeId a = g.AddNode("a"), b = g.AddNode("b"), c = g.AddNode("c");
		const NkNodeId ids[3] = {a, b, c};
		for (int32 i = 0; i < 3; i++) {
			g.AddSocket(ids[i], "in", T, NkSocketDir::Input);
			g.AddSocket(ids[i], "out", T, NkSocketDir::Output);
		}
		NkLinkId l1 = 0, l2 = 0;
		g.Connect(a, "out", b, "in", &l1);
		g.Connect(b, "out", c, "in", &l2);

		NkGraphHistory h;
		h.Reset(g);
		g.RemoveNode(b);
		h.Commit(g);
		const uint32 apresSuppr = g.LinkCount();
		const bool undo = h.Undo(g);
		// On PRELEVE l'etat ici, avant le refaire : mesurer apres donnerait l'etat
		// d'apres, et la ligne annoncerait autre chose que ce qu'elle mesure.
		const uint32 noeudsApresAnnule = g.NodeCount();
		const uint32 liensApresAnnule = g.LinkCount();
		const NkLink *r0 = g.LinkAt(0);
		const NkLink *r1 = g.LinkAt(1);
		const bool memesIds = r0 && r1 && ((r0->id == l1 && r1->id == l2) || (r0->id == l2 && r1->id == l1));
		const bool bRevenu = g.Find(b) != nullptr;
		const bool redo = h.Redo(g);
		GraphPut("%-34s apres-suppr=%u | annule=%d b-revenu=%d noeuds=%u liens=%u memes-ids=%d | refait liens=%u",
				 "graphe/undo-restaure-les-liens", apresSuppr, undo ? 1 : 0, bRevenu ? 1 : 0, noeudsApresAnnule,
				 liensApresAnnule, memesIds ? 1 : 0, redo ? g.LinkCount() : 999u);
	}

	// 5) LA BRANCHE REFAISABLE EST ABANDONNEE quand on modifie apres avoir
	//    annule -- comportement de tous les editeurs. On verifie aussi qu'une
	//    remontee complete redonne EXACTEMENT l'etat initial, texte compris :
	//    comparer des comptes laisserait passer une derive de position ou de
	//    libelle.
	{
		NkNodeGraph g;
		NkGraphHistory h;
		g.AddNode("depart");
		h.Reset(g);
		NkString avant;
		g.Serialize(avant);
		g.AddNode("un");
		h.Commit(g);
		g.AddNode("deux");
		h.Commit(g);
		const uint32 profAvant = h.UndoDepth();
		h.Undo(g);
		h.Undo(g);
		NkString apres;
		g.Serialize(apres);
		bool retourExact = (avant.Size() == apres.Size());
		if (retourExact)
			for (uint32 i = 0; i < (uint32)avant.Size(); ++i)
				if (avant.CStr()[i] != apres.CStr()[i]) {
					retourExact = false;
					break;
				}
		const uint32 refaisable = h.RedoDepth();
		g.AddNode("nouvelle-branche"); // abandonne la branche refaisable
		h.Commit(g);
		GraphPut("%-34s prof=%u retour-exact=%d refaisable-avant=%u apres-nouvelle-branche=%u",
				 "graphe/undo-branche-abandonnee", profAvant, retourExact ? 1 : 0, refaisable, h.RedoDepth());
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
	ModStackBattery();
	ModsBattery();
	ShortcutBattery();
	AnalysisBattery();
	GraphBattery();
	GraphIOBattery();

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
