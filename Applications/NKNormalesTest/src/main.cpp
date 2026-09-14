// =============================================================================
// Applications/NKNormalesTest/src/main.cpp
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// POURQUOI CE PROGRAMME EXISTE
// -----------------------------------------------------------------------------
// `RecomputeNormals` est l'une des vingt-et-une fonctions qui dependent de la
// SOUDURE des sommets coincidents : elle moyenne les normales des faces
// incidentes a l'identite SOUDEE. Si la soudure disparait, chaque coin se
// retrouve seul, la moyenne porte sur une seule face, et TOUT OMBRAGE LISSE
// DISPARAIT -- une sphere devient un tas de facettes.
//
// Or cette fonction n'avait AUCUN banc. Sa regression se verrait a l'œil, sur
// une sphere, et personne ne regarde une sphere a chaque commit. C'est donc la
// dependance dont la casse serait la plus visible pour l'utilisateur et la moins
// signalee par l'outillage -- exactement le pire des deux.
//
// LE CRITERE NE DEPEND D'AUCUNE VALEUR ENREGISTREE
// -----------------------------------------------------------------------------
// Pas d'empreinte de reference a maintenir, pas de fichier a regenerer : sur une
// sphere CENTREE A L'ORIGINE, la normale lissee d'un sommet doit etre COLINEAIRE
// a sa position normalisee. C'est une propriete geometrique, vraie quelle que
// soit la finesse du maillage, et qu'aucune valeur en dur ne peut perimer.
// Un attendu en dur se perime sans prevenir ; une propriete, non.
//
// ET LE NEGATIF EST DANS LA MEME MESURE
// -----------------------------------------------------------------------------
// Sur un CUBE, cette colinearite est FAUSSE par construction (les normales
// lissees d'un coin pointent vers la diagonale, pas vers le coin). Le banc exige
// donc que la sphere passe ET que le cube echoue : un critere qui serait vert
// sur les deux ne mesurerait pas la colinearite, il mesurerait qu'il existe des
// normales.
//
// CE QU'IL NE MESURE PAS
// -----------------------------------------------------------------------------
//   * l'ombrage a l'ecran : il n'y a ni GPU ni image ici, seulement les normales
//     calculees sur le maillage editable ;
//   * l'ombrage FLAT (Shade Flat), qui dedouble les coins a l'affichage et suit
//     un autre chemin.
// =============================================================================
#include "NKRenderer/Mesh/NkEditMesh.h"

#include <stdio.h>
#include <math.h>

using namespace nkentseu;
using namespace nkentseu::renderer;

namespace {

	int32 gPass = 0, gFail = 0;

	void Cas(bool ok, const char *quoi) {
		if (ok) {
			gPass++;
			printf("  OK    %s\n", quoi);
		} else {
			gFail++;
			printf("  FAIL  %s\n", quoi);
		}
	}

	// Sphere UV centree a l'origine, rayon 1. Le generateur du harnais est
	// `static` dans son propre main.cpp, donc hors d'atteinte : celui-ci est
	// minimal et n'a qu'un seul devoir -- produire une sphere dont chaque sommet
	// est a distance 1 de l'origine, ce que le banc VERIFIE avant de s'en servir.
	void MakeSphere(uint32 stacks, uint32 slices, NkVector<NkVertex3D> &v,
					NkVector<uint32> &idx) {
		v.Clear();
		idx.Clear();
		const float32 kPi = 3.14159265358979f;
		for (uint32 i = 0; i <= stacks; ++i) {
			const float32 phi = kPi * (float32)i / (float32)stacks;
			for (uint32 j = 0; j <= slices; ++j) {
				const float32 th = 2.f * kPi * (float32)j / (float32)slices;
				NkVertex3D vt;
				vt.pos = {sinf(phi) * cosf(th), cosf(phi), sinf(phi) * sinf(th)};
				vt.normal = vt.pos; // sera RECALCULEE ; on ne veut pas la croire sur parole
				vt.uv = {(float32)j / (float32)slices, (float32)i / (float32)stacks};
				v.PushBack(vt);
			}
		}
		for (uint32 i = 0; i < stacks; ++i)
			for (uint32 j = 0; j < slices; ++j) {
				const uint32 a = i * (slices + 1) + j, b = a + slices + 1;
				idx.PushBack(a); idx.PushBack(b); idx.PushBack(a + 1);
				idx.PushBack(a + 1); idx.PushBack(b); idx.PushBack(b + 1);
			}
	}

	void MakeCube(NkVector<NkVertex3D> &v, NkVector<uint32> &idx) {
		v.Clear();
		idx.Clear();
		static const float32 P[8][3] = {{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
										{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
		static const uint32 F[12][3] = {{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},
										{2,3,7},{2,7,6},{1,2,6},{1,6,5},{0,4,7},{0,7,3}};
		for (int32 i = 0; i < 8; ++i) {
			NkVertex3D vt;
			vt.pos = {P[i][0], P[i][1], P[i][2]};
			v.PushBack(vt);
		}
		for (int32 f = 0; f < 12; ++f)
			for (int32 k = 0; k < 3; ++k)
				idx.PushBack(F[f][k]);
	}

	// La colinearite normale <-> position, mesuree par le PRODUIT SCALAIRE des
	// deux vecteurs unitaires : 1 = parfaitement aligne. Rend le PIRE cas, parce
	// qu'une moyenne masquerait un seul sommet devenu facette.
	float32 PireColinearite(const NkEditMesh &m) {
		float32 pire = 2.f;
		for (uint32 i = 0; i < (uint32)m.verts.Size(); ++i) {
			const NkVec3f p = m.verts[i].pos, n = m.verts[i].normal;
			const float32 lp = sqrtf(p.x * p.x + p.y * p.y + p.z * p.z);
			const float32 ln = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
			if (lp < 1e-6f || ln < 1e-6f)
				continue; // le pole ou une normale nulle : traites a part
			const float32 d = (p.x * n.x + p.y * n.y + p.z * n.z) / (lp * ln);
			if (d < pire)
				pire = d;
		}
		return pire;
	}

	// Toutes les normales sont-elles unitaires ? Une normale non normalisee ne se
	// verrait pas sur la colinearite (elle divise par la longueur) et pourtant
	// l'eclairage en dependrait.
	float32 PireEcartUnite(const NkEditMesh &m) {
		float32 pire = 0.f;
		for (uint32 i = 0; i < (uint32)m.verts.Size(); ++i) {
			const NkVec3f n = m.verts[i].normal;
			const float32 ln = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
			const float32 e = fabsf(ln - 1.f);
			if (e > pire)
				pire = e;
		}
		return pire;
	}

} // namespace

int main() {
	printf("=== NKNormalesTest : l'ombrage lisse survit-il ? (console, sans GPU) ===\n");

	NkVector<NkVertex3D> v;
	NkVector<uint32> idx;

	// ── LE ZERO : le banc mesure-t-il ce qu'il croit ? ──────────────────────
	printf("\n-- LE ZERO : la sphere est-elle une sphere ? --\n");
	MakeSphere(24, 24, v, idx);
	{
		float32 pireR = 0.f;
		for (uint32 i = 0; i < (uint32)v.Size(); ++i) {
			const NkVec3f p = v[i].pos;
			const float32 r = sqrtf(p.x * p.x + p.y * p.y + p.z * p.z);
			const float32 e = fabsf(r - 1.f);
			if (e > pireR)
				pireR = e;
		}
		char q[128];
		snprintf(q, sizeof(q), "Z1 tous les sommets a distance 1 de l'origine (pire ecart %.6f)",
				 (double)pireR);
		Cas(pireR < 1e-5f, q);
	}

	NkEditMesh sphere;
	sphere.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
	sphere.RecomputeNormals();

	printf("\n-- LA SPHERE : normale lissee colineaire a la position --\n");
	{
		const float32 pire = PireColinearite(sphere);
		char q[160];
		snprintf(q, sizeof(q),
				 "P1 la PIRE colinearite vaut %.6f (attendu > 0,999 ; 1 = parfait)",
				 (double)pire);
		// 0,999 n'est pas un seuil de confort : sur une sphere a 24x24, l'ecart
		// theorique du au maillage est de l'ordre de 1e-3 au pole. Un seuil plus
		// serre rougirait sur un maillage correct -- un attendu en dur se perime,
		// une tolerance DERIVEE de la finesse ne se perime pas.
		Cas(pire > 0.999f, q);
	}
	{
		const float32 e = PireEcartUnite(sphere);
		char q[140];
		snprintf(q, sizeof(q), "P2 toutes les normales sont unitaires (pire ecart %.6f)", (double)e);
		Cas(e < 1e-4f, q);
	}

	// ── LE NEGATIF : le cube doit ECHOUER au meme critere ───────────────────
	printf("\n-- LE NEGATIF : le cube ne doit PAS passer le meme critere --\n");
	MakeCube(v, idx);
	NkEditMesh cube;
	cube.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
	cube.RecomputeNormals();
	{
		const float32 pire = PireColinearite(cube);
		char q[180];
		snprintf(q, sizeof(q),
				 "N1 la pire colinearite du cube vaut %.6f : elle DOIT etre loin de 1",
				 (double)pire);
		// Si ce critere passait aussi sur le cube, P1 ne mesurerait pas la
		// colinearite -- il mesurerait qu'il existe des normales.
		Cas(pire < 0.99f, q);
	}
	{
		const float32 e = PireEcartUnite(cube);
		char q[150];
		snprintf(q, sizeof(q),
				 "N2 mais les normales du cube restent unitaires (pire ecart %.6f)", (double)e);
		Cas(e < 1e-4f, q);
	}

	// ── CE QUI SE CASSERAIT SANS LA SOUDURE ─────────────────────────────────
	// Sans soudure, chaque coin est seul : sa normale serait celle d'UNE face, et
	// deux coins coincidents de faces differentes auraient des normales
	// differentes. Ce critere mesure exactement ca sur la sphere, ou les sommets
	// SONT partages : deux sommets a la meme position doivent porter la MEME
	// normale.
	printf("\n-- LA SOUDURE A L'ŒUVRE : deux sommets coincidents, une seule normale --\n");
	{
		uint32 paires = 0, divergentes = 0;
		const uint32 n = (uint32)sphere.verts.Size();
		for (uint32 i = 0; i < n; ++i)
			for (uint32 j = i + 1; j < n; ++j) {
				const NkVec3f a = sphere.verts[i].pos, b = sphere.verts[j].pos;
				const float32 dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
				if (dx * dx + dy * dy + dz * dz > 1e-8f)
					continue;
				++paires;
				const NkVec3f na = sphere.verts[i].normal, nb = sphere.verts[j].normal;
				const float32 ex = na.x - nb.x, ey = na.y - nb.y, ez = na.z - nb.z;
				if (ex * ex + ey * ey + ez * ez > 1e-8f)
					++divergentes;
			}
		char q[190];
		snprintf(q, sizeof(q),
				 "S1 %u paires de sommets coincidents, %u portent des normales DIFFERENTES",
				 paires, divergentes);
		// `paires > 0` fait partie du critere : une sphere UV referme sa derniere
		// tranche sur la premiere, il y EN A. Si ce compte tombait a zero, le
		// critere serait vert sans rien avoir teste.
		Cas(paires > 0 && divergentes == 0, q);
	}

	printf("\n=== Resultat : %d OK / %d FAIL ===\n", gPass, gFail);
	return gFail == 0 ? 0 : 1;
}
