// =============================================================================
// NkGuiIcons.cpp — jeu d'icones : le mecanisme, jamais le vocabulaire.
// Voir l'en-tete pour le POURQUOI (193 glyphes definis deux fois) et pour la
// regle : NKGui ne connait aucun nom d'icone, et n'en connaitra jamais.
// =============================================================================
#include "NKGui/Core/NkGuiIcons.h"
#include "NKGui/Core/NkGuiDrawList.h"

namespace nkentseu {
	namespace nkgui {

		namespace {
			// Comparaison / copie de noms sans <cstring> : le depot est zero-STL,
			// et la Bare n'aura pas de libc.
			bool NameEq(const char *a, const char *b) noexcept {
				if (!a || !b)
					return false;
				while (*a && *b) {
					if (*a != *b)
						return false;
					++a;
					++b;
				}
				return *a == *b;
			}

			// Copie bornee, toujours terminee. Rend false si le nom est vide ou
			// s'il a fallu tronquer — un nom tronque se resoudrait mal plus tard,
			// mieux vaut le refuser tout de suite que livrer une poignee piegee.
			bool NameCopy(char *dst, const char *src) noexcept {
				if (!src || !*src)
					return false;
				int32 i = 0;
				for (; src[i] && i < NkGuiIconNameMax - 1; ++i)
					dst[i] = src[i];
				dst[i] = '\0';
				return src[i] == '\0';
			}

			// Poignee = (identifiant de jeu << 16) | (indice + 1). L'indice tient
			// sur 16 bits : 65 535 glyphes par jeu, contre 193 aujourd'hui pour
			// tout le depot.
			constexpr uint32 kIndexMask = 0xFFFFu;

			uint32 PackHandle(uint32 setId, int32 index) noexcept {
				return (setId << 16) | (static_cast<uint32>(index) + 1u);
			}

			// Identifiants de jeu, distribues a chaque Reset. Commence a 1 : un
			// jeu jamais Reset a l'identifiant 0 et ne peut donc valider aucune
			// poignee, ce qui est le comportement voulu.
			uint32 gNextSetId = 1u;
		} // namespace

		void NkGuiIconSet::Reset(uint32 texId, int32 atlasW, int32 atlasH) noexcept {
			mGlyphs.Clear();
			mContours.Clear();
			mPoints.Clear();
			mFallback = NkGuiIconHandle{};
			mTexId = texId;
			mAtlasW = atlasW;
			mAtlasH = atlasH;
			mOpenPath = -1;
			// Identifiant NEUF : les poignees d'avant deviennent etrangeres et
			// seront rejetees, jamais reinterpretees sur les nouveaux glyphes.
			mSetId = gNextSetId++;
			if (gNextSetId > kIndexMask)
				gNextSetId = 1u;
		}

		NkGuiIconHandle NkGuiIconSet::AddBitmap(const char *name, int32 x, int32 y, int32 w, int32 h,
												uint32 texId) noexcept {
			if (mSetId == 0u || w <= 0 || h <= 0)
				return NkGuiIconHandle{};
			// Hors de l'atlas : on refuse plutot que d'echantillonner n'importe ou.
			// (Un atlas de taille nulle n'est pas verifiable — cas d'un jeu
			// purement vectoriel a qui l'on ajoute quand meme un bitmap.)
			if (mAtlasW > 0 && mAtlasH > 0)
				if (x < 0 || y < 0 || x + w > mAtlasW || y + h > mAtlasH)
					return NkGuiIconHandle{};

			NkGuiIconGlyph g;
			if (!NameCopy(g.name, name))
				return NkGuiIconHandle{};
			g.kind = NkGuiIconKind::Bitmap;
			g.x = x;
			g.y = y;
			g.w = w;
			g.h = h;
			g.texId = texId;

			// Nom deja present : on REMPLACE, la poignee ne bouge pas.
			for (uint32 i = 0; i < mGlyphs.Size(); ++i) {
				if (NameEq(mGlyphs[i].name, name)) {
					mGlyphs[i] = g;
					return NkGuiIconHandle{PackHandle(mSetId, static_cast<int32>(i))};
				}
			}
			mGlyphs.PushBack(g);
			mOpenPath = -1; // un bitmap ferme le glyphe vectoriel en cours
			return NkGuiIconHandle{PackHandle(mSetId, static_cast<int32>(mGlyphs.Size()) - 1)};
		}

		NkGuiIconHandle NkGuiIconSet::AddPath(const char *name) noexcept {
			if (mSetId == 0u)
				return NkGuiIconHandle{};
			NkGuiIconGlyph g;
			if (!NameCopy(g.name, name))
				return NkGuiIconHandle{};
			g.kind = NkGuiIconKind::Path;
			g.firstContour = static_cast<int32>(mContours.Size());
			g.contourCount = 0;

			for (uint32 i = 0; i < mGlyphs.Size(); ++i) {
				if (NameEq(mGlyphs[i].name, name)) {
					mGlyphs[i] = g; // remplacement : les anciens contours sont abandonnes
					mOpenPath = static_cast<int32>(i);
					return NkGuiIconHandle{PackHandle(mSetId, mOpenPath)};
				}
			}
			mGlyphs.PushBack(g);
			mOpenPath = static_cast<int32>(mGlyphs.Size()) - 1;
			return NkGuiIconHandle{PackHandle(mSetId, mOpenPath)};
		}

		bool NkGuiIconSet::AddContour(NkGuiIconHandle h, const NkVec2 *pts, int32 n, bool filled,
									  float32 thickness, bool closed) noexcept {
			if (!pts || n < 2 || mOpenPath < 0)
				return false;
			// Les contours d'un glyphe sont CONTIGUS : n'accepter que le dernier
			// glyphe vectoriel ouvert, plutot que de melanger deux dessins.
			if (h.v != PackHandle(mSetId, mOpenPath))
				return false;
			if (filled && n < 3)
				return false; // un remplissage demande un triangle au minimum

			NkGuiIconContour c;
			c.first = static_cast<int32>(mPoints.Size());
			c.count = n;
			c.filled = filled;
			c.closed = closed;
			c.thickness = thickness;
			for (int32 i = 0; i < n; ++i)
				mPoints.PushBack(pts[i]);
			mContours.PushBack(c);
			mGlyphs[static_cast<uint32>(mOpenPath)].contourCount++;
			return true;
		}

		NkGuiIconHandle NkGuiIconSet::Find(const char *name) const noexcept {
			if (!name || !*name || mSetId == 0u)
				return NkGuiIconHandle{};
			for (uint32 i = 0; i < mGlyphs.Size(); ++i)
				if (NameEq(mGlyphs[i].name, name))
					return NkGuiIconHandle{PackHandle(mSetId, static_cast<int32>(i))};
			return NkGuiIconHandle{};
		}

		void NkGuiIconSet::SetFallback(NkGuiIconHandle h) noexcept {
			// Un secours etranger au jeu serait pire que pas de secours.
			mFallback = Glyph(h) ? h : NkGuiIconHandle{};
		}

		const NkGuiIconGlyph *NkGuiIconSet::Glyph(NkGuiIconHandle h) const noexcept {
			if (h.v == 0u || mSetId == 0u)
				return nullptr;
			if ((h.v >> 16) != mSetId) // poignee d'un AUTRE jeu : rejetee
				return nullptr;
			const uint32 idx = (h.v & kIndexMask);
			if (idx == 0u || idx > mGlyphs.Size())
				return nullptr;
			return &mGlyphs[idx - 1u];
		}

		const NkGuiIconGlyph *NkGuiIconSet::GlyphAt(int32 index) const noexcept {
			if (index < 0 || static_cast<uint32>(index) >= mGlyphs.Size())
				return nullptr;
			return &mGlyphs[static_cast<uint32>(index)];
		}

		const NkGuiIconContour *NkGuiIconSet::ContourAt(int32 index) const noexcept {
			if (index < 0 || static_cast<uint32>(index) >= mContours.Size())
				return nullptr;
			return &mContours[static_cast<uint32>(index)];
		}

		// ── DESSIN ───────────────────────────────────────────────────────────

		namespace {
			// Trace un glyphe VECTORIEL : les contours vivent dans la boite unite,
			// on les mappe dans `r`. Aucun echantillonnage — la geometrie est
			// emise a la taille finale, donc nette a tout facteur DPI, et la
			// couleur vient entierement de `tint` (recoloration par jeton, sans
			// atlas par couleur).
			void DrawPath(NkGuiDrawList &dl, const NkGuiIconSet &set, const NkGuiIconGlyph &g, const NkRect &r,
						  const NkColor &tint) noexcept {
				const NkVec2 *src = set.Points();
				if (!src)
					return;
				// L'epaisseur suit la plus petite dimension : une icone etiree ne
				// donne pas un trait deux fois plus epais dans un sens.
				const float32 unit = (r.w < r.h ? r.w : r.h);
				NkVec2 buf[64];
				for (int32 k = 0; k < g.contourCount; ++k) {
					const NkGuiIconContour *c = set.ContourAt(g.firstContour + k);
					if (!c || c->count < 2)
						continue;
					int32 n = c->count;
					if (n > 64)
						n = 64; // borne du tampon de pile : au-dela, on tronque plutot
							    // que d'allouer par frame (le dessin reste lisible)
					for (int32 i = 0; i < n; ++i) {
						const NkVec2 &p = src[c->first + i];
						buf[i] = {r.x + p.x * r.w, r.y + p.y * r.h};
					}
					if (c->filled)
						dl.AddConvexPolyFilled(buf, n, tint);
					else
						dl.AddPolyline(buf, n, tint, c->thickness * unit, c->closed);
				}
			}
		} // namespace

		NkGuiIconDraw AddIcon(NkGuiDrawList &dl, const NkGuiIconSet &set, NkGuiIconHandle h, const NkRect &r,
							  const NkColor &tint) noexcept {
			if (r.w <= 0.f || r.h <= 0.f)
				return NkGuiIconDraw::None;

			const NkGuiIconGlyph *g = set.Glyph(h);
			NkGuiIconDraw result = NkGuiIconDraw::Glyph;
			if (!g) {
				// Poignee invalide OU etrangere au jeu. On ne devine pas : soit
				// l'application a declare un secours, soit on ne dessine RIEN.
				g = set.Glyph(set.Fallback());
				if (!g)
					return NkGuiIconDraw::None;
				result = NkGuiIconDraw::Fallback;
			}

			if (g->kind == NkGuiIconKind::Path) {
				if (g->contourCount <= 0)
					return NkGuiIconDraw::None; // glyphe ouvert mais jamais rempli
				DrawPath(dl, set, *g, r, tint);
				return result;
			}

			// Bitmap : UV exactes de la region declaree.
			const uint32 tex = g->texId ? g->texId : set.TexId();
			if (tex == 0u)
				return NkGuiIconDraw::None;
			// Normalisation par les dimensions de l'atlas, portees par le jeu.
			const NkVec2 uv0{static_cast<float32>(g->x), static_cast<float32>(g->y)};
			const NkVec2 uv1{static_cast<float32>(g->x + g->w), static_cast<float32>(g->y + g->h)};
			const float32 sw = set.AtlasW() > 0 ? static_cast<float32>(set.AtlasW()) : 1.f;
			const float32 sh = set.AtlasH() > 0 ? static_cast<float32>(set.AtlasH()) : 1.f;
			dl.AddImage(tex, r, {uv0.x / sw, uv0.y / sh}, {uv1.x / sw, uv1.y / sh}, tint);
			return result;
		}

	} // namespace nkgui
} // namespace nkentseu
