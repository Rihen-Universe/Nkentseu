// =============================================================================
// NkcAmbiguite.cpp — LE BANC QUI MESURE « QUEL COUP PART QUAND JE CLIQUE ICI ? »
//
// Il ne dessine rien et ne charge aucun module : il appelle la SEULE fonction
// qui decide, `NkcCollectMoves`, sur des listes de coups fabriquees
// a la main. C'est volontaire — le defaut qu'on mesure ici n'a jamais eu besoin
// d'une fenetre pour exister, et un banc qui exige une fenetre ne se lance pas.
//
// CE QU'IL MONTRE
// ---------------
// La regle d'avant etait « joue le PREMIER coup qui touche la selection et qui
// va sur la case cliquee ». Elle est reproduite ici sous le nom `PremierCoup`,
// telle qu'elle etait, pour que la difference se MESURE au lieu de se raconter.
//
//   1. deux POUVOIRS, meme lanceur, meme cible, powerId different
//        PremierCoup   -> toujours l'index 0. Le pouvoir n2 est INJOUABLE.
//        CollecterCoups-> 2 candidats : l'atelier demande lequel.
//   2. deux FUSIONS de groupes differents partageant la case resultat
//        meme histoire : un des deux groupes ne partait jamais.
//   3. le cas simple (un seul coup) doit rester a UN clic — une correction qui
//      ferait apparaitre un menu sur un DUPLIQUER banal serait une regression.
//   4. une case qui ne recoit aucun coup renvoie 0, jamais -1 ni un index sale.
//
// LANCEMENT : tests/ambiguite.ps1
// =============================================================================
#include "ConquerorLab/NkcMoveChoice.h"

#include <cstdio>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::conqueror;

static int gChecks = 0, gFails = 0;
#define CHECK(cond, msg)                                                       \
	do {                                                                       \
		if (cond) { ++gChecks; std::printf("  OK   %s\n", msg); }               \
		else      { ++gFails;  std::printf("  RATE %s\n", msg); }               \
	} while (0)

static NkcCoord C(int16 q, int16 r) noexcept {
	NkcCoord c{};
	c.q = q;
	c.r = r;
	return c;
}

/// LA REGLE D'AVANT, conservee pour la mesure : le premier coup qui correspond.
static int32 PremierCoup(const NkVector<NkcMove> &legal, NkcCoord sel, NkcCoord dest) noexcept {
	for (usize i = 0; i < legal.Size(); ++i) {
		const NkcMove &m = legal[i];
		if (m.kind == NkcMoveKind::Pass) continue;
		if (m.kind == NkcMoveKind::Fuse) {
			bool touche = false;
			for (int32 k = 0; k < m.fuseCount && k < static_cast<int32>(kMaxFuseCells); ++k)
				if (CoordEqual(m.fuseCells[k], sel)) { touche = true; break; }
			if (!touche) continue;
		} else if (!CoordEqual(m.from, sel)) {
			continue;
		}
		if (CoordEqual(m.to, dest)) return static_cast<int32>(i);
	}
	return -1;
}

static NkcMove Pouvoir(NkcCoord from, NkcCoord to, int16 id) noexcept {
	NkcMove m{};
	std::memset(&m, 0, sizeof(m));
	m.kind	  = NkcMoveKind::Power;
	m.from	  = from;
	m.to	  = to;
	m.powerId = id;
	return m;
}

static NkcMove Fusion(NkcCoord to, const NkcCoord *cells, int32 n) noexcept {
	NkcMove m{};
	std::memset(&m, 0, sizeof(m));
	m.kind		= NkcMoveKind::Fuse;
	m.to		= to;
	m.fuseCount = n;
	for (int32 k = 0; k < n; ++k) m.fuseCells[k] = cells[k];
	return m;
}

static NkcMove Duplique(NkcCoord from, NkcCoord to) noexcept {
	NkcMove m{};
	std::memset(&m, 0, sizeof(m));
	m.kind = NkcMoveKind::Duplicate;
	m.from = from;
	m.to   = to;
	return m;
}

int main() {
	std::printf("=== Quel coup part quand je clique ici ? ===\n\n");
	int32 out[kMaxChoix];

	// --- 1. deux pouvoirs, meme lanceur, meme cible --------------------------
	{
		NkVector<NkcMove> legal;
		legal.PushBack(Pouvoir(C(1, 1), C(2, 1), 0));
		legal.PushBack(Pouvoir(C(1, 1), C(2, 1), 1));

		const int32 vieux = PremierCoup(legal, C(1, 1), C(2, 1));
		const int32 n	  = NkcCollectMoves(legal, C(1, 1), C(2, 1), out, kMaxChoix);
		std::printf("  1. deux POUVOIRS sur la meme cible : avant -> index %d seulement ;"
					" maintenant -> %d candidats\n", vieux, n);
		CHECK(vieux == 0, "l'ancienne regle ne voyait QUE le premier pouvoir");
		CHECK(n == 2, "les deux pouvoirs sont maintenant proposes");
		CHECK(n == 2 && out[0] == 0 && out[1] == 1, "dans l'ordre du moteur, sans doublon");
		if (n == 2) {
			char a[48], b[48];
			NkcNameMove(legal[static_cast<usize>(out[0])], a, sizeof(a));
			NkcNameMove(legal[static_cast<usize>(out[1])], b, sizeof(b));
			std::printf("       le menu dira : \"%s\" / \"%s\"\n", a, b);
			CHECK(std::strcmp(a, b) != 0, "les deux entrees du menu se DISTINGUENT");
		}
	}

	// --- 2. deux fusions, meme case resultat ---------------------------------
	{
		const NkcCoord ab[2] = {C(0, 0), C(1, 0)};
		const NkcCoord ac[2] = {C(0, 0), C(0, 1)};
		NkVector<NkcMove> legal;
		legal.PushBack(Fusion(C(0, 0), ab, 2));
		legal.PushBack(Fusion(C(0, 0), ac, 2));

		const int32 vieux = PremierCoup(legal, C(0, 0), C(0, 0));
		const int32 n	  = NkcCollectMoves(legal, C(0, 0), C(0, 0), out, kMaxChoix);
		std::printf("  2. deux FUSIONS de groupes differents vers la meme case :"
					" avant -> index %d ; maintenant -> %d candidats\n", vieux, n);
		CHECK(vieux == 0, "l'ancienne regle ne jouait qu'un des deux groupes");
		CHECK(n == 2, "les deux groupes sont proposes");
	}

	// --- 3. le cas simple reste a un clic ------------------------------------
	{
		NkVector<NkcMove> legal;
		legal.PushBack(Duplique(C(2, 2), C(2, 3)));
		legal.PushBack(Duplique(C(2, 2), C(3, 2)));
		const int32 n = NkcCollectMoves(legal, C(2, 2), C(2, 3), out, kMaxChoix);
		CHECK(n == 1, "un DUPLIQUER ordinaire ne fait apparaitre AUCUN menu");
		CHECK(n == 1 && out[0] == 0, "et c'est bien le coup designe qui part");
	}

	// --- 4. une case sans coup ne renvoie rien -------------------------------
	{
		NkVector<NkcMove> legal;
		legal.PushBack(Duplique(C(2, 2), C(2, 3)));
		const int32 n = NkcCollectMoves(legal, C(2, 2), C(9, 9), out, kMaxChoix);
		CHECK(n == 0, "une case hors des destinations ne propose rien");
	}

	// --- 5. CE QUE FAISAIT LE KIT DU STAGIAIRE (avant `MoveTouches`) ---------
	//
	// La question du stagiaire etait « il n'y a pas de BOUTON pour jouer la
	// fusion ». La reponse est ici : avant `MoveTouches`, la selection elle-meme
	// filtrait sur `m.from == case`. Une fusion laisse `from` a {0,0}. Cliquer
	// une case a consommer ne la SELECTIONNAIT donc meme pas : pas de contour,
	// pas d'anneau, rien. Il n'y avait pas de bouton parce qu'il n'y avait
	// AUCUN retour — le stagiaire a decrit exactement ce qu'il voyait.
	{
		const NkcCoord ab[2] = {C(3, 2), C(4, 2)};
		NkVector<NkcMove> legal;
		legal.PushBack(Fusion(C(3, 2), ab, 2));

		bool vieux = false;
		for (usize i = 0; i < legal.Size(); ++i)
			if (CoordEqual(legal[i].from, C(3, 2))) vieux = true;
		CHECK(!vieux, "ancienne selection : une case a FUSIONNER n'etait pas cliquable");

		bool neuf = false;
		for (usize i = 0; i < legal.Size(); ++i)
			if (NkcMoveTouches(legal[i], C(3, 2))) neuf = true;
		CHECK(neuf, "maintenant : elle est cliquable, et le resultat s'affiche");

		// ET LE SYMPTOME LE PLUS TROMPEUR DE TOUS.
		// `from` mis a zero VAUT la coordonnee (0,0). L'ancienne selection
		// faisait donc de la case (0,0) — un coin du plateau, sans rapport —
		// la source apparente de TOUTES les fusions. Cliquer ce coin allumait
		// des destinations ; cliquer la vraie case n'allumait rien. Un
		// stagiaire qui tombe la-dessus ne cherche pas un bug d'interface, il
		// se croit fou.
		bool coinFantome = false;
		for (usize i = 0; i < legal.Size(); ++i)
			if (CoordEqual(legal[i].from, C(0, 0))) coinFantome = true;
		CHECK(coinFantome, "ancienne selection : la case (0,0) passait pour la source de la fusion");
	}

	// --- 6. une FUSION se joue depuis N'IMPORTE QUELLE case consommee --------
	{
		const NkcCoord grp[3] = {C(0, 0), C(1, 0), C(2, 0)};
		NkVector<NkcMove> legal;
		legal.PushBack(Fusion(C(1, 0), grp, 3));
		const int32 a = NkcCollectMoves(legal, C(0, 0), C(1, 0), out, kMaxChoix);
		const int32 b = NkcCollectMoves(legal, C(2, 0), C(1, 0), out, kMaxChoix);
		CHECK(a == 1 && b == 1, "la fusion part de chacune de ses cases consommees");
	}

	std::printf("\n%d verification(s), %d echec(s).\n", gChecks, gFails);
	return gFails ? 1 : 0;
}
