#pragma once
// -----------------------------------------------------------------------------
// @File    NkEditorContextMenu.h
// @Brief   Menu contextuel (clic droit) REUTILISABLE : liste d'items scrollable
//          (V/H), sous-menus (fleche), theme, occlusion d'input ("modal leger" :
//          consomme le clic quand la souris est dedans). Engine-native (ctx/dl/
//          font/theme) -> partageable par tous les editeurs Nkentseu.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NKGui/NKGui.h"
#include "NKEditorKit/NkEditorScrollbar.h" // scrollbar standard
#include "NKEditorKit/NkEditorTextField.h" // NkOverlayTextField (barre de recherche)

namespace nkentseu {
	namespace editorkit {

		using namespace nkentseu;
		using namespace nkentseu::nkgui;

// ── Menu contextuel (clic droit) Copier/Couper/Coller — reutilise par
		//    l'editeur et le terminal. Dessine sur la couche overlay (au-dessus). ──
		// Sous-chaine insensible a la casse (filtre du menu). Local au fichier :
		// l'editorkit ne doit pas dependre des utilitaires d'une application.
		inline bool NkCtxMenuContainsI(const char *hay, const char *needle) {
			if (!needle || !*needle)
				return true;
			if (!hay)
				return false;
			auto up = [](char c) -> char { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : c; };
			for (const char *h = hay; *h; ++h) {
				const char *a = h;
				const char *b = needle;
				while (*a && *b && up(*a) == up(*b)) {
					++a;
					++b;
				}
				if (!*b)
					return true;
			}
			return false;
		}

		struct NkCtxMenu {
				bool open = false;
				NkVec2 pos{0.f, 0.f};
				float32 sx = 0.f, sy = 0.f;			// defilement (reset a la fermeture)
				NkRect rect = {0.f, 0.f, 0.f, 0.f}; // boite de la frame courante (garde d'input)
		};

		// ── LA RANGEE D'ICONES D'ACTION DIRECTE (Lunacy) ─────────────────────
		// ⚠️ POURQUOI DANS LE KIT ET PAS DANS L'APPLICATION. Lunacy pose sous sa
		//    barre de recherche une rangee de sept icones -- coller, dupliquer,
		//    couper, cadenas, oeil, poubelle, composant. C'est de la GEOMETRIE de
		//    menu (hauteur de bande, cellule carree, survol, clic, grisage,
		//    occlusion), donc ca appartient au composant qui dessine le menu ;
		//    l'ecrire chez l'appelant aurait fabrique une seconde geometrie de
		//    menu a cote de celle-ci, et NK3DModeler n'en aurait rien eu.
		//
		// ⚠️ MAIS LE DESSIN DE L'ICONE RESTE A L'APPLICATION, et ce n'est pas un
		//    compromis : il n'existe aucun atlas d'icones partage (102 SVG chez
		//    NK3DModeler, 91 PNG chez NKCode, l'atlas NKGui en cours, un peintre
		//    vectoriel local chez NkUIDesign). Inventer ici un vocabulaire de
		//    poignees en aurait fait un QUATRIEME. Le kit tient donc la place et
		//    l'etat, l'hote peint dedans -- exactement le patron `rowOverlay` du
		//    composant d'arbre, qui existe pour la meme raison.
		struct NkCtxMenuRangee {
				int32 count = 0;					 ///< 0 = pas de rangee (defaut)
				const bool *enabled = nullptr;		 ///< nullptr = toutes actives
				void *user = nullptr;
				/// Peint UNE icone dans sa cellule. `actif` dit la couleur a prendre.
				void (*paint)(void *user, NkGuiDrawList &dl, int32 i, const NkRect &cell,
							  const NkColor &couleur) = nullptr;
				/// L'infobulle de chaque icone. ⚠️ UNE ICONE MUETTE EST UN BOUTON
				/// QU'ON N'OSE PAS PRESSER : c'est le seul indice qu'a l'utilisateur
				/// sur ce que fait un pictogramme, et la seule facon pour une icone
				/// GRISEE de dire POURQUOI elle l'est.
				const char *(*tip)(void *user, int32 i) = nullptr;
				int32 clicked = -1; ///< SORTIE : l'icone cliquee cette frame, -1 sinon
		};

		// Retourne l'index de l'item clique (et ferme le menu), -1 sinon. Se ferme au
		// clic exterieur ou sur Echap. `enabled[i]` grise les items non applicables.
		// `icons` (optionnel) : une texture par item, dessinee a GAUCHE du libelle —
		// meme colonne fixe pour tous, donc les libelles s'alignent.
		// `filter` (optionnel, non nul) : active une BARRE DE RECHERCHE ancree en
		// haut, hors de la zone defilante. L'index retourne reste celui de la liste
		// D'ORIGINE : le filtrage est invisible pour l'appelant.
		// `shortcuts` (optionnel) : le RACCOURCI de chaque item, aligne a DROITE en
		// texte attenue — la colonne des menus deroulants, transposee au menu
		// contextuel. Lunacy l'affiche, et VS Code, et Blender : un geste n'existe
		// pour l'utilisateur que s'il voit comment le refaire au clavier.
		// ⚠️ ADDITIF ET EN DERNIER : les consommateurs existants (NKCode, NkUIDesign,
		//    l'explorateur) ne passent rien et ne changent pas d'un caractere.
		//    `shortcuts[i]` peut valoir nullptr ou "" item par item.
		// `sepAfter` (optionnel) : un TRAIT sous l'item i. Lunacy groupe ses
		// commandes par separateurs, et le groupe est une information -- « ces
		// trois-la vont ensemble » se lit sans mot. ⚠️ Le trait suit son item :
		// filtre et defilement le deplacent avec lui, il ne peut donc pas se
		// retrouver a separer deux autres entrees.
		// `rangee` (optionnel) : la rangee d'icones d'action directe ci-dessus ;
		// elle s'ancre sous la recherche, hors de la zone defilante.
		// ⚠️ TOUS ADDITIFS ET EN DERNIER : les consommateurs existants (NKCode,
		//    l'explorateur, NkUIDesign) ne passent rien et ne changent pas d'un
		//    caractere.
		inline int32 NkCtxMenuDraw(NkGuiContext &ctx, NkCtxMenu &mn, const char *const *items, const bool *enabled,
								   int32 count, int32 *hoveredOut = nullptr, const bool *hasSub = nullptr,
								   const uint32 *icons = nullptr, char *filter = nullptr, int32 filterCap = 0,
								   bool *filterFocus = nullptr, const char *const *shortcuts = nullptr,
								   const bool *sepAfter = nullptr, NkCtxMenuRangee *rangee = nullptr,
							   const bool *checked = nullptr) {
			// ⚠️ `checked` EST ADDITIF ET EN DERNIER (2026-09-01) : les quatre
			//    consommateurs existants (NKCode x2, l'explorateur, NkUIDesign) ne
			//    changent pas d'un caractere. Le manque etait dans le socle -- le
			//    menu du clic droit dans le VIDE de Lunacy est fait AUX TROIS
			//    QUARTS de bascules cochees -- donc il est comble dans le socle,
			//    pas contourne chez l'application. C'est le meme geste que le
			//    parametre `checked` donne a `nkgui::MenuItem` le 28/08, et la
			//    meme raison : l'ecrire chez l'appelant aurait fabrique un second
			//    peintre de menu a cote de celui du kit.
			if (!mn.open)
				return -1;
			// LA MOLETTE APPARTIENT AU MENU OUVERT (2026-09-05, Rodolf : « le scroll de la
			// molette affecte le canvas a l'arriere ») : le menu la reserve pour la
			// prochaine image (NKGui la met de cote, tout le reste voit zero) et lit celle
			// mise de cote pour lui -- meme hors de sa boite, elle ne traverse pas.
			ctx.input.ReserverMolette();

			// ── Filtrage : on remplace les tableaux par leur version filtree, et on
			// retient la correspondance vers les index d'ORIGINE. Le reste de la
			// fonction travaille ensuite normalement, sans savoir qu'un filtre existe.
			enum { kMaxItems = 256 };
			const char *fItems[kMaxItems];
			const char *fShorts[kMaxItems];
			bool fEnabled[kMaxItems], fSub[kMaxItems], fSep[kMaxItems], fChk[kMaxItems];
			uint32 fIcons[kMaxItems];
			int32 fMap[kMaxItems];
			if (rangee)
				rangee->clicked = -1;
			// Au-dela de 8 entrees, derouler devient penible : c'est le seuil ou la
			// recherche gagne sa ligne (meme regle que le combo de la barre d'outils).
			const bool avecFiltre = (filter != nullptr && filterCap > 1 && count > 8);
			if (avecFiltre) {
				int32 n = 0;
				for (int32 i = 0; i < count && n < kMaxItems; ++i) {
					if (filter[0] && !NkCtxMenuContainsI(items[i], filter))
						continue;
					fMap[n] = i;
					fItems[n] = items[i];
					fShorts[n] = shortcuts ? shortcuts[i] : nullptr;
					fEnabled[n] = enabled ? enabled[i] : true;
					fSub[n] = hasSub ? hasSub[i] : false;
					fIcons[n] = icons ? icons[i] : 0u;
					// ⚠️ LE TRAIT SUIT SON ITEM. Recopier `sepAfter` par INDEX
					//    D'AFFICHAGE aurait laisse un trait sous une entree
					//    quelconque des que le filtre en retire une : le groupe
					//    n'aurait plus rien groupe.
					fSep[n] = sepAfter ? sepAfter[i] : false;
					// ⚠️ LA COCHE SUIT SON ITEM, exactement comme le trait
					//    ci-dessus et pour la meme raison : recopiee par index
					//    d'AFFICHAGE, elle se retrouverait sur UNE AUTRE entree
					//    des qu'un filtre en retire une. Une coche qui se deplace
					//    est pire qu'une coche absente -- elle affirme un etat
					//    faux au lieu de n'en affirmer aucun.
					fChk[n] = checked ? checked[i] : false;
					++n;
				}
				items = fItems;
				if (shortcuts)
					shortcuts = fShorts;
				enabled = fEnabled;
				hasSub = fSub;
				icons = fIcons;
				if (sepAfter)
					sepAfter = fSep;
				if (checked)
					checked = fChk;
				count = n;
			}
			NkGuiDrawList &dl = ctx.dlOverlay;
			const float32 lh = (ctx.font && ctx.font->Valid()) ? ctx.font->LineHeight() : 16.f;
			const float32 rowH = lh + 8.f, pad = 10.f, sbT = NkScrollbarWidth(); // scrollbar standard (14)
			// Taille IDEALE (plus long item) puis bornes : 60 % de la largeur, 50 % de la hauteur ;
			// au-dela -> defilement V/H (molette = V, Maj+molette ou molette H = H, barres draggables).
			// Colonne d'icones de largeur FIXE : les libelles demarrent tous au meme x,
			// la liste se lit comme un tableau (meme parti pris que le combo projets).
			float32 iconW = 0.f;
			if (icons)
				for (int32 i = 0; i < count; ++i)
					if (icons[i]) {
						iconW = lh;
						break;
					}
			// ⚠️ LA COLONNE DE COCHES PREND SA PLACE, ET LA PREND POUR TOUT LE
			//    MENU. Sans elle, la coche se dessinerait SUR le libelle -- et
			//    reserver la place seulement sur les lignes cochees ferait
			//    « danser » les libelles d'une ligne a l'autre, ce qui rend une
			//    liste illisible. C'est la meme regle que la colonne d'icones
			//    juste au-dessus : largeur FIXE des qu'UNE entree s'en sert.
			float32 checkW = 0.f;
			if (checked)
				for (int32 i = 0; i < count; ++i)
					if (checked[i]) {
						checkW = 14.f;
						break;
					}
			const float32 searchH = avecFiltre ? (lh + 12.f) : 0.f;
			// La rangee d'icones : une bande ancree, cellule carree de `cellR`.
			const bool avecRangee = (rangee != nullptr && rangee->count > 0 && rangee->paint != nullptr);
			const float32 cellR = lh + 8.f;
			const float32 rangeeH = avecRangee ? (cellR + 8.f) : 0.f;
			// Le TRAIT de separation prend sa place : sans elle il se dessinerait
			// SUR le libelle suivant, et un trait qui touche un texte se lit comme
			// un souligne, pas comme un groupe.
			const float32 sepH = 7.f;
			int32 nSep = 0;
			if (sepAfter)
				for (int32 i = 0; i < count; ++i)
					if (sepAfter[i])
						++nSep;
			float32 wIdeal = 168.f;
			if (ctx.font && ctx.font->Valid())
				for (int32 i = 0; i < count; ++i) {
					// ⚠️ LE RACCOURCI ENTRE DANS LA LARGEUR IDEALE. Sans ca il se
					//    dessinerait par-dessus la fin du libelle sur l'item le plus
					//    long — le defaut se voit sur UN item et sur un seul, donc il
					//    passe les essais courts.
					const float32 sw = (shortcuts && shortcuts[i] && shortcuts[i][0])
										   ? ctx.font->MeasureWidth(shortcuts[i]) + 24.f
										   : 0.f;
					// ⚠️ ET LA COLONNE DE COCHES Y ENTRE AUSSI, pour exactement la
					//    meme raison que le raccourci une ligne plus haut : oubliee,
					//    elle decalerait le libelle le plus long hors de la boite,
					//    sur UN item et un seul -- donc le defaut passerait tous les
					//    essais courts. La lecon est deja ecrite ici ; on l'applique
					//    au parametre neuf au lieu de la laisser au voisin.
					const float32 tw = ctx.font->MeasureWidth(items[i]) + pad * 2.f + 10.f + iconW +
									   checkW + sw +
									   ((hasSub && hasSub[i]) ? 16.f : 0.f); // place de la flèche ▸
					if (tw > wIdeal)
						wIdeal = tw;
				}
			const float32 wCap = static_cast<float32>(ctx.viewW) * 0.6f;
			const float32 hCap = static_cast<float32>(ctx.viewH) * 0.5f;
			const float32 contentH = count * rowH + (float32)nSep * sepH;
			const bool hasH = wIdeal > wCap;
			const float32 w = hasH ? wCap : wIdeal;
			const bool hasV = contentH + 8.f > hCap;
			// La bande de recherche ET la rangee d'icones s'ajoutent a la hauteur
			// SANS entrer dans le calcul de defilement : c'est ce qui les rend
			// insensibles au scroll.
			const float32 h =
				(hasV ? hCap : contentH + 8.f) + (hasH ? sbT : 0.f) + searchH + rangeeH;
			NkRect box = {mn.pos.x, mn.pos.y, w, h};
			if (box.x + box.w > static_cast<float32>(ctx.viewW))
				box.x = static_cast<float32>(ctx.viewW) - box.w;
			if (box.y + box.h > static_cast<float32>(ctx.viewH))
				box.y = static_cast<float32>(ctx.viewH) - box.h;
			if (box.x < 0.f)
				box.x = 0.f;
			if (box.y < 0.f)
				box.y = 0.f;
			const float32 bandeH = searchH + rangeeH; // tout ce qui est ANCRE en haut
			const NkRect inner = {box.x, box.y + bandeH, box.w - (hasV ? sbT : 0.f),
								  box.h - bandeH - (hasH ? sbT : 0.f)};
			const NkVec2 m = ctx.input.mousePos;
			const bool inBox = m.x >= box.x && m.x < box.x + box.w && m.y >= box.y && m.y < box.y + box.h;
			// Molette CONSOMMEE au-dessus du menu (sinon l'editeur en dessous defile aussi).
			const float32 maxSy = contentH + 8.f - inner.h > 0.f ? contentH + 8.f - inner.h : 0.f;
			const float32 maxSx = wIdeal - inner.w > 0.f ? wIdeal - inner.w : 0.f;
			const float32 molette = ctx.input.wheelReserve != 0.f ? ctx.input.wheelReserve : ctx.input.wheel;
			const float32 moletteH = ctx.input.wheelHReserve != 0.f ? ctx.input.wheelHReserve : ctx.input.wheelH;
			if (inBox && molette != 0.f) {
				if (ctx.input.shiftDown)
					mn.sx -= molette * 32.f;
				else
					mn.sy -= molette * rowH * 2.f;
			}
			ctx.input.wheel = 0.f; // consommee, dans la boite ou hors d'elle : rien ne traverse
			ctx.input.wheelReserve = 0.f;
			if (inBox && moletteH != 0.f) {
				mn.sx -= moletteH * 32.f;
			}
			ctx.input.wheelH = 0.f;
			ctx.input.wheelHReserve = 0.f;
			if (false) {
			}
			if (mn.sy < 0.f)
				mn.sy = 0.f;
			if (mn.sy > maxSy)
				mn.sy = maxSy;
			if (mn.sx < 0.f)
				mn.sx = 0.f;
			if (mn.sx > maxSx)
				mn.sx = maxSx;
			mn.rect = box; // exposee : les zones DERRIERE ignorent la souris quand elle est ici
			// Routeur d'occlusion unifie : ce menu est une surface de couche 50 — les
			// hit-tests de couche 0 (panneaux/widgets natifs via ItemHoverable) sous
			// son rect echouent automatiquement des la frame suivante.
			ctx.PushOcclusion(box, 50);
			NkGuiContext::NkInputLayerScope _layer(ctx, 50);
			// Couleurs du THÈME (dark ET light) — plus de valeurs en dur qui juraient
			// en thème clair (fond sombre + texte clair sur UI claire).
			dl.AddRectFilled(box, ctx.theme.panel, 6.f);
			dl.AddRect(box, ctx.theme.border, 1.f);
			int32 clicked = -1;
			// ── Bande de recherche ANCREE : dessinee AVANT le clip de la liste, donc
			// jamais rognee ni deplacee par le defilement.
			if (avecFiltre) {
				// TOUTE la largeur : la gouttiere derive de `inner`, qui demarre SOUS la
				// bande de recherche — rien ne l'occupe ici.
				const NkRect fr = {box.x + 4.f, box.y + 4.f, box.w - 8.f, searchH - 8.f};
				const bool inField = m.x >= fr.x && m.x < fr.x + fr.w && m.y >= fr.y && m.y < fr.y + fr.h;
				if (filterFocus && inField && ctx.input.mouseClicked[0])
					*filterFocus = true;
				NkOverlayTextField(ctx, dl, ctx.font, fr, filter, filterCap, filterFocus ? *filterFocus : true);
				if (!filter[0] && ctx.font && ctx.font->Valid())
					dl.AddText(ctx.font->Face(), ctx.font->TexId(),
							   {fr.x + 8.f, fr.y + (fr.h - lh) * 0.5f + ctx.font->Ascent()}, "Rechercher...",
							   ctx.theme.textDisabled);
			}
			// ── LA RANGEE D'ICONES, ancree elle aussi (sous la recherche) ────────
			// ⚠️ ELLE EST DESSINEE AVANT LE CLIP DE LA LISTE, pour la meme raison
			//    que la recherche : le defilement ne doit pas l'emporter.
			if (avecRangee) {
				const float32 ry = box.y + searchH + 4.f;
				const float32 total = (float32)rangee->count * cellR;
				// Repartie sur toute la largeur quand elle rentre, serree sinon --
				// une rangee qui deborde masquerait ses dernieres icones sans le dire.
				const float32 pasR = (total <= box.w - 8.f)
										 ? (box.w - 8.f) / (float32)rangee->count
										 : cellR;
				for (int32 i = 0; i < rangee->count; ++i) {
					const float32 cx = box.x + 4.f + (float32)i * pasR;
					const NkRect cell = {cx + (pasR - cellR) * 0.5f, ry, cellR, cellR};
					const bool act = rangee->enabled ? rangee->enabled[i] : true;
					const bool hov = m.x >= cell.x && m.x < cell.x + cell.w && m.y >= cell.y &&
									 m.y < cell.y + cell.h;
					if (hov && act) {
						NkColor selBg = ctx.theme.selection;
						selBg.a = 110;
						dl.AddRectFilled(cell, selBg, 4.f);
					}
					rangee->paint(rangee->user, dl, i, cell, act ? ctx.theme.text : ctx.theme.textDisabled);
					// L'INFOBULLE : le seul moyen pour une icone GRISEE de dire
					// pourquoi elle l'est. Peinte APRES la boucle serait plus propre,
					// mais elle serait alors recouverte par la liste ; ici elle est
					// dans la meme couche overlay et passe au-dessus du fond du menu.
					if (hov && rangee->tip && ctx.font && ctx.font->Valid()) {
						const char *t = rangee->tip(rangee->user, i);
						if (t && *t) {
							const float32 tw = ctx.font->MeasureWidth(t) + 12.f;
							NkRect tb = {cell.x, cell.y + cell.h + 2.f, tw, lh + 6.f};
							if (tb.x + tb.w > (float32)ctx.viewW)
								tb.x = (float32)ctx.viewW - tb.w;
							dl.AddRectFilled(tb, ctx.theme.panel, 4.f);
							dl.AddRect(tb, ctx.theme.border, 1.f);
							dl.AddText(ctx.font->Face(), ctx.font->TexId(),
									   {tb.x + 6.f, tb.y + 3.f + ctx.font->Ascent()}, t,
									   ctx.theme.text);
						}
					}
					if (hov && act && ctx.input.mouseClicked[0])
						rangee->clicked = i;
				}
				if (rangee->clicked >= 0) {
					mn.open = false;
					mn.sx = 0.f;
					mn.sy = 0.f;
				}
			}
			dl.PushClipRect(inner, true);
			float32 y = box.y + bandeH + 4.f - mn.sy;
			for (int32 i = 0; i < count; ++i) {
				const NkRect r = {box.x + 3.f, y, inner.w - 6.f, rowH};
				if (y + rowH >= inner.y && y <= inner.y + inner.h) { // row visible
					const bool hov = m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h && m.y >= inner.y &&
									 m.y < inner.y + inner.h;
					if (hov && hoveredOut)
						// Index d'ORIGINE : l'appelant ne doit jamais voir la numerotation
						// interne du filtrage (sinon il ouvrirait le mauvais element).
						*hoveredOut = avecFiltre ? fMap[i] : i;
					if (hov && enabled[i]) {
						NkColor selBg = ctx.theme.selection;
						selBg.a = 110;
						dl.AddRectFilled(r, selBg, 4.f);
					}
					if (iconW > 0.f && icons && icons[i])
						dl.AddImage(icons[i], {r.x + pad - mn.sx, y + (rowH - iconW) * 0.5f, iconW, iconW},
									{0.f, 0.f}, {1.f, 1.f},
									enabled[i] ? ctx.theme.textDisabled : ctx.theme.textDisabled);
					// LA COCHE DES BASCULES : deux traits, comme celle de Lunacy.
					// ⚠️ ELLE EST DESSINEE, PAS ECRITE EN CARACTERE : un glyphe de
					//    coche n'existe pas dans toutes les fontes embarquees, et
					//    une fonte qui ne l'a pas rendrait un carre vide -- un
					//    « etat inconnu » la ou l'on voulait dire « actif ».
					if (checked && checked[i]) {
						const NkColor cc = enabled[i] ? ctx.theme.text : ctx.theme.textDisabled;
						const float32 cx = r.x + pad - mn.sx + 2.f, cy = y + rowH * 0.5f;
						dl.AddRectFilled({cx, cy, 4.f, 1.6f}, cc, 0.f);
						dl.AddRectFilled({cx + 3.f, cy - 4.f, 1.6f, 5.6f}, cc, 0.f);
					}
					if (ctx.font && ctx.font->Valid())
						dl.AddText(ctx.font->Face(), ctx.font->TexId(),
								   {r.x + pad + (iconW > 0.f ? iconW + 6.f : (checkW > 0.f ? checkW : 0.f))
										- mn.sx,
									y + (rowH - lh) * 0.5f + ctx.font->Ascent()},
								   items[i], enabled[i] ? ctx.theme.text : ctx.theme.textDisabled);
					// LE RACCOURCI, aligne a DROITE et attenue — jamais colorable
					// comme le libelle : c'est un rappel, pas une commande.
					if (shortcuts && shortcuts[i] && shortcuts[i][0] && ctx.font && ctx.font->Valid()) {
						const float32 sw = ctx.font->MeasureWidth(shortcuts[i]);
						dl.AddText(ctx.font->Face(), ctx.font->TexId(),
								   {r.x + r.w - pad - sw - ((hasSub && hasSub[i]) ? 14.f : 0.f),
									y + (rowH - lh) * 0.5f + ctx.font->Ascent()},
								   shortcuts[i], ctx.theme.textDisabled);
					}
					if (hasSub && hasSub[i]) { // indicateur de SOUS-MENU : petite flèche ▸ à droite
						const float32 ax = r.x + r.w - 11.f, ay = y + rowH * 0.5f;
						dl.AddTriangleFilled({ax - 3.f, ay - 4.f}, {ax - 3.f, ay + 4.f}, {ax + 3.f, ay},
											 enabled[i] ? ctx.theme.text : ctx.theme.textDisabled);
					}
					if (hov && enabled[i] && ctx.input.mouseClicked[0])
						clicked = i;
				}
				y += rowH;
				// LE TRAIT DE GROUPE (Lunacy) : dessine APRES l'avance de ligne,
				// dans l'espace que `sepH` a reserve pour lui.
				if (sepAfter && sepAfter[i]) {
					const float32 sy2 = y + sepH * 0.5f;
					if (sy2 >= inner.y && sy2 <= inner.y + inner.h)
						dl.AddRectFilled({box.x + 8.f, sy2, inner.w - 16.f, 1.f}, ctx.theme.border, 0.f);
					y += sepH;
				}
			}
			dl.PopClipRect();
			// Barres de defilement (temoins + clic/glisser pour se positionner).
			if (hasV) {
				const NkRect tr = {box.x + box.w - sbT, inner.y, sbT, inner.h};
				NkVScrollbar(ctx, dl, tr, mn.sy, contentH + 8.f, inner.h, 0xC71E0001u, rowH);
			}
			if (hasH) {
				const NkRect tr = {box.x, box.y + box.h - sbT, inner.w, sbT};
				NkHScrollbar(ctx, dl, tr, mn.sx, wIdeal, inner.w, 0xC71E0002u, 32.f);
			}
			if (clicked >= 0) {
				mn.open = false;
				mn.sx = 0.f;
				mn.sy = 0.f;
			} else if ((ctx.input.mouseClicked[0] || ctx.input.mouseClicked[1]) && !inBox) {
				mn.open = false;
				mn.sx = 0.f;
				mn.sy = 0.f;
			}
			if (ctx.input.KeyPressed(NkGuiKey::Escape)) {
				mn.open = false;
				mn.sx = 0.f;
				mn.sy = 0.f;
			}
			if (inBox) { // MODAL leger : rien derriere ne doit voir ce clic
				ctx.input.mouseClicked[0] = false;
				ctx.input.mouseClicked[1] = false;
			}
			// Idem pour le clic : on rend l'index de la liste D'ORIGINE.
			return (clicked >= 0 && avecFiltre) ? fMap[clicked] : clicked;
		}

	} // namespace editorkit
} // namespace nkentseu
