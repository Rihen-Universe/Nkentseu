// -----------------------------------------------------------------------------
// @File    NkContentBrowserUnreal.cpp
// @Brief   LA VARIANTE UNREAL du navigateur de contenu : les sept zones du
//          Content Browser d'Unreal 5, ses cartes de fichiers et de dossiers, et
//          ses gestes (selection simple / Ctrl / Maj / cadre, glisser multiple,
//          historique, verrou, sections repliables).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  LA CIBLE (Applications/UnkenyEditor/design/02-fidelite-ue5.md §3, §3.1)
// =============================================================================
//  1 barre      : « + Ajouter », « Importer », « Tout enregistrer », precedent /
//                 suivant, icone de dossier + fil d'Ariane cliquable, verrou,
//                 « Reglages » a droite.
//  2 sources    : sections repliables « Favoris » et LE PROJET (son arbre de
//                 dossiers, le composant `tree_view`), chacune avec sa loupe.
//  3 collections: section repliable, « + », loupe, compteur par collection.
//  4 filtres    : les puces de type (les natures declarees par l'application).
//  5 recherche  : entonnoir, boite « Rechercher dans <dossier> », tri.
//  6 la vue     : cartes d'Unreal -- vignette sur fond sombre, BANDE FINE de la
//                 couleur du type, nom sur deux lignes, type en gris ; cartes de
//                 DOSSIER (grande icone, nom dessous, couleur choisie) ; survol
//                 eclairci ; selection en bleu plein ; l'etat vide d'Unreal ;
//                 « N elements (M selectionnes) ».
//  7 reglages   : le bouton ; le menu est a l'hote (`reglagesDemandes`).
//
//  NOS TOUCHES (meme document) : le bleu d'etat pour la selection, l'AMBRE pour
//  la selection secondaire (choisie mais pas active) et la cible d'un glisser,
//  des coins a peine arrondis (`card_round`), et la vignette de l'application
//  (`NkContentBrowserHooks::vignetteApp`).
//
// =============================================================================
//  LES REGLES DU FICHIER VOISIN TIENNENT ICI
// =============================================================================
//  - Aucune longueur en pixels : `M("...")` ou une FRACTION d'un rectangle.
//  - Aucune couleur en dur : un role du style, ou une nuance de ce role
//    (`NkTeinter`).
//  - Le composant SIGNALE, l'hote AGIT : rien ici ne touche au disque. Les
//    gestes sur le disque sont dans `NkContentBrowserDisque.h`.
//  - Les pictogrammes sont DESSINES avec les primitives du contrat (porte du
//    04/09 : `Icon` peint un carre plein, il n'y a pas d'atlas).
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkContentBrowserInterne.h"

namespace nkentseu {
	namespace editorkit {

		namespace {

			using namespace cbi;

			/// La meme couleur, d'opacite `a` (0..255). L'empaquetage est 0xRRGGBBAA
			/// (cf. la mise en garde de `Teinter` dans le fichier voisin).
			uint32 Alpha(uint32 rgba, uint32 a) {
				return (rgba & 0xFFFFFF00u) | (a & 0xFFu);
			}

			// ── LES PICTOGRAMMES DE LA BARRE, DESSINES ────────────────────────────
			// Tous tiennent dans un CARRE `r` et ne lisent que des fractions de son
			// cote : ils suivent l'echelle de l'interface sans un nombre de pixels.

			/// « + » : deux barres.
			void Plus(NkComponentPaint &p, const NkPaintRect &r, uint16 role) {
				const float32 e = r.w * 0.18f;
				p.Fill({r.x + (r.w - e) * 0.5f, r.y + r.h * 0.1f, e, r.h * 0.8f}, role, e * 0.3f);
				p.Fill({r.x + r.w * 0.1f, r.y + (r.h - e) * 0.5f, r.w * 0.8f, e}, role, e * 0.3f);
			}

			/// Triangle plein : vers le bas (section ouverte) ou vers la droite.
			void Triangle(NkComponentPaint &p, const NkPaintRect &r, uint16 role, bool bas) {
				const uint32 c = p.ColorOf(role);
				if (bas) {
					const float32 xy[6] = {r.x, r.y + r.h * 0.25f, r.x + r.w, r.y + r.h * 0.25f, r.x + r.w * 0.5f,
										   r.y + r.h * 0.8f};
					if (!p.PolygonHex(xy, 3, c))
						p.Fill({r.x, r.y + r.h * 0.3f, r.w, r.h * 0.4f}, role);
				} else {
					const float32 xy[6] = {r.x + r.w * 0.25f, r.y, r.x + r.w * 0.8f, r.y + r.h * 0.5f, r.x + r.w * 0.25f,
										   r.y + r.h};
					if (!p.PolygonHex(xy, 3, c))
						p.Fill({r.x + r.w * 0.3f, r.y, r.w * 0.4f, r.h}, role);
				}
			}

			/// « > » du fil d'Ariane, en deux traits.
			void Chevron(NkComponentPaint &p, const NkPaintRect &r, uint16 role, float32 ep) {
				const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f, d = r.h * 0.18f;
				p.Line(cx - d * 0.5f, cy - d, cx + d * 0.5f, cy, role, ep);
				p.Line(cx + d * 0.5f, cy, cx - d * 0.5f, cy + d, role, ep);
			}

			/// Un ANNEAU : disque du role, puis disque du fond. `fond` est le role sur
			/// lequel l'anneau est pose (sans primitive de cercle creux, c'est la
			/// seule facon de le dire au contrat).
			void Anneau(NkComponentPaint &p, const NkPaintRect &r, uint16 role, uint16 fond, float32 ep) {
				if (!p.Ellipse(r, role)) {
					p.OutlineSharp(r, role);
					return;
				}
				p.Ellipse({r.x + ep, r.y + ep, r.w - 2.f * ep, r.h - 2.f * ep}, fond);
			}

			void Loupe(NkComponentPaint &p, const NkPaintRect &r, uint16 role, uint16 fond, float32 ep) {
				const NkPaintRect o{r.x + r.w * 0.1f, r.y + r.h * 0.1f, r.w * 0.6f, r.h * 0.6f};
				Anneau(p, o, role, fond, ep * 1.4f);
				p.Line(r.x + r.w * 0.6f, r.y + r.h * 0.6f, r.x + r.w * 0.92f, r.y + r.h * 0.92f, role, ep * 1.8f);
			}

			/// Precedent / suivant d'Unreal : une fleche dans un anneau.
			void FlecheRonde(NkComponentPaint &p, const NkPaintRect &r, uint16 role, uint16 fond, bool gauche,
							 float32 ep) {
				Anneau(p, r, role, fond, ep * 1.3f);
				const float32 cy = r.y + r.h * 0.5f;
				const float32 a = r.x + r.w * 0.3f, b = r.x + r.w * 0.7f, d = r.h * 0.18f;
				p.Line(a, cy, b, cy, role, ep * 1.4f);
				if (gauche) {
					p.Line(a, cy, a + d, cy - d, role, ep * 1.4f);
					p.Line(a, cy, a + d, cy + d, role, ep * 1.4f);
				} else {
					p.Line(b, cy, b - d, cy - d, role, ep * 1.4f);
					p.Line(b, cy, b - d, cy + d, role, ep * 1.4f);
				}
			}

			/// Le cadenas : un corps plein, une anse (ouverte : decalee vers le haut).
			void Verrou(NkComponentPaint &p, const NkPaintRect &r, uint16 role, bool ferme, float32 ep) {
				const NkPaintRect corps{r.x + r.w * 0.2f, r.y + r.h * 0.45f, r.w * 0.6f, r.h * 0.45f};
				p.Fill(corps, role, r.w * 0.06f);
				const float32 g = r.x + r.w * 0.32f, d = r.x + r.w * 0.68f;
				const float32 haut = r.y + r.h * 0.12f, bas = corps.y;
				p.Line(d, bas, d, haut + r.h * 0.08f, role, ep * 1.6f);
				p.Line(g + r.w * 0.04f, haut, d - r.w * 0.04f, haut, role, ep * 1.6f);
				p.Line(g, haut + r.h * 0.08f, g, ferme ? bas : bas - r.h * 0.16f, role, ep * 1.6f);
			}

			/// La roue dentee de « Reglages » : un anneau epais et huit dents.
			void Engrenage(NkComponentPaint &p, const NkPaintRect &r, uint16 role, uint16 fond, float32 ep) {
				const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
				static const float32 kDir[8][2] = {{1.f, 0.f},		{0.707f, 0.707f},	{0.f, 1.f},	 {-0.707f, 0.707f},
												   {-1.f, 0.f}, {-0.707f, -0.707f}, {0.f, -1.f}, {0.707f, -0.707f}};
				for (int32 k = 0; k < 8; ++k)
					p.Line(cx + kDir[k][0] * r.w * 0.22f, cy + kDir[k][1] * r.h * 0.22f, cx + kDir[k][0] * r.w * 0.46f,
						   cy + kDir[k][1] * r.h * 0.46f, role, ep * 2.f);
				Anneau(p, {r.x + r.w * 0.2f, r.y + r.h * 0.2f, r.w * 0.6f, r.h * 0.6f}, role, fond, ep * 2.f);
			}

			/// « Importer » : un plateau et une fleche qui y descend.
			void Importer(NkComponentPaint &p, const NkPaintRect &r, uint16 role, float32 ep) {
				const float32 cx = r.x + r.w * 0.5f;
				p.Line(cx, r.y + r.h * 0.08f, cx, r.y + r.h * 0.62f, role, ep * 1.6f);
				p.Line(cx, r.y + r.h * 0.62f, cx - r.w * 0.2f, r.y + r.h * 0.42f, role, ep * 1.6f);
				p.Line(cx, r.y + r.h * 0.62f, cx + r.w * 0.2f, r.y + r.h * 0.42f, role, ep * 1.6f);
				p.Line(r.x + r.w * 0.1f, r.y + r.h * 0.6f, r.x + r.w * 0.1f, r.y + r.h * 0.9f, role, ep * 1.6f);
				p.Line(r.x + r.w * 0.1f, r.y + r.h * 0.9f, r.x + r.w * 0.9f, r.y + r.h * 0.9f, role, ep * 1.6f);
				p.Line(r.x + r.w * 0.9f, r.y + r.h * 0.9f, r.x + r.w * 0.9f, r.y + r.h * 0.6f, role, ep * 1.6f);
			}

			/// « Tout enregistrer » : la disquette, son volet et son etiquette.
			void Disquette(NkComponentPaint &p, const NkPaintRect &r, uint16 role, uint16 fond) {
				p.Fill({r.x + r.w * 0.1f, r.y + r.h * 0.1f, r.w * 0.8f, r.h * 0.8f}, role, r.w * 0.08f);
				p.Fill({r.x + r.w * 0.28f, r.y + r.h * 0.1f, r.w * 0.38f, r.h * 0.26f}, fond);
				p.Fill({r.x + r.w * 0.24f, r.y + r.h * 0.55f, r.w * 0.52f, r.h * 0.3f}, fond);
			}

			/// L'entonnoir des filtres d'Unreal : trois traits qui raccourcissent.
			void Entonnoir(NkComponentPaint &p, const NkPaintRect &r, uint16 role, float32 ep) {
				for (int32 k = 0; k < 3; ++k) {
					const float32 y = r.y + r.h * (0.25f + 0.25f * (float32)k);
					const float32 m = r.w * 0.12f * (float32)(k * 2);
					p.Line(r.x + r.w * 0.05f + m * 0.5f, y, r.x + r.w * 0.95f - m * 0.5f, y, role, ep * 1.6f);
				}
			}

			/// Le tri : une fleche vers le haut, une vers le bas.
			void Tri(NkComponentPaint &p, const NkPaintRect &r, uint16 role, float32 ep) {
				const float32 a = r.x + r.w * 0.32f, b = r.x + r.w * 0.68f, d = r.w * 0.14f;
				p.Line(a, r.y + r.h * 0.12f, a, r.y + r.h * 0.88f, role, ep * 1.5f);
				p.Line(a, r.y + r.h * 0.12f, a - d, r.y + r.h * 0.32f, role, ep * 1.5f);
				p.Line(a, r.y + r.h * 0.12f, a + d, r.y + r.h * 0.32f, role, ep * 1.5f);
				p.Line(b, r.y + r.h * 0.12f, b, r.y + r.h * 0.88f, role, ep * 1.5f);
				p.Line(b, r.y + r.h * 0.88f, b - d, r.y + r.h * 0.68f, role, ep * 1.5f);
				p.Line(b, r.y + r.h * 0.88f, b + d, r.y + r.h * 0.68f, role, ep * 1.5f);
			}

			// ── LE NOM SUR DEUX LIGNES (Unreal) ───────────────────────────────────
			// La premiere ligne prend le plus long PREFIXE qui tient, coupe de
			// preference apres un separateur (« _ », « - », « . », espace) s'il en est
			// un dans sa seconde moitie -- « Advanced_Lighting_BuiltData » se coupe
			// en « Advanced_Lighting_ » / « BuiltData », comme la planche. La seconde
			// ligne est rendue par `Text`, qui pose les points de suite (son contrat).
			// ⚠️ ON NE COUPE JAMAIS AU MILIEU D'UN CARACTERE UTF-8 : « é » est deux
			//    octets, et une moitie d'accent s'affiche comme un carre.
			void DeuxLignes(NkComponentPaint &p, const NkPaintRect &r, const char *s, uint16 role, bool centre,
							float32 lh) {
				if (!s || !s[0] || r.w <= 0.f)
					return;
				const NkTextAlign al = centre ? NkTextAlign::Center : NkTextAlign::Left;
				if (p.TextWidth(s) <= r.w) {
					p.Text({r.x, r.y, r.w, lh}, s, role, al);
					return;
				}
				int32 k = 0, coupe = 0, sep = 0;
				for (int32 i = 1; s[i - 1]; ++i) {
					if (((unsigned char)s[i] & 0xC0u) == 0x80u)
						continue; // au milieu d'un caractere
					if (p.LargeurPolice(s, s + i, 0) > r.w)
						break;
					k = i;
					const char c = s[i - 1];
					if (c == '_' || c == '-' || c == '.' || c == ' ')
						sep = i;
					if (!s[i])
						break;
				}
				if (k == 0)
					k = 1;
				coupe = (sep > k / 2) ? sep : k;
				const float32 w1 = p.LargeurPolice(s, s + coupe, 0);
				const float32 x1 = centre ? r.x + (r.w - w1) * 0.5f : r.x;
				p.TextePolice({x1, r.y, w1, lh}, s, s + coupe, role, 0);
				const char *reste = s + coupe;
				while (*reste == ' ')
					++reste;
				p.Text({r.x, r.y + lh, r.w, lh}, reste, role, al);
			}

			/// Le chemin est-il de ceux qu'on traine ?
			bool EstTraine(const NkContentBrowserModel &m, const NkString &chemin) {
				if (NkBrowserMemeChemin(chemin, m.glisserChemin))
					return true;
				for (uint32 i = 0; i < (uint32)m.glisserChemins.Size(); ++i)
					if (NkBrowserMemeChemin(chemin, m.glisserChemins[i]))
						return true;
				return false;
			}

			/// Le dernier segment d'un chemin (« Contenu/Textures » -> « Textures »).
			const char *DernierSegment(const NkString &chemin) {
				const char *d = chemin.Data() ? chemin.Data() : "";
				const char *r = d;
				for (const char *q = d; *q; ++q)
					if (*q == '/' || *q == '\\')
						r = q + 1;
				return r;
			}

			/// Le texte d'un filtre se trouve-t-il dans `s` (casse ASCII ignoree) ?
			bool Contient(const char *s, const char *f) {
				if (!f || !f[0])
					return true;
				NkAssetEntry e;
				e.name = NkString(s ? s : "");
				return PassesFilter(e, f);
			}

		} // namespace

		uint32 NkContentBrowserTexteEtat(char *t, uint32 cap, uint32 n, uint32 c) {
			if (!t || cap == 0u)
				return 0u;
			uint32 at = PutUInt(t, cap, 0, n);
			// « 0 element » : zero s'accorde au singulier, comme un.
			at = PutStr(t, cap, at, n <= 1u ? " élément" : " éléments");
			if (c > 0u) {
				at = PutStr(t, cap, at, " (");
				at = PutUInt(t, cap, at, c);
				at = PutStr(t, cap, at, c == 1u ? " sélectionné)" : " sélectionnés)");
			}
			return at;
		}

		NkContentBrowserResult NkDrawContentBrowserUnreal(NkComponentPaint &p, const NkComponentInput &in,
														  const NkPaintRect &rect, NkContentBrowserModel &m,
														  const NkContentBrowserStyle &s,
														  const NkContentBrowserHooks &hooks) {
			NkContentBrowserResult res;
			if (rect.w <= 0.f || rect.h <= 0.f)
				return res;

			// ── LES NOMBRES, TOUS PAR LA DECLARATION (la regle du fichier voisin) ──
			auto M = [&](const char *k) {
				return NkBrowserMetric(s, k) * in.surfaceScale;
			};
			auto P = [&](const char *k) {
				return NkBrowserParam(s, k);
			};
			const float32 pad = M("card_pad");
			const float32 rowH = M("row_h");
			const float32 ep = M("stroke_w");
			const float32 rd = M("card_round");
			const float32 lh = p.LineHeight();
			const float32 seuil = M("drag_threshold");
			// Les jetons de la variante, avec leur repli NOMME (cf. le style).
			const uint16 rHover = s.cardHover ? s.cardHover : s.chipBg;
			const uint16 rAjout = s.addMark ? s.addMark : s.activeMark;
			const uint16 rAmbre = s.dragMark ? s.dragMark : s.chosenMark;
			const uint16 rSurBleu = s.textOnAccent ? s.textOnAccent : s.text;
			const bool multi = P("multi_select") > 0.5f;

			// ── LE FOCUS : le dernier clic, dedans ou dehors ──────────────────────
			const bool dedans = rect.Contains(in.mouseX, in.mouseY);
			if (in.mousePressed || in.rightPressed)
				m.focus = dedans;

			// ── L'HISTORIQUE : l'hote pose le chemin courant, on empile ce qui change
			// ⚠️ UN RETOUR EN ARRIERE NE DOIT PAS S'EMPILER : le clic sur « precedent »
			//    pose lui-meme `cheminCourant` sur l'entree qu'il rejoint, si bien
			//    qu'a l'image suivante le chemin est DEJA celui de la position.
			if (!m.cheminCourant.Empty()) {
				const int32 n = (int32)m.historique.Size();
				const bool pareil = m.historiquePos >= 0 && m.historiquePos < n &&
									m.historique[(uint32)m.historiquePos] == m.cheminCourant;
				if (!pareil) {
					while ((int32)m.historique.Size() > m.historiquePos + 1 && !m.historique.Empty())
						m.historique.PopBack();
					m.historique.PushBack(m.cheminCourant);
					if (m.historique.Size() > 64u)
						m.historique.RemoveAt(0);
					m.historiquePos = (int32)m.historique.Size() - 1;
				}
			}

			// ── LE GLISSER ET LE CADRE : ARME -> ACTIF, AVANT LE DESSIN ─────────
			// (meme ordre que le mixte : le surlignage de la cible doit etre juste
			// des cette image, pas a la suivante).
			if (!m.armeChemin.Empty() && in.mouseDown && m.glisserChemin.Empty()) {
				const float32 dx = in.mouseX - m.armeX, dy = in.mouseY - m.armeY;
				if (dx * dx + dy * dy > seuil * seuil) {
					m.glisserChemin = m.armeChemin;
					m.glisserLibelle = m.armeLibelle;
					m.glisserRail = m.armeRail;
					m.glisserChemins = m.armeChemins;
					if (m.glisserChemins.Empty())
						m.glisserChemins.PushBack(m.armeChemin);
					// Un glisser est parti : l'appui ne reduira plus la selection.
					m.reduireAuRelache = -1;
					m.cadreArme = false;
				}
			}
			if (!in.mouseDown && !in.mousePressed && m.glisserChemin.Empty()) {
				m.armeChemin = NkString();
				m.armeChemins.Clear();
			}
			if (m.cadreArme && in.mouseDown && !m.cadreActif) {
				const float32 dx = in.mouseX - m.cadreX0, dy = in.mouseY - (m.cadreY0 - m.scroll);
				if (dx * dx + dy * dy > seuil * seuil)
					m.cadreActif = true;
			}

			p.PushClip(rect);
			p.Fill(rect, s.panelBg);

			// =================================================================
			//  ZONE 1 : LA BARRE
			// =================================================================
			const float32 tbH = M("toolbar_h");
			const NkPaintRect tb{rect.x, rect.y, rect.w, tbH};
			p.Fill(tb, s.headerBg);
			const float32 bh = rowH;
			const float32 by = tb.y + (tb.h - bh) * 0.5f;
			const float32 ico = bh * 0.62f;			// un pictogramme dans un bouton
			const float32 icoY = by + (bh - ico) * 0.5f;
			const float32 ib = M("icon_btn");
			float32 bx = tb.x + pad;
			auto Survole = [&](const NkPaintRect &r) {
				return r.Contains(in.mouseX, in.mouseY);
			};
			auto Clic = [&](const NkPaintRect &r) {
				return in.mousePressed && r.Contains(in.mouseX, in.mouseY);
			};
			// Un bouton d'Unreal : fond discret, pictogramme a gauche, libelle.
			auto BoutonTexte = [&](const char *lbl, NkPaintRect &out) -> bool {
				const float32 w = pad + ico + pad * 0.5f + p.TextWidth(lbl) + pad;
				out = {bx, by, w, bh};
				p.Fill(out, Survole(out) ? rHover : s.chipBg, rd);
				p.Text({out.x + pad + ico + pad * 0.5f, out.y, w - pad * 2.f - ico - pad * 0.5f + pad, bh}, lbl, s.text);
				bx += w + pad * 0.5f;
				return Clic(out);
			};
			const bool actions = P("show_actions") > 0.5f;
			if (actions) {
				NkPaintRect r;
				if (BoutonTexte("Ajouter", r)) {
					res.ajouterDemande = true;
					res.menuBoutonX = r.x;
					res.menuBoutonY = r.y;
					res.menuBoutonW = r.w;
					res.menuBoutonH = r.h;
					if (hooks.onCreate)
						hooks.onCreate(hooks.user);
				}
				Plus(p, {r.x + pad, icoY, ico, ico}, rAjout);
				res.ajouterX = r.x;
				res.ajouterY = r.y;
				res.ajouterW = r.w;
				res.ajouterH = r.h;
				if (BoutonTexte("Importer", r) && hooks.onImport)
					hooks.onImport(hooks.user);
				Importer(p, {r.x + pad, icoY, ico, ico}, s.text, ep);
				res.importerX = r.x;
				res.importerY = r.y;
				res.importerW = r.w;
				res.importerH = r.h;
				if (BoutonTexte("Tout enregistrer", r) && hooks.onSaveAll)
					hooks.onSaveAll(hooks.user);
				Disquette(p, {r.x + pad, icoY, ico, ico}, s.text, r.Contains(in.mouseX, in.mouseY) ? rHover : s.chipBg);
				bx += pad * 0.5f;
			}
			// Precedent / suivant : grises quand il n'y a rien de ce cote.
			{
				const int32 n = (int32)m.historique.Size();
				for (int32 k = 0; k < 2; ++k) {
					const bool gauche = k == 0;
					const bool possible = gauche ? m.historiquePos > 0 : (m.historiquePos >= 0 && m.historiquePos + 1 < n);
					const NkPaintRect r{bx, by + (bh - ib * 0.8f) * 0.5f, ib * 0.8f, ib * 0.8f};
					if (possible && Survole(r))
						p.Fill({r.x - ep, r.y - ep, r.w + 2.f * ep, r.h + 2.f * ep}, rHover, r.w);
					FlecheRonde(p, r, possible ? s.text : s.textMuted, (possible && Survole(r)) ? rHover : s.headerBg, gauche, ep);
					if (gauche) {
						res.precedentX = r.x;
						res.precedentY = r.y;
						res.precedentW = r.w;
						res.precedentH = r.h;
					} else {
						res.suivantX = r.x;
						res.suivantY = r.y;
						res.suivantW = r.w;
						res.suivantH = r.h;
					}
					if (possible && Clic(r)) {
						m.historiquePos += gauche ? -1 : 1;
						res.allerA = m.historique[(uint32)m.historiquePos];
						// Pose tout de suite : sinon l'image suivante l'empilerait.
						m.cheminCourant = res.allerA;
						res.navigated = true;
						if (hooks.onNavigate)
							hooks.onNavigate(hooks.user, res.allerA.Data() ? res.allerA.Data() : "");
					}
					bx += r.w + pad * 0.5f;
				}
			}
			// A droite : « Reglages », puis le verrou a sa gauche.
			const float32 wReg = pad + ico + pad * 0.5f + p.TextWidth("Réglages") + pad;
			const NkPaintRect rReg{tb.x + tb.w - pad - wReg, by, wReg, bh};
			p.Fill(rReg, Survole(rReg) ? rHover : s.headerBg, rd);
			Engrenage(p, {rReg.x + pad, icoY, ico, ico}, s.text, Survole(rReg) ? rHover : s.headerBg, ep);
			p.Text({rReg.x + pad + ico + pad * 0.5f, rReg.y, wReg - pad - ico, bh}, "Réglages", s.text);
			if (Clic(rReg)) {
				res.reglagesDemandes = true;
				res.menuBoutonX = rReg.x;
				res.menuBoutonY = rReg.y;
				res.menuBoutonW = rReg.w;
				res.menuBoutonH = rReg.h;
			}
			const NkPaintRect rVer{rReg.x - pad * 0.5f - ib, by + (bh - ib) * 0.5f, ib, ib};
			if (m.verrouille)
				p.Fill(rVer, s.activeMark, rd);
			else if (Survole(rVer))
				p.Fill(rVer, rHover, rd);
			Verrou(p, {rVer.x + ib * 0.2f, rVer.y + ib * 0.15f, ib * 0.6f, ib * 0.7f}, m.verrouille ? rSurBleu : s.textMuted,
				   m.verrouille, ep);
			if (Clic(rVer))
				m.verrouille = !m.verrouille;
			// L'icone de dossier et le FIL D'ARIANE entre les deux.
			{
				const NkPaintRect rDos{bx, by, bh, bh};
				Silhouette(p, rDos, NkAssetIcone::Dossier, s.folderTint);
				bx += bh + pad * 0.25f;
				const NkPaintRect fil{bx, tb.y, rVer.x - pad - bx, tb.h};
				if (fil.w > 0.f) {
					p.PushClip(fil);
					float32 cx = fil.x;
					const uint32 nb = (uint32)m.breadcrumb.Size();
					for (uint32 i = 0; i < nb; ++i) {
						const char *t = m.breadcrumb[i].Data();
						if (!t)
							continue;
						const float32 tw = p.TextWidth(t);
						const NkPaintRect cell{cx, by, tw + pad, bh};
						const bool dernier = i + 1 == nb;
						if (Survole(cell) && !dernier)
							p.Fill(cell, rHover, rd);
						p.Text({cell.x + pad * 0.5f, cell.y, tw + pad * 0.5f, bh}, t, dernier || Survole(cell) ? s.text : s.textMuted);
						if (Clic(cell)) {
							res.navigated = true;
							res.navigatedCrumb = (int32)i;
							if (hooks.onNavigate)
								hooks.onNavigate(hooks.user, t);
						}
						cx += cell.w;
						if (!dernier) {
							Chevron(p, {cx, by, bh * 0.6f, bh}, s.textMuted, ep * 1.4f);
							cx += bh * 0.6f;
						}
					}
					p.PopClip();
				}
			}
			p.HLine(rect.x, tb.y + tbH, rect.w, s.border);

			// =================================================================
			//  LE CORPS : les sources a gauche, la vue des assets a droite
			// =================================================================
			const float32 corpsY = tb.y + tbH + ep;
			const float32 corpsB = rect.y + rect.h;
			res.panneauxY = corpsY;
			res.panneauxH = corpsB - corpsY;
			const bool sources = P("show_tree") > 0.5f && !m.treeCollapsed;
			const float32 srcW = sources ? rect.w * P("tree_width") : 0.f;

			// ── ZONES 2 ET 3 : LES SOURCES ──────────────────────────────────────
			if (sources) {
				const NkPaintRect src{rect.x, corpsY, srcW, corpsB - corpsY};
				p.Fill(src, s.headerBg);
				const float32 secH = M("section_h");
				const float32 icoS = secH * 0.5f;
				// La loupe cliquee A CETTE IMAGE : le champ qu'elle ouvre ne doit pas
				// perdre le focus sous le meme clic (il tombe hors du champ).
				bool loupeCliquee = false;
				// Une TETE de section d'Unreal : triangle, titre en gras, loupe (et
				// « + » pour les collections). Rend vrai si le « + » a ete clique.
				auto Section = [&](float32 y, const char *titre, bool &ouvert, uint8 idRecherche, bool plus) -> bool {
					const NkPaintRect h{src.x, y, src.w, secH};
					p.Fill(h, s.panelBg);
					const NkPaintRect tri{h.x + pad * 0.5f, h.y + (secH - icoS * 0.6f) * 0.5f, icoS * 0.6f, icoS * 0.6f};
					Triangle(p, tri, s.textMuted, ouvert);
					const NkPaintRect rLoupe{h.x + h.w - pad * 0.5f - icoS, h.y + (secH - icoS) * 0.5f, icoS, icoS};
					const NkPaintRect rPlus{rLoupe.x - pad * 0.5f - icoS, rLoupe.y, icoS, icoS};
					const bool chercheIci = m.sourcesRecherche == idRecherche;
					Loupe(p, rLoupe, chercheIci ? s.activeMark : s.textMuted, s.panelBg, ep);
					if (plus) {
						// le « + » cerclé d'Unreal
						Anneau(p, rPlus, s.textMuted, s.panelBg, ep * 1.2f);
						Plus(p, {rPlus.x + icoS * 0.25f, rPlus.y + icoS * 0.25f, icoS * 0.5f, icoS * 0.5f}, s.textMuted);
					}
					const float32 tx = tri.x + tri.w + pad * 0.5f;
					const float32 tw = (plus ? rPlus.x : rLoupe.x) - pad * 0.5f - tx;
					if (tw > 0.f) {
						p.PushClip({tx, h.y, tw, h.h});
						p.TextePolice({tx, h.y, tw, h.h}, titre, nullptr, s.text, 1);
						p.PopClip();
					}
					p.HLine(h.x, h.y + h.h, h.w, s.border);
					bool clicPlus = false;
					if (Clic({rLoupe.x - ep, rLoupe.y - ep, rLoupe.w + 2.f * ep, rLoupe.h + 2.f * ep})) {
						m.sourcesRecherche = chercheIci ? 0 : idRecherche;
						m.sourcesRechercheFocus = !chercheIci;
						loupeCliquee = true;
						if (!chercheIci)
							ouvert = true;
					} else if (plus && Clic(rPlus)) {
						clicPlus = true;
					} else if (Clic(h)) {
						ouvert = !ouvert;
					}
					return clicPlus;
				};
				// Le champ de recherche d'une section : peint ici (fond et invite),
				// la FRAPPE est a l'hote, qui a le clavier (meme partage que la
				// recherche de la vue).
				auto ChampRecherche = [&](float32 y, char *tampon, int32 taille) {
					const NkPaintRect f{src.x + pad * 0.5f, y + rowH * 0.1f, src.w - pad, rowH * 0.8f};
					p.Fill(f, s.cardBg, rd);
					p.OutlineSharp(f, m.sourcesRechercheFocus ? s.activeMark : s.border);
					if (!tampon[0])
						p.Text({f.x + pad * 0.5f, f.y, f.w - pad, f.h}, "Rechercher…", s.textMuted);
					res.sourcesRechercheX = f.x + pad * 0.25f;
					res.sourcesRechercheY = f.y;
					res.sourcesRechercheW = f.w - pad * 0.5f;
					res.sourcesRechercheH = f.h;
					res.sourcesTampon = tampon;
					res.sourcesTamponTaille = taille;
					if (in.mousePressed && !loupeCliquee)
						m.sourcesRechercheFocus = f.Contains(in.mouseX, in.mouseY);
				};
				// Une rangee de dossier hors de l'arbre (un FAVORI) : icone, nom.
				auto RangeeDossier = [&](float32 y, const NkString &chemin, uint32 couleur) {
					const NkPaintRect r{src.x, y, src.w, rowH};
					const bool actif = !m.cheminCourant.Empty() && NkBrowserMemeChemin(m.cheminCourant, chemin);
					const bool survol = Survole(r);
					const bool cible = survol && !m.glisserChemin.Empty() && !EstTraine(m, chemin);
					if (actif)
						p.Fill(r, s.activeMark);
					else if (survol)
						p.Fill(r, rHover);
					if (cible) {
						p.FillColor(r, Alpha(p.ColorOf(rAmbre), 0x40u));
						p.OutlineSharp(r, rAmbre);
						res.glisserCible = chemin;
					}
					Silhouette(p, {r.x + pad, r.y, rowH, rowH}, NkAssetIcone::Dossier, s.folderTint, 0, couleur);
					p.Text({r.x + pad + rowH + pad * 0.5f, r.y, r.w - rowH - pad * 2.f, rowH}, DernierSegment(chemin),
						   actif ? rSurBleu : s.text);
					if (survol)
						res.survolDossier = chemin;
					if (Clic(r)) {
						res.favoriClique = chemin;
						res.navigated = true;
						if (hooks.onNavigate)
							hooks.onNavigate(hooks.user, chemin.Data() ? chemin.Data() : "");
					}
					if (in.rightPressed && survol) {
						res.menuCheminRail = chemin;
						res.menuIndex = -1;
						res.menuX = in.mouseX;
						res.menuY = in.mouseY;
					}
				};

				float32 y = src.y;
				// ── Favoris ──
				if (P("show_favorites") > 0.5f) {
					Section(y, "Favoris", m.favorisOuverts, 1, false);
					y += secH;
					if (m.favorisOuverts) {
						if (m.sourcesRecherche == 1) {
							ChampRecherche(y, m.filtreFavoris, (int32)sizeof(m.filtreFavoris));
							y += rowH;
						}
						uint32 nb = 0;
						for (uint32 i = 0; i < (uint32)m.favoris.Size(); ++i) {
							if (!Contient(DernierSegment(m.favoris[i]), m.filtreFavoris))
								continue;
							RangeeDossier(y, m.favoris[i], i < (uint32)m.favorisCouleurs.Size() ? m.favorisCouleurs[i] : 0u);
							y += rowH;
							++nb;
						}
						if (nb == 0)
							y += rowH * 0.5f; // la bande vide d'Unreal : la section existe
					}
				}
				// ── Collections, ancrees en bas ── (calculees AVANT le projet : il
				// prend tout ce qu'elles laissent)
				const bool montrerCol = P("show_collections") > 0.5f;
				float32 colH = 0.f;
				uint32 nbCol = 0;
				if (montrerCol) {
					for (uint32 k = 0; k < (uint32)m.collections.Size(); ++k)
						if (Contient(m.collections[k].nom.Data(), m.filtreCollections))
							++nbCol;
					colH = secH;
					if (m.collectionsOuvertes)
						colH += (m.sourcesRecherche == 3 ? rowH : 0.f) + (float32)(nbCol > 0 ? nbCol : 1u) * rowH;
					const float32 plafond = (corpsB - y) * 0.45f;
					if (colH > plafond)
						colH = plafond;
				}
				// ── Le projet : son arbre de dossiers ──
				Section(y, m.nomProjet.Empty() ? "Projet" : m.nomProjet.Data(), m.projetOuvert, 2, false);
				y += secH;
				const float32 basProjet = corpsB - colH;
				if (m.projetOuvert) {
					if (m.sourcesRecherche == 2) {
						ChampRecherche(y, m.folders.filter, (int32)sizeof(m.folders.filter));
						y += rowH;
					}
					const NkPaintRect arbre{src.x, y, src.w, basProjet - y};
					if (arbre.h > rowH * 0.5f) {
						// LE VRAI ARBRE : le `tree_view` du kit, le meme pont que le mixte.
						TreeBridge pont;
						pont.res = &res;
						pont.hooks = &hooks;
						pont.folders = &m.folders;
						NkTreeViewStyle ts;
						ts.panelBg = s.headerBg;
						ts.headerBg = s.headerBg;
						ts.border = s.border;
						ts.text = s.text;
						ts.textMuted = s.textMuted;
						ts.rowHover = rHover;
						ts.activeMark = s.activeMark;
						ts.activeText = rSurBleu;
						ts.chosenMark = s.chosenMark;
						ts.guide = s.border;
						ts.dropMark = rAmbre;
						ts.iconTint = s.folderTint;
						ts.dimTint = s.textMuted;
						ts.icons = s.treeIcons;
						ts.values = &EmbeddedTreeValues(P("tree_default_open") > 0.5f);
						NkTreeViewHooks th;
						th.user = &pont;
						th.onSelect = &TreeOnSelect;
						th.onContextMenu = &TreeOnMenu;
						NkComponentInput inArbre = in;
						inArbre.dragType = m.glisserChemin.Empty() ? nullptr : "nkfile";
						inArbre.dragReleased = false;
						const NkTreeViewResult tr = NkDrawTreeView(p, inArbre, arbre, m.folders, ts, th);
						const bool surArbre = arbre.Contains(in.mouseX, in.mouseY);
						if (surArbre && tr.survoleIndex >= 0 && tr.survoleIndex < (int32)m.folders.nodes.Size()) {
							const NkTreeNode &nd = m.folders.nodes[(uint32)tr.survoleIndex];
							if (!nd.path.Empty()) {
								res.survolDossier = nd.path;
								// armer un glisser depuis le rail
								if (in.mousePressed && !nd.locked) {
									m.armeChemin = nd.path;
									m.armeLibelle = nd.label;
									m.armeChemins.Clear();
									m.armeChemins.PushBack(nd.path);
									m.armeRail = true;
									m.armeX = in.mouseX;
									m.armeY = in.mouseY;
								}
								// le rail est une cible
								if (!m.glisserChemin.Empty() && !EstTraine(m, nd.path))
									res.glisserCible = nd.path;
							}
						}
						if (!tr.infobulle.Empty()) {
							res.infobulle = tr.infobulle;
							res.infobulleX = arbre.x + arbre.w;
							res.infobulleY = tr.infobulleY;
							res.infobulleH = tr.infobulleH;
						}
						res.railDefilX = tr.defilX;
						res.railDefilY = tr.defilY;
						res.railDefilW = tr.defilW;
						res.railDefilH = tr.defilH;
						res.railDefilContenu = tr.defilContenu;
						res.railDefilVue = tr.defilVue;
						res.railDefilPas = tr.defilPas;
					}
				}
				// ── Collections ──
				if (montrerCol) {
					float32 yc = corpsB - colH;
					p.PushClip({src.x, yc, src.w, colH});
					p.HLine(src.x, yc, src.w, s.border);
					if (Section(yc, "Collections", m.collectionsOuvertes, 3, true))
						res.collectionCreer = true;
					yc += secH;
					if (m.collectionsOuvertes) {
						if (m.sourcesRecherche == 3) {
							ChampRecherche(yc, m.filtreCollections, (int32)sizeof(m.filtreCollections));
							yc += rowH;
						}
						for (uint32 k = 0; k < (uint32)m.collections.Size(); ++k) {
							const NkBrowserCollection &c = m.collections[k];
							if (!Contient(c.nom.Data(), m.filtreCollections))
								continue;
							const NkPaintRect r{src.x, yc, src.w, rowH};
							const bool actif = m.collectionActive == (int32)k;
							if (actif)
								p.Fill(r, s.activeMark);
							else if (Survole(r))
								p.Fill(r, rHover);
							// le carre de couleur de la collection
							const float32 q = rowH * 0.45f;
							const NkPaintRect carre{r.x + pad, r.y + (rowH - q) * 0.5f, q, q};
							if (c.couleur)
								p.FillColor(carre, c.couleur, q * 0.15f);
							else
								p.Fill(carre, s.chosenMark, q * 0.15f);
							// le compteur, a droite (Unreal : une pastille sombre)
							char nb[16];
							PutUInt(nb, sizeof(nb), 0, c.compte);
							const float32 wc = p.TextWidth(nb) + pad;
							const NkPaintRect pastille{r.x + r.w - pad - wc, r.y + rowH * 0.15f, wc, rowH * 0.7f};
							p.Fill(pastille, s.cardBg, rd);
							p.Text(pastille, nb, s.textMuted, NkTextAlign::Center);
							p.Text({carre.x + q + pad * 0.5f, r.y, pastille.x - carre.x - q - pad, rowH},
								   c.nom.Data() ? c.nom.Data() : "", actif ? rSurBleu : s.text);
							if (Clic(r)) {
								m.collectionActive = (int32)k;
								res.collectionCliquee = (int32)k;
								res.navigated = true;
							}
							if (in.rightPressed && Survole(r)) {
								res.menuCollection = (int32)k;
								res.menuIndex = -1;
								res.menuX = in.mouseX;
								res.menuY = in.mouseY;
							}
							yc += rowH;
						}
					}
					p.PopClip();
				}
				p.VLine(src.x + src.w, src.y, src.h, s.border);
			}

			// =================================================================
			//  ZONES 4, 5 ET 6 : LA VUE DES ASSETS
			// =================================================================
			const float32 ax = rect.x + srcW + (sources ? ep : 0.f);
			const float32 aw = rect.x + rect.w - ax;
			float32 y2 = corpsY;
			// ── ZONE 5 : entonnoir, recherche, tri ──
			{
				const float32 srH = M("filter_h");
				const NkPaintRect sr{ax, y2, aw, srH};
				p.Fill(sr, s.panelBg);
				const float32 cy = sr.y + (srH - bh) * 0.5f;
				const NkPaintRect fb{sr.x + pad, cy, bh * 1.5f, bh};
				p.Fill(fb, (Survole(fb) || m.filtresOuverts) ? rHover : s.panelBg, rd);
				Entonnoir(p, {fb.x + bh * 0.15f, cy + bh * 0.15f, bh * 0.7f, bh * 0.7f}, s.text, ep);
				Triangle(p, {fb.x + bh * 0.95f, cy + bh * 0.38f, bh * 0.3f, bh * 0.3f}, s.textMuted, true);
				if (Clic(fb))
					m.filtresOuverts = !m.filtresOuverts;
				const bool montrerTri = P("show_sort") > 0.5f;
				const NkPaintRect rt{sr.x + sr.w - pad - bh * 1.5f, cy, montrerTri ? bh * 1.5f : 0.f, bh};
				if (montrerTri) {
					p.Fill(rt, Survole(rt) ? rHover : s.panelBg, rd);
					Tri(p, {rt.x + bh * 0.1f, cy + bh * 0.15f, bh * 0.7f, bh * 0.7f}, s.text, ep);
					Triangle(p, {rt.x + bh * 0.95f, cy + bh * 0.38f, bh * 0.3f, bh * 0.3f}, s.textMuted, true);
					if (Clic(rt)) {
						res.triDemande = true;
						res.menuBoutonX = rt.x;
						res.menuBoutonY = rt.y;
						res.menuBoutonW = rt.w;
						res.menuBoutonH = rt.h;
					}
				}
				const float32 sx = fb.x + fb.w + pad * 0.5f;
				const NkPaintRect sb{sx, cy, (montrerTri ? rt.x : sr.x + sr.w) - pad * 0.5f - sx, bh};
				if (sb.w > 0.f) {
					p.Fill(sb, s.cardBg, rd);
					p.OutlineSharp(sb, m.searchFocused ? s.activeMark : s.border);
					Loupe(p, {sb.x + pad * 0.5f, sb.y + bh * 0.2f, bh * 0.6f, bh * 0.6f}, s.textMuted, s.cardBg, ep);
					const float32 tx = sb.x + pad + bh * 0.6f;
					if (m.filter[0]) {
						p.Text({tx, sb.y, sb.x + sb.w - tx - pad, bh}, m.filter, s.text);
					} else {
						// Unreal : « Search <dossier> ».
						char invite[160];
						uint32 at = PutStr(invite, sizeof(invite), 0, "Rechercher dans ");
						const uint32 nb = (uint32)m.breadcrumb.Size();
						PutStr(invite, sizeof(invite), at, nb ? (m.breadcrumb[nb - 1u].Data() ? m.breadcrumb[nb - 1u].Data() : "") : "le contenu");
						p.Text({tx, sb.y, sb.x + sb.w - tx - pad, bh}, invite, s.textMuted);
					}
					if (in.mousePressed)
						m.searchFocused = sb.Contains(in.mouseX, in.mouseY);
					res.rechercheX = tx - pad * 0.25f;
					res.rechercheY = sb.y;
					res.rechercheW = sb.x + sb.w - tx;
					res.rechercheH = sb.h;
				}
				y2 += srH;
			}
			// ── ZONE 4 : les puces de type ──
			if (m.filtresOuverts && !m.kinds.Empty()) {
				const float32 fH = M("filter_h");
				const NkPaintRect fr{ax, y2, aw, fH};
				p.Fill(fr, s.panelBg);
				const float32 ch = bh * 0.85f;
				const float32 cy = fr.y + (fH - ch) * 0.5f;
				float32 fx = fr.x + pad;
				for (uint32 k = 0; k < (uint32)m.kinds.Size(); ++k) {
					NkBrowserKind &kind = m.kinds[k];
					const char *lbl = kind.label.Data() ? kind.label.Data() : "";
					const float32 barre = pad * 0.4f;
					const float32 w = barre + pad + p.TextWidth(lbl) + pad;
					const NkPaintRect chip{fx, cy, w, ch};
					if (chip.x + chip.w > fr.x + fr.w - pad)
						break; // pas de puce a moitie coupee
					p.Fill(chip, Survole(chip) ? rHover : s.chipBg, rd);
					// La barre de couleur d'Unreal : pleine si la puce est enfoncee,
					// eteinte sinon.
					const uint32 c = p.ColorOf(kind.role);
					p.FillColor({chip.x, chip.y, barre, chip.h}, kind.active ? c : NkTeinter(c, -0.55f), rd);
					if (kind.active)
						p.OutlineSharp(chip, kind.role);
					p.Text({chip.x + barre + pad * 0.75f, chip.y, w - barre - pad, ch}, lbl, kind.active ? s.text : s.textMuted);
					if (Clic(chip))
						kind.active = !kind.active;
					fx += w + pad * 0.5f;
				}
				y2 += fH;
			}
			p.HLine(ax, y2, aw, s.border);

			// ── LA LIGNE D'ETAT (en bas de la vue) ──
			const float32 stH = M("status_h");
			const NkPaintRect st{ax, corpsB - stH, aw, stH};

			// ── ZONE 6 : LA GRILLE ──
			const NkPaintRect zone{ax, y2 + ep, aw, st.y - (y2 + ep)};
			float32 gouttiere = M("scrollbar_w");
			if (gouttiere > zone.w * 0.5f)
				gouttiere = zone.w * 0.5f;
			if (gouttiere < 0.f)
				gouttiere = 0.f;
			NkPaintRect area = zone;
			area.w -= gouttiere;
			if (area.w < 0.f)
				area.w = 0.f;
			if (area.h < 0.f)
				area.h = 0.f;

			// La liste visible : dossiers (si le reglage les montre), recherche,
			// puces, filtre de l'application ; puis le tri (dossiers d'abord).
			NkVector<int32> vis;
			for (uint32 i = 0; i < (uint32)m.entries.Size(); ++i) {
				const NkAssetEntry &e = m.entries[i];
				if (e.isFolder && !m.montrerDossiers)
					continue;
				if (!PassesFilter(e, m.filter) || !PassesKinds(m, e))
					continue;
				if (hooks.acceptEntry && !hooks.acceptEntry(hooks.user, e))
					continue;
				vis.PushBack((int32)i);
			}
			for (uint32 i = 1; i < (uint32)vis.Size(); ++i) {
				const int32 v = vis[i];
				uint32 j = i;
				while (j > 0) {
					const bool avant = SortBefore(m.entries[(uint32)v], m.entries[(uint32)vis[j - 1]], (NkBrowserTri)m.sortCle);
					// Les dossiers restent EN TETE dans les deux sens.
					const bool dossiers = m.entries[(uint32)v].isFolder != m.entries[(uint32)vis[j - 1]].isFolder;
					if (dossiers ? !avant : (m.sortAsc ? !avant : avant))
						break;
					vis[j] = vis[j - 1];
					--j;
				}
				vis[j] = v;
			}
			res.nbVisibles = (int32)vis.Size();

			const bool liste = m.viewMode == 1;
			const float32 g = M("card_gap") * 0.5f; // Unreal serre ses cartes
			float32 thumb = (m.thumbSize > 0.f ? m.thumbSize : P("thumb_size")) * in.surfaceScale;
			const float32 bande = M("type_band");
			const float32 piedH = bande + pad * 0.25f + 3.f * lh + pad * 0.25f; // nom (2 lignes) + type
			// En bande courte, la vignette cede (le pied porte l'information) ;
			// plancher d'une ligne et demie.
			if (!liste && thumb + piedH + 2.f * g > area.h) {
				thumb = area.h - piedH - 2.f * g;
				if (thumb < rowH * 1.5f)
					thumb = rowH * 1.5f;
			}
			const float32 carteW = thumb;
			const float32 carteH = thumb + piedH;
			const float32 pasX = carteW + g;
			const float32 pasY = liste ? rowH : carteH + g;
			int32 parRang = liste ? 1 : (int32)((area.w - g) / (pasX > 0.f ? pasX : 1.f));
			if (parRang < 1)
				parRang = 1;
			auto RectDe = [&](int32 vi) -> NkPaintRect {
				if (liste)
					return {area.x, area.y + (float32)vi * rowH - m.scroll, area.w, rowH};
				const int32 col = vi % parRang, rang = vi / parRang;
				return {area.x + g + (float32)col * pasX, area.y + g + (float32)rang * pasY - m.scroll, carteW, carteH};
			};

			p.PushClip(area);
			const bool dansZone = area.Contains(in.mouseX, in.mouseY);
			int32 hit = -1, hitVi = -1;
			for (uint32 vi = 0; vi < (uint32)vis.Size(); ++vi) {
				const int32 idx = vis[vi];
				const NkAssetEntry &e = m.entries[(uint32)idx];
				const NkPaintRect c = RectDe((int32)vi);
				if (c.y + c.h < area.y || c.y > area.y + area.h)
					continue;
				if (res.premierVisible < 0 || idx < res.premierVisible)
					res.premierVisible = idx;
				if (idx > res.dernierVisible)
					res.dernierVisible = idx;
				const bool choisi = m.IsChosen(idx);
				const bool actif = m.active == idx;
				const bool survol = dansZone && c.Contains(in.mouseX, in.mouseY);
				const bool cible = survol && e.isFolder && !m.glisserChemin.Empty() && !EstTraine(m, e.path);
				if (survol) {
					hit = idx;
					hitVi = (int32)vi;
					if (e.isFolder)
						res.survolDossier = e.path;
					if (cible)
						res.glisserCible = e.path;
				}
				// La SELECTION SECONDAIRE (choisie, pas active) : un bleu plus sourd et
				// l'anneau ambre -- la touche de la famille (document 02 §3.1).
				const uint32 bleuSourd = NkTeinter(p.ColorOf(s.activeMark), -0.35f);
				const uint16 texteNom = choisi ? rSurBleu : s.text;

				if (liste) {
					// ── une RANGEE : vignette 1 ligne, nom, type ──
					if (choisi && actif)
						p.Fill(c, s.activeMark);
					else if (choisi)
						p.FillColor(c, bleuSourd);
					else if (survol)
						p.Fill(c, rHover);
					p.Fill({c.x, c.y, bande * 1.5f, c.h}, e.isFolder ? s.folderTint : (e.kindRole ? e.kindRole : s.border));
					const NkPaintRect zi{c.x + pad, c.y, rowH, rowH};
					if (!DrawVignette(p, zi, e, idx, hooks) &&
						!(hooks.vignetteApp && !e.isFolder && hooks.vignetteApp(hooks.user, p, idx, zi.x, zi.y, zi.w, zi.h)))
						Silhouette(p, zi, IconeDe(e), e.isFolder ? s.folderTint : e.kindRole, e.contenu, e.couleur);
					const NkPaintRect rn{c.x + pad * 1.5f + rowH, c.y, c.w * 0.55f - rowH - pad * 1.5f, rowH};
					if (m.renomme == idx) {
						res.renommeX = rn.x;
						res.renommeY = rn.y;
						res.renommeW = rn.w;
						res.renommeH = rn.h;
					} else {
						p.Text(rn, Label(e), texteNom);
					}
					p.Text({c.x + c.w * 0.55f, c.y, c.w * 0.25f, rowH}, e.isFolder ? "Dossier" : (e.kindLabel ? e.kindLabel : ""),
						   choisi ? rSurBleu : s.textMuted);
					if (hooks.extraColumnText && hooks.extraColumnCount > 0) {
						const float32 colW = c.w * 0.2f / (float32)hooks.extraColumnCount;
						for (int32 k = 0; k < hooks.extraColumnCount; ++k) {
							const char *t = hooks.extraColumnText(hooks.user, idx, k);
							if (t)
								p.Text({c.x + c.w * 0.8f + (float32)k * colW, c.y, colW, rowH}, t, choisi ? rSurBleu : s.textMuted);
						}
					}
					if (choisi && !actif)
						p.OutlineSharp(c, rAmbre);
					if (cible) {
						p.FillColor(c, Alpha(p.ColorOf(rAmbre), 0x40u));
						p.OutlineSharp(c, rAmbre);
					}
				} else if (e.isFolder) {
					// ── LA CARTE DE DOSSIER (Unreal) : grande icone, nom dessous ──
					if (choisi && actif)
						p.Fill(c, s.activeMark, rd);
					else if (choisi)
						p.FillColor(c, bleuSourd, rd);
					else if (survol)
						p.Fill(c, rHover, rd);
					const NkPaintRect zi{c.x + thumb * 0.1f, c.y + thumb * 0.06f, thumb * 0.8f, thumb * 0.8f};
					Silhouette(p, zi, IconeDe(e), s.folderTint, e.contenu, e.couleur);
					const NkPaintRect rn{c.x + pad * 0.25f, c.y + thumb * 0.9f, c.w - pad * 0.5f, 2.f * lh};
					if (m.renomme == idx) {
						res.renommeX = rn.x;
						res.renommeY = rn.y;
						res.renommeW = rn.w;
						res.renommeH = lh + pad * 0.5f;
					} else {
						DeuxLignes(p, rn, Label(e), texteNom, true, lh);
					}
					if (choisi && !actif)
						p.OutlineSharp(c, rAmbre);
					if (cible) {
						p.FillColor(c, Alpha(p.ColorOf(rAmbre), 0x40u), rd);
						p.OutlineSharp(c, rAmbre);
						p.OutlineSharp({c.x + ep, c.y + ep, c.w - 2.f * ep, c.h - 2.f * ep}, rAmbre);
					}
				} else {
					// ── LA CARTE D'ASSET (Unreal) ──
					const NkPaintRect zv{c.x, c.y, c.w, thumb};
					p.Fill(zv, survol && !choisi ? rHover : s.cardBg, rd);
					const NkPaintRect zi{zv.x + pad * 0.5f, zv.y + pad * 0.5f, zv.w - pad, zv.h - pad};
					if (zi.w > res.zoneVignettePx)
						res.zoneVignettePx = zi.w;
					if (!DrawVignette(p, zi, e, idx, hooks) &&
						!(hooks.vignetteApp && hooks.vignetteApp(hooks.user, p, idx, zi.x, zi.y, zi.w, zi.h)))
						Silhouette(p, zi, IconeDe(e), e.kindRole, e.contenu, e.couleur);
					// la BANDE du type, fine, juste sous la vignette
					p.Fill({c.x, zv.y + thumb, c.w, bande}, e.kindRole ? e.kindRole : s.border);
					// le pied : nom (deux lignes), type en gris
					const NkPaintRect pied{c.x, zv.y + thumb + bande, c.w, c.h - thumb - bande};
					if (choisi && actif)
						p.Fill(pied, s.activeMark, rd);
					else if (choisi)
						p.FillColor(pied, bleuSourd, rd);
					else
						p.Fill(pied, survol ? rHover : s.cardFooterBg, rd);
					const NkPaintRect rn{pied.x + pad * 0.5f, pied.y + pad * 0.25f, pied.w - pad, 2.f * lh};
					if (m.renomme == idx) {
						res.renommeX = rn.x - pad * 0.25f;
						res.renommeY = rn.y;
						res.renommeW = rn.w + pad * 0.5f;
						res.renommeH = lh + pad * 0.5f;
					} else {
						DeuxLignes(p, rn, Label(e), texteNom, false, lh);
					}
					p.Text({pied.x + pad * 0.5f, pied.y + pied.h - lh - pad * 0.25f, pied.w - pad, lh},
						   e.kindLabel ? e.kindLabel : "", choisi ? rSurBleu : s.textMuted);
					if (choisi && !actif)
						p.OutlineSharp(c, rAmbre);
					else if (actif && choisi)
						p.OutlineSharp(c, s.activeMark);
				}
				if (hooks.cardOverlay)
					hooks.cardOverlay(hooks.user, p, idx, c.x, c.y, c.w, c.h);
			}

			// ── L'ETAT VIDE D'UNREAL ──
			if (vis.Empty()) {
				res.vide = true;
				bool filtre = m.filter[0] != '\0';
				for (uint32 k = 0; k < (uint32)m.kinds.Size(); ++k)
					filtre = filtre || m.kinds[k].active;
				const float32 cy = area.y + area.h * 0.4f;
				p.Text({area.x, cy - lh, area.w, lh}, filtre ? "Aucun élément ne correspond au filtre." : "Aucun résultat.",
					   s.textMuted, NkTextAlign::Center);
				p.Text({area.x, cy, area.w, lh}, "Déposez des fichiers ici ou faites un clic droit pour créer du contenu.",
					   s.textMuted, NkTextAlign::Center);
			}

			// =================================================================
			//  LES GESTES DE LA VUE -- un seul endroit, apres le dessin
			// =================================================================
			// ⚠️ (2026-10-01, retour de Rihen) LE CHAMP DU RENOMMAGE EST A L'HOTE :
			//    la souris y place le curseur, y choisit du texte (glisser,
			//    double-clic = un mot). Sans cette garde, l'appui dans le champ
			//    ARMAIT le glisser de la carte (on tirait l'asset au lieu de choisir
			//    des lettres), le double-clic OUVRAIT l'asset et le clic droit
			//    ouvrait son menu : « ce n'est pas un vrai champ ».
			const bool dansRenommage = m.renomme >= 0 && res.renommeW > 0.f &&
									   NkPaintRect{res.renommeX, res.renommeY, res.renommeW, res.renommeH}.Contains(in.mouseX, in.mouseY);
			if (in.mousePressed && dansZone && !dansRenommage) {
				if (hit >= 0) {
					const NkAssetEntry &e = m.entries[(uint32)hit];
					if (in.shift && multi) {
						// LA PLAGE, dans l'ordre VISIBLE, depuis l'ancre (Maj+clic).
						const int32 ancre = m.ancre >= 0 ? m.ancre : (m.active >= 0 ? m.active : hit);
						int32 a = hitVi;
						for (uint32 k = 0; k < (uint32)vis.Size(); ++k)
							if (vis[k] == ancre)
								a = (int32)k;
						if (!in.ctrl)
							m.chosen.Clear();
						const int32 lo = a < hitVi ? a : hitVi, hi = a < hitVi ? hitVi : a;
						for (int32 k = lo; k <= hi; ++k)
							if (!m.IsChosen(vis[(uint32)k]))
								m.chosen.PushBack(vis[(uint32)k]);
						m.active = hit;
						m.ancre = ancre;
					} else if (in.ctrl && multi) {
						if (m.IsChosen(hit)) {
							for (uint32 k = 0; k < (uint32)m.chosen.Size(); ++k)
								if (m.chosen[k] == hit) {
									m.chosen.RemoveAt(k);
									break;
								}
							m.active = m.chosen.Empty() ? -1 : m.chosen[(uint32)m.chosen.Size() - 1u];
						} else {
							m.chosen.PushBack(hit);
							m.active = hit;
						}
						m.ancre = hit;
					} else if (m.IsChosen(hit) && m.chosen.Size() > 1u) {
						// Reduite AU RELACHEMENT, s'il n'y a pas eu de glisser.
						m.reduireAuRelache = hit;
						m.active = hit;
						m.ancre = hit;
					} else {
						m.chosen.Clear();
						m.chosen.PushBack(hit);
						m.active = hit;
						m.ancre = hit;
					}
					res.selectionChanged = true;
					if (hooks.onSelect)
						hooks.onSelect(hooks.user, hit, e.path.Data() ? e.path.Data() : "");
					// L'appui ARME un glisser de TOUTE la selection (si la carte en est).
					if (m.IsChosen(hit) && !e.path.Empty()) {
						m.armeChemin = e.path;
						m.armeChemins.Clear();
						m.armeChemins.PushBack(e.path);
						for (uint32 k = 0; k < (uint32)vis.Size(); ++k) {
							const int32 j = vis[k];
							if (j != hit && m.IsChosen(j) && !m.entries[(uint32)j].path.Empty())
								m.armeChemins.PushBack(m.entries[(uint32)j].path);
						}
						if (m.armeChemins.Size() > 1u) {
							char lbl[48];
							uint32 at = PutUInt(lbl, sizeof(lbl), 0, (uint32)m.armeChemins.Size());
							PutStr(lbl, sizeof(lbl), at, " éléments");
							m.armeLibelle = NkString(lbl);
						} else {
							m.armeLibelle = e.name;
						}
						m.armeRail = false;
						m.armeX = in.mouseX;
						m.armeY = in.mouseY;
					}
				} else {
					// Le VIDE : la selection tombe (sauf Ctrl / Maj), et un CADRE s'arme.
					if (!in.ctrl && !in.shift && (!m.chosen.Empty() || m.active >= 0)) {
						m.ClearSelection();
						res.selectionChanged = true;
					}
					m.cadreArme = true;
					m.cadreActif = false;
					m.cadreX0 = in.mouseX;
					m.cadreY0 = in.mouseY + m.scroll; // en coordonnees de CONTENU
					m.cadreAvant = m.chosen;
				}
			}
			// ── LE CADRE DE SELECTION ──
			if (m.cadreArme && !in.mouseDown && !in.mousePressed) {
				m.cadreArme = false;
				m.cadreActif = false;
			}
			if (m.cadreActif && in.mouseDown) {
				const float32 y0 = m.cadreY0 - m.scroll;
				const NkPaintRect cad{m.cadreX0 < in.mouseX ? m.cadreX0 : in.mouseX, y0 < in.mouseY ? y0 : in.mouseY,
									  m.cadreX0 < in.mouseX ? in.mouseX - m.cadreX0 : m.cadreX0 - in.mouseX,
									  y0 < in.mouseY ? in.mouseY - y0 : y0 - in.mouseY};
				p.FillColor(cad, Alpha(p.ColorOf(s.activeMark), 0x30u));
				p.OutlineSharp(cad, s.activeMark);
				m.chosen = in.ctrl ? m.cadreAvant : NkVector<int32>();
				for (uint32 k = 0; k < (uint32)vis.Size(); ++k) {
					const NkPaintRect c = RectDe((int32)k);
					const bool coupe = c.x < cad.x + cad.w && c.x + c.w > cad.x && c.y < cad.y + cad.h && c.y + c.h > cad.y;
					if (coupe && !m.IsChosen(vis[k])) {
						m.chosen.PushBack(vis[k]);
						m.active = vis[k];
					}
				}
				if (m.chosen.Empty())
					m.active = -1;
				res.selectionChanged = true;
			}
			// ── LE RELACHEMENT SANS GLISSER : la selection se reduit a la carte ──
			if (in.mouseReleased && m.reduireAuRelache >= 0) {
				if (m.glisserChemin.Empty()) {
					m.chosen.Clear();
					m.chosen.PushBack(m.reduireAuRelache);
					m.active = m.reduireAuRelache;
					res.selectionChanged = true;
				}
				m.reduireAuRelache = -1;
			}
			// ── DOUBLE-CLIC : ouvrir (le composant signale) ──
			if (in.doubleClick && dansZone && hit >= 0 && !dansRenommage) {
				res.activatedIndex = hit;
				if (hooks.onDoubleClick)
					hooks.onDoubleClick(hooks.user, hit, m.entries[(uint32)hit].path.Data() ? m.entries[(uint32)hit].path.Data() : "");
			}
			// ── CLIC DROIT : la carte (choisie si elle ne l'etait pas), ou le vide ──
			if (in.rightPressed && dansZone && !dansRenommage) {
				if (hit >= 0) {
					if (!m.IsChosen(hit)) {
						m.chosen.Clear();
						m.chosen.PushBack(hit);
						m.ancre = hit;
						res.selectionChanged = true;
					}
					m.active = hit;
					res.menuIndex = hit;
				} else {
					if (!m.chosen.Empty() || m.active >= 0) {
						m.ClearSelection();
						res.selectionChanged = true;
					}
					res.menuIndex = -1;
				}
				res.menuX = in.mouseX;
				res.menuY = in.mouseY;
				if (hooks.onContextMenu)
					hooks.onContextMenu(hooks.user, res.menuIndex, in.mouseX, in.mouseY);
			}
			if (in.dragReleased && hit >= 0 && m.entries[(uint32)hit].isFolder && hooks.onDrop)
				hooks.onDrop(hooks.user, hit, in.dragType ? in.dragType : "");

			// ── LA MOLETTE : defiler ; avec Ctrl, la TAILLE des vignettes (Unreal) ──
			const int32 rangs = liste ? (int32)vis.Size() : ((int32)vis.Size() + parRang - 1) / parRang;
			const float32 contenuH = (float32)rangs * pasY + (liste ? 0.f : g);
			const float32 maxScroll = contenuH > area.h ? contenuH - area.h : 0.f;
			if (in.wheel != 0.f && zone.Contains(in.mouseX, in.mouseY)) {
				if (in.ctrl) {
					const NkParamDecl *tp = NkContentBrowserDecl().FindParam("thumb_size");
					const float32 lo = tp ? tp->minVal : 0.f, hi = tp ? tp->maxVal : 1.f;
					float32 t = (m.thumbSize > 0.f ? m.thumbSize : P("thumb_size")) + in.wheel * (hi - lo) / 16.f;
					m.thumbSize = t < lo ? lo : (t > hi ? hi : t);
				} else {
					m.scroll -= in.wheel * pasY * 0.5f;
				}
			}
			if (m.scroll > maxScroll)
				m.scroll = maxScroll;
			if (m.scroll < 0.f)
				m.scroll = 0.f;
			res.defilX = area.x + area.w;
			res.defilY = area.y;
			res.defilW = gouttiere;
			res.defilH = area.h;
			res.defilContenu = contenuH;
			res.defilVue = area.h;
			res.defilPas = pasY;

			// ── LE LACHER D'UN GLISSER : source(s) et cible, une seule image ──
			if (!m.glisserChemin.Empty()) {
				res.glisserChemin = m.glisserChemin;
				res.glisserLibelle = m.glisserLibelle;
				res.glisserX = in.mouseX;
				res.glisserY = in.mouseY;
				if (in.mouseReleased) {
					if (!res.glisserCible.Empty()) {
						res.deposeSource = m.glisserChemin;
						res.deposeSources = m.glisserChemins;
						res.deposeCible = res.glisserCible;
						res.deposeX = in.mouseX;
						res.deposeY = in.mouseY;
					}
					m.AnnulerGlisser();
					res.glisserCible = NkString();
				}
			}
			p.PopClip(); // area

			// ── LA LIGNE D'ETAT : « N elements (M selectionnes) » ──
			{
				p.Fill(st, s.panelBg);
				char t[96];
				NkContentBrowserTexteEtat(t, sizeof(t), (uint32)vis.Size(), (uint32)m.chosen.Size());
				p.Text({st.x + pad, st.y, st.w * 0.6f, st.h}, t, s.textMuted);
				const char *droite = m.statusRight.Data();
				if (droite && droite[0])
					p.Text({st.x + st.w * 0.5f, st.y, st.w * 0.5f - pad, st.h}, droite, s.textMuted, NkTextAlign::Right);
			}

			p.PopClip(); // rect
			return res;
		}

	} // namespace editorkit
} // namespace nkentseu
