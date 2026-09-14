// =============================================================================
// NKGeniaTemoin -- LE TEMOIN DE LA PORTE DOUBLE (chantier GENIA, lot 3).
//
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
//
// CE QU'IL MESURE, ET POURQUOI CA.
//   Le pari de GENIA : un glTF genere par un modele (TripoSR) atterrit DANS la
//   representation editable du modeleur (NkEditMesh), donc les outils
//   existants le saisissent -- extruder, deplacer -- comme une primitive. Si
//   l'objet importe est une soupe que les outils ne savent pas saisir, le MVP
//   n'existe pas, et c'est ce qu'il faut savoir tot.
//
// LE CRITERE EST ECRIT AVANT LA MESURE, il vient du code d'ExtrudeSelectedFaces
// (variante Region) :
//   - chaque sommet BRUT distinct des faces selectionnees recoit UN duplicat
//     -> V1 = V0 + D, D = nb de sommets bruts distincts de la selection ;
//   - chaque face selectionnee est remplacee par sa coiffe (meme nombre) et
//     chaque arete orientee (a,b) de la selection dont l'inverse (b,a) n'est
//     PAS dans la selection engendre UN quad -> F1 = F0 + B.
//   - l'identite spatiale (BuildVertexMerge, eps 1e-4) : le nombre de groupes
//     de sommets coincidents G est INVARIANT quand on deplace TOUTES les copies
//     d'un groupe ensemble, et il AUGMENTE de 1 quand on n'en deplace qu'une.
//
// CHAQUE VOLET A SON NEGATIF :
//   [1] LoadGLTF sur un chemin inexistant doit rendre FAUX ;
//   [3] ExtrudeSelectedFaces sur une selection VIDE doit rendre FAUX et ne
//       rien changer ;
//   [4] deplacer UNE copie d'un groupe multiple doit CASSER l'identite (G+1).
//       Un glTF genere partage ses sommets (marching cubes) : ses groupes sont
//       des singletons, le negatif se joue alors sur le CUBE (24 sommets, 8
//       groupes) -- et le temoin DIT sur quoi il l'a joue.
//
// PAS DE SEUIL EN MS : rien ici n'est chronometre. Un banc vert se LIT : chaque
// ligne porte le chiffre attendu ET le chiffre mesure.
//
// USAGE : NKGeniaTemoin <fichier.glb|.gltf>     code 0 = VERT, 1 = ROUGE
// =============================================================================
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKRenderer/Mesh/NkEditMesh.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKLogger/NkLog.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

using namespace nkentseu;
using namespace nkentseu::renderer;

static int g_rouge = 0;

static void Attendu(bool ok, const char *quoi, long long attendu, long long mesure) {
	printf("  [%s] %-58s attendu=%lld mesure=%lld\n", ok ? "VERT " : "ROUGE", quoi, attendu, mesure);
	if (!ok)
		++g_rouge;
}

// ── Le cube du moteur (24 sommets, 8 positions) : DONNEE du volet negatif [4] ─
static void MakeCube(NkVector<NkVertex3D> &v, NkVector<uint32> &idx) {
	v.Clear();
	idx.Clear();
	const NkVec3f n[6] = {{0, 0, 1}, {0, 0, -1}, {-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
	const NkVec3f p[8] = {{-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f},	{0.5f, 0.5f, 0.5f},	 {-0.5f, 0.5f, 0.5f},
						  {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}};
	const int32 fi[6][4] = {{1, 0, 3, 2}, {4, 5, 6, 7}, {0, 4, 7, 3}, {5, 1, 2, 6}, {3, 7, 6, 2}, {0, 1, 5, 4}};
	for (int32 f = 0; f < 6; f++) {
		for (int32 k = 0; k < 4; k++) {
			NkVertex3D vt{};
			vt.pos = p[fi[f][k]];
			vt.normal = n[f];
			vt.color = 0xFFFFFFFFu;
			v.PushBack(vt);
		}
		const uint32 b = (uint32)(f * 4);
		const uint32 q[6] = {b, b + 1, b + 2, b, b + 2, b + 3};
		for (int32 k = 0; k < 6; k++)
			idx.PushBack(q[k]);
	}
}

// Les indices GLOBAUX d'un NkGLTFMeshData : glTF ecrit des indices LOCAUX a la
// primitive et un baseVertex par sous-mesh. La lecture juste, la MEME que
// l'import du modeleur (NkModelerImport.h, NkImportCreate) :
//     global = indices[firstIndex + i] + baseVertex.
// Sans ce rebasage, la face 0 de CHAQUE sous-mesh porte les sommets 0,1,2 :
// vu sur BrainStem.glb (59 sous-mesh) -- selectionner trois sommets y
// selectionnait 59 faces. L'arithmetique d'extrusion tenait quand meme, et
// c'est precisement pourquoi le temoin imprime S : un chiffre qu'on lit.
static void IndicesGlobaux(const NkGLTFMeshData &data, NkVector<uint32> &out) {
	out.Clear();
	const uint32 iTotal = (uint32)data.indices.Size();
	for (uint32 s = 0; s < (uint32)data.subMeshes.Size(); s++) {
		const NkSubMesh &sm = data.subMeshes[s];
		for (uint32 i = 0; i < sm.indexCount && sm.firstIndex + i < iTotal; ++i)
			out.PushBack(data.indices[sm.firstIndex + i] + sm.baseVertex);
	}
}

static uint32 Groupes(const NkEditMesh &m) {
	NkVector<uint32> canon;
	m.BuildVertexMerge(canon);
	uint32 g = 0;
	for (uint32 i = 0; i < (uint32)canon.Size(); i++)
		if (canon[i] == i)
			++g;
	return g;
}

static float Diagonale(const NkEditMesh &m) {
	NkVec3f mn{1e30f, 1e30f, 1e30f}, mx{-1e30f, -1e30f, -1e30f};
	for (uint32 i = 0; i < m.VertCount(); i++) {
		const NkVec3f q = m.verts[i].pos;
		mn.x = q.x < mn.x ? q.x : mn.x;
		mn.y = q.y < mn.y ? q.y : mn.y;
		mn.z = q.z < mn.z ? q.z : mn.z;
		mx.x = q.x > mx.x ? q.x : mx.x;
		mx.y = q.y > mx.y ? q.y : mx.y;
		mx.z = q.z > mx.z ? q.z : mx.z;
	}
	return (mx - mn).Len();
}

// [4] identite spatiale sur un maillage : rend vrai si le volet a pu se jouer
// sur CE maillage (il faut un groupe d'au moins deux copies pour le negatif).
static bool VoletIdentite(NkEditMesh &m, const char *nom) {
	NkVector<uint32> canon;
	m.BuildVertexMerge(canon);
	const uint32 G0 = Groupes(m);
	// Le premier groupe qui a AU MOINS DEUX copies.
	uint32 rep = 0xFFFFFFFFu, copies = 0;
	for (uint32 r = 0; r < (uint32)canon.Size() && rep == 0xFFFFFFFFu; r++) {
		if (canon[r] != r)
			continue;
		uint32 c = 0;
		for (uint32 i = 0; i < (uint32)canon.Size(); i++)
			if (canon[i] == r)
				++c;
		if (c >= 2) {
			rep = r;
			copies = c;
		}
	}
	printf("  %s : %u sommets, %u groupes coincidents (eps 1e-4)\n", nom, m.VertCount(), G0);
	if (rep == 0xFFFFFFFFu) {
		printf("  %s : aucun groupe a deux copies -> ce volet ne peut pas se jouer ici (dit, pas cache)\n", nom);
		return false;
	}
	const NkVec3f d{Diagonale(m) * 0.1f, 0.f, 0.f};
	// Positif : TOUTES les copies du groupe bougent ensemble -> G invariant.
	for (uint32 i = 0; i < (uint32)canon.Size(); i++)
		if (canon[i] == rep)
			m.verts[i].pos = m.verts[i].pos + d;
	Attendu(Groupes(m) == G0, "deplacer TOUTES les copies d'un coin : identite tenue (G)", G0, Groupes(m));
	// Negatif : UNE seule copie bouge -> le groupe se scinde, G + 1.
	m.verts[rep].pos = m.verts[rep].pos + d;
	Attendu(Groupes(m) == G0 + 1, "deplacer UNE copie : identite cassee, comme attendu (G+1)", G0 + 1, Groupes(m));
	(void)copies;
	return true;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("usage : NKGeniaTemoin <fichier.glb|.gltf>\n");
		return 1;
	}
	const char *path = argv[1];
	printf("== NKGeniaTemoin : %s ==\n", path);

	// ── [1] LE CHARGEUR EXISTANT LIT-IL LE GLTF GENERE ? ────────────────────
	printf("[1] chargement par NkGLTFLoader\n");
	NkGLTFMeshData data;
	const bool ok = LoadGLTF(NkString(path), data);
	Attendu(ok && data.IsValid(), "LoadGLTF rend vrai et des sommets", 1, ok && data.IsValid() ? 1 : 0);
	if (!ok || !data.IsValid()) {
		printf("VERDICT : ROUGE -- le chargeur ne lit pas ce fichier, rien d'autre ne peut se mesurer\n");
		return 1;
	}
	uint32 images = 0;
	for (uint32 i = 0; i < (uint32)data.images.Size(); i++)
		if (data.images[i].valid)
			++images;
	printf("  MESURE glTF : sommets=%u indices=%u triangles=%u sous-mesh=%u materiaux=%u images_decodees=%u/%u\n",
		   (uint32)data.vertices.Size(), (uint32)data.indices.Size(), (uint32)data.indices.Size() / 3,
		   (uint32)data.subMeshes.Size(), (uint32)data.materials.Size(), images, (uint32)data.images.Size());
	Attendu((uint32)data.indices.Size() % 3 == 0, "indices multiples de 3 (triangles)", 0, data.indices.Size() % 3);
	{
		// Volet negatif : un chemin inexistant doit etre REFUSE.
		NkString faux(path);
		faux.Append(".inexistant.glb");
		NkGLTFMeshData rien;
		const bool okFaux = LoadGLTF(faux, rien);
		Attendu(!okFaux && !rien.IsValid(), "negatif : chemin inexistant refuse", 0, okFaux ? 1 : 0);
	}

	// ── [2] LA REPRESENTATION EDITABLE ──────────────────────────────────────
	printf("[2] entree dans NkEditMesh (demi-aretes, n-gons)\n");
	NkVector<uint32> gi;
	IndicesGlobaux(data, gi);
	Attendu(gi.Size() == data.indices.Size(), "indices rebases par sous-mesh, meme nombre", (long long)data.indices.Size(),
			(long long)gi.Size());
	NkEditMesh m;
	m.BuildFromIndexed(data.vertices.Data(), (uint32)data.vertices.Size(), gi.Data(), (uint32)gi.Size(), false);
	const uint32 V0 = m.VertCount(), F0 = m.FaceCount();
	Attendu(V0 == (uint32)data.vertices.Size(), "sommets conserves 1:1", (long long)data.vertices.Size(), V0);
	Attendu(F0 == (uint32)gi.Size() / 3, "une face par triangle", (long long)gi.Size() / 3, F0);
	printf("  MESURE editable : V0=%u F0=%u groupes=%u\n", V0, F0, Groupes(m));

	// ── [3] EXTRUDER UNE FACE ───────────────────────────────────────────────
	printf("[3] extrusion de la face 0 (Region, offset 0 = contrat Blender)\n");
	{
		// Volet negatif D'ABORD : selection vide -> refus, rien ne change.
		NkVector<uint8> vide;
		vide.Resize(V0);
		for (uint32 i = 0; i < V0; i++)
			vide[i] = 0;
		m.SetVertSelection(vide.Data(), V0);
		const bool r = m.ExtrudeSelectedFaces();
		Attendu(!r, "negatif : extruder sans selection est refuse", 0, r ? 1 : 0);
		Attendu(m.VertCount() == V0, "negatif : V inchange", V0, m.VertCount());
	}
	{
		// Selection = les sommets de la face 0 (une face est selectionnee si
		// TOUS ses sommets le sont : d'autres faces peuvent l'etre aussi, on
		// COMPTE ce qui l'est reellement, on ne suppose pas « une »).
		NkVector<NkEmId> fv;
		m.GetFaceVerts(0, fv);
		NkVector<uint8> sel;
		sel.Resize(V0);
		for (uint32 i = 0; i < V0; i++)
			sel[i] = 0;
		for (uint32 k = 0; k < (uint32)fv.Size(); k++)
			sel[fv[k]] = 1;
		m.SetVertSelection(sel.Data(), V0);
		// Le CRITERE, calcule AVANT : D (sommets bruts distincts) et B (aretes de bord).
		NkHashMap<uint64, uint8> dir;
		NkVector<uint8> vu;
		vu.Resize(V0);
		for (uint32 i = 0; i < V0; i++)
			vu[i] = 0;
		uint32 S = 0, D = 0;
		NkVector<NkEmId> tmp;
		for (uint32 f = 0; f < F0; f++) {
			if (!m.FaceIsSelected(f) || m.FaceSize(f) < 3)
				continue;
			++S;
			m.GetFaceVerts(f, tmp);
			const uint32 n = (uint32)tmp.Size();
			for (uint32 k = 0; k < n; k++) {
				if (!vu[tmp[k]]) {
					vu[tmp[k]] = 1;
					++D;
				}
				dir.InsertOrAssign(((uint64)tmp[k] << 32) | (uint64)tmp[(k + 1) % n], (uint8)1);
			}
		}
		uint32 B = 0;
		for (uint32 f = 0; f < F0; f++) {
			if (!m.FaceIsSelected(f) || m.FaceSize(f) < 3)
				continue;
			m.GetFaceVerts(f, tmp);
			const uint32 n = (uint32)tmp.Size();
			for (uint32 k = 0; k < n; k++) {
				const uint32 a = tmp[k], b = tmp[(k + 1) % n];
				if (!dir.Find(((uint64)b << 32) | (uint64)a))
					++B;
			}
		}
		printf("  critere : S=%u face(s) selectionnee(s), D=%u sommets distincts, B=%u aretes de bord\n", S, D, B);
		// Une face de 3 sommets partages ne selectionne qu'ELLE, sauf doublon
		// exact : S > 1 sur un maillage sain trahit un assemblage faux (indices
		// non rebases), pas une propriete de l'objet. On l'exige.
		Attendu(S == 1, "la selection de la face 0 ne prend qu'une face (assemblage juste)", 1, S);
		const bool r = m.ExtrudeSelectedFaces();
		Attendu(r, "ExtrudeSelectedFaces rend vrai", 1, r ? 1 : 0);
		Attendu(m.VertCount() == V0 + D, "V1 = V0 + D", V0 + D, m.VertCount());
		Attendu(m.FaceCount() == F0 + B, "F1 = F0 + B", F0 + B, m.FaceCount());
		uint32 selApres = 0;
		for (uint32 i = 0; i < m.VertCount(); i++)
			if (m.verts[i].sel)
				++selApres;
		Attendu(selApres == D, "la selection passe sur la geometrie neuve (D sommets)", D, selApres);
	}

	// ── [4] DEPLACER UN SOMMET : L'IDENTITE SPATIALE A EPSILON ──────────────
	printf("[4] identite spatiale (BuildVertexMerge)\n");
	{
		NkEditMesh mg;
		mg.BuildFromIndexed(data.vertices.Data(), (uint32)data.vertices.Size(), gi.Data(), (uint32)gi.Size(), false);
		if (!VoletIdentite(mg, "objet genere")) {
			NkVector<NkVertex3D> cv;
			NkVector<uint32> ci;
			MakeCube(cv, ci);
			NkEditMesh mc;
			mc.BuildFromIndexed(cv.Data(), (uint32)cv.Size(), ci.Data(), (uint32)ci.Size(), true);
			Attendu(mc.VertCount() == 24 && Groupes(mc) == 8, "cube : 24 sommets, 8 groupes (le temoin se controle)", 8,
					Groupes(mc));
			VoletIdentite(mc, "cube (controle)");
		}
	}

	printf("VERDICT : %s (%d ligne(s) rouge(s))\n", g_rouge == 0 ? "VERT -- l'objet genere repond aux outils d'edition" : "ROUGE",
		   g_rouge);
	return g_rouge == 0 ? 0 : 1;
}
