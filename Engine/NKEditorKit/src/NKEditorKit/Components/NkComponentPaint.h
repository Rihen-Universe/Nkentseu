#pragma once
// -----------------------------------------------------------------------------
// @File    NkComponentPaint.h
// @Brief   Le PEINTRE vu par un composant : une interface, pas une implementation.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  ⚠️ CE FICHIER N'EST PAS LE PEINTRE. C'EST LE CONTRAT DE SA RECEPTION.
// =============================================================================
//  Le peintre partage (palier 1 de `ROADMAP.md` §6) est extrait par l'agent
//  NK3DModeler depuis `NkModelerUI.h` (`NkModelerPainter`, 571 l.). Je le
//  RECOIS, je ne le prends pas — c'est la sequence acceptee le 18/08.
//
//  Ce fichier est donc ce qui manquait pour que la reception ait un sens : une
//  INTERFACE que son peintre satisfera, et sur laquelle les composants peuvent
//  s'ecrire des maintenant sans attendre le demenagement. Un composant ecrit
//  contre cette interface ne changera pas d'une ligne le jour ou l'implementation
//  arrivera — seule la classe passee en argument changera.
//
//  ⚠️ CE QUE J'AI ECRIT ICI EN ATTENDANT, ET QUI DISPARAIT A SON ARRIVEE :
//     `NkGuiComponentPaint` — un adaptateur MINCE sur la liste d'affichage de
//     NKGui (aucune geometrie nouvelle, aucune decision de rendu). Il existe
//     parce qu'une interface sans implementation ne se compile pas contre un
//     appelant reel, et qu'une declaration sans consommateur est exactement le
//     defaut que cette tranche existe pour eviter. Le jour ou le peintre de
//     NK3DModeler arrive, il devient la seconde implementation — et si elle est
//     meilleure, l'adaptateur s'efface. **Il ne doit surtout pas grossir.**
//
// =============================================================================
//  LES DEUX EXIGENCES DE RECEPTION (Q60 §5), ECRITES DANS LA SIGNATURE
// =============================================================================
//  Elles ne sont pas des voeux : elles sont dans le TYPE, donc elles ne peuvent
//  pas etre oubliees a la reception.
//
//  A. ⚠️ **L'ECHELLE N'EST PAS ICI, ET C'EST UNE CORRECTION.** J'avais d'abord
//     ecrit `Scale()` comme methode du peintre, pour tuer la globale de
//     processus `gUiScale` (`NkModelerUI.h:47`) dont deux fenetres a DPI
//     differents partagent fatalement la valeur — l'une des deux est fausse,
//     toujours.
//
//     **Le diagnostic etait bon, le remede etait faux**, et c'est l'arbitrage
//     du 18/08 (canal NK3DModeler) qui l'a montre sur une mesure que je n'avais
//     pas faite : `S(px)` est appelee dans `NkLayout::Compute` et
//     `NkModelerTables.h` — **du code qui ne peint pas**. Faire circuler un
//     peintre jusque-la aurait cree un couplage pire que celui qu'on repare.
//
//     > **L'echelle n'appartient ni au processus ni au peintre : elle
//     > appartient a la SURFACE** (son facteur DPI). Disposition, metrique et
//     > dessin la lisent au meme endroit, et cet endroit est **une instance par
//     > fenetre**.
//
//     Elle vit donc dans `NkComponentInput::surfaceScale` ci-dessous — l'objet
//     que l'hote construit **une fois par fenetre et par image**, et qui porte
//     deja la position de souris, elle aussi exprimee dans la surface. Le
//     peintre, lui, ne connait plus l'echelle du tout.
//
//     ⚠️ TEMOIN EXIGE, ET IL EST DIFFERE : **deux fenetres a DPI differents
//        ouvertes EN MEME TEMPS doivent produire des metriques differentes
//        SIMULTANEMENT.** Un temoin sequentiel (ouvrir l'une, puis l'autre) ne
//        discrimine pas — une globale le passerait aussi. Il exige un GPU et
//        deux fenetres : il n'a PAS ete pris, et rien ici ne pretend le
//        remplacer.
//
//  B. UNE ICONE EST UNE POIGNEE OPAQUE (`uint16`), JAMAIS UNE ENUMERATION.
//     `NkIcon` est le vocabulaire de NK3DModeler (102 glyphes) ; NKCode en a 91
//     autres, en PNG. Si le kit connaissait l'enumeration d'une application, il
//     ne servirait qu'a elle. Le peintre resout la poignee ; le composant ne
//     sait pas ce qu'il dessine, et c'est ce qui le rend partageable.
//
// =============================================================================
//  POURQUOI DES COULEURS EMPAQUETEES (`uint32` 0xRRGGBBAA) ET DES ROLES
// =============================================================================
//  Ce fichier ne connait NI `NkColor`, NI `NkGuiDrawList`, NI `NkTheme` : c'est
//  ce qui lui permet d'etre inclus par un composant sans faire entrer NKGui, et
//  donc de garder valable le banc de neutralite (`ROADMAP.md` §5).
//
//  Le composant passe des ROLES (`uint16`), jamais des couleurs — c'est la
//  deuxieme exigence de Rodolf. `ColorOf` n'existe que pour les rares cas ou une
//  couleur est deja resolue en amont (la table des natures d'asset), et la forme
//  empaquetee est exactement celle que `NkTheme` stocke : une seule traduction,
//  chez l'implementation.
//
// ⚠️ CE QUI N'EST DELIBEREMENT PAS ICI : le cercle creux et le contour arrondi.
//    Ce sont des CONTOURNEMENTS de manques de NKGui (`Ring` = 2 disques,
//    `Outline` = 2 rectangles), pas des primitives. Ils appartiennent a NKGui et
//    l'agent NKGui les a en liste. `Outline` figure ci-dessous parce qu'un
//    composant en a besoin AUJOURD'HUI ; le jour ou NKGui sait arrondir un
//    contour, l'implementation change et pas un composant ne bouge. C'est
//    precisement ce que l'interface achete.
// -----------------------------------------------------------------------------

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace editorkit {

		/// Rectangle en pixels LOGIQUES (avant `Scale()`). Volontairement distinct
		/// de `NkRect` de NKGui : ce fichier ne doit rien savoir de NKGui.
		struct NkPaintRect {
				float32 x = 0.f, y = 0.f, w = 0.f, h = 0.f;

				bool Contains(float32 px, float32 py) const {
					return px >= x && py >= y && px < x + w && py < y + h;
				}
		};

		// ── L'ENTREE ────────────────────────────────────────────────────────────
		// ⚠️ CE BLOC CORRIGE LE DEVIS DU 18/08, ET IL FAUT DIRE POURQUOI.
		//    Le devis fixait la signature type a CINQ arguments :
		//        resultat Dessiner( PEINTRE, RECTANGLE, MODELE, STYLE, GREFFES )
		//    Il en manquait un, et le manque n'etait pas visible tant que la
		//    fonction restait declaree sans etre definie : **un composant ainsi
		//    signe ne peut RIEN recevoir de l'utilisateur.** Il dessine et n'entend
		//    pas. Ses crochets `onSelect` / `onDoubleClick` / `onContextMenu`
		//    seraient donc des pointeurs que rien n'appelle jamais — c'est-a-dire
		//    exactement « un parametre qui n'est pas honore », la regle du corpus
		//    proposee par l'agent Ilyana, et sous sa forme la plus couteuse :
		//    l'application les remplirait et attendrait.
		//
		//    C'est la condition « les evenements des maintenant » posee par Rodolf
		//    qui a rendu le trou visible. Sans elle, la forme se figeait a cinq et
		//    le mouvement se refaisait plus tard — precisement ce qu'il voulait
		//    eviter. **La signature type est donc a SIX arguments**, et
		//    `ROADMAP.md` §3 est corrige en consequence.
		//
		// STRUCTURE PLATE, PAS D'ABONNEMENT, PAS DE NKEvent : le meme choix que
		// `NkGuiInput` (`NkGuiInput.h:30`, NKGui n'inclut pas NKEvent, 0 include,
		// delibere). Sa vertu est mesuree et connue : elle rend le composant
		// PILOTABLE PAR N'IMPORTE QUELLE SOURCE, donc REJOUABLE par un banc — c'est
		// ce qui rend la sonde de `NKUIDesign` possible sans fenetre. L'hote
		// convertit ce qu'il a (NKGui, NKEvent, un script) vers cette structure.
		struct NkComponentInput {
				/// ⚠️ LE FACTEUR DPI DE LA SURFACE — arbitrage du 18/08, cf. le bloc
				///    A en tete de fichier. Il est ICI et pas dans le peintre parce
				///    que la disposition et les tables de metrique le lisent aussi,
				///    et qu'elles ne peignent pas. **Une instance par fenetre**, donc
				///    deux fenetres a DPI differents ont deux valeurs justes en meme
				///    temps — ce qu'une globale de processus ne peut pas offrir.
				float32 surfaceScale = 1.f;

				float32 mouseX = 0.f, mouseY = 0.f;
				float32 wheel = 0.f;
				bool mouseDown = false;		///< bouton gauche maintenu
				bool mousePressed = false;	///< front descendant, CETTE image
				bool mouseReleased = false; ///< front montant, CETTE image
				bool doubleClick = false;
				bool rightPressed = false;
				bool ctrl = false, shift = false;

				/// Une charge de glisser-deposer survole-t-elle le composant ?
				/// Le TYPE est une chaine libre — meme convention que le
				/// glisser-deposer de NKGui, dont la charge est deja opaque et
				/// typee par chaine. Le composant n'a pas besoin d'en savoir plus
				/// pour decider s'il surligne un dossier.
				const char *dragType = nullptr;
				bool dragReleased = false;
		};

		/// Alignement horizontal d'un texte dans son rectangle. `Center` existe
		/// parce que la planche du 18/08 le demande au pied de carte (ecart n.11) et
		/// que NKGui n'expose pas son aide interne — cf. `ROADMAP.md` §2.
		enum class NkTextAlign : uint8 { Left = 0, Center, Right };

		// ── L'INTERFACE ─────────────────────────────────────────────────────────
		class NkComponentPaint {
			public:
				virtual ~NkComponentPaint() = default;

				// ── Theme et metrologie ─────────────────────────────────────────
				/// Couleur d'un role, empaquetee 0xRRGGBBAA — la forme de `NkTheme`.
				virtual uint32 ColorOf(uint16 role) const = 0;
				// ⚠️ PAS DE `Scale()` ICI — voir le bloc A en tete de fichier.
				//    L'echelle appartient a la surface (`NkComponentInput`), pas au
				//    peintre. Les deux mesures ci-dessous sont des metriques de
				//    POLICE, pas de surface : la police est deja rasterisee a sa
				//    taille, il n'y a rien a remettre a l'echelle par-dessus.
				virtual float32 LineHeight() const = 0;
				virtual float32 TextWidth(const char *s) const = 0;

				// ── Primitives ──────────────────────────────────────────────────
				virtual void Fill(const NkPaintRect &r, uint16 role, float32 rounding = 0.f) = 0;
				virtual void FillColor(const NkPaintRect &r, uint32 rgba, float32 rounding = 0.f) = 0;
				/// Contour = plein puis creusement d'un pixel. Deux roles, pas un :
				/// creuser suppose de savoir quoi remettre a l'interieur.
				virtual void Outline(const NkPaintRect &r, uint16 border, uint16 inner,
									 float32 rounding = 0.f) = 0;
				/// Contour a angles vifs, sans repeindre le fond.
				virtual void OutlineSharp(const NkPaintRect &r, uint16 role) = 0;
				virtual void HLine(float32 x, float32 y, float32 w, uint16 role) = 0;
				virtual void VLine(float32 x, float32 y, float32 h, uint16 role) = 0;

				/// Texte borne par un rectangle, avec ellipse si ca deborde.
				/// ⚠️ L'ELLIPSE EST UNE OBLIGATION DE L'IMPLEMENTATION, pas une
				///    option : NKGui coupe au glyphe sans points de suite, et c'est
				///    l'ecart n.11/16 des planches. Une implementation qui coupe
				///    net respecte la signature et trahit le contrat — le genre de
				///    defaut qu'aucun compilateur ne voit.
				virtual void Text(const NkPaintRect &r, const char *s, uint16 role,
								  NkTextAlign align = NkTextAlign::Left) = 0;

				/// Icone par POIGNEE OPAQUE (exigence B ci-dessus). `0` = aucune.
				virtual void Icon(const NkPaintRect &r, uint16 iconHandle, uint16 role) = 0;

				// ── Decoupe ─────────────────────────────────────────────────────
				// Indispensable des qu'un composant defile : sans elle, une carte a
				// demi sortie du panneau deborde sur son voisin.
				virtual void PushClip(const NkPaintRect &r) = 0;
				virtual void PopClip() = 0;
		};

	} // namespace editorkit
} // namespace nkentseu
