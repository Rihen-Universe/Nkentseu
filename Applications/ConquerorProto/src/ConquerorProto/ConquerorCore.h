#pragma once
// ============================================================================
// ConquerorCore — MOTEUR DE RÈGLES « SPREAD » (pur, déterministe)
// ----------------------------------------------------------------------------
// AUCUNE dépendance moteur : ni NKCanvas, ni Noge, ni rendu, ni I/O, ni STL.
// C'est LA partie réutilisable du prototype : elle migrera vers Noge SANS être
// modifiée (c'est de la logique pure, testable en isolation).
//
// Périmètre proto (on répond à UNE question : « le Spread est-il fun ? ») :
//   - Grille carrée 6x6, adjacence 4 directions (fallback GDD §4.1).
//   - Action DUPLIQUER : un totem se copie sur une case vide adjacente, attaque
//     les ennemis adjacents (DMG), les conquiert à 0 HP (GDD §4.3).
//   - Fin de partie : plus aucun coup légal des deux côtés -> vainqueur au nombre.
// HORS proto (viendra dans le vrai jeu, sur Noge) : Fusion N0-N4, 4 pouvoirs,
// grille hexagonale, SAUTER à distance 2, 25 totems.
// ============================================================================

namespace conq {

constexpr int ROWS      = 6;
constexpr int COLS      = 6;
constexpr int START_HP  = 10;   // GDD §4.2 (HP initial)
constexpr int START_DMG = 3;    // GDD §4.2 (dégâts par duplication)

enum class Owner { None = 0, P1 = 1, P2 = 2 };

struct Cell {
	Owner owner = Owner::None;
	int   hp    = 0;
	int   dmg   = 0;
	bool Empty() const { return owner == Owner::None; }
};

struct GameState {
	Cell  cells[ROWS][COLS];
	Owner current  = Owner::P1;
	int   turn     = 0;
	bool  finished = false;
	Owner winner   = Owner::None;
};

Owner     Other(Owner o);
GameState InitGame();
bool      InBounds(int r, int c);
bool      Adjacent(int r1, int c1, int r2, int c2);      // 4-voisinage

// Un coup de duplication est-il légal ? (source = totem du joueur courant ;
// destination = case vide adjacente à la source)
bool IsLegalDuplicate(const GameState& s, int sr, int sc, int dr, int dc);

// Le joueur donné a-t-il au moins un coup légal ?
bool HasAnyLegalMove(const GameState& s, Owner who);

// Applique duplication + attaque + conquête, puis avance le tour (avec passe
// automatique si le suivant est bloqué). Renvoie false si le coup est illégal
// (l'état reste alors inchangé).
bool ApplyDuplicate(GameState& s, int sr, int sc, int dr, int dc);

int  CountTotems(const GameState& s, Owner who);

// Auto-tests des invariants (déterminisme, attaque, conquête, coup illégal).
// Renvoie true si tout passe. Sert de « harness » minimal, sans moteur.
bool SelfTest();

} // namespace conq
