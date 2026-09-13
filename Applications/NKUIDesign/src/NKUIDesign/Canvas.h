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

			// ── LES ZOOMS NOMMES (Ctrl+0..4, source `/canvas`) ───────────────
			/// ZOOM 100 % — l'echelle 1, sans perdre ce qu'on regardait.
			/// ⚠️ ET C'EST LA MOITIE QUI COMPTE : remettre `zoom = 1` en laissant
			///    `panX/panY` tels quels ferait SAUTER le document hors de l'ecran
			///    des qu'on revient d'un zoom fort. On passe donc par `ZoomAt` sur
			///    le CENTRE du viewport — ce zoom-ci n'est qu'un cas particulier
			///    de celui de la molette, et il herite de son recalage.
			void Zoom100() {
				ZoomAt(1.f / (zoom > 0.f ? zoom : 1.f), viewport.x + viewport.w * 0.5f,
					   viewport.y + viewport.h * 0.5f);
			}

			/// AJUSTER la vue sur un rectangle DOCUMENT. `mode` : 0 = les deux
			/// dimensions, 1 = la LARGEUR seule, 2 = la HAUTEUR seule.
			///
			/// ⚠️ TROIS GARDES, ET AUCUNE N'EST THEORIQUE :
			///   - un rectangle de taille NULLE (selection vide, document neuf)
			///     ferait une division par zero et propagerait des NaN jusqu'a
			///     `panX` — la toile disparaitrait sans message. On refuse, et le
			///     retour le DIT ;
			///   - l'echelle se BORNE comme partout ailleurs
			///     (`MinZoom`/`MaxZoom`) : ajuster sur un point minuscule
			///     demanderait un zoom de plusieurs milliers ;
			///   - une MARGE de 4 %, parce qu'un ajustement au pixel colle la
			///     forme aux quatre bords et donne l'impression qu'elle deborde.
			///
			/// ⚠️ ET LE CENTRAGE SE FAIT SUR LES DEUX AXES MEME EN MODE LARGEUR :
			///    ajuster la largeur sans recentrer verticalement laisserait la
			///    forme hors champ en hauteur. Le mode choisit l'ECHELLE, pas le
			///    cadrage.
			/// @return faux si le rectangle ne permet pas d'ajuster (rien n'est
			///         touche) — *une commande qui ne fait rien doit le dire.*
			/// Les BANDES RESERVEES du viewport (px ecran) : la barre d'outils
			/// flottante a gauche, le selecteur de zoom en bas. E3 du cote a
			/// cote (valide par Rodolf, 02/09) : un cadrage qui les ignore pose
			/// le document SOUS le mobilier. Elles s'appliquent a TOUS les
			/// ajustements (ouverture, Ctrl+0..3) -- ajuster la selection sous
			/// le rail serait le meme defaut. Posees par le panneau, qui seul
			/// connait la geometrie de son mobilier.
			float32 reserveGauche = 0.f;
			float32 reserveBas = 0.f;

			/// `zoomPlafond` (0 = aucun) : borne HAUTE demandee par l'appelant --
			/// le cadrage d'ouverture plafonne a 100 %, parce qu'ouvrir un petit
			/// artboard zoome a 400 % desoriente plus qu'il n'aide.
			/// LE VIEWPORT UTILE : le viewport ampute des bandes du mobilier.
			/// 🔑 UNE SEULE REPONSE POUR LES DEUX CADRAGES. `AjusterSur` le
			///    calculait chez lui ; `Reveler` en avait besoin du meme. Deux
			///    copies auraient diverge au premier reglage du mobilier, et
			///    l'un des deux aurait pose le document sous le rail.
			NkPaintRect ViewportUtile() const {
				NkPaintRect vp = viewport;
				if (reserveGauche > 0.f && reserveGauche < vp.w * 0.5f) {
					vp.x += reserveGauche;
					vp.w -= reserveGauche;
				}
				if (reserveBas > 0.f && reserveBas < vp.h * 0.5f)
					vp.h -= reserveBas;
				return vp;
			}

			/// AMENER L'OEIL SUR `cible` SANS TOUCHER AU ZOOM.
			///
			/// 🔴 RODOLF, 03/09 : « cliquer ou double-cliquer sur un element de
			///    la page doit nous amener ou se trouve le composant dans la
			///    grille infinie ». Selectionner sans montrer laisse la main
			///    devant une toile inchangee : l'inspecteur parle d'un objet
			///    que l'oeil ne trouve pas.
			///
			/// ⚠️ ELLE NE FAIT RIEN SI LA CIBLE EST DEJA ENTIEREMENT VISIBLE, et
			///    c'est le coeur du contrat : recentrer a chaque clic ferait
			///    sauter la toile sous la main pendant qu'on travaille — le
			///    remede serait pire que le mal. On ne bouge que pour ce qu'on
			///    ne voit pas.
			///
			/// ⚠️ ET PAS DE ZOOM : c'est `AjusterSur` qui zoome, sur un geste
			///    EXPLICITE (double-clic, `Ctrl+0..3`). Changer le zoom sur une
			///    simple selection ferait perdre l'echelle de travail.
			bool Reveler(const NkPaintRect &cible) {
				const NkPaintRect vp = ViewportUtile();
				if (!(cible.w > 0.f) || !(cible.h > 0.f) || !(vp.w > 0.f) || !(vp.h > 0.f))
					return false;
				// La cible EN PIXELS ECRAN, avec le zoom courant.
				const float32 x0 = ToScreenX(cible.x), y0 = ToScreenY(cible.y);
				const float32 x1 = x0 + cible.w * zoom, y1 = y0 + cible.h * zoom;
				if (x0 >= vp.x && y0 >= vp.y && x1 <= vp.x + vp.w && y1 <= vp.y + vp.h)
					return false; // deja sous les yeux : on ne bouge pas
				const float32 cx = cible.x + cible.w * 0.5f;
				const float32 cy = cible.y + cible.h * 0.5f;
				panX = (vp.x - viewport.x) + vp.w * 0.5f - cx * zoom;
				panY = (vp.y - viewport.y) + vp.h * 0.5f - cy * zoom;
				return true;
			}

			bool AjusterSur(const NkPaintRect &cible, nkentseu::uint32 mode = 0,
							float32 zoomPlafond = 0.f) {
				const NkPaintRect vp = ViewportUtile();
				if (!(cible.w > 0.f) || !(cible.h > 0.f) || !(vp.w > 0.f)
					|| !(vp.h > 0.f))
					return false;
				const float32 kMarge = 0.96f;
				const float32 zx = vp.w * kMarge / cible.w;
				const float32 zy = vp.h * kMarge / cible.h;
				float32 z = (mode == 1u) ? zx : (mode == 2u) ? zy : (zx < zy ? zx : zy);
				if (z < MinZoom())
					z = MinZoom();
				if (z > MaxZoom())
					z = MaxZoom();
				if (zoomPlafond > 0.f && z > zoomPlafond)
					z = zoomPlafond;
				zoom = z;
				// centrer : le centre du rectangle document tombe au centre du
				// viewport, sur les DEUX axes quel que soit le mode.
				const float32 cx = cible.x + cible.w * 0.5f;
				const float32 cy = cible.y + cible.h * 0.5f;
				// centre du viewport UTILE, exprime dans le repere du viewport
				// complet (pan est relatif au viewport entier).
				panX = (vp.x - viewport.x) + vp.w * 0.5f - cx * zoom;
				panY = (vp.y - viewport.y) + vp.h * 0.5f - cy * zoom;
				return true;
			}
	};

} // namespace nkuidesign

#endif // __NKENTSEU_NKUIDESIGN_CANVAS_H__
