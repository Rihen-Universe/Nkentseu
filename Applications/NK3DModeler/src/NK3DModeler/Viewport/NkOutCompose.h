// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  NkOutCompose.h — masques de forme et composition des INCRUSTATIONS      ║
// ╚══════════════════════════════════════════════════════════════════════════╝
//
// La pastille Output pose des cibles secondaires SUR l'image principale, avec
// des formes libres (Rihen : « rectangle, carre, cercle etc. »). Le calcul de
// ces formes vit ICI, et non dans NkDemo3D.cpp, pour une raison precise : c'est
// du calcul PUR, sans GPU ni contexte de rendu, donc il se VERIFIE hors de
// l'application -- on genere une planche des six formes et on la regarde. Une
// fonction enfouie dans un fichier de douze mille lignes ne se teste pas.
//
// Conventions : pixels RGBA 8 bits, ligne du haut en premier, pas de padding.

#pragma once

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace nk3d {

		// Formes d'incrustation. Le CERCLE et le CARRE imposent un cadre carre
		// (leur forme ne tolere pas l'etirement) ; l'OVALE, lui, epouse le
		// rapport demande -- c'est toute la difference entre les deux ronds.
		enum NkInsetShape {
			kInsetRect = 0,
			kInsetSquare,
			kInsetCircle,
			kInsetOval,
			kInsetRoundRect,
			kInsetDiamond,
			kInsetShapeCount
		};

		inline const char *NkInsetShapeName(int32 s) {
			static const char *const kNames[kInsetShapeCount] = {
				"Rectangle", "Carre", "Cercle", "Ovale", "Rectangle arrondi", "Losange"};
			return (s >= 0 && s < kInsetShapeCount) ? kNames[s] : "";
		}

		// Couverture du pixel (u,v) par la forme, dans [0,1]. `aa` est la largeur
		// du degrade de bord, en fraction du cote : le contour est lisse au lieu
		// d'etre en escalier. Rectangle et carre sont PLEINS -- leur particularite
		// est le cadre, pas le masque.
		inline float32 NkInsetMask(int32 shape, float32 u, float32 v, float32 aa) {
			if (aa < 1e-4f)
				aa = 1e-4f;
			const float32 dx = (u - 0.5f) * 2.f, dy = (v - 0.5f) * 2.f;
			const float32 ax = (dx < 0.f ? -dx : dx), ay = (dy < 0.f ? -dy : dy);
			float32 d = 0.f; // distance normalisee au centre : 1 = sur le contour
			switch (shape) {
				case kInsetCircle:
				case kInsetOval:
					d = math::NkSqrt(dx * dx + dy * dy);
					break;
				case kInsetDiamond:
					d = ax + ay;
					break;
				case kInsetRoundRect: {
					// Distance au rectangle dont les coins sont arrondis : dans
					// les coins on mesure au centre de l'arrondi, ailleurs c'est
					// la distance de Tchebychev (le rectangle nu).
					const float32 rr = 0.35f; // rayon, en fraction du demi-cote
					const float32 qx = ax - (1.f - rr), qy = ay - (1.f - rr);
					if (qx > 0.f && qy > 0.f)
						d = (1.f - rr) + math::NkSqrt(qx * qx + qy * qy);
					else
						d = (ax > ay ? ax : ay);
					break;
				}
				default:
					return 1.f; // rectangle / carre : plein jusqu'au bord
			}
			const float32 t = (d - (1.f - aa)) / aa;
			if (t <= 0.f)
				return 1.f;
			if (t >= 1.f)
				return 0.f;
			return 1.f - t;
		}

		// Reglages d'une incrustation, cote composition. La position et la taille
		// vivent ailleurs (ce sont des fractions de la principale) : ici on ne
		// traite que le pixel.
		struct NkInsetStyle {
				int32 shape = kInsetRect;
				float32 border = 2.f; // lisere, en pixels de sortie
				float32 borderCol[3] = {1.f, 1.f, 1.f};
				float32 opacity = 1.f;
		};

		// Pose `src` (sw x sh, RGBA) sur `dst` (dw x dh, RGBA) en (dstX, dstY),
		// a la forme demandee. Le lisere se calcule comme la DIFFERENCE entre le
		// masque de la forme et celui de la meme forme retrecie : un seul jeu de
		// formules sert donc les six formes, contour compris -- pas de cas
		// particulier par forme, donc pas de forme oubliee quand on en ajoute une.
		inline void NkInsetCompose(uint8 *dst, int32 dw, int32 dh, const uint8 *src, int32 sw,
								   int32 sh, int32 dstX, int32 dstY, const NkInsetStyle &st) {
			if (!dst || !src || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
				return;
			const int32 minSide = sw < sh ? sw : sh;
			// Un pixel et demi de degrade : plus fin, le contour recrenele ;
			// plus large, les petites incrustations s'estompent.
			const float32 aa = 1.5f / (float32)minSide;
			const float32 bw = st.border * 2.f / (float32)minSide;
			for (int32 yy = 0; yy < sh; ++yy) {
				const int32 dy = dstY + yy;
				if (dy < 0 || dy >= dh)
					continue;
				const float32 v = ((float32)yy + 0.5f) / (float32)sh;
				for (int32 xx = 0; xx < sw; ++xx) {
					const int32 dx = dstX + xx;
					if (dx < 0 || dx >= dw)
						continue;
					const float32 u = ((float32)xx + 0.5f) / (float32)sw;
					const float32 m = NkInsetMask(st.shape, u, v, aa);
					if (m <= 0.001f)
						continue;
					// Masque INTERIEUR : la meme forme, retrecie de l'epaisseur
					// du lisere. Entre les deux, c'est le contour.
					float32 mi = 1.f;
					if (bw > 1e-4f && bw < 1.f) {
						const float32 uu = 0.5f + (u - 0.5f) / (1.f - bw);
						const float32 vv = 0.5f + (v - 0.5f) / (1.f - bw);
						mi = (uu < 0.f || uu > 1.f || vv < 0.f || vv > 1.f)
								 ? 0.f
								 : NkInsetMask(st.shape, uu, vv, aa);
					}
					const uint8 *sp = src + ((int64)yy * sw + xx) * 4;
					uint8 *dp = dst + ((int64)dy * dw + dx) * 4;
					const float32 a = m * st.opacity;
					for (int32 c = 0; c < 3; ++c) {
						const float32 srcC = (float32)sp[c];
						const float32 brdC = st.borderCol[c] * 255.f;
						const float32 col = brdC + (srcC - brdC) * mi;
						dp[c] = (uint8)((float32)dp[c] + (col - (float32)dp[c]) * a + 0.5f);
					}
					const float32 na = (float32)dp[3] + (255.f - (float32)dp[3]) * a;
					dp[3] = (uint8)(na + 0.5f);
				}
			}
		}

		// Nombre de dimensions REGLABLES d'une forme (Rihen : « chaque forme
		// doit avoir ses propres dimensions »). Un carre et un cercle n'ont
		// qu'un cote a donner -- leur imposer deux champs dont l'un serait
		// ignore serait mentir sur ce qu'on regle.
		inline int32 NkInsetDimCount(int32 shape) {
			return (shape == kInsetSquare || shape == kInsetCircle) ? 1 : 2;
		}
		// Libelles des champs, propres a chaque forme : « Diametre » pour un
		// cercle, « Cote » pour un carre, « Largeur / Hauteur » ailleurs.
		inline const char *NkInsetDimName(int32 shape, int32 axis) {
			if (shape == kInsetCircle)
				return "Diametre";
			if (shape == kInsetSquare)
				return "Cote";
			return axis == 0 ? "Largeur" : "Hauteur";
		}

		// ── ALPHA RECONSTRUIT DEPUIS DEUX FONDS ─────────────────────────────
		// La chaine de post-traitement termine par `vec4(rgb, 1.)` : elle
		// DETRUIT l'alpha, quoi qu'on efface en amont. Corriger ces shaders sur
		// les cinq backends est un chantier a part ; en attendant, on retrouve
		// l'alpha exactement, sans toucher au rendu, en composant la scene deux
		// fois -- une fois sur fond NOIR, une fois sur fond BLANC :
		//
		//   C_noir  = a.C            (le fond noir n'ajoute rien)
		//   C_blanc = a.C + (1-a)    (le fond blanc ajoute son complement)
		//   => a = 1 - (C_blanc - C_noir)      et      C = C_noir / a
		//
		// C'est la methode des compositeurs. Elle rend un alpha PARTIEL correct
		// -- bords antialiases, objets translucides -- la ou un simple seuil sur
		// la couleur ne saurait que dire « dedans » ou « dehors ».
		//
		// `dark` reste intact ; `white` recoit le resultat (couleur + alpha),
		// pour n'allouer aucun troisieme tampon.
		inline void NkAlphaFromTwoBackgrounds(const uint8 *dark, uint8 *white, int32 w, int32 h) {
			if (!dark || !white || w <= 0 || h <= 0)
				return;
			const int64 n = (int64)w * (int64)h;
			// ── LE BLANC REEL, MESURE ───────────────────────────────────────
			// La formule suppose que le fond blanc ressort a 1. C'est faux : le
			// tonemap ACES COMPRESSE les hautes lumieres, si bien qu'un fond
			// pur donnait un ecart de 0,97 au lieu de 1 -- et donc un alpha
			// residuel de 8/255 la ou il n'y a rien (mesure sur un rendu reel).
			// On mesure donc l'ecart MAXIMAL de l'image : c'est celui d'un pixel
			// ou il n'y a rien, donc le blanc effectif apres toute la chaine.
			// Tant que la chaine est monotone, cela l'annule exactement.
			float32 wref = 0.f;
			for (int64 i = 0; i < n; ++i) {
				const uint8 *d = dark + i * 4;
				const uint8 *o = white + i * 4;
				float32 diff = 0.f;
				for (int32 c = 0; c < 3; ++c)
					diff += ((float32)o[c] - (float32)d[c]);
				diff /= (3.f * 255.f);
				if (diff > wref)
					wref = diff;
			}
			// Image sans aucun fond visible : rien a extraire, tout est opaque.
			// Sans cette garde, un ecart nul diviserait par zero.
			if (wref < 0.05f)
				wref = 1.f;
			const float32 invW = 1.f / wref;
			for (int64 i = 0; i < n; ++i) {
				const uint8 *d = dark + i * 4;
				uint8 *o = white + i * 4;
				// L'ecart moyen sur les trois canaux : en theorie chacun donne
				// le meme alpha, la moyenne absorbe le bruit du tonemap.
				float32 diff = 0.f;
				for (int32 c = 0; c < 3; ++c)
					diff += ((float32)o[c] - (float32)d[c]);
				diff /= (3.f * 255.f);
				float32 a = 1.f - diff * invW;
				if (a <= 0.003f) {
					o[0] = o[1] = o[2] = o[3] = 0; // rien ici : pixel efface
					continue;
				}
				if (a > 1.f)
					a = 1.f;
				// Couleur DEMULTIPLIEE : le PNG attend du non-premultiplie.
				for (int32 c = 0; c < 3; ++c) {
					float32 v = (float32)d[c] / a;
					if (v > 255.f)
						v = 255.f;
					o[c] = (uint8)(v + 0.5f);
				}
				o[3] = (uint8)(a * 255.f + 0.5f);
			}
		}

		// Taille en pixels d'une incrustation. `sizeW` est une fraction de la
		// LARGEUR de la principale, `sizeH` une fraction de sa HAUTEUR : chacune
		// est ainsi lisible telle quelle (0,25 = le quart du cote). Carre et
		// cercle n'utilisent que `sizeW` et se ferment sur un cadre carre EN
		// PIXELS -- sans quoi un « carre » de 25 % serait un rectangle des que
		// la sortie n'est pas carree.
		inline void NkInsetPixels(int32 shape, float32 sizeW, float32 sizeH, int32 mainW,
								  int32 mainH, int32 *outW, int32 *outH) {
			int32 ww = (int32)(sizeW * (float32)mainW + 0.5f);
			if (ww < 8)
				ww = 8;
			if (ww > mainW)
				ww = mainW;
			int32 hh;
			if (NkInsetDimCount(shape) == 1)
				hh = ww; // cadre carre en pixels
			else
				hh = (int32)(sizeH * (float32)mainH + 0.5f);
			if (hh < 8)
				hh = 8;
			if (hh > mainH)
				hh = mainH;
			*outW = ww;
			*outH = hh;
		}

		// ── LE CURSEUR DANS LA VIDEO DE TUTORIEL ────────────────────────────
		// La capture de fenetre de l'OS (PrintWindow) NE CONTIENT PAS le
		// curseur : une video de tutoriel sans pointeur montre des menus qui
		// s'ouvrent tout seuls. On le dessine donc nous-memes, avec la TRACE
		// des positions recentes -- c'est le mouvement qui se lit, pas la
		// position instantanee. Uniquement en video : une capture fixe n'a pas
		// de trajectoire a raconter (regle de Rihen).

		// Anneau des dernieres positions. Petit et de taille fixe : une trace
		// plus longue brouillerait l'image au lieu de l'expliquer.
		struct NkCursorTrail {
				static const int32 kMax = 24;
				int32 x[kMax] = {};
				int32 y[kMax] = {};
				int32 count = 0; ///< positions valides (<= kMax)
				int32 head = 0;	 ///< prochaine case a ecrire

				void Push(int32 px, int32 py) {
					x[head] = px;
					y[head] = py;
					head = (head + 1) % kMax;
					if (count < kMax)
						++count;
				}
				void Clear() {
					count = 0;
					head = 0;
				}
				// i = 0 est la position LA PLUS RECENTE.
				void At(int32 i, int32 *px, int32 *py) const {
					const int32 k = ((head - 1 - i) % kMax + kMax) % kMax;
					*px = x[k];
					*py = y[k];
				}
		};

		// Melange une couleur sur un pixel RGBA (alpha 0..255). L'image de
		// capture est opaque : on ecrit donc directement le resultat melange.
		inline void NkBlendPixel(uint8 *px, int32 w, int32 h, int32 X, int32 Y, uint8 r, uint8 g,
								 uint8 b, int32 a) {
			if (X < 0 || Y < 0 || X >= w || Y >= h || a <= 0)
				return;
			if (a > 255)
				a = 255;
			uint8 *p = px + ((int64)Y * w + X) * 4;
			p[0] = (uint8)((p[0] * (255 - a) + r * a) / 255);
			p[1] = (uint8)((p[1] * (255 - a) + g * a) / 255);
			p[2] = (uint8)((p[2] * (255 - a) + b * a) / 255);
		}

		// Disque plein, utilise pour la trace.
		inline void NkBlendDisc(uint8 *px, int32 w, int32 h, int32 cx, int32 cy, float32 rad,
								uint8 r, uint8 g, uint8 b, int32 a) {
			const int32 ir = (int32)(rad + 1.f);
			for (int32 dy = -ir; dy <= ir; ++dy)
				for (int32 dx = -ir; dx <= ir; ++dx) {
					const float32 d = (float32)(dx * dx + dy * dy);
					if (d > rad * rad)
						continue;
					NkBlendPixel(px, w, h, cx + dx, cy + dy, r, g, b, a);
				}
		}

		// LA FLECHE, dessinee a la main. Un pointeur systeme ne peut pas etre
		// recupere de facon fiable (theme, DPI, curseur personnalise) : mieux
		// vaut un dessin CONSTANT, reconnaissable, et identique d'une video a
		// l'autre. Blanc borde de noir pour rester lisible sur clair comme sur
		// sombre -- c'est la raison du contour.
		inline void NkDrawCursorArrow(uint8 *px, int32 w, int32 h, int32 cx, int32 cy,
									  float32 scale) {
			// Silhouette en coordonnees locales (pointe en 0,0), a l'echelle 1.
			// Test d'appartenance par produits vectoriels sur deux triangles :
			// pas de rasterizer a ecrire pour une forme de douze pixels.
			const float32 S = scale;
			const float32 ax = 0.f, ay = 0.f;	  // pointe
			const float32 bx = 0.f, by = 17.f * S; // bord gauche
			const float32 mx = 4.5f * S, my = 12.5f * S;
			const float32 tx = 12.f * S, ty = 12.f * S;
			const int32 x0 = cx - 2, y0 = cy - 2;
			const int32 x1 = cx + (int32)(14.f * S) + 2, y1 = cy + (int32)(19.f * S) + 2;
			// Deux passes : le CONTOUR d'abord (silhouette dilatee d'un pixel),
			// le blanc ensuite. Dessiner l'inverse mangerait le remplissage.
			for (int32 pass = 0; pass < 2; ++pass) {
				const float32 grow = (pass == 0) ? 1.6f : 0.f;
				for (int32 Y = y0; Y <= y1; ++Y)
					for (int32 X = x0; X <= x1; ++X) {
						const float32 lx = (float32)(X - cx), ly = (float32)(Y - cy);
						// Distance au triangle : on teste l'appartenance aux
						// deux triangles qui composent la fleche, dilates.
						bool in = false;
						const float32 tri[2][6] = {{ax, ay, bx, by, mx, my},
												   {ax, ay, mx, my, tx, ty}};
						for (int32 t = 0; t < 2 && !in; ++t) {
							const float32 *T = tri[t];
							// Centre du triangle, pour la dilatation.
							const float32 gx = (T[0] + T[2] + T[4]) / 3.f;
							const float32 gy = (T[1] + T[3] + T[5]) / 3.f;
							float32 P[6];
							for (int32 v = 0; v < 3; ++v) {
								const float32 vx = T[v * 2], vy = T[v * 2 + 1];
								const float32 dx = vx - gx, dy = vy - gy;
								const float32 len = (float32)math::NkSqrt(dx * dx + dy * dy);
								const float32 k = (len > 1e-3f) ? (grow / len) : 0.f;
								P[v * 2] = vx + dx * k;
								P[v * 2 + 1] = vy + dy * k;
							}
							float32 s[3];
							for (int32 e = 0; e < 3; ++e) {
								const float32 x1e = P[e * 2], y1e = P[e * 2 + 1];
								const float32 x2e = P[((e + 1) % 3) * 2];
								const float32 y2e = P[((e + 1) % 3) * 2 + 1];
								s[e] = (x2e - x1e) * (ly - y1e) - (y2e - y1e) * (lx - x1e);
							}
							in = (s[0] >= 0.f && s[1] >= 0.f && s[2] >= 0.f) ||
								 (s[0] <= 0.f && s[1] <= 0.f && s[2] <= 0.f);
						}
						if (!in)
							continue;
						if (pass == 0)
							NkBlendPixel(px, w, h, X, Y, 0, 0, 0, 235);
						else
							NkBlendPixel(px, w, h, X, Y, 255, 255, 255, 255);
					}
			}
		}

		// Trace + fleche, sur l'image capturee. `scale` suit la taille de la
		// fenetre : un curseur de 16 px dans une video 4K serait invisible.
		inline void NkDrawCursorTrail(uint8 *px, int32 w, int32 h, const NkCursorTrail &tr,
									  float32 scale = 1.f) {
			if (!px || tr.count <= 0)
				return;
			// LA TRACE S'EFFACE EN REMONTANT LE TEMPS : la position la plus
			// ancienne est la plus pale. Sans ce degrade, la trace serait un
			// trait qui ne dit pas dans quel sens la souris est allee.
			for (int32 i = tr.count - 1; i >= 1; --i) {
				int32 X = 0, Y = 0;
				tr.At(i, &X, &Y);
				const float32 t = 1.f - (float32)i / (float32)tr.count;
				const int32 a = (int32)(120.f * t * t);
				if (a <= 2)
					continue;
				NkBlendDisc(px, w, h, X, Y, 2.5f * scale * (0.4f + 0.6f * t), 255, 210, 60, a);
			}
			int32 cx = 0, cy = 0;
			tr.At(0, &cx, &cy);
			NkDrawCursorArrow(px, w, h, cx, cy, scale);
		}

	} // namespace nk3d
} // namespace nkentseu
