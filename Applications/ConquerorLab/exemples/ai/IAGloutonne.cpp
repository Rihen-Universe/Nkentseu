// =============================================================================
// IAGloutonne.cpp — l'IA qui REGARDE UN COUP.
//
// OU ELLE SE SITUE
// ----------------
//   IAMinimale.cpp    prend un coup au hasard              ~120 lignes
//   IAGloutonne.cpp   essaie chaque coup, garde le meilleur  CE FICHIER
//   IANegamax.cpp     regarde plusieurs coups d'avance      ~500 lignes
//
// Lisez-les dans cet ordre. Celui-ci introduit LA seule idee dont tout le reste
// decoule : pour choisir, il faut savoir COMPARER deux positions.
//
// L'ALGORITHME TIENT EN QUATRE LIGNES
// -----------------------------------
//   pour chaque coup legal :
//       cloner l'etat, jouer le coup pour de faux
//       donner une note a la position obtenue
//   garder le coup dont la note est la meilleure
//
// C'est tout. Pas d'arbre, pas de recursion, pas de budget de temps a gerer :
// le nombre de coups est borne, donc la duree l'est aussi.
//
// CE QU'ELLE NE SAIT PAS FAIRE, ET C'EST LE POINT
// -----------------------------------------------
// Elle ne voit pas la REPONSE de l'adversaire. Un coup qui gagne trois totems et
// en offre cinq au coup suivant lui parait excellent. C'est exactement le manque
// que negamax vient combler — et le meilleur moyen de comprendre pourquoi
// negamax existe est d'avoir d'abord vu jouer celle-ci.
//
// POUR L'UTILISER : copier dans travail/ai/ et sauvegarder.
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

	constexpr int32 kMaxMoves = 512;

	struct AI {
			NkcState essai	  = nullptr;   ///< l'etat sur lequel on joue « pour de faux »
			NkcMove	 moves[kMaxMoves];
			int32	 poidsTotems = 100;	   ///< un seul reglage, pour commencer
			char	 schema[256];
			char	 debug[256];
			uint32	 dernierExamine = 0;
	};

	// -------------------------------------------------------------------------
	// NOTER UNE POSITION.
	//
	// Du point de vue de `moi`. Positif = bon pour lui.
	//
	// Un seul critere : le nombre de totems. C'est volontairement pauvre — le
	// but est de montrer OU se branche l'evaluation, pas d'en ecrire une bonne.
	// Ajouter des criteres est le premier exercice, et c'est le vrai travail
	// d'une IA de jeu.
	// -------------------------------------------------------------------------
	int32 Noter(const NkcRulesVTable *rules, NkcRules inst, const NkcState st, uint8 moi,
				int32 poids) {
		NkcStateView v;
		std::memset(&v, 0, sizeof(v));
		rules->GetView(inst, st, &v);

		// Position terminale : on ne note pas, on constate.
		if (v.finished) {
			const int32 gagnant = rules->GetWinner(inst, st);
			if (gagnant == static_cast<int32>(moi)) return 1000000;
			if (gagnant == -1)						return 0;
			return -1000000;
		}

		int32 miens = 0, siens = 0;
		if (v.totemCount) {
			for (uint8 p = 0; p < v.playerCount; ++p) {
				if (p == moi) miens += v.totemCount[p];
				else		  siens += v.totemCount[p];
			}
		}
		return (miens - siens) * poids;
	}

	// -------------------------------------------------------------------------
	// La vtable. Neuf fonctions, dont quatre ne font rien : c'est normal, le
	// contrat prevoit large pour des IA qui gardent une memoire entre les coups.
	// -------------------------------------------------------------------------
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

	void V_Configure(NkcAI, const NkcAIConfig *) {
		// Cette IA n'a qu'une force. Declarer `difficultyCount = 1` (plus bas)
		// est la facon HONNETE de le dire : l'atelier n'affichera pas cinq
		// paliers qui donnent tous le meme jeu.
	}

	const char *V_GetParamsSchemaJson(NkcAI self) {
		AI *a = static_cast<AI *>(self);
		std::snprintf(a->schema, sizeof(a->schema),
					  "[{\"key\":\"poids_totems\",\"label\":\"Poids des totems\","
					  "\"group\":\"Evaluation\",\"type\":\"int\","
					  "\"min\":0,\"max\":1000,\"def\":100,\"val\":%d}]",
					  a->poidsTotems);
		return a->schema;
	}

	int32 V_SetParam(NkcAI self, const char *key, float64 value) {
		AI *a = static_cast<AI *>(self);
		if (!a || !key || std::strcmp(key, "poids_totems") != 0) return 0;
		int32 v = static_cast<int32>(value < 0 ? value - 0.5 : value + 0.5);
		// BORNER, pas refuser : le contrat le demande et le banc d'essai le teste.
		if (v < 0)	  v = 0;
		if (v > 1000) v = 1000;
		a->poidsTotems = v;
		return 1;
	}

	// -------------------------------------------------------------------------
	// LE COEUR — et il fait vingt lignes.
	// -------------------------------------------------------------------------
	int32 V_ChooseMove(NkcAI self, const NkcRulesVTable *rules, NkcRules inst,
					   const NkcState st, NkcAIResult *out) {
		AI *a = static_cast<AI *>(self);
		if (!a || !rules || !inst || !st || !out) return 0;
		std::memset(out, 0, sizeof(*out));

		const uint32 total = rules->GenerateLegalMoves(inst, st, a->moves, kMaxMoves);
		const uint32 n	   = total < kMaxMoves ? total : kMaxMoves;
		if (n == 0) return 0;	// aucun coup : l'atelier jouera PASSER

		NkcStateView v;
		std::memset(&v, 0, sizeof(v));
		rules->GetView(inst, st, &v);
		const uint8 moi = v.current;

		// L'etat d'essai est cree A LA PREMIERE REFLEXION : `Create` ne recoit pas
		// `rules`, donc il ne PEUT pas le faire. C'est une contrainte du contrat,
		// pas un choix.
		if (!a->essai) a->essai = rules->CreateState(inst);
		if (!a->essai) return 0;

		int32  meilleurNote = 0;
		uint32 meilleur		= 0;
		bool   premier		= true;

		for (uint32 i = 0; i < n; ++i) {
			// JOUER POUR DE FAUX : on clone, on applique, on note. L'etat recu
			// n'est JAMAIS modifie — le contrat l'interdit, et l'atelier compte
			// dessus pour faire tourner plusieurs parties en parallele.
			rules->CloneState(inst, a->essai, st);
			if (!rules->ApplyMove(inst, a->essai, &a->moves[i], nullptr, nullptr))
				continue;   // le moteur a refuse : il a toujours le dernier mot

			const int32 note = Noter(rules, inst, a->essai, moi, a->poidsTotems);

			// `>` et non `>=` : a note egale on garde le PREMIER. L'ordre des
			// coups est fixe par le moteur, donc ce choix est REPRODUCTIBLE —
			// c'est ce qui permet de rejouer une partie a l'identique.
			if (premier || note > meilleurNote) {
				meilleurNote = note;
				meilleur	 = i;
				premier		 = false;
			}
		}

		out->move		 = a->moves[meilleur];
		out->scoreMilli	 = meilleurNote / 100;	 // en milliemes, borne par l'atelier
		out->simulations = n;					 // « simulations » = coups essayes
		out->depthReached = 1;					 // un coup d'avance, et c'est tout
		a->dernierExamine = n;
		return 1;
	}

	void V_OnMovePlayed(NkcAI, const NkcMove *) {
		// Rien a faire : cette IA ne garde aucune memoire entre deux coups.
	}

	void V_Reset(NkcAI) {
		// Rien non plus, pour la meme raison.
	}

	const char *V_GetDebugJson(NkcAI self) {
		AI *a = static_cast<AI *>(self);
		std::snprintf(a->debug, sizeof(a->debug), "{\"coups_examines\":%u}",
					  static_cast<unsigned>(a->dernierExamine));
		return a->debug;
	}

	void FillFactory(NkcAIFactory *out) {
		if (!out) return;
		std::memset(out, 0, sizeof(*out));
		std::snprintf(out->info.name, sizeof(out->info.name), "IAGloutonne");
		std::snprintf(out->info.version, sizeof(out->info.version), "1.0.0");
		std::snprintf(out->info.author, sizeof(out->info.author), "Cours ConquerorLab");
		out->info.difficultyCount = 1;	 // une seule force, et on le dit
		out->info.isDeterministic = 1;	 // aucun aleatoire
		out->info.isThreadSafe	  = 1;	 // aucun etat global

		out->vtable.Create				= V_Create;
		out->vtable.Destroy				= V_Destroy;
		out->vtable.Configure			= V_Configure;
		out->vtable.GetParamsSchemaJson	= V_GetParamsSchemaJson;
		out->vtable.SetParam			= V_SetParam;
		out->vtable.ChooseMove			= V_ChooseMove;
		out->vtable.OnMovePlayed		= V_OnMovePlayed;
		out->vtable.Reset				= V_Reset;
		out->vtable.GetDebugJson		= V_GetDebugJson;

		NkcAIStamp(out);
	}

} // namespace

NKC_MODULE_EXPORT void nkc_ai_set_allocator(NkcAllocFn a, NkcFreeFn f) { gAlloc = a; gFree = f; }
NKC_MODULE_EXPORT void nkc_ai_get_factory(NkcAIFactory *out) { FillFactory(out); }
