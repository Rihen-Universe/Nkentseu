#pragma once
// -----------------------------------------------------------------------------
// @File    Engine/NKEditorKit/src/NKEditorKit/NkAiPanneauImage.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   L'IMAGE DU PANNEAU, RENDUE PAR L'APPLICATION ELLE-MEME : la liste
//          d'affichage de l'image courante, rasterisee sans GPU, decoupee au
//          rectangle du panneau, ecrite en PNG.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI CE FICHIER EXISTE (21/09)
//   « Les bancs de rectangles etaient verts pendant que Rodolf voyait autre
//   chose. » La preuve exigee n'est plus un compte de pieces : c'est une IMAGE
//   du panneau, rendue par chaque application, mise a cote de la capture cible.
//
//   ⚠️ JAMAIS UNE CAPTURE DE L'ECRAN DE RODOLF. Ce qui est rasterise ici est la
//      liste que l'application vient de remplir pour son GPU -- les memes
//      sommets, les memes textures de glyphes, dans le meme ordre. Le
//      rasteriseur (`NkGuiDrawListRaster`, NKGui) applique les facteurs de
//      melange du dorsal OpenGL : sur un fond opaque, il rend ce que le GPU
//      rend.
//
//   ⚠️ CE QU'IL NE REND PAS, ET QUE L'IMAGE DIT : une texture qu'on ne lui a
//      pas donnee (la vue 3D, une icone chargee sur le GPU seulement) se peint
//      dans la couleur de son sommet. Le NOMBRE de commandes concernees est
//      rendu : zero sur le panneau veut dire que tout ce qui s'y voit est
//      rendu fidelement.
// -----------------------------------------------------------------------------

#include "NKGui/Core/NkGuiDrawListRaster.h"
#include "NKGui/Core/NkGuiFont.h"
#include "NKImage/NKImage.h"
#include "NKContainers/String/NkString.h"
#include <cstdio>

namespace nkentseu {
	namespace editorkit {

		struct NkAiImageResultat {
				bool ok = false;
				uint32 texturesInconnues = 0;
				int32 largeur = 0, hauteur = 0;
				char message[256] = {0};
		};

		/// Rasterise `dl` (toute la fenetre, W x H), garde la zone
		/// (zx, zy, zw, zh), et l'ecrit en PNG. `polices` : les atlas de glyphes a
		/// declarer (ceux que l'application a televerses).
		/// PLUSIEURS listes, peintes dans l'ordre (la couche principale PUIS celle
		/// des surcouches) : c'est l'ordre du dorsal, donc celui de l'ecran.
		inline NkAiImageResultat NkAiEcrireImageListes(const nkgui::NkGuiDrawList *const *listes, int32 nListes,
													   int32 W, int32 H, float32 zx, float32 zy, float32 zw,
													   float32 zh, const nkgui::NkGuiFont *const *polices,
													   int32 nPolices, uint32 fond, const char *chemin) {
			NkAiImageResultat res;
			nkgui::NkGuiDrawListRaster ras;
			if (!ras.Init(W, H)) {
				snprintf(res.message, sizeof(res.message), "cible %d x %d refusee par le rasteriseur", (int)W, (int)H);
				return res;
			}
			ras.Effacer(fond);
			for (int32 i = 0; i < nPolices; ++i) {
				const nkgui::NkGuiFont *f = polices ? polices[i] : nullptr;
				if (f && f->Valid() && f->pixels)
					ras.PoserTexture(f->TexId(), f->pixels, f->atlasW, f->atlasH, 1);
			}
			for (int32 l = 0; l < nListes; ++l)
				if (listes && listes[l])
					res.texturesInconnues += ras.Rasteriser(*listes[l]);
			int32 x0 = (int32)zx, y0 = (int32)zy, x1 = (int32)(zx + zw), y1 = (int32)(zy + zh);
			if (x0 < 0)
				x0 = 0;
			if (y0 < 0)
				y0 = 0;
			if (x1 > W)
				x1 = W;
			if (y1 > H)
				y1 = H;
			if (x1 <= x0 || y1 <= y0) {
				snprintf(res.message, sizeof(res.message), "zone vide (%d,%d)-(%d,%d)", (int)x0, (int)y0, (int)x1, (int)y1);
				return res;
			}
			NkImage img;
			const int32 w = x1 - x0, h = y1 - y0;
			if (!img.Create((uint32)w, (uint32)h, math::NkColor(), 4) || !img.Pixels()) {
				snprintf(res.message, sizeof(res.message), "image %d x %d non allouee", (int)w, (int)h);
				return res;
			}
			const uint8 *src = ras.Pixels();
			uint8 *dst = img.Pixels();
			for (int32 y = 0; y < h; ++y)
				for (int32 x = 0; x < w; ++x) {
					const usize s = ((usize)(y + y0) * (usize)W + (usize)(x + x0)) * 4u;
					const usize d = ((usize)y * (usize)w + (usize)x) * 4u;
					dst[d + 0] = src[s + 0];
					dst[d + 1] = src[s + 1];
					dst[d + 2] = src[s + 2];
					dst[d + 3] = 255u; // l'image se lit a cote d'une capture : opaque
				}
			if (!chemin || !chemin[0] || !img.SavePNG(chemin)) {
				snprintf(res.message, sizeof(res.message), "PNG non ecrit dans %s", chemin ? chemin : "(aucun chemin)");
				return res;
			}
			res.ok = true;
			res.largeur = w;
			res.hauteur = h;
			snprintf(res.message, sizeof(res.message), "%s : %d x %d, %u texture(s) inconnue(s) dans toute la fenetre",
					 chemin, (int)w, (int)h, (unsigned)res.texturesInconnues);
			return res;
		}

		inline NkAiImageResultat NkAiEcrireImage(const nkgui::NkGuiDrawList &dl, int32 W, int32 H, float32 zx,
												 float32 zy, float32 zw, float32 zh,
												 const nkgui::NkGuiFont *const *polices, int32 nPolices,
												 uint32 fond, const char *chemin) {
			const nkgui::NkGuiDrawList *l[1] = {&dl};
			return NkAiEcrireImageListes(l, 1, W, H, zx, zy, zw, zh, polices, nPolices, fond, chemin);
		}

	} // namespace editorkit
} // namespace nkentseu
