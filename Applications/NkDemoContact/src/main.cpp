// =============================================================================
// @File    Applications/NkDemoContact/src/main.cpp
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   LA DEMO DU CONTACT : une bille tombe sur un plan et s'arrete — et une
//          case qui coupe la collision pour la faire traverser, a l'oeil.
//
// POURQUOI ELLE EXISTE (26/09/2026)
//   NKCollision et NKPhysics existent et ont ete mesures (bancs vehicule,
//   test_physics). Mais AUCUNE application produit ne montrait deux corps se
//   rencontrer : les « demos » de Noge sont TOUTES des applications console.
//   Un jeu ne se prouve pas dans une console.
//
//   ⚠️ CETTE DEMO PASSE PAR LE VRAI PONT DE NOGE — `NkPhysicsSystem`, celui que
//      `NkEngineLayer` enregistre — et non par un appel direct a NKPhysics. Ce
//      qu'elle montre est donc le CABLAGE du moteur de jeu, pas le module.
//
// VUE
//   Vue de COTE (plan X-Y du monde 3D), projection orthographique. La physique
//   est bien en trois dimensions ; seul l'affichage est plat. C'est ecrit a
//   l'ecran pour que personne n'ait a le deviner.
//
// CE QUI EST AFFICHE, ET POURQUOI C'EST LA LE POINT
//   Pour chaque corps, deux hauteurs : celle du CORPS et celle de sa FORME DE
//   COLLISION, cette derniere RECALCULEE DEPUIS LE MOTEUR (NkTransformShape sur
//   la restShape), jamais depuis la consigne de la scene.
//
//   ⚠️ Ce depot a deja paye « la pente n'existait pas » : une forme sans
//      orientation posee sur un corps oriente s'annulent, et les mesures etaient
//      prises sur un sol plat qu'on croyait incline. La parade n'est pas de
//      relire le code : c'est de LIRE LA GRANDEUR PAR L'AUTRE BOUT. Les deux
//      hauteurs doivent coincider ; si elles divergent, la scene ne dit pas ce
//      qu'on croit — et c'est le premier chiffre a regarder.
//
// MODES
//   (par defaut)  la fenetre : on regarde tomber.
//   --mesure      console, sans fenetre : 400 pas, verdict chiffre. Le meme
//                 montage de scene que la fenetre -- une seule fonction la
//                 construit, pour qu'on ne mesure pas une scene et qu'on en
//                 montre une autre.
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

#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Physics/NkPhysics.h"
#include "Noge/ECS/Systems/NkPhysicsSystem.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::renderer;

namespace {

	// Le pas fixe de la simulation. Le meme dans les deux modes : un banc qui
	// mesure a un autre pas que ce qu'on montre ne mesure pas ce qu'on montre.
	const float32 kPasFixe = 1.f / 120.f;

	const float32 kSolDemiY = 0.5f;	 // demi-epaisseur du sol -> sommet a y = +0,5
	const float32 kRayonBille = 0.5f;
	const float32 kDepartBille = 6.f;

	// Hauteur d'equilibre attendue : la bille pose sur le sommet du sol.
	const float32 kReposAttendu = kSolDemiY + kRayonBille; // 1,0 m

	// ── La scene, montee UNE SEULE FOIS pour les deux modes ──────────────────
	struct Scene {
			ecs::NkWorld *monde = nullptr; // ⚠️ SUR LE TAS : NkWorld pese 791 640
			NkPhysicsSystem *sys = nullptr; //   octets, il ne vit pas sur la pile.
			// ⚠️ `NkEntityId` est une STRUCTURE, pas un entier : `= 0` ne compile
			//    pas. On la laisse a son etat par defaut.
			ecs::NkEntityId sol{}, bille{};

			void Detruire() {
				delete sys;
				delete monde;
				sys = nullptr;
				monde = nullptr;
			}

			// `collisionCoupee` : la bille ne collisionne avec rien (masque nul).
			// C'est le NEGATIF, et il se voit a l'oeil : elle traverse le sol.
			void Construire(bool collisionCoupee) {
				Detruire();
				monde = new ecs::NkWorld();
				sys = new NkPhysicsSystem();

				sol = monde->CreateEntity();
				{
					ecs::NkTransform tf;
					tf.localPosition = {0.f, 0.f, 0.f};
					ecs::NkRigidbody3D rb;
					rb.bodyType = ecs::NkBodyType::Static;
					ecs::NkCollider3D col;
					col.shape = ecs::NkCollider3DShape::Box;
					col.boxSize = {40.f, kSolDemiY * 2.f, 40.f}; // dimensions PLEINES
					monde->Add<ecs::NkTransform>(sol, tf);
					monde->Add<ecs::NkRigidbody3D>(sol, rb);
					monde->Add<ecs::NkCollider3D>(sol, col);
				}

				bille = monde->CreateEntity();
				{
					ecs::NkTransform tf;
					tf.localPosition = {0.f, kDepartBille, 0.f};
					ecs::NkRigidbody3D rb;
					rb.bodyType = ecs::NkBodyType::Dynamic;
					rb.restitution = 0.35f; // un rebond visible, puis le repos
					ecs::NkCollider3D col;
					col.shape = ecs::NkCollider3DShape::Sphere;
					col.sphereRadius = kRayonBille;
					if (collisionCoupee)
						col.layerMask = 0u; // ne rencontre plus rien
					monde->Add<ecs::NkTransform>(bille, tf);
					monde->Add<ecs::NkRigidbody3D>(bille, rb);
					monde->Add<ecs::NkCollider3D>(bille, col);
				}
			}

			void Avancer(int32 pas) {
				for (int32 i = 0; i < pas; ++i)
					sys->Execute(*monde, kPasFixe);
			}

			float32 HauteurCorps(ecs::NkEntityId e) const {
				const ecs::NkTransform *tf = monde->Get<ecs::NkTransform>(e);
				return tf ? tf->localPosition.y : 0.f;
			}

			// ⚠️ LA HAUTEUR LUE PAR L'AUTRE BOUT : on ne rend pas la consigne, on
			//    reconstruit la forme MONDE que le moteur donnera a la detection,
			//    a partir de la forme de repos qu'il a lui-meme calculee.
			bool HauteurForme(ecs::NkEntityId e, float32 &out) const {
				const ecs::NkCollider3D *col = monde->Get<ecs::NkCollider3D>(e);
				if (!col || col->physicsBodyId == 0)
					return false;
				const physics::NkRigidBody *b =
					sys->World().GetBody(static_cast<physics::NkBodyId>(col->physicsBodyId));
				if (!b)
					return false;
				out = physics::NkTransformShape(b->restShape, b->position, b->orientation).p0.y;
				return true;
			}
	};

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
		//    est vide, on l'ECRIT plutot que de mettre un « ? » — ce depot a paye
		//    « 21 sites sautent argv[0], zero ligne ne le dit ».
		const char *exe =
			state.args.IsEmpty() ? "(chemin non transmis par l'entree)" : state.args[0].CStr();
		std::snprintf(out, taille, "compile le %s a %s  |  %s", __DATE__, __TIME__, exe);
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Mode --mesure : pas de fenetre, un verdict chiffre.
	// ─────────────────────────────────────────────────────────────────────────
	int Mesurer(const NkEntryState &state) {
		char ident[512];
		Identite(ident, sizeof(ident), state);
		std::printf("=== NkDemoContact --mesure\n");
		std::printf("%s\n\n", ident);

		int echecs = 0;
		Scene sc;

		// -- 1. la bille tombe, rencontre le sol, et s'y arrete ----------------
		sc.Construire(false);
		sc.Avancer(400); // ~3,3 s
		const float32 hBille = sc.HauteurCorps(sc.bille);
		float32 fBille = 0.f, fSol = 0.f;
		const bool aFB = sc.HauteurForme(sc.bille, fBille);
		const bool aFS = sc.HauteurForme(sc.sol, fSol);

		std::printf("  corps bille  y = %8.4f   (attendu %.4f)\n", hBille, kReposAttendu);
		std::printf("  forme bille  y = %8.4f   %s\n", fBille, aFB ? "" : "(ILLISIBLE)");
		std::printf("  corps sol    y = %8.4f\n", sc.HauteurCorps(sc.sol));
		std::printf("  forme sol    y = %8.4f   %s\n", fSol, aFS ? "" : "(ILLISIBLE)");

		// ⚠️ LA CONDITION D'ESSAI SE MESURE AVANT LE RESULTAT. Si la forme ne
		//    suit pas le corps, la scene ne dit pas ce qu'on croit, et tout ce
		//    qui suit porte sur autre chose.
		const bool formeSuit = aFB && aFS && (hBille - fBille < 0.001f) &&
							   (hBille - fBille > -0.001f) && (fSol < 0.001f) && (fSol > -0.001f);
		std::printf("\n  [%s] la FORME suit le CORPS (condition d'essai)\n", formeSuit ? "OK" : "ECHEC");
		if (!formeSuit) {
			std::printf("        ecart bille corps-forme = %.4f m ; forme du sol = %.4f m\n",
						hBille - fBille, fSol);
			++echecs;
		}

		const bool posee = (hBille > kReposAttendu - 0.06f) && (hBille < kReposAttendu + 0.06f);
		std::printf("  [%s] la bille S'ARRETE sur le sol (%.4f m, tolerance 6 cm)\n",
					posee ? "OK" : "ECHEC", hBille);
		if (!posee)
			++echecs;

		// -- 2. le NEGATIF : collision coupee -> elle doit TRAVERSER -----------
		// Sans lui, une bille bloquee par un defaut quelconque passerait pour une
		// bille arretee par le sol.
		sc.Construire(true);
		sc.Avancer(400);
		const float32 hTrav = sc.HauteurCorps(sc.bille);
		const bool traverse = hTrav < -5.f;
		std::printf("  [%s] NEGATIF collision coupee -> elle traverse (y = %.4f, exige < -5)\n",
					traverse ? "OK" : "ECHEC", hTrav);
		if (!traverse)
			++echecs;

		sc.Detruire();
		std::printf("\nStatus: %s (%d echec(s))\n", echecs == 0 ? "VERT" : "ROUGE", echecs);
		return echecs == 0 ? 0 : 1;
	}

} // namespace

int nkmain(const NkEntryState &state) {
	for (uint32 i = 0; i < state.args.Size(); ++i)
		if (state.args[i] == "--mesure")
			return Mesurer(state);

	NkWindowConfig cfg;
	cfg.title = "NkDemoContact - une bille tombe sur un plan";
	cfg.width = 960;
	cfg.height = 640;
	cfg.centered = true;
	cfg.resizable = true;

	NkWindow window(cfg);
	if (!window.IsOpen()) {
		logger.Error("[NkDemoContact] creation fenetre echouee");
		return -1;
	}

	NkContextDesc desc;
	desc.api = NkGraphicsApi::NK_GFX_API_OPENGL;
	NkRenderWindow target(window, desc);
	if (!target.IsValid()) {
		logger.Error("[NkDemoContact] cible de rendu invalide");
		return -2;
	}

	// ⚠️ `NkFont` vit dans DEUX espaces de noms : on qualifie.
	renderer::NkFont font;
	const bool aPolice = font.LoadFromFile(*target.GetRenderer(), "Resources/Fonts/Antonio-Bold.ttf");
	if (!aPolice)
		logger.Warn("[NkDemoContact] police introuvable -> les libelles ne s'afficheront pas");

	char ident[512];
	Identite(ident, sizeof(ident), state);
	logger.Info("[NkDemoContact] identite de construction : {0}", ident);

	Case caseCol{20.f, 560.f, 20.f, 20.f, false, "COUPER LA COLLISION (la bille doit traverser le sol)"};
	Case casePause{20.f, 588.f, 20.f, 20.f, false, "Pause"};

	Scene sc;
	sc.Construire(caseCol.cochee);

	float32 sourisX = 0.f, sourisY = 0.f;
	auto &events = NkEvents();

	// Monde -> ecran, vue de cote : x droite, y VERS LE HAUT.
	const float32 echelle = 42.f; // pixels par metre
	const float32 origX = 480.f, origY = 500.f;
	auto sx = [&](float32 wx) { return origX + wx * echelle; };
	auto sy = [&](float32 wy) { return origY - wy * echelle; };

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
					if (DansRect(sourisX, sourisY, caseCol.x, caseCol.y, 460.f, caseCol.h)) {
						caseCol.cochee = !caseCol.cochee;
						sc.Construire(caseCol.cochee); // la scene se remonte : la bille repart de haut
					} else if (DansRect(sourisX, sourisY, casePause.x, casePause.y, 460.f, casePause.h)) {
						casePause.cochee = !casePause.cochee;
					} else {
						sc.Construire(caseCol.cochee); // clic ailleurs = relacher a nouveau
					}
				}
			}
		}

		if (!casePause.cochee)
			sc.Avancer(2); // 2 pas de 1/120 s par image ~ temps reel a 60 Hz

		target.Clear(NkColor2D{22, 24, 28, 255});

		// Le sol, vu de cote.
		{
			NkRectangleShape r({40.f * echelle, kSolDemiY * 2.f * echelle});
			r.SetPosition({sx(-20.f), sy(kSolDemiY)});
			r.SetFillColor(NkColor2D{52, 60, 68, 255});
			r.SetOutlineColor(NkColor2D{96, 104, 112, 255});
			r.SetOutlineThickness(1.f);
			target.Draw(r);
		}

		// La bille.
		const float32 hB = sc.HauteurCorps(sc.bille);
		{
			NkCircleShape c(kRayonBille * echelle, 48u);
			c.SetPosition({sx(0.f) - kRayonBille * echelle, sy(hB) - kRayonBille * echelle});
			c.SetFillColor(caseCol.cochee ? NkColor2D{200, 90, 70, 255} : NkColor2D{235, 180, 60, 255});
			target.Draw(c);
		}

		// Les cases.
		const Case *cases[2] = {&caseCol, &casePause};
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
			id.SetPosition({20.f, 14.f});
			target.Draw(static_cast<NkDrawable &>(id));

			char l1[224];
			std::snprintf(l1, sizeof(l1),
						  "Vue de COTE (plan X-Y) - la physique est bien en 3D. Pont ECS de Noge : "
						  "NkPhysicsSystem.");
			NkText t1(font, l1, 15u);
			t1.SetFillColor(NkColor2D{150, 156, 164, 255});
			t1.SetPosition({20.f, 36.f});
			target.Draw(static_cast<NkDrawable &>(t1));

			float32 fB = 0.f, fS = 0.f;
			const bool aFB = sc.HauteurForme(sc.bille, fB);
			const bool aFS = sc.HauteurForme(sc.sol, fS);
			char l2[256];
			std::snprintf(l2, sizeof(l2),
						  "bille : corps y = %6.3f m   forme de collision y = %s%6.3f m",
						  hB, aFB ? "" : "(illisible) ", fB);
			NkText t2(font, l2, 19u);
			t2.SetFillColor((aFB && (hB - fB < 0.01f) && (hB - fB > -0.01f))
								? NkColor2D{120, 210, 140, 255}
								: NkColor2D{235, 120, 100, 255});
			t2.SetPosition({20.f, 62.f});
			target.Draw(static_cast<NkDrawable &>(t2));

			char l3[256];
			std::snprintf(l3, sizeof(l3), "sol   : corps y = %6.3f m   forme de collision y = %s%6.3f m",
						  sc.HauteurCorps(sc.sol), aFS ? "" : "(illisible) ", fS);
			NkText t3(font, l3, 19u);
			t3.SetFillColor((aFS && fS < 0.01f && fS > -0.01f) ? NkColor2D{120, 210, 140, 255}
															   : NkColor2D{235, 120, 100, 255});
			t3.SetPosition({20.f, 86.f});
			target.Draw(static_cast<NkDrawable &>(t3));

			char l4[224];
			std::snprintf(l4, sizeof(l4),
						  "Les deux hauteurs doivent COINCIDER. Si elles divergent, la scene ne dit "
						  "pas ce qu'on croit.");
			NkText t4(font, l4, 14u);
			t4.SetFillColor(NkColor2D{150, 156, 164, 255});
			t4.SetPosition({20.f, 110.f});
			target.Draw(static_cast<NkDrawable &>(t4));

			NkText t5(font, "Clic dans le vide : relacher la bille a nouveau.", 14u);
			t5.SetFillColor(NkColor2D{150, 156, 164, 255});
			t5.SetPosition({20.f, 532.f});
			target.Draw(static_cast<NkDrawable &>(t5));
		}

		target.Display();
	}

	sc.Detruire();
	return 0;
}
