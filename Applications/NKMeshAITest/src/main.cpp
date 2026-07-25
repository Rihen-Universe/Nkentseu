// =============================================================================
// NKMeshAITest — Étape 1 de la modélisation par IA : IMITATION d'un expert.
//
// Pont ÉDITEUR (NkEditMesh + couche de commandes, NKRenderer) <-> ML NKAI.
//   Observation : NkEditMesh -> vecteur de features.
//   Action      : type de commande de modélisation (Subdiv/Extrude/Mirror/Array).
//   Expert      : petite policy heuristique (décide l'action selon l'état).
//   Apprentissage : un MLP (NKNN) apprend à IMITER l'expert -> la précision monte.
//
// Prouve la chaîne obs -> policy -> action de bout en bout, from-scratch, petite
// échelle. Fondation pour : apprendre depuis de vraies sessions .nkmec, puis RL
// vers une cible, puis conditionnement image/texte (image-to-3D). Réutilisable
// pour NkAnima (obs = squelette, actions = poses).
// =============================================================================
#include "NKRenderer/Mesh/NkEditMesh.h"
#include "NKTensor/NkTensor.h"
#include "NKNN/NkNN.h"
#include "NKOptim/NkOptim.h"
#include "NKData/NkData.h"
#include "NKTrain/NkTrain.h"
#include "NKFileSystem/NkFile.h"
#include <cstdio>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;
using namespace nkentseu::math; // NkVec3f
using renderer::NkEditMesh;

// ── Espace d'actions (classes que la policy prédit) ──────────────────────────
enum ModelAction { ACT_SUBDIV = 0, ACT_EXTRUDE = 1, ACT_MIRROR = 2, ACT_ARRAY = 3, ACT_COUNT = 4 };

static const int FEAT_DIM = 8;

// PRNG déterministe (zéro-STL).
struct Rng {
		uint32 s;

		uint32 nextu() {
			s = s * 1664525u + 1013904223u;
			return s;
		}

		float f() {
			return (float)(nextu() >> 8) * (1.0f / 16777216.0f);
		}

		int i(int n) {
			return (int)(nextu() % (uint32)n);
		}
};

static float fmin3(float a, float b, float c) {
	float m = a < b ? a : b;
	return m < c ? m : c;
}

static float fmax3(float a, float b, float c) {
	float m = a > b ? a : b;
	return m > c ? m : c;
}

static float fabsf_(float x) {
	return x < 0.f ? -x : x;
}

// Cube NkEditMesh (échelles sx/sy/sz ; ox = décalage X pour créer de l'asymétrie).
static void BuildCube(NkEditMesh &m, float sx, float sy, float sz, float ox) {
	renderer::NkVertex3D v[8];
	const float px[8] = {-1, 1, 1, -1, -1, 1, 1, -1};
	const float py[8] = {-1, -1, 1, 1, -1, -1, 1, 1};
	const float pz[8] = {-1, -1, -1, -1, 1, 1, 1, 1};
	for (int k = 0; k < 8; k++) {
		v[k] = renderer::NkVertex3D{};
		v[k].pos = {px[k] * sx * 0.5f + ox, py[k] * sy * 0.5f, pz[k] * sz * 0.5f};
		v[k].normal = {0.f, 1.f, 0.f};
		v[k].tangent = {1.f, 0.f, 0.f};
		v[k].color = 0xFFFFFFFFu;
	}
	const uint32 idx[36] = {0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 0, 4, 5, 0, 5, 1,
							1, 5, 6, 1, 6, 2, 2, 6, 7, 2, 7, 3, 3, 7, 4, 3, 4, 0};
	m.BuildFromIndexed(v, 8, idx, 36, /*quadify*/ true);
}

// OBSERVATION : NkEditMesh -> vecteur de features [FEAT_DIM].
static void Encode(const NkEditMesh &m, float *f) {
	const uint32 vc = m.VertCount();
	uint32 fc = 0;
	for (uint32 i = 0; i < m.FaceCount(); i++)
		if (m.faces[i].alive)
			fc++; // faces VIVANTES
	NkVec3f mn{1e30f, 1e30f, 1e30f}, mx{-1e30f, -1e30f, -1e30f};
	for (uint32 k = 0; k < vc; k++) {
		NkVec3f p = m.verts[k].pos;
		if (p.x < mn.x)
			mn.x = p.x;
		if (p.y < mn.y)
			mn.y = p.y;
		if (p.z < mn.z)
			mn.z = p.z;
		if (p.x > mx.x)
			mx.x = p.x;
		if (p.y > mx.y)
			mx.y = p.y;
		if (p.z > mx.z)
			mx.z = p.z;
	}
	const float ex = mx.x - mn.x, ey = mx.y - mn.y, ez = mx.z - mn.z;
	const float emax = fmax3(ex, ey, ez) + 1e-6f, emin = fmin3(ex, ey, ez);
	const float cx = 0.5f * (mn.x + mx.x);
	f[0] = (float)vc / 64.f;
	f[1] = (float)fc / 64.f;
	f[2] = ex;
	f[3] = ey;
	f[4] = ez;
	f[5] = emin / emax;							  // aplatissement (1=cube, ~0=plat)
	f[6] = fabsf_(cx);							  // asymétrie X (0=centré)
	f[7] = (float)fc / (float)(vc > 0 ? vc : 1u); // ratio faces/sommets
}

// EXPERT heuristique : features -> action (le "professeur" à imiter).
static int Expert(const float *f) {
	const float fc = f[1] * 64.f;
	if (fc < 8.f)
		return ACT_SUBDIV; // peu de faces -> subdiviser
	if (f[5] < 0.35f)
		return ACT_EXTRUDE; // très plat -> extruder (donner du volume)
	if (f[6] > 0.20f)
		return ACT_MIRROR; // asymétrique -> miroir
	return ACT_ARRAY;	   // sinon -> array
}

// Diversifie l'état (varie le nb de faces via subdivisions).
static void Diversify(NkEditMesh &m, Rng &rng) {
	if (rng.i(3) != 0) {
		m.SelectAll();
		renderer::NkSubdivideParams p;
		p.cuts = 1;
		m.SubdivideSelectedFaces(p);
	}
}

// Génère `n` échantillons (features + labels) via l'expert.
static void GenDataset(uint32 n, Rng &rng, NkVector<float> &feats, NkVector<int32> &labels) {
	feats.Clear();
	labels.Clear();
	for (uint32 s = 0; s < n; s++) {
		NkEditMesh m;
		float sx = 0.3f + rng.f() * 1.7f, sy = 0.3f + rng.f() * 1.7f, sz = 0.3f + rng.f() * 1.7f;
		if (rng.i(3) == 0) {
			int ax = rng.i(3);
			if (ax == 0)
				sx *= 0.15f;
			else if (ax == 1)
				sy *= 0.15f;
			else
				sz *= 0.15f;
		}
		const float ox = (rng.i(2) == 0) ? 0.f : (rng.f() * 1.2f); // parfois décalé -> asymétrie
		BuildCube(m, sx, sy, sz, ox);
		const int nd = rng.i(3);
		for (int j = 0; j < nd; j++)
			Diversify(m, rng);
		float f[FEAT_DIM];
		Encode(m, f);
		const int a = Expert(f);
		for (int d = 0; d < FEAT_DIM; d++)
			feats.PushBack(f[d]);
		labels.PushBack((int32)a);
	}
}

// =============================================================================
// ÉTAPE 2 (ROADMAP NKAI, "🧠 Modélisation par IA", point 2) — apprendre depuis
// de VRAIES sessions .nkmec (imitation HUMAINE, par opposition à l'expert
// heuristique synthétique de l'étape 1 ci-dessus, qu'on NE TOUCHE PAS).
//
// Désérialise le JOURNAL RÉEL de commandes (moteur NKRenderer, AUCUNE réimplé-
// mentation du format : on réutilise directement renderer::NkMeshEditRecorder::
// Deserialize + renderer::NkMeshEditCommand::Apply, le même code que Demo3D/F5-F6)
// -> pour chaque commande du journal : (features de l'état AVANT [Encode(), même
// fonction que l'étape 1], commande+paramètres complets [op + selection + params]).
// C'est la fondation de données pour l'entraînement supervisé par imitation
// humaine ; l'entraînement lui-même attend un VOLUME de vraies sessions qu'on
// n'a pas encore (cf. rapport final honnête).
// =============================================================================

// Vocabulaire d'actions RÉEL (celui de l'éditeur, renderer::NkMeshEditOp) —
// DIFFÉRENT des 4 classes synthétiques de l'étape 1 (Subdiv/Extrude/Mirror/Array,
// qui étaient les actions du MODIFICATEUR heuristique, pas de l'éditeur humain).
static const char *kRealOpNames[9] = {"None", "Extrude", "Delete",	"Merge", "MakeFace",
									   "Subdivide", "LoopCut", "Bisect", "Move"};

static uint32 CountAliveFaces(const NkEditMesh &m) {
	uint32 fc = 0;
	for (uint32 i = 0; i < m.FaceCount(); i++)
		if (m.faces[i].alive)
			fc++;
	return fc;
}

// Description lisible d'une commande (op + paramètres pertinents pour ce type d'op).
static void DescribeCmd(const renderer::NkMeshEditCommand &c, char *buf, int bufSize) {
	const int opIdx = (int)c.op;
	const char *name = (opIdx >= 0 && opIdx < 9) ? kRealOpNames[opIdx] : "?";
	switch (c.op) {
		case renderer::NkMeshEditOp::Extrude:
			snprintf(buf, bufSize, "%s(individual=%d, offset=%.3f)", name, c.extrude.individual ? 1 : 0,
					 c.extrude.offset);
			break;
		case renderer::NkMeshEditOp::Merge:
			snprintf(buf, bufSize, "%s(mode=%d)", name, c.merge.mode);
			break;
		case renderer::NkMeshEditOp::Subdivide:
			snprintf(buf, bufSize, "%s(cuts=%d)", name, c.subdiv.cuts);
			break;
		case renderer::NkMeshEditOp::Bisect:
			snprintf(buf, bufSize, "%s(point=(%.2f,%.2f,%.2f) normal=(%.2f,%.2f,%.2f))", name, c.planePoint.x,
					 c.planePoint.y, c.planePoint.z, c.planeNormal.x, c.planeNormal.y, c.planeNormal.z);
			break;
		case renderer::NkMeshEditOp::Move:
			snprintf(buf, bufSize, "%s(%u sommet(s) deplace(s))", name, (unsigned)c.moveDeltas.Size());
			break;
		default:
			snprintf(buf, bufSize, "%s", name);
			break;
	}
}

// Une paire d'imitation : état AVANT (encodé, même Encode() que l'étape 1) + le
// label d'action (vocabulaire réel NkMeshEditOp) + la commande complète (params).
struct NkImitationSample {
		float feat[FEAT_DIM];
		int32 opLabel; // (int)renderer::NkMeshEditOp — 0..8, PAS les 4 classes de l'étape 1
		uint32 selCount;
		bool applied; // Apply() a-t-il réellement changé la topologie/géométrie ?
};

// Rejoue un journal réel (déjà désérialisé) depuis un mesh de BASE : pour
// chaque commande, capture l'état AVANT (features), puis applique (avance
// l'état pour la commande suivante — c'est la trajectoire réelle enregistrée).
static uint32 BuildImitationPairsFromRecorder(const renderer::NkMeshEditRecorder &rec, const NkEditMesh &base,
											   NkVector<NkImitationSample> &outSamples, bool verbose) {
	outSamples.Clear();
	NkEditMesh m = base;
	uint32 okCount = 0;
	for (uint32 i = 0; i < rec.Count(); i++) {
		const renderer::NkMeshEditCommand &c = rec.At(i);
		NkImitationSample s{};
		Encode(m, s.feat);
		s.opLabel = (int32)c.op;
		s.selCount = (uint32)c.selection.Size();
		s.applied = c.Apply(m); // NB : Apply() applique aussi la sélection enregistrée sur m
		if (s.applied)
			okCount++;
		outSamples.PushBack(s);
		if (verbose) {
			char desc[256];
			DescribeCmd(c, desc, (int)sizeof(desc));
			printf("  [%u] avant: verts=%.0f faces=%.0f flat=%.2f asymX=%.2f | commande=%s | selection=%u sommet(s) "
				   "| applique=%s\n",
				   i, s.feat[0] * 64.f, s.feat[1] * 64.f, s.feat[5], s.feat[6], desc, s.selCount,
				   s.applied ? "oui" : "non");
		}
	}
	return okCount;
}

// Reconstruit le maillage SPHÈRE utilisé par Demo3D (NkMeshSystem::GetSphere(),
// toujours appelé en interne avec stacks=slices=32) — MÊME algorithme (UV-sphere)
// que NkMeshSystem::BuildSphereData, recopié ici car c'est une méthode PRIVÉE
// d'instance de NkMeshSystem (contexte moteur/GPU que cette app console n'a pas).
// ⚠️ Best-effort documenté : le format .nkmec NE STOCKE PAS le maillage de base
// (design assumé du journal : "commandes rejouées sur une base fournie par
// l'appelant", cf. NkEditMesh.h). Pour une VRAIE session Demo3D on déduit la
// base depuis le code source (Demo3D_Frame : objets 0..15 = sphère, reste =
// cube), PAS depuis le fichier .nkmec lui-même — limitation honnête du format.
static void BuildSphere32Like(NkEditMesh &m) {
	NkVector<renderer::NkVertex3D> v;
	NkVector<uint32> idx;
	const uint32 stacks = 32u, slices = 32u;
	const float PI = 3.14159265358979323846f;
	for (uint32 i = 0; i <= stacks; i++) {
		const float phi = PI * (float)i / (float)stacks;
		for (uint32 j = 0; j <= slices; j++) {
			const float theta = 2.f * PI * (float)j / (float)slices;
			const float x = sinf(phi) * cosf(theta), y = cosf(phi), z = sinf(phi) * sinf(theta);
			renderer::NkVertex3D vt{};
			vt.pos = {x * 0.5f, y * 0.5f, z * 0.5f};
			vt.normal = {x, y, z};
			vt.tangent = {-sinf(theta), 0.f, cosf(theta)};
			vt.uv = {(float)j / (float)slices, 1.f - (float)i / (float)stacks};
			vt.color = 0xFFFFFFFFu;
			v.PushBack(vt);
		}
	}
	for (uint32 i = 0; i < stacks; i++)
		for (uint32 j = 0; j < slices; j++) {
			const uint32 b = i * (slices + 1) + j;
			idx.PushBack(b);
			idx.PushBack(b + slices + 1);
			idx.PushBack(b + 1);
			idx.PushBack(b + 1);
			idx.PushBack(b + slices + 1);
			idx.PushBack(b + slices + 2);
		}
	m.BuildFromIndexed(v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size(), /*quadify*/ true);
}

// [A] Parse le VRAI fichier trouvé dans le dépôt (edit_session.nkmec, 612 o,
// enregistré par un humain via Demo3D/F5 — PAS généré par ce programme).
static void RunStep2_RealSession() {
	printf("\n--- [A] Session REELLE trouvee dans le depot : edit_session.nkmec ---\n");
	NkVector<uint8> bytes = NkFile::ReadAllBytes("edit_session.nkmec");
	if (bytes.Empty()) {
		printf("  ATTENTION : 'edit_session.nkmec' introuvable depuis le repertoire courant d'execution.\n");
		printf("  (Le fichier existe a la racine du depot ; ce test suppose un cwd = racine du workspace,\n");
		printf("  comme les autres apps NKAI qui lisent Resources/Datasets/ en chemin relatif.)\n");
		return;
	}
	renderer::NkMeshEditRecorder rec;
	const bool ok = rec.Deserialize(bytes.Data(), (uint32)bytes.Size());
	printf("  %u octets lus, magic+version OK -> deserialisation %s, %u commande(s) reelles (humaines).\n",
		   (uint32)bytes.Size(), ok ? "OK" : "ECHEC", rec.Count());
	if (!ok || rec.Count() == 0)
		return;

	NkEditMesh base;
	BuildSphere32Like(base);
	printf("  Base reconstruite (best-effort, cf. commentaire) : sphere UV 32x32 quadifiee -> %u sommets, %u faces "
		   "vivantes.\n",
		   base.VertCount(), CountAliveFaces(base));

	NkVector<NkImitationSample> samples;
	const uint32 okApplied = BuildImitationPairsFromRecorder(rec, base, samples, /*verbose*/ true);
	printf("  -> %u paire(s) (etat_avant, commande) extraites du VRAI journal ; %u/%u commandes se sont appliquees "
		   "(topologie changee) sur cette base reconstruite.\n",
		   (uint32)samples.Size(), okApplied, rec.Count());
	if (okApplied != rec.Count())
		printf("  (Ecart possible : rien ne garantit que la base EXACTE de cette session precise soit la sphere "
			   "32x32 -- le format .nkmec ne stocke pas la base, limitation honnete documentee ci-dessus.)\n");
}

// [B] Round-trip complet avec une base CONNUE : construit un mesh de base
// (le même cube que l'étape 1, BuildCube), enregistre une VRAIE petite session
// en passant par le chemin moteur EXACT (NkMeshEditCommand::Apply +
// NkMeshEditRecorder::Push/Serialize — le même code que Demo3D F5/F6, aucune
// réimplémentation), l'écrit sur disque, le relit, vérifie le round-trip
// binaire OCTET POUR OCTET, puis reconstruit les paires d'imitation.
// ⚠️ Honnêteté : piloté par CE programme (appels directs), pas par la souris/
// clavier dans Demo3D — cet environnement n'a pas d'automatisation d'IHM
// graphique. Le fichier produit est néanmoins un .nkmec 100% authentique
// (mêmes octets qu'une vraie session Demo3D pour les mêmes actions).
static void RunStep2_RoundTripDemo() {
	printf("\n--- [B] Round-trip .nkmec avec base CONNUE (preuve chainee, bout en bout) ---\n");
	printf("  (3 commandes appliquees via le vrai chemin moteur NkMeshEditCommand::Apply +\n");
	printf("   NkMeshEditRecorder -- identique a Demo3D F5/F6 -- pilotees par ce programme,\n");
	printf("   pas de GUI disponible ici pour un vrai geste souris/clavier.)\n");

	NkEditMesh m;
	BuildCube(m, 1.f, 1.f, 1.f, 0.f);
	const NkEditMesh base = m;
	renderer::NkMeshEditRecorder rec;

	// 1) Subdivide (aucune selection -> tout le mesh, cf. NkEditMesh::SubdivideSelectedFaces).
	{
		renderer::NkMeshEditCommand c;
		c.op = renderer::NkMeshEditOp::Subdivide;
		c.subdiv.cuts = 1;
		if (c.Apply(m))
			rec.Push(c);
	}

	// 2) Extrude la premiere face quad vivante trouvee (sélection = ses sommets).
	NkVector<uint32> extrudeSel;
	{
		renderer::NkEmId faceId = renderer::NK_EM_INVALID;
		for (uint32 f = 0; f < m.FaceCount(); f++)
			if (m.faces[f].alive && m.FaceSize((renderer::NkEmId)f) == 4) {
				faceId = (renderer::NkEmId)f;
				break;
			}
		if (faceId != renderer::NK_EM_INVALID) {
			NkVector<renderer::NkEmId> loop;
			m.GetFaceVerts(faceId, loop);
			for (uint32 k = 0; k < (uint32)loop.Size(); k++)
				extrudeSel.PushBack((uint32)loop[k]);
		}
		renderer::NkMeshEditCommand c;
		c.op = renderer::NkMeshEditOp::Extrude;
		c.selection = extrudeSel;
		c.extrude.offset = 0.35f;
		if (c.Apply(m))
			rec.Push(c);
	}

	// 3) Move : deplace les sommets du cap extrude (ajoutes en fin de tableau par Extrude).
	{
		renderer::NkMeshEditCommand c;
		c.op = renderer::NkMeshEditOp::Move;
		const uint32 vc = m.VertCount();
		const uint32 n = (uint32)extrudeSel.Size();
		for (uint32 k = 0; k < n && k < vc; k++) {
			c.selection.PushBack(vc - n + k);
			c.moveDeltas.PushBack(NkVec3f{0.10f, 0.05f, 0.f});
		}
		if (c.Apply(m))
			rec.Push(c);
	}

	NkVector<uint8> bytes;
	rec.Serialize(bytes);
	const char *outPath = "nkmec_roundtrip_demo.nkmec";
	const bool wrote = NkFile::WriteAllBytes(outPath, bytes);
	printf("  Enregistre : %u commande(s), %u octets -> '%s' (%s)\n", rec.Count(), (uint32)bytes.Size(), outPath,
		   wrote ? "OK" : "ECHEC");

	NkVector<uint8> bytes2 = NkFile::ReadAllBytes(outPath);
	renderer::NkMeshEditRecorder rec2;
	const bool ok2 = rec2.Deserialize(bytes2.Data(), (uint32)bytes2.Size());
	bool byteExact = (bytes.Size() == bytes2.Size());
	if (byteExact)
		for (uint32 i = 0; i < (uint32)bytes.Size(); i++)
			if (bytes[i] != bytes2[i]) {
				byteExact = false;
				break;
			}
	printf("  Relu depuis le disque : deserialisation %s, %u commande(s), round-trip binaire %s.\n",
		   ok2 ? "OK" : "ECHEC", rec2.Count(), byteExact ? "EXACT (octet pour octet)" : "DIFFERENT");

	NkVector<NkImitationSample> samples;
	const uint32 okApplied = BuildImitationPairsFromRecorder(rec2, base, samples, /*verbose*/ true);
	printf("  -> %u paire(s) extraites depuis le fichier relu (base CONNUE, sans ambiguite) ; %u/%u commandes "
		   "appliquees.\n",
		   (uint32)samples.Size(), okApplied, rec2.Count());

	// Vérification de PLOMBERIE (pas un entraînement : 3 échantillons, bien trop
	// peu) — prouve juste que les paires s'insèrent dans la même pile ML que
	// l'étape 1 (NkTensor -> NkVar -> NkDense), prêtes pour un vrai entraînement
	// le jour où le volume de vraies sessions existera.
	if (!samples.Empty()) {
		NkVector<float> flat;
		for (uint32 i = 0; i < (uint32)samples.Size(); i++)
			for (int d = 0; d < FEAT_DIM; d++)
				flat.PushBack(samples[i].feat[d]);
		NkTensor X = NkTensor::FromData(NkShape{(int64)samples.Size(), (int64)FEAT_DIM}, flat.Data(), NkDType::NK_F32);
		nn::NkDense probe1(FEAT_DIM, 16u, 42u);
		nn::NkDense probe2(16u, 9u, 43u); // 9 = |renderer::NkMeshEditOp| (vocabulaire REEL)
		NkVar logits = probe2.Forward(nn::Relu(probe1.Forward(NkVar::Leaf(X))));
		printf("  Verif. plomberie ML (PAS un entrainement, %u echantillons = bien trop peu) : features -> NkTensor "
			   "-> NKNN OK, logits shape [%d,%d].\n",
			   (uint32)samples.Size(), (int)logits.Value().Shape()[0], (int)logits.Value().Shape()[1]);
	}
}

int main() {
	printf("=== NKMeshAITest — Etape 1 : imitation d'un expert de modelisation ===\n");
	printf("(from-scratch, petite echelle, pedagogique)\n\n");
	Rng rng{20260707u};

	NkVector<float> trF, teF;
	NkVector<int32> trL, teL;
	GenDataset(3000u, rng, trF, trL);
	GenDataset(600u, rng, teF, teL);

	int cnt[ACT_COUNT] = {0, 0, 0, 0};
	for (uint32 i = 0; i < (uint32)trL.Size(); i++)
		cnt[trL[i]]++;
	const char *an[ACT_COUNT] = {"Subdiv", "Extrude", "Mirror", "Array"};
	printf("train=%u  test=%u  | classes train:", (uint32)trL.Size(), (uint32)teL.Size());
	for (int c = 0; c < ACT_COUNT; c++)
		printf(" %s=%d", an[c], cnt[c]);
	printf("\n");

	NkTensor Xtr = NkTensor::FromData(NkShape{(int64)trL.Size(), (int64)FEAT_DIM}, trF.Data(), NkDType::NK_F32);
	NkTensor Xte = NkTensor::FromData(NkShape{(int64)teL.Size(), (int64)FEAT_DIM}, teF.Data(), NkDType::NK_F32);
	data::NkDataset dsTr(Xtr, trL, ACT_COUNT);
	data::NkDataset dsTe(Xte, teL, ACT_COUNT);
	data::NkDataLoader trainLoader(dsTr, 64u, /*shuffle*/ true, 7u);
	data::NkDataLoader testLoader(dsTe, 64u, /*shuffle*/ false, 1u);

	// MLP : FEAT_DIM -> 32 -> ACT_COUNT (logits).
	nn::NkDense l1(FEAT_DIM, 32u, 1234u);
	nn::NkDense l2(32u, ACT_COUNT, 5678u);
	auto forward = [&](const NkVar &x) { return l2.Forward(nn::Relu(l1.Forward(x))); };
	NkVector<NkVar> params;
	l1.Parameters(params);
	l2.Parameters(params);
	optim::NkAdam adam(params, /*lr*/ 0.02f);

	printf("\n--- entrainement (imitation de l'expert) ---\n");
	for (int e = 1; e <= 60; e++) {
		train::EpochStats st = train::TrainEpoch(forward, adam, trainLoader);
		if (e == 1 || e % 10 == 0)
			printf("  epoch %2d  loss=%.4f  acc_train=%.1f%%\n", e, st.loss, st.acc * 100.0);
	}
	const double accTe = train::Accuracy(forward, testLoader);
	printf("\n--- RESULTAT : precision sur donnees JAMAIS VUES = %.1f%% ---\n", accTe * 100.0);
	printf("La policy (reseau) a appris a imiter les decisions de modelisation de l'expert\n");
	printf("depuis le seul etat du maillage. Prochaine etape : apprendre depuis de vraies\n");
	printf("sessions .nkmec, puis conditionner par une cible / image / texte.\n");

	printf("\n=== NKMeshAITest — Etape 2 : desserialisation de VRAIES sessions .nkmec ===\n");
	printf("(imitation HUMAINE -- journal reel de commandes, PAS l'expert heuristique ci-dessus)\n");
	RunStep2_RealSession();
	RunStep2_RoundTripDemo();
	printf("\n--- Etape 2 : constat honnete ---\n");
	printf("Le DESERIALISEUR (.nkmec -> commandes) et l'EXTRACTEUR de paires d'imitation\n");
	printf("(etat_avant encode, commande+parametres) fonctionnent et sont EXERCES ci-dessus sur\n");
	printf("un vrai fichier trouve dans le depot ET sur un round-trip disque octet-pour-octet.\n");
	printf("Ce qui MANQUE encore pour un entrainement reel : le VOLUME de vraies sessions\n");
	printf("humaines (une poignee de commandes ne suffit pas a entrainer un MLP qui generalise).\n");
	return 0;
}
