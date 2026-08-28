// =============================================================================
//  NkUIDesign — LA VUE DE TOILE : la SEULE traduction entre deux espaces
// =============================================================================
//
//  ⚠️ DEUX ESPACES VONT COEXISTER DES QUE LE ZOOM EXISTE, ET ILS SE MELANGENT
//     SANS BRUIT. On les nomme ici, une fois, et rien d'autre ne les traduit :
//
//       ESPACE DOCUMENT — les coordonnees que le fichier porte (`posX`/`posY`,
//                         les tailles). Elles ne bougent JAMAIS quand on zoome
//                         ou qu'on se deplace : c'est ce que le concepteur a
//                         ecrit, et c'est ce qui sera enregistre.
//
//       ESPACE ECRAN    — les pixels du panneau. Ils changent a chaque coup de
//                         molette et a chaque glissement.
//
//  **Un pixel n'a de sens que rapporte a l'espace dans lequel on le lit.** Un
//  `float32` ne dit pas d'ou il vient ; c'est le NOM de la fonction qui le dit.
//  D'ou `ToScreen*` / `ToDoc*` et jamais de calcul a la main ailleurs.
//
//  ⚠️ POURQUOI CE FICHIER EST PUR (aucun dessin, aucun contexte d'interface) :
//     pour que la sonde le mesure SANS ouvrir de fenetre. La justesse d'un zoom
//     ne se verifie pas a l'oeil — elle se verifie sur des nombres ecrits a la
//     main.
//
//  ⚠️ ET LE PIEGE QUE CE FICHIER PORTE, ECRIT ICI PARCE QU'IL EST INVISIBLE :
//     **un aller-retour `ToDoc(ToScreen(p)) == p` est satisfait par l'IDENTITE.**
//     Une vue qui ignorerait completement zoom et deplacement passerait un tel
//     controle sans broncher. C'est la meme forme que le temoin de bruit qui
//     comparait deux sorties de la meme fonction. Les controles de la famille 39
//     ancrent donc `ToScreen` sur des valeurs CALCULEES A LA MAIN, et 39c
//     verifie explicitement que la vue n'est PAS l'identite.
// =============================================================================

#ifndef __NKENTSEU_NKUIDESIGN_CANVAS_H__
#define __NKENTSEU_NKUIDESIGN_CANVAS_H__

#include "NKCore/NkTypes.h"
#include "NKEditorKit/Components/NkComponentPaint.h"
#include "Document.h"

namespace nkuidesign {

	using nkentseu::float32;
	using nkentseu::editorkit::NkPaintRect;

	struct NkCanvasView {
			/// Facteur d'echelle. 1 = un pixel document vaut un pixel ecran.
			float32 zoom = 1.f;
			/// Deplacement, en pixels ECRAN. Ajoute apres l'echelle.
			float32 panX = 0.f;
			float32 panY = 0.f;
			/// Le rectangle ECRAN dans lequel la toile est dessinee.
			NkPaintRect viewport = {0.f, 0.f, 0.f, 0.f};

			/// Bornes du zoom. Un zoom nul ou negatif rendrait `ToDoc` infini ou
			/// inverse ; on l'interdit a la source plutot que de s'en proteger
			/// partout.
			static float32 MinZoom() {
				return 0.05f;
			}
			static float32 MaxZoom() {
				return 32.f;
			}

			// ── DOCUMENT -> ECRAN ────────────────────────────────────────────
			float32 ToScreenX(float32 docX) const {
				return viewport.x + panX + docX * zoom;
			}
			float32 ToScreenY(float32 docY) const {
				return viewport.y + panY + docY * zoom;
			}
			/// Un rectangle entier. La TAILLE est mise a l'echelle, elle ne subit
			/// pas le deplacement — c'est une longueur, pas une position.
			NkPaintRect ToScreen(const NkPaintRect &d) const {
				NkPaintRect s;
				s.x = ToScreenX(d.x);
				s.y = ToScreenY(d.y);
				s.w = d.w * zoom;
				s.h = d.h * zoom;
				return s;
			}

			// ── ECRAN -> DOCUMENT ────────────────────────────────────────────
			float32 ToDocX(float32 screenX) const {
				return (screenX - viewport.x - panX) / zoom;
			}
			float32 ToDocY(float32 screenY) const {
				return (screenY - viewport.y - panY) / zoom;
			}
			/// Une LONGUEUR ecran rendue en longueur document : pas de
			/// deplacement, seulement l'echelle. Sert aux seuils de saisie (un
			/// rayon de poignee en pixels ecran devient une tolerance document).
			float32 ToDocLength(float32 screenLen) const {
				return screenLen / zoom;
			}

			// ── LE ZOOM AUTOUR D'UN POINT ────────────────────────────────────
			/// ⚠️ C'EST ICI QUE LES DEUX ESPACES SE MELANGENT, ET C'EST LE SEUL
			///    ENDROIT. Zoomer a la molette doit laisser IMMOBILE le point du
			///    document qui se trouve sous le curseur — sinon le contenu fuit
			///    sous la souris et l'outil devient impraticable.
			///
			///    On lit le point document AVANT, on change l'echelle, puis on
			///    recale le deplacement pour que ce meme point retombe sous le
			///    meme pixel. Oublier le recalage donne un zoom qui « part » vers
			///    le coin : c'est le defaut le plus courant de toute toile, et il
			///    ne se voit pas sur une capture fixe.
			void ZoomAt(float32 factor, float32 screenX, float32 screenY) {
				const float32 docX = ToDocX(screenX);
				const float32 docY = ToDocY(screenY);
				float32 z = zoom * factor;
				if (z < MinZoom())
					z = MinZoom();
				if (z > MaxZoom())
					z = MaxZoom();
				zoom = z;
				panX = screenX - viewport.x - docX * zoom;
				panY = screenY - viewport.y - docY * zoom;
			}

			/// Deplacement a la souris : un glissement de N pixels ECRAN deplace
			/// la vue de N pixels ecran, quel que soit le zoom. C'est voulu — la
			/// main suit le curseur, pas le document.
			void PanBy(float32 screenDX, float32 screenDY) {
				panX += screenDX;
				panY += screenDY;
			}
	};

} // namespace nkuidesign

#endif // __NKENTSEU_NKUIDESIGN_CANVAS_H__
