#pragma once
// -----------------------------------------------------------------------------
// @File    Export.h
// @Brief   L'EXPORT EN IMAGE (PNG) d'une page ou d'une selection, a l'echelle
//          1x / 2x / 3x -- le MEME peintre que la toile, rendu sans GPU.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUE C'EST, EN UNE PHRASE
// =============================================================================
//  Le peintre de la toile (`NkDesignPaint` -> `NkGuiDrawList`) dessine le
//  document dans une liste hors ecran, sous une transformee d'echelle ; le
//  rasteriseur logiciel de NKGui (`NkGuiDrawListRaster`) rend cette liste en
//  pixels ; le codec PNG de NKImage ecrit le fichier. Aucune seconde chaine de
//  dessin : degrades, images, melange, bordures, ombres sont ceux de l'ecran.
//
//  MESURE AVANT D'ECRIRE (05/09) :
//    - NKImage porte le codec PNG maison en ecriture (`NkImage::SavePNG`,
//      RFC 2083) -- rien a ecrire de ce cote ;
//    - NKCanvas porte un dorsal Software et une cible hors ecran, mais son
//      contexte exige une FENETRE (`NkSoftwareContext::Initialize(const
//      NkWindow &, ...)`) : le temoin sans fenetre ne pouvait pas s'y appuyer.
//      D'ou le rasteriseur de NKGui, la couche qui possede la liste.
//    - UNE FENETRE N'EST PAS NECESSAIRE : tout ce fichier tourne dans `--probe`.
//
//  LE TEXTE A L'ECHELLE, ET POURQUOI CE N'EST PAS UN AGRANDISSEMENT DE BITMAP :
//  `NkDesignPaint::TextHex` prend l'atlas du costume le plus proche (9..16 px)
//  et le laisse etirer par la matrice. A 2x, un corps de 14 deviendrait un
//  atlas de 16 px etire 1,75 fois : flou. Le peintre d'export charge la police
//  embarquee (Inter) A LA TAILLE EXACTE corps x echelle, et retire l'echelle de
//  la matrice pour le texte : chaque glyphe est rasterise a sa taille finale.
//  `policeExacte = false` garde le chemin de la toile -- c'est la mutation qui
//  prouve la difference (sonde 81).
//
//  CE QUI N'EST PAS ICI, NOMME : le PDF (le meme arbre que le SVG, a ecrire
//  comme un lecteur de plus du format), HTML / CSS / React / Next (les
//  composants et les styles nommes) -- voir le document 14.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkFilePicker.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkPath.h"
#include "NKGui/Core/NkGuiDrawListRaster.h"
#include "NKGui/Core/NkGuiFont.h"
#include "NKImage/NKImage.h"
#include "NKMemory/NkAllocator.h"

#include "Panels.h"

#include <cmath>
#include <cstdio>

namespace nkuidesign {

	// ── LES OPTIONS ET LE RESULTAT ────────────────────────────────────────────
	enum class NkExportFormat : nkentseu::uint8 { PNG = 0, SVG = 1 };

	struct NkExportOptions {
			NkExportFormat format = NkExportFormat::PNG;
			nkentseu::float32 echelle = 1.f; ///< 1, 2, 3 (bornee a 0,25..8)
			bool selection = false;			 ///< faux = la page ; vrai = les noeuds selectionnes
			/// ② (07/09) TOUT LE CANVAS : les elements exportables de la racine, dans
			/// leur boite englobante. Prime sur `selection` quand elle est fausse ; sans
			/// lui, un export sans selection retombait sur LA PREMIERE page, ce qui est
			/// arbitraire des que le document en porte plusieurs.
			bool tout = false;
			nkentseu::int32 page = -1;		 ///< -1 = la page de la selection, sinon la premiere
			bool embarquer = false;			 ///< SVG : les images en data: base64 au lieu d'un chemin relatif
			bool policeExacte = true;		 ///< faux = l'atlas du costume etire (la mutation de la sonde)
			/// ④ (05/09, apres-midi) UN FICHIER PAR OBJET quand plusieurs noeuds sont choisis.
			/// Faux = une seule image, sur la boite englobante de la selection (le defaut).
			/// ⚠️ Sans effet a un seul objet : les deux donnent le meme fichier, et le dialogue
			///    grise donc le choix plutot que de laisser croire a une difference.
			bool unFichierParObjet = false;
	};

	struct NkExportResultat {
			bool ok = false;
			char message[512] = {};
			nkentseu::int32 largeur = 0, hauteur = 0; ///< pixels ecrits
			NkPaintRect zone;						  ///< la zone DOCUMENT exportee
			nkentseu::float32 policeMax = 0.f;		  ///< la plus grande taille de police chargee (px)
			nkentseu::uint32 polices = 0;			  ///< polices chargees
			nkentseu::uint32 images = 0;			  ///< textures d'image declarees
			nkentseu::uint32 texturesInconnues = 0;	  ///< commandes texturees sans texture (doit etre 0)
			nkentseu::uint32 noeuds = 0;			  ///< noeuds racines dessines
			nkentseu::float32 tailles[24] = {};	  ///< les tailles de police chargees, en px
			nkentseu::uint32 nbTailles = 0;
			/// Une police a EXACTEMENT cette taille a-t-elle ete chargee ?
			bool PoliceChargee(nkentseu::float32 px) const {
				for (nkentseu::uint32 i = 0; i < nbTailles; ++i)
					if (tailles[i] > px - 0.05f && tailles[i] < px + 0.05f)
						return true;
				return false;
			}
	};

	// ── LE PEINTRE D'EXPORT ──────────────────────────────────────────────────
	// Herite du peintre de la toile : formes, degrades, images, melange sont les
	// siens. Seul le TEXTE change de police (taille exacte) ; `Text` (par role)
	// passe par `TextHex` pour vivre sous la matrice comme tout le reste.
	class NkExportPaint : public NkDesignPaint {
		public:
			NkExportPaint(nkentseu::nkgui::NkGuiContext &ctx, const NkTheme &theme,
						  nkentseu::nkgui::NkGuiDrawListRaster *raster, bool policeExacte) noexcept
				: NkDesignPaint(ctx, theme), mCtx(ctx), mRaster(raster), mExacte(policeExacte) {}
			~NkExportPaint() override {
				for (nkentseu::int32 i = 0; i < mNb; ++i)
					if (mPolices[i].f)
						nkentseu::memory::NkGetDefaultAllocator().Delete(mPolices[i].f);
			}

			nkentseu::float32 policeMax = 0.f;
			nkentseu::uint32 Polices() const { return (nkentseu::uint32)mNb; }
			nkentseu::float32 Taille(nkentseu::uint32 i) const { return i < (nkentseu::uint32)mNb ? mPolices[i].taille : 0.f; }

			/// Le facteur d'echelle de la matrice en vigueur (racine du determinant).
			nkentseu::float32 Facteur() const {
				const nkentseu::editorkit::NkPaintTransform *m = TransformeActive();
				if (!m)
					return 1.f;
				const nkentseu::float32 det = m->a * m->d - m->b * m->c;
				const nkentseu::float32 k = std::sqrt(det < 0.f ? -det : det);
				return k > 0.001f ? k : 1.f;
			}

			nkentseu::float32 LineHeight() const override {
				const nkentseu::float32 k = Facteur();
				const nkentseu::nkgui::NkGuiFont *f = const_cast<NkExportPaint *>(this)->Police(12.f * k);
				return f ? f->LineHeight() / k : 16.f;
			}
			nkentseu::float32 TextWidth(const char *s) const override {
				const nkentseu::float32 k = Facteur();
				const nkentseu::nkgui::NkGuiFont *f = const_cast<NkExportPaint *>(this)->Police(12.f * k);
				return (f && s) ? f->MeasureWidth(s) / k : 0.f;
			}
			void Text(const NkPaintRect &r, const char *s, nkentseu::uint16 role,
					  nkentseu::editorkit::NkTextAlign align) override {
				// le texte par role (composants du kit) : corps 12, la couleur du role
				TextHex(r, s, ColorOf(role), role, align, 12.f, 0.f);
			}

			void TextHex(const NkPaintRect &r, const char *s, nkentseu::uint32 rgba, nkentseu::uint16 roleRepli,
						 nkentseu::editorkit::NkTextAlign align, nkentseu::float32 px,
						 nkentseu::float32 graisse) override {
				using nkentseu::float32;
				if (!s || !*s)
					return;
				if (px <= 0.f)
					px = 12.f;
				const nkentseu::editorkit::NkPaintTransform *m = TransformeActive();
				const float32 k = mExacte ? Facteur() : 1.f; // la mutation : l'atlas de 1x, etire par la matrice
				nkentseu::nkgui::NkGuiFont *f = Police(px * k);
				if (!f || !f->Valid()) {
					NkDesignPaint::TextHex(r, s, rgba, roleRepli, align, px, graisse);
					return;
				}
				const nkentseu::nkgui::NkColor col = {(nkentseu::uint8)((rgba >> 24) & 0xFFu),
													  (nkentseu::uint8)((rgba >> 16) & 0xFFu),
													  (nkentseu::uint8)((rgba >> 8) & 0xFFu),
													  (nkentseu::uint8)(rgba & 0xFFu)};
				// la geometrie en unites DOCUMENT : largeur / k, ligne / k, ascent / k
				const float32 largeur = f->MeasureWidth(s) / k;
				float32 tx = r.x;
				if (align == nkentseu::editorkit::NkTextAlign::Center)
					tx = r.x + (r.w - largeur) * 0.5f;
				else if (align == nkentseu::editorkit::NkTextAlign::Right)
					tx = r.x + r.w - largeur;
				const float32 hLigne = f->LineHeight() / k;
				const float32 yBase = r.y + (r.h - hLigne) * 0.5f + f->Ascent() / k;
				// la graisse : le meme geste que la toile (deux passes decalees), en px d'atlas
				const float32 eGras = graisse >= 500.f ? (graisse >= 700.f ? 0.8f : graisse >= 600.f ? 0.5f : 0.3f) : 0.f;
				const nkentseu::int32 passes = eGras > 0.f ? 2 : 1;
				if (!m) {
					for (nkentseu::int32 p = 0; p < passes; ++p)
						mCtx.DL().AddText(f->Face(), f->TexId(), {tx + (p ? eGras : 0.f), yBase}, s, col);
					return;
				}
				// LA PORTE DU KIT DECIDE (palier B) : perspective -> chaque glyphe par ses
				// quatre coins, donc le PNG exporte la MEME silhouette que l'ecran, texte
				// compris ; affine -> la matrice sans son echelle (le glyphe est deja a la
				// bonne taille), la translation amenant la ligne de base la ou la matrice
				// complete l'aurait mise. Une seule decision, pour les trois peintres.
				for (nkentseu::int32 p = 0; p < passes; ++p)
					nkentseu::editorkit::NkTexteTransforme(mCtx.DL(), f->Face(), f->TexId(),
														   {tx + (p ? eGras / k : 0.f), yBase}, s, col, *m,
														   1.f / k);
			}

		private:
			struct Police_ {
					nkentseu::float32 taille = 0.f;
					nkentseu::nkgui::NkGuiFont *f = nullptr;
			};
			enum { kMaxPolices = 24 };

			/// La police embarquee (Inter) a une taille EXACTE, chargee une fois par taille
			/// (au dixieme de pixel), declaree au rasteriseur. Sans repli externe
			/// (CJK / emoji) : dit dans le document 14.
			nkentseu::nkgui::NkGuiFont *Police(nkentseu::float32 taille) {
				if (taille < 1.f)
					taille = 1.f;
				if (taille > 512.f)
					taille = 512.f;
				taille = (nkentseu::float32)((nkentseu::int32)(taille * 10.f + 0.5f)) * 0.1f;
				for (nkentseu::int32 i = 0; i < mNb; ++i)
					if (mPolices[i].taille == taille)
						return mPolices[i].f;
				if (mNb >= kMaxPolices)
					return mNb > 0 ? mPolices[mNb - 1].f : nullptr;
				nkentseu::nkgui::NkGuiFont *f = nkentseu::memory::NkGetDefaultAllocator().New<nkentseu::nkgui::NkGuiFont>();
				if (!f)
					return nullptr;
				if (!f->LoadEmbedded(nkentseu::NkEmbeddedFontId::Inter, taille, false)) {
					nkentseu::memory::NkGetDefaultAllocator().Delete(f);
					return nullptr;
				}
				f->texId = 0x4E4B4600u + (nkentseu::uint32)mNb; // 'NKF' + rang : un identifiant par police
				if (mRaster)
					mRaster->PoserTexture(f->texId, f->pixels, f->atlasW, f->atlasH, 1);
				mPolices[mNb].taille = taille;
				mPolices[mNb].f = f;
				++mNb;
				if (taille > policeMax)
					policeMax = taille;
				return f;
			}

			nkentseu::nkgui::NkGuiContext &mCtx;
			nkentseu::nkgui::NkGuiDrawListRaster *mRaster;
			bool mExacte;
			Police_ mPolices[kMaxPolices];
			nkentseu::int32 mNb = 0;
	};

	// ── LES IMAGES DU DOCUMENT, VUES PAR LE RASTERISEUR ─────────────────────
	// Le fournisseur courant (celui de l'application ou de la sonde) est enveloppe :
	// chaque image trouvee est declaree au rasteriseur sous son handle GPU si elle
	// en a un, sinon sous un handle synthetique (sans fenetre, rien n'est televerse).
	// Le peintre ne voit qu'un handle non nul et emet son polygone texture.
	struct NkExportImages {
			renderdetail::NkFournisseurImages avant;
			nkentseu::nkgui::NkGuiDrawListRaster *raster = nullptr;
			nkentseu::uint32 prochain = 0x45580001u; ///< 'EX' + rang
			nkentseu::uint32 declarees = 0;
			static bool Obtenir(void *user, const char *chemin, renderdetail::NkImageSource &out) {
				NkExportImages &ex = *static_cast<NkExportImages *>(user);
				renderdetail::NkImageSource src;
				if (!ex.avant.obtenir || !ex.avant.obtenir(ex.avant.user, chemin, src))
					return false;
				if (!src.pixels || src.w <= 0 || src.h <= 0)
					return false;
				const nkentseu::uint32 h = src.handle ? src.handle : ex.prochain++;
				if (ex.raster)
					ex.raster->PoserTexture(h, src.pixels, src.w, src.h, 4);
				++ex.declarees;
				out = src;
				out.handle = h;
				return true;
			}
	};

	// ── LA ZONE A EXPORTER ───────────────────────────────────────────────────
	/// La page d'un noeud : son ancetre direct sous la racine (lui-meme s'il y est).
	inline nkentseu::int32 NkPageDe(const NkUIDocument &doc, nkentseu::int32 n) {
		if (!doc.IsValidIndex(n) || n == 0)
			return -1;
		while (doc.nodes[(nkentseu::uint32)n].parent > 0)
			n = doc.nodes[(nkentseu::uint32)n].parent;
		return n;
	}
	/// La page par defaut : celle de la selection, sinon la premiere sous la racine.
	// ── ② (07/09) CE QUE LE CANVAS CONTIENT D'EXPORTABLE ────────────────────
	//
	// Rodolf : « rien n'est selectionne mais le panneau d'export s'ouvre. Ce n'est
	// pas normal -- sauf si ca liste tous les elements exportables du canvas
	// infini. »
	//
	// 🔴 CE QUI COMPTE COMME EXPORTABLE, ET LA DEFINITION EST MESURABLE, PAS
	//    ESTHETIQUE. Un element du canvas est exportable s'il remplit LES TROIS :
	//      1. c'est un ENFANT DIRECT DE LA RACINE -- une page, ou une forme posee
	//         sur le canvas. Descendre plus bas listerait les enfants de chaque
	//         page, c'est-a-dire le document entier : l'utilisateur veut choisir
	//         entre ses PAGES, pas entre ses deux cents boutons ;
	//      2. la disposition lui donne une BOITE NON VIDE -- exporter un objet de
	//         zero pixel produit un fichier vide, et un fichier vide est un echec
	//         qui a l'air d'une reussite ;
	//      3. il n'est PAS MASQUE -- le peintre s'arrete avant lui (`NkDrawDocument`
	//         rend la main sur `masque`), donc son image serait vide elle aussi.
	//
	// ⚠️ ELLE EST ICI, PAS DANS LE DIALOGUE. Le dialogue AFFICHE un compte ; c'est
	//    l'export qui sait ce qu'il sait exporter. Ecrite la-bas, elle aurait
	//    diverge de ce que `NkZoneExport` accepte reellement -- et le panneau
	//    aurait annonce des elements que l'export aurait refuses.
	inline nkentseu::uint32 NkElementsExportables(const DesignState &st, const NkLayoutResult &lay,
												  NkVector<nkentseu::int32> *out) {
		using nkentseu::int32;
		using nkentseu::uint32;
		if (out)
			out->Clear();
		if (st.doc.nodes.Empty())
			return 0u;
		const NkVector<int32> &racines = st.doc.nodes[0].children;
		uint32 n = 0u;
		for (uint32 i = 0; i < (uint32)racines.Size(); ++i) {
			const int32 k = racines[i];
			if (!st.doc.IsValidIndex(k) || k <= 0 || !lay.Has(k))
				continue;
			if (st.doc.nodes[(uint32)k].masque)
				continue;
			const NkPaintRect b = lay.At(k);
			if (b.w <= 0.f || b.h <= 0.f)
				continue;
			if (out)
				out->PushBack(k);
			++n;
		}
		return n;
	}

	inline nkentseu::int32 NkPageParDefaut(const DesignState &st) {
		const nkentseu::int32 p = NkPageDe(st.doc, st.selected);
		if (p > 0)
			return p;
		if (!st.doc.nodes.Empty() && !st.doc.nodes[0].children.Empty())
			return st.doc.nodes[0].children[0];
		return -1;
	}
	/// La marge que les effets et une bordure exterieure ajoutent autour d'un noeud.
	inline nkentseu::float32 NkMargeAutour(const NkUINode &n) {
		nkentseu::float32 m = 0.f;
		for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)n.effets.Size(); ++i) {
			const NkEffet &e = n.effets[i];
			if (!e.visible || e.type != NkEffetType::OmbrePortee)
				continue;
			const nkentseu::float32 ax = e.x < 0.f ? -e.x : e.x, ay = e.y < 0.f ? -e.y : e.y;
			const nkentseu::float32 v = (ax > ay ? ax : ay) + e.flou + e.etendue;
			if (v > m)
				m = v;
		}
		for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)n.borders.Size(); ++i) {
			const NkBordure &b = n.borders[i];
			if (!b.visible || b.couleur.Empty())
				continue;
			const nkentseu::float32 e = b.position == NkBordurePos::Exterieur ? b.epaisseur
										: b.position == NkBordurePos::Centre ? b.epaisseur * 0.5f
																			  : 0.f;
			if (e > m)
				m = e;
		}
		return m;
	}
	/// Les noeuds racines a dessiner et la zone document qui les contient.
	/// Selection : les noeuds selectionnes, sans ceux dont un ancetre l'est deja
	/// (NkDrawDocument descend dans les enfants) ; la zone = l'union de leurs boites
	/// TRANSFORMEES, plus la marge des effets. Page : la page, sa boite.
	inline bool NkZoneExport(const DesignState &st, const NkLayoutResult &lay, const NkExportOptions &o,
							 NkVector<nkentseu::int32> &noeuds, NkPaintRect &zone, char *pourquoi,
							 nkentseu::usize cap) {
		using nkentseu::float32;
		noeuds.Clear();
		// ② (07/09) TOUT LE CANVAS -- teste AVANT `selection`, parce qu'il n'est pose
		//    que lorsqu'il n'y a rien de selectionne. La zone est l'union des boites,
		//    marge des effets comprise, comme pour une selection multiple.
		// ⚠️ LA LISTE VIENT DE `NkElementsExportables`, la MEME que le panneau
		//    compte : deux definitions de « exportable » auraient laisse le panneau
		//    annoncer des elements que l'export aurait refuses.
		if (o.tout) {
			NkElementsExportables(st, lay, &noeuds);
			if (noeuds.Empty()) {
				if (pourquoi && cap)
					snprintf(pourquoi, cap, "le canvas ne contient aucun Ã©lÃ©ment exportable");
				return false;
			}
			bool premier = true;
			for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)noeuds.Size(); ++i) {
				const nkentseu::int32 k = noeuds[i];
				const NkPaintRect b = lay.At(k);
				const float32 m = NkMargeAutour(st.doc.nodes[(nkentseu::uint32)k]);
				const float32 x0 = b.x - m, y0 = b.y - m, x1 = b.x + b.w + m, y1 = b.y + b.h + m;
				if (premier) {
					zone.x = x0; zone.y = y0; zone.w = x1 - x0; zone.h = y1 - y0;
					premier = false;
				} else {
					const float32 zx1 = zone.x + zone.w, zy1 = zone.y + zone.h;
					if (x0 < zone.x) zone.x = x0;
					if (y0 < zone.y) zone.y = y0;
					zone.w = (x1 > zx1 ? x1 : zx1) - zone.x;
					zone.h = (y1 > zy1 ? y1 : zy1) - zone.y;
				}
			}
			return true;
		}
		if (o.selection) {
			NkVector<nkentseu::int32> brut;
			if (!st.sel.Empty()) {
				for (nkentseu::uint32 i = 0; i < st.sel.Count(); ++i)
					brut.PushBack(st.sel.items[i]);
			} else if (st.doc.IsValidIndex(st.selected) && st.selected > 0)
				brut.PushBack(st.selected);
			for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)brut.Size(); ++i) {
				const nkentseu::int32 n = brut[i];
				if (!st.doc.IsValidIndex(n) || n <= 0 || !lay.Has(n))
					continue;
				bool couvert = false;
				for (nkentseu::int32 p = st.doc.nodes[(nkentseu::uint32)n].parent; p > 0 && !couvert;
					 p = st.doc.nodes[(nkentseu::uint32)p].parent)
					for (nkentseu::uint32 j = 0; j < (nkentseu::uint32)brut.Size(); ++j)
						if (brut[j] == p)
							couvert = true;
				if (!couvert)
					noeuds.PushBack(n);
			}
			if (noeuds.Empty()) {
				snprintf(pourquoi, cap, "rien n'est sélectionné");
				return false;
			}
			float32 x0 = 1.0e9f, y0 = 1.0e9f, x1 = -1.0e9f, y1 = -1.0e9f;
			for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)noeuds.Size(); ++i) {
				const nkentseu::int32 n = noeuds[i];
				const NkPaintRect r = lay.At(n);
				const float32 mg = NkMargeAutour(st.doc.nodes[(nkentseu::uint32)n]);
				const NkMat2D m = NkMatEffective(st.doc, lay, n);
				const float32 cs[8] = {r.x - mg, r.y - mg, r.x + r.w + mg, r.y - mg,
									   r.x + r.w + mg, r.y + r.h + mg, r.x - mg, r.y + r.h + mg};
				for (nkentseu::int32 k = 0; k < 4; ++k) {
					float32 px = cs[k * 2], py = cs[k * 2 + 1];
					NkMatPoint(m, px, py);
					if (px < x0) x0 = px;
					if (px > x1) x1 = px;
					if (py < y0) y0 = py;
					if (py > y1) y1 = py;
				}
			}
			zone = {x0, y0, x1 - x0, y1 - y0};
		} else {
			const nkentseu::int32 page = o.page > 0 && st.doc.IsValidIndex(o.page) ? o.page : NkPageParDefaut(st);
			if (page <= 0 || !lay.Has(page)) {
				snprintf(pourquoi, cap, "aucune page à exporter");
				return false;
			}
			noeuds.PushBack(page);
			zone = lay.At(page);
		}
		if (zone.w <= 0.f || zone.h <= 0.f) {
			snprintf(pourquoi, cap, "zone vide (%.0f × %.0f)", (double)zone.w, (double)zone.h);
			return false;
		}
		return true;
	}

	// ── L'IMAGE ──────────────────────────────────────────────────────────────
	/// Dessine la zone dans `out` (RGBA8). Sans fenetre ni GPU.
	inline bool NkExporterImage(DesignState &st, const NkExportOptions &o, nkentseu::NkImage &out,
								NkExportResultat &res) {
		using namespace nkentseu;
		res = NkExportResultat();
		float32 s = o.echelle;
		if (s < 0.25f) s = 0.25f;
		if (s > 8.f) s = 8.f;
		// la disposition : celle de l'application quand elle est a jour, sinon la
		// meme surface que main.cpp (la position d'une page est un resultat)
		NkLayoutResult layLocal;
		const NkLayoutResult *lay = &st.layout;
		{
			const int32 page = o.selection ? -1 : (o.page > 0 ? o.page : NkPageParDefaut(st));
			const bool aJour = st.layout.valid.Size() == st.doc.nodes.Size()
							   && (o.selection ? !st.sel.Empty() || st.selected > 0 : page > 0 && st.layout.Has(page));
			if (!aJour) {
				NkComputeLayout(st.doc, NkPaintRect{0.f, 0.f, 1400.f, 900.f}, layLocal);
				lay = &layLocal;
			}
		}
		NkVector<int32> noeuds;
		char pourquoi[160];
		pourquoi[0] = 0;
		if (!NkZoneExport(st, *lay, o, noeuds, res.zone, pourquoi, sizeof(pourquoi))) {
			snprintf(res.message, sizeof(res.message), "ÉCHEC d'export : %s — rien n'a été écrit.", pourquoi);
			return false;
		}
		const int32 W = (int32)std::ceil(res.zone.w * s - 0.001f), H = (int32)std::ceil(res.zone.h * s - 0.001f);
		nkgui::NkGuiDrawListRaster raster;
		if (!raster.Init(W, H)) {
			snprintf(res.message, sizeof(res.message),
					 "ÉCHEC d'export : %d × %d pixels dépasse ce que le rastériseur accepte (16384 de côté) — rien n'a été écrit.",
					 W, H);
			return false;
		}
		static nkgui::NkGuiContext ctx;
		ctx.Init(W, H);
		ctx.dl.Reset();
		ctx.scale = 1.f;
		ctx.font = nullptr;
		NkExportPaint paint(ctx, st.theme, &raster, o.policeExacte);
		// les images : le fournisseur courant, enveloppe pour le rasteriseur
		NkExportImages images;
		images.avant = renderdetail::NkFournisseurCourant();
		images.raster = &raster;
		renderdetail::NkPoserFournisseurImages(&NkExportImages::Obtenir, &images);
		renderdetail::NkPoserResolveur(&st.doc);
		NkDocumentHost host;
		host.SyncTo(st.doc);
		host.docScale = 1.f;
		const NkComponentInput idle;
		editorkit::NkPaintTransform T;
		T.a = s;
		T.d = s;
		T.e = -res.zone.x * s;
		T.f = -res.zone.y * s;
		paint.PushTransform(T);
		for (uint32 i = 0; i < (uint32)noeuds.Size(); ++i)
			NkDrawDocument(paint, idle, st.doc, *lay, host, noeuds[i]);
		paint.PopTransform();
		res.texturesInconnues = raster.Rasteriser(ctx.dl);
		renderdetail::NkFournisseurCourant() = images.avant; // le fournisseur d'avant, tel quel
		res.images = images.declarees;
		res.polices = paint.Polices();
		res.nbTailles = res.polices < 24u ? res.polices : 24u;
		for (uint32 i = 0; i < res.nbTailles; ++i)
			res.tailles[i] = paint.Taille(i);
		res.policeMax = paint.policeMax;
		res.noeuds = (uint32)noeuds.Size();
		res.largeur = W;
		res.hauteur = H;
		if (!out.Create((uint32)W, (uint32)H, math::NkColor(), 4) || !out.Pixels()) {
			snprintf(res.message, sizeof(res.message), "ÉCHEC d'export : l'image %d × %d n'a pas pu être allouée.", W, H);
			return false;
		}
		const uint8 *src = raster.Pixels();
		uint8 *dst = out.Pixels();
		const usize n = (usize)W * (usize)H * 4u;
		for (usize i = 0; i < n; ++i)
			dst[i] = src[i];
		res.ok = true;
		return true;
	}

	/// L'image, ecrite en PNG. Un echec est DIT (jamais un fichier vide en silence).
	inline bool NkExporterPNG(DesignState &st, const NkExportOptions &o, const char *chemin, NkExportResultat &res) {
		using namespace nkentseu;
		NkImage img;
		if (!NkExporterImage(st, o, img, res))
			return false;
		if (!chemin || !*chemin) {
			snprintf(res.message, sizeof(res.message), "ÉCHEC d'export : aucun chemin de destination.");
			res.ok = false;
			return false;
		}
		if (!img.SavePNG(chemin) || !NkFile::Exists(chemin) || NkFile::GetFileSize(chemin) <= 0) {
			snprintf(res.message, sizeof(res.message), "ÉCHEC d'export : le PNG n'a pas pu être écrit dans %s.", chemin);
			res.ok = false;
			return false;
		}
		snprintf(res.message, sizeof(res.message), "Exporté : %s (%d × %d px, ×%.2g, %s%s)", chemin, res.largeur,
				 res.hauteur, (double)o.echelle, o.selection ? "sélection" : "page",
				 res.texturesInconnues ? ", ⚠ textures manquantes" : "");
		return true;
	}

	// ── LE NOM DU FICHIER, TIRE DE L'OBJET (④, 05/09) ───────────────────────
	// Rodolf : « le nom du fichier exporte doit etre celui de sa page » -- et, comme tout
	// s'exporte desormais, celui de l'objet en general (page, groupe, graphique).
	/// Un nom de fichier sur : les caracteres interdits de Windows (`\ / : * ? " < > |`), les
	/// caracteres de controle et les espaces de bord deviennent `_`. Les ACCENTS RESTENT (NTFS
	/// et ext4 les acceptent ; les retirer demanderait de decomposer l'UTF-8, et le nom que
	/// Rodolf lit dans l'arbre doit rester reconnaissable dans son dossier). Vide -> `sans_nom`.
	inline void NkNomFichierAssaini(const char *nom, char *out, nkentseu::usize cap) {
		if (!out || cap == 0)
			return;
		nkentseu::usize k = 0;
		bool vu = false;
		for (const char *p = nom ? nom : ""; *p && k + 1 < cap; ++p) {
			const unsigned char c = (unsigned char)*p;
			char q = *p;
			if (c < 32u || c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<'
				|| c == '>' || c == '|')
				q = '_';
			if (q == ' ' && !vu)
				continue; // pas d'espace de tete
			out[k++] = q;
			if (q != ' ' && q != '_')
				vu = true;
		}
		while (k > 0 && (out[k - 1] == ' ' || out[k - 1] == '.'))
			--k; // ni espace ni point final (Windows les refuse)
		out[k] = '\0';
		if (!vu || k == 0)
			snprintf(out, cap, "sans_nom");
	}

	/// LE NOM DE CE QUI SERA EXPORTE : la page, l'objet, ou « N objets » pour une selection
	/// multiple exportee en UNE image. Sans etiquette, le genre sert de nom (« rect ») --
	/// jamais un numero de noeud, qui ne veut rien dire dans un dossier.
	inline void NkNomObjetExport(const DesignState &st, const NkExportOptions &o, char *out,
								 nkentseu::usize cap) {
		using namespace nkentseu;
		auto nomDe = [&](int32 i) -> const char * {
			if (!st.doc.IsValidIndex(i))
				return "document";
			const NkUINode &n = st.doc.nodes[(uint32)i];
			if (!n.label.Empty())
				return n.label.Data();
			if (!n.shape.Empty())
				return n.shape.Data();
			if (!n.component.Empty())
				return n.component.Data();
			return "objet";
		};
		// ① (07/09) LE NOM SUIT L'ETENDUE. Il ne la suivait pas : sur « tout le
		//    canvas » il proposait le nom de LA PREMIERE PAGE (« Connexion » sur la
		//    capture de Rodolf), donc un fichier nomme d'apres une page pour un
		//    export de trois artboards.
		// 🔴 C'EST LE DEFAUT D'HIER DEPLACE D'UN CRAN : l'etendue est devenue juste,
		//    ce qui la DECRIT ne l'etait pas. Quand on change ce qu'une chose fait,
		//    tout ce qui la nomme doit bouger dans le meme lot -- sinon le mensonge
		//    change simplement de place.
		if (o.tout) {
			NkNomFichierAssaini("canvas", out, cap);
			return;
		}
		if (!o.selection) {
			const int32 page = o.page > 0 && st.doc.IsValidIndex(o.page) ? o.page : NkPageParDefaut(st);
			NkNomFichierAssaini(page > 0 ? nomDe(page) : "document", out, cap);
			return;
		}
		uint32 n = st.sel.Count();
		if (n == 0u && st.doc.IsValidIndex(st.selected) && st.selected > 0)
			n = 1u;
		if (n <= 1u) {
			const int32 i = st.sel.Count() > 0u ? st.sel.items[0] : st.selected;
			NkNomFichierAssaini(nomDe(i), out, cap);
			return;
		}
		char b[64];
		snprintf(b, sizeof(b), "%u objets", n);
		NkNomFichierAssaini(b, out, cap);
	}

	// ── LE NOM PROPOSE ───────────────────────────────────────────────────────
	/// ④ LE NOM PROPOSE EST CELUI DE L'OBJET (05/09) -- avant, c'etait le nom du DOCUMENT
	/// suivi de « _page » ou « _selection » : trois pages exportees donnaient trois fois le
	/// meme nom, et Rodolf devait le retaper a chaque fois.
	inline void NkNomExportPropose(const DesignState &st, const NkExportOptions &o, char *out, nkentseu::usize cap) {
		using namespace nkentseu;
		char base[160];
		NkNomObjetExport(st, o, base, sizeof(base));
		char ech[16];
		ech[0] = 0;
		if (o.format == NkExportFormat::PNG && (o.echelle > 1.001f || o.echelle < 0.999f))
			snprintf(ech, sizeof(ech), "@%gx", (double)o.echelle);
		snprintf(out, cap, "%s%s.%s", base, ech, o.format == NkExportFormat::PNG ? "png" : "svg");
	}

} // namespace nkuidesign
