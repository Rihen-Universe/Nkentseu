#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiIcons.h
// @Brief   Jeu d'icones NKGui — le MECANISME, jamais le vocabulaire.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// ── POURQUOI CE FICHIER EXISTE ───────────────────────────────────────────────
// Mesure du 2026-08-18 : il n'existait AUCUN moyen de dessiner un pictogramme
// dans NKGui. Consequences payees deux fois :
//   - NK3DModeler tient 102 glyphes (`NkIcon`, une texture PAR icone) ;
//   - NKCode en tient 91 autres, de son cote ;
//   - `Nogee/Panels/AssetBrowser.cpp` dessine `AddRectFilled(iconRect, ...)` —
//     un rectangle colore LA OU LA PLANCHE MONTRE UN PICTOGRAMME.
// Soit 193 glyphes definis deux fois, et une application qui s'en passe. C'est
// le meme motif que MouDraw.h / NkoungDraw.h, une couche plus haut.
//
// ── LA REGLE QUI COMMANDE LA CONCEPTION ──────────────────────────────────────
// **NKGui ne connait AUCUN nom d'icone, et n'en connaitra jamais.** Pas
// d'`enum NkGuiIcon`, pas de « dossier », pas de « engrenage ». Le vocabulaire
// appartient au projet, et devra survivre a la declaration de NkUIDesign
// (qui portera aussi le ROLE et l'ARBRE DE SOUS-ELEMENTS, et qui DESSINERA les
// icones). Figer une enumeration ici, ce serait ecrire du code a defaire.
//
// Donc : l'application fournit son jeu ET sa table de noms ; la bibliotheque
// rend une **POIGNEE OPAQUE** dont la valeur n'a aucun sens en dehors du jeu qui
// l'a produite. On dessine par poignee, jamais par enumeration.
//
// ── DEUX SOURCES, UNE SEULE POIGNEE ──────────────────────────────────────────
// Un glyphe est de l'une ou l'autre nature, et l'appelant ne s'en occupe pas :
//
//   Bitmap  une decoupe d'un atlas deja rasterise. C'est ce qui EXISTE
//           aujourd'hui (NK3DModeler, NKCode) — le mecanisme doit l'accepter,
//           sinon rien ne migre.
//   Path    des CONTOURS vectoriels, exprimes dans une boite unite [0,1]^2 et
//           mis a l'echelle du rect demande au moment du dessin.
//
// **Le vectoriel est la source, la rasterisation une etape de SORTIE** (Rodolf,
// 2026-08-18). Un glyphe `Path` se recolore par jeton de theme sans qu'on
// livre un atlas par couleur, suit le facteur DPI **sans flou** (il n'y a aucun
// echantillonnage : on emet la geometrie a la taille finale), et **reste
// editable** dans NkUIDesign — c'est la demande. Les deux primitives qui le
// rendent possible existent deja : `AddConvexPolyFilled` et `AddPolyline`.
// -----------------------------------------------------------------------------

#include "NKGui/NkGuiExport.h"
#include "NKGui/Core/NkGuiTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace nkgui {

		struct NkGuiDrawList;

		/// Longueur maximale d'un nom de glyphe, copie DANS le jeu. Bornee pour
		/// ne dependre d'aucune duree de vie cote appelant : l'application peut
		/// construire ses noms sur la pile.
		static constexpr int32 NkGuiIconNameMax = 48;

		// ── POIGNEE OPAQUE ───────────────────────────────────────────────────
		// Sa valeur n'a AUCUN sens hors du jeu qui l'a rendue, et c'est
		// volontaire. Elle embarque l'identifiant du jeu : une poignee presentee
		// au mauvais jeu est REJETEE au lieu de dessiner silencieusement le
		// mauvais glyphe (le defaut le plus penible a diagnostiquer).
		struct NkGuiIconHandle {
				uint32 v = 0; ///< 0 = invalide

				bool Valid() const noexcept {
					return v != 0u;
				}
		};

		NKENTSEU_NKGUI_API_INLINE bool operator==(const NkGuiIconHandle &a, const NkGuiIconHandle &b) noexcept {
			return a.v == b.v;
		}
		NKENTSEU_NKGUI_API_INLINE bool operator!=(const NkGuiIconHandle &a, const NkGuiIconHandle &b) noexcept {
			return a.v != b.v;
		}

		/// Ce qu'un appel de dessin a REELLEMENT fait. Rendu explicite parce
		/// qu'un glyphe manquant doit se CONSTATER, pas se deviner : un carre
		/// muet a la place d'une icone est exactement le defaut qu'on corrige.
		enum class NkGuiIconDraw : uint8 {
			None = 0, ///< rien dessine (poignee invalide, etrangere, ou rect nul)
			Glyph,	  ///< le glyphe demande
			Fallback  ///< le glyphe de SECOURS — la demande n'a pas abouti, et on le dit
		};

		enum class NkGuiIconKind : uint8 {
			Bitmap = 0, ///< decoupe d'un atlas rasterise
			Path		///< contours vectoriels (boite unite), mis a l'echelle au dessin
		};

		/// Un contour d'un glyphe vectoriel. Les points vivent dans le tampon du
		/// jeu ; ceux-ci sont exprimes dans la **boite unite [0,1]^2**, y vers le
		/// bas — donc independants de la taille finale et du DPI.
		struct NkGuiIconContour {
				int32 first = 0; ///< 1er point dans le tampon du jeu
				int32 count = 0;
				bool filled = true;		 ///< true : rempli (convexe) ; false : trait
				bool closed = true;		 ///< trait : relie le dernier point au premier
				float32 thickness = 0.08f; ///< trait : epaisseur en UNITES DE BOITE
										   ///< (x min(w,h) au dessin -> suit l'echelle)
		};

		/// Un glyphe. `name` n'est lu que par `Find` ; passe ce point, tout est
		/// poignee.
		struct NkGuiIconGlyph {
				char name[NkGuiIconNameMax] = {};
				NkGuiIconKind kind = NkGuiIconKind::Bitmap;
				// ── Bitmap ──
				int32 x = 0, y = 0, w = 0, h = 0; ///< pixels dans l'atlas
				uint32 texId = 0;				  ///< 0 = la texture du jeu
				// ── Path ──
				int32 firstContour = 0, contourCount = 0;
		};

		// ── LE JEU D'ICONES ──────────────────────────────────────────────────
		// L'application le remplit au demarrage, resout ses poignees UNE fois,
		// puis ne manipule plus que des poignees.
		class NKENTSEU_NKGUI_CLASS_EXPORT NkGuiIconSet {
			public:
				/// Vide le jeu et lui attribue un identifiant NEUF : les poignees
				/// rendues avant deviennent etrangeres, donc rejetees — jamais
				/// reinterpretees. `texId`/`atlasW`/`atlasH` ne servent qu'aux
				/// glyphes `Bitmap` ; un jeu 100 % vectoriel passe `Reset(0,0,0)`.
				void Reset(uint32 texId = 0u, int32 atlasW = 0, int32 atlasH = 0) noexcept;

				// ── Source 1 : atlas rasterise (ce qui existe deja) ──────────
				/// Ajoute une decoupe et rend sa poignee. Poignee invalide si le nom
				/// est vide, la region degeneree, ou hors de l'atlas.
				/// `texId` = 0 : la region utilise la texture du jeu. Une texture
				/// par region est permise — c'est le modele « une texture par
				/// icone » de NK3DModeler, qui passe donc sans rien changer.
				/// Un nom deja present est REMPLACE, poignee conservee : recharger
				/// un jeu n'invalide pas ce que l'application tient.
				NkGuiIconHandle AddBitmap(const char *name, int32 x, int32 y, int32 w, int32 h,
										  uint32 texId = 0u) noexcept;

				// ── Source 2 : contours vectoriels (la source a terme) ───────
				/// Ouvre un glyphe vectoriel VIDE et rend sa poignee. Les contours
				/// s'ajoutent ensuite par `AddContour`, qui n'agit que sur le
				/// DERNIER glyphe vectoriel ouvert (les contours d'un glyphe sont
				/// contigus) — `AddContour` le refuse autrement plutot que de
				/// melanger deux dessins en silence.
				NkGuiIconHandle AddPath(const char *name) noexcept;
				/// Ajoute un contour au glyphe vectoriel ouvert. Points dans la
				/// BOITE UNITE [0,1]^2. `filled` : rempli (le contour doit etre
				/// convexe) ou trait d'epaisseur `thickness` (unites de boite).
				bool AddContour(NkGuiIconHandle h, const NkVec2 *pts, int32 n, bool filled = true,
								float32 thickness = 0.08f, bool closed = true) noexcept;

				/// Resout un nom. Poignee invalide si inconnu. C'est le SEUL
				/// endroit ou un nom de glyphe circule.
				NkGuiIconHandle Find(const char *name) const noexcept;

				/// Glyphe de secours, choisi par l'APPLICATION (elle seule sait
				/// lequel de ses dessins veut dire « manquant »). Non defini : une
				/// demande qui echoue ne dessine RIEN.
				void SetFallback(NkGuiIconHandle h) noexcept;
				NkGuiIconHandle Fallback() const noexcept {
					return mFallback;
				}

				int32 Count() const noexcept {
					return static_cast<int32>(mGlyphs.Size());
				}
				uint32 TexId() const noexcept {
					return mTexId;
				}
				/// Dimensions de l'atlas, en pixels — necessaires pour normaliser
				/// les UV d'un glyphe `Bitmap`. 0 pour un jeu purement vectoriel.
				int32 AtlasW() const noexcept {
					return mAtlasW;
				}
				int32 AtlasH() const noexcept {
					return mAtlasH;
				}
				/// Glyphe derriere une poignee, ou nullptr si la poignee est
				/// invalide ou ETRANGERE a ce jeu.
				const NkGuiIconGlyph *Glyph(NkGuiIconHandle h) const noexcept;
				/// Enumeration par indice — pour un inspecteur, un serialiseur ou
				/// le futur NkUIDesign, qui doit pouvoir LISTER et EDITER ce que
				/// l'application a declare.
				const NkGuiIconGlyph *GlyphAt(int32 index) const noexcept;
				const NkGuiIconContour *ContourAt(int32 index) const noexcept;
				const NkVec2 *Points() const noexcept {
					return mPoints.Size() ? &mPoints[0] : nullptr;
				}
				int32 PointCount() const noexcept {
					return static_cast<int32>(mPoints.Size());
				}

			private:
				NkVector<NkGuiIconGlyph> mGlyphs;
				NkVector<NkGuiIconContour> mContours;
				NkVector<NkVec2> mPoints;
				NkGuiIconHandle mFallback;
				uint32 mTexId = 0;
				uint32 mSetId = 0;	  ///< distingue deux jeux — voir NkGuiIconHandle
				int32 mOpenPath = -1; ///< indice du dernier glyphe vectoriel ouvert
				int32 mAtlasW = 0, mAtlasH = 0;
		};

		// ── DESSIN ───────────────────────────────────────────────────────────
		// `Bitmap` : emet un quad dont les UV echantillonnent EXACTEMENT la
		// region declaree. `Path` : emet la geometrie des contours mise a
		// l'echelle de `r` — aucun echantillonnage, donc net a tout facteur DPI.
		//
		// `tint` multiplie l'echantillon (Bitmap) ou colore les contours (Path) :
		// les glyphes se teintent par un JETON DE THEME. Aucune couleur en dur
		// ici — c'est l'appelant qui passe le jeton, volontairement : la
		// bibliotheque ne devine pas le role d'une icone.
		//
		// Le retour dit ce qui s'est passe. Ne pas l'ignorer : c'est lui qui
		// transforme « mon icone n'apparait pas » en fait constatable.
		NKENTSEU_NKGUI_API NkGuiIconDraw AddIcon(NkGuiDrawList &dl, const NkGuiIconSet &set, NkGuiIconHandle h,
												 const NkRect &r, const NkColor &tint) noexcept;

	} // namespace nkgui
} // namespace nkentseu
