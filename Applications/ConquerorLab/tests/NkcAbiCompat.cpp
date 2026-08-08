// =============================================================================
// NkcAbiCompat — un module d'EPOQUE tourne-t-il encore ?
//
// CE QUE CE FICHIER PROTEGE
// -------------------------
// Deux stagiaires travaillent huit semaines sur ce contrat. Chaque fois que
// J'AJOUTE une fonction a la vtable, la question est : leur travail continue-t-il
// de fonctionner sans recompilation ?
//
// La reponse est OUI a une condition — n'ajouter qu'A LA FIN des structures.
// C'est une promesse facile a faire et facile a rompre : il suffit d'inserer un
// champ au milieu, et rien ne le signale a la compilation. Ce banc d'essai est
// ce qui transforme l'intention en propriete verifiee.
//
// COMMENT IL S'Y PREND
// --------------------
//   1. `tests/abi_fige_3_0/` contient les en-tetes TELS QU'ILS ETAIENT.
//      Ils ne sont jamais mis a jour : c'est tout leur interet.
//   2. On compile un module contre CES en-tetes-la.
//   3. On le charge avec les en-tetes d'AUJOURD'HUI, et on lui fait jouer une
//      partie complete.
//
// SI CE TEST ECHOUE, ce n'est pas le test qu'il faut corriger : c'est qu'une
// RUPTURE a ete introduite sans monter la MAJEURE. Le travail des stagiaires
// vient de casser, et il vaut mieux l'apprendre ici que par eux.
//
// COMPILER (voir tests/compat.ps1, qui fait les trois etapes) :
//   clang++ -shared ... -Itests/abi_fige_3_0 ...  -o vieux_module.dll
//   clang++ ...       -Iinclude ...              -o compat.exe NkcAbiCompat.cpp
//   ./compat.exe vieux_regles.dll vieille_ia.dll
// =============================================================================

#include "Conqueror/ConquerorRulesABI.h"
#include "Conqueror/ConquerorAIABI.h"

#include <cstdio>
#include <cstring>
#include <cstdarg>

#if defined(_WIN32)
#	include <windows.h>
#	define NKC_OPEN(p)	 reinterpret_cast<void *>(::LoadLibraryA(p))
#	define NKC_SYM(h, n) reinterpret_cast<void *>(::GetProcAddress(static_cast<HMODULE>(h), n))
#else
#	include <dlfcn.h>
#	define NKC_OPEN(p)	 dlopen(p, RTLD_NOW | RTLD_LOCAL)
#	define NKC_SYM(h, n) dlsym(h, n)
#endif

using namespace nkentseu;
using namespace nkentseu::conqueror;

namespace {

	int32 gOk = 0, gFail = 0;

	void Check(bool cond, const char *what) {
		std::printf("  %s   %s\n", cond ? "OK  " : "RATE", what);
		if (cond) ++gOk; else ++gFail;
	}

	void Info(const char *fmt, ...) {
		va_list ap;
		va_start(ap, fmt);
		std::printf("       ");
		std::vprintf(fmt, ap);
		std::printf("\n");
		va_end(ap);
	}

} // namespace

int main(int argc, char **argv) {
	const char *rulesPath = argc > 1 ? argv[1] : "vieux_regles.dll";
	const char *aiPath	  = argc > 2 ? argv[2] : "vieille_ia.dll";

	std::printf("=== Compatibilite ascendante de l'ABI ===\n\n");
	Info("atelier      : regles %u.%u, IA %u.%u", kRulesAbiMajor, kRulesAbiMinor,
		 kAiAbiMajor, kAiAbiMinor);
	Info("sizeof vtable: regles %u o, IA %u o",
		 static_cast<unsigned>(sizeof(NkcRulesVTable)),
		 static_cast<unsigned>(sizeof(NkcAIVTable)));
	std::printf("\n");

	void *hr = NKC_OPEN(rulesPath);
	void *ha = NKC_OPEN(aiPath);
	Check(hr != nullptr && ha != nullptr, "les deux modules d'epoque se chargent");
	if (!hr || !ha) {
		std::printf("\n  (%s / %s introuvables)\n", rulesPath, aiPath);
		return 1;
	}

	auto getR = reinterpret_cast<NkcRulesGetFactoryFn>(NKC_SYM(hr, NKC_RULES_SYM_GET_FACTORY));
	auto getA = reinterpret_cast<NkcAIGetFactoryFn>(NKC_SYM(ha, NKC_AI_SYM_GET_FACTORY));
	Check(getR && getA, "les symboles de fabrique sont exportes");
	if (!getR || !getA) return 1;

	// LE memset EST LE MECANISME : un module d'epoque ecrit une structure plus
	// COURTE ; la queue reste donc nulle, et chaque entree optionnelle est gardee
	// par un test de nullite. Sans cette mise a zero, on lirait de la memoire non
	// initialisee et on appellerait une adresse au hasard.
	NkcRulesFactory rf;
	NkcAIFactory	af;
	std::memset(&rf, 0, sizeof(rf));
	std::memset(&af, 0, sizeof(af));
	getR(&rf);
	getA(&af);

	Info("regles : %s (ABI %u.%u)", rf.info.name, rf.info.abiVersion, rf.abiMinor);
	Info("IA     : %s (ABI %u.%u)", af.info.name, af.info.abiVersion, af.abiMinor);

	Check(rf.info.abiVersion == kRulesAbiMajor,
		  "meme ABI MAJEURE cote regles (sinon : rupture non signalee)");
	Check(af.info.abiVersion == kAiAbiMajor,
		  "meme ABI MAJEURE cote IA");
	Check(rf.abiMinor <= kRulesAbiMinor,
		  "la mineure du module ne depasse pas celle de l'atelier");

	// La preuve que l'ajout s'est bien fait EN FIN de structure.
	Check(rf.vtable.GetCellCenter == nullptr && rf.vtable.GetCellShape == nullptr &&
			  rf.vtable.GetNeighbors == nullptr,
		  "les entrees ajoutees depuis sont NULLES, pas du hasard");

	// ---- et maintenant, ca doit JOUER --------------------------------------
	NkcRules r	= rf.vtable.Create();
	NkcState st = rf.vtable.CreateState(r);
	Check(r != nullptr && st != nullptr, "instance et etat crees");
	if (!r || !st) return 1;

	rf.vtable.Setup(r, st, 2, 20260807ull);

	NkcAI		ai = af.vtable.Create();
	NkcAIConfig cfg;
	cfg.difficulty = NkcDifficulty::Normal;
	cfg.budgetMs   = 50;
	cfg.seed	   = 7;
	af.vtable.Configure(ai, &cfg);

	// LE POINT CRITIQUE : on passe la vtable D'AUJOURD'HUI (plus grande) a une IA
	// compilee contre l'ancienne definition. Elle n'en lit que le prefixe — ce qui
	// n'est vrai QUE si les ajouts sont en fin de structure.
	int32 coups = 0, illegaux = 0;
	while (!rf.vtable.IsFinished(r, st) && coups < 500) {
		NkcAIResult res;
		std::memset(&res, 0, sizeof(res));
		if (!af.vtable.ChooseMove(ai, &rf.vtable, r, st, &res)) break;
		if (!rf.vtable.IsLegalMove(r, st, &res.move)) { ++illegaux; break; }
		if (!rf.vtable.ApplyMove(r, st, &res.move, nullptr, nullptr)) break;
		if (af.vtable.OnMovePlayed) af.vtable.OnMovePlayed(ai, &res.move);
		++coups;
	}

	Check(illegaux == 0, "l'IA d'epoque ne produit aucun coup illegal");
	Check(coups > 5, "la partie avance");
	Check(rf.vtable.IsFinished(r, st) != 0, "la partie se termine d'elle-meme");
	Info("partie : %d coups, vainqueur %d, empreinte %llu", coups,
		 static_cast<int>(rf.vtable.GetWinner(r, st)),
		 static_cast<unsigned long long>(rf.vtable.HashState(r, st)));

	// La geometrie non declaree DOIT retomber sur la topologie, pas planter.
	NkcStateView v;
	std::memset(&v, 0, sizeof(v));
	rf.vtable.GetView(r, st, &v);
	Check(v.cellCount > 0 && v.coords != nullptr,
		  "la vue reste lisible (repli topologique operationnel)");

	af.vtable.Destroy(ai);
	rf.vtable.DestroyState(r, st);
	rf.vtable.Destroy(r);

	std::printf("\n=== %d verifications OK, %d echecs ===\n", gOk, gFail);
	if (gFail) {
		std::printf("\nUNE RUPTURE A ETE INTRODUITE SANS MONTER LA MAJEURE.\n"
					"Le travail des stagiaires vient de casser. Voir le commentaire\n"
					"en tete de ConquerorRulesABI.h : on n'ajoute qu'A LA FIN.\n");
	}
	return gFail == 0 ? 0 : 1;
}
