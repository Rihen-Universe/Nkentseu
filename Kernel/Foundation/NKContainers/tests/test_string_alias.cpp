// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_string_alias.cpp
//
// NkString recoit-elle sans dommage un pointeur (ou une vue) pris DANS son propre
// tampon ? `s.Append(s)`, `s += s`, `s += s.CStr() + 3`, `s.Insert(5, s)`,
// `s = s.View()...` s'ecrivent sans y penser et std::string les tient.
//
// POURQUOI (08/10) : meme famille que NkVector::PushBack(v[0]) (test_vector.cpp,
// NKContainersVectorAlias). `Append` grandissait le tampon PUIS copiait depuis
// `str` -- qui designait l'ancien tampon, libere. ASan (clang 18) :
// heap-use-after-free, NkMemCopy <- NkString::Append.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) : chaque cas compare a ce
// que std::string rend pour la meme operation, octet pour octet et en longueur.
//   (s1) Append(soi), chaine sur le tas            (s2) += soi
//   (s3) += CStr() + 3                              (s4) Append(pointeur interne, n)
//   (s5) Append(soi) six fois depuis une PETITE chaine (tampon interne -> tas)
//   (s6) Insert(vue interne)                        (s7) Insert(soi)
//   (s8) Insert(CStr() + 4)                         (s9) = vue interne
//   (s10) Replace par une vue interne, longueur egale puis differente
//   (s11) temoin : les memes operations avec une source ETRANGERE ne changent pas
//
// ⚠️ CE QUE CE BANC NE VOIT PAS SEUL. Sans assainisseur, l'ancien code passe sur
//    toute machine dont le tas rend le bloc libere intact (Windows). Il echoue
//    sous Linux (le tas y ecrit son chainage dans le bloc) et, partout, sous
//    AddressSanitizer. Contre-epreuve du 08/10 (WSL, clang 18, glibc) avec le
//    NkString.cpp d'avant : 9 assertions sur 17 echouent sans assainisseur (seul
//    (s11) passe entier) ; sous ASan, 8 echouent et trois rapports
//    heap-use-after-free sortent. Avec la garde : 17 sur 17, aucun rapport.
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringView.h"

#include <string>

using namespace nkentseu;

namespace {

	// 56 caracteres : hors du tampon interne des petites chaines, donc sur le tas.
	const char *const kBase = "0123456789abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJ";

	bool Pareil(const NkString &obtenu, const std::string &attendu) {
		return attendu.size() == static_cast<std::size_t>(obtenu.Length()) && attendu == obtenu.CStr();
	}

} // namespace

TEST_CASE(NKContainersStringAlias, S1_S2_AppendDeSoi) {
	{
		NkString s(kBase);
		std::string r(kBase);
		s.Append(s);
		r += std::string(r);
		ASSERT_TRUE(Pareil(s, r));
	}
	{
		NkString s(kBase);
		std::string r(kBase);
		s += s;
		r += std::string(r);
		ASSERT_TRUE(Pareil(s, r));
	}
}

TEST_CASE(NKContainersStringAlias, S3_S4_AppendDUnPointeurInterne) {
	{
		NkString s(kBase);
		std::string r(kBase);
		s += s.CStr() + 3;
		r += std::string(r.c_str() + 3);
		ASSERT_TRUE(Pareil(s, r));
	}
	{
		NkString s(kBase);
		std::string r(kBase);
		s.Append(s.CStr() + 10, 30);
		r += std::string(r.c_str() + 10, 30);
		ASSERT_TRUE(Pareil(s, r));
	}
}

TEST_CASE(NKContainersStringAlias, S5_PetiteChaineQuiPasseSurLeTas) {
	NkString s("abc");
	std::string r("abc");
	for (int i = 0; i < 6; ++i) {
		s.Append(s);
		r += std::string(r);
	}
	ASSERT_EQUAL(192, static_cast<int>(s.Length()));
	ASSERT_TRUE(Pareil(s, r));
}

TEST_CASE(NKContainersStringAlias, S6_S7_S8_InsertDeSoi) {
	{
		NkString s(kBase);
		std::string r(kBase);
		s.Insert(2, NkStringView(s.CStr() + 20, 30));
		r.insert(2, std::string(r.c_str() + 20, 30));
		ASSERT_TRUE(Pareil(s, r));
	}
	{
		NkString s(kBase);
		std::string r(kBase);
		s.Insert(5, s);
		r.insert(5, std::string(r));
		ASSERT_TRUE(Pareil(s, r));
	}
	{
		NkString s(kBase);
		std::string r(kBase);
		s.Insert(7, s.CStr() + 4);
		r.insert(7, std::string(r.c_str() + 4));
		ASSERT_TRUE(Pareil(s, r));
	}
}

TEST_CASE(NKContainersStringAlias, S9_AffectationDUneVueInterne) {
	{
		NkString s(kBase);
		std::string r(kBase);
		s = NkStringView(s.CStr() + 5, 20);
		r = std::string(r.c_str() + 5, 20);
		ASSERT_TRUE(Pareil(s, r));
	}
	{
		// La vue ENTIERE : `Clear()` ecrivait le zero terminal sur son premier octet.
		NkString s(kBase);
		std::string r(kBase);
		s = s.View();
		ASSERT_TRUE(Pareil(s, r));
	}
}

TEST_CASE(NKContainersStringAlias, S10_ReplaceParUneVueInterne) {
	{
		// longueur egale, zones qui se recouvrent
		NkString s(kBase);
		std::string r(kBase);
		s.Replace(4, 10, NkStringView(s.CStr() + 8, 10));
		r.replace(4, 10, std::string(r.c_str() + 8, 10));
		ASSERT_TRUE(Pareil(s, r));
	}
	{
		// longueur differente : la zone designee est APRES ce qu'on retire
		NkString s(kBase);
		std::string r(kBase);
		s.Replace(2, 6, NkStringView(s.CStr() + 30, 12));
		r.replace(2, 6, std::string(r.c_str() + 30, 12));
		ASSERT_TRUE(Pareil(s, r));
	}
}

TEST_CASE(NKContainersStringAlias, S11_SourceEtrangereInchangee) {
	const NkString autre("ETRANGER-0123456789-ETRANGER-0123456789-ETRANGER");
	const std::string autreStd(autre.CStr());
	NkString s(kBase);
	std::string r(kBase);

	s.Append(autre);
	r += autreStd;
	ASSERT_TRUE(Pareil(s, r));

	s.Insert(9, autre.View());
	r.insert(9, autreStd);
	ASSERT_TRUE(Pareil(s, r));

	s.Replace(3, 5, autre.View());
	r.replace(3, 5, autreStd);
	ASSERT_TRUE(Pareil(s, r));

	s = autre.View();
	r = autreStd;
	ASSERT_TRUE(Pareil(s, r));
}
