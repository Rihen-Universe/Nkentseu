// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_entete_parapluie.cpp
//
// NKSerialization/NKSerialization.h compile SEUL et s'emploie : cette unite de traduction n'inclut QUE lui
// (et le cadre de test).
//
// POURQUOI (22/10/2026) : l'epreuve des kits du 21/10 l'a trouve casse partout : « namespace serial =
// serialization; » (l. 151) visait un espace nkentseu::serialization qui n'existe pas (les types vivent dans
// nkentseu). L'alias, que personne n'employait, est retire (garde en commentaire).
//
// PRE-ENREGISTREMENT :
//   (s1) NkArchive : SetInt32 puis GetInt32 rend la valeur ; Has et Size disent l'entree ;
//   (s2) une cle absente : GetInt32 rend faux.
// Contre-epreuve : remettre « namespace serial = serialization; » : ce fichier ne compile plus.
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKSerialization/NKSerialization.h"

using namespace nkentseu;

TEST_CASE(NKSerializationEnteteParapluie, S1_ArchiveEcrireLire) {
	NkArchive archive;
	ASSERT_TRUE(archive.SetInt32("score", 42));
	nk_int32 lu = 0;
	ASSERT_TRUE(archive.GetInt32("score", lu));
	ASSERT_EQUAL(42, static_cast<int>(lu));
	ASSERT_TRUE(archive.Has("score"));
	ASSERT_EQUAL(static_cast<nk_size>(1), archive.Size());
}

TEST_CASE(NKSerializationEnteteParapluie, S2_CleAbsente) {
	NkArchive archive;
	nk_int32 lu = 7;
	ASSERT_FALSE(archive.GetInt32("absente", lu));
}
