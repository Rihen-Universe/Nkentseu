// -----------------------------------------------------------------------------
// @File    Applications/NkMessagesEnVue/src/main.cpp
// @Brief   LA DEMO du neuvieme puits : on appuie sur un bouton qui n'appelle que
//          `logger.*`, et le message apparait DANS LA VUE.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// CE QU'ON DOIT VOIR (et ce qui prouverait que c'est casse) :
//   • « Emettre une INFORMATION »  -> rien a l'ecran. C'EST LE COMPORTEMENT
//     ATTENDU : le puits filtre a WARN, l'ecran n'est pas la console. La ligne
//     part quand meme dans la console et dans `logs/app.log`. Si une pastille
//     bleue apparaissait, le filtre ne tiendrait pas.
//   • « Emettre un AVERTISSEMENT » -> une pastille AMBRE, douze secondes.
//   • « Emettre une ERREUR »       -> une pastille ROUGE, vingt secondes.
//   • « Douze fois la meme erreur » -> UNE pastille et « x 12 ». Douze pastilles
//     identiques = la fusion ne marche pas.
//   • « DEBRANCHER le puits »      -> plus rien n'apparait ensuite. Si un
//     message apparaissait encore, un AUTRE chemin le peindrait, et tout le
//     reste de cette demo ne prouverait rien.
//
// ⚠️ LE POINT DE LA DEMO EST DANS `Emettre()` : elle appelle `logger`, et
//    strictement rien d'autre. Aucun `NkEcranLog`, aucun peintre, aucune
//    couleur. Si ce bouton dessinait lui-meme, la demo mentirait.
// -----------------------------------------------------------------------------
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEditorKit/NkEditorKit.h"
#include "NKEditorKit/NkEditorCanvasRenderer.h"
#include "NKEditorKit/NkScreenLogSink.h"
#include "NKEditorKit/NkScreenLogView.h"
#include "NKEditorKit/Components/NkGuiComponentPaint.h"
#include "NKMemory/NkUniquePtr.h"

using namespace nkentseu;
using namespace nkentseu::editorkit;

NKENTSEU_DEFINE_APP_DATA(([]() {
	NkAppData d{};
	d.appName = "NkMessagesEnVue";
	d.appVersion = "0.1.0";
	return d;
})());

namespace {

	// ── CE QUE FAIT UN BOUTON, ET C'EST TOUT ────────────────────────────────
	// Un module quelconque du moteur, qui journalise. Il n'inclut pas l'ecran,
	// il ne connait pas la vue, il n'a pas de couleur. C'est le sujet de la demo.
	void EmettreInformation() {
		logger.Infof("%s", "Maillage importe : 12 480 sommets, 24 960 triangles");
	}
	void EmettreAvertissement() {
		logger.Warnf("%s", "Ce maillage n'a pas d'UV : la texture ne sera pas posee");
	}
	void EmettreErreur() {
		logger.Errorf("%s", "Import REFUSE : ouvrez une SCENE (un modele ne contient pas d'objets)");
	}
	void EmettreDouzeFois() {
		for (int32 i = 0; i < 12; ++i)
			logger.Errorf("%s", "Le materiau 'sol' n'a pas de nuanceur");
	}

	// ── LE PANNEAU ──────────────────────────────────────────────────────────
	class PanneauMessages : public NkEditorPanel {
		public:
			PanneauMessages() : NkEditorPanel("Messages en vue", NkEditorDockSide::NK_CENTER) {
			}

			void OnUI(NkEditorFrameContext &ec) override {
				ec.Text("LE NEUVIEME PUITS DE NKLogger — les messages sortent de la console.");
				ec.Text("Chaque bouton appelle logger.* ET RIEN D'AUTRE. Il ne dessine pas.");
				ec.Separator();

				if (ec.Button("Emettre une INFORMATION  (logger.Info)"))
					EmettreInformation();
				if (ec.Button("Emettre un AVERTISSEMENT  (logger.Warn)"))
					EmettreAvertissement();
				if (ec.Button("Emettre une ERREUR  (logger.Error)"))
					EmettreErreur();
				if (ec.Button("Douze fois la MEME erreur  -> une ligne et « x 12 »"))
					EmettreDouzeFois();

				ec.Separator();
				if (ec.Button(mBranche ? "DEBRANCHER le puits (le negatif)"
									   : "puits DEBRANCHE — plus rien ne doit apparaitre")) {
					if (mBranche) {
						NkDebrancherEcranLogPourNegatif();
						mBranche = false;
					}
				}
				ec.Text(mBranche ? "Puits : BRANCHE — le niveau minimum est AVERTISSEMENT."
								 : "Puits : DEBRANCHE — reappuyez sur les boutons : rien ne doit venir.");
				ec.Separator();
				ec.Text("L'information ne monte PAS a l'ecran, et c'est voulu :");
				ec.Text("elle part dans la console et dans logs/app.log.");

				// ── LA ZONE OU LES MESSAGES ARRIVENT ────────────────────────
				// Reservee dans la mise en page du panneau, pour que la demo
				// tienne dans un seul panneau et n'ait pas a peindre par-dessus
				// la coquille.
				auto &ctx = ec.Ui();
				const nkgui::NkRect zone = ctx.NextItemRect(760.f, 240.f);
				ctx.DL().AddRect({zone.x, zone.y, zone.w, zone.h}, nkgui::NkColor{60, 66, 82, 255},
								 1.f);

				// UNE FOIS PAR IMAGE, avec le VRAI dt : le puits est vide dans la
				// vue, et la vue vieillit en secondes.
				NkEcranLog().Tick(ec.dt);
				NkGuiComponentPaint peintre(ctx, mTheme);
				(void)NkEcranLog().Peindre(peintre, {zone.x, zone.y, zone.w, zone.h});
			}

		private:
			NkTheme mTheme = NkTheme::Dark();
			bool mBranche = true;
	};

} // namespace

int nkmain(const NkEntryState &state) {
	(void)state;

	// ── LE BRANCHEMENT, UNE LIGNE ───────────────────────────────────────────
	// A partir d'ici, TOUT ce qui journalise au-dessus de AVERTISSEMENT, dans
	// n'importe quel module lie a ce programme, apparait a l'ecran. Aucun de ces
	// modules n'a une ligne a changer.
	NkBrancherEcranLog(NkLogLevel::NK_WARN);

	auto shell = memory::NkMakeUnique<NkEditorShell>();

	NkEditorShellConfig cfg;
	cfg.title = "NkMessagesEnVue — les messages sortent de la console";
	cfg.width = 1100;
	cfg.height = 760;
	static NkEditorCanvasRenderer canvasRenderer;
	cfg.renderer = &canvasRenderer;
	if (!shell || !shell->Init(cfg))
		return -1;

	static PanneauMessages panneau;
	shell->AddPanel(&panneau);

	return shell->Run();
}
