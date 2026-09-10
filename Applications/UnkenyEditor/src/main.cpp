// =============================================================================
// UnkenyEditor — l'editeur du moteur de jeu 2D Unkeny
//
// Ce fichier ne fait QUE trois choses : lire les arguments, construire
// l'application, la lancer. Tout le reste vit dans `Editeur/` — un fichier par
// element, comme demande.
//
// LANCER
//   UnkenyEditor.exe [--profil=N] [--paysage] [--simuler] [--selftest]
//
//   --profil=N   l'appareil simule (0 = bureau, puis du plus contraint au moins)
//   --paysage    tourne l'appareil
//   --simuler    demarre avec la physique active
//   --selftest   rend INDETERMINE : l'editeur n'a pas de regles a lui, elles
//                vivent dans Unkeny. Un banc vide est pire qu'un banc absent.
// =============================================================================
#include "Editeur/NkEditeurApp.h"

#include "NKWindow/Core/NkEntry.h"
#include "NKWindow/NKMain.h" // fournit WinMain / android_main / ... selon la plateforme

// ⚠️ La macro attend un `NkAppData`, PAS une chaine -- son nom laisse croire
// l inverse, et le compilateur ne dit alors qu un `no viable overloaded =`
// sans nommer la cause. Le patron est celui des jeux du depot.
NKENTSEU_DEFINE_APP_DATA(([]() {
	nkentseu::NkAppData d{};
	d.appName = "UnkenyEditor";
	d.appVersion = "1.0.0";
	return d;
})());

int nkmain(const nkentseu::NkEntryState &state) {
	// ⚠️ L'application est allouee sur le TAS : elle contient une scene, un
	// monde ECS et quatre panneaux. Sur la pile, elle depasserait la limite par
	// defaut sur plusieurs plateformes — et le plantage sortirait au demarrage,
	// sans rapport visible avec sa cause.
	auto app = nkentseu::memory::NkMakeUnique<nkentseu::editeur::NkEditeurApp>();
	if (!app) {
		return -1;
	}

	// Les arguments d'abord : `--selftest` doit pouvoir repondre SANS ouvrir de
	// fenetre, sinon il ne sert a rien en automatique.
	const nkentseu::NkOptional<int> sortie = app->LireArguments(state.args);
	if (sortie.HasValue()) {
		return sortie.Value();
	}

	if (!app->Init()) {
		return -1;
	}
	return app->Run();
}
