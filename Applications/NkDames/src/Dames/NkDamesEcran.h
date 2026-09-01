// =============================================================================
// NkDamesEcran.h — geometrie et dessin, SANS etat de jeu
//
// A QUOI SERT CE FICHIER
//   Il calcule ou vont les choses (NkDamesGeometrie) et il les dessine
//   (les fonctions Dessiner*). Il ne DECIDE rien : il ne sait ni de qui c'est
//   le tour, ni si un coup est legal. Tout ce dont il a besoin lui est donne
//   dans NkDamesVue.
//
// POURQUOI CETTE SEPARATION
//   Parce qu'elle rend chaque fichier lisible seul. Le dessin ne peut pas
//   modifier la partie par megarde, et une regle ne peut pas dependre d'un
//   pixel. C'est aussi ce qui permettra, plus tard, de rendre une position sans
//   ouvrir de fenetre — pour une illustration ou un banc d'image.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - un element visuel     -> une fonction Dessiner* ici
//   - une zone cliquable    -> un NkRect dans NkDamesGeometrie, calcule dans
//                              Calculer() ; le test du clic reste chez le jeu
//   - une couleur           -> NkDamesTheme.h, jamais ici
// =============================================================================
#pragma once

#include "Dames/NkDamesRegles.h"
#include "NKCanvas/App/NkCanvasApp.h"
#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiFont.h"

namespace nkentseu {
	namespace jeux {
		namespace dames {

			using nkgui::NkGuiDrawList;
			using nkgui::NkGuiFont;
			using nkgui::NkRect;
			using math::NkVec2f;

			/// Qui tient un siege. C'est la SEULE notion de mode : "contre
			/// l'ordinateur", "a deux" et "simulation" ne sont que trois
			/// configurations de ce tableau. Deux booleens separes auraient
			/// diverge au premier ajout.
			enum class NkControleur : uint8 { NK_HUMAIN = 0, NK_IA };

			/// Une piece en mouvement. L'etat de la partie est DEJA a jour : ceci
			/// ne decrit que ce qu'on montre pendant que l'oeil rattrape.
			///
			/// ⚠️ `nbEtapes` peut valoir plus de 1 : une rafle enchaine les
			/// prises, et le pion doit passer PAR chaque case d'atterrissage.
			/// Une droite du depart a l'arrivee le ferait traverser le damier.
			struct NkDamesAnim {
					bool actif = false;
					float32 t = 0.f;	  ///< 0..1 sur la duree totale
					float32 duree = 0.f;  ///< secondes
					NkDamesPiece piece = NkDamesPiece::NK_VIDE;
					int8 depR = 0, depC = 0;
					uint8 nbEtapes = 0;
					int8 etapeR[NK_DAMES_MAX_PRISES] = {};
					int8 etapeC[NK_DAMES_MAX_PRISES] = {};

					/// Position courante, en coordonnees de CASE (fractionnaires).
					/// On interpole en cases et non en pixels : une rotation de
					/// l'ecran pendant l'animation deplacerait sinon la piece.
					void Position(float32 &r, float32 &c) const noexcept;

					/// La case ou la piece se trouve DEJA dans les regles, et que
					/// le dessin doit donc sauter tant que l'animation court.
					void CaseFinale(int32 &r, int32 &c) const noexcept;

					// --- Les pieces PRISES, gardees en image ------------------
					// ⚠️ Les regles les ont deja retirees du damier. Sans cette
					// copie, elles disparaissent AVANT que l'attaquant les
					// franchisse, et la rafle se lit a l'envers.
					uint8 nbPrises = 0;
					int8 prisR[NK_DAMES_MAX_PRISES] = {};
					int8 prisC[NK_DAMES_MAX_PRISES] = {};
					NkDamesPiece prisPiece[NK_DAMES_MAX_PRISES] = {};

					/// La prise numero `i` se situe entre l'etape i-1 et l'etape i :
					/// elle s'efface quand l'attaquant atteint le MILIEU de ce
					/// segment, c'est-a-dire au moment ou il passe par-dessus.
					bool PriseVisible(uint8 i) const noexcept {
						if (nbEtapes == 0) {
							return false;
						}
						const float32 avance = t * static_cast<float32>(nbEtapes);
						return avance < static_cast<float32>(i) + 0.55f;
					}
			};

			/// L'ecran affiche. Deux seulement : on choisit, puis on joue.
			enum class NkEcran : uint8 { NK_MENU = 0, NK_PARTIE };

			/// Les trois modes proposes au menu. Ils ne sont qu'un raccourci vers
			/// une configuration de sieges — le mode n'est PAS stocke, sinon il
			/// pourrait contredire les bascules du pied de page.
			enum class NkMode : uint8 { NK_CONTRE_ORDI = 0, NK_A_DEUX, NK_IA_CONTRE_IA };


			// =====================================================================
			// Geometrie — tout ce qui se calcule a partir de la taille de l'ecran
			// =====================================================================
			struct NkDamesGeometrie {
					NkRect plateau{0.f, 0.f, 0.f, 0.f}; ///< le damier seul, carre
					NkRect bandeau{0.f, 0.f, 0.f, 0.f}; ///< l'entete
					NkRect siege[2]{{0.f, 0.f, 0.f, 0.f}, {0.f, 0.f, 0.f, 0.f}};
					NkRect rejouer{0.f, 0.f, 0.f, 0.f};
					NkRect retour{0.f, 0.f, 0.f, 0.f};	 ///< retour au menu, dans le bandeau
					NkRect choix[3]{{0.f, 0.f, 0.f, 0.f}, {0.f, 0.f, 0.f, 0.f}, {0.f, 0.f, 0.f, 0.f}};
					NkRect menuTitre{0.f, 0.f, 0.f, 0.f};
					NkRect menuSousTitre{0.f, 0.f, 0.f, 0.f};
					float32 cellule = 0.f;

					/// Recalcule tout depuis la mise en page. Appelee a chaque
					/// changement de taille — rotation comprise.
					void Calculer(const renderer::NkLayoutInfo &info) noexcept;

					NkVec2f CentreCase(int32 r, int32 c) const noexcept {
						return NkVec2f(plateau.x + (static_cast<float32>(c) + 0.5f) * cellule,
									   plateau.y + (static_cast<float32>(r) + 0.5f) * cellule);
					}
					NkRect CaseRect(int32 r, int32 c) const noexcept {
						return NkRect{plateau.x + static_cast<float32>(c) * cellule,
									  plateau.y + static_cast<float32>(r) * cellule, cellule, cellule};
					}
					/// Rend false quand le point tombe HORS du damier : un clic
					/// dans le bandeau ne doit pas se traduire par une case valide.
					bool CaseSous(const NkVec2f &p, int32 &r, int32 &c) const noexcept;
			};

			bool NkDansRect(const NkRect &r, const NkVec2f &p) noexcept;

			// =====================================================================
			// Vue — tout ce que le dessin a besoin de savoir, et rien de plus
			// =====================================================================
			struct NkDamesVue {
					const NkDamesPartie *partie = nullptr;
					const NkVector<NkDamesCoup> *coupsProposes = nullptr;
					const NkControleur *controleur = nullptr; ///< tableau de 2
					const NkDamesAnim *anim = nullptr;
					int32 selR = -1, selC = -1;
					bool finie = false;
					NkDamesCamp gagnant = NkDamesCamp::NK_BLANC;
			};

			/// Les trois polices dont le dessin se sert. Regroupees pour ne pas
			/// trainer trois parametres de plus dans chaque signature.
			struct NkDamesPolices {
					NkGuiFont *titre = nullptr;
					NkGuiFont *corps = nullptr;
					NkGuiFont *petite = nullptr;
			};

			void DessinerFond(NkGuiDrawList &dl, const renderer::NkLayoutInfo &info);
			void DessinerMenu(NkGuiDrawList &dl, const renderer::NkLayoutInfo &info, const NkDamesGeometrie &geo,
							  const NkDamesPolices &f);
			void DessinerBandeau(NkGuiDrawList &dl, const NkDamesGeometrie &geo, const NkDamesPolices &f,
								 const NkDamesVue &vue);
			void DessinerDamier(NkGuiDrawList &dl, const NkDamesGeometrie &geo, const NkDamesVue &vue);
			void DessinerPieces(NkGuiDrawList &dl, const NkDamesGeometrie &geo, const NkDamesVue &vue);
			void DessinerPiedDePage(NkGuiDrawList &dl, const NkDamesGeometrie &geo, const NkDamesPolices &f,
									const NkDamesVue &vue);
			void DessinerFin(NkGuiDrawList &dl, const renderer::NkLayoutInfo &info, const NkDamesPolices &f,
							 const NkDamesVue &vue);

		} // namespace dames
	} // namespace jeux
} // namespace nkentseu
