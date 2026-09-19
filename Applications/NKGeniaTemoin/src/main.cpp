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
#include "NKRenderer/Mesh/NkOBJLoader.h"
#include "NKRenderer/Mesh/NkEditMesh.h"
#include "NKRenderer/Mesh/NkMeshDecimate.h"
#include "NKRenderer/Mesh/NkMeshRetopo.h"
#include "NKRenderer/Mesh/NkUVUnwrap.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKLogger/NkLog.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace nkentseu;
using namespace nkentseu::renderer;

static int g_rouge = 0;
static double g_diagonale = 0.0; // renseignee par NkRedMode, lue par NkRedUnTaux

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
// L'AIGUILLAGE PAR EXTENSION, et il vit ICI, a un seul endroit.
// POURQUOI : depuis le 2026-09-17 ce temoin juge DEUX producteurs -- le
// generateur d'IMAGE (TripoSR, qui ecrit du glTF) et le generateur de TEXTE
// (NKTexte3D, qui ecrit du .obj parce qu'aucun ecrivain glTF n'existe dans le
// depot). La question mesuree est la MEME : « l'objet produit repond-il aux
// outils d'edition ». Deux temoins auraient pu deriver l'un de l'autre ; un
// seul ne le peut pas. LoadOBJ et LoadGLTF remplissent la meme structure.
static bool ChargerMaillage(const char *path, NkGLTFMeshData &out) {
	const size_t n = path ? strlen(path) : 0;
	const bool obj = n > 4 && (strcmp(path + n - 4, ".obj") == 0 || strcmp(path + n - 4, ".OBJ") == 0);
	return obj ? LoadOBJ(NkString(path), out) : LoadGLTF(NkString(path), out);
}

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

// =============================================================================
// MODE « --reduire » — LE MAILLAGE GENERE EST INEXPLOITABLE, ON L'ALLEGE.
//
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
//
// LE DEFAUT MESURE : TripoSR rend 41 864 sommets et 83 732 triangles pour une
// chaise. Aucun humain ne travaille la-dessus, et rien dans la chaine image ->
// 3D ne l'allege. Le reducteur, lui, EXISTE DEJA et vit dans le MOTEUR :
// `renderer::NkMeshDecimate::DecimateQEM` (quadriques d'erreur, contraction
// d'aretes, trois refus : condition de lien, retournement de face, bord). Ce
// mode ne le reecrit pas : il le BRANCHE sur le producteur et le MESURE sur un
// cas reel -- ses 17 appelants connus sont tous des primitives du harnais, et
// « declarer n'est pas livrer ».
//
// LES CRITERES SONT DERIVES, AUCUN N'EST UN SEUIL DE CONFORT :
//
//   chi = V - E + F EST INVARIANT. Une contraction d'arete sur une surface
//   fermee manifold retire 1 sommet, 3 aretes, 2 faces : -1 + 3 - 2 = 0. Si chi
//   bouge, une contraction a perce un trou ou soude une anse -- ce qu'aucun
//   compte de triangles ne verrait. V est le compte SOUDE (VertOwner) : des
//   copies coincidentes gonflent V sans changer la surface, et les aretes, elles,
//   sont deja construites sur l'identite soudee.
//
//   0 ARETE DE BORD. L'entree est etanche (mesure : volume signe 0,06445, R2.4).
//   Une contraction ne peut pas ouvrir un bord si la condition de lien tient.
//
//   0 ARETE NON MANIFOLD, par le garde-fou qui existe deja :
//   NkEditMesh::NonManifoldEdgeCount().
//
//   LA BORNE DE VOLUME EST UNE INEGALITE GEOMETRIQUE, PAS UN CHOIX. Si toute la
//   surface se deplace d'au plus delta, le volume balaye est majore par
//   Aire x delta (au premier ordre). Donc
//        |Vol_apres - Vol_avant| / Vol_avant  <=  Aire_avant x delta_max / Vol_avant
//   ou delta_max est MESURE par ShapeError, jamais pose. On imprime le rapport
//   mesure/borne : un ecart qu'on sait expliquer vaut mieux qu'un ecart tolere.
//
// LE ZERO EST PROUVE EN PREMIER, ET IL EST DOUBLE :
//   - ShapeError(m, m) sur le maillage avec LUI-MEME doit rendre 0. Un
//     comparateur qui ne rend pas zero sur l'identite ne mesure rien ;
//   - taux = 1,00 ne doit RIEN deplacer : memes comptes, meme volume, delta = 0.
//
// LES MUTATIONS VIVENT DANS LE MEME BINAIRE (NK_REDUC_MUTE), jamais dans une
// seconde construction -- deux constructions peuvent differer par autre chose :
//   =1  preserveTopology = false   -> les refus par lien tombent a 0
//   =2  maxNormalFlipDeg = 180     -> des faces se retournent
//   =3  preserveBoundary = false   -> AUCUN effet attendu (l'objet est ferme).
//       Celle-la doit RESTER VERTE : elle mesure que mes criteres ne rougissent
//       pas au hasard. Une mutation qui fait tout rougir ne prouve pas sa cible.
//
// USAGE : NKGeniaTemoin --reduire <in.glb|.obj> [--taux R] [--faces N]
//                       [--out sortie.obj] [--quads]
//         NKGeniaTemoin --banc-reduction <in.glb|.obj> [--dossier D]
// =============================================================================

struct NkRedMesure {
		uint32 vBrut = 0;	  ///< sommets tels que stockes (copies comprises)
		uint32 vSoude = 0;	  ///< representants distincts : le V d'Euler
		uint32 e = 0;		  ///< aretes vivantes
		uint32 f = 0;		  ///< faces
		uint32 tri = 0;		  ///< triangles apres triangulation
		uint32 bord = 0;	  ///< aretes a une seule face
		uint32 nonMan = 0;	  ///< aretes a trois faces ou plus
		int64 chi = 0;		  ///< V - E + F, sur le compte SOUDE
		double volume = 0.0;  ///< volume signe (divergence)
		double aire = 0.0;	  ///< aire totale
};

static double NkRedAire3(const NkVec3f &a, const NkVec3f &b, const NkVec3f &c) {
	const double ux = (double)b.x - a.x, uy = (double)b.y - a.y, uz = (double)b.z - a.z;
	const double vx = (double)c.x - a.x, vy = (double)c.y - a.y, vz = (double)c.z - a.z;
	const double cx = uy * vz - uz * vy, cy = uz * vx - ux * vz, cz = ux * vy - uy * vx;
	return 0.5 * sqrt(cx * cx + cy * cy + cz * cz);
}

// Volume signe par le theoreme de la divergence : somme de det(a,b,c)/6. Sur une
// surface fermee correctement orientee c'est le volume ; sur une surface ouverte
// ou retournee, il derive -- c'est donc AUSSI un detecteur de retournement.
static double NkRedVol3(const NkVec3f &a, const NkVec3f &b, const NkVec3f &c) {
	const double d = (double)a.x * ((double)b.y * c.z - (double)b.z * c.y) -
					 (double)a.y * ((double)b.x * c.z - (double)b.z * c.x) +
					 (double)a.z * ((double)b.x * c.y - (double)b.y * c.x);
	return d / 6.0;
}

static NkRedMesure NkRedMesurer(NkEditMesh &m) {
	NkRedMesure r;
	m.RebuildEdges();
	r.vBrut = m.VertCount();
	r.f = m.FaceCount();
	r.e = m.EdgeCount();
	// V d'Euler = representants distincts de l'identite soudee.
	{
		NkVector<uint8> vu;
		vu.Resize(r.vBrut);
		for (uint32 i = 0; i < r.vBrut; i++)
			vu[i] = 0;
		for (uint32 i = 0; i < r.vBrut; i++) {
			const uint32 o = m.VertOwner(i);
			if (o < r.vBrut && !vu[o]) {
				vu[o] = 1;
				++r.vSoude;
			}
		}
	}
	// ⚠ On parcourt le TABLEAU, pas le COMPTE : `EdgeCount()` ne rend que les
	// aretes VIVANTES, et s'en servir comme borne d'indice sauterait les
	// dernieres des qu'une seule arete morte traine. C'est la meme faute que
	// « compter au lieu d'ouvrir ».
	for (uint32 e = 0; e < (uint32)m.edges.Size(); e++) {
		if (m.EdgeIsBoundary(e))
			++r.bord;
	}
	r.nonMan = m.NonManifoldEdgeCount();
	r.chi = (int64)r.vSoude - (int64)r.e + (int64)r.f;
	// Volume et aire par triangulation en eventail, sur les positions reelles.
	NkVector<NkEmId> fv;
	for (uint32 f = 0; f < r.f; f++) {
		const uint32 n = m.FaceSize(f);
		if (n < 3)
			continue;
		m.GetFaceVerts(f, fv);
		for (uint32 k = 1; k + 1 < (uint32)fv.Size(); k++) {
			const NkVec3f &a = m.verts[fv[0]].pos, &b = m.verts[fv[k]].pos, &c = m.verts[fv[k + 1]].pos;
			r.volume += NkRedVol3(a, b, c);
			r.aire += NkRedAire3(a, b, c);
			++r.tri;
		}
	}
	return r;
}

static void NkRedImprimer(const char *quand, const NkRedMesure &r) {
	printf("  %-8s V=%u (soude %u) E=%u F=%u tri=%u | bord=%u nonManifold=%u | chi=%lld | vol=%.6f aire=%.6f\n", quand,
		   r.vBrut, r.vSoude, r.e, r.f, r.tri, r.bord, r.nonMan, (long long)r.chi, r.volume, r.aire);
}

// Ecriture OBJ depuis NkEditMesh. Il n'existe AUCUN ecrivain de maillage dans le
// depot (NkGLTFIO.h:50, NkOBJIO.cpp:82 « NON IMPLEMENTE ») ; celui-ci est local
// au temoin et n'ecrit que ce que LoadOBJ sait relire -- v / vn / f a//a.
// C'est la MEME porte que celle par laquelle le texte -> 3D entre deja.
static bool NkRedEcrireObj(const char *path, NkEditMesh &m) {
	NkVector<NkVertex3D> ov;
	NkVector<uint32> oi;
	NkVector<NkEmId> of;
	m.Triangulate(ov, oi, of);
	FILE *fp = fopen(path, "wb");
	if (!fp)
		return false;
	fprintf(fp, "# NKGeniaTemoin --reduire : maillage allege par NkMeshDecimate (QEM)\n");
	fprintf(fp, "# AUTEUR : TEUGUIA TADJUIDJE Rodolf Sederis - Rihen\n");
	for (uint32 i = 0; i < (uint32)ov.Size(); i++)
		fprintf(fp, "v %.6f %.6f %.6f\n", ov[i].pos.x, ov[i].pos.y, ov[i].pos.z);
	for (uint32 i = 0; i < (uint32)ov.Size(); i++)
		fprintf(fp, "vn %.6f %.6f %.6f\n", ov[i].normal.x, ov[i].normal.y, ov[i].normal.z);
	for (uint32 i = 0; i + 2 < (uint32)oi.Size(); i += 3)
		fprintf(fp, "f %u//%u %u//%u %u//%u\n", oi[i] + 1, oi[i] + 1, oi[i + 1] + 1, oi[i + 1] + 1, oi[i + 2] + 1,
				oi[i + 2] + 1);
	fclose(fp);
	return true;
}

// La mutation est LUE UNE FOIS et IMPRIMEE : un banc qui ne dit pas sous quelle
// mutation il tourne rend un vert qu'on ne peut pas situer.
static int NkRedMutation() {
	const char *v = getenv("NK_REDUC_MUTE");
	return v ? atoi(v) : 0;
}

static void NkRedAppliquerMutation(NkDecimateParams &p, int mute) {
	if (mute == 1)
		p.preserveTopology = false;
	else if (mute == 2)
		p.maxNormalFlipDeg = 180.f;
	else if (mute == 3)
		p.preserveBoundary = false;
}

// Une reduction, mesuree de bout en bout. Rend le nombre de lignes rouges.
// ⚠ `avant` est la reference NETTOYEE (taux 1,00), pas l'entree brute, et cette
// distinction est tout le rapport R14 : DecimateQEM TRIANGULE et RECONSTRUIT
// toujours, meme sans contracter. Comparer a l'entree brute melange deux
// causes -- le nettoyage et les contractions -- et fait rougir un moteur
// correct. `brut` reste imprime a cote : c'est le defaut que Rodolf decrit.
static void NkRedUnTaux(const NkVector<NkVertex3D> &verts, const NkVector<uint32> &idx, float taux, uint32 cibleFaces,
						const NkRedMesure &avant, const char *sortie, bool quads) {
	NkEditMesh m;
	m.BuildFromIndexed(verts.Data(), (uint32)verts.Size(), idx.Data(), (uint32)idx.Size(), false);
	NkEditMesh ref;
	ref.BuildFromIndexed(verts.Data(), (uint32)verts.Size(), idx.Data(), (uint32)idx.Size(), false);

	NkDecimateParams p;
	p.targetRatio = taux;
	p.targetFaces = cibleFaces;
	const int mute = NkRedMutation();
	NkRedAppliquerMutation(p, mute);

	NkDecimateStats st;
	const bool ok = NkMeshDecimate::DecimateQEM(m, p, &st);
	if (!ok) {
		printf("  [ROUGE] DecimateQEM rend faux (taux=%.3f)\n", taux);
		++g_rouge;
		return;
	}

	NkRetopoStats rs;
	if (quads) {
		NkRetopoParams rp;
		rp.targetRatio = 1.f; // la decimation a deja eu lieu : on ne fait que fusionner
		NkMeshRetopo::QuadDominant(m, rp, &rs);
	}

	const NkRedMesure ap = NkRedMesurer(m);
	float32 moy = 0.f, max = 0.f;
	NkMeshDecimate::ShapeError(m, ref, moy, max);

	if (cibleFaces)
		printf("\n-- CIBLE %u faces%s --\n", cibleFaces, quads ? " + retopo quads" : "");
	else
		printf("\n-- taux demande %.3f%s --\n", taux, quads ? " + retopo quads" : "");
	NkRedImprimer("apres", ap);
	printf("  QEM      contractions=%u refus(lien=%u flip=%u cout=%u) coutMax=%.6g cible=%s\n", st.collapses,
		   st.rejectedLink, st.rejectedFlip, st.rejectedCost, (double)st.maxCost, st.reachedTarget ? "atteinte" : "NON");
	if (quads)
		printf("  QUADS    quads=%u triangles=%u ratio=%.3f alignement=%.3f faces_epinglees=%u\n", rs.quadsOut,
			   rs.trisOut, (double)rs.quadRatio, (double)rs.alignMean, rs.pinnedFaces);
	printf("  FORME    ecart moyen=%.6f max=%.6f (unites monde)\n", (double)moy, (double)max);
	printf("  >> RODOLF : %u sommets, %u triangles | ecart max = %.3f %% de la diagonale | volume a %.3f %%\n",
		   ap.vSoude, ap.tri, 100.0 * (double)max / (g_diagonale > 0 ? g_diagonale : 1.0),
		   avant.volume != 0.0 ? 100.0 * ((ap.volume - avant.volume) < 0 ? -(ap.volume - avant.volume)
																		: (ap.volume - avant.volume)) / avant.volume
							   : 0.0);

	// ── LES CRITERES, ET LEURS ATTENDUS DERIVES ─────────────────────────────
	// chi = V - E + F : une contraction retire 1 sommet, 3 aretes, 2 faces, donc
	// -1 + 3 - 2 = 0. ⚠ CETTE DERIVATION SUPPOSE UNE SURFACE MANIFOLD, et le
	// maillage de TripoSR n'en est pas une (voir la ligne « brut » : chi IMPAIR).
	// Le critere se prononce donc contre la reference NETTOYEE, la seule contre
	// laquelle il a un sens. Un critere qui se prononce hors de sa condition ne
	// mesure plus rien -- il fabrique du bruit qu'on apprend a ignorer.
	Attendu(ap.chi == avant.chi, "chi invariant / reference nettoyee (-1+3-2 = 0)", (long long)avant.chi,
			(long long)ap.chi);
	Attendu(ap.bord == 0, "0 arete de bord (l'objet reste ferme)", 0, ap.bord);
	// L'entree en porte 279 : exiger 0 serait exiger du reducteur qu'il REPARE ce
	// que le generateur a casse. Ce qu'on exige, et qui est le contrat de
	// `preserveTopology`, c'est qu'il n'en AJOUTE aucune.
	Attendu(ap.nonMan <= avant.nonMan, "aretes non manifold : aucune AJOUTEE (<= reference)", avant.nonMan,
			ap.nonMan);

	// Borne de volume DERIVEE : |dVol| <= Aire x delta_max.
	const double borne = avant.aire * (double)max;
	const double ecart = ap.volume - avant.volume;
	const double dv = ecart < 0 ? -ecart : ecart;
	printf("  VOLUME   avant=%.6f apres=%.6f ecart=%.6f (%.3f %% du volume)\n", avant.volume, ap.volume, ecart,
		   avant.volume != 0.0 ? 100.0 * dv / (avant.volume < 0 ? -avant.volume : avant.volume) : 0.0);
	printf("  BORNE    Aire x delta_max = %.6f x %.6f = %.6f -> mesure/borne = %.4f\n", avant.aire, (double)max, borne,
		   borne > 0 ? dv / borne : 0.0);
	Attendu(dv <= borne, "|dVol| <= Aire x delta_max (inegalite geometrique)", (long long)(borne * 1e6),
			(long long)(dv * 1e6));

	if (sortie && sortie[0]) {
		const bool w = NkRedEcrireObj(sortie, m);
		Attendu(w, "le maillage allege est ecrit en .obj (la porte d'edition)", 1, w ? 1 : 0);
		printf("  ECRIT    %s\n", sortie);
	}
}


// ── LE COMPTEUR DE FACES RETOURNEES, ET POURQUOI IL A FALLU L'ECRIRE ────────
// La garde `maxNormalFlipDeg` refuse les contractions qui feraient tourner une
// face au-dela d'un angle. La lever (NK_REDUC_MUTE=2) n'a fait rougir AUCUN de
// mes criteres : ni chi, ni le bord, ni le volume, ni la borne de forme. Or une
// mutation qui survit dit UNE de deux choses, et il faut savoir laquelle :
//   - le critere ne teste rien ;
//   - ou la garde ne sert a rien SUR CE MAILLAGE.
// Aucun de mes criteres ne pouvait les departager, parce qu'aucun n'est sensible
// a l'ORIENTATION locale : chi est topologique, le volume est global, la borne de
// forme est une distance. Un retournement franc deplace la normale sans deplacer
// ni la topologie ni la position. C'est la famille « un temoin invariant par le
// defaut qu'on cherche » -- sa verdeur ne dit rien.
//
// L'instrument : pour chaque face du maillage allege, on cherche le sommet de la
// REFERENCE le plus proche de son centre (grille de hachage), et on compare la
// normale de la face a la normale de ce sommet. `dot < 0` = la face regarde a
// l'oppose de la surface d'origine. C'est une grandeur que ni chi ni le volume
// ne portent.
static uint32 NkRedFacesRetournees(NkEditMesh &dec, const NkEditMesh &ref) {
	if (ref.VertCount() == 0)
		return 0;
	// Grille de hachage sur la boite de la reference.
	NkVec3f mn{1e30f, 1e30f, 1e30f}, mx{-1e30f, -1e30f, -1e30f};
	for (uint32 i = 0; i < ref.VertCount(); i++) {
		const NkVec3f q = ref.verts[i].pos;
		mn.x = q.x < mn.x ? q.x : mn.x; mn.y = q.y < mn.y ? q.y : mn.y; mn.z = q.z < mn.z ? q.z : mn.z;
		mx.x = q.x > mx.x ? q.x : mx.x; mx.y = q.y > mx.y ? q.y : mx.y; mx.z = q.z > mx.z ? q.z : mx.z;
	}
	const NkVec3f ext = mx - mn;
	const float diag = ext.Len() > 0 ? ext.Len() : 1.f;
	const uint32 R = 48;
	const float pas = diag / (float)R;
	NkVector<NkVector<uint32>> cases;
	cases.Resize(R * R * R);
	auto cellOf = [&](const NkVec3f &q) -> uint32 {
		int32 ix = (int32)((q.x - mn.x) / (pas > 0 ? pas : 1.f));
		int32 iy = (int32)((q.y - mn.y) / (pas > 0 ? pas : 1.f));
		int32 iz = (int32)((q.z - mn.z) / (pas > 0 ? pas : 1.f));
		ix = ix < 0 ? 0 : (ix >= (int32)R ? (int32)R - 1 : ix);
		iy = iy < 0 ? 0 : (iy >= (int32)R ? (int32)R - 1 : iy);
		iz = iz < 0 ? 0 : (iz >= (int32)R ? (int32)R - 1 : iz);
		return (uint32)((iz * R + iy) * R + ix);
	};
	for (uint32 i = 0; i < ref.VertCount(); i++)
		cases[cellOf(ref.verts[i].pos)].PushBack(i);

	uint32 retournees = 0, jugees = 0;
	NkVector<NkEmId> fv;
	for (uint32 f = 0; f < dec.FaceCount(); f++) {
		if (dec.FaceSize(f) < 3)
			continue;
		dec.GetFaceVerts(f, fv);
		const NkVec3f a = dec.verts[fv[0]].pos, b = dec.verts[fv[1]].pos, c = dec.verts[fv[2]].pos;
		const NkVec3f u = b - a, v = c - a;
		NkVec3f n{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
		const float ln = n.Len();
		if (ln <= 0.f)
			continue;
		n = n * (1.f / ln);
		const NkVec3f ctr{(a.x + b.x + c.x) / 3.f, (a.y + b.y + c.y) / 3.f, (a.z + b.z + c.z) / 3.f};
		// Voisinage 3x3x3 de cellules autour du centre.
		int32 cx = (int32)((ctr.x - mn.x) / (pas > 0 ? pas : 1.f));
		int32 cy = (int32)((ctr.y - mn.y) / (pas > 0 ? pas : 1.f));
		int32 cz = (int32)((ctr.z - mn.z) / (pas > 0 ? pas : 1.f));
		float best = 1e30f;
		uint32 bi = 0xFFFFFFFFu;
		for (int32 dz = -1; dz <= 1; dz++)
			for (int32 dy = -1; dy <= 1; dy++)
				for (int32 dx = -1; dx <= 1; dx++) {
					const int32 ix = cx + dx, iy = cy + dy, iz = cz + dz;
					if (ix < 0 || iy < 0 || iz < 0 || ix >= (int32)R || iy >= (int32)R || iz >= (int32)R)
						continue;
					const NkVector<uint32> &lst = cases[(uint32)((iz * R + iy) * R + ix)];
					for (uint32 k = 0; k < (uint32)lst.Size(); k++) {
						const NkVec3f d = ref.verts[lst[k]].pos - ctr;
						const float dd = d.x * d.x + d.y * d.y + d.z * d.z;
						if (dd < best) { best = dd; bi = lst[k]; }
					}
				}
		if (bi == 0xFFFFFFFFu)
			continue; // la reference n'a aucun sommet dans le voisinage : on se TAIT
		const NkVec3f nr = ref.verts[bi].normal;
		if (nr.Len() <= 0.f)
			continue;
		++jugees;
		if (n.x * nr.x + n.y * nr.y + n.z * nr.z < 0.f)
			++retournees;
	}
	printf("  ORIENT   faces jugees=%u (celles qui ont un sommet de reference dans leur voisinage)\n", jugees);
	return retournees;
}

// L'A/B DES GARDES, DANS LE MEME BINAIRE, DANS LA MEME COURSE. Deux
// constructions peuvent differer par autre chose ; deux variables d'un meme
// processus ne different que par ce qu'on a change.
static int NkRedBancGardes(const NkVector<NkVertex3D> &verts, const NkVector<uint32> &idx, uint32 cibleFaces) {
	NkEditMesh ref;
	ref.BuildFromIndexed(verts.Data(), (uint32)verts.Size(), idx.Data(), (uint32)idx.Size(), false);
	printf("\n[G] LES GARDES SERVENT-ELLES ? L'A/B DANS LE MEME BINAIRE (cible %u faces)\n", cibleFaces);
	uint32 res[2] = {0, 0};
	for (int32 k = 0; k < 2; k++) {
		NkEditMesh m;
		m.BuildFromIndexed(verts.Data(), (uint32)verts.Size(), idx.Data(), (uint32)idx.Size(), false);
		NkDecimateParams p;
		p.targetFaces = cibleFaces;
		if (k == 1)
			p.maxNormalFlipDeg = 180.f; // la garde LEVEE
		NkDecimateStats st;
		NkMeshDecimate::DecimateQEM(m, p, &st);
		m.RebuildEdges();
		printf("  %s : refus flip=%u\n", k == 0 ? "garde ACTIVE (90 deg)" : "garde LEVEE  (180 deg)", st.rejectedFlip);
		res[k] = NkRedFacesRetournees(m, ref);
		printf("  %s : faces RETOURNEES contre la reference = %u\n",
			   k == 0 ? "garde ACTIVE (90 deg)" : "garde LEVEE  (180 deg)", res[k]);
	}
	// L'ATTENDU, ET IL EST ECRIT AVANT : si la garde sert, la course LEVEE en
	// porte STRICTEMENT PLUS. Si les deux comptes sont egaux, la garde ne change
	// rien sur ce maillage -- et c'est un resultat, pas un echec.
	Attendu(res[1] > res[0], "la garde anti-retournement SERT (levee => plus de faces retournees)", (long long)res[0] + 1,
			(long long)res[1]);
	return res[1] > res[0] ? 0 : 1;
}

// =============================================================================
// LE BOUCHAGE : la brique que j'avais classee « la plus incertaine des quatre »,
// et qui devient la plus CRITIQUE.
//
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
//
// POURQUOI MAINTENANT. Rodolf veut « decolle les bras » comme une OPERATION du
// modeleur, pas comme une chaine complete. Une operation locale suffit : couper
// le bras, reboucher les deux ouvertures. Pas besoin de rigger tout le
// personnage. Le point incertain passe donc de la peripherie AU CENTRE : si le
// bouchage ne marche pas, l'operation laisse DEUX TROUS et le maillage n'est
// plus etanche.
//
// CE QUE JE MESURE, ET JE NE L'AVAIS PAS FAIT : `NkEditMesh::MakeFaceFromSelected`
// ferme-t-il une GRANDE boucle NON PLANE ? Je l'avais annonce « existant mais non
// mesure » -- c'est exactement le genre d'affirmation que ce chantier interdit.
//
// ⚠️ LE CAS DEFAVORABLE EST IMPOSE D'EMBLEE, et c'est ma lecon de la veille : un
// zero trop favorable masque le defaut. On coupe donc par un plan INCLINE, ce
// qui donne une boucle NON PLANE -- le cas reel d'une section de bras oblique.
// Une coupe horizontale aurait donne un cercle plat, cas le plus facile, et
// aurait valide un outil incapable du cas qui compte.
//
// LES CRITERES, DERIVES :
//   - avant bouchage : exactement N aretes de bord, une seule boucle ;
//   - apres : ZERO arete de bord ;
//   - chi RETROUVE sa valeur d'avant la coupe (une sphere coupee puis bouchee
//     redevient une sphere : chi = 2). C'est le critere qui distingue « bouche »
//     de « bouche n'importe comment » ;
//   - et le volume doit redevenir POSITIF et proche de l'original.
// =============================================================================

static uint32 NkBoucheCompterBords(NkEditMesh &m) {
	m.RebuildEdges();
	uint32 n = 0;
	for (uint32 e = 0; e < (uint32)m.edges.Size(); ++e)
		if (m.EdgeIsBoundary(e))
			++n;
	return n;
}

static int64 NkBoucheChi(NkEditMesh &m) {
	m.RebuildEdges();
	const uint32 nv = m.VertCount();
	NkVector<uint8> vu;
	vu.Resize(nv);
	for (uint32 i = 0; i < nv; ++i)
		vu[i] = 0;
	uint32 soude = 0;
	for (uint32 i = 0; i < nv; ++i) {
		const uint32 o = m.VertOwner(i);
		if (o < nv && !vu[o]) {
			vu[o] = 1;
			++soude;
		}
	}
	return (int64)soude - (int64)m.EdgeCount() + (int64)m.FaceCount();
}

static int NkBoucherMode(const char *chemin) {
	printf("== NKGeniaTemoin --boucher : %s ==\n", chemin);
	printf("   On coupe par un plan INCLINE (boucle NON PLANE), puis on rebouche.\n");
	printf("   ⚠ Une coupe horizontale aurait donne un cercle plat -- le cas le plus\n");
	printf("     facile -- et aurait valide un outil incapable du cas qui compte.\n\n");

	NkGLTFMeshData data;
	if (!ChargerMaillage(chemin, data) || !data.IsValid()) {
		printf("REFUS : le chargeur ne lit pas %s\n", chemin);
		return 2;
	}
	NkVector<uint32> gi;
	IndicesGlobaux(data, gi);
	NkEditMesh m;
	m.BuildFromIndexed(data.vertices.Data(), (uint32)data.vertices.Size(), gi.Data(), (uint32)gi.Size(), false);

	const int64 chi0 = NkBoucheChi(m);
	const uint32 bord0 = NkBoucheCompterBords(m);
	printf("[0] AVANT LA COUPE : V=%u F=%u E=%u | bords=%u chi=%lld\n", m.VertCount(), m.FaceCount(), m.EdgeCount(),
		   bord0, (long long)chi0);
	Attendu(bord0 == 0, "le maillage de depart est FERME (sinon rien ne se mesure)", 0, bord0);
	if (bord0 != 0)
		return 1;

	// ── LA COUPE : supprimer les faces au-dessus d'un plan incline ───────────
	// La normale (0,3 ; 1 ; 0,2) donne une section franchement oblique.
	NkVec3f mn{1e30f, 1e30f, 1e30f}, mx{-1e30f, -1e30f, -1e30f};
	for (uint32 i = 0; i < m.VertCount(); ++i) {
		const NkVec3f q = m.verts[i].pos;
		mn.x = q.x < mn.x ? q.x : mn.x; mn.y = q.y < mn.y ? q.y : mn.y; mn.z = q.z < mn.z ? q.z : mn.z;
		mx.x = q.x > mx.x ? q.x : mx.x; mx.y = q.y > mx.y ? q.y : mx.y; mx.z = q.z > mx.z ? q.z : mx.z;
	}
	const NkVec3f ctr{0.5f * (mn.x + mx.x), 0.5f * (mn.y + mx.y), 0.5f * (mn.z + mx.z)};
	const NkVec3f nrm{0.3f, 1.0f, 0.2f};

	NkVector<uint8> selV;
	selV.Resize(m.VertCount());
	for (uint32 i = 0; i < m.VertCount(); ++i)
		selV[i] = 0;
	NkVector<NkEmId> fv;
	uint32 aSupprimer = 0;
	for (uint32 f = 0; f < m.FaceCount(); ++f) {
		if (m.FaceSize(f) < 3)
			continue;
		m.GetFaceVerts(f, fv);
		float d = 0.f;
		for (uint32 k = 0; k < (uint32)fv.Size(); ++k) {
			const NkVec3f &q = m.verts[fv[k]].pos;
			d += (q.x - ctr.x) * nrm.x + (q.y - ctr.y) * nrm.y + (q.z - ctr.z) * nrm.z;
		}
		if (d / (float)fv.Size() > 0.f) {
			for (uint32 k = 0; k < (uint32)fv.Size(); ++k)
				selV[fv[k]] = 1;
			++aSupprimer;
		}
	}
	printf("[1] LA COUPE : %u face(s) au-dessus du plan incline\n", aSupprimer);
	m.SetVertSelection(selV.Data(), (uint32)selV.Size());
	if (!m.DeleteSelectedFaces()) {
		printf("  [ROUGE] DeleteSelectedFaces rend faux\n");
		return 1;
	}
	const uint32 bord1 = NkBoucheCompterBords(m);
	const int64 chi1 = NkBoucheChi(m);
	printf("  apres coupe : V=%u F=%u | bords=%u chi=%lld\n", m.VertCount(), m.FaceCount(), bord1, (long long)chi1);
	Attendu(bord1 > 0, "la coupe a bien OUVERT le maillage (sinon on boucherait du vide)", 1, bord1 > 0 ? 1 : 0);
	if (bord1 == 0)
		return 1;

	// ── LE BOUCHAGE : selectionner la boucle de bord, puis MakeFaceFromSelected
	NkVector<uint8> selB;
	selB.Resize(m.VertCount());
	for (uint32 i = 0; i < m.VertCount(); ++i)
		selB[i] = 0;
	uint32 nBord = 0;
	for (uint32 e = 0; e < (uint32)m.edges.Size(); ++e) {
		if (!m.EdgeIsBoundary(e))
			continue;
		NkVector<NkEmId> hs;
		m.EdgeHedges(e, hs);
		for (uint32 k = 0; k < (uint32)hs.Size(); ++k) {
			const NkEmId h = hs[k];
			if (h < (NkEmId)m.hedges.Size()) {
				const uint32 v = m.hedges[h].origin;
				if (v < (uint32)selB.Size() && !selB[v]) {
					selB[v] = 1;
					++nBord;
				}
			}
		}
	}
	printf("[2] LE BOUCHAGE : %u sommet(s) sur la boucle de bord\n", nBord);
	m.SetVertSelection(selB.Data(), (uint32)selB.Size());
	const bool ok = m.MakeFaceFromSelected();
	printf("  MakeFaceFromSelected rend %s\n", ok ? "VRAI" : "FAUX");

	const uint32 bord2 = NkBoucheCompterBords(m);
	const int64 chi2 = NkBoucheChi(m);
	printf("  apres bouchage : V=%u F=%u | bords=%u chi=%lld\n", m.VertCount(), m.FaceCount(), bord2,
		   (long long)chi2);

	Attendu(ok, "MakeFaceFromSelected rend vrai", 1, ok ? 1 : 0);
	Attendu(bord2 == 0, "ZERO arete de bord apres bouchage (le maillage est refERME)", 0, bord2);
	// chi RETROUVE sa valeur d'origine : c'est ce qui distingue « bouche » de
	// « bouche n'importe comment ». Une face en trop, ou une boucle fermee de
	// travers, deplacerait chi.
	Attendu(chi2 == chi0, "chi retrouve sa valeur d'avant la coupe", (long long)chi0, (long long)chi2);

	printf("\nVERDICT BOUCHAGE : %s (%d ligne(s) rouge(s))\n", g_rouge == 0 ? "VERT" : "ROUGE", g_rouge);
	return g_rouge == 0 ? 0 : 1;
}

// =============================================================================
// LE DEPLIAGE UV SUR UN MAILLAGE REEL.
//
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
//
// ⚠️ J'AVAIS ANNONCE « aucun deplage UV n'existe dans le depot pour un maillage
// quelconque ». C'ETAIT FAUX. `NkUVUnwrap` fait 1 045 lignes, porte des refus
// NOMMES (`NK_UV_ILOT_NON_DISQUE`, `NK_UV_COINS_SOUDES`), mesure sa propre
// distorsion, et son banc dedie rend **19 tests, 0 echec**. Je l'avais dit
// « non mesure » et c'etait la seule part juste de mon affirmation.
//
// CE QUE CE MODE MESURE, ET QUE LE BANC DEDIE NE MESURE PAS : le comportement
// sur un maillage REEL -- celui d'Ilyana-3DG, allege, avec sa topologie
// irreguliere -- et non sur des cas synthetiques choisis. Un module vert sur un
// cube et une croix peut echouer sur 1 006 sommets issus de marching cubes.
//
// LES QUATRE CRITERES, ECRITS AU §3 DE L'ETAT DE REPRISE AVANT TOUT CODE :
//   distorsion d'ANGLE (conformite) · distorsion d'AIRE · nombre de COUTURES ·
//   taux d'OCCUPATION de l'atlas.
// Les deux premiers sont DEJA calcules par `NkUVMeasureDistortion` : je ne les
// reecris pas, je les lis.
// =============================================================================

static int NkUVMode(const char *chemin) {
	printf("== NKGeniaTemoin --deplier : %s ==\n", chemin);
	NkGLTFMeshData data;
	if (!ChargerMaillage(chemin, data) || !data.IsValid()) {
		printf("REFUS : le chargeur ne lit pas %s\n", chemin);
		return 2;
	}
	NkVector<uint32> gi;
	IndicesGlobaux(data, gi);
	NkEditMesh m;
	m.BuildFromIndexed(data.vertices.Data(), (uint32)data.vertices.Size(), gi.Data(), (uint32)gi.Size(), false);
	printf("  entree : V=%u F=%u\n", m.VertCount(), m.FaceCount());

	NkUVUnwrapParams p;
	p.packIslands = true;
	p.packMargin = 0.02f;
	NkUVResult res;
	NkVector<NkUVIslandInfo> ilots;

	const bool ok = NkUVUnwrap(m, p, res, &ilots);
	printf("  NkUVUnwrap rend %s\n", ok ? "VRAI" : "FAUX");
	if (!ok) {
		// ⚠️ UN REFUS EST UN RESULTAT, PAS UNE PANNE. Le module NOMME sa cause et
		// dit quel ilot fautif : c'est exactement ce qu'on demande partout
		// ailleurs, et il faut le rapporter tel quel plutot que de le traduire.
		printf("  REFUS NOMME : code=%d ilot=%u euler=%d\n", (int)res.refus, res.refusIsland, res.refusEuler);
		printf("  -> ce n'est PAS une panne : le module refuse de deplier un ilot qui n'est pas\n");
		printf("     un disque topologique. Le maillage d'entree doit etre prepare (coutures).\n");
		return 1;
	}

	printf("  ilots=%u coins_soudes=%u iterations=%u residu=%.3e\n", res.islandCount, res.weldedCorners,
		   res.cgIterationsUsed, (double)res.cgResidual);

	NkUVDistortion d;
	if (NkUVMeasureDistortion(m, d)) {
		printf("\n  LES QUATRE CRITERES (ecrits avant, etat de reprise §3) :\n");
		// [1] ANGLE : la conformite. Un depliage conforme preserve les angles ;
		// l'ecart se lit en degres et il est BORNE par construction pour un
		// depliage conforme (LSCM).
		const bool okA = d.angleMax <= 15.f;
		printf("  [%s] distorsion d'ANGLE : max %.3f deg (moyenne %.3f) -- seuil 15 deg\n",
			   okA ? "VERT " : "ROUGE", (double)d.angleMax, (double)d.angleMean);
		// [2] AIRE : le rapport aire 3D / aire UV. 1,0 = isometrie parfaite.
		// ⚠️ Un depliage CONFORME ne preserve PAS les aires : un facteur 2 a 4 est
		// normal sur une forme courbe, et l'exiger a 1,0 serait exiger
		// l'impossible. Le seuil juge l'EXPLOITABILITE, pas la perfection.
		const bool okAi = d.areaMax <= 8.f && d.areaMin >= 0.125f;
		printf("  [%s] distorsion d'AIRE : [%.3f .. %.3f] (moyenne %.3f) -- borne [0,125 .. 8]\n",
			   okAi ? "VERT " : "ROUGE", (double)d.areaMin, (double)d.areaMax, (double)d.areaMean);
		printf("           %u triangles juges\n", d.triCount);
		// [3] COUTURES : leur nombre, rapporte aux aretes. Une couture par arete
		// serait un atlas illisible.
		printf("  [INFO ] ilots : %u\n", res.islandCount);
		// [4] OCCUPATION : la somme des aires UV des ilots, l'atlas valant 1.
		// L'aire UV se calcule depuis les faces : l'ilot ne la porte pas.
		float32 aireUV = 0.f;
		NkVector<NkEmId> fvu;
		for (uint32 f = 0; f < m.FaceCount(); ++f) {
			if (m.FaceSize(f) < 3)
				continue;
			m.GetFaceVerts(f, fvu);
			for (uint32 k = 1; k + 1 < (uint32)fvu.Size(); ++k) {
				const NkVec2f &a = m.verts[fvu[0]].uv, &bb = m.verts[fvu[k]].uv, &c = m.verts[fvu[k + 1]].uv;
				const float ar = 0.5f * fabsf((bb.x - a.x) * (c.y - a.y) - (c.x - a.x) * (bb.y - a.y));
				aireUV += ar;
			}
		}
		const bool okO = aireUV >= 0.25f;
		printf("  [%s] OCCUPATION de l'atlas : %.1f %% -- seuil 25 %%\n", okO ? "VERT " : "ROUGE",
			   (double)(100.f * aireUV));
		printf("           (un atlas a moitie vide gache la moitie de la texture)\n");
		return (okA && okAi && okO) ? 0 : 1;
	}
	printf("  [ROUGE] NkUVMeasureDistortion rend faux\n");
	return 1;
}


// ── MESURER LES UV D'UN MAILLAGE QUI EN PORTE DEJA ──────────────────────────
// La projection planaire est calculee ailleurs (un script, car c'est une mesure
// et non un produit). Ici on LIT le resultat et on le juge avec l'outil du
// depot -- `NkUVMeasureDistortion`, 1 045 lignes deja eprouvees, que je n'ai
// pas a reecrire.
static int NkUVMesurerMode(const char *chemin) {
	printf("== NKGeniaTemoin --mesurer-uv : %s ==\n", chemin);
	NkGLTFMeshData data;
	if (!ChargerMaillage(chemin, data) || !data.IsValid()) {
		printf("REFUS : le chargeur ne lit pas %s\n", chemin);
		return 2;
	}
	NkVector<uint32> gi;
	IndicesGlobaux(data, gi);
	NkEditMesh m;
	m.BuildFromIndexed(data.vertices.Data(), (uint32)data.vertices.Size(), gi.Data(), (uint32)gi.Size(), false);
	// Les UV lues par NkOBJLoader vivent dans les sommets : on les transporte.
	for (uint32 i = 0; i < m.VertCount() && i < (uint32)data.vertices.Size(); ++i)
		m.verts[i].uv = data.vertices[i].uv;
	NkUVDistortion d;
	if (!NkUVMeasureDistortion(m, d)) {
		printf("  [ROUGE] NkUVMeasureDistortion rend faux\n");
		return 1;
	}
	printf("  V=%u F=%u | %u triangles juges\n", m.VertCount(), m.FaceCount(), d.triCount);
	printf("  ANGLE : min %.3f  moyenne %.3f  MAX %.3f  (degres)\n", (double)d.angleMin, (double)d.angleMean,
		   (double)d.angleMax);
	printf("  AIRE  : min %.4f  moyenne %.4f  MAX %.4f  (rapport 3D/UV, 1 = isometrie)\n", (double)d.areaMin,
		   (double)d.areaMean, (double)d.areaMax);
	return 0;
}

// Le point d'entree du mode. Rend le code de sortie (0 = VERT).
static int NkRedMode(int argc, char **argv) {
	const char *in = nullptr;
	const char *out = "";
	const char *dossier = ".";
	float taux = 0.f;
	uint32 cibleFaces = 0;
	bool quads = false;
	bool banc = false;
	bool gardes = false;

	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (strcmp(a, "--reduire") == 0 && i + 1 < argc)
			in = argv[++i];
		else if (strcmp(a, "--banc-reduction") == 0 && i + 1 < argc) {
			in = argv[++i];
			banc = true;
		} else if (strcmp(a, "--taux") == 0 && i + 1 < argc)
			taux = (float)atof(argv[++i]);
		else if (strcmp(a, "--faces") == 0 && i + 1 < argc)
			cibleFaces = (uint32)atoi(argv[++i]);
		else if (strcmp(a, "--out") == 0 && i + 1 < argc)
			out = argv[++i];
		else if (strcmp(a, "--dossier") == 0 && i + 1 < argc)
			dossier = argv[++i];
		else if (strcmp(a, "--quads") == 0)
			quads = true;
		else if (strcmp(a, "--gardes") == 0)
			gardes = true;
		else {
			printf("REFUS : argument inconnu : %s\n", a);
			return 2;
		}
	}
	if (!in) {
		printf("REFUS : aucun fichier d'entree\n");
		return 2;
	}

	const int mute = NkRedMutation();
	printf("== NKGeniaTemoin --%s : %s ==\n", banc ? "banc-reduction" : "reduire", in);
	printf("   mutation NK_REDUC_MUTE=%d %s\n", mute,
		   mute == 0	 ? "(aucune : le produit)"
		   : mute == 1	 ? "(preserveTopology=false : le non-manifold doit apparaitre)"
		   : mute == 2	 ? "(maxNormalFlipDeg=180 : des faces doivent se retourner)"
		   : mute == 3	 ? "(preserveBoundary=false : AUCUN effet attendu, objet ferme)"
						 : "(inconnue)");

	NkGLTFMeshData data;
	if (!ChargerMaillage(in, data) || !data.IsValid()) {
		printf("REFUS : le chargeur ne lit pas %s (ni glTF ni OBJ)\n", in);
		return 2;
	}
	NkVector<uint32> gi;
	IndicesGlobaux(data, gi);

	NkEditMesh m0;
	m0.BuildFromIndexed(data.vertices.Data(), (uint32)data.vertices.Size(), gi.Data(), (uint32)gi.Size(), false);
	const NkRedMesure brut = NkRedMesurer(m0);
	printf("\n[0] L'ETAT D'ENTREE, ET C'EST LE DEFAUT QUE RODOLF DECRIT\n");
	printf("    ⚠ chi IMPAIR ou nonManifold > 0 = l'entree n'est PAS une surface fermee manifold.\n");
	NkRedImprimer("brut", brut);
	g_diagonale = (double)Diagonale(m0);
	printf("  diagonale de l'objet = %.6f (les ecarts de forme se lisent avec elle)\n", g_diagonale);

	// ── LE ZERO, ET IL EST DOUBLE. Avant tout chiffre de reduction. ─────────
	printf("\n[Z] LE ZERO SE PROUVE EN PREMIER\n");
	{
		float32 moy = 0.f, max = 0.f;
		NkMeshDecimate::ShapeError(m0, m0, moy, max);
		Attendu(moy == 0.f && max == 0.f, "ShapeError(m, m) = 0 : le comparateur sait dire 'identique'", 0,
				(long long)(max * 1e6f));
	}
	NkRedMesure ref = brut;
	{
		// taux 1,00 : AUCUNE contraction demandee. Ce que cette course mesure n'est
		// donc pas la decimation, c'est ce que `DecimateQEM` fait AVANT de decimer :
		// elle triangule et reconstruit. C'est le seul moyen de separer les deux
		// causes -- pour localiser un changement il faut DEUX points de mesure qui
		// encadrent le suspect, pas un.
		NkEditMesh m1;
		m1.BuildFromIndexed(data.vertices.Data(), (uint32)data.vertices.Size(), gi.Data(), (uint32)gi.Size(), false);
		NkDecimateParams p;
		p.targetRatio = 1.f;
		NkDecimateStats st;
		NkMeshDecimate::DecimateQEM(m1, p, &st);
		ref = NkRedMesurer(m1);
		NkRedImprimer("ref 1,00", ref);
		Attendu(st.collapses == 0, "taux 1,00 : aucune contraction (le zero du reducteur)", 0, st.collapses);
		// Tolerance DERIVEE, pas choisie : le volume est une somme de 83 732 termes
		// en double ; l'erreur d'accumulation est de l'ordre de N x eps x |terme|,
		// soit 83732 x 2,2e-16 x 1e-6 ~ 2e-17. Tout ecart plus grand vient donc des
		// faces reellement supprimees par la reconstruction, et il faut le DIRE.
		const double d = ref.volume - brut.volume;
		const double dv = d < 0 ? -d : d;
		printf("  NETTOYAGE : %d face(s) supprimee(s) par la reconstruction, dVol = %.3e (%.4f %% du volume)\n",
			   (int)((long long)brut.f - (long long)ref.f), d, brut.volume != 0 ? 100.0 * dv / brut.volume : 0.0);
		Attendu(dv < 1e-6, "taux 1,00 : le nettoyage ne change pas le volume au-dela de 1e-6", 1000,
				(long long)(dv * 1e9));
	}

	if (gardes) {
		NkRedBancGardes(data.vertices, gi, cibleFaces ? cibleFaces : 2000);
	} else if (!banc) {
		if (taux <= 0.f && cibleFaces == 0)
			taux = 0.1f;
		NkRedUnTaux(data.vertices, gi, taux, cibleFaces, ref, out, quads);
	} else {
		// LE BALAYAGE. Les cibles sont ABSOLUES (un compte de faces), parce que
		// la question de Rodolf est absolue : « combien de sommets pour un objet
		// utilisable ». Un taux relatif ne repond pas a cette question.
		const uint32 cibles[] = {20000, 10000, 5000, 2000, 1000, 500};
		char chemin[1024];
		for (uint32 i = 0; i < 6; i++) {
			snprintf(chemin, sizeof(chemin), "%s/reduit_%uf.obj", dossier, cibles[i]);
			NkRedUnTaux(data.vertices, gi, 0.f, cibles[i], ref, chemin, false);
		}
		// Et UNE course avec la retopologie quad, sur la cible mediane, pour
		// dire ce qu'elle vaut au lieu de promettre de l'ecrire.
		snprintf(chemin, sizeof(chemin), "%s/reduit_2000f_quads.obj", dossier);
		NkRedUnTaux(data.vertices, gi, 0.f, 2000, ref, chemin, true);
	}

	printf("\nVERDICT : %s (%d ligne(s) rouge(s))\n", g_rouge == 0 ? "VERT" : "ROUGE", g_rouge);
	return g_rouge == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("usage : NKGeniaTemoin <fichier.glb|.gltf|.obj>\n"
			   "        NKGeniaTemoin --reduire <in> [--taux R|--faces N] [--out f.obj] [--quads]\n"
			   "        NKGeniaTemoin --banc-reduction <in> [--dossier D]\n");
		return 1;
	}
	// AIGUILLAGE. Le mode « --reduire » ne remplace pas le temoin nominal : il
	// s'y ajoute, et le maillage qu'il ecrit REPASSE par le temoin nominal --
	// c'est ainsi qu'on prouve que le maillage allege reste editable par la MEME
	// porte que les modeles importes.
	if (strcmp(argv[1], "--reduire") == 0 || strcmp(argv[1], "--banc-reduction") == 0)
		return NkRedMode(argc, argv);

	// LE BOUCHAGE : la brique que « decolle les bras » rend critique.
	if (strcmp(argv[1], "--boucher") == 0 && argc > 2)
		return NkBoucherMode(argv[2]);

	// LE DEPLIAGE UV sur un maillage REEL d'Ilyana-3DG.
	if (strcmp(argv[1], "--deplier") == 0 && argc > 2)
		return NkUVMode(argv[2]);

	if (strcmp(argv[1], "--mesurer-uv") == 0 && argc > 2)
		return NkUVMesurerMode(argv[2]);

	const char *path = argv[1];
	printf("== NKGeniaTemoin : %s ==\n", path);

	// ── [1] LE CHARGEUR EXISTANT LIT-IL LE GLTF GENERE ? ────────────────────
	printf("[1] chargement par NkGLTFLoader (.glb/.gltf) ou NkOBJLoader (.obj)\n");
	NkGLTFMeshData data;
	const bool ok = ChargerMaillage(path, data);
	Attendu(ok && data.IsValid(), "le chargeur rend vrai et des sommets", 1, ok && data.IsValid() ? 1 : 0);
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
		// L'extension est CONSERVEE : ajouter « .glb » ferait tester le
		// chargeur glTF meme quand le cas porte sur un .obj -- un negatif qui
		// change de chemin de code ne refute pas le chemin mesure.
		const size_t nl = strlen(path);
		const bool objCas = nl > 4 && (strcmp(path + nl - 4, ".obj") == 0 || strcmp(path + nl - 4, ".OBJ") == 0);
		faux.Append(objCas ? ".inexistant.obj" : ".inexistant.glb");
		NkGLTFMeshData rien;
		const bool okFaux = ChargerMaillage(faux.CStr(), rien);
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
