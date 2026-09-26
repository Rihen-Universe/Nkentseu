// =============================================================================
// @File    Applications/NkDemoPiedsSurLaPente/src/main.cpp
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   ÉPROUVETTE : deux jambes se posent sur une pente — ou la traversent,
//          selon qu'on branche, ou non, le monde physique à l'IK des pieds.
//
// ⚠️ CE FICHIER NE DESSINE QUE. La scène vit dans `scene.cpp`, et la frontière
//    est `scene.h` : NKCanvas et NKRenderer **ne peuvent pas cohabiter dans une
//    même unité de compilation** (une quinzaine de types définis deux fois).
//    Voir l'en-tête de `scene.h`.
//
// CE QU'ELLE PROUVE, ET QUI EST UN CÂBLAGE
//   `NkFootIKSystem` sait interroger un monde physique (`SetPhysicsWorld`) et,
//   à défaut, retombe sur un plan plat analytique. Jusqu'au 26/09,
//   `SetPhysicsWorld` n'était appelé NULLE PART et le système n'était même pas
//   enregistré par `NkEngineLayer` : les pieds suivaient TOUJOURS le plan plat.
//   La case fait basculer entre les deux, et la différence se voit à l'œil.
//
// MODES
//   (par défaut)  la fenêtre.
//   --mesure      console, sans fenêtre, verdict chiffré, même montage.
//
// @License Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================

#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKLogger/NkLog.h"

#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/Renderer/Shapes/NkRectangleShape.h"
#include "NKCanvas/Renderer/Shapes/NkCircleShape.h"
#include "NKCanvas/Renderer/Resources/NkFont.h"
#include "NKCanvas/Renderer/Resources/NkSprite.h" // NkText

#include "scene.h"

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

	void Identite(char *out, uint32 taille, const NkEntryState &state) {
		// ⚠️ `argv[0]` n'existe pas ici : `NkEntryState` porte `args`. Quand il
		//    manque, on l'ÉCRIT plutôt que d'afficher un « ? ».
		const char *exe =
			state.args.IsEmpty() ? "(chemin non transmis par l'entree)" : state.args[0].CStr();
		// Les DEUX unites de compilation, parce qu'une seule ment : modifier
		// scene.cpp ne change pas __TIME__ de main.cpp (constate le 26/09).
		std::snprintf(out, taille, "main %s a %s | scene %s | %s", __DATE__, __TIME__,
					  eprouvette::DateCompilationScene(), exe);
	}

} // namespace

int nkmain(const NkEntryState &state) {
	char ident[512];
	Identite(ident, sizeof(ident), state);

	for (uint32 i = 0; i < state.args.Size(); ++i)
		if (state.args[i] == "--mesure") {
			std::printf("=== NkDemoPiedsSurLaPente --mesure\n%s\n\n", ident);
			const int echecs = eprouvette::Mesurer();
			std::printf("\nStatus: %s (%d echec(s))\n", echecs == 0 ? "VERT" : "ROUGE", echecs);
			return echecs == 0 ? 0 : 1;
		}

	NkWindowConfig cfg;
	cfg.title = "NkDemoPiedsSurLaPente - eprouvette : deux jambes et une pente";
	cfg.width = 1000;
	cfg.height = 660;
	cfg.centered = true;
	cfg.resizable = true;

	NkWindow window(cfg);
	if (!window.IsOpen()) {
		logger.Error("[NkDemoPiedsSurLaPente] creation fenetre echouee");
		return -1;
	}

	NkContextDesc desc;
	desc.api = NkGraphicsApi::NK_GFX_API_OPENGL;
	NkRenderWindow target(window, desc);
	if (!target.IsValid()) {
		logger.Error("[NkDemoPiedsSurLaPente] cible de rendu invalide");
		return -2;
	}

	// ⚠️ `NkFont` vit dans DEUX espaces de noms : on qualifie.
	renderer::NkFont font;
	const bool aPolice = font.LoadFromFile(*target.GetRenderer(), "Resources/Fonts/Antonio-Bold.ttf");
	if (!aPolice)
		logger.Warn("[NkDemoPiedsSurLaPente] police introuvable -> pas de libelles");

	logger.Info("[NkDemoPiedsSurLaPente] identite de construction : {0}", ident);

	Case caseFil{20.f, 580.f, 20.f, 20.f, true,
				 "BRANCHER LE MONDE PHYSIQUE a l'IK  (decochez : les pieds traversent la pente)"};
	Case casePause{20.f, 608.f, 20.f, 20.f, false, "Pause du deplacement"};

	eprouvette::Construire(caseFil.cochee);

	// Les os de CesiumMan, designes par leur NOM. Importe UNE FOIS : ce bloc
	// ne fait pas partie de la scene de la pente, il en dit l'etat.
	eprouvette::Import im;
	const bool aCesium = eprouvette::ImporterCesiumMan(im);

	float32 sourisX = 0.f, sourisY = 0.f, x = -5.f, sens = 1.f;
	auto &events = NkEvents();

	const float32 echelle = 58.f;
	const float32 origX = 500.f, origY = 470.f;
	auto sx = [&](float32 wx) { return origX + wx * echelle; };
	auto sy = [&](float32 wy) { return origY - wy * echelle; };

	// Un segment épais, tracé par petits disques : NKCanvas n'a pas de ligne
	// épaisse, et cette démo n'est pas là pour en écrire une.
	auto trait = [&](float32 ax, float32 ay, float32 bx, float32 by, float32 ep, NkColor2D c) {
		const float32 dx = bx - ax, dy = by - ay;
		const float32 len = math::NkSqrt(dx * dx + dy * dy);
		if (len < 0.001f)
			return;
		const int32 n = static_cast<int32>(len / 3.f) + 1;
		for (int32 i = 0; i <= n; ++i) {
			const float32 t = static_cast<float32>(i) / static_cast<float32>(n);
			NkCircleShape p(ep * 0.5f, 10u);
			p.SetPosition({ax + dx * t - ep * 0.5f, ay + dy * t - ep * 0.5f});
			p.SetFillColor(c);
			target.Draw(p);
		}
	};

	while (window.IsOpen()) {
		while (NkEvent *ev = events.PollEvent()) {
			if (ev->Is<NkWindowCloseEvent>()) {
				window.Close();
				break;
			}
			if (auto *mm = ev->As<NkMouseMoveEvent>()) {
				sourisX = static_cast<float32>(mm->GetX());
				sourisY = static_cast<float32>(mm->GetY());
			}
			if (auto *mb = ev->As<NkMouseButtonPressEvent>()) {
				if (mb->GetButton() == NkMouseButton::NK_MB_LEFT) {
					if (DansRect(sourisX, sourisY, caseFil.x, caseFil.y, 640.f, caseFil.h)) {
						caseFil.cochee = !caseFil.cochee;
						eprouvette::Construire(caseFil.cochee);
					} else if (DansRect(sourisX, sourisY, casePause.x, casePause.y, 640.f,
										casePause.h)) {
						casePause.cochee = !casePause.cochee;
					}
				}
			}
		}

		if (!casePause.cochee) {
			x += sens * 0.022f;
			if (x > 5.f) {
				x = 5.f;
				sens = -1.f;
			} else if (x < -5.f) {
				x = -5.f;
				sens = 1.f;
			}
		}
		eprouvette::Placer(x, 2);

		eprouvette::Etat e;
		eprouvette::Lire(x, e);

		target.Clear(NkColor2D{22, 24, 28, 255});

		// La pente, tracée depuis le SOL LU et non depuis la consigne. Si elle
		// n'existait pas, ce trait serait horizontal — et cela se verrait.
		if (e.solTracable)
			trait(sx(-7.f), sy(e.solGaucheY), sx(7.f), sy(e.solDroiteY), 5.f,
				  NkColor2D{96, 104, 112, 255});

		// Les deux jambes : hanche -> cuisse -> mollet -> pied.
		{
			const int32 jambeG[4] = {eprouvette::kHip, eprouvette::kLThigh, eprouvette::kLCalf,
									 eprouvette::kLFoot};
			const int32 jambeD[4] = {eprouvette::kHip, eprouvette::kRThigh, eprouvette::kRCalf,
									 eprouvette::kRFoot};
			const int32 *jambes[2] = {jambeG, jambeD};
			const NkColor2D cols[2] = {NkColor2D{235, 180, 60, 255}, NkColor2D{120, 190, 235, 255}};
			for (int32 j = 0; j < 2; ++j)
				for (int32 s = 0; s < 3; ++s)
					trait(sx(e.osX[jambes[j][s]]), sy(e.osY[jambes[j][s]]),
						  sx(e.osX[jambes[j][s + 1]]), sy(e.osY[jambes[j][s + 1]]), 8.f, cols[j]);
		}

		const Case *cases[2] = {&caseFil, &casePause};
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

		if (aPolice) {
			NkText id(font, ident, 13u);
			id.SetFillColor(NkColor2D{150, 156, 164, 255});
			id.SetPosition({20.f, 14.f});
			target.Draw(static_cast<NkDrawable &>(id));

			NkText ep(font,
					  "EPROUVETTE - pas un personnage : squelette procedural a 9 os. Aucun "
					  "humanoide rigge n'est atteignable (ses os n'ont pas de nom).",
					  14u);
			ep.SetFillColor(NkColor2D{200, 150, 90, 255});
			ep.SetPosition({20.f, 34.f});
			target.Draw(static_cast<NkDrawable &>(ep));

			char l1[224];
			std::snprintf(l1, sizeof(l1),
						  "Pente LUE par deux raycasts : %.2f deg   (consigne %.2f deg)%s",
						  e.penteLueDeg, eprouvette::PenteConsigneDeg(),
						  e.penteLisible ? "" : "   -- AUCUN CONTACT : le rayon ne touche rien");
			const bool penteOk = e.penteLisible &&
								 e.penteLueDeg > eprouvette::PenteConsigneDeg() - 0.5f &&
								 e.penteLueDeg < eprouvette::PenteConsigneDeg() + 0.5f;
			NkText t1(font, l1, 19u);
			t1.SetFillColor(penteOk ? NkColor2D{120, 210, 140, 255} : NkColor2D{235, 120, 100, 255});
			t1.SetPosition({20.f, 58.f});
			target.Draw(static_cast<NkDrawable &>(t1));

			const float32 ecart =
				e.osY[eprouvette::kLFoot] - (e.solSousPiedG + eprouvette::HauteurSemelle());
			char l2[224];
			std::snprintf(l2, sizeof(l2),
						  "Sous le pied gauche : sol %6.3f m   pied %6.3f m   ecart %+6.3f m",
						  e.solSousPiedG, e.osY[eprouvette::kLFoot], ecart);
			NkText t2(font, l2, 19u);
			t2.SetFillColor((e.solLisible && ecart > -0.06f && ecart < 0.06f)
								? NkColor2D{120, 210, 140, 255}
								: NkColor2D{235, 120, 100, 255});
			t2.SetPosition({20.f, 84.f});
			target.Draw(static_cast<NkDrawable &>(t2));

			NkText t3(font,
					  "Case cochee : l'IK interroge le monde physique. Decochee : elle retombe "
					  "sur son plan plat a y = 0.",
					  14u);
			t3.SetFillColor(NkColor2D{150, 156, 164, 255});
			t3.SetPosition({20.f, 108.f});
			target.Draw(static_cast<NkDrawable &>(t3));

			NkText t4(font, "La pente est tracee depuis le SOL LU, jamais depuis la consigne.",
					  14u);
			t4.SetFillColor(NkColor2D{150, 156, 164, 255});
			t4.SetPosition({20.f, 552.f});
			target.Draw(static_cast<NkDrawable &>(t4));

			// ── Les os de CesiumMan, PAR LEUR NOM ────────────────────────────
			// Le nom est ecrit a cote de son indice : c'est la seule facon de
			// verifier a l'oeil qu'on plie l'os qu'on croit plier.
			if (aCesium) {
				char c1[224];
				std::snprintf(c1, sizeof(c1),
							  "CesiumMan.glb : %d os, %d NOMMES  |  %s = os %d   %s = os %d   "
							  "%s = os %d",
							  im.osTotal, im.osNommes, im.cuisse.nom, im.cuisse.indice,
							  im.mollet.nom, im.mollet.indice, im.pied.nom, im.pied.indice);
				NkText c(font, c1, 15u);
				c.SetFillColor((im.osNommes == im.osTotal && im.pied.indice >= 0)
							   ? NkColor2D{120, 210, 140, 255}
							   : NkColor2D{235, 120, 100, 255});
				c.SetPosition({20.f, 500.f});
				target.Draw(static_cast<NkDrawable &>(c));

				// ⚠️ ET CE QUI EMPECHE ENCORE DE S'EN SERVIR, ecrit a l'ecran
				//    plutot que tue dans un rapport : le pont lit la pose LOCALE
				//    comme si elle etait monde.
				char c2[224];
				std::snprintf(c2, sizeof(c2),
							  "...mais %d de ces os ont un PARENT : pose locale y=%.4f, pose monde "
							  "y=%.4f. L'IK lit la pose MONDE : le releve du bas le mesure.",
							  im.osAvecParent, im.piedLocalY, im.piedMondeY);
				NkText c2t(font, c2, 14u);
				// Plus un avertissement : l'ecart locale/monde est desormais la
			// CONDITION qui rend le releve CesiumMan plus fort que l'eprouvette.
			c2t.SetFillColor(NkColor2D{150, 156, 164, 255});
				c2t.SetPosition({20.f, 522.f});
				target.Draw(static_cast<NkDrawable &>(c2t));
			} else {
				NkText c(font, im.refus[0] != '\0' ? im.refus : "CesiumMan.glb non importe", 15u);
				c.SetFillColor(NkColor2D{235, 120, 100, 255});
				c.SetPosition({20.f, 500.f});
				target.Draw(static_cast<NkDrawable &>(c));
			}
		}

		target.Display();
	}

	eprouvette::Detruire();
	return 0;
}
