#pragma once
// =============================================================================
// NkModelerUI.h — PEINTURE de l'interface, calquee sur la maquette Banani.
//
// POURQUOI ON NE PASSE PAS PAR NkEditorShell
//   Le shell de NKEditorKit apporte sa PROPRE chrome : barre de menus, barre
//   d'etat, systeme de docking, palette de commandes. C'est ce qu'il faut pour un
//   IDE, et c'est ce qui empeche de coller a une maquette au pixel pres -- on
//   passerait son temps a lutter contre une disposition qu'on ne controle pas.
//   NK3DModeler doit ressembler EXACTEMENT a l'ecran A : on peint donc nous-memes,
//   directement dans la draw list.
//
// TOUTES LES COULEURS VIENNENT DU THEME. Pas un seul 0xRRGGBB dans ce fichier :
//   c'est la condition pour que le theme clair, les themes utilisateur et les
//   roles propres au produit servent a quelque chose. Les seules constantes ici
//   sont des GEOMETRIES (hauteurs de ligne, largeurs de colonne), reprises des
//   proportions de la maquette.
//
// ETAT D'AVANCEMENT : la STRUCTURE est peinte, les interactions viendront par
//   iterations successives, comme convenu avec Rihen.
// =============================================================================

#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiFont.h"
#include "NKEditorKit/NkTheme.h"
#include "NK3DModeler/Shell/NkModelerTheme.h"
#include "NK3DModeler/Shell/NkModelerIcons.h"

namespace nkentseu {
	namespace nk3d {

		using namespace nkentseu::editorkit;
		using nkgui::NkColor;
		using nkgui::NkGuiDrawList;
		using nkgui::NkGuiFont;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		// ── ECHELLE D'INTERFACE ─────────────────────────────────────────────────
		// Posee UNE FOIS au demarrage depuis NkWindow::GetDpiScale(). Sur un ecran
		// a 125 % ou 150 %, ne pas en tenir compte laisse Windows ETIRER l'image de
		// la fenetre : tout devient mou, et c'est exactement le flou signale.
		// Variable inline (C++17) plutot qu'un parametre traine partout : l'echelle
		// ne change pas en cours de session, et la faire circuler dans chaque appel
		// n'aurait servi qu'a l'oublier quelque part.
		inline float32 gUiScale = 1.f;

		inline float32 S(float32 px) {
			return px * gUiScale;
		}

		// ARRONDI AU PIXEL. C'est la seconde cause du flou, et la plus insidieuse :
		// une ligne de base a 42,5 px echantillonne l'atlas ENTRE deux texels, donc
		// chaque glyphe bave d'un demi-pixel. A 13 px de corps, un demi-pixel
		// represente 4 % de la hauteur du caractere -- assez pour fatiguer l'oeil
		// sans qu'on sache dire pourquoi. Meme chose pour les icones : une image de
		// 16 px posee a 10,3 px est reechantillonnee sur toute sa surface.
		inline float32 Px(float32 v) {
			return (float32)(int32)(v < 0.f ? v - 0.5f : v + 0.5f);
		}

		// Rectangle aligne. On arrondit les DEUX BORDS puis on recalcule la largeur :
		// arrondir la position et la largeur separement laisse des trous d'un pixel
		// entre deux zones adjacentes, visibles comme des rayures claires.
		// Dimensions de la fenetre, posees une fois par image. Les panneaux
		// deroulants en ont besoin pour ne pas sortir de l'ecran, et les faire
		// descendre en parametre a travers Combo, CheckCombo et leurs appelants
		// aurait touche une vingtaine de signatures pour une valeur qui ne change
		// pas de la frame.
		inline float32 &NkPopupBoundsW() {
			static float32 w = 1600.f;
			return w;
		}
		inline float32 &NkPopupBoundsH() {
			static float32 h = 900.f;
			return h;
		}

		inline NkRect PxRect(const NkRect &r) {
			const float32 x0 = Px(r.x), y0 = Px(r.y);
			return {x0, y0, Px(r.x + r.w) - x0, Px(r.y + r.h) - y0};
		}

		// ── PROPORTIONS DE LA MAQUETTE ──────────────────────────────────────────
		// Reprises telles quelles de l'ecran A. Les pourcentages sont bornes par un
		// minimum en pixels : sous 1024 de large, un panneau a 16 % deviendrait
		// illisible et l'utilisateur ne pourrait plus rien y lire.
		struct NkLayout {
				float32 menuH = 0.f, tabsH = 0.f, toolH = 0.f, browserH = 0.f, statusH = 0.f;
				float32 leftW = 0.f, rightW = 0.f;
				NkRect menu{}, tabs{}, tool{}, left{}, view{}, right{}, propsR{}, detailsR{}, browser{},
					status{};

				// Les fractions viennent de l'ETAT (les separateurs les modifient) ;
				// les valeurs par defaut ne servent qu'au premier appel.
				// Poignees de reouverture des panneaux masques. Elles n'existent que si
				// le panneau correspondant est cache ; leur taille est nulle sinon, ce
				// qui les rend inatteignables sans test supplementaire.
				NkRect handleLeft{}, handleRight{}, handleBrowser{};

				void Compute(float32 W, float32 H, float32 fLeft = 0.16f, float32 fRight = 0.29f,
							 float32 fBrowser = 0.22f, float32 fProps = 0.45f, bool showLeft = true,
							 bool showRight = true, bool showBrowser = true) {
					// BARRE DE TITRE AMINCIE, demandee par Rihen. 30 px suffisent a un
					// logo, sept menus et trois boutons ; les 44 precedents venaient
					// d'un pourcentage de la hauteur d'ecran, ce qui n'a aucun sens pour
					// une barre dont le contenu a une taille fixe.
					menuH = S(30.f);
					tabsH = S(28.f);
					// AMINCIE par rapport a la maquette (5 %) : Banani y avait mis des
					// boutons qui font doublon avec les menus de la vue. Seul son
					// commutateur Objet/Edition est garde -- il rend visible un etat qui,
					// chez Blender, n'existe que dans un menu deroulant.
					toolH = S(34.f);
					// TAILLES MINIMALES. En dessous, un panneau ne montre plus ses
					// colonnes : la hierarchie perd l'oeil et le cadenas, le navigateur
					// perd une rangee de cartes. Le separateur bute donc sur ces bornes
					// au lieu de laisser reduire jusqu'a l'illisible.
					//
					// Et comme on ne peut plus reduire a rien, il FAUT pouvoir masquer :
					// c'est la contrepartie de la borne. Un panneau masque GARDE sa
					// fraction -- il rouvre a la largeur qu'il avait, pas au defaut.
					const float32 kMinBrowserH = S(140.f);
					const float32 kMinLeftW = S(200.f);
					const float32 kMinRightW = S(280.f);
					const float32 kHandle = S(14.f); // lisere de reouverture

					browserH = showBrowser ? H * fBrowser : kHandle;
					if (showBrowser && browserH < kMinBrowserH)
						browserH = kMinBrowserH;
					statusH = S(28.f);
					leftW = showLeft ? W * fLeft : kHandle;
					if (showLeft && leftW < kMinLeftW)
						leftW = kMinLeftW;
					rightW = showRight ? W * fRight : kHandle;
					if (showRight && rightW < kMinRightW)
						rightW = kMinRightW;

					float32 y = 0.f;
					menu = {0.f, y, W, menuH};
					y += menuH;
					tabs = {0.f, y, W, tabsH};
					y += tabsH;
					tool = {0.f, y, W, toolH};
					y += toolH;
					const float32 midH = H - y - browserH - statusH;
					left = {0.f, y, leftW, midH};
					view = {leftW, y, W - leftW - rightW, midH};
					right = {W - rightW, y, rightW, midH};
					// Proprietes AU-DESSUS des details, 45 / 55 comme la maquette.
					propsR = {right.x, right.y, right.w, midH * fProps};
					detailsR = {right.x, right.y + propsR.h, right.w, midH - propsR.h};
					y += midH;
					browser = {0.f, y, W, browserH};
					y += browserH;
					status = {0.f, y, W, statusH};

					// Les poignees occupent la place du panneau masque. Elles restent
					// DANS le flux : une bande flottante par-dessus la vue masquerait
					// justement ce qu'on vient de degager.
					handleLeft = showLeft ? NkRect{} : left;
					handleRight = showRight ? NkRect{} : right;
					handleBrowser = showBrowser ? NkRect{} : browser;
				}
		};

		// ── PEINTRE ─────────────────────────────────────────────────────────────
		class NkModelerPainter {
			public:
				NkModelerPainter(NkGuiDrawList &dl, NkGuiFont &font, const NkTheme &theme,
								 const NkModelerRoles &roles, const NkModelerIcons &icons) noexcept
					: mDl(dl), mFont(font), mTh(theme), mRoles(roles), mIcons(icons) {}

				// Conversion theme -> couleur du moteur. Le theme stocke 0xRRGGBBAA ;
				// NkColor attend quatre octets. Un seul endroit fait la traduction.
				NkColor C(NkRole r) const {
					return Unpack(mTh.Get(r));
				}
				NkColor C(uint16 id) const {
					return Unpack(mTh.Get(id));
				}
				// Couleur EMPAQUETEE (0xRRGGBBAA), sans depaquetage : la forme que
				// `NkComponentPaint::ColorOf` exige — et c'est celle que le theme
				// stocke deja. L'adaptateur du kit en fait MOINS que C(), pas plus.
				uint32 PackedColor(uint16 id) const {
					return mTh.Get(id);
				}

				float32 LineH() const {
					return mFont.LineHeight();
				}
				float32 TextW(const char *s) const {
					return mFont.MeasureWidth(s);
				}
				// La POLICE, pour les briques partagees de NKEditorKit (le champ de
				// saisie universel) qui mesurent le texte elles-memes.
				const NkGuiFont *FontPtr() const {
					return &mFont;
				}

				void Fill(const NkRect &r, NkRole role, float32 rounding = 0.f) {
					mDl.AddRectFilled(PxRect(r), C(role), rounding);
				}
				void Fill(const NkRect &r, const NkColor &c, float32 rounding = 0.f) {
					mDl.AddRectFilled(PxRect(r), c, rounding);
				}

				// Trait de separation. Toujours au meme role : une bordure dessinee
				// avec une autre couleur « qui va bien » se voit immediatement quand on
				// change de theme.
				void HLine(float32 x, float32 y, float32 w) {
					mDl.AddRectFilled({Px(x), Px(y), Px(x + w) - Px(x), 1.f}, C(NkRole::Border));
				}
				void VLine(float32 x, float32 y, float32 h) {
					mDl.AddRectFilled({Px(x), Px(y), 1.f, Px(y + h) - Px(y)}, C(NkRole::Border));
				}
				// ── SURCHARGES AVEC ROLE (2026-08-29) — exigees par NkComponentPaint ──
				// Le contrat du kit passe un role a CHAQUE trait, et les composants s'en
				// servent vraiment : mesure sur les deux composants ecrits, 7x border,
				// 1x guide (le guide d'indentation de l'arbre), 1x text (le curseur de
				// renommage). Le codage en dur ci-dessus aurait peint ces deux traits en
				// bordure — rien n'aurait plante, la couleur aurait menti a l'ecran.
				// Les variantes sans role restent le defaut de l'application : un trait
				// de separation EST une bordure tant qu'on ne dit pas autre chose.
				void HLine(float32 x, float32 y, float32 w, uint16 role) {
					mDl.AddRectFilled({Px(x), Px(y), Px(x + w) - Px(x), 1.f}, C(role));
				}
				void VLine(float32 x, float32 y, float32 h, uint16 role) {
					mDl.AddRectFilled({Px(x), Px(y), 1.f, Px(y + h) - Px(y)}, C(role));
				}

				// Segment QUELCONQUE. Indispensable des qu'une ligne n'est ni
				// horizontale ni verticale : les fuyantes du sol et le repere d'axes
				// etaient dessines en rectangles, d'ou les barres etranges au lieu de
				// diagonales.
				// CONTOUR ARRONDI. AddRect ne sait pas arrondir -- d'ou les pastilles
				// de filtre CARREES de la derniere capture, alors qu'elles doivent
				// etre en gelule. On peint donc le contour en PLEIN puis on recreuse
				// l'interieur d'un pixel : deux rectangles arrondis donnent un
				// contour arrondi, avec les primitives dont on dispose.
				void Outline(const NkRect &r, NkRole border, NkRole inner, float32 rounding = 0.f) {
					const NkRect q = PxRect(r);
					mDl.AddRectFilled(q, C(border), rounding);
					mDl.AddRectFilled({q.x + 1.f, q.y + 1.f, q.w - 2.f, q.h - 2.f}, C(inner),
									  rounding > 1.f ? rounding - 1.f : 0.f);
				}
				// Variante a angles vifs quand le fond n'a pas a etre repeint.
				void OutlineSharp(const NkRect &r, NkRole role) {
					mDl.AddRect(PxRect(r), C(role), 1.f);
				}
				// Meme trait, couleur DEJA resolue : les natures d'asset passent par
				// une table (NkAssetColor) qui melange roles communs et roles propres
				// au produit -- elle rend donc une couleur, pas un role.
				void OutlineSharp(const NkRect &r, const NkColor &c) {
					mDl.AddRect(PxRect(r), c, 1.f);
				}

				// Cercle CREUX : les demi-axes negatifs du gizmo de navigation.
				void Ring(float32 cx, float32 cy, float32 radius, NkRole role, NkRole inner) {
					mDl.AddCircleFilled({cx, cy}, radius, C(role));
					mDl.AddCircleFilled({cx, cy}, radius - 1.6f, C(inner));
				}
				void Ring(float32 cx, float32 cy, float32 radius, const NkColor &c, NkRole inner) {
					mDl.AddCircleFilled({cx, cy}, radius, c);
					mDl.AddCircleFilled({cx, cy}, radius - 1.6f, C(inner));
				}
				// Meme cercle creux, mais le TROU prend une couleur libre. Necessaire
				// des que ce qu'il y a derriere n'est plus une couleur de theme : sur
				// la pastille translucide du gizmo de navigation, reprendre la
				// couleur OPAQUE du fond de vue posait un rond plein bien visible au
				// lieu d'un creux.
				void RingColor(float32 cx, float32 cy, float32 radius, const NkColor &c,
							   const NkColor &inner) {
					mDl.AddCircleFilled({cx, cy}, radius, c);
					mDl.AddCircleFilled({cx, cy}, radius - 1.6f, inner);
				}

				// BARRE HORIZONTALE. Meme regle que la verticale : dessinee SEULEMENT
				// si le contenu deborde. Une barre permanente sur un panneau qui tient
				// entierement fait croire qu'il reste quelque chose a voir sur le cote.
				void HScroll(const NkRect &area, float32 contentW, float32 offset) {
					if (contentW <= area.w || area.w <= 0.f)
						return;
					const float32 h = S(6.f);
					const float32 y = area.y + area.h - h - 2.f;
					mDl.AddRectFilled(PxRect({area.x, y, area.w, h}), C(NkRole::WindowBg), 3.f);
					float32 tw = area.w * (area.w / contentW);
					if (tw < S(24.f))
						tw = S(24.f);
					const float32 maxOff = contentW - area.w;
					const float32 t = maxOff > 0.f ? (offset / maxOff) : 0.f;
					const float32 tx = area.x + (area.w - tw) * (t < 0.f ? 0.f : (t > 1.f ? 1.f : t));
					mDl.AddRectFilled(PxRect({tx, y, tw, h}), C(NkRole::TextMuted), 3.f);
				}

				// ASCENSEUR VERTICAL. Dessine SEULEMENT si le contenu depasse : une
				// barre toujours visible sur un panneau qui tient entierement fait
				// croire qu'il reste quelque chose a voir plus bas.
				void VScroll(const NkRect &area, float32 contentH, float32 offset) {
					if (contentH <= area.h || area.h <= 0.f)
						return;
					const float32 w = 6.f;
					// COLLE au bord : les 2 px d'ecart laissaient voir la couleur de
					// la ligne selectionnee derriere la barre (constate par Rihen).
					const float32 x = area.x + area.w - w;
					mDl.AddRectFilled({x, area.y, w, area.h}, C(NkRole::WindowBg), 3.f);
					float32 th = area.h * (area.h / contentH);
					if (th < 24.f)
						th = 24.f;
					const float32 maxOff = contentH - area.h;
					const float32 t = maxOff > 0.f ? (offset / maxOff) : 0.f;
					const float32 ty = area.y + (area.h - th) * (t < 0.f ? 0.f : (t > 1.f ? 1.f : t));
					mDl.AddRectFilled({x, ty, w, th}, C(NkRole::TextMuted), 3.f);
				}

				// Degrade aux quatre coins : LA primitive du picker de couleur
				// (carre SV, barre de teinte) -- celle du ColorPicker4 de NKGui.
				void RectMultiColor(const NkRect &r, NkColor tl, NkColor tr, NkColor br, NkColor bl) {
					mDl.AddRectFilledMultiColor(r, tl, tr, br, bl);
				}

				void Disc(float32 cx, float32 cy, float32 radius, NkRole role) {
					mDl.AddCircleFilled({cx, cy}, radius, C(role));
				}
				void DiscColor(float32 cx, float32 cy, float32 radius, const NkColor &c) {
					mDl.AddCircleFilled({cx, cy}, radius, c);
				}
				// Surcharge par COULEUR, meme raison que OutlineSharp : la table des
				// natures d'asset rend une couleur, pas un role.
				void Disc(float32 cx, float32 cy, float32 radius, const NkColor &c) {
					mDl.AddCircleFilled({cx, cy}, radius, c);
				}
				// Triangle plein tricolore : la brique de la ROUE CHROMATIQUE, qu'on
				// pave de secteurs allant du blanc (centre) a la teinte saturee.
				void TriColor(const NkVec2 &a, const NkVec2 &b, const NkVec2 &c, NkColor ca,
							  NkColor cb, NkColor cc) {
					mDl.AddTriangleMultiColor(a, b, c, ca, cb, cc);
				}

				void Line(float32 x0, float32 y0, float32 x1, float32 y1, NkRole role,
						  float32 thickness = 1.f) {
					mDl.AddLine({x0, y0}, {x1, y1}, C(role), thickness);
				}
				// Couleur BRUTE (guides de composition sur l'image 3D : la teinte
				// ne vient pas du theme mais du contenu qu'elle habille).
				void Line(float32 x0, float32 y0, float32 x1, float32 y1, const NkColor &c,
						  float32 thickness = 1.f) {
					mDl.AddLine({x0, y0}, {x1, y1}, c, thickness);
				}

				// Texte cale sur la LIGNE DE BASE, pas sur le haut du glyphe : sans ca
				// les libelles « sautent » d'un widget a l'autre selon leurs jambages.
				// La LIGNE DE BASE est arrondie, pas le haut du texte : c'est elle
				// qui positionne les glyphes dans l'atlas.
				void Text(float32 x, float32 y, const char *s, NkRole role = NkRole::Text) {
					mDl.AddText(mFont.Face(), mFont.TexId(), {Px(x), Px(y + mFont.Ascent())}, s, C(role));
				}
				void Text(float32 x, float32 y, const char *s, const NkColor &c) {
					mDl.AddText(mFont.Face(), mFont.TexId(), {Px(x), Px(y + mFont.Ascent())}, s, c);
				}
				// IMAGE BRUTE, teinte blanche : c'est la cible hors ecran de la vue 3D.
				// La teinte MULTIPLIE l'echantillon, donc tout autre blanc que le blanc
				// plein assombrirait le rendu -- ce serait un correcteur d'exposition
				// accidentel, applique apres le tonemap.
				void Image(uint32 texId, const NkRect &r) {
					mDl.AddImage(texId, PxRect(r), {0.f, 0.f}, {1.f, 1.f}, NkColor{255, 255, 255, 255});
				}

				// DECOUPE. Tout ce qui est peint entre Clip et Unclip est coupe au
				// rectangle donne. C'est ce qui manquait aux panneaux defilants : leur
				// contenu est dessine a partir de `listTop - defilement`, donc DES LE
				// PREMIER CRAN il commence AU-DESSUS du panneau et se peint par-dessus
				// l'en-tete, puis par-dessus le panneau voisin. Borner le defilement ne
				// suffit pas : le debordement vient du dessin, pas de la valeur.
				void Clip(const NkRect &r) {
					mDl.PushClipRect(PxRect(r), true);
				}
				void Unclip() {
					mDl.PopClipRect();
				}

				// TEXTE TRONQUE a une largeur donnee, avec des points de suite. Sans
				// troncature, un nom d asset un peu long deborde sur la carte voisine et
				// on ne sait plus a laquelle il appartient. Les points de suite disent
				// « il y a plus », ce qu une coupe nette ne dit pas.
				void TextClipped(float32 x, float32 y, float32 maxW, const char *s, NkRole role) {
					if (!s || !*s || maxW <= 0.f)
						return;
					if (mFont.MeasureWidth(s) <= maxW) {
						Text(x, y, s, role);
						return;
					}
					char buf[64];
					const float32 dots = mFont.MeasureWidth("...");
					uint32 n = 0;
					while (s[n] && n < 60u) {
						buf[n] = s[n];
						buf[n + 1] = 0;
						if (mFont.MeasureWidth(buf) + dots > maxW) {
							if (n > 0)
								buf[n] = 0;
							break;
						}
						++n;
					}
					buf[n] = 0;
					char out[68];
					uint32 k = 0;
					for (; buf[k] && k < 60u; ++k)
						out[k] = buf[k];
					out[k] = '.';
					out[k + 1] = '.';
					out[k + 2] = '.';
					out[k + 3] = 0;
					Text(x, y, out, role);
				}

				// Centre verticalement dans une hauteur donnee.
				void TextV(float32 x, float32 y, float32 h, const char *s, NkRole role = NkRole::Text) {
					Text(x, y + (h - mFont.LineHeight()) * 0.5f, s, role);
				}
				void TextV(float32 x, float32 y, float32 h, const char *s, const NkColor &c) {
					Text(x, y + (h - mFont.LineHeight()) * 0.5f, s, c);
				}

				// ── BLOC DE TEXTE QUI VA A LA LIGNE ─────────────────────────────
				// Le systeme de groupes cadre les WIDGETS -- leurs rectangles se
				// calculent depuis la largeur du groupe -- mais pas les CHAINES,
				// qui se peignent la ou on leur dit : un libelle plus large que la
				// colonne debordait tel quel (constate par Rihen). Plutot que de
				// clipper au cas par cas, on decoupe le texte a la largeur utile et
				// on passe a la ligne, comme le fait le panneau de discussion de
				// NkCode -- sans etre pour autant un champ multiligne editable.
				//
				// Coupe aux ESPACES ; un mot plus large que la colonne est place
				// seul sur sa ligne et clippe, plutot que de deborder ou de
				// disparaitre. Renvoie la hauteur consommee, pour que l'appelant
				// avance son curseur vertical sans avoir a compter les lignes.
				float32 TextWrap(float32 x, float32 y, float32 w, const char *s,
								 NkRole role = NkRole::Text, float32 lineGap = 0.f) {
					if (!s || !*s || w <= 1.f)
						return 0.f;
					const float32 lh = mFont.LineHeight() + lineGap;
					float32 cy = y;
					char line[512];
					uint32 len = 0;
					uint32 i = 0;
					while (s[i]) {
						// Mot suivant, espaces de tete compris.
						uint32 ws = i;
						while (s[i] == ' ')
							++i;
						uint32 b = i;
						while (s[i] && s[i] != ' ')
							++i;
						const uint32 wlen = i - ws;
						if (wlen == 0 || len + wlen + 1 >= sizeof(line))
							break;
						// Essai : la ligne courante + ce mot.
						char test[512];
						uint32 t = 0;
						for (uint32 k = 0; k < len; ++k)
							test[t++] = line[k];
						for (uint32 k = ws; k < i; ++k)
							test[t++] = s[k];
						test[t] = 0;
						if (len > 0 && TextW(test) > w) {
							// Ca ne rentre pas : on ferme la ligne et on repart
							// avec ce mot, sans son espace de tete.
							line[len] = 0;
							Text(x, cy, line, role);
							cy += lh;
							len = 0;
							for (uint32 k = b; k < i; ++k)
								line[len++] = s[k];
						} else {
							len = t;
							for (uint32 k = 0; k < t; ++k)
								line[k] = test[k];
						}
					}
					if (len > 0) {
						line[len] = 0;
						Text(x, cy, line, role);
						cy += lh;
					}
					return cy - y;
				}

				// ── ICONES ──────────────────────────────────────────────────────
				// TEINTEES par un role de theme, jamais affichees telles quelles : une
				// icone grise dessinee brute serait invisible en theme clair -- le
				// defaut meme que le systeme de roles existe pour empecher.
				//
				// Une icone MANQUANTE ne dessine RIEN et ne decale rien. Elle degrade
				// l'interface au lieu de la casser : un carre de remplacement serait
				// pire, il ferait croire a un dessin voulu.
				void Icon(float32 x, float32 y, NkIcon ic, NkRole role = NkRole::Text, float32 sz = 0.f) {
					const uint32 tex = mIcons.Tex(ic);
					if (!tex)
						return;
					// La TAILLE est arrondie aussi : une icone de 13,7 px est
					// reechantillonnee sur toute sa surface, d'ou le flou.
					const float32 s = Px(sz > 0.f ? S(sz) : (float32)mIcons.Size());
					mDl.AddImage(tex, {Px(x), Px(y), s, s}, {0.f, 0.f}, {1.f, 1.f}, C(role));
				}
				void Icon(float32 x, float32 y, NkIcon ic, const NkColor &c, float32 sz = 0.f) {
					const uint32 tex = mIcons.Tex(ic);
					if (!tex)
						return;
					const float32 s = Px(sz > 0.f ? S(sz) : (float32)mIcons.Size());
					mDl.AddImage(tex, {Px(x), Px(y), s, s}, {0.f, 0.f}, {1.f, 1.f}, c);
				}
				// Centree verticalement dans une hauteur de ligne.
				void IconV(float32 x, float32 y, float32 h, NkIcon ic, NkRole role = NkRole::Text,
						   float32 sz = 0.f) {
					const float32 s = sz > 0.f ? S(sz) : (float32)mIcons.Size();
					Icon(x, y + (h - s) * 0.5f, ic, role, sz);
				}
				float32 IconSize() const {
					return (float32)mIcons.Size();
				}

				const NkTheme &Theme() const {
					return mTh;
				}
				const NkModelerRoles &Roles() const {
					return mRoles;
				}

			private:
				static NkColor Unpack(uint32 c) {
					return NkColor{(uint8)((c >> 24) & 0xFF), (uint8)((c >> 16) & 0xFF),
								   (uint8)((c >> 8) & 0xFF), (uint8)(c & 0xFF)};
				}
				NkGuiDrawList &mDl;
				NkGuiFont &mFont;
				const NkTheme &mTh;
				const NkModelerRoles &mRoles;
				const NkModelerIcons &mIcons;
		};

		// ── NATURE D'UN ASSET : COULEUR ET NOM ──────────────────────────────────
		// POINT DE PASSAGE UNIQUE. La carte du navigateur, le liseret d'onglet et
		// la pastille de filtre disent tous la meme nature ; les laisser choisir
		// chacun leur couleur, c'est garantir qu'un jour l'un d'eux gardera
		// l'ancienne -- la table etait deja recopiee a trois endroits.
		//
		// `kind` : 0 graphe (PROCEDURAL), 1 dossier, 2 materiau, 3 texture,
		// 4 dataset IA, 5 scene, 6 model. `sub` ne sert qu'aux graphes.
		inline NkColor NkAssetColor(const NkModelerPainter &p, uint8 kind, uint8 sub) {
			if (kind == 0) {
				// PROCEDURAL : une famille a part (cf. NkModelerTheme.h). Un
				// sous-type inconnu retombe sur la modelisation plutot que sur une
				// autre famille -- mieux vaut « procedural, nuance imprecise » que
				// « pris pour un maillage ».
				const NkModelerRoles &R = p.Roles();
				const uint16 id = (sub == 1)   ? R.procTex
								  : (sub == 2) ? R.procMat
								  : (sub == 3) ? R.procMotion
											   : R.procMesh;
				return p.C(id);
			}
			return p.C((kind == 1)	 ? NkRole::TypeFolder
					   : (kind == 4) ? NkRole::AccentUi
					   : (kind == 5) ? NkRole::AxisZ
					   : (kind == 6) ? NkRole::AxisX
					   : (kind == 2) ? NkRole::TypeMat
									 : NkRole::TypeTex);
		}

		/// Nom affiche de la nature. Les quatre graphes se nomment enfin chacun :
		/// la cascade precedente testait `kind == 0` EN PREMIER, si bien que les
		/// quatre libelles par sous-type ecrits en dessous n'etaient jamais
		/// atteints -- tous les graphes s'appelaient « Graphe ».
		inline const char *NkAssetKindName(uint8 kind, uint8 sub) {
			if (kind == 0)
				return (sub == 1)	? "Graphe texturing"
					   : (sub == 2) ? "Graphe materiau"
					   : (sub == 3) ? "Graphe motion"
									: "Graphe modelisation";
			return (kind == 1)	 ? "Dossier"
				   : (kind == 4) ? "Dataset IA"
				   : (kind == 5) ? "Scene"
				   : (kind == 6) ? "Model"
				   : (kind == 2) ? "Materiau"
								 : "Texture";
		}

	} // namespace nk3d
} // namespace nkentseu
