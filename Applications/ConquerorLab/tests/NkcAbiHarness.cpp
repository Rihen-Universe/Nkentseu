// Banc d'essai autonome du contrat Conqueror : charge les deux modules, joue des
// parties completes, verifie le determinisme et la legalite des coups.
#include "Conqueror/ConquerorRulesABI.h"
#include "Conqueror/ConquerorAIABI.h"
#include <cstdio>
#include <cstring>

extern "C" __declspec(dllimport) void *__stdcall LoadLibraryA(const char *);
extern "C" __declspec(dllimport) void *__stdcall GetProcAddress(void *, const char *);

using namespace nkentseu;
using namespace nkentseu::conqueror;

static int gChecks = 0, gFails = 0;
#define CHECK(cond, msg)                                                        \
	do {                                                                        \
		if (cond) { ++gChecks; std::printf("  OK   %s\n", msg); }                \
		else      { ++gFails;  std::printf("  RATE %s\n", msg); }                \
	} while (0)

int main() {
	std::printf("=== Banc d'essai du contrat Conqueror ===\n\n");

	void *hR = LoadLibraryA("ConquerorRulesV2.dll");
	void *hA = LoadLibraryA("ConquerorAIRef.dll");
	CHECK(hR && hA, "les deux modules se chargent");
	if (!hR || !hA) return 1;

	auto getRules = (NkcRulesGetFactoryFn)GetProcAddress(hR, NKC_RULES_SYM_GET_FACTORY);
	auto getAi    = (NkcAIGetFactoryFn)GetProcAddress(hA, NKC_AI_SYM_GET_FACTORY);
	CHECK(getRules && getAi, "les symboles de fabrique sont exportes");
	if (!getRules || !getAi) return 1;

	NkcRulesFactory rf{}; getRules(&rf);
	NkcAIFactory    af{}; getAi(&af);
	CHECK(rf.info.abiVersion == kRulesAbiVersion, "ABI des regles conforme");
	CHECK(af.info.abiVersion == kAiAbiVersion,    "ABI de l'IA conforme");
	std::printf("       regles : %s %s (palier %u)\n", rf.info.name, rf.info.version, rf.info.palier);
	std::printf("       IA     : %s %s\n\n", af.info.name, af.info.version);

	const NkcRulesVTable &R = rf.vtable;
	NkcRules ri = R.Create();
	CHECK(ri != nullptr, "instance de regles creee");

	// --- schema de parametres ------------------------------------------------
	const char *schema = R.GetParamsSchemaJson(ri);
	CHECK(schema && std::strstr(schema, "portee_duplication") != nullptr,
	      "le schema de parametres est expose");

	// --- une partie complete, IA contre IA -----------------------------------
	NkcState st = R.CreateState(ri);
	R.Setup(ri, st, 2, 12345);

	NkcStateView v;
	R.GetView(ri, st, &v);
	std::printf("       plateau : %u cases, %u joueurs\n", v.cellCount, v.playerCount);
	CHECK(v.cellCount == 42, "plateau hexagonal 6x7 = 42 cases");
	CHECK(v.totemCount[0] == 2 && v.totemCount[1] == 2, "2 totems par joueur au depart");

	NkcAI ai[2] = { af.vtable.Create(), af.vtable.Create() };
	NkcAIConfig cfg{};
	cfg.difficulty = NkcDifficulty::Normal; cfg.seed = 777;
	af.vtable.Configure(ai[0], &cfg);
	cfg.difficulty = NkcDifficulty::Easy;   cfg.seed = 999;
	af.vtable.Configure(ai[1], &cfg);

	NkcMove journal[4096];
	uint32  played = 0;
	bool    allLegal = true;

	while (!R.IsFinished(ri, st) && played < 4096) {
		R.GetView(ri, st, &v);
		NkcAIResult res{};
		if (!af.vtable.ChooseMove(ai[v.current], &R, ri, st, &res)) break;
		if (!R.IsLegalMove(ri, st, &res.move)) { allLegal = false; break; }
		journal[played++] = res.move;
		if (!R.ApplyMove(ri, st, &res.move, nullptr, nullptr)) break;
	}

	CHECK(allLegal, "l'IA n'a jamais produit de coup illegal");
	CHECK(R.IsFinished(ri, st), "la partie se termine d'elle-meme");

	R.GetView(ri, st, &v);
	const uint64 h1 = R.HashState(ri, st);
	std::printf("       partie : %u coups, vainqueur = %d, totems %d/%d\n",
	            played, (int)v.winner, v.totemCount[0], v.totemCount[1]);
	CHECK(v.totemCount[0] + v.totemCount[1] <= 42, "jamais plus de totems que de cases");

	// --- determinisme : rejouer le journal donne le meme etat ----------------
	NkcState st2 = R.CreateState(ri);
	R.Setup(ri, st2, 2, 12345);
	bool replayOk = true;
	for (uint32 i = 0; i < played; ++i)
		if (!R.ApplyMove(ri, st2, &journal[i], nullptr, nullptr)) { replayOk = false; break; }
	const uint64 h2 = R.HashState(ri, st2);
	CHECK(replayOk, "le journal se rejoue integralement");
	CHECK(h1 == h2, "rejeu deterministe : empreinte identique");
	std::printf("       empreinte : %llu\n\n", (unsigned long long)h1);

	// --- equivalence blocage <-> absence de coup legal (REGLES §13) ----------
	{
		NkcState probe = R.CreateState(ri);
		R.Setup(ri, probe, 2, 4242);
		bool equiv = true;
		for (int32 step = 0; step < 200 && !R.IsFinished(ri, probe); ++step) {
			NkcStateView pv; R.GetView(ri, probe, &pv);
			NkcMove buf[512];
			const uint32 n = R.GenerateLegalMoves(ri, probe, buf, 512);
			const bool onlyPass = (n == 1 && buf[0].kind == NkcMoveKind::Pass);
			const bool blocked  = R.IsPlayerBlocked(ri, probe, pv.current) != 0;
			if (onlyPass != blocked) { equiv = false; break; }
			if (n == 0) break;
			if (!R.ApplyMove(ri, probe, &buf[0], nullptr, nullptr)) break;
		}
		CHECK(equiv, "« aucun coup legal » equivaut a « joueur bloque »");
		R.DestroyState(ri, probe);
	}

	// --- les parametres sont bornes, et changent bien le comportement --------
	{
		// Hors plage : doit etre borne au minimum du parametre (10), pas accepte.
		R.SetParam(ri, "max_tours", 3);
		CHECK(R.GetParam(ri, "max_tours") == 10.0,
		      "une valeur hors plage est bornee, pas acceptee telle quelle");

		NkcState tiny = R.CreateState(ri);
		R.Setup(ri, tiny, 2, 1);
		int32 steps = 0;
		while (!R.IsFinished(ri, tiny) && steps < 100) {
			NkcMove buf[512];
			if (R.GenerateLegalMoves(ri, tiny, buf, 512) == 0) break;
			R.ApplyMove(ri, tiny, &buf[0], nullptr, nullptr);
			++steps;
		}
		// max_tours = 10, 2 joueurs -> le garde-fou coupe a 20 tours.
		CHECK(steps <= 22, "le garde-fou max_tours coupe bien la partie");
		R.DestroyState(ri, tiny);
		R.SetParam(ri, "max_tours", 200);
	}

	af.vtable.Destroy(ai[0]);
	af.vtable.Destroy(ai[1]);
	R.DestroyState(ri, st);
	R.DestroyState(ri, st2);
	R.Destroy(ri);

	std::printf("\n=== %d verifications OK, %d echecs ===\n", gChecks, gFails);
	return gFails ? 1 : 0;
}
