#pragma once
// -----------------------------------------------------------------------------
// @File    NkFamilleEditeur.h
// @Brief   LA TRAME D'UN EDITEUR DE LA FAMILLE, prete a deriver : le contexte
//          NKGui, ses trois polices, le theme, l'entree, le plan d'UE5, la
//          fenetre sans cadre, les menus -- et l'ORDRE de dessin d'UnkenyEditor
//          (NkEditeurDessinerTrame) : bords, corps (gestes neutralises sous un
//          menu), cloisons, barre d'outils, barre d'etat, onglets, puis barre de
//          titre et menus avec l'entree reelle, puis les raccourcis.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POUR QUI : tout editeur qui veut l'apparence d'UnkenyEditor (R32) sans
// recopier sa trame -- NkAnimaEditor (01/10), et ceux qui suivront (NKScena,
// PV3DE...). L'application derive, peint SON corps (sa vue, ses panneaux avec
// les pieces de la famille), remplit SES menus et execute SES actions.
//
// CE QUE CETTE CLASSE NE FAIT PAS : rendre. Elle produit UNE liste de dessin
// fusionnee par image (`Trame`), que l'hote soumet a SON dorsal (NkGuiRHIBackend,
// NkEditorRHIRenderer, NKCanvas) ; elle donne ses polices a televerser
// (`Polices`). Elle ne connait ni NKRenderer ni NKCanvas.
//
// UTILISATION
//   class MonEditeur : public NkFamilleEditeur { ...hooks... };
//   MonEditeur e;  e.InitialiserGui(W, H);  (televerser e.Polices(...))
//   chaque evenement : e.LireEvenement(*ev);
//   chaque image     : const auto &dl = e.Trame(dt, W, H); soumettre(dl);
//                      e.AppliquerFenetre(fenetre);
// -----------------------------------------------------------------------------

#include "NKEditorKit/Famille/NkFamilleCadre.h"
#include "NKEditorKit/Famille/NkFamilleEntree.h"
#include "NKEditorKit/NkTheme.h"
#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiFont.h"

namespace nkentseu {
	namespace editorkit {

		class NkFamilleEditeur {
			public:
				NkFamilleEditeur() noexcept;
				virtual ~NkFamilleEditeur();
				NkFamilleEditeur(const NkFamilleEditeur &) = delete;
				NkFamilleEditeur &operator=(const NkFamilleEditeur &) = delete;

				// ── La mise en route ──────────────────────────────────────────
				/// Le contexte NKGui, les polices de la famille (DroidSans `corpsPx`,
				/// la petite a 0,78, une chasse fixe pour le terminal) et le theme.
				bool InitialiserGui(int32 largeur, int32 hauteur, float32 corpsPx = 13.f);
				void Terminer();
				bool Prete() const noexcept {
					return mPret;
				}
				nkgui::NkGuiContext &Gui() noexcept {
					return mCtx;
				}
				/// Les polices a televerser par l'hote (atlas gray8, texId distincts).
				int32 Polices(nkgui::NkGuiFont **sortie, int32 maximum) noexcept;
				nkgui::NkGuiFont *PoliceCorps() noexcept {
					return &mPolice;
				}
				nkgui::NkGuiFont *PolicePetite() noexcept {
					return &mPetite;
				}
				nkgui::NkGuiFont *PoliceMono() noexcept {
					return mMono.Valid() ? &mMono : &mPolice;
				}
				void PoserTheme(bool clair);
				bool Clair() const noexcept {
					return mClair;
				}

				// ── Chaque image ──────────────────────────────────────────────
				void LireEvenement(const NkEvent &e) noexcept;
				/// La trame entiere ; rend la liste FUSIONNEE (contenu + surcouche) a
				/// soumettre UNE fois. `fenetreAgrandie` : NkWindow::IsMaximized.
				const nkgui::NkGuiDrawList &Trame(float32 dt, int32 largeur, int32 hauteur, bool fenetreAgrandie);
				/// Les demandes de la barre de titre et des bords, HORS du dessin.
				void AppliquerFenetre(NkWindow &f);
				bool QuitterDemande() const noexcept {
					return mQuitter;
				}
				void DemanderQuitter() noexcept {
					mQuitter = true;
				}
				/// Le curseur que la trame demande, pour la fenetre.
				NkWindow::NkCursorType Curseur() const noexcept {
					return NkFamilleCurseur(mCtx.wantCursor);
				}

				// ── L'etat partage avec l'application ─────────────────────────
				NkFamillePlan plan;
				NkFamilleFenetre fenetre;
				NkFamilleMenus menus;
				NkTheme theme;
				NkFamillePalette pal;
				float32 temps = 0.f; ///< secondes depuis le lancement
				float32 ips = 0.f;	 ///< images par seconde, lissees

			protected:
				// ── Ce que l'application peint ────────────────────────────────
				/// La vue et les panneaux (Placer, Outliner, Details, tiroir...), dans
				/// `plan`. Appele avec les gestes neutralises si un menu est ouvert.
				virtual void PeindreCorps(NkFamilleCtx &c) = 0;
				/// La barre d'outils (NkFamilleBoutonOutil, NkFamilleBoutonsLecture...).
				virtual void PeindreBarreOutils(NkFamilleCtx &c) = 0;
				/// La barre d'etat (defaut : la pastille ÉDITION et les ips).
				virtual void PeindreStatut(NkFamilleCtx &c);
				/// Les onglets de scene (defaut : aucun).
				virtual void PeindreOnglets(NkFamilleCtx &c);
				/// Le titre au centre de la barre de titre.
				virtual NkString Titre() const = 0;
				/// Les menus de la barre de titre (defaut : Fichier, Édition, Fenêtre, Aide).
				virtual const char *const *MenusBarre(int32 &nombre) const;
				/// Le logo du coin (defaut : rien -- la place existe).
				virtual void PeindreLogo(nkgui::NkGuiDrawList &dl, float32 x, float32 y, float32 cote, bool fondSombre);
				/// Les entrees du menu `menu` ; l'action choisie revient par `Executer`.
				virtual void Remplir(int32 menu, NkVector<NkFamilleEntreeMenu> &sortie) = 0;
				virtual void Executer(int32 action) = 0;
				/// Les raccourcis, APRES le dessin (defaut : Echap ferme le menu).
				virtual void Raccourcis(NkFamilleCtx &c);
				/// Avant le corps (journal, demandes de l'application...).
				virtual void AvantCorps(NkFamilleCtx &c);
				/// La croix de la barre de titre (defaut : DemanderQuitter).
				virtual void Fermer();
				/// Une fenetre MODALE est-elle ouverte (le selecteur de fichiers, une
				/// question) ? Alors ni le corps ni les menus ne recoivent la souris,
				/// et les raccourcis se taisent (UnkenyEditor, NkEditeurTrame).
				virtual bool Modale() const;
				/// La fenetre modale, peinte PAR-DESSUS tout avec l'entree reelle.
				virtual void PeindreModale(NkFamilleCtx &c);

			private:
				static void RemplirRappel(void *user, int32 menu, NkVector<NkFamilleEntreeMenu> &sortie);
				static void LogoRappel(nkgui::NkGuiDrawList &dl, float32 x, float32 y, float32 cote, bool fondSombre, void *user);

				nkgui::NkGuiContext mCtx;
				nkgui::NkGuiFont mPolice, mPetite, mMono;
				NkFamilleEntree mEntree;
				nkgui::NkGuiDrawList mFusion;
				bool mPret = false;
				bool mClair = false;
				bool mQuitter = false;
				float32 mTempsIps = 0.f;
				int32 mTramesIps = 0;
		};

	} // namespace editorkit
} // namespace nkentseu
