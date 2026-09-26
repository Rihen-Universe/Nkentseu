// =============================================================================
// @File    Applications/NkDemoDeltaSouris/src/main.cpp
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   LES DEUX DELTAS CÔTE À CÔTE : bougez la souris, puis ARRÊTEZ-LA.
//          L'un retombe à zéro, l'autre garde sa dernière valeur.
//
// POURQUOI ELLE EXISTE (26/09/2026)
//   Un utilisateur cherchait un getter de position de souris dans `NKWindow` — il
//   n'y en a pas, la lecture est dans **NKEvent** (`NkInput.MouseX/MouseY`). Il a
//   cherché la LECTURE là où est l'ÉCRITURE, ce qui est le réflexe normal.
//   Troisième utilisateur en deux jours à buter sur un silence de documentation.
//
//   Et le dépôt documentait déjà un piège : `MouseDeltaX/Y()` **ne se remet pas à
//   zéro** quand la souris s'arrête — `NkDemo3D.cpp:8887` note que le modeleur
//   calcule son delta lui-même « contrairement à NkInput.MouseDelta*() périmé ».
//
// ⚠️ CE QUE CETTE DÉMO MONTRE, ET QU'AUCUN BANC NE POURRAIT MONTRER
//    La différence entre les deux ne se voit que si **une main arrête de bouger**.
//    Nous n'injectons aucune entrée et nos fenêtres de sonde sont invisibles :
//    seul un humain peut faire ce geste. C'est la même raison que pour la forme du
//    curseur.
//
// CE QU'ON DOIT VOIR
//    - en bougeant : les deux colonnes affichent des nombres qui varient ;
//    - ⚠️ **en s'arrêtant : « DE CETTE IMAGE » tombe à 0, « DERNIER ÉVÉNEMENT »
//      garde sa valeur.** C'est tout le sujet.
//    - un compteur d'images immobiles, pour prouver que ce n'est pas un hasard de
//      timing : après 60 images sans bouger, l'une est toujours à 0 et l'autre
//      toujours pas.
//
// ⚠️ AUCUNE DES DEUX N'EST « LA MAUVAISE ». L'écran dit quand chacune est la
//    bonne : le delta persistant sert à garder la dernière DIRECTION connue même
//    à l'arrêt ; celui de l'image sert à tout ce qui doit s'arrêter quand la main
//    s'arrête.
//
// @License Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================

#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkEventDispatcher.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKLogger/NkLog.h"

#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/Renderer/Shapes/NkRectangleShape.h"
#include "NKCanvas/Renderer/Shapes/NkCircleShape.h"
#include "NKCanvas/Renderer/Resources/NkFont.h"
#include "NKCanvas/Renderer/Resources/NkSprite.h" // NkText

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::renderer;

namespace {

	struct Case {
			float32 x, y, w, h;
			bool cochee;
			const char *libelle;
	};

	bool DansRect(float32 px, float32 py, float32 x, float32 y, float32 w, float32 h) {
		return px >= x && px < x + w && py >= y && py < y + h;
	}

} // namespace

namespace {

	// ── Le mode --mesure : le critere, sans main ──────────────────────────
	// ⚠️ ON POUSSE L'ETAT, ON N'INJECTE AUCUNE SOURIS. `OnMove` est public ;
	//    le poste de Rodolf n'est pas touche. Le banc prouve le MECANISME,
	//    la fenetre montre le GESTE -- et seule une main qui s'arrete peut
	//    montrer le geste.
	int Mesurer(const char *ident) {
		std::printf("=== NkDemoDeltaSouris --mesure\n%s\n\n", ident);
		int echecs = 0;

		auto &es = NkEvents();
		NkEventState &st = es.GetInputState();

		// Une image ou la souris BOUGE de (10, 5).
		NkInput.NewFrame();
		st.mouse.OnMove(100, 100, 100, 100);
		NkInput.NewFrame();
		st.mouse.OnMove(110, 105, 110, 105);
		const int32 evX = NkInput.MouseDeltaX(), evY = NkInput.MouseDeltaY();
		const int32 frX = NkInput.MouseDeltaThisFrameX(), frY = NkInput.MouseDeltaThisFrameY();
		std::printf("  la souris bouge de (10,5) :\n"
				"      MouseDelta          = (%+d,%+d)\n"
				"      MouseDeltaThisFrame = (%+d,%+d)\n",
				evX, evY, frX, frY);
		const bool bougent = evX == 10 && evY == 5 && frX == 10 && frY == 5;
		std::printf("  [%s] les DEUX voient le mouvement\n", bougent ? "OK" : "ECHEC");
		if (!bougent)
			++echecs;

		// ⚠️ PUIS 60 IMAGES SANS AUCUN MOUVEMENT, DANS LA MEME COURSE.
		int frNonNul = 0, evPerdu = 0;
		for (int i = 0; i < 60; ++i) {
			NkInput.NewFrame();
			if (NkInput.MouseDeltaThisFrameX() != 0 || NkInput.MouseDeltaThisFrameY() != 0)
				++frNonNul;
			if (NkInput.MouseDeltaX() != 10 || NkInput.MouseDeltaY() != 5)
				++evPerdu;
		}
		std::printf("  60 images immobiles :\n"
				"      ThisFrame non nul sur %d images (exige 0)\n"
				"      MouseDelta a perdu (10,5) sur %d images (exige 0)\n",
				frNonNul, evPerdu);
		std::printf("  [%s] LE DELTA D'IMAGE RETOMBE A ZERO, a chaque image\n",
				frNonNul == 0 ? "OK" : "ECHEC");
		if (frNonNul != 0)
			++echecs;
		std::printf("  [%s] LE DELTA D'EVENEMENT GARDE SA VALEUR -- c'est son contrat\n",
				evPerdu == 0 ? "OK" : "ECHEC");
		if (evPerdu != 0)
			++echecs;

		// ⚠️ ET LE NEGATIF : les deux doivent DIFFERER. Si elles rendaient la
		//    meme chose, on aurait ajoute une fonction pour rien -- et deux
		//    noms qui ne disent pas ce qui les separe est precisement le
		//    danger que le coordinateur a nomme.
		const bool different = (NkInput.MouseDeltaX() != NkInput.MouseDeltaThisFrameX());
		std::printf("  [%s] NEGATIF : a l'arret, les deux DIFFERENT (%+d contre %+d)\n",
				different ? "OK" : "ECHEC", NkInput.MouseDeltaX(),
				NkInput.MouseDeltaThisFrameX());
		if (!different)
			++echecs;

		std::printf("\nStatus: %s (%d echec(s))\n",
				echecs == 0 ? "VERT" : "ROUGE", echecs);
		return echecs == 0 ? 0 : 1;
	}

} // namespace

int nkmain(const NkEntryState &state) {
	// ── L'identité de construction, en première ligne ────────────────────────
	// ⚠️ `argv[0]` n'existe pas ici : `NkEntryState` porte `args`. Quand il
	//    manque, on l'ÉCRIT plutôt que d'afficher un « ? ».
	char ident[512];
	{
		const char *exe =
			state.args.IsEmpty() ? "(chemin non transmis par l'entree)" : state.args[0].CStr();
		std::snprintf(ident, sizeof(ident), "compile le %s a %s  |  %s", __DATE__, __TIME__, exe);
	}

	for (uint32 i = 0; i < state.args.Size(); ++i)
		if (state.args[i] == "--mesure")
			return Mesurer(ident);

	NkWindowConfig cfg;
	cfg.title = "NkDemoDeltaSouris - bougez la souris, puis ARRETEZ-LA";
	cfg.width = 980;
	cfg.height = 560;
	cfg.centered = true;
	cfg.resizable = true;

	NkWindow window(cfg);
	if (!window.IsOpen()) {
		logger.Error("[NkDemoDeltaSouris] creation fenetre echouee");
		return -1;
	}

	NkContextDesc desc;
	desc.api = NkGraphicsApi::NK_GFX_API_OPENGL;
	NkRenderWindow target(window, desc);
	if (!target.IsValid()) {
		logger.Error("[NkDemoDeltaSouris] cible de rendu invalide");
		return -2;
	}

	// ⚠️ `NkFont` vit dans DEUX espaces de noms : on qualifie.
	renderer::NkFont font;
	const bool aPolice = font.LoadFromFile(*target.GetRenderer(), "Resources/Fonts/Antonio-Bold.ttf");
	if (!aPolice)
		logger.Warn("[NkDemoDeltaSouris] police introuvable -> pas de libelles");

	logger.Info("[NkDemoDeltaSouris] identite de construction : {0}", ident);

	// ⚠️ LA CASE QUI FAIT TOUT L'INTÉRÊT : décochée, `NewFrame()` n'est plus
	//    appelé, et le delta d'image rend 0 EN PERMANENCE — avec une ligne de
	//    journal, une seule fois. C'est le comportement voulu : un armement
	//    oublié ne doit pas rendre une valeur périmée en silence.
	Case caseFrame{20.f, 470.f, 20.f, 20.f, true,
				   "Appeler NkInput.NewFrame() (decochez : le delta d'image rend 0 et le dit)"};
	Case casePlacer{20.f, 498.f, 20.f, 20.f, false,
					"Replacer la souris au centre au prochain clic (NkPlaceMouseInWindow)"};

	auto &events = NkEvents();
	int32 immobiles = 0;
	int32 dernierX = 0, dernierY = 0;
	bool premier = true;

	while (window.IsOpen()) {
		// ⚠️ UNE FOIS PAR TOUR DE BOUCLE, AVANT DE DÉPILER : c'est le contrat.
		if (caseFrame.cochee)
			NkInput.NewFrame();

		bool aBouge = false;
		while (NkEvent *ev = events.PollEvent()) {
			if (ev->Is<NkWindowCloseEvent>()) {
				window.Close();
				break;
			}
			if (ev->As<NkMouseMoveEvent>())
				aBouge = true;
			if (auto *mb = ev->As<NkMouseButtonPressEvent>()) {
				if (mb->GetButton() == NkMouseButton::NK_MB_LEFT) {
					const float32 mx = static_cast<float32>(NkInput.MouseX());
					const float32 my = static_cast<float32>(NkInput.MouseY());
					if (DansRect(mx, my, caseFrame.x, caseFrame.y, 640.f, caseFrame.h))
						caseFrame.cochee = !caseFrame.cochee;
					else if (DansRect(mx, my, casePlacer.x, casePlacer.y, 640.f, casePlacer.h))
						casePlacer.cochee = !casePlacer.cochee;
					else if (casePlacer.cochee) {
						// ⚠️ LA PORTE NOMMÉE, en délégation pure vers
						//    SetMousePositionClient. Elle ne se déclenche que sur
						//    un clic EXPLICITE : on ne déplace pas la souris de
						//    Rodolf sans qu'il l'ait demandé.
						const math::NkVec2u t = target.GetSize();
						NkPlaceMouseInWindow(window, static_cast<int32>(t.x) / 2,
											 static_cast<int32>(t.y) / 2);
					}
				}
			}
		}

		// Le compteur d'images immobiles : il prouve que la différence n'est pas
		// un hasard de cadence.
		if (aBouge)
			immobiles = 0;
		else if (immobiles < 100000)
			++immobiles;

		const int32 dEv = NkInput.MouseDeltaX();
		const int32 dEvY = NkInput.MouseDeltaY();
		const int32 dFr = NkInput.MouseDeltaThisFrameX();
		const int32 dFrY = NkInput.MouseDeltaThisFrameY();
		if (premier || aBouge) {
			dernierX = dEv;
			dernierY = dEvY;
			premier = false;
		}

		target.Clear(NkColor2D{22, 24, 28, 255});

		if (aPolice) {
			NkText id(font, ident, 13u);
			id.SetFillColor(NkColor2D{150, 156, 164, 255});
			id.SetPosition({20.f, 14.f});
			target.Draw(static_cast<NkDrawable &>(id));

			NkText t0(font, "Bougez la souris, puis ARRETEZ-LA sans la sortir de la fenetre.", 17u);
			t0.SetFillColor(NkColor2D{235, 235, 235, 255});
			t0.SetPosition({20.f, 40.f});
			target.Draw(static_cast<NkDrawable &>(t0));

			char l1[224];
			std::snprintf(l1, sizeof(l1),
						  "MouseDeltaX/Y()          = %+5d , %+5d   <- DERNIER EVENEMENT, il "
						  "PERSISTE",
						  dEv, dEvY);
			NkText t1(font, l1, 20u);
			// Orange : il ne retombera pas à zéro, et c'est son contrat.
			t1.SetFillColor(NkColor2D{235, 180, 60, 255});
			t1.SetPosition({20.f, 110.f});
			target.Draw(static_cast<NkDrawable &>(t1));

			char l2[224];
			std::snprintf(l2, sizeof(l2),
						  "MouseDeltaThisFrameX/Y() = %+5d , %+5d   <- DE CETTE IMAGE, il "
						  "retombe a 0",
						  dFr, dFrY);
			NkText t2(font, l2, 20u);
			t2.SetFillColor((dFr == 0 && dFrY == 0) ? NkColor2D{120, 210, 140, 255}
													: NkColor2D{110, 190, 240, 255});
			t2.SetPosition({20.f, 140.f});
			target.Draw(static_cast<NkDrawable &>(t2));

			char l3[240];
			std::snprintf(l3, sizeof(l3),
						  "images sans mouvement : %d      position lue : %d , %d", immobiles,
						  NkInput.MouseX(), NkInput.MouseY());
			NkText t3(font, l3, 16u);
			t3.SetFillColor(NkColor2D{175, 180, 188, 255});
			t3.SetPosition({20.f, 176.f});
			target.Draw(static_cast<NkDrawable &>(t3));

			// ⚠️ LE VERDICT À L'ŒIL : après 60 images immobiles, les deux doivent
			//    avoir divergé. C'est ce que Rodolf vient vérifier.
			if (immobiles >= 60) {
				char l4[256];
				std::snprintf(l4, sizeof(l4),
							  "60 images sans bouger : DE CETTE IMAGE = %+d,%+d   et DERNIER "
							  "EVENEMENT garde %+d,%+d",
							  dFr, dFrY, dernierX, dernierY);
				NkText t4(font, l4, 17u);
				const bool attendu = (dFr == 0 && dFrY == 0);
				t4.SetFillColor(attendu ? NkColor2D{120, 210, 140, 255}
										: NkColor2D{235, 120, 100, 255});
				t4.SetPosition({20.f, 212.f});
				target.Draw(static_cast<NkDrawable &>(t4));
			}

			NkText t5(font,
					  "Aucune des deux n'est la mauvaise. PERSISTE : garder la derniere "
					  "DIRECTION connue meme a l'arret",
					  14u);
			t5.SetFillColor(NkColor2D{150, 156, 164, 255});
			t5.SetPosition({20.f, 258.f});
			target.Draw(static_cast<NkDrawable &>(t5));

			NkText t6(font,
					  "(inertie, elan). DE CETTE IMAGE : tourner une camera, tirer un gizmo -- "
					  "tout ce qui doit s'arreter avec la main.",
					  14u);
			t6.SetFillColor(NkColor2D{150, 156, 164, 255});
			t6.SetPosition({20.f, 278.f});
			target.Draw(static_cast<NkDrawable &>(t6));

			NkText t7(font,
					  "Pour LIRE la position : NkInput.MouseX/MouseY (NKEvent). Pour l'ECRIRE : "
					  "NkPlaceMouseInWindow (NKWindow).",
					  14u);
			t7.SetFillColor(NkColor2D{200, 150, 90, 255});
			t7.SetPosition({20.f, 314.f});
			target.Draw(static_cast<NkDrawable &>(t7));
		}

		const Case *cases[2] = {&caseFrame, &casePlacer};
		for (int32 i = 0; i < 2; ++i) {
			NkRectangleShape b({cases[i]->w, cases[i]->h});
			b.SetPosition({cases[i]->x, cases[i]->y});
			b.SetFillColor(cases[i]->cochee ? NkColor2D{235, 180, 60, 255}
											: NkColor2D{40, 44, 50, 255});
			b.SetOutlineColor(NkColor2D{120, 128, 136, 255});
			b.SetOutlineThickness(1.f);
			target.Draw(b);
			if (aPolice) {
				NkText t(font, cases[i]->libelle, 15u);
				t.SetFillColor(NkColor2D::White);
				t.SetPosition({cases[i]->x + 30.f, cases[i]->y + 2.f});
				target.Draw(static_cast<NkDrawable &>(t));
			}
		}

		target.Display();
	}

	return 0;
}
