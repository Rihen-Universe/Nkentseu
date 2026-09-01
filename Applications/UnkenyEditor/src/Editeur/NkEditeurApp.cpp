// -----------------------------------------------------------------------------
// FICHIER: Editeur/NkEditeurApp.cpp
// DESCRIPTION: L'assemblage de l'editeur : disposition, entrees, dessin.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Editeur/NkEditeurApp.h"

#include "Editeur/NkEditeurPanneaux.h"
#include "Editeur/NkEditeurViseur.h"
#include "NKContainers/String/NkString.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace editeur {

		// =====================================================================
		void NkDispoEditeur::Calculer(const renderer::NkLayoutInfo &info, const NkProfilAppareil &profil) noexcept {
			const float32 W = static_cast<float32>(info.width);
			const float32 H = static_cast<float32>(info.height);
			const float32 hBarre = 44.f;
			const float32 hEtat = 26.f;
			// Les colonnes prennent une FRACTION, avec un plancher : sur une
			// fenetre etroite, une fraction seule rend l'inspecteur illisible, et
			// une largeur fixe seule mange tout le viseur sur un grand ecran.
			const float32 lCote = W * 0.17f < 190.f ? 190.f : (W * 0.17f > 300.f ? 300.f : W * 0.17f);

			barre = nkgui::NkRect{0.f, 0.f, W, hBarre};
			etat = nkgui::NkRect{0.f, H - hEtat, W, hEtat};
			hierarchie = nkgui::NkRect{0.f, hBarre, lCote, H - hBarre - hEtat};
			inspecteur = nkgui::NkRect{W - lCote, hBarre, lCote, H - hBarre - hEtat};
			viseur = nkgui::NkRect{lCote, hBarre, W - lCote * 2.f, H - hBarre - hEtat};

			// L'aire d'appareil garde le RAPPORT du profil, centree dans le
			// viseur. C'est ce rapport qui fait juger une mise en page mobile
			// depuis un ecran de bureau — l'etirer la rendrait mensongere.
			const float32 rapport = static_cast<float32>(profil.largeur) / static_cast<float32>(profil.hauteur);
			const float32 marge = 24.f;
			float32 aw = viseur.w - marge * 2.f;
			float32 ah = aw / rapport;
			if (ah > viseur.h - marge * 2.f) {
				ah = viseur.h - marge * 2.f;
				aw = ah * rapport;
			}
			appareil = nkgui::NkRect{viseur.x + (viseur.w - aw) * 0.5f, viseur.y + (viseur.h - ah) * 0.5f, aw, ah};
		}

		// =====================================================================
		NkEditeurApp::NkEditeurApp() {
			Config().title = "Unkeny — editeur";
			Config().width = 1280;
			Config().height = 760;
			Config().clearColor = renderer::NkColor2D{14, 16, 22, 255};
		}

		NkOptional<int> NkEditeurApp::OnCommandLine(const NkVector<NkString> &args) {
			for (uint32 i = 0; i < args.Size(); ++i) {
				// --profil=N et --paysage : choisir l'appareil simule SANS cliquer.
				// C'est ce qui rend une capture d'ecran REPRODUCTIBLE, donc
				// comparable d'une version a l'autre — un reglage qu'on ne peut
				// atteindre qu'a la souris ne se verifie jamais en automatique.
				if (args[i].StartsWith("--profil=")) {
					const int32 n = NkString(args[i].SubStr(9)).ToInt32();
					mProfil = (n >= 0 && n < NkNbProfils()) ? n : 0;
					continue;
				}
				if (args[i] == "--paysage") {
					mPaysage = true;
					continue;
				}
				if (args[i] == "--simuler") {
					mSimuler = true;
					continue;
				}
				if (args[i] == "--selftest") {
					// L'editeur n'a pas de regles a lui : ce qu'il y aurait a
					// verifier appartient a Unkeny. On le DIT plutot que de
					// rendre un vert qui ne mesure rien — un banc vide est pire
					// qu'un banc absent, parce qu'on lui fait confiance.
					logger.Infof("[banc] l'editeur n'a pas de banc propre : ses regles vivent dans Unkeny.\n");
					logger.Infof("[banc] INDETERMINE (aucun cas)\n");
					return NkOptional<int>(0);
				}
			}
			return NkOptional<int>();
		}

		bool NkEditeurApp::OnGuiInit() {
			NkSceneConfig cfg;
			cfg.physique = true; // l'editeur exerce la physique : c'est son role
			cfg.gravite = NkVec2f(0.f, -9.81f);
			if (!mScene.Init(cfg)) {
				return false;
			}
			mCarte.Creer(40, 24, 1.f);
			mCarte.AjouterCouche(0, 1.f);
			mCarte.PoserNature(1, NkNatureTuile::NK_SOLIDE);

			ConstruireSceneExemple();
			CadrerSurTout();
			return true;
		}

		// =====================================================================
		// Une scene d'exemple. Elle n'est pas decorative : elle EXERCE la scene,
		// les composants, la physique et le rendu des le premier lancement.
		// Un editeur qui s'ouvre sur le vide ne prouve rien du moteur.
		// =====================================================================
		void NkEditeurApp::ConstruireSceneExemple() {
			// Le sol : statique, large, sous l'origine.
			{
				const ecs::NkEntityId sol = mScene.Creer("Sol", NkVec2f(0.f, -4.f));
				NkSprite2D s;
				s.taille = NkVec2f(20.f, 1.f);
				s.couleur = 0x3E4756FFu;
				s.couche = -10;
				mScene.Monde().Add<NkSprite2D>(sol, s);

				NkCollisionneur2D c;
				c.forme = NkForme2D::NK_BOITE;
				c.demiTaille = NkVec2f(10.f, 0.5f);
				mScene.Monde().Add<NkCollisionneur2D>(sol, c);

				NkCorps2D b;
				b.type = NkTypeCorps::NK_STATIQUE;
				mScene.AjouterCorps(sol, b);
			}

			// Quelques caisses qui tombent : elles rendent la physique VISIBLE
			// des l'ouverture, sans qu'on ait rien a faire.
			for (int32 i = 0; i < 6; ++i) {
				NkString nom = NkString::Format("Caisse %d", i + 1);
				const float32 x = -3.f + static_cast<float32>(i) * 1.2f;
				const float32 y = 1.f + static_cast<float32>(i % 3) * 1.6f;
				const ecs::NkEntityId e = mScene.Creer(nom.Data(), NkVec2f(x, y));

				NkSprite2D s;
				s.taille = NkVec2f(0.9f, 0.9f);
				s.couleur = (i % 2 == 0) ? 0xE2B028FFu : 0x3C7ACAFFu;
				mScene.Monde().Add<NkSprite2D>(e, s);

				NkCollisionneur2D c;
				c.forme = NkForme2D::NK_BOITE;
				c.demiTaille = NkVec2f(0.45f, 0.45f);
				mScene.Monde().Add<NkCollisionneur2D>(e, c);

				NkCorps2D b;
				b.type = NkTypeCorps::NK_DYNAMIQUE;
				mScene.AjouterCorps(e, b);
			}
		}

		void NkEditeurApp::CadrerSurTout() {
			mScene.Camera().Cadrer(NkVec2f(0.f, -1.f), NkVec2f(22.f, 14.f));
		}

		// =====================================================================
		void NkEditeurApp::OnLayout(const renderer::NkLayoutInfo &info) {
			renderer::NkCanvasGuiApp::OnLayout(info);
			const NkProfilAppareil p = mPaysage ? NkTourner(NkProfil(mProfil)) : NkProfil(mProfil);
			mDispo.Calculer(info, p);
			// ⚠️ Le viseur de la VUE est l'aire d'appareil, pas la fenetre : sans
			// cela, la scene se dessine derriere les panneaux et le cadrage ment
			// sur ce que verrait un telephone.
			mScene.Camera().PoserViseur(mDispo.appareil);
		}

		// =====================================================================
		void NkEditeurApp::PoserEntite(const NkVec2f &monde) {
			mGraine = mGraine * 1664525u + 1013904223u;
			const uint32 teinte = 0x40404000u | ((mGraine >> 8) & 0x00BFBFBFu) | 0xFFu;

			const ecs::NkEntityId e = mScene.Creer("Entite", monde);
			NkSprite2D s;
			s.taille = NkVec2f(0.8f, 0.8f);
			s.couleur = teinte;
			mScene.Monde().Add<NkSprite2D>(e, s);

			NkCollisionneur2D c;
			c.forme = NkForme2D::NK_BOITE;
			c.demiTaille = NkVec2f(0.4f, 0.4f);
			mScene.Monde().Add<NkCollisionneur2D>(e, c);

			NkCorps2D b;
			b.type = NkTypeCorps::NK_DYNAMIQUE;
			mScene.AjouterCorps(e, b);

			mSelection = e;
			mADesSelection = true;
		}

		void NkEditeurApp::SupprimerSelection() {
			if (!mADesSelection) {
				return;
			}
			mScene.Detruire(mSelection);
			mADesSelection = false;
			mDeplace = false;
		}

		// =====================================================================
		bool NkEditeurApp::OnPointer(const NkPointer &p) {
			const NkVec2f pos(p.x, p.y);

			// 1. La barre d'outils passe AVANT le viseur : elle est dessinee
			//    par-dessus, elle doit donc recevoir le clic en premier. L'ordre
			//    du dessin et l'ordre des entrees doivent etre INVERSES l'un de
			//    l'autre — c'est ce qu'on oublie, et le bouton devient inerte.
			if (p.phase == NkPointerPhase::NK_POINTER_UP && NkDansRect(mDispo.barre, pos)) {
				NkActionBarre act = NkBarreClic(mDispo, mTheme, pos, mOutil, mProfil, mPaysage, mSimuler,
												mVoirCollisionneurs, mVoirGrille);
				if (act == NkActionBarre::NK_CADRER) {
					CadrerSurTout();
				} else if (act == NkActionBarre::NK_SUPPRIMER) {
					SupprimerSelection();
				} else if (act == NkActionBarre::NK_PROFIL_CHANGE) {
					OnLayout(Layout()); // le rapport d'ecran a change : on recalcule
				}
				return true;
			}

			// 2. La hierarchie.
			if (p.phase == NkPointerPhase::NK_POINTER_UP && NkDansRect(mDispo.hierarchie, pos)) {
				ecs::NkEntityId choisi;
				if (NkHierarchieClic(mDispo.hierarchie, mScene, pos, choisi)) {
					mSelection = choisi;
					mADesSelection = true;
				}
				return true;
			}

			// 3. Le viseur.
			if (!NkDansRect(mDispo.viseur, pos)) {
				return false;
			}
			const NkVec2f monde = mScene.Camera().EcranVersMonde(pos);

			if (p.phase == NkPointerPhase::NK_POINTER_DOWN) {
				mDernierPointeur = pos;
				if (mOutil == NkOutil::NK_POSER) {
					PoserEntite(monde);
					return true;
				}
				if (mOutil == NkOutil::NK_SELECTION) {
					ecs::NkEntityId trouve;
					NkVec2f centre;
					if (NkEntiteSous(mScene, monde, trouve, centre)) {
						mSelection = trouve;
						mADesSelection = true;
						mDeplace = true;
						// On retient le DECALAGE : sans lui, l'entite saute pour
						// se centrer sous le curseur des le premier pixel.
						mDecalageSaisie = NkVec2f(centre.x - monde.x, centre.y - monde.y);
					} else {
						mADesSelection = false;
						mPanoramique = true; // clic dans le vide = on deplace la vue
					}
					return true;
				}
				if (mOutil == NkOutil::NK_EFFACER) {
					ecs::NkEntityId trouve;
					NkVec2f centre;
					if (NkEntiteSous(mScene, monde, trouve, centre)) {
						mScene.Detruire(trouve);
						mADesSelection = false;
					}
					return true;
				}
			}

			if (p.phase == NkPointerPhase::NK_POINTER_MOVE) {
				if (mDeplace && mADesSelection) {
					// ⚠️ On TELEPORTE : poser directement le transform laisserait
					// le solveur a l'ancienne position, et l'entite reviendrait
					// d'un coup au pas suivant.
					mScene.TeleporterEntite(mSelection,
											NkVec2f(monde.x + mDecalageSaisie.x, monde.y + mDecalageSaisie.y));
					return true;
				}
				if (mPanoramique) {
					// Le panoramique se calcule en MONDE : en pixels, il irait
					// plus ou moins vite selon le zoom.
					const NkVec2f avant = mScene.Camera().EcranVersMonde(mDernierPointeur);
					const NkVec2f c = mScene.Camera().Centre();
					mScene.Camera().PoserCentre(NkVec2f(c.x - (monde.x - avant.x), c.y - (monde.y - avant.y)));
					mDernierPointeur = pos;
					return true;
				}
			}

			if (p.phase == NkPointerPhase::NK_POINTER_UP || p.phase == NkPointerPhase::NK_POINTER_CANCEL) {
				mDeplace = false;
				mPanoramique = false;
			}
			return true;
		}

		// =====================================================================
		bool NkEditeurApp::OnKeyPress(const NkKeyPressEvent &event) {
			(void)event;
			return false;
		}

		void NkEditeurApp::OnTick(float32 deltaTime) {
			// ⚠️ La physique n'avance QUE si on l'a demandee. Un editeur qui
			// simule en permanence ne permet pas de POSER quoi que ce soit :
			// l'objet tombe avant qu'on ait lache le bouton.
			if (mSimuler) {
				mScene.Pas(deltaTime);
			}
		}

		// =====================================================================
		void NkEditeurApp::OnDraw(nkgui::NkGuiDrawList &dl) {
			const renderer::NkLayoutInfo &info = Layout();
			dl.AddRectFilled(nkgui::NkRect{0.f, 0.f, static_cast<float32>(info.width),
										   static_cast<float32>(info.height)},
							 mTheme.fond);

			const NkProfilAppareil profil = mPaysage ? NkTourner(NkProfil(mProfil)) : NkProfil(mProfil);

			mStats = NkDessinerViseur(dl, mScene, mDispo, mTheme, profil, mVoirGrille, mVoirCollisionneurs,
									  mADesSelection ? &mSelection : nullptr);

			NkDessinerHierarchie(dl, mDispo.hierarchie, FontSmall(), mScene, mTheme,
								 mADesSelection ? &mSelection : nullptr);
			NkDessinerInspecteur(dl, mDispo.inspecteur, FontSmall(), FontBody(), mScene, mTheme,
								 mADesSelection ? &mSelection : nullptr);
			NkDessinerBarre(dl, mDispo, FontSmall(), mTheme, mOutil, profil, mPaysage, mSimuler, mVoirCollisionneurs,
							mVoirGrille);
			NkDessinerEtat(dl, mDispo.etat, FontSmall(), mTheme, mScene, mStats, profil);
		}

	} // namespace editeur
} // namespace nkentseu
