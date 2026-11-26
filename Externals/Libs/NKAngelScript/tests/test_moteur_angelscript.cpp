// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKAngelScript_Tests (agent scripts-2, 25/11/2026) : la bibliothèque SEULE, sans
// le moteur. Le plus petit banc possible, celui qu'on portera d'abord sur le
// mobile et le web : créer le moteur, compiler une fonction, lui offrir une
// fonction C++ en appel GÉNÉRIQUE (AS_MAX_PORTABILITY), l'exécuter.
//
// PRÉ-ENREGISTREMENT (écrit avant la mesure) :
//   Calcul(20) = Double(20) + 1 = 41 ; Double a été appelée exactement une fois.
// Contre-épreuve dans le même binaire : NKAS_MOTEUR_FAUX=1 compile « + 2 » :
// le cas doit ROUGIR (42 au lieu de 41).
// L'hôte complet (composants, systèmes, comportements, acteurs) a son banc dans
// Engine/Noge/tests/test_angelscript_*.cpp.
// =============================================================================
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKCore/NkTypes.h"
#include "NKPlatform/NkEnv.h"
#include <angelscript.h>

using namespace nkentseu;

namespace {

	int32 gAppelsDouble = 0;

	void DoubleGenerique(asIScriptGeneric *g) {
		++gAppelsDouble;
		g->SetReturnDWord(static_cast<asDWORD>(g->GetArgDWord(0) * 2u));
	}

	bool MoteurFaux() {
		const char *v = env::GetEnvVar("NKAS_MOTEUR_FAUX");
		return v != nullptr && v[0] == '1';
	}

} // namespace

TEST_CASE(NKAngelScript, LaBibliothequeSeuleCompileEtExecute) {
	asIScriptEngine *moteur = asCreateScriptEngine();
	ASSERT_NOT_NULL(moteur);
	if (moteur == nullptr) {
		return;
	}
	gAppelsDouble = 0;
	const int r = moteur->RegisterGlobalFunction("int Double(int)", asFUNCTION(DoubleGenerique), asCALL_GENERIC);
	ASSERT_TRUE(r >= 0);
	asIScriptModule *module = moteur->GetModule("banc", asGM_ALWAYS_CREATE);
	const char *const source =
		MoteurFaux() ? "int Calcul(int a) { return Double(a) + 2; }" : "int Calcul(int a) { return Double(a) + 1; }";
	ASSERT_TRUE(module != nullptr && module->AddScriptSection("banc.as", source) >= 0);
	ASSERT_TRUE(module != nullptr && module->Build() >= 0);
	asIScriptFunction *calcul = module != nullptr ? module->GetFunctionByDecl("int Calcul(int)") : nullptr;
	ASSERT_NOT_NULL(calcul);
	int32 resultat = -1;
	if (calcul != nullptr) {
		asIScriptContext *ctx = moteur->CreateContext();
		const bool ok = ctx != nullptr && ctx->Prepare(calcul) >= 0 && ctx->SetArgDWord(0, 20u) >= 0 &&
						ctx->Execute() == asEXECUTION_FINISHED;
		ASSERT_TRUE(ok);
		if (ok) {
			resultat = static_cast<int32>(ctx->GetReturnDWord());
		}
		if (ctx != nullptr) {
			ctx->Release();
		}
	}
	moteur->ShutDownAndRelease();
	ASSERT_EQUAL(41, resultat);
	ASSERT_EQUAL(1, gAppelsDouble);
}
