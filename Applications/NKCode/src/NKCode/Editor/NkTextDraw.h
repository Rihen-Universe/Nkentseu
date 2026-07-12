#pragma once
// =============================================================================
// NkTextDraw.h — Rendu de texte avec INTERCEPTION des codepoints "dessinables"
//   (box-drawing U+2500-257F, blocs U+2580-259F, formes geometriques U+25A0-25FF,
//   fleches U+2190-2193). Ces glyphes manquent aux polices embarquees -> on les
//   DESSINE en primitives (lignes/rects), independamment de la police. Le reste
//   du texte passe par AddTextRange (police). Partage par l'editeur et le terminal.
// =============================================================================
#include "NKGui/NKGui.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKEditorKit/NkEditorTextField.h"    // NkOverlayTextField (widget moteur reutilisable)
#include "NKEditorKit/NkEditorContextMenu.h" // NkCtxMenu / NkCtxMenuDraw (widget moteur reutilisable)

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu;
		using namespace nkentseu::nkgui;
		using namespace nkentseu::editorkit;

		// Decode un codepoint UTF-8 ; avance p. 0xFFFD si invalide.
		inline uint32 NkDecodeU8(const char *&p, const char *end) {
			const unsigned char c = static_cast<unsigned char>(*p);
			if (c < 0x80) {
				++p;
				return c;
			}
			if ((c >> 5) == 0x6 && p + 1 < end) {
				uint32 cp = ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(p[1]) & 0x3Fu);
				p += 2;
				return cp;
			}
			if ((c >> 4) == 0xE && p + 2 < end) {
				uint32 cp = ((c & 0x0Fu) << 12) | ((static_cast<unsigned char>(p[1]) & 0x3Fu) << 6) |
							(static_cast<unsigned char>(p[2]) & 0x3Fu);
				p += 3;
				return cp;
			}
			if ((c >> 3) == 0x1E && p + 3 < end) {
				uint32 cp = ((c & 0x07u) << 18) | ((static_cast<unsigned char>(p[1]) & 0x3Fu) << 12) |
							((static_cast<unsigned char>(p[2]) & 0x3Fu) << 6) |
							(static_cast<unsigned char>(p[3]) & 0x3Fu);
				p += 4;
				return cp;
			}
			++p;
			return 0xFFFDu;
		}

		// Bascule ctx.font sur la police de CODE (monospace) le temps d'un scope, puis
		// restaure la police d'interface. Utilise par l'editeur et le terminal pour que
		// tout (metriques + rendu via NkDrawTextU) emploie la police monospace.
		struct NkCodeFontScope {
				NkGuiContext &c;
				NkGuiFont *prev;

				explicit NkCodeFontScope(NkGuiContext &ctx) : c(ctx), prev(ctx.font) {
					if (ctx.codeFont && ctx.codeFont->Valid())
						ctx.font = ctx.codeFont;
				}

				// Variante avec police EXPLICITE (ex. terminal -> atlas propre, decouple du zoom
				// par-onglet de l'editeur). Repli sur ctx.codeFont si f nul/invalide.
				NkCodeFontScope(NkGuiContext &ctx, NkGuiFont *f) : c(ctx), prev(ctx.font) {
					if (f && f->Valid())
						ctx.font = f;
					else if (ctx.codeFont && ctx.codeFont->Valid())
						ctx.font = ctx.codeFont;
				}

				~NkCodeFontScope() {
					c.font = prev;
				}
		};

		// Encode un codepoint en UTF-8 dans dst (>=4 octets). Retourne le nb d'octets.
		inline int32 NkEncodeU8(uint32 cp, char *dst) {
			if (cp < 0x80) {
				dst[0] = static_cast<char>(cp);
				return 1;
			}
			if (cp < 0x800) {
				dst[0] = static_cast<char>(0xC0 | (cp >> 6));
				dst[1] = static_cast<char>(0x80 | (cp & 0x3F));
				return 2;
			}
			if (cp < 0x10000) {
				dst[0] = static_cast<char>(0xE0 | (cp >> 12));
				dst[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
				dst[2] = static_cast<char>(0x80 | (cp & 0x3F));
				return 3;
			}
			dst[0] = static_cast<char>(0xF0 | (cp >> 18));
			dst[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
			dst[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
			dst[3] = static_cast<char>(0x80 | (cp & 0x3F));
			return 4;
		}

		// ── Réparation du DOUBLE-ENCODAGE UTF-8 (« mojibake ») ────────────────────
		// Un fichier UTF-8 relu en CP1252 puis re-sauvé en UTF-8 donne "Ã©" au lieu de "é".
		// Réparer = décoder l'UTF-8 courant en codepoints, remapper chaque codepoint vers
		// son OCTET CP1252 d'origine ; le flux d'octets obtenu EST l'UTF-8 correct.
		// Renvoie l'octet CP1252 d'un codepoint ; ok=false si le codepoint n'en provient pas.
		inline unsigned char NkCp1252ByteOf(uint32 cp, bool &ok) {
			ok = true;
			if (cp < 0x80u)
				return static_cast<unsigned char>(cp);
			if (cp >= 0xA0u && cp <= 0xFFu)
				return static_cast<unsigned char>(cp);
			switch (cp) { // spécifiques CP1252 (plage 0x80-0x9F)
				case 0x20ACu:
					return 0x80;
				case 0x201Au:
					return 0x82;
				case 0x0192u:
					return 0x83;
				case 0x201Eu:
					return 0x84;
				case 0x2026u:
					return 0x85;
				case 0x2020u:
					return 0x86;
				case 0x2021u:
					return 0x87;
				case 0x02C6u:
					return 0x88;
				case 0x2030u:
					return 0x89;
				case 0x0160u:
					return 0x8A;
				case 0x2039u:
					return 0x8B;
				case 0x0152u:
					return 0x8C;
				case 0x017Du:
					return 0x8E;
				case 0x2018u:
					return 0x91;
				case 0x2019u:
					return 0x92;
				case 0x201Cu:
					return 0x93;
				case 0x201Du:
					return 0x94;
				case 0x2022u:
					return 0x95;
				case 0x2013u:
					return 0x96;
				case 0x2014u:
					return 0x97;
				case 0x02DCu:
					return 0x98;
				case 0x2122u:
					return 0x99;
				case 0x0161u:
					return 0x9A;
				case 0x203Au:
					return 0x9B;
				case 0x0153u:
					return 0x9C;
				case 0x017Eu:
					return 0x9E;
				case 0x0178u:
					return 0x9F;
			}
			ok = false;
			return 0;
		}

		// Longueur d'une chaîne C (sans dépendre de <cstring>).
		inline const char *NkStrEndPtr(const char *s) {
			const char *e = s;
			while (*e)
				++e;
			return e;
		}

		// Répare une chaîne double-encodée -> UTF-8 correct. Les codepoints non issus de
		// CP1252 (contenu déjà bon / CJK réel) sont laissés tels quels (sécurité contenu mixte).
		inline NkString NkMojibakeRepair(const char *s) {
			if (!s)
				return NkString();
			const char *end = NkStrEndPtr(s);
			NkVector<char> out;
			for (const char *p = s; p < end;) {
				const char *q = p;
				const uint32 cp = NkDecodeU8(q, end);
				bool ok;
				const unsigned char b = NkCp1252ByteOf(cp, ok);
				if (ok)
					out.PushBack(static_cast<char>(b));
				else {
					char t[4];
					const int32 n = NkEncodeU8(cp, t);
					for (int32 i = 0; i < n; ++i)
						out.PushBack(t[i]);
				}
				p = q;
			}
			out.PushBack('\0');
			return NkString(out.Data());
		}

		// Détecte un contenu vraisemblablement double-encodé : compte les têtes de mojibake
		// (Ã U+00C3, Â U+00C2, â U+00E2) suivies d'un codepoint « haut ». Seuil prudent.
		inline bool NkMojibakeDetect(const char *s) {
			if (!s)
				return false;
			const char *end = NkStrEndPtr(s);
			int32 hits = 0;
			for (const char *p = s; p < end;) {
				const char *q = p;
				const uint32 cp = NkDecodeU8(q, end);
				if (cp == 0x00C3u || cp == 0x00C2u || cp == 0x00E2u) {
					const char *r = q;
					if (r < end) {
						const uint32 nx = NkDecodeU8(r, end);
						if (nx >= 0x80u && nx <= 0x2122u)
							++hits;
					}
				}
				p = q;
			}
			return hits >= 4;
		}

		// ── Menu contextuel (clic droit) : DEPLACE dans NKEditorKit (widget moteur
		//    reutilisable). Ré-exporte NkCtxMenu + NkCtxMenuDraw pour les appelants. ──
		using editorkit::NkCtxMenu;
		using editorkit::NkCtxMenuDraw;

		inline bool NkIsDrawable(uint32 cp) {
			return (cp >= 0x2500u && cp <= 0x25FFu) || (cp >= 0x2190u && cp <= 0x2193u);
		}

		// Dessine le glyphe primitif pour `cp` dans la cellule (x, top, w, h).
		inline void NkDrawGlyphPrim(NkGuiDrawList &dl, uint32 cp, float32 x, float32 top, float32 w, float32 h,
									const NkColor &col) {
			const float32 mx = x + w * 0.5f, my = top + h * 0.5f, r = x + w, b = top + h, t = 1.3f;
			const NkColor a64{col.r, col.g, col.b, 64}, a110{col.r, col.g, col.b, 110}, a170{col.r, col.g, col.b, 170};
			auto H = [&](float32 x0, float32 x1) { dl.AddLine({x0, my}, {x1, my}, col, t); };
			auto V = [&](float32 y0, float32 y1) { dl.AddLine({mx, y0}, {mx, y1}, col, t); };
			switch (cp) {
				case 0x2500:
					H(x, r);
					break; // ─
				case 0x2502:
					V(top, b);
					break; // │
				case 0x250C:
					H(mx, r);
					V(my, b);
					break; // ┌
				case 0x2510:
					H(x, mx);
					V(my, b);
					break; // ┐
				case 0x2514:
					H(mx, r);
					V(top, my);
					break; // └
				case 0x2518:
					H(x, mx);
					V(top, my);
					break; // ┘
				case 0x251C:
					V(top, b);
					H(mx, r);
					break; // ├
				case 0x2524:
					V(top, b);
					H(x, mx);
					break; // ┤
				case 0x252C:
					H(x, r);
					V(my, b);
					break; // ┬
				case 0x2534:
					H(x, r);
					V(top, my);
					break; // ┴
				case 0x253C:
					H(x, r);
					V(top, b);
					break; // ┼
				case 0x2550:
					dl.AddLine({x, my - 2}, {r, my - 2}, col, t);
					dl.AddLine({x, my + 2}, {r, my + 2}, col, t);
					break; // ═
				case 0x2551:
					dl.AddLine({mx - 2, top}, {mx - 2, b}, col, t);
					dl.AddLine({mx + 2, top}, {mx + 2, b}, col, t);
					break; // ║
				case 0x2588:
					dl.AddRectFilled({x, top, w, h}, col);
					break; // █
				case 0x2580:
					dl.AddRectFilled({x, top, w, h * 0.5f}, col);
					break; // ▀
				case 0x2584:
					dl.AddRectFilled({x, my, w, h * 0.5f}, col);
					break; // ▄
				case 0x258C:
					dl.AddRectFilled({x, top, w * 0.5f, h}, col);
					break; // ▌
				case 0x2590:
					dl.AddRectFilled({mx, top, w * 0.5f, h}, col);
					break; // ▐
				case 0x2591:
					dl.AddRectFilled({x, top, w, h}, a64);
					break; // ░
				case 0x2592:
					dl.AddRectFilled({x, top, w, h}, a110);
					break; // ▒
				case 0x2593:
					dl.AddRectFilled({x, top, w, h}, a170);
					break; // ▓
				case 0x25A0:
					dl.AddRectFilled({x + w * 0.2f, top + h * 0.28f, w * 0.6f, h * 0.45f}, col);
					break; // ■
				case 0x25A1:
					dl.AddRect({x + w * 0.2f, top + h * 0.28f, w * 0.6f, h * 0.45f}, col, t);
					break; // □
				case 0x25CF:
					dl.AddCircleFilled({mx, my}, w * 0.3f, col);
					break; // ●
				case 0x25B2:
					dl.AddTriangleFilled({mx, top + h * 0.28f}, {x + w * 0.25f, b - h * 0.28f},
										 {r - w * 0.25f, b - h * 0.28f}, col);
					break; // ▲
				case 0x25BC:
					dl.AddTriangleFilled({x + w * 0.25f, top + h * 0.28f}, {r - w * 0.25f, top + h * 0.28f},
										 {mx, b - h * 0.28f}, col);
					break; // ▼
				case 0x2190:
					H(x, r);
					dl.AddTriangleFilled({x, my}, {x + w * 0.3f, my - h * 0.16f}, {x + w * 0.3f, my + h * 0.16f}, col);
					break; // ←
				case 0x2192:
					H(x, r);
					dl.AddTriangleFilled({r, my}, {r - w * 0.3f, my - h * 0.16f}, {r - w * 0.3f, my + h * 0.16f}, col);
					break; // →
				case 0x2191:
					V(top, b);
					dl.AddTriangleFilled({mx, top}, {mx - w * 0.16f, top + h * 0.3f}, {mx + w * 0.16f, top + h * 0.3f},
										 col);
					break; // ↑
				case 0x2193:
					V(top, b);
					dl.AddTriangleFilled({mx, b}, {mx - w * 0.16f, b - h * 0.3f}, {mx + w * 0.16f, b - h * 0.3f}, col);
					break; // ↓
				default:
					dl.AddRect({x + w * 0.22f, top + h * 0.3f, w * 0.56f, h * 0.4f}, col, 1.f);
					break; // autres : petit cadre
			}
		}

		// Rend [begin,end) en interceptant les codepoints dessinables (primitives) ;
		// le reste via la police. Retourne le x final. (cellTop, cellH) = la ligne.
		inline float32 NkDrawTextU(NkGuiContext &ctx, float32 x, float32 baseline, float32 cellTop, float32 cellH,
								   const char *begin, const char *end, const NkColor &col) {
			if (!ctx.font || !ctx.font->Valid())
				return x;
			const NkFont *face = ctx.font->Face();
			const uint32 tex = ctx.font->TexId();
			const char *run = begin;
			const char *p = begin;
			auto flush = [&](const char *e) {
				if (e > run) {
					ctx.DL().AddTextRange(face, tex, {x, baseline}, run, e, col);
					x += face->CalcTextSizeX(run, e);
				}
			};
			while (p < end) {
				const char *q = p;
				const uint32 cp = NkDecodeU8(q, end);
				// Primitive UNIQUEMENT si la police n'a pas le glyphe (DejaVu a le
				// box-drawing -> on le laisse rendre ; DroidSans non -> on dessine).
				if (NkIsDrawable(cp) && !face->FindGlyphNoFallback(cp)) {
					flush(p);
					const float32 w = face->CalcTextSizeX(p, q); // avance (glyphe de repli)
					NkDrawGlyphPrim(ctx.DL(), cp, x, cellTop, w, cellH, col);
					x += w;
					run = q;
				}
				p = q;
			}
			flush(p);
			return x;
		}

		// ── Champ de saisie mono-ligne minimal pour l'overlay (positionne en absolu) ──
		// DEPLACE dans NKEditorKit (widget reutilisable, engine-native) : on ré-exporte
		// ici l'implementation moteur pour tous les appelants historiques (nkcode).
		using editorkit::NkOverlayTextField;
	} // namespace nkcode
} // namespace nkentseu
