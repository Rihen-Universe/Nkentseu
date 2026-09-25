// -----------------------------------------------------------------------------
// @File    main.cpp  (NKUVUnwrapTest)
// @Brief   Banc du DEPLIAGE UV. Chaque critere a un attendu ECRIT AVANT la mesure
//          et un NEGATIF qui doit echouer ; les tests sont nommes et comptes.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI UNE CIBLE A PART, ET PAS UNE BATTERIE DANS NKEditMeshHarness
//   Ce harnais-la compare sa sortie a `editmesh_baseline.txt`, un fichier
//   VERSIONNE ET PARTAGE. Y ajouter des lignes obligerait a le regenerer, donc a
//   entrer en conflit avec tout agent qui touche NkEditMesh en meme temps. La
//   cible separee ne coute qu'un fichier de construction et ne bloque personne.
//
// MODES
//   (aucun)            u1, u2, u3 et leurs negatifs
//   --u4-ecrire <f>    deplie le cube, ECRIT <f>, imprime l'empreinte des UV
//   --u4-relire <f>    PROCESSUS NEUF : maillage reconstruit, charge <f>,
//                      imprime l'empreinte des UV relues + reecrit <f>.rt
//   Les deux invocations sont deux EXECUTIONS DISTINCTES du binaire : c'est ce
//   qui fait le « processus neuf », mieux qu'un fork interne qui partagerait le
//   tas et les pages deja chaudes.
// -----------------------------------------------------------------------------

#include "NKRenderer/Mesh/NkEditMesh.h"
#include "NKRenderer/Mesh/NkUVUnwrap.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

using namespace nkentseu;
using namespace nkentseu::renderer;
using namespace nkentseu::math;

// ── COMPTAGE DES TESTS ──────────────────────────────────────────────────────
// Un binaire qui n'a execute AUCUN test annonce exactement la meme chose qu'un
// binaire dont tout passe. Le nombre execute est donc imprime, toujours.
static uint32 gTests = 0u;
static uint32 gFail = 0u;

static void Check(const char *nom, bool ok, const char *detail) {
	++gTests;
	if (!ok) ++gFail;
	printf("%-28s %s   %s\n", nom, ok ? "[ OK ]" : "[FAIL]", detail ? detail : "");
	fflush(stdout);
}

// ── MAILLAGES D'ESSAI ───────────────────────────────────────────────────────
static void MakeCube(NkVector<NkVertex3D> &v, NkVector<uint32> &idx) {
	v.Clear();
	idx.Clear();
	const NkVec3f n[6] = {{0, 0, 1}, {0, 0, -1}, {-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
	const NkVec3f p[8] = {{-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f},  {0.5f, 0.5f, 0.5f},  {-0.5f, 0.5f, 0.5f},
						  {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}};
	const int32 fi[6][4] = {{1, 0, 3, 2}, {4, 5, 6, 7}, {0, 4, 7, 3}, {5, 1, 2, 6}, {3, 7, 6, 2}, {0, 1, 5, 4}};
	for (int32 f = 0; f < 6; f++) {
		for (int32 k = 0; k < 4; k++) {
			NkVertex3D vt{};
			vt.pos = p[fi[f][k]];
			vt.normal = n[f];
			vt.tangent = NkVec3f{1.f, 0.f, 0.f};
			vt.uv = NkVec2f{0.f, 0.f};
			vt.uv2 = NkVec2f{0.f, 0.f};
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

// UN QUAD PLAN : le cas dont on connait la reponse. Volontairement NON carre
// (3 x 2) : un carre aurait pu masquer une erreur d'echelle anisotrope, qui est
// exactement le genre de defaut qu'un depliage peut introduire.
static void MakeQuad(NkVector<NkVertex3D> &v, NkVector<uint32> &idx, float32 w, float32 h) {
	v.Clear();
	idx.Clear();
	const NkVec3f p[4] = {{0.f, 0.f, 0.f}, {w, 0.f, 0.f}, {w, h, 0.f}, {0.f, h, 0.f}};
	for (int32 k = 0; k < 4; ++k) {
		NkVertex3D vt{};
		vt.pos = p[k];
		vt.normal = NkVec3f{0.f, 0.f, 1.f};
		vt.tangent = NkVec3f{1.f, 0.f, 0.f};
		vt.color = 0xFFFFFFFFu;
		v.PushBack(vt);
	}
	idx.PushBack(0);
	idx.PushBack(1);
	idx.PushBack(2);
	idx.PushBack(0);
	idx.PushBack(2);
	idx.PushBack(3);
}

// DEUX quads DISJOINTS (aucune arete commune) : sert a prouver le compteur de
// recouvrement SUR ZERO, avant de lui demander quoi que ce soit d'autre.
static void MakeTwoQuads(NkVector<NkVertex3D> &v, NkVector<uint32> &idx) {
	v.Clear();
	idx.Clear();
	const float32 ox[2] = {0.f, 10.f};
	for (int32 q = 0; q < 2; ++q) {
		const NkVec3f p[4] = {{ox[q], 0.f, 0.f}, {ox[q] + 1.f, 0.f, 0.f},
							  {ox[q] + 1.f, 1.f, 0.f}, {ox[q], 1.f, 0.f}};
		for (int32 k = 0; k < 4; ++k) {
			NkVertex3D vt{};
			vt.pos = p[k];
			vt.normal = NkVec3f{0.f, 0.f, 1.f};
			vt.tangent = NkVec3f{1.f, 0.f, 0.f};
			vt.color = 0xFFFFFFFFu;
			v.PushBack(vt);
		}
		const uint32 b = (uint32)q * 4u;
		idx.PushBack(b);
		idx.PushBack(b + 1);
		idx.PushBack(b + 2);
		idx.PushBack(b);
		idx.PushBack(b + 2);
		idx.PushBack(b + 3);
	}
}

// Sphere : le maillage FORTEMENT COURBE du negatif de distorsion. Les bandes
// polaires sont omises (les triangles y degenerent) : on veut mesurer une
// courbure, pas des faces nulles.
//
// `soude` decide de la REPRESENTATION, et c'est tout sauf un detail :
//   soude = true  -> un sommet partage par ses 4 faces (l'import classique).
//                    Une couture ne peut alors PAS porter deux UV : un seul champ
//                    `uv` par `Vert`. Le solveur doit le REFUSER en le nommant.
//   soude = false -> un sommet par coin, comme le cube du depot (24, pas 8).
//                    C'est la forme sur laquelle un depliage a un sens.
// La premiere version de ce banc n'offrait que la forme soudee et croyait mesurer
// une distorsion : elle mesurait un refus.
static void MakeSphere(uint32 stacks, uint32 slices, bool soude, NkVector<NkVertex3D> &v,
					   NkVector<uint32> &idx) {
	v.Clear();
	idx.Clear();
	const uint32 rings = stacks - 1u;
	auto PosAt = [&](uint32 i, uint32 j) {
		const float32 phi = 3.14159265358979f * (float32)(i + 1u) / (float32)stacks;
		const float32 th = 6.28318530717959f * (float32)(j % slices) / (float32)slices;
		return NkVec3f{sinf(phi) * cosf(th), cosf(phi), sinf(phi) * sinf(th)};
	};
	auto Push = [&](const NkVec3f &p) {
		NkVertex3D vt{};
		vt.pos = p;
		vt.normal = p;
		vt.tangent = NkVec3f{1.f, 0.f, 0.f};
		vt.color = 0xFFFFFFFFu;
		v.PushBack(vt);
	};

	if (soude) {
		for (uint32 i = 0; i < rings; ++i)
			for (uint32 j = 0; j < slices; ++j) Push(PosAt(i, j));
		for (uint32 i = 0; i + 1u < rings; ++i) {
			for (uint32 j = 0; j < slices; ++j) {
				const uint32 j2 = (j + 1u) % slices;
				const uint32 a = i * slices + j, b = i * slices + j2;
				const uint32 c = (i + 1u) * slices + j2, d = (i + 1u) * slices + j;
				idx.PushBack(a);
				idx.PushBack(b);
				idx.PushBack(c);
				idx.PushBack(a);
				idx.PushBack(c);
				idx.PushBack(d);
			}
		}
		return;
	}
	// ⚠ OUVERT EN THETA (j s'arrete a slices-2), et ce n'est pas un detail.
	//
	// Une GRILLE de quads posee sur la sphere est deja un disque topologique
	// (Euler = 1) : elle n'a besoin d'AUCUNE couture. Or c'est la seule facon
	// d'obtenir une vraie mesure de distorsion, pour une raison que la premiere
	// version de ce banc ignorait :
	//
	//   DECOUPER LE LONG DU COMPLEMENTAIRE D'UN ARBRE COUVRANT DU DUAL ANNULE LA
	//   DISTORSION PAR CONSTRUCTION. Le patron obtenu n'a aucun cycle -- c'est un
	//   ARBRE de triangles --, et un arbre de triangles se deplie TOUJOURS
	//   isometriquement : on rabat chaque triangle sur le precedent, aucune
	//   contrainte de fermeture ne s'y oppose jamais. L'energie conforme y atteint
	//   zero, exactement.
	//
	// Le premier N3 mesurait donc une sphere decoupee en patron et s'etonnait de
	// lire 0,000 : le solveur avait raison, c'est le NEGATIF qui ne pouvait rien
	// refuter. Sur une grille sans couture, les sommets interieurs gardent leur
	// defaut angulaire (2*pi moins la somme des angles incidents) : la courbure
	// de Gauss discrete est la, et aucun depliage plan ne peut l'effacer.
	for (uint32 i = 0; i + 1u < rings; ++i) {
		for (uint32 j = 0; j + 1u < slices; ++j) {
			const uint32 b0 = (uint32)v.Size();
			Push(PosAt(i, j));
			Push(PosAt(i, j + 1u));
			Push(PosAt(i + 1u, j + 1u));
			Push(PosAt(i + 1u, j));
			idx.PushBack(b0);
			idx.PushBack(b0 + 1u);
			idx.PushBack(b0 + 2u);
			idx.PushBack(b0);
			idx.PushBack(b0 + 2u);
			idx.PushBack(b0 + 3u);
		}
	}
}

static void Build(NkEditMesh &m, NkVector<NkVertex3D> &v, NkVector<uint32> &idx, bool quadify) {
	m.Clear();
	m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), quadify);
	m.RebuildEdges();
}

// -- COUTURES PAR ARBRE COUVRANT DU DUAL : DEMENAGEE DANS LA BIBLIOTHEQUE ---
// Elle vivait ICI, `static`, et c'est precisement ce qui rendait `NkUVUnwrap`
// inappelable depuis le produit : son entree obligatoire (les coutures d'une
// surface fermee) n'existait que dans ce fichier de test. Elle est desormais
// dans `NkUVUnwrap.h/.cpp`.
//
// CE RELAIS EST LA PREUVE DU DEMENAGEMENT, pas une commodite : les douze cas de
// ce banc continuent de l'appeler par le meme nom, aux memes endroits, avec les
// memes attendus derives (coutures = E - (F - 1), ilots = 1, Euler = 1). Si le
// transport avait change quoi que ce soit au comportement, ils rougiraient ici.
static uint32 SeamsFromDualSpanningTree(const NkEditMesh &m, NkVector<NkEmId> &outSeams,
										uint32 dropOne) {
	return nkentseu::renderer::NkUVSeamsFromDualSpanningTree(m, outSeams, dropOne);
}

static uint64 UvFingerprint(const NkEditMesh &m) {
	// FNV-1a sur les OCTETS des float32, pas sur leur valeur decimale : c'est ce
	// qui rend l'empreinte equivalente a un memcmp, donc capable de voir un seul
	// bit de mantisse.
	uint64 h = 1469598103934665603ull;
	for (uint32 i = 0; i < (uint32)m.verts.Size(); ++i) {
		uint8 b[8];
		memcpy(b, &m.verts[i].uv.x, 4);
		memcpy(b + 4, &m.verts[i].uv.y, 4);
		for (uint32 k = 0; k < 8u; ++k) {
			h ^= (uint64)b[k];
			h *= 1099511628211ull;
		}
	}
	return h;
}

// =============================================================================
int main(int argc, char **argv) {
	// ── (u4) phases, en processus distincts ─────────────────────────────────
	if (argc >= 3 && strcmp(argv[1], "--u4-ecrire") == 0) {
		NkVector<NkVertex3D> v;
		NkVector<uint32> idx;
		NkEditMesh m;
		MakeCube(v, idx);
		Build(m, v, idx, true);
		NkVector<NkEmId> seams;
		SeamsFromDualSpanningTree(m, seams, 0u);
		NkUVUnwrapParams pr;
		pr.seams = seams.Data();
		pr.seamCount = (uint32)seams.Size();
		NkUVResult res;
		if (!NkUVUnwrap(m, pr, res)) {
			printf("ECRIRE: refus %s\n", NkUVRefusName(res.refus));
			return 2;
		}
		if (!NkUVSaveLayout(m, argv[2])) {
			printf("ECRIRE: enregistrement echoue\n");
			return 2;
		}
		printf("EMPREINTE %llu\n", (unsigned long long)UvFingerprint(m));
		printf("COINS %u\n", (uint32)m.verts.Size());
		return 0;
	}
	if (argc >= 3 && strcmp(argv[1], "--u4-relire") == 0) {
		NkVector<NkVertex3D> v;
		NkVector<uint32> idx;
		NkEditMesh m;
		MakeCube(v, idx);
		Build(m, v, idx, true);
		// Les UV sont a ZERO ici : rien du processus precedent n'a survecu
		// autrement que par le fichier. On le PROUVE en imprimant l'empreinte
		// AVANT le chargement — sans ca, un chargement qui ne ferait rien du
		// tout passerait pour une reussite si les UV se trouvaient deja bonnes.
		printf("AVANT %llu\n", (unsigned long long)UvFingerprint(m));
		if (!NkUVLoadLayout(m, argv[2])) {
			printf("RELIRE: chargement refuse\n");
			return 2;
		}
		printf("EMPREINTE %llu\n", (unsigned long long)UvFingerprint(m));
		return 0;
	}

	printf("=== NKUVUnwrapTest — depliage UV (LSCM) ===\n\n");

	NkVector<NkVertex3D> v;
	NkVector<uint32> idx;
	NkVector<NkEmId> seams;
	char buf[512];

	// =========================================================================
	// CONDITION D'ESSAI — mesuree, pas supposee
	// Un banc a deja mesure une pente de 10 degres sur un sol parfaitement plat,
	// avec des chiffres coherents entre eux. On verifie donc le maillage AVANT
	// de mesurer quoi que ce soit dessus.
	// ATTENDU : le cube a 24 sommets (4 par face, un Vert = un coin), 6 faces,
	// 12 aretes. Si c'etait 8 sommets, aucune couture ne pourrait porter deux UV
	// et tout le reste du banc serait a relire.
	// =========================================================================
	{
		NkEditMesh m;
		MakeCube(v, idx);
		Build(m, v, idx, true);
		uint32 liveF = 0u, liveE = 0u;
		for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f)
			if (m.faces[f].alive) ++liveF;
		for (uint32 e = 0; e < (uint32)m.edges.Size(); ++e)
			if (m.edges[e].alive) ++liveE;
		snprintf(buf, sizeof(buf), "V=%u (attendu 24)  F=%u (attendu 6)  E=%u (attendu 12)",
				 (uint32)m.verts.Size(), liveF, liveE);
		Check("cond/cube-24-coins", (uint32)m.verts.Size() == 24u && liveF == 6u && liveE == 12u, buf);
	}

	// =========================================================================
	// (u1) ILOTS
	// =========================================================================
	// A1 — cube FERME, AUCUNE couture. Euler = 8 - 12 + 6 = 2 : ce n'est pas un
	// disque, c'est une sphere. REFUS attendu, et AUCUNE UV ecrite.
	{
		NkEditMesh m;
		MakeCube(v, idx);
		Build(m, v, idx, true);
		NkUVUnwrapParams pr; // aucune couture
		NkUVResult res;
		const bool ok = NkUVUnwrap(m, pr, res);
		bool uvIntactes = true;
		for (uint32 i = 0; i < (uint32)m.verts.Size(); ++i)
			if (m.verts[i].uv.x != 0.f || m.verts[i].uv.y != 0.f) uvIntactes = false;
		snprintf(buf, sizeof(buf), "refus=%s euler=%d (attendu 2)  UV non touchees=%s",
				 NkUVRefusName(res.refus), res.refusEuler, uvIntactes ? "oui" : "NON");
		Check("u1/A1-ferme-refuse",
			  !ok && res.refus == NkUVRefus::IlotNonDisque && res.refusEuler == 2 && uvIntactes, buf);
	}

	// A2 — coutures = complementaire d'un arbre couvrant du dual.
	// ATTENDU DERIVE (pas recopie) : coutures = E - (F - 1) = 12 - 5 = 7,
	// 1 seul ilot, Euler = 1, et V = 24 - 2*5 = 14, E = 24 - 5 = 19.
	{
		NkEditMesh m;
		MakeCube(v, idx);
		Build(m, v, idx, true);
		uint32 liveF = 0u, liveE = 0u;
		for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f)
			if (m.faces[f].alive) ++liveF;
		for (uint32 e = 0; e < (uint32)m.edges.Size(); ++e)
			if (m.edges[e].alive) ++liveE;
		const uint32 nSeamsAttendu = liveE - (liveF - 1u);
		const uint32 n = SeamsFromDualSpanningTree(m, seams, 0u);

		NkUVUnwrapParams pr;
		pr.seams = seams.Data();
		pr.seamCount = n;
		NkUVResult res;
		NkVector<NkUVIslandInfo> isl;
		const bool ok = NkUVUnwrap(m, pr, res, &isl);
		const int32 euler = (isl.Size() > 0u) ? isl[0].euler : -999;
		const uint32 vC = (isl.Size() > 0u) ? isl[0].vertCount : 0u;
		const uint32 eC = (isl.Size() > 0u) ? isl[0].edgeCount : 0u;
		snprintf(buf, sizeof(buf), "coutures=%u (derive %u)  ilots=%u (attendu 1)  V=%u/14 E=%u/19 euler=%d/1",
				 n, nSeamsAttendu, res.islandCount, vC, eC, euler);
		Check("u1/A2-croix-1-ilot",
			  ok && n == nSeamsAttendu && res.islandCount == 1u && euler == 1 && vC == 14u && eC == 19u, buf);
	}

	// A3 — TOUTES les aretes coupees : 6 ilots, un par face, chacun Euler = 1.
	{
		NkEditMesh m;
		MakeCube(v, idx);
		Build(m, v, idx, true);
		seams.Clear();
		for (uint32 e = 0; e < (uint32)m.edges.Size(); ++e)
			if (m.edges[e].alive) seams.PushBack((NkEmId)e);
		NkUVUnwrapParams pr;
		pr.seams = seams.Data();
		pr.seamCount = (uint32)seams.Size();
		NkUVResult res;
		NkVector<NkUVIslandInfo> isl;
		const bool ok = NkUVUnwrap(m, pr, res, &isl);
		bool tousDisques = (isl.Size() == 6u);
		for (uint32 i = 0; i < (uint32)isl.Size(); ++i)
			if (isl[i].euler != 1 || isl[i].vertCount != 4u || isl[i].edgeCount != 4u) tousDisques = false;
		snprintf(buf, sizeof(buf), "coutures=%u ilots=%u (attendu 6)  chaque ilot V=4 E=4 euler=1 : %s",
				 (uint32)seams.Size(), res.islandCount, tousDisques ? "oui" : "NON");
		Check("u1/A3-12-coutures-6-ilots", ok && res.islandCount == 6u && tousDisques, buf);
	}

	// NEGATIF N1 — une couture EN MOINS que A3 : deux faces restent reliees.
	// ATTENDU : 5 ilots, PAS 6. Si le compteur rend encore 6, il ne lit pas les
	// coutures : il compte les faces.
	{
		NkEditMesh m;
		MakeCube(v, idx);
		Build(m, v, idx, true);
		seams.Clear();
		bool sauteUne = false;
		for (uint32 e = 0; e < (uint32)m.edges.Size(); ++e) {
			if (!m.edges[e].alive) continue;
			if (!sauteUne) {
				sauteUne = true;
				continue;
			}
			seams.PushBack((NkEmId)e);
		}
		NkUVUnwrapParams pr;
		pr.seams = seams.Data();
		pr.seamCount = (uint32)seams.Size();
		NkUVResult res;
		NkVector<NkUVIslandInfo> isl;
		NkUVUnwrap(m, pr, res, &isl);
		snprintf(buf, sizeof(buf), "coutures=%u ilots=%u (ATTENDU 5, un 6 signerait un compteur aveugle)",
				 (uint32)seams.Size(), res.islandCount);
		Check("u1/N1-negatif-5-ilots", res.islandCount == 5u, buf);
	}

	// =========================================================================
	// (u2) RECOUVREMENT — le compteur est prouve SUR ZERO d'abord
	// =========================================================================
	// B0 — deux triangles UV franchement a cote : 0 paire, aire 0.000000.
	// Tant que ce chiffre n'est pas zero, aucun autre resultat de (u2) ne vaut.
	{
		NkEditMesh m;
		MakeTwoQuads(v, idx);
		Build(m, v, idx, true);
		// UV posees A LA MAIN, cote a cote, sans chevauchement.
		for (uint32 i = 0; i < (uint32)m.verts.Size(); ++i) {
			const float32 ox = (i < 4u) ? 0.f : 2.f;
			const float32 lx[4] = {0.f, 1.f, 1.f, 0.f};
			const float32 ly[4] = {0.f, 0.f, 1.f, 1.f};
			m.verts[i].uv = NkVec2f{ox + lx[i % 4u], ly[i % 4u]};
		}
		float32 aire = -1.f;
		const uint32 pairs = NkUVCountOverlaps(m, &aire);
		snprintf(buf, sizeof(buf), "paires=%u (attendu 0)  aire=%.9f (attendu 0.000000000)", pairs, (double)aire);
		Check("u2/B0-compteur-sur-zero", pairs == 0u && aire == 0.f, buf);
	}

	// B1 — cube deplie en croix : 0 paire.
	{
		NkEditMesh m;
		MakeCube(v, idx);
		Build(m, v, idx, true);
		const uint32 n = SeamsFromDualSpanningTree(m, seams, 0u);
		NkUVUnwrapParams pr;
		pr.seams = seams.Data();
		pr.seamCount = n;
		NkUVResult res;
		const bool ok = NkUVUnwrap(m, pr, res);
		float32 aire = -1.f;
		const uint32 pairs = NkUVCountOverlaps(m, &aire);
		snprintf(buf, sizeof(buf), "paires=%u (attendu 0) aire=%.9f", pairs, (double)aire);
		Check("u2/B1-croix-sans-recouvr", ok && pairs == 0u, buf);
	}

	// B2 — les 6 ilots de A3, ranges cote a cote : 0 paire.
	{
		NkEditMesh m;
		MakeCube(v, idx);
		Build(m, v, idx, true);
		seams.Clear();
		for (uint32 e = 0; e < (uint32)m.edges.Size(); ++e)
			if (m.edges[e].alive) seams.PushBack((NkEmId)e);
		NkUVUnwrapParams pr;
		pr.seams = seams.Data();
		pr.seamCount = (uint32)seams.Size();
		pr.packIslands = true;
		NkUVResult res;
		const bool ok = NkUVUnwrap(m, pr, res);
		float32 aire = -1.f;
		const uint32 pairs = NkUVCountOverlaps(m, &aire);
		snprintf(buf, sizeof(buf), "ilots=%u paires=%u (attendu 0) aire=%.9f", res.islandCount, pairs,
				 (double)aire);
		Check("u2/B2-6-ilots-ranges", ok && pairs == 0u, buf);
	}

	// NEGATIF N2 (OBLIGATOIRE) — l'ilot 1 translate EXACTEMENT sur l'ilot 0.
	// ATTENDU CHIFFRE : deux quads identiques superposes, chacun coupe en
	// eventail en (0,1,2) et (0,2,3). Les triangles HOMOLOGUES se recouvrent
	// entierement (2 paires) ; les croises ne se touchent que sur la diagonale,
	// donc aire nulle et non comptees. => 2 paires, aire = aire d'une face.
	// (Cet attendu corrige celui de R6, qui annoncait 4 paires sans tenir compte
	// des deux paires croisees d'aire nulle. Corrige PAR RAISONNEMENT avant la
	// mesure, pas apres avoir vu le chiffre.)
	{
		NkEditMesh m;
		MakeCube(v, idx);
		Build(m, v, idx, true);
		seams.Clear();
		for (uint32 e = 0; e < (uint32)m.edges.Size(); ++e)
			if (m.edges[e].alive) seams.PushBack((NkEmId)e);
		NkUVUnwrapParams pr;
		pr.seams = seams.Data();
		pr.seamCount = (uint32)seams.Size();
		NkUVResult res;
		NkVector<NkUVIslandInfo> isl;
		NkVector<NkEmId> ifaces;
		const bool okN2 = NkUVUnwrap(m, pr, res, &isl, &ifaces);
		if (!okN2 || isl.Size() < 2u) {
			// Le negatif ne PEUT PAS se mettre en place : on le DIT, au lieu de
			// lire hors du tableau et d'imprimer un chiffre qui aurait l'air d'un
			// resultat. Un banc qui ne peut pas monter son essai doit rougir pour
			// cette raison-la, pas pour celle qu'il croyait mesurer.
			snprintf(buf, sizeof(buf), "montage impossible : deplie=%s ilots=%u (il en faut 2)",
					 okN2 ? "oui" : "non", (uint32)isl.Size());
			Check("u2/N2-negatif-superpose", false, buf);
		} else {
			// Translation de l'ilot 1 sur l'ilot 0 : on deplace les coins de SES faces.
			const NkVec2f d{isl[0].uvMin.x - isl[1].uvMin.x, isl[0].uvMin.y - isl[1].uvMin.y};
			NkVector<NkEmId> fv;
			for (uint32 j = 0; j < isl[1].faceCount; ++j) {
				fv.Clear();
				m.GetFaceVerts(ifaces[isl[1].firstFace + j], fv);
				for (uint32 k = 0; k < (uint32)fv.Size(); ++k) {
					m.verts[fv[k]].uv = NkVec2f{m.verts[fv[k]].uv.x + d.x, m.verts[fv[k]].uv.y + d.y};
				}
			}
			float32 aire = -1.f;
			const uint32 pairs = NkUVCountOverlaps(m, &aire);
			// LE CRITERE PORTE SUR L'AIRE, PAS SUR LE NOMBRE DE PAIRES, et voici
			// pourquoi — c'est la mesure qui m'a repris.
			//
			// J'avais annonce « 2 paires » en supposant que deux faces du cube se
			// deplieraient avec la MEME orientation, donc avec des diagonales de
			// triangulation confondues (les paires croisees se reduisant alors a un
			// segment, d'aire nulle). Cette hypothese portait sur un detail du
			// solveur que je n'avais jamais mesure : chaque ilot choisit ses deux
			// pins independamment, donc l'orientation du carre dans le plan UV
			// n'a aucune raison de coincider d'une face a l'autre. Le compte de
			// paires vaut 2, 3 ou 4 selon cette orientation — ce n'est PAS un
			// invariant, et un attendu pose dessus aurait rougi sur un montage
			// correct.
			//
			// L'AIRE, elle, etait predite AVANT la mesure et l'est restee : deux
			// faces du cube unite exactement superposees se recouvrent sur l'aire
			// d'une face entiere, soit 1.0, quelle que soit la triangulation.
			// C'est l'invariant, et c'est donc lui le critere.
			const float32 aireAttendue = 1.f; // aire d'une face du cube unite
			const float32 ecart = (aire > aireAttendue) ? (aire - aireAttendue) : (aireAttendue - aire);
			snprintf(buf, sizeof(buf),
					 "aire=%.6f (ATTENDU %.1f = une face)  paires=%u (>=2 ; pas un invariant, cf. code)",
					 (double)aire, (double)aireAttendue, pairs);
			Check("u2/N2-negatif-superpose", ecart < 1e-4f && pairs >= 2u, buf);
		}
	}

	// N2b — LE COMPTAGE DE PAIRES, LUI, SUR UN CAS MAITRISE.
	// N2 ne peut pas prouver le compte (il depend de l'orientation). Ici les UV
	// sont posees A LA MAIN : les deux quads sont EXACTEMENT superposes, memes
	// sommets dans le meme ordre, donc memes diagonales. Les deux paires
	// homologues se recouvrent entierement, les deux croisees se reduisent a la
	// diagonale (aire nulle, non comptee).
	// ATTENDU : exactement 2 paires, aire = 1.0 (le carre unite).
	{
		NkEditMesh m;
		MakeTwoQuads(v, idx);
		Build(m, v, idx, true);
		for (uint32 i = 0; i < (uint32)m.verts.Size(); ++i) {
			const float32 lx[4] = {0.f, 1.f, 1.f, 0.f};
			const float32 ly[4] = {0.f, 0.f, 1.f, 1.f};
			m.verts[i].uv = NkVec2f{lx[i % 4u], ly[i % 4u]}; // les DEUX au meme endroit
		}
		float32 aire = -1.f;
		const uint32 pairs = NkUVCountOverlaps(m, &aire);
		snprintf(buf, sizeof(buf), "paires=%u (ATTENDU 2)  aire=%.6f (ATTENDU 1.0)", pairs, (double)aire);
		Check("u2/N2b-comptage-maitrise", pairs == 2u && aire > 0.999f && aire < 1.001f, buf);
	}

	// =========================================================================
	// (u3) DISTORSION
	// =========================================================================
	// C2 AVANT C1 : la mesure se prouve AVANT le solveur. Un quad plan dont on
	// pose les UV ISOMETRIQUES A LA MAIN doit rendre 0. Si C2 rend deja non-nul,
	// c'est la MESURE qui ment ; si seul C1 rend non-nul, c'est le SOLVEUR.
	// Aucune ambiguite n'est alors possible, et c'est tout l'interet de l'ordre.
	const float32 kSeuil = 1e-5f; // eps(float32)=1,19e-7, amplifie par le
								  // conditionnement du systeme 4x4 du quad.
	{
		NkEditMesh m;
		MakeQuad(v, idx, 3.f, 2.f);
		Build(m, v, idx, true);
		for (uint32 i = 0; i < (uint32)m.verts.Size(); ++i)
			m.verts[i].uv = NkVec2f{m.verts[i].pos.x, m.verts[i].pos.y}; // isometrie exacte
		NkUVDistortion d;
		const bool ok = NkUVMeasureDistortion(m, d);
		const float32 ecartAire = (d.areaMax - 1.f > 1.f - d.areaMin) ? (d.areaMax - 1.f) : (1.f - d.areaMin);
		snprintf(buf, sizeof(buf), "aire[%.9f..%.9f] ecart=%.3e  angle max=%.3e deg  tris=%u",
				 (double)d.areaMin, (double)d.areaMax, (double)ecartAire, (double)d.angleMax, d.triCount);
		Check("u3/C2-mesure-sur-isometrie", ok && ecartAire < kSeuil && d.angleMax < kSeuil, buf);
	}

	// C1 — LE CAS DONT ON CONNAIT LA REPONSE : un quad plan deplie par le
	// solveur. Rapport d'aire min = moyenne = max = 1 ; ecart d'angle = 0.
	// Le chiffre BRUT est imprime, pas seulement « sous le seuil » : un 1e-6 et
	// un 1e-14 passeraient tous les deux, et ils ne disent pas la meme chose.
	{
		NkEditMesh m;
		MakeQuad(v, idx, 3.f, 2.f);
		Build(m, v, idx, true);
		NkUVUnwrapParams pr; // un quad est deja un disque : aucune couture requise
		NkUVResult res;
		const bool ok = NkUVUnwrap(m, pr, res);
		const NkUVDistortion &d = res.distortion;
		const float32 ecartAire = (d.areaMax - 1.f > 1.f - d.areaMin) ? (d.areaMax - 1.f) : (1.f - d.areaMin);
		snprintf(buf, sizeof(buf),
				 "aire[%.9f..%.9f] ecart=%.3e  angle max=%.3e deg  residu CG=%.3e  it=%u",
				 (double)d.areaMin, (double)d.areaMax, (double)ecartAire, (double)d.angleMax,
				 (double)res.cgResidual, res.cgIterationsUsed);
		Check("u3/C1-quad-plan-zero", ok && ecartAire < kSeuil && d.angleMax < kSeuil, buf);
	}

	// NEGATIF N3 — une sphere : fortement courbee, NON developpable.
	// ATTENDU : rapport d'aire max > 1,5 ET ecart d'angle max > 5 degres.
	// Si une sphere rend 0, la mesure ne mesure rien du tout.
	{
		NkEditMesh m;
		MakeSphere(10u, 16u, false, v, idx); // un sommet PAR COIN, grille OUVERTE
		Build(m, v, idx, true);
		// AUCUNE COUTURE : la grille est deja un disque (Euler = 1). En poser
		// reviendrait a la decouper en patron, ce qui annulerait la distorsion par
		// construction et viderait ce negatif de tout pouvoir de refutation.
		const uint32 n = 0u;
		NkUVUnwrapParams pr;
		NkUVResult res;
		const bool ok = NkUVUnwrap(m, pr, res);
		const NkUVDistortion &d = res.distortion;
		// ⚠ LA CONDITION D'ESSAI EST IMPRIMEE AVEC LE RESULTAT, et pas seulement
		// le resultat. Sans le nombre d'ILOTS, une distorsion nulle sur une sphere
		// se lit comme un solveur miraculeux au lieu de ce qu'elle est : un
		// maillage decoupe en autant d'ilots que de faces, dont chacune est
		// presque plane et se deplie donc sans rien deformer. Le banc mesurerait
		// alors la planeite d'un quad en croyant mesurer la courbure d'une sphere.
		uint32 liveF = 0u, liveE = 0u;
		for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f)
			if (m.faces[f].alive) ++liveF;
		for (uint32 e = 0; e < (uint32)m.edges.Size(); ++e)
			if (m.edges[e].alive) ++liveE;
		snprintf(buf, sizeof(buf),
				 "%s F=%u E=%u coutures=%u ILOTS=%u (ATTENDU 1) tris=%u | aire max=%.4f (>1.5)  angle "
				 "max=%.3f deg (>5)",
				 ok ? "" : NkUVRefusName(res.refus), liveF, liveE, n, res.islandCount, d.triCount,
				 (double)d.areaMax, (double)d.angleMax);
		Check("u3/N3-negatif-sphere", ok && res.islandCount == 1u && d.areaMax > 1.5f && d.angleMax > 5.f, buf);
	}

	// N5 — LE DEFAUT QUE LE NEGATIF N3 A TROUVE, DEVENU UN CONTROLE PERMANENT.
	// Meme sphere, mais aux sommets SOUDES (un sommet pour 4 faces, l'import
	// classique). Une couture n'y est pas representable : un seul champ `uv` par
	// `Vert`. La premiere version rendait `NK_UV_ILOT_NON_DISQUE` — juste sur le
	// symptome (la soudure effondre le compte des sommets, donc Euler s'ecarte de
	// 1) et FAUX sur la cause, ce qui envoie corriger la topologie d'un maillage
	// qui n'a rien de casse.
	// ATTENDU : refus NK_UV_COINS_SOUDES, avec un nombre de coins > 0.
	{
		NkEditMesh m;
		MakeSphere(10u, 16u, true, v, idx); // sommets PARTAGES
		Build(m, v, idx, true);
		const uint32 n = SeamsFromDualSpanningTree(m, seams, 0u);
		NkUVUnwrapParams pr;
		pr.seams = seams.Data();
		pr.seamCount = n;
		NkUVResult res;
		const bool ok = NkUVUnwrap(m, pr, res);
		snprintf(buf, sizeof(buf), "refus=%s (ATTENDU NK_UV_COINS_SOUDES)  coins soudes=%u (ATTENDU >0)",
				 NkUVRefusName(res.refus), res.weldedCorners);
		Check("u1/N5-coins-soudes-nomme", !ok && res.refus == NkUVRefus::CoinsSoudes && res.weldedCorners > 0u,
			  buf);
	}

	// N5b — et le compteur de coins soudes est prouve SUR ZERO : le cube du depot
	// a un Vert par coin, donc aucune couture n'y est jamais empechee.
	// Un compteur qui n'a pas ete vu rendre 0 peut compter n'importe quoi.
	{
		NkEditMesh m;
		MakeCube(v, idx);
		Build(m, v, idx, true);
		const uint32 n = SeamsFromDualSpanningTree(m, seams, 0u);
		NkUVUnwrapParams pr;
		pr.seams = seams.Data();
		pr.seamCount = n;
		NkUVResult res;
		const bool ok = NkUVUnwrap(m, pr, res);
		snprintf(buf, sizeof(buf), "coins soudes=%u (ATTENDU 0)  deplie=%s", res.weldedCorners,
				 ok ? "oui" : "non");
		Check("u1/N5b-soudes-sur-zero", ok && res.weldedCorners == 0u, buf);
	}

	// =========================================================================
	// (c1..c3) DE-SOUDURE DES COUTURES — NkEditMesh::SplitEdges
	// Le mur reel : un modele importe SANS UV arrive avec ses sommets SOUDES
	// (NkOBJLoader:292 fabrique un sommet par combinaison position/UV/normale,
	// donc une seule cle par position quand il n'y a pas d'UV), et un sommet
	// soude ne peut pas porter deux UV de part et d'autre d'une couture.
	// =========================================================================

	// Positions comparees OCTET POUR OCTET. Une comparaison a epsilon laisserait
	// passer exactement ce qu'on veut interdire : un solveur qui deplace les
	// sommets « un peu » est deja un solveur qui ment.
	auto MemePosition = [](const NkVec3f &a, const NkVec3f &b) -> bool {
		return memcmp(&a, &b, sizeof(NkVec3f)) == 0;
	};

	// c1 — LA SPHERE SOUDEE SE DEPLIE APRES DE-SOUDURE
	{
		NkEditMesh m;
		MakeSphere(10u, 16u, true, v, idx); // sommets PARTAGES, comme un import sans UV
		Build(m, v, idx, true);

		// ATTENDUS DERIVES DES COMPTES DU MAILLAGE, jamais recopies :
		//   arbre couvrant du dual = F-1 aretes, chacune fusionnant 2 paires de coins
		//   V' = (somme des cotes) - 2(F-1)     E' = (somme des cotes) - (F-1)
		//   dupliques = V' - V                  Euler = V' - E' + F = 1
		uint32 F = 0u, sides = 0u;
		NkVector<NkEmId> fvv;
		for (uint32 f = 0; f < (uint32)m.faces.Size(); ++f) {
			if (!m.faces[f].alive) continue;
			++F;
			fvv.Clear();
			m.GetFaceVerts((NkEmId)f, fvv);
			sides += (uint32)fvv.Size();
		}
		const uint32 V0 = (uint32)m.verts.Size();
		const uint32 tree = (F > 0u) ? (F - 1u) : 0u;
		const uint32 vAttendu = sides - 2u * tree;
		const uint32 eAttendu = sides - tree;
		const uint32 dupAttendu = vAttendu - V0;

		NkVector<NkVec3f> posAvant;
		for (uint32 i = 0; i < V0; ++i) posAvant.PushBack(m.verts[i].pos);

		const uint32 n = SeamsFromDualSpanningTree(m, seams, 0u);

		// ⚠ ON MEMORISE LES COUTURES PAR POSITION, PAS PAR INDICE.
		// `SplitEdges` reconstruit le maillage : les indices d'aretes d'apres n'ont
		// aucun rapport avec ceux d'avant. Les POSITIONS, elles, sont conservees au
		// bit (c'est tout l'objet de `keepGeometry`), donc elles servent de pont.
		NkVector<NkVec3f> seamA, seamB;
		for (uint32 i = 0; i < n; ++i) {
			const NkEmId e = seams[i];
			seamA.PushBack(m.verts[m.edges[e].v0].pos);
			seamB.PushBack(m.verts[m.edges[e].v1].pos);
		}

		uint32 dup = 0u;
		const bool split = m.SplitEdges(seams.Data(), n, true, &dup);
		m.RebuildEdges();

		// ⚠ ET ON REPASSE LES COUTURES AU SOLVEUR. J'avais ecrit ici que la decoupe
		// suffisait, « les ilots se forment d'eux-memes ». FAUX, et la mesure me l'a
		// dit : E restait a 272 et Euler a 0. L'identite soudee de NkEditMesh est
		// SPATIALE — `RebuildEdges` refusionne par position, et une de-soudure qui ne
		// deplace rien est donc invisible pour elle. Le maillage se recoud.
		// De-soudure et coutures ne sont pas alternatives : la premiere donne a chaque
		// coin le DROIT de porter son UV, la seconde dit ou couper la connexite.
		NkVector<NkEmId> seams2;
		for (uint32 k = 0; k < (uint32)seamA.Size(); ++k) {
			for (uint32 e = 0; e < (uint32)m.edges.Size(); ++e) {
				if (!m.edges[e].alive) continue;
				const NkVec3f &p0 = m.verts[m.edges[e].v0].pos;
				const NkVec3f &p1 = m.verts[m.edges[e].v1].pos;
				const bool direct = (memcmp(&p0, &seamA[k], sizeof(NkVec3f)) == 0) &&
									(memcmp(&p1, &seamB[k], sizeof(NkVec3f)) == 0);
				const bool inverse = (memcmp(&p0, &seamB[k], sizeof(NkVec3f)) == 0) &&
									 (memcmp(&p1, &seamA[k], sizeof(NkVec3f)) == 0);
				if (direct || inverse) {
					seams2.PushBack((NkEmId)e);
					break;
				}
			}
		}
		NkUVUnwrapParams pr;
		pr.seams = seams2.Data();
		pr.seamCount = (uint32)seams2.Size();
		NkUVResult res;
		NkVector<NkUVIslandInfo> isl;
		const bool ok = NkUVUnwrap(m, pr, res, &isl);
		const int32 euler = (isl.Size() > 0u) ? isl[0].euler : -999;
		// La CONDITION D'ESSAI est publiee avec le resultat : si l'appariement des
		// coutures par position en retrouvait moins que `n`, le depliage rougirait
		// pour une raison qui n'a rien a voir avec le depliage.
		snprintf(buf, sizeof(buf),
				 "V %u->%u (att %u)  dup=%u (att %u)  coutures %u->%u  E=%u/%u euler=%d/1 ilots=%u "
				 "soudes=%u aire max=%.3f",
				 V0, (uint32)m.verts.Size(), vAttendu, dup, dupAttendu, n, (uint32)seams2.Size(),
				 (isl.Size() > 0u) ? isl[0].edgeCount : 0u, eAttendu, euler, res.islandCount,
				 res.weldedCorners, (double)res.distortion.areaMax);
		Check("c1/sphere-soudee-se-deplie",
			  split && (uint32)m.verts.Size() == vAttendu && dup == dupAttendu &&
				  (uint32)seams2.Size() == n && ok && res.islandCount == 1u && euler == 1 &&
				  res.weldedCorners == 0u && res.distortion.areaMax > 1.f,
			  buf);

		// c2 — LA DE-SOUDURE N'A DEPLACE AUCUNE POSITION, AU BIT.
		// Si le `gap` automatique de 1 % avait joue, TOUS les sommets touches
		// auraient bouge et ce controle rougirait en masse.
		uint32 inconnues = 0u;
		for (uint32 i = 0; i < (uint32)m.verts.Size(); ++i) {
			bool trouve = false;
			for (uint32 j = 0; j < (uint32)posAvant.Size() && !trouve; ++j)
				if (MemePosition(m.verts[i].pos, posAvant[j])) trouve = true;
			if (!trouve) ++inconnues;
		}
		snprintf(buf, sizeof(buf), "positions inconnues apres de-soudure : %u (ATTENDU 0, memcmp exact)",
				 inconnues);
		Check("c2/de-soudure-ne-bouge-rien", inconnues == 0u, buf);
	}

	// c1-NEG — AUCUNE COUTURE -> AUCUN SOMMET DUPLIQUE, GEOMETRIE INTACTE.
	// Par quel mecanisme pourrait-il echouer ? Par le `gap` automatique, et par
	// une de-soudure qui s'emballerait sur une liste vide. Il surveille donc les
	// deux facons exactes dont ce lot pouvait mal tourner — il n'est pas impuissant.
	{
		NkEditMesh m;
		MakeSphere(10u, 16u, true, v, idx);
		Build(m, v, idx, true);
		const uint32 V0 = (uint32)m.verts.Size();
		NkVector<NkVec3f> posAvant;
		for (uint32 i = 0; i < V0; ++i) posAvant.PushBack(m.verts[i].pos);

		uint32 dup = 123456u; // sentinelle : l'appel DOIT la remettre a zero
		const bool r = m.SplitEdges(seams.Data(), 0u, true, &dup);

		bool identique = ((uint32)m.verts.Size() == V0);
		for (uint32 i = 0; i < V0 && identique; ++i)
			if (!MemePosition(m.verts[i].pos, posAvant[i])) identique = false;
		snprintf(buf, sizeof(buf), "rend %s  dup=%u (ATTENDU 0)  V=%u/%u  identique au bit : %s",
				 r ? "true" : "false", dup, (uint32)m.verts.Size(), V0, identique ? "oui" : "NON");
		Check("c1/N-aucune-couture", !r && dup == 0u && identique, buf);
	}

	// c3 — L'ANCIEN CHEMIN EST INTACT.
	// `SplitSelectedEdges` doit se comporter comme avant, ECART COMPRIS : c'est un
	// « rip » facon Blender, il DOIT ecarter. Le contraste avec c2 prouve les deux
	// d'un coup — meme decoupe, deux politiques d'ecart, chacune tenant son contrat.
	// Le juge principal reste `editmesh_baseline.txt`, qui doit rester identique au
	// bit : ce banc-ci ne le remplace pas, il le complete.
	{
		NkEditMesh m;
		MakeCube(v, idx);
		Build(m, v, idx, true);
		NkVector<NkVec3f> posAvant;
		for (uint32 i = 0; i < (uint32)m.verts.Size(); ++i) posAvant.PushBack(m.verts[i].pos);
		m.SelectAll();
		const bool r = m.SplitSelectedEdges();
		m.RebuildEdges();
		uint32 bouges = 0u;
		for (uint32 i = 0; i < (uint32)m.verts.Size(); ++i) {
			bool trouve = false;
			for (uint32 j = 0; j < (uint32)posAvant.Size() && !trouve; ++j)
				if (MemePosition(m.verts[i].pos, posAvant[j])) trouve = true;
			if (!trouve) ++bouges;
		}
		snprintf(buf, sizeof(buf),
				 "rend %s  V=%u  sommets ECARTES=%u (ATTENDU >0 : ecarter est son contrat)",
				 r ? "true" : "false", (uint32)m.verts.Size(), bouges);
		Check("c3/ancien-chemin-ecarte", r && bouges > 0u, buf);
	}

	printf("\n--- %u tests executes, %u echec(s) ---\n", gTests, gFail);
	printf("(u4) se joue en DEUX PROCESSUS : --u4-ecrire puis --u4-relire\n");
	return (gFail == 0u) ? 0 : 1;
}
