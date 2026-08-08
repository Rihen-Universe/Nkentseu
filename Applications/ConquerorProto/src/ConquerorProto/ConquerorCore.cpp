// ============================================================================
// ConquerorCore.cpp — implémentation du moteur de règles pur.
// Aucune inclusion moteur : seulement notre en-tête. Zéro STL.
// ============================================================================
#include "ConquerorProto/ConquerorCore.h"

namespace conq {

// Décalages du 4-voisinage (Nord, Sud, Ouest, Est).
static const int kDR[4] = { -1, 1, 0, 0 };
static const int kDC[4] = { 0, 0, -1, 1 };

Owner Other(Owner o) { return o == Owner::P1 ? Owner::P2 : Owner::P1; }

bool InBounds(int r, int c) { return r >= 0 && r < ROWS && c >= 0 && c < COLS; }

bool Adjacent(int r1, int c1, int r2, int c2) {
	int dr = r1 - r2; if (dr < 0) dr = -dr;
	int dc = c1 - c2; if (dc < 0) dc = -dc;
	return (dr + dc) == 1;
}

GameState InitGame() {
	GameState s;
	// 1 totem par joueur, coins opposés (setup minimal du proto).
	s.cells[ROWS - 1][0]    = Cell{ Owner::P1, START_HP, START_DMG };
	s.cells[0][COLS - 1]    = Cell{ Owner::P2, START_HP, START_DMG };
	s.current = Owner::P1;
	return s;
}

bool IsLegalDuplicate(const GameState& s, int sr, int sc, int dr, int dc) {
	if (s.finished) return false;
	if (!InBounds(sr, sc) || !InBounds(dr, dc)) return false;
	if (s.cells[sr][sc].owner != s.current) return false; // la source est à moi
	if (!s.cells[dr][dc].Empty())            return false; // la destination est vide
	if (!Adjacent(sr, sc, dr, dc))           return false; // et adjacente
	return true;
}

bool HasAnyLegalMove(const GameState& s, Owner who) {
	for (int r = 0; r < ROWS; ++r)
		for (int c = 0; c < COLS; ++c) {
			if (s.cells[r][c].owner != who) continue;
			for (int k = 0; k < 4; ++k) {
				int nr = r + kDR[k], nc = c + kDC[k];
				if (InBounds(nr, nc) && s.cells[nr][nc].Empty()) return true;
			}
		}
	return false;
}

int CountTotems(const GameState& s, Owner who) {
	int n = 0;
	for (int r = 0; r < ROWS; ++r)
		for (int c = 0; c < COLS; ++c)
			if (s.cells[r][c].owner == who) ++n;
	return n;
}

// Avance le tour. Si le joueur suivant n'a aucun coup, il passe. Si aucun des
// deux ne peut jouer, la partie se termine (vainqueur = plus de totems).
static void AdvanceTurn(GameState& s) {
	s.turn++;
	Owner next = Other(s.current);
	if (!HasAnyLegalMove(s, next)) {
		if (!HasAnyLegalMove(s, s.current)) {
			s.finished = true;
			int a = CountTotems(s, Owner::P1);
			int b = CountTotems(s, Owner::P2);
			s.winner = (a == b) ? Owner::None : (a > b ? Owner::P1 : Owner::P2);
			return;
		}
		return; // le suivant passe -> le joueur courant rejoue
	}
	s.current = next;
}

bool ApplyDuplicate(GameState& s, int sr, int sc, int dr, int dc) {
	if (!IsLegalDuplicate(s, sr, sc, dr, dc)) return false;

	// Nouveau totem frais dans la case cible.
	s.cells[dr][dc] = Cell{ s.current, START_HP, START_DMG };

	// Attaque des ennemis adjacents ; conquête à 0 HP (GDD §4.3).
	for (int k = 0; k < 4; ++k) {
		int r = dr + kDR[k], c = dc + kDC[k];
		if (!InBounds(r, c)) continue;
		Cell& n = s.cells[r][c];
		if (n.owner == Owner::None || n.owner == s.current) continue;
		n.hp -= START_DMG;
		if (n.hp <= 0) n = Cell{ s.current, START_HP, START_DMG }; // conquis
	}

	AdvanceTurn(s);
	return true;
}

bool SelfTest() {
	// 1) Init : 1 vs 1, tour à P1.
	GameState s = InitGame();
	if (CountTotems(s, Owner::P1) != 1 || CountTotems(s, Owner::P2) != 1) return false;
	if (s.current != Owner::P1) return false;

	// 2) Duplication : +1 totem P1, puis c'est au tour de P2.
	if (!ApplyDuplicate(s, ROWS - 1, 0, ROWS - 1, 1)) return false;
	if (CountTotems(s, Owner::P1) != 2) return false;
	if (s.current != Owner::P2)         return false;

	// 3) Conquête : ennemi à 3 HP adjacent à la case dupliquée -> conquis.
	{
		GameState t;
		t.cells[3][1] = Cell{ Owner::P1, START_HP, START_DMG };
		t.cells[3][3] = Cell{ Owner::P2, 3, START_DMG };
		t.current = Owner::P1;
		if (!ApplyDuplicate(t, 3, 1, 3, 2)) return false;
		if (t.cells[3][3].owner != Owner::P1) return false;
		if (CountTotems(t, Owner::P2) != 0)   return false;
	}

	// 4) Dégâts sans conquête : ennemi à 5 HP -> reste P2 avec 2 HP.
	{
		GameState t;
		t.cells[3][1] = Cell{ Owner::P1, START_HP, START_DMG };
		t.cells[3][3] = Cell{ Owner::P2, 5, START_DMG };
		t.current = Owner::P1;
		if (!ApplyDuplicate(t, 3, 1, 3, 2)) return false;
		if (t.cells[3][3].owner != Owner::P2) return false;
		if (t.cells[3][3].hp != 2)            return false;
	}

	// 5) Coup illégal rejeté (source = totem adverse).
	{
		GameState t = InitGame(); // tour à P1
		if (ApplyDuplicate(t, 0, COLS - 1, 0, COLS - 2)) return false; // source P2
	}
	return true;
}

} // namespace conq
