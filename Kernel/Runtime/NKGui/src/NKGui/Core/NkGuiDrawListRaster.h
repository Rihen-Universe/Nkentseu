#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiDrawListRaster.h
// @Brief   Rasterisation LOGICIELLE d'une NkGuiDrawList vers des pixels RGBA8 --
//          le meme flux de commandes que le GPU, sans fenetre ni GPU.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI ICI, ET PAS DANS L'APPLICATION QUI EN A EU BESOIN
// =============================================================================
//  NkUIDesign exporte une page en PNG (2026-09-05). Ce qu'il exporte est ce que
//  la toile dessine : une NkGuiDrawList (sommets, indices, commandes). La liste
//  appartient a NKGui ; celui qui la rend en pixels sans GPU appartient donc a
//  NKGui aussi -- la couche du dessous, pas l'application (porte du 28/08).
//
//  MESURE AVANT D'ECRIRE : NKCanvas porte un dorsal Software
//  (`NkSoftwareRenderer2D::RasterizeTriangle`, privee) et une cible hors ecran
//  (`NkRenderTexture`), mais son contexte s'initialise SUR UNE FENETRE
//  (`NkSoftwareContext::Initialize(const NkWindow &, ...)`). Sans fenetre, il
//  n'y a donc pas de rasteriseur a reutiliser. Celui-ci est petit (un triangle
//  a la fois, fonctions d'arete) et ne pretend pas remplacer le dorsal : il
//  rend ce qu'une sonde et un export doivent pouvoir prouver sans ecran.
//
//  CE QU'IL FAIT EXACTEMENT :
//    - un triangle par triplet d'indices, test par fonctions d'arete au centre
//      du pixel, regle haut-gauche (une arete partagee n'est couverte qu'une
//      fois : un quad semi-transparent n'a pas de diagonale plus sombre) ;
//    - couleur et uv interpoles par barycentre ; textures echantillonnees en
//      BILINEAIRE, bord repete (le filtre NK_LINEAR que le dorsal pose sur les
//      polices et les images) ; une texture a 1 canal est un alpha sur blanc
//      (ce que `NkGuiCanvasBackend::UploadFontGray8` fabrique) ;
//    - la decoupe par commande (`clipRect`), comme le dorsal ;
//    - les six modes de melange de `NkGuiBlend` avec LES FACTEURS DU DORSAL
//      OpenGL (`NkOpenGLRenderer2D::SetBlendMode`) : Screen ONE /
//      ONE_MINUS_SRC_COLOR, Darken MIN, Lighten MAX, PlusLighter ONE / ONE,
//      Multiply DST_COLOR / ZERO. Le mode Alpha est le « dessus » en alpha
//      droit : sur une destination OPAQUE (une page) il est identique au GPU ;
//      sur une destination transparente (une selection exportee) il garde une
//      couleur juste la ou la formule du GPU assombrirait.
//
//  CE QU'IL NE FAIT PAS, ET C'EST DIT : aucun anticrenelage de ses propres
//  aretes (le dorsal GPU n'en fait pas non plus sur ces listes) ; pas de
//  mipmaps ; les textures sont REFERENCEES, pas copiees -- l'appelant les garde
//  vivantes jusqu'a `Rasteriser`.
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKGui/Core/NkGuiDrawList.h"

namespace nkentseu {
	namespace nkgui {

		/// Une texture connue du rasteriseur : RGBA8 (4 canaux) ou alpha8 (1 canal).
		struct NkGuiRasterTexture {
				uint32 id = 0;
				const uint8 *px = nullptr;
				int32 w = 0, h = 0;
				int32 canaux = 4;
		};

		class NkGuiDrawListRaster {
			public:
				/// Alloue la cible (transparente). Faux si la taille est nulle ou
				/// deraisonnable (plus de 16384 de cote).
				bool Init(int32 w, int32 h) noexcept {
					if (w <= 0 || h <= 0 || w > 16384 || h > 16384)
						return false;
					mW = w;
					mH = h;
					mPx.Resize((usize)w * (usize)h * 4u);
					Effacer(0u);
					return true;
				}

				/// Remplit toute la cible d'une couleur 0xRRGGBBAA.
				void Effacer(uint32 rgba) noexcept {
					const uint8 r = (uint8)((rgba >> 24) & 0xFFu), g = (uint8)((rgba >> 16) & 0xFFu);
					const uint8 b = (uint8)((rgba >> 8) & 0xFFu), a = (uint8)(rgba & 0xFFu);
					uint8 *d = mPx.Data();
					const usize n = (usize)mW * (usize)mH;
					for (usize i = 0; i < n; ++i) {
						d[i * 4u + 0u] = r;
						d[i * 4u + 1u] = g;
						d[i * 4u + 2u] = b;
						d[i * 4u + 3u] = a;
					}
				}

				/// Declare (ou remplace) une texture par son identifiant. Referencee.
				void PoserTexture(uint32 id, const uint8 *px, int32 w, int32 h, int32 canaux) noexcept {
					if (id == 0u || !px || w <= 0 || h <= 0 || (canaux != 1 && canaux != 4))
						return;
					for (uint32 i = 0; i < (uint32)mTex.Size(); ++i)
						if (mTex[i].id == id) {
							mTex[i].px = px;
							mTex[i].w = w;
							mTex[i].h = h;
							mTex[i].canaux = canaux;
							return;
						}
					NkGuiRasterTexture t;
					t.id = id;
					t.px = px;
					t.w = w;
					t.h = h;
					t.canaux = canaux;
					mTex.PushBack(t);
				}

				/// Rend toute la liste, commande par commande, dans l'ordre.
				/// Rend le nombre de commandes TEXTUREES dont la texture etait inconnue
				/// (elles sont peintes sans texture : couleur du sommet seule) -- un
				/// appelant qui obtient autre chose que zero a oublie de declarer une
				/// texture, et il le sait.
				uint32 Rasteriser(const NkGuiDrawList &dl) noexcept {
					uint32 inconnues = 0;
					for (uint32 ci = 0; ci < (uint32)dl.cmds.Size(); ++ci) {
						const NkGuiDrawCmd &c = dl.cmds[ci];
						if (c.idxCount < 3u)
							continue;
						// la decoupe : l'intersection du clip et de la cible, en pixels entiers
						int32 cx0 = 0, cy0 = 0, cx1 = mW, cy1 = mH;
						if (c.clipRect.w < 1.0e8f && c.clipRect.h < 1.0e8f) {
							const float32 x0 = c.clipRect.x, y0 = c.clipRect.y;
							const float32 x1 = c.clipRect.x + c.clipRect.w, y1 = c.clipRect.y + c.clipRect.h;
							if ((int32)x0 > cx0) cx0 = (int32)x0;
							if ((int32)y0 > cy0) cy0 = (int32)y0;
							if ((int32)x1 < cx1) cx1 = (int32)x1;
							if ((int32)y1 < cy1) cy1 = (int32)y1;
							if (cx1 <= cx0 || cy1 <= cy0)
								continue;
						}
						const NkGuiRasterTexture *tex = nullptr;
						if (c.type == NkGuiDrawCmdType::TexturedTriangles && c.texId != 0u) {
							tex = Texture(c.texId);
							if (!tex)
								++inconnues;
						}
						const uint32 fin = c.idxOffset + c.idxCount;
						for (uint32 k = c.idxOffset; k + 2u < fin && k + 2u < (uint32)dl.idx.Size(); k += 3u) {
							const uint32 ia = dl.idx[k], ib = dl.idx[k + 1u], ic = dl.idx[k + 2u];
							if (ia >= (uint32)dl.vtx.Size() || ib >= (uint32)dl.vtx.Size() || ic >= (uint32)dl.vtx.Size())
								continue;
							Triangle(dl.vtx[ia], dl.vtx[ib], dl.vtx[ic], tex, c.blend, cx0, cy0, cx1, cy1);
						}
					}
					return inconnues;
				}

				int32 Largeur() const noexcept { return mW; }
				int32 Hauteur() const noexcept { return mH; }
				const uint8 *Pixels() const noexcept { return mPx.Data(); }
				/// Un pixel, 0xRRGGBBAA ; 0 hors cible.
				uint32 Pixel(int32 x, int32 y) const noexcept {
					if (x < 0 || y < 0 || x >= mW || y >= mH)
						return 0u;
					const uint8 *p = mPx.Data() + ((usize)y * (usize)mW + (usize)x) * 4u;
					return ((uint32)p[0] << 24) | ((uint32)p[1] << 16) | ((uint32)p[2] << 8) | (uint32)p[3];
				}

			private:
				const NkGuiRasterTexture *Texture(uint32 id) const noexcept {
					for (uint32 i = 0; i < (uint32)mTex.Size(); ++i)
						if (mTex[i].id == id)
							return &mTex[i];
					return nullptr;
				}

				/// Echantillon bilineaire, bord repete, en 0..1 (RGBA). Un canal = alpha sur blanc.
				static void Echantillon(const NkGuiRasterTexture &t, float32 u, float32 v, float32 out[4]) noexcept {
					float32 fx = u * (float32)t.w - 0.5f, fy = v * (float32)t.h - 0.5f;
					int32 x0 = (int32)(fx >= 0.f ? fx : fx - 1.f), y0 = (int32)(fy >= 0.f ? fy : fy - 1.f);
					const float32 tx = fx - (float32)x0, ty = fy - (float32)y0;
					int32 x1 = x0 + 1, y1 = y0 + 1;
					if (x0 < 0) x0 = 0;
					if (y0 < 0) y0 = 0;
					if (x1 < 0) x1 = 0;
					if (y1 < 0) y1 = 0;
					if (x0 >= t.w) x0 = t.w - 1;
					if (x1 >= t.w) x1 = t.w - 1;
					if (y0 >= t.h) y0 = t.h - 1;
					if (y1 >= t.h) y1 = t.h - 1;
					float32 c00[4], c10[4], c01[4], c11[4];
					Texel(t, x0, y0, c00);
					Texel(t, x1, y0, c10);
					Texel(t, x0, y1, c01);
					Texel(t, x1, y1, c11);
					for (int32 k = 0; k < 4; ++k) {
						const float32 h0 = c00[k] + (c10[k] - c00[k]) * tx;
						const float32 h1 = c01[k] + (c11[k] - c01[k]) * tx;
						out[k] = h0 + (h1 - h0) * ty;
					}
				}
				static void Texel(const NkGuiRasterTexture &t, int32 x, int32 y, float32 out[4]) noexcept {
					if (t.canaux == 1) {
						const float32 a = (float32)t.px[(usize)y * (usize)t.w + (usize)x] / 255.f;
						out[0] = out[1] = out[2] = 1.f;
						out[3] = a;
						return;
					}
					const uint8 *p = t.px + ((usize)y * (usize)t.w + (usize)x) * 4u;
					out[0] = (float32)p[0] / 255.f;
					out[1] = (float32)p[1] / 255.f;
					out[2] = (float32)p[2] / 255.f;
					out[3] = (float32)p[3] / 255.f;
				}

				static void Depaqueter(uint32 col, float32 out[4]) noexcept {
					// NkGuiPackColor : a<<24 | b<<16 | g<<8 | r
					out[0] = (float32)(col & 0xFFu) / 255.f;
					out[1] = (float32)((col >> 8) & 0xFFu) / 255.f;
					out[2] = (float32)((col >> 16) & 0xFFu) / 255.f;
					out[3] = (float32)((col >> 24) & 0xFFu) / 255.f;
				}

				/// Le melange d'une source (0..1, alpha droit) dans un pixel de la cible.
				static void Melanger(uint8 *d, const float32 s[4], NkGuiBlend mode) noexcept {
					float32 dst[4] = {(float32)d[0] / 255.f, (float32)d[1] / 255.f, (float32)d[2] / 255.f,
									  (float32)d[3] / 255.f};
					float32 o[4];
					switch (mode) {
						case NkGuiBlend::Multiply: // GL_DST_COLOR / GL_ZERO, sur les quatre canaux
							for (int32 k = 0; k < 4; ++k)
								o[k] = s[k] * dst[k];
							break;
						case NkGuiBlend::Screen: // ONE / ONE_MINUS_SRC_COLOR ; alpha ONE / ONE_MINUS_SRC_ALPHA
							for (int32 k = 0; k < 3; ++k)
								o[k] = s[k] + dst[k] * (1.f - s[k]);
							o[3] = s[3] + dst[3] * (1.f - s[3]);
							break;
						case NkGuiBlend::Darken: // equation MIN (facteurs ignores), les quatre canaux
							for (int32 k = 0; k < 4; ++k)
								o[k] = s[k] < dst[k] ? s[k] : dst[k];
							break;
						case NkGuiBlend::Lighten: // equation MAX
							for (int32 k = 0; k < 4; ++k)
								o[k] = s[k] > dst[k] ? s[k] : dst[k];
							break;
						case NkGuiBlend::PlusLighter: // ONE / ONE ; alpha ONE / ONE_MINUS_SRC_ALPHA
							for (int32 k = 0; k < 3; ++k)
								o[k] = s[k] + dst[k];
							o[3] = s[3] + dst[3] * (1.f - s[3]);
							break;
						default: { // Alpha : le « dessus » en alpha droit (= GPU sur une destination opaque)
							const float32 sa = s[3], da = dst[3];
							const float32 oa = sa + da * (1.f - sa);
							if (oa <= 0.f) {
								o[0] = o[1] = o[2] = o[3] = 0.f;
								break;
							}
							for (int32 k = 0; k < 3; ++k)
								o[k] = (s[k] * sa + dst[k] * da * (1.f - sa)) / oa;
							o[3] = oa;
							break;
						}
					}
					for (int32 k = 0; k < 4; ++k) {
						float32 v = o[k];
						if (v < 0.f) v = 0.f;
						if (v > 1.f) v = 1.f;
						d[k] = (uint8)(v * 255.f + 0.5f);
					}
				}

				static bool HautGauche(const NkVec2 &p0, const NkVec2 &p1) noexcept {
					// pour un triangle d'aire POSITIVE (orient2d > 0, y vers le bas) :
					// l'arete « haut » va vers la droite a y constant, l'arete « gauche » monte
					const float32 dx = p1.x - p0.x, dy = p1.y - p0.y;
					return (dy < 0.f) || (dy == 0.f && dx > 0.f);
				}

				void Triangle(const NkGuiVertex &va, const NkGuiVertex &vb, const NkGuiVertex &vc,
							  const NkGuiRasterTexture *tex, NkGuiBlend mode, int32 cx0, int32 cy0, int32 cx1,
							  int32 cy1) noexcept {
					const NkGuiVertex *a = &va, *b = &vb, *c = &vc;
					auto orient = [](const NkVec2 &p, const NkVec2 &q, const NkVec2 &r) -> float64 {
						return (float64)(q.x - p.x) * (float64)(r.y - p.y) - (float64)(q.y - p.y) * (float64)(r.x - p.x);
					};
					float64 aire = orient(a->pos, b->pos, c->pos);
					if (aire == 0.0)
						return;
					if (aire < 0.0) {
						const NkGuiVertex *t = b;
						b = c;
						c = t;
						aire = -aire;
					}
					float32 minx = a->pos.x, maxx = a->pos.x, miny = a->pos.y, maxy = a->pos.y;
					const NkGuiVertex *trois[3] = {a, b, c};
					for (int32 i = 1; i < 3; ++i) {
						if (trois[i]->pos.x < minx) minx = trois[i]->pos.x;
						if (trois[i]->pos.x > maxx) maxx = trois[i]->pos.x;
						if (trois[i]->pos.y < miny) miny = trois[i]->pos.y;
						if (trois[i]->pos.y > maxy) maxy = trois[i]->pos.y;
					}
					int32 px0 = (int32)minx, py0 = (int32)miny;
					int32 px1 = (int32)maxx + 1, py1 = (int32)maxy + 1;
					if (minx < 0.f) px0 = (int32)minx - 1;
					if (miny < 0.f) py0 = (int32)miny - 1;
					if (px0 < cx0) px0 = cx0;
					if (py0 < cy0) py0 = cy0;
					if (px1 > cx1) px1 = cx1;
					if (py1 > cy1) py1 = cy1;
					if (px1 <= px0 || py1 <= py0)
						return;
					const bool tl0 = HautGauche(b->pos, c->pos), tl1 = HautGauche(c->pos, a->pos),
							   tl2 = HautGauche(a->pos, b->pos);
					float32 ca[4], cb[4], cc[4];
					Depaqueter(a->col, ca);
					Depaqueter(b->col, cb);
					Depaqueter(c->col, cc);
					const float64 inv = 1.0 / aire;
					for (int32 y = py0; y < py1; ++y) {
						for (int32 x = px0; x < px1; ++x) {
							const NkVec2 p{(float32)x + 0.5f, (float32)y + 0.5f};
							const float64 w0 = orient(b->pos, c->pos, p);
							const float64 w1 = orient(c->pos, a->pos, p);
							const float64 w2 = orient(a->pos, b->pos, p);
							if (w0 < 0.0 || w1 < 0.0 || w2 < 0.0)
								continue;
							if ((w0 == 0.0 && !tl0) || (w1 == 0.0 && !tl1) || (w2 == 0.0 && !tl2))
								continue;
							const float32 l0 = (float32)(w0 * inv), l1 = (float32)(w1 * inv), l2 = (float32)(w2 * inv);
							float32 s[4];
							for (int32 k = 0; k < 4; ++k)
								s[k] = ca[k] * l0 + cb[k] * l1 + cc[k] * l2;
							if (tex) {
								const float32 u = a->uv.x * l0 + b->uv.x * l1 + c->uv.x * l2;
								const float32 v = a->uv.y * l0 + b->uv.y * l1 + c->uv.y * l2;
								float32 t[4];
								Echantillon(*tex, u, v, t);
								for (int32 k = 0; k < 4; ++k)
									s[k] *= t[k];
							}
							if (s[3] <= 0.f && mode == NkGuiBlend::Alpha)
								continue;
							Melanger(mPx.Data() + ((usize)y * (usize)mW + (usize)x) * 4u, s, mode);
						}
					}
				}

				NkVector<uint8> mPx;
				NkVector<NkGuiRasterTexture> mTex;
				int32 mW = 0, mH = 0;
		};

	} // namespace nkgui
} // namespace nkentseu
