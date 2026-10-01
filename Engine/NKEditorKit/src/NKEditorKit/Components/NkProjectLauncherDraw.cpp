// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkProjectLauncherDraw.cpp — le DESSIN et les GESTES du lanceur de projets.
// Le contrat est dans NkProjectLauncherModel.h ; ici, la mise en page.
//
// LA MISE EN PAGE (facon Unity Hub pour la colonne, Unreal 5 pour les cartes)
//   ┌ barre de titre (fenetres sans cadre seulement) ─────────────────────────┐
//   │ COLONNE      │ Titre de page                    [Ouvrir...] [+ Nouveau] │
//   │ [logo] Nom   │ sous-titre                                               │
//   │ sous-titre   │ (recherche.............)          [grille|liste] [tri]   │
//   │              │ ┌ zone qui defile ───────────────────────────────────┐   │
//   │ ▣ Projets    │ │ bande de version (si l'application en livre une)    │   │
//   │ ▤ Apprendre  │ │ COMMENCER UN NOUVEAU PROJET : cartes illustrees     │   │
//   │ ◎ Communaute │ │ PROJETS RECENTS : grandes vignettes                  │   │
//   │ ⬇ Installat. │ └─────────────────────────────────────────────────────┘   │
//   │ ☾ theme      │ pied : erreur ou texte            astuce                  │
//   └──────────────┴───────────────────────────────────────────────────────────┘
//
// ⚠️ « RIEN DE TRONQUE » (demande de Rihen) : un nom, un dossier ou une
//    description qui ne tient pas sur une ligne PASSE A LA LIGNE, et la carte
//    grandit ; toutes les cartes d'une rangee prennent la hauteur de la plus
//    haute. Les points de suspension ne restent qu'en dernier recours, au-dela
//    de trois lignes.
//
// ⚠️ LES COULEURS MELANGEES (teinte d'accent, voile, survol) passent par
//    `math::NkColor` (Lerp, WithAlpha) : aucun convertisseur maison.
// =============================================================================
#include "NKEditorKit/Components/NkProjectLauncherModel.h"
#include "NKMath/NkColor.h"
#include "NKMath/NkFunctions.h"
#include <cstdio>
#include <initializer_list>

namespace nkentseu {
	namespace editorkit {

		namespace {

			// ── Couleurs (0xRRGGBBAA), par NKMath ───────────────────────────────
			inline uint32 Mix(uint32 a, uint32 b, float32 t) {
				return math::NkColor(a).Lerp(math::NkColor(b), t).ToUint32A();
			}
			inline uint32 Alpha(uint32 c, int32 a) {
				return math::NkColor(c).WithAlpha(a).ToUint32A();
			}
			const uint32 kBlanc = 0xFFFFFFFFu;
			const uint32 kNoir = 0x000000FFu;

			// ── Primitives vectorielles, toutes par `PolygonHex` ────────────────
			// Un trait est un quadrilatere : le peintre n'a besoin que de savoir
			// remplir un polygone convexe, et la couleur est libre (pas un role).
			void Trait(NkComponentPaint &p, float32 x1, float32 y1, float32 x2, float32 y2, float32 ep,
					   uint32 c) {
				const float32 dx = x2 - x1, dy = y2 - y1;
				const float32 l = math::NkSqrt(dx * dx + dy * dy);
				if (l < 1e-4f)
					return;
				const float32 nx = -dy / l * ep * 0.5f, ny = dx / l * ep * 0.5f;
				const float32 q[8] = {x1 + nx, y1 + ny, x2 + nx, y2 + ny, x2 - nx, y2 - ny, x1 - nx, y1 - ny};
				(void)p.PolygonHex(q, 4, c);
			}
			void Disque(NkComponentPaint &p, float32 cx, float32 cy, float32 r, uint32 c) {
				enum { kN = 24 };
				float32 q[kN * 2];
				for (int32 i = 0; i < kN; ++i) {
					const float32 a = 6.2831853f * (float32)i / (float32)kN;
					q[i * 2] = cx + r * math::NkCos(a);
					q[i * 2 + 1] = cy + r * math::NkSin(a);
				}
				(void)p.PolygonHex(q, kN, c);
			}
			void Anneau(NkComponentPaint &p, float32 cx, float32 cy, float32 r, float32 ep, uint32 c) {
				enum { kN = 28 };
				for (int32 i = 0; i < kN; ++i) {
					// Chaque segment deborde un peu : pas de fente entre deux traits.
					const float32 a0 = 6.2831853f * ((float32)i - 0.08f) / (float32)kN;
					const float32 a1 = 6.2831853f * ((float32)i + 1.08f) / (float32)kN;
					Trait(p, cx + r * math::NkCos(a0), cy + r * math::NkSin(a0), cx + r * math::NkCos(a1),
						  cy + r * math::NkSin(a1), ep, c);
				}
			}
			void Poly(NkComponentPaint &p, const float32 *xy, int32 n, uint32 c) {
				(void)p.PolygonHex(xy, n, c);
			}

			// ── Texte ───────────────────────────────────────────────────────────
			struct Ligne {
					const char *d = nullptr;
					const char *f = nullptr;
					bool suspension = false;
			};

			inline const char *CarSuivant(const char *q) {
				++q;
				while (*q && (((unsigned char)*q) & 0xC0u) == 0x80u)
					++q;
				return q;
			}

			/// Coupe `s` en lignes de largeur `w` (police `pol`). Coupe aux espaces,
			/// et aux barres pour un chemin. Au-dela de `maxL` lignes, la derniere
			/// porte des points de suspension -- le seul cas ou l'on tronque.
			int32 Couper(NkComponentPaint &p, const char *s, float32 w, uint8 pol, Ligne *out, int32 maxL,
						 bool chemin) {
				int32 n = 0;
				if (!s || !*s || w <= 4.f || maxL <= 0)
					return 0;
				const char *d = s;
				while (*d && n < maxL) {
					while (*d == ' ')
						++d;
					if (!*d)
						break;
					const char *q = d, *fin = d, *coupe = nullptr;
					bool deborde = false;
					while (*q) {
						const char *nq = CarSuivant(q);
						if (p.LargeurPolice(d, nq, pol) > w) {
							deborde = true;
							break;
						}
						q = nq;
						fin = q;
						if (*q == ' ' || (chemin && (q[-1] == '/' || q[-1] == '\\')))
							coupe = q;
					}
					if (!deborde) {
						out[n++] = Ligne{d, q, false};
						d = q;
						break;
					}
					const char *c = (coupe && coupe > d) ? coupe : (fin > d ? fin : CarSuivant(d));
					out[n++] = Ligne{d, c, false};
					d = c;
				}
				while (*d == ' ')
					++d;
				if (*d && n > 0)
					out[n - 1].suspension = true;
				return n;
			}

			struct Ecrivain {
					NkComponentPaint &p;
					float32 H(uint8 pol) const {
						return p.HauteurPolice(pol);
					}
					float32 W(const char *s, uint8 pol, const char *fin = nullptr) const {
						return s ? p.LargeurPolice(s, fin, pol) : 0.f;
					}
					void T(float32 x, float32 y, const char *s, uint16 role, uint8 pol, const char *fin = nullptr) {
						if (!s || !*s)
							return;
						p.TextePolice({x, y, W(s, pol, fin) + 4.f, H(pol)}, s, fin, role, pol);
					}
					/// Centre horizontalement dans [x, x + w].
					void TC(float32 x, float32 w, float32 y, const char *s, uint16 role, uint8 pol) {
						T(x + (w - W(s, pol)) * 0.5f, y, s, role, pol);
					}
					/// Ecrit des lignes coupees ; rend la hauteur consommee.
					float32 Lignes(float32 x, float32 y, const Ligne *l, int32 n, uint16 role, uint8 pol,
								   float32 interligne) {
						const float32 h = H(pol) + interligne;
						for (int32 i = 0; i < n; ++i) {
							if (l[i].suspension) {
								// La seule troncature : on retire des lettres jusqu'a ce que
								// « ... » tienne dans la largeur de la ligne.
								const float32 wl = W(l[i].d, pol, l[i].f);
								const float32 w3 = W("...", pol);
								const char *f = l[i].f;
								while (f > l[i].d && W(l[i].d, pol, f) + w3 > wl)
									--f;
								T(x, y + h * (float32)i, l[i].d, role, pol, f);
								T(x + W(l[i].d, pol, f), y + h * (float32)i, "...", role, pol);
							} else
								T(x, y + h * (float32)i, l[i].d, role, pol, l[i].f);
						}
						return h * (float32)n;
					}
			};

			// ── Un rectangle arrondi dont seuls les coins du HAUT sont ronds,
			//    rempli d'une image cadree « couvrir » (UV calcules). ───────────
			void ImageCouvrir(NkComponentPaint &p, const NkPaintRect &r, uint32 image, int32 iw, int32 ih,
							  float32 rayon, bool basRond) {
				float32 u0 = 0.f, u1 = 1.f, v0 = 0.f, v1 = 1.f;
				if (iw > 0 && ih > 0 && r.w > 0.f && r.h > 0.f) {
					const float32 ar = r.w / r.h, ai = (float32)iw / (float32)ih;
					if (ai > ar) {
						const float32 k = ar / ai;
						u0 = (1.f - k) * 0.5f;
						u1 = 1.f - u0;
					} else {
						const float32 k = ai / ar;
						v0 = (1.f - k) * 0.5f;
						v1 = 1.f - v0;
					}
				}
				enum { kArc = 6 };
				float32 xy[128], uv[128];
				int32 n = 0;
				auto pt = [&](float32 x, float32 y) {
					xy[n * 2] = x;
					xy[n * 2 + 1] = y;
					uv[n * 2] = u0 + (u1 - u0) * ((x - r.x) / r.w);
					uv[n * 2 + 1] = v0 + (v1 - v0) * ((y - r.y) / r.h);
					++n;
				};
				auto arc = [&](float32 cx, float32 cy, float32 a0) {
					for (int32 i = 0; i <= kArc; ++i) {
						const float32 a = a0 + 1.5707963f * (float32)i / (float32)kArc;
						pt(cx + rayon * math::NkCos(a), cy + rayon * math::NkSin(a));
					}
				};
				arc(r.x + rayon, r.y + rayon, 3.1415927f);			  // haut gauche
				arc(r.x + r.w - rayon, r.y + rayon, -1.5707963f);	  // haut droit
				if (basRond) {
					arc(r.x + r.w - rayon, r.y + r.h - rayon, 0.f);	  // bas droit
					arc(r.x + rayon, r.y + r.h - rayon, 1.5707963f); // bas gauche
				} else {
					pt(r.x + r.w, r.y + r.h);
					pt(r.x, r.y + r.h);
				}
				(void)p.ImagePolygone(xy, uv, n, image, 100.f);
			}

			/// Meme silhouette que `ImageCouvrir`, en aplat.
			void AplatHautRond(NkComponentPaint &p, const NkPaintRect &r, float32 rayon, uint32 c,
							   bool basRond) {
				if (basRond) {
					p.FillColor(r, c, rayon);
					return;
				}
				p.FillColor({r.x, r.y, r.w, r.h * 0.5f + rayon}, c, rayon);
				p.FillColor({r.x, r.y + rayon, r.w, r.h - rayon}, c, 0.f);
			}

			bool Dans(const NkPaintRect &r, const NkComponentInput &in) {
				return r.Contains(in.mouseX, in.mouseY);
			}

			/// Minuscule ASCII pour la recherche (sans accents : le filtre compare
			/// des octets ; « é » ne se plie pas, il se compare tel quel).
			inline char Min(char c) {
				return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
			}
			bool Contient(const NkString &meule, const char *aiguille) {
				if (!aiguille || !*aiguille)
					return true;
				const char *m = meule.CStr();
				for (usize i = 0; m[i]; ++i) {
					usize k = 0;
					while (aiguille[k] && m[i + k] && Min(m[i + k]) == Min(aiguille[k]))
						++k;
					if (!aiguille[k])
						return true;
				}
				return false;
			}
			bool InfNom(const NkString &a, const NkString &b) {
				const char *x = a.CStr(), *y = b.CStr();
				while (*x && *y && Min(*x) == Min(*y)) {
					++x;
					++y;
				}
				return Min(*x) < Min(*y);
			}

			/// Le DOSSIER d'un fichier projet (ce que la carte affiche).
			NkString Dossier(const NkString &chemin) {
				const char *s = chemin.CStr();
				int32 der = -1;
				for (int32 i = 0; s[i]; ++i)
					if (s[i] == '/' || s[i] == '\\')
						der = i;
				if (der <= 0)
					return chemin;
				return NkString(s, (NkString::SizeType)der);
			}

			/// Deux initiales, pour la tuile d'un projet sans apercu.
			void Initiales(const NkString &nom, char out[8]) {
				int32 n = 0;
				bool debut = true;
				for (const char *c = nom.CStr(); *c && n < 2; ++c) {
					const bool lettre = (*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9');
					if (lettre && debut) {
						out[n++] = (*c >= 'a' && *c <= 'z') ? (char)(*c - 32) : *c;
						debut = false;
					}
					if (*c == ' ' || *c == '_' || *c == '-')
						debut = true;
				}
				if (n == 0)
					out[n++] = '?';
				out[n] = 0;
			}

			// ── Un BOUTON (primaire = accent plein, secondaire = contour) ───────
			struct Bouton {
					NkPaintRect r;
					bool survol = false;
			};
			bool PeindreBouton(NkComponentPaint &p, Ecrivain &e, const NkComponentInput &in, const NkPaintRect &r,
							   const char *libelle, NkLanceurGlyphe g, bool primaire, uint32 acc,
							   const NkProjectLauncherStyle &s, float32 k, NkProjectLauncherResult &res) {
				const bool sur = Dans(r, in);
				const float32 ray = 6.f * k;
				if (primaire)
					p.FillColor(r, sur ? Mix(acc, kBlanc, 0.14f) : acc, ray);
				else
					p.OutlineColor(r, sur ? acc : p.ColorOf(s.bord),
								   sur ? p.ColorOf(s.carteSurvol) : p.ColorOf(s.carte), ray);
				const uint16 role = primaire ? s.texteSurAccent : s.texte;
				const uint32 cg = primaire ? p.ColorOf(s.texteSurAccent) : p.ColorOf(s.texte);
				const float32 gs = 16.f * k;
				const float32 tw = e.W(libelle, 0);
				const float32 total = (g != NkLanceurGlyphe::Aucun ? gs + 8.f * k : 0.f) + tw;
				float32 x = r.x + (r.w - total) * 0.5f;
				if (g != NkLanceurGlyphe::Aucun) {
					NkLanceurPeindreGlyphe(p, g, {x, r.y + (r.h - gs) * 0.5f, gs, gs}, cg);
					x += gs + 8.f * k;
				}
				e.T(x, r.y + (r.h - e.H(0)) * 0.5f, libelle, role, 0);
				if (sur)
					res.curseurMain = true;
				return sur && in.mousePressed;
			}

			/// Un bouton ROND a icone (epingler, retirer) pose sur une vignette.
			bool BoutonRond(NkComponentPaint &p, const NkComponentInput &in, float32 cx, float32 cy, float32 rr,
							NkLanceurGlyphe g, uint32 fond, uint32 encre, NkProjectLauncherResult &res) {
				const NkPaintRect r{cx - rr, cy - rr, rr * 2.f, rr * 2.f};
				const bool sur = Dans(r, in);
				Disque(p, cx, cy, rr, sur ? Mix(fond, kBlanc, 0.18f) : fond);
				const float32 gs = rr * 1.15f;
				NkLanceurPeindreGlyphe(p, g, {cx - gs * 0.5f, cy - gs * 0.5f, gs, gs}, encre);
				if (sur)
					res.curseurMain = true;
				return sur && in.mousePressed;
			}

		} // namespace

		// =====================================================================
		//  LES GLYPHES — dessines sur une grille de 24, mis a l'echelle de `r`
		// =====================================================================
		void NkLanceurPeindreGlyphe(NkComponentPaint &p, NkLanceurGlyphe g, const NkPaintRect &r, uint32 c) {
			const float32 side = r.w < r.h ? r.w : r.h;
			if (side <= 1.f || g == NkLanceurGlyphe::Aucun)
				return;
			const float32 u = side / 24.f;
			const float32 ox = r.x + (r.w - side) * 0.5f, oy = r.y + (r.h - side) * 0.5f;
			auto X = [&](float32 v) { return ox + v * u; };
			auto Y = [&](float32 v) { return oy + v * u; };
			const float32 ep = (1.9f * u) < 1.2f ? 1.2f : 1.9f * u;
			const uint32 c50 = Alpha(c, (int32)(((c & 0xFFu) * 50u) / 100u));
			const uint32 c30 = Alpha(c, (int32)(((c & 0xFFu) * 30u) / 100u));
			auto T = [&](float32 a, float32 b, float32 cc, float32 d, float32 e = 0.f) {
				Trait(p, X(a), Y(b), X(cc), Y(d), e > 0.f ? e * u : ep, c);
			};
			auto R = [&](float32 x, float32 y, float32 w, float32 h, float32 ro, uint32 col) {
				p.FillColor({X(x), Y(y), w * u, h * u}, col, ro * u);
			};
			auto P = [&](std::initializer_list<float32> pts, uint32 col) {
				float32 q[64];
				int32 n = 0;
				for (float32 v : pts) {
					q[n] = (n % 2 == 0) ? X(v) : Y(v);
					++n;
				}
				Poly(p, q, n / 2, col);
			};
			switch (g) {
				case NkLanceurGlyphe::Projets:
				case NkLanceurGlyphe::Grille:
					R(3.f, 3.f, 8.f, 8.f, 2.f, c);
					R(13.f, 3.f, 8.f, 8.f, 2.f, c);
					R(3.f, 13.f, 8.f, 8.f, 2.f, c);
					R(13.f, 13.f, 8.f, 8.f, 2.f, c50);
					break;
				case NkLanceurGlyphe::Liste:
					for (int32 i = 0; i < 3; ++i) {
						R(3.f, 4.5f + 6.f * (float32)i, 3.f, 3.f, 1.f, c);
						R(8.5f, 5.f + 6.f * (float32)i, 12.5f, 2.f, 1.f, c);
					}
					break;
				case NkLanceurGlyphe::Apprendre:
					P({2.f, 5.f, 11.f, 6.5f, 11.f, 20.f, 2.f, 18.5f}, c30);
					P({22.f, 5.f, 13.f, 6.5f, 13.f, 20.f, 22.f, 18.5f}, c30);
					T(2.f, 5.f, 11.f, 6.5f);
					T(2.f, 5.f, 2.f, 18.5f);
					T(2.f, 18.5f, 11.f, 20.f);
					T(22.f, 5.f, 13.f, 6.5f);
					T(22.f, 5.f, 22.f, 18.5f);
					T(22.f, 18.5f, 13.f, 20.f);
					T(12.f, 6.5f, 12.f, 20.f, 1.4f);
					break;
				case NkLanceurGlyphe::Communaute: {
					Disque(p, X(16.5f), Y(7.5f), 2.8f * u, c50);
					float32 q2[2 * 14];
					for (int32 i = 0; i <= 12; ++i) {
						const float32 a = 3.1415927f + 3.1415927f * (float32)i / 12.f;
						q2[i * 2] = X(16.5f + 5.f * math::NkCos(a));
						q2[i * 2 + 1] = Y(18.f + 6.f * math::NkSin(a));
					}
					Poly(p, q2, 13, c50);
					Disque(p, X(9.f), Y(8.5f), 3.4f * u, c);
					float32 q[2 * 14];
					for (int32 i = 0; i <= 12; ++i) {
						const float32 a = 3.1415927f + 3.1415927f * (float32)i / 12.f;
						q[i * 2] = X(9.f + 6.5f * math::NkCos(a));
						q[i * 2 + 1] = Y(21.f + 7.5f * math::NkSin(a));
					}
					Poly(p, q, 13, c);
					break;
				}
				case NkLanceurGlyphe::Installations:
					P({3.f, 13.f, 21.f, 13.f, 21.f, 20.5f, 3.f, 20.5f}, c30);
					T(3.f, 13.f, 3.f, 20.5f);
					T(3.f, 20.5f, 21.f, 20.5f);
					T(21.f, 20.5f, 21.f, 13.f);
					T(12.f, 2.5f, 12.f, 12.f, 2.2f);
					P({7.f, 9.f, 17.f, 9.f, 12.f, 15.f}, c);
					break;
				case NkLanceurGlyphe::Nouveau:
					T(12.f, 4.5f, 12.f, 19.5f, 2.4f);
					T(4.5f, 12.f, 19.5f, 12.f, 2.4f);
					break;
				case NkLanceurGlyphe::Ouvrir:
					P({2.f, 5.f, 9.f, 5.f, 11.f, 7.5f, 2.f, 7.5f}, c);
					R(2.f, 7.f, 19.f, 12.5f, 1.5f, c50);
					P({4.5f, 10.f, 23.f, 10.f, 20.5f, 19.5f, 2.f, 19.5f}, c);
					break;
				case NkLanceurGlyphe::Dossier:
					P({2.f, 5.f, 9.f, 5.f, 11.f, 7.5f, 2.f, 7.5f}, c);
					R(2.f, 7.f, 20.f, 13.f, 1.5f, c);
					break;
				case NkLanceurGlyphe::Recherche:
					Anneau(p, X(10.f), Y(10.f), 6.f * u, 2.f * u, c);
					T(14.5f, 14.5f, 20.5f, 20.5f, 2.6f);
					break;
				case NkLanceurGlyphe::Epingle:
					R(7.5f, 2.5f, 9.f, 3.f, 1.f, c);
					P({9.f, 5.f, 15.f, 5.f, 17.f, 13.f, 7.f, 13.f}, c);
					T(12.f, 13.f, 12.f, 21.5f, 1.8f);
					break;
				case NkLanceurGlyphe::Corbeille:
					T(4.f, 6.f, 20.f, 6.f, 2.f);
					R(9.f, 3.f, 6.f, 2.5f, 1.f, c);
					P({6.f, 8.f, 18.f, 8.f, 17.f, 20.5f, 7.f, 20.5f}, c30);
					T(6.f, 8.f, 7.f, 20.5f);
					T(7.f, 20.5f, 17.f, 20.5f);
					T(17.f, 20.5f, 18.f, 8.f);
					T(10.f, 11.f, 10.f, 17.5f, 1.5f);
					T(14.f, 11.f, 14.f, 17.5f, 1.5f);
					break;
				case NkLanceurGlyphe::Soleil:
					Disque(p, X(12.f), Y(12.f), 4.6f * u, c);
					for (int32 i = 0; i < 8; ++i) {
						const float32 a = 0.7853982f * (float32)i;
						Trait(p, X(12.f + 7.f * math::NkCos(a)), Y(12.f + 7.f * math::NkSin(a)),
							  X(12.f + 10.f * math::NkCos(a)), Y(12.f + 10.f * math::NkSin(a)), 2.f * u, c);
					}
					break;
				case NkLanceurGlyphe::Lune: {
					// Croissant = disque C1 prive du disque C2 : l'arc de C1 hors de C2,
					// puis l'arc de C2 dans C1 en sens inverse.
					const float32 c1x = 12.f, c1y = 12.f, r1 = 8.5f, c2x = 16.5f, c2y = 8.f, r2 = 7.f;
					float32 q[2 * 128];
					int32 n = 0;
					enum { kN = 48 };
					int32 debut = -1;
					for (int32 i = 0; i < kN; ++i) {
						const float32 a = 6.2831853f * (float32)i / (float32)kN;
						const float32 x = c1x + r1 * math::NkCos(a), y = c1y + r1 * math::NkSin(a);
						const bool dedans = (x - c2x) * (x - c2x) + (y - c2y) * (y - c2y) < r2 * r2;
						const float32 ap = 6.2831853f * (float32)((i + kN - 1) % kN) / (float32)kN;
						const float32 xp = c1x + r1 * math::NkCos(ap), yp = c1y + r1 * math::NkSin(ap);
						const bool dedansP = (xp - c2x) * (xp - c2x) + (yp - c2y) * (yp - c2y) < r2 * r2;
						if (!dedans && dedansP)
							debut = i;
					}
					if (debut < 0)
						debut = 0;
					for (int32 j = 0; j < kN; ++j) {
						const int32 i = (debut + j) % kN;
						const float32 a = 6.2831853f * (float32)i / (float32)kN;
						const float32 x = c1x + r1 * math::NkCos(a), y = c1y + r1 * math::NkSin(a);
						if ((x - c2x) * (x - c2x) + (y - c2y) * (y - c2y) < r2 * r2)
							break;
						q[n * 2] = X(x);
						q[n * 2 + 1] = Y(y);
						++n;
					}
					// Arc interieur : C2, du point le plus proche de la fin vers le debut.
					const float32 aFin = math::NkAtan2(q[(n - 1) * 2 + 1] / u - oy / u - c2y,
														 q[(n - 1) * 2] / u - ox / u - c2x);
					const float32 aDeb = math::NkAtan2(q[1] / u - oy / u - c2y, q[0] / u - ox / u - c2x);
					float32 da = aDeb - aFin;
					while (da > 0.f)
						da -= 6.2831853f;
					for (int32 i = 1; i < 16 && n < 127; ++i) {
						const float32 a = aFin + da * (float32)i / 16.f;
						q[n * 2] = X(c2x + r2 * math::NkCos(a));
						q[n * 2 + 1] = Y(c2y + r2 * math::NkSin(a));
						++n;
					}
					if (n >= 3)
						Poly(p, q, n, c);
					break;
				}
				case NkLanceurGlyphe::Lien:
					T(11.f, 7.f, 4.f, 7.f);
					T(4.f, 7.f, 4.f, 20.f);
					T(4.f, 20.f, 17.f, 20.f);
					T(17.f, 20.f, 17.f, 13.f);
					T(10.f, 14.f, 19.f, 5.f, 2.f);
					P({13.f, 4.f, 20.f, 4.f, 20.f, 11.f}, c);
					break;
				case NkLanceurGlyphe::Cube:
					P({12.f, 2.5f, 20.5f, 7.f, 12.f, 11.5f, 3.5f, 7.f}, c);
					P({3.5f, 7.f, 12.f, 11.5f, 12.f, 21.5f, 3.5f, 17.f}, c50);
					P({12.f, 11.5f, 20.5f, 7.f, 20.5f, 17.f, 12.f, 21.5f}, c30);
					break;
				case NkLanceurGlyphe::Sphere:
					Disque(p, X(12.f), Y(12.f), 9.f * u, c50);
					Disque(p, X(10.5f), Y(10.5f), 6.5f * u, Alpha(c, (int32)((c & 0xFFu) * 75u / 100u)));
					Disque(p, X(9.f), Y(9.f), 2.6f * u, Mix(c, kBlanc, 0.55f));
					break;
				case NkLanceurGlyphe::Personnage:
					Disque(p, X(12.f), Y(5.5f), 3.f * u, c);
					T(12.f, 9.f, 12.f, 15.5f, 2.6f);
					T(5.5f, 11.f, 18.5f, 11.f, 2.2f);
					T(12.f, 15.f, 8.f, 21.5f, 2.4f);
					T(12.f, 15.f, 16.f, 21.5f, 2.4f);
					break;
				case NkLanceurGlyphe::Os:
					T(7.f, 17.f, 17.f, 7.f, 3.2f);
					Disque(p, X(5.f), Y(16.5f), 2.5f * u, c);
					Disque(p, X(7.5f), Y(19.f), 2.5f * u, c);
					Disque(p, X(16.5f), Y(5.f), 2.5f * u, c);
					Disque(p, X(19.f), Y(7.5f), 2.5f * u, c);
					break;
				case NkLanceurGlyphe::Coeur:
					Disque(p, X(8.3f), Y(9.f), 4.7f * u, c);
					Disque(p, X(15.7f), Y(9.f), 4.7f * u, c);
					P({3.75f, 10.5f, 20.25f, 10.5f, 12.f, 20.5f}, c);
					break;
				case NkLanceurGlyphe::Calques:
					P({12.f, 11.f, 21.f, 15.5f, 12.f, 20.f, 3.f, 15.5f}, c30);
					P({12.f, 7.5f, 21.f, 12.f, 12.f, 16.5f, 3.f, 12.f}, c50);
					P({12.f, 4.f, 21.f, 8.5f, 12.f, 13.f, 3.f, 8.5f}, c);
					break;
				case NkLanceurGlyphe::Code:
					T(8.f, 7.f, 3.f, 12.f, 2.2f);
					T(3.f, 12.f, 8.f, 17.f, 2.2f);
					T(16.f, 7.f, 21.f, 12.f, 2.2f);
					T(21.f, 12.f, 16.f, 17.f, 2.2f);
					T(14.f, 5.f, 10.f, 19.f, 2.f);
					break;
				case NkLanceurGlyphe::Manette:
					R(2.f, 7.5f, 20.f, 10.5f, 5.f, c50);
					T(5.f, 12.75f, 10.f, 12.75f, 2.f);
					T(7.5f, 10.25f, 7.5f, 15.25f, 2.f);
					Disque(p, X(15.5f), Y(11.5f), 1.5f * u, c);
					Disque(p, X(18.f), Y(14.f), 1.5f * u, c);
					break;
				case NkLanceurGlyphe::Paysage:
					P({2.f, 20.f, 9.f, 8.f, 16.f, 20.f}, c);
					P({11.f, 20.f, 16.f, 11.5f, 22.f, 20.f}, c50);
					Disque(p, X(17.f), Y(6.f), 2.4f * u, c);
					break;
				case NkLanceurGlyphe::Camera:
					R(2.f, 7.f, 14.f, 11.f, 2.f, c);
					P({16.5f, 11.f, 22.f, 7.5f, 22.f, 17.5f, 16.5f, 14.f}, c50);
					break;
				case NkLanceurGlyphe::Ampoule:
					Disque(p, X(12.f), Y(10.f), 6.5f * u, c);
					R(9.f, 15.5f, 6.f, 2.2f, 0.8f, c50);
					R(9.5f, 18.5f, 5.f, 2.2f, 0.8f, c50);
					break;
				case NkLanceurGlyphe::Vide:
					for (int32 cote = 0; cote < 4; ++cote)
						for (int32 i = 0; i < 4; ++i) {
							const float32 a = 4.f + 4.25f * (float32)i, b = a + 2.4f;
							if (cote == 0)
								T(a, 4.f, b, 4.f, 1.6f);
							else if (cote == 1)
								T(20.f, a, 20.f, b, 1.6f);
							else if (cote == 2)
								T(a, 20.f, b, 20.f, 1.6f);
							else
								T(4.f, a, 4.f, b, 1.6f);
						}
					T(12.f, 9.f, 12.f, 15.f, 1.8f);
					T(9.f, 12.f, 15.f, 12.f, 1.8f);
					break;
				case NkLanceurGlyphe::Horloge:
					Anneau(p, X(12.f), Y(12.f), 8.5f * u, 1.8f * u, c);
					T(12.f, 12.f, 12.f, 6.5f, 1.8f);
					T(12.f, 12.f, 16.f, 14.f, 1.8f);
					break;
				case NkLanceurGlyphe::Document:
					P({5.f, 3.f, 14.5f, 3.f, 19.f, 7.5f, 19.f, 21.f, 5.f, 21.f}, c30);
					T(5.f, 3.f, 14.5f, 3.f);
					T(14.5f, 3.f, 19.f, 7.5f);
					T(19.f, 7.5f, 19.f, 21.f);
					T(19.f, 21.f, 5.f, 21.f);
					T(5.f, 21.f, 5.f, 3.f);
					T(8.f, 11.f, 16.f, 11.f, 1.5f);
					T(8.f, 14.5f, 16.f, 14.5f, 1.5f);
					T(8.f, 18.f, 13.f, 18.f, 1.5f);
					break;
				case NkLanceurGlyphe::Reglages:
				case NkLanceurGlyphe::Moteur:
					for (int32 i = 0; i < 8; ++i) {
						const float32 a = 0.7853982f * (float32)i;
						Trait(p, X(12.f + 5.5f * math::NkCos(a)), Y(12.f + 5.5f * math::NkSin(a)),
							  X(12.f + 10.f * math::NkCos(a)), Y(12.f + 10.f * math::NkSin(a)), 3.f * u, c);
					}
					Anneau(p, X(12.f), Y(12.f), 6.f * u, 3.f * u, c);
					if (g == NkLanceurGlyphe::Moteur)
						P({13.f, 8.f, 9.5f, 13.f, 12.f, 13.f, 11.f, 16.5f, 14.5f, 11.f, 12.f, 11.f}, c);
					break;
				case NkLanceurGlyphe::Etoile: {
					float32 q[20];
					for (int32 i = 0; i < 10; ++i) {
						const float32 a = -1.5707963f + 0.6283185f * (float32)i;
						const float32 rr = (i % 2 == 0) ? 10.f : 4.2f;
						q[i * 2] = X(12.f + rr * math::NkCos(a));
						q[i * 2 + 1] = Y(12.5f + rr * math::NkSin(a));
					}
					Poly(p, q, 10, c);
					break;
				}
				case NkLanceurGlyphe::Croix:
					T(6.f, 6.f, 18.f, 18.f, 1.8f);
					T(18.f, 6.f, 6.f, 18.f, 1.8f);
					break;
				case NkLanceurGlyphe::Moins:
					T(6.f, 12.f, 18.f, 12.f, 1.8f);
					break;
				case NkLanceurGlyphe::Carre:
					T(6.f, 6.f, 18.f, 6.f, 1.6f);
					T(18.f, 6.f, 18.f, 18.f, 1.6f);
					T(18.f, 18.f, 6.f, 18.f, 1.6f);
					T(6.f, 18.f, 6.f, 6.f, 1.6f);
					break;
				case NkLanceurGlyphe::Pinceau:
					T(20.f, 4.f, 11.f, 13.f, 2.8f);
					Disque(p, X(8.f), Y(16.f), 3.8f * u, c);
					P({4.2f, 17.f, 7.f, 19.5f, 3.f, 21.f}, c);
					break;
				default:
					break;
			}
		}

		// =====================================================================
		//  LE LANCEUR
		// =====================================================================
		NkProjectLauncherResult NkDrawProjectLauncher(NkComponentPaint &p, const NkComponentInput &in,
													  const NkPaintRect &r, NkProjectLauncherModel &m,
													  const NkProjectLauncherStyle &s,
													  const NkProjectLauncherHooks &h) {
			NkProjectLauncherResult res;
			if (r.w < 200.f || r.h < 160.f)
				return res;
			const float32 k = in.surfaceScale > 0.f ? in.surfaceScale : 1.f;
			auto S = [&](float32 v) { return v * k; };
			Ecrivain e{p};
			const uint8 pT = s.policeTitre, pI = s.policeIntertitre, pG = s.policeGrasse, pP = s.policePetite;

			const uint32 acc = m.identite.accent ? m.identite.accent : p.ColorOf(s.accent);
			const uint32 cFond = p.ColorOf(s.fond), cCol = p.ColorOf(s.colonne), cCarte = p.ColorOf(s.carte);
			const uint32 cBord = p.ColorOf(s.bord), cTexte = p.ColorOf(s.texte);
			const uint32 cDiscret = p.ColorOf(s.texteDiscret), cSurAcc = p.ColorOf(s.texteSurAccent);
			const uint32 cSurvol = p.ColorOf(s.carteSurvol), cChamp = p.ColorOf(s.champ);

			// Les pages : sans page declaree, une seule page « Projets ».
			NkLanceurPage pageDefaut;
			pageDefaut.libelle = NkString("Projets");
			const int32 nPages = m.pages.Empty() ? 1 : (int32)m.pages.Size();
			if (m.page < 0 || m.page >= nPages)
				m.page = 0;
			const NkLanceurPage &page = m.pages.Empty() ? pageDefaut : m.pages[(usize)m.page];
			const bool pageProjets = page.type == NkLanceurPageType::Projets;

			// Le clic de cette image, s'il tombe hors de la recherche, la quitte.
			bool clicRecherche = false;

			p.Fill(r, s.fond);

			// ── BARRE DE TITRE (fenetres sans cadre) ────────────────────────────
			float32 haut = r.y;
			if (m.chromeFenetre) {
				const float32 bh = S(34.f);
				const NkPaintRect barre{r.x, r.y, r.w, bh};
				p.FillColor(barre, cCol, 0.f);
				const float32 bw = S(46.f);
				const NkLanceurGlyphe gl[3] = {NkLanceurGlyphe::Moins, NkLanceurGlyphe::Carre, NkLanceurGlyphe::Croix};
				const NkLanceurAction ac[3] = {NkLanceurAction::FenetreReduire, NkLanceurAction::FenetreAgrandir,
												NkLanceurAction::FenetreFermer};
				for (int32 i = 0; i < 3; ++i) {
					const NkPaintRect b{r.x + r.w - bw * (float32)(3 - i), r.y, bw, bh};
					const bool sur = Dans(b, in);
					if (sur)
						p.FillColor(b, i == 2 ? p.ColorOf(s.erreur) : cSurvol, 0.f);
					const float32 gs = S(15.f);
					NkLanceurPeindreGlyphe(p, gl[i], {b.x + (bw - gs) * 0.5f, b.y + (bh - gs) * 0.5f, gs, gs},
										   (sur && i == 2) ? kBlanc : cTexte);
					if (sur && in.mousePressed)
						res.action = ac[i];
				}
				res.barreTitre = {r.x, r.y, r.w - bw * 3.f, bh};
				if (Dans(res.barreTitre, in) && in.mousePressed && res.action == NkLanceurAction::Aucune)
					res.action = NkLanceurAction::FenetreGlisser;
				haut += bh;
				p.HLine(r.x, haut - 1.f, r.w, s.bord);
			}
			const float32 bas = r.y + r.h;

			// ── COLONNE DE NAVIGATION ───────────────────────────────────────────
			float32 colW = r.w * 0.17f;
			if (colW < S(220.f))
				colW = S(220.f);
			if (colW > S(264.f))
				colW = S(264.f);
			{
				const NkPaintRect col{r.x, haut, colW, bas - haut};
				p.FillColor(col, cCol, 0.f);
				p.VLine(r.x + colW - 1.f, haut, bas - haut, s.bord);

				// La marque : le logo, le nom (prefixe a l'accent), le sous-titre.
				const float32 lx = r.x + S(20.f), ly = haut + S(24.f), ls = S(44.f);
				const NkPaintRect logo{lx, ly, ls, ls};
				if (h.peindreLogo)
					h.peindreLogo(h.user, p, logo);
				else if (m.identite.logoImage != 0u) {
					const float32 xy[8] = {logo.x, logo.y, logo.x + ls, logo.y, logo.x + ls, logo.y + ls, logo.x, logo.y + ls};
					const float32 uv[8] = {0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f};
					(void)p.ImagePolygone(xy, uv, 4, m.identite.logoImage, 100.f);
				} else {
					p.FillColor(logo, acc, ls * 0.24f);
					const float32 gi = ls * 0.58f;
					NkLanceurPeindreGlyphe(p, m.identite.glyphe, {lx + (ls - gi) * 0.5f, ly + (ls - gi) * 0.5f, gi, gi},
										   cSurAcc);
				}
				const float32 nx = lx + ls + S(12.f);
				const float32 nw = r.x + colW - S(14.f) - nx;
				float32 ny = ly + (ls - e.H(pI) - e.H(pP) - S(2.f)) * 0.5f;
				if (ny < ly)
					ny = ly;
				const char *nom = m.identite.nom.CStr();
				const usize np = m.identite.prefixe.Size();
				if (np > 0u && m.identite.nom.Size() >= np) {
					e.T(nx, ny, nom, s.accent, pI, nom + np);
					e.T(nx + e.W(nom, pI, nom + np), ny, nom + np, s.texte, pI);
				} else
					e.T(nx, ny, nom, s.texte, pI);
				Ligne ls2[2];
				const int32 nl = Couper(p, m.identite.sousTitre.CStr(), nw, pP, ls2, 2, false);
				e.Lignes(nx, ny + e.H(pI) + S(2.f), ls2, nl, s.texteDiscret, pP, S(1.f));

				// Les entrees de navigation.
				float32 y = ly + ls + S(32.f);
				const float32 ih = S(40.f);
				for (int32 i = 0; i < nPages; ++i) {
					const NkLanceurPage &pg = m.pages.Empty() ? pageDefaut : m.pages[(usize)i];
					const NkPaintRect it{r.x + S(12.f), y, colW - S(24.f), ih};
					const bool actif = (i == m.page);
					const bool sur = Dans(it, in);
					if (actif) {
						p.FillColor(it, Alpha(acc, 0x38), S(7.f));
						p.FillColor({it.x, it.y + S(9.f), S(3.f), ih - S(18.f)}, acc, S(1.5f));
					} else if (sur)
						p.FillColor(it, cSurvol, S(7.f));
					const float32 gs = S(18.f);
					NkLanceurPeindreGlyphe(p, pg.glyphe, {it.x + S(14.f), it.y + (ih - gs) * 0.5f, gs, gs},
										   actif ? acc : (sur ? cTexte : cDiscret));
					e.T(it.x + S(44.f), it.y + (ih - e.H(actif ? pG : 0)) * 0.5f, pg.libelle.CStr(),
						actif || sur ? s.texte : s.texteDiscret, actif ? pG : 0);
					if (sur) {
						res.curseurMain = true;
						if (in.mousePressed && !actif) {
							m.page = i;
							m.defilement = 0.f;
						}
					}
					y += ih + S(4.f);
				}

				// Le pied de colonne : le theme, la version.
				float32 yb = bas - S(18.f) - e.H(pP);
				if (!m.identite.version.Empty()) {
					NkString v("Version ");
					v += m.identite.version;
					e.T(r.x + S(24.f), yb, v.CStr(), s.texteDiscret, pP);
					yb -= S(12.f);
				}
				if (m.themeBasculable) {
					const NkPaintRect it{r.x + S(12.f), yb - ih, colW - S(24.f), ih};
					const bool sur = Dans(it, in);
					if (sur)
						p.FillColor(it, cSurvol, S(7.f));
					const float32 gs = S(18.f);
					NkLanceurPeindreGlyphe(p, m.themeSombre ? NkLanceurGlyphe::Soleil : NkLanceurGlyphe::Lune,
										   {it.x + S(14.f), it.y + (ih - gs) * 0.5f, gs, gs}, sur ? cTexte : cDiscret);
					e.T(it.x + S(44.f), it.y + (ih - e.H(0)) * 0.5f,
						m.themeSombre ? "Passer en theme clair" : "Passer en theme sombre", sur ? s.texte : s.texteDiscret, 0);
					if (sur) {
						res.curseurMain = true;
						if (in.mousePressed)
							res.action = NkLanceurAction::BasculerTheme;
					}
					p.HLine(r.x + S(20.f), it.y - S(10.f), colW - S(40.f), s.bord);
				}
			}

			// ── ZONE PRINCIPALE ─────────────────────────────────────────────────
			const float32 mx = r.x + colW, mw = r.w - colW;
			float32 pad = S(36.f);
			if (mw < S(760.f))
				pad = S(22.f);
			const float32 cx = mx + pad, cw = mw - pad * 2.f;

			// En-tete : titre, sous-titre, boutons.
			float32 y = haut + S(26.f);
			const char *titre = page.titre.Empty() ? page.libelle.CStr() : page.titre.CStr();
			NkString sousTitre = page.sousTitre;
			if (sousTitre.Empty() && pageProjets) {
				char b[200];
				const int32 n = (int32)m.projets.Size();
				if (!m.identite.extensions.Empty())
					snprintf(b, sizeof(b), "%d %s recent%s  ·  fichiers %s", (int)n,
							 n > 1 ? m.identite.motProjets.CStr() : m.identite.motProjet.CStr(), n > 1 ? "s" : "",
							 m.identite.extensions.CStr());
				else
					snprintf(b, sizeof(b), "%d %s recent%s", (int)n,
							 n > 1 ? m.identite.motProjets.CStr() : m.identite.motProjet.CStr(), n > 1 ? "s" : "");
				sousTitre = NkString(b);
			}
			float32 droiteBoutons = cx + cw;
			if (pageProjets) {
				const float32 bh = S(38.f);
				char lib[96];
				snprintf(lib, sizeof(lib), "Nouveau %s", m.identite.motProjet.CStr());
				const float32 w1 = e.W(lib, 0) + S(56.f);
				const NkPaintRect b1{cx + cw - w1, y + S(4.f), w1, bh};
				if (PeindreBouton(p, e, in, b1, lib, NkLanceurGlyphe::Nouveau, true, acc, s, k, res))
					res.action = NkLanceurAction::NouveauProjet;
				droiteBoutons = b1.x;
				if (m.ouvrirPossible) {
					const char *lo = "Ouvrir...";
					const float32 w2 = e.W(lo, 0) + S(56.f);
					const NkPaintRect b2{b1.x - S(10.f) - w2, b1.y, w2, bh};
					if (PeindreBouton(p, e, in, b2, lo, NkLanceurGlyphe::Ouvrir, false, acc, s, k, res))
						res.action = NkLanceurAction::Ouvrir;
					droiteBoutons = b2.x;
				}
			}
			{
				// Le titre n'empiete jamais sur les boutons : il passe a la ligne.
				Ligne lt[2];
				const int32 n = Couper(p, titre, droiteBoutons - S(16.f) - cx, pT, lt, 2, false);
				const float32 ht = e.Lignes(cx, y, lt, n, s.texte, pT, 0.f);
				y += ht + S(4.f);
				Ligne lst[2];
				const int32 n2 = Couper(p, sousTitre.CStr(), droiteBoutons - S(16.f) - cx, pP, lst, 2, false);
				y += e.Lignes(cx, y, lst, n2, s.texteDiscret, pP, S(1.f));
				if (y < haut + S(26.f) + S(46.f))
					y = haut + S(26.f) + S(46.f);
			}
			y += S(18.f);

			// Barre de recherche et vues (page Projets).
			if (pageProjets) {
				const float32 bh = S(36.f);
				float32 rw = cw * 0.46f;
				if (rw > S(460.f))
					rw = S(460.f);
				if (rw < S(220.f))
					rw = S(220.f);
				const NkPaintRect boite{cx, y, rw, bh};
				const bool sur = Dans(boite, in);
				p.OutlineColor(boite, m.rechercheFocus ? acc : (sur ? Mix(cBord, cTexte, 0.25f) : cBord), cChamp,
							   bh * 0.5f);
				const float32 gs = S(16.f);
				NkLanceurPeindreGlyphe(p, NkLanceurGlyphe::Recherche, {boite.x + S(14.f), y + (bh - gs) * 0.5f, gs, gs},
									   cDiscret);
				const bool aTexte = m.filtre[0] != 0;
				const float32 effW = aTexte ? S(30.f) : 0.f;
				res.recherche = {boite.x + S(40.f), y + S(4.f), rw - S(40.f) - S(14.f) - effW, bh - S(8.f)};
				if (!m.rechercheFocus) {
					if (aTexte)
						e.T(res.recherche.x, y + (bh - e.H(0)) * 0.5f, m.filtre, s.texte, 0);
					else {
						char ph[96];
						snprintf(ph, sizeof(ph), "Rechercher un %s", m.identite.motProjet.CStr());
						e.T(res.recherche.x, y + (bh - e.H(0)) * 0.5f, ph, s.texteDiscret, 0);
					}
				}
				if (aTexte) {
					const NkPaintRect ef{boite.x + rw - S(36.f), y + S(6.f), S(24.f), bh - S(12.f)};
					const bool se = Dans(ef, in);
					const float32 g2 = S(12.f);
					NkLanceurPeindreGlyphe(p, NkLanceurGlyphe::Croix, {ef.x + (ef.w - g2) * 0.5f, ef.y + (ef.h - g2) * 0.5f, g2, g2},
										   se ? cTexte : cDiscret);
					if (se) {
						res.curseurMain = true;
						if (in.mousePressed) {
							m.filtre[0] = 0;
							m.defilement = 0.f;
							clicRecherche = true;
						}
					}
				}
				if (sur && in.mousePressed && !clicRecherche) {
					m.rechercheFocus = true;
					clicRecherche = true;
				}

				// A droite : grille | liste, puis le tri.
				const float32 sw = S(36.f);
				float32 xd = cx + cw;
				{
					const char *lt = m.tri == 0u ? "Tri : recents" : "Tri : nom";
					const float32 tw = e.W(lt, 0) + S(28.f);
					const NkPaintRect bt{xd - tw, y, tw, bh};
					const bool st = Dans(bt, in);
					p.OutlineColor(bt, st ? acc : cBord, st ? cSurvol : cCarte, S(6.f));
					e.T(bt.x + S(14.f), y + (bh - e.H(0)) * 0.5f, lt, s.texte, 0);
					if (st) {
						res.curseurMain = true;
						if (in.mousePressed)
							m.tri = (uint8)(m.tri == 0u ? 1u : 0u);
					}
					xd = bt.x - S(10.f);
				}
				const NkPaintRect seg{xd - sw * 2.f, y, sw * 2.f, bh};
				p.OutlineColor(seg, cBord, cCarte, S(6.f));
				for (int32 i = 0; i < 2; ++i) {
					const NkPaintRect b{seg.x + sw * (float32)i, y, sw, bh};
					const bool actif = (i == 1) == m.vueListe;
					const bool sb = Dans(b, in);
					if (actif)
						p.FillColor({b.x + S(3.f), b.y + S(3.f), b.w - S(6.f), b.h - S(6.f)}, Alpha(acc, 0x40), S(4.f));
					const float32 g2 = S(16.f);
					NkLanceurPeindreGlyphe(p, i == 0 ? NkLanceurGlyphe::Grille : NkLanceurGlyphe::Liste,
										   {b.x + (sw - g2) * 0.5f, y + (bh - g2) * 0.5f, g2, g2},
										   actif ? acc : (sb ? cTexte : cDiscret));
					if (sb) {
						res.curseurMain = true;
						if (in.mousePressed)
							m.vueListe = (i == 1);
					}
				}
				y += bh + S(20.f);
			}
			if (in.mousePressed && !clicRecherche)
				m.rechercheFocus = false;

			// ── LE PIED DE PAGE ─────────────────────────────────────────────────
			const float32 piedH = S(40.f);
			const float32 piedY = bas - piedH;
			{
				p.HLine(mx, piedY, mw, s.bord);
				const float32 ty = piedY + (piedH - e.H(pP)) * 0.5f;
				const bool err = !m.erreur.Empty();
				const char *gauche = err ? m.erreur.CStr() : m.piedDePage.CStr();
				float32 wg = e.W(gauche, pP);
				float32 maxG = cw;
				if (!m.astuce.Empty()) {
					const float32 wa = e.W(m.astuce.CStr(), pP);
					// L'astuce ne s'ecrit que si elle tient A COTE du texte de gauche :
					// une erreur se lit en entier, l'astuce peut attendre.
					if (wg + S(32.f) + wa <= cw)
						e.T(cx + cw - wa, ty, m.astuce.CStr(), s.texteDiscret, pP);
				}
				if (wg > maxG)
					wg = maxG;
				if (gauche && *gauche) {
					Ligne lg[1];
					const int32 n = Couper(p, gauche, maxG, pP, lg, 1, false);
					e.Lignes(cx, ty, lg, n, err ? s.erreur : s.texteDiscret, pP, 0.f);
				}
			}

			// ── LA ZONE QUI DEFILE ──────────────────────────────────────────────
			const NkPaintRect zone{mx, y, mw, piedY - y};
			if (zone.h < S(40.f))
				return res;
			const bool dansZone = Dans(zone, in);
			p.PushClip(zone);
			float32 cy = y + S(4.f) - m.defilement;
			const float32 debutContenu = cy;

			auto TitreSection = [&](const char *t, int32 compte) {
				e.T(cx, cy, t, s.texte, pI);
				if (compte >= 0) {
					char b[24];
					snprintf(b, sizeof(b), "%d", (int)compte);
					const float32 x0 = cx + e.W(t, pI) + S(10.f);
					const float32 bw = e.W(b, pP) + S(16.f), bh = e.H(pP) + S(4.f);
					const NkPaintRect pill{x0, cy + (e.H(pI) - bh) * 0.5f, bw, bh};
					p.FillColor(pill, Alpha(acc, 0x40), bh * 0.5f);
					e.TC(pill.x, pill.w, pill.y + S(2.f), b, s.texte, pP);
				}
			};

			if (pageProjets) {
				const bool filtreActif = m.filtre[0] != 0;

				// ── 1. La bande de version ──────────────────────────────────────
				if (m.banniere.image != 0u && !filtreActif) {
					float32 bh = cw * (float32)m.banniere.h / (float32)(m.banniere.w > 0 ? m.banniere.w : 1);
					if (bh > S(210.f))
						bh = S(210.f);
					if (bh >= S(90.f)) {
						const NkPaintRect br{cx, cy, cw, bh};
						ImageCouvrir(p, br, m.banniere.image, m.banniere.w, m.banniere.h, S(12.f), true);
						// Un voile en degrade au bas de l'image : la legende se lit
						// sur n'importe quelle photo.
						for (int32 i = 0; i < 6; ++i) {
							const float32 t = (float32)i / 6.f;
							const float32 yy = br.y + bh * (0.55f + 0.45f * t);
							const float32 hh = bh * 0.45f / 6.f + 1.f;
							if (i == 5)
								p.FillColor({br.x, yy, br.w, br.y + bh - yy}, Alpha(kNoir, 40 + 22 * i), S(12.f));
							else
								p.FillColor({br.x, yy, br.w, hh}, Alpha(kNoir, 40 + 22 * i), 0.f);
						}
						const float32 ly = br.y + bh - S(16.f) - e.H(pG);
						e.T(br.x + S(18.f), ly, m.banniere.legende.CStr(), s.texteSurAccent, pG);
						if (!m.banniere.credit.Empty()) {
							const float32 wc = e.W(m.banniere.credit.CStr(), pP);
							e.T(br.x + br.w - S(18.f) - wc, ly + (e.H(pG) - e.H(pP)) * 0.5f, m.banniere.credit.CStr(),
								s.texteSurAccent, pP);
						}
						cy += bh + S(28.f);
					}
				}

				// ── 2. Les modeles ──────────────────────────────────────────────
				const int32 nMod = (int32)m.modeles.Size();
				if (nMod > 0 && !filtreActif) {
					char t[96];
					snprintf(t, sizeof(t), "Commencer un nouveau %s", m.identite.motProjet.CStr());
					TitreSection(t, -1);
					cy += e.H(pI) + S(14.f);
					const float32 gap = S(16.f);
					int32 cols = (int32)((cw + gap) / (S(196.f) + gap));
					if (cols < 1)
						cols = 1;
					if (cols > 6)
						cols = 6;
					if (cols > nMod)
						cols = nMod < 3 ? 3 : nMod; // peu de modeles : des cartes de taille sage
					const float32 tw = (cw - gap * (float32)(cols - 1)) / (float32)cols;
					const float32 ih = tw * 0.52f;
					const float32 ipad = S(14.f);
					for (int32 debut = 0; debut < nMod; debut += cols) {
						// Hauteur de la RANGEE : celle de la carte la plus haute.
						int32 maxNom = 1, maxDesc = 0;
						Ligne ln[3], ld[4];
						for (int32 i = debut; i < debut + cols && i < nMod; ++i) {
							const NkLanceurModele &md = m.modeles[(usize)i];
							const int32 a = Couper(p, md.nom.CStr(), tw - ipad * 2.f, pG, ln, 2, false);
							const int32 b = Couper(p, md.description.CStr(), tw - ipad * 2.f, pP, ld, 4, true);
							if (a > maxNom)
								maxNom = a;
							if (b > maxDesc)
								maxDesc = b;
						}
						const float32 th = ih + ipad + e.H(pP) + S(4.f) + (e.H(pG) + S(1.f)) * (float32)maxNom + S(6.f) +
										   (e.H(pP) + S(2.f)) * (float32)maxDesc + ipad;
						for (int32 i = debut; i < debut + cols && i < nMod; ++i) {
							const NkLanceurModele &md = m.modeles[(usize)i];
							const NkPaintRect cr{cx + (tw + gap) * (float32)(i - debut), cy, tw, th};
							if (cr.y > zone.y + zone.h || cr.y + cr.h < zone.y)
								continue;
							const bool sur = dansZone && Dans(cr, in) && md.disponible;
							const uint32 teinte = md.couleur ? md.couleur : acc;
							p.FillColor({cr.x, cr.y + S(3.f), cr.w, cr.h}, Alpha(kNoir, 0x26), S(10.f));
							p.OutlineColor(cr, sur ? teinte : cBord, sur ? cSurvol : cCarte, S(10.f));
							// L'illustration : un degrade de la teinte, un sol en perspective,
							// le glyphe au centre.
							const NkPaintRect ir{cr.x + 1.f, cr.y + 1.f, cr.w - 2.f, ih};
							if (md.image != 0u)
								ImageCouvrir(p, ir, md.image, md.imageW, md.imageH, S(9.f), false);
							else {
								p.PushClip(ir); // le sol en perspective ne deborde jamais de l'illustration
								AplatHautRond(p, ir, S(9.f), Mix(cCarte, teinte, 0.20f), false);
								p.FillColor({ir.x, ir.y + ir.h * 0.55f, ir.w, ir.h * 0.45f}, Mix(cCarte, teinte, 0.30f), 0.f);
								const float32 hz = ir.y + ir.h * 0.55f;
								for (int32 l = -4; l <= 4; ++l)
									Trait(p, ir.x + ir.w * 0.5f + (float32)l * ir.w * 0.06f, hz,
										  ir.x + ir.w * 0.5f + (float32)l * ir.w * 0.22f, ir.y + ir.h, 1.f,
										  Mix(cCarte, teinte, 0.42f));
								for (int32 l = 1; l <= 3; ++l) {
									const float32 yy = hz + ir.h * 0.45f * ((float32)(l * l) / 9.f);
									Trait(p, ir.x, yy, ir.x + ir.w, yy, 1.f, Mix(cCarte, teinte, 0.42f));
								}
								const float32 gs = ih * 0.46f;
								Disque(p, ir.x + ir.w * 0.5f, ir.y + ir.h * 0.47f, gs * 0.78f, Alpha(teinte, 0x30));
								NkLanceurPeindreGlyphe(p, md.glyphe,
													   {ir.x + (ir.w - gs) * 0.5f, ir.y + ir.h * 0.47f - gs * 0.5f, gs, gs},
													   Mix(teinte, kBlanc, 0.15f));
								p.PopClip();
							}
							float32 ty = cr.y + ih + ipad;
							if (!md.categorie.Empty())
								e.T(cr.x + ipad, ty, md.categorie.CStr(), s.accent, pP);
							ty += e.H(pP) + S(4.f);
							const int32 a = Couper(p, md.nom.CStr(), tw - ipad * 2.f, pG, ln, 2, false);
							ty += e.Lignes(cr.x + ipad, ty, ln, a, s.texte, pG, S(1.f)) + S(6.f) +
								  (e.H(pG) + S(1.f)) * (float32)(maxNom - a);
							const int32 b = Couper(p, md.description.CStr(), tw - ipad * 2.f, pP, ld, 4, true);
							e.Lignes(cr.x + ipad, ty, ld, b, s.texteDiscret, pP, S(2.f));
							if (!md.disponible) {
								p.FillColor(cr, Alpha(cFond, 0x90), S(10.f));
								const char *rs = md.raison.Empty() ? "a venir" : md.raison.CStr();
								const float32 bw = e.W(rs, pP) + S(16.f), bh = e.H(pP) + S(6.f);
								p.FillColor({cr.x + cr.w - bw - S(10.f), cr.y + S(10.f), bw, bh}, cCol, bh * 0.5f);
								e.TC(cr.x + cr.w - bw - S(10.f), bw, cr.y + S(13.f), rs, s.texteDiscret, pP);
							}
							if (sur) {
								res.curseurMain = true;
								if (in.mousePressed) {
									res.action = NkLanceurAction::NouveauDepuisModele;
									res.index = i;
								}
							}
						}
						cy += th + gap;
					}
					cy += S(18.f);
				}

				// ── 3. Les projets recents ──────────────────────────────────────
				NkVector<int32> vis;
				int32 morts = 0;
				for (usize i = 0; i < m.projets.Size(); ++i) {
					const NkLanceurProjet &pr = m.projets[i];
					if (pr.etat != 0u)
						++morts;
					if (Contient(pr.nom, m.filtre) || Contient(pr.chemin, m.filtre))
						vis.PushBack((int32)i);
				}
				if (m.tri == 1u) // par nom : un tri par insertion suffit a une liste de recents
					for (usize i = 1; i < vis.Size(); ++i)
						for (usize j = i; j > 0 && InfNom(m.projets[(usize)vis[j]].nom, m.projets[(usize)vis[j - 1]].nom); --j) {
							const int32 t = vis[j];
							vis[j] = vis[j - 1];
							vis[j - 1] = t;
						}
				res.projetsVisibles = (int32)vis.Size();

				char tr[96];
				snprintf(tr, sizeof(tr), "%s recents", m.identite.motProjets.CStr());
				if (tr[0] >= 'a' && tr[0] <= 'z')
					tr[0] = (char)(tr[0] - 32);
				TitreSection(filtreActif ? "Resultats" : tr, (int32)vis.Size());
				if (morts > 0 && !filtreActif) {
					char lp[120];
					snprintf(lp, sizeof(lp), "Retirer %d %s introuvable%s ou vide%s", (int)morts,
							 morts > 1 ? m.identite.motProjets.CStr() : m.identite.motProjet.CStr(), morts > 1 ? "s" : "",
							 morts > 1 ? "s" : "");
					const float32 bw = e.W(lp, pP) + S(40.f), bh = S(28.f);
					const NkPaintRect bp{cx + cw - bw, cy + (e.H(pI) - bh) * 0.5f, bw, bh};
					const bool sp = dansZone && Dans(bp, in);
					p.OutlineColor(bp, sp ? p.ColorOf(s.erreur) : cBord, sp ? cSurvol : cCarte, bh * 0.5f);
					const float32 g2 = S(13.f);
					NkLanceurPeindreGlyphe(p, NkLanceurGlyphe::Corbeille, {bp.x + S(12.f), bp.y + (bh - g2) * 0.5f, g2, g2},
										   sp ? p.ColorOf(s.erreur) : cDiscret);
					e.T(bp.x + S(30.f), bp.y + (bh - e.H(pP)) * 0.5f, lp, sp ? s.texte : s.texteDiscret, pP);
					if (sp) {
						res.curseurMain = true;
						if (in.mousePressed)
							res.action = NkLanceurAction::Purger;
					}
				}
				cy += e.H(pI) + S(14.f);

				if (vis.Empty()) {
					// L'etat vide : une illustration et deux phrases, jamais un cadre muet.
					const float32 gs = S(56.f);
					const float32 bh2 = S(170.f);
					const NkPaintRect vr{cx, cy, cw, bh2};
					p.OutlineColor(vr, cBord, Mix(cFond, cCarte, 0.5f), S(12.f));
					NkLanceurPeindreGlyphe(p, filtreActif ? NkLanceurGlyphe::Recherche : NkLanceurGlyphe::Dossier,
										   {cx + (cw - gs) * 0.5f, cy + S(26.f), gs, gs}, Alpha(acc, 0xB0));
					char l1[200], l2[200];
					if (filtreActif) {
						snprintf(l1, sizeof(l1), "Aucun %s ne correspond a « %s ».", m.identite.motProjet.CStr(), m.filtre);
						snprintf(l2, sizeof(l2), "Essayez un autre mot, ou effacez la recherche.");
					} else {
						snprintf(l1, sizeof(l1), "Aucun %s ouvert pour l'instant.", m.identite.motProjet.CStr());
						if (!m.identite.extensions.Empty())
							snprintf(l2, sizeof(l2), "Commencez depuis un modele, ou ouvrez un fichier %s existant.",
									 m.identite.extensions.CStr());
						else
							snprintf(l2, sizeof(l2), "Commencez depuis un modele, ou ouvrez un %s existant.",
									 m.identite.motProjet.CStr());
					}
					e.TC(cx, cw, cy + S(26.f) + gs + S(16.f), l1, s.texte, pG);
					e.TC(cx, cw, cy + S(26.f) + gs + S(20.f) + e.H(pG), l2, s.texteDiscret, pP);
					cy += bh2 + S(16.f);
				} else if (!m.vueListe) {
					// ── la GRILLE de grandes vignettes ──
					const float32 gap = S(18.f);
					int32 cols = (int32)((cw + gap) / (S(250.f) + gap));
					if (cols < 1)
						cols = 1;
					if (cols > 6)
						cols = 6;
					const float32 tw = (cw - gap * (float32)(cols - 1)) / (float32)cols;
					const float32 thH = tw * 9.f / 16.f;
					const float32 ipad = S(14.f);
					const int32 n = (int32)vis.Size();
					for (int32 debut = 0; debut < n; debut += cols) {
						int32 maxNom = 1, maxDos = 1;
						Ligne ln[3], ld[3];
						for (int32 j = debut; j < debut + cols && j < n; ++j) {
							const NkLanceurProjet &pr = m.projets[(usize)vis[(usize)j]];
							const int32 a = Couper(p, pr.nom.CStr(), tw - ipad * 2.f, pG, ln, 2, false);
							const NkString dos = Dossier(pr.chemin);
							const int32 b = Couper(p, dos.CStr(), tw - ipad * 2.f, pP, ld, 3, true);
							if (a > maxNom)
								maxNom = a;
							if (b > maxDos)
								maxDos = b;
						}
						const float32 th = thH + ipad + (e.H(pG) + S(1.f)) * (float32)maxNom + S(6.f) +
										   (e.H(pP) + S(2.f)) * (float32)maxDos + S(10.f) + e.H(pP) + ipad;
						for (int32 j = debut; j < debut + cols && j < n; ++j) {
							const int32 i = vis[(usize)j];
							const NkLanceurProjet &pr = m.projets[(usize)i];
							const NkPaintRect cr{cx + (tw + gap) * (float32)(j - debut), cy, tw, th};
							if (cr.y > zone.y + zone.h || cr.y + cr.h < zone.y)
								continue;
							const bool sain = pr.etat == 0u;
							const bool sur = dansZone && Dans(cr, in);
							p.FillColor({cr.x, cr.y + S(3.f), cr.w, cr.h}, Alpha(kNoir, 0x26), S(10.f));
							p.OutlineColor(cr, sur ? (sain ? acc : p.ColorOf(s.erreur)) : cBord, sur ? cSurvol : cCarte, S(10.f));
							const NkPaintRect ir{cr.x + 1.f, cr.y + 1.f, cr.w - 2.f, thH};
							if (pr.image != 0u)
								ImageCouvrir(p, ir, pr.image, pr.imageW, pr.imageH, S(9.f), false);
							else {
								// Pas d'apercu : une tuile a l'accent, un sol en perspective et
								// les initiales -- chaque projet garde un visage.
								p.PushClip(ir);
								AplatHautRond(p, ir, S(9.f), Mix(cCarte, acc, 0.16f), false);
								const float32 hz = ir.y + ir.h * 0.62f;
								for (int32 l = -5; l <= 5; ++l)
									Trait(p, ir.x + ir.w * 0.5f + (float32)l * ir.w * 0.05f, hz,
										  ir.x + ir.w * 0.5f + (float32)l * ir.w * 0.2f, ir.y + ir.h, 1.f,
										  Mix(cCarte, acc, 0.30f));
								for (int32 l = 1; l <= 3; ++l) {
									const float32 yy = hz + ir.h * 0.38f * ((float32)(l * l) / 9.f);
									Trait(p, ir.x, yy, ir.x + ir.w, yy, 1.f, Mix(cCarte, acc, 0.30f));
								}
								char ini[8];
								Initiales(pr.nom, ini);
								e.TC(ir.x, ir.w, ir.y + ir.h * 0.40f - e.H(pT) * 0.5f, ini, s.texte, pT);
								p.PopClip();
							}
							if (!sain) {
								p.FillColor(ir, Alpha(cFond, 0x80), 0.f);
								const char *et = !pr.etatTexte.Empty() ? pr.etatTexte.CStr()
												 : (pr.etat == 1u ? "introuvable" : (pr.etat == 2u ? "vide" : "indisponible"));
								const float32 bw = e.W(et, pP) + S(18.f), bh = e.H(pP) + S(8.f);
								p.FillColor({ir.x + S(10.f), ir.y + S(10.f), bw, bh}, p.ColorOf(s.erreur), bh * 0.5f);
								e.TC(ir.x + S(10.f), bw, ir.y + S(14.f), et, s.texteSurAccent, pP);
							}
							// Les deux gestes de carte, au survol : epingler, retirer.
							bool geste = false;
							if (sur) {
								const float32 rr = S(15.f);
								const uint32 fondB = Alpha(kNoir, 0xA8);
								if (BoutonRond(p, in, ir.x + ir.w - S(12.f) - rr, ir.y + S(12.f) + rr, rr,
											   NkLanceurGlyphe::Corbeille, fondB, kBlanc, res)) {
									res.action = NkLanceurAction::Retirer;
									res.index = i;
									geste = true;
								}
								if (BoutonRond(p, in, ir.x + ir.w - S(12.f) - rr * 3.f - S(8.f), ir.y + S(12.f) + rr, rr,
											   NkLanceurGlyphe::Epingle, fondB, pr.epingle ? p.ColorOf(s.epingle) : kBlanc,
											   res)) {
									res.action = NkLanceurAction::Epingler;
									res.index = i;
									geste = true;
								}
							}
							float32 ty = cr.y + thH + ipad;
							const int32 a = Couper(p, pr.nom.CStr(), tw - ipad * 2.f, pG, ln, 2, false);
							ty += e.Lignes(cr.x + ipad, ty, ln, a, s.texte, pG, S(1.f)) +
								  (e.H(pG) + S(1.f)) * (float32)(maxNom - a) + S(6.f);
							const NkString dos = Dossier(pr.chemin);
							const int32 b = Couper(p, dos.CStr(), tw - ipad * 2.f, pP, ld, 3, true);
							e.Lignes(cr.x + ipad, ty, ld, b, s.texteDiscret, pP, S(2.f));
							ty += (e.H(pP) + S(2.f)) * (float32)maxDos + S(10.f);
							const float32 g2 = S(13.f);
							NkLanceurPeindreGlyphe(p, NkLanceurGlyphe::Horloge,
												   {cr.x + ipad, ty + (e.H(pP) - g2) * 0.5f, g2, g2}, cDiscret);
							e.T(cr.x + ipad + g2 + S(6.f), ty, pr.date.Empty() ? "date inconnue" : pr.date.CStr(),
								s.texteDiscret, pP);
							if (pr.epingle) {
								const float32 g3 = S(14.f);
								NkLanceurPeindreGlyphe(p, NkLanceurGlyphe::Epingle,
													   {cr.x + cr.w - ipad - g3, ty + (e.H(pP) - g3) * 0.5f, g3, g3},
													   p.ColorOf(s.epingle));
							}
							if (sur && sain)
								res.curseurMain = true;
							if (sur && sain && in.mousePressed && !geste) {
								res.action = NkLanceurAction::OuvrirRecent;
								res.index = i;
							}
						}
						cy += th + gap;
					}
				} else {
					// ── la LISTE ──
					const float32 rh = S(66.f);
					for (usize j = 0; j < vis.Size(); ++j) {
						const int32 i = vis[j];
						const NkLanceurProjet &pr = m.projets[(usize)i];
						const NkPaintRect rr{cx, cy, cw, rh};
						if (!(rr.y > zone.y + zone.h || rr.y + rr.h < zone.y)) {
							const bool sain = pr.etat == 0u;
							const bool sur = dansZone && Dans(rr, in);
							const uint32 fondLigne = sur ? cSurvol : (j % 2 == 0 ? cCarte : Mix(cCarte, cFond, 0.5f));
							p.FillColor(rr, fondLigne, S(8.f));
							if (sur)
								p.FillColor({rr.x, rr.y + S(12.f), S(3.f), rh - S(24.f)}, sain ? acc : p.ColorOf(s.erreur), S(1.5f));
							const float32 vw = S(96.f), vh = S(54.f);
							const NkPaintRect ir{rr.x + S(12.f), rr.y + (rh - vh) * 0.5f, vw, vh};
							if (pr.image != 0u)
								ImageCouvrir(p, ir, pr.image, pr.imageW, pr.imageH, S(6.f), true);
							else {
								p.FillColor(ir, Mix(cCarte, acc, 0.22f), S(6.f));
								char ini[8];
								Initiales(pr.nom, ini);
								e.TC(ir.x, ir.w, ir.y + (vh - e.H(pG)) * 0.5f, ini, s.texte, pG);
							}
							const float32 dateW = S(170.f), boutonsW = S(80.f);
							const float32 tx = ir.x + vw + S(16.f);
							const float32 twl = rr.x + rr.w - boutonsW - dateW - tx;
							const NkString dos = Dossier(pr.chemin);
							Ligne l1[1], l2[1];
							const int32 a = Couper(p, pr.nom.CStr(), twl, pG, l1, 1, false);
							const int32 b = Couper(p, dos.CStr(), twl, pP, l2, 1, true);
							const float32 hBloc = e.H(pG) + S(4.f) + e.H(pP);
							float32 ty = rr.y + (rh - hBloc) * 0.5f;
							e.Lignes(tx, ty, l1, a, s.texte, pG, 0.f);
							e.Lignes(tx, ty + e.H(pG) + S(4.f), l2, b, sain ? s.texteDiscret : s.erreur, pP, 0.f);
							const char *dt = !sain ? (!pr.etatTexte.Empty() ? pr.etatTexte.CStr() : "introuvable")
												   : (pr.date.Empty() ? "date inconnue" : pr.date.CStr());
							e.T(rr.x + rr.w - boutonsW - dateW, rr.y + (rh - e.H(pP)) * 0.5f, dt,
								sain ? s.texteDiscret : s.erreur, pP);
							bool geste = false;
							const float32 rb = S(13.f);
							const float32 by = rr.y + rh * 0.5f;
							const uint32 fondB = sur ? Mix(fondLigne, cTexte, 0.10f) : fondLigne;
							if (BoutonRond(p, in, rr.x + rr.w - S(14.f) - rb, by, rb, NkLanceurGlyphe::Corbeille, fondB,
										   sur ? cTexte : cDiscret, res)) {
								res.action = NkLanceurAction::Retirer;
								res.index = i;
								geste = true;
							}
							if (BoutonRond(p, in, rr.x + rr.w - S(14.f) - rb * 3.f - S(6.f), by, rb, NkLanceurGlyphe::Epingle,
										   fondB, pr.epingle ? p.ColorOf(s.epingle) : (sur ? cTexte : cDiscret), res)) {
								res.action = NkLanceurAction::Epingler;
								res.index = i;
								geste = true;
							}
							if (sur && sain)
								res.curseurMain = true;
							if (sur && sain && in.mousePressed && !geste) {
								res.action = NkLanceurAction::OuvrirRecent;
								res.index = i;
							}
						}
						cy += rh + S(6.f);
					}
				}
			} else {
				// ── PAGE DE LIENS ───────────────────────────────────────────────
				const int32 n = (int32)page.liens.Size();
				const float32 gap = S(16.f);
				int32 cols = (int32)((cw + gap) / (S(320.f) + gap));
				if (cols < 1)
					cols = 1;
				if (cols > 4)
					cols = 4;
				const float32 tw = (cw - gap * (float32)(cols - 1)) / (float32)cols;
				const float32 ipad = S(18.f), gt = S(46.f);
				const float32 txw = tw - ipad * 2.f - gt - S(14.f);
				for (int32 debut = 0; debut < n; debut += cols) {
					int32 maxT = 1, maxD = 0;
					Ligne lt[3], ld[4];
					for (int32 i = debut; i < debut + cols && i < n; ++i) {
						const NkLanceurLien &l = page.liens[(usize)i];
						float32 wt = txw - (l.badge.Empty() ? 0.f : e.W(l.badge.CStr(), pP) + S(26.f));
						const int32 a = Couper(p, l.titre.CStr(), wt, pG, lt, 2, false);
						const int32 b = Couper(p, l.description.CStr(), txw, pP, ld, 4, true);
						if (a > maxT)
							maxT = a;
						if (b > maxD)
							maxD = b;
					}
					float32 th = ipad + (e.H(pG) + S(1.f)) * (float32)maxT + S(6.f) + (e.H(pP) + S(2.f)) * (float32)maxD + ipad;
					if (th < gt + ipad * 2.f)
						th = gt + ipad * 2.f;
					for (int32 i = debut; i < debut + cols && i < n; ++i) {
						const NkLanceurLien &l = page.liens[(usize)i];
						const NkPaintRect cr{cx + (tw + gap) * (float32)(i - debut), cy, tw, th};
						if (cr.y > zone.y + zone.h || cr.y + cr.h < zone.y)
							continue;
						const bool cliquable = l.disponible && !l.url.Empty();
						const bool sur = dansZone && Dans(cr, in) && cliquable;
						p.FillColor({cr.x, cr.y + S(3.f), cr.w, cr.h}, Alpha(kNoir, 0x22), S(10.f));
						p.OutlineColor(cr, sur ? acc : cBord, sur ? cSurvol : cCarte, S(10.f));
						const NkPaintRect gr{cr.x + ipad, cr.y + ipad, gt, gt};
						p.FillColor(gr, l.disponible ? Alpha(acc, 0x36) : Alpha(cDiscret, 0x30), S(10.f));
						const float32 gs = gt * 0.56f;
						NkLanceurPeindreGlyphe(p, l.glyphe, {gr.x + (gt - gs) * 0.5f, gr.y + (gt - gs) * 0.5f, gs, gs},
											   l.disponible ? acc : cDiscret);
						const float32 tx = gr.x + gt + S(14.f);
						float32 wt = txw;
						if (!l.badge.Empty()) {
							const float32 bw = e.W(l.badge.CStr(), pP) + S(16.f), bh = e.H(pP) + S(6.f);
							const NkPaintRect pb{cr.x + cr.w - ipad - bw, cr.y + ipad, bw, bh};
							p.FillColor(pb, l.disponible ? Alpha(acc, 0x40) : cCol, bh * 0.5f);
							e.TC(pb.x, pb.w, pb.y + S(3.f), l.badge.CStr(), l.disponible ? s.texte : s.texteDiscret, pP);
							wt -= bw + S(10.f);
						}
						const int32 a = Couper(p, l.titre.CStr(), wt, pG, lt, 2, false);
						float32 ty = cr.y + ipad;
						ty += e.Lignes(tx, ty, lt, a, l.disponible ? s.texte : s.texteDiscret, pG, S(1.f)) + S(6.f);
						const int32 b = Couper(p, l.description.CStr(), txw, pP, ld, 4, true);
						e.Lignes(tx, ty, ld, b, s.texteDiscret, pP, S(2.f));
						if (cliquable) {
							const float32 g2 = S(13.f);
							NkLanceurPeindreGlyphe(p, NkLanceurGlyphe::Lien,
												   {cr.x + cr.w - ipad - g2, cr.y + cr.h - ipad - g2, g2, g2},
												   sur ? acc : cDiscret);
						}
						if (sur) {
							res.curseurMain = true;
							if (in.mousePressed) {
								res.action = NkLanceurAction::OuvrirLien;
								res.index = i;
								res.url = l.url.CStr();
							}
						}
					}
					cy += th + gap;
				}
			}
			p.PopClip();

			// ── LE DEFILEMENT ───────────────────────────────────────────────────
			const float32 contenuH = (cy + m.defilement) - debutContenu + S(20.f);
			res.contenuH = contenuH;
			const float32 maxDef = contenuH > zone.h ? contenuH - zone.h : 0.f;
			if (dansZone && in.wheel != 0.f)
				m.defilement -= in.wheel * S(60.f);
			if (m.defilement > maxDef)
				m.defilement = maxDef;
			if (m.defilement < 0.f)
				m.defilement = 0.f;
			if (maxDef > 0.f) {
				const float32 piste = zone.h - S(8.f);
				float32 pouce = piste * zone.h / contenuH;
				if (pouce < S(36.f))
					pouce = S(36.f);
				const float32 py = zone.y + S(4.f) + (piste - pouce) * (m.defilement / maxDef);
				p.FillColor({mx + mw - S(9.f), py, S(5.f), pouce}, Alpha(cDiscret, 0x70), S(2.5f));
			}
			(void)cTexte;
			return res;
		}

	} // namespace editorkit
} // namespace nkentseu
