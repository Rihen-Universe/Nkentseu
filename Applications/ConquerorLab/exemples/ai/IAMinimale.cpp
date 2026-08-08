// =============================================================================
// IAMinimale.cpp — LA PLUS PETITE IA qui joue vraiment.
//
// A QUOI CE FICHIER SERT
// ----------------------
// C'est le point de depart du stagiaire A2. Il tient en une idee :
//
//     demander les coups legaux au moteur, en prendre un au hasard.
//
// C'est nul, et c'est le but : c'est le PLANCHER de mesure. Une IA qui ne bat
// pas celle-ci ne vaut rien ; une IA qui la bat 55 % du temps ne vaut pas
// beaucoup plus. Toute la campagne de mesure part de la.
//
// LE POINT DE CONCEPTION A COMPRENDRE
// -----------------------------------
// Cette IA ne sait PAS ce qu'est une duplication. Elle ne connait ni le plateau,
// ni les regles : elle recoit une `NkcRulesVTable` et un handle opaque, et elle
// traverse le moteur pour tout. Trois consequences voulues :
//
//   1. A2 demarre en semaine 1 sans attendre que A1 ait fini ;
//   2. quand A1 change son code interne, l'IA continue de fonctionner ;
//   3. les deux ne peuvent pas diverger sur les regles — il n'en existe qu'une
//      implementation, et l'IA la traverse.
//
// CONTRAINTE DE THREAD
// --------------------
// `ChooseMove` est appelee depuis un THREAD WORKER, jamais depuis le thread de
// rendu. Elle ne doit toucher que l'etat qu'on lui passe (c'est un CLONE prive),
// ne poser aucun verrou, et respecter `budgetMs`. Depasser le budget fige
// l'interface — c'est un echec, pas un detail.
//
// POUR L'UTILISER : copier ce fichier dans Build/ConquerorLab/ai/ et sauvegarder.
// =============================================================================

#include "Conqueror/ConquerorAIABI.h"
#include "Conqueror/ConquerorLog.h"

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
			uint64		rng = 0x243F6A8885A308D3ull;
			char		debug[256] = {};
	};

	/// PRNG xorshift64* PORTE PAR L'INSTANCE, jamais global. Sans cela, deux
	/// instances d'IA jouant en parallele dans une campagne piochent dans le
	/// meme flux, et deux parties de meme graine cessent d'etre reproductibles.
	uint64 NextRand(uint64 &s) {
		s ^= s >> 12;
		s ^= s << 25;
		s ^= s >> 27;
		return s * 2685821657736338717ull;
	}

	// =========================================================================
	NkcAI V_Create() {
		void *mem = RawAlloc(sizeof(AI));
		return mem ? static_cast<NkcAI>(new (mem) AI()) : nullptr;
	}

	void V_Destroy(NkcAI self) {
		if (!self) return;
		AI *a = static_cast<AI *>(self);
		a->~AI();
		RawFree(a);
	}

	/// Appelee au moins une fois avant le premier ChooseMove. L'IA doit pouvoir
	/// etre reconfiguree ENTRE deux coups sans etre recreee.
	void V_Configure(NkcAI self, const NkcAIConfig *cfg) {
		AI *a = static_cast<AI *>(self);
		if (!a || !cfg) return;
		a->cfg = *cfg;
		a->rng = cfg->seed ? cfg->seed : 0x243F6A8885A308D3ull;
	}

	/// Aucun reglage expose. Renvoyer "" est parfaitement legal : l'atelier
	/// n'affichera simplement aucun champ pour cette IA.
	const char *V_GetParamsSchemaJson(NkcAI) { return ""; }
	int32		V_SetParam(NkcAI, const char *, float64) { return 0; }

	/// LE CŒUR. Renvoie 1 et remplit `out` en cas de succes ; 0 si aucun coup
	/// legal n'existe — l'atelier joue alors PASSER lui-meme.
	int32 V_ChooseMove(NkcAI self, const NkcRulesVTable *R, NkcRules ri,
					   const NkcState st, NkcAIResult *out) {
		AI *a = static_cast<AI *>(self);
		if (!a || !R || !ri || !st || !out) return 0;

		// Remise a zero + champs « aucun » : le contrat compare les coups octet
		// a octet, un champ laisse au hasard rendrait le coup illegal.
		std::memset(out, 0, sizeof(*out));
		out->move.targetLevel = -1;
		out->move.powerId	  = -1;

		// TOUT passe par la table : on ne sait pas ce qu'on joue, on demande.
		NkcMove		 moves[kMoveCap];
		const uint32 total = R->GenerateLegalMoves(ri, st, moves, kMoveCap);
		const uint32 n	   = total < kMoveCap ? total : kMoveCap;
		if (n == 0) return 0;

		const uint32 pick = static_cast<uint32>(NextRand(a->rng) % n);
		out->move		  = moves[pick];
		out->simulations  = 1;
		out->depthReached = 0;
		out->scoreMilli	  = 0;

		std::snprintf(a->debug, sizeof(a->debug),
					  "{\"strategy\":\"random\",\"moves\":%u,\"picked\":%u}", n, pick);
		return 1;
	}

	/// Un coup a REELLEMENT ete joue en partie. Une IA a arbre (MCTS) s'en sert
	/// pour reutiliser son travail d'un tour a l'autre. Ici : rien a faire.
	void V_OnMovePlayed(NkcAI, const NkcMove *) {}

	/// Nouvelle partie : vider toute memoire inter-coups.
	void V_Reset(NkcAI) {}

	/// Consomme par l'atelier pour la heatmap et la surbrillance. Format libre,
	/// mais `{"moves":[{"q":..,"r":..,"visits":..,"value":..}]}` est celui que
	/// l'atelier saura afficher le jour ou il l'affichera.
	const char *V_GetDebugJson(NkcAI self) {
		AI *a = static_cast<AI *>(self);
		return a ? a->debug : "";
	}

	void FillFactory(NkcAIFactory *out) {
		if (!out) return;
		std::memset(out, 0, sizeof(*out));
		out->info.abiVersion = kAiAbiVersion;
		std::snprintf(out->info.name, sizeof(out->info.name), "IAMinimale");
		std::snprintf(out->info.version, sizeof(out->info.version), "1.0.0");
		std::snprintf(out->info.author, sizeof(out->info.author), "Cours ConquerorLab");
		out->info.difficultyCount = 1;	  // un seul palier : elle joue pareil partout
		out->info.isDeterministic = 1;	  // (seed, etat) -> toujours le meme coup
		out->info.isThreadSafe	  = 1;	  // plusieurs instances en parallele : OK

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

// =============================================================================
// Les DEUX symboles exportes. Meme piege que pour les regles : les oublier
// donne « Symbole introuvable » au chargement.
// =============================================================================
NKC_MODULE_EXPORT void nkc_ai_set_allocator(NkcAllocFn a, NkcFreeFn f) { gAlloc = a; gFree = f; }
NKC_MODULE_LOGGING(ai)   // <- une ligne, et NKC_LOG_* va dans le panneau « Sortie »
NKC_MODULE_EXPORT void nkc_ai_get_factory(NkcAIFactory *out) { FillFactory(out); }
