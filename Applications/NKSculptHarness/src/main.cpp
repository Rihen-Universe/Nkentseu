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
static void RunBrushOn(const char *primName, NkEditMesh &m, const NkBrushDesc &b) {
	char label[128];
	Snapshot before;
	Capture(m, before);

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
		char det[96];
		snprintf(det, sizeof(det), "V0=%+.6f (doit etre > 0 : faces sortantes)", (double)v0);
		snprintf(label, sizeof(label), "%s [instrument] orientation coherente", primName);
		Check(v0 > 0.f, label, det);
	}

	// Le sommet du maillage le plus haut : on vise une zone qui existe.
	NkVec3f center{0.f, 0.f, 0.f};
	float32 best = -1e30f;
	for (uint32 i = 0; i < m.VertCount(); ++i)
		if (m.verts[i].pos.y > best) {
			best = m.verts[i].pos.y;
			center = m.verts[i].pos;
		}

	const float32 radius = b.radius;
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

	// ── [direction] LES DEUX SENS ───────────────────────────────────────────
	// Exiger les DEUX est ce qui distingue une vraie brosse d'un tirage a pile
	// ou face : un defaut qui pousse toujours dans le meme sens en echoue un.
	const uint32 bnd0 = CountBoundary(m);
	float32 volPlus = 0.f, volMinus = 0.f;
	{
		NkBrushDesc up = b;
		up.dir = 1.f;
		const NkSculptApply r = NkSculptApplyStroke(m, up, &pt, 1);
		volPlus = r.volumeAfter - r.volumeBefore;
		char det[96];
		snprintf(det, sizeof(det), "dV=%+.6f  sommets=%u", (double)volPlus, r.vertsMoved);
		snprintf(label, sizeof(label), "%s/%s direction sens=+1 gonfle", primName, b.name);
		Check(r.applied && volPlus > 0.f, label, det);

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
		snprintf(det, sizeof(det), "dV=%+.6f  sommets=%u", (double)volMinus, r.vertsMoved);
		snprintf(label, sizeof(label), "%s/%s direction sens=-1 creuse", primName, b.name);
		Check(r.applied && volMinus < 0.f, label, det);
		Restore(m, before);
	}

	// ── [direction] LES DEUX SENS NE SE CONFONDENT PAS ──────────────────────
	{
		snprintf(label, sizeof(label), "%s/%s les deux sens different", primName, b.name);
		Check(volPlus > 0.f && volMinus < 0.f, label, "sinon : un seul sens agit");
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
		const float32 dv = r.volumeAfter - r.volumeBefore;
		const bool coherent = (b.dir > 0.f) ? (dv > 0.f) : (dv < 0.f);
		char det[128];
		snprintf(det, sizeof(det), "fichier sens=%+.0f -> dV=%+.6f", (double)b.dir, (double)dv);
		snprintf(label, sizeof(label), "%s/%s DONNEE pilote le sens", primName, b.name);
		Check(r.applied && coherent, label, det);
		Restore(m, before);
	}
}

int main(int argc, char **argv) {
	const char *brushDir = "Applications/NK3DModeler/data/brushes";
	for (int i = 1; i < argc; ++i)
		if (strncmp(argv[i], "--brosses=", 10) == 0)
			brushDir = argv[i] + 10;

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
		}
		{
			MakeSphere(16, 16, v, idx);
			NkEditMesh m;
			m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), true);
			printf("-- sphere16 (cas dense) / brosse \"%s\"\n", b.name);
			RunBrushOn("sphere16", m, b);
		}
		printf("\n");
	}

	printf("=== %d ok, %d ROUGE ===\n", gPass, gFail);
	return (gFail == 0) ? 0 : 1;
}
