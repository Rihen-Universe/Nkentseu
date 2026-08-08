#pragma once
// =============================================================================
// ConquerorAIABI.h — contrat stable entre l'atelier de test et une IA
//                    adversaire de Conqueror.
//
// COMMENT LE STAGIAIRE S'EN SERT
// ------------------------------
// Comme pour les regles : il n'ecrit PAS de DLL. Il depose un .cpp dans
//
//     Build/ConquerorLab/ai/mon_ia.cpp
//
// L'atelier le compile, le charge, et l'ajoute au menu deroulant des joueurs.
//
// POINT DE CONCEPTION CENTRAL
// ---------------------------
// L'IA ne connait PAS l'implementation du moteur de regles : elle recoit une
// NkcRulesVTable et un handle opaque. Consequences voulues :
//
//   1. Le sujet A2 demarre en semaine 1 sur un module bouchon, sans attendre A1.
//   2. Quand A1 change son code interne, l'IA continue de fonctionner.
//   3. Les deux ne peuvent pas diverger sur les regles : il n'en existe qu'une
//      implementation, et l'IA la traverse.
//
// CONTRAINTE DE THREAD (SPIKE Feature #4)
// ---------------------------------------
// ChooseMove est appelee depuis un THREAD WORKER, jamais depuis le thread de
// rendu. Elle doit :
//   - ne toucher que l'etat qu'on lui passe (c'est un CLONE prive) ;
//   - ne poser aucun verrou sur une ressource partagee ;
//   - respecter `budgetMs` : au-dela, elle rend le meilleur coup trouve
//     jusque-la. Depasser le budget fige l'interface — c'est un echec.
// =============================================================================

#include "Conqueror/ConquerorRulesABI.h"

namespace nkentseu {
	namespace conqueror {

		// Meme schema que les regles : MAJEURE = rupture, MINEURE = ajout en fin.
		// Voir le long commentaire de ConquerorRulesABI.h — il vaut pour les deux.
		inline constexpr uint32 kAiAbiMajor = 2;
		inline constexpr uint32 kAiAbiMinor = 1;
		inline constexpr uint32 kAiAbiVersion = kAiAbiMajor;

		/// Cinq paliers (fiche A2 §4.1.3). Reperes, pas implementation imposee :
		/// c'est au module de decider comment il module sa force (budget de
		/// simulations, profondeur, bruit, erreurs volontaires).
		enum class NkcDifficulty : uint8 {
			Easy   = 0,
			Normal = 1,
			Hard   = 2,
			Expert = 3,
			Apex   = 4,
			Count  = 5
		};

		using NkcAI = void *;  ///< handle opaque d'instance d'IA

		struct NkcAIInfo {
				uint32 abiVersion  = kAiAbiVersion;
				char   name[64]	   = {};
				char   version[16] = {};
				char   author[64]  = {};
				uint8  difficultyCount = 1;	 ///< paliers reellement distincts
				uint8  isDeterministic = 0;	 ///< 1 si (seed, etat) -> toujours le meme coup
				uint8  isThreadSafe	   = 0;	 ///< 1 si plusieurs instances en parallele
				uint8  _pad			   = 0;

		};

		/// Configuration d'une reflexion. L'IA doit pouvoir etre reconfiguree
		/// entre deux coups sans etre recreee.
		struct NkcAIConfig {
				NkcDifficulty difficulty = NkcDifficulty::Normal;
				uint8		  _pad[3]	 = {};
				uint32		  budgetMs	 = 1000;  ///< 0 = illimite
				uint32		  maxSimulations = 0; ///< 0 = illimite
				uint64		  seed		 = 0;	  ///< graine du PRNG de l'IA
		};

		/// Retour d'une reflexion. `scoreMilli` est en MILLIEMES (aucun flottant
		/// a la frontiere) : -1000 = position perdue, +1000 = gagnee.
		struct NkcAIResult {
				NkcMove move;
				int32	scoreMilli	 = 0;
				uint32	simulations	 = 0;  ///< simulations/noeuds reellement faits
				uint32	elapsedMs	 = 0;
				uint32	depthReached = 0;
				uint8	hitBudget	 = 0;  ///< 1 si coupee par le budget
				uint8	_pad[3]		 = {};
		};

		struct NkcAIVTable {

				// ---- cycle de vie ------------------------------------------
				NkcAI (*Create)() = nullptr;
				void (*Destroy)(NkcAI self) = nullptr;

				/// Appelee au moins une fois avant le premier ChooseMove.
				void (*Configure)(NkcAI self, const NkcAIConfig *cfg) = nullptr;

				/// Reglages libres du module (poids d'evaluation, constante UCT...),
				/// exposes comme ceux du moteur de regles : meme format de schema,
				/// meme rendu automatique dans l'atelier. Peut renvoyer "".
				const char *(*GetParamsSchemaJson)(NkcAI self) = nullptr;
				int32 (*SetParam)(NkcAI self, const char *key, float64 value) = nullptr;

				// ---- reflexion ----------------------------------------------
				/// `rules` + `rulesInst` donnent acces aux regles ; `st` est un etat
				/// PRIVE que l'IA peut cloner a volonte mais ne doit PAS muter
				/// directement (elle passe par rules->ApplyMove sur ses clones).
				///
				/// Renvoie 1 et remplit `out` en cas de succes ; 0 si aucun coup
				/// legal n'existe — l'atelier joue alors PASSER.
				///
				/// Le coup renvoye DOIT etre legal : l'atelier le valide
				/// systematiquement et signale toute violation.
				int32 (*ChooseMove)(NkcAI self, const NkcRulesVTable *rules,
									NkcRules rulesInst, const NkcState st,
									NkcAIResult *out) = nullptr;

				/// Signale qu'un coup a reellement ete joue en partie : permet la
				/// reutilisation d'arbre entre deux tours (MCTS). Peut ne rien faire.
				void (*OnMovePlayed)(NkcAI self, const NkcMove *mv) = nullptr;

				/// Vide toute memoire inter-coups (nouvelle partie).
				void (*Reset)(NkcAI self) = nullptr;

				/// JSON decrivant la derniere reflexion, consomme par l'atelier pour
				/// la heatmap et la surbrillance :
				///   {"moves":[{"q":0,"r":0,"visits":812,"value":617}, ...]}
				/// `value` en milliemes. Valide jusqu'au prochain ChooseMove.
				const char *(*GetDebugJson)(NkcAI self) = nullptr;
		};

		struct NkcAIFactory {
				NkcAIInfo	info;
				NkcAIVTable vtable;

				// ---- ajoute en MINEURE 1 ------------------------------------
				// APRES la vtable, jamais dans `info` : voir le commentaire de
				// NkcRulesFactory. Grossir `info` decalerait `vtable`.
				uint32 abiMinor	   = kAiAbiMinor;
				uint32 vtableBytes = 0;	 ///< rempli par NkcAIStamp
		};

		/// Equivalent de NkcRulesStamp. A appeler en fin de FillFactory.
		inline void NkcAIStamp(NkcAIFactory *out) noexcept {
			if (!out) return;
			out->info.abiVersion  = kAiAbiMajor;
			out->abiMinor	 = kAiAbiMinor;
			out->vtableBytes = static_cast<uint32>(sizeof(NkcAIVTable));
		}

		using NkcAIGetFactoryFn	  = void (*)(NkcAIFactory *out);
		using NkcAISetAllocatorFn = void (*)(NkcAllocFn a, NkcFreeFn f);
		using NkcAISetLoggerFn	  = void (*)(NkcLogFn fn, void *user, const char *name);

	} // namespace conqueror
} // namespace nkentseu

#define NKC_AI_SYM_GET_FACTORY "nkc_ai_get_factory"
#define NKC_AI_SYM_SET_ALLOC   "nkc_ai_set_allocator"
#define NKC_AI_SYM_SET_LOGGER  "nkc_ai_set_logger"
