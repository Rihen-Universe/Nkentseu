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
//   (e32) le panneau Entrees decrit la table : « Avancer  <-  <code de D>  [J1] »
//   (e33) « Changer » en EDITION : la touche suivante (K) est PRISE et devient
//         l'entree de Sauter ; une zone d'ecran refuse le changement
//   (e34) le fichier de la scene (.nkentrees) : enregistre, les defauts
//         remis, relu -> K revient ; sans fichier -> les defauts, et c'est dit
//   (e35) une AUTRE scene relit SES entrees ; la meme scene ne relit rien (un
//         changement non enregistre n'est pas ecrase)
//   (e36) 30/09 : EN JOUER, Espace fait sauter le heros du jalon « Gelee »
//         (Unkeny/Jeu/NkUnkenyNiveauGelee.h) : l'evenement pose par le banc
//         passe par NkEditeurEntreesEvenement, le controleur mou accorde UN
//         saut et le heros monte d'au moins 30 cm ; (e36n) sans Espace, aucun
//         saut, il ne monte pas de 10 cm. Un second Brancher ne pose pas un
//         second systeme.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurEntrees.h"

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurInterface.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkPath.h"
#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include "Unkeny/Jeu/NkUnkenyControleurs.h"
#include "Unkeny/Jeu/NkUnkenyNiveauGelee.h"

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

			/// La capture du panneau vient-elle d'aboutir (ou d'etre abandonnee) ?
			void SuivreCapture(NkEditeurEntrees &e) {
				if (e.capturee < 0 || e.jeu.CaptureEnCours()) {
					return;
				}
				e.message = NkString("Liaison changee : ") + NkEditeurEntreesDecrire(e, e.capturee);
				e.capturee = -1;
				e.modifiees = true;
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
			// Les MEMES que le joueur autonome : une seule table, dans Unkeny.
			unkeny::NkLiaisonsStandard(e.jeu.Liaisons());
		}

		uint32 NkEditeurEntreesBrancher(NkEditeurEntrees &e, unkeny::NkScene &scene) {
			if (e.sceneBranchee == &scene && e.systemeControleurs != 0u) {
				return e.systemeControleurs;
			}
			e.systemeControleurs = unkeny::NkAjouterControleurs2D(scene, &e.jeu.Actions(0));
			e.sceneBranchee = e.systemeControleurs != 0u ? &scene : nullptr;
			return e.systemeControleurs;
		}

		bool NkEditeurEntreesEvenement(NkEditeurEntrees &e, NkEtatJeu etat, const NkEvent &ev,
									   const nkgui::NkRect &viseur, bool editeurOccupe) {
			// ── Le panneau Entrees attend une touche : le CLAVIER est a la capture,
			//    en edition comme en jeu (la souris reste au panneau : Annuler). ──
			if (e.capturee >= 0) {
				const bool touche = ev.As<NkKeyPressEvent>() != nullptr || ev.As<NkKeyReleaseEvent>() != nullptr;
				if (touche || ev.As<NkKeyRepeatEvent>() != nullptr || ev.As<NkTextInputEvent>() != nullptr) {
					if (touche) {
						e.jeu.Lire(ev);
					}
					SuivreCapture(e);
					return true;
				}
			}
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
			// La capture du panneau ecoute aussi les manettes, meme en edition.
			e.jeu.Trame((e.jeuALaMain || e.capturee >= 0) ? manettes : nullptr);
			SuivreCapture(e);
		}

		// =====================================================================
		// Le panneau Entrees : ses gestes
		// =====================================================================

		bool NkEditeurEntreesChanger(NkEditeurEntrees &e, int32 indice) {
			if (!e.jeu.CapturerProchaineEntree(indice)) {
				e.message = "Cette liaison ne se capture pas (zone d'ecran ou joystick virtuel).";
				return false;
			}
			e.capturee = indice;
			e.message = "Appuyez sur une touche ou un bouton de manette...";
			return true;
		}

		void NkEditeurEntreesAnnulerChangement(NkEditeurEntrees &e) {
			e.jeu.AnnulerCapture();
			e.capturee = -1;
			e.message = "Changement annule.";
		}

		NkString NkEditeurEntreesDecrire(const NkEditeurEntrees &e, int32 indice) {
			const unkeny::NkLiaisons &l = e.jeu.Liaisons();
			if (indice < 0 || indice >= l.Nombre()) {
				return NkString();
			}
			const unkeny::NkLiaison &x = l[indice];
			NkString entree;
			switch (x.nature) {
				case unkeny::NkNatureLiaison::NK_ENTREE:
					entree = x.code.ToString();
					break;
				case unkeny::NkNatureLiaison::NK_ZONE_BOUTON:
					entree = "zone d'ecran";
					break;
				case unkeny::NkNatureLiaison::NK_ZONE_STICK_X:
					entree = "joystick virtuel (X)";
					break;
				case unkeny::NkNatureLiaison::NK_ZONE_STICK_Y:
					entree = "joystick virtuel (Y)";
					break;
			}
			if (x.echelle < 0.f) {
				entree += " (-)";
			}
			const char *nom = l.NomAction(x.action);
			NkString t = NkString(nom[0] != '\0' ? nom : "?") + "  <-  " + entree;
			if (x.joueur == unkeny::NK_UNKENY_TOUS_LES_JOUEURS) {
				t += "  [tous]";
			} else {
				t += NkString::Format("  [J%d]", x.joueur + 1);
			}
			return t;
		}

		NkString NkEditeurEntreesFichier(const char *cheminScene) {
			NkString s(cheminScene != nullptr ? cheminScene : "scene.nkscene");
			if (s.EndsWith(".nkscene")) {
				s = s.SubStr(0, s.Length() - 8);
			}
			s += ".nkentrees";
			return s;
		}

		NkString NkEditeurEntreesCheminScene(const NkEditeurModele &m) {
			if (!m.chemin.Empty()) {
				return m.chemin;
			}
			// Le MEME choix que NkEditeurChemin (NkEditeurActions.cpp), sans
			// ecrire le modele ni creer le dossier : c'est l'enregistrement qui
			// le creera.
			const NkPath base = NkDirectory::GetAppDataDirectory();
			if (!base.ToString().Empty()) {
				return (base / "UnkenyEditor" / "scene.nkscene").ToString();
			}
			return NkString("scene.nkscene");
		}

		bool NkEditeurEntreesEnregistrer(NkEditeurEntrees &e, const char *cheminScene) {
			const NkString chemin = NkEditeurEntreesFichier(cheminScene);
			const NkPath dossier = NkPath(chemin.CStr()).GetParent();
			if (!dossier.ToString().Empty()) {
				NkDirectory::CreateRecursive(dossier);
			}
			const NkString texte = e.jeu.Liaisons().Ecrire();
			const bool ok = NkFile::WriteAllText(chemin.CStr(), texte.CStr());
			e.message = ok ? NkString("Entrees enregistrees : ") + chemin : NkString("Enregistrement impossible : ") + chemin;
			if (ok) {
				e.modifiees = false;
			}
			return ok;
		}

		bool NkEditeurEntreesCharger(NkEditeurEntrees &e, const char *cheminScene) {
			const NkString chemin = NkEditeurEntreesFichier(cheminScene);
			// Les NOMS d'abord (le fichier ne decide que des entrees), puis le
			// fichier par-dessus les liaisons par defaut.
			NkEditeurEntreesParDefaut(e);
			e.modifiees = false;
			if (!NkFile::Exists(chemin.CStr())) {
				e.message = "Entrees par defaut (aucun fichier .nkentrees pour cette scene).";
				return false;
			}
			const unkeny::NkRapportLiaisons r = e.jeu.Liaisons().Lire(NkFile::ReadAllText(chemin.CStr()));
			if (!r.Ok()) {
				// ⚠️ UN REFUS SE DIT : la premiere ligne fautive, avec son numero.
				e.message = NkString::Format("%u ligne(s) refusee(s) dans %s : ", static_cast<uint32>(r.erreurs.Size()),
											 chemin.CStr()) +
							r.erreurs[0];
				return false;
			}
			e.message = NkString("Entrees lues : ") + chemin;
			return true;
		}

		void NkEditeurEntreesSuivreScene(NkEditeurEntrees &e, const char *cheminScene) {
			if (cheminScene == nullptr || e.cheminVu == cheminScene) {
				return;
			}
			e.cheminVu = cheminScene;
			NkEditeurEntreesCharger(e, cheminScene);
		}

		// =====================================================================
		// Le panneau Entrees : le dessin
		// =====================================================================

		namespace {

			/// Un bouton du panneau, dessine et teste ici (le chrome de l'editeur
			/// n'emploie pas les boutons NKGui). `actif` : clic permis.
			bool BoutonPanneau(NkEditeurCadre &c, const nkgui::NkRect &r, const char *texte, bool actif) {
				const nkgui::NkVec2 p = c.ctx.input.mousePos;
				const bool survol = actif && DansRect(r, p.x, p.y);
				c.ctx.dl.AddRectFilled(r, survol ? c.pal.boutonSurvol : c.pal.bouton, 2.f);
				renderer::NkTexteDansBoite(c.ctx.dl, c.petite, r, texte, actif ? c.pal.texte : c.pal.attenue);
				return survol && c.ctx.input.mouseClicked[0];
			}

		} // namespace

		void NkEditeurDessinerPanneauEntrees(NkEditeurCadre &c, NkEditeurEntrees &e) {
			if (!c.ui.panneauEntrees) {
				e.panneauRect = nkgui::NkRect{0.f, 0.f, 0.f, 0.f};
				return;
			}
			auto &dl = c.ctx.dl;
			const unkeny::NkLiaisons &l = e.jeu.Liaisons();
			const float32 ligneH = 22.f;
			const float32 enteteH = 34.f;
			const float32 piedH = 64.f;
			const float32 w = c.ui.ecran.w - 80.f < 620.f ? c.ui.ecran.w - 80.f : 620.f;
			const float32 hMax = c.ui.ecran.h - 120.f;
			int32 visibles = static_cast<int32>((hMax - enteteH - piedH) / ligneH);
			if (visibles > l.Nombre()) {
				visibles = l.Nombre();
			}
			if (visibles < 1) {
				visibles = 1;
			}
			const float32 h = enteteH + piedH + ligneH * static_cast<float32>(visibles);
			const nkgui::NkRect r{c.ui.ecran.x + (c.ui.ecran.w - w) * 0.5f, c.ui.ecran.y + (c.ui.ecran.h - h) * 0.5f, w, h};
			e.panneauRect = r;
			// Les clics : pas sous un menu ouvert, pas sous la boite modale.
			const bool actif = c.ui.menu == NkMenuEditeur::NK_AUCUN && c.ui.confirmation == NK_A_AUCUNE;
			const nkgui::NkVec2 souris = c.ctx.input.mousePos;
			if (actif && DansRect(r, souris.x, souris.y) && c.ctx.input.wheel != 0.f) {
				e.panneauDefil -= c.ctx.input.wheel > 0.f ? 1 : -1;
			}
			if (e.panneauDefil > l.Nombre() - visibles) {
				e.panneauDefil = l.Nombre() - visibles;
			}
			if (e.panneauDefil < 0) {
				e.panneauDefil = 0;
			}

			dl.AddRectFilled(r, c.pal.panneau, 4.f);
			dl.AddRect(r, c.pal.accent, 1.f, 4.f);
			const nkgui::NkRect entete{r.x, r.y, r.w, enteteH};
			dl.AddRectFilled(entete, c.pal.entete, 4.f);
			renderer::NkTexte(dl, c.police, r.x + 12.f, r.y + 8.f, "Entrées du jeu", c.pal.texte);
			const char *etat = e.modifiees ? "modifiées, non enregistrées" : "";
			renderer::NkTexteADroite(dl, c.petite, r.x + r.w - 44.f, r.y + 10.f, etat, c.pal.selection);
			if (BoutonPanneau(c, nkgui::NkRect{r.x + r.w - 32.f, r.y + 6.f, 24.f, 22.f}, "x", actif)) {
				c.ui.panneauEntrees = false;
			}

			// ── Les liaisons, une par ligne ──
			for (int32 k = 0; k < visibles; ++k) {
				const int32 i = e.panneauDefil + k;
				if (i >= l.Nombre()) {
					break;
				}
				const float32 y = r.y + enteteH + ligneH * static_cast<float32>(k);
				const nkgui::NkRect ligne{r.x + 6.f, y, r.w - 12.f, ligneH - 2.f};
				if (i == e.capturee) {
					dl.AddRectFilled(ligne, c.pal.accent, 2.f);
				} else if ((k & 1) == 1) {
					dl.AddRectFilled(ligne, c.pal.fond, 2.f);
				}
				const NkString texte = NkEditeurEntreesDecrire(e, i);
				renderer::NkTexte(dl, c.petite, ligne.x + 8.f, y + 3.f, texte.CStr(),
								  i == e.capturee ? c.pal.surAccent : c.pal.texte, ligne.w - 110.f);
				const bool entree = l[i].nature == unkeny::NkNatureLiaison::NK_ENTREE;
				const nkgui::NkRect bouton{ligne.x + ligne.w - 96.f, y + 1.f, 92.f, ligneH - 4.f};
				if (i == e.capturee) {
					if (BoutonPanneau(c, bouton, "Annuler", actif)) {
						NkEditeurEntreesAnnulerChangement(e);
					}
				} else if (BoutonPanneau(c, bouton, entree ? "Changer" : "-", actif && entree && e.capturee < 0)) {
					NkEditeurEntreesChanger(e, i);
				}
			}

			// ── Le pied : l'annonce, puis Enregistrer / Recharger / Par defaut ──
			const float32 yPied = r.y + r.h - piedH;
			renderer::NkTexte(dl, c.petite, r.x + 12.f, yPied + 6.f, e.message.CStr(), c.pal.attenue, r.w - 24.f);
			const float32 by = yPied + 30.f;
			const NkString cheminScene = NkEditeurEntreesCheminScene(c.m);
			const char *scene = cheminScene.CStr();
			if (BoutonPanneau(c, nkgui::NkRect{r.x + 12.f, by, 120.f, 24.f}, "Enregistrer", actif && e.capturee < 0)) {
				NkEditeurEntreesEnregistrer(e, scene);
			}
			if (BoutonPanneau(c, nkgui::NkRect{r.x + 140.f, by, 120.f, 24.f}, "Recharger", actif && e.capturee < 0)) {
				NkEditeurEntreesCharger(e, scene);
			}
			if (BoutonPanneau(c, nkgui::NkRect{r.x + 268.f, by, 120.f, 24.f}, "Par défaut", actif && e.capturee < 0)) {
				NkEditeurEntreesParDefaut(e);
				e.modifiees = true;
				e.message = "Liaisons par defaut (non enregistrees).";
			}
		}

		void NkEditeurDessinerEntrees(nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *police, const NkEditeurEntrees &e,
									  const nkgui::NkRect &viseur) {
			if (!e.jeuALaMain || police == nullptr) {
				return;
			}
			const unkeny::NkActions &a = e.jeu.Actions(0);
			const unkeny::NkLiaisons &l = e.jeu.Liaisons();
			NkString ligne = "LE JEU A LA MAIN  ·  Échap la rend à l'éditeur";
			for (int32 k = 0; k < unkeny::NK_ACTIONS_STANDARD; ++k) {
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

			// ── (e32) ──
			NkEditeurEntreesParDefaut(e);
			const NkString d0 = NkEditeurEntreesDecrire(e, 0);
			const NkString attendu32 = NkString("Avancer  <-  ") + NkInputCode::Key(NkKey::NK_D).ToString() + "  [J1]";
			if (d0 != attendu32) {
				std::printf("        lu : %s\n", d0.CStr());
			}
			Temoin(d0 == attendu32, "(e32) le panneau decrit la table : Avancer <- Key:D [J1]", 0.f);

			// ── (e33) ──
			const int32 iEspace = e.jeu.Liaisons().Trouver(NK_JEU_SAUTER, NkInputCode::Key(NkKey::NK_SPACE));
			const bool arme33 = NkEditeurEntreesChanger(e, iEspace);
			const NkKeyPressEvent k33(NkKey::NK_K);
			const bool pris33 = NkEditeurEntreesEvenement(e, m.etat, k33, viseur);
			const bool change33 = e.jeu.Liaisons()[iEspace].code == NkInputCode::Key(NkKey::NK_K) && e.capturee < 0 &&
								  e.modifiees;
			int32 iZone = -1;
			for (int32 i = 0; i < e.jeu.Liaisons().Nombre(); ++i) {
				if (e.jeu.Liaisons()[i].nature == unkeny::NkNatureLiaison::NK_ZONE_BOUTON) {
					iZone = i;
				}
			}
			const bool zoneRefusee = !NkEditeurEntreesChanger(e, iZone) && e.capturee < 0;
			Temoin(m.etat == NkEtatJeu::NK_EDITION && arme33 && pris33 && change33 && zoneRefusee,
				   "(e33) Changer en EDITION : K devient Sauter ; une zone d'ecran refuse", 0.f);

			// ── (e34) ──
			const char *scene34 = "banc_entrees_editeur.nkscene";
			const NkString fichier34 = NkEditeurEntreesFichier(scene34);
			const bool ecrit34 = NkEditeurEntreesEnregistrer(e, scene34) && !e.modifiees;
			NkEditeurEntreesParDefaut(e);
			const bool defaut34 = e.jeu.Liaisons()[iEspace].code == NkInputCode::Key(NkKey::NK_SPACE);
			const bool relu34 = NkEditeurEntreesCharger(e, scene34) &&
								e.jeu.Liaisons()[iEspace].code == NkInputCode::Key(NkKey::NK_K);
			const bool absent34 = !NkEditeurEntreesCharger(e, "banc_sans_entrees.nkscene") &&
								  e.jeu.Liaisons()[iEspace].code == NkInputCode::Key(NkKey::NK_SPACE) &&
								  e.message.Find("par defaut") != NkString::npos;
			Temoin(fichier34 == "banc_entrees_editeur.nkentrees" && ecrit34 && defaut34 && relu34 && absent34,
				   "(e34) .nkentrees : enregistre, relu (K revient) ; absent -> defauts, et dit", 0.f);

			// ── (e35) ──
			e.cheminVu = NkString();
			NkEditeurEntreesSuivreScene(e, scene34);
			const bool lue35 = e.jeu.Liaisons()[iEspace].code == NkInputCode::Key(NkKey::NK_K);
			e.jeu.Liaisons().Relier(iEspace, NkInputCode::Key(NkKey::NK_J));
			NkEditeurEntreesSuivreScene(e, scene34);
			const bool garde35 = e.jeu.Liaisons()[iEspace].code == NkInputCode::Key(NkKey::NK_J);
			NkEditeurEntreesSuivreScene(e, "banc_sans_entrees.nkscene");
			const bool autre35 = e.jeu.Liaisons()[iEspace].code == NkInputCode::Key(NkKey::NK_SPACE);
			Temoin(lue35 && garde35 && autre35, "(e35) une autre scene relit ses entrees ; la meme ne relit rien", 0.f);
			NkFile::Delete(fichier34.CStr());

			// ── (e36) ──
			// Le niveau du jalon, dans la scene de l'editeur (vide : la scene
			// neuve a un mur a x = 11,5 et des caisses, qui ne sont pas du jalon).
			{
				NkEditeurArreter(m);
				NkSceneConfig cfg36;
				cfg36.physique = true;
				cfg36.particules = true;
				cfg36.gravite = NkVec2f(0.f, -9.81f);
				const float32 dt = 1.f / 60.f;
				float32 montee[2] = {0.f, 0.f};
				uint32 sauts[2] = {0u, 0u};
				bool pris36 = false;
				bool unSeul = false;
				for (int32 essai = 0; essai < 2; ++essai) {
					const bool avecEspace = essai == 0;
					m.scene.Init(cfg36);
					m.photo.valide = false;
					unkeny::NkNiveauGelee n;
					NkConstruireNiveauGelee(m.scene, n, true);
					NkEditeurEntreesParDefaut(e);
					const uint32 id = NkEditeurEntreesBrancher(e, m.scene);
					unSeul = unSeul || (id != 0u && NkEditeurEntreesBrancher(e, m.scene) == id);
					NkEditeurJouer(m);
					// Qu'elle se pose (la main passe au jeu a la premiere trame).
					for (int32 i = 0; i < 40; ++i) {
						NkEditeurEntreesTrame(e, m.etat, &pads, viseur);
						NkEditeurAvancer(m, dt);
					}
					const float32 y0 = NkPositionHerosGelee(m.scene, n).y;
					if (avecEspace) {
						pris36 = e.jeuALaMain && NkEditeurEntreesEvenement(e, m.etat, espace, viseur);
					}
					float32 yMax = y0;
					for (int32 i = 0; i < 24; ++i) {
						NkEditeurEntreesTrame(e, m.etat, &pads, viseur);
						NkEditeurAvancer(m, dt);
						yMax = math::NkMax(yMax, NkPositionHerosGelee(m.scene, n).y);
					}
					NkEditeurEntreesEvenement(e, m.etat, espaceLache, viseur);
					const unkeny::NkEtatControle2D *c = NkEtatHerosGelee(m.scene, n);
					sauts[essai] = c != nullptr ? c->sauts : 999u;
					montee[essai] = yMax - y0;
					NkEditeurArreter(m);
					NkEditeurEntreesTrame(e, m.etat, &pads, viseur);
				}
				std::printf("        Espace : %u saut(s), montee %.2f m ; sans : %u saut(s), %.2f m\n", sauts[0],
							static_cast<double>(montee[0]), sauts[1], static_cast<double>(montee[1]));
				Temoin(pris36 && unSeul && sauts[0] == 1u && montee[0] >= 0.3f,
					   "(e36) en Jouer, Espace fait sauter le heros de Gelee (montee m)", montee[0]);
				Temoin(sauts[1] == 0u && montee[1] < 0.1f, "(e36n) sans Espace : aucun saut, il ne monte pas (m)", montee[1]);
				NkEditeurNouvelleScene(m);
			}

			alloc.Delete(pe);
			alloc.Delete(pm);
			std::printf("\n%s : %d reussis, %d echec%s\n", gE == 0 ? "BANC JOUER REUSSI" : "BANC JOUER EN ECHEC", gR, gE,
						gE > 1 ? "s" : "");
			return gE == 0 ? 0 : 1;
		}

	} // namespace editeur
} // namespace nkentseu
