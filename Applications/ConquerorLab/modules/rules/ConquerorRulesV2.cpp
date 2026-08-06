// =============================================================================
// ConquerorRulesV2.cpp — implementation de reference des regles Conqueror v2.0
//                        PALIER 0 (noyau) : plateau + DUPLIQUER + transformation
//                        + fin par blocage + decompte.
//
// C'est le module que le stagiaire A1 remplacera par le sien. Il sert de :
//   - preuve que le contrat est implementable ;
//   - adversaire de reference pour comparer une nouvelle implementation ;
//   - exemple de style : zero flottant dans la logique, PRNG porte par l'etat,
//     ordre de generation normatif.
//
// COMPILATION — le stagiaire ne fait RIEN de special : il depose son .cpp dans
// Build/ConquerorLab/rules/ et l'atelier compile. Ce fichier-ci est en plus
// compile DANS l'application (-DNKC_RULES_STATIC=1) pour qu'Android, qui n'a pas
// de compilateur embarque, dispose quand meme d'un moteur.
//
// Reference : REGLES_COMPLETES_v2.md §4, §5, §7, §11, §12, §14, §17.
// =============================================================================

#include "Conqueror/ConquerorRulesABI.h"
#include "Conqueror/ConquerorGeometry.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <new>

using namespace nkentseu;
using namespace nkentseu::conqueror;

namespace {

	// -------------------------------------------------------------------------
	// Hooks memoire — jamais de new/delete bruts.
	// -------------------------------------------------------------------------
	NkcAllocFn gAlloc = nullptr;
	NkcFreeFn  gFree  = nullptr;

	void *RawAlloc(usize n) { return gAlloc ? gAlloc(n) : std::malloc(n); }
	void  RawFree(void *p)  { if (!p) return; if (gFree) gFree(p); else std::free(p); }

	// -------------------------------------------------------------------------
	// Parametres (REGLES §14). Tout en int32 : aucun flottant ne traverse la
	// logique (§17.1). Les Points de Conquete sont en DIXIEMES.
	// -------------------------------------------------------------------------
	enum ParamId : int32 {
		P_TOPOLOGIE = 0,
		P_NB_JOUEURS,
		P_PORTEE_DUPLICATION,
		P_NIVEAU_TOTEM_DUPLIQUE,
		P_SOURCE_RESTE_INTACTE,
		P_NIVEAU_APRES_TRANSFORMATION,
		P_ENERGIE_PAR_NIVEAU,
		P_PC_BASE_DUPLICATION,
		P_PC_BONUS_NIVEAU,
		P_PC_BONUS_ENNEMI_ADJ,
		P_PC_BONUS_ALLIE_ADJ,
		P_FIN_SUR_BLOCAGE,
		P_MAX_TOURS,
		P_COUNT
	};

	struct ParamDesc {
			const char *key;
			const char *group;
			int32		def, lo, hi;
	};

	const ParamDesc kParams[P_COUNT] = {
		{"topologie",					"Plateau",		 0,	  0, 3},
		{"nb_joueurs",					"Plateau",		 2,	  2, static_cast<int32>(kMaxPlayers)},
		{"portee_duplication",			"Duplication",	 1,	  1, 3},
		{"niveau_totem_duplique",		"Duplication",	 0,	  0, 4},
		{"source_reste_intacte",		"Duplication",	 1,	  0, 1},
		{"niveau_apres_transformation", "Transformation", 0,  0, 4},
		{"energie_par_niveau",			"Ressources",	 2,	  0, 5},
		{"pc_base_duplication",			"Ressources",	10,	  0, 100},
		{"pc_bonus_niveau",				"Ressources",	 5,	  0, 100},
		{"pc_bonus_ennemi_adj",			"Ressources",	 2,	  0, 100},
		{"pc_bonus_allie_adj",			"Ressources",	 1,	  0, 100},
		{"fin_sur_blocage",				"Fin de partie", 0,	  0, 1},
		{"max_tours",					"Fin de partie", 200, 10, 100000},
	};

	// -------------------------------------------------------------------------
	// Plateau : une DONNEE, pas une constante (REGLES §4).
	// -------------------------------------------------------------------------
	struct StartPos {
			uint8	 player = 0;
			NkcCoord c;
			int8	 level = 0;
	};

	struct Board {
			NkcCoord cells[kMaxCells];
			uint8	 blocked[kMaxCells] = {};
			uint32	 cellCount			= 0;
			StartPos starts[kMaxPlayers * 8];
			uint32	 startCount = 0;
			uint8	 minPlayers = 2;
			uint8	 maxPlayers = 2;
	};

	// -------------------------------------------------------------------------
	// Etat de partie. Le PRNG vit ICI (§17.2). Au palier 0 il n'est pas encore
	// utilise — aucun aleatoire dans Conqueror a ce stade — mais il est deja
	// serialise pour que le rejeu reste valide quand les artefacts arriveront.
	// -------------------------------------------------------------------------
	struct State {
			NkcCellView cells[kMaxCells];
			uint32		cellCount = 0;

			uint8  playerCount = 2;
			uint8  current	   = 0;
			uint8  finished	   = 0;
			int8   winner	   = -2;
			uint32 turn		   = 0;

			int32 energy[kMaxPlayers]		  = {};
			int32 conquestTenths[kMaxPlayers] = {};
			int32 totemCount[kMaxPlayers]	  = {};
			uint8 eliminated[kMaxPlayers]	  = {};

			uint64 rng = 0x9E3779B97F4A7C15ull;
	};

	struct Rules {
			int32 params[P_COUNT];
			Board board;
			char  schemaBuf[4096];
			char  boardBuf[16384];
	};

	// -------------------------------------------------------------------------
	inline NkcTopology Topo(const Rules *R) {
		return static_cast<NkcTopology>(R->params[P_TOPOLOGIE]);
	}

	int32 FindCell(const Rules *R, NkcCoord c) {
		for (uint32 i = 0; i < R->board.cellCount; ++i)
			if (CoordEqual(R->board.cells[i], c)) return static_cast<int32>(i);
		return -1;
	}

	void Recount(State *s) {
		for (uint32 p = 0; p < kMaxPlayers; ++p) s->totemCount[p] = 0;
		for (uint32 i = 0; i < s->cellCount; ++i) {
			const int8 o = s->cells[i].owner;
			if (o >= 0 && o < static_cast<int8>(kMaxPlayers)) s->totemCount[o]++;
		}
	}

	/// Plateau par defaut : hexagone 6x7 pointy-top, 42 cases, 2 joueurs en
	/// diagonale opposee (REGLES §4.2).
	void BuildDefaultBoard(Rules *R) {
		Board &B	 = R->board;
		B.cellCount	 = 0;
		B.startCount = 0;
		for (int32 row = 0; row < 7; ++row) {
			for (int32 col = 0; col < 6; ++col) {
				if (B.cellCount >= kMaxCells) break;
				NkcCoord c;
				c.q						= static_cast<int16>(col - (row >> 1));	 // odd-r -> axial
				c.r						= static_cast<int16>(row);
				B.blocked[B.cellCount]	= 0;
				B.cells[B.cellCount++]	= c;
			}
		}
		B.minPlayers = 2;
		B.maxPlayers = 2;

		const int32 quads[4]  = {0, static_cast<int32>(B.cellCount) - 1, 5,
								 static_cast<int32>(B.cellCount) - 6};
		const uint8 owners[4] = {0, 0, 1, 1};
		for (int32 i = 0; i < 4; ++i) {
			StartPos sp;
			sp.player				 = owners[i];
			sp.c					 = B.cells[quads[i]];
			sp.level				 = 0;
			B.starts[B.startCount++] = sp;
		}
	}

	// -------------------------------------------------------------------------
	// Mini-scanner JSON : lit UNIQUEMENT le format de plateau du contrat.
	// Pas de bibliotheque, pas d'allocation.
	// -------------------------------------------------------------------------
	const char *SkipWs(const char *p) {
		while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',') ++p;
		return p;
	}

	const char *FindKey(const char *json, const char *key) {
		char pat[64];
		std::snprintf(pat, sizeof(pat), "\"%s\"", key);
		const char *p = std::strstr(json, pat);
		if (!p) return nullptr;
		p = std::strchr(p + std::strlen(pat), ':');
		return p ? SkipWs(p + 1) : nullptr;
	}

	bool ReadInt(const char *&p, int32 &out) {
		p = SkipWs(p);
		if (*p == ']' || *p == '}' || *p == 0) return false;
		char *end	= nullptr;
		const long v = std::strtol(p, &end, 10);
		if (end == p) return false;
		out = static_cast<int32>(v);
		p	= end;
		return true;
	}

	// -------------------------------------------------------------------------
	// Generation des coups. ORDRE NORMATIF (§17.4) : cases dans l'ordre d'index
	// croissant, puis voisins dans l'ordre de Neighbor(). Ne jamais reordonner.
	// -------------------------------------------------------------------------
	uint32 GenMoves(Rules *R, const State *s, NkcMove *out, uint32 cap) {
		uint32 n = 0;
		if (s->finished) return 0;

		const NkcTopology topo	= Topo(R);
		const int32		  range = R->params[P_PORTEE_DUPLICATION];
		const int32		  nbr	= NeighborCount(topo);
		const uint8		  me	= s->current;

		for (uint32 i = 0; i < s->cellCount; ++i) {
			if (s->cells[i].owner != static_cast<int8>(me)) continue;
			const NkcCoord src = R->board.cells[i];

			for (int32 k = 0; k < nbr; ++k) {
				const NkcCoord dst = Neighbor(topo, src, k);
				const int32	   di  = FindCell(R, dst);
				if (di < 0) continue;
				if (R->board.blocked[di]) continue;
				if (s->cells[di].owner != kCellEmpty) continue;
				if (Distance(topo, src, dst) > range) continue;

				if (n < cap) {
					NkcMove m;
					std::memset(&m, 0, sizeof(m));	// comparaison octet a octet
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

		// PASSER : legal uniquement si aucun autre coup ne l'est (REGLES §6.2).
		if (n == 0) {
			if (cap > 0) {
				NkcMove m;
				std::memset(&m, 0, sizeof(m));
				m.kind		  = NkcMoveKind::Pass;
				m.player	  = me;
				m.targetLevel = -1;
				m.powerId	  = -1;
				out[0]		  = m;
			}
			return 1;
		}
		return n;
	}

	bool HasDuplication(Rules *R, const State *s, uint8 player) {
		const NkcTopology topo = Topo(R);
		const int32		  nbr  = NeighborCount(topo);
		for (uint32 i = 0; i < s->cellCount; ++i) {
			if (s->cells[i].owner != static_cast<int8>(player)) continue;
			for (int32 k = 0; k < nbr; ++k) {
				const int32 di = FindCell(R, Neighbor(topo, R->board.cells[i], k));
				if (di >= 0 && !R->board.blocked[di] && s->cells[di].owner == kCellEmpty)
					return true;
			}
		}
		return false;
	}

	void Emit(NkcEventSink sink, void *user, NkcEventKind kind, uint8 player,
			  NkcCoord a, NkcCoord b, int32 value) {
		if (!sink) return;
		NkcEvent ev;
		std::memset(&ev, 0, sizeof(ev));
		ev.kind	  = kind;
		ev.player = player;
		ev.a	  = a;
		ev.b	  = b;
		ev.value  = value;
		sink(user, &ev);
	}

	void CheckEnd(Rules *R, State *s, NkcEventSink sink, void *user) {
		const NkcCoord zero{};

		for (uint8 p = 0; p < s->playerCount; ++p) {
			if (!s->eliminated[p] && s->totemCount[p] == 0) {
				s->eliminated[p] = 1;
				Emit(sink, user, NkcEventKind::PlayerEliminated, p, zero, zero, p);
			}
		}

		int32 alive = 0, lastAlive = -1;
		for (uint8 p = 0; p < s->playerCount; ++p)
			if (!s->eliminated[p]) { ++alive; lastAlive = p; }

		bool over = false;

		if (alive <= 1) {
			over = true;
		} else if (R->params[P_FIN_SUR_BLOCAGE] == 0) {
			// « premier bloque » — conforme au texte 2025 (REGLES §12.1)
			for (uint8 p = 0; p < s->playerCount; ++p) {
				if (s->eliminated[p]) continue;
				if (!HasDuplication(R, s, p)) {
					Emit(sink, user, NkcEventKind::PlayerBlocked, p, zero, zero, p);
					over = true;
					break;
				}
			}
		} else {
			// « un seul joueur actif » — variante anti-kingmaking
			int32 active = 0;
			for (uint8 p = 0; p < s->playerCount; ++p)
				if (!s->eliminated[p] && HasDuplication(R, s, p)) ++active;
			if (active <= 1) over = true;
		}

		// Garde-fou de simulation (REGLES §12.3) — jamais une regle de jeu.
		if (!over &&
			static_cast<int32>(s->turn) >= R->params[P_MAX_TOURS] * static_cast<int32>(s->playerCount))
			over = true;

		if (!over) return;

		// Decompte (REGLES §12.2)
		s->finished		= 1;
		int32 best		= -1, bestCount = -1;
		bool  tie		= false;
		for (uint8 p = 0; p < s->playerCount; ++p) {
			if (s->totemCount[p] > bestCount) {
				bestCount = s->totemCount[p];
				best	  = p;
				tie		  = false;
			} else if (s->totemCount[p] == bestCount) {
				tie = true;
			}
		}
		s->winner = tie ? static_cast<int8>(-1) : static_cast<int8>(best);
		if (alive == 1) s->winner = static_cast<int8>(lastAlive);

		Emit(sink, user, NkcEventKind::GameOver, 0, zero, zero, s->winner);
	}

	void AdvanceTurn(State *s) {
		for (uint32 guard = 0; guard <= kMaxPlayers; ++guard) {
			s->current = static_cast<uint8>((s->current + 1) % s->playerCount);
			if (!s->eliminated[s->current]) return;
		}
	}

	int32 DoApplyMove(Rules *R, State *s, const NkcMove *mv, NkcEventSink sink, void *user) {
		if (!mv || s->finished) return 0;
		if (mv->player != s->current) return 0;

		const NkcTopology topo = Topo(R);
		const int32		  nbr  = NeighborCount(topo);

		if (mv->kind == NkcMoveKind::Pass) {
			NkcMove probe[1];
			if (GenMoves(R, s, probe, 1) != 1 || probe[0].kind != NkcMoveKind::Pass) return 0;
			s->turn++;
			AdvanceTurn(s);
			CheckEnd(R, s, sink, user);
			return 1;
		}

		if (mv->kind != NkcMoveKind::Duplicate) return 0;

		const int32 si = FindCell(R, mv->from);
		const int32 di = FindCell(R, mv->to);
		if (si < 0 || di < 0) return 0;
		if (s->cells[si].owner != static_cast<int8>(mv->player)) return 0;
		if (s->cells[di].owner != kCellEmpty) return 0;
		if (R->board.blocked[di]) return 0;
		if (Distance(topo, mv->from, mv->to) > R->params[P_PORTEE_DUPLICATION]) return 0;

		// --- Placement (REGLES §7.2) — la source reste intacte ---------------
		s->cells[di].owner	   = static_cast<int8>(mv->player);
		s->cells[di].level	   = static_cast<int8>(R->params[P_NIVEAU_TOTEM_DUPLIQUE]);
		s->cells[di].people	   = s->cells[si].people;
		s->cells[di].power	   = -1;
		s->cells[di].powerUsed = 0;
		if (!R->params[P_SOURCE_RESTE_INTACTE]) {
			s->cells[si].owner = kCellEmpty;
			s->cells[si].level = 0;
		}
		Emit(sink, user, NkcEventKind::TotemDuplicated, mv->player, mv->from, mv->to, 0);

		s->conquestTenths[mv->player] += R->params[P_PC_BASE_DUPLICATION];

		// --- Transformation (REGLES §7.3) ------------------------------------
		// Seul le totem PLACE est declencheur : pas de propagation. Les cascades
		// naissent de la multiplicite des voisins. Point le plus facile a rater.
		int32 flipped = 0;
		for (int32 k = 0; k < nbr; ++k) {
			const int32 ni = FindCell(R, Neighbor(topo, mv->to, k));
			if (ni < 0) continue;
			const int8 o = s->cells[ni].owner;
			if (o < 0 || o == static_cast<int8>(mv->player)) continue;

			const int8 lvl = s->cells[ni].level;

			// Energie : niveau x energie_par_niveau (REGLES §11.1)
			s->energy[mv->player] += static_cast<int32>(lvl) * R->params[P_ENERGIE_PAR_NIVEAU];

			// PC : base + niveau + voisinage (REGLES §11.2), tout en dixiemes.
			int32 ennemis = 0, allies = 0;
			for (int32 k2 = 0; k2 < nbr; ++k2) {
				const int32 mi = FindCell(R, Neighbor(topo, R->board.cells[ni], k2));
				if (mi < 0) continue;
				const int8 o2 = s->cells[mi].owner;
				if (o2 < 0) continue;
				if (o2 == static_cast<int8>(mv->player)) ++allies;
				else									 ++ennemis;
			}
			s->conquestTenths[mv->player] += R->params[P_PC_BASE_DUPLICATION]
										   + static_cast<int32>(lvl) * R->params[P_PC_BONUS_NIVEAU]
										   + ennemis * R->params[P_PC_BONUS_ENNEMI_ADJ]
										   + allies * R->params[P_PC_BONUS_ALLIE_ADJ];

			s->cells[ni].owner	   = static_cast<int8>(mv->player);
			s->cells[ni].level	   = static_cast<int8>(R->params[P_NIVEAU_APRES_TRANSFORMATION]);
			s->cells[ni].power	   = -1;   // le pouvoir en reserve est perdu
			s->cells[ni].powerUsed = 0;

			Emit(sink, user, NkcEventKind::TotemTransformed, mv->player,
				 R->board.cells[ni], mv->to, o);
			++flipped;
		}

		if (flipped >= 2)
			Emit(sink, user, NkcEventKind::Cascade, mv->player, mv->to, mv->to, flipped);

		Recount(s);
		s->turn++;
		AdvanceTurn(s);
		CheckEnd(R, s, sink, user);
		return 1;
	}

	// =========================================================================
	// vtable
	// =========================================================================
	NkcRules V_Create() {
		void *mem = RawAlloc(sizeof(Rules));
		if (!mem) return nullptr;
		Rules *R = new (mem) Rules();
		for (int32 i = 0; i < P_COUNT; ++i) R->params[i] = kParams[i].def;
		BuildDefaultBoard(R);
		return static_cast<NkcRules>(R);
	}

	void V_Destroy(NkcRules self) {
		if (!self) return;
		Rules *R = static_cast<Rules *>(self);
		R->~Rules();
		RawFree(R);
	}

	const char *V_GetParamsSchemaJson(NkcRules self) {
		Rules *R	= static_cast<Rules *>(self);
		char  *w	= R->schemaBuf;
		usize  left = sizeof(R->schemaBuf);
		int32  k	= std::snprintf(w, left, "[");
		w += k;
		left -= static_cast<usize>(k);
		for (int32 i = 0; i < P_COUNT; ++i) {
			const bool isBool = (kParams[i].lo == 0 && kParams[i].hi == 1);
			k = std::snprintf(w, left,
							  "%s{\"key\":\"%s\",\"group\":\"%s\",\"type\":\"%s\","
							  "\"min\":%d,\"max\":%d,\"def\":%d,\"val\":%d}",
							  i ? "," : "", kParams[i].key, kParams[i].group,
							  isBool ? "bool" : "int", kParams[i].lo, kParams[i].hi,
							  kParams[i].def, R->params[i]);
			if (k <= 0 || static_cast<usize>(k) >= left) break;
			w += k;
			left -= static_cast<usize>(k);
		}
		std::snprintf(w, left, "]");
		return R->schemaBuf;
	}

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

	int32 V_LoadBoardJson(NkcRules self, const char *json) {
		Rules *R = static_cast<Rules *>(self);
		if (!json) return 0;
		Board B;
		B.cellCount	 = 0;
		B.startCount = 0;

		const char *p = FindKey(json, "topology");
		if (p && *p == '"') {
			if		(std::strncmp(p, "\"HEX_POINTY\"", 12) == 0) R->params[P_TOPOLOGIE] = 0;
			else if (std::strncmp(p, "\"HEX_FLAT\"", 10)   == 0) R->params[P_TOPOLOGIE] = 1;
			else if (std::strncmp(p, "\"SQUARE_4\"", 10)   == 0) R->params[P_TOPOLOGIE] = 2;
			else if (std::strncmp(p, "\"SQUARE_8\"", 10)   == 0) R->params[P_TOPOLOGIE] = 3;
		}

		p = FindKey(json, "cells");
		if (!p || *p != '[') return 0;
		++p;
		while (*p) {
			p = SkipWs(p);
			if (*p == ']') { ++p; break; }
			if (*p != '[') break;
			++p;
			int32 q = 0, r = 0;
			if (!ReadInt(p, q) || !ReadInt(p, r)) break;
			p = SkipWs(p);
			if (*p == ']') ++p;
			if (B.cellCount < kMaxCells) {
				NkcCoord c;
				c.q					   = static_cast<int16>(q);
				c.r					   = static_cast<int16>(r);
				B.blocked[B.cellCount] = 0;
				B.cells[B.cellCount++] = c;
			}
		}
		if (B.cellCount == 0) return 0;

		p = FindKey(json, "blocked");
		if (p && *p == '[') {
			++p;
			while (*p) {
				p = SkipWs(p);
				if (*p == ']') { ++p; break; }
				if (*p != '[') break;
				++p;
				int32 q = 0, r = 0;
				if (!ReadInt(p, q) || !ReadInt(p, r)) break;
				p = SkipWs(p);
				if (*p == ']') ++p;
				for (uint32 i = 0; i < B.cellCount; ++i)
					if (B.cells[i].q == q && B.cells[i].r == r) B.blocked[i] = 1;
			}
		}

		p = FindKey(json, "starts");
		if (p && *p == '[') {
			const char *cur = p;
			while ((cur = std::strchr(cur, '{')) != nullptr) {
				const char *end = std::strchr(cur, '}');
				if (!end) break;
				int32		pl = 0, q = 0, r = 0, lv = 0;
				const char *f;
				if ((f = FindKey(cur, "player")) != nullptr) ReadInt(f, pl);
				if ((f = FindKey(cur, "q")) != nullptr)		ReadInt(f, q);
				if ((f = FindKey(cur, "r")) != nullptr)		ReadInt(f, r);
				if ((f = FindKey(cur, "level")) != nullptr)	ReadInt(f, lv);
				if (B.startCount < kMaxPlayers * 8) {
					StartPos sp;
					sp.player = static_cast<uint8>(
						pl < 0 ? 0 : (pl >= static_cast<int32>(kMaxPlayers)
										  ? static_cast<int32>(kMaxPlayers) - 1 : pl));
					sp.c.q					 = static_cast<int16>(q);
					sp.c.r					 = static_cast<int16>(r);
					sp.level				 = static_cast<int8>(lv);
					B.starts[B.startCount++] = sp;
				}
				cur						= end + 1;
				const char *nextObj		= std::strchr(cur, '{');
				const char *closeArr	= std::strchr(cur, ']');
				if (!nextObj || (closeArr && closeArr < nextObj)) break;
			}
		}

		int32 v = 2;
		if ((p = FindKey(json, "min_players")) != nullptr && ReadInt(p, v)) B.minPlayers = static_cast<uint8>(v);
		v = 2;
		if ((p = FindKey(json, "max_players")) != nullptr && ReadInt(p, v)) B.maxPlayers = static_cast<uint8>(v);

		R->board = B;
		return 1;
	}

	const char *V_GetBoardJson(NkcRules self) {
		Rules			  *R			= static_cast<Rules *>(self);
		static const char *kTopoName[4] = {"HEX_POINTY", "HEX_FLAT", "SQUARE_4", "SQUARE_8"};
		char			  *w			= R->boardBuf;
		usize			   left			= sizeof(R->boardBuf);
		int32			   k			= std::snprintf(w, left, "{\"topology\":\"%s\",\"cells\":[",
													kTopoName[R->params[P_TOPOLOGIE] & 3]);
		w += k; left -= static_cast<usize>(k);
		for (uint32 i = 0; i < R->board.cellCount; ++i) {
			k = std::snprintf(w, left, "%s[%d,%d]", i ? "," : "",
							  R->board.cells[i].q, R->board.cells[i].r);
			if (k <= 0 || static_cast<usize>(k) >= left) break;
			w += k; left -= static_cast<usize>(k);
		}
		k = std::snprintf(w, left, "],\"blocked\":[");
		w += k; left -= static_cast<usize>(k);
		bool first = true;
		for (uint32 i = 0; i < R->board.cellCount; ++i) {
			if (!R->board.blocked[i]) continue;
			k = std::snprintf(w, left, "%s[%d,%d]", first ? "" : ",",
							  R->board.cells[i].q, R->board.cells[i].r);
			first = false;
			if (k <= 0 || static_cast<usize>(k) >= left) break;
			w += k; left -= static_cast<usize>(k);
		}
		k = std::snprintf(w, left, "],\"starts\":[");
		w += k; left -= static_cast<usize>(k);
		for (uint32 i = 0; i < R->board.startCount; ++i) {
			k = std::snprintf(w, left, "%s{\"player\":%u,\"q\":%d,\"r\":%d,\"level\":%d}",
							  i ? "," : "", static_cast<unsigned>(R->board.starts[i].player),
							  R->board.starts[i].c.q, R->board.starts[i].c.r,
							  static_cast<int32>(R->board.starts[i].level));
			if (k <= 0 || static_cast<usize>(k) >= left) break;
			w += k; left -= static_cast<usize>(k);
		}
		std::snprintf(w, left, "],\"min_players\":%u,\"max_players\":%u}",
					  static_cast<unsigned>(R->board.minPlayers),
					  static_cast<unsigned>(R->board.maxPlayers));
		return R->boardBuf;
	}

	NkcState V_CreateState(NkcRules) {
		void *mem = RawAlloc(sizeof(State));
		if (!mem) return nullptr;
		return static_cast<NkcState>(new (mem) State());
	}

	void V_DestroyState(NkcRules, NkcState st) {
		if (!st) return;
		State *s = static_cast<State *>(st);
		s->~State();
		RawFree(s);
	}

	void V_CloneState(NkcRules, NkcState dst, const NkcState src) {
		if (!dst || !src) return;
		*static_cast<State *>(dst) = *static_cast<const State *>(src);  // POD, zero allocation
	}

	int32 V_Setup(NkcRules self, NkcState st, uint8 playerCount, uint64 seed) {
		Rules *R = static_cast<Rules *>(self);
		State *s = static_cast<State *>(st);
		if (!R || !s) return 0;

		if (playerCount < 2) playerCount = 2;
		if (playerCount > static_cast<uint8>(kMaxPlayers)) playerCount = static_cast<uint8>(kMaxPlayers);

		*s			   = State();
		s->cellCount   = R->board.cellCount;
		s->playerCount = playerCount;
		s->rng		   = seed ? seed : 0x9E3779B97F4A7C15ull;

		for (uint32 i = 0; i < s->cellCount; ++i) {
			s->cells[i].owner	  = R->board.blocked[i] ? kCellBlocked : kCellEmpty;
			s->cells[i].level	  = 0;
			s->cells[i].people	  = -1;
			s->cells[i].power	  = -1;
			s->cells[i].artefact  = -1;
			s->cells[i].powerUsed = 0;
		}
		for (uint32 i = 0; i < R->board.startCount; ++i) {
			const StartPos &sp = R->board.starts[i];
			if (sp.player >= playerCount) continue;
			const int32 ci = FindCell(R, sp.c);
			if (ci < 0) continue;
			s->cells[ci].owner	= static_cast<int8>(sp.player);
			s->cells[ci].level	= sp.level;
			s->cells[ci].people = static_cast<int8>(sp.player);
		}
		Recount(s);
		return 1;
	}

	void V_GetView(NkcRules self, const NkcState st, NkcStateView *out) {
		Rules		*R = static_cast<Rules *>(self);
		const State *s = static_cast<const State *>(st);
		if (!out || !R || !s) return;
		out->cells			= s->cells;
		out->coords			= R->board.cells;
		out->cellCount		= s->cellCount;
		out->topology		= Topo(R);
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
		static thread_local NkcMove buf[kMaxCells * 8];
		const uint32 total = GenMoves(static_cast<Rules *>(self), static_cast<const State *>(st),
									  buf, static_cast<uint32>(kMaxCells * 8));
		const uint32 lim = total < kMaxCells * 8 ? total : kMaxCells * 8;
		for (uint32 i = 0; i < lim; ++i)
			if (std::memcmp(&buf[i], mv, sizeof(NkcMove)) == 0) return 1;
		return 0;
	}

	int32 V_ApplyMove(NkcRules self, NkcState st, const NkcMove *mv, NkcEventSink sink, void *user) {
		return DoApplyMove(static_cast<Rules *>(self), static_cast<State *>(st), mv, sink, user);
	}

	int32 V_IsFinished(NkcRules, const NkcState st) {
		return static_cast<const State *>(st)->finished ? 1 : 0;
	}

	int32 V_GetWinner(NkcRules, const NkcState st) {
		return static_cast<int32>(static_cast<const State *>(st)->winner);
	}

	int32 V_IsPlayerBlocked(NkcRules self, const NkcState st, uint8 player) {
		Rules		*R = static_cast<Rules *>(self);
		const State *s = static_cast<const State *>(st);
		if (player >= s->playerCount) return 1;
		if (s->eliminated[player]) return 1;
		// Palier 0 : seules les conditions 1 et 4 de REGLES §12.1 s'appliquent
		// (ni pouvoir, ni artefact, ni fusion).
		return HasDuplication(R, s, player) ? 0 : 1;
	}

	uint32 V_SerializeState(NkcRules, const NkcState st, void *buf, uint32 cap) {
		const uint32 need = static_cast<uint32>(sizeof(State));
		if (!buf || cap < need) return need;
		std::memcpy(buf, st, need);
		return need;
	}

	int32 V_DeserializeState(NkcRules, NkcState st, const void *buf, uint32 size) {
		if (!buf || size != static_cast<uint32>(sizeof(State))) return 0;
		std::memcpy(st, buf, size);
		return 1;
	}

	uint64 V_HashState(NkcRules, const NkcState st) {
		const State *s = static_cast<const State *>(st);
		// FNV-1a 64 sur les champs SIGNIFIANTS, dans un ordre fixe.
		uint64 h = 1469598103934665603ull;
		auto   mix = [&h](uint64 v) {
			  for (int32 b = 0; b < 8; ++b) {
				  h ^= static_cast<uint64>((v >> (b * 8)) & 0xFF);
				  h *= 1099511628211ull;
			  }
		};
		for (uint32 i = 0; i < s->cellCount; ++i) {
			mix(static_cast<uint64>(static_cast<uint8>(s->cells[i].owner)));
			mix(static_cast<uint64>(static_cast<uint8>(s->cells[i].level)));
		}
		mix(s->current);
		mix(s->turn);
		for (uint8 p = 0; p < s->playerCount; ++p) {
			mix(static_cast<uint64>(static_cast<uint32>(s->energy[p])));
			mix(static_cast<uint64>(static_cast<uint32>(s->conquestTenths[p])));
		}
		return h;
	}

	void FillFactory(NkcRulesFactory *out) {
		if (!out) return;
		std::memset(out, 0, sizeof(*out));
		out->info.abiVersion = kRulesAbiVersion;
		std::snprintf(out->info.name, sizeof(out->info.name), "ConquerorRulesV2");
		std::snprintf(out->info.version, sizeof(out->info.version), "2.0.0");
		std::snprintf(out->info.author, sizeof(out->info.author), "Studio RIHEN");
		out->info.supportsHex	 = 1;
		out->info.supportsSquare = 1;
		out->info.maxPlayers	 = static_cast<uint8>(kMaxPlayers);
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
// Points d'entree
// =============================================================================
#if defined(NKC_RULES_STATIC) && NKC_RULES_STATIC
// Compile DANS l'application (Android, Web, et module interne du PC).
void NkcRulesV2GetFactory(NkcRulesFactory *out) { FillFactory(out); }
void NkcRulesV2SetAllocator(NkcAllocFn a, NkcFreeFn f) { gAlloc = a; gFree = f; }
#else
// Compile par l'atelier a partir du .cpp depose par le stagiaire.
NKC_MODULE_EXPORT void nkc_rules_set_allocator(NkcAllocFn a, NkcFreeFn f) { gAlloc = a; gFree = f; }
NKC_MODULE_EXPORT void nkc_rules_get_factory(NkcRulesFactory *out) { FillFactory(out); }
#endif
