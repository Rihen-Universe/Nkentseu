// -----------------------------------------------------------------------------
// @File    NkFamilleCadre.cpp
// @Brief   Le cadre d'Unreal 5 de la famille (voir NkFamilleCadre.h).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NKEditorKit/Famille/NkFamilleCadre.h"

namespace nkentseu {
	namespace editorkit {

		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;
		using C = NkFamilleCotes;

		namespace {
			float32 Borne(float32 v, float32 lo, float32 hi) noexcept {
				if (hi < lo) {
					return lo;
				}
				return v < lo ? lo : (v > hi ? hi : v);
			}
		} // namespace

		// =====================================================================
		// LE PLAN
		// =====================================================================
		void NkFamillePlanifier(NkFamillePlan &p, float32 W, float32 H) noexcept {
			p.ecran = NkRect{0.f, 0.f, W, H};
			p.barreMenus = NkRect{0.f, 0.f, W, C::kMenus};
			p.barreOnglets = NkRect{0.f, C::kMenus, W, C::kOnglets};
			p.logo = NkRect{0.f, 0.f, C::kMenus + C::kOnglets, C::kMenus + C::kOnglets};
			p.barreOutils = NkRect{0.f, C::kMenus + C::kOnglets, W, C::kOutils};
			p.statut = NkRect{0.f, H - C::kStatut, W, C::kStatut};
			const float32 corpsHaut = C::kMenus + C::kOnglets + C::kOutils;
			const float32 corpsBas = H - C::kStatut;
			const float32 corpsH = corpsBas - corpsHaut > 0.f ? corpsBas - corpsHaut : 0.f;

			float32 tiroirH = 0.f;
			if (p.voirTiroir) {
				tiroirH = Borne(p.hauteurTiroir, 90.f, corpsH - 160.f);
			}
			p.tiroir = NkRect{0.f, corpsBas - tiroirH, W, tiroirH};
			const float32 colonnesBas = corpsBas - tiroirH - (p.voirTiroir ? C::kCloison : 0.f);
			const float32 colonnesH = colonnesBas - corpsHaut > 0.f ? colonnesBas - corpsHaut : 0.f;

			// « Placer des acteurs » A GAUCHE DE TOUT, comme UE5 ; l'Outliner glisse d'autant.
			const float32 P = p.voirPlacer ? Borne(p.largeurPlacer, 170.f, W * 0.25f) : 0.f;
			const float32 x0 = P + (P > 0.f ? C::kCloison : 0.f);
			p.placer = NkRect{0.f, corpsHaut, P, colonnesH};
			const float32 L = p.voirOutliner ? Borne(p.largeurOutliner, 150.f, W * 0.35f) : 0.f;
			const float32 R = p.voirDetails ? Borne(p.largeurDetails, 240.f, W * 0.42f) : 0.f;
			p.outliner = NkRect{x0, corpsHaut, L, colonnesH};
			p.details = NkRect{W - R, corpsHaut, R, colonnesH};
			const float32 vx = x0 + L + (L > 0.f ? C::kCloison : 0.f);
			float32 vw = W - R - (R > 0.f ? C::kCloison : 0.f) - vx;
			if (vw < 0.f) {
				vw = 0.f;
			}
			p.vue = NkRect{vx, corpsHaut, vw, colonnesH};
			p.barreVue = NkRect{vx, corpsHaut, vw, C::kBarreVue};
			const float32 viseurH = colonnesH - C::kBarreVue > 0.f ? colonnesH - C::kBarreVue : 0.f;
			p.viseur = NkRect{vx, corpsHaut + C::kBarreVue, vw, viseurH};
		}

		bool NkFamilleCloisons(NkFamilleCtx &c, NkFamillePlan &p) {
			const nkgui::NkGuiInput &in = c.ctx.input;
			const NkRect cloisons[4] = {
				NkRect{p.outliner.x + p.outliner.w, p.outliner.y, C::kCloison, p.outliner.h},
				NkRect{p.details.x - C::kCloison, p.details.y, C::kCloison, p.details.h},
				NkRect{0.f, p.tiroir.y - C::kCloison, p.ecran.w, C::kCloison},
				NkRect{p.placer.x + p.placer.w, p.placer.y, C::kCloison, p.placer.h},
			};
			const bool visibles[4] = {p.voirOutliner, p.voirDetails, p.voirTiroir, p.voirPlacer};

			if (p.cloisonTenue >= 0) {
				if (!in.mouseDown[0]) {
					p.cloisonTenue = -1;
				} else if (p.cloisonTenue == 0) {
					p.largeurOutliner = in.mousePos.x - p.outliner.x; // le panneau Placer est a sa gauche
				} else if (p.cloisonTenue == 1) {
					p.largeurDetails = p.ecran.w - in.mousePos.x;
				} else if (p.cloisonTenue == 2) {
					p.hauteurTiroir = p.statut.y - in.mousePos.y;
				} else {
					p.largeurPlacer = in.mousePos.x - p.placer.x;
				}
			}
			bool aLaSouris = p.cloisonTenue >= 0;
			for (int32 k = 0; k < 4; ++k) {
				if (!visibles[k]) {
					continue;
				}
				// La zone de saisie deborde de 2 px de chaque cote : 4 px se
				// visent mal, et le trait visible n'a pas a grossir pour autant.
				const bool verticale = k != 2;
				const NkRect prise = verticale ? NkRect{cloisons[k].x - 2.f, cloisons[k].y, cloisons[k].w + 4.f, cloisons[k].h}
											   : NkRect{cloisons[k].x, cloisons[k].y - 2.f, cloisons[k].w, cloisons[k].h + 4.f};
				const bool survol = NkFamilleDans(prise, in.mousePos);
				const bool tenue = p.cloisonTenue == k;
				c.ctx.dl.AddRectFilled(cloisons[k], (survol || tenue) ? c.pal.accent : c.pal.fond);
				if (survol || tenue) {
					c.ctx.wantCursor = verticale ? nkgui::NkGuiCursor::ResizeEW : nkgui::NkGuiCursor::ResizeNS;
					aLaSouris = true;
				}
				if (survol && in.mouseClicked[0] && p.cloisonTenue < 0) {
					p.cloisonTenue = k;
				}
			}
			return aLaSouris;
		}

		// =====================================================================
		// LA FENETRE SANS CADRE
		// =====================================================================
		void NkFamilleBordsFenetre(NkFamilleCtx &c, const NkFamillePlan &plan, NkFamilleFenetre &f) {
			// Agrandie, la fenetre n'a pas de bord a saisir : c'est la regle de
			// toutes les fenetres du systeme.
			if (f.agrandie) {
				return;
			}
			nkgui::NkGuiInput &in = c.ctx.input;
			const float32 W = plan.ecran.w;
			const float32 H = plan.ecran.h;
			const float32 bande = 5.f;
			const NkVec2 p = in.mousePos;
			if (p.x < 0.f || p.y < 0.f || p.x >= W || p.y >= H) {
				return; // la sentinelle « nulle part » n'est pas un bord
			}
			// Masque : 1 gauche, 2 droite, 4 haut, 8 bas -- le code de NkEditorShell.
			int32 bords = 0;
			bords |= p.x < bande ? 1 : 0;
			bords |= p.x >= W - bande ? 2 : 0;
			bords |= p.y < bande ? 4 : 0;
			bords |= p.y >= H - bande ? 8 : 0;
			if (bords == 0) {
				return;
			}
			c.ctx.wantCursor = (bords & 3) != 0 ? nkgui::NkGuiCursor::ResizeEW : nkgui::NkGuiCursor::ResizeNS;
			if (!in.mouseClicked[0]) {
				return;
			}
			// Les valeurs de NkWindow::NkResizeEdge : Left, Right, Top, Bottom,
			// TopLeft, TopRight, BottomLeft, BottomRight.
			int32 bord = -1;
			switch (bords) {
				case 1:     bord = 0; break;
				case 2:     bord = 1; break;
				case 4:     bord = 2; break;
				case 8:     bord = 3; break;
				case 1 | 4: bord = 4; break;
				case 2 | 4: bord = 5; break;
				case 1 | 8: bord = 6; break;
				case 2 | 8: bord = 7; break;
				default:    break;
			}
			if (bord >= 0) {
				f.redimDemande = bord;
				// Le clic est au bord, pas a ce qui est dessous.
				in.mouseClicked[0] = false;
			}
		}

		// =====================================================================
		// LA BARRE DE TITRE
		// =====================================================================
		bool NkFamilleFondSombre(const NkColor &fond) noexcept {
			const float32 l = (0.299f * static_cast<float32>(fond.r) + 0.587f * static_cast<float32>(fond.g) +
							   0.114f * static_cast<float32>(fond.b)) /
							  255.f;
			return l < 0.5f;
		}

		int32 NkFamilleBarreTitre(NkFamilleCtx &c, const NkFamillePlan &plan, const NkFamilleTitre &titre,
								  int32 menuOuvert, bool autreMenuOuvert, NkRect &ancre, NkFamilleFenetre &f) {
			auto &dl = c.ctx.dl;
			const NkRect &b = plan.barreMenus;
			const nkgui::NkGuiInput &in = c.ctx.input;
			dl.AddRectFilled(b, c.pal.fond);
			bool surElement = false;
			int32 aOuvrir = -1;

			// ── Les menus, a gauche, A DROITE du logo ─────────────────────────
			float32 x = b.x + plan.logo.w + 2.f;
			for (int32 i = 0; i < titre.nbMenus; ++i) {
				const char *nom = titre.menus[i];
				const float32 w = NkFamilleLargeur(c.police, nom) + 18.f;
				const NkRect r{x, b.y + 4.f, w, b.h - 8.f};
				const bool survol = NkFamilleDans(r, in.mousePos);
				surElement = surElement || survol;
				const bool ouvert = menuOuvert == i;
				if (ouvert) {
					dl.AddRectFilled(r, c.pal.accent, 2.f);
				} else if (survol) {
					dl.AddRectFilled(r, c.pal.boutonSurvol, 2.f);
				}
				NkFamilleTexteDansBoite(dl, c.police, r, nom, ouvert ? c.pal.surAccent : c.pal.texte);
				if (survol && in.mouseClicked[0]) {
					aOuvrir = i;
					ancre = r;
				} else if (survol && !ouvert && menuOuvert >= 0) {
					// Un menu de la barre est deja ouvert : le survol d'un voisin
					// le remplace, comme dans tout logiciel de bureau.
					aOuvrir = i;
					ancre = r;
				}
				x += w + 2.f;
			}

			// ── LE LOGO, coin haut gauche : un carre sur la ligne des menus ET
			//    celle des onglets. Le carre est peint du fond : le trait bas des
			//    onglets ne passe pas sous lui. ──────────────────────────────────
			dl.AddRectFilled(plan.logo, c.pal.fond);
			if (titre.logo != nullptr) {
				titre.logo(dl, plan.logo.x, plan.logo.y, plan.logo.w, NkFamilleFondSombre(c.pal.fond), titre.logoUser);
			}

			// ── Le titre, au centre ──────────────────────────────────────────
			const float32 ty = b.y + (b.h - NkFamilleHauteurLigne(c.police, 16.f)) * 0.5f;
			NkFamilleTexteCentre(dl, c.police, b.x + b.w * 0.5f, ty, titre.titre, c.pal.attenue);

			// ── Reduire / agrandir / fermer, a droite, traces ────────────────
			const bool aucunMenu = menuOuvert < 0 && !autreMenuOuvert;
			const float32 bw = 44.f;
			for (int32 i = 0; i < 3; ++i) {
				const NkRect r{b.x + b.w - static_cast<float32>(3 - i) * bw, b.y, bw, b.h};
				const bool survol = NkFamilleDans(r, in.mousePos);
				surElement = surElement || survol;
				if (survol) {
					// La fermeture rougit au survol : c'est le seul bouton qui perd
					// quelque chose, et on le voit avant de cliquer.
					dl.AddRectFilled(r, i == 2 ? c.ctx.theme.danger : c.pal.boutonSurvol);
				}
				const NkColor t = survol ? c.pal.texte : c.pal.attenue;
				const float32 cx = r.x + r.w * 0.5f;
				const float32 cy = r.y + r.h * 0.5f;
				if (i == 0) {
					dl.AddLine(NkVec2{cx - 5.f, cy + 0.5f}, NkVec2{cx + 5.f, cy + 0.5f}, t, 1.2f);
				} else if (i == 1) {
					if (f.agrandie) {
						dl.AddRect(NkRect{cx - 3.f, cy - 5.f, 8.f, 8.f}, t, 1.f);
						dl.AddRectFilled(NkRect{cx - 5.f, cy - 3.f, 8.f, 8.f}, survol ? c.pal.boutonSurvol : c.pal.fond);
						dl.AddRect(NkRect{cx - 5.f, cy - 3.f, 8.f, 8.f}, t, 1.f);
					} else {
						dl.AddRect(NkRect{cx - 5.f, cy - 5.f, 10.f, 10.f}, t, 1.f);
					}
				} else {
					dl.AddLine(NkVec2{cx - 5.f, cy - 5.f}, NkVec2{cx + 5.f, cy + 5.f}, t, 1.2f);
					dl.AddLine(NkVec2{cx + 5.f, cy - 5.f}, NkVec2{cx - 5.f, cy + 5.f}, t, 1.2f);
				}
				if (survol && in.mouseClicked[0] && aucunMenu) {
					if (i == 0) {
						f.reduireDemande = true;
					} else if (i == 1) {
						f.agrandirDemande = true;
					} else {
						f.fermerDemande = true;
					}
				}
			}

			// ── La poignee : tout ce qui n'est pas un element ────────────────
			const bool dansBarre = NkFamilleDans(b, in.mousePos) && !NkFamilleDans(plan.logo, in.mousePos);
			if (dansBarre && !surElement && aucunMenu) {
				if (in.mouseDoubleClicked[0]) {
					f.titreArme = false;
					f.agrandirDemande = true;
				} else if (in.mouseClicked[0]) {
					f.titreArme = true;
					f.titreAppui = in.mousePos;
				}
			}
			if (f.titreArme) {
				if (!in.mouseDown[0]) {
					f.titreArme = false;
				} else {
					const float32 dx = in.mousePos.x - f.titreAppui.x;
					const float32 dy = in.mousePos.y - f.titreAppui.y;
					if (dx * dx + dy * dy > 9.f) {
						f.titreArme = false;
						f.deplacerDemande = true;
						const float32 w = b.w > 1.f ? b.w : 1.f;
						f.deplacerFractionX = Borne((f.titreAppui.x - b.x) / w, 0.f, 1.f);
					}
				}
			}
			return aOuvrir;
		}

		// =====================================================================
		// LES ONGLETS DE SCENE
		// =====================================================================
		NkFamilleOngletsResultat NkFamilleOngletsScene(NkFamilleCtx &c, const NkFamillePlan &plan,
													   const NkFamilleOngletScene *onglets, int32 n, int32 actif) {
			NkFamilleOngletsResultat res;
			auto &dl = c.ctx.dl;
			const NkRect &b = plan.barreOnglets;
			const nkgui::NkGuiInput &in = c.ctx.input;
			dl.AddRectFilled(b, c.pal.fond);
			dl.AddRectFilled(NkRect{b.x, b.y + b.h - 1.f, b.w, 1.f}, c.pal.bord);
			// [ ● Scene_01  ✕ ] : le point (ambre) dit « modifiee depuis
			// l'enregistrement » ; la PLACE du point est toujours reservee (un
			// onglet qui s'elargit deplace la croix sous le curseur).
			const float32 point = 14.f;
			const float32 croix = 20.f;
			float32 x = b.x + plan.logo.w + 4.f;
			for (int32 i = 0; i < n; ++i) {
				const NkFamilleOngletScene &o = onglets[i];
				const float32 tw = NkFamilleLargeur(c.police, o.nom);
				const float32 w = 12.f + point + tw + 8.f + croix;
				const NkRect onglet{x, b.y + 3.f, w, b.h - 3.f};
				const bool estActif = i == actif;
				const bool survol = NkFamilleDans(onglet, in.mousePos);
				if (estActif) {
					res.rect = onglet;
					dl.AddRectFilled(onglet, c.pal.panneau, 2.f);
					dl.AddRectFilled(NkRect{onglet.x, onglet.y, onglet.w, 2.f}, c.pal.accent);
				} else if (survol) {
					dl.AddRectFilled(onglet, c.pal.boutonSurvol, 2.f);
				}
				const float32 ty = onglet.y + (onglet.h - NkFamilleHauteurLigne(c.police, 16.f)) * 0.5f;
				if (o.modifie) {
					dl.AddCircleFilled(NkVec2{onglet.x + 12.f + 4.f, onglet.y + onglet.h * 0.5f}, 3.5f, c.pal.selection);
				}
				NkFamilleTexte(dl, c.police, onglet.x + 12.f + point, ty, o.nom, estActif ? c.pal.texte : c.pal.attenue);
				const NkRect fermer{onglet.x + onglet.w - croix - 4.f, onglet.y + (onglet.h - 16.f) * 0.5f, 16.f, 16.f};
				const bool survolX = NkFamilleDans(fermer, in.mousePos);
				if (survolX) {
					dl.AddRectFilled(fermer, c.pal.boutonSurvol, 2.f);
				}
				NkFamilleCroix(dl, fermer, survolX ? c.pal.texte : c.pal.attenue);
				if (in.mouseClicked[0]) {
					if (survolX) {
						res.fermer = i;
					} else if (survol) {
						res.choisi = i;
					}
				}
				x += w + 2.f;
			}
			return res;
		}

		// =====================================================================
		// LA BARRE D'OUTILS
		// =====================================================================
		void NkFamilleFondBarreOutils(NkFamilleCtx &c, const NkRect &b) {
			c.ctx.dl.AddRectFilled(b, c.pal.entete);
			c.ctx.dl.AddRectFilled(NkRect{b.x, b.y + b.h - 1.f, b.w, 1.f}, c.pal.bord);
		}

		float32 NkFamilleTrait(NkFamilleCtx &c, const NkRect &b, float32 x) {
			c.ctx.dl.AddRectFilled(NkRect{x + 6.f, b.y + 7.f, 1.f, b.h - 14.f}, c.pal.bord);
			return x + 13.f;
		}

		float32 NkFamilleLargeurBoutonOutil(NkFamilleCtx &c, const char *texte, bool deroulant) {
			return NkFamilleLargeur(c.police, texte) + (deroulant ? 30.f : 20.f);
		}

		float32 NkFamilleBoutonOutil(NkFamilleCtx &c, const NkRect &b, float32 x, const char *texte, bool deroulant,
									 bool ouvert, bool &clic, NkRect &rect, float32 largeurMin, bool actif) {
			float32 w = NkFamilleLargeurBoutonOutil(c, texte, deroulant);
			w = w < largeurMin ? largeurMin : w;
			const NkRect r{x, b.y + 4.f, w, b.h - 8.f};
			rect = r;
			const NkRect texteR{r.x, r.y, deroulant ? r.w - 10.f : r.w, r.h};
			clic = NkFamilleBouton(c, r, "", ouvert, actif);
			const NkColor col = !actif ? c.pal.attenue : (ouvert ? c.pal.surAccent : c.pal.texte);
			NkFamilleTexteDansBoite(c.ctx.dl, c.police, texteR, texte, col);
			if (deroulant) {
				NkFamilleChevron(c.ctx.dl, r.x + r.w - 11.f, r.y + r.h * 0.5f, ouvert ? c.pal.surAccent : c.pal.attenue);
			}
			return x + w + 4.f;
		}

		int32 NkFamilleBoutonsLecture(NkFamilleCtx &c, const NkRect &b, float32 &x, NkFamilleEtatJeu etat) {
			const float32 cote = b.h - 8.f;
			auto &dl = c.ctx.dl;
			int32 clique = -1;
			for (int32 k = 0; k < 4; ++k) {
				const NkRect r{x, b.y + 4.f, cote, cote};
				const bool enfonce = (k == 0 && etat == NkFamilleEtatJeu::Jeu) || (k == 1 && etat == NkFamilleEtatJeu::Pause);
				const bool actif = !(k == 2 && etat == NkFamilleEtatJeu::Edition);
				const bool clic = NkFamilleBouton(c, r, "", enfonce, actif);
				const NkColor g = enfonce ? c.pal.surAccent : (actif ? c.pal.texte : c.pal.attenue);
				const float32 cx = r.x + r.w * 0.5f;
				const float32 cy = r.y + r.h * 0.5f;
				switch (k) {
					case 0: // Jouer : un triangle
						dl.AddTriangleFilled(NkVec2{cx - 4.f, cy - 6.f}, NkVec2{cx - 4.f, cy + 6.f}, NkVec2{cx + 6.f, cy}, g);
						break;
					case 1: // Pause : deux barres
						dl.AddRectFilled(NkRect{cx - 5.f, cy - 6.f, 3.5f, 12.f}, g);
						dl.AddRectFilled(NkRect{cx + 1.5f, cy - 6.f, 3.5f, 12.f}, g);
						break;
					case 2: // Arreter : un carre
						dl.AddRectFilled(NkRect{cx - 5.f, cy - 5.f, 10.f, 10.f}, g);
						break;
					default: // Un pas : triangle + barre
						dl.AddTriangleFilled(NkVec2{cx - 5.f, cy - 6.f}, NkVec2{cx - 5.f, cy + 6.f}, NkVec2{cx + 3.f, cy}, g);
						dl.AddRectFilled(NkRect{cx + 3.5f, cy - 6.f, 2.5f, 12.f}, g);
						break;
				}
				if (clic) {
					clique = k;
				}
				x += cote + 3.f;
			}
			x += 2.f;
			return clique;
		}

		// =====================================================================
		// LA BARRE D'ETAT
		// =====================================================================
		void NkFamilleBarreEtat(NkFamilleCtx &c, const NkRect &b, NkFamilleEtatJeu etat, const char *message,
								const char *compteurs) {
			auto &dl = c.ctx.dl;
			dl.AddRectFilled(b, c.pal.entete);
			dl.AddRectFilled(NkRect{b.x, b.y, b.w, 1.f}, c.pal.bord);
			// L'etat de jeu, en PASTILLE : c'est la chose a ne jamais confondre.
			const char *texteEtat = "ÉDITION";
			NkColor fond = c.pal.bouton;
			NkColor texte = c.pal.texte;
			if (etat == NkFamilleEtatJeu::Jeu) {
				texteEtat = "EN JEU";
				fond = c.pal.accent;
				texte = c.pal.surAccent;
			} else if (etat == NkFamilleEtatJeu::Pause) {
				texteEtat = "EN PAUSE";
				fond = c.pal.selection;
				texte = c.pal.fond;
			}
			const float32 pw = NkFamilleLargeur(c.petite, texteEtat) + 16.f;
			const NkRect pastille{b.x + 6.f, b.y + 3.f, pw, b.h - 6.f};
			dl.AddRectFilled(pastille, fond, 2.f);
			NkFamilleTexteDansBoite(dl, c.petite, pastille, texteEtat, texte);
			const float32 ligneY = b.y + (b.h - NkFamilleHauteurLigne(c.petite, 12.f)) * 0.5f;
			if (message != nullptr && message[0] != '\0') {
				NkFamilleTexte(dl, c.petite, pastille.x + pastille.w + 10.f, ligneY, message, c.pal.texte, b.w * 0.55f);
			}
			if (compteurs != nullptr && compteurs[0] != '\0') {
				NkFamilleTexteADroite(dl, c.petite, b.x + b.w - 10.f, ligneY, compteurs, c.pal.attenue);
			}
		}

		// =====================================================================
		// LES MENUS DEROULANTS
		// =====================================================================
		namespace {
			struct NkListe {
					NkRect cadre{0.f, 0.f, 0.f, 0.f};
					int32 choisie = 0;
					int32 sousSurvole = -1;
					NkRect ligneSous{0.f, 0.f, 0.f, 0.f};
					bool survolSimple = false;
			};

			/// Mesure, place (bornee a l'ecran) et peint une liste de menu en (x, y).
			/// Si elle deborde a droite, elle se pose a gauche de `xRepli` (un
			/// sous-menu passe alors a gauche de son parent).
			NkListe Peindre(NkFamilleCtx &c, const NkRect &ecran, const NkVector<NkFamilleEntreeMenu> &entrees, float32 x,
							float32 y, float32 largeurMin, int32 sousOuvert, float32 xRepli) {
				NkListe res;
				const nkgui::NkGuiInput &in = c.ctx.input;
				auto &dl = c.ctx.dlOverlay;
				const float32 ligneH = NkFamilleHauteurLigne(c.police, 16.f) + 7.f;
				const float32 sepH = 7.f;
				float32 largeur = largeurMin > 190.f ? largeurMin : 190.f;
				float32 hauteur = 8.f;
				for (uint32 i = 0; i < entrees.Size(); ++i) {
					const NkFamilleEntreeMenu &e = entrees[i];
					if (e.separateur) {
						hauteur += sepH;
						continue;
					}
					const float32 w =
						NkFamilleLargeur(c.police, e.libelle.CStr()) + NkFamilleLargeur(c.petite, e.raccourci) + 64.f;
					largeur = w > largeur ? w : largeur;
					hauteur += ligneH;
				}
				if (x + largeur > ecran.w - 2.f) {
					x = xRepli >= 0.f ? xRepli - largeur : ecran.w - 2.f - largeur;
				}
				if (y + hauteur > ecran.h - 2.f) {
					y = ecran.h - 2.f - hauteur;
				}
				x = x < 0.f ? 0.f : x;
				y = y < 0.f ? 0.f : y;
				const NkRect cadre{x, y, largeur, hauteur};
				res.cadre = cadre;

				dl.AddRectFilled(NkRect{cadre.x + 3.f, cadre.y + 4.f, cadre.w, cadre.h}, NkColor{0, 0, 0, 90}, 3.f);
				dl.AddRectFilled(cadre, c.pal.entete, 2.f);
				dl.AddRect(cadre, c.pal.bord, 1.f, 2.f);

				float32 ly = cadre.y + 4.f;
				for (uint32 i = 0; i < entrees.Size(); ++i) {
					const NkFamilleEntreeMenu &e = entrees[i];
					if (e.separateur) {
						dl.AddRectFilled(NkRect{cadre.x + 8.f, ly + sepH * 0.5f, cadre.w - 16.f, 1.f}, c.pal.bord);
						ly += sepH;
						continue;
					}
					const NkRect r{cadre.x + 3.f, ly, cadre.w - 6.f, ligneH};
					const bool aSous = e.sousMenu >= 0;
					const bool cliquable = e.actif && (e.action != 0 || aSous);
					const bool survol = cliquable && NkFamilleDans(r, in.mousePos);
					const bool eclairee = survol || (aSous && sousOuvert == e.sousMenu);
					if (eclairee) {
						dl.AddRectFilled(r, c.pal.accent, 2.f);
					}
					if (survol && aSous) {
						res.sousSurvole = e.sousMenu;
						res.ligneSous = r;
					} else if (survol) {
						res.survolSimple = true;
					}
					const float32 ty = r.y + (r.h - NkFamilleHauteurLigne(c.police, 16.f)) * 0.5f;
					NkColor t = cliquable ? c.pal.texte : c.pal.attenue;
					if (eclairee) {
						t = c.pal.surAccent;
					}
					if (e.coche) {
						NkFamilleCoche(dl, r.x + 12.f, r.y + r.h * 0.5f, t);
					}
					NkFamilleTexte(dl, c.police, r.x + 26.f, ty, e.libelle.CStr(), t);
					if (aSous) {
						const float32 fx = r.x + r.w - 12.f;
						const float32 fy = r.y + r.h * 0.5f;
						dl.AddTriangleFilled(NkVec2{fx - 2.5f, fy - 4.f}, NkVec2{fx - 2.5f, fy + 4.f}, NkVec2{fx + 2.5f, fy}, t);
					} else if (e.raccourci != nullptr && e.raccourci[0] != '\0') {
						const float32 tyP = r.y + (r.h - NkFamilleHauteurLigne(c.petite, 12.f)) * 0.5f;
						NkFamilleTexteADroite(dl, c.petite, r.x + r.w - 8.f, tyP, e.raccourci,
											  eclairee ? c.pal.surAccent : c.pal.attenue);
					}
					if (survol && !aSous && in.mouseClicked[0]) {
						res.choisie = e.action;
					}
					ly += ligneH;
				}
				return res;
			}
		} // namespace

		int32 NkFamilleDessinerMenu(NkFamilleCtx &c, const NkRect &ecran, NkFamilleMenus &m, int32 menuDebut,
									NkFamilleRemplirMenuFn remplir, void *user) {
			if (m.menu < 0 || remplir == nullptr) {
				m.sousMenu = -1;
				return 0;
			}
			const nkgui::NkGuiInput &in = c.ctx.input;
			NkVector<NkFamilleEntreeMenu> entrees;
			remplir(user, m.menu, entrees);
			const NkListe principal = Peindre(c, ecran, entrees, m.ancre.x, m.ancre.y + m.ancre.h, m.ancre.w, m.sousMenu, -1.f);
			m.rect = principal.cadre;
			// Le sous-menu suit le SURVOL : il s'ouvre sur son entree, et se ferme
			// quand on en survole une autre.
			if (principal.sousSurvole >= 0) {
				m.sousMenu = principal.sousSurvole;
				m.sousLigne = principal.ligneSous;
			} else if (principal.survolSimple) {
				m.sousMenu = -1;
			}
			NkListe sous;
			if (m.sousMenu >= 0) {
				NkVector<NkFamilleEntreeMenu> sousEntrees;
				remplir(user, m.sousMenu, sousEntrees);
				sous = Peindre(c, ecran, sousEntrees, principal.cadre.x + principal.cadre.w - 2.f, m.sousLigne.y - 4.f, 170.f,
							   -1, principal.cadre.x + 2.f);
				m.sousRect = sous.cadre;
			} else {
				m.sousRect = NkRect{0.f, 0.f, 0.f, 0.f};
			}
			const int32 choisie = principal.choisie != 0 ? principal.choisie : sous.choisie;
			if (choisie != 0) {
				m.Fermer();
				return choisie;
			}
			// Un clic HORS du menu le ferme -- sauf s'il vient de s'ouvrir sur ce clic.
			const bool clic = in.mouseClicked[0] || in.mouseClicked[1] || in.mouseClicked[2];
			if (clic && m.menu == menuDebut && !m.Contient(in.mousePos)) {
				m.Fermer();
			}
			return 0;
		}

		// =====================================================================
		// LES GESTES SOUS UN MENU
		// =====================================================================
		NkFamilleGestes NkFamilleSauverGestes(const nkgui::NkGuiInput &in) noexcept {
			NkFamilleGestes g;
			g.position = in.mousePos;
			for (int32 i = 0; i < 3; ++i) {
				g.bas[i] = in.mouseDown[i];
				g.clic[i] = in.mouseClicked[i];
				g.relache[i] = in.mouseReleased[i];
				g.double_[i] = in.mouseDoubleClicked[i];
			}
			g.molette = in.wheel;
			g.moletteH = in.wheelH;
			return g;
		}

		void NkFamilleRendreGestes(nkgui::NkGuiInput &in, const NkFamilleGestes &g) noexcept {
			in.mousePos = g.position;
			for (int32 i = 0; i < 3; ++i) {
				in.mouseDown[i] = g.bas[i];
				in.mouseClicked[i] = g.clic[i];
				in.mouseReleased[i] = g.relache[i];
				in.mouseDoubleClicked[i] = g.double_[i];
			}
			in.wheel = g.molette;
			in.wheelH = g.moletteH;
		}

		void NkFamilleNeutraliserGestes(nkgui::NkGuiInput &in, bool surLeMenu) noexcept {
			for (int32 i = 0; i < 3; ++i) {
				in.mouseDown[i] = false;
				in.mouseClicked[i] = false;
				in.mouseReleased[i] = false;
				in.mouseDoubleClicked[i] = false;
			}
			in.wheel = 0.f;
			in.wheelH = 0.f;
			if (surLeMenu) {
				in.mousePos = NkVec2{-100000.f, -100000.f};
			}
		}

	} // namespace editorkit
} // namespace nkentseu
