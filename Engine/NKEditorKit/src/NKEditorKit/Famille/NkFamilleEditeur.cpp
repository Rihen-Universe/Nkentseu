// -----------------------------------------------------------------------------
// @File    NkFamilleEditeur.cpp
// @Brief   La trame d'un editeur de la famille (voir NkFamilleEditeur.h).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NKEditorKit/Famille/NkFamilleEditeur.h"
#include "NKEditorKit/NkThemeToGui.h"

namespace nkentseu {
	namespace editorkit {

		using nkgui::NkRect;

		NkFamilleEditeur::NkFamilleEditeur() noexcept : theme(NkTheme::Dark()) {
			pal = NkFamillePaletteDe(theme);
		}

		NkFamilleEditeur::~NkFamilleEditeur() {
			Terminer();
		}

		bool NkFamilleEditeur::InitialiserGui(int32 largeur, int32 hauteur, float32 corpsPx) {
			if (mPret) {
				return true;
			}
			if (!mCtx.Init(largeur, hauteur)) {
				return false;
			}
			nkgui::SetCurrentContext(&mCtx);
			// LES POLICES DE LA FAMILLE (NkCanvasGuiApp::LoadFonts) : une texId
			// DISTINCTE par police -- deux polices sur la meme texId s'ecrasent.
			mPolice.texId = 0x4E4B4654u;
			mPetite.texId = 0x4E4B4655u;
			mMono.texId = 0x4E4B5445u;
			if (!mPolice.LoadEmbedded(NkEmbeddedFontId::DroidSans, corpsPx)) {
				(void)mPolice.LoadEmbedded(NkEmbeddedFontId::ProggyClean, corpsPx);
			}
			(void)mPetite.LoadEmbedded(NkEmbeddedFontId::DroidSans, corpsPx * 0.78f);
			if (!mMono.LoadEmbedded(NkEmbeddedFontId::DejaVuSansMono, corpsPx, false)) {
				(void)mMono.LoadEmbedded(NkEmbeddedFontId::Cousine, corpsPx, false);
			}
			mCtx.font = &mPolice;
			mCtx.codeFont = PoliceMono();
			mPret = true;
			PoserTheme(mClair);
			return true;
		}

		void NkFamilleEditeur::Terminer() {
			if (mPret) {
				mCtx.Shutdown();
				mPret = false;
			}
		}

		int32 NkFamilleEditeur::Polices(nkgui::NkGuiFont **sortie, int32 maximum) noexcept {
			nkgui::NkGuiFont *p[3] = {&mPolice, &mPetite, &mMono};
			int32 n = 0;
			for (int32 k = 0; k < 3 && n < maximum; ++k) {
				if (p[k]->Valid() && p[k]->pixels != nullptr) {
					sortie[n++] = p[k];
				}
			}
			return n;
		}

		void NkFamilleEditeur::PoserTheme(bool clair) {
			mClair = clair;
			theme = clair ? NkTheme::Light() : NkTheme::Dark();
			pal = NkFamillePaletteDe(theme);
			if (mPret) {
				NkThemeVersGui(mCtx, theme);
				// La geometrie de la famille (coins de 2 px, rangees denses).
				mCtx.theme.rounding = 2.f;
				mCtx.theme.roundingSmall = 2.f;
				mCtx.theme.framePadX = 6.f;
				mCtx.theme.framePadY = 3.f;
			}
		}

		void NkFamilleEditeur::LireEvenement(const NkEvent &e) noexcept {
			if (mPret) {
				(void)mEntree.Lire(mCtx.input, e);
			}
		}

		// =====================================================================
		// LA TRAME : l'ordre d'UnkenyEditor (NkEditeurDessinerTrame)
		// =====================================================================
		const nkgui::NkGuiDrawList &NkFamilleEditeur::Trame(float32 dt, int32 W, int32 H, bool agrandie) {
			mFusion.Reset();
			if (!mPret || W <= 0 || H <= 0) {
				return mFusion;
			}
			dt = dt > 0.f ? dt : 1.f / 60.f;
			temps += dt;
			mTempsIps += dt;
			++mTramesIps;
			if (mTempsIps >= 0.5f) {
				ips = static_cast<float32>(mTramesIps) / mTempsIps;
				mTempsIps = 0.f;
				mTramesIps = 0;
			}
			nkgui::SetCurrentContext(&mCtx);
			mCtx.viewW = W;
			mCtx.viewH = H;
			mCtx.BeginFrame(dt);
			NkFamillePlanifier(plan, static_cast<float32>(W), static_cast<float32>(H));
			NkFamilleCtx c{mCtx, theme, pal, &mPolice, &mPetite};
			nkgui::NkGuiInput &in = mCtx.input;
			auto &dl = mCtx.dl;
			dl.AddRectFilled(plan.ecran, pal.fond);
			fenetre.agrandie = agrandie;
			AvantCorps(c);

			// ── 0. Les bords de la fenetre, avant tout, avec l'entree reelle ──
			NkFamilleBordsFenetre(c, plan, fenetre);

			// ── 1. Le corps, gestes neutralises si un menu est ouvert ─────────
			int32 menuDebut = menus.menu;
			const NkFamilleGestes vrais = NkFamilleSauverGestes(in);
			const bool modale = Modale();
			if (modale) {
				menus.Fermer();
				menuDebut = -1;
				NkFamilleNeutraliserGestes(in, true);
			}
			if (menuDebut >= 0 && vrais.clic[1] && !menus.Contient(vrais.position)) {
				menus.Fermer();
				menuDebut = -1;
			}
			if (menuDebut >= 0) {
				NkFamilleNeutraliserGestes(in, menus.Contient(vrais.position));
			}
			PeindreCorps(c);
			(void)NkFamilleCloisons(c, plan);
			PeindreBarreOutils(c);
			PeindreStatut(c);
			PeindreOnglets(c);

			// ── 2. La barre de titre et les menus, avec l'entree reelle ──────
			if (!modale) {
				NkFamilleRendreGestes(in, vrais);
			}
			int32 nMenus = 0;
			const char *const *noms = MenusBarre(nMenus);
			const NkString titre = Titre();
			NkFamilleTitre t;
			t.menus = noms;
			t.nbMenus = nMenus;
			t.titre = titre.CStr();
			t.logo = &LogoRappel;
			t.logoUser = this;
			const int32 menuBarre = menus.menu >= 0 && menus.menu < nMenus ? menus.menu : -1;
			const bool autre = menus.menu >= nMenus;
			NkRect ancre;
			const int32 aOuvrir = NkFamilleBarreTitre(c, plan, t, menuBarre, autre, ancre, fenetre);
			if (aOuvrir >= 0) {
				if (menuBarre >= 0 && aOuvrir != menuBarre) {
					menus.menu = aOuvrir; // le survol d'un voisin remplace le menu ouvert
					menus.ancre = ancre;
					menus.sousMenu = -1;
				} else {
					menus.Ouvrir(aOuvrir, ancre);
				}
			}
			const int32 action = NkFamilleDessinerMenu(c, plan.ecran, menus, menuDebut, &RemplirRappel, this);
			if (action != 0) {
				Executer(action);
			}

			// ── 3. La fenetre modale par-dessus tout, ou les raccourcis ──────
			if (modale) {
				NkFamilleRendreGestes(in, vrais);
				PeindreModale(c);
			} else {
				Raccourcis(c);
			}
			if (!fenetre.agrandie) {
				dl.AddRect(plan.ecran, pal.bord, 1.f);
			}
			mCtx.EndFrame();
			mFusion.Append(mCtx.dl);
			mFusion.Append(mCtx.dlOverlay);
			mEntree.FinDeTrame(mCtx.input);
			return mFusion;
		}

		void NkFamilleEditeur::AppliquerFenetre(NkWindow &f) {
			nkgui::NkGuiInput &in = mCtx.input;
			if (fenetre.fermerDemande) {
				fenetre.fermerDemande = false;
				Fermer();
			}
			if (fenetre.reduireDemande) {
				fenetre.reduireDemande = false;
				f.Minimize();
			}
			if (fenetre.agrandirDemande) {
				fenetre.agrandirDemande = false;
				if (f.IsMaximized()) {
					f.Restore();
				} else {
					f.Maximize();
				}
			}
			if (fenetre.deplacerDemande) {
				fenetre.deplacerDemande = false;
				// TIRER UNE FENETRE AGRANDIE LA RESTAURE, sous le curseur.
				if (f.IsMaximized()) {
					const nkgui::NkVec2 souris = in.mousePos;
					f.Restore();
					const math::NkVec2u taille = f.GetSize();
					const int32 nx = static_cast<int32>(souris.x - static_cast<float32>(taille.x) * fenetre.deplacerFractionX);
					const int32 ny = static_cast<int32>(souris.y - 12.f);
					f.SetPosition(nx < 0 ? 0 : nx, ny < 0 ? 0 : ny);
				}
				f.BeginDragMove();
				in.mouseDown[0] = false; // la boucle modale a mange le relachement
			}
			if (fenetre.redimDemande >= 0) {
				const NkWindow::NkResizeEdge bord = static_cast<NkWindow::NkResizeEdge>(fenetre.redimDemande);
				fenetre.redimDemande = -1;
				f.BeginResize(bord);
				in.mouseDown[0] = false;
			}
			f.SetCursor(Curseur());
		}

		// =====================================================================
		// LES COMPORTEMENTS PAR DEFAUT
		// =====================================================================
		void NkFamilleEditeur::PeindreStatut(NkFamilleCtx &c) {
			const NkString compteurs = NkString::Format("%.0f ips", static_cast<double>(ips));
			NkFamilleBarreEtat(c, plan.statut, NkFamilleEtatJeu::Edition, "", compteurs.CStr());
		}

		void NkFamilleEditeur::PeindreOnglets(NkFamilleCtx &c) {
			// Pas d'onglet : la bande reste peinte (le logo la couvre a gauche).
			auto &dl = c.ctx.dl;
			dl.AddRectFilled(plan.barreOnglets, pal.fond);
			dl.AddRectFilled(NkRect{plan.barreOnglets.x, plan.barreOnglets.y + plan.barreOnglets.h - 1.f, plan.barreOnglets.w, 1.f},
							 pal.bord);
		}

		const char *const *NkFamilleEditeur::MenusBarre(int32 &nombre) const {
			static const char *const kMenus[4] = {"Fichier", "Édition", "Fenêtre", "Aide"};
			nombre = 4;
			return kMenus;
		}

		void NkFamilleEditeur::PeindreLogo(nkgui::NkGuiDrawList &, float32, float32, float32, bool) {
		}

		void NkFamilleEditeur::Raccourcis(NkFamilleCtx &c) {
			if (c.ctx.input.KeyPressed(nkgui::NkGuiKey::Escape) && menus.Ouvert()) {
				menus.Fermer();
			}
		}

		void NkFamilleEditeur::AvantCorps(NkFamilleCtx &) {
		}

		void NkFamilleEditeur::Fermer() {
			mQuitter = true;
		}

		bool NkFamilleEditeur::Modale() const {
			return false;
		}

		void NkFamilleEditeur::PeindreModale(NkFamilleCtx &) {
		}

		void NkFamilleEditeur::RemplirRappel(void *user, int32 menu, NkVector<NkFamilleEntreeMenu> &sortie) {
			static_cast<NkFamilleEditeur *>(user)->Remplir(menu, sortie);
		}

		void NkFamilleEditeur::LogoRappel(nkgui::NkGuiDrawList &dl, float32 x, float32 y, float32 cote, bool fondSombre,
										  void *user) {
			static_cast<NkFamilleEditeur *>(user)->PeindreLogo(dl, x, y, cote, fondSombre);
		}

	} // namespace editorkit
} // namespace nkentseu
