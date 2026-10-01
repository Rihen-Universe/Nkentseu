// =============================================================================
// NkTerminalPanneau.cpp — le panneau terminal partage : onglets, menu « + »,
// en-tete, grille, menu contextuel, renommage. Voir l'en-tete.
// =============================================================================
#include "NKEditorKit/Terminal/NkTerminalPanneau.h"
#include "NKEditorKit/Components/NkGuiComponentPaint.h"
#include "NKEditorKit/Components/NkTabStripModel.h"
#include "NKEditorKit/NkEditorSurface.h"
#include "NKEditorKit/NkEditorTextField.h"
#include "NKEditorKit/NkThemeToGui.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editorkit {

		using nkgui::NkColor;
		using nkgui::NkGuiContext;
		using nkgui::NkGuiDrawList;
		using nkgui::NkGuiFont;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {
			NkColor Alpha(NkColor c, uint8 a) {
				c.a = a;
				return c;
			}

			void Texte(NkGuiDrawList &dl, const NkGuiFont *f, float32 x, float32 haut, float32 h, const char *s,
					   NkColor c) {
				if (!f || !f->Valid() || !s)
					return;
				dl.AddText(f->Face(), f->TexId(), {x, haut + (h - f->LineHeight()) * 0.5f + f->Ascent()}, s, c);
			}

			/// Coupe un chemin par la GAUCHE pour qu'il tienne dans `w`.
			NkString Couper(const NkGuiFont *f, const NkString &s, float32 w) {
				if (!f || f->MeasureWidth(s.CStr()) <= w)
					return s;
				const char *p = s.CStr();
				while (*p) {
					++p;
					while ((static_cast<unsigned char>(*p) & 0xC0) == 0x80)
						++p;
					const NkString e = NkString("…") + p;
					if (f->MeasureWidth(e.CStr()) <= w)
						return e;
				}
				return NkString("…");
			}
		} // namespace

		// =====================================================================
		//  SESSION
		// =====================================================================
		bool NkTerminalSession::Demarrer(int16 cols, int16 rows) {
			if (demarre)
				return !echec;
			demarre = true;
			echec = false;
			finAnnoncee = false;
			ecran.Resize(cols, rows);
			if (!pty.Start(shell.commande, cols, rows, dossierDepart)) {
				echec = true;
				// L'echec se LIT dans la grille, en couleur d'erreur, au lieu d'un
				// terminal vide qui laisse chercher.
				const NkString m = NkString("\x1b[31m") + pty.Erreur() + "\x1b[0m\r\n\x1b[90mEntree pour reessayer.\x1b[0m\r\n";
				ecran.Feed(m.CStr(), m.Size());
				return false;
			}
			if (!aTaper.Empty()) {
				pty.Write(aTaper.CStr(), aTaper.Size());
				aTaper = NkString();
			}
			return true;
		}

		bool NkTerminalSession::Pomper() {
			if (!demarre || echec)
				return false;
			tampon.Clear();
			pty.Drain(tampon);
			const bool neuf = tampon.Size() > 0;
			if (neuf)
				ecran.Feed(tampon.Data(), tampon.Size());
			// Les questions du shell (position du curseur, identite) : la reponse
			// repart AUSSITOT, sinon fish et PSReadLine attendent.
			tampon.Clear();
			ecran.PrendreReponses(tampon);
			if (tampon.Size() > 0)
				pty.Write(tampon.Data(), tampon.Size());
			if (!finAnnoncee && !pty.Running()) {
				finAnnoncee = true;
				char m[160];
				std::snprintf(m, sizeof(m), "\r\n\x1b[90m[processus termine, code %d] Entree pour relancer.\x1b[0m\r\n",
							  static_cast<int>(pty.CodeSortie()));
				ecran.Feed(m, std::strlen(m));
			}
			return neuf;
		}

		void NkTerminalSession::Envoyer(const char *s, usize n) {
			if (demarre && !echec)
				pty.Write(s, n);
		}

		void NkTerminalSession::Redimensionner(int16 cols, int16 rows) {
			if (cols == ecran.Cols() && rows == ecran.Rows())
				return;
			ecran.Resize(cols, rows);
			if (demarre && !echec)
				pty.Resize(cols, rows);
		}

		// =====================================================================
		//  PANNEAU
		// =====================================================================
		NkTerminalPanneau::NkTerminalPanneau() = default;

		NkTerminalPanneau::~NkTerminalPanneau() {
			FermerTout();
		}

		void NkTerminalPanneau::Decouvrir(bool avecWsl) {
			NkTerminalDecouvrirShells(mShells, avecWsl);
			mShellsConnus = true;
			mWslConnus = avecWsl;
		}

		const NkVector<NkShellDecouvert> &NkTerminalPanneau::Shells() {
			if (!mShellsConnus)
				Decouvrir(true);
			return mShells;
		}

		int32 NkTerminalPanneau::Ouvrir(int32 indice) {
			if (!NkPty::Disponible())
				return -1;
			const NkVector<NkShellDecouvert> &l = Shells();
			if (l.Empty())
				return OuvrirShell(NkTerminalShellDeRepli());
			if (indice < 0 || indice >= static_cast<int32>(l.Size()))
				indice = 0;
			return OuvrirShell(l[static_cast<usize>(indice)]);
		}

		int32 NkTerminalPanneau::OuvrirShell(const NkShellDecouvert &s) {
			NkTerminalSession *t = new NkTerminalSession();
			t->id = mProchainId++;
			t->shell = s;
			t->titre = s.nom.Empty() ? NkString(NkShellGenreNom(s.genre)) : s.nom;
			t->dossierDepart = dossierDepart;
			mSessions.PushBack(t);
			Activer(Nombre() - 1);
			return mActif;
		}

		void NkTerminalPanneau::Fermer(int32 i) {
			if (i < 0 || i >= Nombre())
				return;
			NkTerminalSession *t = mSessions[static_cast<usize>(i)];
			t->pty.Stop(); // le shell ET sa descendance
			delete t;
			mSessions.Erase(mSessions.Begin() + i);
			if (mRenomme == i)
				mRenomme = -1;
			if (mActif >= Nombre())
				mActif = Nombre() - 1;
			else if (mActif > i)
				--mActif;
		}

		void NkTerminalPanneau::FermerTout() {
			while (Nombre() > 0)
				Fermer(Nombre() - 1);
			mActif = -1;
		}

		void NkTerminalPanneau::Renommer(int32 i, const NkString &titre) {
			if (NkTerminalSession *t = Session(i)) {
				if (!titre.Empty()) {
					t->titre = titre;
					t->renomme = true;
				}
			}
		}

		void NkTerminalPanneau::Activer(int32 i) {
			if (i < 0 || i >= Nombre())
				return;
			const bool avaitFocus = AFocus();
			if (NkTerminalSession *a = Session(mActif))
				a->vue.focus = false;
			mActif = i;
			mSessions[static_cast<usize>(i)]->vue.focus = avaitFocus;
		}

		void NkTerminalPanneau::Pomper() {
			for (usize i = 0; i < mSessions.Size(); ++i)
				mSessions[i]->Pomper();
		}

		bool NkTerminalPanneau::AFocus() const {
			return mActif >= 0 && mActif < static_cast<int32>(mSessions.Size()) &&
				   mSessions[static_cast<usize>(mActif)]->vue.focus;
		}

		void NkTerminalPanneau::PerdreFocus() {
			for (usize i = 0; i < mSessions.Size(); ++i)
				mSessions[i]->vue.focus = false;
		}

		void NkTerminalPanneau::PrendreFocus() {
			if (NkTerminalSession *t = Session(mActif))
				t->vue.focus = true;
		}

		void NkTerminalPanneau::Taper(const char *texte) {
			if (!texte || !*texte)
				return;
			if (Nombre() == 0 && Ouvrir(-1) < 0)
				return;
			NkTerminalSession *t = Session(mActif);
			if (t->demarre && !t->echec)
				t->Envoyer(texte, std::strlen(texte));
			else
				t->aTaper += texte;
		}

		bool NkTerminalPanneau::LienClique(NkString &fichier, int32 &ligne, int32 &colonne, bool &estUrl) {
			if (!mLien)
				return false;
			mLien = false;
			fichier = mDernierLien.lienFichier;
			ligne = mDernierLien.lienLigne;
			colonne = mDernierLien.lienColonne;
			estUrl = mDernierLien.lienEstUrl;
			return true;
		}

		// ── LES ONGLETS : la bande du KIT, une icone par shell ────────────────
		void NkTerminalPanneau::DessinerOnglets(NkGuiContext &ctx, NkGuiDrawList &dl, const NkRect &bande,
												const NkTheme &theme) {
			NkTabStripModel m;
			for (usize i = 0; i < mSessions.Size(); ++i) {
				NkTabItem it;
				it.id = mSessions[i]->id;
				it.label = mSessions[i]->titre;
				it.closable = true;
				it.infobulle = mSessions[i]->shell.chemin;
				m.tabs.PushBack(it);
			}
			m.active = Session(mActif) ? Session(mActif)->id : 0;
			NkTabStripStyle s;
			s.bandBg = (uint16)NkRole::PanelBg;
			s.border = (uint16)NkRole::Border;
			s.tabBg = (uint16)NkRole::PanelBg;
			s.tabHoverBg = (uint16)NkRole::InputBg;
			s.tabActiveBg = (uint16)NkRole::PanelHeader;
			s.text = (uint16)NkRole::Text;
			s.textMuted = (uint16)NkRole::TextMuted;
			s.accent = (uint16)NkRole::AccentUi;
			// La place de l'icone : le libelle part plus loin, l'onglet s'elargit
			// d'autant. Des METRIQUES du composant, pas de la geometrie refaite ici.
			static NkComponentInstance sInstance(NkTabStripDecl());
			static bool sRegle = false;
			if (!sRegle) {
				sRegle = true;
				sInstance.SetMetric("label_pad_x", 30.f);
				sInstance.SetMetric("tab_pad_x", 62.f);
				sInstance.SetMetric("band_h", bande.h);
			}
			s.values = &sInstance;

			NkComponentInput ci;
			ci.surfaceScale = ctx.scale;
			ci.mouseX = ctx.input.mousePos.x;
			ci.mouseY = ctx.input.mousePos.y;
			const bool joignable = ctx.PointReachable(ctx.input.mousePos) && mRenomme < 0;
			ci.mouseDown = joignable && ctx.input.mouseDown[0];
			ci.mousePressed = joignable && ctx.input.mouseClicked[0];
			ci.mouseReleased = joignable && ctx.input.mouseReleased[0];
			ci.doubleClick = joignable && ctx.input.mouseDoubleClicked[0];
			ci.rightPressed = joignable && ctx.input.mouseClicked[1];
			ci.ctrl = ctx.input.ctrlDown;
			ci.shift = ctx.input.shiftDown;
			ci.alt = ctx.input.altDown;

			struct Icones {
					NkTerminalPanneau *p;
					NkGuiDrawList *dl;
			} icones{this, &dl};
			NkTabStripHooks hooks;
			hooks.user = &icones;
			hooks.tabOverlay = [](void *user, NkComponentPaint &pc, int32 index, float32 x, float32 y, float32 w,
								  float32 h) {
				(void)pc;
				(void)w;
				Icones *ic = static_cast<Icones *>(user);
				NkTerminalSession *t = ic->p->Session(index);
				if (!t)
					return;
				const float32 c = 14.f;
				NkTerminalDessinerIcone(*ic->dl, {x + 10.f, y + (h - c) * 0.5f, c, c}, t->shell.genre);
			};
			NkGuiComponentPaint peintre(ctx, theme);
			const NkTabStripResult r = NkDrawTabStrip(peintre, ci, {bande.x, bande.y, bande.w, bande.h}, m, s, hooks);

			// Le « + » : la ou la bande l'a pose (sa largeur occupee le dit).
			const float32 addW = NkTabMetric(s, "add_w");
			mAncrePlus = {bande.x + r.usedW - addW, bande.y, addW, bande.h};
			if (r.addRequested)
				mMenuShells = !mMenuShells;
			if (r.selectionChanged)
				for (int32 i = 0; i < Nombre(); ++i)
					if (mSessions[static_cast<usize>(i)]->id == m.active)
						Activer(i);
			// Double-clic sur un onglet : on le RENOMME, en place (le composant
			// rapporte le rectangle du libelle ; le clavier est a nous).
			if (ci.doubleClick && r.hoveredId != 0) {
				for (usize k = 0; k < r.tabs.Size(); ++k)
					if (r.tabs[k].id == r.hoveredId) {
						mRenomme = static_cast<int32>(k);
						mRenommeRect = {r.tabs[k].labelX - 4.f, r.tabs[k].y + 3.f, r.tabs[k].labelW + 8.f, r.tabs[k].h - 6.f};
						std::snprintf(mRenommeTexte, sizeof(mRenommeTexte), "%s", mSessions[k]->titre.CStr());
						mRenommeArme = false;
						PerdreFocus();
					}
			}
			if (r.closeRequested)
				for (int32 i = 0; i < Nombre(); ++i)
					if (mSessions[static_cast<usize>(i)]->id == r.closeId) {
						Fermer(i);
						break;
					}
			// Le champ de renommage, pose sur le libelle (recalcule a chaque image :
			// le renommage peut venir du menu contextuel, qui ne connait pas l'onglet).
			if (mRenomme >= 0 && mRenomme < static_cast<int32>(r.tabs.Size())) {
				const NkTabRect &tr = r.tabs[static_cast<usize>(mRenomme)];
				mRenommeRect = {tr.labelX - 4.f, tr.y + 3.f, tr.labelW + 8.f, tr.h - 6.f};
			}
			if (mRenomme >= 0 && mRenomme < Nombre()) {
				dl.AddRectFilled(mRenommeRect, NkThemeUnpack(theme.Get(NkRole::InputBg)), 3.f);
				dl.AddRect(mRenommeRect, NkThemeUnpack(theme.Get(NkRole::AccentUi)), 1.f, 3.f);
				NkOverlayFieldStyle fs;
				fs.fond = false;
				fs.bord = false;
				fs.texte = NkThemeUnpack(theme.Get(NkRole::Text));
				NkOverlayTextField(ctx, dl, ctx.font, {mRenommeRect.x + 4.f, mRenommeRect.y, mRenommeRect.w - 8.f, mRenommeRect.h},
								   mRenommeTexte, static_cast<int32>(sizeof(mRenommeTexte)), true, &fs);
				const bool clicAilleurs = mRenommeArme && ctx.input.mouseClicked[0] &&
										  !nkgui::NkGuiRectContains(mRenommeRect, ctx.input.mousePos);
				if (ctx.input.KeyPressed(nkgui::NkGuiKey::Enter) || clicAilleurs) {
					Renommer(mRenomme, NkString(mRenommeTexte));
					mRenomme = -1;
				} else if (ctx.input.KeyPressed(nkgui::NkGuiKey::Escape)) {
					mRenomme = -1;
				}
				mRenommeArme = true;
			} else {
				mRenomme = -1;
			}
		}

		// ── LE MENU « + » : les shells de la machine ──────────────────────────
		void NkTerminalPanneau::DessinerMenuShells(NkGuiContext &ctx, const NkRect &borne) {
			if (!mMenuShells)
				return;
			const NkVector<NkShellDecouvert> &l = Shells();
			const NkGuiFont *f = ctx.font;
			const float32 rangH = 30.f, titreH = 28.f, pad = 6.f;
			const int32 n = static_cast<int32>(l.Size());
			float32 w = 320.f;
			for (int32 i = 0; i < n && f; ++i) {
				const float32 lw = f->MeasureWidth(l[static_cast<usize>(i)].nom.CStr()) + 150.f;
				if (lw > w)
					w = lw;
			}
			if (w > 520.f)
				w = 520.f;
			const float32 h = titreH + n * rangH + 9.f + rangH + pad;
			const NkRect r = NkPlacerPresDeLAncre(mAncrePlus, w, h, borne.x + borne.w, borne.y + borne.h, NkCoteAncre::Dessous);
			NkSurfaceFlottante surf(ctx, r, NkCouche::Menu, NkPriseClavier::Oui);
			NkGuiDrawList &o = surf.dl;
			// Ombre douce, boite, bord : la meme forme que les menus de la famille.
			o.AddRectFilled({r.x + 1.f, r.y + 5.f, r.w + 2.f, r.h + 2.f}, NkColor{0, 0, 0, mPalette.sombre ? (uint8)90 : (uint8)40}, 9.f);
			o.AddRectFilled(r, mPalette.entete, 7.f);
			o.AddRect(r, mPalette.bord, 1.f, 7.f);
			Texte(o, f, r.x + 14.f, r.y + 2.f, titreH, "Nouveau terminal", mPalette.attenue);
			if (n == 0)
				Texte(o, f, r.x + 14.f, r.y + titreH, rangH, "Aucun shell trouve sur cette machine.", mPalette.attenue);
			int32 choisi = -1;
			bool actualiser = false;
			float32 y = r.y + titreH;
			const NkColor accent = mPalette.ansi[4];
			for (int32 i = 0; i < n; ++i, y += rangH) {
				const NkShellDecouvert &s = l[static_cast<usize>(i)];
				const NkRect rang = {r.x + 4.f, y, r.w - 8.f, rangH - 2.f};
				const bool survol = surf.Survole(rang);
				if (survol) {
					o.AddRectFilled(rang, Alpha(accent, 46), 5.f);
					o.AddRectFilled({rang.x, rang.y + 6.f, 3.f, rang.h - 12.f}, accent, 2.f);
				}
				NkTerminalDessinerIcone(o, {rang.x + 10.f, rang.y + (rang.h - 16.f) * 0.5f, 16.f, 16.f}, s.genre);
				Texte(o, f, rang.x + 36.f, rang.y, rang.h, s.nom.CStr(), mPalette.enteteTexte);
				float32 xd = rang.x + 36.f + (f ? f->MeasureWidth(s.nom.CStr()) : 80.f) + 14.f;
				if (i == 0 && f) {
					// La pastille « par defaut » : le premier est celui du « + » rapide.
					const char *pd = "par defaut";
					const float32 pw = f->MeasureWidth(pd) + 12.f;
					o.AddRectFilled({xd, rang.y + 7.f, pw, rang.h - 14.f}, Alpha(accent, 40), 4.f);
					Texte(o, f, xd + 6.f, rang.y, rang.h, pd, accent);
					xd += pw + 10.f;
				}
				const float32 reste = rang.x + rang.w - 10.f - xd;
				if (reste > 40.f && f) {
					const NkString c = Couper(f, s.chemin, reste);
					Texte(o, f, rang.x + rang.w - 10.f - f->MeasureWidth(c.CStr()), rang.y, rang.h, c.CStr(), mPalette.attenue);
				}
				if (survol && surf.Clic(rang))
					choisi = i;
			}
			o.AddRectFilled({r.x + 10.f, y + 4.f, r.w - 20.f, 1.f}, mPalette.bord);
			y += 9.f;
			{
				const NkRect rang = {r.x + 4.f, y, r.w - 8.f, rangH - 2.f};
				const bool survol = surf.Survole(rang);
				if (survol)
					o.AddRectFilled(rang, Alpha(accent, 46), 5.f);
				Texte(o, f, rang.x + 36.f, rang.y, rang.h, "Actualiser la liste des shells", mPalette.attenue);
				if (survol && surf.Clic(rang))
					actualiser = true;
			}
			if (choisi >= 0) {
				mMenuShells = false;
				Ouvrir(choisi);
				PrendreFocus();
			} else if (actualiser) {
				Decouvrir(true);
			} else if (ctx.input.KeyPressed(nkgui::NkGuiKey::Escape) ||
					   (surf.ClicDehors() && !nkgui::NkGuiRectContains(mAncrePlus, ctx.input.mousePos))) {
				mMenuShells = false;
			}
		}

		void NkTerminalPanneau::DessinerVide(NkGuiContext &ctx, NkGuiDrawList &dl, const NkRect &zone, const char *titre,
											 const char *detail, bool bouton) {
			dl.AddRectFilled(zone, mPalette.fond);
			const NkGuiFont *f = ctx.font;
			if (!f || !f->Valid())
				return;
			const float32 cx = zone.x + zone.w * 0.5f;
			float32 y = zone.y + zone.h * 0.32f;
			const float32 ic = 28.f;
			NkTerminalDessinerIcone(dl, {cx - ic * 0.5f, y - ic - 10.f, ic, ic}, NkShellGenre::Autre);
			Texte(dl, f, cx - f->MeasureWidth(titre) * 0.5f, y, f->LineHeight() + 4.f, titre, mPalette.texte);
			y += f->LineHeight() + 6.f;
			Texte(dl, f, cx - f->MeasureWidth(detail) * 0.5f, y, f->LineHeight() + 4.f, detail, mPalette.attenue);
			y += f->LineHeight() + 16.f;
			if (bouton) {
				const NkVector<NkShellDecouvert> &l = Shells();
				const NkString lib = NkString("Ouvrir ") + (l.Empty() ? NkString("un terminal") : l[0].nom);
				const float32 bw = f->MeasureWidth(lib.CStr()) + 48.f, bh = 30.f;
				const NkRect b = {cx - bw * 0.5f, y, bw, bh};
				const bool survol = ctx.InputHits(b);
				const NkColor accent = mPalette.ansi[4];
				dl.AddRectFilled(b, survol ? accent : Alpha(accent, 200), 5.f);
				NkTerminalDessinerIcone(dl, {b.x + 10.f, b.y + 8.f, 14.f, 14.f}, l.Empty() ? NkShellGenre::Autre : l[0].genre);
				Texte(dl, f, b.x + 32.f, b.y, bh, lib.CStr(), NkColor{255, 255, 255, 255});
				if (survol && ctx.ClickIn(b) && ctx.popupDepth == 0) {
					Ouvrir(-1);
					PrendreFocus();
				}
			}
		}

		// ── LE PANNEAU ENTIER ─────────────────────────────────────────────────
		void NkTerminalPanneau::Dessiner(NkGuiContext &ctx, NkGuiDrawList &dl, const NkRect &zone, const NkTheme &theme) {
			mZone = zone;
			mPalette = NkTerminalPaletteDuTheme(theme);
			styleGrille.police = police;
			if (zone.w < 60.f || zone.h < 40.f)
				return;
			if (!NkPty::Disponible()) {
				// Android, iOS, Web : on le DIT, proprement, au lieu de planter.
				NkPty sonde;
				sonde.Start(NkString(), 80, 24);
				DessinerVide(ctx, dl, zone, "Pas de terminal sur cette plateforme", sonde.Erreur().CStr(), false);
				return;
			}
			if (!mDejaAffiche) {
				mDejaAffiche = true;
				if (ouvrirAuPremierAffichage && Nombre() == 0)
					Ouvrir(-1);
			}
			const float32 hOnglets = 30.f, hEntete = 28.f;
			const NkRect bande = {zone.x, zone.y, zone.w, hOnglets};
			DessinerOnglets(ctx, dl, bande, theme);
			const NkRect corps = {zone.x, zone.y + hOnglets, zone.w, zone.h - hOnglets};
			NkTerminalSession *s = Session(mActif);
			if (!s) {
				DessinerVide(ctx, dl, corps, "Aucun terminal ouvert",
							 "Le bouton + ouvre PowerShell, bash, WSL... selon ce que la machine propose.", true);
				DessinerMenuShells(ctx, zone);
				return;
			}

			// L'en-tete : le shell, LE DOSSIER COURANT (annonce par le shell, sinon
			// le dossier de depart), la taille et le moteur.
			const NkRect entete = {zone.x, corps.y, zone.w, hEntete};
			const NkString &dossier = s->ecran.DossierCourant().Empty() ? s->dossierDepart : s->ecran.DossierCourant();
			char droite[96];
			std::snprintf(droite, sizeof(droite), "%s%s · %d x %d", s->echec ? "echec · " : (s->finAnnoncee ? "termine · " : ""),
						  NkPty::NomMoteur(), static_cast<int>(s->ecran.Cols()), static_cast<int>(s->ecran.Rows()));
			NkTerminalDessinerEntete(ctx, dl, entete, mPalette, s->shell.genre, s->shell.nom.CStr(), dossier.CStr(), droite);

			// La grille : sa taille decide celle du shell.
			const NkRect grille = {zone.x, entete.y + hEntete, zone.w, zone.h - hOnglets - hEntete};
			int16 cols = 80, rows = 24;
			NkTerminalTailleGrille(ctx, grille, styleGrille, cols, rows);
			if (!s->demarre)
				s->Demarrer(cols, rows);
			else
				s->Redimensionner(cols, rows);
			s->Pomper();
			const NkTerminalGrilleResultat r =
				NkTerminalDessinerGrille(ctx, dl, grille, s->ecran, s->vue, mPalette, styleGrille);

			// Le clavier : un clic dans la grille le donne, un clic HORS du panneau
			// le rend a l'editeur.
			if (r.clicDedans) {
				s->vue.focus = true;
				ctx.inputId = nkgui::NKGUI_ID_NONE; // aucun champ ne garde Ctrl+V
			} else if (ctx.input.mouseClicked[0] && !nkgui::NkGuiRectContains(zone, ctx.input.mousePos)) {
				PerdreFocus();
			}
			if (r.lienClique) {
				mLien = true;
				mDernierLien = r;
			}
			if (r.menuDemande) {
				mMenuContexte.open = true;
				mMenuContexte.pos = {r.menuX, r.menuY};
				s->vue.focus = true;
			}
			if (s->vue.focus && !mMenuShells && !mMenuContexte.open && mRenomme < 0 && ctx.popupDepth == 0) {
				if ((s->echec || s->finAnnoncee) && ctx.input.KeyPressed(nkgui::NkGuiKey::Enter)) {
					// Relancer le MEME shell, dans le meme onglet.
					s->pty.Stop();
					s->ecran.Clear();
					s->demarre = false;
				} else {
					mSortie.Clear();
					if (NkTerminalClavier(ctx, s->ecran, s->vue, mSortie))
						s->Envoyer(mSortie.Data(), mSortie.Size());
				}
			}

			// Le menu contextuel (clic droit).
			static const char *kItems[] = {"Copier", "Coller", "Tout selectionner", "Effacer l'ecran", "Renommer l'onglet",
										   "Fermer le terminal"};
			const bool actifs[] = {s->vue.AUneSelection(), true, true, true, true, true};
			const bool separe[] = {false, false, true, false, true, false};
			const int32 act = NkCtxMenuDraw(ctx, mMenuContexte, kItems, actifs, 6, nullptr, nullptr, nullptr, nullptr, 0,
											nullptr, nullptr, separe);
			if (act == 0) {
				const NkString t = NkTerminalTexteSelection(s->ecran, s->vue);
				if (!t.Empty())
					ctx.SetClipboard(t.CStr());
			} else if (act == 1) {
				mSortie.Clear();
				NkTerminalPreparerCollage(s->ecran, ctx.GetClipboard(), mSortie);
				s->Envoyer(mSortie.Data(), mSortie.Size());
			} else if (act == 2) {
				NkTerminalToutSelectionner(s->ecran, s->vue);
			} else if (act == 3) {
				// L'historique ET l'ecran ; le shell redessine son invite (Ctrl+L).
				s->ecran.Clear();
				s->Envoyer("\x0c", 1);
			} else if (act == 4) {
				mRenomme = mActif;
				std::snprintf(mRenommeTexte, sizeof(mRenommeTexte), "%s", s->titre.CStr());
				mRenommeArme = false;
			} else if (act == 5) {
				Fermer(mActif);
			}
			DessinerMenuShells(ctx, zone);
		}

	} // namespace editorkit
} // namespace nkentseu
