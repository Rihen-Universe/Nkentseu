// =============================================================================
// @File    Applications/NkDemoCurseur/src/main.cpp
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   LA DEMO DU CURSEUR : quatre zones, quatre formes, et une case pour
//          confiner la souris a la fenetre.
//
// POURQUOI ELLE EXISTE (utilisateur reel, 26/09/2026)
//   « les methodes pour curseur dans NKWindow ne fonctionnent pas »
//
//   Le banc `NkWindowSonde` a pu mesurer le confinement, la capture et
//   l'affichage — en INTERROGEANT Windows — mais il ne peut PAS mesurer la
//   FORME du curseur : elle ne s'applique que si une souris SURVOLE la fenetre,
//   et les fenetres du banc sont invisibles, sans aucune entree injectee.
//
//   ⚠️ SEUL UN HUMAIN QUI SURVOLE PEUT LE JUGER. C'est la raison d'etre de cette
//      demo, et c'est aussi, exactement, la raison pour laquelle l'utilisateur
//      a cru que « ca ne marche pas » : la forme est PERSISTANTE.
//
// CE QU'ELLE MONTRE, ET QUI EST LE COEUR DU SUJET
//   `SetCursor` est rappele A CHAQUE IMAGE avec la forme de la zone survolee.
//   Appele une seule fois, Windows remet la fleche au premier mouvement de
//   souris (il envoie WM_SETCURSOR a chaque deplacement). La case « rappeler
//   chaque image » permet de le VOIR : decochez-la, bougez la souris, la forme
//   retombe a la fleche. C'est le defaut de l'utilisateur, reproduit a volonte.
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
#include "NKCanvas/Renderer/Resources/NkFont.h"
#include "NKCanvas/Renderer/Resources/NkSprite.h" // NkText

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::renderer;

namespace {

	// Les quatre zones. Le LIBELLE dit la forme attendue : Rodolf compare ce
	// qu'il voit a ce qui est demande, sans avoir a s'en souvenir.
	struct Zone {
			const char *libelle;
			NkWindow::NkCursorType forme;
			NkColor2D fond;
	};

	const Zone kZones[4] = {
		{"FLECHE (Arrow)", NkWindow::NkCursorType::Arrow, NkColor2D{48, 54, 62, 255}},
		{"TEXTE (I-beam)", NkWindow::NkCursorType::TextInput, NkColor2D{40, 62, 58, 255}},
		{"MAIN (Hand)", NkWindow::NkCursorType::Hand, NkColor2D{62, 52, 40, 255}},
		{"REDIM. HORIZONTAL", NkWindow::NkCursorType::ResizeWE, NkColor2D{56, 42, 62, 255}},
	};

	// Une case a cocher, dessinee a la main : cette demo ne depend d'aucun
	// systeme d'interface, pour qu'elle reste lancable meme si NKGui bouge.
	struct Case {
			float32 x, y, w, h;
			bool cochee;
			const char *libelle;
	};

	bool DansRect(float32 px, float32 py, float32 x, float32 y, float32 w, float32 h) {
		return px >= x && px < x + w && py >= y && py < y + h;
	}

} // namespace

int nkmain(const NkEntryState &state) {
	NkWindowConfig cfg;
	cfg.title = "NkDemoCurseur - survolez les quatre zones";
	cfg.width = 900;
	cfg.height = 600;
	cfg.centered = true;
	cfg.resizable = true;

	NkWindow window(cfg);
	if (!window.IsOpen()) {
		logger.Error("[NkDemoCurseur] creation fenetre echouee");
		return -1;
	}

	NkContextDesc desc;
	desc.api = NkGraphicsApi::NK_GFX_API_OPENGL;
	NkRenderWindow target(window, desc);
	if (!target.IsValid()) {
		logger.Error("[NkDemoCurseur] cible de rendu invalide");
		return -2;
	}

	// ⚠️ `NkFont` VIT DANS DEUX ESPACES DE NOMS : celui de bas niveau
	//    (nkentseu::NkFont) et celui de NKCanvas. On qualifie, sinon le
	//    compilateur refuse -- et le message ne dit pas lequel il fallait.
	renderer::NkFont font;
	const bool aPolice = font.LoadFromFile(*target.GetRenderer(), "Resources/Fonts/Antonio-Bold.ttf");
	if (!aPolice)
		logger.Warn("[NkDemoCurseur] police introuvable -> les libelles ne s'afficheront pas");

	// ── L'IDENTITE DE CONSTRUCTION, en premiere ligne ────────────────────────
	// Exigence posee le 26/09 apres qu'une heure a ete perdue a trois, faute de
	// savoir quel binaire avait ete lance. Elle est ECRITE A L'ECRAN et dans le
	// journal : une capture d'ecran suffit alors a dire de quel binaire on parle.
	char ident[512];
	{
		// ⚠️ `argv[0]` N'EXISTE PAS ICI : `NkEntryState` porte `args`, un
		//    NkVector<NkString>, et le depot a deja paye « 21 sites sautent
		//    argv[0], zero ligne ne le dit ». On prend donc le premier argument
		//    s'il existe, et on le DIT quand il manque plutot que d'ecrire "?".
		const char *exe = state.args.IsEmpty() ? "(chemin non transmis par l'entree)"
											   : state.args[0].CStr();
		std::snprintf(ident, sizeof(ident), "compile le %s a %s  |  %s", __DATE__, __TIME__, exe);
		logger.Info("[NkDemoCurseur] identite de construction : {0}", ident);
	}

	Case caseRappel{20.f, 500.f, 20.f, 20.f, true, "Rappeler SetCursor a CHAQUE image (decochez pour voir le defaut)"};
	Case caseClip{20.f, 530.f, 20.f, 20.f, false, "Confiner la souris a la fenetre (ClipMouseToClient)"};

	float32 sourisX = 0.f, sourisY = 0.f;
	int32 zoneSurvolee = -1;
	auto &events = NkEvents();

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
					if (DansRect(sourisX, sourisY, caseRappel.x, caseRappel.y, 420.f, caseRappel.h))
						caseRappel.cochee = !caseRappel.cochee;
					if (DansRect(sourisX, sourisY, caseClip.x, caseClip.y, 420.f, caseClip.h)) {
						caseClip.cochee = !caseClip.cochee;
						// ⚠️ LE CONFINEMENT EST UN ACTE SUR LE POSTE : il ne se
						//    pose que sur un clic EXPLICITE, jamais au demarrage,
						//    et il se relache a la fermeture (plus bas).
						window.ClipMouseToClient(caseClip.cochee);
					}
				}
			}
		}

		const math::NkVec2u taille = target.GetSize();
		const float32 W = static_cast<float32>(taille.x);
		const float32 largeurZone = W / 4.f;
		const float32 hautZone = 120.f, basZone = 360.f;

		zoneSurvolee = -1;
		for (int32 i = 0; i < 4; ++i)
			if (DansRect(sourisX, sourisY, i * largeurZone, hautZone, largeurZone, basZone - hautZone))
				zoneSurvolee = i;

		// ── LE POINT DE LA DEMO ──────────────────────────────────────────────
		// Rappele CHAQUE IMAGE quand la case est cochee. Sinon : une seule fois,
		// a l'entree dans la zone — et Windows remet la fleche des le mouvement
		// suivant. C'est le defaut de l'utilisateur, reproductible a volonte.
		static int32 derniereZone = -2;
		if (caseRappel.cochee) {
			window.SetCursor(zoneSurvolee >= 0 ? kZones[zoneSurvolee].forme
											   : NkWindow::NkCursorType::Arrow);
		} else if (zoneSurvolee != derniereZone) {
			window.SetCursor(zoneSurvolee >= 0 ? kZones[zoneSurvolee].forme
											   : NkWindow::NkCursorType::Arrow);
		}
		derniereZone = zoneSurvolee;

		target.Clear(NkColor2D{24, 26, 30, 255});

		for (int32 i = 0; i < 4; ++i) {
			NkRectangleShape r({largeurZone - 6.f, basZone - hautZone - 6.f});
			r.SetPosition({i * largeurZone + 3.f, hautZone + 3.f});
			r.SetFillColor(kZones[i].fond);
			r.SetOutlineColor(i == zoneSurvolee ? NkColor2D{235, 180, 60, 255}
												: NkColor2D{70, 76, 84, 255});
			r.SetOutlineThickness(i == zoneSurvolee ? 3.f : 1.f);
			target.Draw(r);
			if (aPolice) {
				NkText t(font, kZones[i].libelle, 18u);
				t.SetFillColor(NkColor2D::White);
				t.SetPosition({i * largeurZone + 14.f, hautZone + 16.f});
				target.Draw(static_cast<NkDrawable &>(t));
			}
		}

		const Case *cases[2] = {&caseRappel, &caseClip};
		for (int32 i = 0; i < 2; ++i) {
			NkRectangleShape b({cases[i]->w, cases[i]->h});
			b.SetPosition({cases[i]->x, cases[i]->y});
			b.SetFillColor(cases[i]->cochee ? NkColor2D{235, 180, 60, 255} : NkColor2D{40, 44, 50, 255});
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
			id.SetPosition({20.f, 16.f});
			target.Draw(static_cast<NkDrawable &>(id));

			char attendu[192];
			std::snprintf(attendu, sizeof(attendu), "Sous le pointeur, la forme ATTENDUE est : %s",
						  zoneSurvolee >= 0 ? kZones[zoneSurvolee].libelle
											: "FLECHE (hors des zones)");
			NkText a(font, attendu, 20u);
			a.SetFillColor(NkColor2D{235, 180, 60, 255});
			a.SetPosition({20.f, 60.f});
			target.Draw(static_cast<NkDrawable &>(a));
		}

		target.Display();
	}

	// ⚠️ ON RELACHE LE CONFINEMENT A LA FERMETURE, toujours. Une application qui
	//    quitte en laissant la souris de Rodolf enfermee dans un rectangle qui
	//    n'existe plus serait pire que le defaut qu'elle vient de montrer.
	window.ClipMouseToClient(false);
	return 0;
}
