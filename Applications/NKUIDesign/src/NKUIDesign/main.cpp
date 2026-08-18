// =============================================================================
// main.cpp — NkUIDesign : editer les composants declares de la bibliotheque.
//
// DEUX MODES, ET UN SEUL TOURNE AUJOURD'HUI :
//   `--probe`  : la sonde headless. Aucune fenetre, aucun GPU. C'est le TEMOIN
//                de la tranche pendant que la campagne d'Ilyana occupe la carte.
//   (defaut)   : l'editeur fenetre. **Il n'a jamais ete lance** — toute fenetre
//                3D meurt en DEVICE_REMOVED cette seance. Il compile ; il n'est
//                pas vu. C'est ecrit ici plutot que sous-entendu.
//
// ⚠️ LE MODE SONDE EST TESTE AVANT TOUTE CREATION DE FENETRE, deliberement :
//    la sonde doit pouvoir tourner sur une machine sans GPU disponible, et un
//    `Init()` place avant elle rendrait ce mode inutilisable exactement quand on
//    en a besoin.
// =============================================================================
#include "NKEditorKit/NkEditorKit.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/NKWindow.h"

#include "Panels.h"
#include "Probe.h"

using namespace nkentseu;
using namespace nkentseu::editorkit;

NKENTSEU_DEFINE_APP_DATA(([]() {
	NkAppData d{};
	d.appName = "NKUIDesign";
	d.appVersion = "0.1.0";
	return d;
})());

static nkuidesign::DesignState gDesign;

static void CmdSave(void *) {
	gDesign.Save();
}
static void CmdLoad(void *) {
	gDesign.Load();
}
static void CmdReset(void *) {
	gDesign.instance.ResetAll();
}
static void CmdQuit(void *user) {
	if (user)
		static_cast<NkEditorShell *>(user)->RequestClose();
}

int nkmain(const NkEntryState &state) {
	// ⚠️ `NkEntryState` porte `args` (un `NkVector<NkString>`), PAS `argc/argv` :
	//    le conteneur est le meme sur les huit plateformes, la ou `argv` n'existe
	//    ni sur UWP ni sur Android.
	// ⚠️ ET LE NOM `gState` ETAIT DEJA PRIS par `nkentseu::gState` (`NkEntry.h`) —
	//    d'ou `gDesign`. Le compilateur l'a dit tout de suite ; c'est le genre de
	//    collision qu'un `using namespace` large rend possible.
	for (uint32 i = 0; i < (uint32)state.args.Size(); ++i) {
		const char *a = state.args[i].Data();
		if (a && NkComponentDecl::StrEq(a, "--probe"))
			return nkuidesign::RunProbe();
	}

	gDesign.Init();

	auto shell = memory::NkMakeUnique<NkEditorShell>();
	NkEditorShellConfig cfg;
	cfg.title = "NkUIDesign - composants declares de NKEditorKit";
	cfg.width = 1440;
	cfg.height = 900;
	if (!shell || !shell->Init(cfg))
		return -1;

	static nkuidesign::ComponentsPanel components(&gDesign);
	static nkuidesign::PreviewPanel preview(&gDesign);
	static nkuidesign::SettingsPanel settings(&gDesign);
	shell->AddPanel(&components);
	shell->AddPanel(&preview);
	shell->AddPanel(&settings);

	shell->RegisterCommand("Reglages: Enregistrer", &CmdSave, nullptr, "Ctrl+S");
	shell->RegisterCommand("Reglages: Recharger", &CmdLoad, nullptr, "Ctrl+R");
	shell->RegisterCommand("Reglages: Reinitialiser", &CmdReset, nullptr);
	shell->RegisterCommand("Application: Quitter", &CmdQuit, shell.Get(), "Ctrl+Q");

	return shell->Run();
}
