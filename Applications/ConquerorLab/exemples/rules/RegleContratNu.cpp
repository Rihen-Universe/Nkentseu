// =============================================================================
// RegleContratNu.cpp — LE PLUS PETIT moteur de regles qui joue vraiment.
//
// A QUOI CE FICHIER SERT
// ----------------------
// C'est le point de depart du stagiaire A1. Il implemente le contrat
// `NkcRulesVTable` en entier, mais avec les regles les plus simples possibles :
//
//     plateau      carre 5x5, 4 voisins (pas d'hexagone : une chose a la fois)
//     action       DUPLIQUER uniquement, portee 1
//     transformation  les ennemis voisins de la case OU L'ON VIENT DE POSER
//                     changent de camp — et eux seuls (pas de propagation)
//     fin          des qu'un joueur ne peut plus dupliquer
//     decompte     le plus de totems gagne
//
// C'est deja un jeu jouable, et c'est assez pour que l'atelier fasse tout ce
// qu'il sait faire : partie humaine, IA, journal, rejeu, campagne de mesure.
//
// CE QU'IL NE FAIT PAS, VOLONTAIREMENT
//   - pas de fusion, pas de pouvoirs, pas d'artefacts (palier 1 et 2) ;
//   - `LoadBoardJson` refuse : le plateau est fige dans le code. C'est
//     precisement ce que REGLES §4 interdit a un vrai moteur — voir la note en
//     bas de fichier — mais c'est un renoncement assume pour un exemple, et
//     l'atelier le signale proprement au lieu de planter.
//
// POUR L'UTILISER : copier ce fichier dans Build/ConquerorLab/rules/ et
// sauvegarder. L'atelier le compile et l'ajoute au menu.
//
// COMPILATION (ce que l'atelier fait pour vous) :
//   clang++ -shared -std=c++17 -O2 -fPIC -static \
//     -I<depot>/Applications/ConquerorLab/include \
//     -I<depot>/Kernel/Foundation/NKCore/src \
//     -I<depot>/Kernel/Foundation/NKPlatform/src \
//     -o RegleContratNu.dll RegleContratNu.cpp
// =============================================================================

#include "Conqueror/ConquerorRulesABI.h"
#include "Conqueror/ConquerorGeometry.h"
#include "Conqueror/ConquerorLog.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <new>

using namespace nkentseu;
using namespace nkentseu::conqueror;

namespace {

	// -------------------------------------------------------------------------
	// 1. MEMOIRE — jamais de new/delete bruts.
	// L'atelier peut injecter ses propres allocateurs (NKMemory) ; sans injection
	// on retombe sur malloc/free. Les deux chemins doivent exister.
	// -------------------------------------------------------------------------
	NkcAllocFn gAlloc = nullptr;
	NkcFreeFn  gFree  = nullptr;

	void *RawAlloc(usize n) { return gAlloc ? gAlloc(n) : std::malloc(n); }
	void  RawFree(void *p)  { if (!p) return; if (gFree) gFree(p); else std::free(p); }

	// -------------------------------------------------------------------------
	// 2. PARAMETRES — aucune constante en dur dans la logique (REGLES §1).
	// Tout est entier : « zero flottant dans la logique de regles » (§17.1).
	// -------------------------------------------------------------------------
	enum ParamId : int32 {
		P_PORTEE = 0,	  ///< distance maximale de duplication
		P_MAX_TOURS,	  ///< garde-fou de simulation, jamais une regle de jeu
		P_COUNT
	};

	struct ParamDesc { const char *key, *group; int32 def, lo, hi; };

	const ParamDesc kParams[P_COUNT] = {
		{"portee_duplication", "Duplication",   1,  1,   3},
		{"max_tours",		   "Fin de partie", 200, 10, 100000},
	};

	// -------------------------------------------------------------------------
	// 3. LE PLATEAU — 5x5 en coordonnees cartesiennes (q = x, r = y).
	// -------------------------------------------------------------------------
	constexpr int32 kSide  = 5;
	constexpr int32 kCells = kSide * kSide;

	// -------------------------------------------------------------------------
	// 4. L'ETAT. C'est VOTRE representation : le contrat ne l'impose pas, il ne
	// voit qu'une `NkcStateView`. Une seule contrainte : elle doit se copier
	// telle quelle (POD), parce que l'IA la clone des milliers de fois par
	// seconde et que CloneState ne doit rien allouer.
	// -------------------------------------------------------------------------
	struct State {
			NkcCellView cells[kCells];
			uint8		playerCount = 2;
			uint8		current		= 0;
			uint8		finished	= 0;
			int8		winner		= -2;	 ///< -2 = en cours, -1 = nul, >= 0 = joueur
			uint32		turn		= 0;
			int32		energy[kMaxPlayers]			= {};
			int32		conquestTenths[kMaxPlayers] = {};
			int32		totemCount[kMaxPlayers]		= {};
			uint64		rng = 0x9E3779B97F4A7C15ull;   ///< PRNG PORTE PAR L'ETAT (§17.2)
	};

	struct Rules {
			int32	 params[P_COUNT];
			NkcCoord coords[kCells];	 ///< coordonnee de chaque index de case
			char	 schemaBuf[1024];
			char	 boardBuf[4096];
	};

	// -------------------------------------------------------------------------
	// Outils internes
	// -------------------------------------------------------------------------
	int32 IndexOf(NkcCoord c) {
		if (c.q < 0 || c.q >= kSide || c.r < 0 || c.r >= kSide) return -1;
		return c.r * kSide + c.q;
	}

	void Recount(State *s) {
		for (uint32 p = 0; p < kMaxPlayers; ++p) s->totemCount[p] = 0;
		for (int32 i = 0; i < kCells; ++i) {
			const int8 o = s->cells[i].owner;
			if (o >= 0 && o < static_cast<int8>(kMaxPlayers)) s->totemCount[o]++;
		}
	}

	/// Ce joueur peut-il encore dupliquer ? C'est LA question de fin de partie.
	bool CanPlay(const Rules *R, const State *s, uint8 player) {
		const int32 nbr = NeighborCount(NkcTopology::Square4);
		for (int32 i = 0; i < kCells; ++i) {
			if (s->cells[i].owner != static_cast<int8>(player)) continue;
			for (int32 k = 0; k < nbr; ++k) {
				const int32 ni = IndexOf(Neighbor(NkcTopology::Square4, R->coords[i], k));
				if (ni >= 0 && s->cells[ni].owner == kCellEmpty) return true;
			}
		}
		return false;
	}

	void Emit(NkcEventSink sink, void *user, NkcEventKind kind, uint8 player,
			  NkcCoord a, NkcCoord b, int32 value) {
		if (!sink) return;
		NkcEvent ev;
		std::memset(&ev, 0, sizeof(ev));
		ev.kind = kind; ev.player = player; ev.a = a; ev.b = b; ev.value = value;
		sink(user, &ev);
	}

	// -------------------------------------------------------------------------
	// 5. GENERATION DES COUPS.
	// L'ORDRE EST NORMATIF : cases par index croissant, puis voisins dans
	// l'ordre de `Neighbor`. Deux executions doivent produire la MEME liste, sur
	// toute plateforme — sans quoi le rejeu et la mesure s'effondrent (§17.4).
	// -------------------------------------------------------------------------
	uint32 GenMoves(Rules *R, const State *s, NkcMove *out, uint32 cap) {
		if (s->finished) return 0;

		uint32		n	 = 0;
		const int32 nbr	 = NeighborCount(NkcTopology::Square4);
		const int32 range = R->params[P_PORTEE];
		const uint8 me	 = s->current;

		for (int32 i = 0; i < kCells; ++i) {
			if (s->cells[i].owner != static_cast<int8>(me)) continue;
			const NkcCoord src = R->coords[i];

			for (int32 k = 0; k < nbr; ++k) {
				const NkcCoord dst = Neighbor(NkcTopology::Square4, src, k);
				const int32	   di  = IndexOf(dst);
				if (di < 0) continue;
				if (s->cells[di].owner != kCellEmpty) continue;
				if (Distance(NkcTopology::Square4, src, dst) > range) continue;

				if (n < cap) {
					NkcMove m;
					// MISE A ZERO OBLIGATOIRE : le contrat compare les coups OCTET
					// A OCTET. Un champ inutilise laisse au hasard rendrait deux
					// coups identiques differents, et `IsLegalMove` echouerait.
					std::memset(&m, 0, sizeof(m));
					m.kind		  = NkcMoveKind::Duplicate;
					m.player	  = me;
					m.from		  = src;
					m.to		  = dst;
					m.targetLevel = -1;
					m.powerId	  = -1;
					out[n]		  = m;
				}
				++n;
			}
		}

		// PASSER n'est legal que si RIEN d'autre ne l'est (REGLES §6.2).
		if (n == 0) {
			if (cap > 0) {
				NkcMove m;
				std::memset(&m, 0, sizeof(m));
				m.kind = NkcMoveKind::Pass;
				m.player = me;
				m.targetLevel = -1;
				m.powerId = -1;
				out[0] = m;
			}
			return 1;
		}
		return n;
	}

	// -------------------------------------------------------------------------
	// 6. FIN DE PARTIE ET DECOMPTE.
	// -------------------------------------------------------------------------
	void CheckEnd(Rules *R, State *s, NkcEventSink sink, void *user) {
		const NkcCoord zero{};
		bool		   over = false;

		// Un joueur qui ne peut plus dupliquer arrete la partie (REGLES §12.1).
		for (uint8 p = 0; p < s->playerCount; ++p)
			if (!CanPlay(R, s, p)) {
				Emit(sink, user, NkcEventKind::PlayerBlocked, p, zero, zero, p);
				over = true;
				break;
			}

		// Garde-fou de SIMULATION : sans lui, une partie sur dix mille ne finit
		// jamais et le batch tourne pour toujours (REGLES §12.3).
		if (!over && static_cast<int32>(s->turn) >=
						 R->params[P_MAX_TOURS] * static_cast<int32>(s->playerCount))
			over = true;

		if (!over) return;

		s->finished	 = 1;
		int32 best	 = -1, bestCount = -1;
		bool  tie	 = false;
		for (uint8 p = 0; p < s->playerCount; ++p) {
			if (s->totemCount[p] > bestCount)		{ bestCount = s->totemCount[p]; best = p; tie = false; }
			else if (s->totemCount[p] == bestCount)	{ tie = true; }
		}
		s->winner = tie ? static_cast<int8>(-1) : static_cast<int8>(best);
		Emit(sink, user, NkcEventKind::GameOver, 0, zero, zero, s->winner);
	}

	// -------------------------------------------------------------------------
	// 7. APPLIQUER UN COUP. Renvoie 0 SANS RIEN MODIFIER si le coup est illegal :
	// c'est le contrat, et l'atelier compte dessus pour survivre a une IA fausse.
	// -------------------------------------------------------------------------
	int32 DoApply(Rules *R, State *s, const NkcMove *mv, NkcEventSink sink, void *user) {
		if (!mv || s->finished || mv->player != s->current) return 0;

		if (mv->kind == NkcMoveKind::Pass) {
			NkcMove probe[1];
			if (GenMoves(R, s, probe, 1) != 1 || probe[0].kind != NkcMoveKind::Pass) return 0;
			s->turn++;
			s->current = static_cast<uint8>((s->current + 1) % s->playerCount);
			CheckEnd(R, s, sink, user);
			return 1;
		}
		if (mv->kind != NkcMoveKind::Duplicate) return 0;

		const int32 si = IndexOf(mv->from);
		const int32 di = IndexOf(mv->to);
		if (si < 0 || di < 0) return 0;
		if (s->cells[si].owner != static_cast<int8>(mv->player)) return 0;
		if (s->cells[di].owner != kCellEmpty) return 0;
		if (Distance(NkcTopology::Square4, mv->from, mv->to) > R->params[P_PORTEE]) return 0;

		// --- placement : la source RESTE INTACTE (REGLES §7.2) ---------------
		s->cells[di].owner = static_cast<int8>(mv->player);
		s->cells[di].level = 0;
		Emit(sink, user, NkcEventKind::TotemDuplicated, mv->player, mv->from, mv->to, 0);
		s->conquestTenths[mv->player] += 10;   // +1,0 PC, en DIXIEMES entiers

		// --- transformation (REGLES §7.3) ------------------------------------
		// SEUL le totem qu'on vient de poser est declencheur. Les cascades
		// naissent de la MULTIPLICITE des voisins, pas d'une propagation. C'est
		// le point le plus facile a implementer de travers : si vous rappelez
		// cette boucle sur les cases retournees, tout le plateau bascule.
		const int32 nbr		= NeighborCount(NkcTopology::Square4);
		int32		flipped = 0;
		for (int32 k = 0; k < nbr; ++k) {
			const int32 ni = IndexOf(Neighbor(NkcTopology::Square4, mv->to, k));
			if (ni < 0) continue;
			const int8 o = s->cells[ni].owner;
			if (o < 0 || o == static_cast<int8>(mv->player)) continue;

			s->energy[mv->player] += 2;
			s->conquestTenths[mv->player] += 10;
			s->cells[ni].owner = static_cast<int8>(mv->player);
			s->cells[ni].level = 0;
			Emit(sink, user, NkcEventKind::TotemTransformed, mv->player, R->coords[ni], mv->to, o);
			++flipped;
		}
		if (flipped >= 2)
			Emit(sink, user, NkcEventKind::Cascade, mv->player, mv->to, mv->to, flipped);

		Recount(s);
		s->turn++;
		s->current = static_cast<uint8>((s->current + 1) % s->playerCount);
		CheckEnd(R, s, sink, user);
		return 1;
	}

	// =========================================================================
	// 8. LA TABLE DE FONCTIONS. Convention : 0 = echec, 1 = succes.
	// =========================================================================
	NkcRules V_Create() {
		void *mem = RawAlloc(sizeof(Rules));
		if (!mem) return nullptr;
		Rules *R = new (mem) Rules();
		for (int32 i = 0; i < P_COUNT; ++i) R->params[i] = kParams[i].def;
		for (int32 y = 0; y < kSide; ++y)
			for (int32 x = 0; x < kSide; ++x) {
				R->coords[y * kSide + x].q = static_cast<int16>(x);
				R->coords[y * kSide + x].r = static_cast<int16>(y);
			}
		return static_cast<NkcRules>(R);
	}

	void V_Destroy(NkcRules self) {
		if (!self) return;
		Rules *R = static_cast<Rules *>(self);
		R->~Rules();
		RawFree(R);
	}

	/// LE SCHEMA. C'est lui, et lui seul, qui fabrique le panneau « Regles » de
	/// l'atelier : ajoutez une ligne ici, le reglage apparait a l'ecran.
	const char *V_GetParamsSchemaJson(NkcRules self) {
		Rules *R	= static_cast<Rules *>(self);
		char  *w	= R->schemaBuf;
		usize  left = sizeof(R->schemaBuf);
		int32  k	= std::snprintf(w, left, "[");
		w += k; left -= static_cast<usize>(k);
		for (int32 i = 0; i < P_COUNT; ++i) {
			k = std::snprintf(w, left,
							  "%s{\"key\":\"%s\",\"group\":\"%s\",\"type\":\"int\","
							  "\"min\":%d,\"max\":%d,\"def\":%d,\"val\":%d}",
							  i ? "," : "", kParams[i].key, kParams[i].group,
							  kParams[i].lo, kParams[i].hi, kParams[i].def, R->params[i]);
			if (k <= 0 || static_cast<usize>(k) >= left) break;
			w += k; left -= static_cast<usize>(k);
		}
		std::snprintf(w, left, "]");
		return R->schemaBuf;
	}

	/// BORNER, ne pas refuser : le banc d'essai verifie qu'une valeur hors plage
	/// est ramenee dans les bornes, pas ignoree.
	int32 V_SetParam(NkcRules self, const char *key, float64 value) {
		Rules *R = static_cast<Rules *>(self);
		if (!key) return 0;
		for (int32 i = 0; i < P_COUNT; ++i) {
			if (std::strcmp(kParams[i].key, key) != 0) continue;
			int32 v = static_cast<int32>(value < 0 ? value - 0.5 : value + 0.5);
			if (v < kParams[i].lo) v = kParams[i].lo;
			if (v > kParams[i].hi) v = kParams[i].hi;
			R->params[i] = v;
			return 1;
		}
		return 0;
	}

	float64 V_GetParam(NkcRules self, const char *key) {
		Rules *R = static_cast<Rules *>(self);
		if (!key) return 0.0;
		for (int32 i = 0; i < P_COUNT; ++i)
			if (std::strcmp(kParams[i].key, key) == 0) return static_cast<float64>(R->params[i]);
		return 0.0;
	}

	/// Plateau fige : on REFUSE tout chargement. L'atelier l'affiche proprement
	/// (« le moteur a REFUSE ce plateau ») au lieu de faire semblant.
	/// Un vrai moteur DOIT l'implementer : voir ConquerorRulesV2.cpp.
	int32 V_LoadBoardJson(NkcRules, const char *) { return 0; }

	const char *V_GetBoardJson(NkcRules self) {
		Rules *R	= static_cast<Rules *>(self);
		char  *w	= R->boardBuf;
		usize  left = sizeof(R->boardBuf);
		int32  k	= std::snprintf(w, left, "{\"topology\":\"SQUARE_4\",\"cells\":[");
		w += k; left -= static_cast<usize>(k);
		for (int32 i = 0; i < kCells; ++i) {
			k = std::snprintf(w, left, "%s[%d,%d]", i ? "," : "", R->coords[i].q, R->coords[i].r);
			if (k <= 0 || static_cast<usize>(k) >= left) break;
			w += k; left -= static_cast<usize>(k);
		}
		std::snprintf(w, left,
					  "],\"blocked\":[],\"starts\":["
					  "{\"player\":0,\"q\":0,\"r\":0,\"level\":0},"
					  "{\"player\":1,\"q\":%d,\"r\":%d,\"level\":0}],"
					  "\"min_players\":2,\"max_players\":2}",
					  kSide - 1, kSide - 1);
		return R->boardBuf;
	}

	NkcState V_CreateState(NkcRules) {
		void *mem = RawAlloc(sizeof(State));
		return mem ? static_cast<NkcState>(new (mem) State()) : nullptr;
	}

	void V_DestroyState(NkcRules, NkcState st) {
		if (!st) return;
		State *s = static_cast<State *>(st);
		s->~State();
		RawFree(s);
	}

	/// CHEMIN CHAUD DE L'IA : une simple copie, aucune allocation.
	void V_CloneState(NkcRules, NkcState dst, const NkcState src) {
		if (dst && src) *static_cast<State *>(dst) = *static_cast<const State *>(src);
	}

	int32 V_Setup(NkcRules self, NkcState st, uint8 playerCount, uint64 seed) {
		Rules *R = static_cast<Rules *>(self);
		State *s = static_cast<State *>(st);
		if (!R || !s) return 0;
		if (playerCount < 2) playerCount = 2;
		if (playerCount > 2) playerCount = 2;	// cet exemple ne gere que le 1v1

		*s = State();
		s->playerCount = playerCount;
		s->rng		   = seed ? seed : 0x9E3779B97F4A7C15ull;
		for (int32 i = 0; i < kCells; ++i) {
			s->cells[i].owner	= kCellEmpty;
			s->cells[i].level	= 0;
			s->cells[i].people	= -1;
			s->cells[i].power	= -1;
			s->cells[i].artefact = -1;
		}
		s->cells[0].owner		   = 0;			// coin haut-gauche
		s->cells[kCells - 1].owner = 1;			// coin bas-droit
		Recount(s);

		// Une trace pour le panneau « Sortie » de l'atelier. Une par PARTIE, pas
		// une par coup : un journal qu'on doit filtrer pour lire n'est plus un
		// journal.
		NKC_LOG_INFO("nouvelle partie : %d cases, %d joueurs, graine %llu",
					 kCells, (int32)s->playerCount, (unsigned long long)seed);
		return 1;
	}

	/// LA VUE. Les pointeurs appartiennent AU MODULE et restent valides jusqu'au
	/// prochain appel mutant. L'appelant ne libere jamais rien.
	void V_GetView(NkcRules self, const NkcState st, NkcStateView *out) {
		Rules		*R = static_cast<Rules *>(self);
		const State *s = static_cast<const State *>(st);
		if (!out || !R || !s) return;
		out->cells			= s->cells;
		out->coords			= R->coords;
		out->cellCount		= kCells;
		out->topology		= NkcTopology::Square4;
		out->playerCount	= s->playerCount;
		out->current		= s->current;
		out->finished		= s->finished;
		out->winner			= s->winner;
		out->turn			= s->turn;
		out->energy			= s->energy;
		out->conquestTenths = s->conquestTenths;
		out->totemCount		= s->totemCount;
	}

	uint32 V_GenerateLegalMoves(NkcRules self, const NkcState st, NkcMove *out, uint32 cap) {
		return GenMoves(static_cast<Rules *>(self), static_cast<const State *>(st), out, cap);
	}

	int32 V_IsLegalMove(NkcRules self, const NkcState st, const NkcMove *mv) {
		if (!mv) return 0;
		NkcMove		 buf[kCells * 4 + 1];
		const uint32 total = GenMoves(static_cast<Rules *>(self), static_cast<const State *>(st),
									  buf, static_cast<uint32>(kCells * 4 + 1));
		const uint32 lim = total < kCells * 4 + 1 ? total : kCells * 4 + 1;
		for (uint32 i = 0; i < lim; ++i)
			if (std::memcmp(&buf[i], mv, sizeof(NkcMove)) == 0) return 1;
		return 0;
	}

	int32 V_ApplyMove(NkcRules self, NkcState st, const NkcMove *mv, NkcEventSink sink, void *user) {
		return DoApply(static_cast<Rules *>(self), static_cast<State *>(st), mv, sink, user);
	}

	int32 V_IsFinished(NkcRules, const NkcState st) {
		return static_cast<const State *>(st)->finished ? 1 : 0;
	}

	int32 V_GetWinner(NkcRules, const NkcState st) {
		return static_cast<int32>(static_cast<const State *>(st)->winner);
	}

	/// DOIT equivaloir a « aucun coup legal hors PASSER ». Le banc d'essai
	/// l'asserte : c'est le meilleur test d'integrite du generateur de coups.
	int32 V_IsPlayerBlocked(NkcRules self, const NkcState st, uint8 player) {
		Rules		*R = static_cast<Rules *>(self);
		const State *s = static_cast<const State *>(st);
		if (player >= s->playerCount) return 1;
		return CanPlay(R, s, player) ? 0 : 1;
	}

	uint32 V_SerializeState(NkcRules, const NkcState st, void *buf, uint32 cap) {
		const uint32 need = static_cast<uint32>(sizeof(State));
		if (!buf || cap < need) return need;   // renvoie la taille NECESSAIRE
		std::memcpy(buf, st, need);
		return need;
	}

	int32 V_DeserializeState(NkcRules, NkcState st, const void *buf, uint32 size) {
		if (!buf || size != static_cast<uint32>(sizeof(State))) return 0;
		std::memcpy(st, buf, size);
		return 1;
	}

	/// FNV-1a 64 sur les champs SIGNIFIANTS, dans un ordre fixe. Deux etats
	/// identiques doivent donner la meme empreinte sur toute plateforme : c'est
	/// l'outil de diagnostic du determinisme.
	uint64 V_HashState(NkcRules, const NkcState st) {
		const State *s = static_cast<const State *>(st);
		uint64		 h = 1469598103934665603ull;
		auto		 mix = [&h](uint64 v) {
			for (int32 b = 0; b < 8; ++b) { h ^= (v >> (b * 8)) & 0xFF; h *= 1099511628211ull; }
		};
		for (int32 i = 0; i < kCells; ++i)
			mix(static_cast<uint64>(static_cast<uint8>(s->cells[i].owner)));
		mix(s->current);
		mix(s->turn);
		return h;
	}

	void FillFactory(NkcRulesFactory *out) {
		if (!out) return;
		std::memset(out, 0, sizeof(*out));
		out->info.abiVersion = kRulesAbiVersion;
		std::snprintf(out->info.name, sizeof(out->info.name), "RegleContratNu");
		std::snprintf(out->info.version, sizeof(out->info.version), "1.0.0");
		std::snprintf(out->info.author, sizeof(out->info.author), "Cours ConquerorLab");
		out->info.supportsHex	 = 0;
		out->info.supportsSquare = 1;
		out->info.maxPlayers	 = 2;
		out->info.palier		 = 0;

		out->vtable.Create				= V_Create;
		out->vtable.Destroy				= V_Destroy;
		out->vtable.GetParamsSchemaJson	= V_GetParamsSchemaJson;
		out->vtable.SetParam			= V_SetParam;
		out->vtable.GetParam			= V_GetParam;
		out->vtable.LoadBoardJson		= V_LoadBoardJson;
		out->vtable.GetBoardJson		= V_GetBoardJson;
		out->vtable.CreateState			= V_CreateState;
		out->vtable.DestroyState		= V_DestroyState;
		out->vtable.CloneState			= V_CloneState;
		out->vtable.Setup				= V_Setup;
		out->vtable.GetView				= V_GetView;
		out->vtable.GenerateLegalMoves	= V_GenerateLegalMoves;
		out->vtable.IsLegalMove			= V_IsLegalMove;
		out->vtable.ApplyMove			= V_ApplyMove;
		out->vtable.IsFinished			= V_IsFinished;
		out->vtable.GetWinner			= V_GetWinner;
		out->vtable.IsPlayerBlocked		= V_IsPlayerBlocked;
		out->vtable.SerializeState		= V_SerializeState;
		out->vtable.DeserializeState	= V_DeserializeState;
		out->vtable.HashState			= V_HashState;
	}

} // namespace

// =============================================================================
// 9. LES DEUX SYMBOLES EXPORTES. Sans eux, l'atelier charge le binaire puis
// affiche « Symbole introuvable — as-tu bien termine ton fichier par la macro
// d'export ? ». C'est la faute la plus frequente du premier jour.
// =============================================================================
NKC_MODULE_EXPORT void nkc_rules_set_allocator(NkcAllocFn a, NkcFreeFn f) { gAlloc = a; gFree = f; }
NKC_MODULE_LOGGING(rules)   // <- une ligne, et NKC_LOG_* va dans le panneau « Sortie »
NKC_MODULE_EXPORT void nkc_rules_get_factory(NkcRulesFactory *out) { FillFactory(out); }
