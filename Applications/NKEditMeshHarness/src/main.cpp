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
