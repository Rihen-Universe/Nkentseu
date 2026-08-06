// =============================================================================
// ConquerorAIRef.cpp — IA de reference de l'atelier Conqueror.
//
// Trois strategies dans un module, choisies par le palier de difficulte :
//   Facile           : aleatoire pur
//   Normal           : glouton (maximise le gain immediat)
//   Hard/Expert/Apex : Minimax alpha-beta, profondeur 2 / 3 / 4
//
// Ce module n'est PAS le livrable du stagiaire A2 : c'est son point de depart,
// son adversaire de calibration, et la preuve que le contrat est implementable.
// A2 livrera un MCTS multithread bien plus fort.
//
// L'IA ne connait QUE la NkcRulesVTable : elle ignore quelles regles elle joue.
// Changer un parametre du moteur ne la casse pas — c'est tout l'interet.
// =============================================================================

#include "Conqueror/ConquerorAIABI.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <new>

using namespace nkentseu;
using namespace nkentseu::conqueror;

namespace {

	NkcAllocFn gAlloc = nullptr;
	NkcFreeFn  gFree  = nullptr;

	void *RawAlloc(usize n) { return gAlloc ? gAlloc(n) : std::malloc(n); }
	void  RawFree(void *p)  { if (!p) return; if (gFree) gFree(p); else std::free(p); }

	constexpr uint32 kMoveCap = 1024;

	struct AI {
			NkcAIConfig cfg;
			uint64		rng		  = 0x243F6A8885A308D3ull;
			uint32		nodes	  = 0;
			int32		wMaterial = 100;   // reglable via SetParam
			int32		wMobility = 10;
			char		debug[4096] = {};
	};

	/// PRNG xorshift64* — porte par l'instance, jamais global.
	uint64 NextRand(uint64 &s) {
		s ^= s >> 12;
		s ^= s << 25;
		s ^= s >> 27;
		return s * 2685821657736338717ull;
	}

	// -------------------------------------------------------------------------
	// Evaluation d'une position du point de vue de `me`.
	// Volontairement simple et documentee : c'est exactement ce que le stagiaire
	// A2 devra remettre en cause (fiche A2 §3, premiere sous-question).
	// -------------------------------------------------------------------------
	int32 Evaluate(const NkcRulesVTable *R, NkcRules ri, const NkcState st,
				   uint8 me, const AI *ai) {
		NkcStateView v;
		R->GetView(ri, st, &v);

		if (v.finished) {
			if (v.winner == static_cast<int8>(me)) return 1000000;
			if (v.winner < 0)					   return 0;	  // nul
			return -1000000;
		}

		int32 mine = 0, theirs = 0;
		for (uint8 p = 0; p < v.playerCount; ++p) {
			if (p == me) mine += v.totemCount[p];
			else		 theirs += v.totemCount[p];
		}

		// Mobilite du joueur au trait, bornee pour rester bon marche.
		NkcMove		 buf[64];
		const uint32 n		  = R->GenerateLegalMoves(ri, st, buf, 64);
		int32		 mobility = static_cast<int32>(n > 64 ? 64 : n);
		if (v.current != me) mobility = -mobility;

		return ai->wMaterial * (mine - theirs) + ai->wMobility * mobility;
	}

	// -------------------------------------------------------------------------
	// Minimax alpha-beta. En 1v1 c'est exact ; a 3-4 joueurs on traite tout
	// adversaire comme « l'autre camp », ce qui est une approximation grossiere
	// — un des points que A2 devra traiter proprement.
	// -------------------------------------------------------------------------
	int32 Search(const NkcRulesVTable *R, NkcRules ri, NkcState work, uint8 me,
				 int32 depth, int32 alpha, int32 beta, AI *ai) {
		ai->nodes++;

		if (depth <= 0 || R->IsFinished(ri, work)) return Evaluate(R, ri, work, me, ai);

		NkcMove		 moves[kMoveCap];
		const uint32 total = R->GenerateLegalMoves(ri, work, moves, kMoveCap);
		const uint32 n	   = total < kMoveCap ? total : kMoveCap;
		if (n == 0) return Evaluate(R, ri, work, me, ai);

		NkcStateView v;
		R->GetView(ri, work, &v);
		const bool maximizing = (v.current == me);

		NkcState child = R->CreateState(ri);
		int32	 best  = maximizing ? -2000000 : 2000000;

		for (uint32 i = 0; i < n; ++i) {
			R->CloneState(ri, child, work);
			if (!R->ApplyMove(ri, child, &moves[i], nullptr, nullptr)) continue;

			const int32 sc = Search(R, ri, child, me, depth - 1, alpha, beta, ai);

			if (maximizing) {
				if (sc > best) best = sc;
				if (best > alpha) alpha = best;
			} else {
				if (sc < best) best = sc;
				if (best < beta) beta = best;
			}
			if (beta <= alpha) break;	// elagage
		}

		R->DestroyState(ri, child);
		return best;
	}

	// =========================================================================
	// vtable
	// =========================================================================
	NkcAI V_Create() {
		void *mem = RawAlloc(sizeof(AI));
		if (!mem) return nullptr;
		AI *a		   = new (mem) AI();
		a->cfg.difficulty = NkcDifficulty::Normal;
		a->cfg.budgetMs	  = 1000;
		a->cfg.seed		  = 0x243F6A8885A308D3ull;
		return static_cast<NkcAI>(a);
	}

	void V_Destroy(NkcAI self) {
		if (!self) return;
		AI *a = static_cast<AI *>(self);
		a->~AI();
		RawFree(a);
	}

	void V_Configure(NkcAI self, const NkcAIConfig *cfg) {
		AI *a = static_cast<AI *>(self);
		if (!a || !cfg) return;
		a->cfg = *cfg;
		a->rng = cfg->seed ? cfg->seed : 0x243F6A8885A308D3ull;
	}

	const char *V_GetParamsSchemaJson(NkcAI self) {
		AI		   *a = static_cast<AI *>(self);
		static char buf[512];
		std::snprintf(buf, sizeof(buf),
					  "[{\"key\":\"poids_materiel\",\"group\":\"Evaluation\",\"type\":\"int\","
					  "\"min\":0,\"max\":500,\"def\":100,\"val\":%d},"
					  "{\"key\":\"poids_mobilite\",\"group\":\"Evaluation\",\"type\":\"int\","
					  "\"min\":0,\"max\":200,\"def\":10,\"val\":%d}]",
					  a->wMaterial, a->wMobility);
		return buf;
	}

	int32 V_SetParam(NkcAI self, const char *key, float64 value) {
		AI *a = static_cast<AI *>(self);
		if (!a || !key) return 0;
		const int32 v = static_cast<int32>(value < 0 ? value - 0.5 : value + 0.5);
		if (std::strcmp(key, "poids_materiel") == 0) { a->wMaterial = v; return 1; }
		if (std::strcmp(key, "poids_mobilite") == 0) { a->wMobility = v; return 1; }
		return 0;
	}

	int32 V_ChooseMove(NkcAI self, const NkcRulesVTable *R, NkcRules ri,
					   const NkcState st, NkcAIResult *out) {
		AI *a = static_cast<AI *>(self);
		if (!a || !R || !ri || !st || !out) return 0;

		std::memset(out, 0, sizeof(*out));
		out->move.targetLevel = -1;
		out->move.powerId	  = -1;
		a->nodes			  = 0;

		NkcMove		 moves[kMoveCap];
		const uint32 total = R->GenerateLegalMoves(ri, st, moves, kMoveCap);
		const uint32 n	   = total < kMoveCap ? total : kMoveCap;
		if (n == 0) return 0;

		NkcStateView v;
		R->GetView(ri, st, &v);
		const uint8 me = v.current;

		// --- Facile : aleatoire pur ------------------------------------------
		if (a->cfg.difficulty == NkcDifficulty::Easy) {
			const uint32 pick = static_cast<uint32>(NextRand(a->rng) % n);
			out->move		  = moves[pick];
			out->simulations  = 1;
			std::snprintf(a->debug, sizeof(a->debug),
						  "{\"strategy\":\"random\",\"moves\":%u}", n);
			return 1;
		}

		// --- Normal : glouton a un coup --------------------------------------
		if (a->cfg.difficulty == NkcDifficulty::Normal) {
			NkcState child	 = R->CreateState(ri);
			int32	 best	 = -2000000;
			uint32	 bestIdx = 0;
			for (uint32 i = 0; i < n; ++i) {
				R->CloneState(ri, child, st);
				if (!R->ApplyMove(ri, child, &moves[i], nullptr, nullptr)) continue;
				const int32 sc = Evaluate(R, ri, child, me, a);
				if (sc > best) { best = sc; bestIdx = i; }
			}
			R->DestroyState(ri, child);
			out->move		   = moves[bestIdx];
			out->scoreMilli	   = best;
			out->simulations   = n;
			out->depthReached  = 1;
			std::snprintf(a->debug, sizeof(a->debug),
						  "{\"strategy\":\"greedy\",\"moves\":%u,\"best\":%d}", n, best);
			return 1;
		}

		// --- Hard / Expert / Apex : Minimax alpha-beta ------------------------
		const int32 depth = (a->cfg.difficulty == NkcDifficulty::Hard)	 ? 2
						  : (a->cfg.difficulty == NkcDifficulty::Expert) ? 3
																		 : 4;

		NkcState child	 = R->CreateState(ri);
		int32	 best	 = -2000000, alpha = -2000000;
		uint32	 bestIdx = 0;
		for (uint32 i = 0; i < n; ++i) {
			R->CloneState(ri, child, st);
			if (!R->ApplyMove(ri, child, &moves[i], nullptr, nullptr)) continue;
			const int32 sc = Search(R, ri, child, me, depth - 1, alpha, 2000000, a);
			if (sc > best) { best = sc; bestIdx = i; if (sc > alpha) alpha = sc; }
		}
		R->DestroyState(ri, child);

		out->move		  = moves[bestIdx];
		out->scoreMilli	  = best;
		out->simulations  = a->nodes;
		out->depthReached = static_cast<uint32>(depth);
		std::snprintf(a->debug, sizeof(a->debug),
					  "{\"strategy\":\"minimax\",\"depth\":%d,\"nodes\":%u,\"best\":%d}",
					  depth, a->nodes, best);
		return 1;
	}

	void V_OnMovePlayed(NkcAI, const NkcMove *) {}
	void V_Reset(NkcAI self) { if (AI *a = static_cast<AI *>(self)) a->nodes = 0; }

	const char *V_GetDebugJson(NkcAI self) {
		AI *a = static_cast<AI *>(self);
		return a ? a->debug : "";
	}

	void FillFactory(NkcAIFactory *out) {
		if (!out) return;
		std::memset(out, 0, sizeof(*out));
		out->info.abiVersion = kAiAbiVersion;
		std::snprintf(out->info.name, sizeof(out->info.name), "AIRef");
		std::snprintf(out->info.version, sizeof(out->info.version), "1.0.0");
		std::snprintf(out->info.author, sizeof(out->info.author), "Studio RIHEN");
		out->info.difficultyCount = static_cast<uint8>(NkcDifficulty::Count);
		out->info.isDeterministic = 1;
		out->info.isThreadSafe	  = 1;

		out->vtable.Create				= V_Create;
		out->vtable.Destroy				= V_Destroy;
		out->vtable.Configure			= V_Configure;
		out->vtable.GetParamsSchemaJson	= V_GetParamsSchemaJson;
		out->vtable.SetParam			= V_SetParam;
		out->vtable.ChooseMove			= V_ChooseMove;
		out->vtable.OnMovePlayed		= V_OnMovePlayed;
		out->vtable.Reset				= V_Reset;
		out->vtable.GetDebugJson		= V_GetDebugJson;
	}

} // namespace

#if defined(NKC_AI_STATIC) && NKC_AI_STATIC
void NkcAIRefGetFactory(NkcAIFactory *out) { FillFactory(out); }
void NkcAIRefSetAllocator(NkcAllocFn a, NkcFreeFn f) { gAlloc = a; gFree = f; }
#else
NKC_MODULE_EXPORT void nkc_ai_set_allocator(NkcAllocFn a, NkcFreeFn f) { gAlloc = a; gFree = f; }
NKC_MODULE_EXPORT void nkc_ai_get_factory(NkcAIFactory *out) { FillFactory(out); }
#endif
