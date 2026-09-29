//
// NkEditeurEntrees.cpp
// =============================================================================
// Description :
//   « Jouer » : le clavier, la manette et le doigt vont au jeu tant que la vue
//   est active et la scene EN JEU. Voir NkEditeurEntrees.h pour les regles.
//
// PRE-ENREGISTREMENT du banc (ecrit avant le premier chiffre) :
//   (e25) EDITION : rien n'est pris, meme une touche liee ; Jouer donne la main
//         au jeu, et Espace y devient Sauter -- l'evenement est PRIS (NKGui ne
//         le verra pas : Espace ne relance pas « Jouer »)
//   (e26) le RELACHEMENT d'une touche passe aussi a l'editeur ; une frappe de
//         texte est prise (un champ focalise ne la recoit pas)
//   (e27) Echap rend la main : pris, le jeu CONTINUE (etat JEU), les actions
//         sont relachees ; la touche suivante va a l'editeur
//   (e28) un clic dans le viseur rend la main au jeu ; un clic ailleurs la lui
//         reprend (le clic, lui, n'est jamais pris : les outils en ont besoin)
//   (e29) la manette de banc : Sud -> Sauter quand le jeu a la main ; ignoree
//         quand il ne l'a pas
//   (e30) Arreter (EDITION) : la main revient a l'editeur, tout est relache
//   (e31) ajoute a la relecture : une boite modale ou un menu ouvert reprend la
//         main (Entree et Echap sont a la boite, pas au jeu)
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurEntrees.h"

#include "Editeur/NkEditeurActions.h"
#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkTouchEvent.h"

#include <cstdio>

namespace nkentseu {
	namespace editeur {

		namespace {

			bool DansRect(const nkgui::NkRect &r, float32 x, float32 y) noexcept {
				return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
			}

			void PrendreLaMain(NkEditeurEntrees &e) noexcept {
				e.jeuALaMain = true;
			}

			void RendreLaMain(NkEditeurEntrees &e) noexcept {
				e.jeuALaMain = false;
				// Le jeu ne verra pas les relachements a venir : on relache tout
				// MAINTENANT, sinon une touche tenue au moment d'Echap resterait
				// enfoncee dans le jeu pour toujours.
				e.jeu.ToutRelacher();
			}

		} // namespace

		void NkEditeurEntreesParDefaut(NkEditeurEntrees &e) {
			unkeny::NkLiaisons &l = e.jeu.Liaisons();
			l.Vider();
			l.NommerAction(NK_JEU_AVANCER, "Avancer");
			l.NommerAction(NK_JEU_MONTER, "Monter");
			l.NommerAction(NK_JEU_SAUTER, "Sauter");
			l.NommerAction(NK_JEU_ACTION, "Action");
			// Clavier : ZQSD en AZERTY et WASD en QWERTY sont les MEMES touches
			// physiques, et NkKey les nomme par leur position QWERTY.
			l.LierTouche(NK_JEU_AVANCER, NkKey::NK_D, 1.f);
			l.LierTouche(NK_JEU_AVANCER, NkKey::NK_A, -1.f);
			l.LierTouche(NK_JEU_AVANCER, NkKey::NK_RIGHT, 1.f);
			l.LierTouche(NK_JEU_AVANCER, NkKey::NK_LEFT, -1.f);
			l.LierTouche(NK_JEU_MONTER, NkKey::NK_W, 1.f);
			l.LierTouche(NK_JEU_MONTER, NkKey::NK_S, -1.f);
			l.LierTouche(NK_JEU_MONTER, NkKey::NK_UP, 1.f);
			l.LierTouche(NK_JEU_MONTER, NkKey::NK_DOWN, -1.f);
			l.LierTouche(NK_JEU_SAUTER, NkKey::NK_SPACE);
			l.LierTouche(NK_JEU_ACTION, NkKey::NK_E);
			// Manette : pour TOUS les joueurs, chacun sur la sienne.
			l.LierAxe(NK_JEU_AVANCER, NkGamepadAxis::NK_GP_AXIS_LX, 1.f, 0.2f);
			l.LierAxe(NK_JEU_MONTER, NkGamepadAxis::NK_GP_AXIS_LY, 1.f, 0.2f);
			l.LierBouton(NK_JEU_AVANCER, NkGamepadButton::NK_GP_DPAD_RIGHT, 1.f);
			l.LierBouton(NK_JEU_AVANCER, NkGamepadButton::NK_GP_DPAD_LEFT, -1.f);
			l.LierBouton(NK_JEU_SAUTER, NkGamepadButton::NK_GP_SOUTH);
			l.LierBouton(NK_JEU_ACTION, NkGamepadButton::NK_GP_WEST);
			// Doigt : joystick virtuel a gauche, Sauter en bas a droite.
			l.LierStick(NK_JEU_AVANCER, NK_JEU_MONTER, unkeny::NkZoneEcran{0.f, 0.35f, 0.45f, 0.65f}, 0.08f, 0.15f);
			l.LierZone(NK_JEU_SAUTER, unkeny::NkZoneEcran{0.7f, 0.55f, 0.3f, 0.45f});
		}

		bool NkEditeurEntreesEvenement(NkEditeurEntrees &e, NkEtatJeu etat, const NkEvent &ev,
									   const nkgui::NkRect &viseur, bool editeurOccupe) {
			// ⚠️ UNE BOITE OU UN MENU OUVERT EST A L'EDITEUR : sans cette ligne, la
			//    boite « scene non enregistree » ouverte en jeu ne recevait ni
			//    Entree ni Echap, que le jeu gardait pour lui.
			if (etat != NkEtatJeu::NK_JEU || editeurOccupe) {
				if (e.jeuALaMain) {
					RendreLaMain(e);
				}
				return false;
			}

			// ── La souris decide QUI a la main ; elle n'est jamais prise ──
			if (const auto *m = ev.As<NkMouseButtonPressEvent>()) {
				const bool dedans = DansRect(viseur, static_cast<float32>(m->GetX()), static_cast<float32>(m->GetY()));
				if (dedans && !e.jeuALaMain) {
					PrendreLaMain(e);
				} else if (!dedans && e.jeuALaMain) {
					RendreLaMain(e);
				}
				return false;
			}
			if (!e.jeuALaMain) {
				return false;
			}

			if (const auto *k = ev.As<NkKeyPressEvent>()) {
				if (k->GetKey() == NkKey::NK_ESCAPE) {
					RendreLaMain(e);
					return true;
				}
				e.jeu.Lire(ev);
				// PRISE, liee ou non : Suppr n'efface pas l'entite, W ne change
				// pas d'outil, Espace ne relance pas Jouer pendant qu'on joue.
				return true;
			}
			if (ev.As<NkKeyRepeatEvent>() != nullptr) {
				e.jeu.Lire(ev);
				return true;
			}
			if (ev.As<NkKeyReleaseEvent>() != nullptr) {
				e.jeu.Lire(ev);
				return false; // l'editeur aussi : aucune touche ne reste collee dans NKGui
			}
			if (ev.As<NkTextInputEvent>() != nullptr) {
				return true;
			}
			const NkEventType::Value t = ev.GetType();
			if (t == NkEventType::NK_TOUCH_BEGIN || t == NkEventType::NK_TOUCH_MOVE || t == NkEventType::NK_TOUCH_END ||
				t == NkEventType::NK_TOUCH_CANCEL) {
				e.jeu.Lire(ev);
				return true;
			}
			return false;
		}

		void NkEditeurEntreesTrame(NkEditeurEntrees &e, NkEtatJeu etat, const NkGamepadSystem *manettes,
								   const nkgui::NkRect &viseur, bool editeurOccupe) {
			if (editeurOccupe && e.jeuALaMain) {
				RendreLaMain(e);
			}
			if (etat != e.etatVu) {
				// Jouer (depuis l'edition ou la pause) donne la main au jeu ;
				// tout autre changement la lui reprend.
				if (etat == NkEtatJeu::NK_JEU && !editeurOccupe) {
					PrendreLaMain(e);
				} else if (e.jeuALaMain) {
					RendreLaMain(e);
				}
				e.etatVu = etat;
			}
			e.jeu.PoserSurface(viseur.x, viseur.y, viseur.w, viseur.h);
			// Sans la main, le jeu tourne peut-etre, mais sans personne aux
			// commandes : ses actions restent a zero (ToutRelacher les y a mises).
			e.jeu.Trame(e.jeuALaMain ? manettes : nullptr);
		}

		void NkEditeurDessinerEntrees(nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *police, const NkEditeurEntrees &e,
									  const nkgui::NkRect &viseur) {
			if (!e.jeuALaMain || police == nullptr) {
				return;
			}
			const unkeny::NkActions &a = e.jeu.Actions(0);
			const unkeny::NkLiaisons &l = e.jeu.Liaisons();
			NkString ligne = "LE JEU A LA MAIN  ·  Échap la rend à l'éditeur";
			for (int32 k = NK_JEU_AVANCER; k <= NK_JEU_ACTION; ++k) {
				if (a.Valeur(k) != 0.f) {
					ligne += NkString::Format("   %s %.2f", l.NomAction(k), static_cast<double>(a.Valeur(k)));
				}
			}
			const float32 h = renderer::NkTexteHauteurLigne(police, 14.f) + 8.f;
			const float32 w = renderer::NkTexteLargeur(police, ligne.CStr()) + 16.f;
			const nkgui::NkRect bandeau{viseur.x + 8.f, viseur.y + 8.f, w, h};
			dl.AddRectFilled(bandeau, nkgui::NkColor{20, 60, 120, 220}, 3.f);
			dl.AddRect(bandeau, nkgui::NkColor{88, 166, 255, 255}, 1.f, 3.f);
			renderer::NkTexte(dl, police, bandeau.x + 8.f, bandeau.y + 4.f, ligne.CStr(),
							  nkgui::NkColor{235, 242, 250, 255});
		}

		// =====================================================================
		// Le banc
		// =====================================================================

		namespace {

			int32 gE = 0;
			int32 gR = 0;

			void Temoin(bool ok, const char *quoi, float32 v) {
				std::printf("  [%s] %-66s %10.4f\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(v));
				(ok ? gR : gE)++;
			}

			class NkManetteDeBanc final : public NkIGamepad {
				public:
					NkGamepadSnapshot etat{};
					bool Init() override {
						return true;
					}
					void Shutdown() override {
					}
					void Poll() override {
					}
					uint32 GetConnectedCount() const override {
						return etat.connected ? 1u : 0u;
					}
					const NkGamepadSnapshot &GetSnapshot(uint32 i) const override {
						static const NkGamepadSnapshot vide{};
						return i == 0 ? etat : vide;
					}
					void Rumble(uint32, float32, float32, float32, float32, uint32) override {
					}
					const char *GetName() const noexcept override {
						return "ManetteDeBancEditeur";
					}
			};

		} // namespace

		int32 NkEditeurLancerBancEntrees() {
			gE = 0;
			gR = 0;
			std::printf("\nUnkenyEditor — banc de « Jouer » (entrees du jeu)\n\n");
			memory::NkAllocator &alloc = memory::NkGetDefaultAllocator();
			NkEditeurModele *pm = alloc.New<NkEditeurModele>(); // gros : sur le tas
			NkEditeurEntrees *pe = alloc.New<NkEditeurEntrees>();
			NkEditeurModele &m = *pm;
			NkEditeurEntrees &e = *pe;
			NkCreerRessourcesSim(m.ressources, &m.textures, nullptr);
			NkEditeurNouvelleScene(m);
			NkEditeurEntreesParDefaut(e);
			const nkgui::NkRect viseur{300.f, 100.f, 600.f, 400.f};

			NkGamepadSystem pads;
			NkManetteDeBanc *banc = alloc.New<NkManetteDeBanc>();
			pads.Init(memory::NkUniquePtr<NkIGamepad>(banc, memory::NkDefaultDelete<NkIGamepad>(&alloc)));
			banc->etat.connected = true;

			const NkKeyPressEvent espace(NkKey::NK_SPACE);
			const NkKeyReleaseEvent espaceLache(NkKey::NK_SPACE);
			const NkKeyPressEvent echap(NkKey::NK_ESCAPE);
			const NkKeyPressEvent suppr(NkKey::NK_DELETE);

			// ── (e25) ──
			NkEditeurEntreesTrame(e, m.etat, &pads, viseur);
			const bool rienEnEdition = !NkEditeurEntreesEvenement(e, m.etat, espace, viseur);
			NkEditeurEntreesEvenement(e, m.etat, espaceLache, viseur);
			NkEditeurJouer(m);
			NkEditeurEntreesTrame(e, m.etat, &pads, viseur);
			const bool main = e.jeuALaMain;
			const bool pris = NkEditeurEntreesEvenement(e, m.etat, espace, viseur);
			NkEditeurEntreesTrame(e, m.etat, &pads, viseur);
			const bool saute = e.jeu.Actions(0).VientDEtrePressee(NK_JEU_SAUTER);
			Temoin(rienEnEdition && main && pris && saute, "(e25) Jouer donne la main : Espace -> Sauter, pris a NKGui",
				   e.jeu.Actions(0).Valeur(NK_JEU_SAUTER));

			// ── (e26) ──
			const bool relachePartage = !NkEditeurEntreesEvenement(e, m.etat, espaceLache, viseur);
			NkEditeurEntreesTrame(e, m.etat, &pads, viseur);
			const bool relache = e.jeu.Actions(0).VientDEtreRelachee(NK_JEU_SAUTER);
			const NkTextInputEvent lettre('z');
			const bool textePris = NkEditeurEntreesEvenement(e, m.etat, lettre, viseur);
			Temoin(relachePartage && relache && textePris, "(e26) relachement partage avec l'editeur ; texte pris", 0.f);

			// ── (e27) ──
			const NkKeyPressEvent droite(NkKey::NK_D);
			NkEditeurEntreesEvenement(e, m.etat, droite, viseur);
			NkEditeurEntreesTrame(e, m.etat, &pads, viseur);
			const bool avance = e.jeu.Actions(0).Valeur(NK_JEU_AVANCER) > 0.5f;
			const bool echapPris = NkEditeurEntreesEvenement(e, m.etat, echap, viseur);
			NkEditeurEntreesTrame(e, m.etat, &pads, viseur);
			const bool rendue = !e.jeuALaMain && m.etat == NkEtatJeu::NK_JEU && e.jeu.Actions(0).Valeur(NK_JEU_AVANCER) == 0.f;
			const bool supprALEditeur = !NkEditeurEntreesEvenement(e, m.etat, suppr, viseur);
			Temoin(avance && echapPris && rendue && supprALEditeur,
				   "(e27) Echap rend la main, le jeu continue, D relache ; Suppr a l'editeur", 0.f);

			// ── (e28) ──
			const NkMouseButtonPressEvent clicDedans(NkMouseButton::NK_MB_LEFT, 500, 300);
			const NkMouseButtonPressEvent clicDehors(NkMouseButton::NK_MB_LEFT, 50, 50);
			const bool clicNonPris = !NkEditeurEntreesEvenement(e, m.etat, clicDedans, viseur);
			const bool reprise = e.jeuALaMain;
			NkEditeurEntreesEvenement(e, m.etat, clicDehors, viseur);
			const bool perdue = !e.jeuALaMain;
			Temoin(clicNonPris && reprise && perdue, "(e28) clic dans le viseur : main au jeu ; ailleurs : a l'editeur",
				   0.f);

			// ── (e29) ──
			banc->etat.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_SOUTH)] = true;
			pads.PollGamepads();
			NkEditeurEntreesTrame(e, m.etat, &pads, viseur);
			const bool ignoree = !e.jeu.Actions(0).Enfoncee(NK_JEU_SAUTER);
			NkEditeurEntreesEvenement(e, m.etat, clicDedans, viseur);
			NkEditeurEntreesTrame(e, m.etat, &pads, viseur);
			const bool lue = e.jeu.Actions(0).Enfoncee(NK_JEU_SAUTER);
			banc->etat.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_SOUTH)] = false;
			pads.PollGamepads();
			Temoin(ignoree && lue, "(e29) manette : ignoree sans la main, Sud -> Sauter avec", 0.f);

			// ── (e30) ──
			NkEditeurEntreesEvenement(e, m.etat, droite, viseur);
			NkEditeurEntreesTrame(e, m.etat, &pads, viseur);
			const bool avant = e.jeu.Actions(0).Enfoncee(NK_JEU_AVANCER);
			NkEditeurArreter(m);
			NkEditeurEntreesTrame(e, m.etat, &pads, viseur);
			const bool arrete = !e.jeuALaMain && !e.jeu.Actions(0).Enfoncee(NK_JEU_AVANCER) &&
								!e.jeu.ToucheTenue(NkKey::NK_D) && m.etat == NkEtatJeu::NK_EDITION;
			const bool plusRien = !NkEditeurEntreesEvenement(e, m.etat, espace, viseur);
			Temoin(avant && arrete && plusRien, "(e30) Arreter : main a l'editeur, tout relache", 0.f);

			// ── (e31) ──
			NkEditeurJouer(m);
			NkEditeurEntreesTrame(e, m.etat, &pads, viseur);
			const bool reprise31 = e.jeuALaMain;
			const NkKeyPressEvent entree(NkKey::NK_ENTER);
			const bool entreeALaBoite = !NkEditeurEntreesEvenement(e, m.etat, entree, viseur, true);
			NkEditeurEntreesTrame(e, m.etat, &pads, viseur, true);
			const bool rendue31 = !e.jeuALaMain;
			NkEditeurArreter(m);
			NkEditeurEntreesTrame(e, m.etat, &pads, viseur);
			Temoin(reprise31 && entreeALaBoite && rendue31, "(e31) boite ou menu ouvert : le clavier revient a l'editeur",
				   0.f);

			alloc.Delete(pe);
			alloc.Delete(pm);
			std::printf("\n%s : %d reussis, %d echec%s\n", gE == 0 ? "BANC JOUER REUSSI" : "BANC JOUER EN ECHEC", gR, gE,
						gE > 1 ? "s" : "");
			return gE == 0 ? 0 : 1;
		}

	} // namespace editeur
} // namespace nkentseu
