// -----------------------------------------------------------------------------
// @File    Applications/NKSculptHarness/src/main.cpp
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   Banc de la sculpture VOLUMIQUE : descripteur, registre, et un trait
//          de brosse applique a un NkEditMesh.
//
// POURQUOI UN BANC A PART, ET NON DES CAS DANS NKEditMeshHarness
//   Ce harnais-la porte une BASELINE de reference partagee. Y ajouter des cas
//   deplace ses lignes et oblige a regenerer un fichier temoin qui protege le
//   travail d'autres chantiers -- et le depot sait deja qu'« un banc deja rouge
//   ne protege plus ». On mesure a cote, sans toucher a sa garde.
//
// ⚠️ CE QUE CHAQUE CRITERE FERAIT ROUGIR, ET CE QUE RENDRAIT UNE VERSION FAITE
//    AU HASARD. Ecrit avant le code, parce qu'un critere qu'un defaut ne peut
//    pas faire echouer ne mesure rien :
//
//   [zero]       force nulle / trait hors du maillage => PAS UN BIT ne change.
//                Au hasard : impossible a satisfaire par accident. C'est le
//                seul critere qu'un bug ne peut pas passer par chance.
//   [zone]       un sommet HORS rayon bouge => ROUGE. Mesure ici avec une COPIE
//                independante des positions d'avant, et NON avec un compteur du
//                module : celui-ci vaudrait zero par construction.
//                Au hasard : non.
//   [direction]  sens=+1 doit AUGMENTER le volume signe ; sens=-1 le diminuer.
//                Au hasard : passerait une fois sur deux -- c'est pourquoi on
//                exige les DEUX sens, jamais « ca a bouge ».
//   [topologie]  le cube ne doit pas se DECHIRER : autant de bords apres
//                qu'avant. Au hasard : non. C'est le cas defavorable, invisible
//                sur une sphere dense.
//   [annulation] apres restauration, egalite AU BIT avec l'etat d'avant.
//   [registre]   une brosse deposee en DONNEES SEULES, sans recompiler,
//                apparait ET agit. C'est LE critere de la demande de Rodolf.
//
// ⚠️ AUCUN ATTENDU EN DUR. Pas un seul nombre grave : tous les criteres sont des
//    INVARIANTS (egalite au bit, signe d'une variation, conservation d'un
//    compte). Un chiffre grave se perime, et il se perime EN VERT.
// -----------------------------------------------------------------------------

#include "NKContainers/Associative/NkHashMap.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKRenderer/Mesh/NkEditMesh.h"
#include "NKRenderer/Mesh/NkOBJLoader.h"
#include "NKRenderer/Tools/MeshSculpt/NkBrushRegistry.h"
#include "NKRenderer/Tools/MeshSculpt/NkMeshSculpt.h"

#include <cmath>
#include <cstdio>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::renderer;
using namespace nkentseu::math;

static int32 gFail = 0;
static int32 gPass = 0;

static void Check(bool ok, const char *label, const char *detail) {
	if (ok) {
		++gPass;
		printf("  [ok   ] %-46s %s\n", label, detail ? detail : "");
	} else {
		++gFail;
		printf("  [ROUGE] %-46s %s\n", label, detail ? detail : "");
	}
}

// ── EGALITE AU BIT ───────────────────────────────────────────────────────────
// ⚠️ PAS DE memcmp SUR LA STRUCTURE. `NkEditMesh::Vert` a du bourrage, et le
//    depot a paye « memcmp sur structure a bourrage peut rougir sur du juste ET
//    verdir sur du casse ». On compare les CHAMPS qui nous interessent, et on
//    les compare par leurs OCTETS (un float compare avec == dirait que deux NaN
//    different, et que +0 et -0 sont egaux : ni l'un ni l'autre n'est ce qu'on
//    veut pour « rien n'a bouge »).
static bool SameBits(float32 a, float32 b) {
	uint32 ua, ub;
	memcpy(&ua, &a, 4);
	memcpy(&ub, &b, 4);
	return ua == ub;
}

static bool SameBits3(const NkVec3f &a, const NkVec3f &b) {
	return SameBits(a.x, b.x) && SameBits(a.y, b.y) && SameBits(a.z, b.z);
}

struct Snapshot {
		NkVector<NkVec3f> pos;
		NkVector<NkVec3f> nrm;
};

static void Capture(const NkEditMesh &m, Snapshot &s) {
	s.pos.Clear();
	s.nrm.Clear();
	for (uint32 i = 0; i < m.VertCount(); ++i) {
		s.pos.PushBack(m.verts[i].pos);
		s.nrm.PushBack(m.verts[i].normal);
	}
}

static void Restore(NkEditMesh &m, const Snapshot &s) {
	for (uint32 i = 0; i < m.VertCount() && i < s.pos.Size(); ++i) {
		m.verts[i].pos = s.pos[i];
		m.verts[i].normal = s.nrm[i];
	}
}

// Nombre de sommets dont la position differe d'au moins un bit.
static uint32 CountMoved(const NkEditMesh &m, const Snapshot &s) {
	uint32 n = 0;
	for (uint32 i = 0; i < m.VertCount() && i < s.pos.Size(); ++i)
		if (!SameBits3(m.verts[i].pos, s.pos[i]))
			++n;
	return n;
}

// Sommets hors rayon qui ont bouge -- LE NEGATIF OBLIGATOIRE, mesure ici avec
// une copie independante, et non par un compteur du module mesure.
static uint32 CountMovedOutside(const NkEditMesh &m, const Snapshot &s, const NkVec3f &center,
								float32 radius) {
	uint32 n = 0;
	for (uint32 i = 0; i < m.VertCount() && i < s.pos.Size(); ++i) {
		const NkVec3f d = s.pos[i] - center;
		const float32 dist = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
		if (dist <= radius)
			continue; // dans la zone : il a le droit de bouger
		if (!SameBits3(m.verts[i].pos, s.pos[i]))
			++n;
	}
	return n;
}

// ── DIRECTION LOCALE : LE SEUL CRITERE VALABLE SUR UN SUJET REEL ──────
// ⚠️ LE VOLUME SIGNE A UNE CONDITION, ET UN SUJET REEL NE LA REMPLIT PAS.
//    Il ne dit « la matiere est sortie » que si la surface est FERMEE et
//    orientee vers l'exterieur. Mesure du 19/09 sur nos propres sorties :
//      cylindre_rho2.obj  → 22 aretes de BORD : surface OUVERTE ;
//      tore_rho1.obj      → V0 = -63,9 : winding INVERSE dans le fichier.
//    Sur ces deux-la, le critere de volume rougissait sur une brosse JUSTE --
//    il mesurait la topologie du sujet, pas le geste. C'est la faute
//    « l'attendu s'ecrit avec la condition qu'il suppose ».
//
//    Celui-ci n'a aucune condition : chaque sommet deplace doit l'etre dans
//    le sens de SA PROPRE normale (sens=+1) ou a son oppose (sens=-1). Vrai
//    sur une surface ouverte, sur un maillage inverse, sur du non-manifold.
//    Et il est mesure depuis les POSITIONS d'avant et d'apres, pas en
//    relisant l'intention du module -- sinon il serait tautologique.
static uint32 CountAgainstNormal(const NkEditMesh &m, const Snapshot &s, float32 sign) {
	uint32 wrong = 0;
	for (uint32 i = 0; i < m.VertCount() && i < s.pos.Size(); ++i) {
		const NkVec3f d = m.verts[i].pos - s.pos[i];
		if (d.x == 0.f && d.y == 0.f && d.z == 0.f)
			continue; // pas deplace : rien a dire
		const NkVec3f &n = s.nrm[i]; // la normale D'AVANT
		const float32 dot = d.x * n.x + d.y * n.y + d.z * n.z;
		if (dot * sign <= 0.f)
			++wrong;
	}
	return wrong;
}

// Compte les aretes de BORD, apres soudure des sommets coincidents. Un cube
// ferme en a zero ; s'il se dechire, ce compte explose.
static uint32 CountBoundary(const NkEditMesh &m) {
	NkVector<uint32> canon;
	m.BuildVertexMerge(canon);
	NkHashMap<uint64, uint32> edges;
	NkVector<NkEmId> loop;
	for (uint32 f = 0; f < m.FaceCount(); ++f) {
		if (!m.faces[f].alive)
			continue;
		loop.Clear();
		m.GetFaceVerts((NkEmId)f, loop);
		if (loop.Size() < 3)
			continue;
		for (uint32 k = 0; k < (uint32)loop.Size(); ++k) {
			const uint32 a = loop[k], b = loop[(k + 1) % (uint32)loop.Size()];
			const uint32 ca = (a < canon.Size()) ? canon[a] : a;
			const uint32 cb = (b < canon.Size()) ? canon[b] : b;
			const uint64 lo = ca < cb ? ca : cb, hi = ca < cb ? cb : ca;
			const uint64 key = (lo << 32) | hi;
			uint32 *e = edges.Find(key);
			if (e)
				(*e)++;
			else
				edges.InsertOrAssign(key, 1u);
		}
	}
	uint32 n = 0;
	for (auto it = edges.Begin(); it != edges.End(); ++it)
		if (it->Second == 1)
			++n;
	return n;
}

// Aretes portant PLUS DE DEUX faces : la signature du non-manifold. Une brosse
// ne doit jamais en CREER -- et si le sujet en porte deja, elle ne doit pas en
// ajouter.
static uint32 CountNonManifold(const NkEditMesh &m) {
	NkVector<uint32> canon;
	m.BuildVertexMerge(canon);
	NkHashMap<uint64, uint32> edges;
	NkVector<NkEmId> loop;
	for (uint32 f = 0; f < m.FaceCount(); ++f) {
		if (!m.faces[f].alive)
			continue;
		loop.Clear();
		m.GetFaceVerts((NkEmId)f, loop);
		if (loop.Size() < 3)
			continue;
		for (uint32 k = 0; k < (uint32)loop.Size(); ++k) {
			const uint32 a = loop[k], b = loop[(k + 1) % (uint32)loop.Size()];
			const uint32 ca = (a < canon.Size()) ? canon[a] : a;
			const uint32 cb = (b < canon.Size()) ? canon[b] : b;
			const uint64 lo = ca < cb ? ca : cb, hi = ca < cb ? cb : ca;
			const uint64 key = (lo << 32) | hi;
			uint32 *e = edges.Find(key);
			if (e)
				(*e)++;
			else
				edges.InsertOrAssign(key, 1u);
		}
	}
	uint32 n = 0;
	for (auto it = edges.Begin(); it != edges.End(); ++it)
		if (it->Second > 2)
			++n;
	return n;
}

// Diagonale de la boite englobante.
// ⚠️ INDISPENSABLE POUR LES SUJETS REELS. Le rayon d'une brosse est en UNITES
//    MONDE, et un modele sorti de notre chaine peut mesurer 0,01 comme 100.
//    Un rayon fixe de 0,25 ne toucherait RIEN sur l'un et TOUT sur l'autre :
//    le banc deviendrait vide ou absurde selon le fichier, sans rien dire.
//    On exprime donc le rayon en fraction de la taille du sujet.
static float32 BBoxDiag(const NkEditMesh &m) {
	if (m.VertCount() == 0)
		return 0.f;
	NkVec3f lo = m.verts[0].pos, hi = m.verts[0].pos;
	for (uint32 i = 1; i < m.VertCount(); ++i) {
		const NkVec3f &p = m.verts[i].pos;
		if (p.x < lo.x) lo.x = p.x;
		if (p.y < lo.y) lo.y = p.y;
		if (p.z < lo.z) lo.z = p.z;
		if (p.x > hi.x) hi.x = p.x;
		if (p.y > hi.y) hi.y = p.y;
		if (p.z > hi.z) hi.z = p.z;
	}
	const NkVec3f d = hi - lo;
	return sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
}

// ── PRIMITIVES ───────────────────────────────────────────────────────────────
static void PushVert(NkVector<NkVertex3D> &v, float32 x, float32 y, float32 z, float32 nx, float32 ny,
					 float32 nz) {
	NkVertex3D a{};
	a.pos = NkVec3f{x, y, z};
	a.normal = NkVec3f{nx, ny, nz};
	v.PushBack(a);
}

// Cube de cote 1 centre a l'origine : 24 sommets pour 8 coins.
// ⚠️ C'EST LE POSITIF DEFAVORABLE. Ses coins sont tripliques : une sculpture
//    qui ne soude pas logiquement le DECHIRE, et une sphere dense ne le dirait
//    jamais.
static void MakeCube(NkVector<NkVertex3D> &v, NkVector<uint32> &idx) {
	v.Clear();
	idx.Clear();
	const float32 h = 0.5f;
	const float32 n[6][3] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
	const float32 c[6][4][3] = {
		{{-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}},
		{{h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h}},
		{{h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h}},
		{{-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h}},
		{{-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h}},
		{{-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h}}};
	for (uint32 f = 0; f < 6; ++f) {
		const uint32 b = (uint32)v.Size();
		for (uint32 k = 0; k < 4; ++k)
			PushVert(v, c[f][k][0], c[f][k][1], c[f][k][2], n[f][0], n[f][1], n[f][2]);
		idx.PushBack(b);
		idx.PushBack(b + 1);
		idx.PushBack(b + 2);
		idx.PushBack(b);
		idx.PushBack(b + 2);
		idx.PushBack(b + 3);
	}
}

// Sphere UV de rayon 0,5 -- le cas favorable (sommets presque tous uniques).
static void MakeSphere(uint32 stacks, uint32 slices, NkVector<NkVertex3D> &v, NkVector<uint32> &idx) {
	v.Clear();
	idx.Clear();
	const float32 PI = 3.14159265358979f;
	for (uint32 i = 0; i <= stacks; ++i) {
		const float32 phi = PI * (float32)i / (float32)stacks;
		for (uint32 j = 0; j <= slices; ++j) {
			const float32 th = 2.f * PI * (float32)j / (float32)slices;
			const float32 x = sinf(phi) * cosf(th), y = cosf(phi), z = sinf(phi) * sinf(th);
			PushVert(v, x * 0.5f, y * 0.5f, z * 0.5f, x, y, z);
		}
	}
	for (uint32 i = 0; i < stacks; ++i)
		for (uint32 j = 0; j < slices; ++j) {
			const uint32 a = i * (slices + 1) + j, b = a + slices + 1;
			// ⚠️ SENS DE PARCOURS. Mesure du 19/09 : avec l'ordre (a, b, a+1) ce
			//    maillage rendait un volume signe de -0,505 alors que le cube
			//    rendait +1,000 exactement. Les deux primitives du banc ne
			//    tournaient donc PAS dans le meme sens, et le critere de
			//    direction rougissait sur un module juste. C'etait l'INSTRUMENT
			//    qui etait faux, et seul le controle d'orientation ajoute au
			//    debut de RunBrushOn permettait de le dire -- sans lui, on
			//    aurait cherche le defaut dans la sculpture.
			idx.PushBack(a);
			idx.PushBack(a + 1);
			idx.PushBack(b);
			idx.PushBack(a + 1);
			idx.PushBack(b + 1);
			idx.PushBack(b);
		}
}

// ── CHARGEMENT DU REGISTRE DEPUIS DE VRAIS FICHIERS ──────────────────────────
// ⚠️ ON LIT LE DISQUE, ON N'EMBARQUE PAS DE CHAINE. Un banc qui parserait un
//    litteral prouverait le PARSEUR et rien d'autre : la demande de Rodolf est
//    qu'une brosse s'ajoute SANS RECOMPILER, et ca ne se prouve qu'en lisant un
//    fichier que le binaire n'a jamais vu.
static uint32 LoadBrushes(NkBrushRegistry &reg, const char *dir) {
	if (!NkDirectory::Exists(dir)) {
		printf("  !! dossier de brosses introuvable : %s\n", dir);
		return 0;
	}
	NkVector<NkString> files = NkDirectory::GetFiles(dir, "*.nkbrush");
	uint32 ok = 0;
	for (uint32 i = 0; i < files.Size(); ++i) {
		// ReadAllBytes et non ReadAllText : on veut les octets du fichier, sans
		// normalisation invisible des fins de ligne. (« WriteAllText ecrit en
		// CRLF, et ReadAllText le masque des deux cotes. »)
		NkVector<nk_uint8> bytes = NkFile::ReadAllBytes(files[i].CStr());
		if (bytes.Size() == 0) {
			printf("  !! vide ou illisible : %s\n", files[i].CStr());
			continue;
		}
		char err[128] = {};
		const NkBrushParse r = reg.AddFromText((const char *)bytes.Data(), (uint32)bytes.Size(), err, 128);
		if (r != NkBrushParse::NK_BRUSH_OK) {
			// ⚠️ UN REFUS EST NOMME. Jamais de brosse qui disparait en silence.
			printf("  !! REFUS %s : %s [%s]\n", files[i].CStr(), NkBrushParseText(r), err);
			continue;
		}
		++ok;
	}
	return ok;
}

// ── UN TRAIT ─────────────────────────────────────────────────────────────────
// ⚠️ LE TRAIT EST POSE PAR UN CROCHET, JAMAIS PAR LA SOURIS. Aucune injection
//    d'entree sur la machine de Rodolf -- ni souris, ni clavier. Effet de bord
//    qui vaut la contrainte : un trait pose ainsi est REJOUABLE, donc mesurable
//    deux fois de suite a l'identique.
static void RunBrushOn(const char *primName, NkEditMesh &m, const NkBrushDesc &b,
					   float32 radiusOverride = 0.f) {
	char label[128];
	Snapshot before;
	Capture(m, before);
	const uint32 bnd0 = CountBoundary(m);
	const uint32 nm0 = CountNonManifold(m);
	// La CONDITION du critere de volume, mesuree sur le sujet lui-meme.
	const bool closedAndOutward = (bnd0 == 0) && (NkSculptSignedVolume(m) > 0.f);

	// ── L'INSTRUMENT AVANT LA MESURE ────────────────────────────────────────
	// ⚠️ LE CRITERE DE DIRECTION SUPPOSE UNE ORIENTATION. « Le volume signe
	//    augmente » ne veut dire « la matiere sort » que si les faces tournent
	//    dans le sens des normales sortantes. Sur une primitive dont le sens de
	//    parcours est inverse, le volume signe est NEGATIF, et gonfler la
	//    surface le rend PLUS negatif -- le critere rougirait sur un module
	//    parfaitement juste.
	//    On mesure donc d'abord l'instrument. Si cette ligne rougit, la faute
	//    est dans la PRIMITIVE du banc, pas dans la sculpture : un instrument
	//    faux se repare, un critere faux se remplace, et confondre les deux
	//    fait corriger le mauvais fichier.
	{
		const float32 v0 = NkSculptSignedVolume(m);
		(void)v0;
		char det[96];
		snprintf(det, sizeof(det), "V0=%+.6f bords=%u -> volume %s", (double)v0, bnd0,
			     closedAndOutward ? "APPLICABLE" : "non applicable");
		snprintf(label, sizeof(label), "%s [sujet] fermeture et orientation", primName);
		// ⚠️ CE N'EST PLUS UN CRITERE, C'EST UNE MESURE DU SUJET. Exiger V0>0
		//    etait juste pour MES primitives, dont je choisis le winding ; c'est
		//    faux pour un fichier dont je ne controle pas la provenance. Un tore
		//    sorti de notre chaine arrive inverse : ce n'est pas un echec de la
		//    brosse, c'est un fait sur le fichier -- et il decide quels criteres
		//    s'appliquent ensuite.
		printf("  [info ] %-46s %s\n", label, det);
	}

	// Le sommet du maillage le plus haut : on vise une zone qui existe.
	NkVec3f center{0.f, 0.f, 0.f};
	float32 best = -1e30f;
	for (uint32 i = 0; i < m.VertCount(); ++i)
		if (m.verts[i].pos.y > best) {
			best = m.verts[i].pos.y;
			center = m.verts[i].pos;
		}

	// Le rayon du descripteur est en unites monde ; sur un sujet reel on le
	// remplace par une fraction de la taille du modele (cf. BBoxDiag).
	const float32 radius = (radiusOverride > 0.f) ? radiusOverride : b.radius;
	NkSculptPoint pt;
	pt.pos = center;
	pt.normal = NkVec3f{0.f, 1.f, 0.f};
	pt.radius = radius;
	pt.pressure = 1.f;

	// ── [zero] FORCE NULLE ──────────────────────────────────────────────────
	{
		NkBrushDesc z = b;
		z.strength = 0.f;
		const NkSculptApply r = NkSculptApplyStroke(m, z, &pt, 1);
		const uint32 moved = CountMoved(m, before);
		snprintf(label, sizeof(label), "%s/%s zero force-nulle", primName, b.name);
		Check(!r.applied && moved == 0, label, "aucun bit ne doit changer");
	}

	// ── [zero] TRAIT HORS DU MAILLAGE ───────────────────────────────────────
	{
		// ⚠️ PAS `far` : c'est une MACRO de windows.h, heritee du modele memoire
		//    16 bits. Elle survit dans les en-tetes du systeme et transforme un
		//    nom de variable parfaitement legal en erreur de syntaxe.
		NkSculptPoint loin = pt;
		loin.pos = NkVec3f{1000.f, 1000.f, 1000.f};
		const NkSculptApply r = NkSculptApplyStroke(m, b, &loin, 1);
		const uint32 moved = CountMoved(m, before);
		snprintf(label, sizeof(label), "%s/%s zero hors-maillage", primName, b.name);
		Check(!r.applied && moved == 0, label, "aucun bit ne doit changer");
	}

	// -- [direction] LES DEUX SENS ----------------------------------------
	// Exiger les DEUX est ce qui distingue une vraie brosse d'un tirage a pile
	// ou face : un defaut qui pousse toujours dans le meme sens en echoue un.
	//
	// [!] MAIS CES CRITERES SUPPOSENT LA PRIMITIVE, ET LE BANC L'IGNORAIT.
	//     « suit la normale » et « le volume gonfle » decrivent
	//     NK_SCULPT_OP_NORMAL, pas une brosse quelconque. Le jour ou `lisser`
	//     est entree dans le registre, ils ont rougi 24 fois -- sur une
	//     primitive qui fonctionne : un lissage ne suit AUCUNE direction
	//     imposee (il vise la moyenne des voisins) et il RETRECIT au lieu de
	//     gonfler (dV = -0,111 sur le cube, ce qui est son comportement juste).
	//
	//     C'est la meme faute que le cylindre et le tore ont deja values a ce
	//     banc : *un critere dont la condition n'est pas verifiee rougit sur du
	//     code sain*. On ne les affaiblit pas et on ne les supprime pas -- on
	//     ecrit la condition sous laquelle ils veulent dire quelque chose.
	const bool dirImposee = (b.op == NkSculptOp::NK_SCULPT_OP_NORMAL);


	float32 volPlus = 0.f, volMinus = 0.f;
	{
		NkBrushDesc up = b;
		up.dir = 1.f;
		const NkSculptApply r = NkSculptApplyStroke(m, up, &pt, 1);
		volPlus = r.volumeAfter - r.volumeBefore;
		char det[96];
		// DIRECTION LOCALE -- sans condition, donc toujours exigee.
		const uint32 wrongUp = CountAgainstNormal(m, before, +1.f);
		snprintf(det, sizeof(det), "a contre-sens=%u / deplaces=%u", wrongUp, r.vertsMoved);
		snprintf(label, sizeof(label), "%s/%s sens=+1 suit la normale", primName, b.name);
		if (dirImposee)
			Check(r.applied && wrongUp == 0, label, det);
		else
			printf("  [ n/a ] %-46s %s (primitive sans direction imposee)\n", label, det);

		// VOLUME SIGNE -- CONDITIONNEL. Il ne veut dire "la matiere est sortie"
		// que sur une surface fermee et orientee sortante. On l'annonce comme
		// non applicable plutot que de le faire passer pour vert.
		snprintf(det, sizeof(det), "dV=%+.6f  sommets=%u", (double)volPlus, r.vertsMoved);
		snprintf(label, sizeof(label), "%s/%s volume sens=+1 gonfle", primName, b.name);
		// [!] DEUX conditions, pas une : la surface doit s'y preter ET la
		//     primitive doit deplacer le long d'une direction imposee. Un
		//     lissage RETRECIT (dV<0) sans etre faux pour autant.
		if (closedAndOutward && dirImposee)
			Check(r.applied && volPlus > 0.f, label, det);
		else if (!dirImposee)
			printf("  [ n/a ] %-46s %s (primitive sans direction imposee)\n", label, det);
		else
			printf("  [ n/a ] %-46s %s (surface ouverte ou inversee)\n", label, det);

		// ── [zone] LE NEGATIF OBLIGATOIRE ───────────────────────────────────
		const uint32 outside = CountMovedOutside(m, before, center, radius);
		snprintf(det, sizeof(det), "hors rayon deplaces=%u", outside);
		snprintf(label, sizeof(label), "%s/%s zone hors-rayon intact", primName, b.name);
		Check(outside == 0, label, det);

		// ── [topologie] LE CUBE NE DOIT PAS SE DECHIRER ─────────────────────
		const uint32 bnd1 = CountBoundary(m);
		snprintf(det, sizeof(det), "bords avant=%u apres=%u", bnd0, bnd1);
		snprintf(label, sizeof(label), "%s/%s topologie sans dechirure", primName, b.name);
		Check(bnd1 == bnd0, label, det);

		// ⚠️ NE JAMAIS AGGRAVER LE NON-MANIFOLD. Un sujet sorti de notre chaine
		//    peut en porter deja ; la question n'est donc pas « y en a-t-il ? »
		//    mais « la brosse en AJOUTE-t-elle ? ». Exiger zero refuserait des
		//    maillages reels que l'outil doit justement servir a corriger ; ne
		//    rien exiger laisserait la brosse fabriquer de la geometrie invalide
		//    en silence. Le critere est donc une NON-AGGRAVATION.
		const uint32 nm1 = CountNonManifold(m);
		snprintf(det, sizeof(det), "non-manifold avant=%u apres=%u", nm0, nm1);
		snprintf(label, sizeof(label), "%s/%s non-manifold non aggrave", primName, b.name);
		Check(nm1 <= nm0, label, det);

		// ── [annulation] RETOUR AU BIT ──────────────────────────────────────
		Restore(m, before);
		const uint32 moved = CountMoved(m, before);
		snprintf(label, sizeof(label), "%s/%s annulation au bit", primName, b.name);
		Check(moved == 0, label, "restauration exacte");
	}
	{
		NkBrushDesc dn = b;
		dn.dir = -1.f;
		const NkSculptApply r = NkSculptApplyStroke(m, dn, &pt, 1);
		volMinus = r.volumeAfter - r.volumeBefore;
		char det[96];
		const uint32 wrongDn = CountAgainstNormal(m, before, -1.f);
		snprintf(det, sizeof(det), "a contre-sens=%u / deplaces=%u", wrongDn, r.vertsMoved);
		snprintf(label, sizeof(label), "%s/%s sens=-1 suit la normale", primName, b.name);
		if (dirImposee)
			Check(r.applied && wrongDn == 0, label, det);
		else
			printf("  [ n/a ] %-46s %s (primitive sans direction imposee)\n", label, det);

		snprintf(det, sizeof(det), "dV=%+.6f  sommets=%u", (double)volMinus, r.vertsMoved);
		snprintf(label, sizeof(label), "%s/%s volume sens=-1 creuse", primName, b.name);
		if (closedAndOutward && dirImposee)
			Check(r.applied && volMinus < 0.f, label, det);
		else if (!dirImposee)
			printf("  [ n/a ] %-46s %s (primitive sans direction imposee)\n", label, det);
		else
			printf("  [ n/a ] %-46s %s (surface ouverte ou inversee)\n", label, det);
		Restore(m, before);
	}

	// -- [direction] LES DEUX SENS NE SE CONFONDENT PAS ---------------------
	// [!] ENONCE EN VOLUME, donc reserve aux primitives qui en deplacent.
	//     L'exigence elle-meme vaut pour TOUTE brosse -- deux sens qui
	//     donnent le meme resultat, c'est un sens qui n'est pas lu -- mais
	//     la QUANTITE qui la mesure change avec la primitive. Pour `lisser`
	//     elle est portee, plus bas, par « sens=-1 fait MONTER la rugosite ».
	//     Garder ce test-ci sur une primitive sans direction imposee, ce
	//     serait exiger d'elle la signature d'une autre.
	{
		snprintf(label, sizeof(label), "%s/%s les deux sens different", primName, b.name);
		if (closedAndOutward && dirImposee)
			Check(volPlus > 0.f && volMinus < 0.f, label, "sinon : un seul sens agit");
		else if (!dirImposee)
			printf("  [ n/a ] %-46s (primitive sans direction imposee)\n", label);
		else
			printf("  [ n/a ] %-46s (surface ouverte ou inversee)\n", label);
	}

	// ── [donnee] LA BROSSE TELLE QU'ELLE EST DECLAREE ───────────────────────
	// ⚠️ LE CRITERE QUI MANQUAIT, ET SON ABSENCE RENDAIT LE BANC COMPLAISANT.
	//    Les cas ci-dessus FORCENT `dir` a +1 puis -1 : ils eprouvent le module,
	//    jamais la DONNEE. Mesure du 19/09 : « dessiner » et « creuser »
	//    rendaient des chiffres IDENTIQUES au dernier decimal, et les deux
	//    passaient -- le banc serait reste vert si le module avait purement
	//    IGNORE le champ `sens` du fichier.
	//    Ici on applique la brosse SANS RIEN TOUCHER, et on exige que le signe
	//    de la variation suive ce que le FICHIER declare. C'est le seul cas qui
	//    relie la donnee au comportement, donc le seul qui prouve qu'une brosse
	//    ajoutee en donnees fait ce que son fichier dit.
	{
		const NkSculptApply r = NkSculptApplyStroke(m, b, &pt, 1);
		char det[128];
		// ⚠️ LE MEME CRITERE, MAIS AVEC L'INSTRUMENT QUE LE SUJET PERMET.
		//    Sur une surface fermee et orientee, le volume signe est le juge le
		//    plus independant (il ne sait rien de la facon dont on deplace). Sur
		//    une surface OUVERTE ou INVERSEE il ne juge plus rien -- mesure du
		//    19/09 : cylindre_rho2.obj (22 bords) faisait rougir une brosse juste.
		//    On retombe alors sur la direction LOCALE, qui n'a pas de condition.
		//    Ce qui est mesure ne change pas : LA DONNEE PILOTE-T-ELLE LE GESTE ?
		bool coherent;
		if (closedAndOutward) {
			const float32 dv = r.volumeAfter - r.volumeBefore;
			coherent = (b.dir > 0.f) ? (dv > 0.f) : (dv < 0.f);
			snprintf(det, sizeof(det), "fichier sens=%+.0f -> dV=%+.6f", (double)b.dir, (double)dv);
		} else {
			const uint32 wrong = CountAgainstNormal(m, before, b.dir);
			coherent = (wrong == 0);
			snprintf(det, sizeof(det), "fichier sens=%+.0f -> a contre-sens=%u/%u", (double)b.dir,
				 wrong, r.vertsMoved);
		}
		snprintf(label, sizeof(label), "%s/%s DONNEE pilote le sens", primName, b.name);
		// [!] MEME RESERVE : « coherent » se lit en volume ou en direction, les
		//     deux signatures de la primitive a direction imposee. Pour lisser,
		//     la meme exigence est tenue par « sens=-1 fait MONTER la rugosite »,
		//     qui est son equivalent exact dans la quantite qui la concerne.
		if (dirImposee)
			Check(r.applied && coherent, label, det);
		else
			printf("  [ n/a ] %-46s %s (voir le bloc lisser)\n", label, det);
		Restore(m, before);
	}
}

// ──────────────────────────────────────────────────────────────────────────
// LE TRAIT SURVIT-IL A UN ALLER-RETOUR SUR DISQUE, ET SE REJOUE-T-IL ?
//
// ⚠️ C'EST LE CRITERE QUI DECIDE DE LA SPIRALE. Rodolf (19/09) veut que les
//    corrections a la main SURVIVENT a une regeneration : on regenere la base
//    depuis le document, puis on REJOUE la pile d'operations. Un coup de
//    brosse qui ne se rejoue pas est perdu au premier tour.
//
//    Le test part du maillage d'origine DEUX FOIS :
//      A = maillage + commande appliquee directement ;
//      B = maillage + commande SERIALISEE, RELUE dans un recorder neuf, puis
//          rejouee par ReplayOnto.
//    On exige A == B AU BIT. Une egalite approchee laisserait passer une
//    perte de precision a l'ecriture, qui est exactement ce qu'un aller-retour
//    binaire doit interdire.
//
// ⚠️ ET ON COMPARE CONTRE L'ORIGINAL AUSSI : si la commande ne faisait RIEN,
//    A et B seraient egaux tous les deux a l'original, et le test passerait
//    en ne prouvant rien. Un critere que l'inaction satisfait ne mesure pas.
static void RunReplay(const char *primName, const NkEditMesh &src, const NkBrushDesc &brush) {
	char label[128], det[128];

	// La commande : le geste, AVEC toutes ses valeurs (rien n'est relu d'un
	// fichier de brosse au rejeu).
	NkMeshEditCommand cmd;
	cmd.op = NkMeshEditOp::Sculpt;
	cmd.sculpt.radius = brush.radius;
	cmd.sculpt.strength = brush.strength;
	cmd.sculpt.hardness = brush.hardness;
	cmd.sculpt.dir = brush.dir;
	cmd.sculpt.falloff = (uint8)brush.falloff;
	cmd.sculpt.primitive = (uint8)brush.op;
	for (uint32 i = 0; i < 47 && brush.name[i]; ++i)
		cmd.sculpt.brushName[i] = brush.name[i];

	// Le trait : trois points sur la surface, en REPERE OBJET.
	uint32 hi = 0;
	for (uint32 i = 1; i < src.VertCount(); ++i)
		if (src.verts[i].pos.y > src.verts[hi].pos.y)
			hi = i;
	for (uint32 k = 0; k < 3; ++k) {
		const uint32 vi = (hi + k * 7u) % src.VertCount();
		cmd.sculptPoints.PushBack(src.verts[vi].pos);
		cmd.sculptNormals.PushBack(src.verts[vi].normal);
	}

	// A : application directe.
	NkEditMesh a = src;
	const bool okA = cmd.Apply(a);
	snprintf(label, sizeof(label), "%s/%s rejeu: la commande agit", primName, brush.name);
	snprintf(det, sizeof(det), "points=%u", (uint32)cmd.sculptPoints.Size());
	Check(okA, label, det);
	if (!okA)
		return;

	// ⚠️ CONTROLE QUE L'INACTION NE PASSERAIT PAS : A doit differer de
	//    l'original, sinon l'egalite A==B qui suit serait vraie pour rien.
	Snapshot s0;
	Capture(src, s0);
	const uint32 movedA = CountMoved(a, s0);
	snprintf(label, sizeof(label), "%s/%s rejeu: A differe de l'original", primName, brush.name);
	snprintf(det, sizeof(det), "sommets deplaces=%u", movedA);
	Check(movedA > 0, label, det);

	// B : par le journal, apres un aller-retour BINAIRE.
	NkMeshEditRecorder rec;
	rec.Push(cmd);
	NkVector<uint8> blob;
	rec.Serialize(blob);
	NkMeshEditRecorder relu; // recorder NEUF : rien ne survit de l'original
	const bool okDe = relu.Deserialize(blob.Data(), (uint32)blob.Size());
	snprintf(label, sizeof(label), "%s/%s rejeu: relecture du journal", primName, brush.name);
	snprintf(det, sizeof(det), "octets=%u commandes=%u", (uint32)blob.Size(), relu.Count());
	Check(okDe && relu.Count() == 1, label, det);
	if (!okDe || relu.Count() != 1)
		return;

	NkEditMesh bmesh = src;
	const uint32 applied = relu.ReplayOnto(bmesh);
	snprintf(label, sizeof(label), "%s/%s rejeu: ReplayOnto applique", primName, brush.name);
	snprintf(det, sizeof(det), "commandes appliquees=%u", applied);
	Check(applied == 1, label, det);

	// A == B AU BIT.
	uint32 diff = 0;
	if (a.VertCount() != bmesh.VertCount()) {
		diff = 0xFFFFFFFFu;
	} else {
		for (uint32 i = 0; i < a.VertCount(); ++i)
			if (!SameBits3(a.verts[i].pos, bmesh.verts[i].pos))
				++diff;
	}
	snprintf(label, sizeof(label), "%s/%s rejeu: A == B AU BIT", primName, brush.name);
	snprintf(det, sizeof(det), "sommets differents=%u / %u", diff, a.VertCount());
	Check(diff == 0, label, det);

	// Le nom de la brosse a traverse le disque (journal et affichage).
	const NkMeshEditCommand &cr = relu.At(0);
	bool nameOk = true;
	for (uint32 i = 0; i < 48; ++i)
		if (cr.sculpt.brushName[i] != cmd.sculpt.brushName[i]) {
			nameOk = false;
			break;
		}
	snprintf(label, sizeof(label), "%s/%s rejeu: nom conserve", primName, brush.name);
	Check(nameOk && cr.sculpt.dir == cmd.sculpt.dir, label, cr.sculpt.brushName);
}

// == LA RUGOSITE : CE QUE `lisser` EST CENSE FAIRE DESCENDRE ==============
//
// Somme des || p_i - moyenne(voisins de i) ||^2, sur l'identite SOUDEE.
// C'est exactement la quantite que le Laplacien uniforme minimise : si la
// brosse lisse, ce nombre DOIT baisser, et s'il monte c'est qu'elle fait
// autre chose. *Un critere qui se contente de « la commande a rendu ok » ne
// distingue pas un lissage d'un deplacement au hasard.*
//
// [!] ELLE EST RECALCULEE ICI, ET C'EST VOULU. Si elle empruntait
//     l'adjacence construite par NkMeshSculpt, une erreur dans cette
//     adjacence rendrait le critere vrai ET le module faux -- le controle
//     et le controle tire de la meme source. On la rebatit depuis les faces.
// [!] ELLE EST LOCALE, ET CE N EST PAS UN RAFFINEMENT : mesuree sur TOUT le
//     maillage, la baisse tombait a 0,01 % -- non parce que le lissage est
//     faible, mais parce que 13 groupes touches sur 134 se noient dans 121
//     groupes intacts. *On mesure la ou on agit, sinon on mesure surtout ce
//     qui n a pas bouge.* rayon < 0 = tout le maillage.
// Boite englobante des sommets VIVANTS. Une longueur, la ou les compteurs
// de sommets ne donnent qu'un cardinal.
static void BBox(const NkEditMesh &mesh, NkVec3f &lo, NkVec3f &hi) {
	lo = NkVec3f{1e30f, 1e30f, 1e30f};
	hi = NkVec3f{-1e30f, -1e30f, -1e30f};
	for (uint32 i = 0; i < mesh.VertCount(); ++i) {
		const NkVec3f &p = mesh.verts[i].pos;
		if (p.x < lo.x) lo.x = p.x;
		if (p.y < lo.y) lo.y = p.y;
		if (p.z < lo.z) lo.z = p.z;
		if (p.x > hi.x) hi.x = p.x;
		if (p.y > hi.y) hi.y = p.y;
		if (p.z > hi.z) hi.z = p.z;
	}
}

static float64 Rugosite(const NkEditMesh &mesh, const NkVec3f &centre, float32 rayon) {
	NkVector<uint32> canon;
	mesh.BuildVertexMerge(canon);
	const uint32 vc = mesh.VertCount();
	if (canon.Size() < vc || vc == 0)
		return -1.0; // instrument incoherent : on refuse de rendre un chiffre
	NkVector<NkVec3f> gPos;
	NkVector<uint32> gCnt;
	gPos.Resize(vc);
	gCnt.Resize(vc);
	for (uint32 i = 0; i < vc; ++i) {
		gPos[i] = NkVec3f{0.f, 0.f, 0.f};
		gCnt[i] = 0;
	}
	for (uint32 i = 0; i < vc; ++i) {
		const uint32 c = canon[i];
		if (c >= vc)
			continue;
		gPos[c] = gPos[c] + mesh.verts[i].pos;
		gCnt[c]++;
	}
	for (uint32 i = 0; i < vc; ++i)
		if (gCnt[i] > 1)
			gPos[i] = gPos[i] * (1.f / (float32)gCnt[i]);
	NkVector<NkVec3f> somme;
	NkVector<uint32> deg;
	somme.Resize(vc);
	deg.Resize(vc);
	for (uint32 i = 0; i < vc; ++i) {
		somme[i] = NkVec3f{0.f, 0.f, 0.f};
		deg[i] = 0;
	}
	NkVector<NkEmId> loop;
	for (uint32 f = 0; f < mesh.FaceCount(); ++f) {
		if (!mesh.faces[f].alive)
			continue;
		loop.Clear();
		mesh.GetFaceVerts((NkEmId)f, loop);
		const uint32 n = (uint32)loop.Size();
		if (n < 3)
			continue;
		for (uint32 k = 0; k < n; ++k) {
			const uint32 a = canon[(uint32)loop[k]];
			const uint32 b2 = canon[(uint32)loop[(k + 1u) % n]];
			if (a >= vc || b2 >= vc || a == b2)
				continue;
			somme[a] = somme[a] + gPos[b2];
			deg[a]++;
			somme[b2] = somme[b2] + gPos[a];
			deg[b2]++;
		}
	}
	float64 acc = 0.0;
	for (uint32 i = 0; i < vc; ++i) {
		if (gCnt[i] == 0 || deg[i] == 0)
			continue;
		if (rayon >= 0.f) {
			const NkVec3f dc = gPos[i] - centre;
			if (dc.x * dc.x + dc.y * dc.y + dc.z * dc.z > rayon * rayon)
				continue;
		}
		const NkVec3f moy = somme[i] * (1.f / (float32)deg[i]);
		const NkVec3f d = gPos[i] - moy;
		acc += (float64)(d.x * d.x + d.y * d.y + d.z * d.z);
	}
	return acc;
}

// Combien de GROUPES soudes coincident encore deux a deux comme avant.
// Si le lissage suit une adjacence NON canonisee, les 3 copies d'un coin de
// cube partent chacune de son cote : ce compte chute, et le cube se dechire
// sans qu'aucune autre mesure ne s'en apercoive.
static uint32 GroupesIntacts(const NkEditMesh &mesh) {
	NkVector<uint32> canon;
	mesh.BuildVertexMerge(canon);
	const uint32 vc = mesh.VertCount();
	if (canon.Size() < vc)
		return 0u;
	uint32 n = 0;
	for (uint32 i = 0; i < vc; ++i)
		if (canon[i] == i)
			++n;
	return n;
}

int main(int argc, char **argv) {
	const char *brushDir = "Applications/NK3DModeler/data/brushes";
	// Les sujets reels vivent HORS du depot (sorties de la chaine 3D).
	const char *subjectDir = "D:/Rihen/Livraisons/Resultats_3D/retopologie";
	for (int i = 1; i < argc; ++i) {
		if (strncmp(argv[i], "--brosses=", 10) == 0)
			brushDir = argv[i] + 10;
		else if (strncmp(argv[i], "--sujets=", 9) == 0)
			subjectDir = argv[i] + 9;
	}

	printf("=== BANC SCULPTURE VOLUMIQUE ===\n");
	printf("dossier de brosses : %s\n\n", brushDir);

	// ── LE REGISTRE ─────────────────────────────────────────────────────────
	NkBrushRegistry reg;
	const uint32 loaded = LoadBrushes(reg, brushDir);
	printf("brosses chargees depuis le DISQUE : %u (registre=%u)\n", loaded, (uint32)reg.Count());
	for (uint16 i = 0; i < reg.Count(); ++i) {
		NkBrushDesc d;
		if (reg.At(i, d))
			printf("   - %-16s libelle=\"%s\" op=%u profil=%u rayon=%.3f force=%.2f sens=%+.0f\n", d.name,
				   d.label, (uint32)d.op, (uint32)d.falloff, (double)d.radius, (double)d.strength,
				   (double)d.dir);
	}
	printf("\n");
	// ⚠️ PAS DE NOMBRE GRAVE : on exige « au moins une », pas « exactement
	//    deux ». Un attendu en dur se perimerait au premier fichier ajoute --
	//    et il se perimerait EN VERT si on l'oubliait.
	Check(reg.Count() >= 1, "registre non vide", "au moins une brosse en donnees");

	// Le registre est idempotent sur le nom : recharger le meme dossier ne doit
	// pas doubler la liste. Sans ca, une brosse incluse deux fois apparaitrait
	// deux fois -- defaut qui ne se voit qu'a l'ecran, donc tard.
	{
		const uint16 n0 = reg.Count();
		LoadBrushes(reg, brushDir);
		char det[64];
		snprintf(det, sizeof(det), "avant=%u apres=%u", (uint32)n0, (uint32)reg.Count());
		Check(reg.Count() == n0, "registre idempotent sur le nom", det);
	}

	// Une donnee qui reclame une primitive absente doit etre REFUSEE, pas
	// repliee sur la primitive par defaut.
	{
		const char *bad = "nkbrush 1\nnom = fantome\noperation = serpent\n";
		char err[128] = {};
		NkBrushDesc d;
		const NkBrushParse r = ParseBrushDesc(bad, (uint32)strlen(bad), d, err, 128);
		Check(r == NkBrushParse::NK_BRUSH_ERR_UNKNOWN_OP && !d.valid,
			  "primitive inconnue refusee avec motif", NkBrushParseText(r));
	}
	// Et un nombre illisible ne doit pas devenir un reglage plausible.
	{
		const char *bad = "nkbrush 1\nnom = x\nforce = 0.5abc\n";
		char err[128] = {};
		NkBrushDesc d;
		const NkBrushParse r = ParseBrushDesc(bad, (uint32)strlen(bad), d, err, 128);
		Check(r == NkBrushParse::NK_BRUSH_ERR_BAD_NUMBER, "nombre illisible refuse",
			  NkBrushParseText(r));
	}
	printf("\n");

	// ── LES DEUX MAILLAGES ──────────────────────────────────────────────────
	NkVector<NkVertex3D> v;
	NkVector<uint32> idx;

	for (uint16 bi = 0; bi < reg.Count(); ++bi) {
		NkBrushDesc b;
		if (!reg.At(bi, b))
			continue;

		{
			MakeCube(v, idx);
			NkEditMesh m;
			m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
			printf("-- cube8 (positif DEFAVORABLE : 8 coins, 24 sommets) / brosse \"%s\"\n", b.name);
			RunBrushOn("cube8", m, b);
			RunReplay("cube8", m, b);
		}
		{
			MakeSphere(16, 16, v, idx);
			NkEditMesh m;
			m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
			printf("-- sphere16 (cas dense) / brosse \"%s\"\n", b.name);
			RunBrushOn("sphere16", m, b);
			RunReplay("sphere16", m, b);
		}
		printf("\n");
	}

	// ──────────────────────────────────────────────────────────────────
	// LES SUJETS REELS.
	// ⚠️ POURQUOI ILS SONT INDISPENSABLES. Le cube et la sphere eprouvent
	//    l'ALGORITHME ; ils ne disent rien de ce qui arrive sur une sortie de
	//    notre propre chaine 3D. Exigence de Rodolf (19/09) : apres generation,
	//    on doit pouvoir corriger a la main -- par brosse, par modelisation ou
	//    par phrase. Une brosse qui ne marche que sur des primitives parfaites
	//    ne corrige rien du tout.
	uint32 realTried = 0, realLoaded = 0;
	{
		// ⚠️ CE QU'ON COMPTE, ET IL FAUT L'ECRIRE : trois chantiers mesurent
		//    maintenant les memes fichiers, et deux conventions derriere le meme
		//    mot se contredisent pour toujours sans qu'aucune soit fausse.
		//      tri=    TRIANGLES, pas faces telles qu'ecrites. LoadOBJ triangule,
		//              et BuildFromIndexed(quadify=true) ne refusionne PAS les
		//              quads d'origine. p512_rho1.obj ecrit 1985 faces (1310
		//              triangles + 675 quads) = 2660 triangles : c'est 2660 qu'on
		//              affiche.
		//      nm(tri) ARETES portees par plus de deux faces, comptees SUR LA
		//              VERSION TRIANGULEE. ⚠️ Mesure du 19/09 : le meme fichier
		//              donne non-manifold=1 sur ses faces ecrites et =2 apres
		//              triangulation. LA TRIANGULATION EN FABRIQUE : couper un
		//              quad ABCD en ABC+ACD cree la diagonale AC, et une de ces
		//              diagonales tombe sur une arete deja portee par deux faces.
		//              Le non-manifold n'est donc PAS une propriete du maillage
		//              seul : il depend d'un choix de diagonale arbitraire.
		//    Le critere de NON-AGGRAVATION reste valide quelle que soit la
		//    convention : il compare avant et apres avec LE MEME instrument.
		static const char *const kSubjects[] = {
			"p512_rho1.obj",     // sortie de notre chaine, porte du non-manifold
			"cylindre_rho2.obj", // tres grossier : le positif defavorable
			"tore_rho1.obj",     // genre 1 : un trou, donc pas une sphere deguisee
		};
		for (uint32 k = 0; k < sizeof(kSubjects) / sizeof(kSubjects[0]); ++k) {
			char path[512];
			snprintf(path, sizeof(path), "%s/%s", subjectDir, kSubjects[k]);
			++realTried;
			if (!NkFile::Exists(path)) {
				// ⚠️ REFUS NOMME. Ces fichiers vivent HORS du depot : leur absence
				//    est normale sur une autre machine, mais elle doit se VOIR.
				printf("-- sujet reel ABSENT : %s\n", path);
				continue;
			}
			renderer::NkGLTFMeshData md;
			if (!renderer::LoadOBJ(NkString(path), md) || md.vertices.Size() == 0) {
				printf("-- sujet reel ILLISIBLE : %s\n", path);
				continue;
			}
			NkEditMesh m;
			m.BuildFromIndexed(md.vertices.Data(), (uint32)md.vertices.Size(), md.indices.Data(),
							   (uint32)md.indices.Size(), true);
			const float32 diag = BBoxDiag(m);
			printf("-- %s : V=%u tri=%u bords=%u nm(tri)=%u diag=%.4f\n", kSubjects[k],
						   m.VertCount(), m.FaceCount(), CountBoundary(m), CountNonManifold(m), (double)diag);
			++realLoaded;
			// Rayon = 15 % de la diagonale : assez grand pour toucher, assez petit
			// pour laisser des sommets DEHORS -- sinon le negatif de zone serait
			// vide, donc vrai par construction, donc sans valeur.
			const float32 r = diag * 0.15f;
			for (uint16 bi = 0; bi < reg.Count(); ++bi) {
				NkBrushDesc b;
				if (reg.At(bi, b)) {
					RunBrushOn(kSubjects[k], m, b, r);
					NkBrushDesc br = b;
					br.radius = r;
					RunReplay(kSubjects[k], m, br);
				}
			}
		}
	}
	// ⚠️ « 0 echec » ne veut rien dire sans le nombre de cas : on dit combien
	//    de sujets reels ont REELLEMENT ete eprouves, pas seulement tentes.
	printf("\nsujets reels : %u eprouve(s) sur %u tente(s)\n", realLoaded, realTried);
	if (realLoaded == 0)
		printf("!! AUCUN SUJET REEL EPROUVE : le banc ne couvre que ses primitives.\n");


	// == `lisser` : LA DEUXIEME PRIMITIVE, EPROUVEE SUR SA PROPRE PROMESSE ===
	//
	// Les criteres communs (zone, rejeu, sens pilote par la donnee) sont deja
	// passes plus haut sur TOUTES les brosses du registre, celle-ci comprise.
	// Ce bloc ne mesure que ce qui lui est PROPRE : est-ce que ca lisse ?
	{
		NkBrushDesc bl;
		bool trouvee = false;
		for (uint16 bi = 0; bi < reg.Count() && !trouvee; ++bi) {
			NkBrushDesc b;
			// [!] PAR LE NOM, PAS PAR LA PRIMITIVE. « la premiere brosse SMOOTH »
			//     a cesse de designer `lisser` a la seconde ou `durcir` est arrivee
			//     dans le dossier : l ordre est alphabetique, et le banc a exige
			//     d une brosse qui DURCIT qu elle fasse baisser la rugosite.
			//     Un critere designe son sujet par son NOM, sinon le sujet change
			//     sous lui au premier fichier depose.
			if (reg.At(bi, b) && strcmp(b.name, "lisser") == 0) {
				bl = b;
				trouvee = true;
			}
		}
	//   [!] L'ABSENCE DE LA BROSSE EST UN ECHEC, PAS UN SAUT. Si aucune brosse
	//       de lissage n'est chargee, sauter ce bloc rendrait un banc tout vert
	//       qui n'a rien eprouve -- exactement le « 0 echec » sans nombre de cas
	//       que ce fichier denonce plus bas.
		Check(trouvee, "lisser : une brosse SMOOTH est chargee",
			trouvee ? bl.name : "aucune brosse de primitive lisser dans le registre");
		if (trouvee) {
			NkVector<NkVertex3D> v;
			NkVector<uint32> idx;
			MakeSphere(12, 12, v, idx);
			NkEditMesh m;
			m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
			// [!] LE CENTRE DU TRAIT EST LU SUR LE SUJET, PAS SUPPOSE.
			//     J'avais ecrit {0, 1, 0} en pensant « pole nord d'une sphere unite ».
			//     La bosse n'atteignait alors AUCUN sommet : changer sa force de 0,35
			//     a 1,0 laissait la rugosite rigoureusement identique (0,020709), ce
			//     qui etait le seul signe visible que le trait tombait dans le vide.
			//     On prend donc un sommet REEL : il est sur la surface par definition.
			const NkVec3f centre = m.verts[0].pos;
	//     On BOSSELLE d'abord avec `dessiner` : lisser une sphere deja lisse
	//     ferait baisser un nombre deja minuscule, et le critere passerait pour
	//     de mauvaises raisons. On fabrique le defaut qu'on veut voir disparaitre.
			NkBrushDesc bosse;
			bool okB = false;
			for (uint16 bi = 0; bi < reg.Count() && !okB; ++bi) {
				NkBrushDesc b;
				// Meme raison : « la premiere NORMAL » designait `creuser`, qui
				// creuse au lieu de bosseler -- le sujet du banc dependait de l ordre
				// alphabetique du dossier de brosses.
				if (reg.At(bi, b) && strcmp(b.name, "dessiner") == 0) {
					bosse = b;
					okB = true;
				}
			}
			// [!] LA MEME REGLE QUE POUR LA BROSSE SMOOTH, ET JE NE L AVAIS ECRITE
			//     QUE LA : sauter silencieusement quand la brosse de bosselage
			//     manque donne une sphere LISSE a lisser, donc une rugosite qui ne
			//     bouge pas -- et un critere qui rougit en accusant le module.
			Check(okB, "lisser : une brosse NORMAL est la pour bosseler",
				  okB ? bosse.name : "aucune : le sujet resterait lisse");
			if (okB) {
				bosse.radius = 0.25f;
				bosse.strength = 1.0f;
				NkSculptPoint p{};
				p.pos = centre;
				p.radius = bosse.radius;
				p.pressure = 1.f;
				(void)NkSculptApplyStroke(m, bosse, &p, 1u);
			}
			const float64 r0 = Rugosite(m, centre, 0.7f);
			const uint32 g0 = GroupesIntacts(m);

			NkEditMesh mLisse = m;
			NkBrushDesc bl2 = bl;
			bl2.radius = 0.6f;
			// Force au maximum utile : on veut un effet FRANC, pas un effet
			// detectable. Un critere qui se contente de « ca a baisse » verdit sur
			// du bruit d arrondi -- premiere version mesuree a 0,003 % d ecart,
			// soit indiscernable d un placebo.
			bl2.strength = 1.f;
			NkSculptPoint q{};
			q.pos = centre;
			q.radius = bl2.radius;
			q.pressure = 1.f;
			const NkSculptApply a1 = NkSculptApplyStroke(mLisse, bl2, &q, 1u);
			const float64 r1 = Rugosite(mLisse, centre, 0.7f);
			char d[192];
			// LE SEUIL EST ECRIT, PAS DEDUIT DU RESULTAT : on exige 1 % de baisse
			// relative. « r1 < r0 » etait vrai a 0,003 % pres -- un ecart que
			// n importe quel arrondi produit. *Different ne veut pas dire visible.*
			const float64 baisse = (r0 > 0.0) ? (r0 - r1) / r0 : 0.0;
			snprintf(d, sizeof(d), "rugosite %.6f -> %.6f (-%.2f %%, %u groupes touches)",
					 r0, r1, baisse * 100.0, (unsigned)a1.groupsInRadius);
			Check(a1.applied && baisse > 0.01, "lisser : la rugosite DESCEND franchement", d);

	//     LE NEGATIF, ET IL EST DANS LA DONNEE : `sens = -1` prend la meme
	//     formule a rebours et doit faire MONTER la rugosite. S'il la faisait
	//     baisser aussi, c'est que le sens n'est pas lu -- et le critere du
	//     dessus serait vrai quoi qu'il arrive.
			NkEditMesh mDur = m;
			NkBrushDesc bd = bl2;
			bd.dir = -1.f;
			const NkSculptApply a2 = NkSculptApplyStroke(mDur, bd, &q, 1u);
			const float64 r2 = Rugosite(mDur, centre, 0.7f);
			snprintf(d, sizeof(d), "rugosite %.6f -> %.6f", r0, r2);
			Check(a2.applied && r2 > r0, "lisser : sens=-1 la fait MONTER", d);

	//     LA SOUDURE TIENT. C'est ce qui separe un lissage d'un dechirement :
	//     les copies coincidentes d'un coin doivent rester coincidentes.
			const uint32 g1 = GroupesIntacts(mLisse);
			snprintf(d, sizeof(d), "%u groupes avant, %u apres", (unsigned)g0, (unsigned)g1);
			Check(g1 == g0, "lisser : la soudure tient (aucun dechirement)", d);

	//     CONVERGENCE : un second passage ne doit pas faire REMONTER la
	//     rugosite. Un schema dont le pas depasse la moyenne oscille, et on
	//     ne le verrait pas sur un seul tampon.
			NkEditMesh mDeux = mLisse;
			(void)NkSculptApplyStroke(mDeux, bl2, &q, 1u);
			const float64 r3 = Rugosite(mDeux, centre, 0.7f);
			snprintf(d, sizeof(d), "%.6f -> %.6f -> %.6f", r0, r1, r3);
			Check(r3 <= r1, "lisser : deux passages ne remontent pas", d);

	//     ET IL NE TOUCHE PAS CE QUI EST HORS DE SA ZONE : meme garde que les
	//     autres primitives, mais elle se verifie ICI aussi, parce que le
	//     lissage lit les VOISINS -- un voisin hors zone pourrait etre deplace
	//     par inadvertance en ecrivant dans le mauvais tableau.
			NkEditMesh mLoin = m;
			NkSculptPoint z{};
			z.pos = NkVec3f{50.f, 50.f, 50.f};
			z.radius = 0.1f;
			z.pressure = 1.f;
			const NkSculptApply a3 = NkSculptApplyStroke(mLoin, bl2, &z, 1u);
			const float64 r4 = Rugosite(mLoin, centre, 0.7f);
			snprintf(d, sizeof(d), "applique=%d rugosite %.6f", a3.applied ? 1 : 0, r4);
			Check(!a3.applied && r4 == r0, "lisser : hors zone, rien n'est ecrit", d);
		}
	}


	// == bevel : CE QUE LA LARGEUR FAIT A LA GEOMETRIE =======================
	//
	// Mesure du 20/09 : sur 10 formulations de biseau, le modele met DEUX fois
	// le nombre de SEGMENTS dans le champ LARGEUR (« sur 3 segments » ->
	// bevel:3). La question restee ouverte etait : sur un cube de cote 1, une
	// largeur de 3 mange-t-elle tout ?
	//
	// [!] LE COMPTE DE SOMMETS NE REPOND PAS, et c'est pour ca que la mesure
	//     vient ici. Dans le modeleur, bevel:0.2:4 et bevel:0:4 rendent le MEME
	//     triplet (28/64/54) : la topologie ne depend que du nombre de segments.
	//     Un biseau de largeur nulle creerait 28 sommets CONFONDUS et le compteur
	//     dirait la meme chose qu'un biseau juste. *Il faut une longueur, pas un
	//     cardinal.* On prend la diagonale de la boite englobante et le volume.
	{
		NkVector<NkVertex3D> v;
		NkVector<uint32> idx;
		const float32 kOffs[4] = {0.f, 0.2f, 1.f, 3.f};
		float64 sVolBevel[4] = {0.0, 0.0, 0.0, 0.0};
		for (int32 oi = 0; oi < 4; ++oi) {
			MakeCube(v, idx);
			NkEditMesh m;
			m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
			const float64 vol0 = (float64)NkSculptSignedVolume(m);
			NkVec3f lo0, hi0;
			BBox(m, lo0, hi0);
			m.SelectAll();
			NkBevelParams bp;
			bp.offset = kOffs[oi];
			bp.segments = 4;
			const bool ok = m.BevelSelected(bp, nullptr);
			const float64 vol1 = (float64)NkSculptSignedVolume(m);
			NkVec3f lo1, hi1;
			BBox(m, lo1, hi1);
			const float32 d0 = (hi0 - lo0).Len();
			const float32 d1 = (hi1 - lo1).Len();
			char lab[96], det[192];
			snprintf(lab, sizeof(lab), "bevel largeur %.1f sur cube 1", (double)kOffs[oi]);
			snprintf(det, sizeof(det), "applique=%d  diag %.3f -> %.3f  volume %.4f -> %.4f",
					 ok ? 1 : 0, (double)d0, (double)d1, vol0, vol1);
	//     LE CRITERE : un biseau ne GRANDIT PAS l'objet et ne le fait pas
	//     disparaitre. La boite englobante doit rester celle du cube (a
	//     l'arrondi pres) et le volume rester strictement positif. Une largeur
	//     demesuree qui ferait l'un ou l'autre serait le defaut cherche.
			const bool sain = ok && (d1 <= d0 * 1.02f) && (vol1 > 0.0);
			Check(sain, lab, det);
			sVolBevel[oi] = vol1;
		}
		// [!] LE FAIT QUI PROTEGE VRAIMENT, ET C'EST LUI QU'ON GARDE.
		//     Largeur 1 et largeur 3 rendent EXACTEMENT le meme volume : le
		//     moteur SATURE -- un biseau ne peut pas depasser ce que les aretes
		//     incidentes permettent. C'est ce qui fait qu'un `bevel:3` lache par
		//     le modele donne un biseau tres marque, PAS un objet mange.
		//     Les quatre criteres du dessus ne peuvent pas rougir tant que cette
		//     saturation existe ; celui-ci rougira le jour ou elle disparaitra,
		//     et c'est exactement ce qu'on veut apprendre.
		{
			char d2[160];
			snprintf(d2, sizeof(d2), "largeur 1.0 -> %.4f, largeur 3.0 -> %.4f",
					 sVolBevel[2], sVolBevel[3]);
			Check(sVolBevel[2] == sVolBevel[3] && sVolBevel[2] > 0.0,
				  "bevel : une largeur demesuree SATURE", d2);
		}
	}


	// == loopcut : UN PARAMETRE INVENTE PEUT-IL DENATURER ? ==================
	//
	// Mesure du 20/09 : le modele ajoute un parametre que la demande ne donne
	// pas 4 fois sur 10, alors que le contrat l'interdit en toutes lettres.
	// `loopcut : Boucles (1 a 5), Glissement (-1 a 1)` -- il ecrit loopcut:2:-1
	// quand on n'a demande que deux boucles.
	//
	// [!] LE JUGE EST LA CARACTERISTIQUE D'EULER, et c'est ce qui rend ce
	//     critere different d'un attendu dicte : V - E + F vaut 2 pour toute
	//     surface fermee de genre 0, quelle que soit la subdivision. On ne dit
	//     donc PAS combien de sommets loopcut doit produire -- on demande
	//     seulement que le maillage reste un maillage. *Un critere qui juge la
	//     coherence interne n'a pas besoin qu'on lui souffle la reponse.*
	{
		NkVector<NkVertex3D> v;
		NkVector<uint32> idx;
		const float32 kSlide[6] = {0.99f, 0.995f, 0.999f, 0.9999f, 1.f, -1.f};
		const char *const kNom[6] = {"glissement 0.99", "glissement 0.995", "glissement 0.999",
							"glissement 0.9999", "glissement 1.00 (borne)",
							"glissement -1.00 (borne)"};
		for (int32 si = 0; si < 6; ++si) {
			MakeCube(v, idx);
			NkEditMesh m;
			m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
			m.SelectAll();
			NkLoopCutParams lp;
			lp.cuts = 2;
			lp.slide = kSlide[si];
			const bool ok = m.LoopCutFromSelectedEdge(lp);
			NkVector<uint32> canon;
			m.BuildVertexMerge(canon);
			uint32 vs = 0;
			for (uint32 i = 0; i < m.VertCount(); ++i)
				if (canon[i] == i)
					++vs;
			uint32 fs = 0;
			for (uint32 f = 0; f < m.FaceCount(); ++f)
				if (m.faces[f].alive)
					++fs;
			NkVector<uint32> pairs;
			m.GetUniqueEdges(pairs);
			const uint32 es = (uint32)(pairs.Size() / 2u);
			const int32 euler = (int32)vs - (int32)es + (int32)fs;
			const uint32 nm = CountNonManifold(m);
			char lab[96], det[192];
			snprintf(lab, sizeof(lab), "loopcut 2 boucles, %s", kNom[si]);
			snprintf(det, sizeof(det), "applique=%d  V=%u E=%u F=%u  V-E+F=%d  non-manifold=%u",
					 ok ? 1 : 0, (unsigned)vs, (unsigned)es, (unsigned)fs, (int)euler,
					 (unsigned)nm);
			Check(ok && euler == 2 && nm == 0u, lab, det);
		}
	}

	printf("=== %d ok, %d ROUGE ===\n", gPass, gFail);
	return (gFail == 0) ? 0 : 1;
}
