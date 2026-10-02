// -----------------------------------------------------------------------------
// @File    main.cpp
// @Brief   Le point d'entree de NKScena (voir NKScena.jenga et ROADMAP.md).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// LANCER (depuis la RACINE du depot : les nuanceurs de NKRenderer y sont lus)
//   ./Build/Bin/Debug-Windows/NKScena/NKScena.exe
//       [--sequence=F.nkseq | --scene=F.nkscene3d | --exemple | --lanceur]
//                                       (sans rien de tout cela : le LANCEUR de la famille)
//       [--temps=S] [--vue-camera] [--jouer] [--theme=clair]
//       [--tiroir=frise|contenu|journal|terminal] [--selection=NOM] [--outil=deplacer|tourner|echelle]
//       [--details=piste|cle|plan|sequence] [--courbes] [--enregistrer[=F.nkseq]]
//       [--rendre [--sortie=DOSSIER]]          « Rendre », puis sortie (fenetre hors ecran)
//       [--capture=IMAGE.png [--temps-capture=S] [--capture-image=N]]   (fenetre HORS ECRAN, puis sortie ;
//                                       avec --rendre : apres le rendu, ou PENDANT, a l'image N)
//   ./Build/Bin/Debug-Windows/NKScena/NKScena.exe --selftest
//       le banc : modele, aller-retour .nkseq, evaluation, rendu sans fenetre
//   ./Build/Bin/Debug-Windows/NKScena/NKScena.exe --rendre-sans-fenetre=F.nkseq [--sortie=DOSSIER]
//       la sequence en PNG numerotes, sans aucune fenetre (DirectX 11 sans surface)
// -----------------------------------------------------------------------------
#include "Noge/Core/NkApplication.h"
#include "NKScena/NKScenaApp.h"
#include "NKScena/NKScenaRendu.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKLogger/NkLog.h"
#include "NKSL/ShaderConvert/NkShaderConvert.h"
#include "NKWindow/NKMain.h"

#include <cstdio>
#include <cstdlib>

nkentseu::NkAppData appData = [] {
	nkentseu::NkAppData d{};
	d.appName = "NKScena";
	d.appVersion = "0.1.0";
	return d;
}();
NKENTSEU_APP_DATA_DEFINED(appData);

namespace nkentseu {
	namespace nkscena {
		int32 NkScenaLancerBanc(); // NKScena/NKScenaBanc.cpp
	} // namespace nkscena
} // namespace nkentseu

namespace {

	/// Les nuanceurs sont lus RELATIVEMENT au repertoire courant : lance d'ailleurs
	/// que la racine, on y remonte si on la reconnait, et on le DIT (Nogee).
	void AllerALaRacine() {
		using nkentseu::NkDirectory;
		if (NkDirectory::Exists("Resources/NKRenderer/Shaders")) {
			return;
		}
		const char *candidats[] = {"../../../..", "../../..", "../..", ".."};
		for (const char *c : candidats) {
			char essai[256];
			std::snprintf(essai, sizeof(essai), "%s/Resources/NKRenderer/Shaders", c);
			if (NkDirectory::Exists(essai)) {
				(void)NkDirectory::SetCurrentDirectory(c);
				std::printf("[NKScena] repertoire courant ramene a la racine du depot (%s)\n", c);
				return;
			}
		}
		std::printf("[NKScena] ATTENTION : Resources/NKRenderer/Shaders introuvable -- lancer depuis la racine du depot\n");
	}

	/// `--rendre-sans-fenetre=F.nkseq` : la sequence (et sa scene) dans un monde
	/// NU, rendue sans fenetre par le chemin du banc.
	int RendreSansFenetre(const nkentseu::NkString &sequence, const nkentseu::NkString &sortie) {
		using namespace nkentseu;
		using namespace nkentseu::nkscena;
		static ecs::NkWorld monde;
		NkScenaModele m;
		m.Brancher(&monde, nullptr);
		(void)m.SceneDemo();
		if (!m.Ouvrir(sequence.CStr())) {
			std::printf("[NKScena] la sequence ne s'ouvre pas : %s\n", sequence.CStr());
			return 2;
		}
		NkScenaRenduDesc d;
		d.largeur = m.sortie.largeur;
		d.hauteur = m.sortie.hauteur;
		d.dossier = sortie.Empty() ? m.sortie.dossier : sortie;
		d.prefixe = m.sortie.prefixe;
		NkScenaRenduResultat r;
		const int32 code = NkScenaRendreSansFenetre(m, d, r);
		std::printf("[NKScena] rendu sans fenetre (%s) : %d image(s) ecrite(s) dans %s%s%s\n", r.api.CStr(), r.ecrites,
					d.dossier.CStr(), r.raison.Empty() ? "" : " -- ", r.raison.CStr());
		std::fflush(stdout);
		return code == 1 ? 0 : (code < 0 ? 77 : 1);
	}

} // namespace

int nkmain(const nkentseu::NkEntryState &state) {
	using namespace nkentseu;
	using namespace nkentseu::nkscena;
#if defined(_WIN32)
	SetConsoleOutputCP(CP_UTF8);
#endif
	AllerALaRacine();
	NkScenaOptions options;
	NkString sansFenetre;
	for (const auto &a : state.GetArgs()) {
		if (a == "--selftest") {
			return NkScenaLancerBanc();
		}
		if (a.StartsWith("--rendre-sans-fenetre=")) {
			sansFenetre = NkString(a.SubStr(22));
		} else if (a.StartsWith("--capture=")) {
			options.capture = NkString(a.SubStr(10));
		} else if (a.StartsWith("--temps-capture=")) {
			options.tempsCapture = static_cast<float32>(std::atof(NkString(a.SubStr(16)).CStr()));
		} else if (a.StartsWith("--scene=")) {
			options.scene = NkString(a.SubStr(8));
		} else if (a.StartsWith("--sequence=")) {
			options.sequence = NkString(a.SubStr(11));
		} else if (a == "--exemple") {
			options.exemple = true;
		} else if (a.StartsWith("--temps=")) {
			options.curseur = static_cast<float32>(std::atof(NkString(a.SubStr(8)).CStr()));
		} else if (a == "--vue-camera") {
			options.vueCamera = true;
		} else if (a == "--jouer") {
			options.jouer = true;
		} else if (a == "--theme=clair") {
			options.clair = true;
		} else if (a.StartsWith("--tiroir=")) {
			const NkString o(a.SubStr(9));
			options.tiroir = o == NkString("contenu") ? 1 : (o == NkString("journal") ? 2 : (o == NkString("terminal") ? 3 : 0));
		} else if (a.StartsWith("--selection=")) {
			options.selection = NkString(a.SubStr(12));
		} else if (a.StartsWith("--outil=")) {
			const NkString o(a.SubStr(8));
			options.outil = o == NkString("deplacer") ? 1 : (o == NkString("tourner") ? 2 : (o == NkString("echelle") ? 3 : 0));
		} else if (a.StartsWith("--details=")) {
			options.choix = NkString(a.SubStr(10));
		} else if (a == "--courbes") {
			options.courbes = true;
		} else if (a == "--enregistrer") {
			options.enregistrer = NkString("1");
		} else if (a.StartsWith("--enregistrer=")) {
			options.enregistrer = NkString(a.SubStr(14));
		} else if (a == "--lanceur") {
			options.lanceur = true;
		} else if (a == "--rendre") {
			options.rendre = true;
		} else if (a.StartsWith("--capture-image=")) {
			options.captureImage = std::atoi(NkString(a.SubStr(16)).CStr());
		} else if (a.StartsWith("--sortie=")) {
			options.sortie = NkString(a.SubStr(9));
		}
	}
	if (!sansFenetre.Empty()) {
		return RendreSansFenetre(sansFenetre, options.sortie);
	}
	options.horsEcran = !options.capture.Empty() || options.rendre;
	// LE LANCEUR de la famille au lancement NU (rien a ouvrir n'est donne).
	if (!options.horsEcran && options.sequence.Empty() && options.scene.Empty() && !options.exemple) {
		options.lanceur = true;
	}

	NkApplicationConfig config(state);
	config.appName = "NKScena";
	config.appVersion = "0.1.0";
	config.fixedTimeStep = 1.f / 60.f;
	config.windowConfig.title = "NKScena — le temps sur une scène Noge";
	config.windowConfig.width = 1280;
	config.windowConfig.height = 800;
	config.windowConfig.minWidth = 960;
	config.windowConfig.minHeight = 600;
	config.windowConfig.centered = true;
	config.windowConfig.resizable = true;
	// SANS CADRE, comme UnkenyEditor, Nogee et NkAnimaEditor : la barre de titre
	// est peinte (logo au coin, menus sur la ligne du titre).
	config.windowConfig.frame = false;
	config.windowConfig.bgColor = 0x141414FFu;
	config.deviceInfo.api = NkGraphicsApi::NK_GFX_API_OPENGL;
	config.deviceInfo.context.vulkan.appName = "NKScena";
	config.deviceInfo.context.vulkan.engineName = "Nkentseu";
	if (options.horsEcran) {
		// LA FENETRE DE CAPTURE NE GENE PERSONNE : hors de tout ecran, sans focus,
		// transparente aux clics, sans bouton de barre des taches (Nogee, NogeDemo).
		config.windowConfig.title = "NKScena -- CAPTURE (pas le produit, se ferme seule)";
		config.windowConfig.centered = false;
		config.windowConfig.x = -20000;
		config.windowConfig.y = -20000;
		config.windowConfig.noActivate = true;
		config.windowConfig.clickThrough = true;
		config.windowConfig.native.utilityWindow = true;
	}
	NkShaderCache::Global().SetCacheDir("Build/ShaderCache");

	NkScenaApp app(config, options);
	if (!app.Init()) {
		logger.Error("[NKScena] l'editeur ne s'initialise pas (fenetre ou device)\n");
		return 2;
	}
	app.Run();
	return app.CodeSortie();
}
