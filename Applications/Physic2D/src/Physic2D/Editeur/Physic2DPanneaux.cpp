// =============================================================================
// Physic2DPanneaux.cpp — tout ce qui se VOIT dans l'editeur
//
// Chaque panneau est une fonction : il lit l'etat, dessine, et ne modifie
// l'application qu'au travers de ses reponses de widgets (un clic, une valeur
// glissee). Aucun panneau n'en connait un autre.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Physic2D/Editeur/Physic2D.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace nkentseu {

	using namespace physic2d;
	using namespace physic2d::ue5;

	// =========================================================================
	// Tables
	// =========================================================================
	const char *Physic2D::NomOutil(Outil o) {
		switch (o) {
			case Outil::SELECTION:
				return "Sélection (Q) : cliquer un corps, le glisser pour le déplacer et le lancer";
			case Outil::SAISIR:
				return "Saisir (W) : attraper la matière à la main et l'étirer";
			case Outil::COUPER:
				return "Couteau (E) : trancher les liaisons sur le trajet de la souris";
			case Outil::EXPLOSION:
				return "Explosion (R) : onde de choc au point cliqué";
			case Outil::AIMANT:
				return "Aimant (T) : attirer en maintenant (Maj : repousser)";
			case Outil::OBSTACLE:
				return "Obstacle (Y) : tracer une paroi statique (clic simple : une cheville)";
			case Outil::GOMME:
				return "Gomme (G) : effacer particules, corps et obstacles";
			case Outil::EPINGLE:
				return "Épingle (P) : fixer / libérer une particule dans l'espace";
			default:
				return "Placer un acteur";
		}
	}

	NkIcone Physic2D::IconeOutil(Outil o) {
		switch (o) {
			case Outil::SELECTION:
				return NkIcone::NK_SELECTION;
			case Outil::SAISIR:
				return NkIcone::NK_SAISIR;
			case Outil::COUPER:
				return NkIcone::NK_COUPER;
			case Outil::EXPLOSION:
				return NkIcone::NK_EXPLOSION;
			case Outil::AIMANT:
				return NkIcone::NK_AIMANT;
			case Outil::OBSTACLE:
				return NkIcone::NK_OBSTACLE;
			case Outil::GOMME:
				return NkIcone::NK_GOMME;
			case Outil::EPINGLE:
				return NkIcone::NK_EPINGLE;
			default:
				return NkIcone::NK_CUBE_PLUS;
		}
	}

	NkIcone Physic2D::IconeActeur(NkActeur a) {
		switch (a) {
			case NkActeur::NK_A_BALLON:
				return NkIcone::NK_A_BALLON;
			case NkActeur::NK_A_BLOB:
			case NkActeur::NK_A_SLIME:
				return NkIcone::NK_A_BLOB;
			case NkActeur::NK_A_GELEE:
				return NkIcone::NK_A_GELEE;
			case NkActeur::NK_A_CAISSE:
				return NkIcone::NK_A_CAISSE;
			case NkActeur::NK_A_EAU:
			case NkActeur::NK_A_MIEL:
				return NkIcone::NK_A_GOUTTE;
			case NkActeur::NK_A_SABLE:
				return NkIcone::NK_A_SABLE;
			case NkActeur::NK_A_ATOMES_BLOC:
			case NkActeur::NK_A_ATOMES_DISQUE:
				return NkIcone::NK_A_ATOMES;
			case NkActeur::NK_A_TISSU:
				return NkIcone::NK_A_TISSU;
			case NkActeur::NK_A_CORDE:
				return NkIcone::NK_A_CORDE;
			case NkActeur::NK_A_PONT:
				return NkIcone::NK_A_PONT;
			default:
				return NkIcone::NK_CUBE_PLUS;
		}
	}

	NkIcone Physic2D::IconeMateriau(NkMateriau m) {
		switch (m) {
			case NkMateriau::NK_BALLON:
				return NkIcone::NK_A_BALLON;
			case NkMateriau::NK_BLOB:
				return NkIcone::NK_A_BLOB;
			case NkMateriau::NK_GELEE:
				return NkIcone::NK_A_GELEE;
			case NkMateriau::NK_EAU:
			case NkMateriau::NK_MIEL:
				return NkIcone::NK_A_GOUTTE;
			case NkMateriau::NK_SABLE:
				return NkIcone::NK_A_SABLE;
			case NkMateriau::NK_ATOMES:
				return NkIcone::NK_A_ATOMES;
			case NkMateriau::NK_CAISSE:
				return NkIcone::NK_A_CAISSE;
			case NkMateriau::NK_TISSU:
				return NkIcone::NK_A_TISSU;
			case NkMateriau::NK_CORDE:
				return NkIcone::NK_A_CORDE;
			default:
				return NkIcone::NK_CUBE_PLUS;
		}
	}

	// =========================================================================
	// Briques communes
	// =========================================================================
	void Physic2D::EnTetePanneau(const NkRect &r, const char *titre, NkIcone ic) {
		const float32 E = mEchelle;
		const float32 h = 30.f * E;
		mDl.AddRectFilled(NkRect{r.x, r.y, r.w, h}, kEnteteOnglets);
		const float32 w = mUi.LargeurTexte(titre, 13.f * E) + 46.f * E;
		const NkRect onglet{r.x, r.y, w, h};
		mDl.AddRectFilled(onglet, kPanneau, 0.f);
		mDl.AddRectFilled(NkRect{onglet.x, onglet.y, onglet.w, 1.f}, NkColor(70, 70, 70));
		NkDessinerIcone(mDl, ic, NkVec2{onglet.x + 16.f * E, onglet.y + h * 0.5f}, 14.f * E, kTexte);
		mUi.TexteGauche(NkRect{onglet.x + 28.f * E, onglet.y, onglet.w, h}, titre, kTexteClair, 13.f * E, 0.f);
	}

	bool Physic2D::LigneProp(const char *id, float32 &y, const NkRect &zone, const char *libelle, float32 &v, float32 mn,
							 float32 mx, const char *fmt) {
		const float32 E = mEchelle;
		const float32 h = 28.f * E;
		const NkRect ligne{zone.x, y, zone.w, h};
		const float32 col = zone.x + zone.w * 0.44f;
		mUi.TexteGauche(NkRect{zone.x + 18.f * E, y, col - zone.x - 18.f * E, h}, libelle, kTexte, 12.5f * E, 0.f);
		const bool change = mUi.Glisseur(id, NkRect{col, y + 4.f * E, zone.x + zone.w - col - 10.f * E, h - 8.f * E}, v, mn, mx, fmt);
		mDl.AddRectFilled(NkRect{ligne.x, ligne.y + h - 1.f, ligne.w, 1.f}, NkColor(30, 30, 30));
		y += h;
		return change;
	}

	bool Physic2D::LigneCase(const char *id, float32 &y, const NkRect &zone, const char *libelle, bool &v) {
		const float32 E = mEchelle;
		const float32 h = 28.f * E;
		const float32 col = zone.x + zone.w * 0.44f;
		mUi.TexteGauche(NkRect{zone.x + 18.f * E, y, col - zone.x - 18.f * E, h}, libelle, kTexte, 12.5f * E, 0.f);
		const bool change = mUi.Case(id, NkRect{col, y + 4.f * E, 24.f * E, h - 8.f * E}, v);
		mDl.AddRectFilled(NkRect{zone.x, y + h - 1.f, zone.w, 1.f}, NkColor(30, 30, 30));
		y += h;
		return change;
	}

	void Physic2D::LigneTexte(float32 &y, const NkRect &zone, const char *libelle, const char *valeur) {
		const float32 E = mEchelle;
		const float32 h = 26.f * E;
		const float32 col = zone.x + zone.w * 0.44f;
		mUi.TexteGauche(NkRect{zone.x + 18.f * E, y, col - zone.x - 18.f * E, h}, libelle, kTexte, 12.5f * E, 0.f);
		const NkRect champ{col, y + 4.f * E, zone.x + zone.w - col - 10.f * E, h - 8.f * E};
		mDl.AddRectFilled(champ, NkColor(26, 26, 26), 3.f * E);
		mUi.TexteGauche(champ, valeur, kTexteFaible, 12.f * E, 7.f * E);
		mDl.AddRectFilled(NkRect{zone.x, y + h - 1.f, zone.w, 1.f}, NkColor(30, 30, 30));
		y += h;
	}

	// =========================================================================
	// Vue
	// =========================================================================
	void Physic2D::DessinerVue() {
		const NkRect &v = mDisp.vue;
		if (v.w <= 0.f || v.h <= 0.f) {
			return;
		}
		mCam.vue = v;
		mOptions.selectionId = mSelection;
		mDl.PushClipRect(v);
		NkDessinerMonde(mDl, mMonde, mCam, mOptions);
		DessinerSurcouchesVue();
		mDl.PopClipRect();
		if (mEtat == Etat::JEU) {
			mDl.AddRect(v, NkAvecAlpha(kJouer, 170), 2.f * mEchelle);
		} else if (mEtat == Etat::PAUSE) {
			mDl.AddRect(v, NkAvecAlpha(kPauseCouleur, 170), 2.f * mEchelle);
		}
	}

	void Physic2D::DessinerSurcouchesVue() {
		const float32 E = mEchelle;
		const NkRect &v = mDisp.vue;
		const bool dansVue = SourisDansVue() && mPopup == Popup::AUCUN;
		const NkVec2 souris{mEntree.x, mEntree.y};
		const NkV2 w = mCam.Monde(mEntree.x, mEntree.y);

		// --- Retours d'outil ---------------------------------------------------
		if (mFlashExplosion > 0.f) {
			const NkVec2 c = mCam.Ecran(mPosExplosion);
			const float32 r = (1.f - mFlashExplosion) * 220.f * mCam.zoom + 6.f;
			mDl.PushBlend(nkgui::NkGuiBlend::PlusLighter);
			mDl.AddCircleFilled(c, r * 0.6f, NkColor(255, 180, 80, static_cast<uint8>(mFlashExplosion * 120.f)), 28);
			mDl.PopBlend();
			mDl.AddCircle(c, r, NkColor(255, 220, 150, static_cast<uint8>(mFlashExplosion * 255.f)), 3.f * E, 40);
		}
		if (mOutil == Outil::COUPER && mNbTraine > 1) {
			for (int32 i = 1; i < mNbTraine; ++i) {
				const float32 t = static_cast<float32>(i) / static_cast<float32>(mNbTraine);
				mDl.AddLine(mCam.Ecran(mTraineCouteau[i - 1]), mCam.Ecran(mTraineCouteau[i]),
							NkColor(255, 255, 255, static_cast<uint8>(60.f + 195.f * t)), (1.f + 3.f * t) * E);
			}
		}
		if (mMonde.EnSaisie()) {
			const NkVec2 c = mCam.Ecran(mMonde.CibleSaisie());
			mDl.AddCircle(c, 10.f * E, kAccentClair, 2.f * E, 20);
			mDl.AddCircleFilled(c, 3.f * E, kAccentClair, 10);
		}
		if (mOutil == Outil::OBSTACLE && mOutilEnCours) {
			const NkVec2 a = mCam.Ecran(mDepartOutil);
			mDl.AddLine(a, souris, NkColor(140, 150, 170, 200), 20.f * mCam.zoom);
			mDl.AddCircleFilled(a, 10.f * mCam.zoom, NkColor(140, 150, 170, 200), 16);
			mDl.AddCircleFilled(souris, 10.f * mCam.zoom, NkColor(140, 150, 170, 200), 16);
		}
		if (mOutil == Outil::PLACER && mActeur == NkActeur::NK_A_PONT && mOutilEnCours) {
			mDl.AddLine(mCam.Ecran(mDepartOutil), souris, NkAvecAlpha(kSelectionVue, 200), 3.f * E);
		}

		if (dansVue) {
			switch (mOutil) {
				case Outil::GOMME:
					mDl.AddCircle(souris, 22.f * E, NkColor(255, 90, 90, 220), 1.5f * E, 28);
					break;
				case Outil::AIMANT: {
					const float32 r = 260.f * mCam.zoom;
					const float32 pulse = 0.5f + 0.5f * std::sin(mTemps * 6.f);
					const NkColor c = mEntree.shift ? NkColor(255, 120, 90) : NkColor(90, 160, 255);
					if (mOutilEnCours) {
						mDl.AddCircleFilled(souris, r, NkAvecAlpha(c, 26), 48);
					}
					mDl.AddCircle(souris, r * (0.92f + 0.08f * pulse), NkAvecAlpha(c, 160), 1.5f * E, 48);
					break;
				}
				case Outil::SAISIR:
					if (!mMonde.EnSaisie()) {
						mDl.AddCircle(souris, 22.f * E, NkColor(255, 255, 255, 90), 1.f * E, 24);
					}
					break;
				case Outil::PLACER: {
					const NkInfoActeur &info = NkActeurInfo(mActeur);
					const NkColor col(info.couleur[0], info.couleur[1], info.couleur[2], 150);
					if (info.pinceau) {
						mDl.AddCircle(souris, 20.f * mCam.zoom + 2.f, NkAvecAlpha(col, 220), 1.5f * E, 28);
					}
					NkDessinerIcone(mDl, IconeActeur(mActeur), NkVec2{souris.x + 22.f * E, souris.y + 22.f * E}, 22.f * E, col);
					mUi.Texte(souris.x + 36.f * E, souris.y + 14.f * E, info.nom, NkColor(255, 255, 255, 200), 12.f * E);
					break;
				}
				default:
					break;
			}
		}

		// Glisser-deposer depuis "Placer des acteurs".
		if (mGlisseActeur && mEntree.bas[0]) {
			const NkInfoActeur &info = NkActeurInfo(mActeur);
			const NkColor col(info.couleur[0], info.couleur[1], info.couleur[2], 190);
			NkDessinerIcone(mDl, IconeActeur(mActeur), souris, 34.f * E, col);
		}

		// --- Barre de la vue (en haut a gauche, comme dans UE5) ---------------
		const float32 bh = 24.f * E;
		float32 x = v.x + 8.f * E;
		const float32 y = v.y + 6.f * E;
		auto puce = [&](const char *id, const char *texte, NkIcone ic, bool fleche, Popup p) {
			const float32 tw = mUi.LargeurTexte(texte, 12.f * E);
			const float32 lw = tw + (ic != NkIcone::NK_AUCUNE ? 22.f * E : 0.f) + (fleche ? 16.f * E : 0.f) + 16.f * E;
			const NkRect r{x, y, lw, bh};
			const bool survol = mUi.Survol(r);
			mDl.AddRectFilled(r, survol ? NkColor(58, 58, 58, 235) : NkColor(22, 22, 22, 215), 3.f * E);
			float32 cx = x + 8.f * E;
			if (ic != NkIcone::NK_AUCUNE) {
				NkDessinerIcone(mDl, ic, NkVec2{cx + 7.f * E, y + bh * 0.5f}, 14.f * E, kTexte);
				cx += 22.f * E;
			}
			mUi.TexteGauche(NkRect{cx, y, tw + 4.f * E, bh}, texte, kTexteClair, 12.f * E, 0.f);
			if (fleche) {
				NkDessinerIcone(mDl, NkIcone::NK_FLECHE_BAS, NkVec2{x + lw - 11.f * E, y + bh * 0.5f}, 10.f * E, kTexteFaible);
			}
			if (p != Popup::AUCUN && mUi.Clic(r)) {
				OuvrirPopup(mPopup == p ? Popup::AUCUN : p, r);
			}
			(void)id;
			x += lw + 4.f * E;
		};
		puce("vue_ortho", "Orthographique 2D", NkIcone::NK_GRILLE, false, Popup::AUCUN);
		puce("vue_mode", NkModeVueNom(mOptions.mode), NkIcone::NK_OEIL, true, Popup::VUE_MODE);
		puce("vue_afficher", "Afficher", NkIcone::NK_AUCUNE, true, Popup::VUE_AFFICHER);

		// --- Indicateurs (en haut a droite) -----------------------------------
		char txt[96];
		const float32 pas = NkPasGrille(mCam.zoom);
		if (pas >= 100.f) {
			std::snprintf(txt, sizeof(txt), "Grille %.0f m   Zoom %.0f %%   %.0f IPS", static_cast<double>(pas / 100.f),
						  static_cast<double>(mCam.zoom * 100.f), static_cast<double>(mFps));
		} else {
			std::snprintf(txt, sizeof(txt), "Grille %.0f cm   Zoom %.0f %%   %.0f IPS", static_cast<double>(pas),
						  static_cast<double>(mCam.zoom * 100.f), static_cast<double>(mFps));
		}
		const float32 tw = mUi.LargeurTexte(txt, 12.f * E) + 16.f * E;
		const NkRect ind{v.x + v.w - tw - 8.f * E, y, tw, bh};
		mDl.AddRectFilled(ind, NkColor(22, 22, 22, 200), 3.f * E);
		mUi.TexteGauche(ind, txt, kTexte, 12.f * E, 8.f * E);

		// --- Etat de la simulation --------------------------------------------
		if (mEtat != Etat::JEU) {
			const char *msg = mEtat == Etat::PAUSE ? "EN PAUSE  ·  Espace : reprendre  ·  N : un pas"
												   : "ÉDITION  ·  simulation arrêtée  ·  Espace : jouer";
			const float32 mw = mUi.LargeurTexte(msg, 12.5f * E) + 24.f * E;
			const NkRect r{v.x + (v.w - mw) * 0.5f, y + bh + 8.f * E, mw, bh};
			mDl.AddRectFilled(r, NkColor(20, 20, 20, 210), 3.f * E);
			mUi.TexteCentre(r, msg, mEtat == Etat::PAUSE ? kPauseCouleur : kTexte, 12.5f * E);
		}

		// --- Gizmo d'axes + coordonnees (en bas a gauche) ----------------------
		const NkVec2 o{v.x + 30.f * E, v.y + v.h - 30.f * E};
		const float32 L = 22.f * E;
		mDl.AddLine(o, NkVec2{o.x + L, o.y}, kAxeX, 2.f * E);
		mDl.AddTriangleFilled(NkVec2{o.x + L + 6.f * E, o.y}, NkVec2{o.x + L, o.y - 4.f * E}, NkVec2{o.x + L, o.y + 4.f * E}, kAxeX);
		mDl.AddLine(o, NkVec2{o.x, o.y - L}, kAxeY, 2.f * E);
		mDl.AddTriangleFilled(NkVec2{o.x, o.y - L - 6.f * E}, NkVec2{o.x - 4.f * E, o.y - L}, NkVec2{o.x + 4.f * E, o.y - L}, kAxeY);
		mUi.Texte(o.x + L + 9.f * E, o.y - 8.f * E, "X", kAxeX, 11.f * E);
		mUi.Texte(o.x - 4.f * E, o.y - L - 22.f * E, "Y", kAxeY, 11.f * E);
		if (dansVue) {
			std::snprintf(txt, sizeof(txt), "x %.0f cm   y %.0f cm", static_cast<double>(w.x), static_cast<double>(w.y));
			mUi.Texte(o.x + 50.f * E, o.y - 7.f * E, txt, kTexteFaible, 11.5f * E);
		}

		// --- Echelle (en bas a droite) -----------------------------------------
		float32 metre = 100.f;
		while (metre * mCam.zoom < 50.f * E) {
			metre *= 10.f;
		}
		while (metre * mCam.zoom > 220.f * E && metre > 1.f) {
			metre /= 10.f;
		}
		const float32 lp = metre * mCam.zoom;
		const float32 ex = v.x + v.w - lp - 20.f * E;
		const float32 ey = v.y + v.h - 20.f * E;
		mDl.AddLine(NkVec2{ex, ey}, NkVec2{ex + lp, ey}, kTexte, 2.f * E);
		mDl.AddLine(NkVec2{ex, ey - 5.f * E}, NkVec2{ex, ey + 1.f}, kTexte, 2.f * E);
		mDl.AddLine(NkVec2{ex + lp, ey - 5.f * E}, NkVec2{ex + lp, ey + 1.f}, kTexte, 2.f * E);
		if (metre >= 100.f) {
			std::snprintf(txt, sizeof(txt), "%.0f m", static_cast<double>(metre / 100.f));
		} else {
			std::snprintf(txt, sizeof(txt), "%.0f cm", static_cast<double>(metre));
		}
		mUi.Texte(ex + lp * 0.5f - mUi.LargeurTexte(txt, 11.f * E) * 0.5f, ey - 20.f * E, txt, kTexte, 11.f * E);
	}

	// =========================================================================
	// Barre de menus
	// =========================================================================
	void Physic2D::OuvrirPopup(Popup p, const NkRect &declencheur) {
		mPopup = p;
		mDeclencheur = declencheur;
	}

	void Physic2D::DessinerMenu() {
		const float32 E = mEchelle;
		const NkRect &r = mDisp.menu;
		mDl.AddRectFilled(r, kBarreTitre);
		// Logo : un disque sombre cercle de blanc, comme celui d'UE.
		const NkVec2 lc{r.x + 18.f * E, r.y + r.h * 0.5f};
		mDl.AddCircleFilled(lc, 10.f * E, NkColor(235, 235, 235), 24);
		mDl.AddCircleFilled(lc, 8.5f * E, NkColor(14, 14, 14), 24);
		mUi.TexteCentre(NkRect{lc.x - 10.f * E, lc.y - 10.f * E, 20.f * E, 20.f * E}, "N", kTexteClair, 12.f * E);

		struct Entree {
				const char *nom;
				Popup p;
		};
		const Entree entrees[5] = {{"Fichier", Popup::MENU_FICHIER},
								   {"Édition", Popup::MENU_EDITION},
								   {"Fenêtre", Popup::MENU_FENETRE},
								   {"Niveaux", Popup::MENU_NIVEAUX},
								   {"Aide", Popup::MENU_AIDE}};
		float32 x = r.x + 38.f * E;
		const bool menuOuvert = mPopup >= Popup::MENU_FICHIER && mPopup <= Popup::MENU_AIDE;
		for (int32 i = 0; i < 5; ++i) {
			const float32 w = mUi.LargeurTexte(entrees[i].nom, 13.f * E) + 18.f * E;
			const NkRect b{x, r.y + 3.f * E, w, r.h - 6.f * E};
			const bool ouvert = mPopup == entrees[i].p;
			const bool survol = mEntree.Dans(b) && (mUi.Survol(b) || menuOuvert);
			if (ouvert) {
				mDl.AddRectFilled(b, kAccentSombre, 3.f * E);
			} else if (survol) {
				mDl.AddRectFilled(b, NkColor(52, 52, 52), 3.f * E);
			}
			mUi.TexteCentre(b, entrees[i].nom, ouvert || survol ? kTexteClair : kTexte, 13.f * E);
			if (mEntree.presse[0] && mEntree.Dans(b) && (mUi.Survol(b) || menuOuvert)) {
				OuvrirPopup(ouvert ? Popup::AUCUN : entrees[i].p, b);
			} else if (menuOuvert && !ouvert && mEntree.Dans(b)) {
				OuvrirPopup(entrees[i].p, b); // glisser d'un menu a l'autre, comme partout
			}
			x += w + 2.f * E;
		}
		// Titre du projet a droite.
		char titre[96];
		std::snprintf(titre, sizeof(titre), "Physic2D  ·  %s", NkNiveauNom(mNiveau));
		mUi.TexteDroite(r, titre, kTexteFaible, 12.f * E, 14.f * E);
		mDl.AddRectFilled(NkRect{r.x, r.y + r.h - 1.f, r.w, 1.f}, kSeparateur);
	}

	// =========================================================================
	// Barre d'outils
	// =========================================================================
	void Physic2D::DessinerBarreOutils() {
		const float32 E = mEchelle;
		const NkRect &r = mDisp.outils;
		mDl.AddRectFilled(r, kBarreOutils);
		mDl.AddRectFilled(NkRect{r.x, r.y + r.h - 1.f, r.w, 1.f}, kSeparateur);
		const float32 t = 34.f * E;
		const float32 y = r.y + (r.h - t) * 0.5f;
		float32 x = r.x + 10.f * E;

		// Outils de la vue.
		const Outil outils[8] = {Outil::SELECTION, Outil::SAISIR, Outil::COUPER, Outil::EXPLOSION,
								 Outil::AIMANT,	   Outil::OBSTACLE, Outil::GOMME, Outil::EPINGLE};
		for (int32 i = 0; i < 8; ++i) {
			char id[24];
			std::snprintf(id, sizeof(id), "outil_%d", i);
			if (mUi.BoutonOutil(id, NkRect{x, y, t, t}, IconeOutil(outils[i]), mOutil == outils[i], NomOutil(outils[i]))) {
				ChoisirOutil(outils[i]);
			}
			x += t + 3.f * E;
			if (i == 1 || i == 4) {
				mDl.AddRectFilled(NkRect{x + 2.f * E, y + 6.f * E, 1.f, t - 12.f * E}, NkColor(60, 60, 60));
				x += 7.f * E;
			}
		}
		x += 8.f * E;
		mDl.AddRectFilled(NkRect{x, y + 4.f * E, 1.f, t - 8.f * E}, NkColor(60, 60, 60));
		x += 10.f * E;

		// L'acteur courant : un rappel, et un raccourci vers l'outil Placer.
		{
			const NkInfoActeur &info = NkActeurInfo(mActeur);
			const NkColor teinte(info.couleur[0], info.couleur[1], info.couleur[2]);
			char lib[64];
			std::snprintf(lib, sizeof(lib), "%s", info.nom);
			const float32 w = mUi.LargeurTexte(lib, 13.f * E) + 52.f * E;
			if (mUi.Bouton("acteur_courant", NkRect{x, y, w, t}, lib, mOutil == Outil::PLACER, IconeActeur(mActeur), &teinte)) {
				ChoisirActeur(mActeur);
			}
			mUi.InfoBulle("Placer l'acteur choisi (1..9, 0) · clic dans la vue");
			x += w + 8.f * E;
		}

		// Jouer / Pause / Pas / Arreter : au centre, comme la barre de PIE d'UE5.
		const float32 largeurGroupe = 4.f * t + 3.f * 3.f * E;
		float32 cx = r.x + (r.w - largeurGroupe) * 0.5f;
		cx = NkMaxf(cx, x + 12.f * E);
		const NkRect groupe{cx - 6.f * E, y - 2.f * E, largeurGroupe + 12.f * E, t + 4.f * E};
		mDl.AddRectFilled(groupe, NkColor(18, 18, 18), 4.f * E);
		const NkColor vert = kJouer;
		const NkColor rouge = mEtat == Etat::EDITION ? kTexteTresFaible : kArret;
		const NkColor jaune = kPauseCouleur;
		if (mUi.BoutonOutil("jouer", NkRect{cx, y, t, t}, NkIcone::NK_JOUER, mEtat == Etat::JEU, "Jouer (Espace)", &vert)) {
			Jouer();
		}
		cx += t + 3.f * E;
		if (mUi.BoutonOutil("pause", NkRect{cx, y, t, t}, NkIcone::NK_PAUSE, mEtat == Etat::PAUSE, "Pause (Espace)", &jaune)) {
			MettreEnPause();
		}
		cx += t + 3.f * E;
		if (mUi.BoutonOutil("pas", NkRect{cx, y, t, t}, NkIcone::NK_PAS, false, "Avancer d'un pas (N)")) {
			UnPas();
		}
		cx += t + 3.f * E;
		if (mUi.BoutonOutil("arreter", NkRect{cx, y, t, t}, NkIcone::NK_STOP, false, "Arrêter et restaurer le niveau (F5)", &rouge)) {
			Arreter();
		}
		cx += t + 12.f * E;

		// Etat.
		const char *etat = mEtat == Etat::JEU ? "SIMULATION" : (mEtat == Etat::PAUSE ? "PAUSE" : "ÉDITION");
		const NkColor ce = mEtat == Etat::JEU ? kJouer : (mEtat == Etat::PAUSE ? kPauseCouleur : kTexteFaible);
		mDl.AddCircleFilled(NkVec2{cx + 5.f * E, y + t * 0.5f}, 4.f * E, ce, 12);
		mUi.TexteGauche(NkRect{cx + 14.f * E, y, 120.f * E, t}, etat, ce, 12.f * E, 0.f);

		// A droite : reglages du monde + choix du niveau.
		const float32 nw = 250.f * E;
		const NkRect niveau{r.x + r.w - nw - 10.f * E, y, nw, t};
		if (niveau.x > cx + 110.f * E) {
			const bool ouvert = mPopup == Popup::NIVEAU;
			mDl.AddRectFilled(niveau, ouvert ? kAccentSombre : (mUi.Survol(niveau) ? kBoutonSurvol : kBouton), 3.f * E);
			NkDessinerIcone(mDl, NkIcone::NK_NIVEAU, NkVec2{niveau.x + 16.f * E, niveau.y + t * 0.5f}, 16.f * E, kTexte);
			mUi.TexteGauche(NkRect{niveau.x + 30.f * E, niveau.y, niveau.w - 50.f * E, t}, NkNiveauNom(mNiveau), kTexteClair,
							12.5f * E, 0.f);
			NkDessinerIcone(mDl, NkIcone::NK_FLECHE_BAS, NkVec2{niveau.x + niveau.w - 14.f * E, niveau.y + t * 0.5f}, 10.f * E, kTexte);
			if (mUi.Clic(niveau)) {
				OuvrirPopup(ouvert ? Popup::AUCUN : Popup::NIVEAU, niveau);
			}
			const NkRect monde{niveau.x - t - 6.f * E, y, t, t};
			if (monde.x > cx + 110.f * E &&
				mUi.BoutonOutil("monde", monde, NkIcone::NK_ENGRENAGE, mSelection == 0, "Paramètres du monde (désélectionne)")) {
				mSelection = 0;
			}
		}
	}

	// =========================================================================
	// Placer des acteurs
	// =========================================================================
	void Physic2D::DessinerPlacerActeurs() {
		const float32 E = mEchelle;
		const NkRect &r = mDisp.gauche;
		mDl.AddRectFilled(r, kPanneau);
		EnTetePanneau(r, "Placer des acteurs", NkIcone::NK_CUBE_PLUS);

		// Categories.
		float32 y = r.y + 36.f * E;
		float32 x = r.x + 6.f * E;
		const char *cats[4] = {"Tout", NkCategorieNom(NkCategorie::NK_CAT_CORPS_MOUS), NkCategorieNom(NkCategorie::NK_CAT_FLUIDES),
							   NkCategorieNom(NkCategorie::NK_CAT_STRUCTURES)};
		for (int32 i = 0; i < 4; ++i) {
			const float32 w = mUi.LargeurTexte(cats[i], 11.5f * E) + 12.f * E;
			const NkRect b{x, y, w, 22.f * E};
			const bool actif = mCategorie == i - 1;
			const bool survol = mUi.Survol(b);
			mDl.AddRectFilled(b, actif ? kAccentSombre : (survol ? kBoutonSurvol : kPanneauSombre), 11.f * E);
			if (actif) {
				mDl.AddRect(b, kAccent, 1.f, 11.f * E);
			}
			mUi.TexteCentre(b, cats[i], actif ? kTexteClair : kTexte, 11.5f * E);
			if (mUi.Clic(b)) {
				mCategorie = i - 1;
				mDefilActeurs = 0.f;
			}
			x += w + 4.f * E;
		}
		y += 30.f * E;

		// Liste.
		const float32 piedH = 46.f * E;
		const NkRect liste{r.x, y, r.w, r.y + r.h - y - piedH};
		const float32 lh = 46.f * E;
		int32 visibles = 0;
		for (int32 a = 0; a < static_cast<int32>(NkActeur::NK_A_COUNT); ++a) {
			const NkInfoActeur &info = NkActeurInfo(static_cast<NkActeur>(a));
			visibles += (mCategorie < 0 || static_cast<int32>(info.categorie) == mCategorie) ? 1 : 0;
		}
		const float32 maxDefil = NkMaxf(0.f, static_cast<float32>(visibles) * lh - liste.h);
		if (mEntree.Dans(liste) && mEntree.molette != 0.f && mPopup == Popup::AUCUN) {
			mDefilActeurs -= mEntree.molette * lh;
		}
		mDefilActeurs = NkClampf(mDefilActeurs, 0.f, maxDefil);
		mDl.PushClipRect(liste);
		float32 ly = liste.y - mDefilActeurs;
		int32 rang = 0;
		for (int32 a = 0; a < static_cast<int32>(NkActeur::NK_A_COUNT); ++a) {
			const NkActeur act = static_cast<NkActeur>(a);
			const NkInfoActeur &info = NkActeurInfo(act);
			if (mCategorie >= 0 && static_cast<int32>(info.categorie) != mCategorie) {
				continue;
			}
			const NkRect ligne{liste.x, ly, liste.w, lh};
			char id[24];
			std::snprintf(id, sizeof(id), "acteur_%d", a);
			const bool choisi = mOutil == Outil::PLACER && mActeur == act;
			if (mUi.Ligne(id, ligne, choisi, (rang & 1) != 0) && mEntree.Dans(liste)) {
				ChoisirActeur(act);
				mGlisseActeur = !info.pinceau && act != NkActeur::NK_A_PONT;
			}
			mUi.InfoBulle(info.description);
			const NkRect tuile{ligne.x + 8.f * E, ligne.y + 6.f * E, 34.f * E, 34.f * E};
			mDl.AddRectFilled(tuile, kChamp, 4.f * E);
			mDl.AddRect(tuile, choisi ? kAccent : NkColor(48, 48, 48), 1.f, 4.f * E);
			NkDessinerIcone(mDl, IconeActeur(act), NkVec2{tuile.x + tuile.w * 0.5f, tuile.y + tuile.h * 0.5f}, 26.f * E,
							NkColor(info.couleur[0], info.couleur[1], info.couleur[2]));
			const float32 tx = tuile.x + tuile.w + 10.f * E;
			mUi.Texte(tx, ligne.y + 7.f * E, info.nom, choisi ? kTexteClair : kTexte, 13.f * E);
			mUi.Texte(tx, ligne.y + 25.f * E, info.description, choisi ? NkColor(200, 215, 240) : kTexteFaible, 11.f * E, false,
					  ligne.x + ligne.w - tx - 30.f * E);
			char touche[4];
			std::snprintf(touche, sizeof(touche), "%d", (a + 1) % 10);
			if (a < 10) {
				const NkRect k{ligne.x + ligne.w - 26.f * E, ligne.y + 12.f * E, 18.f * E, 18.f * E};
				mDl.AddRect(k, NkColor(70, 70, 70), 1.f, 3.f * E);
				mUi.TexteCentre(k, touche, kTexteFaible, 11.f * E);
			}
			if (info.pinceau) {
				mUi.Texte(ligne.x + ligne.w - 58.f * E, ligne.y + 7.f * E, "pinceau", kLogCategorie, 10.5f * E);
			}
			ly += lh;
			++rang;
		}
		mDl.PopClipRect();

		// Pied : l'aide du geste.
		const NkRect pied{r.x, r.y + r.h - piedH, r.w, piedH};
		mDl.AddRectFilled(pied, kPanneauSombre);
		mUi.Texte(pied.x + 10.f * E, pied.y + 6.f * E, "Clic dans la vue : poser  ·  glisser-déposer", kTexteFaible, 11.f * E);
		mUi.Texte(pied.x + 10.f * E, pied.y + 24.f * E, "Maintenir : verser (fluides)  ·  Échap : fin", kTexteFaible, 11.f * E);

		// Fin d'un glisser-deposer.
		if (mGlisseActeur && mEntree.relache[0]) {
			if (mEntree.Dans(mDisp.vue) && !mEntree.Dans(r)) {
				const NkV2 w = mCam.Monde(mEntree.x, mEntree.y);
				const int32 c = NkCreerActeur(mMonde, mActeur, w);
				if (c >= 0) {
					Selectionner(mMonde.corps[static_cast<uint32>(c)].id);
					Journal(Niveau::INFO, "LogActeur", "%s déposé (%u particules)", mMonde.corps[static_cast<uint32>(c)].nom,
							static_cast<unsigned>(mMonde.corps[static_cast<uint32>(c)].nombre));
				}
			}
			mGlisseActeur = false;
		}
		if (!mEntree.bas[0] && !mEntree.relache[0]) {
			mGlisseActeur = false;
		}
	}

	// =========================================================================
	// Outliner
	// =========================================================================
	void Physic2D::DessinerOutliner() {
		const float32 E = mEchelle;
		const NkRect &r = mDisp.outliner;
		mDl.AddRectFilled(r, kPanneau);
		EnTetePanneau(r, "Outliner", NkIcone::NK_NIVEAU);
		const float32 colType = r.x + r.w * 0.66f;
		const NkRect entete{r.x, r.y + 30.f * E, r.w, 24.f * E};
		mDl.AddRectFilled(entete, kPanneauSombre);
		mUi.TexteGauche(NkRect{r.x + 34.f * E, entete.y, colType - r.x, entete.h}, "Libellé de l'élément", kTexteFaible, 11.5f * E, 0.f);
		mUi.TexteGauche(NkRect{colType, entete.y, r.x + r.w - colType, entete.h}, "Type", kTexteFaible, 11.5f * E, 0.f);
		mDl.AddRectFilled(NkRect{colType - 6.f * E, entete.y + 4.f * E, 1.f, entete.h - 8.f * E}, NkColor(60, 60, 60));

		const float32 piedH = 22.f * E;
		const NkRect liste{r.x, entete.y + entete.h, r.w, r.y + r.h - entete.y - entete.h - piedH};
		const float32 lh = 24.f * E;
		uint32 n = 0;
		for (uint32 c = 0; c < mMonde.corps.Size(); ++c) {
			n += mMonde.corps[c].nombre > 0 ? 1u : 0u;
		}
		const float32 maxDefil = NkMaxf(0.f, static_cast<float32>(n + 1) * lh - liste.h);
		if (mEntree.Dans(liste) && mEntree.molette != 0.f && mPopup == Popup::AUCUN) {
			mDefilOutliner -= mEntree.molette * lh * 2.f;
		}
		mDefilOutliner = NkClampf(mDefilOutliner, 0.f, maxDefil);

		mDl.PushClipRect(liste);
		float32 y = liste.y - mDefilOutliner;
		// La racine : le niveau lui-meme, comme le "World" d'UE.
		{
			const NkRect ligne{liste.x, y, liste.w, lh};
			mUi.Ligne("outl_monde", ligne, false, false);
			NkDessinerIcone(mDl, NkIcone::NK_FLECHE_BAS, NkVec2{ligne.x + 12.f * E, y + lh * 0.5f}, 9.f * E, kTexteFaible);
			NkDessinerIcone(mDl, NkIcone::NK_NIVEAU, NkVec2{ligne.x + 28.f * E, y + lh * 0.5f}, 14.f * E, kTexte);
			char t[80];
			std::snprintf(t, sizeof(t), "%s (Éditeur)", NkNiveauNom(mNiveau));
			mUi.TexteGauche(NkRect{ligne.x + 40.f * E, y, colType - ligne.x - 40.f * E, lh}, t, kTexteClair, 12.5f * E, 0.f);
			mUi.TexteGauche(NkRect{colType, y, r.x + r.w - colType, lh}, "Monde", kTexteFaible, 12.f * E, 0.f);
			y += lh;
		}
		int32 rang = 1;
		for (uint32 c = 0; c < mMonde.corps.Size(); ++c) {
			NkCorps &co = mMonde.corps[c];
			if (co.nombre == 0) {
				continue;
			}
			const NkRect ligne{liste.x, y, liste.w, lh};
			if (y + lh >= liste.y && y <= liste.y + liste.h) {
				char id[32];
				std::snprintf(id, sizeof(id), "outl_%u", static_cast<unsigned>(co.id));
				const NkRect oeil{ligne.x + 4.f * E, y + 2.f * E, 20.f * E, lh - 4.f * E};
				const bool clicOeil = mUi.Clic(oeil) && mEntree.Dans(liste);
				const bool clic = mUi.Ligne(id, ligne, co.id == mSelection, (rang & 1) != 0) && mEntree.Dans(liste);
				if (clicOeil) {
					co.visible = !co.visible;
				} else if (clic) {
					Selectionner(co.id);
				}
				NkDessinerIcone(mDl, co.visible ? NkIcone::NK_OEIL : NkIcone::NK_OEIL_BARRE,
								NkVec2{oeil.x + oeil.w * 0.5f, oeil.y + oeil.h * 0.5f}, 13.f * E,
								co.visible ? kTexteFaible : kTexteTresFaible);
				NkDessinerIcone(mDl, IconeMateriau(co.mat), NkVec2{ligne.x + 40.f * E, y + lh * 0.5f}, 15.f * E,
								NkColor(co.couleur[0], co.couleur[1], co.couleur[2]));
				mUi.TexteGauche(NkRect{ligne.x + 54.f * E, y, colType - ligne.x - 58.f * E, lh}, co.nom,
								co.visible ? (co.id == mSelection ? kTexteClair : kTexte) : kTexteTresFaible, 12.5f * E, 0.f);
				mUi.TexteGauche(NkRect{colType, y, r.x + r.w - colType, lh}, NkMateriauNom(co.mat), kTexteFaible, 12.f * E, 0.f);
			}
			y += lh;
			++rang;
		}
		mDl.PopClipRect();

		const NkRect pied{r.x, r.y + r.h - piedH, r.w, piedH};
		mDl.AddRectFilled(pied, kPanneauSombre);
		char t[96];
		std::snprintf(t, sizeof(t), "%u acteurs  (%u sélectionné)  ·  %u obstacles", static_cast<unsigned>(n),
					  mMonde.IndexCorps(mSelection) >= 0 ? 1u : 0u, static_cast<unsigned>(mMonde.obstacles.Size()));
		mUi.TexteGauche(pied, t, kTexteFaible, 11.5f * E, 10.f * E);
	}

	// =========================================================================
	// Details
	// =========================================================================
	void Physic2D::DessinerDetails() {
		const float32 E = mEchelle;
		const NkRect &r = mDisp.details;
		mDl.AddRectFilled(r, kPanneau);
		EnTetePanneau(r, "Détails", NkIcone::NK_ENGRENAGE);
		const NkRect zone{r.x, r.y + 30.f * E, r.w, r.h - 30.f * E};
		if (mEntree.Dans(zone) && mEntree.molette != 0.f && mPopup == Popup::AUCUN && mUi.Actif() == 0) {
			mDefilDetails -= mEntree.molette * 40.f * E;
		}
		mDl.PushClipRect(zone);
		const float32 y0 = zone.y - mDefilDetails;
		float32 y = y0;
		const int32 ci = mMonde.IndexCorps(mSelection);
		if (ci >= 0) {
			DessinerDetailsCorps(mMonde.corps[static_cast<uint32>(ci)], static_cast<uint32>(ci), y, zone);
		} else {
			DessinerDetailsMonde(y, zone);
		}
		mDl.PopClipRect();
		const float32 hauteur = y - y0;
		mDefilDetails = NkClampf(mDefilDetails, 0.f, NkMaxf(0.f, hauteur - zone.h + 10.f * E));
		// Barre de defilement.
		if (hauteur > zone.h) {
			const float32 ratio = zone.h / hauteur;
			const float32 bh = NkMaxf(30.f * E, zone.h * ratio);
			const float32 by = zone.y + (zone.h - bh) * (mDefilDetails / NkMaxf(1.f, hauteur - zone.h));
			mDl.AddRectFilled(NkRect{zone.x + zone.w - 5.f * E, by, 3.f * E, bh}, NkColor(90, 90, 90), 2.f * E);
		}
	}

	void Physic2D::DessinerDetailsCorps(NkCorps &c, uint32 index, float32 &y, const NkRect &zone) {
		const float32 E = mEchelle;
		char buf[96];
		// En-tete : tuile + nom + type.
		{
			const float32 h = 58.f * E;
			const NkRect tuile{zone.x + 10.f * E, y + 9.f * E, 40.f * E, 40.f * E};
			mDl.AddRectFilled(tuile, kChamp, 4.f * E);
			NkDessinerIcone(mDl, IconeMateriau(c.mat), NkVec2{tuile.x + tuile.w * 0.5f, tuile.y + tuile.h * 0.5f}, 30.f * E,
							NkColor(c.couleur[0], c.couleur[1], c.couleur[2]));
			const NkRect nom{tuile.x + tuile.w + 10.f * E, y + 10.f * E, zone.w - tuile.w - 40.f * E, 22.f * E};
			mDl.AddRectFilled(nom, kChamp, 3.f * E);
			mUi.TexteGauche(nom, c.nom, kTexteClair, 13.f * E, 7.f * E);
			std::snprintf(buf, sizeof(buf), "Corps  ·  matériau %s", NkMateriauNom(c.mat));
			mUi.Texte(nom.x + 2.f * E, nom.y + 26.f * E, buf, kTexteFaible, 11.5f * E);
			y += h;
		}

		// Transformation.
		if (mUi.Categorie("cat_transfo", NkRect{zone.x, y, zone.w, 26.f * E}, "Transformation", mCatOuverte[0])) {
		}
		y += 26.f * E;
		if (mCatOuverte[0]) {
			const NkV2 ctr = mMonde.CentreCorps(index);
			NkV2 v;
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				v += mMonde.particules[i].vit;
			}
			v = c.nombre > 0 ? v / static_cast<float32>(c.nombre) : v;
			NkV2 mn, mx;
			mMonde.BoiteCorps(index, mn, mx);
			std::snprintf(buf, sizeof(buf), "X %.1f   Y %.1f  cm", static_cast<double>(ctr.x), static_cast<double>(ctr.y));
			LigneTexte(y, zone, "Position", buf);
			std::snprintf(buf, sizeof(buf), "%.0f cm/s", static_cast<double>(NkLen(v)));
			LigneTexte(y, zone, "Vitesse", buf);
			std::snprintf(buf, sizeof(buf), "%.0f x %.0f cm", static_cast<double>(mx.x - mn.x), static_cast<double>(mx.y - mn.y));
			LigneTexte(y, zone, "Encombrement", buf);
		}

		// Physique.
		mUi.Categorie("cat_phys", NkRect{zone.x, y, zone.w, 26.f * E}, "Physique", mCatOuverte[1]);
		y += 26.f * E;
		if (mCatOuverte[1]) {
			if (LigneProp("p_masse", y, zone, "Masse / particule", c.masseParticule, 0.1f, 10.f, "%.2f kg")) {
				mMonde.AppliquerMasse(index);
			}
			const bool fluide = c.mat == NkMateriau::NK_EAU || c.mat == NkMateriau::NK_MIEL;
			if (!fluide && c.mat != NkMateriau::NK_SABLE) {
				LigneProp("p_raideur", y, zone, "Raideur", c.raideur, 0.f, 1.f, "%.3f");
			}
			LigneProp("p_friction", y, zone, "Friction", c.friction, 0.f, 1.f, "%.2f");
			LigneProp("p_rebond", y, zone, "Rebond", c.rebond, 0.f, 1.f, "%.2f");
			if (c.mat == NkMateriau::NK_BALLON) {
				LigneProp("p_pression", y, zone, "Pression", c.pression, 0.f, 3.f, "%.2f");
			}
			if (c.mat == NkMateriau::NK_GELEE || c.mat == NkMateriau::NK_CAISSE) {
				LigneProp("p_forme", y, zone, "Mémoire de forme", c.formeRaideur, 0.f, 1.f, "%.3f");
			}
			if (NkEstFluide(c.mat)) {
				LigneProp("p_visc", y, zone, "Viscosité", c.viscosite, 0.f, 0.5f, "%.3f");
				LigneProp("p_coh", y, zone, "Cohésion", c.cohesion, 0.f, 1.5f, "%.2f");
			}
			if (c.mat == NkMateriau::NK_BLOB) {
				LigneProp("p_plast", y, zone, "Plasticité (coule)", c.plasticite, 0.f, 25.f, "%.1f");
			}
			if (c.mat != NkMateriau::NK_EAU && c.mat != NkMateriau::NK_MIEL && c.mat != NkMateriau::NK_SABLE &&
				c.mat != NkMateriau::NK_GELEE && c.mat != NkMateriau::NK_CAISSE) {
				LigneProp("p_resist", y, zone, "Rupture à (× repos)", c.resistance, 0.f, 4.f,
						  c.resistance <= 0.f ? "incassable" : "%.2f");
			}
			LigneCase("p_auto", y, zone, "Collisions internes", c.autoCollision);
		}

		// Rendu.
		mUi.Categorie("cat_rendu", NkRect{zone.x, y, zone.w, 26.f * E}, "Rendu", mCatOuverte[2]);
		y += 26.f * E;
		if (mCatOuverte[2]) {
			LigneCase("r_visible", y, zone, "Visible", c.visible);
			const char *noms[3] = {"Rouge", "Vert", "Bleu"};
			const char *ids[3] = {"r_r", "r_g", "r_b"};
			for (int32 k = 0; k < 3; ++k) {
				int32 v = c.couleur[k];
				const float32 h = 28.f * E;
				const float32 col = zone.x + zone.w * 0.44f;
				mUi.TexteGauche(NkRect{zone.x + 18.f * E, y, col - zone.x - 18.f * E, h}, noms[k], kTexte, 12.5f * E, 0.f);
				if (mUi.GlisseurEntier(ids[k], NkRect{col, y + 4.f * E, zone.x + zone.w - col - 44.f * E, h - 8.f * E}, v, 0, 255)) {
					c.couleur[k] = static_cast<uint8>(v);
				}
				if (k == 0) {
					mDl.AddRectFilled(NkRect{zone.x + zone.w - 36.f * E, y + 4.f * E, 26.f * E, 3.f * h - 8.f * E},
									  NkColor(c.couleur[0], c.couleur[1], c.couleur[2]), 3.f * E);
				}
				y += h;
			}
		}

		// Statistiques.
		mUi.Categorie("cat_stats", NkRect{zone.x, y, zone.w, 26.f * E}, "Statistiques", mCatOuverte[3]);
		y += 26.f * E;
		if (mCatOuverte[3]) {
			uint32 liens = 0;
			uint32 casses = 0;
			for (uint32 k = 0; k < mMonde.liens.Size(); ++k) {
				if (mMonde.liens[k].corps == index) {
					if (mMonde.liens[k].casse) {
						++casses;
					} else {
						++liens;
					}
				}
			}
			uint32 epingles = 0;
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				epingles += mMonde.particules[i].epingle ? 1u : 0u;
			}
			std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(c.nombre));
			LigneTexte(y, zone, "Particules", buf);
			std::snprintf(buf, sizeof(buf), "%u actifs, %u rompus", static_cast<unsigned>(liens), static_cast<unsigned>(casses));
			LigneTexte(y, zone, "Liens", buf);
			std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(epingles));
			LigneTexte(y, zone, "Épinglées", buf);
			if (c.mat == NkMateriau::NK_BALLON) {
				LigneTexte(y, zone, "Etat", c.pression > 0.f ? "gonflé" : "crevé");
			}
		}

		// Actions.
		mUi.Categorie("cat_actions", NkRect{zone.x, y, zone.w, 26.f * E}, "Actions", mCatOuverte[4]);
		y += 26.f * E;
		if (mCatOuverte[4]) {
			const float32 bw = (zone.w - 50.f * E) * 0.5f;
			const float32 bh = 26.f * E;
			float32 bx = zone.x + 18.f * E;
			y += 6.f * E;
			if (mUi.Bouton("a_epingler", NkRect{bx, y, bw, bh}, "Tout épingler", false, NkIcone::NK_EPINGLE)) {
				mMonde.EpinglerCorps(index, true);
			}
			if (mUi.Bouton("a_liberer", NkRect{bx + bw + 8.f * E, y, bw, bh}, "Tout libérer")) {
				mMonde.EpinglerCorps(index, false);
			}
			y += bh + 6.f * E;
			if (mUi.Bouton("a_figer", NkRect{bx, y, bw, bh}, "Figer (v = 0)")) {
				mMonde.AppliquerVitesse(index, NkV2());
			}
			if (mUi.Bouton("a_focus", NkRect{bx + bw + 8.f * E, y, bw, bh}, "Focaliser (F)", false, NkIcone::NK_RECADRER)) {
				Focaliser();
			}
			y += bh + 6.f * E;
			const NkColor rouge = kArret;
			if (mUi.Bouton("a_suppr", NkRect{bx, y, bw * 2.f + 8.f * E, bh}, "Supprimer (Suppr)", false, NkIcone::NK_POUBELLE, &rouge)) {
				SupprimerSelection();
			}
			y += bh + 12.f * E;
			bx += 0.f;
		}
	}

	void Physic2D::DessinerDetailsMonde(float32 &y, const NkRect &zone) {
		const float32 E = mEchelle;
		NkReglages &rg = mMonde.reglages;
		{
			const float32 h = 58.f * E;
			const NkRect tuile{zone.x + 10.f * E, y + 9.f * E, 40.f * E, 40.f * E};
			mDl.AddRectFilled(tuile, kChamp, 4.f * E);
			NkDessinerIcone(mDl, NkIcone::NK_ENGRENAGE, NkVec2{tuile.x + tuile.w * 0.5f, tuile.y + tuile.h * 0.5f}, 28.f * E, kTexte);
			mUi.Texte(tuile.x + tuile.w + 10.f * E, y + 12.f * E, "Paramètres du monde", kTexteClair, 15.f * E);
			mUi.Texte(tuile.x + tuile.w + 10.f * E, y + 33.f * E, "Aucun acteur sélectionné", kTexteFaible, 11.5f * E);
			y += h;
		}
		mUi.Categorie("m_cat_monde", NkRect{zone.x, y, zone.w, 26.f * E}, "Monde", mCatOuverte[0]);
		y += 26.f * E;
		if (mCatOuverte[0]) {
			LigneProp("m_gx", y, zone, "Gravité X", rg.gravite.x, -2000.f, 2000.f, "%.0f cm/s2");
			LigneProp("m_gy", y, zone, "Gravité Y", rg.gravite.y, -3000.f, 1000.f, "%.0f cm/s2");
			LigneProp("m_vent", y, zone, "Vent", rg.vent, -2500.f, 2500.f, "%.0f cm/s2");
			LigneProp("m_temp", y, zone, "Température", rg.temperature, 0.f, 100.f, "%.0f");
			LigneProp("m_air", y, zone, "Frottement de l'air", rg.amortAir, 0.f, 3.f, "%.2f");
		}
		mUi.Categorie("m_cat_solveur", NkRect{zone.x, y, zone.w, 26.f * E}, "Solveur", mCatOuverte[1]);
		y += 26.f * E;
		if (mCatOuverte[1]) {
			{
				const float32 h = 28.f * E;
				const float32 col = zone.x + zone.w * 0.44f;
				mUi.TexteGauche(NkRect{zone.x + 18.f * E, y, col - zone.x - 18.f * E, h}, "Sous-pas", kTexte, 12.5f * E, 0.f);
				mUi.GlisseurEntier("m_sp", NkRect{col, y + 4.f * E, zone.x + zone.w - col - 10.f * E, h - 8.f * E}, rg.sousPas, 1, 20);
				y += h;
			}
			LigneProp("m_temps", y, zone, "Échelle du temps", rg.echelleTemps, 0.05f, 2.f, "x %.2f");
			LigneCase("m_coll", y, zone, "Collisions", rg.collisions);
			LigneCase("m_soud", y, zone, "Soudure (atomes, blobs)", rg.soudure);
			LigneCase("m_murs", y, zone, "Murs et plafond", rg.murs);
		}
		mUi.Categorie("m_cat_aff", NkRect{zone.x, y, zone.w, 26.f * E}, "Affichage", mCatOuverte[2]);
		y += 26.f * E;
		if (mCatOuverte[2]) {
			{
				const float32 h = 30.f * E;
				const float32 bw = (zone.w - 36.f * E - 3.f * 4.f * E) / 4.f;
				for (int32 m = 0; m < static_cast<int32>(NkModeVue::NK_COUNT); ++m) {
					char id[16];
					std::snprintf(id, sizeof(id), "m_vue_%d", m);
					if (mUi.Bouton(id, NkRect{zone.x + 18.f * E + static_cast<float32>(m) * (bw + 4.f * E), y + 4.f * E, bw, h - 8.f * E},
								   NkModeVueNom(static_cast<NkModeVue>(m)), static_cast<int32>(mOptions.mode) == m)) {
						mOptions.mode = static_cast<NkModeVue>(m);
					}
				}
				y += h;
			}
			LigneCase("m_grille", y, zone, "Grille", mOptions.grille);
			LigneCase("m_liens", y, zone, "Liens", mOptions.liens);
			LigneCase("m_parts", y, zone, "Particules", mOptions.particules);
			LigneCase("m_vit", y, zone, "Vecteurs vitesse", mOptions.vitesses);
			LigneCase("m_epin", y, zone, "Épingles", mOptions.epingles);
		}
		mUi.Categorie("m_cat_niv", NkRect{zone.x, y, zone.w, 26.f * E}, "Niveau", mCatOuverte[3]);
		y += 26.f * E;
		if (mCatOuverte[3]) {
			const float32 bw = (zone.w - 50.f * E) * 0.5f;
			y += 6.f * E;
			if (mUi.Bouton("m_recharger", NkRect{zone.x + 18.f * E, y, bw, 26.f * E}, "Recharger")) {
				ChargerNiveau(mNiveau);
			}
			if (mUi.Bouton("m_vider", NkRect{zone.x + 26.f * E + bw, y, bw, 26.f * E}, "Vider le niveau")) {
				ChargerNiveau(NK_NB_NIVEAUX - 1);
			}
			y += 38.f * E;
			if (mUi.Bouton("m_obst", NkRect{zone.x + 18.f * E, y, bw * 2.f + 8.f * E, 26.f * E}, "Effacer les obstacles", false,
						   NkIcone::NK_POUBELLE)) {
				mMonde.obstacles.Clear();
				Journal(Niveau::AVERT, "LogActeur", "Obstacles effacés");
			}
			y += 38.f * E;
		}
	}

	// =========================================================================
	// Bas : journal, statistiques, aide
	// =========================================================================
	void Physic2D::DessinerBas() {
		const float32 E = mEchelle;
		const NkRect &r = mDisp.bas;
		mDl.AddRectFilled(r, kPanneau);
		const float32 h = 30.f * E;
		mDl.AddRectFilled(NkRect{r.x, r.y, r.w, h}, kEnteteOnglets);
		const char *onglets[3] = {"Journal de sortie", "Statistiques", "Aide (F1)"};
		const NkIcone icones[3] = {NkIcone::NK_GRILLE, NkIcone::NK_ENGRENAGE, NkIcone::NK_LOUPE};
		float32 x = r.x;
		for (int32 i = 0; i < 3; ++i) {
			const float32 w = mUi.LargeurTexte(onglets[i], 12.5f * E) + 46.f * E;
			if (mUi.Onglet(NkRect{x, r.y, w, h}, onglets[i], mOngletBas == i, icones[i])) {
				mOngletBas = i;
			}
			x += w;
		}
		if (mOngletBas == 0) {
			if (mUi.Bouton("journal_effacer", NkRect{r.x + r.w - 90.f * E, r.y + 4.f * E, 84.f * E, h - 8.f * E}, "Effacer")) {
				mJournal.Clear();
			}
		}
		const NkRect contenu{r.x, r.y + h, r.w, r.h - h};
		if (mOngletBas == 0) {
			DessinerJournal(contenu);
		} else if (mOngletBas == 1) {
			DessinerStats(contenu);
		} else {
			DessinerAide(contenu);
		}
	}

	void Physic2D::DessinerJournal(const NkRect &r) {
		const float32 E = mEchelle;
		mDl.AddRectFilled(r, kPanneauSombre);
		const float32 lh = mTailleMono * 1.4f;
		const int32 visibles = static_cast<int32>((r.h - 8.f * E) / lh);
		const int32 total = static_cast<int32>(mJournal.Size());
		if (mEntree.Dans(r) && mEntree.molette != 0.f && mPopup == Popup::AUCUN) {
			mDefilJournal += mEntree.molette * 3.f;
		}
		mDefilJournal = NkClampf(mDefilJournal, 0.f, NkMaxf(0.f, static_cast<float32>(total - visibles)));
		const int32 fin = total - static_cast<int32>(mDefilJournal);
		const int32 debut = fin - visibles < 0 ? 0 : fin - visibles;
		mDl.PushClipRect(r);
		float32 y = r.y + 4.f * E;
		for (int32 i = debut; i < fin; ++i) {
			const LigneJournal &l = mJournal[static_cast<uint32>(i)];
			char t[24];
			std::snprintf(t, sizeof(t), "[%7.2f]", static_cast<double>(l.temps));
			float32 x = r.x + 8.f * E;
			mUi.Texte(x, y, t, kTexteTresFaible, mTailleMono, true);
			x += mUi.LargeurTexte(t, mTailleMono, true) + 8.f * E;
			char cat[24];
			std::snprintf(cat, sizeof(cat), "%s:", l.categorie);
			mUi.Texte(x, y, cat, kLogCategorie, mTailleMono, true);
			x += mUi.LargeurTexte(cat, mTailleMono, true) + 8.f * E;
			NkColor c = kLogNormal;
			const char *prefixe = "";
			switch (l.niveau) {
				case Niveau::SUCCES:
					c = kLogSucces;
					break;
				case Niveau::AVERT:
					c = kLogAvert;
					prefixe = "Avertissement : ";
					break;
				case Niveau::ERREUR:
					c = kLogErreur;
					prefixe = "Erreur : ";
					break;
				default:
					break;
			}
			if (*prefixe) {
				mUi.Texte(x, y, prefixe, c, mTailleMono, true);
				x += mUi.LargeurTexte(prefixe, mTailleMono, true);
			}
			mUi.Texte(x, y, l.texte, c, mTailleMono, true);
			y += lh;
		}
		mDl.PopClipRect();
	}

	void Physic2D::DessinerStats(const NkRect &r) {
		const float32 E = mEchelle;
		mDl.AddRectFilled(r, kPanneauSombre);
		char t[96];
		const float32 colW = 250.f * E;
		float32 y = r.y + 8.f * E;
		const float32 lh = 19.f * E;
		auto ligne = [&](const char *lib, const char *val) {
			mUi.Texte(r.x + 14.f * E, y, lib, kTexteFaible, 12.f * E);
			mUi.Texte(r.x + 150.f * E, y, val, kTexteClair, 12.f * E);
			y += lh;
		};
		std::snprintf(t, sizeof(t), "%.0f IPS  (%.2f ms)", static_cast<double>(mFps), static_cast<double>(1000.f / NkMaxf(mFps, 1.f)));
		ligne("Images", t);
		std::snprintf(t, sizeof(t), "%.2f ms / trame", static_cast<double>(mMsPhysique));
		ligne("Physique", t);
		std::snprintf(t, sizeof(t), "%u", static_cast<unsigned>(mMonde.particules.Size()));
		ligne("Particules", t);
		std::snprintf(t, sizeof(t), "%u actifs / %u", static_cast<unsigned>(mMonde.LiensActifs()), static_cast<unsigned>(mMonde.liens.Size()));
		ligne("Liens", t);
		std::snprintf(t, sizeof(t), "%u   (dernier pas)", static_cast<unsigned>(mMonde.stats.contacts));
		ligne("Contacts", t);
		std::snprintf(t, sizeof(t), "%u   (depuis le début)", static_cast<unsigned>(mMonde.rupturesTotal));
		ligne("Ruptures", t);
		std::snprintf(t, sizeof(t), "%.1f J", static_cast<double>(mMonde.stats.energieCinetique));
		ligne("Énergie cinétique", t);

		// Courbes.
		auto courbe = [&](const NkRect &g, const float32 *val, const char *titre, const NkColor &col, float32 plafond, float32 repere) {
			mDl.AddRectFilled(g, kChamp, 3.f * E);
			mDl.AddRect(g, NkColor(48, 48, 48), 1.f, 3.f * E);
			float32 vmax = plafond;
			for (int32 i = 0; i < kHist; ++i) {
				vmax = NkMaxf(vmax, val[i]);
			}
			if (repere > 0.f) {
				const float32 ry = g.y + g.h - (repere / vmax) * (g.h - 18.f * E);
				mDl.AddLine(NkVec2{g.x, ry}, NkVec2{g.x + g.w, ry}, NkColor(255, 200, 64, 90), 1.f);
			}
			NkVec2 pts[kHist];
			for (int32 i = 0; i < kHist; ++i) {
				const float32 v = val[(mHistIndex + i) % kHist];
				pts[i] = NkVec2{g.x + g.w * static_cast<float32>(i) / static_cast<float32>(kHist - 1),
								g.y + g.h - 2.f - (v / vmax) * (g.h - 18.f * E)};
			}
			for (int32 i = 1; i < kHist; ++i) {
				mDl.AddTriangleFilled(pts[i - 1], pts[i], NkVec2{pts[i].x, g.y + g.h}, NkAvecAlpha(col, 40));
				mDl.AddTriangleFilled(pts[i - 1], NkVec2{pts[i].x, g.y + g.h}, NkVec2{pts[i - 1].x, g.y + g.h}, NkAvecAlpha(col, 40));
			}
			mDl.AddPolyline(pts, kHist, col, 1.5f * E);
			std::snprintf(t, sizeof(t), "%s  (max %.2f)", titre, static_cast<double>(vmax));
			mUi.Texte(g.x + 6.f * E, g.y + 3.f * E, t, kTexte, 11.f * E);
		};
		const float32 gx = r.x + colW + 20.f * E;
		const float32 gw = (r.w - colW - 50.f * E) / 3.f;
		const float32 gh = r.h - 20.f * E;
		if (gw > 60.f * E) {
			courbe(NkRect{gx, r.y + 10.f * E, gw, gh}, mHistImage, "Trame (ms)", kAccentClair, 20.f, 16.67f);
			courbe(NkRect{gx + gw + 10.f * E, r.y + 10.f * E, gw, gh}, mHistPhys, "Physique (ms)", kJouer, 4.f, 0.f);
			courbe(NkRect{gx + 2.f * (gw + 10.f * E), r.y + 10.f * E, gw, gh}, mHistEnergie, "Énergie (J)", kPauseCouleur, 1.f, 0.f);
		}
	}

	void Physic2D::DessinerAide(const NkRect &r) {
		const float32 E = mEchelle;
		mDl.AddRectFilled(r, kPanneauSombre);
		const char *lignes[][2] = {
			{"Q W E R T Y G P", "Sélection, Saisir, Couteau, Explosion, Aimant, Obstacle, Gomme, Épingle"},
			{"1 .. 9, 0", "Choisir un acteur à placer (clic dans la vue ; fluides : maintenir)"},
			{"Espace / N / F5", "Jouer-Pause / Avancer d'un pas / Arrêter et restaurer"},
			{"Molette", "Zoom sur le curseur"},
			{"Clic droit / milieu", "Déplacer la vue"},
			{"F / H", "Focaliser la sélection / Recadrer le monde"},
			{"V", "Mode de vue : Éclairé, Filaire, Contraintes, Vitesse"},
			{"Suppr, Échap", "Supprimer la sélection, annuler l'outil"},
			{"Maj", "Aimant : repousser  ·  champs : réglage fin"},
		};
		float32 y = r.y + 8.f * E;
		const float32 lh = 17.f * E;
		const int32 n = static_cast<int32>(sizeof(lignes) / sizeof(lignes[0]));
		for (int32 i = 0; i < n && y + lh < r.y + r.h; ++i) {
			const float32 kw = mUi.LargeurTexte(lignes[i][0], mTailleMono, true) + 12.f * E;
			const NkRect k{r.x + 14.f * E, y, kw, lh - 2.f * E};
			mDl.AddRectFilled(k, NkColor(50, 50, 50), 3.f * E);
			mUi.TexteGauche(k, lignes[i][0], kTexteClair, 11.5f * E, 6.f * E);
			mUi.Texte(r.x + 180.f * E, y + 1.f * E, lignes[i][1], kTexte, 12.f * E);
			y += lh;
		}
		(void)E;
	}

	// =========================================================================
	// Menus deroulants
	// =========================================================================
	int32 Physic2D::ElementsPopup(Popup p, ElementMenu *out, int32 max) const {
		int32 n = 0;
		auto ajouter = [&](const char *lib, const char *rac, int32 action, bool coche = false, bool actif = true) {
			if (n < max) {
				out[n].libelle = lib;
				out[n].raccourci = rac;
				out[n].action = action;
				out[n].coche = coche;
				out[n].separateur = false;
				out[n].actif = actif;
				++n;
			}
		};
		auto separer = [&]() {
			if (n < max) {
				out[n] = ElementMenu{};
				out[n].separateur = true;
				++n;
			}
		};
		const bool sel = mMonde.IndexCorps(mSelection) >= 0;
		switch (p) {
			case Popup::MENU_FICHIER:
				ajouter("Nouveau niveau vide", "", 1);
				ajouter("Recharger le niveau", "", 2);
				separer();
				ajouter("Quitter", "Alt+F4", 3);
				break;
			case Popup::MENU_EDITION:
				ajouter("Supprimer la sélection", "Suppr", 1, false, sel);
				ajouter("Tout désélectionner", "Échap", 2, false, sel);
				ajouter("Épingler la sélection", "", 3, false, sel);
				ajouter("Libérer la sélection", "", 4, false, sel);
				separer();
				ajouter("Tout figer (vitesses à zéro)", "", 5);
				ajouter("Effacer les obstacles", "", 6);
				break;
			case Popup::MENU_FENETRE:
				ajouter("Placer des acteurs", "", 1, mMontrerGauche);
				ajouter("Outliner et Détails", "", 2, mMontrerDroite);
				ajouter("Journal de sortie", "", 3, mMontrerBas);
				separer();
				ajouter("Recadrer la vue", "H", 4);
				ajouter("Focaliser la sélection", "F", 5, false, sel);
				break;
			case Popup::MENU_NIVEAUX:
			case Popup::NIVEAU:
				for (int32 i = 0; i < NK_NB_NIVEAUX; ++i) {
					ajouter(NkNiveauNom(i), "", i, i == mNiveau);
				}
				break;
			case Popup::MENU_AIDE:
				ajouter("Raccourcis clavier", "F1", 1);
				ajouter("Lancer le banc du moteur", "--selftest", 2, false, false);
				ajouter("À propos de Physic2D", "", 3);
				break;
			case Popup::VUE_MODE:
				for (int32 m = 0; m < static_cast<int32>(NkModeVue::NK_COUNT); ++m) {
					ajouter(NkModeVueNom(static_cast<NkModeVue>(m)), m == 0 ? "V" : "", m, static_cast<int32>(mOptions.mode) == m);
				}
				break;
			case Popup::VUE_AFFICHER:
				ajouter("Grille", "", 1, mOptions.grille);
				ajouter("Liens", "", 2, mOptions.liens);
				ajouter("Particules", "", 3, mOptions.particules);
				ajouter("Vecteurs vitesse", "", 4, mOptions.vitesses);
				ajouter("Epingles", "", 5, mOptions.epingles);
				break;
			default:
				break;
		}
		return n;
	}

	NkRect Physic2D::RectPopup() const {
		ElementMenu el[16];
		const int32 n = ElementsPopup(mPopup, el, 16);
		const float32 E = mEchelle;
		float32 h = 8.f * E;
		for (int32 i = 0; i < n; ++i) {
			h += el[i].separateur ? 9.f * E : 26.f * E;
		}
		const float32 w = 270.f * E;
		float32 x = mDeclencheur.x;
		const float32 y = mDeclencheur.y + mDeclencheur.h + 2.f * E;
		if (x + w > mDisp.menu.w - 4.f * E) {
			x = mDisp.menu.w - w - 4.f * E;
		}
		return NkRect{x, y, w, h};
	}

	void Physic2D::ExecuterMenu(Popup p, int32 action) {
		switch (p) {
			case Popup::MENU_FICHIER:
				if (action == 1) {
					ChargerNiveau(NK_NB_NIVEAUX - 1);
				} else if (action == 2) {
					ChargerNiveau(mNiveau);
				} else if (action == 3) {
					Quit();
				}
				break;
			case Popup::MENU_EDITION: {
				const int32 ci = mMonde.IndexCorps(mSelection);
				if (action == 1) {
					SupprimerSelection();
				} else if (action == 2) {
					mSelection = 0;
				} else if (action == 3 && ci >= 0) {
					mMonde.EpinglerCorps(static_cast<uint32>(ci), true);
				} else if (action == 4 && ci >= 0) {
					mMonde.EpinglerCorps(static_cast<uint32>(ci), false);
				} else if (action == 5) {
					for (uint32 i = 0; i < mMonde.particules.Size(); ++i) {
						mMonde.particules[i].vit = NkV2();
					}
				} else if (action == 6) {
					mMonde.obstacles.Clear();
				}
				break;
			}
			case Popup::MENU_FENETRE:
				if (action == 1) {
					mMontrerGauche = !mMontrerGauche;
				} else if (action == 2) {
					mMontrerDroite = !mMontrerDroite;
				} else if (action == 3) {
					mMontrerBas = !mMontrerBas;
				} else if (action == 4) {
					Recadrer();
				} else if (action == 5) {
					Focaliser();
				}
				Disposer(static_cast<float32>(Layout().width), static_cast<float32>(Layout().height));
				break;
			case Popup::MENU_NIVEAUX:
			case Popup::NIVEAU:
				ChargerNiveau(action);
				break;
			case Popup::MENU_AIDE:
				if (action == 1) {
					mMontrerBas = true;
					mOngletBas = 2;
					Disposer(static_cast<float32>(Layout().width), static_cast<float32>(Layout().height));
				} else if (action == 3) {
					Journal(Niveau::SUCCES, "LogAide", "Physic2D · bac à sable physique 2D sur NKCanvas + NKGui (Rihen Universe)");
					Journal(Niveau::INFO, "LogAide", "Corps mous, ballons, blobs visqueux, fluides, sable, cristaux, tissus, cordes");
				}
				break;
			case Popup::VUE_MODE:
				mOptions.mode = static_cast<NkModeVue>(action);
				break;
			case Popup::VUE_AFFICHER:
				if (action == 1) {
					mOptions.grille = !mOptions.grille;
				} else if (action == 2) {
					mOptions.liens = !mOptions.liens;
				} else if (action == 3) {
					mOptions.particules = !mOptions.particules;
				} else if (action == 4) {
					mOptions.vitesses = !mOptions.vitesses;
				} else if (action == 5) {
					mOptions.epingles = !mOptions.epingles;
				}
				return; // on garde le menu ouvert : on coche souvent plusieurs cases
			default:
				break;
		}
		mPopup = Popup::AUCUN;
	}

	void Physic2D::DessinerPopup() {
		if (mPopup == Popup::AUCUN) {
			return;
		}
		const float32 E = mEchelle;
		ElementMenu el[16];
		const int32 n = ElementsPopup(mPopup, el, 16);
		const NkRect r = RectPopup();
		mDl.AddRectFilled(NkRect{r.x + 3.f * E, r.y + 4.f * E, r.w, r.h}, NkColor(0, 0, 0, 110), 4.f * E);
		mDl.AddRectFilled(r, NkColor(30, 30, 30), 4.f * E);
		mDl.AddRect(r, NkColor(62, 62, 62), 1.f, 4.f * E);
		float32 y = r.y + 4.f * E;
		const Popup p = mPopup;
		int32 action = -1;
		for (int32 i = 0; i < n; ++i) {
			if (el[i].separateur) {
				mDl.AddRectFilled(NkRect{r.x + 8.f * E, y + 4.f * E, r.w - 16.f * E, 1.f}, NkColor(60, 60, 60));
				y += 9.f * E;
				continue;
			}
			const NkRect ligne{r.x + 4.f * E, y, r.w - 8.f * E, 26.f * E};
			const bool survol = el[i].actif && mUi.Survol(ligne);
			if (survol) {
				mDl.AddRectFilled(ligne, kAccentSombre, 3.f * E);
			}
			if (el[i].coche) {
				const NkVec2 a{ligne.x + 9.f * E, ligne.y + 13.f * E};
				const NkVec2 b{ligne.x + 13.f * E, ligne.y + 17.f * E};
				const NkVec2 c{ligne.x + 20.f * E, ligne.y + 8.f * E};
				mDl.AddLine(a, b, kAccentClair, 2.f * E);
				mDl.AddLine(b, c, kAccentClair, 2.f * E);
			}
			mUi.TexteGauche(NkRect{ligne.x + 28.f * E, ligne.y, ligne.w, ligne.h}, el[i].libelle,
							el[i].actif ? (survol ? kTexteClair : kTexte) : kTexteTresFaible, 12.5f * E, 0.f);
			if (el[i].raccourci && *el[i].raccourci) {
				mUi.TexteDroite(ligne, el[i].raccourci, kTexteFaible, 11.5f * E, 10.f * E);
			}
			if (survol && mEntree.relache[0]) {
				action = el[i].action;
			}
			y += 26.f * E;
		}
		if (action >= 0) {
			ExecuterMenu(p, action);
			return;
		}
		// Clic hors du menu (et hors de son declencheur) : on ferme.
		// Pour un menu de la barre, c'est la barre qui gere son titre ; pour les
		// autres, recliquer le declencheur REFERME (il est bloque par la modale).
		const bool estMenu = p >= Popup::MENU_FICHIER && p <= Popup::MENU_AIDE;
		if ((mEntree.presse[0] || mEntree.presse[1]) && !mEntree.Dans(r) && !(estMenu && mEntree.Dans(mDeclencheur))) {
			mPopup = Popup::AUCUN;
		}
	}

} // namespace nkentseu
