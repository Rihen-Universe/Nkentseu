// =============================================================================
// UnkenyEditor — l'editeur du moteur de jeu 2D Unkeny
//
// Ce fichier ne fait QUE trois choses : lire les arguments, construire
// l'application, la lancer. Tout le reste vit dans `Editeur/` — un fichier par
// element, comme demande.
//
// LANCER
//   UnkenyEditor.exe [--profil=N] [--paysage] [--simuler] [--selftest]
//                    [--outil=NOM] [--selection=NOM] [--capture=IMAGE.png]
//                    [--scene=FICHIER.nkscene] [--exemple=nuit] [--eclairage=off]
//
//   --profil=N      l'appareil simule (0 = bureau, puis du plus contraint au moins)
//   --paysage       tourne l'appareil
//   --simuler       demarre avec la physique active
//   --selftest      le banc d'Unkeny, puis celui des actions de l'editeur ;
//                   code de sortie 0 = tout tient
//   --outil=NOM     selection, deplacer, tourner, echelle, poser, effacer,
//                   saisir, couteau
//   --selection=NOM choisit au depart l'entite dont le nom commence par NOM
//   --capture=...   (la coquille NKCanvas) ecrit une image et sort ; avec les
//                   deux precedentes, une capture de GIZMO sans souris
//   --scene=...     ouvre ce .nkscene au lieu de la scene neuve (et Enregistrer
//                   y ecrira)
//   --exemple=nuit  la nuit au feu de camp (eclairage 2D, effets) au lieu de
//                   la scene neuve
//   --eclairage=off la scene de depart, son eclairage ETEINT (capture « avant »)
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
	// La coquille (NkCanvasGuiApp) lit les arguments AVANT toute fenetre :
	// `--selftest` repond donc sans ecran ni GPU (NkEditeurApp::OnCommandLine).
	// ⚠️ `Run<T>` construit l'application sur la PILE : c'est pourquoi la scene
	// et l'interface, elles, vivent sur le tas (voir NkEditeurApp.h).
#if defined(_WIN32)
	// LA CONSOLE EN UTF-8, avant toute sortie : `--selftest` ecrit « Unkeny — banc
	// du moteur », et une console Windows laissee en page OEM l'affichait
	// « Unkeny ÔÇö banc » (vu par Rihen le 2026-09-29). Ici plutot que dans le
	// banc : c'est l'APPLICATION qui possede sa console, pas le moteur.
	SetConsoleOutputCP(CP_UTF8);
#endif
	return nkentseu::renderer::NkCanvasApp::Run<nkentseu::editeur::NkEditeurApp>(state);
}
