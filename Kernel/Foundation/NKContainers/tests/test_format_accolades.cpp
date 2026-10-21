// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_format_accolades.cpp
//
// NkFormat et ses champs « {} » : chaque champ vide prend l'argument SUIVANT.
//
// POURQUOI (21/10/2026) : des apprenants (devoir ANI-2053) ont vu
// NkAddress::ToString() rendre « 172.172.172.172:172 ». NkAddress formate
// « {}.{}.{}.{}:{} » ; le moteur des accolades (NkRunBrace, NkFormat.h) lisait
// l'index en tete du champ et prenait 0 quand il n'y en avait pas : chaque « {} »
// relisait l'argument 0. 172 chaines litterales du depot ont deux « {} » ou plus
// (journaux, evenements, NkAddress) : toutes repetaient leur premier argument.
//
// PRE-ENREGISTREMENT (ecrit avant la correction) :
//   (a1) cinq « {} », cinq valeurs differentes -> chacune a sa place
//   (a2) « {} » avec specification « {:.2f} » et « {:>5} » -> index suivant
//   (a3) index explicites « {1}-{0} », « {0}{0}{1} » -> inchanges
//   (a4) « {{ » et « }} » -> accolades litterales, n'avancent pas l'index
//   (a5) melange « {} » et « {N} » -> INTERDIT, signale dans le texte (comme
//        std::format et Python), le reste du message survit
//   (a6) plus de « {} » que d'arguments -> signale avec le numero manquant
//   (a7) champ qui n'est ni vide, ni ':', ni un nombre -> signale
// La contre-epreuve (rendre l'index automatique fixe a 0) doit rendre (a1),
// (a2), (a4) et (a6) rouges.
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKContainers/String/NkFormat.h"

using namespace nkentseu;

namespace {

	// Egalite octet pour octet, en longueur, sans la bibliotheque C.
	bool Egal(const NkString &obtenu, const char *attendu) {
		usize n = 0;
		while (attendu[n] != '\0') {
			++n;
		}
		if (static_cast<usize>(obtenu.Length()) != n) {
			return false;
		}
		const char *o = obtenu.CStr();
		for (usize i = 0; i < n; ++i) {
			if (o[i] != attendu[i]) {
				return false;
			}
		}
		return true;
	}

} // namespace

TEST_CASE(NKContainersFormatAccolades, A1_CinqChampsVides) {
	const NkString s = NkFormat("{}.{}.{}.{}:{}", 172u, 16u, 0u, 1u, 8080u);
	ASSERT_TRUE_MSG(Egal(s, "172.16.0.1:8080"), s.CStr());

	const NkString t = NkFormat("{} {} {}", "un", 2, 'c');
	ASSERT_TRUE_MSG(Egal(t, "un 2 c"), t.CStr());
}

TEST_CASE(NKContainersFormatAccolades, A2_ChampsVidesAvecSpecification) {
	const NkString s = NkFormat("{} = {:.2f} [{:>5}]", "pi", 3.14159, 42);
	ASSERT_TRUE_MSG(Egal(s, "pi = 3.14 [   42]"), s.CStr());
}

TEST_CASE(NKContainersFormatAccolades, A3_IndexExplicites) {
	const NkString s = NkFormat("{1}-{0}", 1, 2);
	ASSERT_TRUE_MSG(Egal(s, "2-1"), s.CStr());

	const NkString t = NkFormat("{0}{0}{1}", 7, 8);
	ASSERT_TRUE_MSG(Egal(t, "778"), t.CStr());

	const NkString u = NkFormat("{0:>4}|{1:.1f}", 9, 2.25);
	ASSERT_TRUE_MSG(Egal(u, "   9|2.2"), u.CStr());
}

TEST_CASE(NKContainersFormatAccolades, A4_AccoladesDoublees) {
	const NkString s = NkFormat("{{}} {{{}}} {}", 5, 6);
	ASSERT_TRUE_MSG(Egal(s, "{} {5} 6"), s.CStr());

	const NkString t = NkFormat("{{0}} {0}", 3);
	ASSERT_TRUE_MSG(Egal(t, "{0} 3"), t.CStr());
}

TEST_CASE(NKContainersFormatAccolades, A5_MelangeInterditEtSignale) {
	const NkString s = NkFormat("{0} {}", 1, 2);
	ASSERT_TRUE_MSG(Egal(s, "1 {!format: {} et {N} melanges}"), s.CStr());

	const NkString t = NkFormat("{} {1} fin", 1, 2);
	ASSERT_TRUE_MSG(Egal(t, "1 {!format: {} et {N} melanges} fin"), t.CStr());
}

TEST_CASE(NKContainersFormatAccolades, A6_ArgumentManquant) {
	const NkString s = NkFormat("{} {} {}", 1, 2);
	ASSERT_TRUE_MSG(Egal(s, "1 2 {!format: argument 2 absent}"), s.CStr());

	const NkString t = NkFormat("{3}", 1);
	ASSERT_TRUE_MSG(Egal(t, "{!format: argument 3 absent}"), t.CStr());
}

TEST_CASE(NKContainersFormatAccolades, A7_ChampInvalide) {
	const NkString s = NkFormat("[{x}] {}", 1);
	ASSERT_TRUE_MSG(Egal(s, "[{!format: champ 'x' invalide}] 1"), s.CStr());
}
