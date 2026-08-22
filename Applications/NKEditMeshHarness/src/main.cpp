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
#include "NKGraph/NkGraphDocument.h"
#include "NKGraph/NkNodeGraph.h"
#include "NKRenderer/Mesh/NkMeshRetopo.h"
#include "NKRenderer/Mesh/NkMeshDecimate.h"
#include "NKRenderer/Mesh/NkMeshAnalysis.h"
#include "NKRenderer/Mesh/NkPLYLoader.h"
#include "NKRenderer/Mesh/NkSTLLoader.h"
#include "NKRenderer/Mesh/NkDAELoader.h"
#include "NKRenderer/Mesh/NkUSDALoader.h"
#include "NKRenderer/Core/NkGizmo.h"
#include "NKEditorKit/NkTheme.h"
#include "NKEditorKit/NkShortcutTable.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKContainers/String/NkFormat.h" // formatage TYPE des lignes de mesure
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
// ⚠️ VERIFICATION DU FORMAT PAR LE COMPILATEUR (ajoutee le 2026-08-22).
// Sans cet attribut, un format qui reclame plus de valeurs qu'on ne lui en passe
// ne casse RIEN de visible : il lit de la memoire indeterminee et affiche un
// chiffre PLAUSIBLE. Constate ici meme -- `ops/coupe-par-plan` annoncait
// « nonmanif=18 » sur un cube parfaitement manifold, parce que le format
// demandait sept %u et n'en recevait que six. Le chiffre etait credible, et il
// accusait BisectByPlane a tort.
// L'attribut fait verifier CHAQUE appel a la compilation : un desalignement
// devient un avertissement, plus un faux resultat.
#if defined(__GNUC__) || defined(__clang__)
// ⚠️ ERREUR, PAS AVERTISSEMENT, ET SEULEMENT DANS CE FICHIER. Un avertissement
// se noie dans la sortie d'un build de 25 projets ; or un banc qui affiche un
// chiffre faux est PIRE qu'un banc absent -- on lui fait confiance. Ici, un
// format desaligne doit arreter la compilation. La portee reste locale : aucun
// autre projet du depot n'est affecte.
#pragma GCC diagnostic error "-Wformat"
#define NK_FMT_CHECK(a, b) __attribute__((format(printf, a, b)))
#else
#define NK_FMT_CHECK(a, b)
#endif

static void GraphPut(const char *fmt, ...) NK_FMT_CHECK(1, 2);

// ── FORMATAGE TYPE : `Put` REMPLACE `GraphPut` DANS LES BATTERIES MESUREES ──
// Consigne de Rodolf (2026-08-22) : pas de formatage variadique dans du code qui
// produit une MESURE. `Infof` ne corrige rien -- meme mecanique, meme capacite a
// fabriquer un nombre credible. C'est ce qui a produit ici « nonmanif=18 » sur un
// cube parfaitement manifold : sept `%u` pour six arguments, le septieme lu dans
// la pile.
//
// `NkFormat` capture chaque argument PAR SON TYPE : il n'y a plus de
// reinterpretation possible, donc plus de nombre invente. Un argument absent
// laisse un TROU VISIBLE au lieu d'un entier plausible.
//
// ⚠️ La sortie doit rester IDENTIQUE AU CARACTERE PRES : le harnais compare ses
// lignes a une reference versionnee. Correspondance des specificateurs :
//   %-34s -> {n:<34}   %u/%d -> {n}   %.4f -> {n:.4f}   %.1f -> {n:.1f}
// ⚠️ COMPORTEMENT MESURE SUR ARGUMENT MANQUANT (2026-08-22) : `NkFormat` ne
// laisse PAS un trou -- il LEVE et arrete le banc, sur
// « nkformat: index d'argument hors limites ». C'est plus fort qu'un trou : un
// banc qui meurt ne peut pas mentir, alors qu'un trou peut se lire de travers.
// Mesure faite en injectant reellement un appel a court d'arguments, pas deduite
// du code.
// ⚠️ Effet de bord a connaitre : l'arret intervient EN COURS de course, apres que
// les lignes precedentes ont ete produites. Une sortie tronquee est donc le
// symptome d'un format fautif, pas d'un plantage du moteur.
template <typename... A> static void Put(const char *fmt, const A &...a) {
	if (gLineCount >= 512)
		return;
	const NkString ligne = NkFormat(NkStringView(fmt), a...);
	const char *src = ligne.Data();
	uint32 i = 0;
	for (; src && src[i] && i < 255u; ++i)
		gLines[gLineCount][i] = src[i];
	gLines[gLineCount][i] = 0;
	gLineCount++;
}

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

// -- GRAPHE : SOUS-GRAPHES ET PLAN APLATI ------------------------------------
// Un sous-graphe n'est pas « un graphe range dans un noeud » : c'est une brique
// NOMMEE, definie une fois et INSTANCIEE plusieurs fois -- corriger le groupe
// une fois doit corriger les cinq instances. Les cas visent ce qui distingue une
// vraie implantation d'une approximation :
//   * apres aplatissement, les noeuds d'instance et de frontiere ont DISPARU, et
//     les entrees pointent sur les etapes REELLES, a travers les frontieres ;
//   * deux instances du meme groupe donnent DEUX calculs, pas un partage ;
//   * une entree libre vaut NK_EVAL_NO_SOURCE et surtout PAS 0 -- 0 est un index
//     d'etape valide, et cette confusion produit un resultat plausible.
static void GraphDocBattery() {
	using namespace nkentseu::graph;

	// Construit le groupe « double » : entree -> A -> B -> sortie.
	auto buildGroup = [](NkNodeGraph &g) {
		const NkTypeId T = g.RegisterType("flux");
		NkNodeId gin = g.AddNode(NK_NODE_GROUP_IN, "entree");
		NkNodeId a = g.AddNode("calc.a", "A");
		NkNodeId b = g.AddNode("calc.b", "B");
		NkNodeId gout = g.AddNode(NK_NODE_GROUP_OUT, "sortie");
		g.AddSocket(gin, "e", T, NkSocketDir::Output);
		g.AddSocket(a, "in", T, NkSocketDir::Input);
		g.AddSocket(a, "out", T, NkSocketDir::Output);
		g.AddSocket(b, "in", T, NkSocketDir::Input);
		g.AddSocket(b, "out", T, NkSocketDir::Output);
		g.AddSocket(gout, "s", T, NkSocketDir::Input);
		g.Connect(gin, "e", a, "in");
		g.Connect(a, "out", b, "in");
		g.Connect(b, "out", gout, "s");
	};

	// Rend « src>A>B>dst » a partir du plan : ce sont les LIBELLES des etapes,
	// donc ce qui subsiste reellement apres aplatissement.
	auto trace = [](const NkGraphDocument &doc, const NkEvalPlan &plan, char *buf, uint32 cap) {
		uint32 w = 0;
		buf[0] = 0;
		for (uint32 i = 0; i < plan.Size() && w + 1 < cap; ++i) {
			const NkEvalStep &st = plan.steps[i];
			const NkNode *n = doc.GraphAt(st.graph).Find(st.node);
			const int32 k = snprintf(buf + w, (size_t)(cap - w), "%s%s", w ? ">" : "", n ? n->label.CStr() : "?");
			if (k <= 0)
				break;
			w += (uint32)k;
		}
	};

	// 1) L'APLATISSEMENT ELIMINE LA FRONTIERE. Le plan doit contenir src, A, B,
	//    dst -- ni le noeud d'instance, ni les deux noeuds de frontiere. Et
	//    surtout : l'entree de `dst` doit pointer sur l'etape de B, qui se trouve
	//    de l'autre cote d'une frontiere. Compter les etapes ne suffirait pas ;
	//    c'est ce raccordement qui prouve l'aplatissement.
	{
		NkGraphDocument doc;
		const uint32 gi = doc.AddGraph("double");
		buildGroup(doc.GraphAt(gi));
		const uint32 ri = doc.AddGraph("racine");
		doc.SetRoot(ri);
		NkNodeGraph &r = doc.GraphAt(ri);
		const NkTypeId T = r.RegisterType("flux");
		NkNodeId src = r.AddNode("source", "src");
		NkNodeId inst = r.AddNode(NK_NODE_INSTANCE, "inst");
		NkNodeId dst = r.AddNode("puits", "dst");
		r.Find(inst)->subgraph = NkString("double");
		r.AddSocket(src, "out", T, NkSocketDir::Output);
		r.AddSocket(inst, "e", T, NkSocketDir::Input);
		r.AddSocket(inst, "s", T, NkSocketDir::Output);
		r.AddSocket(dst, "in", T, NkSocketDir::Input);
		r.Connect(src, "out", inst, "e");
		r.Connect(inst, "s", dst, "in");

		NkEvalPlan plan;
		const NkPlanError e = doc.BuildPlan(plan);
		char t[128];
		trace(doc, plan, t, sizeof(t));
		// L'etape de `dst` est la derniere ; sa source doit etre l'etape de B.
		const char *srcDeDst = "?";
		if (plan.Size() > 0) {
			const NkEvalStep &last = plan.steps[plan.Size() - 1];
			if (last.inputs.Size() > 0 && last.inputs[0].srcStep != NK_EVAL_NO_SOURCE
				&& last.inputs[0].srcStep < plan.Size()) {
				const NkEvalStep &p = plan.steps[last.inputs[0].srcStep];
				const NkNode *n = doc.GraphAt(p.graph).Find(p.node);
				if (n)
					srcDeDst = n->label.CStr();
			}
		}
		GraphPut("%-34s %s etapes=%u [%s] dst<-%s (doit etre B)", "graphe/aplati-frontiere-disparait",
				 NkPlanErrorName(e), plan.Size(), t, srcDeDst);
	}

	// 2) LE CAS QUI TRANCHE -- DEUX INSTANCES DU MEME GROUPE.
	//    Une implantation qui memoriserait le resultat par GRAPHE n'emettrait le
	//    groupe qu'UNE fois, et les deux branches liraient la meme valeur. Il faut
	//    donc voir A et B DEUX fois, et les deux puits doivent pointer sur des
	//    etapes DIFFERENTES.
	{
		NkGraphDocument doc;
		const uint32 gi = doc.AddGraph("double");
		buildGroup(doc.GraphAt(gi));
		const uint32 ri = doc.AddGraph("racine");
		doc.SetRoot(ri);
		NkNodeGraph &r = doc.GraphAt(ri);
		const NkTypeId T = r.RegisterType("flux");
		NkNodeId d1 = 0, d2 = 0;
		for (int32 k = 0; k < 2; k++) {
			char sn[16], in[16], dn[16];
			snprintf(sn, sizeof(sn), "src%d", k + 1);
			snprintf(in, sizeof(in), "i%d", k + 1);
			snprintf(dn, sizeof(dn), "dst%d", k + 1);
			NkNodeId src = r.AddNode("source", sn);
			NkNodeId inst = r.AddNode(NK_NODE_INSTANCE, in);
			NkNodeId dst = r.AddNode("puits", dn);
			r.Find(inst)->subgraph = NkString("double");
			r.AddSocket(src, "out", T, NkSocketDir::Output);
			r.AddSocket(inst, "e", T, NkSocketDir::Input);
			r.AddSocket(inst, "s", T, NkSocketDir::Output);
			r.AddSocket(dst, "in", T, NkSocketDir::Input);
			r.Connect(src, "out", inst, "e");
			r.Connect(inst, "s", dst, "in");
			(k == 0 ? d1 : d2) = dst;
		}
		NkEvalPlan plan;
		const NkPlanError e = doc.BuildPlan(plan);
		uint32 nbA = 0, s1 = NK_EVAL_NO_SOURCE, s2 = NK_EVAL_NO_SOURCE;
		for (uint32 i = 0; i < plan.Size(); ++i) {
			const NkNode *n = doc.GraphAt(plan.steps[i].graph).Find(plan.steps[i].node);
			if (!n)
				continue;
			if (strcmp(n->label.CStr(), "A") == 0)
				nbA++;
			if (n->id == d1 && plan.steps[i].inputs.Size())
				s1 = plan.steps[i].inputs[0].srcStep;
			if (n->id == d2 && plan.steps[i].inputs.Size())
				s2 = plan.steps[i].inputs[0].srcStep;
		}
		GraphPut("%-34s %s etapes=%u A-emis=%u (doit valoir 2) dst1<-%u dst2<-%u distincts=%d",
				 "graphe/aplati-deux-instances", NkPlanErrorName(e), plan.Size(), nbA, s1, s2,
				 (s1 != s2 && s1 != NK_EVAL_NO_SOURCE) ? 1 : 0);
	}

	// 3) RECURSION REFUSEE, directe ET indirecte. L'indirecte est le vrai cas :
	//    une garde qui ne comparerait qu'au graphe courant laisserait passer
	//    G -> H -> G et ferait deborder la pile.
	{
		NkGraphDocument dd;
		const uint32 g = dd.AddGraph("G");
		dd.SetRoot(g);
		{
			NkNodeGraph &x = dd.GraphAt(g);
			NkNodeId i = x.AddNode(NK_NODE_INSTANCE, "moi-meme");
			x.Find(i)->subgraph = NkString("G");
		}
		NkEvalPlan p1;
		const NkPlanError e1 = dd.BuildPlan(p1);

		NkGraphDocument di;
		const uint32 a = di.AddGraph("G"), b = di.AddGraph("H");
		di.SetRoot(a);
		{
			NkNodeId i = di.GraphAt(a).AddNode(NK_NODE_INSTANCE, "vers-H");
			di.GraphAt(a).Find(i)->subgraph = NkString("H");
			NkNodeId j = di.GraphAt(b).AddNode(NK_NODE_INSTANCE, "retour-G");
			di.GraphAt(b).Find(j)->subgraph = NkString("G");
		}
		NkEvalPlan p2;
		const NkPlanError e2 = di.BuildPlan(p2);

		NkGraphDocument du;
		const uint32 u = du.AddGraph("racine");
		du.SetRoot(u);
		{
			NkNodeId i = du.GraphAt(u).AddNode(NK_NODE_INSTANCE, "fantome");
			du.GraphAt(u).Find(i)->subgraph = NkString("nexiste.pas");
		}
		NkEvalPlan p3;
		const NkPlanError e3 = du.BuildPlan(p3);

		GraphPut("%-34s directe=%s indirecte=%s inconnu=%s", "graphe/aplati-recursion-refusee",
				 NkPlanErrorName(e1), NkPlanErrorName(e2), NkPlanErrorName(e3));
	}

	// 4) LA SENTINELLE. Une entree LIBRE doit valoir NK_EVAL_NO_SOURCE et non 0 :
	//    0 est l'index de la PREMIERE etape, donc une sentinelle a zero ferait
	//    lire sa sortie a chaque entree non branchee -- resultat plausible, faux,
	//    et invisible au comptage.
	{
		NkGraphDocument doc;
		const uint32 ri = doc.AddGraph("racine");
		doc.SetRoot(ri);
		NkNodeGraph &r = doc.GraphAt(ri);
		const NkTypeId T = r.RegisterType("flux");
		NkNodeId premier = r.AddNode("premier", "P");
		NkNodeId libre = r.AddNode("libre", "L");
		r.AddSocket(premier, "out", T, NkSocketDir::Output);
		r.AddSocket(libre, "in", T, NkSocketDir::Input);
		// AUCUNE connexion : l'entree de L reste libre.
		NkEvalPlan plan;
		const NkPlanError e = doc.BuildPlan(plan);
		uint32 srcLibre = 12345u;
		for (uint32 i = 0; i < plan.Size(); ++i)
			if (plan.steps[i].node == libre && plan.steps[i].inputs.Size())
				srcLibre = plan.steps[i].inputs[0].srcStep;
		GraphPut("%-34s %s etapes=%u entree-libre=%s (0 serait un BUG)", "graphe/aplati-entree-libre",
				 NkPlanErrorName(e), plan.Size(),
				 srcLibre == NK_EVAL_NO_SOURCE ? "sans-source" : (srcLibre == 0 ? "ETAPE-0-BUG" : "autre-BUG"));
	}

	// 5) ALLER-RETOUR DU DOCUMENT. Le champ `subgraph` est ce qui se perd le plus
	//    facilement : un document relu sans lui donnerait des instances vides,
	//    donc un plan reduit au graphe racine. On compare les TEXTES, puis on
	//    verifie que le plan RECONSTRUIT est identique -- une donnee peut survivre
	//    a l'ecriture et ne plus rien piloter.
	{
		NkGraphDocument doc;
		const uint32 gi = doc.AddGraph("double");
		buildGroup(doc.GraphAt(gi));
		const uint32 ri = doc.AddGraph("racine");
		doc.SetRoot(ri);
		NkNodeGraph &r = doc.GraphAt(ri);
		const NkTypeId T = r.RegisterType("flux");
		NkNodeId src = r.AddNode("source", "src");
		NkNodeId inst = r.AddNode(NK_NODE_INSTANCE, "inst");
		NkNodeId dst = r.AddNode("puits", "dst");
		r.Find(inst)->subgraph = NkString("double");
		r.AddSocket(src, "out", T, NkSocketDir::Output);
		r.AddSocket(inst, "e", T, NkSocketDir::Input);
		r.AddSocket(inst, "s", T, NkSocketDir::Output);
		r.AddSocket(dst, "in", T, NkSocketDir::Input);
		r.Connect(src, "out", inst, "e");
		r.Connect(inst, "s", dst, "in");

		NkString t1;
		doc.Serialize(t1);
		NkGraphDocument relu;
		const bool lu = relu.Deserialize(t1.CStr());
		NkString t2;
		relu.Serialize(t2);
		bool identique = (t1.Size() == t2.Size());
		if (identique)
			for (uint32 i = 0; i < (uint32)t1.Size(); ++i)
				if (t1.CStr()[i] != t2.CStr()[i]) {
					identique = false;
					break;
				}
		NkEvalPlan pa, pb;
		doc.BuildPlan(pa);
		const NkPlanError eb = relu.BuildPlan(pb);
		char ta[128], tb[128];
		trace(doc, pa, ta, sizeof(ta));
		trace(relu, pb, tb, sizeof(tb));
		bool memeTrace = true;
		for (uint32 i = 0; ta[i] || tb[i]; ++i)
			if (ta[i] != tb[i]) {
				memeTrace = false;
				break;
			}
		GraphPut("%-34s lu=%d texte-identique=%d graphes=%u plan=%s [%s] meme-plan=%d",
				 "graphe/doc-aller-retour", lu ? 1 : 0, identique ? 1 : 0, relu.GraphCount(),
				 NkPlanErrorName(eb), tb, memeTrace ? 1 : 0);
	}
}

// -- GRAPHE : INTERFACE D'UN GROUPE INSTANCIE --------------------------------
// Le defaut se produit des qu'un groupe est MODIFIE APRES avoir ete instancie.
// Sans controle, une entree se retrouve silencieusement debranchee et le calcul
// continue avec une valeur manquante.
//
// LE CAS QUI TRANCHE est le troisieme : les deux graphes attribuent le MEME
// NUMERO a des types de NOMS DIFFERENTS. Chaque graphe tient son propre
// registre, donc un controle par identifiant les croirait d'accord. Seul un
// controle par NOM refuse. C'est pour cela qu'un registre partage n'aurait pas
// suffi : il aurait rendu la comparaison moins couteuse, sans rien refuser.
static void GraphIfaceBattery() {
	using namespace nkentseu::graph;

	// Groupe « G » : interface = entree « e » (flux) -> sortie « s » (flux).
	auto buildG = [](NkNodeGraph &g) {
		const NkTypeId T = g.RegisterType("flux");
		NkNodeId gin = g.AddNode(NK_NODE_GROUP_IN, "entree");
		NkNodeId mid = g.AddNode("calc", "M");
		NkNodeId gout = g.AddNode(NK_NODE_GROUP_OUT, "sortie");
		g.AddSocket(gin, "e", T, NkSocketDir::Output);
		g.AddSocket(mid, "in", T, NkSocketDir::Input);
		g.AddSocket(mid, "out", T, NkSocketDir::Output);
		g.AddSocket(gout, "s", T, NkSocketDir::Input);
		g.Connect(gin, "e", mid, "in");
		g.Connect(mid, "out", gout, "s");
	};

	// Fabrique un document dont la racine instancie G, avec l'interface DEMANDEE
	// sur le noeud d'instance. `premierType` est enregistre EN PREMIER dans la
	// racine : il recoit donc l'identifiant 1, comme « flux » dans le groupe.
	auto essai = [&](const char *premierType, const char *nomEntree, const char *typeEntree, bool socketEnTrop,
					 NkString &detailOut) {
		NkGraphDocument doc;
		buildG(doc.GraphAt(doc.AddGraph("G")));
		const uint32 ri = doc.AddGraph("racine");
		doc.SetRoot(ri);
		NkNodeGraph &r = doc.GraphAt(ri);
		const NkTypeId premier = r.RegisterType(premierType);
		const NkTypeId tEntree = r.RegisterType(typeEntree);
		const NkTypeId tFlux = r.RegisterType("flux");
		(void)premier;
		NkNodeId inst = r.AddNode(NK_NODE_INSTANCE, "inst");
		r.Find(inst)->subgraph = NkString("G");
		r.AddSocket(inst, nomEntree, tEntree, NkSocketDir::Input);
		r.AddSocket(inst, "s", tFlux, NkSocketDir::Output);
		if (socketEnTrop)
			r.AddSocket(inst, "oublie", tFlux, NkSocketDir::Input);
		NkEvalPlan plan;
		const NkPlanError e = doc.BuildPlan(plan);
		detailOut = plan.errorDetail;
		return e;
	};

	NkString d1, d2, d3, d4;
	// 1) conforme : meme nom, meme type. Sans ce cas, un controle qui refuserait
	//    TOUT passerait les trois autres et ne prouverait rien.
	const NkPlanError e1 = essai("flux", "e", "flux", false, d1);
	// 2) nom absent : le groupe attend « e », l'instance offre « entree ».
	const NkPlanError e2 = essai("flux", "entree", "flux", false, d2);
	// 3) LE CAS QUI TRANCHE : « maillage » est enregistre EN PREMIER dans la
	//    racine, il porte donc l'identifiant 1 -- exactement celui de « flux »
	//    dans le groupe. Les numeros coincident, les noms non.
	const NkPlanError e3 = essai("maillage", "e", "maillage", false, d3);
	// 4) socket EN TROP : un fil branche sur rien.
	const NkPlanError e4 = essai("flux", "e", "flux", true, d4);

	GraphPut("%-34s conforme=%s | nom-absent=%s | MEME-ID-AUTRE-NOM=%s | en-trop=%s",
			 "graphe/interface-groupe", NkPlanErrorName(e1), NkPlanErrorName(e2), NkPlanErrorName(e3),
			 NkPlanErrorName(e4));
	// Le message doit NOMMER le socket fautif : un code d'erreur seul obligerait
	// a chercher dans un document qui peut compter des dizaines de groupes.
	GraphPut("%-34s [%s] [%s] [%s]", "graphe/interface-messages", d2.CStr(), d3.CStr(), d4.CStr());
}

// -- DECIMATION QEM ----------------------------------------------------------
// Premiere passe de RETOPOLOGIE : alleger EN GARDANT LA FORME. La decimation par
// clustering deja presente dans NKGen moyenne les sommets par cellule de grille,
// donc elle arrondit les aretes vives ; QEM mesure de combien la surface
// s'ecarterait des plans d'origine et retire d'abord ce qui ne porte pas la
// forme.
//
// Les cas sont batis pour qu'une implantation FAUSSE echoue :
//   * sur une surface PLANE, l'erreur QEM d'une contraction interieure est
//     exactement nulle -- l'erreur de forme doit donc rester ~0 meme apres une
//     decimation massive. Une implantation approximative deformerait ;
//   * le drapeau de BORD est teste dans les DEUX positions sur le meme maillage :
//     sans lui la boite englobante doit RETRECIR. Un seul essai ne prouverait
//     rien, puisqu'une bbox intacte peut simplement signifier qu'on n'a rien
//     decime ;
//   * les compteurs de REFUS doivent etre NON NULS : un garde-fou qui ne se
//     declenche jamais ne prouve rien, il decore.
static void DecimateBattery() {
	NkVector<NkVertex3D> gv, cv;
	NkVector<uint32> gi, ci;
	MakeGrid(8, gv, gi);
	MakeCube(cv, ci);

	auto bboxOf = [](const NkEditMesh &m, float32 &dx, float32 &dz) {
		const NkMeshStats a = NkMeshAnalysis::Analyze(m);
		dx = a.bboxMax.x - a.bboxMin.x;
		dz = a.bboxMax.z - a.bboxMin.z;
	};

	// 1) SURFACE PLANE. Toutes les contractions interieures coutent exactement
	//    zero, donc on peut alleger tres fort SANS deformer. C'est la propriete
	//    qui distingue QEM d'une decimation qui se contente de compter.
	{
		NkEditMesh ref;
		ref.BuildFromIndexed(gv.Data(), (uint32)gv.Size(), gi.Data(), (uint32)gi.Size(), true);
		NkEditMesh m = ref;
		NkDecimateParams p;
		p.targetRatio = 0.15f;
		NkDecimateStats st;
		const bool ok = NkMeshDecimate::DecimateQEM(m, p, &st);
		float32 mean = 0.f, max = 0.f;
		NkMeshDecimate::ShapeError(m, ref, mean, max);
		float32 dx = 0.f, dz = 0.f;
		bboxOf(m, dx, dz);
		GraphPut("%-34s ok=%d tris %u->%u contract=%u | erreur moy=%.5f max=%.5f | bbox %.2fx%.2f",
				 "decim/plan-erreur-nulle", ok ? 1 : 0, st.trisBefore, st.trisAfter, st.collapses, (double)mean,
				 (double)max, (double)dx, (double)dz);
	}

	// 2) LE DRAPEAU DE BORD, DANS LES DEUX POSITIONS, sur le meme maillage et la
	//    meme cible.
	//
	//    PREMIERE VERSION DE CE CAS : une grille PLATE. Elle ne prouvait RIEN --
	//    sur un plan, deplacer un sommet DANS le plan coute zero, avec ou sans
	//    contrainte de bord ; les deux essais donnaient la meme boite englobante.
	//    Il faut une surface COURBE a bord plat : la ou l'interieur bombe, un bord
	//    non retenu se fait aspirer vers le haut et vers le centre.
	{
		NkEditMesh dome;
		dome.BuildFromIndexed(gv.Data(), (uint32)gv.Size(), gi.Data(), (uint32)gi.Size(), true);
		for (uint32 i = 0; i < dome.VertCount(); ++i) {
			const float32 x = dome.verts[i].pos.x, z = dome.verts[i].pos.z;
			// cos s'annule en +/-0,5 : le BORD reste plat a y=0, l'interieur bombe.
			dome.verts[i].pos.y = 0.35f * cosf(3.14159265f * x) * cosf(3.14159265f * z);
		}
		dome.RecomputeNormals();
		float32 dx0 = 0.f, dy0 = 0.f;
		{
			const NkMeshStats a = NkMeshAnalysis::Analyze(dome);
			dx0 = a.bboxMax.x - a.bboxMin.x;
			dy0 = a.bboxMax.y - a.bboxMin.y;
		}

		NkEditMesh avec = dome, sans = dome;
		NkDecimateParams pa;
		pa.targetRatio = 0.12f;
		pa.preserveBoundary = true;
		NkDecimateParams ps = pa;
		ps.preserveBoundary = false;
		NkDecimateStats sa, ss;
		NkMeshDecimate::DecimateQEM(avec, pa, &sa);
		NkMeshDecimate::DecimateQEM(sans, ps, &ss);
		const NkMeshStats ra = NkMeshAnalysis::Analyze(avec);
		const NkMeshStats rs = NkMeshAnalysis::Analyze(sans);
		const float32 dxa = ra.bboxMax.x - ra.bboxMin.x, dxs = rs.bboxMax.x - rs.bboxMin.x;
		GraphPut("%-34s depart larg=%.3f | avec-bord larg=%.3f bords=%u | sans-bord larg=%.3f bords=%u | perte=%.3f",
				 "decim/bord-retenu-ou-non", (double)dx0, (double)dxa, ra.boundaryEdges, (double)dxs,
				 rs.boundaryEdges, (double)(dxa - dxs));
		(void)dy0;
	}

	// 3) MAILLAGE FERME. Un cube subdivise deux fois est une variete fermee. Apres
	//    decimation il doit le RESTER : zero arete non manifold. Et les compteurs
	//    de refus doivent etre non nuls, sinon les garde-fous ne servent a rien.
	{
		NkEditMesh ref;
		ref.BuildFromIndexed(cv.Data(), (uint32)cv.Size(), ci.Data(), (uint32)ci.Size(), true);
		ref.SubdivideCatmullClark(3);
		const NkMeshStats before = NkMeshAnalysis::Analyze(ref);
		NkEditMesh m = ref;
		NkDecimateParams p;
		p.targetRatio = 0.35f;
		NkDecimateStats st;
		NkMeshDecimate::DecimateQEM(m, p, &st);
		const NkMeshStats after = NkMeshAnalysis::Analyze(m);
		float32 mean = 0.f, max = 0.f;
		NkMeshDecimate::ShapeError(m, ref, mean, max);
		GraphPut("%-34s tris %u->%u | nonmanif %u->%u ferme=%d | err moy=%.4f", "decim/ferme-reste-manifold",
				 st.trisBefore, st.trisAfter, before.nonManifoldEdges, after.nonManifoldEdges,
				 after.IsClosed() ? 1 : 0, (double)mean);

		// CE QUE CE CAS MONTRE, ET QUI N'EST PAS CE QUE J'ATTENDAIS.
		// Je pensais qu'a cible absurde la condition de lien finirait par refuser.
		// Elle ne refuse jamais ici : contracter une arete de tetraedre est LICITE
		// au sens du lien, et donne deux triangles superposes -- une variete fermee
		// degeneree. Le resultat est donc coherent, mais inutilisable.
		// Conclusion a retenir : la cible en nombre de faces n'est PAS un garde-fou
		// de qualite. Le vrai controle est le PLAFOND D'ERREUR (cas suivant).
		NkEditMesh ex = ref;
		NkDecimateParams pe;
		pe.targetRatio = 0.004f; // 3 triangles vises : impossible sur une variete fermee
		NkDecimateStats se;
		NkMeshDecimate::DecimateQEM(ex, pe, &se);
		const NkMeshStats ea = NkMeshAnalysis::Analyze(ex);
		GraphPut("%-34s tris %u->%u (cible 3) | refus lien=%u flip=%u | nonmanif=%u ferme=%d",
				 "decim/cible-absurde-degenere", se.trisBefore, se.trisAfter, se.rejectedLink,
				 se.rejectedFlip, ea.nonManifoldEdges, ea.IsClosed() ? 1 : 0);

		// Meme a l'extreme, un cube subdivise ne met PAS la condition de lien en
		// defaut : sa regularite fait que toute contraction reste licite. Il faut
		// une forme construite pour la violer.
		//
		// TUBE OUVERT A TROIS COTES. Sur l'arete du bas b0-b1, le sommet b2 est
		// voisin des DEUX extremites sans etre oppose a l'arete (une seule face la
		// porte, elle est au bord). Contracter b0 dans b1 pincerait le tube : deux
		// nappes se toucheraient par un sommet. C'est exactement ce que la
		// condition de lien existe pour refuser.
		//
		// On lance les DEUX versions : avec le controle, aucune arete non manifold
		// ne doit apparaitre ; sans lui, le maillage doit se degrader. Un seul essai
		// ne prouverait rien.
		{
			const float32 h = 0.866f;
			const NkVec3f P6[6] = {{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {0.5f, 0.f, h},
								   {0.f, 1.f, 0.f}, {1.f, 1.f, 0.f}, {0.5f, 1.f, h}};
			const uint32 T6[18] = {0, 1, 4, 0, 4, 3, 1, 2, 5, 1, 5, 4, 2, 0, 3, 2, 3, 5};
			NkVector<NkVertex3D> tv;
			NkVector<uint32> ti;
			for (int32 k = 0; k < 6; k++) {
				NkVertex3D x{};
				x.pos = P6[k];
				x.normal = {0.f, 1.f, 0.f};
				x.tangent = {1.f, 0.f, 0.f};
				x.color = 0xFFFFFFFFu;
				tv.PushBack(x);
			}
			for (int32 k = 0; k < 18; k++)
				ti.PushBack(T6[k]);

			NkEditMesh tubeA, tubeB;
			tubeA.BuildFromIndexed(tv.Data(), 6, ti.Data(), 18, false);
			tubeB = tubeA;
			NkDecimateParams pt;
			// 6 triangles au depart : viser 20 % donnerait 1, ce qui effondre tout et
			// ne teste rien. On vise 4 -- juste assez pour que la seule contraction
			// possible soit celle que la condition de lien doit refuser.
			pt.targetFaces = 4;
			pt.preserveTopology = true;
			NkDecimateParams pn = pt;
			pn.preserveTopology = false;
			NkDecimateStats sA, sB;
			const bool okA = NkMeshDecimate::DecimateQEM(tubeA, pt, &sA);
			const bool okB = NkMeshDecimate::DecimateQEM(tubeB, pn, &sB);
			const NkMeshStats aA = NkMeshAnalysis::Analyze(tubeA);
			const NkMeshStats aB = NkMeshAnalysis::Analyze(tubeB);
			GraphPut("%-34s avec ok=%d tris=%u refus-lien=%u nonmanif=%u | sans ok=%d tris=%u nonmanif=%u",
					 "decim/condition-de-lien", okA ? 1 : 0, sA.trisAfter, sA.rejectedLink, aA.nonManifoldEdges,
					 okB ? 1 : 0, sB.trisAfter, aB.nonManifoldEdges);
		}
	}

	// 4) L'ERREUR DOIT CROITRE quand la cible se resserre. Une erreur qui
	//    stagnerait signalerait que la cible n'est pas suivie, ou que la mesure
	//    ne mesure rien.
	{
		NkEditMesh ref;
		ref.BuildFromIndexed(cv.Data(), (uint32)cv.Size(), ci.Data(), (uint32)ci.Size(), true);
		ref.SubdivideCatmullClark(3);
		char buf[160];
		int32 w = 0;
		for (int32 k = 0; k < 3; k++) {
			const float32 ratios[3] = {0.60f, 0.30f, 0.12f};
			NkEditMesh m = ref;
			NkDecimateParams p;
			p.targetRatio = ratios[k];
			NkDecimateStats st;
			NkMeshDecimate::DecimateQEM(m, p, &st);
			float32 mean = 0.f, max = 0.f;
			NkMeshDecimate::ShapeError(m, ref, mean, max);
			w += snprintf(buf + w, sizeof(buf) - (size_t)w, "%s%.0f%%:tris=%u,err=%.4f", w ? " | " : "",
						  (double)(ratios[k] * 100.f), st.trisAfter, (double)mean);
		}
		GraphPut("%-34s %s", "decim/erreur-croit-avec-la-cible", buf);
	}

	// 5) LE PLAFOND D'ERREUR. « Allege tant que tu ne deformes pas » plutot que
	//    « atteins ce compte coute que coute » : avec un plafond serre sur un cube
	//    subdivise, la cible ne doit PAS etre atteinte -- et c'est le comportement
	//    voulu, pas un echec.
	{
		NkEditMesh ref;
		ref.BuildFromIndexed(cv.Data(), (uint32)cv.Size(), ci.Data(), (uint32)ci.Size(), true);
		ref.SubdivideCatmullClark(3);
		NkEditMesh m = ref;
		NkDecimateParams p;
		p.targetRatio = 0.05f;
		p.maxError = 1e-5f;
		NkDecimateStats st;
		NkMeshDecimate::DecimateQEM(m, p, &st);
		GraphPut("%-34s tris %u->%u cible-atteinte=%d (0 attendu) refus-cout=%u cout-max=%.6f",
				 "decim/plafond-erreur", st.trisBefore, st.trisAfter, st.reachedTarget ? 1 : 0, st.rejectedCost,
				 (double)st.maxCost);
	}
}

// -- RETOPOLOGIE : CHAMP DE CROIX + FUSION QUAD-DOMINANTE --------------------
// Le juge de paix est le DEMI-CYLINDRE : une grille pliee autour de l'axe X.
//   * Quadify() exige la coplanarite (cos > 0,985) ; a 8 segments le diedre
//     entre les deux triangles d'une cellule vaut ~22,5 degres (cos 0,924) --
//     Quadify doit donc rendre ZERO quad. Si le champ reussit la ou Quadify
//     echoue, la fusion guidee apporte reellement quelque chose ;
//   * le champ est initialise sur l'arete LA PLUS LONGUE, la DIAGONALE (45
//     degres de travers, alignement ~0,71). Mesurer a 0 iteration PUIS a 20
//     prouve que le lissage TRAVAILLE -- sans le depart oblique, un champ ne
//     faisant rien passerait le test.
static void RetopoBattery() {
	NkVector<NkVertex3D> gv, cv;
	NkVector<uint32> gi, ci;
	MakeGrid(8, gv, gi);
	MakeCube(cv, ci);

	// La grille pliee en demi-cylindre autour de X : le bord droit reste droit
	// (le long de X), les anneaux suivent la courbure. Rayon 1/pi pour que la
	// longueur deroulee reste 1.
	auto bend = [&](NkEditMesh &m) {
		const float32 r = 1.f / 3.14159265f;
		for (uint32 i = 0; i < m.VertCount(); ++i) {
			const float32 z = m.verts[i].pos.z;
			const float32 th = 3.14159265f * (z + 0.5f);
			m.verts[i].pos.y = r * sinf(th);
			m.verts[i].pos.z = r * cosf(th);
		}
		m.RecomputeNormals();
	};
	// Alignement d'une direction tangente au cylindre sur la croix attendue
	// {axe X, direction d'anneau} : l'anneau vit dans le plan YZ, donc
	// sqrt(y^2+z^2) mesure la composante d'anneau. Une diagonale donne ~0,71,
	// une direction alignee ~1.
	auto alignCyl = [](const NkVec3f &d) {
		const float32 ring = sqrtf(d.y * d.y + d.z * d.z);
		const float32 ax = fabsf(d.x);
		return ax > ring ? ax : ring;
	};

	// 1) LE LISSAGE TRAVAILLE. Champ a 0 iteration (depart diagonal) puis a 20 :
	//    l'alignement moyen doit MONTER nettement. Les deux chiffres cote a cote
	//    sont la preuve ; l'un sans l'autre ne prouverait rien.
	{
		NkEditMesh cyl;
		cyl.BuildFromIndexed(gv.Data(), (uint32)gv.Size(), gi.Data(), (uint32)gi.Size(), false);
		bend(cyl);
		NkVector<NkVec3f> d0, d20;
		NkVector<uint8> p0, p20;
		NkMeshRetopo::ComputeCrossField(cyl, 40.f, 0, d0, p0);
		NkMeshRetopo::ComputeCrossField(cyl, 40.f, 20, d20, p20);
		float32 a0 = 0.f, a20 = 0.f;
		uint32 pinned = 0;
		for (uint32 i = 0; i < (uint32)d0.Size(); ++i) {
			a0 += alignCyl(d0[i]);
			a20 += alignCyl(d20[i]);
			if (p20[i])
				pinned++;
		}
		if (!d0.Empty()) {
			a0 /= (float32)d0.Size();
			a20 /= (float32)d0.Size();
		}
		GraphPut("%-34s faces=%u epinglees=%u | align 0-iter=%.3f -> 20-iter=%.3f (doit monter)",
				 "retopo/champ-lissage-travaille", (uint32)d0.Size(), pinned, (double)a0, (double)a20);
	}

	// 2) CE QUE LE CYLINDRE PROUVE -- ET CE QU'IL NE PROUVE PAS.
	//    Je croyais Quadify incapable ici (diedre 22,5 degres). FAUX, et le
	//    premier passage du harnais l'a montre : un cylindre est une surface
	//    DEVELOPPABLE, chaque cellule pliee reste un rectangle PLAN, donc
	//    Quadify reussit aussi (64 quads). Le diedre est entre CELLULES, pas
	//    dans la cellule. Ce cas prouve donc l'ALIGNEMENT du champ et la
	//    coherence d'orientation des quads emis (zero arete non manifold) --
	//    la discrimination naif/champ, elle, est au cas suivant.
	{
		NkEditMesh naive, field;
		naive.BuildFromIndexed(gv.Data(), (uint32)gv.Size(), gi.Data(), (uint32)gi.Size(), false);
		bend(naive);
		field = naive;
		naive.Quadify(0.985f);
		const NkMeshStats an = NkMeshAnalysis::Analyze(naive);
		NkRetopoParams rp; // targetRatio=1 : fusion seule, sans decimation
		NkRetopoStats rs;
		const bool ok = NkMeshRetopo::QuadDominant(field, rp, &rs);
		const NkMeshStats af = NkMeshAnalysis::Analyze(field);
		GraphPut("%-34s naif quads=%u (cellules PLANES : il reussit aussi) | champ ok=%d quads=%u align=%.3f nonmanif=%u",
				 "retopo/cylindre-alignement", an.quads, ok ? 1 : 0, rs.quadsOut, (double)rs.alignMean,
				 af.nonManifoldEdges);
	}

	// 2bis) LA VRAIE DISCRIMINATION : APRES DECIMATION. Quadify ne considere que
	//    les paires CONSECUTIVES (f, f+1) issues d'une triangulation
	//    quad-par-quad ; apres une decimation QEM, cet ordre n'existe plus.
	//    Le champ, lui, apparie par adjacence reelle. Les deux chiffres cote a
	//    cote sont la preuve que la fusion guidee apporte quelque chose.
	{
		NkEditMesh m;
		m.BuildFromIndexed(cv.Data(), (uint32)cv.Size(), ci.Data(), (uint32)ci.Size(), true);
		m.SubdivideCatmullClark(3);
		NkDecimateParams dp;
		dp.targetRatio = 0.35f;
		NkMeshDecimate::DecimateQEM(m, dp, nullptr);
		NkEditMesh naive = m, field = m;
		naive.Quadify(0.985f);
		const NkMeshStats an = NkMeshAnalysis::Analyze(naive);
		NkRetopoParams rp; // fusion seule : la decimation est deja faite
		NkRetopoStats rs;
		NkMeshRetopo::QuadDominant(field, rp, &rs);
		GraphPut("%-34s naif quads=%u | champ quads=%u tris-restants=%u (le champ doit dominer largement)",
				 "retopo/apres-decimation-naif-champ", an.quads, rs.quadsOut, rs.trisOut);
	}

	// 3) PIPELINE COMPLET sur un maillage FERME : cube Catmull-Clark (quads) ->
	//    decimation QEM (triangles) -> fusion (quads a nouveau). Le maillage doit
	//    RESTER ferme et manifold -- c'est ce que casserait une emission de quads
	//    aux orientations incoherentes, et qu'aucun compte de faces ne montre.
	{
		NkEditMesh m;
		m.BuildFromIndexed(cv.Data(), (uint32)cv.Size(), ci.Data(), (uint32)ci.Size(), true);
		m.SubdivideCatmullClark(3);
		NkRetopoParams rp;
		rp.targetRatio = 0.35f;
		NkRetopoStats rs;
		const bool ok = NkMeshRetopo::QuadDominant(m, rp, &rs);
		const NkMeshStats a = NkMeshAnalysis::Analyze(m);
		GraphPut("%-34s ok=%d decim %u->%u tris | sortie quads=%u tris=%u ratio=%.2f | ferme=%d manifold=%d genre=%d",
				 "retopo/pipeline-ferme-manifold", ok ? 1 : 0, rs.decim.trisBefore, rs.decim.trisAfter,
				 rs.quadsOut, rs.trisOut, (double)rs.quadRatio, a.IsClosed() ? 1 : 0, a.IsManifold() ? 1 : 0,
				 a.genus);
	}

	// 4) LE SEUIL DE QUALITE AGIT. Premier essai sur le cylindre : INERTE, ses
	//    cellules sont des rectangles exacts, qualite 1,0 partout, rien a
	//    filtrer. On le mesure donc sur le maillage DECIME, dont les triangles
	//    irreguliers donnent des quads de qualite variable : le seuil strict
	//    doit en refuser une partie.
	{
		NkEditMesh m;
		m.BuildFromIndexed(cv.Data(), (uint32)cv.Size(), ci.Data(), (uint32)ci.Size(), true);
		m.SubdivideCatmullClark(3);
		NkDecimateParams dp;
		dp.targetRatio = 0.35f;
		NkMeshDecimate::DecimateQEM(m, dp, nullptr);
		NkEditMesh a = m, b = m;
		NkRetopoParams pa;
		NkRetopoParams pb;
		pb.minQuadQuality = 0.75f;
		NkRetopoStats sa, sb;
		NkMeshRetopo::QuadDominant(a, pa, &sa);
		NkMeshRetopo::QuadDominant(b, pb, &sb);
		GraphPut("%-34s defaut(0,25) quads=%u | strict(0,75) quads=%u (doit chuter)",
				 "retopo/seuil-qualite-agit", sa.quadsOut, sb.quadsOut);
	}

	// 5) ARETE VIVE EPINGLEE. Sur un cube BRUT (aretes a 90 degres), toutes les
	//    faces touchent une arete vive : elles doivent etre epinglees, et le
	//    champ doit etre aligne sur les axes SANS aucun lissage. C'est le premier
	//    temps du calcul (les epingles) teste isolement du second (le lissage).
	{
		NkEditMesh cube;
		cube.BuildFromIndexed(cv.Data(), (uint32)cv.Size(), ci.Data(), (uint32)ci.Size(), false);
		NkVector<NkVec3f> d;
		NkVector<uint8> pin;
		NkMeshRetopo::ComputeCrossField(cube, 40.f, 0, d, pin);
		uint32 pinned = 0;
		float32 axisAlign = 0.f;
		for (uint32 i = 0; i < (uint32)d.Size(); ++i) {
			if (pin[i])
				pinned++;
			float32 best = fabsf(d[i].x);
			if (fabsf(d[i].y) > best)
				best = fabsf(d[i].y);
			if (fabsf(d[i].z) > best)
				best = fabsf(d[i].z);
			axisAlign += best;
		}
		if (!d.Empty())
			axisAlign /= (float32)d.Size();
		GraphPut("%-34s faces=%u epinglees=%u (toutes attendues) | align-axes=%.3f (1,0 attendu)",
				 "retopo/aretes-vives-epinglees", (uint32)d.Size(), pinned, (double)axisAlign);
	}
}

// -- THEMES ------------------------------------------------------------------
// Regle de UI_SPEC 10bis : les couleurs PORTEUSES DE SENS vivent DANS le theme
// -- axes, etats de selection, types d'assets compris. Laissees en dur, le theme
// clair serait illisible sans que personne s'en apercoive avant la capture.
//
// Les cas visent les deux pieges du systeme :
//   * un theme UTILISATEUR ne redefinit que quelques couleurs ; un chargeur naif
//     laisserait les autres a zero, donc NOIR, et le theme paraitrait charge tout
//     en etant inutilisable ;
//   * un theme clair fabrique en INVERSANT les gris garderait les couleurs
//     metier du sombre -- elles deviendraient illisibles sur fond clair. On le
//     MESURE au lieu de l'affirmer.
static void ThemeBattery() {
	using namespace nkentseu::editorkit;

	// 1) ALLER-RETOUR. On modifie deux roles, on enregistre, on relit dans une
	//    base NEUVE : les deux modifications ET les 27 autres doivent survivre.
	//    Enregistrer puis relire un theme INCHANGE ne prouverait rien.
	{
		NkTheme t = NkTheme::Dark();
		t.SetName("Mon theme");
		t.Set(NkRole::AccentUi, NkTheme::FromHex("#123456"));
		t.Set(NkRole::AxisX, NkTheme::FromHex("#ABCDEF"));
		NkString txt;
		t.Save(txt);
		NkTheme relu = NkTheme::Dark();
		uint32 unknown = 0, applied = 0;
		const bool ok = relu.Load(txt.CStr(), &unknown, &applied);
		uint32 diff = 0;
		for (uint16 i = 0; i < (uint16)NkRole::Count; ++i)
			if (relu.Get((NkRole)i) != t.Get((NkRole)i))
				diff++;
		GraphPut("%-34s lu=%d appliques=%u inconnus=%u | roles differents=%u (0 attendu) nom=%s",
				 "theme/aller-retour", ok ? 1 : 0, applied, unknown, diff, relu.Name().CStr());
	}

	// 2) LE PIEGE — HERITAGE D'UN THEME PARTIEL. Trois lignes seulement. Les
	//    trois doivent ecraser, les 26 autres doivent rester CELLES DE LA BASE.
	//    Un chargeur qui repartirait de zero les mettrait a noir : le compte de
	//    roles appliques serait le meme, et rien ne le signalerait.
	{
		const char *partiel = "nktheme 1\n"
							  "nom Bleu nuit\n"
							  "accent_ui = #2266FF\n"
							  "panel_bg = #101820\n"
							  "# une ligne de commentaire, doit etre ignoree\n"
							  "accent_sel = #FFAA22\n";
		const NkTheme base = NkTheme::Dark();
		NkTheme t = NkTheme::Dark();
		uint32 unknown = 0, applied = 0;
		t.Load(partiel, &unknown, &applied);
		uint32 herites = 0, noirs = 0;
		for (uint16 i = 0; i < (uint16)NkRole::Count; ++i) {
			const NkRole r = (NkRole)i;
			if (r == NkRole::AccentUi || r == NkRole::PanelBg || r == NkRole::AccentSel)
				continue;
			if (t.Get(r) == base.Get(r))
				herites++;
			if ((t.Get(r) >> 8) == 0u)
				noirs++;
		}
		char h[10];
		NkTheme::ToHex(t.Get(NkRole::AccentUi), h);
		GraphPut("%-34s appliques=%u | herites=%u/%u noirs=%u (0 attendu) | accent_ui=%s nom=%s",
				 "theme/heritage-partiel", applied, herites, (uint32)NkRole::Count - 3u, noirs, h,
				 t.Name().CStr());
	}

	// 3) ROLE INCONNU TOLERE. Un theme ecrit pour une version plus recente doit
	//    se charger quand meme, en le SIGNALANT. Refuser tout le fichier pour une
	//    ligne inconnue rendrait chaque ajout de role incompatible avec l'existant.
	{
		const char *futur = "nktheme 1\n"
							"accent_ui = #2266FF\n"
							"couleur_de_2027 = #FF00FF\n"
							"axis_x = #FF0000\n";
		NkTheme t = NkTheme::Dark();
		uint32 unknown = 0, applied = 0;
		const bool ok = t.Load(futur, &unknown, &applied);
		char h[10];
		NkTheme::ToHex(t.Get(NkRole::AxisX), h);
		GraphPut("%-34s lu=%d inconnus=%u (1 attendu) appliques=%u (2 attendus) | axis_x=%s",
				 "theme/role-inconnu-tolere", ok ? 1 : 0, unknown, applied, h);
	}

	// 4) CE QUI JUSTIFIE TOUT LE SYSTEME. Le theme clair n'INVERSE pas les gris,
	//    il les REMPLACE, et il ASSOMBRIT les couleurs metier. On mesure le
	//    contraste de l'axe Y sur le fond de panneau dans les deux themes, PUIS
	//    celui qu'on aurait eu en gardant la couleur du sombre sur fond clair.
	//    Ce troisieme chiffre est la preuve : s'il n'etait pas nettement plus
	//    faible, mettre les axes dans le theme n'aurait servi a rien.
	{
		const NkTheme d = NkTheme::Dark();
		const NkTheme l = NkTheme::Light();
		const float32 cd = NkTheme::Contrast(d.Get(NkRole::AxisY), d.Get(NkRole::PanelBg));
		const float32 cl = NkTheme::Contrast(l.Get(NkRole::AxisY), l.Get(NkRole::PanelBg));
		const float32 naif = NkTheme::Contrast(d.Get(NkRole::AxisY), l.Get(NkRole::PanelBg));
		GraphPut("%-34s axeY/panneau sombre=%.2f clair=%.2f | couleur-du-sombre-sur-clair=%.2f (doit chuter)",
				 "theme/couleurs-metier-dans-theme", (double)cd, (double)cl, (double)naif);
	}

	// 5) VALIDATION. L'utilisateur pouvant fabriquer ses themes, il en fabriquera
	//    un illisible. Les deux themes livres doivent passer largement ; un theme
	//    volontairement mauvais doit etre signale bas. Sans le mauvais cas, un
	//    validateur qui renverrait toujours un grand nombre passerait aussi.
	{
		const NkTheme d = NkTheme::Dark();
		const NkTheme l = NkTheme::Light();
		NkTheme mauvais = NkTheme::Dark();
		mauvais.Set(NkRole::Text, NkTheme::FromHex("#303030")); // gris sur gris
		NkThemeIssue id{}, il{}, im{};
		const uint32 fd = d.Validate(&id);
		const uint32 fl = l.Validate(&il);
		const uint32 fm = mauvais.Validate(&im);
		GraphPut("%-34s defauts sombre=%u clair=%u (0 attendu) | theme casse : %u defaut(s), pire %s sur %s = %.2f (exige %.1f)",
				 "theme/validation-contraste", fd, fl, fm, NkRoleName(im.fg), NkRoleName(im.bg),
				 (double)im.ratio, (double)im.required);
		// Le SEUIL DEPEND DE L'USAGE, et le verifier compte : le meme rapport doit
		// passer pour un element graphique et echouer pour du texte. Un validateur
		// a seuil unique rendrait ces deux lignes identiques.
		GraphPut("%-34s pire rapport brut sombre=%.2f clair=%.2f | seuils texte=4,5 graphique=3,0",
				 "theme/seuils-par-usage", (double)d.WorstContrast(), (double)l.WorstContrast());
	}

	// 6) ROLES D'APPLICATION. Le garde-fou n.1 de NKGraph applique aux themes :
	//    NK3DModeler enregistre SES roles, NKEditorKit n'a pas a les connaitre.
	//    LE PIEGE est l'enregistrement : une sauvegarde qui ne parcourrait que
	//    l'enumeration du coeur perdrait ces roles EN SILENCE, et le fichier
	//    paraitrait pourtant complet. On verifie donc l'aller-retour, pas la
	//    simple ecriture.
	{
		const uint16 brosse = NkRoleRegistry::Register("nk3d.anneau_brosse");
		const uint16 encore = NkRoleRegistry::Register("nk3d.anneau_brosse");
		const uint16 cle = NkRoleRegistry::Register("nk3d.cle_anim");
		NkTheme t = NkTheme::Dark();
		t.Set(brosse, NkTheme::FromHex("#FF00AA"));
		t.Set(cle, NkTheme::FromHex("#00FF88"));
		NkString txt;
		t.Save(txt);
		NkTheme relu = NkTheme::Dark();
		uint32 unknown = 0, applied = 0;
		relu.Load(txt.CStr(), &unknown, &applied);
		char h[10];
		NkTheme::ToHex(relu.Get(brosse), h);
		GraphPut("%-34s ids coeur=%u ext=%u,%u idempotent=%d | apres aller-retour %s inconnus=%u",
				 "theme/roles-application", (uint32)NkRole::Count, brosse, cle, (brosse == encore) ? 1 : 0, h,
				 unknown);
	}

	// 7) UN THEME D'UNE AUTRE APPLICATION SE CHARGE QUAND MEME. C'est la
	//    consequence recherchee : Nogee lisant un theme ecrit pour NkAnima ne doit
	//    pas echouer, seulement ignorer ce qu'il ne connait pas. Refuser tout le
	//    fichier rendrait les themes non partageables entre applications.
	{
		const char *autre = "nktheme 1\n"
							"nom Venu d'ailleurs\n"
							"accent_ui = #445566\n"
							"nkanima.cle_de_pose = #FFEE00\n"
							"nogee.gizmo_lumiere = #00EEFF\n";
		NkTheme t = NkTheme::Dark();
		uint32 unknown = 0, applied = 0;
		const bool ok = t.Load(autre, &unknown, &applied);
		char h[10];
		NkTheme::ToHex(t.Get(NkRole::AccentUi), h);
		GraphPut("%-34s lu=%d appliques=%u inconnus=%u (2 attendus) | accent_ui=%s",
				 "theme/theme-d-une-autre-app", ok ? 1 : 0, applied, unknown, h);
	}

	// 8) BIBLIOTHEQUE. « Choisir parmi plusieurs ou creer le sien » suppose une
	//    LISTE. Et un theme utilisateur qui REPREND UN NOM EXISTANT doit le
	//    REMPLACER : c'est ce qu'attend quelqu'un qui surcharge « Sombre » depuis
	//    son dossier personnel. En ajouter un homonyme donnerait deux entrees
	//    indistinguables dans le menu.
	{
		NkThemeLibrary lib;
		lib.AddBuiltins();
		const uint32 apresBuiltins = lib.Count();
		// Un theme NEUF, base sur le clair : il doit hériter du CLAIR, pas du sombre.
		lib.AddFromText("nktheme 1\nnom Papier\naccent_ui = #AA3300\n", false);
		const bool trouve = lib.SetCurrent("Papier");
		const bool clairHerite = (lib.Current().Get(NkRole::WindowBg) == NkTheme::FromHex("#F5F5F5"));
		// Une SURCHARGE de « Sombre » : le compte ne doit PAS augmenter.
		const uint32 avantSurcharge = lib.Count();
		lib.AddFromText("nktheme 1\nnom Sombre\nwindow_bg = #000000\n", true);
		const bool remplace = (lib.Count() == avantSurcharge);
		lib.SetCurrent("Sombre");
		char h[10];
		NkTheme::ToHex(lib.Current().Get(NkRole::WindowBg), h);
		GraphPut("%-34s builtins=%u | Papier trouve=%d herite-du-clair=%d | surcharge sans doublon=%d Sombre.fond=%s",
				 "theme/bibliotheque", apresBuiltins, trouve ? 1 : 0, clairHerite ? 1 : 0, remplace ? 1 : 0, h);
	}

	// 9) LES SIX COULEURS IMPOSEES par Rihen sont bien la, aux roles decides en
	//    UI_SPEC 10bis. Un test qui se contenterait de charger le theme ne dirait
	//    pas si j'ai respecte l'affectation.
	{
		const NkTheme d = NkTheme::Dark();
		struct Att {
				NkRole r;
				const char *hex;
		};
		// LES TROIS GRIS NE SONT PLUS CONTRACTUELS. Rihen a demande le 31/07 un
		// theme sombre facon GitHub Dark, qui les REMPLACE (#010409 / #0D1117 /
		// #161B22). Ce qui reste impose, ce sont les couleurs PORTEUSES DE SENS :
		// l'orange unique et les deux sarcelles. Elles traversent les themes parce
		// qu'elles disent quelque chose -- « selectionne », « noeud de donnees » --
		// alors qu'un gris de fond ne dit rien qu'un autre gris ne dirait aussi bien.
		//
		// La HIERARCHIE A TROIS NIVEAUX, elle, reste la regle : fenetre plus sombre
		// que panneau, panneau plus sombre qu'en-tete. C'est elle qui structure la
		// lecture, pas les valeurs exactes -- et c'est elle qu'on verifie ci-dessous.
		const Att att[3] = {
			{NkRole::AccentSel, "#F2980E"},
			{NkRole::NodeDataHeader, "#0A545E"},
			{NkRole::NodeDataHeaderHot, "#095461"},
		};
		uint32 conformes = 0;
		for (int32 i = 0; i < 3; ++i)
			if (d.Get(att[i].r) == NkTheme::FromHex(att[i].hex))
				conformes++;
		// Les deux sarcelles doivent RESTER proches : c'est voulu (meme en-tete,
		// deux etats). Si elles s'ecartaient, ce serait une seconde famille.
		const float32 ecart =
			NkTheme::Contrast(d.Get(NkRole::NodeDataHeader), d.Get(NkRole::NodeDataHeaderHot));
		// La hierarchie a trois niveaux : chaque cran doit etre PLUS CLAIR que le
		// precedent. Comparer les luminances plutot que les valeurs permet a un
		// theme de changer de palette sans casser la regle.
		const float32 lWin = NkTheme::Contrast(d.Get(NkRole::WindowBg), 0x000000FFu);
		const float32 lPan = NkTheme::Contrast(d.Get(NkRole::PanelBg), 0x000000FFu);
		const float32 lHdr = NkTheme::Contrast(d.Get(NkRole::PanelHeader), 0x000000FFu);
		const bool hierarchie = (lWin < lPan) && (lPan < lHdr);
		GraphPut("%-34s porteuses de sens=%u/3 | ecart sarcelles=%.3f | hierarchie a 3 niveaux=%d",
				 "theme/palette-imposee", conformes, (double)ecart, hierarchie ? 1 : 0);
	}
}

// ── COMPOSANTES CONNEXES / `L` ──────────────────────────────────────────────
// Ce que ces cas verifient, et pourquoi chacun est la :
//
//  1. UN CUBE = UNE composante. MakeCube duplique ses sommets PAR FACE (24 pour
//     8 positions) : une connexite par indice BRUT y verrait 6 ilots, un par
//     face. Ce cas echoue bruyamment si l'identite soudee n'est pas respectee —
//     c'est le seul qui protege le choix central de l'implementation.
//  2. DEUX cubes disjoints = DEUX composantes, et `L` depuis l'un ne prend que
//     l'autre moitie. C'est le geste de Rihen, litteralement.
//  3. LES DEUX INVARIANTS de la specification : la somme des tailles egale le
//     nombre de sommets, et aucun sommet ne reste sans composante. Ils tiennent
//     lieu de test exhaustif — on ne peut pas les satisfaire par hasard.
static void LinkedBattery() {
	NkVector<NkVertex3D> v;
	NkVector<uint32> idx;
	MakeCube(v, idx);

	// ── 1. Un seul cube ─────────────────────────────────────────────────────
	{
		NkEditMesh m;
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		m.RebuildEdges();
		NkVector<int32> comp;
		const uint32 nc = m.ComputeConnectedComponents(comp);
		uint32 assigned = 0;
		for (uint32 i = 0; i < (uint32)comp.Size(); ++i)
			if (comp[i] >= 0)
				assigned++;
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s composantes=%u sommets=%u assignes=%u",
					 "linked/cube-soude", nc, (uint32)m.verts.Size(), assigned);
			gLineCount++;
		}
	}

	// ── 2. Deux cubes disjoints ─────────────────────────────────────────────
	{
		NkVector<NkVertex3D> v2 = v;
		NkVector<uint32> i2 = idx;
		const uint32 base = (uint32)v.Size();
		for (uint32 i = 0; i < (uint32)v.Size(); ++i) {
			NkVertex3D t = v[i];
			t.pos.x += 10.f; // largement hors tolerance de soudure
			v2.PushBack(t);
		}
		for (uint32 i = 0; i < (uint32)idx.Size(); ++i)
			i2.PushBack(idx[i] + base);

		NkEditMesh m;
		m.BuildFromIndexed(v2.Data(), (uint32)v2.Size(), i2.Data(), (uint32)i2.Size(), true);
		m.RebuildEdges();
		NkVector<int32> comp;
		const uint32 nc = m.ComputeConnectedComponents(comp);

		// Tailles par composante -> invariant de somme.
		uint32 sizes[8] = {0, 0, 0, 0, 0, 0, 0, 0};
		uint32 orphelins = 0;
		for (uint32 i = 0; i < (uint32)comp.Size(); ++i) {
			if (comp[i] < 0)
				orphelins++;
			else if ((uint32)comp[i] < 8)
				sizes[comp[i]]++;
		}
		uint32 somme = 0;
		for (uint32 c = 0; c < 8 && c < nc; ++c)
			somme += sizes[c];

		// `L` depuis le sommet 0 : il ne doit prendre QUE son ilot.
		m.SelectNone();
		const bool ok = m.SelectLinked(0, true);
		uint32 sel = 0;
		for (uint32 i = 0; i < (uint32)m.verts.Size(); ++i)
			if (m.verts[i].sel)
				sel++;

		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256,
					 "%-34s composantes=%u somme=%u sommets=%u orphelins=%u L(ok=%d)=%u", "linked/deux-cubes", nc,
					 somme, (uint32)m.verts.Size(), orphelins, ok ? 1 : 0, sel);
			gLineCount++;
		}

		// Ctrl+L depuis la meme selection : l'ilot est DEJA entier, donc il ne
		// doit rien ajouter -- et le dire (aChange=0). C'est ce cas qui a revele
		// que mon booleen repondait « oui » sans que rien ne bouge.
		// Puis, sans aucune selection, il doit REFUSER.
		const bool grew = m.SelectLinkedFromSelection();
		uint32 sel2 = 0;
		for (uint32 i = 0; i < (uint32)m.verts.Size(); ++i)
			if (m.verts[i].sel)
				sel2++;
		m.SelectNone();
		const bool vide = m.SelectLinkedFromSelection();
		if (gLineCount < 512) {
			snprintf(gLines[gLineCount], 256, "%-34s aChange=%d apres=%u surSelectionVide=%d", "linked/ctrl-L",
					 grew ? 1 : 0, sel2, vide ? 1 : 0);
			gLineCount++;
		}
	}
}

// ── MATERIAU PAR FACE ───────────────────────────────────────────────────────
// CE QUE MESURE CETTE BATTERIE, ET POURQUOI ELLE EXISTE
// Rodolf : « l'objectif est que un groupe de vertex ou de face lier ou non
// partage meme material comme sur blender ». Le mot qui commande est
// « lier ou NON » : les faces d'un meme materiau n'ont aucune raison d'etre
// connexes. La figure de reference est donc DEUX FACES OPPOSEES d'un cube
// (+Z et -Z), qui ne partagent aucune arete.
//
// La partie facile est le stockage. La partie difficile — et la seule qui
// distingue un vrai materiau par face d'un decoupage cosmetique — est la
// SURVIE AUX OPERATIONS TOPOLOGIQUES : toutes passent par le round-trip
// ToPolygons/BuildFromPolygons, qui reconstruit les `Face` a neuf.
//
// REGIMES COUVERTS : subdivision lineaire, Catmull-Clark, extrusion,
// triangulation, soudure. NON COUVERTS : la decimation (la regle d'heritage
// lors d'une fusion de faces n'est pas encore tranchee — voir la ligne
// mat/decim-non-tranche, qui MESURE l'etat actuel sans le valider), et tout
// ce qui touche au rendu.
static void MaterialBattery() {
	NkVector<NkVertex3D> v;
	NkVector<uint32> idx;
	MakeCube(v, idx);

	// Trouve la face dont la normale est la plus proche de `dir`. On designe les
	// faces par leur GEOMETRIE et jamais par un index en dur : quadify peut
	// reordonner, et un test qui suppose l'ordre mesurerait l'ordre, pas le
	// materiau.
	auto faceAlong = [](const NkEditMesh &m, NkVec3f dir) -> uint32 {
		uint32 best = 0;
		float32 bestDot = -2.f;
		for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f) {
			if (!m.faces[f].alive)
				continue;
			const float32 d = m.faces[f].normal.Dot(dir);
			if (d > bestDot) {
				bestDot = d;
				best = f;
			}
		}
		return best;
	};

	auto countMat = [](const NkEditMesh &m, uint16 slot) -> uint32 {
		uint32 n = 0;
		for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f)
			if (m.faces[f].alive && m.faces[f].material == slot)
				n++;
		return n;
	};

	// Deux faces partagent-elles une arete ? Sert a PROUVER que la figure est bien
	// « non liee » au lieu de le supposer.
	auto shareEdge = [](const NkEditMesh &m, uint32 fa, uint32 fb) -> bool {
		NkVector<NkEmId> la, lb;
		m.GetFaceVerts(fa, la);
		m.GetFaceVerts(fb, lb);
		uint32 common = 0;
		for (uint32 i = 0; i < (uint32)la.Size(); ++i)
			for (uint32 j = 0; j < (uint32)lb.Size(); ++j)
				if (la[i] == lb[j])
					common++;
		return common >= 2; // deux sommets communs = une arete commune
	};

	// Construit le cube SOUDE (quadify) et pose le slot 1 sur +Z et -Z.
	auto makeTwoIslands = [&](NkEditMesh &m) {
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		m.RebuildEdges();
		const uint32 fz = faceAlong(m, {0.f, 0.f, 1.f});
		const uint32 fmz = faceAlong(m, {0.f, 0.f, -1.f});
		m.faces[fz].material = 1;
		m.faces[fmz].material = 1;
		return shareEdge(m, fz, fmz);
	};

	// ── 1. La figure elle-meme : deux ilots, aucune arete commune ────────────
	{
		NkEditMesh m;
		const bool touching = makeTwoIslands(m);
		Put("{0:<34} faces={1} slot1={2} slot0={3} aretecommune={4} (0 attendu)", "mat/ilots-disjoints",
				 (uint32)m.faces.Size(), countMat(m, 1), countMat(m, 0), touching ? 1 : 0);
	}

	// ── 2. Les sous-mailles sont DEDUITES, et un slot donne DEUX plages ──────
	// ⚠ FIGURE CHOISIE : DEUX CUBES SEPARES, pas les deux faces opposees d'un
	// seul. Premiere version de ce cas ecrite sur un cube unique : elle exigeait
	// 2 plages et n'en obtenait qu'1 — non parce que le regroupement etait faux,
	// mais parce que +Z et -Z sont CONSECUTIVES dans l'ordre des faces, donc
	// elles fusionnent en une seule plage. Le cas mesurait l'ordre des faces, pas
	// le regroupement par materiau. Deux cubes distants mettent d'autres faces
	// entre les deux ilots : la plage ne PEUT plus etre unique, et le resultat ne
	// depend plus d'un hasard d'ordonnancement.
	{
		NkVector<NkVertex3D> v2 = v;
		NkVector<uint32> i2 = idx;
		const uint32 base = (uint32)v.Size();
		for (uint32 i = 0; i < (uint32)v.Size(); ++i) {
			NkVertex3D t = v[i];
			t.pos.x += 10.f; // largement hors tolerance de soudure
			v2.PushBack(t);
		}
		for (uint32 i = 0; i < (uint32)idx.Size(); ++i)
			i2.PushBack(idx[i] + base);

		NkEditMesh m;
		m.BuildFromIndexed(v2.Data(), (uint32)v2.Size(), i2.Data(), (uint32)i2.Size(), true);
		m.RebuildEdges();
		// Le +Z de CHAQUE cube porte le slot 1. Aucun des deux ne touche l'autre.
		uint32 fa = 0, fb = 0;
		float32 da = -2.f, db = -2.f;
		for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f) {
			if (!m.faces[f].alive)
				continue;
			NkVector<NkEmId> lp;
			m.GetFaceVerts(f, lp);
			if (lp.Size() == 0)
				continue;
			const bool droite = m.verts[lp[0]].pos.x > 5.f; // le cube translate
			const float32 d = m.faces[f].normal.Dot({0.f, 0.f, 1.f});
			if (droite && d > db) {
				db = d;
				fb = f;
			}
			if (!droite && d > da) {
				da = d;
				fa = f;
			}
		}
		m.faces[fa].material = 1;
		m.faces[fb].material = 1;

		NkVector<NkVertex3D> ov;
		NkVector<uint32> oi;
		NkVector<NkEditMesh::SubMeshRange> ranges;
		uint32 distinct = 0;
		m.BuildSubMeshRanges(ov, oi, ranges, &distinct);
		uint32 plagesSlot1 = 0, idxSlot1 = 0;
		for (uint32 r = 0; r < (uint32)ranges.Size(); ++r)
			if (ranges[r].material == 1) {
				plagesSlot1++;
				idxSlot1 += ranges[r].indexCount;
			}
		Put("{0:<34} slots={1} plages={2} plages-slot1={3} (2 attendues) indices-slot1={4} total={5}",
				 "mat/sous-mailles-deduites", distinct, (uint32)ranges.Size(), plagesSlot1, idxSlot1,
				 (uint32)oi.Size());
	}

	// ── 3. SURVIE A LA SUBDIVISION LINEAIRE — le cas qui tranche ─────────────
	// Deux subdivisions : chaque face mere donne 4 filles, puis 16. Les deux
	// ilots doivent porter 16 faces slot 1 au total (2 x 4 x ... selon le
	// decoupage), et SURTOUT rester non nuls.
	{
		NkEditMesh m;
		makeTwoIslands(m);
		const uint32 avant = countMat(m, 1);
		NkSubdivideParams p;
		p.cuts = 1;
		m.SelectNone();
		m.SubdivideSelectedFaces(p);
		const uint32 apres1 = countMat(m, 1);
		m.SelectNone();
		m.SubdivideSelectedFaces(p);
		const uint32 apres2 = countMat(m, 1);
		Put("{0:<34} slot1 {1} -> {2} -> {3} | faces={4} (doit croitre, jamais tomber a 0)",
				 "mat/survie-subdivision", avant, apres1, apres2, (uint32)m.faces.Size());
	}

	// ── 4. SURVIE A CATMULL-CLARK ───────────────────────────────────────────
	{
		NkEditMesh m;
		makeTwoIslands(m);
		const uint32 avant = countMat(m, 1);
		m.SubdivideCatmullClark(1);
		Put("{0:<34} slot1 {1} -> {2} | faces={3}", "mat/survie-catmull", avant, countMat(m, 1),
				 (uint32)m.faces.Size());
	}

	// ── 5. SURVIE A L'EXTRUSION ─────────────────────────────────────────────
	{
		NkEditMesh m;
		makeTwoIslands(m);
		const uint32 avant = countMat(m, 1);
		const uint32 facesAvant = (uint32)m.faces.Size();
		// ⚠ SelectAll, pas un `faces[f].sel = 1` pose a la main. Premiere version
		// de ce cas : elle selectionnait la seule face +Z par son drapeau et
		// affichait « slot1 2 -> 2 », donc VERT — alors que le compte de faces
		// n'avait pas bouge d'une unite : l'extrusion n'avait rien fait. Un cas
		// qui reussit parce que l'operation ne s'est pas produite ne prouve rien.
		// D'ou le temoin `faces` dans la signature : il rend le no-op visible.
		m.SelectAll();
		NkExtrudeParams ep;
		ep.offset = 0.5f;
		const bool ok = m.ExtrudeSelectedFaces(ep);
		Put("{0:<34} slot1 {1} -> {2} | faces {3} -> {4} ok={5} (faces DOIVENT croitre)", "mat/survie-extrusion",
				 avant, countMat(m, 1), facesAvant, (uint32)m.faces.Size(), ok ? 1 : 0);
	}

	// ── 6. SURVIE A LA TRIANGULATION ────────────────────────────────────────
	// Les n triangles d'un n-gon doivent TOUS porter l'index de leur face mere :
	// slot 1 sur 2 quads = 4 triangles = 12 indices, jamais 6.
	{
		NkEditMesh m;
		makeTwoIslands(m);
		NkVector<NkVertex3D> ov;
		NkVector<uint32> oi;
		NkVector<NkEditMesh::SubMeshRange> ranges;
		m.BuildSubMeshRanges(ov, oi, ranges, nullptr);
		uint32 idxSlot1 = 0;
		for (uint32 r = 0; r < (uint32)ranges.Size(); ++r)
			if (ranges[r].material == 1)
				idxSlot1 += ranges[r].indexCount;
		Put("{0:<34} indices-slot1={1} (12 attendus : 2 quads -> 4 tris) total={2}", "mat/survie-triangulation",
				 idxSlot1, (uint32)oi.Size());
	}

	// ── 7. AFFECTATION PAR SOMMETS — la regle de Blender ─────────────────────
	// Selectionner les 4 coins d'une face affecte CETTE face ; une face dont un
	// seul coin manque n'est PAS affectee. C'est ce qui rend « un groupe de
	// vertex partage meme material » sans inventer de materiau par sommet.
	{
		NkEditMesh m;
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		m.RebuildEdges();
		const uint32 fz = faceAlong(m, {0.f, 0.f, 1.f});
		NkVector<NkEmId> loop;
		m.GetFaceVerts(fz, loop);
		m.SelectNone();
		for (uint32 k = 0; k < (uint32)loop.Size(); ++k)
			m.verts[loop[k]].sel = 1;
		const uint32 nAffect = m.AssignMaterialToSelectedFaces(2);
		// Puis on RETIRE un coin : plus aucune face ne doit etre entierement
		// couverte par la selection.
		NkEditMesh m2;
		m2.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		m2.RebuildEdges();
		NkVector<NkEmId> loop2;
		m2.GetFaceVerts(faceAlong(m2, {0.f, 0.f, 1.f}), loop2);
		m2.SelectNone();
		for (uint32 k = 1; k < (uint32)loop2.Size(); ++k) // saute le premier coin
			m2.verts[loop2[k]].sel = 1;
		const uint32 nPartiel = m2.AssignMaterialToSelectedFaces(2);
		Put("{0:<34} tousCoins={1} (1 attendu) unCoinManquant={2} (0 attendu)", "mat/assign-par-sommets", nAffect,
				 nPartiel);
	}

	// ── 8. LES SLOTS NE SE RENUMEROTENT JAMAIS ──────────────────────────────
	// Supprimer un slot du milieu laisse un TROU. Si on decalait, toutes les
	// faces d'index superieur changeraient de materiau EN SILENCE.
	{
		NkEditMesh m;
		makeTwoIslands(m);
		m.materialSlots.Clear();
		for (uint32 i = 0; i < 3; ++i) {
			NkEditMesh::MaterialSlot s;
			s.id = 100u + i;
			m.materialSlots.PushBack(s);
		}
		const uint32 fmz = faceAlong(m, {0.f, 0.f, -1.f});
		m.faces[fmz].material = 2;
		m.materialSlots[1].alive = 0; // supprime CELUI DU MILIEU
		Put("{0:<34} slots={1} vivants={2} id[2]={3} (102 attendu) faceMz={4} (2 attendu)",
				 "mat/slots-trou-jamais-decalage", (uint32)m.materialSlots.Size(),
				 (uint32)(m.materialSlots[0].alive + m.materialSlots[1].alive + m.materialSlots[2].alive),
				 m.materialSlots[2].id, (uint32)m.faces[fmz].material);
	}

	// ── 9. LES SLOTS SURVIVENT A UNE OPERATION D'EDITION ─────────────────────
	// Clear() est appele a chaque BuildFromPolygons : si les slots y passaient,
	// la liste des materiaux du maillage disparaitrait a la premiere extrusion.
	{
		NkEditMesh m;
		makeTwoIslands(m);
		NkEditMesh::MaterialSlot s;
		s.id = 777;
		m.materialSlots.PushBack(s);
		const uint32 avant = (uint32)m.materialSlots.Size();
		NkSubdivideParams p;
		p.cuts = 1;
		m.SelectNone();
		m.SubdivideSelectedFaces(p);
		Put("{0:<34} slots {1} -> {2} | dernier id={3} (777 attendu)", "mat/slots-survivent-a-l-edition", avant,
				 (uint32)m.materialSlots.Size(),
				 m.materialSlots.Size() ? m.materialSlots[(uint32)m.materialSlots.Size() - 1].id : 0u);
	}

	// ── 10. DECIMATION : ETAT MESURE, REGLE NON TRANCHEE ────────────────────
	// Quand une contraction fusionne des faces, de qui la survivante herite-t-elle ?
	// La question n'est PAS tranchee (elle demande un arbitrage produit). Cette
	// ligne MESURE le comportement actuel pour qu'un changement se voie ; elle ne
	// le valide pas. Ecrire un attendu ici serait inventer une decision.
	{
		NkEditMesh m;
		makeTwoIslands(m);
		const uint32 avant = countMat(m, 1);
		NkDecimateParams dp;
		dp.targetRatio = 0.5f;
		NkDecimateStats st;
		NkMeshDecimate::DecimateQEM(m, dp, &st);
		Put("{0:<34} slot1 {1} -> {2} | tris {3}->{4} (MESURE, pas un attendu)", "mat/decim-non-tranche", avant,
				 countMat(m, 1), st.trisBefore, st.trisAfter);
	}

	// ── 11. TEMOIN : ce que `smooth` fait DEJA, et qui a dicte la conception ──
	// `smooth` ne traverse PAS le round-trip : BuildFromPolygons cree des Face
	// neuves, donc smooth retombe a 0. Il n'a jamais gene parce que
	// BuildFromIndexed le RE-DEDUIT des normales des coins. Un materiau ne peut
	// pas se re-deduire — d'ou le transport explicite. Cette ligne fige le
	// comportement de smooth pour qu'on voie si quelqu'un le change.
	{
		NkEditMesh m;
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		m.RebuildEdges();
		for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f)
			m.faces[f].smooth = 1;
		uint32 avant = 0;
		for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f)
			if (m.faces[f].smooth)
				avant++;
		NkSubdivideParams p;
		p.cuts = 1;
		m.SelectNone();
		m.SubdivideSelectedFaces(p);
		uint32 apres = 0;
		for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f)
			if (m.faces[f].smooth)
				apres++;
		Put("{0:<34} smooth {1} -> {2} sur {3} faces (TEMOIN du comportement existant)", "mat/temoin-smooth",
				 avant, apres, (uint32)m.faces.Size());
	}

	// ── 12. SURVIE A LA SOUDURE ─────────────────────────────────────────────
	// ⚠ AJOUTEE EN FIN, comme SelOrderBattery et LinkedBattery le font au niveau
	// des batteries : une insertion AU MILIEU decale toutes les lignes suivantes
	// et `--check` les signale comme des divergences alors qu'aucune valeur n'a
	// bouge. Erreur commise puis corrigee ici : 2 fausses divergences.
	// Le cube importe duplique ses coins par face (24 sommets pour 8 positions).
	// Une soudure par distance les fusionne : les faces SURVIVANTES doivent
	// garder leur index. C'est le cas le plus proche d'un import reel, ou le
	// premier geste de l'utilisateur est « Remove Doubles ».
	{
		NkEditMesh m;
		makeTwoIslands(m);
		const uint32 avant = countMat(m, 1);
		const uint32 vAvant = (uint32)m.verts.Size();
		m.SelectAll();
		NkMergeParams mp;
		mp.mode = NkMergeParams::ByDistance;
		mp.distance = 0.001f;
		const bool ok = m.MergeSelectedVerts(mp);
		Put("{0:<34} slot1 {1} -> {2} | sommets {3} -> {4} ok={5}", "mat/survie-soudure", avant, countMat(m, 1),
				 vAvant, (uint32)m.verts.Size(), ok ? 1 : 0);
	}
}

// ── HISTORIQUE ANNULER / REFAIRE DU MAILLAGE ────────────────────────────────
// POURQUOI CETTE BATTERIE EXISTE
// `NkEditHistory` etait, au 2026-08-22, un SYSTEME ENTIER NON PROUVE : aucune de
// ses methodes n'etait appelee par un banc console du depot. Mesure de la
// colonne trois (cf. NKRenderer/ROADMAP.md) : `Commit`, `CanUndo`, `UndoCount`,
// `SetLimit`, `Undo`, `Redo` -- zero couverture.
//
// ⚠️ Le harnais contenait pourtant deja `h.Undo(g)` : c'est l'historique du
// GRAPHE (NkGraph), pas celui du maillage. Un comptage par mot-cle declarait
// donc l'annulation couverte. C'est la raison pour laquelle cette batterie
// existe, et la raison pour laquelle elle nomme ses cas `hist/` et non `undo/`.
//
// CE QUE MESURE LA SIGNATURE
// Un HACHAGE D'ETAT COMPLET (positions, index de materiau, selection, rangs de
// selection) plutot qu'un compte de sommets : une annulation qui restaure le bon
// NOMBRE de sommets aux MAUVAISES positions passerait un comptage.
//
// REGIMES COUVERTS : aller-retour simple et en chaine, pile vide, plafond,
// branche abandonnee, et la survie des attributs (materiau par face, selection,
// ordre de selection). NON COUVERT : la concurrence (l'historique n'est pas
// thread-safe et ne pretend pas l'etre) et le cout memoire des instantanes.
// ⚠️ CES SEPT CAS SAVENT ECHOUER — VERIFIE, PAS SUPPOSE (2026-08-22).
// Une batterie verte du premier coup sur un systeme jamais exerce doit etre
// suspectee avant d'etre crue : c'est « reussir pour la mauvaise raison ».
// Deux defauts ont donc ete injectes dans NkEditHistory, puis retires :
//   • EM_PushCapped jetant le plus RECENT au lieu du plus ancien
//       -> hist/plafond : « x 0.5 -> 0.5 » au lieu de « -> 2.5 »
//   • Commit ne vidant plus la pile de refaire
//       -> hist/branche-abandonnee : « apresNouveauCommit=1 » au lieu de 0,
//          « canRedo=1 » au lieu de 0
// Dans les deux essais, les CINQ autres cas sont restes verts : les cas ne se
// contaminent pas entre eux. Un garde-fou qu'on n'a pas vu echouer ne garde rien.
static void HistoryBattery() {
	NkVector<NkVertex3D> v;
	NkVector<uint32> idx;
	MakeCube(v, idx);

	// Hachage d'ETAT COMPLET. On veut qu'un octet qui bouge se voie : un
	// aller-retour d'annulation doit rendre un etat IDENTIQUE, pas un etat
	// « de la meme taille ».
	auto empreinte = [](const NkEditMesh &m) -> uint64 {
		uint64 h = 1469598103934665603ull; // FNV-1a 64
		auto mange = [&h](const void *p, uint32 n) {
			const unsigned char *b = (const unsigned char *)p;
			for (uint32 i = 0; i < n; ++i) {
				h ^= (uint64)b[i];
				h *= 1099511628211ull;
			}
		};
		for (uint32 i = 0; i < (uint32)m.verts.Size(); ++i) {
			mange(&m.verts[i].pos, (uint32)sizeof(NkVec3f));
			mange(&m.verts[i].sel, 1u);
			mange(&m.verts[i].selOrder, (uint32)sizeof(uint32));
		}
		for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f) {
			mange(&m.faces[f].alive, 1u);
			mange(&m.faces[f].sel, 1u);
			mange(&m.faces[f].material, (uint32)sizeof(uint16));
		}
		return h;
	};

	auto cube = [&](NkEditMesh &m) {
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		m.RebuildEdges();
	};

	// ── 1. ALLER-RETOUR : annuler rend l'etat EXACT, refaire rend le mute ────
	{
		NkEditMesh m;
		cube(m);
		NkEditHistory h;
		const uint64 avant = empreinte(m);
		h.Commit(m); // pre-etat, AVANT la mutation
		NkSubdivideParams p;
		p.cuts = 1;
		m.SelectNone();
		m.SubdivideSelectedFaces(p);
		const uint64 mute = empreinte(m);
		const bool undo = h.Undo(m);
		const uint64 apresUndo = empreinte(m);
		const bool redo = h.Redo(m);
		const uint64 apresRedo = empreinte(m);
		Put("{0:<34} undo={1} redo={2} | retourExact={3} refaitExact={4} muteDifferent={5}", "hist/aller-retour",
				 undo ? 1 : 0, redo ? 1 : 0, apresUndo == avant ? 1 : 0, apresRedo == mute ? 1 : 0,
				 mute != avant ? 1 : 0);
	}

	// ── 2. PILE VIDE : ne ment pas, ne casse rien ────────────────────────────
	{
		NkEditMesh m;
		cube(m);
		NkEditHistory h;
		const uint64 avant = empreinte(m);
		const bool u = h.Undo(m);
		const bool r = h.Redo(m);
		Put("{0:<34} undoVide={1} (0 attendu) redoVide={2} (0 attendu) etatIntact={3} canUndo={4}",
				 "hist/pile-vide", u ? 1 : 0, r ? 1 : 0, empreinte(m) == avant ? 1 : 0, h.CanUndo() ? 1 : 0);
	}

	// ── 3. PLAFOND : jette le PLUS ANCIEN, pas le plus recent ────────────────
	// Le sens compte : jeter le plus recent rendrait l'annulation inutile.
	{
		NkEditMesh m;
		cube(m);
		NkEditHistory h;
		h.SetLimit(3);
		const float32 x0 = m.verts[0].pos.x; // referentiel : sans lui, x final ne prouve rien
		// Cinq mutations distinctes, chacune precedee de son Commit.
		for (int32 k = 0; k < 5; ++k) {
			h.Commit(m);
			m.verts[0].pos.x += 1.f; // mutation minimale et tracable
		}
		const uint32 prof = h.UndoCount();
		// On remonte tout ce qu'on peut : avec un plafond de 3, on doit revenir a
		// x0 + 2 (les deux plus anciens points de retour ont ete jetes), pas a x0.
		// ⚠️ ON COMPTE LES REMONTEES ET ON RAPPELLE x0. Premiere version de ce cas :
		// elle affichait « x=2.5 » sans dire d'ou l'on part ni combien de retours ont
		// eu lieu -- donc invérifiable, et incapable de distinguer « 3 retours sur la
		// bonne pile » de « 2 retours seulement ». Un chiffre sans son referentiel
		// n'est pas une mesure.
		uint32 remontees = 0;
		while (h.CanUndo() && h.Undo(m))
			remontees++;
		Put("{0:<34} profondeur={1} (3 attendue) remontees={2} | x {3:.1f} -> {4:.1f} (x0+2 attendu : 2 plus anciens jetes)",
				 "hist/plafond", prof, remontees, x0, m.verts[0].pos.x);
	}

	// ── 4. BRANCHE ABANDONNEE : un commit apres annulation invalide le refaire ─
	{
		NkEditMesh m;
		cube(m);
		NkEditHistory h;
		h.Commit(m);
		m.verts[0].pos.x += 1.f;
		h.Undo(m);
		const uint32 refaisableAvant = h.RedoCount();
		h.Commit(m); // nouvelle branche
		m.verts[0].pos.y += 1.f;
		Put("{0:<34} refaisableAvant={1} (1 attendu) apresNouveauCommit={2} (0 attendu) canRedo={3}",
				 "hist/branche-abandonnee", refaisableAvant, h.RedoCount(), h.CanRedo() ? 1 : 0);
	}

	// ── 5. LE MATERIAU PAR FACE SURVIT A L'ANNULATION ────────────────────────
	// Le lien entre les deux chantiers de la nuit, et il n'avait jamais ete
	// verifie. L'instantane est une COPIE de NkEditMesh, donc l'index de materiau
	// devrait suivre « par construction » -- mais « par construction » est
	// exactement le genre d'affirmation qui se revele fausse le jour ou un champ
	// est ajoute apres coup. On le mesure au lieu de le supposer.
	{
		NkEditMesh m;
		cube(m);
		uint32 fz = 0;
		float32 meilleur = -2.f;
		for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f) {
			if (!m.faces[f].alive)
				continue;
			const float32 d = m.faces[f].normal.Dot({0.f, 0.f, 1.f});
			if (d > meilleur) {
				meilleur = d;
				fz = f;
			}
		}
		m.faces[fz].material = 7;
		NkEditMesh::MaterialSlot slot;
		slot.id = 4242;
		m.materialSlots.PushBack(slot);

		auto compte7 = [](const NkEditMesh &x) {
			uint32 n = 0;
			for (uint32 f = 0; f < (uint32)x.faces.Size(); ++f)
				if (x.faces[f].alive && x.faces[f].material == 7)
					n++;
			return n;
		};
		const uint32 avant = compte7(m);

		NkEditHistory h;
		h.Commit(m);
		NkSubdivideParams p;
		p.cuts = 1;
		m.SelectNone();
		m.SubdivideSelectedFaces(p);
		const uint32 apresSubdiv = compte7(m);
		h.Undo(m);
		Put("{0:<34} slot7 {1} -> {2} -> {3} (retour a {4} attendu) | slots={5} id={6}", "hist/materiau-survit",
				 avant, apresSubdiv, compte7(m), avant, (uint32)m.materialSlots.Size(),
				 m.materialSlots.Size() ? m.materialSlots[(uint32)m.materialSlots.Size() - 1].id : 0u);
	}

	// ── 6. LA SELECTION ET SON ORDRE SURVIVENT ───────────────────────────────
	// L'ordre de selection n'est pas un detail : « fusionner au premier / au
	// dernier » en depend (famille `ordre/`). Une annulation qui rend la bonne
	// selection dans le mauvais ORDRE casserait ces commandes en silence.
	{
		NkEditMesh m;
		cube(m);
		// La selection passe par SetVertSelection (tableau ENTIER), comme le fait
		// l'editeur : c'est lui qui attribue les rangs de selection.
		NkVector<uint8> f1;
		f1.Resize(m.VertCount());
		for (uint32 i = 0; i < m.VertCount(); ++i)
			f1[i] = (i < 4) ? (uint8)1 : (uint8)0;
		m.SetVertSelection(f1.Data(), (uint32)f1.Size());
		uint32 selAvant = 0, ordreAvant = 0;
		for (uint32 i = 0; i < (uint32)m.verts.Size(); ++i)
			if (m.verts[i].sel) {
				selAvant++;
				ordreAvant += m.verts[i].selOrder;
			}
		NkEditHistory h;
		h.Commit(m);
		NkVector<uint8> f2;
		f2.Resize(m.VertCount());
		for (uint32 i = 0; i < m.VertCount(); ++i)
			f2[i] = (i == 9) ? (uint8)1 : (uint8)0;
		m.SetVertSelection(f2.Data(), (uint32)f2.Size());
		h.Undo(m);
		uint32 selApres = 0, ordreApres = 0;
		for (uint32 i = 0; i < (uint32)m.verts.Size(); ++i)
			if (m.verts[i].sel) {
				selApres++;
				ordreApres += m.verts[i].selOrder;
			}
		Put("{0:<34} selectionnes {1} -> {2} | somme des rangs {3} -> {4} (identiques attendus)",
				 "hist/selection-survit", selAvant, selApres, ordreAvant, ordreApres);
	}

	// ── 7. ANNULATIONS EN CHAINE : trois mutations, trois retours ────────────
	{
		NkEditMesh m;
		cube(m);
		NkEditHistory h;
		const uint64 origine = empreinte(m);
		for (int32 k = 0; k < 3; ++k) {
			h.Commit(m);
			m.verts[(uint32)k].pos.y += 0.5f;
		}
		uint32 remontees = 0;
		while (h.CanUndo() && h.Undo(m))
			remontees++;
		Put("{0:<34} remontees={1} (3 attendues) retourOrigine={2} profondeur={3} refaisables={4}", "hist/chaine",
				 remontees, empreinte(m) == origine ? 1 : 0, h.UndoCount(), h.RedoCount());
	}
}

// ── LES SIX OPERATIONS QUI N'AVAIENT AUCUN BANC ─────────────────────────────
// POURQUOI CETTE BATTERIE EXISTE
// Mesure de la colonne trois (cf. NKRenderer/ROADMAP.md, 2026-08-22) : six
// commandes d'edition de `NkEditMesh` n'etaient appelees par AUCUN banc console
// du depot -- `BisectByPlane`, `DeleteSelectedFaces`, `ExtrudeSelectedVertices`,
// `LoopCutFromSelectedEdge`, `MakeFaceFromSelected`, `SpinSelected`.
//
// ⚠️ `SpinSelected` comptait pourtant comme exercee : le harnais contient bien
// « Spin », mais uniquement en LIAISON DE RACCOURCI CLAVIER
// (`t.Bind("mesh.spin", ...)`). L'operation n'etait jamais appelee. C'est le
// motif « compter des noms au lieu de mesurer des usages » -- chercher le NOM
// d'une capacite et chercher son USAGE donnent deux reponses differentes.
//
// CE QUE MESURE LA SIGNATURE
// Pour chaque operation : son booleen de retour, ET l'effet TOPOLOGIQUE attendu
// (comptes de sommets/faces/aretes, bord, non-manifold). Le booleen seul ne
// prouve rien -- une operation peut rendre `true` sans avoir rien change ; c'est
// exactement le faux vert qui avait ete pris en flagrant delit sur
// `mat/survie-extrusion`. D'ou le temoin « avant -> apres » sur chaque ligne.
//
// REGIMES COUVERTS : le cas nominal de chaque operation, plus son REFUS quand la
// selection ne s'y prete pas. NON COUVERT : la qualite geometrique du resultat
// (planeite du chanfrein, regularite du pas de la vis) -- ces bancs mesurent la
// topologie, pas l'esthetique.
static void SixOpsBattery() {
	NkVector<NkVertex3D> v;
	NkVector<uint32> idx;
	MakeCube(v, idx);
	NkVector<NkVertex3D> gv;
	NkVector<uint32> gi;
	MakeGrid(4, gv, gi);

	auto cube = [&](NkEditMesh &m) {
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		m.RebuildEdges();
	};
	auto grille = [&](NkEditMesh &m) {
		m.BuildFromIndexed(gv.Data(), (uint32)gv.Size(), gi.Data(), (uint32)gi.Size(), true);
		m.RebuildEdges();
	};
	auto facesVivantes = [](const NkEditMesh &m) {
		uint32 n = 0;
		for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f)
			if (m.faces[f].alive)
				n++;
		return n;
	};
	// Selectionne toutes les copies coincidentes d'une position : le cube duplique
	// ses coins par face, donc « cliquer un coin » en touche plusieurs.
	auto selPos = [](NkEditMesh &m, const NkVec3f &p, float32 eps = 1e-5f) {
		NkVector<uint8> f;
		f.Resize(m.VertCount());
		uint32 n = 0;
		for (uint32 i = 0; i < m.VertCount(); ++i) {
			const bool hit = (m.verts[i].pos - p).Len() < eps;
			f[i] = hit ? (uint8)1 : (uint8)0;
			if (hit)
				n++;
		}
		m.SetVertSelection(f.Data(), (uint32)f.Size());
		return n;
	};

	// ── 1. SUPPRIMER LES FACES SELECTIONNEES ────────────────────────────────
	// Supprimer une face d'un cube ouvre un TROU : le bord passe de 0 a 4 aretes.
	// C'est cet invariant qu'on mesure, pas le seul compte de faces.
	{
		NkEditMesh m;
		cube(m);
		const Sig avant = Signature(m);
		// Selectionne la face +Z par ses quatre coins.
		uint32 fz = 0;
		float32 best = -2.f;
		for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f) {
			if (!m.faces[f].alive)
				continue;
			const float32 d = m.faces[f].normal.Dot({0.f, 0.f, 1.f});
			if (d > best) {
				best = d;
				fz = f;
			}
		}
		NkVector<NkEmId> boucle;
		m.GetFaceVerts(fz, boucle);
		NkVector<uint8> fl;
		fl.Resize(m.VertCount());
		for (uint32 i = 0; i < m.VertCount(); ++i)
			fl[i] = 0;
		for (uint32 k = 0; k < (uint32)boucle.Size(); ++k)
			fl[boucle[k]] = 1;
		m.SetVertSelection(fl.Data(), (uint32)fl.Size());
		const bool ok = m.DeleteSelectedFaces();
		m.RebuildEdges();
		const Sig apres = Signature(m);
		Put("{0:<34} ok={1} faces {2} -> {3} | bord {4} -> {5} (un trou s ouvre) nonmanif={6}", "ops/supprimer-faces",
				 ok ? 1 : 0, avant.faces, apres.faces, avant.boundary, apres.boundary, apres.nonManifold);
	}

	// ── 2. SUPPRIMER SANS RIEN DE SELECTIONNE : doit REFUSER ────────────────
	// Le refus compte autant que l'effet : une commande qui « reussit » sur une
	// selection vide detruirait le modele au premier raccourci mal frappe.
	{
		NkEditMesh m;
		cube(m);
		m.SelectNone();
		const uint32 avant = facesVivantes(m);
		const bool ok = m.DeleteSelectedFaces();
		Put("{0:<34} ok={1} (0 attendu) faces {2} -> {3} (inchange attendu)", "ops/supprimer-refus", ok ? 1 : 0,
				 avant, facesVivantes(m));
	}

	// ── 3. EXTRUDER DES SOMMETS -> ARETES FIL ───────────────────────────────
	// L'extrusion de sommet ne cree pas de surface : elle cree des « faces » a
	// deux sommets, les aretes fil de Blender. Le compte de FACES ne doit donc
	// pas bouger comme pour une extrusion de face -- c'est le piege de ce cas.
	{
		NkEditMesh m;
		cube(m);
		const uint32 vAvant = m.VertCount();
		const uint32 fAvant = facesVivantes(m);
		selPos(m, m.verts[0].pos);
		NkExtrudeParams p;
		p.offset = 0.4f;
		const bool ok = m.ExtrudeSelectedVertices(p);
		m.RebuildEdges();
		// ⚠️ ON SEPARE FACES PLEINES ET ARETES FIL. Premiere version de ce cas :
		// elle affichait « faces pleines 6 -> 9 » en comptant TOUTES les faces
		// vivantes -- or l extrusion de sommet ne cree aucune surface, elle cree
		// des faces a DEUX sommets (les aretes fil de Blender). Le libelle mentait
		// sur ce qu il comptait, et c est precisement le piege que le commentaire
		// d en-tete annonce.
		uint32 pleines = 0, fils = 0;
		{
			NkVector<NkEmId> b;
			for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f) {
				if (!m.faces[f].alive)
					continue;
				b.Clear();
				m.GetFaceVerts(f, b);
				if (b.Size() >= 3)
					pleines++;
				else if (b.Size() == 2)
					fils++;
			}
		}
		Put("{0:<34} ok={1} sommets {2} -> {3} | faces pleines {4} -> {5} (INCHANGE attendu) aretes fil={6}",
				 "ops/extruder-sommets", ok ? 1 : 0, vAvant, m.VertCount(), fAvant, pleines, fils);
	}

	// ── 4. FAIRE UNE FACE DEPUIS LA SELECTION (touche F) ────────────────────
	// On supprime une face puis on la RECONSTRUIT depuis les quatre coins du
	// trou : le bord doit revenir a 0. C'est l'aller-retour qui prouve
	// l'operation, pas un simple « +1 face ».
	{
		NkEditMesh m;
		cube(m);
		uint32 fz = 0;
		float32 best = -2.f;
		for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f) {
			if (!m.faces[f].alive)
				continue;
			const float32 d = m.faces[f].normal.Dot({0.f, 0.f, 1.f});
			if (d > best) {
				best = d;
				fz = f;
			}
		}
		NkVector<NkEmId> boucle;
		m.GetFaceVerts(fz, boucle);
		NkVector<NkVec3f> coins;
		for (uint32 k = 0; k < (uint32)boucle.Size(); ++k)
			coins.PushBack(m.verts[boucle[k]].pos);
		NkVector<uint8> fl;
		fl.Resize(m.VertCount());
		for (uint32 i = 0; i < m.VertCount(); ++i)
			fl[i] = 0;
		for (uint32 k = 0; k < (uint32)boucle.Size(); ++k)
			fl[boucle[k]] = 1;
		m.SetVertSelection(fl.Data(), (uint32)fl.Size());
		m.DeleteSelectedFaces();
		m.RebuildEdges();
		const Sig troue = Signature(m);
		// Re-selectionne les memes positions (les indices ont pu bouger) puis F.
		NkVector<uint8> fl2;
		fl2.Resize(m.VertCount());
		for (uint32 i = 0; i < m.VertCount(); ++i) {
			fl2[i] = 0;
			for (uint32 k = 0; k < (uint32)coins.Size(); ++k)
				if ((m.verts[i].pos - coins[k]).Len() < 1e-5f)
					fl2[i] = 1;
		}
		m.SetVertSelection(fl2.Data(), (uint32)fl2.Size());
		const bool ok = m.MakeFaceFromSelected();
		m.RebuildEdges();
		const Sig refait = Signature(m);
		Put("{0:<34} ok={1} faces {2} -> {3} | bord {4} -> {5} (retour a 0 attendu)", "ops/faire-face", ok ? 1 : 0,
				 troue.faces, refait.faces, troue.boundary, refait.boundary);
	}

	// ── 5. COUPE DE BOUCLE (Ctrl+R) ─────────────────────────────────────────
	// Sur une grille de quads, inserer une boucle ajoute des sommets ET des
	// faces sans ouvrir de bord ni creer de non-manifold.
	{
		NkEditMesh m;
		grille(m);
		const Sig avant = Signature(m);
		// Une arete interieure : ses DEUX extremites selectionnees.
		bool pose = false;
		for (uint32 h = 0; h < (uint32)m.hedges.Size() && !pose; ++h) {
			const uint32 o = m.hedges[h].origin;
			const uint32 d = m.hedges[m.hedges[h].next].origin;
			if (o >= m.VertCount() || d >= m.VertCount())
				continue;
			NkVector<uint8> fl;
			fl.Resize(m.VertCount());
			for (uint32 i = 0; i < m.VertCount(); ++i)
				fl[i] = (i == o || i == d) ? (uint8)1 : (uint8)0;
			m.SetVertSelection(fl.Data(), (uint32)fl.Size());
			pose = true;
		}
		NkLoopCutParams p;
		p.cuts = 1;
		const bool ok = m.LoopCutFromSelectedEdge(p);
		m.RebuildEdges();
		const Sig apres = Signature(m);
		Put("{0:<34} ok={1} sommets {2} -> {3} faces {4} -> {5} | nonmanif={6} bord {7} -> {8}", "ops/coupe-de-boucle",
				 ok ? 1 : 0, avant.verts, apres.verts, avant.faces, apres.faces, apres.nonManifold, avant.boundary,
				 apres.boundary);
	}

	// ── 6. VIS / REVOLUTION (SpinSelected) ──────────────────────────────────
	// Une revolution de 360 degres en 12 pas autour de Y, sur une grille : elle
	// doit multiplier la geometrie. `duplicate=false` -> la source est deplacee.
	{
		NkEditMesh m;
		grille(m);
		m.SelectAll();
		const Sig avant = Signature(m);
		NkSpinParams p;
		// ⚠️ AXE DECALE, ET C EST LE FOND DU CAS. Premiere version : centre a
		// l origine, donc l axe TRAVERSAIT la grille -- la surface balayee se
		// recoupait elle-meme et rendait 88 aretes non-manifold. Ce chiffre ne
		// disait rien du code : il disait que la FIGURE etait mal choisie. Un tour
		// de potier tourne autour d un axe EXTERIEUR au profil.
		p.center = {-3.f, 0.f, 0.f};
		p.axis = {0.f, 1.f, 0.f};
		p.angle = 3.14159265f; // demi-tour : evite le recouvrement exact du tour complet
		p.steps = 6;
		p.duplicate = true;
		const bool ok = m.SpinSelected(p, NkMat4f::Identity());
		m.RebuildEdges();
		const Sig apres = Signature(m);
		Put("{0:<34} ok={1} sommets {2} -> {3} faces {4} -> {5} | nonmanif {6} -> {7} (temoin)", "ops/vis-revolution",
				 ok ? 1 : 0, avant.verts, apres.verts, avant.faces, apres.faces, avant.nonManifold,
				 apres.nonManifold);
	}

	// ── 7. COUPE PAR UN PLAN (Bisect) ───────────────────────────────────────
	// Le plan Y=0 traverse le cube en son milieu : la coupe doit AJOUTER des
	// sommets sur l'intersection, sans ouvrir le volume.
	{
		NkEditMesh m;
		cube(m);
		m.SelectAll();
		const Sig avant = Signature(m);
		const bool ok = m.BisectByPlane({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, NkMat4f::Identity());
		m.RebuildEdges();
		const Sig apres = Signature(m);
		// ⚠️ LE NON-MANIFOLD EST DONNE AVANT ET APRES. Sans son temoin, « nonmanif=18 »
		// ne dit pas si la coupe l a INTRODUIT ou s il etait deja la -- et la
		// signature mesure sur l IDENTITE SOUDEE, ou des sommets coincidents crees
		// par la coupe peuvent confondre deux aretes distinctes. Le chiffre est
		// donc une OBSERVATION a instruire, pas un verdict sur BisectByPlane.
		Put("{0:<34} ok={1} sommets {2} -> {3} faces {4} -> {5} | bord {6} -> {7} nonmanif {8} -> {9}",
				 "ops/coupe-par-plan", ok ? 1 : 0, avant.verts, apres.verts, avant.faces, apres.faces,
				 avant.boundary, apres.boundary, avant.nonManifold, apres.nonManifold);
	}
}

// ── LA CONVENTION D'ENROULEMENT, MESUREE SANS GPU ───────────────────────────
// POURQUOI CE CAS EXISTE
// Le 2026-08-22, un arbitrage demandant `frontFace = CW` dans tout le moteur a
// ete RETIRE : l'agent rendu avait deduit une convention d'un « cull=FRONT et
// cull=NONE donnent la meme image », et la vraie cause etait un retournement
// vertical manquant dans le generateur GLSL. Il compensait un flip absent par une
// fausse convention. La convention d'enroulement du depot a donc ete declaree
// « question ouverte ».
//
// ⚠️ ELLE NE L'EST PAS COTE CPU, ET CE CAS LE PROUVE SANS ALLUMER DE GPU.
// La question « quelle normale le noyau calcule-t-il pour une boucle donnee ? »
// se tranche par arithmetique, pas par capture d'ecran. Aucun shader, aucun flip,
// aucun etat de rasterisation n'intervient dans NkEmFaceCross.
//
// LA MESURE : on compare la normale que NkEditMesh CALCULE pour chaque face du
// cube a la normale DECLAREE par la primitive dans ses propres donnees. Si les
// deux coincident, la convention du noyau est celle des primitives -- et le
// produit vectoriel CCW standard, lui, donne l'oppose.
//
// ⚠️ Ce cas ne dit RIEN de l'etat de rasterisation a declarer (`frontFace`) :
// c'est une question de rendu, elle depend du GPU et du generateur de shaders, et
// elle reste ouverte. Confondre les deux est exactement l'erreur qui a coute
// l'arbitrage retire. Un banc doit dire ce qu'il ne couvre pas.
static void WindingBattery() {
	NkVector<NkVertex3D> v;
	NkVector<uint32> idx;
	MakeCube(v, idx);

	NkEditMesh m;
	m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
	m.RebuildEdges();

	// Pour chaque face vivante : la normale calculee par le noyau, contre la
	// normale portee par les sommets source (la primitive declare la sienne).
	uint32 accord = 0, desaccord = 0;
	float32 pireEcart = 0.f;
	NkVector<NkEmId> boucle;
	for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f) {
		if (!m.faces[f].alive)
			continue;
		boucle.Clear();
		m.GetFaceVerts(f, boucle);
		if (boucle.Size() < 3)
			continue;
		// Normale DECLAREE : celle du premier coin (la primitive donne la meme a
		// tous les coins d'une face plate).
		const NkVec3f declaree = m.verts[boucle[0]].normal;
		const NkVec3f calculee = m.faces[f].normal;
		const float32 d = declaree.Dot(calculee);
		if (d > 0.9f)
			accord++;
		else
			desaccord++;
		const float32 ecart = 1.f - (d < 0.f ? -d : d);
		if (ecart > pireEcart)
			pireEcart = ecart;
	}

	// Le produit vectoriel CCW standard, calcule ICI a la main sur une face
	// connue : il doit donner l'OPPOSE de ce que le noyau calcule.
	//   face du dessus du cube : boucle {3,7,6,2}, normale declaree (0,1,0)
	const NkVec3f p3 = {-0.5f, 0.5f, 0.5f};
	const NkVec3f p7 = {-0.5f, 0.5f, -0.5f};
	const NkVec3f p6 = {0.5f, 0.5f, -0.5f};
	const NkVec3f u = p7 - p3;
	const NkVec3f w = p6 - p3;
	const NkVec3f ccw = {u.y * w.z - u.z * w.y, u.z * w.x - u.x * w.z, u.x * w.y - u.y * w.x};
	// ccw.y < 0 alors que la primitive declare +Y => face avant = SENS HORAIRE.

	Put("{0:<34} accord={1} desaccord={2} (0 attendu) pire ecart={3:.4f} | CCW sur face +Y = {4:.1f} (negatif "
		"attendu)",
		"enroulement/noyau-contre-primitive", accord, desaccord, pireEcart, ccw.y);
}

// ── SURVIE DU MATERIAU A TRAVERS LES AUTRES OPERATIONS ──────────────────────
// Le materiau par face traverse deja la subdivision lineaire, Catmull-Clark,
// l'extrusion de faces et la soudure (famille `mat/`). Onze operations restaient
// non cablees : toutes passent par le round-trip ToPolygons/BuildFromPolygons,
// qui reconstruit les `Face` a neuf et perd donc tout ce qu'elles portaient.
//
// Cette batterie mesure la survie a travers celles qui ont deja un banc de
// topologie (famille `ops/`), donc celles dont on peut verifier que l'operation
// a REELLEMENT eu lieu -- sans quoi un « materiau conserve » ne prouverait que
// l'inaction.
//
// ⚠️ CHAQUE LIGNE PORTE SON TEMOIN D'ACTIVITE (faces avant -> apres, ou sommets).
// Une operation qui ne fait rien conserve trivialement le materiau : c'est le
// faux vert deja pris en flagrant delit sur `mat/survie-extrusion`.
static void MatSurvieBattery() {
	NkVector<NkVertex3D> v;
	NkVector<uint32> idx;
	MakeCube(v, idx);
	NkVector<NkVertex3D> gv;
	NkVector<uint32> gi;
	MakeGrid(4, gv, gi);

	auto compte = [](const NkEditMesh &m, uint16 slot) {
		uint32 n = 0;
		for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f)
			if (m.faces[f].alive && m.faces[f].material == slot)
				n++;
		return n;
	};
	auto vivantes = [](const NkEditMesh &m) {
		uint32 n = 0;
		for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f)
			if (m.faces[f].alive)
				n++;
		return n;
	};
	// Pose le slot 1 sur TOUTES les faces vivantes : on veut voir si l'operation
	// ramene des faces au slot 0, pas suivre une face en particulier.
	auto peindre = [&](NkEditMesh &m) {
		for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f)
			if (m.faces[f].alive)
				m.faces[f].material = 1;
	};
	auto cube = [&](NkEditMesh &m) {
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		m.RebuildEdges();
	};
	auto grille = [&](NkEditMesh &m) {
		m.BuildFromIndexed(gv.Data(), (uint32)gv.Size(), gi.Data(), (uint32)gi.Size(), true);
		m.RebuildEdges();
	};
	auto faceVers = [](const NkEditMesh &m, NkVec3f dir) {
		uint32 best = 0;
		float32 bd = -2.f;
		for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f) {
			if (!m.faces[f].alive)
				continue;
			const float32 d = m.faces[f].normal.Dot(dir);
			if (d > bd) {
				bd = d;
				best = f;
			}
		}
		return best;
	};
	auto selFace = [](NkEditMesh &m, uint32 f) {
		NkVector<NkEmId> b;
		m.GetFaceVerts(f, b);
		NkVector<uint8> fl;
		fl.Resize(m.VertCount());
		for (uint32 i = 0; i < m.VertCount(); ++i)
			fl[i] = 0;
		for (uint32 k = 0; k < (uint32)b.Size(); ++k)
			fl[b[k]] = 1;
		m.SetVertSelection(fl.Data(), (uint32)fl.Size());
	};

	// ── 1. SUPPRESSION DE FACES ─────────────────────────────────────────────
	{
		NkEditMesh m;
		cube(m);
		peindre(m);
		const uint32 av = compte(m, 1), fav = vivantes(m);
		selFace(m, faceVers(m, {0.f, 0.f, 1.f}));
		m.DeleteSelectedFaces();
		m.RebuildEdges();
		Put("{0:<34} slot1 {1} -> {2} | faces {3} -> {4} (l operation a bien eu lieu)", "matops/supprimer-faces",
			av, compte(m, 1), fav, vivantes(m));
	}

	// ── 2. FAIRE UNE FACE (touche F) ────────────────────────────────────────
	// La face NEUVE ne peut hériter de personne : elle prend le slot 0. Ce qui
	// doit survivre, ce sont les AUTRES.
	{
		NkEditMesh m;
		cube(m);
		peindre(m);
		const uint32 av = compte(m, 1);
		const uint32 f0 = faceVers(m, {0.f, 0.f, 1.f});
		NkVector<NkEmId> b;
		m.GetFaceVerts(f0, b);
		NkVector<NkVec3f> coins;
		for (uint32 k = 0; k < (uint32)b.Size(); ++k)
			coins.PushBack(m.verts[b[k]].pos);
		selFace(m, f0);
		m.DeleteSelectedFaces();
		m.RebuildEdges();
		NkVector<uint8> fl;
		fl.Resize(m.VertCount());
		for (uint32 i = 0; i < m.VertCount(); ++i) {
			fl[i] = 0;
			for (uint32 k = 0; k < (uint32)coins.Size(); ++k)
				if ((m.verts[i].pos - coins[k]).Len() < 1e-5f)
					fl[i] = 1;
		}
		m.SetVertSelection(fl.Data(), (uint32)fl.Size());
		const bool ok = m.MakeFaceFromSelected();
		m.RebuildEdges();
		Put("{0:<34} slot1 {1} -> {2} (5 attendu) | slot0={3} (1 : la face neuve) ok={4}", "matops/faire-face", av,
			compte(m, 1), compte(m, 0), ok ? 1 : 0);
	}

	// ── 3. EXTRUSION DE SOMMETS ─────────────────────────────────────────────
	{
		NkEditMesh m;
		cube(m);
		peindre(m);
		const uint32 av = compte(m, 1), vav = m.VertCount();
		NkVector<uint8> fl;
		fl.Resize(m.VertCount());
		for (uint32 i = 0; i < m.VertCount(); ++i)
			fl[i] = ((m.verts[i].pos - m.verts[0].pos).Len() < 1e-5f) ? (uint8)1 : (uint8)0;
		m.SetVertSelection(fl.Data(), (uint32)fl.Size());
		NkExtrudeParams p;
		p.offset = 0.4f;
		m.ExtrudeSelectedVertices(p);
		m.RebuildEdges();
		Put("{0:<34} slot1 {1} -> {2} | sommets {3} -> {4}", "matops/extruder-sommets", av, compte(m, 1), vav,
			m.VertCount());
	}

	// ── 4. COUPE DE BOUCLE ──────────────────────────────────────────────────
	{
		NkEditMesh m;
		grille(m);
		peindre(m);
		const uint32 av = compte(m, 1), fav = vivantes(m);
		for (uint32 h = 0; h < (uint32)m.hedges.Size(); ++h) {
			const uint32 o = m.hedges[h].origin;
			const uint32 d = m.hedges[m.hedges[h].next].origin;
			if (o >= m.VertCount() || d >= m.VertCount())
				continue;
			NkVector<uint8> fl;
			fl.Resize(m.VertCount());
			for (uint32 i = 0; i < m.VertCount(); ++i)
				fl[i] = (i == o || i == d) ? (uint8)1 : (uint8)0;
			m.SetVertSelection(fl.Data(), (uint32)fl.Size());
			break;
		}
		NkLoopCutParams p;
		p.cuts = 1;
		m.LoopCutFromSelectedEdge(p);
		m.RebuildEdges();
		Put("{0:<34} slot1 {1} -> {2} | faces {3} -> {4}", "matops/coupe-de-boucle", av, compte(m, 1), fav,
			vivantes(m));
	}

	// ── 5. COUPE PAR UN PLAN ────────────────────────────────────────────────
	{
		NkEditMesh m;
		cube(m);
		peindre(m);
		const uint32 av = compte(m, 1), fav = vivantes(m);
		m.SelectAll();
		m.BisectByPlane({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, NkMat4f::Identity());
		m.RebuildEdges();
		Put("{0:<34} slot1 {1} -> {2} | faces {3} -> {4}", "matops/coupe-par-plan", av, compte(m, 1), fav,
			vivantes(m));
	}

	// ── 6. VIS / REVOLUTION ─────────────────────────────────────────────────
	{
		NkEditMesh m;
		grille(m);
		peindre(m);
		const uint32 av = compte(m, 1), fav = vivantes(m);
		m.SelectAll();
		NkSpinParams p;
		p.center = {-3.f, 0.f, 0.f};
		p.axis = {0.f, 1.f, 0.f};
		p.angle = 3.14159265f;
		p.steps = 6;
		p.duplicate = true;
		m.SpinSelected(p, NkMat4f::Identity());
		m.RebuildEdges();
		Put("{0:<34} slot1 {1} -> {2} | faces {3} -> {4}", "matops/vis-revolution", av, compte(m, 1), fav,
			vivantes(m));
	}

	// ── 7. DEFORMATION PURE (shrink/fatten) ─────────────────────────────────
	// Elle ne change AUCUNE face : le materiau doit survivre a l'identique, et
	// le compte de faces doit rester constant -- c'est ce dernier qui prouve
	// qu'on mesure bien une deformation et non une refonte topologique.
	{
		NkEditMesh m;
		cube(m);
		peindre(m);
		const uint32 av = compte(m, 1), fav = vivantes(m);
		m.SelectAll();
		NkShrinkFattenParams p;
		p.offset = 0.2f;
		// ⚠️ ON AFFICHE `ok`, ET C EST TOUT L INTERET DE CETTE LIGNE. Premiere
		// version de ce cas : elle annonçait « slot1 6 -> 6 » et passait pour
		// verte -- alors que sur un cube SelectAll, ShrinkFatten est REFUSE
		// (la reference le dit depuis toujours : « cube/selectall+shrinkfatten
		// [REFUSE] »). Le materiau etait conserve parce que RIEN N AVAIT EU LIEU.
		// C est exactement le faux vert deja pris sur mat/survie-extrusion, et je
		// l ai reproduit moi-meme deux jours plus tard.
		const bool ok = m.ShrinkFattenSelected(p);
		m.RebuildEdges();
		Put("{0:<34} ok={1} slot1 {2} -> {3} | faces {4} -> {5} (si ok=0, ce cas ne prouve RIEN)",
			"matops/deformation-pure", ok ? 1 : 0, av, compte(m, 1), fav, vivantes(m));
	}
	// ⚠️ AJOUTEES EN FIN, et j'ai du les DEPLACER pour ca. Premiere version :
	// inserees avant le cas 7, elles decalaient toutes les lignes suivantes et
	// `--check` signalait une divergence de VALEUR sur `matops/deformation-pure`
	// alors qu'aucune valeur n'avait bouge. Meme erreur que `mat/survie-soudure`
	// la veille : dans ce fichier, un cas neuf va TOUJOURS a la fin.
	// ── 8 a 12. LES CINQ DERNIERES OPERATIONS ───────────────────────────────
	// Chacune est deja exercee par la batterie principale (familles `cube/` et
	// `grille4/`), donc on sait qu'elles fonctionnent. Ce qu'on ajoute ici est la
	// seule chose qu'aucune ne mesurait : le materiau survit-il ?
	// Chaque ligne porte `ok` ET le compte de faces : sans les deux, un « slot1
	// conserve » peut vouloir dire « l'operation a ete refusee ».
	{
		struct Cas {
				const char *nom;
				int32 quoi; // 0 bevel, 1 inset, 2 split, 3 dissolve, 4 extrude-aretes
		};
		const Cas cs[5] = {{"matops/chanfrein", 0},
						   {"matops/inset", 1},
						   {"matops/split-aretes", 2},
						   {"matops/dissolve", 3},
						   {"matops/extruder-aretes", 4}};
		for (int32 c = 0; c < 5; ++c) {
			NkEditMesh m;
			// La grille pour dissolve (refuse sur un cube ferme), le cube sinon.
			if (cs[c].quoi == 3)
				grille(m);
			else
				cube(m);
			peindre(m);
			const uint32 av = compte(m, 1), fav = vivantes(m);
			m.SelectAll();
			bool ok = false;
			if (cs[c].quoi == 0) {
				NkBevelParams p;
				p.segments = 1;
				ok = m.BevelSelected(p);
			} else if (cs[c].quoi == 1) {
				NkInsetParams p;
				p.thickness = 0.1f;
				ok = m.InsetSelectedFaces(p);
			} else if (cs[c].quoi == 2) {
				NkEdgeSplitParams p;
				ok = m.SplitSelectedEdges(p);
			} else if (cs[c].quoi == 3) {
				NkDissolveParams p;
				ok = m.DissolveSelected(p);
			} else {
				NkExtrudeParams p;
				p.offset = 0.2f;
				ok = m.ExtrudeSelectedEdges(p);
			}
			m.RebuildEdges();
			Put("{0:<34} ok={1} slot1 {2} -> {3} | faces {4} -> {5}", cs[c].nom, ok ? 1 : 0, av, compte(m, 1), fav,
				vivantes(m));
		}
	}
}

// ── LES QUATRE CHARGEURS QUI N AVAIENT AUCUN BANC ───────────────────────────
// POURQUOI CETTE BATTERIE EXISTE
// Mesure de la colonne trois (cf. NKRenderer/ROADMAP.md) : OBJ, glTF et FBX sont
// couverts par NkAssetIODemo et NkFBXParityDemo. **PLY, STL, DAE et USDA ne
// l etaient par aucun banc du depot** -- quatre chargeurs livres, jamais exerces.
//
// CE QUE MESURE LA SIGNATURE
// Le compte de sommets et de triangles, plus la BOITE ENGLOBANTE. Le compte seul
// ne suffit pas : un chargeur qui lit les bons NOMBRES aux mauvaises POSITIONS
// (permutation d axes, echelle, boutisme) passerait un comptage. La bbox attrape
// ces trois-la.
//
// ⚠️ LA FIGURE EST UN CUBE, ET C EST CE QUI REND LE BANC LISIBLE : sa bbox doit
// etre un cube centre. Une bbox non cubique denonce immediatement une echelle ou
// un axe permute, sans qu on ait besoin de connaitre le fichier.
//
// ⚠️ PLY et STL sont exerces en ASCII **ET** en BINAIRE, et la ligne compare les
// deux entre eux : deux encodages du meme cube doivent donner la MEME geometrie.
// C est le controle le plus fort de cette batterie, parce qu il ne depend
// d aucune valeur ecrite a la main -- il compare le chargeur a lui-meme.
//
// REGIMES COUVERTS : lecture de fichiers presents, et refus propre sur fichier
// absent. NON COUVERT : les materiaux et les hierarchies de ces formats (DAE et
// USDA en portent ; ce banc ne regarde que la geometrie), et les gros fichiers.
// ⚠️ LE BANC NE DOIT PAS DEPENDRE DU REPERTOIRE DE LANCEMENT.
// Constate ici meme : lance depuis `Applications/NKEditMeshHarness` (le dossier
// ou vit `editmesh_baseline.txt`, donc celui d'ou l'on fait `--check`), les six
// chargeurs rendaient `ok=0` sur toute la ligne -- non parce qu'ils sont casses,
// mais parce que `Resources/` se resout depuis la RACINE du worktree. Le banc a
// donc besoin des DEUX repertoires a la fois, et c'est insoluble par le seul
// repertoire courant.
// C'est la meme dette que celle deja nommee pour les shaders dans le CLAUDE.md
// parent : deux politiques de chemin dans un meme programme.
// Remede local : on essaie le chemin tel quel, puis en remontant de deux crans.
// Un banc muet parce qu'il a ete lance d'ailleurs est pire qu'un banc absent --
// il rend `ok=0` et ressemble a un chargeur casse.
static NkString CheminRessource(const char *relatif) {
	const char *prefixes[3] = {"", "../../", "../../../"};
	for (int32 i = 0; i < 3; ++i) {
		NkString essai = NkString(prefixes[i]) + NkString(relatif);
		FILE *f = fopen(essai.Data(), "rb");
		if (f) {
			fclose(f);
			return essai;
		}
	}
	return NkString(relatif); // aucun trouve : on rend l'original, le chargeur dira non
}

static void LoadersBattery() {
	struct Cas {
			const char *nom;
			const char *chemin;
			int32 type; // 0 PLY, 1 STL, 2 DAE, 3 USDA
	};
	const Cas cs[6] = {
		{"chargeurs/ply-ascii", "Resources/Models/test/cube_ascii.ply", 0},
		{"chargeurs/ply-binaire", "Resources/Models/test/cube_bin.ply", 0},
		{"chargeurs/stl-ascii", "Resources/Models/test/cube_ascii.stl", 1},
		{"chargeurs/stl-binaire", "Resources/Models/test/cube_bin.stl", 1},
		{"chargeurs/dae", "Resources/Models/test/cube.dae", 2},
		{"chargeurs/usda", "Resources/Models/test/cube.usda", 3},
	};

	// Empreintes gardees pour la comparaison ASCII <-> binaire.
	uint32 vPly[2] = {0, 0}, tPly[2] = {0, 0};
	uint32 vStl[2] = {0, 0}, tStl[2] = {0, 0};
	float32 dPly[2] = {0.f, 0.f}, dStl[2] = {0.f, 0.f};

	for (int32 c = 0; c < 6; ++c) {
		renderer::NkGLTFMeshData d;
		bool ok = false;
		if (cs[c].type == 0)
			ok = renderer::LoadPLY(CheminRessource(cs[c].chemin), d);
		else if (cs[c].type == 1)
			ok = renderer::LoadSTL(CheminRessource(cs[c].chemin), d);
		else if (cs[c].type == 2)
			ok = renderer::LoadDAE(CheminRessource(cs[c].chemin), d);
		else
			ok = renderer::LoadUSDA(CheminRessource(cs[c].chemin), d);

		const uint32 nv = (uint32)d.vertices.Size();
		const uint32 nt = (uint32)d.indices.Size() / 3u;
		// Boite englobante : c est elle qui attrape un axe permute ou une echelle.
		float32 mnx = 1e30f, mny = 1e30f, mnz = 1e30f;
		float32 mxx = -1e30f, mxy = -1e30f, mxz = -1e30f;
		for (uint32 i = 0; i < nv; ++i) {
			const NkVec3f &p = d.vertices[i].pos;
			if (p.x < mnx)
				mnx = p.x;
			if (p.y < mny)
				mny = p.y;
			if (p.z < mnz)
				mnz = p.z;
			if (p.x > mxx)
				mxx = p.x;
			if (p.y > mxy)
				mxy = p.y;
			if (p.z > mxz)
				mxz = p.z;
		}
		const float32 dx = (nv ? mxx - mnx : 0.f), dy = (nv ? mxy - mny : 0.f), dz = (nv ? mxz - mnz : 0.f);
		// « Cubique » : les trois cotes egaux a 1 % pres. Une RELATION entre les
		// trois dimensions, pas une valeur en dur -- le banc reste juste si
		// quelqu un remplace le cube de test par un cube d une autre taille.
		const float32 mx = (dx > dy ? (dx > dz ? dx : dz) : (dy > dz ? dy : dz));
		const float32 mn = (dx < dy ? (dx < dz ? dx : dz) : (dy < dz ? dy : dz));
		const int32 cubique = (mx > 1e-6f && (mx - mn) / mx < 0.01f) ? 1 : 0;

		if (cs[c].type == 0) {
			vPly[c] = nv;
			tPly[c] = nt;
			dPly[c] = dx;
		} else if (cs[c].type == 1) {
			vStl[c - 2] = nv;
			tStl[c - 2] = nt;
			dStl[c - 2] = dx;
		}

		Put("{0:<34} ok={1} sommets={2} triangles={3} | bbox {4:.3f}x{5:.3f}x{6:.3f} cubique={7}", cs[c].nom,
			ok ? 1 : 0, nv, nt, dx, dy, dz, cubique);
	}

	// ── LA COMPARAISON QUI NE DEPEND D AUCUNE VALEUR ECRITE A LA MAIN ────────
	// ⚠️ `> 0` EN PLUS DE L EGALITE, ET CE N EST PAS DU ZELE. Premiere version :
	// elle testait `vPly[0] == vPly[1]` seul, et annoncait « identiques=1 » alors
	// que les DEUX valaient zero -- les fichiers n etaient pas trouves. Un
	// controle d egalite sans controle d existence reussit toujours sur du vide.
	// ASCII et binaire encodent le MEME cube : le chargeur doit rendre la meme
	// geometrie. Si les deux divergent, l un des deux chemins est faux -- et on
	// le sait sans avoir eu besoin de connaitre le bon compte.
	Put("{0:<34} PLY ascii/bin sommets {1}/{2} tris {3}/{4} identiques={5} | cote {6:.3f}/{7:.3f}",
		"chargeurs/ply-ascii-vs-binaire", vPly[0], vPly[1], tPly[0], tPly[1],
		(vPly[0] > 0u && vPly[0] == vPly[1] && tPly[0] == tPly[1]) ? 1 : 0, dPly[0], dPly[1]);
	Put("{0:<34} STL ascii/bin sommets {1}/{2} tris {3}/{4} identiques={5} | cote {6:.3f}/{7:.3f}",
		"chargeurs/stl-ascii-vs-binaire", vStl[0], vStl[1], tStl[0], tStl[1],
		(vStl[0] > 0u && vStl[0] == vStl[1] && tStl[0] == tStl[1]) ? 1 : 0, dStl[0], dStl[1]);

	// ── REFUS PROPRE SUR FICHIER ABSENT ─────────────────────────────────────
	// Un chargeur qui rend `true` sur un fichier inexistant ferait passer un
	// maillage VIDE pour un import reussi.
	{
		renderer::NkGLTFMeshData a, b, e, u;
		const int32 rp = renderer::LoadPLY(NkString("Resources/Models/test/_absent_.ply"), a) ? 1 : 0;
		const int32 rs = renderer::LoadSTL(NkString("Resources/Models/test/_absent_.stl"), b) ? 1 : 0;
		const int32 rd = renderer::LoadDAE(NkString("Resources/Models/test/_absent_.dae"), e) ? 1 : 0;
		const int32 ru = renderer::LoadUSDA(NkString("Resources/Models/test/_absent_.usda"), u) ? 1 : 0;
		Put("{0:<34} ply={1} stl={2} dae={3} usda={4} (0 attendu partout)", "chargeurs/refus-fichier-absent", rp,
			rs, rd, ru);
	}
}

// -- ACCESSEURS, PREDICATS TOPOLOGIQUES ET LES TROIS SYSTEMES VOISINS --------
// Ce que couvre cette batterie : les 19 methodes publiques que les six bancs
// n'appelaient pas. Elles sont pour l'essentiel des LECTURES -- et c'est
// precisement pourquoi elles etaient restees dehors : un accesseur ne casse
// rien de visible quand il ment, il fait mentir celui qui s'en sert.
//
// DEUX REGLES APPLIQUEES A CHAQUE CAS.
//  1. Pour une operation, son booleen de retour ET son effet observable --
//     jamais l'un des deux seul (`MoveDown`, `ReplayOnto`).
//  2. Pour un predicat, une forme ou il repond OUI et une ou il repond NON.
//     Un predicat teste sur un seul cas ne prouve pas qu'il discrimine : la
//     fonction qui rend toujours `true` passerait.
static void AccessBattery() {
	NkVector<NkVertex3D> v;
	NkVector<uint32> idx;

	// -- 1. LES QUATRE CLASSES D'ARETE : une PARTITION, pas quatre comptes ----
	// L'invariant qui tient a toute taille de maillage : chaque arete vivante
	// tombe dans EXACTEMENT une classe. Un cube ferme n'a que du manifold ;
	// une grille ouverte a des bords. Les deux formes ensemble prouvent que les
	// predicats discriminent.
	for (int32 forme = 0; forme < 2; ++forme) {
		if (forme == 0)
			MakeCube(v, idx);
		else
			MakeGrid(3, v, idx);
		NkEditMesh m;
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		m.RebuildEdges();
		uint32 vivantes = 0, fil = 0, bord = 0, manif = 0, nonmanif = 0;
		for (uint32 e = 0; e < (uint32)m.edges.Size(); ++e) {
			if (!m.edges[e].alive)
				continue;
			vivantes++;
			if (m.EdgeIsWire((NkEmId)e))
				fil++;
			if (m.EdgeIsBoundary((NkEmId)e))
				bord++;
			if (m.EdgeIsManifold((NkEmId)e))
				manif++;
			if (m.EdgeIsNonManifold((NkEmId)e))
				nonmanif++;
		}
		const int32 partition = ((fil + bord + manif + nonmanif) == vivantes) ? 1 : 0;
		Put("{0:<34} vivantes={1} fil={2} bord={3} manif={4} nonmanif={5} | partition exacte={6}",
			forme == 0 ? "acces/classes-arete-cube" : "acces/classes-arete-grille", vivantes, fil, bord, manif,
			nonmanif, partition);
	}

	// -- 2. ALLER-RETOUR DEMI-ARETE <-> ARETE --------------------------------
	// `EdgeHedges` rend le cycle radial, `EdgeOfHedge` fait le chemin inverse.
	// Le controle ne porte AUCUN nombre ecrit a la main : pour chaque arete, le
	// nombre de demi-aretes rendues doit valoir son `RadialCount`, et chacune
	// doit renvoyer a l'arete dont elle vient. C'est vrai a 12 aretes comme a
	// 12 000.
	{
		MakeCube(v, idx);
		NkEditMesh m;
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		m.RebuildEdges();
		uint32 aretes = 0, ecartCompte = 0, ecartRetour = 0;
		NkVector<NkEmId> hs;
		for (uint32 e = 0; e < (uint32)m.edges.Size(); ++e) {
			if (!m.edges[e].alive)
				continue;
			aretes++;
			hs.Clear();
			const uint32 n = m.EdgeHedges((NkEmId)e, hs);
			if (n != m.RadialCount((NkEmId)e) || n != (uint32)hs.Size())
				ecartCompte++;
			for (uint32 k = 0; k < (uint32)hs.Size(); ++k)
				if (m.EdgeOfHedge(hs[k]) != (NkEmId)e)
					ecartRetour++;
		}
		Put("{0:<34} aretes={1} ecart compte={2} ecart retour={3} (0 attendu)", "acces/aller-retour-demi-arete",
			aretes, ecartCompte, ecartRetour);
	}

	// -- 3. JUMELLE RADIALE : elle doit REFUSER sur un bord ------------------
	// Le contrat le dit : sur une arete non manifold ou de bord, il n'y a pas
	// d'oppose unique, et en designer un serait un mensonge. Le cas mesure donc
	// les DEUX reponses -- involution sur le cube ferme, refus sur les bords de
	// la grille. Sans le second, une fonction qui rend toujours `twin`
	// passerait.
	{
		MakeCube(v, idx);
		NkEditMesh mc;
		mc.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		mc.RebuildEdges();
		uint32 hCube = 0, involution = 0, memeArete = 0;
		for (uint32 h = 0; h < (uint32)mc.hedges.Size(); ++h) {
			const NkEmId e = mc.EdgeOfHedge((NkEmId)h);
			if (e == NK_EM_INVALID)
				continue;
			hCube++;
			const NkEmId t = mc.RadialTwin((NkEmId)h);
			if (t == NK_EM_INVALID)
				continue;
			if (mc.EdgeOfHedge(t) == e)
				memeArete++;
			if (mc.RadialTwin(t) == (NkEmId)h)
				involution++;
		}
		MakeGrid(3, v, idx);
		NkEditMesh mg;
		mg.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		mg.RebuildEdges();
		uint32 hBord = 0, refus = 0;
		NkVector<NkEmId> hs;
		for (uint32 e = 0; e < (uint32)mg.edges.Size(); ++e) {
			if (!mg.edges[e].alive || !mg.EdgeIsBoundary((NkEmId)e))
				continue;
			hs.Clear();
			mg.EdgeHedges((NkEmId)e, hs);
			for (uint32 k = 0; k < (uint32)hs.Size(); ++k) {
				hBord++;
				if (mg.RadialTwin(hs[k]) == NK_EM_INVALID)
					refus++;
			}
		}
		Put("{0:<34} cube h={1} involution={2} meme arete={3} | grille bord h={4} refus={5}",
			"acces/jumelle-radiale", hCube, involution, memeArete, hBord, refus);
	}

	// -- 4. SELECTION : les deux etats, et la face qui suit ses sommets ------
	{
		MakeCube(v, idx);
		NkEditMesh m;
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		m.RebuildEdges();
		uint32 facesVivantes = 0;
		for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f)
			if (m.faces[f].alive)
				facesVivantes++;
		m.SelectAll();
		const int32 anyTout = m.AnyVertSelected() ? 1 : 0;
		uint32 selTout = 0;
		for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f)
			if (m.faces[f].alive && m.FaceIsSelected((NkEmId)f))
				selTout++;
		m.SelectNone();
		const int32 anyRien = m.AnyVertSelected() ? 1 : 0;
		uint32 selRien = 0;
		for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f)
			if (m.faces[f].alive && m.FaceIsSelected((NkEmId)f))
				selRien++;
		Put("{0:<34} faces={1} | tout: any={2} faces sel={3} | rien: any={4} faces sel={5}",
			"acces/selection-tout-rien", facesVivantes, anyTout, selTout, anyRien, selRien);
	}

	// -- 5. TAMPON DE SELECTION : un RANG qui avance, pas un nombre fige -----
	// Ce qui est verifie est une RELATION -- le compteur ne recule jamais et
	// avance quand la selection change reellement. Le cas resterait juste si le
	// rang de depart changeait.
	{
		MakeCube(v, idx);
		NkEditMesh m;
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		const uint32 n = (uint32)m.verts.Size();
		NkVector<uint8> flags;
		flags.Resize(n);
		for (uint32 i = 0; i < n; ++i)
			flags[i] = 0u;
		const uint32 s0 = m.SelectionStamp();
		flags[0] = 1u;
		m.SetVertSelection(flags.Data(), n);
		const uint32 s1 = m.SelectionStamp();
		flags[1] = 1u;
		m.SetVertSelection(flags.Data(), n);
		const uint32 s2 = m.SelectionStamp();
		// Repousser la MEME selection ne doit rien consommer : c'est ce qui
		// permet a l'editeur de pousser son tableau entier a chaque frame.
		m.SetVertSelection(flags.Data(), n);
		const uint32 s3 = m.SelectionStamp();
		Put("{0:<34} rangs {1}->{2}->{3}->{4} | croit={5} stable sur rappel identique={6}",
			"acces/tampon-de-selection", s0, s1, s2, s3, (s2 > s1 && s1 > s0) ? 1 : 0, (s3 == s2) ? 1 : 0);
	}

	// -- 6. PROPAGATION AUX COINCIDENTS --------------------------------------
	// Le cube de test a ses coins DEDOUBLES (24 sommets pour 8 positions) :
	// selectionner un coin n'en marque qu'une copie sur trois. C'est le defaut
	// que cette methode existe pour reparer, et il n'est mesurable que sur une
	// forme aux coins dedoubles -- d'ou le choix du cube plutot que de la
	// grille.
	{
		MakeCube(v, idx);
		NkEditMesh m;
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		const uint32 n = (uint32)m.verts.Size();
		NkVector<uint8> flags;
		flags.Resize(n);
		for (uint32 i = 0; i < n; ++i)
			flags[i] = 0u;
		flags[0] = 1u;
		m.SetVertSelection(flags.Data(), n);
		uint32 avant = 0;
		for (uint32 i = 0; i < n; ++i)
			if (m.verts[i].sel)
				avant++;
		m.PropagateSelectionToCoincident();
		uint32 apres = 0;
		for (uint32 i = 0; i < n; ++i)
			if (m.verts[i].sel)
				apres++;
		// Toutes les copies retenues doivent occuper la MEME position que la
		// graine : sinon la propagation aurait deborde.
		const NkVec3f graine = m.verts[0].pos;
		uint32 horsPosition = 0;
		for (uint32 i = 0; i < n; ++i) {
			if (!m.verts[i].sel)
				continue;
			const NkVec3f d = m.verts[i].pos - graine;
			if (d.Dot(d) > 1e-8f)
				horsPosition++;
		}
		Put("{0:<34} sommets={1} selection {2}->{3} croit={4} | hors position={5} (0 attendu)",
			"acces/propagation-coincidente", n, avant, apres, (apres > avant) ? 1 : 0, horsPosition);
	}

	// -- 7. OMBRAGE : les TROIS etats, parce que deux ne suffisent pas -------
	// `AnyFaceSmooth` et `AllFacesSmooth` ne se distinguent que sur un maillage
	// MIXTE. Teste seulement en tout-lisse et tout-plat, une implantation qui
	// confondrait les deux passerait.
	{
		MakeCube(v, idx);
		NkEditMesh m;
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		m.RebuildEdges();
		m.SelectAll();
		m.SetShadeSmooth(false);
		const int32 aPlat = m.AnyFaceSmooth() ? 1 : 0, tPlat = m.AllFacesSmooth() ? 1 : 0;
		m.SetShadeSmooth(true);
		const int32 aLisse = m.AnyFaceSmooth() ? 1 : 0, tLisse = m.AllFacesSmooth() ? 1 : 0;
		// Etat MIXTE : on ne remet a plat qu'UNE face, via la selection de ses
		// seuls sommets (une face est selectionnee si tous ses sommets le sont).
		NkVector<NkEmId> boucle;
		NkEmId cible = NK_EM_INVALID;
		for (uint32 f = 0; f < (uint32)m.faces.Size() && cible == NK_EM_INVALID; ++f)
			if (m.faces[f].alive)
				cible = (NkEmId)f;
		m.GetFaceVerts(cible, boucle);
		const uint32 n = (uint32)m.verts.Size();
		NkVector<uint8> flags;
		flags.Resize(n);
		for (uint32 i = 0; i < n; ++i)
			flags[i] = 0u;
		for (uint32 k = 0; k < (uint32)boucle.Size(); ++k)
			flags[boucle[k]] = 1u;
		m.SetVertSelection(flags.Data(), n);
		const int32 change = m.SetShadeSmooth(false, true) ? 1 : 0;
		const int32 aMixte = m.AnyFaceSmooth() ? 1 : 0, tMixte = m.AllFacesSmooth() ? 1 : 0;
		Put("{0:<34} plat any={1} all={2} | lisse any={3} all={4} | mixte change={5} any={6} all={7}",
			"acces/ombrage-trois-etats", aPlat, tPlat, aLisse, tLisse, change, aMixte, tMixte);
	}

	// -- 8. TRIANGULATION OMBREE : identique quand personne ne se dispute ----
	// Le contrat annonce deux regimes. Cube (coins dedoubles) : aucun sommet
	// dispute, sortie STRICTEMENT identique a `Triangulate`. Grille (sommets
	// partages, faces plates) : les coins doivent etre dedoubles, donc la
	// sortie GRANDIT. Le controle porte sur la RELATION entre les deux tailles,
	// pas sur un compte en dur.
	for (int32 forme = 0; forme < 2; ++forme) {
		if (forme == 0)
			MakeCube(v, idx);
		else
			MakeGrid(3, v, idx);
		NkEditMesh m;
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		m.RebuildEdges();
		m.SelectAll();
		m.SetShadeSmooth(false); // tout plat : c'est le regime qui dedouble
		NkVector<NkVertex3D> pv, sv;
		NkVector<uint32> pi, si;
		NkVector<NkEmId> pf, sf;
		m.Triangulate(pv, pi, pf);
		m.TriangulateShaded(sv, si, sf);
		const int32 memeIdx = (pi.Size() == si.Size()) ? 1 : 0;
		const int32 identique = (pv.Size() == sv.Size() && memeIdx) ? 1 : 0;
		Put("{0:<34} verts={1} plate V={2} ombree V={3} identique={4} | tris idem={5}",
			forme == 0 ? "acces/triangule-ombree-cube" : "acces/triangule-ombree-grille", (uint32)m.verts.Size(),
			(uint32)pv.Size(), (uint32)sv.Size(), identique, memeIdx);
	}

	// -- 9. POIDS PROPORTIONNEL : la courbe, pas un point --------------------
	// Cette fonction est exposee pour que l'editeur DESSINE le cercle avec la
	// meme courbe que celle appliquee : si elle diverge, l'utilisateur voit un
	// rayon et en obtient un autre. On mesure donc ce qui doit valoir pour
	// TOUTES les courbes -- plein au centre, nul hors du rayon, jamais
	// croissante -- plutot que six valeurs en dur.
	{
		const float32 r = 2.f;
		uint32 modes = 0, centrePlein = 0, horsNul = 0, decroissantes = 0;
		char detail[96];
		detail[0] = 0;
		for (int32 fo = 0; fo <= 5; ++fo) {
			modes++;
			const float32 w0 = NkEditMesh::ProportionalWeight(0.f, r, fo);
			const float32 wHors = NkEditMesh::ProportionalWeight(r * 1.5f, r, fo);
			if (w0 > 0.999f && w0 < 1.001f)
				centrePlein++;
			if (wHors > -0.001f && wHors < 0.001f)
				horsNul++;
			bool decroit = true;
			float32 prec = 2.f;
			for (int32 k = 0; k <= 16; ++k) {
				const float32 w = NkEditMesh::ProportionalWeight(r * (float32)k / 16.f, r, fo);
				if (w > prec + 1e-4f)
					decroit = false;
				prec = w;
			}
			if (decroit)
				decroissantes++;
			const float32 mi = NkEditMesh::ProportionalWeight(r * 0.5f, r, fo);
			char un[16];
			snprintf(un, sizeof(un), "%s%.2f", fo ? "/" : "", (double)mi);
			strncat(detail, un, sizeof(detail) - strlen(detail) - 1);
		}
		Put("{0:<34} modes={1} centre plein={2} hors nul={3} decroissantes={4} | w(r/2)={5}",
			"acces/poids-proportionnel", modes, centrePlein, horsNul, decroissantes, detail);
	}

	// -- 10. BOUCLE DE FACES -------------------------------------------------
	// L'anneau des faces TRAVERSEES par une arete. Ce qui est verifiable sans
	// figer un compte : toutes vivantes, toutes distinctes, et sur une grille
	// n x n l'anneau traverse une RANGEE entiere -- donc autant de faces que la
	// grille a de colonnes. Le cas reste juste si quelqu'un change `n`.
	{
		const uint32 n = 4;
		MakeGrid(n, v, idx);
		NkEditMesh m;
		m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		m.RebuildEdges();
		// Deux depart : une arete de BORD (0,1) sur la rangee du bas, et une
		// arete INTERIEURE (6,7). L'anneau de faces doit traverser la grille
		// dans les deux cas -- c'est la difference avec la boucle d'ARETES, qui
		// ne progresse qu'a travers des quads et s'arrete donc sur un bord.
		const uint32 dep[2][2] = {{0u, 1u}, {6u, 7u}};
		for (int32 d = 0; d < 2; ++d) {
			NkVector<NkEmId> faces;
			m.GetFaceLoop(dep[d][0], dep[d][1], faces);
			uint32 vivantes = 0, doublons = 0;
			for (uint32 i = 0; i < (uint32)faces.Size(); ++i) {
				if (faces[i] < (NkEmId)m.faces.Size() && m.faces[faces[i]].alive)
					vivantes++;
				for (uint32 j = i + 1; j < (uint32)faces.Size(); ++j)
					if (faces[i] == faces[j])
						doublons++;
			}
			NkVector<uint32> paires;
			m.GetEdgeLoop(dep[d][0], dep[d][1], paires);
			Put("{0:<34} grille={1} faces={2} vivantes={3} doublons={4} rangee entiere={5} | boucle aretes={6}",
				d == 0 ? "acces/boucle-de-faces-bord" : "acces/boucle-de-faces-interieur", n, (uint32)faces.Size(),
				vivantes, doublons, ((uint32)faces.Size() == n) ? 1 : 0, (uint32)(paires.Size() / 2u));
		}
	}

	// -- 11. PILE DE MODIFICATEURS : trouver par identifiant, et DESCENDRE ---
	// `MoveDown` est juge sur son booleen ET sur l'ordre obtenu. Une
	// implantation qui rendrait `true` sans rien deplacer passerait le premier
	// controle seul -- et l'ordre de la pile est un PARAMETRE DE RESULTAT
	// (miroir puis tableau ne donne pas tableau puis miroir).
	{
		NkMeshModifier mir;
		mir.type = NkModifierType::Mirror;
		NkMeshModifier arr;
		arr.type = NkModifierType::Array;
		arr.arrayCount = 3;
		NkModifierStack st;
		const uint32 idMir = st.Add(mir);
		const uint32 idArr = st.Add(arr);
		// Trouver par identifiant : present, absent, et coherent avec l'indice.
		const NkMeshModifier *tMir = st.FindById(idMir);
		const NkMeshModifier *tArr = st.FindById(idArr);
		const NkMeshModifier *tRien = st.FindById(0xFFFFFFFFu);
		const int32 trouve = (tMir && tMir->id == idMir && tArr && tArr->id == idArr) ? 1 : 0;
		const int32 refuseInconnu = (tRien == nullptr) ? 1 : 0;
		// DESCENDRE le premier : booleen, puis ordre reellement echange.
		const int32 okBas = st.MoveDown(0) ? 1 : 0;
		const int32 echange = (st.Count() == 2u && st.modifiers[0].id == idArr && st.modifiers[1].id == idMir) ? 1 : 0;
		// Descendre le DERNIER doit refuser, et ne rien bouger.
		const int32 okFin = st.MoveDown(st.Count() - 1u) ? 1 : 0;
		const int32 intact = (st.Count() == 2u && st.modifiers[0].id == idArr && st.modifiers[1].id == idMir) ? 1 : 0;
		// L'identifiant survit au deplacement, l'indice non.
		const NkMeshModifier *apres = st.FindById(idMir);
		const int32 idStable = (apres && apres->id == idMir && st.IndexOfId(idMir) == 1) ? 1 : 0;
		Put("{0:<34} trouve={1} refus inconnu={2} | bas ok={3} echange={4} | fin ok={5} intact={6} | id stable={7}",
			"acces/pile-trouver-et-descendre", trouve, refuseInconnu, okBas, echange, okFin, intact, idStable);
	}

	// -- 12. ENREGISTREUR DE COMMANDES : empiler, puis REJOUER ---------------
	// C'est la couche de commandes rendue DONNEE -- le socle des modificateurs
	// non destructifs et des donnees d'imitation NKAI. Un rejeu qui rend un
	// compte sans rien appliquer serait le pire des faux verts : la session
	// serait reputee rejouee alors que le maillage n'aurait pas bouge. Le cas
	// exige donc le compte rendu ET le maillage change.
	{
		MakeCube(v, idx);
		NkEditMesh base;
		base.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
		base.RebuildEdges();
		uint32 facesAvant = 0;
		for (uint32 f = 0; f < (uint32)base.faces.Size(); ++f)
			if (base.faces[f].alive)
				facesAvant++;

		NkMeshEditCommand c;
		c.op = NkMeshEditOp::Subdivide;
		c.subdiv.cuts = 1;
		for (uint32 i = 0; i < (uint32)base.verts.Size(); ++i)
			c.selection.PushBack(i);

		NkMeshEditRecorder rec;
		const uint32 n0 = rec.Count();
		rec.Push(c);
		const uint32 n1 = rec.Count();
		rec.Push(c);
		const uint32 n2 = rec.Count();
		// `At` doit rendre ce qui a ete empile, sinon le rejeu porterait sur
		// autre chose que la session enregistree.
		const int32 relu = (rec.Count() == 2u && rec.At(0).op == NkMeshEditOp::Subdivide) ? 1 : 0;

		NkEditMesh cible = base;
		const uint32 applique = rec.ReplayOnto(cible);
		uint32 facesApres = 0;
		for (uint32 f = 0; f < (uint32)cible.faces.Size(); ++f)
			if (cible.faces[f].alive)
				facesApres++;
		Put("{0:<34} count {1}->{2}->{3} relu={4} | applique={5} faces {6}->{7} change={8}",
			"acces/enregistreur-rejeu", n0, n1, n2, relu, applique, facesAvant, facesApres,
			(facesApres != facesAvant) ? 1 : 0);
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
	GraphDocBattery();
	GraphIfaceBattery();
	DecimateBattery();
	RetopoBattery();
	ThemeBattery();
	// AJOUTEE EN FIN, pour la meme raison que SelOrderBattery : les lignes
	// precedentes gardent leur numero, donc la reference reste comparable.
	LinkedBattery();
	// AJOUTEE EN FIN, meme raison : les 169 lignes precedentes gardent leur
	// numero, donc `--check` compare toujours l'ancien perimetre a l'identique.
	MaterialBattery();
	// AJOUTEE EN FIN, meme raison : les 181 lignes precedentes gardent leur numero.
	HistoryBattery();
	// AJOUTEE EN FIN, meme raison : les 188 lignes precedentes gardent leur numero.
	SixOpsBattery();
	// AJOUTEE EN FIN, meme raison : les 195 lignes precedentes gardent leur numero.
	WindingBattery();
	// AJOUTEE EN FIN, meme raison : les 196 lignes precedentes gardent leur numero.
	MatSurvieBattery();
	// AJOUTEE EN FIN, meme raison : les 208 lignes precedentes gardent leur numero.
	LoadersBattery();
	// AJOUTEE EN FIN, meme raison : les 217 lignes precedentes gardent leur numero.
	AccessBattery();

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
