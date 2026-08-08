// =============================================================================
// GrilleLibre.cpp — LA GRILLE EST DEFINIE EN C++, PAS EN JSON.
//
// CE QUE CE FICHIER DEMONTRE
// --------------------------
// Un plateau CIRCULAIRE : trois anneaux concentriques, 19 cellules en secteurs.
// Ni hexagone, ni carre. Impossible a decrire avec `NkcTopology`, impossible a
// dessiner avec la projection standard de l'atelier — et pourtant il se joue et
// s'affiche, sans qu'on ait touche une ligne de l'atelier.
//
//         anneau 0 : 1 cellule  (le centre)
//         anneau 1 : 6 cellules
//         anneau 2 : 12 cellules
//
// C'est le module qui decide de TOUT :
//
//   la FORME du plateau      quelles coordonnees existent      -> `coords[]`
//   le VOISINAGE             qui touche qui                    -> `Neighbors()`
//   les cases BLOQUEES       kCellBlocked dans la vue
//   la POSITION a l'ecran    ou dessiner chaque cellule        -> GetCellCenter
//   la FORME des cellules    un secteur d'anneau, pas un hexa  -> GetCellShape
//
// Les trois premiers points ont TOUJOURS ete l'affaire du module — beaucoup de
// gens ne s'en rendaient pas compte parce que les exemples utilisaient la
// topologie fournie. Les deux derniers sont arrives avec l'ABI 3, precisement
// parce que la projection ecran etait la seule chose qui echappait au module.
//
// OU EST LA FRONTIERE DU FLOTTANT
// -------------------------------
// `GetCellCenter` et `GetCellShape` renvoient des `float32`. Ce n'est PAS une
// entorse a « zero flottant dans la logique de regles » (REGLES §17.1) : ces
// deux fonctions ne font que de la PRESENTATION. Regardez `Neighbors()` : elle
// est entierement ENTIERE, et c'est elle qui decide du jeu. Le rejeu bit-a-bit
// reste garanti.
//
// POUR L'UTILISER : copier dans Build/ConquerorLab/rules/ et sauvegarder.
// =============================================================================

#include "Conqueror/ConquerorRulesABI.h"
#include "Conqueror/ConquerorLog.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <new>

using namespace nkentseu;
using namespace nkentseu::conqueror;

namespace {

	NkcAllocFn gAlloc = nullptr;
	NkcFreeFn  gFree  = nullptr;

	void *RawAlloc(usize n) { return gAlloc ? gAlloc(n) : std::malloc(n); }
	void  RawFree(void *p)  { if (!p) return; if (gFree) gFree(p); else std::free(p); }

	// -------------------------------------------------------------------------
	// 1. LA FORME DU PLATEAU, EN DUR ET EN ENTIERS.
	// Une coordonnee vaut (q = anneau, r = index dans l'anneau). C'est NOTRE
	// convention : le contrat ne dit rien du sens de q et r hors topologie
	// standard, et c'est exactement ce qui rend la chose possible.
	// -------------------------------------------------------------------------
	constexpr int32 kRings			= 3;
	constexpr int32 kRingSize[kRings] = {1, 6, 12};
	constexpr int32 kCells			= 1 + 6 + 12;	 // 19
	constexpr int32 kRingStart[kRings] = {0, 1, 7};	 // index du 1er de chaque anneau

	constexpr float32 kPi = 3.14159265358979f;

	int32 IndexOf(NkcCoord c) {
		if (c.q < 0 || c.q >= kRings) return -1;
		if (c.r < 0 || c.r >= kRingSize[c.q]) return -1;
		return kRingStart[c.q] + c.r;
	}

	NkcCoord CoordOf(int32 index) {
		NkcCoord c{};
		for (int32 ring = kRings - 1; ring >= 0; --ring)
			if (index >= kRingStart[ring]) {
				c.q = static_cast<int16>(ring);
				c.r = static_cast<int16>(index - kRingStart[ring]);
				return c;
			}
		return c;
	}

	// -------------------------------------------------------------------------
	// 2. LE VOISINAGE — NOTRE definition, entierement entiere.
	//
	// C'est le coeur de la demonstration : `ConquerorGeometry.h` n'est meme pas
	// inclus. Aucune topologie du contrat ne sait exprimer « le centre touche
	// tout l'anneau 1 », ni « une cellule de l'anneau 1 touche deux cellules de
	// l'anneau 2 ». Ici, si.
	//
	// L'ORDRE reste NORMATIF (REGLES §17.4) : il fixe l'ordre de generation des
	// coups, donc la reproductibilite. Ne jamais le reordonner.
	// -------------------------------------------------------------------------
	uint32 Neighbors(NkcCoord c, NkcCoord *out, uint32 cap) {
		uint32 n = 0;
		auto   push = [&](int32 ring, int32 idx) {
			  if (ring < 0 || ring >= kRings) return;
			  const int32 sz = kRingSize[ring];
			  const int32 r  = ((idx % sz) + sz) % sz;	 // modulo circulaire
			  if (n < cap) {
				  out[n].q = static_cast<int16>(ring);
				  out[n].r = static_cast<int16>(r);
			  }
			  ++n;
		};

		if (c.q == 0) {								 // le centre touche tout l'anneau 1
			for (int32 i = 0; i < kRingSize[1]; ++i) push(1, i);
			return n;
		}
		push(c.q, c.r - 1);							 // voisin arriere sur l'anneau
		push(c.q, c.r + 1);							 // voisin avant  sur l'anneau
		if (c.q == 1) {
			push(0, 0);								 // vers le centre
			push(2, c.r * 2);						 // vers l'exterieur : deux cellules
			push(2, c.r * 2 + 1);
		} else {									 // anneau 2
			push(1, c.r / 2);						 // vers l'interieur : une cellule
		}
		return n;
	}

	// -------------------------------------------------------------------------
	// 3. Etat et moteur. Rien de particulier : c'est le meme squelette que
	// RegleMinimale.cpp, avec `Neighbors` a la place de `Neighbor(topologie,…)`.
	// -------------------------------------------------------------------------
	enum ParamId : int32 { P_MAX_TOURS = 0, P_COUNT };

	struct ParamDesc { const char *key, *group; int32 def, lo, hi; };
	const ParamDesc kParams[P_COUNT] = {
		{"max_tours", "Fin de partie", 200, 10, 100000},
	};

	struct State {
			NkcCellView cells[kCells];
			uint8		playerCount = 2;
			uint8		current		= 0;
			uint8		finished	= 0;
			int8		winner		= -2;
			uint32		turn		= 0;
			int32		energy[kMaxPlayers]			= {};
			int32		conquestTenths[kMaxPlayers] = {};
			int32		totemCount[kMaxPlayers]		= {};
			uint64		rng = 0x9E3779B97F4A7C15ull;
	};

	struct Rules {
			int32	 params[P_COUNT];
			NkcCoord coords[kCells];
			char	 schemaBuf[512];
			char	 boardBuf[2048];
	};

	void Recount(State *s) {
		for (uint32 p = 0; p < kMaxPlayers; ++p) s->totemCount[p] = 0;
		for (int32 i = 0; i < kCells; ++i) {
			const int8 o = s->cells[i].owner;
			if (o >= 0 && o < static_cast<int8>(kMaxPlayers)) s->totemCount[o]++;
		}
	}

	bool CanPlay(const Rules *R, const State *s, uint8 player) {
		NkcCoord nb[16];
		for (int32 i = 0; i < kCells; ++i) {
			if (s->cells[i].owner != static_cast<int8>(player)) continue;
			const uint32 n = Neighbors(R->coords[i], nb, 16);
			for (uint32 k = 0; k < n && k < 16; ++k) {
				const int32 ni = IndexOf(nb[k]);
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

	uint32 GenMoves(Rules *R, const State *s, NkcMove *out, uint32 cap) {
		if (s->finished) return 0;
		uint32		n	= 0;
		const uint8 me	= s->current;
		NkcCoord	nb[16];

		for (int32 i = 0; i < kCells; ++i) {
			if (s->cells[i].owner != static_cast<int8>(me)) continue;
			const NkcCoord src = R->coords[i];
			const uint32   cnt = Neighbors(src, nb, 16);
			for (uint32 k = 0; k < cnt && k < 16; ++k) {
				const int32 di = IndexOf(nb[k]);
				if (di < 0 || s->cells[di].owner != kCellEmpty) continue;
				if (n < cap) {
					NkcMove m;
					std::memset(&m, 0, sizeof(m));	 // comparaison octet a octet
					m.kind		  = NkcMoveKind::Duplicate;
					m.player	  = me;
					m.from		  = src;
					m.to		  = nb[k];
					m.targetLevel = -1;
					m.powerId	  = -1;
					out[n]		  = m;
				}
				++n;
			}
		}
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

	void CheckEnd(Rules *R, State *s, NkcEventSink sink, void *user) {
		const NkcCoord zero{};
		bool		   over = false;
		for (uint8 p = 0; p < s->playerCount; ++p)
			if (!CanPlay(R, s, p)) {
				Emit(sink, user, NkcEventKind::PlayerBlocked, p, zero, zero, p);
				over = true;
				break;
			}
		if (!over && static_cast<int32>(s->turn) >=
						 R->params[P_MAX_TOURS] * static_cast<int32>(s->playerCount))
			over = true;
		if (!over) return;

		s->finished = 1;
		int32 best = -1, bestCount = -1;
		bool  tie = false;
		for (uint8 p = 0; p < s->playerCount; ++p) {
			if (s->totemCount[p] > bestCount)	   { bestCount = s->totemCount[p]; best = p; tie = false; }
			else if (s->totemCount[p] == bestCount) { tie = true; }
		}
		s->winner = tie ? static_cast<int8>(-1) : static_cast<int8>(best);
		Emit(sink, user, NkcEventKind::GameOver, 0, zero, zero, s->winner);
	}

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

		// La destination doit etre VOISINE de la source — selon NOTRE voisinage.
		{
			NkcCoord	 nb[16];
			const uint32 cnt = Neighbors(mv->from, nb, 16);
			bool		 ok	 = false;
			for (uint32 k = 0; k < cnt && k < 16; ++k)
				if (nb[k].q == mv->to.q && nb[k].r == mv->to.r) { ok = true; break; }
			if (!ok) return 0;
		}

		s->cells[di].owner = static_cast<int8>(mv->player);
		s->cells[di].level = 0;
		Emit(sink, user, NkcEventKind::TotemDuplicated, mv->player, mv->from, mv->to, 0);
		s->conquestTenths[mv->player] += 10;

		// Transformation : les ennemis voisins de la case POSEE, et eux seuls.
		NkcCoord	 nb[16];
		const uint32 cnt	 = Neighbors(mv->to, nb, 16);
		int32		 flipped = 0;
		for (uint32 k = 0; k < cnt && k < 16; ++k) {
			const int32 ni = IndexOf(nb[k]);
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
	// 4. LA GEOMETRIE D'AFFICHAGE — le coeur de l'exemple (ABI 3).
	//
	// PRESENTATION UNIQUEMENT. Ces deux fonctions ne sont jamais appelees par la
	// logique de jeu ; leurs flottants ne rentrent nulle part dans un etat, une
	// empreinte ou une comparaison de coups.
	// =========================================================================

	/// Angle du centre d'une cellule, en radians.
	float32 CellAngle(NkcCoord c) {
		if (c.q == 0) return 0.f;
		const float32 sz = static_cast<float32>(kRingSize[c.q]);
		return 2.f * kPi * static_cast<float32>(c.r) / sz;
	}

	/// Centre, en UNITES DE CELLULE. L'atelier cadre et met a l'echelle ensuite :
	/// on raisonne en « pas de grille », jamais en pixels.
	int32 V_GetCellCenter(NkcRules, NkcCoord c, float32 *outXY) {
		if (!outXY) return 0;
		if (c.q < 0 || c.q >= kRings) return 0;
		if (c.q == 0) { outXY[0] = 0.f; outXY[1] = 0.f; return 1; }
		const float32 a = CellAngle(c);
		const float32 R = static_cast<float32>(c.q);	// anneau 1 -> rayon 1, etc.
		outXY[0] = R * std::cos(a);
		outXY[1] = R * std::sin(a);
		return 1;
	}

	/// Contour, en unites, RELATIF au centre. Un secteur d'anneau : deux arcs
	/// (interieur et exterieur) relies par deux rayons. Le centre, lui, est un
	/// petit disque approche par un polygone.
	/// Le voisinage, DECLARE. Sans cette entree, une IA qui evalue une position
	/// sur ce plateau devinerait l'adjacence a partir de `topology` — et
	/// devinerait faux, en silence.
	uint32 V_GetNeighbors(NkcRules, NkcCoord c, NkcCoord *out, uint32 cap) {
		return Neighbors(c, out, cap);
	}

	uint32 V_GetCellShape(NkcRules, NkcCoord c, float32 *outXY, uint32 capPoints) {
		if (!outXY || capPoints < 3) return 0;
		if (c.q < 0 || c.q >= kRings) return 0;

		// --- le centre : un disque ---
		if (c.q == 0) {
			const uint32 n = capPoints < 10u ? capPoints : 10u;
			for (uint32 i = 0; i < n; ++i) {
				const float32 a = 2.f * kPi * static_cast<float32>(i) / static_cast<float32>(n);
				outXY[i * 2]	 = 0.46f * std::cos(a);
				outXY[i * 2 + 1] = 0.46f * std::sin(a);
			}
			return n;
		}

		// --- un anneau : secteur entre R-0.46 et R+0.46, sur un demi-angle ---
		const float32 a0	= CellAngle(c);
		const float32 R		= static_cast<float32>(c.q);
		const float32 half	= kPi / static_cast<float32>(kRingSize[c.q]) * 0.94f;
		const float32 cx	= R * std::cos(a0);
		const float32 cy	= R * std::sin(a0);

		// 4 points sur l'arc exterieur, puis 4 sur l'interieur en sens inverse.
		const uint32 seg = 4;
		uint32		 n	 = 0;
		if (capPoints < seg * 2) return 0;

		for (uint32 i = 0; i < seg; ++i) {
			const float32 t = static_cast<float32>(i) / static_cast<float32>(seg - 1);
			const float32 a = a0 - half + 2.f * half * t;
			outXY[n * 2]	 = (R + 0.46f) * std::cos(a) - cx;
			outXY[n * 2 + 1] = (R + 0.46f) * std::sin(a) - cy;
			++n;
		}
		for (uint32 i = 0; i < seg; ++i) {
			const float32 t = static_cast<float32>(seg - 1 - i) / static_cast<float32>(seg - 1);
			const float32 a = a0 - half + 2.f * half * t;
			outXY[n * 2]	 = (R - 0.46f) * std::cos(a) - cx;
			outXY[n * 2 + 1] = (R - 0.46f) * std::sin(a) - cy;
			++n;
		}
		return n;
	}

	// =========================================================================
	// 5. Le reste de la vtable.
	// =========================================================================
	NkcRules V_Create() {
		void *mem = RawAlloc(sizeof(Rules));
		if (!mem) return nullptr;
		Rules *R = new (mem) Rules();
		for (int32 i = 0; i < P_COUNT; ++i) R->params[i] = kParams[i].def;
		for (int32 i = 0; i < kCells; ++i) R->coords[i] = CoordOf(i);
		return static_cast<NkcRules>(R);
	}

	void V_Destroy(NkcRules self) {
		if (!self) return;
		Rules *R = static_cast<Rules *>(self);
		R->~Rules();
		RawFree(R);
	}

	const char *V_GetParamsSchemaJson(NkcRules self) {
		Rules *R = static_cast<Rules *>(self);
		std::snprintf(R->schemaBuf, sizeof(R->schemaBuf),
					  "[{\"key\":\"max_tours\",\"group\":\"Fin de partie\",\"type\":\"int\","
					  "\"min\":%d,\"max\":%d,\"def\":%d,\"val\":%d}]",
					  kParams[0].lo, kParams[0].hi, kParams[0].def, R->params[0]);
		return R->schemaBuf;
	}

	int32 V_SetParam(NkcRules self, const char *key, float64 value) {
		Rules *R = static_cast<Rules *>(self);
		if (!key || std::strcmp(key, "max_tours") != 0) return 0;
		int32 v = static_cast<int32>(value < 0 ? value - 0.5 : value + 0.5);
		if (v < kParams[0].lo) v = kParams[0].lo;
		if (v > kParams[0].hi) v = kParams[0].hi;
		R->params[0] = v;
		return 1;
	}

	float64 V_GetParam(NkcRules self, const char *key) {
		Rules *R = static_cast<Rules *>(self);
		if (!key || std::strcmp(key, "max_tours") != 0) return 0.0;
		return static_cast<float64>(R->params[0]);
	}

	/// Plateau defini EN CODE : il n'y a rien a charger. On refuse franchement
	/// plutot que de faire semblant, et l'atelier l'affiche proprement.
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
					  "{\"player\":0,\"q\":2,\"r\":0,\"level\":0},"
					  "{\"player\":1,\"q\":2,\"r\":6,\"level\":0}],"
					  "\"min_players\":2,\"max_players\":2}");
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

	void V_CloneState(NkcRules, NkcState dst, const NkcState src) {
		if (dst && src) *static_cast<State *>(dst) = *static_cast<const State *>(src);
	}

	int32 V_Setup(NkcRules self, NkcState st, uint8 playerCount, uint64 seed) {
		Rules *R = static_cast<Rules *>(self);
		State *s = static_cast<State *>(st);
		if (!R || !s) return 0;
		(void)playerCount;

		*s			   = State();
		s->playerCount = 2;
		s->rng		   = seed ? seed : 0x9E3779B97F4A7C15ull;
		for (int32 i = 0; i < kCells; ++i) {
			s->cells[i].owner	 = kCellEmpty;
			s->cells[i].level	 = 0;
			s->cells[i].people	 = -1;
			s->cells[i].power	 = -1;
			s->cells[i].artefact = -1;
		}
		// Depart SYMETRIQUE : deux cellules diametralement opposees de l'anneau
		// exterieur (REGLES §4.2 — sans symetrie, tout winrate est ininterpretable).
		NkcCoord a{}; a.q = 2; a.r = 0;
		NkcCoord b{}; b.q = 2; b.r = 6;
		s->cells[IndexOf(a)].owner = 0;
		s->cells[IndexOf(b)].owner = 1;
		Recount(s);
		NKC_LOG_INFO("nouvelle partie : %d cellules sur %d anneaux, graine %llu",
					 kCells, kRings, (unsigned long long)seed);
		return 1;
	}

	void V_GetView(NkcRules self, const NkcState st, NkcStateView *out) {
		Rules		*R = static_cast<Rules *>(self);
		const State *s = static_cast<const State *>(st);
		if (!out || !R || !s) return;
		out->cells			= s->cells;
		out->coords			= R->coords;
		out->cellCount		= kCells;
		// La topologie n'est plus qu'un repli : GetCellCenter la court-circuite.
		// On declare la plus neutre pour que le jour ou l'atelier tournerait sur
		// une version anterieure de l'ABI, il affiche quand meme quelque chose.
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
		NkcMove		 buf[kCells * 8];
		const uint32 total = GenMoves(static_cast<Rules *>(self), static_cast<const State *>(st),
									  buf, static_cast<uint32>(kCells * 8));
		const uint32 lim = total < kCells * 8 ? total : kCells * 8;
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

	int32 V_IsPlayerBlocked(NkcRules self, const NkcState st, uint8 player) {
		Rules		*R = static_cast<Rules *>(self);
		const State *s = static_cast<const State *>(st);
		if (player >= s->playerCount) return 1;
		return CanPlay(R, s, player) ? 0 : 1;
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
		std::snprintf(out->info.name, sizeof(out->info.name), "GrilleLibre (3 anneaux)");
		std::snprintf(out->info.version, sizeof(out->info.version), "1.0.0");
		std::snprintf(out->info.author, sizeof(out->info.author), "Cours ConquerorLab");
		out->info.supportsHex	 = 0;
		out->info.supportsSquare = 0;	// ni l'un ni l'autre : geometrie propre
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

		// ---- LES DEUX ENTREES QUI CHANGENT TOUT (ABI 3) ----
		out->vtable.GetCellCenter		= V_GetCellCenter;
		out->vtable.GetCellShape		= V_GetCellShape;
		out->vtable.GetNeighbors		= V_GetNeighbors;
	}

} // namespace

NKC_MODULE_EXPORT void nkc_rules_set_allocator(NkcAllocFn a, NkcFreeFn f) { gAlloc = a; gFree = f; }
NKC_MODULE_LOGGING(rules)   // <- une ligne, et NKC_LOG_* va dans le panneau « Sortie »
NKC_MODULE_EXPORT void nkc_rules_get_factory(NkcRulesFactory *out) { FillFactory(out); }
