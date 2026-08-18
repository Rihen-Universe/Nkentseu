// =============================================================================
// NkTensorGpuTest — valide le contexte GPU de NKTensor (NkTensorGpu) au runtime :
// buffers GPU, upload/download, kernel élémentaire (add) écrit en NkSL, matmul.
// =============================================================================
#include "NKTensor/NkTensorGpu.h"
#include "NKTensor/NkTensor.h"
#include "NKTensor/NkTensorOps.h"

#include "NKTime/NkChrono.h" // banc d'echelle : horloge monotone haute resolution

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

static const char *kAddNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) buffer BufC { float data[]; } C;
@binding(set=0, binding=3) uniform Params { uint count; } pc;

layout(local_size_x = 64) in;

@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < pc.count) {
        C.data[i] = A.data[i] + B.data[i];
    }
}
)NKSL";

static int g_ok = 0, g_fail = 0;

static void check(bool cond, const char *what) {
	printf("  [%s] %s\n", cond ? " OK " : "FAIL", what);
	if (cond)
		++g_ok;
	else
		++g_fail;
}

// =============================================================================
// BANC D'ECHELLE SUR `add` — separer un COUT FIXE d'un DEBIT
// =============================================================================
// Le profil par noyau du 15/08 mesurait `add` a 1,25-1,36 ms pour 15,1 Mo de
// trafic obligatoire, la ou 448 Go/s en demanderaient 34 µs — facteur ~40. Deux
// causes possibles que le profil ne separe PAS (grille, face 9 : mesurer juste
// sans pouvoir separer deux causes) :
//   - un cout FIXE par operation (descripteurs, tampon de commandes, soumission,
//     WaitIdle), auquel cas la taille ne change presque rien ;
//   - un DEBIT reel tres inferieur a la crete, auquel cas le temps est
//     proportionnel a la taille.
//
// L'instrument qui les separe n'est pas une mesure plus fine du meme point :
// c'est de FAIRE VARIER la taille et de regarder la PENTE. Modele :
//     t(N) = F + 12N / B      (12 octets par element : 2 lectures + 1 ecriture)
// Temps plat  -> F domine  -> le remede est de LANCER MOINS d'operations.
// Pente nette -> B domine  -> le remede est de deplacer MOINS d'octets.
//
// ⚠️ REGIMES COUVERTS (face 7 : un banc doit declarer ses regimes).
//   serie A — dispatch SEUL, tampons deja alloues et deja remplis ;
//   serie B — forme de PRODUCTION : `NkGpuAdd` alloue son tampon de sortie a
//             chaque appel, donc CreateBuffer + dispatch + DestroyBuffer.
// L'ecart A/B attribue directement la part de l'allocation, qui est le poste
// n°1 de l'ordre de bataille.
//
// ⚠️ Le noyau appele ici est le MEME que celui de production : `RunBinary("add",
// kAddNkSL, ...)` est litteralement l'appel que fait `NkGpuAdd` dans
// NkTensorGpu.cpp. Ce n'est donc pas une reconstruction du chemin (face 4), mais
// le chemin lui-meme.

struct BancPoint {
		int64 n;
		double minUs, medUs, moyUs;
};

static int cmpDouble(const void *a, const void *b) {
	const double x = *(const double *)a, y = *(const double *)b;
	return (x < y) ? -1 : (x > y) ? 1 : 0;
}

// Renvoie min / mediane / moyenne des R mesures, en microsecondes.
static BancPoint BancMesure(NkTensorGpu &gpu, int64 n, int reps, bool avecAlloc) {
	const nk_size octets = (nk_size)n * sizeof(float);
	uint64 ba = gpu.CreateBuffer(octets);
	uint64 bb = gpu.CreateBuffer(octets);
	uint64 bcFixe = avecAlloc ? 0 : gpu.CreateBuffer(octets);

	// Remplissage : un upload par tampon, HORS de la boucle chronometree.
	{
		float *tmp = (float *)malloc((size_t)octets);
		for (int64 i = 0; i < n; ++i)
			tmp[i] = (float)(i & 1023);
		gpu.Upload(ba, tmp, octets);
		gpu.Upload(bb, tmp, octets);
		free(tmp);
	}

	// Chauffe : le TOUT PREMIER appel compile le noyau et paie la relecture
	// « sortie non nulle ». Le mesurer melangerait un cout unique au cout par appel.
	for (int w = 0; w < 3; ++w) {
		uint64 bc = avecAlloc ? gpu.CreateBuffer(octets) : bcFixe;
		gpu.RunBinary("add", NkString(kAddNkSL), ba, bb, bc, (uint32)n);
		if (avecAlloc)
			gpu.DestroyBuffer(bc);
	}

	double *ech = (double *)malloc(sizeof(double) * (size_t)reps);
	for (int r = 0; r < reps; ++r) {
		const double t0 = NkChrono::Now().nanoseconds;
		uint64 bc = avecAlloc ? gpu.CreateBuffer(octets) : bcFixe;
		gpu.RunBinary("add", NkString(kAddNkSL), ba, bb, bc, (uint32)n);
		if (avecAlloc)
			gpu.DestroyBuffer(bc);
		ech[r] = (NkChrono::Now().nanoseconds - t0) / 1000.0; // µs
	}

	double somme = 0.0;
	for (int r = 0; r < reps; ++r)
		somme += ech[r];
	qsort(ech, (size_t)reps, sizeof(double), cmpDouble);

	BancPoint p;
	p.n = n;
	p.minUs = ech[0];
	p.medUs = ech[reps / 2];
	p.moyUs = somme / (double)reps;
	free(ech);

	gpu.DestroyBuffer(ba);
	gpu.DestroyBuffer(bb);
	if (!avecAlloc)
		gpu.DestroyBuffer(bcFixe);
	return p;
}

static void BancSerie(NkTensorGpu &gpu, const char *titre, bool avecAlloc, const int64 *tailles, int nT,
					  int reps) {
	printf("\n--- %s ---\n", titre);
	printf("  %12s %10s %10s %10s %10s %9s\n", "N", "Mo trafic", "min µs", "med µs", "moy µs", "Go/s");
	BancPoint pts[16];
	for (int i = 0; i < nT; ++i) {
		pts[i] = BancMesure(gpu, tailles[i], reps, avecAlloc);
		const double mo = 12.0 * (double)tailles[i] / 1.0e6;
		const double gos = (12.0 * (double)tailles[i]) / (pts[i].minUs * 1.0e-6) / 1.0e9;
		printf("  %12lld %10.2f %10.1f %10.1f %10.1f %9.2f\n", (long long)tailles[i], mo, pts[i].minUs,
			   pts[i].medUs, pts[i].moyUs, gos);
	}
	// Ajustement sur les DEUX EXTREMES : t = F + 12N/B. Deux points suffisent, et
	// les extremes sont ceux qui ecartent le plus les deux causes.
	if (nT >= 2) {
		const BancPoint &p0 = pts[0], &p1 = pts[nT - 1];
		const double dOctets = 12.0 * (double)(p1.n - p0.n);
		const double dSec = (p1.minUs - p0.minUs) * 1.0e-6;
		const double debit = (dSec > 0.0) ? (dOctets / dSec / 1.0e9) : 0.0;
		const double fixe = p0.minUs - (12.0 * (double)p0.n / (debit * 1.0e9)) * 1.0e6;
		printf("  => pente entre N=%lld et N=%lld : debit marginal %.1f Go/s, cout fixe %.1f µs\n",
			   (long long)p0.n, (long long)p1.n, debit, fixe);
	}
}

// ---- LEST : reproduire l'ETAT du peripherique pendant l'entrainement --------
// Le banc ci-dessus tourne sur un peripherique quasi vide : 3 tampons, ~450 Mo.
// L'entrainement, lui, tient ~9 316 allocations vivantes et ~6,6 Go de VRAM.
// Si le dispatch mesure 150 µs a vide et 1 270 µs en production, l'ECART est une
// propriete de l'ETAT, pas du noyau — et il faut savoir LAQUELLE des deux
// grandeurs le porte, parce que le remede n'est pas le meme :
//   beaucoup de TAMPONS  -> une reserve de tampons (poste n°1) le supprime ;
//   beaucoup de VRAM     -> aucune reserve n'y change rien, il faut moins de
//                           tenseurs vivants (ce qui est un autre chantier).
// Les tester ensemble ne repondrait pas (grille, face 9 : un instrument qui ne
// separe pas deux causes ne mesure ni l'une ni l'autre, il constate).
struct Lest {
		uint64 *ids;
		int64 n;
};

static Lest LestPoser(NkTensorGpu &gpu, int64 nTampons, nk_size octetsChacun) {
	Lest l;
	l.ids = (uint64 *)malloc(sizeof(uint64) * (size_t)nTampons);
	l.n = 0;
	for (int64 i = 0; i < nTampons; ++i) {
		uint64 id = gpu.CreateBuffer(octetsChacun);
		if (!id)
			break; // allocation refusee : on s'arrete et on DIT combien on a pose
		l.ids[l.n++] = id;
	}
	printf("  [lest] %lld tampons de %.1f Ko poses = %.2f Go (demande : %lld)\n", (long long)l.n,
		   (double)octetsChacun / 1024.0, (double)l.n * (double)octetsChacun / 1.0e9, (long long)nTampons);
	return l;
}

static void LestRetirer(NkTensorGpu &gpu, Lest &l) {
	for (int64 i = 0; i < l.n; ++i)
		gpu.DestroyBuffer(l.ids[i]);
	free(l.ids);
	l.ids = nullptr;
	l.n = 0;
}

static int BancAdd(NkTensorGpu &gpu, int passes) {
	// Le point de PRODUCTION est 1 258 291 elements : 15,1 Mo de trafic par appel,
	// exactement la ligne `add` du profil du 15/08.
	static const int64 tailles[] = {1024, 16384, 262144, 1258291, 4194304, 12582912};
	const int nT = (int)(sizeof(tailles) / sizeof(tailles[0]));
	const int reps = 20;

	printf("\n=== BANC D'ECHELLE `add` — cout fixe contre debit ===\n");
	printf("Modele : t(N) = F + 12N/B. Crete memoire de la carte : ~448 Go/s.\n");
	printf("Point de production : N = 1 258 291 (15,1 Mo de trafic), mesure a 1250-1360 µs le 15/08.\n");
	printf("Repetitions par point : %d (3 de chauffe jetees). Passes : %d (temoin).\n", reps, passes);

	for (int pass = 0; pass < passes; ++pass) {
		printf("\n########## PASSE %d / %d ##########\n", pass + 1, passes);
		BancSerie(gpu, "SERIE A — dispatch seul (tampons preexistants)", false, tailles, nT, reps);
		BancSerie(gpu, "SERIE B — forme production (CreateBuffer + dispatch + DestroyBuffer)", true, tailles,
				  nT, reps);

		// SERIE C — le meme dispatch seul, mais sous l'ETAT de l'entrainement.
		// Trois lests, pour separer le NOMBRE de tampons de la QUANTITE de VRAM.
		{
			printf("\n  ..... C1 : BEAUCOUP de tampons, PEU de VRAM .....\n");
			Lest l = LestPoser(gpu, 9316, 4096); // ~38 Mo
			BancSerie(gpu, "SERIE C1 — dispatch seul, 9 316 tampons vivants (~38 Mo)", false, tailles, nT,
					  reps);
			LestRetirer(gpu, l);
		}
		{
			printf("\n  ..... C2 : PEU de tampons, BEAUCOUP de VRAM .....\n");
			Lest l = LestPoser(gpu, 11, 500ull * 1000ull * 1000ull); // ~5,5 Go
			BancSerie(gpu, "SERIE C2 — dispatch seul, 11 tampons vivants (~5,5 Go)", false, tailles, nT, reps);
			LestRetirer(gpu, l);
		}
		// SERIE D — la MEME forme production, mais vue par l'instrument DE LA
		// PRODUCTION. C'est le seul moyen de savoir OU le cout de l'allocation est
		// FACTURE. Hypothese a refuter : `CreateBuffer` rend la main avant que le
		// pilote ait engage la memoire, et l'attente reelle tombe dans le
		// `WaitIdle` du dispatch — donc sur la ligne `add`, pas sur `~alloc`.
		// Si c'est vrai, la ligne `add` du profil du 15/08 (10,5 % du pas) mesure
		// en grande partie de l'ALLOCATION, et le poste n°1 vaut bien plus que ses
		// 17,8-22,0 % annonces.
		{
			printf("\n  ..... D : forme production, vue par l'instrument de production .....\n");
			const int64 nProd = 1258291;
			const int repsD = 60;
			NkTensorGpu::ProfilRaz(true);
			const double t0 = NkChrono::Now().nanoseconds;
			BancMesure(gpu, nProd, repsD, /*avecAlloc*/ true);
			const double sec = (NkChrono::Now().nanoseconds - t0) / 1.0e9;
			NkTensorGpu::ProfilRapport(sec, 1);
			NkTensorGpu::ProfilRaz(false);
			printf("  (fenetre D : %.3f s murales pour %d appels + 3 de chauffe, N = %lld)\n", sec, repsD,
				   (long long)nProd);
		}

		{
			printf("\n  ..... C3 : les DEUX, forme de l'entrainement .....\n");
			Lest l = LestPoser(gpu, 9316, 590000); // ~5,5 Go en 9 316 tampons
			BancSerie(gpu, "SERIE C3 — dispatch seul, 9 316 tampons vivants (~5,5 Go)", false, tailles, nT,
					  reps);
			LestRetirer(gpu, l);
		}
	}
	return 0;
}

// ============================================================================
// BANC D'ECHELLE `matmul_t4` — LA VARIANCE D'ABORD, LE DEBIT ENSUITE
// ============================================================================
// ⚠️ POURQUOI CE BANC COMMENCE PAR LE BRUIT, ET NON PAR LE DEBIT.
// `matmul_t4` a varie d'un facteur ~1,8 entre deux executions du MEME binaire
// (ROADMAP, 15-16/08), et ce n'est pas la machine : le banc `add` se reproduit
// a mieux de 15 % sur trois executions. L'instabilite est donc PROPRE a ce
// noyau. Tant qu'elle n'est pas bornee, aucun chiffre de debit sur ce noyau ne
// vaut rien — on ne saurait pas si un ecart mesure est un effet ou du bruit
// (grille, face 6 : mesurer un ecart sans avoir mesure le bruit).
// C'est pourquoi chaque point rapporte MIN / MED / MOY / MAX et le rapport
// max/min : le plancher sous lequel on ne conclura pas.
//
// ⚠️ REGIMES COUVERTS (face 7 : un banc doit declarer ses regimes).
//   serie A  — dispatch SEUL, tampons deja alloues et deja remplis ;
//   serie B  — forme de PRODUCTION : allocation du tampon de sortie a chaque
//              appel (CreateBuffer + dispatch + DestroyBuffer) ;
//   serie C1 — dispatch seul, BEAUCOUP de tampons vivants, PEU de VRAM ;
//   serie C2 — dispatch seul, PEU de tampons, BEAUCOUP de VRAM.
// C1/C2 separent le NOMBRE de tampons de la QUANTITE de VRAM, parce que le
// remede n'est pas le meme (face 9 : un instrument qui ne separe pas deux
// causes ne mesure ni l'une ni l'autre, il constate).
//
// ⚠️ CE QUE CE BANC NE COUVRE PAS, et il faut le lire AVEC le resultat :
//   - il mesure UN noyau isole, PAS le debit d'entrainement. Le pas de
//     production enchaine 28 119 operations ; un chiffre de matmul seul ne dit
//     rien du temps total, et la ROADMAP montre deja que l'essentiel du temps
//     attribue a un noyau n'est PAS du temps GPU ;
//   - le plafond de `matmul_t4` SEUL est chiffre x1,11 a x1,19 en ROADMAP :
//     meme parfait, ce noyau ne decide pas du run ;
//   - horloge MURALE cote hote (NkChrono), pas d'horodatage GPU : le temps
//     inclut le cout de lancement et l'attente de synchronisation.
//
// ⚠️ Le chemin appele est celui de PRODUCTION : `RunMatMul` est litteralement
// ce qu'appelle `ops::Matmul`, et le choix `matmul_t4` contre `matmul` se fait
// DANS RunMatMul sur M*N >= 65536 && K >= 16. Toutes les tailles ci-dessous
// respectent ce seuil : on mesure donc bien `matmul_t4`, pas le naif.
//
// Modele : t = F + travail / debit_effectif.
//   FLOPs = 2*M*N*K
//   trafic REEL du pave 4x4 : chaque tuile 4x4 lit 4 lignes de A et 4 colonnes
//   de B, soit 8*K flottants (32*K octets) pour 16 sorties et 32*K FLOPs
//   -> intensite arithmetique = 1 FLOP/octet EXACTEMENT.
//   A ~448 Go/s, le plafond de ce noyau serait donc ~448 GFLOP/s, soit ~2,7 %
//   de la crete de 16 600 GFLOPS (RTX 3070 **Laptop** ; le 20 300 utilise
//   jusqu'au 2026-08-19 etait celui d'une carte de BUREAU, ~22 % trop haut).
//
// ⚠️ CE PLAFOND A ETE DEPASSE PAR LA MESURE — ET C'EST LE MODELE QUI A TORT.
//   Mesure du 2026-08-16 sur `1536x32769x640` : **1 066 GFLOP/s**, soit 2,4x
//   au-dessus des 448 GFLOP/s calcules ci-dessus. Le calcul suppose que CHAQUE
//   relecture atteint la DRAM ; en pratique le **cache L2 sert une large part**
//   des colonnes de B relues entre groupes de travail.
//   -> « intensite = 1 FLOP/octet » decrit le trafic EMIS par le noyau, pas
//      celui qui atteint la DRAM. Seul le second fixe un plafond, et on ne le
//      connait pas sans compteurs materiels.
//   -> Donc NE PAS conclure « un point proche du plafond tourne au maximum de
//      ce que son intensite permet » : c'etait ecrit ici, et c'est faux. Le
//      noyau est a 15,6x de son plancher CALCUL (3,88 ms contre 60,43 ms
//      mesurees) et 2,4x SOUS son plafond MEMOIRE sans cache. Les deux
//      ressources restent candidates ; ce banc ne les separe pas.
//   -> Ce qui les separerait : des compteurs DRAM (Nsight), ou une serie a FLOP
//      constant et intensite variable. Tant que ce n'est pas fait, aucune
//      conclusion sur « il n'y a rien a y gagner ».

struct MatPoint {
		uint32 M, N, K;
		double minUs, medUs, moyUs, maxUs;
};

struct MatTaille {
		uint32 M, N, K;
		const char *quoi;
};

static MatPoint BancMatMesure(NkTensorGpu &gpu, uint32 M, uint32 N, uint32 K, int reps, bool avecAlloc) {
	const nk_size octA = (nk_size)M * (nk_size)K * sizeof(float);
	const nk_size octB = (nk_size)K * (nk_size)N * sizeof(float);
	const nk_size octC = (nk_size)M * (nk_size)N * sizeof(float);

	MatPoint p;
	p.M = M;
	p.N = N;
	p.K = K;
	p.minUs = p.medUs = p.moyUs = p.maxUs = 0.0;

	uint64 ba = gpu.CreateBuffer(octA);
	uint64 bb = gpu.CreateBuffer(octB);
	uint64 bcFixe = avecAlloc ? 0 : gpu.CreateBuffer(octC);
	if (!ba || !bb || (!avecAlloc && !bcFixe)) {
		// Allocation refusee : on le DIT, au lieu de rendre un zero qui passerait
		// pour une mesure (face 2 : reussir pour la mauvaise raison).
		printf("  [!] allocation refusee pour M=%u N=%u K=%u — point NON mesure\n", M, N, K);
		if (ba)
			gpu.DestroyBuffer(ba);
		if (bb)
			gpu.DestroyBuffer(bb);
		if (bcFixe)
			gpu.DestroyBuffer(bcFixe);
		return p;
	}

	// Remplissage HORS de la boucle chronometree.
	{
		const nk_size nA = (nk_size)M * (nk_size)K, nB = (nk_size)K * (nk_size)N;
		float *tmp = (float *)malloc((size_t)(octA > octB ? octA : octB));
		for (nk_size i = 0; i < nA; ++i)
			tmp[i] = (float)((int64)(i & 255) - 128) * 0.01f;
		gpu.Upload(ba, tmp, octA);
		for (nk_size i = 0; i < nB; ++i)
			tmp[i] = (float)((int64)(i & 127) - 64) * 0.01f;
		gpu.Upload(bb, tmp, octB);
		free(tmp);
	}

	// Chauffe : le premier appel compile le noyau. Le mesurer melangerait un cout
	// unique au cout par appel.
	for (int w = 0; w < 3; ++w) {
		uint64 bc = avecAlloc ? gpu.CreateBuffer(octC) : bcFixe;
		if (bc)
			gpu.RunMatMul(ba, bb, bc, M, N, K);
		if (avecAlloc && bc)
			gpu.DestroyBuffer(bc);
	}

	double *ech = (double *)malloc(sizeof(double) * (size_t)reps);
	for (int r = 0; r < reps; ++r) {
		const double t0 = NkChrono::Now().nanoseconds;
		uint64 bc = avecAlloc ? gpu.CreateBuffer(octC) : bcFixe;
		if (bc)
			gpu.RunMatMul(ba, bb, bc, M, N, K);
		if (avecAlloc && bc)
			gpu.DestroyBuffer(bc);
		ech[r] = (NkChrono::Now().nanoseconds - t0) / 1000.0; // µs
	}

	double somme = 0.0;
	for (int r = 0; r < reps; ++r)
		somme += ech[r];
	qsort(ech, (size_t)reps, sizeof(double), cmpDouble);

	p.minUs = ech[0];
	p.medUs = ech[reps / 2];
	p.moyUs = somme / (double)reps;
	p.maxUs = ech[reps - 1];
	free(ech);

	gpu.DestroyBuffer(ba);
	gpu.DestroyBuffer(bb);
	if (!avecAlloc)
		gpu.DestroyBuffer(bcFixe);
	return p;
}

static void BancMatSerie(NkTensorGpu &gpu, const char *titre, bool avecAlloc, const MatTaille *t, int nT,
						 int reps) {
	printf("\n--- %s ---\n", titre);
	printf("  %-20s %9s %9s %9s %9s %8s %9s %8s  %s\n", "M x N x K", "min us", "med us", "moy us", "max us",
		   "max/min", "GFLOP/s", "% crete", "quoi");
	for (int i = 0; i < nT; ++i) {
		MatPoint p = BancMatMesure(gpu, t[i].M, t[i].N, t[i].K, reps, avecAlloc);
		if (p.minUs <= 0.0)
			continue;
		char forme[64];
		snprintf(forme, sizeof(forme), "%ux%ux%u", t[i].M, t[i].N, t[i].K);
		const double flops = 2.0 * (double)t[i].M * (double)t[i].N * (double)t[i].K;
		const double gflops = flops / (p.minUs * 1.0e-6) / 1.0e9;
		// RTX 3070 LAPTOP. Le 20 300 employe jusqu'au 2026-08-19 etait la crete
		// d'une carte de BUREAU : il sous-estimait tous les « % de crete » de ~22 %.
		const double pctCrete = gflops / 16600.0 * 100.0;
		const double ratio = p.maxUs / p.minUs;
		printf("  %-20s %9.1f %9.1f %9.1f %9.1f %8.2f %9.1f %8.3f  %s\n", forme, p.minUs, p.medUs, p.moyUs,
			   p.maxUs, ratio, gflops, pctCrete, t[i].quoi);
	}
}

static int BancMatmul(NkTensorGpu &gpu, int passes) {
	// Tailles : de petit a la PRODUCTION. Les formes de production viennent du
	// montage d'entrainement `--d 640 --layers 10 --heads 8 --T 256 --B 6` :
	// 6*256 = 1536 lignes, d = 640, vocabulaire 32 769.
	// Toutes respectent M*N >= 65536 et K >= 16, donc toutes passent par t4.
	static const MatTaille tailles[] = {
		{128, 512, 64, "petit"},		 {256, 512, 128, "petit"},
		{512, 512, 256, "moyen"},		 {1536, 640, 640, "PROD projection"},
		{1536, 2560, 640, "PROD mlp"}, {1536, 32769, 640, "PROD logits"},
	};
	const int nT = (int)(sizeof(tailles) / sizeof(tailles[0]));
	const int reps = 15;

	printf("\n=== BANC D'ECHELLE `matmul_t4` — LA VARIANCE D'ABORD ===\n");
	printf("Intensite arithmetique du pave 4x4 : 1 FLOP/octet EXACTEMENT.\n");
	printf("Plafond MODELE de ce noyau = ~448 Go/s x 1 = ~448 GFLOP/s = ~2,7 %% de la crete (16 600 GFLOPS, 3070 Laptop).\n");
	printf("ATTENTION : ce plafond a DEJA ete depasse — 1 066 GFLOP/s mesures le 16/08, soit 2,4x au-dessus.\n");
	printf("Le L2 sert une part des relectures, donc l'intensite 1 FLOP/o decrit le trafic EMIS, pas celui qui\n");
	printf("atteint la DRAM. Ne PAS lire un point proche de 2,7 %% comme  au maximum de son intensite  :\n");
	printf("ce banc ne separe pas la borne calcul de la borne memoire.\n");
	printf("Repetitions par point : %d (3 de chauffe jetees). Passes : %d (TEMOIN).\n", reps, passes);
	printf("⚠️ Horloge MURALE hote : inclut lancement et synchronisation, pas seulement le GPU.\n");
	printf("⚠️ Ce banc ne dit RIEN du debit d'entrainement : il mesure un noyau isole.\n");

	for (int pass = 0; pass < passes; ++pass) {
		printf("\n########## PASSE %d / %d ##########\n", pass + 1, passes);
		BancMatSerie(gpu, "SERIE A — dispatch seul (tampons preexistants)", false, tailles, nT, reps);
		BancMatSerie(gpu, "SERIE B — forme production (CreateBuffer + dispatch + DestroyBuffer)", true,
					 tailles, nT, reps);
		{
			printf("\n  ..... C1 : BEAUCOUP de tampons, PEU de VRAM .....\n");
			Lest l = LestPoser(gpu, 9316, 4096);
			BancMatSerie(gpu, "SERIE C1 — dispatch seul, 9 316 tampons vivants (~38 Mo)", false, tailles, nT,
						 reps);
			LestRetirer(gpu, l);
		}
		{
			printf("\n  ..... C2 : PEU de tampons, BEAUCOUP de VRAM .....\n");
			Lest l = LestPoser(gpu, 8, 500ull * 1000ull * 1000ull);
			BancMatSerie(gpu, "SERIE C2 — dispatch seul, 8 tampons vivants (~4 Go)", false, tailles, nT,
						 reps);
			LestRetirer(gpu, l);
		}
	}
	return 0;
}

// ============================================================================
// BANC DE LA RESERVE DE TAMPONS (chantier n°2)
// ============================================================================
// ⚠️ UNE RESERVE EST UN CACHE, ET UN CACHE REPOND TOUJOURS. C'est la famille de
// defauts que ce depot paie depuis une semaine. Donc ce banc mesure DEUX choses
// dans cet ordre, et la seconde ne vaut rien sans la premiere :
//   1. LA JUSTESSE — la reserve sert-elle VRAIMENT, et que rend-elle ?
//      Compteurs `servis` / `neufs` : leur somme doit egaler le nombre d'appels.
//      Si `servis == 0`, tout gain affiche serait une illusion.
//   2. LE GAIN — serie B (forme production) avec et sans reserve.
//
// ⚠️ MEME BINAIRE pour les deux bras : `ReserveActive()` est un interrupteur.
// C'est ce qui a rendu defendable le x1,57 du chantier n°1 — aucun ecart de
// compilation ne peut se glisser entre LEGACY et NEUF.
//
// ⚠️ ORDRE ALTERNE. Alterner les modes ne suffit pas : la machine s'accelere au
// fil des courses (mesure du 16/08 : -25 % sur un bras non modifie). Chaque mode
// occupe donc les deux positions.

static void ReserveEtat(const char *quand) {
	printf("  [temoin %s] servis=%lld  neufs=%lld  retenus=%lld tampons (%.1f Mo)  evictions=%lld\n", quand,
		   (long long)NkTensorGpu::ReserveServis(), (long long)NkTensorGpu::ReserveNeufs(),
		   (long long)NkTensorGpu::ReserveTamponsRetenus(),
		   (double)NkTensorGpu::ReserveOctetsRetenus() / 1.0e6, (long long)NkTensorGpu::ReserveEvictions());
}

// TEMOIN DE JUSTESSE : que rend exactement un tampon recycle ?
// Ce cas existe parce qu'un tampon recycle porte les DONNEES DE SON PRECEDENT
// USAGE. Si un appelant comptait sur un tampon neuf implicitement nul, la
// reserve produirait un resultat faux EN SILENCE. On le montre, puis on montre
// que le zerotage EXPLICITE (chantier n°1) est ce qui rend le recyclage sur.
static int ReserveTemoinJustesse(NkTensorGpu &gpu) {
	printf("\n=== TEMOIN DE JUSTESSE — que rend un tampon RECYCLE ? ===\n");
	int echecs = 0;
	const nk_size n = 4096, oct = n * sizeof(float);

	NkTensorGpu::ReserveVider();
	NkTensorGpu::ReserveActive(true);
	NkTensorGpu::ReserveRazCompteurs();

	// 1) un tampon, rempli d'un motif NON NUL, puis rendu a la reserve.
	uint64 b1 = gpu.CreateBuffer(oct);
	{
		float *m = (float *)malloc(oct);
		for (nk_size i = 0; i < n; ++i)
			m[i] = 123.5f;
		gpu.Upload(b1, m, oct);
		free(m);
	}
	gpu.DestroyBuffer(b1);
	printf("  1) tampon rempli de 123.5 puis rendu a la reserve\n");

	// 2) un tampon de MEME TAILLE : il DOIT venir de la reserve.
	const int64 servisAvant = NkTensorGpu::ReserveServis();
	uint64 b2 = gpu.CreateBuffer(oct);
	const bool vientDeLaReserve = (NkTensorGpu::ReserveServis() == servisAvant + 1);
	printf("  2) nouveau tampon de meme taille -> %s\n",
		   vientDeLaReserve ? "SERVI PAR LA RESERVE (compteur +1)" : "alloue a neuf (compteur inchange)");
	if (!vientDeLaReserve) {
		printf("  [ KO ] la reserve n'a PAS servi : tout gain mesure ensuite serait une illusion\n");
		++echecs;
	} else {
		printf("  [ OK ] la reserve sert reellement — le compteur le prouve\n");
	}

	// 3) ce qu'il CONTIENT : la preuve que le recyclage rend des donnees remanentes.
	{
		float *v = (float *)malloc(oct);
		for (nk_size i = 0; i < n; ++i)
			v[i] = -1.0f;
		gpu.Download(b2, v, oct);
		const bool remanent = (v[0] == 123.5f);
		printf("  3) contenu du tampon recycle : v[0] = %.1f -> %s\n", (double)v[0],
			   remanent ? "REMANENT (donnees du precedent usage)" : "non remanent");
		if (remanent)
			printf("      ⚠️ c'est le RISQUE de toute reserve : un appelant qui compterait sur un\n"
				   "         tampon neuf implicitement nul obtiendrait un resultat FAUX en silence.\n");

		// 4) le zerotage EXPLICITE est ce qui rend le recyclage sur.
		if (gpu.Clear(b2, oct, 0)) {
			for (nk_size i = 0; i < n; ++i)
				v[i] = -1.0f;
			gpu.Download(b2, v, oct);
			const bool nul = (v[0] == 0.0f && v[n - 1] == 0.0f);
			printf("  4) apres Clear() explicite : v[0] = %.1f, v[n-1] = %.1f -> %s\n", (double)v[0],
				   (double)v[n - 1], nul ? "ZEROS" : "PAS zeros");
			if (!nul) {
				printf("  [ KO ] le zerotage explicite ne nettoie pas un tampon recycle\n");
				++echecs;
			} else {
				printf("  [ OK ] `NkGpuZeros` remet a zero APRES CreateBuffer : le recyclage est SUR\n"
					   "         sur ce chemin. C'est le chantier n°1 qui a rendu ce zerotage explicite.\n");
			}
		}
		free(v);
	}
	gpu.DestroyBuffer(b2);

	// 5) l'instrument est-il juste ? servis + neufs doit egaler le nombre d'appels.
	const int64 s = NkTensorGpu::ReserveServis(), nf = NkTensorGpu::ReserveNeufs();
	printf("  5) instrument : servis=%lld + neufs=%lld = %lld pour 2 appels a CreateBuffer -> %s\n",
		   (long long)s, (long long)nf, (long long)(s + nf), (s + nf == 2) ? "COHERENT" : "INCOHERENT");
	if (s + nf != 2) {
		printf("  [ KO ] le compteur ne couvre pas tous les appels : aucun gain ne serait lisible\n");
		++echecs;
	}

	NkTensorGpu::ReserveActive(false);
	printf("\n  => temoin de justesse : %d echec(s)\n", echecs);
	return echecs;
}

static int BancReserve(NkTensorGpu &gpu) {
	if (ReserveTemoinJustesse(gpu) != 0) {
		printf("\n⚠️ LE TEMOIN DE JUSTESSE A ECHOUE — on ne mesure PAS le gain.\n"
			   "   Mesurer la vitesse d'un cache dont on n'a pas prouve qu'il sert est\n"
			   "   exactement le piege que ce banc existe pour eviter.\n");
		return 1;
	}

	static const MatTaille tailles[] = {
		{128, 512, 64, "petit"},
		{256, 512, 128, "petit"},
		{512, 512, 256, "moyen"},
		{1536, 640, 640, "PROD projection"},
	};
	const int nT = (int)(sizeof(tailles) / sizeof(tailles[0]));
	const int reps = 15;

	printf("\n=== GAIN DE LA RESERVE — serie B (forme production) ===\n");
	printf("La serie B alloue le tampon de sortie a chaque appel : c'est LA ou vit le\n");
	printf("cout d'allocation mesure a +427 a +492 us (banc `add` ET banc `matmul`).\n");
	printf("Ordre ALTERNE : la machine s'accelere au fil des courses.\n");

	for (int tour = 0; tour < 2; ++tour) {
		const bool neufDabord = (tour == 1);
		printf("\n########## TOUR %d / 2 — %s en premier ##########\n", tour + 1,
			   neufDabord ? "NEUF" : "LEGACY");
		for (int bras = 0; bras < 2; ++bras) {
			const bool actif = neufDabord ? (bras == 0) : (bras == 1);
			NkTensorGpu::ReserveVider();
			NkTensorGpu::ReserveActive(actif);
			NkTensorGpu::ReserveRazCompteurs();
			BancMatSerie(gpu, actif ? "RESERVE ACTIVE (NEUF)" : "RESERVE INACTIVE (LEGACY)", true, tailles,
						 nT, reps);
			ReserveEtat(actif ? "NEUF" : "LEGACY");
			if (actif && NkTensorGpu::ReserveServis() == 0) {
				printf("  ⚠️ servis = 0 alors que la reserve est ACTIVE : le gain affiche ne\n"
					   "     viendrait PAS de la reserve. Resultat a jeter.\n");
			}
		}
	}
	NkTensorGpu::ReserveActive(false);
	return 0;
}

int main(int argc, char **argv) {
	bool bancAdd = false;
	bool bancMat = false;
	bool bancRes = false;
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--banc-add") == 0)
			bancAdd = true;
		if (strcmp(argv[i], "--banc-matmul") == 0)
			bancMat = true;
		if (strcmp(argv[i], "--banc-reserve") == 0)
			bancRes = true;
	}

	printf("=== NkTensorGpuTest ===\n");
	NkTensorGpu &gpu = NkTensorGpu::Get();
	printf("GPU disponible : %d  (backend: %s)\n", gpu.IsAvailable(), gpu.BackendName());
	if (!gpu.IsAvailable()) {
		printf("Pas de GPU compute -> test ignoré.\n");
		return 0;
	}

	if (bancAdd) {
		// Deux passes DANS le meme processus : c'est le temoin (face 6 de la
		// grille). Sans lui, aucune echelle ne dit si un ecart entre deux tailles
		// est un effet ou du bruit — et `matmul_t4` a deja varie d'un facteur 1,8
		// entre deux executions du meme binaire.
		int r = BancAdd(gpu, 2);
		gpu.Shutdown();
		return r;
	}

	if (bancMat) {
		// Deux passes DANS le meme processus, meme raison que pour `add` : c'est le
		// TEMOIN. Ici il est le sujet meme du banc — la variance de ~1,8x de
		// `matmul_t4` entre executions est ce qu'on vient borner.
		int r = BancMatmul(gpu, 2);
		gpu.Shutdown();
		return r;
	}

	if (bancRes) {
		int r = BancReserve(gpu);
		gpu.Shutdown();
		return r;
	}

	// ---- 0) ClearBuffer : la primitive fait-elle VRAIMENT quelque chose ? -------
	// ⚠️ Ce cas existe parce que `NkICommandBuffer::ClearBuffer` a vecu des mois
	// declare sur les six backends et implemente sur AUCUN : corps vide, zero
	// surcharge, deux appelants convaincus du contraire. Une signature ne prouve
	// rien. Ce test ecrit un motif non nul, appelle la remise a zero, et RELIT.
	{
		const uint32 N = 64;
		uint32 motif[N], relu[N];
		for (uint32 i = 0; i < N; i++) {
			motif[i] = 0xDEADBEEFu;
			relu[i] = 0xFFFFFFFFu;
		}
		uint64 b = gpu.CreateBuffer(N * sizeof(uint32));
		gpu.Upload(b, motif, N * sizeof(uint32));
		const bool efface = gpu.Clear(b, N * sizeof(uint32), 0);
		gpu.Download(b, relu, N * sizeof(uint32));
		bool zeros = efface;
		for (uint32 i = 0; i < N; i++)
			if (relu[i] != 0u)
				zeros = false;
		printf("  clear: relu[0..3]=[%08X %08X %08X %08X] (attendu 00000000 x4), backend=%s\n", relu[0], relu[1],
			   relu[2], relu[3], gpu.BackendName());
		check(zeros, "ClearBuffer met REELLEMENT le tampon a zero (temoin ecriture/relecture)");
		check(NkTensorGpu::ClearDisponible() == zeros, "ClearDisponible() dit la meme chose que le temoin");
		gpu.DestroyBuffer(b);

		// Et le tenseur de zeros GPU, qui est ce qui interesse l'entrainement.
		NkTensor z = NkGpuZeros(NkShape{4, 5});
		bool zok = z.IsValid() && z.Device() == NkDevice::NK_GPU;
		if (zok) {
			NkTensor c = z.ToCPU();
			const float *p = c.DataAs<float>();
			for (int i = 0; i < 20 && zok; i++)
				if (p[i] != 0.f)
					zok = false;
		}
		check(zok, "NkGpuZeros([4,5]) : tenseur GPU valide et rempli de zeros, SANS upload");

		// Le parametre `device` des fabriques CPU doit echouer BRUYAMMENT, pas mentir.
		NkTensor piege = NkTensor::Zeros(NkShape{4}, NkDType::NK_F32, NkDevice::NK_GPU);
		check(!piege.IsValid(), "NkTensor::Zeros(..., NK_GPU) refuse au lieu de rendre un tenseur qui ment");
	}

	// ---- 1) Élémentaire : C = A + B --------------------------------------------
	{
		const uint32 N = 100;
		float a[N], b[N], c[N] = {0};
		for (uint32 i = 0; i < N; i++) {
			a[i] = (float)i;
			b[i] = (float)(2 * i);
		}
		uint64 ba = gpu.CreateBuffer(N * sizeof(float));
		uint64 bb = gpu.CreateBuffer(N * sizeof(float));
		uint64 bc = gpu.CreateBuffer(N * sizeof(float));
		gpu.Upload(ba, a, N * sizeof(float));
		gpu.Upload(bb, b, N * sizeof(float));
		bool ran = gpu.RunBinary("add", NkString(kAddNkSL), ba, bb, bc, N);
		gpu.Download(bc, c, N * sizeof(float));
		bool ok = ran;
		for (uint32 i = 0; i < N; i++)
			if (fabs(c[i] - (a[i] + b[i])) > 1e-4f)
				ok = false;
		printf("  add: C[0..4]=[%.0f %.0f %.0f %.0f %.0f] (attendu 0 3 6 9 12)\n", c[0], c[1], c[2], c[3], c[4]);
		check(ok, "add elementwise (N=100) sur GPU == CPU");
		gpu.DestroyBuffer(ba);
		gpu.DestroyBuffer(bb);
		gpu.DestroyBuffer(bc);
	}

	// ---- 2) MatMul : C[2x2] = A[2x3] · B[3x2] = [[58,64],[139,154]] -------------
	{
		float a[6] = {1, 2, 3, 4, 5, 6};
		float b[6] = {7, 8, 9, 10, 11, 12};
		float c[4] = {0, 0, 0, 0};
		uint64 ba = gpu.CreateBuffer(6 * sizeof(float));
		uint64 bb = gpu.CreateBuffer(6 * sizeof(float));
		uint64 bc = gpu.CreateBuffer(4 * sizeof(float));
		gpu.Upload(ba, a, 6 * sizeof(float));
		gpu.Upload(bb, b, 6 * sizeof(float));
		bool ran = gpu.RunMatMul(ba, bb, bc, 2, 2, 3); // M=2,N=2,K=3
		gpu.Download(bc, c, 4 * sizeof(float));
		printf("  matmul: C=[%.0f %.0f %.0f %.0f] (attendu 58 64 139 154)\n", c[0], c[1], c[2], c[3]);
		bool ok = ran && fabs(c[0] - 58) < 0.5f && fabs(c[1] - 64) < 0.5f && fabs(c[2] - 139) < 0.5f &&
				  fabs(c[3] - 154) < 0.5f;
		check(ok, "matmul 2x3 * 3x2 sur GPU");
		gpu.DestroyBuffer(ba);
		gpu.DestroyBuffer(bb);
		gpu.DestroyBuffer(bc);
	}

	// ---- 3) Intégration tenseur : roundtrip CPU -> GPU -> CPU ------------------
	{
		NkTensor cpu = NkTensor::Arange(0.0, 12.0); // [0,1,...,11]
		NkTensor g = cpu.ToGPU();
		NkTensor back = g.ToCPU();
		bool ok = g.IsValid() && back.IsValid() && g.Device() == NkDevice::NK_GPU &&
				  back.Device() == NkDevice::NK_CPU && back.Numel() == 12;
		if (ok) {
			const float *r = back.DataAs<float>();
			const float *o = cpu.DataAs<float>();
			for (int i = 0; i < 12; i++)
				if (fabs(r[i] - o[i]) > 1e-4f)
					ok = false;
		}
		printf("  roundtrip: back[0..3]=[%.0f %.0f %.0f %.0f] (attendu 0 1 2 3)\n", back.DataAs<float>()[0],
			   back.DataAs<float>()[1], back.DataAs<float>()[2], back.DataAs<float>()[3]);
		check(ok, "NkTensor CPU -> ToGPU -> ToCPU preserve les donnees");
	}

	// ---- 4) API UNIFIÉE : ops::Matmul dispatché automatiquement sur GPU --------
	{
		float av[6] = {1, 2, 3, 4, 5, 6};
		float bv[6] = {7, 8, 9, 10, 11, 12};
		NkShape sa;
		sa.PushBack(2);
		sa.PushBack(3);
		NkShape sb;
		sb.PushBack(3);
		sb.PushBack(2);
		NkTensor a = NkTensor::FromData(sa, av, NkDType::NK_F32).ToGPU();
		NkTensor b = NkTensor::FromData(sb, bv, NkDType::NK_F32).ToGPU();
		NkTensor c = ops::Matmul(a, b); // <- MÊME API que le CPU, routée GPU
		NkTensor cpu = c.ToCPU();
		const float *r = cpu.DataAs<float>();
		printf("  ops::Matmul(GPU) -> C=[%.0f %.0f %.0f %.0f] (attendu 58 64 139 154)\n", r[0], r[1], r[2], r[3]);
		bool ok = c.IsValid() && c.Device() == NkDevice::NK_GPU && fabs(r[0] - 58) < 0.5f && fabs(r[3] - 154) < 0.5f;
		check(ok, "ops::Matmul dispatché sur GPU (API unifiée)");
	}

	// ---- 4bis) GRAND produit de matrices : le noyau PAVÉ, contre le CPU --------
	// Au-delà d'un seuil (M·N >= 65536), `RunMatMul` bascule sur un noyau où
	// chaque fil calcule un bloc 4×4 — quatre fois moins de trafic mémoire. Les
	// petits cas ci-dessus ne l'exercent PAS : ils restent sous le seuil et
	// valident l'ancien noyau. Sans ce test-ci, une erreur dans le noyau pavé ne
	// se verrait que par une perte d'entraînement « un peu différente », ce qui
	// est indiscernable d'un simple effet d'arrondi.
	{
		// ⚠️ Dimensions NON multiples de 4, EXPRÈS : avec M et N divisibles par 4,
		// les tests de bornes du noyau pavé ne sont jamais empruntés — et c'est
		// justement là qu'une erreur se cache. À l'entraînement, le vocabulaire
		// vaut 16385 = 4×4096 + 1 : le dernier bloc n'a qu'UNE colonne valide.
		const int64 M = 301, K = 37, N = 259; // M·N = 77 959 > seuil, aucun n'est multiple de 4
		NkShape sa;
		sa.PushBack(M);
		sa.PushBack(K);
		NkShape sb;
		sb.PushBack(K);
		sb.PushBack(N);
		// Valeurs déterministes, non triviales, de magnitudes variées.
		NkTensor a = NkTensor::Zeros(sa);
		NkTensor b = NkTensor::Zeros(sb);
		{
			float *pa = a.DataAs<float>();
			for (int64 i = 0; i < M * K; ++i)
				pa[i] = (float)(((i * 37) % 19) - 9) * 0.125f;
			float *pb = b.DataAs<float>();
			for (int64 i = 0; i < K * N; ++i)
				pb[i] = (float)(((i * 53) % 23) - 11) * 0.0625f;
		}
		NkTensor refCpu = ops::Matmul(a, b);				  // oracle CPU
		NkTensor gpu = ops::Matmul(a.ToGPU(), b.ToGPU());	  // passe par matmul_t4
		NkTensor got = gpu.ToCPU().Contiguous();
		const float *pr = refCpu.Contiguous().DataAs<float>();
		const float *pg = got.DataAs<float>();
		double emax = 0.0;
		for (int64 i = 0; i < M * N; ++i) {
			const double e = fabs((double)pr[i] - (double)pg[i]);
			if (e > emax)
				emax = e;
		}
		printf("  grand matmul %lldx%lld * %lldx%lld : ecart max GPU vs CPU = %.3e\n", (long long)M, (long long)K,
			   (long long)K, (long long)N, emax);
		check(emax < 1e-3, "noyau matmul PAVE identique au CPU sur un grand produit");
	}

	// ---- 5) Ops ÉLÉMENTAIRES dispatchées sur GPU (résidence) : Sub/Mul/Relu/Sig/Tanh
	//         Même API ops:: que le CPU ; on compare au CPU de référence. -----------
	{
		const int64 N = 1024;
		NkTensor a = NkTensor::Arange(-512.0, 512.0); // [-512 .. 511], Numel=1024
		NkTensor b = NkTensor::Arange(0.0, 1024.0);	  // [0 .. 1023]
		// Référence CPU
		NkTensor rSub = ops::Sub(a, b);
		NkTensor rMul = ops::Mul(a, b);
		NkTensor rRel = ops::Relu(a);
		NkTensor rSig = ops::Sigmoid(a);
		NkTensor rTan = ops::Tanh(a);
		// Versions GPU (opérandes résidents GPU -> dispatch automatique)
		NkTensor ag = a.ToGPU(), bg = b.ToGPU();
		auto maxErr = [N](const NkTensor &gpuRes, const NkTensor &cpuRef) -> float {
			NkTensor back = gpuRes.ToCPU();
			if (!back.IsValid() || back.Numel() != N)
				return 1e9f;
			const float *r = back.DataAs<float>();
			const float *o = cpuRef.DataAs<float>();
			float e = 0.0f;
			for (int64 i = 0; i < N; i++) {
				float d = fabsf(r[i] - o[i]);
				if (d > e)
					e = d;
			}
			return e;
		};
		NkTensor gSub = ops::Sub(ag, bg);
		NkTensor gMul = ops::Mul(ag, bg);
		NkTensor gRel = ops::Relu(ag);
		NkTensor gSig = ops::Sigmoid(ag);
		NkTensor gTan = ops::Tanh(ag);
		float eSub = maxErr(gSub, rSub), eMul = maxErr(gMul, rMul);
		float eRel = maxErr(gRel, rRel), eSig = maxErr(gSig, rSig), eTan = maxErr(gTan, rTan);
		printf("  err max GPU vs CPU : sub=%.2e mul=%.2e relu=%.2e sigmoid=%.2e tanh=%.2e\n", eSub, eMul, eRel, eSig,
			   eTan);
		check(gSub.Device() == NkDevice::NK_GPU && eSub < 1e-3f, "ops::Sub  dispatché GPU == CPU");
		check(gMul.Device() == NkDevice::NK_GPU && eMul < 1e-1f, "ops::Mul  dispatché GPU == CPU");
		check(gRel.Device() == NkDevice::NK_GPU && eRel < 1e-3f, "ops::Relu dispatché GPU == CPU");
		check(gSig.Device() == NkDevice::NK_GPU && eSig < 1e-3f, "ops::Sigmoid dispatché GPU == CPU");
		check(gTan.Device() == NkDevice::NK_GPU && eTan < 1e-3f, "ops::Tanh dispatché GPU == CPU");
	}

	// ---- 6) RÉDUCTIONS dispatchées sur GPU : Sum/Mean/Max global + par axe -------
	{
		float m[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}; // [4,3] connu
		NkShape s;
		s.PushBack(4);
		s.PushBack(3);
		NkTensor cpu = NkTensor::FromData(s, m, NkDType::NK_F32);
		NkTensor g = cpu.ToGPU();
		auto scal = [](const NkTensor &t) -> float {
			NkTensor c = t.ToCPU();
			return c.IsValid() ? c.DataAs<float>()[0] : 1e9f;
		};
		auto errV = [](const NkTensor &gr, const NkTensor &cr) -> float {
			NkTensor b = gr.ToCPU();
			if (!b.IsValid() || b.Numel() != cr.Numel())
				return 1e9f;
			const float *x = b.DataAs<float>();
			const float *y = cr.DataAs<float>();
			float e = 0.0f;
			for (int64 i = 0; i < cr.Numel(); i++) {
				float d = fabsf(x[i] - y[i]);
				if (d > e)
					e = d;
			}
			return e;
		};
		// Global (attendus : somme=78, moyenne=6.5, max=12)
		check(fabsf(scal(ops::Sum(g)) - 78.0f) < 1e-3f, "ops::Sum  global GPU (=78)");
		check(fabsf(scal(ops::Mean(g)) - 6.5f) < 1e-3f, "ops::Mean global GPU (=6.5)");
		check(fabsf(scal(ops::Max(g)) - 12.0f) < 1e-3f, "ops::Max  global GPU (=12)");
		// Par axe : axe0 -> [3]=[22,26,30], axe1 -> [4]=[6,15,24,33]
		float e0s = errV(ops::Sum(g, 0), ops::Sum(cpu, 0));
		float e1s = errV(ops::Sum(g, 1), ops::Sum(cpu, 1));
		float e0m = errV(ops::Mean(g, 0), ops::Mean(cpu, 0));
		float e1x = errV(ops::Max(g, 1), ops::Max(cpu, 1));
		printf("  err reductions axe : sum0=%.2e sum1=%.2e mean0=%.2e max1=%.2e\n", e0s, e1s, e0m, e1x);
		check(ops::Sum(g, 0).Device() == NkDevice::NK_GPU && e0s < 1e-3f, "ops::Sum  axe0 dispatché GPU == CPU");
		check(e1s < 1e-3f, "ops::Sum  axe1 GPU == CPU");
		check(e0m < 1e-3f, "ops::Mean axe0 GPU == CPU");
		check(e1x < 1e-3f, "ops::Max  axe1 GPU == CPU");
	}

	// ---- 7) PERMUTE / TRANSPOSE sur GPU (gather par strides, résidence) ---------
	{
		// Transpose 2D : [3,4] -> [4,3]
		float m[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
		NkShape s;
		s.PushBack(3);
		s.PushBack(4);
		NkTensor cpu = NkTensor::FromData(s, m, NkDType::NK_F32);
		NkTensor cpuT = cpu.Transpose(0, 1).Contiguous();		// référence CPU
		NkTensor gT = cpu.ToGPU().Transpose(0, 1).Contiguous(); // gather GPU
		NkTensor back = gT.ToCPU();
		float e = 0.0f;
		const float *x = back.DataAs<float>();
		const float *y = cpuT.DataAs<float>();
		for (int64 i = 0; i < 12; i++) {
			float d = fabsf(x[i] - y[i]);
			if (d > e)
				e = d;
		}
		printf("  transpose GPU back[0..3]=[%.0f %.0f %.0f %.0f] (attendu 1 5 9 2)\n", x[0], x[1], x[2], x[3]);
		check(gT.Device() == NkDevice::NK_GPU && back.Numel() == 12 && e < 1e-4f,
			  "Transpose+Contiguous sur GPU == CPU");

		// Permute 3D : [2,3,4] -> ordre (2,0,1) = [4,2,3]
		float m3[24];
		for (int i = 0; i < 24; i++)
			m3[i] = (float)i;
		NkShape s3;
		s3.PushBack(2);
		s3.PushBack(3);
		s3.PushBack(4);
		NkTensor c3 = NkTensor::FromData(s3, m3, NkDType::NK_F32);
		NkShape ord;
		ord.PushBack(2);
		ord.PushBack(0);
		ord.PushBack(1);
		NkTensor refP = c3.Permute(ord).Contiguous();
		NkTensor gP = c3.ToGPU().Permute(ord).Contiguous();
		NkTensor bP = gP.ToCPU();
		float e2 = 0.0f;
		const float *xp = bP.DataAs<float>();
		const float *yp = refP.DataAs<float>();
		for (int64 i = 0; i < 24; i++) {
			float d = fabsf(xp[i] - yp[i]);
			if (d > e2)
				e2 = d;
		}
		check(gP.Device() == NkDevice::NK_GPU && bP.Numel() == 24 && e2 < 1e-4f,
			  "Permute 3D (2,0,1)+Contiguous sur GPU == CPU");
	}

	// ---- 8) im2col / col2im sur GPU (conv résidente) == référence CPU -----------
	{
		const int64 B = 1, Cin = 2, H = 3, W = 3, kH = 2, kW = 2, stride = 1, pad = 0;
		const int64 outH = 2, outW = 2, K = Cin * kH * kW, M = B * outH * outW;
		float xd[18];
		for (int i = 0; i < 18; i++)
			xd[i] = (float)(i + 1); // x [1,2,3,3]
		NkShape xs;
		xs.PushBack(1);
		xs.PushBack(2);
		xs.PushBack(3);
		xs.PushBack(3);
		NkTensor x = NkTensor::FromData(xs, xd, NkDType::NK_F32);

		// Référence CPU im2col (même formule que le noyau)
		float ref[32];
		for (int64 b = 0; b < B; b++)
			for (int64 oh = 0; oh < outH; oh++)
				for (int64 ow = 0; ow < outW; ow++) {
					int64 row = (b * outH + oh) * outW + ow;
					for (int64 ic = 0; ic < Cin; ic++)
						for (int64 ky = 0; ky < kH; ky++)
							for (int64 kx = 0; kx < kW; kx++) {
								int64 iy = oh * stride - pad + ky, ix = ow * stride - pad + kx,
									  kcol = (ic * kH + ky) * kW + kx;
								ref[row * K + kcol] = (iy >= 0 && iy < H && ix >= 0 && ix < W)
														  ? xd[((b * Cin + ic) * H + iy) * W + ix]
														  : 0.f;
							}
				}
		NkTensor gcol = NkGpuIm2Col(x.ToGPU(), kH, kW, stride, pad, outH, outW);
		NkTensor bcol = gcol.ToCPU();
		float e = 0;
		const float *c = bcol.DataAs<float>();
		for (int64 i = 0; i < M * K; i++) {
			float d = fabsf(c[i] - ref[i]);
			if (d > e)
				e = d;
		}
		check(gcol.Device() == NkDevice::NK_GPU && bcol.Numel() == M * K && e < 1e-4f, "im2col GPU == CPU");

		// Référence CPU col2im (accumulation)
		float refx[18];
		for (int i = 0; i < 18; i++)
			refx[i] = 0.f;
		for (int64 b = 0; b < B; b++)
			for (int64 oh = 0; oh < outH; oh++)
				for (int64 ow = 0; ow < outW; ow++) {
					int64 row = (b * outH + oh) * outW + ow;
					for (int64 ic = 0; ic < Cin; ic++)
						for (int64 ky = 0; ky < kH; ky++)
							for (int64 kx = 0; kx < kW; kx++) {
								int64 iy = oh * stride - pad + ky, ix = ow * stride - pad + kx;
								if (iy >= 0 && iy < H && ix >= 0 && ix < W) {
									int64 kcol = (ic * kH + ky) * kW + kx;
									refx[((b * Cin + ic) * H + iy) * W + ix] += ref[row * K + kcol];
								}
							}
				}
		NkTensor gdx = NkGpuCol2Im(gcol, B, Cin, H, W, kH, kW, stride, pad, outH, outW);
		NkTensor bdx = gdx.ToCPU();
		float e2 = 0;
		const float *dxp = bdx.DataAs<float>();
		for (int64 i = 0; i < B * Cin * H * W; i++) {
			float d = fabsf(dxp[i] - refx[i]);
			if (d > e2)
				e2 = d;
		}
		printf("  im2col err=%.2e  col2im err=%.2e\n", e, e2);
		check(gdx.Device() == NkDevice::NK_GPU && bdx.Numel() == 18 && e2 < 1e-4f, "col2im GPU == CPU");
	}

	// ---- 9) Softmax par ligne sur GPU == référence CPU -------------------------
	{
		float m[8] = {1, 2, 3, 4, 2, 1, 0, -1}; // [2,4]
		NkShape s;
		s.PushBack(2);
		s.PushBack(4);
		NkTensor x = NkTensor::FromData(s, m, NkDType::NK_F32);
		NkTensor gsm = NkGpuSoftmaxRows(x.ToGPU());
		NkTensor bsm = gsm.ToCPU();
		float ref[8];
		for (int r = 0; r < 2; r++) {
			const float *row = m + r * 4;
			float mx = row[0];
			for (int c = 1; c < 4; c++)
				if (row[c] > mx)
					mx = row[c];
			double sum = 0;
			float e[4];
			for (int c = 0; c < 4; c++) {
				e[c] = (float)std::exp((double)(row[c] - mx));
				sum += e[c];
			}
			for (int c = 0; c < 4; c++)
				ref[r * 4 + c] = (float)(e[c] / sum);
		}
		float es = 0;
		const float *p = bsm.DataAs<float>();
		for (int i = 0; i < 8; i++) {
			float d = fabsf(p[i] - ref[i]);
			if (d > es)
				es = d;
		}
		check(gsm.Device() == NkDevice::NK_GPU && es < 1e-4f, "softmax par ligne GPU == CPU");
	}

	// ---- 10) MaxPool2D GPU (forward + backward) == référence CPU ---------------
	{
		// x[1,1,4,4], kernel=2, stride=2 -> [1,1,2,2]
		float xd[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
		NkShape xs;
		xs.PushBack(1);
		xs.PushBack(1);
		xs.PushBack(4);
		xs.PushBack(4);
		NkTensor x = NkTensor::FromData(xs, xd, NkDType::NK_F32);
		NkTensor arg;
		NkTensor out = NkGpuMaxPool2D(x.ToGPU(), 2, 2, arg);
		NkTensor oc = out.ToCPU();
		// Réf CPU : max de chaque fenêtre 2x2 -> [6,8,14,16]
		float refOut[4] = {6, 8, 14, 16};
		float eo = 0;
		const float *op = oc.DataAs<float>();
		for (int i = 0; i < 4; i++) {
			float d = fabsf(op[i] - refOut[i]);
			if (d > eo)
				eo = d;
		}
		check(out.Device() == NkDevice::NK_GPU && eo < 1e-4f, "maxpool2d forward GPU == CPU");
		// Backward : grad=ones[1,1,2,2] -> dX vaut 1 aux argmax (positions 6,8,14,16), 0 sinon.
		NkTensor grad = NkTensor::Ones(NkShape{(int64)1, (int64)1, (int64)2, (int64)2});
		NkTensor dX = NkGpuMaxPool2DBackward(grad.ToGPU(), arg, 1, 1, 4, 4, 2, 2, 2, 2);
		NkTensor dc = dX.ToCPU();
		const float *dp = dc.DataAs<float>();
		double sum = 0;
		for (int i = 0; i < 16; i++)
			sum += dp[i];
		// 4 fenêtres -> exactement 4 positions à 1 ; les argmax sont les indices 5,7,13,15.
		bool routed = (dp[5] == 1.f && dp[7] == 1.f && dp[13] == 1.f && dp[15] == 1.f);
		check(dX.Device() == NkDevice::NK_GPU && fabs(sum - 4.0) < 1e-4 && routed,
			  "maxpool2d backward GPU (grad routé aux argmax) == CPU");
	}

	// ---- 11) Exp GPU == CPU ----------------------------------------------------
	{
		float m[6] = {-1, 0, 1, 2, -0.5f, 0.5f};
		NkShape s;
		s.PushBack(6);
		NkTensor x = NkTensor::FromData(s, m, NkDType::NK_F32);
		NkTensor g = ops::Exp(x.ToGPU());
		NkTensor b = g.ToCPU();
		float e = 0;
		const float *p = b.DataAs<float>();
		for (int i = 0; i < 6; i++) {
			float d = fabsf(p[i] - (float)std::exp((double)m[i]));
			if (d > e)
				e = d;
		}
		check(g.Device() == NkDevice::NK_GPU && e < 1e-4f, "exp GPU == CPU");
	}

	// ---- 12) Upsample2x GPU (forward + backward) == CPU ------------------------
	{
		float xd[4] = {1, 2, 3, 4}; // [1,1,2,2]
		NkShape xs;
		xs.PushBack(1);
		xs.PushBack(1);
		xs.PushBack(2);
		xs.PushBack(2);
		NkTensor x = NkTensor::FromData(xs, xd, NkDType::NK_F32);
		NkTensor up = NkGpuUpsample2x(x.ToGPU());
		NkTensor uc = up.ToCPU();
		const float *u = uc.DataAs<float>();
		// nearest ×2 : out[oy,ox]=in[oy/2,ox/2] -> ligne0 = 1 1 2 2, etc.
		float ref[16];
		for (int oy = 0; oy < 4; oy++)
			for (int ox = 0; ox < 4; ox++)
				ref[oy * 4 + ox] = xd[(oy / 2) * 2 + (ox / 2)];
		float eu = 0;
		for (int i = 0; i < 16; i++) {
			float d = fabsf(u[i] - ref[i]);
			if (d > eu)
				eu = d;
		}
		check(up.Device() == NkDevice::NK_GPU && eu < 1e-4f, "upsample2x forward GPU == CPU");
		// backward : grad=ones[1,1,4,4] -> chaque entrée reçoit 4.
		NkTensor grad = NkTensor::Ones(NkShape{(int64)1, (int64)1, (int64)4, (int64)4});
		NkTensor dIn = NkGpuUpsample2xBackward(grad.ToGPU(), 1, 1, 2, 2);
		NkTensor dc = dIn.ToCPU();
		const float *dpp = dc.DataAs<float>();
		bool ok = true;
		for (int i = 0; i < 4; i++)
			if (fabsf(dpp[i] - 4.f) > 1e-4f)
				ok = false;
		check(dIn.Device() == NkDevice::NK_GPU && ok, "upsample2x backward GPU == CPU");
	}

	// ---- 13) ConvTranspose2D GPU (forward + dX + dW) == CPU --------------------
	{
		// x[1,1,2,2], w[1,1,2,2], stride1 pad0 -> y[1,1,3,3]
		float xd[4] = {1, 2, 3, 4};
		float wd[4] = {1, 1, 1, 1};
		NkShape xs;
		xs.PushBack(1);
		xs.PushBack(1);
		xs.PushBack(2);
		xs.PushBack(2);
		NkShape ws;
		ws.PushBack(1);
		ws.PushBack(1);
		ws.PushBack(2);
		ws.PushBack(2);
		NkTensor x = NkTensor::FromData(xs, xd, NkDType::NK_F32);
		NkTensor w = NkTensor::FromData(ws, wd, NkDType::NK_F32);
		NkTensor y = NkGpuConvTranspose2D(x.ToGPU(), w.ToGPU(), 1, 0);
		NkTensor yc = y.ToCPU();
		const float *yp = yc.DataAs<float>();
		float ref[9];
		for (int oy = 0; oy < 3; oy++)
			for (int ox = 0; ox < 3; ox++) {
				float s = 0;
				for (int ky = 0; ky < 2; ky++)
					for (int kx = 0; kx < 2; kx++) {
						int iy = oy - ky, ix = ox - kx;
						if (iy >= 0 && iy < 2 && ix >= 0 && ix < 2)
							s += xd[iy * 2 + ix] * wd[ky * 2 + kx];
					}
				ref[oy * 3 + ox] = s;
			}
		float ef = 0;
		for (int i = 0; i < 9; i++) {
			float d = fabsf(yp[i] - ref[i]);
			if (d > ef)
				ef = d;
		}
		check(y.Device() == NkDevice::NK_GPU && ef < 1e-4f, "convT2d forward GPU == CPU");
		// dX avec grad=ones[1,1,3,3] -> chaque dX = somme(w) = 4.
		NkTensor grad = NkTensor::Ones(NkShape{(int64)1, (int64)1, (int64)3, (int64)3});
		NkTensor dX = NkGpuConvTranspose2DBackwardX(grad.ToGPU(), w.ToGPU(), 1, 1, 2, 2, 1, 2, 2, 1, 0, 3, 3);
		NkTensor dxc = dX.ToCPU();
		const float *dxp = dxc.DataAs<float>();
		bool okx = true;
		for (int i = 0; i < 4; i++)
			if (fabsf(dxp[i] - 4.f) > 1e-4f)
				okx = false;
		check(dX.Device() == NkDevice::NK_GPU && okx, "convT2d dX GPU == CPU");
		// dW avec grad=ones -> chaque dW = somme(x) = 10.
		NkTensor dW = NkGpuConvTranspose2DBackwardW(x.ToGPU(), grad.ToGPU(), 1, 1, 2, 2, 1, 2, 2, 1, 0, 3, 3);
		NkTensor dwc = dW.ToCPU();
		const float *dwp = dwc.DataAs<float>();
		bool okw = true;
		for (int i = 0; i < 4; i++)
			if (fabsf(dwp[i] - 10.f) > 1e-4f)
				okw = false;
		check(dW.Device() == NkDevice::NK_GPU && okw, "convT2d dW GPU == CPU");
	}

	// ---- 14) Conv3D GPU (forward + dX + dW) == CPU ----------------------------
	{
		// x[1,1,3,3,3], w[1,1,2,2,2]=ones, stride1 pad0 -> y[1,1,2,2,2]
		float xd[27];
		for (int i = 0; i < 27; i++)
			xd[i] = (float)(i + 1);
		float wd[8];
		for (int i = 0; i < 8; i++)
			wd[i] = 1.f;
		NkShape xs;
		xs.PushBack(1);
		xs.PushBack(1);
		xs.PushBack(3);
		xs.PushBack(3);
		xs.PushBack(3);
		NkShape ws;
		ws.PushBack(1);
		ws.PushBack(1);
		ws.PushBack(2);
		ws.PushBack(2);
		ws.PushBack(2);
		NkTensor x = NkTensor::FromData(xs, xd, NkDType::NK_F32);
		NkTensor w = NkTensor::FromData(ws, wd, NkDType::NK_F32);
		// forward GPU
		NkTensor y = NkGpuConv3D(x.ToGPU(), w.ToGPU(), 1, 0);
		NkTensor yc = y.ToCPU();
		const float *yp = yc.DataAs<float>();
		float refY[8];
		for (int od = 0; od < 2; od++)
			for (int oy = 0; oy < 2; oy++)
				for (int ox = 0; ox < 2; ox++) {
					float s = 0;
					for (int kz = 0; kz < 2; kz++)
						for (int ky = 0; ky < 2; ky++)
							for (int kx = 0; kx < 2; kx++)
								s += xd[(od + kz) * 9 + (oy + ky) * 3 + (ox + kx)] * wd[kz * 4 + ky * 2 + kx];
					refY[od * 4 + oy * 2 + ox] = s;
				}
		float ef = 0;
		for (int i = 0; i < 8; i++) {
			float d = fabsf(yp[i] - refY[i]);
			if (d > ef)
				ef = d;
		}
		check(y.Device() == NkDevice::NK_GPU && ef < 1e-3f, "conv3d forward GPU == CPU");
		// dX avec grad=ones[1,1,2,2,2]
		NkTensor grad = NkTensor::Ones(NkShape{(int64)1, (int64)1, (int64)2, (int64)2, (int64)2});
		NkTensor dX = NkGpuConv3DBackwardX(grad.ToGPU(), w.ToGPU(), x.ToGPU(), 1, 0);
		NkTensor dxc = dX.ToCPU();
		const float *dxp = dxc.DataAs<float>();
		float refDX[27];
		for (int iz = 0; iz < 3; iz++)
			for (int iy = 0; iy < 3; iy++)
				for (int ix = 0; ix < 3; ix++) {
					float s = 0;
					for (int kz = 0; kz < 2; kz++)
						for (int ky = 0; ky < 2; ky++)
							for (int kx = 0; kx < 2; kx++) {
								int od = iz - kz, oy = iy - ky, ox = ix - kx;
								if (od >= 0 && od < 2 && oy >= 0 && oy < 2 && ox >= 0 && ox < 2)
									s += wd[kz * 4 + ky * 2 + kx];
							}
					refDX[iz * 9 + iy * 3 + ix] = s;
				}
		float ex = 0;
		for (int i = 0; i < 27; i++) {
			float d = fabsf(dxp[i] - refDX[i]);
			if (d > ex)
				ex = d;
		}
		check(dX.Device() == NkDevice::NK_GPU && ex < 1e-3f, "conv3d dX GPU == CPU");
		// dW avec grad=ones
		NkTensor dW = NkGpuConv3DBackwardW(grad.ToGPU(), x.ToGPU(), w.ToGPU(), 1, 0);
		NkTensor dwc = dW.ToCPU();
		const float *dwp = dwc.DataAs<float>();
		float refDW[8];
		for (int kz = 0; kz < 2; kz++)
			for (int ky = 0; ky < 2; ky++)
				for (int kx = 0; kx < 2; kx++) {
					float s = 0;
					for (int od = 0; od < 2; od++)
						for (int oy = 0; oy < 2; oy++)
							for (int ox = 0; ox < 2; ox++)
								s += xd[(od + kz) * 9 + (oy + ky) * 3 + (ox + kx)];
					refDW[kz * 4 + ky * 2 + kx] = s;
				}
		float ew = 0;
		for (int i = 0; i < 8; i++) {
			float d = fabsf(dwp[i] - refDW[i]);
			if (d > ew)
				ew = d;
		}
		check(dW.Device() == NkDevice::NK_GPU && ew < 1e-3f, "conv3d dW GPU == CPU");
	}

	// ---- 15) ConvTranspose3D GPU (forward + dX + dW) == CPU -------------------
	{
		// x[1,1,2,2,2], w[1,1,2,2,2]=ones, stride1 pad0 -> y[1,1,3,3,3]
		float xd[8];
		for (int i = 0; i < 8; i++)
			xd[i] = (float)(i + 1); // somme = 36
		float wd[8];
		for (int i = 0; i < 8; i++)
			wd[i] = 1.f; // somme = 8
		NkShape xs;
		xs.PushBack(1);
		xs.PushBack(1);
		xs.PushBack(2);
		xs.PushBack(2);
		xs.PushBack(2);
		NkShape ws;
		ws.PushBack(1);
		ws.PushBack(1);
		ws.PushBack(2);
		ws.PushBack(2);
		ws.PushBack(2);
		NkTensor x = NkTensor::FromData(xs, xd, NkDType::NK_F32);
		NkTensor w = NkTensor::FromData(ws, wd, NkDType::NK_F32);
		NkTensor y = NkGpuConvTranspose3D(x.ToGPU(), w.ToGPU(), 1, 0);
		NkTensor yc = y.ToCPU();
		const float *yp = yc.DataAs<float>();
		float refY[27];
		for (int od = 0; od < 3; od++)
			for (int oy = 0; oy < 3; oy++)
				for (int ox = 0; ox < 3; ox++) {
					float s = 0;
					for (int kz = 0; kz < 2; kz++)
						for (int ky = 0; ky < 2; ky++)
							for (int kx = 0; kx < 2; kx++) {
								int iz = od - kz, iy = oy - ky, ix = ox - kx;
								if (iz >= 0 && iz < 2 && iy >= 0 && iy < 2 && ix >= 0 && ix < 2)
									s += xd[iz * 4 + iy * 2 + ix] * wd[kz * 4 + ky * 2 + kx];
							}
					refY[od * 9 + oy * 3 + ox] = s;
				}
		float ef = 0;
		for (int i = 0; i < 27; i++) {
			float d = fabsf(yp[i] - refY[i]);
			if (d > ef)
				ef = d;
		}
		check(y.Device() == NkDevice::NK_GPU && ef < 1e-3f, "convT3d forward GPU == CPU");
		// dX (grad=ones[1,1,3,3,3]) -> chaque = somme(w) = 8 ; dW -> chaque = somme(x) = 36.
		NkTensor grad = NkTensor::Ones(NkShape{(int64)1, (int64)1, (int64)3, (int64)3, (int64)3});
		NkTensor dX = NkGpuConvTranspose3DBackwardX(grad.ToGPU(), w.ToGPU(), x.ToGPU(), 1, 0);
		NkTensor dxc = dX.ToCPU();
		const float *dxp = dxc.DataAs<float>();
		bool okx = true;
		for (int i = 0; i < 8; i++)
			if (fabsf(dxp[i] - 8.f) > 1e-3f)
				okx = false;
		check(dX.Device() == NkDevice::NK_GPU && okx, "convT3d dX GPU == CPU");
		NkTensor dW = NkGpuConvTranspose3DBackwardW(grad.ToGPU(), x.ToGPU(), w.ToGPU(), 1, 0);
		NkTensor dwc = dW.ToCPU();
		const float *dwp = dwc.DataAs<float>();
		bool okw = true;
		for (int i = 0; i < 8; i++)
			if (fabsf(dwp[i] - 36.f) > 1e-3f)
				okw = false;
		check(dW.Device() == NkDevice::NK_GPU && okw, "convT3d dW GPU == CPU");
	}

	// ---- 16) Matmul par LOTS (batched) GPU == CPU -----------------------------
	{
		// [2,2,3] · [2,3,2] -> [2,2,2]
		float ad[12];
		for (int i = 0; i < 12; i++)
			ad[i] = (float)(i + 1);
		float bd[12];
		for (int i = 0; i < 12; i++)
			bd[i] = (float)(i + 1) * 0.5f;
		NkShape as;
		as.PushBack(2);
		as.PushBack(2);
		as.PushBack(3);
		NkShape bs;
		bs.PushBack(2);
		bs.PushBack(3);
		bs.PushBack(2);
		NkTensor A = NkTensor::FromData(as, ad, NkDType::NK_F32);
		NkTensor B = NkTensor::FromData(bs, bd, NkDType::NK_F32);
		NkTensor cCpu = ops::Matmul(A, B);				   // CPU batched
		NkTensor cGpu = ops::Matmul(A.ToGPU(), B.ToGPU()); // GPU batched
		NkTensor cg = cGpu.ToCPU();
		float e = 0;
		const float *x = cg.DataAs<float>();
		const float *y = cCpu.DataAs<float>();
		for (int i = 0; i < 8; i++) {
			float d = fabsf(x[i] - y[i]);
			if (d > e)
				e = d;
		}
		check(cGpu.Device() == NkDevice::NK_GPU && cg.Numel() == 8 && e < 1e-3f, "batched matmul GPU == CPU");
	}

	// ---- 17) LayerNorm (dernier axe) GPU (forward + backward) == CPU ----------
	{
		float m[8] = {1, 3, 2, 5, -1, 0, 4, 2}; // [2,4]
		NkShape s;
		s.PushBack(2);
		s.PushBack(4);
		NkTensor x = NkTensor::FromData(s, m, NkDType::NK_F32);
		// forward
		NkTensor gy = NkGpuLayerNormStd(x.ToGPU());
		NkTensor by = gy.ToCPU();
		const float *yp = by.DataAs<float>();
		float refY[8];
		for (int r = 0; r < 2; r++) {
			const float *xr = m + r * 4;
			double mean = 0;
			for (int c = 0; c < 4; c++)
				mean += xr[c];
			mean /= 4.0;
			double var = 0;
			for (int c = 0; c < 4; c++) {
				double t = xr[c] - mean;
				var += t * t;
			}
			var /= 4.0;
			double inv = 1.0 / std::sqrt(var + 1e-5);
			for (int c = 0; c < 4; c++)
				refY[r * 4 + c] = (float)((xr[c] - mean) * inv);
		}
		float ef = 0;
		for (int i = 0; i < 8; i++) {
			float d = fabsf(yp[i] - refY[i]);
			if (d > ef)
				ef = d;
		}
		check(gy.Device() == NkDevice::NK_GPU && ef < 1e-3f, "layernorm forward GPU == CPU");
		// backward avec grad = petit motif
		float gd[8] = {0.1f, -0.2f, 0.3f, 0.05f, -0.4f, 0.2f, 0.1f, -0.1f};
		NkShape gs;
		gs.PushBack(2);
		gs.PushBack(4);
		NkTensor grad = NkTensor::FromData(gs, gd, NkDType::NK_F32);
		NkTensor gdx = NkGpuLayerNormStdBackward(x.ToGPU(), grad.ToGPU());
		NkTensor bdx = gdx.ToCPU();
		const float *dxp = bdx.DataAs<float>();
		float refDX[8];
		for (int r = 0; r < 2; r++) {
			const float *xr = m + r * 4;
			const float *gr = gd + r * 4;
			double mean = 0;
			for (int c = 0; c < 4; c++)
				mean += xr[c];
			mean /= 4.0;
			double var = 0;
			for (int c = 0; c < 4; c++) {
				double t = xr[c] - mean;
				var += t * t;
			}
			var /= 4.0;
			double inv = 1.0 / std::sqrt(var + 1e-5);
			double m1 = 0, m2 = 0;
			for (int c = 0; c < 4; c++) {
				double xh = (xr[c] - mean) * inv;
				m1 += gr[c];
				m2 += gr[c] * xh;
			}
			m1 /= 4.0;
			m2 /= 4.0;
			for (int c = 0; c < 4; c++) {
				double xh = (xr[c] - mean) * inv;
				refDX[r * 4 + c] = (float)(inv * (gr[c] - m1 - xh * m2));
			}
		}
		float ex = 0;
		for (int i = 0; i < 8; i++) {
			float d = fabsf(dxp[i] - refDX[i]);
			if (d > ex)
				ex = d;
		}
		check(gdx.Device() == NkDevice::NK_GPU && ex < 1e-3f, "layernorm backward GPU == CPU");
	}

	// ---- 18) Softmax backward + Softmax CAUSAL GPU == CPU ---------------------
	{
		// softmax backward : y = softmax(scores) [2,3], grad motif -> dx == formule.
		float sd[6] = {1, 2, 0.5f, -1, 0, 2};
		NkShape s2;
		s2.PushBack(2);
		s2.PushBack(3);
		NkTensor sc = NkTensor::FromData(s2, sd, NkDType::NK_F32);
		NkTensor y = NkGpuSoftmaxRows(sc.ToGPU());
		NkTensor yc = y.ToCPU();
		const float *yp = yc.DataAs<float>();
		float gd[6] = {0.2f, -0.1f, 0.3f, 0.4f, -0.2f, 0.1f};
		NkTensor grad = NkTensor::FromData(s2, gd, NkDType::NK_F32);
		NkTensor gdx = NkGpuSoftmaxBackward(y, grad.ToGPU());
		NkTensor bdx = gdx.ToCPU();
		const float *dxp = bdx.DataAs<float>();
		float refDX[6];
		for (int r = 0; r < 2; r++) {
			const float *yr = yp + r * 3;
			const float *gr = gd + r * 3;
			double sdot = 0;
			for (int c = 0; c < 3; c++)
				sdot += (double)gr[c] * yr[c];
			for (int c = 0; c < 3; c++)
				refDX[r * 3 + c] = (float)(yr[c] * (gr[c] - sdot));
		}
		float ex = 0;
		for (int i = 0; i < 6; i++) {
			float d = fabsf(dxp[i] - refDX[i]);
			if (d > ex)
				ex = d;
		}
		check(gdx.Device() == NkDevice::NK_GPU && ex < 1e-3f, "softmax backward GPU == CPU");

		// softmax CAUSAL sur [1,1,3,3] (T=3) : triangle supérieur strict == 0 (futur masqué).
		float scores[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
		NkShape s3;
		s3.PushBack(1);
		s3.PushBack(1);
		s3.PushBack(3);
		s3.PushBack(3);
		NkTensor sc3 = NkTensor::FromData(s3, scores, NkDType::NK_F32);
		NkTensor yc3 = NkGpuSoftmaxCausal(sc3.ToGPU()).ToCPU();
		const float *p = yc3.DataAs<float>();
		// ligne i (requête) : colonnes j>i doivent valoir 0 ; ligne somme (j<=i) == 1.
		bool okMask = (p[1] == 0.f && p[2] == 0.f && p[5] == 0.f); // (0,1)(0,2)(1,2) masqués
		bool okSum = fabsf((p[0]) - 1.f) < 1e-4f				   // ligne0 : 1 seule clé
					 && fabsf((p[3] + p[4]) - 1.f) < 1e-4f		   // ligne1 : 2 clés
					 && fabsf((p[6] + p[7] + p[8]) - 1.f) < 1e-4f; // ligne2 : 3 clés
		check(okMask && okSum, "softmax causal GPU (futur masqué + lignes normalisées)");
	}

	// ---- 19) GELU GPU (forward + backward) == CPU -----------------------------
	{
		float m[6] = {-2, -0.5f, 0, 0.5f, 1, 2};
		NkShape s;
		s.PushBack(6);
		NkTensor x = NkTensor::FromData(s, m, NkDType::NK_F32);
		NkTensor gy = NkGpuGelu(x.ToGPU()).ToCPU();
		const float *yp = gy.DataAs<float>();
		const double c = 0.7978845608;
		float refY[6];
		for (int i = 0; i < 6; i++) {
			double v = m[i];
			double inner = c * (v + 0.044715 * v * v * v);
			refY[i] = (float)(0.5 * v * (1.0 + std::tanh(inner)));
		}
		float ef = 0;
		for (int i = 0; i < 6; i++) {
			float d = fabsf(yp[i] - refY[i]);
			if (d > ef)
				ef = d;
		}
		check(ef < 1e-3f, "gelu forward GPU == CPU");
		float gd[6] = {0.1f, -0.2f, 0.3f, 0.4f, -0.1f, 0.2f};
		NkTensor grad = NkTensor::FromData(s, gd, NkDType::NK_F32);
		NkTensor dx = NkGpuGeluBackward(x.ToGPU(), grad.ToGPU()).ToCPU();
		const float *dxp = dx.DataAs<float>();
		float refDX[6];
		for (int i = 0; i < 6; i++) {
			double v = m[i];
			double v2 = v * v;
			double inner = c * (v + 0.044715 * v2 * v);
			double t = std::tanh(inner);
			double dg = 0.5 * (1.0 + t) + 0.5 * v * (1.0 - t * t) * c * (1.0 + 3.0 * 0.044715 * v2);
			refDX[i] = (float)(gd[i] * dg);
		}
		float ex = 0;
		for (int i = 0; i < 6; i++) {
			float d = fabsf(dxp[i] - refDX[i]);
			if (d > ex)
				ex = d;
		}
		check(ex < 1e-3f, "gelu backward GPU == CPU");
	}

	// ---- 20) Embedding GPU (forward + backward scatter-add) == CPU -------------
	{
		float td[6] = {1, 2, 3, 4, 5, 6}; // table [3,2]
		float idd[4] = {0, 1, 2, 1};	  // indices [4]
		NkShape ts;
		ts.PushBack(3);
		ts.PushBack(2);
		NkShape is;
		is.PushBack(4);
		NkTensor table = NkTensor::FromData(ts, td, NkDType::NK_F32);
		NkTensor idx = NkTensor::FromData(is, idd, NkDType::NK_F32);
		NkTensor out = NkGpuEmbedding(table.ToGPU(), idx.ToGPU()).ToCPU(); // [4,2]
		const float *op = out.DataAs<float>();
		float refO[8] = {1, 2, 3, 4, 5, 6, 3, 4};
		float eo = 0;
		for (int i = 0; i < 8; i++) {
			float d = fabsf(op[i] - refO[i]);
			if (d > eo)
				eo = d;
		}
		check(out.Numel() == 8 && eo < 1e-4f, "embedding forward GPU == CPU");
		// backward : grad=ones[4,2] -> dTable[0]=[1,1], [1]=[2,2] (row1 utilisé 2×), [2]=[1,1]
		NkTensor grad = NkTensor::Ones(NkShape{(int64)4, (int64)2});
		NkTensor dt = NkGpuEmbeddingBackward(grad.ToGPU(), idx.ToGPU(), 3, 2).ToCPU(); // [3,2]
		const float *dp = dt.DataAs<float>();
		float refDT[6] = {1, 1, 2, 2, 1, 1};
		float ed = 0;
		for (int i = 0; i < 6; i++) {
			float d = fabsf(dp[i] - refDT[i]);
			if (d > ed)
				ed = d;
		}
		check(dt.Numel() == 6 && ed < 1e-4f, "embedding backward GPU (scatter-add) == CPU");
	}

	printf("\n=== Résultat : %d OK, %d échec(s) ===\n", g_ok, g_fail);
	gpu.Shutdown();
	return g_fail;
}
