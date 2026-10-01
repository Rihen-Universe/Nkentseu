// -----------------------------------------------------------------------------
// @File    NogeeInterface.cpp
// @Brief   La couche d'interface de Nogee : la trame, le chrome, les menus, les
//          actions (voir NogeeInterface.h).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "Nogee/Editeur/NogeeInterface.h"
#include "Nogee/Editeur/NogeeLogo.h"
#include "Nogee/Editeur/NogeeVue3D.h"

#include "Noge/Core/NkApplication.h"
#include "Noge/Layers/NkEngineLayer.h"
#include "NKEditorKit/NkThemeToGui.h"
#include "NKLogger/NkLog.h"
#include "NKRenderer/NkRenderer.h" // SetUIOverlayCallback (passe Overlay2D)
#include "NKWindow/Core/NkWindow.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace nkentseu {
	namespace nogee {

		using namespace editorkit;
		using nkgui::NkColor;
		using nkgui::NkGuiKey;
		using nkgui::NkRect;
		using nkgui::NkVec2;
		using O = editorkit::NkFamilleOngletPlacer;

		namespace {
			void RemplirMenu(void *user, int32 menu, NkVector<NkFamilleEntreeMenu> &sortie) {
				static_cast<NogeeInterface *>(user)->Remplir(menu, sortie);
			}

			const float32 kPasGrille[] = {0.05f, 0.1f, 0.25f, 0.5f, 1.f, 2.f};
			const float32 kPasAngle[] = {1.f, 5.f, 10.f, 15.f, 30.f, 45.f, 90.f};
			const float32 kPasEchelle[] = {0.05f, 0.1f, 0.25f, 0.5f, 1.f};

			const char *Fichier(const NkString &chemin) {
				const char *s = chemin.CStr();
				const char *r = s;
				for (const char *p = s; *p != '\0'; ++p) {
					if (*p == '/' || *p == '\\') {
						r = p + 1;
					}
				}
				return r;
			}
		} // namespace

		// =====================================================================
		// LA COUCHE
		// =====================================================================
		NogeeInterface::NogeeInterface(const NogeeHote &hote) noexcept
			: NkOverlay("NogeeInterface"), mHote(hote), mTheme(NkTheme::Dark()) {
			mPal = NkFamillePaletteDe(mTheme);
		}

		NogeeInterface::~NogeeInterface() = default;

		void NogeeInterface::PoserTheme(bool clair) {
			mClair = clair;
			mTheme = clair ? NkTheme::Light() : NkTheme::Dark();
			mPal = NkFamillePaletteDe(mTheme);
			if (mPret) {
				NkThemeVersGui(mCtx, mTheme);
				// La geometrie n'est pas une couleur : celle de la famille (coins de
				// 2 px, rangees denses), comme UnkenyEditor.
				mCtx.theme.rounding = 2.f;
				mCtx.theme.roundingSmall = 2.f;
				mCtx.theme.framePadX = 6.f;
				mCtx.theme.framePadY = 3.f;
			}
		}

		void NogeeInterface::OnAttach() {
			NkIDevice *dev = mHote.device;
			const uint32 W = dev != nullptr ? dev->GetSwapchainWidth() : 1280u;
			const uint32 H = dev != nullptr ? dev->GetSwapchainHeight() : 760u;
			if (!mCtx.Init(static_cast<int32>(W), static_cast<int32>(H))) {
				logger.Errorf("[Nogee] NkGuiContext::Init a echoue : pas d'interface\n");
				return;
			}
			nkgui::SetCurrentContext(&mCtx);
			// LES POLICES DE LA FAMILLE (NkCanvasGuiApp::LoadFonts) : DroidSans 13 px,
			// la petite a 0,78 ; une texId DISTINCTE par police.
			mPolice.texId = 0x4E4B4654u;
			mPetite.texId = 0x4E4B4655u;
			mMono.texId = 0x4E4B5445u;
			const float32 corps = 13.f;
			if (!mPolice.LoadEmbedded(NkEmbeddedFontId::DroidSans, corps)) {
				(void)mPolice.LoadEmbedded(NkEmbeddedFontId::ProggyClean, corps);
			}
			(void)mPetite.LoadEmbedded(NkEmbeddedFontId::DroidSans, corps * 0.78f);
			if (!mMono.LoadEmbedded(NkEmbeddedFontId::DejaVuSansMono, corps, false)) {
				(void)mMono.LoadEmbedded(NkEmbeddedFontId::Cousine, corps, false);
			}
			mCtx.font = &mPolice;
			mCtx.codeFont = mMono.Valid() ? &mMono : &mPolice;
			mPret = true;
			PoserTheme(mClair);

			// LE RENDU : le backend sur la passe de la swapchain, les atlas, puis le
			// rappel dans la passe Overlay2D du renderer de l'application (UNE
			// Submit par image, contenu + surcouche fusionnes).
			if (dev != nullptr && mBackend.Init(dev, dev->GetSwapchainRenderPass(), mHote.api)) {
				mBackendPret = true;
				nkgui::NkGuiFont *polices[3] = {&mPolice, &mPetite, &mMono};
				for (nkgui::NkGuiFont *f : polices) {
					if (f->Valid() && f->pixels != nullptr) {
						mBackend.UploadTextureGray8(f->TexId(), f->pixels, f->atlasW, f->atlasH);
					}
				}
				if (renderer::NkRenderer *r = NkApplication::Get().GetRenderer()) {
					r->SetUIOverlayCallback([this](NkICommandBuffer *cmd) {
						if (!mBackendPret || mHote.device == nullptr) {
							return;
						}
						mBackend.Submit(cmd, mFusion, mHote.device->GetSwapchainWidth(), mHote.device->GetSwapchainHeight());
					});
					mRappelPose = true;
				}
			} else {
				logger.Errorf("[Nogee] le backend NKGui -> NKRHI est refuse : l'interface ne sera pas rendue\n");
			}

			// Les panneaux.
			mContenu.racine = mHote.contenu;
			mContenu.nomProjet = NkString("Nogee");
			mTerminal.dossierDepart = NkString(".");
			mTerminal.police = mMono.Valid() ? &mMono : nullptr;
			mJournal.Ajouter("Nogee — l'éditeur de Noge, à l'apparence d'UnkenyEditor (R32)");
			mJournal.Ajouter("Vue 3D : le chemin de jeu de Noge (NkApplication + NkEngineLayer), rendu hors écran");
			logger.Infof("[Nogee] interface attachee %ux%u (backend RHI : %d)\n", W, H, mBackendPret ? 1 : 0);
		}

		void NogeeInterface::LibererGpu() noexcept {
			if (mRappelPose) {
				if (NkApplication::HasInstance()) {
					if (renderer::NkRenderer *r = NkApplication::Get().GetRenderer()) {
						r->SetUIOverlayCallback(renderer::NkUIOverlayCallback{});
					}
				}
				mRappelPose = false;
			}
			if (mBackendPret) {
				if (mHote.device != nullptr) {
					mHote.device->WaitIdle();
				}
				mBackend.Destroy();
				mBackendPret = false;
			}
			mHote.device = nullptr;
		}

		void NogeeInterface::OnDetach() {
			LibererGpu();
			mTerminal.FermerTout();
			if (mPret) {
				mCtx.Shutdown();
				mPret = false;
			}
		}

		void NogeeInterface::OnUpdate(float dt) {
			mDt = dt > 0.f ? dt : 1.f / 60.f;
			mTemps += mDt;
			M().messageAge += mDt;
			mTempsIps += mDt;
			++mTramesIps;
			if (mTempsIps >= 0.5f) {
				mIps = static_cast<float32>(mTramesIps) / mTempsIps;
				mTempsIps = 0.f;
				mTramesIps = 0;
			}
			mTerminal.Pomper();
		}

		bool NogeeInterface::OnEvent(NkEvent *event) {
			if (event != nullptr && mPret) {
				(void)mEntree.Lire(mCtx.input, *event);
			}
			// Rien n'est CONSOMME : le moteur (sa carte d'entree) lit les memes
			// evenements -- en jeu, ce sont les siens.
			return false;
		}

		void NogeeInterface::OnUIRender() {
			if (!mPret || mHote.device == nullptr || mHote.modele == nullptr || !M().Pret()) {
				return;
			}
			const uint32 W = mHote.device->GetSwapchainWidth();
			const uint32 H = mHote.device->GetSwapchainHeight();
			if (W == 0u || H == 0u) {
				return;
			}
			nkgui::SetCurrentContext(&mCtx);
			mCtx.viewW = static_cast<int32>(W);
			mCtx.viewH = static_cast<int32>(H);
			// La vue 3D, publiee aupres du backend (de nouveau si sa cible a ete refaite).
			if (mBackendPret && mHote.vue != nullptr && mHote.vue->Pret() && mHote.vue->Generation() != mGenerationVue) {
				if (mBackend.RegisterTexture(kTexVue, mHote.vue->Texture())) {
					mGenerationVue = mHote.vue->Generation();
				}
			}
			mCtx.BeginFrame(mDt);
			NkFamillePlanifier(mPlan, static_cast<float32>(W), static_cast<float32>(H));
			NkFamilleCtx c{mCtx, mTheme, mPal, &mPolice, &mPetite};
			Trame(c);
			if (mHote.fenetre != nullptr) {
				mHote.fenetre->SetCursor(NkFamilleCurseur(mCtx.wantCursor));
			}
			mCtx.EndFrame();
			mFusion.Reset();
			mFusion.Append(mCtx.dl);
			mFusion.Append(mCtx.dlOverlay);
			mEntree.FinDeTrame(mCtx.input);
			AppliquerFenetre();
		}

		// =====================================================================
		// LA TRAME : l'ordre de dessin d'UnkenyEditor (NkEditeurDessinerTrame)
		// =====================================================================
		void NogeeInterface::Trame(NkFamilleCtx &c) {
			auto &dl = mCtx.dl;
			nkgui::NkGuiInput &in = mCtx.input;
			dl.AddRectFilled(mPlan.ecran, mPal.fond);
			mFen.agrandie = mHote.fenetre != nullptr && mHote.fenetre->IsMaximized();
			Journaliser();

			// ── 0. Les bords de la fenetre, avant tout, avec l'entree reelle ──
			NkFamilleBordsFenetre(c, mPlan, mFen);

			// ── 1. Le corps, gestes neutralises si un menu est ouvert ─────────
			// Le menu est decide AVANT le corps, avec le rectangle de la trame
			// d'avant : un clic hors du menu le ferme et ne traverse pas.
			int32 menuDebut = mMenus.menu;
			const NkFamilleGestes vrais = NkFamilleSauverGestes(in);
			if (menuDebut >= 0 && vrais.clic[1] && !mMenus.Contient(vrais.position)) {
				mMenus.Fermer();
				menuDebut = -1;
			}
			if (menuDebut >= 0) {
				NkFamilleNeutraliserGestes(in, mMenus.Contient(vrais.position));
			}
			Vue(c);
			Placer(c);
			Outliner(c);
			Details(c);
			Tiroir(c);
			(void)NkFamilleCloisons(c, mPlan);
			BarreOutils(c);
			Statut(c);
			OngletsScene(c);

			// ── 2. Les menus, avec l'entree reelle ───────────────────────────
			NkFamilleRendreGestes(in, vrais);
			BarreTitre(c);
			const int32 action = NkFamilleDessinerMenu(c, mPlan.ecran, mMenus, menuDebut, &RemplirMenu, this);
			if (action != NOGEE_A_AUCUNE) {
				Executer(action);
			}

			// ── 3. Les raccourcis, APRES le dessin (un champ garde ses touches) ─
			Raccourcis(c);
			// Un liseret autour de la fenetre sans cadre ; agrandie, elle n'a pas de bord.
			if (!mFen.agrandie) {
				dl.AddRect(mPlan.ecran, mPal.bord, 1.f);
			}
		}

		void NogeeInterface::Journaliser() {
			NogeeModele &m = M();
			for (uint32 i = 0; i < m.aJournaliser.Size(); ++i) {
				const int32 s = static_cast<int32>(mTemps);
				const NkString ligne = NkString::Format("[%02d:%02d]  %s", s / 60, s % 60, m.aJournaliser[i].CStr());
				const uint8 n = i < m.niveaux.Size() ? m.niveaux[i] : 0u;
				mJournal.Ajouter(ligne.CStr(), n == 3 ? NkFamilleNiveau::Erreur
										   : n == 2 ? NkFamilleNiveau::Avertissement
										   : n == 1 ? NkFamilleNiveau::Succes
													: NkFamilleNiveau::Info);
			}
			m.aJournaliser.Clear();
			m.niveaux.Clear();
		}

		// =====================================================================
		// LE CHROME
		// =====================================================================
		void NogeeInterface::BarreTitre(NkFamilleCtx &c) {
			static const char *const kMenus[4] = {"Fichier", "Édition", "Fenêtre", "Aide"};
			const NogeeModele &m = M();
			const NkString titre = NkString::Format("Nogee  —  %s%s", Fichier(m.chemin), m.modifie ? " *" : "");
			NkFamilleTitre t;
			t.menus = kMenus;
			t.nbMenus = 4;
			t.titre = titre.CStr();
			t.logo = &NogeeDessinerLogo;
			const int32 menuBarre = mMenus.menu >= 0 && mMenus.menu <= NOGEE_MENU_AIDE ? mMenus.menu : -1;
			const bool autre = mMenus.menu > NOGEE_MENU_AIDE;
			NkRect ancre;
			const int32 aOuvrir = NkFamilleBarreTitre(c, mPlan, t, menuBarre, autre, ancre, mFen);
			if (aOuvrir >= 0) {
				if (menuBarre >= 0 && aOuvrir != menuBarre) {
					mMenus.menu = aOuvrir; // le survol d'un voisin remplace le menu ouvert
					mMenus.ancre = ancre;
					mMenus.sousMenu = -1;
				} else {
					mMenus.Ouvrir(aOuvrir, ancre);
				}
			}
		}

		void NogeeInterface::OngletsScene(NkFamilleCtx &c) {
			const NogeeModele &m = M();
			NkFamilleOngletScene o;
			o.nom = Fichier(m.chemin);
			o.modifie = m.modifie;
			const NkFamilleOngletsResultat r = NkFamilleOngletsScene(c, mPlan, &o, 1, 0);
			if (r.fermer == 0) {
				if (m.modifie) {
					M().Annoncer("La scène est modifiée : Enregistrer (Ctrl+S) d'abord, ou Fichier > Nouvelle scène", 2);
				} else {
					Executer(NOGEE_A_NOUVEAU);
				}
			}
		}

		void NogeeInterface::BarreOutils(NkFamilleCtx &c) {
			const NkRect &b = mPlan.barreOutils;
			NkFamilleFondBarreOutils(c, b);
			NogeeModele &m = M();
			// La place des boutons dont le libelle VARIE : celle du plus long.
			float32 largeurOutil = 0.f;
			for (int32 k = 0; k < static_cast<int32>(NogeeOutil::Count); ++k) {
				const NkString s = NkString::Format("Outil : %s", NogeeNomOutil(static_cast<NogeeOutil>(k)));
				const float32 w = NkFamilleLargeurBoutonOutil(c, s.CStr(), true);
				largeurOutil = w > largeurOutil ? w : largeurOutil;
			}
			bool clic = false;
			NkRect r;
			float32 x = b.x + 8.f;
			x = NkFamilleBoutonOutil(c, b, x, "Enregistrer", false, false, clic, r);
			if (clic) {
				Executer(NOGEE_A_ENREGISTRER);
			}
			x = NkFamilleTrait(c, b, x);
			const NkString outil = NkString::Format("Outil : %s", NogeeNomOutil(m.outil));
			x = NkFamilleBoutonOutil(c, b, x, outil.CStr(), true, mMenus.menu == NOGEE_MENU_OUTIL, clic, r, largeurOutil);
			if (clic) {
				mMenus.Ouvrir(NOGEE_MENU_OUTIL, r);
			}
			x = NkFamilleTrait(c, b, x);
			x = NkFamilleBoutonOutil(c, b, x, "+ Ajouter", true, mMenus.menu == NOGEE_MENU_AJOUTER, clic, r);
			if (clic) {
				mMenus.Ouvrir(NOGEE_MENU_AJOUTER, r);
			}
			x = NkFamilleTrait(c, b, x);
			const NkFamilleEtatJeu etat = m.etat == NogeeEtatJeu::Jeu	 ? NkFamilleEtatJeu::Jeu
										  : m.etat == NogeeEtatJeu::Pause ? NkFamilleEtatJeu::Pause
																		  : NkFamilleEtatJeu::Edition;
			const int32 lecture = NkFamilleBoutonsLecture(c, b, x, etat);
			if (lecture >= 0) {
				static const int32 kActions[4] = {NOGEE_A_JOUER, NOGEE_A_PAUSE, NOGEE_A_ARRETER, NOGEE_A_PAS};
				Executer(kActions[lecture]);
			}
			x = NkFamilleTrait(c, b, x);
			x = NkFamilleBoutonOutil(c, b, x, "Appareil : Bureau", true, mMenus.menu == NOGEE_MENU_APPAREIL, clic, r,
									 NkFamilleLargeurBoutonOutil(c, "Appareil : Bureau", true) + 60.f);
			if (clic) {
				mMenus.Ouvrir(NOGEE_MENU_APPAREIL, r);
			}
			x = NkFamilleTrait(c, b, x);
			(void)NkFamilleBoutonOutil(c, b, x, "Réglages", true, mMenus.menu == NOGEE_MENU_REGLAGES, clic, r);
			if (clic) {
				mMenus.Ouvrir(NOGEE_MENU_REGLAGES, r);
			}
		}

		void NogeeInterface::Statut(NkFamilleCtx &c) {
			const NogeeModele &m = M();
			const NkFamilleEtatJeu etat = m.etat == NogeeEtatJeu::Jeu	 ? NkFamilleEtatJeu::Jeu
										  : m.etat == NogeeEtatJeu::Pause ? NkFamilleEtatJeu::Pause
																		  : NkFamilleEtatJeu::Edition;
			uint32 appels = 0;
			if (mHote.moteur != nullptr) {
				appels = mHote.moteur->GetRenderSystem().GetLastSubmittedCount();
			}
			const NkString compteurs =
				NkString::Format("%u entités   ·   maillages soumis %u   ·   vue %ux%u   ·   %.0f ips",
								 static_cast<unsigned>(mArbreEntites.Size()), static_cast<unsigned>(appels),
								 mHote.vue != nullptr ? mHote.vue->Largeur() : 0u, mHote.vue != nullptr ? mHote.vue->Hauteur() : 0u,
								 static_cast<double>(mIps));
			NkFamilleBarreEtat(c, mPlan.statut, etat, m.messageAge < 4.f ? m.message.CStr() : "", compteurs.CStr());
		}

		void NogeeInterface::AppliquerFenetre() {
			NkWindow *f = mHote.fenetre;
			nkgui::NkGuiInput &in = mCtx.input;
			if (mFen.fermerDemande) {
				mFen.fermerDemande = false;
				Executer(NOGEE_A_QUITTER);
			}
			if (f == nullptr) {
				return;
			}
			if (mFen.reduireDemande) {
				mFen.reduireDemande = false;
				f->Minimize();
			}
			if (mFen.agrandirDemande) {
				mFen.agrandirDemande = false;
				if (f->IsMaximized()) {
					f->Restore();
				} else {
					f->Maximize();
				}
			}
			if (mFen.deplacerDemande) {
				mFen.deplacerDemande = false;
				// TIRER UNE FENETRE AGRANDIE LA RESTAURE, sous le curseur (la recette
				// d'UnkenyEditor, AppliquerDemandesFenetre).
				if (f->IsMaximized()) {
					const NkVec2 souris = in.mousePos;
					f->Restore();
					const math::NkVec2u taille = f->GetSize();
					const int32 nx = static_cast<int32>(souris.x - static_cast<float32>(taille.x) * mFen.deplacerFractionX);
					const int32 ny = static_cast<int32>(souris.y - 12.f);
					f->SetPosition(nx < 0 ? 0 : nx, ny < 0 ? 0 : ny);
				}
				f->BeginDragMove();
				in.mouseDown[0] = false; // la boucle modale a mange le relachement
			}
			if (mFen.redimDemande >= 0) {
				const NkWindow::NkResizeEdge bord = static_cast<NkWindow::NkResizeEdge>(mFen.redimDemande);
				mFen.redimDemande = -1;
				f->BeginResize(bord);
				in.mouseDown[0] = false;
			}
		}

		// =====================================================================
		// LES MENUS
		// =====================================================================
		void NogeeInterface::Remplir(int32 menu, NkVector<NkFamilleEntreeMenu> &out) {
			NogeeModele &m = M();
			const bool sel = m.SelectionValide();
			const bool edition = m.etat == NogeeEtatJeu::Edition;
			int32 nCat = 0;
			const NogeeElement *cat = NogeeCatalogue(nCat);
			auto Catalogue = [&](int32 base) {
				static const struct {
						const char *titre;
						O onglet;
				} kGroupes[] = {{"Base", O::Base}, {"Lumières", O::Lumieres}, {"Formes", O::Formes}};
				for (const auto &g : kGroupes) {
					out.PushBack(NkFamilleIntitule(g.titre));
					for (int32 k = 0; k < nCat; ++k) {
						if ((cat[k].onglets & NkFamilleBitOnglet(g.onglet)) != 0u &&
							!(g.onglet == O::Formes && (cat[k].onglets & NkFamilleBitOnglet(O::Base)) != 0u)) {
							out.PushBack(NkFamilleLigneMenu(cat[k].nom, base + k, "", false, edition));
						}
					}
				}
			};
			switch (menu) {
				case NOGEE_MENU_FICHIER:
					out.PushBack(NkFamilleLigneMenu("Nouvelle scène", NOGEE_A_NOUVEAU, "Ctrl+N"));
					out.PushBack(NkFamilleLigneMenu("Ouvrir la scène enregistrée", NOGEE_A_OUVRIR, "Ctrl+O"));
					out.PushBack(NkFamilleSeparateur());
					out.PushBack(NkFamilleLigneMenu("Enregistrer", NOGEE_A_ENREGISTRER, "Ctrl+S", false, edition));
					out.PushBack(NkFamilleSeparateur());
					out.PushBack(NkFamilleLigneMenu("Quitter", NOGEE_A_QUITTER, "Ctrl+Q"));
					break;
				case NOGEE_MENU_EDITION:
					out.PushBack(NkFamilleLigneMenu("Annuler", NOGEE_A_ANNULER, "Ctrl+Z", false, m.PeutAnnuler() && edition));
					out.PushBack(NkFamilleLigneMenu("Rétablir", NOGEE_A_REFAIRE, "Ctrl+Y", false, m.PeutRefaire() && edition));
					out.PushBack(NkFamilleSeparateur());
					out.PushBack(NkFamilleLigneMenu("Nouvelle entité", NOGEE_A_NOUVELLE_ENTITE, "Ctrl+E", false, edition));
					out.PushBack(NkFamilleLigneMenu("Dupliquer", NOGEE_A_DUPLIQUER, "Ctrl+D", false, sel && edition));
					out.PushBack(NkFamilleLigneMenu("Renommer", NOGEE_A_RENOMMER, "F2", false, sel));
					out.PushBack(NkFamilleLigneMenu("Supprimer", NOGEE_A_SUPPRIMER, "Suppr", false, sel && edition));
					out.PushBack(NkFamilleSeparateur());
					out.PushBack(NkFamilleLigneMenu("Cadrer la sélection", NOGEE_A_CADRER, "F", false, sel));
					out.PushBack(NkFamilleLigneMenu("Tout désélectionner", NOGEE_A_DESELECTIONNER, "", false, sel));
					break;
				case NOGEE_MENU_FENETRE:
					out.PushBack(NkFamilleLigneMenu("Placer des acteurs", NOGEE_A_VOIR_PLACER, "", mPlan.voirPlacer));
					out.PushBack(NkFamilleLigneMenu("Outliner", NOGEE_A_VOIR_OUTLINER, "", mPlan.voirOutliner));
					out.PushBack(NkFamilleLigneMenu("Détails", NOGEE_A_VOIR_DETAILS, "", mPlan.voirDetails));
					out.PushBack(NkFamilleLigneMenu("Tiroir du bas", NOGEE_A_VOIR_TIROIR, "", mPlan.voirTiroir));
					out.PushBack(NkFamilleSeparateur());
					out.PushBack(NkFamilleLigneMenu("Contenu", NOGEE_A_TIROIR_CONTENU, "", mOngletTiroir == 0));
					out.PushBack(NkFamilleLigneMenu("Journal", NOGEE_A_TIROIR_JOURNAL, "", mOngletTiroir == 1));
					out.PushBack(NkFamilleLigneMenu("Terminal", NOGEE_A_TIROIR_TERMINAL, "", mOngletTiroir == 2));
					out.PushBack(NkFamilleSeparateur());
					out.PushBack(NkFamilleLigneMenu("Thème clair", NOGEE_A_THEME, "", mClair));
					out.PushBack(NkFamilleLigneMenu("Réinitialiser la disposition", NOGEE_A_DISPOSITION));
					out.PushBack(NkFamilleSeparateur());
					// CE QUI NE RENTRE PAS ENCORE : l'ancienne coquille (NkEditorShell,
					// panneaux ancrables, palette Ctrl+P, sondes) reste entiere.
					out.PushBack(NkFamilleLigneMenu("Ancienne coquille Nogee (NkEditorShell)…", NOGEE_A_ANCIENNE_COQUILLE));
					break;
				case NOGEE_MENU_AIDE:
					out.PushBack(NkFamilleIntitule("Nogee — l'éditeur de Noge (moteur 3D)"));
					out.PushBack(NkFamilleIntitule("Disposition d'UnkenyEditor (R32), pièces NKEditorKit/Famille"));
					out.PushBack(NkFamilleSeparateur());
					out.PushBack(NkFamilleIntitule("Clic droit + souris : tourner la vue"));
					out.PushBack(NkFamilleIntitule("Clic milieu : glisser la vue · molette : zoom"));
					out.PushBack(NkFamilleIntitule("Q W E R : sélection, déplacer, tourner, échelle"));
					out.PushBack(NkFamilleIntitule("F : cadrer · Espace : jouer · Échap : arrêter"));
					out.PushBack(NkFamilleIntitule("Ctrl+Z / Ctrl+Y · Ctrl+D · Suppr · F2"));
					break;
				case NOGEE_MENU_OUTIL:
					for (int32 k = 0; k < static_cast<int32>(NogeeOutil::Count); ++k) {
						static const char *const kTouches[4] = {"Q", "W", "E", "R"};
						out.PushBack(NkFamilleLigneMenu(NogeeNomOutil(static_cast<NogeeOutil>(k)), NOGEE_A_OUTIL + k, kTouches[k],
														m.outil == static_cast<NogeeOutil>(k)));
					}
					break;
				case NOGEE_MENU_AJOUTER:
					Catalogue(NOGEE_A_PLACER);
					break;
				case NOGEE_MENU_AJOUTER_ICI:
					Catalogue(NOGEE_A_PLACER_ICI);
					break;
				case NOGEE_MENU_APPAREIL:
					out.PushBack(NkFamilleLigneMenu("Bureau (la fenêtre)", NOGEE_A_AUCUNE, "", true, false));
					out.PushBack(NkFamilleSeparateur());
					out.PushBack(NkFamilleIntitule("Les profils d'appareils 3D viendront avec"));
					out.PushBack(NkFamilleIntitule("les réglages propres à Nogee (R32, étape 2)"));
					break;
				case NOGEE_MENU_REGLAGES:
					out.PushBack(NkFamilleLigneMenu("Grille", NOGEE_A_GRILLE, "", m.voirGrille));
					out.PushBack(NkFamilleLigneMenu("Filaire", NOGEE_A_FILAIRE, "", mFilaire));
					out.PushBack(NkFamilleSeparateur());
					out.PushBack(NkFamilleLigneMenu("Repère local", NOGEE_A_REPERE_LOCAL, "", m.repereLocal));
					out.PushBack(NkFamilleLigneMenu("Accrochage des déplacements", NOGEE_A_ACCROCHE_GRILLE, "", m.accrocheGrille));
					out.PushBack(NkFamilleLigneMenu("Accrochage des rotations", NOGEE_A_ACCROCHE_ANGLE, "", m.accrocheAngle));
					out.PushBack(NkFamilleLigneMenu("Accrochage des échelles", NOGEE_A_ACCROCHE_ECHELLE, "", m.accrocheEchelle));
					break;
				case NOGEE_MENU_CTX_ENTITE:
					out.PushBack(NkFamilleLigneMenu("Renommer", NOGEE_A_RENOMMER, "F2"));
					out.PushBack(NkFamilleLigneMenu("Dupliquer", NOGEE_A_DUPLIQUER, "Ctrl+D", false, edition));
					out.PushBack(NkFamilleLigneMenu("Cadrer", NOGEE_A_CADRER, "F"));
					out.PushBack(NkFamilleLigneMenu("Détacher du parent", NOGEE_A_DETACHER, "", false,
													sel && m.Parent(m.selection).IsValid() && edition));
					out.PushBack(NkFamilleSeparateur());
					out.PushBack(NkFamilleLigneMenu("Supprimer", NOGEE_A_SUPPRIMER, "Suppr", false, edition));
					break;
				case NOGEE_MENU_CTX_VIDE:
					out.PushBack(NkFamilleSousMenu("Ajouter ici", NOGEE_MENU_AJOUTER_ICI));
					out.PushBack(NkFamilleLigneMenu("Nouvelle entité", NOGEE_A_NOUVELLE_ENTITE, "Ctrl+E", false, edition));
					out.PushBack(NkFamilleSeparateur());
					out.PushBack(NkFamilleLigneMenu("Tout désélectionner", NOGEE_A_DESELECTIONNER, "", false, sel));
					break;
				case NOGEE_MENU_COMPOSANT: {
					static const struct {
							const char *nom;
							NogeeComposant k;
							int32 variante;
					} kAjouts[] = {
						{"Maillage : cube", NogeeComposant::Maillage, 0},		 {"Maillage : sphère", NogeeComposant::Maillage, 1},
						{"Lumière ponctuelle", NogeeComposant::Lumiere, 1},	 {"Lumière directionnelle", NogeeComposant::Lumiere, 0},
						{"Projecteur", NogeeComposant::Lumiere, 2},			 {"Caméra", NogeeComposant::Camera, 0},
						{"Corps rigide", NogeeComposant::Corps, 0},			 {"Collisionneur : boîte", NogeeComposant::Collisionneur, 0},
						{"Collisionneur : sphère", NogeeComposant::Collisionneur, 1}, {"Collisionneur : capsule", NogeeComposant::Collisionneur, 2},
					};
					out.PushBack(NkFamilleIntitule("Ajouter un composant"));
					out.PushBack(NkFamilleSeparateur());
					bool un = false;
					for (const auto &a : kAjouts) {
						if (PeutAjouter(a.k)) {
							out.PushBack(NkFamilleLigneMenu(a.nom, NOGEE_A_COMPOSANT + static_cast<int32>(a.k) * 10 + a.variante, "",
															false, edition));
							un = true;
						}
					}
					if (!un) {
						out.PushBack(NkFamilleIntitule("(l'entité a déjà tous les composants)"));
					}
					break;
				}
				case NOGEE_MENU_CARTE:
					out.PushBack(NkFamilleLigneMenu("Retirer le composant", NOGEE_A_RETIRER_COMPOSANT, "", false,
													edition && mCarteMenu != static_cast<int32>(NogeeComposant::Transform) &&
														mCarteMenu != static_cast<int32>(NogeeComposant::Hierarchie)));
					break;
				case NOGEE_MENU_PAS_GRILLE:
					for (int32 k = 0; k < static_cast<int32>(sizeof(kPasGrille) / sizeof(kPasGrille[0])); ++k) {
						out.PushBack(NkFamilleLigneMenu(NkString::Format("%g m", static_cast<double>(kPasGrille[k])).CStr(),
														NOGEE_A_PAS_GRILLE + k, "", m.pasGrille == kPasGrille[k]));
					}
					break;
				case NOGEE_MENU_PAS_ANGLE:
					for (int32 k = 0; k < static_cast<int32>(sizeof(kPasAngle) / sizeof(kPasAngle[0])); ++k) {
						out.PushBack(NkFamilleLigneMenu(NkString::Format("%g°", static_cast<double>(kPasAngle[k])).CStr(),
														NOGEE_A_PAS_ANGLE + k, "", m.pasAngle == kPasAngle[k]));
					}
					break;
				case NOGEE_MENU_PAS_ECHELLE:
					for (int32 k = 0; k < static_cast<int32>(sizeof(kPasEchelle) / sizeof(kPasEchelle[0])); ++k) {
						out.PushBack(NkFamilleLigneMenu(NkString::Format("x%g", static_cast<double>(kPasEchelle[k])).CStr(),
														NOGEE_A_PAS_ECHELLE + k, "", m.pasEchelle == kPasEchelle[k]));
					}
					break;
				default:
					break;
			}
		}

		// =====================================================================
		// LES ACTIONS
		// =====================================================================
		void NogeeInterface::Executer(int32 a) {
			NogeeModele &m = M();
			const bool edition = m.etat == NogeeEtatJeu::Edition;
			if (a >= NOGEE_A_PLACER_ICI && a < NOGEE_A_PLACER_ICI + 200) {
				if (edition) {
					(void)m.PoserElement(a - NOGEE_A_PLACER_ICI, PointDePose(a - NOGEE_A_PLACER_ICI, false, NkVec2{0.f, 0.f}));
				}
				return;
			}
			if (a >= NOGEE_A_PLACER && a < NOGEE_A_PLACER + 200) {
				if (edition) {
					const int32 k = a - NOGEE_A_PLACER;
					(void)m.PoserElement(k, PointDePose(k, true, NkVec2{0.f, 0.f}));
					mPlacer.Retenir(k);
				}
				return;
			}
			if (a >= NOGEE_A_COMPOSANT && a < NOGEE_A_COMPOSANT + 100) {
				const int32 v = a - NOGEE_A_COMPOSANT;
				AjouterComposant(static_cast<NogeeComposant>(v / 10), v % 10);
				return;
			}
			if (a >= NOGEE_A_OUTIL && a < NOGEE_A_OUTIL + static_cast<int32>(NogeeOutil::Count)) {
				m.outil = static_cast<NogeeOutil>(a - NOGEE_A_OUTIL);
				return;
			}
			if (a >= NOGEE_A_PAS_GRILLE && a < NOGEE_A_PAS_GRILLE + 6) {
				m.pasGrille = kPasGrille[a - NOGEE_A_PAS_GRILLE];
				m.accrocheGrille = true;
				return;
			}
			if (a >= NOGEE_A_PAS_ANGLE && a < NOGEE_A_PAS_ANGLE + 7) {
				m.pasAngle = kPasAngle[a - NOGEE_A_PAS_ANGLE];
				m.accrocheAngle = true;
				return;
			}
			if (a >= NOGEE_A_PAS_ECHELLE && a < NOGEE_A_PAS_ECHELLE + 5) {
				m.pasEchelle = kPasEchelle[a - NOGEE_A_PAS_ECHELLE];
				m.accrocheEchelle = true;
				return;
			}
			switch (a) {
				case NOGEE_A_NOUVEAU:
					if (m.etat != NogeeEtatJeu::Edition) {
						m.Arreter();
					}
					m.NouvelleScene();
					break;
				case NOGEE_A_OUVRIR:
					if (m.etat != NogeeEtatJeu::Edition) {
						m.Arreter();
					}
					(void)m.Ouvrir(m.chemin.CStr());
					break;
				case NOGEE_A_ENREGISTRER:
					(void)m.Enregistrer();
					break;
				case NOGEE_A_QUITTER:
					mDemandeQuitter = true;
					break;
				case NOGEE_A_ANNULER:
					(void)m.Annuler();
					break;
				case NOGEE_A_REFAIRE:
					(void)m.Refaire();
					break;
				case NOGEE_A_DUPLIQUER:
					if (m.SelectionValide() && edition) {
						(void)m.Dupliquer(m.selection);
					}
					break;
				case NOGEE_A_SUPPRIMER:
					if (m.SelectionValide() && edition) {
						m.Supprimer(m.selection);
					}
					break;
				case NOGEE_A_RENOMMER:
					// La saisie s'ouvre EN PLACE dans l'Outliner, a la trame suivante.
					if (m.SelectionValide()) {
						mOutliner.renommerDemande = true;
					}
					break;
				case NOGEE_A_DESELECTIONNER:
					m.Deselectionner();
					break;
				case NOGEE_A_CADRER:
					if (m.SelectionValide()) {
						m.Cadrer(m.selection);
					}
					break;
				case NOGEE_A_DETACHER:
					if (m.SelectionValide() && edition) {
						m.Retenir();
						m.Rattacher(m.selection, ecs::NkEntityId::Invalid());
					}
					break;
				case NOGEE_A_NOUVELLE_ENTITE:
					if (edition) {
						int32 n = 0;
						const NogeeElement *cat = NogeeCatalogue(n);
						for (int32 k = 0; k < n; ++k) {
							if (cat[k].genre == NogeeGenre::ActeurVide) {
								(void)m.PoserElement(k, PointDePose(k, true, NkVec2{0.f, 0.f}));
								break;
							}
						}
					}
					break;
				case NOGEE_A_JOUER:
					m.Jouer();
					break;
				case NOGEE_A_PAUSE:
					m.Pause();
					break;
				case NOGEE_A_ARRETER:
					m.Arreter();
					break;
				case NOGEE_A_PAS:
					m.Pas();
					break;
				case NOGEE_A_VOIR_PLACER:
					mPlan.voirPlacer = !mPlan.voirPlacer;
					break;
				case NOGEE_A_VOIR_OUTLINER:
					mPlan.voirOutliner = !mPlan.voirOutliner;
					break;
				case NOGEE_A_VOIR_DETAILS:
					mPlan.voirDetails = !mPlan.voirDetails;
					break;
				case NOGEE_A_VOIR_TIROIR:
					mPlan.voirTiroir = !mPlan.voirTiroir;
					break;
				case NOGEE_A_TIROIR_CONTENU:
					mOngletTiroir = 0;
					mPlan.voirTiroir = true;
					break;
				case NOGEE_A_TIROIR_JOURNAL:
					mOngletTiroir = 1;
					mPlan.voirTiroir = true;
					break;
				case NOGEE_A_TIROIR_TERMINAL:
					mOngletTiroir = 2;
					mPlan.voirTiroir = true;
					break;
				case NOGEE_A_THEME:
					PoserTheme(!mClair);
					break;
				case NOGEE_A_DISPOSITION: {
					const NkFamillePlan neuf;
					mPlan.largeurPlacer = neuf.largeurPlacer;
					mPlan.largeurOutliner = neuf.largeurOutliner;
					mPlan.largeurDetails = neuf.largeurDetails;
					mPlan.hauteurTiroir = neuf.hauteurTiroir;
					mPlan.voirPlacer = mPlan.voirOutliner = mPlan.voirDetails = mPlan.voirTiroir = true;
					break;
				}
				case NOGEE_A_ANCIENNE_COQUILLE: {
#if defined(_WIN32)
					// Le MEME executable, sur son ancien chemin : rien n'a ete detruit.
					wchar_t exe[MAX_PATH] = {};
					if (GetModuleFileNameW(nullptr, exe, MAX_PATH) > 0) {
						wchar_t ligne[MAX_PATH + 64];
						std::swprintf(ligne, MAX_PATH + 64, L"\"%ls\" --ancienne-coquille", exe);
						STARTUPINFOW si{};
						si.cb = sizeof(si);
						PROCESS_INFORMATION pi{};
						if (CreateProcessW(nullptr, ligne, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
							CloseHandle(pi.hThread);
							CloseHandle(pi.hProcess);
							m.Annoncer("Ancienne coquille ouverte dans une autre fenêtre (--ancienne-coquille)");
						} else {
							m.Annoncer("L'ancienne coquille ne s'ouvre pas", 3);
						}
					}
#else
					m.Annoncer("Ancienne coquille : relancer Nogee avec --ancienne-coquille", 2);
#endif
					break;
				}
				case NOGEE_A_GRILLE:
					m.voirGrille = !m.voirGrille;
					break;
				case NOGEE_A_FILAIRE:
					mFilaire = !mFilaire;
					if (mHote.moteur != nullptr) {
						mHote.moteur->GetRenderSystem().SetWireframe(mFilaire);
					}
					break;
				case NOGEE_A_REPERE_LOCAL:
					m.repereLocal = !m.repereLocal;
					break;
				case NOGEE_A_ACCROCHE_GRILLE:
					m.accrocheGrille = !m.accrocheGrille;
					break;
				case NOGEE_A_ACCROCHE_ANGLE:
					m.accrocheAngle = !m.accrocheAngle;
					break;
				case NOGEE_A_ACCROCHE_ECHELLE:
					m.accrocheEchelle = !m.accrocheEchelle;
					break;
				case NOGEE_A_RETIRER_COMPOSANT:
					AjouterComposant(static_cast<NogeeComposant>(mCarteMenu), -1);
					break;
				default:
					break;
			}
		}

		// =====================================================================
		// LES RACCOURCIS
		// =====================================================================
		bool NogeeInterface::ChampAuClavier() const noexcept {
			return mCtx.inputId != nkgui::NKGUI_ID_NONE || !mOutliner.Libre() || !mDetails.Libre() || mPlacer.filtreFocus ||
				   mJournal.rechercheFocus || mContenu.modele.searchFocused || mTerminal.AFocus();
		}

		void NogeeInterface::Raccourcis(NkFamilleCtx &c) {
			const nkgui::NkGuiInput &in = c.ctx.input;
			if (in.KeyPressed(NkGuiKey::Escape) && mMenus.Ouvert()) {
				mMenus.Fermer();
				return;
			}
			if (ChampAuClavier()) {
				return;
			}
			if (in.ctrlDown) {
				if (in.KeyPressed(NkGuiKey::Z)) {
					Executer(in.shiftDown ? NOGEE_A_REFAIRE : NOGEE_A_ANNULER);
					return;
				}
				struct NkRaccourci {
						NkGuiKey touche;
						int32 action;
				};
				static const NkRaccourci kCtrl[] = {
					{NkGuiKey::Y, NOGEE_A_REFAIRE},			 {NkGuiKey::N, NOGEE_A_NOUVEAU}, {NkGuiKey::O, NOGEE_A_OUVRIR},
					{NkGuiKey::S, NOGEE_A_ENREGISTRER},		 {NkGuiKey::D, NOGEE_A_DUPLIQUER}, {NkGuiKey::E, NOGEE_A_NOUVELLE_ENTITE},
					{NkGuiKey::Q, NOGEE_A_QUITTER},
				};
				for (const NkRaccourci &r : kCtrl) {
					if (in.KeyPressed(r.touche)) {
						Executer(r.action);
					}
				}
				return;
			}
			struct NkTouche {
					NkGuiKey touche;
					int32 action;
			};
			// Q / W / E / R sont ceux d'UE5 (selection, deplacer, tourner, echelle).
			static const NkTouche kSimples[] = {
				{NkGuiKey::Delete, NOGEE_A_SUPPRIMER},
				{NkGuiKey::Space, NOGEE_A_JOUER},
				{NkGuiKey::Escape, NOGEE_A_ARRETER},
				{NkGuiKey::F, NOGEE_A_CADRER},
				{NkGuiKey::F2, NOGEE_A_RENOMMER},
				{NkGuiKey::Q, NOGEE_A_OUTIL + static_cast<int32>(NogeeOutil::Selection)},
				{NkGuiKey::W, NOGEE_A_OUTIL + static_cast<int32>(NogeeOutil::Deplacer)},
				{NkGuiKey::E, NOGEE_A_OUTIL + static_cast<int32>(NogeeOutil::Tourner)},
				{NkGuiKey::R, NOGEE_A_OUTIL + static_cast<int32>(NogeeOutil::Echelle)},
			};
			for (const NkTouche &t : kSimples) {
				if (in.KeyPressed(t.touche)) {
					Executer(t.action);
				}
			}
		}

	} // namespace nogee
} // namespace nkentseu
