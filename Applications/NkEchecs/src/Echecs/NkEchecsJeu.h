// =============================================================================
// NkEchecsJeu.h — la classe application
//
// A QUOI SERT CE FICHIER
//   Il fait le lien : il tient la partie (NkEchecsRegles), calcule la geometrie
//   et appelle le dessin (NkEchecsEcran), et recoit les entrees. C'est le SEUL
//   fichier du jeu qui a le droit de modifier l'etat.
//
// CE QU'IL NE FAIT PAS : ni fenetre, ni boucle, ni pointeur souris/doigt, ni
//   zone sure, ni rotation, ni cycle de vie mobile. Tout cela vient de
//   NkCanvasGuiApp.
//
// LES MODES : chaque siege a un CONTROLEUR, humain ou IA.
//   humain / IA      le defaut
//   humain / humain  a deux sur le meme ecran   (--mode=duo)
//   IA / IA          simulation                 (--mode=ia)
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - une regle du jeu   -> NkEchecsRegles
//   - un element visuel  -> NkEchecsEcran
//   - un cas de banc     -> NkEchecsBanc
//   - un enchainement    -> ici
// =============================================================================
#pragma once

#include "Echecs/NkEchecsEcran.h"
#include "Echecs/NkEchecsRegles.h"
#include "NKCanvas/App/NkCanvasGuiApp.h"

namespace nkentseu {
	namespace jeux {
		namespace echecs {

			class NkEchecsJeu : public renderer::NkCanvasGuiApp {
				public:
					NkEchecsJeu();

				protected:
					NkOptional<int> OnCommandLine(const NkVector<NkString> &args) override;
					bool OnGuiInit() override;
					void OnLayout(const renderer::NkLayoutInfo &info) override;
					bool OnPointer(const NkPointer &p) override;
					void OnTick(float32 deltaTime) override;
					void OnDraw(nkgui::NkGuiDrawList &dl) override;

				private:
					void Rejouer();
					void ChoisirMode(NkMode mode);
					/// Arme l'animation d'un coup DEJA joue. `piece` est lue AVANT
					/// que le coup soit applique : ensuite la case de depart est
					/// vide, et une promotion a change le type — on verrait une
					/// dame traverser l'echiquier au lieu du pion qui la devient.
					void ArmerAnimation(const NkEchecsCoup &coup, NkEchecsPiece piece, NkEchecsPiece prise,
										int32 priseR, int32 priseC);
					void VerifierFin();
					void AppliquerMode(const NkString &mode);
					NkEchecsVue Vue() const;

					int32 TraitIndex() const noexcept {
						return mPartie.Trait() == NkEchecsCamp::NK_BLANC ? 0 : 1;
					}
					bool EstHumain(int32 siege) const noexcept {
						return mControleur[siege] == NkControleur::NK_HUMAIN;
					}

					NkEchecsPartie mPartie;
					NkVector<NkEchecsCoup> mCoupsProposes;
					NkEchecsGeometrie mGeo;
					NkEcran mEcran = NkEcran::NK_MENU; ///< on commence par CHOISIR
					NkEchecsAnim mAnim;

					NkControleur mControleur[2] = {NkControleur::NK_HUMAIN, NkControleur::NK_IA};

					int32 mSelR = -1, mSelC = -1;
					bool mFinie = false;
					NkEchecsEtat mEtat = NkEchecsEtat::NK_EN_COURS;
					float32 mAttenteIA = 0.f;
					uint32 mGraine = 20260901u;
			};

		} // namespace echecs
	} // namespace jeux
} // namespace nkentseu
