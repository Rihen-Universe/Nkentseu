// -----------------------------------------------------------------------------
// @File    NKScenaInterface.cpp
// @Brief   La couche d'interface de NKScena : la trame, le chrome, les menus, les
//          actions, les raccourcis (voir NKScenaInterface.h).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NKScena/NKScenaInterface.h"
#include "NKScena/NKScenaLogo.h"
#include "NKScena/NKScenaPont.h"
#include "Nogee/Editeur/NogeeVue3D.h"

#include "Noge/Core/NkApplication.h"
#include "Noge/Layers/NkEngineLayer.h"
#include "NKEditorKit/NkFilePickerNav.h"
#include "NKEditorKit/NkThemeToGui.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKLogger/NkLog.h"
#include "NKRenderer/NkRenderer.h" // SetUIOverlayCallback (passe Overlay2D)
#include "NKWindow/Core/NkWindow.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace nkscena {

		using namespace editorkit;
		using nkgui::NkColor;
		using nkgui::NkGuiKey;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {
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

			const char *NomOutil(int32 o) {
				switch (o) {
					case 1:
						return "Déplacer";
					case 2:
						return "Tourner";
					case 3:
						return "Échelle";
					default:
						return "Sélection";
				}
			}

			/// « 00:00:01:12 » : heures, minutes, secondes, images (le timecode du
			/// montage, design/02 §3.1).
			NkString Timecode(float32 t, float32 ips) {
				const int32 images = static_cast<int32>(t * ips + 0.5f);
				const int32 parSeconde = static_cast<int32>(ips + 0.5f) > 0 ? static_cast<int32>(ips + 0.5f) : 24;
				const int32 s = images / parSeconde;
				return NkString::Format("%02d:%02d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60, images % parSeconde);
			}
		} // namespace

		// =====================================================================
		// LA COUCHE
		// =====================================================================
		NkScenaInterface::NkScenaInterface(const NkScenaHote &hote) noexcept
			: NkOverlay("NkScenaInterface"), mHote(hote), mSelecteur(memory::NkMakeUnique<NkFilePickerNavState>()) {
			// La frise est la SURFACE PROPRE de NKScena : le tiroir est plus haut que
			// chez Nogee, et « Placer des acteurs » n'a rien a placer ici.
			plan.voirPlacer = false;
			plan.hauteurTiroir = 300.f;
		}

		NkScenaInterface::~NkScenaInterface() = default;

		void NkScenaInterface::OnAttach() {
			NkIDevice *dev = mHote.device;
			const uint32 W = dev != nullptr ? dev->GetSwapchainWidth() : 1280u;
			const uint32 H = dev != nullptr ? dev->GetSwapchainHeight() : 760u;
			if (!InitialiserGui(static_cast<int32>(W), static_cast<int32>(H))) {
				logger.Errorf("[NKScena] NkGuiContext::Init a echoue : pas d'interface\n");
				return;
			}
			mPret = true;
			// LE RENDU : le backend sur la passe de la swapchain, les atlas, puis le
			// rappel dans la passe Overlay2D du renderer de l'application (le chemin
			// de Nogee, au pixel).
			if (dev != nullptr && mBackend.Init(dev, dev->GetSwapchainRenderPass(), mHote.api)) {
				mBackendPret = true;
				nkgui::NkGuiFont *polices[3];
				const int32 n = Polices(polices, 3);
				for (int32 k = 0; k < n; ++k) {
					mBackend.UploadTextureGray8(polices[k]->TexId(), polices[k]->pixels, polices[k]->atlasW, polices[k]->atlasH);
				}
				if (renderer::NkRenderer *r = NkApplication::Get().GetRenderer()) {
					r->SetUIOverlayCallback([this](NkICommandBuffer *cmd) {
						if (!mBackendPret || mHote.device == nullptr || mListe == nullptr) {
							return;
						}
						mBackend.Submit(cmd, *mListe, mHote.device->GetSwapchainWidth(), mHote.device->GetSwapchainHeight());
					});
					mRappelPose = true;
				}
			} else {
				logger.Errorf("[NKScena] le backend NKGui -> NKRHI est refuse : l'interface ne sera pas rendue\n");
			}
			mContenu.racine = mHote.contenu;
			mContenu.nomProjet = NkString("NKScena");
			mTerminal.dossierDepart = NkString(".");
			mTerminal.police = PoliceMono();
			mJournal.Ajouter("NKScena — le temps sur une scène Noge : pistes, clés, plans caméra, rendu en images");
			mJournal.Ajouter("Vue 3D : la scène de Noge (NkEngineLayer) rendue hors écran, la vue de Nogee empruntée");
			logger.Infof("[NKScena] interface attachee %ux%u (backend RHI : %d)\n", W, H, mBackendPret ? 1 : 0);
		}

		void NkScenaInterface::LibererGpu() noexcept {
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
			mListe = nullptr;
		}

		void NkScenaInterface::OnDetach() {
			LibererGpu();
			mTerminal.FermerTout();
			if (mPret) {
				Terminer();
				mPret = false;
			}
		}

		void NkScenaInterface::OnUpdate(float dt) {
			mDt = dt > 0.f ? dt : 1.f / 60.f;
			M().scene.messageAge += mDt;
			mTerminal.Pomper();
		}

		bool NkScenaInterface::OnEvent(NkEvent *event) {
			if (event != nullptr && mPret) {
				LireEvenement(*event);
			}
			return false;
		}

		void NkScenaInterface::OnUIRender() {
			if (!mPret || mHote.device == nullptr || mHote.modele == nullptr || !M().Pret()) {
				return;
			}
			const uint32 W = mHote.device->GetSwapchainWidth();
			const uint32 H = mHote.device->GetSwapchainHeight();
			if (W == 0u || H == 0u) {
				return;
			}
			if (mBackendPret && mHote.vue != nullptr && mHote.vue->Pret() && mHote.vue->Generation() != mGenerationVue) {
				if (mBackend.RegisterTexture(kTexVue, mHote.vue->Texture())) {
					mGenerationVue = mHote.vue->Generation();
				}
			}
			const bool agrandie = mHote.fenetre != nullptr && mHote.fenetre->IsMaximized();
			mListe = &Trame(mDt, static_cast<int32>(W), static_cast<int32>(H), agrandie);
			if (mHote.fenetre != nullptr) {
				AppliquerFenetre(*mHote.fenetre);
			}
		}

		void NkScenaInterface::Journal(const char *texte, NkFamilleNiveau niveau) {
			const int32 s = static_cast<int32>(temps);
			const NkString ligne = NkString::Format("[%02d:%02d]  %s", s / 60, s % 60, texte != nullptr ? texte : "");
			mJournal.Ajouter(ligne.CStr(), niveau);
		}

		void NkScenaInterface::Journaliser() {
			nogee::NogeeModele &sc = M().scene;
			for (uint32 i = 0; i < sc.aJournaliser.Size(); ++i) {
				const uint8 n = i < sc.niveaux.Size() ? sc.niveaux[i] : 0u;
				Journal(sc.aJournaliser[i].CStr(), n == 3	? NkFamilleNiveau::Erreur
												   : n == 2 ? NkFamilleNiveau::Avertissement
												   : n == 1 ? NkFamilleNiveau::Succes
															: NkFamilleNiveau::Info);
			}
			sc.aJournaliser.Clear();
			sc.niveaux.Clear();
		}

		// =====================================================================
		// CE QUE NKSCENA PEINT DANS LA TRAME DE LA FAMILLE
		// =====================================================================
		void NkScenaInterface::AvantCorps(NkFamilleCtx &) {
			Journaliser();
		}

		void NkScenaInterface::PeindreCorps(NkFamilleCtx &c) {
			Vue(c);
			Outliner(c);
			Details(c);
			Tiroir(c);
		}

		NkString NkScenaInterface::Titre() const {
			const NkScenaModele &m = M();
			return NkString::Format("NKScena  —  %s%s  ·  %s", Fichier(m.chemin), m.modifie ? " *" : "", Fichier(m.CheminScene()));
		}

		const char *const *NkScenaInterface::MenusBarre(int32 &nombre) const {
			static const char *const kMenus[6] = {"Fichier", "Édition", "Piste", "Rendu", "Fenêtre", "Aide"};
			nombre = 6;
			return kMenus;
		}

		void NkScenaInterface::PeindreLogo(nkgui::NkGuiDrawList &dl, float32 x, float32 y, float32 cote, bool fondSombre) {
			NkScenaDessinerLogo(dl, x, y, cote, fondSombre);
		}

		void NkScenaInterface::Fermer() {
			Executer(SCENA_A_QUITTER);
		}

		bool NkScenaInterface::Modale() const {
			return mSelecteur && mSelecteur->pickerOpen;
		}

		void NkScenaInterface::PeindreModale(NkFamilleCtx &) {
			(void)NkDrawSelecteur(Gui(), *mSelecteur, theme);
			if (mSelecteur->pickerCancelled) {
				mSelecteur->pickerCancelled = false;
				mUsageSelecteur = 0;
			}
			if (mSelecteur->pickerConfirmed) {
				mSelecteur->pickerConfirmed = false;
				NkString chemin(mSelecteur->pickerResultPath);
				NkScenaModele &m = M();
				if (mUsageSelecteur == 1) {
					(void)m.Ouvrir(chemin.CStr());
				} else if (mUsageSelecteur == 2) {
					if (!chemin.EndsWith(".nkseq")) {
						chemin.Append(".nkseq");
					}
					(void)m.Enregistrer(chemin.CStr());
				} else if (mUsageSelecteur == 3) {
					(void)m.OuvrirScene(chemin.CStr());
				}
				mUsageSelecteur = 0;
			}
		}

		// =====================================================================
		// LE CHROME DE NKSCENA
		// =====================================================================
		void NkScenaInterface::PeindreOnglets(NkFamilleCtx &c) {
			const NkScenaModele &m = M();
			NkFamilleOngletScene o;
			o.nom = Fichier(m.chemin);
			o.modifie = m.modifie;
			(void)NkFamilleOngletsScene(c, plan, &o, 1, 0);
		}

		void NkScenaInterface::PeindreBarreOutils(NkFamilleCtx &c) {
			const NkRect &b = plan.barreOutils;
			NkFamilleFondBarreOutils(c, b);
			NkScenaModele &m = M();
			const bool rendu = mHote.rendu != nullptr && mHote.rendu->actif;
			float32 largeurOutil = 0.f;
			for (int32 k = 0; k < 4; ++k) {
				const NkString s = NkString::Format("Outil : %s", NomOutil(k));
				const float32 w = NkFamilleLargeurBoutonOutil(c, s.CStr(), true);
				largeurOutil = w > largeurOutil ? w : largeurOutil;
			}
			bool clic = false;
			NkRect r;
			float32 x = b.x + 8.f;
			x = NkFamilleBoutonOutil(c, b, x, "Ouvrir scène", false, false, clic, r, 0.f, !rendu);
			if (clic) {
				Executer(SCENA_A_OUVRIR_SCENE);
			}
			x = NkFamilleBoutonOutil(c, b, x, "Enregistrer séquence", false, false, clic, r, 0.f, !rendu);
			if (clic) {
				Executer(SCENA_A_ENREGISTRER);
			}
			x = NkFamilleTrait(c, b, x);
			const NkString outil = NkString::Format("Outil : %s", NomOutil(mOutil));
			x = NkFamilleBoutonOutil(c, b, x, outil.CStr(), true, menus.menu == SCENA_MENU_OUTIL, clic, r, largeurOutil);
			if (clic) {
				menus.Ouvrir(SCENA_MENU_OUTIL, r);
			}
			x = NkFamilleTrait(c, b, x);
			x = NkFamilleBoutonOutil(c, b, x, "+ Piste", true, menus.menu == SCENA_MENU_PISTE_PLUS, clic, r, 0.f, !rendu);
			if (clic) {
				menus.Ouvrir(SCENA_MENU_PISTE_PLUS, r);
			}
			x = NkFamilleBoutonOutil(c, b, x, "Clé (I)", false, false, clic, r, 0.f, !rendu);
			if (clic) {
				Executer(SCENA_A_CLE);
			}
			x = NkFamilleTrait(c, b, x);
			// LE TRANSPORT : les boutons de la famille (Jouer, Pause, Stop, Image
			// suivante), puis le TIMECODE (design/02 §3.1).
			const NkFamilleEtatJeu etat = m.frise.playing ? NkFamilleEtatJeu::Jeu
										  : m.frise.cursor > m.frise.PlayStart() + 1e-4f ? NkFamilleEtatJeu::Pause
																						 : NkFamilleEtatJeu::Edition;
			const int32 lecture = NkFamilleBoutonsLecture(c, b, x, etat);
			if (lecture >= 0 && !rendu) {
				static const int32 kActions[4] = {SCENA_A_JOUER, SCENA_A_PAUSE, SCENA_A_STOP, SCENA_A_IMAGE_SUIVANTE};
				Executer(kActions[lecture]);
			}
			{
				const NkString tc = Timecode(m.frise.cursor, m.frise.fps);
				const NkString images = NkString::Format("img %d / %d", m.frise.FrameOf(m.frise.cursor),
														 m.frise.FrameOf(m.frise.duration));
				auto &dl = c.ctx.dl;
				const float32 h = NkFamilleHauteurLigne(c.police, 16.f);
				const float32 wTc = NkFamilleLargeur(c.police, "00:00:00:00") + 16.f;
				const NkRect champ{x + 6.f, b.y + (b.h - 22.f) * 0.5f, wTc, 22.f};
				dl.AddRectFilled(champ, pal.champ, 2.f);
				dl.AddRect(champ, pal.bord, 1.f, 2.f);
				NkFamilleTexte(dl, c.police, champ.x + 8.f, champ.y + (champ.h - h) * 0.5f, tc.CStr(), pal.texte);
				NkFamilleTexte(dl, c.petite, champ.x + champ.w + 8.f, champ.y + 5.f, images.CStr(), pal.attenue);
				x = champ.x + champ.w + 14.f + NkFamilleLargeur(c.petite, images.CStr());
			}
			x = NkFamilleTrait(c, b, x);
			const char *vue = m.vueCamera ? "Vue : caméra du plan" : "Vue : libre";
			x = NkFamilleBoutonOutil(c, b, x, vue, false, m.vueCamera, clic, r,
									 NkFamilleLargeurBoutonOutil(c, "Vue : caméra du plan", false));
			if (clic) {
				Executer(SCENA_A_VUE_CAMERA);
			}
			x = NkFamilleTrait(c, b, x);
			if (rendu) {
				const NkString t = NkString::Format("Rendu %d / %d — Annuler", mHote.rendu->image, mHote.rendu->total);
				(void)NkFamilleBoutonOutil(c, b, x, t.CStr(), false, true, clic, r);
				if (clic) {
					Executer(SCENA_A_ANNULER_RENDU);
				}
			} else {
				(void)NkFamilleBoutonOutil(c, b, x, "Rendre", false, false, clic, r);
				if (clic) {
					Executer(SCENA_A_RENDRE);
				}
			}
		}

		void NkScenaInterface::PeindreStatut(NkFamilleCtx &c) {
			const NkScenaModele &m = M();
			const bool rendu = mHote.rendu != nullptr && mHote.rendu->actif;
			const NkFamilleEtatJeu etat = rendu || m.frise.playing ? NkFamilleEtatJeu::Jeu : NkFamilleEtatJeu::Edition;
			uint32 plans = 0;
			if (const NkTimelineTrack *p = m.frise.Track(m.PistePlans())) {
				plans = static_cast<uint32>(p->clips.Size());
			}
			const NkString compteurs = NkString::Format(
				"%u pistes  ·  %u clés  ·  %u plans  ·  %.0f i/s  ·  %.2f s  ·  vue %ux%u  ·  %.0f ips",
				static_cast<unsigned>(m.frise.tracks.Size()), static_cast<unsigned>(m.NombreCles()), static_cast<unsigned>(plans),
				static_cast<double>(m.frise.fps), static_cast<double>(m.frise.duration),
				mHote.vue != nullptr ? mHote.vue->Largeur() : 0u, mHote.vue != nullptr ? mHote.vue->Hauteur() : 0u,
				static_cast<double>(ips));
			const nogee::NogeeModele &sc = m.scene;
			NkFamilleBarreEtat(c, plan.statut, etat, sc.messageAge < 4.f ? sc.message.CStr() : "", compteurs.CStr());
		}

		// =====================================================================
		// LES MENUS
		// =====================================================================
		void NkScenaInterface::Remplir(int32 menu, NkVector<NkFamilleEntreeMenu> &out) {
			NkScenaModele &m = M();
			const bool rendu = mHote.rendu != nullptr && mHote.rendu->actif;
			const bool sel = m.scene.SelectionValide();
			const NkString nomSel(sel ? m.scene.Nom(m.scene.selection) : "");
			switch (menu) {
				case SCENA_MENU_FICHIER:
					out.PushBack(NkFamilleLigneMenu("Nouvelle séquence", SCENA_A_NOUVELLE, "Ctrl+N", false, !rendu));
					out.PushBack(NkFamilleLigneMenu("Ouvrir une séquence…", SCENA_A_OUVRIR_SEQUENCE, "Ctrl+O", false, !rendu));
					out.PushBack(NkFamilleSeparateur());
					out.PushBack(NkFamilleLigneMenu("Enregistrer la séquence", SCENA_A_ENREGISTRER, "Ctrl+S", false, !rendu));
					out.PushBack(NkFamilleLigneMenu("Enregistrer sous…", SCENA_A_ENREGISTRER_SOUS, "", false, !rendu));
					out.PushBack(NkFamilleSeparateur());
					out.PushBack(NkFamilleLigneMenu("Ouvrir une scène…", SCENA_A_OUVRIR_SCENE, "", false, !rendu));
					out.PushBack(NkFamilleLigneMenu("Scène de démonstration (NogeDemo)", SCENA_A_SCENE_DEMO, "", false, !rendu));
					out.PushBack(NkFamilleLigneMenu("Exemple : le joueur traverse", SCENA_A_EXEMPLE, "", false, !rendu));
					out.PushBack(NkFamilleSeparateur());
					out.PushBack(NkFamilleLigneMenu("Quitter", SCENA_A_QUITTER, "Ctrl+Q"));
					break;
				case SCENA_MENU_EDITION:
					out.PushBack(NkFamilleLigneMenu("Annuler", SCENA_A_ANNULER, "Ctrl+Z", false, m.frise.CanUndo() && !rendu));
					out.PushBack(NkFamilleLigneMenu("Rétablir", SCENA_A_REFAIRE, "Ctrl+Y", false, m.frise.CanRedo() && !rendu));
					out.PushBack(NkFamilleSeparateur());
					out.PushBack(NkFamilleLigneMenu("Copier les clés", SCENA_A_COPIER, "Ctrl+C", false, m.frise.SelectionCount() > 0u));
					out.PushBack(NkFamilleLigneMenu("Coller au curseur", SCENA_A_COLLER, "Ctrl+V", false, !m.frise.clipboard.Empty() && !rendu));
					out.PushBack(NkFamilleLigneMenu("Supprimer", SCENA_A_SUPPRIMER, "Suppr", false,
													(m.frise.SelectionCount() > 0u || m.frise.ClipSelectionCount() > 0u) && !rendu));
					out.PushBack(NkFamilleSeparateur());
					out.PushBack(NkFamilleLigneMenu("Cadrer la sélection", SCENA_A_CADRER, "F", false, sel));
					out.PushBack(NkFamilleLigneMenu("Tout désélectionner", SCENA_A_DESELECTIONNER, "", false, sel));
					break;
				case SCENA_MENU_PISTE: {
					const NkString piste = sel ? NkString::Format("Piste de transformation : %s", nomSel.CStr())
											   : NkString("Piste de transformation (choisir une entité)");
					out.PushBack(NkFamilleLigneMenu(piste.CStr(), SCENA_A_PISTE_ENTITE, "", false,
													sel && !m.APisteTransform(nomSel.CStr()) && !rendu));
					out.PushBack(NkFamilleSousMenu("Ajouter une piste", SCENA_MENU_PISTE_PLUS));
					out.PushBack(NkFamilleLigneMenu("Piste des plans caméra", SCENA_A_PISTE_PLANS, "", m.PistePlans() != 0, !rendu));
					out.PushBack(NkFamilleSousMenu("Plan de caméra au curseur", SCENA_MENU_PLAN_ICI));
					out.PushBack(NkFamilleSeparateur());
					out.PushBack(NkFamilleLigneMenu("Poser une clé au curseur", SCENA_A_CLE, "I", false, !rendu));
					out.PushBack(NkFamilleLigneMenu("Clé sur toute l'entité choisie", SCENA_A_CLES_ENTITE, "", false,
													sel && m.APisteTransform(nomSel.CStr()) && !rendu));
					out.PushBack(NkFamilleSeparateur());
					out.PushBack(NkFamilleLigneMenu("Image précédente", SCENA_A_IMAGE_PRECEDENTE, "←"));
					out.PushBack(NkFamilleLigneMenu("Image suivante", SCENA_A_IMAGE_SUIVANTE, "→"));
					break;
				}
				case SCENA_MENU_RENDU: {
					out.PushBack(NkFamilleLigneMenu("Rendre la séquence en images", SCENA_A_RENDRE, "Ctrl+R", false, !rendu));
					out.PushBack(NkFamilleLigneMenu("Annuler le rendu", SCENA_A_ANNULER_RENDU, "", false, rendu));
					out.PushBack(NkFamilleSeparateur());
					const NkString taille = NkString::Format("Sortie : %u x %u, PNG numérotés, %.0f i/s",
															 static_cast<unsigned>(m.sortie.largeur), static_cast<unsigned>(m.sortie.hauteur),
															 static_cast<double>(m.frise.fps));
					out.PushBack(NkFamilleIntitule(taille.CStr()));
					const NkString dossier = NkString::Format("Dossier : %s", m.sortie.dossier.CStr());
					out.PushBack(NkFamilleIntitule(dossier.CStr()));
					out.PushBack(NkFamilleIntitule("La vue rend ce que filme la caméra du plan (temps réel)"));
					break;
				}
				case SCENA_MENU_FENETRE:
					out.PushBack(NkFamilleLigneMenu("Outliner", SCENA_A_VOIR_OUTLINER, "", plan.voirOutliner));
					out.PushBack(NkFamilleLigneMenu("Détails", SCENA_A_VOIR_DETAILS, "", plan.voirDetails));
					out.PushBack(NkFamilleLigneMenu("Tiroir du bas", SCENA_A_VOIR_TIROIR, "", plan.voirTiroir));
					out.PushBack(NkFamilleSeparateur());
					out.PushBack(NkFamilleLigneMenu("Frise", SCENA_A_TIROIR_FRISE, "", mOngletTiroir == 0));
					out.PushBack(NkFamilleLigneMenu("Contenu", SCENA_A_TIROIR_CONTENU, "", mOngletTiroir == 1));
					out.PushBack(NkFamilleLigneMenu("Journal", SCENA_A_TIROIR_JOURNAL, "", mOngletTiroir == 2));
					out.PushBack(NkFamilleLigneMenu("Terminal", SCENA_A_TIROIR_TERMINAL, "", mOngletTiroir == 3));
					out.PushBack(NkFamilleSeparateur());
					out.PushBack(NkFamilleLigneMenu("Grille", SCENA_A_GRILLE, "", mVoirGrille));
					out.PushBack(NkFamilleLigneMenu("Thème clair", SCENA_A_THEME, "", Clair()));
					out.PushBack(NkFamilleLigneMenu("Réinitialiser la disposition", SCENA_A_DISPOSITION));
					break;
				case SCENA_MENU_AIDE:
					out.PushBack(NkFamilleIntitule("NKScena — le temps sur une scène Noge"));
					out.PushBack(NkFamilleIntitule("Pistes, clés et plans caméra sur la frise, rendu en images"));
					out.PushBack(NkFamilleSeparateur());
					out.PushBack(NkFamilleIntitule("Espace : jouer / pause · ← → : image par image"));
					out.PushBack(NkFamilleIntitule("I : poser une clé · Suppr : supprimer · Ctrl+Z / Ctrl+Y"));
					out.PushBack(NkFamilleIntitule("0 : vue libre / caméra du plan · Ctrl+R : rendre"));
					out.PushBack(NkFamilleIntitule("Clic droit + souris : tourner la vue · molette : zoom"));
					out.PushBack(NkFamilleIntitule("Q W E R : sélection, déplacer, tourner, échelle"));
					break;
				case SCENA_MENU_OUTIL:
					for (int32 k = 0; k < 4; ++k) {
						static const char *const kTouches[4] = {"Q", "W", "E", "R"};
						out.PushBack(NkFamilleLigneMenu(NomOutil(k), SCENA_A_OUTIL + k, kTouches[k], mOutil == k));
					}
					break;
				case SCENA_MENU_PISTE_PLUS: {
					out.PushBack(NkFamilleLigneMenu("Plans caméra", SCENA_A_PISTE_PLANS, "", m.PistePlans() != 0,
													m.PistePlans() == 0 && !rendu));
					out.PushBack(NkFamilleSeparateur());
					out.PushBack(NkFamilleIntitule("Transformation d'une entité"));
					mNomsMenu.Clear();
					NkVector<ecs::NkEntityId> ids;
					m.scene.Entites(ids);
					for (uint32 i = 0; i < ids.Size(); ++i) {
						const char *n = m.scene.Nom(ids[i]);
						mNomsMenu.PushBack(NkString(n));
						out.PushBack(NkFamilleLigneMenu(n, SCENA_A_PISTE_DE + static_cast<int32>(i), "", m.APisteTransform(n),
														!m.APisteTransform(n) && !rendu));
					}
					break;
				}
				case SCENA_MENU_PLAN_ICI: {
					const NkString t = NkString::Format("Un plan à %.2f s, filmé par :", static_cast<double>(mTempsMenuPlan));
					out.PushBack(NkFamilleIntitule(t.CStr()));
					mNomsMenu.Clear();
					m.Cameras(mNomsMenu);
					for (uint32 i = 0; i < mNomsMenu.Size(); ++i) {
						out.PushBack(NkFamilleLigneMenu(mNomsMenu[i].CStr(), SCENA_A_PLAN_DE + static_cast<int32>(i), "", false, !rendu));
					}
					if (mNomsMenu.Empty()) {
						out.PushBack(NkFamilleIntitule("(la scène n'a pas de caméra)"));
					}
					break;
				}
				case SCENA_MENU_CTX_ENTITE: {
					out.PushBack(NkFamilleLigneMenu("Ajouter sa piste de transformation", SCENA_A_PISTE_ENTITE, "", false,
													sel && !m.APisteTransform(nomSel.CStr()) && !rendu));
					out.PushBack(NkFamilleLigneMenu("Poser une clé au curseur", SCENA_A_CLES_ENTITE, "I", false,
													sel && m.APisteTransform(nomSel.CStr()) && !rendu));
					const bool camera = sel && m.scene.Monde().Has<ecs::NkCameraComponent>(m.scene.selection);
					if (camera) {
						out.PushBack(NkFamilleSeparateur());
						mNomsMenu.Clear();
						mNomsMenu.PushBack(nomSel);
						mTempsMenuPlan = m.frise.cursor;
						out.PushBack(NkFamilleLigneMenu("Un plan de cette caméra au curseur", SCENA_A_PLAN_DE, "", false, !rendu));
					}
					out.PushBack(NkFamilleSeparateur());
					out.PushBack(NkFamilleLigneMenu("Cadrer", SCENA_A_CADRER, "F"));
					break;
				}
				default:
					break;
			}
		}

		// =====================================================================
		// LES ACTIONS
		// =====================================================================
		void NkScenaInterface::Executer(int32 a) {
			NkScenaModele &m = M();
			NkTimelineModel &f = m.frise;
			const bool rendu = mHote.rendu != nullptr && mHote.rendu->actif;
			// Pendant un rendu, la sequence ne change pas sous la camera.
			if (rendu && a != SCENA_A_ANNULER_RENDU && a != SCENA_A_QUITTER && !(a >= SCENA_A_VOIR_OUTLINER && a <= SCENA_A_GRILLE)) {
				m.Annoncer("Un rendu est en cours : annulez-le d'abord (barre d'outils, ou Rendu > Annuler)", 2);
				return;
			}
			if (a >= SCENA_A_PISTE_DE && a < SCENA_A_PISTE_DE + 1000) {
				const uint32 k = static_cast<uint32>(a - SCENA_A_PISTE_DE);
				if (k < mNomsMenu.Size() && m.AjouterPisteTransform(mNomsMenu[k].CStr())) {
					m.scene.Choisir(m.Entite(mNomsMenu[k].CStr()));
				}
				return;
			}
			if (a >= SCENA_A_PLAN_DE && a < SCENA_A_PLAN_DE + 1000) {
				const uint32 k = static_cast<uint32>(a - SCENA_A_PLAN_DE);
				if (k < mNomsMenu.Size()) {
					// La duree : jusqu'au plan suivant, ou jusqu'a la fin de la sequence
					// (au moins une seconde).
					float32 fin = f.duration;
					if (const NkTimelineTrack *p = f.Track(m.PistePlans())) {
						for (uint32 i = 0; i < p->clips.Size(); ++i) {
							if (p->clips[i].start > mTempsMenuPlan + 1e-4f && p->clips[i].start < fin) {
								fin = p->clips[i].start;
							}
						}
					}
					const float32 duree = fin - mTempsMenuPlan > 1.f ? fin - mTempsMenuPlan : 1.f;
					(void)m.AjouterPlan(mNomsMenu[k].CStr(), mTempsMenuPlan, duree);
				}
				return;
			}
			if (a >= SCENA_A_OUTIL && a < SCENA_A_OUTIL + 4) {
				mOutil = a - SCENA_A_OUTIL;
				return;
			}
			auto OuvrirSelecteur = [&](int32 usage, int32 mode, const char *depart, const char *ext, const char *nom) {
				if (mSelecteur && !mSelecteur->pickerOpen) {
					mUsageSelecteur = usage;
					mTamponSelecteur[0] = '\0';
					mSelecteur->selectionMultiple = false;
					mSelecteur->OuvrirNav(mode, NkDirectory::Exists(depart) ? depart : ".", ext, nom, mTamponSelecteur,
										  static_cast<int32>(sizeof(mTamponSelecteur)));
				}
			};
			switch (a) {
				case SCENA_A_NOUVELLE:
					m.NouvelleSequence(5.f, f.fps > 0.f ? f.fps : 24.f);
					m.Annoncer("Nouvelle séquence (5 s, sur la scène ouverte) : « + Piste » pour commencer");
					break;
				case SCENA_A_OUVRIR_SEQUENCE:
					OuvrirSelecteur(1, NkSelecteurOuvrirFichier, "Build/NKScena", ".nkseq", nullptr);
					break;
				case SCENA_A_ENREGISTRER:
					(void)m.Enregistrer();
					break;
				case SCENA_A_ENREGISTRER_SOUS:
					OuvrirSelecteur(2, NkSelecteurEnregistrer, "Build/NKScena", ".nkseq", Fichier(m.chemin));
					break;
				case SCENA_A_OUVRIR_SCENE:
					OuvrirSelecteur(3, NkSelecteurOuvrirFichier, "Build/NKScena", ".nkscene3d", nullptr);
					break;
				case SCENA_A_SCENE_DEMO:
					(void)m.SceneDemo();
					break;
				case SCENA_A_EXEMPLE:
					(void)m.Exemple();
					break;
				case SCENA_A_QUITTER:
					if (mHote.rendu != nullptr && mHote.rendu->actif) {
						mHote.rendu->annuler = true;
					}
					DemanderQuitter();
					break;
				case SCENA_A_ANNULER:
					if (f.Undo()) {
						m.Annoncer("Annulé (Ctrl+Z)");
					}
					break;
				case SCENA_A_REFAIRE:
					if (f.Redo()) {
						m.Annoncer("Rétabli (Ctrl+Y)");
					}
					break;
				case SCENA_A_SUPPRIMER:
					(void)f.DeleteSelection();
					break;
				case SCENA_A_COPIER: {
					const uint32 n = f.CopySelection();
					m.Annoncer(NkString::Format("%u clé(s) copiée(s)", static_cast<unsigned>(n)).CStr());
					break;
				}
				case SCENA_A_COLLER:
					(void)f.PasteAt(f.cursor);
					break;
				case SCENA_A_JOUER:
					if (f.cursor >= f.PlayEnd() - 1e-4f) {
						f.cursor = f.PlayStart();
					}
					f.playing = true;
					break;
				case SCENA_A_PAUSE:
					f.playing = false;
					break;
				case SCENA_A_STOP:
					f.playing = false;
					f.cursor = f.PlayStart();
					break;
				case SCENA_A_IMAGE_SUIVANTE:
					f.StepFrame(1);
					break;
				case SCENA_A_IMAGE_PRECEDENTE:
					f.StepFrame(-1);
					break;
				case SCENA_A_CLE: {
					// La piste ACTIVE de la frise d'abord ; sinon l'entite choisie.
					const NkTimelineTrack *t = f.Track(f.activeTrack);
					if (t != nullptr && t->kind != NkTimelineValueKind::Clips && NkScenaPrefixeCanal(t->property) != nullptr &&
						!(m.scene.SelectionValide() && NkString(m.scene.Nom(m.scene.selection)) != t->object)) {
						if (m.PoserCle(t->id, f.cursor)) {
							m.Annoncer(NkString::Format("Clé : %s / %s à %.2f s", t->object.CStr(), t->property.CStr(),
														static_cast<double>(f.cursor))
										   .CStr());
						}
						break;
					}
					Executer(SCENA_A_CLES_ENTITE);
					break;
				}
				case SCENA_A_CLES_ENTITE: {
					if (!m.scene.SelectionValide()) {
						m.Annoncer("Poser une clé : choisissez une entité (vue, Outliner) ou une piste de la frise", 2);
						break;
					}
					const NkString nom(m.scene.Nom(m.scene.selection));
					if (!m.APisteTransform(nom.CStr())) {
						(void)m.AjouterPisteTransform(nom.CStr());
					}
					if (m.PoserCles(nom.CStr(), f.cursor)) {
						m.Annoncer(NkString::Format("Clés : %s (position, rotation, échelle) à %.2f s", nom.CStr(),
													static_cast<double>(f.cursor))
									   .CStr());
					}
					break;
				}
				case SCENA_A_PISTE_ENTITE:
					if (m.scene.SelectionValide()) {
						(void)m.AjouterPisteTransform(m.scene.Nom(m.scene.selection));
					} else {
						m.Annoncer("Piste de transformation : choisissez d'abord une entité", 2);
					}
					break;
				case SCENA_A_PISTE_PLANS:
					(void)m.AjouterPistePlans();
					break;
				case SCENA_A_RENDRE:
					if (mHote.demandeRendu != nullptr) {
						*mHote.demandeRendu = true;
					}
					break;
				case SCENA_A_ANNULER_RENDU:
					if (mHote.rendu != nullptr && mHote.rendu->actif) {
						mHote.rendu->annuler = true;
					}
					break;
				case SCENA_A_VUE_CAMERA:
					m.vueCamera = !m.vueCamera;
					if (m.vueCamera && !m.CameraDuPlan(f.cursor).IsValid()) {
						m.Annoncer("Vue caméra : aucun plan à cet instant — la vue reste libre jusqu'au prochain plan", 2);
					}
					break;
				case SCENA_A_CADRER:
					if (m.scene.SelectionValide()) {
						m.scene.Cadrer(m.scene.selection);
					}
					break;
				case SCENA_A_DESELECTIONNER:
					m.scene.Deselectionner();
					f.ClearSelection();
					m.choix = NkScenaChoix::Sequence;
					break;
				case SCENA_A_VOIR_OUTLINER:
					plan.voirOutliner = !plan.voirOutliner;
					break;
				case SCENA_A_VOIR_DETAILS:
					plan.voirDetails = !plan.voirDetails;
					break;
				case SCENA_A_VOIR_TIROIR:
					plan.voirTiroir = !plan.voirTiroir;
					break;
				case SCENA_A_TIROIR_FRISE:
				case SCENA_A_TIROIR_CONTENU:
				case SCENA_A_TIROIR_JOURNAL:
				case SCENA_A_TIROIR_TERMINAL:
					mOngletTiroir = a - SCENA_A_TIROIR_FRISE;
					plan.voirTiroir = true;
					break;
				case SCENA_A_THEME:
					PoserTheme(!Clair());
					break;
				case SCENA_A_GRILLE:
					mVoirGrille = !mVoirGrille;
					break;
				case SCENA_A_DISPOSITION: {
					const NkFamillePlan neuf;
					plan.largeurOutliner = neuf.largeurOutliner;
					plan.largeurDetails = neuf.largeurDetails;
					plan.hauteurTiroir = 300.f;
					plan.voirOutliner = plan.voirDetails = plan.voirTiroir = true;
					plan.voirPlacer = false;
					break;
				}
				default:
					break;
			}
		}

		// =====================================================================
		// LES RACCOURCIS
		// =====================================================================
		bool NkScenaInterface::ChampAuClavier() const noexcept {
			return const_cast<NkScenaInterface *>(this)->Gui().inputId != nkgui::NKGUI_ID_NONE || !mOutliner.Libre() ||
				   !mDetails.Libre() || mJournal.rechercheFocus || mContenu.modele.searchFocused || mTerminal.AFocus();
		}

		void NkScenaInterface::Raccourcis(NkFamilleCtx &c) {
			const nkgui::NkGuiInput &in = c.ctx.input;
			if (in.KeyPressed(NkGuiKey::Escape) && menus.Ouvert()) {
				menus.Fermer();
				return;
			}
			if (ChampAuClavier()) {
				return;
			}
			struct NkTouche {
					NkGuiKey touche;
					int32 action;
			};
			if (in.ctrlDown) {
				if (in.KeyPressed(NkGuiKey::Z)) {
					Executer(in.shiftDown ? SCENA_A_REFAIRE : SCENA_A_ANNULER);
					return;
				}
				static const NkTouche kCtrl[] = {
					{NkGuiKey::Y, SCENA_A_REFAIRE},	 {NkGuiKey::N, SCENA_A_NOUVELLE}, {NkGuiKey::O, SCENA_A_OUVRIR_SEQUENCE},
					{NkGuiKey::S, SCENA_A_ENREGISTRER}, {NkGuiKey::C, SCENA_A_COPIER},	 {NkGuiKey::V, SCENA_A_COLLER},
					{NkGuiKey::R, SCENA_A_RENDRE},	 {NkGuiKey::Q, SCENA_A_QUITTER},
				};
				for (const NkTouche &t : kCtrl) {
					if (in.KeyPressed(t.touche)) {
						Executer(t.action);
					}
				}
				return;
			}
			if (in.KeyPressed(NkGuiKey::Space)) {
				Executer(M().frise.playing ? SCENA_A_PAUSE : SCENA_A_JOUER);
			}
			static const NkTouche kSimples[] = {
				{NkGuiKey::I, SCENA_A_CLE},
				{NkGuiKey::Delete, SCENA_A_SUPPRIMER},
				{NkGuiKey::Right, SCENA_A_IMAGE_SUIVANTE},
				{NkGuiKey::Left, SCENA_A_IMAGE_PRECEDENTE},
				{NkGuiKey::Home, SCENA_A_STOP},
				{NkGuiKey::F, SCENA_A_CADRER},
				{NkGuiKey::Num0, SCENA_A_VUE_CAMERA},
				{NkGuiKey::Q, SCENA_A_OUTIL + 0},
				{NkGuiKey::W, SCENA_A_OUTIL + 1},
				{NkGuiKey::E, SCENA_A_OUTIL + 2},
				{NkGuiKey::R, SCENA_A_OUTIL + 3},
			};
			for (const NkTouche &t : kSimples) {
				if (in.KeyPressed(t.touche)) {
					Executer(t.action);
				}
			}
		}

	} // namespace nkscena
} // namespace nkentseu
