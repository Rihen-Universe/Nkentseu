// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_exemples_documentation.cpp
//
// Les exemples des commentaires de NkLogger.h (lignes 107, 181, 186) COMPILENT.
//
// POURQUOI (21/10/2026, retour d'apprenants transmis par RihenPlatform) : ces
// exemples appelaient une fonction de memory:: qui n'existe pas (MakeShared sans
// le prefixe Nk). Seul memory::NkMakeShared existe (NKMemory/NkSharedPtr.h). Et
// meme corriges, ils ne compilaient pas : NkLogger::AddSink attend un
// NkSharedPtr<NkISink>, et NkSharedPtr<NkConsoleSink> ne s'y convertissait pas
// (pas de conversion vers la base, contrairement a std::shared_ptr). La
// conversion est ajoutee dans NKMemory (banc NKMemory/tests/test_sharedptr_conversion.cpp).
//
// Ce fichier recopie les exemples TELS QUE LA DOCUMENTATION LES ECRIT (seul le
// nom du fichier journal change, pour ne rien ecrire a la racine), et les
// execute. Un commentaire qui ment redevient une erreur de compilation ici.
// Contre-epreuve : remettre l'ancien nom dans un exemple ci-dessous, ou retirer
// la conversion de NkSharedPtr : ce fichier ne compile plus.
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKLogger/NkLogger.h"
#include "NKLogger/Sinks/NkConsoleSink.h"
#include "NKLogger/Sinks/NkFileSink.h"

TEST_CASE(NKLoggerExemplesDoc, L107_UsageBasique) {
	// NkLogger.h, exemple « Usage basique » (ligne 107).
	nkentseu::NkLogger appLogger("MyApp");
	appLogger.AddSink(nkentseu::memory::NkMakeShared<nkentseu::NkConsoleSink>());
	appLogger.SetLevel(nkentseu::NkLogLevel::NK_INFO);
	ASSERT_EQUAL(static_cast<nkentseu::usize>(1), appLogger.GetSinkCount());
	appLogger.ClearSinks();
}

TEST_CASE(NKLoggerExemplesDoc, L181_L186_AddSink) {
	// NkLogger.h, exemple de AddSink (lignes 181 et 186).
	nkentseu::NkLogger logger("Exemple");

	// Ajout d'un sink console pour développement
	auto consoleSink = nkentseu::memory::NkMakeShared<nkentseu::NkConsoleSink>();
	consoleSink->SetLevel(nkentseu::NkLogLevel::NK_DEBUG);
	logger.AddSink(consoleSink);

	// Ajout d'un sink fichier pour production
	auto fileSink = nkentseu::memory::NkMakeShared<nkentseu::NkFileSink>("Build/Tests/exemple_doc_nklogger.log");
	fileSink->SetLevel(nkentseu::NkLogLevel::NK_INFO);
	logger.AddSink(fileSink);

	ASSERT_EQUAL(static_cast<nkentseu::usize>(2), logger.GetSinkCount());
	// Le logger et la variable locale partagent chaque sink.
	ASSERT_EQUAL(2, consoleSink.UseCount());
	ASSERT_EQUAL(2, fileSink.UseCount());
	logger.ClearSinks();
	ASSERT_EQUAL(1, consoleSink.UseCount());
	ASSERT_EQUAL(1, fileSink.UseCount());
}
